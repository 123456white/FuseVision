#ifndef PERMISSIONGUARD_H
#define PERMISSIONGUARD_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "PermissionInfo.h"

// =============================================================================
// PermissionGuard — Widget 级权限代理（QObject，信号驱动）
// =============================================================================
// 每个功能 Widget（DeepLearningWidget / TraditionalWidget / UserManagementWidget）
// 持有一个 PermissionGuard 实例，用于管理该 Widget 关注的权限点。
//
// 使用模式（标准三步）：
//   1. 构造：  m_guard = new PermissionGuard(this);
//   2. 注册：  m_guard->watch("深度学习.模型训练");  // 声明关注哪些权限点
//   3. 查询：  m_guard->canRead("深度学习.模型训练");  // 查询当前用户是否有读/写权限
//
// 自动刷新：
//   watch() 内部自动连接 SessionManager::sessionChanged 信号，
//   用户登录/登出时自动从 PermissionRegistry 拉取最新权限状态，
//   如有变更则发射 changed() 信号 → Widget::applyPermissions() 重绘 UI
// =============================================================================

class PermissionGuard : public QObject
{
    Q_OBJECT

public:
    explicit PermissionGuard(QObject* parent = nullptr);

    // ── 注册关注的权限点 ──────────────────────────────────────────
    void watch(const QString& moduleKey);         // 注册单个模块（转发到批量重载）
    void watch(const QStringList& moduleKeys);    // 批量注册，并自动连接 SessionManager::sessionChanged

    // ── 权限查询（O(1) 内存查表） ─────────────────────────────────
    bool canRead(const QString& moduleKey)  const;   // 当前用户对该模块的读权限
    bool canWrite(const QString& moduleKey) const;   // 当前用户对该模块的写权限

    QStringList watchedKeys() const;                 // 返回所有关注的模块键

signals:
    void changed();   // 权限状态变更时发射 → Widget 连接后调用 applyPermissions()

private slots:
    void refresh();   // 从 PermissionRegistry 重新拉取所有关注模块的权限状态

private:
    int currentUserId() const;                       // 获取当前会话用户 ID（委托 SessionManager）
    QMap<QString, PermissionInfo> m_state;           // 权限状态缓存：模块名 → PermissionInfo
};

#endif // PERMISSIONGUARD_H
