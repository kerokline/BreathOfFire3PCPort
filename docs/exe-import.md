# `base/exe/`: the exe-resident tables imported into the cache

**Status:** IN PROGRESS (2026-10-08, cloud session; unified-data step 8, the
importer half - [`unified-data-plan.md`](unified-data-plan.md) section 5
step 3 and section 8 row 8. The importer writes `base/exe/` from `BOF3.exe`
or from any held disc alone and verifies it; the engine half - mapping
`base/exe/` where `.data` was - waits on the owner's machine and the state
hash, and section 6 is its worklist. Its first part landed 2026-10-10 in
[`exe-import-engine.md`](exe-import-engine.md): the data-pointer rebuild, the
places by pointer, `.rdata`'s constants, state 3's design and `BOF3X_EXEIMAGE`;
a disc-built image's numbers below are now that doc's section 1.2.)

The engine reads the game's tables from `BOF3.exe`'s initialised `.data` at
their linked addresses. A cache built without the PC's executable needs that
image from a disc. This step writes it, measures how much of it a disc
alone reproduces against the PC's, and lists what the engine must hold
itself. Every number below comes from a run in this session (the commands
are in section 8). No table value appears here, in the recipe or in a commit
message (CLAUDE.md rule 1); the images and the measurements are in the cache
and in `analysis/exe_import/`.

## 0. What came out

1. **`base/exe/`'s format** (section 1): `data.bin`, the PC's initialised
   `.data` byte for byte where the source allows, and `data.toml`, its
   address, size, sha256, the build, and how every range was produced.
2. **The producer**, [`tools/exe_tables.py`](../tools/exe_tables.py): from
   `BOF3.exe` a copy; from a disc, `exe_maps/<build>.tsv`'s segments, the
   catalogued tables from their recorded places (the name tables widened), the
   PC's pointer words left unfilled. Two generated files:
   [`recipes/exe.toml`](../recipes/exe.toml) (each build's image hash and its
   counts against the PC's) and
   [`recipes/exe-pointers.tsv`](../recipes/exe-pointers.tsv) (the pointer
   words, 19,311 of them in 3,134 runs).
3. **The measurement** (sections 3 and 4): a PSX disc alone places 491,721 of
   `.data`'s 638,976 bytes (JP), and 488,582 of those equal the PC's; 3,065 of
   the 3,139 that differ are the blanked names. With the zeros, **555,969
   bytes (87.0 %) of the JP-built image equal the PC's**; 553,490 (86.6 %) of
   the PSP-EU-built one, the least. **All 27 catalogued tables a disc carries
   come out byte-identical to the PC's from every Western PSX disc** (26 of
   26, the name tables with their names blank), 25 from the JP disc, 23 of the
   25 the PSPs carry; every difference is one step 5 already listed as
   regional, so `tables.toml` and `exe_maps/` are unchanged.
4. **The engine's worklist** (section 6): of the 5,122 addresses `src/` reads
   in `.rdata` / `.data` / `.bss`, 1,647 are pointer words a disc cannot give
   the PC's values for - 1,542 of them into `.text`, Capcom's code, which
   state 3 does not map - and 321 (JP) are data no disc carries.
5. **The hook** (section 7): `importer.py build` writes `base/exe/` and
   records it in the manifest; `verify` checks it against `recipes/exe.toml`;
   `check` checks the recipe.

## 1. The format

```
base/exe/data.bin     0x9C000 bytes: BOF3.exe's .data from 0x5DA000 to its raw end 0x676000
base/exe/data.toml    [image] file, va, size, bss_end, sha256, build, source, generated; ranges = [...]
```

- **One image, `.data` only.** `BOF3.exe`'s section table (read with
  `exe_tables.pe_sections`): `.text` `0x401000`, `.rdata` `0x5C4000` (raw
  `0x16000`), `.data` `0x5DA000` (raw `0x9C000`, virtual `0x3636EC`, so it
  runs to `0x93D6EC`), `.rsrc` `0x93E000`. All 28 catalogued tables with a PC
  symbol are in `.data`; the three `symbols.toml` `[[data]]` entries in
  `.rdata` are the import table's `Imp_GetTickCount` and two DirectInput
  descriptors. `.rdata` is the compiler's constants and the imports - no disc
  has it (step 5 did not survey it for that reason) - so it is not in
  `base/exe/`; section 6 counts what `src/` reads there.
