# DXVK-native's two libraries, libdxvk_d3d11.so and libdxvk_dxgi.so, built
# for the Linux ELF (docs/linux-round.md section 6, call 1; stage LC, group
# C1). cmake/linux_target.cmake includes this file; a Windows configure never
# reads it.
#
# Pinned to the commit cmake/dxvk_headers.cmake names (v3.1.1,
# b1a1c99ab52b687cf950d62c88bc2fa316b41663), so the headers the engine
# compiles against and the libraries it links are one DXVK. DXVK builds with
# meson; this is an external project, not a CMake subproject. Built with
# -Dnative_sdl3=enabled: the window is SDL's (src/game/platform/
# game_window_other.cpp) and DXVK's surface is made from it. Direct3D 8, 9
# and 10 are not built; the game uses 11 alone. Nothing of DXVK enters this
# repository (docs/LICENSING.md section 4; DXVK is zlib).
#
# Configured only on a Linux host: meson, ninja, glslang (DXVK compiles its
# shaders with it at build time) and a C++ compiler for the host are needed,
# and the libraries are the host's own build. On Windows (the cross build of
# cmake/x86_64-linux.cmake) BOF3X_DXVK_LIBRARIES stays empty and the link
# reports DXVK's names as undefined, which is what the measure of the link
# wants to count.
#
# What DXVK does at run time, read at that commit
# (src/wsi/sdl3/wsi_platform_sdl3.cpp): its SDL3 driver opens "libSDL3.so.0"
# by name and calls SDL through it. SDL is only its headers at build time
# (meson's partial_dependency: compile arguments and includes); the
# sdl3.pc written below points meson at the SDL3 this build fetched.
set(BOF3X_DXVK_LIBRARIES "")
# Libraries built elsewhere (a Linux machine's meson build of the same commit,
# with the options below): their folder, and nothing is built here. The way a
# cross build on Windows links them.
set(BOF3X_DXVK_LIBDIR "" CACHE PATH "A folder holding libdxvk_d3d11.so and libdxvk_dxgi.so built at the pinned commit")
if(BOF3X_DXVK_LIBDIR)
  foreach(_lib d3d11 dxgi)
    if(NOT EXISTS "${BOF3X_DXVK_LIBDIR}/libdxvk_${_lib}.so")
      message(FATAL_ERROR "BOF3X_DXVK_LIBDIR=${BOF3X_DXVK_LIBDIR} holds no libdxvk_${_lib}.so")
    endif()
    add_library(bof3x_dxvk_${_lib} SHARED IMPORTED GLOBAL)
    set_target_properties(bof3x_dxvk_${_lib} PROPERTIES IMPORTED_LOCATION "${BOF3X_DXVK_LIBDIR}/libdxvk_${_lib}.so")
    list(APPEND BOF3X_DXVK_LIBRARIES bof3x_dxvk_${_lib})
  endforeach()
  return()
endif()
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "bof3x: DXVK-native's libraries are built on a Linux host only (or named by BOF3X_DXVK_LIBDIR); "
                 "this link leaves their names undefined")
  return()
endif()

find_program(BOF3X_MESON meson REQUIRED)
find_program(BOF3X_NINJA ninja REQUIRED)
find_program(BOF3X_GLSLANG NAMES glslang glslangValidator)
if(NOT BOF3X_GLSLANG)
  message(FATAL_ERROR "DXVK-native needs glslang to build its shaders (glslang or glslangValidator on PATH; "
                      "Debian and Ubuntu: glslang-tools)")
endif()
# meson finds dependency('sdl3') through pkg-config alone (measured in this
# machine's WSL, which has none: "Pkg-config for machine host machine not
# found").
find_program(BOF3X_PKG_CONFIG NAMES pkg-config pkgconf)
if(NOT BOF3X_PKG_CONFIG)
  message(FATAL_ERROR "DXVK-native's meson build needs pkg-config to find SDL3's headers (Debian and Ubuntu: "
                      "pkgconf)")
endif()

# SDL3's headers for meson's dependency('sdl3'): the fetched source's include
# folder and the revision header SDL's configure wrote.
set(_dxvk_pc_dir "${CMAKE_BINARY_DIR}/gen/dxvk-pkgconfig")
file(MAKE_DIRECTORY "${_dxvk_pc_dir}")
file(WRITE "${_dxvk_pc_dir}/sdl3.pc"
  "Name: sdl3\n"
  "Description: SDL3's headers, as bof3x fetched them (cmake/dxvk_native.cmake)\n"
  "Version: 3.4.16\n"
  "Cflags: -I${sdl3_SOURCE_DIR}/include -I${sdl3_BINARY_DIR}/include-revision\n"
  "Libs:\n")

include(ExternalProject)
set(_dxvk_prefix "${CMAKE_BINARY_DIR}/_deps/dxvk-native")
ExternalProject_Add(dxvk_native
  GIT_REPOSITORY https://github.com/doitsujin/dxvk.git
  GIT_TAG b1a1c99ab52b687cf950d62c88bc2fa316b41663   # v3.1.1, as cmake/dxvk_headers.cmake
  GIT_SUBMODULES_RECURSE ON
  PREFIX "${_dxvk_prefix}"
  CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env "PKG_CONFIG_PATH=${_dxvk_pc_dir}"
                    "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
                    "${BOF3X_MESON}" setup <BINARY_DIR> <SOURCE_DIR>
                    --buildtype=release --prefix=<INSTALL_DIR> --libdir=lib
                    -Dnative_sdl3=enabled -Dnative_sdl2=disabled -Dnative_glfw=disabled
                    -Denable_d3d8=false -Denable_d3d9=false -Denable_d3d10=false
                    -Denable_d3d11=true -Denable_dxgi=true
  BUILD_COMMAND "${BOF3X_NINJA}" -C <BINARY_DIR>
  INSTALL_COMMAND "${BOF3X_MESON}" install -C <BINARY_DIR>
  BUILD_BYPRODUCTS "${_dxvk_prefix}/lib/libdxvk_d3d11.so" "${_dxvk_prefix}/lib/libdxvk_dxgi.so")

foreach(_lib d3d11 dxgi)
  add_library(bof3x_dxvk_${_lib} SHARED IMPORTED GLOBAL)
  set_target_properties(bof3x_dxvk_${_lib} PROPERTIES IMPORTED_LOCATION "${_dxvk_prefix}/lib/libdxvk_${_lib}.so")
  add_dependencies(bof3x_dxvk_${_lib} dxvk_native)
  list(APPEND BOF3X_DXVK_LIBRARIES bof3x_dxvk_${_lib})
endforeach()
