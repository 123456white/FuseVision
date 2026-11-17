/** @file
 * @brief ProjectConfig 的公共声明。
 * @ingroup dl_storage
 */
#ifndef PROJECTCONFIG_H
#define PROJECTCONFIG_H

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QJsonObject>

// =============================================================================
// ProjectConfig — .fvproj / .fvdl JSON 配置读写工具类（纯静态方法）
// =============================================================================
// 职责：
//   - createProject()：在指定项目根目录下创建项目文件夹和初始 .fvproj
//   - loadProject()  ：解析 .fvproj 文件，填充 FvprojInfo
//   - saveProject()  ：将 FvprojInfo 序列化回 .fvproj
//   - scanProjects() ：扫描 dl_data/ 下所有包含 .fvproj 的项目文件夹
//
// .fvproj JSON 契约见 docs/logic/DATA-CONTRACTS.md。
// =============================================================================

#include "deeplearning/domain/ProjectTypes.h"

// =============================================================================
// ProjectConfig — 纯静态工具类（无状态，无需实例化）
// =============================================================================
/** @brief 项目配置校验、读取和原子保存；调用方负责写权限与用户反馈。
 * @ingroup dl_storage
 */
class ProjectConfig
{
public:
    // ── 新建项目 ──────────────────────────────────────────────
    // 在 projectDataPath 下创建 ProjectName/ 文件夹，生成 project.fvproj。
    static bool createProject(const QString& projectName,
                              const QString& description,
                              const QString& projectDataPath,
                              QString* errorOut = nullptr);

    // ── 加载项目 ──────────────────────────────────────────────
    static FvprojInfo loadProject(const QString& fvprojPath,
                                  QString* errorOut = nullptr);

    // ── 保存项目 ──────────────────────────────────────────────
    /** @brief 原子更新并保留未知字段；损坏已有配置拒绝覆盖，失败填 errorOut。 */
    static bool saveProject(const FvprojInfo& info,
                            QString* errorOut = nullptr);

    // ── 扫描项目列表 ──────────────────────────────────────────
    static QList<ProjectCardInfo> scanProjects(const QString& projectDataPath);

    // ── 更新模型状态 ──────────────────────────────────────────
    static bool setModelStatus(const QString& fvprojPath,
                               const QString& modelId,
                               const QString& statusKey,
                               bool value);

    // ── 校验项目名称 ──────────────────────────────────────────
    static bool isValidProjectName(const QString& name);

private:
    static QJsonObject buildDefaultFvproj(const QString& projectName,
                                          const QString& description,
                                          const QString& projectRoot);
};

#endif // PROJECTCONFIG_H
