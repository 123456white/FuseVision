#include "ProjectConfig.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

// =============================================================================
// ProjectConfig.cpp — .fvproj JSON 配置读写实现
// =============================================================================

// ── 新建项目 ──────────────────────────────────────────────────

bool ProjectConfig::createProject(const QString& projectName,
                                  const QString& description,
                                  const QString& dlDataPath,
                                  const QString& dlDatasetPath,
                                  const QString& dlModelPath,
                                  QString* errorOut)
{
    if (!isValidProjectName(projectName)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("项目名称无效：仅允许英文字母、数字和下划线，长度 1~64 个字符");
        return false;
    }

    const QString dataRoot    = QDir(dlDataPath).absoluteFilePath(projectName);
    const QString datasetRoot = QDir(dlDatasetPath).absoluteFilePath(projectName);
    const QString modelRoot   = QDir(dlModelPath).absoluteFilePath(projectName);

    // 检查是否已存在
    if (QDir(dataRoot).exists() || QDir(datasetRoot).exists() || QDir(modelRoot).exists()) {
        if (errorOut)
            *errorOut = QString::fromUtf8("项目 \"%1\" 已存在，请使用其他名称").arg(projectName);
        return false;
    }

    // 创建三个根目录
    QDir dir;
    if (!dir.mkpath(dataRoot)) {
        if (errorOut) *errorOut = QString::fromUtf8("无法创建数据目录: %1").arg(dataRoot);
        return false;
    }
    if (!dir.mkpath(datasetRoot)) {
        if (errorOut) *errorOut = QString::fromUtf8("无法创建数据集目录: %1").arg(datasetRoot);
        return false;
    }
    if (!dir.mkpath(modelRoot)) {
        if (errorOut) *errorOut = QString::fromUtf8("无法创建模型目录: %1").arg(modelRoot);
        return false;
    }

    // 生成 .fvproj
    const QString fvprojPath = QDir(dataRoot).absoluteFilePath("project.fvproj");
    QJsonObject root = buildDefaultFvproj(projectName, description, dataRoot, datasetRoot, modelRoot);

    QJsonDocument doc(root);
    QFile file(fvprojPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = QString::fromUtf8("无法写入配置文件: %1").arg(fvprojPath);
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    Logger::info(QString::fromUtf8("项目已创建: %1 (%2)").arg(projectName, fvprojPath));
    return true;
}

// ── 加载项目 ──────────────────────────────────────────────────

FvprojInfo ProjectConfig::loadProject(const QString& fvprojPath, QString* errorOut)
{
    FvprojInfo info;
    QFile file(fvprojPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("无法打开配置文件: %1").arg(fvprojPath);
        return info;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut)
            *errorOut = QString::fromUtf8("配置文件 JSON 解析失败: %1").arg(parseError.errorString());
        return info;
    }

    QJsonObject root = doc.object();

    info.projectName  = root.value("project_name").toString();
    info.description  = root.value("description").toString();
    info.fvprojPath   = fvprojPath;
    info.currentModelId = root.value("current_model_id").toString();
    info.models       = root.value("models").toObject();
    info.createdTime  = QDateTime::fromString(root.value("created_time").toString(), Qt::ISODate);
    info.modifiedTime = QDateTime::fromString(root.value("modified_time").toString(), Qt::ISODate);

    QJsonObject paths = root.value("paths").toObject();
    info.dataRoot    = paths.value("data_root").toString();
    info.datasetRoot = paths.value("dataset_root").toString();
    info.modelRoot   = paths.value("model_root").toString();

    // 路径不存在时，尝试基于 fvprojPath 推断
    QDir fvprojDir = QFileInfo(fvprojPath).absoluteDir();
    if (info.dataRoot.isEmpty())
        info.dataRoot = fvprojDir.absolutePath();
    if (info.datasetRoot.isEmpty()) {
        // 尝试根据 dataRoot 推断 datasetRoot（同级目录替换）
        QString dataParent = QFileInfo(info.dataRoot).absolutePath();   // .../dl_data
        info.datasetRoot = QDir(dataParent).absoluteFilePath(
            QFileInfo(info.dataRoot).fileName());  // .../dl_datasets/ProjectA
        // 更准确的推断：向上两级再找 dl_datasets
        QDir d(dataParent);
        d.cdUp();
        info.datasetRoot = d.absoluteFilePath("dl_datasets/" + info.projectName);
    }
    if (info.modelRoot.isEmpty()) {
        QString dataParent = QFileInfo(info.dataRoot).absolutePath();
        QDir d(dataParent);
        d.cdUp();
        info.modelRoot = d.absoluteFilePath("dl_models/" + info.projectName);
    }

    Logger::info(QString::fromUtf8("项目已加载: %1 (%2 个模型)")
                     .arg(info.projectName)
                     .arg(info.models.size()));
    return info;
}

