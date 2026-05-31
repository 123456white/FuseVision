#include "PermissionRegistry.h"
#include "DatabaseManager.h"
#include "Logger.h"

/*
 * PermissionRegistry.cpp — 权限缓存层的实现
 *
 * 查询策略：缓存优先（cachedOrDefault），若缓存未命中则返回默认 PermissionInfo{false,false}
 *           即"无记录则无权限"的严格策略，而非回退到 DB 查询
 * 缓存更新：registerModule() 时自动为所有用户补齐权限记录并刷新全量缓存
 *           refreshPermissions() 允许外部手动刷新单用户缓存
 */

PermissionRegistry& PermissionRegistry::instance()
{
    static PermissionRegistry inst;
    return inst;
}

PermissionRegistry::PermissionRegistry()
    : QObject(nullptr) {}

PermissionRegistry::~PermissionRegistry() = default;

/* ── 模块注册 / 注销 ──────────────────────────────────────────── */

void PermissionRegistry::registerModule(const QString& moduleName)
{
    if (m_modules.contains(moduleName)) {
        Logger::warn(QString("Module '%1' already registered, skipping.").arg(moduleName));
        return;
    }

    m_modules.append(moduleName);
    DatabaseManager::instance().initModulePermissions(moduleName);

    const auto userIds = DatabaseManager::instance().allUsers();
    for (const auto& u : userIds)
        refreshPermissions(u.id);

    Logger::info(QString("Module registered: %1").arg(moduleName));
    emit moduleRegistered(moduleName);
}

void PermissionRegistry::unregisterModule(const QString& moduleName)
{
    if (!m_modules.contains(moduleName)) return;

    m_modules.removeAll(moduleName);
    m_cache.clear();                                         // 注销后清全量缓存，防止脏读

    Logger::info(QString("Module unregistered: %1").arg(moduleName));
    emit moduleUnregistered(moduleName);
}

/* ── 查询 ─────────────────────────────────────────────────────── */

QStringList PermissionRegistry::modules() const      { return m_modules; }
bool PermissionRegistry::hasModule(const QString& m) const { return m_modules.contains(m); }

/* ── 权限校验（缓存 -> DB 回退） ────────────────────────────────── */

static PermissionInfo cachedOrDefault(
    const QMap<int, QMap<QString, PermissionInfo>>& cache, int userId, const QString& module)
{
    auto userIt = cache.constFind(userId);
    if (userIt != cache.constEnd()) {
        auto modIt = userIt->constFind(module);
        if (modIt != userIt->constEnd())
            return *modIt;
    }
    return {};                                               // 默认 {false, false} —— 无记录无权限
}

bool PermissionRegistry::canRead(int userId, const QString& moduleName)
{
    return cachedOrDefault(m_cache, userId, moduleName).canRead;
}

bool PermissionRegistry::canWrite(int userId, const QString& moduleName)
{
    return cachedOrDefault(m_cache, userId, moduleName).canWrite;
}

/* ── 缓存同步 ─────────────────────────────────────────────────── */

void PermissionRegistry::refreshPermissions(int userId)
{
    auto perms = DatabaseManager::instance().getPermissions(userId);
    m_cache[userId] = perms;
}
