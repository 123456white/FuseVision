#include "DeepLearningWidget.h"
#include "ui/WriteAccess.h"
#include "ProjectManagement.h"
#include "ModelManagement.h"
#include "DatasetManagement.h"
#include "AnnotationManagement.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QCoreApplication>
#include <QTimer>

// =============================================================================
// DeepLearningWidget.cpp — 深度学习模块 9 标签页
// =============================================================================

// 9 个标签页的名称
const QStringList DeepLearningWidget::s_tabNames = {
    QString::fromUtf8("项目管理"), QString::fromUtf8("模型管理"),
    QString::fromUtf8("数据集管理"), QString::fromUtf8("数据标注"),
    QString::fromUtf8("数据检查"), QString::fromUtf8("数据拆分"),
    QString::fromUtf8("模型训练"), QString::fromUtf8("模型预测"),
    QString::fromUtf8("模型导出")
};

// 9 个权限键
const QStringList DeepLearningWidget::s_permissionKeys = {
    QString::fromUtf8("深度学习.项目管理"), QString::fromUtf8("深度学习.模型管理"),
    QString::fromUtf8("深度学习.数据集管理"), QString::fromUtf8("深度学习.数据标注"),
    QString::fromUtf8("深度学习.数据检查"), QString::fromUtf8("深度学习.数据拆分"),
    QString::fromUtf8("深度学习.模型训练"), QString::fromUtf8("深度学习.模型预测"),
    QString::fromUtf8("深度学习.模型导出")
};

// ── 析构 ──────────────────────────────────────────────────────

DeepLearningWidget::~DeepLearningWidget() = default;

// ── 构造 ──────────────────────────────────────────────────────

DeepLearningWidget::DeepLearningWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();

    m_guard = new PermissionGuard(this);
    m_guard->watch(s_permissionKeys);
    connect(m_guard, &PermissionGuard::changed,
            this, &DeepLearningWidget::applyPermissions);
    applyPermissions();
}

// ── UI 构建 ───────────────────────────────────────────────────

void DeepLearningWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 8, 24, 20);
    mainLayout->setSpacing(12);

    // ===== 顶部标签栏 =====
    m_tabBar = new QTabBar;
    m_tabBar->setShape(QTabBar::RoundedNorth);
    m_tabBar->setExpanding(true);
    m_tabBar->setDrawBase(true);
    m_tabBar->setUsesScrollButtons(false);
    m_tabBar->setContentsMargins(0, 0, 0, 0);

    for (const QString& name : s_tabNames)
        m_tabBar->addTab(name);

    connect(m_tabBar, &QTabBar::currentChanged,
            this, &DeepLearningWidget::onTabChanged);

    // ===== 内容区容器 =====
    QFrame* contentFrame = new QFrame;
    contentFrame->setObjectName("dlContentFrame");
    QVBoxLayout* frameLayout = new QVBoxLayout(contentFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget;

    // Page 0 — 项目管理
    m_projectManagement = new ProjectManagement;
    m_stackedWidget->addWidget(m_projectManagement);

    // Page 1 — 模型管理
    m_modelManagement = new ModelManagement;
    m_stackedWidget->addWidget(m_modelManagement);

    // Page 2 — 数据集管理
    m_datasetManagement = new DatasetManagement;
    m_stackedWidget->addWidget(m_datasetManagement);

    // Page 3 — 数据标注
    m_annotationManagement = new AnnotationManagement;
    m_stackedWidget->addWidget(m_annotationManagement);

    // Page 4 — 数据检查（占位，待开发）
    {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        pageLayout->setAlignment(Qt::AlignCenter);
        QLabel* placeholder = new QLabel(
            QString::fromUtf8("数据检查 — 内容待开发"));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("placeholderLabel");
        pageLayout->addWidget(placeholder);
        m_stackedWidget->addWidget(page);
    }

    // Page 5~8 — 占位
    for (int i = 5; i < s_tabNames.size(); ++i) {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        pageLayout->setAlignment(Qt::AlignCenter);
        QLabel* placeholder = new QLabel(
            QString::fromUtf8("%1 — 内容待开发").arg(s_tabNames[i]));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("placeholderLabel");
        pageLayout->addWidget(placeholder);
        m_stackedWidget->addWidget(page);
    }
    m_stackedWidget->setCurrentIndex(0);
    frameLayout->addWidget(m_stackedWidget);

    // ===== 正常模式页面 = TabBar + ContentFrame =====
    QWidget* normalPage = new QWidget;
    QVBoxLayout* normalLayout = new QVBoxLayout(normalPage);
    normalLayout->setContentsMargins(0, 0, 0, 0);
    normalLayout->setSpacing(0);
    normalLayout->addWidget(m_tabBar);
    normalLayout->addWidget(contentFrame, 1);

    // ===== 垂直分割器：内容 + 日志 =====
    QSplitter* vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->setHandleWidth(2);
    vSplitter->setChildrenCollapsible(false);
    vSplitter->addWidget(normalPage);

    m_logMonitor = new LogMonitor;
    m_logMonitor->setMaximumHeight(300);
    vSplitter->addWidget(m_logMonitor);

    vSplitter->setStretchFactor(0, 4);
    vSplitter->setStretchFactor(1, 1);
    vSplitter->setSizes({600, 150});

    mainLayout->addWidget(vSplitter, 1);

    m_datasetManagement->hasPendingAnnotations=[this]() { return m_annotationManagement->hasUnsavedChanges(); };
    connect(m_datasetManagement,&DatasetManagement::datasetFilesChanged,m_annotationManagement,&AnnotationManagement::clearContext);
    m_projectManagement->beforeContextChange = [this]() { return prepareToLeave(); };
    m_modelManagement->beforeContextChange = [this]() { return prepareToLeave(); };
    // ── 信号连接 ─────────────────────────────────────
    // 项目 → 模型管理
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, &DeepLearningWidget::onProjectOpened);
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, &DeepLearningWidget::onProjectClosed);

    // 项目 → ModelManagement 绑定
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, [this](const QString&, const QString&) {
                m_modelManagement->setCurrentProject(
                    m_projectManagement->currentProject());
            });
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, [this]() {
                m_modelManagement->setCurrentProject(FvprojInfo());
            });

    // ModelManagement 日志
    connect(m_modelManagement, &ModelManagement::modelCreated,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(
                    QString::fromUtf8("模型已创建: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::modelOpened,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(
                    QString::fromUtf8("模型已打开: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(
                    QString::fromUtf8("当前模型切换至: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            this, [this](const QString&) {
                emit dlModelChanged(m_modelManagement->currentModelName());
            });

    // 双击模型 → 跳数据集
    connect(m_modelManagement, &ModelManagement::openDatasetRequested,
            this, [this](const QString& modelId) {
                m_logMonitor->log(
                    QString::fromUtf8("跳转数据集管理: %1").arg(modelId));
                if (m_tabBar->isTabVisible(2)) {
                    m_tabBar->setCurrentIndex(2);
                    m_stackedWidget->setCurrentIndex(2);
                }
            });

    // ── DatasetManagement ────────────────────────────
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, [this](const QString&, const QString&) {
                m_datasetManagement->setCurrentProject(
                    m_projectManagement->currentProject());
            });
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, [this]() {
                m_datasetManagement->setCurrentProject(FvprojInfo());
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            this, [this](const QString& modelId) {
                m_annotationManagement->clearContext();
                m_datasetManagement->setCurrentModel(
                    modelId, m_modelManagement->currentModelInfo());
            });
    connect(m_datasetManagement, &DatasetManagement::dataImported,
            m_logMonitor, [this](const QString& modelId) {
                m_logMonitor->log(
                    QString::fromUtf8("数据集已导入: %1").arg(modelId));
            });

    // 双击图像 → 跳数据标注
    connect(m_datasetManagement, &DatasetManagement::openAnnotationRequested,
            this, [this](const QString& modelId, const QString& imagePath) {
                m_logMonitor->log(
                    QString::fromUtf8("跳转数据标注: %1 → %2")
                        .arg(modelId, imagePath));
                m_annotationManagement->setDatasetSource(m_datasetManagement);
                if (!m_annotationManagement->openImage(modelId, imagePath)) return;
                if (m_tabBar->isTabVisible(3)) {
                    m_tabBar->setCurrentIndex(3);
                    m_stackedWidget->setCurrentIndex(3);
                }
            });

    // 标注返回 → 切回数据集（同步标签类别变动）
    connect(m_annotationManagement, &AnnotationManagement::returnToGallery,
            this, [this]() {
                // 刷新当前数据集标注统计
                m_datasetManagement->detectLabelsFromAnnotations();
                if (m_tabBar->isTabVisible(2)) {
                    m_tabBar->setCurrentIndex(2);
                    m_stackedWidget->setCurrentIndex(2);
                }
            });

    // 数据集标签变动 → 同步到标注模块
    connect(m_datasetManagement, &DatasetManagement::labelsChanged,
            this, [this]() {
                m_annotationManagement->syncLabels();
            });

    m_logMonitor->log(QString::fromUtf8("深度学习模块已加载"));
}

// ── 首次显示时按比例分配日志面板高度 ─────────────────────────

void DeepLearningWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_shown) return;
    m_shown = true;
    if (auto* s = findChild<QSplitter*>()) {
        int h = s->height();
        if (h > 0 && s->count() == 2)
            s->setSizes({ h * 3 / 4, h / 4 });
    }
}

