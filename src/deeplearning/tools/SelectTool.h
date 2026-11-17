/** @file
 * @brief SelectTool 的公共声明。
 * @ingroup dl_tools
 */
#ifndef SELECTTOOL_H
#define SELECTTOOL_H

#include "AnnotationTool.h"

/** @brief 形状选择、拖动和顶点修改；预览可取消。
 * @ingroup dl_tools
 */
class SelectTool : public AnnotationTool
{
    Q_OBJECT
public:
    explicit SelectTool(QObject* parent = nullptr);

    QString name() const override { return QString::fromUtf8("选择"); }
    QString icon() const override { return QString::fromUtf8("🖱"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_S); }
    Qt::CursorShape cursor() const override { return Qt::ArrowCursor; }

    void mousePress(QPointF imagePos, Qt::MouseButton button) override;
    void mouseMove(QPointF imagePos) override;
    void mouseRelease(QPointF imagePos, Qt::MouseButton button) override;

    QPainterPath previewPath() const override { return {}; }

    int editShapeIndex() const { return m_editShapeIdx; }
    int editVertexIndex() const { return m_editVertexIdx; }

    void clearSelection();
    /// 与拖动共用同一套命中规则，容差保持为八个屏幕像素。
    int shapeAtPosition(QPointF p) const { return hitTestShape(p); }
    void cancelDrawing() override {AnnotationTool::cancelDrawing();clearSelection();}

private:
    int  m_editShapeIdx  = -1;
    int  m_editVertexIdx = -1;
    QPointF m_editStartPos;
    bool m_dragging = false;

    int  hitTestShape(QPointF p) const;
    int  hitTestVertex(int shapeIdx, QPointF p) const;

    void startDragShape(int idx, QPointF p);
    void startDragVertex(int si, int vi, QPointF p);
    void continueDrag(QPointF p);
    void endDrag();
};

#endif