- **`bss_end`**: `[va + size, bss_end)` is zero-initialised; the engine
  reserves and zeros it as the loader does today.
- **`ranges`**: `[pc, end, how, from]` covering the image in order. `how` is
  `exe` (the PC's own section, one range), `map` (an `exe_maps` segment;
  `from` is the file, its section and the address of `pc` there), `table` (a
  `tables.toml` table read at its `[[table.psx]]` place), `widen` (a name
  table: the disc's numbers at the PC's offsets, the 16-byte name blank;
  `from` is the table's start), `pointer` (a pointer word of the PC's,
  unfilled) or `none` (no disc carries it, unfilled). Unfilled bytes are
  zero. A disc-built `data.toml` is about 6,400 lines (it lives in the cache,
  not the repo).
- **The PC layout is kept byte for byte where the source allows**: no
  re-stride and no re-ordering; the engine half maps `data.bin` at `va` and
  every address `src/` uses means what it means today.

## 2. The producer

`exe_tables.build_from(path, build, out)`:

- **`pc-zh`** (`BOF3.exe`): the section's raw bytes. The oracle: its sha256
  `b9f91e2392f9...` is the hash of the bytes `dd` cuts from the file at
  `.data`'s raw offset `0x1DA000`, length `0x9C000`, and of what
  `exe_tables.py build --source BOF3.exe` writes.
