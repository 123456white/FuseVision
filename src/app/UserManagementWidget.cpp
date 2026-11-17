#include "UserManagementWidget.h"
#include "ui/WriteAccess.h"
#include "core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>

UserManagementWidget::UserManagementWidget(QWidget* parent):QWidget(parent) {
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(24,16,24,20);
    auto* title=new QLabel(tr("管理账户"),this);title->setObjectName("sectionTitle");layout->addWidget(title);
    m_infoLabel=new QLabel(this);m_infoLabel->setWordWrap(true);layout->addWidget(m_infoLabel);
    auto* form=new QFormLayout;
    auto password=[this](const char* name) {auto* edit=new QLineEdit(this);edit->setObjectName(name);edit->setEchoMode(QLineEdit::Password);return edit;};
    m_oldPassword=password("oldPassword");m_newPassword=password("newPassword");m_confirmPassword=password("confirmPassword");
    form->addRow(tr("当前密码"),m_oldPassword);form->addRow(tr("新密码（至少6位）"),m_newPassword);form->addRow(tr("确认新密码"),m_confirmPassword);
    layout->addLayout(form);
    m_changePasswordBtn=new QPushButton(tr("修改密码"),this);m_changePasswordBtn->setObjectName("changePassword");
    layout->addWidget(m_changePasswordBtn,0,Qt::AlignLeft);layout->addStretch();
    connect(m_changePasswordBtn,&QPushButton::clicked,this,&UserManagementWidget::onChangePassword);
    AccessControl::watch(this,[this] {onSessionChanged();});onSessionChanged();
}
void UserManagementWidget::onSessionChanged() {
    const bool loggedIn=SessionManager::instance().isLoggedIn();
    const auto account=DatabaseManager::instance().singleAccount();
    m_infoLabel->setText(tr("管理账户：%1\n%2").arg(account.username,loggedIn?tr("已登录，可修改所有已实现功能的数据和设置。"):tr("未登录，只读浏览。请使用左下角登录按钮解除只读。")));
    for(auto* edit:{m_oldPassword,m_newPassword,m_confirmPassword}) {edit->clear();edit->setEnabled(loggedIn);}
    m_changePasswordBtn->setEnabled(loggedIn);
}
void UserManagementWidget::onChangePassword() {
    if(!WriteAccess::require("账户设置",this)) return;
    if(m_newPassword->text().size()<6 || m_newPassword->text()!=m_confirmPassword->text()) {
        QMessageBox::warning(this,tr("无法修改密码"),tr("新密码至少6位，且两次输入必须一致。"));return;
    }
    if(!DatabaseManager::instance().changePassword(SessionManager::instance().currentUserId(),m_oldPassword->text(),m_newPassword->text())) {
        QMessageBox::warning(this,tr("修改失败"),tr("当前密码不正确或数据库无法写入，原密码保持不变。"));return;
    }
    QSettings settings(QSettings::defaultFormat(),QSettings::UserScope,"FuseVisionTeam","FuseVisionLogin");settings.remove("remember");settings.remove("lastUser");settings.remove("lastPassword");
    onSessionChanged();
    QMessageBox::information(this,tr("修改成功"),tr("管理账户密码已更新，下次登录请使用新密码。"));
}
