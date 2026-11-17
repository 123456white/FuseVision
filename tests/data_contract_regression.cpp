#ifdef QT_WIDGETS_LIB
#error "The data regression target must not depend on Qt Widgets."
#endif

#include "core/AtomicFile.h"
#include "deeplearning/domain/AnnotationShape.h"
#include "deeplearning/domain/DatasetTypes.h"
#include "deeplearning/domain/ModelTypes.h"
#include "deeplearning/domain/ProjectTypes.h"
#include "deeplearning/model/AnnotationModel.h"
#include "deeplearning/render/AnnotationRender.h"
#include "deeplearning/storage/AnnotationStore.h"
#include "deeplearning/storage/DatasetFileService.h"
#include "deeplearning/storage/ModelConfig.h"
#include "deeplearning/storage/ProjectConfig.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool result, const char* message)
{
    if (!result)
        throw std::runtime_error(message);
}

QByteArray read(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read output");
    return file.readAll();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    try {
        require(temporary.isValid(), "temporary root");
        const auto root = temporary.path();
        QString error;
        require(ProjectConfig::createProject("DataOnly", "project description", root, &error), "create project without Widgets");
        auto project = ProjectConfig::loadProject(root + "/DataOnly/project.fvproj", &error);
        require(project.description == "project description", "project description round trip");
        require(ModelConfig::createModel("Detect", ModelTaskTypes::ObjDetectFree, project.projectRoot,
                                        project.fvprojPath, &error, "模型说明"), "create model without Widgets");
        auto model = ModelConfig::loadModel(project.projectRoot + "/Detect/Detect.fvdl", &error);
        require(model.description == "模型说明", "model description round trip");
        std::cout << "PASS data-only project and model configuration" << std::endl;

        const auto modelRoot = project.projectRoot + "/Detect";
        QImage image(24, 20, QImage::Format_RGB32);
        image.fill(Qt::white);
        require(image.save(modelRoot + "/images/sample.png"), "save source image");
        AnnotationShape rectangle;
        rectangle.type = AnnotationShape::RectShape;
        rectangle.labelId = 0;
        rectangle.bbox = {3, 4, 10, 8};
        AnnotationModel state;
        state.reset({rectangle});
        auto second = rectangle;
        second.uuid = QUuid::createUuid();
        second.bbox.moveLeft(12);
        state.appendShape(second);
        require(state.isDirty() && state.shapeCount() == 2, "data-only editing state");
        state.undo();
        require(state.shapeCount() == 1 && !state.isDirty(), "data-only undo baseline");
        state.redo();
        require(state.shapeCount() == 2 && state.isDirty(), "data-only redo");
        const auto label = modelRoot + "/labels/sample.json";
        require(AnnotationStore::save(label, "sample.png", image, state.shapes(), false, false, 1).ok, "atomic JSON save");
        QList<AnnotationShape> reopened;
        require(AnnotationStore::load(label, image.size(), false, {"defect"}, reopened, error), "reload JSON");
        require(reopened == state.shapes(), "shape identity and geometry survive module separation");
        const auto mask = root + "/mask.png";
        require(AnnotationStore::save(mask, "sample.png", image, state.shapes(), true, false, 1).ok, "atomic PNG save");
        require(AnnotationStore::load(mask, image.size(), false, {"defect"}, reopened, error)
                    && reopened == state.shapes(), "PNG native round trip");
        const auto committed = read(label);
        QFile blocker(root + "/blocked");
        require(blocker.open(QIODevice::WriteOnly) && blocker.write("blocked") == 7, "write failure fixture");
        blocker.close();
        const auto failed = AnnotationStore::save(root + "/blocked/sample.json", "sample.png", image,
                                                  state.shapes(), false, false, 1);
        require(!failed.ok && !failed.error.isEmpty() && state.isDirty() && read(label) == committed,
                "failed data-only save preserves editing state and committed file");
        std::cout << "PASS data-only undo storage round trip and failure" << std::endl;

        DatasetSample sample;
        sample.id = 1;
        sample.fileName = "sample.png";
        sample.absolutePath = modelRoot + "/images/sample.png";
        sample.width = image.width();
        sample.height = image.height();
        const auto output = root + "/export/annotations.json";
        const auto report = DatasetFileService::exportFiles(DatasetFileService::Coco, output, {sample}, {},
            modelRoot + "/labels", {"defect"}, LabelTypes::CocoBBox, model.modelType, [] { return true; });
        require(report.ok() && report.succeeded.size() == 1, "export without dataset widget");
        const auto coco = QJsonDocument::fromJson(read(output)).object();
        const auto annotations = coco["annotations"].toArray();
        require(annotations.size() == 2 && coco["images"].toArray().size() == 1
                    && annotations[0].toObject()["category_id"] == coco["categories"].toArray()[0].toObject()["id"],
                "independent export categories and instances");
        require(read(root + "/export/sample.png") == read(sample.absolutePath), "export image pairing");
        std::cout << "PASS data-only paired export independently reread" << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL data-only contract: " << error.what() << std::endl;
        return 1;
    }
}
