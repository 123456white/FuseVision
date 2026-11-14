#ifndef LOADINGOVERLAY_H
#define LOADINGOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>

// =============================================================================
// LoadingOverlay — 通用的加载动画覆盖层
// =============================================================================
// 用法：
//   auto* overlay = new LoadingOverlay(targetWidget, "正在加载...");
//   overlay->showOverlay();
//   // ... 执行耗时操作 ...
//   overlay->hideOverlay();
//
// 覆盖层自动填充父控件大小，半透明深色背景 + 居中进度条 + 文字提示。
// =============================================================================

class LoadingOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent, const QString& text = QStringLiteral("正在加载..."));
    ~LoadingOverlay() override;

    void setText(const QString& text);
    void showOverlay();
    void hideOverlay();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void syncGeometry();

    QLabel*      m_label = nullptr;
    QProgressBar* m_bar  = nullptr;
};

#endif // LOADINGOVERLAY_H
