cmake_minimum_required(VERSION 3.15)
project(maxminddb_ext LANGUAGES C)
set(CMAKE_C_STANDARD 23)

# Load our configurable options.
include(${CMAKE_SOURCE_DIR}/cmake/Options.cmake)

# Include libmaxminddb
include(${CMAKE_SOURCE_DIR}/cmake/MaxMindDB.cmake)

# Create our shared library.
add_library(maxminddb_ext SHARED ${CMAKE_SOURCE_DIR}/source/sqlite3_maxminddb.c)

# Find SQLite headers
target_include_directories(maxminddb_ext PRIVATE ${CMAKE_SOURCE_DIR}/source)

# Link our required libraries.
target_link_libraries(maxminddb_ext PRIVATE mmdb sqlite3)