/** @file
 * @brief AnnotationShape 的公共声明。
 * @ingroup dl_domain
 */
#pragma once
#include <QImage>
#include <QPolygonF>
#include <QRectF>
#include <QUuid>
#include <QDateTime>
#include <QString>

/** @brief 单个标注实例的几何、类别、掩码及原生身份元数据。
 * @ingroup dl_domain
 */
struct AnnotationShape {
    enum ShapeType { BrushMask = 0, PolygonShape, RectShape, RotatedRectShape };
    ShapeType type = BrushMask;
    int       labelId = 0;
    QPolygonF polygon;
    QRectF    bbox;
    double    rotation  = 0.0;
    QPointF   rotCenter;
    QImage    maskLayer;
    bool      visible = true;

    // v3.0 新增
    QString   comment;
    QDateTime createdTime;
    QDateTime modifiedTime;
    QUuid     uuid = QUuid::createUuid();

    bool operator==(const AnnotationShape& o) const {
        return type == o.type && labelId == o.labelId
            && polygon == o.polygon && bbox == o.bbox
            && rotation == o.rotation && rotCenter == o.rotCenter
            && visible == o.visible
            && comment == o.comment && uuid == o.uuid
            && maskLayer == o.maskLayer;
    }
    bool operator!=(const AnnotationShape& o) const { return !(*this == o); }

    // 便捷方法
    QRectF boundingRect() const;
    bool isEmpty() const;
};
