#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <QMap>
#include <QPair>
#include <QList>
#include "PermissionInfo.h"

/*
 * DatabaseManager — SQLite 数据访问层（Meyer's Singleton）
 *
 * 职责：
 *   1. 数据库生命周期管理（initDatabase / isConnected）
 *   2. 用户认证（validateLogin → LoginResult）
 *   3. 用户 CRUD（addUser / deleteUser / allUsers / changePassword）
 *   4. 模块权限 CRUD（initModulePermissions / getPermissions / setPermission）
 *
 * Schema:
 *   users(id, username, password_hash, role)               — 用户表
 *   user_permissions(user_id, module_name, can_read, can_write) — 权限表
 *
 * 角色体系:  0=用户  1=管理员  2=超级管理员
 * 安全设计:  密码使用 SHA-256 哈希存储，hashPassword() 为私有静态方法
 */

struct UserInfo {
    int     id;
    QString username;
    int     role;
};

struct LoginResult {
    bool    success = false;
    int     userId  = -1;
    int     role    = 0;
};

class DatabaseManager
{
public:
    static DatabaseManager& instance();

    /* 生命周期 */
    bool initDatabase();
    bool isConnected() const;

    /* 认证 */
    LoginResult validateLogin(const QString& username, const QString& password);

    /* 用户管理 */
    QList<UserInfo> allUsers() const;
    int  getUserId(const QString& username) const;
    int  addUser(const QString& username, const QString& password, int role);
    bool deleteUser(int userId);
    bool setUserRole(int userId, int role);
    bool changePassword(int userId, const QString& oldPassword, const QString& newPassword);
    bool changePasswordByAdmin(int userId, const QString& newPassword);

    /* 模块权限 */
    void initModulePermissions(const QString& moduleName);
    QMap<QString, PermissionInfo> getPermissions(int userId) const;
    QMap<int, QMap<QString, PermissionInfo>> getAllPermissions() const;
    void setPermission(int userId, const QString& moduleName, bool canRead, bool canWrite);

private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void   createTables();
    void   seedUsers();
    static QString hashPassword(const QString& password);

    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
