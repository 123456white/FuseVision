#include "AnnotationManagement.h"
#include "AnnotationCanvas.h"
#include "AnnotationModel.h"
#include "AnnotationRender.h"
#include "Tools/AnnotationTool.h"
#include "Tools/BrushTool.h"
#include "Tools/EraserTool.h"
#include "Tools/PolygonTool.h"
#include "Tools/RectTool.h"
#include "Tools/FillTool.h"
#include "Tools/SelectTool.h"
#include "DatasetManagement.h"
#include "core/Logger.h"
#include "core/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPen>
#include <QApplication>
#include <QMenu>
#include <QStack>
#include <QSet>
#include <QHeaderView>
#include <QFileDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QStandardPaths>
#include "core/SettingsManager.h"
// AnnotationUndoCommand
// AnnoUndoCommandLegacy (Legacy — P4 迁移到 AnnotationModel::Transaction)

static QString selectPythonEnv(QWidget* parent);

AnnoUndoCommandLegacy::AnnoUndoCommandLegacy(
        const QList<AnnotationShape>& before,
        const QList<AnnotationShape>& after,
        AnnotationManagement* mgmt,
        const QString& description)
    : QUndoCommand(description),
      m_before(before), m_after(after), m_mgmt(mgmt)
{}

void AnnoUndoCommandLegacy::undo() { m_mgmt->restoreShapes(m_before); }
void AnnoUndoCommandLegacy::redo() { m_mgmt->restoreShapes(m_after); }

// =============================================================================
// 构造
// =============================================================================

AnnotationManagement::AnnotationManagement(QWidget* parent)
    : QWidget(parent)
{
    initUI();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AnnotationManagement::applyTheme);
}

void AnnotationManagement::setDatasetSource(DatasetManagement* dataset)
{
    m_dataset = dataset;
    if (!m_dataset) return;
    m_labelNames  = m_dataset->currentLabelNames();
    m_labelColors = m_dataset->currentLabelColors();
    m_labelType   = m_dataset->currentLabelType();
    m_labelInstanceCounts = m_dataset->currentLabelInstanceCounts();
    m_samples     = m_dataset->allSamples();

    // v3.0: 同步上下文到渲染器
    if (m_render) {
        m_render->setLabelColors(m_labelColors);
        m_render->setLabelOffset(m_dataset->currentLabelOffset());
    }
}

// ── 双向同步：从 DatasetManagement 拉取最新标签，并重新映射 shapes 的 labelId ──
void AnnotationManagement::syncLabels()
{
    if (!m_dataset) return;

    // 1) 保存当前选中标签名称（用于恢复选中）
    QString activeName;
    if (m_activeLabelIdx >= 0 && m_activeLabelIdx < m_labelNames.size())
        activeName = m_labelNames[m_activeLabelIdx];

    // 2) 构建旧 labelId → 新 labelId 映射（按名称匹配）
    QStringList newNames = m_dataset->currentLabelNames();
    QStringList newColors = m_dataset->currentLabelColors();
    QList<int>  newCounts = m_dataset->currentLabelInstanceCounts();

    QMap<int, int> idRemap;
    for (int oldIdx = 0; oldIdx < m_labelNames.size(); ++oldIdx) {
        int newIdx = newNames.indexOf(m_labelNames[oldIdx]);
        if (newIdx >= 0)
            idRemap[oldIdx] = newIdx;
        // else: 旧标签已被删除 → 不映射（shapes 中引用该 labelId 的将被重定向到默认）
    }

    // 3) 更新本地副本
    m_labelNames  = newNames;
    m_labelColors = newColors;
    m_labelInstanceCounts = newCounts;
    m_labelType   = m_dataset->currentLabelType();
    m_samples     = m_dataset->allSamples();

    // 4) 更新 renderer
    if (m_render) {
        m_render->setLabelColors(m_labelColors);
        m_render->setLabelOffset(m_dataset->currentLabelOffset());
    }

    // 5) 重映射当前 shapes 的 labelId
    for (auto& s : m_shapes) {
        if (idRemap.contains(s.labelId))
            s.labelId = idRemap[s.labelId];
        else
            s.labelId = 0;  // 原标签已被删除 → 回退到第一个标签
    }

    // 6) 恢复选中状态
    m_activeLabelIdx = m_labelNames.indexOf(activeName);
    if (m_activeLabelIdx < 0) m_activeLabelIdx = 0;

    // 7) 刷新 UI
    updateLabelTable();
    updateShapeList();
    if (m_render) m_render->invalidateCache();
}

void AnnotationManagement::openImage(const QString& modelId, const QString& imagePath)
{
    if (!m_dataset) return;
    if (!m_currentImagePath.isEmpty() && m_modified)
        saveAnnotations();

    m_loadingOverlay->showOverlay();

    m_labelNames  = m_dataset->currentLabelNames();
    m_labelColors = m_dataset->currentLabelColors();
    m_labelType   = m_dataset->currentLabelType();
    m_labelInstanceCounts = m_dataset->currentLabelInstanceCounts();
    m_samples     = m_dataset->allSamples();
    m_currentModelId = modelId;

    m_currentImagePath = imagePath;
    m_sourceImage = QImage(imagePath);
    if (m_sourceImage.isNull()) {
        Logger::warn(QString::fromUtf8("无法加载图像: %1").arg(imagePath));
        m_loadingOverlay->hideOverlay();
        return;
    }

    m_currentImageIdx = m_dataset->findSampleIndex(imagePath);
    m_shapes.clear();
    m_undoStack->clear();
    loadExistingAnnotations();

    // ── 亮度/对比度调节后的显示图像 ──
    applyImageAdjustments();

    // v3.0: 同步数据到模型和渲染器
    m_model->clear();
    m_model->appendShapes(m_shapes);
    m_render->setSourceImage(m_sourceImage);
    m_render->setLabelColors(m_labelColors);
    m_render->setLabelOffset(m_dataset->currentLabelOffset());  // Halcon minGray
    m_render->invalidateCache();

    renderOverlay();
    m_canvas->setImage(m_displayImage);
    m_canvas->fitToWindow();
    updateNavigationInfo();
    updateLabelTable();
    updateShapeList();
    updateToolUI();
    updateUndoRedoButtons();
    updateNavigator();
    updateFilmIndexLabel();
    m_modified = false;
    m_lblModified->setText("");
    m_loadingOverlay->hideOverlay();
}

void AnnotationManagement::initUI()
{
    setAutoFillBackground(true);
    // 顶层布局：主三栏 + 底部状态栏
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ═══ 左:中:右 = 1:5:1 ═══
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->setHandleWidth(4);
    m_mainSplitter->setChildrenCollapsible(false);

    // ── 左面板 ──
    QWidget* leftPanel = new QWidget;
    leftPanel->setObjectName("annoLeftPanel");
    leftPanel->setMinimumWidth(140);
    leftPanel->setMaximumWidth(280);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(4, 4, 2, 4);
    leftLayout->setSpacing(2);
    initLeftPanel(leftLayout);
    m_mainSplitter->addWidget(leftPanel);

    // ── 中部面板 ──
    QWidget* centerPanel = new QWidget;
    centerPanel->setObjectName("annoCenterPanel");
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    // 中部顶栏
    QWidget* centerTopBar = new QWidget;
    centerTopBar->setObjectName("annoTopBar");
    centerTopBar->setFixedHeight(36);
    QHBoxLayout* topLayout = new QHBoxLayout(centerTopBar);
    topLayout->setContentsMargins(6, 2, 6, 2);
    topLayout->setSpacing(4);
    initCenterTopBar(topLayout);
    centerLayout->addWidget(centerTopBar);

    // 中部主体：工具侧边栏 + 画布（原始页面 0)
    QSplitter* centerSplitter = new QSplitter(Qt::Horizontal);
    centerSplitter->setHandleWidth(2);
    centerSplitter->setChildrenCollapsible(false);

    QWidget* toolPanel = new QWidget;
    toolPanel->setObjectName("annoToolPanel");
    toolPanel->setFixedWidth(44);
    QVBoxLayout* toolLayout = new QVBoxLayout(toolPanel);
    toolLayout->setContentsMargins(2, 4, 2, 4);
    toolLayout->setSpacing(2);
    toolLayout->setAlignment(Qt::AlignTop);
    initCenterToolBar(toolLayout);
    centerSplitter->addWidget(toolPanel);

    // 画布
    m_canvas = new AnnotationCanvas;
    m_canvas->setObjectName("annoCanvas");
    m_canvas->setFocusPolicy(Qt::StrongFocus);
    connect(m_canvas, &AnnotationCanvas::mousePressed,
            this, &AnnotationManagement::onCanvasMousePressed);
    connect(m_canvas, &AnnotationCanvas::mouseMoved,
            this, &AnnotationManagement::onCanvasMouseMoved);
    connect(m_canvas, &AnnotationCanvas::mouseReleased,
            this, &AnnotationManagement::onCanvasMouseReleased);
    connect(m_canvas, &AnnotationCanvas::mouseDoubleClicked,
            this, &AnnotationManagement::onCanvasDoubleClicked);
    connect(m_canvas, &AnnotationCanvas::keyPressed,
            this, &AnnotationManagement::onCanvasKeyPressed);
    connect(m_canvas, &AnnotationCanvas::zoomChanged,
            this, &AnnotationManagement::onCanvasZoomChanged);
    centerSplitter->addWidget(m_canvas);
    centerSplitter->setStretchFactor(0, 0);
    centerSplitter->setStretchFactor(1, 1);

    // ── 加载动画覆盖层（覆盖画布区域）──
    m_loadingOverlay = new LoadingOverlay(m_canvas,
        QString::fromUtf8("正在加载图像..."));

    centerLayout->addWidget(centerSplitter, 1);
    m_mainSplitter->addWidget(centerPanel);

    // ── 右面板 ──
    QWidget* rightPanel = new QWidget;
    rightPanel->setObjectName("annoRightPanel");
    rightPanel->setMinimumWidth(160);
    rightPanel->setMaximumWidth(320);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);
    initRightPanel(rightLayout);
    m_mainSplitter->addWidget(rightPanel);

    m_mainSplitter->setStretchFactor(0, 1);  // 左 1
    m_mainSplitter->setStretchFactor(1, 5);  // 中 5
    m_mainSplitter->setStretchFactor(2, 1);  // 右 1
    m_mainSplitter->setSizes({200, 1000, 200});

    mainLayout->addWidget(m_mainSplitter, 1);

    // ═══ 底部: 状态栏 ═══
    QWidget* bottomBar = new QWidget;
    bottomBar->setObjectName("annoBottomBar");
    bottomBar->setFixedHeight(28);
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(6, 2, 6, 2);
    bottomLayout->setSpacing(6);
    initBottomBar(bottomLayout);
    mainLayout->addWidget(bottomBar);

    // ═══ 撤销栈 ═══
    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(50);

    // ═══ v3.0 数据模型 + 渲染管线 ═══
    m_model = new AnnotationModel(this);
    m_render = new AnnotationRender(this);

    // 模型变更时自动重新渲染
    connect(m_model, &AnnotationModel::dataChanged, this, [this]() {
        if (!m_displayImage.isNull()) {
            bool showHandles = (m_currentTool == AnnoTool::Select ||
                                m_editShapeIndex >= 0);
            QImage overlay = m_render->renderOverlay(
                m_model->shapes(), showHandles, m_editShapeIndex);
            m_canvas->setOverlay(overlay);
        }
        if (m_autoSaveTimer) m_autoSaveTimer->start();
    });

    // 脏状态同步
    connect(m_model, &AnnotationModel::dirtyStateChanged, this, [this](bool dirty) {
        m_modified = dirty;
        m_lblModified->setText(dirty ? "*" : "");
    });

    // ═══ v3.0 工具插件 ═══
    auto registerTool = [this](AnnotationTool* tool) {
        m_toolsMap[tool->type()] = tool;
        connect(tool, &AnnotationTool::requestRender, this, [this]() {
            renderOverlay();
        });
        connect(tool, &AnnotationTool::requestPreviewUpdate, this, [this]() {
            if (m_activeTool && m_canvas) {
                QPainterPath path = m_activeTool->previewPath();
                if (path.isEmpty())
                    m_canvas->clearPreview();
                else
                    m_canvas->setShapesPreview(path,
                        m_activeTool->previewColor(),
                        m_activeTool->previewPenWidth());
            }
        });
        connect(tool, &AnnotationTool::shapeCreated, this, [this]() {
            updateShapeList();
        });
        connect(tool, &AnnotationTool::toolStatusChanged, this, [this](const QString& s) {
            m_lblToolInfo->setText(s);
        });
    };

    registerTool(new BrushTool(this));
    registerTool(new EraserTool(this));
    registerTool(new PolygonTool(this));
    registerTool(new RectTool(this));
    registerTool(new FillTool(this));
    registerTool(new SelectTool(this));

    m_activeTool = m_toolsMap.value(AnnoTool::Brush);

    // ═══ v3.0 自动保存 (30秒无操作) ═══
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(30000);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, [this]() {
        if (m_modified && !m_currentImagePath.isEmpty()) {
            saveAnnotations();
            Logger::info(QString::fromUtf8("[自动保存] %1").arg(
                QFileInfo(m_currentImagePath).fileName()));
        }
    });

    // 导航器跟随视口滚动同步（在所有控件初始化完成后连接，避免初始化期信号触发）
    connect(m_canvas->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &AnnotationManagement::updateNavigator);
    connect(m_canvas->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &AnnotationManagement::updateNavigator);
}

