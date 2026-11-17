// Analysis-only harness: original Qt widgets, isolated INI settings and SQLite.
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QRadioButton>
#include <QComboBox>
#include <QTabBar>
#include <QPainter>
#include <QFontDatabase>
#include <QPointer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <iostream>
#include <stdexcept>
#include "app/FuseVision.h"
#include "app/Login.h"
#include "deeplearning/widgets/DeepLearningWidget.h"
#include "deeplearning/widgets/ProjectManagement.h"
#include "deeplearning/widgets/ModelManagement.h"
#include "deeplearning/widgets/DatasetManagement.h"
#include "deeplearning/widgets/AnnotationManagement.h"
#include "deeplearning/widgets/AnnotationCanvas.h"
#include "deeplearning/model/AnnotationModel.h"
#include "core/SettingsManager.h"
#include "core/DatabaseManager.h"
#include "core/PermissionRegistry.h"
#include "core/SessionManager.h"
#include "ui/ThemeManager.h"
#include "core/Logger.h"

static void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
static QAbstractButton* findButton(QWidget* root, const QString& text) {
    for (auto* button : root->findChildren<QAbstractButton*>())
        if (button->isVisible() && (button->text().trimmed() == text || button->toolTip() == text)) return button;
    throw std::runtime_error(("Button missing: " + text).toStdString());
}

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    app.setFont(QFont("Microsoft YaHei", 10));
    require(argc == 3, "Usage: Capture screenshots-dir evidence-json");
    const QString screenshots = QDir::cleanPath(QString::fromLocal8Bit(argv[1]));
    const QString evidencePath = QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(screenshots);
    QTemporaryDir scratch;
    require(scratch.isValid(), "Temporary workspace unavailable");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch.path() + "/ini");
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, scratch.path() + "/ini-system");
    app.setOrganizationName("FuseVisionTeam");
    app.setApplicationName("FuseVision");
    app.setApplicationVersion("1.0.0");
    app.setStyle("Fusion");
    auto& settings = SettingsManager::instance();
    settings.setProjectDataPath(scratch.path() + "/projects");
    settings.setDbPath(scratch.path() + "/database");
    settings.setTraditionalDataPath(scratch.path() + "/traditional");
    settings.setTraditionalCameraPath(scratch.path() + "/cameras");
    settings.setLogPath(scratch.path() + "/logs/capture");
    Logger::init(settings.logPath(), Logger::Info);
    app.setStyleSheet(ThemeManager::instance().styleSheet());
    require(DatabaseManager::instance().initDatabase(":memory:"), "In-memory database initialization failed");
    const QStringList modules = {"用户管理", "系统设置", "深度学习.项目管理", "深度学习.模型管理", "深度学习.数据集管理", "深度学习.数据标注", "深度学习.数据检查", "深度学习.数据拆分", "深度学习.模型训练", "深度学习.模型预测", "深度学习.模型导出", "传统视觉.流程编辑", "传统视觉.相机采集", "传统视觉.相机标定", "传统视觉.相机建模", "传统视觉.通讯设置", "传统视觉.系统设置", "传统视觉.视觉工具区"};
    for (const auto& key : modules) PermissionRegistry::instance().registerModule(key);
    QJsonArray captures, observations;
    auto snap = [&](QWidget* widget, const QString& id, const QString& description) {
        QTest::qWait(120);
        require(widget->grab().save(screenshots + "/" + id + ".png"), "Could not save Qt widget screenshot");
        QJsonArray controls;
        for (auto* button : widget->findChildren<QAbstractButton*>()) {
            if (!button->isVisible()) continue;
            QPoint pos = button->mapTo(widget, QPoint());
            QJsonArray childLabels;
            for (auto* label : button->findChildren<QLabel*>())
                if (!label->text().isEmpty()) childLabels.append(label->text());
            controls.append(QJsonObject{{"text", button->text().trimmed()}, {"tooltip", button->toolTip()},
                {"childLabels", childLabels}, {"objectName", button->objectName()},
                {"enabled", button->isEnabled()}, {"class", button->metaObject()->className()},
                {"x", pos.x()}, {"y", pos.y()}, {"width", button->width()}, {"height", button->height()}});
        }
        captures.append(QJsonObject{{"id", id}, {"description", description}, {"file", "screenshots/" + id + ".png"},
            {"method", "Original Qt widget grab; offscreen platform; isolated synthetic dataset"}, {"controls", controls}});
    };
    QString mode, originalImage, originalMask;
    QTimer modalDriver;
    modalDriver.setInterval(70);
    QObject::connect(&modalDriver, &QTimer::timeout, [&]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog || dialog->property("analysisHandled").toBool()) return;
        dialog->setProperty("analysisHandled", true);
        std::cout << "DIALOG: " << dialog->windowTitle().toStdString() << std::endl;
        if (auto* box = qobject_cast<QMessageBox*>(dialog)) {
            if (box->standardButtons().testFlag(QMessageBox::Yes)) {
                box->button(QMessageBox::Yes)->click();
            } else if (box->standardButtons().testFlag(QMessageBox::Ok)) {
                box->button(QMessageBox::Ok)->click();
            } else dialog->reject();
        } else if (auto* fileDialog = qobject_cast<QFileDialog*>(dialog)) {
            const QString selected = dialog->windowTitle().contains("标签") ? originalMask : originalImage;
            fileDialog->setDirectory(QFileInfo(selected).absolutePath());
            fileDialog->selectFile(selected);
            QPointer<QFileDialog> guarded(fileDialog);
            QTimer::singleShot(250, fileDialog, [guarded, selected]() {
                if (!guarded) return;
                if (auto* field = guarded->findChild<QLineEdit*>("fileNameEdit")) field->setText(QDir::toNativeSeparators(selected));
                std::cout << "SELECT: " << guarded->selectedFiles().join(";").toStdString() << std::endl;
                QMetaObject::invokeMethod(guarded, "accept", Qt::DirectConnection);
                if (guarded && guarded->isVisible()) guarded->setProperty("analysisHandled", false);
            });
        } else if (auto* input = qobject_cast<QInputDialog*>(dialog)) {
            if (mode == "label") input->setTextValue("surface_defect");
            input->accept();
        } else if (mode == "project" || mode == "model") {
            for (auto* field : dialog->findChildren<QLineEdit*>()) if (!field->isReadOnly()) {
                field->setText(mode == "project" ? "UI_Logic_Demo" : "SurfaceSeg"); break;
            }
            for (auto* field : dialog->findChildren<QTextEdit*>()) field->setPlainText("隔离验证数据，仅用于功能整理截图。");
            snap(dialog, mode == "project" ? "P02-new-project-dialog" : "P03-new-model-dialog", "实际创建对话框（隔离验证）");
            dialog->accept();
        } else dialog->reject();
    });
    modalDriver.start();
    QTimer::singleShot(45000, &app, []() { std::cerr << "Analysis watchdog expired\n"; std::exit(3); });
    try {
        LoginDialog login;
        login.show();
        snap(&login, "P00-login", "原始登录页，只展示、不提交，不保存登录设置");
        login.hide();
        auto admin = DatabaseManager::instance().validateLogin("superadmin", "admin123");
        require(admin.success, "Seed administrator login failed");
        SessionManager::instance().login(admin.userId, "superadmin", admin.role);
        FuseVision window;
        window.initPagesAsync();
        window.resize(1440, 940);
        window.show();
        QTest::qWait(700);
        auto* project = window.findChild<ProjectManagement*>();
        auto* model = window.findChild<ModelManagement*>();
        auto* dataset = window.findChild<DatasetManagement*>();
        auto* annotation = window.findChild<AnnotationManagement*>();
        auto* dl = window.findChild<DeepLearningWidget*>();
        require(project && model && dataset && annotation && dl, "Main feature widgets not initialized");
        auto* tabs = dl->findChild<QTabBar*>();
        require(tabs && tabs->count() == 9, "Expected nine deep-learning tabs");
        snap(&window, "P01-dl-empty", "深度学习首页，未创建项目");
        mode = "project";
        findButton(project, "新建项目")->click();
        mode.clear();
        QTest::qWait(150);
        auto projectCards = project->findChildren<ProjectCard*>();
        require(projectCards.size() == 1, "New project card not generated");
        snap(&window, "P02-project-created", "真实按钮创建项目后的项目卡片");
        QTest::mouseDClick(projectCards.first(), Qt::LeftButton);
        QTest::qWait(150);
        require(project->hasProject(), "Project card double-click did not open project");
        QSignalSpy createdSpy(model, &ModelManagement::modelCreated);
        QSignalSpy changedSpy(model, &ModelManagement::currentModelChanged);
        mode = "model";
        findButton(model, "新建模型")->click();
        mode.clear();
        QTest::qWait(150);
        require(model->hasModel(), "New model not selected");
        observations.append(QJsonObject{{"case", "new-model-signals"}, {"modelCreated", createdSpy.count()}, {"currentModelChanged", changedSpy.count()}, {"selectedModel", model->currentModelId()}});
        snap(&window, "P03-model-created", "真实按钮创建模型；自动选中新模型");
        auto modelCards = model->findChildren<ModelCard*>();
        require(modelCards.size() == 1, "Model card missing");
        QTest::mouseDClick(modelCards.first(), Qt::LeftButton);
        QTest::qWait(250);
        require(tabs->currentIndex() == 2, "Model double-click did not navigate to dataset");
        snap(&window, "P04-dataset-empty", "模型双击进入数据集，尚无图像");
        QDir().mkpath(scratch.path() + "/fixture/images");
        QDir().mkpath(scratch.path() + "/fixture/labels");
        originalImage = scratch.path() + "/fixture/images/surface_demo.png";
        originalMask = scratch.path() + "/fixture/labels/surface_demo.png";
        QImage sample(800, 520, QImage::Format_RGB32);
        sample.fill(QColor("#bdc8d2"));
        { QPainter painter(&sample); painter.fillRect(80, 80, 640, 360, QColor("#6e8290"));
          painter.setPen(QPen(QColor("#d74343"), 12)); painter.drawLine(220, 160, 530, 330);
          painter.setPen(Qt::white); painter.setFont(QFont("Microsoft YaHei", 16)); painter.drawText(110, 130, "合成演示图像 / 隔离验证"); }
        require(sample.save(originalImage), "Could not create synthetic sample");
        QImage mask(800, 520, QImage::Format_Grayscale8); mask.fill(0);
        for (int y = 155; y < 190; ++y) for (int x = 210; x < 365; ++x) mask.scanLine(y)[x] = 1;
        require(mask.save(originalMask), "Could not create synthetic mask");
        sample.save(QFileInfo(evidencePath).absolutePath() + "/fixture-image.png");
        mask.save(QFileInfo(evidencePath).absolutePath() + "/fixture-mask.png");
        mode = "import";
        require(QMetaObject::invokeMethod(dataset, "onImportImages", Qt::DirectConnection), "Image import slot unavailable");
        mode.clear();
        QTest::qWait(200);
        require(dataset->allSamples().size() == 1, "Image import did not produce one sample");
        snap(&window, "P04-dataset-imported", "真实文件导入流程：一张合成图像和一张 PNG 掩码");
        require(QMetaObject::invokeMethod(dataset, "onThumbnailDoubleClicked", Qt::DirectConnection, Q_ARG(int, 0)), "Image opening slot unavailable");
        QTest::qWait(200);
        require(tabs->currentIndex() == 3, "Image opening did not navigate to annotation");
        auto* canvas = annotation->findChild<AnnotationCanvas*>();
        require(canvas && canvas->hasImage(), "Annotation canvas image not loaded");
        snap(&window, "P05-annotation-loaded", "标注页加载真实导入的演示图像及掩码");
        require(QMetaObject::invokeMethod(annotation, "onToolChanged", Qt::DirectConnection, Q_ARG(int, int(AnnoTool::Rectangle))), "Rectangle tool slot unavailable");
        const QPoint start = canvas->mapFromScene(QPointF(400, 240));
        const QPoint end = canvas->mapFromScene(QPointF(610, 370));
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas->viewport(), end, 40);
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QTest::qWait(100);
        auto* shapeModel = annotation->findChild<AnnotationModel*>();
        require(shapeModel, "Annotation model missing");
        const int shapeCountBeforeSave = shapeModel->shapeCount();
        const bool dirtyBeforeSave = annotation->hasUnsavedChanges();
        QMetaObject::invokeMethod(annotation, "onSave", Qt::DirectConnection);
        const QString savedMask = dataset->annotationsDir() + "/surface_demo.png";
        require(QFileInfo::exists(savedMask), "Saved annotation file missing");
        QImage saved(savedMask);
        saved.save(QFileInfo(evidencePath).absolutePath() + "/saved-mask.png");
        observations.append(QJsonObject{{"case", "annotation-save"}, {"dirtyBeforeSave", dirtyBeforeSave}, {"dirtyAfterSave", annotation->hasUnsavedChanges()}, {"modelShapeCountBeforeSave", shapeCountBeforeSave}, {"modelDirtyAfterSave", shapeModel->isDirty()}, {"savedMaskReadable", !saved.isNull()}, {"savedPixelInNewRectangle", saved.pixelColor(500, 300).red()}});
        snap(&window, "P05-annotation-saved", "两次点击完成矩形后保存；以模型形状数和文件像素验证实际结果");
        annotation->openImage(model->currentModelId(), dataset->allSamples().first().absolutePath);
        observations.append(QJsonObject{{"case", "annotation-reopen"}, {"modelShapeCountAfterReopen", shapeModel->shapeCount()}});
        snap(&window, "P05-annotation-reopened", "重新加载保存的图像后，新增矩形未恢复");
        QMetaObject::invokeMethod(annotation, "onReturnToGallery", Qt::DirectConnection);
        QTest::qWait(100);
        snap(&window, "P04-dataset-after-annotation", "返回数据集后的标签和实例统计");
        for (int index = 4; index < tabs->count(); ++index) {
            tabs->setCurrentIndex(index);
            snap(&window, QString("P%1-placeholder").arg(index + 2, 2, 10, QChar('0')), tabs->tabText(index) + "占位页");
        }
        findButton(&window, "传统视觉")->click();
        snap(&window, "P11-traditional", "传统视觉当前界面骨架");
        findButton(&window, "用户管理")->click();
        snap(&window, "P12-users", "内存数据库中的用户与权限矩阵");
        findButton(&window, "系统设置")->click();
        snap(&window, "P13-settings", "系统设置页面，显示隔离验证路径");
        auto user = DatabaseManager::instance().validateLogin("user", "123");
        SessionManager::instance().login(user.userId, "user", user.role);
        findButton(&window, "深度学习")->click(); tabs->setCurrentIndex(0);
        const bool newEnabled = findButton(project, "新建项目")->isEnabled();
        observations.append(QJsonObject{{"case", "ordinary-user-project-write-control"}, {"canWrite", PermissionRegistry::instance().canWrite(user.userId, "深度学习.项目管理")}, {"newProjectEnabled", newEnabled}});
        snap(&window, "P14-user-readonly", "切换内存会话到普通用户后观察项目写按钮");
        QFile evidence(evidencePath);
        require(evidence.open(QIODevice::WriteOnly), "Could not write runtime evidence");
        evidence.write(QJsonDocument(QJsonObject{{"platform", QGuiApplication::platformName()}, {"isolation", "QTemporaryDir datasets/logs, in-memory SQLite; application paths overridden in memory; native QSettings may be read by original constructors; no login submission/settings save/theme toggle"}, {"captures", captures}, {"observations", observations}}).toJson(QJsonDocument::Indented));
        std::cout << "Captured " << captures.size() << " original Qt widget states\n";
        window.close();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    return 0;
}
