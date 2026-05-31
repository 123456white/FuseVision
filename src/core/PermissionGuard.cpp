#include "PermissionGuard.h"
#include "PermissionRegistry.h"
#include "SessionManager.h"
#include "DatabaseManager.h"

PermissionGuard::PermissionGuard(QObject* parent)
    : QObject(parent)
{
}

void PermissionGuard::watch(const QString& moduleKey)
{
    watch(QStringList{moduleKey});
}

void PermissionGuard::watch(const QStringList& moduleKeys)
{
    for (const QString& key : moduleKeys) {
        PermissionInfo info;
        info.canRead  = PermissionRegistry::instance().canRead(currentUserId(), key);
        info.canWrite = PermissionRegistry::instance().canWrite(currentUserId(), key);
        m_state[key]  = info;
    }

    connect(&SessionManager::instance(), &SessionManager::sessionChanged,
            this, &PermissionGuard::refresh, Qt::UniqueConnection);
}

bool PermissionGuard::canRead(const QString& moduleKey) const
{
    auto it = m_state.constFind(moduleKey);
    return (it != m_state.constEnd()) ? it->canRead : false;
}

bool PermissionGuard::canWrite(const QString& moduleKey) const
{
    auto it = m_state.constFind(moduleKey);
    return (it != m_state.constEnd()) ? it->canWrite : false;
}

QStringList PermissionGuard::watchedKeys() const
{
    return m_state.keys();
}

void PermissionGuard::refresh()
{
    int uid = currentUserId();
    bool anyChanged = false;

    for (auto it = m_state.begin(); it != m_state.end(); ++it) {
        PermissionInfo fresh;
        fresh.canRead  = PermissionRegistry::instance().canRead(uid, it.key());
        fresh.canWrite = PermissionRegistry::instance().canWrite(uid, it.key());
        if (fresh.canRead != it->canRead || fresh.canWrite != it->canWrite) {
            it->canRead  = fresh.canRead;
            it->canWrite = fresh.canWrite;
            anyChanged   = true;
        }
    }

    if (anyChanged)
        emit changed();
}

int PermissionGuard::currentUserId() const
{
    return SessionManager::instance().currentUserId();
}
