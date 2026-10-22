# The narrow areas pillarboxed (wide-ring round, side group PB)

**Status:** BUILT (2026-10-21), measured headless live (self-tests, the state hash, two routes' captures);
**cut to three areas on the owner's look at the crafted-save sheets** (group
PX, 2026-10-21, section 11; branch `wide-ring/px` off `wide-ring/round`
`4b07b9a8`); the three-area build not seen by the owner. Branch `wide-ring/pb` off `31249d0c`. Group PB of
[`wide-ring-round.md`](wide-ring-round.md) section 7. A DIV-0041 amendment,
drafted in section 8 (the orchestrator numbers and folds it). Scripts and
runs: `C:\Users\kerok\bof3x-runs\r_ring\PB` (`tools/map_extent.py`,
`chain.py`, `logs/`, `shots/`, `sh/`); PX's in `C:\Users\kerok\bof3x-runs\r_ring\PX`.
**Folded 2026-10-21 (group END):** section 8 is DIV-0041's 2026-10-21
pillarbox amendment (areas 31, 90 and 198, as PX redrafted it; menus in a
framed area stay framed, the owner's call).

## 0. The answer

- In the wide modes **areas 31, 90 and 198 are framed as a narrower
  picture**: black bars drawn by us over the bands, the picture inside them
  one of the port's three widths. **31 and 198 are framed at 320** at 16:9
  and 21:9; **90 at 426** at 21:9 and unchanged at 16:9. Every other area,
  and 4:3, is unchanged. *(As PB built it, every area narrower than the
  picture was framed - eleven; the owner took eight out on the crafted-save
  sheets, section 10, because at their own cameras they show scenery past
  the frame, not void. Section 11.)*
- **Which areas is listed; the class is measured.** The three are the
  owner's choice (2026-10-21), named in the code as such (`Listed`), not a
  Capcom table. The width each is framed at comes from the area's map
  block as the game loaded it (`AreaMap_Header`), by the same metric as
  [`ultrawide-culls.md`](ultrawide-culls.md) 5, at area entry. Re-derived
  offline over all 200 `AREA*.DAT` (section 1): eleven areas narrower than
  the picture, two classes, no area between 10.0 and 13.4 columns.
- **Draw-only.** The bars are appended to the frame at the present, after the
  frame hash; nothing the game reads changes except one thing on purpose:
  `Widescreen_Anchor` (the side message boxes, the area HUDs) anchors to the
  framed picture's edge, so a side box is not drawn under a bar.
- **The owner's worry - anything drawn past the area's width:** by reading
  every area's code and every scene that names these areas, nothing is placed
  past the framed edge on purpose (section 5). What the bars hide is the void,
  symmetric full-frame fills (a fade, area 198's red fills and bars), and
  overflow the original's 320 picture clipped too. Three camera views the
  metric does not model are listed for the owner's eye (section 5.2).
- **The rule the brief named was not the rule its lists follow** (section 2):
  "the narrowest preset that holds the area" frames 53's class at 560, not
  426. What gives the brief's lists is the *widest* preset the area's widest
  row reaches, never below 320.

## 1. The measurement (re-derived)

`tools/map_extent.py` (harness root; on `tools/dat.py`): for every
`DAT/AREA*.DAT`, the kind-0 chunk tagged `0xC8000` is the area block
`LoadDatFile` puts at `AreaMap_Header` `0x8CB580` (u8 width, u8 height, u16
offset; a cell `(x, y)` is present when the u16 at `(x + w y + 2 off) 2` is
not 0, interior `1..w-2 x 1..h-2`, as `MapView_CellToMap` `0x56F910` tests).
Along each diagonal `x + y` - one ring row, one screen row at the field's yaw
`0x200` - the span of present cells in ring columns; the widest such row.

| Area | Map | Cells | Widest row (columns) | Frame |
|---|---|--:|--:|---|
| 31, 186 | 10 x 241 | 1,680 | 8 | 320 |
| 149 | 43 x 30 | 266 | 7 | 320 |
| 198 | 30 x 30 | 25 | 5 | 320 |
| 53 | 80 x 55 | 396 | 15 | 426 (21:9) |
| 90 | 80 x 55 | 480 | 15 | 426 (21:9) |
| 153, 154, 192, 194 | 80 x 55 | 397 | 15 | 426 (21:9) |
| 98 | 100 x 80 | 516 | 16 | 426 (21:9) |
| the other 189 | | | 18 or more | none |

UC's table is reproduced exactly (its script was lost with its scratchpad).
No area has a widest row of 11 to 13 columns, so nothing is framed at 320 at
16:9 for being under 426 only, and no area is framed at 426 at 16:9.

## 2. The rule

**Which areas (group PX, 2026-10-21): 31, 90 and 198** - `pillarbox.h:57`
`Listed`, `:61` `Visible` (a listed area by the width rule below, any other
area at the full picture), read in `pillarbox.cpp:239..240`. The owner's
decision in chat on 2026-10-21, after the crafted-save sheets of section 10:
*only the areas with real void past the frame keep the bars - 31, 90 and
198; every other area renders fine without it.* So 149, 186, 98 and the
camps (53, 153, 154, 192, 194) are not framed: at their own cameras they
draw sky, wall, floor and the clearing's rim past the frame (section 10's
table). This is a choice of ours, not a value of Capcom's; the list is three
area numbers and nothing else.

**The width a listed area is framed at** is still measured, not listed: it
is what gives 90 a different frame at 16:9 (none - 426 is the picture) and
21:9 (426) without a table of widths. Each listed area's measurement is
logged at its entry (`pillarbox.cpp:56` `WidestNow`); one that measured as
wide as the 21:9 picture - no void to frame, the list and the data
disagreeing - says so on that line (`LISTED, but as wide as the 21:9
picture`). An unlisted area is not measured at all.

`pillarbox.h:43` `FrameColumns`, `:50` `Framed`. One ring column is 32
screen px at the field camera (the 320 view's widest row is 10.0 columns,
`ultrawide-culls.md` 5). The area's width is `widest x 32`. The frame is the
widest of the three pictures (320, 426, 560 - each by
`widescreen_rule::ColumnsForAspect`, not a literal) that this width reaches,
and 320 below them all; the visible columns are the frame's or the picture's,
whichever is fewer. So 8 columns (256 px) -> 320; 15 (480 px) -> 426 at 21:9,
426 at 16:9 (the picture itself: no bars); 18 or more -> the picture.

The brief said "the narrowest preset that holds the area (320 or 426)". That
rule frames 53's class (480 px) at 560 at 21:9, which contradicts the brief's
own list and the owner's widening. The rule above gives both of the brief's
lists. It means **the 426 class loses the tips of its widest rows**: at
480 px, about 27 px a side where a row that wide is on screen (of the class,
only 90 is still framed). The self-test (`pillarbox.cpp:113` `SelfTest`)
checks the width rule on 12 widths, the list over every area word (exactly
31, 90 and 198), and the rule by area on 12 rows (the three listed, the
eight taken out, and area 2); it keeps two negative controls that must each
be refused: PB's rule with no list (every area by its measurement - refused
on the eight taken out) and "holds" (refused on 90).

