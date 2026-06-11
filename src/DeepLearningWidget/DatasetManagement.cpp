#include "DatasetManagement.h"
#include "ProjectManagement.h"  // StepGuideBar
#include "core/Logger.h"
#include "core/SettingsManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QColorDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QIcon>
#include <QFont>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QImageReader>
#include <QListWidget>
#include <QTimer>
#include <climits>
#include <QMap>
#include <QMenu>
#include <QSet>
#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollBar>
#include <QProgressBar>
#include <QEvent>
#include <QDate>
#include <QTextStream>
#include <QSet>
#include <QPainter>
#include <QPen>

// =============================================================================
// DatasetManagement.cpp — 数据集管理标签页实现
// =============================================================================
// 布局：左侧属性面板（4面板）+ 右侧图像画廊（含内嵌状态横幅和空态入口）
// 对应 Deeplearn.md 第五章设计
// =============================================================================

// ── 预定义标签颜色（16 种循环使用）─────────────────────────────
static const QStringList s_defaultColors = {
    "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4",
    "#FFEAA7", "#DDA0DD", "#98D8C8", "#F7DC6F",
    "#BB8FCE", "#85C1E9", "#F8C471", "#82E0AA",
    "#F1948A", "#A3E4D7", "#D2B4DE", "#AED6F1"
};

// =============================================================================
// 构造 / 析构
// =============================================================================

DatasetManagement::DatasetManagement(QWidget* parent)
    : QWidget(parent)
{
    initUI();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &DatasetManagement::applyTheme);
}

// ── 绑定项目和模型 ────────────────────────────────────────────

void DatasetManagement::setCurrentProject(const FvprojInfo& project)
{
    m_project     = project;
    m_dataRoot    = project.dataRoot;
    m_datasetRoot = project.datasetRoot;
    m_modelRoot   = project.modelRoot;
}

void DatasetManagement::setCurrentModel(const QString& modelId, const FvdlInfo& model)
{
    // ── 防重入：refreshGallery 正在逐张加载时，第二个单击/双击会经 processEvents 放行 ──
    if (m_inRefresh) {
        // 同模型 → 忽略（数据已在加载中）；异模型 → 等当前加载完成后再切
        if (modelId == m_currentModelId) return;
        // 异模型：先更新绑定，当前 refreshGallery 完成后下次操作会加载新数据
        m_currentModelId = modelId;
        m_currentModel   = model;
        return;
    }

    m_currentModelId = modelId;
    m_currentModel   = model;
    m_samples.clear();
    m_labelNames.clear();
    m_labelColors.clear();
    m_labelInstanceCounts.clear();
    m_datasetConfig = QJsonObject();
    m_selectedIndices.clear();
    m_activeThumbnailIdx = -1;
    m_lastGalleryWidth = 0;   // 重置宽度记录，确保切页后重新布局

    // 面板2 重置
    if (m_imageInfoPanel)
        setPanelEnabled(m_imageInfoPanel, false);

    // ── 阶段 1：立即显示骨架（模型信息 + 加载提示）──
    updateModelInfoPanel();
    updateStepGuide();
    if (m_galleryEmptyHint)
        m_galleryEmptyHint->setText(QString::fromUtf8("\u23f3 正在加载数据集..."));
    if (m_galleryStack)
        m_galleryStack->setCurrentIndex(0);  // 显示空态提示页

    // ── 阶段 2：延迟加载数据（让 UI 先渲染骨架，再逐步填充）──
    // 取消上一次的延迟加载（防止快速切模型时重复触发）
    if (m_loadDeferTimer) {
        m_loadDeferTimer->stop();
        delete m_loadDeferTimer;
    }
    m_loadDeferTimer = new QTimer(this);
    m_loadDeferTimer->setSingleShot(true);
    connect(m_loadDeferTimer, &QTimer::timeout, this, [this]() {
        loadDatasetConfig();
        refreshGallery();
        saveDatasetConfig();   // ── 写入 instance_count 到 config（ModelManagement 展示用）──
        updateModelInfoPanel();
        updateStepGuide();
        // ── 强制刷新右侧面板布局（双击切标签页后确保 stepGuideBar 可见）──
        if (m_rightContainer) {
            m_rightContainer->updateGeometry();
            m_rightContainer->layout()->activate();
        }
    });
    m_loadDeferTimer->start(30);

    Logger::info(QString::fromUtf8("数据集管理：绑定模型 \"%1\"").arg(modelId));
}

// ── 路径工具 ──────────────────────────────────────────────────

QString DatasetManagement::datasetDir() const
{
    return QDir(m_datasetRoot).absoluteFilePath(m_currentModel.datasetRef);
}

QString DatasetManagement::imagesDir() const
{
    return QDir(datasetDir()).absoluteFilePath("images");
}

QString DatasetManagement::annotationsDir() const
{
    return QDir(datasetDir()).absoluteFilePath("annotations");
}

QString DatasetManagement::configPath() const
{
    return QDir(datasetDir()).absoluteFilePath("dataset_config.json");
}

QString DatasetManagement::imageFileFilter()
{
    // 硬编码常用格式作为兜底 — vcpkg Qt6 的图像插件 DLL 可能未部署，
    // QImageReader::supportedImageFormats() 可能漏掉 jpg/jpeg/webp 等。
    static const QStringList s_hardcodedFormats = {
        "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif",
        "*.webp", "*.gif", "*.ico", "*.pbm", "*.pgm", "*.ppm"
    };
    QSet<QString> extSet(s_hardcodedFormats.begin(), s_hardcodedFormats.end());
    for (const QByteArray& fmt : QImageReader::supportedImageFormats())
        extSet.insert(QString("*.%1").arg(QString::fromLatin1(fmt)));

    QStringList exts(extSet.begin(), extSet.end());
    exts.sort();
    return QString::fromUtf8("图像文件 (%1);;所有文件 (*.*)").arg(exts.join(" "));
}

// ── 加载数据集配置 ────────────────────────────────────────────

void DatasetManagement::loadDatasetConfig()
{
    // ── 防重入：refreshGallery 的 processEvents 可能释放其他 timer callback ──
    if (m_inRefresh) return;

    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;

    m_datasetConfig = doc.object();

    // 解析标签格式类型
    m_labelType = m_datasetConfig.value("label_type").toString();

    // 解析标签
    m_labelNames.clear();
    m_labelColors.clear();
    m_labelInstanceCounts.clear();
    QJsonArray cats = m_datasetConfig.value("categories").toArray();
    for (const QJsonValue& v : cats) {
        QJsonObject cat = v.toObject();
        m_labelNames.append(cat.value("name").toString());
        m_labelColors.append(cat.value("color").toString("#888888"));
        m_labelInstanceCounts.append(cat.value("instance_count").toInt(-1));
    }

    // 扫描 images 目录（硬编码格式兜底 + 动态格式合并）
    QDir imgDir(imagesDir());
    static const QStringList s_hardcodedImgFormats = {
        "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif",
        "*.webp", "*.gif", "*.ico", "*.pbm", "*.pgm", "*.ppm"
    };
    QSet<QString> filterSet(s_hardcodedImgFormats.begin(), s_hardcodedImgFormats.end());
    for (const QByteArray& fmt : QImageReader::supportedImageFormats())
        filterSet.insert(QString("*.%1").arg(QString::fromLatin1(fmt)));
    QStringList filters(filterSet.begin(), filterSet.end());
    imgDir.setNameFilters(filters);

    m_samples.clear();

    QFileInfoList files = imgDir.entryInfoList(QDir::Files, QDir::Name);
    QMap<QString, QList<int>> baseToLabelIdx; // baseName → label index list (multi-label)

    // 扫描 annotations 目录获取已标注文件及对应标签
    QDir annDir(annotationsDir());
    if (annDir.exists()) {
        // ── JSON 标注（labelme shapes 或 COCO annotations）──
        QFileInfoList annFiles = annDir.entryInfoList({"*.json"}, QDir::Files);
        for (const QFileInfo& af : annFiles) {
            QFile annF(af.absoluteFilePath());
            if (!annF.open(QIODevice::ReadOnly)) continue;
            QJsonDocument adoc = QJsonDocument::fromJson(annF.readAll());
            annF.close();

            QJsonObject root = adoc.object();

            // 收集所有 category_id（多标签支持）
            QSet<int> lids;

            // 1. COCO annotations 提取
            QJsonArray annots = root.value("annotations").toArray();
            for (const QJsonValue& av : annots) {
                int cid = av.toObject().value("category_id").toInt(-1);
                if (cid >= 0) lids.insert(cid);
            }

            // 2. labelme shapes 提取
            if (lids.isEmpty()) {
                QJsonArray shapes = root.value("shapes").toArray();
                for (const QJsonValue& sv : shapes) {
                    QJsonObject shape = sv.toObject();
                    int cid = shape.value("category_id").toInt(-1);
                    if (cid < 0) {
                        QString lbl = shape.value("label").toString();
                        cid = m_labelNames.indexOf(lbl);
                    }
                    if (cid >= 0) lids.insert(cid);
                }
            }

            for (int cid : lids)
                baseToLabelIdx[af.completeBaseName()].append(cid);
            if (lids.isEmpty())
                baseToLabelIdx[af.completeBaseName()]; // 空标注占位
        }
        // PNG 掩码标注 — 收集所有类别灰度值
        annFiles = annDir.entryInfoList({"*.png"}, QDir::Files);
        for (const QFileInfo& af : annFiles) {
            QImage img(af.absoluteFilePath());
            if (img.isNull()) continue;
            QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
            int w = gray.width(), h = gray.height();
            QSet<int> grayVals;
            // 采样找出所有非零灰度值
            int stepY = qMax(1, h / 20);
            int stepX = qMax(1, w / 20);
            for (int y = 0; y < h; y += stepY) {
                const uchar* line = gray.constScanLine(y);
                for (int x = 0; x < w; x += stepX) {
                    if (line[x] > 0 && line[x] < 250)
                        grayVals.insert(line[x]);
                }
            }
            // 灰度值 → 0-based 类别索引（兼容 Halcon 偏移：minGray≥2 时自动矫正）
            if (!grayVals.isEmpty()) {
                int offset = *std::min_element(grayVals.begin(), grayVals.end());
                for (int gv : grayVals) {
                    int lid = gv - offset;  // 标准: gv=1→0  Halcon: gv=2→0
                    if (lid >= 0)
                        baseToLabelIdx[af.completeBaseName()].append(lid);
                }
            }
            if (grayVals.isEmpty())
                baseToLabelIdx[af.completeBaseName()]; // 全黑掩码占位
        }
    }

    for (const QFileInfo& fi : files) {
        DatasetSample sample;
        sample.id           = m_samples.size() + 1;  // COCO 兼容 1-based
        sample.fileName     = fi.fileName();
        sample.absolutePath = fi.absoluteFilePath();
        QString base = fi.completeBaseName();
        // 有标注 = baseToLabelIdx 中有该项且 categoryIds 非空（空掩码/空标注不算已标注）
        sample.annotated    = baseToLabelIdx.contains(base) && !baseToLabelIdx.value(base).isEmpty();
        sample.categoryIds  = baseToLabelIdx.value(base);

        // 读取实例数
        if (sample.annotated) {
            QString annJson = QDir(annotationsDir()).absoluteFilePath(base + ".json");
            QFile af(annJson);
            if (af.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(af.readAll());
                af.close();
                sample.instanceCount = doc.object().value("shapes").toArray().size();
                if (sample.instanceCount == 0)
                    sample.instanceCount = doc.object().value("annotations").toArray().size();
            }
            // PNG 掩码回退：统计掩码中出现的不同灰度值数
            if (sample.instanceCount == 0) {
                QString annPng = QDir(annotationsDir()).absoluteFilePath(base + ".png");
                QImage mask(annPng);
                if (!mask.isNull()) {
                    QImage grayImg = mask.convertToFormat(QImage::Format_Grayscale8);
                    QSet<int> uniqueVals;
                    int w = grayImg.width(), h = grayImg.height();
                    int step = qMax(1, qMin(w, h) / 100);
                    for (int y = 0; y < h; y += step) {
                        const uchar* line = grayImg.constScanLine(y);
                        for (int x = 0; x < w; x += step) {
                            if (line[x] > 0 && line[x] < 250)
                                uniqueVals.insert(line[x]);
                        }
                    }
                    sample.instanceCount = uniqueVals.size();
                }
            }
        }

        // 读取图像尺寸（缓存）
        QImageReader reader(sample.absolutePath);
        QSize sz = reader.size();
        if (sz.isValid()) {
            sample.width  = sz.width();
            sample.height = sz.height();
        }

        m_samples.append(sample);
    }
}

// ── 保存数据集配置 ────────────────────────────────────────────

void DatasetManagement::saveDatasetConfig()
{
    QDir().mkpath(datasetDir());

    QJsonArray cats;
    for (int i = 0; i < m_labelNames.size(); ++i) {
        QJsonObject cat;
        cat["id"]    = i + 1;
        cat["name"]  = m_labelNames[i];
        cat["color"] = m_labelColors.value(i, s_defaultColors[i % s_defaultColors.size()]);
        cat["instance_count"] = m_labelInstanceCounts.value(i, -1);
        cats.append(cat);
    }

    QJsonObject cfg = m_datasetConfig;
    cfg["categories"]  = cats;
    cfg["image_count"] = m_samples.size();
    cfg["label_type"]  = m_labelType;

    // ── 写入顶层 instance_count（ModelManagement 面板展示用）──
    int totalInstances = 0;
    for (const auto& s : m_samples)
        totalInstances += s.instanceCount;
    cfg["instance_count"] = totalInstances;

    m_datasetConfig = cfg;

    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
        f.close();
    }
}

// =============================================================================
// UI 构建
// =============================================================================

void DatasetManagement::initUI()
{
    setAutoFillBackground(true);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(1);
    m_mainSplitter->setChildrenCollapsible(false);

    initLeftPanel();
    initRightPanel();

    m_mainSplitter->addWidget(m_leftSplitter);
    m_mainSplitter->addWidget(m_rightContainer);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 5);
    m_mainSplitter->setSizes({ 200, 1000 });

    mainLayout->addWidget(m_mainSplitter, 1);
}

// ── 左侧属性侧边栏 ────────────────────────────────────────────

void DatasetManagement::initLeftPanel()
{
    m_leftSplitter = new QSplitter(Qt::Vertical);
    m_leftSplitter->setHandleWidth(1);
    m_leftSplitter->setChildrenCollapsible(false);

    const auto& p = ThemeManager::instance().palette();

    // ══════════════════════════════════════════════════════
    // 面板1：当前绑定模型信息区
    // ══════════════════════════════════════════════════════
    QWidget* panel1 = new QWidget;
    panel1->setObjectName("dsPanel1");
    QVBoxLayout* p1Layout = new QVBoxLayout(panel1);
    p1Layout->setContentsMargins(10, 10, 10, 10);
    p1Layout->setSpacing(6);

    QLabel* p1Title = new QLabel(QString::fromUtf8("绑定模型信息"));
    QFont boldF = p1Title->font();
    boldF.setBold(true);
    boldF.setPointSize(11);
    p1Title->setFont(boldF);
    p1Title->setObjectName("dsPanelTitle");
    p1Layout->addWidget(p1Title);

    m_modelNameLabel = new QLabel(QString::fromUtf8("模型：--"));
    m_modelNameLabel->setWordWrap(true);
    m_modelNameLabel->setObjectName("dsPanelValue");
    p1Layout->addWidget(m_modelNameLabel);

    m_modelTypeLabel = new QLabel(QString::fromUtf8("任务类型：--"));
    m_modelTypeLabel->setWordWrap(true);
    m_modelTypeLabel->setObjectName("dsPanelValue");
    p1Layout->addWidget(m_modelTypeLabel);

    m_datasetPathLabel = new QLabel(QString::fromUtf8("数据集路径：--"));
    m_datasetPathLabel->setWordWrap(true);
    m_datasetPathLabel->setObjectName("dsPanelPath");
    m_datasetPathLabel->setCursor(Qt::PointingHandCursor);
    m_datasetPathLabel->setToolTip(QString::fromUtf8("点击复制路径"));
    connect(m_datasetPathLabel, &QLabel::linkActivated, this, [this](const QString&) {
        QApplication::clipboard()->setText(datasetDir());
        Logger::info(QString::fromUtf8("已复制数据集路径: %1").arg(datasetDir()));
    });
    p1Layout->addWidget(m_datasetPathLabel);

    m_labelTypeLabel = new QLabel(QString::fromUtf8("标签格式：--"));
    m_labelTypeLabel->setWordWrap(true);
    m_labelTypeLabel->setObjectName("dsPanelValue");
    p1Layout->addWidget(m_labelTypeLabel);

    p1Layout->addStretch();

    // ══════════════════════════════════════════════════════
    // 面板2：图像信息面板
    // ══════════════════════════════════════════════════════
    QWidget* panel2 = new QWidget;
    panel2->setObjectName("dsPanel2");
    QVBoxLayout* p2Layout = new QVBoxLayout(panel2);
    p2Layout->setContentsMargins(10, 10, 10, 10);
    p2Layout->setSpacing(6);

    QLabel* p2Title = new QLabel(QString::fromUtf8("图像信息"));
    p2Title->setFont(boldF);
    p2Title->setObjectName("dsPanelTitle");
    p2Layout->addWidget(p2Title);

    // 无选中时的提示
    m_imageInfoHint = new QLabel(QString::fromUtf8("请选择右侧预览图像"));
    m_imageInfoHint->setWordWrap(true);
    m_imageInfoHint->setObjectName("dsHint");
    m_imageInfoHint->setAlignment(Qt::AlignCenter);
    p2Layout->addWidget(m_imageInfoHint);

    // 图像信息内容区
    m_imageInfoPanel = new QWidget;
    m_imageInfoPanel->setObjectName("dsImageInfoContent");
    m_imageInfoPanel->setVisible(false);
    QVBoxLayout* infoContentLayout = new QVBoxLayout(m_imageInfoPanel);
    infoContentLayout->setContentsMargins(0, 0, 0, 0);
    infoContentLayout->setSpacing(4);

    m_imageFileName = new QLabel;
    m_imageFileName->setWordWrap(true);
    m_imageFileName->setObjectName("dsPanelValue");
    infoContentLayout->addWidget(m_imageFileName);

    m_imagePathLabel = new QLabel;
    m_imagePathLabel->setWordWrap(true);
    m_imagePathLabel->setObjectName("dsPanelPath");
    m_imagePathLabel->setToolTip(QString::fromUtf8("点击复制路径"));
    m_imagePathLabel->setCursor(Qt::PointingHandCursor);
    connect(m_imagePathLabel, &QLabel::linkActivated, this, [this](const QString&) {
        if (!m_imagePathLabel->text().contains("--"))
            QApplication::clipboard()->setText(m_imagePathLabel->text());
    });
    infoContentLayout->addWidget(m_imagePathLabel);

    m_imageResolution = new QLabel;
    m_imageResolution->setObjectName("dsPanelValue");
    infoContentLayout->addWidget(m_imageResolution);

    m_imageInstances = new QLabel;
    m_imageInstances->setWordWrap(true);
    m_imageInstances->setObjectName("dsPanelValue");
    infoContentLayout->addWidget(m_imageInstances);

    infoContentLayout->addSpacing(6);

    // 功能按钮行
    QHBoxLayout* btnRow = new QHBoxLayout;
    m_btnRefreshImage = new QPushButton(QString::fromUtf8("刷新"));
    m_btnRefreshImage->setObjectName("dsSmallBtn");
    m_btnRefreshImage->setCursor(Qt::PointingHandCursor);
    connect(m_btnRefreshImage, &QPushButton::clicked, this, &DatasetManagement::onRefreshImageInfo);

    m_btnOpenFolder = new QPushButton(QString::fromUtf8("打开文件夹"));
    m_btnOpenFolder->setObjectName("dsSmallBtn");
    m_btnOpenFolder->setCursor(Qt::PointingHandCursor);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, [this]() {
        if (m_samples.isEmpty()) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(imagesDir()));
    });

    btnRow->addWidget(m_btnRefreshImage);
    btnRow->addWidget(m_btnOpenFolder);
    infoContentLayout->addLayout(btnRow);

    p2Layout->addWidget(m_imageInfoPanel);
    p2Layout->addStretch();

    // ══════════════════════════════════════════════════════
    // 面板4：标签类别管理
    // ══════════════════════════════════════════════════════
    QWidget* panel4 = new QWidget;
    panel4->setObjectName("dsPanel4");
    QVBoxLayout* p4Layout = new QVBoxLayout(panel4);
    p4Layout->setContentsMargins(10, 10, 10, 10);
    p4Layout->setSpacing(6);

    QLabel* p4Title = new QLabel(QString::fromUtf8("标签类别管理"));
    p4Title->setFont(boldF);
    p4Title->setObjectName("dsPanelTitle");
    p4Layout->addWidget(p4Title);

    m_labelListWidget = new QListWidget;
    m_labelListWidget->setObjectName("dsLabelList");
    m_labelListWidget->setMaximumHeight(180);
    m_labelListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_labelListWidget->setDefaultDropAction(Qt::MoveAction);
    connect(m_labelListWidget->model(), &QAbstractItemModel::rowsMoved,
            this, [this]() { onLabelOrderChanged(); });
    p4Layout->addWidget(m_labelListWidget);

    // 操作按钮
    QGridLayout* lblBtnGrid = new QGridLayout;
    lblBtnGrid->setSpacing(4);

    m_btnAddLabel = new QPushButton(QString::fromUtf8("+ 添加"));
    m_btnAddLabel->setObjectName("dsSmallBtn");
    m_btnAddLabel->setCursor(Qt::PointingHandCursor);
    connect(m_btnAddLabel, &QPushButton::clicked, this, &DatasetManagement::onAddLabel);

    m_btnEditLabel = new QPushButton(QString::fromUtf8("编辑"));
    m_btnEditLabel->setObjectName("dsSmallBtn");
    m_btnEditLabel->setCursor(Qt::PointingHandCursor);
    connect(m_btnEditLabel, &QPushButton::clicked, this, &DatasetManagement::onEditLabel);

    m_btnDeleteLabel = new QPushButton(QString::fromUtf8("删除"));
    m_btnDeleteLabel->setObjectName("dsSmallBtnDanger");
    m_btnDeleteLabel->setCursor(Qt::PointingHandCursor);
    connect(m_btnDeleteLabel, &QPushButton::clicked, this, &DatasetManagement::onDeleteLabel);

    m_btnImportLabels = new QPushButton(QString::fromUtf8("导入"));
    m_btnImportLabels->setObjectName("dsSmallBtn");
    m_btnImportLabels->setCursor(Qt::PointingHandCursor);
    connect(m_btnImportLabels, &QPushButton::clicked, this, &DatasetManagement::onImportLabels);

    lblBtnGrid->addWidget(m_btnAddLabel,    0, 0);
    lblBtnGrid->addWidget(m_btnEditLabel,   0, 1);
    lblBtnGrid->addWidget(m_btnDeleteLabel, 1, 0);
    lblBtnGrid->addWidget(m_btnImportLabels,1, 1);
    p4Layout->addLayout(lblBtnGrid);

    m_labelPanel = panel4;

    m_leftSplitter->addWidget(panel1);
    m_leftSplitter->addWidget(panel2);
    m_leftSplitter->addWidget(panel4);
    m_leftSplitter->setSizes({ 150, 220, 260 });
}

