#include "deeplearning/storage/DatasetFileService.h"
#include "core/RecoveryStore.h"
#include <QTemporaryDir>
#include "deeplearning/storage/AnnotationStore.h"
#include "core/AtomicFile.h"
#include "deeplearning/storage/StorageContract.h"
#include "ui/WriteAccess.h"
#include "DatasetManagement.h"
#include "core/Logger.h"
#include "ui/ThemeManager.h"
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
#include <QDirIterator>
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
#include <QRegularExpression>
#include <QTableWidget>
#include <QHeaderView>

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

// ── 统一色块绘制（所有色块共用同一套参数）────────────────
static QPixmap makeColorChip(const QColor& fill, int size, const QColor& border, int borderW, int radius)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(fill);
    p.setPen(QPen(border, borderW));
    p.drawRoundedRect(1, 1, size - 2, size - 2, radius, radius);
    p.end();
    return pm;
}

// =============================================================================
// 构造 / 析构
// =============================================================================

DatasetManagement::DatasetManagement(QWidget* parent)
    : QWidget(parent)
{
    initUI();
    AccessControl::watch(this,[this]() { applyPermissions(); });
    applyPermissions();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &DatasetManagement::applyTheme);

    // 防抖定时器：避免窗口缩放时连续触发 refreshGallery 导致卡死/闪退
    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    m_resizeDebounce->setInterval(150);
    connect(m_resizeDebounce, &QTimer::timeout, this, [this]() {
        if (m_inRefresh) return;
        QSet<int> saved = m_selectedIndices;
        refreshGallery();
        m_selectedIndices = saved;
        updateSelectionUI();
    });
}

// ── 绑定项目和模型 ────────────────────────────────────────────

void DatasetManagement::setCurrentProject(const FvprojInfo& project)
{
    m_project      = project;
    m_projectRoot  = project.projectRoot;
}

void DatasetManagement::setCurrentModel(const QString& modelId, const FvdlInfo& model)
{
    const auto generation=++m_modelGeneration;
    // ── 防重入：refreshGallery 正在逐张加载时，第二个单击/双击会经 processEvents 放行 ──
    if (m_inRefresh) {
        QTimer::singleShot(50,this,[this,generation,modelId,model]() {
            if(generation==m_modelGeneration) setCurrentModel(modelId,model);
        });
        return;
    }

    m_currentModelId = modelId;
    m_currentModel   = model;
    m_labelType.clear();
    m_classification=false;
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
    if(modelId.isEmpty()) {
        m_loadDeferTimer=nullptr;
        updateModelInfoPanel();updateLabelPanel();refreshGallery();applyPermissions();return;
    }
    m_loadDeferTimer = new QTimer(this);
    m_loadDeferTimer->setSingleShot(true);
    connect(m_loadDeferTimer, &QTimer::timeout, this, [this,generation]() {
        if(generation!=m_modelGeneration) return;
        m_loadingOverlay->showOverlay();
        loadDatasetConfig();
        refreshGallery();
        // Opening is read-only; persist derived statistics after successful writes.
        updateModelInfoPanel();
        if (m_btnFormatConvert)
            m_btnFormatConvert->setEnabled(hasModel() && AccessControl::canWrite("深度学习.数据集管理"));
        // ── 强制刷新右侧面板布局 ──
        if (m_rightContainer) {
            m_rightContainer->updateGeometry();
            m_rightContainer->layout()->activate();
        }
    });
    m_loadDeferTimer->start(30);
    applyPermissions();

    Logger::info(QString::fromUtf8("数据集管理：绑定模型 \"%1\"").arg(modelId));
}

// ── 路径工具 ──────────────────────────────────────────────────

QString DatasetManagement::datasetDir() const
{
    return QDir(m_projectRoot).absoluteFilePath(m_currentModelId);
}

QString DatasetManagement::imagesDir() const
{
    if(m_classification) return datasetDir();
    return QDir(datasetDir()).absoluteFilePath("images");
}

QString DatasetManagement::labelsDir() const
{
    return QDir(datasetDir()).absoluteFilePath("labels");
}

QString DatasetManagement::annotationsDir() const
{
    // 兼容旧接口：标注文件存放在 labels/ 目录下
    return labelsDir();
}

QString DatasetManagement::configPath() const
{
    return QDir(datasetDir()).absoluteFilePath("dataset_config.json");
}

int DatasetManagement::currentLabelOffset() const
{
    // Halcon 兼容：GrayMask minGray 偏移，为 ColorIndex 留出 0 值
    return 1;
}

// ── 标注模块集成：按路径查找样本索引 ──────────────────────────

int DatasetManagement::findSampleIndex(const QString& imagePath) const
{
    QString clean = QDir::cleanPath(imagePath);
    for (int i = 0; i < m_samples.size(); ++i) {
        if (QDir::cleanPath(m_samples[i].absolutePath) == clean)
            return i;
    }
    return -1;
}

void DatasetManagement::onAnnotationUpdated(const QString& imagePath,
                                             const QList<int>& categoryIds,
                                             int instanceCount)
{
    if(!AccessControl::canWrite("深度学习.数据标注")) return;
    int idx = findSampleIndex(imagePath);
    if (idx < 0 || idx >= m_samples.size()) {
        Logger::warn(QString::fromUtf8(
            "onAnnotationUpdated: 找不到图像 %1").arg(imagePath));
        return;
    }

    // 更新样本状态
    m_samples[idx].annotated     = true;
    m_samples[idx].categoryIds   = categoryIds;
    m_samples[idx].instanceCount = instanceCount;

    refreshAnnotationStatistics();

    // 保存配置 + 刷新 UI
    saveDatasetConfig();
    updateLabelPanel();
    updateModelInfoPanel();

    if(!ProjectConfig::setModelStatus(m_project.fvprojPath,m_currentModelId,"annotated",true))
        QMessageBox::warning(this,tr("项目状态更新失败"),tr("标签已保存；项目状态更新失败，可重新打开后重试：%1").arg(m_project.fvprojPath));
    applyPermissions();
    emit annotationSaved(m_currentModelId);
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
    if(!f.exists()) {Logger::warn(configPath()+tr("：缺少原生数据集配置，请新建模型"));return;}
    {
        if(!f.open(QIODevice::ReadOnly)) {Logger::warn(configPath()+"："+f.errorString());return;}
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(f.readAll(),&error);f.close();
        if(error.error!=QJsonParseError::NoError || !doc.isObject()) {Logger::warn(configPath()+tr("：配置JSON损坏，未覆盖"));return;}
        QString contractError;
        if (!StorageContract::validate(doc.object(), "fusevision.dataset", &contractError)) {
            Logger::warn(configPath()+"："+contractError);return;
        }
        if ((doc.object()["dataset_mode"] == QJsonValue("classification")) != (m_currentModel.datasetLayout == "classification")) {
            Logger::warn(configPath()+tr("：数据集模式与模型布局不匹配"));return;
        }
        m_datasetConfig=doc.object();
    }
    m_labelType=m_datasetConfig.value("label_type").toString();
    m_classification=m_datasetConfig.value("dataset_mode").toString()=="classification";

    // 解析标签
    m_labelNames.clear();
    m_labelColors.clear();
    m_labelInstanceCounts.clear();
    QJsonArray cats = m_datasetConfig.value("categories").toArray();
    for (const QJsonValue& v : cats) {
        QJsonObject cat = v.toObject();
        QString name = cat.value("name").toString();
        m_labelNames.append(name);
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

    if(m_classification) {
        m_labelNames={"OK","NG"};m_labelColors={s_defaultColors[0],s_defaultColors[1]};
        for(const auto& phase:{"train","val","test"}) for(int category=0;category<2;++category) {
            const auto relative=QString(phase)+"/"+m_labelNames[category];
            for(const auto& fi:QDir(QDir(datasetDir()).filePath(relative)).entryInfoList(filters,QDir::Files,QDir::Name)) {
                DatasetSample sample;sample.id=m_samples.size()+1;sample.fileName=relative+"/"+fi.fileName();sample.absolutePath=fi.absoluteFilePath();
                const auto size=QImageReader(sample.absolutePath).size();sample.width=size.width();sample.height=size.height();
                sample.categoryIds={category};sample.annotated=true;sample.instanceCount=1;
                sample.split=QString(phase)=="train"?DatasetSample::Train:QString(phase)=="val"?DatasetSample::Val:DatasetSample::Test;
                m_samples.append(sample);
            }
        }
        refreshAnnotationStatistics();applyPermissions();return;
    }

    QFileInfoList files = imgDir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        DatasetSample sample;
        sample.id           = m_samples.size() + 1;  // COCO 兼容 1-based
        sample.fileName     = fi.fileName();
        sample.absolutePath = fi.absoluteFilePath();
        QString base = fi.completeBaseName();
        QImageReader reader(sample.absolutePath);const auto size=reader.size();
        sample.width=size.width();sample.height=size.height();
        m_samples.append(sample);
    }
    refreshAnnotationStatistics();
}

// ── 保存数据集配置 ────────────────────────────────────────────

