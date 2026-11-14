#ifndef DEEPLEARNINGWIDGET_H
#define DEEPLEARNINGWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QShowEvent>
#include <QPushButton>
#include <QProcess>
#include "core/PermissionGuard.h"
#include "core/LogMonitor.h"
#include "core/LoadingOverlay.h"

// =============================================================================
// DeepLearningWidget — 深度学习模块页面（9 标签页 + LabelMe 嵌入模式）
// =============================================================================
// 正常模式：
//   ┌──────────────────────────────────────────┐
//   │ [项目管理] [模型管理] [数据集] ... [导出]  │ ← QTabBar
//   ├──────────────────────────────────────────┤
//   │        QStackedWidget (9 个标签页)        │
//   ├──────────────────────────────────────────┤
//   │ 日志监控                              ▲   │ ← LogMonitor（始终可见）
//   └──────────────────────────────────────────┘
//
// LabelMe 嵌入模式：
//   ┌──────────────────────────────────────────┐
//   │  ← 退出 LabelMe                          │ ← 退出按钮栏
//   ├──────────────────────────────────────────┤
//   │                                          │
//   │         LabelMe (PyQt5 嵌入)              │
//   │                                          │
//   ├──────────────────────────────────────────┤
//   │ 日志监控                              ▲   │ ← LogMonitor（始终可见）
//   └──────────────────────────────────────────┘
// =============================================================================

class ProjectManagement;
class ModelManagement;
class DatasetManagement;
class AnnotationManagement;

class DeepLearningWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeepLearningWidget(QWidget* parent = nullptr);
    ~DeepLearningWidget() override;

    QString currentProjectName() const;

    // LabelMe 嵌入
    void enterLabelMeMode(const QString& imageDir, const QString& currentFile,
                          const QString& pythonExe, const QString& sdkPath,
                          const QStringList& labels, const QString& outputDir);
    void exitLabelMeMode();

signals:
    void dlProjectChanged(const QString& projectName);
    void dlModelChanged(const QString& modelName);

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void applyPermissions();
    void onTabChanged(int index);
    void onProjectOpened(const QString& name, const QString& path);
    void onProjectClosed();

private:
    void initUI();
    void embedLabelMeWindow();
    void finishLabelMeCleanup();

    // ── 成员变量 ──────────────────────────────────────────────
    PermissionGuard* m_guard          = nullptr;
    QTabBar*         m_tabBar         = nullptr;
    QStackedWidget*  m_stackedWidget  = nullptr;
    LogMonitor*      m_logMonitor     = nullptr;
    bool             m_shown          = false;

    // ── LabelMe 嵌入 ──
    QStackedWidget*  m_topStack        = nullptr;  // 0=正常, 1=LabelMe
    QWidget*         m_labelmeContainer = nullptr;
    QPushButton*     m_btnExitLabelMe   = nullptr;
    LoadingOverlay*  m_loadingOverlay   = nullptr;
    QProcess*        m_labelmeProcess   = nullptr;
    WId              m_labelmeWinId     = 0;
    bool             m_labelmeEmbedded  = false;
    bool             m_exitingLabelMe   = false;

    // ── 子页面 ──────────────────────────────────────────────
    ProjectManagement*   m_projectManagement   = nullptr;
    ModelManagement*     m_modelManagement     = nullptr;
    DatasetManagement*   m_datasetManagement   = nullptr;
    AnnotationManagement* m_annotationManagement = nullptr;

    // 8 个标签页名称
    static const QStringList s_tabNames;
    static const QStringList s_permissionKeys;
};

#endif // DEEPLEARNINGWIDGET_H
