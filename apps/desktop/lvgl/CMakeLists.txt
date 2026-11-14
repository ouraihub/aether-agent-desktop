if(NOT WIN32)
    message(STATUS "Aether LVGL Frontend preview is Windows-only")
    return()
endif()

add_executable(aether_frontend_lvgl frontend.c)
target_link_libraries(aether_frontend_lvgl PRIVATE aether_lvgl)
target_compile_features(aether_frontend_lvgl PRIVATE c_std_17)
target_compile_options(aether_frontend_lvgl PRIVATE /W4 /WX /utf-8)
set_target_properties(aether_frontend_lvgl PROPERTIES
    OUTPUT_NAME "aether_frontend_lvgl")
