# Widescreen — the plan

**Status:** IN PROGRESS (2026-09-23). Step 6 of [`display-overhaul.md`](display-overhaul.md)
§5. **The survey build of §3e is built** (DIV-0041, `BOF3X_WIDE=1`,
`src/game/widescreen.{h,cpp}`): §3a's frame, the terrain cull and the
area-map frame pass's ranges. Its findings are §5.

## 1. What the owner decided

- **426 x 240, no crop.** The PSP release's 384 x 216 (320 + 2 x 32 columns,
  12 rows cut top and bottom, [`psp-widescreen.md`](psp-widescreen.md)) was
  offered and turned down: "I don't think I want to crop". 426 is 240 x 16/9,
  rounded down to an even number: **53 extra columns a side**, every row kept.
- **A 21:9 monitor stays pillarboxed.** 426 x 240 at the largest integer
  scale, black at the sides - no wider view.
- DIV-0036's rule (a window's k from the launcher, borderless the largest k
  that fits) "will get re-evaluated" with widescreen: at 426 x 240 the owner's
  3440 x 1440 fits k = 6 (2556 x 1440), full height.

## 2. The approach: Capcom's, from the PSP

Two ways to widen are on record:

- **Shift the primitives (the PSP's, chosen).** The game keeps drawing in its
  0..320 space with the projection centre at (160, 120); every primitive's x
  is moved by +53 on the way to the screen, so the original picture sits
  centred in a 426-wide frame. Anything the game already draws past its
  edges - 3D geometry, scrolling layers - fills the side bands for free, and
  anything centred stays centred. What needs work is only what was cut or
  anchored at the old edges: culls, full-frame fills, edge-anchored UI.
  Capcom did exactly this on the PSP (`+32` in all 21 primitive converters,
  51 sites, psp-widescreen §2.3) and touched six other things (§3a there).
- **Widen the coordinate space (not chosen).** Move the projection centre to
  213 and add the offset to every centred UI element - what the peer project
  `bof3ext` does (`src/hooks/widescreen_patches.ixx`, read for its site list
  only; `CLAUDE.md` rule 5). It needs every UI position in code found and
  moved, and misses show up as off-centre menus. Its site list is still the
  best inventory of PC addresses that know the screen is 320 wide (§4).

A side band the game draws nothing into stays black: the failure mode of the
chosen approach is an internal pillarbox, never a broken layout.

## 3. The work, in order

### 3a. The frame (ours, the backend)

- The render target 426k x 240k; `D3d_ScaleX/Y` stay k; the scene vertex
  shader adds 53k target pixels to x (`render_d3d11.cpp`, next to the
  `PIXEL_OFFSET` of the edge-pixel A/B).
- The game's clears, Blts and full-target operations: check each against
  the new width (`render_shim.cpp` - the back buffer is still the game's
  640 x 480 surface as it sees it; only the target grows).
- The present: 426:240 at the largest integer scale; DIV-0036's k rule
  re-evaluated with the owner.
- Behind `BOF3X_WIDE=1` for the survey; the launcher and the ledger entry
  once it holds up.

### 3b. The culls - what pops in at the edges

The PSP widened only the two culls that visibly pop (terrain and the area
map's frame pass) because its 32 columns stayed inside every other margin.
**53 does not**: three ranges Capcom left alone are now too tight. Kept
screen-x intervals, from psp-widescreen §3a / §3b and `bof3ext`'s list:

| Cull | PC | Now | Needs to reach | Note |
|---|---|---|---|---|
| Terrain | `MapView_Build` `0x56EC00` (`0x56EDFA` / `0x56EE14`), **ours** | `[-50, 370]` | about `[-117, 437]` | PSP widened by 46 for 32; ours - patch our source |
| Area-map frame pass, wide | `AreaMap_FrameAreaBD` `0x510780` (`0x51097E` / `0x510991`) | `[-200, 520]` | `[-252, 572]` | PSP +31 for 32 |
| Area-map frame pass, narrow | same (`0x5109BB` / `0x5109D2`) | `[-50, 370]` | `[-102, 422]` | PSP +31 for 32 |
| Sprite | `0x4CF319`, `0x4FF6A3`, `0x571366` | `[-60, 380]` | beyond `[-53, 373]` + sprite width | 7 px of margin left: pops. `0x571366` is `MapCell_DrawAnimated`, ours: **`[-113, 433]` since 2026-09-30** (`Widescreen_Fill`); `0x4CF319` is the battle field's, reading `.rdata`, left |
| `[-40, 360]` | PSX `80161ef4`; PC twin unread | `[-40, 360]` | wider | inside the new view: pops |
| "unknown" | `0x5054E3` / `0x5054FA` | `[-20, 340]` | wider | inside the new view: pops |
| Object | `0x4CEC09`, `0x5700D1` | `[-100, 420]` | - | 47 px margin, probably fine. `0x5700D1` is `MapCell_DrawQuads`, ours: **`[-153, 473]` since 2026-09-30**; `0x4CEC09` the battle field's, left |
| Object 2 | `0x570319` / `0x570333` | `[-80, 400]` | - | 27 px: **the trees popped** (the owner, 2026-09-30). `MapCell_DrawUprights`, ours: **`[-133, 453]`** |
| `Sprite_Draw` | `0x59360B` / `0x593615` (int16), **ours** | `[-64, 384]` | - | 11 px: **`[-117, 437]` since 2026-09-30** |
| `0x59293A` | in `bof3ext`'s list | ? | ? | unread |

Every one is read before it is changed: many sit in functions that are ours
now, where the change is to our source, not to bytes.

**Read 2026-09-23, the two the survey build changes.** The addresses above
(and in `bof3ext`'s list) are not where the floats are: they are the 4-byte
operands of `fcomp dword ptr [mem]` instructions, and the floats sit in
`.rdata` - `0x5C4230` 370, `0x5C4234` -50, `0x5C423C` 520, `0x5C4240` -200
(with `0x5C4238` 121 and `0x5C4244` 120, the y tests that choose the wide or
narrow range, untouched). An image scan finds six references to the four
range floats, all in `MapView_Build` (`0x56EDFA`, `0x56EE14`, `0x56EE2B`
for the y bound) and `AreaMap_FrameAreaBD` (`0x51097E`, `0x510991`,
`0x5109BB`, `0x5109D2`). `MapView_Build` is ours and compares against its
own constants (`Widescreen_TerrainLo/Hi`), so the `.rdata` floats stay as
they are and the frame pass's four operands are re-aimed at floats in the
dll (`PatchBytes "Widescreen"`), the way `sprt_draw.cpp` re-aims the
far-edge table. `Widescreen_Inject` runs last in `inject_all.cpp` so every
start-up fuzz compares the original bounds; a live `BOF3X_SHADOW=map_layers`
under `BOF3X_WIDE=1` will report the terrain cull's divergence as mismatches,
by design.

**Found 2026-09-27, the owner's coast route: the wider cull ran the
draw-item pool dry.** The cull keeps about half as many cells again as the
original's, a cutscene pan asks for them all at once, and the pool's 1,024
items were sized for `[-50, 370]`: cells refused an item are not drawn, their
walls show through as blue faces, and one refused every frame stays missing.
Fixed by DIV-0062, the pool doubled (its array below 16 MB, reserved by the
launcher). The narrow view already peaked at 855 of 1,023 in that scene.

### 3c. Full-frame fills and fades

Anything that fills `(0, 0, 320, 240)` - fades to black, flashes, the
battle transition - leaves the side bands unfaded: a fade to black with two
bright strips. The PSP widened these to `(-32, 0, 384, 240)` (47 sites);
ours become `(-53, 0, 426, 240)`. Known: `mode_flow.cpp:171`,
`save_menu.cpp:391` (ours, in logical units), the black fade `0x4957CF`
(`bof3ext`). The rest are found by the survey - a fade with bright sides is
unmistakable - or by a scan for `320` paired with `240` near a fill call.

### 3d. Edge-anchored UI

Centred UI needs nothing under the chosen approach. What was placed by its
distance from a screen edge moves outward by 53, as the PSP moved it by 32:

- `MsgBox_PlacementTable` `0x66AE10` entries 3..6, the side placements:
  145 / 226 to **92 / 279** (the PSP's 113 / 258). Centre placements stay.
- Candidates from `bof3ext`'s list, each to be read and judged - under the
  primitive shift most of them are centred and need nothing: the title's
  menu panel and items (`0x588C26`..`0x588C76`, `0x58893B`, `0x588A19`), the
  save menu (`0x5881AC`..`0x5887E8`), the field menu's panels (`0x589EB7`,
  `0x589F9C`), overworld popups (`0x571C85`), `0x462560`, `0x573CE0`,
  `0x576960`.
- Screen-wide art: the menu backdrop repeats a 32-column pattern a fixed
  number of times (`0x575732`); widened, it needs two more columns a side or
  its edges show. ~~The sky (`0x4112A9`)~~ - read 2026-09-23: that site is
  `WorldMap_PinSprite` `0x4112A0`, a sprite pinned to (160, 80), centred,
  nothing to do ([`area-backdrop.md`](area-backdrop.md) §1); the sky is
  `AreaMap_DrawBackdrop` `0x571BE0`, §5. The title and the full-screen
  pictures are 320 wide by nature: they stay pillarboxed inside the frame.

### 3e. Survey, then the owner's routes

1. The survey build: 3a plus the terrain and area-map culls. The attract
   sequence captured wide (`input_run.py` recipes; `analysis/shots/`), the
   field in the recipes' saves, a menu walk, a fade. Every pop, bright
   band and off-edge element goes in §5 below.
2. Fix by class (3b, 3c, 3d), one ledger entry for the view and one per
   re-authored element class, as the PSP's were.
3. The owner plays with `BOF3X_RECORD`; each route's leftovers become the
   next list, like the takeover rounds.
4. Default on or off: the owner, once it holds up.

## 4. Checks

- **The oracle and the frame hash do not change** with the view: the shift
  is in the backend and a cull's margin changes what is drawn, not logic -
  except where a cull gates logic (an object update skipped off-screen).
  Run both with `BOF3X_WIDE=1` once; a difference there is a finding.
- The 55-shot attract A/B against the 320 view, cropped to the middle 640
  columns at k = 2: the centre must be identical; only the bands differ.
- The owner's eye on everything else.

## 5. Open

- **Found by the survey, 2026-09-23** (the recipes wide at k = 2, 852 x 480
  captures in `analysis/shots/wide_*/`, `_sheet.png` a contact sheet each):
  - `wide_field`: the field of save 5 fills all 852 columns, no band, the
    sprite centred. Nothing to do.
  - `wide_title_timeline`: the title's scrolling mural fills the frame; its
    first frame (f0060) has a dark left edge, which the owner reads as the
    fade-in, not a band. The logo, PRESS START and the copyright are
    centred on black. Nothing to do.
  - `wide_field_menu`: **the wood backdrop is 320 wide** - black bands both
    sides (`Menu_DrawBackdrop`, ours; **fixed the same evening**, two more
    column pairs from -0x40 under `Widescreen_Live()`). The time box, the
    money box and the tab row keep their 320 positions: inside the wide
    picture with the bands beside them - to be judged by the owner (anchor
    to the new edges, or leave).
  - `wide_menu_screens`: **the time and money boxes that a sub-menu slides
    off the 320 edges hang visible in the bands**, cut at the old edge.
    The original relied on the screen edge to hide them. **Fixed later the
    same night:** the boxes are window-task states in 0x596000..0x59D000
    that step x by 0x20 a frame to an off-edge bound - nine `mov ecx,
    imm32` (-170, -300, -100, -180, -110, -200, -150, -120 and 320) and five
    `cmp cx / ax, imm16` (322, -190, -165, 347, 323) that free the window
    once past it - and each bound moves outward by 53 (`kSlides` in
    `widescreen.cpp`, `PatchBytes "Widescreen"`). Recaptured
    (`wide2_menu_screens`, `wide2_shop_ab`, 40 shots): nothing in the bands.
  - `wide_attract_cycle` (55 shots; three grabbed the desktop instead of
    the game while the owner took their own screenshots, deleted): the
    mine's 3D scenes fill the frame; narration, "Dauna Mine" and the title
    centred. **The dialogue box at the lower left sits against the old
    edge** (`MsgBox_PlacementTable` entries 3..6, §3d) with the band beside
    it. The owner, 2026-09-23: "the dialogue boxes seem ok to me" - left as
    they are unless one looks wrong.
  - **The owner, watching live:** (1) on an area change the 320 view goes
    black while the bands keep the last area for half a second - the fade
    tile `Transition_DrawTile` (ours, `mode_flow.cpp`) is 320 wide;
    **widened the same evening** to (-53, 0) 426 x 240 under
    `Widescreen_Live()`, with the save menu's black tile (`save_menu.cpp`).
    Whether every area change goes through that tile is for the owner's
    next look; a black centre with live bands means another fill. (2)
    About 20 px pop in and out at the top left and right when the attract
    sequence rotates the map: the terrain cull at [-117, 437]; **widened to
    [-150, 470]** (`kTerrainMargin` 100), to be re-checked by eye.
  - `wide_shop`: the recorded route has no shots; nothing seen.
  - **The sky (the owner's screenshot, 2026-09-23):** an area's gradient
    backdrop is 320 wide, black in the bands. It is `AreaMap_EntryKind1`
    `0x571BE0` (entry 1 of `AreaMap_EntryHandlers`, Capcom's, 0x147 bytes):
    a Gouraud quad from (0, 0) to (320, 240) through `Gpu_SetDrawMode`,
    `Gpu_SetPolyG4`, `Gpu_SetSemiTrans` and two `Gfx_CommitPrim`s, the two
    colours from the entry, shown while the focus lies in the entry's
    bounds. The left x is a zero register, not an immediate, so it cannot
    be byte-patched; it needs a takeover beside `AreaMap_ClutCycle` in
    `map_layers.cpp` with the quad at (-53, 0) 426 x 240 under
    `Widescreen_Live()`. Deferred until a recorded route reached it; the
    world-map route did (`world-map.md` §4). **Taken over the same night**
    as `AreaMap_DrawBackdrop` (`src/game/area_backdrop.cpp`,
    [`area-backdrop.md`](area-backdrop.md)) with its two sibling handlers:
    the quad's x from -53 to 373 under `Widescreen_Live()`, the fade tile's
    rule; the faithful quad fuzzed at 0 mismatches. **Owed:** the route's
    captures wide and narrow after the merge (the coordinator's live
    check).
  - Recorded in `wide_field_menu2`: the backdrop after its fix, seven
    column pairs, no band.
- **Manillo's trade screen, 2026-10-03** (fix wave group MB; DIV-0041's
  amendment of that date). *What the owner saw:* the tiled fish backdrop
  of "Will that be all?  Yes No" 320 wide, black bands
  (`owner_catalogue/manillo_will_that_be_all.png`; the same in
  `analysis/shots/manillo_1003/` frames 3555..3735, `caughFish.txt` wide
  at k = 5, 2130 x 1200, the morning's build). *The cause:* the screen's
  backdrop is `ItemTrade_DrawBackground` `0x5942C0`, ours since round
  thirteen's E1G (`effect_1g.cpp`; every trade state draws it last) -
  two POLY_FT4s `(0, 0)..(0xA0, 240)` and `(0xA0, 0)..(0x140, 240)`, u
  `0..0xA0`, v `0..0xF0`, under a draw mode whose texture window is a
  32 x 32 tile `(32 * (Frame_Counter >> 4 & 3), 0x80)` - the pattern
  steps through four tiles every 16 frames. The port has no texture
  window in the GPU sense: the window is the page texture's cache key,
  and `Tex_Convert4` / `_8` build the page by repeating the 32 x 32
  block over 256 x 256, so u 0..255 is eight repeats. Nothing in the
  draw knew about the bands. *What ours does:* under the columns the
  left quad runs `(-53, 0)..(0xA0, 240)` with u `11..224`, the right
  `(0xA0, 0)..(373, 240)` with u `0..213` - texels equal to columns, no
  stretch, and u at column 0 still 0 mod 32, so the band columns show
  the tiles the pattern would have had there (`ItemTrade_BackdropSpan`,
  `effect_1g.h`). The columns come from `Widescreen_Fill()` (0 until
  every self-test has run), not `Widescreen_Live()`: `Effect1G_Inject`
  sits below `Widescreen_Inject` in `inject_all.cpp`, so `Live` would be
  53 in its fuzz. *Checked:* `BOF3X_SHADOW=effect_1g` narrow and with
  `BOF3X_WIDE=1`, 84,000 rounds at 0 mismatches each, plus a property
  check of the spans for 0..63 columns in the same self-test (edges at
  `0 - c` and `320 + c`, texels = columns, phase); controls in
  [`effect_1g.md`](effect_1g.md) §12. *Owed the owner's eye:* the
  coordinator's live check below.

  **For the coordinator's live check.** The route is the owner's
  `caughFish.txt` (`# save camping`, `BOF3X_LANG=en`, about 3,890
  frames; the trade screen opens near frame 3,550 and "Will that be
  all?" is up from about 3,690 to the end). A shot copy with
  `tools/recipe_shots.py` (never by hand), then the run on the wide
  ini (`wide=1` in the launcher's `bof3x.ini`, as the morning's
  `caughFish_shots45` run had):

      python tools/recipe_shots.py --every 45 --out <scratch>/caughFish_shots45.txt tools/recipes/caughFish.txt
      python tools/input_run.py <scratch>/caughFish_shots45.txt --out analysis/shots/manillo_wide --lang en --no-front

  Frames to look at: 3555, 3600, 3645 (the list, the needs), 3690 and
  3735 ("Will that be all?"). Right: the fish pattern runs edge to edge
  across all 426 columns, no black at either side, the tiles in the
  bands continuing the columns of the middle 320 without a seam at
  columns 53 and 373 (the same tile size as the middle - a stretch would
  show wider fish at the sides), and the pattern still stepping every 16
  frames. Wrong: black bands (ours not reached or not armed - check the
  log's `DIV-0041    full-frame fills` line), wide fish in the bands (a
  stretch), or a half-tile jump at the band edges (the phase). The
  middle 320 columns must match `manillo_1003`'s pixel for pixel at the
  same frames. Not touched and still to judge: the hand a word left of
  `Yes` (DIV-0027's stops, another group's).

  **The nine full-frame sites DIV-0041's 2026-09-30 amendment left
  Capcom's** - read 2026-10-03 (`tools/pe_disasm.py`) and placed by
  `analysis/round13_cut.tsv`, not taken. Whether an owner's recipe shows
  one 320 wide was judged headless, from what is already on disk: the
  390 call traces under `analysis/calltrace/` (today's
  `reach_balioAndSunder_1/_2`, `reach_bossAndFlash`, `reach_dragonGene`
  among them) record **no** entry into any of the hosts below, nor any
  catalogued function in `0x484000..0x494000` or `0x500000..0x512000`;
  and no capture under `analysis/shots/` shows a flash or tint with
  bright or dark bands. So none is known to be visible on a recipe -
  each is **unknown**, not "fine". (A trace arms only catalogued,
  not-ours entries: the three marked hidden are entered through a
  state table and could run unrecorded if their kind ran - their hosts'
  state tables never did on any traced route.)

  | Site | Function (cut) | Group | What it draws |
  |---|---|---|---|
  | `0x489D47` | `0x489CD0` | E4B (ours 2026-10-03, still 320 wide) | effect kind 137's unit: a semi-transparent TILE `(0, 0)` 320 x 240, colour from the effect record `+0x5D..+0x5F` (a tint) |
  | `0x48CB07` | `0x48CA90` | E4D (**widened** 2026-10-03, `Effect_DrawScreenTint`) | kind 145's unit: the same tint, slot 5 |
  | `0x48CD10` | `0x48CC90` | E4D (**widened** 2026-10-03) | kind 148's unit: the same tint, its blend mode an argument |
  | `0x48DC19` | `0x48DBA0` | E4D (**widened** 2026-10-03, `EffectKind98_DrawFlash`) | kind 152's unit: a grey TILE, blend 1, level the clamped argument (a white flash or a fade) |
  | `0x493308` | `0x4932E0` | E4F (ours 2026-10-03, `EffectKindAF_DrawScreen`, still 320 wide) | kind 176's unit: a red TILE (red the argument), semi-transparent (a red flash) |
  | `0x507BDC` | `0x507BC0` (hidden, host `0x5073D0`) | E5E (**widened** 2026-10-03, `EffectKind18Sub3F_WhiteOut`) | a world-2 overlay's step: an opaque white POLY_F4 over the frame, sound `0x202`, then a scene call (a white-out) |
  | `0x507CE3` | `0x507CB0` | E5E (**widened** 2026-10-03, `EffectKind18Sub3F_DrawSky`) | a POLY_G4 gradient over the frame, two 15-bit colours from the argument (a sky, like `Gfx_DrawSkyGradient`) |
  | `0x50B4B5` | `0x50B480` (hidden, host `0x50B220`) | E5G (**widened** 2026-10-03, `EffectKind18Sub36_Pulse`) | `EffectKind18_States`: a semi-transparent POLY_F4 `(0, 0x30, b)` over the frame, b from a 4-step table - a pulsing blue tint |
  | `0x50F7B5` | `0x50F780` (hidden, host `0x50F590`) | E6B | the same pulsing tint, its own table |

  Each is a plain full-frame fill, so when its group's wave takes it, the
  fill moves to `Widescreen_FillX()` / `Widescreen_FillWidth()` (§3c) -
  except `0x507CB0`, a gradient, which takes `Gfx_DrawSkyGradient`'s
  corners. The x cull `0x5054E3` (`[-20, 340]`, in `0x505480`, E5D) is
  likewise unreached by any trace.
- Since the survey the launcher has a "Widescreen" box (`wide=1` in
  `bof3x.ini`, which sets `BOF3X_WIDE=1`); the owner plays from it.
- DIV-0036's k rule at 426 (the owner). *Overtaken (noted 2026-09-24): since
  DIV-0042 there is no fixed k rule to revisit - the window resizes freely
  and k follows it, snapped to whole multiples of the picture or fitted to
  the height.*
- The two PSP `SetGeomOffset` callers without PC twins (`(160, 144)`,
  `(?, 185)`, psp-widescreen §5) - an unmoved projection there would put
  something off-centre by 53.
- The FMVs stay 4:3 (640 x 480 at an integer scale, DIV-0035); nothing to do.
