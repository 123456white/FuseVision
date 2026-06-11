#ifndef DATASETMANAGEMENT_H
#define DATASETMANAGEMENT_H

#include <QWidget>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QGridLayout>
#include <QListWidget>
#include <QComboBox>
#include <QFrame>
#include <QCheckBox>
#include <QLineEdit>
#include <QJsonObject>
#include <QMouseEvent>
#include <QSet>
#include "ProjectConfig.h"
#include "ModelConfig.h"
#include "core/ThemeManager.h"

class StepGuideBar;

// ── 标签格式类型 ──────────────────────────────────────────────
// 不同模型类型对应不同的标签格式：
//   实例分割 → 灰度掩码 PNG（灰度值 1=类别1, 2=类别2...）
//   异常检测 → 二值 PNG（0=正常, 1=异常）
//   物体检测(非自由矩形) → JSON 边界框（COCO 格式）
//   物体检测(自由矩形)   → JSON 边界框（COCO 格式，含旋转）
struct LabelTypes {
    static constexpr const char* GrayscaleMask = "灰度掩码PNG";   // 实例分割
    static constexpr const char* BinaryMask    = "二值掩码PNG";    // 异常检测
    static constexpr const char* CocoBBox      = "COCO边界框JSON"; // 物体检测(非自由矩形)
    static constexpr const char* CocoRotated   = "COCO旋转框JSON"; // 物体检测(自由矩形)

    static QStringList all() {
        return { QString::fromUtf8(GrayscaleMask),
                 QString::fromUtf8(BinaryMask),
                 QString::fromUtf8(CocoBBox),
                 QString::fromUtf8(CocoRotated) };
    }

    // 根据模型类型推荐标签格式
    static QString recommendForModelType(const QString& modelType) {
        if (modelType == ModelTaskTypes::InstanceSeg)    return GrayscaleMask;
        if (modelType == ModelTaskTypes::AnomalyDetect)  return BinaryMask;
        if (modelType == ModelTaskTypes::ObjDetectFixed) return CocoBBox;
        if (modelType == ModelTaskTypes::ObjDetectFree)  return CocoRotated;
        return GrayscaleMask;
    }

    // 标签格式是否为 PNG 掩码类型
    static bool isMaskType(const QString& labelType) {
        return labelType == GrayscaleMask || labelType == BinaryMask;
    }

    // 标签格式是否为 JSON 类型
    static bool isJsonType(const QString& labelType) {
        return labelType == CocoBBox || labelType == CocoRotated;
    }
};

// ── 统一数据集样本结构体（Phase 1 重构）────────────────────
// 对标 Halcon HDict + COCO image schema，统一承载图像元数据、
// 标注状态、选择状态与拆分归属，消除原先三个并行列表的同步问题。
struct DatasetSample {
    int         id          = -1;       // COCO 兼容 image_id
    QString     fileName;               // 文件名
    QString     absolutePath;           // 绝对路径
    int         width       = 0;        // 图像宽度
    int         height      = 0;        // 图像高度

    // ── 标注信息 ──
    bool        annotated   = false;    // 是否有标注文件
    QList<int>  categoryIds;            // 所有类别 ID（0-based，多标签支持）
    int         instanceCount = 0;      // 实例数（shapes/annotations 总数）

    // ── UI 状态（非持久化）──
    bool        selected    = false;    // 是否被选中

    // ── 拆分归属（预留）──
    enum Split { None = 0, Train, Val, Test };
    Split       split       = None;

    // ── 辅助 ──
    int  primaryCategory() const { return categoryIds.isEmpty() ? -1 : categoryIds.first(); }
    bool hasEmptyMask()     const { return annotated && categoryIds.isEmpty(); }
};

// =============================================================================
// DatasetManagement — 数据集管理标签页（DeepLearningWidget Tab 2）
// =============================================================================
// 布局（左右 1:5）：
//   左侧属性侧边栏（3 个固定面板）：
//     ┌──────────────────────┐
//     │ 面板1: 模型绑定信息   │ ← 模型名/任务类型/数据集路径
//     ├──────────────────────┤
//     │ 面板2: 图像信息       │ ← 选中图像的元数据
//     ├──────────────────────┤
//     │ 面板3: 标签类别管理   │ ← 可折叠列表 CRUD
//     └──────────────────────┘
//   右侧图像画廊主体：
//     ┌──────────────────────┐
//     │ 工具栏: 导入/统计/视图 │
//     ├──────────────────────┤
//     │ 缩略图网格画廊        │ ← 单击选中/双击标注
//     ├──────────────────────┤
//     │ StepGuideBar 步骤引导 │
//     └──────────────────────┘
// =============================================================================

class DatasetManagement : public QWidget
{
    Q_OBJECT
public:
    explicit DatasetManagement(QWidget* parent = nullptr);

