#include "ProjectManagement.h"
#include "core/Logger.h"
#include "core/SettingsManager.h"
#include "core/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QScrollArea>
#include <QFrame>
#include <QIcon>
#include <QFont>
#include <QDir>
#include <QApplication>
#include <QStyle>
#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QResizeEvent>

// =============================================================================
// 步骤引导条常量
// =============================================================================
static const char* s_stepNames[] = {
    "项目管理", "模型管理", "数据集管理",
    "数据标注", "数据拆分", "模型训练"
};
static const int s_stepCount = 6;

static const char* s_logicDescText =
    "单项目管多模型，单模型管单数据集，单数据集管单数据标注，"
    "单数据集管单数据拆分，单数据集管多版本模型训练，"
    "单数据集管多版本模型预测，单数据集管多版本模型导出，"
    "模型训练对应模型预测与导出，界面刷新或操作执行前，须校验。";

// =============================================================================
// StepGuideBar 实现
// =============================================================================

StepGuideBar::StepGuideBar(QWidget* parent)
    : QWidget(parent)
{
    m_completed = QList<bool>(s_stepCount, false);
    buildSteps();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &StepGuideBar::refreshTheme);
}

void StepGuideBar::buildSteps()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(0);

    // 先添加所有步骤，让它们均匀分布
    for (int i = 0; i < s_stepCount; ++i) {
        // 每个步骤用 QWidget 包裹，以便设置 stretch
        QWidget* stepWidget = new QWidget;
        QVBoxLayout* stepLayout = new QVBoxLayout(stepWidget);
        stepLayout->setContentsMargins(0, 0, 0, 0);
        stepLayout->setSpacing(4);

        QLabel* dot = new QLabel;
        dot->setFixedSize(18, 18);
        dot->setAlignment(Qt::AlignCenter);
        dot->setText(QString::number(i + 1));

        QLabel* label = new QLabel(QString::fromUtf8(s_stepNames[i]));
        label->setAlignment(Qt::AlignCenter);

        stepLayout->addWidget(dot, 0, Qt::AlignHCenter);
        stepLayout->addWidget(label, 0, Qt::AlignHCenter);

        // 每个步骤等宽分布
        layout->addWidget(stepWidget, 1);

        // 连接线
        QFrame* line = nullptr;
        if (i < s_stepCount - 1) {
            line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            line->setFixedHeight(2);
            line->setMinimumWidth(24);
            layout->addWidget(line, 0, Qt::AlignVCenter);
        }
        m_steps.append({dot, label, line});
    }

    refreshTheme();
}

void StepGuideBar::setCurrentStep(int step)
{
    if (step < 0 || step >= s_stepCount) return;
    m_currentStep = step;
    applyStepStyles();
}

void StepGuideBar::setStepCompleted(int step)
{
    if (step < 0 || step >= s_stepCount) return;
    m_completed[step] = true;
    applyStepStyles();
}

void StepGuideBar::refreshTheme()
{
    const auto& p = ThemeManager::instance().palette();

    // 容器背景 + 顶部分割线
    setStyleSheet(QString("QWidget { background-color: %1; border-top: 1px solid %2; }")
                      .arg(p.bgSecondary, p.borderLight));

    // 连接线颜色
    for (auto& s : m_steps) {
        if (s.line)
            s.line->setStyleSheet(QString("QFrame { color: %1; }").arg(p.borderLight));
    }

    applyStepStyles();
}

