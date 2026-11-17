qt_add_library(FuseVisionCore STATIC
    AccessControl.h
    AtomicFile.h
    RecoveryStore.h
    Logger.h Logger.cpp
    DatabaseManager.h DatabaseManager.cpp
    PermissionInfo.h
    PermissionRegistry.h PermissionRegistry.cpp
    SessionManager.h SessionManager.cpp
    SettingsManager.h SettingsManager.cpp
)
target_include_directories(FuseVisionCore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(FuseVisionCore PUBLIC Qt6::Core Qt6::Sql PRIVATE spdlog::spdlog)
target_compile_definitions(FuseVisionCore PRIVATE FUSEVISION_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
