#include "DatasetFileService.h"
#include "AnnotationStore.h"
#include "core/AtomicFile.h"
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTransform>
#include <QRegularExpression>
#include <QTextStream>
#include <cmath>

QString DatasetTransferReport::summary() const {
    return QObject::tr("成功 %1 项，跳过 %2 项，失败 %3 项%4\n\n")
        .arg(succeeded.size()).arg(skipped.size()).arg(failures.size()).arg(cancelled?QObject::tr("（已取消，已完成项保留）"):QString())
        + (warnings.isEmpty()?QString():QObject::tr("格式限制：\n")+warnings.join("\n")+"\n\n")
        + QObject::tr("成功：\n")+succeeded.join("\n")+QObject::tr("\n\n跳过：\n")+skipped.join("\n")
        + QObject::tr("\n\n失败（修复原因后可按原路径重试）：\n")+failures.join("\n");
}

bool DatasetFileService::copyAtomic(const QString& source,const QString& target,QString& error) {
    if(QFileInfo(source).canonicalFilePath()==QFileInfo(target).canonicalFilePath() && QFileInfo::exists(target)) return true;
    QFile in(source);if(!in.open(QIODevice::ReadOnly)) {error=source+"\n"+in.errorString();return false;}
    if(!QDir().mkpath(QFileInfo(target).absolutePath())) {error=QObject::tr("无法创建目录：%1").arg(target);return false;}
    QSaveFile out(target);if(!out.open(QIODevice::WriteOnly)) {error=target+"\n"+out.errorString();return false;}
    while(!in.atEnd()) {
        const auto bytes=in.read(1024*1024);
        if(in.error()!=QFileDevice::NoError || out.write(bytes)!=bytes.size()) {error=QObject::tr("复制失败：%1 → %2").arg(source,target);return false;}
    }
    if(!out.commit()) {error=target+"\n"+out.errorString();return false;}
    return true;
}

DatasetTransferReport DatasetFileService::removeSamples(const QStringList& images,const QString& imageDir,
        const QString& labelDir,const QString& recoveryRoot,const Allowed& allowed) {
    DatasetTransferReport report;
    for(const auto& image:images) {
        if(!allowed || !allowed()) {report.cancelled=true;break;}
        const auto relative=QDir(imageDir).relativeFilePath(image);
        if(relative==".." || relative.startsWith("../") || QDir::isAbsolutePath(relative) || QFileInfo(image).isSymLink() || !QFileInfo::exists(image)) {
            report.failures.append(image+QObject::tr("：文件缺失或不在当前图像目录"));continue;
        }
        QStringList sources{image};
        const auto labelFolder=QDir(labelDir).filePath(QFileInfo(relative).path());
        for(const auto& entry:QDir(labelFolder).entryInfoList(QDir::Files|QDir::Hidden))
            if(entry.completeBaseName().compare(QFileInfo(image).completeBaseName(),Qt::CaseInsensitive)==0 &&
                QStringList{"png","json","txt","xml"}.contains(entry.suffix().toLower())) sources.append(entry.absoluteFilePath());
        QString error;
        if(!QDir().mkpath(recoveryRoot)) {report.failures.append(recoveryRoot+QObject::tr("：无法创建恢复目录"));continue;}
        QTemporaryDir recovery(QDir(recoveryRoot).filePath("sample-XXXXXX"));
        if(!recovery.isValid()) {report.failures.append(recoveryRoot+QObject::tr("：无法创建恢复记录"));continue;}
        QStringList targets;QJsonArray manifest;
        for(int i=0;i<sources.size();++i) {
            const auto target=recovery.filePath(QString::number(i)+"-"+QFileInfo(sources[i]).fileName());targets.append(target);
            manifest.append(QJsonObject{{"original",sources[i]},{"recovery",QFileInfo(target).fileName()}});
            if(QFileInfo(sources[i]).isSymLink()) error=QObject::tr("拒绝移动符号链接：%1").arg(sources[i]);
        }
        if(error.isEmpty()) AtomicFile::write(recovery.filePath("restore.json"),QJsonDocument(manifest).toJson(),&error);
        if(error.isEmpty() && !allowed()) error=QObject::tr("提交前权限已变化");
        int moved=0;
        while(error.isEmpty() && moved<sources.size()) {
            if(!QDir().rename(sources[moved],targets[moved])) error=QObject::tr("无法移动（可能被占用）：%1").arg(sources[moved]);
            else ++moved;
        }
        if(!error.isEmpty()) {
            bool restored=true;for(int i=moved-1;i>=0;--i) restored &= QDir().rename(targets[i],sources[i]);
            if(!restored) {recovery.setAutoRemove(false);error+=QObject::tr("；恢复不完整，按restore.json恢复：%1").arg(recovery.path());}
            report.failures.append(image+"："+error);report.failedSources.append(image);
        } else {recovery.setAutoRemove(false);report.succeeded.append(image+" → "+recovery.path());}
    }
    return report;
}

