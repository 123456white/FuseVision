#ifndef USERMANAGEMENTWIDGET_H
#define USERMANAGEMENTWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QDialog>

class PermissionGuard;

class UserManagementWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UserManagementWidget(QWidget* parent = nullptr);

private slots:
    void onSessionChanged();
    void onAddUser();
    void onDeleteUser();
    void onResetPassword();
    void onUserSelectionChanged();
    void onPermissionChanged(QTableWidgetItem* item);
    void applyPermissions();

private:
    void initUI();
    void refreshUserTable();
    void refreshPermissionMatrix();
    void addGroupHeaderRow(int row, const QString& title);
    void addPermissionRow(int row, const QString& moduleKey, const QString& displayName);
    void setEditMode(bool canEdit);

    PermissionGuard* m_guard          = nullptr;

    QTableWidget* m_userTable      = nullptr;
    QTableWidget* m_permTable      = nullptr;
    QPushButton*  m_addUserBtn     = nullptr;
    QPushButton*  m_deleteUserBtn  = nullptr;
    QPushButton*  m_resetPwdBtn    = nullptr;
    QLabel*       m_infoLabel      = nullptr;

    bool m_canEdit = false;
    int  m_selectedUserId = -1;
};

class AddUserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddUserDialog(QWidget* parent = nullptr);
    QString username() const;
    QString password() const;
    int     role()     const;

private:
    QLineEdit*  m_userEdit   = nullptr;
    QLineEdit*  m_pwdEdit    = nullptr;
    QComboBox*  m_roleCombo  = nullptr;
};

class ResetPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ResetPasswordDialog(const QString& username, QWidget* parent = nullptr);
    QString newPassword() const;

private:
    QLineEdit* m_pwdEdit = nullptr;
};

#endif // USERMANAGEMENTWIDGET_H