void StepGuideBar::applyStepStyles()
{
    const auto& p = ThemeManager::instance().palette();

    for (int i = 0; i < m_steps.size(); ++i) {
        auto& s = m_steps[i];
        if (m_completed[i]) {
            s.dot->setText(QString::fromUtf8("\u2713"));
            s.dot->setStyleSheet(
                QString("QLabel { background-color: %1; border-radius: 9px; "
                        "color: white; font-size: 10px; font-weight: bold; }")
                    .arg(p.success));
            s.label->setStyleSheet(
                QString("QLabel { color: %1; font-size: 11px; font-weight: bold; }")
                    .arg(p.success));
        } else if (i == m_currentStep) {
            s.dot->setText(QString::number(i + 1));
            s.dot->setStyleSheet(
                QString("QLabel { background-color: %1; border-radius: 9px; "
                        "color: white; font-size: 10px; font-weight: bold; }")
                    .arg(p.accentPrimary));
            s.label->setStyleSheet(
                QString("QLabel { color: %1; font-size: 11px; font-weight: bold; }")
                    .arg(p.accentPrimary));
        } else {
            s.dot->setText(QString::number(i + 1));
            s.dot->setStyleSheet(
                QString("QLabel { background-color: %1; border-radius: 9px; "
                        "color: white; font-size: 10px; font-weight: bold; }")
                    .arg(p.textMuted));
            s.label->setStyleSheet(
                QString("QLabel { color: %1; font-size: 11px; }")
                    .arg(p.textMuted));
        }
    }
}

// =============================================================================
// ProjectCard 实现
// =============================================================================

ProjectCard::ProjectCard(const ProjectCardInfo& info, QWidget* parent)
    : QFrame(parent), m_info(info)
{
    setFixedSize(200, 210);
    setCursor(Qt::PointingHandCursor);
    setObjectName("projectCard");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // 项目图标
    m_imageLabel = new QLabel;
    m_imageLabel->setFixedSize(180, 160);
    m_imageLabel->setAlignment(Qt::AlignCenter);

    QPixmap pix(":/res/FuseVisionProject.png");
    if (!pix.isNull())
        m_imageLabel->setPixmap(pix.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        m_imageLabel->setText(QString::fromUtf8("\xF0\x9F\x93\x81"));

    layout->addWidget(m_imageLabel);

    // 项目名称
    m_nameLabel = new QLabel(m_info.projectName);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setMaximumHeight(36);
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(10);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    layout->addWidget(m_nameLabel);

    // 初始主题
    refreshTheme();
}

void ProjectCard::refreshTheme()
{
    const auto& p = ThemeManager::instance().palette();
    m_imageLabel->setStyleSheet(
        QString("QLabel { background-color: %1; border-radius: 8px; color: %2; }")
            .arg(p.bgTertiary, p.textSecondary));
    m_nameLabel->setStyleSheet(QString("QLabel { color: %1; }").arg(p.textPrimary));

    // 暗色主题下使用暗色项目图标
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    QPixmap pix(isDark ? ":/res/FuseVisionProject_D.png" : ":/res/FuseVisionProject.png");
    if (!pix.isNull())
        m_imageLabel->setPixmap(pix.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    applyCardStyle();
}

void ProjectCard::applyCardStyle()
{
    const auto& p = ThemeManager::instance().palette();

    if (m_selected) {
        setStyleSheet(
            QString("QFrame#projectCard { border: 2px solid %1; border-radius: 10px; "
                    "background-color: %2; }")
                .arg(p.accentPrimary, p.bgSelected));
    } else if (m_hovered) {
        setStyleSheet(
            QString("QFrame#projectCard { border: 1px solid %1; border-radius: 10px; "
                    "background-color: %2; }")
                .arg(p.accentPrimary, p.bgHover));
    } else {
        setStyleSheet(
            QString("QFrame#projectCard { border: 1px solid %1; border-radius: 10px; "
                    "background-color: %2; }")
                .arg(p.borderLight, p.bgSecondary));
    }
}

void ProjectCard::setSelected(bool selected)
{
    m_selected = selected;
    applyCardStyle();
}

void ProjectCard::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    emit clicked(this);
}

void ProjectCard::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    emit doubleClicked(this);
}

void ProjectCard::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    applyCardStyle();
    QFrame::enterEvent(event);
}

void ProjectCard::leaveEvent(QEvent* event)
{
    m_hovered = false;
    applyCardStyle();
    QFrame::leaveEvent(event);
}

// =============================================================================
// ProjectManagement 实现
// =============================================================================

