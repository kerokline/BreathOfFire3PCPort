#!/bin/sh
# Build the synthetic corpus (corpus.c) as a /FIXED i686 PE at 0x400000 with no
# CRT, BOF3.exe's layout, and write its entry list for lift_x86.py.
#
#   sh tools/lift/corpus/build.sh OUTDIR
#
# -march=pentium -mno-sse: x87 floating point and no cmov, as an MSVC6 build
# targets. Needs clang with the i686-w64-mingw32 target, lld and llvm-nm; no
# mingw headers or libraries (the corpus includes nothing).
set -eu
out=${1:?output directory}
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
clang --target=i686-w64-mingw32 -O2 -march=pentium -mno-sse -mno-mmx \
    -ffreestanding -fno-stack-protector -fno-asynchronous-unwind-tables \
    -nostdlib -fuse-ld=lld \
    -Wl,--image-base=0x400000 -Wl,--disable-dynamicbase -Wl,--disable-reloc-section \
    -Wl,--entry=entry \
    "$here/corpus.c" -o "$out/corpus.exe"
# Every function symbol in .text, with the handlers only a table reaches: a
# lifter needs its entries given, as BOF3.exe's come from symbols.toml and the
# entry lists, not from call sites alone.
llvm-nm "$out/corpus.exe" | awk '$2 ~ /^[Tt]$/ { print $1, $3 }' | sort > "$out/entries.txt"
echo "corpus: $(wc -l < "$out/entries.txt") entries -> $out/corpus.exe"
