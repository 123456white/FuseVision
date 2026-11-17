#include "EraserTool.h"
#include <QPainter>
#include <algorithm>

EraserTool::EraserTool(QObject* parent)
    : AnnotationTool(Eraser, parent)
{}

void EraserTool::mousePress(QPointF p, Qt::MouseButton button)
{
    if (button != Qt::LeftButton) return;
    if(m_ctx.sourceImage.isNull() || !m_ctx.model || m_ctx.activeLabelId<0) return;
    m_drawing = true;
    m_erasePath = QPainterPath();
    m_erasePath.moveTo(p);
    m_erasePath.lineTo(p+QPointF(0.001,0));
    emit requestPreviewUpdate();
}

void EraserTool::mouseMove(QPointF p)
{
    if (!m_drawing) return;
    m_erasePath.lineTo(p);
    emit requestPreviewUpdate();
    emit toolStatusChanged(QString::fromUtf8("橡皮擦 | %1px")
        .arg(m_ctx.brushSize));
}

void EraserTool::mouseRelease(QPointF p, Qt::MouseButton button)
{
    Q_UNUSED(p);
    if (button != Qt::LeftButton || !m_drawing) return;
    endErase();
}

void EraserTool::endErase()
{
    m_drawing = false;
    if (m_erasePath.isEmpty() || !m_ctx.model) {
        emit requestPreviewUpdate();
        return;
    }

    // 擦除所有 BrushMask 上的路径区域
    AnnotationModel::Transaction tx(m_ctx.model, QString::fromUtf8("橡皮擦"));

    for (auto& s : tx.shapes()) {
        if (s.type != AnnotationShape::BrushMask) continue;
        QPainter pr(&s.maskLayer);
        pr.setRenderHint(QPainter::Antialiasing,false);
        pr.setPen(QPen(Qt::black, m_ctx.brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        pr.drawPath(m_erasePath);
        pr.end();
    }

    // 移除全零图层
    tx.shapes().erase(
        std::remove_if(tx.shapes().begin(), tx.shapes().end(),
            [](const AnnotationShape& s) {
                if (s.type != AnnotationShape::BrushMask) return false;
                for (int y = 0; y < s.maskLayer.height(); y++) {
                    const uchar* line = s.maskLayer.constScanLine(y);
                    for (int x = 0; x < s.maskLayer.width(); x++)
                        if (line[x] > 0) return false;
                }
                return true;
            }),
        tx.shapes().end());

    m_erasePath = QPainterPath();
    emit shapeCreated();
    emit requestRender();
    emit requestPreviewUpdate();
}

QPainterPath EraserTool::previewPath() const
{
    if (!m_drawing) return {};
    return m_erasePath;
}
