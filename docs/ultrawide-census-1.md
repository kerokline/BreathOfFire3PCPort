# The "before" census: what 16:9 and 21:9 show, and what they change (ultrawide round, group EB-1)

**Status:** MEASURED (2026-10-13), live. Group EB-1 of the ultrawide round
([`ultrawide-round.md`](ultrawide-round.md) sections 3 and 4a), branch
`wide/eb1` off the stage-B tip `caf7b768`. The round's one live group: the
owner left the machine to it. Nothing in `src/` changed; this is the census
**before** UC2 (the flag split, the six culls) and ST (the staging fixes).
EB-2 re-runs it after them.

Everything named `analysis/...` below is gitignored and lives in the main
checkout: the logs and tables in `analysis/ultrawide/census1/`, the sheets
and shots in `analysis/shots/uw_census1/`, the hash runs in
`analysis/statehash/cn_<route>_eb1*`. The scripts are in the session
scratchpad's `uw_eb1/` (named below). No capture, dump or game text is in the
tree.

## 0. The answer

- **The census ran on 32 recipes at both widths** (64 runs, all at `--speed
  8`), the attract cycle among them: **165 scenes** with something drawn in a
  band (87 at 16:9, 78 at 21:9), from 907 joined rows at 16:9 and 2,044 at
  21:9.
  At 21:9 the rows of the 16:9 band are the 16:9 run's rows again, count for
  count (159 parked, 136 parked-scene, 52 still, 33 still-scene; 527 against
  529 passing), and 1,135 rows more lie in the 21:9-only columns (section 2).
