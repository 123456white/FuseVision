#include "DatabaseManager.h"
#include "Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>

/*
 * DatabaseManager.cpp — SQLite 数据访问层实现
 *
 * 数据库位置：<exe目录>/data/fusevision.db
 * 连接名称：fusevision_conn（命名连接，避免与 QSqlDatabase::defaultConnection 冲突）
 *
 * Schema 设计（详见 createTables）：
 *   users(id/username/password_hash/role)
 *   user_permissions(user_id/module_name/can_read/can_write)  FK→users ON DELETE CASCADE
 *
 * 密码存储：SHA-256 哈希（QCryptographicHash::Sha256），仅存储哈希值
 * 角色体系：0=普通用户  1=管理员  2=超级管理员
 * 权限体系：模块级读写控制（canRead/canWrite），INSERT OR IGNORE 保证幂等初始化
 */

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::DatabaseManager()  = default;
DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) m_db.close();
}

QString DatabaseManager::hashPassword(const QString& password)
{
    return QString(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

/* ── 生命周期 ─────────────────────────────────────────────────── */

bool DatabaseManager::initDatabase()
{
    const QString dbPath = QCoreApplication::applicationDirPath() + "/data/fusevision.db";
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    m_db = QSqlDatabase::addDatabase("QSQLITE", "fusevision_conn");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        Logger::error(QString("Failed to open database: %1").arg(m_db.lastError().text()));
        return false;
    }

    Logger::info(QString("Database opened: %1").arg(dbPath));

    createTables();
    seedUsers();
    return true;
}

bool DatabaseManager::isConnected() const
{
    return m_db.isOpen();
}

/* ── 建表 & 种子 ──────────────────────────────────────────────── */

void DatabaseManager::createTables()
{
    QSqlQuery q(m_db);

    q.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username      TEXT    NOT NULL UNIQUE,"
        "  password_hash TEXT    NOT NULL,"
        "  role          INTEGER NOT NULL DEFAULT 0"
        ")");

    q.exec(
        "CREATE TABLE IF NOT EXISTS user_permissions ("
        "  user_id     INTEGER NOT NULL,"
        "  module_name TEXT    NOT NULL,"
        "  can_read    INTEGER NOT NULL DEFAULT 0,"
        "  can_write   INTEGER NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (user_id, module_name),"
        "  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
        ")");

    Logger::info("Database tables ensured.");
}

void DatabaseManager::seedUsers()
{
    QSqlQuery q(m_db);
    q.exec("SELECT COUNT(*) FROM users");
    if (q.next() && q.value(0).toInt() > 0) return;

    q.prepare("INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)");

    q.addBindValue("user");       q.addBindValue(hashPassword("123"));         q.addBindValue(0); q.exec();
    q.addBindValue("admin");      q.addBindValue(hashPassword("manager123"));  q.addBindValue(1); q.exec();
    q.addBindValue("superadmin"); q.addBindValue(hashPassword("admin123"));    q.addBindValue(2); q.exec();

    Logger::info("Default users seeded: user(0), admin(1), superadmin(2)");
}

/* ── 认证 ─────────────────────────────────────────────────────── */

LoginResult DatabaseManager::validateLogin(const QString& username, const QString& password)
{
    LoginResult result;

    QSqlQuery q(m_db);
    q.prepare("SELECT id, password_hash, role FROM users WHERE username = ?");
    q.addBindValue(username);

    if (!q.exec() || !q.next()) return result;

    if (q.value(1).toString() != hashPassword(password))
        return result;

    result.success = true;
    result.userId  = q.value(0).toInt();
    result.role    = q.value(2).toInt();
    return result;
}

/* ── 用户管理 ─────────────────────────────────────────────────── */

QList<UserInfo> DatabaseManager::allUsers() const
{
    QList<UserInfo> list;
    QSqlQuery q(m_db);
    q.exec("SELECT id, username, role FROM users ORDER BY id");
    while (q.next()) {
        list.append({ q.value(0).toInt(), q.value(1).toString(), q.value(2).toInt() });
    }
    return list;
}

int DatabaseManager::getUserId(const QString& username) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM users WHERE username = ?");
    q.addBindValue(username);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