// ── 右侧主体（对齐 PM/MM 模式：容器 → 工具栏 + 画廊）──────

void DatasetManagement::initRightPanel()
{
    m_rightContainer = new QWidget;
    m_rightContainer->setObjectName("dsRightContainer");
    QVBoxLayout* rightLayout = new QVBoxLayout(m_rightContainer);
    rightLayout->setContentsMargins(16, 16, 16, 0);
    rightLayout->setSpacing(0);

    // ── 顶部工具栏 ──
    QWidget* toolbar = new QWidget;
    toolbar->setObjectName("dsToolbar");
    QHBoxLayout* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(0, 4, 0, 8);
    tbLayout->setSpacing(8);

    // ── 导入数据集按钮（常驻顶部）──
    const auto& p = ThemeManager::instance().palette();

    QPushButton* btnImportFile = new QPushButton(QString::fromUtf8("导入数据集文件"));
    btnImportFile->setObjectName("dsToolBtn");
    btnImportFile->setCursor(Qt::PointingHandCursor);
    btnImportFile->setFixedHeight(28);
    btnImportFile->setStyleSheet(QString(
        "QPushButton#dsToolBtn {"
        "  background-color: %1; color: #ffffff; border: 1px solid %1;"
        "  border-radius: 4px; padding: 4px 14px; font-size: 12px; }"
        "QPushButton#dsToolBtn:hover { background-color: %2; border-color: %2; }")
        .arg(p.accentPrimary, p.accentHover));
    connect(btnImportFile, &QPushButton::clicked, this, &DatasetManagement::onImportImages);
    tbLayout->addWidget(btnImportFile);

    QPushButton* btnImportFolder = new QPushButton(QString::fromUtf8("导入数据集文件夹"));
    btnImportFolder->setObjectName("dsToolBtn");
    btnImportFolder->setCursor(Qt::PointingHandCursor);
    btnImportFolder->setFixedHeight(28);
    btnImportFolder->setStyleSheet(QString(
        "QPushButton#dsToolBtn {"
        "  background-color: %1; color: #ffffff; border: 1px solid %1;"
        "  border-radius: 4px; padding: 4px 14px; font-size: 12px; }"
        "QPushButton#dsToolBtn:hover { background-color: %2; border-color: %2; }")
        .arg(p.accentPrimary, p.accentHover));
    connect(btnImportFolder, &QPushButton::clicked, this, &DatasetManagement::onImportFolder);
    tbLayout->addWidget(btnImportFolder);

    QFrame* sepImport = new QFrame;
    sepImport->setFrameShape(QFrame::VLine);
    sepImport->setObjectName("dsTbSep");
    tbLayout->addWidget(sepImport);

    // 统计信息
    auto addStat = [&](const QString& label, QLabel*& valueLabel) {
        QLabel* lbl = new QLabel(label);
        lbl->setObjectName("dsStatLabel");
        tbLayout->addWidget(lbl);
        valueLabel = new QLabel("0");
        valueLabel->setObjectName("dsStatValue");
        tbLayout->addWidget(valueLabel);
    };
    addStat(QString::fromUtf8("总计:"), m_lblTotalImages);
    addStat(QString::fromUtf8("已标注:"), m_lblAnnotatedCount);
    addStat(QString::fromUtf8("实例:"), m_lblInstanceCount);
    addStat(QString::fromUtf8("类别:"), m_lblLabelCount);

    // 质量告警指示器
    m_lblQualityWarn = new QLabel;
    m_lblQualityWarn->setObjectName("dsQualityWarn");
    m_lblQualityWarn->setCursor(Qt::PointingHandCursor);
    m_lblQualityWarn->setToolTip(QString::fromUtf8("点击查看质量详情"));
    m_lblQualityWarn->hide();
    connect(m_lblQualityWarn, &QLabel::linkActivated, this, [this](const QString&) {
        DatasetStats stats = computeStats();
        runQualityCheck(stats);
        if (stats.warnings.isEmpty()) {
            QMessageBox::information(this, QString::fromUtf8("质量检查"),
                QString::fromUtf8("数据集质量良好，未发现问题。\n\n"
                "总计: %1 张 | 已标注: %2 张 (%3%)\n"
                "实例: %4 | 类别: %5")
                .arg(stats.totalImages).arg(stats.annotatedImages)
                .arg(stats.annotRate, 0, 'f', 1)
                .arg(stats.totalInstances).arg(stats.categoryCount));
        } else {
            QMessageBox::warning(this, QString::fromUtf8("质量检查"),
                QString::fromUtf8("发现 %1 个问题:\n\n%2")
                .arg(stats.warnings.size()).arg(stats.warnings.join("\n")));
        }
    });
    tbLayout->addWidget(m_lblQualityWarn);

    QFrame* sepStats = new QFrame;
    sepStats->setFrameShape(QFrame::VLine);
    sepStats->setObjectName("dsTbSep");
    tbLayout->addWidget(sepStats);

    // 全选复选框
    m_selectAllCheck = new QCheckBox(QString::fromUtf8("全选"));
    m_selectAllCheck->setObjectName("dsSelectAll");
    m_selectAllCheck->setTristate(false);
    m_selectAllCheck->setStyleSheet(
        QString("QCheckBox#dsSelectAll { color: %1; font-size: 11px; spacing: 4px; }"
                "QCheckBox#dsSelectAll:checked { color: %2; font-weight: bold; }"
                "QCheckBox#dsSelectAll:indeterminate { color: %3; font-weight: bold; }")
            .arg(p.textMuted, p.success, p.warning));
    connect(m_selectAllCheck, &QCheckBox::stateChanged, this, &DatasetManagement::onSelectAll);
    tbLayout->addWidget(m_selectAllCheck);

    // 已选计数
    m_lblSelectedCount = new QLabel(QString::fromUtf8("已选: 0"));
    m_lblSelectedCount->setObjectName("dsSelectedCount");
    m_lblSelectedCount->setFixedWidth(55);
    tbLayout->addWidget(m_lblSelectedCount);

    // 删除选中按钮
    m_btnDeleteSelected = new QPushButton(QString::fromUtf8("删除选中"));
    m_btnDeleteSelected->setObjectName("dsDeleteBtn");
    m_btnDeleteSelected->setCursor(Qt::PointingHandCursor);
    m_btnDeleteSelected->setFixedHeight(24);
    m_btnDeleteSelected->setStyleSheet(
        QString("QPushButton#dsDeleteBtn { background: transparent; color: %1;"
                " border: 1px solid %1; border-radius: 4px; padding: 2px 10px; font-size: 11px; }"
                "QPushButton#dsDeleteBtn:hover { background: %1; color: #ffffff; }"
                "QPushButton#dsDeleteBtn:disabled { color: %2; border-color: %2; }")
            .arg(p.danger, p.textMuted));
    connect(m_btnDeleteSelected, &QPushButton::clicked, this, &DatasetManagement::onDeleteSelected);
    tbLayout->addWidget(m_btnDeleteSelected);

    tbLayout->addStretch();

    QFrame* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setObjectName("dsTbSep");
    tbLayout->addWidget(sep2);

    m_viewModeCombo = new QComboBox;
    m_viewModeCombo->addItem(QString::fromUtf8("网格视图"));
    m_viewModeCombo->addItem(QString::fromUtf8("列表视图"));
    m_viewModeCombo->setObjectName("dsCombo");
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DatasetManagement::onViewModeChanged);
    tbLayout->addWidget(m_viewModeCombo);

    m_filterCombo = new QComboBox;
    m_filterCombo->addItem(QString::fromUtf8("全部"));
    m_filterCombo->addItem(QString::fromUtf8("未标注"));
    m_filterCombo->addItem(QString::fromUtf8("已标注"));
    m_filterCombo->setObjectName("dsCombo");
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DatasetManagement::onFilterChanged);
    tbLayout->addWidget(m_filterCombo);

    // ── 占位按钮：预处理（Phase 1 预留，功能待实现）──
    m_btnPreprocess = new QPushButton(QString::fromUtf8("预处理"));
    m_btnPreprocess->setObjectName("dsPlaceholderBtn");
    m_btnPreprocess->setCursor(Qt::PointingHandCursor);
    m_btnPreprocess->setEnabled(false);  // 灰显
    m_btnPreprocess->setToolTip(QString::fromUtf8("预处理功能开发中…"));
    connect(m_btnPreprocess, &QPushButton::clicked, this, &DatasetManagement::onPreprocessClicked);
    tbLayout->addWidget(m_btnPreprocess);

    // ── 占位按钮：数据增强（Phase 1 预留，功能待实现）──
    m_btnAugment = new QPushButton(QString::fromUtf8("数据增强"));
    m_btnAugment->setObjectName("dsPlaceholderBtn");
    m_btnAugment->setCursor(Qt::PointingHandCursor);
    m_btnAugment->setEnabled(false);  // 灰显
    m_btnAugment->setToolTip(QString::fromUtf8("数据增强功能开发中…"));
    connect(m_btnAugment, &QPushButton::clicked, this, &DatasetManagement::onAugmentClicked);
    tbLayout->addWidget(m_btnAugment);

    QFrame* sep3 = new QFrame;
    sep3->setFrameShape(QFrame::VLine);
    sep3->setObjectName("dsTbSep");
    tbLayout->addWidget(sep3);

    // ── 导出按钮 ──
    m_btnExport = new QPushButton(QString::fromUtf8("导出"));
    m_btnExport->setObjectName("dsExportBtn");
    m_btnExport->setCursor(Qt::PointingHandCursor);
    m_btnExport->setToolTip(QString::fromUtf8("导出数据集（COCO JSON / YOLO 格式）"));
    connect(m_btnExport, &QPushButton::clicked, this, &DatasetManagement::onExportDataset);
    tbLayout->addWidget(m_btnExport);

    rightLayout->addWidget(toolbar);

    // ── 画廊滚动区（内含 StackedWidget：空态 / 内容态）──
m_galleryScrollArea = new QScrollArea;
    m_galleryScrollArea->setWidgetResizable(true);
    m_galleryScrollArea->setFrameShape(QFrame::NoFrame);
    m_galleryScrollArea->setObjectName("dsGalleryScroll");
    m_galleryScrollArea->installEventFilter(this);  // 监听宽度变化以重新排列网格

    m_galleryStack = new QStackedWidget;
    m_galleryStack->setObjectName("dsGalleryStack");

    // ═══ Page 0：空态 — 居中导入入口 ═══
    m_emptyPage = new QWidget;
    m_emptyPage->setObjectName("dsEmptyPage");
    QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(16);

    emptyLayout->addStretch(2);

    QLabel* emptyIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x81"));
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyIcon->setStyleSheet("font-size: 48px;");
    emptyLayout->addWidget(emptyIcon);

    QLabel* emptyTitle = new QLabel(QString::fromUtf8("暂无数据集"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyTitle->setObjectName("dsEmptyTitle");
    emptyLayout->addWidget(emptyTitle);

    QLabel* emptyDesc = new QLabel(QString::fromUtf8(
        "导入图像与标签文件，或选择包含 images/ + labels/ 的数据集文件夹"));
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyDesc->setWordWrap(true);
    emptyDesc->setObjectName("dsEmptyDesc");
    emptyLayout->addWidget(emptyDesc);

    m_btnEmptyImport = new QPushButton(QString::fromUtf8("  导入数据集文件"));
    m_btnEmptyImport->setIcon(QIcon(":/res/NewProject.png"));
    m_btnEmptyImport->setIconSize(QSize(20, 20));
    m_btnEmptyImport->setCursor(Qt::PointingHandCursor);
    m_btnEmptyImport->setObjectName("dsEmptyImportBtn");
    m_btnEmptyImport->setFixedSize(180, 40);
    connect(m_btnEmptyImport, &QPushButton::clicked, this, &DatasetManagement::onImportImages);

    m_btnEmptyImportFolder = new QPushButton(QString::fromUtf8("  导入数据集文件夹"));
    m_btnEmptyImportFolder->setIcon(QIcon(":/res/OpenProject.png"));
    m_btnEmptyImportFolder->setIconSize(QSize(20, 20));
    m_btnEmptyImportFolder->setCursor(Qt::PointingHandCursor);
    m_btnEmptyImportFolder->setObjectName("dsEmptyImportFolderBtn");
    m_btnEmptyImportFolder->setFixedSize(180, 40);
    connect(m_btnEmptyImportFolder, &QPushButton::clicked, this, &DatasetManagement::onImportFolder);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(16);
    btnRow->setAlignment(Qt::AlignCenter);
    btnRow->addWidget(m_btnEmptyImport);
    btnRow->addWidget(m_btnEmptyImportFolder);
    emptyLayout->addLayout(btnRow);

    m_galleryEmptyHint = new QLabel;
    m_galleryEmptyHint->setAlignment(Qt::AlignCenter);
    m_galleryEmptyHint->setObjectName("dsEmptyHint");
    m_galleryEmptyHint->setWordWrap(true);
    m_galleryEmptyHint->setText(QString::fromUtf8("\xf0\x9f\x9f\xa1 未导入图像"));
    emptyLayout->addWidget(m_galleryEmptyHint);

    emptyLayout->addStretch(3);

    m_galleryStack->addWidget(m_emptyPage);  // index 0

    // ═══ Page 1：正常内容态 — 缩略图网格 ═══
    m_galleryPage = new QWidget;
    m_galleryPage->setObjectName("dsGalleryPage");
    QVBoxLayout* galleryPageLayout = new QVBoxLayout(m_galleryPage);
    galleryPageLayout->setContentsMargins(0, 0, 0, 0);
    galleryPageLayout->setSpacing(0);

    // 内部滚动区：包裹网格容器 + 列表视图，解决超过视口高度的内容无法滚动的问题
    QScrollArea* innerScroll = new QScrollArea;
    innerScroll->setWidgetResizable(true);
    innerScroll->setFrameShape(QFrame::NoFrame);
    innerScroll->setObjectName("dsInnerScroll");
    QWidget* innerWidget = new QWidget;
    innerWidget->setObjectName("dsInnerWidget");
    QVBoxLayout* innerLayout = new QVBoxLayout(innerWidget);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(8);

    m_galleryContainer = new QWidget;
    m_galleryContainer->setObjectName("dsGalleryContainer");
    m_galleryContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_galleryLayout = new QGridLayout(m_galleryContainer);
    m_galleryLayout->setContentsMargins(8, 8, 8, 8);
    m_galleryLayout->setSpacing(16);
    // 不设 alignment，让网格填充容器宽度；Qt::AlignTop 保证从顶部开始布局
    m_galleryLayout->setAlignment(Qt::AlignTop);
    innerLayout->addWidget(m_galleryContainer, 0);

    // ── 列表视图 ──
    m_galleryListView = new QListWidget;
    m_galleryListView->setObjectName("dsGalleryList");
    m_galleryListView->setSelectionMode(QAbstractItemView::NoSelection);
    m_galleryListView->setWordWrap(true);
    m_galleryListView->setIconSize(QSize(48, 48));
    m_galleryListView->setSpacing(2);
    m_galleryListView->hide();
    connect(m_galleryListView, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        int idx = item->data(Qt::UserRole).toInt();
        if (item->checkState() == Qt::Checked)
            m_selectedIndices.insert(idx);
        else
            m_selectedIndices.remove(idx);
        updateSelectionUI();
    });
    connect(m_galleryListView, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        int idx = item->data(Qt::UserRole).toInt();
        updateImageInfoPanel(idx);
    });
    innerLayout->addWidget(m_galleryListView, 1);

    innerLayout->addStretch();
    innerScroll->setWidget(innerWidget);
    galleryPageLayout->addWidget(innerScroll, 1);

    m_galleryStack->addWidget(m_galleryPage);  // index 1

    m_galleryScrollArea->setWidget(m_galleryStack);
    rightLayout->addWidget(m_galleryScrollArea, 1);

    // ── 步骤引导条（嵌入右侧底部）──
    m_stepGuideBar = new StepGuideBar;
    m_stepGuideBar->setCurrentStep(2);        // 当前在数据集管理
    m_stepGuideBar->setStepCompleted(0);       // 项目管理已完成
    m_stepGuideBar->setStepCompleted(1);       // 模型管理已完成
    rightLayout->addWidget(m_stepGuideBar);
}

