#!/bin/bash
# linux_sysroot.sh DEST - copy what the linux-x86_64 preset needs from this
# Linux system into DEST, a sysroot a clang on Windows can read.
#
# Why: the preset's cross build on Windows (cmake/x86_64-linux.cmake) can
# compile against the WSL's root through //wsl.localhost/<distribution>, but
# it cannot link there: Windows does not follow the WSL's symbolic links
# (/lib64, libstdc++.so, libmvec.so and others read as I/O errors), and
# libc.so's linker script names /lib64/ld-linux-x86-64.so.2. This script runs
# on the Linux side (in the WSL: wsl -e bash cmake/linux_sysroot.sh
# /mnt/c/...) and copies the headers and the link files with every link
# followed, so DEST holds plain files only.
#
# What it copies: /usr/include; the gcc folder clang's driver reads
# (crtbegin.o, libgcc, libstdc++); glibc's start files and libraries; the
# dynamic loader at /lib64 as libc.so names it. No game data, nothing of
# this repository. DEST is a build input like a build folder: keep it out of
# the repository (C:/Users/<you>/bof3x-runs/linux-sysroot, say).
#
# On a Linux machine the preset needs none of this: it builds against /.
set -euo pipefail
if [ $# -ne 1 ]; then
  echo "usage: linux_sysroot.sh DEST" >&2
  exit 2
fi
dest=$1
multi=x86_64-linux-gnu
gcc_dir=$(ls -d /usr/lib/gcc/$multi/* 2>/dev/null | sort -V | tail -n 1)
if [ -z "$gcc_dir" ]; then
  echo "linux_sysroot: no /usr/lib/gcc/$multi/<version>: install g++ (libstdc++'s headers and libraries)" >&2
  exit 1
fi
mkdir -p "$dest/usr" "$dest/usr/lib/$multi" "$dest/usr/lib/gcc/$multi" "$dest/lib64"
echo "linux_sysroot: /usr/include"
rm -rf "$dest/usr/include"
# On a Windows disk (/mnt/c) names are not case-sensitive, and a few of the
# kernel's netfilter headers differ in case alone (xt_CONNMARK.h and
# xt_connmark.h): tar writes the second of each pair over the first, where cp
# would fail. No source here includes them. -h follows the links.
tar -C /usr -chf - include | tar -C "$dest/usr" -xf -
echo "linux_sysroot: $gcc_dir"
rm -rf "$dest$gcc_dir"
cp -rL "$gcc_dir" "$dest$gcc_dir"
echo "linux_sysroot: /usr/lib/$multi (start files and glibc)"
for f in crt1.o crti.o crtn.o Scrt1.o rcrt1.o \
         libc.so libc.so.6 libc_nonshared.a ld-linux-x86-64.so.2 \
         libm.so libm.so.6 libmvec.so libmvec.so.1 \
         libdl.so.2 libdl.a libpthread.so.0 libpthread.a librt.so.1 librt.a \
         libgcc_s.so.1 libstdc++.so.6; do
  if [ -e "/usr/lib/$multi/$f" ]; then
    cp -L "/usr/lib/$multi/$f" "$dest/usr/lib/$multi/$f"
  fi
done
cp -L /usr/lib/$multi/ld-linux-x86-64.so.2 "$dest/lib64/ld-linux-x86-64.so.2"
echo "linux_sysroot: done: $dest ($(du -sh "$dest" | cut -f1))"
