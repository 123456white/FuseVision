#include "DatasetFixture.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFontDatabase>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QCryptographicHash>
#include <QTimer>
#include <QMessageBox>
#include <QLabel>
#include <iostream>
#include <stdexcept>
#include "core/DatabaseManager.h"
#include "core/SettingsManager.h"
#include "core/SessionManager.h"
#include "deeplearning/model/AnnotationModel.h"
#include "deeplearning/storage/AnnotationStore.h"
#include "deeplearning/storage/DatasetFileService.h"
#include "deeplearning/widgets/DatasetManagement.h"
#include "deeplearning/widgets/AnnotationManagement.h"

static void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static void write(const QString& path,const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(),"write fixture");
}
static QByteArray read(const QString& path) {QFile f(path);check(f.open(QIODevice::ReadOnly),"read fixture");return f.readAll();}
static void isolate(const QString& root) {
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,root+"/settings");
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,root+"/system");
}

class WorkflowRegression {
public:
    static void datasetStorage(const QString& root) {
        QString error;check(ProjectConfig::createProject("Storage","",root,&error),"create storage project");
        const auto project=ProjectConfig::loadProject(root+"/Storage/project.fvproj",&error);
        check(ModelConfig::createModel("Dataset",ModelTaskTypes::ObjDetectFree,project.projectRoot,project.fvprojPath,&error),"create storage model");
        const auto dir=project.projectRoot+"/Dataset",path=dir+"/dataset_config.json";
        auto legacy=datasetFixture(LabelTypes::CocoBBox,QJsonArray{QJsonObject{{"name","defect"},{"color","#123456"}}});
        legacy["extension"]=QJsonObject{{"retain",42}};
        write(path,QJsonDocument(legacy).toJson());const auto initial=read(path);
        DatasetManagement data;data.setCurrentProject(project);data.setCurrentModel("Dataset",ModelConfig::loadModel(dir+"/Dataset.fvdl",&error));
        data.loadDatasetConfig();check(read(path)==initial && data.currentLabelNames()==QStringList{"defect"},"current dataset load never rewrites");
        auto concurrent=legacy;concurrent["extension"]=QJsonObject{{"retain",99}};write(path,QJsonDocument(concurrent).toJson());
        check(data.saveDatasetConfig(),"save current dataset config");
        const auto saved=QJsonDocument::fromJson(read(path)).object();
        check(saved["document_type"]=="fusevision.dataset" && saved["format_version"]=="1.0","dataset type/version persisted");
        check(saved["extension"]==concurrent["extension"],"latest unknown dataset fields retained");
        QTimer dismiss;QObject::connect(&dismiss,&QTimer::timeout,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->done(QMessageBox::Ok);});dismiss.start(10);
        for(const auto& entry:{std::pair<QString,QJsonValue>{"format_version","99.0"},{"document_type","another.application"},{"categories","invalid"}}) {
            auto invalid=saved;invalid[entry.first]=entry.second;write(path,QJsonDocument(invalid).toJson());const auto before=read(path);
            check(!data.saveDatasetConfig() && read(path)==before,"unsupported dataset config refuses overwrite");
        }
        for(const auto* key:{"document_type","format_version"}) {
            auto invalid=saved;invalid.remove(key);write(path,QJsonDocument(invalid).toJson());const auto before=read(path);
            data.loadDatasetConfig();check(!data.saveDatasetConfig() && read(path)==before,"unmarked dataset rejected without upgrade");
        }
        check(QFile::remove(path),"remove isolated dataset config");data.loadDatasetConfig();
        check(!data.saveDatasetConfig() && !QFileInfo::exists(path),"missing config is not silently reconstructed");
        dismiss.stop();write(path,QJsonDocument(saved).toJson());
        check(data.saveDatasetConfig(),"dataset config retries after file corrected");
    }
    static void invalidImport(const QString& root) {
        QImage image(64,64,QImage::Format_RGB32);image.fill(Qt::white);
        QDir().mkpath(root+"/source");QDir().mkpath(root+"/images");QDir().mkpath(root+"/labels");
        check(image.save(root+"/source/sample.png") && image.save(root+"/images/sample.png"),"image fixtures");
        QString error;AnnotationShape shape;shape.type=AnnotationShape::RectShape;shape.labelId=0;shape.bbox={1,2,3,4};
        const auto target=root+"/labels/sample.json";
        check(AnnotationStore::save(target,"sample.png",image,{shape},false,false,1).ok,"valid old label");
        const auto oldLabel=read(target),oldImage=read(root+"/images/sample.png");
        for(const auto& bytes:{QByteArray(R"({"annotations":[{"category_id":1,"bbox":[1,2,-3,4]}]})"),
                              QByteArray(R"({"annotations":[{"category_id":-1,"bbox":[1,2,3,4]}]})"),
                              QByteArray(R"({"annotations":[{"category_id":1.5,"bbox":[1,2,3,4]}]})"),
                              QByteArray(R"({"shapes":[{"label":"new","shape_type":"rectangle","points":[["bad",2],[3,4]]}]})"),
                              QByteArray(R"({"shapes":[{"label":"defect","shape_type":"circle","points":[[1,2],[3,4]]}]})")}) {
            write(root+"/source/sample.json",bytes);
            const auto report=DatasetFileService::importFiles({root+"/source/sample.png"},{root+"/source/sample.json"},root+"/images",root+"/labels",true,[]{return true;});
            check(!report.ok() && report.succeeded.isEmpty(),"ISS-011 invalid label must fail before overwrite");
            check(read(target)==oldLabel && read(root+"/images/sample.png")==oldImage,"ISS-011 failed import preserves pair byte for byte");
        }
        write(root+"/source/sample.json",R"({"shapes":[{"label":"new","shape_type":"rectangle","points":[[1,2],[3,4]]}]})");
        auto report=DatasetFileService::importFiles({root+"/source/sample.png"},{root+"/source/sample.json"},root+"/images",root+"/labels",true,[]{return true;});
        check(report.ok() && report.succeeded.size()==1,"valid new LabelMe category import");
        QList<AnnotationShape> shapes;check(AnnotationStore::load(target,image.size(),false,{"new"},shapes,error) && shapes.size()==1,"new category reopens");
        QImage mask(64,64,QImage::Format_Grayscale8);mask.fill(255);check(mask.save(root+"/source/mask.png"),"mask fixture");
        check(image.save(root+"/source/mask-image.png"),"mask image");
        // Use matching basenames in distinct source directories.
        QDir().mkpath(root+"/source/masks");check(mask.save(root+"/source/masks/mask-image.png"),"matching mask fixture");
        report=DatasetFileService::importFiles({root+"/source/mask-image.png"},{root+"/source/masks/mask-image.png"},root+"/images",root+"/labels",false,[]{return true;},{"anomaly"},true);
        check(report.ok() && AnnotationStore::load(root+"/labels/mask-image.png",image.size(),true,{"anomaly"},shapes,error),"binary mask semantics remain readable");
        QDir().mkpath(root+"/source/native");shape.visible=false;
        check(image.save(root+"/source/hidden.png") && AnnotationStore::save(root+"/source/native/hidden.png","hidden.png",image,{shape},true,false,1).ok,"hidden native PNG fixture");
        report=DatasetFileService::importFiles({root+"/source/hidden.png"},{root+"/source/native/hidden.png"},root+"/images",root+"/labels",false,[]{return true;});
        check(report.ok() && report.succeeded.size()==1,"native hidden shape imports without visible category pixels");
        check(AnnotationStore::save(root+"/source/hidden.json","hidden.png",image,{shape},false,false,1).ok,"hidden native JSON fixture");
        report=DatasetFileService::importFiles({root+"/source/hidden.png"},{root+"/source/hidden.json"},root+"/json-images",root+"/json-labels",false,[]{return true;});
        check(report.ok() && report.succeeded.size()==1,"native hidden shape imports without visible annotations");
        check(ProjectConfig::createProject("Native","",root,&error),"native import project");
        const auto project=ProjectConfig::loadProject(root+"/Native/project.fvproj",&error);
        check(ModelConfig::createModel("Mask",ModelTaskTypes::InstanceSeg,project.projectRoot,project.fvprojPath,&error),"native import model");
        DatasetManagement data;data.setCurrentProject(project);data.setCurrentModel("Mask",ModelConfig::loadModel(project.projectRoot+"/Mask/Mask.fvdl",&error));data.loadDatasetConfig();
        QTimer closeReport;QObject::connect(&closeReport,&QTimer::timeout,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->done(QMessageBox::Ok);});closeReport.start(10);
        data.importSelectedFiles({root+"/source/hidden.png"},{root+"/source/native/hidden.png"});closeReport.stop();
        check(data.currentLabelNames().size()==1 && data.allSamples().size()==1,"native hidden class discovered after widget import");
        check(AnnotationStore::load(data.labelsDir()+"/hidden.png",image.size(),false,data.currentLabelNames(),shapes,error) && shapes.size()==1 && !shapes[0].visible,"native hidden metadata reopens with discovered class");
    }
    static void rotatedMove(const QString& root) {
        AnnotationShape shape;shape.type=AnnotationShape::RotatedRectShape;shape.labelId=0;
        shape.bbox={80,95,40,10};shape.rotCenter={100,100};shape.rotation=90;
        AnnotationModel model;model.reset({shape});
        {AnnotationModel::Transaction tx(&model,"move");model.translateShape(0,{10,0});}
        QByteArray text;QString error;QStringList warnings;
        check(DatasetFileService::yoloText(model.shapes(),{200,200},"obb",text,warnings,error),"OBB export");
        const auto fields=text.simplified().split(' ');
        // Rotating (80,95) 90 degrees about (100,100), then translating gives (115,80).
        check(qAbs(fields[1].toDouble()*200-115)<0.001 && qAbs(fields[2].toDouble()*200-80)<0.001,"ISS-012 rendered corner follows screen translation");
        check(model.shapes()[0].rotCenter==QPointF(110,100),"explicit center translates");
        model.undo();check(model.shapes()[0]==shape,"move undo restores center");model.redo();
        model.copyToClipboard({0});model.pasteFromClipboard({3,7});
        check(model.shapes()[1].rotCenter==QPointF(113,107) && model.shapes()[1].uuid!=model.shapes()[0].uuid,"paste offsets explicit center with new identity");
        model.undo();check(model.shapeCount()==1,"paste undo");model.redo();
        QImage image(200,200,QImage::Format_RGB32);image.fill(Qt::white);
        check(AnnotationStore::save(root+"/obb.json","sample.png",image,model.shapes(),false,false,1).ok,"save moved OBB");
        QList<AnnotationShape> reopened;check(AnnotationStore::load(root+"/obb.json",image.size(),false,{"defect"},reopened,error),"reopen moved OBB");
        check(reopened==model.shapes(),"move/paste geometry survives save and reopen");
        shape.rotCenter={};model.reset({shape});{AnnotationModel::Transaction tx(&model,"implicit");model.translateShape(0,{4,5});}
        model.copyToClipboard({0});model.pasteFromClipboard({3,7});
        check(model.shapes()[0].rotCenter.isNull() && model.shapes()[1].rotCenter.isNull(),"implicit center sentinel retained");
    }
    static void classification(const QString& root) {
        QString error;check(ProjectConfig::createProject("Fixture","",root,&error),"project fixture");
        const auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj",&error);
        check(ModelConfig::createModel("Anomaly",ModelTaskTypes::AnomalyDetect,project.projectRoot,project.fvprojPath,&error),"classification fixture");
        const auto ds=project.projectRoot+"/Anomaly";QImage image(64,64,QImage::Format_RGB32);image.fill(Qt::white);
        check(image.save(ds+"/train/OK/same.png") && image.save(ds+"/train/NG/same.png") && image.save(ds+"/val/NG/other.png"),"classification images");
        DatasetManagement data;data.setCurrentProject(project);data.setCurrentModel("Anomaly",ModelConfig::loadModel(ds+"/Anomaly.fvdl",&error));
        data.loadDatasetConfig();
        check(data.allSamples().size()==3,"ISS-014 gallery reads phase/class directories");
        check(data.currentLabelNames()==QStringList{"OK","NG"} && data.currentLabelInstanceCounts()==QList<int>{1,2},"classification category counts");
        auto stats=data.computeStats();data.runQualityCheck(stats);
        check(stats.annotatedImages==3 && stats.missingAnnotations==0 && stats.corruptedFiles==0,"classification has labels without mask files");
        check(!data.m_btnAddLabel->isEnabled() && !data.m_btnEditLabel->isEnabled(),"directory class names locked");
        bool annotationOpened=false;QObject::connect(&data,&DatasetManagement::openAnnotationRequested,&data,[&]{annotationOpened=true;});
        bool previewSeen=false;
        QTimer::singleShot(0,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            previewSeen=dialog && dialog->objectName()=="classificationPreview" && dialog->findChild<QLabel*>("classificationImage") && !dialog->findChild<QLabel*>("classificationImage")->pixmap().isNull();
            if(dialog) dialog->reject();});
        data.onThumbnailDoubleClicked(0);check(previewSeen && !annotationOpened,"classification opens read-only image preview");
        AnnotationManagement annotation;annotation.setDatasetSource(&data);check(!annotation.openImage("Anomaly",data.allSamples()[0].absolutePath),"direct annotation entry refuses classification");
        auto report=DatasetFileService::exportFiles(DatasetFileService::Classification,root+"/export",data.allSamples(),{},data.labelsDir(),data.currentLabelNames(),data.currentLabelType(),ModelTaskTypes::AnomalyDetect,[]{return true;});
        check(report.ok() && report.succeeded.size()==3 && report.warnings.isEmpty(),"classification export supports repeated basenames across classes");
        for(const auto& sample:data.allSamples()) check(read(root+"/export/"+sample.fileName)==read(sample.absolutePath),"export preserves split/class and image bytes");
        check(read(root+"/export/data.yaml").contains("task: classify"),"classification YAML");
        report=DatasetFileService::exportFiles(DatasetFileService::Classification,root+"/selected",data.allSamples(),{2},data.labelsDir(),data.currentLabelNames(),data.currentLabelType(),ModelTaskTypes::AnomalyDetect,[]{return true;});
        check(report.ok() && report.succeeded.size()==1 && QFileInfo::exists(root+"/selected/val/NG/other.png") && !QFileInfo::exists(root+"/selected/train/OK/same.png"),"selection export retains val split");
        report=DatasetFileService::importClassificationFiles({root+"/export/train/OK/same.png"},ds,"test","OK",[]{return true;});
        check(report.ok() && report.succeeded.size()==1,"classification service import");
        data.loadDatasetConfig();check(data.allSamples().size()==4 && data.currentLabelInstanceCounts()==QList<int>{2,2},"import reload updates counts");
        check(data.saveDatasetConfig(),"save classification mode");data.loadDatasetConfig();check(data.isClassificationDataset() && data.allSamples().size()==4,"persisted classification mode reloads");
        const auto removed=ds+"/test/OK/same.png";
        report=DatasetFileService::removeSamples({removed},data.imagesDir(),data.labelsDir(),ds+"/.fusevision-trash",[]{return true;});
        check(report.ok() && report.succeeded.size()==1 && !QFileInfo::exists(removed),"classification recoverable deletion");
        const auto recovery=QDir(ds+"/.fusevision-trash").entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
        check(recovery.size()==1,"classification recovery record");
        const auto manifest=QJsonDocument::fromJson(read(recovery[0].absoluteFilePath()+"/restore.json")).array();
        for(const auto& value:manifest) {const auto o=value.toObject();check(QFile::rename(recovery[0].absoluteFilePath()+"/"+o["recovery"].toString(),o["original"].toString()),"restore deleted classification image");}
        SessionManager::instance().logout();data.applyPermissions();check(!data.m_btnExport->isEnabled(),"guest cannot export");
        report=DatasetFileService::importClassificationFiles({root+"/export/train/OK/same.png"},ds,"test","NG",[]{return false;});
        check(report.cancelled && !QFileInfo::exists(ds+"/test/NG/same.png"),"guest classification import denied");
        SessionManager::instance().login(DatabaseManager::instance().singleAccount().id);
        const auto configPath=ds+"/dataset_config.json";
        write(configPath,QJsonDocument(datasetFixture(LabelTypes::BinaryMask,QJsonArray{QJsonObject{{"name","defect"}}})).toJson());
        const auto before=read(configPath);data.loadDatasetConfig();
        QTimer dismiss;QObject::connect(&dismiss,&QTimer::timeout,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->done(QMessageBox::Ok);});dismiss.start(10);
        check(data.isClassificationDataset() && !data.saveDatasetConfig() && read(configPath)==before,"classification rejects mismatched annotation mode without rewriting");
        dismiss.stop();
    }
    static void database(const QString& root) {
        const auto runtime=root+"/runtime";QDir().mkpath(runtime);
        const auto binary=runtime+"/database-test.exe";
        check(QFile::copy(QCoreApplication::applicationFilePath(),binary),"copy isolated database executable");
        auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",QCoreApplication::applicationDirPath()+";"+env.value("PATH"));
        for(const auto& action:{"seed","configured","reopen","relocate-wal","moved-reopen","missing","existing","blocked"}) {
            QProcess child;child.setProcessEnvironment(env);
            child.start(binary,{"--database-child",root,action,QCoreApplication::applicationDirPath()});
            check(child.waitForFinished(15000),"database subprocess completed");
            std::cout<<child.readAllStandardOutput().constData()<<child.readAllStandardError().constData();
            check(child.exitStatus()==QProcess::NormalExit && child.exitCode()==0,"ISS-013 configured path/restart/account preservation");
        }
    }
};

