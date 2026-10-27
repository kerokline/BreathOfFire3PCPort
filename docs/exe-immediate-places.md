# The owning-function place kind: `imm` and `immref` rows

**Status:** IN PROGRESS (2026-10-18, install-gap round stage A, group OT, branch
`install-gap/ot` off `06a209bd`. Built, checked and measured on the seven held
disc builds; the regenerated recipes are not committed, the coordinator
regenerates them at the merge. Nothing played. **Section 7 added 2026-10-26:** the
effect, boss and scene rows read by hand on a worktree off `main` `25d12620`;
12 rows placed, four rule changes, the recipes regenerated and checked there.)

The kind [`exe-import-engine.md`](exe-import-engine.md) 3.3 measured and did not
build, staged in [`install-gap-round.md`](install-gap-round.md) 2 (OT): a gap row
of the disc-built image is rebuilt from the code of the PSX twin of a PC
function that reads it. Addresses, symbol names and counts only (CLAUDE.md rule 1).

## 0. What came out

1. **Stage B is small as staged.** Of the 66 P3 rows of the census's section 4
   (68, less `Area146_EffectStates` and `Area169_Rects`, which RD moved to TW),
   64 have a reader and **11 have a reader with a PSX twin after the feed-back
   (10 before)**: 16 of the 137 readers have a twin, 13 before. None of the
   effect, magic, boss and scenario rows (OF-E's family and most of OF-A's) gains
   one. Their readers are effect-state functions reached through code-pointer
   tables that no placed word pairs, so the code-pointer pairs do not reach them.
   Stage B needs function twins found another way (section 6) before most of its
   rows can be placed.
2. **The rows the census called "folded into instructions" were data on the
   disc** wherever a twin could be read. The twin's code holds the *address* of
   the row's bytes, as a `lui` and a `%lo` load, in its own overlay. The values
   are not immediates. Searched over every region with a twinned reader, the
   value shape (`imm`, a local array filled by `li` / `sb`) matched nothing on any
   build. The address shape (`immref`) placed every row this round places. The
   two P3 rows it places are the last bytes of their overlay section, past the
   last whole word a place or segment can take, and too short for a 16-byte
   window.
3. **The kind is built:** `tools/exe_imm.py` (the decoder and the search),
   `recipes/exe-imm.tsv` (`exe_tables.py recipe` writes it, `build` applies it),
   and `exe_tables.py check`. The check refuses a row its disc does not decode,
   or one that does not rebuild the PC's bytes when given the discs and the PC.
   Both shapes are in the format and the decoder. Section 1.
4. **Placed** (section 2): 7 rows on each PSX build (20 PC non-zero bytes) and 1
   on each PSP build (3). Two are P3 game-value rows: `Area26_EffectKinds+0x4`
   and `Area94_EffectArgsE`. Five are the census's "area message words" P3 item.
   These are u16 message numbers classed `language` by name: `Area05_MessagesB`,
   `Area08_ChoiceMessages`, `Area10_MessagesB`, `Area21_Messages+0x8` and
   `Area150_MessagesB+0x4`, on all five PSX builds. PSP-JP and PSP-EU get
   `Area08_ChoiceMessages`. `placed_differ` is unchanged on every build, and
   `identical` rises by the bytes placed.
5. **The code-pointer pairs are fed back** (section 3): 944 new `psx` fields at
   `psx_status = "hypothesis"`, and 376 `psx_overlay` fields added to existing
   `psx` addresses the pairs confirm. Each pair was checked by a call and
   constant fingerprint first. **A tenth of the raw pairs are wrong**: a placed
   table shifted one entry against the PSX's (area 26's handler table is the
   case read). The fingerprint check refuses most of those.
6. **The kind meets one engine rule** (staging section 8): on PSX-US the
   `immref` search would place D310's level-up ability-name format
   `0x654900..0x654904` from `SCENARIO/SCENA10.EMI`. `exe_own.cpp` holds that
   region as engine-own `game-format`, so it is excluded and nothing changes.
   Whether a disc's copy should replace the engine's there is the coordinator's
   call, or the owner's. Section 4.

## 1. The kind

### 1.1 Rows

`recipes/exe-imm.tsv`: `build kind pc pc_end file section func at insns`.
`func` is the twin's address in `file` / `section`. Offsets in `insns` are hex
from `func`. The two shapes are:

- **`immref`**: `insns` is `H+L`. `H` is a `lui`. `L` is an instruction on
  the lui's register that adds the low half: `addiu`, `ori`, or a load or
  store. `addiu`'s and a load's 16-bit immediate are sign-extended. The row's
  bytes are the file's at that address plus `at`, in the one section of `file`
  that holds them.
