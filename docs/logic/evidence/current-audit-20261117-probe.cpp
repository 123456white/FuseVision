// Read-only audit probes against the current Release libraries.
// All settings, project files and images use temporary directories. The database
// default-path probe runs from an isolated executable directory under out/.
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include "core/AtomicFile.h"
#include "core/DatabaseManager.h"
#include "core/SettingsManager.h"
#include "deeplearning/model/AnnotationModel.h"
#include "deeplearning/storage/AnnotationStore.h"
#include "deeplearning/storage/DatasetFileService.h"
#include "deeplearning/storage/ModelConfig.h"
#include "deeplearning/storage/ProjectConfig.h"
#include "deeplearning/widgets/DatasetManagement.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const auto fonts = QDir(qEnvironmentVariable("SystemRoot")).filePath("Fonts/");
    for (const auto& name : {"msyh.ttc", "msyhbd.ttc", "segoeui.ttf"})
        QFontDatabase::addApplicationFont(fonts + name);
    app.setFont(QFont("Microsoft YaHei", 10));
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;
    const auto root = temporary.path();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root + "/settings");
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, root + "/system");
    auto& settings = SettingsManager::instance();
    settings.setDbPath(root + "/configured-database");
    if (!settings.save()) return 2;
    settings.load();
    if (!DatabaseManager::instance().initDatabase()) return 2;
    const auto actualDb = QSqlDatabase::database("fusevision_conn").databaseName();
    std::cout << "OBSERVE database configured=" << settings.dbPath().toStdString()
              << " actual=" << actualDb.toStdString()
              << " configured_path_used=" << actualDb.startsWith(settings.dbPath()) << std::endl;

    AnnotationShape rotated;
    rotated.type = AnnotationShape::RotatedRectShape;
    rotated.labelId = 0;
    rotated.bbox = {80, 95, 40, 10};
    rotated.rotCenter = {100, 100};
    rotated.rotation = 90;
    AnnotationModel model;
    model.reset({rotated});
    QByteArray before, after;
    QString error;
    QStringList warnings;
    if (!DatasetFileService::yoloText(model.shapes(), {200, 200}, "obb", before, warnings, error)) return 2;
    {
        AnnotationModel::Transaction transaction(&model, "audit translate");
        model.translateShape(0, {10, 0});
    }
    if (!DatasetFileService::yoloText(model.shapes(), {200, 200}, "obb", after, warnings, error)) return 2;
    const auto first = before.simplified().split(' ');
    const auto last = after.simplified().split(' ');
    std::cout << "OBSERVE rotated_move requested=(10,0) rendered_first_corner_delta=("
              << (last[1].toDouble() - first[1].toDouble()) * 200 << ","
              << (last[2].toDouble() - first[2].toDouble()) * 200 << ") center_unchanged="
              << (model.shapes()[0].rotCenter == rotated.rotCenter) << std::endl;

    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(Qt::white);
    QDir().mkpath(root + "/source");
    const auto sourceImage = root + "/source/sample.png";
    const auto sourceLabel = root + "/source/sample.json";
    if (!image.save(sourceImage)) return 2;
    const QJsonObject invalid{{"annotations", QJsonArray{QJsonObject{
        {"category_id", 1}, {"bbox", QJsonArray{1, 2, -3, 4}}}}}};
    if (!AtomicFile::write(sourceLabel, QJsonDocument(invalid).toJson(), &error)) return 2;
    const auto destinationImages = root + "/destination/images";
    const auto destinationLabels = root + "/destination/labels";
    QDir().mkpath(destinationLabels);
    const auto previousLabel = destinationLabels + "/sample.json";
    AnnotationShape valid;
    valid.type = AnnotationShape::RectShape;
    valid.labelId = 0;
    valid.bbox = {1, 2, 3, 4};
    if (!AnnotationStore::save(previousLabel, "sample.png", image, {valid}, false, false, 1).ok) return 2;
    const auto report = DatasetFileService::importFiles({sourceImage}, {sourceLabel},
        destinationImages, destinationLabels, true, [] { return true; });
    QList<AnnotationShape> reopened;
    error.clear();
    const bool canRead = DatasetFileService::readShapes(previousLabel, image.size(), {"defect"}, false, reopened, error);
    QFile replacement(previousLabel);
    if (!replacement.open(QIODevice::ReadOnly)) return 2;
    std::cout << "OBSERVE invalid_label_overwrite import_ok=" << report.ok()
              << " reopen_ok=" << canRead
              << " previous_valid_label_replaced=" << (replacement.readAll() == QJsonDocument(invalid).toJson())
              << std::endl;

    if (!ProjectConfig::createProject("Audit", "", root, &error)) return 2;
    auto project = ProjectConfig::loadProject(root + "/Audit/project.fvproj", &error);
    if (!ModelConfig::createModel("Anomaly", ModelTaskTypes::AnomalyDetect, project.projectRoot,
                                  project.fvprojPath, &error)) return 2;
    const auto modelRoot = project.projectRoot + "/Anomaly";
    if (!image.save(modelRoot + "/train/OK/ok.png") || !image.save(modelRoot + "/train/NG/ng.png")) return 2;
    DatasetManagement dataset;
    dataset.setCurrentProject(project);
    dataset.setCurrentModel("Anomaly", ModelConfig::loadModel(modelRoot + "/Anomaly.fvdl", &error));
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();
    std::cout << "OBSERVE anomaly_classification stored_images=2 gallery_samples="
              << dataset.allSamples().size() << std::endl;
    return 0;
}
