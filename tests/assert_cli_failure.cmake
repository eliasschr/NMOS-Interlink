if(NOT DEFINED CLI)
    message(FATAL_ERROR "CLI executable path is required")
endif()

if(CASE STREQUAL "invalid-option")
    execute_process(
        COMMAND "${CLI}" --not-an-option
        RESULT_VARIABLE result
        ERROR_VARIABLE stderr
    )
elseif(CASE STREQUAL "help-extra")
    execute_process(
        COMMAND "${CLI}" --help extra
        RESULT_VARIABLE result
        ERROR_VARIABLE stderr
    )
else()
    message(FATAL_ERROR "Unknown CLI failure test case: ${CASE}")
endif()

if(NOT result EQUAL 2)
    message(FATAL_ERROR "Expected exit code 2, got ${result}")
endif()

if(NOT stderr MATCHES "(unknown option|unexpected argument)")
    message(FATAL_ERROR "Expected an error diagnostic, got: ${stderr}")
endif()
