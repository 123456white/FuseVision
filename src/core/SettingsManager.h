#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>
#include <QSettings>

// =============================================================================
// SettingsManager — 应用配置持久化层（Meyer's Singleton）
// =============================================================================
// 职责：封装 QSettings，管理日志路径 / 数据库路径的读写
//
// 数据流：
//   SystemSettingsWidget（UI）→ setLogPath / setDbPath → save() → QSettings.ini
//   FuseVision / main.cpp（应用启动）→ load() → QSettings → logPath / dbPath
//
// 脏标记机制：
//   needsRestart() 返回 true 表示配置已变更但未重启，
//   SystemSettingsWidget 据此显示"重启生效"提示
// =============================================================================

class SettingsManager
{
public:
    static SettingsManager& instance();

    // ── 公开接口 ──────────────────────────────────────────────
    QString logPath()    const;               // 获取日志存储路径
    void    setLogPath(const QString& path);  // 设置日志存储路径（仅改内存）

    QString dbPath()     const;               // 获取数据库存储路径
    void    setDbPath(const QString& path);   // 设置数据库存储路径（仅改内存）

    int     logLevel()   const;               // 获取日志级别（0=Trace ~ 6=Off）
    void    setLogLevel(int level);           // 设置日志级别（仅改内存）

    bool needsRestart()  const;               // 是否有未生效的配置变更
    void setNeedsRestart(bool v);             // 标记配置已变更需重启

    void save();  // 将内存中的路径持久化到 QSettings，同时 setNeedsRestart(true)
    void load();  // 从 QSettings 恢复路径到内存

private:
    SettingsManager();
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    QSettings m_settings;   // 底层存储（组织名: FuseVisionTeam, 应用名: FuseVision）
    bool      m_dirty = false;  // 脏标记：true = 路径变更后尚未重启

    QString   m_logPath;    // 缓存：日志路径
    QString   m_dbPath;     // 缓存：数据库路径
    int       m_logLevel = 2; // 缓存：日志级别（默认 Info=2）
};

#endif // SETTINGSMANAGER_H
