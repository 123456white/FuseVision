#include "DeepLearningWidget.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QFrame>

// =============================================================================
// DeepLearningWidget.cpp — 深度学习模块 8 标签页实现
// =============================================================================
// 布局：QTabBar(8 标签) + QFrame 内容容器 + QStackedWidget(8 空白页)
// 每个标签页当前仅显示"内容待开发"占位文本
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

    // ===== 内容区（8 个空白页） =====
    m_stackedWidget = new QStackedWidget;

    for (int i = 0; i < s_tabNames.size(); ++i) {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        pageLayout->setAlignment(Qt::AlignCenter);

        QLabel* placeholder = new QLabel("内容待开发");
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

    m_logMonitor = new LogMonitor;
    m_logMonitor->log("深度学习模块已加载");
    vSplitter->addWidget(m_logMonitor);

    mainLayout->addWidget(vSplitter, 1);
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
