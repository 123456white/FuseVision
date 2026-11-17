#include "AnnotationStore.h"
#include "deeplearning/render/AnnotationRender.h"
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QPainter>
#include <cmath>

namespace {
constexpr auto metadataKey = "FuseVision.Annotation.v1";
QJsonArray point(QPointF p) { return {p.x(), p.y()}; }
QJsonArray box(QRectF r) { return {r.x(),r.y(),r.width(),r.height()}; }
QByteArray jsonDigest(const QJsonArray& annotations) {
    return QCryptographicHash::hash(QJsonDocument(annotations).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex();
}
bool samePixels(const QImage& a,const QImage& b) {
    if(a.size()!=b.size()) return false;
    const auto x=a.convertToFormat(QImage::Format_Grayscale8),y=b.convertToFormat(QImage::Format_Grayscale8);
    for(int row=0;row<x.height();++row) if(memcmp(x.constScanLine(row),y.constScanLine(row),x.width())!=0) return false;
    return true;
}
QJsonArray encodeShapes(const QList<AnnotationShape>& shapes) {
    QJsonArray result;
    for (const auto& s : shapes) {
        QJsonArray polygon;
        for (auto p : s.polygon) { polygon.append(p.x()); polygon.append(p.y()); }
        QJsonObject o{{"type",int(s.type)}, {"label_id",s.labelId}, {"visible",s.visible},
            {"bbox",box(s.bbox)}, {"polygon",polygon}, {"rotation",s.rotation},
            {"rot_center",point(s.rotCenter)}, {"uuid",s.uuid.toString()}, {"comment",s.comment}};
        if (s.type == AnnotationShape::BrushMask) {
            QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly);
            if (!s.maskLayer.save(&buffer,"PNG")) return {};
            o["mask_png"] = QString::fromLatin1(png.toBase64());
        }
        result.append(o);
    }
    return result;
}
bool valid(const QList<AnnotationShape>& shapes, QSize size, int categories, QString& error) {
    for (const auto& s : shapes) {
        bool ok = s.labelId >= 0 && s.labelId < categories;
        if (s.type == AnnotationShape::BrushMask)
            ok &= !s.maskLayer.isNull() && s.maskLayer.size() == size;
        else if (s.type == AnnotationShape::PolygonShape) {
            ok &= s.polygon.size() >= 3;
            for (auto p : s.polygon) ok &= std::isfinite(p.x()) && std::isfinite(p.y());
        } else {
            ok &= (s.type == AnnotationShape::RectShape || s.type == AnnotationShape::RotatedRectShape)
                && std::isfinite(s.bbox.x()) && std::isfinite(s.bbox.y())
                && std::isfinite(s.bbox.width()) && std::isfinite(s.bbox.height())
                && s.bbox.width() > 0 && s.bbox.height() > 0 && std::isfinite(s.rotation)
                && std::isfinite(s.rotCenter.x()) && std::isfinite(s.rotCenter.y());
        }
        if (!ok) { error = QString::fromUtf8("标注含非法类别、坐标或掩码尺寸"); return false; }
    }
    return true;
}
bool decodeShapes(const QJsonArray& data, QList<AnnotationShape>& result, QString& error) {
    for (const auto& value : data) {
        auto o=value.toObject(); AnnotationShape s;
        if (!o.contains("type") || !o.contains("label_id")) { error="Invalid native annotation"; return false; }
        s.type=AnnotationShape::ShapeType(o["type"].toInt(-1)); s.labelId=o["label_id"].toInt(-1);
        s.visible=o["visible"].toBool(true); s.comment=o["comment"].toString();
        if (!QUuid(o["uuid"].toString()).isNull()) s.uuid=QUuid(o["uuid"].toString());
        auto bb=o["bbox"].toArray(); if(bb.size()==4) s.bbox={bb[0].toDouble(),bb[1].toDouble(),bb[2].toDouble(),bb[3].toDouble()};
        auto poly=o["polygon"].toArray();
        for(int i=0;i+1<poly.size();i+=2) s.polygon<<QPointF(poly[i].toDouble(),poly[i+1].toDouble());
        s.rotation=o["rotation"].toDouble(); auto rc=o["rot_center"].toArray();
        if(rc.size()==2) s.rotCenter={rc[0].toDouble(),rc[1].toDouble()};
        if(s.type==AnnotationShape::BrushMask) s.maskLayer=QImage::fromData(QByteArray::fromBase64(o["mask_png"].toString().toLatin1()),"PNG").convertToFormat(QImage::Format_Grayscale8);
        result.append(s);
    }
    return true;
}
}

