#include "ThemeManager.h"
#include <QSettings>
#include <QOperatingSystemVersion>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ThemePalette lightPalette()
{
    return {
        "#f0f2f5", "#ffffff", "#f8f9fa", "#f5f5f5",
        "#edf2fa", "#ecf0f1",
        "#2c3e50", "#6b7280", "#8b95a5", "#5b8def",
        "#dfe3e8", "#c8cdd3", "#5b8def",
        "#5b8def", "#4a7de0", "#3d6bc7",
        "#34a853", "#ed8936", "#e53e3e",
        "#ffffff", "#ffffff",
        "#ffffff", "#edf2fa", "#f5f5f5", "none",
        "#f8f9fa", "#dfe3e8",
        "#dfe3e8",
        "#f0f0f0", "#c8cdd3", "#a8b0b8"
    };
}

ThemePalette darkPalette()
{
    return {
        "#1a1a2e", "#1e2540", "#243050", "#2a3858",
        "#223a5a", "#2a3050",
        "#e2e8f0", "#c0c8d4", "#96a0b0", "#6b9ff2",
        "#3a4860", "#4a5870", "#4a80e0",
        "#4a80e0", "#3b6ec8", "#305ab0",
        "#34a853", "#ed8936", "#e53e3e",
        "#1e2540", "#243050",
        "#1e2540", "#223a5a", "#2a3858", "none",
        "#243050", "#3a4860",
        "#3a4860",
        "#243050", "#4a5870", "#5a6880"
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
         + buildDialogQSS();
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
        QLabel#widgetTitle {
            font-size: 18px;
            font-weight: bold;
            color: %1;
        }
        QLabel#infoLabel {
            color: %2;
            font-size: 12px;
            padding: 4px 0;
        }
        QLabel#restartLabel {
            color: %3;
            font-weight: bold;
            padding: 8px;
            background-color: %4;
            border-radius: 4px;
        }
        QLabel#placeholderLabel {
            color: %5;
            font-size: 13px;
        }
        QLabel#hintLabel {
            color: %5;
            font-size: 12px;
        }
    )").arg(p.textPrimary).arg(p.textMuted).arg(p.warning).arg(p.bgSelected).arg(p.textMuted);
}

QString ThemeManager::buildButtonQSS() const
{
    const auto p = palette();
    return QString(R"(
        QPushButton {
            padding: 8px 20px;
            border-radius: 4px;
            border: 1px solid %1;
            background-color: %2;
            color: %3;
            font-size: 13px;
        }
        QPushButton:hover  { background-color: %4; }
        QPushButton:pressed{ background-color: %5; }
        QPushButton:disabled { color: %6; background-color: %7; }
        QPushButton#primaryBtn {
            background-color: %8; color: white; border: none; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background-color: %9; }
        QPushButton#primaryBtn:pressed{ background-color: %10; }
        QPushButton#loginBtn {
            background-color: %8; color: white; border: none; font-weight: bold; padding: 6px 20px;
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
            gridline-color: %2;
            background-color: %3;
            alternate-background-color: %4;
        }
        QTableWidget::item { padding: 4px 8px; }
        QTableWidget::item:selected { background-color: %5; color: %6; }
        QHeaderView::section {
            background-color: %7;
            border: none;
            border-bottom: 2px solid %1;
            padding: 6px 8px;
            font-weight: bold;
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
            padding: 6px 8px;
            border: 1px solid %1;
            border-radius: 4px;
            background-color: %2;
            color: %3;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: %4;
        }
        QComboBox::drop-down {
            border: none;
            padding-right: 6px;
        }
        QComboBox QAbstractItemView {
            background-color: %2;
            border: 1px solid %1;
            selection-background-color: %5;
            color: %3;
        }
    )").arg(p.borderMedium).arg(p.surfaceWhite).arg(p.textPrimary)
       .arg(p.accentPrimary).arg(p.bgSelected);
}

QString ThemeManager::buildMenuQSS() const
{
    const auto p = palette();
    return QString(R"(
        QWidget#menuContainer { background-color: %1; border-right: none; }
        QListWidget {
            background-color: %1;
            border: none;
            outline: none;
            font-size: 12px;
        }
        QListWidget::item {
            height: 44px;
            padding: 0px;
            margin: 0px;
            text-align: center;
        }
        QListWidget::item:selected {
            background-color: %2;
            border-left: 3px solid %3;
        }
        QListWidget::item:hover { background-color: %4; }
    )").arg(p.menuBg).arg(p.menuSelectedBg).arg(p.accentPrimary).arg(p.menuHoverBg);
}

QString ThemeManager::buildStatusBarQSS() const
{
    const auto p = palette();
    return QString(R"(
        QStatusBar { background-color: %1; border-top: 1px solid %2; min-height: 26px; font-size: 12px; }
        QStatusBar QLabel { padding: 0 6px; color: %3; }
        QLabel#readyLabel { color: %4; font-weight: bold; }
        QLabel#busyLabel  { color: #e67e22; font-weight: bold; }
        QLabel#versionLabel { color: %5; }
    )").arg(p.statusBarBg).arg(p.statusBarBorder).arg(p.textPrimary)
       .arg(p.success).arg(p.accentPrimary);
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
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }
    )").arg(p.borderLight);
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
        QScrollBar:vertical {
            background-color: %3;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background-color: %4;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background-color: %5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QScrollBar:horizontal {
            background-color: %3;
            height: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background-color: %4;
            border-radius: 4px;
            min-width: 24px;
        }
        QScrollBar::handle:horizontal:hover { background-color: %5; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
    )").arg(p.bgSecondary).arg(p.borderLight).arg(p.scrollbarBg)
       .arg(p.scrollbarHandle).arg(p.scrollbarHandleHover);
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
