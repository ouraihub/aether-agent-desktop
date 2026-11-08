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
    src/foundation/string_view.c
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
    src/core/runtime/registry.c
    src/core/runtime/runtime.c
    src/core/runtime/reference.c
)

target_include_directories(aether_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(aether_core PUBLIC aether_foundation)
target_compile_features(aether_core PUBLIC c_std_17)
target_include_directories(aether_core PRIVATE src/core/internal)

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

add_executable(aether_runtime_registry_test tests/core/runtime_registry_test.c)
target_link_libraries(aether_runtime_registry_test PRIVATE aether_core)
if(MSVC)
    target_compile_options(aether_runtime_registry_test PRIVATE /W4 /WX)
else()
    target_compile_options(aether_runtime_registry_test PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
endif()
add_test(NAME aether_runtime_registry_test COMMAND aether_runtime_registry_test)

add_executable(aether_runtime_registry_allocation_test
    tests/core/runtime_registry_allocation_test.c
    src/core/runtime/registry.c
    src/core/runtime/runtime.c
    src/core/runtime/reference.c)
target_include_directories(aether_runtime_registry_allocation_test PRIVATE
    include src/core/internal)
target_compile_definitions(aether_runtime_registry_allocation_test PRIVATE
    malloc=aether_test_malloc calloc=aether_test_calloc free=aether_test_free)
target_link_libraries(aether_runtime_registry_allocation_test PRIVATE
    aether_foundation)
if(MSVC)
    target_compile_options(aether_runtime_registry_allocation_test PRIVATE
        /W4 /WX)
else()
    target_compile_options(aether_runtime_registry_allocation_test PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
endif()
add_test(NAME aether_runtime_registry_allocation_test
    COMMAND aether_runtime_registry_allocation_test)

add_library(aether_platform STATIC src/platform/common/path.c)
target_include_directories(aether_platform PRIVATE src/platform/internal)
if(WIN32)
    target_sources(aether_platform PRIVATE
        src/platform/windows/clock.c
        src/platform/windows/path.c
    )
    target_link_libraries(aether_platform PRIVATE shell32 ole32 uuid)
else()
    target_sources(aether_platform PRIVATE
        src/platform/posix/clock.c
        src/platform/posix/path.c
    )
endif()

if(WIN32)
    add_executable(aether_platform_windows_path_test
        tests/platform/windows_path_test.c
    )
    target_link_libraries(aether_platform_windows_path_test PRIVATE
        aether_platform shell32 ole32 uuid
    )
    if(MSVC)
        target_compile_options(aether_platform_windows_path_test PRIVATE /W4 /WX)
    else()
        target_compile_options(aether_platform_windows_path_test PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror
        )
    endif()
    add_test(NAME aether_platform_windows_path_test
        COMMAND aether_platform_windows_path_test)
endif()

if(NOT WIN32)
    add_executable(aether_platform_posix_path_test
        tests/platform/posix_path_test.c)
    target_link_libraries(aether_platform_posix_path_test PRIVATE aether_platform)
    target_compile_options(aether_platform_posix_path_test PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
    add_test(NAME aether_platform_posix_path_test
        COMMAND aether_platform_posix_path_test)
endif()

add_executable(aether_platform_allocation_test
    tests/platform/allocation_test.c
    src/platform/common/path.c
)
target_include_directories(aether_platform_allocation_test PRIVATE
    include src/platform/internal
)
target_compile_definitions(aether_platform_allocation_test PRIVATE
    malloc=aether_test_malloc free=aether_test_free
)
target_link_libraries(aether_platform_allocation_test PRIVATE aether_foundation)
if(WIN32)
    target_sources(aether_platform_allocation_test PRIVATE
        src/platform/windows/path.c)
    target_link_libraries(aether_platform_allocation_test PRIVATE
        shell32 ole32 uuid)
else()
    target_sources(aether_platform_allocation_test PRIVATE
        src/platform/posix/path.c)
endif()
if(MSVC)
    target_compile_options(aether_platform_allocation_test PRIVATE /W4 /WX)
else()
    target_compile_options(aether_platform_allocation_test PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror)
endif()
add_test(NAME aether_platform_allocation_test
    COMMAND aether_platform_allocation_test)

target_include_directories(aether_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(aether_platform PUBLIC aether_foundation)
target_compile_features(aether_platform PUBLIC c_std_17)

if(MSVC)
    target_compile_options(aether_platform PRIVATE /W4 /WX)
else()
    target_compile_options(aether_platform PRIVATE
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

add_executable(aether_platform_smoke
    tests/platform/platform_smoke.c
)

target_link_libraries(aether_platform_smoke PRIVATE aether_platform)

if(MSVC)
    target_compile_options(aether_platform_smoke PRIVATE /W4 /WX)
else()
    target_compile_options(aether_platform_smoke PRIVATE
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

add_test(NAME aether_platform_smoke COMMAND aether_platform_smoke)

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
    src/backend/requests/request.c
)

target_include_directories(aether_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/internal
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

add_executable(aether_backend_request_test tests/backend/request_test.c)
target_link_libraries(aether_backend_request_test PRIVATE aether_backend)
if(MSVC)
    target_compile_options(aether_backend_request_test PRIVATE /W4 /WX)
else()
    target_compile_options(aether_backend_request_test PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror
    )
endif()
add_test(NAME aether_backend_request_test COMMAND aether_backend_request_test)

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
