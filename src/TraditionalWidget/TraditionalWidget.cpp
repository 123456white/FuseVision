#include "TraditionalWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QAction>
#include <QFrame>
#include <QDebug>

// =============================================================================
// TraditionalWidget.cpp — 传统视觉模块三区布局实现
// =============================================================================

// 7 个权限键（工具栏按钮 + 资源区操作）
const QStringList TraditionalWidget::s_permissionKeys = {
    "传统视觉.流程编辑", "传统视觉.相机采集", "传统视觉.相机标定",
    "传统视觉.相机建模", "传统视觉.通讯设置", "传统视觉.系统设置",
    "传统视觉.视觉工具区"
};

// ── 构造 ──────────────────────────────────────────────────────

TraditionalWidget::TraditionalWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();  // 步骤 1：构建三区布局

    // 步骤 2：注册 7 个权限点
    m_guard = new PermissionGuard(this);
    m_guard->watch(s_permissionKeys);

    // 步骤 3：权限变更 → 刷新 UI
    connect(m_guard, &PermissionGuard::changed,
            this, &TraditionalWidget::applyPermissions);

    // 步骤 4：初始渲染
    applyPermissions();
}

// ── UI 构建 ───────────────────────────────────────────────────

void TraditionalWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 8, 24, 20);
    mainLayout->setSpacing(12);

    // 顶部工具栏
    initToolBar();
    mainLayout->addWidget(m_toolBar);

    // 三区水平分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->setHandleWidth(1);

    initResourcePanel();   // 左：资源管理区
    initEditorArea();      // 中：流程编辑器
    initToolPanel();       // 右：视觉工具区

    m_mainSplitter->addWidget(m_resourceFrame);
    m_mainSplitter->addWidget(m_editorFrame);
    m_mainSplitter->addWidget(m_toolFrame);

    // 默认比例：资源区 / 弹性编辑区 / 工具区
    m_mainSplitter->setSizes({ 120, 500, 110 });

    // ===== 垂直分割器：三区 + 日志面板（可拖拽调整高低）=====
    QSplitter* vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->setHandleWidth(1);
    vSplitter->setChildrenCollapsible(false);
    vSplitter->addWidget(m_mainSplitter);

    m_logMonitor = new LogMonitor;
    m_logMonitor->log("传统视觉模块已加载");
    vSplitter->addWidget(m_logMonitor);

    mainLayout->addWidget(vSplitter, 1);
}

// ── 首次显示时按比例分配日志面板高度 ─────────────────────────

void TraditionalWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_shown) return;
    m_shown = true;
    if (auto* s = findChild<QSplitter*>("", Qt::FindDirectChildrenOnly)) {
        int h = s->height();
        if (h > 0 && s->count() == 2)
            s->setSizes({ h * 3 / 4, h / 4 });
    }
}

// ── 顶部工具栏 ───────────────────────────────────────────────

void TraditionalWidget::initToolBar()
{
    m_toolBar = new QToolBar;
    m_toolBar->setMovable(false);           // 不可拖拽
    m_toolBar->setIconSize(QSize(20, 20));
    m_toolBar->setContentsMargins(0, 0, 0, 0);

    QAction* actFlow  = m_toolBar->addAction("流程文件");
    QAction* actCam   = m_toolBar->addAction("相机配置");
    QAction* actCalib = m_toolBar->addAction("相机标定");
    QAction* actModel = m_toolBar->addAction("相机建模");
    m_toolBar->addSeparator();
    QAction* actComm  = m_toolBar->addAction("通讯设置");
    QAction* actSys   = m_toolBar->addAction("系统设置");

    // 存储引用便于权限控制时禁用
    m_toolBar->setProperty("actFlow",  QVariant::fromValue<QAction*>(actFlow));
    m_toolBar->setProperty("actCam",   QVariant::fromValue<QAction*>(actCam));
    m_toolBar->setProperty("actCalib", QVariant::fromValue<QAction*>(actCalib));
    m_toolBar->setProperty("actModel", QVariant::fromValue<QAction*>(actModel));
    m_toolBar->setProperty("actComm",  QVariant::fromValue<QAction*>(actComm));
    m_toolBar->setProperty("actSys",   QVariant::fromValue<QAction*>(actSys));
}

// ── 左侧资源管理区 ─────────────────────────────────────────────

void TraditionalWidget::initResourcePanel()
{
    m_resourceFrame = new QFrame;
    m_resourceFrame->setObjectName("sectionFrame");

    QVBoxLayout* resLayout = new QVBoxLayout(m_resourceFrame);
    resLayout->setContentsMargins(8, 0, 8, 8);
    resLayout->setSpacing(0);

    // ── 资源区标题 ──
    QLabel* resTitle = new QLabel("资源管理");
    resTitle->setObjectName("sectionTitle");
    resLayout->addWidget(resTitle);

    // ── 资源树 ──
    m_resourceTree = new QTreeWidget;
    m_resourceTree->setHeaderHidden(true);
    m_resourceTree->setMinimumWidth(100);

    // 占位节点（后续由流程文件加载/相机枚举填充）
    QTreeWidgetItem* flows = new QTreeWidgetItem({"流程文件"});
    flows->addChild(new QTreeWidgetItem({"示例流程"}));
    m_resourceTree->addTopLevelItem(flows);

    QTreeWidgetItem* cameras = new QTreeWidgetItem({"相机设备"});
    cameras->addChild(new QTreeWidgetItem({"未连接"}));
    m_resourceTree->addTopLevelItem(cameras);

    resLayout->addWidget(m_resourceTree, 1);
}

// ── 中间编辑区（双模式） ─────────────────────────────────────

