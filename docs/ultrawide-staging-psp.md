# Ultrawide staging: did the PSP move anyone? (group PD)

**Status:** MEASURED (2026-10-13), group PD of the ultrawide round
([`ultrawide-round.md`](ultrawide-round.md) section 2 "PD"). It is a reading:
nothing in `src/` changed and no game was run. It is written as a section of
[`ultrawide-staging.md`](ultrawide-staging.md) (group SS's doc), and the
coordinator folds it in. Every number cites the scratch script that measured
it (section 6). Scripts and their outputs stay in the session scratchpad
(`uw_pd/`), because they print script bytes; this doc gives deltas, screen
pixels, counts and addresses only (CLAUDE.md rule 1).

## 0. The answer

**Yes. Capcom re-staged a handful of scenes for the PSP's 32 columns, and
left most of them alone.**

- **SS's 56 band rows:** the PSP changed the staging behind **9 rows, in 7
  scenes** (areas 0, 27, 32, 60, 94 and 175, the last one twice). **47 rows are
  byte for byte the PSX's**, on PSP-JP and on PSP-EU alike (section 2, the
  table). Of the 56, **30** would have shown at least partly in the PSP's
  wider view, using SS's camera and a 12-pixel half sprite. The PSP fixed 7
  of those 30 and left the other 23 as they were.
- **Case zero (area 175, chapter 7, `+0x18[4]` and `[5]`) is one of the
  scenes Capcom changed, and it changed it the other way round.** On the PSX the
  two party members walk 24 steps west and wait at screen x -40, at the left
  edge. On the PSP they walk 21 steps west and then 6 steps north, and wait
  **inside the 4:3 view, at about x 38** (SS's projection with SS's focus).
  Later the PSP adds 6 steps back south, so they rejoin the same final point.
  The bank, `Scena07_Scene2`, is unchanged: its leader and party points are
  held on both sides (section 3).
- **The scenes Capcom changed use two shapes:**
  - **Out:** the actor's start, or its point just before a hide, moves farther
    out, past the PSP's edge. This is area 94 (the start, from -34 to -63),
    area 19 (from -16 to -70), area 0 (from 360 to 373), area 32 (from -46 to
    -74), area 27 (a parked point, from -46 to -60) and area 60 (it now walks
    6 steps farther before its hide toggle). In each case the walk grows by the
    same amount, so the end point stays where it was.
  - **In:** the actor waits inside the view instead of at the edge. This is
    case zero.

  Both shapes are the ones [`ultrawide-round.md`](ultrawide-round.md) 4 calls
  "park farther" and "leave", applied scene by scene.
- **No hide was added.** No `E6` or `EB` op was added or removed in any
  script. Area 60 keeps its `EB` and walks farther first.
- **No camera set-up changed** in the banks. The coordinate constants of every
  paired field and scenario function agree once the two compilers' forms are
  normalised (section 4). One camera script did change: area 88's kind-2
  script, the camera object's walk, is 2.5 cells (about 51 px) shorter. A
  second, in area 43, is half a cell longer.
- **Beyond SS's rows** (section 5), the general diff finds 39 changed scripts
  (4 placement, 35 movement) across 22 areas. Of those, 14 sites in 11 areas
  move something by 30 px or more, in the same two shapes: entrances started farther out, and exits walked
  farther before an `E6` or `EB`. The PSP also textured the dock's edge cells
  (AREA128, [`opt-layers.md`](opt-layers.md) 4), the same area whose three
  party entrances start 4 cells (about 80 px) farther out.
- **What this says about SS's 16:9 list:** Capcom fixed scenes **one by one**,
  not by a rule. It did not even clear the sites its own 32 columns exposed (23
  of 30 were left). So:
  - a row that is untouched on the PSP is **not** shown to be fine at 53
    columns;
  - a row the PSP *did* change is Capcom's own confirmation that the original
    staging read badly in a wider view.

  Section 7 lists the latter for ST.

