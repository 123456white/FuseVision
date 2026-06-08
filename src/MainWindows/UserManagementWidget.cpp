#include "UserManagementWidget.h"
#include "core/Logger.h"
#include "core/SessionManager.h"
#include "core/PermissionRegistry.h"
#include "core/PermissionGuard.h"
#include "core/DatabaseManager.h"
#include "core/ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QMap>
#include <QTimer>

/* ================================================================
 *  AddUserDialog — 新增用户弹窗
 * ================================================================ */
AddUserDialog::AddUserDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("添加用户");
    setFixedSize(340, 200);
    setModal(true);

    QFormLayout* form = new QFormLayout(this);
    form->setSpacing(12);

    m_userEdit = new QLineEdit;
    m_userEdit->setPlaceholderText("请输入用户名");
    form->addRow("用户名:", m_userEdit);

    m_pwdEdit = new QLineEdit;
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setPlaceholderText("请输入密码");
    form->addRow("密码:", m_pwdEdit);

    m_roleCombo = new QComboBox;
    m_roleCombo->addItem("用户 (只读)",         0);
    m_roleCombo->addItem("管理员 (可编辑)",      1);
    m_roleCombo->addItem("超级管理员 (全部权限)", 2);
    form->addRow("角色:", m_roleCombo);

    QDialogButtonBox* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_userEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "错误", "用户名不能为空");
            return;
        }
        if (m_pwdEdit->text().isEmpty()) {
            QMessageBox::warning(this, "错误", "密码不能为空");
            return;
        }
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString AddUserDialog::username() const { return m_userEdit->text().trimmed(); }
QString AddUserDialog::password() const { return m_pwdEdit->text(); }
int     AddUserDialog::role()     const { return m_roleCombo->currentData().toInt(); }

/* ================================================================
 *  ResetPasswordDialog — 重置密码弹窗
 * ================================================================ */
ResetPasswordDialog::ResetPasswordDialog(const QString& username, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("重置密码 - " + username);
    setFixedSize(320, 140);
    setModal(true);

    QFormLayout* form = new QFormLayout(this);
    form->setSpacing(12);

    QLabel* hintLabel = new QLabel(QString("为「%1」设置新密码:").arg(username));
    form->addRow(hintLabel);

    m_pwdEdit = new QLineEdit;
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setPlaceholderText("请输入新密码（至少6位）");
    form->addRow("新密码:", m_pwdEdit);

    QDialogButtonBox* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_pwdEdit->text().length() < 6) {
            QMessageBox::warning(this, "错误", "密码长度至少6位");
            return;
        }
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ResetPasswordDialog::newPassword() const { return m_pwdEdit->text(); }

/* ================================================================
 *  UserManagementWidget 主体
 * ================================================================ */

UserManagementWidget::UserManagementWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();

    m_guard = new PermissionGuard(this);
    m_guard->watch(QStringList{"用户管理"});

    connect(m_guard, &PermissionGuard::changed, this, &UserManagementWidget::applyPermissions);
    applyPermissions();

    connect(&SessionManager::instance(), &SessionManager::sessionChanged,
            this, &UserManagementWidget::onSessionChanged);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        // 推迟到下一轮事件循环，避免与 setStyleSheet 样式重绘冲突
        QTimer::singleShot(0, this, &UserManagementWidget::refreshPermissionMatrix);
    });

    onSessionChanged();

    Logger::info("UserManagementWidget initialized");
}

void UserManagementWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 8, 24, 20);
    mainLayout->setSpacing(12);

    m_infoLabel = new QLabel;
    m_infoLabel->setObjectName("infoLabel");
    mainLayout->addWidget(m_infoLabel);

    QHBoxLayout* toolbar = new QHBoxLayout;
    m_addUserBtn    = new QPushButton("添加用户");
    m_deleteUserBtn = new QPushButton("删除用户");
    m_resetPwdBtn   = new QPushButton("重置密码");
    m_addUserBtn->setObjectName("primaryBtn");
    toolbar->addWidget(m_addUserBtn);
    toolbar->addWidget(m_deleteUserBtn);
    toolbar->addWidget(m_resetPwdBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    m_userTable = new QTableWidget(0, 3);
    m_userTable->setHorizontalHeaderLabels({"ID", "用户名", "角色"});
    m_userTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_userTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_userTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userTable->verticalHeader()->setVisible(false);
    m_userTable->setAlternatingRowColors(true);
    m_userTable->verticalHeader()->setDefaultSectionSize(48);

    QGroupBox* permGroup = new QGroupBox("模块权限矩阵");
    QVBoxLayout* permLayout = new QVBoxLayout(permGroup);

    m_permTable = new QTableWidget(0, 3);
    m_permTable->setHorizontalHeaderLabels({"模块名称", "可读 (Read)", "可写 (Write)"});
    m_permTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_permTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_permTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_permTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_permTable->verticalHeader()->setVisible(false);
    m_permTable->setAlternatingRowColors(true);
    permLayout->addWidget(m_permTable);

    // 使用 QSplitter 支持拖拽调整比例，初始 1:4
    QSplitter* splitter = new QSplitter(Qt::Vertical);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_userTable);
    splitter->addWidget(permGroup);
    splitter->setSizes({ 1, 4 });  // 初始比例 1:4
    mainLayout->addWidget(splitter, 1);

    connect(m_addUserBtn,    &QPushButton::clicked, this, &UserManagementWidget::onAddUser);
    connect(m_deleteUserBtn, &QPushButton::clicked, this, &UserManagementWidget::onDeleteUser);
    connect(m_resetPwdBtn,   &QPushButton::clicked, this, &UserManagementWidget::onResetPassword);
    connect(m_userTable,     &QTableWidget::currentCellChanged,
            this, &UserManagementWidget::onUserSelectionChanged);
    connect(m_permTable,     &QTableWidget::itemChanged,
            this, &UserManagementWidget::onPermissionChanged);
}

void UserManagementWidget::onSessionChanged()
{
    if (!SessionManager::instance().isLoggedIn()) {
        m_infoLabel->setText("请先登录");
        m_canEdit = false;
        setEditMode(false);
        return;
    }

    m_canEdit = m_guard->canWrite("用户管理");
    setEditMode(m_canEdit);

    const char* roleStr = (SessionManager::instance().currentUserRole() == 2) ? "超级管理员"
                        : (SessionManager::instance().currentUserRole() == 1) ? "管理员"
                        : "用户";
    m_infoLabel->setText(QString("当前用户: %1 (%2) | %3")
        .arg(SessionManager::instance().currentUsername())
        .arg(roleStr)
        .arg(m_canEdit ? "有编辑权限" : "只读模式"));

    refreshUserTable();
}

void UserManagementWidget::setEditMode(bool canEdit)
{
    m_addUserBtn->setEnabled(canEdit);
    m_deleteUserBtn->setEnabled(canEdit);
    m_resetPwdBtn->setEnabled(canEdit);
}

void UserManagementWidget::applyPermissions()
{
    if (!SessionManager::instance().isLoggedIn())
        return;

    m_canEdit = m_guard->canWrite("用户管理");
    setEditMode(m_canEdit);
}

void UserManagementWidget::refreshUserTable()
{
    m_userTable->setRowCount(0);
    m_selectedUserId = -1;

    const auto users = DatabaseManager::instance().allUsers();
    m_userTable->setRowCount(users.size());

    const QStringList roleNames = {"用户", "管理员", "超级管理员"};

    for (int i = 0; i < users.size(); ++i) {
        const auto& u = users[i];

        QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(u.id));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        idItem->setData(Qt::UserRole, u.id);
        m_userTable->setItem(i, 0, idItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(u.username);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_userTable->setItem(i, 1, nameItem);

        QComboBox* roleCombo = new QComboBox;
        roleCombo->addItems(roleNames);
        roleCombo->setCurrentIndex(qBound(0, u.role, 2));
        roleCombo->setEnabled(m_canEdit);
        roleCombo->setFixedHeight(44);
        roleCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        // 容器垂直居中，使下拉菜单与用户名在同一水平线
        QWidget* container = new QWidget;
        QHBoxLayout* cl = new QHBoxLayout(container);
        cl->setContentsMargins(2, 0, 2, 0);
        cl->setSpacing(0);
        cl->setAlignment(Qt::AlignVCenter);
        cl->addWidget(roleCombo);
        m_userTable->setCellWidget(i, 2, container);

        int uid = u.id;
        connect(roleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, uid](int newRole) {
            if (DatabaseManager::instance().setUserRole(uid, newRole)) {
                Logger::info(QString("Role updated: uid=%1 role=%2").arg(uid).arg(newRole));

                const QStringList mods = PermissionRegistry::instance().modules();
                for (const QString& mod : mods) {
                    bool canW = (newRole == 1 || newRole == 2);
                    DatabaseManager::instance().setPermission(uid, mod, true, canW);
                }

                PermissionRegistry::instance().refreshPermissions(uid);
                refreshPermissionMatrix();
            }
        });
    }

    m_userTable->resizeColumnToContents(2);
    m_userTable->setColumnWidth(2, qMax(m_userTable->columnWidth(2), 200));

    if (m_userTable->rowCount() > 0)
        m_userTable->selectRow(0);
    else
        refreshPermissionMatrix();
}

