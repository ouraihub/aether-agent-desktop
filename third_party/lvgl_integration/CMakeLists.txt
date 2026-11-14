# Project-owned LVGL 9.6.0 integration. The release tree is unchanged.
set(LV_BUILD_CONF_PATH
    "${CMAKE_CURRENT_LIST_DIR}/../lvgl_config/lv_conf.h"
    CACHE PATH "LVGL configuration for the Aether frontend" FORCE)
set(LV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_USE_FIND_PACKAGE OFF CACHE BOOL "" FORCE)
set(LV_USE_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(LV_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
set(LV_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_LVGL_H_SIMPLE_INCLUDE ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lvgl
    ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

if(MSVC)
    target_compile_options(lvgl PRIVATE /W0 /utf-8)
else()
    target_compile_options(lvgl PRIVATE -w)
endif()

add_library(aether_lvgl ALIAS lvgl)
