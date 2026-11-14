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
project(FuseVision
    VERSION 1.0.0
    DESCRIPTION "FuseVision Studio Application"
    LANGUAGES CXX
)

include(CTest)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


# 查找依赖
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)
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

#（管理所有非 main 的源文件）
add_subdirectory(src)

# 创建可执行目标【关键：必须先创建目标，再设置属性】
add_executable(${PROJECT_NAME}
    "src/main.cpp"
    ${QT_RESOURCES}
    "FuseVision.rc"
)

# Windows 控制台配置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,ON,OFF>
    )
    # Windows 编码修复
    if(DEFINED ENV{CHCP})
        execute_process(COMMAND chcp 65001 OUTPUT_QUIET)
    endif()
endif()

# 头文件包含路径（需要包含 src 目录，以便找到各子模块头文件）
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MVS_INCLUDE_DIR}
)
if(HALCON_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${HALCON_INCLUDE_DIR} ${HALCONCPP_INCLUDE_DIR})
endif()

target_link_directories(${PROJECT_NAME} PRIVATE
    ${MVS_LIBRARY_DIR}
)
if(HALCON_FOUND)
    target_link_directories(${PROJECT_NAME} PRIVATE ${HALCON_LIB_DIR})
endif()

# ========== 链接库 ==========
target_link_libraries(${PROJECT_NAME} PRIVATE
    FuseVisionUI
    FuseVisionCore
    FuseVisionDeepLearning
    FuseVisionTraditional
    dwmapi
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    spdlog::spdlog
    ${MVS_LIBRARIES}
    ${OpenCV_LIBS}
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FUSEVISION_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
if(HALCON_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE halcon halconcpp)
endif()

# ========== 构建后部署运行时依赖 ==========
include(cmake/DeployRuntime.cmake)
if(WIN32)
    fusevision_deploy_runtime(TARGET ${PROJECT_NAME})
endif()

if(BUILD_TESTING)
    add_test(NAME startup-smoke COMMAND $<TARGET_FILE:${PROJECT_NAME}>)
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
