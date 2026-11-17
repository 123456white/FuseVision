/** @file
 * @brief DatasetManagement 的公共声明。
 * @ingroup dl_widgets
 */
#ifndef DATASETMANAGEMENT_H
#define DATASETMANAGEMENT_H
#include <functional>

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
#include "deeplearning/storage/ProjectConfig.h"
#include "deeplearning/storage/ModelConfig.h"
#include "ui/LoadingOverlay.h"


#include "deeplearning/domain/DatasetTypes.h"
struct DatasetTransferReport;

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
//     └──────────────────────┘
// =============================================================================

/** @brief 数据集浏览、类别和文件操作反馈界面，文件传输交给 DatasetFileService。
 * @ingroup dl_widgets
 */
class DatasetManagement : public QWidget
{
    friend class WorkflowRegression;
    Q_OBJECT
public:
    explicit DatasetManagement(QWidget* parent = nullptr);

    // 绑定当前项目和模型
    void setCurrentProject(const FvprojInfo& project);
    void setCurrentModel(const QString& modelId, const FvdlInfo& model);

    QString currentModelId() const { return m_currentModelId; }
    bool    hasModel()       const { return !m_currentModelId.isEmpty(); }
    bool isClassificationDataset() const { return m_classification; }

    // ── 数据集属性访问器（供标注模块使用）──
    QStringList currentLabelNames() const         { return m_labelNames; }
    QStringList currentLabelColors() const        { return m_labelColors; }
    QString     currentLabelType() const          { return m_labelType; }
    QList<int>  currentLabelInstanceCounts() const { return m_labelInstanceCounts; }
    QVector<DatasetSample> allSamples() const     { return m_samples; }
    int         currentLabelOffset() const;        // Halcon 兼容：GrayMask minGray 偏移

    // ── 标注集成 ──────────────────────────────────────────
    int  findSampleIndex(const QString& imagePath) const;
    void onAnnotationUpdated(const QString& imagePath,
                             const QList<int>& categoryIds,
                             int instanceCount);
    void detectLabelsFromAnnotations(); // 扫描 annotations 自动识别标签类别（公开：外部需在标注返回时触发）

    // ── 数据集路径工具 ────────────────────────────────────────
    QString datasetDir()   const;      // projectRoot/ModelName/
    QString imagesDir()    const;      // .../images/
    QString labelsDir()    const;      // .../labels/
    QString annotationsDir() const;   // .../labels/ (兼容旧接口)
    bool hasPersistedAnnotations() const;
    std::function<bool()> hasPendingAnnotations;

signals:
    void datasetFilesChanged();                                    // 已提交的导入/删除使旧画布上下文失效
    void dataImported(const QString& modelId);                      // 图像导入完成
    void annotationSaved(const QString& modelId);                   // 标注保存完成
    void labelsChanged();                                          // 标签类别变动（增/删/改/重排/导入）
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
    void onFormatConvert();             // 标注格式转换（YOLO txt / PNG掩码 / LabelMe JSON → 目标格式）
    void onAugmentClicked();           // 数据增强占位按钮
    void onExportDataset();            // 导出数据集（COCO / YOLO）

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;  // 缩略图点击事件

private:
    void showContextMenu(const QPoint& pos, int thumbIdx);    // 右键菜单

private:
    void applyPermissions();
    void initUI();
    void initLeftPanel();
    void initRightPanel();
    void updateModelInfoPanel();       // 刷新面板1
    void updateImageInfoPanel(int idx); // 刷新面板2
    void updateLabelPanel();           // 刷新面板3（原面板4）
    void refreshGallery();             // 刷新画廊
    void loadDatasetConfig();          // 加载 dataset_config.json
    bool saveDatasetConfig();          // 保存 dataset_config.json
    void setPanelEnabled(QWidget* panel, bool enabled);  // 面板启用/禁用
    bool ensureLabelTypeBeforeImport(); // 导入前确认标签类型
    bool validateLabelCount();          // 校验 labels 目录标签数与配置一致
    void importSelectedFiles(const QStringList& images,const QStringList& labels);
    bool convertAnnotations(const QString& suffix);
    void updateSelectionUI();          // 更新全选框/已选计数/删除按钮状态

