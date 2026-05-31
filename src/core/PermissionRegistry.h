#ifndef PERMISSIONREGISTRY_H
#define PERMISSIONREGISTRY_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include "PermissionInfo.h"

/*
 * PermissionRegistry — 模块注册 + 权限缓存层（QObject 单例）
 *
 * 核心职责：
 *   1. 模块注册/注销：registerModule(moduleName) 同步到 DB 所有用户
 *   2. 权限查询：canRead(userId, module) / canWrite(userId, module)
 *      - 优先命中内存缓存（微秒级），未命中返回 false（严格拒绝）
 *   3. 缓存刷新：refreshPermissions(userId) 从 DB 重新拉取指定用户权限
 *
 * 架构上位于 DatabaseManager 之上，对上层（SessionManager / Widget）
 * 提供 O(1) 权限判断，避免每次查询都走 SQLite
 *
 * 信号：
 *   moduleRegistered(name) — 新模块注册成功
 *   moduleUnregistered(name) — 模块已注销
 */
class PermissionRegistry : public QObject
{
    Q_OBJECT

public:
    static PermissionRegistry& instance();

    void registerModule(const QString& moduleName);
    void unregisterModule(const QString& moduleName);

    QStringList modules() const;
    bool        hasModule(const QString& moduleName) const;

    bool canRead(int userId, const QString& moduleName);
    bool canWrite(int userId, const QString& moduleName);

    void refreshPermissions(int userId);

signals:
    void moduleRegistered(const QString& moduleName);
    void moduleUnregistered(const QString& moduleName);

private:
    PermissionRegistry();
    ~PermissionRegistry();
    PermissionRegistry(const PermissionRegistry&) = delete;
    PermissionRegistry& operator=(const PermissionRegistry&) = delete;

    QStringList m_modules;                                   // 已注册模块名列表
    QMap<int, QMap<QString, PermissionInfo>> m_cache;        // userId → {module → PermissionInfo}
};

#endif // PERMISSIONREGISTRY_H
