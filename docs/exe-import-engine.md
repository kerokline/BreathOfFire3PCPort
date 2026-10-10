# `base/exe/`, the engine half: the data pointers, `.rdata`, the unplaced, the map

**Status:** IN PROGRESS (verified 2026-10-10, on the owner's machine: the
seven held discs and `BOF3.exe`, headless self-tests only; branch
`catchup/step8-engine`)

Unified-data step 8's engine half - [`exe-import.md`](exe-import.md) section 6's
worklist, taken in the order [`HANDOFF.md`](HANDOFF.md) gives: the data-pointer
rebuild transform first, then `.rdata`'s reads, then the addresses no disc
carries, then the design of state 3's map ([`platform-layers-plan.md`](platform-layers-plan.md)
section 3) and its first step. Every number below is from a run in this
session; the commands are in section 6. No table value appears here or in the
recipes (CLAUDE.md rule 1): the recipes hold addresses, counts and hashes, the
per-word and per-address lists are in `analysis/exe_import/` (gitignored).

## 0. What came out

1. **The data-pointer rebuild** (section 1), in `tools/exe_tables.py`, applied
   by every disc build of `base/exe/`: a pointer word into `.data` that a
   segment places is the disc's own address; the segments of the same file
   that hold that address give the PC address it means. Words the rule gets
   wrong on a build are listed in the generated `recipes/exe-rebuild.tsv` and
   stay unfilled, so **every rebuilt word is the PC's**. With section 3's
   places: **7,138 of the 9,959 data pointers from the JP disc**, 6,959 from
   the US, 4,327 from PSP-JP. Of the JP disc's data words whose word and
   target are both placed, 98.0 % rebuild exactly (7,138 of 7,282).
2. **`.rdata`** (section 2): of the 65 addresses `src/` names there, 44 - 40
   float and double literals and four interface GUIDs - are the engine's own
   constants and now live in [`src/game/rdata_consts.h`](../src/game/rdata_consts.h);
   the 77 declarations that named them now read the engine's copies, which
   a start-up check compares with `BOF3.exe`'s `.rdata` every run (a Fatal
   on a difference). The other
   21 are the import table (13), the DirectInput keyboard format (1), four
   floats only Capcom's code reads, and three literals that are not `.rdata`
   reads. No behaviour changes.
3. **The unplaced, placed by pointers** (section 3): the twin method keyed by
   the PC's own pointers instead of bytes, `recipes/exe-places.tsv` (184
   places, 11,987 bytes on JP). With the rebuild, **577,657 bytes (90.4 %)
   of the JP-built image equal the PC's**, from 555,969 (87.0 %). The
   owning-function method was measured and not built: 19 of the mixed,
   small and other addresses have a function with a PSX twin to read.
4. **State 3's map** (section 4): the design - an entry-stub page at
   Capcom's addresses, not a rewrite of the 9,142 code-pointer words - with
   its measurements, and its first step built: **`BOF3X_EXEIMAGE`**, which
   checks `base/exe/data.bin` against the running `.data` at start-up and
   lays a PC-built image over it (byte-identical, so a verification, not a
   divergence). Self-tested: a PC-built cache passes, a disc-built one is
   compared (576,301 of 638,976 bytes equal for US, the recipe's count), a
   one-byte-tampered PC cache stops with a Fatal.

## 1. The data-pointer rebuild

### 1.1 The rule

`exe_tables.rebuild_candidates`. For each word of `recipes/exe-pointers.tsv`
of class `data` (the PC's word points into `.data`):

1. The word's four bytes must be placed by one range (a `map` segment, a
   section 3 `place`, or a catalogued `table`); its disc value `w` is an
   address in that build.
2. The image's linear pieces (every `map`, `place` and `table` span, as the
   bytes ended up; `widen` spans are not linear and are left out) of the
   **same file and section** as the word that hold `w` give PC values
   `piece.pc + w - piece.addr`; if none hold it, the pieces of the **same
   file, any section**.
