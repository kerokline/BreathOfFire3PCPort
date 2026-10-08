# Host-side (system compiler) build of the SPU model and its unit tests.
# cmake -S tools/bgm/host -B /tmp/spu_build && cmake --build /tmp/spu_build && /tmp/spu_build/spu_tests
cmake_minimum_required(VERSION 3.20)
project(bof3_bgm_host CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(REPO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)

add_library(psx_spu STATIC ${REPO_SRC}/audio/spu.cpp)
target_include_directories(psx_spu PUBLIC ${REPO_SRC})
target_compile_options(psx_spu PRIVATE -Wall -Wextra)

add_executable(spu_tests spu_tests.cpp)
target_link_libraries(spu_tests PRIVATE psx_spu)
target_compile_options(spu_tests PRIVATE -Wall -Wextra)
