#include "PermissionGuard.h"
#include "core/AccessControl.h"

PermissionGuard::PermissionGuard(QObject* parent):QObject(parent) {
    connect(&SessionManager::instance(),&SessionManager::sessionChanged,this,&PermissionGuard::refresh);
}
void PermissionGuard::watch(const QString& key) { watch(QStringList{key}); }
void PermissionGuard::watch(const QStringList& keys) {
    for(const auto& key:keys) if(!m_keys.contains(key)) m_keys.append(key);
}
bool PermissionGuard::canRead(const QString& key) const { return AccessControl::canRead(key); }
bool PermissionGuard::canWrite(const QString& key) const { return AccessControl::canWrite(key); }
QStringList PermissionGuard::watchedKeys() const { return m_keys; }
int PermissionGuard::currentUserId() const { return SessionManager::instance().currentUserId(); }
void PermissionGuard::refresh() { emit changed(); }
