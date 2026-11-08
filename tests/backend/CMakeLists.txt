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

foreach(target aether_backend_query_test aether_backend_query_allocation_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