3. Exactly one value: the word is rebuilt to it. Two different values (the PC
   placed the same disc bytes twice) is `ambiguous`; none is `target unplaced`.

`recipe` (which has the PC) compares every rebuilt word with the PC's and
writes the words that differ to `recipes/exe-rebuild.tsv` (build, PC address,
words); a disc build skips them, so the rebuilt words are written into
`data.bin` as `rebuilt` ranges (`from`: the disc file and the disc's word) and
every one equals `BOF3.exe`'s. The image's hash per build in
`recipes/exe.toml` changed with it; `verify` checks it as before.

**Three wider rules were measured and are not tried** (a prototype run on JP
and PSP-JP before the rule was fixed): another file's pieces when neither the
section nor the file holds `w` - 0 of 283 JP words the PC's value (275
ambiguous, 8 wrong: an overlay slot several files load into, a `GAME.EMI`
word pointing at a scene overlay); the resident EXE's pieces from an overlay
word - no word reached that tier; a pointer one past a piece's end - 0 of 1
on JP, 32 of 301 on PSP-JP.

`bss` (147) and `rdata` (63) words are not tried: there is no map of the
discs' `.bss`, and all 63 `.rdata` words lie where no disc places anything
(they are the PC's own `Task_StackTop`-region data).

### 1.2 Per build

The data pointers (9,959 words) after the rebuild and section 3's places:

| Build | rebuilt, the PC's | refused (wrong) | ambiguous | target unplaced | word unplaced | image identical to the PC's |
|---|---:|---:|---:|---:|---:|---:|
| `psx-jp` | **7,138** | 121 | 23 | 638 | 2,039 | **577,657** (90.4 %) |
| `psx-us` | 6,959 | 191 | 95 | 637 | 2,077 | 576,417 |
| `psx-eu-en` | 6,896 | 195 | 97 | 618 | 2,153 | 576,242 |
| `psx-fr` | 6,892 | 195 | 96 | 617 | 2,159 | 576,164 |
| `psx-de` | 6,890 | 195 | 96 | 619 | 2,159 | 576,145 |
| `psp-jp` | 4,327 | 133 | 73 | 3,086 | 2,340 | 566,574 |
| `psp-eu` | 4,348 | 126 | 87 | 3,088 | 2,310 | 566,631 (88.7 %) |

The rebuild alone, before section 3 (the first commit): JP 6,119 rebuilt, 107
refused, 23 ambiguous, 418 target unplaced, 3,292 word unplaced, 574,223
bytes identical; US 5,967 / 177 / 86 / 398 / 3,331, 573,063; PSP-JP 3,804 /
130 / 52 / 2,985 / 2,988, 564,834. Before step 8's engine half the images
were 555,969 (JP) to 553,490 (PSP-EU) bytes identical
([`exe-import.md`](exe-import.md) section 3).

`exe-import.md` 5.3's "93 %" (6,229 of 6,667 placed JP words translate)
counted the map run *forwards* from the PC's value; the rule here runs it
backwards from the disc's word, which is what a disc-only build can do, and
gives the same 6,119 + 107 = 6,226 resolved words on the maps alone.

### 1.3 What is not rebuilt, by group (`analysis/exe_import/<build>.rebuild.tsv`)

- **Refused, JP 121**: 117 point at 117 zero-filled four-byte objects in
  `0x6758E0..0x675EDC`, past the last named table of `.data` (every PSX
  build; the disc's word points at the area's own bytes, the PC's at a
  pooled zero object the port's linker put there) - layout no disc carries;
  4 others. **US and the other Western builds add 70**: `Area175_ScriptMessages`
  pointing into itself, the US script's own layout (language).
- **Word unplaced, JP 2,039**: 899 in the PC's sound-file name table
  (`MasterGrant_Steps`+ ... pointing at `SND\%s.DAT`-style names after
  `Music_LoadedTrack`) and 742 in `Dat_FileNames`+ (the DAT names) - the
  PC's file layer, the engine's own by nature; the rest small groups
  (`Area` 114, `Math_SinTable`+ 60, `Battle` 43, `Menu` 40,
  `BattleMenuWin` 38, ...).
- **Target unplaced, JP 638**: `Area` 502, `Scenario` 39, `Effect` 36,
  `Menu` 22, `BossMyria` 16 - pointers into data the maps do not place.
- **The PSPs' 3,086 target unplaced**: the ELF's words point into the ELF's
  own copies, which the maps take from the EMIs (`exe-import.md` 5.3).

So of the 8,318 data pointers that are game data (all but the file layer's
1,641), the JP disc rebuilds 7,138 (85.8 %).

### 1.4 Checks

- `exe_tables.py check` (and so `importer.py check`, CI): `exe-rebuild.tsv`
  hashes as `recipes/exe.toml` records, every refused word is a `data`
  pointer of `exe-pointers.tsv`, each build's refusals match its count, its
  rebuilt bytes are 4 x (rebuilt - refused) words, the counts add up to the
  image; and **a synthetic round trip of the rule** (`selftest_rebuild`: two
  overlay sections, a boot EXE, a duplicated piece, an unplaced word, another
  file's word - no game data).
- `measure` fails if `exe-rebuild.tsv` is stale against the PC (a word the
  rule now rebuilds wrong that the file does not list, or the reverse).
- Proved: `exe_tables.py build` + `verify` from the US disc, PSP-JP and
  `BOF3.exe`; the rebuilt words of the US and PSP-JP images compared with
  `BOF3.exe` outside the tool (5,967 and 3,804 words at the first commit, 0
  different); `importer.py build --source <US cue>` + `verify`: `base/exe/`
  "the image recipes/exe.toml records for it" (the run's one problem is
  `opt/area4-walls`' slot 6, which this branch does not touch);
  `BOF3X_EXEIMAGE` in process (section 4.4).

## 2. `.rdata`'s 65 addresses

`exe_tables.py xref`'s `rdata` class: 3 symbols and 62 raw constants at
`exe-import.md`'s writing (63 raw now: section 2.2's `pad_read.cpp` cites
`0x5C4828` itself). Read from `src/`, `symbols.toml` and `BOF3.exe`'s bytes
(the value at each address read with `exe_tables.pe_sections`; GUIDs by
`uuid.UUID(bytes_le=...)`):

| What | Addresses | Settled |
|---|---|---|
| Float and double literals our code reads | 40: `0x5C41B8..0x5C427C` (30 floats: 1.0, 3.0, 2.0, 4.0, 8.0, 16.0, 32.0, 0.5, 0.0, 0.0625, 64.0, 48.0, 24.0, 37.0, 12.0, 300.0, -150.0, 420.0, -100.0, 380.0, -60.0, 340.0, -20.0, 89.0, 90.0, 512.0, 13.0, 40.0, 88.0, 127.0), `0x5C4608..0x5C4658` (65536.0, 1/320, 0.1, 1/128, the double 1/32, 1/255, 10000.0, 1/127, the doubles 1/3.14 and 2048.0) | **the engine's own**: the 2001 source's literals |
| Interface identifiers | 4: `0x5C4488` IID_IDirect3DTexture2, `0x5C45B8` IID_IDirectSoundNotify, `0x5C4718` IID_IDirectInputDevice2A, `0x5C4828` GUID_SysKeyboard (`DInput_KeyboardGuid`) | **the engine's own**: the SDK's public GUIDs |
| The import address table | 13: `Imp_GetTickCount` `0x5C407C` and `0x5C4020`/`24`/`28` (GDI32 SetTextColor, SetBkMode, TextOutA), `0x5C4080`/`84`/`88` (CreateEventA, CloseHandle, GetDriveTypeA), `0x5C40B8`/`0x5C4114`/`0x5C4158` (ExitProcess, TerminateProcess, PostQuitMessage), `0x5C4164`/`68`/`74` (MsgWaitForMultipleObjects, the slot `gfx_texcache.cpp` tests, MessageBoxA) | **the loader's**: state 3's executable imports its own; today the slots are read as bound, kept |
| `DInput_KeyboardFormat` `0x5C4948` | 1: `c_dfDIKeyboard`, whose object table and its GUIDs are more `.rdata` | the SDK's (`libdinput`'s `c_dfDIKeyboard`) in state 3; kept the exe's today (section 5) |
| Floats only Capcom's code reads | 4: `0x5C4230`, `34`, `3C`, `40` (370.0, -50.0, 520.0, -200.0) | not reads of ours: `widescreen.cpp` checks that the operands at `0x5109BB`.. name them before re-aiming them; moot in state 3 |
| Not `.rdata` reads | 3: `0x5C4000` (`win_main.cpp`'s bound of `.text`), `0x5C8000` and `0x5D0000` (field coordinates in `scena_sc9b.cpp`, `area_w3f.cpp`, `area_w4b.cpp`) | nothing to hold |

### 2.1 What was built

[`src/game/rdata_consts.h`](../src/game/rdata_consts.h) /
[`.cpp`](../src/game/rdata_consts.cpp): the 44 constants as a `constexpr`
table, each with its PC address (a load-bearing constant: it names the copy
it replaces), its value written as the literal (`F(0x5C41CC, 8.0f, ...)`, a
GUID in the SDK's notation), and the engine's copies laid out at the same
offsets from `0x5C41B8` (a double stays 8-aligned). `rdata::Const` is the
address type: its constructor is `consteval` and refuses, at compile time,
an address the table does not hold; it converts to the address of the
engine's copy. So the 77 declarations that named these addresses
(`constexpr std::uint32_t kQuadEight = 0x5C41CC;` in 33 files) became
`constexpr rdata::Const kQuadEight{0x5C41CC};` and their readers -
`At(at::kQuadEight)`, `F(at::kZero)`, the inline-asm operands of
`sound.cpp` - read the engine's copies without changing shape (five sites in
`sound.cpp` / `sound_rest.cpp` needed an explicit cast; `pad_read.cpp`'s
GUID read names `0x5C4828` through a `Const`).

**The verification**: `rdata::Verify()`, `InjectAll`'s first step, compares
every copy with `BOF3.exe`'s bytes and stops with a Fatal on a difference
(`rdata: 44 constants (1.0f .. GUID_SysKeyboard ...) held by the engine,
each equal to BOF3.exe's .rdata` in every self-test log since). The fuzz
harnesses that run Capcom's clones beside ours now compare a clone reading
`.rdata` with ours reading the copy - a real cross-check. Behaviour is
unchanged by construction (the same bits from another address); the full
`BOF3X_SHADOW='*'` self-test passed at the tip (section 6).

Left as the exe's: the import slots, `c_dfDIKeyboard`, and the GUID-free
helpers that compare `.rdata` bytes in the fuzz files (`glyph_draw_fuzz.cpp`
reads `0x5C4618` raw to check the original's value).

## 3. The addresses no disc carries

### 3.1 The twin method keyed by pointers

`exe_tables.find_places`, run by `recipe` (it needs the PC), written to
`recipes/exe-places.tsv` and applied by `disc_image` after the maps'
segments and before the catalogued tables (`place` ranges in `data.toml`):

1. Every unplaced region of the image (no segment, no table) holding a PC
   pointer word that can be keyed: a **data pointer** whose target the image
   places (the target's address in the build, by the map run forwards), or a
   **code pointer the image has already paired** - every placed code-pointer
   word pairs (its file, the PC's function) with the build's function, kept
   where every such word of that file agrees.
2. Every 4-aligned hit of a key's build word in a section of the key's file
   votes for the region's place there (file, section, displacement). The
   data keys' votes decide when there are any; the best place must have more
   votes than the next.
3. The best place is checked against the PC byte by byte: numbers equal; a
   data pointer the target's address in the build; a paired code pointer its
   pair, an unpaired one a code address of the build (`0x80010000..` PSX,
   `0x08800000..` PSP); `bss` / `rdata` pointers wild. The longest stretch
   with no disagreement holding two keys (or one key and 16 equal bytes) is
   placed.
4. New places give new targets and new pairs: it runs again (at most eight
   rounds) until nothing moves.

The pointer decision (`decide_pointers`) stays the maps' alone, so
`recipes/exe-pointers.tsv` is unchanged (its hash in `exe.toml` is the
same). Every placed number byte equals the PC's: `placed_differ` is the same
on every build before and after (3,139 JP).

| Build | places | bytes | number bytes no disc carried, now placed (`none_pc_nonzero` before - after) |
|---|---:|---:|---:|
| `psx-jp` | 184 | 11,987 | 403 (22,734 - 22,331) |
| `psx-us` | 192 | 11,915 | 403 |
| `psx-eu-en` | 196 | 11,329 | 403 |
| `psx-fr` | 195 | 11,305 | 403 |
| `psx-de` | 195 | 11,305 | 403 |
| `psp-jp` | 196 | 5,008 | 182 |
| `psp-eu` | 201 | 5,308 | 184 |

Most of what it places is pointer words: 1,253 JP data-pointer words that
were unplaced are now placed (3,292 to 2,039), and 1,019 more rebuild. The
code-pointer keys (the second commit) added 51 places on JP over the
data-pointer keys alone (133, 9,911 bytes) and 110 number bytes; on every
build both counts are at or above the data-pointer-only pass.

`exe_tables.py check` validates the file: its hash, its rows in order inside
`.data`, keyed, and over no `exe_maps/` segment of the build.

### 3.2 The addresses `src/` reads, before and after

`src/` has grown since `exe-import.md` was measured (2,568 raw constants
then, 2,672 at this branch's start), so the "before" is re-measured on
today's `src/` with neither the rebuild nor the places
(`xref` with `disc_built` replaced by `disc_image` + `mask`). JP:

| Class | symbols before | after | raw before | after |
|---|---:|---:|---:|---:|
| `none` | 67 | **56** | 338 | 337 |
| `none+pointer` | 11 | 10 | 3 | 3 |
| `partly` | 26 | 22 | 8 | 8 |
| `pointer:data` | 24 | 16 | 101 | 91 |
| `reproduced` | 569 | 595 | 555 | 570 |
| `reproduced+pointer` | 27 | 25 | 7 | 3 |

By group, `none` JP (symbols and raw): mixed 122 to 120, other 119, language
78 to 74, small 42 to 37, re-laid 15 to 14, the platform groups unchanged
(29). So the pointer keys moved few *addresses*: what `src/` reads in the
mixed and other groups is mostly in regions with no usable key. Read by the
unplaced region holding them (`regions_left`, a session script: 98 regions
hold the 436 none / none+pointer / partly addresses):

- `0x669CCC..0x66A450` holds 119 (from past `Encounter_SlotChance`'s 28
  bytes: `Battle_CommandLabels` and their pointers, the community's member
  names, `Menu_Verbs` - mostly text the port re-laid) and
  `0x6531F4..0x6536E0` 64 (the page-format and glyph tables `rest_4e.cpp`
  names - "page / pages"): 183 of the 436, mostly language data a language
  layer carries, mis-grouped as `mixed` / `other` by
  `exe_tables.group_of` because their symbols' names say nothing of text.
- The effect state tables (`EffectKind18Sub2E_States`+, `EffectKind34_V1States`+,
  ...): regions of 20..240 code-pointer words and a few number bytes whose
  code pointers no placed word pairs - the next method's (3.3).
- The platform regions (`Cfg_Fullscreen`+, `Window_Handler8KindTable`+,
  `Dat_FileNames`, the sound-name table) and the plate tables DIV-0055 re-laid.

### 3.3 The owning-function method, measured and not built

For each JP none / none+pointer / partly address, the PC functions that name
it (`analysis/pc_xref.json`, `tools/pe_xref.py`'s index) and whether any of
them has a PSX twin in `symbols.toml` (`psx`): mixed 14 of 141 have one,
other 5 of 120, small 0 of 49, language 11 of 85, platform 2 of 34
(54 mixed, 90 other, 28 small have no indexed reader at all; measured after
the rebuild, before the places). 1,528 of the
10,099 functions carry a `psx` address and most are resident; the unplaced
data is overlay data. So the method reaches about 19 of the mixed / small /
other addresses today; it grows with the function twins, not with this
step. The code-pointer pairs of 3.1 are a function-twin table the image
learns for itself (every placed code-pointer word is one pair); feeding
them back into `symbols.toml`'s `psx` fields, per file, is the cheap way to
widen it.

## 4. State 3's map: the design

### 4.1 What has to be owned

`exe-import.md` 6 item 1, re-measured (`codeptr.py` / `codeptr2.py`, session
scripts over `exe-pointers.tsv`, `symbols.toml` and `BOF3.exe`):

- **9,142 `.data` words point into `.text`, at 6,346 targets.** 6,211
  targets (8,673 words) are `[[func]]` starts, every one with `impl` (ours).
  135 targets (469 words) lie inside a function; **462 of those words are
  where no disc places anything** (312 in `Dat_FileNames`+, the PC's file
  table, all pointing inside one function, `Scena03_Scene7`; 98+ in the
  `Task_StackTop`+ region) - `decide_pointers` calls a pointer-shaped word a
  pointer by default when no disc can test it, so these are likely numbers
  that look like addresses; not read further. **7 words that a
  disc places point inside a function**: the real cases to read.
- By placement: 4,886 + 7 code-pointer words some disc places, 3,787 + 462
  no disc places (the PC's own data - the file layer, the port's tables).
- `.rdata` holds 127 words into `.text` (81 targets, none a `[[func]]`
  start; not read further) - not mapped in state 3.
- `src/` names 10,254 distinct `.text` addresses (47,621 uses), 10,026 of
  them function starts and 6,194 also targets of `.data` pointers: detour
  sites, `CallSite` keys, clone bases, and handler addresses our code
  writes into the game's records.
- 10,099 `[[func]]` starts, 10,082 with `impl`; the 17 without are the MP3
  decoder (8), the C runtime (6), `Gfx_PackRgb`, `MoveCmd_Move`,
  `Fmv_EnterFullscreen` - state 2's work. **The smallest gap between two
  starts is 11 bytes**, so a five-byte jump fits at every start.

### 4.2 Two ways, and the choice

**(a) Rewrite the words to our bindings.** At map time each code-pointer word
`T` becomes `&ours(T)` (`symbols.toml`'s `impl` binding, which
`gen_symbols.py` already emits). Clean in the end state - no fixed `.text`
range - but every other holder of a Capcom address must follow: the 6,194
`.data` targets `src/` also names as literals, every handler address our
code stores into a task or a record, every compare of a stored handler with
a Capcom address (`if (task->fn == 0x...)`), the `.bss` words written at run
time, and the 462 + 7 words inside functions, which have no binding to
rewrite to. A word missed is a silent fork (rule 4's failure mode), and the
words no disc places cannot be told from numbers.

**(b) An entry-stub page at Capcom's addresses** (recommended). Our
executable reserves `0x401000..0x5C4000` and writes, at each of the 10,099
`[[func]]` starts, `jmp ours` - exactly the jump `Inject` writes today, on a
page with no Capcom bytes beneath. Every code pointer, wherever it lives
(`.data`, `.bss`, our literals, the game's records), keeps meaning what it
means today; nothing is rewritten, nothing compares differently, and a
pointer-shaped number that is not a pointer is left alone because nothing
touches it. Needed: the 17 functions without `impl` owned first (state 2);
the 7 placed mid-function words read (a missing `[[func]]` start, or a label
our function does not need); a stub at any further address the tracer sees
reached; the page made read-execute after writing. Cost: 1.8 MB of address
space (`0x401000..0x5C4000`), 50 KB of stubs.

(b) is state 3 as a strict continuation of state 2 - the detours become the
executable's own - and (a) remains open for later, one table at a time,
when a reason (64 bits, another system: [`PLAN.md`](PLAN.md) phase 4) makes
the fixed `.text` range a cost. The data side does not depend on the
choice: `base/exe/data.bin` is mapped at `0x5DA000` either way.

### 4.3 The data map

State 3's loader, from `base/exe/data.toml`: reserve `0x5DA000..bss_end`
(`0x93D6EC`, rounded to the page), copy `data.bin` (0x9C000 bytes) to
`0x5DA000`, leave the rest zero (the loader's `.bss`). From the PC's exe the
image is the section and nothing else is needed for `.data`. From a disc:
the unfilled words - the code pointers (with (b) they are the PC's values,
which a disc cannot give: the engine holds them as layout, like
`symbols.toml`, or reads them from the PC's exe; that is the owner's call
already listed as `exe-import.md` 8's call 4, now for code pointers too),
the refused and unrebuilt data pointers, and the 22,331 non-zero bytes no
disc carries (JP). Until those are held, a disc-built image is not
runnable, and `BOF3X_EXEIMAGE` refuses to lay one over `.data`.

### 4.4 The first step, built: `BOF3X_EXEIMAGE`

[`src/hook/exe_image.cpp`](../src/hook/exe_image.cpp), called from
`DllMain` right after `VerifyImage` and before `InjectAll` (so before anything
of ours writes `.data`). `BOF3X_EXEIMAGE=<cache>` (the directory
`importer.py build` or `exe_tables.py build` wrote); unset, nothing.

1. `base/exe/data.toml`'s `va`, `size` and `bss_end` against the running
   image's `.data` section header (`0x5DA000`, raw `0x9C000`, virtual end
   `0x93D6EC`).
2. `.bss` zero from the raw end to `bss_end` (the loader's, which state 3's
   engine will have to do itself).
3. Every byte of `data.bin` against the running `.data`, counted by
   `data.toml`'s ranges (which must cover the image in order).
4. Built from `BOF3.exe` (`build = "pc-zh"`): every byte must be equal - a
   Fatal naming the first difference otherwise - and the image is then
   copied over `.data`, changing no byte: the map step, exercised. A
   disc-built image is compared and logged, never copied.

Self-tested headless (`BOF3X_SELFTEST_ONLY=1`, no ini):

| Cache | Exit | Log |
|---|---|---|
| from `BOF3.exe` | 0 | `exe 638976 of 638976 bytes equal`; `base/exe/data.bin (pc-zh) laid over .data at 0x5DA000: 638976 bytes, byte-identical; .bss zero to 0x93D6EC` |
| from the US disc | 0 | `map 468144 of 468241`, `place 3611 of 3611`, `table 9518 of 9518`, `widen 10067 of 13132`, `rebuilt 27828 of 27828`, `pointer 13026 of 49416`, `none 44107 of 67230`; `compared, not mapped: 576301 of 638976 bytes equal` - the in-process count is `recipes/exe.toml`'s `identical` for `psx-us` |
| from `BOF3.exe`, one byte of `data.bin` flipped | 3 | `FATAL: ... 1 of its 638976 bytes differ from the running .data, the first at 0x5DB234` |

It is a verification, not a divergence: no ledger entry (with the switch
unset nothing runs; with it set, the only write is the same bytes). It is
the check the state hash's A/B will want too: an image the engine maps is
the image the importer wrote.

## 5. What is left

1. **The data pointers that do not rebuild** (JP 2,821 of 9,959): the
   file layer's 1,641 are the engine's own; the 117 pooled zero objects are
   layout (hold them, as `exe-pointers.tsv` holds addresses - the owner's
   call 4 of `exe-import.md` 8); the rest wait on more places.
2. **The 147 `.bss` and 63 `.rdata` pointer words**: no disc map of either;
   the engine's own layout, held.
3. **The code pointers** (9,142 words): section 4.2 (b) - the 17 functions
   without `impl` (state 2), the 7 placed mid-function words read, then the
   stub page in our own executable. Not started. The decoder's eight are
   to be minimp3 behind the music seam ([`mp3-decoder-choice.md`](mp3-decoder-choice.md),
   the owner 2026-10-10).
4. **`c_dfDIKeyboard`** from the SDK in state 3 (one more `.rdata` read
   that the engine can own: `libdinput`'s object, verified against the
   exe's like 2.1); the import slots go with the executable.
5. **The 436 addresses `src/` reads that no disc carries** (JP): 183 are
   language data (3.2); the platform groups are the engine's; the effect
   state tables want function twins (3.3: the image's own code-pointer pairs
   into `symbols.toml`'s `psx`), then a re-run of `recipe`.
6. `exe_tables.group_of` would class the 183 better by region than by
   symbol name.
7. The PSPs: the ELF's data pointers point into the ELF's copies, which the
   maps take from the EMIs; a second map of the ELF's own copies would let
   the rule rebuild them (3,086 target unplaced on PSP-JP).

## 6. Commands

```
D="--disc <JP cue> --disc <US cue> --disc <EU cue> --disc <FR cue> --disc <DE cue> --disc <PSP-JP iso> --disc <PSP-EU iso>"
python tools/exe_tables.py recipe  --game <dir with BOF3.exe> $D     # exe.toml, exe-pointers.tsv, exe-places.tsv, exe-rebuild.tsv (~80 s)
python tools/exe_tables.py measure --game <dir> $D --out analysis/exe_import   # sections 1.2, 1.3 (+ <build>.rebuild.tsv)
python tools/exe_tables.py xref    --game <dir> $D --out analysis/exe_import   # section 3.2
python tools/exe_tables.py build   --source <disc or BOF3.exe> --out <cache>; python tools/exe_tables.py verify --cache <cache>
python tools/exe_tables.py check; python tools/tables.py check; python tools/importer.py check
python tools/importer.py build --source <US cue> --out <cache>; python tools/importer.py verify --cache <cache>

cmake --preset i686 && cmake --build build
# a copy of build/bof3x-launcher.exe + bof3x.dll in a directory with no bof3x.ini:
BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' ./bof3x-launcher.exe --game <dir> --no-config        # exit 0 (18 min)
BOF3X_SELFTEST_ONLY=1 BOF3X_EXEIMAGE='<cache, a Windows path>' ./bof3x-launcher.exe --game <dir> --no-config
```

The section 3.2 "before" ran `xref` with `exe_tables.disc_built` replaced by
`disc_image` + `mask` (a session script); section 3.3's reach and section
4.1's counts are session scripts over the same inputs (`analysis/pc_xref.json`,
`exe-pointers.tsv`, `symbols.toml`).

## For the other files

Not edited here (the coordinator folds them in):

- **`docs/STATUS.md` / `docs/HANDOFF.md`**: step 8's engine half - the
  data-pointer rebuild and the places built (JP 90.4 % of the image the PC's,
  7,138 of 9,959 data pointers), `.rdata`'s 44 constants the engine's own,
  state 3's design chosen (the entry-stub page) and `BOF3X_EXEIMAGE` built;
  left: section 5. HANDOFF's item 3 first bullet becomes section 5's items
  3 and 5.
- **`docs/platform-layers-plan.md`** section 3, state 3: "the engine
  rewrites those words to its own functions or maps entry stubs" - the
  stubs, measured (section 4 here); `.rdata`'s 65 are now 44 the engine's,
  13 the loader's, 1 the SDK's, 7 nothing to map.
- **`docs/unified-data-plan.md`** section 8 row 8: the engine half started
  (this doc).
- **`docs/importer.md`**: `build` rebuilds the data pointers and applies the
  places from a disc; `check` covers the two new recipe files.
- **`docs/SCAFFOLDING.md`**: `BOF3X_EXEIMAGE` among the switches.