DatasetTransferReport DatasetFileService::importFiles(const QStringList& images,const QStringList& labels,
        const QString& imageDir,const QString& labelDir,bool overwrite,const Allowed& allowed,
        const QStringList& categories,bool binary,int offset) {
    DatasetTransferReport report;
    QMap<QString,QStringList> groups;
    for(const auto& p:images) groups[QFileInfo(p).completeBaseName().toCaseFolded()].append(p);
    for(const auto& p:labels) groups[QFileInfo(p).completeBaseName().toCaseFolded()].append(p);
    for(auto group=groups.begin();group!=groups.end();++group) {
        if(!allowed || !allowed()) {report.cancelled=true;report.failures.append(QObject::tr("权限不足或操作已取消，剩余项未写入"));break;}
        const auto sources=group.value();QString image,label,error;QStringList targets;
        for(const auto& src:sources) {
            if(images.contains(src)) {if(!image.isEmpty()) error=QObject::tr("同一基名对应多张图像，无法确定标签配对");image=src;}
            else {if(!label.isEmpty()) error=QObject::tr("同一图像对应多个标签格式，请分批选择");label=src;}
        }
        const auto existing=QDir(imageDir).entryInfoList(QDir::Files);
        QString existingImage;
        for(const auto& item:existing) if(item.completeBaseName().compare(group.key(),Qt::CaseInsensitive)==0) {
            if(!existingImage.isEmpty()) error=QObject::tr("目标数据集存在同基名图像歧义");existingImage=item.absoluteFilePath();
        }
        if(!image.isEmpty() && !existingImage.isEmpty() && QFileInfo(image).fileName().compare(QFileInfo(existingImage).fileName(),Qt::CaseInsensitive)!=0)
            error=QObject::tr("同基名不同扩展名会共用标签，拒绝覆盖");
        const QString pairedImage=image.isEmpty()?existingImage:image;
        const QImage decoded(pairedImage);
        if(decoded.isNull()) error=QObject::tr("图像损坏或标签没有唯一配对图像：%1").arg(pairedImage);
        if(!label.isEmpty()) {
            auto candidateCategories=categories;
            int maxId=-1;
            auto includeNativeCategories=[&](const QJsonArray& native) {
                for(const auto& value:native) {
                    const auto id=value.toObject()["label_id"].toDouble(-1);
                    if(id>=0 && id<=65535 && id==std::floor(id)) maxId=qMax(maxId,int(id));
                }
            };
            if(QFileInfo(label).suffix().compare("png",Qt::CaseInsensitive)==0) {
                QImage mask(label);if(mask.isNull() || mask.size()!=decoded.size()) error=QObject::tr("掩码损坏或尺寸与图像不一致");
                includeNativeCategories(QJsonDocument::fromJson(mask.text("FuseVision.Annotation.v1").toUtf8()).array());
                const auto gray=mask.convertToFormat(QImage::Format_Grayscale8);
                for(int y=0;y<gray.height();++y) for(int x=0;x<gray.width();++x)
                    if(gray.constScanLine(y)[x]) maxId=qMax(maxId,binary?0:int(gray.constScanLine(y)[x])-offset);
            } else {
                QFile f(label);QJsonParseError parse;
                if(!f.open(QIODevice::ReadOnly)) error=f.errorString();
                else {auto doc=QJsonDocument::fromJson(f.readAll(),&parse);
                    if(parse.error!=QJsonParseError::NoError || !doc.isObject() || (!doc.object()["annotations"].isArray() && !doc.object()["shapes"].isArray())) error=QObject::tr("无效JSON标签");
                    else {
                        includeNativeCategories(doc.object()["fusevision_shapes"].toArray());
                        for(const auto& value:doc.object()["annotations"].toArray()) {
                            const auto id=value.toObject()["category_id"].toDouble(-1);
                            if(id<offset || id>65535+offset || id!=std::floor(id)) {error=QObject::tr("非法类别编号");break;}
                            maxId=qMax(maxId,int(id)-offset);
                        }
                        for(const auto& value:doc.object()["shapes"].toArray()) {
                            const auto shape=value.toObject();const auto name=shape["label"].toString();
                            if(!name.trimmed().isEmpty() && !candidateCategories.contains(name)) candidateCategories.append(name);
                            if(name.trimmed().isEmpty()) {
                                const auto id=shape["category_id"].toDouble(-1);
                                if(id<0 || id>65535 || id!=std::floor(id)) {error=QObject::tr("非法类别编号或空类别名称");break;}
                                maxId=qMax(maxId,int(id));
                            }
                        }
                    }
                }
            }
            while(candidateCategories.size()<=maxId) candidateCategories.append(QString("class_%1").arg(candidateCategories.size()+1));
            QList<AnnotationShape> shapes;
            if(error.isEmpty() && !AnnotationStore::load(label,decoded.size(),binary,candidateCategories,shapes,error,offset))
                error=QObject::tr("标注语义校验失败：%1").arg(error);
        }
        bool conflict=false;
        for(const auto& src:sources) {
            const auto dst=QDir(images.contains(src)?imageDir:labelDir).filePath(QFileInfo(src).fileName());targets.append(dst);
            conflict|=QFileInfo::exists(dst);
        }
        if(conflict && !overwrite) {report.skipped.append(sources.join(" + ")+QObject::tr("：同名项及其配对整体跳过"));continue;}
        if(!image.isEmpty() && label.isEmpty() && !existingImage.isEmpty()) {
            for(const auto& item:QDir(labelDir).entryInfoList(QDir::Files)) if(item.completeBaseName().compare(group.key(),Qt::CaseInsensitive)==0)
                error=QObject::tr("覆盖已标注图像时必须同时选择配对标签，以免旧标签应用于新图像");
        }
        if(!label.isEmpty()) for(const auto& item:QDir(labelDir).entryInfoList(QDir::Files))
            if(item.completeBaseName().compare(group.key(),Qt::CaseInsensitive)==0 && item.suffix().compare(QFileInfo(label).suffix(),Qt::CaseInsensitive)!=0)
                error=QObject::tr("存在同基名其他格式标签，拒绝创建会被旧格式遮蔽的标注；请先备份并明确保留一种格式：%1").arg(item.absoluteFilePath());
        // Stage the pair, then back up destinations. A failed pair restores both files.
        if(!QDir().mkpath(imageDir) || !QDir().mkpath(labelDir)) error=QObject::tr("无法创建目标目录");
        QTemporaryDir stage(QFileInfo(imageDir).absolutePath()+"/.fusevision-import-XXXXXX");
        if(!stage.isValid()) error=QObject::tr("无法创建导入暂存目录");
        for(int i=0;i<sources.size() && error.isEmpty();++i) copyAtomic(sources[i],stage.filePath(QString::number(i)),error);
        QList<int> backups,committed;
        if(error.isEmpty() && (!allowed || !allowed())) error=QObject::tr("提交前权限变化或操作已取消");
        for(int i=0;i<targets.size() && error.isEmpty();++i) {
            if(QFileInfo::exists(targets[i])) {
                if(!QDir().rename(targets[i],stage.filePath("backup-"+QString::number(i)))) {error=QObject::tr("无法备份目标：%1").arg(targets[i]);break;}
                backups.append(i);
            }
            if(!QDir().rename(stage.filePath(QString::number(i)),targets[i])) {error=QObject::tr("无法提交：%1").arg(targets[i]);break;}
            committed.append(i);
        }
        if(!error.isEmpty()) {
            bool restored=true;
            for(int i:committed) restored &= QFile::remove(targets[i]);
            for(int i:backups) restored &= QDir().rename(stage.filePath("backup-"+QString::number(i)),targets[i]);
            if(!restored) {stage.setAutoRemove(false);error+=QObject::tr("；自动恢复不完整，备份保留在 %1").arg(stage.path());}
            report.failures.append(sources.join(" + ")+" → "+targets.join(" + ")+"："+error);report.failedSources.append(sources);
        } else report.succeeded.append(sources.join(" + ")+" → "+targets.join(" + "));
    }
    return report;
}