## 1. Method

The PSP's `BOOT.BIN` holds the area data natively. That covers the area
table, the descriptors, the placement scripts and the movement scripts, at
link-time offsets with pointers as offsets. JP's area table is at `0x2E32F0`
(the one 200-word run that `exe_maps/psp-jp.tsv` places at the PC's
`0x667590`). EU's is at `0x2E2B08`.

| Step | Script (scratch `uw_pd/scripts/`) | What it does |
|---|---|---|
| 1 | `table.py`, `scripts.py` | Pairs every PC descriptor with the PSP's by index. Walks both placement scripts (`event_scan.Walk`) and every movement script of `+0x10` / `+0x18` / `+0x1C` (`movement_scan`'s arrays). |
| 2 | `threeway.py`, `reach.py` | For each script, compares the bytes of the **reachable ops only** (`movement_scan`'s flow walk, run on each side) on three sides: PSX-JP (through `exe_maps/psx-jp.tsv` and the disc's EMI sections), PC, and PSP. The PC's 4-byte padding after a script is not counted. |
| 3 | `broad.py`, `filt.py`, `moved.py`, `mapq.py` | Diffs every byte of the area data blocks between the paired anchors (7,142 anchors, 462,136 bytes), with pointer words masked. Blocks found only on one side are searched for elsewhere in `BOOT.BIN`. |
| 4 | `emis.py` | Hashes every EMI section on the PSX disc against the PSP disc, JP and EU. |
| 5 | `imm.py`, `show.py`, `camera.py`, `bank.py`, `banklit.py` | Compares the coordinate constants of every paired field and scenario function. The pairing is `psp_pair.py`'s, rebuilt; the comparison is raw, not the diff's cancelling one. It also checks the camera block's users and SS's bank literal points. |
| 6 | `crossref.py`, `psp_scan.py`, `cmp_scan.py`, `timeline.py`, `proj.py`, `gen_table.py` | Checks SS's rows one by one. SS's own scanner (`tools/staging_scan.py` at `a69e064e`, a scratch copy) is run on a hybrid image, the PC's executable with the area table, descriptors and scripts served from `BOOT.BIN`. Its rows are then compared with SS's. The rerun on the PC alone reproduces SS's 332 rows exactly. |
| 7 | `eu.py`, `eu2.py` | Compares PSP-EU with PSP-JP, script by script. |

A cell is one whole coordinate unit (`0x10000`; the object vertex is the
position `>> 9`). A move op's step is half a cell on each axis that moves
(`MoveCmd_F9Steps`, read from the exe). With SS's camera (`project()`), at
the focus, one cell along x moves the sprite 20.5 px right and 10 px down. One
cell along z moves it 20.5 px left and 10 px down. So **30 screen px is about
1.5 cells**, and the "hit" threshold of the brief is 1.5 cells, or 3 steps,
along an axis (`proj.py`, `python -c ss.project`).

## 2. SS's 56 band rows, one by one

Key to the columns:
- **PSP view?** says whether SS's sx, ±12 px, overlaps the PSP's -32..352.
- **Top** is SS's top-ten rank.
- **PSP changed?** describes the row's own script or placement op on PSP-JP
  (`crossref.py`). The rest point and sx are from `psp_scan.py` / `proj.py`.
- **EU** is PSP-EU against PSP-JP (`crossref.py`, `eu.py`).
- **Moves by:** "past 32" means the new point is wholly outside the PSP's view
  (sx + 12 < -32, or sx - 12 > 352). "Past 53" means it is outside the 16:9
  view as well.

