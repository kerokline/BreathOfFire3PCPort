# Install-gap readers: the P4 rows and two reader questions

**Status:** STABLE (measured 2026-10-18 against `06a209bd`, group RD of the
install-gap round, [`install-gap-round.md`](install-gap-round.md) 2. Reading
only: no `src/` change, no recipe regenerated, no game run.)

The census ([`install-gap-census.md`](install-gap-census.md) 4 and 8) left
three "find a reader first" items (P4) and asked which reader owns each byte
of `Area146_EffectStates`' two rows and `Area169_Rects`' game-value row.
This document answers each with the measurement behind it. Addresses, ranges,
counts, symbol and file names only (CLAUDE.md rule 1).

## 0. The answers

| Row (census name) | Range | What it really is | Reader | Census status now | Handed to |
|---|---|---|---|---|---|
| `Music_LoadedTrack+0x1E30` | `0x6671EC..0x6671F4` (PSX West), `..0x6671F1` (JP, PSP) | the tail of a region of PSX title-overlay lists the PC never reads | **none** | **HARMLESS (unread)**; reclass by an `exe_own.py` region | the coordinator (section 1.4) |
| `ConfigMenu_Steps+0xC` | `0x667518..0x66751F` (PSP) | `Char_TraitLists`' list 1, not the Config menu's | `Char_ApplyTraits` `0x590800`, through the pointer at `0x66754C` | **NEEDS FIX - P2**, a `BOOT.BIN` twin | PB (stage B append to `TWINS`) |
| song 166 | `BGM\166.DAT` | the lost fight's music: the battle bundles' sub-song 2 | `BattleEnd_LossBanner` `0x431550`, `Music_Play(0xA6, 10)` | **NEEDS FIX - P2**: lacking on every disc-only install (PSX and PSP) | the coordinator or a music group (section 3.4), not TW / PB / stage B |
| `Area146_EffectStates+0x1230` | `0x635344..0x635358` | area **148**'s descriptor `+0x0C` table (attach vectors 0..3) | `MoveCmd_AttachOffset` `0x578EB0` | NEEDS FIX - P2, a `twin` (TW's) | TW, located by the reader (section 4) |
| `Area146_EffectStates+0x124C` | `0x635360..0x635364` | the same table, vector 5 | `MoveCmd_AttachOffset` | **NEEDS FIX - P2, a `twin`** (not P3: no immediates) | TW, the same row as the one above, **not** stage B |
| `Area169_Rects+0xA` | `0x63CA46..0x63CB03` | `EffectKind7D`'s three dial grids | `EffectKind7D_SetMap` `0x486D60`, `EffectKind7D_DrawDial` `0x486990` | NEEDS FIX - P1 (unchanged) | TW (unchanged) |
| `Area169_Rects+0xB9C` | `0x63D5D8..0x63D5DC` | area **170**'s descriptor `+0x0C` table, vector 2 | `MoveCmd_AttachOffset` | **NEEDS FIX - P2, a `twin`** (not P3) | TW (or stage B's `TWINS` append), **not** OF-A |

Two of the census's reader columns named the wrong function, for one reason:
`exe_own.py measure` counts the instructions keyed from the row's **nearest
data symbol below** to the row's end, so a row far past its symbol inherits
that symbol's readers (`Music_LoadedTrack`'s `Music_LoadFile` / `Music_Play`,
`Area146_EffectStates`' `Area146_EffectB4Run`) while its own reader, reached
through a pointer word, is not counted. Section 5 lists the `symbols.toml`
entries that would name these rows right.

## 1. `Music_LoadedTrack+0x1E30`: unread

### 1.1 What the row is

`Music_LoadedTrack` is the dword `0x6653BC`; the row is `0x1E30` bytes past it
because no `[[data]]` entry lies between. `tools/exe_own.py`'s region
`0x6653BC..0x666FC6` (the SND names, the SND / BGM formats) ends 0x226 bytes
before it. What lies between, read off the PC's `.data`:

- `0x666FC8..0x6671EA`: three lists of 3-byte records, each ended by an
  `0xFF` byte (ends at `0x6670A2`, `0x667145`, `0x6671E9`);
