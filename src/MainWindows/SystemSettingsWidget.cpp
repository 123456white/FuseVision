#include "SystemSettingsWidget.h"
#include "core/Logger.h"
#include "core/SettingsManager.h"
#include "core/ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QFileInfo>
#include <QDateTime>

// =============================================================================
// SystemSettingsWidget.cpp — 系统设置实现
// =============================================================================
// 本 Widget 作为主窗口 QStackedWidget 的第 4 页，数据流向：
//   UI → SettingsManager（单例，内存）→ save() → QSettings（磁盘）
//   下次启动：SettingsManager 构造 → load() → QSettings → 内存
// =============================================================================

// ── 构造 ──────────────────────────────────────────────────────

SystemSettingsWidget::SystemSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();       // 构建 UI
    loadSettings(); // 从 SettingsManager 加载当前配置
    Logger::info("SystemSettingsWidget initialized");
}

// ── UI 构建 ───────────────────────────────────────────────────

void SystemSettingsWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // 标题
    QLabel* titleLabel = new QLabel("系统设置");
    titleLabel->setObjectName("widgetTitle");
    mainLayout->addWidget(titleLabel);

    // 路径配置区（QGroupBox + QFormLayout）
    QGroupBox* pathGroup = new QGroupBox("存储路径");
    QFormLayout* pathForm = new QFormLayout(pathGroup);

    // 日志路径行：QLineEdit + "浏览..."按钮
    m_logPathEdit = new QLineEdit;
    m_logPathEdit->setMinimumWidth(320);
    QPushButton* browseLogBtn = new QPushButton("浏览...");
    QHBoxLayout* logLayout = new QHBoxLayout;
    logLayout->addWidget(m_logPathEdit);
    logLayout->addWidget(browseLogBtn);
    pathForm->addRow("日志存储路径:", logLayout);

    // 数据库路径行：QLineEdit + "浏览..."按钮
    m_dbPathEdit = new QLineEdit;
    m_dbPathEdit->setMinimumWidth(320);
    QPushButton* browseDbBtn = new QPushButton("浏览...");
    QHBoxLayout* dbLayout = new QHBoxLayout;
    dbLayout->addWidget(m_dbPathEdit);
    dbLayout->addWidget(browseDbBtn);
    pathForm->addRow("数据库存储路径:", dbLayout);

    mainLayout->addWidget(pathGroup);

    // 日志级别配置区
    QGroupBox* logLevelGroup = new QGroupBox("日志设置");
    QFormLayout* logForm = new QFormLayout(logLevelGroup);

    m_logLevelCombo = new QComboBox;
    m_logLevelCombo->addItem("关闭 (Off)",     Logger::Off);
    m_logLevelCombo->addItem("调试 (Debug)",   Logger::Debug);
    m_logLevelCombo->addItem("信息 (Info)",    Logger::Info);
    m_logLevelCombo->addItem("警告 (Warn)",    Logger::Warn);
    m_logLevelCombo->addItem("错误 (Error)",   Logger::Error);
    logForm->addRow("日志级别:", m_logLevelCombo);

    QLabel* logHint = new QLabel("调试版本同时输出控制台+文件；运行版本仅写文件，此设置立即生效无需重启。");
    logHint->setWordWrap(true);
    logHint->setObjectName("hintLabel");
    logForm->addRow(logHint);

    mainLayout->addWidget(logLevelGroup);

    // 外观设置区（主题选择）
    QGroupBox* themeGroup = new QGroupBox("外观设置");
    QFormLayout* themeForm = new QFormLayout(themeGroup);

    m_themeCombo = new QComboBox;
    m_themeCombo->addItem("跟随系统", -1);
    m_themeCombo->addItem("浅色 (Light)", ThemeManager::Light);
    m_themeCombo->addItem("深色 (Dark)",  ThemeManager::Dark);
    themeForm->addRow("主题:", m_themeCombo);

    mainLayout->addWidget(themeGroup);

    // 按钮行（右对齐）
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_cleanBtn = new QPushButton("清理旧日志");
    m_resetBtn = new QPushButton("重置");
    m_saveBtn  = new QPushButton("保存配置");
    m_saveBtn->setObjectName("primaryBtn");  // objectName 匹配 ThemeManager QSS 规则

    btnLayout->addWidget(m_cleanBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    // 重启提示标签（默认隐藏，路径变更后显示）
    m_restartLabel = new QLabel;
    m_restartLabel->setObjectName("restartLabel");  // 匹配 QSS #restartLabel 规则
    m_restartLabel->setVisible(false);
    mainLayout->addWidget(m_restartLabel);

    mainLayout->addStretch();

    // 信号连接
    connect(browseLogBtn, &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseLogPath);
    connect(browseDbBtn,  &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseDbPath);
    connect(m_saveBtn,    &QPushButton::clicked, this, &SystemSettingsWidget::onSave);
    connect(m_resetBtn,   &QPushButton::clicked, this, &SystemSettingsWidget::onReset);
    connect(m_cleanBtn,   &QPushButton::clicked, this, &SystemSettingsWidget::onCleanLogs);
}

// ── 从 SettingsManager 加载当前配置 ──────────────────────────