ProjectManagement::ProjectManagement(QWidget* parent)
    : QWidget(parent)
{
    auto& sm = SettingsManager::instance();
    m_dlDataPath    = sm.dlDataPath();
    m_dlDatasetPath = sm.dlDatasetPath();
    m_dlModelPath   = sm.dlModelPath();

    if (m_dlDataPath.isEmpty())
        m_dlDataPath = QDir::currentPath() + "/dl_data";
    if (m_dlDatasetPath.isEmpty())
        m_dlDatasetPath = QDir::currentPath() + "/dl_datasets";
    if (m_dlModelPath.isEmpty())
        m_dlModelPath = QDir::currentPath() + "/dl_models";

    initUI();
    applyTheme();
    refreshCards();

    // 主题变更 → 刷新所有样式
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ProjectManagement::applyTheme);

    Logger::info(QString::fromUtf8("项目管理模块已初始化 (数据根: %1)").arg(m_dlDataPath));
}

// ── UI 构建 ───────────────────────────────────────────────────

void ProjectManagement::initUI()
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
    m_mainSplitter->setStretchFactor(0, 1);  // 左侧 1
    m_mainSplitter->setStretchFactor(1, 5);  // 右侧 5
    m_mainSplitter->setSizes({ 200, 1000 }); // 初始尺寸

    mainLayout->addWidget(m_mainSplitter, 1);
}

// ── 左侧边栏 ──────────────────────────────────────────────────

void ProjectManagement::initLeftPanel()
{
    m_leftSplitter = new QSplitter(Qt::Vertical);
    m_leftSplitter->setHandleWidth(1);
    m_leftSplitter->setChildrenCollapsible(false);

    // ===== 上栏：新建/打开项目按钮 =====
    QWidget* topPanel = new QWidget;
    topPanel->setObjectName("pmLeftTop");
    QVBoxLayout* topLayout = new QVBoxLayout(topPanel);
    topLayout->setContentsMargins(10, 12, 10, 8);
    topLayout->setSpacing(8);

    m_newProjectBtn = new QPushButton(QString::fromUtf8("  新建项目"));
    m_newProjectBtn->setIcon(QIcon(":/res/NewProject.png"));
    m_newProjectBtn->setIconSize(QSize(20, 20));
    m_newProjectBtn->setCursor(Qt::PointingHandCursor);
    m_newProjectBtn->setObjectName("pmNewProjectBtn");
    connect(m_newProjectBtn, &QPushButton::clicked, this, &ProjectManagement::onNewProject);

    m_openProjectBtn = new QPushButton(QString::fromUtf8("  打开项目"));
    m_openProjectBtn->setIcon(QIcon(":/res/OpenProject.png"));
    m_openProjectBtn->setIconSize(QSize(20, 20));
    m_openProjectBtn->setCursor(Qt::PointingHandCursor);
    m_openProjectBtn->setObjectName("pmOpenProjectBtn");
    connect(m_openProjectBtn, &QPushButton::clicked, this, &ProjectManagement::onOpenProject);

    topLayout->addWidget(m_newProjectBtn);
    topLayout->addWidget(m_openProjectBtn);
    topLayout->addStretch();

    // ===== 下栏：模型列表 + 项目信息 + 逻辑说明 =====
    QWidget* bottomPanel = new QWidget;
    bottomPanel->setObjectName("pmLeftBottom");
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(10, 8, 10, 12);
    bottomLayout->setSpacing(6);

    m_modelListTitle = new QLabel(QString::fromUtf8("模型列表"));
    QFont titleFont = m_modelListTitle->font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    m_modelListTitle->setFont(titleFont);
    bottomLayout->addWidget(m_modelListTitle);

    m_modelList = new QListWidget;
    m_modelList->setMaximumHeight(150);
    m_modelList->setObjectName("pmModelList");
    connect(m_modelList, &QListWidget::itemClicked,
            this, &ProjectManagement::onModelSelected);
    bottomLayout->addWidget(m_modelList);

    m_projectTimeLabel = new QLabel;
    m_projectTimeLabel->setWordWrap(true);
    m_projectTimeLabel->setObjectName("pmProjectTime");
    bottomLayout->addWidget(m_projectTimeLabel);

    // 项目描述（打开项目后显示）
    m_projectDescLabel = new QLabel;
    m_projectDescLabel->setWordWrap(true);
    m_projectDescLabel->setObjectName("pmProjectDesc");
    bottomLayout->addWidget(m_projectDescLabel);

    QFrame* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("pmSep");
    bottomLayout->addWidget(sep);

    // 弹簧将逻辑说明推到底部
    bottomLayout->addStretch();

    m_logicDescLabel = new QLabel(QString::fromUtf8(s_logicDescText));
    m_logicDescLabel->setWordWrap(true);
    m_logicDescLabel->setObjectName("pmLogicDesc");
    bottomLayout->addWidget(m_logicDescLabel);

    m_leftSplitter->addWidget(topPanel);
    m_leftSplitter->addWidget(bottomPanel);
    m_leftSplitter->setSizes({ 90, 810 });

    updateLeftPanel();
}