// =============================================================================
// 左面板：标签类别 + 胶片条
// =============================================================================

void AnnotationManagement::initLeftPanel(QVBoxLayout* layout)
{
    // ── 图像导航 ──
    QLabel* navTitle = new QLabel(QString::fromUtf8("图像导航"));
    navTitle->setObjectName("annoSectTitle");
    layout->addWidget(navTitle);

    // 导航按钮行: |◀  ◀   3/50   ▶  ▶|
    QWidget* navBar = new QWidget;
    navBar->setObjectName("annoNavBar");
    QHBoxLayout* navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(2, 2, 2, 2);
    navLayout->setSpacing(2);

    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    m_btnFilmFirst = new QPushButton;
    m_btnFilmFirst->setObjectName("annoFilmBtn");
    m_btnFilmFirst->setCursor(Qt::PointingHandCursor);
    m_btnFilmFirst->setFixedSize(28, 24);
    m_btnFilmFirst->setToolTip(QString::fromUtf8("第一张"));
    m_btnFilmFirst->setIcon(QIcon(
        isDark ? ":/res/AnnoNavFirst_D.png" : ":/res/AnnoNavFirst.png"));
    m_btnFilmFirst->setIconSize(QSize(16, 16));
    connect(m_btnFilmFirst, &QPushButton::clicked, this, [this]() {
        if (m_samples.isEmpty() || m_currentModelId.isEmpty()) return;
        onNavigateTo(0);
    });
    navLayout->addWidget(m_btnFilmFirst);

    m_btnFilmPrev = new QPushButton;
    m_btnFilmPrev->setObjectName("annoFilmBtn");
    m_btnFilmPrev->setCursor(Qt::PointingHandCursor);
    m_btnFilmPrev->setFixedSize(28, 24);
    m_btnFilmPrev->setToolTip(QString::fromUtf8("上一张"));
    m_btnFilmPrev->setIcon(QIcon(
        isDark ? ":/res/AnnoNavPrev_D.png" : ":/res/AnnoNavPrev.png"));
    m_btnFilmPrev->setIconSize(QSize(16, 16));
    connect(m_btnFilmPrev, &QPushButton::clicked, this, &AnnotationManagement::onNavigatePrev);
    navLayout->addWidget(m_btnFilmPrev);

    m_filmIndexLabel = new QLabel(QString::fromUtf8("- / -"));
    m_filmIndexLabel->setObjectName("annoFilmIndex");
    m_filmIndexLabel->setAlignment(Qt::AlignCenter);
    m_filmIndexLabel->setMinimumWidth(48);
    navLayout->addWidget(m_filmIndexLabel);

    m_btnFilmNext = new QPushButton;
    m_btnFilmNext->setObjectName("annoFilmBtn");
    m_btnFilmNext->setCursor(Qt::PointingHandCursor);
    m_btnFilmNext->setFixedSize(28, 24);
    m_btnFilmNext->setToolTip(QString::fromUtf8("下一张"));
    m_btnFilmNext->setIcon(QIcon(
        isDark ? ":/res/AnnoNavNext_D.png" : ":/res/AnnoNavNext.png"));
    m_btnFilmNext->setIconSize(QSize(16, 16));
    connect(m_btnFilmNext, &QPushButton::clicked, this, &AnnotationManagement::onNavigateNext);
    navLayout->addWidget(m_btnFilmNext);

    m_btnFilmLast = new QPushButton;
    m_btnFilmLast->setObjectName("annoFilmBtn");
    m_btnFilmLast->setCursor(Qt::PointingHandCursor);
    m_btnFilmLast->setFixedSize(28, 24);
    m_btnFilmLast->setToolTip(QString::fromUtf8("最后一张"));
    m_btnFilmLast->setIcon(QIcon(
        isDark ? ":/res/AnnoNavLast_D.png" : ":/res/AnnoNavLast.png"));
    m_btnFilmLast->setIconSize(QSize(16, 16));
    connect(m_btnFilmLast, &QPushButton::clicked, this, [this]() {
        if (m_samples.isEmpty() || m_currentModelId.isEmpty()) return;
        onNavigateTo(m_samples.size() - 1);
    });
    navLayout->addWidget(m_btnFilmLast);

    layout->addWidget(navBar);

    // ── 标签表格 ──
    QLabel* labelTitle = new QLabel(QString::fromUtf8("标签类别"));
    labelTitle->setObjectName("annoSectTitle");
    layout->addWidget(labelTitle);

    m_labelTable = new QTableWidget(0, 4);
    m_labelTable->setObjectName("annoLabelTable");
    m_labelTable->setHorizontalHeaderLabels({
        QString::fromUtf8("标号"), "", QString::fromUtf8("名称"),
        QString::fromUtf8("#") });
    m_labelTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_labelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_labelTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_labelTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_labelTable->setColumnWidth(0, 28);
    m_labelTable->setColumnWidth(1, 18);
    m_labelTable->setColumnWidth(3, 28);
    m_labelTable->verticalHeader()->setVisible(false);
    m_labelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_labelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_labelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_labelTable->setShowGrid(false);
    connect(m_labelTable, &QTableWidget::cellPressed,
            this, &AnnotationManagement::onLabelTableCellClicked);
    layout->addWidget(m_labelTable, 1);

    // ── 工具属性 ──
    QLabel* propTitle = new QLabel(QString::fromUtf8("画笔"));
    propTitle->setObjectName("annoSectTitle");
    layout->addWidget(propTitle);

    m_brushSizeLabel = new QLabel(QString::fromUtf8("大小: 12px"));
    m_brushSizeLabel->setObjectName("annoPropLabel");
    layout->addWidget(m_brushSizeLabel);

    m_brushSizeSlider = new QSlider(Qt::Horizontal);
    m_brushSizeSlider->setRange(2, 50);
    m_brushSizeSlider->setValue(12);
    m_brushSizeSlider->setObjectName("annoSlider");
    connect(m_brushSizeSlider, &QSlider::valueChanged,
            this, &AnnotationManagement::onBrushSizeChanged);
    layout->addWidget(m_brushSizeSlider);

    QLabel* opacityLabel = new QLabel(QString::fromUtf8("覆盖透明度: 60%"));
    opacityLabel->setObjectName("annoPropLabel");
    layout->addWidget(opacityLabel);

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(10, 100);
    m_opacitySlider->setValue(60);
    m_opacitySlider->setObjectName("annoSlider");
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this, opacityLabel](int v) {
        opacityLabel->setText(QString::fromUtf8("覆盖透明度: %1%").arg(v));
        onOverlayOpacityChanged(v);
    });
    layout->addWidget(m_opacitySlider);
}

// =============================================================================
// 中部顶栏：保存/撤销/重做 + 图像信息 + 视图控制 + 返回
// =============================================================================

void AnnotationManagement::initCenterTopBar(QHBoxLayout* layout)
{
    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    auto mkBtn = [](const QString& text, const char* obj) {
        auto* b = new QPushButton(text);
        b->setObjectName(obj);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(30);
        return b;
    };
    auto mkIconBtn = [&](const QString& iconName, const QString& tip, const char* obj) {
        auto* b = new QPushButton;
        b->setObjectName(obj);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(30, 30);
        b->setToolTip(tip);
        b->setIcon(QIcon(
            isDark ? QString(":/res/%1_D.png").arg(iconName)
                   : QString(":/res/%1.png").arg(iconName)));
        b->setIconSize(QSize(18, 18));
        return b;
    };

    // ── 文件操作 ──
    m_btnSave = mkIconBtn("AnnoSave",
        QString::fromUtf8("保存标注  (Ctrl+S)"), "annoSaveBtn");
    connect(m_btnSave, &QPushButton::clicked, this, &AnnotationManagement::onSave);
    layout->addWidget(m_btnSave);

    m_btnUndo = mkIconBtn("AnnoUndo",
        QString::fromUtf8("撤销  (Ctrl+Z)"), "annoTopBtn");
    m_btnUndo->setEnabled(false);
    connect(m_btnUndo, &QPushButton::clicked, this, &AnnotationManagement::onUndo);
    layout->addWidget(m_btnUndo);

    m_btnRedo = mkIconBtn("AnnoRedo",
        QString::fromUtf8("重做  (Ctrl+Y)"), "annoTopBtn");
    m_btnRedo->setEnabled(false);
    connect(m_btnRedo, &QPushButton::clicked, this, &AnnotationManagement::onRedo);
    layout->addWidget(m_btnRedo);

    // 分隔线
    QFrame* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setObjectName("annoVSep");
    layout->addWidget(sep1);

    // ── 图像信息 ──
    m_lblImageInfo = new QLabel(QString::fromUtf8("未打开图像"));
    m_lblImageInfo->setObjectName("annoTopInfo");
    layout->addWidget(m_lblImageInfo, 1);

    m_lblModified = new QLabel("");
    m_lblModified->setObjectName("annoModifiedLabel");
    m_lblModified->setFixedWidth(12);
    layout->addWidget(m_lblModified);

    layout->addStretch();

    // ── 视图控制 ──
    m_btnFit = mkIconBtn("AnnoFit",
        QString::fromUtf8("适应窗口  (Ctrl+0)"), "annoTopBtn");
    m_btnFit->setToolTip(QString::fromUtf8("适应窗口\nCtrl+0"));
    connect(m_btnFit, &QPushButton::clicked, this, &AnnotationManagement::onFitToWindow);
    layout->addWidget(m_btnFit);

    m_btnZoom1 = mkIconBtn("AnnoZoom1",
        QString::fromUtf8("原始大小  (Ctrl+1)"), "annoTopBtn");
    m_btnZoom1->setToolTip(QString::fromUtf8("原始大小\nCtrl+1"));
    connect(m_btnZoom1, &QPushButton::clicked, this, &AnnotationManagement::onResetZoom);
    layout->addWidget(m_btnZoom1);

    // 缩放文字按钮
    m_btnZoomOut = new QPushButton(QString::fromUtf8("-"));
    m_btnZoomOut->setObjectName("annoZoomBtn");
    m_btnZoomOut->setCursor(Qt::PointingHandCursor);
    m_btnZoomOut->setFixedSize(30, 30);
    m_btnZoomOut->setToolTip(QString::fromUtf8("缩小  (Ctrl+-)"));
    m_btnZoomOut->setStyleSheet(
        "QPushButton { background: transparent; color: #1a202c; border: 1px solid #c4cad3;"
        " border-radius: 4px; font-weight: bold; font-size: 14px;"
        " min-width: 28px; min-height: 28px; padding: 0px; }"
        "QPushButton:hover { background: #e8ecf0; }");
    connect(m_btnZoomOut, &QPushButton::clicked, this, &AnnotationManagement::onZoomOut);
    layout->addWidget(m_btnZoomOut);

    m_btnZoomIn = new QPushButton(QString::fromUtf8("+"));
    m_btnZoomIn->setObjectName("annoZoomBtn");
    m_btnZoomIn->setCursor(Qt::PointingHandCursor);
    m_btnZoomIn->setFixedSize(30, 30);
    m_btnZoomIn->setToolTip(QString::fromUtf8("放大  (Ctrl++)"));
    m_btnZoomIn->setStyleSheet(
        "QPushButton { background: transparent; color: #1a202c; border: 1px solid #c4cad3;"
        " border-radius: 4px; font-weight: bold; font-size: 14px;"
        " min-width: 28px; min-height: 28px; padding: 0px; }"
        "QPushButton:hover { background: #e8ecf0; }");
    connect(m_btnZoomIn, &QPushButton::clicked, this, &AnnotationManagement::onZoomIn);
    layout->addWidget(m_btnZoomIn);

    // 分隔线
    QFrame* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setObjectName("annoVSep");
    layout->addWidget(sep2);

    m_btnLabelMe = mkBtn(QString::fromUtf8("  LabelMe  "), "annoLabelMeBtn");
    m_btnLabelMe->setMinimumWidth(100);
    m_btnLabelMe->setFixedHeight(34);
    m_btnLabelMe->setToolTip(QString::fromUtf8("使用 LabelMe 打开当前图像进行外部标注\n右键可重新选择 Python 环境"));
    m_btnLabelMe->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_btnLabelMe, &QPushButton::clicked, this, &AnnotationManagement::onLaunchLabelMe);
    connect(m_btnLabelMe, &QPushButton::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(m_btnLabelMe);
        QAction* act = menu.addAction(QString::fromUtf8("重新选择 Python 环境..."));
        if (menu.exec(m_btnLabelMe->mapToGlobal(pos)) == act) {
            QString py = selectPythonEnv(this);
            if (!py.isEmpty()) {
                SettingsManager::instance().setLabelmePythonPath(py);
                SettingsManager::instance().save();
            }
        }
    });
    layout->addWidget(m_btnLabelMe);

    m_btnBack = mkBtn(QString::fromUtf8("返回数据集管理"), "annoBackBtn");
    m_btnBack->setFixedHeight(30);
    connect(m_btnBack, &QPushButton::clicked, this, &AnnotationManagement::onReturnToGallery);
    layout->addWidget(m_btnBack);
}

