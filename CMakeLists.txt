# NOTE:  If you installed SDL2 at an unusual place and FindSDL2 fails,
#        you can define SDL2_INCLUDE_DIR on the cmdline.  For example:
#        cmake -DSDL2_INCLUDE_DIR=/opt/SDL2/include/SDL2 [other stuff]

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(triple arm-linux-gnueabihf)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

add_compile_options(-Wall -Wextra -pedantic -Werror)

cmake_minimum_required(VERSION 3.0.0)
project(gptokeyb
        VERSION 0.2.0
        LANGUAGES C)

list(APPEND CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_LIST_DIR}/cmake/modules"
)

find_package(LIBEVDEV REQUIRED)
find_package(SDL2 REQUIRED)

add_subdirectory(interpose)

include_directories(
    interpose
    ${LIBEVDEV_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    )

add_executable(gptokeyb2
    src/analog.c
    src/config.c
    src/event.c
    src/ini.c
    src/input.c
    src/keyboard.c
    src/keys.c
    src/main.c
    src/state.c
    src/util.c
    src/xbox360.c
    )

target_link_libraries(gptokeyb2
    ${SDL2_LIBRARIES}
    ${LIBEVDEV_LIBRARIES}
    m
    interpose
    )
