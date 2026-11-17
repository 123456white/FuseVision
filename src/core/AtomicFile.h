/** @file
 * @brief AtomicFile 的公共声明。
 * @ingroup core
 */
#pragma once
#include <QSaveFile>
#include <QString>

namespace AtomicFile {
/** @brief 用 QSaveFile 提交单文件；失败返回 false 并填写路径和原因，不创建父目录。 */
inline bool write(const QString& path, const QByteArray& bytes, QString* error=nullptr) {
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {
        if(error) *error=path+"\n"+file.errorString();
        return false;
    }
    return true;
}
}
