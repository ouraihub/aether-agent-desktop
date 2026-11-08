add_executable(aether_backend_query_test query_test.c)
target_link_libraries(aether_backend_query_test PRIVATE aether_backend)
add_test(NAME aether_backend_query_test COMMAND aether_backend_query_test)

add_executable(aether_backend_query_allocation_test query_allocation_test.c
    ${PROJECT_SOURCE_DIR}/src/backend/api/query.c
    ${PROJECT_SOURCE_DIR}/src/backend/api/query_content.c)
target_include_directories(aether_backend_query_allocation_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/backend/internal)
target_compile_definitions(aether_backend_query_allocation_test PRIVATE
    malloc=aether_test_malloc free=aether_test_free)
target_link_libraries(aether_backend_query_allocation_test PRIVATE aether_core)
add_test(NAME aether_backend_query_allocation_test
    COMMAND aether_backend_query_allocation_test)

add_executable(aether_backend_async_query_test async_query_test.c)
target_link_libraries(aether_backend_async_query_test PRIVATE aether_backend)
add_test(NAME aether_backend_async_query_test COMMAND aether_backend_async_query_test)
set_tests_properties(aether_backend_async_query_test PROPERTIES TIMEOUT 30)

add_executable(aether_backend_async_close_test async_query_close_test.c
    ${PROJECT_SOURCE_DIR}/src/backend/api/async_query.c)
target_compile_definitions(aether_backend_async_close_test PRIVATE
    aether_backend_query_block=aether_test_query_block)
target_link_libraries(aether_backend_async_close_test PRIVATE
    aether_backend aether_executor)
add_test(NAME aether_backend_async_close_test COMMAND aether_backend_async_close_test)
set_tests_properties(aether_backend_async_close_test PROPERTIES TIMEOUT 30)

add_executable(aether_backend_async_allocation_test async_query_allocation_test.c
    ${PROJECT_SOURCE_DIR}/src/backend/api/async_query.c)
target_compile_definitions(aether_backend_async_allocation_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free)
target_link_libraries(aether_backend_async_allocation_test PRIVATE
    aether_backend aether_executor)
add_test(NAME aether_backend_async_allocation_test
    COMMAND aether_backend_async_allocation_test)
set_tests_properties(aether_backend_async_allocation_test PROPERTIES TIMEOUT 30)

add_executable(aether_backend_state_command_test state_command_test.c)
target_link_libraries(aether_backend_state_command_test PRIVATE aether_backend)
add_test(NAME aether_backend_state_command_test COMMAND aether_backend_state_command_test)
set_tests_properties(aether_backend_state_command_test PROPERTIES TIMEOUT 30)

add_executable(aether_backend_state_cancel_test state_command_cancel_test.c
    ${PROJECT_SOURCE_DIR}/src/backend/api/async_query.c)
target_compile_definitions(aether_backend_state_cancel_test PRIVATE
    aether_backend_rename_workspace=aether_test_rename_workspace)
target_link_libraries(aether_backend_state_cancel_test PRIVATE
    aether_backend aether_executor)
add_test(NAME aether_backend_state_cancel_test COMMAND aether_backend_state_cancel_test)
set_tests_properties(aether_backend_state_cancel_test PROPERTIES TIMEOUT 30)

foreach(target aether_backend_query_test aether_backend_query_allocation_test
               aether_backend_async_query_test aether_backend_async_close_test
               aether_backend_async_allocation_test aether_backend_state_command_test
               aether_backend_state_cancel_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
