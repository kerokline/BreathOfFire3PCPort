# The PSP's image of `.data`, and the game values a PSP disc carries

**Status:** MEASURED (2026-10-12, the PSP-only round's group IM, branch
`psp/im`; the seven held discs and `BOF3.exe`; headless self-tests only,
**not played**)

The PSP-only round's group IM ([`psp-only-round.md`](psp-only-round.md)
sections 2 and 4): the PSP-built image of `BOF3.exe`'s `.data` measured
against the PSX-JP one class by class, and the search of `BOOT.BIN` for the
game-value gaps a disc-only install leaves zero
([`disc-only-install.md`](disc-only-install.md) 3). No table value appears
here, in the recipe or in a commit (CLAUDE.md rule 1): addresses, counts,
hashes and rules only. Every count cites its command (section 7).

## 0. What came out

1. **The four largest game-value gaps are on the discs, in another shape.**
   `Magic_Rows`' effect files (600 bytes), `Effect_HpDamageVariance`'s
   dragon set-file tables (525), `EventBattle_Records`' battle files (82 of
   its 163) and the formation wheel's spots (`Equip_SlotIcons`+0x50, 120 of
   279) were "no disc carries them" because no 16-byte window of the PC's
   bytes is on a disc verbatim. Each is on the PSP disc, **and on every PSX
   disc held**, in the disc's own file numbering or at the disc's narrower
   name width. A rule turns each into the PC's bytes exactly: 1,326 bytes on
   each PSP build, every one equal to `BOF3.exe`'s (section 2).
2. **The file-id rule.** A disc names a file by its index in the disc's file
   table, the PC by its index in `Dat_FileNames`. `BOOT.BIN` holds the disc's
   table as path records; the PC's is in the tree already (the layout's
   `Dat_FileNames` words into the name pool `exe_own.cpp` lays from
   `recipes/pc-zh.toml`). Joined by the path's stem, with one more rule for
   the 10 effect files the PC ships no DAT for (section 3). No number is
   copied.
3. **Built** (section 4): `tools/psp_exe.py` and its generated
   `recipes/exe-psp.tsv` (151 rows: the four rules per PSP build, and 143
   runs a PSP disc's own EMIs give at the PSX-JP map's places). A PSP build's
   image takes them (`exe_tables.disc_image`, one call); a PSX build's image
   is unchanged, byte for byte. The manifest's `[build]` table gets
   `exe_tables_from`.
4. **The PSP image now** (PSP-JP): 499 gap ranges to **319** (PSX-JP 435);
   game-value bytes the PC has non-zero 1,756 to **744** (PSX-JP 1,653);
   611,644 of 638,976 bytes the PC's (609,917 before; PSX-JP 612,863).
5. **Beside a PSX disc** (section 5): the same rows would fill 1,326 bytes
   of a PSX-JP-built image and 1,363 of a US one, every byte the PC's. Not
   turned on: the gaps are the owner's open call. A PSX disc alone carries
   the same four tables (section 5.2); only the id table's half needs the
   PSX boot EXE's LBA table, not built.
