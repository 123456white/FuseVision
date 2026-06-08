#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>

struct ThemePalette;

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme { Light, Dark };

    static ThemeManager& instance();

    Theme currentTheme() const;
    void   setTheme(Theme theme);

    ThemePalette palette() const;
    QString      styleSheet() const;

    Theme detectSystemTheme() const;

signals:
    void themeChanged(Theme theme);

private:
    ThemeManager();
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void save();
    void load();

    QString buildGlobalQSS()             const;
    QString buildWidgetQSS()             const;
    QString buildButtonQSS()             const;
    QString buildTableQSS()              const;
    QString buildFormQSS()               const;
    QString buildMenuQSS()               const;
    QString buildStatusBarQSS()          const;
    QString buildSplitterQSS()           const;
    QString buildGroupBoxQSS()           const;
    QString buildToolBarQSS()            const;
    QString buildDialogQSS()             const;
    QString buildTabBarQSS()             const;
    QString buildTreeQSS()               const;
    QString buildToolButtonQSS()         const;
    QString buildScrollBarQSS()          const;
    QString buildSidebarBtnQSS()         const;
    QString buildLogMonitorQSS()         const;

    Theme m_theme = Light;
};

struct ThemePalette {
    QString bgPrimary;
    QString bgSecondary;
    QString bgTertiary;
    QString bgHover;
    QString bgSelected;
    QString bgDisabled;

    QString textPrimary;
    QString textSecondary;
    QString textMuted;
    QString textAccent;

    QString borderLight;
    QString borderMedium;
    QString borderFocus;

    QString accentPrimary;
    QString accentHover;
    QString accentPressed;

    QString success;
    QString warning;
    QString danger;

    QString surfaceWhite;
    QString surfaceCard;

    QString menuBg;
    QString menuSelectedBg;
    QString menuHoverBg;
    QString menuBorder;

    QString statusBarBg;
    QString statusBarBorder;

    QString splitterHandle;

    QString scrollbarBg;
    QString scrollbarHandle;
    QString scrollbarHandleHover;
};

ThemePalette lightPalette();
ThemePalette darkPalette();

#endif // THEMEMANAGER_H
