add_library(FuseVisionUI STATIC
    FuseVision.h
    FuseVision.cpp
    Login.h
    Login.cpp
    UserManagementWidget.h
    UserManagementWidget.cpp
    SystemSettingsWidget.h
    SystemSettingsWidget.cpp
)

target_include_directories(FuseVisionUI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FuseVisionUI
    PUBLIC
        Qt6::Widgets
        FuseVisionCore
        FuseVisionDeepLearning
        FuseVisionTraditional
)
