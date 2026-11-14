#ifndef ANNOTATIONIO_H
#define ANNOTATIONIO_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QList>
#include <QSize>
#include "AnnotationModel.h"

// =============================================================================
// AnnotationIO — 多格式标注导入导出 (v3.0)
// =============================================================================

class AnnotationIO : public QObject
{
    Q_OBJECT
public:
    enum Format { PNGMask, COCO, YOLO, PascalVOC, Labelme, Unknown };

    explicit AnnotationIO(QObject* parent = nullptr);

    // ── 格式检测 ──────────────────────────────────────────────
    static Format detectFormat(const QString& annDir, const QString& baseName);

    // ── 导出 ──────────────────────────────────────────────────
    static bool savePNGMask(const QString& path, const QImage& mask);
    static bool saveCOCO(const QString& path,
                         const QList<AnnotationShape>& shapes,
                         int imageId, const QString& fileName,
                         QSize imageSize, int labelOffset = 1);
    static bool saveYOLO(const QString& path,
                         const QList<AnnotationShape>& shapes,
                         QSize imageSize,
                         int labelOffset = 1);
    static bool savePascalVOC(const QString& path,
                              const QList<AnnotationShape>& shapes,
                              QSize imageSize,
                              const QStringList& labelNames,
                              int labelOffset = 1);

    // ── 导入 ──────────────────────────────────────────────────
    static QList<AnnotationShape> loadPNGMask(const QImage& mask, int labelOffset = 1);
    static QList<AnnotationShape> loadCOCO(const QString& jsonPath, int labelOffset = 1);
    static QList<AnnotationShape> loadYOLO(const QString& txtPath,
                                            QSize imageSize,
                                            const QStringList& labelNames);
    static QList<AnnotationShape> loadPascalVOC(const QString& xmlPath,
                                                 const QStringList& labelNames);
    static QList<AnnotationShape> loadLabelme(const QString& jsonPath,
                                               const QStringList& labelNames);

    // ── 批量转换 ──────────────────────────────────────────────
    static int convertAll(const QString& srcDir, Format srcFmt,
                          const QString& dstDir, Format dstFmt,
                          const QStringList& labelNames,
                          int labelOffset = 1);
};

#endif