int DatabaseManager::addUser(const QString& username, const QString& password, int role)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)");
    q.addBindValue(username);
    q.addBindValue(hashPassword(password));
    q.addBindValue(role);

    if (!q.exec()) {
        Logger::error(QString("Failed to add user '%1': %2").arg(username, q.lastError().text()));
        return -1;
    }

    int newId = q.lastInsertId().toInt();
    Logger::info(QString("User added: %1 (id=%2, role=%3)").arg(username).arg(newId).arg(role));
    return newId;
}

bool DatabaseManager::deleteUser(int userId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM users WHERE id = ?");
    q.addBindValue(userId);
    if (!q.exec()) {
        Logger::error(QString("Failed to delete user %1: %2").arg(userId).arg(q.lastError().text()));
        return false;
    }
    Logger::info(QString("User deleted: id=%1").arg(userId));
    return true;
}

bool DatabaseManager::setUserRole(int userId, int role)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET role = ? WHERE id = ?");
    q.addBindValue(role);
    q.addBindValue(userId);
    if (!q.exec()) {
        Logger::error(QString("Failed to set role for user %1: %2").arg(userId).arg(q.lastError().text()));
        return false;
    }
    Logger::info(QString("User role updated: id=%1 role=%2").arg(userId).arg(role));
    return true;
}

bool DatabaseManager::changePassword(int userId, const QString& oldPassword, const QString& newPassword)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT password_hash FROM users WHERE id = ?");
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) return false;
    if (q.value(0).toString() != hashPassword(oldPassword)) return false;

    q.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    q.addBindValue(hashPassword(newPassword));
    q.addBindValue(userId);

    if (!q.exec()) {
        Logger::error(QString("Failed to change password: %1").arg(q.lastError().text()));
        return false;
    }
    Logger::info(QString("Password changed for user id: %1").arg(userId));
    return true;
}

bool DatabaseManager::changePasswordByAdmin(int userId, const QString& newPassword)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    q.addBindValue(hashPassword(newPassword));
    q.addBindValue(userId);

    if (!q.exec()) {
        Logger::error(QString("Failed to change password by admin: %1").arg(q.lastError().text()));
        return false;
    }
    Logger::info(QString("Password changed by admin for user id: %1").arg(userId));
    return true;
}

/* ── 模块权限 ─────────────────────────────────────────────────── */

void DatabaseManager::initModulePermissions(const QString& moduleName)
{
    const QList<UserInfo> users = allUsers();

    QSqlQuery insertQ(m_db);
    insertQ.prepare(
        "INSERT OR IGNORE INTO user_permissions (user_id, module_name, can_read, can_write) "
        "VALUES (?, ?, ?, ?)");

    for (const auto& u : users) {
        const bool canWrite = (u.role == 1 || u.role == 2);
        insertQ.addBindValue(u.id);
        insertQ.addBindValue(moduleName);
        insertQ.addBindValue(1);                              // canRead always true
        insertQ.addBindValue(canWrite ? 1 : 0);
        insertQ.exec();
    }

    Logger::info(QString("Module '%1' permissions synced for %2 users.")
                     .arg(moduleName).arg(users.size()));
}

QMap<QString, PermissionInfo> DatabaseManager::getPermissions(int userId) const
{
    QMap<QString, PermissionInfo> result;
    QSqlQuery q(m_db);
    q.prepare("SELECT module_name, can_read, can_write FROM user_permissions WHERE user_id = ?");
    q.addBindValue(userId);
    q.exec();
    while (q.next()) {
        result[q.value(0).toString()] = { q.value(1).toBool(), q.value(2).toBool() };
    }
    return result;
}

QMap<int, QMap<QString, PermissionInfo>> DatabaseManager::getAllPermissions() const
{
    QMap<int, QMap<QString, PermissionInfo>> result;
    QSqlQuery q(m_db);
    q.exec("SELECT user_id, module_name, can_read, can_write FROM user_permissions");
    while (q.next()) {
        result[q.value(0).toInt()][q.value(1).toString()] = {
            q.value(2).toBool(), q.value(3).toBool()
        };
    }
    return result;
}

void DatabaseManager::setPermission(int userId, const QString& moduleName,
                                    bool canRead, bool canWrite)
{
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT OR REPLACE INTO user_permissions (user_id, module_name, can_read, can_write) "
        "VALUES (?, ?, ?, ?)");
    q.addBindValue(userId);
    q.addBindValue(moduleName);
    q.addBindValue(canRead ? 1 : 0);
    q.addBindValue(canWrite ? 1 : 0);

    if (!q.exec()) {
        Logger::error(QString("Failed to set permission: %1").arg(q.lastError().text()));
    }
}