- `0x6671EA..0x6671F4`: two bytes, then the row (8 bytes; 5 non-zero on the
  PC in its first five, which is the JP and PSP builds' 5-byte row);
- `0x6671F4`: `TitleFlow_Step`'s jump table (`0x587DB8`).

The disc places the lists from the title overlay: every PSX map
(`exe_maps/psx-*.tsv`) has `0x666FC4..0x6671EC` (JP `0x666FC8..`) from
`ETC/START.EMI` section 8 (`0x801ECE4C` JP), and `0x6671F1..` resumes from the
same section 0x8D bytes further on (`0x801ED0FD` JP): the PSX section holds
more there than the PC's 8 bytes, which is why the row is unplaced. The PSP
maps place the second and third lists from `BOOT.BIN` and the rest from
`START.EMI` alike.

### 1.2 The measurement

A full capstone sweep of `BOF3.exe`'s `.text` (`0x401000..0x5C3668`, every
memory displacement and immediate operand; session script `scan.py`) and a
scan of every 4-byte word of `.rdata`, `.data` and `.rsrc` for a value in
`0x666FC0..0x6671F4`:

| Source | Hits in `0x666FC0..0x6671F3` |
|---|---|
| instruction operands (displacement or immediate) | **0** |
| pointer-shaped words in any section | **0** |
| `analysis/pc_xref.json` keys | **0** (nearest below: `0x666FB8`, `Music_LoadFile`'s `push` of the BGM name format; nearest above: `0x6671F4`) |
| `src/` literals | **0** (the only one near is `exe_own.cpp`'s `0x666F9C..0x666FC5` sound formats) |

So nothing reads the three lists or the row by name. An indexed read from a
base below cannot reach it either:

- `Sound_LoadStream` `0x587910` indexes the table at `0x6653B0` by `id >> 12`;
  reaching `0x6671EC` would need index `0x78F`, and its callers pass 0 or 2 or
  the movement-script op `89`'s two bytes (`id <= 0xFFFF`, index `<= 0xF`).
- `TitleFlow_Step` indexes `0x6671F4` upward by a zero-extended word
  (`xor eax, eax; mov ax, [0x66C7EA]`): never below its base.
- The `push 0x666FB8` / `0x666FA8` strings are `sprintf` formats, read to their
  NUL, which comes before `0x666FC8`.

### 1.3 Why no live run

The brief's run (`BOF3X_DATLOG=1` and a memory watch) was for the case the
static reading left open. It does not: every instruction of Capcom's that the
exe host still runs is ours (the census, section 1), ours names no address in
the region, and Capcom's instructions name none either. RW's gap watch
(`BOF3X_GAPWATCH`, the same round) guards every gap range over the reference
routes; if it ever logs a read of `0x6671EC`, this section is wrong and says so
there.

### 1.4 Status and the fix

**HARMLESS (unread)**, all seven builds. The class in `recipes/exe-gaps.tsv`
is `gap / game-value` only because of the base-symbol artefact above. The
fix is a region row in `tools/exe_own.py` `REGIONS`, for example
`(0x666FC6, 0x6671F4, "unread", "three lists of the PSX title overlay and their tail: no instruction and no word of the exe's file names them")`,
after which `exe_tables.py gaps` writes the row as `unread`. `exe_own.py` is
OT's file in stage A (round doc 4), so the coordinator adds it at the merge.
The PSX title code that reads these lists on the disc was not traced.

## 2. `ConfigMenu_Steps+0xC` (PSP): `Char_TraitLists`' list 1

### 2.1 What the row is

`ConfigMenu_Steps` `0x66750C` is a three-entry jump table (Open, Body, Close;
[`rest_2f.md`](rest_2f.md)), ending at `0x667518`. The bytes after it are the
packed (kind, amount) lists of `Char_TraitLists` `0x667548`
([`char-stats.md`](char-stats.md), `Char_ApplyTraits`): pointer words in
`.data` at `0x66754C`, `0x667550`, `0x667554`, `0x667558`, `0x667564`,
`0x667570`, `0x667584`, `0x667588` point at `0x667518`, `0x667520`,
`0x667524`, `0x66752C`, `0x667530`, `0x667538`, `0x66753C`, `0x667544`
(`recipes/exe-layout.tsv` has them too). The census's scan looked for
operands inside the row or at a table base below it; the reader reaches it
through a pointer word, which that scan did not follow.

### 2.2 The reader

`Char_ApplyTraits` `0x590800`: the trait byte `+0x1F` (`0xFF` none) indexes
`Char_TraitLists` (`mov esi, [eax*4 + 0x667548]` at `0x590816`), null is none,
then pairs until kind `0xE`. **List 1 is `Char_TraitLists[1]` = `0x667518`.**
On the PC, its terminator is at `+4`, so the reader reads `0x667518..0x66751C`
(five bytes); `0x66751D..0x66751F` are the 4-byte alignment between lists
(unread). The row's five non-zero bytes are exactly the five read.

### 2.3 The PSP's copy, and its reader

On both PSP discs `BOOT.BIN` holds the row's seven bytes **once** (session
script `trait.py`, `tools/psp_exe.py`'s `Elf`):

| Build | Copy | Relocated `.data` word pointing at it | The table it is entry 1 of | PSP code reading the table |
|---|---|---|---|---|
| psp-jp | `0x2EC080` | `0x2EC0B0` | `0x2EC0AC` | `lui`/`addiu` at `0xB7E58` / `0xB7E60` |
| psp-eu | `0x2EBD90` | `0x2EBDC0` | `0x2EBDBC` | `lui`/`addiu` at `0xB7C60` / `0xB7C68` |

The PSP table has the PC's shape entry for entry (null, five lists, nulls at
the PC's null entries, ...), with 2-aligned lists where the PC's are
4-aligned. The PSP-JP code at `0xB7E54..` is `Char_ApplyTraits`' twin
statement for statement: index times 4 plus the table, the null test, `lbu`,
compare with `0xE`, then the stat offsets `+0x2B..` (the PSP record's).
So the copy is the one the PSP's own reader reads. (The map's run
`0x66751F..0x667536` from `0x2EC085` already lines the PC's list 2 up with
the PSP's list 2; the PC's alignment byte `0x66751F` happens to equal the
PSP's list 1 tail.)

On the PSX builds the whole lists region `0x667518..0x667584` is placed from
the boot EXE (`exe_maps/psx-*.tsv`), so only the PSP rows remain.

### 2.4 Status and the hand-off

**NEEDS FIX - P2** (a twin the disc search already locates; the trait lists
change characters' stats and resistances, so a gameplay effect on a PSP-only
install for whoever carries trait 1). **PB**, appended to `TWINS` in stage B:

| Row | File | Builds | Why this copy |
|---|---|---|---|
| `0x667518..0x66751F` `ConfigMenu_Steps+0xC` (list 1 of `Char_TraitLists`) | `BOOT.BIN` | psp-jp, psp-eu | the only copy; entry 1 of the PSP's trait-list table, read by the PSP's `Char_ApplyTraits` twin (table above) |

This is the `ItemUse_HandlerIndex+0xD0` shape (`BOOT.BIN`, PSP), not a
second-copy question. The check after it: `exe_scan.py` on a `psp-jp` cache
shows `0x667518..0x66751C` equal to the PC's.

## 3. Song 166: the lost fight's music

### 3.1 What asks for it

A capstone scan of every call to `Music_Play` `0x587AE0` and `Music_LoadFile`
`0x587A20` (152 sites between them, session script `calls.py`) finds one constant `0xA6`:
**`BattleEnd_LossBanner` `0x431550`** (`BattleEnd_LossSteps[0]`,
[`rest_3a.md`](rest_3a.md)): unless bit `0x40` of `0x904AE5`,
`Music_FadeOutStop(10)` then `Music_Play(0xA6, 10)`. Its counterpart
`BattleEnd_WinBegin` `0x4314C0` plays `0xA5` (165, the fanfare) under the same
test. So 166 is asked for at the banner of every lost fight that does not
set that bit. Whether any fight sets it, and which, is not read here.
Callers that pass a track from data (the movement-script ops `B4` / `BE`'s
byte, `CommuMusic_Play`'s 40-byte table, the area's song) could also name
166; those tables and scripts were not searched, so "only the loss banner"
holds for constant arguments only.

### 3.2 What the PlayStation plays there

The PSX twins are in `BATTLE/BATTLE.EMI` section 3 (and its copies in
`BATTLE2` and the `BOSS` overlays, same addresses): at `0x801D6D20` (win) and
`0x801D6E1C` (loss), after the same bit-`0x40` test, `jal 0x8015DCF0`
(`SsSepStop` / `SsSepSetVol` / `SsSepPlay` of the resident SEP,
[`libsnd-reading.md`](libsnd-reading.md) 5.1) with `a0` 0 (the fight's own
SEP), `a1` **1** for the win and **2** for the loss, volume 100. (PSX-JP;
session script `psxjal.py`.) The PSX never passes `0xA5` or `0xA6` to its
song-table `Music_Play` `0x80162610`: on PSX-JP its constant arguments top
out at `0xA4` (every `jal` to it in the boot EXE and all 2,624 overlay
sections, `psxmusic.py`; the other builds' `Music_Play` sits elsewhere and was
not scanned).

Sub-song 2 of every battle bundle (`BGMBAT00`..`06` and their copies) has the
same shape - 2,702 ticks, 317 notes, 69 bpm (`analysis/bgm/inventory.json`) -
and so does `BGM082` sub 2, which is table song 86. So on the PSX a loss
plays song 86's sequence through the fight's own bank, as a win plays the
fanfare (sub 1) through it ([`seq-import.md`](seq-import.md) 3).

The PC's `166.DAT` is 57.05 s; `086.DAT` is 57.00 s. The onset correlation
of 0.32 ([`bgm-comparison.md`](bgm-comparison.md) 6.3) is what a different
bank under the same notes would give. The owner's names agree:
`data/music-names.tsv` row 166, "Like a Loser (Alternate)", beside row 86's
"Like a Loser". **Hypothesis**, not settled: the render that settles it is a
synth render of a battle bundle's sub 2 (bank `BGMBAT00`, the cache's 165
rule) against `166.DAT`, by `tools/bgm/synth_check.py`'s method.

### 3.3 Which installs lack it

| Install | Has a song 166? | Evidence |
|---|---|---|
| PC-only, PC plus a disc | yes: `BGM\166.DAT` (the MP3; DIV-0087 falls back to it) | [`unified-data-plan.md`](unified-data-plan.md) 6 |
| PSX-only (all five) | **no**: `base/bgm/` holds `000..165` and `BGM\` is empty | [`seq-import.md`](seq-import.md) table "166 absent" |
| PSP-only (both) | **no**: `ppms.py` writes the same 166 files, `000..165` | [`psp-music.md`](psp-music.md) |

What happens on the two "no" rows, by reading (not run): `music_seq::LoadFile(166)`
finds no cache song, `Music_LoadFile` then opens neither BGM name, and
`Music_Play` starts `Music_File` anyway - D24 ([`known-defects.md`](known-defects.md)):
the previous track, the fight's own music that `Music_FadeOutStop` just
faded, restarts from the top and loops, where a PC install plays the loss
tune. `Music_LoadedTrack` keeps naming the previous track.

### 3.4 Status and the hand-off

**NEEDS FIX - P2** (audible at every lost fight that plays it; no screen
or gameplay effect, so not P1 by the census's definition; the disc holds
the notes). Not a TW / PB / stage B row: it is a song, not exe data. The fix by
the existing 165 rule: the importer writes `base/bgm/166.DAT` from a battle
bundle's sub 2 under `BGMBAT00`'s bank, as it writes 165 from sub 1
(`tools/seq.py`, `ppms.py` for a PSP source), once the render in 3.2 settles
the pairing. A ledger entry either way (what the PC asked, what the disc
plays, which an install gets); whether to play it through the fight's own
bank is the same open choice as 165's. Handed to the coordinator for a music
group; nothing for the owner unless the render does not match.

## 4. Area 148's and area 170's attach vectors; area 170's dial grids

### 4.1 The descriptors

`Area_Descriptors` `0x667590` (200 pointers, PSX `0x801802EC`) entry 148 is
`0x635388` and entry 170 is `0x63D5F8` (pointer words at `0x6677E0` and
`0x667838`, found by the word scan). Their `+0x0C` words are `0x635340` and
`0x63D5C8`: each area's table of attach vectors, three `short`s each, read by
`MoveCmd_AttachOffset` `0x578EB0` (PSX `FUN_801AD1DC`;
[`movement-script.md`](movement-script.md), `src/game/field_objects.cpp`) as
`vectors[index * 3 .. + 2]` of the **current** area's table. Each table runs to
that descriptor's `+0x34` (choice handlers): `0x635364` and `0x63D5DC`.

Of the 34 instructions that load `Area_Descriptors[...]`
(`analysis/pc_xref.json`), only `MoveCmd_AttachOffset` (three) and
`Scena00_Run11` read `+0x0C` (session script `descc.py`); `Scena00`'s corner
words (`+0x0C` then `+0x12`) are read in its own areas (1, 2, 4, 9, `0x18`,
`0x19`, `0x1F`, [`scena_sc0.md`](scena_sc0.md)), not 148 or 170. So both
tables have one reader. The census's `Area146_EffectB4Run` is the base
symbol's reader (section 0) and reads `0x634114..0x63411B` only; "area 169 (no
instruction found)" is the same artefact.

### 4.2 The byte ranges per reader

Area 148's table `0x635340..0x635364` (six vectors):

| Bytes | Vector | Census row |
|---|---|---|
| `0x635340..0x635344` | vector 0, x and y | placed / zero |
| `0x635344..0x635346` | vector 0, z | `+0x1230` (disc-unplaced) |
| `0x635346..0x63534C` | vector 1 | `+0x1230` |
| `0x63534C..0x635352` | vector 2 | `+0x1230` |
| `0x635352..0x635358` | vector 3 | `+0x1230` |
| `0x635358..0x63535E` | vector 4 | placed / zero |
| `0x63535E..0x635360` | vector 5, x | placed / zero |
| `0x635360..0x635364` | vector 5, y and z (3 of the 4 bytes non-zero on the PC) | `+0x124C` (game-value) |

Area 170's table `0x63D5C8..0x63D5DC` (three vectors and two bytes of
alignment):

| Bytes | Vector | Census row |
|---|---|---|
| `0x63D5C8..0x63D5D8` | vectors 0, 1 and vector 2's x, y | placed / zero |
| `0x63D5D8..0x63D5DA` | vector 2, z (the row's two non-zero bytes) | `Area169_Rects+0xB9C` (game-value) |
| `0x63D5DA..0x63D5DC` | alignment (zero on the PC); read only as a vector 3 the area does not have | `Area169_Rects+0xB9C` |

Area 170's dial grids, TW's P1 row `Area169_Rects+0xA` `0x63CA46..0x63CB03`:

| Bytes | Reader |
|---|---|
| `0x63CA3C..0x63CA46` (not in the row) | `Area169_Rects` itself: two 5-byte rectangles, `Area169_MemberRect` `0x4269F0` (its cursor from `0x63CA3E` to `0x63CA48`, reading `cursor - 2 ..`) and the facing at `[i * 5 + 0x63CA40]` (`0x42690F`, `0x426959`) |
| `0x63CA46..0x63CA48` | none; zero on the PC (alignment before the grids) |
| `0x63CA48..0x63CA87`, `0x63CA87..0x63CAC6`, `0x63CAC6..0x63CB03` | the three 63-byte dial grids: `EffectKind7D_SetMap` `0x486D60` (`[value + row offset + 0x3F (k + 1) + 0x63CA09]` at `0x486DD3`, all three grids) and `EffectKind7D_DrawDial` `0x486990` via `EffectKind7D_DrawPanel` (`push 0x63CA48` / `0x63CA87` / `0x63CAC6`) |

So TW's area-169 row and the area-170 vector row share no byte and no reader;
the area-148 rows and TW's twin row share a reader with each other only.

### 4.3 The disc's copies, found by the reader

The PSX's `Area_Descriptors[148]` and `[170]` (boot EXE, `0x801802EC` on
PSX-JP) are `0x801F60F4` and `0x801F6474`, in `WORLD03/AREA148.EMI` and
`WORLD04/AREA170.EMI` section 13. Their `+0x0C` words point at the tables, and
their `+0x34` words sit `0x24` and `0x14` past the table as the PC's do
(session script `desc_psx.py`):

| Build | `AREA148.EMI#13` `+0x0C` table (36 bytes) | `AREA170.EMI#13` `+0x0C` table (20 bytes) |
|---|---|---|
| psx-jp, psx-us | `0x801F60B0`: **36 of 36 equal the PC's** | `0x801F6448`: **20 of 20 equal** |
| psx-eu-en, psx-fr, psx-de | `0x801F60B4`: 36 of 36 | `0x801F6448`: 20 of 20 |
| psp-jp, psp-eu | `0x801F60B0`: 36 of 36 (the PSP disc's `AREA148.EMI#13`) | `0x801F6448`: 20 of 20 |

The 16-byte window search missed them because the rows are short and their
windows run into neighbours the disc lays out differently (the PSX
descriptor sits `0x44` past its table, the PC's `0x48`). On the PSP, which
copy the PSP's own code reads was not traced: `BOOT.BIN` holds no
PSX-address descriptor table (`0x801F60F4` occurs nowhere in it), so the PSP
reaches its area data another way; the bytes are verbatim at the PSX
descriptor's target in the PSP's own area file all the same.

### 4.4 Status and the hand-offs

All three rows are **twins**, not owning-function rows: the reader is a
generic indexed read with no immediates to rebuild from, and the disc holds
the bytes verbatim at a place the reader's own pointer chain names.

| Row | File, place | Builds | To |
|---|---|---|---|
| `0x635344..0x635364` (both `Area146_EffectStates` rows as one; `0x635358..0x635360` between them is equal on the disc too) | `WORLD03/AREA148.EMI` #13, the table at `0x801F60B0` (JP, US, PSP) / `0x801F60B4` (EU, FR, DE), plus 4 | all 7 | **TW** (its disc-unplaced row, extended over the game-value row); OF-A drops `Area146_EffectStates` (game-value) from its list |
| `0x63D5D8..0x63D5DC` (`Area169_Rects+0xB9C`) | `WORLD04/AREA170.EMI` #13, `0x801F6448` plus `0x10` | all 7 | **TW** if it takes it in stage A, else stage B's `TWINS` append; OF-A drops `Area169_Rects` (game-value) from its list |
| `0x63CA46..0x63CB03` (`Area169_Rects+0xA`) | as TW has it (20 of 21 windows, the pieced rule) | PSX 5 | TW, unchanged; readers in 4.2 |

If TW keeps its row at `0x635344..0x635358` alone, the game-value row is the
same file and place plus `0x1C`; either way it is not OF-A's.

## 5. For the coordinator

- **Census section 8:** `Music_LoadedTrack+0x1E30` HARMLESS (unread);
  `ConfigMenu_Steps+0xC` P2 twin (PB); song 166 P2 (a music fix, hypothesis
  until the render); `Area146_EffectStates` (game-value) and `Area169_Rects`
  (game-value) move from P3 to P2 twins (TW). The census's reader column for
  these five rows wants the readers above.
- **`symbols.toml` entries that would name the rows right** (OT owns the file
  in stage A; tier `evidence` from this doc): `0x666FC8` the title overlay's
  unread lists; `0x667518` `Char_TraitLists`' list bytes (count 0x6C to
  `0x667584`); `0x635340` area 148's attach vectors (`short`, count 18);
  `0x63D5C8` area 170's attach vectors (`short`, count 9, then 2 bytes);
  `0x63CA48` `EffectKind7D`'s dial grids (count 189). With them,
  `exe_tables.py gaps` names each row by its own table instead of a symbol
  thousands of bytes below.
- **`tools/exe_own.py`:** the `unread` region of 1.4. More generally,
  `measure` attributes a base symbol's instructions to every row up to the
  next symbol; a row with no instruction or pointer word inside its own
  bytes would class `unread` if the count started at the row's start rather
  than at its base symbol. That change would move other rows too and is not
  measured here.

## 6. Sources and scripts

All in the session scratchpad (`rd/`), run against the main checkout's
`bof3/BOF3.exe` and `analysis/` and the discs in `D:\bof3x-discs\paths.json`
(`python -I` for the disc readers):

| Script | Measures |
|---|---|
| `scan.py` | every `.text` operand and every word of the other sections in a set of windows; `pc_xref.json` keys there |
| `near.py` | the xref keys and `[[data]]` symbols around a range |
| `rdis.py` | an x86 disassembly at an address |
| `trait.py` | the trait-list row's copies in each PSP `BOOT.BIN`, the relocated words naming them, the tables around |
| `mipsref.py` | `lui` / immediate pairs in a PSP `BOOT.BIN`'s `.text` aimed at an address |
| `calls.py` | every call of a function with the eight instructions before it |
| `psxmusic.py`, `psxjal.py` | every PSX `jal` to an address with its `a0` / its context |
| `desc.py`, `desc_psx.py`, `descc.py` | the area descriptors pointing into a range; the PSX / PSP descriptors' `+0x0C` tables against the PC's; the `+0x0C` readers among `Area_Descriptors`' loaders |