| # | Area | Ch | Script | SS class | Band | sx | PSP view? | Top | PSP changed? | EU |
|--:|--:|--:|---|---|---|--:|---|--:|---|---|
| 1 | 0 | 2 | `+0x18[0]` | parked | 16:9 | -15 | yes | 2 | no | same |
| 2 | 0 | 1 | `+0x10[4]` | spawn pop | 21:9 only | -53 | no | | no | same |
| 3 | 0 | 1 | `+0x10[5]` | spawn pop | 16:9 | 340 | yes | | no | same |
| 4 | 0 | 1 | `+0x10[6]` | spawn pop | 16:9 | 360 | yes | | **yes**: placed 1 cell farther (z -1); sx 360 -> **373**, past 32 by 9 px, just at the 16:9 edge. The script gains a re-placement at the original point, after its visibility toggle | same |
| 5 | 7 | 1 | `+0x10[1]` | parked | 16:9 | 347 | yes | 8 | no | same |
| 6 | 7 | 1 | `+0x10[1]` | parked | 21:9 only | 384 | no | 8 | no | same |
| 7 | 7 | 1 | `+0x18[0]` | parked | 21:9 only | 404 | no | | no | same |
| 8 | 26 | 2 | `+0x10[27]` | despawn pop | 16:9 | 343 | yes | | no | same |
| 9 | 26 | 2 | `+0x10[2]` | parked | 16:9 | -26 | yes | | no | same |
| 10 | 27 | 2 | `+0x10[2]` | parked | 16:9 | -46 | no | 3 | **yes**: the last walk before `E6` is 2 steps longer (1 cell west); sx -46 -> **-60**, past 32 by 16 px, past 53 | same |
| 11 | 32 | 8 | `+0x10[4]` | spawn pop | 16:9 | -46 | no | | **yes**: the `0D` placement 1 cell farther (z +1), the walk 1 step longer; sx -46 -> **-74**, past 32 by 30 px | same |
| 12 | 39 | 6 | `+0x10[14]` | parked | 21:9 only | -101 | no | | no | same |
| 13 | 39 | 6 | `+0x10[15]` | parked | 16:9 | -27 | yes | 10 | no | same |
| 14 | 39 | 9 | `+0x10[33]` | parked (waits, then enters) | 21:9 only | -65 | no | | no | same |
| 15 | 39 | 9 | `+0x10[34]` | parked (waits, then enters) | 16:9 | -46 | no | | no | same |
| 16 | 39 | - | `+0x18[14]` | spawn pop | 16:9 | -38 | yes | | no | same |
| 17 | 43 | 8 | `+0x18[3]` | parked (waits, then enters) | 16:9 | 334 | yes | | no | same |
| 18 | 47 | - | `+0x10[1]` | parked | 21:9 only | -113 | no | | script changed (its exit walk before `E6` is 1 cell shorter); **this rest unchanged** | same |
| 19 | 47 | - | `+0x10[5]` | parked | 21:9 only | -56 | no | | no | same |
| 20 | 50 | - | `+0x18[2]` | entrance seen early | 16:9 | 347 | yes | | no | same |
| 21 | 60 | - | `+0x18[2]` | despawn pop | 16:9 | 351 | yes | | **yes**: 6 steps (3 cells) more north before its `EB`; the toggle now happens at sx **389**, sy -18 (off the top, and past 53) | same |
| 22 | 60 | - | `+0x18[2]` | parked | 16:9 | 351 | yes | 5 | **yes**: the same change; the parked point is that new point | same |
| 23 | 60 | - | `+0x18[1]` | parked | 16:9 | 351 | yes | 9 | no | same |
| 24 | 66 | - | `+0x10[1]` | entrance seen early | 16:9 | 365 | no | | no | same |
| 25 | 69 | - | `+0x10[1]` | despawn pop | 16:9 | 332 | yes | | no | same |
| 26 | 69 | - | `+0x10[2]` | parked (waits, then enters) | 16:9 | -44 | no | | no | same |
| 27 | 80 | - | `+0x10[3]` | spawn pop | 21:9 only | 374 | no | | no | same |
| 28 | 86 | - | `+0x18[5]` | entrance seen early | 21:9 only | 394 | no | | no | same |
| 29 | 94 | - | `+0x10[39]` | entrance seen early | 16:9 | -34 | yes | | **yes**: placed 2 cells farther (x -2), the first walk 4 steps longer; the start sx -34 -> **-63**, past 32 by 19 px; the walk's end unchanged | same |
| 30 | 94 | - | `+0x10[37]` | parked | 21:9 only | 417 | no | | no | same |
| 31 | 94 | - | `+0x10[39]` | parked | 21:9 only | 404 | no | | the same script (row 29); **this rest unchanged** | same |
| 32 | 94 | - | `+0x10[9]` | parked | 16:9 | 333 | yes | | no | same |
| 33 | 94 | - | `+0x10[39]` | parked (waits, then enters) | 16:9 | -34 | yes | | **yes**: as row 29 (the wait is at the start) | same |
| 34 | 119 | - | `+0x18[18]` | parked (waits, then enters) | 21:9 only | 408 | no | | no | same |
| 35 | 119 | - | `+0x18[19]` | parked (waits, then enters) | 21:9 only | 393 | no | | no | same |
| 36 | 119 | - | `+0x18[20]` | parked (waits, then enters) | 21:9 only | 423 | no | | no | same |
| 37 | 120 | - | `+0x18[15]` | entrance seen early | 16:9 | 334 | yes | | no | same |
| 38 | 120 | - | `+0x18[4]` | parked | 21:9 only | 389 | no | | no | same |
| 39 | 120 | - | `+0x18[5]` | parked | 21:9 only | 404 | no | | no | same |
| 40 | 120 | - | `+0x18[15]` | parked (waits, then enters) | 16:9 | 334 | yes | | no | same |
| 41 | 143 | - | `+0x18[8]` | entrance seen early | 16:9 | 334 | yes | | no | same |
| 42 | 143 | - | `+0x18[6]` | parked | 21:9 only | 404 | no | | no | same |
| 43 | 143 | - | `+0x18[7]` | parked | 16:9 | 366 | no | 1 | no | same |
| 44 | 143 | - | `+0x18[6]` | parked | 16:9 | 334 | yes | | no | same |
| 45 | 143 | - | `+0x18[7]` | parked | 16:9 | 334 | yes | 1 | no | same |
| 46 | 143 | - | `+0x10[2]` | parked | 21:9 only | 402 | no | | no | same |
| 47 | 143 | - | `+0x18[8]` | parked (waits, then enters) | 16:9 | 334 | yes | | no | same |
| 48 | 172 | - | `+0x18[15]` | entrance seen early | 21:9 only | -98 | no | | no | same |
| 49 | 172 | 14 | `+0x10[6]` | parked | 16:9 | 357 | yes | 4 | no | same |
| 50 | 172 | 14 | `+0x10[7]` | parked | 21:9 only | 393 | no | | no | same |
| 51 | 175 | 7 | `+0x10[6]` | despawn pop | 16:9 | 353 | yes | | no | same |
| 52 | 175 | 7 | `+0x10[7]` | despawn pop | 16:9 | 358 | yes | | no | same |
| 53 | 175 | 7 | `+0x18[4]` | parked (**case zero**) | 16:9 | -40 | yes | 6 | **yes**: the wait point moves 1.5 cells east and 3 north. It is now 21 steps west and 6 north, where it was 24 west; sx -40 -> **38, inside the 4:3 view**. The later route gains 6 steps south and loses 3 east, so it ends where it did | same |
| 54 | 175 | 7 | `+0x18[5]` | parked, waits then enters (**case zero**) | 16:9 | -40 | yes | 7 | **yes**: the same change; sx -40 -> **38** | same |
| 55 | 175 | 7 | `+0x10[7]` | spawn pop | 16:9 | 337 | yes | | no | same |
| 56 | 188 | 12 | `+0x10[1]` | parked | 16:9 | -28 | yes | | no | same |

