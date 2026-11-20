# The Win32 Jukebox's samples without its window or waveOut (docs/launcher-portable.md,
# stage P5): old_pcm.cpp compiles src/launcher/music_player.cpp as it is - the same
# translation unit as the Win32 launcher's - and writes what its Stream classes render,
# in the blocks its Output handed waveOut, to a file. tools/jukebox_host/compare.py holds
# that against the drawn launcher's --samples file of the same song. Windows only (the
# Win32 player is); no game data in the tree: the songs are read from an install.
# Built with the player's toolchain and flags, so the decoders compile as the launcher's:
#   cmake -S tools/jukebox_host -B <scratch>/jh -G Ninja
#         -DCMAKE_TOOLCHAIN_FILE=<repo>/cmake/i686-llvm-mingw.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build <scratch>/jh
#   <scratch>/jh/old_pcm.exe seq <song .DAT> <its bank\ folder> <frames> <out file>
#   <scratch>/jh/old_pcm.exe mp3 <file> <song number> <loops 0|1> <frames> <out file>
cmake_minimum_required(VERSION 3.20)
project(bof3x_jukebox_host C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT WIN32)
  message(FATAL_ERROR "the Win32 Jukebox's samples need a Windows target: music_player.cpp is the Win32 window's")
endif()

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(REPO_SRC ${REPO}/src)
# The main build's options (CMakeLists.txt), so the decoders compile alike.
add_compile_options(-Wall -Wextra -Werror=return-type -falign-functions=16 -fno-strict-aliasing -fno-auto-import)
add_link_options(-static)   # as every binary of the main build: no runtime DLL found on PATH
find_program(BOF3X_PYTHON NAMES python python3 REQUIRED)
set(_suites_hdr "${CMAKE_BINARY_DIR}/gen/launcher/suites.gen.h")
add_custom_command(
  OUTPUT "${_suites_hdr}"
  COMMAND "${BOF3X_PYTHON}" "${REPO}/tools/suites.py" gen --out "${_suites_hdr}"
  DEPENDS "${REPO}/recipes/suites.toml" "${REPO}/recipes/opt.toml" "${REPO}/recipes/psp-layers.toml" "${REPO}/fixtures.toml" "${REPO}/tools/suites.py"
  VERBATIM)
add_subdirectory(${REPO_SRC}/os "${CMAKE_BINARY_DIR}/os")
add_executable(old_pcm old_pcm.cpp ${REPO_SRC}/launcher/config.cpp ${REPO_SRC}/launcher/suites.cpp
               ${REPO_SRC}/common/cache_manifest.cpp ${REPO_SRC}/common/home.cpp
               ${REPO_SRC}/input/bindings.cpp ${REPO_SRC}/input/function_keys.cpp
               ${REPO_SRC}/audio/spu.cpp ${REPO_SRC}/audio/seq.cpp ${REPO_SRC}/audio/song.cpp
               ${REPO_SRC}/media/xa_clip.cpp ${REPO_SRC}/media/xa_adpcm.cpp ${REPO_SRC}/media/resample.cpp
               "${_suites_hdr}")
target_include_directories(old_pcm PRIVATE ${REPO_SRC} ${REPO}/third_party "${CMAKE_BINARY_DIR}/gen")
target_link_libraries(old_pcm PRIVATE bof3x_os winmm)
target_link_options(old_pcm PRIVATE -municode)
