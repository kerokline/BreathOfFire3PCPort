# The "after" census, part 1: the state hash with the flag split and the staging (ultrawide round, group EB-2)

**Status:** MEASURED (2026-10-13), live. Group EB-2 of the ultrawide round
([`ultrawide-round.md`](ultrawide-round.md) section 3, "Stage C and later"),
branch `wide/eb2` off the round tip `cf557d34`. That tip holds UC2's flag
split ([`ultrawide-culls-fix.md`](ultrawide-culls-fix.md)) and ST's staging
and 21:9 hide ([`wide-staging.md`](wide-staging.md)). This is **the hash
half**. The census half follows once ST has re-keyed the hide. Nothing in
`src/` changed. One diagnostic build was made and is not committed (section
1). The "before" is [`ultrawide-census-1.md`](ultrawide-census-1.md)
(EB-1).

## 0. The answer

- **The flag split works, where the flag was the cause.** `combat`'s
  `Sprite_Objects[2]` byte 0 is now `0x81` at 21:9, as it is narrow (raw
  dumps, ticks 1200 and 2400; EB-1 had `0x01`). The object pages of
  `caughFish` and `masterAndManillo` (both widths) and of `shop` at 16:9 now
  match narrow on every tick.
- **Two other causes remain, both now named and measured.** Each one moves a
  route off narrow, and the split does not touch either.
  1. **The cell inset lowered by the columns feeds the random stream.**
     `MapCell_DrawRising` (`0x570660`, ours, `map_cells.cpp` `DrawRising`)
     draws eight squares per map cell and calls `Rand` once for each.
     DIV-0041 lowers `MapView_Build`'s inset by `ceil(columns / 20)` (3 at
     16:9, 6 at 21:9). The ring walk therefore reaches more cells, so a
     rising-square cell in the added columns makes 8 more `Rand` calls a
     frame. That shifts the CRT generator, and everything drawn from it after
     that point: field walkers, encounters, battle damage.
     - Measured three ways:
       - The randlog first parts on the frame a band cell comes in: on
         `whelpBoss` at frame 285, 24 calls narrow against 32 at 16:9.
       - A diagnostic build that logs each `Rand` caller puts all the extra
         calls on the world-map route at `DrawRising`.
       - **`BOF3X_WIDE_INSET=0`** (the ring walked as narrow) **brings
         `whelpBoss` (16:9 and 21:9), `shop` and `worldMapAndAreaTransition`
         (21:9) back to narrow**: the same `Rand` count on every frame, and no
         game page differs.
     - This is what moved the field objects on `shop` and the world map, and
       it is how `whelpBoss` parts.
  2. **The slide-off bounds moved by the columns delay when a window
     closes.** UW's rule moves fourteen slide-off bounds by `c`
     (`widescreen.cpp` `g_slides`). A window counts as active until it passes
     its bound, so at 16:9 it closes 2 frames later (53 / 0x20) and at 21:9
     4 frames later (120 / 0x20). The battle and menu code waits on the
     record before it acts on the next input, so the recorded inputs land at
     a different state.
     - Measured: `BOF3X_ORIGINAL=Widescreen` (hosted) reverts the 18 patched
       values. With it, **`dragonTransform` (16:9 and 21:9) and
       `cutsceneAndNue` (21:9) stay on the narrow route**: the same `Rand`
       count, and no game page differs. Pinning the inset does not keep them
       there.
     - The windows that differ in the dumps:
       - `dragonTransform`: `MenuSlide_RightOff` `0x59A5E0` (bound 320 at
         `0x59A5E6`, 373 / 440 wide).
       - `cutsceneAndNue`: `MenuSlide_LeftOff` `0x59A580` (-200 at
         `0x59A586`, -253 / -320) and `MenuSlide_LeftOff170` `0x596920`
         (-170 at `0x596926`, -223 / -290).
