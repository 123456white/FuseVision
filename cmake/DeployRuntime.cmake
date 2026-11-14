# =============================================================================
# DeployRuntime.cmake — 构建后复制运行时依赖到输出目录
# =============================================================================
# 用法: include(cmake/DeployRuntime.cmake)
#       fusevision_deploy_runtime(TARGET <target_name>)
#
# 功能:
#   - Qt 平台插件 (qwindows.dll)
#   - Qt SQL 驱动 (qsqlite.dll)
#   - Qt 图像格式插件 (qjpeg.dll 等)
#   - JPEG 运行时 DLL (jpeg62.dll, turbojpeg.dll)
#   - Halcon 运行时 DLL (halcon.dll, hdevenginecpp.dll)
#   - sdk/labelme 源码目录
# =============================================================================

function(fusevision_deploy_runtime)
    cmake_parse_arguments(ARG "" "TARGET" "" ${ARGN})
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "fusevision_deploy_runtime: TARGET is required")
    endif()

    set(OUT_DIR "$<TARGET_FILE_DIR:${ARG_TARGET}>")

    # ── Qt 平台插件 ──────────────────────────────────────────
    add_custom_command(
        TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/platforms"
            "${OUT_DIR}/platforms"
        COMMENT "Copy Qt platform plugin ($<CONFIG>) -> platforms/"
        VERBATIM
    )

    # ── Qt SQLite 驱动 ───────────────────────────────────────
    add_custom_command(
        TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/sqldrivers/$<IF:$<CONFIG:Debug>,qsqlited.dll,qsqlite.dll>"
            "${OUT_DIR}/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>bin/sqlite3.dll"
            "${OUT_DIR}"
        COMMENT "Copy Qt SQLite driver ($<CONFIG>) -> sqldrivers/"
        VERBATIM
    )

    # ── Qt 图像格式插件 ──────────────────────────────────────
    add_custom_command(
        TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/imageformats"
            "${OUT_DIR}/imageformats"
        COMMENT "Copy Qt imageformat plugins ($<CONFIG>) -> imageformats/"
        VERBATIM
    )

    # ── JPEG 运行时 DLL ─────────────────────────────────────
    add_custom_command(
        TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>bin/jpeg62.dll"
            "${OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>bin/turbojpeg.dll"
            "${OUT_DIR}"
        COMMENT "Copy JPEG runtime DLLs -> ${OUT_DIR}"
        VERBATIM
    )

    # ── Halcon 运行时 DLL ───────────────────────────────────
    if(HALCON_FOUND)
        add_custom_command(
            TARGET ${ARG_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HALCON_BIN_DIR}/halcon.dll" "${OUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HALCON_BIN_DIR}/hdevenginecpp.dll" "${OUT_DIR}"
            COMMENT "Copy Halcon runtime DLLs (${HALCON_SOURCE}) -> ${OUT_DIR}"
            VERBATIM
        )
    endif()

    # ── sdk/labelme (打包部署) ───────────────────────────────
    add_custom_command(
        TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/sdk/labelme"
            "${OUT_DIR}/sdk/labelme"
        COMMENT "Copy sdk/labelme -> ${OUT_DIR}/sdk/labelme"
        VERBATIM
    )
endfunction()
