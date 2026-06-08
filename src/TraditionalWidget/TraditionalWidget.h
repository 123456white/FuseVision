#ifndef TRADITIONALWIDGET_H
#define TRADITIONALWIDGET_H

#include <QWidget>
#include <QFrame>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QToolBox>
#include <QToolBar>
#include <QLabel>
#include <QShowEvent>
#include "core/PermissionGuard.h"
#include "core/LogMonitor.h"

// =============================================================================
// TraditionalWidget — 传统视觉模块页面（三区布局 + 双模式）
// =============================================================================
//
// 布局结构（三区均有统一标题）：
//   ┌──────────────────────────────────────────────────┐
//   │ [流程文件] [相机配置] [相机标定] ... [系统设置]     │ ← QToolBar
//   ├──────────┬─────────────────────────┬─────────────┤
//   │资源管理  │       流程编辑器         │  视觉工具    │ ← sectionTitle × 3
//   │QTree     │  QStackedWidget(2 页)   │  QToolBox   │
//   │Widget    │  [0] 流程编辑模式        │  采集/预处理│
//   │项目树    │  [1] 图像显示模式        │  定位/测量  │
//   │          │                         │  识别       │
//   └──────────┴─────────────────────────┴─────────────┘
//   ├──────────────────────────────────────────────────┤
//   │ 日志监控                                     ▲   │ ← LogMonitor（可折叠）
//   │ [10:30:15] 相机采集已启动 ...                   │
//   └──────────────────────────────────────────────────┘
//
// 双模式切换：通过工具栏或资源区右键菜单切换编辑区 StackedWidget 页面
//   流程编辑模式：拖拽式流程编辑器（预留 QGraphicsView）
//   图像显示模式：实时采集图像 + ROI 叠加（预留 QLabel）
//
// 权限控制：7 个模块权限点
// =============================================================================

class TraditionalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TraditionalWidget(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;  // 初始化分割器比例

private slots:
    void applyPermissions();       // 权限变更时刷新 UI
    void onModeSwitch(int mode);   // 双模式切换（0=流程编辑, 1=图像显示）

private:
    void initUI();
    void initToolBar();            // 构建顶部工具栏
    void initResourcePanel();      // 构建左侧资源管理区
    void initEditorArea();         // 构建中间编辑区（QStackedWidget 双模式）
    void initToolPanel();          // 构建右侧工具区

    // ── 成员变量 ──────────────────────────────────────────────
    PermissionGuard* m_guard          = nullptr;  // 权限代理

    // 工具栏
    QToolBar*        m_toolBar        = nullptr;

    // 三区分割器
    QSplitter*       m_mainSplitter   = nullptr;  // 水平三区

    // 左：资源管理区
    QFrame*          m_resourceFrame  = nullptr;  // 资源区外框（标题+树）
    QTreeWidget*     m_resourceTree   = nullptr;  // 项目树

    // 中：编辑区（双模式）
    QFrame*          m_editorFrame    = nullptr;  // 流程编辑器外框（标题+双模式）
    QStackedWidget*  m_editorStack    = nullptr;  // 双模式容器
    QWidget*         m_flowEditPage   = nullptr;  // [0] 流程编辑模式
    QWidget*         m_imageViewPage  = nullptr;  // [1] 图像显示模式

    // 右：工具区
    QFrame*          m_toolFrame      = nullptr;  // 工具区外框（标题+工具盒）
    QToolBox*        m_toolBox        = nullptr;  // 视觉工具库

    // 底部日志
    LogMonitor*      m_logMonitor     = nullptr;  // 日志监控面板
    bool             m_shown          = false;     // 首次显示标记

    // 7 个权限键
    static const QStringList s_permissionKeys;
};

#endif // TRADITIONALWIDGET_H
