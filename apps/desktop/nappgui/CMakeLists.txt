cmake_minimum_required(VERSION 3.20)
project(aether_frontend_nappgui LANGUAGES C)

set(NAPPGUI_ROOT "" CACHE PATH "NAppGUI source/build root")
if(NOT NAPPGUI_ROOT)
    message(FATAL_ERROR "Set NAPPGUI_ROOT to the configured NAppGUI SDK build root")
endif()
set(NAPPGUI_SOURCE "${CMAKE_SOURCE_DIR}/build/ui-native/vendor/frang75-nappgui_src-3a918e2")
set(NAPPGUI_INCLUDE "${NAPPGUI_SOURCE}/src")
if(WIN32)
    set(NAPPGUI_LIB "${NAPPGUI_ROOT}/Release/lib")
    set(NAPPGUI_EXT "lib")
    add_compile_definitions(_WINDOWS)
else()
    set(NAPPGUI_LIB "${NAPPGUI_ROOT}/Release/lib")
    set(NAPPGUI_EXT "a")
endif()
if(NOT EXISTS "${NAPPGUI_INCLUDE}/nappgui.h")
    message(FATAL_ERROR "NAppGUI source headers not found: ${NAPPGUI_INCLUDE}")
endif()
if(NOT EXISTS "${NAPPGUI_LIB}/osapp.${NAPPGUI_EXT}")
    message(FATAL_ERROR "Build NAppGUI first; missing ${NAPPGUI_LIB}/osapp.${NAPPGUI_EXT}")
endif()

if(WIN32)
    add_compile_definitions(_WINDOWS)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
endif()

add_executable(aether_frontend_nappgui frontend.c)
target_include_directories(aether_frontend_nappgui PRIVATE
    "${NAPPGUI_INCLUDE}")
if(WIN32)
    target_link_libraries(aether_frontend_nappgui PRIVATE
        comctl32 gdiplus shlwapi ws2_32 uxtheme ole32)
    set_target_properties(aether_frontend_nappgui PROPERTIES
        OUTPUT_NAME "aether_frontend_nappgui"
        WIN32_EXECUTABLE TRUE)
    target_link_libraries(aether_frontend_nappgui PRIVATE
        "${NAPPGUI_LIB}/osapp.lib"
        "${NAPPGUI_LIB}/gui.lib"
        "${NAPPGUI_LIB}/osgui.lib"
        "${NAPPGUI_LIB}/draw2d.lib"
        "${NAPPGUI_LIB}/geom2d.lib"
        "${NAPPGUI_LIB}/core.lib"
        "${NAPPGUI_LIB}/osbs.lib"
        "${NAPPGUI_LIB}/sewer.lib")
else()
    target_link_libraries(aether_frontend_nappgui PRIVATE
        "${NAPPGUI_LIB}/libosapp.a"
        "${NAPPGUI_LIB}/libgui.a"
        "${NAPPGUI_LIB}/libosgui.a"
        "${NAPPGUI_LIB}/libdraw2d.a"
        "${NAPPGUI_LIB}/libgeom2d.a"
        "${NAPPGUI_LIB}/libcore.a"
        "${NAPPGUI_LIB}/libosbs.a"
        "${NAPPGUI_LIB}/libsewer.a"
        PkgConfig::GTK3
        m)
endif()
if(MSVC)
    target_compile_options(aether_frontend_nappgui PRIVATE /W4 /utf-8)
else()
    target_compile_options(aether_frontend_nappgui PRIVATE -Wall -Wextra -Wpedantic)
endif()
