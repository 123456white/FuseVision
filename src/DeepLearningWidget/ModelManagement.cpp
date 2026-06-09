#include "ModelManagement.h"
#include "ModelConfig.h"
#include "ProjectManagement.h"  // StepGuideBar
#include "core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QIcon>
#include <QFont>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>

// ── 任务类型描述（8行左右）───────────────────────────────────
static const char* s_typeDescs[] = {
    "对图像中的每个像素进行逐像素分类，\n"
    "生成高精度语义分割掩码。\n"
    "适用于：缺陷检测、目标分割、\n"
    "医学影像分析、遥感地物分类、\n"
    "工业质检等需要精确轮廓的\n"
    "像素级理解任务。",

    "仅需正常样本即可训练，\n"
    "自动学习正常模式分布，\n"
    "检测偏离正常范围的异常区域。\n"
    "适用于：工业瑕疵检测、\n"
    "异常行为识别、设备故障\n"
    "预警等少见缺陷场景。",

    "检测目标并输出不可旋转的\n"
    "水平矩形边界框（X/Y/W/H）。\n"
    "适用于：行人检测、车辆识别、\n"
    "零件计数、通用目标检测等\n"
    "传统定位与分类任务。",

    "检测目标并以可自由旋转的\n"
    "矩形框标注（中心点+角度），\n"
    "精准贴合任意方向物体。\n"
    "适用于：倾斜文字检测、\n"
    "遥感图像旋转目标、任意\n"
    "方向零件定位等场景。"
};

// ── 类型图标资源（浅色）─────────────────────────────────────
static const char* s_typeIconRes[] = {
    ":/res/TaskSegmentation.png",     // 实例分割
    ":/res/TaskAnomaly.png",          // 异常检测
    ":/res/TaskDetectionAxis.png",    // 物体检测(非自由矩形)
    ":/res/TaskDetectionRotated.png", // 物体检测(自由矩形)
};

// ── 类型图标资源（暗色）─────────────────────────────────────
static const char* s_typeIconResDark[] = {
    ":/res/TaskSegmentation_D.png",
    ":/res/TaskAnomaly_D.png",
    ":/res/TaskDetectionAxis_D.png",
    ":/res/TaskDetectionRotated_D.png",
};

// =============================================================================
// ModelCard 实现（紧凑卡片）
// =============================================================================

ModelCard::ModelCard(const QString& modelId, const QString& modelType, QWidget* parent)
    : QFrame(parent), m_modelId(modelId), m_modelType(modelType)
{
    setFixedSize(200, 210);
    setCursor(Qt::PointingHandCursor);
    setObjectName("modelCard");

    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    m_clickTimer->setInterval(300);  // 300ms 内双击则取消单击事件
    connect(m_clickTimer, &QTimer::timeout, this, &ModelCard::onClickTimer);

    // 卡片图标（使用项目图标）
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // 图标
    m_imageLabel = new QLabel;
    m_imageLabel->setFixedSize(180, 160);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setObjectName("mcImage");
    QPixmap pix(":/res/FuseVisionProject.png");
    if (!pix.isNull())
        m_imageLabel->setPixmap(pix.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 名称 + 类型合并
    m_infoLabel = new QLabel(QString("%1\n%2").arg(modelId, modelType));
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setObjectName("mcInfo");
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setMaximumHeight(36);

    layout->addWidget(m_imageLabel, 0, Qt::AlignHCenter);
    layout->addWidget(m_infoLabel, 1);

    applyCardStyle();
}

ModelCard::~ModelCard()
{
    m_clickTimer->stop();
}

void ModelCard::setSelected(bool selected)
{
    m_selected = selected;
    applyCardStyle();
}

void ModelCard::refreshTheme()
{
    const auto& p = ThemeManager::instance().palette();
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);

    m_infoLabel->setStyleSheet(
        QString("QLabel#mcInfo { color: %1; font-size: 10px; font-weight: bold; }").arg(p.textPrimary));
    m_imageLabel->setStyleSheet(
        QString("QLabel#mcImage { background-color: %1; border-radius: 8px; }")
            .arg(p.bgTertiary));

    QPixmap pix(isDark ? ":/res/FuseVisionProject_D.png" : ":/res/FuseVisionProject.png");
    if (!pix.isNull())
        m_imageLabel->setPixmap(pix.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    applyCardStyle();
}

void ModelCard::applyCardStyle()
{
    const auto& p = ThemeManager::instance().palette();

    QString borderColor;
    QString bg;
    if (m_selected) {
        borderColor = p.accentPrimary;
        bg = p.bgSelected;
    } else if (m_hovered) {
        borderColor = p.accentPrimary;
        bg = p.bgHover;
    } else {
        borderColor = p.borderLight;
        bg = p.bgSecondary;
    }

    setStyleSheet(
        QString("QFrame#modelCard { background-color: %1; border: 2px solid %2;"
                " border-radius: 10px; }").arg(bg, borderColor));
}

void ModelCard::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_clickTimer->start();  // 启动延迟，单击事件在 300ms 后触发
}

