#include "core/RecoveryStore.h"
#include "ui/WriteAccess.h"
#include "ProjectManagement.h"
#include "core/Logger.h"
#include "core/SettingsManager.h"
#include "ui/ThemeManager.h"

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

// ── 辅助函数：长单词插入零宽空格，确保 QLabel wordWrap 可在任意处换行 ──
static QString breakLongWords(const QString& text, int maxLen = 25)
{
    QStringList parts = text.split(' ');
    for (QString& part : parts) {
        if (part.length() > maxLen) {
            QString broken;
            for (int i = 0; i < part.length(); ++i) {
                if (i > 0 && i % maxLen == 0)
                    broken += QChar(0x200B);  // 零宽空格
                broken += part[i];
            }
            part = broken;
        }
    }
    return parts.join(' ');
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
    m_projectDataPath = sm.activeProjectDataPath();

    if (m_projectDataPath.isEmpty())
        m_projectDataPath = QApplication::applicationDirPath() + "/project_data";

    initUI();
    AccessControl::watch(this,[this]() { applyPermissions(); });
    applyPermissions();
    applyTheme();
    refreshCards();

    // 主题变更 → 刷新所有样式
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ProjectManagement::applyTheme);

    Logger::info(QString::fromUtf8("项目管理模块已初始化 (数据根: %1)").arg(m_projectDataPath));
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

    // ===== 上栏：新建/打开项目按钮（固定大小）=====
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

    m_deleteProjectBtn = new QPushButton(QString::fromUtf8("  删除项目"));
    m_deleteProjectBtn->setIcon(QIcon(":/res/Del.png"));
    m_deleteProjectBtn->setIconSize(QSize(20, 20));
    m_deleteProjectBtn->setCursor(Qt::PointingHandCursor);
    m_deleteProjectBtn->setEnabled(false);
    m_deleteProjectBtn->setObjectName("pmDeleteProjectBtn");
    connect(m_deleteProjectBtn, &QPushButton::clicked, this, &ProjectManagement::onDeleteProject);

    topLayout->addWidget(m_newProjectBtn);
    topLayout->addWidget(m_openProjectBtn);
    topLayout->addWidget(m_deleteProjectBtn);
    topLayout->addStretch();

    // ===== 中栏：模型列表（占剩余空间 1/2）=====
    QWidget* modelPanel = new QWidget;
    modelPanel->setObjectName("pmModelPanel");
    QVBoxLayout* modelLayout = new QVBoxLayout(modelPanel);
    modelLayout->setContentsMargins(10, 8, 10, 4);
    modelLayout->setSpacing(4);

    m_modelListTitle = new QLabel(QString::fromUtf8("模型列表"));
    QFont titleFont = m_modelListTitle->font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    m_modelListTitle->setFont(titleFont);
    m_modelListTitle->setWordWrap(true);
    m_modelListTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    modelLayout->addWidget(m_modelListTitle);

    m_modelList = new QListWidget;
    m_modelList->setObjectName("pmModelList");
    connect(m_modelList, &QListWidget::itemClicked,
            this, &ProjectManagement::onModelSelected);
    modelLayout->addWidget(m_modelList, 1);

    // ===== 下栏：项目说明（占剩余空间 1/2，带滚动）=====
    QWidget* infoPanel = new QWidget;
    infoPanel->setObjectName("pmInfoPanel");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(10, 8, 10, 12);
    infoLayout->setSpacing(6);

    QLabel* infoTitle = new QLabel(QString::fromUtf8("项目说明"));
    QFont infoTitleFont = infoTitle->font();
    infoTitleFont.setPointSize(11);
    infoTitleFont.setBold(true);
    infoTitle->setFont(infoTitleFont);
    infoLayout->addWidget(infoTitle);

    QFrame* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("pmSep");
    infoLayout->addWidget(sep);

    m_projectFileLabel = new QLabel;
    m_projectFileLabel->setWordWrap(true);
    m_projectFileLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_projectFileLabel->setObjectName("pmInfoItem");
    infoLayout->addWidget(m_projectFileLabel);

    m_projectCreatedLabel = new QLabel;
    m_projectCreatedLabel->setWordWrap(true);
    m_projectCreatedLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_projectCreatedLabel->setObjectName("pmInfoItem");
    infoLayout->addWidget(m_projectCreatedLabel);

    m_projectModifiedLabel = new QLabel;
    m_projectModifiedLabel->setWordWrap(true);
    m_projectModifiedLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_projectModifiedLabel->setObjectName("pmInfoItem");
    infoLayout->addWidget(m_projectModifiedLabel);

    m_projectDescLabel = new QLabel;
    m_projectDescLabel->setWordWrap(true);
    m_projectDescLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_projectDescLabel->setObjectName("pmInfoDesc");
    infoLayout->addWidget(m_projectDescLabel);

    infoLayout->addStretch();

    // ── 将 infoPanel 包裹在 QScrollArea 中，仅纵向滚动 ──
    m_infoScrollArea = new QScrollArea;
    m_infoScrollArea->setWidgetResizable(true);
    m_infoScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_infoScrollArea->setFrameShape(QFrame::NoFrame);
    m_infoScrollArea->setObjectName("pmInfoScrollArea");
    m_infoScrollArea->setWidget(infoPanel);

    m_leftSplitter->addWidget(topPanel);
    m_leftSplitter->addWidget(modelPanel);
    m_leftSplitter->addWidget(m_infoScrollArea);
    m_leftSplitter->setSizes({ 90, 405, 405 });

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

    m_emptyHint = new QLabel(QString::fromUtf8("暂无项目\n请点击「新建项目」创建项目，或点击「打开项目」导入已有项目fvproj"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setObjectName("pmEmptyHint");

    QWidget* stackWidget = new QWidget;
    QVBoxLayout* stackLayout = new QVBoxLayout(stackWidget);
    stackLayout->setContentsMargins(0, 0, 0, 0);
    stackLayout->addWidget(m_emptyHint);
    stackLayout->addWidget(m_cardContainer);

    m_scrollArea->setWidget(stackWidget);
    layout->addWidget(m_scrollArea, 1);

    // ── 加载动画覆盖层 ──
    m_loadingOverlay = new LoadingOverlay(m_cardContainer,
        QString::fromUtf8("正在加载项目..."));
}

// ── 主题刷新 ──────────────────────────────────────────────────

void ProjectManagement::applyTheme()
{
    const auto& p = ThemeManager::instance().palette();

    // ── 左侧上栏背景 ──
    if (auto* w = m_leftSplitter->widget(0))  // topPanel
        w->setStyleSheet(
            QString("#pmLeftTop { background-color: %1; }").arg(p.bgSecondary));

    // ── 模型列表面板背景 ──
    if (auto* w = m_leftSplitter->widget(1))  // modelPanel
        w->setStyleSheet(
            QString("#pmModelPanel { background-color: %1; }").arg(p.bgSecondary));

    // ── 项目信息面板背景 ──
    if (m_infoScrollArea && m_infoScrollArea->widget())
        m_infoScrollArea->widget()->setStyleSheet(
            QString("#pmInfoPanel { background-color: %1; }").arg(p.bgSecondary));

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

    // ── 删除项目按钮（危险操作，选中时红色，未选中时灰色不可用）──
    m_deleteProjectBtn->setStyleSheet(
        QString("QPushButton#pmDeleteProjectBtn {"
                "  background-color: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 8px;"
                "  padding: 10px 16px; font-size: 13px; font-weight: bold;"
                "  text-align: left; }"
                "QPushButton#pmDeleteProjectBtn:hover {"
                "  background-color: %4; border-color: %5; }"
                "QPushButton#pmDeleteProjectBtn:hover:disabled {"
                "  background-color: %1; border-color: %3; }")
            // enabled: red bg, white text
            .arg(QString("#e74c3c"), QString("white"), QString("#c0392b"),
                 QString("#c0392b"), QString("#a93226"))
        +
        // disabled override (must be after to take priority for disabled state)
        QString("QPushButton#pmDeleteProjectBtn:disabled {"
                "  background-color: transparent; color: %1; border-color: %2; }")
            .arg(p.textMuted, p.borderLight));

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

    // ── 项目信息项（文件/创建时间/修改时间）──
    const QString infoItemStyle = QString(
        "QLabel#pmInfoItem { color: %1; font-size: 11px;"
        " padding: 2px 4px; }")
        .arg(p.textSecondary);

    m_projectFileLabel->setStyleSheet(infoItemStyle);
    m_projectCreatedLabel->setStyleSheet(infoItemStyle);
    m_projectModifiedLabel->setStyleSheet(infoItemStyle);

    // ── 项目描述 ──
    m_projectDescLabel->setStyleSheet(
        QString("QLabel#pmInfoDesc { color: %1; font-size: 11px;"
                " padding: 4px 6px; background-color: %2; border-radius: 4px; }")
            .arg(p.textSecondary, p.bgTertiary));

    // ── 分隔线 ──
    if (auto* sep = m_leftSplitter->widget(2)->findChild<QFrame*>("pmSep"))
        sep->setStyleSheet(QString("QFrame { color: %1; }").arg(p.borderLight));

    // ── 滚动区域 ──
    m_scrollArea->setStyleSheet(
        QString("QScrollArea#pmScrollArea { border: none; background-color: %1; }")
            .arg(p.bgPrimary));
    m_cardContainer->setStyleSheet(
        QString("#pmCardContainer { background-color: %1; }").arg(p.bgPrimary));

    // ── 项目信息滚动区域 ──
    if (m_infoScrollArea) {
        m_infoScrollArea->setStyleSheet(
            QString("QScrollArea#pmInfoScrollArea { border: none; background-color: %1; }")
                .arg(p.bgSecondary));
    }

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
    m_loadingOverlay->showOverlay();

    for (ProjectCard* card : m_cards) {
        m_cardLayout->removeWidget(card);
        delete card;
    }
    m_cards.clear();
    m_selectedCard = nullptr;

    QList<ProjectCardInfo> projects = ProjectConfig::scanProjects(m_projectDataPath);

    if (projects.isEmpty()) {
        m_emptyHint->setVisible(true);
        m_cardContainer->setVisible(false);
        m_loadingOverlay->hideOverlay();
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
        m_loadingOverlay->hideOverlay();
    }
}

// ── 新建项目 ──────────────────────────────────────────────────

void ProjectManagement::onNewProject()
{
    if(!WriteAccess::require("深度学习.项目管理",this)) return;
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
    if(!WriteAccess::require("深度学习.项目管理",this)) return;
    if (!ProjectConfig::createProject(name, desc, m_projectDataPath, &error)) {
        QMessageBox::warning(this, QString::fromUtf8("创建失败"), error);
        return;
    }

    QMessageBox::information(this, QString::fromUtf8("创建成功"),
        QString::fromUtf8("项目 \"%1\" 已创建。\n\n项目目录: %2/%1")
            .arg(name, m_projectDataPath));

    refreshCards();
}

// ── 打开项目 ──────────────────────────────────────────────────

void ProjectManagement::onOpenProject()
{
    if(!AccessControl::canRead("深度学习.项目管理")) return;
    QString fvprojPath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("打开项目"),
        m_projectDataPath,
        QString::fromUtf8("FuseVision 项目文件 (project.fvproj)"));

    if (fvprojPath.isEmpty()) return;
    if(beforeContextChange && !beforeContextChange()) return;

    m_loadingOverlay->showOverlay();

    QString error;
    FvprojInfo info = ProjectConfig::loadProject(fvprojPath, &error);
    if (info.projectName.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("打开失败"), error);
        m_loadingOverlay->hideOverlay();
        return;
    }

    // Open the explicitly chosen file in place; changing defaults never migrates a project.
    if(!AccessControl::canRead("深度学习.项目管理")) {m_loadingOverlay->hideOverlay();return;}
    m_currentProject     = info;
    m_currentProjectName = info.projectName;
    m_currentProjectPath = info.fvprojPath;
    m_hasProject         = true;

    refreshCards();
    updateLeftPanel();

    // 选中对应的卡片
    for (ProjectCard* card : m_cards) {
        if (card->fvprojPath() == info.fvprojPath) {
            card->setSelected(true);
            m_selectedCard = card;
            break;
        }
    }

    m_loadingOverlay->hideOverlay();

    Logger::info(QString::fromUtf8("项目已打开: %1").arg(info.projectName));
    emit projectOpened(info.projectName, info.fvprojPath);
}

