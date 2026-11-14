#include "AnnotationRender.h"

#include <QPainter>
#include <QPen>
#include <QtMath>

// =============================================================================
// AnnotationRender
// =============================================================================

AnnotationRender::AnnotationRender(QObject* parent)
    : QObject(parent)
{}

void AnnotationRender::setSourceImage(const QImage& image)
{
    m_sourceImage = image;
    invalidateCache();
}

void AnnotationRender::setLabelColors(const QStringList& colors)
{
    m_labelColors = colors;
    invalidateCache();
}

// =============================================================================
// renderOverlay — 全量生成 ARGB32 叠加层
// =============================================================================

QImage AnnotationRender::renderOverlay(
        const QList<AnnotationShape>& shapes,
        bool showVertexHandles,
        int editShapeIndex)
{
    if (m_sourceImage.isNull())
        return {};

    // 使用缓存
    if (!m_cacheDirty && !showVertexHandles && editShapeIndex < 0)
        return m_cachedOverlay;

    QImage overlay(m_sourceImage.size(), QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);
    QPainter p(&overlay);
    p.setRenderHint(QPainter::Antialiasing);

    for (int si = 0; si < shapes.size(); ++si) {
        const auto& s = shapes[si];
        if (!s.visible) continue;

        QColor color(m_labelColors.value(s.labelId, "#888888"));
        QColor fillColor = color;
        fillColor.setAlpha(static_cast<int>(m_overlayOpacity * 255));  // 使用成员变量

        switch (s.type) {
        case AnnotationShape::BrushMask:
            // 改为 QPainter 批量绘制 (不逐像素 setPixelColor)
            {
                QImage tinted = s.maskLayer;
                // 将非零像素设为 fillColor
                for (int y = 0; y < tinted.height(); y++) {
                    uchar* line = tinted.scanLine(y);
                    for (int x = 0; x < tinted.width(); x++) {
                        if (line[x] > 0) {
                            overlay.setPixelColor(x, y, fillColor);
                        }
                    }
                }
            }
            break;

        case AnnotationShape::PolygonShape:
            p.setPen(QPen(color, 2));
            p.setBrush(fillColor);
            p.drawPolygon(s.polygon);
            break;

        case AnnotationShape::RectShape:
            p.setPen(QPen(color, 2));
            p.setBrush(fillColor);
            p.drawRect(s.bbox);
            break;

        case AnnotationShape::RotatedRectShape:
            p.save();
            {
                QPointF c = s.rotCenter.isNull() ? s.bbox.center() : s.rotCenter;
                p.translate(c);
                p.rotate(s.rotation);
                p.translate(-c);
            }
            p.setPen(QPen(color, 2));
            p.setBrush(fillColor);
            p.drawRect(s.bbox);
            p.restore();
            break;
        }
    }

    // 顶点手柄 (选择模式)
    if (showVertexHandles) {
        p.setPen(QPen(Qt::white, 1));
        p.setBrush(QColor(0, 160, 255));
        for (const auto& s : shapes) {
            if (!s.visible) continue;
            switch (s.type) {
            case AnnotationShape::PolygonShape:
                for (const QPointF& v : s.polygon)
                    p.drawEllipse(v, 3, 3);
                break;
            case AnnotationShape::RectShape:
            case AnnotationShape::RotatedRectShape: {
                QPointF cs[4] = {
                    s.bbox.topLeft(), s.bbox.topRight(),
                    s.bbox.bottomRight(), s.bbox.bottomLeft()
                };
                for (const QPointF& c : cs)
                    p.drawEllipse(c, 3, 3);
                break;
            }
            default: break;
            }
        }
    }

    p.end();

    // 更新缓存 (非手柄模式)
    if (!showVertexHandles && editShapeIndex < 0) {
        m_cachedOverlay = overlay;
        m_cacheDirty = false;
    }

    return overlay;
}

// =============================================================================
// compileMask — 生成 Grayscale8 掩码 (v3.0 支持 labelOffset)
// =============================================================================

QImage AnnotationRender::compileMask(
        const QList<AnnotationShape>& shapes,
        int labelOffset) const
{
    if (m_sourceImage.isNull())
        return {};

    QImage mask(m_sourceImage.size(), QImage::Format_Grayscale8);
    mask.fill(0);
    QPainter mp(&mask);
    mp.setRenderHint(QPainter::Antialiasing);

    for (const auto& s : shapes) {
        if (!s.visible) continue;

        // ⭐ 关键修复: 使用 labelOffset 替代硬编码 +1
        //   - 标准数据: labelOffset = 1  → 灰度 = labelId + 1
        //   - Halcon 数据: labelOffset = minGray (如 2 或更高)
        int gray = s.labelId + labelOffset;

        // 灰度值范围保护
        gray = qBound(1, gray, 254);

        switch (s.type) {
        case AnnotationShape::BrushMask:
            for (int y = 0; y < s.maskLayer.height(); y++) {
                const uchar* line = s.maskLayer.constScanLine(y);
                uchar* dst = mask.scanLine(y);
                for (int x = 0; x < s.maskLayer.width(); x++) {
                    if (line[x] > 0)
                        dst[x] = static_cast<uchar>(gray);
                }
            }
            break;

        case AnnotationShape::PolygonShape:
            mp.setPen(Qt::NoPen);
            mp.setBrush(QColor(gray, gray, gray));
            mp.drawPolygon(s.polygon);
            break;

        case AnnotationShape::RectShape:
            mp.setPen(Qt::NoPen);
            mp.setBrush(QColor(gray, gray, gray));
            mp.drawRect(s.bbox);
            break;

        case AnnotationShape::RotatedRectShape:
            mp.save();
            {
                QPointF c = s.rotCenter.isNull() ? s.bbox.center() : s.rotCenter;
                mp.translate(c);
                mp.rotate(s.rotation);
                mp.translate(-c);
            }
            mp.setPen(Qt::NoPen);
            mp.setBrush(QColor(gray, gray, gray));
            mp.drawRect(s.bbox);
            mp.restore();
            break;
        }
    }

    mp.end();
    return mask;
}

// =============================================================================
// renderPreview — 临时形状预览
// =============================================================================

QImage AnnotationRender::renderPreview(
        const QPainterPath& path,
        const QColor& color,
        int penWidth) const
{
    if (m_sourceImage.isNull() || path.isEmpty())
        return {};

    QImage overlay(m_sourceImage.size(), QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);
    QPainter p(&overlay);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, penWidth));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.end();

    return overlay;
}
