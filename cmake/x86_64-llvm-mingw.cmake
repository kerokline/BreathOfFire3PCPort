# Toolchain: llvm-mingw targeting 64-bit Windows (x86-64).
#
# The second width of the 64-bit build round (docs/x64-build-round.md 3
# "XA4", the owner's call 2: x86-64 Windows alone, with the stack switch
# shaped for a third variant). It builds the exe host - bof3x.exe and the
# bof3x.dll it imports - and the player's launcher. It never builds what
# loads into BOF3.exe: the hosted mode, the shadow fuzz and the developer
# launcher are the i686 build's for ever (round doc section 2), since
# BOF3.exe is an i386 image. CMakeLists.txt reads the width from
# CMAKE_SIZEOF_VOID_P and drops those targets for 8.
#
# Same toolchain, same driver family (cmake/i686-llvm-mingw.cmake says why
# there is only one): the x86_64-w64-mingw32-* drivers sit beside the i686
# ones in every llvm-mingw release. They must be on PATH, or set
# LLVM_MINGW_ROOT.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(_triple x86_64-w64-mingw32)
if(DEFINED ENV{LLVM_MINGW_ROOT})
  set(_bin "$ENV{LLVM_MINGW_ROOT}/bin/")
else()
  set(_bin "")
endif()

set(CMAKE_C_COMPILER   "${_bin}${_triple}-clang")
set(CMAKE_CXX_COMPILER "${_bin}${_triple}-clang++")
set(CMAKE_RC_COMPILER  "${_bin}${_triple}-windres")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
