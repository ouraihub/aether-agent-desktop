add_executable(aether_message_test message_test.c)
target_link_libraries(aether_message_test PRIVATE aether_core)
add_test(NAME aether_message_test COMMAND aether_message_test)

add_executable(aether_message_blocks_test message_blocks_test.c)
target_link_libraries(aether_message_blocks_test PRIVATE aether_core)
add_test(NAME aether_message_blocks_test COMMAND aether_message_blocks_test)

add_executable(aether_message_allocation_test message_allocation_test.c
    ${PROJECT_SOURCE_DIR}/src/core/message.c
    ${PROJECT_SOURCE_DIR}/src/core/messages/apply.c
    ${PROJECT_SOURCE_DIR}/src/core/messages/payload.c
    ${PROJECT_SOURCE_DIR}/src/core/messages/association.c)
target_include_directories(aether_message_allocation_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/core/internal)
target_compile_definitions(aether_message_allocation_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free)
target_link_libraries(aether_message_allocation_test PRIVATE aether_core)
add_test(NAME aether_message_allocation_test COMMAND aether_message_allocation_test)

foreach(target aether_message_test aether_message_blocks_test
               aether_message_allocation_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