void ModelCard::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_clickTimer->stop();   // 双击时取消单击事件
    emit doubleClicked(this);
}

void ModelCard::onClickTimer()
{
    emit clicked(this);     // 300ms 无双击，则发出单击信号
}

void ModelCard::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hovered = true;
    if (!m_selected) applyCardStyle();
}

void ModelCard::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hovered = false;
    if (!m_selected) applyCardStyle();
}

// =============================================================================
// ModelManagement 实现
// =============================================================================

ModelManagement::ModelManagement(QWidget* parent)
    : QWidget(parent)
{
    initUI();
    applyTheme();
    // 初始空状态
    updateLeftInfo();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ModelManagement::applyTheme);
}

void ModelManagement::setCurrentProject(const FvprojInfo& project)
{
    m_project      = project;
    m_dataRoot     = project.dataRoot;
    m_datasetRoot  = project.datasetRoot;
    m_modelRoot    = project.modelRoot;
    m_currentModelId.clear();
    m_currentModel  = FvdlInfo();
    refreshCards();
    updateLeftInfo();
    updateStepGuide();

    if (project.projectName.isEmpty())
        Logger::debug("模型管理：项目已关闭");
    else
        Logger::info(QString::fromUtf8("模型管理：绑定项目 \"%1\" (%2 个模型)")
            .arg(project.projectName).arg(project.models.size()));
}

// ── UI 构建 ───────────────────────────────────────────────────

