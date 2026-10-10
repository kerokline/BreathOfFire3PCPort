# Host build of the launcher's settings code (src/launcher/config.cpp) and its
# tests - a Windows host, any C++17 compiler (the cmake-clang-v1 clang++ on the
# owner's machine); no game data.
#   cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++
#   cmake --build <scratch>/lh && <scratch>/lh/config_tests.exe
cmake_minimum_required(VERSION 3.20)
project(bof3x_launcher_host CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_executable(config_tests config_tests.cpp ${REPO_SRC}/launcher/config.cpp ${REPO_SRC}/input/bindings.cpp)
target_include_directories(config_tests PRIVATE ${REPO_SRC})
target_compile_options(config_tests PRIVATE -Wall -Wextra)
