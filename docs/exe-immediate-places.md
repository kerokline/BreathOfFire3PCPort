# The owning-function place kind: `imm` and `immref` rows

**Status:** IN PROGRESS (2026-10-18, install-gap round stage A, group OT, branch
`install-gap/ot` off `06a209bd`. Built, checked and measured on the seven held
disc builds; the regenerated recipes are not committed, the coordinator
regenerates them at the merge. Nothing played.)

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
   equal the PC's, four or more of them, two non-zero. An `imm` where the twin's
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

## For the other files

- **`install-gap-census.md` section 8** (the coordinator's): the area message
  words item is placed on the five PSX builds (Area08 on the PSPs too).
  `Area26_EffectKinds` and `Area94_EffectArgsE` are placed on the five PSX
  builds. The P3 tail's reach is section 0 item 1.
- **`exe-import-engine.md` 3.3**: built, here.