bool DatasetManagement::saveDatasetConfig()
{
    if(!hasModel() || !(AccessControl::canWrite("深度学习.数据集管理") || AccessControl::canWrite("深度学习.数据标注"))) return false;
    QFile existing(configPath());
    QJsonObject existingConfig = m_datasetConfig;
    if(!existing.exists()) {QMessageBox::warning(this,tr("配置保存失败"),tr("缺少原生数据集配置，请新建模型：%1").arg(configPath()));return false;}
    {
        QJsonParseError parse;
        if (!existing.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this,tr("配置保存失败"),configPath()+"\n"+existing.errorString());return false;
        }
        const auto document = QJsonDocument::fromJson(existing.readAll(), &parse);
        QString contractError;
        if(!document.isObject() || parse.error!=QJsonParseError::NoError ||
           !StorageContract::validate(document.object(), "fusevision.dataset", &contractError)) {
            QMessageBox::warning(this,tr("配置保存失败"),tr("原数据集配置不可读、损坏或版本不受支持，拒绝覆盖：%1").arg(configPath())+"\n"+contractError);return false;
        }
        if ((document.object()["dataset_mode"] == QJsonValue("classification")) != (m_currentModel.datasetLayout == "classification")) {
            QMessageBox::warning(this,tr("配置保存失败"),tr("数据集模式与模型布局不匹配，拒绝覆盖"));return false;
        }
        existingConfig = document.object();
        existing.close();
    }
    QDir().mkpath(datasetDir());

    QJsonArray cats;
    for (int i = 0; i < m_labelNames.size(); ++i) {
        const QString& name = m_labelNames[i];
        if (name.trimmed().isEmpty()) {QMessageBox::warning(this,tr("类别无效"),tr("类别名称为空，拒绝改变编号"));return false;}
        QJsonObject cat;
        cat["id"]    = cats.size() + 1;
        cat["name"]  = name;
        cat["color"] = m_labelColors.value(i, s_defaultColors[i % s_defaultColors.size()]);
        cat["instance_count"] = m_labelInstanceCounts.value(i, -1);
        cats.append(cat);
    }

    QJsonObject cfg = existingConfig;
    cfg["categories"]  = cats;
    cfg["image_count"] = m_samples.size();
    cfg["label_type"]  = m_labelType;
    cfg["dataset_mode"]=m_classification ? "classification" : "annotation";

    // ── 写入顶层 instance_count（ModelManagement 面板展示用）──
    int totalInstances = 0;
    for (const auto& s : m_samples)
        totalInstances += s.instanceCount;
    cfg["instance_count"] = totalInstances;
    StorageContract::stamp(cfg, "fusevision.dataset");

    QString error;
    if (!StorageContract::validate(cfg, "fusevision.dataset", &error)) {
        QMessageBox::warning(this,tr("配置保存失败"),configPath()+"\n"+error);return false;
    }
    if(!AtomicFile::write(configPath(),QJsonDocument(cfg).toJson(QJsonDocument::Indented),&error)) {
        Logger::warn(tr("数据集配置保存失败：%1").arg(error));
        QMessageBox::warning(this,tr("数据集配置保存失败"),error+tr("\n已提交的标签保留；重新打开可重建统计。"));return false;
    }
    m_datasetConfig=cfg;return true;
}

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

    // ── 格式转换按钮 ──
    m_btnFormatConvert = new QPushButton(QString::fromUtf8("格式转换"));
    m_btnFormatConvert->setObjectName("dsPlaceholderBtn");
    m_btnFormatConvert->setCursor(Qt::PointingHandCursor);
    m_btnFormatConvert->setEnabled(false);  // 需要先绑定模型
    m_btnFormatConvert->setToolTip(QString::fromUtf8("将导入的标注转换为当前模型所需的 YOLO txt 格式"));
    connect(m_btnFormatConvert, &QPushButton::clicked, this, &DatasetManagement::onFormatConvert);
    tbLayout->addWidget(m_btnFormatConvert);

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

    // ── 加载动画覆盖层（覆盖整个画廊区域）──
    m_loadingOverlay = new LoadingOverlay(m_galleryStack,
        QString::fromUtf8("正在加载数据集..."));
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

    // 格式转换按钮样式
    if (m_btnFormatConvert) {
        m_btnFormatConvert->setStyleSheet(QString(
            "QPushButton#dsPlaceholderBtn {"
            "  background-color: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 4px; padding: 4px 12px; font-size: 11px; }"
            "QPushButton#dsPlaceholderBtn:hover { border-color: %4; background-color: %5; }"
            "QPushButton#dsPlaceholderBtn:disabled { color: %6; border-style: dashed; }")
            .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.accentPrimary,
                 p.bgHover, p.textMuted));
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
                QString chipColor = m_labelColors.value(cid, s_defaultColors[cid % s_defaultColors.size()]);

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

                // 用 QPixmap 绘制色块（统一辅助函数，与标签面板一致）
                QPixmap chipBg = makeColorChip(
                    fold ? QColor(128, 128, 128, 180) : QColor(chipColor),
                    chipSize, Qt::white, 1, 3);
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
            if (s.annotated && primaryCat2 >= 0) {
                QString color = m_labelColors.value(primaryCat2,
                    s_defaultColors[primaryCat2 % s_defaultColors.size()]);
                item->setForeground(QColor(color));
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

    m_loadingOverlay->hideOverlay();
    m_inRefresh = false;
}

// ── 事件过滤器：处理缩略图点击 + 网格容器宽度变化 ───────────

bool DatasetManagement::eventFilter(QObject* obj, QEvent* event)
{
    // 外层滚动区宽度变化 → 防抖后重新排列缩略图列数（避免连续 resize 时反复重建导致卡死）
    if (obj == m_galleryScrollArea && event->type() == QEvent::Resize) {
        int w = m_galleryScrollArea->width();
        if (w > 0 && w != m_lastGalleryWidth) {
            m_lastGalleryWidth = w;
            if (m_resizeDebounce)
                m_resizeDebounce->start();
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
    QAction* actReannotate = menu.addAction(m_classification?tr("预览分类图像"):tr("重新标注"));

    QAction* chosen = menu.exec(pos);
    if (!chosen) return;

    if (chosen == actDelete) {
        m_selectedIndices={idx};onDeleteSelected();
    } else if (chosen == actOpenDir) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(imgPath).absolutePath()));
    } else if (chosen == actCopyPath) {
        QApplication::clipboard()->setText(imgPath);
        Logger::info(QString::fromUtf8("已复制图像路径"));
    } else if (chosen == actReannotate) {
        onThumbnailDoubleClicked(idx);
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
        if(m_classification) {
            m_labelTypeLabel->setText(tr("标签格式：OK/NG 目录分类"));
        } else if (!hasModel) {
            m_labelTypeLabel->setText(QString::fromUtf8("标签格式：--"));
        } else if (m_labelType.isEmpty()) {
            QString recommended = LabelTypes::recommendForModelType(m_currentModel.modelType);
            m_labelTypeLabel->setText(QString::fromUtf8("标签格式：未导入（推荐：%1）").arg(recommended));
        } else {
            m_labelTypeLabel->setText(QString::fromUtf8("标签格式：%1 → TXT").arg(m_labelType));
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

    m_imageInstances->setText(QString::fromUtf8("标注实例数：%1").arg(s.instanceCount));
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

        // 颜色方块（统一辅助函数，与缩略图色块一致）
        QColor chipColor(color);
        if (isEmpty) chipColor.setAlpha(100);
        QPixmap chipPix = makeColorChip(chipColor, 14,
            isEmpty ? QColor(p.borderMedium) : QColor(Qt::white), 1, 3);

        QLabel* colorBlock = new QLabel;
        colorBlock->setFixedSize(14, 14);
        colorBlock->setPixmap(chipPix);
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
    updateModelInfoPanel();
    Logger::info(QString::fromUtf8("标签格式已设置为: %1").arg(m_labelType));
    return true;
}

// ── 扫描 annotations 自动识别标签类别（合并模式：保留空标签 + 新增检测到的）──
// 同时扫描 JSON（labelme shapes / COCO annotations）和 PNG 掩码，覆盖所有标注格式
void DatasetManagement::detectLabelsFromAnnotations()
{
    if(m_classification) {refreshAnnotationStatistics();return;}
    QDir annDir(annotationsDir());
    if (!annDir.exists()) return;

    // 初始化实例计数（保留已有标签，先全部置 0）
    QMap<QString, int> nameToInstances;  // name → 实例数
    for (int i = 0; i < m_labelNames.size(); ++i) {
        if (!m_labelNames[i].trimmed().isEmpty())
            nameToInstances[m_labelNames[i]] = 0;  // 重置计数，后续重新统计
    }

    // Hidden native shapes still own category slots even when standard pixels/annotations are empty.
    for(const auto& fi:annDir.entryInfoList({"*.json","*.png"},QDir::Files)) {
        QJsonArray native;
        if(fi.suffix().compare("png",Qt::CaseInsensitive)==0)
            native=QJsonDocument::fromJson(QImage(fi.absoluteFilePath()).text("FuseVision.Annotation.v1").toUtf8()).array();
        else {
            QFile file(fi.absoluteFilePath());
            if(file.open(QIODevice::ReadOnly)) native=QJsonDocument::fromJson(file.readAll()).object()["fusevision_shapes"].toArray();
        }
        for(const auto& value:native) {
            const int id=value.toObject()["label_id"].toInt(-1);
            if(id<0 || id>65535) continue;
            while(m_labelNames.size()<=id) {
                const auto name=QString("class_%1").arg(m_labelNames.size()+1);
                m_labelNames.append(name);m_labelColors.append(s_defaultColors[(m_labelNames.size()-1)%s_defaultColors.size()]);m_labelInstanceCounts.append(0);nameToInstances[name]=0;
            }
        }
    }

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
        for(const auto& value:root["annotations"].toArray()) {
            const int id=value.toObject()["category_id"].toInt(-1)-currentLabelOffset();
            if(id<0 || id>65535) continue;
            while(m_labelNames.size()<=id) {
                const auto name=QString("class_%1").arg(m_labelNames.size()+1);
                m_labelNames.append(name);m_labelColors.append(s_defaultColors[(m_labelNames.size()-1)%s_defaultColors.size()]);m_labelInstanceCounts.append(0);nameToInstances[name]=0;
            }
        }

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
            // COCO annotations（1-based category_id → 0-based 索引）
            for (const QJsonValue& av : annots) {
                int cid = av.toObject().value("category_id").toInt(-1);
                // 减 1 转换为 0-based 索引（与 saveAnnotations 写入时的 +labelOffset 对应）
                int lid = cid - currentLabelOffset();
                if (lid >= 0 && lid < m_labelNames.size()
                    && !m_labelNames[lid].trimmed().isEmpty())
                    nameToInstances[m_labelNames[lid]]++;
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
    // Step 2：掩码类型时，扫描全部 PNG 掩码确定类别数
    // ══════════════════════════════════════════════════
    // 灰度值 0 恒为背景，类别从灰度值 1 开始
    // 总类别数 = maxGray - minGray + 1（支持 Halcon 偏移 minGray≥2）
    if (LabelTypes::isMaskType(m_labelType)) {
        QMap<int, int> grayToMaskCount;  // 灰度值 → 包含该值的掩码图片数
        QStringList pngFilters = { "*.png" };
        QFileInfoList pngFiles = annDir.entryInfoList(pngFilters, QDir::Files);

        // 扫描全部 PNG 掩码，收集所有非零灰度值
        for (const QFileInfo& pngFi : pngFiles) {
            QImage img(pngFi.absoluteFilePath());
            if (img.isNull()) continue;
            QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
            int w = gray.width(), h = gray.height();
            // 自适应步长：大图稀采样，小图逐像素扫描
            int step = 1;
            QSet<int> uniqueVals;
            for (int y = 0; y < h; y += step) {
                const uchar* line = gray.constScanLine(y);
                for (int x = 0; x < w; x += step) {
                    int v = line[x];
                    if (v > 0) uniqueVals.insert(m_labelType==LabelTypes::BinaryMask?1:v);
                }
            }
            for (int gv : uniqueVals)
                grayToMaskCount[gv]++;
        }

        if (grayToMaskCount.isEmpty()) {
            // 无有效灰度值，跳过掩码解析
            goto endMaskStep2;
        }

        // 自动检测偏移：标准掩码 minGray=1, Halcon 掩码 minGray≥2
        int minGray = currentLabelOffset();
        int maxGray = m_labelType==LabelTypes::BinaryMask?1:grayToMaskCount.lastKey();

        // ── 有效类别总数 = maxGray - minGray + 1（灰度值 0 已排除）──
        // 填充缺失灰度值（中间缺口 → 掩码计数 0，类别名称仍创建）
        for (int g = minGray; g <= maxGray; ++g) {
            if (!grayToMaskCount.contains(g))
                grayToMaskCount[g] = 0;
        }

        // 映射：灰度值 g → 0-based 索引（g - minGray）
        QMap<int, int> grayToIndex;
        int totalClasses = maxGray - minGray + 1;
        for (int g = minGray, idx = 0; g <= maxGray; ++g, ++idx)
            grayToIndex[g] = idx;

        // ── 同步标签列表 ──
        // 确保 m_labelNames 大小 = totalClasses（0-based 索引连续）
        // 已有 JSON 名称保留，不足部分用占位名填充
        bool added = false;
        while (m_labelNames.size() < totalClasses) {
            int idx = m_labelNames.size();
            QString autoName = QString::fromUtf8("class_%1").arg(idx + 1);
            m_labelNames.append(autoName);
            m_labelColors.append(s_defaultColors[idx % s_defaultColors.size()]);
            m_labelInstanceCounts.append(0);
            nameToInstances[autoName] = 0;
            added = true;
        }
        // 更新实例计数：每个灰度值对应的掩码图片数
        for (int g = minGray, idx = 0; g <= maxGray; ++g, ++idx) {
            int maskCount = grayToMaskCount.value(g, 0);
            if (idx < m_labelInstanceCounts.size()) {
                m_labelInstanceCounts[idx] = maskCount;
                if (idx < m_labelNames.size())
                    nameToInstances[m_labelNames[idx]] = maskCount;
            }
        }

        if (added) {
            // 重映射样本 categoryIds（考虑 offset）
            for (auto& s : m_samples) {
                if (s.annotated) {
                    for (int& cid : s.categoryIds) {
                        int grav = cid + minGray;
                        if (grayToIndex.contains(grav))
                            cid = grayToIndex[grav];
                    }
                }
            }
            saveDatasetConfig();
            Logger::info(QString::fromUtf8("从掩码图像自动识别 %1 个类别").arg(grayToMaskCount.size()));
        }
    }
    endMaskStep2:

    // ══════════════════════════════════════════════════
    // Step 3：合并 nameToInstances → m_labelNames
    // ══════════════════════════════════════════════════
    bool imported = false;
    for (auto it = nameToInstances.begin(); it != nameToInstances.end(); ++it) {
        const QString& name = it.key();
        // 跳过空名称（可能来自损坏的 dataset_config 或误保存的残留条目）
        if (name.trimmed().isEmpty()) continue;
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

    // ── 清理遗留的空名称条目（保持三数组同步 + 重映射样本 categoryIds）──
    int labelCount = m_labelNames.size();
    // 1) 标记哪些索引即将被移除
    QVector<bool> isEmpty(labelCount, false);
    for (int i = 0; i < labelCount; ++i)
        isEmpty[i] = m_labelNames[i].trimmed().isEmpty();

    // 2) 构建 old-index → new-index 映射
    QMap<int, int> idxRemap;
    int newIdx = 0;
    for (int i = 0; i < labelCount; ++i) {
        if (!isEmpty[i])
            idxRemap[i] = newIdx++;
    }

    // 3) 逆向移除空条目（保持三数组同步）
    for (int i = labelCount - 1; i >= 0; --i) {
        if (isEmpty[i]) {
            m_labelNames.removeAt(i);
            if (i < m_labelColors.size()) m_labelColors.removeAt(i);
            if (i < m_labelInstanceCounts.size()) m_labelInstanceCounts.removeAt(i);
        }
    }

    // 4) 重映射所有样本的 categoryIds，避免颜色/名称错位
    for (auto& s : m_samples) {
        if (!s.annotated) continue;
        QList<int> remapped;
        for (int cid : s.categoryIds) {
            if (idxRemap.contains(cid))
                remapped.append(idxRemap[cid]);
            // 旧索引指向空名称 → 丢弃（不应再存在此类引用）
        }
        s.categoryIds = remapped;
    }

    refreshAnnotationStatistics();
    if (imported) {
        saveDatasetConfig();
        Logger::info(QString::fromUtf8("从标注文件自动识别 %1 个新标签类别")
            .arg(nameToInstances.size() - m_labelNames.size() + 1));
    }

    updateLabelPanel();
    emit labelsChanged();
}

// =============================================================================
// 槽函数
// =============================================================================

void DatasetManagement::onImportImages()
{
    if(m_classification) {convertAnomalyClassification();return;}
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
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
        "  • 标签文件 (*.json 或 *.png)       → 放入 labels/\n\n"
        "不同模型类型对应不同标签格式：\n"
        "  • 实例分割 → 灰度掩码PNG，灰度值1=类别1, 2=类别2…\n"
        "    例: labels/img_01.png 为单通道灰度图，像素值即类别ID\n"
        "  • 异常检测 → 二值掩码PNG，0=正常, 1=异常\n"
        "    例: labels/img_01.png 为黑白二值图\n"
        "  • 物体检测 → COCO JSON，含 bbox 字段\n"
        "    例: labels/img_01.json 含 {\"bbox\":[x,y,w,h],\"category_id\":1}\n"
        "  • 自由矩形 → COCO JSON，含旋转角度\n"
        "    例: labels/img_01.json 含 bbox:[x,y,w,h]、rotation、rot_center 与 category_id\n\n"
        "建议先导入原图，再导入对应的标签文件。"));
    fmtBox.setIcon(QMessageBox::Information);
    fmtBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    if (fmtBox.exec() != QMessageBox::Ok)
        return;

    // 2. 先导入原图（标签类型延后到实际需要时再确认）
    QStringList imgFiles = QFileDialog::getOpenFileNames(
        this,
        QString::fromUtf8("第一步：选择原图文件"),
        m_projectRoot,
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
                m_projectRoot,
                QString::fromUtf8("标签文件 (*.json *.png);;JSON 标签 (*.json);;PNG 掩码 (*.png);;所有文件 (*.*)"));
        }
    }

    if(importLabels && lblFiles.isEmpty()) return; // Cancelling label selection commits nothing.
    importSelectedFiles(imgFiles,lblFiles);
}