**The top ten** (SS's ranks 1..10):
- **Changed:** ranks 3 (area 27), 5 (area 60 `+0x18[2]`), and 6 and 7
  (case zero).
- **Unchanged:** ranks 1 (143), 2 (area 0's party), 4 (172), 8 (7), 9 (60
  `+0x18[1]`) and 10 (39).

Rank 1 sits at sx 366 and does not overlap the PSP's view with a 12-px half
sprite, so leaving it is consistent. Ranks 2, 4, 8, 9 and 10 do overlap it, and
were left.

**Hide and visibility ops:** in none of the 56 rows' scripts did the PSP add or
remove an `E6` or an `EB`. Rows 4 and 21 / 22 keep theirs, and the actor is
moved before it.

**Measured, or the scan's:**
- Rows 4, 10, 11, 29 and 33 (their PSP sx) are `psp_scan.py`'s rerun of SS's
  scanner on the PSP data.
- Rows 21, 22, 53 and 54 vanish from that rerun, because their new point is no
  longer in a band. Their sx is `proj.py`: SS's `project()`, with the focus
  recovered exactly from SS's own row (residual 0 px²).
- Case zero's focus is the leader point that `Scena07_Scene2` sets, which is
  held on both sides (section 3).

**What the hybrid rerun also changed:** besides the rows above, it differs
from SS's table in areas 9, 28, 43, 52, 53, 101 and 128 (`cmp_scan.py`).
- Areas 19 and 128 are real script changes (section 5), but in census rows,
  not band rows.
- The rest are artefacts:
  - areas 9 and 43: the sound op `B2`, which makes the decode of the rest of
    the script diverge;
  - area 101: its camera script;
  - areas 52 and 53: the PC's run-time script block, disabled for the PSP;
  - area 128: sub-pixel drifts.
- Each artefact traces to a script that step 2 found identical in its
  reachable ops, or to that decode. None is a band row.

## 3. The banks and the camera

- **Case zero's bank, `Scena07_Scene2` `0x54F420`:**
  - It has no recorded PSX twin (`analysis/pairs_propagated.json`).
  - Every literal point that SS's bank table takes from it (the leader's area
    change point and the party point) is held as a constant by the PSX's
    `SCENA07.EMI` code and by the PSP code over the span of that overlay's
    twins (`banklit.py`).
  - The same holds for every literal of the band rows' other banks:
    `Scena01_Scene0B` / `0D`, `Scena02_Scene02` / `09` / `11` / `StepHook`,
    `Scena07_EnterArea` / `Scene1`, and `Scena08_EnterArea` / `Scene1` /
    `Scene11`. That is 27 of 27 points, on both axes.