    // ── 导入/导出辅助方法 ────────────────────────────────────
    void writeCocoJson(const QString& outputPath, const QList<int>& exportIndices = {});    // 导出 COCO JSON（指定索引则部分导出）
    void writeYoloFormat(const QString& outputDir, const QList<int>& exportIndices = {});   // 导出 YOLO 格式（指定索引则部分导出）
    void writeMaskFormat(const QString& outputDir, const QList<int>& exportIndices = {});    // 导出 PNG 掩码（实例分割标准格式）
    bool parseCocoJson(const QString& jsonPath, const QStringList& selectedImages = {});      // 从 COCO JSON 导入

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
    void refreshAnnotationStatistics();
    void runQualityCheck(DatasetStats& stats) const;    // 质量检查
    void updateStatsDisplay(const DatasetStats& stats); // 刷新统计显示

    // ── 标注格式转换 ────────────────────────────────────────
    enum InputFormat { Fmt_YOLO_Txt, Fmt_PNG_Mask, Fmt_LabelMe_JSON, Fmt_Unknown };
    InputFormat detectInputFormat() const;               // 自动检测 labels/ 下的标注格式

    struct ConvertResult {
        QString file;       // 文件名
        int     line   = -1;
        enum { Pass, Warning, Error } status = Pass;
        QString message;
    };

    // 检测当前标注是否符合目标格式（转换前的校验）
    QList<ConvertResult> validateCurrentFormat() const;

    // 三种输入格式 → 目标 YOLO txt 转换
    bool convertYoloTxtToTarget();      // YOLO txt 格式校验/修正
    bool convertPngMaskToTarget();      // PNG 掩码 → YOLO txt
    bool convertLabelMeJsonToTarget();  // LabelMe JSON → YOLO txt

    // 异常检测：引导导入 OK/NG 图像
    bool convertAnomalyClassification();
    bool finishClassificationImport(const DatasetTransferReport& report);

    // 转换结果展示
    void showConvertResultDialog(const QList<ConvertResult>& results, int converted, int failed);

    // ── 数据集路径工具 ────────────────────────────────────────
    static QString imageFileFilter();       // "图像文件 (*.png *.jpg ...);;所有文件 (*.*)"
    QString configPath()   const;      // .../dataset_config.json

    // ── 成员变量 ──────────────────────────────────────────────
    FvprojInfo    m_project;
    FvdlInfo      m_currentModel;
    QString       m_currentModelId;
    QString       m_projectRoot;

    // ── 数据集配置（内存缓存）──────────────────────────────────
    QJsonObject   m_datasetConfig;        // dataset_config.json 完整内容
    QStringList   m_labelNames;           // 标签名称列表
    QStringList   m_labelColors;          // 标签颜色列表
    QList<int>    m_labelInstanceCounts;  // 每类标签的实例数（0=空标签占位）
    QString       m_labelType;            // 标签格式类型（LabelTypes 常量）
    bool          m_classification = false;
    QVector<DatasetSample> m_samples;     // 统一样本列表（替代原先 m_imagePaths + m_annotatedFlags + m_annotatedLabelIdx）
    QSet<int>     m_selectedIndices;      // 当前选中的图像索引（id → index 快速查找）
    int           m_activeThumbnailIdx = -1; // 当前单击高亮的缩略图索引（单张预览追踪）
    bool          m_galleryDeferred = false;  // refreshGallery 延迟重试防重入
    bool          m_inRefresh = false;        // refreshGallery 重入锁（防 processEvents 触发 resize 递归）
    QTimer*       m_loadDeferTimer = nullptr; // setCurrentModel 延迟加载定时器（可取消）
    quint64 m_modelGeneration = 0;
    QTimer*       m_resizeDebounce = nullptr; // 防抖：窗口缩放结束后再刷新画廊

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

    // ── 工具栏按钮 ──
    QPushButton*  m_btnFormatConvert   = nullptr;
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
    LoadingOverlay* m_loadingOverlay    = nullptr;  // 画廊加载动画
    QList<QFrame*> m_thumbnailFrames;  // 缩略图 Frame 列表
    int           m_lastGalleryWidth   = 0;       // 上次宽度，用于防抖列数重排

    // ── 列表视图 ──
    QListWidget*  m_galleryListView    = nullptr;
};

#endif // DATASETMANAGEMENT_H