void DatasetManagement::onImportFolder()
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    if(m_classification) {
        const auto generation=m_modelGeneration;
        const auto source=QFileDialog::getExistingDirectory(this,tr("选择分类数据集根目录（train、val、test 下的 OK/NG）"),m_projectRoot);
        if(source.isEmpty() || generation!=m_modelGeneration) return;
        DatasetTransferReport report;
        for(const auto& phase:{"train","val","test"}) for(const auto& category:{"OK","NG"}) {
            QStringList images;
            for(const auto& fi:QDir(QDir(source).filePath(QString(phase)+"/"+category)).entryInfoList(QDir::Files,QDir::Name)) images.append(fi.absoluteFilePath());
            const auto part=DatasetFileService::importClassificationFiles(images,datasetDir(),phase,category,[this,generation]{return generation==m_modelGeneration && AccessControl::canWrite("深度学习.数据集管理");});
            report.succeeded+=part.succeeded;report.skipped+=part.skipped;report.failures+=part.failures;report.failedSources+=part.failedSources;report.cancelled|=part.cancelled;
        }
        finishClassificationImport(report);return;
    }
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
        "    例: labels/img_01.json 含 bbox:[x,y,w,h]、rotation、rot_center 与 category_id\n\n"
        "请确认目标文件夹已按要求组织好 images/ 和 labels/。"));
    fmtBox.setIcon(QMessageBox::Information);
    fmtBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    if (fmtBox.exec() != QMessageBox::Ok)
        return;

    // 2. 选择文件夹
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        QString::fromUtf8("选择数据集根目录（需包含 images/ 子目录，labels/ 可选）"),
        m_projectRoot);

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

    QStringList imagePaths,labelPaths;
    for(const auto& file:imgFiles) imagePaths.append(file.absoluteFilePath());
    for(const auto& file:lblFiles) labelPaths.append(file.absoluteFilePath());
    importSelectedFiles(imagePaths,labelPaths);
}

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
        // 扫描全部 PNG 掩码，收集所有非零灰度值
        for (const QString& fname : pngFiles) {
            QImage img(annDir.absoluteFilePath(fname));
            if (img.isNull()) continue;
            QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
            int w = gray.width(), h = gray.height();
            int step = 1;
            for (int y = 0; y < h; y += step) {
                const uchar* line = gray.constScanLine(y);
                for (int x = 0; x < w; x += step) {
                    if (line[x] > 0)
                        grayValues.insert(line[x]);
                }
            }
        }
        // 灰度值 0 恒为背景，类别总数 = maxGray - minGray + 1
        if (!grayValues.isEmpty()) {
            int minGray = *std::min_element(grayValues.begin(), grayValues.end());
            int maxGray = *std::max_element(grayValues.begin(), grayValues.end());
            actualCount = maxGray - minGray + 1;
        } else {
            actualCount = 0;
        }
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
            for (const QJsonValue& v : cats) {
                QString name = v.toObject().value("name").toString();
                if (!name.trimmed().isEmpty())
                    catNames.insert(name);
            }
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
    if(!AccessControl::canRead("深度学习.数据集管理")) return;
    if (idx >= 0 && idx < m_samples.size()) {
        if(m_classification) {
            const auto& sample=m_samples[idx];QDialog preview(this);preview.setObjectName("classificationPreview");
            preview.setWindowTitle(tr("分类预览：%1").arg(sample.fileName));auto* layout=new QVBoxLayout(&preview);
            auto* label=new QLabel;label->setObjectName("classificationImage");label->setPixmap(QPixmap(sample.absolutePath).scaled(1000,700,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            layout->addWidget(label);layout->addWidget(new QLabel(tr("%1 · 目录类别：%2").arg(sample.fileName.section('/',0,0),m_labelNames.value(sample.primaryCategory()))));
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close);connect(buttons,&QDialogButtonBox::rejected,&preview,&QDialog::reject);layout->addWidget(buttons);preview.exec();return;
        }
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
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    if (m_selectedIndices.isEmpty()) return;

    if(hasPendingAnnotations && hasPendingAnnotations()) {
        QMessageBox::warning(this,tr("请先处理标注"),tr("存在未保存标注，请先返回标注页保存或放弃，再删除图像。"));return;
    }
    const auto selected=m_selectedIndices;const auto generation=m_modelGeneration;
    QStringList paths;for(int idx:selected) if(idx>=0 && idx<m_samples.size()) paths.append(m_samples[idx].absolutePath);
    if(QMessageBox::question(this,tr("确认删除"),tr("将 %1 张图像和精确配对标签移入模型内 .fusevision-trash 恢复目录：\n%2")
        .arg(paths.size()).arg(paths.join("\n")),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes) return;
    if(!WriteAccess::require("深度学习.数据集管理",this) || generation!=m_modelGeneration) return;
    const auto report=DatasetFileService::removeSamples(paths,imagesDir(),labelsDir(),datasetDir()+"/.fusevision-trash",
        [] {return AccessControl::canWrite("深度学习.数据集管理");});
    if(!report.succeeded.isEmpty()) emit datasetFilesChanged();
    loadDatasetConfig();m_selectedIndices.clear();m_activeThumbnailIdx=-1;
    saveDatasetConfig();updateSelectionUI();refreshGallery();updateModelInfoPanel();
    QMessageBox box(this);box.setWindowTitle(tr("删除结果"));box.setText(report.summary().section("\n",0,0));box.setDetailedText(report.summary());box.exec();
    if(!report.succeeded.isEmpty()) Logger::info(tr("图像已移入恢复目录：%1").arg(report.succeeded.join("\n")));

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
        m_btnDeleteSelected->setEnabled(selCount > 0 && AccessControl::canWrite("深度学习.数据集管理"));
}

// ── 标签 CRUD ────────────────────────────────────────────────

void DatasetManagement::onAddLabel()
{
    if(m_classification) return;
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
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

    const auto oldNames=m_labelNames;const auto oldColors=m_labelColors;const auto oldCounts=m_labelInstanceCounts;
    m_labelNames.append(name);
    m_labelColors.append(color.name());
    m_labelInstanceCounts.append(0);  // 新标签初始实例数为 0
    if(!saveDatasetConfig()) {m_labelNames=oldNames;m_labelColors=oldColors;m_labelInstanceCounts=oldCounts;updateLabelPanel();return;}
    updateLabelPanel();
    emit labelsChanged();

    Logger::info(QString::fromUtf8("已添加标签: %1 [%2]").arg(name, color.name()));
}

void DatasetManagement::onEditLabel()
{
    if(m_classification) return;
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
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

    const auto oldName=m_labelNames[row];m_labelNames[row] = newName;
    if(!saveDatasetConfig()) {m_labelNames[row]=oldName;updateLabelPanel();return;}
    updateLabelPanel();
    emit labelsChanged();
    Logger::info(QString::fromUtf8("已编辑标签: %1").arg(newName));
}

void DatasetManagement::onDeleteLabel()
{
    if(m_classification) return;
    if(hasPersistedAnnotations() || (hasPendingAnnotations && hasPendingAnnotations())) {
        QMessageBox::warning(this,tr("类别受保护"),tr("已有标注或未保存编辑，删除会改变类别编号含义。请保留类别；本轮不迁移标注。\n%1").arg(labelsDir()));return;
    }
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
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

    const auto oldNames=m_labelNames;const auto oldColors=m_labelColors;const auto oldCounts=m_labelInstanceCounts;
    QString removed = m_labelNames[row];
    m_labelNames.removeAt(row);
    m_labelColors.removeAt(row);
    if (row < m_labelInstanceCounts.size()) m_labelInstanceCounts.removeAt(row);
    if(!saveDatasetConfig()) {m_labelNames=oldNames;m_labelColors=oldColors;m_labelInstanceCounts=oldCounts;updateLabelPanel();return;}
    updateLabelPanel();
    emit labelsChanged();
    Logger::info(QString::fromUtf8("已删除标签: %1").arg(removed));
}

void DatasetManagement::onImportLabels()
{
    if(m_classification) return;
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
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

        const auto oldNames=m_labelNames,oldColors=m_labelColors;const auto oldCounts=m_labelInstanceCounts;
        int imported = 0;
        for (const QJsonValue& v : cats) {
            QJsonObject cat = v.toObject();
            QString name = cat.value("name").toString();
            if (!name.isEmpty() && !m_labelNames.contains(name)) {
                m_labelNames.append(name);
                m_labelColors.append(cat.value("color").toString(
                    s_defaultColors[(m_labelNames.size() - 1) % s_defaultColors.size()]));
                m_labelInstanceCounts.append(0);  // 从 JSON 导入的标签初始实例数未知
                imported++;
            }
        }
        if (imported > 0) {
            if(!saveDatasetConfig()) {m_labelNames=oldNames;m_labelColors=oldColors;m_labelInstanceCounts=oldCounts;return;}
            updateLabelPanel();
            emit labelsChanged();
            Logger::info(QString::fromUtf8("已从 JSON 导入 %1 个标签").arg(imported));
        }
    }
}

// ── 标签拖拽重排 ──────────────────────────────────────────────
void DatasetManagement::onLabelOrderChanged()
{
    if(m_classification) {updateLabelPanel();return;}
    if(!WriteAccess::require("深度学习.数据集管理",this)) {updateLabelPanel();return;}
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
    if(hasPersistedAnnotations() || (hasPendingAnnotations && hasPendingAnnotations())) {
        updateLabelPanel();
        QMessageBox::warning(this,tr("类别受保护"),tr("已有标注或未保存编辑，禁止重排类别编号。\n%1").arg(labelsDir()));return;
    }
    QSet<int> unique(newOrder.begin(),newOrder.end());
    if(unique.size()!=count || *std::min_element(newOrder.begin(),newOrder.end())!=0 || *std::max_element(newOrder.begin(),newOrder.end())!=count-1) {updateLabelPanel();return;}

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
    const auto oldNames=m_labelNames,oldColors=m_labelColors;const auto oldCounts=m_labelInstanceCounts;const auto oldSamples=m_samples;
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
    if(!saveDatasetConfig()) {m_labelNames=oldNames;m_labelColors=oldColors;m_labelInstanceCounts=oldCounts;m_samples=oldSamples;updateLabelPanel();return;}
    updateLabelPanel();
    refreshGallery();
    emit labelsChanged();
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

// ── 标注格式转换 ──────────────────────────────────────────────
DatasetManagement::InputFormat DatasetManagement::detectInputFormat() const
{
    QDir annDir(annotationsDir());
    if (!annDir.exists()) return Fmt_Unknown;

    int txtCount = 0, pngCount = 0, jsonCount = 0;
    QFileInfoList files = annDir.entryInfoList(QDir::Files);
    // 递归检查子目录
    for (const char* phase : {"train", "val", "test"}) {
        QDir subDir(annDir.absoluteFilePath(phase));
        if (subDir.exists())
            files.append(subDir.entryInfoList(QDir::Files));
    }

    for (const auto& fi : files) {
        QString suf = fi.suffix().toLower();
        if (suf == "txt") txtCount++;
        else if (suf == "png") pngCount++;
        else if (suf == "json") jsonCount++;
    }

    // 优先级：json > png > txt（更具体的格式优先）
    if (jsonCount > 0 && jsonCount >= txtCount && jsonCount >= pngCount)
        return Fmt_LabelMe_JSON;
    if (pngCount > 0 && pngCount >= txtCount)
        return Fmt_PNG_Mask;
    if (txtCount > 0)
        return Fmt_YOLO_Txt;

    return Fmt_Unknown;
}

// ── 目标格式描述与示例 ──────────────────────────────────────
static QString targetFormatDesc(const QString& modelType)
{
    if (modelType == ModelTaskTypes::ObjDetectFree)
        return QString::fromUtf8(
            "自由矩形检测 — YOLO txt 格式\n"
            "每行：类别ID 中心点x 中心点y 宽度 高度\n"
            "示例：0 0.512300 0.489100 0.235600 0.312400\n"
            "数值归一化到 [0,1]（除以图像宽高）");
    if (modelType == ModelTaskTypes::InstanceSeg)
        return QString::fromUtf8(
            "实例分割 — YOLO txt 格式\n"
            "每行：类别ID x1 y1 x2 y2 ... xn yn\n"
            "示例：1 0.681 0.485 0.670 0.487 0.676 0.487\n"
            "点按顺序连接成闭合多边形，坐标归一化到 [0,1]");
    if (modelType == ModelTaskTypes::ObjDetectFixed)
        return QString::fromUtf8(
            "旋转框(OBB) — YOLO txt 格式\n"
            "每行：类别ID x1 y1 x2 y2 x3 y3 x4 y4\n"
            "示例：0 0.780811 0.743961 0.782371 0.746860 0.777691 0.752174 0.776131 0.749758\n"
            "四点按顺时针排列，坐标归一化到 [0,1]");
    // 异常检测
    return QString::fromUtf8(
        "异常检测 — 分类目录格式\n"
        "无标签文件，文件夹名即标签\n"
        "目录结构：train/OK/ train/NG/ val/OK/ val/NG/");
}

// ── 主入口：格式转换 ────────────────────────────────────────
void DatasetManagement::onFormatConvert()
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    if (!hasModel()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("请先选择一个模型。"));
        return;
    }

    QString modelType = m_currentModel.modelType;

    // ── 异常检测：引导 OK/NG 导入 ──
    if (m_classification) {
        convertAnomalyClassification();
        return;
    }

    // 检测输入格式
    InputFormat inputFmt = detectInputFormat();
    if (inputFmt == Fmt_Unknown) {
        QDir annDir(annotationsDir());
        if (!annDir.exists() || annDir.entryList(QDir::Files).isEmpty()) {
            QMessageBox::information(this, QString::fromUtf8("格式转换"),
                QString::fromUtf8("labels/ 目录下没有找到标注文件。\n\n"
                    "请先导入标注数据（支持 YOLO txt、PNG 掩码、LabelMe JSON）。"));
        } else {
            QMessageBox::warning(this, QString::fromUtf8("格式转换"),
                QString::fromUtf8("无法识别 labels/ 下的标注格式。\n"
                    "支持的格式：YOLO txt (.txt)、PNG 掩码 (.png)、LabelMe JSON (.json)"));
        }
        return;
    }

    // 先校验当前格式是否已经是目标格式
    auto validationResults = validateCurrentFormat();
    bool allPass = true;
    for (const auto& r : validationResults) {
        if (r.status == ConvertResult::Error) { allPass = false; break; }
    }
    if (allPass && inputFmt == Fmt_YOLO_Txt) {
        // 已经是正确的 YOLO txt 格式
        int passCount = 0;
        for (const auto& r : validationResults)
            if (r.status == ConvertResult::Pass) passCount++;
        QMessageBox::information(this, QString::fromUtf8("格式转换"),
            QString::fromUtf8("当前标注已是正确的 %1 格式，无需转换。\n\n%2 项通过校验。")
                .arg(modelType).arg(passCount));
        return;
    }

    // 格式不匹配时，展示目标格式说明并确认转换
    QString inputName;
    switch (inputFmt) {
    case Fmt_YOLO_Txt:    inputName = QString::fromUtf8("YOLO txt"); break;
    case Fmt_PNG_Mask:    inputName = QString::fromUtf8("PNG 掩码"); break;
    case Fmt_LabelMe_JSON: inputName = QString::fromUtf8("LabelMe JSON"); break;
    default: break;
    }

    QString desc = targetFormatDesc(modelType);
    QString msg = QString::fromUtf8(
        "检测到输入格式：%1\n\n"
        "目标格式：\n%2\n\n"
        "是否将标注转换为目标格式？\n"
        "（原始标注文件将保留，转换结果写入 labels/*.txt）")
        .arg(inputName, desc);

    QMessageBox::StandardButton btn = QMessageBox::question(
        this, QString::fromUtf8("格式转换 — %1").arg(modelType), msg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (btn != QMessageBox::Yes) return;

    // 执行转换
    bool ok = false;
    switch (inputFmt) {
    case Fmt_YOLO_Txt:    ok = convertYoloTxtToTarget(); break;
    case Fmt_PNG_Mask:    ok = convertPngMaskToTarget(); break;
    case Fmt_LabelMe_JSON: ok = convertLabelMeJsonToTarget(); break;
    default: break;
    }

    if (ok) {
        // 刷新
        loadDatasetConfig();
        detectLabelsFromAnnotations();
        validateLabelCount();
        refreshGallery();
        updateModelInfoPanel();

        Logger::info(QString::fromUtf8("标注格式转换完成: %1 → %2").arg(inputName, modelType));
    }
}

// ── 校验当前 YOLO txt 是否符合目标格式 ──────────────────────
QList<DatasetManagement::ConvertResult> DatasetManagement::validateCurrentFormat() const
{
    QList<ConvertResult> results;
    QString modelType = m_currentModel.modelType;
    QDir annDir(annotationsDir());

    // 收集所有 .txt 标签文件
    QFileInfoList allLabels;
    for (const char* phase : {"", "train", "val", "test"}) {
        QDir dir = phase[0] ? QDir(annDir.absoluteFilePath(phase)) : annDir;
        if (!dir.exists()) continue;
        allLabels.append(dir.entryInfoList({"*.txt"}, QDir::Files));
    }

    for (const auto& fi : allLabels) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ConvertResult r;
            r.file = fi.fileName();
            r.status = ConvertResult::Error;
            r.message = QString::fromUtf8("无法打开文件");
            results.append(r);
            continue;
        }

        QTextStream ts(&f);
        int lineNum = 0;
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            lineNum++;
            if (line.isEmpty() || line.startsWith('#')) continue;

            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.isEmpty()) continue;

            bool ok = false;
            int classId = parts[0].toInt(&ok);
            if (!ok || classId < 0) {
                ConvertResult r;
                r.file = fi.fileName(); r.line = lineNum;
                r.status = ConvertResult::Error;
                r.message = QString::fromUtf8("无效的类别ID: \"%1\"").arg(parts[0]);
                results.append(r);
                continue;
            }

            if (modelType == ModelTaskTypes::ObjDetectFree && parts.size() != 5) {
                ConvertResult r;
                r.file = fi.fileName(); r.line = lineNum;
                r.status = ConvertResult::Error;
                r.message = QString::fromUtf8("自由矩形应为5个值(class cx cy w h)，实际%1个\n示例: 0 0.512300 0.489100 0.235600 0.312400")
                    .arg(parts.size());
                results.append(r);
            } else if (modelType == ModelTaskTypes::ObjDetectFixed && parts.size() != 9) {
                ConvertResult r;
                r.file = fi.fileName(); r.line = lineNum;
                r.status = ConvertResult::Error;
                r.message = QString::fromUtf8("旋转框应为9个值(class x1 y1 ... x4 y4)，实际%1个\n示例: 0 0.780 0.743 0.782 0.746 0.777 0.752 0.776 0.749")
                    .arg(parts.size());
                results.append(r);
            } else if (modelType == ModelTaskTypes::InstanceSeg) {
                int coords = parts.size() - 1;
                if (coords < 6 || coords % 2 != 0) {
                    ConvertResult r;
                    r.file = fi.fileName(); r.line = lineNum;
                    r.status = ConvertResult::Error;
                    r.message = QString::fromUtf8("实例分割应为偶数个坐标(class x1 y1 ... xn yn)，实际%1个坐标\n示例: 1 0.681 0.485 0.670 0.487 0.676 0.487")
                        .arg(coords);
                    results.append(r);
                }
            } else {
                ConvertResult r;
                r.file = fi.fileName(); r.line = lineNum;
                r.status = ConvertResult::Pass;
                r.message = QString::fromUtf8("格式正确");
                results.append(r);
            }

            // 检查归一化（仅检查数值列）
            if (parts.size() > 1) {
                int checkEnd = qMin(parts.size(), modelType == ModelTaskTypes::ObjDetectFixed ? 9 : parts.size());
                for (int i = 1; i < checkEnd; ++i) {
                    double val = parts[i].toDouble(&ok);
                    if (!ok || val < 0.0 || val > 1.0) {
                        ConvertResult r;
                        r.file = fi.fileName(); r.line = lineNum;
                        r.status = ConvertResult::Error;
                        r.message = QString::fromUtf8("坐标值 %1 超出 [0,1] 归一化范围").arg(parts[i]);
                        results.append(r);
                    }
                }
            }
        }
        f.close();
    }
    return results;
}

