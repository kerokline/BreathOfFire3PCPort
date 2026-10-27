# `base/exe/`, the engine half: the data pointers, `.rdata`, the unplaced, the map

**Status:** IN PROGRESS (merged: PR #49, section 7 with PR #55; live play owed. Verified 2026-10-10, on the owner's machine: the
seven held discs and `BOF3.exe`, headless self-tests only; branch
`catchup/step8-engine`; section 7 measured 2026-10-12, branch
`disc-only/pointers`)

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
reads `0x5C4618` raw to check the original's value). Since the disc-only
round, section 2.2.

### 2.2 What is left of the file (disc-only round, group RD, 2026-10-12)

**The exe host reads no byte of `.rdata` from `BOF3.exe`.** `exe_host.cpp`
step 3 no longer copies the section; `rdata::Map` writes the 44 constants
at their PC addresses and, at `DInput_KeyboardFormat` `0x5C4948`, the SDK's
`DIDATAFORMAT` header; `BindSlots` writes the 14 import slots. The rest of
`0x5C4000..0x5DA000` stays zero. What else could read it, measured:

- **The 63 `.data` words that point into `.rdata`** (`recipes/exe-pointers.tsv`,
  class `rdata`, read against `BOF3.exe` with a session script):
  all 63 are Capcom's C runtime's - printf's two
  `(null)` strings (`0x672948`, `0x67294C`), the 18 `R6xxx` / math-error
  message pointers (`0x675134..0x6751BC`) and the 43 locale day and month
  names and formats (`0x675528..0x6755D0`). No line of `src/` names any of
  the three runs (a grep for `0x6729xx`, `0x6751xx`, `0x6755xx`), and the
  runtime they belong to is not ours to run.
- **The import slots**: every IAT address `src/` names is one of the 14
  `BindSlots` binds (state3-build.md 2.2's list, the grep repeated:
  `0x5C4014..0x5C4174`, each named twice - the slot table and its one
  reader); none reads a neighbouring slot.
