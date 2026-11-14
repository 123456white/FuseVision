#include "AnnotationModel.h"
#include "AnnotationManagement.h"  // 仅为 AnnoTool 枚举 (兼容)
#include "core/Logger.h"

#include <QPainter>
#include <algorithm>

// =============================================================================
// AnnotationShape 便捷方法
// =============================================================================

QRectF AnnotationShape::boundingRect() const
{
    switch (type) {
    case BrushMask:
        return QRectF(0, 0, maskLayer.width(), maskLayer.height());
    case PolygonShape:
        return polygon.boundingRect();
    case RectShape:
    case RotatedRectShape:
        return bbox;
    }
    return {};
}

bool AnnotationShape::isEmpty() const
{
    switch (type) {
    case BrushMask:
        if (maskLayer.isNull()) return true;
        for (int y = 0; y < maskLayer.height(); y++) {
            const uchar* line = maskLayer.constScanLine(y);
            for (int x = 0; x < maskLayer.width(); x++)
                if (line[x] > 0) return false;
        }
        return true;
    case PolygonShape:
        return polygon.size() < 3;
    case RectShape:
    case RotatedRectShape:
        return bbox.width() < 3 || bbox.height() < 3;
    }
    return true;
}

// =============================================================================
// AnnotationUndoCommand
// =============================================================================

AnnotationUndoCommand::AnnotationUndoCommand(
        const QList<AnnotationShape>& before,
        const QList<AnnotationShape>& after,
        AnnotationModel* model,
        const QString& description)
    : QUndoCommand(description)
    , m_before(before)
    , m_after(after)
    , m_model(model)
{}

void AnnotationUndoCommand::undo()
{
    m_model->restoreShapes(m_before);
}

void AnnotationUndoCommand::redo()
{
    m_model->restoreShapes(m_after);
}

// =============================================================================
// AnnotationModel
// =============================================================================

AnnotationModel::AnnotationModel(QObject* parent)
    : QObject(parent)
{
    m_undoStack.setUndoLimit(50);
}

// ── 数据访问 ──────────────────────────────────────────────────

AnnotationShape* AnnotationModel::shapeAt(int index)
{
    if (index < 0 || index >= m_shapes.size()) return nullptr;
    return &m_shapes[index];
}

const AnnotationShape* AnnotationModel::shapeAt(int index) const
{
    if (index < 0 || index >= m_shapes.size()) return nullptr;
    return &m_shapes[index];
}

// ── 查询 ──────────────────────────────────────────────────────

int AnnotationModel::hitTestShape(QPointF p) const
{
    for (int i = m_shapes.size() - 1; i >= 0; --i) {
        const auto& s = m_shapes[i];
        if (!s.visible) continue;
        switch (s.type) {
        case AnnotationShape::BrushMask:
            if (p.x() >= 0 && p.y() >= 0 &&
                p.x() < s.maskLayer.width() && p.y() < s.maskLayer.height()) {
                if (s.maskLayer.scanLine((int)p.y())[(int)p.x()] > 0)
                    return i;
            }
            break;
        case AnnotationShape::PolygonShape:
            if (s.polygon.containsPoint(p, Qt::OddEvenFill)) return i;
            break;
        case AnnotationShape::RectShape:
        case AnnotationShape::RotatedRectShape:
            if (s.bbox.contains(p)) return i;
            break;
        }
    }
    return -1;
}

int AnnotationModel::hitTestVertex(int si, QPointF p) const
{
    if (si < 0 || si >= m_shapes.size()) return -1;
    const auto& s = m_shapes[si];
    const double r2 = 64.0;  // 8px radius squared

    switch (s.type) {
    case AnnotationShape::PolygonShape:
        for (int i = 0; i < s.polygon.size(); ++i) {
            QPointF d = p - s.polygon[i];
            if (d.x() * d.x() + d.y() * d.y() <= r2) return i;
        }
        break;
    case AnnotationShape::RectShape:
    case AnnotationShape::RotatedRectShape: {
        QPointF corners[4] = {
            s.bbox.topLeft(), s.bbox.topRight(),
            s.bbox.bottomRight(), s.bbox.bottomLeft()
        };
        for (int i = 0; i < 4; ++i) {
            QPointF d = p - corners[i];
            if (d.x() * d.x() + d.y() * d.y() <= r2) return i;
        }
        break;
    }
    default: break;
    }
    return -1;
}

