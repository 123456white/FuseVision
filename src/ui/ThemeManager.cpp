#include "ThemeManager.h"
#include <QSettings>
#include <QOperatingSystemVersion>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ThemePalette lightPalette()
{
    return {
        // ── 背景层级（由深到浅） ────────────────────────────
        "#f0f2f5",  // bgPrimary    主窗口背景
        "#ffffff",  // bgSecondary  卡片/对话框/GroupBox 背景
        "#f5f6f8",  // bgTertiary   交替行 / Header 背景
        "#e8ecf0",  // bgHover      hover 态
        "#dce6f8",  // bgSelected   选中态（淡蓝）
        "#f0f0f0",  // bgDisabled   禁用态

        // ── 文字层级（由主到次） ────────────────────────────
        "#1a202c",  // textPrimary  正文
        "#4a5568",  // textSecondary 次要文字 / 菜单图标
        "#8896ab",  // textMuted    占位符 / 提示 / 禁用文字
        "#4a7de0",  // textAccent   链接 / 强调文字

        // ── 边框层级 ────────────────────────────────────────
        "#dde1e6",  // borderLight  分割线 / GroupBox 边框
        "#c4cad3",  // borderMedium 输入框边框
        "#4a7de0",  // borderFocus  聚焦态边框（蓝色）

        // ── 主题色 ──────────────────────────────────────────
        "#4a7de0",  // accentPrimary   主色按钮 / 选中指示条
        "#3b6cc7",  // accentHover     主色 hover
        "#305aa8",  // accentPressed   主色 pressed

        // ── 语义色 ──────────────────────────────────────────
        "#2ea043",  // success   成功 / 就绪绿
        "#d97706",  // warning   警告橙
        "#dc2626",  // danger    错误红

        // ── 浮层 ─────────────────────────────────────────────
        "#ffffff",  // surfaceWhite  下拉面板 / 输入框白底
        "#f8f9fb",  // surfaceCard   ToolTip 底

        // ── 左侧菜单 ─────────────────────────────────────────
        "#f7f8fa",  // menuBg          菜单背景
        "#dce6f8",  // menuSelectedBg  菜单选中
        "#eef1f5",  // menuHoverBg     菜单 hover
        "none",     // menuBorder      无边框

        // ── 状态栏 ──────────────────────────────────────────
        "#f0f2f5",  // statusBarBg
        "#dde1e6",  // statusBarBorder

        // ── 分割线 ──────────────────────────────────────────
        "#dde1e6",  // splitterHandle

        // ── 滚动条 ──────────────────────────────────────────
        "#eef0f4",  // scrollbarBg
        "#c0c7cf",  // scrollbarHandle
        "#a0a8b0",  // scrollbarHandleHover
    };
}

ThemePalette darkPalette()
{
    return {
        // ── 背景层级（由深到浅） ────────────────────────────
        "#0f172a",  // bgPrimary    主窗口背景（更深更纯）
        "#1e293b",  // bgSecondary  卡片/对话框/GroupBox 背景
        "#253449",  // bgTertiary   交替行 / Header 背景（与 surface 拉开）
        "#334155",  // bgHover      hover 态（明显提亮）
        "#1e3a6b",  // bgSelected   选中态（蓝底，与背景拉开）
        "#1e293b",  // bgDisabled   禁用态

        // ── 文字层级（由主到次） ────────────────────────────
        "#e8edf2",  // textPrimary  正文（微暖减少刺眼）
        "#94a3b8",  // textSecondary 次要文字
        "#64748b",  // textMuted    占位符 / 提示 / 禁用文字
        "#6b9ff2",  // textAccent   链接 / 强调文字

        // ── 边框层级（整体提亮 15~20%） ─────────────────────
        "#3b4f6b",  // borderLight  分割线 / GroupBox 边框
        "#4e6380",  // borderMedium 输入框边框
        "#5b8def",  // borderFocus  聚焦态边框

        // ── 主题色 ──────────────────────────────────────────
        "#5b8def",  // accentPrimary
        "#4a7de0",  // accentHover
        "#3d6bc7",  // accentPressed

        // ── 语义色 ──────────────────────────────────────────
        "#2ea043",  // success
        "#e08a30",  // warning
        "#e5534b",  // danger

        // ── 浮层 ─────────────────────────────────────────────
        "#1e293b",  // surfaceWhite  下拉面板底（与 bgSecondary 同色）
        "#162033",  // surfaceCard   ToolTip 底（更深）

        // ── 左侧菜单 ─────────────────────────────────────────
        "#0f172a",  // menuBg         与主背景同色
        "#1e3a6b",  // menuSelectedBg 菜单选中（蓝底）
        "#1e3348",  // menuHoverBg    菜单 hover
        "none",     // menuBorder

        // ── 状态栏 ──────────────────────────────────────────
        "#0f172a",  // statusBarBg
        "#3b4f6b",  // statusBarBorder

        // ── 分割线 ──────────────────────────────────────────
        "#3b4f6b",  // splitterHandle

        // ── 滚动条 ──────────────────────────────────────────
        "#1e293b",  // scrollbarBg
        "#3b4f6b",  // scrollbarHandle
        "#4e6380",  // scrollbarHandleHover
    };
}

