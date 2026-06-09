#include "FuseVision.h"
#include "core/SessionManager.h"
#include "Login.h"
#include <QFont>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#endif

// =============================================================================
// FuseVision.cpp — 主窗口实现
// =============================================================================
// 主窗口是应用的视觉根节点，包含左侧 PNG 图标菜单 + 右侧多页内容区。
// 构造流程：initUI() → applyTheme() → logSystemInfo() → 连接信号 → refreshStatusBar()
//
// 信号驱动：
//   SessionManager::sessionChanged → onSessionChanged → refreshStatusBar
//   ThemeManager::themeChanged      → applyTheme（lambda）
//
// 侧边栏导航：4 个 QPushButton（checked 互斥）+ QButtonGroup
//   图标使用 QRC 内置 PNG（亮色/暗色双套），由 applyTheme() 自动切换。
// =============================================================================

// ── 导航图标资源路径（QRC 内置，亮色 [0] / 暗色 [1] 两套）───
static const char* s_navIconRes[4][2] = {
    { ":/res/DeepLearningWidget.png", ":/res/DeepLearningWidget_D.png" },
    { ":/res/TraditionalWidget.png",  ":/res/TraditionalWidget_D.png"  },
    { ":/res/UserManagement.png",     ":/res/UserManagement_D.png"     },
    { ":/res/SystemSettings.png",     ":/res/SystemSettings_D.png"     },
};
static const char* s_navTips[4]   = { "深度学习", "传统视觉", "用户管理", "系统设置" };

// ── 页面标题（页面切换时动态更新窗口标题 + 页面顶部标题）───
static const char* s_pageTitles[4] = {
    "FuseVision - 深度学习 · 视觉AI引擎",
    "FuseVision - 传统视觉 · 经典算法引擎",
    "FuseVision - 用户管理",
    "FuseVision - 系统设置",
};

// ── 构造 / 析构 ───────────────────────────────────────────────

FuseVision::FuseVision(QWidget* parent)
    : QMainWindow(parent)
{
    Logger::info("FuseVision application starting...");
    initUI();
    applyTheme();
    logSystemInfo();

    connect(&SessionManager::instance(), &SessionManager::sessionChanged,
            this, &FuseVision::onSessionChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });

    if (SessionManager::instance().isLoggedIn())
        SessionManager::instance().broadcastSession();

    refreshStatusBar();
}

FuseVision::~FuseVision()
{
    Logger::info("FuseVision application shutting down...");
}

// ── 会话变更 ─────────────────────────────────────────────────

void FuseVision::onSessionChanged() { refreshStatusBar(); }

void FuseVision::refreshStatusBar()
{
    if (!m_userValueLabel) return;
    if (!SessionManager::instance().isLoggedIn()) {
        m_userValueLabel->setText("未登录");
        return;
    }
    const int role = SessionManager::instance().currentUserRole();
    const char* roleStr = (role == 2) ? " (超级管理员)"
                        : (role == 1) ? " (管理员)"
                        :               " (用户)";
    m_userValueLabel->setText(
        SessionManager::instance().currentUsername() + QString::fromUtf8(roleStr));
}

void FuseVision::setProjectName(int page, const QString& name)
{
    QString text = name.isEmpty() ? "无" : name;
    switch (page) {
    case 0:
        m_dlProjectLabel->setText("深度学习项目: " + text);
        m_dlProjectLabel->update();
        m_statusBar->update();
        break;
    case 1:
        m_traditionalProjectLabel->setText("传统视觉项目: " + text);
        m_traditionalProjectLabel->update();
        m_statusBar->update();
        break;
    }
}

// ── 主题应用 ─────────────────────────────────────────────────

void FuseVision::applyTheme()
{
    const auto& tm = ThemeManager::instance();
    qApp->setStyleSheet(tm.styleSheet());

    const auto p = tm.palette();
    bool isDark = (tm.currentTheme() == ThemeManager::Dark);

    // Windows 标题栏暗色适配
#ifdef Q_OS_WIN
    BOOL dark = isDark ? TRUE : FALSE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()),
                          DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    // 标题栏背景色跟随主题
    COLORREF captionColor = isDark ? RGB(30, 30, 30) : RGB(255, 255, 255);
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()),
                          DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
