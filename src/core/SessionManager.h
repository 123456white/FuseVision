#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QString>

/*
 * SessionManager — 当前登录用户会话管理（QObject 单例）
 *
 * 设计目的：消除 "main→LoginDialog→FuseVision→UserManagementWidget" 的
 *           手动状态传递链，改为信号驱动 —— 任何 Widget 只需：
 *             connect(&SessionManager::instance(), &SessionManager::sessionChanged, ...)
 *           即可自动感知登录/登出事件，无需改动上游签名。
 *
 * 登录时自动调用 PermissionRegistry::refreshPermissions(userId)，确保权限缓存就绪。
 *
 * 信号：
 *   sessionChanged() — 登录或登出后触发，通知所有监听方刷新 UI
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    static SessionManager& instance();

    void login(int userId, const QString& username, int role);
    void logout();

    bool    isLoggedIn() const;
    int     currentUserId()   const;
    QString currentUsername() const;
    int     currentUserRole()   const;

signals:
    void sessionChanged();

private:
    SessionManager();
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    int     m_userId   = -1;
    QString m_username;
    int     m_role     = 0;
};

#endif // SESSIONMANAGER_H
