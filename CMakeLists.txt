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

# ========== Halcon 配置（自动切换：系统安装版 → sdk/halcon 内置版 → 禁用） ==========
# 检测优先级：
#   1. 系统安装的 Halcon（HALCONROOT / HALCONARCH 环境变量）
#   2. 项目自带的 sdk/halcon 目录（CI / 无 Halcon 环境时使用）
#   3. 均未找到 → 禁用 Halcon 功能，编译时跳过相关代码
set(HALCON_FOUND OFF)
set(HALCON_SOURCE "none")

# ── 优先检测系统安装的 Halcon ──
if(DEFINED ENV{HALCONROOT} AND DEFINED ENV{HALCONARCH})
    set(HALCON_INCLUDE_DIR "$ENV{HALCONROOT}/include")
    set(HALCONCPP_INCLUDE_DIR "$ENV{HALCONROOT}/include/halconcpp")
    set(HALCON_LIB_DIR "$ENV{HALCONROOT}/lib/$ENV{HALCONARCH}")
    set(HALCON_BIN_DIR "$ENV{HALCONROOT}/bin/$ENV{HALCONARCH}")
    set(HALCON_FOUND ON)
    set(HALCON_SOURCE "system")
    message(STATUS "✅ Halcon 已检测到（系统安装）: $ENV{HALCONROOT} (架构: $ENV{HALCONARCH})")

