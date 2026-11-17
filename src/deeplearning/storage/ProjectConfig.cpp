#include "ProjectConfig.h"
#include "core/Logger.h"
#include "core/AtomicFile.h"
#include "StorageContract.h"

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
                                  const QString& projectDataPath,
                                  QString* errorOut)
{
    if (!isValidProjectName(projectName)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("项目名称无效：仅允许英文字母、数字和下划线，长度 1~64 个字符");
        return false;
    }

    const QString projectRoot = QDir(projectDataPath).absoluteFilePath(projectName);

    // 检查是否已存在
    if (QDir(projectRoot).exists()) {
        if (errorOut)
            *errorOut = QString::fromUtf8("项目 \"%1\" 已存在，请使用其他名称").arg(projectName);
        return false;
    }

    // 创建项目根目录
    QDir dir;
    if (!dir.mkpath(projectRoot)) {
        if (errorOut) *errorOut = QString::fromUtf8("无法创建项目目录: %1").arg(projectRoot);
        return false;
    }

    // 生成 .fvproj
    const QString fvprojPath = QDir(projectRoot).absoluteFilePath("project.fvproj");
    QJsonObject root = buildDefaultFvproj(projectName, description, projectRoot);

    QJsonDocument doc(root);
    if(!AtomicFile::write(fvprojPath,doc.toJson(QJsonDocument::Indented),errorOut)) {
        QDir(projectRoot).rmdir(".");
        return false;
    }

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
    if (parseError.error != QJsonParseError::NoError || !doc.isObject() || doc.object()["project_name"].toString().trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QString::fromUtf8("配置文件 JSON 解析失败: %1").arg(parseError.errorString());
        return info;
    }

    QJsonObject root = doc.object();
    if (!StorageContract::validate(root, "fusevision.project", errorOut)) return info;

    info.projectName  = root.value("project_name").toString();
    info.description  = root.value("description").toString();
    info.fvprojPath   = fvprojPath;
    info.currentModelId = root.value("current_model_id").toString();
    info.models       = root.value("models").toObject();
    info.createdTime  = QDateTime::fromString(root.value("created_time").toString(), Qt::ISODate);
    info.modifiedTime = QDateTime::fromString(root.value("modified_time").toString(), Qt::ISODate);

    info.projectRoot=QFileInfo(fvprojPath).absolutePath();
    info.fvprojPath=QFileInfo(fvprojPath).absoluteFilePath();
    // Current on-disk references are relative; resolve model files only in memory.
    for(auto it=info.models.begin();it!=info.models.end();++it) {
        auto entry=it.value().toObject();
        entry["model_file"]=QDir(info.projectRoot).absoluteFilePath(entry["model_file"].toString());
        it.value()=entry;
    }

    Logger::info(QString::fromUtf8("项目已加载: %1 (%2 个模型)")
                     .arg(info.projectName)
                     .arg(info.models.size()));
    return info;
}

// ── 保存项目 ──────────────────────────────────────────────────

bool ProjectConfig::saveProject(const FvprojInfo& info, QString* errorOut)
{
    QJsonObject root;
    QFile original(info.fvprojPath);
    if(original.exists()) {
        if(!original.open(QIODevice::ReadOnly)) {if(errorOut) *errorOut=original.errorString();return false;}
        QJsonParseError parse;
        const auto doc=QJsonDocument::fromJson(original.readAll(),&parse);
        if(parse.error!=QJsonParseError::NoError || !doc.isObject()) {if(errorOut) *errorOut=QString::fromUtf8("原项目配置损坏，拒绝覆盖");return false;}
        root=doc.object();original.close();
    }
    if (original.exists() && !StorageContract::validate(root, "fusevision.project", errorOut)) return false;
    const auto actualRoot = QFileInfo(info.fvprojPath).absolutePath();
    QJsonObject paths=root["paths"].toObject();paths["project_root"]=".";
    root["project_name"]     = info.projectName;
    root["description"]      = info.description;
    root["created_time"]     = info.createdTime.toString(Qt::ISODate);
    root["modified_time"]    = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["paths"]            = paths;
    root["current_model_id"] = info.currentModelId;
    auto models = info.models;
    for (auto it = models.begin(); it != models.end(); ++it) {
        if (!it.value().isObject()) {if(errorOut) *errorOut=QString::fromUtf8("模型登记项必须为对象");return false;}
        auto entry = it.value().toObject();
        for (const auto* key : {"model_file", "dataset_folder"}) {
            if (entry.contains(key) && !entry[key].isString()) {if(errorOut) *errorOut=QString::fromUtf8("模型路径必须为字符串");return false;}
            if (entry.contains(key)) entry[key] = StorageContract::portablePath(actualRoot, entry[key].toString());
        }
        it.value() = entry;
    }
    root["models"] = models;
    StorageContract::stamp(root, "fusevision.project");
    if (!StorageContract::validate(root, "fusevision.project", errorOut)) return false;

    if(!AtomicFile::write(info.fvprojPath,QJsonDocument(root).toJson(QJsonDocument::Indented),errorOut)) return false;

    Logger::debug(QString::fromUtf8("项目配置已保存: %1").arg(info.fvprojPath));
    return true;
}

// ── 扫描项目列表 ──────────────────────────────────────────────

QList<ProjectCardInfo> ProjectConfig::scanProjects(const QString& projectDataPath)
{
    QList<ProjectCardInfo> result;
    QDir dataDir(projectDataPath);
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
        if (err.error != QJsonParseError::NoError || !doc.isObject() ||
            !StorageContract::validate(doc.object(), "fusevision.project")) continue;

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
    return StorageContract::validName(name);
}

// ── 更新模型状态 ──────────────────────────────────────────────

bool ProjectConfig::setModelStatus(const QString& fvprojPath,
                                   const QString& modelId,
                                   const QString& statusKey,
                                   bool value)
{
    auto project = loadProject(fvprojPath);
    if (project.projectName.isEmpty()) return false;
    QJsonObject models = project.models;
    if(!models.contains(modelId)) return false;
    QJsonObject modelObj = models.value(modelId).toObject();
    QJsonObject status   = modelObj.value("status").toObject();

    status[statusKey] = value;
    modelObj["status"] = status;
    models[modelId] = modelObj;
    project.models = models;
    if (!saveProject(project)) return false;

    Logger::debug(QString::fromUtf8("模型状态更新: %1.%2 = %3")
        .arg(modelId, statusKey).arg(value ? "true" : "false"));
    return true;
}

// ── 构建默认 .fvproj JSON ─────────────────────────────────────

QJsonObject ProjectConfig::buildDefaultFvproj(const QString& projectName,
                                              const QString& description,
                                              const QString& projectRoot)
{
    QJsonObject paths;
    Q_UNUSED(projectRoot);
    paths["project_root"] = ".";

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject root;
    StorageContract::stamp(root, "fusevision.project");
    root["project_name"]     = projectName;
    root["description"]      = description;
    root["created_time"]     = now;
    root["modified_time"]    = now;
    root["paths"]            = paths;
    root["current_model_id"] = "";
    root["models"]           = QJsonObject();  // 空模型集合

    return root;
}
