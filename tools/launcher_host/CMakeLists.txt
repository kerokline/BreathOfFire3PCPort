# Host build of the launcher's settings code (src/launcher/config.cpp,
# suites.cpp) and its tests - a Windows host, any C++17 compiler (the
# cmake-clang-v1 clang++ on the owner's machine), Python 3.11+ for the suites'
# generated table; no game data.
#   cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++
#   cmake --build <scratch>/lh && <scratch>/lh/config_tests.exe
cmake_minimum_required(VERSION 3.20)
project(bof3x_launcher_host CXX)

set(CMAKE_CXX_STANDARD 17)
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
               ${REPO_SRC}/input/bindings.cpp "${_suites_hdr}")
target_include_directories(config_tests PRIVATE ${REPO_SRC} "${CMAKE_BINARY_DIR}/gen")
target_compile_options(config_tests PRIVATE -Wall -Wextra)
# The presets' reference rows (tools/ledger_registry.py presets --tsv), read by TestPresetsFixture.
target_compile_definitions(config_tests PRIVATE BOF3X_LAUNCHER_HOST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
