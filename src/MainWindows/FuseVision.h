#ifndef FUSEVISION_H
#define FUSEVISION_H

#include <QMainWindow>
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include "DeepLearningWidget/DeepLearningWidget.h"
#include "TraditionalWidget/TraditionalWidget.h"
#include "MainWindows/UserManagementWidget.h"
#include "MainWindows/SystemSettingsWidget.h"
#include "core/Logger.h"
#include "core/ThemeManager.h"

// =============================================================================
// FuseVision — 应用主窗口（QMainWindow）
// =============================================================================
// 布局结构（最大层级深度 = 4）：
//   QMainWindow
//   └── QWidget（m_centralWidget）
//       └── QSplitter（m_mainSplitter, Horizontal）
//           ├── QWidget（m_menuWidget, 固定 48px）= 左侧图标菜单
//           │   ├── QListWidget（m_topMenuList）    = 深度学习 / 传统视觉
//           │   ├── QListWidget（m_bottomMenuList）  = 用户管理 / 系统设置
//           │   └── QPushButton（m_themeBtn）        = 亮色/暗色切换
//           └── QStackedWidget（m_stackedWidget）     = 右侧内容区
//               ├── DeepLearningWidget             (index 0)
//               ├── TraditionalWidget              (index 1)
//               ├── UserManagementWidget           (index 2)
//               └── SystemSettingsWidget           (index 3)
//
// 信号连接：
//   SessionManager::sessionChanged → FuseVision::onSessionChanged → refreshStatusBar
//   ThemeManager::themeChanged     → applyTheme（lambda）
//
// 交叉模块耦合点：
//   本文件直接 include 并实例化了 4 个功能 Widget，
//   是唯一了解所有子模块存在的地方（Facade 模式）。
// =============================================================================

class FuseVision : public QMainWindow
{
    Q_OBJECT

public:
    explicit FuseVision(QWidget* parent = nullptr);
    ~FuseVision();

    void setReadyStatus(bool ready);  // 设置状态栏"就绪"/"繁忙"

private slots:
    void onSessionChanged();          // 登录/登出 → 刷新状态栏用户名
    void onMenuItemSelected(int index); // 左侧菜单点击 → 切换 QStackedWidget 页面
    void onToggleTheme();             // 主题按钮点击 → Light/Dark 切换
    void onSwitchUser();              // 切换用户 → 登出 + 重新弹出登录框

private:
    void initUI();                    // UI 总入口（调用以下 4 个 init 方法）
    void initLeftMenu();              // 构建 48px 图标导航栏
    void initMainContent();           // 构建 QStackedWidget + 4 个页面
    void initStatusBar();             // 构建底部状态栏
    void applyTheme();                // 主题变更时重绘 UI（菜单背景、按钮图标）
    void logSystemInfo();             // 启动时输出系统信息日志
    void refreshStatusBar();          // 更新状态栏用户名 / 角色 / 就绪指示

    // ── UI 控件 ────────────────────────────────────────────────
    QWidget*         m_centralWidget  = nullptr;  // 中央容器
    QSplitter*       m_mainSplitter   = nullptr;  // 左右分割器
    QWidget*         m_menuWidget     = nullptr;  // 左侧菜单容器
    QListWidget*     m_topMenuList    = nullptr;  // 上方菜单（深度学习/传统视觉）
    QListWidget*     m_bottomMenuList = nullptr;  // 下方菜单（用户管理/系统设置）
    QStackedWidget*  m_stackedWidget  = nullptr;  // 右侧多页容器
    QPushButton*     m_themeBtn       = nullptr;  // 主题切换按钮（太阳/月亮图标）
    QPushButton*     m_switchUserBtn  = nullptr;  // 切换用户按钮

    // ── 状态栏控件 ─────────────────────────────────────────────
    QStatusBar* m_statusBar     = nullptr;
    QLabel*     m_userValueLabel = nullptr;  // 显示"用户名 (角色)"
    QLabel*     m_versionLabel  = nullptr;  // 显示版本号
    QLabel*     m_readyLabel    = nullptr;  // 显示"就绪"或"繁忙"

    // ── 功能页面（由主窗口持有，生命周期与主窗口相同） ──────────
    DeepLearningWidget*    m_deepLearningWidget    = nullptr;
    TraditionalWidget*     m_traditionalWidget     = nullptr;
    UserManagementWidget*  m_userManagementWidget  = nullptr;
    SystemSettingsWidget*  m_systemSettingsWidget  = nullptr;
};

#endif // FUSEVISION_H
