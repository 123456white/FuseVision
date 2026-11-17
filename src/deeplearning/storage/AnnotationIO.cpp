#include "AnnotationIO.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTextStream>
#include <QtMath>

// =============================================================================
// Format Detection
// =============================================================================

AnnotationIO::Format AnnotationIO::detectFormat(const QString& annDir, const QString& baseName)
{
    if (QFile::exists(annDir + "/" + baseName + ".png"))
        return PNGMask;
    if (QFile::exists(annDir + "/" + baseName + ".json"))
        return COCO;  // Could be COCO or labelme — check content later
    if (QFile::exists(annDir + "/" + baseName + ".txt"))
        return YOLO;
    if (QFile::exists(annDir + "/" + baseName + ".xml"))
        return PascalVOC;
    return Unknown;
}

AnnotationIO::AnnotationIO(QObject* parent) : QObject(parent) {}

// =============================================================================
// PNG Mask Export
// =============================================================================

bool AnnotationIO::savePNGMask(const QString& path, const QImage& mask)
{
    return mask.save(path, "PNG");
}

// =============================================================================
// COCO JSON Export
// =============================================================================

bool AnnotationIO::saveCOCO(const QString& path,
                             const QList<AnnotationShape>& shapes,
                             int imageId, const QString& fileName,
                             QSize imageSize, int labelOffset)
{
    Q_UNUSED(imageSize);
    QJsonArray annotations;
    int annId = 1;

    for (const auto& s : shapes) {
        if (!s.visible) continue;
        QJsonObject a;
        a["id"] = annId++;
        a["image_id"] = imageId;
        a["category_id"] = s.labelId + labelOffset;

        if (s.type == AnnotationShape::RectShape) {
            QJsonArray bb;
            bb.append(s.bbox.x()); bb.append(s.bbox.y());
            bb.append(s.bbox.width()); bb.append(s.bbox.height());
            a["bbox"] = bb;
        } else if (s.type == AnnotationShape::RotatedRectShape) {
            QJsonArray bb;
            bb.append(s.bbox.x()); bb.append(s.bbox.y());
            bb.append(s.bbox.width()); bb.append(s.bbox.height());
            a["bbox"] = bb;
            a["rotation"] = s.rotation;
            QJsonArray rc;
            rc.append(s.rotCenter.x()); rc.append(s.rotCenter.y());
            a["rot_center"] = rc;
        } else if (s.type == AnnotationShape::PolygonShape) {
            QRectF br = s.polygon.boundingRect();
            QJsonArray bb;
            bb.append(br.x()); bb.append(br.y());
            bb.append(br.width()); bb.append(br.height());
            a["bbox"] = bb;
            QJsonArray seg;
            for (const QPointF& pt : s.polygon) {
                seg.append(pt.x()); seg.append(pt.y());
            }
            a["segmentation"] = seg;
        }
        annotations.append(a);
    }

    QJsonObject root;
    QJsonObject imgObj;
    imgObj["id"] = imageId;
    imgObj["file_name"] = fileName;
    root["images"] = QJsonArray{ imgObj };
    root["annotations"] = annotations;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

// =============================================================================
// YOLO Export
// =============================================================================

bool AnnotationIO::saveYOLO(const QString& path,
                             const QList<AnnotationShape>& shapes,
                             QSize imageSize, int labelOffset)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out.setRealNumberPrecision(6);

    for (const auto& s : shapes) {
        if (!s.visible) continue;
        QRectF box = s.type == AnnotationShape::PolygonShape
            ? s.polygon.boundingRect() : s.bbox;

        double cx = (box.x() + box.width() / 2.0) / imageSize.width();
        double cy = (box.y() + box.height() / 2.0) / imageSize.height();
        double w = box.width() / imageSize.width();
        double h = box.height() / imageSize.height();

        // Clamp
        cx = qBound(0.0, cx, 1.0);
        cy = qBound(0.0, cy, 1.0);
        w = qBound(0.0, w, 1.0);
        h = qBound(0.0, h, 1.0);

        int classId = s.labelId + labelOffset - 1;  // YOLO: 0-indexed
        out << classId << " " << cx << " " << cy << " " << w << " " << h << "\n";
    }

    f.close();
    return true;
}

// =============================================================================
// Pascal VOC Export
// =============================================================================

