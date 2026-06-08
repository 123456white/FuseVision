#ifndef LOGMONITOR_H
#define LOGMONITOR_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

// =============================================================================
// LogMonitor — 可折叠日志监控面板
// =============================================================================
//
// 布局：
//   ┌──────────────────────────────────────────┐
//   │ 日志监控                              ▲   │ ← 标题栏 + 折叠按钮
//   ├──────────────────────────────────────────┤
//   │ [2026-06-08 10:30:12] 相机采集已启动      │
//   │ [2026-06-08 10:30:15] 图像处理完成        │ ← QTextEdit (只读)
//   │ ...                                      │
//   └──────────────────────────────────────────┘
//
// 用法：
//   LogMonitor* monitor = new LogMonitor(parent);
//   monitor->log("相机采集已启动");
//   monitor->log("图像处理完成", LogMonitor::Warning);
// =============================================================================

class LogMonitor : public QFrame
{
    Q_OBJECT
public:
    enum Level { Info, Warning, Error };

    explicit LogMonitor(QWidget* parent = nullptr);

    /// 添加一条日志（自动带时间戳和级别标记）
    void log(const QString& message, Level level = Info);

    /// 清空所有日志
    void clear();

    /// 是否处于展开状态
    bool isExpanded() const { return m_expanded; }

public slots:
    void toggle();   // 展开/折叠切换
    void expand();   // 展开
    void collapse(); // 折叠

protected:
    QSize sizeHint()        const override;  // 折叠时仅报标题栏高度
    QSize minimumSizeHint() const override;

private:
    void initUI();

    QLabel*      m_titleLabel   = nullptr;
    QPushButton* m_toggleBtn    = nullptr;
    QTextEdit*   m_logView      = nullptr;
    bool         m_expanded     = true;
};

#endif // LOGMONITOR_H
