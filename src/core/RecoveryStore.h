/** @file
 * @brief RecoveryStore 的公共声明。
 * @ingroup core
 */
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QUuid>

// A same-volume rename is reversible. No recursive deletion occurs in the UI.
namespace RecoveryStore {
/** @brief 同卷移动到唯一恢复位置；失败返回 false，不把失败视为删除成功。 */
inline bool move(const QString& source, const QString& recoveryRoot, QString& destination, QString& error) {
    const QFileInfo info(source);
    if(!info.exists() || info.isSymLink()) {error=QObject::tr("目标不存在或为符号链接：%1").arg(source);return false;}
    if(!QDir().mkpath(recoveryRoot)) {error=QObject::tr("无法创建恢复目录：%1").arg(recoveryRoot);return false;}
    destination=QDir(recoveryRoot).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)+"-"+info.fileName());
    if(!QDir().rename(source,destination)) {error=QObject::tr("无法移动到恢复目录（可能被占用）：%1").arg(source);return false;}
    return true;
}
}
