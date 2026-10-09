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

# The sequencer (libsnd's semantics, docs/libsnd-reading.md) over the SPU
# model, and the renderer that plays a song of the importer's cache to a WAV:
#   synth_render --cache <scratch>/cache --song 0 --out song000.wav --seconds 120
add_library(psx_music STATIC ${REPO_SRC}/audio/seq.cpp ${REPO_SRC}/audio/song.cpp)
target_include_directories(psx_music PUBLIC ${REPO_SRC})
target_link_libraries(psx_music PUBLIC psx_spu)
target_compile_options(psx_music PRIVATE -Wall -Wextra)

add_executable(synth_render synth_render.cpp)
target_link_libraries(synth_render PRIVATE psx_music)
target_compile_options(synth_render PRIVATE -Wall -Wextra)

add_executable(seq_tests seq_tests.cpp)
target_link_libraries(seq_tests PRIVATE psx_music)
target_compile_options(seq_tests PRIVATE -Wall -Wextra)
