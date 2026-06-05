#ifndef DEEPLEARNINGWIDGET_H
#define DEEPLEARNINGWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include "core/PermissionGuard.h"

// =============================================================================
// DeepLearningWidget — 深度学习模块页面（8 标签页）
// =============================================================================
// 采用顶部 QTabBar + 右侧 QStackedWidget 的布局：
//
//   ┌──────────────────────────────────────────┐
//   │ [项目管理] [模型管理] [数据集] ... [导出]  │ ← QTabBar（水平标签栏）
//   ├──────────────────────────────────────────┤
//   │                                          │
//   │           当前标签页的空白内容区           │ ← QStackedWidget (index 0~7)
//   │                                          │
//   └──────────────────────────────────────────┘
//
// 8 个标签页：项目管理 / 模型管理 / 数据集管理 / 数据标注 /
//            数据拆分 / 模型训练 / 模型预测 / 模型导出
//
// 权限控制：每个标签页独立 watch("深度学习.xxxx")
//   canRead  → 控制 Tab 是否可见
//   canWrite → 控制页内操作按钮（预留）
// =============================================================================

class DeepLearningWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeepLearningWidget(QWidget* parent = nullptr);

private slots:
    void applyPermissions();          // 权限变更时刷新 UI
    void onTabChanged(int index);     // Tab 切换 → StackedWidget 翻页

private:
    void initUI();

    // ── 成员变量 ──────────────────────────────────────────────
    PermissionGuard* m_guard          = nullptr;  // 权限代理
    QTabBar*         m_tabBar         = nullptr;  // 顶部标签栏（8 个标签）
    QStackedWidget*  m_stackedWidget  = nullptr;  // 右侧内容区（8 个空白页）

    // 8 个标签页名称（与 QStackedWidget index 对应）
    static const QStringList s_tabNames;
    // 8 个权限键（与 PermissionRegistry 注册名对应）
    static const QStringList s_permissionKeys;
};

#endif // DEEPLEARNINGWIDGET_H
