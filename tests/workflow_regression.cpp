#include "DatasetFixture.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMessageBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QInputDialog>
#include <QColorDialog>
#include <QTabBar>
#include <QFontDatabase>
#include <QTableWidget>
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlDatabase>
#include "app/FuseVision.h"
#include "app/Login.h"
#include "app/UserManagementWidget.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include "deeplearning/widgets/AnnotationManagement.h"
#include "deeplearning/widgets/AnnotationCanvas.h"
#include "deeplearning/widgets/DatasetManagement.h"
#include "deeplearning/tools/SelectTool.h"
#include "deeplearning/tools/BrushTool.h"
#include "deeplearning/tools/EraserTool.h"
#include "deeplearning/tools/FillTool.h"
#include "deeplearning/tools/PolygonTool.h"
#include "core/DatabaseManager.h"
#include "core/PermissionRegistry.h"
#include "core/SessionManager.h"
#include "core/SettingsManager.h"
#include "deeplearning/widgets/ProjectManagement.h"
#include "deeplearning/storage/AnnotationStore.h"
#include "deeplearning/storage/DatasetFileService.h"
#include "deeplearning/widgets/ModelManagement.h"
#include "app/SystemSettingsWidget.h"
#include "core/Logger.h"
#include "core/AccessControl.h"
#include "ui/ThemeManager.h"
#include "ui/LogMonitor.h"
#include <QListWidget>
#include <QTextDocument>
#include "deeplearning/widgets/DeepLearningWidget.h"

static std::function<void(QDialog*)> dialogAction;

static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void writeFile(const QString& path, const QByteArray& data) {
    QFile f(path); check(f.open(QIODevice::WriteOnly), "fixture open");
    check(f.write(data) == data.size(), "fixture write");
}
static void drain() {
    QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec();
}

