#ifndef RECTTOOL_H
#define RECTTOOL_H

#include "AnnotationTool.h"
#include <QRectF>

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
    void keyPress(int key, Qt::KeyboardModifiers mods) override;

    QPainterPath previewPath() const override;
    QColor previewColor() const override { return QColor(100, 255, 100); }
    int previewPenWidth() const override { return 2; }

    void cancelDrawing() override;

private:
    QPointF m_rectStart;
    QRectF  m_currentRect;
    bool    m_placedStart = false;  // 已放置起点

    void startRect(QPointF p);
    void endRect(QPointF p);
};

#endif