// =============================================================================
// 中部工具侧边栏（垂直）
// =============================================================================

void AnnotationManagement::initCenterToolBar(QVBoxLayout* layout)
{
    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);

    struct ToolDef {
        int id;
        QString iconName;
        QString tip;
    };
    QList<ToolDef> tools = {
        { AnnoTool::Brush,     "AnnoBrush",     QString::fromUtf8("画笔 (B)\n涂抹绘制标注区域\n[ ] 调整笔刷大小") },
        { AnnoTool::Eraser,    "AnnoEraser",    QString::fromUtf8("橡皮 (E)\n擦除标注区域\n右键临时橡皮") },
        { AnnoTool::Polygon,   "AnnoPolygon",   QString::fromUtf8("多边形 (P)\n点击顶点 | 双击闭合\nEnter闭合 | Back退格 | Esc取消") },
        { AnnoTool::Rectangle, "AnnoRectangle", QString::fromUtf8("矩形 (R)\n两次点击 | 拖拽\nEnter确认 | Esc取消") },
        { AnnoTool::Fill,      "AnnoFill",      QString::fromUtf8("填充 (F)\n单击封闭区域填充") },
        { AnnoTool::Select,    "AnnoSelect",    QString::fromUtf8("选择 (S)\n移动/编辑形状\nDel删除 | Ctrl+D复制") },
    };

    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;

    for (const auto& t : tools) {
        QPushButton* btn = new QPushButton;
        btn->setCheckable(true);
        btn->setObjectName("annoToolBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(36, 36);
        btn->setToolTip(t.tip);
        btn->setIcon(QIcon(
            isDark ? QString(":/res/%1_D.png").arg(t.iconName)
                   : QString(":/res/%1.png").arg(t.iconName)));
        btn->setIconSize(QSize(24, 24));
        m_toolGroup->addButton(btn, t.id);
        layout->addWidget(btn);
    }
    m_toolGroup->button(AnnoTool::Brush)->setChecked(true);
    connect(m_toolGroup, &QButtonGroup::idClicked,
            this, &AnnotationManagement::onToolChanged);

    layout->addStretch();  // 填充剩余空间，避免底部空白区域
}

// =============================================================================
// 右面板：标注列表 + 图像调节 + 导航器
// =============================================================================

void AnnotationManagement::initRightPanel(QVBoxLayout* layout)
{
    // ── 图像导航器（顶部）──
    QLabel* navTitle = new QLabel(QString::fromUtf8("导航器"));
    navTitle->setObjectName("annoSectTitle");
    layout->addWidget(navTitle);

    m_navigatorLabel = new QLabel;
    m_navigatorLabel->setObjectName("annoNavigator");
    m_navigatorLabel->setAlignment(Qt::AlignCenter);
    m_navigatorLabel->setMinimumHeight(80);
    m_navigatorLabel->setMaximumHeight(160);
    m_navigatorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_navigatorLabel->setScaledContents(false);
    layout->addWidget(m_navigatorLabel);

    layout->addSpacing(4);

    // ── 图像调节（中间）──
    QLabel* adjustTitle = new QLabel(QString::fromUtf8("图像调节"));
    adjustTitle->setObjectName("annoSectTitle");
    layout->addWidget(adjustTitle);

    // 亮度
    m_brightnessLabel = new QLabel(QString::fromUtf8("亮度: 0"));
    m_brightnessLabel->setObjectName("annoPropLabel");
    layout->addWidget(m_brightnessLabel);
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    m_brightnessSlider->setObjectName("annoSlider");
    connect(m_brightnessSlider, &QSlider::valueChanged,
            this, &AnnotationManagement::onBrightnessChanged);
    layout->addWidget(m_brightnessSlider);

    // 对比度
    m_contrastLabel = new QLabel(QString::fromUtf8("对比度: 0"));
    m_contrastLabel->setObjectName("annoPropLabel");
    layout->addWidget(m_contrastLabel);
    m_contrastSlider = new QSlider(Qt::Horizontal);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);
    m_contrastSlider->setObjectName("annoSlider");
    connect(m_contrastSlider, &QSlider::valueChanged,
            this, &AnnotationManagement::onContrastChanged);
    layout->addWidget(m_contrastSlider);

    layout->addSpacing(4);

    // ── 标注列表（底部，拉伸填充）──
    QLabel* shapeTitle = new QLabel(QString::fromUtf8("标注列表"));
    shapeTitle->setObjectName("annoSectTitle");
    layout->addWidget(shapeTitle);

    m_shapeList = new QListWidget;
    m_shapeList->setObjectName("annoShapeList");
    m_shapeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_shapeList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_shapeList->setIconSize(QSize(14, 14));
    connect(m_shapeList, &QListWidget::customContextMenuRequested,
            this, &AnnotationManagement::onShapeListContextMenu);
    layout->addWidget(m_shapeList, 1);

    m_btnDeleteShape = new QPushButton(QString::fromUtf8("🗑 删除选中"));
    m_btnDeleteShape->setObjectName("annoDeleteBtn");
    m_btnDeleteShape->setCursor(Qt::PointingHandCursor);
    m_btnDeleteShape->setFixedHeight(26);
    m_btnDeleteShape->setEnabled(false);
    connect(m_btnDeleteShape, &QPushButton::clicked,
            this, &AnnotationManagement::onDeleteSelectedShapes);
    layout->addWidget(m_btnDeleteShape);
}

// =============================================================================
// 底部工具+状态栏
// =============================================================================

void AnnotationManagement::initBottomBar(QHBoxLayout* layout)
{
    m_lblPosition = new QLabel(QString::fromUtf8("(0, 0)"));
    m_lblPosition->setObjectName("annoStatusLabel");
    layout->addWidget(m_lblPosition);

    m_lblDimensions = new QLabel("");
    m_lblDimensions->setObjectName("annoStatusLabel");
    layout->addWidget(m_lblDimensions);

    layout->addStretch();

    // 缩放滑块
    QLabel* zoomIcon = new QLabel(QString::fromUtf8("🔍"));
    zoomIcon->setObjectName("annoStatusLabel");
    layout->addWidget(zoomIcon);

    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setObjectName("annoZoomSlider");
    m_zoomSlider->setRange(10, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(120);
    m_zoomSlider->setToolTip(QString::fromUtf8("缩放"));
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        m_canvas->setZoom(v / 100.0);
    });
    layout->addWidget(m_zoomSlider);

    m_lblZoom = new QLabel(QString::fromUtf8("100%"));
    m_lblZoom->setObjectName("annoStatusLabel");
    m_lblZoom->setMinimumWidth(40);
    layout->addWidget(m_lblZoom);

    layout->addSpacing(8);

    // 分隔线
    QFrame* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setObjectName("annoVSep");
    layout->addWidget(sep);

    layout->addSpacing(8);

    m_lblToolInfo = new QLabel(QString::fromUtf8("画笔 | 12px"));
    m_lblToolInfo->setObjectName("annoStatusLabel");
    layout->addWidget(m_lblToolInfo);
}

// =============================================================================
// 导航
// =============================================================================

void AnnotationManagement::updateFilmIndexLabel()
{
    if (m_currentImageIdx >= 0 && m_currentImageIdx < m_samples.size()) {
        m_filmIndexLabel->setText(QString("%1 / %2")
            .arg(m_currentImageIdx + 1).arg(m_samples.size()));
    } else {
        m_filmIndexLabel->setText(QString::fromUtf8("- / -"));
    }
}

void AnnotationManagement::updateNavigationInfo()
{
    if (m_currentImageIdx < 0 || m_samples.isEmpty()) {
        m_lblImageInfo->setText(QFileInfo(m_currentImagePath).fileName());
    } else {
        m_lblImageInfo->setText(QString::fromUtf8("%1  (%2/%3)")
            .arg(m_samples[m_currentImageIdx].fileName)
            .arg(m_currentImageIdx + 1)
            .arg(m_samples.size()));
    }
    if (!m_sourceImage.isNull())
        m_lblDimensions->setText(QString::fromUtf8("%1×%2")
            .arg(m_sourceImage.width()).arg(m_sourceImage.height()));
}

void AnnotationManagement::onNavigateNext()
{
    if (m_currentImageIdx < 0 || m_samples.isEmpty()) return;
    if (m_modified) saveAnnotations();
    if (m_currentImageIdx + 1 >= m_samples.size()) return;
    openImage(m_currentModelId, m_samples[m_currentImageIdx + 1].absolutePath);
}

void AnnotationManagement::onNavigatePrev()
{
    if (m_currentImageIdx <= 0 || m_samples.isEmpty()) return;
    if (m_modified) saveAnnotations();
    openImage(m_currentModelId, m_samples[m_currentImageIdx - 1].absolutePath);
}

void AnnotationManagement::onNavigateTo(int index)
{
    if (index < 0 || index >= m_samples.size()) return;
    if (index == m_currentImageIdx) return;
    if (m_modified) saveAnnotations();
    openImage(m_currentModelId, m_samples[index].absolutePath);
}

void AnnotationManagement::onReturnToGallery()
{
    if (m_modified) saveAnnotations();
    emit returnToGallery();
}

// 尝试注册 python.exe（去重，检测可执行性）
static void tryAddPython(const QString& exePath, QSet<QString>& seen, QStringList& envs)
{
    QFileInfo fi(exePath);
    if (!fi.exists() || !fi.isExecutable()) return;
    QString canonical = fi.canonicalFilePath();
    if (seen.contains(canonical)) return;
    seen.insert(canonical);
    envs << QDir::toNativeSeparators(exePath);
}

// 扫描一个目录下所有子目录中的 Scripts\python.exe（venv / virtualenv 模式）
static void scanVenvsInDir(const QString& root, int maxDepth,
                            QSet<QString>& seen, QStringList& envs)
{
    if (maxDepth < 0 || !QDir(root).exists()) return;
    QDir dir(root);
    QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        QString subDir = root + "/" + entry;
        // 检测该子目录是否就是 venv 根（含 Scripts/python.exe）
        tryAddPython(subDir + "/Scripts/python.exe", seen, envs);
        // 同时检测其 Scripts 子目录（扁平结构）
        if (QDir(subDir + "/Scripts").exists()) {
            tryAddPython(subDir + "/Scripts/python.exe", seen, envs);
        }
        // 递归扫描（限制深度防止过慢）
        if (maxDepth > 0)
            scanVenvsInDir(subDir, maxDepth - 1, seen, envs);
    }
}

// 扫描模式目录（如 C:/Python*、%LOCALAPPDATA%/Programs/Python/*）
static void scanGlobPython(const QString& pattern,
                            QSet<QString>& seen, QStringList& envs)
{
    QString parent = pattern.section('*', 0, 0);
    if (!QDir(parent).exists()) return;
    QStringList entries = QDir(parent).entryList(
        {QFileInfo(pattern).fileName()}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        tryAddPython(parent + "/" + entry + "/python.exe", seen, envs);
    }
}