- **A disc** (any of the seven held builds):
  1. every segment of `exe_maps/<build>.tsv`, read from its file (the boot
     EXE by its load address, an EMI section by its destination, the PSP ELF
     by its link-time offset - `exe_twins.sources`, as the maps were made);
  2. every `tables.toml` table with a PC symbol and a `[[table.psx]]` row for
     the build, read from that row instead (on a PSP disc the ELF's row, the
     copy the PSP's own code reads, as the maps prefer). The six item and
     ability tables are **widened**: each record's numbers move to the PC's
     offsets (`tables.narrowed`, the 8- or 12-byte name of `[meta] name_len`
     against the PC's 16) and the name field is left blank. The names are
     glyph codes of a language layer's font, so they belong in `loc/`, as
     step 3 split the enemy tables ([`importer-transforms.md`](importer-transforms.md)
     section 2); `loc_build.py` already writes them as overlay chunks;
  3. every word of `recipes/exe-pointers.tsv` cleared and listed.
  The image is hashed against `recipes/exe.toml`'s row for the build; a
  different hash stops the build.

**Which words are pointers** (`exe_tables.py recipe`, needs the PC and the
discs): the PC's `.data` has 25,382 4-aligned words whose value lies in the
image (`0x401000..0x940000`, the twin method's wildcard), name fields
excluded. Of those, 6,071 hold the same value on every disc that places them:
numbers that look like addresses, which are copied. The other **19,311 are
pointers** - 9,142 into `.text`, 9,959 into `.data`, 147 into `.bss`, 63 into
`.rdata` - and are never copied: a disc's word there is its own build's
address. `exe-pointers.tsv` lists them as runs (address, words, the section
pointed at): addresses and counts, a fact of the PC's layout like
`symbols.toml`'s.

**The pointer tables of step 5's section 6 item 2** fall out of this rule:
`Area_Descriptors` is wholly pointer words (all 800 bytes unfilled from every
build), `Magic_Rows` and the area handler arrays are pointer words between
numbers; the numbers are copied, the words are not. Nothing is translated:
section 5.3 measures how far the map run backwards would rebuild the data
pointers.

## 3. What a disc alone reproduces

`exe_tables.py measure` (every byte of the 638,976 classed; `recipes/exe.toml`
carries the same counts):

| Build | placed, equal | placed, differ | pointer words (unfilled) | no disc carries it, PC zero | no disc carries it, PC non-zero | **identical to the PC's** |
|---|---:|---:|---:|---:|---:|---:|
| `psx-jp` | 488,582 | 3,139 | 77,244 | 47,277 | 22,734 | **555,969** (87.0 %) |
| `psx-us` | 487,729 | 3,162 | 77,244 | 47,425 | 23,416 | 555,264 |
| `psx-eu-en` | 487,741 | 3,160 | 77,244 | 47,427 | 23,404 | 555,278 |
| `psx-fr` | 487,653 | 3,162 | 77,244 | 47,449 | 23,468 | 555,212 |
| `psx-de` | 487,642 | 3,159 | 77,244 | 47,447 | 23,484 | 555,199 |
| `psp-jp` | 487,219 | 3,384 | 77,244 | 46,168 | 24,961 | 553,497 |
| `psp-eu` | 487,154 | 3,348 | 77,244 | 46,226 | 25,004 | 553,490 (86.6 %) |

`identical` counts every byte equal to the PC's, the unfilled zeros where the
PC holds zero (20,110 of the pointer words' bytes are zero high bytes)
included. Step 5's "81 % placed" counts the maps' `agree`, in which a pointer
word agrees by definition; here a pointer word is never placed, so 76.5 %
(JP) of `.data` is a disc's bytes equal to the PC's, 12.1 % is pointer words,
and 11.0 % no disc carries (two thirds of it zero in the PC).

**The bytes that differ** (`measure`'s `differing placed bytes by how`, and the
per-symbol counts in `analysis/exe_import/<build>.tsv`):

| Build | names blanked (`widen`) | a catalogued table (`table`) | inside a map segment (`map`) |
|---|---:|---:|---:|
| `psx-jp` | 3,065 | 1 | 73 |
| `psx-us` | 3,065 | 0 | 97 |
| `psx-eu-en` | 3,065 | 0 | 95 |
| `psx-fr` | 3,065 | 0 | 97 |
| `psx-de` | 3,065 | 0 | 94 |
| `psp-jp` | 3,066 | 249 | 69 |
| `psp-eu` | 3,066 | 249 | 33 |

- `widen`: the PC's non-zero name bytes, blank by design (section 2); the
  PSP's one more is consumable 87 (below).
- `table`: the known regional differences of section 4.
- `map` - bytes in the up-to-64-byte gaps a segment spans, the disc's own
  version of the PC's data. Read by symbol (`analysis/exe_import/<build>.tsv`,
  `differ_map`), every one is language, a regional change, or padding:
  - `Char_DefaultRecords`, the eight new-game character records: 51 (JP,
    PSP-JP), 56 (Western PSX), 15 (PSP-EU) bytes - their names, 9 bytes on the
    PC and 5 on the discs (step 5 section 5 "Widened"), copied at the wrong
    width by a segment that spans them. **Not a catalogued table yet** (step
    5 section 6 item 3); until it is, the image carries the disc's bytes there.
  - `Scena17_EndSteps`, 34 bytes in three gaps of the ending's `SCENA17`
    section 0, every Western PSX build: the regional credits (step 5 lists
    `Scena17_RollLines` as language).
  - One byte, the same change in four world-4 areas' section 13 (`AREA175`,
    `176`, `177`, `183`), every Western PSX build: script data, not read.
  - `Area25_Handlers` +0x6B9..+0x6DB (PC `0x5EB38D..0x5EB3B4`), 21 bytes, **the
    JP disc only**: `AREA026` section 13 there is the later builds' bytes
    shifted by one; the US and PSP-JP builds equal the PC's. The PC follows
    the later builds here - a JP-disc difference `region-diff.md`'s section
    rows (JP is their reference) could not show.
  - `SimoonDust_Offsets`, 18 bytes, both PSPs: the PSP compile's own values
    in a gap of its ELF's segment.
  - Single bytes: `FieldEquip_SlotCategories` +6/+7 (past its six entries:
    padding; JP 1, US 2, FR and Europe-English 1), `Title_Sprites` (US, FR:
    one coordinate of the title sprites, regional), `SuperComboHit_TaskTable`
    +0x14 (FR).

## 4. The 29 catalogued tables, per build

`exe_tables.py measure`'s table lines (`table_identity`: the image's bytes at
the table's PC address against the PC's, the name fields blanked on the PC
side for the six name tables):

| Tables | `psx-jp` | `psx-us` | `psx-eu-en` | `psx-fr` | `psx-de` | `psp-jp` | `psp-eu` |
|---|---|---|---|---|---|---|---|
| the six item and ability tables | identical, names blank | identical, names blank | the same | the same | the same | consumable 87 differs (1 byte); the other five identical, names blank | the same as `psp-jp` |
| `shop_records`, `event_battles`, the two encounter tables, the six battle rate tables, the two steal tables, `move_op_lengths`, `direction_angles`, `move_speeds`, `direction_steps`, `sprite_key_adjust` (17) | identical | identical | identical | identical | identical | identical | identical |
| `exp_table` | identical | identical | identical | identical | identical | 249 bytes differ, roster index 6's rows | the same 249 |
| `event_op_lengths` | entry 15 differs (1 byte) | identical | identical | identical | identical | identical | identical |
| `sin_table` | identical | identical | identical | identical | identical | **unfilled**: not in the PSP | unfilled |
| `area_descriptors` | unfilled: all pointer words | the same | | | | | |
| `key_defaults` | unfilled: the PC's own (no disc has it) | the same | | | | | |
| `songs` | no PC twin: not in `.data` | | | | | | |

So of the 29, a PSX disc carries 26 (all but `area_descriptors`,
`key_defaults` and `songs`): **26 of 26 identical from every Western PSX
disc** (the six name tables with blank names), 25 from the JP disc. A PSP
disc carries 25 (no `sin_table`): 23 identical, `exp_table` and
`consumables` not. Every difference is
one step 5 listed as regional ([`exe-tables-by-build.md`](exe-tables-by-build.md)
section 4.4): the PSP's rebalanced level table for one character, the PSP's
consumable 87, the JP disc's unused `EventScript_OpLengths` entry 15. None is
a reading error; `tables.toml` and `exe_maps/` needed no change. Three are not
the cache's from any disc: `area_descriptors` (the engine's, section 6),
`key_defaults` (the port's own), `songs` (no PC twin; the PC's music is file
N for song N). A PSP-only install also lacks `sin_table`.

