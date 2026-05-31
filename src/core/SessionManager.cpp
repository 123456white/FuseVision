#include "SessionManager.h"
#include "Logger.h"
#include "PermissionRegistry.h"

/*
 * SessionManager.cpp — 会话中枢的实现
 *
 * 登录流程：
 *   1. LoginDialog 验证成功后调用 SessionManager::login(uid, name, role)
 *   2. 内部自动调用 PermissionRegistry::refreshPermissions(uid) 预热缓存
 *   3. 发射 sessionChanged() 信号 → FuseVision 刷新状态栏 / UserManagementWidget 校验权限
 *
 * 登出流程：
 *   1. 调用 logout() → 清空状态 → 发射 sessionChanged()
 *   2. 所有监听的 Widget 自行处理"无登录态"的 UI 表现
 */

SessionManager& SessionManager::instance()
{
    static SessionManager inst;
    return inst;
}

SessionManager::SessionManager()
    : QObject(nullptr) {}

void SessionManager::login(int userId, const QString& username, int role)
{
    m_userId   = userId;
    m_username = username;
    m_role     = role;

    PermissionRegistry::instance().refreshPermissions(userId);

    Logger::info(QString("Session started: %1 (id=%2, role=%3)")
                     .arg(username).arg(userId).arg(role));
    emit sessionChanged();
}

void SessionManager::logout()
{
    Logger::info(QString("Session ended: %1").arg(m_username));

    m_userId   = -1;
    m_username.clear();
    m_role     = 0;

    emit sessionChanged();
}

bool    SessionManager::isLoggedIn()        const { return m_userId >= 0; }
int     SessionManager::currentUserId()     const { return m_userId; }
QString SessionManager::currentUsername()   const { return m_username; }
int     SessionManager::currentUserRole()     const { return m_role; }
