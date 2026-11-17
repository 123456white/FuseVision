/** @file
 * @brief DatabaseManager 的公共声明。
 * @ingroup core
 */
#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QList>

/** @brief 历史用户记录的兼容视图；role 不参与当前读写授权。
 * @ingroup core
 */
struct UserInfo {
    int id=-1;
    QString username;
    int role=0; // Retained for compatibility with existing databases only.
};
/** @brief 账户认证结果；失败时 success=false 且 userId=-1。
 * @ingroup core
 */
struct LoginResult {
    bool success=false;
    int userId=-1;
};

// Single local account. Existing rows and password hashes are never migrated away.
/** @brief 单个本地管理账户的 SQLite 存储，按应用线程调用；实例不归调用者所有。
 * @ingroup core
 */
class DatabaseManager {
public:
    static DatabaseManager& instance();
    /** @brief 默认启动使用配置目录，空目标从最近活动库建一致副本；显式路径/:memory:不迁移。
     * @details 失败返回 false 并写 errorOut；不因迁移失败回落默认账户库。已有连接只允许复用相同路径。
     */
    bool initDatabase(const QString& databasePath=QString(),QString* errorOut=nullptr);
    bool isConnected() const;
    /** @brief 优先复用 superadmin，否则选最高历史 role、最早 ID；不改动原密码。 */
    UserInfo singleAccount() const;
    /** @brief 只认证唯一兼容账户；失败不建立会话，历史其他账户不能登录。 */
    LoginResult validateLogin(const QString& username,const QString& password);
    QList<UserInfo> allUsers() const; // Historical records, not available login choices.
    int getUserId(const QString& username) const;
    /** @brief 已登录的自身账户验证旧密码后比较更新；失败保留原密码，新密码至少六位。 */
    bool changePassword(int userId,const QString& oldPassword,const QString& newPassword);
private:
    DatabaseManager()=default;
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&)=delete;
    DatabaseManager& operator=(const DatabaseManager&)=delete;
    bool createTables();
    bool seedUsers();
    static QString hashPassword(const QString& password);
    QSqlDatabase m_db;
};