static QStringList detectPythonEnvs()
{
    QStringList envs;
    QSet<QString> seen;
    QString home = qEnvironmentVariable("USERPROFILE");
    QString appData = qEnvironmentVariable("APPDATA");
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");

    // ── 1. PATH 中的 python ──────────────────────────────
    QString pathEnv = qEnvironmentVariable("PATH");
    for (const QString& dir : pathEnv.split(';', Qt::SkipEmptyParts))
        tryAddPython(QDir::cleanPath(dir + "/python.exe"), seen, envs);

    // ── 2. 全局 Python 安装 ──────────────────────────────
    scanGlobPython("C:/Python*", seen, envs);
    if (!localAppData.isEmpty())
        scanGlobPython(localAppData + "/Programs/Python/*", seen, envs);
    QString pf = qEnvironmentVariable("ProgramFiles");
    QString pfx86 = qEnvironmentVariable("ProgramFiles(x86)");
    if (!pf.isEmpty())    scanGlobPython(pf + "/Python*", seen, envs);
    if (!pfx86.isEmpty()) scanGlobPython(pfx86 + "/Python*", seen, envs);

    // ── 3. pyenv-win ─────────────────────────────────────
    if (!home.isEmpty())
        scanGlobPython(home + "/.pyenv/pyenv-win/versions/*", seen, envs);

    // ── 4. Conda / Miniconda / Anaconda ──────────────────
    QStringList condaRoots;
    if (!home.isEmpty()) {
        condaRoots << home + "/miniconda3"
                   << home + "/anaconda3"
                   << home + "/AppData/Local/miniconda3"
                   << home + "/AppData/Local/anaconda3"
                   << "C:/Miniconda3"   << "C:/Anaconda3"
                   << "C:/ProgramData/miniconda3"
                   << "C:/ProgramData/anaconda3";
    }
    for (const QString& root : condaRoots) {
        // base python
        tryAddPython(root + "/python.exe", seen, envs);
        // conda envs
        QDir envsDir(root + "/envs");
        if (envsDir.exists()) {
            QStringList envNames = envsDir.entryList(
                QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& env : envNames)
                tryAddPython(root + "/envs/" + env + "/python.exe", seen, envs);
        }
    }

    // ── 5. pipenv (PIPENV_VENV_IN_PROJECT / 全局 .virtualenvs) ──
    if (!home.isEmpty()) {
        scanVenvsInDir(home + "/.virtualenvs", 2, seen, envs);
        // pipenv project-local: <project>/.venv/Scripts/python.exe 已在 venv 扫描覆盖
    }

    // ── 6. virtualenvwrapper-win (WORKON_HOME, 默认 %USERPROFILE%\Envs) ──
    QString workonHome = qEnvironmentVariable("WORKON_HOME");
    if (workonHome.isEmpty() && !home.isEmpty()) workonHome = home + "/Envs";
    if (!workonHome.isEmpty()) scanVenvsInDir(workonHome, 2, seen, envs);

    // ── 7. Poetry 缓存 (Windows: %APPDATA%\pypoetry\Cache\virtualenvs) ──
    if (!appData.isEmpty()) {
        QString poetryHome = qEnvironmentVariable("POETRY_CACHE_DIR");
        if (poetryHome.isEmpty()) poetryHome = appData + "/pypoetry/Cache/virtualenvs";
        scanVenvsInDir(poetryHome, 2, seen, envs);
    }

    // ── 8. uv 工具缓存 ───────────────────────────────────
    if (!localAppData.isEmpty()) {
        // uv 在 %LOCALAPPDATA%\uv\cache 下的 .venv-* 目录
        QStringList entries = QDir(localAppData + "/uv/cache").entryList(
            {"*"}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries)
            tryAddPython(localAppData + "/uv/cache/" + entry + "/Scripts/python.exe", seen, envs);
    }

    // ── 9. 项目目录递归扫描 .venv / venv（限深 3 层） ──
    scanVenvsInDir(
        QString::fromUtf8(FUSEVISION_SOURCE_DIR), 3, seen, envs);

    // ── 10. Windows Store Python ─────────────────────────
    if (!localAppData.isEmpty()) {
        QStringList wsEntries = QDir(localAppData + "/Microsoft/WindowsApps").entryList(
            {"python*.exe"}, QDir::Files | QDir::NoDotAndDotDot);
        for (const QString& exe : wsEntries) {
            if (exe.contains("python") && !exe.contains("_")) // 排除 shim
                tryAddPython(localAppData + "/Microsoft/WindowsApps/" + exe, seen, envs);
        }
    }

    // ── 11. 通过 py launcher 发现 ────────────────────────
    // py -0 列出已安装版本，但无法直接获取路径；尝试常见 py-launcher 安装位置
    if (!localAppData.isEmpty())
        tryAddPython(localAppData + "/Programs/Python/Launcher/py.exe", seen, envs);

    return envs;
}

// 检测 Python 是否安装了 labelme
static bool validateLabelMe(const QString& pythonExe)
{
    QProcess proc;
    proc.start(pythonExe, {"-c", "import labelme"});
    proc.waitForFinished(8000);
    return proc.exitCode() == 0 && proc.readAllStandardError().isEmpty();
}

// 显示 Python 环境选择对话框，返回选中的 python.exe 路径（空 = 取消）
static QString selectPythonEnv(QWidget* parent)
{
    QStringList envs = detectPythonEnvs();

    // ── 构建对话框 ──
    QDialog dlg(parent);
    dlg.setWindowTitle(QString::fromUtf8("选择 Python 环境"));
    dlg.setMinimumWidth(520);

    auto* lay = new QVBoxLayout(&dlg);
    auto* infoLbl = new QLabel(
        QString::fromUtf8("请选择用于启动 LabelMe 的 Python 环境（需安装 labelme 包）："));
    infoLbl->setWordWrap(true);
    lay->addWidget(infoLbl);

    // 表格方式展示（路径 + 状态列）
    auto* table = new QTableWidget(envs.size(), 2, &dlg);
    table->setHorizontalHeaderLabels({QString::fromUtf8("Python 路径"), QString::fromUtf8("状态")});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->setColumnWidth(1, 140);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(220);

    for (int i = 0; i < envs.size(); ++i) {
        auto* pathItem = new QTableWidgetItem(envs[i]);
        pathItem->setToolTip(envs[i]);
        table->setItem(i, 0, pathItem);

        bool ok = validateLabelMe(envs[i]);
        auto* statusItem = new QTableWidgetItem(
            ok ? QString::fromUtf8("✓ labelme 可用")
               : QString::fromUtf8("✗ 未安装 labelme"));
        statusItem->setForeground(ok ? QColor("#4caf50") : QColor("#f44336"));
        table->setItem(i, 1, statusItem);

        // 自动选中第一个可用的
        if (ok && table->currentRow() < 0)
            table->selectRow(i);
    }

    lay->addWidget(table);

    // 浏览按钮
    auto* browseBtn = new QPushButton(QString::fromUtf8("浏览..."));
    QObject::connect(browseBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(
            &dlg, QString::fromUtf8("选择 python.exe"),
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
            QString::fromUtf8("Python 可执行文件 (python.exe)"));
        if (!path.isEmpty()) {
            QFileInfo fi(path);
            if (fi.exists() && fi.isReadable()) {
                int row = table->rowCount();
                table->setRowCount(row + 1);
                auto* pathItem = new QTableWidgetItem(QDir::toNativeSeparators(fi.absoluteFilePath()));
                pathItem->setToolTip(fi.absoluteFilePath());
                table->setItem(row, 0, pathItem);
                bool ok = validateLabelMe(fi.absoluteFilePath());
                auto* statusItem = new QTableWidgetItem(
                    ok ? QString::fromUtf8("✓ labelme 可用")
                       : QString::fromUtf8("✗ 未安装 labelme"));
                statusItem->setForeground(ok ? QColor("#4caf50") : QColor("#f44336"));
                table->setItem(row, 1, statusItem);
                table->selectRow(row);
            }
        }
    });

    auto* btnLay = new QHBoxLayout();
    btnLay->addWidget(browseBtn);
    btnLay->addStretch();

    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    btnLay->addWidget(btnBox);
    lay->addLayout(btnLay);

    if (dlg.exec() != QDialog::Accepted)
        return {};

    int row = table->currentRow();
    if (row < 0 || row >= table->rowCount())
        return {};

    return table->item(row, 0)->text();
}

void AnnotationManagement::onLaunchLabelMe()
{
    if (m_currentImagePath.isEmpty()) return;

    // ── 确定 Python 解释器 ──
    QString pythonExe = SettingsManager::instance().labelmePythonPath();
    if (pythonExe.isEmpty() || !QFileInfo::exists(pythonExe)) {
        pythonExe = selectPythonEnv(this);
        if (pythonExe.isEmpty()) return;
        SettingsManager::instance().setLabelmePythonPath(pythonExe);
        SettingsManager::instance().save();
    }

    // ── SDK 路径 ──
    QString sdkPath = QString::fromUtf8(FUSEVISION_SOURCE_DIR "/src/labelme");
    if (!QDir(sdkPath).exists()) {
        sdkPath = QDir::cleanPath(
            QApplication::applicationDirPath() + "/src/labelme");
    }

    // ── 收集参数 → 发出信号让 DeepLearningWidget 全屏嵌入 ──
    QString imgDir = QFileInfo(m_currentImagePath).absolutePath();
    QStringList labels;
    QString outputDir;
    if (m_dataset) {
        labels = m_dataset->currentLabelNames();
        QString annDir = m_dataset->annotationsDir();
        if (QDir(annDir).exists())
            outputDir = annDir;
    }

    emit labelMeRequested(imgDir, m_currentImagePath, pythonExe, sdkPath,
                          labels, outputDir);
}

// =============================================================================
// 工具切换
// =============================================================================

void AnnotationManagement::onToolChanged(int toolId)
{
    // v3.0: 停用旧工具，切换到新工具
    if (m_activeTool && m_activeTool->type() == toolId) return;

    AnnotationTool::Context ctx;
    ctx.sourceImage = m_sourceImage;
    ctx.model = m_model;
    ctx.render = m_render;
    ctx.labelColors = m_labelColors;
    ctx.activeLabelId = m_activeLabelIdx;
    ctx.brushSize = m_brushSize;

    if (m_activeTool) {
        m_activeTool->deactivated(ctx);
        // 断开旧工具的光标
        m_canvas->setCursor(Qt::ArrowCursor);
    }

    m_activeTool = m_toolsMap.value(toolId);
    m_currentTool = toolId;

    if (m_activeTool) {
        m_activeTool->activated(ctx);
        m_canvas->setCursor(m_activeTool->cursor());
        m_erasing = (toolId == AnnoTool::Eraser);
    }

    // 清除选择状态
    m_editShapeIndex = -1;
    m_editVertexIndex = -1;
    m_rectPlacedStart = false;
    m_drawing = false;

    updateToolUI();
    m_canvas->clearPreview();
    renderOverlay();
}

void AnnotationManagement::onLabelSelected(int labelIdx)
{
    m_activeLabelIdx = labelIdx;
    updateLabelTable();
    updateToolUI();
}

void AnnotationManagement::onBrushSizeChanged(int value)
{
    m_brushSize = value;
    m_brushSizeLabel->setText(QString::fromUtf8("画笔大小: %1px").arg(value));
    updateToolUI();
}

void AnnotationManagement::onOverlayOpacityChanged(int value)
{
    m_canvas->setOverlayOpacity(value / 100.0);
}

void AnnotationManagement::onBrightnessChanged(int value)
{
    m_brightness = value;
    m_brightnessLabel->setText(QString::fromUtf8("亮度: %1").arg(value));
    applyImageAdjustments();
}

void AnnotationManagement::onContrastChanged(int value)
{
    m_contrast = value;
    m_contrastLabel->setText(QString::fromUtf8("对比度: %1").arg(value));
    applyImageAdjustments();
}

void AnnotationManagement::applyImageAdjustments()
{
    if (m_sourceImage.isNull()) return;

    // 不改变原图，仅在显示图像上做调整
    m_displayImage = m_sourceImage;

    // 亮度: -100 ~ 100 → 倍增因子 0.2 ~ 3.0 (1.0 = 不变)
    double bFactor = 1.0 + m_brightness / 100.0;
    // 对比度: -100 ~ 100 → 因子 0.0 ~ 4.0 (1.0 = 不变)
    double cFactor = 1.0 + m_contrast / 100.0;

    if (bFactor != 1.0 || cFactor != 1.0) {
        // 转为 ARGB32 以便逐像素处理
        m_displayImage = m_displayImage.convertToFormat(QImage::Format_ARGB32);
        const int h = m_displayImage.height();
        const int w = m_displayImage.width();
        const double half = 127.5;

        for (int y = 0; y < h; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(m_displayImage.scanLine(y));
            for (int x = 0; x < w; ++x) {
                QRgb p = line[x];
                int r = qBound(0, qRound((qRed(p) - half) * cFactor + half * bFactor), 255);
                int g = qBound(0, qRound((qGreen(p) - half) * cFactor + half * bFactor), 255);
                int b = qBound(0, qRound((qBlue(p) - half) * cFactor + half * bFactor), 255);
                line[x] = qRgba(r, g, b, qAlpha(p));
            }
        }
    }

    if (!m_displayImage.isNull()) {
        m_canvas->setImage(m_displayImage);
        updateNavigator();
    }
}

