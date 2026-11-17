#include <QApplication>
#include <QMessageBox>
#include <QIcon>
#include <QDir>
#include <QCoreApplication>
#include <QStyleFactory>
#include <QRegularExpression>
#include <QSplashScreen>
#include <QPainter>
#include <QFont>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QTemporaryDir>
#include <cstdio>
#include "app/FuseVision.h"
#include "core/Logger.h"
#include "core/DatabaseManager.h"
#include "core/PermissionRegistry.h"
#include "core/SessionManager.h"
#include "core/SettingsManager.h"
#include "ui/ThemeManager.h"
#include "app/Login.h"

#ifdef _WIN32
#include <windows.h>
#endif

// =============================================================================
// Qt 消息过滤器 — 静默内部平台日志（setGeometry 等）
// =============================================================================
// Qt 平台层会输出大量无意义的几何调整日志，将其丢弃。
// Debug 模式下保留所有其他 Qt 日志（转发到 spdlog），
// Release 模式下仅转发 Warning 及以上。

static const QRegularExpression s_noisyPatterns[] = {
    QRegularExpression("QWindowsWindow::setGeometry"),
    QRegularExpression("QWindowsWindow::setWindowState"),
    QRegularExpression("QWindowsContext::windowsProc"),
};

static bool isNoisyQtMessage(const QString& text)
{
    for (const auto& re : s_noisyPatterns) {
        if (re.match(text).hasMatch())
            return true;
    }
    return false;
}

static void qtMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (isNoisyQtMessage(msg))
        return;  // 丢弃平台噪音

    switch (type) {
    case QtDebugMsg:
#ifndef NDEBUG
        Logger::debug(QString("[Qt] %1").arg(msg));
#endif
        break;
    case QtInfoMsg:
        Logger::info(QString("[Qt] %1").arg(msg));
        break;
    case QtWarningMsg:
        Logger::warn(QString("[Qt] %1").arg(msg));
        break;
    case QtCriticalMsg:
        Logger::error(QString("[Qt] %1").arg(msg));
        break;
    case QtFatalMsg:
        Logger::critical(QString("[Qt] FATAL: %1").arg(msg));
        break;
    }
}

// =============================================================================
// main.cpp — 应用入口 & 启动流程编排
// =============================================================================
// 这是整个应用的控制中枢，按严格顺序初始化所有全局单例。
//
// 启动流程（8 步，顺序不可打乱）：
//   1. Windows 控制台编码设为 UTF-8（否则中文乱码）
//   2. 创建 QApplication + 设置组织/应用名 + Fusion 风格
//   3. 初始化 ThemeManager（加载上次主题选择或检测系统主题）
//   4. 初始化 SettingsManager → Logger::init（在此之前不能打日志）
//   5. 初始化 DatabaseManager（打开 SQLite、建表、唯一管理账户）
//   6. 登记页面键（PermissionRegistry，无角色权限矩阵）
//   7. 以未登录只读状态启动
//   8. 主窗口事件循环（QApplication::exec）
//
// 编译时日志级别：
//   Debug 模式：LOG_BUILD_LEVEL = Logger::Debug（控制台输出 Debug 及以上）
//   Release 模式：LOG_BUILD_LEVEL = Logger::Info（仅 Info 及以上）
// =============================================================================

#ifndef LOG_BUILD_LEVEL
#  ifdef NDEBUG
#    define LOG_BUILD_LEVEL Logger::Info    // Release：仅 Info/Warn/Error/Critical
#  else
#    define LOG_BUILD_LEVEL Logger::Debug   // Debug：输出 Debug 及以上所有级别
#  endif
#endif