ThemeManager& ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager()
    : QObject(nullptr)
{
    load();
}

void ThemeManager::load()
{
    QSettings s("FuseVisionTeam", "FuseVision");
    QString stored = s.value("ui/theme", "").toString();

    if (stored == "dark")
        m_theme = Dark;
    else if (stored == "light")
        m_theme = Light;
    else
        m_theme = detectSystemTheme();
}

void ThemeManager::save()
{
    QSettings s("FuseVisionTeam", "FuseVision");
    s.setValue("ui/theme", (m_theme == Dark) ? "dark" : "light");
}

ThemeManager::Theme ThemeManager::currentTheme() const { return m_theme; }

void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;
    save();

    emit themeChanged(m_theme);
}

ThemePalette ThemeManager::palette() const
{
    return (m_theme == Dark) ? darkPalette() : lightPalette();
}

QString ThemeManager::styleSheet() const
{
    return buildGlobalQSS()
         + buildWidgetQSS()
         + buildButtonQSS()
         + buildTableQSS()
         + buildFormQSS()
         + buildMenuQSS()
         + buildStatusBarQSS()
         + buildSplitterQSS()
         + buildGroupBoxQSS()
         + buildToolBarQSS()
         + buildToolButtonQSS()
         + buildScrollBarQSS()
         + buildDialogQSS()
         + buildTabBarQSS()
         + buildTreeQSS()
         + buildSidebarBtnQSS()
         + buildLogMonitorQSS();
}

ThemeManager::Theme ThemeManager::detectSystemTheme() const
{
#ifdef Q_OS_WIN
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
    }
    return (value == 0) ? Dark : Light;
#elif defined(Q_OS_MACOS)
    return Light;
#else
    return Light;
#endif
}

QString ThemeManager::buildGlobalQSS() const
{
    const auto p = palette();
    return QString(R"(
        /* ── 全局基础 ─────────────────────────────────────── */
        QMainWindow { background-color: %1; }
        QWidget   { color: %2; font-family: "Microsoft YaHei", "Segoe UI", sans-serif; }
        QToolTip  { background-color: %3; color: %2; border: 1px solid %4; padding: 4px 8px; border-radius: 4px; }
        QMenu     { background-color: %5; color: %2; border: 1px solid %4; padding: 4px; }
        QMenu::item { padding: 6px 24px; }
        QMenu::item:selected { background-color: %6; }
        QMenu::separator { height: 1px; background-color: %4; margin: 4px 8px; }
    )").arg(p.bgPrimary).arg(p.textPrimary).arg(p.surfaceCard)
       .arg(p.borderLight).arg(p.surfaceWhite).arg(p.bgSelected);
}