void UserManagementWidget::onUserSelectionChanged()
{
    int row = m_userTable->currentRow();
    if (row < 0) {
        m_selectedUserId = -1;
        refreshPermissionMatrix();
        return;
    }
    m_selectedUserId = m_userTable->item(row, 0)->data(Qt::UserRole).toInt();
    refreshPermissionMatrix();
}

void UserManagementWidget::refreshPermissionMatrix()
{
    m_permTable->blockSignals(true);
    m_permTable->setRowCount(0);

    if (m_selectedUserId < 0) {
        m_permTable->blockSignals(false);
        return;
    }

    QStringList allModules = PermissionRegistry::instance().modules();
    if (allModules.isEmpty()) {
        m_permTable->blockSignals(false);
        return;
    }

    QMap<QString, QStringList> groups;
    for (const QString& mod : allModules) {
        int dotPos = mod.indexOf('.');
        if (dotPos > 0)
            groups[mod.left(dotPos)].append(mod);
        else
            groups[mod].append(mod);
    }

    int row = 0;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QString& groupName  = it.key();
        const QStringList& children = it.value();

        bool hasSubModules = (children.size() > 1 || (children.size() == 1 && children[0] != groupName));

        if (hasSubModules) {
            addGroupHeaderRow(row++, groupName);

            for (const QString& child : children) {
                QString shortName = QString::fromUtf8("  \u2514\u2500 ") + child.mid(groupName.length() + 1);
                addPermissionRow(row++, child, shortName);
            }
        } else {
            addPermissionRow(row++, children[0], groupName);
        }
    }

    m_permTable->blockSignals(false);
}

void UserManagementWidget::addGroupHeaderRow(int row, const QString& title)
{
    const auto& p = ThemeManager::instance().palette();

    m_permTable->insertRow(row);
    m_permTable->setRowHeight(row, 28);

    QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8("▸  ") + title);
    QFont f = item->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 1);
    item->setFont(f);
    item->setFlags(Qt::NoItemFlags);
    item->setForeground(QColor(p.textPrimary));
    item->setBackground(QColor(p.bgTertiary));
    m_permTable->setItem(row, 0, item);

    for (int col = 1; col <= 2; ++col) {
        QTableWidgetItem* emptyItem = new QTableWidgetItem;
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setBackground(QColor(p.bgTertiary));
        m_permTable->setItem(row, col, emptyItem);
    }
}

void UserManagementWidget::addPermissionRow(int row, const QString& moduleKey,
                                             const QString& displayName)
{
    m_permTable->insertRow(row);

    QTableWidgetItem* modItem = new QTableWidgetItem(displayName);
    modItem->setFlags(modItem->flags() & ~Qt::ItemIsEditable);
    modItem->setData(Qt::UserRole, moduleKey);
    m_permTable->setItem(row, 0, modItem);

    bool canRead  = PermissionRegistry::instance().canRead(m_selectedUserId, moduleKey);
    bool canWrite = PermissionRegistry::instance().canWrite(m_selectedUserId, moduleKey);

    QTableWidgetItem* readItem = new QTableWidgetItem;
    if (m_canEdit) {
        readItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    } else {
        readItem->setFlags(Qt::NoItemFlags);
    }
    readItem->setCheckState(canRead ? Qt::Checked : Qt::Unchecked);
    m_permTable->setItem(row, 1, readItem);

    QTableWidgetItem* writeItem = new QTableWidgetItem;
    if (m_canEdit) {
        writeItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    } else {
        writeItem->setFlags(Qt::NoItemFlags);
    }
    writeItem->setCheckState(canWrite ? Qt::Checked : Qt::Unchecked);
    m_permTable->setItem(row, 2, writeItem);
}