void ProjectManagement::onDeleteProject()
{
    if(!WriteAccess::require("深度学习.项目管理",this)) return;
    if(!m_selectedCard) {QMessageBox::information(this,tr("提示"),tr("请先选中要删除的项目卡片。"));return;}
    const auto file=QFileInfo(m_selectedCard->fvprojPath()).absoluteFilePath();
    QString error;const auto target=ProjectConfig::loadProject(file,&error);
    const auto root=QFileInfo(file).absolutePath();
    if(target.projectName.isEmpty() || QDir::cleanPath(target.projectRoot)!=QDir::cleanPath(root)) {
        QMessageBox::warning(this,tr("删除失败"),tr("项目路径不一致，拒绝删除：%1\n%2").arg(file,error));return;
    }
    const bool current=m_hasProject && QFileInfo(m_currentProjectPath).absoluteFilePath()==file;
    QMessageBox box(this);box.setWindowTitle(tr("确认删除"));
    box.setText(tr("删除项目「%1」\n项目目录：%2\n项目文件：%3").arg(target.projectName,root,file));
    const auto recovery=QDir(QFileInfo(root).absolutePath()).filePath(".fusevision-trash");
    box.setInformativeText(tr("所选对象将移入恢复目录，不清除已有数据。可手动移回原路径恢复。\n%1").arg(recovery));
    auto* entire=box.addButton(tr("删除整个项目"),QMessageBox::DestructiveRole);
    auto* config=box.addButton(tr("仅删除项目文件"),QMessageBox::AcceptRole);
    auto* cancel=box.addButton(QMessageBox::Cancel);box.setDefaultButton(cancel);box.exec();
    if(box.clickedButton()!=entire && box.clickedButton()!=config) return;
    if(current && beforeContextChange && !beforeContextChange()) return;
    if(!WriteAccess::require("深度学习.项目管理",this)) return;
    QString destination;
    if(!RecoveryStore::move(box.clickedButton()==entire?root:file,recovery,destination,error)) {
        QMessageBox::warning(this,tr("删除失败"),error);return;
    }
    clearSelection();m_previewProject={};
    if(current) {
        m_hasProject=false;m_currentProject={};m_currentProjectName.clear();m_currentProjectPath.clear();
        emit projectClosed();
    }
    refreshCards();updateLeftPanel();applyPermissions();
    Logger::info(tr("项目已删除：%1；恢复路径：%2").arg(file,destination));
    QMessageBox::information(this,tr("删除成功"),tr("项目「%1」已移入：\n%2").arg(target.projectName,destination));
}