// =============================================================================
// 主题刷新
// =============================================================================

void DatasetManagement::applyTheme()
{
    const auto& p = ThemeManager::instance().palette();
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);

    // 左侧面板背景
    for (int i = 0; i < m_leftSplitter->count(); ++i) {
        QWidget* w = m_leftSplitter->widget(i);
        if (w) w->setStyleSheet(QString("#%1 { background-color: %2; }")
            .arg(w->objectName(), p.bgSecondary));
    }

    // 通用面板标题/值/提示样式
    QString panelTitleQss = QString("QLabel#dsPanelTitle { color: %1; font-weight: bold; }")
        .arg(p.textPrimary);
    QString panelValueQss = QString("QLabel#dsPanelValue { color: %1; font-size: 11px; }")
        .arg(p.textSecondary);
    QString panelPathQss = QString("QLabel#dsPanelPath { color: %1; font-size: 10px;"
        " background-color: %2; border-radius: 4px; padding: 4px; border: 1px solid %3; }")
        .arg(p.accentPrimary, p.bgTertiary, p.borderLight);
    QString hintQss = QString("QLabel#dsHint { color: %1; font-size: 11px; }")
        .arg(p.textMuted);

    // 应用面板通用样式
    QString globalQss = panelTitleQss + panelValueQss + panelPathQss + hintQss;
    setStyleSheet(globalQss);

    // 小按钮样式
    QString smallBtnQss = QString(
        "QPushButton#dsSmallBtn {"
        "  background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 4px 10px; font-size: 11px; }"
        "QPushButton#dsSmallBtn:hover { background-color: %4; }")
        .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.bgHover);
    QString smallBtnDangerQss = QString(
        "QPushButton#dsSmallBtnDanger {"
        "  background-color: transparent; color: %1; border: 1px solid %1;"
        "  border-radius: 4px; padding: 4px 10px; font-size: 11px; }"
        "QPushButton#dsSmallBtnDanger:hover { background-color: %2; }")
        .arg(p.danger, p.bgHover);

    // 空态导入按钮（主按钮）
    if (m_btnEmptyImport) {
        m_btnEmptyImport->setStyleSheet(
            QString("QPushButton#dsEmptyImportBtn {"
                    "  background-color: %1; color: #ffffff; border: none;"
                    "  border-radius: 8px; padding: 14px 32px; font-size: 14px;"
                    "  font-weight: bold; }"
                    "QPushButton#dsEmptyImportBtn:hover { background-color: %2; }")
                .arg(p.accentPrimary, p.accentHover));
        m_btnEmptyImport->setIcon(QIcon(isDark ? ":/res/NewProject_D.png" : ":/res/NewProject.png"));
    }

    // 空态文件夹导入按钮
    if (m_btnEmptyImportFolder) {
        m_btnEmptyImportFolder->setStyleSheet(
            QString("QPushButton#dsEmptyImportFolderBtn {"
                    "  background-color: %1; color: %2; border: 2px solid %3;"
                    "  border-radius: 8px; padding: 10px 24px; font-size: 13px;"
                    "  font-weight: bold; }"
                    "QPushButton#dsEmptyImportFolderBtn:hover { background-color: %4; }")
                .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.bgHover));
    }

    // 工具栏
    QWidget* toolbar = findChild<QWidget*>("dsToolbar");
    if (toolbar)
        toolbar->setStyleSheet(QString("#dsToolbar { background-color: %1; }").arg(p.bgPrimary));

    // 工具栏导入按钮样式
    for (QPushButton* btn : toolbar->findChildren<QPushButton*>()) {
        if (btn->objectName() == "dsToolBtn") {
            btn->setStyleSheet(QString(
                "QPushButton#dsToolBtn {"
                "  background-color: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 4px; padding: 4px 14px; font-size: 11px; font-weight: bold; }"
                "QPushButton#dsToolBtn:hover { background-color: %4; border-color: %5; }")
                .arg(p.accentPrimary, "#ffffff", p.accentPrimary, p.accentHover, p.accentPrimary));
        }
    }

    // QComboBox 样式
    for (QComboBox* cb : toolbar->findChildren<QComboBox*>()) {
        if (cb->objectName() == "dsCombo") {
            cb->setStyleSheet(QString(
                "QComboBox#dsCombo {"
                "  background-color: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 4px; padding: 3px 8px; font-size: 11px; min-width: 80px; }"
                "QComboBox#dsCombo:hover { border-color: %4; }"
                "QComboBox#dsCombo::drop-down {"
                "  subcontrol-origin: padding; subcontrol-position: top right;"
                "  width: 18px; border-left: 1px solid %3; }"
                "QComboBox#dsCombo::down-arrow {"
                "  width: 8px; height: 8px; }"
                "QComboBox#dsCombo QAbstractItemView {"
                "  background-color: %1; color: %2;"
                "  border: 1px solid %3; selection-background-color: %4;"
                "  selection-color: %2; outline: none; padding: 2px; }"
                "QComboBox#dsCombo QAbstractItemView::item {"
                "  padding: 4px 8px; min-height: 20px; }"
                "QComboBox#dsCombo QAbstractItemView::item:hover {"
                "  background-color: %5; }")
                .arg(p.bgSecondary, p.textPrimary, p.borderMedium,
                     p.accentPrimary, p.bgHover));
        }
    }

    // 内部滚动区样式
    for (QScrollArea* sa : findChildren<QScrollArea*>()) {
        if (sa->objectName() == "dsInnerScroll")
            sa->setStyleSheet(QString(
                "QScrollArea#dsInnerScroll { background: transparent; border: none; }"
                "QScrollArea#dsInnerScroll > QWidget { background: transparent; }"
                "QScrollArea#dsInnerScroll QScrollBar:vertical {"
                "  width: 6px; background: transparent; }"
                "QScrollArea#dsInnerScroll QScrollBar::handle:vertical {"
                "  background: %1; border-radius: 3px; min-height: 20px; }"
                "QScrollArea#dsInnerScroll QScrollBar::add-line:vertical,"
                "QScrollArea#dsInnerScroll QScrollBar::sub-line:vertical { height: 0; }")
                .arg(p.borderMedium));
    }

    // 占位按钮样式（灰显 + "开发中" badge 感）
    if (m_btnPreprocess) {
        m_btnPreprocess->setStyleSheet(QString(
            "QPushButton#dsPlaceholderBtn {"
            "  background-color: %1; color: %2; border: 1px dashed %3;"
            "  border-radius: 4px; padding: 4px 12px; font-size: 11px; }"
            "QPushButton#dsPlaceholderBtn:hover { border-color: %4; }")
            .arg(p.bgSecondary, p.textMuted, p.borderMedium, p.textSecondary));
    }
    if (m_btnAugment) {
        m_btnAugment->setStyleSheet(QString(
            "QPushButton#dsPlaceholderBtn {"
            "  background-color: %1; color: %2; border: 1px dashed %3;"
            "  border-radius: 4px; padding: 4px 12px; font-size: 11px; }"
            "QPushButton#dsPlaceholderBtn:hover { border-color: %4; }")
            .arg(p.bgSecondary, p.textMuted, p.borderMedium, p.textSecondary));
    }

    // 导出按钮样式
    if (m_btnExport) {
        m_btnExport->setStyleSheet(QString(
            "QPushButton#dsExportBtn {"
            "  background-color: %1; color: #ffffff; border: none;"
            "  border-radius: 4px; padding: 4px 14px; font-size: 11px; font-weight: bold; }"
            "QPushButton#dsExportBtn:hover { background-color: %2; }")
            .arg(p.accentPrimary, p.accentHover));
    }

    // 统计标签
    QString statLabelQss = QString("QLabel#dsStatLabel { color: %1; font-size: 12px; }")
        .arg(p.textSecondary);
    QString statValueQss = QString("QLabel#dsStatValue { color: %1; font-size: 12px;"
                                    " font-weight: bold; }")
        .arg(p.textPrimary);

    // 全选复选框 & 已选计数
    if (m_selectAllCheck)
        m_selectAllCheck->setStyleSheet(
            QString("QCheckBox#dsSelectAll { color: %1; font-size: 12px; }").arg(p.textPrimary));
    if (m_lblSelectedCount)
        m_lblSelectedCount->setStyleSheet(
            QString("QLabel#dsSelectedCount { color: %1; font-size: 11px; }").arg(p.textMuted));

    // 分隔线
    QString sepQss = QString("QFrame#dsTbSep { color: %1; }").arg(p.borderLight);

    // ComboBox
    QString comboQss = QString(
        "QComboBox#dsCombo {"
        "  background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 4px 8px; font-size: 12px; min-width: 80px; }"
        "QComboBox#dsCombo:hover { border-color: %4; }"
        "QComboBox#dsCombo QAbstractItemView {"
        "  background-color: %1; color: %2; selection-background-color: %5; }")
        .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.accentPrimary, p.bgSelected);

    // 画廊 & 空态页
    m_galleryScrollArea->setStyleSheet(
        QString("QScrollArea#dsGalleryScroll { border: none; background-color: %1; }"
                "QScrollArea#dsGalleryScroll > QWidget { background-color: %1; }")
            .arg(p.bgPrimary));
    // 直接设置 viewport 背景（QSS 子选择器可能穿透不足）
    if (m_galleryScrollArea->viewport()) {
        m_galleryScrollArea->viewport()->setAutoFillBackground(true);
        m_galleryScrollArea->viewport()->setStyleSheet(
            QString("background-color: %1;").arg(p.bgPrimary));
    }
    m_galleryStack->setStyleSheet(
        QString("#dsGalleryStack { background-color: %1; }").arg(p.bgPrimary));
    m_emptyPage->setStyleSheet(
        QString("#dsEmptyPage { background-color: %1; }").arg(p.bgPrimary));
    m_galleryPage->setStyleSheet(
        QString("#dsGalleryPage { background-color: %1; }").arg(p.bgPrimary));
    m_galleryContainer->setStyleSheet(
        QString("#dsGalleryContainer { background-color: %1; }").arg(p.bgPrimary));

    // 列表视图
    if (m_galleryListView) {
        m_galleryListView->setStyleSheet(QString(
            "QListWidget#dsGalleryList { background-color: %1; color: %2; border: none; }"
            "QListWidget#dsGalleryList::item { padding: 6px; border-bottom: 1px solid %3; }"
            "QListWidget#dsGalleryList::item:hover { background-color: %4; }")
            .arg(p.bgPrimary, p.textPrimary, p.borderLight, p.bgSecondary));
    }

    // 空态文字
    QString emptyTitleQss = QString("QLabel#dsEmptyTitle { color: %1; font-size: 22px; font-weight: bold; }")
        .arg(p.textPrimary);
    QString emptyDescQss = QString("QLabel#dsEmptyDesc { color: %1; font-size: 13px; }")
        .arg(p.textMuted);
    QString emptyHintQss = QString("QLabel#dsEmptyHint { color: %1; font-size: 13px; font-weight: bold; }")
        .arg(p.textMuted);

    // 空态大按钮
    if (m_btnEmptyImport) {
        m_btnEmptyImport->setStyleSheet(
            QString("QPushButton#dsEmptyImportBtn {"
                    "  background-color: %1; color: #ffffff; border: none;"
                    "  border-radius: 8px; padding: 10px 24px; font-size: 15px;"
                    "  font-weight: bold; }"
                    "QPushButton#dsEmptyImportBtn:hover { background-color: %2; }")
                .arg(p.accentPrimary, p.accentHover));
        m_btnEmptyImport->setIcon(QIcon(isDark ? ":/res/NewProject_D.png" : ":/res/NewProject.png"));
    }

    // 合并全局样式（含自己背景）
    QString selfBg = QString("DatasetManagement { background-color: %1; }").arg(p.bgPrimary);
    QString additionalQss = selfBg + emptyTitleQss + emptyDescQss + emptyHintQss
        + comboQss + sepQss + smallBtnQss + smallBtnDangerQss
        + statLabelQss + statValueQss;
    QString finalQss = globalQss + additionalQss;
    setStyleSheet(finalQss);

    // ── 标签列表 ──
    m_labelListWidget->setStyleSheet(
        QString("QListWidget#dsLabelList {"
                "  background-color: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 4px; font-size: 11px; }"
                "QListWidget#dsLabelList::item {"
                "  padding: 0px; margin: 0px; height: 24px; }"
                "QListWidget#dsLabelList::item:selected {"
                "  background-color: %4; }")
            .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.bgSelected));

    // ── 右侧容器背景 ──
    if (m_rightContainer)
        m_rightContainer->setStyleSheet(
            QString("#dsRightContainer { background-color: %1; }").arg(p.bgPrimary));

    // ── 工具栏背景 ──
    for (QWidget* w : findChildren<QWidget*>()) {
        if (w->objectName() == "dsToolbar")
            w->setStyleSheet(QString(
                "#dsToolbar { background-color: %1; border-bottom: 1px solid %2; }")
                .arg(p.bgPrimary, p.borderLight));
    }

    // ── 内部滚动容器的内容区背景 ──
    for (QWidget* w : findChildren<QWidget*>()) {
        if (w->objectName() == "dsInnerWidget") {
            w->setAutoFillBackground(true);
            w->setStyleSheet(
                QString("#dsInnerWidget { background-color: %1; }").arg(p.bgPrimary));
        }
    }

    // ── 工具栏主按钮 ──
    for (QPushButton* btn : findChildren<QPushButton*>()) {
        if (btn->objectName() == "dsToolBtn")
            btn->setStyleSheet(QString(
                "QPushButton#dsToolBtn {"
                "  background-color: %1; color: %2; border: 1px solid %3;"
                "  border-radius: 4px; padding: 4px 14px; font-size: 12px; }"
                "QPushButton#dsToolBtn:hover { background-color: %4; border-color: %5; }")
                .arg(p.accentPrimary, "#ffffff", p.accentPrimary, p.accentHover, p.accentHover));
    }

    // ── 删除选中按钮 ──
    if (m_btnDeleteSelected)
        m_btnDeleteSelected->setStyleSheet(QString(
            "QPushButton#dsDeleteBtn {"
            "  background: transparent; color: %1; border: 1px solid %1;"
            "  border-radius: 4px; padding: 2px 10px; font-size: 11px; }"
            "QPushButton#dsDeleteBtn:hover { background: %1; color: #ffffff; }"
            "QPushButton#dsDeleteBtn:disabled { color: %2; border-color: %2; }")
            .arg(p.danger, p.textMuted));

    // ── 质量告警指示器 ──
    if (m_lblQualityWarn)
        m_lblQualityWarn->setStyleSheet(QString(
            "QLabel#dsQualityWarn { color: %1; font-size: 11px; font-weight: bold;"
            " padding: 2px 8px; border: 1px solid %1; border-radius: 3px; }")
            .arg(p.warning));

    // 刷新所有缩略图主题
    for (QFrame* f : m_thumbnailFrames) {
        f->setStyleSheet(QString("QFrame#dsThumb { background-color: %1;"
                                  " border: 1px solid %2; border-radius: 6px; }")
            .arg(p.bgSecondary, p.borderLight));
    }
}

// =============================================================================
// 画廊刷新
// =============================================================================

