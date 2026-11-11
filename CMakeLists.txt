cmake_minimum_required(VERSION 3.25)
project(bof3x VERSION 0.1.0 LANGUAGES CXX RC)

# The version every binary carries (src/version.rc, tools/make_exe.py):
# project(VERSION) above is the number; the tag is the release's name, which
# the release workflow passes (-DBOF3X_VERSION_TAG=v0.1.0-alpha.2, or
# latest-<sha> for a push to main) and a local build leaves at v<number>.
set(BOF3X_VERSION_TAG "v${PROJECT_VERSION}" CACHE STRING "The release tag stamped into the binaries' ProductVersion")

# Phase 0 scaffolding (docs/SCAFFOLDING.md): a launcher that starts the player's
# own BOF3.exe suspended and loads our DLL into it, and the DLL, which redirects
# individual original functions to our reimplementations.
#
# No game data is read at build time and none is produced.

# Two widths (docs/x64-build-round.md 3 "XA4"). BOF3X_WIDTH is 32 or 64:
#   32  --preset i686: everything - the DLL in both its modes (hosted inside
#       BOF3.exe, an i386 image, and the exe host), the shadow fuzz, both
#       launchers, bof3x.exe as a PE32. The player's build and the oracle
#       (round doc section 2).
#   64  --preset x86_64: the exe host alone - bof3x.dll without the fuzz
#       files, bof3x.exe as a PE32+, the player's launcher. Nothing that
#       loads into BOF3.exe. BOF3X_HOSTED_BUILD is the switch below.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(BOF3X_WIDTH 32)
  set(BOF3X_HOSTED_BUILD ON)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BOF3X_WIDTH 64)
  set(BOF3X_HOSTED_BUILD OFF)
else()
  message(FATAL_ERROR
    "bof3x is built 32-bit (--preset i686, cmake/i686-llvm-mingw.cmake: the hosted build inside "
    "BOF3.exe, an i386 image) or 64-bit (--preset x86_64, cmake/x86_64-llvm-mingw.cmake: the exe "
    "host alone); CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P}.")
