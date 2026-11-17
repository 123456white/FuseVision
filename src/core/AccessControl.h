/** @file
 * @brief AccessControl 的公共声明。
 * @ingroup core
 */
#pragma once
#include "SessionManager.h"
#include <functional>

// All pages are readable. One authenticated session unlocks every business write.
/** @brief 无界面授权策略：全部可读，已登录才可写；key 只描述操作。
 * @ingroup core
 */
namespace AccessControl {
inline bool canRead(const QString&) { return true; }
inline bool canWrite(const QString&) { return SessionManager::instance().isLoggedIn(); }
inline void watch(QObject* owner, const std::function<void()>& refresh) {
    QObject::connect(&SessionManager::instance(),&SessionManager::sessionChanged,owner,refresh);
}
}
