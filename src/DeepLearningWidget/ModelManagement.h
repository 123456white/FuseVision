#ifndef MODELMANAGEMENT_H
#define MODELMANAGEMENT_H

#include <QWidget>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>
#include <QJsonObject>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QTimer>
#include "ProjectConfig.h"
#include "ModelConfig.h"
#include "core/ThemeManager.h"

// =============================================================================
// ModelCard — 紧凑模型卡片（图标 + 合并的名称/类型，单击选中/双击进入数据集）
// =============================================================================
class ModelCard : public QFrame
{
    Q_OBJECT
public:
    explicit ModelCard(const QString& modelId, const QString& modelType,
                       QWidget* parent = nullptr);
    ~ModelCard();

    QString modelId()   const { return m_modelId; }
    QString modelType() const { return m_modelType; }
    bool isSelected()   const { return m_selected; }

    void setSelected(bool selected);
    void refreshTheme();

signals:
    void clicked(ModelCard* card);
    void doubleClicked(ModelCard* card);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onClickTimer();

private:
    void applyCardStyle();

    QString m_modelId;
    QString m_modelType;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_infoLabel  = nullptr;  // 合并显示 名称 + 类型
    bool    m_selected   = false;
    bool    m_hovered    = false;
    QTimer* m_clickTimer = nullptr;
};

class StepGuideBar;  // 前向声明（定义在 ProjectManagement.h）

// =============================================================================
// ModelManagement — 模型管理标签页（DeepLearningWidget Tab 1）
// =============================================================================
// 布局（左右 1:5）：
//   左侧边栏（上下 1:9）：
//     ┌────────────────────┐
//     │ 新建模型│打开模型   │ ← 圆角按钮
//     ├────────────────────┤
//     │ 当前模型            │
//     │ ·模型文件路径       │
//     │ ·图像目录           │
//     │ ·标签类别 / 实例    │
//     │ ·创建/修改时间      │
//     └────────────────────┘
//   右侧主体（纯卡片画廊）：
//     ┌────────────────────┐
//     │ ┌──┐ ┌──┐ ┌──┐    │ ← 模型卡片画廊
//     │ │图│ │图│ │图│    │   单击选中 / 双击进入数据集
//     │ │名│ │名│ │名│    │
//     │ └──┘ └──┘ └──┘    │
//     └────────────────────┘
// =============================================================================
class ModelManagement : public QWidget
{
    Q_OBJECT
public:
    explicit ModelManagement(QWidget* parent = nullptr);

    void setCurrentProject(const FvprojInfo& project);

    QString currentModelName() const { return m_currentModel.modelName; }
    QString currentModelId()   const { return m_currentModelId; }
    FvdlInfo currentModelInfo() const { return m_currentModel; }
    bool    hasModel()         const { return !m_currentModelId.isEmpty(); }

signals:
    void modelCreated(const QString& modelName);
    void modelOpened(const QString& modelName);
    void currentModelChanged(const QString& modelName);
    void openDatasetRequested(const QString& modelId);  // 双击卡片 → 进入数据集管理

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onNewModel();
    void onOpenModel();

private:
    void onCardClicked(ModelCard* card);
    void onCardDoubleClicked(ModelCard* card);

    void initUI();
    void initLeftPanel();
    void initRightPanel();
    void applyTheme();
    void updateLeftInfo();         // 刷新左侧下栏模型详情
    void updateStepGuide();        // 根据模型 status 刷新步骤引导条
    void refreshCards();
    void relayoutCards();

    // ── 项目上下文 ─────────────────────────────────────────────
    FvprojInfo m_project;
    QString    m_dataRoot;
    QString    m_datasetRoot;
    QString    m_modelRoot;
    QString    m_currentModelId;
    FvdlInfo   m_currentModel;

    // ── 左侧边栏 ──────────────────────────────────────────────
    QPushButton* m_newModelBtn   = nullptr;
    QPushButton* m_openModelBtn  = nullptr;
    QLabel*      m_detailLabel   = nullptr;   // 当前模型详细信息（文件路径/目录/标签/时间）

    // ── 右侧主体 ──────────────────────────────────────────────
    QWidget*     m_rightContainer = nullptr;
    QScrollArea* m_scrollArea     = nullptr;
    QWidget*     m_cardContainer  = nullptr;
    QGridLayout* m_cardLayout     = nullptr;
    QLabel*      m_emptyHint      = nullptr;   // 无模型时提示
    StepGuideBar* m_stepGuideBar   = nullptr;   // 底部步骤引导条

    // ── 卡片状态 ──────────────────────────────────────────────
    QList<ModelCard*> m_cards;
    ModelCard*        m_selectedCard = nullptr;

    // ── 布局容器 ──────────────────────────────────────────────
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_leftSplitter = nullptr;
};

#endif // MODELMANAGEMENT_H
