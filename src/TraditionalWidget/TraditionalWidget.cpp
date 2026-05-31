#include "TraditionalWidget.h"
#include <QVBoxLayout>
#include <QLabel>

TraditionalWidget::TraditionalWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();

    m_guard = new PermissionGuard(this);
    m_guard->watch(QStringList{"传统视觉.相机采集", "传统视觉.图像处理"});

    connect(m_guard, &PermissionGuard::changed, this, &TraditionalWidget::applyPermissions);
    applyPermissions();
}

void TraditionalWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel* title = new QLabel("传统视觉 - 经典算法引擎");
    title->setObjectName("widgetTitle");
    mainLayout->addWidget(title);

    QGroupBox* captureGroup = new QGroupBox("相机采集");
    m_captureGroup = captureGroup;
    QVBoxLayout* captureLayout = new QVBoxLayout(captureGroup);

    m_captureBtn = new QPushButton("启动采集");
    captureLayout->addWidget(m_captureBtn);

    mainLayout->addWidget(captureGroup);

    QGroupBox* procGroup = new QGroupBox("图像处理");
    m_processGroup = procGroup;
    QVBoxLayout* procLayout = new QVBoxLayout(procGroup);

    m_processBtn = new QPushButton("执行处理");
    procLayout->addWidget(m_processBtn);

    mainLayout->addWidget(procGroup);
    mainLayout->addStretch();
}

void TraditionalWidget::applyPermissions()
{
    bool canReadCap = m_guard->canRead("传统视觉.相机采集");
    bool canWriteCap = m_guard->canWrite("传统视觉.相机采集");
    bool canReadProc = m_guard->canRead("传统视觉.图像处理");
    bool canWriteProc = m_guard->canWrite("传统视觉.图像处理");

    if (m_captureGroup)
        m_captureGroup->setVisible(canReadCap);
    m_captureBtn->setEnabled(canWriteCap);
    m_captureBtn->setText(canWriteCap ? "启动采集" : "无采集权限");

    if (m_processGroup)
        m_processGroup->setVisible(canReadProc);
    m_processBtn->setEnabled(canWriteProc);
    m_processBtn->setText(canWriteProc ? "执行处理" : "无处理权限");
}
