find_package(Doxygen REQUIRED)

if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is required for documentation,  you can disable Doxygen with -DUSE_DOXYGEN=OFF")
endif()

set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_SOURCE_DIR}/Doxyfile)

configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
add_custom_target(doxygen ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/source
    COMMENT "Generating documentation with Doxygen"
    VERBATIM)