void SystemSettingsWidget::loadSettings()
{
    auto& s = SettingsManager::instance();

    // 日志路径：若未设置，默认 <exe>/logs
    QString logPath = s.logPath();
    if (logPath.isEmpty())
        logPath = QCoreApplication::applicationDirPath() + "/logs";
    m_logPathEdit->setText(QDir::toNativeSeparators(logPath));

    // 数据库路径：若未设置，默认 <exe>/data
    QString dbPath = s.dbPath();
    if (dbPath.isEmpty())
        dbPath = QCoreApplication::applicationDirPath() + "/data";
    m_dbPathEdit->setText(QDir::toNativeSeparators(dbPath));

    // 日志级别：从 SettingsManager 恢复上次配置
    int savedLevel = s.logLevel();
    int idx = m_logLevelCombo->findData(savedLevel);
    if (idx >= 0) m_logLevelCombo->setCurrentIndex(idx);

    // 主题：从 ThemeManager 恢复当前主题
    int currentTheme = ThemeManager::instance().currentTheme();
    idx = m_themeCombo->findData(currentTheme);
    if (idx >= 0) m_themeCombo->setCurrentIndex(idx);

    refreshRestartNotice();  // 检查是否有未重启生效的配置
}

// ── 浏览目录 ─────────────────────────────────────────────────

void SystemSettingsWidget::onBrowseLogPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择日志存储目录",
        m_logPathEdit->text());  // 默认打开当前设置的路径
    if (!dir.isEmpty())
        m_logPathEdit->setText(QDir::toNativeSeparators(dir));
}

void SystemSettingsWidget::onBrowseDbPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据库存储目录",
        m_dbPathEdit->text());
    if (!dir.isEmpty())
        m_dbPathEdit->setText(QDir::toNativeSeparators(dir));
}

// ── 保存 / 重置 ──────────────────────────────────────────────

void SystemSettingsWidget::onSave()
{
    auto& s = SettingsManager::instance();
    // 写入内存（路径 + 日志级别）
    s.setLogPath(QDir::fromNativeSeparators(m_logPathEdit->text()));
    s.setDbPath(QDir::fromNativeSeparators(m_dbPathEdit->text()));
    s.setLogLevel(m_logLevelCombo->currentData().toInt());
    // 持久化到磁盘
    s.save();

    // 日志级别立即生效（无需重启，路径变更需重启）
    Logger::setLevel(static_cast<Logger::Level>(s.logLevel()));

    // 主题立即生效
    int themeData = m_themeCombo->currentData().toInt();
    if (themeData >= 0)  // -1 = 跟随系统，不强制设置
        ThemeManager::instance().setTheme(static_cast<ThemeManager::Theme>(themeData));

    s.setNeedsRestart(true);
    refreshRestartNotice();

    Logger::info(QString("System settings saved (log level: %1)").arg(s.logLevel()));
    QMessageBox::information(this, "配置已保存",
        "配置已保存。路径变更将在下次启动时生效，日志级别已立即生效。");
}

void SystemSettingsWidget::onReset()
{
    auto& s = SettingsManager::instance();

    // 恢复默认值
    QString defaultLogPath = QCoreApplication::applicationDirPath() + "/logs";
    QString defaultDbPath  = QCoreApplication::applicationDirPath() + "/data";

    s.setLogPath(defaultLogPath);
    s.setDbPath(defaultDbPath);
    s.save();

    loadSettings();  // 刷新 UI 显示
    Logger::info("System settings reset to defaults");
}

// ── 重启提示 ─────────────────────────────────────────────────

void SystemSettingsWidget::refreshRestartNotice()
{
    if (SettingsManager::instance().needsRestart()) {
        m_restartLabel->setText("⚠ 路径变更需要重启应用才能生效");
        m_restartLabel->setVisible(true);
    } else {
        m_restartLabel->setVisible(false);
    }
}

// ── 清理旧日志 ────────────────────────────────────────────────

void SystemSettingsWidget::onCleanLogs()
{
    // 弹出对话框让用户选择保留天数
    QDialog dlg(this);
    dlg.setWindowTitle("清理旧日志");
    dlg.setFixedSize(320, 120);

    QVBoxLayout* dlgLayout = new QVBoxLayout(&dlg);
    QLabel* hint = new QLabel("删除指定天数之前的旧日志文件：");
    dlgLayout->addWidget(hint);

    QSpinBox* daysSpin = new QSpinBox;
    daysSpin->setRange(1, 365);
    daysSpin->setValue(30);
    daysSpin->setSuffix(" 天前");
    dlgLayout->addWidget(daysSpin);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    QPushButton* cancelBtn = new QPushButton("取消");
    QPushButton* okBtn = new QPushButton("清理");
    okBtn->setObjectName("primaryBtn");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    dlgLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn,     &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    int days = daysSpin->value();
    QString logPath = SettingsManager::instance().logPath();
    if (logPath.isEmpty())
        logPath = QCoreApplication::applicationDirPath() + "/logs";

    QDir logDir(logPath);
    if (!logDir.exists()) {
        Logger::warn(QString("Log directory not found: %1").arg(logPath));
        QMessageBox::warning(this, "清理日志", "日志目录不存在。");
        return;
    }

    // 遍历目录中的 .log 文件
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-days);
    QStringList filters{"fusevision_*.log"};
    QFileInfoList files = logDir.entryInfoList(filters, QDir::Files);

    int deleted = 0;
    for (const QFileInfo& fi : files) {
        if (fi.lastModified() < cutoff) {
            if (QFile::remove(fi.absoluteFilePath())) {
                ++deleted;
                Logger::debug(QString("Deleted old log: %1").arg(fi.fileName()));
            }
        }
    }

    QString msg = deleted > 0
        ? QString("已清理 %1 个旧日志文件（%2 天以前）。").arg(deleted).arg(days)
        : QString("没有需要清理的日志文件（%1 天内）。").arg(days);

    Logger::info(msg);
    QMessageBox::information(this, "清理完成", msg);
}
