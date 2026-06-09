#include "ModelConfig.h"
#include "ProjectConfig.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

// =============================================================================
// ModelConfig.cpp — .fvdl 模型配置读写实现
// =============================================================================

// ── 新建模型 ──────────────────────────────────────────────────

bool ModelConfig::createModel(const QString& modelName,
                              const QString& modelType,
                              const QString& modelRoot,
                              const QString& datasetRoot,
                              const QString& projectName,
                              const QString& fvprojPath,
                              QString* errorOut)
{
    if (!isValidModelName(modelName)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("模型名称无效：仅允许英文字母、数字和下划线，长度 1~64");
        return false;
    }

    // 创建 .fvdl 文件
    const QString fvdlPath = QDir(modelRoot).absoluteFilePath(modelName + ".fvdl");
    if (QFile::exists(fvdlPath)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("模型 \"%1\" 已存在").arg(modelName);
        return false;
    }

    const QString datasetFolder = modelName + "_dataset";
    const QString datasetPath = QDir(datasetRoot).absoluteFilePath(datasetFolder);

    // 创建数据集目录
    if (!QDir().mkpath(QDir(datasetPath).absoluteFilePath("images")) ||
        !QDir().mkpath(QDir(datasetPath).absoluteFilePath("annotations")) ||
        !QDir().mkpath(QDir(datasetPath).absoluteFilePath("splits"))) {
        if (errorOut)
            *errorOut = QString::fromUtf8("无法创建数据集目录: %1").arg(datasetPath);
        return false;
    }

    // 生成 .fvdl JSON
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject root;
    root["format_version"] = "1.0";
    root["model_name"]     = modelName;
    root["model_type"]     = modelType;
    root["description"]    = "";
    root["created_time"]   = now;
    root["modified_time"]  = now;
    root["project_ref"]    = projectName;
    root["dataset_ref"]    = datasetFolder;

    QJsonObject arch;
    arch["backbone"] = "resnet50";
    arch["neck"]     = "FPN";
    root["architecture"] = arch;

    root["training_versions"] = QJsonArray();
    root["current_version"]   = "";
    QJsonObject infer;
    infer["confidence_threshold"] = 0.5;
    infer["nms_threshold"]        = 0.4;
    root["inference_config"] = infer;

    QJsonDocument doc(root);
    QFile file(fvdlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = QString::fromUtf8("无法写入模型文件: %1").arg(fvdlPath);
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // 更新 .fvproj
    QString loadErr;
    FvprojInfo proj = ProjectConfig::loadProject(fvprojPath, &loadErr);
    if (proj.projectName.isEmpty()) {
        if (errorOut) *errorOut = loadErr;
        return false;
    }

    QJsonObject models = proj.models;
    QJsonObject modelEntry;
    modelEntry["model_file"]    = fvdlPath;
    modelEntry["dataset_folder"] = datasetFolder;
    QJsonObject status;
    status["data_imported"] = false;
    status["annotated"]     = false;
    status["split_done"]    = false;
    status["trained"]       = false;
    modelEntry["status"] = status;
    models[modelName] = modelEntry;

    proj.models = models;
    proj.currentModelId = modelName;
    if (!ProjectConfig::saveProject(proj, &loadErr)) {
        if (errorOut) *errorOut = loadErr;
        return false;
    }

    Logger::info(QString::fromUtf8("模型已创建: %1 (%2)").arg(modelName, fvdlPath));
    return true;
}

// ── 加载模型 ──────────────────────────────────────────────────

FvdlInfo ModelConfig::loadModel(const QString& fvdlPath, QString* errorOut)
{
    FvdlInfo info;
    QFile file(fvdlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("无法打开模型文件: %1").arg(fvdlPath);
        return info;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut)
            *errorOut = QString::fromUtf8("模型文件 JSON 解析失败: %1").arg(parseError.errorString());
        return info;
    }

    QJsonObject root = doc.object();
    info.modelName    = root.value("model_name").toString();
    info.modelType    = root.value("model_type").toString();
    info.description  = root.value("description").toString();
    info.fvdlPath     = fvdlPath;
    info.projectRef   = root.value("project_ref").toString();
    info.datasetRef   = root.value("dataset_ref").toString();
    info.createdTime  = QDateTime::fromString(root.value("created_time").toString(), Qt::ISODate);
    info.modifiedTime = QDateTime::fromString(root.value("modified_time").toString(), Qt::ISODate);

    Logger::info(QString::fromUtf8("模型已加载: %1 (%2)").arg(info.modelName, info.modelType));
    return info;
}

// ── 校验模型名称 ──────────────────────────────────────────────

bool ModelConfig::isValidModelName(const QString& name)
{
    if (name.isEmpty() || name.length() > 64) return false;
    static const QRegularExpression re("^[A-Za-z0-9_]+$");
    return re.match(name).hasMatch();
}