## 3. When the bars show

`pillarbox.cpp:235` `VisibleColumns`, `:249` `Cover`:

- the area is one of the three listed (section 2);
- the view is wide and `Widescreen_ArmFills` has run (every self-test
  compares the original's picture);
- `BOF3X_PILLARBOX` is not `0` (the switch; default on);
- `Game_Mode` is not 5, the battle (`mode-rest.md` 1.1) - the arena is not
  the area. Every other mode in a framed area is framed: the field (2), its
  scripted frame (4), the area entry (1), the look (6, 11), the shop (7), the
  menus (3, 9, 10). The menu is the question in section 9;
- `Game_AreaNumber` is believed (`:77` `UpdateTrust`): task slot 0 is the
  field task's and the title menu's (`TitleTask_Run` `0x588E70`), and the title
  menu runs with the area word as the demo left it - Start pressed in the
  demo inside area 31 would have framed the title menu. So the word counts
  only once it has changed since slot 0 was last seen free at a present (the
  demo and the title menu are always separated by free frames - the title's
  states 5, 6 and 0..3; `mode-tasks.md` 2). `GameMode_Start` writes `0xFFFF`
  first, so a game loaded into the area the demo ended in still counts. The
  one miss: a load into that same area where no present falls between mode
  0 and mode 1 - the bars then come at the next area change. Not seen;
  noted.

**On the fade.** `Area_Enter` stores the area number and loads the area's
block in the same step, after the leaving transition has gone to black
(`mode-flow.md` 4 step 5), and the entering fade runs in mode 1 with the new
number. So the bars change with the picture while it is black. The log line
`pillarbox   frame F, area A, mode M: V columns a side visible of C` names
each change; section 6 has where they fell on the routes.

**The bars.** Two untextured opaque black quads, `[-c, -v)` and
`[320 + v, 320 + c)` in game x, the full height, in the draws' own
coordinates (game units times `D3d_ScaleX` / `_ScaleY`), appended by
`render::AppendFill` (`render_shim.cpp:1160`) at `Present`
(`render_shim.cpp:427`) after `BOF3X_FRAMEHASH`'s hash. They are in the
render target, so the recipe shots (`BOF3X_SHOT_DIR`) and F11 have them.

## 4. The anchors follow the frame

`Widescreen_Anchor` (`widescreen.cpp:57`, the one call in SW's file) is now
`pillarbox::Anchor(Widescreen_Fill())` (`pillarbox.cpp:243`): the fill
columns, or the visible columns when fewer. Its users
(`window_task.cpp:61..74` - the side message-box placements 3..6 of kinds 0
and 2, and their settled-x readers; `area_w2e.cpp` area 104's gauge, ring and
panel; `area_w3b.cpp` area 121's gauge; `area_w4b.cpp` area 172's panel)
then put a side box at the framed edge: Capcom's place in a 320-framed area,
the 16:9 place in a 426-framed one. Only the message boxes are live in the
eleven areas PB framed (and so in the three still framed). The anchor writes a window record's x, which the wide modes
already move ([`ultrawide-census-3.md`](ultrawide-census-3.md) 0: "window x
words and message pen x"); here it moves it back towards narrow's.

## 5. The owner's worry: anything drawn past the area's width

Read 2026-10-21 (every area's code in `area_w*.cpp` and every scene that
compares `Game_AreaNumber` with one of the eleven; none of their area files
calls a `Widescreen_*` function or holds a screen-x constant):

| Area | What draws there | Past the framed edge on purpose? |
|---|---|---|
| 31 | no area code; chapter 0 sets pitch +0x100, distance 0x100 (`scena_sc0.cpp:218..234`), turns the camera to pitch +0x1C0 (`:415`), loops the corridor by shifting the map 0x1E cells (`:423..431`) | no |
| 53 | a choice pair, a trigger; scenes place sprites; a camera turn to pitch 0 and back over 120 frames (`scena_sc6.cpp:646..660`) | no (the turn: 5.2) |
| 90 | a choice pair; music | no |
| 98 | effect kind 0x37 at an object; chapter 10's scene (`scena_sc9b.cpp:1294..1485`): kinds 0x61 (a member scattered to particles), 0x62 (rays and a ring onto a map point), 0x69, 0x64 - all at map or actor points | no: kind 0x61's particles fly past the edges as overflow; at 21:9 they now leave at the 16:9 edge |
| 149 | dust (kind 0x37) behind the leader, a view shift (`area_w3g.cpp:653..772`); chapter 15 starts the camera at distance 0xD00 and zooms in (`scena_sc15.cpp:352..361`) | no (farther only narrows the map) |
| 153, 154 | a choice pair each | no |
| 186 | the camera (0x100, 0, 0) at distance 0x100, the corridor loop (`area_w4d.cpp:613..637`) | no |
| 192 | messages, a transition, six event objects | no; the message box: section 4 |
| 194 | chapter 13's first visit: distance -0xE00 (much closer), zoomed out over 30 frames (`scena_sc13.cpp:275..290`); kind 0x75, a full-screen tile through `Widescreen_FillX` | no (the close camera: 5.2) |
| 198 | kinds 0xA6, 0xA7 at objects; 0xAC, 0xAA red full-frame gradients and 0xA8 / 0xB0 red bars, all `Widescreen_FillX` .. `FillRight` (`rest_3f.cpp:1178`, `:1410`); 0xA9 a half disc from (160, 240) growing to about 210 px radius; 0xAD, 0xAE at points | no: the fills are symmetric and cropped; 0xA9 overflowed 0..320 in the original too |

`wide_staging`'s re-staged walks (DIV-0122) touch none of the eleven.

### 5.1 What the bars do hide

The void the brief is about; symmetric fills widened by DIV-0041 (fades,
flashes, area 198's red) now end at the frame - the framed area looks as the
4:3 (or 16:9) game did; overflow of effects that left the 320 picture in the
original. A sky backdrop (`AreaMap_DrawBackdrop`, `area_backdrop.cpp:97..118`)
would be cropped where an area has one: which of the eleven have one was not
read from the data (the captures of 31 and 90 show black, section 6).

### 5.2 Camera views the metric does not model (for the owner's eye)

The width is measured at the field camera (yaw `0x200`, distance 0, level
ground). Three of the eleven are seen otherwise:

- **31 and 186**, the corridor, at yaw 0, pitch +0x100, distance 0x100. At
  yaw 0 a screen row runs along map x, not the diagonal, so the diagonal
  metric (8) is not the screen width. The corridor's interior is 8 cells
  across in x too (x 1..8 of 10; `map_extent.py`'s x span). Whether it is
  under 320 px at every row of the picture is section 6's capture: the
  census had it "black on both sides at every wide width"
  ([`ultrawide-census-1.md`](ultrawide-census-1.md) 6.5).
- **194**'s first visit starts with the camera far closer (distance -0xE00)
  and pulls back over 30 frames: for those frames the map is magnified and
  wider than 426, so at 21:9 the bars hide map the 560 picture showed (as
  16:9 always did). Not captured (no route reaches 194).
- **53**'s pitch-0 turn: the visible width at pitch 0 is not modelled. Not
  captured.

## 6. Checks

All from `C:\Users\kerok\bof3x-runs\r_ring\PB` (`chain.py`, `logs/`,
`summary.txt`), the branch at `0e095c08`, a launcher copy with no ini, the
machine shared with three other groups' games the whole time (so the noise
pages of a loaded machine are expected, `state-hash.md` 4).

- **Build:** `cmake --build build`, no diagnostic.
- **Self-test** (`BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'`,
  `--game bof3 --no-config`, the dev launcher): exit 0 at `BOF3X_WIDE` unset,
  1 and 2; `inject: 10087 ours, 0 left original`; no non-zero `MISMATCH`
  line; `pillarbox self-test: 45 rounds, 0 mismatches; the control refused on
  5 rows` at each width. (A first try failed on the test, not the rule: a
  block with offset 0 and every cell set writes cell (1, 0)'s word over the
  header's offset. The test sets interior cells only now.)
- **State hash, the ten `cn_` routes, narrow** (`chain_sh`'s conditions:
  `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the committed skip list, exe host),
  against `cn_<route>_orig3` / `_orig3b`: the same pages as main's own
  `cn_<route>_a1020` on caughFish, cutsceneAndNue, dragonTransform,
  field_menu, menu_screens and worldMapAndAreaTransition (the ledgered
  DIV-0092 and button-round pages); whelpBoss a subset of a1020's; combat
  seven pages at tick 3 only, shop and masterAndManillo the sound page
  `0x7DE000` besides - the loaded machine's noise class. The pillarbox is
  inert narrow by construction (`Cover` returns before reading anything;
  `Anchor(0)` is 0).
- **Wide against narrow:** `Rand` (the `randlog` line a frame) **equal on
  every frame of all ten routes at 16:9 and 21:9** (2,621 to 13,182
  frames). The differing pages are the draw-state and display pages the
  ultrawide round lists ([`ultrawide-census-3.md`](ultrawide-census-3.md) 0):
  `0x66B000`, `0x6BE000`, `0x7E0000`, `0x903000`, `0x905000`, `0x929000`,
  `0x92A000`, `0x937000..0x939000`, the VRAM shadow, the sound page.
- **The pillarbox itself, off against on** (`BOF3X_PILLARBOX=0` / unset,
  21:9, the two routes that enter a framed area): whelpBoss (area 31) differs
  only on VRAM-shadow, sound and `0x937000` pages at a few isolated ticks
  from tick 3, `Rand` equal on 13,182 frames; masterAndManillo (area 90) on
  `0x7DE000` at scattered ticks and `0x6C3000` (`Gfx_TexCache`) from tick 324
  - and that page differs from the same tick between *any* two of the
  route's four runs (w0 / w1, w1 / w2, w0 / w2-off, w2-off / w1), so it is
  run to run, not the bars. `Rand` equal on 5,520 frames.
- **Where the bars switched** (the change log lines): whelpBoss at 21:9 on at
  frame 1650 entering area 31, off at 1861 entering area 1;
  masterAndManillo on at 259 entering 90, off at 321 entering 45 - every
  switch in mode 1, the area entry, inside the transition's black. The shot
  route's capture at its switch (whelp f01580, 21:9) is black whole.
- **Captures** (point filter, scale 2, `shots/shot_a31_w0..2`,
  `shot_a90_w0..2`):
  - area 31 (the lift shaft): narrow, the shaft lies within x 50..270 of
    320; at 16:9 and 21:9 the same picture between bars at 0 and 320 -
    nothing of the shaft cut at its own camera (pitch +0x100), so **its class
    holds at the camera its scenes set** (the brief's question; area 186
    has the same map and camera);
  - area 90 (the room): 16:9 unchanged; 21:9 framed at 426 - the room sits
    inside the frame with black around it, nothing cut at the route's focus;
  - area 90's menus (items and config, opened there at 21:9): framed at 426 -
    section 9 question 1;
  - the control, area 2 at 21:9 (whelp f01560): the full 560 picture.
- **Band census** (`BOF3X_BANDLOG` on the two shot routes at 16:9 and 21:9):
  no row in area 31 or 90 - no sprite drawn wholly in the bands there.

## 7. Not verified

- *Since captured by crafted saves, section 10* (all nine; area 194's first
  visit excepted). As first written: areas 53, 98, 149, 153, 154, 186, 192,
  194 and 198 were not captured: no
  recorded route reaches them, and a chapter-jump save lands where its
  chapter's Start puts it, not in a chosen area. The class of each is the
  measurement of section 1 and the log line its first entry writes.
- The owner's eye on everything (and, since section 11, on the three-area build).

## 8. The DIV-0041 amendment (drafted, not numbered)

> **Amended 2026-10-21 (the wide-ring round, side groups PB and PX,
> [`narrow-pillarbox.md`](narrow-pillarbox.md)): three narrow areas framed.**
> *Original behaviour:* the 4:3 picture; in the wide modes an area narrower
> than the picture showed the void past its map's edge in the bands (eleven
> areas by their maps, [`ultrawide-culls.md`](ultrawide-culls.md) 5).
> *New behaviour:* in the wide modes **areas 31, 90 and 198** are framed as
> a narrower picture on purpose: black bars drawn by us over the bands. The
> three are the owner's choice, not a value of Capcom's: the areas with real
> void past the frame. The width each is framed at is measured: the widest
> of the port's pictures (320, 426, 560) that the area's widest screen row
> reaches, never below 320, the row in ring columns of 32 px from the map
> block the game loaded (`AreaMap_Header`), once per area entry - on the
> shipped data 31 and 198 at 320 (16:9 and 21:9) and 90 at 426 (21:9 only).
> The other eight areas the measurement names (149, 186, 98 and the camps
> 53, 153, 154, 192, 194) are not framed: at their own cameras they draw
> sky, wall, floor and the clearing's rim past the frame, which the
> crafted-save captures showed (`narrow-pillarbox.md` 10). The bars are
> appended to the frame at the present, after the frame hash
> (`render_shim.cpp` `Present`, `pillarbox.cpp` `Cover`); they change with the
> area number, which `Area_Enter` sets while the picture is black. Off in a
> battle (`Game_Mode` 5), narrow, before `Widescreen_ArmFills`, until the
> area word has changed since task slot 0 was last free (the title menu runs
> with the demo's last area), and under `BOF3X_PILLARBOX=0`. Edge-anchored
> elements follow the frame in: `Widescreen_Anchor` gives the visible
> columns when fewer than the fill's, so DIV-0041's side message boxes stand
> at the framed edge. Draw-only otherwise: the view, the inset, the culls,
> the ring and `Rand` are the wide mode's. *Why:* the owner, 2026-10-15
> (`owner-review.md`, "a per-scene pillarbox"); 2026-10-21 widened to every
> area narrower than the picture, then, on the crafted-save sheets the same
> day, cut to the three with void past the frame ("every other area renders
> fine without it"). *Verification:* the rule's self-test
> (`BOF3X_SHADOW=pillarbox`: the widest row on test blocks, the frame at 0,
> 53 and 120 columns, the list over every area word, the rule by area on the
> eleven and an ordinary area; two controls refused - every area by its
> measurement, and "the narrowest picture that holds the area"); `Rand`
> equal to narrow on every frame of the ten hash routes at 16:9 and 21:9;
> no page changed by the bars (`BOF3X_PILLARBOX=0` against on, the two
> routes that enter areas 31 and 90, PB); captures of 31, 90 and 198 framed
> and 149, 154 and 186 unframed at 16:9 and 21:9 (PX). *Owed:* the owner's
> eye on 90's cropped row tips at 21:9 and on a framed menu.

## 9. Questions for the owner

1. **Menus in a framed area.** The menu (and the shop, BATE's screens) opened
   in a framed area is framed too, so it has no pop at its fade. Keep, or
   show menus at the full picture (bars off in modes 3, 9, 10)?
   *Recommendation:* keep - it switches on no fade of its own, and the menu's
   layout is the 4:3 one anyway.
2. **The 426 class's row tips.** Framing 53's class at 426 crops about 27 px
   a side of its widest rows (480 px). Keep 426, or leave those seven at 560
   with their void? *Recommendation:* 426, as the owner's list says; look at
   area 90's 21:9 capture.
3. **Area 194's close camera** (section 5.2): framed at 426 at 21:9 from its
   first frame. *Recommendation:* keep; 16:9 has always shown this much.

## 10. The nine areas captured from crafted saves (group CS, 2026-10-21)

**Status:** MEASURED headless, branch `wide-ring/cs` off `wide-ring/round`
`862325c4`; not seen by the owner. Section 7's "not captured" answered.

**How.** A chapter-jump save does land in a chosen area: the game block's
pending area change (`+0x24` area, `+0x27` entry flags, `+0x28` / `+0x2C`
x, z - `GameMode_Start` hands them to `Field_ChangeArea`) is in the save
beside the chapter bytes, and `Area_Enter` runs the chapter's EnterArea for
that area before state 2 resumes the run at the saved step. So a craft
puts the party where the original's own `Field_ChangeArea` would, with the
run at the step after it. `tools/chapter_jump.py` (committed) writes them
from the owner's recipe saves: the chapter, run and step, the area and
position, chapter flags (`Cond_Flags + 8 c`, block `+0x5B0 + 8 c`) and
story flags (block `+0x650`), the `+0x70` checksum (the donor's checked
first). The saves are game data and stay in the harness root
(`C:\Users\kerok\bof3x-runs\r_ring\CS\saves`); the recipes are
`tools/recipes/cs_<area>.txt` (ending17's load opening, shots named by
recipe frame, the scene's state peeked with each). Runs: launcher copies
`CS\L1..L5` in game directories of the root's own (`CS\g1..g5`, junctions to
`bof3\DAT`, `BGM`, `SND`), `BOF3X_OPT=none`, point filter; dialogue shows in
the PC's Chinese (no translation source in an ini-less directory). Sheets in
`C:\Users\kerok\bof3x-runs\r_ring\CS\sheets\pb_<area>.png`: rows 4:3,
16:9, 21:9, 21:9 with `BOF3X_PILLARBOX=0`; the shots in `CS\shots\cs_<area>_<w0|w1|w2|w2off>`.

**The class, by each area's first-entry line** (`pillarbox   area A: ...
framed at N columns a side`): 149, 186, 198 at 0 (320) at 16:9 and 21:9;
53, 98, 153, 154, 192, 194 at 53 (426) at 21:9 and the picture at 16:9 -
section 1's table, every one. The bars switch on in mode 1, frame 423, the
load's entry fade.

| Area | Craft (donor; what it resumes) | What the sheet shows |
|---|---|---|
| 53 | `ninaWalkBehindBlock`, chapter 6 run 3 step 0xB in 53 at (0x230000, 0x290000), flag 0xA set, 0xB clear: `Scena06_Run03` after its own change into 53 | **The pitch-0 turn:** step 0xC (pitch 0) is black at every width (f0450..f0500); the turn back (step 0xD, f0510..f0630) looks up past the clearing's trees into the dark. **At 21:9 the bars crop real scenery:** with `PILLARBOX=0` the trees and the grass ellipse of the clearing reach the 560 picture's edges (f0570, f0610, f0700); framed at 426 they are cut at the bars |
| 98 | `identify`, chapter 10, no run, at `Scena10_Run2`'s entry (0x380000, 0x240000) | the dais room; off, the lit floor runs on into the bands and darkens there; framed, cut at 426. Nothing drawn on purpose past the frame |
| 149 | `adult_ryu`, chapter 15 run 6 step 0x32 in 149 at (0x80000, 0xC0000), flag 0x16 set: `Scena15_Run6` after step 0x31 | **Cropped: a sky backdrop and the map itself.** The scene's camera is off the field yaw: the bridge runs corner to corner over a gradient sky; `Scena15_EnterArea` starts it at distance 0xD00 (peeked 0xBB0 at f0448) and zooms it to 0 by f0688. Framed at 320, both the sky and the bridge are cut at the bars at every frame; `PILLARBOX=0` shows both continuing to the 560 edges |
| 153, 154, 192, 194 | `adult_ryu`, chapter 14 (194: chapter 13, flags 2..4 set), no run, at (0x120000, 0x180000) | camps (the same map as 53 and 90): the ground ellipse is wider than 426 - off, its dark rim reaches nearly to the 560 edges; framed, cut. No sprite past the frame |
| 186 | `adult_ryu`, chapter 14, at `Area173_Tail38`'s entry (0x48000, 0x640000) | **Cropped: the corridor's wall.** At its own camera the machinery wall runs on to the left past 320 (off: to the 560 edge); framed, cut at the bar. The 31 / 186 check of section 6 (the shaft inside 50..270) holds for 31's view, not for this one |
| 198 | `adult_ryu`, chapter 15 run 6 from step 0 in 198 (as `Scena15_Run5`'s step 0x24 leaves it) | the void, a floating figure, a message box: nothing past 320; the side box stands at Capcom's place framed and at the 560 place off (section 4's anchor, as designed) |

**Area 194's first visit was not reached** by CS; group CR reached it
(`cs194f`, the party restored) and settled the crash as the craft's - the
donor's one-member party run through `Scena13_SaveParty7` again - with
Capcom's own code crashing the same way on it:
[`area194-entry.md`](area194-entry.md). CS's account: the craft (`cs194`: chapter 13
run 2 step 1, flags 2..4 clear) and two others into the same entry (no run;
run 2 from step 0 in area 153, so the run's own `Field_ChangeArea` takes
the party in) all crash at the entry, mode 1: `Sprite_DrawPass`'s sort
(`draw_pass.cpp:245`) reads a draw-list pointer past `Sprite_DrawList`'s 40
(`Sprite_DrawListCount` 0x3E..0x3F; read `0x80903932`). The entry runs
`Scena13_SaveParty7` (call table A 1: the party set swapped for set 7)
before it. The donor is a chapter-14 save rewound to 13, and the only
chapter-13-or-later save there is; whether the original crashes the same
way on this state was not tested (a run with `BOF3X_ORIGINAL` over
`Sprite_DrawPass` and the chapter-13 entry was attempted with a mangled
variable and never started; not repeated). **The close camera (distance -0xE00 pulled back over 30 frames)
stays unseen.** To see it: a save the owner makes in chapter 13 before
area 194's first visit (any save in chapter 13 with flag 2 clear would do
as a donor for `cs194`).

**What this says about the metric.** Section 5.2's three unmodelled views
were the corridor (31, 186) and the two scene cameras (194, 53); the
captures add **149** and **186** as areas whose own camera shows map past
the 320 frame, and the **426 camps** (53, 153, 154, 192, 194, and 98's
room) whose drawn ground is wider than 426 - the "row tips" of section 2
are the visible rim of the clearing, not void. Every one of the nine was
otherwise as section 5 read it: nothing placed past the frame on purpose.

### 10.1 Questions for the owner (from the captures)

1. **Area 149's sky and bridge.** The framed 320 picture cuts a sky gradient
   and the bridge across it; the scene is a far camera over the sky, not a
   narrow room. *Recommendation:* take 149 out of the framed set (or frame
   it by the camera, not the map); look at `sheets/pb_149.png`.
2. **Area 186's wall.** The same: its camera shows the corridor's wall past
   320. *Recommendation:* take 186 out, or look at it beside 31 first.
3. **The camps at 426** (53, 153, 154, 192, 194; and 98). The bars cut the
   clearing's rim and trees; the void PB was framing is only the far
   corners. *Recommendation:* leave them at 560 (unframed), since what the
   bars hide is scenery; `sheets/pb_053.png`, `pb_154.png`.
4. **Area 194's first visit:** a chapter-13 save before it, if the owner
   has one or makes one, to see the close camera and to settle whether the
   crash is the craft's or Capcom's.

### 10.2 The owner's answer (2026-10-21)

Questions 1..3 answered by taking the areas out: **only 31, 90 and 198 keep
the bars** - the three with real void past the frame; 149, 186, 98 and the
camps (53, 153, 154, 192, 194) render fine unframed. Area 194's first
visit went to group CR. Built by group PX, section 11.

## 11. Three areas framed (group PX, 2026-10-21)

**Status:** BUILT, measured headless; not seen by the owner. Branch
`wide-ring/px` off `wide-ring/round` `4b07b9a8`. Runs and scripts:
`C:\Users\kerok\bof3x-runs\r_ring\PX` (`chain.py`, `summary.py`,
`sheet.py`, `logs/`, `sh/`, `shots/`, `sheets/`; slots `g0..g4` / `L0..L4`:
game directories of the root's own with junctions to `bof3\DAT`, `BGM` and
`SND`, and ini-less launcher copies).

**What changed.** `pillarbox.h:57` `Listed` (31, 90, 198: the owner's list,
cited, not a Capcom table) and `:61` `Visible` (a listed area by the width
rule, any other at the full picture); `pillarbox.cpp:239..240`
`VisibleColumns` returns the picture for an unlisted area before measuring
it. The measurement stays as the listed areas' class (it is what puts 90 at
426 at 21:9 and leaves it alone at 16:9) and as a check: a listed area whose
widest row reaches the 21:9 picture logs `LISTED, but as wide as the 21:9
picture` (`pillarbox.cpp:56..74` `WidestNow`). The trust guard, the battle
exclusion, `BOF3X_PILLARBOX=0`, the side-box anchor and the bars are
unchanged.

**Checks.**

- **Build:** `cmake --build build`, no diagnostic in our files.
- **Self-test** (`BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'`,
  dev launcher, `--no-config`, an ini-less copy): launcher exit 0 at
  `BOF3X_WIDE` unset, 1 and 2; `inject: 10087 ours, 0 left original`; every
  `MISMATCH` line reads `0 MISMATCHES`; `pillarbox self-test: 82 rounds, 0
  mismatches; the controls refused on 8 rows (no list) and 1 (holds)` at
  each width.
- **State hash, the ten `cn_` routes** at w0, w1 and w2 (`BOF3X_OPT=none`,
  `BOF3X_LAYERING=0`, the committed skip list): **`Rand` equal to narrow on
  every frame of all ten at 16:9 and 21:9** (1,455 to 13,182 frames). Wide
  against narrow, the pages are PB's classes: the display and draw-state
  pages (`0x66B000`, `0x6BE000`, `0x7E0000`, `0x7E1000`, `0x801000`,
  `0x803000`, `0x903000`, `0x905000`, `0x929000`, `0x92A000`,
  `0x937000..0x939000`), the sound page `0x7DE000`, `Gfx_TexCache`
  `0x6C3000`, and VRAM-shadow pages (`0x7B9000..0x7C7000`, shop's at ticks
  2..4 only; `0x762000..0x78E000` on menu_screens at 21:9, the class PB saw
  on field_menu and dragonTransform). No game page. Narrow against
  `cn_<route>_orig3` / `_orig3b`: combat exit 0; the others the pages
  section 6 lists for them (the DIV-0092 and button-round pages, the noise
  class of a loaded machine), plus VRAM-shadow and `0x937000` pages on
  cutsceneAndNue, masterAndManillo, menu_screens and worldMap. Two runs
  (cutsceneAndNue w0, w1) were first refused by `input_run`'s slot-0 guard
  while the self-test's game was up, and re-run.
- **Where the bars switched** (the change lines): area 31 (whelpBoss's
  recipe) on at frame 1580, off at 1791 entering area 1, at 16:9 and 21:9;
  area 90 (balioAndSunder_2's) on at 397 at 21:9 only (`framed at 53
  columns a side of 53` at 16:9: no bars), off at 736; area 198 (`cs198`)
  on at 423 at both widths. Areas 149 (`cs149`, in 149 from recipe frame
  430), 154 and 186 (`cs154`, `cs186`, from frame 480): no change line and
  no measurement line - unframed, unmeasured.
- **Captures** (point filter, en-US, scale 2, this build;
  `shots/shot_a<area>_w1|w2`): sheets `sheets\px_219.png` (21:9) and
  `sheets\px_169.png` (16:9). 31: the shaft between bars at 0 and 320, both
  widths. 90: the room framed at 426 at 21:9 (and its menu, f00500); at
  16:9 the full picture. 198: the void and the floating figure, the message
  box inside the 320 frame. 149: the bridge and the sky gradient to the 560
  edges. 154: the camp's ground to the edges. 186: the machinery wall to
  the left edge. The crafted saves were regenerated with `chapter_jump.py
  make` into the PX root from the main checkout's donors.
