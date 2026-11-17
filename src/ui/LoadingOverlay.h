/** @file
 * @brief LoadingOverlay 的公共声明。
 * @ingroup ui_common
 */
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

/** @brief GUI 线程使用的加载遮罩，由所属 Widget 管理生命周期。
 * @ingroup ui_common
 */
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