void ModelManagement::initUI()
{
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

// ── 左侧边栏 ──────────────────────────────────────────────────

void ModelManagement::initLeftPanel()
{
    m_leftSplitter = new QSplitter(Qt::Vertical);
    m_leftSplitter->setHandleWidth(1);
    m_leftSplitter->setChildrenCollapsible(false);

    // ===== 上栏：新建/打开模型按钮 =====
    QWidget* topPanel = new QWidget;
    topPanel->setObjectName("mmLeftTop");
    QVBoxLayout* topLayout = new QVBoxLayout(topPanel);
    topLayout->setContentsMargins(10, 12, 10, 8);
    topLayout->setSpacing(8);

    m_newModelBtn = new QPushButton(QString::fromUtf8("  新建模型"));
    m_newModelBtn->setIcon(QIcon(":/res/NewProject.png"));
    m_newModelBtn->setIconSize(QSize(20, 20));
    m_newModelBtn->setCursor(Qt::PointingHandCursor);
    m_newModelBtn->setObjectName("mmNewModelBtn");
    connect(m_newModelBtn, &QPushButton::clicked, this, &ModelManagement::onNewModel);

    m_openModelBtn = new QPushButton(QString::fromUtf8("  打开模型"));
    m_openModelBtn->setIcon(QIcon(":/res/OpenProject.png"));
    m_openModelBtn->setIconSize(QSize(20, 20));
    m_openModelBtn->setCursor(Qt::PointingHandCursor);
    m_openModelBtn->setObjectName("mmOpenModelBtn");
    connect(m_openModelBtn, &QPushButton::clicked, this, &ModelManagement::onOpenModel);

    topLayout->addWidget(m_newModelBtn);
    topLayout->addWidget(m_openModelBtn);
    topLayout->addStretch();

    // ===== 下栏：当前模型详细信息 =====
    QWidget* bottomPanel = new QWidget;
    bottomPanel->setObjectName("mmLeftBottom");
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(10, 8, 10, 12);
    bottomLayout->setSpacing(8);

    QLabel* infoTitle = new QLabel(QString::fromUtf8("当前模型"));
    QFont titleFont = infoTitle->font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    infoTitle->setFont(titleFont);
    infoTitle->setObjectName("mmInfoTitle");
    bottomLayout->addWidget(infoTitle);

    m_detailLabel = new QLabel(QString::fromUtf8("--"));
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setObjectName("mmDetailLabel");
    m_detailLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    QFont df = m_detailLabel->font();
    df.setPointSize(10);
    m_detailLabel->setFont(df);
    bottomLayout->addWidget(m_detailLabel, 1);

    bottomLayout->addStretch();

    m_leftSplitter->addWidget(topPanel);
    m_leftSplitter->addWidget(bottomPanel);
    m_leftSplitter->setSizes({ 90, 810 });
}

// ── 右侧主体（纯卡片画廊，无详情面板）─────────────────────────

void ModelManagement::initRightPanel()
{
    m_rightContainer = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_rightContainer);
    layout->setContentsMargins(16, 16, 16, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("mmScrollArea");

    m_cardContainer = new QWidget;
    m_cardContainer->setObjectName("mmCardContainer");
    m_cardLayout = new QGridLayout(m_cardContainer);
    m_cardLayout->setContentsMargins(8, 8, 8, 8);
    m_cardLayout->setSpacing(16);
    m_cardLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_emptyHint = new QLabel(QString::fromUtf8(
        "当前项目无模型\n\n"
        "可点击左侧\"新建模型\"创建新模型，\n"
        "或点击\"打开模型\"选择已有 .fvdl 文件。"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setObjectName("mmEmptyHint");
    m_emptyHint->setWordWrap(true);

    QWidget* stackWidget = new QWidget;
    QVBoxLayout* stackLayout = new QVBoxLayout(stackWidget);
    stackLayout->setContentsMargins(0, 0, 0, 0);
    stackLayout->addWidget(m_emptyHint);
    stackLayout->addWidget(m_cardContainer);

    m_scrollArea->setWidget(stackWidget);
    layout->addWidget(m_scrollArea, 1);

    // 步骤引导条（直接占满宽度）
    m_stepGuideBar = new StepGuideBar;
    m_stepGuideBar->setCurrentStep(1);       // 当前在模型管理
    m_stepGuideBar->setStepCompleted(0);     // 项目管理已完成
    layout->addWidget(m_stepGuideBar);
}

// ── 主题刷新 ──────────────────────────────────────────────────

void ModelManagement::applyTheme()
{
    const auto& p = ThemeManager::instance().palette();
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);

    // ── 左侧面板背景 ──
    if (auto* w = m_leftSplitter->widget(0))
        w->setStyleSheet(QString("#mmLeftTop { background-color: %1; }").arg(p.bgSecondary));
    if (auto* w = m_leftSplitter->widget(1))
        w->setStyleSheet(QString("#mmLeftBottom { background-color: %1; }").arg(p.bgSecondary));

    // ── 新建模型按钮（主按钮）──
    m_newModelBtn->setStyleSheet(
        QString("QPushButton#mmNewModelBtn {"
                "  background-color: %1; color: #ffffff; border: none;"
                "  border-radius: 8px; padding: 10px 16px; font-size: 13px;"
                "  font-weight: bold; text-align: left; }"
                "QPushButton#mmNewModelBtn:hover { background-color: %2; }")
            .arg(p.accentPrimary, p.accentHover));

    // ── 打开模型按钮（描边按钮）──
    m_openModelBtn->setStyleSheet(
        QString("QPushButton#mmOpenModelBtn {"
                "  background-color: transparent; color: %1;"
                "  border: 1px solid %2; border-radius: 8px;"
                "  padding: 10px 16px; font-size: 13px; font-weight: bold;"
                "  text-align: left; }"
                "QPushButton#mmOpenModelBtn:hover { background-color: %3; }")
            .arg(p.accentPrimary, p.accentPrimary, p.bgHover));

    // ── 左侧详情标签、标题 ──
    if (auto* title = findChild<QLabel*>("mmInfoTitle"))
        title->setStyleSheet(
            QString("QLabel { color: %1; }").arg(p.textPrimary));
    m_detailLabel->setStyleSheet(
        QString("QLabel#mmDetailLabel { color: %1; line-height: 1.6;"
                " background-color: %2; border-radius: 6px; padding: 10px;"
                " border: 1px solid %3; }")
            .arg(p.textPrimary, p.bgTertiary, p.borderLight));

    // ── 右侧容器、卡片区、空状态 ──
    m_rightContainer->setStyleSheet(QString("QWidget { background-color: %1; }").arg(p.bgPrimary));
    m_cardContainer->setStyleSheet(
        QString("#mmCardContainer { background-color: %1; }").arg(p.bgPrimary));
    m_emptyHint->setStyleSheet(
        QString("QLabel#mmEmptyHint { color: %1; font-size: 15px; }").arg(p.textMuted));
    m_scrollArea->setStyleSheet(
        QString("QScrollArea#mmScrollArea { border: none; background-color: %1; }")
            .arg(p.bgPrimary));

    // ── 图标暗色适配 ──
    m_newModelBtn->setIcon(QIcon(isDark ? ":/res/NewProject_D.png" : ":/res/NewProject.png"));
    m_openModelBtn->setIcon(QIcon(isDark ? ":/res/OpenProject_D.png" : ":/res/OpenProject.png"));

    // ── 刷新所有卡片主题 ──
    for (ModelCard* card : m_cards)
        card->refreshTheme();
}

// ── 新建模型 ──────────────────────────────────────────────────

void ModelManagement::onNewModel()
{
    if (m_project.projectName.isEmpty()) {
        Logger::warn("onNewModel: 未打开项目，已拒绝");
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在项目管理中打开一个项目。"));
        return;
    }

    const auto& p = ThemeManager::instance().palette();

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("新建模型"));
    dlg.setMinimumSize(560, 480);
    dlg.setStyleSheet(
        QString("QDialog { background-color: %1; } QLabel { color: %2; }")
            .arg(p.bgSecondary, p.textPrimary));

    QVBoxLayout* mainDlgLayout = new QVBoxLayout(&dlg);

    // ── 第一栏：任务类型选择（图标+名称，选中高亮边框，右栏=描述）──
    QHBoxLayout* typeRow = new QHBoxLayout;

    QWidget* typeLeftWidget = new QWidget;
    QVBoxLayout* typeLeftLayout = new QVBoxLayout(typeLeftWidget);
    typeLeftLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* typeTitle = new QLabel(QString::fromUtf8("深度学习类型"));
    QFont boldFont = typeTitle->font();
    boldFont.setBold(true);
    typeTitle->setFont(boldFont);
    typeLeftLayout->addWidget(typeTitle);

    QButtonGroup* typeGroup = new QButtonGroup(&dlg);
    QStringList types = ModelTaskTypes::all();
    QList<QPushButton*> typeBtns;
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    const auto& iconSet = isDark ? s_typeIconResDark : s_typeIconRes;

    for (int i = 0; i < types.size(); ++i) {
        QPushButton* btn = new QPushButton(
            QIcon(iconSet[i]),
            QString("  %1").arg(types[i]));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName("mmTypeBtn");
        btn->setStyleSheet(
            QString("QPushButton#mmTypeBtn {"
                    "  text-align: left; padding: 6px 10px; font-size: 12px;"
                    "  border: 2px solid %1; border-radius: 6px;"
                    "  background-color: %2; color: %3; }"
                    "QPushButton#mmTypeBtn:checked {"
                    "  border: 2px solid %4; background-color: %5; color: %6; }"
                    "QPushButton#mmTypeBtn:hover:!checked {"
                    "  background-color: %7; }")
                .arg(p.borderLight, p.bgSecondary, p.textPrimary,
                     p.accentPrimary, p.bgSelected, p.accentPrimary,
                     p.bgHover));
        btn->setIconSize(QSize(20, 20));
        typeGroup->addButton(btn, i);
        typeLeftLayout->addWidget(btn);
        typeBtns.append(btn);
    }
    typeBtns[0]->setChecked(true);

    QLabel* typeDescLabel = new QLabel(QString::fromUtf8(s_typeDescs[0]));
    typeDescLabel->setWordWrap(true);
    typeDescLabel->setMinimumWidth(220);
    typeDescLabel->setMinimumHeight(130);
    typeDescLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 11px; padding: 8px;"
                " background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary, p.bgTertiary));

    for (int i = 0; i < typeBtns.size(); ++i) {
        connect(typeBtns[i], &QPushButton::toggled, this,
                [i, typeDescLabel](bool checked) {
                    if (checked) typeDescLabel->setText(QString::fromUtf8(s_typeDescs[i]));
                });
    }

    typeRow->addWidget(typeLeftWidget);
    typeRow->addWidget(typeDescLabel, 1);

    mainDlgLayout->addLayout(typeRow);

    // ── 分隔线 ──
    QFrame* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QString("QFrame { color: %1; }").arg(p.borderLight));
    mainDlgLayout->addWidget(sep1);

    // ── 第二栏：模型名称 + 模型文件路径 ──
    QFormLayout* form = new QFormLayout;
    form->setSpacing(10);

    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QString::fromUtf8("仅允许英文字母、数字和下划线"));
    nameEdit->setMaxLength(64);
    nameEdit->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2;"
                " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.accentPrimary));
    form->addRow(QString::fromUtf8("模型名称 *"), nameEdit);

    // 模型文件路径
    QHBoxLayout* pathRow = new QHBoxLayout;
    QLineEdit* pathEdit = new QLineEdit;
    QString defaultPath = QDir(m_modelRoot).absoluteFilePath("model" + QString(".fvdl"));
    pathEdit->setText(defaultPath);
    pathEdit->setReadOnly(true);
    pathEdit->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2;"
                " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }")
            .arg(p.bgTertiary, p.textMuted, p.borderLight));

    QPushButton* browseBtn = new QPushButton(QString::fromUtf8("浏览"));
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; border: 1px solid %3;"
                " border-radius: 4px; padding: 6px 12px; }"
                "QPushButton:hover { background-color: %4; }")
            .arg(p.bgSecondary, p.textPrimary, p.borderMedium, p.bgHover));

    connect(browseBtn, &QPushButton::clicked, this, [&dlg, pathEdit, this]() {
        QString path = QFileDialog::getSaveFileName(
            &dlg,
            QString::fromUtf8("保存模型文件"),
            m_modelRoot,
            QString::fromUtf8("FuseVision 模型文件 (*.fvdl)"));
        if (!path.isEmpty()) pathEdit->setText(path);
    });

    connect(nameEdit, &QLineEdit::textChanged, this, [nameEdit, pathEdit, this](const QString& text) {
        QString name = text.trimmed();
        if (!name.isEmpty()) {
            QString newPath = QDir(m_modelRoot).absoluteFilePath(name + ".fvdl");
            pathEdit->setText(newPath);
        }
    });

    pathRow->addWidget(pathEdit, 1);
    pathRow->addWidget(browseBtn);
    form->addRow(QString::fromUtf8("文件路径"), pathRow);

    // ── 模型描述 ──
    QTextEdit* descEdit = new QTextEdit;
    descEdit->setPlaceholderText(QString::fromUtf8("可选，简要描述模型用途"));
    descEdit->setMaximumHeight(60);
    descEdit->setStyleSheet(
        QString("QTextEdit { background-color: %1; color: %2;"
                " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }"
                "QTextEdit:focus { border-color: %4; }")
            .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.accentPrimary));
    form->addRow(QString::fromUtf8("模型描述"), descEdit);

    mainDlgLayout->addLayout(form);

    // ── 第三栏：按钮 ──
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("创建模型"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainDlgLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("创建失败"),
                             QString::fromUtf8("模型名称不能为空。"));
        return;
    }

    QString type = types[typeGroup->checkedId()];

    QString error;
    if (!ModelConfig::createModel(name, type, m_modelRoot, m_datasetRoot,
                                    m_project.projectName, m_project.fvprojPath, &error)) {
        QMessageBox::warning(this, QString::fromUtf8("创建失败"), error);
        return;
    }

    // 重新加载项目信息
    m_project = ProjectConfig::loadProject(m_project.fvprojPath, &error);
    refreshCards();

    // 自动选中新建模型
    QSignalBlocker blk(this);
    for (ModelCard* card : m_cards) {
        if (card->modelId() == name) {
            onCardClicked(card);
            break;
        }
    }

    Logger::info(QString::fromUtf8("模型已创建: %1 (%2)").arg(name, type));
    emit modelCreated(name);
}

