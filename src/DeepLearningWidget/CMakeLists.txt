add_library(FuseVisionDeepLearning STATIC
    DeepLearningWidget.h
    DeepLearningWidget.cpp
    ProjectManagement.h
    ProjectManagement.cpp
    ProjectConfig.h
    ProjectConfig.cpp
    ModelConfig.h
    ModelConfig.cpp
    ModelManagement.h
    ModelManagement.cpp
    DatasetManagement.h
    DatasetManagement.cpp
    AnnotationManagement.h
    AnnotationManagement.cpp
    AnnotationCanvas.h
    AnnotationCanvas.cpp
    AnnotationModel.h
    AnnotationModel.cpp
    AnnotationRender.h
    AnnotationRender.cpp
    AnnotationIO.h
    AnnotationIO.cpp
    Tools/AnnotationTool.h
    Tools/AnnotationTool.cpp
    Tools/BrushTool.h
    Tools/BrushTool.cpp
    Tools/EraserTool.h
    Tools/EraserTool.cpp
    Tools/PolygonTool.h
    Tools/PolygonTool.cpp
    Tools/RectTool.h
    Tools/RectTool.cpp
    Tools/FillTool.h
    Tools/FillTool.cpp
    Tools/SelectTool.h
    Tools/SelectTool.cpp
)

target_include_directories(FuseVisionDeepLearning
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FuseVisionDeepLearning
    PUBLIC
        Qt6::Widgets
        FuseVisionCore
)

target_compile_definitions(FuseVisionDeepLearning PRIVATE
    FUSEVISION_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