// ── YOLO txt 格式校验修正 ──────────────────────────────────
bool DatasetManagement::convertYoloTxtToTarget()
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return false;
    QDir annDir(annotationsDir());

    // 收集所有 .txt 标签文件
    QFileInfoList allLabels;
    for (const char* phase : {"", "train", "val", "test"}) {
        QDir dir = phase[0] ? QDir(annDir.absoluteFilePath(phase)) : annDir;
        if (!dir.exists()) continue;
        allLabels.append(dir.entryInfoList({"*.txt"}, QDir::Files));
    }

    // 扫描所有使用的类别 ID
    QSet<int> usedClassIds;
    for (const auto& fi : allLabels) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream ts(&f);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                bool ok = false;
                int cid = parts[0].toInt(&ok);
                if (ok && cid >= 0) usedClassIds.insert(cid);
            }
        }
        f.close();
    }

    if (usedClassIds.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("格式转换"),
            QString::fromUtf8("标注文件中未发现有效类别。"));
        return false;
    }

    // ── 类别设置对话框 ──
    const auto& p = ThemeManager::instance().palette();

    QDialog catDlg(this);
    catDlg.setWindowTitle(QString::fromUtf8("设置类别名称 — YOLO txt 导入"));
    catDlg.setMinimumSize(460, 380);
    catDlg.setStyleSheet(QString("QDialog { background: %1; }").arg(p.bgPrimary));

    QVBoxLayout* catLayout = new QVBoxLayout(&catDlg);
    catLayout->setSpacing(10);

    QLabel* hintLabel = new QLabel(QString::fromUtf8(
        "YOLO txt 标注中仅包含类别 ID（数字），请为每个 ID 指定类别名称。\n"
        "类别名称仅支持英文、数字和下划线。"));
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(p.textSecondary));
    catLayout->addWidget(hintLabel);

    // 类别映射表
    QTableWidget* catTable = new QTableWidget;
    catTable->setColumnCount(2);
    catTable->setHorizontalHeaderLabels({
        QString::fromUtf8("类别 ID"), QString::fromUtf8("类别名称")});
    catTable->horizontalHeader()->setStretchLastSection(true);
    catTable->horizontalHeader()->resizeSection(0, 80);
    catTable->setAlternatingRowColors(true);
    catTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    catTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    catTable->setStyleSheet(QString(
        "QTableWidget { background: %1; color: %2; border: 1px solid %3;"
        " gridline-color: %4; font-size: 12px; }"
        "QHeaderView::section { background: %5; color: %6; border: none;"
        " padding: 4px; font-weight: bold; }")
        .arg(p.bgPrimary, p.textPrimary, p.borderMedium,
             p.borderLight, p.bgTertiary, p.textSecondary));

    QList<int> sortedIds(usedClassIds.begin(), usedClassIds.end());
    std::sort(sortedIds.begin(), sortedIds.end());
    catTable->setRowCount(sortedIds.size());

    for (int i = 0; i < sortedIds.size(); ++i) {
        int cid = sortedIds[i];
        auto* idItem = new QTableWidgetItem(QString::number(cid));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);  // ID 不可编辑
        idItem->setTextAlignment(Qt::AlignCenter);
        catTable->setItem(i, 0, idItem);

        // 如果已有对应名称则预填，否则填 class_{id}
        QString name;
        if (cid < m_labelNames.size() && !m_labelNames[cid].isEmpty())
            name = m_labelNames[cid];
        else
            name = QString("class_%1").arg(cid);
        catTable->setItem(i, 1, new QTableWidgetItem(name));
    }

    catLayout->addWidget(catTable, 1);

    // 按钮行
    QHBoxLayout* btnRow = new QHBoxLayout;
    QPushButton* btnAddRow = new QPushButton(QString::fromUtf8("+ 添加类别"));
    QPushButton* btnRemoveRow = new QPushButton(QString::fromUtf8("- 删除选中"));
    QPushButton* btnImport = new QPushButton(QString::fromUtf8("导入类别"));

    for (auto* btn : {btnAddRow, btnRemoveRow, btnImport}) {
        btn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 4px; padding: 4px 10px; font-size: 11px; }"
            "QPushButton:hover { background: %4; }")
            .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.bgHover));
        btn->setCursor(Qt::PointingHandCursor);
    }

    btnRow->addWidget(btnAddRow);
    btnRow->addWidget(btnRemoveRow);
    btnRow->addWidget(btnImport);
    btnRow->addStretch();
    catLayout->addLayout(btnRow);

    // 添加行
    connect(btnAddRow, &QPushButton::clicked, this, [catTable]() {
        int nextId = 0;
        for (int r = 0; r < catTable->rowCount(); ++r) {
            int cid = catTable->item(r, 0)->text().toInt();
            if (cid >= nextId) nextId = cid + 1;
        }
        int row = catTable->rowCount();
        catTable->insertRow(row);
        auto* idItem = new QTableWidgetItem(QString::number(nextId));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        idItem->setTextAlignment(Qt::AlignCenter);
        catTable->setItem(row, 0, idItem);
        catTable->setItem(row, 1, new QTableWidgetItem(QString("class_%1").arg(nextId)));
    });

    // 删除行
    connect(btnRemoveRow, &QPushButton::clicked, this, [catTable]() {
        int row = catTable->currentRow();
        if (row >= 0) catTable->removeRow(row);
    });

    // 从文件导入类别
    connect(btnImport, &QPushButton::clicked, this, [catTable, &p]() {
        QString filePath = QFileDialog::getOpenFileName(nullptr,
            QString::fromUtf8("导入类别文件"),
            QString(),
            QString::fromUtf8("文本文件 (*.txt *.csv);;所有文件 (*.*)"));
        if (filePath.isEmpty()) return;
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QSet<int> existingIds;
        for (int r = 0; r < catTable->rowCount(); ++r)
            existingIds.insert(catTable->item(r, 0)->text().toInt());
        int added = 0;
        while (!f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            // 格式：id name 或 name（自动分配 id）
            QStringList parts = line.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
            if (parts.isEmpty()) continue;
            bool ok = false;
            int id = parts[0].toInt(&ok);
            QString name;
            if (ok && parts.size() >= 2) {
                name = parts[1];
            } else {
                name = parts[0];
                id = 0;
                while (existingIds.contains(id)) id++;
            }
            if (!existingIds.contains(id)) {
                int row = catTable->rowCount();
                catTable->insertRow(row);
                auto* idItem = new QTableWidgetItem(QString::number(id));
                idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
                idItem->setTextAlignment(Qt::AlignCenter);
                catTable->setItem(row, 0, idItem);
                catTable->setItem(row, 1, new QTableWidgetItem(name));
                existingIds.insert(id);
                added++;
            }
        }
        f.close();
        QMessageBox::information(nullptr, QString::fromUtf8("导入类别"),
            QString::fromUtf8("已导入 %1 个新类别").arg(added));
    });

    // 确定/取消
    QDialogButtonBox* bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bbox->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 6px 20px; }"
        "QPushButton:hover { background: %4; }")
        .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.bgHover));
    catLayout->addWidget(bbox);
    connect(bbox->button(QDialogButtonBox::Ok), &QPushButton::clicked, &catDlg, &QDialog::accept);
    connect(bbox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, &catDlg, &QDialog::reject);

    if (catDlg.exec() != QDialog::Accepted) return false;

    // ── 从表格获取类别映射 ──
    QMap<int, QString> idToName;
    for (int r = 0; r < catTable->rowCount(); ++r) {
        int cid = catTable->item(r, 0)->text().toInt();
        QString name = catTable->item(r, 1)->text().trimmed();
        if (name.isEmpty()) name = QString("class_%1").arg(cid);
        if(!QRegularExpression("^[A-Za-z0-9_]+$").match(name).hasMatch()) {QMessageBox::warning(this,tr("类别无效"),tr("名称须为英文、数字或下划线；不会自动改写已有名称。"));return false;}
        idToName[cid] = name;
    }

    if (idToName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("至少需要一个类别。"));
        return false;
    }

    for(int used:usedClassIds) if(!idToName.contains(used)) {QMessageBox::warning(this,tr("类别受保护"),tr("不能移除已被标注引用的类别编号 %1").arg(used));return false;}
    int maxId=qMax(idToName.keys().last(),int(m_labelNames.size())-1);
    if(maxId>65535) {QMessageBox::warning(this,tr("类别无效"),tr("类别编号过大"));return false;}
    for(int i=0;i<m_labelNames.size();++i) {
        if(idToName.contains(i) && idToName[i]!=m_labelNames[i]) {
            QMessageBox::warning(this,tr("类别受保护"),tr("导入类别不能改变已有编号 %1 的名称，请使用类别重命名入口。就地修改名称不会迁移标注。 ").arg(i));return false;
        }
    }
    auto names=m_labelNames,colors=m_labelColors;auto counts=m_labelInstanceCounts;
    for(int i=names.size();i<=maxId;++i) {
        names.append(idToName.value(i,QString("class_%1").arg(i)));
        colors.append(QColor::fromHsv((i*360/(maxId+1))%360,200,200).name());counts.append(0);
    }
    if(QSet<QString>(names.begin(),names.end()).size()!=names.size()) {QMessageBox::warning(this,tr("类别无效"),tr("类别名称不能重复"));return false;}
    if(!WriteAccess::require("深度学习.数据集管理",this)) return false;
    const auto oldNames=m_labelNames,oldColors=m_labelColors;const auto oldCounts=m_labelInstanceCounts;
    m_labelNames=names;m_labelColors=colors;m_labelInstanceCounts=counts;
    if(!saveDatasetConfig()) {m_labelNames=oldNames;m_labelColors=oldColors;m_labelInstanceCounts=oldCounts;return false;}

    // ── 校验格式 ──
    auto results = validateCurrentFormat();
    int errorCount = 0;
    for (const auto& r : results)
        if (r.status == ConvertResult::Error) errorCount++;

    if (errorCount > 0) {
        showConvertResultDialog(results, 0, errorCount);
        return false;
    }

    QMessageBox::information(this, QString::fromUtf8("格式转换"),
        QString::fromUtf8("YOLO txt 格式校验通过，类别已设置。"));
    return true;
}