// ── 打开模型 ──────────────────────────────────────────────────

void ModelManagement::onOpenModel()
{
    if (m_project.projectName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("请先在项目管理中打开一个项目。"));
        return;
    }

    QString fvdlPath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("打开模型"),
        m_modelRoot,
        QString::fromUtf8("FuseVision 模型文件 (*.fvdl)"));

    if (fvdlPath.isEmpty()) return;

    QString error;
    FvdlInfo info = ModelConfig::loadModel(fvdlPath, &error);
    if (info.modelName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("打开失败"), error);
        return;
    }

    if (info.projectRef != m_project.projectName) {
        QMessageBox::warning(this, QString::fromUtf8("项目不匹配"),
            QString::fromUtf8("模型 \"%1\" 不属于当前项目 \"%2\"。")
                .arg(info.modelName, m_project.projectName));
        return;
    }

    m_currentModel   = info;
    m_currentModelId = info.modelName;
    updateLeftInfo();
    updateStepGuide();

    // 同步卡片选中状态
    for (ModelCard* card : m_cards) {
        card->setSelected(card->modelId() == info.modelName);
        if (card->modelId() == info.modelName)
            m_selectedCard = card;
    }

    Logger::info(QString::fromUtf8("模型已打开: %1 (%2)").arg(info.modelName, info.modelType));
    emit modelOpened(info.modelName);
}

