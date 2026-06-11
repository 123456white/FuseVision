#include "Login.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QIcon>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QPalette>
#include "core/ThemeManager.h"
#include "core/Logger.h"
#include "core/DatabaseManager.h"
#include "core/SessionManager.h"

// =============================================================================
// LoginDialog.cpp — 用户登录对话框实现
// =============================================================================
// 完整登录流程：
//   1. 构造 → setupUI() + setupStyle() + loadSettings()（恢复记住的凭据）
//   2. 用户选择角色 → onUserChanged() → 自动填充/清空密码
//   3. 点击登录 → onLoginClicked()
//      a. 将 ComboBox 显示名映射为 DB 用户名（user/admin/superadmin）
//      b. DatabaseManager::validateLogin(username, password) → LoginResult
//      c. 成功 → SessionManager::login() → accept()（关闭对话框，main 继续）
//      d. 失败 → QMessageBox warning → 密码框全选聚焦
//   4. 取消 → onCancelClicked() → reject() → main 中 return 0 退出应用
//
// ComboBox → DB 映射：
//   ComboBox索引  显示名       DB username    DB role
//     0           "用户"       "user"          0
//     1           "管理员"     "admin"         1
//     2           "超级管理员" "superadmin"    2
// =============================================================================

// ── 构造 / 析构 ───────────────────────────────────────────────

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
    , m_settings("FuseVisionTeam", "FuseVisionLogin")  // 独立的 QSettings 命名空间
{
    setupUI();     // 构建控件树

    // 必须在 setupStyle 之前设置固定大小，否则渐变计算时 height() 为 0
    setFixedSize(420, 280);

    setupStyle();  // 应用渐变背景 + 全局 QSS
    loadSettings(); // 恢复"记住密码"状态

    // 加载动画定时器
    m_spinTimer = new QTimer(this);
    m_spinTimer->setInterval(80);
    connect(m_spinTimer, &QTimer::timeout, this, &LoginDialog::tickSpinner);

    // 对话框属性
    setWindowTitle("登录 FuseVision");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setModal(true);  // 模态对话框，阻塞主事件循环
}

LoginDialog::~LoginDialog() = default;

// ── UI 构建 ───────────────────────────────────────────────────

void LoginDialog::setupUI()
{
    // 标题
    QLabel* titleLabel = new QLabel("FuseVision 智能视觉系统", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    // 账号行（QLabel + QComboBox）
    QLabel* userLabel  = new QLabel("账号：", this);
    m_userCombo = new QComboBox(this);
    m_userCombo->addItem("用户");
    m_userCombo->addItem("管理员");
    m_userCombo->addItem("超级管理员");
    m_userCombo->setEditable(false);  // 不允许手动输入
    m_userCombo->setCurrentIndex(0);  // 默认选中"用户"

    // 密码行（QLabel + QLineEdit）
    QLabel* pwdLabel = new QLabel("密码：", this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);  // 密码掩码
    m_passwordEdit->setPlaceholderText("请输入密码");

    // 记住密码复选框
    m_rememberCheck = new QCheckBox("记住密码", this);

    // 按钮行
    m_loginBtn  = new QPushButton("登 录", this);
    m_loginBtn->setDefault(true);  // 回车键触发
    m_cancelBtn = new QPushButton("取 消", this);

    // Grid 布局（账号/密码/记住密码）
    QGridLayout* gridLayout = new QGridLayout;
    gridLayout->setHorizontalSpacing(15);
    gridLayout->setVerticalSpacing(12);
    gridLayout->addWidget(userLabel,      0, 0, 1, 1);
    gridLayout->addWidget(m_userCombo,    0, 1, 1, 2);
    gridLayout->addWidget(pwdLabel,       1, 0, 1, 1);
    gridLayout->addWidget(m_passwordEdit, 1, 1, 1, 2);
    gridLayout->addWidget(m_rememberCheck,2, 1, 1, 2);

    // 按钮水平布局（居中）
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();

    // 主垂直布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 25, 30, 25);
    mainLayout->setSpacing(15);
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(10);
    mainLayout->addLayout(gridLayout);
    mainLayout->addSpacing(5);
    mainLayout->addLayout(btnLayout);

    // 信号连接
    connect(m_loginBtn, &QPushButton::clicked,
            this, &LoginDialog::onLoginClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &LoginDialog::onCancelClicked);
    connect(m_userCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoginDialog::onUserChanged);

    onUserChanged(0);  // 初始触发，填充默认密码

    // ── 加载动画遮罩（初始隐藏）──
    m_loadingOverlay = new QWidget(this);
    m_loadingOverlay->setFixedSize(420, 280);
    m_loadingOverlay->move(0, 0);
    m_loadingOverlay->setStyleSheet("background: rgba(0,0,0,120);");
    m_loadingOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);

    // 旋转 spinner + 文字
    QVBoxLayout* spinLayout = new QVBoxLayout(m_loadingOverlay);
    spinLayout->setAlignment(Qt::AlignCenter);
    spinLayout->setSpacing(10);

    m_spinnerLabel = new QLabel(m_loadingOverlay);
    m_spinnerLabel->setAlignment(Qt::AlignCenter);
    m_spinnerLabel->setStyleSheet("color: white; font-size: 28px; background: transparent;");
    spinLayout->addWidget(m_spinnerLabel);

    QLabel* loadingText = new QLabel("正在登录...", m_loadingOverlay);
    loadingText->setAlignment(Qt::AlignCenter);
    loadingText->setStyleSheet("color: rgba(255,255,255,200); font-size: 13px; background: transparent;");
    spinLayout->addWidget(loadingText);

    m_loadingOverlay->hide();  // 初始隐藏
}

