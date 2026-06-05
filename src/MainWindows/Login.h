#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>

// =============================================================================
// LoginDialog — 用户登录对话框（模态）
// =============================================================================
// 数据流：
//   ComboBox 选择角色 → 映射 DB username → validateLogin → LoginResult
//   → 成功 → SessionManager::login(uid, displayName, role) → accept()
//   → 失败 → 弹窗提示 → 密码框全选聚焦
//
// 凭证持久化：
//   QSettings("FuseVisionTeam", "FuseVisionLogin") 管理：
//     - remember（bool）        : 是否记住密码
//     - lastUser（QString）     : 上次登录的角色显示名
//     - lastPassword（QString） : 上次输入的密码
//
// 默认凭据（由 DatabaseManager 种子用户提供）：
//   用户(0)     "用户"         → DB username "user"       / "123"
//   管理员(1)   "管理员"       → DB username "admin"      / "manager123"
//   超级管理员(2) "超级管理员"   → DB username "superadmin" / "admin123"
// =============================================================================

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    enum UserRole {
        SuperAdmin,   // 超级管理员（role=2）
        Admin,        // 管理员（role=1）
        User          // 普通用户（role=0）
    };

    explicit LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog();

    QString getUsername() const { return m_username; }     // 登录成功后的显示名
    UserRole getUserRole() const { return m_userRole; }    // 登录成功后的角色枚举

private slots:
    void onLoginClicked();         // 点击"登录" → 验证 → SessionManager::login
    void onCancelClicked();        // 点击"取消" → reject()
    void onUserChanged(int index); // ComboBox 角色切换 → 自动填充/清空密码

private:
    void loadSettings();           // 从 QSettings 恢复记住的用户名密码
    void saveSettings();           // 保存当前用户名密码到 QSettings
    void setupUI();                // 构建 UI 控件树
    void setupStyle();             // 设置渐变背景 + QSS 样式表

    // ── UI 控件 ────────────────────────────────────────────────
    QComboBox*   m_userCombo;      // 角色下拉（用户/管理员/超级管理员）
    QLineEdit*   m_passwordEdit;   // 密码输入框（EchoMode=Password）
    QCheckBox*   m_rememberCheck;  // "记住密码"复选框
    QPushButton* m_loginBtn;       // "登 录"按钮
    QPushButton* m_cancelBtn;      // "取 消"按钮

    QString  m_username;           // 登录成功后的显示名
    UserRole m_userRole = User;    // 登录成功后的角色
    QSettings m_settings;          // 持久化存储（"FuseVisionTeam", "FuseVisionLogin"）
};

#endif // LOGIN_H