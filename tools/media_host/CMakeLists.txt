# Host build of src/media (the STR / XA decoders and the XA clip reader) and
# its tests - any C++17 compiler on Linux or Windows; no game data.
#   cmake -S tools/media_host -B <scratch>/mh && cmake --build <scratch>/mh
#   <scratch>/mh/media_tests
# media_dump is the oracle checks' tool (docs/psx-media-round.md M1): an STR
# file to raw frames and PCM, an XA clip to PCM.
cmake_minimum_required(VERSION 3.20)
project(bof3x_media_host CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(REPO_SRC ${REPO}/src)
add_library(media_lib STATIC
  ${REPO_SRC}/media/xa_adpcm.cpp
  ${REPO_SRC}/media/resample.cpp
  ${REPO_SRC}/media/xa_clip.cpp
  ${REPO_SRC}/media/str_movie.cpp)
target_include_directories(media_lib PUBLIC ${REPO_SRC})
target_compile_options(media_lib PRIVATE -Wall -Wextra)

add_executable(media_tests media_tests.cpp)
target_link_libraries(media_tests PRIVATE media_lib)
target_compile_options(media_tests PRIVATE -Wall -Wextra)

add_executable(media_dump media_dump.cpp)
target_link_libraries(media_dump PRIVATE media_lib)
target_compile_options(media_dump PRIVATE -Wall -Wextra)