// ── 右侧主体 ──────────────────────────────────────────────────

void ProjectManagement::initRightPanel()
{
    m_rightContainer = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_rightContainer);
    layout->setContentsMargins(16, 16, 16, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("pmScrollArea");

    m_cardContainer = new QWidget;
    m_cardContainer->setObjectName("pmCardContainer");
    m_cardLayout = new QGridLayout(m_cardContainer);
    m_cardLayout->setContentsMargins(8, 8, 8, 8);
    m_cardLayout->setSpacing(16);
    m_cardLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_emptyHint = new QLabel(QString::fromUtf8("暂无项目，请点击左上角\"新建项目\"创建第一个项目"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setObjectName("pmEmptyHint");

    QWidget* stackWidget = new QWidget;
    QVBoxLayout* stackLayout = new QVBoxLayout(stackWidget);
    stackLayout->setContentsMargins(0, 0, 0, 0);
    stackLayout->addWidget(m_emptyHint);
    stackLayout->addWidget(m_cardContainer);

    m_scrollArea->setWidget(stackWidget);
    layout->addWidget(m_scrollArea, 1);

    // 步骤引导条（直接占满宽度）
    m_stepGuideBar = new StepGuideBar;
    layout->addWidget(m_stepGuideBar);
}

// ── 主题刷新 ──────────────────────────────────────────────────

void ProjectManagement::applyTheme()
{
    const auto& p = ThemeManager::instance().palette();

    // ── 左侧上栏背景 ──
    if (auto* w = m_leftSplitter->widget(0))  // topPanel
        w->setStyleSheet(
            QString("#pmLeftTop { background-color: %1; }").arg(p.bgSecondary));

    // ── 左侧下栏背景 ──
    if (auto* w = m_leftSplitter->widget(1))  // bottomPanel
        w->setStyleSheet(
            QString("#pmLeftBottom { background-color: %1; }").arg(p.bgSecondary));

    // ── 新建项目按钮（主按钮）──
    m_newProjectBtn->setStyleSheet(
        QString("QPushButton#pmNewProjectBtn {"
                "  background-color: %1; color: #ffffff; border: none;"
                "  border-radius: 8px; padding: 10px 16px; font-size: 13px;"
                "  font-weight: bold; text-align: left; }"
                "QPushButton#pmNewProjectBtn:hover { background-color: %2; }")
            .arg(p.accentPrimary, p.accentHover));

    // ── 打开项目按钮（描边按钮）──
    m_openProjectBtn->setStyleSheet(
        QString("QPushButton#pmOpenProjectBtn {"
                "  background-color: transparent; color: %1;"
                "  border: 1px solid %2; border-radius: 8px;"
                "  padding: 10px 16px; font-size: 13px; font-weight: bold;"
                "  text-align: left; }"
                "QPushButton#pmOpenProjectBtn:hover { background-color: %3; }")
            .arg(p.accentPrimary, p.accentPrimary, p.bgHover));

    // ── 模型列表标题 ──
    m_modelListTitle->setStyleSheet(
        QString("QLabel { color: %1; }").arg(p.textPrimary));

    // ── 模型列表 ──
    m_modelList->setStyleSheet(
        QString("QListWidget#pmModelList { border: 1px solid %1; border-radius: 4px;"
                " background-color: %2; color: %3; }"
                "QListWidget::item { padding: 6px 8px; }"
                "QListWidget::item:selected { background-color: %4; color: %5; }"
                "QListWidget::item:hover { background-color: %6; }")
            .arg(p.borderLight, p.bgSecondary, p.textPrimary,
                 p.bgSelected, p.accentPrimary, p.bgHover));

    // ── 项目时间 ──
    m_projectTimeLabel->setStyleSheet(
        QString("QLabel#pmProjectTime { color: %1; font-size: 11px; }")
            .arg(p.textSecondary));

    // ── 项目描述 ──
    m_projectDescLabel->setStyleSheet(
        QString("QLabel#pmProjectDesc { color: %1; font-size: 11px;"
                " padding: 6px 8px; background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary, p.bgTertiary));

    // ── 分隔线 ──
    if (auto* sep = findChild<QFrame*>("pmSep"))
        sep->setStyleSheet(QString("QFrame { color: %1; }").arg(p.borderLight));

    // ── 逻辑说明 ──
    m_logicDescLabel->setStyleSheet(
        QString("QLabel#pmLogicDesc { color: %1; font-size: 10px; line-height: 1.6;"
                " padding: 4px; background-color: %2; border-radius: 4px; }")
            .arg(p.textMuted, p.bgTertiary));

    // ── 滚动区域 ──
    m_scrollArea->setStyleSheet(
        QString("QScrollArea#pmScrollArea { border: none; background-color: %1; }")
            .arg(p.bgPrimary));
    m_cardContainer->setStyleSheet(
        QString("#pmCardContainer { background-color: %1; }").arg(p.bgPrimary));

    // ── 空状态提示 ──
    m_emptyHint->setStyleSheet(
        QString("QLabel#pmEmptyHint { color: %1; font-size: 15px; }")
            .arg(p.textMuted));

    // ── 右侧容器背景 ──
    m_rightContainer->setStyleSheet(
        QString("QWidget { background-color: %1; }").arg(p.bgPrimary));

    // ── 按钮图标暗色适配 ──
    bool isDark = (ThemeManager::instance().currentTheme() == ThemeManager::Dark);
    m_newProjectBtn->setIcon(QIcon(isDark ? ":/res/NewProject_D.png" : ":/res/NewProject.png"));
    m_openProjectBtn->setIcon(QIcon(isDark ? ":/res/OpenProject_D.png" : ":/res/OpenProject.png"));

    // ── 所有卡片重绘 ──
    for (ProjectCard* card : m_cards)
        card->refreshTheme();
}

// ── 刷新卡片 ──────────────────────────────────────────────────

void ProjectManagement::refreshCards()
{
    for (ProjectCard* card : m_cards) {
        m_cardLayout->removeWidget(card);
        delete card;
    }
    m_cards.clear();
    m_selectedCard = nullptr;

    QList<ProjectCardInfo> projects = ProjectConfig::scanProjects(m_dlDataPath);

    if (projects.isEmpty()) {
        m_emptyHint->setVisible(true);
        m_cardContainer->setVisible(false);
    } else {
        m_emptyHint->setVisible(false);
        m_cardContainer->setVisible(true);

        for (const ProjectCardInfo& proj : projects) {
            ProjectCard* card = new ProjectCard(proj);
            connect(card, &ProjectCard::clicked,
                    this, &ProjectManagement::onCardClicked);
            connect(card, &ProjectCard::doubleClicked,
                    this, &ProjectManagement::onCardDoubleClicked);
            m_cards.append(card);
        }
        relayoutCards();
    }

    updateStepGuide();
}

// ── 新建项目 ──────────────────────────────────────────────────

void ProjectManagement::onNewProject()
{
    // 自定义对话框：项目名称 + 描述（明暗主题适配）
    const auto& p = ThemeManager::instance().palette();

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("新建项目"));
    dlg.setMinimumSize(420, 320);
    dlg.setStyleSheet(
        QString("QDialog { background-color: %1; }"
                "QLabel { color: %2; }")
            .arg(p.bgSecondary, p.textPrimary));

    QFormLayout* form = new QFormLayout(&dlg);
    form->setSpacing(12);

    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QString::fromUtf8("仅允许英文字母、数字和下划线"));
    nameEdit->setMaxLength(64);
    nameEdit->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2;"
                " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.accentPrimary));
    form->addRow(QString::fromUtf8("项目名称 *"), nameEdit);

    QTextEdit* descEdit = new QTextEdit;
    descEdit->setPlaceholderText(QString::fromUtf8("可选，简要描述项目用途"));
    descEdit->setMaximumHeight(80);
    descEdit->setStyleSheet(
        QString("QTextEdit { background-color: %1; color: %2;"
                " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }"
                "QTextEdit:focus { border-color: %4; }")
            .arg(p.surfaceWhite, p.textPrimary, p.borderMedium, p.accentPrimary));
    form->addRow(QString::fromUtf8("项目描述"), descEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("创建失败"),
                             QString::fromUtf8("项目名称不能为空。"));
        return;
    }

    QString desc = descEdit->toPlainText().trimmed();

    QString error;
    if (!ProjectConfig::createProject(name, desc, m_dlDataPath, m_dlDatasetPath, m_dlModelPath, &error)) {
        QMessageBox::warning(this, QString::fromUtf8("创建失败"), error);
        return;
    }

    QMessageBox::information(this, QString::fromUtf8("创建成功"),
        QString::fromUtf8("项目 \"%1\" 已创建。\n\n数据目录: %2/%1\n数据集目录: %3/%1\n模型目录: %4/%1")
            .arg(name, m_dlDataPath, m_dlDatasetPath, m_dlModelPath));

    refreshCards();
}

// ── 打开项目 ──────────────────────────────────────────────────

void ProjectManagement::onOpenProject()
{
    QString fvprojPath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("打开项目"),
        m_dlDataPath,
        QString::fromUtf8("FuseVision 项目文件 (project.fvproj)"));

    if (fvprojPath.isEmpty()) return;

    QString error;
    FvprojInfo info = ProjectConfig::loadProject(fvprojPath, &error);
    if (info.projectName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("打开失败"), error);
        return;
    }

    // 校验三个根路径
    QStringList missing;
    if (!QDir(info.dataRoot).exists())    missing << QString::fromUtf8("数据目录: ") + info.dataRoot;
    if (!QDir(info.datasetRoot).exists()) missing << QString::fromUtf8("数据集目录: ") + info.datasetRoot;
    if (!QDir(info.modelRoot).exists())   missing << QString::fromUtf8("模型目录: ") + info.modelRoot;

    if (!missing.isEmpty()) {
        QString msg = QString::fromUtf8("以下目录不存在：\n\n%1\n\n是否仍要打开此项目？\n"
                                         "您可以稍后在系统设置中修复路径。")
                          .arg(missing.join("\n"));
        QMessageBox::StandardButton btn = QMessageBox::question(
            this, QString::fromUtf8("路径缺失"), msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
    }

    m_currentProject     = info;
    m_currentProjectName = info.projectName;
    m_currentProjectPath = fvprojPath;
    m_hasProject         = true;

    updateLeftPanel();
    updateStepGuide();

    Logger::info(QString::fromUtf8("项目已打开: %1").arg(info.projectName));
    emit projectOpened(info.projectName, fvprojPath);
}

