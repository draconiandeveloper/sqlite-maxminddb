# Include our libmaxminddb header files (if available).
find_path(LIBMAXMINDDB_INCLUDE_DIR NAMES maxminddb.h)

# Show a message if we can't find libmaxminddb.
if(LIBMAXMINDDB_INCLUDE_DIR-NOTFOUND)
    message(STATUS "Skipping libmaxminddb, though it is recommended to clone the latest release in \"${CMAKE_SOURCE_DIR}\" before building!")
endif()

# Include libmaxminddb's headers if found.
include_directories(LIBMAXMINDDB_INCLUDE_DIR)

# Check for 128-bit integer support.
include(CheckTypeSize)
check_type_size("unsigned __int128" UINT128)
check_type_size("unsigned int __attribute__((mode(TI)))" UINT128_USING_MODE)

if(HAVE_UINT128)
    set(MMDB_UINT128_USING_MODE 0)
    set(MMDB_UINT128_IS_BYTE_ARRAY 0)
elseif(HAVE_UINT128_USING_MODE)
    set(MMDB_UINT128_USING_MODE 1)
    set(MMDB_UINT128_IS_BYTE_ARRAY 0)
else()
    set(MMDB_UINT128_USING_MODE 0)
    set(MMDB_UINT128_IS_BYTE_ARRAY 1)
endif()

# Add the libmaxminddb project.
if(NOT LIBMAXMINDDB_INCLUDE_DIR-NOTFOUND)
    set(MAXMINDDB_SOVERSION 0.0.7)

    configure_file(
        ${CMAKE_SOURCE_DIR}/libmaxminddb/include/maxminddb_config.h.cmake.in
        ${CMAKE_SOURCE_DIR}/libmaxminddb/generated/maxminddb_config.h
    )

    add_library(mmdb STATIC
        ${CMAKE_SOURCE_DIR}/libmaxminddb/src/maxminddb.c
        ${CMAKE_SOURCE_DIR}/libmaxminddb/src/data-pool.c
    )

    set_source_files_properties(${CMAKE_SOURCE_DIR}/libmaxminddb/src/maxminddb.c PROPERTIES COMPILE_FLAGS "-w")

    set_target_properties(mmdb PROPERTIES VERSION "1.12.2")
    target_compile_definitions(mmdb PRIVATE PACKAGE_VERSION="1.12.2")
    
    target_include_directories(mmdb 
        PRIVATE ${CMAKE_SOURCE_DIR}/libmaxminddb/src
        PRIVATE ${CMAKE_SOURCE_DIR}/libmaxminddb/include
        PRIVATE ${CMAKE_SOURCE_DIR}/libmaxminddb/generated)
endif()