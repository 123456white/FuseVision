/** @file
 * @brief RectTool 的公共声明。
 * @ingroup dl_tools
 */
#ifndef RECTTOOL_H
#define RECTTOOL_H

#include "AnnotationTool.h"
#include <QRectF>

/** @brief 支持拖放及既有两次点击交互的连续矩形标注。
 * @ingroup dl_tools
 */
class RectTool : public AnnotationTool
{
    Q_OBJECT
public:
    explicit RectTool(QObject* parent = nullptr);

    QString name() const override { return QString::fromUtf8("矩形"); }
    QString icon() const override { return QString::fromUtf8("▭"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_R); }

    void mousePress(QPointF imagePos, Qt::MouseButton button) override;
    void mouseMove(QPointF imagePos) override;
    void mouseRelease(QPointF imagePos, Qt::MouseButton button) override;
    void keyPress(int key, Qt::KeyboardModifiers mods) override;

    QPainterPath previewPath() const override;
    QColor previewColor() const override { return QColor(100, 255, 100); }
    int previewPenWidth() const override { return 2; }

    void cancelDrawing() override;

private:
    QPointF m_rectStart;
    QPointF m_lastMousePos;
    QRectF  m_currentRect;
    bool    m_placedStart = false;  // 已放置起点
    bool m_dragCandidate = false;

    void startRect(QPointF p);
    void endRect(QPointF p);
};

#endif