int main(int argc,char** argv) {
    if(argc>1 && QByteArray(argv[1])=="--database-child") {
        QCoreApplication app(argc,argv);const auto args=app.arguments();isolate(args[2]);QCoreApplication::addLibraryPath(args[4]);
        try {
            auto& settings=SettingsManager::instance();auto& db=DatabaseManager::instance();const auto action=args[3];
            if(action=="seed") {
                settings.setDbPath("");check(settings.save() && db.initDatabase(),"seed default isolated database");
                SessionManager::instance().login(db.singleAccount().id);
                check(db.changePassword(db.singleAccount().id,"admin123","kept-password"),"change isolated password");
                settings.setDbPath(args[2]+"/configured");check(settings.save(),"persist configured directory");
            } else if(action=="relocate-wal") {
                auto source=QSqlDatabase::addDatabase("QSQLITE","wal-source");source.setDatabaseName(args[2]+"/configured/fusevision.db");
                check(source.open(),"open live WAL source");QSqlQuery query(source);
                check(query.exec("PRAGMA journal_mode=WAL") && query.next() && query.value(0).toString()=="wal","WAL source mode");
                check(query.exec("PRAGMA wal_autocheckpoint=0"),"retain committed WAL pages");
                query.prepare("UPDATE users SET password_hash=?");query.addBindValue(QString::fromLatin1(QCryptographicHash::hash("wal-password",QCryptographicHash::Sha256).toHex()));
                check(query.exec(),"password change committed in WAL");
                settings.setDbPath(args[2]+"/moved");check(settings.save() && db.initDatabase(),"migrate custom directory with WAL source open");
                check(db.validateLogin("superadmin","wal-password").success,"snapshot includes committed WAL pages");
                check(QFileInfo::exists(args[2]+"/configured/fusevision.db"),"custom migration source kept");
            } else if(action=="missing") {
                check(QFile::rename(args[2]+"/moved/fusevision.db",args[2]+"/moved/held.db"),"hold previous active database");
                settings.setDbPath(args[2]+"/missing-target");check(settings.save(),"save missing-source target");
                check(!db.initDatabase() && !QFileInfo::exists(args[2]+"/missing-target/fusevision.db"),"missing last active source fails without fresh credentials");
                check(QFile::rename(args[2]+"/moved/held.db",args[2]+"/moved/fusevision.db"),"restore previous active source");
            } else if(action=="existing") {
                settings.setDbPath(args[2]+"/configured");check(settings.save() && db.initDatabase(),"reuse existing configured database");
                check(db.validateLogin("superadmin","wal-password").success,"existing target password retained");
            } else if(action=="blocked") {
                write(args[2]+"/blocked","file");settings.setDbPath(args[2]+"/blocked");check(settings.save(),"save blocked path");
                check(!db.initDatabase(),"unusable configured path must fail without fallback");
            } else {
                check(db.initDatabase(),"open configured database");
                const auto directory=action=="moved-reopen"?"moved":"configured";
                const auto password=action=="moved-reopen"?"wal-password":"kept-password";
                check(QDir::cleanPath(QSqlDatabase::database("fusevision_conn").databaseName())==QDir::cleanPath(args[2]+"/"+directory+"/fusevision.db"),"startup consumes saved database directory");
                check(db.validateLogin("superadmin",password).success && !db.validateLogin("superadmin","admin123").success,"migration/restart retains password");
                check(QFileInfo::exists(QCoreApplication::applicationDirPath()+"/data/fusevision.db"),"legacy source retained");
            }
            return 0;
        } catch(const std::exception& e) {std::cerr<<"FAIL database child: "<<e.what()<<std::endl;return 1;}
    }
    QApplication app(argc,argv);const auto fonts=QDir(qEnvironmentVariable("SystemRoot")).filePath("Fonts/");
    for(const auto& name:{"msyh.ttc","msyhbd.ttc","segoeui.ttf"}) QFontDatabase::addApplicationFont(fonts+name);
    app.setFont(QFont("Microsoft YaHei",10));QTemporaryDir tmp;check(tmp.isValid(),"temporary root");isolate(tmp.path());
    check(DatabaseManager::instance().initDatabase(":memory:"),"isolated widget account");SessionManager::instance().login(DatabaseManager::instance().singleAccount().id);
    int failures=0;
    const std::pair<const char*,void(*)(const QString&)> cases[]={{"storage-config",WorkflowRegression::datasetStorage},{"ISS-011",WorkflowRegression::invalidImport},{"ISS-012",WorkflowRegression::rotatedMove},{"ISS-013",WorkflowRegression::database},{"ISS-014",WorkflowRegression::classification}};
    for(const auto& test:cases) try {const auto root=tmp.path()+"/"+test.first;QDir().mkpath(root);test.second(root);std::cout<<"PASS "<<test.first<<std::endl;}
        catch(const std::exception& e) {++failures;std::cerr<<"FAIL "<<test.first<<": "<<e.what()<<std::endl;}
    return failures?1:0;
}
