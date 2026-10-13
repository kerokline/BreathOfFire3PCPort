# The culls and capacities at 120 columns (ultrawide round, group UC)

**Status:** MEASURED (2026-10-13), a reading group: no `src/` change. Stage A
of [`ultrawide-round.md`](ultrawide-round.md), at the round tip `6b4c97cc`.
What every screen-x bound in our source does at 0, 53 and 120 columns, whether
the field's 28-column cell ring reaches a 560-px view, what the draw-item pool
should peak at, and where the map's edge shows. Section 7 is the table the
cull-fix group (UC2) and the ring group (RG) build from. Nothing here was seen
in play; every number is from the code, the exe, or the raw `.data` dumps
already on disk, and section 8 says what that leaves open.

The view at each width, in the game's 320-wide units: narrow `[0, 320)`,
16:9 `[-53, 373)`, 21:9 `[-120, 440)`. A bound's **margin** is how far past
the view's edge it keeps things; a bound that lies inside the view drops
something the player can see.

## 0. The answer

- **Nineteen screen-x bounds** in our source (section 1). Seven already move
  by the columns and keep their margin at 120 with no new work, once UW's
  runtime columns reach `Widescreen_Fill()`. Six more still hold Capcom's
  values, and all of them lie inside the 21:9 view; four of those lie inside
  the 16:9 view too, or within 7 px of its edge. Two are sweeps of an effect across the screen rather
  than culls. Two are not picture culls at all (a fight's placement, a
  dropped sound pan). One is dead code.
- **The off-screen flag is game logic, and DIV-0041 already moves it at 16:9.**
  `Sprite_Draw` `0x5935B0` writes bit 7 of an object's byte 0 when it culls
  it. Nine functions read that bit to decide what to do next, among them the
  party's following (`Member_FollowStep`, `Member_CatchUp`), a chapter 8 scene
  that waits for an actor to leave, a boss walk, and effects that free
  themselves once off screen (section 2). The cull was widened on 2026-09-30
  by the columns, so under `BOF3X_WIDE=1` today those decisions are taken
  53 px further out than the original takes them. This is unledgered.
  DIV-0041's text says a cull changes "what is drawn, not logic". The state
  hash has never been run wide (no `WIDE` run in `analysis/statehash/`), so
  nothing has measured it. At 120 the distance becomes 120 px.
  `Sprite_ProjectA`'s cull (type-0x0A sprites, Capcom's margins, not widened)
  writes the same bit.
- **The ring: 28 columns reach 560 px at the field's own camera, but not at
  the far cameras some scenes set** (section 3). At `Camera_Distance` 0, the
  value in every field dump on disk, a 560-px view needs at most 8.8 ring
  columns either side of the focus over level ground, and 10.8 with ground
  512 units below the focus. The ring holds 13.5 either side at inset 0, and
  UW's inset rule (`ceil(columns / 20)` = 6) gives inset 0 there. The ring
  runs out at about `Camera_Distance` 2,200 over level ground, and at about
  1,600 over ground 256 below. Our code writes 2,176, 2,432 and 2,560 in three
  areas, and up to 7,296 in effect kind 0x8D's camera pull. It also runs out
  when the camera's yaw is off its usual 0x200 by 0x200 and the ground is
  low. **RG is needed for those scenes, not for ordinary field play.** A ring
  of 32 columns covers every persistent distance our code writes, over ground
  as low as 256. The PSP's 33 (I37) covers up to about 3,200 (ground 256
  below) or 3,700 (level ground). Only kind 0x8D's pull is beyond it, and that
  pull is also beyond 28 at 16:9 today. The ring's 56 rows do not depend on
  the width at the usual yaw.
- **The pool: 2,048 should hold** (section 4). The model gives 1.38 to 1.56
  times the narrow view's cells at 21:9, and 1.17 to 1.26 times 16:9's. The
  one logged narrow figure, 855 in the coast pan, so becomes about 1,180 to
  1,340. The pool runs dry only if a scene's narrow use passes about 1,310.
  No 16:9 peak was ever logged; EB measures it.
- **Map edges: there is no camera clamp** (section 5). `AreaMap_Frame`
  `0x56E6C0` steps the focus toward its target with no test against the
  map's size. A cell past the map is simply not drawn, at any width. So the
  void is there at 320 too, and the wider view shows more of it. Of the 200
  area blocks, 189 are wider than the 560 view on their widest screen row at
  the field camera, 7 fit 426 but not 560 (so the void is always in the 21:9
  bands there), and 4 are narrower than 320 already.

## 1. Every screen-x bound in our source

**Method.** Three scans, each covering a shape the others miss:

1. Our C++ (`src/game/*.cpp`, fuzz files excluded): every comparison against a
   literal in `-300..-1` or `321..700`, hex or decimal, int or float (145
   lines, read one by one); then every comparison against `320` / `319` /
   `0x140` / `0x13F`; then every comparison on an object's screen word
   `+0x2E`.
2. `BOF3.exe`'s `.rdata` and `.data`: every float or double equal to a whole
   number in those two ranges. Then every `.text` instruction (`fcom*`,
   `fld`, the float arithmetic) whose memory operand names one, placed in
   its function by `symbols.toml`, and every `src/` file naming the float or
   the instruction (17 floats, 33 sites).
3. `.text`: every pair of `cmp r, -N` and `cmp r, 320 + M` within 0x60 bytes
   (`|M - N| <= 40`), the `add r, N; cmp r, 320 + 2N` unsigned-range idiom,
   and every `cmp` against `0x13F..0x141` or `fcom` against a 319, 320 or 321
   float.

The scripts are in the session scratchpad (`uw_uc/scan_src.py`,
`scan_rdata.py`, `scan_int.py`, `scan_edge.py`). They are not committed:
each is a page of capstone over the exe, described above in full. **Every
site found is in a function of ours**, as the round doc expected.

