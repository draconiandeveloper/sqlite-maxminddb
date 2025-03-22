if(WIN32)
    find_library(SQLiteCross_LIBRARY NAMES sqlite3 PATHS "C:\\ProgramData\\chocolatey\\lib\\SQLite")
    find_path(SQLiteCross_INCLUDE_DIR NAMSE sqlite3.h PATHS "C:\\ProgramData\\chocolatey\\lib\\SQLite")
else()
    find_library(SQLiteCross_LIBRARY NAMES sqlite3)
    find_path(SQLiteCross_INCLUDE_DIR NAMES sqlite3.h)
endif()

mark_as_advanced(SQLiteCross_LIBRARY SQLiteCross_INCLUDE_DIR)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLiteCross 
    DEFAULT_MSG SQLiteCross_LIBRARY SQLiteCross_INCLUDE_DIR)

if(NOT SQLiteCross_FOUND)
    message(FATAL_ERROR "Unable to find SQLite 3 library!")
endif()