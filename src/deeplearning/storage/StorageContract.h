/** @file
 * @brief 原生配置文档的版本、字段类型与可移植路径规则。
 * @ingroup dl_storage
 */
#pragma once
#include "deeplearning/domain/DatasetTypes.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace StorageContract {
inline bool validName(const QString& name);
/** @brief 仅接受完整1.0原生文档；拒绝无标记旧文件，保留合法扩展字段。 */
inline bool validate(const QJsonObject& root, const QString& type, QString* error = nullptr)
{
    const auto fail = [&](const QString& reason) {
        if (error) *error = reason;
        return false;
    };
    if (root["document_type"] != QJsonValue(type))
        return fail(QString::fromUtf8("配置缺少或不匹配document_type：%1").arg(type));
    if (root["format_version"] != QJsonValue("1.0"))
        return fail(QString::fromUtf8("仅支持明确标记的原生1.0格式，请新建数据；拒绝自动转换"));
    QStringList strings, objects, arrays;
    if (type == "fusevision.project") {
        if (!validName(root["project_name"].toString())) return fail(QString::fromUtf8("项目名称无效"));
        strings = {"project_name", "description", "created_time", "modified_time", "current_model_id"};
        objects = {"paths", "models"};
    } else if (type == "fusevision.model") {
        if (!validName(root["model_name"].toString()) || !ModelTaskTypes::all().contains(root["model_type"].toString()))
            return fail(QString::fromUtf8("模型名称或任务类型无效"));
        strings = {"model_name", "model_type", "description", "created_time", "modified_time",
                   "project_ref", "dataset_ref", "dataset_layout", "current_version"};
        objects = {"architecture", "inference_config"};
        arrays = {"training_versions"};
    } else if (type == "fusevision.dataset") {
        strings = {"label_type", "dataset_mode"};
        arrays = {"categories"};
    } else return fail(QString::fromUtf8("未知原生文档类型"));
    for (const auto& key : strings)
        if (!root[key].isString()) return fail(key + QString::fromUtf8(" 缺失或不是字符串"));
    for (const auto& key : objects)
        if (!root[key].isObject()) return fail(key + QString::fromUtf8(" 缺失或不是对象"));
    for (const auto& key : arrays)
        if (!root[key].isArray()) return fail(key + QString::fromUtf8(" 缺失或不是数组"));
    const auto integer = [](const QJsonValue& value, qint64 minimum) {
        return value.isDouble() && value.toDouble() >= minimum && value.toDouble() == value.toInteger(minimum - 1);
    };
    if (type == "fusevision.dataset") {
        const bool classification = root["dataset_mode"] == QJsonValue("classification");
        if (!classification && root["dataset_mode"] != QJsonValue("annotation")) return fail(QString::fromUtf8("数据集模式无效"));
        if (classification ? root["label_type"] != QJsonValue("OK/NG分类") : !LabelTypes::all().contains(root["label_type"].toString()))
            return fail(QString::fromUtf8("标签格式与数据集模式不匹配"));
        for (const auto* key : {"image_count", "instance_count"})
            if (!integer(root[key], 0)) return fail(QString(key) + QString::fromUtf8(" 缺失或不是非负整数"));
        QStringList names;
        const auto categories = root["categories"].toArray();
        for (qsizetype i = 0; i < categories.size(); ++i) {
            if (!categories[i].isObject()) return fail(QString::fromUtf8("类别项必须为对象"));
            const auto category = categories[i].toObject();
            const auto name = category["name"].toString();
            if (category["id"] != QJsonValue(int(i + 1)) || name.trimmed().isEmpty() || names.contains(name) ||
                !category["color"].isString() || !integer(category["instance_count"], -1))
                return fail(QString::fromUtf8("类别需要连续1-based编号、唯一非空名称、颜色及实例数（-1表示未知）"));
            names.append(name);
        }
        if (classification && names != QStringList{"OK", "NG"}) return fail(QString::fromUtf8("分类类别必须依次为OK、NG"));
    }
    if (type == "fusevision.model") {
        const auto layout = ModelTaskTypes::hasLabelsFolder(root["model_type"].toString()) ? "native" : "classification";
        if (root["dataset_layout"] != QJsonValue(layout) || root["dataset_ref"] != root["model_name"] ||
            !validName(root["project_ref"].toString())) return fail(QString::fromUtf8("模型布局或项目/数据集引用不符合当前标准"));
    }
    if (type == "fusevision.project") {
        if (root["paths"].toObject()["project_root"] != QJsonValue(".")) return fail(QString::fromUtf8("project_root必须为.（项目文件所在目录）"));
        const auto models = root["models"].toObject();
        const auto current = root["current_model_id"].toString();
        if (!current.isEmpty() && !models.contains(current)) return fail(QString::fromUtf8("当前模型未登记"));
        for (auto it = models.begin(); it != models.end(); ++it) {
            if (!validName(it.key()) || !it.value().isObject()) return fail(QString::fromUtf8("模型登记项无效：") + it.key());
            const auto entry = it.value().toObject();
            const auto path = entry["model_file"].toString();
            if (!entry["model_file"].isString() || path.isEmpty() || QDir::isAbsolutePath(path) ||
                path.contains('\\') || path.contains(':') || path.split('/').contains("..") ||
                path != QDir::cleanPath(path) || !path.startsWith(it.key() + "/") || !path.endsWith(".fvdl") ||
                entry["dataset_folder"] != QJsonValue(it.key()) || !entry["status"].isObject())
                return fail(QString::fromUtf8("模型引用必须为项目内正斜线相对路径，数据集目录须与模型同名"));
            const auto status = entry["status"].toObject();
            for (const auto* key : {"data_imported", "annotated", "split_done", "trained"})
                if (!status[key].isBool()) return fail(QString(key) + QString::fromUtf8(" 缺失或不是布尔值"));
        }
    }
    return true;
}
/** @brief 仅在成功提交时写入当前原生格式标记；读取不升级文件。 */
inline void stamp(QJsonObject& root, const QString& type)
{
    root["document_type"] = type;
    root["format_version"] = "1.0";
}
/** @brief 根目录内绝对引用转为正斜线相对路径，外部引用保持绝对路径。 */
inline QString portablePath(const QString& root, const QString& path)
{
    if (path.isEmpty()) return path;
    const auto normalized = QDir::fromNativeSeparators(path);
    if (!QDir::isAbsolutePath(normalized)) return QDir::cleanPath(normalized);
    const auto relative = QDir(root).relativeFilePath(normalized);
    return relative != ".." && !relative.startsWith("../") && !QDir::isAbsolutePath(relative)
        ? relative : QDir::cleanPath(normalized);
}
/** @brief 新建文件夹使用跨平台安全名称，拒绝Windows设备名。 */
inline bool validName(const QString& name)
{
    static const QRegularExpression allowed("^[A-Za-z0-9_]{1,64}$");
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$", QRegularExpression::CaseInsensitiveOption);
    return allowed.match(name).hasMatch() && !reserved.match(name).hasMatch();
}
}
