#ifndef FUSEVISION_H
#define FUSEVISION_H

#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QAbstractButton>
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
//           ├── QWidget（m_menuWidget, 固定 44px）= 左侧图标菜单
//           │   ├── QPushButton x4（PNG 图标）= 4 个功能导航
//           │   ├── QPushButton（m_themeBtn）          = 主题切换
//           │   └── QPushButton（m_switchUserBtn）      = 切换用户
//           └── QStackedWidget（m_stackedWidget）       = 右侧内容区
//               ├── DeepLearningWidget              (index 0)
//               ├── TraditionalWidget               (index 1)
//               ├── UserManagementWidget            (index 2)
//               └── SystemSettingsWidget            (index 3)
//
// 侧边栏采用 QPushButton + QRC PNG 图标（checked 状态管理选中），
// 亮色/暗色两套 PNG 随主题自动切换，无需 Unicode 字符。
// =============================================================================

class FuseVision : public QMainWindow
{
    Q_OBJECT

public:
    explicit FuseVision(QWidget* parent = nullptr);
    ~FuseVision();

    void setReadyStatus(bool ready);  // 设置状态栏"就绪"/"繁忙"

private slots:
    void onSessionChanged();            // 登录/登出 → 刷新状态栏
    void onMenuItemClicked(QAbstractButton* btn); // 侧边栏导航按钮点击 → 切换页面
    void onToggleTheme();               // 主题按钮 → Light/Dark
    void onSwitchUser();                // 切换用户 → 登出 + 重新登录

private:
    void initUI();                      // UI 总入口
    void initSidebar();                 // 构建 44px PNG 图标侧边栏
    void updateNavIcons(bool isDark);   // 根据主题切换导航图标（亮色/暗色）
    void initMainContent();             // QStackedWidget + 4 页面
    void initStatusBar();               // 底部状态栏
    void applyTheme();                  // 主题变更时重绘
    void logSystemInfo();               // 系统信息日志
    void refreshStatusBar();            // 更新状态栏用户名/角色
    void setProjectName(int page, const QString& name); // 更新状态栏项目名

    // ── 布局容器 ──────────────────────────────────────────────
    QWidget*        m_centralWidget  = nullptr;
    QSplitter*      m_mainSplitter   = nullptr;
    QWidget*        m_menuWidget     = nullptr;  // 侧边栏容器（44px）
    QWidget*        m_contentContainer = nullptr; // 右侧内容容器
    QStackedWidget* m_stackedWidget  = nullptr;  // 右侧多页区
    // ── 侧边栏导航按钮（PNG 图标，checked 互斥）────────────
    QPushButton* m_navBtns[4] = {};  // [0]深度学习 [1]传统视觉 [2]用户管理 [3]系统设置
    QPushButton* m_themeBtn      = nullptr;  // 主题切换
    QPushButton* m_switchUserBtn = nullptr;  // 切换用户

    // ── 状态栏控件 ─────────────────────────────────────────────
    QStatusBar* m_statusBar      = nullptr;
    QLabel*     m_userValueLabel = nullptr;
    QLabel*     m_versionLabel   = nullptr;
    QLabel*     m_readyLabel     = nullptr;
    QLabel*     m_dlProjectLabel = nullptr;         // 深度学习项目名
    QLabel*     m_traditionalProjectLabel = nullptr; // 传统视觉项目名

    // ── 功能页面 ───────────────────────────────────────────────
    DeepLearningWidget*    m_deepLearningWidget    = nullptr;
    TraditionalWidget*     m_traditionalWidget     = nullptr;
    UserManagementWidget*  m_userManagementWidget  = nullptr;
    SystemSettingsWidget*  m_systemSettingsWidget  = nullptr;
};

#endif // FUSEVISION_H