// ── 保存项目 ──────────────────────────────────────────────────

bool ProjectConfig::saveProject(const FvprojInfo& info, QString* errorOut)
{
    QJsonObject paths;
    paths["data_root"]    = info.dataRoot;
    paths["dataset_root"] = info.datasetRoot;
    paths["model_root"]   = info.modelRoot;

    QJsonObject root;
    root["project_name"]     = info.projectName;
    root["description"]      = info.description;
    root["created_time"]     = info.createdTime.toString(Qt::ISODate);
    root["modified_time"]    = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["paths"]            = paths;
    root["current_model_id"] = info.currentModelId;
    root["models"]           = info.models;

    QJsonDocument doc(root);
    QFile file(info.fvprojPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("无法写入配置文件: %1").arg(info.fvprojPath);
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    Logger::debug(QString::fromUtf8("项目配置已保存: %1").arg(info.fvprojPath));
    return true;
}

// ── 扫描项目列表 ──────────────────────────────────────────────

QList<ProjectCardInfo> ProjectConfig::scanProjects(const QString& dlDataPath)
{
    QList<ProjectCardInfo> result;
    QDir dataDir(dlDataPath);
    if (!dataDir.exists()) return result;

    const QFileInfoList subDirs = dataDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo& subDir : subDirs) {
        QFileInfo fvproj(subDir.absoluteFilePath(), "project.fvproj");
        if (!fvproj.exists()) continue;

        // 快速读取项目名称和基本信息
        QFile file(fvproj.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        QByteArray data = file.readAll();
        file.close();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) continue;

        QJsonObject root = doc.object();

        ProjectCardInfo card;
        card.projectName = root.value("project_name").toString(subDir.fileName());
        card.fvprojPath  = fvproj.absoluteFilePath();
        card.createdTime = QDateTime::fromString(
            root.value("created_time").toString(), Qt::ISODate);
        card.modelCount  = root.value("models").toObject().size();
        result.append(card);
    }

    return result;
}

// ── 名称校验 ──────────────────────────────────────────────────

bool ProjectConfig::isValidProjectName(const QString& name)
{
    if (name.isEmpty() || name.length() > 64) return false;
    // 仅允许英文字母、数字、下划线
    static const QRegularExpression re("^[A-Za-z0-9_]+$");
    return re.match(name).hasMatch();
}

// ── 构建默认 .fvproj JSON ─────────────────────────────────────

QJsonObject ProjectConfig::buildDefaultFvproj(const QString& projectName,
                                              const QString& description,
                                              const QString& dataRoot,
                                              const QString& datasetRoot,
                                              const QString& modelRoot)
{
    QJsonObject paths;
    paths["data_root"]    = dataRoot;
    paths["dataset_root"] = datasetRoot;
    paths["model_root"]   = modelRoot;

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject root;
    root["project_name"]     = projectName;
    root["description"]      = description;
    root["created_time"]     = now;
    root["modified_time"]    = now;
    root["paths"]            = paths;
    root["current_model_id"] = "";
    root["models"]           = QJsonObject();  // 空模型集合

    return root;
}
