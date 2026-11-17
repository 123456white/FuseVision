qt_add_library(FuseVisionUI STATIC
    FuseVision.h FuseVision.cpp
    Login.h Login.cpp
    UserManagementWidget.h UserManagementWidget.cpp
    SystemSettingsWidget.h SystemSettingsWidget.cpp
)
# Business pages are implementation details of the application shell.
target_link_libraries(FuseVisionUI PUBLIC FuseVisionUiCommon PRIVATE FuseVisionDeepLearning FuseVisionTraditional)
if(WIN32)
    target_link_libraries(FuseVisionUI PRIVATE dwmapi)
endif()