DatasetTransferReport DatasetFileService::importClassificationFiles(const QStringList& images,const QString& root,
        const QString& phase,const QString& category,const Allowed& allowed) {
    DatasetTransferReport report;
    if(!QStringList{"train","val","test"}.contains(phase) || !QStringList{"OK","NG"}.contains(category)) {
        report.failures.append(QObject::tr("非法分类划分或类别"));return report;
    }
    for(const auto& source:images) {
        if(!allowed || !allowed()) {report.cancelled=true;break;}
        const auto target=QDir(root).filePath(phase+"/"+category+"/"+QFileInfo(source).fileName());
        if(QFileInfo::exists(target)) {report.skipped.append(source+QObject::tr("：同名项跳过"));continue;}
        QString error;
        if(QImage(source).isNull()) error=QObject::tr("图像无法解码");
        if(error.isEmpty() && !allowed()) {report.cancelled=true;break;}
        if(error.isEmpty()) copyAtomic(source,target,error);
        if(error.isEmpty()) report.succeeded.append(source+" → "+target);
        else {report.failures.append(source+"："+error);report.failedSources.append(source);}
    }
    return report;
}

bool DatasetFileService::readShapes(const QString& path,QSize size,const QStringList& categories,bool binary,
        QList<AnnotationShape>& shapes,QString& error) {
    if(QFileInfo(path).suffix().compare("txt",Qt::CaseInsensitive)!=0)
        return AnnotationStore::load(path,size,binary,categories,shapes,error);
    shapes.clear();QFile file(path);if(!file.open(QIODevice::ReadOnly)) {error=file.errorString();return false;}
    int line=0;
    while(!file.atEnd()) {
        ++line;const auto text=QString::fromUtf8(file.readLine()).trimmed();if(text.isEmpty()) continue;
        const auto fields=text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);bool ok=false;
        AnnotationShape s;s.labelId=fields[0].toInt(&ok);
        if(!ok || s.labelId<0 || s.labelId>=categories.size()) {error=QObject::tr("第%1行类别非法").arg(line);return false;}
        QList<double> coords;
        for(int i=1;i<fields.size();++i) {const auto value=fields[i].toDouble(&ok);if(!ok || !std::isfinite(value) || value<0 || value>1) {error=QObject::tr("第%1行坐标非法").arg(line);return false;}coords.append(value);}
        if(coords.size()==4) {
            s.type=AnnotationShape::RectShape;s.bbox={(coords[0]-coords[2]/2)*size.width(),(coords[1]-coords[3]/2)*size.height(),coords[2]*size.width(),coords[3]*size.height()};
            if(s.bbox.width()<=0 || s.bbox.height()<=0) {error="Empty YOLO bbox";return false;}
        } else if(coords.size()>=6 && coords.size()%2==0) {
            s.type=AnnotationShape::PolygonShape;for(int i=0;i<coords.size();i+=2) s.polygon<<QPointF(coords[i]*size.width(),coords[i+1]*size.height());
        } else {error=QObject::tr("第%1行字段数非法").arg(line);return false;}
        shapes.append(s);
    }
    return true;
}

