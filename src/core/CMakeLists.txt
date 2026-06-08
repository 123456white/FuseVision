add_library(FuseVisionCore STATIC
    Logger.h
    Logger.cpp
    PermissionInfo.h
    DatabaseManager.h
    DatabaseManager.cpp
    PermissionRegistry.h
    PermissionRegistry.cpp
    SessionManager.h
    SessionManager.cpp
    SettingsManager.h
    SettingsManager.cpp
    PermissionGuard.h
    PermissionGuard.cpp
    ThemeManager.h
    ThemeManager.cpp
    LogMonitor.h
    LogMonitor.cpp
)

target_include_directories(FuseVisionCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FuseVisionCore
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Sql
        spdlog::spdlog
)