void TraditionalWidget::initEditorArea()
{
    // 外框容器（划定区域边界）
    m_editorFrame = new QFrame;
    m_editorFrame->setObjectName("sectionFrame");
    m_editorFrame->setFrameStyle(QFrame::StyledPanel);

    QVBoxLayout* frameLayout = new QVBoxLayout(m_editorFrame);
    frameLayout->setContentsMargins(8, 0, 8, 8);
    frameLayout->setSpacing(8);

    // 编辑器标题（三区统一标题样式）
    QLabel* editorTitle = new QLabel("流程编辑器");
    editorTitle->setObjectName("sectionTitle");
    frameLayout->addWidget(editorTitle);

    // 双模式 StackedWidget
    m_editorStack = new QStackedWidget;
    m_editorStack->setObjectName("editorStack");

    // [0] 流程编辑模式（预留 QGraphicsView 拖拽式编辑器）
    m_flowEditPage = new QWidget;
    QVBoxLayout* flowLayout = new QVBoxLayout(m_flowEditPage);
    flowLayout->setAlignment(Qt::AlignCenter);
    QLabel* flowPlaceholder = new QLabel("流程编辑器\n（拖拽式，待开发）");
    flowPlaceholder->setAlignment(Qt::AlignCenter);
    flowPlaceholder->setObjectName("placeholderLabel");
    flowLayout->addWidget(flowPlaceholder);
    m_editorStack->addWidget(m_flowEditPage);

    // [1] 图像显示模式（预留 ROI 叠加 + 实时采集显示）
    m_imageViewPage = new QWidget;
    QVBoxLayout* imgLayout = new QVBoxLayout(m_imageViewPage);
    imgLayout->setAlignment(Qt::AlignCenter);
    QLabel* imgPlaceholder = new QLabel("图像显示\n（实时采集 + ROI 叠加，待开发）");
    imgPlaceholder->setAlignment(Qt::AlignCenter);
    imgPlaceholder->setObjectName("placeholderLabel");
    imgLayout->addWidget(imgPlaceholder);
    m_editorStack->addWidget(m_imageViewPage);

    m_editorStack->setCurrentIndex(0);  // 默认流程编辑模式

    frameLayout->addWidget(m_editorStack, 1);
}

// ── 右侧视觉工具区 ─────────────────────────────────────────────

void TraditionalWidget::initToolPanel()
{
    m_toolFrame = new QFrame;
    m_toolFrame->setObjectName("sectionFrame");

    QVBoxLayout* toolLayout = new QVBoxLayout(m_toolFrame);
    toolLayout->setContentsMargins(8, 0, 8, 8);
    toolLayout->setSpacing(0);

    // ── 工具区标题 ──
    QLabel* toolTitle = new QLabel("视觉工具");
    toolTitle->setObjectName("sectionTitle");
    toolLayout->addWidget(toolTitle);

    // ── 工具盒 ──
    m_toolBox = new QToolBox;
    m_toolBox->setMinimumWidth(90);

    // 5 个视觉工具分类（均为空占位）
    QWidget* capPage   = new QWidget; capPage->setLayout(new QVBoxLayout);
    QWidget* prePage   = new QWidget; prePage->setLayout(new QVBoxLayout);
    QWidget* locPage   = new QWidget; locPage->setLayout(new QVBoxLayout);
    QWidget* measPage  = new QWidget; measPage->setLayout(new QVBoxLayout);
    QWidget* recPage   = new QWidget; recPage->setLayout(new QVBoxLayout);

    auto addPlaceholder = [](QWidget* page, const QString& text) {
        QLabel* lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setObjectName("placeholderLabel");
        page->layout()->addWidget(lbl);
    };

    addPlaceholder(capPage,  "采集工具\n待开发");
    addPlaceholder(prePage,  "预处理工具\n待开发");
    addPlaceholder(locPage,  "定位工具\n待开发");
    addPlaceholder(measPage, "测量工具\n待开发");
    addPlaceholder(recPage,  "识别工具\n待开发");

    m_toolBox->addItem(capPage,  "采集");
    m_toolBox->addItem(prePage,  "预处理");
    m_toolBox->addItem(locPage,  "定位");
    m_toolBox->addItem(measPage, "测量");
    m_toolBox->addItem(recPage,  "识别");

    toolLayout->addWidget(m_toolBox, 1);
}

// ── 双模式切换 ────────────────────────────────────────────────

void TraditionalWidget::onModeSwitch(int mode)
{
    if (mode >= 0 && mode < m_editorStack->count())
        m_editorStack->setCurrentIndex(mode);
}

// ── 权限响应 ─────────────────────────────────────────────────

void TraditionalWidget::applyPermissions()
{
    // 7 个权限键 → 7 个工具栏按钮的映射
    struct { const char* key; const char* prop; } map[] = {
        { "传统视觉.流程编辑", "actFlow"  },
        { "传统视觉.相机采集", "actCam"   },
        { "传统视觉.相机标定", "actCalib" },
        { "传统视觉.相机建模", "actModel" },
        { "传统视觉.通讯设置", "actComm" },
        { "传统视觉.系统设置", "actSys"   },
    };

    for (auto& m : map) {
        QAction* act = m_toolBar->property(m.prop).value<QAction*>();
        if (act)
            act->setEnabled(m_guard->canWrite(m.key));
    }

    // 视觉工具区权限 → 控制工具区（canRead 可见, canWrite 启用）
    bool canReadProc  = m_guard->canRead("传统视觉.视觉工具区");
    bool canWriteProc = m_guard->canWrite("传统视觉.视觉工具区");
    m_toolBox->setVisible(canReadProc);
    m_toolBox->setEnabled(canWriteProc);
}