QString ThemeManager::buildWidgetQSS() const
{
    const auto p = palette();
    return QString(R"(
        QLabel#sectionTitle {
            font-size: 13px;
            font-weight: bold;
            color: %1;
            background-color: %8;
            border-bottom: 2px solid %7;
            padding: 6px 10px;
        }
        QFrame#sectionFrame {
            border: 1px solid %7;
            border-radius: 6px;
            background-color: %6;
        }
        QLabel#infoLabel {
            color: %2;
            font-size: 12px;
            padding: 4px 0;
        }
        QLabel#placeholderLabel {
            color: %5;
            font-size: 13px;
        }
        QLabel#hintLabel {
            color: %5;
            font-size: 12px;
        }
        QFrame#dlContentFrame {
            border: 1px solid %7;
            border-radius: 6px;
            background-color: %6;
        }
        QStackedWidget#editorStack {
            background-color: transparent;
            border: none;
        }
        QStackedWidget {
            background-color: %6;
        }
        QToolBox {
            background-color: transparent;
            border: none;
        }
        QToolBox::tab {
            background-color: %8;
            color: %1;
            padding: 6px 8px;
            min-height: 18px;
            font-size: 13px;
            font-weight: normal;
            border: 1px solid transparent;
            border-radius: 4px;
        }
        QToolBox::tab:selected {
            background-color: %4;
            color: %3;
            font-weight: bold;
            border-left: 3px solid %3;
        }
        QToolBox::tab:hover:!selected {
            background-color: %4;
            color: %1;
        }
        QToolBox QWidget {
            background-color: transparent;
        }
    )").arg(p.textPrimary).arg(p.textMuted).arg(p.accentPrimary).arg(p.bgSelected)
       .arg(p.textMuted).arg(p.bgPrimary).arg(p.borderMedium).arg(p.bgSecondary);
}

QString ThemeManager::buildButtonQSS() const
{
    const auto p = palette();
    return QString(R"(
        QPushButton {
            padding: 7px 18px;
            border-radius: 4px;
            border: 1px solid %1;
            background-color: %2;
            color: %3;
            font-size: 13px;
            min-height: 22px;
        }
        QPushButton:hover  { background-color: %4; }
        QPushButton:pressed{ background-color: %5; }
        QPushButton:disabled { color: %6; background-color: %7; }
        QPushButton#primaryBtn {
            background-color: %8; color: #ffffff; border: none; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background-color: %9; }
        QPushButton#primaryBtn:pressed{ background-color: %10; }
        QPushButton#loginBtn {
            background-color: %8; color: #ffffff; border: none; font-weight: bold; padding: 6px 20px;
        }
        QPushButton#loginBtn:hover   { background-color: %9; }
        QPushButton#loginBtn:pressed { background-color: %10; }
        QPushButton#cancelBtn {
            padding: 6px 20px; border-radius: 4px;
            border: 1px solid %1; background-color: %2; color: %3;
        }
        QPushButton#cancelBtn:hover { background-color: %4; }
    )").arg(p.borderMedium).arg(p.bgDisabled).arg(p.textPrimary)
       .arg(p.bgHover).arg(p.bgTertiary).arg(p.textMuted).arg(p.bgTertiary)
       .arg(p.accentPrimary).arg(p.accentHover).arg(p.accentPressed);
}

QString ThemeManager::buildTableQSS() const
{
    const auto p = palette();
    return QString(R"(
        QTableWidget {
            border: 1px solid %1;
            border-radius: 6px;
            gridline-color: %2;
            background-color: %3;
            alternate-background-color: %4;
        }
        QTableWidget::item { padding: 4px 8px; font-size: 13px; }
        QTableWidget::item:selected { background-color: %5; color: %6; }
        QHeaderView::section {
            background-color: %7;
            color: %6;
            border: none;
            border-bottom: 2px solid %1;
            padding: 6px 8px;
            font-weight: bold;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
        }
    )").arg(p.borderLight).arg(p.borderLight).arg(p.surfaceWhite)
       .arg(p.bgTertiary).arg(p.bgSelected).arg(p.textPrimary)
       .arg(p.bgTertiary);
}

