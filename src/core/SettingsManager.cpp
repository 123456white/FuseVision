#include "SettingsManager.h"
#include <QCoreApplication>

// =============================================================================
// SettingsManager.cpp — QSettings 持久化层的实现
// =============================================================================
// 数据流：
//   写：  Widget → setXxx（内存）→ save() → QSettings.sync() → .ini 文件
//   读：  load()（构造时自动调用）→ QSettings.value() → 内存
//   日志路径：保存后主线程重新初始化 Logger，立即生效
//   数据库路径：SQLite 已打开，保存后下次启动生效
// =============================================================================

// ── 单例（Meyer's Singleton，C++11 线程安全） ──────────────────

SettingsManager& SettingsManager::instance()
{
    static SettingsManager inst;  // 局部静态变量，首次调用时构造，程序退出时析构
    return inst;
}

SettingsManager::SettingsManager()
    : m_settings(QSettings::defaultFormat(), QSettings::UserScope, "FuseVisionTeam", "FuseVision")
{
    load();  // 构造时自动从持久化存储恢复配置
}

// ── 持久化读写 ────────────────────────────────────────────────

void SettingsManager::load()
{
    // 从 QSettings 读取路径，若不存在则返回空字符串（由上层逻辑补默认值）
    m_logPath         = m_settings.value("paths/log",        "").toString();
    m_dbPath          = m_settings.value("paths/database",    "").toString();
    m_projectDataPath = m_settings.value("paths/project_data", "").toString();
    m_traditionalDataPath  = m_settings.value("paths/traditional_data",  "").toString();
    m_traditionalCameraPath = m_settings.value("paths/traditional_camera", "").toString();
    m_labelmePythonPath    = m_settings.value("tools/labelme_python",   "").toString();
    m_logLevel        = m_settings.value("logger/level",       2).toInt();  // 默认 Info
}

bool SettingsManager::save()
{
    QSettings target(m_settings.fileName(),m_settings.format());
    const QStringList keys{"paths/log","paths/database","paths/project_data","paths/traditional_data","paths/traditional_camera","tools/labelme_python","logger/level"};
    QMap<QString,QVariant> previous;
    for(const auto& key:keys) if(target.contains(key)) previous[key]=target.value(key);
    // 将内存中的路径写入 QSettings 并强制同步到磁盘
    target.setValue("paths/log",          m_logPath);
    target.setValue("paths/database",     m_dbPath);
    target.setValue("paths/project_data", m_projectDataPath);
    target.setValue("paths/traditional_data",  m_traditionalDataPath);
    target.setValue("paths/traditional_camera", m_traditionalCameraPath);
    target.setValue("tools/labelme_python",   m_labelmePythonPath);
    target.setValue("logger/level",       m_logLevel);
    target.sync();  // 立即落盘，避免进程崩溃丢失配置
    if(target.status()==QSettings::NoError) return true;
    // Restore pending values as well as persisted values; load() must not read a failed save from QSettings' cache.
    for(const auto& key:keys) {
        if(previous.contains(key)) target.setValue(key,previous[key]);else target.remove(key);
    }
    target.sync();return false;
}

// ── 属性存取 ──────────────────────────────────────────────────

QString SettingsManager::logPath()    const { return m_logPath; }
void    SettingsManager::setLogPath(const QString& p) { m_logPath = p; }
QString SettingsManager::dbPath()     const { return m_dbPath; }
void    SettingsManager::setDbPath(const QString& p)  { m_dbPath  = p; }
int     SettingsManager::logLevel()   const { return m_logLevel; }
void    SettingsManager::setLogLevel(int level)       { m_logLevel = level; }
QString SettingsManager::projectDataPath()  const { return m_projectDataPath; }
QString SettingsManager::activeProjectDataPath() {
    if(m_activeProjectDataPath.isEmpty()) m_activeProjectDataPath=m_projectDataPath.isEmpty()
        ?QCoreApplication::applicationDirPath()+"/project_data":m_projectDataPath;
    return m_activeProjectDataPath;
}
void    SettingsManager::setProjectDataPath(const QString& p) { m_projectDataPath = p; }
QString SettingsManager::traditionalDataPath()  const { return m_traditionalDataPath; }
void    SettingsManager::setTraditionalDataPath(const QString& p) { m_traditionalDataPath = p; }
QString SettingsManager::traditionalCameraPath() const { return m_traditionalCameraPath; }
void    SettingsManager::setTraditionalCameraPath(const QString& p) { m_traditionalCameraPath = p; }
QString SettingsManager::labelmePythonPath() const { return m_labelmePythonPath; }
void    SettingsManager::setLabelmePythonPath(const QString& p) { m_labelmePythonPath = p; }