- **The staging (DIV D / E) applied no row on any of the ten routes**
  (`BOF3X_WIDE_STAGING_LOG=1`: no `site` or `case zero` line). With the
  staging at its defaults:
  - **16:9 is identical to staging-off on every tick** on all ten routes.
    `field_menu` differs only on the exe host's known tick-3 pages, and
    `whelpBoss` only by a first run whose own repeat matches (section 4.1).
  - **At 21:9 every difference is the hide's (DIV F)**: the draw-list and
    packet pages only, in the stretches the census classed as scene-masked:
    `shop` from 372, the world map 377..392, `caughFish` 769..785,
    `masterAndManillo` 652..3309, `cutsceneAndNue` 475..521, `whelpBoss`
    394..8444.
  - The hide acts on the world map (`caughFish`, `masterAndManillo`; mask
    `0C`), as EB-1 warned.

## 1. Runs

- **Build:** `cf557d34`, `cmake --build build` (exit 0, no error lines;
  `wide_staging.cpp` built). The copy `uw_eb1/L2/` is ini-less, with the
  launcher, `bof3x.exe` and the DLL. Its log reads `inject: 10087 ours, 0
  left original by BOF3X_ORIGINAL`, `DIV-D/E the wide staging off
  (BOF3X_WIDE_STAGING=0): 0 of 24 rows armed`, and with the defaults `24 of
  24 rows armed at 53 / 120 columns`.
- **Hash runs:** as in EB-1 (census-1 section 5.1). `BOF3X_OPT=none
  BOF3X_LAYERING=0`, `tools/statehash_skip.txt`, no language overlay, speed
  1, the shop with `--slot0-hold`. Files are in
  `analysis/statehash/cn_<route>_<tag>`; scripts are in the scratchpad's
  `uw_eb1/` (`eb2_part1.sh`, `eb2_isolate.sh`, `eb2_part3.sh`,
  `eb2_hosted.sh`, `eb2_part5..7.sh`, `hash2_one.sh`).

  | Tag | What |
  |---|---|
  | `eb2s0w0/1/2` | the ten routes, staging and hide off (`BOF3X_WIDE_STAGING=0 BOF3X_WIDE_HIDE=0`): the flag split alone |
  | `eb2w1/2` | the ten routes, staging and hide at their defaults, `BOF3X_WIDE_STAGING_LOG=1` |
  | `eb2iw1/2` | staging off, `BOF3X_WIDE_INSET=0` |
  | `eb2hw0/2`, `eb2how2`, `eb2hoiw2`, `eb2oiw1` (and `eb2ow2`, `eb2oiw2` of `cutsceneAndNue`) | hosted `BOF3.exe` (`--host bof3`; `BOF3X_ORIGINAL` needs it): narrow, 21:9, 21:9 with `BOF3X_ORIGINAL=Widescreen`, and that plus inset 0 |
  | `eb2dw0/2` (combat), `eb2dw0/1` (whelpBoss), `eb2dhoff`/`eb2dhon` (shop) | raw dumps |
  | `eb2w1b`, `eb2w2b`, `eb2s0w1b`, `eb2s0w2b` | whelpBoss repeats |

- **The baseline is ours narrow, the same hour** (`eb2s0w0`). It is again
  equal to the `orig2` pairs except their known staleness. The hosted
  narrow runs (`eb2hw0`) are **identical to the exe-hosted `eb2s0w0` on
  every tick** of `dragonTransform` and `cutsceneAndNue`, so the hosted
  isolation runs compare cleanly with it.
- **The randlog** (`randlog frame F rand K`, every scripted run's game log,
  `input_script.cpp`) is the instrument that found cause 1. It is compared
  by `uw_eb1/randcmp.py` / `randfile.py`, and the page lists by
  `diffvs.py` / `hash_check.sh` / `stagecmp.py`.
- **The diagnostic build** (`uw_eb1/L3/`, **not committed**; the source
  edit was reverted with `git checkout`): `BOF3X_RANDWHO=lo-hi` makes our
  `Rand` (`crt_rest.cpp`) log its return address in those recipe frames.
  At `worldMapAndAreaTransition` frame 378, narrow made calls 127..134 and
  21:9 made 135..150. All of them return to `bof3x.dll+0x40E47`, which
  `llvm-symbolizer` places at `DrawRising` (`map_cells.cpp:217`, the `g_rand()`
  of the half-side).
