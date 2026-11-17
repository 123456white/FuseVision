#include "PolygonTool.h"
#include <QtMath>

PolygonTool::PolygonTool(QObject* parent)
    : AnnotationTool(Polygon, parent)
{}

void PolygonTool::mousePress(QPointF p, Qt::MouseButton button)
{
    if (button == Qt::RightButton) {
        cancelDrawing();
        return;
    }
    if (button != Qt::LeftButton) return;

    if (m_drawing)
        addVertex(p);
    else
        startPolygon(p);
}

void PolygonTool::mouseMove(QPointF p)
{
    m_lastMousePos = p;
    if (m_drawing) {
        emit requestPreviewUpdate();
        emit toolStatusChanged(QString::fromUtf8("多边形 | %1 顶点 | %2")
            .arg(m_currentPolygon.size())
            .arg(m_ctx.labelColors.value(m_ctx.activeLabelId, "")));
    }
}

void PolygonTool::mouseDoubleClick(QPointF /*p*/)
{
    if (m_drawing && m_currentPolygon.size() >= 3)
        closePolygon();
}

void PolygonTool::keyPress(int key, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (!m_drawing) return;

    switch (key) {
    case Qt::Key_Escape:
        cancelDrawing();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_currentPolygon.size() >= 3)
            closePolygon();
        break;
    case Qt::Key_Backspace:
        if (m_currentPolygon.size() > 1) {
            m_currentPolygon.removeLast();
            emit requestPreviewUpdate();
            emit toolStatusChanged(QString::fromUtf8("多边形 | %1 顶点")
                .arg(m_currentPolygon.size()));
        }
        break;
    }
}

void PolygonTool::startPolygon(QPointF p)
{
    m_drawing = true;
    m_currentPolygon.clear();
    m_currentPolygon << p;
    m_lastMousePos = p;
    emit requestPreviewUpdate();
}

void PolygonTool::addVertex(QPointF p)
{
    // 首点重击闭合 (Halcon 习惯: 8px 内点击首点 = 闭合)
    if (m_currentPolygon.size() >= 3) {
        QPointF d = p - m_currentPolygon.first();
        if (d.x() * d.x() + d.y() * d.y() <= 64.0/(m_ctx.viewScale*m_ctx.viewScale)) {
            closePolygon();
            return;
        }
    }
    m_currentPolygon << p;
    m_lastMousePos = p;
    emit requestPreviewUpdate();
}

void PolygonTool::closePolygon()
{
    if (m_currentPolygon.size() < 3) {
        cancelDrawing();
        return;
    }
    m_drawing = false;

    AnnotationShape shape;
    shape.type = AnnotationShape::PolygonShape;
    shape.labelId = m_ctx.activeLabelId;
    shape.polygon = m_currentPolygon;

    if (m_ctx.model) {
        AnnotationModel::Transaction tx(m_ctx.model, QString::fromUtf8("闭合多边形"));
        tx.shapes().append(shape);
    }

    m_currentPolygon.clear();
    emit shapeCreated();
    emit requestRender();
    emit requestPreviewUpdate();
}

void PolygonTool::cancelDrawing()
{
    m_drawing = false;
    m_currentPolygon.clear();
    emit requestPreviewUpdate();
}

QPainterPath PolygonTool::previewPath() const
{
    QPainterPath path;
    if (m_currentPolygon.isEmpty()) return path;

    // 已放置的顶点
    path.moveTo(m_currentPolygon.first());
    for(int i=1;i<m_currentPolygon.size();++i) path.lineTo(m_currentPolygon[i]);
    for (const QPointF& v : m_currentPolygon)
        path.addEllipse(v, 3, 3);

    if (m_currentPolygon.size() >= 1 && m_drawing) {
        // 从最后一个顶点到鼠标位置的虚线
        path.moveTo(m_currentPolygon.last());
        path.lineTo(m_lastMousePos);
    }

    return path;
}