void AnnotationManagement::updateNavigator()
{
    if (m_sourceImage.isNull() || !m_navigatorLabel) return;

    // 缩略图缩放：保持宽高比适配 navigator 区域
    QSize labelSize = m_navigatorLabel->size();
    if (labelSize.width() < 10) labelSize = QSize(160, 120); // 兜底
    QImage thumb = m_displayImage.isNull() ? m_sourceImage
        : m_displayImage.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (thumb.isNull()) thumb = m_sourceImage.scaled(
        labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (thumb.isNull()) return;

    // 绘制当前视口矩形指示框
    if (m_canvas && m_canvas->hasImage()) {
        QPainter painter(&thumb);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF imgRect(0, 0, m_sourceImage.width(), m_sourceImage.height());
        QRectF viewportRect = m_canvas->mapToScene(
            m_canvas->viewport()->rect()).boundingRect();

        // 将视口矩形映射到缩略图坐标
        double sx = thumb.width() / imgRect.width();
        double sy = thumb.height() / imgRect.height();
        QRectF vr(viewportRect.x() * sx, viewportRect.y() * sy,
                  viewportRect.width() * sx, viewportRect.height() * sy);

        painter.setPen(QPen(QColor(0, 180, 255), 2));
        painter.setBrush(QColor(0, 180, 255, 40));
        painter.drawRect(vr);
        painter.end();
    }

    m_navigatorLabel->setPixmap(QPixmap::fromImage(thumb));
}

void AnnotationManagement::onFitToWindow()  { m_canvas->fitToWindow(); updateNavigator(); }
void AnnotationManagement::onResetZoom()    { m_canvas->resetZoom();   updateNavigator(); }
void AnnotationManagement::onZoomIn()       { m_canvas->zoomIn();      updateNavigator(); }
void AnnotationManagement::onZoomOut()      { m_canvas->zoomOut();     updateNavigator(); }

void AnnotationManagement::onCanvasZoomChanged(double level)
{
    int pct = (int)(level * 100);
    m_lblZoom->setText(QString::fromUtf8("%1%").arg(pct));
    // 同步滑块，避免信号循环
    if (m_zoomSlider) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(pct);
        m_zoomSlider->blockSignals(false);
    }
    updateNavigator();
}

void AnnotationManagement::updateToolUI()
{
    bool showBrush = (m_currentTool == AnnoTool::Brush ||
                      m_currentTool == AnnoTool::Eraser);
    m_brushSizeSlider->setVisible(showBrush);
    m_brushSizeLabel->setVisible(showBrush);

    static const char* s_names[] = { "画笔", "橡皮擦", "多边形", "矩形", "填充", "选择" };
    int ti = qBound(0, m_currentTool, 5);
    QString info = QString::fromUtf8(s_names[ti]);
    if (showBrush) info += QString::fromUtf8(" | %1px").arg(m_brushSize);
    if (m_activeLabelIdx >= 0 && m_activeLabelIdx < m_labelNames.size())
        info += QString::fromUtf8(" | %1").arg(m_labelNames[m_activeLabelIdx]);
    m_lblToolInfo->setText(info);

    switch (m_currentTool) {
    case AnnoTool::Brush:     m_canvas->setCursor(Qt::CrossCursor); break;
    case AnnoTool::Eraser:    m_canvas->setCursor(Qt::CrossCursor); break;
    case AnnoTool::Polygon:   m_canvas->setCursor(Qt::CrossCursor); break;
    case AnnoTool::Rectangle: m_canvas->setCursor(Qt::CrossCursor); break;
    case AnnoTool::Fill:      m_canvas->setCursor(Qt::PointingHandCursor); break;
    case AnnoTool::Select:    m_canvas->setCursor(Qt::ArrowCursor); break;
    }
}

// =============================================================================
// 键盘快捷 键
// =============================================================================

void AnnotationManagement::onCanvasKeyPressed(int key, Qt::KeyboardModifiers mods)
{
    // ═══ Ctrl 组合键 ═══
    if (mods == Qt::ControlModifier) {
        switch (key) {
        case Qt::Key_S:      onSave(); return;
        case Qt::Key_Z:      onUndo(); return;
        case Qt::Key_Y:      onRedo(); return;
        case Qt::Key_0:      onFitToWindow(); return;
        case Qt::Key_1:      onResetZoom(); return;
        case Qt::Key_Plus:
        case Qt::Key_Equal:  onZoomIn(); return;
        case Qt::Key_Minus:  onZoomOut(); return;
        case Qt::Key_C: {
            // 复制选中标注到剪贴板
            m_clipboard.clear();
            for (auto* item : m_shapeList->selectedItems()) {
                int idx = item->data(Qt::UserRole).toInt();
                if (idx >= 0 && idx < m_shapes.size())
                    m_clipboard.append(m_shapes[idx]);
            }
            if (!m_clipboard.isEmpty())
                Logger::info(QString::fromUtf8("已复制 %1 个标注").arg(m_clipboard.size()));
            return;
        }
        case Qt::Key_V: {
            // 粘贴剪贴板中的标注
            if (m_clipboard.isEmpty()) return;
            pushUndoState(QString::fromUtf8("粘贴标注"));
            for (auto s : m_clipboard) {
                QPointF offset(20, 20);
                switch (s.type) {
                case AnnotationShape::PolygonShape:    s.polygon.translate(offset); break;
                case AnnotationShape::RectShape:
                case AnnotationShape::RotatedRectShape: s.bbox.translate(offset); break;
                case AnnotationShape::BrushMask: {
                    QImage shifted(s.maskLayer.size(), QImage::Format_Grayscale8);
                    shifted.fill(0);
                    QPainter sp(&shifted);
                    sp.drawImage(QPointF(20, 20), s.maskLayer);
                    sp.end();
                    s.maskLayer = shifted;
                    break;
                }
                }
                m_shapes.append(s);
            }
            commitUndoState(QString::fromUtf8("粘贴标注"));
            m_modified = true; m_lblModified->setText("*");
            renderOverlay(); updateShapeList();
            return;
        }
        case Qt::Key_D: {
            // Ctrl+D = 复制并在原位偏移粘贴
            QList<QListWidgetItem*> sel = m_shapeList->selectedItems();
            if (sel.isEmpty()) return;
            m_clipboard.clear();
            for (auto* item : sel) {
                int idx = item->data(Qt::UserRole).toInt();
                if (idx >= 0 && idx < m_shapes.size())
                    m_clipboard.append(m_shapes[idx]);
            }
            if (m_clipboard.isEmpty()) return;
            pushUndoState(QString::fromUtf8("复制标注"));
            for (auto s : m_clipboard) {
                QPointF offset(20, 20);
                switch (s.type) {
                case AnnotationShape::PolygonShape:    s.polygon.translate(offset); break;
                case AnnotationShape::RectShape:
                case AnnotationShape::RotatedRectShape: s.bbox.translate(offset); break;
                case AnnotationShape::BrushMask: {
                    QImage shifted(s.maskLayer.size(), QImage::Format_Grayscale8);
                    shifted.fill(0);
                    QPainter sp(&shifted);
                    sp.drawImage(QPointF(20, 20), s.maskLayer);
                    sp.end();
                    s.maskLayer = shifted;
                    break;
                }
                }
                m_shapes.append(s);
            }
            commitUndoState(QString::fromUtf8("复制标注"));
            m_modified = true; m_lblModified->setText("*");
            renderOverlay(); updateShapeList();
            return;
        }
        }
        return;  // 未识别的 Ctrl 组合键不传递给工具
    }

    // ═══ 先转发给活动工具（Enter/Backspace/Escape 由工具自行处理） ═══
    if (m_activeTool) {
        m_activeTool->keyPress(key, mods);
    }

    // ═══ 全局快捷键（非 Ctrl） ═══
    switch (key) {
    case Qt::Key_Delete:
        onDeleteSelectedShapes(); return;
    case Qt::Key_Home:       onNavigateTo(0); return;
    case Qt::Key_End:        onNavigateTo(m_samples.size() - 1); return;
    case Qt::Key_PageUp:     onNavigatePrev(); return;
    case Qt::Key_PageDown:   onNavigateNext(); return;
    case Qt::Key_BracketLeft:
        m_brushSizeSlider->setValue(qMax(2, m_brushSize - 2)); return;
    case Qt::Key_BracketRight:
        m_brushSizeSlider->setValue(qMin(50, m_brushSize + 2)); return;
    case Qt::Key_D:          onNavigateNext(); return;
    case Qt::Key_A:          onNavigatePrev(); return;
    case Qt::Key_0:          onResetZoom(); return;
    case Qt::Key_Minus:      onZoomOut(); return;
    case Qt::Key_Equal:      onZoomIn(); return;
    }

    // 数字键 1-9 → 快速选标签
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        int li = key - Qt::Key_1;
        if (li < m_labelNames.size()) { onLabelSelected(li); return; }
    }

    // 工具切换（首字母助记）
    switch (key) {
    case Qt::Key_B: m_toolGroup->button(AnnoTool::Brush)->setChecked(true);
        onToolChanged(AnnoTool::Brush); break;
    case Qt::Key_E: m_toolGroup->button(AnnoTool::Eraser)->setChecked(true);
        onToolChanged(AnnoTool::Eraser); break;
    case Qt::Key_P: m_toolGroup->button(AnnoTool::Polygon)->setChecked(true);
        onToolChanged(AnnoTool::Polygon); break;
    case Qt::Key_R: m_toolGroup->button(AnnoTool::Rectangle)->setChecked(true);
        onToolChanged(AnnoTool::Rectangle); break;
    case Qt::Key_F: m_toolGroup->button(AnnoTool::Fill)->setChecked(true);
        onToolChanged(AnnoTool::Fill); break;
    case Qt::Key_S: m_toolGroup->button(AnnoTool::Select)->setChecked(true);
        onToolChanged(AnnoTool::Select); break;
    }
}

// =============================================================================
// 标签表格
// =============================================================================

void AnnotationManagement::onLabelTableCellClicked(int row, int /*column*/)
{
    if (row >= 0 && row < m_labelNames.size())
        onLabelSelected(row);
}

void AnnotationManagement::updateLabelTable()
{
    m_labelTable->setRowCount(m_labelNames.size());
    for (int i = 0; i < m_labelNames.size(); ++i) {
        // 标号
        QTableWidgetItem* idxItem = new QTableWidgetItem(QString::number(i));
        idxItem->setTextAlignment(Qt::AlignCenter);
        m_labelTable->setItem(i, 0, idxItem);

        // 色块
        QPixmap chip(14, 14);
        chip.fill(Qt::transparent);
        QPainter cp(&chip);
        cp.setRenderHint(QPainter::Antialiasing);
        cp.setBrush(QColor(m_labelColors.value(i, "#888888")));
        cp.setPen(QPen(Qt::white, 1));
        cp.drawRoundedRect(1, 1, 12, 12, 3, 3);
        cp.end();
        QTableWidgetItem* colorItem = new QTableWidgetItem;
        colorItem->setData(Qt::DecorationRole, chip);
        m_labelTable->setItem(i, 1, colorItem);

        // 名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(m_labelNames[i]);
        if (i == m_activeLabelIdx) {
            nameItem->setBackground(QColor("#3a6fc5"));
            nameItem->setForeground(Qt::white);
        }
        m_labelTable->setItem(i, 2, nameItem);

        // 实例数
        int cnt = m_labelInstanceCounts.value(i, 0);
        m_labelTable->setItem(i, 3, new QTableWidgetItem(
            cnt > 0 ? QString::number(cnt) : "—"));
    }
}

// =============================================================================
// 标注列表
// =============================================================================

void AnnotationManagement::updateShapeList()
{
    m_shapeList->clear();
    for (int i = 0; i < m_shapes.size(); ++i) {
        const auto& s = m_shapes[i];
        QString typeStr;
        switch (s.type) {
        case AnnotationShape::BrushMask:        typeStr = QString::fromUtf8("画笔"); break;
        case AnnotationShape::PolygonShape:      typeStr = QString::fromUtf8("多边形"); break;
        case AnnotationShape::RectShape:         typeStr = QString::fromUtf8("矩形"); break;
        case AnnotationShape::RotatedRectShape:  typeStr = QString::fromUtf8("旋转矩形"); break;
        }
        QString label = m_labelNames.value(s.labelId, QString::fromUtf8("未知"));

        // ── 色块（与标签表格统一样式）──
        QPixmap chip(14, 14);
        chip.fill(Qt::transparent);
        QPainter cp(&chip);
        cp.setRenderHint(QPainter::Antialiasing);
        cp.setBrush(QColor(m_labelColors.value(s.labelId, "#888888")));
        cp.setPen(QPen(Qt::white, 1));
        cp.drawRoundedRect(1, 1, 12, 12, 3, 3);
        cp.end();

        QListWidgetItem* item = new QListWidgetItem(
            QIcon(chip),
            QString::fromUtf8("%1  %2").arg(typeStr, label));
        item->setData(Qt::UserRole, i);
        m_shapeList->addItem(item);
    }
    m_btnDeleteShape->setEnabled(!m_shapeList->selectedItems().isEmpty());
}

