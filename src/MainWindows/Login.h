#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>

/*
 * LoginDialog — 用户登录对话框
 *
 * 功能：
 *   - 三档角色选择（用户 / 管理员 / 超级管理员）
 *   - 密码输入 + 记住密码（QSettings 持久化）
 *   - 登录验证（DatabaseManager::validateLogin → LoginResult）
 *   - 成功后自动调用 SessionManager::login() 启动会话
 *
 * 默认凭据：
 *   用户(0)     user         / 123
 *   管理员(1)   admin        / manager123
 *   超级管理员(2) superadmin / admin123
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    enum UserRole {
        SuperAdmin,
        Admin,
        User
    };

    explicit LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog();

    QString getUsername() const { return m_username; }
    UserRole getUserRole() const { return m_userRole; }

private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onUserChanged(int index);

private:
    void loadSettings();
    void saveSettings();
    void setupUI();
    void setupStyle();

    QComboBox* m_userCombo;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_rememberCheck;
    QPushButton* m_loginBtn;
    QPushButton* m_cancelBtn;

    QString m_username;
    UserRole m_userRole;
    QSettings m_settings;
};

#endif // LOGIN_H
