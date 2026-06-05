#include "FuseVision.h"
#include "core/SessionManager.h"
#include "Login.h"
#include <QListWidgetItem>
#include <QIcon>
#include <QFont>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

// =============================================================================
// FuseVision.cpp — 主窗口实现
// =============================================================================
// 主窗口是应用的视觉根节点，包含左侧图标菜单 + 右侧多页内容区。
// 构造流程：initUI() → applyTheme() → logSystemInfo() → 连接信号 → refreshStatusBar()
//
// 信号驱动（构造时自动连接，无需手动管理）：
//   SessionManager::sessionChanged → onSessionChanged → refreshStatusBar
//   ThemeManager::themeChanged      → lambda → applyTheme（切换主题时立即重绘）
//
// 左侧菜单导航：
//   QListWidget::currentRowChanged → lambda → onMenuItemSelected(index)
//   顶部分组（topMenuList）索引 0-1 → 深度学习/传统视觉
//   底部分组（bottomMenuList）索引 0-1（实际 index+2）→ 用户管理/系统设置
// =============================================================================

// ── 构造 / 析构 ───────────────────────────────────────────────

FuseVision::FuseVision(QWidget* parent)
    : QMainWindow(parent)
{
    Logger::info("FuseVision application starting...");
    initUI();          // 步骤 1：构建完整 UI 树
    applyTheme();      // 步骤 2：应用主题样式
    logSystemInfo();   // 步骤 3：打印系统环境信息

    // 步骤 4：连接信号
    // 登录/登出 → 刷新状态栏用户名/角色
    connect(&SessionManager::instance(), &SessionManager::sessionChanged,
            this, &FuseVision::onSessionChanged);
    // 主题切换 → 重新应用 QSS（lambda 直接调用 applyTheme）
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });

    // 步骤 5：如果已登录，重新广播 sessionChanged
    //         解决"初始登录先于 FuseVision 构造，所有监听者错过首次信号"的时序问题
    if (SessionManager::instance().isLoggedIn())
        SessionManager::instance().broadcastSession();

    refreshStatusBar(); // 步骤 6：初始渲染状态栏
}

FuseVision::~FuseVision()
{
    Logger::info("FuseVision application shutting down...");
}

// ── 会话变更 → 状态栏刷新 ─────────────────────────────────────

void FuseVision::onSessionChanged()
{
    refreshStatusBar();
}

void FuseVision::refreshStatusBar()
{
    if (!m_userValueLabel) return;

    // 未登录态
    if (!SessionManager::instance().isLoggedIn()) {
        m_userValueLabel->setText("未登录");
        return;
    }

    // 根据角色显示不同后缀
    const int role = SessionManager::instance().currentUserRole();
    const char* roleStr = (role == 2) ? " (超级管理员)"
                        : (role == 1) ? " (管理员)"
                        :               " (用户)";

    m_userValueLabel->setText(
        SessionManager::instance().currentUsername() + QString::fromUtf8(roleStr));
}

// ── 主题应用 ─────────────────────────────────────────────────

void FuseVision::applyTheme()
{
    // 全局 QSS（ThemeManager 统一管理所有样式）
    qApp->setStyleSheet(ThemeManager::instance().styleSheet());

    const auto p = ThemeManager::instance().palette();

    // 菜单容器背景（使用调色板中的 menuBg 颜色）
    m_menuWidget->setStyleSheet(
        QString("QWidget#menuContainer { background-color: %1; border-right: none; }").arg(p.menuBg));

    // 主题按钮图标：亮色模式显示太阳(☀)，暗色模式显示月亮(☽)
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    m_themeBtn->setText(isDark ? QString::fromUtf8("\u2600") : QString::fromUtf8("\u263D"));
    m_themeBtn->setStyleSheet(
        QString("QPushButton { font-size: 16px; border: none; background: transparent; color: %1; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary).arg(p.menuHoverBg));

    // 切换用户按钮：统一风格
    m_switchUserBtn->setStyleSheet(
        QString("QPushButton { font-size: 14px; border: none; background: transparent; color: %1; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary).arg(p.menuHoverBg));
}

// ── 主题切换 ─────────────────────────────────────────────────

void FuseVision::onToggleTheme()
{
    auto& tm = ThemeManager::instance();
    tm.setTheme((tm.currentTheme() == ThemeManager::Dark)
                ? ThemeManager::Light : ThemeManager::Dark);
    // ThemeManager::setTheme 会发射 themeChanged → applyTheme 自动被调用
}

// ── 切换用户 ─────────────────────────────────────────────────

void FuseVision::onSwitchUser()
{
    Logger::info("Switch user requested");
    SessionManager::instance().logout();  // 登出当前用户 → sessionChanged → refreshStatusBar

    // 重新弹出登录对话框（模态阻塞）
    LoginDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // 登录成功 → SessionManager 已更新 → sessionChanged → refreshStatusBar
        Logger::info("User switched successfully");
    }
}

// ── UI 总入口 ────────────────────────────────────────────────

void FuseVision::initUI()
{
    Logger::debug("Initializing main UI components...");

    // ── 窗口尺寸（70% 屏幕，居中定位） ──────────────────────────
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int width = static_cast<int>(screenGeometry.width() * 0.7);
        int height = static_cast<int>(screenGeometry.height() * 0.7);
        resize(width, height);
        move((screenGeometry.width() - width) / 2,
             (screenGeometry.height() - height) / 2);
    }

    // 中央容器
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    // 主分割器（水平：左菜单 | 右内容）
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_centralWidget);
    m_mainSplitter->setChildrenCollapsible(false);  // 不允许完全折叠

    // 构建左侧菜单和右侧内容区
    initLeftMenu();
    initMainContent();
    initStatusBar();

    // 装配到 Splitter
    m_mainSplitter->addWidget(m_menuWidget);     // 左侧：48px 图标菜单
    m_mainSplitter->addWidget(m_stackedWidget);   // 右侧：多页内容区
    m_mainSplitter->setSizes({ 48, width() - 48 });  // 初始比例

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_mainSplitter);

    Logger::info("UI initialization completed successfully");
}

