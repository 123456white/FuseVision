# =============================================================================
# FindHalcon.cmake — 检测 Halcon（系统安装 → sdk/halcon 内置 → 禁用）
# =============================================================================
# 用法: include(cmake/FindHalcon.cmake)
#
# 输出变量:
#   HALCON_FOUND          - ON/OFF
#   HALCON_SOURCE         - "system" | "sdk/halcon" | "none"
#   HALCON_INCLUDE_DIR    - 头文件路径
#   HALCONCPP_INCLUDE_DIR - halconcpp 头文件路径
#   HALCON_LIB_DIR        - 库文件路径
#   HALCON_BIN_DIR        - DLL 路径
# =============================================================================

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
    message(STATUS "Halcon detected (system): $ENV{HALCONROOT} (arch: $ENV{HALCONARCH})")

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
        message(STATUS "Halcon detected (sdk/halcon): ${HALCON_SDK_DIR}")
    else()
        set(HALCON_INCLUDE_DIR "")
        set(HALCONCPP_INCLUDE_DIR "")
        set(HALCON_LIB_DIR "")
        set(HALCON_BIN_DIR "")
        message(WARNING "Halcon not found: neither system install nor sdk/halcon is available.")
        message(STATUS "  Expected sdk/halcon structure: include/HalconCpp.h, lib/halcon.lib, lib/halconcpp.lib")
    endif()
endif()
