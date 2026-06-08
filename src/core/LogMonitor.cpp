#include "LogMonitor.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QDateTime>

// ── 构造 ──────────────────────────────────────────────────────

LogMonitor::LogMonitor(QWidget* parent)
    : QFrame(parent), m_expanded(true)
{
    initUI();
}

// ── UI 构建 ───────────────────────────────────────────────────

void LogMonitor::initUI()
{
    setObjectName("logMonitor");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 0, 8, 8);
    mainLayout->setSpacing(0);

    // ── 标题栏 ──
    QFrame* headerBar = new QFrame;
    headerBar->setObjectName("logMonitorHeader");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(4, 4, 4, 4);

    m_titleLabel = new QLabel("日志监控");
    m_titleLabel->setObjectName("sectionTitle");

    m_toggleBtn = new QPushButton("▲");
    m_toggleBtn->setObjectName("logToggleBtn");
    m_toggleBtn->setFixedSize(24, 24);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setToolTip("折叠日志面板");

    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(m_toggleBtn);

    mainLayout->addWidget(headerBar);

    // ── 日志视图 ──
    m_logView = new QTextEdit;
    m_logView->setObjectName("logView");
    m_logView->setReadOnly(true);
    m_logView->document()->setMaximumBlockCount(1000);  // 最多保留 1000 行
    mainLayout->addWidget(m_logView, 1);

    // ── 信号 ──
    connect(m_toggleBtn, &QPushButton::clicked, this, &LogMonitor::toggle);
}

// ── 折叠/展开 ─────────────────────────────────────────────────

void LogMonitor::toggle()
{
    m_expanded = !m_expanded;
    m_logView->setVisible(m_expanded);
    m_toggleBtn->setText(m_expanded ? "▲" : "▼");
    m_toggleBtn->setToolTip(m_expanded ? "折叠日志面板" : "展开日志面板");

    // 直接操控父 QSplitter 的尺寸分配
    auto* splitter = qobject_cast<QSplitter*>(parentWidget());
    if (splitter && splitter->count() == 2) {
        if (m_expanded) {
            int total = splitter->orientation() == Qt::Vertical
                ? splitter->height() : splitter->width();
            splitter->setSizes({ total * 3 / 4, total / 4 });
        } else {
            // 折叠态：日志面板仅保留标题栏高度 (~28px)
            int headerH = m_titleLabel->sizeHint().height() + 10;
            int total = splitter->orientation() == Qt::Vertical
                ? splitter->height() : splitter->width();
            splitter->setSizes({ total - headerH, headerH });
        }
    }
}

void LogMonitor::expand()
{
    if (!m_expanded) toggle();
}

void LogMonitor::collapse()
{
    if (m_expanded) toggle();
}

// ── 尺寸提示（已被 toggle() 中的 setSizes 覆盖，保留作为 fallback）

QSize LogMonitor::sizeHint() const
{
    if (m_expanded)
        return QFrame::sizeHint();
    int h = m_titleLabel ? m_titleLabel->sizeHint().height() + 10 : 30;
    return QSize(200, h);
}

QSize LogMonitor::minimumSizeHint() const
{
    return sizeHint();
}

// ── 日志输出 ─────────────────────────────────────────────────

void LogMonitor::log(const QString& message, Level level)
{
    QString timestamp = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]");
    const auto& p = ThemeManager::instance().palette();

    QString levelStr;
    QString color;
    switch (level) {
    case Warning: levelStr = "[WARN]";  color = p.warning; break;
    case Error:   levelStr = "[ERROR]"; color = p.danger;  break;
    default:      levelStr = "[INFO]";  color = p.success;  break;
    }

    QString html = QString("<span style='color:%1'>%2</span> "
                           "<span style='color:%3'>%4</span> "
                           "%5")
                       .arg(p.textMuted, timestamp, color, levelStr, message.toHtmlEscaped());

    m_logView->append(html);
}

void LogMonitor::clear()
{
    m_logView->clear();
}
