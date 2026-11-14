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
    info.projectRoot = paths.value("project_root").toString();

    // 路径不存在时，基于 fvprojPath 推断（fvproj 所在目录即为项目根目录）
    if (info.projectRoot.isEmpty())
        info.projectRoot = QFileInfo(fvprojPath).absolutePath();

    Logger::info(QString::fromUtf8("项目已加载: %1 (%2 个模型)")
                     .arg(info.projectName)
                     .arg(info.models.size()));
    return info;
}

// ── 保存项目 ──────────────────────────────────────────────────

bool ProjectConfig::saveProject(const FvprojInfo& info, QString* errorOut)
{
    QJsonObject paths;
    paths["project_root"] = info.projectRoot;

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

// ── 更新模型状态 ──────────────────────────────────────────────

bool ProjectConfig::setModelStatus(const QString& fvprojPath,
                                   const QString& modelId,
                                   const QString& statusKey,
                                   bool value)
{
    QFile file(fvprojPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject root     = doc.object();
    QJsonObject models   = root.value("models").toObject();
    QJsonObject modelObj = models.value(modelId).toObject();
    QJsonObject status   = modelObj.value("status").toObject();

    status[statusKey] = value;
    modelObj["status"] = status;
    models[modelId] = modelObj;
    root["models"] = models;
    root["modified_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

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
    paths["project_root"] = projectRoot;

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
