#ifndef ANNOTATIONMODEL_H
#define ANNOTATIONMODEL_H

#include <QObject>
#include <QImage>
#include <QPolygonF>
#include <QRectF>
#include <QUndoStack>
#include <QUndoCommand>
#include <QUuid>
#include <QDateTime>
#include <QPointF>
#include <QList>
#include <QPair>
#include <functional>

// =============================================================================
// AnnotationShape — 单个标注形状
// =============================================================================
struct AnnotationShape {
    enum ShapeType { BrushMask = 0, PolygonShape, RectShape, RotatedRectShape };
    ShapeType type;
    int       labelId;
    QPolygonF polygon;
    QRectF    bbox;
    double    rotation  = 0.0;
    QPointF   rotCenter;
    QImage    maskLayer;
    bool      visible = true;

    // v3.0 新增
    QString   comment;
    QDateTime createdTime;
    QDateTime modifiedTime;
    QUuid     uuid = QUuid::createUuid();

    bool operator==(const AnnotationShape& o) const {
        return type == o.type && labelId == o.labelId
            && polygon == o.polygon && bbox == o.bbox
            && rotation == o.rotation && rotCenter == o.rotCenter
            && visible == o.visible
            && comment == o.comment && uuid == o.uuid
            && maskLayer == o.maskLayer;
    }
    bool operator!=(const AnnotationShape& o) const { return !(*this == o); }

    // 便捷方法
    QRectF boundingRect() const;
    bool isEmpty() const;
};

// =============================================================================
// AnnotationModel — 标注数据模型 (v3.0)
// =============================================================================
// 职责: 标注形状存储、Undo/Redo、剪贴板、查询
// 线程: 仅主线程使用 (Qt GUI 线程)
// =============================================================================

class AnnotationManagement; // 前向声明

class AnnotationModel : public QObject
{
    Q_OBJECT
public:
    explicit AnnotationModel(QObject* parent = nullptr);

    // ── 数据访问 ──────────────────────────────────────────────
    const QList<AnnotationShape>& shapes() const { return m_shapes; }
    AnnotationShape* shapeAt(int index);
    const AnnotationShape* shapeAt(int index) const;
    int shapeCount() const { return m_shapes.size(); }
    bool isEmpty() const { return m_shapes.isEmpty(); }

    // ── 查询 ──────────────────────────────────────────────────
    int hitTestShape(QPointF imagePos) const;
    int hitTestVertex(int shapeIdx, QPointF imagePos) const;
    QList<int> usedCategoryIds() const;

    // ── 事务式修改 (RAII 自动 Undo) ─────────────────────────────
    class Transaction {
    public:
        Transaction(AnnotationModel* model, const QString& description);
        ~Transaction();  // 析构自动 commit (若未显式调用)
        void commit();   // 显式提交 (多次调用安全)

        QList<AnnotationShape>& shapes() { return m_snapshot; }
        QString description() const { return m_description; }

    private:
        AnnotationModel* m_model;
        QList<AnnotationShape> m_snapshot;
        QString m_description;
        bool m_committed = false;
    };

    // ── 非事务操作 (内部自动 Undo) ─────────────────────────────
    void deleteShapes(const QList<int>& indices);
    void setVisibility(int index, bool visible);
    void translateShape(int index, QPointF delta);
    void setPolygonVertex(int shapeIdx, int vertexIdx, QPointF pos);
    void setBBoxVertex(int shapeIdx, int cornerIdx, QPointF pos);
    void appendShape(const AnnotationShape& shape);
    void appendShapes(const QList<AnnotationShape>& shapes);
    void clear();

    // ── 剪贴板 ────────────────────────────────────────────────
    void copyToClipboard(const QList<int>& indices);
    void pasteFromClipboard(QPointF offset = QPointF(20, 20));
    bool hasClipboard() const { return !m_clipboard.isEmpty(); }
    int clipboardSize() const { return m_clipboard.size(); }

    // ── Undo 系统 ─────────────────────────────────────────────
    QUndoStack* undoStack() { return &m_undoStack; }
    const QUndoStack* undoStack() const { return &m_undoStack; }
    bool canUndo() const { return m_undoStack.canUndo(); }
    bool canRedo() const { return m_undoStack.canRedo(); }
    void undo();
    void redo();

    // ── 脏状态 ────────────────────────────────────────────────
    bool isDirty() const { return m_dirty; }
    void markClean();

signals:
    void dataChanged();               // 任何数据变更
    void dirtyStateChanged(bool dirty);

private:
    friend class AnnotationUndoCommand;
    friend class Transaction;

    void restoreShapes(const QList<AnnotationShape>& newShapes);
    void markDirty();
    void pushCommand(QUndoCommand* cmd);

    QList<AnnotationShape> m_shapes;
    QList<AnnotationShape> m_clipboard;
    QUndoStack m_undoStack;
    bool m_dirty = false;
    bool m_undoInProgress = false;

    // 事务锁: 禁止嵌套 Transaction
    Transaction* m_activeTransaction = nullptr;
};

// =============================================================================
// AnnotationUndoCommand
// =============================================================================
class AnnotationUndoCommand : public QUndoCommand
{
public:
    AnnotationUndoCommand(const QList<AnnotationShape>& before,
                          const QList<AnnotationShape>& after,
                          AnnotationModel* model,
                          const QString& description);
    void undo() override;
    void redo() override;

private:
    QList<AnnotationShape> m_before;
    QList<AnnotationShape> m_after;
    AnnotationModel* m_model;
};

#endif // ANNOTATIONMODEL_H