QString ThemeManager::buildFormQSS() const
{
    const auto p = palette();
    return QString(R"(
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            padding: 6px 10px;
            border: 1px solid %1;
            border-radius: 4px;
            background-color: %2;
            color: %3;
            min-height: 20px;
            font-size: 13px;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: %4;
        }
        QComboBox::drop-down {
            border: none;
            padding-right: 6px;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background-color: %2;
            border: 1px solid %1;
            border-radius: 4px;
            selection-background-color: %5;
            color: %3;
            outline: none;
        }
        QLabel {
            font-size: 13px;
            color: %3;
        }
    )").arg(p.borderMedium).arg(p.surfaceWhite).arg(p.textPrimary)
       .arg(p.accentPrimary).arg(p.bgSelected);
}

QString ThemeManager::buildMenuQSS() const
{
    const auto p = palette();
    return QString(R"(
        QListWidget {
            background-color: %1;
            border: none;
            outline: none;
            font-size: 12px;
            color: %5;
        }
        QListWidget::item {
            height: 44px;
            padding: 0px;
            margin: 0px;
            text-align: center;
            color: %5;
        }
        QListWidget::item:selected {
            background-color: %2;
            border-left: 3px solid %3;
        }
        QListWidget::item:hover { background-color: %4; }
    )").arg(p.menuBg).arg(p.menuSelectedBg).arg(p.accentPrimary).arg(p.menuHoverBg).arg(p.textSecondary);
}

QString ThemeManager::buildStatusBarQSS() const
{
    const auto p = palette();
    return QString(R"(
        QStatusBar { background-color: %1; border-top: 1px solid %2; min-height: 26px; font-size: 12px; }
        QStatusBar QLabel { padding: 0 6px; color: %3; }
        QLabel#readyLabel { color: %4; font-weight: bold; }
        QLabel#busyLabel  { color: %6; font-weight: bold; }
        QLabel#versionLabel { color: %5; }
        QLabel#projectLabel { color: %3; font-weight: bold; margin-left: 8px; }
    )").arg(p.statusBarBg).arg(p.statusBarBorder).arg(p.textPrimary)
       .arg(p.success).arg(p.accentPrimary).arg(p.warning);
}

QString ThemeManager::buildSplitterQSS() const
{
    const auto p = palette();
    return QString(R"(
        QSplitter::handle { background-color: %1; width: 1px; }
    )").arg(p.splitterHandle);
}

QString ThemeManager::buildGroupBoxQSS() const
{
    const auto p = palette();
    return QString(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid %1;
            border-radius: 8px;
            margin-top: 12px;
            padding: 18px 12px 12px 12px;
            background-color: %2;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: %3;
        }
    )").arg(p.borderLight).arg(p.bgSecondary).arg(p.textPrimary);
}

QString ThemeManager::buildToolBarQSS() const
{
    const auto p = palette();
    return QString(R"(
        QToolBar {
            background-color: %1;
            border-bottom: 1px solid %2;
            padding: 4px;
            spacing: 4px;
        }
    )").arg(p.bgSecondary).arg(p.borderLight);
}

// ── 滚动条 ──────────────────────────────────────────────────