- **`Scena07_EnterArea`** is paired (PSX `0x801FA6B4`, PSP `0x28FCE8`). Its 4
  coordinate constants are equal (`bank.py`).
- **The camera block's users:**
  - 451 paired or unpaired field functions read or write the block around
    `Camera_Distance` (PSX `0x80149328`, read off the PSX twins of
    `Area19_CameraOut`, `Area99`'s and `Area121`'s camera handlers).
  - Of these, 220 differ in some constant or are unpaired (`camera.py`).
  - The differences read are compiler forms: byte masks against
    sign-extended negatives, `0x60` / `0x22C` against `0xA0` / `0xA1`,
    packet sizes, and the PSP's full-frame `-32` / `0x160` (the widescreen
    fills already in [`psp-widescreen-placements.md`](psp-widescreen-placements.md)).
  - None is a distance, angle or focus literal that moved. **Not every one was
    read to the instruction.**
- **The camera scripts (`+0x1C`, the kind-2 object):** two changed.
  - **Area 88 `+0x1C[2]`:** a long west walk is 5 steps (2.5 cells, about
    51 px of pan) shorter. This is the brief's "camera set-up changed".
  - **Area 43 `+0x1C[0]`:** a move-and-rise is one step (half a cell) longer.

## 4. The code: coordinate immediates of paired functions

