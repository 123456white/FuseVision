#ifndef DEEPLEARNINGWIDGET_H
#define DEEPLEARNINGWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QShowEvent>
#include "core/PermissionGuard.h"
#include "core/LogMonitor.h"

// =============================================================================
// DeepLearningWidget — 深度学习模块页面（8 标签页）
// =============================================================================
// 采用顶部 QTabBar + 右侧 QStackedWidget 的布局：
//
//   ┌──────────────────────────────────────────┐
//   │ [项目管理] [模型管理] [数据集] ... [导出]  │ ← QTabBar（水平标签栏）
//   ├──────────────────────────────────────────┤
//   │                                          │
//   │           当前标签页内容区                  │ ← QStackedWidget (index 0~7)
//   │                                          │
//   ├──────────────────────────────────────────┤
//   │ 日志监控                              ▲   │ ← LogMonitor（可折叠）
//   │ [10:30:12] 模型训练已启动 ...            │
//   └──────────────────────────────────────────┘
//
// Tab 0（项目管理）已实现 ProjectManagement 组件，
// Tab 1~7 为占位页面。
//
// 权限控制：每个标签页独立 watch("深度学习.xxxx")
//   canRead  → 控制 Tab 是否可见
//   canWrite → 控制页内操作按钮（预留）
//
// 信号：
//   dlProjectChanged(projectName) → FuseVision 状态栏更新项目名
// =============================================================================

class ProjectManagement;  // 前向声明
class ModelManagement;     // 前向声明

class DeepLearningWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeepLearningWidget(QWidget* parent = nullptr);

    QString currentProjectName() const;   // 获取当前项目管理模块中的项目名

signals:
    void dlProjectChanged(const QString& projectName);  // 项目切换时通知主窗口

protected:
    void showEvent(QShowEvent* event) override;  // 初始化分割器比例

private slots:
    void applyPermissions();          // 权限变更时刷新 UI
    void onTabChanged(int index);     // Tab 切换 → StackedWidget 翻页
    void onProjectOpened(const QString& name, const QString& path);  // 项目打开回调
    void onProjectClosed();           // 项目关闭回调

private:
    void initUI();

    // ── 成员变量 ──────────────────────────────────────────────
    PermissionGuard* m_guard          = nullptr;  // 权限代理
    QTabBar*         m_tabBar         = nullptr;  // 顶部标签栏（8 个标签）
    QStackedWidget*  m_stackedWidget  = nullptr;  // 右侧内容区（8 个页面）
    LogMonitor*      m_logMonitor     = nullptr;  // 底部日志面板
    bool             m_shown          = false;     // 首次显示标记

    // ── 子页面 ──────────────────────────────────────────────
    ProjectManagement* m_projectManagement = nullptr;  // Tab 0：项目管理
    ModelManagement*   m_modelManagement   = nullptr;  // Tab 1：模型管理

    // 8 个标签页名称（与 QStackedWidget index 对应）
    static const QStringList s_tabNames;
    // 8 个权限键（与 PermissionRegistry 注册名对应）
    static const QStringList s_permissionKeys;
};

#endif // DEEPLEARNINGWIDGET_H
