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
//   - createProject()：在三个全局根目录下创建项目文件夹 + 初始 .fvproj
//   - loadProject()  ：解析 .fvproj 文件，填充 FvprojInfo
//   - saveProject()  ：将 FvprojInfo 序列化回 .fvproj
//   - scanProjects() ：扫描 dl_data/ 下所有包含 .fvproj 的项目文件夹
//
// .fvproj JSON 结构见 Deeplearn.md 2.2 节。
// =============================================================================

// ── 项目完整信息（内存结构）──────────────────────────────────
struct FvprojInfo {
    QString    projectName;
    QString    description;       // 项目描述（用户自定义）
    QString    fvprojPath;        // 项目数据根路径/ProjectName/project.fvproj
    QString    projectRoot;       // 项目数据根路径/ProjectName/
    QString    currentModelId;
    QDateTime  createdTime;
    QDateTime  modifiedTime;
    QJsonObject models;           // key = modelId, value = { model_file, dataset_folder, status }
};

// ── 项目卡片简要信息（供画廊视图使用）────────────────────────
struct ProjectCardInfo {
    QString   projectName;
    QString   fvprojPath;
    QDateTime createdTime;
    int       modelCount = 0;
};

// =============================================================================
// ProjectConfig — 纯静态工具类（无状态，无需实例化）
// =============================================================================
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