void DatasetManagement::refreshGallery()
{
    // ── 防重入：processEvents 可能触发 resize → 递归调用 ──
    if (m_inRefresh) return;
    m_inRefresh = true;

    const auto& p = ThemeManager::instance().palette();

    // ── 清除旧缩略图 ──
    for (QFrame* f : m_thumbnailFrames) {
        if (f->parentWidget())
            m_galleryLayout->removeWidget(f);
        f->deleteLater();
    }
    m_thumbnailFrames.clear();

    // ── 清除列表视图旧项目 ──
    if (m_galleryListView)
        m_galleryListView->clear();

    int filter = m_filterCombo ? m_filterCombo->currentIndex() : 0;

    // 构建可见索引列表
    QList<int> visibleIndices;
    for (int i = 0; i < m_samples.size(); ++i) {
        bool annotated = m_samples[i].annotated;
        if (filter == 1 && annotated) continue;   // 只看未标注
        if (filter == 2 && !annotated) continue;   // 只看已标注
        visibleIndices.append(i);
    }

    // ── 网格视图（缩略图） ──
    // 动态列数：ScrollArea 宽度 / 216（缩略图200 + 间距16），最少 1 列，默认 4 列
    int containerW = m_galleryScrollArea ? m_galleryScrollArea->width() : 864;
    if (containerW < 216) containerW = 864;  // 未铺开时用默认 4 列宽度
    int perCol = 200 + 16;
    int cols = qMax(1, containerW / perCol);
    int col = 0, row = 0;

    int totalVisible = visibleIndices.size();
    const int kBatchSize = 1;  // 逐张加载，缩略图逐个出现

    // ── 防篡改：记录快照大小，循环内检测 m_samples 是否被 processEvents 中修改 ──
    const int snapshotSize = m_samples.size();

    // ── 立即切换到画廊页，让用户看到缩略图逐个填充 ──
    if (m_galleryStack && totalVisible > 0)
        m_galleryStack->setCurrentIndex(1);

    for (int vi = 0; vi < totalVisible; ++vi) {
        int i = visibleIndices[vi];
        const DatasetSample& s = m_samples[i];

        QFrame* thumbFrame = new QFrame;
        thumbFrame->setFixedSize(200, 220);
        thumbFrame->setObjectName("dsThumb");
        thumbFrame->setCursor(Qt::PointingHandCursor);

        QVBoxLayout* thumbLayout = new QVBoxLayout(thumbFrame);
        thumbLayout->setContentsMargins(4, 4, 4, 4);
        thumbLayout->setSpacing(2);

        // 图像容器
        QWidget* imgContainer = new QWidget;
        imgContainer->setFixedSize(190, 160);
        imgContainer->setStyleSheet(QString("background: transparent;"));

        // 图片
        QLabel* imgLabel = new QLabel(imgContainer);
        imgLabel->setFixedSize(190, 160);
        imgLabel->setAlignment(Qt::AlignCenter);
        imgLabel->setObjectName("dsThumbImg");
        imgLabel->setStyleSheet(QString("background: transparent;"));

        QString imgPath = s.absolutePath;
        QPixmap pix;
        if (QFileInfo::exists(imgPath) && QFileInfo(imgPath).size() > 0) {
            QImage img;
            QString lastError;

            // 诊断：首次加载时打印支持的图像格式
            static bool s_diagPrinted = false;
            if (!s_diagPrinted) {
                s_diagPrinted = true;
                QStringList fmts;
                for (const QByteArray& f : QImageReader::supportedImageFormats())
                    fmts << QString::fromLatin1(f);
                Logger::info(QString::fromUtf8(
                    "QImageReader 支持格式: %1").arg(fmts.join(", ")));
                if (fmts.isEmpty())
                    Logger::warn(QString::fromUtf8(
                        "⚠ 未检测到任何图像格式插件！请确保 imageformats/ 目录与 exe 同级"));
            }

            // 尝试 1：QImageReader 自动检测 + 缩放
            {
                QImageReader reader(imgPath);
                reader.setAutoTransform(true);
                reader.setScaledSize(QSize(190, 160));
                img = reader.read();
                if (!img.isNull()) goto have_image;
                lastError = reader.errorString();
            }

            // 尝试 2：QImageReader 不缩放（部分格式 setScaledSize 会失败）
            {
                QImageReader readerNS(imgPath);
                readerNS.setAutoTransform(true);
                QImage raw = readerNS.read();
                if (!raw.isNull()) {
                    img = raw.scaled(190, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    if (!img.isNull()) goto have_image;
                }
            }

            // 尝试 3：QImage 全量加载
            {
                QImage fallback(imgPath);
                if (!fallback.isNull()) {
                    img = fallback.scaled(190, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    if (!img.isNull()) goto have_image;
                }
            }

            // 尝试 4：强制 jpg 格式
            {
                QImageReader readerJpg(imgPath);
                readerJpg.setAutoDetectImageFormat(false);
                readerJpg.setFormat("jpg");
                readerJpg.setScaledSize(QSize(190, 160));
                img = readerJpg.read();
                if (!img.isNull()) goto have_image;
            }

            // 尝试 5：强制 jpeg 格式
            {
                QImageReader readerJpeg(imgPath);
                readerJpeg.setAutoDetectImageFormat(false);
                readerJpeg.setFormat("jpeg");
                readerJpeg.setScaledSize(QSize(190, 160));
                img = readerJpeg.read();
                if (!img.isNull()) goto have_image;
                lastError = readerJpeg.errorString();
            }

            Logger::warn(QString::fromUtf8("无法解码图像: %1 (%2)")
                .arg(s.fileName, lastError.isEmpty()
                    ? QString::fromUtf8("所有解码器均失败 — 请检查 imageformats/ 插件")
                    : lastError));

        have_image:
            if (!img.isNull()) {
                pix = QPixmap::fromImage(img);
            }
        }
        if (!pix.isNull()) {
            imgLabel->setPixmap(pix);
        } else {
            imgLabel->setText(QString::fromUtf8("无预览"));
            imgLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(p.textMuted));
        }

        // 复选框 — 左上角
        QCheckBox* chk = new QCheckBox(imgContainer);
        chk->setObjectName("dsThumbChk");
        chk->move(4, 4);
        chk->setFixedSize(18, 18);
        bool isSelected = m_selectedIndices.contains(i);
        chk->setChecked(isSelected);

        // 标签色块 — 右下角
        // 规则：
        //   0 标签 → 不显示色块
        //   1-3 标签 → 逐个显示小色块，颜色与标签一致
        //   4+ 标签 或 色块面积 > 预览区 1/3 → 折叠为混合色块
        const QList<int>& catIds = s.categoryIds;
        int nCats = catIds.size();
        if (nCats > 0) {
            // 每个色块 14×14，最大容纳 3 列
            const int chipSize = 14;
            const int chipGap  = 3;
            int maxChips = 9;  // 3×3 grid max
            int maxCols  = 3;
            // 预览区 190×160，1/3 面积 ≈ 190×53 ≈ 10070 px²
            // 每 chip 14×14 = 196，加上间距 ≈ 250
            int chipsByArea = (190 * 160 / 3) / (chipSize * chipSize + chipGap * chipGap);
            int maxByArea = qMax(1, chipsByArea);

            bool fold = (nCats > maxChips || nCats > maxByArea);
            int visibleChips = fold ? 1 : nCats;

            for (int ci = 0; ci < visibleChips; ++ci) {
                int cid = catIds[ci];
                QString chipColor = p.success;
                if (cid >= 0 && cid < m_labelColors.size())
                    chipColor = m_labelColors[cid];

                QString chipText;
                QString chipTooltip;
                if (fold) {
                    // 混合色块：显示 "+N"，列出全部类别
                    chipText = QString("+%1").arg(nCats);
                    QStringList names;
                    for (int c : catIds) {
                        names.append(m_labelNames.value(c, QString::fromUtf8("未知%1").arg(c)));
                    }
                    chipTooltip = names.join(", ");
                } else {
                    chipText = QString::number(cid);
                    chipTooltip = QString::fromUtf8("类别 %1: %2")
                        .arg(cid).arg(m_labelNames.value(cid, QString::fromUtf8("未知")));
                }

                QLabel* chip = new QLabel(imgContainer);
                chip->setFixedSize(chipSize, chipSize);
                chip->setAlignment(Qt::AlignCenter);
                // 从右下角向左上排列
                int chipX = 186 - (ci % maxCols) * (chipSize + chipGap) - chipSize;
                int chipY = 156 - (ci / maxCols) * (chipSize + chipGap) - chipSize;
                chip->move(chipX, chipY);
                chip->setToolTip(chipTooltip);

                // 用 QPixmap 绘制色块 — 避免 QLabel stylesheet 在父容器未完成布局时不生效
                QPixmap chipBg(chipSize, chipSize);
                chipBg.fill(Qt::transparent);
                QPainter painter(&chipBg);
                painter.setRenderHint(QPainter::Antialiasing, true);
                if (fold) {
                    painter.setBrush(QColor(128, 128, 128, 180));
                    painter.setPen(QPen(Qt::white, 2));
                } else {
                    painter.setBrush(QColor(chipColor));
                    painter.setPen(QPen(Qt::white, 2));
                }
                painter.drawRoundedRect(1, 1, chipSize - 2, chipSize - 2, 4, 4);
                painter.end();
                chip->setPixmap(chipBg);

                // 文本叠加 — 用富文本或单独 label
                QLabel* chipLabel = new QLabel(chip);
                chipLabel->setFixedSize(chipSize, chipSize);
                chipLabel->setAlignment(Qt::AlignCenter);
                chipLabel->setText(chipText);
                chipLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
                chipLabel->setStyleSheet(
                    QString("color: white; font-size: %1px; font-weight: bold; background: transparent;")
                        .arg(fold ? 7 : 8));
                chipLabel->move(0, 0);
                chipLabel->show();
            }
        }

        thumbLayout->addWidget(imgContainer);

        // 文件名
        QLabel* nameLabel = new QLabel(s.fileName);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setObjectName("dsThumbName");
        nameLabel->setWordWrap(true);
        nameLabel->setFixedHeight(28);
        nameLabel->setStyleSheet(QString("QLabel#dsThumbName { color: %1; font-size: 10px; }")
            .arg(p.textPrimary));
        thumbLayout->addWidget(nameLabel);

        // 高亮样式：复选框选中 (selected) / 单击激活 (active) 两套独立视觉
        bool isActive = (i == m_activeThumbnailIdx);
        if (isSelected) {
            // 复选框勾选 → 蓝色选中底 + 强调色边框
            thumbFrame->setStyleSheet(QString(
                "QFrame#dsThumb { background: %1; border: 2px solid %2; border-radius: 6px; }")
                .arg(p.bgSelected, p.accentPrimary));
        } else if (isActive) {
            // 单击激活 → 三级底色 + 强调色边框（区别于选中态）
            thumbFrame->setStyleSheet(QString(
                "QFrame#dsThumb { background: %1; border: 2px solid %2; border-radius: 6px; }"
                "QFrame#dsThumb:hover { border-color: %2; }")
                .arg(p.bgTertiary, p.accentPrimary));
        } else {
            thumbFrame->setStyleSheet(QString(
                "QFrame#dsThumb { background: %1; border: 2px solid transparent; border-radius: 6px; }"
                "QFrame#dsThumb:hover { border-color: %2; }")
                .arg(p.bgSecondary, p.borderMedium));
        }

        thumbFrame->setProperty("imageIndex", i);
        thumbFrame->installEventFilter(this);

        connect(chk, &QCheckBox::toggled, this, [this, i](bool checked) {
            onThumbnailCheckChanged(i, checked);
        });

        m_galleryLayout->addWidget(thumbFrame, row, col);
        m_thumbnailFrames.append(thumbFrame);

        col++;
        if (col >= cols) { col = 0; row++; }

        // ── 逐张渲染：每张让出事件循环，缩略图逐个出现 ──
        if (vi + 1 < totalVisible) {
            QApplication::processEvents();
            // ── 安全网：processEvents 中可能被篡改 m_samples，检测并中止 ──
            if (m_samples.size() != snapshotSize) {
                Logger::warn(QString::fromUtf8(
                    "refreshGallery 中止：m_samples 在 processEvents 中被修改 (%1 → %2)")
                    .arg(snapshotSize).arg(m_samples.size()));
                m_inRefresh = false;
                return;
            }
        }
    }

    // ── 让网格列均匀拉伸以填满容器宽度 ──
    for (int c = 0; c < cols; ++c)
        m_galleryLayout->setColumnStretch(c, 1);

    // ── 列表视图 ──
    if (m_galleryListView) {
        m_galleryListView->blockSignals(true);
        for (int vi = 0; vi < visibleIndices.size(); ++vi) {
            int i = visibleIndices[vi];
            const DatasetSample& s = m_samples[i];

            QString labelText;
            int primaryCat = s.primaryCategory();
            if (s.annotated && primaryCat >= 0
                && primaryCat < m_labelNames.size()) {
                labelText = QString::fromUtf8("%1  [%2]").arg(s.fileName, m_labelNames[primaryCat]);
            } else {
                labelText = s.fileName;
            }

            QListWidgetItem* item = new QListWidgetItem(labelText);
            item->setData(Qt::UserRole, i);
            item->setFlags(Qt::ItemIsEnabled);

            // 缩略图（每次尝试新 reader，因为 QImageReader::read() 只能调用一次）
            QPixmap thumbPix;
            if (QFileInfo::exists(s.absolutePath) && QFileInfo(s.absolutePath).size() > 0) {
                QImage img;
                // 尝试 1：QImageReader 自动检测
                {
                    QImageReader reader(s.absolutePath);
                    reader.setAutoTransform(true);
                    reader.setScaledSize(QSize(48, 48));
                    img = reader.read();
                }
                // 尝试 2：QImage 全量加载
                if (img.isNull()) {
                    QImage fallback(s.absolutePath);
                    if (!fallback.isNull())
                        img = fallback.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                // 尝试 3：新建 reader，强制 jpg 格式
                if (img.isNull()) {
                    QImageReader reader2(s.absolutePath);
                    reader2.setAutoDetectImageFormat(false);
                    reader2.setFormat("jpg");
                    reader2.setScaledSize(QSize(48, 48));
                    img = reader2.read();
                }
                if (!img.isNull())
                    thumbPix = QPixmap::fromImage(img);
            }
            if (!thumbPix.isNull())
                item->setIcon(QIcon(thumbPix));

            // 标签色块装饰 — 使用主类别颜色
            int primaryCat2 = s.primaryCategory();
            if (s.annotated && primaryCat2 >= 0
                && primaryCat2 < m_labelColors.size()) {
                QColor c(m_labelColors[primaryCat2]);
                item->setForeground(c);
            }

            // 复选框
            item->setCheckState(m_selectedIndices.contains(i) ? Qt::Checked : Qt::Unchecked);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

            m_galleryListView->addItem(item);
        }
        m_galleryListView->blockSignals(false);
    }

    // ── 视图模式切换 ──
    int viewMode = m_viewModeCombo ? m_viewModeCombo->currentIndex() : 0;
    if (m_galleryContainer)
        m_galleryContainer->setVisible(viewMode == 0);
    if (m_galleryListView)
        m_galleryListView->setVisible(viewMode == 1);

    // 切换画廊页
    if (m_galleryStack) {
        bool hasImages = (m_samples.size() > 0);
        m_galleryStack->setCurrentIndex(hasImages ? 1 : 0);
    }

    // 更新统计数据
    DatasetStats stats = computeStats();
    runQualityCheck(stats);
    updateStatsDisplay(stats);

    updateLabelPanel();

    // ── 强制刷新 — 色块等子控件需要显式 repaint ──
    if (m_galleryContainer) {
        m_galleryContainer->update();
        m_galleryContainer->repaint();
    }
    if (m_galleryScrollArea && m_galleryScrollArea->viewport())
        m_galleryScrollArea->viewport()->update();

    m_inRefresh = false;
}

// ── 事件过滤器：处理缩略图点击 + 网格容器宽度变化 ───────────

bool DatasetManagement::eventFilter(QObject* obj, QEvent* event)
{
    // 外层滚动区宽度变化 → 重新排列缩略图列数
    if (obj == m_galleryScrollArea && event->type() == QEvent::Resize) {
        int w = m_galleryScrollArea->width();
        if (w > 0 && w != m_lastGalleryWidth) {
            m_lastGalleryWidth = w;
            QSet<int> saved = m_selectedIndices;
            refreshGallery();
            m_selectedIndices = saved;
            updateSelectionUI();
        }
        return false;
    }

    for (int i = 0; i < m_thumbnailFrames.size(); ++i) {
        if (obj == m_thumbnailFrames[i]) {
            int imgIdx = m_thumbnailFrames[i]->property("imageIndex").toInt();

            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    onThumbnailClicked(imgIdx);
                } else if (me->button() == Qt::RightButton) {
                    // 右键仅弹菜单，不刷新面板（避免不必要的标注读取）
                    showContextMenu(me->globalPos(), i);
                }
                return true;
            }
            if (event->type() == QEvent::MouseButtonDblClick) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    onThumbnailDoubleClicked(imgIdx);
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── 右键菜单 ─────────────────────────────────────────────────

void DatasetManagement::showContextMenu(const QPoint& pos, int thumbIdx)
{
    QMenu menu(this);
    const auto& p = ThemeManager::instance().palette();
    menu.setStyleSheet(
        QString("QMenu { background-color: %1; color: %2; border: 1px solid %3;"
                " border-radius: 6px; padding: 4px; }"
                "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
                "QMenu::item:selected { background-color: %4; }")
            .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.bgSelected));

    int idx = m_thumbnailFrames[thumbIdx]->property("imageIndex").toInt();
    if (idx < 0 || idx >= m_samples.size()) return;
    QString imgPath = m_samples[idx].absolutePath;

    QAction* actDelete = menu.addAction(QString::fromUtf8("删除当前图像"));
    QAction* actOpenDir = menu.addAction(QString::fromUtf8("打开文件目录"));
    QAction* actCopyPath = menu.addAction(QString::fromUtf8("复制图像路径"));
    QAction* actReannotate = menu.addAction(QString::fromUtf8("重新标注"));

    QAction* chosen = menu.exec(pos);
    if (!chosen) return;

    if (chosen == actDelete) {
        if (QMessageBox::question(this, QString::fromUtf8("确认删除"),
                QString::fromUtf8("确定要删除图像 \"%1\" 吗？").arg(QFileInfo(imgPath).fileName()))
                == QMessageBox::Yes) {
            QFile::remove(imgPath);
            // 同时删除标注文件
            QFile annFile(QDir(annotationsDir()).absoluteFilePath(
                QFileInfo(imgPath).completeBaseName() + ".json"));
            if (annFile.exists()) annFile.remove();
            m_samples.removeAt(idx);
            saveDatasetConfig();
            refreshGallery();
            Logger::info(QString::fromUtf8("已删除图像: %1").arg(imgPath));
        }
    } else if (chosen == actOpenDir) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(imgPath).absolutePath()));
    } else if (chosen == actCopyPath) {
        QApplication::clipboard()->setText(imgPath);
        Logger::info(QString::fromUtf8("已复制图像路径"));
    } else if (chosen == actReannotate) {
        emit openAnnotationRequested(m_currentModelId, imgPath);
    }
}

// =============================================================================
// 面板更新
// =============================================================================

void DatasetManagement::updateModelInfoPanel()
{
    if (!m_modelNameLabel) return;

    bool hasModel = !m_currentModelId.isEmpty();
    m_modelNameLabel->setText(QString::fromUtf8("模型：%1")
        .arg(hasModel ? m_currentModel.modelName : "--"));
    m_modelTypeLabel->setText(QString::fromUtf8("任务类型：%1")
        .arg(hasModel ? m_currentModel.modelType : "--"));

    QString dsPath = hasModel ? datasetDir() : "--";
    m_datasetPathLabel->setText(QString::fromUtf8("数据集路径：%1").arg(dsPath));

    // 标签格式类型
    if (m_labelTypeLabel) {
        if (!hasModel) {
            m_labelTypeLabel->setText(QString::fromUtf8("标签格式：--"));
        } else if (m_labelType.isEmpty()) {
            QString recommended = LabelTypes::recommendForModelType(m_currentModel.modelType);
            m_labelTypeLabel->setText(QString::fromUtf8("标签格式：未设置（推荐：%1）").arg(recommended));
        } else {
            m_labelTypeLabel->setText(QString::fromUtf8("标签格式：%1").arg(m_labelType));
        }
    }

    // 更新空态提示 & 画廊页面切换
    if (!hasModel) {
        if (m_galleryEmptyHint)
            m_galleryEmptyHint->setText(QString::fromUtf8("\xe2\x9d\x8c 请先在模型管理中选择一个模型"));
        if (m_galleryStack)
            m_galleryStack->setCurrentIndex(0);
    } else {
        int total = m_samples.size();

        if (m_galleryEmptyHint) {
            if (total == 0)
                m_galleryEmptyHint->setText(QString::fromUtf8("\xf0\x9f\x9f\xa1 未导入图像"));
            else
                m_galleryEmptyHint->setText(QString::fromUtf8("\xf0\x9f\x9f\xa2 %1 张图像已导入").arg(total));
        }

        if (m_galleryStack)
            m_galleryStack->setCurrentIndex(total > 0 ? 1 : 0);
    }
}

void DatasetManagement::updateImageInfoPanel(int idx)
{
    if (idx < 0 || idx >= m_samples.size() || !m_imageInfoPanel) {
        if (m_imageInfoPanel) setPanelEnabled(m_imageInfoPanel, false);
        return;
    }

    setPanelEnabled(m_imageInfoPanel, true);

    const DatasetSample& s = m_samples[idx];
    m_imageFileName->setText(QString::fromUtf8("文件名：%1").arg(s.fileName));
    m_imagePathLabel->setText(QString::fromUtf8("路径：%1").arg(s.absolutePath));

    // 分辨率
    QImageReader reader(s.absolutePath);
    QSize size = reader.size();
    if (size.isValid())
        m_imageResolution->setText(QString::fromUtf8("分辨率：%1 x %2").arg(size.width()).arg(size.height()));
    else
        m_imageResolution->setText(QString::fromUtf8("分辨率：未知"));

    // 标注实例数
    QFileInfo fi(s.absolutePath);
    QString annFile = QDir(annotationsDir()).absoluteFilePath(fi.completeBaseName() + ".json");
    int instanceCount = 0;
    QFile af(annFile);
    if (af.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(af.readAll());
        af.close();
        QJsonArray shapes = doc.object().value("shapes").toArray();
        instanceCount = shapes.size();
    }
    m_imageInstances->setText(QString::fromUtf8("标注实例数：%1").arg(instanceCount));
}

