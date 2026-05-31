#include "FuseVision.h"
#include "core/SessionManager.h"
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

    refreshStatusBar();
}

FuseVision::~FuseVision()
{
    Logger::info("FuseVision application shutting down...");
}

void FuseVision::onSessionChanged()
{
    refreshStatusBar();
}

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

void FuseVision::applyTheme()
{
    qApp->setStyleSheet(ThemeManager::instance().styleSheet());

    const auto p = ThemeManager::instance().palette();

    m_menuWidget->setStyleSheet(
        QString("QWidget#menuContainer { background-color: %1; border-right: none; }").arg(p.menuBg));

    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    m_themeBtn->setText(isDark ? QString::fromUtf8("\u2600") : QString::fromUtf8("\u263D"));
    m_themeBtn->setStyleSheet(
        QString("QPushButton { font-size: 16px; border: none; background: transparent; color: %1; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary).arg(p.menuHoverBg));
}

void FuseVision::onToggleTheme()
{
    auto& tm = ThemeManager::instance();
    tm.setTheme((tm.currentTheme() == ThemeManager::Dark)
                ? ThemeManager::Light : ThemeManager::Dark);
}

void FuseVision::initUI()
{
    Logger::debug("Initializing main UI components...");

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int width = static_cast<int>(screenGeometry.width() * 0.7);
        int height = static_cast<int>(screenGeometry.height() * 0.7);
        resize(width, height);
        move((screenGeometry.width() - width) / 2, (screenGeometry.height() - height) / 2);
    }

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_centralWidget);
    m_mainSplitter->setChildrenCollapsible(false);

    initLeftMenu();
    initMainContent();
    initStatusBar();

    m_mainSplitter->addWidget(m_menuWidget);
    m_mainSplitter->addWidget(m_stackedWidget);
    m_mainSplitter->setSizes({ 48, width() - 48 });

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_mainSplitter);

    Logger::info("UI initialization completed successfully");
}

void FuseVision::initLeftMenu()
{
    Logger::debug("Initializing left menu (collapsible 48px, icon only)");

    m_menuWidget = new QWidget(this);
    m_menuWidget->setObjectName("menuContainer");
    m_menuWidget->setFixedWidth(48);
    m_menuWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    QVBoxLayout* menuLayout = new QVBoxLayout(m_menuWidget);
    menuLayout->setContentsMargins(0, 0, 0, 0);
    menuLayout->setSpacing(0);

    m_topMenuList = new QListWidget(this);
    m_topMenuList->setIconSize(QSize(28, 28));
    m_topMenuList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_topMenuList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_topMenuList->setFrameShape(QFrame::NoFrame);

    struct MenuItem { QString iconPath; QString text; };

    QList<MenuItem> topItems = {
        { ":/res/DeepLearningWidget.png", "深度学习" },
        { ":/res/TraditionalWidget.png",  "传统视觉" }
    };

    for (const auto& item : topItems) {
        QListWidgetItem* listItem = new QListWidgetItem(QIcon(item.iconPath), "");
        listItem->setToolTip(item.text);
        listItem->setSizeHint(QSize(48, 44));
        listItem->setTextAlignment(Qt::AlignCenter);
        m_topMenuList->addItem(listItem);
    }

    m_bottomMenuList = new QListWidget(this);
    m_bottomMenuList->setIconSize(QSize(28, 28));
    m_bottomMenuList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bottomMenuList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bottomMenuList->setFrameShape(QFrame::NoFrame);
    m_bottomMenuList->setFixedHeight(88);

    QList<MenuItem> bottomItems = {
        { ":/res/UserManagement.png", "用户管理" },
        { ":/res/SystemSettings.png", "系统设置" }
    };

    for (const auto& item : bottomItems) {
        QListWidgetItem* listItem = new QListWidgetItem(QIcon(item.iconPath), "");
        listItem->setToolTip(item.text);
        listItem->setSizeHint(QSize(48, 44));
        listItem->setTextAlignment(Qt::AlignCenter);
        m_bottomMenuList->addItem(listItem);
    }

    m_themeBtn = new QPushButton;
    m_themeBtn->setFixedSize(48, 36);
    m_themeBtn->setToolTip("切换主题");
    connect(m_themeBtn, &QPushButton::clicked, this, &FuseVision::onToggleTheme);

    if (m_topMenuList->count() > 0)
        m_topMenuList->setCurrentRow(0);

    connect(m_topMenuList, &QListWidget::currentRowChanged, this,
        [this](int row) {
            if (row < 0) return;
            m_bottomMenuList->setCurrentRow(-1);
            onMenuItemSelected(row);
        });
    connect(m_bottomMenuList, &QListWidget::currentRowChanged, this,
        [this](int row) {
            if (row < 0) return;
            m_topMenuList->setCurrentRow(-1);
            onMenuItemSelected(row + 2);
        });

    menuLayout->addWidget(m_topMenuList);
    menuLayout->addStretch();
    menuLayout->addWidget(m_bottomMenuList);
    menuLayout->addWidget(m_themeBtn, 0, Qt::AlignCenter);
    menuLayout->addSpacing(4);

    Logger::debug("Left menu initialized: 48px width, tooltip on hover");
}

void FuseVision::initMainContent()
{
    Logger::debug("Initializing main content area...");

    m_stackedWidget          = new QStackedWidget(this);
    m_deepLearningWidget     = new DeepLearningWidget(this);
    m_traditionalWidget      = new TraditionalWidget(this);
    m_userManagementWidget   = new UserManagementWidget(this);
    m_systemSettingsWidget   = new SystemSettingsWidget(this);

    m_stackedWidget->addWidget(m_deepLearningWidget);
    m_stackedWidget->addWidget(m_traditionalWidget);
    m_stackedWidget->addWidget(m_userManagementWidget);
    m_stackedWidget->addWidget(m_systemSettingsWidget);

    m_stackedWidget->setCurrentIndex(0);
}

void FuseVision::initStatusBar()
{
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);

    QLabel* userTitleLabel = new QLabel("用户:", this);
    m_userValueLabel = new QLabel(this);
    m_userValueLabel->setObjectName("userValueLabel");

    m_readyLabel = new QLabel("就绪", this);
    m_readyLabel->setObjectName("readyLabel");

    m_versionLabel = new QLabel(QApplication::applicationVersion(), this);
    m_versionLabel->setObjectName("versionLabel");

    m_statusBar->addPermanentWidget(userTitleLabel);
    m_statusBar->addPermanentWidget(m_userValueLabel);
    m_statusBar->addPermanentWidget(m_readyLabel);
    m_statusBar->addPermanentWidget(m_versionLabel);
}

void FuseVision::onMenuItemSelected(int index)
{
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
        m_readyLabel->setObjectName("readyLabel");
    } else {
        m_readyLabel->setText("繁忙");
        m_readyLabel->setObjectName("busyLabel");
    }
    applyTheme();
}

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
