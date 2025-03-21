find_program(VALGRIND_FOUND NAMES valgrind)

if(NOT VALGRIND_FOUND)
    message(FATAL_ERROR "Valgrind is required for testing purposes, you can disable Valgrind with -DENABLE_VALGRIND_TESTS=OFF")
endif()

message("Found Valgrind at ${VALGRIND_FOUND}")

function(VALGRIND_TOOL FILENAME TOOLNAME)
    string(TOUPPER ${TOOLNAME} TOOLNAME_UPPER)
    set(VALGRIND_OPTIONS "--tool=${TOOLNAME}")

    if(${TOOLNAME} STREQUAL "memcheck")
        list(APPEND VALGRIND_OPTIONS "--leak-check=full" "--show-leak-kinds=all" "--track-origins=yes")
    endif()

    list(APPEND VALGRIND_OPTIONS "--show-error-list=yes" "--error-exitcode=1" "${FILENAME}")
    add_test(NAME VALGRIND_${TOOLNAME_UPPER}_TEST COMMAND ${VALGRIND_FOUND} ${VALGRIND_OPTIONS})
endfunction()
