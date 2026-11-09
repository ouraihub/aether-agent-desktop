if(NOT WIN32)
    return()
endif()

add_library(aether_network STATIC network.c)
target_include_directories(aether_network PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(aether_network PUBLIC aether_platform PRIVATE aether_curl)
target_compile_features(aether_network PUBLIC c_std_17)

add_executable(aether_network_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/network_test.c)
target_link_libraries(aether_network_test PRIVATE aether_network)
add_test(NAME aether_network_test COMMAND aether_network_test)
set_tests_properties(aether_network_test PROPERTIES TIMEOUT 30)
add_subdirectory(${PROJECT_SOURCE_DIR}/tests/infrastructure/network
    ${CMAKE_CURRENT_BINARY_DIR}/tests)

foreach(target aether_network aether_network_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