Classes: **moved** (already moves by the columns; its margin holds at any
width), **fine**, **pops at 21:9 only**, **pops at 16:9 already** (the bound
lies inside the 16:9 view, or leaves less margin there than the thing it
culls is wide). **Logic**: the result decides something other than whether
a primitive is drawn. That was read from the code each time, not from a
name.

### 1a. Already moved by the columns

Each is `bound - Widescreen_Fill()` / `bound + Widescreen_Fill()` (or UW's
operand copies). At 120 they follow UW's runtime columns with no new code.
UW should check that each reads the columns through the function it reads
today, which is the point of its property check.

| # | Site | Function | 0 | 53 | 120 | Gates |
|---|---|---|---|---|---|---|
| 1 | `map_layers.cpp:257` | `MapView_Build` `0x56EC00` (terrain; operands `0x56EDF8` / `0x56EE12`) | `[-50, 370]` | `[-150, 470]` (`kTerrainMargin` 100, tuned by eye) | UW's: `columns + 47` past the original's, `[-217, 537]`; the owner's eye | drawing (a culled cell's item is released, `MapView_BuildFlags`) |
| 2 | `rest_3g.cpp` via `Widescreen_RangeOperand` | `AreaMapBD_BuildView` `0x510780` (area 189's frame pass; `0x51097C` / `0x51098F`, `0x5109B9` / `0x5109D0`) | `[-200, 520]`, `[-50, 370]` | `[-252, 572]`, `[-102, 422]` | `[-319, 639]`, `[-169, 489]` (`columns - 1`) | drawing |
| 3 | `map_cells.cpp:168` | `MapCell_DrawQuads` `0x5700D1` | `(-100, 420)` | `(-153, 473)` | `(-220, 540)` | drawing |
| 4 | `map_field_objects.cpp:187` | `MapCell_DrawUprights` `0x570317` | `[-80, 400]` | `[-133, 453]` | `[-200, 520]` | drawing |
| 5 | `map_field_objects.cpp:345` | `MapCell_DrawAnimated` `0x571364` | `[-60, 380]` | `[-113, 433]` | `[-180, 500]` | drawing |
| 6 | `sprite_draw.cpp:103` | `Sprite_Draw` `0x5935B0` (`0x593609`, int16) | `[-64, 384]` | `[-117, 437]` | `[-184, 504]` | **logic**: byte 0 bit 7 (section 2) |
| 7 | `magic_s24.cpp:1085` | `Fx105_MoteDraw` `0x4D2220` (`0x4D2235`) | `[-64, 384]` | `[-117, 437]` | `[-184, 504]` | drawing |

Also moved, and UW's: the fourteen slide-off bounds (`g_slides`), including
the three gene-list slide-outs that read them back.

### 1b. Still Capcom's values

| # | Site | Function | Bound (all widths) | Margin at 0 / 53 / 120 | Class | Gates |
|---|---|---|---|---|---|---|
| 8 | `battle_e6.cpp` `MapCell_DrawTexQuads` (`at::kScreenXLo/Hi`, the `.rdata` floats `0x5C4208` / `0x5C4204`, `fcomp` at `0x4CEC07` / `0x4CEC1D`) | `0x4CEB40`, the battle field's map cell 40, `bof3ext`'s `0x4CEC09` | `(-100, 420)` on a quad's first vertex | 100 / 47 / **-20** | pops at 21:9 only (at 16:9 the margin is the one `MapCell_DrawQuads` had when it was widened) | drawing |
| 9 | `battle_e6.cpp` `MapCell_DrawGroundSprite` (`0x5C4210` / `0x5C420C`, `0x4CF317` / `0x4CF333`) | `0x4CF270`, cell 43, `bof3ext`'s `0x4CF319` | `[-60, 380]` on the sprite's centre | 60 / **7** / **-60** | **pops at 16:9 already** | drawing |
| 10 | `effect_5b.cpp:410` `EffectKind18Sub0D_DrawGlow` (`at::kCullLeft/Right`, `0x4FF6A1` / `0x4FF6BD`) | `0x4FF610` | `[-60, 380]` on the glow's centre | 60 / **7** / **-60** | **pops at 16:9 already** | drawing |
| 11 | `effect_5d.cpp:880` `EffectKind18Sub1C_OnScreen` (`0x5C422C` / `0x5C4228`; x at `0x5054E1` / `0x5054F8`, y at `0x50550B` / `0x50551E`) | `0x505480`, `bof3ext`'s "unknown" `0x5054E3` | `[-20, 340]`, x and y both | 20 / **-33** / **-100** | inside both wide views | **a sound only**: `WaitThenSound` plays effect `0x200` when the column is on screen (sub-kinds 0x1C / 0x1D). Not a draw and not game state, but heard |
| 12 | `field_frame.cpp:376` `Sprite_ProjectA` (`Sprite_ScreenMargins` `0x663B28`, entry by record byte 3 bit 3) | `0x57B860`, type-0x0A sprites' screen update | entry 0 `[-80, 400]`; entry 1 `[-100, 420]` | 80 / 27 / **-40**; 100 / 47 / **-20** | pops at 21:9; entry 0 also at 16:9 for a sprite wider than 54 px | **logic**: sets byte 0 bit 7 and keeps the sprite off `Sprite_DrawList` (section 2) |
| 13 | `magic_s28.cpp:599` `BreathMote_Fly` (`cmp cx, 0x140` at `0x4DEDBE`) | `0x4DECA0` | `(0, 320)` x, `(0, 240)` y, after the state's step | 0 / **-53** / **-120** | **pops at 16:9 already** (a mote stops being drawn at the old edge) | drawing (the state has stepped already) |
| 14 | `effect_1a.cpp:1256..1315` `EffectKind0D_Start` / `_SlideOut` | `0x465F30`, `0x466050` | starts at x 0x186 (390), slides by -30 to 160, out to `<= -70`, then the record goes idle | 70 / 17 / **-50** both ends | a sweep: at 21:9 it appears inside the right band and vanishes inside the left one; at 16:9 only if the sprite is wider than about 34 px | **logic**: the end bound sets the record's state (`+1 = 0`) |
| 15 | `area_w4b.cpp:1103` `Area172_EffectA5Grow` | `0x428240`, effect kind 0xA5 of area 172 | the shade's left edge `x - 0x40` sweeps right; at `x > 0x190` (400) state 2 | the shade is dropped with 37 px of it in the 16:9 band, 104 px in the 21:9 band | a sweep; **pops at 16:9 already** | **logic**: the end bound adds one to the script variable after `MoveScript_Var7` and moves the state |

Not picture culls, listed so that UC2 does not take them for culls:

| # | Site | What it is | Class |
|---|---|---|---|
| 16 | `inventory_ops.cpp:297` `Encounter_OnScreen` `0x5928F0` (`fcomp [0x5C4280]` at `0x592938`, `bof3ext`'s `0x59293A`) | Whether a fight placed at (x, z) is on screen: refused when any corner's screen x is 320 or more, or its y 240 or more. The port dropped the PSX's lower bounds, so there is none ([`inventory_ops.md`](inventory_ops.md) section 5). | **logic** (where a fight's actors stand). Leave it: widening it would move fights. The 16:9 view already shows corners the test refused; nobody has reported a fight being placed differently. The owner's call |
| 17 | PSX `0x80161E44`'s `[-64, 304]` / `[-40, 360]` (`80161ea4`, `80161ef4`; the PSP's `0x96884`) | The PSX's positional sound pan and volume ([`psp-code-diff-field-1.md`](psp-code-diff-field-1.md) row 297). The PC calls `Port_DroppedCall`, a bare `ret`, from `Sprite_UpdateScreen` and `Sprite_QueueOverlay` instead ([`movement-script.md`](movement-script.md), `symbols.toml`). | **no PC twin**: nothing to widen. If the pan is ever restored, its x range moves by the columns. ([`psp-widescreen-placements.md`](psp-widescreen-placements.md) section 3 gives this range to `Encounter_OnScreen`. That is a mislabel: the PSX's `FUN_801C805C` compares `0x13F` / `0xEF`.) |
| 18 | `d3d_rest.cpp:502` `D3d_AfterDraw` `0x59F580` (`cmp ax, 0x140`) | A frame capture's size. Nothing in `.text` sets its request ([`d3d-rest.md`](d3d-rest.md) section 5). | dead; nothing to do |
| 19 | `layering.cpp:194` `LayerFor` (DIV-0071) | Ours from the start: a sanity window `(-64, 1024)` before the feet test, in target units, far outside any view | fine |

