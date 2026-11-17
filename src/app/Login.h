/** @file
 * @brief Login 的公共声明。
 * @ingroup app
 */
#pragma once
#include <QDialog>
class QLineEdit;

// A single account unlocks editing; closing or cancelling keeps the guest session.
/** @brief 可取消的单账户登录对话框；成功解除只读，不持久化密码。
 * @ingroup app
 */
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent=nullptr);
private slots:
    void onLoginClicked();
    void onCancelClicked();
private:
    QLineEdit* m_passwordEdit=nullptr;
};
