add_library(aether_credentials STATIC
    store.c
    json.c
    records.c)

target_include_directories(aether_credentials
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aether_credentials
    PUBLIC aether_foundation
    PRIVATE aether_sqlite3 aether_yyjson)
target_compile_features(aether_credentials PUBLIC c_std_17)

add_executable(aether_credentials_test
    ${PROJECT_SOURCE_DIR}/tests/infrastructure/credentials_test.c)
target_link_libraries(aether_credentials_test PRIVATE aether_credentials)
target_include_directories(aether_credentials_test PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/sqlite)
add_test(NAME aether_credentials_test COMMAND aether_credentials_test)
set_tests_properties(aether_credentials_test PROPERTIES TIMEOUT 30)

foreach(target aether_credentials aether_credentials_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    endif()
endforeach()