// ── PNG 掩码 → YOLO txt ────────────────────────────────────
bool DatasetManagement::convertPngMaskToTarget()
{
    return convertAnnotations("png");
}


bool DatasetManagement::convertLabelMeJsonToTarget()
{
    return convertAnnotations("json");
}


bool DatasetManagement::convertAnomalyClassification()
{
    if(!m_classification || !WriteAccess::require("深度学习.数据集管理",this)) return false;
    const auto generation=m_modelGeneration;
    bool ok=false;
    const auto category=QInputDialog::getItem(this,tr("选择图像类别"),tr("导入图像的目录类别："),{"OK","NG"},0,false,&ok);
    if(!ok || generation!=m_modelGeneration) return false;
    const auto phase=QInputDialog::getItem(this,tr("选择数据集划分"),tr("导入到哪个子集？"),{"train","val","test"},0,false,&ok);
    if(!ok || generation!=m_modelGeneration) return false;
    const auto files=QFileDialog::getOpenFileNames(this,tr("选择 %1 图像").arg(category),m_projectRoot,imageFileFilter());
    if(files.isEmpty() || generation!=m_modelGeneration) return false;
    const auto report=DatasetFileService::importClassificationFiles(files,datasetDir(),phase,category,
        [this,generation]{return generation==m_modelGeneration && AccessControl::canWrite("深度学习.数据集管理");});
    return finishClassificationImport(report);
}