namespace {
QPolygonF corners(QRectF r) {return {r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()};}
QPolygonF geometry(const AnnotationShape& s) {
    if(s.type==AnnotationShape::PolygonShape) return s.polygon;
    auto polygon=corners(s.bbox);
    if(s.type==AnnotationShape::RotatedRectShape) {
        const auto center=s.rotCenter.isNull()?s.bbox.center():s.rotCenter;
        QTransform rotate;rotate.translate(center.x(),center.y());rotate.rotate(s.rotation);rotate.translate(-center.x(),-center.y());polygon=rotate.map(polygon);
    }
    return polygon;
}
QRectF maskBounds(const QImage& mask) {
    int left=mask.width(),top=mask.height(),right=-1,bottom=-1;
    for(int y=0;y<mask.height();++y) for(int x=0;x<mask.width();++x) if(mask.constScanLine(y)[x]) {
        left=qMin(left,x);right=qMax(right,x);top=qMin(top,y);bottom=qMax(bottom,y);
    }
    return right<0?QRectF():QRectF(left,top,right-left+1,bottom-top+1);
}
QString taskFor(const QString& modelType) {
    return modelType==ModelTaskTypes::InstanceSeg?"segment":modelType==ModelTaskTypes::ObjDetectFixed?"obb":"detect";
}
}

