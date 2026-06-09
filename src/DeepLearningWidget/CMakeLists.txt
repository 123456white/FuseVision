add_library(FuseVisionDeepLearning STATIC
    DeepLearningWidget.h
    DeepLearningWidget.cpp
    ProjectManagement.h
    ProjectManagement.cpp
    ProjectConfig.h
    ProjectConfig.cpp
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
