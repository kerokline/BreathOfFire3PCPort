# State 3: the first build of our own executable

**Status:** MEASURED (merged as PR #53, 2026-10-12; verified 2026-10-11, on `platform/state3` from `5a447d87`; the
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

### 2.1 The executable (`tools/make_exe.py`)

Written after `bof3x.dll` by a CMake custom command and verified there; the
layout is one table, [`src/hook/exe_layout.h`](../src/hook/exe_layout.h),
which the engine reads too and the script parses (`kName = 0x...;` lines).

    python tools/pe_read.py build/bof3x.exe
    base 0x400000 entry 0x987000 timestamp 0x6acc8e00 characteristics 0x10f dll characteristics 0x0
      subsystem 2 4.0 stack 0x100000/0x10000 size 0x58b000
      .stub    0x00401000 +0x1c3000  raw 0x00000  0xe00000a0
      .game    0x005c4000 +0x37b000  raw 0x00000  0xc0000080
      .pool    0x0093f000 +0x048000  raw 0x00200  0xc0000040
      .text    0x00987000 +0x00003d  raw 0x00200  0x60000020
      .idata   0x00988000 +0x0000e0  raw 0x00200  0xc0000040
      .rsrc    0x00989000 +0x001528  raw 0x01600  0x40000040
      import KERNEL32.dll: ExitProcess, GetModuleHandleW, GetCommandLineA, GetStartupInfoA
      import bof3x.dll: Engine_Main
      resource (3, 1, 0): 4264 bytes            the placeholder icon, 32 x 32, 32 bpp (until DIV-0095's)
      resource (14, 'BOF3_ICON', 0): 20 bytes   the name Game_WinMain's LoadIconA asks for
      resource (16, 1, 0): ...                  the version block (2026-10-16, below)
      resource (24, 1, 0): 867 bytes            the manifest

The verification (`make_exe.py`'s `verify`, through `tools/pe_read.py`, the
capstone-free parser `pe_funcs.py` now takes its `parse_pe` from) checks the
base, the timestamp, `RELOCS_STRIPPED`, no `DYNAMIC_BASE`, no base-relocation
directory, the stack, the six sections' names, VAs, sizes, file bytes and
protections, the `.pool` stamp, the entry inside `.text`, the import list
exactly, the three resources, the manifest's three declarations, and that
`bof3x.dll` exports `Engine_Main`; a mismatch deletes the file and fails the
build. The entry, as capstone reads it:

    sub esp, 0x44 / push esp / call [GetStartupInfoA]
    mov eax, 10 / test byte [esp+0x2c], 1 / je +5 / movzx eax, word [esp+0x30]   show
    push eax / call [GetCommandLineA] / push eax / push 0 / call [GetModuleHandleW] / push eax
    call [Engine_Main] / add esp, 0xc / push eax / call [ExitProcess] / int3

Three choices the brief did not spell out, made to keep behaviour:

- **The subsystem and OS version are 4.0 and `DllCharacteristics` is 0**, as
  `BOF3.exe`'s (llvm-readobj, 2026-10-11): below 6.0 Windows keeps the old
  window-frame metrics the game's window was laid out under, and no DEP or
  ASLR opt-in changes from the hosted process.
- **The manifest declares DPI awareness** (per-monitor v2, the v1 line as
  the fallback) since the owner's call of 2026-10-11 - the first build left
  it out because `BOF3.exe` has no manifest and the brief had not decided
  it. DIV-0036 is amended: the default windowed k follows the display's
  scale (twice the scale rounded down, never below 2), so the window opens
  the same apparent size as the stretched one did, now sharp. Common
  controls 6 (themed message boxes), `activeCodePage` UTF-8 and
  `longPathAware` are declared too.
- **The program name is skipped in `Engine_Main`** (`PastProgramName`, C++),
  not in the hand-assembled entry: the same `_wincmdln` rule, reviewable.

**The version block and the signature check (2026-10-16).** A VirusTotal run
of the launcher that day scored 11/70, every hit a machine-learning label
(Wacatac.B!ml, "Suspicious PE", Crypt.XPACK.Gen) keyed on the shape above:
two sections with no file bytes, a 61-byte `.text`, four imports, a
timestamp that read as 2064, no version block, no signature. The layout
stays; `kTimestamp` became `0x6ACC8E00` (2026-10-12, a past date); every
binary now carries a `VS_VERSIONINFO` - the DLL's and the launcher's from
`src/version.rc`, the executable's written by `make_exe.py`'s
`build_version` from `--version` (CMake's `project(VERSION)`) and
`--version-tag` (the release tag, `-DBOF3X_VERSION_TAG`, which the release
workflow passes; `v<version>` locally). The verify checks the block byte for
byte against what it would write. It also accepts an Authenticode signature:
data directory 4 absent, or naming exactly the file's trailing blob with a
`WIN_CERTIFICATE` header of revision 2.0 and type 2 - so a signed release
re-verifies with `--verify-only` and is reported signed (tested with a
synthetic blob; a wrong type or an offset inside the image fails). Signing
itself is the release workflow's step, once the owner's signing identity
exists (Azure Trusted Signing was being set up that day).

The custom target depends on a stamp, not on `bof3x.exe`: CMake reads a
`DEPENDS` path named `<target>.exe` as the target `bof3x` (the DLL) and
dropped the file rule - the first configure built no executable at all.

### 2.2 The engine side (`src/hook/host.*`, `exe_host.*`, `code_bytes.*`)

`DllMain` opens the log and calls `DetectHost` (Capcom's timestamp and
`SizeOfImage`, or ours with `.stub` at `0x401000`; anything else a Fatal);
in the exe host it returns. `Engine_Main` then runs section 4 in order. The
exe-host log of a self-test (`build/st_exe/bof3x.log`, 2026-10-11):

    host: bof3x.exe, our own executable (main module at 0x400000, timestamp 0x6ACC8E00)
    Engine_Main: hinstance 00400000, show 10, on thread 27632
    code page: GetACP() = 65001 (UTF-8, as the manifest asks)
    layout: .stub 0x00401000 +0x1C3000 RWX, .game 0x005C4000 +0x37B000 RW, .pool 0x0093F000 +0x48000 RW, as exe_layout.h
    data: ...\bof3\BOF3.exe, sha256 15984158...dfb8cb (the build symbols.toml describes)
    data: .rdata 0x005C4000 + 0x16000 bytes from the file (its section's file bytes: 0x16000), .data 0x005DA000
          + 0x9C000 bytes from the file (its file bytes: 0x9C000); .bss zero 0x00676000..0x0093F000 (virtual end 0x0093D6EC)
    slots: 14 import slots bound and read back (DSOUND #1, GDI32 3, KERNEL32 6, USER32 4); GetTickCount through 0x5C407C answers ...
    x87: control word 0x027F -> 0x027F (53-bit precision, every exception masked, round to nearest, ...); MXCSR 0x00001F80, not set
    stub: .stub 0x00401000..0x005C4000 filled with int3
    rdata: 44 constants (1.0f .. GUID_SysKeyboard (DInput_KeyboardGuid)) held by the engine, each equal to BOF3.exe's .rdata
    DIV-0062    draw-item pool's room at 0x0093F000 (288 KB, below 16 MB)
    inject: 10087 ours, 0 left original by BOF3X_ORIGINAL
    stub: inventory zero - .stub holds 10087 jmps of ours and DirectSoundCreate's thunk 0x005ACBBC, every other byte int3; ...
    stub: .stub is read-execute; a later write faults
    codebytes   after InjectAll: 43 reads, 450 bytes of BOF3.exe's .text held as data (none of it at its address, none executable)
    stack: header reserve 0x100000 commit 0x10000; main thread ... (this thread, Engine_Main's) ... (reserved 0x100000 bytes, committed 0x10000, ...)
    self-test only: done, ending the process

The fourteen slots' names were read from the player's import directory
(a session script over the import descriptors, 2026-10-11): `0x5C4014`
DSOUND #1; `0x5C4020/24/28` GDI32 `SetTextColor`, `SetBkMode`, `TextOutA`;
`0x5C407C/80/84/88` KERNEL32 `GetTickCount`, `CreateEventA`, `CloseHandle`,
`GetDriveTypeA`; `0x5C40B8`, `0x5C4114` `ExitProcess`, `TerminateProcess`;
`0x5C4158/64/68/74` USER32 `PostQuitMessage`, `MsgWaitForMultipleObjects`,
`IntersectRect`, `MessageBoxA` - every IAT address `src/` names (a grep for
`0x5C40xx..0x5C41B4`, plus `Imp_GetTickCount`). `0x5C4168` is `IntersectRect`,
which only the `Gfx_InvalidateTextures` clone calls.

**The x87 word was already `0x027F` at `Engine_Main`** (the log's "before"):
the brief's `0x037F` (section 4 step 5, from [`crt-startup.md`](crt-startup.md)
4.1) is what the toolchain's *executable* start-up leaves after its
`_fpreset`; `bof3x.exe` has no runtime start-up, and Windows starts a thread
with `0x027F`. The engine sets and asserts the word anyway, so the answer does
not depend on which image's start-up ran.

The count is **10,087**, not the brief's 10,088: the log's `inject:` line,
the same count the hosted build prints (HANDOFF's "`inject: 10087 ours`").

The per-host constants: `CodeLo()` / `CodeHi()` (`host.h`) are `.text`
(`0x401000` + `0x1C2668`, `symbols.toml`'s `text_va` / `text_size`, now in
`symbols.gen.h`) hosted and `.stub` in the exe host; `crash.cpp`'s
return-address filter and `calltrace.cpp`'s range read them; `exe_image.cpp`'s
`.data` extent is `exe_layout.h`'s; `stackwatch.cpp` takes the current thread
as the main thread in the exe host; `statehash.cpp` already used constants
(`0x400000..0x93F000`, which our image spans as Capcom's did);
`detour.cpp`'s `VerifyImage` runs hosted only.

The refusals, each run headless with `--host exe` (exit 3, the FATAL line):
`BOF3X_SHADOW=*`, `BOF3X_ORIGINAL=Game_WinMain`, `BOF3X_MP3=original`,
`BOF3X_CALLTRACE=x.txt`. **The call trace is refused in the exe host**, a
decision the brief left to this build (section 12): the tracer recognises
its own `int3` by address and resumes after putting the "original byte"
back - on the fill, that byte is `0xCC` again, and the thread would trap
forever; and every entry it could arm is either `int3` or a jmp of ours.

### 2.3 Section 7, measured again: forty reads, not one

The brief's section 7 counted **one** direct read of Capcom's code bytes
(`yes_no_layout.cpp:368`). That is wrong. A scan of `src/` (comments
excluded, the fuzz files excluded) for constants in `0x401000..0x5C4000`
that are not `[[func]]` starts - 526 lines, less map coordinates (values with
the low 15 bits clear) and harness ranges - and of every use of the names
they define, found our implementations reading Capcom's code in about forty
places, in four kinds:

| Kind | Where | What they read |
|---|---|---|
| A patched operand, read back | `battle_e7`, `menu_draw_helpers`, `menu_lists`, `rest_2f`, `rest_2g`, `rest_2h` (DIV-0041's slide bounds); `rest_3g` (DIV-0062's pool bound and array, DIV-0041's cull bounds); `field_c1`, `rest_4f` (DIV-0017 / 0026 / 0051's config widths, anchors, frame sizes, the second-cell skip); `save_menu`, `title_menu` (DIV-0014's widths); `field_o` (DIV-0029's inset); `yes_no_layout` (DIV-0027's stops); `cheats` (DIV-0046's masks); `draw_pool` | the operand bytes, to draw or bound as the patched code would |
| A re-aimed call site, read for its target | `rest_2d`, `rest_2f`, `rest_2h`, `rest_4f`, `field_s`, `field_c1`, `menu_verbs`, `yes_no_layout` | which frame or text draw (Capcom's or the divergence's) to call |
| A table the compiler put in the code | `area_w4a` (33 bytes), `d3d_list` (0xD1 and 0xD5), `magic_s02` (0x99), `magic_s20` (0xA3), `field_blocked` (0x100 + a 3-entry jump table) | a jump table's byte index, read as data |
| A verification | `char_names` (New Game's operands), `labels` (a push operand or a `.data` table) | that the code reads what we expect |

Read in the exe host, every one of them would have got `0xCC`: widths of 204,
bounds of -13108, calls to `0xCCCCCCCC`. The first exe-host self-test showed
it at once (`FATAL: field_blocked: cell table entry 0 is 204`).

**The rule as built** (the brief's "a variable of ours", in one place rather
than forty): `src/hook/code_bytes.h`. `CodeBytes(va, n)` is `va` itself
hosted. In the exe host it is a **store of ours**: a reserved region, pages
committed on use, the requested bytes read from the player's `BOF3.exe`
(the file `Engine_Main` copied `.rdata` and `.data` from, already
SHA-256-verified) on first read, held as data - never at `0x401000..`, never
executable. `PatchBytes` and `RetargetCall` into Capcom's code range write
the store instead of the code (checking `expected` against the file's
bytes, as hosted they check the code), so every reader sees exactly what it
sees hosted. The readers call `CodeBytes` / `CodeByte` / `CodeWord` /
`CodeLong` / `CodeCallTarget`; no behaviour changes in either host. The
exe-host self-test's store after `InjectAll`: 43 reads, 450 bytes (the
config text's sites and the title widths join it when FIRST.DAT and
START.DAT load). **The owner, 2026-10-11: the store stays as it is** until
the disc-only round replaces it - that round makes each operand a value of
ours because a disc has no PC operands to read, which is the brief's
"variable of ours" arrived at from the other side.

**The inventory's self-test** (`CheckStub`, after `InjectAll`): every byte
of `.stub` is `int3` but the 10,087 jmps and the thunk's six; a write of
anything else, a site that reached `.stub` by any path, is a Fatal naming
the first address. Then `.stub` goes read-execute, so a later write faults.
The brief's "PatchBytes into `.stub` is a Fatal" is subsumed: nothing of
ours can reach `.stub` but `Inject`. The brief's "dead in the exe host" was
not used for any site: a re-aimed call is read by a screen of ours (which
frame a menu draws), so even the `RetargetCall`s carry state.

What the scan cannot see: a read through a pointer computed at run time
(nothing found of that shape) and the fifteen software-renderer handlers
`d3d_list.cpp`'s table names by address (`0x5A3A60`..`0x5A4C40`, not
`[[func]]` starts): reached only when `Gfx_RenderFlags` selects the software
path, they land on `int3` in the exe host - a `CRASH` line, rule 4's loud
abort, not a silent fork. Not run.

**The store is gone (2026-10-12, the disc-only round):** each of these reads
is a value of ours now, owned by the divergence that patches or re-aims the
site or by our reimplementation of the function whose table it was;
`PatchBytes` / `RetargetCall` write nothing in the exe host and answer
whether they went in; `code_bytes.*` is a hosted check that compares every
value with the patched code (77 values, 1,310 bytes). The sites, their values
and the check: [`operand-store.md`](operand-store.md).

### 2.4 The launcher and the code page

`bof3x-launcher` starts `bof3x.exe` by default (beside itself, the game
directory as current directory, no suspension, no reservation, no remote
thread); `--host bof3` keeps the hosted start; **with no `--host`, a
hosted-only switch in the environment picks the hosted build** and says so
on stderr - an addition to the brief, so that every documented command
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' build/bof3x-launcher.exe ...`, the
`--original` A/Bs) keeps its meaning. `tools/attract_run.py` and
`input_run.py` pass `--host` (bof3 with `--original`, else exe; `--host`
overrides).

The cache path's UTF-8 half: `Config::cache` is UTF-8 in the ini and in
memory (`ConfigCacheFromIni`: valid UTF-8 kept, other bytes - an older ini
the ANSI code page wrote - read once as that code page); it reaches the game
through `SetEnvironmentVariableW`; `ToAnsi` and its refusal are gone; the
dialog's 226 counts UTF-8 bytes, as the DLL counts them.

### 2.5 The hosted build's fuzz: a layout coincidence fixed

`BOF3X_SHADOW='*'` on this branch's first build failed in `battle_sprites`
(204 mismatches in `Battle_OpenEnemyNames`), where the base `5a447d87`
passes (built from `git archive` in the scratchpad, same machine: 0
mismatches). Not this work's semantics: the function's eight-kinds walk
reads its own return address as list entry 8, the high word of it as that
entry's count, and the harness's two passes, unrolled by the compiler, had
two call sites - in this build `0x1019FFD1` and `0x101A00E8`, either side of
a 64 KB line. Calling both sides through one `noinline` helper fixed that and
exposed the same dependence in `Sprite_SetTint`, which reads a fifth
argument the harness never pushed; the helper now pushes a fifth dword
derived from the round's arguments, the same for both sides.

## 3. The checks

Every self-test ran from a fresh directory under `build/` holding only
`bof3x-launcher.exe`, `bof3x.dll` and `bof3x.exe` (no `bof3x.ini`), with
`--game C:\Users\kerok\Documents\GitHub\BreathOfFire3PCPort\bof3 --no-config`.

| Check | Command (from the test directory) | Exit | Result |
|---|---|--:|---|
| exe host, self-test | `BOF3X_SELFTEST_ONLY=1 bof3x-launcher.exe --game <bof3> --no-config` | 0 | `inject: 10087 ours`; inventory zero; store 43 reads / 450 bytes |
| exe host, run directly | `cd <bof3> && BOF3X_SELFTEST_ONLY=1 <dir>/bof3x.exe` | 0 | the same lines |
| exe host refuses `BOF3X_SHADOW=*` | `... --host exe` | 3 | `FATAL: BOF3X_SHADOW is set: ...` |
| exe host refuses `BOF3X_ORIGINAL=Game_WinMain` | `... --host exe` | 3 | `FATAL: BOF3X_ORIGINAL is set: ...` |
| exe host refuses `BOF3X_MP3=original` | `... --host exe` | 3 | `FATAL: BOF3X_MP3=original: ...` |
| exe host refuses `BOF3X_CALLTRACE=x.txt` | `... --host exe` | 3 | `FATAL: BOF3X_CALLTRACE is set: ...` |
| no `--host`, `BOF3X_ORIGINAL` set | `BOF3X_SELFTEST_ONLY=1 BOF3X_ORIGINAL=Game_WinMain bof3x-launcher.exe ...` | 0 | the stderr note; `host: BOF3.exe ... (hosted)` |
| hosted, `battle_sprites` fuzz | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=battle_sprites bof3x-launcher.exe ... --host bof3` | 0 | `32 functions, 93600 rounds, ... 0 MISMATCHES` |
| a UTF-8 cache folder (`缓存Ω-cache`), exe host | `bof3x.ini` `cache=` as UTF-8; self-test, `--host exe` | 0 | `DIV-0089 ...\缓存Ω-cache has no manifest.toml: ...` - opened |
| the same folder, hosted | the same ini, `--host bof3` | 3 | `FATAL: DIV-0089: BOF3X_CACHE=...\??O-cache is not a directory` (ACP 1252: as before state 3) |
| an older ini in the ANSI code page (`café-cache`, cp1252 bytes) | self-test, exe host | 0 | the folder named correctly in the log: read once as the code page |
| the build's own verification | `cmake --build build` | 0 | `make_exe: ... verified: base 0x400000, 6 sections ...` |
| CI's import rule, run locally on the three binaries | the workflow's `llvm-readobj` pipeline | - | `bof3x.exe`: `bof3x.dll kernel32.dll`; the DLL and launcher import no `bof3x`, `lib*` or `SDL3` DLL |
| hosted, the whole fuzz, first run | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' bof3x-launcher.exe ... --host bof3` | 3 | `FATAL: the battle sprite functions differ ... in 204 self-test rounds` - section 2.5; the base passes the same fuzz (exit 0) |
| hosted, the whole fuzz, after 2.5's fix | the same | (124) | the harness's own `timeout 900` ended the launcher at 15 minutes; the game process ran on and its log ends `inject: 10087 ours` ... `self-test only: done, ending the process` (the path that exits 0), no `FATAL`, no `MISMATCH` among 1,051 summary lines. The exit code: the next row |
| **hosted, the whole fuzz, untimed** | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' bof3x-launcher.exe --game <bof3> --no-config` (no `--host`: the launcher picked the hosted build and said so) | **0** | `game process exit code 0`; 1,068 summary lines, 0 with a mismatch |

### 3.1 The attract sequence on the state hash

`BOF3X_STATEHASH=<run>.sh BOF3X_STATEHASH_SKIP=tools/statehash_skip.txt
BOF3X_OPT=none python tools/attract_run.py --out <run>.tsv --minutes 6
--no-front --launcher build/st_attract/bof3x-launcher.exe --host exe|bof3`
(the skip list's 168 ranges, as the references; the outputs in the build's
scratchpad, not committed), checked with `tools/statehash.py check
analysis/statehash/attract_r14_orig.sh attract_r14_origb.sh <run>.sh` (the
main checkout's references, read only):

| Run | Exit | Ticks | Against `attract_r14_orig` / `_origb` | Against the hosted run of this build |
|---|--:|--:|---|---|
| hosted (`--host bof3`), this build | 0 | 10,318 frames in 360 s | **identical on all 10,305 compared ticks** (check exit 0) | - |
| **exe host** (`--host exe`), this build | 0 | 10,789 frames in 360 s; 11,201 records | 2 pages differ on every tick from 0 (check exit 1): `0x93C000`, `0x93D000` | the same 2 pages, every tick (`statehash.py diff`); every other page identical over 10,305 ticks |

The two pages, by `BOF3X_STATEHASH_DUMP=0,5000` on both sides and
`statehash.py bytes`: every differing byte on them is a variable of Capcom's
C runtime that its start-up writes and the game never reads - the
multibyte tables (`0x93C35C`, `0x93C3C1..0x93C580`), `__pioinfo` `0x93C5A0`,
`_nhandle` `0x93C6A0`, `0x93C6A4`, `0x93C6A8`, `__piob` `0x93C6B4`,
`_nstream` `0x93D6C1`, `_crtheap` / `__active_heap` `0x93D6E2`, `_acmdln`
`0x93D6E8` ([`crt-startup.md`](crt-startup.md) 9, the rows "drop") - zero
in the exe host, as the brief decided, and the same bytes at tick 5,000 as
at tick 0 on both sides. The runtime's other start-up words (the TLS index
`0x672950`, `_iob`, the locks, the version words, `_pgmptr`'s string) differ
too in the dumps but lie in the skip list's ranges. **So the exe host plays
the attract sequence as the hosted build and as Capcom's code do, but for
the runtime's start-up data it no longer writes**. **Added to the skip list
the same day** (`tools/statehash_skip.txt`, by variable, not by page - the
first page also holds the enemy records and the battle message ring): the
multibyte code-page word and its two tables, `_nhandle` with the environment
and code-page words beside it, `_nstream`; `__pioinfo`, `__piob` and the heap
and command-line words were in the list already as noise. The differing
ranges at tick 0 and at tick 5000 are the same (`statehash.py bytes` on the
dumps, 2026-10-11); `Sound_Channels` went in with them
([`state-hash.md`](state-hash.md) 7 item 4). The check across the cutover
is owed a run with the new list - the attract sequence in the exe host
against `attract_r14_orig` / `_origb` - in the next away batch.

The exe-host run took more logic frames in the same 360 s (10,789 against
10,318); the hash aligns by tick, so the count does not enter the
comparison. Not measured why: whether the start-up is faster, or the
frame pacing differs, is a question for the routes.

`tools/attract_watch.py`'s `rand_n` column is empty in the exe host
(`holdrand @ None`): it finds `holdrand` through Capcom's runtime's TLS index
at `0x672950`, which no start-up writes now. The state hash does not use it.

### 3.2 Not run

- The ten routes against `cn_<route>_orig2` / `_orig2b`, and under `en-US`
  against `en_<route>_sh3a` / `_sh3b` (the brief's section 11 order): not
  run - time.
- `BOF3X_STACKWATCH=1` on the F9 quit route in the exe host: not run. The
  self-test's start line shows the exe host's stack (reserve `0x100000`,
  commit `0x10000`, the main thread `Engine_Main`'s).
- `BOF3X_EXEIMAGE` as the source in the exe host: built, not run (no cache
  in this worktree; the main checkout's was not used).
- The dialog (a window): the UTF-8 half was checked through `bof3x.ini`
  only.
- Anything the owner sees: the window, the FMVs, the sound, the icon - the
  attract run was unattended and `--no-front`.

## 4. What section 11 leaves

The routes, the stack watch, the cache as the `.data` source and the attract
check with the skip list's new entries ran in the away batch (section 5).
Owed from it: the four Chinese route pairs DIV-0092 made stale (5.4 item 1),
finding 2's pointer words (the owner's call on the hash), finding 3's cause
(a build difference since `105a0c77` under `en-US`) and what 5.5 lists.
Then the owner plays it. Beyond the brief:
the fifteen software-renderer handlers `d3d_list.cpp` names without a
`[[func]]` (int3 in the exe host); `attract_watch.py`'s rand count; the
owner's icon - since generated, not supplied (below); `dpiAware` - declared,
the owner's call of 2026-10-11 (section 2.1); the operand store - kept until
the disc-only round, the owner's call the same day.

**The icon is generated, not supplied** (DIV-0095, [`exe-icon.md`](exe-icon.md),
the owner's decision of 2026-10-11, after this build): the placeholder
`make_exe.py` writes stays in the built `bof3x.exe`, and on the player's
machine the engine replaces it - the window's icon as soon as the load
screen or the field menu has uploaded the whelp's sheet and CLUT, the file's
through `bof3x.exe.new`, which the launcher moves into place at its next
start. The build's verification above still checks the placeholder's three
resources; the rewritten file is checked by the engine itself.

## 5. The away batch (2026-10-11)

The owner was away. One game at a time, every run `--no-front`, from an
ini-less copy of `build/` (`build/away/launch/`: launcher, DLL, exe), the
player's game `--game C:/Users/kerok/Documents/GitHub/BreathOfFire3PCPort/bof3`,
the outputs in `build/away/` (gitignored, not committed), the references the
main checkout's `analysis/statehash/` (read only). Every run:
`BOF3X_STATEHASH=<run>.sh BOF3X_STATEHASH_SKIP=tools/statehash_skip.txt
BOF3X_OPT=none`. The attract runs: `python tools/attract_run.py --out
<run>.tsv --minutes 6 --no-front --host exe|bof3 --launcher <copy>`. The
routes: `python tools/input_run.py tools/recipes/<route>.txt --out <run>
--minutes <ticks/3600 + 6> --no-front --host exe|bof3 --launcher <copy>
[--lang en-US] --env BOF3X_STATEHASH=... --env BOF3X_STATEHASH_SKIP=...
--env BOF3X_OPT=none --env BOF3X_LAYERING=0` (the shop also `--slot0-hold`).
No run crashed (no `CRASH` or `FATAL` line in any output), none hung, none
was killed; every route ended `recipe done` and `end: done`, runner exit 0.
Defender left the launcher alone.

### 5.1 Three ways the brief did not hold

1. **The stock `check` refused every comparison.** This branch's skip list
   has **173** ranges (hash `2C491AB2`): `87ec7d9a` added the runtime's
   start-up words and `Sound_Channels`. Every reference - `attract_r14_*`,
   `cn_<route>_orig2*`, `en_<route>_sh3*` - was hashed under **168**
   (`13375623`), and `statehash.py check` exits 1 on each (`skips is 173,
   ... has 168 - the runs were not hashed alike ...; record a new pair`). A
   page's hash depends on the list only where a range lies on it, so the
   added ranges leave every page but three hashing alike under both lists:
   `0x6BC000` (`Sound_Channels`), `0x93C000` and `0x93D000` (the runtime's
   words). The checks below are the stock `check`'s logic with those three
   pages left out (a scratch `xcheck.py` in the session's scratchpad, not
   committed). For the three pages,
   the same tree was run **hosted** (`--host bof3`) under the 173-range list
   and diffed against the exe host with the stock `statehash.py diff` - the
   cutover's own question, page for page. **Since the same day the stock
   tool does this** (5.6).
2. **`input_run.py` does not pin `BOF3X_LAYERING=0`.** `attract_run.py` does,
   and the pairs were recorded with it (DIV-0071 is on by default), so every
   route here passes it with `--env`. The shop route, as its pairs, ran with
   `--slot0-hold`.
3. **Five logs were lost** to a bug in the batch's own script: its
   wait-for-exit loop overwrote the run-name variable, so the log copy went
   to `build/away/.log` and was overwritten. Lost: the first attract run's
   and the `cn_` combat, menu_screens, field_menu and
   worldMapAndAreaTransition logs (caughFish's was recovered). Their `.sh`
   and `.out` files are whole; nothing below rests on those logs.

### 5.2 The results

"Against the pair" is the check of 5.1 item 1 on 865 pages; "exe/hosted" is
`statehash.py diff <run>_hosted.sh <run>_exe.sh`, all 868 pages under 173
ranges. The tick-3 pages (`0x7BB000..0x7BF000` the VRAM shadow, `0x7DE000`,
`0x937000`) are the noise the attract pair itself shows at tick 3 (an upload
landing a frame later).

| Run | Exit | Ticks | Against the pair (865 pages) | Exe/hosted (868 pages) |
|---|--:|--:|---|---|
| attract, exe host | 0 | 10,787 frames in 360 s | `attract_r14_orig` / `_origb`: identical on all 10,305 ticks | identical on all 10,305 ticks (hosted: 10,317 frames) |
| attract, exe host, `BOF3X_EXEIMAGE=<main>/analysis/cache/pc-plus-us` | 0 | 10,788 frames | identical on all 10,305 ticks | identical to the plain exe run on all 11,137 ticks |
| cn combat | 0 | 2,561 | `cn_combat_orig2` / `_orig2b`: `0x7DE000` at tick 227 only (noise) | tick-3 noise; `0x7DE000` at 227 |
| cn menu_screens | 0 | 1,729 | music pages from 752 (finding 1); tick-3 noise | tick-3 noise only |
| cn field_menu | 0 | 1,409 | music pages from 752 (finding 1) | identical |
| cn worldMapAndAreaTransition | 0 | 2,113 | identical | identical |
| cn caughFish | 0 | 3,841 | identical (`0x6BC000` noise in the pair) | identical |
| cn masterAndManillo | 0 | 5,505 | identical | identical |
| cn dragonTransform | 0 | 4,289 | music pages from 332 (finding 1) | identical |
| cn cutsceneAndNue | 0 | 7,361 | music pages from 204 (finding 1) | identical |
| cn shop | 0 | 3,137 | identical | identical |
| cn whelpBoss | 0 | 13,121 | identical (`0x6BC000` noise in the pair) | tick-3 noise only, over 13,185 ticks |
| en combat | 0 | 2,561 | `en_combat_sh3a` / `_sh3b`: identical | identical |
| en menu_screens | 0 | 1,729 | identical | identical |
| en field_menu | 0 | 1,409 | identical | identical |
| en worldMapAndAreaTransition | 0 | 2,113 | identical | identical |
| en caughFish | 0 | 3,841 | `0x7E1000` from 832 (finding 2) | the same (finding 2) |
| en masterAndManillo | 0 | 5,505 | `0x7E1000` from 715 (finding 2) | the same (finding 2) |
| en dragonTransform | 0 | 4,289 | `0x93B000` 2474..3162, 331 ticks (finding 2) | the same (finding 2) |
| en cutsceneAndNue | 0 | 7,361 | `0x93B000` from 5506 (finding 2); `0x905000` from 6985, `0x7E0000` and `0x929000` 6986..7023 (finding 3) | tick-3 noise; `0x93B000` from 5506 |
| en shop | 0 | 3,137 | `0x7DE000` from 565 (finding 2) | the same (finding 2) |
| en whelpBoss | 0 | 13,121 | `0x93B000` from 1002 (finding 2) | the same (finding 2) |
| F9 quit, `BOF3X_STACKWATCH=1` | 0 | recipe frame 404 | - | 5.3 |

The hosted en runs differ from the en pairs on the same pages as the exe
runs. The cache run's log: `BOF3X_EXEIMAGE: .data at 0x5DA000 from
base/exe/data.bin (pc-zh), 638976 bytes: the source in the exe host`. The
route wall times are the same in both hosts to 0.5 s (the runners'
`analysis/run_times.tsv` - the worktree's, their `ROOT`); the attract's
360 s again held more logic frames in the exe host (10,787 against 10,317),
as in 3.1.

### 5.3 The stack watch on the F9 route, beside [`crt-startup.md`](crt-startup.md) 11.4

`input_run.py tools/recipes/f9ExitMode.txt ... --host exe --env
BOF3X_STACKWATCH=1`; the log's `stack:` and `stackwatch` lines:

| | exe host (this batch) | hosted (11.4) |
|---|---|---|
| the stack | reserve `0x100000`, commit `0x10000`; base `0x1B0000`, low `0xB0000` | reservation `0x40000`, base `0xF0000`, commit `0x10000` |
| `Engine_Main`'s `esp` | `0x508` below base, x87 `0x027F` | - |
| `Task_SetStackBase`'s arrival `esp` | `0x314` below base, twice, the same | `0x294` below base, twice |
| the slices' bottom | `0x14314` below base, inside | `0x14294` below base |
| committed at exit | `0x1A0000..0x1B0000` = `0x10000`, unchanged; guard `0x2000` | `0x10000`, unchanged |
| deepest non-zero dword | **`0xD5F8`** below base (`0x2A08` to spare) | `0xD578` |
| slices 0 / 1 / 2 / 3 used | `0x2398` / `0x1354` / `0x12E4` (partly committed) / 0 (reserved) | `0x2B14` / `0x1334` / `0x12E4` / 0 |

In the exe host the armed check's call runs on the main thread (no other
thread exists then); `stackwatch.cpp` already keeps that call from taking the
slices' copy. 11.3's rule stands under the exe host's header: commit
`0x10000`, the deepest use `0xD5F8`.

### 5.4 Findings

1. **The music pages on four Chinese routes are DIV-0092, not the exe
   host.** `0x904000` (`Music_Track`), `0x665000` (`Music_LoadedTrack`),
   `0x6BD000` (`Music_File*`) and `0x7DE000` differ from `cn_*_orig2` from the
   route's save load on (menu_screens and field_menu 752, dragonTransform 332,
   cutsceneAndNue 204), in the exe host **and** in the hosted build of this
   tree (`cn_<route>_hosted`: the same pages and ticks). The hosted run with
   `--original AreaTrackFallback` (DIV-0092's switch) is identical to the pair
   on all 1,409 ticks of field_menu and all 4,289 of dragonTransform
   (`o_cn_*_hosted`). The pairs were recorded 2026-10-10 afternoon, DIV-0092
   that night. So these four pairs want recording again, or their check run
   with `BOF3X_ORIGINAL=AreaTrackFallback`.
2. **Pointers of ours hashed raw (state-hash.md 7, point 5's class).** The
   DLL loaded at `0x5C800000` in the exe host and at `0x5C5F0000` hosted (each
   log's `statehash   our image` line), so a word holding a pointer into our
   text that is not one of the census's slots differs between the hosts, and
   between the pair's launches and ours. Dumps of both hosts
   (`BOF3X_STATEHASH_DUMP`, `statehash.py bytes`, runs `d_en_*`) show exactly
   these words on the reported pages, each at the same offset into our image
   on both sides:
   - **`Effect_Objects` records 7, 8 and 9, `+0x50` / `+0x54`** (`0x7E15B0`,
     `0x7E1630`, `0x7E16B0`; caughFish ticks 900 and 2000): `0x5D96689D`
     against `0x5D75689D`, both our base plus `0x116689D` - a text pointer.
   - **`0x93B908`**, a second word of the battle result's message beside the
     run-time slot `0x93B8E4` (dragonTransform tick 2500: `0x5D950756`
     against `0x5D740756`). At tick 3000 the word that differs raw is
     `0x93B8E4`, which is normalised, and that tick is identical.
   - **`MsgBoxState` `+0xC`, `+0x10`, `+0x14`** (`0x7DEE4C..0x7DEE57`: the
     box's string and stepper pointers; shop ticks 600 and 2000). Nothing
     else on `0x7DE000` outside the skip list differs.

   Not a difference of the game. They are candidates for the run-time slot
   list of state-hash.md 7 (measured 3) - a change of the hash, the owner's
   call.
3. **en-US cutsceneAndNue, `0x905000` / `0x7E0000` / `0x929000` from tick
   6985: a difference between this build and the pairs' (`105a0c77`), not
   finding 2's class; its cause not found.** The same in both hosts (absent
   from exe/hosted), absent on the Chinese route. A third exe run with
   `BOF3X_STATEHASH_DUMP=6984,6990,7100` (`d_en_cutsceneAndNue_exe`) hashes
   like the first but for `0x7DE000` at one tick (5037), so the run is
   repeatable. On its dumps, the three pages hold **no aligned word inside a
   section of ours a pointer can name** (`0x5C801000..0x5FEA9010`, the log's
   `statehash   our image` line) at any of the three ticks: the one aligned
   word inside our image, `0x905C68` = `0x616E694E`, is the text "Nina" and
   lies past those sections; the other values in range are unaligned byte
   data (`MapView_Cells`, `MoveScript_TintRecords`, `Sprite_OverlayList`).
   What changes on the pages between 6984 and 6990 is draw state
   (`statehash.py bytes` on the two dumps: `Gfx_PacketNext` `0x7E0670`,
   `Draw_SortOnX` `0x905BC5..0x905BD1`, `Gfx_OtPointers` `0x929EA4`,
   `Gfx_UnpackNext` `0x929F02`). The pair's own game log
   (`analysis/statehash/en_cutsceneAndNue_sh3a.log`) and this run's agree
   line for line on every `randlog`, `input`, `save` and file-open line (7,503
   lines each, the paths aside), so the same frames, the same random draws,
   the same files - the BGM order 141N, 027, 029, 155, 165, 029 included. Not
   the icon: DIV-0095 generated it at `Frame_Counter` 95. No dump of the pair's
   build exists, so `statehash.py bytes` across the builds was not possible;
   the next step is one run of a `105a0c77` build with the same dump ticks,
   or a bisection over the 33 commits since it (57 files under `src/`,
   among them the `CodeBytes` reads in `src/game` and DIV-0093). Not fixed
   here. **Narrowed the same evening** (the main checkout's launcher, ini-less
   copies, `--lang en-US --no-front --env BOF3X_OPT=none --env
   BOF3X_LAYERING=0`, the stock `check` against `en_cutsceneAndNue_sh3a` /
   `_sh3b`): **`main`'s own build (`a5946f64`, hosted) differs the same way** -
   `0x905000` from 6985, `0x7E0000` and `0x929000` 6986..7023, plus
   `0x93B000` from 5506 (finding 2's word) - so it is not state 3's; and
   **the commit before DIV-0093 (`c0d099d2`, built in a scratch worktree)
   differs the same way too**, so DIV-0093 is not the cause. Of the eight
   commits since `105a0c77` only one other touches code that runs in play:
   `7d162e4f`, the input script's keyboard keys and quit step, one line in
   `Game_WinMain`'s loop (`InputScript_PausedLatch`) and `input_script.cpp`.
   **The pairs' own commit `105a0c77`, built and run the same way, differs
   the same way too** - so the code is cleared entirely. Then the inputs,
   each checked: the install's tagged overlays were all rewritten at 17:09
   that day (by no agent of this session; another session or the owner),
   but all 247 `en-US` files are byte-identical to the cache's layer
   (`analysis/cache/pc-plus-us/loc/en-US`) the morning's install also came
   from; the base `DAT/` files, `BOF3.CFG` and every save slot are older than
   the pairs; the recipe, its save `nue.DAT`, the shot handling (five shots,
   all frozen, at the same frames, `recipe done at 7421` on both) and the
   runners are unchanged; the morning's launcher directory was ini-less like
   today's, with no `bof3x.window`; every `DIV-` and path line of the two
   game logs is identical but the load-address census. What changes at tick
   6985 in today's dumps (6984 against 6990) is the fight's own state -
   `Draw_SortOnX+0x1D..+0x2A` ("the battle bytes", [`mode-rest.md`](mode-rest.md)
   section 272's row), `Gfx_PacketNext`, `Gfx_OtPointers`, `Gfx_UnpackNext`
   - with the Rand log and the inputs identical to the pairs' throughout.
   **What is left is something the game reads from the machine**, stable
   within a morning and within an evening but not between them: wall-clock
   pacing or the display's state (the pairs were recorded with the owner at
   the desk; every run of the evening was unattended with the screen
   asleep). It is not a state 3 matter - the exe host and the hosted build
   agree on this route tick for tick - but it is a reference-stability
   matter for the route. **The experiment that settles it:** this route once
   with the owner at the machine, or the pair re-recorded now and checked
   against the evening's runs; and a read of what in the Nue fight's state
   at tick 6985 could depend on the clock (`Game_Clock`, DIV-0022, or the
   render count). **The same evening, the owner at the desk:** the route run
   again on `main`'s build differs at the same ticks on the same pages - so
   attendance and the display are cleared too - and the five captures of
   that run are pixel-identical to the pair's (`sunset_tint` 601,
   `night_house` 1501, `dialogue` 2401, `equip_window` 4501, `nue_attack`
   6001), so the overlay's text and the route's course are the same to
   frame 6001. The owner says the 17:09 overlay rewrite was theirs (a
   rebuild while testing the jukebox branch; the bytes equal the cache's).
   **Where this stands:** every run since the afternoon agrees with every
   other, the morning's two agree with each other, and nothing named here
   differs between the two days. The pages hold draw state (`Gfx_BufferIndex`,
   `Gfx_PacketNext`, the ordering-table pointers) and the fight's bytes;
   the loop renders when early and skips when late, so a render skipped at
   a deterministic tick - a long frame at the fight's start, say, that the
   file cache's state makes longer or shorter - would show exactly this
   shape on pages the hash sees. A hypothesis, not a measurement. **Open:**
   a read of what the fight's frames at 6985..7023 do that the render count
   or `Game_Clock` (DIV-0022) can reach; and the pair re-recorded, the
   morning's kept beside it as `_old`, so the route has a reference that
   matches the machine as it is. Not state 3's: the exe host and the hosted
   build agree tick for tick. **Two more exclusions the same night:** the
   track is the same (the owner's question - the pages that name the track
   and the file position, `0x904000`, `0x665000`, `0x6BD000`, do not differ
   under `en-US`; only the buffer page `0x7DE000`, always noise); and the
   game logs of the pair and of tonight's run are **identical line for line**
   (17,781 lines each, volatile numbers stripped), so nothing the engine
   logs - loads, injects, patches, pad, music path, window - differs. By the
   recipe, the fight ends and the walk begins about tick 6660 (`hold right`,
   `up+right 71` ... after the last `circle`), so the difference at 6985 is
   some 300 ticks into that walk, where the pages' `Field_StepCount`,
   `CameraTurn_*`, `MapView_Origin` and `Camera_Angles` say a map edge or a
   camera turn is reached - not the battle's end and not the music resume.
   **For the re-recorded pair:** record it with `BOF3X_STATEHASH_DUMP=6984,6990,7100`
   so the next shift, if one comes, can be read byte for byte against a
   reference dump; tonight's dumps of ticks 6984 / 6990 / 7100 are in the
   build's `away/`.

### 5.5 Not done yet

- Finding 3's cause: code, data, saves and settings cleared (5.4); a run with the owner present, or the pair re-recorded, decides between the clock and the display.
- The four Chinese pairs of finding 1 and finding 2's words: the owner's.
- Anything seen or heard: the batch was unattended.

### 5.6 `statehash.py` compares a list that only adds ranges

`tools/statehash.py`'s `check` and `diff` now compare a run whose skip list
adds ranges to the references' on every page no added range touches. They
name the pages they leave out and refuse every other difference of lists, or
any difference under `--strict` ([`state-hash.md`](state-hash.md) section 4).
`python tools/statehash.py check analysis/statehash/attract_r14_orig.sh
attract_r14_origb.sh build/away/attract_exe.sh`: the note names `0x6BC000`
(`0x6BC8C8 92`), `0x93C000` (`0x93C35C 4`, `0x93C380 516`, `0x93C6A0 12`)
and `0x93D000` (`0x93D6C0 4`); then `identical on all 10305 compared ticks`,
**exit 0**. With `--strict`, exit 1. On synthetic runs (the lists through
`--skip-list`), a range removed, a length changed and a NEW list narrower than
the references' are each refused. The same `check` on `cn_combat_exe` reports
`0x7DE000` at tick 227 alone, as 5.2 has it.