void DatasetManagement::updateLabelPanel()
{
    if (!m_labelListWidget) return;
    const auto& p = ThemeManager::instance().palette();
    m_labelListWidget->clear();

    for (int i = 0; i < m_labelNames.size(); ++i) {
        QString color = m_labelColors.value(i, s_defaultColors[i % s_defaultColors.size()]);
        int instCount = m_labelInstanceCounts.value(i, -1);
        bool isEmpty = (instCount == 0);  // 空标签占位

        // 创建紧凑行：[颜色方块] 编号 名称 [实例数或"(空)"]
        QWidget* itemWidget = new QWidget;
        itemWidget->setObjectName("dsLabelItem");
        QHBoxLayout* itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(4, 2, 4, 2);
        itemLayout->setSpacing(6);

        // 颜色方块（空标签虚线边框 + 半透明）
        QLabel* colorBlock = new QLabel;
        colorBlock->setFixedSize(14, 14);
        if (isEmpty)
            colorBlock->setStyleSheet(
                QString("background-color: %1; border: 1px dashed %2;"
                        " border-radius: 2px; opacity: 0.5;")
                    .arg(color, p.borderMedium));
        else
            colorBlock->setStyleSheet(
                QString("background-color: %1; border: 1px solid %2;"
                        " border-radius: 2px;")
                    .arg(color, p.borderMedium));
        itemLayout->addWidget(colorBlock);

        // 编号
        QLabel* idxLabel = new QLabel(QString("%1").arg(i));
        idxLabel->setFixedWidth(20);
        idxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        idxLabel->setStyleSheet(isEmpty
            ? QString("color: %1; font-size: 10px;").arg(p.textMuted)
            : QString("color: %1; font-size: 10px;").arg(p.textSecondary));
        itemLayout->addWidget(idxLabel);

        // 名称（空标签灰显 + "(空)" 后缀）
        QString displayName = m_labelNames[i];
        if (isEmpty) displayName += QString::fromUtf8("  (空)");
        QLabel* nameLabel = new QLabel(displayName);
        nameLabel->setStyleSheet(isEmpty
            ? QString("color: %1; font-size: 11px; font-style: italic;").arg(p.textMuted)
            : QString("color: %1; font-size: 11px;").arg(p.textPrimary));
        itemLayout->addWidget(nameLabel, 1);

        QListWidgetItem* item = new QListWidgetItem(m_labelListWidget);
        item->setSizeHint(QSize(0, 24));
        item->setData(Qt::UserRole, i);
        m_labelListWidget->setItemWidget(item, itemWidget);
    }
}

void DatasetManagement::updateStepGuide()
{
    if (!m_stepGuideBar) return;
    m_stepGuideBar->setStepCompleted(0);
    m_stepGuideBar->setStepCompleted(1);

    bool hasModel = !m_currentModelId.isEmpty();
    if (m_samples.size() > 0)
        m_stepGuideBar->setStepCompleted(2);

    // ── 确保始终可见（双击切标签页时 resize 触发的空 refreshGallery 可能影响布局）──
    m_stepGuideBar->raise();
    m_stepGuideBar->setVisible(true);
}

void DatasetManagement::setPanelEnabled(QWidget* panel, bool enabled)
{
    if (!panel) return;
    panel->setVisible(enabled);
    if (m_imageInfoHint)
        m_imageInfoHint->setVisible(!enabled);
}

// ── 导入前确认标签类型 ──────────────────────────────────────
bool DatasetManagement::ensureLabelTypeBeforeImport()
{
    if (!m_labelType.isEmpty())
        return true;  // 已设置

    // 根据模型类型推荐
    QString recommended = LabelTypes::recommendForModelType(m_currentModel.modelType);

    QStringList types = LabelTypes::all();
    bool ok = false;
    QString selected = QInputDialog::getItem(this,
        QString::fromUtf8("设置标签格式"),
        QString::fromUtf8("导入数据集前请先选择标签格式：\n\n"
            "• 灰度掩码PNG — 实例分割（灰度值 1, 2, 3…=类别）\n"
            "• 二值掩码PNG  — 异常检测（0=正常, 1=异常）\n"
            "• COCO边界框JSON — 物体检测(非自由矩形)\n"
            "• COCO旋转框JSON — 物体检测(自由矩形)"),
        types,
        types.indexOf(recommended) >= 0 ? types.indexOf(recommended) : 0,
        false, &ok);

    if (!ok || selected.isEmpty())
        return false;

    m_labelType = selected;
    saveDatasetConfig();
    updateModelInfoPanel();
    Logger::info(QString::fromUtf8("标签格式已设置为: %1").arg(m_labelType));
    return true;
}

// ── 扫描 annotations 自动识别标签类别（合并模式：保留空标签 + 新增检测到的）──
// 同时扫描 JSON（labelme shapes / COCO annotations）和 PNG 掩码，覆盖所有标注格式
void DatasetManagement::detectLabelsFromAnnotations()
{
    QDir annDir(annotationsDir());
    if (!annDir.exists()) return;

    // 初始化实例计数（保留已有标签，先全部置 0）
    QMap<QString, int> nameToInstances;  // name → 实例数
    for (int i = 0; i < m_labelNames.size(); ++i)
        nameToInstances[m_labelNames[i]] = 0;  // 重置计数，后续重新统计

    // ══════════════════════════════════════════════════
    // Step 1：始终扫描 JSON 标注（labelme shapes + COCO annotations）
    // ══════════════════════════════════════════════════
    QStringList jsonFilters = { "*.json" };
    QFileInfoList jsonFiles = annDir.entryInfoList(jsonFilters, QDir::Files);

    for (const QFileInfo& fi : jsonFiles) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) continue;

        QJsonObject root = doc.object();

        // 从 categories 收集类别名
        QJsonArray cats = root.value("categories").toArray();
        for (const QJsonValue& v : cats) {
            QString name = v.toObject().value("name").toString();
            if (!name.isEmpty() && !nameToInstances.contains(name))
                nameToInstances[name] = 0;
        }

        // 统计实例数：优先 COCO annotations，回退到 labelme shapes
        QJsonArray annots = root.value("annotations").toArray();
        QJsonArray shapes = root.value("shapes").toArray();

        if (!annots.isEmpty()) {
            // COCO annotations
            for (const QJsonValue& av : annots) {
                int cid = av.toObject().value("category_id").toInt(-1);
                if (cid >= 0 && cid < m_labelNames.size())
                    nameToInstances[m_labelNames[cid]]++;
            }
        } else if (!shapes.isEmpty()) {
            // labelme shapes（多边形 / 矩形 / 圆形等）
            for (const QJsonValue& sv : shapes) {
                QJsonObject shape = sv.toObject();
                int cid = shape.value("category_id").toInt(-1);
                if (cid >= 0 && cid < m_labelNames.size())
                    nameToInstances[m_labelNames[cid]]++;
                // label 字段回退
                QString lbl = shape.value("label").toString();
                if (!lbl.isEmpty() && cid < 0)
                    nameToInstances[lbl]++;
            }
        }
    }

    // ══════════════════════════════════════════════════
    // Step 2：掩码类型时，额外扫描 PNG 掩码（按图片数统计，非像素数）
    // ══════════════════════════════════════════════════
    if (LabelTypes::isMaskType(m_labelType)) {
        QMap<int, int> grayToMaskCount;  // 灰度值 → 包含该值的掩码图片数
        QStringList pngFilters = { "*.png" };
        QFileInfoList pngFiles = annDir.entryInfoList(pngFilters, QDir::Files);

        int scanLimit = qMin(pngFiles.size(), 50);
        for (int i = 0; i < scanLimit; ++i) {
            QImage img(pngFiles[i].absoluteFilePath());
            if (img.isNull()) continue;
            QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
            int w = gray.width(), h = gray.height();
            // 采样检测该掩码中包含哪些灰度值，每图去重
            int step = qMax(1, qMin(w, h) / 50);
            QSet<int> uniqueVals;
            for (int y = 0; y < h; y += step) {
                const uchar* line = gray.constScanLine(y);
                for (int x = 0; x < w; x += step) {
                    int v = line[x];
                    if (v > 0 && v < 250) uniqueVals.insert(v);
                }
            }
            for (int gv : uniqueVals)
                grayToMaskCount[gv]++;
        }

        // 灰度值 → 连续索引映射（兼容 Halcon 偏移）
        int maxGray = 0;
        for (auto it = grayToMaskCount.begin(); it != grayToMaskCount.end(); ++it)
            maxGray = qMax(maxGray, it.key());

        // 自动检测偏移：标准掩码 minGray=1, Halcon 掩码 minGray≥2
        int minGray = grayToMaskCount.isEmpty() ? 1 : grayToMaskCount.firstKey();

        // 填充缺失灰度值（缺口 → 空标签占位）
        for (int g = minGray; g <= maxGray; ++g) {
            if (!grayToMaskCount.contains(g))
                grayToMaskCount[g] = 0;
        }

        // 映射：灰度值 g → 索引 g-minGray (0-based)
        QMap<int, int> grayToIndex;
        for (auto it = grayToMaskCount.begin(); it != grayToMaskCount.end(); ++it)
            grayToIndex[it.key()] = it.key() - minGray;

        bool added = false;
        // 按灰度值顺序创建标签（保证索引连续）
        for (int gv = minGray; gv <= maxGray; ++gv) {
            int mappedIdx = grayToIndex[gv];
            while (m_labelNames.size() <= mappedIdx) {
                QString autoName = QString::fromUtf8("class_%1").arg(m_labelNames.size());
                m_labelNames.append(autoName);
                m_labelColors.append(s_defaultColors[(m_labelNames.size() - 1) % s_defaultColors.size()]);
                m_labelInstanceCounts.append(0);
                nameToInstances[autoName] = 0;
                added = true;
            }
            // 实例数 = 包含该灰度值的掩码图片数量
            int maskCount = grayToMaskCount.value(gv, 0);
            m_labelInstanceCounts[mappedIdx] = maskCount;
            nameToInstances[m_labelNames[mappedIdx]] = maskCount;
        }

        if (added) {
            for (auto& s : m_samples) {
                if (s.annotated) {
                    for (int& cid : s.categoryIds) {
                        int grav = cid + minGray;  // 转换回灰度值（考虑 Halcon 偏移）
                        if (grayToIndex.contains(grav))
                            cid = grayToIndex[grav];
                    }
                }
            }
            saveDatasetConfig();
            Logger::info(QString::fromUtf8("从掩码图像自动识别 %1 个类别").arg(grayToMaskCount.size()));
        }
    }

    // ══════════════════════════════════════════════════
    // Step 3：合并 nameToInstances → m_labelNames
    // ══════════════════════════════════════════════════
    bool imported = false;
    for (auto it = nameToInstances.begin(); it != nameToInstances.end(); ++it) {
        const QString& name = it.key();
        if (!m_labelNames.contains(name)) {
            m_labelNames.append(name);
            m_labelColors.append(s_defaultColors[(m_labelNames.size() - 1) % s_defaultColors.size()]);
            m_labelInstanceCounts.append(it.value());
            imported = true;
        } else {
            int idx = m_labelNames.indexOf(name);
            if (idx >= 0 && idx < m_labelInstanceCounts.size())
                m_labelInstanceCounts[idx] = it.value();
        }
    }

    if (imported) {
        saveDatasetConfig();
        Logger::info(QString::fromUtf8("从标注文件自动识别 %1 个新标签类别")
            .arg(nameToInstances.size() - m_labelNames.size() + 1));
    }

    updateLabelPanel();
}

// =============================================================================
// 槽函数
// =============================================================================

