/** @file
 * @brief AnnotationRender 的公共声明。
 * @ingroup dl_state
 */
#ifndef ANNOTATIONRENDER_H
#define ANNOTATIONRENDER_H

#include <QObject>
#include <QImage>
#include <QPainterPath>
#include <QColor>
#include <QRect>
#include "deeplearning/domain/AnnotationShape.h"  // AnnotationShape

// =============================================================================
// AnnotationRender — 标注渲染管线 (v3.0)
// =============================================================================
// 职责:
//   - renderOverlay: 从 shapes 列表生成 ARGB32 半透明叠加层
//   - compileMask:  从 shapes 列表生成 Grayscale8 PNG 掩码
//   - renderPreview: 生成单条临时预览形状 (绘制中)
//
// 关键修复: compileMask 接受 labelOffset 参数, 不再硬编码 +1
//           兼容 Halcon minGray >= 2 的偏移规则
// =============================================================================

/** @brief 从形状值对象渲染叠加图和灰度掩码，不依赖业务窗口。
 * @ingroup dl_state
 */
class AnnotationRender : public QObject
{
    Q_OBJECT
public:
    explicit AnnotationRender(QObject* parent = nullptr);

    // ── 上下文设置 ────────────────────────────────────────────
    void setSourceImage(const QImage& image);
    void setLabelColors(const QStringList& colors);
    void setLabelOffset(int offset) { m_labelOffset = offset; }
    void setOverlayOpacity(double opacity) { m_overlayOpacity = qBound(0.0, opacity, 1.0); }

    QSize sourceSize() const { return m_sourceImage.size(); }
    int labelOffset() const { return m_labelOffset; }

    // ── 全量渲染 (数据变更时调用) ───────────────────────────────
    QImage renderOverlay(const QList<AnnotationShape>& shapes,
                         bool showVertexHandles = false,
                         int editShapeIndex = -1);

    // ── 编译 PNG 掩码 (保存时调用) ──────────────────────────────
    // labelOffset = DatasetManagement::currentMinGray()
    //   - 标准数据: labelOffset = 1  → 灰度 = labelId + 1
    //   - Halcon 数据: labelOffset = minGray (如 2 或更高)
    QImage compileMask(const QList<AnnotationShape>& shapes,
                       int labelOffset) const;

    // ── 临时预览渲染 ───────────────────────────────────────────
    QImage renderPreview(const QPainterPath& path,
                         const QColor& color,
                         int penWidth) const;

    // ── 缓存管理 ───────────────────────────────────────────────
    void invalidateCache() { m_cacheDirty = true; }
    bool isCacheValid() const { return !m_cacheDirty; }

private:
    QImage m_sourceImage;
    QStringList m_labelColors;
    int m_labelOffset = 1;          // Halcon minGray 偏移
    double m_overlayOpacity = 0.6;

    // 全量 overlay 缓存
    QImage m_cachedOverlay;
    bool m_cacheDirty = true;
};

#endif // ANNOTATIONRENDER_H