    // 绑定当前项目和模型
    void setCurrentProject(const FvprojInfo& project);
    void setCurrentModel(const QString& modelId, const FvdlInfo& model);

    QString currentModelId() const { return m_currentModelId; }
    bool    hasModel()       const { return !m_currentModelId.isEmpty(); }

signals:
    void dataImported(const QString& modelId);                      // 图像导入完成
    void openAnnotationRequested(const QString& modelId,
                                 const QString& imagePath);         // 双击 → 标注
    void currentModelChanged(const QString& modelId);              // 状态更新通知

private slots:
    void applyTheme();
    void onImportImages();             // 导入数据集文件
    void onImportFolder();             // 导入数据集文件夹（images/ + labels/）
    void onThumbnailClicked(int idx);  // 单击缩略图
    void onThumbnailDoubleClicked(int idx); // 双击缩略图
    void onViewModeChanged(int mode);  // 网格/列表切换
    void onFilterChanged(int filter);  // 全部/未标注/已标注
    void onSelectAll(int state);          // 全选/取消全选
    void onDeleteSelected();             // 删除选中图像
    void onThumbnailCheckChanged(int idx, bool checked); // 复选框状态变化
    void onAddLabel();                 // 添加标签类别
    void onEditLabel();                // 编辑标签类别
    void onDeleteLabel();              // 删除标签类别
    void onImportLabels();             // 从 labels.txt / COCO 导入标签
    void onRefreshImageInfo();         // 刷新单图信息
    void onLabelOrderChanged();        // 标签拖拽重排后更新映射
    void onPreprocessClicked();        // 预处理占位按钮
    void onAugmentClicked();           // 数据增强占位按钮
    void onExportDataset();            // 导出数据集（COCO / YOLO）

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;  // 缩略图点击事件

private:
    void showContextMenu(const QPoint& pos, int thumbIdx);    // 右键菜单

private:
    void initUI();
    void initLeftPanel();
    void initRightPanel();
    void updateModelInfoPanel();       // 刷新面板1
    void updateImageInfoPanel(int idx); // 刷新面板2
    void updateLabelPanel();           // 刷新面板3（原面板4）
    void refreshGallery();             // 刷新画廊
    void loadDatasetConfig();          // 加载 dataset_config.json
    void saveDatasetConfig();          // 保存 dataset_config.json
    void updateStepGuide();            // 更新步骤引导条
    void setPanelEnabled(QWidget* panel, bool enabled);  // 面板启用/禁用
    void detectLabelsFromAnnotations(); // 扫描 annotations 自动识别标签类别
    bool ensureLabelTypeBeforeImport(); // 导入前确认标签类型
    bool validateLabelCount();          // 校验 labels 目录标签数与配置一致
    void updateSelectionUI();          // 更新全选框/已选计数/删除按钮状态

    // ── 导入/导出辅助方法 ────────────────────────────────────
    void writeCocoJson(const QString& outputPath, const QList<int>& exportIndices = {});    // 导出 COCO JSON（指定索引则部分导出）
    void writeYoloFormat(const QString& outputDir, const QList<int>& exportIndices = {});   // 导出 YOLO 格式（指定索引则部分导出）
    void writeMaskFormat(const QString& outputDir, const QList<int>& exportIndices = {});    // 导出 PNG 掩码（实例分割标准格式）
    bool parseCocoJson(const QString& jsonPath);      // 从 COCO JSON 导入

    // ── 统计与质量检查（Phase 2）────────────────────────────
    struct DatasetStats {
        int totalImages      = 0;
        int annotatedImages  = 0;
        int unannotatedImages = 0;
        int totalInstances   = 0;
        int categoryCount    = 0;
        double annotRate     = 0.0;
        // 质量告警
        QStringList warnings;
        int missingAnnotations  = 0;  // 有图无标注
        int orphanAnnotations   = 0;  // 有标注无图
        int corruptedFiles      = 0;  // 损坏的文件
        int emptyAnnotations    = 0;  // 空标注文件
    };
    DatasetStats computeStats() const;                  // 计算数据集统计
    void runQualityCheck(DatasetStats& stats) const;    // 质量检查
    void updateStatsDisplay(const DatasetStats& stats); // 刷新统计显示

    // ── 数据集路径工具 ────────────────────────────────────────
    QString datasetDir()   const;      // dl_datasets/Project/Model_dataset/
    QString imagesDir()    const;      // .../images/
    QString annotationsDir() const;   // .../annotations/
    static QString imageFileFilter();       // "图像文件 (*.png *.jpg ...);;所有文件 (*.*)"
    QString configPath()   const;      // .../dataset_config.json

    // ── 成员变量 ──────────────────────────────────────────────
    FvprojInfo    m_project;
    FvdlInfo      m_currentModel;
    QString       m_currentModelId;
    QString       m_dataRoot;
    QString       m_datasetRoot;
    QString       m_modelRoot;

