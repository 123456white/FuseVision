# =============================================================================
# FindMVS.cmake — 检测海康 MVS SDK
# =============================================================================
# 用法: include(cmake/FindMVS.cmake)
#
# 输出变量:
#   MVS_INCLUDE_DIR  - 头文件路径
#   MVS_LIBRARY_DIR  - 库文件路径
#   MVS_LIBRARIES    - 库列表（空 = 未找到）
# =============================================================================

set(MVS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sdk/MVS")
set(MVS_INCLUDE_DIR "${MVS_SDK_ROOT}/Includes")
set(MVS_LIBRARY_DIR "${MVS_SDK_ROOT}/Libraries/win64")

if(EXISTS "${MVS_INCLUDE_DIR}" AND EXISTS "${MVS_LIBRARY_DIR}")
    set(MVS_LIBRARIES MvCameraControl)
    message(STATUS "MVS SDK configured for Windows")
else()
    set(MVS_LIBRARIES "")
    message(WARNING "MVS SDK not found in sdk/MVS")
endif()
