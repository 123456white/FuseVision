#include "BrushTool.h"
#include <QPainter>

BrushTool::BrushTool(QObject* parent)
    : AnnotationTool(Brush, parent)
{}

void BrushTool::mousePress(QPointF p, Qt::MouseButton button)
{
    if (button != Qt::LeftButton) return;
    if(m_ctx.sourceImage.isNull() || !m_ctx.model || m_ctx.activeLabelId<0) return;
    m_drawing = true;
    m_brushPath = QPainterPath();
    m_brushPath.moveTo(p);
    m_brushPath.lineTo(p+QPointF(0.001,0));
    emit requestPreviewUpdate();
}

void BrushTool::mouseMove(QPointF p)
{
    if (!m_drawing) return;
    m_brushPath.lineTo(p);
    emit requestPreviewUpdate();
    emit toolStatusChanged(QString::fromUtf8("画笔 | %1px | %2")
        .arg(m_ctx.brushSize)
        .arg(m_ctx.labelColors.value(m_ctx.activeLabelId, "")));
}

void BrushTool::mouseRelease(QPointF p, Qt::MouseButton button)
{
    Q_UNUSED(p);
    if (button != Qt::LeftButton || !m_drawing) return;
    endStroke();
}

void BrushTool::endStroke()
{
    m_drawing = false;
    if (m_brushPath.isEmpty()) {
        emit requestPreviewUpdate();
        return;
    }

    QImage maskLayer(m_ctx.sourceImage.size(), QImage::Format_Grayscale8);
    maskLayer.fill(0);
    {
        QPainter pr(&maskLayer);
        pr.setRenderHint(QPainter::Antialiasing,false);
        pr.setPen(QPen(Qt::white, m_ctx.brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        pr.drawPath(m_brushPath);
    }

    AnnotationShape shape;
    shape.type = AnnotationShape::BrushMask;
    shape.labelId = m_ctx.activeLabelId;
    shape.maskLayer = maskLayer;

    // 尝试合并到已存在的同标签图层
    if (m_ctx.model) {
        bool merged = false;
        AnnotationModel::Transaction tx(m_ctx.model, QString::fromUtf8("画笔"));
        for (auto& s : tx.shapes()) {
            if (s.visible && s.type == AnnotationShape::BrushMask &&
                s.labelId == m_ctx.activeLabelId) {
                for(int y=0;y<maskLayer.height();++y)
                    for(int x=0;x<maskLayer.width();++x)
                        if(maskLayer.constScanLine(y)[x]) s.maskLayer.scanLine(y)[x]=255;
                merged = true;
                break;
            }
        }
        if (!merged) tx.shapes().append(shape);
    }

    m_brushPath = QPainterPath();
    emit shapeCreated();
    emit requestRender();
    emit requestPreviewUpdate();
}

QPainterPath BrushTool::previewPath() const
{
    if (!m_drawing) return {};
    return m_brushPath;
}

QColor BrushTool::previewColor() const
{
    return QColor(m_ctx.labelColors.value(m_ctx.activeLabelId, "#FF0000"));
}
