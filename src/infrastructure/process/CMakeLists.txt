add_library(aether_process STATIC service.c io.c control.c pump.c)
target_link_libraries(aether_process PUBLIC aether_platform)
target_include_directories(aether_process PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(aether_process_child
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/process_child.c)
if(MSVC)
    set_property(TARGET aether_process_child PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
add_executable(aether_process_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/process_test.c)
target_link_libraries(aether_process_test PRIVATE aether_process)
add_test(NAME aether_process_test
    COMMAND aether_process_test $<TARGET_FILE:aether_process_child>)
set_tests_properties(aether_process_test PROPERTIES TIMEOUT 60)

add_executable(aether_process_allocation_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/process_allocation_test.c
    service.c io.c control.c pump.c)
target_compile_definitions(aether_process_allocation_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free)
target_link_libraries(aether_process_allocation_test PRIVATE aether_platform)
add_test(NAME aether_process_allocation_test COMMAND aether_process_allocation_test)
set_tests_properties(aether_process_allocation_test PROPERTIES TIMEOUT 30)

add_executable(aether_process_native_allocation_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/process_native_allocation_test.c
    ${PROJECT_SOURCE_DIR}/src/platform/windows/process_launch.c
    ${PROJECT_SOURCE_DIR}/src/platform/windows/process_spawn.c
    ${PROJECT_SOURCE_DIR}/src/platform/windows/process_io.c)
target_include_directories(aether_process_native_allocation_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/platform/internal)
target_compile_definitions(aether_process_native_allocation_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free)
target_link_libraries(aether_process_native_allocation_test PRIVATE aether_foundation)
add_test(NAME aether_process_native_allocation_test
    COMMAND aether_process_native_allocation_test)
set_tests_properties(aether_process_native_allocation_test PROPERTIES TIMEOUT 30)

add_executable(aether_process_gate_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/process_gate_test.c)
target_link_libraries(aether_process_gate_test PRIVATE aether_backend aether_process)
target_include_directories(aether_process_gate_test PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/core)
add_test(NAME aether_process_gate_test
    COMMAND aether_process_gate_test $<TARGET_FILE:aether_process_child>)
set_tests_properties(aether_process_gate_test PROPERTIES TIMEOUT 60)

add_executable(aether_process_gate_allocation_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/process_gate_allocation_test.c
    ${PROJECT_SOURCE_DIR}/src/backend/services/process_gate.c
    ${PROJECT_SOURCE_DIR}/src/platform/windows/launch_guard.c)
target_compile_definitions(aether_process_gate_allocation_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free)
target_include_directories(aether_process_gate_allocation_test PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/core)
target_link_libraries(aether_process_gate_allocation_test PRIVATE
    aether_core aether_process)
add_test(NAME aether_process_gate_allocation_test
    COMMAND aether_process_gate_allocation_test $<TARGET_FILE:aether_process_child>)
set_tests_properties(aether_process_gate_allocation_test PROPERTIES TIMEOUT 30)

foreach(target aether_process aether_process_child aether_process_test
               aether_process_allocation_test aether_process_native_allocation_test
               aether_process_gate_test aether_process_gate_allocation_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
