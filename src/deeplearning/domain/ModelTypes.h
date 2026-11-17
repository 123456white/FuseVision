/** @file
 * @brief ModelTypes 的公共声明。
 * @ingroup dl_domain
 */
#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QList>

// ── 模型完整信息（内存结构）──────────────────────────────────
/** @brief 模型配置内存表示，保留描述、任务类型和数据集引用。
 * @ingroup dl_domain
 */
struct FvdlInfo {
    QString    modelName;
    QString    modelType;       // 实例分割 / 异常检测 / 物体检测(非自由矩形) / 物体检测(自由矩形)
    QString    description;
    QString    fvdlPath;        // ProjectName/modelName.fvdl
    QString    projectRef;      // 所属项目名
    QString    datasetRef;      // 绑定的数据集文件夹名（同 modelName）
    QString    datasetLayout;   // "native" 或 "classification"
    QDateTime  createdTime;
    QDateTime  modifiedTime;
};

// ── 模型简要信息（供列表视图使用）─────────────────────────────
/** @brief 模型列表使用的名称、类型和文件路径摘要。
 * @ingroup dl_domain
 */
struct ModelBriefInfo {
    QString   modelName;
    QString   modelType;
    QString   fvdlPath;
};

// ── 4 种深度学习任务类型 ─────────────────────────────────────
/** @brief 兼容已有四种任务名称；产品尚未确认的任务映射保持不变。
 * @ingroup dl_domain
 */
struct ModelTaskTypes {
    static constexpr const char* InstanceSeg    = "实例分割";
    static constexpr const char* AnomalyDetect  = "异常检测";
    static constexpr const char* ObjDetectFixed = "物体检测(非自由矩形)";
    static constexpr const char* ObjDetectFree  = "物体检测(自由矩形)";

    static QStringList all() {
        return { QString::fromUtf8(InstanceSeg),
                 QString::fromUtf8(AnomalyDetect),
                 QString::fromUtf8(ObjDetectFixed),
                 QString::fromUtf8(ObjDetectFree) };
    }

    // 判断任务类型是否需要 labels 文件夹（异常检测不需要）
    static bool hasLabelsFolder(const QString& type) {
        return type != QString::fromUtf8(AnomalyDetect);
    }
};

