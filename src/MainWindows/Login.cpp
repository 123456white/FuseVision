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

/*
 * LoginDialog.cpp — 登录对话框实现
 *
 * onLoginClicked 数据流：
 *   ComboBox 选择 → 映射为 DB username → validateLogin → LoginResult
 *   → 成功则 SessionManager::login(userId, displayName, role)
 *     失败则弹窗提示，密码框全选聚焦
 *
 * 凭证持久化：
 *   QSettings("FuseVisionTeam", "FuseVisionLogin") 保存
 *   remember / lastUser / lastPassword 三项
 *
 * 便捷功能：
 *   选择"用户"时自动填入密码 "123"
 *   切换角色时已填入的密码自动清除（若非 "123"）
 */

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
    , m_settings("FuseVisionTeam", "FuseVisionLogin")
{
    setupUI();
    setupStyle();
    loadSettings();
    setWindowTitle("登录 FuseVision");
    setFixedSize(420, 280);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setModal(true);
}

LoginDialog::~LoginDialog() = default;

void LoginDialog::setupUI()
{
    QLabel* titleLabel = new QLabel("FuseVision 智能视觉系统", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    QLabel* userLabel  = new QLabel("账号：", this);
    m_userCombo = new QComboBox(this);
    m_userCombo->addItem("用户");
    m_userCombo->addItem("管理员");
    m_userCombo->addItem("超级管理员");
    m_userCombo->setEditable(false);
    m_userCombo->setCurrentIndex(0);

    QLabel* pwdLabel = new QLabel("密码：", this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("请输入密码");

    m_rememberCheck = new QCheckBox("记住密码", this);

    m_loginBtn  = new QPushButton("登 录", this);
    m_loginBtn->setDefault(true);
    m_cancelBtn = new QPushButton("取 消", this);

    QGridLayout* gridLayout = new QGridLayout;
    gridLayout->setHorizontalSpacing(15);
    gridLayout->setVerticalSpacing(12);
    gridLayout->addWidget(userLabel,      0, 0, 1, 1);
    gridLayout->addWidget(m_userCombo,    0, 1, 1, 2);
    gridLayout->addWidget(pwdLabel,       1, 0, 1, 1);
    gridLayout->addWidget(m_passwordEdit, 1, 1, 1, 2);
    gridLayout->addWidget(m_rememberCheck,2, 1, 1, 2);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 25, 30, 25);
    mainLayout->setSpacing(15);
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(10);
    mainLayout->addLayout(gridLayout);
    mainLayout->addSpacing(5);
    mainLayout->addLayout(btnLayout);

    connect(m_loginBtn, &QPushButton::clicked,
            this, &LoginDialog::onLoginClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &LoginDialog::onCancelClicked);
    connect(m_userCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoginDialog::onUserChanged);

    onUserChanged(0);
}

void LoginDialog::setupStyle()
{
    QPalette pal = palette();
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(240, 248, 255));
    gradient.setColorAt(1.0, QColor(230, 242, 255));
    pal.setBrush(QPalette::Window, QBrush(gradient));
    setPalette(pal);
    setAutoFillBackground(true);

    setStyleSheet(ThemeManager::instance().styleSheet());

    m_loginBtn->setObjectName("loginBtn");
    m_cancelBtn->setObjectName("cancelBtn");
}

void LoginDialog::onUserChanged(int index)
{
    if (index == 0) {
        m_passwordEdit->setPlaceholderText("默认密码 123");
        if (m_passwordEdit->text().isEmpty() && !m_rememberCheck->isChecked()) {
            m_passwordEdit->setText("123");
        }
    } else if (index == 1) {
        m_passwordEdit->setPlaceholderText("请输入管理员密码");
        if (m_passwordEdit->text() == "123") {
            m_passwordEdit->clear();
        }
    } else {
        m_passwordEdit->setPlaceholderText("请输入超级管理员密码");
        if (m_passwordEdit->text() == "123") {
            m_passwordEdit->clear();
        }
    }
}

void LoginDialog::onLoginClicked()
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
    accept();
}

void LoginDialog::onCancelClicked()
{
    Logger::info("Login cancelled");
    reject();
}

void LoginDialog::loadSettings()
{
    bool remember = m_settings.value("remember", false).toBool();
    m_rememberCheck->setChecked(remember);

    if (remember) {
        QString lastUser = m_settings.value("lastUser", "用户").toString();
        int index = m_userCombo->findText(lastUser);
        if (index >= 0) {
            m_userCombo->setCurrentIndex(index);
        }
        QString lastPwd = m_settings.value("lastPassword", "").toString();
        if (!lastPwd.isEmpty()) {
            m_passwordEdit->setText(lastPwd);
        } else if (index == 0) {
            m_passwordEdit->setText("123");
        }
    } else {
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
        m_settings.remove("lastUser");
        m_settings.remove("lastPassword");
    }
}
