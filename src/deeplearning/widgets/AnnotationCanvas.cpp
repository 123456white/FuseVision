#include "AnnotationCanvas.h"
#include "core/Logger.h"

#include <QScrollBar>
#include <QApplication>
#include <QPainter>
#include <cmath>

// =============================================================================
// AnnotationCanvas.cpp — 标注画布实现
// =============================================================================

AnnotationCanvas::AnnotationCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
    // ── 场景 ──
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    // ── 视图配置 ──
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);  // 手动处理拖拽
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontSavePainterState);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(30, 30, 30));  // 深色背景（画布外区域）

    // ── 图像底层 ──
    m_imageItem = new QGraphicsPixmapItem;
    m_imageItem->setZValue(0);
    m_scene->addItem(m_imageItem);

    // ── 标注叠加层 ──
    m_overlayItem = new QGraphicsPixmapItem;
    m_overlayItem->setZValue(1);
    m_overlayItem->setOpacity(0.6);
    m_scene->addItem(m_overlayItem);

    // ── 临时预览层（绘制中的形状）──
    m_previewItem = new QGraphicsPathItem;
    m_previewItem->setZValue(2);
    m_previewItem->setVisible(false);
    m_scene->addItem(m_previewItem);
    m_selectionItem=new QGraphicsPathItem;
    m_selectionItem->setZValue(3);m_scene->addItem(m_selectionItem);
    QPen pen(Qt::white,1);pen.setCosmetic(true);m_selectionItem->setPen(pen);
    m_selectionItem->setBrush(QColor(0,160,255));
}

// ── 图像设置 ──────────────────────────────────────────────────

void AnnotationCanvas::setImage(const QImage& image, bool preserveView)
{
    const auto center = mapToScene(viewport()->rect().center());
    const auto previous = m_imageRect;
    if (image.isNull()) {
        m_imageRect={};m_imageItem->setPixmap({});m_overlayItem->setPixmap({});
        clearPreview();m_selectionItem->setPath({});m_scene->setSceneRect({});resetTransform();m_zoomLevel=1.0;
        m_panning=false;m_spacePanning=false;m_panButton=Qt::NoButton;
        emit zoomChanged(m_zoomLevel);return;
    }

    m_imageRect = QRectF(0, 0, image.width(), image.height());
    m_imageItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_imageRect.adjusted(-image.width(),-image.height(),image.width(),image.height()));
    if (preserveView && !previous.isEmpty())
        centerOn(center.x()/previous.width()*image.width(),center.y()/previous.height()*image.height());
    else fitToWindow();
}

void AnnotationCanvas::setOverlayVisible(bool visible) { m_overlayItem->setVisible(visible);m_selectionItem->setVisible(visible); }
void AnnotationCanvas::setBrushDiameter(int diameter) { m_brushDiameter=diameter; viewport()->update(); }
void AnnotationCanvas::setSelectionPath(const QPainterPath& path) {m_selectionItem->setPath(path);}

void AnnotationCanvas::setOverlay(const QImage& overlay)
{
    if (overlay.isNull()) {
        m_overlayItem->setPixmap(QPixmap());
        return;
    }
    m_overlayItem->setPixmap(QPixmap::fromImage(overlay));
}

void AnnotationCanvas::setOverlayOpacity(double opacity)
{
    m_overlayOpacity = qBound(0.0, opacity, 1.0);
    m_overlayItem->setOpacity(m_overlayOpacity);
}

void AnnotationCanvas::setShapesPreview(const QPainterPath& path,
                                         const QColor& color,
                                         int penWidth)
{
    m_previewItem->setPath(path);
    m_previewItem->setPen(QPen(color, penWidth));
    m_previewItem->setBrush(Qt::NoBrush);
    m_previewItem->setVisible(true);
}

void AnnotationCanvas::clearPreview()
{
    m_previewItem->setVisible(false);
}

// ── 视图控制 ──────────────────────────────────────────────────

void AnnotationCanvas::setZoom(double level)
{
    level = qBound(0.05, level, 20.0);
    double factor = level / m_zoomLevel;
    scale(factor, factor);
    m_zoomLevel = level;
    emit zoomChanged(m_zoomLevel);
}