endif()
message(STATUS "bof3x: ${BOF3X_WIDTH}-bit build, hosted mode and fuzz ${BOF3X_HOSTED_BUILD}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -falign-functions: the OFF direction of a detour writes five bytes over the
# start of OUR function, so none of ours may be shorter than that before the
# next one begins.
# -fno-strict-aliasing: the game's memory is read and written through whatever
# type the 2001 source liked, and a reimplementation has to alias the same way.
# Without it the optimiser may reorder a short* read past a long* write to the
# same bytes - which hid a deliberately wrong Gte_ApplyMatrix from its own
# differential fuzz (docs/psx-library-layer.md section 2).
# -fno-auto-import: every binary here links statically and takes no data from a DLL, so a global
# another file defines is read directly, not through a `.refptr.<name>` stub the compiler lays in
# case the linker must auto-import it. With the stubs a function reads a global of its own file
# and one of another file by different code, so moving a function between files changed it
# (docs/source-layout-round.md, the `refptr` class, 1,400 gate exceptions; IDEAS I60). The linker
# is told the same (--disable-auto-import), so a data import that is ever needed fails the link
# instead of being patched at load, where on x86-64 a 32-bit reach may not hold.
add_compile_options(-Wall -Wextra -Werror=return-type -falign-functions=16 -fno-strict-aliasing -fno-auto-import)
add_link_options(-Wl,--disable-auto-import)
# The DLL is injected by path into a process whose DLL search order we do not
# control, so it may not depend on libc++.dll / libunwind.dll sitting nearby.
add_link_options(-static)

# --- symbols.toml -> bof3/symbols.gen.h -------------------------------------
# Anaconda's `python` on this machine; any 3.11+ works (tomllib).
find_program(BOF3X_PYTHON NAMES python python3 REQUIRED)
set(_gen_dir "${CMAKE_BINARY_DIR}/gen")
configure_file(src/version.h.in "${_gen_dir}/version.gen.h" @ONLY)
set(_gen_hdr "${_gen_dir}/bof3/symbols.gen.h")
# bind.gen.h beside it: the bind table src/hook/bind.cpp alone includes
# (docs/relocatable-image-design.md 4.1).
set(_bind_hdr "${_gen_dir}/bof3/bind.gen.h")
add_custom_command(
  OUTPUT "${_gen_hdr}" "${_bind_hdr}"
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/gen_symbols.py"
          --toml "${CMAKE_SOURCE_DIR}/symbols.toml" --out "${_gen_hdr}" --bind-out "${_bind_hdr}"
  DEPENDS "${CMAKE_SOURCE_DIR}/symbols.toml"
          "${CMAKE_SOURCE_DIR}/tools/gen_symbols.py"
          "${CMAKE_SOURCE_DIR}/tools/record_sizes.py"
          "${CMAKE_SOURCE_DIR}/src/game/records.h"
          "${CMAKE_SOURCE_DIR}/src/game/records_effect.h"
          "${CMAKE_SOURCE_DIR}/src/game/records_gpu.h"
  COMMENT "Generating bof3/symbols.gen.h from symbols.toml"
  VERBATIM)
add_custom_target(bof3x_symbols DEPENDS "${_gen_hdr}" "${_bind_hdr}")

# --- recipes/exe-pointers.tsv -> bof3/rebase.gen.h ----------------------------
# The pointer words of .data the exe host moves with the image when bof3x.exe
# lies at another base (docs/relocatable-image-design.md 3): addresses and
# counts only, as symbols.gen.h.
set(_rebase_hdr "${_gen_dir}/bof3/rebase.gen.h")
add_custom_command(
  OUTPUT "${_rebase_hdr}"
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/gen_rebase.py"
          --pointers "${CMAKE_SOURCE_DIR}/recipes/exe-pointers.tsv" --out "${_rebase_hdr}"
  DEPENDS "${CMAKE_SOURCE_DIR}/recipes/exe-pointers.tsv"
          "${CMAKE_SOURCE_DIR}/tools/gen_rebase.py"
  COMMENT "Generating bof3/rebase.gen.h from recipes/exe-pointers.tsv"
  VERBATIM)
add_custom_target(bof3x_rebase DEPENDS "${_rebase_hdr}")

# --- recipes/suites.toml -> launcher/suites.gen.h ----------------------------
# The launcher's three suites (docs/launcher-suites.md): the catalogue, checked
# against recipes/opt.toml and fixtures.toml, as C++ constants.
set(_suites_hdr "${_gen_dir}/launcher/suites.gen.h")
add_custom_command(
  OUTPUT "${_suites_hdr}"
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/suites.py" gen --out "${_suites_hdr}"
  DEPENDS "${CMAKE_SOURCE_DIR}/recipes/suites.toml"
          "${CMAKE_SOURCE_DIR}/recipes/opt.toml"
          "${CMAKE_SOURCE_DIR}/recipes/psp-layers.toml"
          "${CMAKE_SOURCE_DIR}/fixtures.toml"
          "${CMAKE_SOURCE_DIR}/tools/suites.py"
  COMMENT "Generating launcher/suites.gen.h from recipes/suites.toml"
  VERBATIM)
add_custom_target(bof3x_suites DEPENDS "${_suites_hdr}")

# --- SDL3, for the pad (DIV-0050, docs/controls.md) and the sound output
# (DIV-0201, src/audio/sound_sdl.cpp) ------------------------------------------
# Fetched at configure time and built static into the DLL: the joystick and
# gamepad layers and the audio subsystem (WASAPI on Windows), no video or
# render. zlib licence (docs/THIRD_PARTY.md section 2). Pinned to a release
# tag; the first configure needs the network.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO ON CACHE BOOL "" FORCE)
foreach(_sub VIDEO RENDER GPU CAMERA HAPTIC SENSOR POWER DIALOG OPENGL OPENGLES VULKAN DIRECTX)
  set(SDL_${_sub} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.16
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

# --- the injected DLL --------------------------------------------------------
# One source list for both widths. The 64-bit DLL serves the exe host alone,
# so it drops every fuzz file (any *_fuzz.cpp: the shadow fuzz runs Capcom's
# function beside ours inside BOF3.exe, which no 64-bit process can hold) -
# the hosted mode's own files (inject_all, detour, exe_image, code_bytes)
# stay, since the exe host shares them (the bind table patches through
# detour.cpp, Engine_Main runs InjectAll) and the host is told apart at run
# time (host.cpp DetectHost); what of them a 64-bit process cannot do is
# stage B4's #if. The i686 list is the whole list, in this order.
# src/game's part of the list, generated from the tree (tools/game_sources.py):
include(cmake/game_sources.cmake)
# The operating-system seam (src/os/os.h): one header per service, one backend
# folder per system. Nothing outside it names a system.
add_subdirectory(src/os)
set(_dll_sources
  src/hook/dllmain.cpp
  src/hook/host.cpp
  src/hook/exe_host.cpp
  src/hook/exe_icon.cpp
  src/hook/code_bytes.cpp
  src/common/sha256.cpp
  src/common/file_token.cpp
  src/common/cache_manifest.cpp
  src/hook/bind.cpp
  src/hook/detour.cpp
  src/hook/exe_image.cpp
  src/hook/exe_own.cpp
  src/hook/calltrace.cpp
  src/hook/gapwatch.cpp
  src/hook/input_script.cpp
  src/hook/statehash.cpp
  src/hook/wav_dump.cpp
  src/hook/stackwatch.cpp
  src/hook/stallwatch.cpp
  src/hook/snapshot.cpp
  src/hook/draw_order.cpp
  src/hook/write_watch.cpp
  src/hook/stp_census.cpp
  src/hook/crash.cpp
  src/hook/log.cpp
  src/hook/inject_all.cpp
  ${BOF3X_GAME_SOURCES}
  src/hook/bof3x.rc)
if(NOT BOF3X_HOSTED_BUILD)
  list(FILTER _dll_sources EXCLUDE REGEX "_fuzz\.cpp$")
endif()
add_library(bof3x SHARED ${_dll_sources})
add_dependencies(bof3x bof3x_symbols bof3x_rebase)
target_include_directories(bof3x PRIVATE src third_party "${_gen_dir}")
set_target_properties(bof3x PROPERTIES PREFIX "" OUTPUT_NAME "bof3x")
# The Direct3D 11 render backend (docs/render-backend.md): the OS's own
# libraries, nothing vendored; the shader compiler is d3dcompiler_47.dll.
# ole32 and windowscodecs: DIV-0095's icon - WIC encodes its 256 px PNG and
# the self-test decodes the .ico files (src/hook/exe_icon.cpp).
# dinput and dxguid: the SDK's c_dfDIKeyboard and its GUID_Key, static data
# (src/game/rdata_consts.cpp); no import of dinput.dll, which pad_read.cpp
# loads itself.
target_link_libraries(bof3x PRIVATE d3d11 dxgi d3dcompiler_47 uuid winmm ole32 windowscodecs dinput dxguid SDL3::SDL3-static bof3x_os)
# A fixed load address (the owner, 2026-10-19; docs/quicksave.md 4a): a
# quicksave's task stacks hold return addresses into this DLL and the span
# holds pointers into it, so a quick-load is good only where the DLL sat at
# the take. Without DYNAMIC_BASE the loader takes the preferred base,
# 0x10000000 (clear of Capcom's range 0x400000..0x987000 and the draw pool's
# candidates below 16 MB), whenever it is free. The relocation section is
# KEPT: when the range is taken the loader still relocates rather than
# failing to load (the hosted build inside BOF3.exe), and the quicksave's
# header check refuses the mismatch.
target_link_options(bof3x PRIVATE -Wl,--disable-dynamicbase -Wl,--image-base,0x10000000)

# --- bof3x.exe, our own executable (docs/state3-design.md section 1.3) ------
# Written by tools/make_exe.py, not linked: lld will not place a section
# before .text (docs/state3-build.md section 1.2). Based at 0x400000 and
# spanning Capcom's range, it imports bof3x.dll!Engine_Main. A PE32 in the
# 32-bit build, a PE32+ in the 64-bit one (--width; the same sections at the
# same RVAs, the entry in the width's machine code). The script reads
# the layout from src/hook/exe_layout.h - the table the engine reads too -
# and verifies what it wrote (section table, base, no relocations, stack,
# imports, resources, and the DLL's Engine_Main export) with tools/pe_read.py;
# a mismatch deletes the output and fails the build.
# The target depends on a stamp, not on bof3x.exe itself: CMake reads a
# DEPENDS path named <target>.exe as the target `bof3x` (the DLL) and drops
# the file rule (measured 2026-10-11: the command never ran).
set(_exe_out "${CMAKE_BINARY_DIR}/bof3x.exe")
set(_exe_stamp "${CMAKE_BINARY_DIR}/bof3x_exe.verified")
add_custom_command(
  OUTPUT "${_exe_out}" "${_exe_stamp}"
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/make_exe.py"
          --layout "${CMAKE_SOURCE_DIR}/src/hook/exe_layout.h"
          --dll "$<TARGET_FILE:bof3x>"
          --version "${PROJECT_VERSION}" --version-tag "${BOF3X_VERSION_TAG}"
          --width ${BOF3X_WIDTH}
          --out "${_exe_out}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${_exe_stamp}"
  DEPENDS bof3x
          "${CMAKE_SOURCE_DIR}/tools/make_exe.py"
          "${CMAKE_SOURCE_DIR}/tools/pe_read.py"
          "${CMAKE_SOURCE_DIR}/src/hook/exe_layout.h"
          "${CMAKE_SOURCE_DIR}/data/icon/bof3x.ico"
          "${_gen_dir}/version.gen.h"
  COMMENT "Writing and verifying bof3x.exe (tools/make_exe.py)"
  VERBATIM)
add_custom_target(bof3x_exe ALL DEPENDS "${_exe_stamp}")

# The same executable at base 0xA00000, for the relocatable image's proof
# (docs/relocatable-image-design.md 1.5): its span 0xA00000..0xF8F000 lies in
# the 16 MB window and off Capcom's range 0x400000..0x98F000, which the engine
# then fences, so an address left absolute faults at its own VA. Started only
# by the launcher's developer switch BOF3X_IMAGEBASE=0xA00000; never shipped.
set(_exe_moved_out "${CMAKE_BINARY_DIR}/bof3x-A00000.exe")
set(_exe_moved_stamp "${CMAKE_BINARY_DIR}/bof3x_exe_A00000.verified")
add_custom_command(
  OUTPUT "${_exe_moved_out}" "${_exe_moved_stamp}"
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/make_exe.py"
          --layout "${CMAKE_SOURCE_DIR}/src/hook/exe_layout.h"
          --dll "$<TARGET_FILE:bof3x>"
          --version "${PROJECT_VERSION}" --version-tag "${BOF3X_VERSION_TAG}"
          --width ${BOF3X_WIDTH} --image-base 0xA00000
          --out "${_exe_moved_out}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${_exe_moved_stamp}"
  DEPENDS bof3x
          "${CMAKE_SOURCE_DIR}/tools/make_exe.py"
          "${CMAKE_SOURCE_DIR}/tools/pe_read.py"
          "${CMAKE_SOURCE_DIR}/src/hook/exe_layout.h"
          "${CMAKE_SOURCE_DIR}/data/icon/bof3x.ico"
          "${_gen_dir}/version.gen.h"
  COMMENT "Writing and verifying bof3x-A00000.exe (tools/make_exe.py --image-base)"
  VERBATIM)
add_custom_target(bof3x_exe_moved ALL DEPENDS "${_exe_moved_stamp}")

# --- the launcher ------------------------------------------------------------
# The settings dialog is a Win32 DIALOGEX resource driven by DialogBoxParam:
# no toolkit, nothing vendored, so the launcher stays one dependency-free exe
# (docs/launcher-settings.md section 2). RC is enabled in project() above:
# the DLL carries a version resource too (src/hook/bof3x.rc).
# Two launchers from one set of sources (docs/launcher-settings.md section 2):
# bof3x-launcher.exe, the player's, holds no code that writes into another
# process; bof3x-launcher-dev.exe (BOF3X_HOSTED=1) also starts the hosted build
# (--host bof3: BOF3.exe with bof3x.dll injected) for the harnesses, and the
# player's hands such a run to it. The release ships only the player's.
add_library(bof3x_launcher_parts OBJECT
  src/launcher/config.cpp
  src/launcher/config_dialog.cpp
  src/launcher/exe_image.cpp
  src/launcher/music_player.cpp
  src/launcher/save_dialog.cpp
  src/launcher/save/block.cpp
  src/launcher/save/card.cpp
  src/launcher/save/namemap.cpp
  src/launcher/save/names.cpp
  src/launcher/sources_dialog.cpp
  src/launcher/suites.cpp
  src/audio/spu.cpp
  src/audio/seq.cpp
  src/audio/song.cpp
  src/common/sha256.cpp
  src/common/cache_manifest.cpp
  src/common/home.cpp
  src/input/bindings.cpp
  src/input/pad_sdl.cpp)
# symbols.gen.h for exe_image.cpp alone: Key_TableDefault is read out of the
# player's BOF3.exe, not copied (docs/exe-table-audit.md).
target_include_directories(bof3x_launcher_parts PRIVATE src third_party "${_gen_dir}")
target_link_libraries(bof3x_launcher_parts PRIVATE SDL3::SDL3-static bof3x_os)
add_dependencies(bof3x_launcher_parts bof3x_symbols bof3x_suites)
function(bof3x_add_launcher target output hosted)
  add_executable(${target} src/launcher/launcher.cpp src/launcher/launcher.rc $<TARGET_OBJECTS:bof3x_launcher_parts>)
  target_compile_definitions(${target} PRIVATE BOF3X_HOSTED=${hosted})
  target_include_directories(${target} PRIVATE src third_party "${_gen_dir}")
  add_dependencies(${target} bof3x_symbols bof3x_suites)
# SDL3 here too, static (docs/launcher-settings.md section 2): the Controls
# dialog's capture reads the pad, and the dialogs take the pad's d-pad, cross
# and circle as navigation.
# shell32 and ole32: the Cache folder box's Browse... (SHBrowseForFolderW);
# comdlg32: the Sources page's File... and Locate ffmpeg.exe... (GetOpenFileNameW);
# windowscodecs: the title page's picture, decoded by WIC (docs/launcher-settings.md 2a).
  target_link_libraries(${target} PRIVATE comctl32 comdlg32 shell32 ole32 windowscodecs winmm SDL3::SDL3-static bof3x_os)
  # -mwindows: no console of its own; wmain still the entry, AttachParentConsole
  # (launcher.cpp) writes to the terminal it was started from.
  target_link_options(${target} PRIVATE -municode -mwindows)
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${output}")
endfunction()
bof3x_add_launcher(bof3x_launcher bof3x-launcher 0)
# bof3x-fmvdump: the FMV decoders (src/video, DIV-0205) as a console tool
# that writes an AVI's frames raw, for tools/fmv_compare.py to hold against
# ffmpeg's decode of the same file. Not a player file.
add_executable(bof3x_fmvdump src/video/fmv_dump.cpp src/video/avi_reader.cpp src/video/cinepak.cpp
  src/video/msvideo1.cpp src/video/movie.cpp)
target_include_directories(bof3x_fmvdump PRIVATE src)
set_target_properties(bof3x_fmvdump PROPERTIES OUTPUT_NAME "bof3x-fmvdump")
# The developer launcher starts BOF3.exe with the DLL injected, which only a
# 32-bit process can do to a 32-bit one: the hosted build's alone.
if(BOF3X_HOSTED_BUILD)
  bof3x_add_launcher(bof3x_launcher_dev bof3x-launcher-dev 1)
endif()
# The Jukebox's names (docs/music-player.md section 4), beside the launcher.
# Edit data/music-names.tsv, not the copy: every build refreshes it.
# A rule of its own, so an edit to the file alone is copied too.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/music-names.tsv"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/data/music-names.tsv" "${CMAKE_BINARY_DIR}/music-names.tsv"
  DEPENDS "${CMAKE_SOURCE_DIR}/data/music-names.tsv"
  VERBATIM)
add_custom_target(bof3x_music_names DEPENDS "${CMAKE_BINARY_DIR}/music-names.tsv")
add_dependencies(bof3x_launcher bof3x_music_names)
# The save editor's reference names (docs/save-editor.md section 4), the same way.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/save-names.tsv"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/data/save-names.tsv" "${CMAKE_BINARY_DIR}/save-names.tsv"
  DEPENDS "${CMAKE_SOURCE_DIR}/data/save-names.tsv"
  VERBATIM)
add_custom_target(bof3x_save_names DEPENDS "${CMAKE_BINARY_DIR}/save-names.tsv")
add_dependencies(bof3x_launcher bof3x_save_names)

# --- every object, no link ---------------------------------------------------
# The x86_64 CI job's compile step (docs/x64-build-round.md 3 "XA4"): a
# target that builds every object of the DLL, the launcher parts, the
# launcher and SDL3 and links nothing, so a build whose link cannot yet
# resolve (the 64-bit build until the round's stage C) still gives a whole
# compile log. `cmake --build <dir> --target bof3x_compile`. Not ALL: a plain
# build is unchanged by it.
add_custom_target(bof3x_compile
  DEPENDS $<TARGET_OBJECTS:bof3x> $<TARGET_OBJECTS:bof3x_launcher_parts>
          $<TARGET_OBJECTS:bof3x_launcher> $<TARGET_OBJECTS:SDL3-static>)
