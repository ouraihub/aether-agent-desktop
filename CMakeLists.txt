cmake_minimum_required(VERSION 3.25)

project(Aether VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_library(aether_foundation STATIC
    src/foundation/error.c
    src/foundation/id.c
)

target_include_directories(aether_foundation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(aether_foundation PUBLIC c_std_17)

if(MSVC)
    target_compile_options(aether_foundation PRIVATE /W4 /WX)
else()
    target_compile_options(aether_foundation PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_library(aether_core STATIC
    src/core/core.c
)

target_include_directories(aether_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(aether_core PUBLIC aether_foundation)
target_compile_features(aether_core PUBLIC c_std_17)

if(MSVC)
    target_compile_options(aether_core PRIVATE /W4 /WX)
else()
    target_compile_options(aether_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_executable(aether_desktop
    apps/desktop/main.c
)

target_link_libraries(aether_desktop PRIVATE aether_core)

if(MSVC)
    target_compile_options(aether_desktop PRIVATE /W4 /WX)
else()
    target_compile_options(aether_desktop PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_library(aether_backend STATIC
    src/backend/backend.c
)

target_include_directories(aether_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(aether_backend PUBLIC aether_core)
target_compile_features(aether_backend PUBLIC c_std_17)

if(MSVC)
    target_compile_options(aether_backend PRIVATE /W4 /WX)
else()
    target_compile_options(aether_backend PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_executable(aether_backend_smoke
    tests/backend/backend_smoke.c
)

target_link_libraries(aether_backend_smoke PRIVATE aether_backend)
target_compile_features(aether_backend_smoke PUBLIC c_std_17)

if(MSVC)
    target_compile_options(aether_backend_smoke PRIVATE /W4 /WX)
else()
    target_compile_options(aether_backend_smoke PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_test(NAME aether_backend_smoke COMMAND aether_backend_smoke)

add_executable(aether_core_smoke
    tests/core/core_smoke.c
)

target_link_libraries(aether_core_smoke PRIVATE aether_core)

if(MSVC)
    target_compile_options(aether_core_smoke PRIVATE /W4 /WX)
else()
    target_compile_options(aether_core_smoke PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_test(NAME aether_core_smoke COMMAND aether_core_smoke)

add_executable(aether_foundation_smoke
    tests/foundation/foundation_smoke.c
)

target_link_libraries(aether_foundation_smoke PRIVATE aether_foundation)

if(MSVC)
    target_compile_options(aether_foundation_smoke PRIVATE /W4 /WX)
else()
    target_compile_options(aether_foundation_smoke PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wformat=2
        -Wundef
        -Werror
    )
endif()

add_test(NAME aether_foundation_smoke COMMAND aether_foundation_smoke)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(aether_check_code
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_code.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
    add_test(
        NAME aether_code_quality
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_code.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