#endif

    // 侧边栏容器背景
    m_menuWidget->setStyleSheet(
        QString("QWidget#menuContainer { background-color: %1; border-right: none; }")
            .arg(p.menuBg));

    // 导航图标：亮色/暗色两套 PNG 切换
    updateNavIcons(isDark);

    // 主题按钮：亮色☀ / 暗色☽
    m_themeBtn->setText(isDark ? QString::fromUtf8("\u2600")   // ☀
                               : QString::fromUtf8("\u263D")); // ☽
    m_themeBtn->setStyleSheet(
        QString("QPushButton { font-size: 16px; border: none; background: transparent;"
                " color: %1; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary).arg(p.menuHoverBg));

    // 切换用户按钮
    m_switchUserBtn->setStyleSheet(
        QString("QPushButton { font-size: 14px; border: none; background: transparent;"
                " color: %1; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary).arg(p.menuHoverBg));
}

// ── 主题切换 ─────────────────────────────────────────────────

void FuseVision::onToggleTheme()
{
    auto& tm = ThemeManager::instance();
    tm.setTheme((tm.currentTheme() == ThemeManager::Dark)
                ? ThemeManager::Light : ThemeManager::Dark);
}

// ── 切换用户 ─────────────────────────────────────────────────

void FuseVision::onSwitchUser()
{
    Logger::info("Switch user requested");
    LoginDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        Logger::info("User switched successfully");
    // 注：login() 已由 LoginDialog 内部调用，覆盖当前会话并发射 sessionChanged，
    //     无需手动 logout()，否则会把新登录的用户登出。
}

// ── UI 总入口 ────────────────────────────────────────────────

void FuseVision::initUI()
{
    Logger::debug("Initializing main UI components...");

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        int w = static_cast<int>(sg.width() * 0.7);
        int h = static_cast<int>(sg.height() * 0.7);
        resize(w, h);
        move((sg.width() - w) / 2, (sg.height() - h) / 2);
    }

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_centralWidget);
    m_mainSplitter->setChildrenCollapsible(false);

    initSidebar();
    initMainContent();
    initStatusBar();

    // 初始显示深度学习项目名，隐藏传统视觉项目名
    m_traditionalProjectLabel->setVisible(false);

    m_mainSplitter->addWidget(m_menuWidget);
    m_mainSplitter->addWidget(m_contentContainer);
    m_mainSplitter->setSizes({ 44, width() - 44 });

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_mainSplitter);

    Logger::info("UI initialization completed successfully");
}

// ── 侧边栏（44px，PNG 图标，亮色/暗色双套）───────────────────

void FuseVision::initSidebar()
{
    Logger::debug("Initializing sidebar (44px, dual-theme PNG icons)");

    m_menuWidget = new QWidget(this);
    m_menuWidget->setObjectName("menuContainer");
    m_menuWidget->setFixedWidth(44);
    m_menuWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(m_menuWidget);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(0);

    // 4 个导航按钮（使用 QButtonGroup 互斥）
    QButtonGroup* group = new QButtonGroup(this);
    group->setExclusive(true);

    for (int i = 0; i < 4; ++i) {
        // 默认加载亮色图标，applyTheme() 时会切换到对应主题
        m_navBtns[i] = new QPushButton;
        m_navBtns[i]->setIcon(QIcon(s_navIconRes[i][0]));
        m_navBtns[i]->setIconSize(QSize(24, 24));
        m_navBtns[i]->setObjectName("sidebarNavBtn");     // QSS 匹配
        m_navBtns[i]->setToolTip(s_navTips[i]);
        m_navBtns[i]->setFixedSize(44, 44);
        m_navBtns[i]->setCheckable(true);
        m_navBtns[i]->setCursor(Qt::PointingHandCursor);
        group->addButton(m_navBtns[i], i);
        layout->addWidget(m_navBtns[i], 0, Qt::AlignCenter);
    }

    m_navBtns[0]->setChecked(true);  // 默认选中深度学习

    connect(group, &QButtonGroup::buttonClicked,
            this, &FuseVision::onMenuItemClicked);

    // 弹簧：顶部导航 / 底部操作分隔
    layout->addStretch();

    // 主题切换
    m_themeBtn = new QPushButton;
    m_themeBtn->setFixedSize(44, 36);
    m_themeBtn->setToolTip("切换主题");
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_themeBtn, &QPushButton::clicked, this, &FuseVision::onToggleTheme);
    layout->addWidget(m_themeBtn, 0, Qt::AlignCenter);

    // 切换用户
    m_switchUserBtn = new QPushButton(QString::fromUtf8("\u21C4"));  // ⇄
    m_switchUserBtn->setFixedSize(44, 36);
    m_switchUserBtn->setToolTip("切换用户");
    m_switchUserBtn->setCursor(Qt::PointingHandCursor);
    connect(m_switchUserBtn, &QPushButton::clicked, this, &FuseVision::onSwitchUser);
    layout->addWidget(m_switchUserBtn, 0, Qt::AlignCenter);

    layout->addSpacing(6);

    Logger::debug("Sidebar initialized: 44px, 4 nav (PNG) + 2 action buttons");
}