bool DatasetManagement::finishClassificationImport(const DatasetTransferReport& result)
{
    auto report=result;
    if(!report.succeeded.isEmpty()) {
        emit datasetFilesChanged();loadDatasetConfig();saveDatasetConfig();refreshGallery();updateModelInfoPanel();applyPermissions();
        if(!ProjectConfig::setModelStatus(m_project.fvprojPath,m_currentModelId,"data_imported",true))
            report.warnings.append(tr("图像已导入，项目状态保存失败。"));
        emit dataImported(m_currentModelId);
    }
    QMessageBox::information(this,tr("分类导入结果"),report.summary());
    return !report.succeeded.isEmpty();
}

// ── 转换结果展示 ──────────────────────────────────────────
void DatasetManagement::showConvertResultDialog(const QList<ConvertResult>& results, int converted, int failed)
{
    const auto& p = ThemeManager::instance().palette();

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("格式转换结果"));
    dlg.setMinimumSize(600, 400);
    dlg.setStyleSheet(QString("QDialog { background: %1; }").arg(p.bgPrimary));

    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);

    // 统计摘要
    int passCount = 0, warnCount = 0, errorCount = 0;
    for (const auto& r : results) {
        if (r.status == ConvertResult::Pass) passCount++;
        else if (r.status == ConvertResult::Warning) warnCount++;
        else errorCount++;
    }

    QLabel* summaryLabel = new QLabel;
    summaryLabel->setStyleSheet(QString("font-size: 13px; font-weight: bold; color: %1;")
        .arg(failed > 0 ? p.danger : (warnCount > 0 ? p.warning : p.success)));
    summaryLabel->setText(QString::fromUtf8("转换完成：%1 成功，%2 失败，%3 警告，%4 通过")
        .arg(converted).arg(failed).arg(warnCount).arg(passCount));
    layout->addWidget(summaryLabel);

    // 结果表格
    QTableWidget* table = new QTableWidget;
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        QString::fromUtf8("状态"),
        QString::fromUtf8("文件"),
        QString::fromUtf8("行号"),
        QString::fromUtf8("描述")
    });
    table->setRowCount(results.size());
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->resizeSection(0, 60);
    table->horizontalHeader()->resizeSection(1, 180);
    table->horizontalHeader()->resizeSection(2, 50);
    table->setStyleSheet(QString(
        "QTableWidget { background: %1; color: %2; border: 1px solid %3;"
        " gridline-color: %4; font-size: 12px; }"
        "QHeaderView::section { background: %5; color: %6; border: none;"
        " padding: 4px; font-weight: bold; }")
        .arg(p.bgPrimary, p.textPrimary, p.borderMedium,
             p.borderLight, p.bgTertiary, p.textSecondary));

    for (int i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        QString statusText, statusColor;
        switch (r.status) {
        case ConvertResult::Pass:
            statusText = QString::fromUtf8("通过"); statusColor = p.success; break;
        case ConvertResult::Warning:
            statusText = QString::fromUtf8("警告"); statusColor = p.warning; break;
        case ConvertResult::Error:
            statusText = QString::fromUtf8("错误"); statusColor = p.danger; break;
        }
        auto* item0 = new QTableWidgetItem(statusText);
        item0->setForeground(QColor(statusColor));
        table->setItem(i, 0, item0);
        table->setItem(i, 1, new QTableWidgetItem(r.file));
        table->setItem(i, 2, new QTableWidgetItem(
            r.line >= 0 ? QString::number(r.line) : QString()));
        table->setItem(i, 3, new QTableWidgetItem(r.message));
    }

    layout->addWidget(table, 1);

    QPushButton* closeBtn = new QPushButton(QString::fromUtf8("关闭"));
    closeBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 6px 24px; font-size: 12px; }"
        "QPushButton:hover { background: %4; }")
        .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.bgHover));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg.exec();
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
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    if(m_classification) {
        if(m_samples.isEmpty()) {QMessageBox::information(this,tr("导出"),tr("没有可导出的分类图像。"));return;}
        const auto generation=m_modelGeneration;
        const auto target=QFileDialog::getExistingDirectory(this,tr("选择空目录导出 OK/NG 分类数据（保留划分）"),m_projectRoot);
        if(target.isEmpty() || generation!=m_modelGeneration) return;
        const auto report=DatasetFileService::exportFiles(DatasetFileService::Classification,target,m_samples,m_selectedIndices.values(),labelsDir(),
            m_labelNames,m_labelType,m_currentModel.modelType,[]{return AccessControl::canWrite("深度学习.数据集管理");});
        QMessageBox::information(this,tr("分类导出结果"),report.summary());return;
    }
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
            QString::fromUtf8("选择空目录导出 PNG 掩码"),
            m_projectRoot);
        if (dir.isEmpty()) return;
        writeMaskFormat(dir, exportIndices);
    } else if (choice.contains("COCO")) {
        QString savePath = QFileDialog::getSaveFileName(this,
            QString::fromUtf8("导出 COCO JSON（请选择空目录）"),
            QDir(datasetDir()).absoluteFilePath("annotations.json"),
            QString::fromUtf8("JSON 文件 (*.json)"));
        if (savePath.isEmpty()) return;
        writeCocoJson(savePath, exportIndices);
    } else {
        QString dir = QFileDialog::getExistingDirectory(this,
            QString::fromUtf8("选择空目录导出 YOLO"),
            m_projectRoot);
        if (dir.isEmpty()) return;
        writeYoloFormat(dir, exportIndices);
    }
}

