# The existing UI is a placeholder; this target owns the optional vision SDK links.
qt_add_library(FuseVisionTraditional STATIC TraditionalWidget.h TraditionalWidget.cpp)
target_link_libraries(FuseVisionTraditional PUBLIC FuseVisionUiCommon PRIVATE FuseVisionVisionSdk)
