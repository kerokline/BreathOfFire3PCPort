# State 3: the first build of our own executable

**Status:** IN PROGRESS (2026-10-11, on `platform/state3` from `5a447d87`; the
build agent's record against [`state3-design.md`](state3-design.md) - the
probes, what was built, every check with its command and exit code)

The design is [`state3-design.md`](state3-design.md). This document records
what the build measured and did, in its order (the brief's section 11), and
where the build found the brief wrong. A check that was not run is listed as
not run.

## 1. The two probes of the brief's section 12

Both run on the owner's machine (Windows 11 Pro 10.0.26200, llvm-mingw
clang 22.1.8), session scripts in the build's scratchpad, before any code.

### 1.1 Where the heap goes when the image is based elsewhere

A console image with no runtime (`-nostdlib`, entry `Entry`) that prints, at
its entry point, `GetModuleHandle(nullptr)`, `GetProcessHeap()`, its TEB and
PEB, and `VirtualQuery`'s regions over `0x400000..0x93F000`:

    i686-w64-mingw32-clang -O1 -nostdlib -Wl,--entry,_Entry -Wl,--image-base,0x10000000 \
        [-Wl,--disable-dynamicbase] -Wl,--subsystem,console heap_probe.c -lkernel32 -luser32

| Image | Runs | Range wholly free at the entry | Process heap inside the range |
|---|--:|--:|--:|
| based `0x10000000`, `--disable-dynamicbase` (loaded at `0x10000000`) | 20 | **0** | **20** (at `0x530000..0x780000`) |
| based `0x10000000`, ASLR on (lld's default; loaded at `0x0F250000` / `0x0FA40000`) | 20 | 6 | 14 |

With the image fixed elsewhere every run found 17 or 18 free gaps and 30 to
33 occupied regions in Capcom's range: the PEB and TEB below `0x400000`, the
first thread's stack reservation, the process heap, the code page and locale
mappings (`MEM_MAPPED`, type `0x40000`) and the activation-context data, all
before the image's first instruction. The brief's claim (1.1) is measured
true: **a run-time reservation at the entry comes too late**, so the image
itself must be based at `0x400000` and span the range. (A result of "always
free" would not have changed the design, as the brief says; it is not the
result.)

### 1.2 Whether lld places a custom first section

An assembly file with a `.stub` section (`"bwx"`, `0x1C3000` bytes) and a
`.game` section (`"bw"`, `0x37B000`), linked by lld with
`--image-base 0x400000 --disable-dynamicbase`, once with `-nostdlib` and once
with the toolchain's CRT objects (a C `main` with a `.stub` array):

| Link | Section order lld wrote |
|---|---|
| `-nostdlib`, the `.s` alone | `.text` `0x1000`, `.rdata`, `.buildid`, `.game` `0x4000`, `.stub` `0x37F000`, `.reloc` |
| with the CRT | `.text` `0x1000`, `.rdata`, `.buildid`, `.data`, `.stub` `0x6000`, `.tls`, `.reloc`, the DWARF sections |

lld ranks its output sections itself (code first) and does not honour the
input order, and it writes a `.reloc` even with `--disable-dynamicbase`.
**The assembly route is closed**; the executable is written by
`tools/make_exe.py`, the brief's recommendation.

## 2. What was built

(Filled in as the build proceeds.)

## 3. The checks

(Filled in as the build proceeds.)
