#include "DeepLearningWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

DeepLearningWidget::DeepLearningWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();

    m_guard = new PermissionGuard(this);
    m_guard->watch(QStringList{"深度学习.模型训练", "深度学习.推理"});

    connect(m_guard, &PermissionGuard::changed, this, &DeepLearningWidget::applyPermissions);
    applyPermissions();
}

void DeepLearningWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel* title = new QLabel("深度学习 - 视觉 AI 引擎");
    title->setObjectName("widgetTitle");
    mainLayout->addWidget(title);

    QGroupBox* trainGroup = new QGroupBox("模型训练");
    m_trainGroup = trainGroup;
    QVBoxLayout* trainLayout = new QVBoxLayout(trainGroup);
    trainLayout->setSpacing(8);

    m_modelStatus = new QLabel("就绪");
    trainLayout->addWidget(m_modelStatus);

    QHBoxLayout* trainBtnLayout = new QHBoxLayout;
    m_trainBtn  = new QPushButton("开始训练");
    m_exportBtn = new QPushButton("导出模型");
    trainBtnLayout->addWidget(m_trainBtn);
    trainBtnLayout->addWidget(m_exportBtn);
    trainBtnLayout->addStretch();
    trainLayout->addLayout(trainBtnLayout);

    mainLayout->addWidget(trainGroup);

    QGroupBox* inferGroup = new QGroupBox("模型推理");
    m_inferGroup = inferGroup;
    QVBoxLayout* inferLayout = new QVBoxLayout(inferGroup);
    inferLayout->setSpacing(8);

    m_inferStatus = new QLabel("就绪");
    inferLayout->addWidget(m_inferStatus);

    m_inferBtn = new QPushButton("执行推理");
    inferLayout->addWidget(m_inferBtn);

    mainLayout->addWidget(inferGroup);
    mainLayout->addStretch();
}

void DeepLearningWidget::applyPermissions()
{
    bool canReadTrain  = m_guard->canRead("深度学习.模型训练");
    bool canWriteTrain  = m_guard->canWrite("深度学习.模型训练");
    bool canReadInfer  = m_guard->canRead("深度学习.推理");
    bool canWriteInfer  = m_guard->canWrite("深度学习.推理");

    if (m_trainGroup)
        m_trainGroup->setVisible(canReadTrain);
    m_trainBtn->setEnabled(canWriteTrain);
    m_exportBtn->setEnabled(canWriteTrain);
    m_modelStatus->setText(canWriteTrain ? "就绪" : "无训练权限");

    if (m_inferGroup)
        m_inferGroup->setVisible(canReadInfer);
    m_inferBtn->setEnabled(canWriteInfer);
    m_inferStatus->setText(canWriteInfer ? "就绪" : "无推理权限");
}
