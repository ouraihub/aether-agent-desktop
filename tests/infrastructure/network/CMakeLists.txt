# Test-only Windows loopback fixture; production code never links Winsock test IO.
if(NOT WIN32)
    return()
endif()

target_sources(aether_network_test PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/network_fixture.c
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/network_test_command.c
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/network_concurrency_test.c)
target_link_libraries(aether_network_test PRIVATE ws2_32 shell32)

find_program(AETHER_PWSH_EXECUTABLE NAMES pwsh)
if(AETHER_PWSH_EXECUTABLE)
    add_test(NAME aether_network_tls_untrusted_test
        COMMAND ${AETHER_PWSH_EXECUTABLE} -NoLogo -NoProfile -File
            ${PROJECT_SOURCE_DIR}/tests/infrastructure/network_test_runner.ps1
            -Executable $<TARGET_FILE:aether_network_test>
            -Scenario TlsUntrusted)
    add_test(NAME aether_network_fixture_failure_test
        COMMAND ${AETHER_PWSH_EXECUTABLE} -NoLogo -NoProfile -File
            ${PROJECT_SOURCE_DIR}/tests/infrastructure/network_test_runner.ps1
            -Executable $<TARGET_FILE:aether_network_test>
            -Scenario FixtureFailure)
    set_tests_properties(aether_network_fixture_failure_test PROPERTIES TIMEOUT 15)
    set_tests_properties(aether_network_tls_untrusted_test PROPERTIES TIMEOUT 20)
else()
    message(WARNING "PowerShell 7 unavailable: local TLS fixture is not registered")
endif()
