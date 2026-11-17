#include "Login.h"
#include "core/DatabaseManager.h"
#include "core/SessionManager.h"
#include "ui/ThemeManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>

LoginDialog::LoginDialog(QWidget* parent):QDialog(parent) {
    setWindowTitle(tr("登录并解除只读"));
    setMinimumWidth(380);
    setStyleSheet(ThemeManager::instance().styleSheet());
    auto* layout=new QVBoxLayout(this);
    auto* hint=new QLabel(tr("未登录时可查看所有界面；登录后可修改数据和设置。"),this);
    hint->setWordWrap(true);layout->addWidget(hint);
    auto* form=new QFormLayout;
    auto* account=new QLineEdit(DatabaseManager::instance().singleAccount().username,this);
    account->setObjectName("loginAccount");account->setReadOnly(true);
    m_passwordEdit=new QLineEdit(this);m_passwordEdit->setObjectName("loginPassword");
    m_passwordEdit->setEchoMode(QLineEdit::Password);m_passwordEdit->setPlaceholderText(tr("请输入管理账户密码"));
    form->addRow(tr("账户"),account);form->addRow(tr("密码"),m_passwordEdit);layout->addLayout(form);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("登录"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("保持只读"));
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,this,&LoginDialog::onLoginClicked);
    connect(buttons,&QDialogButtonBox::rejected,this,&LoginDialog::onCancelClicked);
    m_passwordEdit->setFocus();
}
void LoginDialog::onLoginClicked() {
    const auto account=DatabaseManager::instance().singleAccount();
    const auto result=DatabaseManager::instance().validateLogin(account.username,m_passwordEdit->text());
    if(!result.success) {
        QMessageBox::warning(this,tr("登录失败"),tr("密码错误或管理账户不可用，请重试。"));
        m_passwordEdit->selectAll();m_passwordEdit->setFocus();return;
    }
    SessionManager::instance().login(result.userId);
    // Do not persist unlock credentials. Remove old remembered plaintext after explicit login.
    QSettings settings(QSettings::defaultFormat(),QSettings::UserScope,"FuseVisionTeam","FuseVisionLogin");
    settings.remove("remember");settings.remove("lastUser");settings.remove("lastPassword");
    accept();
}
void LoginDialog::onCancelClicked() { reject(); }
