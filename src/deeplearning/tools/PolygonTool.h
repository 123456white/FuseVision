/** @file
 * @brief PolygonTool 的公共声明。
 * @ingroup dl_tools
 */
#ifndef POLYGONTOOL_H
#define POLYGONTOOL_H

#include "AnnotationTool.h"
#include <QPolygonF>

/** @brief 维护绘制预览并在确认后提交多边形实例。
 * @ingroup dl_tools
 */
class PolygonTool : public AnnotationTool
{
    Q_OBJECT
public:
    explicit PolygonTool(QObject* parent = nullptr);

    QString name() const override { return QString::fromUtf8("多边形"); }
    QString icon() const override { return QString::fromUtf8("⬠"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_P); }

    void mousePress(QPointF imagePos, Qt::MouseButton button) override;
    void mouseMove(QPointF imagePos) override;
    void mouseDoubleClick(QPointF imagePos) override;
    void keyPress(int key, Qt::KeyboardModifiers mods) override;

    QPainterPath previewPath() const override;
    QColor previewColor() const override { return QColor(100, 100, 255); }

    void cancelDrawing() override;

private:
    QPolygonF m_currentPolygon;
    QPointF m_lastMousePos;

    void startPolygon(QPointF p);
    void addVertex(QPointF p);
    void closePolygon();
};

#endif
