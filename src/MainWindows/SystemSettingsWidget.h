#ifndef SYSTEMSETTINGSWIDGET_H
#define SYSTEMSETTINGSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>

// =============================================================================
// SystemSettingsWidget — 系统设置页面
// =============================================================================
// 作为 QStackedWidget 的第 4 页，提供日志路径 / 数据库路径的配置功能。
//
// 数据流：
//   读：loadSettings() → SettingsManager::logPath() / dbPath() → UI 输入框
//   写：onSave() → SettingsManager::setLogPath/setDbPath → save() → 弹窗提示
//
// 重启提示：
//   保存后 SettingsManager::needsRestart() 变为 true，
//   refreshRestartNotice() 显示"需重启应用生效"的黄色提示。
// =============================================================================

class SystemSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SystemSettingsWidget(QWidget* parent = nullptr);

private slots:
    void onBrowseLogPath();     // 浏览日志目录（QFileDialog）
    void onBrowseDbPath();      // 浏览数据库目录
    void onSave();              // 保存配置 → SettingsManager + 弹窗提示
    void onReset();             // 恢复默认路径并保存
    void onCleanLogs();         // 清理 N 天前的旧日志

private:
    void initUI();              // 构建路径输入框 / 浏览按钮 / 保存重置按钮
    void loadSettings();        // 从 SettingsManager 加载当前路径到 UI
    void refreshRestartNotice(); // 根据 needsRestart() 显示/隐藏"需重启"提示

    QLineEdit*   m_logPathEdit   = nullptr;  // 日志路径输入框
    QLineEdit*   m_dbPathEdit    = nullptr;  // 数据库路径输入框
    QComboBox*   m_logLevelCombo = nullptr;  // 日志级别下拉框
    QComboBox*   m_themeCombo    = nullptr;  // 主题选择下拉框
    QLabel*      m_restartLabel  = nullptr;  // "需重启"提示标签

    QPushButton* m_saveBtn  = nullptr;  // "保存配置"按钮
    QPushButton* m_resetBtn = nullptr;  // "重置"按钮
    QPushButton* m_cleanBtn = nullptr;  // "清理旧日志"按钮
};

#endif // SYSTEMSETTINGSWIDGET_H