void DatasetManagement::onImportImages()
{
    if (m_currentModelId.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在模型管理中选择一个模型。"));
        return;
    }

    // 1. 数据集格式说明
    QMessageBox fmtBox(this);
    fmtBox.setWindowTitle(QString::fromUtf8("数据集格式说明"));
    fmtBox.setText(QString::fromUtf8(
        "数据集需遵循以下格式：\n\n"
        "  • 原图文件 (任何 Qt 支持的图像格式) → 放入 images/\n"
        "  • 标签文件 (*.json 或 *.png)       → 放入 annotations/\n\n"
        "不同模型类型对应不同标签格式：\n"
        "  • 实例分割 → 灰度掩码PNG，灰度值1=类别1, 2=类别2…\n"
        "    例: labels/img_01.png 为单通道灰度图，像素值即类别ID\n"
        "  • 异常检测 → 二值掩码PNG，0=正常, 1=异常\n"
        "    例: labels/img_01.png 为黑白二值图\n"
        "  • 物体检测 → COCO JSON，含 bbox 字段\n"
        "    例: labels/img_01.json 含 {\"bbox\":[x,y,w,h],\"category_id\":1}\n"
        "  • 自由矩形 → COCO JSON，含旋转角度\n"
        "    例: labels/img_01.json 含 {\"bbox\":[cx,cy,w,h,angle],\"category_id\":1}\n\n"
        "建议先导入原图，再导入对应的标签文件。"));
    fmtBox.setIcon(QMessageBox::Information);
    fmtBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    if (fmtBox.exec() != QMessageBox::Ok)
        return;

    // 2. 先导入原图（标签类型延后到实际需要时再确认）
    QStringList imgFiles = QFileDialog::getOpenFileNames(
        this,
        QString::fromUtf8("第一步：选择原图文件"),
        m_dataRoot,
        imageFileFilter());

    // 3. 再导入标签（可选）
    QStringList lblFiles;
    bool importLabels = false;
    if (!imgFiles.isEmpty()) {
        int ret = QMessageBox::question(this,
            QString::fromUtf8("导入标签文件"),
            QString::fromUtf8("已选择 %1 张原图。是否继续导入标签文件？").arg(imgFiles.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        importLabels = (ret == QMessageBox::Yes);

        if (importLabels) {
            // 需要标签时才确认标签格式
            if (!ensureLabelTypeBeforeImport())
                return;
            lblFiles = QFileDialog::getOpenFileNames(
                this,
                QString::fromUtf8("第二步：选择标签文件"),
                m_dataRoot,
                QString::fromUtf8("标签文件 (*.json *.png);;JSON 标签 (*.json);;PNG 掩码 (*.png);;所有文件 (*.*)"));
        }
    }

    if (imgFiles.isEmpty() && lblFiles.isEmpty()) return;

    // 确保目标目录存在
    QDir().mkpath(imagesDir());
    QDir().mkpath(annotationsDir());

    // 检测同名冲突
    QDir dstImagesDir(imagesDir());
    QDir dstAnnDir(annotationsDir());
    QStringList dupImages, dupLabels;
    for (const QString& srcPath : imgFiles) {
        QString fn = QFileInfo(srcPath).fileName();
        if (dstImagesDir.exists(fn)) dupImages << fn;
    }
    for (const QString& srcPath : lblFiles) {
        QString fn = QFileInfo(srcPath).fileName();
        if (dstAnnDir.exists(fn)) dupLabels << fn;
    }
    bool overwrite = true;
    if (!dupImages.isEmpty() || !dupLabels.isEmpty()) {
        QString msg = QString::fromUtf8("以下 %1 个文件已存在，是否覆盖？\n\n").arg(dupImages.size() + dupLabels.size());
        for (const QString& n : dupImages.mid(0, 8)) msg += QString::fromUtf8("  图像: %1\n").arg(n);
        if (dupImages.size() > 8) msg += QString::fromUtf8("  ... 还有 %1 个\n").arg(dupImages.size() - 8);
        for (const QString& n : dupLabels.mid(0, 8)) msg += QString::fromUtf8("  标签: %1\n").arg(n);
        if (dupLabels.size() > 8) msg += QString::fromUtf8("  ... 还有 %1 个\n").arg(dupLabels.size() - 8);
        msg += QString::fromUtf8("\n选「否」将跳过同名文件。");
        int ret = QMessageBox::question(this, QString::fromUtf8("文件冲突"), msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        overwrite = (ret == QMessageBox::Yes);
    }

    int imported = 0, skipped = 0, overwritten = 0, labelsImported = 0;

    // 复制图像（根据 overwrite 决定是否覆盖）
    for (const QString& srcPath : imgFiles) {
        QFileInfo fi(srcPath);
        if (!fi.exists()) continue;
        QString dstPath = QDir(imagesDir()).absoluteFilePath(fi.fileName());
        if (QFile::exists(dstPath)) {
            if (!overwrite) { skipped++; continue; }
            QFile::remove(dstPath);
            overwritten++;
        }
        if (QFile::copy(srcPath, dstPath))
            imported++;
        else
            skipped++;
    }

    // 复制标签
    for (const QString& srcPath : lblFiles) {
        QFileInfo fi(srcPath);
        if (!fi.exists()) continue;
        QString dstPath = QDir(annotationsDir()).absoluteFilePath(fi.fileName());
        if (QFile::exists(dstPath)) {
            if (!overwrite) { skipped++; continue; }
            QFile::remove(dstPath);
            overwritten++;
        }
        if (QFile::copy(srcPath, dstPath))
            labelsImported++;
        else
            skipped++;
    }

    // 重新扫描
    loadDatasetConfig();
    refreshGallery();
    updateModelInfoPanel();
    updateStepGuide();

    // 自动识别标签类别
    if (imported > 0 || labelsImported > 0) {
        detectLabelsFromAnnotations();
        validateLabelCount();
    }

    // 更新项目状态：data_imported = true
    if (imported > 0 && !m_currentModelId.isEmpty()) {
        ProjectConfig::setModelStatus(m_project.fvprojPath, m_currentModelId,
                                      "data_imported", true);
        emit dataImported(m_currentModelId);
    }

    Logger::info(QString::fromUtf8("数据集文件导入完成: 图像 %1 张（覆盖 %2）, 标签 %3 个, 失败 %4")
        .arg(imported).arg(overwritten).arg(labelsImported).arg(skipped));
}

void DatasetManagement::onImportFolder()
{
    if (m_currentModelId.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在模型管理中选择一个模型。"));
        return;
    }

    // 1. 数据集格式说明
    QMessageBox fmtBox(this);
    fmtBox.setWindowTitle(QString::fromUtf8("数据集格式说明"));
    fmtBox.setText(QString::fromUtf8(
        "数据集文件夹需包含以下同级子目录：\n\n"
        "  images/  原始图像 (Qt 支持的图像格式)\n"
        "  labels/  标签文件\n\n"
        "不同模型类型对应不同标签格式：\n"
        "  • 实例分割 → 灰度掩码PNG，灰度值1=类别1, 2=类别2…\n"
        "    例: labels/img_01.png 为单通道灰度图，像素值即类别ID\n"
        "  • 异常检测 → 二值掩码PNG，0=正常, 1=异常\n"
        "    例: labels/img_01.png 为黑白二值图\n"
        "  • 物体检测 → COCO JSON，含 bbox 字段\n"
        "    例: labels/img_01.json 含 {\"bbox\":[x,y,w,h],\"category_id\":1}\n"
        "  • 自由矩形 → COCO JSON，含旋转角度\n"
        "    例: labels/img_01.json 含 {\"bbox\":[cx,cy,w,h,angle],\"category_id\":1}\n\n"
        "请确认目标文件夹已按要求组织好 images/ 和 labels/。"));
    fmtBox.setIcon(QMessageBox::Information);
    fmtBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    if (fmtBox.exec() != QMessageBox::Ok)
        return;

    // 2. 选择文件夹
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        QString::fromUtf8("选择数据集根目录（需包含 images/ 子目录，labels/ 可选）"),
        m_dataRoot);

    if (folderPath.isEmpty()) return;

    QDir rootDir(folderPath);
    QDir srcImagesDir(rootDir.absoluteFilePath("images"));
    QDir srcLabelsDir(rootDir.absoluteFilePath("labels"));

    // images/ 必须存在
    if (!srcImagesDir.exists()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("所选文件夹缺少 images/ 子目录！"));
        return;
    }

    // labels/ 可选：存在时需要确认标签格式
    bool hasLabels = srcLabelsDir.exists();
    if (hasLabels) {
        if (!ensureLabelTypeBeforeImport())
            return;
    }

    // 统计文件列表（硬编码兜底 + 动态格式）
    QStringList imgFilters;
    static const QStringList s_hardcodedImgFmts = {
        "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif",
        "*.webp", "*.gif", "*.ico", "*.pbm", "*.pgm", "*.ppm"
    };
    QSet<QString> imgFilterSet(s_hardcodedImgFmts.begin(), s_hardcodedImgFmts.end());
    for (const QByteArray& fmt : QImageReader::supportedImageFormats())
        imgFilterSet.insert(QString("*.%1").arg(QString::fromLatin1(fmt)));
    imgFilters = QStringList(imgFilterSet.begin(), imgFilterSet.end());
    QFileInfoList imgFiles = srcImagesDir.entryInfoList(imgFilters, QDir::Files);
    QFileInfoList lblFiles = srcLabelsDir.entryInfoList(
        { "*.png", "*.json" }, QDir::Files);

    int totalFiles = imgFiles.size() + lblFiles.size();
    if (totalFiles == 0) {
        QMessageBox::warning(this, QString::fromUtf8("空数据集"),
            QString::fromUtf8("images/ 目录中没有找到有效图像文件。"));
        return;
    }

    // 确保目标目录存在
    QDir().mkpath(imagesDir());
    QDir().mkpath(annotationsDir());

    // 检测同名冲突
    QDir dstImagesDir(imagesDir());
    QDir dstAnnDir(annotationsDir());
    QStringList dupImages, dupLabels;
    for (const QFileInfo& fi : imgFiles) {
        if (dstImagesDir.exists(fi.fileName()))
            dupImages << fi.fileName();
    }
    for (const QFileInfo& fi : lblFiles) {
        if (dstAnnDir.exists(fi.fileName()))
            dupLabels << fi.fileName();
    }
    bool overwrite = true;  // 默认覆盖
    if (!dupImages.isEmpty() || !dupLabels.isEmpty()) {
        QString msg = QString::fromUtf8("以下 %1 个文件已存在，是否覆盖？\n\n").arg(dupImages.size() + dupLabels.size());
        int showMax = 8;
        for (int i = 0; i < qMin(dupImages.size(), showMax); ++i) msg += QString::fromUtf8("  图像: %1\n").arg(dupImages[i]);
        if (dupImages.size() > showMax) msg += QString::fromUtf8("  ... 还有 %1 个图像\n").arg(dupImages.size() - showMax);
        if (!dupImages.isEmpty() && !dupLabels.isEmpty()) msg += "\n";
        int lblShow = qMin(dupLabels.size(), showMax);
        for (int i = 0; i < lblShow; ++i) msg += QString::fromUtf8("  标签: %1\n").arg(dupLabels[i]);
        if (dupLabels.size() > lblShow) msg += QString::fromUtf8("  ... 还有 %1 个标签\n").arg(dupLabels.size() - lblShow);
        msg += QString::fromUtf8("\n选「否」将跳过同名文件。");
        int ret = QMessageBox::question(this, QString::fromUtf8("文件冲突"), msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        overwrite = (ret == QMessageBox::Yes);
    }

    // 进度条
    QProgressDialog progress(
        QString::fromUtf8("正在导入数据集..."), QString::fromUtf8("取消"), 0, totalFiles, this);
    progress.setWindowTitle(QString::fromUtf8("导入进度"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(200);
    progress.setValue(0);

    int imported = 0, skipped = 0, overwritten = 0, labelsImported = 0;
    int progressIdx = 0;

    // 复制 images/
    for (int i = 0; i < imgFiles.size(); ++i) {
        if (progress.wasCanceled()) {
            Logger::warn(QString::fromUtf8("用户取消文件夹导入 (已复制图像 %1 张)").arg(imported));
            break;
        }

        const QFileInfo& fi = imgFiles[i];
        QString dstPath = QDir(imagesDir()).absoluteFilePath(fi.fileName());
        if (QFile::exists(dstPath)) {
            if (!overwrite) { skipped++; continue; }
            QFile::remove(dstPath);
            overwritten++;
        }
        if (QFile::copy(fi.absoluteFilePath(), dstPath)) {
            imported++;
        } else {
            skipped++;
        }

        progressIdx++;
        progress.setValue(progressIdx);
        progress.setLabelText(
            QString::fromUtf8("正在复制图像... (%1/%2)").arg(i + 1).arg(imgFiles.size()));
        QApplication::processEvents();
    }

    // 复制 labels/
    for (int i = 0; i < lblFiles.size(); ++i) {
        if (progress.wasCanceled()) {
            Logger::warn(QString::fromUtf8("用户取消文件夹导入 (已复制标签 %1 个)").arg(labelsImported));
            break;
        }

        const QFileInfo& fi = lblFiles[i];
        QString dstPath = QDir(annotationsDir()).absoluteFilePath(fi.fileName());
        if (QFile::exists(dstPath)) {
            if (!overwrite) { skipped++; continue; }
            QFile::remove(dstPath);
            overwritten++;
        }
        if (QFile::copy(fi.absoluteFilePath(), dstPath))
            labelsImported++;

        progressIdx++;
        progress.setValue(progressIdx);
        progress.setLabelText(
            QString::fromUtf8("正在复制标签... (%1/%2)").arg(i + 1).arg(lblFiles.size()));
        QApplication::processEvents();
    }

    progress.setValue(totalFiles);

    // 检测并处理 labels/ 中的 COCO 格式统一 JSON（images+annotations+categories）
    QDir annDirForCoco(annotationsDir());
    QStringList jsonFiles = annDirForCoco.entryList({ "*.json" }, QDir::Files);
    for (const QString& jf : jsonFiles) {
        QFile f(annDirForCoco.absoluteFilePath(jf));
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (d.isObject()) {
            QJsonObject obj = d.object();
            if (obj.contains("images") && obj.contains("annotations")) {
                // 发现 COCO 格式统一标注文件，拆分为按图像独立 JSON
                parseCocoJson(annDirForCoco.absoluteFilePath(jf));
                break;
            }
        }
    }

    // 重新扫描（必须在 parseCocoJson 之后，因为 COCO 导入可能额外复制图像）
    loadDatasetConfig();

    refreshGallery();
    updateModelInfoPanel();
    updateStepGuide();

    // 自动识别标签类别
    detectLabelsFromAnnotations();
    validateLabelCount();

    // 更新项目状态
    if (imported > 0 && !m_currentModelId.isEmpty()) {
        ProjectConfig::setModelStatus(m_project.fvprojPath, m_currentModelId,
                                      "data_imported", true);
        emit dataImported(m_currentModelId);
    }

    Logger::info(QString::fromUtf8("数据集文件夹导入完成: 图像 %1 张（覆盖 %2）, 标签 %3 个, 失败 %4")
        .arg(imported).arg(overwritten).arg(labelsImported).arg(skipped));
}

// ── 校验 labels 目录实际标签数与配置是否一致 ─────────────────
bool DatasetManagement::validateLabelCount()
{
    QDir annDir(annotationsDir());
    if (!annDir.exists() || m_labelType.isEmpty())
        return true;  // 无需校验

    int configuredCount = m_labelNames.size();
    int actualCount = 0;

    // 从标签数据中提取实际的类别
    if (LabelTypes::isMaskType(m_labelType)) {
        QSet<int> grayValues;
        QStringList pngFiles = annDir.entryList({ "*.png" }, QDir::Files);
        int scanLimit = qMin(pngFiles.size(), 50);
        for (int i = 0; i < scanLimit; ++i) {
            QImage img(annDir.absoluteFilePath(pngFiles[i]));
            if (img.isNull()) continue;
            QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
            int w = gray.width(), h = gray.height();
            int step = qMax(1, qMin(w, h) / 100);
            for (int y = 0; y < h; y += step) {
                const uchar* line = gray.constScanLine(y);
                for (int x = 0; x < w; x += step) {
                    if (line[x] > 0)
                        grayValues.insert(line[x]);
                }
            }
        }
        actualCount = grayValues.size();
    } else if (LabelTypes::isJsonType(m_labelType)) {
        QSet<QString> catNames;
        QStringList jsonFiles = annDir.entryList({ "*.json" }, QDir::Files);
        for (const QString& fname : jsonFiles) {
            QFile f(annDir.absoluteFilePath(fname));
            if (!f.open(QIODevice::ReadOnly)) continue;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            QJsonArray cats = doc.object().value("categories").toArray();
            QJsonArray annots = doc.object().value("annotations").toArray();
            for (const QJsonValue& v : cats)
                catNames.insert(v.toObject().value("name").toString());
            for (const QJsonValue& v : annots) {
                int cid = v.toObject().value("category_id").toInt(-1);
                if (cid >= 0)
                    catNames.insert(QString::number(cid));
            }
        }
        actualCount = catNames.size();
    }

    // 允许空标签：配置类别 > 实际类别 = 正常
    if (actualCount <= configuredCount) {
        if (actualCount < configuredCount) {
            Logger::info(QString::fromUtf8("标签校验通过 (配置 %1 类, 数据含 %2 类, 有空标签)")
                .arg(configuredCount).arg(actualCount));
        } else {
            Logger::info(QString::fromUtf8("标签配置校验通过 (共 %1 类)").arg(configuredCount));
        }
        return true;
    }

    // 实际类别 > 配置：数据中有未定义的类别
    int extra = actualCount - configuredCount;
    int choice = QMessageBox::warning(this,
        QString::fromUtf8("存在未知标签类别"),
        QString::fromUtf8(
            "标签文件中包含 %1 个未在配置中定义的类别。\n\n"
            "当前已配置 %2 类，实际数据含 %3 类。\n\n"
            "• [添加缺失] 根据标签数据补充未定义的类别\n"
            "• [保持不变] 维持现有配置（数据中额外类别将被忽略）\n"
            "• [稍后处理] 稍后手动调整")
            .arg(extra).arg(configuredCount).arg(actualCount),
        QString::fromUtf8("添加缺失"),
        QString::fromUtf8("保持不变"),
        QString::fromUtf8("稍后处理"));

    if (choice == 0) {
        // 扫描 annotations，只添加配置中没有的新类别
        detectLabelsFromAnnotations();
        Logger::info(QString::fromUtf8("标签配置已补充 (从 %1 类 → %2 类)")
            .arg(configuredCount).arg(m_labelNames.size()));
        return true;
    } else if (choice == 1) {
        Logger::info(QString::fromUtf8("标签配置保持不变 (配置 %1 类, 数据含 %2 类)")
            .arg(configuredCount).arg(actualCount));
        return true;
    } else {
        Logger::warn(QString::fromUtf8("标签校验不一致（实际 %1 vs 配置 %2），用户选择稍后处理")
            .arg(actualCount).arg(configuredCount));
        return false;
    }
}

void DatasetManagement::onThumbnailClicked(int idx)
{
    // 单击预览卡：只更新前后两张卡片的样式（不重建整个画廊）
    const auto& p = ThemeManager::instance().palette();
    int prevActive = m_activeThumbnailIdx;

    // 更新激活索引
    if (idx == m_activeThumbnailIdx) {
        m_activeThumbnailIdx = -1;  // toggle off
    } else {
        m_activeThumbnailIdx = idx;
    }

    // 只刷新两张卡片：旧激活 → 取消 / 新激活 → 应用
    auto updateOne = [&](int sampleIdx) {
        for (QFrame* f : m_thumbnailFrames) {
            if (f->property("imageIndex").toInt() == sampleIdx) {
                bool isSel = m_selectedIndices.contains(sampleIdx);
                bool isAct = (sampleIdx == m_activeThumbnailIdx);
                if (isSel) {
                    f->setStyleSheet(QString(
                        "QFrame#dsThumb { background: %1; border: 2px solid %2; border-radius: 6px; }")
                        .arg(p.bgSelected, p.accentPrimary));
                } else if (isAct) {
                    f->setStyleSheet(QString(
                        "QFrame#dsThumb { background: %1; border: 2px solid %2; border-radius: 6px; }"
                        "QFrame#dsThumb:hover { border-color: %2; }")
                        .arg(p.bgTertiary, p.accentPrimary));
                } else {
                    f->setStyleSheet(QString(
                        "QFrame#dsThumb { background: %1; border: 2px solid transparent; border-radius: 6px; }"
                        "QFrame#dsThumb:hover { border-color: %2; }")
                        .arg(p.bgSecondary, p.borderMedium));
                }
                return;
            }
        }
    };

    if (prevActive >= 0) updateOne(prevActive);
    if (m_activeThumbnailIdx >= 0) updateOne(m_activeThumbnailIdx);

    updateImageInfoPanel(idx);
}

void DatasetManagement::onThumbnailDoubleClicked(int idx)
{
    if (idx >= 0 && idx < m_samples.size()) {
        emit openAnnotationRequested(m_currentModelId, m_samples[idx].absolutePath);
    }
}

void DatasetManagement::onViewModeChanged(int mode)
{
    Q_UNUSED(mode);
    m_activeThumbnailIdx = -1;
    refreshGallery();  // 简单刷新，后续可扩展列表视图
}

void DatasetManagement::onFilterChanged(int filter)
{
    Q_UNUSED(filter);
    m_selectedIndices.clear();
    m_activeThumbnailIdx = -1;
    updateSelectionUI();
    refreshGallery();
}

// ── 全选/取消全选（仅选当前可见图像）─────────────────────────
void DatasetManagement::onSelectAll(int state)
{
    m_selectedIndices.clear();
    if (state == Qt::Checked) {
        int filter = m_filterCombo ? m_filterCombo->currentIndex() : 0;
        for (int i = 0; i < m_samples.size(); ++i) {
            bool annotated = m_samples[i].annotated;
            if (filter == 1 && annotated) continue;
            if (filter == 2 && !annotated) continue;
            m_selectedIndices.insert(i);
        }
    }
    updateSelectionUI();
    refreshGallery();
}

// ── 删除选中图像 ──────────────────────────────────────────────
void DatasetManagement::onDeleteSelected()
{
    if (m_selectedIndices.isEmpty()) return;

    int count = m_selectedIndices.size();
    int ret = QMessageBox::question(this,
        QString::fromUtf8("确认删除"),
        QString::fromUtf8("确定要删除选中的 %1 张图像及其关联标签吗？\n此操作不可撤销。").arg(count),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (ret != QMessageBox::Yes) return;

    // 从高索引到低索引删除，避免索引偏移
    QList<int> sortedIndices = m_selectedIndices.values();
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    for (int idx : sortedIndices) {
        // 删除图像文件
        QString imgPath = m_samples[idx].absolutePath;
        QFile::remove(imgPath);

        // 删除关联的标签文件
        QFileInfo fi(imgPath);
        QString baseName = fi.completeBaseName();
        QDir annDir(annotationsDir());
        for (const QString& fname : annDir.entryList(QDir::Files)) {
            QFileInfo afi(annDir.absoluteFilePath(fname));
            if (afi.completeBaseName() == baseName) {
                QFile::remove(afi.absoluteFilePath());
            }
        }

        // 从数据中移除
        m_samples.removeAt(idx);
    }

    m_selectedIndices.clear();
    m_activeThumbnailIdx = -1;
    updateSelectionUI();
    refreshGallery();
    updateModelInfoPanel();

    // 保存配置
    saveDatasetConfig();

    Logger::info(QString::fromUtf8("已删除 %1 张图像").arg(count));
}

// ── 缩略图复选框切换 ──────────────────────────────────────────
void DatasetManagement::onThumbnailCheckChanged(int idx, bool checked)
{
    if (checked)
        m_selectedIndices.insert(idx);
    else
        m_selectedIndices.remove(idx);
    updateSelectionUI();
}

// ── 更新选择 UI 状态 ──────────────────────────────────────────
void DatasetManagement::updateSelectionUI()
{
    const auto& p = ThemeManager::instance().palette();
    int selCount = m_selectedIndices.size();
    int total = m_samples.size();

    if (m_selectAllCheck) {
        m_selectAllCheck->blockSignals(true);
        if (selCount == 0)
            m_selectAllCheck->setCheckState(Qt::Unchecked);
        else if (selCount == total && total > 0)
            m_selectAllCheck->setCheckState(Qt::Checked);
        else
            m_selectAllCheck->setCheckState(Qt::PartiallyChecked);
        m_selectAllCheck->blockSignals(false);

        // 动态更新样式：选中/部分选中时高亮
        QString baseStyle =
            "QCheckBox#dsSelectAll { font-size: 11px; spacing: 4px; }";
        if (selCount == total && total > 0)
            m_selectAllCheck->setStyleSheet(baseStyle +
                QString("QCheckBox#dsSelectAll { color: %1; font-weight: bold; }").arg(p.success));
        else if (selCount > 0)
            m_selectAllCheck->setStyleSheet(baseStyle +
                QString("QCheckBox#dsSelectAll { color: %1; font-weight: bold; }").arg(p.warning));
        else
            m_selectAllCheck->setStyleSheet(baseStyle +
                QString("QCheckBox#dsSelectAll { color: %1; }").arg(p.textMuted));
    }

    if (m_lblSelectedCount)
        m_lblSelectedCount->setText(QString::fromUtf8("已选: %1").arg(selCount));

    if (m_btnDeleteSelected)
        m_btnDeleteSelected->setEnabled(selCount > 0);
}

// ── 标签 CRUD ────────────────────────────────────────────────

void DatasetManagement::onAddLabel()
{
    bool ok;
    QString name = QInputDialog::getText(this, QString::fromUtf8("添加标签"),
        QString::fromUtf8("标签名称："), QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    if (m_labelNames.contains(name)) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("标签 \"%1\" 已存在").arg(name));
        return;
    }

    QColor color = QColorDialog::getColor(
        QColor(s_defaultColors[m_labelNames.size() % s_defaultColors.size()]),
        this, QString::fromUtf8("选择标签颜色"));
    if (!color.isValid()) return;

    m_labelNames.append(name);
    m_labelColors.append(color.name());
    m_labelInstanceCounts.append(0);  // 新标签初始实例数为 0
    saveDatasetConfig();
    updateLabelPanel();

    Logger::info(QString::fromUtf8("已添加标签: %1 [%2]").arg(name, color.name()));
}

void DatasetManagement::onEditLabel()
{
    int row = m_labelListWidget->currentRow();
    if (row < 0 || row >= m_labelNames.size()) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先选择一个标签"));
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, QString::fromUtf8("编辑标签"),
        QString::fromUtf8("标签名称："), QLineEdit::Normal, m_labelNames[row], &ok);
    if (!ok || newName.trimmed().isEmpty()) return;
    newName = newName.trimmed();

    // 检查重名
    for (int i = 0; i < m_labelNames.size(); ++i) {
        if (i != row && m_labelNames[i] == newName) {
            QMessageBox::warning(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("标签 \"%1\" 已存在").arg(newName));
            return;
        }
    }

    m_labelNames[row] = newName;
    saveDatasetConfig();
    updateLabelPanel();
    Logger::info(QString::fromUtf8("已编辑标签: %1").arg(newName));
}

void DatasetManagement::onDeleteLabel()
{
    int row = m_labelListWidget->currentRow();
    if (row < 0 || row >= m_labelNames.size()) {
        QMessageBox::information(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先选择一个标签"));
        return;
    }

    if (QMessageBox::question(this, QString::fromUtf8("确认删除"),
            QString::fromUtf8("确定要删除标签 \"%1\" 吗？\n此操作不可恢复。")
                .arg(m_labelNames[row]))
            != QMessageBox::Yes)
        return;

    QString removed = m_labelNames[row];
    m_labelNames.removeAt(row);
    m_labelColors.removeAt(row);
    if (row < m_labelInstanceCounts.size()) m_labelInstanceCounts.removeAt(row);
    saveDatasetConfig();
    updateLabelPanel();
    Logger::info(QString::fromUtf8("已删除标签: %1").arg(removed));
}

void DatasetManagement::onImportLabels()
{
    QString file = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("导入标签文件"),
        datasetDir(),
        QString::fromUtf8("标签文件 (*.json);;COCO JSON (*.json)"));

    if (file.isEmpty()) return;

    QFileInfo fi(file);
    if (fi.suffix().toLower() == "json") {
        // COCO 格式
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();

        QJsonArray cats = doc.object().value("categories").toArray();
        if (cats.isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("提示"),
                                 QString::fromUtf8("未找到 categories 字段"));
            return;
        }

        int imported = 0;
        for (const QJsonValue& v : cats) {
            QJsonObject cat = v.toObject();
            QString name = cat.value("name").toString();
            if (!name.isEmpty() && !m_labelNames.contains(name)) {
                m_labelNames.append(name);
                m_labelColors.append(cat.value("color").toString(
                    s_defaultColors[m_labelNames.size() % s_defaultColors.size()]));
                m_labelInstanceCounts.append(0);  // 从 JSON 导入的标签初始实例数未知
                imported++;
            }
        }
        if (imported > 0) {
            saveDatasetConfig();
            updateLabelPanel();
            Logger::info(QString::fromUtf8("已从 JSON 导入 %1 个标签").arg(imported));
        }
    }
}

// ── 标签拖拽重排 ──────────────────────────────────────────────
void DatasetManagement::onLabelOrderChanged()
{
    if (!m_labelListWidget) return;
    int count = m_labelListWidget->count();
    if (count != m_labelNames.size()) return;  // 数量不匹配，跳过

    // 读取新顺序（UserRole = 原始索引）
    QList<int> newOrder;
    for (int i = 0; i < count; ++i) {
        QListWidgetItem* item = m_labelListWidget->item(i);
        newOrder.append(item ? item->data(Qt::UserRole).toInt() : i);
    }

    // 检查是否真的发生了变化
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        if (newOrder[i] != i) { changed = true; break; }
    }
    if (!changed) return;

    // 构建 oldIndex → newIndex 映射
    QMap<int, int> remap;  // oldIndex → newIndex
    for (int newIdx = 0; newIdx < count; ++newIdx)
        remap[newOrder[newIdx]] = newIdx;

    // 重新排列 labels
    QStringList newNames(count), newColors(count);
    QList<int>   newCounts(count);
    for (int oldIdx = 0; oldIdx < count; ++oldIdx) {
        int newIdx = remap.value(oldIdx, oldIdx);
        newNames[newIdx]  = m_labelNames[oldIdx];
        newColors[newIdx] = m_labelColors.value(oldIdx, s_defaultColors[oldIdx % s_defaultColors.size()]);
        newCounts[newIdx] = m_labelInstanceCounts.value(oldIdx, 0);
    }
    m_labelNames  = newNames;
    m_labelColors = newColors;
    m_labelInstanceCounts = newCounts;

    // 重映射所有图像的标注索引（全部 categoryIds）
    for (int i = 0; i < m_samples.size(); ++i) {
        for (int& cid : m_samples[i].categoryIds) {
            if (cid >= 0 && cid < count)
                cid = remap.value(cid, cid);
        }
    }

    // 保存并刷新
    saveDatasetConfig();
    updateLabelPanel();
    refreshGallery();
    Logger::info(QString::fromUtf8("标签顺序已更新（共 %1 类）").arg(count));
}

// ── 刷新数据 ──────────────────────────────────────────────────
void DatasetManagement::onRefreshImageInfo()
{
    // 重新加载配置并刷新
    loadDatasetConfig();
    refreshGallery();
    updateModelInfoPanel();
    Logger::info(QString::fromUtf8("数据集已刷新"));
}

// ── 占位按钮：预处理（Phase 1 预留，功能待后续迭代实现）──
void DatasetManagement::onPreprocessClicked()
{
    QMessageBox::information(this,
        QString::fromUtf8("预处理"),
        QString::fromUtf8("预处理功能将在后续版本中实现。\n\n"
            "届时支持：\n"
            "  • 图像尺寸统一（缩放到网络输入大小）\n"
            "  • 像素归一化（mean/std 或 min/max）\n"
            "  • 通道顺序转换（RGB ↔ BGR ↔ Grayscale）\n"
            "  • 分割掩码编码"));
}

// ── 占位按钮：数据增强（Phase 1 预留，功能待后续迭代实现）──
void DatasetManagement::onAugmentClicked()
{
    QMessageBox::information(this,
        QString::fromUtf8("数据增强"),
        QString::fromUtf8("数据增强功能将在后续版本中实现。\n\n"
            "届时支持：\n"
            "  • 几何增强（翻转/旋转/缩放/裁剪）\n"
            "  • 光度增强（亮度/对比度/伽马/Hue）\n"
            "  • 噪声增强（高斯噪声/椒盐噪声）\n"
            "  • 标注同步变换（掩码/边界框跟随增强）"));
}

// =============================================================================
// 数据集导入/导出（Phase 3：COCO JSON + YOLO 格式）
// =============================================================================

// ── 导出入口（弹出格式选择对话框）─────────────────────────────
void DatasetManagement::onExportDataset()
{
    // 多选导出逻辑：有选中项时仅导出选中项
    QList<int> exportIndices;
    if (!m_selectedIndices.isEmpty()) {
        exportIndices = m_selectedIndices.values();
        std::sort(exportIndices.begin(), exportIndices.end());
    } else {
        for (int i = 0; i < m_samples.size(); ++i)
            exportIndices.append(i);
    }

    if (exportIndices.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("没有可导出的数据。"));
        return;
    }

    int exportCount = exportIndices.size();
    QString countHint = exportCount < m_samples.size()
        ? QString::fromUtf8("（已选 %1 / %2 张）").arg(exportCount).arg(m_samples.size())
        : QString::fromUtf8("（全部 %1 张）").arg(exportCount);

    QStringList formats;
    // 掩码类型数据集：PNG 掩码是原生格式，优先提供
    if (LabelTypes::isMaskType(m_labelType))
        formats << QString::fromUtf8("PNG 掩码（images/ + labels/ 目录）— 实例分割标准格式");
    formats << QString::fromUtf8("COCO JSON (*.json)");
    formats << QString::fromUtf8("YOLO 格式（images/ + labels/ 目录）");

    bool ok = false;
    QString choice = QInputDialog::getItem(this,
        QString::fromUtf8("选择导出格式"),
        QString::fromUtf8("请选择导出格式：%1").arg(countHint),
        formats, 0, false, &ok);

    if (!ok || choice.isEmpty()) return;

    if (choice.contains(QString::fromUtf8("PNG 掩码"))) {
        QString dir = QFileDialog::getExistingDirectory(this,
            QString::fromUtf8("选择 PNG 掩码导出目录"),
            m_dataRoot);
        if (dir.isEmpty()) return;
        writeMaskFormat(dir, exportIndices);
    } else if (choice.contains("COCO")) {
        QString savePath = QFileDialog::getSaveFileName(this,
            QString::fromUtf8("导出 COCO JSON"),
            QDir(datasetDir()).absoluteFilePath("annotations.json"),
            QString::fromUtf8("JSON 文件 (*.json)"));
        if (savePath.isEmpty()) return;
        writeCocoJson(savePath, exportIndices);
    } else {
        QString dir = QFileDialog::getExistingDirectory(this,
            QString::fromUtf8("选择 YOLO 导出目录"),
            m_dataRoot);
        if (dir.isEmpty()) return;
        writeYoloFormat(dir, exportIndices);
    }
}

