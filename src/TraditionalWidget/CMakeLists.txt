add_library(FuseVisionTraditional STATIC
    TraditionalWidget.h
    TraditionalWidget.cpp
)

target_include_directories(FuseVisionTraditional
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FuseVisionTraditional
    PUBLIC
        Qt6::Widgets
)
