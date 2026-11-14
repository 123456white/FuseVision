#include "DeepLearningWidget.h"
#include "ProjectManagement.h"
#include "ModelManagement.h"
#include "DatasetManagement.h"
#include "AnnotationManagement.h"

#ifdef Q_OS_WIN
#  define NOMINMAX
#  include <windows.h>
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QCoreApplication>
#include <QTimer>

// =============================================================================
// DeepLearningWidget.cpp — 深度学习模块 9 标签页 + LabelMe 嵌入模式
// =============================================================================

// 9 个标签页的名称
const QStringList DeepLearningWidget::s_tabNames = {
    QString::fromUtf8("项目管理"), QString::fromUtf8("模型管理"),
    QString::fromUtf8("数据集管理"), QString::fromUtf8("数据标注"),
    QString::fromUtf8("数据检查"), QString::fromUtf8("数据拆分"),
    QString::fromUtf8("模型训练"), QString::fromUtf8("模型预测"),
    QString::fromUtf8("模型导出")
};

// 9 个权限键
const QStringList DeepLearningWidget::s_permissionKeys = {
    QString::fromUtf8("深度学习.项目管理"), QString::fromUtf8("深度学习.模型管理"),
    QString::fromUtf8("深度学习.数据集管理"), QString::fromUtf8("深度学习.数据标注"),
    QString::fromUtf8("深度学习.数据检查"), QString::fromUtf8("深度学习.数据拆分"),
    QString::fromUtf8("深度学习.模型训练"), QString::fromUtf8("深度学习.模型预测"),
    QString::fromUtf8("深度学习.模型导出")
};

// ── 析构 ──────────────────────────────────────────────────────

DeepLearningWidget::~DeepLearningWidget()
{
    // 标记正在销毁，阻止所有定时器回调
    m_exitingLabelMe = true;

    // 必须在 Qt 父子析构链删除 QProcess 前终止 Python 进程
    if (m_labelmeProcess) {
        m_labelmeProcess->disconnect();
        if (m_labelmeProcess->state() != QProcess::NotRunning) {
            m_labelmeProcess->terminate();
            if (!m_labelmeProcess->waitForFinished(3000))
                m_labelmeProcess->kill();
            m_labelmeProcess->waitForFinished(2000);
        }
        // 不 deleteLater——析构函数中让 Qt 父子链自然回收
        m_labelmeProcess = nullptr;
    }
}

// ── 构造 ──────────────────────────────────────────────────────

DeepLearningWidget::DeepLearningWidget(QWidget* parent)
    : QWidget(parent)
{
    initUI();

    m_guard = new PermissionGuard(this);
    m_guard->watch(s_permissionKeys);
    connect(m_guard, &PermissionGuard::changed,
            this, &DeepLearningWidget::applyPermissions);
    applyPermissions();
}

// ── UI 构建 ───────────────────────────────────────────────────

