# Three scene faults from the owner's play: Teepo's kick, Rei's tail, the Mt. Glaus sunset

**Status:** IN PROGRESS (2026-11-25, measured and built on branch `fix/scene-touchups`; the owner's eye in play is owed)

The owner's report, 2026-11-25, three things seen at 16:9 on the play install:

A fourth came while the work was open: the plate on that table looks squared at its corners (section 3a).

1. On the Yraall road, Teepo is drawn over Ryu when he "kicks" him. Wanted: Teepo behind Ryu.
2. Upstairs in Bunyan's house, Rei's tail "clips through the table". Wanted: the tail behind the table until
   the end, when he swipes it over the table and hits the plate.
3. In the Mt. Glaus cutscene the sky "used to turn orange red" and now stays blue.

None is Capcom's. The first two are faults of DIV-0071's rule (ours); the third is known-defects D284, a
fault of the wide picture's grown ring (ours, DIV-0041), on record since 2026-10-13 and not a new
regression. All runs: `en-US`, the PC install (`bof3`), an ini-less launcher copy, the harness root
`D:\bof3x-runs\scene1125` (`L` the build of `main` `8a7c4954`, `L2` this branch). Captures are not
committed.

## 1. The sunset (D284)

**Not a regression in time.** The route `cutsceneAndNue.txt` has a shot named `sunset_tint` at recipe frame
601. Fifty-six captures of it are on this machine, from 2026-10-03 to 2026-11-24. Every 4:3 one is orange,
`D:\bof3x-runs\orig6\suite\shots\cutsceneAndNue_w32` of 2026-11-24 included. The two wide ones
(`analysis/shots/uw_census3/cutsceneAndNue_w1` and `_w2`, 2026-10-14) are blue across the width. So the
sky was right at 4:3 always and wrong at 16:9 and 21:9 since the wide ring.

**Cause, measured.** The route cut at the shot, with `peek 0x929ED8 u8` (`MapView_BuildFlags`) at the
shot and 60 frames later: 0 and 0 at 4:3, 1 and 1 at 16:9.

- The sunset is `EffectKind18_01_Sunset` `0x4FD2E0`: `Gfx_DrawSkyGradient` `0x4FD350`, a quad over the
  frame in OT slot 7, its colour stepped from blue to orange.
- The area's own sky is `AreaMap_DrawBackdrop` `0x571BE0`, also a quad over the frame in slot 7, drawn
  only while `MapView_BuildFlags` is not 0. `MapView_Build` sets bit 0 for a view cell off the map.
- At 4:3 no cell of Capcom's 28 x 56 walk is off the map in this scene, so the backdrop is not drawn and
  the effect's sky is the only one.
- Under the wide picture the walk is the grown ring's (rows -1..55, grown columns). Its rim cells are off
  the map here, they set bit 0, and the area's blue backdrop is drawn over the sunset.

Capcom's own idiom for the same conflict is in kind 0x18's sub-kind 0x3F: its handlers write
`MapView_BuildFlags = 0` before they draw their sky (`effect_kind_18_sub_3f.cpp`, `Sky()`). The sunset has
no such write, because at 4:3 it needs none.

**Fixed** (DIV-0041's amendment of 2026-11-25). The grown walk notes whether any off-map cell is one
Capcom's walk reads (view rows 0..54, view columns inset..27 - inset). `Gfx_DrawSkyGradient` notes the
frame it draws in. `AreaMap_DrawBackdrop` leaves the backdrop out when the flag byte is 1, the bit is the
rim's alone, and the effect's sky was drawn this frame or the one before. No game byte is written, so the
state hash's pages are as they were. At 4:3, in every self-test and with `BOF3X_WIDE_RING=0` nothing
changes.

**The same class elsewhere.** A scene effect that draws its own sky, in an area with a backdrop entry
that the 4:3 view never triggers and the wide ring does. Read, not run:

| Sky drawn by | Guard against the backdrop | Under the wide ring |
|---|---|---|
| `Gfx_DrawSkyGradient` (the sunset, its one caller) | none | **was wrong; fixed here** |
| kind 0x18 sub 0x3F (`EffectKind18Sub3F_DrawSky`) | writes `MapView_BuildFlags = 0` itself, after the build | right by its own write |
| kind 0x18 sub 0x15 (the sea bridge), sub 0x44, sub 0x61 (the ending) | none read | not measured here; the sea bridge's sky and the ending's were seen right at 16:9 by the owner (owner-review, DIV-0127) |

A census of every area with a backdrop entry against every effect that draws a sky was not made.

## 2. Teepo's kick (the Yraall road, area 19)

`yraallRoad.txt`, recipe frames 3,030..3,046. `BOF3X_LAYERING_LOG=1` with a `BOF3X_DRAWORDER` window, 4:3:

| Object | Key | Depth (`+0x60`) | Capcom's layer | DIV-0071 before this fix |
|---|---|--:|--:|---|
| Ryu, `ObjTrio[0]` | `0x1B9D` | 1,124 | 27 | kept in 27 ("a sprite or a table item of the layer before": Teepo) |
| Teepo, `ObjTrio[2]` | `0x1B9E` | 1,133 | 27 | **drawn in layer 30** ("stopped by nothing") |

In Capcom's order the two are one run of layer 27, and a run is drawn in descending order of the s16 at
`+0x60` (`Sprite_DrawPass`): Teepo first, Ryu over him. DIV-0071 moved Teepo three layers later and left
Ryu, so the walk drew Ryu (`#384`, layer 27) and then Teepo (`#431`, layer 30) over him - through the
kick's frames 3,040..3,042 and, less visibly, while the two stood. With `BOF3X_LAYERING=0` Teepo is behind
Ryu.

**Why the rule missed it.** `LayerFor` stops a sprite at a sprite "of the layer before" by reading the
draw list *after* it. The list is in key order; a run is drawn in depth order. Ryu's key is below Teepo's,
so Teepo's test never read Ryu.

**Fixed** (DIV-0071's amendment of 2026-11-25, the first of two; general). After every sprite is judged, two
overlapping sprites of one layer keep Capcom's order: the one Capcom draws first is not moved past the
layer the other is drawn in (`Defer`, `DrawnAfter`).

## 3. Rei's tail (Bunyan's house, area 8)

`bunyanHouseChoppingMiniGame.txt`, recipe frames 1,178..1,560. Rei is `Sprite_Objects[7]` (`0x7DF2FC`),
movement script 3, on cell (79, 29), layer 20. The table's cells at frame 1,361:

| Walk | What | Layer | Box (320 x 240) |
|---|---|--:|---|
| `#183` | list 1: cell (80, 28), the table's back cell's top | 20 | 179..219, 73..91 |
| `#192` | **Rei**, moved by DIV-0071 from layer 20 | 21 | 141..192, 51..106 |
| `#193` | draw table, key `0x1586`: cell (80, 29), the table's near cell's top | 21 | 160..200, 82..101 |
| `#194` | the plate, key `0x1586` | 21 | 173..189, 86..102 |
| `#197` | list 1: the table's left face | 21 | 160..180, 91..120 |

In Capcom's order Rei is drawn in layer 20 before `#183`: both tops cover the tail for the whole scene,
the swipe included. DIV-0071 moved him one layer later - the back cell's top does not reach his 28 px body
box - so the tail drew over the back cell and under the near one: the "clip". With `BOF3X_LAYERING=0` the
tail is behind the table throughout (the captures differ from the rule's in the tail's box only).

**Fixed, in two parts.**

- **The clip** (DIV-0071's amendment of 2026-11-25, the second): that object on that cell is kept in its
  own layer (`ReiAtTable`). Rei stays in layer 20 and the table covers the tail, as Capcom drew it. The
  general form was tried first and withdrawn (section 4).
- **The swipe** (DIV-0246, Intent, the owner's request): Rei's poses through the scene are facing 5 (the
  talk, animations 5, 41, 49, 65) to recipe frame 1,469; facing 6 at 1,470, 7 from 1,471, 0 from 1,505 (his
  back to the camera, the plate knocked off at about 1,516), and back through 7, 6, 5 from 1,550. While he
  faces 6, 7 or 0 he is drawn one layer later with that layer's last key: after both tops, before the
  table's left face, which still covers his legs.

## 3a. The plate (the owner's second ask, the same day)

"Right now it kind of looks like its corners are squared, I think because it is getting clipped by other
tiles." It is. The plate is `Sprite_Objects[6]` (`0x7DF258`), movement script 1, at map point (80, 29) - the
corner where the table's cells meet - feet 6, key `0x1586`, layer 21, box 173..189 x 86..102. After it the
walk draws `#197` the table's left face (list 1 of layer 21, 160..180 x 91..120), `#205` the top of cell
(81, 29) (list 0 of layer 22, 180..221 x 91..111) and `#212` the front face (list 1 of layer 22,
180..201 x 101..130): each cuts an edge of the plate. Capcom's order, the same with `BOF3X_LAYERING=0`.

**Changed** (DIV-0247, Intent): at rest on that corner the plate is drawn two layers later, after the
three. A first form kept the rule while the plate slid over cell (79, 29); it is at rest only, the slide
being drawn whole already by DIV-0071.

## 4. The general form of the tail's fix, built and withdrawn

The first build tested the second list of the layer before against what the sprite drew last pass
(`reach_box`) for every sprite. Seven routes at 16:9, a frame every 8, `main`'s build against it
(`reg.py`, `cmp.py` in the root):

| Route | Frames | Differ | What |
|---|--:|--:|---|
| `yraallRoad` | 624 | 11 | the Ryu / Teepo / Rei box, 2,960..3,040: wanted |
| `ninaWalkBehindBlock`, `pathCouple` | 88, 127 | 0 | |
| `fairyVillage` | 589 | 50 | **Ryu's feet cut by the floor again** (3,760); a faerie's arm behind a pillar |
| `garrBlocks` | 165 | 25 | Garr's wing and arm behind the block where the owner accepted them over it (2026-10-29) |
| `vaccinationAndFireScene` | 413 | 7 | Garr's hand behind a block's corner |
| `bunyanHouseChoppingMiniGame` | 1,715 | 136 | Rei's feet and shadow cut on his walk to the table, besides the table scene |

A sprite's drawn box holds its shadow and its empty margins, so it reaches tops that nothing visible of
the sprite touches, and the sprite is then left where the floor cuts its feet - the fault DIV-0071 was
made for. Withdrawn; Rei at the table is named instead (section 3).

## 5. Verification

The build of this branch (`L2`) against `main` `8a7c4954` (`L`), 16:9, `en-US`, a frame every 8:

| Route | Frames | Differ | Where |
|---|--:|--:|---|
| `yraallRoad` | 624 | 11 | 2,960..3,040, the three members' box: Teepo behind Ryu, and behind Rei's tail as he walks round |
| `ninaWalkBehindBlock`, `pathCouple`, `fairyVillage`, `garrBlocks`, `vaccinationAndFireScene` | 88, 127, 589, 165, 413 | 0 | |
| `bunyanHouseChoppingMiniGame` | 1,715 | 173 | the plate's box from its first frame (89 frames before the table scene, the camera elsewhere); the table scene 1,178..1,600; 19 frames after it, a 15 x 8 px box where Ryu lies in front of Rei's foot - Rei in his own layer is drawn under Ryu's hair there, Capcom's order |

By eye, sheets in the root (`road_fix.png`, `house_fix2.png`, `plate2.png`, `sliver.png`): Teepo behind Ryu
through 3,040..3,042; Rei's tail cut by the table's edge to 1,469, over the table 1,470..1,551; the plate
round at its lower edge. One thing seen and left: at 1,528, Ryu diving, a dark sliver two pixels wide on
the table's left face beside his leg, in this build and not in `main`'s; it is not the plate's (the same
with the plate's rule limited to rest) and was not traced.

The sunset: `sunset_tint` and a shot 60 frames later, this build - orange at 16:9 and at 21:9 (the same
colour at x = 20, 560 and 1,100 of the 1,120-wide capture); at 4:3 both shots are pixel-identical to
`main`'s.

Self-tests, the hosted `BOF3.exe`, `BOF3X_SELFTEST_ONLY=1`:

- `BOF3X_SHADOW='area_backdrop,draw_pass,map_rim,map_layers,effect_5a,shell,magic_080,sprite_2,draw_emit,map_cells'`
  at `BOF3X_WIDE` 0 and 1: 21 lines, 0 mismatches, exit 0.
- The exe host (`--host exe`, `BOF3X_EXEFILE=1`): done, exit 0.
- `BOF3X_SHADOW='*'` **stops in the sound group**: `SndBuf_Play` differs in 55 of 2,000 rounds. `main`'s
  build fails the same way (`BOF3X_SHADOW=sound`, the same 55), so it is not this branch's; known-defects
  D438. The groups after `sound` in the suite's order were therefore not run whole.
- `ledger_check.py`, `ledger_registry.py --check`, `ledger_switches.py --check` pass.

**Not run:** the ten state-hash routes (a hashed run sets `BOF3X_LAYERING=0`, which turns DIV-0071,
DIV-0246 and DIV-0247 off; the sky's change writes no game byte); the 64-bit and Linux builds; any install
but the PC's; `BOF3X_LANG` other than `en-US`. **Not seen in play.**
