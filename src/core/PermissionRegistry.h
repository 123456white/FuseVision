/** @file
 * @brief PermissionRegistry 的公共声明。
 * @ingroup core
 */
#pragma once
#include <QObject>
#include <QStringList>

// Compatibility facade for existing page keys; no role matrix or database cache.
/** @brief 页面键兼容注册表；不读取旧权限矩阵，授权取当前登录态。
 * @ingroup core
 */
class PermissionRegistry : public QObject {
    Q_OBJECT
public:
    static PermissionRegistry& instance();
    void registerModule(const QString& moduleName);
    void unregisterModule(const QString& moduleName);
    QStringList modules() const;
    bool hasModule(const QString& moduleName) const;
    bool canRead(int userId, const QString& moduleName);
    bool canWrite(int userId, const QString& moduleName);
    void refreshPermissions(int userId);
signals:
    void permissionsChanged(int userId);
    void moduleRegistered(const QString& moduleName);
    void moduleUnregistered(const QString& moduleName);
private:
    PermissionRegistry() = default;
    QStringList m_modules;
};