// ── COCO JSON 导出 ──────────────────────────────────────────
void DatasetManagement::writeCocoJson(const QString& outputPath, const QList<int>& exportIndices)
{
    QJsonObject coco;

    bool exportAll = exportIndices.isEmpty();

    // info
    QJsonObject info;
    info["description"] = QString::fromUtf8("FuseVision exported dataset");
    info["version"]     = "1.0";
    info["year"]        = QDate::currentDate().year();
    coco["info"] = info;

    // licenses (placeholder)
    QJsonArray licenses;
    coco["licenses"] = licenses;

    // categories
    QJsonArray cats;
    for (int i = 0; i < m_labelNames.size(); ++i) {
        QJsonObject cat;
        cat["id"]   = i;   // COCO 0-based
        cat["name"] = m_labelNames[i];
        if (m_labelColors.size() > i)
            cat["color"] = m_labelColors[i];
        cats.append(cat);
    }
    coco["categories"] = cats;

    // images — 仅导出选中项（或全部）
    QJsonArray images;
    QMap<QString, int> baseNameToNewId;  // completeBaseName → 新 image_id
    int newImgId = 1;
    int exportCount = exportAll ? m_samples.size() : exportIndices.size();
    for (int ei = 0; ei < exportCount; ++ei) {
        int i = exportAll ? ei : exportIndices[ei];
        if (i < 0 || i >= m_samples.size()) continue;
        const DatasetSample& s = m_samples[i];
        QJsonObject img;
        img["id"]        = newImgId;
        img["file_name"] = s.fileName;
        img["width"]     = s.width  > 0 ? s.width  : 0;
        img["height"]    = s.height > 0 ? s.height : 0;

        // 试试读取真实尺寸
        if (s.width == 0 && QFileInfo::exists(s.absolutePath)) {
            QImageReader reader(s.absolutePath);
            QSize sz = reader.size();
            if (sz.isValid()) {
                img["width"]  = sz.width();
                img["height"] = sz.height();
            }
        }
        images.append(img);
        baseNameToNewId[QFileInfo(s.fileName).completeBaseName()] = newImgId;
        newImgId++;
    }
    coco["images"] = images;

    // annotations — 仅导出选中图像的标注（非全量导出时过滤）
    QJsonArray annotations;
    int annId = 1;
    QDir annDir(annotationsDir());
    if (annDir.exists()) {
        QFileInfoList annFiles = annDir.entryInfoList({"*.json"}, QDir::Files);
        for (const QFileInfo& af : annFiles) {
            QString baseName = af.completeBaseName();
            // 部分导出时，跳过未选中的图像标注
            int imageId = baseNameToNewId.value(baseName, -1);
            if (imageId < 0) continue;

            QFile f(af.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly)) continue;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();

            // 提取 shapes（labelme 格式）或 annotations（COCO 格式）
            QJsonArray shapes = doc.object().value("shapes").toArray();
            QJsonArray existingAnns = doc.object().value("annotations").toArray();

            if (!existingAnns.isEmpty()) {
                // 已经是 COCO 格式，直接合并
                for (const QJsonValue& v : existingAnns) {
                    QJsonObject ann = v.toObject();
                    ann["id"] = annId++;
                    ann["image_id"] = imageId;
                    annotations.append(ann);
                }
            } else {
                // labelme shapes → COCO annotations
                for (const QJsonValue& v : shapes) {
                    QJsonObject shape = v.toObject();
                    QJsonObject ann;
                    ann["id"]       = annId++;
                    ann["image_id"] = imageId;
                    ann["category_id"] = shape.value("category_id").toInt(0);

                    QString label = shape.value("label").toString();
                    if (!label.isEmpty()) {
                        int cid = m_labelNames.indexOf(label);
                        if (cid >= 0) ann["category_id"] = cid;
                        else         ann["category_id"] = 0;
                    }

                    // bbox
                    QJsonArray points = shape.value("points").toArray();
                    if (!points.isEmpty()) {
                        double xmin = 1e9, ymin = 1e9, xmax = -1, ymax = -1;
                        for (const QJsonValue& pv : points) {
                            QJsonArray pt = pv.toArray();
                            double x = pt[0].toDouble(), y = pt[1].toDouble();
                            xmin = qMin(xmin, x); ymin = qMin(ymin, y);
                            xmax = qMax(xmax, x); ymax = qMax(ymax, y);
                        }
                        QJsonArray bbox;
                        bbox << xmin << ymin << (xmax - xmin) << (ymax - ymin);
                        ann["bbox"] = bbox;
                        ann["area"] = (xmax - xmin) * (ymax - ymin);
                    }

                    // segmentation
                    if (shape.contains("points")) {
                        QJsonArray seg;
                        for (const QJsonValue& pv : points) {
                            seg << pv.toArray()[0].toDouble()
                                << pv.toArray()[1].toDouble();
                        }
                        ann["segmentation"] = QJsonArray{seg};
                    }
                    ann["iscrowd"] = 0;
                    annotations.append(ann);
                }
            }
        }
    }

    // ── PNG 掩码 → COCO annotations（掩码类型数据集导出）──
    // 遍历导出图像，对 JSON 标注未覆盖的图像，从 PNG 掩码提取 bbox
    if (LabelTypes::isMaskType(m_labelType)) {
        // 收集已有 JSON 标注覆盖的 image_id
        QSet<int> jsonCovered;
        for (const QJsonValue& v : annotations)
            jsonCovered.insert(v.toObject().value("image_id").toInt(-1));

        for (int ei = 0; ei < exportCount; ++ei) {
            int i = exportAll ? ei : exportIndices[ei];
            if (i < 0 || i >= m_samples.size()) continue;
            const DatasetSample& s = m_samples[i];
            int imageId = baseNameToNewId.value(QFileInfo(s.fileName).completeBaseName(), -1);
            if (imageId < 0 || jsonCovered.contains(imageId)) continue;

            QString maskPath = annDir.absoluteFilePath(
                QFileInfo(s.fileName).completeBaseName() + ".png");
            QImage mask(maskPath);
            if (mask.isNull()) continue;
            QImage gray = mask.convertToFormat(QImage::Format_Grayscale8);
            int mw = gray.width(), mh = gray.height();
            if (mw == 0 || mh == 0) continue;

            // 按灰度值聚合 bbox
            struct BBox { int x1=INT_MAX,y1=INT_MAX,x2=-1,y2=-1; };
            QMap<int, BBox> bboxes;
            for (int y = 0; y < mh; ++y) {
                const uchar* line = gray.constScanLine(y);
                for (int x = 0; x < mw; ++x) {
                    int gv = line[x];
                    if (gv == 0) continue;
                    BBox& b = bboxes[gv];
                    if (x < b.x1) b.x1 = x; if (x > b.x2) b.x2 = x;
                    if (y < b.y1) b.y1 = y; if (y > b.y2) b.y2 = y;
                }
            }

            for (auto it = bboxes.begin(); it != bboxes.end(); ++it) {
                int gv = it.key();
                BBox& b = it.value();
                if (b.x2 < 0) continue;
                int cid = gv - 1;  // 灰度值 → 0-based 类别索引
                if (cid < 0 || cid >= m_labelNames.size()) continue;

                QJsonObject ann;
                ann["id"] = annId++;
                ann["image_id"] = imageId;
                ann["category_id"] = cid;
                double bw = b.x2 - b.x1 + 1, bh = b.y2 - b.y1 + 1;
                QJsonArray bboxArr;
                bboxArr << (double)b.x1 << (double)b.y1 << bw << bh;
                ann["bbox"] = bboxArr;
                ann["area"] = bw * bh;
                ann["iscrowd"] = 0;
                // segmentation: 简化为 bbox 四点
                QJsonArray seg;
                seg << (double)b.x1 << (double)b.y1
                    << (double)b.x2 << (double)b.y1
                    << (double)b.x2 << (double)b.y2
                    << (double)b.x1 << (double)b.y2;
                ann["segmentation"] = QJsonArray{seg};
                annotations.append(ann);
            }
        }
    }
    coco["annotations"] = annotations;

    // 写入文件
    QFile out(outputPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(coco).toJson(QJsonDocument::Indented));
        out.close();
        Logger::info(QString::fromUtf8("COCO JSON 已导出: %1 (%2 图, %3 标注)")
            .arg(outputPath).arg(m_samples.size()).arg(annotations.size()));
        QMessageBox::information(this, QString::fromUtf8("导出成功"),
            QString::fromUtf8("已导出 %1 张图像, %2 个标注到:\n%3")
                .arg(m_samples.size()).arg(annotations.size()).arg(outputPath));
    } else {
        Logger::error(QString::fromUtf8("COCO JSON 导出失败: %1").arg(outputPath));
        QMessageBox::critical(this, QString::fromUtf8("导出失败"),
            QString::fromUtf8("无法写入文件: %1").arg(outputPath));
    }
}

