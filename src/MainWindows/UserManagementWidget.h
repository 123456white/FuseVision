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

// =============================================================================
// UserManagementWidget — 用户管理 + 权限矩阵页面
// =============================================================================
// 作为 QStackedWidget 的第 3 页，提供完整的用户 CRUD 和模块权限管理功能。
//
// 布局结构（上下双面板）：
//   上：用户表格（User QTableWidget）
//     ┌────┬──────────┬────────┐
//     │ ID │ 用户名   │ 角色   │  ← 角色列是 QComboBox，可直接修改
//     ├────┼──────────┼────────┤
//     │  1 │ user     │ 用户   │
//     │  2 │ admin    │ 管理员 │
//     └────┴──────────┴────────┘
//
//   下：权限矩阵（Permission QTableWidget）
//     ┌──────────────────┬──────────┬──────────┐
//     │ 模块名称         │ 可读     │ 可写     │  ← checkbox 列
//     ├──────────────────┼──────────┼──────────┤
//     │ ▸ 用户管理       │    ✓     │    ✓     │
//     │ ▸ 系统设置       │    ✓     │    ✓     │
//     │ ▸ 深度学习       │          │          │
//     │   └─ 模型训练   │    ✓     │    ✓     │
//     │   └─ 推理       │    ✓     │    ✓     │
//     └──────────────────┴──────────┴──────────┘
//
// 权限门控：
//   通过 PermissionGuard watch("用户管理") 控制编辑模式。
//   普通用户只读，管理员可编辑。
// =============================================================================

class UserManagementWidget : public QWidget
{
    Q_OBJECT
public:
    explicit UserManagementWidget(QWidget* parent = nullptr);

private slots:
    void onSessionChanged();                    // 登录/登出 → 刷新编辑模式 + 用户表
    void onAddUser();                           // 弹出 AddUserDialog → 添加用户
    void onDeleteUser();                        // 确认后删除选中用户（不能删自己）
    void onResetPassword();                     // 弹出 ResetPasswordDialog → 重置密码
    void onUserSelectionChanged();              // 选中用户变化 → 刷新权限矩阵
    void onPermissionChanged(QTableWidgetItem* item);  // checkbox 点击 → 写 DB + 刷新缓存
    void applyPermissions();                    // 权限变更 → 启用/禁用按钮

private:
    void initUI();                              // 构建双面板 UI（用户表 + 权限矩阵）
    void refreshUserTable();                    // 重载用户表全部行
    void refreshPermissionMatrix();             // 根据 m_selectedUserId 重建权限矩阵
    void addGroupHeaderRow(int row, const QString& title);           // 彩色分组标题行
    void addPermissionRow(int row, const QString& moduleKey,         // 权限行（checkbox）
                          const QString& displayName);
    void setEditMode(bool canEdit);             // 切换编辑/只读模式

    // ── 成员变量 ──────────────────────────────────────────────
    PermissionGuard* m_guard = nullptr;         // 权限代理：watch("用户管理")
    QTableWidget* m_userTable = nullptr;        // 上半：用户列表
    QTableWidget* m_permTable = nullptr;        // 下半：权限矩阵
    QPushButton*  m_addUserBtn = nullptr;       // "添加用户"按钮
    QPushButton*  m_deleteUserBtn = nullptr;    // "删除用户"按钮
    QPushButton*  m_resetPwdBtn = nullptr;      // "重置密码"按钮
    QLabel*       m_infoLabel = nullptr;        // 页面顶部提示（当前用户/编辑模式）

    bool m_canEdit = false;                     // 当前编辑权限
    int  m_selectedUserId = -1;                 // 当前选中的用户 ID（-1 = 无选中）
};

// =============================================================================
// AddUserDialog — 新增用户弹窗
// =============================================================================
// 包含三个输入：用户名（QLineEdit）、密码（QLineEdit）、角色（QComboBox）
// 公开三个 getter：username() / password() / role()
class AddUserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddUserDialog(QWidget* parent = nullptr);
    QString username() const;
    QString password() const;
    int     role()     const;

private:
    QLineEdit* m_userEdit;
    QLineEdit* m_pwdEdit;
    QComboBox* m_roleCombo;
};

// =============================================================================
// ResetPasswordDialog — 重置密码弹窗
// =============================================================================
// 仅一个输入：新密码（QLineEdit），最小长度 6 位
class ResetPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ResetPasswordDialog(const QString& username, QWidget* parent = nullptr);
    QString newPassword() const;

private:
    QLineEdit* m_pwdEdit;
};

#endif // USERMANAGEMENTWIDGET_H