void ProjectManagement::onCardClicked(ProjectCard* card)
{
    if(!AccessControl::canRead("深度学习.项目管理")) return;
    clearSelection();
    m_selectedCard = card;
    card->setSelected(true);

    // 单击即加载该项目元信息到左侧面板（不含"打开"语义，不发射信号、不切 Tab）
    QString error;
    FvprojInfo info = ProjectConfig::loadProject(card->fvprojPath(), &error);
    if (info.projectName.isEmpty()) return;

    // 注入到临时预览状态：仅刷新左侧信息，不标记 m_hasProject
    m_previewProject = info;

    m_projectFileLabel->setText(
        breakLongWords(QString::fromUtf8("项目文件: %1").arg(info.projectRoot)));

    m_projectCreatedLabel->setText(
        breakLongWords(QString::fromUtf8("创建时间: %1")
            .arg(info.createdTime.toString("yyyy-MM-dd hh:mm"))));

    m_projectModifiedLabel->setText(
        breakLongWords(QString::fromUtf8("修改时间: %1")
            .arg(info.modifiedTime.toString("yyyy-MM-dd hh:mm"))));

    if (!info.description.isEmpty())
        m_projectDescLabel->setText(
            breakLongWords(QString::fromUtf8("项目描述: %1").arg(info.description)));
    else
        m_projectDescLabel->setText(QString::fromUtf8("项目描述: （暂无）"));

    m_modelListTitle->setText(
        QString::fromUtf8("模型列表 (%1)").arg(info.models.size()));
    m_modelList->clear();
    QJsonObject models = info.models;
    for (auto it = models.begin(); it != models.end(); ++it)
        m_modelList->addItem(it.key());

    emit projectPreviewed(info.projectName);
    m_deleteProjectBtn->setEnabled(AccessControl::canWrite("深度学习.项目管理"));
}