## 5. Findings on the way

1. **A PSP-built image is not JP's base**: it carries the PSP's level table and
   consumable 87, as `base/` takes a build's own bytes. Under the plan's data
   policy (section 7 there) a later build's content change is an `opt/` layer
   off by default; for a PSP-only player there is no other source. The
   owner's call (below).
2. **The JP disc's `AREA026` differs from the PC where the later builds do
   not** (section 3, 21 bytes): the PC took this area's data from a later
   build than the JP disc, or the JP disc predates a fix. Not read further.
3. **The data pointers can be rebuilt from a disc** (measured here; built
   2026-10-10, [`exe-import-engine.md`](exe-import-engine.md) section 1):
   `measure`'s `pointer words` line. On the JP disc 6,667 placed words point
   into `.data` at a byte some segment places; for **6,229 of them the disc's
   word is exactly the target's address in that build** (the map run
   backwards), 30 differ, 408 point at bytes no segment places (93 %); the
   other Western discs 6,199 to 6,201 of about 6,630. On the PSPs 3,841 and 3,847 (2,696 and 2,702
   differ: the ELF's layout is not the EMIs' the maps take some targets from).
   Code pointers (3,506 placed on JP) cannot be rebuilt this way: their
   targets are Capcom's code, which no build shares an address with.