// ── 卡片交互 ──────────────────────────────────────────────────

void ProjectManagement::onCardClicked(ProjectCard* card)
{
    clearSelection();
    m_selectedCard = card;
    card->setSelected(true);

    // 单击即加载该项目元信息到左侧面板（不含"打开"语义，不发射信号、不切 Tab）
    QString error;
    FvprojInfo info = ProjectConfig::loadProject(card->fvprojPath(), &error);
    if (info.projectName.isEmpty()) return;

    // 注入到临时预览状态：仅刷新左侧信息，不标记 m_hasProject
    m_previewProject = info;

    m_projectTimeLabel->setText(
        QString::fromUtf8("创建: %1\n修改: %2")
            .arg(info.createdTime.toString("yyyy-MM-dd hh:mm"))
            .arg(info.modifiedTime.toString("yyyy-MM-dd hh:mm")));
    m_projectTimeLabel->setVisible(true);

    if (!info.description.isEmpty()) {
        m_projectDescLabel->setText(info.description);
        m_projectDescLabel->setVisible(true);
    } else {
        m_projectDescLabel->setVisible(false);
    }

    m_modelListTitle->setText(
        QString::fromUtf8("模型列表 (%1)").arg(info.models.size()));
    m_modelList->setVisible(true);
    m_modelList->clear();
    QJsonObject models = info.models;
    for (auto it = models.begin(); it != models.end(); ++it)
        m_modelList->addItem(it.key());

    m_stepGuideBar->setCurrentStep(0);  // 仅在预览，停留在步骤 0

    emit projectPreviewed(info.projectName);
}

