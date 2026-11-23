# The Linux build (--preset linux-x86_64, cmake/x86_64-linux.cmake;
# docs/linux-round.md, stage LC, group C1). CMakeLists.txt includes this file
# when the target is not Windows, in place of everything from the DLL's
# target down, and stops there: no PE, no make_exe.py, and of the launchers
# the player's alone (bof3x_launcher below, bof3x-launcher: the drawn launcher
# of docs/launcher-portable.md, the player's on both systems since stage P6;
# the developer launcher is Windows' only).
#
# One target, `bof3x`: an ELF executable made of the exe host's sources -
# the 64-bit DLL's list (_dll_sources in CMakeLists.txt, fuzz files already
# out) - less the Windows files by design and plus their counterparts. The
# rule is tools/linux_probe/linux_tree.py's (--cmake-list), and so is the
# table it reads (WINDOWS_ONLY, SKIP): the build and the probe cannot drift.
#
# Since wave B (B1) the link completes with no undefined name: the start-up's
# `main` is src/hook/exe_host_posix.cpp, and the files the Windows list never
# holds are linux_tree.py's LINUX_ONLY. `bof3x_compile` builds every object
# and links nothing, as on Windows.

# --- the source list ----------------------------------------------------------
set(_linux_in "${CMAKE_BINARY_DIR}/gen/linux_sources_windows.txt")
set(_linux_gen "${CMAKE_BINARY_DIR}/gen/linux_sources.gen.cmake")
string(REPLACE ";" "\n" _linux_in_text "${_dll_sources}")
file(WRITE "${_linux_in}" "${_linux_in_text}\n")
execute_process(
  COMMAND "${BOF3X_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/linux_probe/linux_tree.py" --cmake-list "${_linux_in}" "${_linux_gen}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  RESULT_VARIABLE _linux_rc)
if(NOT _linux_rc EQUAL 0)
  message(FATAL_ERROR "tools/linux_probe/linux_tree.py --cmake-list failed (${_linux_rc})")
endif()
include("${_linux_gen}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/linux_probe/linux_tree.py")

# --- how every object of ours is compiled --------------------------------------
# __cdecl as ms_abi (stage LA3's road, docs/linux-round.md "ms_abi verdict"):
# the asm texts are written to the Win64 convention, and one text serves both
# systems when every __cdecl function keeps it. __stdcall, WINAPI and
# STDMETHODCALLTYPE stay as DXVK-native's headers define them (empty), so the
# calls into DXVK and SDL keep System V. -D__stdcall= gives every file that
# empty __stdcall, also one that includes no DXVK header (the generated
# symbols.gen.h, the *_callees.h): clang ignores the keyword on x86-64 Linux
# and said so once per use, 6,574 of the build's -Wignored-attributes, so
# the code is the same and the warning stays on for every other attribute.
# DXVK's own `#define __stdcall` is the same empty text, so it redefines
# nothing. -fdeclspec: __declspec(naked) and the others. The same flags as
# tools/linux_probe/linux_tree.py's compile.
# -fno-pie: the image's code reaches our functions by rel32 (stage LA2), so
# the ELF is not position-independent.
set(BOF3X_LINUX_COMPILE_OPTIONS -fdeclspec "-D__cdecl=__attribute__((ms_abi))" -D__stdcall= -fno-pie)

# --- how the ELF is linked ------------------------------------------------------
# Non-PIE, linked at 0x10000000 (stage LA2, docs/linux-round.md "LA2, low
# memory"): the data image is mapped by MAP_FIXED_NOREPLACE at 0x400000 or at
# the moved base 0xA00000, and an ELF at the default non-PIE base 0x400000
# takes that range itself (the probe's mapping failed with EEXIST). At
# 0x10000000 the image's rel32 calls still reach it (the probe's stub,
# +264,241,723 bytes), and it is the address the Windows DLL prefers too.
# GNU ld spells it -Ttext-segment; ld.lld refuses that spelling and takes
# --image-base, which places the first segment the same way.
set(BOF3X_LINUX_BASE 0x10000000)
include(CheckLinkerFlag)
check_linker_flag(CXX "-Wl,-Ttext-segment=${BOF3X_LINUX_BASE}" BOF3X_LINK_TTEXT_SEGMENT)
if(BOF3X_LINK_TTEXT_SEGMENT)
  set(_linux_base_flag "-Wl,-Ttext-segment=${BOF3X_LINUX_BASE}")
else()
  check_linker_flag(CXX "-Wl,--image-base=${BOF3X_LINUX_BASE}" BOF3X_LINK_IMAGE_BASE)
  if(NOT BOF3X_LINK_IMAGE_BASE)
    message(FATAL_ERROR "the linker takes neither -Ttext-segment nor --image-base: the ELF cannot be linked at "
      "${BOF3X_LINUX_BASE} (docs/linux-round.md, LA2)")
  endif()
  set(_linux_base_flag "-Wl,--image-base=${BOF3X_LINUX_BASE}")
endif()

# --- DXVK-native's libraries (cmake/dxvk_native.cmake) --------------------------
include(cmake/dxvk_native.cmake)

# --- the media decoders: src/media, as the Windows list's bof3x_media ----------
file(GLOB _linux_media CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src/media/*.cpp")
add_library(bof3x_media STATIC ${_linux_media})
target_include_directories(bof3x_media PRIVATE src)
target_compile_options(bof3x_media PRIVATE ${BOF3X_LINUX_COMPILE_OPTIONS})
target_compile_options(bof3x_os PRIVATE ${BOF3X_LINUX_COMPILE_OPTIONS})
# The seam's message box (os/dialog.h) is SDL3's where the build links SDL's
# video: both programs that take bof3x_os here do. Without this a Fatal on
# Linux wrote to stderr and the log alone (B2b's finding, wave B).
target_compile_definitions(bof3x_os PRIVATE BOF3X_OS_SDL)
target_link_libraries(bof3x_os PRIVATE ${BOF3X_SDL})

# --- bof3x, the ELF ---------------------------------------------------------------
add_executable(bof3x ${BOF3X_LINUX_SOURCES})
add_dependencies(bof3x bof3x_symbols bof3x_rebase)
target_include_directories(bof3x PRIVATE src third_party "${_gen_dir}")
target_compile_options(bof3x PRIVATE ${BOF3X_LINUX_COMPILE_OPTIONS})
set_target_properties(bof3x PROPERTIES POSITION_INDEPENDENT_CODE OFF)
target_link_options(bof3x PRIVATE -no-pie ${_linux_base_flag})
# Every undefined name at once, not ld.lld's first 20: the list is wave B's
# work order (analysis/linux_link.tsv). GNU ld reports them all already.
check_linker_flag(CXX "-Wl,--error-limit=0" BOF3X_LINK_ERROR_LIMIT)
if(BOF3X_LINK_ERROR_LIMIT)
  target_link_options(bof3x PRIVATE -Wl,--error-limit=0)
endif()
target_link_libraries(bof3x PRIVATE bof3x_media bof3x_dxvk_headers ${BOF3X_SDL} bof3x_os ${BOF3X_DXVK_LIBRARIES})

# --- the three libraries beside the ELF (wave B, B1; the owner's call 3 of
# 2026-11-20) -------------------------------------------------------------------
# SDL3 is shared off Windows (CMakeLists.txt, BOF3X_SDL): DXVK-native opens
# "libSDL3.so.0" by name, so the ELF and DXVK must find one file. Both are
# found beside the ELF: RUNPATH $ORIGIN (the ELF's NEEDED names are the
# libraries' sonames - libSDL3.so.0, libdxvk_d3d11.so.0, libdxvk_dxgi.so.0 -
# and DXVK's dlopen of libSDL3.so.0 finds the copy the ELF has loaded
# already, by its soname). The build copies each library under its soname
# into the build folder, beside bof3x and bof3x-launcher, which is the
# tarball's layout (call 5 of section 6).
set_target_properties(bof3x PROPERTIES BUILD_RPATH "\$ORIGIN" INSTALL_RPATH "\$ORIGIN"
                      BUILD_WITH_INSTALL_RPATH ON)
# RUNPATH, not the older RPATH: LD_LIBRARY_PATH may still override it.
check_linker_flag(CXX "-Wl,--enable-new-dtags" BOF3X_LINK_NEW_DTAGS)
if(BOF3X_LINK_NEW_DTAGS)
  target_link_options(bof3x PRIVATE -Wl,--enable-new-dtags)
endif()
add_custom_command(TARGET bof3x POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>"
          "$<TARGET_FILE_DIR:bof3x>/$<TARGET_SONAME_FILE_NAME:SDL3::SDL3-shared>"
  COMMENT "Copying libSDL3.so.0 beside bof3x"
  VERBATIM)
foreach(_lib ${BOF3X_DXVK_LIBRARIES})
  # An imported library: its file, and its soname (libdxvk_<x>.so.0), read
  # from the file by the copy step's own name.
  add_custom_command(TARGET bof3x POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${_lib}>"
            "$<TARGET_FILE_DIR:bof3x>/$<TARGET_FILE_NAME:${_lib}>.0"
    COMMENT "Copying $<TARGET_FILE_NAME:${_lib}>.0 beside bof3x"
    VERBATIM)
endforeach()

# --- the launcher, bof3x-launcher (docs/launcher-portable.md, stages P1 to P6) ---
# The same sources as on Windows (BOF3X_LAUNCHER_SOURCES, CMakeLists.txt),
# compiled as the ELF's are: the seam's library is built with these options,
# and a caller of it must agree on __cdecl.
add_executable(bof3x_launcher ${BOF3X_LAUNCHER_SOURCES})
add_dependencies(bof3x_launcher bof3x_symbols bof3x_suites)
target_include_directories(bof3x_launcher PRIVATE src third_party "${_gen_dir}")
target_compile_options(bof3x_launcher PRIVATE ${BOF3X_LINUX_COMPILE_OPTIONS})
set_target_properties(bof3x_launcher PROPERTIES POSITION_INDEPENDENT_CODE OFF OUTPUT_NAME "bof3x-launcher")
target_link_options(bof3x_launcher PRIVATE -no-pie)
target_link_libraries(bof3x_launcher PRIVATE bof3x_imgui ${BOF3X_SDL} bof3x_os)
# RUNPATH $ORIGIN, as bof3x's: the launcher finds the libSDL3.so.0 beside it.
set_target_properties(bof3x_launcher PROPERTIES BUILD_RPATH "\$ORIGIN" INSTALL_RPATH "\$ORIGIN"
                      BUILD_WITH_INSTALL_RPATH ON)
if(BOF3X_LINK_NEW_DTAGS)
  target_link_options(bof3x_launcher PRIVATE -Wl,--enable-new-dtags)
endif()
# The Jukebox's names beside it (stage P5), as CMakeLists.txt copies them for
# the Windows launcher: edit data/music-names.tsv, not the copy.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/music-names.tsv"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/data/music-names.tsv" "${CMAKE_BINARY_DIR}/music-names.tsv"
  DEPENDS "${CMAKE_SOURCE_DIR}/data/music-names.tsv"
  VERBATIM)
add_custom_target(bof3x_music_names DEPENDS "${CMAKE_BINARY_DIR}/music-names.tsv")
add_dependencies(bof3x_launcher bof3x_music_names)
# The save editor's reference names beside it (stage P5), as CMakeLists.txt copies them on Windows.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/save-names.tsv"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/data/save-names.tsv" "${CMAKE_BINARY_DIR}/save-names.tsv"
  DEPENDS "${CMAKE_SOURCE_DIR}/data/save-names.tsv"
  VERBATIM)
add_custom_target(bof3x_save_names DEPENDS "${CMAKE_BINARY_DIR}/save-names.tsv")
add_dependencies(bof3x_launcher bof3x_save_names)

# --- every object, no link (as CMakeLists.txt's bof3x_compile on Windows) --------
add_custom_target(bof3x_compile
  DEPENDS $<TARGET_OBJECTS:bof3x> $<TARGET_OBJECTS:bof3x_media> $<TARGET_OBJECTS:bof3x_os> $<TARGET_OBJECTS:SDL3-shared>)
