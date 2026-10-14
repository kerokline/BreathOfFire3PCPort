# The "after" census: the state hash and the band census with every fix in (ultrawide round, group EB-2, part 2)

**Status:** MEASURED (2026-10-13 night to 2026-10-14), live. Group EB-2 of the
ultrawide round, branch `wide/eb2b` off the round tip `4743b5ec`, which holds:
- SL's faster slide ([`ultrawide-slides.md`](ultrawide-slides.md));
- RN's random split ([`ultrawide-rand.md`](ultrawide-rand.md), D283);
- ST's staging with the hide keyed to field scenes ([`wide-staging.md`](wide-staging.md));
- main's DIV-0115..0121, the stallion fix among them.

Nothing in `src/` changed. Everything `analysis/...` is gitignored and in the
main checkout: logs and tables in `analysis/ultrawide/census3/`, shots and
sheets in `analysis/shots/uw_census3/`, hash runs as
`analysis/statehash/cn_<route>_eb3*`. Scripts are in the session scratchpad's
`uw_eb1/` (`c3_*.sh`, `census3_one.sh`, `hash3_one.sh`, `census_report3.py`,
`cmp13.py`, `czsheet.py`, `rand12.py`). Earlier parts:
[`ultrawide-census-1.md`](ultrawide-census-1.md) (before) and
[`ultrawide-census-2.md`](ultrawide-census-2.md) (the causes).

## 0. The answer

