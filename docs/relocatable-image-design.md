# The relocatable image: the loader's design

**Status:** DRAFT (2026-10-28, the relocatable-image round's group RA3, written
on `relocatable-image/ra3` from `main` `15518394` before any code; every
number below is a count, a file and line, or a session script run against
that checkout and the player's `BOF3.exe`, read only - section 14. Nothing
was run. The owner's calls 2 and 3 of
[`relocatable-image-round.md`](relocatable-image-round.md) section 7 are
open; section 6 and section 11 give the measurements they need.)

[`relocatable-image-round.md`](relocatable-image-round.md) section 2 asks
RA3 for one document, in the shape of [`state3-design.md`](state3-design.md),
that stage C's agent builds to: where the image goes, the rebase of `.data`'s
pointer words, the code words, `.rdata` and the import slots, the entry-stub
page, and what follows for the quicksave, the crash reporter and the tools.
It also fixes the spelling stage B writes (section 2), which RB0 follows. It
cites rather than restates: [`state3-design.md`](state3-design.md) (the exe
host as it is), [`exe-import-engine.md`](exe-import-engine.md) 4 and 7 (the
stub-page choice, the layout table), [`exe-import.md`](exe-import.md) 5 and
6, [`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md),
[`quicksave.md`](quicksave.md), [`state-hash.md`](state-hash.md) 7 and
[`IDEAS.md`](IDEAS.md) I44 and I46.

## 0. The shape in one paragraph

The image stays where state 3 put it - **the sections of `bof3x.exe`**,
`.stub`, `.game` and `.pool` at their RVAs `0x1000`, `0x1C4000`, `0x53F000` -
but nothing of ours assumes the executable's base any more: the engine reads
it once, `GetModuleHandle(nullptr)`, in both hosts (one `bof3x.dll` serves
both). Every address in the source stays written as **Capcom's address**
(a "VA", the frame `symbols.toml`, the docs, the recipes, the logs and the
tools all use), and is turned into a place by one helper: `Img(va)` for data,
`Code(va)` for code. After the data copy the loader adds the move to every
pointer word `recipes/exe-pointers.tsv` lists (18,831 words), and the 8,784
code words are either moved with the stub page (mode S) or bound by name to
our functions with the stub page turned into a no-access trap (mode B). The
image must stay **below 16 MB**, because the ordering-table links are 24-bit,
so the base can move only within `0x10000..0xA70000`. A quicksave taken at one
base is refused at another; the crash reporter and the tools keep speaking
VAs, translating at three reader functions. Stage C is proved at the default
base and at `0xA00000`, a base disjoint from Capcom's range, with the old
range fenced so that any address left absolute faults.

## 1. Where the image goes

### 1.1 Two places, measured

| | **(B) The executable's own sections** (recommended) | (A) A `VirtualAlloc` block |
|---|---|---|
| What holds `0x587000` bytes below 16 MB | the PE loader, at process creation, before any heap or thread block exists ([`state3-design.md`](state3-design.md) 1.1) | our code, at `Engine_Main`, after `ntdll`, the heap, the TEB and the DLLs have taken what they take - **not measured**; hosted, the range below 16 MB was cut to a largest free gap of 192 KB by the end of the injects (2026-09-27, `draw_pool.cpp` `DrawPool_Reserve`'s comment) |
| `GetModuleHandle(nullptr)` | the image base, by construction | `bof3x.exe`'s base, unrelated to the image |
| How the base is chosen | the PE header's `ImageBase` (`tools/make_exe.py` writes it; no `.reloc`, no `DYNAMIC_BASE`) | at run time: a `BOF3X_IMAGEBASE` variable works directly |
| The forced base for the proof | a second executable written by `make_exe.py --image-base` (section 1.5) | the variable |
| Host detection | as today, less the base test (section 1.3) | a new identity: `.stub` is no longer a section of the file |
| `make_exe.py` | one option; the entry's five absolute operands (`call [IAT]`, `make_exe.py:268-289`: `GetStartupInfoA`, `GetCommandLineA`, `GetModuleHandleW`, `Engine_Main`, `ExitProcess`) written for the chosen base | the three big sections dropped (`SizeOfImage` `0x58F000` to about `0x8000`) |

**Recommended: (B).** It keeps the one guarantee state 3 was built on - the
range is the loader's before anything can land in it - and turns the base
into a build parameter the source no longer knows. `DYNAMIC_BASE` stays off
and the file keeps no `.reloc`: ASLR would place the executable wherever the
system likes, and section 1.2 bounds where it may go. The player's executable
stays at `0x400000`; what the round changes is the source, which no longer
names that number, and the proof that it does not. (A) is the later shape,
when the image leaves the executable (the 64-bit build, I44 "What follows");
a ten-line probe at `Engine_Main` (`VirtualQuery` over `0x10000..0x1000000`)
would say whether it can find `0x587000` bytes there today.

### 1.2 The 16 MB window: the ordering table's links are 24-bit

Measured in the source:

- `Gfx_DrawOTag` walks the ordering table and, for a link whose top byte is
  set, masks it to 24 bits and skips the node (`src/game/d3d_list.cpp:209`,
  `if (p & 0xFF000000u) p &= 0xFFFFFFu;`). A packet at `0x1000000` or above
  is therefore never drawn, and the walk continues at a truncated address.
- `Gpu_AddPrim` stores the packet's full address with bit 31 of the old link
  (`src/game/psx_gpu.cpp:114-119`).
- DIV-0062's pool asserts it lies below 16 MB "as everything of Capcom's
  does" (`src/game/draw_pool.cpp:250`, the reason at `:293`; the fallback
  search stops at `0x1000000`, `:270`).

The packets live in the image's `.bss` and in `.pool`. With the image's
layout unchanged, `.pool` ends at base + `0x587000`
(`src/hook/exe_layout.h:42-45`), so **base <= `0x1000000 - 0x587000` =
`0xA79000`**, at 64 KB granularity **`0xA70000`**; the floor is the first
allocatable address, `0x10000`. The loader checks the base against this
window and stops with a `Fatal` otherwise. This is also a fact for the owner's
call 1: either 64-bit road needs low memory for the packets, or widened links.

### 1.3 What `GetModuleHandle(nullptr)` must still be

`bof3x.exe`'s module, because these want **the executable's module handle**,
not an address:

| Site | Use |
|---|---|
| `tools/make_exe.py:282-283` | the entry passes `GetModuleHandleW(NULL)` to `Engine_Main` as `hinstance` |
| `src/game/win_main.cpp:576` | `Game_HInstance = hinstance` (kept across a quick-load, `snapshot.cpp:154`) |
| `src/game/win_main.cpp:585`, `:586`, `:607` | `RegisterClassA`'s `hInstance`, `LoadIconA(hinstance, ...)` (our `.rsrc`'s icon), `CreateWindowExA` |
| `src/game/pad_read.cpp:244` | `DirectInputCreateA(hinstance, ...)` |

Under (B) it is also the image base, so nothing changes for them. What
changes is the code that **tests** it as a number:

| Site | Today | Then |
|---|---|---|
| `src/hook/host.cpp:28` | the exe host only `at == exe_layout::kImageBase` | the identity alone: the timestamp `0x6ACC8E00` and `.stub` at RVA `0x1000` (`:29-31`) |
| `src/hook/exe_host.cpp:90` | `m.AllocationBase != At(kImageBase)` | `ImageBase()` |
| `src/hook/gapwatch.cpp:458` | `Va(m) == exe_layout::kImageBase` | `ImageBase()` |
| `src/hook/detour.cpp:148-151` (`VerifyImage`), `host.cpp:26` | the hosted build: `BOF3.exe` at `0x400000` | unchanged: that is the player's file, which is fixed |

### 1.4 The base at run time: one DLL, two hosts

The round doc's stage C keeps the base `constexpr` in the hosted build. One
binary cannot do that: `CMakeLists.txt:97` builds one `bof3x SHARED`, loaded
by `BOF3.exe` and by `bof3x.exe` alike. So the base is **a run-time value in
both hosts**, `ImageBase()`, which reads `0x400000` hosted (`BOF3.exe` is
fixed) and whatever the executable's header said in the exe host. The fuzz
files need no change for it: they run hosted only, where `Img` is the
identity at run time.

`ImageBase()` must be valid before `DllMain`: namespace-scope tables built
from addresses run as dynamic initialisers in the DLL's CRT start-up (the
pattern is `shell.cpp:45-55`, `const Callees kOriginals = {...}` holding
`kSoundSetup`, a code address cast to a function pointer). So it is computed
on first use from `GetModuleHandleW(nullptr)` (safe there: the executable is
mapped before any DLL initialises) and checked by `DetectHost` against the
identity and the 16 MB window. How many such initialisers exist is not
counted (section 13).

### 1.5 The forced base, and the fence

`make_exe.py --image-base <addr>` writes the same executable at another
base (the five entry operands follow; the section table, `exe_layout.h` and
the imports are RVAs and do not). For stage C's proof the build writes a
second file at **`0xA00000`**: its span `0xA00000..0xF8F000` lies inside the
16 MB window and **does not overlap** Capcom's range `0x400000..0x98F000`.
`BOF3X_IMAGEBASE` is then the launcher's developer switch that starts that
file instead of `bof3x.exe` (a value with no such file is a `Fatal`).

**The fence.** When the base is not `0x400000`, the engine reserves the free
parts of `0x400000..0x98F000` `PAGE_NOACCESS` at `Engine_Main` and logs what
it could not take. Any address the stage B groups missed - a data VA read or
written as a place, a code VA called - then faults at its own VA, and the
crash reporter names it (section 8). The state hash alone would not show a
missed *read* that happens to return zero; the fence does.

## 2. The spelling: a VA names, `Img(va)` places

### 2.1 The choice for `at::` constants: addresses, with one helper

Of the two spellings the round doc offers - constants as offsets from the
base, or constants as addresses with one helper adding the base - **this
design takes addresses with one helper**, for four reasons:

1. **Every other artifact speaks VAs.** `symbols.toml`'s 3,248 `[[data]]` and
   10,100 `[[func]]` entries are keyed by `pc`; `tools/data_census.py`
   matches constants to them by value (8,674 exact, 2,352 inside an entry);
   43 tool files hold 851 image-range literals (section 9); the input
   recipes' `peek` / `poke` / `watch` take VAs (`input_script.cpp:51-52`,
   `:97`); the logs print VAs; the docs cite them by the thousand. An offset
   would differ from every citation by `0x400000`, and `grep 0x904EFC` would
   stop finding the code - rule 3's audit is a grep.
2. **Code addresses must stay VAs anyway.** They key `Inject`, `BOF3X_ORIGINAL`
   (hosted), the harnesses' stand-ins and the bind table (section 4); one
   frame for data and code keeps one rule.
3. **The machine code is the same either way in stage B**: `Img(va)` with a
   `constexpr` base `0x400000` folds to `va`, as `base + offset` would.
4. **Differences of two VAs need nothing.** `pillarbox.cpp:54`
   (`image::kBase + image::kSizeOfImage - AreaMap_Header_addr`) is a size;
   the base cancels. With offsets it would read the same, but with addresses
   such a site is visibly correct as written, and RA1's census can leave it.

The cost is the same as the offset spelling's: a constant becomes a place
only where a helper is applied, because the memory accessors also take real
pointers (record sites, `Word(p + 0x1C)`, 9,047 of them) and so cannot add
the base themselves.

### 2.2 The shape RB0 emits

| Name | Stage B | Stage C |
|---|---|---|
| `bof3::image::kLinkedBase` | `0x400000`: the frame every VA is written in (today's `image::kBase`, kept for `BOF3.exe`'s own checks) | the same, for ever |
| `bof3::ImageBase()` | `constexpr`, returns `0x400000` | run-time, section 1.4 |
| `bof3::Img(va)` | `ImageBase() + (va - kLinkedBase)`, `constexpr` - the identity | a place in the moved image |
| `bof3::Code(va)` | `constexpr`, the identity | mode S: `Img(va)`; mode B: the bound function (section 4.4) |

A function, `ImageBase()`, rather than a constant `kImage`: the name survives
stage C unchanged, where a `k` name would be a constant that is not.

**Generated accessors** (`tools/gen_symbols.py:364-370`):
`#define Party_Zenny (*reinterpret_cast<unsigned long *>(::bof3::Img(0x904058)))`;
`BOF3_GAME` (`:196`) through `Img(kGameDataVa)`; `bof3::addr::Name` (`:309-312`)
and `<Name>_addr` (`:203-206`, `:377`, `:391`) stay VAs - names - and their
5,493 uses outside the fuzz and harness files (301 files; 7,414 more in fuzz
and harness files) are sites that place one (`Mem(ObjTrio_addr)`,
`At(ObjTrio_addr)`) and need the helper.

**Recommended to RB0, to be proved on one file family first: a type for a
VA.** `struct Va { std::uint32_t v; }`, `constexpr`, with `Va + int` and
`Va - Va` (an integer), no implicit conversion to an integer, and an
overload of each memory accessor taking a `Va` and applying `Img`. Then
`B(at::kX + 4 * i)` needs no edit, and every site that needs a decision -
a VA stored as a value, compared with a place, passed to an integer
parameter - **fails to compile** instead of compiling to the old address. The
compiler, not a grep, is then the completeness check, and stage B's edits
shrink to the definitions (4,108 `at::` definitions, 838 module data
constants, the generated header) and the sites the compiler names. If
`codegen_same.py` shows the overloads change machine code anywhere (a trivial
struct by value should not), RB0 falls back to plain integers and explicit
`Img()` at sites, with RA1's census as the completeness check.

### 2.3 What stays a VA

A VA is a **name** wherever it is compared with another name or shown:
`Inject` / `RetargetCall` / `PatchBytes` / `CloneOriginal`'s address
arguments (76 code-address uses outside the fuzz and harness files, by the
session script of section 14); the operand-store values compared by name
(`rest_2d.cpp:133`, `:139`, `rest_2f.cpp:1477`, `:1521`: a value that is
either `addr::Text_DrawAt` or a pointer of ours, never stored in the game's
memory); every log line and every recipe address. `Inject` gets
`Img`/`Code` inside, where it writes (`detour.cpp` `WriteJmp`), and its
range test becomes a test on the VA (`detour.cpp:171`, `host.h:34`
`InCode`, which `crash.cpp:97` uses for places - split into a VA test and a
place test).

## 3. The rebase of `.data`'s pointer words

### 3.1 The words, by class

`recipes/exe-pointers.tsv` at `15518394`: 2,679 runs, **18,831 words**
(the round doc's 2,687 runs, 9,959 data and 35 `.bss` words are the
2026-10-12 counts of [`exe-import-engine.md`](exe-import-engine.md) 7.1;
today's, re-counted):

| Class (`to`) | Words | Runs | What the loader does |
|---|---:|---:|---|
| `data` | 9,951 | 1,793 | add the move |
| `text` (code) | 8,784 | 850 | section 4 |
| `rdata` | 63 | 20 | add the move (section 5) |
| `bss` | 33 | 16 | add the move (but section 3.4) |
| `number` (`exe-layout.tsv` only) | 371 | | nothing |

The words lie on 142 of the 156 pages of initialised `.data`; code words on
123 of them (`exe-layout.tsv`, section 14).

### 3.2 When, and the check

After `CopyData` (`exe_host.cpp:185-211`: the file's or the cache's
`.data`, `rdata::Map`, the default keys, the `BOF3X_OPT` layers, the
comparison) and **before** anything of ours writes `.data` - `BindSlots`,
`StateHash_BeforeInjects` (whose slot census must then see the moved values),
`InjectAll` and the language overlays that re-aim Capcom's pointer tables at
our text ([`state-hash.md`](state-hash.md) 7). For each listed word: its
value must lie in the linked frame of its class's section
(`0x5DA000..0x93D6EC` for `data` and `bss`, `0x5C4000..0x5DA000` for
`rdata`, `0x401000..0x5C4000` for `text`), else a `Fatal` naming the word -
a layer or a cache that wrote a non-pointer over a pointer word is refused,
not moved. Then the move is added. At the default base the move is zero and
the check still runs: it is the loader's own test of the table. The table is
generated into the DLL at build time from the two recipe files (addresses
only), as `symbols.gen.h` is from `symbols.toml`; 2,679 runs and, for mode B,
8,673 named rows (section 4.1).

Hosted, the loader does nothing: `BOF3.exe`'s own loader placed the image at
the linked base.

### 3.3 A disc-built image

Since the layout table ([`exe-import-engine.md`](exe-import-engine.md) 7.1),
**no pointer word of a disc-built image is unfilled**: the rebuild and
`recipes/exe-layout.tsv` fill every one with a target in the PC's linked
frame (`verify`: "pointer words unfilled: 0"), and `exe_tables.py check`
holds the `gap`, `language`, `unread` and `none` ranges clear of every pointer
word. So the rebase does not depend on the source: a word of `data.bin`,
from `BOF3.exe` or from a disc, is a linked-frame address, and the same
table moves it. A disc build's unfilled bytes are numbers (zero where the PC
has a value); none is a pointer, and the rebase never touches them. The
check of 3.2 is what would catch a cache written before the layout table.

### 3.4 What is not moved, and the risk in what is

- **371 `number` words** (358 into `.text`, the rest `.bss`-shaped,
  [`exe-import-engine.md`](exe-import-engine.md) 7.2): left alone, as today.
- **6,987 pointer-shaped words `exe-pointers.tsv` does not list** (section 14,
  `image_words.py`): of the 26,189 aligned words of `.data` whose value lies in
  `BOF3.exe`'s image - the same 26,189 `BOF3SH3`'s slot census counts
  ([`state-hash.md`](state-hash.md) 7) - 18,831 are listed pointers, 371
  listed numbers and 6,987 unlisted: words every disc that places them holds
  equal, which the recipe therefore calls numbers (the file's header). By
  target: 2,636 into `.text`, 3,543 into `.data`, 20 into `.rdata`, 1 into
  `.rsrc`, 787 outside a section's virtual extent. Not moved. A real pointer among them would
  break silently at another base; the fence (1.5) turns its first
  dereference into a fault.
- **Four `bss` words held as pointers that may be numbers**: of the six
  doubtful words of [`exe-import-engine.md`](exe-import-engine.md) 7.2, four
  are still in the 33 - `0x5E1AC0` (to `0x800E00`), `0x6542F0` and
  `0x65432C` (to `0x808000`), `0x654C08` (to `0x828284`, colour-shaped). At
  the default base the move is zero; at another, moving a number corrupts
  it, and **the address-independent hash would hide it** (RA2 hashes a word
  in a pointer slot as its offset, so a moved number hashes like the
  original). These 16 bytes want a reading before stage C, and the same blind
  spot holds for any `data` word the recipe misclasses (it does not reclass
  `data` words: a string pointer and text cannot be told apart by shape).

### 3.5 Run-time pointers

The game's own run-time pointers - heap, arena, the records it links - are
built from places it already holds (a moved word, a stack address, a heap
block), so they are right at any base. What is built **from a constant** is
our code's spelling of a VA as a place: `Sprite_Current = Mem(ObjTrio_addr)`
(`area_011.cpp:58`), `Gfx_CurrentEnv = env` from `0x903880 + index * 0x90`
(`win_main.cpp:768`, the loop RA1 reads), every `_addr` and `at::` value
stored into the game's memory. These are stage B's sites (section 2.2; the
typed `Va` makes the compiler list the ones that store a VA as a value). No
pointer built from a constant is known in a disc-loaded chunk; that is not
measured (section 13), and the fence covers it.

## 4. The code words

### 4.1 Bound by name

`recipes/exe-layout.tsv`'s 8,784 `code` rows (section 14): **8,673 name a
`[[func]]`, 6,211 functions, and every one of those has `impl`** - no word
targets one of the twelve without (`Fmv_EnterFullscreen`, `Crt_ftol`,
`Crt_GetPtd`, the decoder's eight, `Gfx_PackRgb`). Mode B writes `&ours` into
each, from a generated table of (word VA, function) built from the same
`impl` binding `gen_symbols.py` already emits - 8,673 rows, about 69 KB.

### 4.2 By address: 111 words, 90 targets, no function

The C runtime's tables after `Crt_GetPtd`, the blitters' jump table after
`Gfx_PackRgb`, three at the head of `.data` (`0x5DA00C`, `0x5DA010`,
`0x5DA01C`). Nothing of ours reads them
([`exe-import-engine.md`](exe-import-engine.md) 7.6). They are moved into the
trap (4.5): `Img(target)`, so a call through one faults at a place that names
its VA.

### 4.3 Mid-function words

None is left in the table. Of the 469 `.data` words into the middle of a
function, 111 are pointers (each right after a `ret`, the 4.2 rows) and 358
numbers (the `number` class);
[`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md)'s seven are
among the 358. Numbers stay numbers: neither mode touches them.

### 4.4 Our code's code places

The stub page today makes every Capcom code address our code holds a working
call. Without it (mode B), each code VA our code **places** - calls, or
stores where the game will call or compare it - must give the bound function,
which is `Code(va)`. By the session script of section 14 (comments and
strings blanked; `addr::` uses are exact, hex literals in `0x401000..0x5C4000`
are not - most are field coordinates such as `0x480000`):

| Use, outside the fuzz and harness files | Count | Fix point |
|---|---:|---|
| calls through an address: `AH_AT` 193, `BH_AT` 168, `MH_AT` 173, `SH_AT` 364 (898 in all), `Phase(` 300, `Call0..6(addr::...)` 309, function-pointer casts from an address 24 | ~1,531 | the four harnesses' `Call` templates (`area_harness.h:98-106`, `boss_harness.h:200-202`, `magic_harness.h:76-78`, `scenario_harness.h:172-174`), `Phase`, the `CallN` helpers: `Code()` applied once inside each, so the 1,531 lines need no edit. A value already ours (a bound word read by `Phase`) passes through |
| `addr::<func>` as a value (definitions such as `constexpr U kX = bof3::addr::Zenny_Sub;`, table entries, stores) | 3,351 | the definitions; the compiler's list if code VAs get their own type (2.2) |
| code VAs compared with a place | a handful: the measured compares are names (2.3) or hosted only (`psx_gte_*.cpp` fuzz set-up, `calltrace.cpp:344`, `field_blocked.cpp:647`) | per site |
| callee tables holding a code VA (`shell.cpp:51` `kSoundSetup`) | 24 casts above | the definitions |
| the `DirectSoundCreate` thunk `0x5ACBBC` (`sound_rest_callees.h:18`, called at `sound_rest.cpp:53`) | 1 | not a `[[func]]` (`symbols.toml` names it only in evidence): mode B binds it to the slot's resolved `DirectSoundCreate` as one extra row; mode S writes the thunk with its operand at `Img(0x5C4014)` (today the bytes are fixed, `exe_host.cpp:291-292`) |

`Code(va)` in mode B: the VA looked up in the injected table (`detour.cpp`'s
owned list, which then keeps `ours` beside each VA), a binary search over
10,088 sorted rows, a `Fatal` naming the VA on a miss. Hosted it is the
identity (so `BOF3X_ORIGINAL` still reaches Capcom's code through the
address, the reason the 61 calls of
[`round-13-cleanup.md`](round-13-cleanup.md) 1.3 were left as they are). The
1,531 call-through lines are a far larger set than those 61; the fix point
being the helpers is what keeps them out of stage B's site count.

### 4.5 What replaces the `int3` wall

In mode B `.stub` is not written and goes **`PAGE_NOACCESS`** after the
layout check: 1.8 MB of reserved address space (`kStubSize` `0x1C3000`), no
jmps. Every code value that was not bound - one of the 111 words, a VA our
code placed without `Code()`, a value computed from a code constant - lands
on it, faults with an execute access violation at `base + (VA - 0x400000)`,
and the crash reporter writes the VA and its `symbols.toml` name (section 8).
That is rule 4's "aborts loudly", with the address named, as the `int3`
fill was. A `Code(va)` miss is a `Fatal` before the call. In mode S the page
is today's: `int3` throughout, a `jmp` at each of the 10,088 owned starts, at
the moved base.

## 5. `.rdata` and the import slots

- **The import slots: 14 rows, not 13** - `exe_host.cpp:224-239`'s `kSlots`,
  the thirteen our code calls through plus `0x5C4014`, the thunk's. Each is
  written at `Img(slot)` with the DLL's function, whose address lies outside
  the image and is not moved. Our readers place them: `Imp_GetTickCount`
  (`game_clock.cpp:95`, DIV-0022's `PatchBytes` on the slot),
  `gfx_texcache.cpp:213` (`0x5C4168`, "bound yet"), `shell.cpp:49`
  (`Shell_ImportGetDriveType_addr`), the `psx_rest` / `sound` callee headers,
  `wav_dump.cpp`'s hook of `0x5C4014`, the self-test at `exe_host.cpp:264`.
- **`.data`'s 63 words into `.rdata`**: in `0x672948..0x6755D0`, 62 distinct
  targets in `0x5D8DBC..0x5D9340` (the C runtime's strings). In the exe host
  those bytes are zero: `rdata::Map` (`rdata_consts.cpp:97-104`) writes only
  the 44 constants and `c_dfDIKeyboard`'s header. Nothing of `src/` names a
  target (a `grep` of the range). Moved with the rest, so the table has one
  rule and the hash sees an offset on both sides.
- **`.rdata`'s own words**: 541 aligned words of the file's `.rdata` lie in
  the image (127 into `.text` - the 81 targets of
  [`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md) 1 - 202
  into `.data`, 208 into `.rdata`, 4 elsewhere). **None exists in the exe
  host**: `.rdata` is the engine's own, not the file's. Nothing to do.
- **`rdata::Map`** writes its 44 constants and the keyboard header at
  `Img(pc)`; the header's object-table pointer names the SDK's table in our
  DLL, not the image. The 44 constants' readers (`rdata::Const`, 73
  definitions in the headers) are stage B sites.

## 6. The entry-stub page: two options (the owner's call 2)

### 6.1 The two modes

- **Mode S, the stub page moved.** `.stub` is filled with `int3` and the
  `jmp`s at the moved base, every code word gets the move like a data word,
  `Code(va)` = `Img(va)`. One representation for a code value - the stub
  address - as today. This is state 3 at another base.
- **Mode B, bound.** Section 4: every code value in memory is `&ours`;
  `.stub` is a trap. One representation again - our address.

A **mixed** mode (words bound, the page kept working as a safety net) is
refused by this design: a function would then have two representations in
the game's memory, and a compare of a stored handler with either silently
fails for the other - the failure [`exe-import-engine.md`](exe-import-engine.md)
4.2 (a) named. The net must be a trap, not a second working entry.

### 6.2 Stage C in two steps, whatever the call

**C1** builds mode S and proves the move alone: the image at `0xA00000`
against `0x400000`, nothing bound. **C2** builds mode B and proves the binding
alone, at the default base and at `0xA00000`. Each step's proof then has one
cause for any difference. Mode S costs almost nothing to build: hosted,
`Code(va)` is already the identity, and the exe host's `jmp` writing exists
today (`exe_host.cpp:294-354`); C1 adds the move. The owner's call is
whether C1's path survives C2.

### 6.3 The options, with their costs

| | **K: kept one release behind a switch** (`BOF3X_STUBPAGE=1` selects mode S) | **R: removed at the end of stage C** |
|---|---|---|
| Code | the loader's two code-word paths (move or bind), `Inject`'s two exe-host paths (write the `jmp`, or record the pair), the thunk in two forms; `Code()` costs no extra branch, since the hosted path is mode S's already | one path; the `jmp`-writing in the exe host, `CheckStub` and `WriteThunk` deleted (about 60 lines of `exe_host.cpp`) |
| Proof runs (one at a time, the coordinator's) | 11 routes (the ten and the attract) x 2 bases x 2 modes = **44** | 11 x 2 bases = **22** |
| State hash | both modes must hash a code value by its VA (section 12, RA2); then S and B compare with each other and with the references | the same requirement, for B against the references |
| Quicksave | the mode is part of the build id: the header's `host` field (`quicksave.md` 4) takes a third value, and a load across modes is refused | nothing |
| Address space | the same: `.stub`'s 1.8 MB exists in both (a trap in B) | |
| Later | one more change in the release after, to delete mode S | none |
| What it buys | a field fallback: a binding defect a player meets is a switch, not a wait for a fix; and an A/B of the binding on one build | simpler code; a binding defect is a loud trap and a fix release |

Recommended by this design: **R**, because the trap makes a binding miss
loud and named (it cannot fork silently), the 22-run proof covers both bases,
and the owner's own play comes before a release; **K** if the owner wants
a player-side fallback for the first release that binds code. Either way C1
is built and proved first.

## 7. The quicksave

**Refuse, no rebase.** A quicksave is native memory: the span holds the
game's run-time pointers at the take's base in slots no table lists (the
26,189 static slots are the load-time census; the game writes pointers into
records, the arena and `.bss` as it runs), and the task arena holds our
frames' locals, some of them image addresses ([`quicksave.md`](quicksave.md)
1, 3). A rebase could move only the listed words; anything else would be
restored pointing into the old base, or, worse, a number that looks like an
address would be moved. Refusing costs a player nothing: the player's
executable is always at `0x400000`, so a different base means the proof's
second executable or a developer's.

What it takes: nothing new in the format. The header already records the
span's base and length and refuses another (`snapshot.cpp:453-456`, "another
span"); with the span computed as `Img(kDataVa)..Img(kOwnVa)` (`snapshot.cpp:201`),
a take at `0x400000` and a load at `0xA00000` differ there and are refused
before a byte is written. The keep list's addresses (`snapshot.cpp:151-161`)
and every participant's reads are `Img()` sites. Under option K the mode
joins the build id (6.3). I46's question - a doctored quicksave can run
anything - is unchanged: refusing adds no path.

The proof's check (round doc 4): a quicksave taken at one base and loaded at
the other prints the refusal, the game untouched; taken and loaded at the
same moved base, the quick-load works as at `0x400000`.

## 8. The crash reporter

| Site | Change |
|---|---|
| `crash.cpp:83-95` `Where` | none: it prints module + offset (`bof3x.exe+0x...`), base-independent already |
| `crash.cpp:97` `InCode` | the place test: `CodeLo()` / `CodeHi()` (`host.cpp:50-54`) at `Img`; in mode B no return address lies in `.stub`, and an execute fault there is the trap |
| `crash.cpp:119-130` | when the faulting or read address lies in the image, also print its **VA** (`addr - ImageBase() + 0x400000`) and, for `.stub`, the `[[func]]` the VA starts or lies in - the trap's message |
| `crash.cpp:150-151` | `Peek(addr::Task_Records ...)`, `addr::MsgBoxState`: `Img()` |
| `tools/crash_report.py:100-114` | it names addresses by `symbols.toml` VAs and reads the file's sections at the PC's base; it reads the dump's module list (`:50-53`), so: translate an address inside `bof3x.exe`'s module by that module's base, and take `--read` / `--u32` arguments as VAs |

## 9. `tools/` that name Capcom's addresses

43 tool files hold 851 literals in `0x401000..0x98F000` (section 14). Most
read the player's file or a disc offline and are untouched by where the exe
host puts the image. The ones that touch a running exe host or its products:

| Kind | Tools | Fix point |
|---|---|---|
| live memory (`ReadProcessMemory` at VAs) | `task_stacks.py` (its `Game.read`, `:54`, also serves `ini_probe.py`, `key_probe.py`, `close_probe.py`, `mem_watch.py`), `attract_watch.py:72`, `mem_dump.py:69` | **three reader functions**: add the process's main-module base minus `0x400000` (the module base from `EnumProcessModules`, or the `host:` log line, `host.cpp:40`, which prints it) |
| the input recipe's `peek` / `poke` / `watch ADDR` | `door_sweep.py`, `spell_sweep.py`, `chapter_jump.py` and every recipe under `recipes/` | **one engine site**: `input_script.cpp` takes ADDR as a VA and places it with `Img`; its log lines keep printing the VA. No tool or recipe changes |
| log lines that print addresses | `gapwatch`, `statehash`, `stp_census`, `draw_order` | the rule: logs print VAs. A log line printing a place would make every parser base-dependent |
| the state hash | `statehash.py` (`BASE, END = 0x5DA000, 0x93E000`, `:38`), `quicksave_check.py:666`, `door_sweep.py` | RA2's: page numbers as offsets from the image base |
| minidumps | `crash_report.py` | section 8 |
| hosted only | `calltrace.py`, `owned_sweep.py` (reads `.text` ranges of the file) | none: the call trace is refused in the exe host (`exe_host.cpp:75-77`) |

## 10. Stage C's loader, in order

The exe host's `Engine_Main` (`exe_host.cpp:420-476`), with what changes:

1. `DetectHost` (`DllMain`): the identity without the base; `ImageBase()`
   in the 16 MB window, else `Fatal`.
2. The fence, when `ImageBase() != 0x400000` (1.5).
3. `CheckLayout` (`:99-106`) at `Img`.
4. `CopyData` (`:185-211`) into `Img(kDataVa)`; `ExeImage_Source` and
   `ExeImage_LayOptTables` already take the place (`to`) and the VA (`va`)
   apart (`exe_image.h:34`, `:68`) - the pattern the rest follows.
5. **The rebase** (3.2): the check, then the move on `data` / `bss` /
   `rdata` words; code words moved (mode S) or bound (mode B, 4.1, 4.2).
6. `BindSlots` (`:252-270`) at `Img(slot)`.
7. The x87 word, the crash reporter, the state hash's census (over the moved
   values).
8. Mode S: `FillStub`, `InjectAll` writing at `Img(va)`, `WriteThunk` with
   `Img(0x5C4014)`, `CheckStub`, `ProtectStub` - today's steps, moved.
   Mode B: `InjectAll` recording (VA, ours) pairs only, the thunk's row added,
   `.stub` `PAGE_NOACCESS`.
9. `Game_WinMain(GetModuleHandle(nullptr), ...)` as today.

**The proof** (round doc 4), the coordinator's, one run at a time: C1 - mode
S at `0x400000` against the current references (unchanged: the move is zero)
and at `0xA00000` against those, through RA2's normalisation; C2 - mode B at
both bases against the same; the headless self-tests in every configuration;
the quicksave check of section 7. `codegen_same.py` cannot be the gate in
stage C: the machine code changes everywhere by design (round doc 1).

## 11. The owner's call 3: stage B in one wave or two

What the measurements bear on:

- **The size depends on the spelling's mechanism (2.2).** With plain integers,
  every place is an edit: RA1's 17,259 global sites, the 5,493 `_addr`
  places, the 171 asm operands, the code places. With the typed `Va` and
  accessor overloads, the edits are the definitions (4,108 + 838 + the
  generated header) and the sites the compiler lists. RB0's prototype on one
  family measures the second number; the call is better made after it.
- **Code places are a different kind of work.** The data side is mechanical
  and `codegen_same.py`-gated (the gate proves no behaviour change, not
  completeness: `Img` folds to the identity, so a missed site compiles the
  same - completeness is RA1's census at zero, or the compiler under the
  typed `Va`). The code side is mostly the helpers (4.4) plus a few hundred
  definitions, each a judgment - a place (`Code()`) or a name (stays a VA) -
  that `codegen_same.py` cannot check (`Code` also folds) and only stage C2's
  run proves.

Reading: **two waves, split by kind rather than by count** - wave 1 the data
places (RB0, then file groups by family, sized by RA1), wave 2 the code places
(the helpers, the code-VA definitions, the thunk), a small wave whose review
is the place-or-name reading. If the typed `Va` works, wave 1 is small enough
to need no split by size.

## 12. For the other groups

- **RA1**: count, as their own classes, the `<Name>_addr` places (5,493
  outside fuzz and harness files), the call-through lines of 4.4 (they need no
  edit if the helpers place them, but the census should show they are
  covered), code-VA definitions, and VA - VA differences (base-independent,
  2.1 item 4).
- **RA2**: in mode B a code word holds `&ours`, which `BOF3SH3` hashes as the
  tag `0x80000000` ([`state-hash.md`](state-hash.md) 7, point 3), while the
  references hold the VA (raw today, an offset under RA2). For the proof of
  C2 against the references, a code value must hash as **its VA in both
  modes**: a stub-page value as `value - ImageBase() + 0x400000`, a pointer
  of ours that `Inject` bound as the VA it was bound for (the owned table,
  one to one). The 123 pages that hold code words would otherwise differ from
  tick 0. And: RA2's offset normalisation hides a number moved by mistake
  (3.4).
- **RB0**: the names of 2.2; `ImageBase()` a function from the start;
  `<Name>_addr` and `addr::` stay VAs; the typed `Va` tried on one family
  first.

## 13. Not verified, and the measurements missing

- That Windows maps a no-`.reloc` executable at `0xA00000` (1.5): a
  self-test-only run of the moved file settles it (section 14's command; the
  coordinator's).
- What is free in `0x400000..0x98F000` at `Engine_Main` when the executable is
  at `0xA00000` (the fence's reach), and in `0x10000..0x1000000` (option A).
- The cost of a run-time base: one load more per global access over about
  17,000 sites; the attract's pace (10,305 logic frames in 360 s,
  [`state-hash.md`](state-hash.md) 1) is the check.
- How many namespace-scope initialisers compute a place (1.4).
- Whether any disc-loaded chunk carries an address in the PC image (expected
  none: the PC's data files carry the PlayStation's layout); the fence would
  fault on its first use.
- The four doubtful `bss` words (3.4), unread.
- Whether the typed `Va` keeps `codegen_same.py` at `0 differ` (2.2).
- The hex-literal half of section 4.4's counts mixes code VAs with field
  coordinates; the `addr::` half is exact. RA1's census is the authority for
  stage B's counts.

## 14. Commands

```
# today's pointer words and the layout classes (section 3.1): counts per class and runs
python - <<'EOF'   # in the worktree root
import collections
c, r = collections.Counter(), collections.Counter()
for l in open('recipes/exe-pointers.tsv'):
    p = l.rstrip('\n').split('\t')
    if p[0].startswith('0x'): c[p[2]] += int(p[1]); r[p[2]] += 1
print(c, r)
EOF

# session scripts, kept in C:\Users\kerok\bof3x-runs\ri1027\ra3 (not in the tree), run from the worktree root:
python <ra3>/image_words.py <BOF3.exe>        # 3.4, 5: aligned words in the image by class, .data and .rdata
python <ra3>/code_consts.py [--list value|compare]   # 4.4: code VAs in src/ by use (comments and strings blanked)
# 4.1, 4.2: exe-layout.tsv's code rows by name / by address, each name's impl (a tomllib read of symbols.toml)
# 1.1, 1.2: the baseline build's PE headers (bof3x.exe 0x400000 + 0x58F000, no .reloc; bof3x.dll 0x10000000)

python tools/data_census.py --counts           # the census numbers cited (27,435 sites, 4,108 at:: definitions, ...)

# the forced-base probe, for the coordinator (stage C, after make_exe.py gains --image-base):
python tools/make_exe.py ... --image-base 0xA00000 --out <dir>/bof3x.exe
BOF3X_SELFTEST_ONLY=1 <dir>/bof3x-launcher.exe --game <game dir> --no-config   # a clean launcher dir, no ini
```