// ── 样式设置 ─────────────────────────────────────────────────

void LoginDialog::setupStyle()
{
    // 渐变背景（跟随主题）
    const auto& p = ThemeManager::instance().palette();
    QPalette pal = palette();
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(p.bgSecondary));
    gradient.setColorAt(1.0, QColor(p.bgPrimary));
    pal.setBrush(QPalette::Window, QBrush(gradient));
    setPalette(pal);
    setAutoFillBackground(true);

    // 应用全局 QSS 样式表
    setStyleSheet(ThemeManager::instance().styleSheet());

    // 设置按钮的 objectName，匹配 ThemeManager 中预定义的 #loginBtn / #cancelBtn QSS 规则
    m_loginBtn->setObjectName("loginBtn");
    m_cancelBtn->setObjectName("cancelBtn");
}

// ── 角色切换 → 自动填充/清空密码 ──────────────────────────────

void LoginDialog::onUserChanged(int index)
{
    if (index == 0) {
        // 选择"用户"→ 占位符提示默认密码，自动填入 "123"
        m_passwordEdit->setPlaceholderText("默认密码 123");
        if (m_passwordEdit->text().isEmpty() && !m_rememberCheck->isChecked()) {
            m_passwordEdit->setText("123");
        }
    } else if (index == 1) {
        // 选择"管理员"→ 如果之前是默认的 "123"，则清空
        m_passwordEdit->setPlaceholderText("请输入管理员密码");
        if (m_passwordEdit->text() == "123") {
            m_passwordEdit->clear();
        }
    } else {
        // 选择"超级管理员"→ 同理清空 "123"
        m_passwordEdit->setPlaceholderText("请输入超级管理员密码");
        if (m_passwordEdit->text() == "123") {
            m_passwordEdit->clear();
        }
    }
}

// ── 登录验证（先显示动画，再异步执行）─────────────────────

void LoginDialog::onLoginClicked()
{
    // 输入校验
    if (m_passwordEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入密码！");
        m_passwordEdit->setFocus();
        return;
    }

    // 显示加载动画 + 禁用输入
    showLoading();
    m_loginBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);
    m_userCombo->setEnabled(false);
    m_passwordEdit->setEnabled(false);

    // 延迟 120ms 让动画先渲染，再执行实际登录
    QTimer::singleShot(120, this, &LoginDialog::doLogin);
}