bool DatasetFileService::yoloText(const QList<AnnotationShape>& shapes,QSize size,const QString& task,
        QByteArray& bytes,QStringList& warnings,QString& error) {
    QString output;QTextStream text(&output);text.setRealNumberNotation(QTextStream::FixedNotation);text.setRealNumberPrecision(6);
    for(const auto& s:shapes) {
        if(!s.visible) continue;
        auto polygon=geometry(s);
        if(s.type==AnnotationShape::BrushMask) {
            const auto bounds=maskBounds(s.maskLayer);if(bounds.isEmpty()) continue;
            polygon=corners(bounds);warnings.append(QObject::tr("PNG/画笔区域在YOLO中按每类别包围区域近似，孔洞、分离实例和细轮廓可能丢失；原生PNG可保留像素。"));
        }
        if(task=="obb" && polygon.size()!=4) {
            polygon=corners(polygon.boundingRect());warnings.append(QObject::tr("非四点多边形转OBB时使用水平包围框。"));
        }
        if(polygon.size()<3 || size.isEmpty()) {error="Invalid geometry or image size";return false;}
        for(auto p:polygon) if(!std::isfinite(p.x()) || !std::isfinite(p.y()) || p.x()<0 || p.y()<0 || p.x()>size.width() || p.y()>size.height()) {
            error=QObject::tr("标注超出图像边界，请先修正坐标；未静默裁切。");return false;
        }
        text<<s.labelId;
        if(task=="detect") {
            const auto r=polygon.boundingRect();text<<' '<<r.center().x()/size.width()<<' '<<r.center().y()/size.height()<<' '<<r.width()/size.width()<<' '<<r.height()/size.height();
            if(s.type!=AnnotationShape::RectShape) warnings.append(QObject::tr("detect格式仅保留水平包围框，旋转角度和轮廓会丢失。"));
        } else for(auto p:polygon) text<<' '<<p.x()/size.width()<<' '<<p.y()/size.height();
        text<<'\n';
    }
    text.flush();bytes=output.toUtf8();return true;
}