QList<int> AnnotationModel::usedCategoryIds() const
{
    QList<int> ids;
    for (const auto& s : m_shapes) {
        if (s.visible && !ids.contains(s.labelId))
            ids.append(s.labelId);
    }
    return ids;
}

// ── Transaction ───────────────────────────────────────────────

AnnotationModel::Transaction::Transaction(AnnotationModel* model, const QString& description)
    : m_model(model)
    , m_snapshot(model->m_shapes)  // 深拷贝当前状态
    , m_description(description)
{
    Q_ASSERT_X(!m_model->m_activeTransaction,
               "AnnotationModel::Transaction",
               "Nested Transaction detected! Only one Transaction at a time.");
    m_model->m_activeTransaction = this;
}

AnnotationModel::Transaction::~Transaction()
{
    if (!m_committed) {
        commit();
    }
    m_model->m_activeTransaction = nullptr;
}

void AnnotationModel::Transaction::commit()
{
    if (m_committed) return;
    m_committed = true;

    // 深拷贝 after 状态 (m_snapshot 可能已被外部修改)
    QList<AnnotationShape> after = m_snapshot;

    // 只有实际变更时才 push undo
    if (m_model->m_shapes != after) {
        m_model->m_undoStack.push(
            new AnnotationUndoCommand(m_model->m_shapes, after, m_model, m_description));
        m_model->m_shapes = after;
        m_model->markDirty();
        emit m_model->dataChanged();
    }
}

// ── 非事务操作 ────────────────────────────────────────────────

void AnnotationModel::deleteShapes(const QList<int>& indices)
{
    Transaction tx(this, QString::fromUtf8("删除标注"));
    QList<int> sorted(indices);
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int idx : sorted) {
        if (idx >= 0 && idx < tx.shapes().size())
            tx.shapes().removeAt(idx);
    }
    // 析构自动 commit
}

void AnnotationModel::setVisibility(int index, bool visible)
{
    if (index < 0 || index >= m_shapes.size()) return;
    Transaction tx(this, QString::fromUtf8("切换可见性"));
    tx.shapes()[index].visible = visible;
}

void AnnotationModel::translateShape(int index, QPointF delta)
{
    if (index < 0 || index >= m_shapes.size()) return;
    auto& s = m_shapes[index];  // 直接修改 (调用者负责 push undo)
    switch (s.type) {
    case AnnotationShape::PolygonShape:
        s.polygon.translate(delta);
        break;
    case AnnotationShape::RectShape:
    case AnnotationShape::RotatedRectShape:
        s.bbox.translate(delta);
        break;
    case AnnotationShape::BrushMask: {
        QImage shifted(s.maskLayer.size(), QImage::Format_Grayscale8);
        shifted.fill(0);
        QPainter sp(&shifted);
        sp.drawImage(delta, s.maskLayer);
        sp.end();
        s.maskLayer = shifted;
        break;
    }
    }
    markDirty();
    emit dataChanged();
}

void AnnotationModel::setPolygonVertex(int shapeIdx, int vertexIdx, QPointF pos)
{
    if (shapeIdx < 0 || shapeIdx >= m_shapes.size()) return;
    auto& s = m_shapes[shapeIdx];
    if (s.type != AnnotationShape::PolygonShape) return;
    if (vertexIdx < 0 || vertexIdx >= s.polygon.size()) return;
    s.polygon[vertexIdx] = pos;
    markDirty();
    emit dataChanged();
}

