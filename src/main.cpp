#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QCoreApplication>
#include <QStyleFactory>
#include "MainWindows/FuseVision.h"
#include "core/Logger.h"
#include "core/DatabaseManager.h"
#include "core/PermissionRegistry.h"
#include "core/SettingsManager.h"
#include "core/ThemeManager.h"
#include "MainWindows/Login.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef LOG_BUILD_LEVEL
#  ifdef NDEBUG
#    define LOG_BUILD_LEVEL Logger::Info
#  else
#    define LOG_BUILD_LEVEL Logger::Debug
#  endif
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    QApplication app(argc, argv);
    app.setOrganizationName("FuseVisionTeam");
    app.setApplicationName("FuseVision");
    app.setApplicationVersion("1.0.0");
    app.setStyle(QStyleFactory::create("Fusion"));

    ThemeManager::instance();
    app.setStyleSheet(ThemeManager::instance().styleSheet());

    auto& settings = SettingsManager::instance();
    settings.setNeedsRestart(false);

    QString logPath = settings.logPath();
    if (logPath.isEmpty())
        logPath = QCoreApplication::applicationDirPath() + "/logs/fusevision";
    Logger::init(logPath, LOG_BUILD_LEVEL);

    Logger::info("=== Application started ===");

    if (!DatabaseManager::instance().initDatabase()) {
        Logger::error("Failed to initialize database!");
        return -1;
    }

    PermissionRegistry::instance().registerModule("用户管理");
    PermissionRegistry::instance().registerModule("系统设置");
    PermissionRegistry::instance().registerModule("深度学习.模型训练");
    PermissionRegistry::instance().registerModule("深度学习.推理");
    PermissionRegistry::instance().registerModule("传统视觉.相机采集");
    PermissionRegistry::instance().registerModule("传统视觉.图像处理");

    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        Logger::info("Login cancelled, application will exit.");
        return 0;
    }

    QIcon appIcon(":/res/FuseVision.png");
    if (!appIcon.isNull())
        app.setWindowIcon(appIcon);

    FuseVision w;
    w.setWindowTitle("FuseVision - 智能视觉系统");
    w.show();
    w.showMaximized();

    int ret = app.exec();
    Logger::info(QString("=== Application exited with code: %1 ===").arg(ret));
    Logger::flush();
    return ret;
}
