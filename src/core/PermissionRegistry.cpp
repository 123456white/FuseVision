#include "PermissionRegistry.h"
#include "SessionManager.h"

PermissionRegistry& PermissionRegistry::instance() { static PermissionRegistry inst; return inst; }
void PermissionRegistry::registerModule(const QString& name) {
    if(m_modules.contains(name)) return;
    m_modules.append(name);
    emit moduleRegistered(name);
}
void PermissionRegistry::unregisterModule(const QString& name) {
    if(m_modules.removeAll(name)) emit moduleUnregistered(name);
}
QStringList PermissionRegistry::modules() const { return m_modules; }
bool PermissionRegistry::hasModule(const QString& name) const { return m_modules.contains(name); }
bool PermissionRegistry::canRead(int, const QString&) { return true; }
bool PermissionRegistry::canWrite(int userId, const QString&) {
    const auto& session=SessionManager::instance();
    return session.isLoggedIn() && userId==session.currentUserId();
}
void PermissionRegistry::refreshPermissions(int userId) { emit permissionsChanged(userId); }
