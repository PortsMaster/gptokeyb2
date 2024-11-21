set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(triple arm-linux-gnueabihf)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

cmake_minimum_required(VERSION 3.0.0)
project(gptokeyb
        VERSION 0.2.0
        LANGUAGES C)

list(APPEND CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_LIST_DIR}/cmake/modules"
)

add_compile_options(-Wall -Wextra -pedantic -Werror)

# NOTE: you can specify custom installation locations of the libraries
# by setting CMake variables -DSDL_INCLUDE_DIR=... or -DLIBEVDEV_INCLUDE_DIR=...
# on the command line
find_package(LIBEVDEV REQUIRED)
find_package(SDL2 REQUIRED)

add_subdirectory(interpose)

add_executable(gptokeyb2
    src/analog.c
    src/config.c
    src/event.c
    src/gptokeyb2.h
    src/ini.c
    src/input.c
    src/keyboard.c
    src/keys.c
    src/main.c
    src/state.c
    src/util.c
    src/xbox360.c
    )

target_include_directories(gptokeyb2 PRIVATE
    interpose
    ${LIBEVDEV_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(gptokeyb2 PRIVATE
    interpose
    ${LIBEVDEV_LIBRARIES}
    ${SDL2_LIBRARIES}
    m
)