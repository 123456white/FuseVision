# Documentation is optional for application builds; an explicit executable is cacheable.
find_program(DOXYGEN_EXECUTABLE NAMES doxygen
    HINTS "$ENV{DOXYGEN_ROOT}" "$ENV{DOXYGEN_ROOT}/bin"
    DOC "Path to the Doxygen executable")
if(DOXYGEN_EXECUTABLE)
    add_custom_target(docs
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/out/docs/doxygen"
        COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/docs/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Generating FuseVision API documentation"
        VERBATIM)
else()
    message(STATUS "Doxygen not found; set DOXYGEN_ROOT or DOXYGEN_EXECUTABLE to enable the docs target")
endif()