# ── 回退到 sdk/halcon 内置 SDK ──
else()
    set(HALCON_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk/halcon")
    set(HALCON_SDK_INCLUDE "${HALCON_SDK_DIR}/include")
    set(HALCON_SDK_LIB "${HALCON_SDK_DIR}/lib")
    set(HALCON_SDK_BIN "${HALCON_SDK_DIR}/bin")

    if((EXISTS "${HALCON_SDK_INCLUDE}/HalconCpp.h" OR EXISTS "${HALCON_SDK_INCLUDE}/halconcpp/HalconCpp.h") AND
       EXISTS "${HALCON_SDK_LIB}/halcon.lib" AND
       EXISTS "${HALCON_SDK_LIB}/halconcpp.lib")
        set(HALCON_INCLUDE_DIR "${HALCON_SDK_INCLUDE}")
        set(HALCONCPP_INCLUDE_DIR "${HALCON_SDK_INCLUDE}/halconcpp")
        set(HALCON_LIB_DIR "${HALCON_SDK_LIB}")
        set(HALCON_BIN_DIR "${HALCON_SDK_BIN}")
        set(HALCON_FOUND ON)
        set(HALCON_SOURCE "sdk/halcon")
        message(STATUS "✅ Halcon 已检测到（sdk/halcon 内置 SDK）: ${HALCON_SDK_DIR}")
    else()
        set(HALCON_INCLUDE_DIR "")
        set(HALCONCPP_INCLUDE_DIR "")
        set(HALCON_LIB_DIR "")
        set(HALCON_BIN_DIR "")
        message(WARNING "⚠️  Halcon 未检测到：系统未安装且 sdk/halcon 目录不完整。传统视觉模块的 Halcon 功能将不可用。")
        message(STATUS "   期望的 sdk/halcon 结构: include/HalconCpp.h (或 include/halconcpp/HalconCpp.h), lib/halcon.lib, lib/halconcpp.lib")
    endif()
endif()

# 设置MVS SDK路径（已从 src/ 迁移至 sdk/）
set(MVS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sdk/MVS")
set(MVS_INCLUDE_DIR "${MVS_SDK_ROOT}/Includes")
set(MVS_LIBRARY_DIR "${MVS_SDK_ROOT}/Libraries/win64")

if(EXISTS "${MVS_INCLUDE_DIR}" AND EXISTS "${MVS_LIBRARY_DIR}")
    set(MVS_LIBRARIES MvCameraControl)
    message(STATUS "MVS SDK configured for Windows")
    message(STATUS "MVS Include Directory: ${MVS_INCLUDE_DIR}")
    message(STATUS "MVS Library Directory: ${MVS_LIBRARY_DIR}")
else()
    set(MVS_LIBRARIES "")
    message(WARNING "MVS SDK not found in expected locations:")
    message(WARNING "Include dir: ${MVS_INCLUDE_DIR}")
    message(WARNING "Library dir: ${MVS_LIBRARY_DIR}")
endif()

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
if(HALCON_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE halcon halconcpp)
endif()

# ========== Windows Qt 平台插件自动复制（区分 Debug/Release 模式，适配单/多配置生成器） ==========
if(WIN32)
    # 插件源路径：生成器表达式自动区分Debug/Release，兼容所有CMake生成器
    set(QT_PLATFORM_PLUGIN_SRC "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/platforms")
    # 插件目标路径：修复生成器表达式语法错误，正确获取exe所在目录并拼接platforms
    set(QT_PLATFORM_PLUGIN_DST "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms")
    
    # 构建后增量复制插件（仅文件变更时执行）
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_PLATFORM_PLUGIN_SRC}"
            "${QT_PLATFORM_PLUGIN_DST}"
        COMMENT "✅ 复制 Qt 平台插件($<CONFIG>)到：${QT_PLATFORM_PLUGIN_DST}"
        VERBATIM
    )

    # ========== 复制 SQLite 数据库驱动（区分 Debug/Release） ==========
    # Debug 版本使用 qsqlited.dll，Release 版本使用 qsqlite.dll
    set(QT_SQL_DRIVER_NAME "$<IF:$<CONFIG:Debug>,qsqlited.dll,qsqlite.dll>")
    set(QT_SQL_PLUGIN_SRC "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/sqldrivers")
    set(QT_SQL_PLUGIN_DST "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers")
    
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${QT_SQL_PLUGIN_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_SQL_PLUGIN_SRC}/${QT_SQL_DRIVER_NAME}" ${QT_SQL_PLUGIN_DST}
        COMMENT "✅ 复制 Qt SQLite 驱动($<CONFIG>)到：${QT_SQL_PLUGIN_DST}"
        VERBATIM
    )

    # ========== 复制 Qt 图像格式插件（JPG/PNG/GIF/ICO 等） ==========
    # 项目使用本地 vcpkg 子模块，vcpkg_installed 在 build 目录下
    set(QT_IMAGE_PLUGIN_SRC "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/imageformats")
    set(QT_IMAGE_PLUGIN_DST "$<TARGET_FILE_DIR:${PROJECT_NAME}>/imageformats")
    
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${QT_IMAGE_PLUGIN_DST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_IMAGE_PLUGIN_SRC}"
            "${QT_IMAGE_PLUGIN_DST}"
        COMMENT "Copy Qt imageformats plugins ($<CONFIG>) -> imageformats/"
        VERBATIM
    )

    # ========== 复制 JPEG 运行时依赖 ==========
    # qjpegd.dll 依赖 jpeg62.dll / turbojpeg.dll，vcpkg 自动部署可能遗漏
    set(VCPKG_DEBUG_BIN "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/bin")
    set(OUTPUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_DEBUG_BIN}/jpeg62.dll"
            "${OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_DEBUG_BIN}/turbojpeg.dll"
            "${OUTPUT_DIR}"
        COMMENT "Copy JPEG runtime DLLs -> ${OUTPUT_DIR}"
        VERBATIM
    )
    # ========== 复制 Halcon 运行时 DLL（系统安装版 / sdk/halcon 内置版） ==========
    if(HALCON_FOUND)
        set(OUTPUT_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${HALCON_BIN_DIR}/halcon.dll" ${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${HALCON_BIN_DIR}/hdevenginecpp.dll" ${OUTPUT_DIR}
            COMMENT "✅ 复制 Halcon 运行时 DLL (${HALCON_SOURCE}) 到输出目录"
            VERBATIM
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