add_library(aether_executor STATIC executor.c work.c timer.c)
target_include_directories(aether_executor PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(aether_executor PUBLIC aether_platform)

add_executable(aether_executor_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/executor_test.c)
target_link_libraries(aether_executor_test PRIVATE aether_executor)
add_test(NAME aether_executor_test COMMAND aether_executor_test)
set_tests_properties(aether_executor_test PROPERTIES TIMEOUT 30)

add_executable(aether_executor_timer_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/executor_timer_test.c)
target_link_libraries(aether_executor_timer_test PRIVATE aether_executor)
add_test(NAME aether_executor_timer_test COMMAND aether_executor_timer_test)
set_tests_properties(aether_executor_timer_test PROPERTIES TIMEOUT 30)

add_executable(aether_executor_failure_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/executor_failure_test.c
    executor.c work.c timer.c)
target_include_directories(aether_executor_failure_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(aether_executor_failure_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free
    aether_platform_thread_create=aether_test_thread_create
    aether_platform_monitor_create=aether_test_monitor_create)
target_link_libraries(aether_executor_failure_test PRIVATE aether_platform)
add_test(NAME aether_executor_failure_test COMMAND aether_executor_failure_test)
set_tests_properties(aether_executor_failure_test PROPERTIES TIMEOUT 30)

foreach(target aether_executor aether_executor_test aether_executor_timer_test
               aether_executor_failure_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
