cmake_minimum_required(VERSION 3.21)
project(FuseVisionAnalysisCapture LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTORCC ON)
get_filename_component(FUSEVISION_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(EXISTING_BUILD "${FUSEVISION_ROOT}/out/build/windows-x64-release" CACHE PATH "Existing Release build")
list(PREPEND CMAKE_PREFIX_PATH "${EXISTING_BUILD}/vcpkg_installed/x64-windows")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Test)
find_package(spdlog REQUIRED)
add_executable(FuseVisionAnalysisCapture capture_ui.cpp "${FUSEVISION_ROOT}/FuseVision.qrc")
target_include_directories(FuseVisionAnalysisCapture PRIVATE "${FUSEVISION_ROOT}/src")
target_compile_options(FuseVisionAnalysisCapture PRIVATE /utf-8)
target_link_libraries(FuseVisionAnalysisCapture PRIVATE
    "${EXISTING_BUILD}/lib/Release/FuseVisionUI.lib"
    "${EXISTING_BUILD}/lib/Release/FuseVisionDeepLearning.lib"
    "${EXISTING_BUILD}/lib/Release/FuseVisionTraditional.lib"
    "${EXISTING_BUILD}/lib/Release/FuseVisionUiCommon.lib"
    "${EXISTING_BUILD}/lib/Release/FuseVisionDeepLearningData.lib"
    "${EXISTING_BUILD}/lib/Release/FuseVisionCore.lib"
    Qt6::Widgets Qt6::Sql Qt6::Test spdlog::spdlog dwmapi)
