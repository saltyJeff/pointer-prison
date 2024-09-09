cmake_minimum_required(VERSION 3.5)

project(PointerPrison
    VERSION 1.0
    LANGUAGES C
)

include (${CMAKE_ROOT}/Modules/FindX11.cmake)
message("X11_FOUND: ${X11_FOUND}")

add_library(pp SHARED pp.c xlib_funcs.inc)
include_directories(${X11_INCLUDE_DIR})

add_custom_target(TestXCalc 
${CMAKE_COMMAND} -E env "LD_PRELOAD=$<TARGET_FILE:pp>" xcalc
DEPENDS pp)

add_custom_target(TestGlxGears 
${CMAKE_COMMAND} -E env "LD_PRELOAD=$<TARGET_FILE:pp>" glxgears
DEPENDS pp)

add_custom_target(TestFlappy 
${CMAKE_COMMAND} -E env "LD_PRELOAD=$<TARGET_FILE:pp>" /home/jeffe/Programming/flappy/flappy
DEPENDS pp)