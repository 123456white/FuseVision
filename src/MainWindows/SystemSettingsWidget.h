#ifndef SYSTEMSETTINGSWIDGET_H
#define SYSTEMSETTINGSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

class SystemSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SystemSettingsWidget(QWidget* parent = nullptr);

private slots:
    void onBrowseLogPath();
    void onBrowseDbPath();
    void onSave();
    void onReset();

private:
    void initUI();
    void loadSettings();
    void refreshRestartNotice();

    QLineEdit*   m_logPathEdit   = nullptr;
    QLineEdit*   m_dbPathEdit    = nullptr;
    QLabel*      m_restartLabel  = nullptr;

    QPushButton* m_saveBtn  = nullptr;
    QPushButton* m_resetBtn = nullptr;
};

#endif // SYSTEMSETTINGSWIDGET_H
