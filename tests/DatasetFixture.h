#pragma once
#include <QJsonArray>
#include <QJsonObject>

// Literal test data: intentionally independent of the production schema writer.
inline QJsonObject datasetFixture(const QString& labelType, QJsonArray categories = {},
                                 const QString& mode = "annotation")
{
    for (qsizetype i = 0; i < categories.size(); ++i) {
        auto category = categories[i].toObject();
        category["id"] = int(i + 1);
        if (!category.contains("color")) category["color"] = "#888888";
        if (!category.contains("instance_count")) category["instance_count"] = 0;
        categories[i] = category;
    }
    return {{"document_type", "fusevision.dataset"}, {"format_version", "1.0"},
            {"dataset_mode", mode}, {"label_type", labelType}, {"categories", categories},
            {"image_count", 0}, {"instance_count", 0}};
}
