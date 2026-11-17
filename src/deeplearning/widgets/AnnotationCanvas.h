/** @file
 * @brief AnnotationCanvas 的公共声明。
 * @ingroup dl_widgets
 */
#ifndef ANNOTATIONCANVAS_H
#define ANNOTATIONCANVAS_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>

// =============================================================================
// AnnotationCanvas — 标注画布（QGraphicsView 子类）
// =============================================================================
// 职责：
//   - 显示原图（底层） + 标注叠加层（上层）
//   - 鼠标缩放（滚轮） / 平移（中键 / Space+拖拽）
//   - 键盘事件转发给 AnnotationManagement
//   - 发射图像坐标的鼠标事件供 AnnotationManagement 处理工具交互
// =============================================================================

/** @brief 图像坐标输入及显示控件，通过信号发送事件，不负责持久化。
 * @ingroup dl_widgets
 */
class AnnotationCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit AnnotationCanvas(QWidget* parent = nullptr);

    // ── 图像设置 ──────────────────────────────────────────────
    /// 更新底图；preserveView 保持缩放及相对于图像的视口中心。
    void setImage(const QImage& image, bool preserveView = false);
    void setOverlayVisible(bool visible);
    void setBrushDiameter(int diameter);
    void setSelectionPath(const QPainterPath& path);
    void setOverlay(const QImage& overlay);       // 标注蒙版（半透明叠加层）
    void setOverlayOpacity(double opacity);       // 0.0~1.0
    void setShapesPreview(const QPainterPath& previewPath,
                          const QColor& color,
                          int penWidth);          // 绘制中的临时形状预览
    void clearPreview();                          // 清除临时预览

    // ── 视图控制 ──────────────────────────────────────────────
    void setZoom(double level);
    void zoomIn()  { setZoom(m_zoomLevel * 1.25); }
    void zoomOut() { setZoom(m_zoomLevel / 1.25); }
    void fitToWindow();
    void resetZoom();                             // 1:1

    double zoomLevel() const { return m_zoomLevel; }
    QPointF imageToScene(const QPoint& screenPos) const;
    bool   hasImage() const { return !m_imageRect.isEmpty(); }

signals:
    void mousePressed(QPointF imagePos, Qt::MouseButton button);
    void mouseMoved(QPointF imagePos);
    void mouseReleased(QPointF imagePos, Qt::MouseButton button);
    void mouseDoubleClicked(QPointF imagePos);
    void zoomChanged(double level);
    void keyPressed(int key, Qt::KeyboardModifiers modifiers);  // 键盘事件转发
    void gestureCancelled();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QGraphicsScene*      m_scene        = nullptr;
    QGraphicsPixmapItem* m_imageItem    = nullptr;   // 原图
    QGraphicsPixmapItem* m_overlayItem  = nullptr;   // 标注蒙版
    QGraphicsPathItem*   m_previewItem  = nullptr;   // 临时预览（绘制中的形状）
    QGraphicsPathItem* m_selectionItem = nullptr;

    QRectF               m_imageRect;                // 图像区域（用于坐标换算）
    double               m_zoomLevel    = 1.0;
    double               m_overlayOpacity = 0.6;
    bool                 m_panning      = false;     // 中键平移中
    QPoint               m_panStart;
    bool                 m_spacePanning   = false;   // Space 键平移
    Qt::MouseButton m_panButton = Qt::NoButton;
    QCursor m_toolCursor;
    int m_brushDiameter = 0;
    QPointF m_pointer;
    bool m_pointerInside = false;
};

#endif // ANNOTATIONCANVAS_H