void DeepLearningWidget::initUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 8, 24, 20);
    mainLayout->setSpacing(12);

    // ===== 顶部标签栏 =====
    m_tabBar = new QTabBar;
    m_tabBar->setShape(QTabBar::RoundedNorth);
    m_tabBar->setExpanding(true);
    m_tabBar->setDrawBase(true);
    m_tabBar->setUsesScrollButtons(false);
    m_tabBar->setContentsMargins(0, 0, 0, 0);

    for (const QString& name : s_tabNames)
        m_tabBar->addTab(name);

    connect(m_tabBar, &QTabBar::currentChanged,
            this, &DeepLearningWidget::onTabChanged);

    // ===== 内容区容器 =====
    QFrame* contentFrame = new QFrame;
    contentFrame->setObjectName("dlContentFrame");
    QVBoxLayout* frameLayout = new QVBoxLayout(contentFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget;

    // Page 0 — 项目管理
    m_projectManagement = new ProjectManagement;
    m_stackedWidget->addWidget(m_projectManagement);

    // Page 1 — 模型管理
    m_modelManagement = new ModelManagement;
    m_stackedWidget->addWidget(m_modelManagement);

    // Page 2 — 数据集管理
    m_datasetManagement = new DatasetManagement;
    m_stackedWidget->addWidget(m_datasetManagement);

    // Page 3 — 数据标注
    m_annotationManagement = new AnnotationManagement;
    m_stackedWidget->addWidget(m_annotationManagement);

    // Page 4 — 数据检查（占位，待开发）
    {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        pageLayout->setAlignment(Qt::AlignCenter);
        QLabel* placeholder = new QLabel(
            QString::fromUtf8("数据检查 — 内容待开发"));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("placeholderLabel");
        pageLayout->addWidget(placeholder);
        m_stackedWidget->addWidget(page);
    }

    // Page 5~8 — 占位
    for (int i = 5; i < s_tabNames.size(); ++i) {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        pageLayout->setAlignment(Qt::AlignCenter);
        QLabel* placeholder = new QLabel(
            QString::fromUtf8("%1 — 内容待开发").arg(s_tabNames[i]));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setObjectName("placeholderLabel");
        pageLayout->addWidget(placeholder);
        m_stackedWidget->addWidget(page);
    }
    m_stackedWidget->setCurrentIndex(0);
    frameLayout->addWidget(m_stackedWidget);

    // ===== 正常模式页面 = TabBar + ContentFrame =====
    QWidget* normalPage = new QWidget;
    QVBoxLayout* normalLayout = new QVBoxLayout(normalPage);
    normalLayout->setContentsMargins(0, 0, 0, 0);
    normalLayout->setSpacing(0);
    normalLayout->addWidget(m_tabBar);
    normalLayout->addWidget(contentFrame, 1);

    // ===== LabelMe 嵌入页面 =====
    QWidget* labelmePage = new QWidget;
    QVBoxLayout* labelmeLayout = new QVBoxLayout(labelmePage);
    labelmeLayout->setContentsMargins(0, 0, 0, 0);
    labelmeLayout->setSpacing(0);

    // --- 退出按钮栏 ---
    QWidget* exitBar = new QWidget;
    exitBar->setObjectName("labelmeExitBar");
    exitBar->setFixedHeight(40);
    QHBoxLayout* exitBarLayout = new QHBoxLayout(exitBar);
    exitBarLayout->setContentsMargins(8, 4, 12, 4);

    m_btnExitLabelMe = new QPushButton(QString::fromUtf8("\u25C0  退出 LabelMe"));
    m_btnExitLabelMe->setObjectName("btnExitLabelMe");
    m_btnExitLabelMe->setCursor(Qt::PointingHandCursor);
    m_btnExitLabelMe->setMinimumHeight(32);
    m_btnExitLabelMe->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_btnExitLabelMe->setStyleSheet(
        "QPushButton#btnExitLabelMe {"
        " background: #e74c3c; color: #fff; border: none; border-radius: 4px;"
        " padding: 4px 16px; font-size: 13px; font-weight: bold;"
        " min-width: 130px; }"
        "QPushButton#btnExitLabelMe:hover { background: #c0392b; }");
    connect(m_btnExitLabelMe, &QPushButton::clicked,
            this, &DeepLearningWidget::exitLabelMeMode);

    exitBarLayout->addWidget(m_btnExitLabelMe);
    exitBarLayout->addStretch();
    labelmeLayout->addWidget(exitBar);

    // --- LabelMe 容器 ---
    m_labelmeContainer = new QWidget;
    m_labelmeContainer->setObjectName("labelmeEmbedContainer");
    m_labelmeContainer->setAttribute(Qt::WA_NativeWindow, true);      // 必须有原生 HWND
    m_labelmeContainer->setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_labelmeContainer->setAutoFillBackground(false);
    labelmeLayout->addWidget(m_labelmeContainer, 1);

    // --- 加载动画覆盖层 ---
    m_loadingOverlay = new LoadingOverlay(m_labelmeContainer,
        QString::fromUtf8("正在启动 LabelMe..."));

    // ===== QStackedWidget：0 = 正常, 1 = LabelMe =====
    m_topStack = new QStackedWidget;
    m_topStack->addWidget(normalPage);   // index 0
    m_topStack->addWidget(labelmePage);  // index 1
    m_topStack->setCurrentIndex(0);

    // ===== 垂直分割器：内容 + 日志 =====
    QSplitter* vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->setHandleWidth(2);
    vSplitter->setChildrenCollapsible(false);
    vSplitter->addWidget(m_topStack);

    m_logMonitor = new LogMonitor;
    m_logMonitor->setMaximumHeight(300);
    vSplitter->addWidget(m_logMonitor);

    vSplitter->setStretchFactor(0, 4);
    vSplitter->setStretchFactor(1, 1);
    vSplitter->setSizes({600, 150});

    mainLayout->addWidget(vSplitter, 1);

    // ── 信号连接 ─────────────────────────────────────
    // 项目 → 模型管理
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, &DeepLearningWidget::onProjectOpened);
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, &DeepLearningWidget::onProjectClosed);
    connect(m_projectManagement, &ProjectManagement::projectPreviewed,
            this, [this](const QString& name) { emit dlProjectChanged(name); });

    // 项目 → ModelManagement 绑定
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, [this](const QString&, const QString&) {
                m_modelManagement->setCurrentProject(
                    m_projectManagement->currentProject());
            });
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, [this]() {
                m_modelManagement->setCurrentProject(FvprojInfo());
            });

    // ModelManagement 日志
    connect(m_modelManagement, &ModelManagement::modelCreated,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(
                    QString::fromUtf8("模型已创建: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::modelOpened,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(
                    QString::fromUtf8("模型已打开: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            m_logMonitor, [this](const QString& name) {
                m_logMonitor->log(
                    QString::fromUtf8("当前模型切换至: %1").arg(name));
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            this, [this](const QString&) {
                emit dlModelChanged(m_modelManagement->currentModelName());
            });

    // 双击模型 → 跳数据集
    connect(m_modelManagement, &ModelManagement::openDatasetRequested,
            this, [this](const QString& modelId) {
                m_logMonitor->log(
                    QString::fromUtf8("跳转数据集管理: %1").arg(modelId));
                if (m_tabBar->isTabVisible(2)) {
                    m_tabBar->setCurrentIndex(2);
                    m_stackedWidget->setCurrentIndex(2);
                }
            });

    // ── DatasetManagement ────────────────────────────
    connect(m_projectManagement, &ProjectManagement::projectOpened,
            this, [this](const QString&, const QString&) {
                m_datasetManagement->setCurrentProject(
                    m_projectManagement->currentProject());
            });
    connect(m_projectManagement, &ProjectManagement::projectClosed,
            this, [this]() {
                m_datasetManagement->setCurrentProject(FvprojInfo());
            });
    connect(m_modelManagement, &ModelManagement::currentModelChanged,
            this, [this](const QString& modelId) {
                m_datasetManagement->setCurrentModel(
                    modelId, m_modelManagement->currentModelInfo());
            });
    connect(m_modelManagement, &ModelManagement::modelOpened,
            this, [this](const QString&) {
                m_datasetManagement->setCurrentModel(
                    m_modelManagement->currentModelId(),
                    m_modelManagement->currentModelInfo());
            });
    connect(m_datasetManagement, &DatasetManagement::dataImported,
            m_logMonitor, [this](const QString& modelId) {
                m_logMonitor->log(
                    QString::fromUtf8("数据集已导入: %1").arg(modelId));
            });

    // 双击图像 → 跳数据标注
    connect(m_datasetManagement, &DatasetManagement::openAnnotationRequested,
            this, [this](const QString& modelId, const QString& imagePath) {
                m_logMonitor->log(
                    QString::fromUtf8("跳转数据标注: %1 → %2")
                        .arg(modelId, imagePath));
                m_annotationManagement->setDatasetSource(m_datasetManagement);
                m_annotationManagement->openImage(modelId, imagePath);
                if (m_tabBar->isTabVisible(3)) {
                    m_tabBar->setCurrentIndex(3);
                    m_stackedWidget->setCurrentIndex(3);
                }
            });

    // 标注返回 → 切回数据集（同步标签类别变动）
    connect(m_annotationManagement, &AnnotationManagement::returnToGallery,
            this, [this]() {
                // 重新检测标签（labelme 可能创建了新类别）
                m_datasetManagement->detectLabelsFromAnnotations();
                if (m_tabBar->isTabVisible(2)) {
                    m_tabBar->setCurrentIndex(2);
                    m_stackedWidget->setCurrentIndex(2);
                }
            });

    // 数据集标签变动 → 同步到标注模块
    connect(m_datasetManagement, &DatasetManagement::labelsChanged,
            this, [this]() {
                m_annotationManagement->syncLabels();
            });

    // 标注模块请求进入 LabelMe 嵌入模式
    connect(m_annotationManagement, &AnnotationManagement::labelMeRequested,
            this, [this](const QString& imgDir, const QString& currentFile,
                         const QString& pythonExe, const QString& sdkPath,
                         const QStringList& labels, const QString& outputDir) {
                enterLabelMeMode(imgDir, currentFile, pythonExe, sdkPath,
                                 labels, outputDir);
            });

    m_logMonitor->log(QString::fromUtf8("深度学习模块已加载"));
}

// ── LabelMe 嵌入模式 ─────────────────────────────────────────

void DeepLearningWidget::enterLabelMeMode(
    const QString& imageDir, const QString& currentFile,
    const QString& pythonExe, const QString& sdkPath,
    const QStringList& labels, const QString& outputDir)
{
    if (m_labelmeEmbedded) return;

    // 清理残留进程（避免 QProcess: Destroyed while process is still running）
    if (m_labelmeProcess) {
        m_labelmeProcess->disconnect();
        if (m_labelmeProcess->state() != QProcess::NotRunning) {
            m_labelmeProcess->terminate();
            m_labelmeProcess->waitForFinished(3000);
            if (m_labelmeProcess->state() != QProcess::NotRunning)
                m_labelmeProcess->kill();
        }
        m_labelmeProcess->deleteLater();
        m_labelmeProcess = nullptr;
    }

    // 切换到 LabelMe 页面 → 显示加载动画
    m_topStack->setCurrentIndex(1);
    m_loadingOverlay->showOverlay();

    m_logMonitor->log(
        QString::fromUtf8("LabelMe 嵌入模式启动中..."));

    // 构建参数
    QStringList args;
    args << QDir::toNativeSeparators(
        QDir::cleanPath(sdkPath + "/labelme_embed.py"));
    args << imageDir;
    args << currentFile;
    if (!labels.isEmpty())
        args << "--labels" << labels.join(',');
    if (!outputDir.isEmpty() && QDir(outputDir).exists())
        args << "--output" << QDir::toNativeSeparators(outputDir);

    // 启动进程
    m_labelmeProcess = new QProcess(this);
    m_labelmeProcess->setWorkingDirectory(sdkPath);

    QObject::connect(m_labelmeProcess, &QProcess::readyReadStandardOutput,
        this, [this]() {
            QString out = QString::fromUtf8(
                m_labelmeProcess->readAllStandardOutput());
            QRegularExpression re("HWND:(\\d+)");
            auto match = re.match(out);
            if (match.hasMatch() && !m_labelmeWinId) {
                m_labelmeWinId = (WId)match.captured(1).toULongLong();
                embedLabelMeWindow();
            }
        });

    QObject::connect(m_labelmeProcess,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this](int exitCode, QProcess::ExitStatus) {
            m_logMonitor->log(
                QString::fromUtf8("LabelMe 进程已退出 (code=%1)")
                    .arg(exitCode));
            m_labelmeEmbedded = false;
            m_labelmeWinId = 0;
            // 尚未手动退出 → 自动恢复
            if (m_topStack->currentIndex() == 1 && !m_exitingLabelMe)
                exitLabelMeMode();
        });

    m_labelmeProcess->start(pythonExe, args);
    if (!m_labelmeProcess->waitForStarted(5000)) {
        m_logMonitor->log(QString::fromUtf8(
            "LabelMe 启动失败: %1").arg(pythonExe), LogMonitor::Error);
        exitLabelMeMode();
    }
}

void DeepLearningWidget::embedLabelMeWindow()
{
#ifdef Q_OS_WIN
    if (!m_labelmeWinId || !m_labelmeContainer) return;

    HWND hwnd = reinterpret_cast<HWND>(m_labelmeWinId);
    HWND parentHwnd = reinterpret_cast<HWND>(m_labelmeContainer->winId());

    // 1. 嵌入到容器窗口
    SetParent(hwnd, parentHwnd);

    // 2. 移除所有顶层窗口样式，设为子窗口
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME |
               WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU |
               WS_BORDER | WS_DLGFRAME);
    style |= WS_CHILD | WS_CLIPSIBLINGS;
    SetWindowLong(hwnd, GWL_STYLE, style);

    // 移除所有扩展样式
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE |
                 WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW |
                 WS_EX_APPWINDOW | WS_EX_STATICEDGE);
    exStyle |= WS_EX_NOPARENTNOTIFY;
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    // 3. 使容器支持子窗口裁剪
    LONG parentStyle = GetWindowLong(parentHwnd, GWL_STYLE);
    parentStyle |= WS_CLIPCHILDREN;
    SetWindowLong(parentHwnd, GWL_STYLE, parentStyle);

    // 4. 强制容器完成布局
    m_labelmeContainer->updateGeometry();
    QCoreApplication::processEvents();

    // 5. 以容器客户区尺寸定位嵌入窗口
    RECT cr;
    GetClientRect(parentHwnd, &cr);
    int cw = cr.right - cr.left;
    int ch = cr.bottom - cr.top;
    if (cw < 100) cw = 800;
    if (ch < 100) ch = 600;

    // 6. 应用样式变更 + 定位 + 显示
    SetWindowPos(hwnd, nullptr, 0, 0, cw, ch,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOW);

    m_labelmeEmbedded = true;
    m_labelmeContainer->installEventFilter(this);

    // 7. 延迟重复校准（PyQt 的 QMainWindow 内部布局是异步的）
    struct { int delay; bool forceShow; } calibrations[] = {
        { 50,  true  },
        { 150, true  },
        { 350, false },
        { 750, false },
    };
    for (const auto& cal : calibrations) {
        QTimer::singleShot(cal.delay, this, [this, forceShow = cal.forceShow]() {
            if (!m_labelmeEmbedded || !m_labelmeWinId || !m_labelmeContainer)
                return;
            HWND hwnd = reinterpret_cast<HWND>(m_labelmeWinId);
            HWND ph = reinterpret_cast<HWND>(m_labelmeContainer->winId());
            RECT rect;
            GetClientRect(ph, &rect);
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;
            if (w > 0 && h > 0) {
                UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
                if (forceShow) flags |= SWP_SHOWWINDOW;
                SetWindowPos(hwnd, nullptr, 0, 0, w, h, flags);
                // WM_SIZE 通知 PyQt 重新布局内部控件
                SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(w, h));
                InvalidateRect(hwnd, nullptr, TRUE);
                UpdateWindow(hwnd);
            }
        });
    }

    // 等最后一次校准 (750ms) 完成后再隐藏动画
    QTimer::singleShot(800, this, [this]() {
        if (m_labelmeEmbedded)
            m_loadingOverlay->hideOverlay();
    });

    m_logMonitor->log(QString::fromUtf8("LabelMe 已嵌入"));