// ── COCO JSON 导出 ──────────────────────────────────────────
void DatasetManagement::writeCocoJson(const QString& outputPath, const QList<int>& exportIndices)
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    const auto report=DatasetFileService::exportFiles(DatasetFileService::Coco,outputPath,m_samples,exportIndices,labelsDir(),
        m_labelNames,m_labelType,m_currentModel.modelType,[]() {return AccessControl::canWrite("深度学习.数据集管理");});
    QMessageBox box(this);box.setWindowTitle(report.ok()?tr("导出完成"):tr("导出未完成"));
    box.setText(tr("%1\n%2").arg(outputPath).arg(report.summary().section("\n",0,0)));
    box.setDetailedText(report.summary());box.exec();
    if(report.ok()) Logger::info(tr("数据集已导出：%1").arg(outputPath));
}


void DatasetManagement::writeYoloFormat(const QString& outputDir, const QList<int>& exportIndices)
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    const auto report=DatasetFileService::exportFiles(DatasetFileService::Yolo,outputDir,m_samples,exportIndices,labelsDir(),
        m_labelNames,m_labelType,m_currentModel.modelType,[]() {return AccessControl::canWrite("深度学习.数据集管理");});
    QMessageBox box(this);box.setWindowTitle(report.ok()?tr("导出完成"):tr("导出未完成"));
    box.setText(tr("%1\n%2").arg(outputDir).arg(report.summary().section("\n",0,0)));
    box.setDetailedText(report.summary());box.exec();
    if(report.ok()) Logger::info(tr("数据集已导出：%1").arg(outputDir));
}


void DatasetManagement::writeMaskFormat(const QString& outputDir, const QList<int>& exportIndices)
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return;
    const auto report=DatasetFileService::exportFiles(DatasetFileService::Mask,outputDir,m_samples,exportIndices,labelsDir(),
        m_labelNames,m_labelType,m_currentModel.modelType,[]() {return AccessControl::canWrite("深度学习.数据集管理");});
    QMessageBox box(this);box.setWindowTitle(report.ok()?tr("导出完成"):tr("导出未完成"));
    box.setText(tr("%1\n%2").arg(outputDir).arg(report.summary().section("\n",0,0)));
    box.setDetailedText(report.summary());box.exec();
    if(report.ok()) Logger::info(tr("数据集已导出：%1").arg(outputDir));
}


bool DatasetManagement::parseCocoJson(const QString& jsonPath,const QStringList& selectedImages)
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return false;
    QFile f(jsonPath);QJsonParseError parse;
    if(!f.open(QIODevice::ReadOnly)) {QMessageBox::warning(this,tr("COCO导入失败"),jsonPath+"\n"+f.errorString());return false;}
    const auto doc=QJsonDocument::fromJson(f.readAll(),&parse);const auto coco=doc.object();
    QString error;
    if(parse.error!=QJsonParseError::NoError || !coco["images"].isArray() || !coco["annotations"].isArray() || !coco["categories"].isArray()) error=tr("需要有效的images、annotations及categories数组");
    auto names=m_labelNames;QMap<int,int> ids;QSet<QString> catNames;
    for(const auto& value:coco["categories"].toArray()) {
        const auto cat=value.toObject();const auto id=cat["id"].toInt(-1);const auto name=cat["name"].toString().trimmed();
        if(!cat["id"].isDouble() || cat["id"].toDouble()!=id || id<0 || name.isEmpty() || ids.contains(id) || catNames.contains(name)) {error=tr("类别编号或名称重复/无效");break;}
        if(!names.contains(name)) names.append(name);
        ids[id]=names.indexOf(name);catNames.insert(name);
    }
    QMap<int,QJsonArray> annotations;QSet<int> imageIds;
    for(const auto& value:coco["images"].toArray()) {
        const auto image=value.toObject();const auto id=image["id"].toInt(-1);
        if(!image["id"].isDouble() || image["id"].toDouble()!=id || id<0 || imageIds.contains(id)) error=tr("图像编号重复或无效");
        imageIds.insert(id);
    }
    for(const auto& value:coco["annotations"].toArray()) {
        auto ann=value.toObject();const auto id=ann["image_id"].toInt(-1),cat=ann["category_id"].toInt(-1);
        if(!imageIds.contains(id) || !ids.contains(cat)) {error=tr("标注引用了不存在的图像或类别编号");break;}
        ann["category_id"]=ids[cat]+1;annotations[id].append(ann);
    }
    QTemporaryDir stage;QStringList images,labels;QSet<QString> bases;
    if(!stage.isValid()) error=tr("无法创建临时目录");
    bool conflict=false;
    for(const auto& value:coco["images"].toArray()) {
        if(!error.isEmpty()) break;
        const auto info=value.toObject();const auto filename=info["file_name"].toString();const auto clean=QDir::cleanPath(filename);
        if(filename.isEmpty() || QDir::isAbsolutePath(filename) || clean==".." || clean.startsWith("../") || filename.contains('\\')) {error=tr("不安全的图像相对路径：%1").arg(filename);break;}
        QString source=QFileInfo(jsonPath).absoluteDir().filePath(clean);
        if(!QFileInfo::exists(source)) source=QFileInfo(jsonPath).absoluteDir().filePath("images/"+clean);
        if(!QFileInfo::exists(source)) {
            QStringList candidates;for(const auto& selected:selectedImages) if(QFileInfo(selected).fileName().compare(QFileInfo(clean).fileName(),Qt::CaseInsensitive)==0) candidates.append(selected);
            if(candidates.size()>1) {error=tr("所选图像同名，无法确定COCO配对：%1").arg(filename);break;}
            source=candidates.isEmpty()?QDir(imagesDir()).filePath(QFileInfo(clean).fileName()):candidates.first();
        }
        const QImage image(source);const auto base=QFileInfo(clean).completeBaseName().toCaseFolded();
        if(image.isNull() || bases.contains(base)) {error=tr("图像损坏、缺失或同基名配对不唯一：%1").arg(source);break;}bases.insert(base);
        if((info["width"].toInt()>0 && info["width"].toInt()!=image.width()) || (info["height"].toInt()>0 && info["height"].toInt()!=image.height())) {error=tr("COCO声明尺寸与图像不一致：%1").arg(source);break;}
        const auto temporary=stage.filePath("input.json");
        if(!AtomicFile::write(temporary,QJsonDocument(QJsonObject{{"annotations",annotations[info["id"].toInt()]}}).toJson(),&error)) break;
        QList<AnnotationShape> shapes;
        if(!AnnotationStore::load(temporary,image.size(),m_labelType==LabelTypes::BinaryMask,names,shapes,error)) break;
        const auto label=stage.filePath(QFileInfo(clean).completeBaseName()+(LabelTypes::isMaskType(m_labelType)?".png":".json"));
        const auto result=AnnotationStore::save(label,QFileInfo(clean).fileName(),image,shapes,LabelTypes::isMaskType(m_labelType),m_labelType==LabelTypes::BinaryMask,names.size());
        if(!result.ok) {error=source+"\n"+result.error;break;}
        images.append(source);labels.append(label);
        conflict|=QFileInfo::exists(QDir(imagesDir()).filePath(QFileInfo(clean).fileName())) || QFileInfo::exists(QDir(labelsDir()).filePath(QFileInfo(label).fileName()));
    }
    if(!error.isEmpty()) {QMessageBox::warning(this,tr("COCO导入失败"),jsonPath+"\n"+error);return false;}
    bool overwrite=false;
    if(conflict) {
        const auto choice=QMessageBox::question(this,tr("COCO文件冲突"),tr("是：成对覆盖；否：同名图像及标签一起跳过；取消：不写入。"),QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,QMessageBox::Cancel);
        if(choice==QMessageBox::Cancel) return false;overwrite=choice==QMessageBox::Yes;
    }
    if(!WriteAccess::require("深度学习.数据集管理",this)) return false;
    // Persist appended IDs first; existing slots are never reordered or overwritten.
    const auto oldNames=m_labelNames,oldColors=m_labelColors;const auto oldCounts=m_labelInstanceCounts;
    m_labelNames=names;
    while(m_labelColors.size()<names.size()) {m_labelColors.append(s_defaultColors[m_labelColors.size()%s_defaultColors.size()]);m_labelInstanceCounts.append(0);}
    if(!saveDatasetConfig()) {m_labelNames=oldNames;m_labelColors=oldColors;m_labelInstanceCounts=oldCounts;return false;}
    const auto report=DatasetFileService::importFiles(images,labels,imagesDir(),labelsDir(),overwrite,[] {return AccessControl::canWrite("深度学习.数据集管理");});
    updateLabelPanel();emit labelsChanged();
    QMessageBox box(this);box.setWindowTitle(tr("COCO导入结果"));box.setText(report.summary().section("\n",0,0));box.setDetailedText(jsonPath+"\n"+report.summary());box.exec();
    return report.ok();

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
void DatasetManagement::refreshAnnotationStatistics()
{
    m_labelInstanceCounts=QList<int>(m_labelNames.size(),0);
    if(m_classification) {
        for(auto& sample:m_samples) {
            const int category=int(m_labelNames.indexOf(sample.fileName.section('/',1,1)));
            sample.annotated=category>=0;sample.categoryIds.clear();sample.instanceCount=sample.annotated?1:0;
            if(sample.annotated) {sample.categoryIds={category};++m_labelInstanceCounts[category];}
        }
        return;
    }
    const QStringList suffixes=LabelTypes::isMaskType(m_labelType)?QStringList{"png","json","txt"}:QStringList{"json","txt","png"};
    for(auto& sample:m_samples) {
        sample.annotated=false;sample.categoryIds.clear();sample.instanceCount=0;
        for(const auto& suffix:suffixes) {
            const auto path=QDir(labelsDir()).filePath(QFileInfo(sample.absolutePath).completeBaseName()+"."+suffix);
            if(!QFileInfo::exists(path)) continue;
            sample.annotated=true;QList<AnnotationShape> shapes;QString error;
            if(DatasetFileService::readShapes(path,QSize(sample.width,sample.height),m_labelNames,m_labelType==LabelTypes::BinaryMask,shapes,error)) {
                for(const auto& shape:shapes) if(shape.visible) {
                    ++sample.instanceCount;++m_labelInstanceCounts[shape.labelId];
                    if(!sample.categoryIds.contains(shape.labelId)) sample.categoryIds.append(shape.labelId);
                }
            } else Logger::warn(path+"："+error);
            break;
        }
    }
}