void ProjectManagement::onCardDoubleClicked(ProjectCard* card)
{
    m_selectedCard = card;
    card->setSelected(true);

    QString fvproj = card->fvprojPath();
    QString error;
    FvprojInfo info = ProjectConfig::loadProject(fvproj, &error);
    if (info.projectName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("打开失败"), error);
        return;
    }

    m_currentProject     = info;
    m_currentProjectName = info.projectName;
    m_currentProjectPath = fvproj;
    m_hasProject         = true;

    updateLeftPanel();
    updateStepGuide();

    Logger::info(QString::fromUtf8("项目已打开(双击): %1").arg(info.projectName));
    emit projectOpened(info.projectName, fvproj);
}

void ProjectManagement::clearSelection()
{
    for (ProjectCard* c : m_cards)
        c->setSelected(false);
    m_selectedCard = nullptr;
}

// ── 模型列表 ──────────────────────────────────────────────────

void ProjectManagement::onModelSelected(QListWidgetItem* item)
{
    if (!item) return;
    Logger::debug(QString::fromUtf8("模型切换: %1").arg(item->text()));
}

void ProjectManagement::refreshModelList()
{
    m_modelList->clear();
    if (!m_hasProject) return;

    QJsonObject models = m_currentProject.models;
    for (auto it = models.begin(); it != models.end(); ++it) {
        m_modelList->addItem(it.key());
    }
}