// ── YOLO 格式导出 ────────────────────────────────────────────
void DatasetManagement::writeYoloFormat(const QString& outputDir, const QList<int>& exportIndices)
{
    QDir root(outputDir);
    root.mkpath("images/train");
    root.mkpath("labels/train");

    bool exportAll = exportIndices.isEmpty();
    int exportCount = exportAll ? m_samples.size() : exportIndices.size();

    // 导出类别列表 data.yaml
    QString yamlPath = root.absoluteFilePath("data.yaml");
    QFile yf(yamlPath);
    if (yf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&yf);
        ts << QString::fromUtf8("# FuseVision exported YOLO dataset\n");
        ts << QString("path: %1\n").arg(outputDir);
        ts << "train: images/train\n";
        ts << "nc: " << m_labelNames.size() << "\n";
        ts << "names:\n";
        for (int i = 0; i < m_labelNames.size(); ++i)
            ts << QString("  %1: %2\n").arg(i).arg(m_labelNames[i]);
        yf.close();
    }

    int exported = 0, labelsExported = 0;
    QDir annDir(annotationsDir());

    for (int ei = 0; ei < exportCount; ++ei) {
        int i = exportAll ? ei : exportIndices[ei];
        if (i < 0 || i >= m_samples.size()) continue;
        const DatasetSample& s = m_samples[i];
        // 复制图像
        QString dstImg = root.absoluteFilePath("images/train/" + s.fileName);
        if (!QFile::exists(dstImg))
            QFile::copy(s.absolutePath, dstImg);

        // 生成 YOLO 标注
        QString baseName = QFileInfo(s.absolutePath).completeBaseName();

        // 尝试读取 JSON 标注（labelme 格式）
        QString jsonAnn = annDir.absoluteFilePath(baseName + ".json");
        QFile jf(jsonAnn);
        QJsonArray shapes;
        if (jf.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());
            jf.close();
            shapes = doc.object().value("shapes").toArray();
        }

        // 同时尝试直接读取已有的 COCO annotations
        QJsonArray existingAnns;
        if (shapes.isEmpty()) {
            jf.setFileName(jsonAnn);
            if (jf.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());
                jf.close();
                existingAnns = doc.object().value("annotations").toArray();
            }
        }

        // PNG 掩码回退：JSON 无标注时从灰度掩码提取 bbox
        QList<QPair<int, QRectF>> maskBoxes;  // (categoryId, normalized_bbox)
        if (shapes.isEmpty() && existingAnns.isEmpty()
            && LabelTypes::isMaskType(m_labelType)) {
            QString maskPath = annDir.absoluteFilePath(baseName + ".png");
            QImage mask(maskPath);
            if (!mask.isNull()) {
                QImage gray = mask.convertToFormat(QImage::Format_Grayscale8);
                int mw = gray.width(), mh = gray.height();
                if (mw > 0 && mh > 0) {
                    struct MBox { int x1=INT_MAX,y1=INT_MAX,x2=-1,y2=-1; };
                    QMap<int, MBox> boxes;
                    for (int y = 0; y < mh; ++y) {
                        const uchar* line = gray.constScanLine(y);
                        for (int x = 0; x < mw; ++x) {
                            int gv = line[x];
                            if (gv == 0) continue;
                            MBox& b = boxes[gv];
                            if (x < b.x1) b.x1 = x; if (x > b.x2) b.x2 = x;
                            if (y < b.y1) b.y1 = y; if (y > b.y2) b.y2 = y;
                        }
                    }
                    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
                        int gv = it.key();
                        MBox& b = it.value();
                        if (b.x2 < 0) continue;
                        int cid = gv - 1;
                        if (cid < 0 || cid >= m_labelNames.size()) continue;
                        double bw = b.x2 - b.x1 + 1, bh = b.y2 - b.y1 + 1;
                        maskBoxes.append({cid, QRectF((double)b.x1, (double)b.y1, bw, bh)});
                    }
                }
            }
        }

        // YOLO 标注：始终创建 label 文件（无标注 = 空文件，即负样本/无缺陷，用于降低过检）
        {
            QString dstLabel = root.absoluteFilePath("labels/train/" + baseName + ".txt");
            QFile lf(dstLabel);
            if (lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&lf);

                if (!shapes.isEmpty() || !existingAnns.isEmpty() || !maskBoxes.isEmpty()) {
                    // 获取图像尺寸
                    double imgW = s.width, imgH = s.height;
                    if (imgW == 0 && QFileInfo::exists(s.absolutePath)) {
                        QImageReader reader(s.absolutePath);
                        QSize sz = reader.size();
                        if (sz.isValid()) { imgW = sz.width(); imgH = sz.height(); }
                    }
                    if (imgW == 0) { imgW = 640; imgH = 640; }

                    // 处理 shapes
                    for (const QJsonValue& v : shapes) {
                        QJsonObject shape = v.toObject();
                        int cid = shape.value("category_id").toInt(0);
                        QString label = shape.value("label").toString();
                        if (!label.isEmpty()) {
                            int found = m_labelNames.indexOf(label);
                            if (found >= 0) cid = found;
                        }

                        QJsonArray points = shape.value("points").toArray();
                        if (points.size() >= 2) {
                            double xmin = 1e9, ymin = 1e9, xmax = -1, ymax = -1;
                            for (const QJsonValue& pv : points) {
                                double x = pv.toArray()[0].toDouble();
                                double y = pv.toArray()[1].toDouble();
                                xmin = qMin(xmin, x); ymin = qMin(ymin, y);
                                xmax = qMax(xmax, x); ymax = qMax(ymax, y);
                            }
                            double cx = (xmin + xmax) / 2.0 / imgW;
                            double cy = (ymin + ymax) / 2.0 / imgH;
                            double w  = (xmax - xmin) / imgW;
                            double h  = (ymax - ymin) / imgH;
                            ts << QString("%1 %2 %3 %4 %5\n")
                                .arg(cid).arg(cx, 0, 'f', 6).arg(cy, 0, 'f', 6)
                                .arg(w,  0, 'f', 6).arg(h,  0, 'f', 6);
                        }
                    }

                    // 处理已有 COCO annotations
                    for (const QJsonValue& v : existingAnns) {
                        QJsonObject ann = v.toObject();
                        int cid = ann.value("category_id").toInt(0);
                        QJsonArray bbox = ann.value("bbox").toArray();
                        if (bbox.size() >= 4) {
                            double x = bbox[0].toDouble(), y = bbox[1].toDouble();
                            double w = bbox[2].toDouble(), h = bbox[3].toDouble();
                            double cx = (x + w / 2.0) / imgW;
                            double cy = (y + h / 2.0) / imgH;
                            double nw = w / imgW;
                            double nh = h / imgH;
                            ts << QString("%1 %2 %3 %4 %5\n")
                                .arg(cid).arg(cx, 0, 'f', 6).arg(cy, 0, 'f', 6)
                                .arg(nw, 0, 'f', 6).arg(nh, 0, 'f', 6);
                        }
                    }

                    // 处理 PNG 掩码提取的 bbox
                    for (const auto& [cid, rect] : maskBoxes) {
                        double cx = (rect.x() + rect.width() / 2.0) / imgW;
                        double cy = (rect.y() + rect.height() / 2.0) / imgH;
                        double nw = rect.width() / imgW;
                        double nh = rect.height() / imgH;
                        ts << QString("%1 %2 %3 %4 %5\n")
                            .arg(cid).arg(cx, 0, 'f', 6).arg(cy, 0, 'f', 6)
                            .arg(nw, 0, 'f', 6).arg(nh, 0, 'f', 6);
                    }
                }
                // 无标注 → 空文件（负样本）

                lf.close();
                labelsExported++;
            }
        }
        exported++;
    }

    Logger::info(QString::fromUtf8("YOLO 格式已导出: %1 (%2 图, %3 标签)")
        .arg(outputDir).arg(exported).arg(labelsExported));
    QMessageBox::information(this, QString::fromUtf8("导出成功"),
        QString::fromUtf8("YOLO 格式已导出到:\n%1\n\n图像: %2, 标签文件: %3\n类别: %4")
            .arg(outputDir).arg(exported).arg(labelsExported).arg(m_labelNames.size()));
}

// ── PNG 掩码导出（实例分割标准格式：images/ + labels/ 目录）──
void DatasetManagement::writeMaskFormat(const QString& outputDir, const QList<int>& exportIndices)
{
    QDir root(outputDir);
    root.mkpath("images");
    root.mkpath("labels");

    bool exportAll = exportIndices.isEmpty();
    int exportCount = exportAll ? m_samples.size() : exportIndices.size();
    QDir annDir(annotationsDir());

    int exported = 0, masksExported = 0;
    for (int ei = 0; ei < exportCount; ++ei) {
        int i = exportAll ? ei : exportIndices[ei];
        if (i < 0 || i >= m_samples.size()) continue;
        const DatasetSample& s = m_samples[i];

        // 复制原图
        QString dstImg = root.absoluteFilePath("images/" + s.fileName);
        if (!QFile::exists(dstImg))
            QFile::copy(s.absolutePath, dstImg);
        exported++;

        // 复制对应 PNG 掩码（按 baseName 匹配）
        QString baseName = QFileInfo(s.fileName).completeBaseName();
        QStringList pngMasks = annDir.entryList(
            { baseName + ".png", baseName + "_*.png" }, QDir::Files);
        for (const QString& maskFn : pngMasks) {
            QString dstMask = root.absoluteFilePath("labels/" + maskFn);
            if (!QFile::exists(dstMask)) {
                QFile::copy(annDir.absoluteFilePath(maskFn), dstMask);
                masksExported++;
            }
        }
    }

    Logger::info(QString::fromUtf8("PNG 掩码已导出: %1 (%2 图, %3 掩码)")
        .arg(outputDir).arg(exported).arg(masksExported));
    QMessageBox::information(this, QString::fromUtf8("导出成功"),
        QString::fromUtf8("PNG 掩码格式已导出到:\n%1\n\n图像: %2, 掩码: %3")
            .arg(outputDir).arg(exported).arg(masksExported));
}

// ── COCO JSON 导入 ──────────────────────────────────────────
bool DatasetManagement::parseCocoJson(const QString& jsonPath)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        Logger::warn(QString::fromUtf8("COCO JSON 无法打开: %1").arg(jsonPath));
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject()) {
        Logger::warn(QString::fromUtf8("COCO JSON 格式无效: %1").arg(jsonPath));
        return false;
    }

    QJsonObject coco = doc.object();
    QJsonArray cocoImages = coco.value("images").toArray();
    QJsonArray cocoAnns   = coco.value("annotations").toArray();
    QJsonArray cocoCats   = coco.value("categories").toArray();

    if (cocoImages.isEmpty()) {
        Logger::warn(QString::fromUtf8("COCO JSON 中没有 images 字段: %1").arg(jsonPath));
        return false;
    }

    // 确保目标目录存在
    QDir().mkpath(imagesDir());
    QDir().mkpath(annotationsDir());

    // 导入类别
    for (const QJsonValue& v : cocoCats) {
        QJsonObject cat = v.toObject();
        QString name = cat.value("name").toString();
        if (!name.isEmpty() && !m_labelNames.contains(name)) {
            m_labelNames.append(name);
            m_labelColors.append(cat.value("color").toString(
                s_defaultColors[(m_labelNames.size() - 1) % s_defaultColors.size()]));
            m_labelInstanceCounts.append(0);  // 实例数待后续统计
        }
    }
    if (!cocoCats.isEmpty()) saveDatasetConfig();

    // 构建 image_id → fileName 映射
    QMap<int, QString> idToFileName;
    QMap<int, int> idToWidth, idToHeight;
    for (const QJsonValue& v : cocoImages) {
        QJsonObject img = v.toObject();
        int id = img.value("id").toInt();
        idToFileName[id] = img.value("file_name").toString();
        idToWidth[id]    = img.value("width").toInt(0);
        idToHeight[id]   = img.value("height").toInt(0);
    }

    // 按 image_id 分组 annotations
    QMap<int, QJsonArray> annsByImage;
    for (const QJsonValue& v : cocoAnns) {
        QJsonObject ann = v.toObject();
        int imageId = ann.value("image_id").toInt();
        annsByImage[imageId].append(ann);
    }

    // 导入 — 先从 images/ 目录查找，找不到再从 JSON 同级目录复制
    int imported = 0, imagesCopied = 0, labelsWritten = 0;
    QDir imgDir(imagesDir());
    QDir jsonDir = QFileInfo(jsonPath).absoluteDir();  // JSON 文件所在目录

    for (auto it = idToFileName.begin(); it != idToFileName.end(); ++it) {
        int imageId = it.key();
        QString fileName = it.value();

        // 1. 先在 images/ 目录中查找
        QString imgPath = imgDir.absoluteFilePath(fileName);
        if (!QFile::exists(imgPath)) {
            // 2. 尝试从 JSON 同级目录查找并复制
            QString srcPath = jsonDir.absoluteFilePath(fileName);
            if (QFile::exists(srcPath)) {
                if (QFile::copy(srcPath, imgPath)) {
                    imagesCopied++;
                    Logger::info(QString::fromUtf8("COCO 导入: 已复制图像 %1").arg(fileName));
                } else {
                    Logger::warn(QString::fromUtf8("COCO 导入: 复制图像失败: %1").arg(fileName));
                    continue;
                }
            } else {
                // 3. 都不存在则跳过
                Logger::warn(QString::fromUtf8("COCO 导入: 图像不存在，跳过: %1").arg(fileName));
                continue;
            }
        }
        imported++;

        // 写入该图像的标注 JSON（labelme 兼容格式）
        QJsonArray anns = annsByImage.value(imageId);
        if (!anns.isEmpty()) {
            QJsonObject labelmeAnn;
            labelmeAnn["version"] = "5.0.1";
            labelmeAnn["flags"]   = QJsonObject();

            QJsonArray shapes;
            for (const QJsonValue& av : anns) {
                QJsonObject ann = av.toObject();
                QJsonObject shape;
                shape["label"] = QString::fromUtf8("object");

                int cid = ann.value("category_id").toInt(0);
                shape["category_id"] = cid;
                if (cid >= 0 && cid < m_labelNames.size())
                    shape["label"] = m_labelNames[cid];

                // bbox → 矩形 points
                QJsonArray bbox = ann.value("bbox").toArray();
                if (bbox.size() >= 4) {
                    double x = bbox[0].toDouble(), y = bbox[1].toDouble();
                    double w = bbox[2].toDouble(), h = bbox[3].toDouble();
                    QJsonArray pts;
                    pts << QJsonArray{x, y}
                        << QJsonArray{x + w, y}
                        << QJsonArray{x + w, y + h}
                        << QJsonArray{x, y + h};
                    shape["points"]      = pts;
                    shape["shape_type"]  = "rectangle";
                }

                // segmentation → polygon points
                QJsonArray seg = ann.value("segmentation").toArray();
                if (!seg.isEmpty() && seg[0].isArray()) {
                    QJsonArray poly = seg[0].toArray();
                    QJsonArray pts;
                    for (int i = 0; i < poly.size(); i += 2) {
                        pts << QJsonArray{poly[i].toDouble(), poly[i+1].toDouble()};
                    }
                    shape["points"]      = pts;
                    shape["shape_type"]  = "polygon";
                }

                shapes.append(shape);
            }

            // 同时写入 COCO annotations（保留原始数据）
            labelmeAnn["shapes"]      = shapes;
            labelmeAnn["annotations"] = anns;  // 保留原始 COCO annotations

            labelmeAnn["imagePath"]   = fileName;
            labelmeAnn["imageData"]   = QJsonValue::Null;
            labelmeAnn["imageHeight"] = idToHeight.value(imageId, 0);
            labelmeAnn["imageWidth"]  = idToWidth.value(imageId, 0);

            QString baseName = QFileInfo(fileName).completeBaseName();
            QString annPath  = QDir(annotationsDir()).absoluteFilePath(baseName + ".json");
            QFile af(annPath);
            if (af.open(QIODevice::WriteOnly)) {
                af.write(QJsonDocument(labelmeAnn).toJson(QJsonDocument::Indented));
                af.close();
                labelsWritten++;
            }
        }
    }

    Logger::info(QString::fromUtf8("COCO JSON 拆分为每图独立标注: 图像 %1 张, 标注 %2 个, 类别 %3 个")
        .arg(imported).arg(labelsWritten).arg(m_labelNames.size()));

    return imported > 0;
}

// =============================================================================
// 数据集统计与质量检查（Phase 2）
// =============================================================================

// ── 计算数据集统计 ───────────────────────────────────────────
DatasetManagement::DatasetStats DatasetManagement::computeStats() const
{
    DatasetStats stats;

    stats.totalImages    = m_samples.size();
    stats.categoryCount  = m_labelNames.size();

    for (const auto& s : m_samples) {
        if (s.annotated) {
            stats.annotatedImages++;
            stats.totalInstances += s.instanceCount;
        } else {
            stats.unannotatedImages++;
        }
    }

    stats.annotRate = stats.totalImages > 0
        ? (double)stats.annotatedImages / stats.totalImages * 100.0 : 0.0;

    return stats;
}

// ── 运行质量检查 ─────────────────────────────────────────────
void DatasetManagement::runQualityCheck(DatasetStats& stats) const
{
    stats.warnings.clear();
    stats.missingAnnotations = 0;
    stats.orphanAnnotations  = 0;
    stats.corruptedFiles     = 0;
    stats.emptyAnnotations   = 0;

    if (m_samples.isEmpty()) return;

    QDir imgDir(imagesDir());
    QDir annDir(annotationsDir());

    // 1. 检查有图无标注
    for (const auto& s : m_samples) {
        if (!s.annotated) {
            stats.missingAnnotations++;
        } else {
            // 2. 检查标注是否为空
            QString baseName = QFileInfo(s.absolutePath).completeBaseName();
            QString annPath = annDir.absoluteFilePath(baseName + ".json");
            QFile af(annPath);
            if (af.open(QIODevice::ReadOnly)) {
                QByteArray data = af.readAll();
                af.close();
                if (data.trimmed().isEmpty() || data == "{}") {
                    stats.emptyAnnotations++;
                    stats.warnings.append(QString::fromUtf8("空标注: %1").arg(s.fileName));
                }
            }

            // 3. 检查图像文件是否可读
            QImageReader reader(s.absolutePath);
            if (!reader.canRead()) {
                stats.corruptedFiles++;
                stats.warnings.append(QString::fromUtf8("损坏图像: %1").arg(s.fileName));
            }
        }
    }

    // 4. 检查孤立的标注文件
    if (annDir.exists()) {
        QSet<QString> sampleBaseNames;
        for (const auto& s : m_samples)
            sampleBaseNames.insert(QFileInfo(s.absolutePath).completeBaseName());

        QFileInfoList annFiles = annDir.entryInfoList({"*.json", "*.png"}, QDir::Files);
        for (const QFileInfo& fi : annFiles) {
            if (!sampleBaseNames.contains(fi.completeBaseName())) {
                stats.orphanAnnotations++;
                stats.warnings.append(QString::fromUtf8("孤立标注: %1").arg(fi.fileName()));
            }
        }
    }

    // 汇总告警
    if (stats.missingAnnotations > 0)
        stats.warnings.prepend(QString::fromUtf8("%1 张图像未标注").arg(stats.missingAnnotations));
    if (stats.emptyAnnotations > 0)
        stats.warnings.prepend(QString::fromUtf8("%1 个空标注文件").arg(stats.emptyAnnotations));
    if (stats.corruptedFiles > 0)
        stats.warnings.prepend(QString::fromUtf8("%1 个损坏文件").arg(stats.corruptedFiles));
    if (stats.orphanAnnotations > 0)
        stats.warnings.prepend(QString::fromUtf8("%1 个孤立标注文件").arg(stats.orphanAnnotations));
}

// ── 更新统计显示 ─────────────────────────────────────────────
void DatasetManagement::updateStatsDisplay(const DatasetStats& stats)
{
    if (m_lblTotalImages)    m_lblTotalImages->setText(QString::number(stats.totalImages));
    if (m_lblAnnotatedCount) m_lblAnnotatedCount->setText(
        QString("%1 (%2%)").arg(stats.annotatedImages).arg(stats.annotRate, 0, 'f', 1));
    if (m_lblInstanceCount)  m_lblInstanceCount->setText(QString::number(stats.totalInstances));
    if (m_lblLabelCount) {
        int nonEmpty = 0;
        for (int c : m_labelInstanceCounts) { if (c > 0) nonEmpty++; }
        m_lblLabelCount->setText(QString("%1 (%2非空)").arg(stats.categoryCount).arg(nonEmpty));
    }

    if (m_lblQualityWarn) {
        const auto& p = ThemeManager::instance().palette();
        int totalIssues = stats.missingAnnotations + stats.orphanAnnotations
                        + stats.corruptedFiles + stats.emptyAnnotations;
        if (totalIssues > 0) {
            m_lblQualityWarn->setText(QString::fromUtf8("\u26a0 %1").arg(totalIssues));
            m_lblQualityWarn->setStyleSheet(
                QString("QLabel#dsQualityWarn { color: %1; font-size: 11px; font-weight: bold;"
                        " padding: 2px 8px; border: 1px solid %1; border-radius: 3px; }")
                    .arg(p.warning));
            m_lblQualityWarn->setToolTip(stats.warnings.join("\n"));
            m_lblQualityWarn->show();
        } else {
            m_lblQualityWarn->setText(QString::fromUtf8("\u2713 无问题"));
            m_lblQualityWarn->setStyleSheet(
                QString("QLabel#dsQualityWarn { color: %1; font-size: 11px;"
                        " padding: 2px 8px; }")
                    .arg(p.success));
            m_lblQualityWarn->setToolTip(QString::fromUtf8("数据集质量良好"));
            m_lblQualityWarn->show();
        }
    }
}