#ifndef MODELCONFIG_H
#define MODELCONFIG_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>

// =============================================================================
// ModelConfig — .fvdl 模型配置读写工具类（纯静态方法）
// =============================================================================
// 职责：
//   - createModel()：在 modelRoot 下创建 .fvdl，在 datasetRoot 下创建数据集子文件夹，
//                    并更新 .fvproj 中 models 条目
//   - loadModel()  ：解析 .fvdl 文件，填充 FvdlInfo
//   - isValidModelName()：校验模型名称（^[A-Za-z0-9_]+$）
//
// .fvdl JSON 结构见 Deeplearn.md 4.2 节。
// =============================================================================

// ── 模型完整信息（内存结构）──────────────────────────────────
struct FvdlInfo {
    QString    modelName;
    QString    modelType;       // 实例分割 / 异常检测 / 物体检测(非自由矩形) / 物体检测(自由矩形)
    QString    description;
    QString    fvdlPath;        // dl_models/ProjectA/model1.fvdl
    QString    projectRef;      // 所属项目名
    QString    datasetRef;      // 绑定的数据集文件夹名
    QDateTime  createdTime;
    QDateTime  modifiedTime;
};

// ── 模型简要信息（供列表视图使用）─────────────────────────────
struct ModelBriefInfo {
    QString   modelName;
    QString   modelType;
    QString   fvdlPath;
};

// ── 4 种深度学习任务类型 ─────────────────────────────────────
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
};

// =============================================================================
// ModelConfig — 纯静态工具类（无状态，无需实例化）
// =============================================================================
class ModelConfig
{
public:
    // ── 新建模型 ──────────────────────────────────────────────
    // 在 modelRoot 下创建 .fvdl 文件，在 datasetRoot 下创建数据集子文件夹，
    // 并更新 .fvproj 中 models 条目。
    static bool createModel(const QString& modelName,
                            const QString& modelType,
                            const QString& modelRoot,
                            const QString& datasetRoot,
                            const QString& projectName,
                            const QString& fvprojPath,
                            QString* errorOut = nullptr);

    // ── 加载模型 ──────────────────────────────────────────────
    static FvdlInfo loadModel(const QString& fvdlPath,
                              QString* errorOut = nullptr);

    // ── 校验模型名称 ──────────────────────────────────────────
    static bool isValidModelName(const QString& name);
};

#endif // MODELCONFIG_H