#endif
}

void DeepLearningWidget::exitLabelMeMode()
{
    if (!m_labelmeEmbedded && m_topStack->currentIndex() == 0) return;

    // 防止重入
    if (m_exitingLabelMe) return;
    m_exitingLabelMe = true;

    // 1. 先隐藏 LabelMe 原生窗口（原生窗口在 Qt overlay 上层，不隐藏则遮罩不可见）
    if (m_labelmeWinId) {
        ShowWindow(reinterpret_cast<HWND>(m_labelmeWinId), SW_HIDE);
    }

    // 2. 显示退出动画（盖在 LabelMe 页上）
    m_loadingOverlay->setText(QString::fromUtf8("正在退出 LabelMe..."));
    m_loadingOverlay->showOverlay();

    m_labelmeEmbedded = false;
    m_labelmeWinId = 0;

    if (m_labelmeContainer)
        m_labelmeContainer->removeEventFilter(this);

    // 2. 异步终止进程（不阻塞事件循环）
    if (m_labelmeProcess && m_labelmeProcess->state() != QProcess::NotRunning) {

        m_labelmeProcess->disconnect(this);

        QObject::connect(m_labelmeProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                m_logMonitor->log(
                    QString::fromUtf8("LabelMe 进程已退出 (code=%1)")
                        .arg(exitCode));
                finishLabelMeCleanup();
            });

        m_labelmeProcess->terminate();

        // 3 秒超时强杀
        QTimer::singleShot(3000, this, [this]() {
            if (m_labelmeProcess && m_labelmeProcess->state() != QProcess::NotRunning)
                m_labelmeProcess->kill();
        });
    } else {
        finishLabelMeCleanup();
    }
}