void AnnotationManagement::onShapeListContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_shapeList->itemAt(pos);
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_shapes.size()) return;

    QMenu menu(this);
    QAction* visAction = menu.addAction(
        m_shapes[idx].visible ? QString::fromUtf8("隐藏") : QString::fromUtf8("显示"));
    menu.addSeparator();
    menu.addAction(QString::fromUtf8("删除"), this, &AnnotationManagement::onDeleteSelectedShapes);

    QAction* chosen = menu.exec(m_shapeList->mapToGlobal(pos));
    if (chosen == visAction) {
        pushUndoState(QString::fromUtf8("切换可见性"));
        m_shapes[idx].visible = !m_shapes[idx].visible;
        commitUndoState(QString::fromUtf8("切换可见性"));
        m_modified = true; m_lblModified->setText("*");
        renderOverlay(); updateShapeList();
    }
}

void AnnotationManagement::onToggleShapeVisible()
{
    auto sel = m_shapeList->selectedItems();
    if (sel.isEmpty()) return;
    pushUndoState(QString::fromUtf8("切换可见性"));
    for (auto* item : sel) {
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_shapes.size())
            m_shapes[idx].visible = !m_shapes[idx].visible;
    }
    commitUndoState(QString::fromUtf8("切换可见性"));
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
}

void AnnotationManagement::onDeleteSelectedShapes()
{
    QList<QListWidgetItem*> sel = m_shapeList->selectedItems();
    if (sel.isEmpty()) return;
    pushUndoState(QString::fromUtf8("删除标注"));
    QList<int> indices;
    for (auto* item : sel)
        indices.append(item->data(Qt::UserRole).toInt());
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    for (int idx : indices)
        m_shapes.removeAt(idx);
    commitUndoState(QString::fromUtf8("删除标注"));
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
}

// =============================================================================
// 画笔 / 橡皮擦（共享路径引擎）
// =============================================================================

void AnnotationManagement::startBrushStroke(QPointF p)
{
    m_drawing = true;
    m_currentBrushPath = QPainterPath();
    m_currentBrushPath.moveTo(p);
}

void AnnotationManagement::continueBrushStroke(QPointF p)
{
    m_currentBrushPath.lineTo(p);
    QColor c(m_erasing ? QColor(255,0,0,80) : QColor(m_labelColors.value(m_activeLabelIdx,"#FF0000")));
    m_canvas->setShapesPreview(m_currentBrushPath, c, m_brushSize);
}

