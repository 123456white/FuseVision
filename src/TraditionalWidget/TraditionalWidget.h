#ifndef TRADITIONALWIDGET_H
#define TRADITIONALWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QGroupBox>
#include "core/PermissionGuard.h"

class TraditionalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TraditionalWidget(QWidget* parent = nullptr);

private slots:
    void applyPermissions();

private:
    void initUI();

    PermissionGuard* m_guard       = nullptr;
    QGroupBox*       m_captureGroup = nullptr;
    QGroupBox*       m_processGroup = nullptr;
    QPushButton*     m_captureBtn  = nullptr;
    QPushButton*     m_processBtn  = nullptr;
};

#endif // TRADITIONALWIDGET_H
