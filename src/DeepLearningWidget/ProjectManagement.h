#ifndef PROJECTMANAGEMENT_H
#define PROJECTMANAGEMENT_H

#include <QWidget>
#include <QSplitter>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QEnterEvent>
#include "ProjectConfig.h"
#include "core/ThemeManager.h"

// =============================================================================
// StepGuideBar — 底部工作流步骤引导条
// =============================================================================
// 横向展示 6 个固定步骤（每步骤一个圆点 + 步骤名）：
//   项目管理 → 模型管理 → 数据集管理 → 数据标注 → 数据拆分 → 模型训练
// 当前所在步骤高亮，行进方向为从左到右。
class StepGuideBar : public QWidget
{
    Q_OBJECT
public:
    explicit StepGuideBar(QWidget* parent = nullptr);
    void setCurrentStep(int step);          // 0~5，设置当前高亮步骤
    void setStepCompleted(int step);        // 标记某步骤为已完成

private:
    void buildSteps();
    void refreshTheme();                    // 主题切换时重绘样式
    void applyStepStyles();                 // 根据当前步/完成状态刷颜色

    struct StepDot {
        QLabel* dot   = nullptr;
        QLabel* label = nullptr;
        QFrame* line  = nullptr;
    };
    QList<StepDot> m_steps;
    int            m_currentStep = 0;
    QList<bool>    m_completed;  // size = 6
};

// =============================================================================
// ProjectCard — 项目卡片（图片 + 名称，点击可选中/打开）
// =============================================================================
class ProjectCard : public QFrame
{
    Q_OBJECT
public:
    explicit ProjectCard(const ProjectCardInfo& info, QWidget* parent = nullptr);
    QString projectName() const { return m_info.projectName; }
    QString fvprojPath() const { return m_info.fvprojPath; }

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void refreshTheme();

signals:
    void clicked(ProjectCard* card);
    void doubleClicked(ProjectCard* card);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void applyCardStyle();          // 根据 selected/hovered 刷边框背景

    ProjectCardInfo m_info;
    QLabel*  m_imageLabel  = nullptr;
    QLabel*  m_nameLabel   = nullptr;
    bool     m_selected    = false;
    bool     m_hovered     = false;
};

// =============================================================================
// ProjectManagement — 项目管理标签页（DeepLearningWidget Tab 0）
// =============================================================================
// 布局（左右 1:5）：
//   左侧边栏（上下 1:9）：
//     ┌────────────────────┐
//     │ 新建项目│打开项目   │ ← 圆角按钮（左图标右文字）
//     ├────────────────────┤
//     │ 模型列表           │
//     │ ┌────────────────┐ │
//     │ │ model1 (分割)  │ │ ← 当前项目的所有模型
//     │ │ model2 (检测)  │ │
//     │ └────────────────┘ │
//     │ 创建/修改时间       │
//     │ ─────────────────  │
//     │ 逻辑说明文字       │
//     └────────────────────┘
//   右侧主体：
//     ┌────────────────────┐
//     │ ┌──┐ ┌──┐ ┌──┐    │
//     │ │图│ │图│ │图│    │ ← 项目卡片网格（3 列）
//     │ │名│ │名│ │名│    │
//     │ └──┘ └──┘ └──┘    │
//     ├────────────────────┤
//     │ ○→○→○→○→○→○     │ ← 步骤引导条
//     └────────────────────┘
// =============================================================================
class ProjectManagement : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectManagement(QWidget* parent = nullptr);

    QString currentProjectName() const { return m_currentProjectName; }
    QString currentProjectPath() const { return m_currentProjectPath; }
    bool    hasProject()         const { return m_hasProject; }
    FvprojInfo currentProject()  const { return m_hasProject ? m_currentProject : m_previewProject; }

public slots:
    void refreshCards();           // 重新扫描并刷新卡片网格

signals:
    void projectOpened(const QString& projectName, const QString& projectPath);
    void projectClosed();
    void projectPreviewed(const QString& projectName);  // 单击选中时触发

protected:
    void resizeEvent(QResizeEvent* event) override;  // 窗口大小变化 → 重排卡片列数

private slots:
    void onNewProject();                                                // 新建项目对话框
    void onOpenProject();                                               // 打开项目对话框
    void onCardClicked(ProjectCard* card);                              // 单击选中卡片
    void onCardDoubleClicked(ProjectCard* card);                        // 双击打开项目
    void onModelSelected(QListWidgetItem* item);                        // 切换当前模型

private:
    void initUI();                     // 构建整体布局
    void initLeftPanel();              // 左侧边栏（按钮 + 模型列表 + 信息）
    void initRightPanel();             // 右侧主体（卡片网格 + 步骤引导条）
    void applyTheme();                 // 主题切换时刷新所有子控件样式
    void updateLeftPanel();            // 随项目状态刷新左侧面板
    void refreshModelList();           // 刷新模型列表
    void updateStepGuide();            // 刷新步骤引导条高亮
    void relayoutCards();              // 根据当前可用宽度重排卡片网格
    void clearSelection();             // 清除卡片选中

    // ── 项目状态 ──────────────────────────────────────────────
    QString    m_currentProjectName;
    QString    m_currentProjectPath;
    FvprojInfo m_currentProject;
    FvprojInfo m_previewProject;    // 单击卡片时的临时预览
    bool       m_hasProject = false;

    // ── 全局路径 ──────────────────────────────────────────────
    QString    m_dlDataPath;
    QString    m_dlDatasetPath;
    QString    m_dlModelPath;

    // ── 左侧边栏控件 ──────────────────────────────────────────
    QPushButton*  m_newProjectBtn   = nullptr;
    QPushButton*  m_openProjectBtn  = nullptr;
    QLabel*       m_modelListTitle  = nullptr;
    QListWidget*  m_modelList       = nullptr;
    QLabel*       m_projectTimeLabel = nullptr;
    QLabel*       m_projectDescLabel = nullptr;   // 项目描述
    QLabel*       m_logicDescLabel  = nullptr;

    // ── 右侧主体控件 ──────────────────────────────────────────
    QWidget*       m_rightContainer = nullptr;  // 右侧整体容器
    QScrollArea*   m_scrollArea    = nullptr;
    QWidget*       m_cardContainer = nullptr;
    QGridLayout*   m_cardLayout    = nullptr;
    QLabel*        m_emptyHint     = nullptr;
    StepGuideBar*  m_stepGuideBar  = nullptr;

    // ── 卡片选中状态 ──────────────────────────────────────────
    QList<ProjectCard*> m_cards;
    ProjectCard*        m_selectedCard = nullptr;

    // ── 布局容器 ──────────────────────────────────────────────
    QSplitter*     m_mainSplitter = nullptr;
    QSplitter*     m_leftSplitter = nullptr;
};

#endif // PROJECTMANAGEMENT_H