6. The class-by-class reading of the PSP image before the fills (section 1):
   the extra `own` bytes are `Math_SinTable` whole; the placed bytes that
   differ are `exp_table` and consumable 87 as expected, and six more named
   symbols (SimoonDust_Offsets's 18 bytes a misplacement of the PSP map).

## 1. The PSP image against PSX-JP's, class by class (before the fills)

From `recipes/exe-gaps.tsv` at `668a9b67` and `exe_own.py measure` /
`exe_tables.py measure` over the JP, PSP-JP and PSP-EU discs.

| Class (`how` / `kind`) | PSX-JP ranges / bytes | PSP-JP | PSP-EU |
|---|---:|---:|---:|
| `gap` / `game-value` | 303 / 2,958 | 358 / 3,164 | 360 / 3,191 |
| `gap` / `disc-unplaced` | 50 / 1,143 | 59 / 1,969 | 59 / 1,982 |
| `gap` / `language-zero` | 65 / 307 | 64 / 292 | 64 / 292 |
| `gap` / `port-relaid` | 17 / 227 | 18 / 217 | 18 / 224 |
| all `gap` | **435 / 4,635** | **499 / 5,642** | **501 / 5,689** |
| `language` | 92 / 2,894 | 92 / 2,888 | 92 / 2,884 |
| `unread` | 107 / 37,427 | 112 / 37,515 | 116 / 37,542 |
| `own`, PC non-zero | 14,899 | 16,895 | 16,895 |
| identical to the PC's | 612,863 | 609,917 | 609,912 |

(Range bytes include the PC's zero bytes inside a row; `exe_own.py measure`
counts PC non-zero bytes: PSP-JP game-value 1,756, disc-unplaced 1,303.)

**The 64 extra gap ranges** are a net figure. PSP-JP has 1,814 gap bytes
PSX-JP has not, and lacks 807 PSX-JP has (`gapdiff.py`, scratch):
- **Not on a PSP disc in that shape** (422 bytes): what PSX-JP places from
  its boot EXE (`SLPS_009.90`), which the PSP has no copy of: the tail of
  `Formation_SlotVectors`, the `MapCell_Upright*` tables,
  `ItemUse_HandlerIndex`+0xD0, `MoveScript_EffectArg`, `Fish_Kinds`+0x748,
  `EventScript_OpLengths`+0x10, `Sprite_ScreenMargins`, `ConfigMenu_Steps`,
  `Char_WhelpSlot`. The PSP's native code holds its own forms of them in
  `BOOT.BIN`; not found (CD's pairing would name the readers' twins).
- **On the PSP disc at PSX-JP's place** (795 bytes, PSP-JP): the area and
  effect tables the PSX-JP map places from an EMI the PSP disc ships
  unchanged (`Area*_Buttons` / `_Directions` / `_Sprites`, `Area_CellHooks`,
  the spell effects' tables, `ShopMode_States`+0x2C, ...). The PSP map
  prefers the ELF's copy and placed none of them. Now `psxmap` rows (4.2).
- **Not one PSX-JP segment** (584): runs PSX-JP fills from two sources.

**The extra 1,996 `own` bytes** are `Math_SinTable` whole: PSX-JP's boot EXE
places all 1,996 of its non-zero bytes; no PSP source holds the table (the
PSP's code does not use it, `exe-tables-by-build.md` 5). The engine computes
it by rule (NB), so they are filled either way (`ownregions.py`, scratch).

**The extra `unread` ranges** (PSP-JP 8 rows PSX-JP lacks, 2 the other way):
handler-table tails of areas 7, 10, 20, 84, 121 and 128, and 8 bytes past
`Math_SinTable` (PSX-JP's boot EXE places them). PSP-EU adds four name tails
of `Char_DefaultRecords`. Nothing the exe host runs reads any of them.

**Every placed byte that differs from the PC's, by symbol** (`exe_tables.py
measure`, `pdiff.py` scratch):

| Symbol | PSX-JP | PSP-JP | PSP-EU | What |
|---|---:|---:|---:|---|
| `Char_ExpTable` (`exp_table`) | 0 | 249 | 249 | expected: roster index 6's 99 rows, the PSP's own (DIV B) |
| `NameTable_Consumables` | 532 | 533 | 533 | the blank names (widen), and **consumable 87**'s one byte: expected (DIV B) |
| the other five `NameTable_*` | 2,533 | 2,533 | 2,533 | the blank names (widen) |
| `Char_DefaultRecords` | 51 | 51 | 15 | the eight character names at the disc's width (known, `exe-tables-by-build.md` 6) |
| `SimoonDust_Offsets` | 0 | 18 | 18 | **a misplacement**: the map's `BOOT.BIN` run reaches seven words `BOOT.BIN`'s relocations mark as `.text` pointers (a jump table); the PC holds numbers there (`relplaced.py`) |
| `MsgBox_PlacementTable` | 0 | 6 | 6 | two box positions, each x 32 less and y 32 more: a PSP layout change (a guess: the wider screen, `psp-widescreen.md`) |
| `Scena17_EndSteps` | 0 | 3 | 3 | three text bytes of the ending's steps |
| `Title_Sprites` | 0 | 1 | 1 | one byte of a title sprite's record (a guess: the PSP's title layout) |
| `Area07_Handlers`, `Area25_Handlers` | 0, 21 | 1, 1 | 1, 1 | one byte each where the PC has zero (PSX-JP's 21 are its known AREA026 shift) |
| `MeteorStrikeChip_Lifts` | 1 | 1 | 1 | the same byte on every disc: the PC's own value |
| `Area111_GridStart`, `FieldEquip_SlotCategories`, `EventScript_OpLengths` | 3, 1, 1 | 0 | 0 | PSX-JP only |

So beyond `exp_table` and consumable 87 the PSP's own differences are the
message-box placement, one title-sprite byte, three ending-step bytes and
two area-handler bytes; SimoonDust is the map's fault, not content. What
reads each of the six is CD's to confirm.

## 2. The game-value gaps, searched in `BOOT.BIN` by structure

`BOOT.BIN` carries its relocations (`.rel.data`: 18,798 `R_MIPS_32` words on
PSP-JP, every pointer word of `.data`; `.rel.text`: the code's `HI16`/`LO16`
pairs). That is structure no PSX disc has: which words are pointers, and
which addresses the code reads.

| Gap ([`disc-only-install.md`](disc-only-install.md) 3) | Found | How | Rule | Bytes (PSP-JP / EU) |
|---|---|---|---|---:|
| `Magic_Rows` +0x8.. (the u16 file beside each code pointer) | `BOOT.BIN` `0x2F2FA8` / `0x2F2DB8` | the **one** run in `.data` of exactly 151 stride-8 rows whose +4 word is relocated into `.text` and whose +0 word is not; the code reads it at +0 and +0x4B8 (8 `HI16`/`LO16` pairs) | `rows8`: the u16 file through the id rule, 0xFFFF kept, the other u16 as is | 604 / 604 |
| `Effect_HpDamageVariance`+0xE0, 525 bytes (the dragon forms' set files) | `BATTLE/BATTLE.EMI#15` on the PSP disc (the PSX overlay, unchanged) | the two `exe_maps` segments either side are one file at one displacement: the gap is a hole in that copy; its extent is the eight records' `exe-layout.tsv` targets (tables of 0x28) | `holes`: every non-zero u16 through the id rule | 528 / 528 |
| `EventBattle_Records`+0xE0, 163 bytes | `BOOT.BIN` `0x2F2C30` / `0x2F2A40` | the catalogued record table's place (`tables.toml`) + its 56 x 4; one entry per index the records' byte +3 names (1 + the largest: 41) | `pairs`: the disc's u16 file array (stride 2) to the PC's (file, track) pairs (stride 4), the file through the id rule | 82 / 82 |
| `Equip_SlotIcons`+0x50, 279 bytes (the formation wheel) | `BOOT.BIN` `0x2EC770` / `0x2EC48C` (the lowest of four equal copies, each read by one code site) | ten 20-byte entries (an 8-byte name, three s16 (x, z) spots) for the PC's ten 28-byte ones (a 16-byte name) | `widen8`: the spots copied; the names left zero for the language layer | 120 / 120 |
| `Scena17_EndSteps` (US and the PAL discs) | `BOOT.BIN`, placed by the map already | verbatim | - | (not a gap on either PSP build) |

The **track half** of `EventBattle_Records`' pairs (41 words) stays a gap:
the PSX's `GameMode5_Load` (`0x80198C94`) loads only the file, and plays no
song there; the track numbers are the PC port's own MP3 track ids. The
**names** of the formation wheel stay a gap too (language: the PC's
Chinese). Neither is filled, and both are logged as now.

Every row was kept only when every byte it writes equals `BOF3.exe`'s
(`psp_exe.py recipe`: "0 not BOF3.exe's" on all eight). The wheel's place
is the one value-keyed find (the PC's spots searched at recipe time, as
`exe_twins.py survey` finds every map segment); the other three are found by
shape alone, and checked against the PC.

## 3. The file-id rule (`psp_exe.id_rule`)

- **Disc id to path.** `BOOT.BIN`'s `.data` holds a table of 0x40-byte
  records, one path each (`BATTLE/BATL_DRA.EMI` first), found by the disc's
  own paths: 888 records on both PSP builds, the first 883 the JP PSX disc's
  id-to-path table record for record (the sibling's `file_ids.json`, from
  the PSX boot EXE's LBA table); the last five are PSP files in place of
  `LOGO/` and the boot files.
- **Path to PC id.** `Dat_FileNames` (799 words, 742 non-null) holds, per PC
  file id, a pointer into the DAT name pool; `recipes/exe-layout.tsv` holds
  those 742 words as addresses, and `exe_own.cpp`'s pool rule lays the
  names of `recipes/pc-zh.toml` downward from the region's top. So PC id to
  name is in the tree, checked against `BOF3.exe` once: 742 of 742.
- **Joined by stem** (`MAGIC216.EMI` to `MAGIC216.DAT`): 742 disc ids. The PC
  numbers each disc directory as one block at one displacement (`BMAGIC`
  +0x105, `BOSS` -0xDF, the five `WORLD` directories -0x2A8; `BPLCHAR` in six
  blocks). The 10 `BMAGIC` files the PC ships no DAT for keep their block's
  displacement into a slot `Dat_FileNames` leaves null: 13 of `Magic_Rows`'
  151 rows name one, and `LoadDatFile` returns at once on a null slot, as on
  the PC. A disc id with neither (no name, no agreeing block) refuses the
  row.

The rule is cut-content.md's "the PC's file id is the PSX's plus 0x105"
generalised: that holds for `BMAGIC` only; the dragon set files
(`BPLCHAR`) are +0x86 and the event battles' (`BATTLE`, `BOSS`, `WORLD`) three
other displacements.

## 4. What was built

### 4.1 `tools/psp_exe.py`

```
python tools/psp_exe.py recipe  --game DIR --disc PSP-JP --disc PSP-EU    # recipes/exe-psp.tsv (needs BOF3.exe)
python tools/psp_exe.py measure --game DIR --disc DISC [...] --out DIR    # section 1's comparison
python tools/psp_exe.py beside  --game DIR --disc PSX [...] --disc PSP     # section 5 (reports only)
python tools/psp_exe.py check                                              # the recipe alone (CI, via exe_tables check)
```

`recipes/exe-psp.tsv` (generated, addresses and counts): `build rule pc
pc_end file section addr filled`, the rules `rows8`, `pairs`, `holes`,
`widen8`, `psxmap`. Its hash is in `recipes/exe.toml` (`psp_sha256`), and
`exe_tables.py check` runs `psp_exe.check`.

### 4.2 `psxmap`

What the PSP image leaves unfilled that the PSX-JP map places from an EMI
the PSP disc also ships: each such run read from the PSP's file at PSX-JP's
address, kept when the whole run equals `BOF3.exe`'s. PSP-JP 87 runs, 1,486
bytes (867 the PC has non-zero); PSP-EU 56 runs, 1,327 (803).

### 4.3 Where it applies

`exe_tables.disc_image` ends with one call, `psp_exe.fill(disc, bid,
owner)`: for a PSP build, every recipe row's bytes that nothing else filled,
as `place` ranges (`from` = `BOOT.BIN:rows8@0x002F2FA8`, ...); for any other
build, nothing. So `recipe`, `build`, `measure` and `exe_own.py measure` all
see the same image, and `exe.toml` records its hash. `exe_image.cpp` needs
no change: `place` is a class it knows, and with `BOF3.exe` present its
compare names any differing `place` byte (none, section 6).

The recipe regenerated (`exe_tables.py recipe`, `exe_own.py measure`,
`exe_tables.py gaps`, all seven discs): the five PSX builds' `exe.toml`
sections and `exe-gaps.tsv` rows are unchanged; `exe-pointers.tsv`
unchanged; `exe-places.tsv` gains 7 rows per PSP build (`find_places` finds
more beside the fills); `exe-layout.tsv` loses 25 `number` rows (pointer-shaped
words a PSP image now places equal, so the discs alone already call them
numbers; `number` rows are never filled, and no PSX image moves).

| | before | after |
|---|---:|---:|
| PSP-JP gap ranges / bytes | 499 / 5,642 | **319 / 2,925** |
| PSP-JP game-value, PC non-zero | 1,756 | **744** |
| PSP-JP disc-unplaced, PC non-zero | 1,303 | 609 |
| PSP-JP identical | 609,917 | **611,644** |
| PSP-EU gap ranges / bytes | 501 / 5,689 | 352 / 3,132 |
| PSP-EU game-value, PC non-zero | 1,769 | 800 |
| PSP-EU identical | 609,912 | 611,575 |

### 4.4 The manifest

`exe_tables_from = "<build>"` in the `[build]` table SI's
`psp_standins.manifest_lines` opens (the contract with LT): `pc-zh`, a PSX
build or a PSP build. A PSP value means the image carries the PSP's
`exp_table` and consumable 87 and this doc's fills.

## 5. Beside a PSX disc, and a PSX disc alone

**Superseded 2026-10-12** by the owner's "let's go ahead and fill them": every
PSX build now takes the four fills from its own disc
([`disc-gap-fills.md`](disc-gap-fills.md)). The measurements below are the
ones that led there.

### 5.1 The same rows over a PSX-built image (`psp_exe.py beside`, PSP-JP)

| PSX image | rows8 | holes | pairs | widen8 | psxmap | differ |
|---|---:|---:|---:|---:|---:|---:|
| `psx-jp` | 600 | 525 | 82 | 119 | 0 | 0 |
| `psx-us`, `psx-eu-en` | 600 | 525 | 82 | 119 | 37 | 0 |

Laying the whole PSP-JP image under a PSX build's gaps (`psxfill.py`,
scratch) fills more, verbatim from `BOOT.BIN`: of the PC's non-zero
game-value gap bytes, PSX-JP 984 of 1,653; PSX-US 1,521 of 2,196 (538 of
them `Scena17_EndSteps`, which the US disc lacks and PSP-JP's ELF holds; 4
bytes there differ). **Not turned on**: a PSX-only cache is byte-identical
to before (section 6).

### 5.2 A PSX disc alone (scratch measurements, not built)

The JP, US and European PSX discs hold all four tables:
`Magic_Rows`' numbers in `BATTLE/BATTLE.EMI#15` (and every `BOSS*.EMI#15`)
at `0x800B3538` (JP), 151 of 151 rows under the rule; the dragon tables in
the same section's hole, 264 of 264 halfwords; the event battles' file array
at `GAME.EMI#0` record table + 0xE0, 41 of 41; the wheel in `ETC/BATE.EMI#0`
and four other menu overlays. Only the id table is missing from the
built path: on a PSX disc it is the boot EXE's LBA table joined to the
disc's directory (the sibling's `tools/file_ids.py`). So the owner's
"accept, rule, or require the PC's exe" call on the gaps
([`disc-only-round.md`](disc-only-round.md) 2) has a fourth answer for
these four: the disc's own tables through the id rule.

## 6. Checks

| Check | Result |
|---|---|
| `python tools/importer.py check` | 0 errors (`exe_tables check`: 8 builds, 0 errors, incl. `psp_exe.check`) |
| `python tools/ledger_check.py` | 101 entries, 0 errors, 2 notes |
| JP + PC, US alone, European alone: `importer.py build` with `668a9b67`'s tools and with this branch's (SI merged), every file but the manifest hashed | **unchanged**, all three |
| the manifests, `built =` aside | only the `[build]` table differs (SI's `base_policy` and this `exe_tables_from`: `pc-zh`, `psx-us`, `psx-eu-en`); all parse as TOML |
| `importer.py build`, PSP-JP alone and PSP-EU alone | exit 0; `exe_tables_from = "psp-jp"` / `"psp-eu"` in the one `[build]` table; `base/exe/` equal to `exe_tables.py build`'s; `verify`: layout 14,512 of 14,512, 0 problems |
| exe host self-test (`BOF3X_SELFTEST_ONLY=1`, a launcher copy in an ini-less scratch folder), the real install, `BOF3X_EXEIMAGE` = the PSP-JP image | exit 0; `place 543 ranges 5299 bytes`, `gap 319 ranges 2925 bytes` (each logged); against `BOF3.exe`: `628587 of 638976 bytes equal; every class that must equal does`; **no `place` range differs**; `inject: 10087 ours`; `self-test only: done` |
| the same, the PSP-EU image | exit 0; `place 527 / 5296`, `gap 352 / 3132`; `628518 of 638976`, every must-equal class equal, no `place` range differs |

**PSP-JP's and PSP-EU's `base/exe/` differ** (SI's observation: both files):
`data.toml` by its ranges' sources, and `data.bin` in 124 bytes - 89 where
one build's `psxmap` reaches a run the other's does not (JP fills
`Area_CellHooks`, `BattleItemCmd_EquipSideSteps`, `MenuList_GeneWinSteps`,
`EffectKind34_V0Areas`, `GameMode11_Steps`, `Escape_WinSteps` and four plate
tables; EU one plate table: each filled side equals the PC's, the other is
zero), 23 in `Char_DefaultRecords`' names (language), and 12 in
`SimoonDust_Offsets` (each build's misplaced jump table, section 1).

## 7. Commands and scratch

The measurements are `tools/exe_own.py measure`, `tools/exe_tables.py
measure`, `tools/psp_exe.py recipe / measure / beside`, and session scripts
in the session scratch `psp_im/` (`gapdiff.py`, `ownregions.py`,
`pdiff.py`, `relplaced.py`, `newgaps.py`, `psxfill.py`, `psx_alone.py`,
`holes.py`, `find_magic.py`, `idrule2.py`, `evb.py`, `wheel.py`). They read
`BOF3.exe` and the discs and print values; none of that output is in the
tree.

## 8. Not verified

- **Not played.** No battle with magic, no event battle, no transformation,
  no formation wheel was seen from a PSP-built image. The fills equal the
  PC's bytes, so they should behave as the PC does; the owner's V4 play is
  the check.
- The six PSP-only placed differences (section 1) are named by bytes and
  symbol, not by their readers: CD's.
- The 422 PSX-boot-EXE bytes the PSP lacks (section 1) were not searched in
  `BOOT.BIN` beyond verbatim and the id rule; their readers' PSP twins
  would locate them (CD's pairing).
- The PSX id table (section 5.2) was not built; the PSP's table stood in for
  it in the measurement (equal for ids below 883).
- `SimoonDust_Offsets`' misplacement is reported, not fixed: the map is
  `exe_twins.py`'s, and splitting PSP runs at relocated words would also
  unplace about 600 coincidentally equal bytes.
- No live run (the brief's rule).

## For the other files

Not edited here; the coordinator folds them in.

**DIV B - A PSP-built image of `.data` carries the PSP's level table and
consumable 87.**
- Original: `BOF3.exe`'s `.data` holds the PSX's `Char_ExpTable` (every PSX
  build equal) and consumable 87 as the PSX has it.
- New: an image built from a PSP disc (`exe_tables_from` a PSP build: a
  PSP-only install) holds `BOOT.BIN`'s: roster index 6's 99 rows of
  `exp_table` (249 bytes: the EXP and stat-gain bytes, not the learnt
  abilities) and consumable 87's one differing byte. With a PSX disc or the
  PC's exe first, nothing changes.
- Why: the owner's call of 2026-10-10, the PSP's data as the base of a
  PSP-only install ([`psp-only-build.md`](psp-only-build.md) 7).
- Also in the PSX version? No: these are the PSP's changes; every PSX disc
  holds the PC's values.
- Verification: `exe_tables.py measure` names exactly these two among the
  tables (`exe_table differs 249 bytes in records 629..`, `consumables
  differs 1 bytes in record 87`); not played.
- Note for DIV-0101 (the gaps, provisional): on a PSP-built image four gaps
  are now filled with the PC's own bytes by rule from the disc
  (`recipes/exe-psp.tsv`): not a divergence from the PC, and the PSX-built
  images are unchanged.

**`docs/STATUS.md` / `docs/HANDOFF.md`**: the four largest game-value gaps
(`Magic_Rows`' effect files, the dragon set files, the event battles' files,
the formation spots) are on every disc held, through a file-id rule; filled
for PSP builds (PSP-JP game-value gap bytes 1,756 to 744); a PSX-only build
could take the same with the PSX boot EXE's id table - the owner's call.

**`docs/README.md`**: a row for `psp-image.md`.

**`docs/psp-only-build.md`** section 2.3: the PSP image's pointers are 0
unfilled (CP), its `sin_table` the engine's, its gaps 319 ranges (this doc).

**`docs/owner-review.md`**, one entry: the gaps call (`disc-only-round.md` 2)
now has the disc's own tables as an answer for the four largest; filled for
PSP-only; for PSX-only, say whether to build the PSX id table and fill them
too.

**Known defects** (for numbering): `SimoonDust_Offsets` misplaced in both
PSP maps (18 bytes from a `BOOT.BIN` jump table).
