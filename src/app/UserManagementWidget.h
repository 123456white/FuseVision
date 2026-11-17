/** @file
 * @brief UserManagementWidget 的公共声明。
 * @ingroup app
 */
#pragma once
#include <QWidget>
class QLabel;
class QLineEdit;
class QPushButton;

// Account page. Class name is retained for existing page integration.
/** @brief 账户状态与自身密码修改页面，不提供多用户或角色编辑。
 * @ingroup app
 */
class UserManagementWidget : public QWidget {
    Q_OBJECT
public:
    explicit UserManagementWidget(QWidget* parent=nullptr);
private slots:
    void onSessionChanged();
    void onChangePassword();
private:
    QLabel* m_infoLabel=nullptr;
    QLineEdit* m_oldPassword=nullptr;
    QLineEdit* m_newPassword=nullptr;
    QLineEdit* m_confirmPassword=nullptr;
    QPushButton* m_changePasswordBtn=nullptr;
};
