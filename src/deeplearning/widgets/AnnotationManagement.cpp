#include "ui/WriteAccess.h"
#include "AnnotationManagement.h"
#include "AnnotationCanvas.h"
#include "deeplearning/model/AnnotationModel.h"
#include "deeplearning/render/AnnotationRender.h"
#include "deeplearning/storage/AnnotationStore.h"
#include "deeplearning/storage/DatasetFileService.h"
#include <QMessageBox>
#include "deeplearning/tools/AnnotationTool.h"
#include "deeplearning/tools/BrushTool.h"
#include "deeplearning/tools/EraserTool.h"
#include "deeplearning/tools/PolygonTool.h"
#include "deeplearning/tools/RectTool.h"
#include "deeplearning/tools/FillTool.h"
#include "deeplearning/tools/SelectTool.h"
#include "DatasetManagement.h"
#include "core/Logger.h"
#include "ui/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QFileInfo>
#include <QDir>
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
#include <QLabel>
#include <QSignalBlocker>
#include <QShortcut>
#include <QTransform>
// AnnotationUndoCommand
// AnnoUndoCommandLegacy (Legacy — P4 迁移到 AnnotationModel::Transaction)



AnnotationManagement::AnnotationManagement(QWidget* parent)
    : QWidget(parent)
{
    initUI();
    for(auto* button:findChildren<QPushButton*>()) button->setFocusPolicy(Qt::NoFocus);
    auto shortcut=[this](int key,Qt::KeyboardModifiers mods=Qt::NoModifier) {
        auto* action=new QShortcut(QKeySequence(QKeyCombination(mods,Qt::Key(key))),this);
        action->setContext(Qt::WidgetWithChildrenShortcut);
        action->setAutoRepeat(false);
        connect(action,&QShortcut::activated,this,[this,key,mods](){onCanvasKeyPressed(key,mods);});
    };
    for(int key:{Qt::Key_S,Qt::Key_Z,Qt::Key_Y,Qt::Key_C,Qt::Key_D,Qt::Key_V,Qt::Key_0,Qt::Key_1,Qt::Key_Return,Qt::Key_Enter}) shortcut(key,Qt::ControlModifier);
    for(int key:{Qt::Key_B,Qt::Key_E,Qt::Key_P,Qt::Key_R,Qt::Key_F,Qt::Key_S,Qt::Key_V,Qt::Key_H,Qt::Key_Delete,Qt::Key_PageUp,Qt::Key_PageDown,Qt::Key_Escape,Qt::Key_BracketLeft,Qt::Key_BracketRight}) shortcut(key);
    AccessControl::watch(this,[this]() { applyPermissions(); });
    applyPermissions();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &AnnotationManagement::applyTheme);
}

AnnotationManagement::~AnnotationManagement()
{
    // QUndoStack emits indexChanged while being destroyed, after sibling widgets may be gone.
    m_autoSaveTimer->stop();
    disconnect(m_model, nullptr, this, nullptr);
    disconnect(m_model->undoStack(), nullptr, this, nullptr);
    cancelPendingTools();
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
    // Category indices are stable; renaming must never remap by name.
    m_labelNames = m_dataset->currentLabelNames();
    m_labelColors = m_dataset->currentLabelColors();
    m_labelInstanceCounts = m_dataset->currentLabelInstanceCounts();
    m_labelType = m_dataset->currentLabelType();
    m_samples = m_dataset->allSamples();
    m_render->setLabelColors(m_labelColors);
    m_activeLabelIdx = qBound(0, m_activeLabelIdx, qMax(0, int(m_labelNames.size())-1));
    updateLabelTable(); updateShapeList(); renderOverlay();
}


