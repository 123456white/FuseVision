#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>
#include <QSettings>

class SettingsManager
{
public:
    static SettingsManager& instance();

    QString logPath()    const;
    void    setLogPath(const QString& path);

    QString dbPath()     const;
    void    setDbPath(const QString& path);

    bool needsRestart()  const;
    void setNeedsRestart(bool v);

    void save();
    void load();

private:
    SettingsManager();
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    QSettings m_settings;
    bool      m_dirty = false;

    QString   m_logPath;
    QString   m_dbPath;
};

#endif // SETTINGSMANAGER_H
