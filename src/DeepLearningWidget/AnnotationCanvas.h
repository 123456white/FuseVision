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

class AnnotationCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit AnnotationCanvas(QWidget* parent = nullptr);

    // ── 图像设置 ──────────────────────────────────────────────
    void setImage(const QImage& image);
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

private:
    QGraphicsScene*      m_scene        = nullptr;
    QGraphicsPixmapItem* m_imageItem    = nullptr;   // 原图
    QGraphicsPixmapItem* m_overlayItem  = nullptr;   // 标注蒙版
    QGraphicsPathItem*   m_previewItem  = nullptr;   // 临时预览（绘制中的形状）

    QRectF               m_imageRect;                // 图像区域（用于坐标换算）
    double               m_zoomLevel    = 1.0;
    double               m_overlayOpacity = 0.6;
    bool                 m_panning      = false;     // 中键平移中
    QPoint               m_panStart;
    bool                 m_spacePanning   = false;   // Space 键平移
};

#endif // ANNOTATIONCANVAS_H
