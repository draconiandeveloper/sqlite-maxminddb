# Create an option for pulling the libmaxminddb repository.
option(GIT_PULL_LIBMAXMINDDB "Automatically retrieve the libmaxminddb source code from GitHub." ON)

# Create an option for enabling Valgrind testing.
option(ENABLE_VALGRIND_TESTS "Perform Valgrind memory leak tests." ON)

# Create an option for enabling CPPCheck code formatting tests.
option(ENABLE_CPPCHECK_TESTS "Perform CPPCheck code analysis." ON)

# Create an option for enabling SQLite3 extension testing.
option(ENABLE_SQLITE3_TESTS "Perform multiple SQLite3 queries on known public IP addresses." ON)

# Enable Doxygen to generate documentation from the code.
option(USE_DOXYGEN "Run Doxygen to generate documentation." ON)

###

if(GIT_PULL_LIBMAXMINDDB)
    include(FetchContent)
    
    FetchContent_Declare(
        libmaxminddb
        GIT_REPOSITORY https://github.com/maxmind/libmaxminddb.git
        GIT_TAG        cba618d6581b7dbe83478c798d9e58faeaa6b582 # release 1.12.2
        SOURCE_DIR     "${CMAKE_SOURCE_DIR}/libmaxminddb"
    )

    FetchContent_MakeAvailable(libmaxminddb)
endif()

if(ENABLE_VALGRIND_TESTS)
    enable_testing()
    include(${CMAKE_SOURCE_DIR}/cmake/Valgrind.cmake)
    set(BUILD_LIBFILE "")

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(BUILD_LIBFILE "${CMAKE_BINARY_DIR}/libmaxminddb_ext.so")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(BUILD_LIBFILE "${CMAKE_BINARY_DIR}/maxminddb_ext.dll")
    else()
        set(BUILD_LIBFILE "${CMAKE_BINARY_DIR}/libmaxminddb_ext.dylib")
    endif()

    VALGRIND_TOOL(${BUILD_LIBFILE} memcheck)
    #VALGRIND_TOOL(${BUILD_LIBFILE} cachegrind)
    #VALGRIND_TOOL(${BUILD_LIBFILE} callgrind)
    #VALGRIND_TOOL(${BUILD_LIBFILE} helgrind)
    #VALGRIND_TOOL(${BUILD_LIBFILE} drd)
    #VALGRIND_TOOL(${BUILD_LIBFILE} massif)
    #VALGRIND_TOOL(${BUILD_LIBFILE} dhat)
    #VALGRIND_TOOL(${BUILD_LIBFILE} lackey)
    #VALGRIND_TOOL(${BUILD_LIBFILE} exp-bbv)
endif()

if(ENABLE_CPPCHECK_TESTS)
    enable_testing()
    include(${CMAKE_SOURCE_DIR}/cmake/CPPCheck.cmake)
endif()

if(ENABLE_SQLITE3_TESTS)
    # TODO: Incorporate testing method that runs multiple
    #       SQLite queries retrieve and check the output.
endif()

if(USE_DOXYGEN)
    include(${CMAKE_SOURCE_DIR}/cmake/Doxygen.cmake)
endif()