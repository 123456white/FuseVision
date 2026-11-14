#include "LoadingOverlay.h"
#include <QVBoxLayout>
#include <QResizeEvent>

LoadingOverlay::LoadingOverlay(QWidget* parent, const QString& text)
    : QWidget(parent)
{
    setObjectName("loadingOverlay");
    setStyleSheet(
        "QWidget#loadingOverlay {"
        "  background: rgba(30, 30, 30, 220);"
        "  border-radius: 8px; }");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    m_label = new QLabel(text);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    m_label->setStyleSheet(
        "color: #e0e0e0; font-size: 16px; font-weight: bold;"
        "background: transparent;");
    layout->addWidget(m_label);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 0);          // indeterminate
    m_bar->setFixedHeight(6);
    m_bar->setMinimumWidth(200);
    m_bar->setMaximumWidth(300);
    m_bar->setTextVisible(false);
    m_bar->setStyleSheet(
        "QProgressBar {"
        "  background: #555; border: none; border-radius: 3px; }"
        "QProgressBar::chunk {"
        "  background: #3498db; border-radius: 3px; }");
    layout->addWidget(m_bar, 0, Qt::AlignCenter);

    setVisible(false);

    // 跟踪父控件大小变化
    if (parent) {
        parent->installEventFilter(this);
        syncGeometry();
    }
}

LoadingOverlay::~LoadingOverlay()
{
    if (parentWidget())
        parentWidget()->removeEventFilter(this);
}

void LoadingOverlay::setText(const QString& text)
{
    m_label->setText(text);
}

void LoadingOverlay::showOverlay()
{
    syncGeometry();
    raise();
    setVisible(true);
}

void LoadingOverlay::hideOverlay()
{
    setVisible(false);
}

void LoadingOverlay::syncGeometry()
{
    if (parentWidget())
        setGeometry(parentWidget()->rect());
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        syncGeometry();
    }
    return QWidget::eventFilter(obj, event);
}