4. **`Char_DefaultRecords` needs the catalogue** (step 5's owed item 3): 51 to
   56 of the differing bytes are its names, copied at the disc's width.

## 6. What the engine will need (read and listed here; the engine half started in [`exe-import-engine.md`](exe-import-engine.md))

**Since 2026-10-10** ([`exe-import-engine.md`](exe-import-engine.md)): item 2's
rebuild transform is built (JP 7,138 of the 9,959 data pointers, every one the
PC's; the words it would get wrong per build in `recipes/exe-rebuild.tsv`);
item 3's `.rdata` reads are settled (44 the engine's own constants in
`src/game/rdata_consts.h`, verified against the exe at every start; 13 import
slots, `c_dfDIKeyboard`, four floats only Capcom's code reads and three
non-reads stay); item 4's unplaced data is placed further by the twin method
keyed by pointers (`recipes/exe-places.tsv`; JP image 90.4 % the PC's); item
1's design is chosen (an entry-stub page at Capcom's addresses) and its first
step, `BOF3X_EXEIMAGE`, built. The tables below are this section's 2026-10-08
measurement, kept as the record; the engine doc's section 3.2 re-measures
them on today's `src/`.

`exe_tables.py xref` reads every address `src/` uses in `.rdata`, `.data` or
`.bss`: the 2,554 `symbols.toml` `[[data]]` entries `src/` names (a word
match over `*.cpp` / `*.h`, `exe_twins`' rule) or cites by literal, each with
its extent (its `count` x `ctype`, else to the next symbol, at most 64 KiB),
and the 2,568 six-digit hex literals in code (comments stripped) that start no
symbol, each probed as one word - 1,067 of them in `.data`, step 5's count.
Each is classed against each build's image; the per-address list is
`analysis/exe_import/src_addresses.tsv` (gitignored).

| Class | what the engine half does | symbols, JP | (range over builds) | raw, JP | (range) |
|---|---|---:|---|---:|---|
| `reproduced` | read from `base/exe/` | 569 | 550..569 | 555 | 543..562 |
| `reproduced+pointer` | `base/exe/`, its pointer words the engine's | 27 | 25..27 | 7 | 7..8 |
| `differs` | `base/exe/`; a language or regional difference (section 3) | 6 | 5..7 | 9 | 8..9 |
| `differs+pointer` | the same, with pointer words (`Sprite_KeyAdjust`'s symbol extent runs past the 16-byte table) | 1 | 0..1 | 0 | |
| `pointer:text` | **the engine's own**: words pointing into Capcom's `.text` | 1,484 | 1,482..1,484 | 58 | 58 |
| `pointer:data` | the engine's own, or rebuilt (5.3) | 24 | 24..25 | 81 | 81 |
| `none` | **the engine's own**, or a disc source still to find | 67 | 67..82 | 254 | 246..266 |
| `none+pointer` | the same, with pointer words | 11 | 11..12 | 3 | 2..3 |
| `partly` | mixed: some bytes placed, some none | 26 | 26..32 | 8 | 7..13 |
| `zero` | `.data` bytes zero in the PC and unplaced: nothing to carry | 23 | 22..23 | 92 | 91..92 |
| `rdata` | **the engine's own** (`.rdata` is not in `base/exe/`) | 3 | | 62 | |
| `bss` | zero at start: no image bytes | 313 | | 1,439 | |
| total | | 2,554 | | 2,568 | |

What the `none`, `none+pointer` and `partly` addresses are (JP, symbols and
raw together, by `exe_tables.group_of`: the symbol's name, its size, and
whether a pointer word lies within 16 bytes):

| Group | none | none+pointer | partly | Examples |
|---|---:|---:|---:|---|
| mixed: within 16 bytes of a pointer word | 113 | 7 | 12 | the menu and window step tables (`Battle_CommandLabelPointers`+, `BattleMenuWin_EquipItemsSteps`+, `ConfigScreen_OpenStates`+, `CommuRank_Modes`+), effect state records, `Magic_Rows` (partly), `WorldMap_Records` (partly), `Area0_Descriptor` - records of pointers and numbers in which the survey's 16-byte pointer-free key never fits, so no run seeded |
| language: text, names, formats | 75 | 0 | 7 | the area message and choice tables (`Area21_Messages`, `Area79_ChoiceMessages`, `Area08_MessageFormat`), `Menu_Verbs`, the community's name records, `Scena17_RollLines` (partly) |
| other | 47 | 0 | 1 | raw constants past the end of a catalogued table (`Encounter_SlotChance`+0x1C.. are the unnamed data after it, `EventBattle_Records`+0xE0 past its 56 records), `Area115_Sprites`, `Area151_Sprites`, the world-map `AreaNN_Buttons`, `Battle_CommandLabels`, `Fx106_ShardTables` |
| small: 16 bytes or less | 42 | 2 | 5 | `Area15_PosesA` / `B`, `Scena04_Quake`, `SnapWave_FacingPhase`, the `AreaNN_Record8Anims`, boss sound pairs - step 5 section 5's "too small to place" |
| platform: graphics | 17 | 2 | 0 | `Gfx_ScreenRect`, `Gpu_OtTerminator`, `Cursor_Hidden`+, `D3d_AlphaOpCache` |
| re-laid by the port | 15 | 1 | 7 | the world maps' `AreaNN_DriftUV`, `Area104_PlateAnims`, `WeretigerImage_UV` (DIV-0055's territory) |
| platform: C runtime, imports, file layer | 6 | 1 | 0 | `Dat_FileNames`+, `File_CdRootBuf`+, `Task_StackTop` (the stack) |
| platform: keys and input | 3 | 1 | 0 | `Cfg_Fullscreen`, `Cfg_RenderMode`, `Key_TableDefault` (the catalogued `key_defaults`) |
| platform: sound and music | 3 | 0 | 1 | `Music_LoadedTrack`+ |
| widened | 0 | 0 | 1 | `Char_DefaultRecords` (also `differs` by one raw constant) |
| total (JP) | 321 | 14 | 34 | |

**Read as the engine half's worklist** (state 3 of
[`platform-layers-plan.md`](platform-layers-plan.md) section 3):

1. **The code pointers are the engine's, whatever the source.** 9,142 `.data`
   words point into `.text`, at 6,346 distinct addresses, 6,211 of them
   `symbols.toml` `[[func]]` starts (a one-off count over
   `exe-pointers.tsv`'s `text` runs against the PC's words); 1,484 of the
   symbols `src/` reads are wholly such tables (the handler arrays, state
   tables, descriptors' hooks). Today they reach our code through the
   detours at Capcom's entries. In state 3 Capcom's code is not mapped, so
   either the engine rewrites every such word to its own function (the
   binding `symbols.toml` already generates) or it maps a page of entry
   stubs at Capcom's addresses. **This is true with the PC's `BOF3.exe` as
   the source too** - state 3 never maps the PC's `.text` - and section 3 of
   the platform plan does not say it yet.
2. **The data pointers** (9,959 words into `.data`, 147 into `.bss`, 63 into
   `.rdata`) are addresses of the PC's layout; with a PC source they are
   right as copied. From a disc the engine holds them (they are layout, like
   `symbols.toml`), or a later importer transform rebuilds them by the map run
   backwards (5.3: 93 % on the PSX discs).
3. **`.rdata`**: 3 symbols and 62 raw constants `src/` reads. No disc has
   them: they are the engine's own constants, or `base/exe/` grows an
   `rdata.bin` from the PC only.
4. **No disc carries** 321 addresses' bytes (JP; 67 symbols, 254 raw), 14
   more beside pointer words, 34 partly - 22,734 non-zero bytes of the image.
   The platform groups (34 addresses) are the engine's own data by nature;
   the 23 the port re-laid (the plate drift tables) are the engine's too, or
   a transform from the disc's own plates (DIV-0055). The language group is a language layer's job, not
   `base/`'s. The mixed, small and other groups are game data a disc may
   well hold that the byte survey could not place: the twin method by owning
   function (step 5 section 6 item 3) is the way to place them, one table at
   a time, before the engine settles for holding any of them itself.

## 7. The hook

`importer.py build` writes `base/exe/` after the containers, from the PC's
`BOF3.exe` when it is a source, else from the first disc in the player's
order (`exe_tables.build_from`), and records `[exe]` (build, file, sha256) in
`manifest.toml`. `importer.py verify` checks `base/exe/data.bin` against its
`data.toml`, the manifest, and `recipes/exe.toml`'s hash for that build - the
PC's section hash when the PC was the source, the recorded disc-built image
otherwise. `importer.py check` (CI) runs `exe_tables.py check` too: the
pointer list hashes as the recipe says, its runs are in order inside `.data`,
every build is `fixtures.toml`'s, each build's counts add up to the image.

Proved: JP disc + PC `DAT/` + `BOF3.exe` built and verified (742 of 742
containers, `base/exe/` from `pc-zh`, 0 problems); the US disc alone built
and verified (`base/exe/` from `psx-us`); one byte of the US `data.bin`
changed and `verify` reported all three mismatches and exited 1;
`exe_tables.py build` + `verify` for the PC, US and PSP-JP each.

## 8. Commands

```
D="--disc <JP cue> --disc <US cue> --disc <EU cue> --disc <FR cue> --disc <DE cue> --disc psp-jp.iso --disc psp-eu.iso"
python tools/exe_tables.py recipe  --game <dir with BOF3.exe> $D     # recipes/exe.toml, recipes/exe-pointers.tsv
python tools/exe_tables.py measure --game <dir> $D --out analysis/exe_import   # sections 3, 4, 5.3
python tools/exe_tables.py xref    --game <dir> $D --out analysis/exe_import   # section 6
python tools/exe_tables.py build   --source <BOF3.exe or a disc> --out <cache>
python tools/exe_tables.py verify  --cache <cache>
python tools/exe_tables.py check
python tools/importer.py build --source <disc> [--source DAT --source BOF3.exe] --out <cache>
python tools/importer.py verify --cache <cache>
python tools/tables.py check; python tools/importer.py check
```

`recipe` and `measure` take about 13 s for all seven discs. The section 3 and
4 tables are `measure`'s output; `exe.toml` carries the same counts per build.

## For the other files

Not edited here (the coordinator folds them in):

- **`docs/unified-data-plan.md`** section 5 step 3: the importer half done
  (`tools/exe_tables.py`, `base/exe/` from the PC or any disc, 87.0 % of the
  JP-built image equal to the PC's, every catalogued table a disc carries
  identical but the three regional rows); the engine half open, its worklist
  [`exe-import.md`](exe-import.md) section 6. Section 8 row 8: "importer half
  done 2026-10-08; the engine half waits on the owner's machine and the
  state hash". Section 2's cache sketch: `base/exe/` is `data.bin` +
  `data.toml`. Section 8's closing line ("After 8, a player with a US disc
  and no PC install has everything but music and video") wants a caveat: not
  until the engine holds the code pointers and the ~22.7 KiB no disc carries
  (section 6 here).
- **`docs/exe-tables-by-build.md`** section 6: item 2 (the pointer tables)
  answered - not copied, left unfilled, listed in `recipes/exe-pointers.tsv`
  (19,311 words; 6,071 pointer-shaped words are numbers); the data pointers
  are 93 % rebuildable from a PSX disc by the map run backwards (section 5.3
  here), the code pointers are the engine's. Item 3: `Char_DefaultRecords`
  is now also seen in the images' differing bytes (section 3 here). A new
  finding for section 4.4: the JP disc's `AREA026` section 13 differs from
  the PC, US and PSP-JP in 21 bytes (PC `0x5EB38D..0x5EB3B4`).
- **`docs/platform-layers-plan.md`** section 3, state 3: "maps the player's
  `BOF3.exe` data sections ... without touching a pointer" holds for data
  pointers only. 9,142 `.data` words point into `.text` (6,346 targets) and
  state 3 maps no Capcom code: the engine rewrites them to its own functions
  or maps entry stubs at Capcom's addresses. And the data the engine maps
  becomes `base/exe/data.bin` (any source), plus `.rdata` (65 addresses
  `src/` reads; the PC only, or the engine's own).
- **`docs/importer.md`**: `build` writes `base/exe/` and `[exe]` in the
  manifest; `verify` checks it; `check` runs `exe_tables.py check`.
- **`docs/HANDOFF.md`**, the engine half's worklist: (1) map `base/exe/data.bin`
  at `0x5DA000` in place of the exe's `.data`, zero to `bss_end`; (2) the code
  pointers (section 6 item 1); (3) the data pointers from a disc (item 2;
  hold, or a rebuild transform); (4) `.rdata`'s 65 addresses; (5) the 321
  addresses no disc carries, platform first, the rest by the twin method;
  (6) the state hash checks a disc-built image against the PC-built one,
  using `data.toml`'s ranges as the skip list.
- **`docs/STATUS.md`**: unified-data step 8, importer half done.
- **`.github/workflows/checks.yml`**: nothing to add - `importer.py check`
  now runs `exe_tables.py check`.

### The owner's calls

1. **`recipes/exe-pointers.tsv` in the repo.** It lists where the PC's
   pointer words are (address, word count, the section pointed at): no
   values, the layout fact `symbols.toml` is full of, but 3,134 lines derived
   from the executable. Kept as committed under rule 1's "addresses and
   counts" reading; say if it should be generated on the player's machine
   instead (it needs the PC's executable and the discs to generate, so a
   disc-only player could not).
2. **Names in `base/exe/`.** From a disc the six name tables' names are
   blank (they are a language layer's, as step 3 decided for the enemy
   tables); from the PC the image carries the Chinese names, since it is the
   section as is. Splitting the PC's names out into `loc/zh-CN/` too would
   make `base/exe/` one thing from every source; the engine's DIV-0008 path
   already lays names over these tables for the other languages.
3. **A PSP-only install's `base/exe/`** takes the PSP's level table for one
   character and its consumable 87 (the PSP's content), and has no
   `sin_table`. The plan's policy makes a later build's content change an
   `opt/` layer, off by default; with only a PSP disc there is no other
   source. Accept (and ledger it when the engine reads it), or require a PSX
   disc or the PC for `base/exe/`.
4. **The data pointers from a disc**: held by the engine as layout, or a
   rebuild transform (section 5.3)? Recommended: the transform, since 93 %
   rebuild exactly and each word can be checked against the PC-built image's
   hash per build as everything else here is.