// ── 导航图标主题切换 ─────────────────────────────────────────

void FuseVision::updateNavIcons(bool isDark)
{
    const int idx = isDark ? 1 : 0;
    for (int i = 0; i < 4; ++i) {
        m_navBtns[i]->setIcon(QIcon(s_navIconRes[i][idx]));
        m_navBtns[i]->setIconSize(QSize(24, 24));
    }
}

// ── 导航点击（按钮指针由 QButtonGroup::buttonClicked 信号传入）─

void FuseVision::onMenuItemClicked(QAbstractButton* btn)
{
    if (!btn) return;

    for (int i = 0; i < 4; ++i) {
        if (m_navBtns[i] == btn) {
            m_stackedWidget->setCurrentIndex(i);
            setWindowTitle(QString::fromUtf8(s_pageTitles[i]));

            // 切换页面时刷新项目名显示
            if (i == 0)
                m_traditionalProjectLabel->setVisible(false),
                m_dlProjectLabel->setVisible(true);
            else if (i == 1)
                m_dlProjectLabel->setVisible(false),
                m_traditionalProjectLabel->setVisible(true);
            else
                m_dlProjectLabel->setVisible(false),
                m_traditionalProjectLabel->setVisible(false);

            return;
        }
    }
}

// ── 右侧内容区 ──────────────────────────────────────────────

void FuseVision::initMainContent()
{
    Logger::debug("Initializing main content area...");

    // 右侧内容区容器
    m_contentContainer = new QWidget;
    QVBoxLayout* contentLayout = new QVBoxLayout(m_contentContainer);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    m_deepLearningWidget    = new DeepLearningWidget(this);
    m_traditionalWidget     = new TraditionalWidget(this);
    m_userManagementWidget  = new UserManagementWidget(this);
    m_systemSettingsWidget  = new SystemSettingsWidget(this);

    // DL 项目变更 → 状态栏更新
    connect(m_deepLearningWidget, &DeepLearningWidget::dlProjectChanged,
            this, [this](const QString& name) { setProjectName(0, name); });

    m_stackedWidget->addWidget(m_deepLearningWidget);     // index 0
    m_stackedWidget->addWidget(m_traditionalWidget);      // index 1
    m_stackedWidget->addWidget(m_userManagementWidget);   // index 2
    m_stackedWidget->addWidget(m_systemSettingsWidget);   // index 3

    m_stackedWidget->setCurrentIndex(0);
    contentLayout->addWidget(m_stackedWidget, 1);
}

// ── 底部状态栏 ────────────────────────────────────────────────

void FuseVision::initStatusBar()
{
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);

    QLabel* userTitleLabel = new QLabel("用户:", this);
    m_userValueLabel = new QLabel(this);
    m_userValueLabel->setObjectName("userValueLabel");

    // 深度学习项目名
    m_dlProjectLabel = new QLabel("深度学习项目: 无", this);
    m_dlProjectLabel->setObjectName("projectLabel");

    // 传统视觉项目名
    m_traditionalProjectLabel = new QLabel("传统视觉项目: 无", this);
    m_traditionalProjectLabel->setObjectName("projectLabel");

    m_readyLabel = new QLabel("就绪", this);
    m_readyLabel->setObjectName("readyLabel");

    m_versionLabel = new QLabel(QApplication::applicationVersion(), this);
    m_versionLabel->setObjectName("versionLabel");

    m_statusBar->addWidget(userTitleLabel);
    m_statusBar->addWidget(m_userValueLabel);
    m_statusBar->addWidget(m_dlProjectLabel);
    m_statusBar->addWidget(m_traditionalProjectLabel);
    m_statusBar->addPermanentWidget(m_readyLabel);
    m_statusBar->addPermanentWidget(m_versionLabel);
}

void FuseVision::setReadyStatus(bool ready)
{
    if (ready) {
        m_readyLabel->setText("就绪");
        m_readyLabel->setObjectName("readyLabel");
    } else {
        m_readyLabel->setText("繁忙");
        m_readyLabel->setObjectName("busyLabel");
    }
    applyTheme();
}

// ── 系统信息日志 ──────────────────────────────────────────────

void FuseVision::logSystemInfo()
{
    Logger::info(QString("Screen count: %1").arg(QGuiApplication::screens().count()));
    if (QGuiApplication::primaryScreen()) {
        Logger::info(QString("Primary screen: %1x%2")
            .arg(QGuiApplication::primaryScreen()->size().width())
            .arg(QGuiApplication::primaryScreen()->size().height()));
    }
    Logger::info(QString("Theme: %1")
        .arg((ThemeManager::instance().currentTheme() == ThemeManager::Dark) ? "Dark" : "Light"));
}
