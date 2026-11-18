# Host build of the keyboard's SDL scancode table (src/input/key_sdl.cpp) and
# its test against SDL's own Windows table: the system compiler, no game
# data, no SDL library - SDL's headers and its src/events table only.
#   cmake -S tools/key_host -B <scratch>/key-host -DSDL_SRC=<build>/_deps/sdl3-src
#   cmake --build <scratch>/key-host && <scratch>/key-host/key_tests
# In the WSL (no cmake there):
#   g++ -std=c++20 -Wall -Wextra -Isrc -Itools/key_host/shim -I$SDL/include -I$SDL/src
#       tools/key_host/key_tests.cpp src/input/key_sdl.cpp src/input/bindings.cpp -o key_tests
cmake_minimum_required(VERSION 3.20)
project(bof3x_key_host CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(SDL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../build/_deps/sdl3-src CACHE PATH "SDL3's source tree (the build's FetchContent copy)")
if(NOT EXISTS ${SDL_SRC}/src/events/scancodes_windows.h)
  message(FATAL_ERROR "SDL_SRC=${SDL_SRC} holds no src/events/scancodes_windows.h: configure the game's build first")
endif()
add_executable(key_tests key_tests.cpp ${REPO_SRC}/input/key_sdl.cpp ${REPO_SRC}/input/bindings.cpp)
# The shim first, so that SDL's table finds our SDL_internal.h, not SDL's.
target_include_directories(key_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shim ${REPO_SRC} ${SDL_SRC}/include ${SDL_SRC}/src)
target_compile_options(key_tests PRIVATE -Wall -Wextra)