- **Repeats:**
  - **The first-run routes:** every difference in section 2 repeats EB-1's
    first ticks and page sets exactly. The code paths are the same, apart
    from the split, which removed the flag pages.
  - **The two causes:** each isolation result was seen in at least two runs.
    Inset 0 brings `whelpBoss` back at 16:9 and 21:9, and also `shop` and the
    world map. `BOF3X_ORIGINAL=Widescreen` brings `cutsceneAndNue` back with
    and without inset 0, and `dragonTransform` at 21:9 with and without
    inset 0 and at 16:9.
  - **The staging differences:** section 3.
  - **Run cost:** the session's run lines are in the worktree's
    `analysis/run_times.tsv`.

## 2. Step 1: the flag split alone (staging and hide off)

Each route's runs at `--wide 1` and `2` are diffed against `--wide 0`.
"Picture" means the pages EB-1 read byte by byte as draw state (census-1
section 5.2): draw records, packets, map view cells, inset, display, window x
words, message box pen positions, GTE scratch, and the two static pages.

| Route | 16:9 | 21:9 | Against EB-1 |
|---|---|---|---|
| combat | picture (11 pages); `Rand` equal | picture (12); `Rand` equal; **bit 7 of object 2 `0x81` as narrow** (dumps 1200, 2400) | the flag byte fixed |
| shop | picture (12); `Rand` equal; **no object page** | `Sprite_Objects` pages (`0x7DF000`, `0x802000`) still differ; `Rand` parts at frame 373 | 16:9 fixed; 21:9 is cause 1 |
| worldMapAndAreaTransition | object pages differ; `Rand` parts at 504 | object pages; `Rand` parts at 378 | cause 1 at both widths |
| field_menu | picture | picture | unchanged (was clean) |
| menu_screens | picture | picture | unchanged |
| caughFish | picture (11); **no object page** | the same | fixed (EB-1: `0x7DF000` from 769) |
| masterAndManillo | picture (11); **no object page** | the same | fixed (EB-1: `0x7DF000` 652..3306) |
| dragonTransform | **parts** (200 pages; battle records from 1679; `Rand` from 1682) | **parts** (199) | unchanged; cause 2 |
| cutsceneAndNue | converges (20 pages: `0x904000` and `Effect_Objects` from 4589, no battle records; `Rand` equal) | **parts** (313; `EnemyWorkingRecords` from 5120; `Rand` from 5154) | unchanged; cause 2 |
| whelpBoss | **parts** (392; `Rand` from frame 285) | **parts** (392) | unchanged; cause 1 |

## 3. Step 2: the causes

### 3.1 Cause 1: `MapCell_DrawRising`'s `Rand` and the inset

