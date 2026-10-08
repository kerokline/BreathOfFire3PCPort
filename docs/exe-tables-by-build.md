# The exe-resident tables by build: the catalogue and the per-build address maps

**Status:** IN PROGRESS (2026-10-08, cloud session; unified-data step 5,
[`unified-data-plan.md`](unified-data-plan.md) sections 5 and 8. Measured
against `BOF3.exe` and the US, FR, DE and Europe-English PSX discs and both PSP
discs. **The JP PSX disc was not uploaded this session**, so the `psx-jp`
rows are the addresses already recorded in `symbols.toml` plus ones read from
the PSP-JP disc's copies of the EMIs, tiered `hypothesis`: section 6 has the
one command that settles them.)

The evidence rule ([`README.md`](README.md)) holds throughout: every count
below is followed by the command that produced it, and every address in
[`tables.toml`](../tables.toml) and [`exe_maps/`](../exe_maps) carries the
way it was found. No table value appears here, in `tables.toml`, in the maps,
or in a commit message (rule 1). The outputs of `exe_twins.py survey` and
`tables.py dump` go to `analysis/` or the terminal.

## 0. What came out

1. **A per-build address map of the PC's whole `.data`**:
   [`exe_maps/<build>.tsv`](../exe_maps), one per held build but JP. Each line
   says a range of `BOF3.exe`'s initialised data is the bytes of a file in that
   build (the boot EXE, an EMI section, or the PSP ELF), from an address on.
   The PSX maps place **518,246 to 518,532 bytes of the PC's 638,976 bytes of
   initialised `.data`** (81 %), the PSP maps 521,204 and 521,217 (section
   3). Any PC address, a symbol or one of the 1,067 raw constants `src/` uses,
   is looked up there by address; nothing has to be named first. This is what
   step 8 (the importer writing `base/exe/`) reads.
2. **The catalogue**: [`tables.toml`](../tables.toml) has 29 tables (was 6):
   the six item and ability tables and 23 engine-wide content tables (shops,
   the level table, event battles, encounters, the battle rates, the field
   scripts' op lengths, directions and speeds, the sine table, the PC-only key
   defaults, and the PSX song table, which has no PC twin), with 121 fields,
   every one with a tier and a citation. **202 `[[table.psx]]` rows** across
   seven builds. The other 570-odd of the survey's 603 candidates - area-,
   spell- and scene-local tables `src/` also reads by address - are located
   through the maps, not catalogued one by one (section 2.3 says why).
