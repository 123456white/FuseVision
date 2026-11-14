#ifndef ERASERTOOL_H
#define ERASERTOOL_H

#include "AnnotationTool.h"
#include <QPainterPath>

class EraserTool : public AnnotationTool
{
    Q_OBJECT
public:
    explicit EraserTool(QObject* parent = nullptr);

    QString name() const override { return QString::fromUtf8("橡皮擦"); }
    QString icon() const override { return QString::fromUtf8("🧹"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_E); }
    bool showBrushSize() const override { return true; }

    void mousePress(QPointF imagePos, Qt::MouseButton button) override;
    void mouseMove(QPointF imagePos) override;
    void mouseRelease(QPointF imagePos, Qt::MouseButton button) override;

    QPainterPath previewPath() const override;
    QColor previewColor() const override { return QColor(255, 0, 0, 80); }
    int previewPenWidth() const override { return m_ctx.brushSize; }

private:
    QPainterPath m_erasePath;
    void endErase();
};

#endif
