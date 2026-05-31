#include "SettingsManager.h"

SettingsManager& SettingsManager::instance()
{
    static SettingsManager inst;
    return inst;
}

SettingsManager::SettingsManager()
    : m_settings("FuseVisionTeam", "FuseVision")
{
    load();
}

void SettingsManager::load()
{
    m_logPath  = m_settings.value("paths/log",     "").toString();
    m_dbPath   = m_settings.value("paths/database", "").toString();
}

void SettingsManager::save()
{
    m_settings.setValue("paths/log",      m_logPath);
    m_settings.setValue("paths/database", m_dbPath);
    m_settings.sync();
    m_dirty = true;
}

QString SettingsManager::logPath()    const { return m_logPath; }
void    SettingsManager::setLogPath(const QString& p) { m_logPath = p; }
QString SettingsManager::dbPath()     const { return m_dbPath; }
void    SettingsManager::setDbPath(const QString& p)  { m_dbPath  = p; }
bool    SettingsManager::needsRestart() const { return m_dirty; }
void    SettingsManager::setNeedsRestart(bool v) { m_dirty = v; }
