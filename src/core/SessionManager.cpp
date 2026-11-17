#include "SessionManager.h"
#include "DatabaseManager.h"
#include "Logger.h"

SessionManager& SessionManager::instance() { static SessionManager inst;return inst; }
void SessionManager::login(int userId) {
    const auto account=DatabaseManager::instance().singleAccount();
    if(userId<0 || account.id!=userId) return;
    m_userId=userId;m_username=account.username;
    Logger::info("Editing unlocked: "+m_username);
    emit sessionChanged();
}
void SessionManager::logout() {
    m_userId=-1;m_username.clear();
    Logger::info("Read-only guest session");
    emit sessionChanged();
}
bool SessionManager::isLoggedIn() const { return m_userId>=0; }
int SessionManager::currentUserId() const { return m_userId; }
QString SessionManager::currentUsername() const { return m_username; }
void SessionManager::broadcastSession() { emit sessionChanged(); }
