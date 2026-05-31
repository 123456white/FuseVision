#ifndef FUSEVISION_H
#define FUSEVISION_H

#include <QMainWindow>
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include "DeepLearningWidget/DeepLearningWidget.h"
#include "TraditionalWidget/TraditionalWidget.h"
#include "MainWindows/UserManagementWidget.h"
#include "MainWindows/SystemSettingsWidget.h"
#include "core/Logger.h"
#include "core/ThemeManager.h"

class FuseVision : public QMainWindow
{
    Q_OBJECT

public:
    explicit FuseVision(QWidget* parent = nullptr);
    ~FuseVision();

    void setReadyStatus(bool ready);

private slots:
    void onSessionChanged();
    void onMenuItemSelected(int index);
    void onToggleTheme();

private:
    void initUI();
    void initLeftMenu();
    void initMainContent();
    void initStatusBar();
    void applyTheme();
    void logSystemInfo();
    void refreshStatusBar();

    QWidget*         m_centralWidget  = nullptr;
    QSplitter*       m_mainSplitter   = nullptr;
    QWidget*         m_menuWidget     = nullptr;
    QListWidget*     m_topMenuList    = nullptr;
    QListWidget*     m_bottomMenuList = nullptr;
    QStackedWidget*  m_stackedWidget  = nullptr;
    QPushButton*     m_themeBtn       = nullptr;

    QStatusBar* m_statusBar     = nullptr;
    QLabel*     m_userValueLabel = nullptr;
    QLabel*     m_versionLabel  = nullptr;
    QLabel*     m_readyLabel    = nullptr;

    DeepLearningWidget*    m_deepLearningWidget    = nullptr;
    TraditionalWidget*     m_traditionalWidget     = nullptr;
    UserManagementWidget*  m_userManagementWidget  = nullptr;
    SystemSettingsWidget*  m_systemSettingsWidget  = nullptr;
};

#endif // FUSEVISION_H