DatasetTransferReport DatasetFileService::exportFiles(Format format,const QString& destination,
        const QVector<DatasetSample>& samples,const QList<int>& indices,const QString& labelsDir,
        const QStringList& categories,const QString& labelType,const QString& modelType,const Allowed& allowed) {
    DatasetTransferReport report;
    const auto target=format==Coco?QFileInfo(destination).absolutePath():QDir(destination).absolutePath();
    if(!allowed || !allowed()) {report.failures.append(QObject::tr("没有导出写权限"));return report;}
    if(QFileInfo::exists(target) && !QDir(target).entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty()) {
        report.failures.append(QObject::tr("请选择空导出目录，避免与已有数据混合：%1").arg(target));return report;
    }
    if(!QDir().mkpath(QFileInfo(target).absolutePath())) {report.failures.append(QObject::tr("无法创建导出父目录"));return report;}
    QTemporaryDir stage(QFileInfo(target).absolutePath()+"/.fusevision-export-XXXXXX");
    if(!stage.isValid()) {report.failures.append(QObject::tr("无法创建导出暂存目录"));return report;}
    QList<int> selected=indices;if(selected.isEmpty()) for(int i=0;i<samples.size();++i) selected.append(i);
    QJsonArray images,annotations,cats;QSet<QString> used;int imageId=0,annotationId=0;QString error;
    for(int i=0;i<categories.size();++i) cats.append(QJsonObject{{"id",i+1},{"name",categories[i]}});
    for(int index:selected) {
        if(!allowed()) {report.cancelled=true;break;}
        if(index<0 || index>=samples.size()) {report.failures.append("Invalid export selection");continue;}
        const auto& sample=samples[index];const auto base=QFileInfo(sample.fileName).completeBaseName();
        if(format==Classification) {
            const auto parts=sample.fileName.split('/');
            if(parts.size()!=3 || !QStringList{"train","val","test"}.contains(parts[0]) || !QStringList{"OK","NG"}.contains(parts[1]) ||
               parts[2].isEmpty() || parts[2]=="." || parts[2]==".." || parts[2].contains('\\') || parts[2].contains(':') ||
               used.contains(sample.fileName.toCaseFolded()) || QImage(sample.absolutePath).isNull()) {
                report.failures.append(sample.absolutePath+QObject::tr("：分类路径冲突、非法或图像损坏"));continue;
            }
            used.insert(sample.fileName.toCaseFolded());
            if(copyAtomic(sample.absolutePath,stage.filePath(sample.fileName),error)) report.succeeded.append(sample.absolutePath+" → "+target+"/"+sample.fileName);
            else report.failures.append(error);
            continue;
        }
        if(used.contains(base.toLower())) {report.failures.append(sample.absolutePath+QObject::tr("：基名冲突"));continue;}used.insert(base.toLower());
        QImage image(sample.absolutePath);if(image.isNull()) {report.failures.append(sample.absolutePath+QObject::tr("：图像损坏"));continue;}
        QString label;
        const QStringList suffixes=LabelTypes::isMaskType(labelType)?QStringList{"png","json","txt"}:QStringList{"json","txt","png"};
        for(const auto& ext:suffixes) {const auto candidate=QDir(labelsDir).filePath(base+"."+ext);if(QFileInfo::exists(candidate)) {label=candidate;break;}}
        QList<AnnotationShape> shapes;
        if(label.isEmpty()) report.warnings.append(QObject::tr("未标注图像也包含在导出范围；COCO/YOLO会将其表现为空目标，请核对是否应作为负样本：%1").arg(sample.fileName));
        if(!label.isEmpty() && !readShapes(label,image.size(),categories,labelType==LabelTypes::BinaryMask,shapes,error)) {report.failures.append(label+"："+error);continue;}
        const auto imageSub=format==Yolo?"images/train/":format==Mask?"images/":"";
        if(!copyAtomic(sample.absolutePath,stage.filePath(imageSub+sample.fileName),error)) {report.failures.append(error);continue;}
        ++imageId;images.append(QJsonObject{{"id",imageId},{"file_name",sample.fileName},{"width",image.width()},{"height",image.height()}});
        if(format==Mask) {
            if(!label.isEmpty() && QFileInfo(label).suffix().compare("png",Qt::CaseInsensitive)!=0) {report.failures.append(label+QObject::tr("：原生PNG导出需要PNG标签"));continue;}
            if(!label.isEmpty() && !copyAtomic(label,stage.filePath("labels/"+base+".png"),error)) {report.failures.append(error);continue;}
        } else if(format==Yolo) {
            QByteArray text;
            if(!yoloText(shapes,image.size(),taskFor(modelType),text,report.warnings,error) || !QDir().mkpath(stage.filePath("labels/train")) || !AtomicFile::write(stage.filePath("labels/train/"+base+".txt"),text,&error)) {report.failures.append(sample.absolutePath+"："+error);continue;}
        } else {
            for(const auto& shape:shapes) {
                if(!shape.visible) continue;
                QJsonObject ann{{"id",++annotationId},{"image_id",imageId},{"category_id",shape.labelId+1},{"iscrowd",0}};
                QPolygonF polygon=geometry(shape);QRectF r;
                if(shape.type==AnnotationShape::BrushMask) {
                    r=maskBounds(shape.maskLayer);if(r.isEmpty()) continue;
                    // Uncompressed COCO RLE uses column-major runs, starting with background.
                    QJsonArray runs;int count=0;bool foreground=false;int area=0;
                    for(int x=0;x<image.width();++x) for(int y=0;y<image.height();++y) {
                        const bool value=shape.maskLayer.constScanLine(y)[x]>0;if(value) ++area;
                        if(value!=foreground) {runs.append(count);count=0;foreground=value;}++count;
                    }
                    runs.append(count);ann["segmentation"]=QJsonObject{{"size",QJsonArray{image.height(),image.width()}},{"counts",runs}};
                    ann["area"]=area;ann["iscrowd"]=1;
                } else {
                    r=polygon.boundingRect();QJsonArray points;double area=0;
                    for(int k=0;k<polygon.size();++k) {auto p=polygon[k];auto q=polygon[(k+1)%polygon.size()];points.append(p.x());points.append(p.y());area+=p.x()*q.y()-q.x()*p.y();}
                    ann["segmentation"]=QJsonArray{points};ann["area"]=std::abs(area)/2;
                }
                ann["bbox"]=QJsonArray{r.x(),r.y(),r.width(),r.height()};annotations.append(ann);
            }
        }
        report.succeeded.append(sample.absolutePath+" → "+target);
    }
    if(format==Classification) {
        if(!AtomicFile::write(stage.filePath("data.yaml"),"path: .\ntrain: train\nval: val\ntest: test\ntask: classify\nnc: 2\nnames: [OK, NG]\n",&error)) report.failures.append(error);
    } else if(format==Coco) {
        QJsonObject coco{{"images",images},{"annotations",annotations},{"categories",cats}};
        if(!AtomicFile::write(stage.filePath(QFileInfo(destination).fileName()),QJsonDocument(coco).toJson(),&error)) report.failures.append(error);
    } else if(format==Yolo) {
        QString yaml="path: .\ntrain: images/train\ntask: "+taskFor(modelType)+"\nnc: "+QString::number(categories.size())+"\nnames:\n";
        for(int i=0;i<categories.size();++i) {
            const auto quoted=QString::fromUtf8(QJsonDocument(QJsonArray{categories[i]}).toJson(QJsonDocument::Compact));
            yaml+="  "+QString::number(i)+": "+quoted.mid(1,quoted.size()-2)+"\n";
        }
        if(!AtomicFile::write(stage.filePath("data.yaml"),yaml.toUtf8(),&error)) report.failures.append(error);
    }
    report.warnings.removeDuplicates();
    if(!report.ok() || !allowed()) {report.succeeded.clear();report.failures.append(QObject::tr("导出未提交，目标原有数据未修改。"));return report;}
    if(QFileInfo::exists(target) && !QDir().rmdir(target)) {report.succeeded.clear();report.failures.append(QObject::tr("无法替换空导出目录：%1").arg(target));return report;}
    if(!QDir().rename(stage.path(),target)) {report.succeeded.clear();report.failures.append(QObject::tr("无法提交导出目录：%1").arg(target));return report;}
    stage.setAutoRemove(false);return report;
}