- **The hash: all eleven routes stay on the narrow route at 16:9 and 21:9.**
  The routes are the ten `cn_` routes and `depress`. On every route the `Rand`
  count matches narrow on every frame, and no game page differs. What does
  differ is the draw state, the static display pages, window x words and
  message pen x (WA's anchors), the exe host's tick-3 pages, and one VRAM
  upload a tick apart.
  - Two first runs needed a repeat, and both repeats are clean.
  - `shop` at 21:9 first loaded another save's state at tick 286. I take that
    to be a slot-0 clash while a second run of mine overlapped it.
  - `dragonTransform` at 21:9 first showed one VRAM tick, which did not repeat.
  - The three fixes do what census-2 asked of them. `dragonTransform`,
    `cutsceneAndNue` and `whelpBoss` no longer part, and `shop` and the world
    map no longer move field objects.
- **Case zero (DIV E) applies and reads as the owner expected, with two
  findings.** The log lines `DIV-E case zero ... the PSP's staging` appear
  for both scripts at both of their west walks (frames 5984..5994 and
  7674..7690) and at the east walk (8959..8975). The census has no parked row
  for the pair in either band; they cross the left 16:9 band for 2 frames
  only.
  1. **In part one the pair wait inside the 4:3 columns, but just above the
     top edge.** The bottom few rows of one sprite (its feet) show as a
     sliver on the top row at game x 25..48, at 16:9 and at 21:9, for the
     whole wait. Narrow, with the original's staging, shows nothing there. In
     part two their path carries them fully off view: gone from the top by
     frame 7802.
  2. **The staging changes the scene's timing, so the recorded route parts.**
     With the staging on, `dolphinBoss` at 16:9 and 21:9 parts from narrow:
     dialogue boxes differ from about recipe frame 6,184, and the `Rand`
     count from 10,921, where the fight goes differently. With
     `BOF3X_WIDE_STAGING=0` it stays on narrow at both widths, with `Rand`
     equal on all 14,923 frames. The hide is not the cause (hide off, staging
     on: the same parting). Case zero is the only staging applied before
     6,184, and the PSP's walk is 3 steps longer each way (21 + 6 against 24).
     That is a recorded-route concern (a player's own inputs follow the
     screen), but it is a fact of DIV E's timing (section 3.2).
- **The census at 16:9:** the re-staged rows ST's table names are not parked
  in the 16:9 band wherever these routes reach them.
  - Site 3 (area 27) applies in `bossAndFlash`, and the route keeps narrow's
    `Rand` count.
  - Sites 15, 16 and 17 (area 175) apply in `dolphinBoss`.
  - Case zero is as described above.
  - The 16:9 band's other parked and still rows are the same ones census 1
    had (areas 1, 2, 3, 4, 9, 16, 19, 23, 24, 39, 41, 45, 77), plus the areas
    the save headers newly reach: 57, 65, 67, 175 and 177.
- **The census at 21:9:**
  - **The hide works in field scenes:** 215 joined rows `hidden yes`, in areas
    1, 2, 7, 9, 19, 23, 39, 41, 50, 53, 67, 77 and 175.
  - **Nothing on the world map is hidden**, nor in the attract demo (areas 16
    and 45, mask `0C`; areas 2 and 4, chapter 16).
  - **But most band rows in a scene are still drawn, for two reasons:**
    - `Game_Mode` 4: 175 object rows and 26 party rows, most in
      `balioAndSunder_2`'s areas 39 and 41. The hide keys on mode 2, and the
      field runs as mode 4 after `Field_Request` 2 (`mode-tasks.md` 3,
      unread there).
    - The leader, by design: 67 rows.

    Mode 4 is a question for ST (section 4.3).
- **New areas, one of them narrow:** with the nine save headers the routes
  now reach area **53** (`vaccinationAndFireScene`), one of UC's seven
  areas narrower than 560, and 57, 60, 65, 67, 87, 99, 117, 177.

## 1. Runs

- **The build and the launcher copy.** Built `4743b5ec` with `cmake --build
  build` (exit 0, no error lines). The ini-less copy is `uw_eb1/L4/`. A second
  copy, `uw_eb1/L5/`, was used for one side chain only; see the caveat
  below. Its log reads `DIV-D/E the wide staging on: 24 of 24 rows armed at
  53 / 120 columns`, and at 21:9 `DIV-F the 21:9 hide on`.
- **Other agents ran games on this machine at the same time.** I used only my
  own directories and ended only my own pids; `input_run.py` ends each game
  by pid.
  - One census run gave up waiting for slot 0, which another agent's run held
    (`dolphinBoss` 16:9, 23:07). It was run again from `L5`.
  - After that, both run scripts wait up to 2,400 s for slot 0
    (`--slot0-wait`).
- **The hash.** Run as in census 2 section 1: `BOF3X_OPT=none
  BOF3X_LAYERING=0`, the skip list, no overlay, speed 1, the shop with
  `--slot0-hold`, and `BOF3X_WIDE_STAGING_LOG=1`. The staging and the hide
  are at their defaults.
  - Tags: `eb3w0/1/2`, with repeats `eb3w2b` (shop, dragonTransform) and
    `eb3w0b` (shop).
  - **The baseline is ours narrow from the same hour** (`eb3w0`). `depress`
    has no `cn_` pair, so narrow is its only reference.
- **The census.** Run as in census 1 section 1, with `--speed 8`, shots every
  120 frames (`tools/recipe_shots.py`), `BOF3X_BANDLOG`, and
  `BOF3X_WIDE_STAGING_LOG=1`. Covered:
  - all 32 recipes of census 1, plus `depress` and `dolphinBoss`, at 16:9 and
    21:9;
  - `dolphinBoss` and `bossAndFlash` narrow as well, for the `Rand`
    reference.
- **Save headers added in the scratch copies** (`uw_eb1/mkrecipes3.py`):
  `bridgeWalk`, `fairyVillage`, `identify`, `ninaWalkBehindBlock`,
  `tarmanLogic` and `vaccinationAndFireScene` take the save of their own
  name. `combatGroup` and `sortScreens` have none of their own, so I used
  `combat` (a guess: the young party those routes show). `stallion` now
  carries its own header.
- **Case zero.** `dolphinBoss` copied with a shot every 8 frames over recipe
  frames 5960..6600, 7650..8300 and 8940..9200 (`recipe_shots.py` three
  times, 196 shots), run at 0, 1 and 2 (`dolphinBoss_cz_w<N>`). The side
  chain (`L5`) ran `dolphinBoss` with `BOF3X_WIDE_STAGING=0` at 1 and 2
  (`_s0`) and with `BOF3X_WIDE_HIDE=0` at 2 (`_h0`).
- **Comparisons.**
  - Hash runs: `diffvs.py`, which lists each route's page differences and
    compares the `randlog` against narrow.
  - Census runs: `rand12.py` for the `randlog`; `cmp13.py` for census 1
    against census 3; `census_report3.py` for the tables and sheets.

## 2. The hash

Each width is diffed against narrow (`eb3w0`). "Picture" means:
- the two static pages from tick 0 (`0x66B000`, `0x7DE000`);
- the draw and map-view pages from the first field frame (`0x7E0000`,
  `0x903000`, `0x929000`, `0x905000`, `0x92A000`, `0x937000`, `0x938000`);
- `Desktop_Width`'s page (`0x939000`) and the window page;
- `WindowRecords`' x words.

| Route | 16:9 | 21:9 | `Rand` |
|---|---|---|---|
| combat | picture, 16 pages (5 VRAM pages at tick 3 only) | picture, 16 | equal on all frames, both widths |
| shop | picture, 16 (tick-3 VRAM) | **first run: parted at tick 286** (`Party_Zenny`, then the area map: another save's state). **Repeat (`eb3w2b`): picture, 16; on route** | equal (the repeat) |
| worldMapAndAreaTransition | picture, 16 | picture, 11 | equal |
| field_menu | picture, 14 | picture, 14 | equal |
| menu_screens | picture, 16 (and `Effect_Objects`' page 1235..1490: the message pen x) | the same | equal |
| dragonTransform | picture, 11 | picture, 37 (26 VRAM pages at tick 394 only); **repeat: 11** | equal |
| caughFish | picture, 11 | picture, 11 | equal |
| masterAndManillo | picture, 11 | picture, 11 | equal |
| cutsceneAndNue | picture, 13 (the message pen x, 936..5113) | the same | equal |
| whelpBoss | picture, 14 (`DrawTable`; the message pen x 1384..11771) | the same | equal |
| depress | picture, 9 | picture, 9 | equal |

- **The tick-3 pages.** A narrow repeat of `shop` (`eb3w0b`) against
  `eb3w0` differs on exactly the 6 VRAM pages at tick 3 and nothing else.
  These are the exe host's first-tick pages (HANDOFF's trap), not the
  width's.
- **No staging row applied** on any hash route. With
  `BOF3X_WIDE_STAGING_LOG=1` no `cn_*_eb3w?.log` has a `DIV-D site` or
  `DIV-E case zero` line.

## 3. Case zero

### 3.1 What the pictures show

The sheets are in `analysis/shots/uw_census3/sheets/`:
- **`casezero_part1.png`**: the moment, narrow, 16:9 and 21:9 side by side
  at recipe frames 6008, 6048, 6072, 6096, 6200 and 6400. The 4:3 edges are
  in yellow, and the 16:9 edges in blue on the 21:9 tiles.
- `casezero_part2.png`: frames 7698..8202.
- `casezero_east.png`: frames 8964..9100.

Every shot of the windows is in `analysis/shots/uw_census3/dolphinBoss_cz_w<N>/`.

- **Part one.**
  - Up to frame 6048 the three widths match: the pair walk up and to the
    left together.
  - At 6072 narrow has them leaving at the top-left corner, and the wide
    modes show them at the 4:3 view's top-left.
  - **From about 6096, at 16:9 and 21:9, a sliver of one sprite (its feet,
    a few rows) stays on the top row at game x 25..48, inside the 4:3
    columns, through the wait** (shots 6200 and 6600, zoomed). Narrow
    shows nothing there.
  - So the PSP's walk does bring the pair inside the 4:3 view, but to a
    point just above its top edge.
  - Whether the PSP's taller 272-row picture shows them, and whether this
    sliver is wanted, is the owner's eye.
- **Part two.** The pair walk to the top-left again (7738, 7762). At 7786
  they are at the corner, and **from 7802 they are wholly off view**, which
  is the owner's expectation.
- **The east walk** (8959 / 8975, the 6 steps south first): see
  `casezero_east.png`.
- **The census agrees.**
  - `dolphinBoss_w1` / `_w2` have no parked or still row for the pair in
    either band.
  - The pair's records (party slots 1 and 2 in the census) cross the left
    16:9 band for 2 frames at recipe 6069..6080 and 7759..7776, classed
    `passing`.
  - The DIV-E lines are in the run logs (`runs3/dolphinBoss_w2.gamelog`):
    `+0x18[5] +0x074` at 5984, `[4] +0x060` at 5994, `[5] +0x094` at 7674,
    `[4] +0x082` at 7690, `[4] +0x08B` at 8959, `[5] +0x09D` at 8975.

### 3.2 The staging moves the recorded route

| Run | `Rand` against narrow (`dolphinBoss_w0`) |
|---|---|
| 16:9, staging and hide on (`dolphinBoss_w1`, and `_cz_w1`) | parts at frame 10,921 |
| 21:9, staging and hide on (`_w2`, `_cz_w2`) | parts at 10,921 |
| 21:9, staging on, hide off (`_h0_w2`) | parts at 10,921 |
| 16:9, staging off (`_s0_w1`) | **equal on all 14,923 frames** |
| 21:9, staging off (`_s0_w2`) | **equal on all frames** |

- **On screen,** the window shots match narrow through frame 6152 and differ
  from about 6184: the dialogue box holds another line at the same frame.
  By the fight the actions differ (narrow transforms, the wide runs do not;
  census shots 12000..14880). The wide route's other sites (15, 16, 17) all
  apply later, at 8876 and 10607.
- **The cause.** Case zero walks each actor 21 + 6 steps where the original
  walks 24, and `[4]` steps the scene clock after its walk (`wide-staging.md`
  5). The scene therefore runs a few steps' frames longer, and the recorded
  button presses land on other lines.
- **What it means.** A player is unaffected. Their inputs follow the screen,
  and the PSP has the same timing. But a recipe recorded narrow does not
  replay past a re-staged scene in the wide modes. That matters for the
  references, and for ST's rule "make sure the actor still arrives before
  anything waits on it", which is about the scene and not the route.
  `bossAndFlash`'s site 3 (3 steps at 16:9, 8 at 21:9) does **not** move its
  route: `Rand` is equal to narrow at both widths.

## 4. The census

### 4.1 Totals against census 1

`cmp13.py`, joined rows:

| Class, band, hidden | 16:9: census 1 -> 3 | 21:9: census 1 -> 3 |
|---|---|---|
| parked, 16:9 | 159 -> 190 | 159 -> 190 |
| parked-scene, 16:9 | 136 -> 206 | 136 -> 206 |
| still-scene, 16:9 | 33 -> 60 | 33 -> 60 |
| passing, 16:9 | 527 -> 591 | 529 -> 591 |
| parked, 21:9 only | - | 204 -> 227 |
| parked-scene, 21:9 only | - | 195 -> 78 not hidden, **159 hidden** |
| still-scene, 21:9 only | - | 43 -> 45 not hidden, **20 hidden** |
| passing, 21:9 only | - | 617 -> 608 not hidden, **36 hidden** |

The rise at 16:9 comes from the newly reached areas (the save headers,
`dolphinBoss`, `depress`). In the areas census 1 had, the 16:9-band rows are
the same, area for area (`cmp13.txt`). At 21:9, census 3's 16:9-band rows
equal its 16:9 run's again, count for count.

The `Rand` count of every census recipe is the same at 16:9 and 21:9
(`rand12.py`, every recipe); `dolphinBoss` is the one that parts from
narrow (section 3.2).

### 4.2 The 16:9 band: the re-staged rows

The routes reach the following of ST's 24 rows (by the logs' `DIV-D site`
lines):

| Row | Area | Recipe | Applied | Census at the site |
|--:|--:|---|---|---|
| 3 | 27 `+0x10[2]` | bossAndFlash | frame 5281; 3 steps (16:9), 8 (21:9) | no band row for it at either width. Area 27's only band row is `obj0` on the right, 534..584, outside a scene (mask `00`), as in census 1. `Rand` equals narrow |
| 15, 16 | 175 `+0x10[6]`, `[7]` | dolphinBoss | 10607; 5 / 4 steps (16:9), 10 / 14 (21:9) | no band row |
| 17 | 175 `+0x10[7]` | dolphinBoss | 8876; 7 steps (16:9), 17 (21:9) | no band row |
| E (case zero) | 175 `+0x18[4]`, `[5]` | dolphinBoss | 5984..8975 | section 3 |

The other rows' areas are reached only in other chapters or scenes (39
chapter 3, 19, 50, 60), or not at all.
- **Area 19** (`textureglitch`): row 2's site did not apply. The three party
  members stand in the left 16:9 band at 621..679, as in census 1. That scene
  is not row 2's: row 2 is `+0x18[3]`'s entrance, which this route does not
  play.