void DeepLearningWidget::finishLabelMeCleanup()
{
    if (m_labelmeProcess) {
        m_labelmeProcess->deleteLater();
        m_labelmeProcess = nullptr;
    }

    // 3. 进程已死，切回标注页（此时标注页已就绪，切过去立即可见）
    m_topStack->setCurrentIndex(0);

    // 4. 重新检测标签类别（labelme 可能创建了新类别 → labelsChanged 会同步到标注模块）
    m_datasetManagement->detectLabelsFromAnnotations();

    // 5. 短暂保留遮罩让用户看到"退出"反馈，然后去掉
    QTimer::singleShot(300, this, [this]() {
        m_loadingOverlay->hideOverlay();
        m_loadingOverlay->setText(QString::fromUtf8("正在启动 LabelMe..."));
        m_exitingLabelMe = false;
    });

    m_logMonitor->log(QString::fromUtf8("已退出 LabelMe 嵌入模式"));
}

bool DeepLearningWidget::eventFilter(QObject* obj, QEvent* event)
{
#ifdef Q_OS_WIN
    if (obj == m_labelmeContainer &&
        event->type() == QEvent::Resize && m_labelmeWinId) {
        QResizeEvent* re = static_cast<QResizeEvent*>(event);
        HWND hwnd = reinterpret_cast<HWND>(m_labelmeWinId);
        int w = re->size().width();
        int h = re->size().height();
        if (w > 0 && h > 0) {
            SetWindowPos(hwnd, nullptr, 0, 0, w, h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(w, h));
        }
    }
#endif
    return QWidget::eventFilter(obj, event);
}