void ProjectManagement::onCardDoubleClicked(ProjectCard* card)
{
    if(!AccessControl::canRead("深度学习.项目管理")) return;
    if(!card || (beforeContextChange && !beforeContextChange())) return;
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

    refreshCards();
    updateLeftPanel();

    // 选中对应的卡片
    for (ProjectCard* card : m_cards) {
        if (card->fvprojPath() == fvproj) {
            card->setSelected(true);
            m_selectedCard = card;
            break;
        }
    }

    Logger::info(QString::fromUtf8("项目已打开(双击): %1").arg(info.projectName));
    emit projectOpened(info.projectName, fvproj);
}

void ProjectManagement::clearSelection()
{
    for (ProjectCard* c : m_cards)
        c->setSelected(false);
    m_selectedCard = nullptr;
}

// ── 递归复制目录 ──────────────────────────────────────────────

bool ProjectManagement::copyDirectory(const QString& src, const QString& dst)
{
    QDir srcDir(src);
    if (!srcDir.exists()) return false;

    QDir dstDir(dst);
    if (!dstDir.exists() && !dstDir.mkpath(dst)) return false;

    // 复制文件
    for (const QFileInfo& fi : srcDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if(fi.isSymLink()) return false;
        if (!QFile::copy(fi.absoluteFilePath(), dstDir.absoluteFilePath(fi.fileName())))
            return false;
    }

    // 递归复制子目录
    for (const QFileInfo& fi : srcDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if(fi.isSymLink()) return false;
        if (!copyDirectory(fi.absoluteFilePath(), dstDir.absoluteFilePath(fi.fileName())))
            return false;
    }

    return true;
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
        m_projectFileLabel->setText(
            breakLongWords(QString::fromUtf8("项目文件: %1").arg(m_currentProject.projectRoot)));

        m_projectCreatedLabel->setText(
            breakLongWords(QString::fromUtf8("创建时间: %1")
                .arg(m_currentProject.createdTime.toString("yyyy-MM-dd hh:mm"))));

        m_projectModifiedLabel->setText(
            breakLongWords(QString::fromUtf8("修改时间: %1")
                .arg(m_currentProject.modifiedTime.toString("yyyy-MM-dd hh:mm"))));

        if (!m_currentProject.description.isEmpty())
            m_projectDescLabel->setText(
                breakLongWords(QString::fromUtf8("项目描述: %1").arg(m_currentProject.description)));
        else
            m_projectDescLabel->setText(QString::fromUtf8("项目描述: （暂无）"));

        m_modelListTitle->setText(
            QString::fromUtf8("模型列表 (%1)").arg(m_currentProject.models.size()));
        m_modelList->clear();
        refreshModelList();
        m_deleteProjectBtn->setEnabled(AccessControl::canWrite("深度学习.项目管理"));
    } else {
        m_projectFileLabel->setText(QString::fromUtf8("项目文件: —"));
        m_projectCreatedLabel->setText(QString::fromUtf8("创建时间: —"));
        m_projectModifiedLabel->setText(QString::fromUtf8("修改时间: —"));
        m_projectDescLabel->setText(QString::fromUtf8("项目描述: —"));
        m_modelListTitle->setText(QString::fromUtf8("模型列表"));
        m_modelList->clear();
        m_deleteProjectBtn->setEnabled(false);
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

void ProjectManagement::applyPermissions()
{
    const bool write=AccessControl::canWrite("深度学习.项目管理");
    m_newProjectBtn->setEnabled(write);
    m_openProjectBtn->setEnabled(AccessControl::canRead("深度学习.项目管理"));
    m_deleteProjectBtn->setEnabled(write && m_selectedCard);
}