QString ThemeManager::buildScrollBarQSS() const
{
    const auto p = palette();
    return QString(R"(
        QScrollBar:vertical {
            background-color: %1;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background-color: %2;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background-color: %3; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QScrollBar:horizontal {
            background-color: %1;
            height: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background-color: %2;
            border-radius: 4px;
            min-width: 24px;
        }
        QScrollBar::handle:horizontal:hover { background-color: %3; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
    )").arg(p.scrollbarBg).arg(p.scrollbarHandle).arg(p.scrollbarHandleHover);
}

QString ThemeManager::buildDialogQSS() const
{
    const auto p = palette();
    return QString(R"(
        QDialog {
            background-color: %1;
        }
        QDialog QLabel {
            color: %2;
        }
        QMessageBox {
            background-color: %1;
        }
    )").arg(p.bgSecondary).arg(p.textPrimary);
}

QString ThemeManager::buildTabBarQSS() const
{
    const auto p = palette();
    return QString(R"(
        QTabBar {
            background-color: %1;
        }
        QTabBar::tab {
            background-color: %1;
            color: %2;
            padding: 7px 18px;
            min-width: 64px;
            border: 1px solid transparent;
            border-bottom: 2px solid transparent;
            border-radius: 4px;
            margin-right: 2px;
            font-size: 13px;
            min-height: 22px;
        }
        QTabBar::tab:selected {
            color: %3;
            border-bottom: 2px solid %3;
            font-weight: bold;
            background-color: %5;
        }
        QTabBar::tab:hover:!selected {
            color: %4;
            background-color: %5;
            border-color: %6;
        }
    )").arg(p.bgPrimary).arg(p.textSecondary).arg(p.accentPrimary)
       .arg(p.textPrimary).arg(p.bgHover).arg(p.borderMedium);
}

QString ThemeManager::buildTreeQSS() const
{
    const auto p = palette();
    return QString(R"(
        QTreeWidget {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            outline: none;
        }
        QTreeWidget::item {
            padding: 4px 8px;
        }
        QTreeWidget::item:selected {
            background-color: %4;
            color: %5;
        }
        QTreeWidget::item:hover:!selected {
            background-color: %6;
        }
        QTreeWidget::branch {
            background-color: %1;
        }
    )").arg(p.surfaceWhite).arg(p.textPrimary).arg(p.borderLight)
       .arg(p.bgSelected).arg(p.textPrimary).arg(p.bgHover);
}

// ── 工具栏按钮（QToolButton / QToolBar 中的 QAction 按钮） ────

QString ThemeManager::buildToolButtonQSS() const
{
    const auto p = palette();
    return QString(R"(
        QToolBar QToolButton {
            background-color: transparent;
            color: %1;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 7px 18px;
            font-size: 13px;
            min-height: 22px;
        }
        QToolBar QToolButton:hover {
            background-color: %2;
            border-color: %3;
        }
        QToolBar QToolButton:pressed {
            background-color: %4;
            border-color: %5;
        }
        QToolBar QToolButton:checked {
            background-color: %4;
            color: %5;
            font-weight: bold;
        }
        QToolBar QToolButton::menu-indicator {
            image: none;
        }
    )").arg(p.textSecondary).arg(p.bgHover).arg(p.borderMedium)
       .arg(p.bgSelected).arg(p.accentPrimary);
}

// ── 侧边栏导航按钮（PNG 图标，checked 高亮）────────────────

QString ThemeManager::buildSidebarBtnQSS() const
{
    const auto p = palette();
    return QString(R"(
        QPushButton#sidebarNavBtn {
            background-color: transparent;
            border: 1px solid transparent;
            border-left: 3px solid transparent;
            border-radius: 6px;
            min-height: 30px;
            min-width: 30px;
            padding: 6px 0px;
            margin: 3px 5px;
        }
        QPushButton#sidebarNavBtn:hover {
            background-color: %1;
            border-color: %4;
        }
        QPushButton#sidebarNavBtn:checked {
            background-color: %2;
            border-color: %4;
            border-left: 3px solid %3;
        }
    )").arg(p.menuHoverBg).arg(p.menuSelectedBg).arg(p.accentPrimary).arg(p.borderMedium);
}

// ── 日志监控面板 ─────────────────────────────────────────────

QString ThemeManager::buildLogMonitorQSS() const
{
    const auto p = palette();
    return QString(R"(
        QFrame#logMonitor {
            border: 1px solid %1;
            border-radius: 6px;
            background-color: %2;
        }
        QFrame#logMonitorHeader {
            background-color: transparent;
            border-bottom: 2px solid %3;
        }
        QPushButton#logToggleBtn {
            background-color: transparent;
            border: 1px solid %1;
            border-radius: 4px;
            color: %4;
            font-size: 12px;
            padding: 0;
        }
        QPushButton#logToggleBtn:hover {
            background-color: %5;
            border-color: %3;
        }
        QTextEdit#logView {
            background-color: %6;
            color: %7;
            border: none;
            font-family: "Consolas", "Courier New", "Microsoft YaHei", monospace;
            font-size: 12px;
            padding: 4px 6px;
            border-radius: 4px;
        }
    )").arg(p.borderLight).arg(p.bgSecondary).arg(p.accentPrimary)
       .arg(p.textPrimary).arg(p.bgHover).arg(p.bgPrimary).arg(p.textSecondary);
}