bool AnnotationIO::savePascalVOC(const QString& path,
                                  const QList<AnnotationShape>& shapes,
                                  QSize imageSize,
                                  const QStringList& labelNames,
                                  int labelOffset)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("annotation");

    xml.writeTextElement("folder", QFileInfo(path).dir().dirName());
    xml.writeTextElement("filename", QFileInfo(path).completeBaseName());

    xml.writeStartElement("size");
    xml.writeTextElement("width", QString::number(imageSize.width()));
    xml.writeTextElement("height", QString::number(imageSize.height()));
    xml.writeTextElement("depth", "3");
    xml.writeEndElement();  // size

    for (const auto& s : shapes) {
        if (!s.visible) continue;
        QRectF box = s.type == AnnotationShape::PolygonShape
            ? s.polygon.boundingRect() : s.bbox;

        QString name = (s.labelId >= 0 && s.labelId < labelNames.size())
            ? labelNames[s.labelId] : QString::fromUtf8("unknown");

        xml.writeStartElement("object");
        xml.writeTextElement("name", name);
        xml.writeTextElement("pose", "Unspecified");
        xml.writeTextElement("truncated", "0");
        xml.writeTextElement("difficult", "0");

        xml.writeStartElement("bndbox");
        xml.writeTextElement("xmin", QString::number((int)box.left()));
        xml.writeTextElement("ymin", QString::number((int)box.top()));
        xml.writeTextElement("xmax", QString::number((int)(box.left() + box.width())));
        xml.writeTextElement("ymax", QString::number((int)(box.top() + box.height())));
        xml.writeEndElement();  // bndbox
        xml.writeEndElement();  // object
    }

    xml.writeEndElement();  // annotation
    xml.writeEndDocument();
    f.close();
    return true;
}

// =============================================================================
// PNG Mask Import
// =============================================================================

QList<AnnotationShape> AnnotationIO::loadPNGMask(const QImage& mask, int labelOffset)
{
    QList<AnnotationShape> result;
    if (mask.isNull()) return result;

    QImage gray = mask.convertToFormat(QImage::Format_Grayscale8);
    QMap<int, QImage> layers;

    for (int y = 0; y < gray.height(); y++) {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); x++) {
            if (line[x] > 0) {
                int lid = line[x] - labelOffset;
                if (!layers.contains(lid)) {
                    layers[lid] = QImage(mask.size(), QImage::Format_Grayscale8);
                    layers[lid].fill(0);
                }
                layers[lid].setPixel(x, y, 255);
            }
        }
    }

    for (auto it = layers.begin(); it != layers.end(); ++it) {
        AnnotationShape s;
        s.type = AnnotationShape::BrushMask;
        s.labelId = it.key();
        s.maskLayer = it.value();
        result.append(s);
    }
    return result;
}

// =============================================================================
// COCO Import
// =============================================================================

QList<AnnotationShape> AnnotationIO::loadCOCO(const QString& jsonPath, int labelOffset)
{
    QList<AnnotationShape> result;
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return result;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    for (const QJsonValue& av : doc.object()["annotations"].toArray()) {
        QJsonObject a = av.toObject();
        AnnotationShape s;
        s.labelId = a["category_id"].toInt() - labelOffset;

        if (a.contains("segmentation")) {
            s.type = AnnotationShape::PolygonShape;
            QJsonArray seg = a["segmentation"].toArray();
            for (int i = 0; i + 1 < seg.size(); i += 2)
                s.polygon << QPointF(seg[i].toDouble(), seg[i+1].toDouble());
        } else if (a.contains("bbox")) {
            QJsonArray bb = a["bbox"].toArray();
            if (a.contains("rotation")) {
                s.type = AnnotationShape::RotatedRectShape;
                s.rotation = a["rotation"].toDouble();
                QJsonArray rc = a["rot_center"].toArray();
                if (rc.size() >= 2)
                    s.rotCenter = QPointF(rc[0].toDouble(), rc[1].toDouble());
            } else {
                s.type = AnnotationShape::RectShape;
            }
            s.bbox = QRectF(bb[0].toDouble(), bb[1].toDouble(),
                            bb[2].toDouble(), bb[3].toDouble());
        }
        result.append(s);
    }
    return result;
}

// =============================================================================
// YOLO Import
// =============================================================================

QList<AnnotationShape> AnnotationIO::loadYOLO(const QString& txtPath,
                                               QSize imageSize,
                                               const QStringList& labelNames)
{
    Q_UNUSED(labelNames);
    QList<AnnotationShape> result;
    QFile f(txtPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 5) continue;

        int classId = parts[0].toInt();        // YOLO: 0-indexed
        double cx = parts[1].toDouble();
        double cy = parts[2].toDouble();
        double w = parts[3].toDouble();
        double h = parts[4].toDouble();

        double x = (cx - w / 2.0) * imageSize.width();
        double y = (cy - h / 2.0) * imageSize.height();
        double bw = w * imageSize.width();
        double bh = h * imageSize.height();

        AnnotationShape s;
        s.type = AnnotationShape::RectShape;
        s.labelId = classId;  // 0-indexed already
        s.bbox = QRectF(x, y, bw, bh);
        result.append(s);
    }
    f.close();
    return result;
}

// =============================================================================
// Pascal VOC Import
// =============================================================================

