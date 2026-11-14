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
                              const QString& projectRoot,
                              const QString& fvprojPath,
                              QString* errorOut)
{
    if (!isValidModelName(modelName)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("模型名称无效：仅允许英文字母、数字和下划线，长度 1~64");
        return false;
    }

    // 模型目录 = projectRoot/ModelName/
    const QString modelDir = QDir(projectRoot).absoluteFilePath(modelName);
    if (QDir(modelDir).exists()) {
        if (errorOut)
            *errorOut = QString::fromUtf8("模型 \"%1\" 已存在").arg(modelName);
        return false;
    }

    // 根据任务类型创建不同的目录结构
    const bool needLabels = ModelTaskTypes::hasLabelsFolder(modelType);
    QStringList subDirs;

    if (needLabels) {
        // YOLO 布局：images + labels 并列
        subDirs = {
            "/images/train", "/images/val", "/images/test",
            "/labels/train", "/labels/val", "/labels/test"
        };
    } else {
        // 分类布局：train/OK train/NG val/OK val/NG test/OK test/NG
        subDirs = {
            "/train/OK", "/train/NG",
            "/val/OK",   "/val/NG",
            "/test/OK",  "/test/NG"
        };
    }

    for (const QString& sub : subDirs) {
        if (!QDir().mkpath(modelDir + sub)) {
            if (errorOut)
                *errorOut = QString::fromUtf8("无法创建目录: %1").arg(modelDir + sub);
            return false;
        }
    }

    // 生成 data.yaml（按任务类型差异化）
    const QString dataYamlPath = QDir(modelDir).absoluteFilePath("data.yaml");
    QFile yamlFile(dataYamlPath);
    if (yamlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&yamlFile);
        out << QString::fromUtf8("# %1 - %2 dataset config\n").arg(modelName, modelType);
        out << "\n";
        out << "path: " << QDir::toNativeSeparators(modelDir) << "\n";

        // 写入 task 参数
        if (modelType == QString::fromUtf8(ModelTaskTypes::InstanceSeg))
            out << "task: segment\n";
        else if (modelType == QString::fromUtf8(ModelTaskTypes::AnomalyDetect))
            out << "task: classify\n";
        else if (modelType == QString::fromUtf8(ModelTaskTypes::ObjDetectFixed))
            out << "task: obb\n";
        else
            out << "task: detect\n";

        if (needLabels) {
            out << "train: images/train\n";
            out << "val: images/val\n";
            out << "test: images/test\n";
            out << "\n";
            out << "# 标签类别（请在导入数据后更新）\n";
            out << "names:\n";
            out << "  0: background\n";
        } else {
            // 异常检测：分类布局，YOLO 自动从文件夹名称读取类别
            out << "train: train\n";
            out << "val: val\n";
            out << "test: test\n";
            out << "\n";
            out << "# 类别由文件夹名称自动识别（OK / NG）\n";
        }
        yamlFile.close();
    }

    // 生成 .fvdl
    const QString fvdlPath = QDir(modelDir).absoluteFilePath(modelName + ".fvdl");
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonObject root;
    root["format_version"] = "1.0";
    root["model_name"]     = modelName;
    root["model_type"]     = modelType;
    root["description"]    = "";
    root["created_time"]   = now;
    root["modified_time"]  = now;
    root["project_ref"]    = QFileInfo(projectRoot).fileName();
    root["dataset_ref"]    = modelName;
    root["dataset_layout"] = needLabels ? "yolo" : "classification";

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
    modelEntry["dataset_folder"] = modelName;
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
    info.datasetLayout= root.value("dataset_layout").toString("yolo");
    info.createdTime  = QDateTime::fromString(root.value("created_time").toString(), Qt::ISODate);
    info.modifiedTime = QDateTime::fromString(root.value("modified_time").toString(), Qt::ISODate);

    Logger::info(QString::fromUtf8("模型已加载: %1 (%2)").arg(info.modelName, info.modelType));
    return info;
}

// ── 保存模型 ──────────────────────────────────────────────────

bool ModelConfig::saveModel(const FvdlInfo& info, QString* errorOut)
{
    // 先读取现有文件以保留未在 FvdlInfo 中暴露的字段
    QFile file(info.fvdlPath);
    QJsonObject root;

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        root = doc.object();
    }

    root["model_name"]     = info.modelName;
    root["model_type"]     = info.modelType;
    root["description"]    = info.description;
    root["modified_time"]  = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["project_ref"]    = info.projectRef;
    root["dataset_ref"]    = info.datasetRef;
    root["dataset_layout"] = info.datasetLayout;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = QString::fromUtf8("无法写入模型文件: %1").arg(info.fvdlPath);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    Logger::debug(QString::fromUtf8("模型配置已保存: %1").arg(info.fvdlPath));
    return true;
}

// ── 校验模型名称 ──────────────────────────────────────────────

bool ModelConfig::isValidModelName(const QString& name)
{
    if (name.isEmpty() || name.length() > 64) return false;
    static const QRegularExpression re("^[A-Za-z0-9_]+$");
    return re.match(name).hasMatch();
}