void AnnotationCanvas::fitToWindow()
{
    if (m_imageRect.isEmpty()) return;
    fitInView(m_imageRect, Qt::KeepAspectRatio);
    m_zoomLevel = transform().m11();
    emit zoomChanged(m_zoomLevel);
}

void AnnotationCanvas::resetZoom()
{
    resetTransform();
    m_zoomLevel = 1.0;
    emit zoomChanged(m_zoomLevel);
}

QPointF AnnotationCanvas::imageToScene(const QPoint& screenPos) const
{
    return mapToScene(screenPos);
}

// ── 事件处理 ──────────────────────────────────────────────────

void AnnotationCanvas::wheelEvent(QWheelEvent* event)
{
    if (!hasImage()) return;
    const double delta=event->pixelDelta().isNull() ? event->angleDelta().y()/120.0 : event->pixelDelta().y()/120.0;
    if (delta==0) {event->accept();return;}
    const QPointF anchor=mapToScene(event->position().toPoint());
    setZoom(m_zoomLevel*std::pow(1.25,delta));
    const QPointF offset=anchor-mapToScene(event->position().toPoint());
    centerOn(mapToScene(viewport()->rect().center())+offset);
    event->accept();
}

void AnnotationCanvas::mousePressEvent(QMouseEvent* event)
{
    // Space 平移 → hand cursor
    setFocus(Qt::MouseFocusReason);
    if ((m_spacePanning && event->button()==Qt::LeftButton) || event->button()==Qt::MiddleButton) {
        if(!m_spacePanning) m_toolCursor=cursor();
        m_panButton=event->button();
        m_panning = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    QPointF pos = imageToScene(event->pos());
    emit mousePressed(pos, event->button());
}

void AnnotationCanvas::mouseMoveEvent(QMouseEvent* event)
{
    m_pointer=mapToScene(event->pos());m_pointerInside=true;viewport()->update();
    // 平移（中键或 Space）
    if (m_panning) {
        QPoint delta = event->pos() - m_panStart;
        m_panStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        return;
    }

    QPointF pos = imageToScene(event->pos());
    emit mouseMoved(pos);
}

void AnnotationCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    // 中键释放
    if (event->button() == m_panButton) {
        m_panning = false;
        m_panButton=Qt::NoButton;
        setCursor(m_spacePanning ? QCursor(Qt::OpenHandCursor) : m_toolCursor);
        return;
    }

    QPointF pos = imageToScene(event->pos());
    emit mouseReleased(pos, event->button());
}

void AnnotationCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    QPointF pos = imageToScene(event->pos());
    emit mouseDoubleClicked(pos);
}

void AnnotationCanvas::keyPressEvent(QKeyEvent* event)
{
    // Space 键 → 临时平移模式（按住平移，松开还原）
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        if(!m_spacePanning && !m_panning) m_toolCursor=cursor();
        m_spacePanning = true;
        setCursor(Qt::OpenHandCursor);
        return;
    }

    // 转发所有键盘事件给 AnnotationManagement 统一处理
    emit keyPressed(event->key(), event->modifiers());
    // 不调用基类 — 阻止 QGraphicsView 默认的滚屏行为
}

void AnnotationCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePanning = false;
        if (!m_panning)
            setCursor(m_toolCursor);
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void AnnotationCanvas::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect);
    if(m_brushDiameter>0 && m_pointerInside && !m_panning && !m_spacePanning) {
        QPen pen(Qt::white,1);pen.setCosmetic(true);painter->setPen(pen);painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(m_pointer,m_brushDiameter/2.0,m_brushDiameter/2.0);
    }
}

void AnnotationCanvas::focusOutEvent(QFocusEvent* event)
{
    if(m_spacePanning || m_panning) setCursor(m_toolCursor);
    m_panning=false;m_spacePanning=false;m_panButton=Qt::NoButton;
    emit gestureCancelled();
    QGraphicsView::focusOutEvent(event);
}

void AnnotationCanvas::leaveEvent(QEvent* event)
{
    m_pointerInside=false;viewport()->update();QGraphicsView::leaveEvent(event);
}

void AnnotationCanvas::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    // 首次设置图像后重新适应窗口
    if (!m_imageRect.isEmpty() && m_zoomLevel < 0.01) {
        fitToWindow();
    }
}