// ── 左侧面板状态 ──────────────────────────────────────────────

void ProjectManagement::updateLeftPanel()
{
    if (m_hasProject) {
        m_projectTimeLabel->setText(
            QString::fromUtf8("创建: %1\n修改: %2")
                .arg(m_currentProject.createdTime.toString("yyyy-MM-dd hh:mm"))
                .arg(m_currentProject.modifiedTime.toString("yyyy-MM-dd hh:mm")));
        m_projectTimeLabel->setVisible(true);

        // 项目描述
        if (!m_currentProject.description.isEmpty()) {
            m_projectDescLabel->setText(m_currentProject.description);
            m_projectDescLabel->setVisible(true);
        } else {
            m_projectDescLabel->setVisible(false);
        }

        m_modelListTitle->setText(
            QString::fromUtf8("模型列表 (%1)").arg(m_currentProject.models.size()));
        m_modelList->setVisible(true);
        refreshModelList();
    } else {
        m_projectTimeLabel->setVisible(false);
        m_projectDescLabel->setVisible(false);
        m_modelListTitle->setText(QString::fromUtf8("模型列表"));
        m_modelList->clear();
        m_modelList->setVisible(false);
    }
}

// ── 步骤引导条 ────────────────────────────────────────────────

void ProjectManagement::updateStepGuide()
{
    if (m_hasProject) {
        m_stepGuideBar->setCurrentStep(1);
        m_stepGuideBar->setStepCompleted(0);
    } else {
        m_stepGuideBar->setCurrentStep(0);
    }
}

// ── 窗口大小变化 → 卡片网格重排 ──────────────────────────────

void ProjectManagement::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_cards.isEmpty())
        relayoutCards();
}

void ProjectManagement::relayoutCards()
{
    // 从 grid 临时移除全部卡片（不 delete）
    for (ProjectCard* card : m_cards)
        m_cardLayout->removeWidget(card);

    // 当前可用宽度（viewport 宽去掉 padding）
    int availWidth = m_scrollArea->viewport()->width()
                     - m_cardLayout->contentsMargins().left()
                     - m_cardLayout->contentsMargins().right();
    const int cols = qMax(1, (availWidth + m_cardLayout->spacing())
                            / (200 + m_cardLayout->spacing()));

    // 按新列数放回
    for (int i = 0; i < m_cards.size(); ++i)
        m_cardLayout->addWidget(m_cards[i], i / cols, i % cols);
}
