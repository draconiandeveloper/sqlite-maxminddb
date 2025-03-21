find_program(CPPCHECK_FOUND NAMES cppcheck)

if(NOT CPPCHECK_FOUND)
    message(FATAL_ERROR "CPPCheck is required for testing purposes, you can disable CPPCheck with -DENABLE_CPPCHECK_TESTS=OFF")
endif()

message("Found CPPCheck at ${CPPCHECK_FOUND}")

add_test(NAME CPPCHECK_TEST 
    COMMAND ${CPPCHECK_FOUND}
        --check-level=exhaustive
        --language=c
        --enable=all
        --platform=unix64
        --suppress=missingIncludeSystem
        --suppress=missingInclude
        --suppress=unusedFunction
        --suppress=unusedStructMember
        --suppress=unmatchedSuppression
        --suppress=shadowVariable
        --suppress=shiftTooManyBits
        --suppress=checkersReport
        --suppress=varFuncNullUB
        --error-exitcode=1
        ${CMAKE_SOURCE_DIR}/source
)