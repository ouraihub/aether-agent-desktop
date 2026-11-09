# Upstream sources are deliberately compiled separately from project warning policy.
add_library(aether_sqlite3 STATIC sqlite/sqlite3.c)
target_include_directories(aether_sqlite3 SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite)
target_compile_definitions(aether_sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DEFAULT_MEMSTATUS=0)

add_library(aether_yyjson STATIC yyjson/yyjson.c)
target_include_directories(aether_yyjson SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/yyjson)

if(MSVC)
    target_compile_options(aether_sqlite3 PRIVATE /W0)
    target_compile_options(aether_yyjson PRIVATE /W0)
else()
    target_compile_options(aether_sqlite3 PRIVATE -w)
    target_compile_options(aether_yyjson PRIVATE -w)
endif()

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(aether_sqlite3 PRIVATE Threads::Threads)
endif()
# Extension loading and optional SQL math functions are disabled; no dl/m needed.

# curl's options are isolated in its own directory, including BUILD_TESTING.
add_subdirectory(curl)