AnnotationSaveResult AnnotationStore::save(const QString& path, const QString& imageName,
        const QImage& image, const QList<AnnotationShape>& shapes, bool mask, bool binary,
        int categoryCount, int offset) {
    AnnotationSaveResult result{false,path,{}};
    if (image.isNull() || !valid(shapes,image.size(),categoryCount,result.error)) return result;
    if (mask && !binary && categoryCount + offset - 1 > 255) {
        result.error=QString::fromUtf8("PNG最多支持255个非背景类别"); return result;
    }
    auto native=encodeShapes(shapes);
    if(native.size()!=shapes.size()) { result.error="Cannot encode annotation mask"; return result; }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        result.error=QString::fromUtf8("无法创建标注目录"); return result;
    }
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)) { result.error=file.errorString(); return result; }
    if(mask) {
        AnnotationRender render; render.setSourceImage(image);
        QImage pixels=render.compileMask(shapes,offset);
        if(binary) for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width();++x)
            if(pixels.scanLine(y)[x]) pixels.scanLine(y)[x]=255;
        pixels.setText(metadataKey,QString::fromUtf8(QJsonDocument(native).toJson(QJsonDocument::Compact)));
        if(!pixels.save(&file,"PNG")) { result.error=QString::fromUtf8("PNG编码或写入失败：%1").arg(file.errorString()); return result; }
    } else {
        QJsonArray annotations; int id=1;
        for(const auto& s:shapes) {
            if(!s.visible) continue;
            QJsonObject a{{"id",id++},{"image_id",1},{"category_id",s.labelId+offset}};
            if(s.type==AnnotationShape::PolygonShape) {
                QJsonArray poly; for(auto p:s.polygon) {poly.append(p.x());poly.append(p.y());}
                a["segmentation"]=poly; a["bbox"]=box(s.polygon.boundingRect());
            } else if(s.type==AnnotationShape::BrushMask) {
                result.error=QString::fromUtf8("边界框JSON不能表达画笔掩码，请使用矩形/多边形工具或掩码任务"); return result;
            } else {
                a["bbox"]=box(s.bbox);
                if(s.type==AnnotationShape::RotatedRectShape) {a["rotation"]=s.rotation;a["rot_center"]=point(s.rotCenter);}
            }
            annotations.append(a);
        }
        QJsonObject root{{"images",QJsonArray{QJsonObject{{"id",1},{"file_name",imageName},{"width",image.width()},{"height",image.height()}}}},
            {"annotations",annotations},{"fusevision_shapes",native},{"fusevision_version",1},
            {"fusevision_annotations_sha256",QString::fromLatin1(jsonDigest(annotations))}};
        auto bytes=QJsonDocument(root).toJson(QJsonDocument::Indented);
        if(file.write(bytes)!=bytes.size()) { result.error=file.errorString(); return result; }
    }
    result.ok=file.commit();
    if(!result.ok) result.error=file.errorString();
    return result;
}

