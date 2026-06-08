#include "SystemSettingsWidget.h"
#include "core/Logger.h"
#include "core/SettingsManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QFrame>
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
    mainLayout->setContentsMargins(24, 8, 24, 20);
    mainLayout->setSpacing(12);

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

    // ══════ 模块数据存储（左右双栏） ══════
    QGroupBox* dataGroup = new QGroupBox("模块数据存储");
    QHBoxLayout* dataLayout = new QHBoxLayout(dataGroup);
    dataLayout->setSpacing(16);

    // ── 左栏：深度学习 ──
    QFrame* dlFrame = new QFrame;
    dlFrame->setObjectName("sectionFrame");
    dlFrame->setMinimumWidth(220);

    QVBoxLayout* dlLayout = new QVBoxLayout(dlFrame);
    dlLayout->setContentsMargins(8, 0, 8, 8);
    dlLayout->setSpacing(6);

    QLabel* dlTitle = new QLabel("深度学习");
    dlTitle->setObjectName("sectionTitle");
    dlLayout->addWidget(dlTitle);

    QFormLayout* dlForm = new QFormLayout;
    dlForm->setSpacing(8);

    m_dlDataPathEdit = new QLineEdit;
    QPushButton* browseDlDataBtn = new QPushButton("浏览...");
    QHBoxLayout* dlDataRow = new QHBoxLayout;
    dlDataRow->addWidget(m_dlDataPathEdit);
    dlDataRow->addWidget(browseDlDataBtn);
    dlForm->addRow("数据路径:", dlDataRow);

    m_dlModelPathEdit = new QLineEdit;
    QPushButton* browseDlModelBtn = new QPushButton("浏览...");
    QHBoxLayout* dlModelRow = new QHBoxLayout;
    dlModelRow->addWidget(m_dlModelPathEdit);
    dlModelRow->addWidget(browseDlModelBtn);
    dlForm->addRow("模型路径:", dlModelRow);

    m_dlDatasetPathEdit = new QLineEdit;
    QPushButton* browseDlDatasetBtn = new QPushButton("浏览...");
    QHBoxLayout* dlDatasetRow = new QHBoxLayout;
    dlDatasetRow->addWidget(m_dlDatasetPathEdit);
    dlDatasetRow->addWidget(browseDlDatasetBtn);
    dlForm->addRow("数据集路径:", dlDatasetRow);

    dlLayout->addLayout(dlForm);
    dlLayout->addStretch();

    // ── 右栏：传统视觉 ──
    QFrame* traditionalFrame = new QFrame;
    traditionalFrame->setObjectName("sectionFrame");
    traditionalFrame->setMinimumWidth(220);

    QVBoxLayout* traditionalLayout = new QVBoxLayout(traditionalFrame);
    traditionalLayout->setContentsMargins(8, 0, 8, 8);
    traditionalLayout->setSpacing(6);

    QLabel* traditionalTitle = new QLabel("传统视觉");
    traditionalTitle->setObjectName("sectionTitle");
    traditionalLayout->addWidget(traditionalTitle);

    QFormLayout* traditionalForm = new QFormLayout;
    traditionalForm->setSpacing(8);

    m_traditionalDataPathEdit = new QLineEdit;
    QPushButton* browseTradDataBtn = new QPushButton("浏览...");
    QHBoxLayout* tradDataRow = new QHBoxLayout;
    tradDataRow->addWidget(m_traditionalDataPathEdit);
    tradDataRow->addWidget(browseTradDataBtn);
    traditionalForm->addRow("数据路径:", tradDataRow);

    m_traditionalCameraPathEdit = new QLineEdit;
    QPushButton* browseTradCameraBtn = new QPushButton("浏览...");
    QHBoxLayout* tradCameraRow = new QHBoxLayout;
    tradCameraRow->addWidget(m_traditionalCameraPathEdit);
    tradCameraRow->addWidget(browseTradCameraBtn);
    traditionalForm->addRow("相机配置:", tradCameraRow);

    traditionalLayout->addLayout(traditionalForm);
    traditionalLayout->addStretch();

    dataLayout->addWidget(dlFrame, 1);
    dataLayout->addWidget(traditionalFrame, 1);
    mainLayout->addWidget(dataGroup, 1);  // 弹性拉伸填满剩余空间

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

    // 信号连接
    connect(browseLogBtn, &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseLogPath);
    connect(browseDbBtn,  &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseDbPath);
    connect(browseDlDataBtn,    &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseDlData);
    connect(browseDlModelBtn,   &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseDlModel);
    connect(browseDlDatasetBtn, &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseDlDataset);
    connect(browseTradDataBtn,  &QPushButton::clicked, this, &SystemSettingsWidget::onBrowseTraditionalData);
    connect(browseTradCameraBtn,&QPushButton::clicked, this, &SystemSettingsWidget::onBrowseTraditionalCamera);
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

    // 深度学习路径
    QString dlData = s.dlDataPath();
    if (dlData.isEmpty()) dlData = QCoreApplication::applicationDirPath() + "/dl_data";
    m_dlDataPathEdit->setText(QDir::toNativeSeparators(dlData));

    QString dlModel = s.dlModelPath();
    if (dlModel.isEmpty()) dlModel = QCoreApplication::applicationDirPath() + "/dl_models";
    m_dlModelPathEdit->setText(QDir::toNativeSeparators(dlModel));

    QString dlDataset = s.dlDatasetPath();
    if (dlDataset.isEmpty()) dlDataset = QCoreApplication::applicationDirPath() + "/dl_datasets";
    m_dlDatasetPathEdit->setText(QDir::toNativeSeparators(dlDataset));

    // 传统视觉路径
    QString tradData = s.traditionalDataPath();
    if (tradData.isEmpty()) tradData = QCoreApplication::applicationDirPath() + "/traditional_data";
    m_traditionalDataPathEdit->setText(QDir::toNativeSeparators(tradData));

    QString tradCamera = s.traditionalCameraPath();
    if (tradCamera.isEmpty()) tradCamera = QCoreApplication::applicationDirPath() + "/traditional_camera";
    m_traditionalCameraPathEdit->setText(QDir::toNativeSeparators(tradCamera));
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

