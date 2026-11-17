#ifdef QT_WIDGETS_LIB
#error "Storage tests must remain independent of Widgets."
#endif
#include <gtest/gtest.h>
#include "core/AtomicFile.h"
#include "core/Logger.h"
#include "deeplearning/storage/ProjectConfig.h"
#include "deeplearning/storage/ModelConfig.h"
#include "deeplearning/storage/StorageContract.h"
#include "DatasetFixture.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QTemporaryDir>

namespace {
QByteArray read(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { ADD_FAILURE() << path.toStdString(); return {}; }
    return f.readAll();
}
QJsonObject json(const QString& path) { return QJsonDocument::fromJson(read(path)).object(); }
void write(const QString& path, const QJsonObject& value)
{
    ASSERT_TRUE(AtomicFile::write(path, QJsonDocument(value).toJson()));
}
class Storage : public testing::Test {
protected:
    QTemporaryDir temporary;
    QString path, modelPath, error;
    void SetUp() override
    {
        ASSERT_TRUE(temporary.isValid());
        ASSERT_TRUE(ProjectConfig::createProject("Project", "中文说明", temporary.path(), &error));
        path = temporary.path()+"/Project/project.fvproj";
        ASSERT_TRUE(ModelConfig::createModel("Model", ModelTaskTypes::ObjDetectFree,
            QFileInfo(path).absolutePath(), path, &error));
        modelPath = temporary.path()+"/Project/Model/Model.fvdl";
    }
};
TEST_F(Storage, NewDocumentsHaveTypeVersionAndRelativePaths)
{
    const auto project=json(path), model=json(modelPath);
    EXPECT_EQ(project["document_type"].toString(), "fusevision.project");
    EXPECT_EQ(project["format_version"].toString(), "1.0");
    EXPECT_EQ(model["document_type"].toString(), "fusevision.model");
    EXPECT_EQ(model["format_version"].toString(), "1.0");
    EXPECT_EQ(project["paths"].toObject()["project_root"].toString(), ".");
    EXPECT_EQ(project["models"].toObject()["Model"].toObject()["model_file"].toString(), "Model/Model.fvdl");
}
TEST_F(Storage, MoveAndSaveKeepPortableReferencesAndUnknownFields)
{
    auto raw=json(path);raw["vendor_extension"]=QJsonObject{{"keep",42}};write(path,raw);
    const auto moved=temporary.path()+"/Moved";
    ASSERT_TRUE(QDir().rename(temporary.path()+"/Project",moved));
    path=moved+"/project.fvproj";
    auto info=ProjectConfig::loadProject(path,&error);
    ASSERT_EQ(info.description,"中文说明");
    EXPECT_EQ(info.models["Model"].toObject()["model_file"].toString(),moved+"/Model/Model.fvdl");
    ASSERT_TRUE(ProjectConfig::saveProject(info,&error));
    EXPECT_EQ(json(path)["vendor_extension"],raw["vendor_extension"]);
    EXPECT_EQ(json(path)["models"].toObject()["Model"].toObject()["model_file"].toString(),"Model/Model.fvdl");
}
TEST_F(Storage, LegacyAbsoluteReferencesAreRejectedWithoutWriting)
{
    auto raw=json(path);raw.remove("format_version");raw.remove("document_type");
    raw["paths"]=QJsonObject{{"project_root",temporary.path()+"/old"}};
    raw["models"]=QJsonObject{{"Model",QJsonObject{{"model_file",temporary.path()+"/old/Model/Model.fvdl"}}}};
    write(path,raw);const auto before=read(path);
    const auto info=ProjectConfig::loadProject(path,&error);
    EXPECT_TRUE(info.projectName.isEmpty());EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(read(path),before);
}
TEST_F(Storage, AnnotationWorkspaceMatchesRuntimeDirectories)
{
    const auto dir=QFileInfo(modelPath).absolutePath();
    EXPECT_TRUE(QDir(dir+"/images").exists());EXPECT_TRUE(QDir(dir+"/labels").exists());
    EXPECT_FALSE(QDir(dir+"/images/train").exists());
    EXPECT_EQ(json(modelPath)["dataset_layout"].toString(),"native");
    const auto yaml=read(dir+"/data.yaml").replace("\r\n","\n");
    EXPECT_FALSE(read(dir+"/data.yaml").contains("\r\n"));
    EXPECT_TRUE(yaml.contains("path: .\n"));
    EXPECT_TRUE(yaml.contains("images: images\n"));
    EXPECT_FALSE(yaml.contains("train:"));
}
TEST_F(Storage, ClassificationKeepsSixFoldersAndPortableYaml)
{
    const auto root=QFileInfo(path).absolutePath();
    ASSERT_TRUE(ModelConfig::createModel("Classify",ModelTaskTypes::AnomalyDetect,root,path,&error));
    for(const auto* phase:{"train","val","test"}) for(const auto* category:{"OK","NG"})
        EXPECT_TRUE(QDir(root+"/Classify/"+phase+"/"+category).exists());
    const auto yaml=read(root+"/Classify/data.yaml").replace("\r\n","\n");
    EXPECT_FALSE(read(root+"/Classify/data.yaml").contains("\r\n"));
    EXPECT_TRUE(yaml.contains("path: .\n"));EXPECT_TRUE(yaml.contains("task: classify\n"));
}
class InvalidDocument : public Storage, public testing::WithParamInterface<std::pair<const char*,QJsonValue>> {};
TEST_P(InvalidDocument, ProjectRejectsLoadSaveAndStatusWithoutChangingBytes)
{
    auto info=ProjectConfig::loadProject(path,&error);auto raw=json(path);
    raw[GetParam().first]=GetParam().second;write(path,raw);const auto before=read(path);
    EXPECT_TRUE(ProjectConfig::loadProject(path,&error).projectName.isEmpty());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(ProjectConfig::saveProject(info,&error));
    EXPECT_FALSE(ProjectConfig::setModelStatus(path,"Model","trained",true));
    EXPECT_EQ(read(path),before);
}
TEST_P(InvalidDocument, ModelRejectsLoadSaveWithoutChangingBytes)
{
    auto info=ModelConfig::loadModel(modelPath,&error);auto raw=json(modelPath);
    raw[GetParam().first]=GetParam().second;write(modelPath,raw);const auto before=read(modelPath);
    EXPECT_TRUE(ModelConfig::loadModel(modelPath,&error).modelName.isEmpty());
    EXPECT_FALSE(error.isEmpty());EXPECT_FALSE(ModelConfig::saveModel(info,&error));
    EXPECT_EQ(read(modelPath),before);
}
INSTANTIATE_TEST_SUITE_P(Format,InvalidDocument,testing::Values(
    std::make_pair("format_version",QJsonValue("99.0")),
    std::make_pair("format_version",QJsonValue(1)),
    std::make_pair("document_type",QJsonValue("another.application"))));
TEST_F(Storage, InvalidInMemorySavePreservesCommittedFiles)
{
    auto project=ProjectConfig::loadProject(path,&error);auto model=ModelConfig::loadModel(modelPath,&error);
    const auto p=read(path),m=read(modelPath);
    project.projectName="";model.modelType="unsupported";
    EXPECT_FALSE(ProjectConfig::saveProject(project,&error));EXPECT_FALSE(ModelConfig::saveModel(model,&error));
    EXPECT_EQ(read(path),p);EXPECT_EQ(read(modelPath),m);
}
TEST_F(Storage, MalformedAndWrongFieldTypesAreRejected)
{
    auto p=json(path);p["models"]=QJsonArray{1};write(path,p);
    EXPECT_TRUE(ProjectConfig::loadProject(path,&error).projectName.isEmpty());
    auto m=json(modelPath);m["inference_config"]="invalid";write(modelPath,m);
    EXPECT_TRUE(ModelConfig::loadModel(modelPath,&error).modelName.isEmpty());
}
TEST_F(Storage, ModelUnknownFieldsSurviveSave)
{
    auto raw=json(modelPath);raw["vendor_extension"]=QJsonArray{1,2,3};write(modelPath,raw);
    auto info=ModelConfig::loadModel(modelPath,&error);info.description="changed";
    ASSERT_TRUE(ModelConfig::saveModel(info,&error));
    EXPECT_EQ(json(modelPath)["vendor_extension"],raw["vendor_extension"]);
    EXPECT_EQ(ModelConfig::loadModel(modelPath,&error).description,"changed");
}
TEST_F(Storage, SemanticallyInvalidOriginalModelIsNotOverwritten)
{
    auto info=ModelConfig::loadModel(modelPath,&error);auto raw=json(modelPath);
    raw["model_type"]="unknown";write(modelPath,raw);const auto before=read(modelPath);
    EXPECT_FALSE(ModelConfig::saveModel(info,&error));EXPECT_EQ(read(modelPath),before);
}
TEST_F(Storage, LegacyModelWithoutMarkersIsRejectedWithoutWriting)
{
    auto valid=ModelConfig::loadModel(modelPath,&error);
    auto raw=json(modelPath);raw.remove("document_type");raw.remove("format_version");raw["dataset_layout"]="yolo";
    write(modelPath,raw);const auto before=read(modelPath);
    const auto info=ModelConfig::loadModel(modelPath,&error);
    EXPECT_TRUE(info.modelName.isEmpty());EXPECT_EQ(read(modelPath),before);
    EXPECT_FALSE(ModelConfig::saveModel(valid,&error));EXPECT_EQ(read(modelPath),before);
}
TEST_F(Storage, StatusUpdateRejectsLegacyPaths)
{
    auto raw=json(path);raw["paths"]=QJsonObject{{"project_root",QFileInfo(path).absolutePath()}};
    auto models=raw["models"].toObject();auto model=models["Model"].toObject();model["model_file"]=modelPath;
    models["Model"]=model;raw["models"]=models;write(path,raw);
    const auto before=read(path);
    EXPECT_FALSE(ProjectConfig::setModelStatus(path,"Model","annotated",true));EXPECT_EQ(read(path),before);
}
TEST(StorageFiles, DatasetRejectsWrongFieldTypes)
{
    QString error;
    const auto valid=datasetFixture("COCO边界框JSON");
    ASSERT_TRUE(StorageContract::validate(valid,"fusevision.dataset",&error));
    for(const auto& entry:{std::pair<QString,QJsonValue>{"categories","wrong"},{"categories",QJsonArray{1}},
        {"categories",QJsonArray{QJsonObject{{"name",42}}}},{"image_count",-1},{"image_count",0.5}}) {
        auto invalid=valid;invalid[entry.first]=entry.second;
        EXPECT_FALSE(StorageContract::validate(invalid,"fusevision.dataset",&error));
    }
}
TEST_F(Storage, ExternalModelReferenceIsRejectedWithoutChangingProject)
{
    auto info=ProjectConfig::loadProject(path,&error);
    const auto external=temporary.path()+"/External/Other.fvdl";
    auto entry=info.models["Model"].toObject();entry["model_file"]=external;info.models["Model"]=entry;
    const auto before=read(path);EXPECT_FALSE(ProjectConfig::saveProject(info,&error));EXPECT_EQ(read(path),before);
}
TEST_F(Storage, ModelCreationCommitsCompleteDatasetConfig)
{
    const auto config=json(QFileInfo(modelPath).absolutePath()+"/dataset_config.json");
    EXPECT_EQ(config["document_type"].toString(),"fusevision.dataset");
    EXPECT_EQ(config["format_version"].toString(),"1.0");
    EXPECT_EQ(config["dataset_mode"].toString(),"annotation");
    EXPECT_TRUE(config["categories"].isArray());EXPECT_EQ(config["image_count"].toInt(-1),0);
    const auto root=QFileInfo(path).absolutePath();
    ASSERT_TRUE(ModelConfig::createModel("Classify",ModelTaskTypes::AnomalyDetect,root,path,&error));
    const auto classify=json(root+"/Classify/dataset_config.json");
    EXPECT_EQ(classify["dataset_mode"].toString(),"classification");
    EXPECT_EQ(classify["label_type"].toString(),"OK/NG分类");
    ASSERT_EQ(classify["categories"].toArray().size(),2);
    EXPECT_EQ(classify["categories"].toArray()[0].toObject()["name"].toString(),"OK");
    EXPECT_EQ(classify["categories"].toArray()[1].toObject()["name"].toString(),"NG");
}
class MissingMarker : public Storage, public testing::WithParamInterface<const char*> {};
TEST_P(MissingMarker, RejectProjectAndModelWithoutAutomaticUpgrade)
{
    auto project=ProjectConfig::loadProject(path,&error);auto model=ModelConfig::loadModel(modelPath,&error);
    auto p=json(path),m=json(modelPath);p.remove(GetParam());m.remove(GetParam());write(path,p);write(modelPath,m);
    const auto pb=read(path),mb=read(modelPath);
    EXPECT_TRUE(ProjectConfig::loadProject(path,&error).projectName.isEmpty());
    EXPECT_TRUE(ModelConfig::loadModel(modelPath,&error).modelName.isEmpty());
    EXPECT_FALSE(ProjectConfig::saveProject(project,&error));EXPECT_FALSE(ModelConfig::saveModel(model,&error));
    EXPECT_EQ(read(path),pb);EXPECT_EQ(read(modelPath),mb);
}
TEST_P(MissingMarker, RejectDatasetWithoutAutomaticUpgrade)
{
    auto data=datasetFixture("COCO边界框JSON");data.remove(GetParam());
    EXPECT_FALSE(StorageContract::validate(data,"fusevision.dataset",&error));
}
INSTANTIATE_TEST_SUITE_P(Standard,MissingMarker,testing::Values("document_type","format_version"));
TEST_F(Storage, LegacyYoloLayoutIsRejectedEvenWithVersion)
{
    auto raw=json(modelPath);raw["dataset_layout"]="yolo";write(modelPath,raw);
    EXPECT_TRUE(ModelConfig::loadModel(modelPath,&error).modelName.isEmpty());
}
TEST_F(Storage, MissingRequiredFieldsAreRejected)
{
    for(const auto* key:{"models","paths","created_time"}) {
        auto raw=json(path);raw.remove(key);
        EXPECT_FALSE(StorageContract::validate(raw,"fusevision.project",&error));
    }
    for(const auto* key:{"dataset_layout","dataset_ref","architecture"}) {
        auto raw=json(modelPath);raw.remove(key);
        EXPECT_FALSE(StorageContract::validate(raw,"fusevision.model",&error));
    }
    const auto data=datasetFixture("COCO边界框JSON");
    for(const auto* key:{"dataset_mode","label_type","categories","image_count"}) {
        auto raw=data;raw.remove(key);EXPECT_FALSE(StorageContract::validate(raw,"fusevision.dataset",&error));
    }
}
class ReservedName : public testing::TestWithParam<const char*> {};
TEST_P(ReservedName, RejectBeforeCreatingFiles)
{
    EXPECT_FALSE(ProjectConfig::isValidProjectName(GetParam()));
    EXPECT_FALSE(ModelConfig::isValidModelName(GetParam()));
}
INSTANTIATE_TEST_SUITE_P(Windows,ReservedName,testing::Values("CON","nul","AUX","PRN","COM1","LPT9"));
TEST(StorageFiles, AtomicFailureRetainsExistingFile)
{
    QTemporaryDir t;ASSERT_TRUE(t.isValid());const auto file=t.path()+"/file";
    ASSERT_TRUE(AtomicFile::write(file,"original"));QString error;
    EXPECT_FALSE(AtomicFile::write(file+"/child","new",&error));EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(read(file),"original");
}
TEST(StorageFiles, DottedLogDirectoryIsRespected)
{
    QTemporaryDir t;ASSERT_TRUE(t.isValid());const auto dir=t.path()+"/logs.v1";QString error;
    ASSERT_TRUE(QDir().mkpath(dir));ASSERT_TRUE(Logger::init(dir,Logger::Info,&error));
    EXPECT_EQ(Logger::activeDirectory(),dir);EXPECT_EQ(Logger::activeBaseName(),"fusevision");
    ASSERT_TRUE(Logger::init(t.path()+"/new.logs",Logger::Info,&error));
    EXPECT_EQ(Logger::activeDirectory(),t.path()+"/new.logs");
    ASSERT_TRUE(Logger::init(t.path()+"/application.log",Logger::Info,&error));
    EXPECT_EQ(Logger::activeDirectory(),t.path());EXPECT_EQ(Logger::activeBaseName(),"application");
}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    QTemporaryDir settings;
    if(!settings.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
}