`imm.py`: 4,584 paired PSX functions in the field and scenario files (2,582
WORLD, 1,128 SCENARIO, 663 GAME, 211 COMMU), with 1,684 unpaired.

- **Raw differences:** 2,032 pairs differ in some raw constant, and 252 hold
  a coordinate-like value (a multiple of half a cell, `0x8000`) on one side
  only.
- **Normalised:** 9 pairs keep a difference on both sides, and 28 on one
  side. The normalisation recognises:
  - the PSX's `lui`+`ori 0x8000` against the PSP's `lui (n+1)`+`addiu -0x8000`;
  - the PSX's range test `z - K <= M` against the PSP's two bounds;
  - constants folded by the PSP compiler (for example `0x3E0000 - 0xA0000`);
  - the value as a whole-cell number that a helper shifts.
- **Not found in the twin's neighbours:** looking for the value in the PSP's
  functions within ±`0x6000` of the twin, and in the whole PSX file, leaves 3.
  All 3 are a loop's strength-reduced induction value, `0x380000`, in AREA102
  and AREA119's effect code (`show.py`).
- **Read to the instruction:** the 9 two-sided pairs, in AREA030 (×2), 100,
  112, 119, 134 (×3), 170, 189 and SCENA03 / 06. Every one is a carry or range
  form of the same value (`show.py`, `show2.txt`, `show3.txt`).

**Result: no coordinate in any paired field or scenario function moved by a
cell or more.** Moves under one cell are not separated from the carry noise
here. They are below the brief's threshold anyway.

## 5. The general diff: every changed script

`threeway.py` (JP):
- **Placement scripts:** 196 same, 4 changed.
- **Movement scripts:** 3,650 same, 35 changed. The classifier's 35
  `psx=pc, PSP DIFFERS` include area 59's mispaired entry (below). One of its
  `all differ` rows, area 31 `+0x18[0]`, is a real PSP change: the PSX map
  only differs past that script's end.
  - The other 3 `all differ` rows are cases where PC and PSP agree and the
    PSX-JP map is off (the known AREA026 shift and tail bytes).
  - 8 are `psp missing`: entries past a null in the array, where the PSP's
    array has a different structure (area 59 `+0x1C[2..3]`). Unresolved.

PSP-EU equals PSP-JP in every placement script and in all but one movement
script: area 39 `+0x10[27]`, two ops of groups `5x` / `8x`, no move (`eu.py`).

The changes, screen px by SS's camera at the focus (section 1):

