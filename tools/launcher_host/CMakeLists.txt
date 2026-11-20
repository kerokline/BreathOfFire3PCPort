# Host build of the launcher's settings code (src/launcher/config.cpp,
# suites.cpp) and its tests - a Windows host, any C++17 compiler (the
# cmake-clang-v1 clang++ on the owner's machine), Python 3.11+ for the suites'
# generated table; no game data.
#   cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++
#   cmake --build <scratch>/lh && <scratch>/lh/config_tests.exe
cmake_minimum_required(VERSION 3.20)
project(bof3x_launcher_host CXX)

# C++20 and the OS seam's library, as the main build: src/common's files call bof3::os (src/os/os.h,
# whose text.h needs std::u8string) since the Linux round's LB1; with C++17 and no library the host
# build stopped at home.cpp (found 2026-11-12)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(REPO_SRC ${REPO}/src)
find_program(BOF3X_PYTHON NAMES python python3 REQUIRED)
set(_suites_hdr "${CMAKE_BINARY_DIR}/gen/launcher/suites.gen.h")
add_custom_command(
  OUTPUT "${_suites_hdr}"
  COMMAND "${BOF3X_PYTHON}" "${REPO}/tools/suites.py" gen --out "${_suites_hdr}"
  DEPENDS "${REPO}/recipes/suites.toml" "${REPO}/recipes/opt.toml" "${REPO}/recipes/psp-layers.toml" "${REPO}/fixtures.toml" "${REPO}/tools/suites.py"
  VERBATIM)
add_executable(config_tests config_tests.cpp ${REPO_SRC}/launcher/config.cpp ${REPO_SRC}/launcher/suites.cpp
               ${REPO_SRC}/common/cache_manifest.cpp ${REPO_SRC}/common/home.cpp
               ${REPO_SRC}/input/bindings.cpp ${REPO_SRC}/input/function_keys.cpp "${_suites_hdr}")
add_subdirectory(${REPO_SRC}/os "${CMAKE_BINARY_DIR}/os")
target_include_directories(config_tests PRIVATE ${REPO_SRC} "${CMAKE_BINARY_DIR}/gen")
target_link_libraries(config_tests PRIVATE bof3x_os)
target_compile_options(config_tests PRIVATE -Wall -Wextra)
# The presets' reference rows (tools/ledger_registry.py presets --tsv), read by TestPresetsFixture.
target_compile_definitions(config_tests PRIVATE BOF3X_LAUNCHER_HOST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# The save editor's format core (src/launcher/save/; docs/save-editor.md):
#   <scratch>/lh/save_tests.exe <a folder of your own save files> ...
# With the image's readers (src/launcher/exe_image.cpp; docs/exe-retire-round.md X4), which see
# symbols.toml's addresses: bof3/symbols.gen.h generated here as the main build makes it.
#   BOF3X_ORACLE_EXE=<BOF3.exe> BOF3X_ORACLE_CACHE=<a cache> <scratch>/lh/save_tests.exe   the oracle, counts only
set(_symbols_hdr "${CMAKE_BINARY_DIR}/gen/bof3/symbols.gen.h")
add_custom_command(
  OUTPUT "${_symbols_hdr}" "${CMAKE_BINARY_DIR}/gen/bof3/bind.gen.h"
  COMMAND "${BOF3X_PYTHON}" "${REPO}/tools/gen_symbols.py" --toml "${REPO}/symbols.toml" --out "${_symbols_hdr}"
          --bind-out "${CMAKE_BINARY_DIR}/gen/bof3/bind.gen.h"
  DEPENDS "${REPO}/symbols.toml" "${REPO}/tools/gen_symbols.py" "${REPO}/tools/record_sizes.py"
          "${REPO_SRC}/game/records.h" "${REPO_SRC}/game/records_effect.h" "${REPO_SRC}/game/records_gpu.h"
  VERBATIM)
add_executable(save_tests save_tests.cpp ${REPO_SRC}/launcher/save/block.cpp ${REPO_SRC}/launcher/save/card.cpp
               ${REPO_SRC}/launcher/save/finds.cpp ${REPO_SRC}/launcher/save/namemap.cpp
               ${REPO_SRC}/launcher/exe_image.cpp ${REPO_SRC}/common/sha256.cpp
               ${REPO_SRC}/input/bindings.cpp ${REPO_SRC}/input/function_keys.cpp "${_symbols_hdr}")
target_include_directories(save_tests PRIVATE ${REPO_SRC} "${CMAKE_BINARY_DIR}/gen")
target_link_libraries(save_tests PRIVATE bof3x_os)
target_compile_options(save_tests PRIVATE -Wall -Wextra)
# symbols.gen.h declares Capcom's functions __stdcall, which a 64-bit or Linux target ignores with a
# warning per declaration; exe_image.cpp takes only its addresses.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  target_compile_options(save_tests PRIVATE -Wno-ignored-attributes)
endif()
