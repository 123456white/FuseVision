/** @file
 * @brief ModelConfig 的公共声明。
 * @ingroup dl_storage
 */
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
// .fvdl JSON 契约见 docs/logic/DATA-CONTRACTS.md。
// =============================================================================

#include "deeplearning/domain/ModelTypes.h"

// =============================================================================
// ModelConfig — 纯静态工具类（无状态，无需实例化）
// =============================================================================
/** @brief 模型配置创建与持久化；失败返回原因，不切换活动界面上下文。
 * @ingroup dl_storage
 */
class ModelConfig
{
public:
    // ── 新建模型 ──────────────────────────────────────────────
    // 在 projectRoot 下创建 ModelName/ 文件夹
    //   原生标注工作目录（实例分割/自由矩形/OBB）:
    //     images/  labels/；训练分区由YOLO导出生成
    //   分类布局（异常检测）:
    //     train/OK/  train/NG/  val/OK/  val/NG/  test/OK/  test/NG/
    //   均生成 data.yaml + modelName.fvdl，并更新 .fvproj
    /** @brief 创建模型、布局和工程登记；失败清理本次未提交目录；description 实际持久化。 */
    static bool createModel(const QString& modelName,
                            const QString& modelType,
                            const QString& projectRoot,
                            const QString& fvprojPath,
                            QString* errorOut = nullptr,
                            const QString& description = {});

    // ── 加载模型 ──────────────────────────────────────────────
    static FvdlInfo loadModel(const QString& fvdlPath,
                              QString* errorOut = nullptr);

    // ── 保存模型 ──────────────────────────────────────────────
    static bool saveModel(const FvdlInfo& info,
                          QString* errorOut = nullptr);

    // ── 校验模型名称 ──────────────────────────────────────────
    static bool isValidModelName(const QString& name);
};

#endif // MODELCONFIG_H
