# Toolchain: llvm-mingw targeting 32-bit Windows.
#
# 32-bit is not a preference. Our DLL is loaded into BOF3.exe's address space,
# and BOF3.exe is an i386 image — this stays true until the last original
# instruction is gone (docs/prior-art/openrct2.md section 2.2).
#
# Deliberately the only supported toolchain (docs/prior-art/tr1x.md section
# 2.8): the way to never grow an MSVC dependency is to never have an MSVC path.
# The i686-w64-mingw32-* drivers must be on PATH, or set LLVM_MINGW_ROOT.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(_triple i686-w64-mingw32)
if(DEFINED ENV{LLVM_MINGW_ROOT})
  set(_bin "$ENV{LLVM_MINGW_ROOT}/bin/")
else()
  set(_bin "")
endif()

set(CMAKE_C_COMPILER   "${_bin}${_triple}-clang")
set(CMAKE_CXX_COMPILER "${_bin}${_triple}-clang++")
set(CMAKE_RC_COMPILER  "${_bin}${_triple}-windres")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
