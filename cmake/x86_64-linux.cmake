# Toolchain: clang targeting x86-64 Linux (docs/linux-round.md, stage LC).
#
# The third build, the --preset linux-x86_64: the exe host as one ELF, no
# hosted mode, no shadow fuzz (CMakeLists.txt reads the width from
# CMAKE_SIZEOF_VOID_P, 8 here, as for the x86_64 preset), no launcher yet.
# What the target asks of a link - non-PIE, linked high, __cdecl as ms_abi -
# is cmake/linux_target.cmake's, not this file's: this file names the
# compiler and the system only.
#
# Two ways to use it:
#
#   * On a Linux machine with clang: nothing to set. The system's own headers
#     and libraries are the sysroot, and the system's linker links.
#   * On Windows, with the clang of the llvm-mingw toolchain (any clang
#     targets any triple): set BOF3X_LINUX_SYSROOT to a Linux root that holds
#     libstdc++'s headers and glibc's link files as plain files.
#     cmake/linux_sysroot.sh makes one from the WSL (Windows does not follow
#     the WSL's symbolic links, so //wsl.localhost/<distribution> compiles
#     but does not link). The link is then ld.lld's, from the same toolchain.
#     Nothing built this way runs on Windows: it is the link's measure, and a
#     build to copy into a Linux system.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_triple x86_64-linux-gnu)
set(CMAKE_C_COMPILER   clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET   ${_triple})
set(CMAKE_CXX_COMPILER_TARGET ${_triple})

if(DEFINED ENV{BOF3X_LINUX_SYSROOT} AND NOT "$ENV{BOF3X_LINUX_SYSROOT}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{BOF3X_LINUX_SYSROOT}" CMAKE_SYSROOT)
endif()
if(CMAKE_HOST_WIN32)
  if(NOT CMAKE_SYSROOT)
    message(FATAL_ERROR
      "cmake/x86_64-linux.cmake on Windows needs BOF3X_LINUX_SYSROOT: a Linux root with libstdc++'s headers "
      "and glibc's link files as plain files. Make one from the WSL with "
      "wsl -e bash cmake/linux_sysroot.sh /mnt/c/<folder>, then set BOF3X_LINUX_SYSROOT=C:/<folder>.")
  endif()
  if(NOT EXISTS "${CMAKE_SYSROOT}/usr/lib/x86_64-linux-gnu/crt1.o")
    message(FATAL_ERROR "BOF3X_LINUX_SYSROOT=${CMAKE_SYSROOT} holds no usr/lib/x86_64-linux-gnu/crt1.o: "
      "not a sysroot cmake/linux_sysroot.sh made")
  endif()
  # Windows' own linker cannot write an ELF: the toolchain's ld.lld does.
  set(CMAKE_EXE_LINKER_FLAGS_INIT    "-fuse-ld=lld")
  set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
  set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
  # The build's pkg-config, if any, is Windows': it may find nothing of the
  # host's (SDL3 probes for dbus, X11 and others through it).
  set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/x86_64-linux-gnu/pkgconfig")
  set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
endif()

if(CMAKE_SYSROOT)
  set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()
