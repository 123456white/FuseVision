#include "RectTool.h"
#include <QtMath>

RectTool::RectTool(QObject* parent)
    : AnnotationTool(Rectangle, parent)
{}

void RectTool::mousePress(QPointF p, Qt::MouseButton button)
{
    if (button == Qt::RightButton) {
        cancelDrawing();
        return;
    }
    if (button != Qt::LeftButton) return;

    if (!m_placedStart)
        startRect(p);
    else
        endRect(p);
}

void RectTool::mouseMove(QPointF p)
{
    if (!m_placedStart) return;
    m_currentRect = QRectF(m_rectStart, p).normalized();
    emit requestPreviewUpdate();
    emit toolStatusChanged(QString::fromUtf8("矩形 | %1×%2")
        .arg((int)m_currentRect.width()).arg((int)m_currentRect.height()));
}

void RectTool::keyPress(int key, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods);
    if (!m_placedStart) return;

    switch (key) {
    case Qt::Key_Escape:
        cancelDrawing();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        endRect(m_currentRect.bottomRight());
        break;
    }
}

void RectTool::startRect(QPointF p)
{
    m_rectStart = p;
    m_placedStart = true;
    m_drawing = true;
    emit requestPreviewUpdate();
}

void RectTool::endRect(QPointF p)
{
    m_drawing = false;
    m_placedStart = false;
    emit requestPreviewUpdate();

    m_currentRect = QRectF(m_rectStart, p).normalized();
    if (m_currentRect.width() < 3 || m_currentRect.height() < 3)
        return;

    AnnotationShape shape;
    shape.type = AnnotationShape::RectShape;
    shape.labelId = m_ctx.activeLabelId;
    shape.bbox = m_currentRect;

    if (m_ctx.model) {
        AnnotationModel::Transaction tx(m_ctx.model, QString::fromUtf8("矩形"));
        tx.shapes().append(shape);
    }

    emit shapeCreated();
    emit requestRender();
}

void RectTool::cancelDrawing()
{
    m_drawing = false;
    m_placedStart = false;
    emit requestPreviewUpdate();
}

QPainterPath RectTool::previewPath() const
{
    QPainterPath path;
    if (!m_placedStart) return path;

    if (m_currentRect.isValid()) {
        // 预览矩形
        path.addRect(m_currentRect);
    } else {
        // 仅起点十字标记
        path.moveTo(m_rectStart.x() - 5, m_rectStart.y());
        path.lineTo(m_rectStart.x() + 5, m_rectStart.y());
        path.moveTo(m_rectStart.x(), m_rectStart.y() - 5);
        path.lineTo(m_rectStart.x(), m_rectStart.y() + 5);
    }
    return path;
}
