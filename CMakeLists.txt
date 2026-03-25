cmake_minimum_required(VERSION 3.26)
project(infra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE
    $<$<CONFIG:Debug>:-Wall>
    $<$<CONFIG:Debug>:-Wextra>
    $<$<CONFIG:Debug>:-Wpedantic>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

target_link_options(project_options INTERFACE
    $<$<CONFIG:Debug>: -fsanitize=address,undefined>
)