void AnnotationManagement::endBrushStroke()
{
    m_drawing = false;
    m_canvas->clearPreview();
    if (m_currentBrushPath.isEmpty()) return;

    if (m_erasing) {
        pushUndoState(QString::fromUtf8("橡皮擦"));
        for (auto& s : m_shapes) {
            if (s.type != AnnotationShape::BrushMask) continue;
            QPainter pr(&s.maskLayer);
            pr.setRenderHint(QPainter::Antialiasing);
            pr.setPen(QPen(Qt::black, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            pr.drawPath(m_currentBrushPath);
            pr.end();
        }
        m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
            [](const AnnotationShape& s) {
                if (s.type != AnnotationShape::BrushMask) return false;
                for (int y = 0; y < s.maskLayer.height(); y++) {
                    const uchar* line = s.maskLayer.constScanLine(y);
                    for (int x = 0; x < s.maskLayer.width(); x++)
                        if (line[x] > 0) return false;
                }
                return true;
            }), m_shapes.end());
        commitUndoState(QString::fromUtf8("橡皮擦"));
    } else {
        pushUndoState(QString::fromUtf8("画笔"));
        AnnotationShape shape;
        shape.type = AnnotationShape::BrushMask;
        shape.labelId = m_activeLabelIdx;
        shape.maskLayer = QImage(m_sourceImage.size(), QImage::Format_Grayscale8);
        shape.maskLayer.fill(0);
        QPainter pr(&shape.maskLayer);
        pr.setRenderHint(QPainter::Antialiasing);
        pr.setPen(QPen(Qt::white, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        pr.drawPath(m_currentBrushPath);
        pr.end();
        bool merged = false;
        for (auto& s : m_shapes) {
            if (s.type == AnnotationShape::BrushMask && s.labelId == m_activeLabelIdx) {
                QPainter mp(&s.maskLayer);
                mp.drawImage(0, 0, shape.maskLayer);
                mp.end();
                merged = true;
                break;
            }
        }
        if (!merged) m_shapes.append(shape);
        commitUndoState(QString::fromUtf8("画笔"));
    }
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
}

void AnnotationManagement::startEraser(QPointF p)  { m_erasing = true; startBrushStroke(p); }
void AnnotationManagement::continueEraser(QPointF p) { continueBrushStroke(p); }
void AnnotationManagement::endEraser()
{
    endBrushStroke();
    m_erasing = (m_currentTool == AnnoTool::Eraser);
}

// =============================================================================
// 多边形
// =============================================================================

void AnnotationManagement::startPolygon(QPointF p)
{
    m_drawing = true;
    m_currentPolygon.clear();
    m_currentPolygon << p;
    QPainterPath path;
    path.addEllipse(p, 3, 3);
    m_canvas->setShapesPreview(path, QColor(255, 255, 0), 1);
}

void AnnotationManagement::addPolygonVertex(QPointF p)
{
    // 首点重击闭合（Halcon 习惯：点击首顶点 = 闭合）
    if (m_currentPolygon.size() >= 3) {
        QPointF d = p - m_currentPolygon.first();
        if (d.x()*d.x() + d.y()*d.y() <= 64.0) {  // 8px 内
            closePolygon();
            return;
        }
    }
    m_currentPolygon << p;
    QPainterPath path;
    path.addPolygon(m_currentPolygon);
    path.moveTo(m_currentPolygon.last());
    path.lineTo(p);
    m_canvas->setShapesPreview(path, QColor(100, 100, 255), 1);
}

void AnnotationManagement::closePolygon()
{
    if (m_currentPolygon.size() < 3) { cancelPolygon(); return; }
    pushUndoState(QString::fromUtf8("闭合多边形"));
    m_drawing = false;
    m_canvas->clearPreview();
    AnnotationShape shape;
    shape.type = AnnotationShape::PolygonShape;
    shape.labelId = m_activeLabelIdx;
    shape.polygon = m_currentPolygon;
    m_shapes.append(shape);
    commitUndoState(QString::fromUtf8("闭合多边形"));
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
}

void AnnotationManagement::cancelPolygon()
{
    m_drawing = false;
    m_currentPolygon.clear();
    m_canvas->clearPreview();
}

// =============================================================================
// 矩形 — Halcon 风格：两次点击
// =============================================================================

void AnnotationManagement::startRectangle(QPointF p)
{
    if (!m_rectPlacedStart) {
        // 第一次点击：设定起点
        m_rectStart = p;
        m_rectPlacedStart = true;
        m_drawing = true;
        // 画一个小十字标记起点
        QPainterPath path;
        path.moveTo(p.x() - 5, p.y());
        path.lineTo(p.x() + 5, p.y());
        path.moveTo(p.x(), p.y() - 5);
        path.lineTo(p.x(), p.y() + 5);
        m_canvas->setShapesPreview(path, QColor(100, 255, 100), 1);
    } else {
        // 第二次点击：确认矩形
        endRectangle(p);
    }
}

void AnnotationManagement::updateRectanglePreview(QPointF p)
{
    if (!m_rectPlacedStart) return;
    m_currentRect = QRectF(m_rectStart, p).normalized();
    QPainterPath path;
    path.addRect(m_currentRect);
    m_canvas->setShapesPreview(path, QColor(100, 255, 100), 2);
}

void AnnotationManagement::endRectangle(QPointF p)
{
    m_drawing = false;
    m_rectPlacedStart = false;
    m_canvas->clearPreview();
    m_currentRect = QRectF(m_rectStart, p).normalized();
    if (m_currentRect.width() < 3 || m_currentRect.height() < 3) return;
    pushUndoState(QString::fromUtf8("矩形"));
    AnnotationShape shape;
    shape.type = AnnotationShape::RectShape;
    shape.labelId = m_activeLabelIdx;
    shape.bbox = m_currentRect;
    m_shapes.append(shape);
    commitUndoState(QString::fromUtf8("矩形"));
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
}

void AnnotationManagement::cancelRectangle()
{
    m_drawing = false;
    m_rectPlacedStart = false;
    m_canvas->clearPreview();
}

// =============================================================================
// 填充
// =============================================================================

void AnnotationManagement::startFill(QPointF p)
{
    int sx = (int)p.x(), sy = (int)p.y();
    if (sx < 0 || sy < 0 || sx >= m_sourceImage.width() || sy >= m_sourceImage.height())
        return;
    pushUndoState(QString::fromUtf8("填充区域"));
    floodFillRegion(sx, sy, m_activeLabelIdx);
    commitUndoState(QString::fromUtf8("填充区域"));
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
}

void AnnotationManagement::floodFillRegion(int seedX, int seedY, int fillLabelId)
{
    int w = m_sourceImage.width(), h = m_sourceImage.height();
    QColor seedColor = m_sourceImage.pixelColor(seedX, seedY);
    int sr = seedColor.red(), sg = seedColor.green(), sb = seedColor.blue();

    // 已标注掩码
    QImage occupied(w, h, QImage::Format_Grayscale8);
    occupied.fill(0);
    for (const auto& s : m_shapes) {
        switch (s.type) {
        case AnnotationShape::BrushMask:
            for (int y = 0; y < h; y++) {
                const uchar* src = s.maskLayer.constScanLine(y);
                uchar* dst = occupied.scanLine(y);
                for (int x = 0; x < w; x++)
                    if (src[x] > 0) dst[x] = 255;
            }
            break;
        case AnnotationShape::PolygonShape: {
            QImage pm(w, h, QImage::Format_Grayscale8); pm.fill(0);
            QPainter pp(&pm); pp.setPen(Qt::NoPen); pp.setBrush(Qt::white);
            pp.drawPolygon(s.polygon); pp.end();
            for (int y = 0; y < h; y++) {
                const uchar* src = pm.constScanLine(y);
                uchar* dst = occupied.scanLine(y);
                for (int x = 0; x < w; x++) if (src[x] > 0) dst[x] = 255;
            }
            break;
        }
        case AnnotationShape::RectShape:
        case AnnotationShape::RotatedRectShape: {
            int rx = (int)s.bbox.x(), ry = (int)s.bbox.y();
            int rw = (int)s.bbox.width(), rh = (int)s.bbox.height();
            for (int y = qMax(0, ry); y < qMin(h, ry+rh); y++) {
                uchar* dst = occupied.scanLine(y);
                for (int x = qMax(0, rx); x < qMin(w, rx+rw); x++) dst[x] = 255;
            }
            break;
        }
        }
    }

    QImage fillMask(w, h, QImage::Format_Grayscale8);
    fillMask.fill(0);
    const int tolerance = 32;
    int tolSq = tolerance * tolerance * 3;
    QVector<bool> visited(w * h, false);
    QStack<QPair<int,int>> stack;
    stack.push({seedX, seedY});

    while (!stack.isEmpty()) {
        auto [x, y] = stack.pop();
        int left = x;
        while (left > 0) {
            int idx = (left-1) + y*w;
            if (visited[idx] || occupied.scanLine(y)[left-1] != 0) break;
            QColor c = m_sourceImage.pixelColor(left-1, y);
            int dr = c.red()-sr, dg = c.green()-sg, db = c.blue()-sb;
            if (dr*dr+dg*dg+db*db > tolSq) break;
            left--;
        }
        int right = x;
        while (right < w-1) {
            int idx = (right+1) + y*w;
            if (visited[idx] || occupied.scanLine(y)[right+1] != 0) break;
            QColor c = m_sourceImage.pixelColor(right+1, y);
            int dr = c.red()-sr, dg = c.green()-sg, db = c.blue()-sb;
            if (dr*dr+dg*dg+db*db > tolSq) break;
            right++;
        }
        uchar* fl = fillMask.scanLine(y);
        for (int fx = left; fx <= right; fx++) {
            fl[fx] = 255;
            visited[fx + y*w] = true;
        }
        for (int ny = y-1; ny <= y+1; ny += 2) {
            if (ny < 0 || ny >= h) continue;
            for (int nx = left; nx <= right; nx++) {
                int nidx = nx + ny*w;
                if (visited[nidx] || occupied.scanLine(ny)[nx] != 0) continue;
                QColor c = m_sourceImage.pixelColor(nx, ny);
                int dr = c.red()-sr, dg = c.green()-sg, db = c.blue()-sb;
                if (dr*dr+dg*dg+db*db <= tolSq) { stack.push({nx, ny}); break; }
            }
        }
    }

    AnnotationShape shape;
    shape.type = AnnotationShape::BrushMask;
    shape.labelId = fillLabelId;
    shape.maskLayer = fillMask;
    bool merged = false;
    for (auto& s : m_shapes) {
        if (s.type == AnnotationShape::BrushMask && s.labelId == fillLabelId) {
            QPainter mp(&s.maskLayer); mp.drawImage(0, 0, fillMask); mp.end();
            merged = true; break;
        }
    }
    if (!merged) m_shapes.append(shape);
}

// =============================================================================
// 画布事件分发
// =============================================================================

void AnnotationManagement::onCanvasMousePressed(QPointF p, Qt::MouseButton btn)
{
    if (btn == Qt::RightButton) {
        // v3.0: 右键橡皮擦 (画笔模式下临时切换)
        if (m_activeTool && m_activeTool->type() == AnnoTool::Brush) {
            AnnotationTool* eraserTool = m_toolsMap.value(AnnoTool::Eraser);
            if (eraserTool) {
                AnnotationTool::Context ctx;
                ctx.sourceImage = m_sourceImage;
                ctx.model = m_model;
                ctx.render = m_render;
                ctx.labelColors = m_labelColors;
                ctx.activeLabelId = m_activeLabelIdx;
                ctx.brushSize = m_brushSize;
                eraserTool->updateContext(ctx);
                eraserTool->mousePress(p, btn);
                m_erasing = true;
                return;
            }
        }
        if (m_activeTool && m_activeTool->isDrawing())
            m_activeTool->mousePress(p, btn);
        return;
    }
    if (btn != Qt::LeftButton) return;

    // v3.0: 委托给活动工具
    if (m_activeTool) {
        // 更新工具上下文
        AnnotationTool::Context ctx;
        ctx.sourceImage = m_sourceImage;
        ctx.model = m_model;
        ctx.render = m_render;
        ctx.labelColors = m_labelColors;
        ctx.activeLabelId = m_activeLabelIdx;
        ctx.brushSize = m_brushSize;
        m_activeTool->updateContext(ctx);

        // Select 工具需要 undo 事务
        if (m_activeTool->type() == AnnoTool::Select) {
            auto* selTool = qobject_cast<SelectTool*>(m_activeTool);
            if (selTool) {
                int si = m_model->hitTestShape(p);
                if (si >= 0) {
                    int vi = m_model->hitTestVertex(si, p);
                    QString desc = vi >= 0 ? QString::fromUtf8("拖拽顶点")
                                           : QString::fromUtf8("移动标注");
                    m_activeTransaction = new AnnotationModel::Transaction(
                        m_model, desc);
                    m_editShapeIndex = si;
                    m_editVertexIndex = vi;
                }
            }
        }

        m_activeTool->mousePress(p, btn);
    }
}

void AnnotationManagement::onCanvasMouseMoved(QPointF p)
{
    m_lblPosition->setText(QString::fromUtf8("(%1, %2)").arg((int)p.x()).arg((int)p.y()));

    // v3.0: 右键橡皮擦拖拽
    if (m_erasing) {
        AnnotationTool* eraserTool = m_toolsMap.value(AnnoTool::Eraser);
        if (eraserTool && eraserTool->isDrawing()) {
            eraserTool->mouseMove(p);
            return;
        }
    }

    // v3.0: 委托给活动工具
    if (m_activeTool) {
        m_activeTool->mouseMove(p);
    }
}

void AnnotationManagement::onCanvasMouseReleased(QPointF p, Qt::MouseButton btn)
{
    // v3.0: 右键橡皮擦释放
    if (m_erasing && btn == Qt::RightButton) {
        AnnotationTool* eraserTool = m_toolsMap.value(AnnoTool::Eraser);
        if (eraserTool) {
            eraserTool->mouseRelease(p, Qt::LeftButton);  // 触发 endErase
        }
        m_erasing = false;
        return;
    }

    if (btn != Qt::LeftButton) return;

    // v3.0: 提交 Select 事务
    if (m_activeTransaction) {
        m_activeTransaction->commit();
        delete m_activeTransaction;
        m_activeTransaction = nullptr;
        m_editShapeIndex = -1;
        m_editVertexIndex = -1;
        updateShapeList();
    }

    // v3.0: 委托给活动工具
    if (m_activeTool) {
        m_activeTool->mouseRelease(p, btn);
    }
}

void AnnotationManagement::onCanvasDoubleClicked(QPointF p)
{
    // v3.0: 委托给活动工具
    if (m_activeTool) {
        m_activeTool->mouseDoubleClick(p);
    }
}

// =============================================================================
// 选择/编辑
// =============================================================================

int AnnotationManagement::hitTestShape(QPointF p) const
{
    for (int i = m_shapes.size()-1; i >= 0; --i) {
        const auto& s = m_shapes[i];
        if (!s.visible) continue;
        switch (s.type) {
        case AnnotationShape::BrushMask:
            if (p.x() >= 0 && p.y() >= 0 && p.x() < s.maskLayer.width() && p.y() < s.maskLayer.height())
                if (s.maskLayer.scanLine((int)p.y())[(int)p.x()] > 0) return i;
            break;
        case AnnotationShape::PolygonShape:
            if (s.polygon.containsPoint(p, Qt::OddEvenFill)) return i;
            break;
        case AnnotationShape::RectShape:
        case AnnotationShape::RotatedRectShape:
            if (s.bbox.contains(p)) return i;
            break;
        }
    }
    return -1;
}

int AnnotationManagement::hitTestVertex(int si, QPointF p) const
{
    if (si < 0 || si >= m_shapes.size()) return -1;
    const auto& s = m_shapes[si];
    const double r2 = 64.0;  // 8px radius squared
    switch (s.type) {
    case AnnotationShape::PolygonShape:
        for (int i = 0; i < s.polygon.size(); ++i) {
            QPointF d = p - s.polygon[i];
            if (d.x()*d.x()+d.y()*d.y() <= r2) return i;
        }
        break;
    case AnnotationShape::RectShape:
    case AnnotationShape::RotatedRectShape: {
        QPointF corners[4] = { s.bbox.topLeft(), s.bbox.topRight(),
            s.bbox.bottomRight(), s.bbox.bottomLeft() };
        for (int i = 0; i < 4; ++i) {
            QPointF d = p - corners[i];
            if (d.x()*d.x()+d.y()*d.y() <= r2) return i;
        }
        break;
    }
    default: break;
    }
    return -1;
}

void AnnotationManagement::startShapeDrag(int si, QPointF p)
{
    m_editShapeIndex = si;
    m_editVertexIndex = -1;
    m_editStartPos = p;
    pushUndoState(QString::fromUtf8("移动标注"));
}

void AnnotationManagement::continueShapeDrag(QPointF p)
{
    if (m_editShapeIndex < 0 || m_editShapeIndex >= m_shapes.size()) return;
    QPointF delta = p - m_editStartPos;
    auto& s = m_shapes[m_editShapeIndex];
    switch (s.type) {
    case AnnotationShape::PolygonShape: s.polygon.translate(delta); break;
    case AnnotationShape::RectShape:
    case AnnotationShape::RotatedRectShape: s.bbox.translate(delta); break;
    case AnnotationShape::BrushMask: {
        QImage shifted(s.maskLayer.size(), QImage::Format_Grayscale8);
        shifted.fill(0);
        QPainter sp(&shifted); sp.drawImage(delta, s.maskLayer); sp.end();
        s.maskLayer = shifted;
        break;
    }
    }
    m_editStartPos = p;
    m_modified = true; m_lblModified->setText("*");
    renderOverlay();
}

void AnnotationManagement::endShapeDrag()
{
    commitUndoState(QString::fromUtf8("移动标注"));
}

// =============================================================================
// 渲染
// =============================================================================

void AnnotationManagement::renderOverlay()
{
    if (m_sourceImage.isNull() || !m_render) return;

    bool showHandles = (m_currentTool == AnnoTool::Select ||
                        m_editShapeIndex >= 0);
    QImage overlay = m_render->renderOverlay(
        m_shapes, showHandles, m_editShapeIndex);
    m_canvas->setOverlay(overlay);
}

QImage AnnotationManagement::compileMask() const
{
    if (m_sourceImage.isNull() || !m_render) return {};

    // v3.0: 委托给 AnnotationRender (支持 labelOffset)
    int offset = m_dataset ? m_dataset->currentLabelOffset() : 1;
    return m_render->compileMask(m_shapes, offset);
}

// =============================================================================
// 保存 / 加载
// =============================================================================

void AnnotationManagement::saveAnnotations()
{
    if (m_currentImagePath.isEmpty() || !m_dataset) return;
    QString base = QFileInfo(m_currentImagePath).completeBaseName();
    QString annDir = m_dataset->annotationsDir();
    QDir().mkpath(annDir);

    if (LabelTypes::isMaskType(m_labelType)) {
        // v3.0: 使用 AnnotationRender::compileMask (支持 labelOffset)
        int offset = m_dataset ? m_dataset->currentLabelOffset() : 1;
        QImage mask = m_render->compileMask(m_shapes, offset);
        if (m_labelType == LabelTypes::BinaryMask) {
            for (int y = 0; y < mask.height(); y++) {
                uchar* line = mask.scanLine(y);
                for (int x = 0; x < mask.width(); x++)
                    if (line[x] > 0) line[x] = 255;
            }
        }
        mask.save(annDir + "/" + base + ".png", "PNG");
    } else {
        QJsonArray annotations;
        int annId = 1;
        for (const auto& s : m_shapes) {
            if (!s.visible) continue;
            QJsonObject a;
            a["id"] = annId++;
            a["image_id"] = m_currentImageIdx + 1;
            // v3.0: 使用 labelOffset 替代硬编码 +1
            a["category_id"] = s.labelId + (m_dataset ? m_dataset->currentLabelOffset() : 1);
            if (s.type == AnnotationShape::RectShape) {
                QJsonArray bb; bb.append(s.bbox.x()); bb.append(s.bbox.y());
                bb.append(s.bbox.width()); bb.append(s.bbox.height());
                a["bbox"] = bb;
            } else if (s.type == AnnotationShape::RotatedRectShape) {
                QJsonArray bb; bb.append(s.bbox.x()); bb.append(s.bbox.y());
                bb.append(s.bbox.width()); bb.append(s.bbox.height());
                a["bbox"] = bb; a["rotation"] = s.rotation;
                QJsonArray rc; rc.append(s.rotCenter.x()); rc.append(s.rotCenter.y());
                a["rot_center"] = rc;
            } else if (s.type == AnnotationShape::PolygonShape) {
                QRectF br = s.polygon.boundingRect();
                QJsonArray bb; bb.append(br.x()); bb.append(br.y());
                bb.append(br.width()); bb.append(br.height());
                a["bbox"] = bb;
                QJsonArray seg;
                for (const QPointF& pt : s.polygon) { seg.append(pt.x()); seg.append(pt.y()); }
                a["segmentation"] = seg;
            }
            annotations.append(a);
        }
        QJsonObject root;
        QJsonObject imgObj;
        imgObj["id"] = m_currentImageIdx + 1;
        imgObj["file_name"] = QFileInfo(m_currentImagePath).fileName();
        root["images"] = QJsonArray{ imgObj };
        root["annotations"] = annotations;
        QFile f(annDir + "/" + base + ".json");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            f.close();
        }
    }

    QList<int> cids;
    for (const auto& s : m_shapes)
        if (s.visible && !cids.contains(s.labelId)) cids.append(s.labelId);
    m_dataset->onAnnotationUpdated(m_currentImagePath, cids, m_shapes.size());
    m_modified = false;
    m_lblModified->setText("");
}

void AnnotationManagement::loadExistingAnnotations()
{
    if (!m_dataset) return;
    QString base = QFileInfo(m_currentImagePath).completeBaseName();
    QString annDir = m_dataset->annotationsDir();

    QString pngPath = annDir + "/" + base + ".png";
    QImage mask(pngPath);
    if (!mask.isNull()) {
        // v3.0: 使用 DatasetManagement 提供的 labelOffset 代替硬编码 -1
        int offset = m_dataset ? m_dataset->currentLabelOffset() : 1;
        QImage gray = mask.convertToFormat(QImage::Format_Grayscale8);
        QMap<int, QImage> layers;
        for (int y = 0; y < gray.height(); y++) {
            const uchar* line = gray.constScanLine(y);
            for (int x = 0; x < gray.width(); x++) {
                if (line[x] > 0) {
                    int lid = line[x] - offset;
                    if (!layers.contains(lid)) {
                        layers[lid] = QImage(m_sourceImage.size(), QImage::Format_Grayscale8);
                        layers[lid].fill(0);
                    }
                    layers[lid].setPixel(x, y, 255);
                }
            }
        }
        for (auto it = layers.begin(); it != layers.end(); ++it) {
            AnnotationShape s;
            s.type = AnnotationShape::BrushMask;
            s.labelId = it.key();
            s.maskLayer = it.value();
            m_shapes.append(s);
        }
    }

    QString jsonPath = annDir + "/" + base + ".json";
    QFile jf(jsonPath);
    if (jf.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());
        jf.close();
        for (const QJsonValue& av : doc.object()["annotations"].toArray()) {
            QJsonObject a = av.toObject();
            AnnotationShape s;
            // v3.0: 使用 labelOffset 替代硬编码 -1
            int offset = m_dataset ? m_dataset->currentLabelOffset() : 1;
            s.labelId = a["category_id"].toInt() - offset;
            if (a.contains("segmentation")) {
                s.type = AnnotationShape::PolygonShape;
                QJsonArray seg = a["segmentation"].toArray();
                for (int i = 0; i + 1 < seg.size(); i += 2)
                    s.polygon << QPointF(seg[i].toDouble(), seg[i+1].toDouble());
            } else if (a.contains("bbox")) {
                QJsonArray bb = a["bbox"].toArray();
                if (a.contains("rotation")) {
                    s.type = AnnotationShape::RotatedRectShape;
                    s.rotation = a["rotation"].toDouble();
                    QJsonArray rc = a["rot_center"].toArray();
                    if (rc.size() >= 2) s.rotCenter = QPointF(rc[0].toDouble(), rc[1].toDouble());
                } else {
                    s.type = AnnotationShape::RectShape;
                }
                s.bbox = QRectF(bb[0].toDouble(), bb[1].toDouble(),
                                bb[2].toDouble(), bb[3].toDouble());
            }
            m_shapes.append(s);
        }
    }
}