// ── 卡片单击 ──────────────────────────────────────────────────

void ModelManagement::onCardClicked(ModelCard* card)
{
    if (!card) return;
    if (m_selectedCard)
        m_selectedCard->setSelected(false);
    card->setSelected(true);
    m_selectedCard = card;

    QString modelId = card->modelId();
    QJsonObject models = m_project.models;
    if (!models.contains(modelId)) return;

    QJsonObject entry = models[modelId].toObject();
    QString fvdlPath = entry["model_file"].toString();

    QString error;
    FvdlInfo info = ModelConfig::loadModel(fvdlPath, &error);
    if (info.modelName.isEmpty()) return;

    m_currentModel   = info;
    m_currentModelId = modelId;
    updateLeftInfo();
    updateStepGuide();
    Logger::info(QString::fromUtf8("模型单卡选中: %1 (%2)").arg(modelId, info.modelType));
    emit currentModelChanged(modelId);
}

// ── 卡片双击 → 进入数据集管理页 ───────────────────────────────

void ModelManagement::onCardDoubleClicked(ModelCard* card)
{
    if (!card) return;
    QString modelId = card->modelId();

    // 加载模型信息并更新状态
    QJsonObject models = m_project.models;
    if (!models.contains(modelId)) return;

    QJsonObject entry = models[modelId].toObject();
    QString fvdlPath = entry["model_file"].toString();

    QString error;
    FvdlInfo info = ModelConfig::loadModel(fvdlPath, &error);
    if (info.modelName.isEmpty()) return;

    // 同步选中状态
    if (m_selectedCard)
        m_selectedCard->setSelected(false);
    card->setSelected(true);
    m_selectedCard = card;

    m_currentModel   = info;
    m_currentModelId = modelId;
    updateLeftInfo();
    updateStepGuide();

    Logger::info(QString::fromUtf8("模型已打开(双击): %1").arg(modelId));
    emit modelOpened(modelId);
    emit openDatasetRequested(modelId);
}

