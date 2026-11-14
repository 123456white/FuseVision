#include "ModelManagement.h"
#include "ModelConfig.h"
#include "ProjectManagement.h"
#include "core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
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
    m_projectRoot  = project.projectRoot;
    m_currentModelId.clear();
    m_currentModel  = FvdlInfo();
    refreshCards();
    updateLeftInfo();

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

    m_deleteModelBtn = new QPushButton(QString::fromUtf8("  删除模型"));
    m_deleteModelBtn->setIcon(QIcon(":/res/Del.png"));
    m_deleteModelBtn->setIconSize(QSize(20, 20));
    m_deleteModelBtn->setCursor(Qt::PointingHandCursor);
    m_deleteModelBtn->setEnabled(false);
    m_deleteModelBtn->setObjectName("mmDeleteModelBtn");
    connect(m_deleteModelBtn, &QPushButton::clicked, this, &ModelManagement::onDeleteModel);
    topLayout->addWidget(m_deleteModelBtn);

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

    // ── 加载动画覆盖层 ──
    m_loadingOverlay = new LoadingOverlay(m_cardContainer,
        QString::fromUtf8("正在加载模型..."));
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

    // ── 删除模型按钮（危险操作，选中时红色，未选中时灰色不可用）──
    m_deleteModelBtn->setStyleSheet(
        QString("QPushButton#mmDeleteModelBtn {"
                "  background-color: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 8px;"
                "  padding: 10px 16px; font-size: 13px; font-weight: bold;"
                "  text-align: left; }"
                "QPushButton#mmDeleteModelBtn:hover {"
                "  background-color: %4; border-color: %5; }"
                "QPushButton#mmDeleteModelBtn:hover:disabled {"
                "  background-color: %1; border-color: %3; }")
            // enabled: red bg, white text
            .arg(QString("#e74c3c"), QString("white"), QString("#c0392b"),
                 QString("#c0392b"), QString("#a93226"))
        +
        QString("QPushButton#mmDeleteModelBtn:disabled {"
                "  background-color: transparent; color: %1; border-color: %2; }")
            .arg(p.textMuted, p.borderLight));

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
    m_deleteModelBtn->setIcon(QIcon(isDark ? ":/res/Del_D.png" : ":/res/Del.png"));

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
    dlg.setMinimumSize(480, 560);
    dlg.setStyleSheet(
        QString("QDialog { background-color: %1; } QLabel { color: %2; }")
            .arg(p.bgSecondary, p.textPrimary));

    QVBoxLayout* mainDlgLayout = new QVBoxLayout(&dlg);

    // ── 第一栏：任务类型选择（2×2 四宫格卡片）──
    QLabel* typeTitle = new QLabel(QString::fromUtf8("深度学习类型"));
    QFont boldFont = typeTitle->font();
    boldFont.setBold(true);
    typeTitle->setFont(boldFont);
    mainDlgLayout->addWidget(typeTitle);

    QButtonGroup* typeGroup = new QButtonGroup(&dlg);
    QStringList types = ModelTaskTypes::all();
    QList<QPushButton*> typeBtns;
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    const auto& iconSet = isDark ? s_typeIconResDark : s_typeIconRes;

    QGridLayout* typeGrid = new QGridLayout;
    typeGrid->setSpacing(12);

    for (int i = 0; i < types.size(); ++i) {
        QPushButton* btn = new QPushButton;
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName("mmTypeBtn");

        // 卡片内容：左图标 + 右(类型名 + 描述摘要)
        QHBoxLayout* btnLayout = new QHBoxLayout(btn);
        btnLayout->setContentsMargins(14, 12, 14, 12);
        btnLayout->setSpacing(12);

        QLabel* iconLabel = new QLabel;
        iconLabel->setAlignment(Qt::AlignCenter);
        QPixmap pix(iconSet[i]);
        iconLabel->setPixmap(pix.scaled(52, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setStyleSheet("background: transparent; border: none;");
        iconLabel->setFixedSize(60, 60);

        QVBoxLayout* textLayout = new QVBoxLayout;
        textLayout->setSpacing(4);

        QLabel* nameLabel = new QLabel(types[i]);
        nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        nameLabel->setStyleSheet(
            QString("background: transparent; border: none; font-size: 13px; font-weight: bold; color: %1;")
                .arg(p.textPrimary));

        // 截取描述的前两行作为摘要
        QString descFull = QString::fromUtf8(s_typeDescs[i]);
        QStringList descLines = descFull.split('\n');
        QString summary = descLines.size() >= 2
            ? descLines[0].trimmed() + "\n" + descLines[1].trimmed()
            : descFull.left(40);
        QLabel* descLabel = new QLabel(summary);
        descLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet(
            QString("background: transparent; border: none; font-size: 10px; color: %1;")
                .arg(p.textSecondary));

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(descLabel, 1);

        btnLayout->addWidget(iconLabel);
        btnLayout->addLayout(textLayout, 1);

        btn->setFixedSize(240, 100);
        btn->setStyleSheet(
            QString("QPushButton#mmTypeBtn {"
                    "  border: 2px solid %1; border-radius: 10px;"
                    "  background-color: %2; }"
                    "QPushButton#mmTypeBtn:checked {"
                    "  border: 2px solid %3; background-color: %4; }"
                    "QPushButton#mmTypeBtn:hover:!checked {"
                    "  background-color: %5; }")
                .arg(p.borderLight, p.bgSecondary,
                     p.accentPrimary, p.bgSelected,
                     p.bgHover));

        typeGroup->addButton(btn, i);
        typeGrid->addWidget(btn, i / 2, i % 2);
        typeBtns.append(btn);
    }
    typeBtns[0]->setChecked(true);

    mainDlgLayout->addLayout(typeGrid);

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
    QString defaultPath = QDir(m_projectRoot).absoluteFilePath("model" + QString(".fvdl"));
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
            m_projectRoot,
            QString::fromUtf8("FuseVision 模型文件 (*.fvdl)"));
        if (!path.isEmpty()) pathEdit->setText(path);
    });

    connect(nameEdit, &QLineEdit::textChanged, this, [nameEdit, pathEdit, this](const QString& text) {
        QString name = text.trimmed();
        if (!name.isEmpty()) {
            QString newPath = QDir(m_projectRoot).absoluteFilePath(name + ".fvdl");
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
    if (!ModelConfig::createModel(name, type, m_projectRoot,
                                    m_project.fvprojPath, &error)) {
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
        m_projectRoot,
        QString::fromUtf8("FuseVision 模型文件 (*.fvdl)"));

    if (fvdlPath.isEmpty()) return;

    m_loadingOverlay->showOverlay();

    QString error;
    FvdlInfo info = ModelConfig::loadModel(fvdlPath, &error);
    if (info.modelName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("打开失败"), error);
        m_loadingOverlay->hideOverlay();
        return;
    }

    // 模型来自其他项目 → 询问是否导入到当前项目
    if (info.projectRef != m_project.projectName) {
        QString msg = QString::fromUtf8(
            "模型 \"%1\" 属于项目 \"%2\"，不属于当前项目 \"%3\"。\n\n"
            "是否将此模型导入到当前项目？\n"
            "（模型数据将复制到当前项目目录，原数据保留）")
            .arg(info.modelName, info.projectRef, m_project.projectName);
        QMessageBox::StandardButton btn = QMessageBox::question(
            this, QString::fromUtf8("导入模型"), msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (btn != QMessageBox::Yes) {
            m_loadingOverlay->hideOverlay();
            return;
        }
        // 更新项目归属
        info.projectRef = m_project.projectName;
    }

    // 检查模型是否在当前项目目录下，若不在则复制过来
    QDir srcModelDir = QFileInfo(fvdlPath).absoluteDir();
    QString targetModelDir = QDir(m_projectRoot).absoluteFilePath(info.modelName);
    bool needImport = (QDir::cleanPath(srcModelDir.absolutePath())
                       != QDir::cleanPath(targetModelDir));

    if (needImport) {
        if (QDir(targetModelDir).exists()) {
            // 目标目录已存在同名模型，直接使用
            info.fvdlPath = QDir(targetModelDir).absoluteFilePath(
                QFileInfo(fvdlPath).fileName());
        } else {
            bool copied = ProjectManagement::copyDirectory(
                srcModelDir.absolutePath(), targetModelDir);
            if (!copied) {
                QMessageBox::warning(this, QString::fromUtf8("导入失败"),
                    QString::fromUtf8("无法将模型复制到：%1").arg(targetModelDir));
                m_loadingOverlay->hideOverlay();
                return;
            }
            info.fvdlPath = QDir(targetModelDir).absoluteFilePath(
                QFileInfo(fvdlPath).fileName());
            Logger::info(QString::fromUtf8("模型已复制: %1 -> %2")
                .arg(srcModelDir.absolutePath(), targetModelDir));
        }

        // 更新 .fvdl 中的项目归属和路径
        info.datasetRef = info.modelName;  // 确保数据集引用正确
        ModelConfig::saveModel(info, &error);

        // 在 .fvproj 中注册此模型（若尚未注册）
        FvprojInfo proj = ProjectConfig::loadProject(m_project.fvprojPath, &error);
        QJsonObject models = proj.models;
        if (!models.contains(info.modelName)) {
            QJsonObject modelEntry;
            modelEntry["model_file"]    = info.fvdlPath;
            modelEntry["dataset_folder"] = info.modelName;
            QJsonObject status;
            status["data_imported"] = false;
            status["annotated"]     = false;
            status["split_done"]    = false;
            status["trained"]       = false;
            modelEntry["status"] = status;
            models[info.modelName] = modelEntry;
            proj.models = models;
            ProjectConfig::saveProject(proj, &error);
        } else {
            // 已存在则更新路径
            QJsonObject modelObj = models[info.modelName].toObject();
            modelObj["model_file"] = info.fvdlPath;
            models[info.modelName] = modelObj;
            proj.models = models;
            ProjectConfig::saveProject(proj, &error);
        }
        m_project = proj;
    }

    m_currentModel   = info;
    m_currentModelId = info.modelName;

    refreshCards();
    updateLeftInfo();
    m_deleteModelBtn->setEnabled(true);

    // 选中对应的卡片
    for (ModelCard* card : m_cards) {
        if (card->modelId() == info.modelName) {
            card->setSelected(true);
            m_selectedCard = card;
            break;
        }
    }

    m_loadingOverlay->hideOverlay();

    Logger::info(QString::fromUtf8("模型已打开: %1 (%2)").arg(info.modelName, info.modelType));
    emit modelOpened(info.modelName);
}

// ── 卡片单击 ──────────────────────────────────────────────────

void ModelManagement::onDeleteModel()
{
    if (m_currentModelId.isEmpty()) {
        QMessageBox::information(this,
            QString::fromUtf8("提示"),
            QString::fromUtf8("请先选中一个模型卡片，再进行删除操作。"));
        return;
    }

    QString modelName = m_currentModel.modelName;
    QString modelRoot = m_projectRoot + "/" + modelName;
    QString fvdlPath  = m_currentModel.fvdlPath;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString::fromUtf8("确认删除"));
    msgBox.setText(QString::fromUtf8("确定要删除模型「%1」吗？").arg(modelName));
    msgBox.setInformativeText(QString::fromUtf8(
        "● 删除整个模型：移除模型目录及所有数据\n"
        "● 仅删除模型文件：只移除 .fvdl，保留数据"));
    msgBox.setIcon(QMessageBox::Warning);

    QPushButton* btnEntire = msgBox.addButton(QString::fromUtf8("删除整个模型"), QMessageBox::DestructiveRole);
    QPushButton* btnFvdl   = msgBox.addButton(QString::fromUtf8("仅删除模型文件"), QMessageBox::AcceptRole);
    QPushButton* btnCancel = msgBox.addButton(QString::fromUtf8("取消"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(btnCancel);
    msgBox.exec();

    bool success = false;

    if (msgBox.clickedButton() == btnEntire) {
        QDir dir(modelRoot);
        success = dir.removeRecursively();
    } else if (msgBox.clickedButton() == btnFvdl) {
        QFile fvdlFile(fvdlPath);
        success = fvdlFile.remove();
    } else {
        return;
    }

    if (!success) {
        QMessageBox::warning(this,
            QString::fromUtf8("删除失败"),
            QString::fromUtf8("操作失败，请检查文件权限。"));
        return;
    }

    // 从 fvproj 中移除模型条目
    QString error;
    FvprojInfo proj = ProjectConfig::loadProject(m_project.fvprojPath, &error);
    QJsonObject models = proj.models;
    models.remove(modelName);
    proj.models = models;

    if (!ProjectConfig::saveProject(proj, &error)) {
        Logger::warn(QString::fromUtf8("更新项目文件失败: %1").arg(error));
    }

    // 清除选中状态
    m_currentModelId.clear();
    m_currentModel = FvdlInfo();
    m_selectedCard = nullptr;
    m_deleteModelBtn->setEnabled(false);

    // 刷新
    m_project = proj;
    refreshCards();
    updateLeftInfo();

    QMessageBox::information(this,
        QString::fromUtf8("删除成功"),
        QString::fromUtf8("模型「%1」已删除。").arg(modelName));

    Logger::info(QString::fromUtf8("模型已删除: %1").arg(modelName));
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
    m_deleteModelBtn->setEnabled(true);
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
    m_deleteModelBtn->setEnabled(true);

    Logger::info(QString::fromUtf8("模型已打开(双击): %1").arg(modelId));
    emit modelOpened(modelId);
    emit openDatasetRequested(modelId);
}

// ── 刷新卡片画廊 ──────────────────────────────────────────────

void ModelManagement::refreshCards()
{
    m_loadingOverlay->showOverlay();

    for (ModelCard* card : m_cards)
        m_cardLayout->removeWidget(card);
    qDeleteAll(m_cards);
    m_cards.clear();
    m_selectedCard = nullptr;

    QJsonObject models = m_project.models;
    if (models.isEmpty()) {
        m_emptyHint->show();
        m_loadingOverlay->hideOverlay();
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
    m_loadingOverlay->hideOverlay();
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
        m_deleteModelBtn->setEnabled(false);
        return;
    }

    const FvdlInfo& m = m_currentModel;

    // 读取 dataset_config.json
    QString labelInfo = QString::fromUtf8("无");
    QString instanceInfo = QString::fromUtf8("--");

    QString datasetConfigPath = QDir(m_projectRoot)
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
             m_projectRoot, m.datasetRef,
             labelInfo, instanceInfo,
             m.createdTime.toString("yyyy-MM-dd hh:mm"),
             m.modifiedTime.toString("yyyy-MM-dd hh:mm"));

    m_detailLabel->setText(info);
}
