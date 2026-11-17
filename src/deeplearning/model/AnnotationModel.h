/** @file
 * @brief AnnotationModel 的公共声明。
 * @ingroup dl_state
 */
#ifndef ANNOTATIONMODEL_H
#define ANNOTATIONMODEL_H

#include <QObject>
#include "deeplearning/domain/AnnotationShape.h"
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


// =============================================================================
// AnnotationModel — 标注数据模型 (v3.0)
// =============================================================================
// 职责: 标注形状存储、Undo/Redo、剪贴板、查询
// 线程: 仅主线程使用 (Qt GUI 线程)
// =============================================================================


/** @brief 权威标注集合、保存基线、事务和撤销重做；在对象所属线程调用。
 * @ingroup dl_state
 */
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
    int hitTestVertex(int shapeIdx, QPointF imagePos, double radius = 8.0) const;
    QList<int> usedCategoryIds() const;

    // ── 事务式修改 (RAII 自动 Undo) ─────────────────────────────
    /** @brief 借用模型的不可嵌套修改事务；析构提交，cancel 恢复预览前状态。 */
    class Transaction {
    public:
        Transaction(AnnotationModel* model, const QString& description);
        ~Transaction();  // 析构自动 commit (若未显式调用)
        void commit();   // 显式提交 (多次调用安全)
        void cancel();

        QList<AnnotationShape>& shapes() { return m_snapshot; }
        QString description() const { return m_description; }

    private:
        AnnotationModel* m_model;
        QList<AnnotationShape> m_before;
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
    /** @brief 载入完整快照、清空撤销并建立未修改基线；不写入文件。 */
    void reset(const QList<AnnotationShape>& shapes = {});

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
    /** @brief 将当前形状作为保存基线；调用方只能在持久化成功后调用。 */
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
    QList<AnnotationShape> m_cleanShapes;
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
/** @brief 保留修改前后快照的撤销命令，由 QUndoStack 管理生命周期。
 * @ingroup dl_state
 */
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
