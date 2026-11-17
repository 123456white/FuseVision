# CMakeList.txt: FuseVision 的 CMake 项目，在此处包括源代码并定义

# 规定项目必须使用 ≥3.21 版本的 CMake（与 vcpkg manifest 模式兼容的最低版本）
cmake_minimum_required(VERSION 3.21)
# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# 定义一个变量 QT_RESOURCES，指向你的资源文件 FuseVision.qrc
set(QT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/FuseVision.qrc)

# 项目定义（初始化编译器和系统变量）
option(BUILD_TESTING "Build regression tests" ON)
if(BUILD_TESTING)
    list(APPEND VCPKG_MANIFEST_FEATURES tests)
endif()
project(FuseVision
    VERSION 1.0.0
    DESCRIPTION "FuseVision Studio Application"
    LANGUAGES CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# vcpkg 内部消费此变量（消除 CMake "not used" 警告）
set(_vcpkg_bs_used "${VCPKG_BINARY_SOURCES}")

# 64位编译器检测（变量已初始化，检测有效）
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "
    构建失败：当前编译器为 32 位！
    本项目仅支持 x86_64 (64 位) 编译器，请切换至 64 位编译环境。
    Windows：使用 x64 Native Tools Command Prompt for VS 2022
    Linux/macOS：确认系统与编译器为 64 位版本
    ")
endif()
message(STATUS "检测通过：当前使用 64 位编译器，指针宽度: ${CMAKE_SIZEOF_VOID_P} 字节")

# 输出目录配置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
# Qt 自动编译配置
set(CMAKE_AUTORCC ON)


# 查找依赖
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Sql)
qt_standard_project_setup(REQUIRES 6.8)
find_package(spdlog REQUIRED)
find_package(OpenCV REQUIRED)

if(OpenCV_FOUND)
    message(STATUS "OpenCV version: ${OpenCV_VERSION}")
    message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "OpenCV not found! Please make sure OpenCV is installed via vcpkg.")
endif()

# ========== 第三方 SDK 检测 ==========
include(cmake/FindHalcon.cmake)
include(cmake/FindMVS.cmake)

