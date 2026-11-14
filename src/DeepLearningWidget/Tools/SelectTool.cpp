#include "SelectTool.h"
#include <QPainter>
#include <QtMath>

SelectTool::SelectTool(QObject* parent)
    : AnnotationTool(Select, parent)
{}

void SelectTool::mousePress(QPointF p, Qt::MouseButton button)
{
    if (button != Qt::LeftButton) return;
    if (!m_ctx.model) return;

    int si = hitTestShape(p);
    if (si >= 0) {
        int vi = hitTestVertex(si, p);
        if (vi >= 0) {
            startDragVertex(si, vi, p);
        } else {
            startDragShape(si, p);
        }
    }
}

void SelectTool::mouseMove(QPointF p)
{
    if (m_dragging) {
        continueDrag(p);
    }
    emit toolStatusChanged(QString::fromUtf8("选择 | (%1, %2)")
        .arg((int)p.x()).arg((int)p.y()));
}

void SelectTool::mouseRelease(QPointF p, Qt::MouseButton button)
{
    Q_UNUSED(p);
    if (button != Qt::LeftButton) return;
    if (m_dragging) endDrag();
}

void SelectTool::clearSelection()
{
    m_editShapeIdx = -1;
    m_editVertexIdx = -1;
    m_dragging = false;
}

// ── Hit Testing ───────────────────────────────────────────────

int SelectTool::hitTestShape(QPointF p) const
{
    if (!m_ctx.model) return -1;
    const auto& shapes = m_ctx.model->shapes();
    for (int i = shapes.size() - 1; i >= 0; --i) {
        const auto& s = shapes[i];
        if (!s.visible) continue;
        switch (s.type) {
        case AnnotationShape::BrushMask:
            if (p.x() >= 0 && p.y() >= 0 &&
                p.x() < s.maskLayer.width() && p.y() < s.maskLayer.height())
                if (s.maskLayer.scanLine((int)p.y())[(int)p.x()] > 0) return i;
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

int SelectTool::hitTestVertex(int si, QPointF p) const
{
    if (!m_ctx.model || si < 0) return -1;
    const auto* s = m_ctx.model->shapeAt(si);
    if (!s) return -1;
    const double r2 = 64.0;

    switch (s->type) {
    case AnnotationShape::PolygonShape:
        for (int i = 0; i < s->polygon.size(); ++i) {
            QPointF d = p - s->polygon[i];
            if (d.x() * d.x() + d.y() * d.y() <= r2) return i;
        }
        break;
    case AnnotationShape::RectShape:
    case AnnotationShape::RotatedRectShape: {
        QPointF corners[4] = {
            s->bbox.topLeft(), s->bbox.topRight(),
            s->bbox.bottomRight(), s->bbox.bottomLeft()
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

// ── Drag Operations ───────────────────────────────────────────

void SelectTool::startDragShape(int si, QPointF p)
{
    m_editShapeIdx = si;
    m_editVertexIdx = -1;
    m_editStartPos = p;
    m_dragging = true;
    // 事务在 mousePress 中创建 (外部)
}

void SelectTool::startDragVertex(int si, int vi, QPointF p)
{
    m_editShapeIdx = si;
    m_editVertexIdx = vi;
    m_editStartPos = p;
    m_dragging = true;
}

void SelectTool::continueDrag(QPointF p)
{
    if (!m_ctx.model || m_editShapeIdx < 0) return;
    QPointF delta = p - m_editStartPos;
    const auto& shapes = m_ctx.model->shapes();

    if (m_editShapeIdx >= shapes.size()) return;

    if (m_editVertexIdx >= 0) {
        // 拖拽顶点
        const auto& s = shapes[m_editShapeIdx];
        switch (s.type) {
        case AnnotationShape::PolygonShape:
            if (m_editVertexIdx < s.polygon.size())
                m_ctx.model->setPolygonVertex(m_editShapeIdx, m_editVertexIdx, p);
            break;
        case AnnotationShape::RectShape:
        case AnnotationShape::RotatedRectShape:
            m_ctx.model->setBBoxVertex(m_editShapeIdx, m_editVertexIdx, p);
            break;
        default: break;
        }
    } else {
        // 整体移动 (delta 累积)
        m_ctx.model->translateShape(m_editShapeIdx, delta);
    }

    m_editStartPos = p;
    emit requestRender();
}

void SelectTool::endDrag()
{
    m_dragging = false;
    // m_editShapeIdx/m_editVertexIdx 保持，方便连续拖拽
    // 外部调用 clearSelection() 来清除
}
