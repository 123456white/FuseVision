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
//   5. 初始化 DatabaseManager（打开 SQLite、建表、种子用户）
//   6. 注册权限模块（PermissionRegistry）
//   7. 登录对话框（模态阻塞）
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
    if (logPath.isEmpty())
        logPath = QCoreApplication::applicationDirPath() + "/logs/fusevision";
    // 初始化日志系统（从此刻起 Logger::info 等才能正常工作）
    Logger::init(logPath, LOG_BUILD_LEVEL);
    // 应用用户配置的日志级别（覆盖编译时默认值）
    Logger::setLevel(static_cast<Logger::Level>(settings.logLevel()));

    Logger::info("=== Application started ===");

    // ── 步骤 5：初始化数据库 ───────────────────────────────────
    // DatabaseManager 自动打开 SQLite、建表、插入种子用户
    // 路径：<exe>/data/fusevision.db
    if (!DatabaseManager::instance().initDatabase()) {
        Logger::error("Failed to initialize database!");
        return -1;  // 数据库初始化失败，应用无法运行
    }

    // ── 步骤 6：权限数据迁移（一次性） ─────────────────────────
    DatabaseManager::instance().migratePermissionKey("传统视觉.图像处理", "传统视觉.视觉工具区");

    // ── 步骤 7：注册权限模块 ───────────────────────────────────
    // 每个模块的权限点格式：一级模块名.子模块名
    // 注册后 PermissionRegistry 会自动在所有用户的 user_permissions 表中
    // 创建对应记录（管理员可读写，普通用户只读）
    PermissionRegistry::instance().registerModule("用户管理");
    PermissionRegistry::instance().registerModule("系统设置");
    PermissionRegistry::instance().registerModule("深度学习.项目管理");
    PermissionRegistry::instance().registerModule("深度学习.模型管理");
    PermissionRegistry::instance().registerModule("深度学习.数据集管理");
    PermissionRegistry::instance().registerModule("深度学习.数据标注");
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

    // ── 步骤 8：显示登录对话框 ─────────────────────────────────
    // exec() 为模态阻塞调用：
    //   - 点击"登录"→ validateLogin → SessionManager::login → accept() → 返回 Accepted
    //   - 点击"取消"→ reject() → 返回 Rejected → return 0 退出

    // 设置应用图标（必须在登录对话框之前，否则登录窗口无图标）
    QIcon appIcon(":/res/FuseVision.png");
    if (!appIcon.isNull())
        app.setWindowIcon(appIcon);

    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        Logger::info("Login cancelled, application will exit.");
        return 0;  // 用户取消登录，正常退出
    }

    // ── 步骤 9：创建主窗口 & 进入事件循环 ───────────────────────
    // 创建主窗口（构造函数中自动初始化 UI、连接信号）
    FuseVision w;
    w.setWindowTitle("FuseVision - 智能视觉系统");
    w.show();
    w.showMaximized();  // 启动时最大化

    // 进入 Qt 事件循环（阻塞直到所有窗口关闭）
    int ret = app.exec();
    Logger::info(QString("=== Application exited with code: %1 ===").arg(ret));

    // ── 清理 ──────────────────────────────────────────────────
    Logger::flush();  // 确保退出前所有日志落盘
    return ret;
}