// ── 左侧菜单栏（48px 折叠图标菜单） ────────────────────────────

void FuseVision::initLeftMenu()
{
    Logger::debug("Initializing left menu (collapsible 48px, icon only)");

    m_menuWidget = new QWidget(this);
    m_menuWidget->setObjectName("menuContainer");  // 匹配 ThemeManager QSS #menuContainer
    m_menuWidget->setFixedWidth(48);               // 固定 48px
    m_menuWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    QVBoxLayout* menuLayout = new QVBoxLayout(m_menuWidget);
    menuLayout->setContentsMargins(0, 0, 0, 0);
    menuLayout->setSpacing(0);

    // ===== 顶部菜单列表（深度学习 / 传统视觉） =====
    m_topMenuList = new QListWidget(this);
    m_topMenuList->setIconSize(QSize(28, 28));  // 图标 28x28
    m_topMenuList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_topMenuList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_topMenuList->setFrameShape(QFrame::NoFrame);
    m_topMenuList->setFixedWidth(48);            // 与菜单容器等宽

    struct MenuItem { QString iconPath; QString text; };

    // 顶部功能页（索引 0-1）
    QList<MenuItem> topItems = {
        { ":/res/DeepLearningWidget.png", "深度学习" },
        { ":/res/TraditionalWidget.png",  "传统视觉" }
    };

    for (const auto& item : topItems) {
        QListWidgetItem* listItem = new QListWidgetItem(QIcon(item.iconPath), "");
        listItem->setToolTip(item.text);            // 悬停提示
        listItem->setSizeHint(QSize(48, 48));       // 统一 48x48
        listItem->setTextAlignment(Qt::AlignCenter); // 图标居中
        m_topMenuList->addItem(listItem);
    }

    // ===== 底部菜单列表（用户管理 / 系统设置） =====
    m_bottomMenuList = new QListWidget(this);
    m_bottomMenuList->setIconSize(QSize(28, 28));
    m_bottomMenuList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bottomMenuList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bottomMenuList->setFrameShape(QFrame::NoFrame);
    m_bottomMenuList->setFixedWidth(48);            // 与顶部等宽，保证图标同垂线
    m_bottomMenuList->setFixedHeight(96);           // 固定高度：2 个图标

    QList<MenuItem> bottomItems = {
        { ":/res/UserManagement.png", "用户管理" },
        { ":/res/SystemSettings.png", "系统设置" }
    };

    for (const auto& item : bottomItems) {
        QListWidgetItem* listItem = new QListWidgetItem(QIcon(item.iconPath), "");
        listItem->setToolTip(item.text);
        listItem->setSizeHint(QSize(48, 48));
        listItem->setTextAlignment(Qt::AlignCenter);
        m_bottomMenuList->addItem(listItem);
    }

    // 主题切换按钮（放在底部菜单下方）
    m_themeBtn = new QPushButton;
    m_themeBtn->setFixedSize(48, 36);
    m_themeBtn->setToolTip("切换主题");
    connect(m_themeBtn, &QPushButton::clicked, this, &FuseVision::onToggleTheme);

    // 切换用户按钮
    m_switchUserBtn = new QPushButton;
    m_switchUserBtn->setFixedSize(48, 36);
    m_switchUserBtn->setText(QString::fromUtf8("\u21C4"));  // ⇄ 切换图标
    m_switchUserBtn->setToolTip("切换用户");
    connect(m_switchUserBtn, &QPushButton::clicked, this, &FuseVision::onSwitchUser);

    // 默认选中第一个菜单项
    if (m_topMenuList->count() > 0)
        m_topMenuList->setCurrentRow(0);

    // 顶部菜单点击 → 切换页面（索引 0-1）
    connect(m_topMenuList, &QListWidget::currentRowChanged, this,
        [this](int row) {
            if (row < 0) return;
            m_bottomMenuList->setCurrentRow(-1);  // 取消底部选中
            onMenuItemSelected(row);              // DeepLearningWidget / TraditionalWidget
        });
    // 底部菜单点击 → 切换页面（索引 2-3，即 row + 2）
    connect(m_bottomMenuList, &QListWidget::currentRowChanged, this,
        [this](int row) {
            if (row < 0) return;
            m_topMenuList->setCurrentRow(-1);     // 取消顶部选中
            onMenuItemSelected(row + 2);          // UserManagement / SystemSettings
        });

    // 垂直布局：顶部菜单 + 弹簧 + 底部菜单 + 主题按钮 + 切换用户
    menuLayout->addWidget(m_topMenuList);
    menuLayout->addStretch();
    menuLayout->addWidget(m_bottomMenuList);
    menuLayout->addWidget(m_themeBtn, 0, Qt::AlignCenter);
    menuLayout->addWidget(m_switchUserBtn, 0, Qt::AlignCenter);
    menuLayout->addSpacing(4);

    Logger::debug("Left menu initialized: 48px width, tooltip on hover");
}

