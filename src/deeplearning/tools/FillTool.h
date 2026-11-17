/** @file
 * @brief FillTool 的公共声明。
 * @ingroup dl_tools
 */
#ifndef FILLTOOL_H
#define FILLTOOL_H

#include "AnnotationTool.h"
#include <QStack>
#include <QPair>

/** @brief 在有效图像区域生成填充掩码并合并已有类别像素。
 * @ingroup dl_tools
 */
class FillTool : public AnnotationTool
{
    Q_OBJECT
public:
    explicit FillTool(QObject* parent = nullptr);

    QString name() const override { return QString::fromUtf8("填充"); }
    QString icon() const override { return QString::fromUtf8("🪣"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::Key_F); }
    Qt::CursorShape cursor() const override { return Qt::PointingHandCursor; }

    void mousePress(QPointF imagePos, Qt::MouseButton button) override;

private:
    void floodFill(int seedX, int seedY, int fillLabelId);
    QImage buildOccupiedMask(const QList<AnnotationShape>& shapes) const;
};

#endif
