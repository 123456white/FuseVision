#include "DatasetFixture.h"
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QPushButton>
#include <QFontDatabase>
#include <iostream>
#include "deeplearning/widgets/AnnotationCanvas.h"
#include "deeplearning/domain/DatasetTypes.h"
#include "deeplearning/widgets/AnnotationManagement.h"
#include "deeplearning/tools/RectTool.h"
#include "deeplearning/tools/SelectTool.h"
#include "deeplearning/storage/ProjectConfig.h"
#include "deeplearning/storage/ModelConfig.h"
#include "deeplearning/widgets/DatasetManagement.h"
#include "core/DatabaseManager.h"
#include "core/SessionManager.h"
#include "core/SettingsManager.h"

static QByteArray read(const QString& path) {QFile f(path);if(!f.open(QIODevice::ReadOnly)) return {};return f.readAll();}

static int failures = 0;
static void check(bool ok, const char* message) {
    std::cout << (ok ? "PASS " : "FAIL ") << message << std::endl;
    if (!ok) ++failures;
}
static void mouse(AnnotationCanvas& canvas, QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, QPointF(120,120), QPointF(120,120), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &event);
}
static void key(AnnotationCanvas& canvas, QEvent::Type type, int value) {
    QKeyEvent event(type,value,Qt::NoModifier); QApplication::sendEvent(&canvas,&event);
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    // Windows offscreen has no native font discovery; match the workflow harness.
    const auto fontRoot=QDir(qEnvironmentVariable("SystemRoot")).filePath("Fonts/");
    for(const auto& name:{"msyh.ttc","msyhbd.ttc","segoeui.ttf"})
        if(QFile::exists(fontRoot+name)) QFontDatabase::addApplicationFont(fontRoot+name);
    app.setFont(QFont("Microsoft YaHei",10));
    QTemporaryDir temp;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path()+"/settings");
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temp.path()+"/system-settings");
    SettingsManager::instance().setProjectDataPath(temp.path());
    if(!DatabaseManager::instance().initDatabase(":memory:")) return 2;
    const auto login=DatabaseManager::instance().validateLogin("superadmin","admin123");
    if(!login.success) return 2;
    SessionManager::instance().login(login.userId);
    QTimer modalDriver;
    QObject::connect(&modalDriver,&QTimer::timeout,[](){
        if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            box->done(box->standardButtons().testFlag(QMessageBox::Cancel)?QMessageBox::Cancel:QMessageBox::Ok);
    });modalDriver.start(10);
    AnnotationCanvas canvas; canvas.resize(800,600); canvas.show(); app.processEvents();
    QImage image(1000,800,QImage::Format_RGB32); image.fill(Qt::black); canvas.setImage(image);
    check(canvas.hasMouseTracking() && canvas.focusPolicy()==Qt::StrongFocus,"hover preview and keyboard focus enabled");
    int releases=0,moves=0;
    QObject::connect(&canvas,&AnnotationCanvas::mouseReleased,[&](){++releases;});
    QObject::connect(&canvas,&AnnotationCanvas::mouseMoved,[&](){++moves;});
    for(bool spaceFirst : {false,true}) {
        key(canvas,QEvent::KeyPress,Qt::Key_Space);
        mouse(canvas,QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton);
        if(spaceFirst) key(canvas,QEvent::KeyRelease,Qt::Key_Space);
        mouse(canvas,QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton);
        if(!spaceFirst) key(canvas,QEvent::KeyRelease,Qt::Key_Space);
        mouse(canvas,QEvent::MouseMove,Qt::NoButton,Qt::NoButton);
    }
    check(releases==0 && moves==2,"space pan consumes its release and always ends");
    canvas.setZoom(2.0);const auto center=canvas.mapToScene(canvas.viewport()->rect().center());
    canvas.setImage(image,true);
    check(canvas.zoomLevel()==2.0 && QLineF(center,canvas.mapToScene(canvas.viewport()->rect().center())).length()<2,"image refresh preserves view");
    QWheelEvent zero({150,150},{150,150},{},{},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(canvas.viewport(),&zero);
    check(canvas.zoomLevel()==2.0,"zero wheel delta does not zoom");
    const auto anchor=canvas.mapToScene(QPoint(150,150));
    QWheelEvent wheel({150,150},{150,150},{},{0,120},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(canvas.viewport(),&wheel);
    check(QLineF(anchor,canvas.mapToScene(QPoint(150,150))).length()<2,"wheel zoom holds mouse anchor");
    AnnotationModel model; RectTool rect; AnnotationTool::Context ctx;ctx.model=&model;rect.updateContext(ctx);
    rect.mousePress({10,10},Qt::LeftButton);rect.mouseMove({40,50});rect.mouseRelease({40,50},Qt::LeftButton);
    check(model.shapes().size()==1 && !rect.isDrawing(),"drag rectangle commits one shape");
    rect.cancelDrawing();model.reset();
    rect.mousePress({10,10},Qt::LeftButton);rect.mouseRelease({10,10},Qt::LeftButton);
    rect.mouseMove({40,50});rect.mousePress({40,50},Qt::LeftButton);rect.mouseRelease({40,50},Qt::LeftButton);
    check(model.shapes().size()==1 && model.undoStack()->count()==1,"two-click rectangle remains compatible");
    model.reset();rect.mousePress({60,60},Qt::LeftButton);rect.mouseRelease({60,60},Qt::LeftButton);
    rect.mouseMove({30,20});rect.keyPress(Qt::Key_Return,Qt::NoModifier);
    check(model.shapeCount()==1 && model.shapes()[0].bbox==QRectF(30,20,30,40),"Enter completes reverse-direction rectangle");
    AnnotationShape rotated;rotated.type=AnnotationShape::RotatedRectShape;rotated.bbox={80,95,40,10};rotated.rotCenter={100,100};rotated.rotation=90;
    model.reset({rotated});
    check(model.hitTestShape({100,117})==0 && model.hitTestShape({117,100})==-1,"rotated rectangle hit test follows rendered orientation");
    model.reset();AnnotationShape box;box.type=AnnotationShape::RectShape;box.bbox={10,10,100,100};model.appendShape(box);
    {AnnotationModel::Transaction tx(&model,"resize");model.setBBoxVertex(0,0,{30,30});}
    check(model.shapes()[0].bbox==QRectF(30,30,80,80),"corner drag can shrink rectangle");
    AnnotationManagement editor;
    check(editor.metaObject()->indexOfMethod("onLaunchLabelMe()")<0,"native editor has no external launch entry");
    QString error;
    if(!ProjectConfig::createProject("NativeFixture","",temp.path(),&error)) return 2;
    const auto project=ProjectConfig::loadProject(temp.path()+"/NativeFixture/project.fvproj");
    if(!ModelConfig::createModel("Seg",ModelTaskTypes::InstanceSeg,project.projectRoot,project.fvprojPath,&error)) return 2;
    const auto root=project.projectRoot+"/Seg";
    QFile config(root+"/dataset_config.json");if(!config.open(QIODevice::WriteOnly)) return 2;
    config.write(QJsonDocument(datasetFixture(LabelTypes::GrayscaleMask,QJsonArray{QJsonObject{{"name","defect"},{"color","#ff0000"}}})).toJson());config.close();
    if(!image.save(root+"/images/a.png") || !image.save(root+"/images/b.png")) return 2;
    DatasetManagement dataset;dataset.setCurrentProject(project);dataset.setCurrentModel("Seg",ModelConfig::loadModel(root+"/Seg.fvdl"));
    QEventLoop loaded;QTimer::singleShot(100,&loaded,&QEventLoop::quit);loaded.exec();
    check(dataset.currentLabelNames()==QStringList{"defect"},"dataset async category initialization completes");
    editor.setDatasetSource(&dataset);editor.resize(1440,900);editor.show();app.processEvents();
    if(!editor.openImage("Seg",root+"/images/a.png")) return 2;
    auto* state=editor.findChild<AnnotationModel*>();auto* list=editor.findChild<QListWidget*>("annoShapeList");
    auto* view=editor.findChild<AnnotationCanvas*>();
    QMetaObject::invokeMethod(&editor,"onToolChanged",Q_ARG(int,int(AnnoTool::Rectangle)));
    QMetaObject::invokeMethod(&editor,"onCanvasMousePressed",Q_ARG(QPointF,QPointF(10,10)),Q_ARG(Qt::MouseButton,Qt::LeftButton));
    QMetaObject::invokeMethod(&editor,"onSaveAndNext");
    check(editor.hasUnsavedChanges() && state->isEmpty() && !QFile::exists(root+"/labels/a.png"),"unfinished shape blocks save-and-next without discarding preview");
    QMetaObject::invokeMethod(&editor,"onCanvasKeyPressed",Q_ARG(int,int(Qt::Key_Escape)),Q_ARG(Qt::KeyboardModifiers,Qt::NoModifier));
    box.bbox={40,40,120,120};state->appendShape(box);
    list->item(0)->setSelected(true);auto* originalItem=list->item(0);
    state->translateShape(0,{1,1});
    check(list->selectedItems().size()==1 && list->item(0)==originalItem,"model update preserves UUID selection and list item");
    QMetaObject::invokeMethod(&editor,"onToolChanged",Q_ARG(int,int(AnnoTool::Select)));
    auto press=[&](QPointF p){QMetaObject::invokeMethod(&editor,"onCanvasMousePressed",Q_ARG(QPointF,p),Q_ARG(Qt::MouseButton,Qt::LeftButton));};
    auto move=[&](QPointF p){QMetaObject::invokeMethod(&editor,"onCanvasMouseMoved",Q_ARG(QPointF,p));};
    auto release=[&](QPointF p){QMetaObject::invokeMethod(&editor,"onCanvasMouseReleased",Q_ARG(QPointF,p),Q_ARG(Qt::MouseButton,Qt::LeftButton));};
    const auto undoCount=state->undoStack()->count();press({100,100});move({110,110});move({120,120});release({120,120});
    check(list->selectedItems().size()==1 && state->undoStack()->count()==undoCount+1,"canvas drag synchronizes list and creates one undo");
    const auto moved=state->shapes();press({120,120});move({130,130});
    QFocusEvent focusOut(QEvent::FocusOut);QApplication::sendEvent(view,&focusOut);
    check(state->shapes()==moved,"focus loss restores uncommitted drag");
    press({100,100});release({100,100});QMetaObject::invokeMethod(&editor,"onDeleteSelectedShapes");
    check(state->isEmpty(),"canvas selection deletes intended object");state->undo();
    view->setZoom(2.0);const auto viewCenter=view->mapToScene(view->viewport()->rect().center());
    QMetaObject::invokeMethod(&editor,"onBrightnessChanged",Q_ARG(int,20));
    check(view->zoomLevel()==2.0 && QLineF(viewCenter,view->mapToScene(view->viewport()->rect().center())).length()<2,"brightness change preserves zoom and center");
    const auto shapes=state->shapes();const auto dirty=state->isDirty();
    editor.findChild<QPushButton*>("annoOverlayBtn")->click();
    check(state->shapes()==shapes && state->isDirty()==dirty,"original-image toggle leaves annotation unchanged");
    if(!editor.saveAnnotations()) return 2;
    const auto saved=read(root+"/labels/a.png");
    state->translateShape(0,{2,2});const auto unsaved=state->shapes();
    if(!QDir().rename(root+"/labels",root+"/labels-backup")) return 2;
    QFile blocked(root+"/labels");if(!blocked.open(QIODevice::WriteOnly)) return 2;blocked.write("blocked");blocked.close();
    QMetaObject::invokeMethod(&editor,"onSaveAndNext");
    check(state->shapes()==unsaved && state->isDirty() && read(root+"/labels-backup/a.png")==saved,"failed save-and-next retains edits and committed file");
    QFile::remove(root+"/labels");if(!QDir().rename(root+"/labels-backup",root+"/labels")) return 2;
    editor.activateWindow();list->setFocus();app.processEvents();
    QKeyEvent saveNext(QEvent::KeyPress,Qt::Key_Return,Qt::ControlModifier);
    QApplication::sendEvent(list,&saveNext);
    check(state->isEmpty() && !state->isDirty() && view->zoomLevel()==2.0,"retry shortcut from shape list saves and advances while keeping view");
    state->appendShape(box);QMetaObject::invokeMethod(&editor,"onSaveAndNext");
    check(state->shapeCount()==1 && !state->isDirty() && QFile::exists(root+"/labels/b.png"),"last image saves and remains open");
    check(editor.openImage("Seg",root+"/images/a.png") && state->shapes()==unsaved,"saved-next annotation reopens with shape identity");
    QMetaObject::invokeMethod(&editor,"onNavigateNext");
    check(state->shapeCount()==1 && !state->isDirty(),"next navigation loads saved second image");
    QMetaObject::invokeMethod(&editor,"onNavigatePrev");
    check(state->shapes()==unsaved,"previous navigation keeps destination valid across sample refresh");
    const auto persisted=read(root+"/labels/a.png");SessionManager::instance().logout();
    QMetaObject::invokeMethod(&editor,"onSaveAndNext");press({100,100});move({140,140});release({140,140});
    check(state->shapes()==unsaved && !state->isDirty() && read(root+"/labels/a.png")==persisted,"guest handlers cannot mutate or advance through saving");
    const auto evidence=qEnvironmentVariable("FUSEVISION_NATIVE_EVIDENCE_DIR");
    if(!evidence.isEmpty()) {
        const auto fonts=QDir(qEnvironmentVariable("SystemRoot")).filePath("Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont(fonts);app.setFont(QFont("Microsoft YaHei",10));
        SessionManager::instance().login(login.userId);editor.findChild<QPushButton*>("annoOverlayBtn")->setChecked(true);
        view->fitToWindow();app.processEvents();QDir().mkpath(evidence);
        check(editor.grab().save(evidence+"/P05-native-annotation-offscreen-20261117.png"),"offscreen native workspace capture");
    }
    return failures ? 1 : 0;
}
