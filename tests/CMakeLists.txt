function(add_cli_success_test name argument output)
    add_test(
        NAME ${name}
        COMMAND $<TARGET_FILE:nmos-interlink> ${argument}
    )

    set_tests_properties(${name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${output}"
    )
endfunction()

add_cli_success_test(nmos-interlink-help --help "-h, --help")
add_cli_success_test(nmos-interlink-help-short -h "-h, --help")
add_cli_success_test(
    nmos-interlink-version
    --version
    "nmos-interlink ${PROJECT_VERSION_MAJOR}\\.${PROJECT_VERSION_MINOR}\\.${PROJECT_VERSION_PATCH}"
)
add_cli_success_test(
    nmos-interlink-version-short
    -V
    "nmos-interlink ${PROJECT_VERSION_MAJOR}\\.${PROJECT_VERSION_MINOR}\\.${PROJECT_VERSION_PATCH}"
)

function(add_cli_failure_test name case)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:nmos-interlink>
            -DCASE=${case}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/assert_cli_failure.cmake
    )
endfunction()

add_cli_failure_test(nmos-interlink-invalid-option invalid-option)
add_cli_failure_test(nmos-interlink-rejects-extra-positional-argument help-extra)