void DatasetManagement::runQualityCheck(DatasetStats& stats) const
{
    stats.warnings.clear();stats.missingAnnotations=0;stats.orphanAnnotations=0;stats.corruptedFiles=0;stats.emptyAnnotations=0;
    QSet<QString> bases;
    for(const auto& sample:m_samples) {
        const auto base=QFileInfo(sample.absolutePath).completeBaseName();bases.insert(base);
        const QImage image(sample.absolutePath);
        if(image.isNull()) {++stats.corruptedFiles;stats.warnings.append(tr("损坏图像：%1").arg(sample.absolutePath));}
        if(m_classification) continue;
        bool found=false;
        for(const auto& suffix:{"json","png","txt"}) {
            const auto path=QDir(labelsDir()).filePath(base+"."+suffix);if(!QFileInfo::exists(path)) continue;found=true;
            QList<AnnotationShape> shapes;QString error;
            if(!DatasetFileService::readShapes(path,image.size(),m_labelNames,m_labelType==LabelTypes::BinaryMask,shapes,error)) {
                ++stats.corruptedFiles;stats.warnings.append(tr("非法标注：%1：%2").arg(path,error));
            } else if(shapes.isEmpty()) {++stats.emptyAnnotations;stats.warnings.append(tr("空标注（可作为负样本）：%1").arg(path));}
        }
        if(!found) {++stats.missingAnnotations;stats.warnings.append(tr("缺少配对标签：%1").arg(sample.absolutePath));}
    }
    if(m_classification) return;
    for(const auto& file:QDir(labelsDir()).entryInfoList({"*.json","*.png","*.txt","*.xml"},QDir::Files))
        if(!bases.contains(file.completeBaseName())) {++stats.orphanAnnotations;stats.warnings.append(tr("孤立标注：%1").arg(file.absoluteFilePath()));}
}

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
void DatasetManagement::applyPermissions()
{
    const bool write=AccessControl::canWrite("深度学习.数据集管理") && hasModel();
    for(auto* button:{m_btnAddLabel,m_btnEditLabel,m_btnDeleteLabel,m_btnImportLabels,m_btnEmptyImport,m_btnEmptyImportFolder,m_btnFormatConvert,m_btnExport})
        if(button) button->setEnabled(write);
    for(auto* button:{m_btnAddLabel,m_btnEditLabel,m_btnDeleteLabel,m_btnImportLabels}) if(button) button->setEnabled(write && !m_classification);
    m_labelListWidget->setDragDropMode(write && !m_classification?QAbstractItemView::InternalMove:QAbstractItemView::NoDragDrop);
    m_btnDeleteSelected->setEnabled(write && !m_selectedIndices.isEmpty());
}

bool DatasetManagement::hasPersistedAnnotations() const
{
    if(!hasModel()) return false;
    QDirIterator files(labelsDir(),{"*.png","*.json","*.txt","*.xml"},QDir::Files,QDirIterator::Subdirectories);
    return files.hasNext();
}

void DatasetManagement::importSelectedFiles(const QStringList& images,const QStringList& labels)
{
    if(!WriteAccess::require("深度学习.数据集管理",this) || !hasModel()) return;
    if(m_classification) {QMessageBox::information(this,tr("分类导入"),tr("请使用 OK/NG 图像导入或分类文件夹导入。"));return;}
    if(images.isEmpty() && labels.isEmpty()) return;
    if(hasPendingAnnotations && hasPendingAnnotations()) {QMessageBox::warning(this,tr("请先处理标注"),tr("请先保存或放弃未保存标注，再导入文件。"));return;}
    QStringList ordinary,coco;QSet<QString> aggregateImages;
    for(const auto& path:labels) {
        QFile f(path);if(QFileInfo(path).suffix().compare("json",Qt::CaseInsensitive)==0 && f.open(QIODevice::ReadOnly)) {
            const auto root=QJsonDocument::fromJson(f.readAll()).object();
            if(root["images"].isArray() && root["categories"].isArray()) {coco.append(path);for(const auto& image:root["images"].toArray()) aggregateImages.insert(QFileInfo(image.toObject()["file_name"].toString()).fileName().toCaseFolded());continue;}
        }
        ordinary.append(path);
    }
    QStringList standalone;for(const auto& image:images) if(!aggregateImages.contains(QFileInfo(image).fileName().toCaseFolded())) standalone.append(image);
    bool conflict=false;
    for(const auto& path:standalone) conflict|=QFileInfo::exists(QDir(imagesDir()).filePath(QFileInfo(path).fileName()));
    for(const auto& path:ordinary) conflict|=QFileInfo::exists(QDir(labelsDir()).filePath(QFileInfo(path).fileName()));
    bool overwrite=false;
    if(conflict) {
        const auto choice=QMessageBox::question(this,tr("文件冲突"),tr("目标存在同名文件。是：成对覆盖；否：同名图像及其配对标签一起跳过；取消：不导入。"),QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,QMessageBox::Cancel);
        if(choice==QMessageBox::Cancel) return;overwrite=choice==QMessageBox::Yes;
    }
    auto report=DatasetFileService::importFiles(standalone,ordinary,imagesDir(),labelsDir(),overwrite,
        []() {return AccessControl::canWrite("深度学习.数据集管理");},m_labelNames,m_labelType==LabelTypes::BinaryMask,currentLabelOffset());
    for(const auto& path:coco) {
        if(parseCocoJson(path,images)) report.succeeded.append(path);else {report.failures.append(path+tr("：COCO导入未全部完成，查看详细错误后可重试。"));report.failedSources.append(path);}
    }
    if(!report.succeeded.isEmpty()) {
        emit datasetFilesChanged();
        loadDatasetConfig();detectLabelsFromAnnotations();saveDatasetConfig();loadDatasetConfig();
        refreshGallery();updateModelInfoPanel();applyPermissions();
        if(!ProjectConfig::setModelStatus(m_project.fvprojPath,m_currentModelId,"data_imported",true))
            report.warnings.append(tr("数据已导入，但项目状态更新失败：%1").arg(m_project.fvprojPath));
        emit dataImported(m_currentModelId);
    }
    QMessageBox box(this);box.setWindowTitle(tr("导入结果"));box.setText(report.summary().section("\n",0,0));box.setDetailedText(report.summary());
    box.setStandardButtons(report.failedSources.isEmpty()?QMessageBox::Ok:QMessageBox::Retry|QMessageBox::Close);
    if(box.exec()==QMessageBox::Retry) {
        QStringList retryImages,retryLabels;
        for(const auto& path:report.failedSources) (images.contains(path)?retryImages:retryLabels).append(path);
        importSelectedFiles(retryImages,retryLabels);
    }
}

bool DatasetManagement::convertAnnotations(const QString& suffix)
{
    if(!WriteAccess::require("深度学习.数据集管理",this)) return false;
    if(hasPendingAnnotations && hasPendingAnnotations()) {QMessageBox::warning(this,tr("请先处理标注"),tr("请先保存或放弃未保存标注，再转换文件。"));return false;}
    QList<ConvertResult> results;int converted=0,failed=0;
    const auto task=m_currentModel.modelType==ModelTaskTypes::InstanceSeg?"segment":m_currentModel.modelType==ModelTaskTypes::ObjDetectFixed?"obb":"detect";
    QDirIterator files(labelsDir(),{"*."+suffix},QDir::Files,QDirIterator::Subdirectories);
    while(files.hasNext()) {
        const auto path=files.next();const auto fi=QFileInfo(path);QString error;
        const auto relative=QDir(labelsDir()).relativeFilePath(fi.absolutePath());
        QDir imageFolder(QDir(imagesDir()).filePath(relative));QString imagePath;
        for(const auto& image:imageFolder.entryInfoList(QDir::Files)) if(image.completeBaseName()==fi.completeBaseName()) {
            if(!imagePath.isEmpty()) {error=tr("同基名图像配对有歧义");break;}imagePath=image.absoluteFilePath();
        }
        const QImage image(imagePath);if(image.isNull()) error=tr("缺少配对图像或图像损坏");
        QList<AnnotationShape> shapes;QByteArray bytes;QStringList warnings;
        if(error.isEmpty()) DatasetFileService::readShapes(path,image.size(),m_labelNames,m_labelType==LabelTypes::BinaryMask,shapes,error);
        if(error.isEmpty()) DatasetFileService::yoloText(shapes,image.size(),task,bytes,warnings,error);
        const auto output=fi.absolutePath()+"/"+fi.completeBaseName()+".txt";
        if(error.isEmpty() && QFileInfo::exists(output)) {
            QFile old(output);if(old.open(QIODevice::ReadOnly) && old.readAll()==bytes) {
                ConvertResult row;row.file=path;row.status=ConvertResult::Pass;row.message=tr("结果已一致，跳过写入");results.append(row);continue;
            }
            const auto choice=QMessageBox::question(this,tr("转换覆盖确认"),tr("转换将替换已有标签：%1").arg(output),QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,QMessageBox::Cancel);
            if(choice==QMessageBox::Cancel) break;
            if(choice==QMessageBox::No) {ConvertResult row;row.file=path;row.status=ConvertResult::Warning;row.message=tr("跳过已有输出");results.append(row);continue;}
        }
        if(error.isEmpty() && !AccessControl::canWrite("深度学习.数据集管理")) error=tr("提交前权限变化，未写入");
        if(error.isEmpty()) AtomicFile::write(output,bytes,&error);
        ConvertResult row;row.file=path;
        if(error.isEmpty()) {++converted;row.status=warnings.isEmpty()?ConvertResult::Pass:ConvertResult::Warning;row.message=tr("已写入：%1").arg(output)+"\n"+warnings.join("\n");}
        else {++failed;row.status=ConvertResult::Error;row.message=error;}
        results.append(row);
    }
    showConvertResultDialog(results,converted,failed);return converted>0 && failed==0;
}
