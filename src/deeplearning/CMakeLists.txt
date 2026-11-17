# Data contracts, editing state, codecs and geometry remain usable without Widgets.
qt_add_library(FuseVisionDeepLearningData STATIC
    domain/AnnotationShape.h domain/AnnotationShape.cpp
    domain/DatasetTypes.h domain/ModelTypes.h domain/ProjectTypes.h
    model/AnnotationModel.h model/AnnotationModel.cpp
    render/AnnotationRender.h render/AnnotationRender.cpp
    storage/ProjectConfig.h storage/ProjectConfig.cpp
    storage/ModelConfig.h storage/ModelConfig.cpp
    storage/StorageContract.h
    storage/AnnotationStore.h storage/AnnotationStore.cpp
    storage/AnnotationIO.h storage/AnnotationIO.cpp
    storage/DatasetFileService.h storage/DatasetFileService.cpp
    tools/AnnotationTool.h tools/AnnotationTool.cpp
    tools/BrushTool.h tools/BrushTool.cpp
    tools/EraserTool.h tools/EraserTool.cpp
    tools/PolygonTool.h tools/PolygonTool.cpp
    tools/RectTool.h tools/RectTool.cpp
    tools/FillTool.h tools/FillTool.cpp
    tools/SelectTool.h tools/SelectTool.cpp
)
target_include_directories(FuseVisionDeepLearningData PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(FuseVisionDeepLearningData PUBLIC FuseVisionCore Qt6::Gui)

qt_add_library(FuseVisionDeepLearning STATIC
    widgets/DeepLearningWidget.h widgets/DeepLearningWidget.cpp
    widgets/ProjectManagement.h widgets/ProjectManagement.cpp
    widgets/ModelManagement.h widgets/ModelManagement.cpp
    widgets/DatasetManagement.h widgets/DatasetManagement.cpp
    widgets/AnnotationManagement.h widgets/AnnotationManagement.cpp
    widgets/AnnotationCanvas.h widgets/AnnotationCanvas.cpp
)
target_link_libraries(FuseVisionDeepLearning PUBLIC FuseVisionUiCommon FuseVisionDeepLearningData)