QList<AnnotationShape> AnnotationIO::loadPascalVOC(const QString& xmlPath,
                                                    const QStringList& labelNames)
{
    QList<AnnotationShape> result;
    QFile f(xmlPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;

    QXmlStreamReader xml(&f);
    QString currentName;
    double xmin = 0, ymin = 0, xmax = 0, ymax = 0;
    bool inBndBox = false;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("object"))
                currentName.clear();
            else if (xml.name() == QStringLiteral("name"))
                currentName = xml.readElementText();
            else if (xml.name() == QStringLiteral("bndbox"))
                inBndBox = true;
            else if (inBndBox) {
                if (xml.name() == QStringLiteral("xmin"))
                    xmin = xml.readElementText().toDouble();
                else if (xml.name() == QStringLiteral("ymin"))
                    ymin = xml.readElementText().toDouble();
                else if (xml.name() == QStringLiteral("xmax"))
                    xmax = xml.readElementText().toDouble();
                else if (xml.name() == QStringLiteral("ymax"))
                    ymax = xml.readElementText().toDouble();
            }
        } else if (xml.isEndElement()) {
            if (xml.name() == QStringLiteral("bndbox"))
                inBndBox = false;
            else if (xml.name() == QStringLiteral("object") && !currentName.isEmpty()) {
                int labelId = labelNames.indexOf(currentName);
                if (labelId < 0) labelId = 0;  // fallback

                AnnotationShape s;
                s.type = AnnotationShape::RectShape;
                s.labelId = labelId;
                s.bbox = QRectF(xmin, ymin, xmax - xmin, ymax - ymin);
                result.append(s);
            }
        }
    }

    f.close();
    return result;
}

// =============================================================================
// Labelme Import
// =============================================================================

QList<AnnotationShape> AnnotationIO::loadLabelme(const QString& jsonPath,
                                                  const QStringList& labelNames)
{
    QList<AnnotationShape> result;
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return result;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    for (const QJsonValue& sv : doc.object()["shapes"].toArray()) {
        QJsonObject shape = sv.toObject();
        QString label = shape["label"].toString();

        int labelId = labelNames.indexOf(label);
        if (labelId < 0) {
            // Try numeric
            bool ok;
            labelId = label.toInt(&ok);
            if (!ok) labelId = 0;
        }

        QString shapeType = shape["shape_type"].toString();
        QJsonArray points = shape["points"].toArray();

        AnnotationShape s;
        s.labelId = labelId;

        if (shapeType == "rectangle" && points.size() >= 2) {
            s.type = AnnotationShape::RectShape;
            QJsonArray p0 = points[0].toArray();
            QJsonArray p1 = points[1].toArray();
            double x = qMin(p0[0].toDouble(), p1[0].toDouble());
            double y = qMin(p0[1].toDouble(), p1[1].toDouble());
            double w = qAbs(p1[0].toDouble() - p0[0].toDouble());
            double h = qAbs(p1[1].toDouble() - p0[1].toDouble());
            s.bbox = QRectF(x, y, w, h);
        } else if (shapeType == "polygon" || shapeType == "linestrip") {
            s.type = AnnotationShape::PolygonShape;
            for (const QJsonValue& pt : points) {
                QJsonArray p = pt.toArray();
                if (p.size() >= 2)
                    s.polygon << QPointF(p[0].toDouble(), p[1].toDouble());
            }
        } else {
            continue;  // unsupported shape type
        }
        result.append(s);
    }
    return result;
}

// =============================================================================
// Batch Conversion
// =============================================================================

int AnnotationIO::convertAll(const QString& srcDir, Format srcFmt,
                              const QString& dstDir, Format dstFmt,
                              const QStringList& labelNames, int labelOffset)
{
    QDir().mkpath(dstDir);
    int count = 0;

    QStringList filters;
    switch (srcFmt) {
    case PNGMask:   filters << "*.png"; break;
    case COCO: case Labelme: filters << "*.json"; break;
    case YOLO:     filters << "*.txt"; break;
    case PascalVOC: filters << "*.xml"; break;
    default: return 0;
    }

    QDir dir(srcDir);
    for (const QString& file : dir.entryList(filters, QDir::Files)) {
        QString base = QFileInfo(file).completeBaseName();
        QList<AnnotationShape> shapes;

        switch (srcFmt) {
        case PNGMask:
            shapes = loadPNGMask(QImage(srcDir + "/" + file), labelOffset);
            break;
        case COCO:
            shapes = loadCOCO(srcDir + "/" + file, labelOffset);
            break;
        case YOLO:
            shapes = loadYOLO(srcDir + "/" + file, QSize(), labelNames);
            break;
        case PascalVOC:
            shapes = loadPascalVOC(srcDir + "/" + file, labelNames);
            break;
        case Labelme:
            shapes = loadLabelme(srcDir + "/" + file, labelNames);
            break;
        default: break;
        }

        if (shapes.isEmpty()) continue;

        bool ok = false;
        switch (dstFmt) {
        case PNGMask: {
            // Need mask compilation — caller handles this
            break;
        }
        case COCO:
            ok = saveCOCO(dstDir + "/" + base + ".json",
                          shapes, count + 1, base + ".jpg",
                          QSize(1920, 1080), labelOffset);
            break;
        case YOLO:
            ok = saveYOLO(dstDir + "/" + base + ".txt",
                          shapes, QSize(1920, 1080), labelOffset);
            break;
        case PascalVOC:
            ok = savePascalVOC(dstDir + "/" + base + ".xml",
                               shapes, QSize(1920, 1080), labelNames, labelOffset);
            break;
        default: break;
        }

        if (ok) count++;
    }
    return count;
}