class WorkflowRegression {
public:
    static void singleAccountFlow(const QString& root) {
        auto& db=DatabaseManager::instance();auto& session=SessionManager::instance();session.logout();
        check(db.allUsers().size()==1,"fresh database has one account");
        check(!db.validateLogin("user","123").success && !db.validateLogin("admin","manager123").success,"old role credentials do not unlock");
        // Simulate an old database without deleting or changing legacy rows/passwords.
        QSqlQuery sql(QSqlDatabase::database("fusevision_conn"));
        check(sql.exec("INSERT INTO users(username,password_hash,role) SELECT 'user',password_hash,0 FROM users WHERE username='superadmin'"),"legacy account fixture");
        check(sql.exec("CREATE TABLE user_permissions(user_id INTEGER,module_name TEXT,can_read INTEGER,can_write INTEGER)"),"legacy permission fixture");
        check(sql.exec("INSERT INTO user_permissions SELECT id,'系统设置',0,0 FROM users WHERE username='superadmin'"),"legacy denied permission fixture");
        check(!db.validateLogin("user","admin123").success,"old account remains stored but cannot login");
        session.login(db.getUserId("user"));check(!session.isLoggedIn(),"old account cannot establish write session");
        check(!db.changePassword(db.getUserId("superadmin"),"admin123","unauthorized"),"guest cannot change password through data layer");
        FuseVision window;window.initPagesAsync();window.resize(1280,800);window.show();drain();
        check(window.m_switchUserBtn->toolTip().contains("登录") && window.m_userValueLabel->text().contains("只读"),"guest status and login action");
        for(int i=0;i<4;++i) {check(window.m_navBtns[i]->isEnabled(),"guest navigation enabled");window.m_navBtns[i]->click();check(window.m_stackedWidget->currentIndex()==i,"guest enters every main page");}
        window.m_navBtns[0]->click();
        auto* deep=window.m_deepLearningWidget;auto* tabs=deep->findChild<QTabBar*>();
        check(tabs && tabs->count()==9,"all deep learning tabs");
        for(int i=0;i<tabs->count();++i) {check(tabs->isTabVisible(i),"guest sees every tab");tabs->setCurrentIndex(i);check(tabs->currentIndex()==i,"guest enters every deep learning page");}
        auto* projects=deep->findChild<ProjectManagement*>();auto* models=deep->findChild<ModelManagement*>();auto* data=deep->findChild<DatasetManagement*>();auto* annotation=deep->findChild<AnnotationManagement*>();
        check(projects->m_openProjectBtn->isEnabled() && !projects->m_newProjectBtn->isEnabled(),"guest may open projects but cannot create");
        const auto projectPath=root+"/Fixture/project.fvproj",modelPath=root+"/Fixture/Detect/Detect.fvdl",configPath=root+"/Fixture/Detect/dataset_config.json";
        const auto projectBytes=read(projectPath),modelBytes=read(modelPath),configBytes=read(configPath);
        ProjectCard* pc=nullptr;for(auto* c:projects->m_cards) if(c->fvprojPath()==projectPath) pc=c;
        check(pc,"guest project fixture");projects->onCardDoubleClicked(pc);
        ModelCard* mc=nullptr;for(auto* c:models->m_cards) if(c->modelId()=="Detect") mc=c;
        check(mc,"guest model fixture");models->onCardDoubleClicked(mc);drain();
        const auto guestImage=data->imagesDir()+"/one.png";
        const int guestIndex=data->findSampleIndex(guestImage);check(guestIndex>=0,"guest image in gallery");
        data->onThumbnailDoubleClicked(guestIndex);
        check(tabs->currentIndex()==3 && annotation->findChild<AnnotationCanvas*>()->hasImage() && !annotation->hasUnsavedChanges(),"guest reads image and label through gallery");
        check(read(projectPath)==projectBytes && read(modelPath)==modelBytes && read(configPath)==configBytes,"guest browsing never saves business configuration");
        const auto labelPath=data->labelsDir()+"/one.json";const auto originalLabel=read(labelPath);
        auto* state=annotation->findChild<AnnotationModel*>();check(state,"guest annotation state");
        const auto originalShapes=state->shapes();
        QMetaObject::invokeMethod(annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(2,2)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        check(state->shapes()==originalShapes && !annotation->saveAnnotations() && read(labelPath)==originalLabel,"guest canvas and direct save cannot mutate labels");
        auto* account=window.m_userManagementWidget;
        check(account->findChildren<QTableWidget*>().isEmpty() && account->findChildren<QComboBox*>().isEmpty(),"no user list or permission matrix");
        auto* old=account->findChild<QLineEdit*>("oldPassword");auto* next=account->findChild<QLineEdit*>("newPassword");auto* confirm=account->findChild<QLineEdit*>("confirmPassword");
        check(old && next && confirm && !old->isEnabled(),"guest password fields locked");
        QMetaObject::invokeMethod(account,"onChangePassword");check(db.validateLogin("superadmin","admin123").success,"guest account handler cannot change password");
        const auto theme=ThemeManager::instance().currentTheme();window.onToggleTheme();check(ThemeManager::instance().currentTheme()==theme,"guest direct theme write denied");
        QSettings remembered(QSettings::defaultFormat(),QSettings::UserScope,"FuseVisionTeam","FuseVisionLogin");remembered.setValue("remember",true);remembered.setValue("lastUser","超级管理员");remembered.setValue("lastPassword","admin123");remembered.sync();
        dialogAction=[](QDialog* dialog) {if(auto* login=qobject_cast<LoginDialog*>(dialog)) login->reject();};window.onSwitchUser();dialogAction={};
        check(!session.isLoggedIn(),"cancel optional login stays guest");
        {LoginDialog login;check(login.findChildren<QComboBox*>().isEmpty(),"login has no role chooser");
        const auto evidence=qEnvironmentVariable("FUSEVISION_ACCOUNT_EVIDENCE_DIR");
        if(!evidence.isEmpty()) {QDir().mkpath(evidence);login.show();drain();check(login.grab().save(evidence+"/P00-single-account-20261117.png"),"login screenshot");}
        check(login.findChild<QLineEdit*>("loginAccount")->isReadOnly(),"single account name fixed");
        auto* password=login.findChild<QLineEdit*>("loginPassword");check(password->text().isEmpty(),"no remembered unlock password");password->setText("wrong");
        QMetaObject::invokeMethod(&login,"onLoginClicked");check(!session.isLoggedIn(),"wrong password leaves guest");}
        dialogAction=[](QDialog* dialog) {if(auto* login=qobject_cast<LoginDialog*>(dialog)) {login->findChild<QLineEdit*>("loginPassword")->setText("admin123");QMetaObject::invokeMethod(login,"onLoginClicked");}};
        window.onSwitchUser();dialogAction={};
        check(session.isLoggedIn() && projects->m_newProjectBtn->isEnabled() && old->isEnabled() && AccessControl::canWrite("系统设置"),"login unlocks all writes despite old denied permissions");
        check(window.m_userValueLabel->text().contains("可编辑"),"editable status");
        check(!remembered.contains("lastPassword") && !remembered.contains("remember"),"successful login removes legacy remembered plaintext");
        AnnotationShape shape;shape.type=AnnotationShape::RectShape;shape.labelId=0;shape.bbox={2,2,5,5};state->appendShape(shape);
        window.onSwitchUser();check(session.isLoggedIn() && annotation->hasUnsavedChanges(),"cancel logout preserves session and edits");
        const auto labelDir=data->labelsDir();check(QDir().rename(labelDir,labelDir+"-held"),"hold label directory");writeFile(labelDir,"blocks-save");
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {if(auto* save=box->button(QMessageBox::Save)) save->click();else box->done(QMessageBox::Ok);}};
        window.onSwitchUser();check(session.isLoggedIn() && annotation->hasUnsavedChanges(),"save failure prevents logout");
        check(QFile::remove(labelDir) && QDir().rename(labelDir+"-held",labelDir),"repair label directory");
        window.onSwitchUser();dialogAction={};check(!session.isLoggedIn() && !annotation->hasUnsavedChanges(),"successful save permits logout");
        check(read(labelPath)!=originalLabel && !projects->m_newProjectBtn->isEnabled() && window.m_stackedWidget->isVisible(),"saved data retained and guest pages stay visible");
        dialogAction=[](QDialog* dialog) {if(auto* login=qobject_cast<LoginDialog*>(dialog)) {login->findChild<QLineEdit*>("loginPassword")->setText("admin123");QMetaObject::invokeMethod(login,"onLoginClicked");}};
        window.onSwitchUser();dialogAction={};
        old->setText("wrong");next->setText("newpass123");confirm->setText("newpass123");QMetaObject::invokeMethod(account,"onChangePassword");check(db.validateLogin("superadmin","admin123").success,"wrong current password does not alter stored hash");
        old->setText("admin123");next->setText("newpass123");confirm->setText("mismatch");QMetaObject::invokeMethod(account,"onChangePassword");check(db.validateLogin("superadmin","admin123").success,"confirmation mismatch preserves password");
        confirm->setText("newpass123");QMetaObject::invokeMethod(account,"onChangePassword");check(db.validateLogin("superadmin","newpass123").success && !db.validateLogin("superadmin","admin123").success,"password changes for sole account");
        check(db.changePassword(session.currentUserId(),"newpass123","admin123"),"restore isolated test password");
        check(db.allUsers().size()==2 && sql.exec("SELECT can_read,can_write FROM user_permissions") && sql.next() && sql.value(0).toInt()==0 && sql.value(1).toInt()==0,"historical records and old permissions remain unchanged");
        sql.finish();
        check(sql.exec("BEGIN"),"begin fallback account fixture");
        check(sql.exec("UPDATE users SET username='primary' WHERE username='superadmin'"),"legacy custom account fixture");
        check(db.singleAccount().username=="primary" && db.validateLogin("primary","admin123").success,"existing database without superadmin reuses its highest-role account and password");
        check(sql.exec("ROLLBACK"),"restore legacy account fixture");
        const auto evidence=qEnvironmentVariable("FUSEVISION_ACCOUNT_EVIDENCE_DIR");
        if(!evidence.isEmpty()) {QDir().mkpath(evidence);window.m_navBtns[2]->click();check(window.grab().save(evidence+"/P12-single-account-20261117.png"),"account screenshot");}
        const auto savedLabel=read(labelPath);const auto savedCount=state->shapeCount();state->appendShape(shape);
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {if(auto* discard=box->button(QMessageBox::Discard)) discard->click();else box->done(QMessageBox::Ok);}};
        window.onSwitchUser();dialogAction={};
        check(!session.isLoggedIn() && !annotation->hasUnsavedChanges() && state->shapeCount()==savedCount && read(labelPath)==savedLabel,"discard logout restores persisted annotations without writing");
        if(!evidence.isEmpty()) check(window.grab().save(evidence+"/P01-guest-readonly-20261117.png"),"guest screenshot");
        window.close();
    }
    static void integratedFlow(const QString& root) {
        DeepLearningWidget deep;deep.resize(1400,900);deep.show();drain();
        auto* projects=deep.findChild<ProjectManagement*>();auto* models=deep.findChild<ModelManagement*>();auto* dataset=deep.findChild<DatasetManagement*>();auto* annotation=deep.findChild<AnnotationManagement*>();auto* tabs=deep.findChild<QTabBar*>();
        check(projects && models && dataset && annotation && tabs,"integrated widgets");
        ProjectCard* projectCard=nullptr;for(auto* card:projects->m_cards) if(card->fvprojPath().endsWith("/Fixture/project.fvproj")) projectCard=card;
        check(projectCard,"integrated project card");projects->onCardDoubleClicked(projectCard);
        ModelCard* modelCard=nullptr;for(auto* card:models->m_cards) if(card->modelId()=="Seg") modelCard=card;
        check(modelCard,"integrated model card");models->onCardDoubleClicked(modelCard);drain();
        const auto image=dataset->imagesDir()+"/a.png";const int index=dataset->findSampleIndex(image);
        check(dataset->currentModelId()=="Seg" && index>=0 && tabs->currentIndex()==2,"model signal binds dataset and opens gallery");
        dataset->onThumbnailDoubleClicked(index);check(tabs->currentIndex()==3,"thumbnail opens annotation tab");
        auto* state=annotation->findChild<AnnotationModel*>();check(state && !state->isDirty(),"integrated clean load");
        QMetaObject::invokeMethod(annotation,"onToolChanged",Q_ARG(int,int(AnnoTool::Rectangle)));
        QMetaObject::invokeMethod(annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(10,10)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        QMetaObject::invokeMethod(annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(25,25)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        check(state->shapeCount()==1 && annotation->hasUnsavedChanges(),"integrated edit");
        tabs->setCurrentIndex(2);check(tabs->currentIndex()==3 && state->isDirty(),"cancel tab change retains editor");
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {if(auto* b=box->button(QMessageBox::Save)) b->click();else box->done(QMessageBox::Ok);}};
        tabs->setCurrentIndex(2);dialogAction={};check(tabs->currentIndex()==2 && !state->isDirty(),"save then return to gallery through real coordinator");
        dataset->onThumbnailDoubleClicked(index);check(state->shapeCount()==1 && state->shapes()[0].bbox==QRectF(10,10,15,15),"integrated reopen preserves geometry");
        const auto evidence=qEnvironmentVariable("FUSEVISION_WORKFLOW_EVIDENCE_DIR");
        if(!evidence.isEmpty()) {QDir().mkpath(evidence);check(deep.grab().save(evidence+"/P05-workflow-reopened-20261115.png"),"capture repaired annotation widget");}
        QMetaObject::invokeMethod(annotation,"onReturnToGallery");check(tabs->currentIndex()==2,"return button signal");
        dataset->writeCocoJson(root+"/integrated-export/annotations.json",{index});
        auto coco=QJsonDocument::fromJson(read(root+"/integrated-export/annotations.json")).object();check(coco["images"].toArray().size()==1 && coco["annotations"].toArray().size()==1,"integrated save-reopen-export closes file workflow");
        if(!evidence.isEmpty()) check(deep.grab().save(evidence+"/P04-workflow-gallery-20261115.png"),"capture repaired gallery widget");
        dataset->m_selectedIndices={index};
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {if(auto* b=box->button(QMessageBox::Yes)) b->click();else box->done(QMessageBox::Ok);}};
        dataset->onDeleteSelected();dialogAction={};
        check(!QFileInfo::exists(image),"integrated sample file deletion committed");
        check(!annotation->findChild<AnnotationCanvas*>()->hasImage(),"successful sample deletion clears stale clean editor canvas");
        check(!annotation->saveAnnotations(),"cleared editor cannot save orphan labels");
        deep.hide();
    }
    static void toolMasks(const QString& root) {
        QImage image(40,30,QImage::Format_RGB32);image.fill(Qt::white);AnnotationModel model;
        AnnotationTool::Context ctx;ctx.sourceImage=image;ctx.model=&model;ctx.activeLabelId=0;ctx.brushSize=4;ctx.labelColors={"#ff0000","#00ff00"};
        BrushTool brush;brush.activated(ctx);brush.mousePress({3,3},Qt::LeftButton);brush.mouseRelease({3,3},Qt::LeftButton);
        check(model.shapeCount()==1 && model.shapes()[0].maskLayer.constScanLine(3)[3]>0,"single-click brush persists a dot");
        brush.mousePress({10,10},Qt::LeftButton);brush.mouseMove({15,10});brush.mouseRelease({15,10},Qt::LeftButton);
        check(model.shapes()[0].maskLayer.constScanLine(3)[3]>0 && model.shapes()[0].maskLayer.constScanLine(10)[12]>0,"brush strokes merge without erasing old pixels");
        EraserTool eraser;eraser.activated(ctx);eraser.mousePress({3,3},Qt::LeftButton);eraser.mouseRelease({3,3},Qt::LeftButton);
        check(model.shapes()[0].maskLayer.constScanLine(3)[3]==0,"single-click eraser");model.undo();check(model.shapes()[0].maskLayer.constScanLine(3)[3]>0,"undo eraser");
        FillTool fill;fill.activated(ctx);fill.mousePress({25,20},Qt::LeftButton);
        check(model.shapes()[0].maskLayer.constScanLine(3)[3]>0,"fill merge retains earlier strokes");
        const auto before=model.shapes();fill.mousePress({25,20},Qt::LeftButton);check(model.shapes()==before,"occupied fill seed is a no-op");
        const auto path=root+"/tools.png";check(AnnotationStore::save(path,"tools.png",image,model.shapes(),true,false,2).ok,"tool masks persist");
        QList<AnnotationShape> loaded;QString error;check(AnnotationStore::load(path,image.size(),false,{"a","b"},loaded,error) && loaded==model.shapes(),"tool mask pixels and identities round trip");
        PolygonTool polygon;ctx.activeLabelId=1;polygon.activated(ctx);polygon.mousePress({4,4},Qt::LeftButton);polygon.mousePress({30,4},Qt::LeftButton);polygon.mousePress({20,25},Qt::LeftButton);polygon.keyPress(Qt::Key_Return,Qt::NoModifier);
        check(model.shapeCount()==2 && model.shapes()[1].labelId==1,"polygon completes into shared model");
        check(AnnotationStore::save(path,"tools.png",image,model.shapes(),true,false,2).ok,"mixed tool save");check(AnnotationStore::load(path,image.size(),false,{"a","b"},loaded,error) && loaded==model.shapes(),"mixed geometry and masks retain categories and coordinates");
    }
    static QByteArray read(const QString& path) {QFile f(path);check(f.open(QIODevice::ReadOnly),"read fixture");return f.readAll();}
    static void externalEdits(const QString& root) {
        QImage image(10,8,QImage::Format_RGB32);image.fill(Qt::white);
        AnnotationShape shape;shape.type=AnnotationShape::RectShape;shape.labelId=0;shape.bbox={1,1,3,3};
        const auto path=root+"/external.png";
        check(AnnotationStore::save(path,"a.png",image,{shape},true,false,1).ok,"save external fixture");
        QImage pixels(path);pixels.fill(0);check(pixels.save(path),"external raster edit retains PNG text");
        QList<AnnotationShape> loaded;QString error;
        check(AnnotationStore::load(path,image.size(),false,{"cat"},loaded,error) && loaded.isEmpty(),"external pixel changes must not resurrect stale native geometry");
        const auto json=root+"/external.json";check(AnnotationStore::save(json,"a.png",image,{shape},false,false,1).ok,"JSON external fixture");
        auto doc=QJsonDocument::fromJson(read(json)).object();doc["annotations"]=QJsonArray{};writeFile(json,QJsonDocument(doc).toJson());
        check(AnnotationStore::load(json,image.size(),false,{"cat"},loaded,error) && loaded.isEmpty(),"external JSON edit invalidates native cache");
    }
    static void transferRecovery(const QString& root) {
        const auto source=root+"/transfer-source",target=root+"/transfer-target";
        QDir().mkpath(source);QDir().mkpath(target+"/images");QDir().mkpath(target+"/labels");
        QImage old(12,10,QImage::Format_RGB32);old.fill(Qt::red);check(old.save(target+"/images/a.png"),"old paired image");
        writeFile(target+"/labels/a.json","{\"annotations\":[]}");
        const auto oldImage=read(target+"/images/a.png"),oldLabel=read(target+"/labels/a.json");
        QImage image(12,10,QImage::Format_RGB32);image.fill(Qt::blue);check(image.save(source+"/a.png"),"new image");
        writeFile(source+"/a.json","{\"annotations\":[]}");
        const auto allowed=[] {return true;};
        auto report=DatasetFileService::importFiles({source+"/a.png"},{source+"/a.json"},target+"/images",target+"/labels",false,allowed);
        check(report.skipped.size()==1 && read(target+"/images/a.png")==oldImage && read(target+"/labels/a.json")==oldLabel,"skip keeps whole pair");
        writeFile(source+"/a.json","broken");
        report=DatasetFileService::importFiles({source+"/a.png"},{source+"/a.json"},target+"/images",target+"/labels",true,allowed);
        check(!report.ok() && read(target+"/images/a.png")==oldImage && read(target+"/labels/a.json")==oldLabel,"failed overwrite retains pair");
        writeFile(source+"/a.json","{\"annotations\":[]}");
        int checks=0;report=DatasetFileService::importFiles({source+"/a.png"},{source+"/a.json"},target+"/images",target+"/labels",true,[&] {return ++checks<2;});
        check(!report.ok() && read(target+"/images/a.png")==oldImage,"revocation before commit retains old image");
        report=DatasetFileService::importFiles({source+"/a.png"},{source+"/a.json"},target+"/images",target+"/labels",true,allowed);
        check(report.ok() && report.succeeded.size()==1 && read(target+"/images/a.png")==read(source+"/a.png"),"repair and retry pair overwrite");
        writeFile(target+"/labels/ab.json","keep-neighbor");
        report=DatasetFileService::removeSamples({target+"/images/a.png"},target+"/images",target+"/labels",target+"/trash",allowed);
        check(report.ok() && report.succeeded.size()==1 && !QFileInfo::exists(target+"/images/a.png") && !QFileInfo::exists(target+"/labels/a.json") && QFileInfo::exists(target+"/labels/ab.json"),"delete exact pair preserves prefix neighbor");
        const auto entries=QDir(target+"/trash").entryList(QDir::Dirs|QDir::NoDotAndDotDot);check(entries.size()==1,"sample recovery retained");
        const auto recovery=target+"/trash/"+entries[0];
        for(const auto& item:QJsonDocument::fromJson(read(recovery+"/restore.json")).array()) {
            const auto entry=item.toObject();check(QFile::rename(recovery+"/"+entry["recovery"].toString(),entry["original"].toString()),"restore paired deletion from manifest");
        }
        check(read(target+"/images/a.png")==read(source+"/a.png"),"restored image bytes");
        report=DatasetFileService::removeSamples({target+"/images/a.png"},target+"/images",target+"/labels",target+"/denied",[] {return false;});
        check(!report.ok() && QFileInfo::exists(target+"/images/a.png") && !QFileInfo::exists(target+"/denied"),"denied delete leaves filesystem unchanged");
    }
    static void cocoRoundTrip(const QString& root) {
        auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj");QString error;
        check(ModelConfig::createModel("CocoImport",ModelTaskTypes::InstanceSeg,project.projectRoot,project.fvprojPath,&error),"create COCO model");
        const auto model=ModelConfig::loadModel(project.projectRoot+"/CocoImport/CocoImport.fvdl");
        DatasetManagement dataset;dataset.setCurrentProject(project);dataset.setCurrentModel("CocoImport",model);drain();
        dataset.m_labelNames={"existing"};dataset.m_labelColors={"#ff0000"};dataset.m_labelInstanceCounts={0};check(dataset.saveDatasetConfig(),"persist stable class slot");
        const auto source=root+"/coco-source";QDir().mkpath(source);
        QImage image(10,8,QImage::Format_RGB32);image.fill(Qt::white);check(image.save(source+"/sparse.png"),"sparse COCO image");check(image.save(source+"/negative.png"),"negative COCO image");
        QJsonObject coco{{"categories",QJsonArray{QJsonObject{{"id",17},{"name","new"}}}},
            {"images",QJsonArray{QJsonObject{{"id",9},{"file_name","sparse.png"},{"width",10},{"height",8}},QJsonObject{{"id",12},{"file_name","negative.png"},{"width",10},{"height",8}}}},
            {"annotations",QJsonArray{QJsonObject{{"id",1},{"image_id",9},{"category_id",17},{"bbox",QJsonArray{1,2,3,4}}}}}};
        writeFile(source+"/annotations.json",QJsonDocument(coco).toJson());
        check(dataset.parseCocoJson(source+"/annotations.json"),"sparse COCO import succeeds");
        check(dataset.m_labelNames==QStringList{"existing","new"},"COCO maps sparse external IDs without renumbering existing categories");
        QList<AnnotationShape> shapes;check(AnnotationStore::load(dataset.labelsDir()+"/sparse.png",image.size(),false,dataset.m_labelNames,shapes,error) && shapes.size()==1 && shapes[0].labelId==1,"COCO native category and geometry");
        check(AnnotationStore::load(dataset.labelsDir()+"/negative.png",image.size(),false,dataset.m_labelNames,shapes,error) && shapes.isEmpty(),"COCO persists negative annotation");
        AnnotationShape mask;mask.type=AnnotationShape::BrushMask;mask.labelId=1;mask.maskLayer=QImage(image.size(),QImage::Format_Grayscale8);mask.maskLayer.fill(0);mask.maskLayer.scanLine(1)[2]=255;mask.maskLayer.scanLine(6)[7]=255;
        check(AnnotationStore::save(dataset.labelsDir()+"/sparse.png","sparse.png",image,{mask},true,false,2).ok,"RLE source mask");
        dataset.loadDatasetConfig();
        const auto report=DatasetFileService::exportFiles(DatasetFileService::Coco,root+"/rle-export/annotations.json",dataset.m_samples,{},dataset.labelsDir(),dataset.m_labelNames,dataset.m_labelType,model.modelType,[] {return true;});
        check(report.ok(),"RLE COCO export");
        auto exported=QJsonDocument::fromJson(read(root+"/rle-export/annotations.json")).object();
        QJsonArray rle;for(const auto& ann:exported["annotations"].toArray()) if(ann.toObject()["segmentation"].isObject()) rle.append(ann);
        check(rle.size()==1,"COCO mask has one RLE instance");
        writeFile(root+"/rle.json",QJsonDocument(QJsonObject{{"annotations",rle}}).toJson());
        check(AnnotationStore::load(root+"/rle.json",image.size(),false,dataset.m_labelNames,shapes,error) && shapes.size()==1 && shapes[0].maskLayer==mask.maskLayer,"independently decode exported column-major RLE");
        coco["images"]=QJsonArray{QJsonObject{{"id",9},{"file_name","../escape.png"}}};
        writeFile(source+"/bad.json",QJsonDocument(coco).toJson());const auto before=read(dataset.datasetDir()+"/dataset_config.json");
        check(!dataset.parseCocoJson(source+"/bad.json") && read(dataset.datasetDir()+"/dataset_config.json")==before,"unsafe COCO failure leaves category config unchanged");
        writeFile(dataset.imagesDir()+"/broken.png","not-an-image");
        writeFile(dataset.labelsDir()+"/orphan.json","{\"annotations\":[]}");
        writeFile(dataset.labelsDir()+"/negative.txt","99 0.5 0.5 0.2 0.2");dataset.loadDatasetConfig();
        auto stats=dataset.computeStats();dataset.runQualityCheck(stats);
        check(stats.corruptedFiles>=2 && stats.orphanAnnotations>=1 && stats.warnings.join("\n").contains("broken.png") && stats.warnings.join("\n").contains("negative.txt"),"quality checks unannotated corrupt images and illegal classes with paths");
    }
    static void selectedDeletion(const QString& root) {
        QString error;check(ProjectConfig::createProject("DeleteA","",root,&error),"create A");check(ProjectConfig::createProject("DeleteB","",root,&error),"create B");
        ProjectManagement widget;widget.refreshCards();
        auto card=[&](const QString& name) {for(auto* c:widget.m_cards) if(c->fvprojPath().contains("/"+name+"/")) return c;return static_cast<ProjectCard*>(nullptr);};
        check(card("DeleteA") && card("DeleteB"),"project cards");widget.onCardDoubleClicked(card("DeleteA"));
        int guarded=0,closed=0;widget.beforeContextChange=[&] {++guarded;return false;};QObject::connect(&widget,&ProjectManagement::projectClosed,[&] {++closed;});
        widget.onCardClicked(card("DeleteB"));widget.onDeleteProject();check(QFileInfo::exists(root+"/DeleteB/project.fvproj"),"cancel B delete");
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {for(auto* b:box->buttons()) if(b->text().contains("删除整个")) {b->click();return;}box->done(QMessageBox::Ok);}};
        widget.onDeleteProject();check(!QFileInfo::exists(root+"/DeleteB") && widget.m_currentProjectName=="DeleteA" && closed==0 && guarded==0,"A active B selected deletes B only");
        widget.onCardClicked(card("DeleteA"));widget.onDeleteProject();check(QFileInfo::exists(root+"/DeleteA") && guarded==1 && closed==0,"current project unsaved cancellation blocks deletion");
        widget.beforeContextChange=[] {return true;};widget.onDeleteProject();check(!widget.m_hasProject && closed==1,"current project clears context once");
        dialogAction={};
        auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj");
        check(ModelConfig::createModel("DeleteMA",ModelTaskTypes::InstanceSeg,project.projectRoot,project.fvprojPath,&error),"create MA");check(ModelConfig::createModel("DeleteMB",ModelTaskTypes::InstanceSeg,project.projectRoot,project.fvprojPath,&error),"create MB");
        ModelManagement models;models.setCurrentProject(ProjectConfig::loadProject(project.fvprojPath));
        auto modelCard=[&](const QString& id) {for(auto* c:models.m_cards) if(c->modelId()==id) return c;return static_cast<ModelCard*>(nullptr);};
        check(modelCard("DeleteMA") && modelCard("DeleteMB"),"model cards");models.onCardDoubleClicked(modelCard("DeleteMA"));
        int changes=0;QObject::connect(&models,&ModelManagement::currentModelChanged,[&] {++changes;});
        models.onCardClicked(modelCard("DeleteMB"));
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {for(auto* b:box->buttons()) if(b->text().contains("删除整个")) {b->click();return;}box->done(QMessageBox::Ok);}};
        writeFile(project.projectRoot+"/.fusevision-trash","blocks-recovery-directory");
        models.onDeleteModel();check(QFileInfo::exists(project.projectRoot+"/DeleteMB/DeleteMB.fvdl") && ProjectConfig::loadProject(project.fvprojPath).models.contains("DeleteMB") && changes==0,"failed model delete leaves files config and context");
        QFile::remove(project.projectRoot+"/.fusevision-trash");models.onDeleteModel();
        check(models.currentModelId()=="DeleteMA" && changes==0 && !ProjectConfig::loadProject(project.fvprojPath).models.contains("DeleteMB"),"delete selected noncurrent model preserves active model");
        models.onCardClicked(modelCard("DeleteMA"));models.beforeContextChange=[] {return false;};models.onDeleteModel();check(changes==0 && !models.currentModelId().isEmpty(),"current model cancellation");
        models.beforeContextChange=[] {return true;};models.onDeleteModel();check(models.currentModelId().isEmpty() && changes==1,"current model deletion clears downstream once");dialogAction={};
    }
    static void deniedHandlers(const QString& root) {
        auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj");auto model=ModelConfig::loadModel(project.projectRoot+"/Seg/Seg.fvdl");
        ProjectManagement projects;ModelManagement models;models.setCurrentProject(project);DatasetManagement dataset;dataset.setCurrentProject(project);dataset.setCurrentModel("Seg",model);drain();
        AnnotationManagement annotation;annotation.setDatasetSource(&dataset);check(annotation.openImage("Seg",dataset.imagesDir()+"/a.png"),"permission annotation fixture");
        auto* state=annotation.findChild<AnnotationModel*>();AnnotationShape s;s.type=AnnotationShape::RectShape;s.bbox={1,1,5,5};state->appendShape(s);
        const auto config=read(project.fvprojPath),labels=read(dataset.labelsDir()+"/a.png"),data=read(dataset.datasetDir()+"/dataset_config.json");
        const auto users=DatabaseManager::instance().allUsers().size();
        const QStringList keys{"深度学习.项目管理","深度学习.模型管理","深度学习.数据集管理","深度学习.数据标注","系统设置"};
        SessionManager::instance().logout();
        for(const auto& key:keys) check(AccessControl::canRead(key) && !AccessControl::canWrite(key),"guest can read every page but cannot write");
        projects.onNewProject();projects.onDeleteProject();models.onNewModel();models.onDeleteModel();
        dataset.onAddLabel();dataset.onImportLabels();dataset.m_selectedIndices={0};dataset.onDeleteSelected();
        dataset.importSelectedFiles({root+"/transfer-source/a.png"},{});check(!dataset.saveDatasetConfig(),"direct dataset save denied");
        check(!annotation.saveAnnotations() && state->isDirty(),"denied annotation save retains dirty data");
        QMetaObject::invokeMethod(&annotation,"onDeleteSelectedShapes");QMetaObject::invokeMethod(&annotation,"onUndo");check(state->shapeCount()==1,"denied edit handlers retain shapes");
        SystemSettingsWidget settings;settings.m_projectDataPathEdit->setText(root+"/unauthorized");settings.onSave();
        check(read(project.fvprojPath)==config && read(dataset.labelsDir()+"/a.png")==labels && read(dataset.datasetDir()+"/dataset_config.json")==data && DatabaseManager::instance().allUsers().size()==users,"denied handlers preserve protected file and database data");
        check(!projects.m_newProjectBtn->isEnabled(),"readonly button");
        const auto admin=DatabaseManager::instance().validateLogin("superadmin","admin123");
        SessionManager::instance().login(admin.userId);
        check(projects.m_newProjectBtn->isEnabled() && AccessControl::canWrite(keys[0]),"login immediately unlocks write controls");
        SessionManager::instance().logout();
        check(!projects.m_newProjectBtn->isEnabled() && AccessControl::canRead(keys[1]),"logout restores read-only without hiding pages");
    }
    static void exportSemantics(const QString& root) {
        QString error;
        auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj");
        check(ModelConfig::createModel("Detect",ModelTaskTypes::ObjDetectFree,project.projectRoot,project.fvprojPath,&error),"create export model");
        auto model=ModelConfig::loadModel(project.projectRoot+"/Detect/Detect.fvdl");
        const auto dataRoot=project.projectRoot+"/Detect";
        QImage image(100,80,QImage::Format_RGB32);image.fill(Qt::white);check(image.save(dataRoot+"/images/one.png"),"export image fixture");
        AnnotationShape rect;rect.type=AnnotationShape::RectShape;rect.labelId=0;rect.bbox={10,20,30,40};
        check(AnnotationStore::save(dataRoot+"/labels/one.json","one.png",image,{rect},false,false,1).ok,"native JSON fixture");
        DatasetManagement dataset;dataset.setCurrentProject(project);dataset.setCurrentModel("Detect",model);drain();
        dataset.m_labelNames={"cat"};dataset.m_labelColors={"#ff0000"};dataset.m_labelType=LabelTypes::CocoBBox;dataset.saveDatasetConfig();dataset.loadDatasetConfig();
        QDir().mkpath(root+"/export-coco");dataset.writeCocoJson(root+"/export-coco/annotations.json",{0});
        QFile file(root+"/export-coco/annotations.json");check(file.open(QIODevice::ReadOnly),"read COCO independently");
        const auto coco=QJsonDocument::fromJson(file.readAll()).object();
        const auto categories=coco["categories"].toArray();const auto anns=coco["annotations"].toArray();
        check(anns.size()==1 && categories.size()==1 && anns[0].toObject()["category_id"]==categories[0].toObject()["id"],"COCO class IDs must reference exported categories");
        check(QFileInfo::exists(root+"/export-coco/one.png"),"COCO export must include image pairing");
        dataset.writeYoloFormat(root+"/export-yolo",{0});
        QFile yolo(root+"/export-yolo/labels/train/one.txt");check(yolo.open(QIODevice::ReadOnly),"read YOLO independently");
        const auto parts=QString::fromUtf8(yolo.readAll()).trimmed().split(' ',Qt::SkipEmptyParts);
        check(parts.size()==5 && parts[0]=="0","native one-based category becomes zero-based YOLO");
        check(qAbs(parts[1].toDouble()-0.25)<0.00001 && qAbs(parts[2].toDouble()-0.5)<0.00001,"YOLO normalized coordinates");
        const auto before=read(root+"/export-coco/annotations.json");
        auto report=DatasetFileService::exportFiles(DatasetFileService::Coco,root+"/export-coco/annotations.json",dataset.m_samples,{},dataset.labelsDir(),dataset.m_labelNames,dataset.m_labelType,model.modelType,[] {return true;});
        check(!report.ok() && read(root+"/export-coco/annotations.json")==before,"existing export cannot be overwritten");
        AnnotationShape rotated=rect;rotated.type=AnnotationShape::RotatedRectShape;rotated.bbox={30,25,20,10};rotated.rotation=30;rotated.rotCenter={40,30};
        QByteArray obb;QStringList warnings;
        check(DatasetFileService::yoloText({rotated},image.size(),"obb",obb,warnings,error),"rotated YOLO OBB conversion");
        const auto coords=QString::fromUtf8(obb).trimmed().split(' ');
        check(coords.size()==9 && qAbs(coords[1].toDouble()-0.338397)<0.00001 && qAbs(coords[2].toDouble()-0.258373)<0.00001,"OBB corners preserve rotation in normalized coordinates");
        QByteArray detect;check(DatasetFileService::yoloText({rotated},image.size(),"detect",detect,warnings,error) && !warnings.isEmpty(),"loss of rotation is disclosed for detect");
        auto bad=rect;bad.bbox.moveLeft(95);check(AnnotationStore::save(dataRoot+"/labels/one.json","one.png",image,{bad},false,false,1).ok,"out-of-bounds source fixture");
        report=DatasetFileService::exportFiles(DatasetFileService::Yolo,root+"/failed-export",dataset.m_samples,{0},dataset.labelsDir(),dataset.m_labelNames,dataset.m_labelType,model.modelType,[] {return true;});
        check(!report.ok() && report.succeeded.isEmpty() && !QFileInfo::exists(root+"/failed-export"),"failed export never commits incomplete output");
        check(AnnotationStore::save(dataRoot+"/labels/one.json","one.png",image,{rect},false,false,1).ok,"repair export source");
        QImage second(100,80,QImage::Format_RGB32);second.fill(Qt::black);check(second.save(dataRoot+"/images/two.png"),"second export source");check(AnnotationStore::save(dataRoot+"/labels/two.json","two.png",second,{},false,false,1).ok,"empty label export fixture");
        dataset.loadDatasetConfig();
        report=DatasetFileService::exportFiles(DatasetFileService::Yolo,root+"/selected-export",dataset.m_samples,{1},dataset.labelsDir(),dataset.m_labelNames,dataset.m_labelType,model.modelType,[] {return true;});
        check(report.ok() && read(root+"/selected-export/labels/train/two.txt").isEmpty() && !QFileInfo::exists(root+"/selected-export/images/train/one.png"),"selected-only export preserves explicit empty annotations");
    }
    static void settingsAndLogs(const QString& root) {
        auto& settings=SettingsManager::instance();const auto active=settings.activeProjectDataPath();
        check(Logger::init(root+"/logger"),"isolated logger init");Logger::info("fixture");Logger::flush();
        const auto files=QDir(Logger::activeDirectory()).entryList(QDir::Files);
        check(files.size()==1,"daily logger fixture file");
        check(!files[0].endsWith(".log"),"reproduce original log cleanup extension mismatch");
        const auto old=root+"/logger/fusevision_2020-01-01";writeFile(old,"old");
        QFile oldFile(old);check(oldFile.open(QIODevice::ReadWrite),"open aged log");
        check(oldFile.setFileTime(QDateTime(QDate(2020,1,1),QTime(0,0)),QFileDevice::FileModificationTime),"age log");oldFile.close();
        check(SystemSettingsWidget::expiredLogs(root+"/logger","fusevision",QDateTime::currentDateTime().addDays(-30))==QStringList{old},"cleanup matches actual log names and protects active log");
        SystemSettingsWidget widget;widget.m_logPathEdit->setText(root+"/settings-logs");
        widget.m_projectDataPathEdit->setText(root+"/next-projects");widget.m_dbPathEdit->setText(root+"/next-db");
        widget.m_traditionalDataPathEdit->setText(root+"/next-traditional");widget.m_traditionalCameraPathEdit->setText(root+"/next-camera");
        widget.onSave();settings.load();
        check(settings.projectDataPath()==root+"/next-projects" && settings.activeProjectDataPath()==active,"configured and active paths differ until restart");
        check(!QFileInfo::exists(root+"/next-projects"),"saving paths must not migrate/create projects");
        check(Logger::activeDirectory()==root+"/settings-logs","log path applies immediately");
        const auto settingsDir=root+"/settings/FuseVisionTeam";
        check(QDir().rename(settingsDir,settingsDir+"-backup"),"temporarily block isolated settings path");writeFile(settingsDir,"blocked");
        widget.m_projectDataPathEdit->setText(root+"/failed-settings");widget.onSave();
        check(settings.projectDataPath()==root+"/next-projects","failed settings save restores in-memory values");
        QFile::remove(settingsDir);check(QDir().rename(settingsDir+"-backup",settingsDir),"restore isolated settings path");widget.onSave();settings.load();
        check(settings.projectDataPath()==root+"/failed-settings","settings save can retry after failure");
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) box->done(QMessageBox::No);};
        widget.onReset();dialogAction={};check(settings.projectDataPath()==root+"/failed-settings","cancel settings reset preserves values");
        // Retain initial runtime fixture settings for later tests without moving files.
        settings.setProjectDataPath(active);
    }
    static void categoryProtection(const QString& root) {
        auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj");
        auto model=ModelConfig::loadModel(project.projectRoot+"/Seg/Seg.fvdl");
        DatasetManagement dataset;dataset.setCurrentProject(project);dataset.setCurrentModel("Seg",model);drain();
        dataset.m_labelNames={"first","second"};dataset.m_labelColors={"#ff0000","#00ff00"};dataset.m_labelInstanceCounts={0,0};
        dataset.saveDatasetConfig();dataset.updateLabelPanel();
        auto* moved=dataset.m_labelListWidget->takeItem(0);dataset.m_labelListWidget->insertItem(1,moved);
        dataset.onLabelOrderChanged();
        check(dataset.currentLabelNames()==QStringList{"first","second"},"persisted annotations prevent category reorder");
        dataset.m_labelListWidget->setCurrentRow(0);
        dialogAction=[](QDialog* dialog) { if(auto* box=qobject_cast<QMessageBox*>(dialog)) box->done(QMessageBox::Yes); };
        dataset.onDeleteLabel();dialogAction={};
        check(dataset.currentLabelNames()==QStringList{"first","second"},"persisted annotations prevent category removal");
        QImage image(20,16,QImage::Format_RGB32);image.fill(Qt::white);check(image.save(dataset.imagesDir()+"/protected.png"),"category image fixture");
        AnnotationShape shape;shape.type=AnnotationShape::RectShape;shape.labelId=1;shape.bbox={2,2,8,7};
        auto secondShape=shape;secondShape.uuid=QUuid::createUuid();secondShape.bbox.moveLeft(10);
        const auto path=dataset.labelsDir()+"/protected.png";check(AnnotationStore::save(path,"protected.png",image,{shape,secondShape},true,false,2).ok,"category label fixture");const auto before=read(path);
        dialogAction=[](QDialog* dialog) {if(auto* input=qobject_cast<QInputDialog*>(dialog)) {input->setTextValue("renamed");input->accept();}};
        dataset.onEditLabel();dialogAction={};
        check(dataset.currentLabelNames()==QStringList{"renamed","second"} && read(path)==before,"renaming preserves persisted slot meaning and label bytes");
        dialogAction=[](QDialog* dialog) {if(auto* input=qobject_cast<QInputDialog*>(dialog)) {input->setTextValue("appended");input->accept();}else if(auto* color=qobject_cast<QColorDialog*>(dialog)) {color->setCurrentColor(Qt::blue);color->accept();}};
        dataset.onAddLabel();dialogAction={};dataset.loadDatasetConfig();
        check(dataset.currentLabelNames()==QStringList{"renamed","second","appended"} && read(path)==before,"append retains existing slots after reopen");
        QList<AnnotationShape> loaded;QString error;check(AnnotationStore::load(path,image.size(),false,dataset.currentLabelNames(),loaded,error) && loaded.size()==2 && loaded[0].labelId==1 && dataset.currentLabelNames()[loaded[0].labelId]=="second","existing annotation still references same category after operations");
        check(dataset.currentLabelInstanceCounts().value(1)==2 && dataset.m_samples[dataset.findSampleIndex(dataset.imagesDir()+"/protected.png")].instanceCount==2,"class and image instance counts rebuild from committed shapes rather than stale config or image totals");
    }
    static void additionalFunctions(const QString& root) {
        auto& theme=ThemeManager::instance();const auto original=theme.currentTheme();
        {
            FuseVision window;window.onToggleTheme();
            check(theme.currentTheme()!=original,"authenticated theme toggle applies");
            QSettings persisted("FuseVisionTeam","FuseVision");persisted.sync();
            check(persisted.value("ui/theme").toString()==(theme.currentTheme()==ThemeManager::Dark?"dark":"light"),"theme persists in isolated settings");
            window.onToggleTheme();check(theme.currentTheme()==original,"theme toggles back");
        }
        LogMonitor monitor;auto* log=monitor.findChild<QTextEdit*>("logView");
        check(log && log->isReadOnly(),"log panel is read-only");
        monitor.log("<b>literal & message</b>",LogMonitor::Warning);
        check(log->toPlainText().contains("<b>literal & message</b>") && log->toPlainText().contains("[WARN]"),"log escapes external markup and preserves severity");
        monitor.collapse();check(!monitor.isExpanded() && log->isHidden(),"log collapse");
        monitor.expand();check(monitor.isExpanded() && !log->isHidden(),"log expand");
        for(int i=0;i<1100;++i) monitor.log(QString::number(i));
        check(log->document()->blockCount()<=1000 && log->toPlainText().contains("1099"),"log retains newest entries within limit");
        monitor.clear();check(log->toPlainText().isEmpty(),"log clears display");

        QString error;check(ProjectConfig::createProject("Coverage","",root,&error),"coverage project");
        const auto project=ProjectConfig::loadProject(root+"/Coverage/project.fvproj",&error);
        check(ModelConfig::createModel("Samples",ModelTaskTypes::InstanceSeg,project.projectRoot,project.fvprojPath,&error),"coverage model");
        const auto dir=project.projectRoot+"/Samples",config=dir+"/dataset_config.json";
        writeFile(config,QJsonDocument(datasetFixture(LabelTypes::GrayscaleMask,QJsonArray{QJsonObject{{"name","first"}},QJsonObject{{"name","second"}}})).toJson());
        DatasetManagement data;data.setCurrentProject(project);data.setCurrentModel("Samples",ModelConfig::loadModel(dir+"/Samples.fvdl",&error));drain();
        const auto before=read(config);int notifications=0;QObject::connect(&data,&DatasetManagement::labelsChanged,[&]{++notifications;});
        const auto reverse=[&]{auto* item=data.m_labelListWidget->takeItem(0);data.m_labelListWidget->insertItem(1,item);data.onLabelOrderChanged();};
        check(QFile::rename(config,config+".held") && QDir().mkdir(config),"block isolated config commit");
        reverse();check(data.currentLabelNames()==QStringList{"first","second"} && notifications==0,"failed reorder restores names without notification");
        check(data.m_labelListWidget->item(0)->data(Qt::UserRole).toInt()==0,"failed reorder restores displayed order");
        check(QDir().rmdir(config) && QFile::rename(config+".held",config) && read(config)==before,"failed reorder preserves committed config");
        reverse();data.loadDatasetConfig();check(data.currentLabelNames()==QStringList{"second","first"} && notifications==1,"reorder retry persists current order once");
        QImage image(50,40,QImage::Format_RGB32);image.fill(Qt::white);
        check(image.save(dir+"/images/a.png") && image.save(dir+"/images/b.png"),"filter image fixtures");
        AnnotationShape shape;shape.type=AnnotationShape::RectShape;shape.labelId=0;shape.bbox={2,2,10,10};
        check(AnnotationStore::save(dir+"/labels/a.png","a.png",image,{shape},true,false,2).ok,"filter annotated fixture");
        data.loadDatasetConfig();check(data.allSamples().size()==2,"filter scans two samples");
        data.m_filterCombo->setCurrentIndex(1);data.onSelectAll(Qt::Checked);
        check(data.m_selectedIndices==QSet<int>{1},"unannotated filter selects visible sample only");
        data.m_filterCombo->setCurrentIndex(2);check(data.m_selectedIndices.isEmpty(),"filter switch clears selection");
        data.onSelectAll(Qt::Checked);check(data.m_selectedIndices==QSet<int>{0},"annotated filter selects visible sample only");
        data.m_filterCombo->setCurrentIndex(0);data.onSelectAll(Qt::Checked);check(data.m_selectedIndices.size()==2,"all filter selects both");
        data.onSelectAll(Qt::Unchecked);check(data.m_selectedIndices.isEmpty(),"unselect clears selection");
    }
    static void modelCreation(const QString& root) {
        auto project=ProjectConfig::loadProject(root+"/Fixture/project.fvproj");
        ModelManagement models;models.setCurrentProject(project);
        int created=0,changed=0;QObject::connect(&models,&ModelManagement::modelCreated,[&]{++created;});
        QObject::connect(&models,&ModelManagement::currentModelChanged,[&]{++changed;});
        dialogAction=[](QDialog* dialog) {
            if(qobject_cast<QMessageBox*>(dialog)) {dialog->reject();return;}
            for(auto* edit:dialog->findChildren<QLineEdit*>()) if(!edit->isReadOnly()) {edit->setText("Created");break;}
            for(auto* edit:dialog->findChildren<QTextEdit*>()) edit->setPlainText("中文模型描述");
            dialog->accept();
        };
        models.onNewModel();dialogAction={};
        check(created==1 && changed==1,"model creation must notify once");
        check(models.currentModelId()=="Created","new model context");
        check(ModelConfig::loadModel(project.projectRoot+"/Created/Created.fvdl").description=="中文模型描述","model description persists");
        dialogAction=[](QDialog* dialog) {dialog->reject();};models.onNewModel();dialogAction={};
        check(created==1 && changed==1 && models.currentModelId()=="Created","cancel model creation sends no success and retains context");
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    // Offscreen has no native font discovery; use installed fonts for reviewable captures.
    const auto fonts=QDir(qEnvironmentVariable("SystemRoot")).filePath("Fonts/");
    for(const auto& name:{"msyh.ttc","msyhbd.ttc","segoeui.ttf"})
        if(QFileInfo::exists(fonts+name)) QFontDatabase::addApplicationFont(fonts+name);
    app.setFont(QFont("Microsoft YaHei",10));
#endif
    QTemporaryDir temp;
    check(temp.isValid(), "temporary directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path()+"/settings");
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temp.path()+"/system-settings");
    SettingsManager::instance().setProjectDataPath(temp.path());
    check(DatabaseManager::instance().initDatabase(":memory:"), "isolated database");
    for (const auto& key : {"深度学习.项目管理", "深度学习.模型管理", "深度学习.数据集管理", "深度学习.数据标注", "系统设置"})
        PermissionRegistry::instance().registerModule(QString::fromUtf8(key));
    auto admin = DatabaseManager::instance().validateLogin("superadmin", "admin123");
    check(admin.success, "seed administrator");
    SessionManager::instance().login(admin.userId);
    QTimer modalDriver;
    QObject::connect(&modalDriver,&QTimer::timeout,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if(dialog && dialogAction) {dialogAction(dialog);return;}
        if(auto* box=qobject_cast<QMessageBox*>(dialog)) {
            std::cerr << "DIALOG " << box->text().toStdString() << std::endl;
            box->done(box->standardButtons().testFlag(QMessageBox::Cancel)?QMessageBox::Cancel:QMessageBox::Ok);
        }
    });
    modalDriver.start(10);
    int failed = 0;
    auto run = [&](const char* name, auto test) {
        dialogAction={};SessionManager::instance().login(admin.userId);
        try { test(); std::cout << "PASS " << name << std::endl; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << std::endl; }
        dialogAction={};SessionManager::instance().login(admin.userId);
    };
    run("annotation model undo/redo", [&] {
        AnnotationModel m;
        AnnotationShape s; s.type=AnnotationShape::RectShape; s.labelId=0; s.bbox={10,10,30,30};
        m.appendShape(s); check(m.isDirty() && m.shapeCount()==1, "append dirty");
        m.undo(); check(m.isEmpty(), "undo append");
        m.redo(); check(m.shapeCount()==1, "redo append");
    });
    run("widget load edit save reopen", [&] {
        QString error;
        check(ProjectConfig::createProject("Fixture", "", temp.path(), &error), "create project");
        auto project=ProjectConfig::loadProject(temp.path()+"/Fixture/project.fvproj", &error);
        check(ModelConfig::createModel("Seg", ModelTaskTypes::InstanceSeg, project.projectRoot, project.fvprojPath, &error), "create model");
        auto model=ModelConfig::loadModel(project.projectRoot+"/Seg/Seg.fvdl", &error);
        const auto root=project.projectRoot+"/Seg";
        writeFile(root+"/dataset_config.json",QJsonDocument(datasetFixture(LabelTypes::GrayscaleMask,QJsonArray{QJsonObject{{"name","defect"},{"color","#ff0000"}}})).toJson());
        QImage image(80,60,QImage::Format_RGB32); image.fill(Qt::white);
        check(image.save(root+"/images/a.png"), "save image");
        QImage mask(80,60,QImage::Format_Grayscale8); mask.fill(0); mask.scanLine(5)[5]=1;
        check(mask.save(root+"/labels/a.png"), "save mask");
        DatasetManagement dataset; dataset.setCurrentProject(project); dataset.setCurrentModel("Seg",model); drain();
        AnnotationManagement annotation; annotation.setDatasetSource(&dataset); annotation.openImage("Seg",root+"/images/a.png");
        auto* m=annotation.findChild<AnnotationModel*>(); check(m, "model child");
        check(!m->isDirty() && !annotation.hasUnsavedChanges(), "loaded image must be clean");
        check(QMetaObject::invokeMethod(&annotation,"onToolChanged", Q_ARG(int,int(AnnoTool::Rectangle))), "rectangle slot");
        QMetaObject::invokeMethod(&annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(30,20)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        QMetaObject::invokeMethod(&annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(60,45)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        check(m->shapeCount()==2 && annotation.hasUnsavedChanges(), "rectangle must update authoritative data");
        QMetaObject::invokeMethod(&annotation,"onUndo"); check(m->shapeCount()==1,"widget undo");
        QMetaObject::invokeMethod(&annotation,"onRedo"); check(m->shapeCount()==2,"widget redo");
        QMetaObject::invokeMethod(&annotation,"onSave");
        check(!m->isDirty() && !annotation.hasUnsavedChanges(), "saved image must be clean");
        QImage saved(root+"/labels/a.png"); check(saved.convertToFormat(QImage::Format_Grayscale8).constScanLine(30)[40]==1, "new rectangle pixels persisted");
        annotation.openImage("Seg",root+"/images/a.png");
        check(m->shapeCount()==2, "reopen preserves shape semantics");
        QMetaObject::invokeMethod(&annotation,"onToolChanged",Q_ARG(int,int(AnnoTool::Select)));
        const auto oldBox=m->shapes()[1].bbox;
        QMetaObject::invokeMethod(&annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(44,32)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        QMetaObject::invokeMethod(&annotation,"onCanvasMouseMoved",Q_ARG(QPointF,QPointF(49,35)));
        QMetaObject::invokeMethod(&annotation,"onCanvasMouseReleased",Q_ARG(QPointF,QPointF(49,35)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        check(m->shapes()[1].bbox==oldBox.translated(5,3),"selection transaction retains drag");
        m->undo(); check(m->shapes()[1].bbox==oldBox,"undo drag");
        m->redo(); check(m->shapes()[1].bbox==oldBox.translated(5,3),"redo drag");
        const auto beforeEscape=m->shapes();
        QMetaObject::invokeMethod(&annotation,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(49,35)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
        QMetaObject::invokeMethod(&annotation,"onCanvasMouseMoved",Q_ARG(QPointF,QPointF(52,38)));
        QMetaObject::invokeMethod(&annotation,"onCanvasKeyPressed",Q_ARG(int,int(Qt::Key_Escape)),Q_ARG(Qt::KeyboardModifiers,Qt::NoModifier));
        check(m->shapes()==beforeEscape,"Escape cancels selection preview without committing");
        auto* list=annotation.findChild<QListWidget*>(); check(list && list->count()==2,"shape list synchronized");
        list->item(1)->setSelected(true); QMetaObject::invokeMethod(&annotation,"onDeleteSelectedShapes");
        check(m->shapeCount()==1,"delete uses model"); m->undo(); check(m->shapeCount()==2,"undo delete");
        int notifications=0;
        QObject::connect(&dataset,&DatasetManagement::annotationSaved,[&]{++notifications;});
        check(QDir().rename(root+"/labels",root+"/labels-backup"),"move fixture labels");
        writeFile(root+"/labels","blocked");
        check(!annotation.saveAnnotations(),"failed save must be reported");
        check(annotation.hasUnsavedChanges() && m->shapeCount()==2 && notifications==0,"failed save retains edits and statistics");
        check(!annotation.prepareToLeave(),"cancel must retain context");
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) box->done(box->standardButtons().testFlag(QMessageBox::Save)?QMessageBox::Save:QMessageBox::Ok);};
        check(!annotation.prepareToLeave() && annotation.hasUnsavedChanges(),"save failure blocks leave");dialogAction={};
        QFile::remove(root+"/labels"); check(QDir().rename(root+"/labels-backup",root+"/labels"),"restore fixture labels");
        check(annotation.saveAnnotations() && !annotation.hasUnsavedChanges() && notifications==1,"retry commits once");
        m->deleteShapes({0,1}); check(annotation.saveAnnotations(),"empty annotation save");
        annotation.openImage("Seg",root+"/images/a.png"); check(m->isEmpty(),"empty annotation must replace old shapes");
        AnnotationShape extra;extra.type=AnnotationShape::RectShape;extra.bbox={4,4,10,10};m->appendShape(extra);
        dialogAction=[](QDialog* dialog) {if(auto* box=qobject_cast<QMessageBox*>(dialog)) {if(auto* b=box->button(QMessageBox::Discard)) b->click();else box->done(QMessageBox::Ok);}};
        check(annotation.prepareToLeave() && m->isEmpty() && !m->isDirty(),"discard restores last saved state");dialogAction={};
    });
    run("JSON rotated and polygon round trip and failed replacement",[&] {
        QImage image(80,60,QImage::Format_RGB32); image.fill(Qt::white);
        AnnotationShape rectangle; rectangle.type=AnnotationShape::RotatedRectShape; rectangle.labelId=1;
        rectangle.bbox={12,13,20,21};rectangle.rotation=30;rectangle.rotCenter={22,23.5};
        AnnotationShape polygon;polygon.type=AnnotationShape::PolygonShape;polygon.labelId=0;
        polygon.polygon<<QPointF(1,1)<<QPointF(25,2)<<QPointF(5,28);
        const QList<AnnotationShape> shapes{rectangle,polygon};const auto path=temp.path()+"/roundtrip.json";
        check(AnnotationStore::save(path,"a.png",image,shapes,false,false,2).ok,"JSON save");
        QList<AnnotationShape> loaded;QString error;
        check(AnnotationStore::load(path,image.size(),false,{"a","b"},loaded,error),"JSON load");
        check(loaded==shapes,"JSON shape identity and coordinates");
        QFile f(path);check(f.open(QIODevice::ReadOnly),"read committed JSON");auto original=f.readAll();f.close();
        auto invalid=shapes;invalid[0].labelId=5;
        check(!AnnotationStore::save(path,"a.png",image,invalid,false,false,2).ok,"reject invalid category");
        check(f.open(QIODevice::ReadOnly) && f.readAll()==original,"failed replacement preserves bytes");
    });
    run("theme logs filters and reorder recovery",[&] {WorkflowRegression::additionalFunctions(temp.path());});
    run("category protection",[&] {WorkflowRegression::categoryProtection(temp.path());});
    run("model creation signals",[&] {WorkflowRegression::modelCreation(temp.path());});
    run("export semantic baseline",[&] {WorkflowRegression::exportSemantics(temp.path());});
    run("external PNG edit",[&] {WorkflowRegression::externalEdits(temp.path());});
    run("brush eraser fill polygon masks",[&] {WorkflowRegression::toolMasks(temp.path());});
    run("paired import recovery",[&] {WorkflowRegression::transferRecovery(temp.path());});
    run("COCO sparse IDs empty labels RLE and quality",[&] {WorkflowRegression::cocoRoundTrip(temp.path());});
    run("selected project and model deletion",[&] {WorkflowRegression::selectedDeletion(temp.path());});
    run("guest denied handlers and login state",[&] {WorkflowRegression::deniedHandlers(temp.path());});
    run("settings and actual log retention",[&] {WorkflowRegression::settingsAndLogs(temp.path());});
    run("integrated project model gallery editor reopen export",[&] {WorkflowRegression::integratedFlow(temp.path());});
    run("guest pages single account login logout and password",[&] {WorkflowRegression::singleAccountFlow(temp.path());});
    return failed ? 1 : 0;
}