int main(int argc, char* argv[])
{
    const bool startupSmokeTest = qgetenv("FUSEVISION_STARTUP_SMOKE_TEST") == "1";
    // ── 步骤 1：Windows 控制台编码设置 ──────────────────────────
    // 将控制台代码页设为 UTF-8（65001），确保 spdlog 控制台日志
    // 和 QDebug 能正确输出中文
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // 标准输出 UTF-8
    SetConsoleCP(65001);        // 标准输入 UTF-8
#endif

    // ── 步骤 2：创建 QApplication ──────────────────────────────
    QApplication app(argc, argv);
    app.setOrganizationName("FuseVisionTeam");   // QSettings 组织名
    app.setApplicationName("FuseVision");        // QSettings 应用名
    app.setApplicationVersion("1.0.0");          // 版本号 → 状态栏显示
    app.setStyle(QStyleFactory::create("Fusion")); // Qt 内置 Fusion 风格（跨平台一致）

    // ── 步骤 3：初始化主题管理器 ───────────────────────────────
    // 在应用任何 Widget 之前初始化 ThemeManager，
    // 它会从 QSettings 恢复上次主题选择，或调用 detectSystemTheme()
    ThemeManager::instance();
    // 应用全局 QSS 样式表（必须在第一个 Widget 创建之前）
    app.setStyleSheet(ThemeManager::instance().styleSheet());

    // ── 步骤 4：初始化 SettingsManager & Logger ─────────────────
    // SettingsManager 独立于 Logger，提供日志路径和数据库路径的配置
    auto& settings = SettingsManager::instance();

    // 获取日志路径（默认 <exe>/logs/fusevision）
    QString logPath = settings.logPath();
    if (startupSmokeTest)
        logPath = QCoreApplication::applicationDirPath() + "/startup-smoke/logs";
    if (logPath.isEmpty())
        logPath = QCoreApplication::applicationDirPath() + "/logs/fusevision";
    // 初始化日志系统（从此刻起 Logger::info 等才能正常工作）
    Logger::init(logPath, LOG_BUILD_LEVEL);
    // 应用用户配置的日志级别（覆盖编译时默认值）
    Logger::setLevel(startupSmokeTest ? Logger::Info : static_cast<Logger::Level>(settings.logLevel()));

    // 安装 Qt 消息过滤器（静默平台噪音日志）
    qInstallMessageHandler(qtMessageHandler);

    Logger::info("=== Application started ===");

    // ── 步骤 5：初始化数据库 ───────────────────────────────────
    // DatabaseManager 打开 SQLite、建表，仅在空库创建唯一管理账户
    // 使用保存的数据库目录；空目标从上次活动库建立保留账户的副本。
    QString databaseError;
    if (!DatabaseManager::instance().initDatabase(startupSmokeTest ? QString(":memory:") : QString(),&databaseError)) {
        Logger::error("Failed to initialize database!");
        if(!startupSmokeTest) QMessageBox::critical(nullptr,QString::fromUtf8("数据库初始化失败"),databaseError);
        return -1;  // 数据库初始化失败，应用无法运行
    }


    // ── 步骤 7：注册权限模块 ───────────────────────────────────
    // 每个模块的权限点格式：一级模块名.子模块名
    // 仅保留页面键登记；不创建或读取角色权限记录。
    PermissionRegistry::instance().registerModule("账户设置");
    PermissionRegistry::instance().registerModule("系统设置");
    PermissionRegistry::instance().registerModule("深度学习.项目管理");
    PermissionRegistry::instance().registerModule("深度学习.模型管理");
    PermissionRegistry::instance().registerModule("深度学习.数据集管理");
    PermissionRegistry::instance().registerModule("深度学习.数据标注");
    PermissionRegistry::instance().registerModule("深度学习.数据检查");
    PermissionRegistry::instance().registerModule("深度学习.数据拆分");
    PermissionRegistry::instance().registerModule("深度学习.模型训练");
    PermissionRegistry::instance().registerModule("深度学习.模型预测");
    PermissionRegistry::instance().registerModule("深度学习.模型导出");
    PermissionRegistry::instance().registerModule("传统视觉.流程编辑");
    PermissionRegistry::instance().registerModule("传统视觉.相机采集");
    PermissionRegistry::instance().registerModule("传统视觉.相机标定");
    PermissionRegistry::instance().registerModule("传统视觉.相机建模");
    PermissionRegistry::instance().registerModule("传统视觉.通讯设置");
    PermissionRegistry::instance().registerModule("传统视觉.系统设置");
    PermissionRegistry::instance().registerModule("传统视觉.视觉工具区");

    if (startupSmokeTest) {
        auto& database = DatabaseManager::instance();
        const auto admin = database.validateLogin("superadmin", "admin123");
        if (!admin.success || database.validateLogin("superadmin", "incorrect-password").success
            || database.validateLogin("user", "123").success || database.allUsers().size()!=1) {
            Logger::error("Startup smoke test failed: authentication or permission initialization.");
            return 1;
        }

        // Exercise a Qt modal event loop without touching saved login settings.
        LoginDialog testLogin;
        QTimer::singleShot(0, &testLogin, &QDialog::reject);
        if (testLogin.exec() != QDialog::Rejected)
            return 1;

        QTemporaryDir testData;
        if (!testData.isValid())
            return 1;
        QImage image(24, 16, QImage::Format_RGB32);
        image.fill(Qt::green);
        for (const QString& format : {QString("png"), QString("jpg"), QString("bmp"),
                                      QString("tiff"), QString("webp")}) {
            const QString path = testData.filePath("codec." + format);
            if (!image.save(path) || QImage(path).size() != image.size()) {
                Logger::error("Startup smoke test failed: image codec " + format);
                return 1;
            }
        }
        settings.setProjectDataPath(testData.path() + "/projects");
        settings.setTraditionalDataPath(testData.path() + "/traditional");
        settings.setTraditionalCameraPath(testData.path() + "/cameras");
        SessionManager::instance().logout();
        FuseVision testWindow;
        testWindow.initPagesAsync();
        testWindow.resize(1280, 800);
        testWindow.show();
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
        const int result = app.exec();
        if (result != 0)
            return result;
        Logger::info("Startup smoke test completed successfully.");
        Logger::flush();
        std::puts("PASS startup image codecs and isolated initialization");
        return 0;
    }

    // Startup is always guest/read-only. Login is an optional main-window action.
    SessionManager::instance().logout();
    QIcon appIcon(":/res/FuseVision.png");
    if (!appIcon.isNull()) app.setWindowIcon(appIcon);

    // Show the splash while constructing the browsable main window.
    QWidget splashWidget;
    splashWidget.setFixedSize(480, 280);
    splashWidget.setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    splashWidget.setAutoFillBackground(true);
    splashWidget.setStyleSheet(
        "QWidget {"
        "  background-color: #1e1e28;"
        "  border: 1px solid rgba(255, 255, 255, 20);"
        "  border-radius: 12px;"
        "}");

    QVBoxLayout* splashLayout = new QVBoxLayout(&splashWidget);
    splashLayout->setContentsMargins(40, 50, 40, 36);
    splashLayout->setSpacing(16);

    QLabel* titleLabel = new QLabel("FuseVision", &splashWidget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "color: white; font-size: 26px; font-weight: bold; background: transparent; border: none;");
    splashLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(QString::fromUtf8("智能视觉系统"), &splashWidget);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet(
        "color: rgba(255,255,255,180); font-size: 13px; background: transparent; border: none;");
    splashLayout->addWidget(subtitleLabel);

    splashLayout->addSpacing(10);

    // 循环加载条（不指定具体进度，持续滚动示意加载中）
    QProgressBar* progressBar = new QProgressBar(&splashWidget);
    progressBar->setRange(0, 0);  // 0,0 = 不确定模式，循环滚动
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(6);
    progressBar->setStyleSheet(
        "QProgressBar { background: rgba(255,255,255,30); border: none; border-radius: 3px; }"
        "QProgressBar::chunk { background: #4a9eff; border-radius: 3px; }");
    splashLayout->addWidget(progressBar);

    splashLayout->addSpacing(8);

    QLabel* hintLabel = new QLabel(QString::fromUtf8("正在启动，请稍候..."), &splashWidget);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet(
        "color: rgba(255,255,255,120); font-size: 12px; background: transparent; border: none;");
    splashLayout->addWidget(hintLabel);

    splashLayout->addStretch();

    splashWidget.show();
    QApplication::processEvents();

    // 后台创建主窗口后关闭启动画面
    FuseVision* w = nullptr;
    QTimer::singleShot(100, [&]() {
        w = new FuseVision();
        w->setWindowTitle("FuseVision - 智能视觉系统");
        w->setAttribute(Qt::WA_DeleteOnClose);

        // 在 splash 期间创建子页面，processEvents() 保证进度条持续滚动
        w->initPagesAsync();

        w->showMaximized();
        splashWidget.close();
        QApplication::processEvents();
    });

    // 进入 Qt 事件循环（GIF 在 splashWidget 上持续播放）
    int ret = app.exec();
    Logger::info(QString("=== Application exited with code: %1 ===").arg(ret));

    // ── 清理 ──────────────────────────────────────────────────
    Logger::flush();  // 确保退出前所有日志落盘
    return ret;
}
