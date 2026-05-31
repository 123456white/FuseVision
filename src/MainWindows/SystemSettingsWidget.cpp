#include "SystemSettingsWidget.h"
#include "core/Logger.h"
#include "core/SettingsManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>

SystemSettingsWidget::SystemSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();
    loadSettings();
    Logger::info("SystemSettingsWidget initialized");
}

void SystemSettingsWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel* titleLabel = new QLabel("系统设置");
    titleLabel->setObjectName("widgetTitle");
    mainLayout->addWidget(titleLabel);

    QGroupBox* pathGroup = new QGroupBox("存储路径");
    QFormLayout* pathForm = new QFormLayout(pathGroup);

    m_logPathEdit = new QLineEdit;
    m_logPathEdit->setMinimumWidth(320);
    QPushButton* browseLogBtn = new QPushButton("浏览...");
    QHBoxLayout* logLayout = new QHBoxLayout;
    logLayout->addWidget(m_logPathEdit);
    logLayout->addWidget(browseLogBtn);
    pathForm->addRow("日志存储路径:", logLayout);

    m_dbPathEdit = new QLineEdit;
    m_dbPathEdit->setMinimumWidth(320);
    QPushButton* browseDbBtn = new QPushButton("浏览...");
    QHBoxLayout* dbLayout = new QHBoxLayout;
    dbLayout->addWidget(m_dbPathEdit);
    dbLayout->addWidget(browseDbBtn);
    pathForm->addRow("数据库存储路径:", dbLayout);

    mainLayout->addWidget(pathGroup);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_resetBtn = new QPushButton("重置");
    m_saveBtn  = new QPushButton("保存配置");
    m_saveBtn->setObjectName("primaryBtn");

    btnLayout->addWidget(m_resetBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    m_restartLabel = new QLabel;
    m_restartLabel->setObjectName("restartLabel");
    m_restartLabel->setVisible(false);
    mainLayout->addWidget(m_restartLabel);

    mainLayout->addStretch();

    connect(browseLogBtn, &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseLogPath);
    connect(browseDbBtn,  &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseDbPath);
    connect(m_saveBtn,    &QPushButton::clicked, this, &SystemSettingsWidget::onSave);
    connect(m_resetBtn,   &QPushButton::clicked, this, &SystemSettingsWidget::onReset);
}

void SystemSettingsWidget::loadSettings()
{
    auto& s = SettingsManager::instance();

    QString logPath = s.logPath();
    if (logPath.isEmpty())
        logPath = QCoreApplication::applicationDirPath() + "/logs";
    m_logPathEdit->setText(QDir::toNativeSeparators(logPath));

    QString dbPath = s.dbPath();
    if (dbPath.isEmpty())
        dbPath = QCoreApplication::applicationDirPath() + "/data";
    m_dbPathEdit->setText(QDir::toNativeSeparators(dbPath));

    refreshRestartNotice();
}

void SystemSettingsWidget::onBrowseLogPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择日志存储目录",
        m_logPathEdit->text());
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

void SystemSettingsWidget::onSave()
{
    auto& s = SettingsManager::instance();
    s.setLogPath(QDir::fromNativeSeparators(m_logPathEdit->text()));
    s.setDbPath(QDir::fromNativeSeparators(m_dbPathEdit->text()));
    s.save();
    s.setNeedsRestart(true);
    refreshRestartNotice();

    Logger::info("System settings saved");
    QMessageBox::information(this, "配置已保存",
        "配置已保存。路径变更将在下次启动时生效。");
}

void SystemSettingsWidget::onReset()
{
    auto& s = SettingsManager::instance();

    QString defaultLogPath = QCoreApplication::applicationDirPath() + "/logs";
    QString defaultDbPath  = QCoreApplication::applicationDirPath() + "/data";

    s.setLogPath(defaultLogPath);
    s.setDbPath(defaultDbPath);
    s.save();

    loadSettings();
    Logger::info("System settings reset to defaults");
}

void SystemSettingsWidget::refreshRestartNotice()
{
    if (SettingsManager::instance().needsRestart()) {
        m_restartLabel->setText("⚠ 路径变更需要重启应用才能生效");
        m_restartLabel->setVisible(true);
    } else {
        m_restartLabel->setVisible(false);
    }
}