`DrawRising` (original `0x570660`): eight flat squares stacked on a map cell
and redrawn every frame. Each square's half-side takes `Rand() & 3`
(`map_cells.cpp:194..217`). Our `Rand` is the CRT's (`crt_rest.cpp`), never
reseeded, and the one generator the whole game draws from. The cells are the
ones `MapView_Build`'s walk reaches, which is the 28-column ring less the
inset (`map_layers.cpp` `Inset()`). DIV-0041 lowers the inset by
`Widescreen_InsetColumns()` (UW's `ceil(c / 20)`), so the wide view walks more
columns. Any rising-square cell in them adds 8 `Rand` calls a frame. Those
calls are not tied to whether the cell is on screen: `DrawRising` has no
cull.

| Route | Width | `Rand` first parts | With `BOF3X_WIDE_INSET=0` |
|---|---|---|---|
| whelpBoss | 16:9 | frame 285 (24 calls narrow, 32 wide) | `Rand` equal on all 13,121 frames; 13 pages, all picture (`DrawTable`, and `Effect_Objects`' page 1384..11771 - its bytes at tick 2000 are the message box pen and line x, WA's anchors) |
| whelpBoss | 21:9 | frame 285 (24 / 32) | `Rand` equal; the same 13 pages |
| shop | 21:9 | frame 373 (12 / 20) | `Rand` equal; 10 pages, picture; no object page |
| worldMapAndAreaTransition | 16:9 / 21:9 | 504 / 378 (16 / 24) | (21:9) `Rand` equal; 10 pages, picture |

`BOF3X_WIDE_TERRAIN=0` (the terrain cull at the original's) at 21:9 leaves
the world map's `Rand` parting at 378, so the cause is the inset and not the
terrain margin.

**Why it matters:** `BreathMote_Fly` was left at Capcom's bound by UC2 for
this very reason (it calls `Rand`, [`ultrawide-culls-fix.md`](ultrawide-culls-fix.md)
section 3). `DrawRising` is the same hazard on the map side, and it reaches
the encounter (`whelpBoss`, a fight at one width only) and field walkers
(`shop`, the world map).

**Fix shapes, for the coordinator and the owner** (no code here):
- (a) keep the `Rand` calls to the cells the original walks. In the wide
  walk, a cell outside the original's inset draws its squares with a
  draw-only generator (or the values its narrow twin would use). This is
  draw-only, and logic stays Capcom's.
- (b) pin the inset at narrow and let RG's ring cover the bands. That costs
  the bands' terrain at the original's inset.

  (a) is the split pattern UC2 used for the flag. Either way the check is
  the randlog, which must be equal to narrow on every frame of every route.

### 3.2 Cause 2: the slide-off bounds

The window records (`WindowRecords` `0x803160`, 0x24 each: `+0` active,
`+2` kind, `+3` state, `+4` x, `+6` y) were read from the raw dumps of EB-1
(`cn_<route>_eb1dw<N>.sh.<tick>.bin`; the slide code is the same at
`cf557d34`).

- **`dragonTransform`, tick 973:** record 16 (the battle's right-sliding
  window) has state 0 and x 320 narrow. At 16:9 and 21:9 it has state 1 and
  x 339, still sliding. At 980 it rests at 373 (16:9) and 440 (21:9).
  Records 17 and 21 then run a step (16:9) or two (21:9) out of phase with
  narrow (y 41 / 9 / -23, x 194 / 258 / 322 at tick 980), and are equal
  again by 1100. The battle records part from 1679. `MenuSlide_RightOff`
  `0x59A5E0` frees its window at `x >= 320` (`0x59A5E6`, UW's `320 + c`).
- **`cutsceneAndNue`, tick 4589:** records 16 and 18 are inactive narrow,
  parked at -200 and -170 (`MenuSlide_LeftOff` `0x59A580`'s and
  `MenuSlide_LeftOff170` `0x596920`'s bounds). At 16:9 and 21:9 they are
  still active (-238 / -223 at 16:9; -238 / -290 at 21:9), and all close by
  4600. At 5120, at 21:9 only, records 2 and 3 are in state 1 where narrow
  and 16:9 have 2. `Rand` parts at 5154 and `EnemyWorkingRecords` from
  5120. At 16:9 the same late close happens and the route converges. The
  inputs happen to fall in states that agree.
- **The isolation (hosted):**

  | Route | Run | Against hosted narrow |
  |---|---|---|
  | dragonTransform | 21:9 | parts (199 pages, `Rand` from 1682) |
  | dragonTransform | 21:9, `BOF3X_ORIGINAL=Widescreen` | **on route**: 11 pages, picture; `Rand` equal |
  | dragonTransform | 21:9, original + inset 0 | on route: 9 pages; `Rand` equal |
  | dragonTransform | 16:9, original + inset 0 | on route: 35 pages, picture, plus 27 VRAM-shadow pages at ticks 1294..1296 (one upload three frames apart); `Rand` equal |
  | cutsceneAndNue | 21:9 | parts (306 pages, `Rand` from 5154) |
  | cutsceneAndNue | 21:9, `BOF3X_ORIGINAL=Widescreen` | **on route**: 13 pages, picture (`Effect_Objects`' page is the message pen x, 936..4322); `Rand` equal |
  | cutsceneAndNue | 21:9, original + inset 0 | on route: 11 pages; `Rand` equal |
  | both | 21:9, inset 0 only | still parts, as without |

  `BOF3X_ORIGINAL=Widescreen` reverts all 18 patched values: the 14 slide
  bounds and `AreaMapBD_BuildView`'s 4 range operands (`0x51097E..0x5109D2`,
  the world map's frame pass). These routes part in battle menus, where the
  area-map ranges are not used, and the window records name the three slide
  functions. Which of the 14 alone is enough was not tested one by one: the
  switch has no finer grain.
- **Why it is logic:** the original frees a window the frame it passes the
  4:3 edge, so the next window or input waits on that frame. UW's rule moved
  the bound so the box would clear the wide picture before it vanishes. That
  is a picture goal reached through the record's lifetime.
- **Fix shape:** free the record at Capcom's bound, as the flag split did,
  and let a draw-only copy finish the slide into the band. Or keep the
  original bound and accept the box vanishing inside the band (the slide's
  last frames pop). The owner's call. Note that the slide-**ins** at 21:9
  already start inside the band (census-1 section 6.3).
- **Not from a missed bit-7 reader:** with both causes removed, no
  object, party or battle page differs on any route tested (sections 3.1
  and 3.2). UC2's scans found no tenth reader either.

## 4. Step 3: staging and hide at their defaults

`uw_eb1/stagecmp.py`: `eb2w<N>` against `eb2s0w<N>` (the same width, staging
off), and the run's `DIV-D/E/F` log lines.

| Route | 16:9 (staging on vs off) | 21:9 (staging and hide on vs off) |
|---|---|---|
| combat | identical, all 2,561 ticks | identical |
| shop | identical | 4 pages from 372 (draw: `0x6BE000`, `0x7E0000`, `0x903000`, `0x929000`) to 2181 |
| worldMapAndAreaTransition | identical | the same 4 pages, 377..392 |
| field_menu | tick 3 only (the exe host's first-tick pages; HANDOFF's trap) | identical |
| menu_screens | identical | identical |
| dragonTransform | identical | identical |
| caughFish | identical | the 4 pages, 769..785 |
| masterAndManillo | identical | the 4 pages, 652..3309 |
| cutsceneAndNue | identical | 2 pages (`0x6BE000`, `0x7E0000`), 475..521 |
| whelpBoss | first pair: `0x7DE000` from 4586, VRAM shadow 8114..9782; **the repeats are identical** (section 4.1) | the 4 pages 394..8444; the first pair also `0x7DE000` 7996..8692, **not in the repeat** (section 4.1) |

- **No staging row applied** on any route at either width. With
  `BOF3X_WIDE_STAGING_LOG=1` no log has a `DIV-D site` or `DIV-E case zero`
  line; only the arm lines `24 of 24 rows armed`. None of these routes
  passes one of ST's 24 sites. The staging's effect on a route that does is
  for the census half (EB-2 part 2) and a route into its scenes.
- **The hide's pages are draw state.** The four pages hold the draw list,
  packets and ordering table (`Sprite_DrawRecords`, `Gfx_PacketNext`,
  `Gfx_OtPointers`, `Sprite_DrawList`), and their stretches are the census's
  scene-masked rows at 21:9: shop 372..415 and 2154..2180, the world map
  (`0C`) on `caughFish` and `masterAndManillo`, area 30's `14` on
  `masterAndManillo`. The dumps (section 4.1) show no game byte moving.

### 4.1 Repeats and dumps

- **whelpBoss at 16:9.** Four runs: staging off (`eb2s0w1`), its repeat
  (`eb2s0w1b`), staging on (`eb2w1`) and its repeat (`eb2w1b`).
  - The two staging-on runs are **identical on all 13,185 ticks**.
  - The staging-off repeat equals them too: the same 11 pages against
    `eb2s0w1`, `Rand` equal.
  - So the odd run out is the first staging-off run, not the staging: the
    sound page from 4586 and a VRAM upload at 8114..9782 differ run to run
    on this parted route.
- **whelpBoss at 21:9.** Staging off (`eb2s0w2b`) and on (`eb2w2b`) again,
  with raw dumps at 8100 and 8400.
  - The off repeat differs from the first off run only on `0x7DE000` from
    7996, the same run-to-run sound-page difference.
  - On against off, repeated: the 4 draw pages (394..8444), nothing else, and
    `Rand` equal.
  - The dumps at 8100 and 8400 differ only in the renderer's state and
    `SpriteCell_Table` (the cells the draw built).
  - So the hide is draw-only here, and the 436-tick `0x7DE000` difference of
    section 4 was the first run's noise.
- **shop at 21:9, hide on against off** (`eb2dhon` / `eb2dhoff`, dumps at
  ticks 390 and 2170, both inside the census's 21:9-only scene-masked rows):
  - 390: renderer state only (texture caches, packet pools).
  - 2170: that, `SpriteCell_Table` and `SpriteCell_Count`, the cells the
    draw built.
  - No object, script, window or party byte differs. **The 21:9 hide is
    draw-only at both ticks**, as ST built it.
- **The parted routes vary run to run at the sound page.** `whelpBoss` at
  16:9 and 21:9 is a different fight from narrow's. Two runs of the same
  configuration differ on `0x7DE000` (and once on a VRAM upload), with the
  `Rand` count equal. The narrow route's repeats are identical (census-1
  section 5.1), so this belongs to the fight the route parts into. It is a
  reason more to remove cause 1 before the next comparison.

## 5. What was not verified

- Which single slide bound decides each route: `BOF3X_ORIGINAL=Widescreen`
  reverts all 18 patched values at once. The window records name
  `MenuSlide_RightOff` (dragonTransform) and `MenuSlide_LeftOff` /
  `MenuSlide_LeftOff170` (cutsceneAndNue).
- That `DrawRising` is the only map-side `Rand` consumer the inset adds:
  shown for the world map's first differing frame (every extra call is its),
  and implied for the rest by inset 0 making the randlog equal on every
  frame of four routes.
- The staging rows themselves: none applied on these routes.
- Whether `MapView_Inset` / `Scratch_Swap` (game variables the inset
  writes) are read by logic: with the inset at 0 no game page differs, so
  not on these routes.

## 6. For the other files

**`docs/README.md`**, a row:

| [`ultrawide-census-2.md`](ultrawide-census-2.md) | The ultrawide round's "after" state hash (group EB-2, part 1, live, 2026-10-13): the flag split confirmed (combat's bit 7 as narrow; caughFish / masterAndManillo / shop at 16:9 back on narrow), and the two causes left named and isolated - `MapCell_DrawRising`'s `Rand` over the columns the lowered inset adds (`BOF3X_WIDE_INSET=0` restores whelpBoss, shop and the world map) and the slide-off bounds moved by the columns (`BOF3X_ORIGINAL=Widescreen` restores dragonTransform and cutsceneAndNue); the staging applied no row on the ten routes and the 21:9 hide changes draw pages only. MEASURED. |

**`docs/STATUS.md`**, the round's line: EB-2 part 1 - the flag split
verified; the wide modes still change the random stream (the cell inset,
`MapCell_DrawRising`) and window timing (the slide-off bounds); three routes
part until those are split as the flag was.

**`docs/HANDOFF.md`:**
- The inset and `DrawRising`: keep `Rand` to the original's cells (fix
  shape (a), section 3.1). The randlog against narrow on every route is the
  check. The owner or the coordinator chooses, and RG should know.
- The slide-off bounds: free the record at Capcom's bound and draw the rest
  of the slide (section 3.2). It belongs with UW's rules and is the owner's
  call.
- The hide's keying (ST): it acts on the world map; see the census half.

**Ledger material:** DIV-0041's 2026-09-30 and 2026-10-13 amendments (the
inset rule, the slide-bound rule) change logic, not only the picture: the
`Rand` stream through `MapCell_DrawRising`, and window lifetimes. The
measurements are sections 3.1 and 3.2. No DIV is drafted here, since nothing
was changed.