3. **Tools**: [`tools/exe_twins.py`](../tools/exe_twins.py) (new: the survey
   and the maps), [`tools/tables.py`](../tools/tables.py) (PSP discs,
   `locate`, the maps' check, `pc_twin`, `repeat`, `count_status`).
   `tables.py check` is clean and checks the maps too; CI runs it.
4. **Findings on the way** (section 4): a PSP-only rebalance of one
   character's level table, four PSP-JP key-item renames and an ability
   rename nobody had listed, the event-battle table's real length, the song
   table in every build, and two corrections to `psp-stallion.md`'s units.

## 1. Sources

| Build | Held as | Verified |
|---|---|---|
| `pc-zh` | `BOF3.exe`, `DAT/` | `verify_fixtures.py --exe`: OK; `--tree DAT`: all 742 files match |
| `psx-us` | `.bin` + `.cue` | `--tree`: all 887 files match |
| `psx-fr` | two-track `.bin` + `.cue` | `--tree`: all 889 files match |
| `psx-de` | two-track `.bin` + `.cue` | `--tree`: all 889 files match |
| `psp-jp`, `psp-eu` | `.iso` | `--tree`: all 1,846 / 1,837 files match |
| `psx-eu-en` | two-track `.bin` + `.cue` | **no manifest yet** (fixtures.toml has it as `known`, not held): `--tree` names `psx-de` the closest, 607 of 889 files equal. Track 1's sha256 `1072fb1f…cf3469`, track 2's `ce5509fa…c77` (byte-identical to the FR and DE audio tracks). Identified by its boot line, `SLES_013.04`. |
| `psx-jp` | **not uploaded** | - |

The `psx-eu-en` row of `fixtures.toml` can now become `verified`:
`region_diff.py files <cue> --out fixtures/psx-eu-en.files.tsv` and the hashes
above (not done here: `fixtures.toml` is outside this step's files).

## 2. Method

### 2.1 The survey: `tools/exe_twins.py survey`

```
python tools/exe_twins.py survey --game <dir with BOF3.exe> --disc <cue or iso> [--disc ...] --out analysis/exe_twins
```

- **Candidates.** Every `symbols.toml` `[[data]]` entry in `BOF3.exe`'s
  initialised data (`0x5C4000..0x676000`) that `src/` names, whose bytes over
  its size (its `count` x its `ctype`, else up to the next symbol) are not all
  zero and not all pointers into the image. Those two kinds are runtime state
  and dispatch tables, which become our code, not a disc's data.
- **Sources** per build: the boot EXE (PSX, by `SYSTEM.CNF`) or the ELF's
  first program header (PSP, `PSP_GAME/SYSDIR/BOOT.BIN`, link-time offsets),
  and every EMI section of type 0 bound for main RAM (on the PSP with bit 31
  restored, as `region_diff.nd` does). About 41 MB a PSX disc, 43 MB a PSP
  disc. Art (type 1, 3), audio and video are not searched.
- **Runs.** A 16-byte key every 16 bytes of the PC's `.data` (no pointer word
  in it, at least four distinct bytes) is searched with Aho-Corasick
  (`pip install pyahocorasick`); a key is a seed when its hits fall at no more
  than eight distinct addresses (an overlay copied into many files - the
  battle engine is in `BATTLE.EMI` and every `BOSS` file at one address -
  counts once, extended in its first file: boot, `GAME.EMI`, `BATTLE.EMI`,
  then by name). Each seed is extended both ways while the bytes agree; a PC
  word that points into the PC image is a wildcard (the other build's pointer
  is its own). `.rdata` is not surveyed: it is the compiler's constants and
  the import table, and an arithmetic sequence there matched a spell file by
  chance in the first pass.
- **Pass 2**, for tables no run reaches: the table's wildcard-free head
  searched alone; where several places hold it equally, the place the nearest
  located neighbour in the PC's `.data` predicts (same file, same
  displacement), else the file both nearest neighbours are in, else the file
  the symbol's name names (`Area72_` to `AREA072.EMI`; the weakest, marked
  `name`). A neighbour-predicted place with no search hit counts only when the
  whole table agrees there.

### 2.2 The maps: `tools/exe_twins.py map`

```
python tools/exe_twins.py map analysis/exe_twins/*.json      # writes exe_maps/<build>.tsv
```

