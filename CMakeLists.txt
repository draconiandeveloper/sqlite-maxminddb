cmake_minimum_required(VERSION 3.15)
project(maxminddb_ext LANGUAGES C)
set(CMAKE_C_STANDARD 23)

# Load our configurable options.
include(${CMAKE_SOURCE_DIR}/cmake/Options.cmake)

# Append our cmake directory data to the CMake module path.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find SQLite3
find_package(SQLiteCross REQUIRED)

# Prevent any further configuration if SQLite3 is not found on the system.
if(NOT SQLiteCross_FOUND)
    message(FATAL_ERROR "Could not find SQLite 3 libraries, make sure to check your PATH variable.")
endif()

# Include libmaxminddb
include(${CMAKE_SOURCE_DIR}/cmake/MaxMindDB.cmake)

# Create our shared library.
add_library(maxminddb_ext SHARED ${CMAKE_SOURCE_DIR}/source/sqlite3_maxminddb.c)
target_include_directories(maxminddb_ext PRIVATE ${SQLiteCross_INCLUDE_DIR})

# Link our required libraries.
target_link_libraries(maxminddb_ext PRIVATE mmdb ${SQLiteCross_LIBRARY})