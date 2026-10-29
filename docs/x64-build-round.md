# The 64-bit build round: staging (PLAN phase 4, I44 "What follows")

**Status:** STAGED (2026-10-29; not launched. Needs the relocatable-image
round on `main` first ([`relocatable-image-round.md`](relocatable-image-round.md),
built through stage D on `relocatable-image/round` the same morning). The
owner's calls in section 7 come first; stage A needs none of them.)

The relocatable-image round ends with the source free of Capcom's addresses:
every data place is `Img(va)`, every code word is bound by name, the
executable has no `/FIXED` base and no entry-stub page. Its section 9 names
what it leaves: "widening the records' pointers: the 64-bit build (PLAN
phase 4, I33's platform library)". This round is that build: `bof3x.exe`
as an x86-64 Windows executable, the same source, the game's behaviour
unchanged, proved by the state hash against the 32-bit references.

Section 1 argues that the "widening" I44 foresaw is not the cheap road and
need not be taken at all: the image's words can stay 32 bits wide inside a
64-bit process. That is the owner's call 1.

## 0. What pins the build to 32 bits today, measured 2026-10-29

On `relocatable-image/round` `41372e3c` (the source after stage D; the branch tip at check-in, `0f0e0bea`, changes no counted file) and on
`main` `628aa521` where noted. Fuzz files (63 of the 126 game files that
hold asm) are counted apart: the shadow fuzz runs in the hosted build only,
which stays 32-bit for ever (section 2).

| What | Count | Source |
|---|---|---|
| The toolchain's guard | `CMakeLists.txt:16` refuses any `CMAKE_SIZEOF_VOID_P` but 4; one preset, `i686`; CI builds `i686` alone (`build.yml:33`) | the files |
| Toolchains on this machine | `x86_64-w64-mingw32-clang++` and `aarch64-w64-mingw32-clang++` sit beside the `i686` driver in the same llvm-mingw (`~/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin`) | `which` |
| Inline asm in live code (`src/`, fuzz files apart) | **174 blocks in 70 files**: 125 x87 arithmetic (55 files hold nothing else: `d3d_draw` 8, `d3d_rest` 8, `effect_3d` 8, `effect_1a` 7, `effect_3c` 7 ...), 29 stack or control flow (15 files: `task_sched` 6, `sound_rest` 5, `stackwatch`, `mp3.cpp`'s decoder stack, `shell`, `field_e2`, the naked callees), 20 other (the x87 control word 9, `fs:` TEB reads 4 in `boss_harness`, `fs:0x30` in `ImageBase()`, one unaligned load, stackwatch's register probes); 6 asm-label aliases (`__asm__("_Music_Play")`, the eax-result functions) | a classifier over every `asm(` block by mnemonic, this doc's staging session |
| `__attribute__((naked))` | 69 in all, **40 in 16 live files** (`task_sched`, `sound_rest`, `shell`, `scena_calls`, `save_menu`, `mode_tasks`, `field_*`, `area_slope`, `battle_misc`, `magic_s28`, `map_field_objects`) | `grep` |
| A native address stored into a 32-bit slot | **382 sites** of `static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(...))`: 196 of a `p`, 23 of `g_prims`, 11 `f`, 5 `callee`, 3 `entry`, 3 `code` ...; 19 take a function | `grep` on the round branch |
| A 32-bit value used as a native address | 933 sites of `reinterpret_cast<T>(static_cast<std::uintptr_t>(...))`; 1,785 spellings of `std::uintptr_t`; `sizeof(void *)` twice | `grep` |
| `Img()` places | 14,976 (`VaOf()` 50): the one helper every data place goes through, `std::uint32_t` in and out (`src/common/image_base.h`), `ImageBase()` read from the PEB through `fs:0x30` | `grep`, the header |
| Code words | 8,784 `.text` words, 6,346 targets, bound by name through `Code(va)` since stage C2; 1,340 run-time calls through an address (1,284 through the call-through helpers) | [`relocatable-image-round.md`](relocatable-image-round.md) 0 and RA1 |
| `unsigned long` | 1,331 spellings in `src/game`; the generated ctypes: 1,745 `unsigned long`, 42 `long`, 1 `unsigned long *` of 3,167 `[[data]]` ctypes | `grep`, `symbols.toml` |
| The records | 43 structs in `src/game/records*.h`, **no pointer member**: a pointer field is a `std::uint32_t` slot holding a VA, 39 of them | `grep` |
| Allocation in `src/game` | 47 sites: 20 `malloc`, 8 `Crt_malloc`, 14 `VirtualAlloc`, 3 `Crt_calloc`, 2 `calloc` - each a native address the game may write into a 32-bit slot | `grep` |
| The task stacks | four slices of `0x4000` on the **process's main thread stack**, `Task_StackTop` a 32-bit slot at `0x66C858` holding the arriving `esp`; the switch is naked asm (`task_sched.cpp` 1..23); four slices need `0x14294` bytes below base on the 32-bit build | `symbols.toml:803`, [`quicksave.md`](quicksave.md) 1, [`state3-design.md`](state3-design.md) 5 |
| Where the packets may lie | below 16 MB: the ordering table's links are 24-bit (`d3d_list.cpp:209`), DIV-0062's pool asserts it; the image's `.pool` ends at base + `0x587000`, so the image's base is at most `0xA70000` | [`relocatable-image-design.md`](relocatable-image-design.md) 1.2 |
| The image span | `.data` at `0x5C4000`, `0x37B000` bytes; `.bss` zeroed to `0x93D6EC`; the `GameData` struct 2,965 members | [`relocatable-image-round.md`](relocatable-image-round.md) 0 |
| The Windows surface | `d3d11` in 2 files (`src/render`), `dinput` in 4 (3 `src/game`, 1 `src/input`), `mmsystem` in 2 (`src/hook`: MCI for the FMV, DIV-0035; the launcher); **SDL3 3.4.16 already fetched** for the pad (DIV-0050, `src/input/pad_sdl.cpp`, [`THIRD_PARTY.md`](THIRD_PARTY.md) 2) | `grep`, `CMakeLists.txt:80-94` |
| The FMV codec | `capcom.avi` shipped as Indeo 5, which no 64-bit Windows decodes; already re-encoded to Cinepak by the ledger's entry "Re-encode `capcom.avi` from Indeo 5 to Cinepak" ([`DIVERGENCE.md`](DIVERGENCE.md) line 279) | the ledger |
| **The low-memory probe** (a 64-bit program built with the `x86_64` driver, run on this machine) | `VirtualAlloc(0x400000, 0x600000)` succeeds; the window `0x10000..0x1000000` is **entirely free** before our block (`0xFF0000` of `0xFF0000`); 64 MB at `0x1000000` succeeds. The same program linked with `--image-base 0x400000` is moved by ASLR to `0x4E0000` and the block then fails (error 487): **our executable must stay high, the image is a block** | `tools/probes/lowmem_probe.cpp`, run 2026-10-29 |

Three consequences:

- **The game's words never need to widen.** No record holds a pointer; a
  pointer field is a 32-bit VA slot already. What the 64-bit build must do is
  keep every address the game can see representable in 32 bits: the image,
  the packets, the heap, the task stacks, and a token for each of our
  functions the game stores.
- **Most of the asm survives x86-64 as written.** x87 exists on x86-64 and
  the 125 arithmetic blocks use memory operands and `st`; they assemble under
  the `x86_64` driver unchanged (section 3's XA3 confirms this by building,
  not by assertion). The 29 stack and control blocks and the 40 naked
  functions do not: `esp`, `pushl`, the cdecl stack and `fs:` are 32-bit by
  nature.
- **The proof already exists.** With the image's words 32 bits wide, a page
  of the 64-bit build's image is byte for byte what the 32-bit build's is,
  up to the pointer normalisation RA2 built ([`state-hash.md`](state-hash.md)
  8). The `cn_<route>_orig4r` references compare directly.

## 1. The two roads, and the one to take (the owner's call 1)

| | **(N) The narrow image** (recommended) | (W) The wide image |
|---|---|---|
| The image's words | 32-bit, the layout BOF3.exe's, loaded as today | every pointer slot widened to 64 bits at load; `GameData` regenerated with new offsets |
| Where the image lies | a `VirtualAlloc` block at `0x400000` (design 1.1's option A; the probe above) | anywhere |
| The game's run-time pointers | must lie below 4 GB (packets below 16 MB): a low heap, low task stacks, a low thunk table for code tokens | native, anywhere |
| `Img(va)` | returns a native pointer; `VaOf()` narrows with a check | the table maps VA to a widened place; every record-offset site (9,047 `record` sites, the 27,435 `typed_left` raw sites) re-derived |
| The saves, the quicksave, the cache, the PSX-layout data files | unchanged: they hold the 32-bit layout, which is still the layout | every format translated; the quicksave's native span (DIV-0145..0147) redesigned |
| The state-hash references | compare directly, pointer slots as offsets | a new normalisation and new references; no 32-bit against 64-bit comparison |
| The hosted fuzz against Capcom's code | unchanged, in the 32-bit hosted build | unchanged |
| Lines changed | the loader, `image_base.h`, the 382 narrowing sites, the asm of section 0 | those, and every typed accessor's offset, every record struct, every tool that reads a save or a dump |
| What it does not give | records with native pointers in "reconstructed source" | - |

**Recommended: (N).** It is the build the proof can see. (W) is I44's
"same table applied once more" and it is not: the table rebases words, it
does not move every field behind them. Nothing the project wants from 64
bits (a modern toolchain's default target, more address space for our own
caches, the step toward the platform library) needs the game's own words
wider. If a later round wants native pointers in the records for
readability, it starts from (N) with the proof in hand.

(N) has one dependency on the operating system: a program may map memory
below 16 MB and below 4 GB. Measured true on Windows 11 here (section 0's
probe). On Linux `mmap_min_addr` is 64 KB by default; on macOS the default
`__PAGEZERO` covers the low 4 GB and the linker can shrink it. Neither is
this round's target (section 9); both are a one-line probe when they are.

## 2. The method: one source, two builds, the 32-bit build the oracle

The typed-data and relocatable-image rounds proved their spellings with
`tools/codegen_same.py` (machine code identical) and their moves with the
state hash. Both gates stay, and a third joins:

1. **Every source change lands in the `i686` build first** and is judged
   there. A pure spelling (a narrowing helper in place of a cast, a
   `std::uint32_t` in place of an `unsigned long`) keeps `codegen_same.py` at
   `0 differ`. An asm block rewritten as C++ changes the machine code, so it
   is proved the way every takeover was: the **hosted fuzz against Capcom's
   function** (the 63 fuzz files, `'*'` narrow and wide), which exists only
   in the 32-bit hosted build - the reason the hosted build keeps its
   base and width for ever.
2. **The `x86_64` build compiles the same source.** What differs by width
   is behind `sizeof(void *)` or the preprocessor in a handful of headers
   (`image_base.h`, `task_sched`'s switch, the stackwatch), never in a game
   file. A game file that needs `#if` by width is a defect of the design,
   listed in the stage D audit.
3. **The 64-bit executable is proved by the state hash**: the attract
   sequence and the ten routes, the `x86_64` exe host against the
   `cn_<route>_orig4r` references, identical pair for pair; the self-tests
   (`'*'`, `en-US`) headless on it; and the Rand counts the references'.

The order follows from this: stage B's rewrites are 32-bit work with the
old gates, and only stage C builds and runs the 64-bit executable. Stage B
is therefore safe to merge to `main` before any 64-bit binary exists.

## 3. Stage A: census, design, the build (four groups, in parallel, no `src/game` change)

### XA1: the census of what assumes a width
`tools/width_census.py`, written to run in CI like `reloc_census.py`: every
site in `src/` that assumes a pointer is 32 bits, by class -

- a native address narrowed into a 32-bit slot (the 382 of section 0, each
  read: is the address one the game stores, or ours alone?);
- a 32-bit word used as a native address (the 933);
- `long` and `unsigned long` where the width matters (Win64 is LLP64, so
  these are 32 bits there; the census marks them for the LP64 targets of
  section 9 and for the generated ctypes);
- inline asm, by the three classes of section 0, with the file's fuzz or
  live standing;
- naked functions, asm-label aliases, `__cdecl` / `__stdcall` spellings
  (ignored on x86-64, listed to be sure nothing depends on them);
- `fs:` reads, `VirtualAlloc` with an address, `sizeof(void *)`,
  `std::uintptr_t`, the `%p` and `%08X` log formats that print a place.

Output a TSV under `analysis/` and the counts in this doc. It sizes stage
B's groups and stage D's audit.

### XA2: the low-memory design
A design doc in the shape of [`relocatable-image-design.md`](relocatable-image-design.md),
answering with measurements:

- **the image as a block**: `VirtualAlloc` at `0x400000`, size the span to
  `.pool`'s end, before any heap or thread of ours exists (the probe says
  the window is free at `main`; the design says at what point of start-up
  it is still free and what `Fatal` says when it is not);
- **the heap the game sees**: which of the 47 allocation sites hand an
  address to the game (into a record, a packet, a `GameData` slot) and which
  are ours alone; a low arena for the first class (`VirtualAlloc` at
  `0x1000000`, measured; its size from the game's peak, `BOF3X_DATLOG` or a
  counting allocator over the routes), the toolchain's heap for the second;
- **the task stacks**: four `0x4000` slices today on the main thread; in
  the 64-bit build a low arena of our own, slice size from a stackwatch
  measurement of the 64-bit frames (the 32-bit need is `0x14294` for four,
  [`state3-design.md`](state3-design.md) 5; 64-bit frames are larger), and
  the switch written for `rsp` with the x86-64 callee-saved set (`rbx`,
  `rbp`, `rsi`, `rdi`, `r12`..`r15`, and the x87 state the switch never
  saved because the original never needed it - measured, not assumed);
- **code tokens**: a function of ours stored where the game reads a code
  word (19 sites narrow a function; the 8,784 bound words; the call-through
  helpers) - either a thunk table below 4 GB (one `jmp` per target, 6,346
  targets, under 128 KB) so the stored word is a real address, or the VA
  kept as the token and every indirect call resolved through `Code()`. The
  design measures which indirect calls read a word the game wrote at run
  time and which read load-time words, and recommends;
- `Img()` as a native pointer, `VaOf()` as a checked narrowing, `ImageBase()`
  as the block's address rather than the PEB's (the block is not a module);
  what `GetModuleHandle(nullptr)` means now (nothing of the game's, section
  1.3 of the reloc design retired);
- the quicksave: its span is a block at a fixed base in both widths, so a
  32-bit take and a 64-bit load compare equal on the image span and differ
  in what our frames hold on the task stacks ([`quicksave.md`](quicksave.md)
  1 and 3); whether a width tag in the build id refuses the cross-load or a
  rebase of the stack arena allows it;
- the crash reporter's addresses (our code is high now; the image's are
  VAs as before) and the `tools/` that read a dump.

### XA3: the asm policy, settled by a build
Compile every live file that holds asm with the `x86_64` driver, in
isolation, with a stub header that defines what the file includes. Report:
which of the 125 x87 blocks assemble unchanged (expected all: memory
operands, `st`, no general register of a 32-bit name), which of the 29
stack and control blocks fail and what each does in words, which naked
functions are reachable in the exe host at all (several exist only for
`BOF3X_ORIGINAL`'s switch of Capcom's code, which the 64-bit build cannot
offer - section 9). The report is stage B1's list. **An x87 block that
survives stays asm** in this round: the x87 semantics (the control word,
80-bit intermediates, `fistp` under chop) are what the original computes,
and a C++ rewrite is a divergence until fuzzed; the round that leaves x86
altogether (section 9) owns that question.

### XA4: the build
A second preset `x86_64` and `cmake/x86_64-llvm-mingw.cmake`; the guard at
`CMakeLists.txt:16` becomes a choice of the two; the hosted targets (the
injected DLL's hosted mode, the fuzz) built for `i686` alone, the exe host
and the launcher parts for both; SDL3 and minimp3 built for the width; a
CI job `x86_64` that configures and compiles (nothing runs in CI, rule 1)
and uploads `bof3x-x86_64-<sha>`. Until stage C the `x86_64` job is
allowed to fail at link; its compile log is XA3's second opinion.

## 4. Stage B: the rewrites, in the 32-bit build, gated (file groups, after XA1 and XA3 merge)

Each group owns its files, resets onto the round branch's tip, runs the
gate that applies, and reports. Merge one at a time.

- **B1, the stack and control asm** (from XA3's list; `task_sched`,
  `sound_rest`, `shell`, `scena_calls`, `save_menu`, `mode_tasks`,
  `field_e2`, `field_misc`, `field_modes`, `area_slope`, `battle_misc`,
  `magic_s28`, `map_field_objects`, `mp3.cpp`, `stackwatch`,
  `boss_harness`'s TEB reads). Each block becomes one of: C++ that the
  hosted fuzz proves against Capcom's function; a width-conditional pair
  (`i686` asm kept byte for byte, `x86_64` asm beside it) where the block
  is a stack switch no C++ expresses; or an `[[gnu::naked]]`-free thunk
  where the function only re-arranges arguments for a Capcom convention
  (the eax-result aliases). `codegen_same.py` for the files whose 32-bit
  code is meant to be unchanged, the fuzz for the rest.
- **B2, the narrowings**: the 382 sites through one helper, `Low32(p)`,
  which on the 32-bit build is the cast (`0 differ`) and on the 64-bit
  build checks the address lies below 4 GB and is `Fatal` otherwise - the
  loud trap for an address the design of XA2 missed; the 933 widenings
  through `Img()` or the helper's inverse; `%p` prints of places as VAs.
- **B3, the types**: the generated ctypes' `unsigned long` and `long` as
  `std::uint32_t` / `std::int32_t` in `tools/gen_symbols.py`'s output, and
  the 1,331 spellings in `src/game` where the census says the width is the
  image's (`0 differ` on Win32, where `long` is 32 bits; this group is for
  the LP64 targets and for the reader).
- **B4, the loader and the helpers**: `image_base.h` by width,
  `exe_host.cpp`'s start-up (the block, the low arena, the task-stack arena,
  the thunk table or the token rule), `host.cpp`'s identity for a 64-bit
  exe host, `snapshot.cpp`'s build id. Compiled for both widths; the 32-bit
  build's code identical (`0 differ`) or fuzzed, since the 32-bit exe host
  must not change behaviour.

Stage B merges to `main` as a PR of its own when every group is in: the
32-bit build is proved by its own gates, and the 64-bit build is not yet
claimed.

## 5. Stage C: the first 64-bit run (one agent, then the coordinator's runs)

The agent builds the `x86_64` preset to a linked `bof3x.exe`, fixes what
the link finds (an import, a symbol the asm-label aliases named with a
leading underscore that x86-64 does not use), and runs the self-tests
headless under the round's harness root. The coordinator then runs:

1. the attract sequence and the ten routes on the 64-bit exe host, the
   state hash against `cn_<route>_orig4r`: identical pair for pair, the
   Rand counts the references';
2. the quicksave: a take and a load on the 64-bit host (works); a 32-bit
   take loaded on the 64-bit host (the design's answer: refused by the
   build id, or loaded);
3. the FMV: `capcom.avi` plays through MCI on the 64-bit host (the Cinepak
   re-encode is the ledger's; a 64-bit `iccvid` is Windows' own, to be seen
   not assumed), and the two other AVIs;
4. the launcher: launches the 64-bit executable from the Sources page as it
   does the 32-bit one, the `bof3x.exe` path a setting; the 32-bit launcher
   may launch either;
5. the pace: the attract's 10,305 logic frames in 360 s
   ([`state-hash.md`](state-hash.md) 1) on the 64-bit host, against the
   32-bit host the same day.

A difference in 1 is a defect of stage B or of XA2's design and is fixed on
the round branch before anything else merges.

## 6. Stage D: audit and docs

A read-only audit of every width-conditional line in `src/` (the list is
XA1's census re-run on the round's tip: the count of `#if` by width in a
game file should be zero), of every `Low32()` that fires in no run but
could, and of the ledger: a 64-bit build that behaves as the 32-bit one
needs no DIV entry; anything stage C changed for the player (a refused
quicksave, a Fatal on low memory) gets one. `STATUS.md`, `HANDOFF.md`,
[`SCAFFOLDING.md`](SCAFFOLDING.md) 4 (the presets), `release.yml` (the
second artefact, signed as the first), this doc's "as run" sections.

## 7. For the owner

1. **The road: (N) the narrow image, or (W) the wide one** (section 1). The
   proposal is (N). Stage A is useful to both; (W) adds a stage B of the
   typed-data round's size and a new proof.
2. **The target set for this round.** Proposed: x86-64 Windows alone. The
   125 x87 blocks stay asm because x86-64 has an x87; the first non-x86
   target (an ARM64 Windows handheld, Apple silicon) needs an x87 model in
   software or a fuzzed C++ for each block, and the Linux and macOS targets
   need the platform library (I33's decision 3). That is a round of its
   own; naming it now only decides whether B1 writes its stack switch with
   a third variant in mind.
3. **What the 64-bit executable is for, at first.** A developer's build and
   a second release artefact beside the 32-bit one (proposed), or the
   player's default. The second needs a release of play on it first; the
   launcher can offer both from one Sources page.
4. **The platform library, early decision 3 of I33.** SDL3 is already in
   the tree for the pad (DIV-0050, zlib). Confirming it as the library for
   window, audio output and timing later is not this round's work, but B4's
   loader is written once and should not be written against an API the
   next round removes. A one-word answer is enough.

## 8. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/width_census.py`, `analysis/width_census.tsv` | XA1 |
| `docs/x64-build-design.md` | XA2 |
| XA3's report (no source) | XA3 |
| `CMakePresets.json`, `cmake/x86_64-llvm-mingw.cmake`, `CMakeLists.txt`, `build.yml` | XA4 |
| each source file | its stage B group, one owner |
| `src/common/image_base.h`, `src/hook/exe_host.cpp`, `host.cpp`, `snapshot.cpp` | B4 |
| the 64-bit link and the first runs | stage C |
| `STATUS.md`, `HANDOFF.md`, reference runs | the coordinator |

Merge order: XA1, XA2, XA3, XA4 (any order, no shared files); B1..B3 one at
a time with the gate after each; B4; stage B's PR; stage C; stage D. Every
commit `git commit -s`. Agents run no game: the coordinator runs the
self-tests and the proof, one at a time, in this round's own harness root
under `C:\Users\kerok\bof3x-runs\x64<mmdd>` (Defender-excluded,
one root per session). Ledger entries from DIV-0187, defects from
D398, both checked against `main` at launch. Worktrees reset onto the round
branch's tip, not `main`.

## 9. Not in this round

- Widening the image's words (road W), unless call 1 picks it.
- Any target but x86-64 Windows: ARM64, Linux, macOS, and with them the x87
  model and the platform library's window, audio and timing (I33).
- `BOF3X_ORIGINAL` and every switch of Capcom's code on the 64-bit host:
  Capcom's code is 32-bit and cannot run in the process. The ledger's
  `Switch:` fields that name it hold for the 32-bit build; the 64-bit host
  logs "original: not in this build" once and runs ours (the one line of
  player-facing text this round adds, STE).
- The hosted build and the fuzz: 32-bit at `0x400000` for ever.
- I5's translation-unit layout (after this, per the owner's 2026-10-21
  word on I44).
- The quicksave's size (I45) and the modding layers (I33).

## 10. Not verified, and the measurements missing

- That the 125 x87 blocks assemble under the `x86_64` driver as written:
  the classifier read mnemonics, not operand constraints; XA3 builds them.
- Which of the 47 allocation sites hand an address to the game: XA2 reads
  them.
- The 64-bit frames' depth on the task stacks: a stackwatch run on the
  64-bit host, stage C.
- That Windows ships a 64-bit Cinepak decoder reachable through MCI: stage
  C's run 3.
- The pace cost of `Img()` as a native pointer with a run-time block base:
  stage C's run 5.
- Whether anything in `src/game` depends on `__stdcall` / `__cdecl` beyond
  the spelling (the aliases of section 0): XA1 lists, XA3 compiles.
- Which naked functions exist only for `BOF3X_ORIGINAL`'s switch: XA3.
- The classifier's "20 other" blocks were read by hand for this doc from
  their first lines only; XA1 reads each.
