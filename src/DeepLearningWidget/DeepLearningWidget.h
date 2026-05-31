#ifndef DEEPLEARNINGWIDGET_H
#define DEEPLEARNINGWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include "core/PermissionGuard.h"

class DeepLearningWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeepLearningWidget(QWidget* parent = nullptr);

private slots:
    void applyPermissions();

private:
    void initUI();

    PermissionGuard* m_guard          = nullptr;
    QGroupBox*       m_trainGroup     = nullptr;
    QGroupBox*       m_inferGroup     = nullptr;
    QLabel*          m_modelStatus    = nullptr;
    QPushButton*     m_trainBtn       = nullptr;
    QPushButton*     m_exportBtn      = nullptr;
    QLabel*          m_inferStatus    = nullptr;
    QPushButton*     m_inferBtn       = nullptr;
};

#endif // DEEPLEARNINGWIDGET_H
