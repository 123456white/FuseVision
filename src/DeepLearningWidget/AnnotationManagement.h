#ifndef ANNOTATIONMANAGEMENT_H
#define ANNOTATIONMANAGEMENT_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QListWidget>
#include <QTableWidget>
#include <QButtonGroup>
#include <QImage>
#include <QPolygonF>
#include <QPainterPath>
#include <QUndoStack>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVector>
#include <QMap>
#include <QSplitter>
#include <QTimer>
#include "AnnotationModel.h"  // AnnotationShape + AnnotationModel (v3.0)
#include "core/LoadingOverlay.h"


class AnnotationCanvas;
class DatasetManagement;
class AnnotationModel;
class AnnotationRender;
class AnnotationTool;  // v3.0

// =============================================================================
// 标注工具类型
// =============================================================================
namespace AnnoTool {
    enum Type { Brush = 0, Eraser, Polygon, Rectangle, Fill, Select };
}

// AnnotationShape 现在定义在 AnnotationModel.h (v3.0)
// =============================================================================
// AnnotationManagement — 数据标注标签页
// =============================================================================
// 布局（左中右三栏 1:5:1）：
//   ┌────────┬──────────────────────────────────┬────────────┐
//   │ 图像导航│ [💾][↩][↪] 文件名                │ 导航器     │
//   │|◀◀3/50▶▶|│ [适应][1:1][-][+]  [返回]        │ (缩略图)   │
//   │ ■■■*■■ │──────────────────────────────────│            │
//   │────────│ 🖌│                             │ 图像调节   │
//   │────────│ 🧹│      Canvas                 │ 亮度: 0    │
//   │ 标签类别│ ⬠│                             │ [═══──]    │
//   │ 色块│名│ ▭│                             │ 对比度: 0  │
//   │ #│👁   │ 🪣│                             │ [═══──]    │
//   │        │ 🖱│                             │            │
//   │        │                                 │ 标注列表   │
//   │────────│                                 │ 🔴 画笔 bg │
//   │ 画笔   │                                 │ 🔴 多边形  │
//   │ 大小   │                                 │ [🗑删除]   │
//   │ [══]   │                                 │            │
//   │ 透明度 │                                 │            │
//   │ [══]   │                                 │            │
//   ├────────┴──────────────────────────────────┴────────────┤
//   │ (x,y)  WxH      🔍═══── 100%    │ 画笔 | 12px         │
//   └────────────────────────────────────────────────────────┘

class AnnotationManagement : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationManagement(QWidget* parent = nullptr);

    void setDatasetSource(DatasetManagement* dataset);
    void openImage(const QString& modelId, const QString& imagePath);
    void syncLabels();  // 从 DatasetManagement 同步标签类别（名称/颜色/实例数）
    bool hasUnsavedChanges() const { return m_modified; }

    // Undo 系统内部接口
    void restoreShapes(const QList<AnnotationShape>& shapes);
    void rebuildFromShapes();

signals:
    void returnToGallery();
    void labelMeRequested(const QString& imageDir, const QString& currentFile,
                          const QString& pythonExe, const QString& sdkPath,
                          const QStringList& labels, const QString& outputDir);

private slots:
    void onNavigateNext();
    void onNavigatePrev();
    void onNavigateTo(int index);
    void onReturnToGallery();
    void onLaunchLabelMe();

    // ── 工具 ──
    void onToolChanged(int toolId);
    void onLabelSelected(int labelIdx);
    void onBrushSizeChanged(int value);
    void onOverlayOpacityChanged(int value);
    void onBrightnessChanged(int value);
    void onContrastChanged(int value);
    void updateNavigator();
    void applyImageAdjustments();      // 将亮度/对比度应用到 m_displayImage

    // ── 视图 ──
    void onFitToWindow();
    void onResetZoom();
    void onZoomIn();
    void onZoomOut();

    // ── 键盘 ──
    void onCanvasKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    void onCanvasZoomChanged(double level);

    // ── 标签表格 ──
    void onLabelTableCellClicked(int row, int column);

    // ── 标注列表 ──
    void onShapeListContextMenu(const QPoint& pos);
    void onDeleteSelectedShapes();
    void onToggleShapeVisible();

    // ── 画布事件 ──
    void onCanvasMousePressed(QPointF imagePos, Qt::MouseButton button);
    void onCanvasMouseMoved(QPointF imagePos);
    void onCanvasMouseReleased(QPointF imagePos, Qt::MouseButton button);
    void onCanvasDoubleClicked(QPointF imagePos);

    // ── 保存/撤销 ──
    void onSave();
    void onUndo();
    void onRedo();
    void applyTheme();

