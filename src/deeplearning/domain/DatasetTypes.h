/** @file
 * @brief DatasetTypes 的公共声明。
 * @ingroup dl_domain
 */
#pragma once
#include "ModelTypes.h"
#include <QStringList>
#include <QList>

// ── 标签格式类型 ──────────────────────────────────────────────
// 不同模型类型对应不同的标签格式：
//   实例分割 → 灰度掩码 PNG（灰度值 1=类别1, 2=类别2...）
//   异常检测 → 二值 PNG（0=背景/正常, 255=异常）
//   物体检测(非自由矩形) → JSON 边界框（COCO 格式）
//   物体检测(自由矩形)   → JSON 边界框（COCO 格式，含旋转）
/** @brief 既有标签格式名称与任务推荐映射，不改变历史磁盘编号规则。
 * @ingroup dl_domain
 */
struct LabelTypes {
    static constexpr const char* GrayscaleMask = "灰度掩码PNG";   // 实例分割
    static constexpr const char* BinaryMask    = "二值掩码PNG";    // 异常检测
    static constexpr const char* CocoBBox      = "COCO边界框JSON"; // 物体检测(非自由矩形)
    static constexpr const char* CocoRotated   = "COCO旋转框JSON"; // 物体检测(自由矩形)

    static QStringList all() {
        return { QString::fromUtf8(GrayscaleMask),
                 QString::fromUtf8(BinaryMask),
                 QString::fromUtf8(CocoBBox),
                 QString::fromUtf8(CocoRotated) };
    }

    // 根据模型类型推荐标签格式
    static QString recommendForModelType(const QString& modelType) {
        if (modelType == ModelTaskTypes::InstanceSeg)    return GrayscaleMask;
        if (modelType == ModelTaskTypes::AnomalyDetect)  return BinaryMask;
        if (modelType == ModelTaskTypes::ObjDetectFixed) return CocoBBox;
        if (modelType == ModelTaskTypes::ObjDetectFree)  return CocoRotated;
        return GrayscaleMask;
    }

    // 标签格式是否为 PNG 掩码类型
    static bool isMaskType(const QString& labelType) {
        return labelType == GrayscaleMask || labelType == BinaryMask;
    }

    // 标签格式是否为 JSON 类型
    static bool isJsonType(const QString& labelType) {
        return labelType == CocoBBox || labelType == CocoRotated;
    }
};

// ── 统一数据集样本结构体（Phase 1 重构）────────────────────
// 对标 Halcon HDict + COCO image schema，统一承载图像元数据、
// 标注状态、选择状态与拆分归属，消除原先三个并行列表的同步问题。
/** @brief 数据集样本值对象；selected 为非持久化兼容字段，分类 split 来源于目录。
 * @ingroup dl_domain
 */
struct DatasetSample {
    int         id          = -1;       // COCO 兼容 image_id
    QString     fileName;               // 文件名
    QString     absolutePath;           // 绝对路径
    int         width       = 0;        // 图像宽度
    int         height      = 0;        // 图像高度

    // ── 标注信息 ──
    bool        annotated   = false;    // 是否有标注文件
    QList<int>  categoryIds;            // 所有类别 ID（0-based，多标签支持）
    int         instanceCount = 0;      // 实例数（shapes/annotations 总数）

    // ── UI 状态（非持久化）──
    bool        selected    = false;    // 是否被选中

    // ── 拆分归属（分类目录使用）──
    enum Split { None = 0, Train, Val, Test };
    Split       split       = None;

    // ── 辅助 ──
    int  primaryCategory() const { return categoryIds.isEmpty() ? -1 : categoryIds.first(); }
    bool hasEmptyMask()     const { return annotated && categoryIds.isEmpty(); }
};