| Area | Script | Change | Size | Shape |
|--:|---|---|---|---|
| 0 | placement `0x5DA216`, `+0x10[2]`, `[6]` | object placed 1 cell farther; the entrance's `0D` start 2 cells farther, its walk 4 steps longer | 2 cells, ~41 px | entrance farther (row 4) |
| 9 | `+0x10[7]` | sound op `B2` gains an operand-like `0xFF` ahead of the original (twice) | - | sound, not staging (not decoded: the PSP's `B2` handler not read) |
| 12 | `+0x10[1]` | an `E9` (repeat on a test) operand | - | not read |
| 19 | `+0x18[3]` | `0D` start 2 cells farther, walk 4 steps longer | ~41 px, sx -16 -> -70 | entrance farther |
| 26 | `+0x10[28]`, `[29]`, `[30]`, `[37]` | two 7-step moves become one move of 18 (`[37]`: 21) | +2 cells (~41 px), `[37]` +3.5 (~72 px) | walks farther before its `EB` |
| 27 | `+0x10[2]` | one more 2-step west move before `E6` | 1 cell, sx -46 -> -60 | exit farther (row 10) |
| 28 | `+0x10[1]` | a re-route: `0D` moved (-3, -5) cells, moves added and removed | ~41 px | re-route |
| 31 | `+0x18[0]` | `F8 07` attach: the offset index changes, and a facing op is added | not measured (the descriptor `+0x0C` vectors not read) | attach offset |
| 32 | `+0x10[4]` | `0D` 1 cell farther, walk 1 step longer | sx -46 -> -74 | entrance farther (row 11) |
| 43 | `+0x18[4]` | `B2` sound op, as area 9 | - | sound |
| 43 | `+0x1C[0]` | camera move-and-rise one step longer | 0.5 cell | camera |
| 47 | `+0x10[1]` | one `FA` move (2 steps) removed before `E6` | 1 cell | exit shorter |
| 60 | `+0x18[2]` | 6 steps added before `EB` | 3 cells, sx 351 -> 389 | exit farther (rows 21, 22) |
| 71 | `+0x10[2]` | a `C6` operand | - | not read |
| 77 | `+0x18[9]`, `[10]`, `[11]` | `0D` x +1 cell | ~20 px | under threshold |
| 88 | `+0x1C[2]` | camera walk 5 steps shorter | 2.5 cells, ~51 px | **camera** |
| 94 | placement `0x6158D4`, `+0x10[39]` | placed 2 cells farther, walk 4 steps longer | sx -34 -> -63 | entrance farther (rows 29, 33) |
| 94 | `+0x10[4]` | one 4-step move becomes 7 + 2 | +2.5 cells, ~51 px | walk farther |
| 98 | placement `0x617EEA`, `+0x10[1]` | placed 1 cell back (z -1), walk 1 step longer | ~20 px | under threshold |
| 128 | `+0x18[0]`, `[1]`, `[2]` | `0D` start 4 cells farther, walk 9 steps longer (the end moves half a cell) | 4 cells, ~80 px | entrance farther (the dock; see below) |
| 141 | placement `0x62F117`, `+0x10[1]` | placed 2 cells on (z +2), walk 3 steps shorter, a facing op added | ~41 px start | re-staged |
| 150 | `+0x10[1]`, `[4]` | the walk before `E6` is 3 and 4 steps longer | 1.5 and 2 cells, ~30 and ~41 px | exit farther |
| 175 | `+0x10[5]` | an `FA` diagonal move 2 steps longer | ~41 px | walk farther |
| 175 | `+0x18[4]`, `[5]` | case zero (rows 53, 54) | 1.5 + 3 cells, sx -40 -> 38 | **wait brought into view** |
| 197 | `+0x18[9]` | a `D9` wait (on the field's state) added | - | timing |

**Hits at 30 px or more:**
- areas 0, 19, 26, 28, 60, 88, 94 (×2), 128, 141, 150 (×2) and 175 (×2), off
  SS's band rows as well as on them;
- areas 27 and 32, which sit just under 30 px of change but carry their actor
  past the PSP's edge.

**The area data beyond the scripts** (`broad.py`, `filt.py`, `moved.py`,
`mapq.py`):
- **394,326 of 462,136 bytes are equal.** The rest is padding (4,509 runs),
  pointer words (6,936), or blocks the PSP holds elsewhere.
  - The large one-sided blocks of areas 67, 77, 104, 108, 121, 135, 145 and
    173 are not in `BOOT.BIN`. The PSP map places them from the EMI's overlay
    section, which is identical on both discs (`emis.py`: no `#13` section of
    any WORLD or SCENARIO EMI differs).
- **Two content changes outside the scripts:**
  - a sprite entry's second byte, `8` to `9`, in the descriptor `+0x08` tables
    of areas 33, 104, 121 and 152. This is a resource, not a position
    (inference);
  - the PC's `Area*_Buttons` values, which the PSP holds as pointers (a
    representation).
- **The EMIs' data sections** that differ are palettes (`0x8002BE00`,
  `0x8002D800`), message text (`0x80010000`) and eleven map bands
  (`0x80104000`). [`opt-layers.md`](opt-layers.md) 4 has already read the map
  bands. **AREA128's band gives 152 untextured cells at the dock's edge a
  texture**, a map-edge "void" fix in the area whose party entrances were also
  moved 4 cells out. That is two widescreen edits in one area (inference that
  they belong together).