// ── 首次显示时按比例分配日志面板高度 ─────────────────────────

void DeepLearningWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_shown) return;
    m_shown = true;
    if (auto* s = findChild<QSplitter*>()) {
        int h = s->height();
        if (h > 0 && s->count() == 2)
            s->setSizes({ h * 3 / 4, h / 4 });
    }
}

void DeepLearningWidget::onTabChanged(int index)
{
    if (index >= 0 && index < m_stackedWidget->count())
        m_stackedWidget->setCurrentIndex(index);
}

void DeepLearningWidget::applyPermissions()
{
    for (int i = 0; i < s_permissionKeys.size(); ++i) {
        bool canRead = m_guard->canRead(s_permissionKeys[i]);
        m_tabBar->setTabVisible(i, canRead);
        if (!canRead && m_tabBar->currentIndex() == i) {
            for (int j = 0; j < m_tabBar->count(); ++j) {
                if (m_tabBar->isTabVisible(j)) {
                    m_tabBar->setCurrentIndex(j);
                    break;
                }
            }
        }
    }
}

QString DeepLearningWidget::currentProjectName() const
{
    return m_projectManagement
        ? m_projectManagement->currentProjectName() : QString();
}

void DeepLearningWidget::onProjectOpened(const QString& name,
                                          const QString& path)
{
    m_logMonitor->log(
        QString::fromUtf8("项目已打开: %1 (%2)").arg(name, path));
    emit dlProjectChanged(name);
    if (m_tabBar->isTabVisible(1)) {
        m_tabBar->setCurrentIndex(1);
        m_stackedWidget->setCurrentIndex(1);
    }
}

void DeepLearningWidget::onProjectClosed()
{
    m_logMonitor->log(QString::fromUtf8("项目已关闭"));
    emit dlProjectChanged(QString());
}