- **Area 60** (`bridgeWalk`, newly reached): no site line; the route does not
  play rows 14/15's scene.

**Still in the 16:9 band in a scene, not among the rows** (shots on the
`parked-scene_w1_*` / `still-scene_w1_*` sheets):

| Area, ch | Recipe, recipe frames | What | Mask |
|---|---|---|---|
| 175, 7 | dolphinBoss 3571..3645, 3940..3999, **4332..4740** | party slots 1 and 2 standing in the right 16:9 band before case zero's walk; slot 2 for 391 frames | `12` |
| 177, 9 | fairyVillage 886..980, fairyVillageNews 589..631 | two objects in the left band | `01` |
| 67, 6 | stallion 444..600, 861..983, 1596..1676, 7074..7729 | up to 81 rows, both sides | `12` |
| 57, 6 | vaccinationAndFireScene 292..314, 952..1235 | both sides | `0C`, `0E` |
| 65, 9 | dragonMenu 408..433 | one object, left | `0C` |
| 1, 2, 9 | whelpBoss (the scenes census 1 had) | the right band | `12`, `1E` |
| 39, 41 | balioAndSunder_2 chapter 3 (as census 1) | both bands | `12` |

### 4.3 The 21:9 band: the hide

- **Hidden where it should be.** 215 joined rows are `hidden yes`, all in
  field scenes:
  - areas 1, 9 (whelpBoss);
  - 19 (textureglitch: party slots 1 and 2);
  - 39, 41 (balioAndSunder_2);
  - 67 (stallion);
  - 77 (eldritchGene);
  - 175 (dolphinBoss);
  - 50 (tarmanLogic);
  - 53 (vaccinationAndFireScene);
  - 7 (shop), 2 and 23.

  By record: 193 field objects and 22 party members. The leader is never
  hidden.