// ── 刷新卡片画廊 ──────────────────────────────────────────────

void ModelManagement::refreshCards()
{
    for (ModelCard* card : m_cards)
        m_cardLayout->removeWidget(card);
    qDeleteAll(m_cards);
    m_cards.clear();
    m_selectedCard = nullptr;

    QJsonObject models = m_project.models;
    if (models.isEmpty()) {
        m_emptyHint->show();
        return;
    }

    m_emptyHint->hide();

    for (auto it = models.begin(); it != models.end(); ++it) {
        QString modelId = it.key();
        QJsonObject entry = it.value().toObject();

        QString type;
        QString fvdlPath = entry["model_file"].toString();
        if (!fvdlPath.isEmpty()) {
            FvdlInfo info = ModelConfig::loadModel(fvdlPath);
            type = info.modelType;
        }

        ModelCard* card = new ModelCard(modelId, type, m_cardContainer);
        card->refreshTheme();
        connect(card, &ModelCard::clicked, this, &ModelManagement::onCardClicked);
        connect(card, &ModelCard::doubleClicked, this, &ModelManagement::onCardDoubleClicked);
        m_cards.append(card);
    }

    relayoutCards();
}

void ModelManagement::relayoutCards()
{
    if (m_cards.isEmpty()) return;

    QScrollArea* sa = m_scrollArea;
    int availWidth = sa->viewport()->width()
                     - m_cardLayout->contentsMargins().left()
                     - m_cardLayout->contentsMargins().right();
    if (availWidth <= 0) availWidth = 600;

    const int cardWidth = 200;
    const int spacing = 16;
    int cols = qMax(1, (availWidth + spacing) / (cardWidth + spacing));

    for (int i = 0; i < m_cards.size(); ++i)
        m_cardLayout->addWidget(m_cards[i], i / cols, i % cols);
}