- **`imm`**: `insns` lists the elements `S=V`, `S=H+L` or `S=z`. `S` is a
  `sb` / `sh` / `sw` of the value made by the `li` at `V` (an `addiu` or `ori`
  from `$zero`, or a lone `lui`), or of the pair `H+L`, or of `$zero`. `at` is
  the displacement of `pc` off the stores' one base register. Each element
  lands at `pc + disp(S) - at`.

Decoding refuses any of these, loudly: an opcode that is not the row's, a value
made in another register than the one stored, a value wider than its store,
two elements over one byte, stores off different base registers, a pair whose
low half does not read the lui's register, an address in no section or in two.
`exe_tables.disc_image` applies the rows last, only to bytes nothing else
filled, as `place` ranges whose `from` ends `:imm` or `:immref` (the C++ image
loader's existing class). `psp_exe`'s recipe base image leaves them out, so
`TWINS` rows keep precedence.

### 1.2 How `recipe` finds them (`exe_tables.find_imm`)

For each run of the image that no range fills, outside the pointer words and
the engine's own bytes, holding a byte the PC has non-zero:

1. **Readers**: every 4-byte value at any offset of `.text` that lies from the
   region's data symbol (at most 0x400 below) to the region's end, credited to
   its `[[func]]` (`text_readers`). The 0x400 bound keeps the artefact RD found
   in `exe_own.py measure` out: a row far past its symbol inherits that
   symbol's readers.
2. **Twins** (`build_twins`, `sym_twins`): each placed code-pointer word's pair
   from this build's own image (`code_pairs`, factored out of `find_places`
   unchanged), where a section of the word's own file holds the address. On
   psx-jp, also symbols.toml's `psx`, in the section `psx_overlay` names or else
   in the boot executable or a file the pairs name. On the other builds, each
   psx-jp twin found again by its body, exactly once, in the same file section
   (`body_signature`: jump targets, lui immediates and lui-based low halves
   masked).
