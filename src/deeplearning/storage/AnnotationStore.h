/** @file
 * @brief AnnotationStore 的公共声明。
 * @ingroup dl_storage
 */
#pragma once
#include "deeplearning/domain/AnnotationShape.h"

/** @brief 单文件保存结果；失败时保留目标路径和可显示的原因。
 * @ingroup dl_storage
 */
struct AnnotationSaveResult {
    bool ok = false;
    QString path;
    QString error;
};

// The native editor metadata travels in the same atomic PNG/JSON commit.
// Ordinary mask readers and legacy per-image JSON readers retain their format.
/** @brief 兼容现有 PNG/JSON 标签的原子持久化接口，无弹窗或会话依赖。
 * @ingroup dl_storage
 */
class AnnotationStore {
public:
    /**
     * @brief 校验类别和几何后原子保存 PNG/JSON；仅成功时 ok=true。
     * @details labelId 为零基；offset 是磁盘编号偏移，mask 决定 PNG 或 JSON。
     * 不更改传入形状或调用方 dirty 状态。失败保留已有目标，并报告路径和原因。
     */
    static AnnotationSaveResult save(const QString& path, const QString& imageName,
        const QImage& image, const QList<AnnotationShape>& shapes, bool mask, bool binary,
        int categoryCount, int offset = 1);
    /** @brief 兼容原生/普通标签；成功整体替换 shapes，失败通过 error 返回原因。 */
    static bool load(const QString& path, QSize imageSize, bool binary,
        const QStringList& categories, QList<AnnotationShape>& shapes, QString& error, int offset = 1);
};