- **Not hidden on the world map and in the demo.**
  - World map: areas 16 and 45, mask `0C`, `other` records. The rows include
    `caughFish` area 16 712..738 and `masterAndManillo` area 16 3338..3344,
    which census 1 had in the 21:9-only columns.
  - Attract demo: areas 2 and 4, chapter 16, mask `02`, 35 rows.

  As ST2 intended.
- **Hidden on two non-world-map fields with mask `0C`.**
  - Area 7: `shop` 2154..2180, which is area 7's entry fade, as ST notes.
  - Area 50: `tarmanLogic` 310..325 and 442..457.

  These are scenes by the mask, so they are hidden by the rule.
- **Not hidden, though in a field scene:**

  | `Game_Mode` | Rows not hidden | Where |
  |--:|--:|---|
  | 4 | 175 object rows, 26 party rows (slots 0 and 1) | mostly balioAndSunder_2 areas 39 and 41 (chapter 3's long scenes), also 53, 67 |
  | 2 | 43 party rows (slot 0, the leader: by design) and 22 others (the world map's `other`, the demo) | |
  | 1, 5, 7, 8 | 26 | the area load, battle, menus |

  **Mode 4** is the field mode `Field_Request` 2 switches to (`mode-tasks.md`
  3: "what modes 4 ... are is not read here"). The field keeps running, and
  so do its scenes. ST's `FieldScene()` admits only mode 2, so the long
  chapter-3 scenes of areas 39 and 41 draw their 21:9-band actors. The
  shots on the `parked-scene_w2_*` sheets show them. **For ST:** read mode 4
  and decide whether it is a field mode for the hide.

### 4.4 Narrow areas reached

**Area 53** is reached by `vaccinationAndFireScene`. It is one of UC's seven
areas wider than 426 but narrower than 560. Its rows are in `scenes.tsv`.
Areas 31 and 90 are reached as in census 1.

### 4.5 Against SS's table

`census3/ss_compare.tsv` (census 1's method). Of SS's 209 band and
needs-census rows, the routes now reach the areas of 63:

| Verdict | Rows |
|---|--:|
| confirmed (x within 40) | 13 |
| confirmed (side) | 1 |
| claimed by a nearer SS row | 28 |
| refuted on these routes | 10 |
| other chapter | 6 |
| no band row | 5 |
| not reached | 146 |

## 5. What was not verified

- **Whether the case-zero sliver is visible on the PSP** (272 rows), or
  wanted.
- **Which of case zero's walks alone moves `dolphinBoss`'s route.** There is
  no per-row switch, but case zero is the only row applied before the screens
  part.
- **The mode-4 frames' nature.** Mode 4 is unread in our docs.
- **The shop's first 21:9 run.** I read its loaded state as another save's
  (`Party_Zenny` and the area map from tick 286, in the title-to-load
  frames). I did not prove which process wrote slot 0.
- **The staging rows the routes do not reach:** 20 of the 24, and the
  other-chapter rows.

## 6. For the other files

**`docs/README.md`**, a row:

| [`ultrawide-census-3.md`](ultrawide-census-3.md) | The ultrawide round's "after" census (group EB-2 part 2, live, 2026-10-14): with SL, RN and ST2 in, the eleven hash routes stay on the narrow route at 16:9 and 21:9 (`Rand` equal, game pages equal); case zero applies (PSP staging; a sliver of the pair's feet at the top edge in part one; the longer scene moves a recorded route); the census against census 1 - the re-staged sites reached carry no band row, the 21:9 hide hides 215 rows in field scenes and none on the world map, but not in `Game_Mode` 4. MEASURED. |

**`docs/STATUS.md`:** EB-2 - the wide modes no longer change the recorded
routes, except where a staging row lengthens a scene (`dolphinBoss`, case
zero). The hide misses `Game_Mode` 4.

**`docs/HANDOFF.md`:**
- ST: the hide and `Game_Mode` 4 (section 4.3).
- The owner's eye: case zero's top-edge sliver (`casezero_part1.png`); the
  remaining 16:9-band scene rows (section 4.2), with
  `parked-scene_w1_*.png` and `still-scene_w1_*.png` in
  `analysis/shots/uw_census3/sheets/`.
- References: a recipe recorded narrow does not replay past case zero in the
  wide modes. Record `dolphinBoss` again at a wide width if it is to be a
  wide reference.
- Area 53 is reached (`vaccinationAndFireScene`); RG's narrow-area work can
  use it.

**Ledger material:** DIV E's entry should say that the PSP's walk lengthens
the scene by its extra steps (section 3.2): the timing is the PSP's, and a
narrow recording does not replay through it.
