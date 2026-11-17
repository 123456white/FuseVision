/** @file
 * @brief DeepLearningWidget 的公共声明。
 * @ingroup dl_widgets
 */
#ifndef DEEPLEARNINGWIDGET_H
#define DEEPLEARNINGWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QShowEvent>
#include <QPushButton>
#include "ui/PermissionGuard.h"
#include "ui/LogMonitor.h"
#include "ui/LoadingOverlay.h"

// 深度学习页面：项目、模型、数据集和 Qt 原生标注工作区。

class ProjectManagement;
class ModelManagement;
class DatasetManagement;
class AnnotationManagement;

/** @brief 深度学习页面和上下文协调器，连接项目、模型、数据集和标注信号。
 * @ingroup dl_widgets
 */
class DeepLearningWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeepLearningWidget(QWidget* parent = nullptr);
    ~DeepLearningWidget() override;

    QString currentProjectName() const;
    bool prepareToLeave();

signals:
    void dlProjectChanged(const QString& projectName);
    void dlModelChanged(const QString& modelName);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void applyPermissions();
    void onTabChanged(int index);
    void onProjectOpened(const QString& name, const QString& path);
    void onProjectClosed();

private:
    void initUI();

    // ── 成员变量 ──────────────────────────────────────────────
    PermissionGuard* m_guard          = nullptr;
    QTabBar*         m_tabBar         = nullptr;
    QStackedWidget*  m_stackedWidget  = nullptr;
    LogMonitor*      m_logMonitor     = nullptr;
    bool             m_shown          = false;

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