void AnnotationModel::setBBoxVertex(int shapeIdx, int cornerIdx, QPointF pos)
{
    if (shapeIdx < 0 || shapeIdx >= m_shapes.size()) return;
    auto& s = m_shapes[shapeIdx];
    if (s.type != AnnotationShape::RectShape &&
        s.type != AnnotationShape::RotatedRectShape) return;
    if (cornerIdx < 0 || cornerIdx > 3) return;

    QPointF corners[4] = {
        s.bbox.topLeft(), s.bbox.topRight(),
        s.bbox.bottomRight(), s.bbox.bottomLeft()
    };
    corners[cornerIdx] = pos;

    qreal x1 = qMin(qMin(corners[0].x(), corners[1].x()),
                    qMin(corners[2].x(), corners[3].x()));
    qreal y1 = qMin(qMin(corners[0].y(), corners[1].y()),
                    qMin(corners[2].y(), corners[3].y()));
    qreal x2 = qMax(qMax(corners[0].x(), corners[1].x()),
                    qMax(corners[2].x(), corners[3].x()));
    qreal y2 = qMax(qMax(corners[0].y(), corners[1].y()),
                    qMax(corners[2].y(), corners[3].y()));
    s.bbox = QRectF(x1, y1, x2 - x1, y2 - y1);

    markDirty();
    emit dataChanged();
}

void AnnotationModel::appendShape(const AnnotationShape& shape)
{
    Transaction tx(this, QString::fromUtf8("添加标注"));
    tx.shapes().append(shape);
}

void AnnotationModel::appendShapes(const QList<AnnotationShape>& shapes)
{
    if (shapes.isEmpty()) return;
    Transaction tx(this, QString::fromUtf8("添加标注块"));
    tx.shapes().append(shapes);
}

void AnnotationModel::clear()
{
    if (m_shapes.isEmpty()) return;
    Transaction tx(this, QString::fromUtf8("清空标注"));
    tx.shapes().clear();
}

// ── 剪贴板 ────────────────────────────────────────────────────

void AnnotationModel::copyToClipboard(const QList<int>& indices)
{
    m_clipboard.clear();
    for (int idx : indices) {
        if (idx >= 0 && idx < m_shapes.size())
            m_clipboard.append(m_shapes[idx]);
    }
    if (!m_clipboard.isEmpty())
        Logger::info(QString::fromUtf8("已复制 %1 个标注").arg(m_clipboard.size()));
}

void AnnotationModel::pasteFromClipboard(QPointF offset)
{
    if (m_clipboard.isEmpty()) return;
    Transaction tx(this, QString::fromUtf8("粘贴标注"));
    for (auto s : m_clipboard) {
        switch (s.type) {
        case AnnotationShape::PolygonShape:
            s.polygon.translate(offset);
            break;
        case AnnotationShape::RectShape:
        case AnnotationShape::RotatedRectShape:
            s.bbox.translate(offset);
            break;
        case AnnotationShape::BrushMask: {
            QImage shifted(s.maskLayer.size(), QImage::Format_Grayscale8);
            shifted.fill(0);
            QPainter sp(&shifted);
            sp.drawImage(offset, s.maskLayer);
            sp.end();
            s.maskLayer = shifted;
            break;
        }
        }
        s.uuid = QUuid::createUuid();  // 新 UUID
        tx.shapes().append(s);
    }
}

// ── Undo ──────────────────────────────────────────────────────

void AnnotationModel::undo()
{
    if (m_undoStack.canUndo())
        m_undoStack.undo();
}

void AnnotationModel::redo()
{
    if (m_undoStack.canRedo())
        m_undoStack.redo();
}

void AnnotationModel::restoreShapes(const QList<AnnotationShape>& newShapes)
{
    m_undoInProgress = true;
    m_shapes = newShapes;
    markDirty();
    emit dataChanged();
    m_undoInProgress = false;
}

// ── 脏状态 ────────────────────────────────────────────────────

void AnnotationModel::markDirty()
{
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyStateChanged(true);
    }
}

void AnnotationModel::markClean()
{
    if (m_dirty) {
        m_dirty = false;
        emit dirtyStateChanged(false);
    }
}
