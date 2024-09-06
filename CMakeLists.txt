cmake_minimum_required(VERSION 3.5)

project(PointerPrison
    VERSION 1.0
)

include (${CMAKE_ROOT}/Modules/FindX11.cmake)
message("X11_FOUND: ${X11_FOUND}")

add_library(pp SHARED pp.cpp)
include_directories(${X11_INCLUDE_DIR})
target_link_libraries(pp ${X11_LIBRARIES})

add_custom_target(TestXCalc 
${CMAKE_COMMAND} -E env "LD_PRELOAD=$<TARGET_FILE:pp>" xcalc
DEPENDS pp)