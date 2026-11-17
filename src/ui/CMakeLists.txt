qt_add_library(FuseVisionUiCommon STATIC
    WriteAccess.h WriteAccess.cpp
    PermissionGuard.h PermissionGuard.cpp
    ThemeManager.h ThemeManager.cpp
    LogMonitor.h LogMonitor.cpp
    LoadingOverlay.h LoadingOverlay.cpp
)
target_include_directories(FuseVisionUiCommon PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(FuseVisionUiCommon PUBLIC FuseVisionCore Qt6::Widgets)
