/** @file
 * @brief ProjectTypes 的公共声明。
 * @ingroup dl_domain
 */
#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QList>

// ── 项目完整信息（内存结构）──────────────────────────────────
/** @brief 项目文件的内存表示；projectRoot 随所打开文件的位置解析。
 * @ingroup dl_domain
 */
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
/** @brief 项目列表显示需要的摘要值对象。
 * @ingroup dl_domain
 */
struct ProjectCardInfo {
    QString   projectName;
    QString   fvprojPath;
    QDateTime createdTime;
    int       modelCount = 0;
};