bool AnnotationManagement::openImage(QString modelId, QString imagePath)
{
    if(!AccessControl::canRead("深度学习.数据标注")) return false;
    if (!m_dataset || modelId != m_dataset->currentModelId() || m_dataset->isClassificationDataset()) return false;
    // Validate the destination before asking to discard the current image.
    QImage image(imagePath);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("打开失败"), tr("无法加载图像：%1").arg(imagePath));
        return false;
    }
    const auto names = m_dataset->currentLabelNames();
    const auto labelType = m_dataset->currentLabelType();
    auto path = m_dataset->annotationsDir()+"/"+QFileInfo(imagePath).completeBaseName()
        +(LabelTypes::isMaskType(labelType)?".png":".json");
    if(!QFileInfo::exists(path)) for(const auto& suffix:{".png",".json",".txt"}) {
        const auto candidate=m_dataset->annotationsDir()+"/"+QFileInfo(imagePath).completeBaseName()+suffix;
        if(QFileInfo::exists(candidate)) {path=candidate;break;}
    }
    QList<AnnotationShape> shapes; QString error;
    if (!prepareToLeave()) return false;
    if (!DatasetFileService::readShapes(path,image.size(),names,labelType==LabelTypes::BinaryMask,shapes,error)) {
        QMessageBox::warning(this,tr("标注加载失败"),path+"\n"+error);
        return false;
    }
    cancelPendingTools();
    m_labelNames=names; m_labelColors=m_dataset->currentLabelColors();
    m_labelType=labelType; m_labelInstanceCounts=m_dataset->currentLabelInstanceCounts();
    const bool keepView=m_btnKeepView->isChecked() && modelId==m_currentModelId && m_canvas->hasImage();
    m_samples=m_dataset->allSamples(); m_currentModelId=modelId;
    m_currentImagePath=imagePath; m_loadedAnnotationPath=path; m_sourceImage=image;
    m_currentImageIdx=m_dataset->findSampleIndex(imagePath);
    applyImageAdjustments();
    m_render->setSourceImage(image); m_render->setLabelColors(m_labelColors);
    m_render->setLabelOffset(m_dataset->currentLabelOffset());
    m_model->reset(shapes);
    if (!keepView) m_canvas->fitToWindow();
    updateNavigationInfo(); updateLabelTable(); updateToolUI(); updateNavigator(); updateFilmIndexLabel();
    m_autoSaveTimer->stop(); applyPermissions();
    return true;
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
    m_canvas->setBrushDiameter(m_brushSize);
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


    // ═══ v3.0 数据模型 + 渲染管线 ═══
    m_model = new AnnotationModel(this);
    m_render = new AnnotationRender(this);

    connect(m_model, &AnnotationModel::dataChanged, this, [this]() {
        renderOverlay(); if(!m_activeTransaction) updateShapeList(); updateUndoRedoButtons();
        if (m_autoSaveTimer && m_model->isDirty()) m_autoSaveTimer->start();
    });
    connect(m_model, &AnnotationModel::dirtyStateChanged, this, [this](bool dirty) {
        m_lblModified->setText(dirty ? "*" : "");
    });
    connect(m_model->undoStack(), &QUndoStack::indexChanged, this, [this]() { updateUndoRedoButtons(); });

    // ═══ v3.0 工具插件 ═══
    auto registerTool = [this](AnnotationTool* tool) {
        m_toolsMap[tool->type()] = tool;

        connect(tool, &AnnotationTool::requestPreviewUpdate, this, [this,tool]() {
            if (m_canvas && (tool==m_activeTool || (m_erasing && tool->type()==AnnoTool::Eraser))) {
                QPainterPath path = tool->previewPath();
                if (path.isEmpty())
                    m_canvas->clearPreview();
                else
                    m_canvas->setShapesPreview(path,
                        tool->previewColor(),
                        tool->previewPenWidth());
            }
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

    connect(m_canvas,&AnnotationCanvas::gestureCancelled,this,[this]() {
        if(m_activeTransaction || m_erasing || (m_activeTool && m_activeTool->showBrushSize() && m_activeTool->isDrawing())) {
            cancelPendingTools();updateShapeList();renderOverlay();
        }
    });
    connect(m_shapeList,&QListWidget::itemSelectionChanged,this,[this]() {
        const auto selected=m_shapeList->selectedItems();
        m_editShapeIndex=selected.isEmpty()?-1:selected.first()->data(Qt::UserRole).toInt();
        m_btnDeleteShape->setEnabled(AccessControl::canWrite("深度学习.数据标注") && !selected.isEmpty());
        updateSelectionHandles();
    });

    // ═══ v3.0 自动保存 (30秒无操作) ═══
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(30000);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, [this]() {
        if (AccessControl::canWrite("深度学习.数据标注") && m_model->isDirty() && !hasPendingDrawing() && !m_currentImagePath.isEmpty() && saveAnnotations()) {
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

    m_btnSaveNext = mkBtn(tr("保存并下一张"), "annoSaveNextBtn");
    m_btnSaveNext->setToolTip(tr("保存成功后打开下一张；最后一张保存后停留。Ctrl+Enter"));
    connect(m_btnSaveNext,&QPushButton::clicked,this,&AnnotationManagement::onSaveAndNext);
    layout->addWidget(m_btnSaveNext);
    m_btnKeepView = mkBtn(tr("保持视野"), "annoKeepViewBtn");
    m_btnKeepView->setCheckable(true);m_btnKeepView->setChecked(true);
    m_btnKeepView->setToolTip(tr("同一模型切图时保持缩放和相对位置；Ctrl+0 适应当前图片。"));
    layout->addWidget(m_btnKeepView);
    m_btnOverlay = mkBtn(tr("标注可见"), "annoOverlayBtn");
    m_btnOverlay->setCheckable(true);m_btnOverlay->setChecked(true);
    m_btnOverlay->setToolTip(tr("显示/隐藏叠加层以检查原图，不修改标注。H"));
    connect(m_btnOverlay,&QPushButton::toggled,this,[this](bool visible){m_canvas->setOverlayVisible(visible);});
    layout->addWidget(m_btnOverlay);

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
        { AnnoTool::Eraser,    "AnnoEraser",    QString::fromUtf8("橡皮 (E)\n擦除画笔/填充掩码\n画笔下右键临时橡皮") },
        { AnnoTool::Polygon,   "AnnoPolygon",   QString::fromUtf8("多边形 (P)\n点击顶点 | 双击闭合\nEnter闭合 | Back退格 | Esc取消") },
        { AnnoTool::Rectangle, "AnnoRectangle", QString::fromUtf8("矩形 (R)\n拖放或先点起点，再点终点\nEnter确认 | Esc取消") },
        { AnnoTool::Fill,      "AnnoFill",      QString::fromUtf8("填充 (F)\n单击相近颜色的未标注连通区域") },
        { AnnoTool::Select,    "AnnoSelect",    QString::fromUtf8("选择 (S / V)\n移动/编辑形状\nDel删除 | Ctrl+D复制") },
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
    if (m_currentImageIdx + 1 >= m_samples.size()) return;
    openImage(m_currentModelId, m_samples[m_currentImageIdx + 1].absolutePath);
}

void AnnotationManagement::onNavigatePrev()
{
    if (m_currentImageIdx <= 0 || m_samples.isEmpty()) return;
    openImage(m_currentModelId, m_samples[m_currentImageIdx - 1].absolutePath);
}

void AnnotationManagement::onNavigateTo(int index)
{
    if (index < 0 || index >= m_samples.size()) return;
    if (index == m_currentImageIdx) return;
    openImage(m_currentModelId, m_samples[index].absolutePath);
}

void AnnotationManagement::onReturnToGallery()
{
    if (prepareToLeave()) emit returnToGallery();
}

// 工具切换：先取消未提交手势，再激活新工具。
void AnnotationManagement::onToolChanged(int toolId)
{
    if(!m_toolsMap.contains(toolId)) return;
    if(auto* button=m_toolGroup->button(toolId)) button->setChecked(true);
    // v3.0: 停用旧工具，切换到新工具
    if (m_activeTool && m_activeTool->type() == toolId) {m_canvas->setFocus();return;}
    cancelPendingTools();

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
        m_erasing = false;
    }

    if(m_activeTransaction) { delete m_activeTransaction; m_activeTransaction=nullptr; }
    // 清除选择状态
    m_editShapeIndex = -1;
    m_editVertexIndex = -1;
    m_rectPlacedStart = false;
    m_drawing = false;

    updateToolUI();
    m_canvas->setBrushDiameter(m_activeTool && m_activeTool->showBrushSize()?m_brushSize:0);
    m_canvas->setFocus();
    m_canvas->clearPreview();
    renderOverlay();
}

void AnnotationManagement::onLabelSelected(int labelIdx)
{
    if(labelIdx<0 || labelIdx>=m_labelNames.size()) return;
    m_activeLabelIdx = labelIdx;
    m_canvas->setFocus();
    updateLabelTable();
    updateToolUI();
}

void AnnotationManagement::onBrushSizeChanged(int value)
{
    m_brushSize = value;
    m_canvas->setBrushDiameter(m_activeTool && m_activeTool->showBrushSize()?value:0);
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
        m_canvas->setImage(m_displayImage,true);
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
    for(auto* tool:m_toolsMap) tool->context().viewScale=qMax(0.01,level);
    updateSelectionHandles();
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
    if(key==Qt::Key_H && mods==Qt::NoModifier) {m_btnOverlay->toggle();return;}
    if((key==Qt::Key_Return || key==Qt::Key_Enter) && mods==Qt::ControlModifier) {onSaveAndNext();return;}
    if(key==Qt::Key_Escape) {cancelPendingTools();renderOverlay();return;}
    if(!AccessControl::canWrite("深度学习.数据标注") && (key==Qt::Key_V || key==Qt::Key_D || key==Qt::Key_Delete || key==Qt::Key_Z || key==Qt::Key_Y || key==Qt::Key_S || hasPendingDrawing())) return;
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
        case Qt::Key_C:
        case Qt::Key_D: {
            QList<int> indices;
            for (auto* item : m_shapeList->selectedItems()) indices.append(item->data(Qt::UserRole).toInt());
            m_model->copyToClipboard(indices);
            if (key == Qt::Key_D && !hasPendingDrawing()) m_model->pasteFromClipboard();
            return;
        }
        case Qt::Key_V: if(!hasPendingDrawing()) m_model->pasteFromClipboard(); return;
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
    case Qt::Key_V:
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
    const QSignalBlocker blocker(m_shapeList);
    QSet<QUuid> selected;
    for(auto* item:m_shapeList->selectedItems()) selected.insert(item->data(Qt::UserRole+1).toUuid());
    QMap<QUuid,QListWidgetItem*> existing;
    while(m_shapeList->count()) {
        auto* item=m_shapeList->takeItem(0);existing.insert(item->data(Qt::UserRole+1).toUuid(),item);
    }
    for (int i = 0; i < m_model->shapes().size(); ++i) {
        const auto& s = m_model->shapes()[i];
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

        auto* item=existing.take(s.uuid);
        if(!item) item=new QListWidgetItem;
        item->setIcon(QIcon(chip));item->setText(QString::fromUtf8("%1  %2").arg(typeStr,label));
        item->setData(Qt::UserRole+1,s.uuid);
        item->setData(Qt::UserRole, i);
        m_shapeList->addItem(item);item->setSelected(selected.contains(s.uuid));
    }
    qDeleteAll(existing);
    const auto selection=m_shapeList->selectedItems();
    m_editShapeIndex=selection.isEmpty()?-1:selection.first()->data(Qt::UserRole).toInt();
    m_btnDeleteShape->setEnabled(!m_shapeList->selectedItems().isEmpty() && AccessControl::canWrite("深度学习.数据标注"));
    updateSelectionHandles();
}

void AnnotationManagement::onShapeListContextMenu(const QPoint& pos)
{
    if(!WriteAccess::require("深度学习.数据标注",this)) return;
    auto* item=m_shapeList->itemAt(pos); if(!item) return;
    const int idx=item->data(Qt::UserRole).toInt();
    const auto* shape=m_model->shapeAt(idx); if(!shape) return;
    QMenu menu(this);
    auto* visibility=menu.addAction(shape->visible?tr("隐藏"):tr("显示"));
    auto* remove=menu.addAction(tr("删除"));
    auto* chosen=menu.exec(m_shapeList->mapToGlobal(pos));
    if(!AccessControl::canWrite("深度学习.数据标注") || hasPendingDrawing()) return;
    if(chosen==visibility) m_model->setVisibility(idx,!m_model->shapeAt(idx)->visible);
    else if(chosen==remove) m_model->deleteShapes({idx});
}


void AnnotationManagement::onToggleShapeVisible()
{
    if(!WriteAccess::require("深度学习.数据标注",this)) return;
    if(hasPendingDrawing()) return;
    QList<int> indices;
    for(auto* item:m_shapeList->selectedItems()) indices.append(item->data(Qt::UserRole).toInt());
    AnnotationModel::Transaction tx(m_model,tr("切换可见性"));
    for(int i:indices) if(i>=0 && i<tx.shapes().size()) tx.shapes()[i].visible=!tx.shapes()[i].visible;
}


void AnnotationManagement::onDeleteSelectedShapes()
{
    if(!WriteAccess::require("深度学习.数据标注",this)) return;
    if(hasPendingDrawing()) return;
    QList<int> indices;
    for(auto* item:m_shapeList->selectedItems()) indices.append(item->data(Qt::UserRole).toInt());
    m_model->deleteShapes(indices);
}


void AnnotationManagement::onCanvasMousePressed(QPointF p, Qt::MouseButton btn)
{
    if(!AccessControl::canWrite("深度学习.数据标注")) return; // Guest canvas gestures are view-only.
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
        ctx.viewScale=m_canvas->zoomLevel();
                eraserTool->updateContext(ctx);
                m_erasing = true;
                eraserTool->mousePress(p, Qt::LeftButton);
                return;
            }
        }
        if (m_activeTool && m_activeTool->isDrawing())
            m_activeTool->mousePress(p, btn);
        return;
    }
    if (btn != Qt::LeftButton || m_activeTransaction || m_sourceImage.isNull()) return;
    if(m_currentTool!=AnnoTool::Select && (m_activeLabelIdx<0 || m_activeLabelIdx>=m_labelNames.size())) {QMessageBox::warning(this,tr("未选择类别"),tr("请先在数据集中新增类别并选中后标注。"));return;}

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
        ctx.viewScale=m_canvas->zoomLevel();
        m_activeTool->updateContext(ctx);

        // Select 工具需要 undo 事务
        if (m_activeTool->type() == AnnoTool::Select) {
            auto* selTool = qobject_cast<SelectTool*>(m_activeTool);
            if (selTool) {
                int si = selTool->shapeAtPosition(p);
                m_shapeList->clearSelection();
                if(si>=0 && si<m_shapeList->count()) m_shapeList->item(si)->setSelected(true);
                if (si >= 0) {
                    int vi = m_model->hitTestVertex(si, p, 8.0/m_canvas->zoomLevel());
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

    if(!AccessControl::canWrite("深度学习.数据标注")) return;
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
    if(!AccessControl::canWrite("深度学习.数据标注")) return; // Guest canvas gestures are view-only.
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
        m_editVertexIndex = -1;
        updateShapeList();renderOverlay();
    }

    // v3.0: 委托给活动工具
    if (m_activeTool) {
        m_activeTool->mouseRelease(p, btn);
    }
}

void AnnotationManagement::onCanvasDoubleClicked(QPointF p)
{
    if(!AccessControl::canWrite("深度学习.数据标注")) return; // Guest canvas gestures are view-only.
    // v3.0: 委托给活动工具
    if (m_activeTool) {
        m_activeTool->mouseDoubleClick(p);
    }
}

// =============================================================================
// 选择/编辑
// =============================================================================

void AnnotationManagement::renderOverlay()
{
    if (m_sourceImage.isNull() || !m_render) return;
    m_render->invalidateCache();
    QImage overlay = m_render->renderOverlay(
        m_model->shapes());
    m_canvas->setOverlay(overlay);
    updateSelectionHandles();
}

void AnnotationManagement::updateSelectionHandles()
{
    QPainterPath path;
    if(m_model && m_currentTool==AnnoTool::Select) {
        const double radius=4.0/qMax(0.01,m_canvas->zoomLevel());
        for(auto* item:m_shapeList->selectedItems()) {
            const auto* shape=m_model->shapeAt(item->data(Qt::UserRole).toInt());
            if(!shape || !shape->visible) continue;
            QPolygonF vertices;
            if(shape->type==AnnotationShape::PolygonShape) vertices=shape->polygon;
            else if(shape->type==AnnotationShape::RectShape || shape->type==AnnotationShape::RotatedRectShape) {
                vertices<<shape->bbox.topLeft()<<shape->bbox.topRight()<<shape->bbox.bottomRight()<<shape->bbox.bottomLeft();
                if(shape->type==AnnotationShape::RotatedRectShape) {
                    const auto center=shape->rotCenter.isNull()?shape->bbox.center():shape->rotCenter;
                    QTransform rotation;rotation.translate(center.x(),center.y());rotation.rotate(shape->rotation);rotation.translate(-center.x(),-center.y());
                    vertices=rotation.map(vertices);
                }
            }
            for(const auto& vertex:vertices) path.addEllipse(vertex,radius,radius);
        }
    }
    m_canvas->setSelectionPath(path);
}

QImage AnnotationManagement::compileMask() const
{
    if (m_sourceImage.isNull() || !m_render) return {};

    // v3.0: 委托给 AnnotationRender (支持 labelOffset)
    int offset = m_dataset ? m_dataset->currentLabelOffset() : 1;
    return m_render->compileMask(m_model->shapes(), offset);
}

// =============================================================================
// 保存 / 加载
// =============================================================================

bool AnnotationManagement::saveAnnotations()
{
    if(!WriteAccess::require("深度学习.数据标注",this)) return false;
    if (!m_dataset || m_currentImagePath.isEmpty() || m_currentModelId!=m_dataset->currentModelId()) return false;
    if (hasPendingDrawing()) {
        QMessageBox::warning(this,tr("标注尚未完成"),tr("请先完成当前绘制，或按 Esc 取消绘制后重试保存。"));
        return false;
    }
    const auto path=m_dataset->annotationsDir()+"/"+QFileInfo(m_currentImagePath).completeBaseName()
        +(LabelTypes::isMaskType(m_labelType)?".png":".json");
    const auto result=AnnotationStore::save(path,QFileInfo(m_currentImagePath).fileName(),m_sourceImage,
        m_model->shapes(),LabelTypes::isMaskType(m_labelType),m_labelType==LabelTypes::BinaryMask,
        m_labelNames.size(),m_dataset->currentLabelOffset());
    if (!result.ok) {
        QMessageBox::warning(this,tr("保存失败"),result.path+"\n"+result.error);
        Logger::warn(tr("标注保存失败：%1：%2").arg(result.path,result.error));
        return false;
    }
    m_model->markClean(); m_autoSaveTimer->stop();
    m_loadedAnnotationPath=result.path;
    int visible=0; QList<int> categories;
    for(const auto& shape:m_model->shapes()) if(shape.visible) {
        ++visible; if(!categories.contains(shape.labelId)) categories.append(shape.labelId);
    }
    m_dataset->onAnnotationUpdated(m_currentImagePath,categories,visible);
    Logger::info(tr("标注已保存：%1").arg(result.path));
    return true;
}


void AnnotationManagement::updateUndoRedoButtons()
{
    m_btnUndo->setEnabled(m_model->canUndo() && AccessControl::canWrite("深度学习.数据标注"));
    m_btnRedo->setEnabled(m_model->canRedo() && AccessControl::canWrite("深度学习.数据标注"));
}


void AnnotationManagement::onSave() { saveAnnotations(); }


void AnnotationManagement::onUndo() {
    if(!WriteAccess::require("深度学习.数据标注",this)) return; if(!hasPendingDrawing()) m_model->undo(); }


void AnnotationManagement::onRedo() {
    if(!WriteAccess::require("深度学习.数据标注",this)) return; if(!hasPendingDrawing()) m_model->redo(); }


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

    if(m_btnSaveNext) m_btnSaveNext->setStyleSheet(QString(
        "QPushButton {background:%1;color:white;border:0;border-radius:4px;padding:4px 8px;}"
        "QPushButton:disabled {background:%2;}").arg(p.accentPrimary,p.bgSecondary));
    for(auto* button:{m_btnKeepView,m_btnOverlay}) if(button) button->setStyleSheet(QString(
        "QPushButton {background:%1;color:%2;border:1px solid %3;border-radius:4px;padding:4px 8px;}"
        "QPushButton:checked {background:%4;color:white;border-color:%4;}")
        .arg(p.bgSecondary,p.textPrimary,p.borderMedium,p.accentPrimary));
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

bool AnnotationManagement::hasPendingDrawing() const
{
    if(m_activeTransaction) return true;
    for(auto* tool:m_toolsMap) if(tool->isDrawing()) return true;
    return false;
}

bool AnnotationManagement::hasUnsavedChanges() const
{
    return m_model->isDirty() || hasPendingDrawing();
}

void AnnotationManagement::cancelPendingTools()
{
    if(m_activeTransaction) {m_activeTransaction->cancel();delete m_activeTransaction;m_activeTransaction=nullptr;}
    for(auto* tool:m_toolsMap) {
        tool->cancelDrawing();
        if(auto* selection=qobject_cast<SelectTool*>(tool)) selection->clearSelection();
    }
    m_erasing=false; m_editShapeIndex=-1; m_editVertexIndex=-1;
    m_canvas->clearPreview();
}

bool AnnotationManagement::prepareToLeave()
{
    if(!hasUnsavedChanges()) return true;
    const bool resumeAutoSave=m_autoSaveTimer->isActive();m_autoSaveTimer->stop();
    const auto choice=QMessageBox::warning(this,tr("未保存的标注"),
        tr("图像 %1 有未保存的编辑或尚未完成的绘制。").arg(m_currentImagePath),
        QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel);
    if(choice==QMessageBox::Save) return saveAnnotations();
    if(choice!=QMessageBox::Discard) {if(resumeAutoSave) m_autoSaveTimer->start();return false;}
    cancelPendingTools();
    // Restore the last persisted version, so cancelling a later operation is safe.
    auto path=m_dataset->annotationsDir()+"/"+QFileInfo(m_currentImagePath).completeBaseName()
        +(LabelTypes::isMaskType(m_labelType)?".png":".json");
    if(!QFileInfo::exists(path) && !m_loadedAnnotationPath.isEmpty()) path=m_loadedAnnotationPath;
    QList<AnnotationShape> shapes; QString error;
    if(!DatasetFileService::readShapes(path,m_sourceImage.size(),m_labelNames,m_labelType==LabelTypes::BinaryMask,shapes,error)) {
        QMessageBox::warning(this,tr("恢复失败"),path+"\n"+error); return false;
    }
    m_model->reset(shapes); m_autoSaveTimer->stop(); return true;
}

void AnnotationManagement::clearContext()
{
    cancelPendingTools(); m_autoSaveTimer->stop(); m_model->reset();
    m_currentImagePath.clear(); m_loadedAnnotationPath.clear(); m_currentModelId.clear(); m_sourceImage={}; m_displayImage={};
    m_samples.clear(); m_currentImageIdx=-1;
    m_canvas->setImage({}); m_canvas->setOverlay({}); m_render->setSourceImage({});
    m_labelNames.clear();m_labelColors.clear();m_labelInstanceCounts.clear();
    m_lblImageInfo->clear();m_lblDimensions->clear();m_navigatorLabel->clear();
    updateShapeList();updateLabelTable();updateNavigationInfo();updateFilmIndexLabel();applyPermissions();
}

void AnnotationManagement::applyPermissions()
{
    const bool write=AccessControl::canWrite("深度学习.数据标注") && !m_sourceImage.isNull();
    m_btnSave->setEnabled(write); m_btnSaveNext->setEnabled(write);
    if(!write) cancelPendingTools();
    for(auto* button:m_toolGroup->buttons()) button->setEnabled(write);
    m_btnDeleteShape->setEnabled(write && !m_shapeList->selectedItems().isEmpty());
    updateUndoRedoButtons();
}

void AnnotationManagement::onSaveAndNext()
{
    if(!saveAnnotations()) return;
    if(m_currentImageIdx+1<m_samples.size()) onNavigateNext();
    m_canvas->setFocus();
}
