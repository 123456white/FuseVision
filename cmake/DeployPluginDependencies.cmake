# Plugins are loaded dynamically and are absent from the executable import table.
cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${DUMPBIN}")
set(plugin_root "${DEPENDENCY_DIR}/../Qt6/plugins")
file(GLOB plugins "${plugin_root}/imageformats/*.dll" "${plugin_root}/platforms/*.dll")
if(DEPENDENCY_DIR MATCHES "/debug/bin$")
    list(APPEND plugins "${plugin_root}/sqldrivers/qsqlited.dll")
else()
    list(APPEND plugins "${plugin_root}/sqldrivers/qsqlite.dll")
endif()
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES ${plugins}
    DIRECTORIES "${DEPENDENCY_DIR}"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*" ".*[Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\].*")
if(unresolved)
    message(FATAL_ERROR "Unresolved Qt plugin dependencies: ${unresolved}")
endif()
foreach(dependency IN LISTS resolved)
    # System DLLs stay on Windows; only deploy the manifest's vcpkg runtime files.
    cmake_path(IS_PREFIX DEPENDENCY_DIR "${dependency}" NORMALIZE from_vcpkg)
    if(from_vcpkg)
        file(COPY "${dependency}" DESTINATION "${RUNTIME_DIR}")
    endif()
endforeach()