void SystemSettingsWidget::onBrowseDlData()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择 DL 数据目录",
        m_dlDataPathEdit->text());
    if (!dir.isEmpty()) m_dlDataPathEdit->setText(QDir::toNativeSeparators(dir));
}

void SystemSettingsWidget::onBrowseDlModel()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择 DL 模型目录",
        m_dlModelPathEdit->text());
    if (!dir.isEmpty()) m_dlModelPathEdit->setText(QDir::toNativeSeparators(dir));
}

void SystemSettingsWidget::onBrowseDlDataset()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择 DL 数据集目录",
        m_dlDatasetPathEdit->text());
    if (!dir.isEmpty()) m_dlDatasetPathEdit->setText(QDir::toNativeSeparators(dir));
}

void SystemSettingsWidget::onBrowseTraditionalData()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择传统视觉数据目录",
        m_traditionalDataPathEdit->text());
    if (!dir.isEmpty()) m_traditionalDataPathEdit->setText(QDir::toNativeSeparators(dir));
}

void SystemSettingsWidget::onBrowseTraditionalCamera()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择相机配置目录",
        m_traditionalCameraPathEdit->text());
    if (!dir.isEmpty()) m_traditionalCameraPathEdit->setText(QDir::toNativeSeparators(dir));
}

// ── 保存 / 重置 ──────────────────────────────────────────────

void SystemSettingsWidget::onSave()
{
    auto& s = SettingsManager::instance();
    // 写入内存（路径 + 日志级别）
    s.setLogPath(QDir::fromNativeSeparators(m_logPathEdit->text()));
    s.setDbPath(QDir::fromNativeSeparators(m_dbPathEdit->text()));
    s.setLogLevel(m_logLevelCombo->currentData().toInt());
    s.setDlDataPath(QDir::fromNativeSeparators(m_dlDataPathEdit->text()));
    s.setDlModelPath(QDir::fromNativeSeparators(m_dlModelPathEdit->text()));
    s.setDlDatasetPath(QDir::fromNativeSeparators(m_dlDatasetPathEdit->text()));
    s.setTraditionalDataPath(QDir::fromNativeSeparators(m_traditionalDataPathEdit->text()));
    s.setTraditionalCameraPath(QDir::fromNativeSeparators(m_traditionalCameraPathEdit->text()));
    // 持久化到磁盘
    s.save();

    // 日志路径 + 级别立即生效（重新初始化 Logger）
    Logger::init(QDir::fromNativeSeparators(m_logPathEdit->text()),
                 static_cast<Logger::Level>(s.logLevel()));
    Logger::info(QString("System settings saved (log level: %1)").arg(s.logLevel()));
    QMessageBox::information(this, "配置已保存",
        "所有配置已保存并立即生效。\n\n"
        "注意：数据库路径变更将在下次启动时生效（当前连接已打开）。");
}

void SystemSettingsWidget::onReset()
{
    auto& s = SettingsManager::instance();

    // 恢复默认值
    QString defaultLogPath = QCoreApplication::applicationDirPath() + "/logs";
    QString defaultDbPath  = QCoreApplication::applicationDirPath() + "/data";

    s.setLogPath(defaultLogPath);
    s.setDbPath(defaultDbPath);
    s.setDlDataPath(QCoreApplication::applicationDirPath() + "/dl_data");
    s.setDlModelPath(QCoreApplication::applicationDirPath() + "/dl_models");
    s.setDlDatasetPath(QCoreApplication::applicationDirPath() + "/dl_datasets");
    s.setTraditionalDataPath(QCoreApplication::applicationDirPath() + "/traditional_data");
    s.setTraditionalCameraPath(QCoreApplication::applicationDirPath() + "/traditional_camera");
    s.save();

    loadSettings();  // 刷新 UI 显示
    Logger::info("System settings reset to defaults");
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
