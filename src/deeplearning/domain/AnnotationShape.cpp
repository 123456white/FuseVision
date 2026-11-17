#include "AnnotationShape.h"

QRectF AnnotationShape::boundingRect() const
{
    switch (type) {
    case BrushMask:
        return QRectF(0, 0, maskLayer.width(), maskLayer.height());
    case PolygonShape:
        return polygon.boundingRect();
    case RectShape:
    case RotatedRectShape:
        return bbox;
    }
    return {};
}

bool AnnotationShape::isEmpty() const
{
    switch (type) {
    case BrushMask:
        if (maskLayer.isNull()) return true;
        for (int y = 0; y < maskLayer.height(); y++) {
            const uchar* line = maskLayer.constScanLine(y);
            for (int x = 0; x < maskLayer.width(); x++)
                if (line[x] > 0) return false;
        }
        return true;
    case PolygonShape:
        return polygon.size() < 3;
    case RectShape:
    case RotatedRectShape:
        return bbox.width() < 3 || bbox.height() < 3;
    }
    return true;
}