void LoginDialog::doLogin()
{
    QString displayName = m_userCombo->currentText();
    QString password    = m_passwordEdit->text();

    QString dbUsername;
    if (displayName == "用户") {
        dbUsername = "user";
    } else if (displayName == "管理员") {
        dbUsername = "admin";
    } else {
        dbUsername = "superadmin";
    }

    LoginResult result = DatabaseManager::instance().validateLogin(dbUsername, password);

    if (!result.success) {
        hideLoading();
        m_loginBtn->setEnabled(true);
        m_cancelBtn->setEnabled(true);
        m_userCombo->setEnabled(true);
        m_passwordEdit->setEnabled(true);
        QMessageBox::warning(this, "登录失败", "用户名或密码错误，请重试！");
        Logger::warn(QString("Login failed for user: %1").arg(displayName));
        m_passwordEdit->selectAll();
        m_passwordEdit->setFocus();
        return;
    }

    switch (result.role) {
    case 2: m_userRole = SuperAdmin; break;
    case 1: m_userRole = Admin;      break;
    default: m_userRole = User;      break;
    }

    m_username = displayName;
    saveSettings();

    SessionManager::instance().login(result.userId, displayName, result.role);

    Logger::info(QString("User logged in: %1 (role: %2)")
        .arg(m_username)
        .arg(m_userRole == SuperAdmin ? "SuperAdmin" :
             (m_userRole == Admin ? "Admin" : "User")));

    hideLoading();
    accept();
}

// ── 加载动画 ─────────────────────────────────────────────────

void LoginDialog::showLoading()
{
    m_spinFrame = 0;
    m_spinnerLabel->setText("◌");
    m_loadingOverlay->raise();
    m_loadingOverlay->show();
    m_spinTimer->start();
}

void LoginDialog::hideLoading()
{
    m_spinTimer->stop();
    m_loadingOverlay->hide();
}

void LoginDialog::tickSpinner()
{
    // 8 帧旋转：◌ ◔ ◑ ◕ ● ◕ ◑ ◔
    static const QStringList frames = {"◌", "◔", "◑", "◕", "●", "◕", "◑", "◔"};
    m_spinFrame = (m_spinFrame + 1) % frames.size();
    m_spinnerLabel->setText(frames[m_spinFrame]);
}

// ── 取消 ─────────────────────────────────────────────────────

void LoginDialog::onCancelClicked()
{
    Logger::info("Login cancelled");
    reject();  // 返回 QDialog::Rejected → main 中 return 0 退出应用
}

// ── 凭证持久化 ────────────────────────────────────────────────

void LoginDialog::loadSettings()
{
    bool remember = m_settings.value("remember", false).toBool();
    m_rememberCheck->setChecked(remember);

    if (remember) {
        // 恢复上次登录的角色
        QString lastUser = m_settings.value("lastUser", "用户").toString();
        int index = m_userCombo->findText(lastUser);
        if (index >= 0) {
            m_userCombo->setCurrentIndex(index);
        }
        // 恢复上次的密码
        QString lastPwd = m_settings.value("lastPassword", "").toString();
        if (!lastPwd.isEmpty()) {
            m_passwordEdit->setText(lastPwd);
        } else if (index == 0) {
            m_passwordEdit->setText("123");
        }
    } else {
        // 未勾选"记住密码"，仅对"用户"角色自动填入 "123"
        if (m_userCombo->currentIndex() == 0) {
            m_passwordEdit->setText("123");
        } else {
            m_passwordEdit->clear();
        }
    }
}

void LoginDialog::saveSettings()
{
    bool remember = m_rememberCheck->isChecked();
    m_settings.setValue("remember", remember);

    if (remember) {
        m_settings.setValue("lastUser", m_userCombo->currentText());
        m_settings.setValue("lastPassword", m_passwordEdit->text());
    } else {
        // 不记住则清除历史记录
        m_settings.remove("lastUser");
        m_settings.remove("lastPassword");
    }
}