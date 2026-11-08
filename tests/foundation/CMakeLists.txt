foreach(kind exhaustion signed_boundary)
    set(target aether_id_${kind}_test)
    add_executable(${target} id_boundary_test.c
        ${PROJECT_SOURCE_DIR}/src/foundation/id.c)
    target_link_libraries(${target} PRIVATE aether_platform)
    if(kind STREQUAL "exhaustion")
        target_compile_definitions(${target} PRIVATE AETHER_TEST_ID_EXHAUSTION)
        if(MSVC)
            target_compile_definitions(${target} PRIVATE AETHER_ID_INITIAL_VALUE=-32)
        else()
            target_compile_definitions(${target} PRIVATE
                "AETHER_ID_INITIAL_VALUE=(UINT64_MAX-31U)")
        endif()
    else()
        target_compile_definitions(${target} PRIVATE
            "AETHER_ID_INITIAL_VALUE=(INT64_MAX-31)")
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 30)
endforeach()
