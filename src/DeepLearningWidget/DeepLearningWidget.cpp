#include "DeepLearningWidget.h"
#include "ProjectManagement.h"
#include "ModelManagement.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QFrame>

// =============================================================================
// DeepLearningWidget.cpp — 深度学习模块 8 标签页实现
// =============================================================================
// 布局：QTabBar(8 标签) + QFrame 内容容器 + QStackedWidget(8 页)
// Tab 0 = ProjectManagement（已实现），Tab 1~7 = 占位页面
// =============================================================================

// 8 个标签页的名称（与 s_permissionKeys 一一对应）
const QStringList DeepLearningWidget::s_tabNames = {
    "项目管理", "模型管理", "数据集管理", "数据标注",
    "数据拆分", "模型训练", "模型预测", "模型导出"
};

// 8 个权限键（格式：深度学习.标签名）
const QStringList DeepLearningWidget::s_permissionKeys = {
    "深度学习.项目管理", "深度学习.模型管理", "深度学习.数据集管理",
    "深度学习.数据标注", "深度学习.数据拆分", "深度学习.模型训练",
    "深度学习.模型预测", "深度学习.模型导出"
};

// ── 构造 ──────────────────────────────────────────────────────

DeepLearningWidget::DeepLearningWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();  // 步骤 1：构建 8 标签页 UI

    // 步骤 2：注册 8 个权限点
    m_guard = new PermissionGuard(this);
    m_guard->watch(s_permissionKeys);

    // 步骤 3：权限变更 → 刷新 Tab 可见性
    connect(m_guard, &PermissionGuard::changed,
            this, &DeepLearningWidget::applyPermissions);

    // 步骤 4：初始渲染
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
    m_tabBar->setExpanding(true);         // 标签均匀拉伸，文字不截断
    m_tabBar->setDrawBase(true);          // 绘制底部基线
    m_tabBar->setUsesScrollButtons(false);
    m_tabBar->setContentsMargins(0, 0, 0, 0);

    for (const QString& name : s_tabNames)
        m_tabBar->addTab(name);

    connect(m_tabBar, &QTabBar::currentChanged,
            this, &DeepLearningWidget::onTabChanged);

    mainLayout->addWidget(m_tabBar);

    // ===== 内容区容器（划定边界，与标签栏视觉分离） =====
    QFrame* contentFrame = new QFrame;
    contentFrame->setObjectName("dlContentFrame");
    QVBoxLayout* frameLayout = new QVBoxLayout(contentFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);

    // ===== 内容区（Tab 0 = ProjectManagement，Tab 1~7 = 占位页）=====
    m_stackedWidget = new QStackedWidget;

    // Page 0 — 项目管理（已实现）
    m_projectManagement = new ProjectManagement;
    m_stackedWidget->addWidget(m_projectManagement);

    // Page 1 — 模型管理
    m_modelManagement = new ModelManagement;
    m_stackedWidget->addWidget(m_modelManagement);

    // Page 2~7 — 占位页面
    for (int i = 2; i < s_tabNames.size(); ++i) {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        pageLayout->setAlignment(Qt::AlignCenter);

        QLabel* placeholder = new QLabel(QString::fromUtf8("%1 — 内容待开发").arg(s_tabNames[i]));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("placeholderLabel");
        pageLayout->addWidget(placeholder);

        m_stackedWidget->addWidget(page);
    }

    m_stackedWidget->setCurrentIndex(0);
    frameLayout->addWidget(m_stackedWidget);

    // ===== 垂直分割器：内容区 + 日志面板（可拖拽调整高低）=====
    QSplitter* vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->setHandleWidth(1);
    vSplitter->setChildrenCollapsible(false);
    vSplitter->addWidget(contentFrame);

    // m_logMonitor 必须在 connect 调用之前创建
    m_logMonitor = new LogMonitor;
    vSplitter->addWidget(m_logMonitor);

    mainLayout->addWidget(vSplitter, 1);

    // ── 信号连接（m_logMonitor 已就绪）─────────────────────
    // 项目打开/关闭/预览 → 日志 + 信号转发
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, &DeepLearningWidget::onProjectOpened);
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, &DeepLearningWidget::onProjectClosed);
    connect(m_projectManagement, &ProjectManagement::projectPreviewed,
            this, [this](const QString& name) { emit dlProjectChanged(name); });

    // ProjectManagement → ModelManagement 项目绑定
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, [this](const QString&, const QString&) {
                m_modelManagement->setCurrentProject(m_projectManagement->currentProject());
            });
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, [this]() { m_modelManagement->setCurrentProject(FvprojInfo()); });

    // ModelManagement 信号 → 日志
    connect(m_modelManagement, &ModelManagement::modelCreated,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(QString::fromUtf8("模型已创建: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::modelOpened,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(QString::fromUtf8("模型已打开: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(QString::fromUtf8("当前模型切换至: %1").arg(name));
            });

    // ModelManagement 双击卡片 → 跳数据集管理
    connect(m_modelManagement, &ModelManagement::openDatasetRequested,
            this, [this](const QString& modelId) {
                m_logMonitor->log(QString::fromUtf8("跳转数据集管理: %1").arg(modelId));
                if (m_tabBar->isTabVisible(2)) {
                    m_tabBar->setCurrentIndex(2);
                    m_stackedWidget->setCurrentIndex(2);
                }
            });

    m_logMonitor->log("深度学习模块已加载");
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

// ── Tab 切换 ──────────────────────────────────────────────────

void DeepLearningWidget::onTabChanged(int index)
{
    if (index >= 0 && index < m_stackedWidget->count())
        m_stackedWidget->setCurrentIndex(index);
}

// ── 权限响应 ─────────────────────────────────────────────────

void DeepLearningWidget::applyPermissions()
{
    for (int i = 0; i < s_permissionKeys.size(); ++i) {
        bool canRead = m_guard->canRead(s_permissionKeys[i]);
        // canRead 控制 Tab 是否可见
        m_tabBar->setTabVisible(i, canRead);
        // 如果当前选中的 Tab 被隐藏，切换到第一个可见的
        if (!canRead && m_tabBar->currentIndex() == i) {
            for (int j = 0; j < m_tabBar->count(); ++j) {
                if (m_tabBar->isTabVisible(j)) {
                    m_tabBar->setCurrentIndex(j);
                    break;
                }
            }
        }
    }
}

// ── 项目信号中转 ─────────────────────────────────────────────

QString DeepLearningWidget::currentProjectName() const
{
    return m_projectManagement ? m_projectManagement->currentProjectName() : QString();
}

void DeepLearningWidget::onProjectOpened(const QString& name, const QString& path)
{
    m_logMonitor->log(QString::fromUtf8("项目已打开: %1 (%2)").arg(name, path));
    emit dlProjectChanged(name);

    // 自动跳转到"模型管理"标签页
    if (m_tabBar->isTabVisible(1)) {
        m_tabBar->setCurrentIndex(1);
        m_stackedWidget->setCurrentIndex(1);
    }
}

void DeepLearningWidget::onProjectClosed()
{
    m_logMonitor->log(QString::fromUtf8("项目已关闭"));
    emit dlProjectChanged(QString());
}
