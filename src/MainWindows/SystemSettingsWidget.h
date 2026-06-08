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
// 作为 QStackedWidget 的第 4 页，提供日志路径 / 数据库路径 / 模块数据路径配置。
// （主题切换由左侧边栏控制，不在此页重复提供）
// =============================================================================

class SystemSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SystemSettingsWidget(QWidget* parent = nullptr);

private slots:
    void onBrowseLogPath();     // 浏览日志目录（QFileDialog）
    void onBrowseDbPath();      // 浏览数据库目录
    void onBrowseDlData();      // 浏览 DL 数据目录
    void onBrowseDlModel();     // 浏览 DL 模型目录
    void onBrowseDlDataset();   // 浏览 DL 数据集目录
    void onBrowseTraditionalData();   // 浏览传统视觉数据目录
    void onBrowseTraditionalCamera(); // 浏览传统视觉相机配置目录
    void onSave();              // 保存配置 → SettingsManager + 弹窗提示
    void onReset();             // 恢复默认路径并保存
    void onCleanLogs();         // 清理 N 天前的旧日志

private:
    void initUI();              // 构建路径输入框 / 浏览按钮 / 保存重置按钮
    void loadSettings();        // 从 SettingsManager 加载当前路径到 UI

    QLineEdit*   m_logPathEdit   = nullptr;  // 日志路径输入框
    QLineEdit*   m_dbPathEdit    = nullptr;  // 数据库路径输入框
    QComboBox*   m_logLevelCombo = nullptr;  // 日志级别下拉框

    QPushButton* m_saveBtn  = nullptr;  // "保存配置"按钮
    QPushButton* m_resetBtn = nullptr;  // "重置"按钮
    QPushButton* m_cleanBtn = nullptr;  // "清理旧日志"按钮

    // ── 深度学习设置 ──────────────────────────────────────────
    QLineEdit* m_dlDataPathEdit    = nullptr;  // DL 数据路径
    QLineEdit* m_dlModelPathEdit   = nullptr;  // DL 模型路径
    QLineEdit* m_dlDatasetPathEdit = nullptr;  // DL 数据集路径

    // ── 传统视觉设置 ──────────────────────────────────────────
    QLineEdit* m_traditionalDataPathEdit   = nullptr;  // 传统视觉数据路径
    QLineEdit* m_traditionalCameraPathEdit = nullptr;  // 传统视觉相机配置路径
};

#endif // SYSTEMSETTINGSWIDGET_H