Every PC byte goes to the longest run holding it (on a PSP disc the ELF's
runs first: the copy the PSP's code reads), runs with one displacement merge
across gaps of at most 64 bytes, and pass 2's single-table places are added
where no segment holds them. Columns: `pc`, `pc_end`, `file`, `section`,
`addr`, `agree` (bytes compared equal), `copies`, `how` (`run`, `search`,
`neighbour`, `search+neighbour`, `neighbour-file`, `name`). `tables.py check`
validates each file: a known build, segments in order, inside the data, not
overlapping, `agree` no more than the span.

### 2.3 The catalogue: `tools/tables.py locate`

```
python tools/tables.py locate --game <dir> --disc <cue or iso> [--disc ...]   # [[table.psx]] rows to review
```

A table with a name field is found by its numbers (the PC record's numeric
bytes with the name narrowed to the build's width, `[meta] name_len`), across
every source, allowing two records to differ and saying which; any other
table through the build's map, then read back from the disc and compared
byte for byte. The rows' `cite` is the comparison.

Why 29 tables and not 600: the brief asks for fields "as far as they are
read", which means reading each one's readers. That was done for the tables
every part of the game reads (the plan's "content tables"). The rest are
local to one area, one spell effect or one scene (`Area45_Cells`,
`BlizzardShard_Offsets`, `Scena10_Pickups`, ...), their element types are in
`symbols.toml`, their places are in the maps, and the importer needs the
places, not the fields. Each can be promoted to a `[[table]]` when someone
reads it.

## 3. Counts

**The candidate set** (`python - <<EOF` over `symbols.toml` and `src/`, the
same selection `exe_twins.candidates` makes):

| | Count |
|---|---:|
| `symbols.toml` `[[data]]` entries | 2,691 |
| in the PC's initialised `.rdata` / `.data` | 2,372 |
| named in `src/` (a word match over `*.cpp`, `*.h`) | 2,058 |
| survey candidates (not zero, not pointers only) | **603** |
| raw `.data` addresses in `src/` code that start no symbol | 1,067 (837 named by a `k...` constant) |

**The survey** (`exe_twins.py survey`, all six discs in one run, 2026-10-08;
`whole` = the table's size or extent equal at one place; `-multi` = equal at
several and not resolved; `inside` = a run starts inside it, its head
differs):

| Build | whole | part | whole-multi | part-multi | inside | none |
|---|---:|---:|---:|---:|---:|---:|
| `psx-us` | 469 | 18 | 53 | 1 | 25 | 37 |
| `psx-fr` | 470 | 18 | 53 | 1 | 25 | 36 |
| `psx-de` | 468 | 17 | 56 | 1 | 25 | 36 |
| `psx-eu-en` | 469 | 18 | 53 | 1 | 25 | 37 |
| `psp-jp` | 423 | 15 | 104 | 3 | 23 | 35 |
| `psp-eu` | 422 | 15 | 105 | 3 | 24 | 34 |

The PSP's extra `-multi` are the ELF's copy and the EMI's copy of the same
table at two addresses, both real; the map takes the ELF's.

**The maps** (`exe_twins.py map`; `agree` summed):

| Build | segments | `.data` bytes placed | of which by `run` | raw `src/` constants inside a segment |
|---|---:|---:|---:|---:|
| `psx-us` | 649 | 518,452 | 568 segments | 576 of 1,067 |
| `psx-fr` | 642 | 518,246 | 560 | 573 |
| `psx-de` | 646 | 518,299 | 563 | 572 |
| `psx-eu-en` | 648 | 518,532 | 567 | 580 |
| `psp-jp` | 4,096 | 521,204 | 4,049 | 599 |
| `psp-eu` | 4,102 | 521,217 | 4,055 | 596 |

Where the bytes are on a PSX disc (US, `agree` summed by the file's first
path component): the five `WORLD0n` directories' area overlays 462,357;
`ETC` (mostly `GAME.EMI` section 0: the shops, menus and tables) 22,093; the
boot EXE 12,915; `BATTLE` 10,227; `BMAGIC` 4,658; `SCENARIO` 4,509; `BOSS`
1,693. On the PSP the ELF holds 433,981 (EU) and 434,334 (JP) of them: its native compile carries every area's data
in `.data`, where the PSX loads it per area. The PSP map is in many more,
shorter segments because that compile lays its `.data` out in a different
order.

What is not placed (the other 19 %): the pointer tables (wildcards seed
nothing), the PC's Chinese text and formats, the port's own data (DirectX,
the CRT, the config, the file names, the pause lines, the keys), the tables
the port re-laid (section 5), and four- to eight-byte tables too common to
place.

## 4. Results

### 4.1 The catalogue's rows

The table below is `tables.py list` condensed; `(h)` marks a `hypothesis`
row, every other row is `evidence`. `GAME.EMI#0` is `ETC/GAME.EMI` section 0;
`BOOT.BIN` addresses are the ELF's link-time offsets.

| table | `psx-jp` | `psx-us` | `psx-fr` | `psx-de` / `psx-eu-en` | `psp-jp` | `psp-eu` |
|---|---|---|---|---|---|---|
| `consumables` .. `abilities` (6) | GAME.EMI#0 | GAME.EMI#0 | GAME.EMI#0 | GAME.EMI#0 | BOOT.BIN, and GAME.EMI#0 at the JP address | BOOT.BIN |
| `shop_records`, `exp_table`, `event_battles`, `encounter_slot_chance`, `encounter_areas` | GAME.EMI#0 (h) | GAME.EMI#0 | GAME.EMI#0 | GAME.EMI#0 | BOOT.BIN | BOOT.BIN |
| `damage_variance`, `holy_affinity` | BATTLE.EMI#3 | BATTLE.EMI#3 | BATTLE.EMI#3 | BATTLE.EMI#3 | BOOT.BIN | BOOT.BIN |
| `status_resist`, `psi_affinity`, `status_resist_20`, `hp_damage_variance` | BATTLE.EMI#15 (two (h)) | BATTLE.EMI#15 | BATTLE.EMI#15 | BATTLE.EMI#15 | BOOT.BIN (`hp_damage_variance`: BATTLE.EMI#15) | the same |
| `steal_rates`, `skill_steal_rates` | MAGIC065 / MAGIC216 #0 (h) | the same (h) | (h) | (h) | the EMIs and two ELF copies (h) | (h) |
| `area_descriptors`, `move_speeds`, `sprite_key_adjust` | BOOT | BOOT | BOOT | BOOT | BOOT.BIN | BOOT.BIN |
| `direction_steps`, `sin_table` | - (not recorded) | BOOT | BOOT | BOOT | BOOT.BIN / not found | BOOT.BIN / not found |
| `move_op_lengths`, `event_op_lengths`, `direction_angles` | GAME.EMI#0 | GAME.EMI#0 | GAME.EMI#0 | GAME.EMI#0 | BOOT.BIN | BOOT.BIN |
| `songs` (no PC twin) | BOOT | BOOT (h) | BOOT (h) | BOOT (h) | BOOT.BIN (h) | BOOT.BIN (h) |
| `key_defaults` (PC only) | - | - | - | - | - | - |

Counts by build and tier (`tables.py list`, counted): `psx-jp` 17 evidence
and 9 hypothesis; `psx-us`, `-fr`, `-de`, `-eu-en` 25 and 3 each; `psp-jp`
30 and 5; `psp-eu` 24 and 5.

- **`psx-de` and `psx-eu-en` share every address** in the catalogue; `psx-fr`
  is 4 bytes later in `GAME.EMI` and at the same places in the boot EXE and
  `BATTLE.EMI`. The PAL builds' layouts are one build's, the text aside.
- **The JP item tables' file is settled**: `GAME.EMI` section 0, not the boot
  EXE. The PSP-JP disc's `GAME.EMI` holds all six at exactly the JP addresses
  `symbols.toml` records, and the PSP keeps the PSX's EMI data sections
  (`region-diff.md` 5). The JP disc itself was not re-read.
- **Every numeric byte of the six item and ability tables is equal** on US,
  FR, DE, Europe-English and the PSP-EU ELF (`numbers equal in all N records`,
  the rows' cites), but consumable 87 on both PSP ELFs (section 4.4).
- The PSX's tables of the field engine are in `GAME.EMI` and the boot EXE,
  the battle's in `BATTLE.EMI`'s sections 3 and 15 (and the copies in every
  `BOSS` file), the steal rates in the two spell files that steal.

### 4.2 The item and ability tables on the PSP, against `psp-stallion.md`

- `psp-stallion.md` 3.2 and 5.1 give the PSP-EU ELF's tables at **file
  offsets** (`psp_stallion.py tables` searches the whole `BOOT.BIN`); the
  catalogue uses **link-time offsets** (`psp_elf.py`'s convention), 0x80
  less: the ELF's first program header starts 0x80 into the file (`psp_elf.py
  sections`: `.text` at offset 0x80). The accessories' `0x2E6ED8` there is
  `0x2E6E58` here.
- Its ability table frames a record as the eight parameter bytes then the
  next name (`0x801CA718` on the US disc); the catalogue frames it as the
  readers do, name first (`Ability_Records`, `0x801CA70C` on the US disc, 12
  earlier). So its "id 115" is `Ability_Records` id 116, and its renamed ids
  45, 46, 115, 217 are 46, 47, 116, 218 here - measured: the PSP-EU ELF's
  names differ from the US disc's `GAME.EMI` at exactly 46, 47, 116 and 218.
  Which framing the AI rows' ability ids use was not re-read.
- **`psp-stallion.md` section 6's two open items, closed:** the PSP-JP ELF's
  accessory records equal the PC's numbers in all 52 (so the Holy Mantle's,
  21, is the PSX's on PSP-JP too); and **the PSP-JP ELF renames one ability**,
  `Ability_Records` id 116 - the same record the English PSP renames - and no
  other (its names against its own `GAME.EMI` copy, which is the JP disc's).

### 4.3 The song table in every build

`bgm-comparison.md` 6.2 has the JP table at `0x80182830` in `SLPS_009.90`:
165 entries of `{u16 file id, u8 seq, u8 sub}`, `seq` 0 throughout, `sub`
0..3. With no JP disc and no PC twin to search by, it was found **by shape**:
runs of words whose third byte is 0, fourth at most 3 and first half-word
below 0x1000, in each boot EXE and ELF (a scratch script over
`exe_twins.sources`). Each build has two runs of 165 or more: one of 215
words whose fourth byte is always 0 (another table), and one whose fourth
byte takes all of 0..3 - 167 words on the four PSX discs, 166 on the PSP
ELFs. On the US disc that run's word 165 is a file id far outside the others'
range, so the table is its first 165, as the JP description has it. **Those
165 entries hash the same in all six builds** (sha256 of the 660 bytes,
truncated `2460cab356d5`). US
`0x80181EB4`, FR / DE / Europe-English `0x80182380`, PSP-JP `0x2E5808`,
PSP-EU `0x2E5020`. Tiered `hypothesis` until the JP twin is compared byte for
byte.

### 4.4 Content differences the survey turned up

None of these is in [`region-diff.md`](region-diff.md)'s rows, because that
diff compares EMI sections and the PSP's changes here are in its ELF.

- **The PSP rebalanced one character's level table.** `exp_table` is equal
  in all 5,544 bytes on every PSX build held (and in PSP-JP's `GAME.EMI`
  copy); both PSP ELFs differ from it in 249 bytes, all in roster index 6's
  99 rows, in the EXP and the stat-gain bytes (`+0..+5`), not in the learnt
  abilities (`+6`, `+7`). The two PSP ELFs' copies are equal to each other in
  all 5,544 bytes. A lead for I32's list.
- **Consumable 87** differs on the PSP-JP ELF as on the PSP-EU ELF
  (`psp-stallion.md` 3.2 had it for EU only): a PSP change, not an English
  one.
- **PSP-JP renames key items 2, 5, 7 and 9** and ability 116 against its own
  `GAME.EMI` copy (section 4.2); weapons, armour and accessories keep their
  names.
- **`EventBattle_Records` is 56 records**, not the 97 to the next symbol:
  records 0..55 are equal on every PSX build and none after them is. The
  catalogue's count is 56 (`count_status` hypothesis).
- **`EventScript_OpLengths` entry 15** (never used) is the JP disc's value
  only on the JP disc: US, both PSP ELFs and the PC agree with each other
  there (via PSP-JP's `GAME.EMI` copy; `symbols.toml` had JP's).
- **`Battle_DamageVarianceTable` is not "u16 on the PSX"** (its
  `symbols.toml` note): the US `BATTLE.EMI` and PSP-JP's copy hold the same 32
  bytes as the PC's eight u32s.
- `Sprite_KeyAdjust` is 16 bytes to the next named data, not `0x300`; the
  `0x300` is the reach of an unbounded byte index times 3. All 16 equal on
  every build.

## 5. What was not found, and why

From the survey's `none` and `inside` rows on `psx-us` (62):

- **The port's own data** (no disc has it): `Imp_GetTickCount` and the import
  table, `DInput_*`, `Cfg_Fullscreen`, `Cfg_RenderMode`, `Cursor_Hidden`,
  `Gfx_ScreenRect`, `D3d_AlphaOpCache`, `Gpu_OtTerminator`, `Task_StackTop`,
  `Key_TableDefault`, `Music_LoadedTrack` (the MP3 player's).
- **Language**: the six name tables by bytes (found by numbers instead,
  section 4.1); `Menu_Verbs`; the Chinese message formats and choice lists
  (`Area08_MessageFormat`, `Area13_ChoiceMessages`, `Area21_Messages`,
  `Area130_ChoiceMessages`, ...); `Scena17_RollLines` (the credits).
- **Re-laid by the port**: the world maps' plate drift tables
  (`Area16_DriftUV`, `Area45_DriftUV`, ... nine) - the PC's plate pages are
  re-packed (DIV-0055's territory); `Area104_PlateAnims`;
  `Gte_IdentityRotation` (the PC's GTE shim).
- **Widened**: `Char_DefaultRecords`, the eight new-game character records,
  whose name is 9 bytes on the PC and 5 on the discs (`symbols.toml`: equal
  past the name, four bytes earlier). Needs a name-aware find like the item
  tables'; the importer's transform is the save converter's widening.
- **`WorldMap_Records`** (head differs), `Area0_Descriptor`,
  `Area26_EffectKinds`, `Area121_BandHeights`: pointer-led, a run begins
  inside them.
- **Too small to place**: twenty four- to eight-byte tables
  (`Area15_PosesA`, `SnapWave_FacingPhase`, `Scena04_Quake`, ...), equal in
  too many places, with no located neighbour to choose by.
- `sin_table` on the PSP: the PSP's native code does not use the PSX
  library's table (not found in its ELF or EMIs).

## 6. Owed

1. **The JP disc.** One command settles every `psx-jp` row and writes the
   JP map:
   ```
   python tools/verify_fixtures.py --tree <JP cue>
   python tools/exe_twins.py survey --game <dir> --disc <JP cue> --out analysis/exe_twins
   python tools/exe_twins.py map analysis/exe_twins/psx-jp.json
   python tools/tables.py locate --game <dir> --disc <JP cue>
   ```
   then the nine `hypothesis` rows replaced, `direction_steps` and
   `sin_table` given theirs, the song table compared byte for byte.
2. **Names for the raw constants.** 1,067 `.data` addresses in `src/` start
   no symbol; 576 of them are inside a US map segment already. They need
   `symbols.toml` names only where a reader wants one; the importer does not.
3. **The pointer tables** (`Area_Descriptors`, `Magic_Rows`, the area handler
   arrays): their places are known, their contents are the disc build's own
   addresses. Step 8 rebuilds them from the PC's layout, never copies them.
4. `Char_DefaultRecords` by a five-byte name; the twenty small tables by the
   owning function's PSX twin (the catalogue's labels) rather than by bytes.

## 7. For the other files (not changed here)

- `docs/unified-data-plan.md` section 5 steps 1-2 and section 8 step 5: done
  but the JP run (section 6 above); section 1's "every exe-resident table's
  address on each SKU" open item: answered for all held builds but JP.
- `docs/psp-stallion.md`: its ELF addresses are file offsets (+0x80 on this
  catalogue's), its ability ids are this catalogue's minus one, and its
  section 6's two items are closed (section 4.2).
- `docs/region-diff.md` / I32: the PSP ELF's level-table rebalance and the
  PSP-JP renames (section 4.4).
- `fixtures.toml`: `psx-eu-en` can be verified (section 1).
- **`symbols.toml` quotes table values** in the evidence strings of about 25
  `[[data]]` entries (a regex for six or more numbers in a row, function
  address lists excluded): `Battle_DamageVarianceTable`,
  `Battle_HolyAffinityTable`, `Steal_RateTable`, `Encounter_SlotChance`,
  `Field_MoveSpeeds`, `EventScript_OpLengths`, `Sprite_DirectionAngles`,
  `Clut_Sixteens` / `_PerRow` / `_Stride`, `Gte_IdentityRotation`,
  `Sparkle_CountByKind` / `_DelayByKind`, `FieldMenu_TitleIds` / `_IconIds`,
  `WorldMap_Records`, `WorldMap33_PlateAnims` / `_PlaceMessages`,
  `Area29_Weights`, `EffectKind06_Anims`, `Field_EventCells`,
  `Field_BlockingCells`, `MapCell_UprightCounts` / `_UprightHeights`,
  `BattleObj_StateTable`; and `Field_EncounterAreas`' pairs, which the regex
  misses. Rule 1 says these go; the owner's call, since the strings are
  older than this step.