private:
    void initUI();
    void initLeftPanel(QVBoxLayout* layout);
    void initCenterTopBar(QHBoxLayout* layout);
    void initCenterToolBar(QVBoxLayout* layout);
    void initRightPanel(QVBoxLayout* layout);
    void initBottomBar(QHBoxLayout* layout);

    void updateNavigationInfo();
    void updateShapeList();
    void updateLabelTable();
    void updateToolUI();
    void updateUndoRedoButtons();
    void updateFilmIndexLabel();

    // ── 文件 I/O ──
    void loadExistingAnnotations();
    void saveAnnotations();

    // ── 渲染 ──
    void renderOverlay();
    QImage compileMask() const;

    // ── Undo ──
    void pushUndoState(const QString& description);
    void commitUndoState(const QString& description);

    // ── 工具状态机 ──
    void startBrushStroke(QPointF imagePos);
    void continueBrushStroke(QPointF imagePos);
    void endBrushStroke();
    void startEraser(QPointF imagePos);
    void continueEraser(QPointF imagePos);
    void endEraser();
    void startPolygon(QPointF imagePos);
    void addPolygonVertex(QPointF imagePos);
    void closePolygon();
    void cancelPolygon();
    void startRectangle(QPointF imagePos);
    void updateRectanglePreview(QPointF imagePos);
    void endRectangle(QPointF imagePos);
    void cancelRectangle();
    void startFill(QPointF imagePos);
    void floodFillRegion(int seedX, int seedY, int fillLabelId);

    // ── 选择/编辑 ──
    int  hitTestShape(QPointF imagePos) const;
    int  hitTestVertex(int shapeIdx, QPointF imagePos) const;
    void startShapeDrag(int shapeIdx, QPointF imagePos);
    void continueShapeDrag(QPointF imagePos);
    void endShapeDrag();

    // ── DatasetManagement 数据 ──
    DatasetManagement* m_dataset       = nullptr;
    AnnotationModel*   m_model         = nullptr;   // v3.0 数据模型
    AnnotationRender*  m_render        = nullptr;   // v3.0 渲染管线
    QStringList   m_labelNames;
    QStringList   m_labelColors;
    QString       m_labelType;
    QList<int>    m_labelInstanceCounts;
    QVector<struct DatasetSample> m_samples;

    // ── 编辑状态 ──
    QString       m_currentImagePath;
    QString       m_currentModelId;
    QImage        m_sourceImage;
    QImage        m_displayImage;            // 经亮度/对比度调整后的显示图像
    double        m_brightness     = 0.0;     // -100~100
    double        m_contrast       = 0.0;     // -100~100
    QList<AnnotationShape> m_shapes;
    int           m_currentTool      = AnnoTool::Brush;
    int           m_activeLabelIdx   = 0;
    bool          m_modified         = false;
    int           m_currentImageIdx  = -1;

    // ── 绘制中间态 ──
    bool          m_drawing          = false;
    QPainterPath  m_currentBrushPath;
    QPolygonF     m_currentPolygon;
    QPointF       m_rectStart;
    QRectF        m_currentRect;
    bool          m_rectPlacedStart  = false;  // 两次点击矩形: 已放起点
    int           m_brushSize        = 12;
    bool          m_erasing          = false;

    // ── 选择/编辑模式 ──
    int           m_editShapeIndex   = -1;
    int           m_editVertexIndex  = -1;
    QPointF       m_editStartPos;

    // ── 剪贴板 ──
    QList<AnnotationShape> m_clipboard;

    // ── Undo ──
    QUndoStack*   m_undoStack        = nullptr;
    QList<AnnotationShape> m_undoSnapshot;
    bool          m_undoInProgress   = false;

    // ── 顶栏 ──
    QPushButton* m_btnSave           = nullptr;
    QPushButton* m_btnUndo           = nullptr;
    QPushButton* m_btnRedo           = nullptr;
    QPushButton* m_btnFit            = nullptr;
    QPushButton* m_btnZoom1          = nullptr;
    QPushButton* m_btnZoomIn         = nullptr;
    QPushButton* m_btnZoomOut        = nullptr;
    QLabel*      m_lblImageInfo      = nullptr;
    QLabel*      m_lblModified       = nullptr;

    // ── 右面板 ──
    QListWidget*  m_shapeList        = nullptr;
    QPushButton*  m_btnDeleteShape   = nullptr;
    QLabel*       m_navigatorLabel   = nullptr;   // 图像导航器缩略图
    QSlider*      m_brightnessSlider = nullptr;   // 亮度调节
    QLabel*       m_brightnessLabel  = nullptr;
    QSlider*      m_contrastSlider   = nullptr;   // 对比度调节
    QLabel*       m_contrastLabel    = nullptr;

    // ── 左面板 ──
    QTableWidget* m_labelTable       = nullptr;
    QSlider*      m_brushSizeSlider  = nullptr;
    QLabel*       m_brushSizeLabel   = nullptr;
    QSlider*      m_opacitySlider    = nullptr;

    // ── 底部工具栏 ──
    QButtonGroup* m_toolGroup        = nullptr;
    QMap<int, AnnotationTool*> m_toolsMap;  // v3.0 工具插件
    AnnotationTool* m_activeTool     = nullptr;  // v3.0
    AnnotationModel::Transaction* m_activeTransaction = nullptr;  // v3.0 Select undo
    QLabel*       m_lblPosition      = nullptr;
    QLabel*       m_lblDimensions    = nullptr;
    QSlider*      m_zoomSlider       = nullptr;   // 底部缩放滑块
    QLabel*       m_lblZoom          = nullptr;
    QLabel*       m_lblToolInfo      = nullptr;

    // ── 导航按钮 ──
    QPushButton*  m_btnFilmFirst     = nullptr;
    QPushButton*  m_btnFilmPrev      = nullptr;
    QPushButton*  m_btnFilmNext      = nullptr;
    QPushButton*  m_btnFilmLast      = nullptr;
    QLabel*       m_filmIndexLabel   = nullptr;

    // ── 其他 ──
    AnnotationCanvas* m_canvas       = nullptr;
    LoadingOverlay*   m_loadingOverlay = nullptr;  // 图像加载动画
    QPushButton* m_btnBack          = nullptr;
    QPushButton* m_btnLabelMe       = nullptr;  // 第三方 labelme 工具
    QSplitter*   m_mainSplitter     = nullptr;  // v3.0 可调面板

    QTimer*           m_autoSaveTimer = nullptr; // v3.0 自动保存
};

// =============================================================================
// AnnotationUndoCommand
// AnnotationUndoCommand (Legacy — P4 迁移到 AnnotationModel::Transaction)
class AnnoUndoCommandLegacy : public QUndoCommand
{
public:
    AnnoUndoCommandLegacy(const QList<AnnotationShape>& before,
                          const QList<AnnotationShape>& after,
                          AnnotationManagement* mgmt,
                          const QString& description);
    void undo() override;
    void redo() override;
private:
    QList<AnnotationShape> m_before;
    QList<AnnotationShape> m_after;
    AnnotationManagement*  m_mgmt;
};

#endif