- **Against SS's table:** of SS's 209 band and "needs census" rows, the
  routes reach the areas of 29. **9 are confirmed** (an object on the
  predicted side, 8 of them within 40 px of SS's x), **4 refuted** on these
  routes, 11 consistent but claimed by a nearer SS row, 5 in an area the
  route loads but another chapter or no band row; 180 are in areas no route
  loads. **89 census scenes have no SS row** (section 4). **Case zero is not
  reached**: no route loads area 175 (section 4.3).
- **The wide picture changes game state, and on three of ten routes it
  changes the route.** Ours at `--wide 0` is identical to the `orig2` pairs
  except their known staleness (one page, `0x905000`, from the first
  save-list read). At `--wide 1` and `--wide 2`, every route differs from it
  (each difference repeated once, the repeat identical to the first run but
  for the known sound-page tick). Read byte by byte (raw dumps):
  - **the off-screen flag**, as UC predicted: `Sprite_Objects` object 2's
    byte 0 is `0x81` narrow and `0x01` at 21:9 on `combat` (ticks 1200 and
    2400), and nothing else of the game's state differs there;
  - **field objects stand elsewhere**: on `shop` and
    `worldMapAndAreaTransition` the objects' map x / z / height (`+0x34`,
    `+0x38`, `+0x3E`) and animation bytes differ from narrow at 21:9;
    `caughFish` and `masterAndManillo` differ on the same page
    (`Sprite_Objects+0x180`) at both widths. No route outcome changes there;
  - **three routes go another way**: `dragonTransform` (both widths; the
    battle's party and event records part between ticks 1100 and 1679),
    `cutsceneAndNue` (21:9 only; a battle window's state differs from tick
    ~4589, and the Nue fight's shot at recipe frame 6001 shows a different
    hit) and `whelpBoss` (both widths; between ticks 950 and 980 the leader's
    state is 3 narrow and 6 wide, a message and a battle start at one width
    only - 392 pages apart by the end). **At 16:9, this is what DIV-0041 has
    shipped since 2026-09-30**, unledgered (UC's finding, now measured).
    Section 5.
- **The looks** (section 6): the left message box at 21:9 draws whole at the
  picture's left edge (game x -104, the same pixels as 16:9's -37 shifted by
  the columns); Manillo's backdrop at 21:9 is seamless across all 560
  columns and its middle 640 x 480 is pixel-identical to 16:9's; the menu
  slide-outs keep their timing at 21:9, but a sliding window is **seen whole
  in the 21:9 band** at its first and last frames (the slide-ins start where
  the original started, inside the band); no ring notch was found at the
  21:9 edges (the black bands are interiors, the title and the battle
  floor's fall-off); two of the narrow areas are entered: **31** (a lift
  shaft, `attract_cycle` and `whelpBoss`) and **90** (a small room,
  `balioAndSunder_2`, `campingFishing`, `masterAndManillo`), both with black
  bands at every wide width.
- **Found on the way:** `stallion.txt` crashes at recipe frame 10,681 at
  **every** width, narrow included (a null zone list in `Area_ZoneAtIn`),
  so not this round's; and the census's scene flag is set on the world map
  (mask `0C`) and the fishing spot (`10`), not only in scenes.

## 1. What ran

**The build:** `caf7b768` in the worktree, `cmake --preset i686 && cmake
--build build` (exit 0, no error lines in the log; 873 steps from a fresh
configure). `inject: 10088 ours, 0 left original by BOF3X_ORIGINAL`. The
launcher, `bof3x.dll` and `bof3x.exe` copied to an ini-less folder
(`uw_eb1/L/`) and used for every run; the main checkout's `bof3/` as the
game; one game at a time, each ended by `input_run.py` by its own pid and
waited for.

**The census runs** (`uw_eb1/census_all.sh`, `census_one.sh`):
`BOF3X_OPT=none python tools/input_run.py <recipe> --no-front --speed 8
--wide N [--lang en-US] --env BOF3X_BANDLOG=<log>` with the shots in
`analysis/shots/uw_census1/<recipe>_w<N>/`. `BOF3X_BANDLOG` reaches the game
through `--env`: the first run's log says `bandlog BOF3X_BANDLOG on: ..., 53
columns a side, parked after 30 still frames, rows cut at 60 frames`, and at
`--wide 2` `120 columns a side`.

- **Recorded recipes** (28), under `--lang en-US` as their headers ask, each
  copied with a shot every 120 frames by `tools/recipe_shots.py --every 120`
  (never by hand): `balioAndSunder_1`, `balioAndSunder_2`, `bossAndFlash`,
  `bridgeWalk`, `campingFishing`, `caughFish`, `combat`, `combatGroup`,
  `cutsceneAndNue`, `dragonGene`, `dragonMenu`, `dragonTransform`,
  `eldritchGene`, `fairyVillage`, `fairyVillageNews`, `gameover`,
  `identify`, `masterAndManillo`, `musicStopCheck`, `ninaWalkBehindBlock`,
  `shop`, `sortScreens`, `stallion`, `tarmanLogic`, `textureglitch`,
  `vaccinationAndFireScene`, `whelpBoss`, `worldMapAndAreaTransition`.
- **Written recipes** (4), with their own shots and the original language:
  `attract_cycle` (the attract cycle), `camera_rotate`, `field_view`,
  `battle_commands`.
- **Skipped:** the title, config and menu recipes `title_timeline`,
  `config_screen`, `config_controller`, `load_list`, `backdrop_kinds`,
  `menu_screens`, `menu_titles`, `field_menu`; the `_ab` copies
  (`combat_ab`, `shop_ab`, `whelpBoss_ab`, `dragonTransform_ab`,
  `eldritchGene_ab`, `textureglitch_ab`, `worldMapAndAreaTransition_ab`),
  the same inputs as their bases; `combat_exp` (the combat route plus result
  shots); `worldmap_sliver` (a prefix of the world-map route); `field_f9`
  (needs an outside driver for F9); `f9ExitMode` (the title and F9's quit).
- **Caveat: nine recipes have no `# save` header** (`bridgeWalk`,
  `combatGroup`, `fairyVillage`, `identify`, `ninaWalkBehindBlock`,
  `sortScreens`, `stallion`, `tarmanLogic`, `vaccinationAndFireScene`) and
  so load the owner's slot 0 (HANDOFF's trap). Every one of them loaded only
  area 29 (`census1/areas.tsv`, from the game log's `DAT\AREAnnn.DAT` opens),
  so they played off their recorded route; their rows are area 29's.
- **Cost:** 64 census runs in 65 minutes (12:48 to 13:53), the whole session
  111 runs, 186.5 minutes of wall time (`tools/run_times.py --since
  "2026-10-13 12:40"`; the lines are in the worktree's `analysis/run_times.tsv`).

**`--speed 8` and one route:** `whelpBoss`'s 16:9-band rows start 52 recipe
frames earlier at 21:9 than at 16:9 from about frame 1,200, 536 by area 9
(`uw_eb1/w1w2.py`); every other recipe's rows start on the same frames at
both widths. The state hash (speed 1) says why: the route itself parts at the
wide widths (section 5.3), so this is not the speed.

## 2. The census

`python tools/band_census.py LOG --shots DIR --rows` per run, written to
`analysis/ultrawide/census1/<recipe>_w<N>.txt`; all scenes in one table,
`census1/scenes.tsv` (`uw_eb1/census_report.py`). A **scene** is
`band_census.py`'s: rows of one area and chapter within 60 frames of each
other, classed by the worst row.

| Class | 16:9 scenes | 21:9 scenes (16:9 band touched / 21:9 only) |
|---|--:|--:|
| parked-scene | 25 | 25 (23 / 2) |
| still-scene | 12 | 14 (12 / 2) |
| parked | 25 | 18 (14 / 4) |
| still | 5 | 6 (5 / 1) |
| passing | 20 | 15 (14 / 1) |

At 21:9 the scenes merge (a band 120 wide keeps more rows close in time), so
the 21:9 scene count is lower while its row count is more than twice 16:9's.

| Rows (joined) | 16:9 | 21:9: 16:9 band | 21:9: 21:9 only |
|---|--:|--:|--:|
| parked-scene | 136 | 136 | 195 |
| still-scene | 33 | 33 | 43 |
| parked | 159 | 159 | 204 |
| still | 52 | 52 | 76 |
| passing | 527 | 529 | 617 |

### 2.1 Parked in a scene (the owner's class)

Mask bits: 2 `Field_ScriptFlags` 0x100, 4 `Field_ScriptFlags2` 0x40, 8
`Field_Request` 5, 0x10 the leader's state not 1 ([`band-census.md`](band-census.md) 3).

| Recipe | W | Area | Ch | Recipe frames | Side | Band | x | Objects | Mask | Shot |
|---|--:|--:|--:|---|---|---|---|--:|--:|---|
| attract_cycle | 1 | 4 | 16 | 1577..1667 | L | 16:9 | -106..-0 | 3 | 02 | a00 |
| attract_cycle | 1 | 4 | 16 | 1782..1849 | R | 16:9 | 321..404 | 2 | 02 | a01 |
| attract_cycle | 1 | 2 | 16 | 4532..4737 | LR | 16:9 | -72..374 | 2 | 02 | a13 |
| attract_cycle | 1 | 2 | 16 | 4913..5117 | L | 16:9 | -78..-0 | 2 | 02 | a15 |
| attract_cycle | 2 | 4 | 16 | 1554..1667 | L | 16:9 | -164..-0 | 3 | 02 | a00 |
| attract_cycle | 2 | 4 | 16 | 1782..1924 | R | 16:9 | 321..456 | 2 | 02 | a01 |
| attract_cycle | 2 | 2 | 16 | 4532..5170 | LR | 16:9 | -146..374 | 2 | 02 | a15 |
| balioAndSunder_2 | 1 | 39 | 3 | 7286..9682 | LR | 16:9 | -108..418 | 16 | 1E | f09240 |
| balioAndSunder_2 | 1 | 41 | 3 | 9686..10045 | LR | 16:9 | -94..382 | 4 | 1E | f09720 |
| balioAndSunder_2 | 1 | 41 | 3 | 10521..10623 | LR | 16:9 | -124..412 | 5 | 12 | f10560 |
| balioAndSunder_2 | 1 | 41 | 3 | 10703..10794 | LR | 16:9 | -90..398 | 3 | 12 | f10800 |
| balioAndSunder_2 | 1 | 41 | 3 | 10955..10995 | L | 16:9 | -113..-1 | 2 | 12 | f10920 |
| balioAndSunder_2 | 1 | 41 | 3 | 13548..13665 | LR | 16:9 | -76..424 | 4 | 12 | f13560 |
| balioAndSunder_2 | 1 | 41 | 3 | 16105..16151 | R | 16:9 | 320..422 | 3 | 12 | f16080 |
| balioAndSunder_2 | 2 | 39 | 3 | 7286..9682 | LR | 16:9 | -170..485 | 16 | 1E | f09240 |
| balioAndSunder_2 | 2 | 41 | 3 | 9686..10045 | LR | 16:9 | -165..382 | 4 | 1E | f09720 |
| balioAndSunder_2 | 2 | 41 | 3 | 10521..15802 | LR | 16:9 | -182..477 | 9 | 12 | f15240 |
| balioAndSunder_2 | 2 | 41 | 3 | 15967..16019 | L | 16:9 | -115..-3 | 1 | 1C | f15960 |
| balioAndSunder_2 | 2 | 41 | 3 | 16104..16151 | R | 16:9 | 320..462 | 3 | 12 | f16080 |
| balioAndSunder_2 | 2 | 41 | 3 | 21388..21832 | LR | 16:9 | -115..416 | 2 | 1E | f21720 |
| battle_commands | 1, 2 | 24 | 0 | 1558..2082 | L | 16:9 | -64..-0 | 2 | 18 | cmd_attack |
| eldritchGene | 1 | 77 | 6 | 1296..1421 | R | 16:9 | 321..416 | 5 | 12 | f01320 |
| eldritchGene | 2 | 77 | 6 | 1296..1430 | R | 16:9 | 321..487 | 5 | 12 | f01320 |
| masterAndManillo | 2 | 30 | 3 | 3194..3305 | R | 21:9 only | 374..409 | 1 | 14 | f03240 |
| shop | 2 | 7 | 1 | 2154..2180 | L | 21:9 only | -141..-54 | 1 | 0C | f02160 |
| textureglitch | 1, 2 | 19 | 1 | 1567..1598 | R | 16:9 | 320..374 | 1 | 12 | f01560 |
| whelpBoss | 1 | 2 | 0 | 1267..1302 | L | 16:9 | -50..-4 | 1 | 12 | f01320 |
| whelpBoss | 1 | 1 | 0 | 1965..2041 | R | 16:9 | 320..417 | 5 | 12 | f02040 |
| whelpBoss | 1 | 1 | 0 | 2146..2201 | R | 16:9 | 320..406 | 5 | 1E | f02160 |
| whelpBoss | 1 | 1 | 0 | 2427..2475 | R | 16:9 | 320..410 | 8 | 12 | f02400 |
| whelpBoss | 1 | 9 | 1 | 4935..5012 | LR | 16:9 | -120..419 | 6 | 02 | f04920 |
| whelpBoss | 2 | 2 | 0 | 1215..1250 | L | 16:9 | -50..-4 | 1 | 12 | f01200 |
| whelpBoss | 2 | 1 | 0 | 1864..1989 | R | 16:9 | 320..478 | 5 | 1E | f01920 |
| whelpBoss | 2 | 1 | 0 | 2094..2149 | R | 16:9 | 320..459 | 5 | 1E | f02160 |
| whelpBoss | 2 | 1 | 0 | 2375..2440 | R | 16:9 | 320..477 | 8 | 12 | f02400 |
| whelpBoss | 2 | 9 | 1 | 3925..3973 | R | 16:9 | 323..466 | 1 | 02 | f03960 |
| whelpBoss | 2 | 9 | 1 | 4399..4476 | LR | 16:9 | -164..486 | 6 | 02 | f04440 |

The attract cycle's further repeats (frames 7,099.., 10,054.., 12,621..) are
the same three scenes again and are in `scenes.tsv`; the whelpBoss route's
frames differ between widths because the route parts (section 5.3). The
attract's rows are townsfolk who walk a half-cell and wait (script `wait`),
so a 19-frame "parked" stretch there is a pause in a walk, not an actor left
standing; the demo sets `Field_ScriptFlags` 0x100 throughout, so all of it
counts as a scene.

### 2.2 The other classes

`census1/scenes.tsv` has every scene; the main groups:

- **still-scene** (a party member or the kind-2 / extra object at rest in a
  scene): `balioAndSunder_1` area 3 ch 2, `balioAndSunder_2` areas 41 and
  45, `cutsceneAndNue` area 23, `textureglitch` area 19 (all three party
  members, -82..-1 at 16:9, -135..-1 at 21:9), and world-map areas 16 and 45
  (`campingFishing`, `caughFish`, `masterAndManillo`: one `other` record,
  mask `0C`). At 21:9 only: `caughFish` area 16 (712..738) and
  `masterAndManillo` area 16 (3338..3344).
- **parked** (the player in control): the fishing spot, area 30, in
  `campingFishing`, `caughFish` and `masterAndManillo` (up to 21 objects,
  mask `10`: the leader's state is the fishing game's, so these are the
  fish), towns (`worldMapAndAreaTransition` area 7, `fairyVillage` area 29,
  `bossAndFlash` area 27, `balioAndSunder_2` areas 41 and 51), and at 21:9
  only `balioAndSunder_2` area 51 (926..969, x 437..530), `fairyVillage`
  area 29 (twice, x 421..465) and `shop` area 7 (x -142..-54).
- **still** and **passing**: 5 / 20 scenes at 16:9, 6 / 15 at 21:9.

**BA's open question, "free walking reads scene 0":** yes where the player
walks a town (`worldMapAndAreaTransition` area 7, `fairyVillage` 29,
`bossAndFlash` 27, `balioAndSunder_2` 41 and 51: mask `00`). Two places set
the scene bits without a scene: the world map (mask `0C`, bits 4 and 8, on
every world-map area the routes cross: 16, 45, and area 7's entry) and the
attract demo (`02`). ST's 21:9 hide keys on the scene mask: on the world
map it would hide every band object. The fishing spot sets only `0x10`,
which is not part of `scene`.

### 2.3 Contact sheets

`analysis/shots/uw_census1/sheets/<class>_w<N>_<k>.png` (20 sheets): per
class and width, the shot nearest each scene (preferring one inside the
scene's longest worst-class row), twelve to a sheet, each with a caption
(recipe, width, area, chapter, class, band, recipe frames, shot, objects),
the 0 / 320 edges in yellow, the 16:9 edges in blue on the 21:9 tiles, and
every row's rectangle - red parked, orange still, green moving, bright when
the row spans the shot's frame. `slideouts_w1_w2.png` is section 6.3's.
`uw_eb1/census_report.py` rebuilds them.

Seen on them (by eye, not counted): the whelpBoss area-1 scene has figures
standing in the right 16:9 band; in balioAndSunder_2 area 41 the boxes lie
partly over black (actors drawn in the dark, or the box's transparent part:
band-census.md 5's caveat).

## 3. The middle of the picture

`uw_eb1/mid640.py`: every 16:9 shot against the 21:9 shot of the same name,
the 4:3 middle (640 x 480 at k = 2) compared. With any difference counted,
112 of 1,114 pairs are identical; most differ by 1 in a channel at a few
dozen scattered pixels (bilinear sampling into a wider target). Counting
only differences above 8: **817 of 1,114 identical**. The rest:

- **message boxes anchored to the picture's edge** (12,000..22,000 pixels
  in a box-shaped region at y 336..454, x 0..332 or 310..640): WA's anchors,
  `Widescreen_Anchor()`, move 67 columns further at 21:9; by design;
- **single pixels** (1 each): sprite edges;
- **whole frames** on `whelpBoss`, `dragonTransform` and `cutsceneAndNue`
  shots after the routes part (section 5.3).

And from the hash runs' own shots at speed 1 (`uw_eb1/mid_vs_narrow.py`):
`field_menu`'s seven and `menu_screens`' five shots have a middle identical
to the narrow shot at both widths (0 pixels above 8); `dragonTransform`'s
`gene` shot differs (50,251 at 16:9, 46,108 at 21:9).

## 4. Against SS's table

`census1/ss_compare.tsv` (`census_report.py`'s `compare`): each SS row in a
band or "needs census" against the census scenes of its area (and its
chapter when SS has one), by side (L or R from SS's `sx`); a census object
(area, chapter, record, side) confirms only the SS row whose `sx` is nearest
its rectangle. "Beyond 560" rows are left out.

| Verdict | Rows | Of them SS's 16:9 / 21:9 / needs census |
|---|--:|---|
| confirmed, x within 40 px | 8 | 2 / 0 / 6 |
| confirmed, side only | 1 | 0 / 0 / 1 |
| consistent, but its side's objects sit nearer another SS row | 11 | 0 / 2 / 9 |
| refuted on this route (area and chapter seen, nothing on that side) | 4 | 1 / 0 / 3 |
| area loaded, other chapter | 4 | 2 / 2 / 0 |
| area loaded, no band row at all | 1 | 0 / 0 / 1 |
| not reached (no route loads the area) | 180 | |

### 4.1 Confirmed

| Area | Ch | SS site | SS class, band | SS x, y | Census |
|--:|--:|---|---|---|---|
| 7 | 1 | `6x` `0x5DFF42` `+0x10[1]` (SS's #8) | parked, 16:9 | 347, 214 | `worldMapAndAreaTransition` 447..505, obj1 R 16:9, x 322..418, parked, scene 0 |
| 39 | - | party `+0x18[14]` `0x5F35F4` | spawn pop, 16:9 | -38, 175 | `balioAndSunder_2` 7286..9682, obj6 L 16:9, x -69..-29, scene 1 |
| 9 | - | `0x` `0x5E116E` `+0x10[6]` | parked, needs census | -19, 66 | `whelpBoss` 4935..5012, obj16 L, x -75..-6, parked, scene 1 |
| 9 | - | `0x` `0x5E1107` `+0x10[0]` | parked, needs census | 1222, 159 | the same scene, obj2 R (side only) |
| 19 | - | `0x` `0x5E6761` `+0x10[0]` | parked, needs census (21:9 if centred) | 411, 246 | `textureglitch` 1567..1598, obj5 R 16:9, x 320..374, parked, scene 1 |
| 19 | - | party `+0x18[5]` `0x5E6A38` | waits, then enters; needs census | -38, 38 | `textureglitch` 621..679, party2 L, x -66..-1, still, scene 1 |
| 41 | 3 | `6x` `0x5F4D4B` `+0x10[3]` | parked, needs census | -80, 65 | `balioAndSunder_2` 9686..10045, obj5 L, x -77..-2, parked, scene 1 |
| 51 | - | `0x` `0x5FCDF7` `+0x10[2]` | waits, then enters; needs census | -58, 239 | `balioAndSunder_2` 1224..1319, obj4 L, x -114..-0, parked, scene 0 |
| 77 | - | `0x` `0x60AE20` `+0x10[4]` | spawn pop, needs census | -30, 9 | `eldritchGene` 3310..3312, obj1 L, x -43..-4, scene 1 |

"Confirmed" means an object of that area (and chapter) was drawn in the band
on that side with its rectangle near SS's point - not that it is the same
object: the census names the record slot (`obj5`), SS the script. Matching
the two needs `+0x83`'s script index in the census row (a small
`band_census` addition, section 8).

### 4.2 Refuted on these routes

- area 27 ch 2, `7x` `0x5EC98A` `+0x10[2]`, parked, 16:9 band, -46, 17
  (SS's #3): `bossAndFlash` plays area 27 ch 2 and its only band row is one
  object on the right (534..584, x 321..397). Either the scene is a branch
  the route does not take, or SS's camera point is off.
- area 27, `0x` `0x5EC9E7` `+0x10[17]`, spawn pop, needs census: same route,
  same answer.
- area 23, party `+0x18[20]` / `[21]`, needs census (beyond 560 if the
  speakers are centred): `cutsceneAndNue` has area 23 band rows only on the
  left (party0, party2, 4075..4162); nothing on the right.

The four "other chapter" rows are area 39 chapters 6 and 9 (SS's #10 among
them); `balioAndSunder_2` is in area 39 at chapter 3.

### 4.3 New: census scenes no SS row explains

`census1/new_rows.tsv`: **89 scenes** (parked-scene, still-scene or parked)
whose objects confirmed no SS row - 45 at 16:9, 44 at 21:9 - in areas 1, 2,
3, 4, 7, 16, 23, 24, 27, 29, 30, 41, 45, 51, 77 and 141. Most are what SS
does not model: the party (still-scene, `+0x18` scripts SS reads but scene
banks move), the attract demo (areas 2 and 4, chapter 16), the fishing spot's
fish (area 30), the world map's objects (areas 16, 45), walkers in towns.
The ones that look like the owner's class - actors at rest in a band while a
scene runs, not in SS: **whelpBoss area 1 ch 0** (standing figures, three scenes,
5 to 8 objects, right band), **balioAndSunder_2 area 41 ch 3** (several
scenes, both sides), **eldritchGene area 77 ch 6** (five objects incl. the
party, right band, 1296..1430) and **battle_commands area 24 ch 0** (two
objects left, 1558..2082). Their shots are on the parked-scene sheets.

**Case zero** (area 175, chapter 7, `Scena07_Scene2`): **not reached.**
`fairyVillage` has no save header, loaded the owner's slot 0 and stayed in
area 29; no other route loads area 175 (`census1/areas.tsv`). A route that
starts in the community at chapter 7 is needed for EB-2 to see it.

## 5. The state hash at 16:9 and 21:9

### 5.1 Method

`state-hash.md` and `abc-sweep-1012.md` 6: the ini-less launcher copy,
`BOF3X_OPT=none BOF3X_LAYERING=0`, `BOF3X_STATEHASH_SKIP=tools/statehash_skip.txt`
(173 ranges), no language overlay, speed 1, the shop with `--slot0-hold`;
`uw_eb1/hash_all.sh`. Each of the ten routes with `cn_<route>_orig2` /
`_orig2b` references ran three times: **`--wide 0` as the control** (ours,
the same hour, the same build), `1` and `2`. Files
`analysis/statehash/cn_<route>_eb1w<N>.sh`. Then `statehash.py check` of
each against the pair and `statehash.py diff` of `w1` / `w2` against `w0`
(`uw_eb1/hash_check.sh`, outputs `uw_eb1/runs/hashcheck_<route>_eb1.txt`).

**The repeat.** Six routes ran again with raw dumps (`BOF3X_STATEHASH_DUMP`,
`uw_eb1/after_hash*.sh`; files `cn_<route>_eb1dw<N>.sh` and `.bin`), at
`w0` and `w2` (and `w1` for `combat`, `dragonTransform`, `cutsceneAndNue`,
`whelpBoss`). `uw_eb1/repeat.py`: every repeat is identical to its first run
on every tick but for the sound page `0x7DE000` at one tick (`cutsceneAndNue`
w0 6574 and w1 3574, `shop` w2 1968) and the exe host's tick-3 pages on the
first world-map run (HANDOFF's trap). So every difference below is the
width's, not the machine's. The byte reads are `statehash.py bytes` on the
dumps, grouped by `uw_eb1/bytes.py`, which counts the platform and renderer
symbols (window, device, texture caches, packet pools, VRAM shadow) on one
line and lists the rest.

### 5.2 Results

`w0` against the `orig2` pair: on every route only the pair's known
staleness - `0x905000` from the first save-list read (combat tick 226) - and,
on `dragonTransform` and `field_menu`, the music pages (`0x904000`
`Music_Track`, `0x665000`, `0x6BD000`, `0x7DE000`) from about tick 333 /
753; the narrow control is the baseline below.

| Route | Ticks | w1 vs w0 | w2 vs w0 | Byte read (dumps) |
|---|--:|---|---|---|
| combat | 2,561 | 11 pages | 12 pages (`0x6BE000` too, 378..450) | ticks 300 / 1200 / 2400, w2: **`Sprite_Objects` object 2 byte 0 `0x81` narrow, `0x01` wide (bit 7)**; otherwise picture state only |
| shop | 3,137 | 13 pages | 14 | ticks 500 / 1500 / 2500, w2: objects 0 and 2's position (`+0x2E..+0x3E`), state (`+1`, `+8`, `+0xD`, `+0x2A`) differ; `WindowRecords` x words (WA) |
| worldMapAndAreaTransition | 2,113 | 13 | 18 (tick-3 pages: first-run noise) | ticks 600 / 1200 / 1800, w2: object 2 (600), object 0 (1800) position and state; `Text_CurrentName` |
| field_menu | 1,409 | 14 | 9 | not dumped; no object page among them |
| menu_screens | 1,729 | 10 | 10 | not dumped; `0x803000` (`WindowRecords`) from 1156 |
| caughFish | 3,841 | 14 | 14 | not dumped; `0x7DF000` (`Sprite_Objects+0x180`) from 769, `0x7E1000` (`Effect_Objects`) 394..767 |
| masterAndManillo | 5,505 | 14 | 14 | not dumped; `0x7DF000` 652..3306 |
| dragonTransform | 4,289 | **200 pages** | **199** | 973 / 980 / 1100: picture only (and a known `EnemyWorkingRecords+0x8E4` word); **1700: `PartyWorkingRecords`, `BattleHook_Event`**; 2100: those and `EnemyWorkingRecords` |
| cutsceneAndNue | 7,425 | 20 | **313** | 940, 4500: picture only (message box pen x, WA); 4589: battle entry (`Field_Members`, `BattleHook_Event`) a tick apart; 4600: converged; **5120: `WindowRecords` state bytes `+0x4B` / `+0x6F` 2 against 1, `Text_Records`, `MoveScript_TintRecords`** |
| whelpBoss | 13,121 | **392** | **392** | w1: 500 / 850 / 900 / 950: picture only; **980: `ObjTrio+1` (the leader's state) 3 narrow, 6 wide; `Text_Records`, `WindowRecords`, `BattleHook_Event`, `EnemyWorkingRecords`** |

"Picture state" is every page and symbol of the draw: `Sprite_DrawRecords`,
`Gfx_PacketNext` / `_PacketPools`, `MapView_CellItems` / `_CornerPtr` /
`_Inset` (3 or 6 against 0) / `_ScreenXY`, `Prim_VertexScratch`, the GTE
scratch, `Desktop_Width`, `Gfx_ScreenRect` (the static pages `0x66B000` and
`0x7DE000`, which differ from tick 0), and `WindowRecords`' x words where WA
anchors a window.

### 5.3 What it means

- **UC's prediction holds:** the off-screen flag is decided at the wide
  bound. On `combat` it is the only game-state byte that differs, and it
  differs exactly as predicted (one object off screen narrow, on screen
  wide).
- **Field objects move differently** on `shop` and the world-map route at
  21:9 (dumped), and the object page differs on `caughFish` and
  `masterAndManillo` at both widths. Which reader turns the flag (or the
  width) into a position is not traced. UC's nine readers do not include a
  field object's movement script; either a reader UC's one-shape scan missed
  (UC2 greps again) or another width-dependent test. The routes reach the
  same end here.
- **Three routes part**, and they are the battles and battle-like menus:
  - `dragonTransform`, at **both** widths: between ticks 1100 and 1679 (the
    party-record page `0x802000` first differs at 1679), in a fight with the
    gene menu. Already at recipe frame 981 (the recipe's `gene` shot) the
    screens differ: narrow shows the gene grid open, 16:9 the grid with the
    command bar elsewhere, 21:9 no grid and another help line - while at
    ticks 973, 980 and 1100 the only non-picture bytes that differ are
    `WindowRecords` x words (`+0x26A`, `+0x2F8`) and the text pen. The
    windows are where it starts; the battle records follow.
  - `cutsceneAndNue`, at **21:9 only** (16:9 converges): the battle windows'
    state from about 5,000; at recipe frame 6,001 narrow and 16:9 show one
    hit on Nue, 21:9 a hit on a party member.
  - `whelpBoss`, at **both** widths: between ticks 950 and 980 the leader's
    state (`ObjTrio+1`) is 3 narrow and 6 wide, and one side opens a message
    and a fight the other does not. Before 950 no game symbol in the dumps
    differs. **Not traced.** Two candidates, neither checked: the CRT's
    `rand` state is in the heap, not in `.data`, so the hash cannot see a
    `Rand` call made only at one width (by a bit-7 reader, or by an effect
    that lives longer off screen) until its result shows; and
    `Encounter_OnScreen` (UC's row 16) keeps Capcom's bound but is fed
    camera numbers that may differ.
  - The battle windows' slide-off bounds are among UW's fourteen rules
    (`battle_e7.cpp`'s gene lists, `bound + c`): a window that frees itself
    later moves when the recorded inputs land. That fits `dragonTransform`
    and `cutsceneAndNue` (a window state differs first), and is a guess.
- **At 16:9 this has been the shipped behaviour since 2026-09-30** (UC's
  section 2): `dragonTransform` and `whelpBoss` part at `--wide 1` too. A
  player of the wide mode does not replay a narrow recording; the game is
  not broken by it - each side plays on - but it is a divergence of logic,
  not of picture, and it is unledgered. It is the measurement UC's draft A
  and UC2's DIV B ask for.

What EB-2 should do after UC2: the same ten routes at 0, 1 and 2. With the
flag split, `combat`'s bit-7 byte should be gone; whether the object
positions and the three routes come back to narrow says whether the flag
was the whole cause or the slide bounds / an unknown reader are.

## 6. The looks

### 6.1 The left message box at 21:9

UW's 3.4: the kind-0 left box's centre word goes to -4 at 120 columns. In the
attract cycle (`attract_cycle_w1/a49`, `_w2/a49`; a speaker's box at the
bottom left) the box **draws whole** at both widths, at the same pixels of
the picture (x 30..435 of the 2x shot): game x -38 at 16:9 and -105 at 21:9,
i.e. 15 px from the picture's left edge either way. Text, frame and fill are
intact; no wrap of the u16 shows. Whether every consumer of the centre word
reads it as signed was not traced; this box's draw does. At 21:9 the box sits
far from the 4:3 action, at the edge of a 560-wide picture - **the owner's
eye**.

### 6.2 Manillo's backdrop at 21:9

`masterAndManillo` reaches the trade screen (shots f02520..f03120).
`masterAndManillo_w2/f02520`: the tiled fish pattern covers all 560 columns;
a check of every column against the column 64 px (32 game px) to its right,
rows 440..479 below the windows, finds **0** breaks at 21:9 and at 16:9 (the
pattern keeps its 32-px phase across UW's band quads); the middle 640 x 480
of the 21:9 shot is **identical** to the 16:9 shot's middle. UW's four-quad
backdrop works as built.

### 6.3 The menu slide-outs

A window of shots every 3 frames over `sortScreens` 1650..1800 (the field
menu's Status page to Config and back; `recipe_shots.py --every 3 --from
1650 --to 1800`), at both widths: `sheets/slideouts_w1_w2.png`.

- **Timing is unchanged**: at every frame the window stands at the same
  place relative to the 4:3 view at 16:9 and 21:9, and the screens settle on
  the same frames (1680, 1719).
- **But at 21:9 the sliding windows are seen in the bands**: at frame 1671
  the party list is still whole inside the left band; at 1674 the Config
  table appears whole at the right of the picture, inside the band, on its
  first frame; at 1716 the party list reappears inside the left band. At
  16:9 the same frames have the windows cut by the picture's edge. The
  slide-**offs** were moved by the columns (UW); the slide-**ins** still
  start where the original started, which at 21:9 is inside the picture.
  **The owner's eye**: a pop at the slide's first frame, in the band.

### 6.4 The terrain margin and the ring at the edges

`uw_eb1/voidscan.py`: in every 21:9 shot, the share of pure-black pixels in
the 21:9-only columns against the 4:3 middle; 247 of the 21:9 shots have a band
at least 0.25 blacker. By eye on a sample of six (`campingFishing` f00840,
`whelpBoss` f08040, `dragonTransform` f00960, `cutsceneAndNue` f01320,
`field_view` field0, `balioAndSunder_2` f16200) and the world-map thumbnails:
**every one is an interior** (a room on black), the title (pillarboxed art),
or the battle floor's fall-off - no notch where the ring runs out and no
terrain cut at the margin of 167. The outdoor fields (world map, towns,
cliffs) fill the 560 columns. Pops at the edges while the camera moves were
not looked for (the shots are 120 frames apart). The margin of 167 is still
**the owner's eye**, in play.

### 6.5 The narrow areas

Of UC's seven areas narrower than 560 (53, 90, 98, 153, 154, 192, 194) and
four narrower than 320 (31, 149, 186, 198), the routes enter **90**
(`balioAndSunder_2` f00480..f00720, `campingFishing` f00600..f02880,
`masterAndManillo`) and **31** (`attract_cycle` a09..a34, `whelpBoss`
f01680..f01800), by the game log's file opens (`uw_eb1/areashots.py`).
Area 31 is a lift shaft seen at another camera, black on both sides at every
wide width; area 90 is a small room on black. Neither looks broken; both are
the void UC described. None of the others is reached.

### 6.6 The draw-item pool

Read from the raw dumps (`uw_eb1/pool.py`, `DrawItemPool_Top` - 1):
`worldMapAndAreaTransition` tick 1200 **671 narrow, 1,161 at 21:9**
(x 1.73); combat 454 / 665 / 809 at 0 / 1 / 2; dragonTransform 278 / 388 /
444; shop 147 at 21:9. Single ticks, not peaks; all inside 2,047. The world
map's ratio is above UC's model (1.38..1.56). No peak logging exists yet.

## 7. Defects met (described, not numbered)

- **`stallion.txt` crashes at recipe frame 10,681 at every width**:
  `CRASH 0: exception 0xC0000005 ... reading 0x00000000` in `Area_ZoneAtIn`
  (`window_task.cpp:227`, the original `0x595450`) from `Area_PickMusic` /
  `Field_ChangeArea` / `Field_LeaderStepLands`, area `0x1D` - a zone list
  pointer of 0 for the area being entered (the original's loop has no end
  test). The same at `--wide 0` (`uw_eb1/runs/stallion_w0.runlog`), so not
  the width. The recipe has no save header and loads the owner's slot 0, so
  the route is not its own; where it leads, and whether Capcom's side
  crashes the same, were not checked (dump
  `uw_eb1/L/bof3x.crash-43004-0.dmp`).
- **The wide modes change logic** (section 5): bit 7, field object
  positions, and three routes. Ledger material, not a defect list: DIV B
  (UC2) is where it goes.
- **The census's scene mask is set on the world map** (section 2.2):
  matters for ST's 21:9 hide.

## 8. What was not verified

- **Nothing was traced to a cause** in section 5.3 beyond the bit-7 byte:
  which reader moves the field objects, and why the three routes part, are
  inferences or open.
- The census's rows were checked against the pictures by eye on the sheets,
  not counted; "confirmed" in section 4 is same area, side and nearby x,
  not the same script.
- Case zero, area 175, and every SS area in section 4's "not reached" list.
- The nine no-save recipes played area 29 only; their own routes were not
  censused.
- The terrain margin and the slide-ins in motion: shots 120 (3) frames apart
  only.
- The pool's peak, the packet buffer, the void channel (not built).
- `--speed 8` against speed 1 for the census routes (DIV-0048's "check each
  new route once"): not done; the hash runs are speed 1.

## 9. For the other files

**`docs/README.md`**, a row:

| [`ultrawide-census-1.md`](ultrawide-census-1.md) | The ultrawide round's "before" census (group EB-1, live, 2026-10-13): `BOF3X_BANDLOG` on 32 recipes and the attract at 16:9 and 21:9 (165 scenes, the class tables, contact sheets), SS's table checked (9 confirmed, 4 refuted, 89 new scenes; case zero not reached), the state hash at 0 / 1 / 2 on the ten routes with byte reads (the off-screen flag measured; field objects move; three routes part, at 16:9 too), the looks (left box, Manillo's backdrop, slide-ins in the band, edges, areas 31 and 90). MEASURED. |

**`docs/STATUS.md`**, the round's line: EB-1 - the census ran; the wide
modes change logic on every route (bit 7 measured) and change the route on
`dragonTransform`, `whelpBoss` (16:9 and 21:9) and `cutsceneAndNue` (21:9).

**`docs/HANDOFF.md`**:
- UC2: the bit-7 split is measured necessary (`combat`); after it, the
  object positions on `shop` / world map and the three parted routes are
  the check - if they persist, the slide-off bounds of the battle windows
  and a reader UC's scan missed are next.
- ST: the scene mask is set on the world map (`0C`) and by the attract
  (`02`); key the 21:9 hide on more than the mask, or the world map's band
  objects vanish. The slide-ins at 21:9 start inside the band.
- EB-2: a route into area 175 at chapter 7 (case zero); save headers for the
  nine no-save recipes; `band_census` could log `+0x83` (the script index)
  so a census row names SS's site.
- `stallion.txt` crashes at frame 10,681 at every width (section 7).

**Ledger material** (the coordinator's): section 5 is the measurement UC's
draft A / UC2's DIV B cite for "the 2026-09-30 widening changed logic until
now": at 16:9, `dragonTransform` and `whelpBoss` part from narrow; at 21:9
`cutsceneAndNue` too; bit 7 of `Sprite_Objects[2]` measured on `combat`.
No new DIV is drafted here: nothing was changed.