// =============================================================================
// Undo
// =============================================================================

void AnnotationManagement::pushUndoState(const QString&)
{
    if (m_undoInProgress) return;
    m_undoSnapshot = m_shapes;
}

void AnnotationManagement::commitUndoState(const QString& desc)
{
    if (m_undoInProgress) return;
    if (m_undoSnapshot.isEmpty() && m_shapes.isEmpty()) return;
    m_undoStack->push(new AnnoUndoCommandLegacy(m_undoSnapshot, m_shapes, this, desc));
    m_undoSnapshot.clear();
    updateUndoRedoButtons();
}

void AnnotationManagement::restoreShapes(const QList<AnnotationShape>& shapes)
{
    m_undoInProgress = true;
    m_shapes = shapes;
    m_modified = true; m_lblModified->setText("*");
    renderOverlay(); updateShapeList();
    updateUndoRedoButtons();
    m_undoInProgress = false;
}

void AnnotationManagement::rebuildFromShapes()
{
    renderOverlay(); updateShapeList(); updateUndoRedoButtons();
}

void AnnotationManagement::updateUndoRedoButtons()
{
    m_btnUndo->setEnabled(m_undoStack->canUndo());
    m_btnRedo->setEnabled(m_undoStack->canRedo());
}

void AnnotationManagement::onSave()
{
    saveAnnotations();
    Logger::info(QString::fromUtf8("标注已保存: %1").arg(m_currentImagePath));
}

void AnnotationManagement::onUndo()
{
    if (m_undoStack->canUndo()) { m_undoStack->undo(); updateUndoRedoButtons(); }
}

void AnnotationManagement::onRedo()
{
    if (m_undoStack->canRedo()) { m_undoStack->redo(); updateUndoRedoButtons(); }
}

// =============================================================================
// 主题
// =============================================================================

void AnnotationManagement::applyTheme()
{
    const auto& p = ThemeManager::instance().palette();

    setStyleSheet(QString("AnnotationManagement { background-color: %1; }").arg(p.bgPrimary));

    // 顶栏
    QWidget* tb = findChild<QWidget*>("annoTopBar");
    if (tb) tb->setStyleSheet(QString("#annoTopBar { background-color: %1; border-bottom: 1px solid %2; }")
        .arg(p.bgSecondary, p.borderMedium));

    // 分隔线
    for (QFrame* sep : findChildren<QFrame*>("annoVSep"))
        sep->setStyleSheet(QString("#annoVSep { color: %1; }").arg(p.borderMedium));

    // 左面板
    QWidget* lp = findChild<QWidget*>("annoLeftPanel");
    if (lp) lp->setStyleSheet(QString("#annoLeftPanel { background-color: %1; }").arg(p.bgSecondary));

    // 工具侧边栏
    QWidget* tp = findChild<QWidget*>("annoToolPanel");
    if (tp) tp->setStyleSheet(QString("#annoToolPanel { background-color: %1; }").arg(p.bgTertiary));

    // 右面板
    QWidget* rp = findChild<QWidget*>("annoRightPanel");
    if (rp) rp->setStyleSheet(QString("#annoRightPanel { background-color: %1; }").arg(p.bgSecondary));

    // 底部
    QWidget* bb = findChild<QWidget*>("annoBottomBar");
    if (bb) bb->setStyleSheet(QString("#annoBottomBar { background-color: %1; }").arg(p.bgSecondary));

    // 导航条
    QWidget* nb = findChild<QWidget*>("annoNavBar");
    if (nb) nb->setStyleSheet(QString("#annoNavBar { background-color: %1; border-radius: 3px; }").arg(p.bgTertiary));

    // 胶片索引标签
    if (m_filmIndexLabel)
        m_filmIndexLabel->setStyleSheet(QString(
            "QLabel#annoFilmIndex { color: %1; font-size: 11px; font-weight: bold; }")
            .arg(p.textPrimary));

    // ── 主题切换时刷新图标 ──
    bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Dark;
    auto icon = [&](const QString& name) {
        return QIcon(isDark ? QString(":/res/%1_D.png").arg(name)
                            : QString(":/res/%1.png").arg(name));
    };

    // 工具侧边栏按钮
    for (auto* btn : m_toolGroup->buttons()) {
        int id = m_toolGroup->id(btn);
        QString name;
        switch (id) {
        case AnnoTool::Brush:     name = "AnnoBrush";     break;
        case AnnoTool::Eraser:    name = "AnnoEraser";    break;
        case AnnoTool::Polygon:   name = "AnnoPolygon";   break;
        case AnnoTool::Rectangle: name = "AnnoRectangle"; break;
        case AnnoTool::Fill:      name = "AnnoFill";      break;
        case AnnoTool::Select:    name = "AnnoSelect";    break;
        default: continue;
        }
        btn->setIcon(icon(name));
    }

    // 顶栏操作按钮
    if (m_btnSave)  m_btnSave->setIcon(icon("AnnoSave"));
    if (m_btnUndo)  m_btnUndo->setIcon(icon("AnnoUndo"));
    if (m_btnRedo)  m_btnRedo->setIcon(icon("AnnoRedo"));
    if (m_btnFit)   m_btnFit->setIcon(icon("AnnoFit"));
    if (m_btnZoom1) m_btnZoom1->setIcon(icon("AnnoZoom1"));


    // 导航按钮
    if (m_btnFilmFirst) m_btnFilmFirst->setIcon(icon("AnnoNavFirst"));
    if (m_btnFilmPrev)  m_btnFilmPrev->setIcon(icon("AnnoNavPrev"));
    if (m_btnFilmNext)  m_btnFilmNext->setIcon(icon("AnnoNavNext"));
    if (m_btnFilmLast)  m_btnFilmLast->setIcon(icon("AnnoNavLast"));

    // ── 保存按钮 ──
    if (m_btnSave)
        m_btnSave->setStyleSheet(QString(
            "QPushButton#annoSaveBtn { background: transparent; color: %1; border: none;"
            " border-radius: 3px; }"
            "QPushButton#annoSaveBtn:hover { background: %2; }")
            .arg(p.accentPrimary, p.bgHover));

    // ── 顶栏图标按钮 ──
    for (QPushButton* btn : findChildren<QPushButton*>("annoTopBtn")) {
        btn->setStyleSheet(QString(
            "QPushButton#annoTopBtn { background: transparent; color: %1; border: none;"
            " border-radius: 3px; font-size: 13px; }"
            "QPushButton#annoTopBtn:hover { background: %2; }"
            "QPushButton#annoTopBtn:disabled { color: %3; }")
            .arg(p.textPrimary, p.bgHover, p.textMuted));
    }

    // ── 缩放按钮 ──
    auto zoomBtnStyle = [&](QPushButton* btn) {
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
            " border-radius: 4px; font-weight: bold; font-size: 14px;"
            " min-width: 28px; min-height: 28px; padding: 0px; }"
            "QPushButton:hover { background: %3; }")
            .arg(p.textPrimary, p.borderMedium, p.bgHover));
    };
    if (m_btnZoomOut) zoomBtnStyle(m_btnZoomOut);
    if (m_btnZoomIn)  zoomBtnStyle(m_btnZoomIn);

    // ── LabelMe 按钮 ──
    if (m_btnLabelMe)
        m_btnLabelMe->setStyleSheet(QString(
            "QPushButton#annoLabelMeBtn { background: %1; color: #fff; border: none;"
            " border-radius: 4px; padding: 4px 18px; font-size: 13px; font-weight: bold; }"
            "QPushButton#annoLabelMeBtn:hover { background: %2; }")
            .arg(p.accentPrimary, p.accentHover));

    // ── 返回按钮（文字）──
    if (m_btnBack)
        m_btnBack->setStyleSheet(QString(
            "QPushButton#annoBackBtn { background: transparent; color: %1; border: 1px solid %2;"
            " border-radius: 4px; padding: 4px 12px; font-size: 13px; }"
            "QPushButton#annoBackBtn:hover { background: %3; }")
            .arg(p.textPrimary, p.borderMedium, p.bgHover));

    // 底部工具按钮
    for (QPushButton* btn : findChildren<QPushButton*>("annoToolBtn")) {
        btn->setStyleSheet(QString(
            "QPushButton#annoToolBtn { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 2px; font-size: 14px; }"
            "QPushButton#annoToolBtn:checked { background: %4; color: #fff; border-color: %4; }"
            "QPushButton#annoToolBtn:hover { border-color: %4; }")
            .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.accentPrimary));
    }

    // 删除按钮
    if (m_btnDeleteShape)
        m_btnDeleteShape->setStyleSheet(QString(
            "QPushButton#annoDeleteBtn { background: transparent; color: %1; border: 1px solid %1;"
            " border-radius: 3px; padding: 2px 8px; }"
            "QPushButton#annoDeleteBtn:hover { background: %1; color: #fff; }"
            "QPushButton#annoDeleteBtn:disabled { color: %2; border-color: %2; }")
            .arg(p.danger, p.textMuted));

    // 导航按钮
    for (QPushButton* btn : findChildren<QPushButton*>("annoFilmBtn")) {
        btn->setStyleSheet(QString(
            "QPushButton#annoFilmBtn { background: %1; color: %2; border: none;"
            " border-radius: 2px; }"
            "QPushButton#annoFilmBtn:hover { background: %3; }"
            "QPushButton#annoFilmBtn:disabled { color: %4; }")
            .arg(p.bgSecondary, p.textSecondary, p.bgHover, p.textMuted));
    }

    // 标签表格
    if (m_labelTable)
        m_labelTable->setStyleSheet(QString(
            "QTableWidget#annoLabelTable { background: %1; color: %2; border: 1px solid %3;"
            " gridline-color: %3; }"
            "QTableWidget#annoLabelTable::item { padding: 2px 4px; }"
            "QHeaderView::section { background: %4; color: %2; border: none; padding: 2px 4px; }")
            .arg(p.bgPrimary, p.textPrimary, p.borderMedium, p.bgSecondary));

    // 标注列表
    if (m_shapeList)
        m_shapeList->setStyleSheet(QString(
            "QListWidget#annoShapeList { background: %1; color: %2; border: 1px solid %3; }"
            "QListWidget#annoShapeList::item { padding: 2px 4px; }"
            "QListWidget#annoShapeList::item:selected { background: %4; color: #fff; }")
            .arg(p.bgPrimary, p.textPrimary, p.borderMedium, p.accentPrimary));

    // 标题
    for (QLabel* lbl : findChildren<QLabel*>("annoSectTitle"))
        lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; padding-top: 6px;")
            .arg(p.textSecondary));

    // 属性标签
    for (QLabel* lbl : findChildren<QLabel*>("annoPropLabel"))
        lbl->setStyleSheet(QString("color: %1; font-size: 11px;").arg(p.textSecondary));

    // 未保存指示器
    if (m_lblModified)
        m_lblModified->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;")
            .arg(p.accentPrimary));

    // 顶栏信息
    if (m_lblImageInfo)
        m_lblImageInfo->setStyleSheet(QString("color: %1; font-weight: bold;").arg(p.textPrimary));

    // 状态栏标签
    for (QLabel* lbl : findChildren<QLabel*>("annoStatusLabel"))
        lbl->setStyleSheet(QString("color: %1; font-size: 11px;").arg(p.textSecondary));
}