void DeepLearningWidget::onTabChanged(int index)
{
    if(index<0 || index>=s_permissionKeys.size() || !AccessControl::canRead(s_permissionKeys[index])) return;
    const int previous=m_stackedWidget->currentIndex();
    if(previous==3 && index!=3 && !prepareToLeave()) {
        QSignalBlocker blocker(m_tabBar); m_tabBar->setCurrentIndex(previous); return;
    }
    if (index >= 0 && index < m_stackedWidget->count())
        m_stackedWidget->setCurrentIndex(index);
}

bool DeepLearningWidget::prepareToLeave()
{
    return !m_annotationManagement || m_annotationManagement->prepareToLeave();
}

void DeepLearningWidget::applyPermissions()
{
    int first=-1;
    for (int i = 0; i < s_permissionKeys.size(); ++i) {
        bool canRead = m_guard->canRead(s_permissionKeys[i]);
        if(canRead && first<0) first=i;
        m_tabBar->setTabVisible(i, canRead);
        if (!canRead && m_tabBar->currentIndex() == i) {
            for (int j = 0; j < m_tabBar->count(); ++j) {
                if (m_tabBar->isTabVisible(j)) {
                    m_tabBar->setCurrentIndex(j);
                    break;
                }
            }
        }
    }
    m_stackedWidget->setVisible(first>=0);
    if(first>=0 && !m_guard->canRead(s_permissionKeys[m_stackedWidget->currentIndex()])) {
        QSignalBlocker blocker(m_tabBar);
        m_tabBar->setCurrentIndex(first); m_stackedWidget->setCurrentIndex(first);
    }
}

QString DeepLearningWidget::currentProjectName() const
{
    return m_projectManagement
        ? m_projectManagement->currentProjectName() : QString();
}

void DeepLearningWidget::onProjectOpened(const QString& name,
                                          const QString& path)
{
    m_annotationManagement->clearContext();
    m_datasetManagement->setCurrentModel({},{});
    emit dlModelChanged({});
    m_logMonitor->log(
        QString::fromUtf8("项目已打开: %1 (%2)").arg(name, path));
    emit dlProjectChanged(name);
    if (m_tabBar->isTabVisible(1)) {
        m_tabBar->setCurrentIndex(1);
        m_stackedWidget->setCurrentIndex(1);
    }
}

void DeepLearningWidget::onProjectClosed()
{
    m_annotationManagement->clearContext();
    m_datasetManagement->setCurrentModel({},{});
    emit dlModelChanged({});
    m_logMonitor->log(QString::fromUtf8("项目已关闭"));
    emit dlProjectChanged(QString());
}