bool AnnotationStore::load(const QString& path, QSize imageSize, bool binary,
        const QStringList& categories, QList<AnnotationShape>& shapes, QString& error, int offset) {
    shapes.clear();
    if(!QFileInfo::exists(path)) return true;
    QList<AnnotationShape> loaded;
    if(QFileInfo(path).suffix().compare("png",Qt::CaseInsensitive)==0) {
        QImage mask(path);
        if(mask.isNull() || mask.size()!=imageSize) {error=QString::fromUtf8("掩码损坏或尺寸与图像不同"); return false;}
        const auto native=mask.text(metadataKey);
        if(!native.isEmpty()) {
            QJsonParseError parse; auto doc=QJsonDocument::fromJson(native.toUtf8(),&parse);
            if(parse.error!=QJsonParseError::NoError || !doc.isArray() || !decodeShapes(doc.array(),loaded,error)) {
                error=QString::fromUtf8("原生标注元数据损坏：%1").arg(error); return false;
            }
            if(!valid(loaded,imageSize,categories.size(),error)) return false;
            AnnotationRender render;QImage source(imageSize,QImage::Format_RGB32);source.fill(0);render.setSourceImage(source);
            auto expected=render.compileMask(loaded,offset);
            if(binary) for(int y=0;y<expected.height();++y) for(int x=0;x<expected.width();++x) if(expected.scanLine(y)[x]) expected.scanLine(y)[x]=255;
            // Other editors can preserve PNG text while changing pixels. The raster remains authoritative then.
            if(!samePixels(expected,mask)) loaded.clear();
            else {shapes=loaded;return true;}
        }
        {
            const auto gray=mask.convertToFormat(QImage::Format_Grayscale8); QMap<int,QImage> layers;
            for(int y=0;y<gray.height();++y) for(int x=0;x<gray.width();++x) {
                const int pixel=gray.constScanLine(y)[x]; if(!pixel) continue;
                const int label=binary?0:pixel-offset;
                if(!layers.contains(label)) {layers[label]=QImage(imageSize,QImage::Format_Grayscale8);layers[label].fill(0);}
                layers[label].scanLine(y)[x]=255;
            }
            for(auto it=layers.begin();it!=layers.end();++it) {
                AnnotationShape s; s.labelId=it.key(); s.maskLayer=it.value(); loaded.append(s);
            }
        }
    } else {
        QFile f(path); if(!f.open(QIODevice::ReadOnly)) {error=f.errorString();return false;}
        QJsonParseError parse; auto doc=QJsonDocument::fromJson(f.readAll(),&parse);
        if(parse.error!=QJsonParseError::NoError || !doc.isObject()) {error=QString::fromUtf8("JSON解析失败：%1").arg(parse.errorString());return false;}
        const auto root=doc.object();
        const bool metadataMatches=!root.contains("fusevision_annotations_sha256") ||
            root["fusevision_annotations_sha256"].toString().toLatin1()==jsonDigest(root["annotations"].toArray());
        if(root.contains("fusevision_shapes") && metadataMatches) {
            if(root["fusevision_version"].toInt()!=1 || !root["fusevision_shapes"].isArray() || !decodeShapes(root["fusevision_shapes"].toArray(),loaded,error)) {
                error=QString::fromUtf8("不支持或损坏的原生标注元数据");return false;
            }
        } else {
            if(!root["annotations"].isArray() && root["shapes"].isArray()) {
                for(const auto& value:root["shapes"].toArray()) {
                    const auto o=value.toObject();AnnotationShape s;
                    s.labelId=categories.indexOf(o["label"].toString());
                    if(s.labelId<0) s.labelId=o["category_id"].toInt(-1);
                    auto points=o["points"].toArray();
                    for(const auto& v:points) {
                        const auto p=v.toArray();if(p.size()!=2 || !p[0].isDouble() || !p[1].isDouble()) {error="Invalid LabelMe point";return false;}
                        s.polygon<<QPointF(p[0].toDouble(),p[1].toDouble());
                    }
                    const auto type=o["shape_type"].toString("polygon");
                    if(type=="rectangle" && s.polygon.size()==2) {
                        s.type=AnnotationShape::RectShape;s.bbox=QRectF(s.polygon[0],s.polygon[1]).normalized();s.polygon.clear();
                    } else if(type=="polygon") s.type=AnnotationShape::PolygonShape;
                    else {error=QString::fromUtf8("暂不支持此LabelMe形状：%1").arg(type);return false;}
                    loaded.append(s);
                }
            } else if(!root["annotations"].isArray()) {error=QString::fromUtf8("需要annotations或shapes数组");return false;}
            for(const auto& value:root["annotations"].toArray()) {
                auto a=value.toObject(); AnnotationShape s; s.labelId=a["category_id"].toInt(-1)-offset;
                if(a["segmentation"].isObject()) {
                    const auto rle=a["segmentation"].toObject();const auto size=rle["size"].toArray();
                    if(size.size()!=2 || size[0].toInt()!=imageSize.height() || size[1].toInt()!=imageSize.width() || !rle["counts"].isArray()) {
                        error=QString::fromUtf8("只支持与图像同尺寸的非压缩COCO RLE；压缩RLE需先由COCO工具解码");return false;
                    }
                    s.type=AnnotationShape::BrushMask;s.maskLayer=QImage(imageSize,QImage::Format_Grayscale8);s.maskLayer.fill(0);
                    qint64 position=0;bool foreground=false;const qint64 total=qint64(imageSize.width())*imageSize.height();
                    for(const auto& count:rle["counts"].toArray()) {
                        const double number=count.toDouble(-1);
                        if(!std::isfinite(number) || number<0 || number!=std::floor(number) || number>total-position) {error="Invalid COCO RLE count";return false;}
                        const auto end=position+qint64(number);
                        if(foreground) for(auto i=position;i<end;++i) s.maskLayer.scanLine(int(i%imageSize.height()))[int(i/imageSize.height())]=255;
                        position=end;foreground=!foreground;
                    }
                    if(position!=total) {error="Incomplete COCO RLE";return false;}
                    loaded.append(s);continue;
                }
                auto seg=a["segmentation"].toArray();
                if(seg.size()>1 && seg.first().isArray()) {
                    s.type=AnnotationShape::BrushMask;s.maskLayer=QImage(imageSize,QImage::Format_Grayscale8);s.maskLayer.fill(0);
                    QPainter painter(&s.maskLayer);painter.setPen(Qt::NoPen);painter.setBrush(Qt::white);
                    for(const auto& value:seg) {
                        const auto coords=value.toArray();QPolygonF polygon;
                        if(coords.size()<6 || coords.size()%2) {error="Invalid COCO polygon";return false;}
                        for(int i=0;i<coords.size();i+=2) {
                            if(!coords[i].isDouble() || !coords[i+1].isDouble()) {error="Invalid COCO coordinate";return false;}
                            polygon<<QPointF(coords[i].toDouble(),coords[i+1].toDouble());
                        }
                        painter.drawPolygon(polygon);
                    }
                    painter.end();loaded.append(s);continue;
                }
                if(!seg.isEmpty() && seg.first().isArray()) seg=seg.first().toArray();
                if(!seg.isEmpty()) {
                    s.type=AnnotationShape::PolygonShape;
                    if(seg.size()%2) {error="Invalid polygon";return false;}
                    for(int i=0;i+1<seg.size();i+=2) {
                        if(!seg[i].isDouble() || !seg[i+1].isDouble()) {error="Invalid polygon coordinate";return false;}
                        s.polygon<<QPointF(seg[i].toDouble(),seg[i+1].toDouble());
                    }
                } else {
                    auto bb=a["bbox"].toArray(); if(bb.size()!=4) {error="Invalid bbox";return false;}
                    for(const auto& v:bb) if(!v.isDouble()) {error="Invalid bbox coordinate";return false;}
                    s.type=a.contains("rotation")?AnnotationShape::RotatedRectShape:AnnotationShape::RectShape;
                    s.bbox={bb[0].toDouble(),bb[1].toDouble(),bb[2].toDouble(),bb[3].toDouble()}; s.rotation=a["rotation"].toDouble();
                    auto rc=a["rot_center"].toArray(); if(rc.size()==2) s.rotCenter={rc[0].toDouble(),rc[1].toDouble()};
                }
                loaded.append(s);
            }
        }
    }
    if(!valid(loaded,imageSize,categories.size(),error)) return false;
    shapes=loaded; return true;
}