3. **Match**: an `immref` where an address a twin's lui pair forms, displaced
   as the reader's operand is from the region, holds the region's bytes. Every
   number byte from the operand to the region's end (or the section's end) must
   equal the PC's, four or more of them, two non-zero - or, since 2026-10-25
   (issue #149), a whole region of three non-zero bytes or more, every one
   equal, the row anchored by the twin's address: `BoneDanceShake_Counts`'
   three beat lengths and `EffectKind75_States+0xC`'s two shades and a step,
   each followed by a zero the PC pads with. An `imm` where the twin's
   stores of constants rebuild every non-zero byte, two elements or more. A
   region that two places fit is left as ambiguous.

`recipe` writes `exe-imm.tsv`, its hash (`imm_sha256`) and each build's
`imm_rows` into `exe.toml`. A tree with no `exe-imm.tsv` and no `imm` key
checks clean, which is this branch as committed.

### 1.3 Checks

- `python tools/exe_imm.py selftest` (also run by `exe_tables.py check`)
  assembles a synthetic function and checks a few behaviours. It has a frame, a
  three-byte local array, a halfword, a lui / ori word, a `$zero` byte, a
  negative `addiu` and a lui-addressed load. It has a call whose delay slot
  keeps a value and a branch past the first `jr $ra`. The checks:
  - the extent, the stores and the refs;
  - a unique match and its elements, and the decode back to the run;
  - twelve refusals;
  - an ambiguous run;
  - `decode_ref`, with addiu's sign;
  - `find_body`, with the masked fields moved and with a kept field changed;
  - `pair_agrees`.
  It holds no game data.
- `python tools/exe_imm.py fixtures <discs>` runs
  `tools/exe_imm_fixtures.tsv`, which holds addresses and counts only. The
  extents, store and ref counts and an address pair's decode are checked on
  three psx-jp area functions and one psx-us one. One check finds a psx-jp
  function's body in psx-us's copy of its file, four times: a templated area
  handler, so `build_twins` takes none of the four. Five rows, 0 problems.
- `python tools/exe_tables.py check`: 0 errors on the committed tree. On the
  regenerated recipe, with `--game` and all seven `--disc`s, it decodes every
  row on its disc against BOF3.exe: 0 errors. Three rows were corrupted by hand
  and each was refused:
  - `at` moved past the section's end: in no section;
  - `L` pointed at a nop: not addiu / ori / a load or store;
  - `at` moved one byte: does not rebuild BOF3.exe's bytes.
- `exe_tables.py build` from the psx-us disc with the regenerated recipe
  matches `exe.toml`'s hash, and `verify` gives 0 problems. `data.toml` names
  the seven `:immref` ranges.
- `importer.py check`: 0 errors. `verify_fixtures.py`: the PC exe and the JP
  disc tree match. The sibling's boot-EXE path is absent from a worktree, as
  before.
- The regeneration changes `exe.toml` and adds `exe-imm.tsv`. Every other
  recipe comes out byte-identical, so the `code_pairs` refactor moved nothing.

## 2. What was placed, and what was not

Per PSX build (`psx-jp`, `-us`, `-eu-en`, `-fr`, `-de`), all `immref`, the
twin in the area's own overlay, section 13:

| Symbol | Range | Twin file | Census status before |
|---|---|---|---|
| `Area05_MessagesB` | `0x5DF6EC..0x5DF6F4` | `WORLD00/AREA005.EMI` | P3 verify-first (area message words) |
| `Area08_ChoiceMessages` | `0x5E10BC..0x5E10C0` | `WORLD00/AREA008.EMI` | as above; also both PSP builds |
| `Area10_MessagesB` | `0x5E23F4..0x5E23FC` | `WORLD00/AREA010.EMI` | as above |
| `Area21_Messages+0x8` | `0x5E72CC..0x5E72CD` | `WORLD00/AREA021.EMI` | as above |
| `Area26_EffectKinds+0x4` | `0x5EC5EC..0x5EC5ED` | `WORLD00/AREA026.EMI` | P3 game-value |
| `Area94_EffectArgsE` | `0x616DE4..0x616DED` | `WORLD02/AREA094.EMI` | P3 game-value |
| `Area150_MessagesB+0x4` | `0x636A64..0x636A65` | `WORLD03/AREA150.EMI` | P3 verify-first |

Both game-value rows end at their overlay section's end. Only the bytes the
section holds are placed (`Area26_EffectKinds+0x4`'s first byte, all nine of
`Area94_EffectArgsE`'s that it holds). The PC's bytes after them are zero.

On psx-jp the recipe sees 284 such regions:
- 110 have no reader;
- 89 have readers but none with a twin;
- 85 have a twinned reader. Of those, 78 fit nothing and 7 are placed.

The other builds are alike: 71..74 twinned, 7 placed. PSP-JP has 66 twinned and
1 placed, and PSP-EU 17 before the feed-back and 66 after.

**Not placed, why (measured):** the other P3 game-value rows have no twinned
reader (section 0 item 1), or their twin's code addresses no copy of the
row's bytes. The value shape matched no region on any build. `Area175_ScriptMessages`
(Western) and PSP's `Area08_MessageFormat+0x9FA..` have no twinned reader.

## 3. The feed-back into symbols.toml

`python tools/exe_tables.py feed --game <dir> --disc <psx-jp cue> [--out TSV]
[--write]` takes the placed code-pointer words' pairs from the psx-jp image.
symbols.toml's `psx` addresses are SLPS-00990's. The pairs cover 2,323 PC
functions:

| Verdict | Functions | Written |
|---|---:|---|
| `new`: no `psx` yet, the fingerprints agree | 944 | `psx`, `psx_overlay` (when not the boot executable), `psx_status = "hypothesis"`, `psx_evidence` |
| `overlay`: the entry's own `psx`, confirmed, its file now known | 376 | `psx_overlay` |
| `same` | 1 | - |
| `differs`: the fingerprints agree on another address than the entry's `psx` | 3 | left (below) |
| `copies`: one PC function, a PSX copy per area file | 98 | left |
| `refused`: the fingerprints disagree | 901 | left |

The check (`exe_imm.pair_agrees`) requires two things. First, every call either
side makes to a function the resident twins pair must be matched on the other
side. Second, one call must agree, or the constant sets must overlap by half.
It was tuned on the pairs that already agree with an evidence-tier `psx`, where
the labels are known. Of the 542 pairs that agree, it accepts 386. Of the 59
that disagree, it accepts 5. Three of those five are the `differs` rows:
`0x4140F0`, `0x528CD0`, `0x5295F0`. In the last two, the pair's twin fits
better than the entry's own `psx` (7 and 2 calls matched, against 0 and 1), so
the entry may be the one that is wrong. Read before changing either.

The shifted-table case read by hand: `0x5EC558`'s place
(`WORLD00/AREA026.EMI#13`) pairs each area-26 handler with its neighbour's twin.
`Area26_Flag29Clear` pairs with the function that sets flag 0x29 and clears
0x28. The place agrees on its numbers, and its code pointers were unpaired, so
they were wild. All 15 pairs from it are refused. The pairs keep the
"one file agrees" rule of `exe-import-engine.md` 3.1, which this case passes,
so a later user of `code_pairs` should not treat its pairs as checked.

`symbols.toml`'s header documents `psx_overlay`, `psx_status` and
`psx_evidence`. Afterwards: `tomllib` loads it (10,100 functions, 2,472 with
`psx`, 1,357 with `psx_overlay`), `gen_symbols.py` writes the header (10,088
ours), and `cmake --build build` links. The feed-back leaves the recipe's rows
as they were. A second recipe run after it gives the same seven rows.

## 4. The engine rule it meets (staging section 8)

With the engine's own regions not excluded, the search on psx-jp, psx-us and
psp-jp finds one more row: psx-us `0x654900..0x654904`, an `immref` from
`SCENARIO/SCENA10.EMI`. This is `exe_own.cpp`'s `game-format` region for D310,
the level-up's ability-name format, which `exe_own.py` says each disc keeps in
its reading overlay "where no search can place it". The address shape places
it on US. psx-jp has no such row. Nothing is changed: the region stays the
engine's. If the disc's copy should win where a disc carries it, that needs a
ledger entry (DIV-0101's class), and the call belongs to the coordinator or
the owner.

## 5. Also in this branch

- `tools/exe_own.py` `REGIONS`: `0x666FC6..0x6671F4` `unread`, three lists of
  the PSX title overlay and their tail, as RD asked
  ([`install-gap-readers.md`](install-gap-readers.md) 1.4).

## 6. For stage B and the coordinator

- **Stage B's reach** is 11 rows of 66 with a twinned reader. The two rows
  this round places (Area26, Area94) are among the 11; 9 are left. The effect
  rows need their readers' twins first, from somewhere else: a call-graph or
  BSim pairing of the effect-state functions, or the state tables' own pointers
  once something pairs them. They should not be staged as "read the immediates".
- **Expect `immref`, not `imm`**, wherever a twin exists.
- **A cheaper fix the measurement points at**, not built here (it is
  `find_places`'s and `exe_twins`' rule, and it would move recipes):
  `stretches` treats a word that runs past a section's end as a disagreement.
  The two game-value rows above are section tails that a place could take up to
  the section's last byte. A plain-tail count found 5 regions on psx-jp and on
  psx-us (14 non-zero bytes: 8 `language`, 4 game-value, 2 `port-relaid`).
- **Regenerate after the merge** (staging section 5's chain; `exe_tables.py
  recipe` now also writes `exe-imm.tsv`):
  `psp_exe.py recipe`, `exe_tables.py recipe`, `exe_own.py measure`,
  `exe_tables.py gaps`, `exe_tables.py recipe`. Then `exe_tables.py check
  --game <dir> --disc ...` (the decode against the PC), `psp_exe.py check`,
  `importer.py check`. `exe-gaps.tsv` then loses the placed bytes and
  `0x666FC6..0x6671F4` becomes `unread`.
- `psp_exe.py measure`'s `filled_by_psp` counts every `place` whose `from` has
  a `:`, so it now counts the `immref` ranges too, 1 PSP row. That count is
  PB's to adjust if it matters.

## 7. The effect, boss and scene rows read by hand (2026-10-26)

The remaining `game-value` gap rows of the spells, effects, bosses and scenes
(census 8.5's "twinned, not placed", "readers unpaired" and OF-A lists), each
read at its PSX twin with capstone from the seven held discs, on a worktree off
`main` `25d12620`. Scratch under `D:\bof3x-runs\egt` (`egt.py`, `rows.py`,
`scan.py`, `refscan.py`, `d.py`, `fstart.py`, `bodies.py`, `symfix.py`,
`gapdiff.py`, `chain.sh`; addresses and counts only). Three things came out of
the reading, and four rules grew from them.

### 7.1 What the reading found

1. **Every one of these rows that is a Capcom value is on the disc, verbatim,
   at the address the twin's own `lui` pair forms.** None is folded into
   instructions: `refscan.py` (every lui + low-half pair in every section's
   code of psx-us whose formed address, in a section of the same file, holds
   the row's bytes) finds exactly one reader for `LinkedObjectD_Amounts`
   (`ETC/GAME.EMI#0` `0x801AD958`) and one for `WardMote_Tilts`
   (`BMAGIC/MAGIC043.EMI#3` `0x801EF758`), and the area / scene / magic
   overlays' own readers for the rest (7.3). The rows were unplaced because
   their readers had no twin in `symbols.toml`, or the wrong one.
2. **The kind-0x18 sub-program files were off by one.** The `tables` method's
   slot-to-file choice ([`install-gap-pairs.md`](install-gap-pairs.md)) gave
   `Sub2E` AREA147, `Sub30` AREA158 and `Sub31` AREA161. The sub-program's seven
   functions (`0x801F2C04`, `0x2C48`, `0x2DB8`, `0x2EF8`, `0x2F54`, `0x3064`,
   `0x30D4`) are the same 92-word `_Place` body in AREA158, AREA160, AREA161 and
   AREA162; AREA147's copy is a 107-word variant with other function starts
   (`bodies.py`). Each PC sub's state bytes are one file's and no other
   section's on any disc (`scan.py`): `Sub2E_States+0x14`'s ten bytes are
   AREA158's at `0x801F3434`, `Sub30`'s six AREA161's at `0x801F3438`,
   `Sub31`'s six AREA162's at `0x801F342C`. `Sub2F` (AREA160, already placed)
   fits the same order. `symbols.toml` moved the three subs' 21 entries
   (`psx_status = "evidence"`, the data match and the body set as evidence).
   Which PC sub AREA147's variant is was not read.
3. **One PC function reads another sub's table.** `EffectKind18Sub21_DrawSpiral`
   (`0x505E60`) is AREA091's `0x801F35F4` (the same sprite set-up as
   `Area121_DrawTopBand`'s twin, then rsin / rcos over an `lb` sign table).
   The PC's `EffectKind18Sub52_Draw` (`0x506290`, kind 0x18 slot 0x52's
   handler) is AREA142's `0x801F3118`: its distance test
   `|lh - +0x36| + |lh - +0x3A| < 0x15` and its frame-count x sign loop are
   that function's. It reads the sign table `01 FF 01` from Sub21's states
   (`0x65E330`), where the PSX Sub52 copy has its own at `0x801F355C`.
   AREA091, AREA141 and AREA142 each hold the three bytes; the row is placed
   through AREA142's reader.

### 7.2 The four rule changes (`tools/exe_tables.py`)

- **`build_twins`: several body hits, one at psx-jp's address.** The boss
  `*_Enter` twins' masked bodies match three functions in `BOSS035.EMI#16`
  (`0x800C1860`, `0x800C1994`, `0x800C1A98`: the same state-0 template with
  other tables) on every build but psx-jp, so `build_twins` took none
  (install-gap-pairs.md 8.1's unapplied follow-up). When the hits include the
  psx-jp address itself, in the same file section, that hit is the twin. The
  byte compare still decides the row. `BossGaist_Anims+0x2` places on the six
  builds that lacked it.
- **`find_imm`: the index fold (`IMM_FOLD`, 8 bytes).** `AirRaidImage_Bounce`
  reads its shade table as `[idx*2 + 0x65A696]` for an index that starts at 1:
  the x86 compiler folded the `-1` into the operand, two bytes below the
  symbol `AirRaidImage_ShadeSteps` (`0x65A698`), inside `KickImage_Kinds`' last
  pointer word, so `text_readers` credited it to no region. Its twin
  (`MAGIC015.EMI#0` `0x801F0864`) keeps the `addiu -1` and forms `0x801F1640`,
  the table itself. A reader whose operand lies up to 8 bytes below a region
  that starts at its own data symbol is now a reader of that region, anchored
  at the symbol (`at` 0). The floor (four bytes, two non-zero) is unchanged;
  the four shade bytes meet it.
- **`find_imm`: the row ends at its symbol's declared extent (`data_extents`,
  `ctype x count`).** `WardMote_Tilts` is `short[4]`, and the PSX table at
  `MAGIC043.EMI#3` `0x801EFC9C` is `0400 0800 0C00 0000` followed by the
  overlay's zero padding (the EU disc's copy sits at `0x801EFCA8`). The PC's
  next four bytes `02 00 06 00` are MAGIC044's: the two shorts before
  `EvilEyeChild_Kinds`' code-pointer table, at `0x801F0A18` in
  `MAGIC044.EMI#3`, which no PC operand names. The gap run spans both. Where
  the twin's bytes differ past the reader symbol's declared end, the row ends
  there instead of being refused. The tilts' eight bytes place;
  `WardMote_Tilts+0x8` stays a gap (MAGIC044's bytes, no reader; 7.4).
- **`find_imm`: the whole-region floor at a section's tail.**
  `Area121_BandHeights`' region is `00 78 C8 00 00`, and `AREA121.EMI#14` ends
  with `00 78 C8` at `0x801F68B8`: three compared bytes, two non-zero, and the
  section's end. Issue 149's whole-region floor asked for three non-zero bytes.
  A whole region that reaches the section's end with three bytes or more
  compared, two of them non-zero, now also passes, anchored by the twin.

### 7.3 Placed (`immref`), per build

Each row's twin was read against its PC function (`pcdis.py`, capstone x86)
before the pairing was written; the `psx_evidence` of each entry says what was
compared. The two `_Place` / `_DrawRing` readers also pair, but `find_imm`
records the first reader's row.

| Row | PC range placed | Twin (file, function) | Builds |
|---|---|---|---|
| `BossGaist_Anims+0x2` | `0x64D78E..0x64D798` | `BOSS/BOSS035.EMI#16` `0x800C1860` `BossGaist_Enter` (multi-hit) | US, EU-en, FR, DE, PSP-JP, PSP-EU (JP had it) |
| `EffectKind18Sub2E_States+0x14` | `0x65EB44..0x65EB4E` | `WORLD04/AREA158.EMI#13` `0x801F2C04` `_Run` | JP, US, FR, DE, PSP-JP, PSP-EU (EU-en had a `twin`) |
| `EffectKind18Sub30_States+0x14` | `0x65EB80..0x65EB86` | `WORLD04/AREA161.EMI#13` `0x801F2C04` | JP, EU-en, DE, PSP-JP, PSP-EU |
| `EffectKind18Sub31_States+0x14` | `0x65EB9C..0x65EBA2` (JP, PSPs `..A3`) | `WORLD04/AREA162.EMI#13` `0x801F2C04` | all 7 |
| `EffectKind18Sub45_States+0xC` | `0x65EED8..0x65EEDC` | `WORLD03/AREA143.EMI#13` `0x801F31EC` `_Run` | PSX 5 |
| `EffectKind18Sub21_States+0x8` | `0x65E330..0x65E333` | `WORLD03/AREA142.EMI#13` `0x801F3118` `EffectKind18Sub52_Draw` | all 7 |
| `Area121_BandHeights` | `0x624720..0x624723` | `WORLD03/AREA121.EMI#14` `0x801F6390` `_DrawTopBand` (tail floor) | PSX 5 |
| `Scena02_EffectX` | `0x660E44..0x660E50` (the run begins four zero bytes before the operand: `at` -4) | `SCENARIO/SCENA02.EMI#0` `0x801FCD14` `_PlaceEffect70` | all 7 |
| `AirRaidImage_ShadeSteps` | `0x65A698..0x65A69C` | `BMAGIC/MAGIC015.EMI#0` `0x801F0864` `_Bounce` (fold) | PSX 5 (the PSPs' `BOOT.BIN` map holds the four bytes; 7.6) |
| `LinkedObjectD_Amounts` | `0x660CCC..0x660CD0` | `ETC/GAME.EMI#0` `0x801AE90C` `_ShowAmount` (US `0x801AD90C`, PAL `0x801ADB90`) | JP, US, EU-en, FR, DE, PSP-JP |
| `ReviveHalo_MoteCounts` | `0x65B1D4..0x65B1D8` (PSPs `..D7`) | `BMAGIC/MAGIC077.EMI#3` `0x801EF4A8` `_Spawn` | all 7 (the PSPs since 7.6; the pass wrote "the PSPs fill it", a misread) |
| `WardMote_Tilts` | `0x65A918..0x65A920` (PSPs `+0x1..`) | `BMAGIC/MAGIC043.EMI#3` `0x801EF72C` `_PushMatrix` (symbol end) | PSX 5, PSP-JP, PSP-EU |

Two rows of every Western PSX build changed their named twin without moving:
`0x65E2DC..0x65E2E0` and `0x65E2F4..0x65E2F8` (`AREA102.EMI#13`) now name the
`_Run` twins (`0x801F2C04`, `0x801F3264`) the multi-hit rule gives them, in
place of the `_Place` / draw twins: the same place, the same bytes (`found`
keeps the first reader's row; readers sort by PC address).

### 7.4 Not placed, with the evidence

- **`EffectKind66_States+0x18` / `+0x20` / `+0x28`** (Western PSX, three u16
  x positions `0xD4`, `0xD9`, `0xD9`): `AREA135.EMI#14` at `0x801F6230` holds
  `0x0D4 / 0x0D9 / 0x0D9` on psx-jp and `0x10C / 0x111 / 0x111` on psx-us (the
  pieces moved 56 px right for the Latin layout). The PC's values are the JP
  disc's; the Western discs hold other values, the PC's nowhere. The owner's
  regional call stands (twins 2).
- **`Rest3e_Windows+0x14` / `+0x18` / `+0x1C`** (Western PSX): the rest menu's
  four windows at `SCENA10.EMI#0` `0x801FE7CC` (`EffectKind5E_Wait`'s twin
  `0x801F7758` reads `0x801FE7DC..`). psx-jp and psp-eu hold the PC's
  `0x82 / 0x18 / 0x10E`; psx-us and psx-de `0x81 / 0x6C / 0x70`, psx-fr
  `0x81 / 0x54 / 0x98`: re-laid for each language. The same class as the row
  above; an owner's call, not a placement.
- **`TornadoMote_Phases+0x10`** (`0x65AAA8..0x65AAC4`, all 7): the bytes are
  the ASCII labels `WEAK`, `EXP`, `ITEM`, `POINT` of the Identify panel.
  `MAGIC060.EMI#0` at `0x801F0B64` holds `WEAK / EXP / `` / ITEM / POINT / GET`
  on psx-us and psx-eu-en, `OBJET` on psx-fr, `EP` and `OBJEKT` on psx-de, and
  the psx-jp overlay holds code pointers there (its panel draws no ASCII). A
  `language` row by nature, not `game-value`: the PC's `EXP` (read at
  `0x65AAB0` by `Identify_DrawMember` / `_DrawEnemy`, unpaired) equals the US /
  EU-en disc's; the PC's zero word after it is the disc's `0x60`; `WEAK`,
  `ITEM` and `POINT` have no PC operand. For LA's layer, not a placement rule.
- **`WardMote_Tilts+0x8`** (`0x65A920..0x65A924`, `02 00 06 00`): MAGIC044's
  two shorts before `EvilEyeChild_Kinds` (7.2); no PC operand names them.
  Left as a gap; the neighbour-displacement place rule (import-engine 7.9)
  could take it from `MAGIC044.EMI#3 0x801F0A18` once something reads it.
- **`LinkedObjectD_Amounts` on psp-eu**: the twin's body is in PSP-EU's
  `GAME.EMI#0` at `0x801AD90C` (the US address) and its pair forms
  `0x801C9930` (the JP data address), where the extracted section holds other
  bytes (`01 00 A8 40 ..`); no lui pair of any PSP-EU section addresses
  `00 00 01 0A`. PSP-EU's `GAME.EMI` mixes the layouts; for the PSP round.
- **`Area121_BandHeights+0x1` on the PSPs** (`0x624721..0x624723`): the PSPs
  fill the first byte, and the two left are under every floor (two compared,
  two non-zero).

### 7.5 Before and after

The chain of section 6 run twice on this worktree over the seven discs
(`chain.sh`): once unchanged (every recipe byte-identical but `exe.toml`'s
date), once with the rules and pairings above. `exe-gaps.tsv`'s `gap` rows /
bytes / PC non-zero bytes (`gapdiff.py`):

| Build | before | after | `imm_rows` | `identical` |
|---|---|---|---|---|
| psx-jp | 31 / 367 / 192 | 21 / 299 / 147 | 18 -> 29 | +45 |
| psx-us | 58 / 750 / 539 | 48 / 680 / 494 | 18 -> 29 | +45 |
| psx-eu-en | 59 / 789 / 569 | 49 / 723 / 528 | 17 -> 28 | +41 |
| psx-fr | 62 / 828 / 596 | 52 / 758 / 551 | 16 -> 27 | +45 |
| psx-de | 63 / 836 / 602 | 52 / 758 / 551 | 18 -> 30 | +51 |
| psp-jp | 29 / 330 / 180 | 22 / 270 / 141 | 9 -> 17 | +39 |
| psp-eu | 27 / 296 / 156 | 21 / 240 / 119 | 9 -> 16 | +37 |

The rows that closed are exactly 7.3's; the one row that opened on every
build is `WardMote_Tilts+0x8` (7.4). Since 7.6 (2026-10-26 night) psp-jp reads
21 / 267 / 139 and psp-eu 20 / 237 / 117, `imm_rows` 18 and 17:
`ReviveHalo_MoteCounts` closed on both. `placed_differ` is unchanged on every
build. `exe_tables.py check --game --disc` x7: 0 errors (every new row decoded
on its disc against `BOF3.exe`); `exe_imm.py selftest` 0 problems;
`psp_exe.py check` OK; `importer.py check` 0 errors. No other recipe moved.
Nothing played.

### 7.6 The PSP-only install's revive halo (2026-10-26 night, D394)

The spell sweep's PSP-EU-only run ([`spell-sweep.md`](spell-sweep.md) 9.3)
drew the revive halo without its motes, and its gap watch read
`ReviveHalo_MoteCounts` `0x65B1D4..0x65B1D7` before any write. 7.3's "PSX 5
(the PSPs fill it)" was a misread of `exe-gaps.tsv`'s PSP range, which is
three bytes where the PSX builds' was four: the PSPs fill only the fourth.

What the PSPs hold, read on this branch (`diag.py` / `diag2.py` under
`C:\Users\kerok\bof3x-runs\ss1026\psp`, addresses and counts only):

- Both PSP discs carry `BMAGIC/MAGIC077.EMI` (and MAGIC076, the same image)
  with section 3 at `0x801EEC00`, `0x36A4` bytes, as the PSX discs do;
  `build_twins` finds `ReviveHalo_Spawn`'s psx-jp twin by its body
  (signature 82 words) exactly once in it, at the same `0x801EF4A8`, on
  psp-jp and psp-eu. Its lui pair forms `0x801F049C` (insns `48+50`), and
  the four bytes there equal the PC's `ReviveHalo_MoteCounts` on both. So
  the pairing was never missing: the twin, the table and the reader all
  resolved. (The PSP's own `BOOT.BIN` holds no copy of the body: the
  signature finds nothing in it on either disc. The machinery places from
  the EMI, so nothing else was needed.)
- What took the fourth byte: the PSPs' `BOOT.BIN` map segment that places
  `ReviveMote_Types` (`0x65B1D8..`) begins at `0x65B1D7`, one byte early, on
  a zero byte that is the low byte of a pointer word before the table
  (the map's `BOOT.BIN` address, the loaded image's - the file offset is `0x80` more: psp-eu `0x2F56EB`, psp-jp `0x2F555B`). The region
  `imm_regions` gave `find_imm` was `0x65B1D4..0x65B1D7`: three compared
  bytes, two non-zero, under the four-byte floor and not a section tail, so
  `nothing fits` - and the row stayed a gap with the counts zero.
- The two other "the PSPs fill it" notes are true. `AirRaidImage_ShadeSteps`
  `0x65A698..0x65A69C`: the PSPs' `BOOT.BIN` map holds all four bytes
  (psp-eu `0x2F4B38`, psp-jp `0x2F4998`, the same neighbourhood as MAGIC003's data in [`install-gap-labels.md`](install-gap-labels.md) 4.5), equal to the PC's, no
  `immref` row needed. `WardMote_Tilts`: the map holds `0x65A914..0x65A919`
  (the first tilt byte, a zero; psp-eu `0x2F4C0A`, psp-jp `0x2F4A6A`) and
  the `immref` row `0x65A919..0x65A920` (`at` 1) the other seven; the image
  equals the PC's over the eight.

**The fifth rule (`find_imm`): the owned tail counts for the floor.** When
the compare reaches the region's end and the reader symbol's declared
extent runs past it, the bytes from there to the extent that another range
already holds equal to the PC, and that the twin's table holds equal too,
add to the floor's counts (`agree`, `agree_nz`); the first byte that is
unowned or differs ends the walk. The row placed stays the region's
(`disc_image` writes only what nothing else filled), so `0x65B1D4..0x65B1D7`
places on the PSPs with pc_end `0x65B1D7`, the PSX rows unchanged at
`..0x65B1D8`. The floor itself is unchanged: four bytes compared, two
non-zero, now counted over the symbol where the map already proved the
rest.

After (the chain of section 6 on this branch over the seven discs;
`gapdiff.py`'s `gap` rows / bytes / PC non-zero): psp-jp 22 / 270 / 141 ->
21 / 267 / 139, psp-eu 21 / 240 / 119 -> 20 / 237 / 117, `imm_rows` 17 -> 18
and 16 -> 17; the five PSX builds unchanged to the byte (`exe-imm.tsv` gains
the two rows and loses nothing; `exe.toml`'s two PSP sections move by one
range and three `placed_equal` bytes each); `placed_differ` unchanged.
`exe_tables.py check --game --disc` x7: 0 errors; `exe_imm.py selftest` 0
problems; `psp_exe.py check` OK; `importer.py check` 0 errors. 7.5's table
reads with these two lines for the PSPs. Played: the PSP-only install
rebuilt from this branch, Raise Dead and Resurrect over a fallen Ryu
([`spell-sweep.md`](spell-sweep.md) 9.3).

## For the other files

- **`install-gap-census.md` section 8** (the coordinator's): the area message
  words item is placed on the five PSX builds (Area08 on the PSPs too).
  `Area26_EffectKinds` and `Area94_EffectArgsE` are placed on the five PSX
  builds. The P3 tail's reach is section 0 item 1.
- **`exe-import-engine.md` 3.3**: built, here.
