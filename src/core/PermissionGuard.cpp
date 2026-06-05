#include "PermissionGuard.h"
#include "PermissionRegistry.h"
#include "SessionManager.h"
#include "DatabaseManager.h"

// =============================================================================
// PermissionGuard.cpp — Widget 权限代理实现
// =============================================================================
// 核心机制：
//   1. watch() 初始化权限快照（m_state），并连接 SessionManager::sessionChanged
//   2. 用户登录/登出 → sessionChanged → refresh()
//   3. refresh() 从 PermissionRegistry 拉取最新权限，与快照对比
//   4. 如有差异 → 发射 changed() → Widget::applyPermissions() 刷新 UI
// =============================================================================

// ── 构造 ──────────────────────────────────────────────────────

PermissionGuard::PermissionGuard(QObject* parent)
    : QObject(parent)
{
}

// ── 注册关注的权限点 ───────────────────────────────────────────

void PermissionGuard::watch(const QString& moduleKey)
{
    watch(QStringList{moduleKey});  // 转发到批量重载
}

void PermissionGuard::watch(const QStringList& moduleKeys)
{
    // 步骤 1：初始化权限快照 — 从 PermissionRegistry 内存缓存拷贝当前权限
    for (const QString& key : moduleKeys) {
        PermissionInfo info;
        info.canRead  = PermissionRegistry::instance().canRead(currentUserId(), key);
        info.canWrite = PermissionRegistry::instance().canWrite(currentUserId(), key);
        m_state[key]  = info;
    }

    // 步骤 2：自动连接会话变更信号（Qt::UniqueConnection 防止重复连接）
    //        每次登录/登出 → refresh() 重新拉取权限
    connect(&SessionManager::instance(), &SessionManager::sessionChanged,
            this, &PermissionGuard::refresh, Qt::UniqueConnection);
}

// ── 权限查询（O(log n) QMap 查找） ─────────────────────────────────

bool PermissionGuard::canRead(const QString& moduleKey) const
{
    auto it = m_state.constFind(moduleKey);
    return (it != m_state.constEnd()) ? it->canRead : false;  // 未注册的模块默认无权限
}

bool PermissionGuard::canWrite(const QString& moduleKey) const
{
    auto it = m_state.constFind(moduleKey);
    return (it != m_state.constEnd()) ? it->canWrite : false;
}

QStringList PermissionGuard::watchedKeys() const
{
    return m_state.keys();  // 返回所有已 watch 的模块键
}

// ── 权限刷新（Authentication → Authorization） ────────────────────

void PermissionGuard::refresh()
{
    int uid = currentUserId();  // 委托 SessionManager 获取当前用户 ID
    if (uid < 0) return;        // 未登录，无需刷新
    bool anyChanged = false;

    // 步骤 1：一次 DB 查询拉取当前用户全部权限 → 刷新 Registry 缓存
    PermissionRegistry::instance().refreshPermissions(uid);

    // 步骤 2：遍历关注的模块，从缓存对比（纯内存操作，无 DB 查询）
    for (auto it = m_state.begin(); it != m_state.end(); ++it) {
        PermissionInfo fresh;
        fresh.canRead  = PermissionRegistry::instance().canRead(uid, it.key());
        fresh.canWrite = PermissionRegistry::instance().canWrite(uid, it.key());
        if (fresh.canRead != it->canRead || fresh.canWrite != it->canWrite) {
            it->canRead  = fresh.canRead;
            it->canWrite = fresh.canWrite;
            anyChanged   = true;  // 标记有变更，需要通知 Widget
        }
    }

    if (anyChanged)
        emit changed();  // 发射信号 → 所有连接的 Widget::applyPermissions() 被调用
}

int PermissionGuard::currentUserId() const
{
    return SessionManager::instance().currentUserId();  // 未登录时返回 -1
}
