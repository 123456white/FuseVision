/** @file
 * @brief SessionManager 的公共声明。
 * @ingroup core
 */
#pragma once
#include <QObject>
#include <QString>

// Guest may view every page; the sole account unlocks editing until logout.
/** @brief 当前只读或已登录会话；进程单例，不拥有业务页面。
 * @ingroup core
 */
class SessionManager : public QObject {
    Q_OBJECT
public:
    static SessionManager& instance();
    /** @brief 建立已认证账户的会话并通知控件；调用方必须先 validateLogin，仅接受唯一账户 ID。 */
    void login(int userId);
    /** @brief 恢复只读并发 sessionChanged；界面调用方必须先处理未保存内容。 */
    void logout();
    bool isLoggedIn() const;
    int currentUserId() const;
    QString currentUsername() const;
    void broadcastSession();
signals:
    void sessionChanged();
private:
    SessionManager()=default;
    int m_userId=-1;
    QString m_username;
};