    // ── 数据集配置（内存缓存）──────────────────────────────────
    QJsonObject   m_datasetConfig;        // dataset_config.json 完整内容
    QStringList   m_labelNames;           // 标签名称列表
    QStringList   m_labelColors;          // 标签颜色列表
    QList<int>    m_labelInstanceCounts;  // 每类标签的实例数（0=空标签占位）
    QString       m_labelType;            // 标签格式类型（LabelTypes 常量）
    QVector<DatasetSample> m_samples;     // 统一样本列表（替代原先 m_imagePaths + m_annotatedFlags + m_annotatedLabelIdx）
    QSet<int>     m_selectedIndices;      // 当前选中的图像索引（id → index 快速查找）
    int           m_activeThumbnailIdx = -1; // 当前单击高亮的缩略图索引（单张预览追踪）
    bool          m_galleryDeferred = false;  // refreshGallery 延迟重试防重入
    bool          m_inRefresh = false;        // refreshGallery 重入锁（防 processEvents 触发 resize 递归）
    QTimer*       m_loadDeferTimer = nullptr; // setCurrentModel 延迟加载定时器（可取消）

    // ── UI 组件
    // 主分割器
    QSplitter*    m_mainSplitter  = nullptr;
    QSplitter*    m_leftSplitter  = nullptr;
    QWidget*      m_rightContainer = nullptr;   // 右侧容器（对齐 PM/MM 模式）

    // ── 面板1：模型绑定信息 ──
    QLabel*       m_modelNameLabel    = nullptr;
    QLabel*       m_modelTypeLabel    = nullptr;
    QLabel*       m_datasetPathLabel  = nullptr;
    QLabel*       m_labelTypeLabel    = nullptr;   // 标签格式类型

    // ── 面板2：图像信息 ──
    QLabel*       m_imageFileName     = nullptr;
    QLabel*       m_imagePathLabel    = nullptr;
    QLabel*       m_imageResolution   = nullptr;
    QLabel*       m_imageInstances    = nullptr;
    QPushButton*  m_btnRefreshImage   = nullptr;
    QPushButton*  m_btnOpenFolder     = nullptr;
    QWidget*      m_imageInfoPanel    = nullptr;
    QLabel*       m_imageInfoHint     = nullptr;

    // ── 面板3：标签类别管理（原面板4） ──
    QListWidget*  m_labelListWidget    = nullptr;
    QPushButton*  m_btnAddLabel        = nullptr;
    QPushButton*  m_btnEditLabel       = nullptr;
    QPushButton*  m_btnDeleteLabel     = nullptr;
    QPushButton*  m_btnImportLabels    = nullptr;
    QWidget*      m_labelPanel         = nullptr;

    // ── 右侧画廊 ──
    QLabel*       m_lblTotalImages     = nullptr;
    QLabel*       m_lblAnnotatedCount  = nullptr;
    QLabel*       m_lblInstanceCount   = nullptr;
    QLabel*       m_lblLabelCount      = nullptr;
    QLabel*       m_lblQualityWarn    = nullptr;   // 质量告警指示器
    QCheckBox*    m_selectAllCheck     = nullptr;
    QPushButton*  m_btnDeleteSelected  = nullptr;
    QLabel*       m_lblSelectedCount   = nullptr;
    QComboBox*    m_viewModeCombo      = nullptr;
    QComboBox*    m_filterCombo        = nullptr;
    QScrollArea*  m_galleryScrollArea  = nullptr;

    // ── 占位按钮（Phase 1：UI 预留，功能待后续实现）──
    QPushButton*  m_btnPreprocess      = nullptr;
    QPushButton*  m_btnAugment         = nullptr;

    // ── 导入/导出按钮 ──
    QPushButton*  m_btnExport          = nullptr;

    // 画廊内部：StackedWidget 切换空态/内容
    QStackedWidget* m_galleryStack    = nullptr;
    QWidget*        m_emptyPage       = nullptr;   // 空态：大按钮导入
    QWidget*        m_galleryPage     = nullptr;   // 正常：横幅 + 缩略图
    QPushButton*    m_btnEmptyImport       = nullptr;   // 空态：导入文件
    QPushButton*    m_btnEmptyImportFolder = nullptr;   // 空态：导入文件夹

    QWidget*      m_galleryContainer   = nullptr;
    QGridLayout*  m_galleryLayout      = nullptr;
    QLabel*       m_galleryEmptyHint   = nullptr;
    QList<QFrame*> m_thumbnailFrames;  // 缩略图 Frame 列表
    int           m_lastGalleryWidth   = 0;       // 上次宽度，用于防抖列数重排

    // ── 列表视图 ──
    QListWidget*  m_galleryListView    = nullptr;

    // ── 步骤引导条 ──
    StepGuideBar* m_stepGuideBar       = nullptr;
};

#endif // DATASETMANAGEMENT_H