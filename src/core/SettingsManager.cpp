#include "SettingsManager.h"

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
    : m_settings("FuseVisionTeam", "FuseVision")  // 组织名/应用名 → Windows 注册表 or ini
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

void SettingsManager::save()
{
    // 将内存中的路径写入 QSettings 并强制同步到磁盘
    m_settings.setValue("paths/log",          m_logPath);
    m_settings.setValue("paths/database",     m_dbPath);
    m_settings.setValue("paths/project_data", m_projectDataPath);
    m_settings.setValue("paths/traditional_data",  m_traditionalDataPath);
    m_settings.setValue("paths/traditional_camera", m_traditionalCameraPath);
    m_settings.setValue("tools/labelme_python",   m_labelmePythonPath);
    m_settings.setValue("logger/level",       m_logLevel);
    m_settings.sync();  // 立即落盘，避免进程崩溃丢失配置
}

// ── 属性存取 ──────────────────────────────────────────────────

QString SettingsManager::logPath()    const { return m_logPath; }
void    SettingsManager::setLogPath(const QString& p) { m_logPath = p; }
QString SettingsManager::dbPath()     const { return m_dbPath; }
void    SettingsManager::setDbPath(const QString& p)  { m_dbPath  = p; }
int     SettingsManager::logLevel()   const { return m_logLevel; }
void    SettingsManager::setLogLevel(int level)       { m_logLevel = level; }
QString SettingsManager::projectDataPath()  const { return m_projectDataPath; }
void    SettingsManager::setProjectDataPath(const QString& p) { m_projectDataPath = p; }
QString SettingsManager::traditionalDataPath()  const { return m_traditionalDataPath; }
void    SettingsManager::setTraditionalDataPath(const QString& p) { m_traditionalDataPath = p; }
QString SettingsManager::traditionalCameraPath() const { return m_traditionalCameraPath; }
void    SettingsManager::setTraditionalCameraPath(const QString& p) { m_traditionalCameraPath = p; }
QString SettingsManager::labelmePythonPath() const { return m_labelmePythonPath; }
void    SettingsManager::setLabelmePythonPath(const QString& p) { m_labelmePythonPath = p; }