void UserManagementWidget::onPermissionChanged(QTableWidgetItem* item)
{
    if (!item || m_selectedUserId < 0 || !m_canEdit)
        return;

    m_permTable->blockSignals(true);

    int row = item->row();
    QTableWidgetItem* nameItem = m_permTable->item(row, 0);
    if (!nameItem) { m_permTable->blockSignals(false); return; }

    QString moduleName = nameItem->data(Qt::UserRole).toString();
    if (moduleName.isEmpty()) { m_permTable->blockSignals(false); return; }

    bool canRead  = (m_permTable->item(row, 1)
                     && m_permTable->item(row, 1)->checkState() == Qt::Checked);
    bool canWrite = (m_permTable->item(row, 2)
                     && m_permTable->item(row, 2)->checkState() == Qt::Checked);

    DatabaseManager::instance().setPermission(m_selectedUserId, moduleName, canRead, canWrite);
    PermissionRegistry::instance().refreshPermissions(m_selectedUserId);

    Logger::info(QString("Permission updated: uid=%1 module=%2 read=%3 write=%4")
        .arg(m_selectedUserId).arg(moduleName).arg(canRead).arg(canWrite));

    m_permTable->blockSignals(false);
}

void UserManagementWidget::onAddUser()
{
    AddUserDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    int newId = DatabaseManager::instance().addUser(dlg.username(), dlg.password(), dlg.role());
    if (newId < 0) {
        QMessageBox::warning(this, "错误", "添加用户失败，用户名可能已存在");
        return;
    }

    const QStringList allMods = PermissionRegistry::instance().modules();
    for (const QString& mod : allMods) {
        bool canWrite = (dlg.role() == 1 || dlg.role() == 2);
        DatabaseManager::instance().setPermission(newId, mod, true, canWrite);
    }

    PermissionRegistry::instance().refreshPermissions(newId);
    refreshUserTable();

    Logger::info(QString("User added via UI: %1 (role=%2)").arg(dlg.username()).arg(dlg.role()));
}

void UserManagementWidget::onDeleteUser()
{
    if (m_selectedUserId < 0) return;

    int currentUid = SessionManager::instance().currentUserId();
    if (m_selectedUserId == currentUid) {
        QMessageBox::warning(this, "错误", "不能删除当前登录的用户");
        return;
    }

    int row = m_userTable->currentRow();
    QString name = m_userTable->item(row, 1)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString("确定要删除用户「%1」吗？\n此操作不可撤销。").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    if (DatabaseManager::instance().deleteUser(m_selectedUserId)) {
        Logger::info(QString("User deleted via UI: %1 (id=%2)").arg(name).arg(m_selectedUserId));
        refreshUserTable();
    } else {
        QMessageBox::warning(this, "错误", "删除用户失败");
    }
}

void UserManagementWidget::onResetPassword()
{
    if (m_selectedUserId < 0) return;

    int row = m_userTable->currentRow();
    QString name = m_userTable->item(row, 1)->text();

    ResetPasswordDialog dlg(name, this);
    if (dlg.exec() != QDialog::Accepted) return;

    if (DatabaseManager::instance().changePasswordByAdmin(
            m_selectedUserId, dlg.newPassword())) {
        QMessageBox::information(this, "成功",
            QString("用户「%1」密码已重置").arg(name));
        Logger::info(QString("Password reset via UI: %1 (id=%2)").arg(name).arg(m_selectedUserId));
    } else {
        QMessageBox::warning(this, "错误", "重置密码失败");
    }
}