// ── 右侧内容区（QStackedWidget） ─────────────────────────────

void FuseVision::initMainContent()
{
    Logger::debug("Initializing main content area...");

    m_stackedWidget = new QStackedWidget(this);

    // 实例化 4 个功能页面（主窗口持有所有权）
    m_deepLearningWidget     = new DeepLearningWidget(this);
    m_traditionalWidget      = new TraditionalWidget(this);
    m_userManagementWidget   = new UserManagementWidget(this);
    m_systemSettingsWidget   = new SystemSettingsWidget(this);

    // 按索引顺序添加（与 onMenuItemSelected 的 switch 对应）
    m_stackedWidget->addWidget(m_deepLearningWidget);     // index 0
    m_stackedWidget->addWidget(m_traditionalWidget);      // index 1
    m_stackedWidget->addWidget(m_userManagementWidget);   // index 2
    m_stackedWidget->addWidget(m_systemSettingsWidget);   // index 3

    m_stackedWidget->setCurrentIndex(0);  // 默认显示深度学习页面
}

// ── 底部状态栏 ────────────────────────────────────────────────

void FuseVision::initStatusBar()
{
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);

    // 用户标签（"用户:" 前缀 + 用户名/角色）
    QLabel* userTitleLabel = new QLabel("用户:", this);
    m_userValueLabel = new QLabel(this);
    m_userValueLabel->setObjectName("userValueLabel");

    // 就绪/繁忙指示灯
    m_readyLabel = new QLabel("就绪", this);
    m_readyLabel->setObjectName("readyLabel");  // 匹配 ThemeManager QSS #readyLabel

    // 版本号
    m_versionLabel = new QLabel(QApplication::applicationVersion(), this);
    m_versionLabel->setObjectName("versionLabel");  // 匹配 QSS #versionLabel

    // 左对齐：用户信息（addWidget）
    m_statusBar->addWidget(userTitleLabel);
    m_statusBar->addWidget(m_userValueLabel);
    // 右对齐：就绪灯 + 版本号（addPermanentWidget）
    m_statusBar->addPermanentWidget(m_readyLabel);
    m_statusBar->addPermanentWidget(m_versionLabel);
}

// ── 页面切换 ─────────────────────────────────────────────────

void FuseVision::onMenuItemSelected(int index)
{
    // 索引与 initMainContent 中 addWidget 的顺序严格对应
    switch (index) {
    case 0: m_stackedWidget->setCurrentWidget(m_deepLearningWidget);    break;
    case 1: m_stackedWidget->setCurrentWidget(m_traditionalWidget);     break;
    case 2: m_stackedWidget->setCurrentWidget(m_userManagementWidget);  break;
    case 3: m_stackedWidget->setCurrentWidget(m_systemSettingsWidget);  break;
    }
}

void FuseVision::setReadyStatus(bool ready)
{
    if (ready) {
        m_readyLabel->setText("就绪");
        m_readyLabel->setObjectName("readyLabel");  // 绿色（QSS #readyLabel）
    } else {
        m_readyLabel->setText("繁忙");
        m_readyLabel->setObjectName("busyLabel");   // 橙色（QSS #busyLabel）
    }
    applyTheme();  // 重新应用 QSS，使 objectName 变更生效
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
