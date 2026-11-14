#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include "AnnotationTool.h"
#include <QPainterPath>

class BrushTool : public AnnotationTool
{
    Q_OBJECT
public:
    explicit BrushTool(QObject* parent = nullptr);

    QString name() const override { return QString::fromUtf8("画笔"); }
    QString icon() const override { return QString::fromUtf8("🖌"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_B); }
    bool showBrushSize() const override { return true; }

    void mousePress(QPointF imagePos, Qt::MouseButton button) override;
    void mouseMove(QPointF imagePos) override;
    void mouseRelease(QPointF imagePos, Qt::MouseButton button) override;

    QPainterPath previewPath() const override;
    QColor previewColor() const override;
    int previewPenWidth() const override { return m_ctx.brushSize; }

private:
    QPainterPath m_brushPath;
    void endStroke();
};

#endif
