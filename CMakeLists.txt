cmake_minimum_required(VERSION 3.15)
project(maxminddb_ext LANGUAGES C)
set(CMAKE_C_STANDARD 23)

# Load our configurable options.
include(${CMAKE_SOURCE_DIR}/cmake/Options.cmake)

# Find SQLite3 dependency.
find_package(SQLite3 REQUIRED)

# Include our libmaxminddb header files (if available).
find_path(LIBMAXMINDDB_INCLUDE_DIR NAMES maxminddb.h)

if(LIBMAXMINDDB_INCLUDE_DIR-NOTFOUND)
    message(STATUS "Skipping libmaxminddb, though it is recommended to clone the latest release in \"${CMAKE_SOURCE_DIR}\" before building!")
endif()

include_directories(LIBMAXMINDDB_INCLUDE_DIR)
include_directories(SQLite::SQLite3)

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

# Create our shared library.
add_library(maxminddb_ext SHARED ${CMAKE_SOURCE_DIR}/source/sqlite3_maxminddb.c)

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
    target_include_directories(mmdb PRIVATE ${CMAKE_SOURCE_DIR}/libmaxminddb/src)
    target_include_directories(mmdb PRIVATE ${CMAKE_SOURCE_DIR}/libmaxminddb/include)
    target_include_directories(mmdb PRIVATE ${CMAKE_SOURCE_DIR}/libmaxminddb/generated)
    target_link_libraries(maxminddb_ext PRIVATE mmdb)
endif()

# Link our required libraries.
target_link_libraries(maxminddb_ext PRIVATE SQLite::SQLite3)