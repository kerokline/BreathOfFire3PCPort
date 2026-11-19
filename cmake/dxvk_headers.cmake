# DXVK-native's Direct3D 11 and DXGI headers, for a build that is not
# Windows' (docs/linux-round.md section 6, call 1; R9). CMakeLists.txt
# includes this file only when the target is not Windows: a Windows configure
# never reads it, fetches nothing and keeps the toolchain's own headers.
#
# Headers only. Nothing of DXVK is built here and nothing is copied into this
# repository: the libraries a Linux build links (libdxvk_d3d11.so,
# libdxvk_dxgi.so, meson -Dnative_sdl3=enabled) are stage LC's.
#
# Pinned to the tag v3.1.1 by its commit (the tag object 1e0f9231 points at
# it), the version the probe measured (tools/linux_probe/dxvk_probe.cpp).
# Of DXVK's submodules only include/native/directx is fetched: the
# mingw-directx-headers that hold d3d11.h, dxgi.h and the rest.
#
# Licences: DXVK is zlib (its LICENSE). The directx submodule is MinGW-w64's
# DirectX headers, LGPL 2.1 or later, with its README's note that a program
# using them is not bound by it (LGPL 2.1 section 5: data layouts, small
# macros and inline functions) - the same headers, from the same MinGW-w64
# source, that the Windows builds already compile against through
# llvm-mingw. Nothing of either is vendored (docs/LICENSING.md section 4).
include(FetchContent)
FetchContent_Declare(dxvk
  GIT_REPOSITORY https://github.com/doitsujin/dxvk.git
  GIT_TAG b1a1c99ab52b687cf950d62c88bc2fa316b41663   # v3.1.1
  GIT_SUBMODULES include/native/directx
  # DXVK builds with meson; no CMake project of its is entered.
  SOURCE_SUBDIR no-cmake-project-here)
FetchContent_MakeAvailable(dxvk)

# windows/ holds DXVK's stand-ins for windows.h and the COM headers that
# d3d11.h includes; directx/ the Direct3D headers themselves. A file that
# includes <windows.h> itself must not lean on the first (os_census.py's
# boundary gate; linux_tree.py --dxvk counts it as a failure).
add_library(bof3x_dxvk_headers INTERFACE)
target_include_directories(bof3x_dxvk_headers SYSTEM INTERFACE
  "${dxvk_SOURCE_DIR}/include/native/windows"
  "${dxvk_SOURCE_DIR}/include/native/directx")
