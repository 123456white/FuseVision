#include "ui/WriteAccess.h"
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
#include <QRegularExpression>

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
    initUI();
    AccessControl::watch(this,[this]() { applyPermissions(); });
    applyPermissions();       // 构建 UI
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

    auto* effectHint=new QLabel(tr("日志路径与级别：保存后立即生效。\n数据库及模块数据路径：重启后生效；不会移动已有项目。\n重置路径仅恢复五项路径，不重置日志级别、Python或主题。"));
    effectHint->setWordWrap(true);pathForm->addRow(effectHint);
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

    m_projectDataPathEdit = new QLineEdit;
    QPushButton* browseDlDataBtn = new QPushButton("浏览...");
    QHBoxLayout* dlDataRow = new QHBoxLayout;
    dlDataRow->addWidget(m_projectDataPathEdit);
    dlDataRow->addWidget(browseDlDataBtn);
    dlForm->addRow("项目数据:", dlDataRow);

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
    m_resetBtn = new QPushButton("重置路径");
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

    // 项目数据路径
    QString projectData = s.projectDataPath();
    if (projectData.isEmpty()) projectData = QCoreApplication::applicationDirPath() + "/project_data";
    m_projectDataPathEdit->setText(QDir::toNativeSeparators(projectData));

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
    QString dir = QFileDialog::getExistingDirectory(this, "选择项目数据根目录",
        m_projectDataPathEdit->text());
    if (!dir.isEmpty()) m_projectDataPathEdit->setText(QDir::toNativeSeparators(dir));
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
    if(!WriteAccess::require("系统设置",this)) return;
    for(auto* edit:{m_logPathEdit,m_dbPathEdit,m_projectDataPathEdit,m_traditionalDataPathEdit,m_traditionalCameraPathEdit}) {
        const auto value=QDir::fromNativeSeparators(edit->text().trimmed());
        if(value.isEmpty() || !QDir::isAbsolutePath(value) || QFileInfo(value).isFile()) {
            QMessageBox::warning(this,tr("路径无效"),tr("请填写绝对目录路径：%1").arg(value));return;
        }
    }
    auto& s=SettingsManager::instance();
    const auto oldLog=s.logPath(); const auto oldLevel=s.logLevel(); QString error;
    const auto oldDb=s.dbPath(),oldProject=s.projectDataPath(),oldTraditional=s.traditionalDataPath(),oldCamera=s.traditionalCameraPath();
    const auto log=QDir::fromNativeSeparators(m_logPathEdit->text().trimmed());
    const auto level=m_logLevelCombo->currentData().toInt();
    if(!Logger::init(log,static_cast<Logger::Level>(level),&error)) {
        QMessageBox::warning(this,tr("日志设置失败"),log+"\n"+error);return;
    }
    s.setLogPath(log);s.setLogLevel(level);
    s.setDbPath(QDir::fromNativeSeparators(m_dbPathEdit->text().trimmed()));
    s.setProjectDataPath(QDir::fromNativeSeparators(m_projectDataPathEdit->text().trimmed()));
    s.setTraditionalDataPath(QDir::fromNativeSeparators(m_traditionalDataPathEdit->text().trimmed()));
    s.setTraditionalCameraPath(QDir::fromNativeSeparators(m_traditionalCameraPathEdit->text().trimmed()));
    if(!s.save()) {
        s.setLogPath(oldLog);s.setLogLevel(oldLevel);s.setDbPath(oldDb);s.setProjectDataPath(oldProject);
        s.setTraditionalDataPath(oldTraditional);s.setTraditionalCameraPath(oldCamera);
        Logger::init(oldLog,static_cast<Logger::Level>(oldLevel));
        QMessageBox::warning(this,tr("配置保存失败"),tr("配置存储不可写，请检查权限后重试。"));return;
    }
    QMessageBox::information(this,tr("配置已保存"),tr("日志路径与级别已立即生效。\n数据库及模块数据路径将在重启后生效；已有项目不会迁移。"));
}

void SystemSettingsWidget::onReset()
{
    if(!WriteAccess::require("系统设置",this)) return;
    if(QMessageBox::question(this,tr("重置路径"),tr("仅重置日志、数据库、项目、传统视觉数据、相机配置这五项路径。\n日志级别、Python与主题保留，已有数据不移动。\n日志立即生效，其余路径重启后生效。"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
    const auto root=QCoreApplication::applicationDirPath();
    m_logPathEdit->setText(root+"/logs");m_dbPathEdit->setText(root+"/data");
    m_projectDataPathEdit->setText(root+"/project_data");
    m_traditionalDataPathEdit->setText(root+"/traditional_data");
    m_traditionalCameraPathEdit->setText(root+"/traditional_camera");
    onSave();
}

void SystemSettingsWidget::onCleanLogs()
{
    if(!WriteAccess::require("系统设置",this)) return;
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

    if(!WriteAccess::require("系统设置",this)) return;

    int days = daysSpin->value();
    QString logPath = Logger::activeDirectory();
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
    const auto files=expiredLogs(logPath,Logger::activeBaseName(),cutoff);

    int deleted = 0; QStringList failures;
    for (const QString& path : files) {
        if(!AccessControl::canWrite("系统设置")) {failures.append(tr("权限已变化，剩余日志未清理"));break;}
        const QFileInfo fi(path);
        if (fi.lastModified() < cutoff) {
            if (QFile::remove(fi.absoluteFilePath())) {
                ++deleted;
                Logger::debug(QString("Deleted old log: %1").arg(fi.fileName()));
            } else failures.append(fi.absoluteFilePath());
        }
    }

    QString msg = deleted > 0
        ? QString("已清理 %1 个旧日志文件（%2 天以前）。").arg(deleted).arg(days)
        : QString("没有需要清理的日志文件（%1 天内）。").arg(days);

    if(!failures.isEmpty()) msg+=tr("\n以下文件清理失败，可稍后重试：\n")+failures.join("\n");
    Logger::info(msg);
    QMessageBox::information(this, "清理完成", msg);
}

void SystemSettingsWidget::applyPermissions()
{
    const bool write=AccessControl::canWrite("系统设置");
    for(auto* button:findChildren<QPushButton*>()) button->setEnabled(write);
    for(auto* edit:findChildren<QLineEdit*>()) edit->setReadOnly(!write);
    m_logLevelCombo->setEnabled(write);
}

QStringList SystemSettingsWidget::expiredLogs(const QString& directory, const QString& baseName, const QDateTime& cutoff)
{
    QStringList result;if(baseName.isEmpty()) return result;
    const QRegularExpression pattern("^"+QRegularExpression::escape(baseName)+"_(\\d{4}-\\d{2}-\\d{2})(?:\\.log)?$");
    for(const auto& file:QDir(directory).entryInfoList(QDir::Files)) {
        const auto match=pattern.match(file.fileName());
        const auto day=QDate::fromString(match.captured(1),"yyyy-MM-dd");
        if(match.hasMatch() && day.isValid() && day<cutoff.date() && file.lastModified()<cutoff && !file.isSymLink()) result.append(file.absoluteFilePath());
    }
    return result;
}