Read and already handled (not bounds to move): `EffectKind18Sub15_Draw`'s
strips at 320 (`effect_5c.cpp:1119`, widened 2026-10-07),
`ItemTrade_DrawBackground` (`0x5943A6`, widened). Every `320.0f` that builds
a fill has been widened since 2026-10-12 (DIV-0041).

**Sites with no screen-x bound found** that the brief asked about:
`0x4CF319` and `0x4CEC09` are rows 9 and 8; `0x5054E3` is row 11; `0x59293A`
is row 16; PSX `80161ef4` is row 17. In `bof3ext`'s list, `0x4FF6A3` is row
10 and `0x571366` / `0x5700D1` / `0x570319` are rows 5, 3 and 4.

## 2. The off-screen flag: where a cull is game logic

`Sprite_Draw` (row 6) ends its cull with `object[0] |= 0x80` and returns; an
object inside the bounds gets `object[0] &= 0x7F`
([`sprite-draw-order.md`](sprite-draw-order.md); its fuzz control "the cull
leaving bit 7 set" is refused 3,352 times). `Sprite_ProjectA` (row 12) sets
and clears the same bit by its own margins. `Sprite_UpdateScreen` sets it for
a sprite outside the ring's layers 0..0x36 (section 3.4). So the bit means
"not drawn this frame", and the next logic frame reads it.

**The readers.** A scan of the exe for `mov r, [Sprite_Current]` followed by
a byte-0 bit-7 test (`uw_uc/scan_bit7.py`), plus `grep '[0] & 0x80'` over our
source. All are ours:

| Reader | Address | What bit 7 decides |
|---|---|---|
| `Member_FollowStep` | `0x51B050` | a follower that is off screen may jump onto the leader's next half-cell outright ([`member-sprites.md`](member-sprites.md)) |
| `Member_CatchUp` | `0x51B5D0` | only an off-screen member left behind is moved toward its target |
| `Scena08_Scene4` | `0x551DE0` | chapter 8's run 4, step 2, waits until the sprite counter 3 names is off screen, then drops in the next actor and ends the run |
| `BossNina_WalkStep` | `0x439EC0` | a battle walk: steps `+0x34` until the sprite is off screen, then moves the state on (that the battle's sprites cull through `Sprite_Draw` is inference from the draw pass, not traced) |
| `EffectKind32_Arc`, `EffectKind32_Settle` | `0x46A1E0`, `0x46A290` | an arcing effect hops, or lands with sound `0x106`, when off screen |
| `EffectKind34_V4Move` | `0x46CEA0` | variant 4 frees itself (`Effect_Release`) once off screen |
| `Area65_Record8Move` | `0x40C490` | the world maps' record +8 effect (ten world maps) frees itself once it has been on screen and goes off |
| `Fish_Swim` | `0x52B8F0` | the nibble's sound `0x206` is skipped for an off-screen fish |

**What that means for the wide views.** Each of these decides at the
`Sprite_Draw` bound, which today is `[-117, 437]` at 16:9: an actor walks
53 px further before the scene goes on, an effect lives longer, a follower
waits longer before it jumps. Nothing measured says this has changed a route.
The followers are usually near the screen's centre, and the scene and the
walk depend on an actor's walk speed. But the state hash would show it, and
it has never been run wide. **At 120 the distance is 120 px.** Type-0x0A
sprites decide at Capcom's `Sprite_ScreenMargins` (row 12) at every width,
because that cull was not widened.

**The fix shape for UC2 and the owner.** Split "drawn" from "off screen":
draw by the widened bounds, but set bit 7 by the original's. `Sprite_Draw`
then keeps `x > 0x180 || x < -0x40` (and the y test) for the bit, and culls
the draw by the widened bounds. The same split applies to `Sprite_ProjectA`:
the draw-list test widens, and the bit keeps the table's margins. Logic would
then be the original's at every width, and only the picture would widen.
**The catch:** an actor that a scene waits on to leave would now be drawn in
the band after the scene has gone on. If the scene then hides or moves it,
it pops there. That is the staging class (`ultrawide-round.md` section 4),
not a cull. **This changes 16:9's state against `6b4c97cc`** (it restores
the original's logic there), so under the round's rule it is the owner's
call, not UC2's to make silently. The alternative is to keep today's
behaviour, ledger it as a divergence (draft A, section 9), and let EB's state
hash count what it moves.

## 3. The cell ring at 560 px

### 3.1 The projection

From our code: `AreaMap_Frame` `0x56E6C0` loads `Rx(a0) Ry(a1) Rz(a2)`
(`Gte_RotMatrix`, `psx_gte_matrix.cpp`) and the translation
`R * focus + (Camera_ShiftX, Camera_ShiftY, Camera_Distance + 0x1194)`
(`map_layers.cpp:184..189`). The projection is `Gte_SetGeomScreen(0x3E8)`
and `Gte_SetGeomOffset(0xA0, 0x78)` (`Field_ViewReset`, `map_scroll.cpp`).
A map cell is 128 units: `MapView_Build`'s vertex is `cell << 7`, and a
sprite's is `x >> 9` of a 16.16 cell position. The depth at the focus is
`Camera_Distance + 4,500`. As a check, `Sprite_Draw`'s scale
`0x4650000 / depth`, over the quarter depth `Gte_StoreDepthQuarter` stores,
is exactly 1 at 4,500, so sprites are drawn 1:1 at the focus.

Ring geometry, from `MapView_CellToMap` (`x = Ox + col + (row + 1) / 2`,
`z = Oz + row / 2 - col`) and `Field_ViewReset`'s origin
(`Ox = focus x - 0x18`, `Oz = focus z + 3`). A ring column is one step of
`(+1, -1)` cells, 181 units across. A ring row is half a `(+1, +1)` step, 90.5
units. **The focus sits at ring row 21 and between columns 13 and 14.**
`MapView_Build` walks the slots `inset .. 27 - inset` of each of 0x37 rows
(the loop at `map_layers.cpp:468..479`). Odd rows sit half a column over, so
every row covers at least `13.5 - inset` columns either side of the focus,
and the stair-step past that is the owner's notch of 2026-10-06. At the yaw
0x200, the camera's x axis lies along the ring's columns.

### 3.2 The cameras on disk

Every raw `.data` dump the state hash wrote (`analysis/statehash/*.sh.*.bin`,
123 files) read at `Camera_Distance` `0x903840`, `Camera_Angles` `0x929EC8`,
`MapView_Inset` `0x905D80` and `Game_AreaNumber` (`uw_uc/cam_dumps.py`):

| Where | Dumps | `Camera_Distance` | Angles (pitch, -, yaw) | Inset |
|---|---|---|---|---|
| areas 2, 4, 7, 23, 29, 30, 141 | 74 | 0 | (-682, 0, 0x200) | 6 |
| area 2, `t3` tick 5000 | 3 | 1,124 | (-684, 0, 0x200) | 5 |
| area 2, `t4` ticks 4455 / 4460 | 3 + 3 | 1,516 / 1,543 | (-682, 0, 0x200) | 4 |
| area 31, `t3` / `t4` tick 9000 | 6 | 256 | (256, 0, 0) | 0 |
| no area (title, battle) | 34 | - | - | - |

`Field_ViewReset` sets the pitch to -682 and the yaw to 0x200. The distances
our source writes (`grep Camera_Distance`): -5,120 (area 11's zoom, the
camera inside the focus; not modelled), -3,456 (area 189, with
`Gte_SetGeomScreen(0x12C)`; not modelled), -1,060 then 1,500 (area 121),
-256, 0x100, 1,664, 2,176, 2,432 (four areas), +2,560 relative (area 19,
unbounded), and 7,296 falling by 42 a frame to 1,024 (effect kind 0x8D).

### 3.3 What the view needs

The inverse projection (`uw_uc/ring_model.py`, `ring_signed.py`): for every
4th column and row of the view, the ground point under it at a height `h`
relative to the focus (`+` is lower ground, the vertex z axis). The output
is its distance from the focus in ring columns `u` (`+` is screen right) and
ring rows `v` (`-` is the screen's top). The table gives the worst `|u|` at
yaw 0x200, which is symmetric to within 0.1:

| `Camera_Distance` | `h` | narrow | 16:9 | 21:9 | original inset (kept either side) |
|---|---|---|---|---|---|
| 0 (the field) | 0 / 256 / 512 | 5.02 / 5.59 / 6.16 | 6.68 / 7.44 / 8.20 | 8.78 / 9.78 / 10.78 | 6 (7.5) |
| 1,124 | 0 / 256 | 6.28 / 6.86 | 8.36 / 9.13 | 11.00 / 12.00 | 5 (8.5) |
| 1,543 | 0 / 256 / 512 | 6.74 / 7.31 / 7.88 | 8.97 / 9.73 / 10.49 | 11.79 / 12.79 / 13.79 | 4 (9.5) |
| 2,176 | 0 / 256 | 7.44 / 8.01 | 9.91 / 10.67 | 13.03 / **14.03** | 3 (10.5) |
| 2,560 | 0 / 256 | 7.87 / 8.44 | 10.48 / 11.24 | **13.78** / **14.77** | 3 (10.5) |
| 3,900 | 0 / 256 | 9.37 / 9.94 | 12.47 / 13.23 | **16.39** / **17.39** | 0 (13.5) |
| 7,296 (kind 0x8D's pull) | 0 / 256 | 13.15 / **13.72** | **17.51** / **18.27** | **23.02** / **24.02** | 0 (13.5) |
| 0, yaw 0x300 | 0 / 256 | 7.51 / 9.30 | 9.05 / 11.01 | 10.99 / 13.17 (left) | 0 (13.5) |
| 0, yaw 0x400 | 0 / 256 | 8.86 / 11.60 | 10.04 / 12.90 | 11.52 / **14.56** (left) | 0 (13.5) |
| 256, area 31's camera | 0 / 256 | 5.67 / 6.41 | 6.70 / 7.51 | 8.01 / 8.89 | 0 (13.5) |

**Bold** is beyond the ring at inset 0 (13.5). **Checked against what was
seen:** at 16:9 with the original inset 6 (7.5 either side), the field needs
6.68 over level ground and 7.44 to 8.20 over ground 256 to 512 below. That
is a notch only where the ground falls away, which is the owner's bridge
over the sea of 2026-10-06. After the inset was lowered by 3 (10.5), the
same view needs at most 8.20 and was seen clean. The model agrees with both.

**What it says at 120:**

- **The field's own camera is covered.** At distance 0 the 560 view needs
  8.8 to 10.8, within 13.5. UW's inset rule (lower by `ceil(columns / 20)` =
  6, from 6 to 0) is enough. A rule proportional to the width,
  `13.5 - (13.5 - inset) * (160 + columns) / 160`, gives the same 0 at 120
  and the same 3 at 53 for the field camera. So the two rules agree where it
  matters, and UW's needs no change.
- **The ring is the limit beyond about `Camera_Distance` 2,200** (level
  ground) **or 1,600** (ground 256 below), and at yaws far off 0x200 over low
  ground. Our code sets 2,176, 2,432 and 2,560 in three areas: a notch at the
  21:9 edges there. Effect kind 0x8D's pull runs past the ring at 16:9 today
  (17.5 needed against 13.5), and at 4:3 it is already at the limit (13.2).
- **The ring size that covers it:** `2 * (need + 0.5)` columns. 32 columns
  cover every persistent distance our source writes, over ground as low as
  256 (14.77 + 0.5 at 2,560). 36 cover 3,900. 50 cover the 0x8D pull at its
  start. **The PSP's 33 x 58** (IDEAS I37,
  [`psp-widescreen-placements.md`](psp-widescreen-placements.md) 2.4) gives
  16 either side: every distance up to about 3,700 over level ground, 3,200
  over ground 256 below. That is everything our source writes except the
  0x8D pull's first frames.
- **Rows.** At yaw 0x200 the rows a view needs do not depend on its width
  (screen y alone sets them). The top of the screen needs 15.0 rows over
  level ground and 21.6 over ground 256 below, against the 21 rows the ring
  holds on that side. So the original's own top edge reaches the ring's end
  over low ground, at any width. At yaws off 0x200 the rows grow with the
  width: 23.0 rows at 21:9 at yaw 0x400 over level ground, against 17.7
  narrow. The PSP's 58 rows add one either side.

### 3.4 What RG has to carry with the ring

- `Sprite_UpdateScreen` `0x588F20` hides a sprite whose draw key lies
  outside the ring's layers `0..0x36` (`sprite_screen.cpp:79`), and that sets
  the same bit 7 as section 2. A larger ring changes that test's range, and
  so the logic that reads the bit. RG keeps `0..0x36` for the bit, or
  ledgers the change.
- `AreaMapBD_BuildView` (area 189) walks its own fixed grid: 0x38 rows of
  0x1C cells, or 0x2C of 0x28 by the octant. That is the ring's size by
  another road, not the inset, and it is unmeasured at 120.
- The 23 sites of I37 are the PSP's list. In ours the ring's 28 / 56 also
  appear as `0x1C` / `0x1B` / `0x37` / `0x38` immediates
  (`map_layers.cpp:472..476`, `map_scroll.cpp`'s four shifts and its reset
  loop).

## 4. The draw-item pool at 120

DIV-0062 holds 2,048 items (2,047 usable), doubled from 1,024. Two sources of
evidence:

- **Logged peaks:** one, narrow: 855 of 1,023 in the coast pan
  (`textureglitch.txt`, DIV-0062). DIV-0062 also says the 16:9 cull "keeps
  about half as many cells again", and "near 1,070" with a 50 margin. **No
  16:9 peak was ever logged**, and nothing on disk logs the pool. The raw
  dumps hold `DrawItemPool_Top` `0x9039D4` (in use = Top - 1) at single
  ticks, all narrow (`uw_uc/pool_dumps.py`): 119 to 553 in use, 549 to 552
  in area 2 at `Camera_Distance` 1,516 to 1,543. Those are single frames, not
  peaks.
- **The geometry** (`uw_uc/pool_model.py`): the cells `MapView_Build` keeps
  from a full map (every cell with a tile), at each width's inset rule and
  terrain cull (`columns + 47` past the original's at 120, `[-217, 537]`).
  The absolute count depends on the map, but the ratio between widths
  depends only on the geometry:

  | Camera | narrow | 16:9 | 21:9 | 21:9 / narrow | 21:9 / 16:9 |
  |---|---|---|---|---|---|
  | distance 0, level / 256 below | 391 / 369 | 485 / 453 | 607 / 571 | 1.55 / 1.55 | 1.25 / 1.26 |
  | distance 1,543 | 561 / 538 | 695 / 666 | 873 / 838 | 1.56 | 1.26 |
  | distance 2,560 | 693 / 665 | 857 / 821 | 1,053 / 1,006 | 1.52 / 1.51 | 1.23 |
  | yaw 0x300 | 415 / 397 | 514 / 490 | 628 / 595 | 1.51 / 1.50 | 1.22 / 1.21 |
  | yaw 0x400 | 470 / 442 | 556 / 520 | 654 / 609 | 1.39 / 1.38 | 1.18 / 1.17 |

**Estimate.** 855 × 1.38 to 1.56 = **1,180 to 1,340** at 21:9. If
DIV-0062's 1.5 for 16:9 is the right figure rather than the model's 1.25,
the estimate is 855 × 1.5 × 1.26 = **about 1,620**. Both leave room in
2,047. The pool runs dry at 21:9 only in a scene whose narrow use is above
about 1,310 (2,047 / 1.56), half again the highest ever seen. The model
counts cells, not items: a cell with sides takes three, as it does narrow,
so the ratio carries over, but EB measures the actual peak.
`BOF3X_ORIGINAL=DrawPool` (1,024 items) runs dry at 21:9 for certain. What
UC2 or EB should log: `DrawItemPool_Top`'s high-water mark per route, and
`Gfx_PacketNext`'s against `kPoolLimit` (`draw_emit.cpp:15`), the 64 KB
packet buffer. The wider culls feed the packet buffer too (walls, uprights,
sprites), and nothing has logged it either.

## 5. The map's edge

**There is no camera clamp.** `AreaMap_Frame` `0x56E6C0`
(`map_layers.cpp:97..137`) steps `MapView_FocusX` / `_FocusZ` toward
`Field_Kind2X` / `_Kind2Z` by `MoveScript_F3Divisor` a frame, with no test
against the map's width or height. The target is set by the leader's moves
and by scenes (`event_leader.cpp:480`, the `Field_Kind2X` writers in
`area_*`, `boss_sg.cpp`, `boss_si.cpp`). `MapView_CellToMap` leaves a ring
slot empty for any map cell outside `1..w-2 x 1..h-2`, and an empty slot draws
nothing (`symbols.toml`, `sprite-draw-order.md`). So the edge of the map is
wherever the focus puts it, at 320 too, and what shows past it is whatever
the frame has behind the cells. The wider view shows more of it whenever the
focus is near an edge. No width-dependent clamp exists to adjust. The
"camera clamp" of [`display-overhaul.md`](display-overhaul.md) and IDEAS'
map-edge note was a guess that the code does not bear out.

**The areas narrower than the view.** For every area block (`DAT/AREA*.DAT`,
kind 0, tag `0xC8000`; `uw_uc/map_extent.py` on `tools/dat.py`): along each
map diagonal `x + z` (one screen-horizontal line at yaw 0x200), the span of
cells with a tile, in ring columns. That is compared with the view's widest
row at the field camera over level ground: 10.0 columns narrow, 13.4 at 16:9,
17.6 at 21:9.

| Class | Areas | Count |
|---|---|---|
| narrower than the 320 view on every row (void at the sides at every width) | 31, 149, 186, 198 | 4 |
| wider than 426 on some row, narrower than 560 on every row: **void in both 21:9 bands wherever the focus is** | 53, 90, 153, 154, 192, 194 (each 80 x 55, 396 to 480 cells, widest row 15 columns); 98 (100 x 80, 516 cells, 16) | 7 |
| wider than 560 on some row | the other 189 | 189 |

Areas 31 and 186 (10 x 241) are a corridor seen at another camera (area 31's
dumps: pitch +256, yaw 0, distance 256). At yaw 0 the screen's rows run along
map x, not the diagonal, so this metric does not describe them. What their
band shows is for the census. Which way the focus moves in the seven, and
whether the void there is black or a backdrop, is for BA's void channel and
the owner's eye. A map that is wider than the view still shows void at 21:9
whenever the focus comes within about 8.8 ring columns of its edge (5.0
narrow).

## 6. Pops at the edges that are not culls

Two sweep bounds (rows 14, 15) also gate the state that follows them. The
14 `g_slides` bounds and the gene-list slide-outs are UW's. A grep for screen
words compared with constants (`+0x2E` against literals) found the world-map
HUD's frame slides (`area_w0b` etc., `world_map.cpp:155..168`). Those are y
slides (`Area16_DrawFrame(0x10, y)`), so nothing changes for the width. The
staging classes proper (actors parked, entrances, spawn and despawn pops)
belong to SS and BA.

## 7. The table UC2 builds from

Each rule writes the original's value moved by the columns, so that at 0 it
is Capcom's and at 53 it is today's. "Fill" means `Widescreen_Fill()`, which
reads 0 during the fuzz and narrow.

| # | Site (file, function, address) | Rule | 0 / 53 / 120 after | Logic? | Who |
|---|---|---|---|---|---|
| 8 | `battle_e6.cpp`, `MapCell_DrawTexQuads` `0x4CEB40` | x bounds `-100 - Fill`, `420 + Fill`, compared in our code (the `.rdata` floats are Capcom's code's too: leave them) | `(-100, 420)` / `(-153, 473)` / `(-220, 540)` | no | UC2. **Changes 16:9's picture** (it pops only at 21:9): under the round's rule, either widen at 120 only, or take the owner's yes for 16:9 |
| 9 | `battle_e6.cpp`, `MapCell_DrawGroundSprite` `0x4CF270` | `-60 - Fill`, `380 + Fill` | `[-60, 380]` / `[-113, 433]` / `[-180, 500]` | no | UC2; 16:9 changes (it pops there now): the owner's yes |
| 10 | `effect_5b.cpp`, `EffectKind18Sub0D_DrawGlow` `0x4FF610` | `-60 - Fill`, `380 + Fill` | as row 9 | no | UC2; 16:9 changes: the owner's yes |
| 11 | `effect_5d.cpp`, `EffectKind18Sub1C_OnScreen` `0x505480` | x `-20 - Fill`, `340 + Fill`; **y unchanged** (the same two floats serve y: split them) | `[-20, 340]` / `[-73, 393]` / `[-140, 460]` | a sound | UC2 if the owner wants the band heard; 16:9 changes |
| 12 | `field_frame.cpp`, `Sprite_ProjectA` `0x57B860` | the draw-list test at `0x140 + m0 + Fill` / `-m1 - Fill`; **bit 7 at the table's margins** (section 2) | entry 0 `[-80, 400]` / `[-133, 453]` / `[-200, 520]`; entry 1 `[-100, 420]` / `[-153, 473]` / `[-220, 540]` | **yes** | the split is the owner's call; the draw half is UC2's |
| 13 | `magic_s28.cpp`, `BreathMote_Fly` `0x4DECA0` | x `0 - Fill < x < 320 + Fill` (both ends, strictly, as now) | `(0, 320)` / `(-53, 373)` / `(-120, 440)` | no | UC2; 16:9 changes |
| 14 | `effect_1a.cpp`, `EffectKind0D_Start` / `_SlideOut` | start `0x186 + Fill`, end `<= -0x46 - Fill` | 390 / 443 / 510; -70 / -123 / -190 | **yes** (when the record goes idle) | the owner: it changes how long the effect runs. A park-farther shape, not a cull |
| 15 | `area_w4b.cpp`, `Area172_EffectA5Grow` `0x428240` | end at `x > 0x190 + Fill` | 400 / 453 / 520 | **yes** (a script variable and the state) | the owner: it changes when the script goes on |
| 6 | `sprite_draw.cpp`, `Sprite_Draw` `0x5935B0` | **already moved**; split as row 12: draw at `+-Fill`, bit 7 at `[-64, 384]` | as today; bit 7 back to Capcom's | **yes** | the owner. It changes 16:9's state against `6b4c97cc` (back to the original's); otherwise ledger draft A |
| 16 | `inventory_ops.cpp`, `Encounter_OnScreen` | leave | - | yes | nobody, unless the owner asks |
| 1..5, 7 | the moved culls | nothing; UW's columns reach them | - | no | UW's property check |
| ring | `map_layers.cpp` `Inset()`, `Widescreen_InsetColumns` | UW's `ceil(columns / 20)` is enough at the field camera | inset 0 at 120 | no | UW |
| ring | RG (I37) | 32 columns covers every persistent distance our source writes; the PSP's 33 x 58 adds rows. Keep `Sprite_UpdateScreen`'s `0..0x36` for bit 7 | - | via bit 7 | RG, if the owner wants the far-camera scenes clean |

**Rows 8 to 13 under the round's rule.** "Narrow and 16:9 stay bit for bit"
means a draw-only cull that pops at 16:9 today stays popping at 16:9 unless
the owner says otherwise. UC2 can take two rules:

- `Fill` everywhere: 16:9 changes too, as the 2026-09-30 culls did.
- Width-gated, `columns > 53 ? Fill : 0`.

The first is the rule DIV-0041 has followed so far. The second is what the
round's rule asks for literally. This is a question for the owner (section 8).

## 8. What was not verified, and the owner's calls

**Not verified:**

- Nothing was run: no self-test was needed (no `src/` change), and no live
  run. Every pop above is inferred from bounds and geometry, none seen.
- The `.text` scans find compares against immediates and against `.rdata`
  / `.data` floats. A bound built in a register from arithmetic, or read
  through a pointer table, would be missed. The C++ scan finds literals in
  comparisons: a bound held in a named constant and compared later would
  only be found by its `.rdata` address (scan 2) or not at all.
- The bit-7 reader scan matches one load-and-test shape. `Scena08_Scene4`
  and `EffectKind32_Settle` were found by the source grep instead. Others
  may exist. Every reader found is ours.
- That `BossNina_WalkStep`'s sprite is culled by `Sprite_Draw` is inference
  from the draw pass, not a trace.
- Which objects are type 0x0A (row 12) in practice is not counted.
- The ring model assumes flat ground at one height per run. Walls hang 128
  below an edge (`MapCell_DrawWalls`), and real maps mix heights. The `h`
  columns bracket the case, and the model reproduces the 16:9 notch and its
  fix. Areas 11 and 189 (a camera inside the focus, and another projection
  distance) are not modelled.
- The pool estimate is a ratio applied to the only narrow peak ever logged.
  The 16:9 peak was never logged.
- The map-edge classes use the field camera at distance 0 and level ground.
  Area 31's camera is different (yaw 0).

**The owner's calls met:**

1. **The off-screen flag** (section 2): keep today's 16:9 behaviour and
   ledger it (draft A), or split the flag so that logic is Capcom's at every
   width (this changes 16:9's state back to the original's).
2. **Rows 8 to 13**: widen the draw-only culls that pop at 16:9 today at
   16:9 too (the DIV-0041 habit), or only at 120 (the round's literal rule).
3. **Rows 14 and 15**: the two sweeps that end a state at an edge. Moving
   them changes timing, so they are staging-class fixes, not culls.
4. **RG**: the ring is needed only for the far cameras (three areas at
   2,176..2,560, effect kind 0x8D) and off-axis low ground. Whether that is
   worth I37's half-week is the owner's call.
5. **The 7 areas** whose map is narrower than 560 on every row: EB's
   captures and BA's void numbers first, then leave, narrow-scene, or
   something else.

**From other groups:** UW's runtime columns in `Widescreen_Fill()` and
`Widescreen_InsetColumns()` (rows 1..7 depend on them). EB: the state hash at
`BOF3X_WIDE=1` and `=2` against the `orig` references (section 2 predicts a
difference wherever an actor or effect crosses `[-64, 384]`); the pool's and
the packet buffer's high-water marks; BA's void fraction for the seven areas
of section 5.

## 9. For the other files

**`docs/README.md`, one row:**

> | [`ultrawide-culls.md`](ultrawide-culls.md) | The ultrawide round's read of the culls and capacities at 120 columns (group UC, 2026-10-13): every screen-x bound in our source classed at 0 / 53 / 120 (7 already moved, 6 of Capcom's inside the 21:9 view and 4 of them inside 16:9, 2 sweeps that end a state, 2 that are not culls); the off-screen flag (`Sprite_Draw`'s bit 7) read by nine functions and moved by DIV-0041 at 16:9 since 2026-09-30; the cell ring reaching 560 at the field camera but not past `Camera_Distance` ~2,200 (32 columns cover what our code sets; the PSP's 33 x 58 nearly all); the pool's estimate (about 1,180 to 1,620 of 2,047); no camera clamp exists, and 7 areas are narrower than 560 on every row. The table UC2 and RG build from. MEASURED. |

**`docs/DIVERGENCE.md`, draft A: an amendment of DIV-0041 (if the owner
keeps today's behaviour; drop it if the flag is split):**

> **Amended (ultrawide round, group UC, [`ultrawide-culls.md`](ultrawide-culls.md) section 2): the sprite cull is game logic.** `Sprite_Draw` `0x5935B0` sets bit 7 of an object's byte 0 when it culls it, and nine functions read the bit: `Member_FollowStep` and `Member_CatchUp` (a follower off screen may jump or catch up), `Scena08_Scene4` (waits for an actor to leave), `BossNina_WalkStep` (walks until off screen), `EffectKind32_Arc` / `_Settle`, `EffectKind34_V4Move` and `Area65_Record8Move` (effects that hop, land or free themselves off screen), and `Fish_Swim` (a sound). Since the 2026-09-30 amendment that cull is `[-64, 384]` moved by the columns, so in the wide picture those decisions are taken at the wide edge: 53 px further out at 16:9, 120 at 21:9. The rationale's "a cull's margin changes what is drawn, not logic" does not hold for this one. Kept as the wide picture's behaviour by the owner's word of `<date>`: an actor the original stopped drawing at the old edge, and acted on, is acted on when it leaves the picture the player sees. **Verification owed:** the state hash at `BOF3X_WIDE=1` / `=2` against the narrow references, where any route that moves an actor or effect across `[-64, 384]` should differ.

(If the owner chooses the split instead, UC2's entry is an amendment of the
same DIV: "the off-screen flag keeps Capcom's bounds at every width, the
draw widens". It is then a change to 16:9's state, back to the original's.)

**Defects to number (the coordinator):**

- *16:9: the sprite cull's off-screen flag moved with the picture*
  (section 2). Since 2026-09-30, under `BOF3X_WIDE=1`, the nine readers
  decide at `[-117, 437]` where the original decides at `[-64, 384]`.
  Unledgered, unmeasured. Ours; not in the PSX (no wide view).
- *16:9: four Capcom culls inside the 16:9 view*: rows 9, 10, 11 and 13.
  A ground sprite, a glow and a breath mote stop being drawn at or near the
  old edge, and a column's sound plays only inside `[-20, 340]`. Unseen in
  play.
- *16:9: effect kind 0x8D's camera pull exceeds the cell ring*: 17.5 ring
  columns needed against 13.5 at its start. A notch at the edges for the
  pull's first frames, by the model. Unseen.

**`docs/IDEAS.md`, I37 (the ring grown), a note:**

> Measured for 21:9 by [`ultrawide-culls.md`](ultrawide-culls.md) section 3: the field camera (distance 0) needs no ring growth at 120 columns (UW's inset rule reaches inset 0); scenes at `Camera_Distance` above about 2,200 (three areas, effect kind 0x8D) and off-axis yaws over low ground do. 32 columns cover every persistent distance our source writes; the PSP's 33 x 58 covers all but kind 0x8D's first frames. `Sprite_UpdateScreen`'s layer test `0..0x36` sets the off-screen flag the logic reads: a grown ring keeps it, or ledgers it.

**`docs/STATUS.md` / `docs/HANDOFF.md`:** the owner's calls of section 8
(the flag split, the 16:9 rule for rows 8 to 13, the two sweeps, RG's need)
go in HANDOFF's open list. In STATUS, one line: "UC's read: the culls at 120
classed; the sprite cull's off-screen flag is logic and already moved at
16:9 (unledgered); the ring covers 21:9 at the field camera; pool estimate
1,180 to 1,620 of 2,047".

**`docs/display-overhaul.md` and `docs/IDEAS.md`'s map-edge note** say
"the camera clamp must change per map". There is no clamp (section 5). For
the coordinator: a one-line correction pointing here.

**`docs/psp-widescreen-placements.md` section 3** gives `[-40, 360]` to
`Encounter_OnScreen`. That range is PSX `0x80161E44`'s, the sound pan the PC
dropped (row 17). For the coordinator: correct the row.