- **The four floats only Capcom's code reads** (`0x5C4230..0x5C4240`):
  `widescreen.cpp` and `rest_3g_callees.h` name them in comments and compare
  the operands that name them (code-byte reads, OP's), never their values.
  Nothing to hold.
- **The fuzz files' raw reads** (`glyph_draw_fuzz.cpp`'s `0x5C4618`, the
  shell fuzz's clones): only under `BOF3X_SHADOW`, which the exe host
  refuses at step 1. Hosted only, as before.

**`c_dfDIKeyboard` is the SDK's**, linked from llvm-mingw's `libdinput.a`
(with `GUID_Key` from `libdxguid.a`; static data, no import of
`dinput.dll`). `pad_read.cpp` passes `rdata::KeyboardFormat()` in both hosts.
`rdata::Verify` compares it with Capcom's: the header's five counts
(`0x18`, `0x10`, `DIDF_RELAXIS`, 256, 256) and each of the 256 objects'
offset, type, flags and GUID by value - `rdata: the keyboard format is the
SDK's c_dfDIKeyboard, equal to BOF3.exe's at 0x5C4948 (its 256 objects at
0x5ACBD0, their GUIDs by value)` in every self-test below. **Capcom's object
table is at `0x5ACBD0`, inside `.text`** (the header's `rgodf`, read from the
file; `symbols.toml`'s `0x5AC850` was wrong and is corrected), so in the exe
host as merged in PR #53 the header copied from the file pointed at
`.stub`'s int3 fill. A scratch program (session 24422d10) measured what
DirectInput 7 does with such a table: `SetDataFormat` `E_INVALIDARG`,
`Acquire` `E_INVALIDARG`, `GetDeviceState` `DIERR_NOTINITIALIZED`, against
`DI_OK` three times for the SDK's. So `bof3x.exe` has had no DirectInput
keyboard since state 3 (the pad, through SDL, and the input script were
unaffected); this round's change restores it. A live check is owed: a key
press in `bof3x.exe`.

**The verifications against the file have one predicate**, `ExeFile_Present()`
(`hook/exe_host.h`): always true hosted; in the exe host, whether the game
directory held `BOF3.exe` at start. `ExeFile_Read(va, to, n)` reads the
file's `.text`, `.rdata` or `.data` by address (exe host only). Without the
file `rdata::Verify` and the key table say in the log that they did not
compare.

**`BOF3.exe` is optional in the exe host** when `BOF3X_EXEIMAGE` names the
`.data` source: no hash, no reads, `data: no BOF3.exe in <dir>: a
disc-built install - .data from BOF3X_EXEIMAGE's cache, nothing read from
the file, and the checks against it skipped`. Without the file and without
the variable, a Fatal. What still needs the file in this branch: the
code-byte store (`code_bytes.cpp`, 43 reads, OP's), whose first read is a
Fatal without it.

**The default key table is a rule** (the owner, 2026-10-11):
`input::DefaultKeys()` (`src/input/bindings.cpp`), 24 entries by `DIK_` name
(docs/controls.md section 1). `Shell_MapDefaultKeys` writes it to
`Key_TableDefault` `0x66C648` at map time (the exe host after `.data` is
placed; hosted in `Shell_Inject`, before the fuzz), after comparing it with
the file's 0x80 bytes when there is one; `Cfg_SetDefaultKeys` copies the
rule; the shell fuzz holds the region at the rule. **`0x66C648..0x66C6C8`
is the engine's own** - for NB's list and a disc-built image's skip ranges.

Self-tests (ini-less copies of this branch's build, headless):

| Run | Exit | Log |
|---|---|---|
| exe host, the real install | 0 | `keys: Key_TableDefault 0x66C648 written from the engine's rule (24 keys), equal to BOF3.exe's`; both `rdata:` lines; `inject: 10087 ours` |
| exe host, `BOF3X_EXEIMAGE` = `analysis/cache/pc-plus-us` (pc-zh) | 0 | the same, `.data ... from BOF3X_EXEIMAGE's data.bin` |
| exe host, a scratch game directory with `DAT/` (a junction) and no `BOF3.exe`, the same cache | 3 | `data: no BOF3.exe ...`; `keys: ... no BOF3.exe to compare it with`; `rdata: ... no BOF3.exe to compare them with`; then `FATAL: codebytes: 0x00497A22 read before the player's BOF3.exe was opened` - the store, OP's |
| the same with the store pointed at the real file (a scratch build, not committed) | 0 | `inject: 10087 ours`, `self-test only: done`: the store is the only read of the file left |
| hosted, `BOF3X_SHADOW=shell,pad_read,field_blocked` | 0 | `shell self-test: 21000 rounds ..., 0 MISMATCHES`; `pad_read shadow: 20000 rounds, 0 differ`; `field_blocked self-test: 45000 rounds ..., 0 MISMATCHES` |

The launcher (`--no-config`, the real install, `BOF3X_SELFTEST_ONLY`) prints
the 24 keys it compared: `the default key table, 24 keys by the engine's
rule, equal to BOF3.exe's: Up=up Down=down ... Numpad3=down+right`.

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
- **2026-10-10: 12 without `impl`.** `MoveCmd_Move` `0x578C10` is ours
  (`move_cmds.cpp`, [`move-cmds.md`](move-cmds.md) section 8), as are four of
  the six C runtime entries since the count above; the twelve left are the
  MP3 decoder (8, DIV-0091), `Crt_ftol`, `Crt_GetPtd`, `Fmv_EnterFullscreen`
  and `Gfx_PackRgb`. `tools/owned_sweep.py` lists which of them our code
  still names or calls: the decoder's eight (`sound.cpp`'s table, reached only
  under `BOF3X_MP3=original`), `Crt_ftol` (fuzz clones' callee rows) and
  `Crt_GetPtd` (`crt_rest.cpp`'s `Rand` seed instrument).

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
our function does not need - read 2026-10-10: neither, all seven are numbers,
[`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md)); a stub at any further address the tracer sees
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
runnable, and `BOF3X_EXEIMAGE` refuses to lay one over `.data`. **Held since
the disc-only round** (section 7, [`exe-engine-own.md`](exe-engine-own.md));
the exe host maps a disc-built image (4.4, steps 5..8).

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
   disc-built image is compared and logged, never copied - **in the hosted
   build**; `bof3x.exe` maps it (steps 5..8).

**The exe host, a disc-built image (the disc-only round, IN, 2026-10-12).**
The refusal is gone. `ExeImage_Source` (`exe_host.cpp`'s `CopyData`) checks
instead:

5. Every range of `data.toml` is a class the loader accounts for
   (`kClasses` in `exe_image.cpp`): `map`, `place`, `table`, `widen`,
   `rebuilt`, `layout`, `own`, and for what no disc fills, as
   `recipes/exe-gaps.tsv` names it (`exe_tables.py gaps`, from
   `exe_own.py measure` on the seven discs): `language` (a language layer's
   handlers write it or our code reads around it,
   [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4), `unread`
   (nothing the exe host runs reads it), `keys` (`Key_TableDefault`, the
   engine's rule), `gap` (zero where the PC has a value: Capcom's game values
   no disc carries, bytes on the disc the places miss, DIV-0055's plates, LG's
   eight language addresses - `from` names the kind and symbol), and `none`
   (the PC's bytes are zero there too, by the table). A `pointer` range, or an
   image written before the table (no `gaps =` line), is a Fatal: its `none`
   could hide a PC value (the no-stub rule).
   **An outdated image (D305, 2026-10-16).** `FilledSince` in
   `exe_image.cpp` lists bytes that every disc build has carried since an
   importer fix. An image whose `data.toml` still calls one of them a `gap`
   was written before that fix, and is refused with a message box telling the
   player to rebuild the install. Today it lists one fix: `WorldMap_Records`'
   eleven area words (`0x653910 + 0x1C*k + 0x18`, the `wmrec` rule of
   2026-10-12 evening, [`visual-findings-1012.md`](visual-findings-1012.md)
   section 8). A cache built before that rule crashed entering area 16 or 45:
   `WorldMap_RecordIndex` answered the "none" record, and its effect kind 1
   called `MoveCmd_AttachOffset` with the area descriptor's null `+0x0C`.
   That was D305, a PSX-US cache of 2026-10-12 17:35. Add a row when an
   importer fix changes what a disc build must hold. Only the stale shape
   (`gap`) is refused, so caches built after the fix keep working.
6. The copy, then `ExeOwn_Fill` (NB's regions); each `gap` range logged once
   with its symbol and size, the counts by class logged;
   `BOF3X_EXEIMAGE_STRICT=1` makes the first gap a Fatal.
7. `Shell_MapDefaultKeys` (RD's), then `ExeImage_CompareWithFile`: with
   `BOF3.exe` there, the filled image against the file's `.data`, every
   differing range logged with its class; a difference in `none`, `own`,
   `keys`, `layout`, `rebuilt` or `exe` is a Fatal - so with the file present
   the table's completeness is proved, not assumed. Without the file, a line
   says it did not compare.
8. `ExeOwn_CheckAtStart` (InjectAll step 2) verifies the regions against the
   file's `.data` when a disc image is mapped (against the running image it
   would compare the fill with itself), and says so without the file.

The table `recipes/exe-gaps.tsv`, per build (US): 663 rows; the PC's non-zero
bytes there `gap` 3,358 (game-value 2,196, disc-unplaced 818, language-zero
238, port-relaid 106), `language` 1,394, `unread` 3,932, `keys` 48.
`exe_tables.py check` (CI) holds it to the recipe's disc builds, in order,
inside `.data`, clear of the engine's regions and every pointer word.

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

Self-tested headless, the exe host (2026-10-12, `disc-only/integration`):

| Install, cache | Exit | Log |
|---|---|---|
| the real install, the US cache (`psx-us`) | 0 | `gap 456 ranges 5455 bytes`; `against BOF3.exe`: `map` 13 ranges / 104 bytes differ, `widen` 6 / 3,065 (the blank names), `language` 99 / 1,394, `unread` 107 / 3,932, `gap` the rest; every must-equal class 0; `627123 of 638976 bytes equal`; `exe_own ... each equal to BOF3.exe's .data (the file; the running .data is a disc-built image)` |
| the disc-only scratch install (no `BOF3.exe`), the US cache as `BOF3X_CACHE` and `BOF3X_EXEIMAGE`, `BOF3X_LANG=en-US` | 0 | `no BOF3.exe to compare the disc-built image with`; `exe_own: 17 regions, 20866 bytes filled over a disc-built image; no BOF3.exe to verify them against`; `inject: 10088 ours` |
| the real install, `analysis/cache/pc-plus-us` (pc-zh) | 0 | unchanged: `exe_own ... each equal to base/exe/data.bin (pc-zh)`, `inject: 10087 ours` |
| hosted, `BOF3X_SHADOW=shell,exe_own,dat_cache`, the US cache | 0 | compared, not mapped: `612176 of 638976` (the hosted build's line, with the new classes counted) |

The gate's skip list: `exe_tables.py gate-skip --cache <disc-built cache>
--game <dir with BOF3.exe>` writes `<cache>/base/exe/gate-skip.txt`, the
committed `tools/statehash_skip.txt` plus every range whose bytes the
disc-built image does not share with the PC's at the map: the `language`,
`unread` and `gap` ranges whole, and the runs of `map` / `place` / `table` /
`widen` bytes that differ from the file's (US: 1,222 ranges, 48,955 bytes).
The engine-filled `layout`, `own` and `keys` are left hashed: step 7 proves
them equal at the map, and skipping them would blind the hash to the game's
own writes there (`Music_LoadedTrack`, the config words); `--with-filled`
adds them.

The gate's pair (2026-10-12, attract, 6 minutes, details in
[`disc-only-install.md`](disc-only-install.md) 5): the disc-only install
against the PC-plus-US cache over the real install, the same scenes; the
state hash identical at tick 0 and first differs at tick 1 (the VRAM shadow
and page `0x903000`), then the field scene's pages from tick 1310 - the data
the two sources load, not the `.data` image, which the skip list covers.

## 5. What is left

1. ~~**The data pointers that do not rebuild** (JP 2,821 of 9,959): the
   file layer's 1,641 are the engine's own; the 117 pooled zero objects are
   layout (hold them, as `exe-pointers.tsv` holds addresses - the owner's
   call 4 of `exe-import.md` 8); the rest wait on more places.~~ Held as
   layout (section 7, 2026-10-12): every one of the 9,959 is in
   `recipes/exe-layout.tsv`, and a disc build fills the ones it does not
   rebuild from there (JP 2,819, US 2,998). More places still shrink what
   the rebuild leaves to the table, and are no longer needed for it.
2. ~~**The 147 `.bss` and 63 `.rdata` pointer words**: no disc map of either;
   the engine's own layout, held.~~ Held (section 7): the 63 `.rdata` and
   35 `.bss` words that are pointers; the other 112 `.bss`-shaped words are
   numbers (section 7.2), no longer in `exe-pointers.tsv`.
3. **The code pointers** (9,142 words): ~~as `.data` words, unfilled in a
   disc build~~ - held as layout (section 7): the 8,673 words at a
   `[[func]]` start by name and 111 at function starts the catalogue lacks
   by address; the other 358 are numbers (section 7.2). Still open: section
   4.2 (b) - the 17 functions
   without `impl` (state 2), then the stub page in our own executable. The
   7 placed mid-function words are read (2026-10-10,
   [`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md),
   `tools/exe_midfunc_pointers.py`): all seven are numbers that
   `exe-pointers.tsv` misclasses (a title sprite's size, box positions, a
   lift, credits text, area record bytes), so the page needs nothing for
   them; no missing start, no takeover gap. The page itself not started. The decoder's eight are
   to be minimp3 behind the music seam ([`mp3-decoder-choice.md`](mp3-decoder-choice.md),
   the owner 2026-10-10).
4. ~~**`c_dfDIKeyboard`** from the SDK in state 3~~ - done in the
   disc-only round (section 2.2); the import slots go with the executable.
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

## 7. The layout table (the disc-only round, CP, 2026-10-12)

The owner's call of 2026-10-11 ([`disc-only-round.md`](disc-only-round.md)
section 0): the pointer words no disc places or rebuilds are **held as
layout** - addresses, like `exe-pointers.tsv` and `symbols.toml`, never
values. Measured on the seven held discs and `BOF3.exe` (branch
`disc-only/pointers`); the commands are section 6's plus `verify`.

### 7.1 `recipes/exe-layout.tsv`

A new file, not an extension of `exe-pointers.tsv`: that file's rows are runs
(`pc`, `words`, `to`) and a target per word does not fit a run. One row per
word, `pc  kind  target`, generated by `recipe` from `BOF3.exe`,
`exe-pointers.tsv`'s decision and `symbols.toml`:

| kind | words | target |
|---|---:|---|
| `code` | 8,784 | 8,673 by `[[func]]` name (6,211 functions); 111 by address (90 function starts the catalogue lacks: the C runtime's after `Crt_GetPtd`, the blitters' after `Gfx_PackRgb`, 3 at the head of `.data`) |
| `data` | 9,959 | a `.data` address |
| `rdata` | 63 | a `.rdata` address (`0x5D8DBC..0x5D9340`, the C runtime's strings) |
| `bss` | 35 | a `.bss` address (the inventory lists 9, the C runtime's 15, `Field_MoveSpeeds` 5, 6 others) |
| `number` | 471 | none: a pointer-shaped word the recipe now classes a number (7.2) |

The table holds **every** pointer word, not only those a build leaves
unfilled: so the table itself depends on no disc, and every rebuilt word is a
cross-check - `apply_layout` stops the build if a word the disc rebuilt is
not the table's target (none is, on any of the seven).

`disc_built` (and so `build`, `xref`, `measure`) applies it after the
rebuild: every pointer word still unfilled gets its target, as one `layout`
range in `data.toml`. `check` (CI, no game data): the hash in `exe.toml`;
rows in order, aligned, inside `.data`; the pointer rows exactly
`exe-pointers.tsv`'s words with their kind; no `number` row among them;
every code name a `[[func]]`, every code address in `.text` and no
`[[func]]` start, as many as `exe.toml`'s `layout_code_unnamed` (111); every
other target inside its section (`.bss` to `bss_end`); and per build,
rebuilt + layout words = every pointer word, no pointer byte unfilled.
`verify` on a disc-built cache reads each `layout` word of `data.bin`
against the table: `layout 11880 of 11880 words recipes/exe-layout.tsv's
targets; pointer words unfilled: 0` (US), `11701 of 11701` (JP).

### 7.2 The pointer-shaped words that are numbers

`decide_pointers` called a pointer-shaped word a pointer whenever no disc
could test it. Read, three kinds are not:

**Into `.text`** (`code_numbers`, reading `BOF3.exe`'s `.text` with
capstone): a code pointer points at a function start - a `[[func]]` start,
or an instruction boundary (a linear sweep from the `[[func]]` start below)
right after a `ret` or `int3`. Of the 469 words into the middle of a
function ([`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md)),
**111 are pointers** (every one right after a `ret`) and **358 numbers**:
349 *not an instruction boundary* (the 312 `DAT\0` extensions of
`Dat_FileNames`, key names, the C runtime's `PST` / `PDT`), 9 *inside a
function* (two small halfwords: the title sprite's size, the list box and
message box positions; `DEL`, `INS`, `LID`). The seven words that doc read are
among the 358, so a disc now places them (`placed_differ` grows by their
misplaced segments: JP 3,139 to 3,143). The rule is applied whether or not
a disc places the word; at a `[[func]]` start nothing is decided by shape
(the effect state tables' starts look like halfword pairs and are pointers).

**Into `.bss`** (`bss_numbers`; no disc maps `.bss`): a pointer to a `.bss`
variable is 4-aligned - every one the C runtime and the inventory lists hold
is. 112 of the 147 are numbers: 75 *not 4-aligned* (packed bytes of the area
tables' 12-byte rows: `Area110_Weights`, `Area117_SpawnKinds2`,
`Area143_EffectStates`, ...), 19 *two small halfwords*
(`MsgBox_PlacementTable`, `Title_Sprites`, `EffectKind36_Frames`, the C
runtime's character-type flags), 4 *text* (`pen`, `%lx`, `der`), 14 *a column
of numbers* (aligned by chance where the same column holds such numbers
either side). **Six held as `bss` pointers are doubtful** - `0x5E1AC0`,
`0x61AB60`, `0x620404` (an area table's aligned word each) and `0x6542F0`,
`0x65432C`, `0x654C08` (colour-shaped): 24 bytes, a reading owed; if they
are numbers the table holds 24 bytes of Capcom's values.

**Inside the engine's own regions** (`own_numbers`): the NB group's
`src/hook/exe_own.cpp` regions ([`exe-engine-own.md`](exe-engine-own.md))
are strings the engine fill writes; a pointer-shaped word inside them is
text (319 words of the old `exe-pointers.tsv`; the rules above had already
classed 318). Read through `tools/exe_own.py`'s `own_regions` when that tool
is in the tree.

`exe-pointers.tsv`: 19,311 words to **18,840**. The `data` words are not
reclassed: a disc tests most, the rebuild proves 7,139 (JP), and shape
cannot tell a string pointer (unaligned, printable bytes) from text.

### 7.3 `data.toml`'s classes

Two new `how`s: `layout` (filled from the table; `from` is `exe-layout.tsv`)
and `own` (unfilled: the bytes of an `exe_own.cpp` region, which the engine
fills; ahead of `pointer` and `none`). On a disc build `pointer` no longer
occurs. **`src/hook/exe_image.cpp` must learn both** (IN's, one line: its
`Count by[]` table) - today it stops with `unknown how "layout"`; with the
line added (a local build, not committed) the hosted self-test of the US
cache logs `layout 47520 of 47520 bytes equal`, `own 3913 of 18812`, `none
40326 of 49058`, `compared, not mapped: 612176 of 638976` (from 576,301).

### 7.4 Places by bytes

`find_places` keys a region no pointer keys by a 16-byte window of its
numbers (no pointer word, six distinct byte values; up to eight windows),
searched in the files of the region's placed neighbours and taken when it is
found exactly once; the stretch must agree on 24 bytes or more around the
window (`keys` 0 in `exe-places.tsv`). JP 9 places more, 662 number bytes
(of the 1,026 [`exe-engine-own.md`](exe-engine-own.md) 4.4 found on the
disc); `placed_differ` unchanged by it on every build.

**A window between two code pointers** (story-flag seed, 2026-10-12): a
16-byte window of numbers with no pointer word, a code pointer word on
either side and more than one byte value (`code_windows`) is a key too,
matched only where the build's words either side are code addresses, and
tried whenever no pointer key places the region (before, a region whose
pointer votes went to a place that placed nothing was left). It found
`Scenario_SubInit`'s 16-byte story-flag seed (`0x662CD8`, between the two
code pointers of the dispatch table `0x662CD0` and `Field_ModeTailKinds`):
the region `0x662C68..0x662F28` mixes `SHOP.EMI`'s words, the scenario
data pointers and `GAME.EMI`'s run, and its code-pointer votes went to
`SHOP.EMI`, where that run does not agree. One place per PlayStation
build, `0x662CA4..0x662F24` from `ETC/GAME.EMI#0` (JP `0x801C9470`, US
`0x801C8478`; the seed at JP `0x801C94A4`, the twin `title_states.cpp`
cites, right after the PSX table's own two entries `0x801A8C34` /
`0x801A8CB4`), 32 bytes agreeing, 4 keys; it carries the seed and the 8
bytes at `0x662DE8` (`FieldTail_DropInMove`'s table), both byte-identical
to the PC's, so their two `game-value` rows leave `exe-gaps.tsv` on those
five builds (US: 455 gap ranges to 453, 2,187 game-value non-zero bytes to
2,183). Nothing else moved: `placed_differ` unchanged, `identical` +4 on
each. The PSP builds' `GAME.EMI` holds the same run with PSX-range code
words (`0x801A8C34`), which `code_word`'s PSP range excludes: widening it
moved some 20 other PSP places (one `AREA104.EMI` run placed five times),
so it was not taken and the PSP builds keep both rows.

### 7.5 Before and after, per build

`exe.toml`'s counts at `6e6b4f3b` and now (bytes of the 638,976):

| Build | pointer (unfilled) | layout | none, PC non-zero | own, PC non-zero | placed_differ | identical to the PC's |
|---|---:|---:|---:|---:|---:|---:|
| `psx-jp` | 48,692 to **0** | 46,804 (11,701 words) | 22,331 to 8,071 | 14,899 | 3,139 to 3,143 | 577,657 to **612,863** |
| `psx-us` | 49,408 to **0** | 47,520 (11,880) | 23,013 to 8,732 | 14,899 | 3,162 to 3,169 | 576,417 to 612,176 |
| `psx-eu-en` | 49,660 to **0** | 47,772 (11,943) | 23,001 to 8,720 | 14,899 | 3,160 to 3,166 | 576,242 to 612,191 |
| `psx-fr` | 49,676 to **0** | 47,788 (11,947) | 23,065 to 8,784 | 14,899 | 3,162 to 3,168 | 576,164 to 612,125 |
| `psx-de` | 49,684 to **0** | 47,796 (11,949) | 23,081 to 8,800 | 14,899 | 3,159 to 3,165 | 576,145 to 612,112 |
| `psp-jp` | 59,936 to **0** | 58,048 (14,512) | 24,779 to 8,767 | 16,895 | 3,384 to 3,397 | 566,574 to 609,917 |
| `psp-eu` | 59,852 to **0** | 57,964 (14,491) | 24,820 to 8,808 | 16,895 | 3,348 to 3,361 | 566,631 to 609,912 |

Per build the layout fills the words the rebuild does not: 8,784 code, 98
`.bss` / `.rdata`, and the data words not rebuilt (JP 2,819; US 2,998; PSP-JP
5,630). The bytes still unfilled, by region (JP, PC non-zero, a session
script over `disc_built`): pointer words nowhere; `none` - game data 4,453
(from 11,087), the language regions 1,445 (LG's / NB's, unchanged), the C
runtime past `Task_StackTop` 2,168 (2,117 and the numbers 7.2 moved), the
sound-name table's tail 5; `own` - the file layer 8,622, the rest of
`exe_own.cpp`'s regions 6,277.

### 7.6 Not done, and for the coordinator

- The recipe was generated with the NB group's `tools/exe_own.py` and
  `src/hook/exe_own.cpp` (`disc-only/engine-own`, `b59a7b34`) in the tree,
  as the merged tree will have them. On this branch alone `check`, `build`
  and `verify` pass (the image's bytes do not depend on them: `own` and
  `none` are both unfilled), but a `recipe` re-run without them would move
  the one word `own_numbers` alone settles and the `own` counts. Re-run
  `recipe` after the merge if NB's regions change.
- `exe_image.cpp` (7.3); the exe host still refuses a disc-built image
  (`ExeImage_Source`), IN's.
- The six doubtful `.bss` words (7.2).
- The 111 code words at starts the catalogue lacks have no `[[func]]`, so
  the stub page has no stub there; nothing of ours reads those `.data` words
  (the C runtime's tables, the blitters' jump table), so nothing is reached
  through them in the exe host - a fact to keep if that changes.
- `measure` and `xref` refuse an `--out` whose path merely begins with the
  worktree's path (a sibling directory such as `cp_scratch`): a string
  prefix test; not changed here.
- No ledger entry: no game behaviour changes (a disc-built image is not
  mapped yet; a PC-built cache is byte-identical as before).

### 7.7 Places by the pointers into a region (2026-10-16, [`IDEAS.md`](IDEAS.md) I43)

**Why.** 3.1 and 7.4 key a region only by what is *in* it: its pointer
words, or a 16-byte window of at least six distinct byte values. A run of
plain records holds neither, though the pointers that locate it point *in*
from a table that is placed. The case that showed it (the owner, a PSX-only
install, 2026-10-16): the child's field sprite in the forest after the mine
drawn in pieces. A bisection over the disc-built image's `gap` ranges found
one, `0x669868..0x669883` - the record lists of party members 7 and 9 that
the member-animation table `0x669884` points at
([`sprite-pose.md`](sprite-pose.md) 3). The PlayStation gives each of the five
members with no records its own one-word empty list between the others; the
PC moved those to `.bss` and packed the rest, so the byte-run map breaks at
`0x669868` and nothing placed the 27 bytes. The disc's own words at
`0x6698A0` / `0x6698A8`'s places name the lists (US `0x80181C58`,
`0x80181C68`), byte for byte the PC's.

**The rule** (`find_places`, each round): a data-pointer word of
`exe-pointers.tsv` whose four bytes one `map`, `table` or `place` range
placed, and whose PC target lies in an unplaced region, gives in the disc's
word at its place the target's address in the build. If exactly one section
of the pointer's own file holds that address (else exactly one section of the
whole disc), that is a place for the region, checked by `stretches` as every
other place is: a stretch that disagrees nowhere, holds the target's word and
agrees on 8 bytes or more is placed. Its `keys` count the pointers into it
that name that place, as well as any in it. A target already inside a place
this round found is skipped, and a stretch that overlaps one is dropped.

**Before and after** (`exe-places.tsv` rows; the bytes of the new rows;
`exe-gaps.tsv`'s `gap` ranges and bytes, then its `game-value` and
`disc-unplaced` bytes; `exe.toml`'s `identical` and `placed_differ`), on `fix/psx-only-1016` at `da58415f`:

| Build | places | new bytes | gap ranges | gap bytes | game-value | disc-unplaced | identical | placed_differ |
|---|---|---:|---|---|---|---|---|---|
| `psx-jp` | 189 to 207 | 368 | 170 to 162 | 1,817 to 1,672 | 998 to 947 | 669 to 575 | 614,596 to 614,667 | 3,143 to 3,143 |
| `psx-us` | 198 to 221 | 447 | 187 to 181 | 2,502 to 2,277 | 1,584 to 1,490 | 768 to 637 | 613,952 to 614,089 | 3,182 to 3,182 |
| `psx-eu-en` | 202 to 220 | 368 | 185 to 177 | 2,453 to 2,308 | 1,577 to 1,526 | 726 to 632 | 613,993 to 614,064 | 3,179 to 3,179 |
| `psx-fr` | 201 to 219 | 368 | 189 to 181 | 2,601 to 2,456 | 1,607 to 1,556 | 844 to 750 | 613,879 to 613,950 | 3,181 to 3,181 |
| `psx-de` | 201 to 219 | 368 | 192 to 184 | 2,630 to 2,485 | 1,624 to 1,573 | 856 to 762 | 613,866 to 613,937 | 3,180 to 3,180 |
| `psp-jp` | 207 to 261 | 1,063 | 158 to 150 | 1,396 to 1,251 | 875 to 824 | 401 to 307 | 612,531 to 612,606 | 3,391 to 3,391 |
| `psp-eu` | 212 to 265 | 1,020 | 159 to 151 | 1,406 to 1,261 | 882 to 831 | 401 to 307 | 612,525 to 612,600 | 3,368 to 3,368 |

`placed_differ` is unchanged on every build: no new place puts a byte other
than the PC's. The new rows hold more bytes than the gaps lose and than
`identical` gains; where the rest stood before (other classes of `data.toml`)
is not broken down here. On the two PSP builds five earlier places
of area-overlay data (`AREA028`, `AREA053`, `AREA094`, `AREA095`, each `#13`)
are split: their first bytes now come from `BOOT.BIN`'s copy of the same
data, which a pointer from `BOOT.BIN` reached in an earlier round, and the
rest from the area as before (and `0x5FEAB8`'s `BOOT.BIN` place starts 4
bytes earlier). The bytes are equal either way. The known case is placed on
all seven builds (US `0x80181C58` and `0x80181C68`, 12 and 15 bytes agreeing,
one key each).

**Checks.** The seven discs through `exe_tables.py recipe`, `psp_exe.py
recipe`, `exe_tables.py recipe`, `exe_own.py measure`, `exe_tables.py gaps`;
then `exe_tables.py check`, `psp_exe.py check`, `importer.py check`,
`tables.py check`, `ledger_check.py`: 0 errors each (the place check's
"keyless" rule is why the incoming pointers count as keys). The forest on a
rebuilt PSX-EU disc-only install (root `abg`): `whelpBoss` frame 4710's
sprite region against the PC's, 5,190 differing pixels to 44; the attract
with this image and with the gaps it closes zeroed again, 366 frame pairs,
0 differing ([`IDEAS.md`](IDEAS.md) I43's outcome).

**An install built before this.** The image is the importer's, made at
setup, so a new release's binaries do not bring the fix; `exe_image.cpp`'s
`FilledSince()` (D305's list) now holds `0x669868..0x669883` too, and an
image that still has it as a gap is refused at start with a message to
rebuild the install. Each row now carries what the player would see without
it (the refusal's text named only D305's crash before). Self-tested headless
(`BOF3X_SELFTEST_ONLY=1`, an ini-less copy of the build, `abg\g_psxeu`): the
2026-10-13 EU cache `abv\c_psxeu`, exit 3, the message naming the members'
records; the rebuilt `abg\c_psxeu`, exit 0, `inject: 10088 ours`. The
general form, an image stamped with its recipes' version: HANDOFF, "Pick up
here" 11.

### 7.8 Places on a vote tie (2026-10-24, issue 130)

**Why.** ScrubOasis, on a disc-built install (the US PlayStation disc as
base, the European PSP disc after it, no PC game), could not leave the house
near Maekyss Gorge (issue 130) and could not enter Mt. Zublo's cave (issue
134). Both areas' exits are `Area_LinkAt`'s links, bounded by the descriptor's
`+0x30` ([`area-entry.md`](area-entry.md)): area 62's holds 2 on the PC, three
links, and the disc-built image held 0, one link. The range `0x604210..0x60421C`
sat in `exe-gaps.tsv` as **`unread`**, which the image zeroes without a log
line: the engine reads it as `Area_Descriptors[n] + 0x30`, and `exe_own.py`'s
reader search sees neither a name, an instruction nor a pointer word reaching
that segment (the pointer word reaches the descriptor's start, `0x6041E0`, in
the segment before it). Area 97's descriptor had the same hole from `+0x24`
(`Area96_Handlers+0x154`), areas 162 and 163's from `+0x24` as `game-value`
rows (`Area154_ChoicePairs+0x18C8`, `+0x19B8`).

**The cause**, read against the US disc (`WORLD01/AREA062.EMI#13`): the PC
inserted one zero word after area 62's one-entry handler table, so
`0x6041CC..0x604224` holds three data pointers before the inserted word at
one displacement from the overlay (`0x801F2EB8`) and three after it at another
(`0x801F2EC8`, four bytes less). `find_places` counted the votes 3 to 3 and,
by the tie rule written for shared code pointers, placed nothing; no byte
window keyed the region (`top` was not empty, and no code pointer frames a
window), and section 7.7's incoming pointer is `Area_Descriptors[62]`, which
no disc places.

**The rule.** A tie between **data-pointer** votes is a region the port relaid
by a word inside it: every tied place is tried, in a fixed order (file,
section, displacement), and the stretches that pass `stretches`' byte check are
kept where they do not overlap one already kept. A tie between code-pointer
votes stays refused. The check is the one every place passes (two keys, or one
key and 16 equal bytes), so a wrong tied place places nothing.

**Before and after** (`exe.toml`; `exe-gaps.tsv`'s `gap` rows / bytes, then its
`unread` rows / bytes), on `fix/issue-130-descriptor-places` off `main` `f472072a`:

| Build | places | identical | placed_differ | gap rows / bytes | unread rows / bytes |
|---|---|---|---|---|---|
| `psx-jp` | 207 to 215 | 615,282 to 615,286 | 3,151, unchanged | 53 / 639 to 51 / 567 | 110 / 37,344 to 108 / 37,296 |
| `psx-us` | 221 to 229 | 614,726 to 614,730 | 3,194, unchanged | 72 / 1,155 to 70 / 1,084 | 111 / 37,353 to 109 / 37,305 |
| `psx-eu-en` | 220 to 243 | 614,663 to 614,733 | 3,191, unchanged | 71 / 1,245 to 71 / 1,093 | 111 / 37,353 to 109 / 37,305 |
| `psx-fr` | 219 to 242 | 614,595 to 614,665 | 3,193, unchanged | 72 / 1,278 to 72 / 1,128 | 112 / 37,361 to 110 / 37,313 |
| `psx-de` | 219 to 242 | 614,586 to 614,656 | 3,192, unchanged | 74 / 1,297 to 74 / 1,145 | 112 / 37,361 to 110 / 37,313 |
| `psp-jp` | 261 to 268 | 613,044, unchanged | 3,398, unchanged | 51 / 532, unchanged | 107 / 37,276, unchanged |
| `psp-eu` | 265 to 268 | 613,035, unchanged | 3,378, unchanged | 49 / 501, unchanged | 111 / 37,303, unchanged |

`placed_differ` is unchanged on every build: no tied place puts a byte other
than the PC's. On every PlayStation build the four descriptor rows leave
`exe-gaps.tsv` (areas 62, 97, 162, 163: `0x6041E0..0x604224`,
`0x617E58..0x617E9C`, `0x63A238..0x63A27C`, `0x63A328..0x63A36C`, each from its
area's `EMI#13`), and on the three Western builds `Scena17_EndSteps`' three
rows narrow to five smaller ones. The PSP builds already had the four
descriptors from `BOOT.BIN` (section 7.7), so their gaps do not move; their
three new places are elsewhere. A single-disc `recipe` run is not the seven-disc
one: `exe-pointers.tsv`'s number-or-pointer vote over the discs differs, and
with it a few rows' `keys`.

**Checks.** The seven discs through `exe_tables.py recipe`, `exe_own.py
measure`, `exe_tables.py gaps`; then `exe_tables.py check`, `psp_exe.py check`,
`importer.py check`, `tables.py check`, `ledger_check.py`: 0 errors each. A
scratch install built as ScrubOasis's (`importer.py setup`, the US cue and the
European PSP iso, `D:\bof3x-runs\i130\home`): the owner's recipe
`maekyssGorge` (its save `tools/recipe_saves/maekyssGorge.DAT`) on the image
before the fix ends inside the house with no area change out of 62; on the
image rebuilt from the new recipe (`exe_tables.py build`) the same inputs leave
area 62 for the world map (area 45) three times. The exe host's self-test on
the rebuilt image: exit 0, `inject: 10088 ours`. `importer.py stale --home`
on the old install names `exe` as stale, so the launcher's prompt
([`install-staleness.md`](install-staleness.md)) brings a player the rebuild;
no `FilledSince` row was added.

**Not the image:** the owner's save (made at the camp diary, not an F12 save) loads into area 62 on the PC install and
into camp area 90, then the world map, on the disc-built one - unchanged with
every `gap`, `unread` and `language` range filled from the PC's bytes and with
a JP-disc image, so the cause is in the install's data or layers, not here;
open - the owner's thought: a load's area-read logic may differ between releases (HANDOFF item 000000).

**Left.** Three descriptor holes stay on every PlayStation build, where no
pointer in the region has a placed target (the region's only data pointers
point at the area's own tables, which a generic 107-byte `run` of
`WORLD00/AREA030.EMI#3` placed): area 89's and 129's `+0x30` word
(`0x6130E8..0x6130FC`, `0x627B38..0x627B50`; the fishing spots: their link
count is 0 on the PC too - the owner, 2026-10-24: a fishing spot has no
entrance or exit, it is its own screen - so the hole costs only the
`Field_InputFlags` byte `+0x32`, 8, on which `GameMode_Enter` takes
`Game_Mode` 8, the fishing screen; what a disc-built install does at a
fishing spot is **not seen in play**), and area 134's `+0x48`
(`0x62C168..0x62C175`). The JP-disc image makes no difference to the
save-load question in the next paragraph. The general
place for a descriptor is its PSX twin through the PSX `Area_Descriptors`
table (`0x801802EC`, each entry an overlay address inside the area's own
`EMI#13`), a rule rather than a vote; not built. The `unread` class hides
such holes: a read through a pointer plus an offset is not a reader the
measurement sees, and the image logs nothing for the class.

### 7.9 Places at a neighbour's displacement (2026-10-24, issue 139)

**Why.** ScrubOasis, on the same disc-built install as issue 130 (the US
PlayStation disc as base, the European PSP disc after it), walked into Deis's
temple from Mt. Zublo and came out in McNeil Village, off the map, with its
music. The owner's `deisCave` recipe (its save `tools/recipe_saves/deisCave.DAT`,
not in the repository) replayed on the `i130` install: `AREA098.DAT` opens with
song 67, then `AREA000.DAT` with song 2 - the temple, then area 0 at (0, 0); on
the PC install the same inputs open songs 67, 54 and 19 - the temple, Mt. Zublo
(area 97) and the world map. The link lists and the descriptors of all 200
areas are byte for byte the PC's in the disc-built image (a scratch walk of
`Area_Descriptors` through both, and against the US disc's own table at
`0x8017F974`), so this was not 7.8's hole. Area 98's one link is indirect
([`area-entry.md`](area-entry.md): `+2` `0xFFFF`), its alternatives a list of
10-byte entries at `0x617EB0`: the first to area 195 (the volcano) when its
condition holds, the second to area 97. The first entry's bytes `0x617EB0..0x617EB8`
sat in `exe-gaps.tsv` as `gap` `game-value Area96_Handlers+0x184` on every build,
"no disc carries it", and the image zeroes a gap: the entry read as area 0 at
(0, 0) with a condition on flag byte 0 bit 0, which holds.

**The cause.** The US disc carries the eight bytes at `0x801F5FD4` in
`WORLD02/AREA098.EMI#13`, between two places the image already had
(`0x617EA0..0x617EAC` and `0x617EBC..0x617EC8`, psp_exe's `:twin` runs). The
port inserted a zero word before the entry (`0x617EAC`) and another after the
list (`0x617ECC`), so the two neighbours' displacements differ by a word, and
no rule reached the run: no pointer in it (one key short for 7.3's check), the
pointer to it at `0x617EC8` a layout word (not a `map`/`table`/`place` word, so
7.7's incoming rule does not see it), and eight bytes too few for 7.4's 16-byte
window.

**The rule.** Once the keyed rounds place nothing more, every unplaced run is
tried at each placed neighbour's displacement - the stretch ending at its
start and the one starting at its end, of a file the disc has (`map`,
`table`, a plain `place` or a `:twin` run; not an owning-function row, whose
address is not linear) - and at that displacement shifted by one and two words
either way, for the port's inserted or dropped words. A stretch that agrees
byte for byte (`stretches` with `plain`: a wild pointer word ends it), four
bytes with two of them non-zero or more, is placed with the neighbour as its
one key. The rule runs after the keys so that a keyed place is never
pre-empted: no row of the previous recipe moved. A placed stretch is by
construction the PC's bytes, so `placed_differ` cannot move.

**Before and after** (`exe.toml`; `exe-gaps.tsv`'s `gap` rows / bytes, then its
`unread` rows / bytes), on `fix/issue-139-neighbour-places` off `main` `c0ba3b64`:

| Build | places | identical | placed_differ | gap rows / bytes | unread rows / bytes |
|---|---|---|---|---|---|
| `psx-jp` | 215 to 310 | 615,286 to 615,401 | 3,151, unchanged | 51 / 567 to 33 / 374 | 108 / 37,296 to 107 / 37,285 |
| `psx-us` | 229 to 336 | 614,730 to 614,945 | 3,194, unchanged | 70 / 1,084 to 60 / 757 | 109 / 37,305 to 108 / 37,294 |
| `psx-eu-en` | 243 to 346 | 614,733 to 614,918 | 3,191, unchanged | 71 / 1,093 to 61 / 796 | 109 / 37,305 to 108 / 37,294 |
| `psx-fr` | 242 to 346 | 614,665 to 614,861 | 3,193, unchanged | 72 / 1,128 to 64 / 835 | 110 / 37,313 to 108 / 37,294 |
| `psx-de` | 242 to 347 | 614,656 to 614,854 | 3,192, unchanged | 74 / 1,145 to 65 / 843 | 110 / 37,313 to 108 / 37,294 |
| `psp-jp` | 268 to 465 | 613,044 to 613,180 | 3,398, unchanged | 51 / 532 to 31 / 336 | 107 / 37,276 to 106 / 37,265 |
| `psp-eu` | 268 to 455 | 613,035 to 613,163 | 3,378, unchanged | 49 / 501 to 29 / 302 | 111 / 37,303 to 110 / 37,292 |

On `psx-us` the rows that leave `exe-gaps.tsv` (some as narrower rows), each a
game value the disc carried all along: area 98's alternative
(`0x617EAF..0x617EB8`, placed on all seven builds from `AREA098.EMI#13` at
`0x801F5FD3`, 9 bytes, the next `:twin` run's displacement); `Area115_Directions`
and `Area115_Record8Anims`; `Area104_PlateAnims`, `Area121_Record8Anims`,
`Area135_EffectKinds` (area 134's `+0x48`, 7.8's third hole),
`Area141_CellEntries+0x7C`, `Area172_EffectStates+0xFF0`, `Area196_Keys1+0x658`;
the `DriftUV` rows of areas 65, 87, 88 and 115; `BossTorch_Sounds`,
`AuraBreathDome_Height`, `EffectKind18Sub37_States+0x10` and `Sub38`'s,
`GameMode3_Steps+0xC`, `Scena03_Bob4` and `Bob2`, and nine `Scena17_EndSteps`
rows narrowed to the bytes no disc has. None of these was seen in play before
this; what each would have done zeroed is not established, except the
alternative's exit.

**Checks.** The seven discs through `exe_tables.py recipe`, `psp_exe.py
recipe`, `exe_tables.py recipe`, `exe_own.py measure` (its `--analysis` the
checkout's, `analysis/pc_xref.json` being ignored by git), `exe_tables.py
gaps`; then `exe_tables.py check`, `psp_exe.py check`, `importer.py check`,
`tables.py check`, `ledger_check.py`: 0 errors each; of the previous recipe's
place rows none is gone (898 added). The `i130` install's image rebuilt
(`exe_tables.py build`, 215 bytes changed, `verify` 0 problems): the
`deisCave` recipe now opens `AREA098.DAT`, `AREA097.DAT` and `AREA087.DAT`
with songs 67, 54 and 19 - the PC's sequence. As in 7.8 no `FilledSince` row
was added: `importer.py stale` names `exe` on an install built before this,
and the launcher's prompt brings a player the rebuild.

**Left.** The gap rows that remain on a PlayStation build are the bytes no
disc holds at any neighbour's displacement: on `psx-us` 60 rows, 757 bytes,
25 of them `Scena17_EndSteps`', 10 `Equip*`, and two areas' - `Area88_DriftUV+0x78`
(area 89's `+0x30`, the fishing spot, 7.8) and `Area121_BandHeights`; area 129's
`+0x30` stays `unread` (`Area128_Handlers+0xBC`). The audit that found this -
every `gap` / `unread` range read from the disc at its neighbours'
displacements and compared with the PC's bytes - is what the rule now does on
every recipe run, so it needs no separate script; the remaining rows are its
residue. The descriptor-table rule 7.8 names (a descriptor's place from the
PSX `Area_Descriptors` table, `0x8017F974` on the US disc) is still not
built, and would place area 89's and 129's holes.

### 7.10 A one-byte run closed at a neighbour's displacement (2026-10-25, the data diff)

**Why.** [`data-diff.md`](data-diff.md)'s first run found area 6's descriptor
link index zero on both PSP-built images: `0x5DFD34..0x5DFD3D` (`+0x2C..+0x3C`)
unplaced and classed `language` in `exe-gaps.tsv` (`Area05_MessagesB+0x648`:
`exe_own.py`'s symbol-name rule reaching into the descriptor after it), so the
area's second link - indirect, three alternatives - was unreachable.
`BOOT.BIN` carries the nine bytes at the displacement of the place before them
(`psp-jp` `0x003008D0`, `psp-eu` `0x00300A90`), byte for byte the PC's; 7.9's
rule refused them for holding one non-zero byte, not two.

**The rule.** A neighbour-displacement stretch with one non-zero byte is
placed when it closes its run: every byte of the unplaced run outside it is a
pointer word (here the `+0x28` layout word). Nothing else changed: over the
seven discs one place row added on each PSP build, the two `language` gap rows
gone, no other row of any recipe file moved, `placed_differ` unchanged
(`exe_tables.py recipe`, `psp_exe.py recipe`, `exe_tables.py recipe`,
`exe_own.py measure`, `exe_tables.py gaps`; the checks 0 errors). A PSP-built
install needs the rebuild (`importer.py stale` names `exe`); what area 6's
second link did on one before is not seen in play.

### 7.11 A descriptor's place through the build's own `Area_Descriptors` (2026-10-27)

**Why.** 7.8 and 7.9 left the fishing spots' descriptors open on every
PlayStation build: areas 89's and 129's `+0x30..+0x43` (`0x6130E8..0x6130FC`,
`0x627B38..0x627B4C`; `recipes/data-known.toml`'s hole
`fishing-spot-input-flags`). The map places the first `0x30` bytes of each
from `WORLD00/AREA030.EMI#3` (the three fishing spots' section 3 is one
overlay, `copies` 3 in `exe_maps/`), the run stops at `+0x30`, no pointer in
the rest has a placed target and no neighbour holds it at its displacement. The
bytes held the `Field_InputFlags` byte `+0x32`, 8, on which `GameMode_Enter`
takes `Game_Mode` 8, the fishing screen. Area 134's `+0x48` (7.8's third hole,
`0x62C168..0x62C175`) is past that area's `0x44`-byte descriptor
(`0x62C120..0x62C164`) and was placed by 7.9 already (`AREA135.EMI#14`); it is
not this rule's.

**The table, measured.** A session script (`C:\Users\kerok\bof3x-runs\away1027\desc\scripts\measure_desc.py`,
output `measure_desc.out`; then `check_table.py`) searched each disc's boot
file for a 4-aligned run of 200 words whose entry *k* lies in a section of
area *k*'s own `AREA`*kkk*`.EMI`, with the descriptor's number bytes
`+0x30..+0x33` there equal to the PC descriptor *k*'s (`Area_Descriptors`
`0x667590`, `symbols.toml`). One such run on each disc:

| Build | Table | Entries |
|---|---|---|
| `psx-jp` | `BOOT` `0x801802EC` (the sibling's `names/area_records.toml`, verified) | 185 in `#13`, 10 in `#14`, 2 in `#9`, 3 in `#3` (areas 30, 89, 129) |
| `psx-us` | `BOOT` `0x8017F974` | the same sections |
| `psx-eu-en`, `psx-fr`, `psx-de` | `BOOT` `0x8017FE40` | the same sections |
| `psp-jp` | `BOOT.BIN` `0x002E32F0` | all 200 inside `BOOT.BIN` (its relocated copy of the descriptors) |
| `psp-eu` | `BOOT.BIN` `0x002E2B08` | all 200 inside `BOOT.BIN` |

Every number byte of the 200 descriptors at the table's place equals the PC's on
the five PSX builds (6,716 bytes, 0 differ); on the two PSPs 6,713 equal and 3
differ, area 24's `+0x40..+0x42`: its init word, null on the PC and not on the
PSPs (an init function the PC's area 24 does not call; not read further, and
not this rule's to place: the PSP map covers it). **The regression check**: for areas 62, 97, 162 and 163 the
table's entry is exactly the place 7.8's tie placed (US `AREA062.EMI#13`
`0x801F2EC8`, `AREA097.EMI#13` `0x801F2E24`, `AREA162.EMI#13` `0x801F3508`,
`AREA163.EMI#13` `0x801F34D8`; the same addresses on JP), so the rule and the
vote land on the same bytes. The fishing spots: areas 89 and 129 both at
`0x801F2C3C` in their own `#3` on every PSX build; on the PSPs `BOOT.BIN`
`0x00330CC8` / `0x00342DA0` (JP), `0x00330EA8` / `0x00342F68` (EU), which the
PSP map segments already cover - the PSPs held all three holes before this.

**The rule** (`exe_tables.descriptor_table`, `find_places`). The table is
found by shape as above, once per disc, not by a per-build address; no table
found exactly once, no place. Once the keyed rounds place nothing more, in the
same round as 7.9's neighbour displacements and ahead of them, what of each
descriptor `[d, d + 0x44)` is still unplaced is tried at the table entry's
displacement through `stretches`, area by area in table order: a stretch that
disagrees nowhere (numbers equal, a data pointer the target's address in the
build, a code pointer the paired function) and holds four equal bytes, one of
them a non-zero number byte, is placed with the table entry as its one key. A
lone pointer word is left to the layout. A first version ran the rule as a
round of its own: on the three European builds the keyed rounds take seven of
the eight, so the rule took the eighth from the neighbour rule and 85 of its
places were lost (area 98's alternative among them, which `data_diff.py run`
flagged at once); sharing the round fixed it.

**Before and after** (`exe.toml`; `exe-gaps.tsv`'s `gap` rows / bytes, then its
`unread` rows / bytes). The "before" is a fresh seven-disc run of `main`
`e84aaf6d` in a detached worktree (`desc\base_wt`): it reproduces the
committed recipes but for `exe.toml`'s date and `data-diff.tsv`'s build order.

| Build | places | identical | placed_differ | gap rows / bytes | unread rows / bytes |
|---|---|---|---|---|---|
| `psx-jp` | 310 to 312 | 615,452 to 615,454 | 3,151, unchanged | 21 / 299 to 20 / 279 | 107 / 37,285 to 106 / 37,261 |
| `psx-us` | 336 to 338 | 614,996 to 614,998 | 3,194, unchanged | 48 / 680 to 47 / 660 | 108 / 37,294 to 107 / 37,270 |
| `psx-eu-en` | 346 to 348 | 614,965 to 614,967 | 3,191, unchanged | 49 / 723 to 48 / 703 | 108 / 37,294 to 107 / 37,270 |
| `psx-fr` | 346 to 348 | 614,912 to 614,914 | 3,193, unchanged | 52 / 758 to 51 / 738 | 108 / 37,294 to 107 / 37,270 |
| `psx-de` | 347 to 349 | 614,911 to 614,913 | 3,192, unchanged | 52 / 758 to 51 / 738 | 108 / 37,294 to 107 / 37,270 |
| `psp-jp` | 466, unchanged | 613,246, unchanged | 3,380, unchanged | 21 / 267, unchanged | 106 / 37,265, unchanged |
| `psp-eu` | 456, unchanged | 613,227, unchanged | 3,360, unchanged | 20 / 237, unchanged | 110 / 37,292, unchanged |

Two place rows per PSX build and nothing else in any recipe file moved: area
89's `0x6130E8..0x6130FC` from `WORLD02/AREA089.EMI#3` `0x801F2C6C` and area
129's `0x627B38..0x627B4C` from `WORLD03/AREA129.EMI#3` `0x801F2C6C`, 20 bytes
agreeing, one key each; the `gap` row `Area88_DriftUV+0x78` and the `unread`
row `Area128_Handlers+0xBC` leave `exe-gaps.tsv`. `placed_differ` is unchanged:
no byte other than the PC's. `identical` grows by 2, the two `+0x32` bytes (the
rest of the 40 were zero on the PC and in the image alike). `data_diff.py run`:
618 rows, 0 holes, 0 errors once `fishing-spot-input-flags` was dropped from
`data-known.toml` (628 before, the ten fishing rows gone).

**Checks.** The seven discs through `exe_tables.py recipe`, `psp_exe.py
recipe`, `exe_tables.py recipe`, `exe_own.py measure` (its `--analysis` the
checkout's), `exe_tables.py gaps`, `data_diff.py run`; then `exe_tables.py
check`, `psp_exe.py check`, `importer.py check`, `tables.py check`,
`ledger_check.py`, `data_diff.py check` and `selftest`, `exe_own.py check`: 0
errors each. No ledger entry: the place is the disc's own bytes, the PC's
(DIV-0101 covers the importer). As in 7.8 no `FilledSince` row was added: an
install built before this is named stale by `importer.py stale` (`exe`).

**The `unread` class.** Of the `unread` rows in `exe-gaps.tsv`, none now lies
inside a descriptor or inside a value `data_diff.py` decodes (its descriptor,
link, zone and music-set views and every `tables.toml` table's records, read
from the PC's image; script `unread_count.py`): 0 of 106 (JP), 107 (US, EU, FR,
DE), 106 (`psp-jp`), 110 (`psp-eu`). Against the recipe before this the same
count is 1 per PSX build, area 129's row. So the class hides no hole in what the
diff decodes; what it may hide elsewhere is outside every decoder.

**Seen in play** (root `C:\Users\kerok\bof3x-runs\away1027\desc`, frames
under `shots\`). Two US-only installs (`importer.py build` from the US cue,
then `install --base`): `before` from `main`'s recipe, `after` from this one;
their `.data` images differ in exactly two bytes, `0x6130EA` and `0x627B3A`.
No recorded route reaches areas 89 or 129, so the check loads a crafted save:
`camping.DAT` with the block's area word (`+0x24`,
[`save-interchange.md`](save-interchange.md) 2a) set to 89 or 129 and the
checksum at `+0x70` recomputed, loaded with `caughFish.txt`'s first 14 lines,
then 1,500 frames without input, a frame every 150 (`input_run.py
--no-front`, the game writing its own frames). On the PC install both areas
open the fishing screen (the rod, the lure line, the catch panel). On `before`
both open as an ordinary field: a dark screen, the leader standing alone, no
fishing panel, every frame from 450 on differing from `after`'s in all 307,200
pixels. On `after` every frame from 900 on (from 450 for area 129) is pixel for
pixel the PC install's; the earlier frames are the title and load menu, which
differ between a PC and a disc-built install. The owner's `campingFishing.txt`
(area 30, the lake, which the map placed whole on every build) is the same on
both disc-built images and the PC's in eight of its nine shots; the ninth,
`fishing_equip`, differs from the PC's in 4,028 pixels on `before` and `after`
alike, so not through this rule (read below: the headings' draw under a stale
overlay). So a disc-built PlayStation install at the sea or river fishing
spot was entered as a field, not the fishing game, until this rule.

**The `fishing_equip` difference, read** (2026-10-27, from the same frames).
The 4,028 pixels are two bands, `y 144..167` (`x 116..501`, the `EQUIP` and
`LURE` title bars) and `y 272..295` (`x 116..215`, the `GUIDE` bar). They are
the same three words in the same glyphs, only placed differently: centred in
their bars on the disc-built installs and left-set on the PC's. `before` and
`after` are identical. The words are `EffectKind0F_PanelTitles`
(`0x653EA0..0x653EB4`). On the US image that range is not a `language` row
(only FR and DE have one, D286): it is a `place`, `WORLD00/AREA030.EMI#4:twin`
from `0x801E21CC`, byte for byte the PC's. So the image is not the cause. The
draw is: `effect_1b.cpp` `DrawHeading` centres the overlay's word by its real
width once the fishing chunk's tag 3 is loaded (DIV-0069 as amended for D286,
[`install-gap-labels.md`](install-gap-labels.md) 1). Without it, it draws the
original's `count` glyphs at the original's x, which assumes 12-unit glyphs
and so sets 8-unit Latin left. The disc-built install's log has `DIV-0069: 4 of
4 fishing headings (D286)`. The PC install's (the tooling folder `bof3\`) does
not: its `en-US` overlay files are dated 2026-10-13, before tag 3 (2026-10-18),
the "overlay built before" case that entry names under *Reversible*. This is an
expected layer difference on a stale overlay, not a row no layer writes. No
label-group work is needed. A PC install whose `en-US` overlay is rebuilt from
the US disc should draw the disc-built frames; not run here.

**Left.** Nothing of the 200 descriptors is unplaced on any build. Whether the
field `before` showed would have let the player leave (the fishing spot has no
links, 7.8) is not tried; `importer.py stale` brings the rebuild.

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