void ModelManagement::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayoutCards();
}

// ── 刷新左侧下栏模型详情 ─────────────────────────────────────

void ModelManagement::updateLeftInfo()
{
    if (m_currentModelId.isEmpty()) {
        m_detailLabel->setText(QString::fromUtf8(
            "未选定模型\n\n"
            "可点击左侧\"新建模型\"创建新模型，\n"
            "或点击\"打开模型\"选择已有模型。"));
        return;
    }

    const FvdlInfo& m = m_currentModel;

    // 读取 dataset_config.json
    QString labelInfo = QString::fromUtf8("无");
    QString instanceInfo = QString::fromUtf8("--");

    QString datasetConfigPath = QDir(m_datasetRoot)
        .absoluteFilePath(m.datasetRef + "/dataset_config.json");
    QFile cfgFile(datasetConfigPath);
    if (cfgFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray cfgData = cfgFile.readAll();
        cfgFile.close();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(cfgData, &err);
        if (err.error == QJsonParseError::NoError) {
            QJsonObject cfg = doc.object();
            QJsonArray categories = cfg.value("categories").toArray();
            if (categories.isEmpty()) {
                labelInfo = QString::fromUtf8("0 (空)");
            } else {
                QStringList names;
                for (const auto& cat : categories)
                    names << cat.toObject().value("name").toString();
                labelInfo = QString::fromUtf8("%1 (%2)")
                    .arg(categories.size()).arg(names.join(", "));
            }
            int imgCount = cfg.value("image_count").toInt(0);
            int instCount = cfg.value("instance_count").toInt(0);
            if (imgCount > 0 || instCount > 0)
                instanceInfo = QString::fromUtf8("%1 图，%2 实例")
                    .arg(imgCount).arg(instCount);
        }
    }

    QString info = QString::fromUtf8(
        "<b>名称</b>: %1<br>"
        "<b>类型</b>: %2<br>"
        "<br>"
        "<b>模型文件</b>:<br>%3<br>"
        "<br>"
        "<b>图像目录</b>:<br>%4/%5/images<br>"
        "<br>"
        "<b>标签类别</b>: %6<br>"
        "<b>实例个数</b>: %7<br>"
        "<br>"
        "<b>创建</b>: %8<br>"
        "<b>修改</b>: %9")
        .arg(m.modelName, m.modelType,
             m.fvdlPath,
             m_datasetRoot, m.datasetRef,
             labelInfo, instanceInfo,
             m.createdTime.toString("yyyy-MM-dd hh:mm"),
             m.modifiedTime.toString("yyyy-MM-dd hh:mm"));

    m_detailLabel->setText(info);
}

// ── 步骤引导条状态解锁 ─────────────────────────────────────

void ModelManagement::updateStepGuide()
{
    if (m_currentModelId.isEmpty()) {
        // 无选定模型：仅步骤 0 完成，当前在步骤 1
        m_stepGuideBar->setCurrentStep(1);
        m_stepGuideBar->setStepCompleted(0);
        return;
    }

    // 步骤 0 始终完成
    m_stepGuideBar->setStepCompleted(0);
    m_stepGuideBar->setCurrentStep(1);

    // 从项目配置中读取当前模型的业务状态
    QJsonObject models = m_project.models;
    if (!models.contains(m_currentModelId)) return;

    QJsonObject entry = models[m_currentModelId].toObject();
    QJsonObject status = entry.value("status").toObject();

    // 步骤 2（数据集管理）：data_imported
    if (status.value("data_imported").toBool(false))
        m_stepGuideBar->setStepCompleted(2);

    // 步骤 3（数据标注）：annotated
    if (status.value("annotated").toBool(false))
        m_stepGuideBar->setStepCompleted(3);

    // 步骤 4（数据拆分）：split_done
    if (status.value("split_done").toBool(false))
        m_stepGuideBar->setStepCompleted(4);

    // 步骤 5（模型训练）：trained
    if (status.value("trained").toBool(false))
        m_stepGuideBar->setStepCompleted(5);
}