## 6. What was not verified

- Nothing was seen in play, on the PSP or on ours.
- **Screen positions:**
  - They are SS's projection at the default camera (`Camera_Distance` 0, the
    field angles). The focus is SS's: for case zero it is the bank's leader
    point, held on both sides.
  - An area with another distance or a moving camera scales the pixels.
  - "PSP view?" uses a 12-px half sprite and the PSP's -32..352. It ignores the
    PSP's 12-row crop, which would hide area 60's rows at sy 0 still more.
- **Movement scripts are compared over their reachable ops.** An op the PSP
  added past a path end would not show. The `B2` change in areas 9 and 43
  makes the PSP's decode diverge after that op. Byte inspection (`broad.py`)
  shows nothing else changed there.
- **Not read:**
  - the attach offset change (area 31);
  - the `E9`, `C6` and `D9` operands (areas 12, 71 and 197);
  - the PSP's `B2` handler;
  - area 59's array entries past a null.
- **The banks:**
  - Bank functions with no recorded PSX twin (all the band rows' scenes but
    `Scena07_EnterArea`) were checked by literal presence only. A value held
    somewhere in the overlay on both sides does not prove that the same call
    site uses it.
  - 478 SCENARIO and 1,124 WORLD PSX functions have no PSP twin, so their
    immediates were not compared.
  - The camera users were scanned for differing constants, not each read.
  - PSP-EU's banks were not compared: only its scripts were (`eu.py`).
- **The code immediates:** the comparison tolerates one cell of carry noise,
  so a real move under 1.5 cells in code is not excluded.

## 7. For ST and EB

- **Capcom's own fixes are a model, and they are data.** Each is a script
  edit: a start farther out with the walk lengthened by the same amount, an
  extra walk before a hide, or a wait moved inside the view.
  - In our code, the same shape is "park farther" (or, for case zero, "bring
    in"). It is expressed as a rule in the columns, never as a copy of the
    PSP's script bytes (the owner's stance of 2026-10-10).
  - The PSP's amounts were chosen for 32 columns. Areas 0, 27, 32, 60, 94 and
    19 land 9 to 54 px past the PSP's edge. Areas 0 (373) and 27 (-60) are
    still inside our 21:9 band, and area 0 sits on our 16:9 edge.
- **Case zero:** Capcom's answer was to stage the pair inside the view, not to
  hide them. That is an option the owner may prefer to "hide when parked" for
  this scene. It is the owner's call.
- **EB's census** should treat the 9 changed rows as confirmed sites. The 47
  unchanged rows stay on SS's evidence alone.

## For the other files

- **`docs/ultrawide-staging.md`:** fold this doc in as the PD section, after
  SS's section 5 (the coordinator's merge).
- **`docs/README.md`, one row:** `ultrawide-staging-psp.md` | Group PD of
  the ultrawide round (2026-10-13): PSX against PSP for the scenes' staging.
  The PSP changed 9 of SS's 56 band rows (7 scenes), moving starts and exits
  past its 32-column edge, and staged case zero's pair inside the view. No hide
  added, no bank camera literal changed, one camera script shortened (area
  88). 39 changed scripts in all, 14 hits of 30 px or more. Untouched rows are
  not cleared by it. MEASURED. (Fold into the staging doc's row if the
  coordinator merges the docs.)
- **`docs/STATUS.md` / `HANDOFF.md`:** the PSP re-staged case zero (area 175)
  by moving the party's wait point into view. This is an owner's call for ST:
  Capcom's staging, or the round's "hide when parked".
- **Ledger:** none. This is a reading, and no behaviour changed.
- **Defects:** none found.