# SDK usage requirements belong to the traditional vision module.
add_library(FuseVisionVisionSdk INTERFACE)
target_include_directories(FuseVisionVisionSdk INTERFACE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(FuseVisionVisionSdk INTERFACE ${OpenCV_LIBS})
if(MVS_LIBRARIES)
    target_include_directories(FuseVisionVisionSdk INTERFACE ${MVS_INCLUDE_DIR})
    target_link_directories(FuseVisionVisionSdk INTERFACE ${MVS_LIBRARY_DIR})
    target_link_libraries(FuseVisionVisionSdk INTERFACE ${MVS_LIBRARIES})
endif()
if(HALCON_FOUND)
    target_include_directories(FuseVisionVisionSdk INTERFACE ${HALCON_INCLUDE_DIR} ${HALCONCPP_INCLUDE_DIR})
    target_link_directories(FuseVisionVisionSdk INTERFACE ${HALCON_LIB_DIR})
    target_link_libraries(FuseVisionVisionSdk INTERFACE halcon halconcpp)
endif()

#（管理所有非 main 的源文件）
add_subdirectory(src)

# 创建可执行目标【关键：必须先创建目标，再设置属性】
qt_add_executable(${PROJECT_NAME}
    "src/main.cpp"
    ${QT_RESOURCES}
    "FuseVision.rc"
)

# Windows 控制台配置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,ON,OFF>
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    # Windows 编码修复
    if(DEFINED ENV{CHCP})
        execute_process(COMMAND chcp 65001 OUTPUT_QUIET)
    endif()
endif()

# The shell carries its private business dependencies; startup only consumes public APIs.
target_link_libraries(${PROJECT_NAME} PRIVATE FuseVisionUI)

include(cmake/ModuleBoundaries.cmake)
include(cmake/Documentation.cmake)

# ========== 构建后部署运行时依赖 ==========
include(cmake/DeployRuntime.cmake)
if(WIN32)
    fusevision_deploy_runtime(TARGET ${PROJECT_NAME})
endif()

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    qt_add_executable(FuseVisionStorageTests tests/storage_regression.cpp)
    target_link_libraries(FuseVisionStorageTests PRIVATE FuseVisionDeepLearningData GTest::gtest)
    gtest_discover_tests(FuseVisionStorageTests DISCOVERY_MODE PRE_TEST NO_PRETTY_VALUES
        TEST_PREFIX "storage." PROPERTIES TIMEOUT 30)
    qt_add_executable(FuseVisionNativeAnnotationTests tests/native_annotation_regression.cpp FuseVision.qrc)
    target_link_libraries(FuseVisionNativeAnnotationTests PRIVATE FuseVisionDeepLearning)
    add_test(NAME native-annotation-regression COMMAND $<TARGET_FILE:FuseVisionNativeAnnotationTests>)
    set_tests_properties(native-annotation-regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME documentation-tools COMMAND ${Python3_EXECUTABLE} -X utf8 -m unittest discover
        -s ${PROJECT_SOURCE_DIR}/tests -p "test_*.py" -v)
    set_tests_properties(documentation-tools PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} TIMEOUT 120)
    qt_add_executable(FuseVisionDataTests tests/data_contract_regression.cpp)
    target_link_libraries(FuseVisionDataTests PRIVATE FuseVisionDeepLearningData)
    add_test(NAME data-contract-regression COMMAND $<TARGET_FILE:FuseVisionDataTests>)
    set_tests_properties(data-contract-regression PROPERTIES TIMEOUT 30)
    add_test(NAME module-boundaries COMMAND ${Python3_EXECUTABLE} -X utf8 ${PROJECT_SOURCE_DIR}/tests/check_module_boundaries.py)
    set_tests_properties(module-boundaries PROPERTIES TIMEOUT 30)
    qt_add_executable(FuseVisionWorkflowTests tests/workflow_regression.cpp FuseVision.qrc)
    target_include_directories(FuseVisionWorkflowTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(FuseVisionWorkflowTests PRIVATE FuseVisionUI FuseVisionDeepLearning FuseVisionTraditional Qt6::Sql)
    add_test(NAME workflow-regression COMMAND $<TARGET_FILE:FuseVisionWorkflowTests>)
    set_tests_properties(workflow-regression PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )
    add_test(NAME startup-smoke COMMAND $<TARGET_FILE:${PROJECT_NAME}>)
    qt_add_executable(FuseVisionCurrentIssuesTests tests/current_issues_regression.cpp FuseVision.qrc)
    target_link_libraries(FuseVisionCurrentIssuesTests PRIVATE FuseVisionDeepLearning Qt6::Sql)
    add_test(NAME current-issues-regression COMMAND $<TARGET_FILE:FuseVisionCurrentIssuesTests>)
    set_tests_properties(current-issues-regression PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(startup-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;FUSEVISION_STARTUP_SMOKE_TEST=1"
        TIMEOUT 30
    )
    if(WIN32)
        set_property(TEST startup-smoke APPEND PROPERTY ENVIRONMENT
            "PATH=$ENV{SystemRoot}/System32"
            "QT_PLUGIN_PATH="
            "QT_QPA_PLATFORM_PLUGIN_PATH="
        )
    endif()
endif()

# ========== 打印构建信息 ==========
message(STATUS "========== 项目构建信息 ==========")
message(STATUS "项目名称：${PROJECT_NAME}")
message(STATUS "项目版本：${PROJECT_VERSION}")
message(STATUS "C++ 标准：${CMAKE_CXX_STANDARD}")
message(STATUS "构建类型：${CMAKE_BUILD_TYPE}")
message(STATUS "编译器：${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Qt 版本：${Qt6_VERSION}")
message(STATUS "Qt 库路径：${Qt6_DIR}")

message(STATUS "vcpkg 三元组：${VCPKG_TARGET_TRIPLET}")
message(STATUS "Halcon 来源：${HALCON_SOURCE}")
if(HALCON_FOUND)
    message(STATUS "Halcon 头文件目录：${HALCON_INCLUDE_DIR}")
    message(STATUS "Halcon 库目录：${HALCON_LIB_DIR}")
endif()
message(STATUS "MVS SDK 根目录：${MVS_SDK_ROOT}")
message(STATUS "OpenCV 版本：${OpenCV_VERSION}") 
message(STATUS "spdlog 版本：${spdlog_VERSION}")
message(STATUS "==================================")
