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
    for(int i=m_ctx.model->shapeCount()-1;i>=0;--i)
        if(hitTestVertex(i,p)>=0) return i;
    return m_ctx.model->hitTestShape(p);
}

int SelectTool::hitTestVertex(int si, QPointF p) const
{
    return m_ctx.model ? m_ctx.model->hitTestVertex(si,p,8.0/qMax(0.01,m_ctx.viewScale)) : -1;
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

}

void SelectTool::endDrag()
{
    m_dragging = false;
    // m_editShapeIdx/m_editVertexIdx 保持，方便连续拖拽
    // 外部调用 clearSelection() 来清除
}
