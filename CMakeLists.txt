cmake_minimum_required(VERSION 3.19)
project(gentree C)

set(CMAKE_C_STANDARD 11)

add_executable(gentree main.c)

set_property(TARGET gentree PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
