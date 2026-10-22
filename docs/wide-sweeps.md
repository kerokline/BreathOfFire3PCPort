# The two edge sweeps drawn mapped (wide-ring round, group SW; IDEAS I38)

**Status:** BUILT 2026-10-21 on `wide-ring/sw` (round doc
[`wide-ring-round.md`](wide-ring-round.md) 2, SW). The owner decided the shape
on 2026-10-15 ([`owner-review.md`](owner-review.md)): keep each sweep's logic x
and draw it mapped onto the wide picture, so it starts and ends past the wide
edge on the original's frames. Both sweeps captured at 4:3, 16:9 and 21:9
(kind 0xD from the `caughFish` route, area 172 from a chapter-jump save);
the state hash on the eleven routes keeps narrow's `Rand` at both widths
(section 4). **The owner's look, 2026-10-21 (chat), off the two sheets:** the
shade's right-anchored map (the "Fin" reveal frame-exact) and the banner's
faster slide in the bands (1.23x / 1.52x) both accepted; not yet seen in play.
**Folded 2026-10-21 (group END):** section 5 is DIV-0041's 2026-10-21 sweeps
amendment; IDEAS I38 marked built.

## 1. The two sweeps, read again

`c` is the columns a side: 53 at 16:9 (`BOF3X_WIDE=1`), 120 at 21:9 (`=2`),
0 at 4:3. Both sweeps are listed in [`wide-staging.md`](wide-staging.md) 8 and
[`ultrawide-culls.md`](ultrawide-culls.md) rows 14 and 15.

- **Effect kind 0xD** (`src/game/effect_1a.cpp`, `EffectKind0D_Start`
  `0x465F30`, `_SlideIn` `0x465FE0`, `_Hold` `0x466020`, `_SlideOut`
  `0x466050`). `_Start` puts the screen x `+0x2E` at 0x186 (390) and draws
  nothing. `_SlideIn` steps it by -30 and queues the record as an overlay
  (`Sprite_QueueOverlay` `0x5890E0`): drawn at 360, 330, ..., 180, then held
  at 160 (0xA0) for 30 frames by `_Hold`; `_SlideOut` steps on to 130 ... -50
  and, at -80 (`<= -0x46`), puts 0xA0 back and sets `+1 = 0` (idle) without
  drawing. The x reaches the screen through `+0x74`: `Sprite_DrawOverlays`
  (`0x592F20`, ours, `src/game/field_frame.cpp`) writes the float there from
  `+0x2E` just before `Sprite_Draw` (`0x5935B0`) reads it. `Sprite_Draw` culls
  and sets the off-screen bit 7 from `+0x2E` (the bit DIV-0041's UC2 split
  keeps at Capcom's bound). **What it is (seen, section 4):** the fishing
  minigame's *READY!* banner, sliding in from the right before a cast and out
  to the left. No code of ours spawns kind 0xD (it comes from data); symbols.toml
  had "no recorded route enters it", but the round-14-and-later routes do:
  `caughFish` slides it seven times, `masterAndManillo` five (the log lines of
  section 3). Its PSX twin hypothesis sits in the fishing spot's overlay
  (`AREA030.EMI#4`, "つりば みずうみ" in the sibling's `places.toml`).
- **Area 172's shade** (`src/game/area_w4b.cpp`, `Area172_EffectA5Grow`
  `0x428240`). Effect kind 0xA5 is spawned by **scenario chapter 15, run 5,
  step 0x16** (`Scena15_Run5`, `src/game/scena_sc15.cpp`): the bad ending.
  Step 0x13 sets `Draw_PassFlags = 0`, so the field is not drawn; step 0x14 a
  message; step 0x16 the effect. Each frame the effect draws the 64 x 32 panel
  (`Area172_DrawPanel`, at `PanelX()` = 0xDC + c since DIV-0041's WA
  amendment) and then the shade (`Area172_DrawShade` `0x428370`): a
  subtractive `POLY_G4` over rows 200..248 from `(x - 64, x)` (black to grey)
  to the right edge (white; 320 + c since DIV-0041). Its x grows from 0 by 10
  a frame; above 400 the step byte goes on (0x17 to 0x18, which waits for
  Start) and the effect stops drawing the shade. **Seen on the capture
  (section 4):** the screen is black and the panel is the word *Fin*; the
  shade is a wipe that reveals it left to right, so its only visible effect is
  over the panel.

## 2. The maps

Both are piecewise linear in the logic x, the identity when `c = 0`, rounded
to the nearest (`widescreen_rule::SweepLine`, `src/game/widescreen.h`).

| Sweep | Narrow x | Drawn x | Why |
|---|---|---|---|
| kind 0xD | `[160, 390]` | `[160, 390 + c]` | the owner's map: the hold fixed, the start pushed past the wide right edge |
| kind 0xD | `[-70, 160]` | `[-70 - c, 160]` | the end pushed past the wide left edge |
| shade | `[0, 220]` | `[-c, 220 + c]` | the start past the wide left edge as 0 is past the narrow one |
| shade | `220` on | `x + c` | anchored right with the panel (0xDC + c) and the shade's own right edge (320 + c): the panel's reveal on the original's frames, the bound 400 drawn at 400 + c |

At 16:9 the slide is drawn from 406 (narrow 360) to -98 (narrow -50); at 21:9
from 464 to -160 (each leg's slope is (230 + c) / 230, so the last drawn
frame, 210 left of the hold, lands 258 or 370 left of it). The shade at 16:9 starts at -38 (narrow 10) and ends at 463
(narrow 410); at 21:9 -99 and 530 (the run logs, section 4).

**The shade's map, and the brief's words.** The brief asks for "the same
kind of map past its start, so it reaches the wide edge when the logic's edge
reaches 400". A single stretch from the start (`[0, 400]` onto `[0, 400 + c]`)
or about the centre would also take 400 to 400 + c, but the panel is anchored
right (+c) and the shade only shows over it, so a stretch moves the reveal:
the edge crosses the panel's left edge at logic x 241 (stretch from 0) or 253
(about 160) at 16:9 instead of 220 - two or three frames late, more at 21:9.
The piecewise map keeps the reveal frame-exact (it is a plain shift by `c`
over the panel) and still starts the shade past the wide left edge. Owner
question 1.

## 3. What was built

- `src/game/widescreen.h` (the block after `Widescreen_BattleTile`):
  `widescreen_rule::SweepLine`, `SlideMap`, `ShadeMap` and their constants
  (`0xA0`, `0x186`, `-0x46`; `0`, `0xDC`, `0x190`), and the five entry points
  below.
- `src/game/widescreen.cpp`: `Widescreen_SweepMark`, `_SweepBefore`,
  `_SweepAfter`, `_SweepClear`, `_SweepShadeX`, `_CheckSweeps` (the section
  after `Widescreen_ArmFills`, which calls the check), compile-time
  `static_assert`s of both maps at 0, 53 and 120 beside the other rules'.
- Kind 0xD: `EffectKind0D_SlideIn`, `_Hold` and `_SlideOut` call
  `Widescreen_SweepMark(S())` before `Sprite_QueueOverlay`; the mark holds the
  record and the `+0x2E` it was queued with. `Sprite_DrawOverlays` calls
  `Widescreen_SweepBefore(sprite)` / `_SweepAfter(sprite)` around
  `g.sprite_draw()`: a marked record whose `+0x2E` is still the marked x gets
  the mapped float in `+0x74` for the draw and Capcom's float back after it.
  `Field_DrawFrame` calls `Widescreen_SweepClear()` where it empties the
  overlay list. `+0x2E`, the off-screen bit, the hold count and the idle frame
  are never touched; `+0x74` holds Capcom's value outside the one draw call.
- The shade: `Area172_EffectA5Grow` passes `Area172_DrawShade` the result of
  `Widescreen_SweepShadeX` of the zero-extended word it passed before (the same
  int back when off). The word and the `> 0x190` test are Capcom's.
- **Off** when `Widescreen_Fill()` is 0 - before `Widescreen_ArmFills`, so in
  every module's fuzz, and for good when narrow - and under
  `BOF3X_ORIGINAL=Widescreen` (the faster slide's switch byte). Kind 0xD's half
  is off also while `Sprite_DrawOverlays` or `Field_DrawFrame` is Capcom's
  (nothing would put `+0x74` back or empty the marks). The start-up line says
  which: `DIV-0041    the two sweeps drawn mapped: shade on, slide on; ...`.
- **The property check** (`Widescreen_CheckSweeps`, run by
  `Widescreen_ArmFills` whatever the width): at 0, 53 and 120 columns, over x
  -100..420 (slide) and -70..430 (shade), the identity at 0, the ends and
  fixed points where the rule puts them, never decreasing, the slide never
  drawn nearer the hold than Capcom's x, the shade `x + c` from 220 on; the
  shade's entry over every 7th x of -0x8000..0xFFFF as wired; no mark before
  play. 17,277 checks, Fatal on a difference.
- A log line for each sweep's first and last drawn frame (at most 32 a run),
  the evidence a run reached it.
- **The quicksave:** nothing to carry. A mark lives from a state's queue to
  the same frame's `Field_DrawFrame`; the shade's map is a function of the
  word alone.

## 4. Checks

All on `wide-ring/sw` at the code commit, built with llvm-mingw (`cmake
--build build`, no warning in a touched file). Harness root
`C:\Users\kerok\bof3x-runs\r_ring\SW\` (scripts `st.sh`, `chain_hash.py`,
`cmp.py`, `run_recipe.sh`, `craft.py`, `sheet.py`; logs, hash files, shots and
sheets beside them - game-derived, not in the tree).

- **`--selftest`, everything** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'
  BOF3X_LANG=original`, an ini-less launcher copy `L\`): at `BOF3X_WIDE`
  0, 1 and 2, exit 0; 1,088 / 1,090 / 1,090 lines with 0 mismatches and none
  with any other count, no `FATAL`; `inject: 10087 ours, 0 left original`.
  The touched modules' lines at all three: `effect_1a self-test: 210000
  rounds over 70 functions ..., 0 MISMATCHES`; `field_frame self-test: 24000
  rounds ..., 0 MISMATCHES`; `area_w4b` five lines, 0 MISMATCHES each
  (`Area172_EffectA5Grow` among the 7..22 functions). The map's check:
  `the two sweeps' map check: 17277, 0 differences at 0, 53 and 120 columns
  (the view is narrow: off)` narrow, and at 1 / 2 `the two sweeps drawn
  mapped: shade on, slide on; kind 0xD's slide [-70, 390] onto [-123, 443]` /
  `[-190, 510]`, `area 172's shade [0, 400] onto [-53, 453]` / `[-120, 520]`.
- **The state hash, eleven routes** (the ten `cn_` routes and `depress`; the
  `orig3` conditions: `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the skip list,
  ini-less launcher, speed 1, the shop with `--slot0-hold`), each at 0, 1
  and 2:
  - **`Rand` per frame: wide identical to narrow on every frame of every
    route at both widths** (combat 2,621 frames, caughFish 3,890,
    cutsceneAndNue 7,422, dragonTransform 4,330, field_menu 1,455,
    masterAndManillo 5,520, menu_screens 1,771, shop 3,158, whelpBoss 13,182,
    worldMapAndAreaTransition 2,144, depress 1,633).
  - **Narrow against the `orig3` pairs:** `check` exits 1 on all ten, as ours
    at main did on 2026-10-20. Against that day's classified runs
    (`cn_<route>_a1020`, HANDOFF 3: DIV-0092's music pages, the button
    round's glyphs into VRAM), the same pages on every route; the only extra
    pages are `0x7BB000..0x7BF000` (`Gfx_VramShadow`), `0x7DE000` (sound) and
    `0x937000` at tick 3 alone on six routes - the start-up noise class
    HANDOFF 3 names ("a few VRAM and sound pages at tick 3"). Narrow, the new
    code returns before touching anything (the map's switch is
    `Widescreen_Fill()`, 0 when narrow).
  - **Wide against narrow (`statehash.py diff`):** every differing page on
    eight routes is in the picture set census 3 listed (draw state, display,
    window x words, map view, sound, VRAM). Outside it: `0x7C9000`
    (`Font_TexCache`, `D3d_*`; display, 21:9 only, from tick 0), `0x801000`
    (`DrawTable`, whelpBoss) and `0x7E1000` (`Text_LineX` with
    `Effect_Objects`; cutsceneAndNue, menu_screens, whelpBoss) - on routes
    where neither sweep runs (no sweep log line, no kind-0xD mark), so not
    this change; `Text_LineX` is WA's message-pen anchor census 3 names. **On
    the two routes where the slide runs (caughFish, masterAndManillo) no page
    outside the picture set differs** - the effect records' `+0x74` is put
    back. One first run of field_menu at 16:9 differed on 386 pages from tick
    551: another session's run took slot 0 under it (its runlog: "the lock is
    pid 16024's now"); the re-run differs on 14 picture-set pages only.
- **Captures** (`BOF3X_FILTER=point`, frame-exact `recipe_shots.py` shots;
  sheets `sheet_slide0D.png`, `sheet_shade172.png` in the harness root):
  - Kind 0xD: `caughFish` frames 780..840 every 4. The banner enters at the
    right edge and leaves at the left edge of each picture on the same frames
    at all three widths (the first drawn frame 788, the hold 796..820, gone
    by 832).
  - Area 172: a chapter-jump save (`dolphinBoss.DAT`, block bytes `0F 00 00
    00 05 13`, chapter 15, state 0, run 5, step 0x13, area word 0xAC, the
    `+0x70` checksum recomputed; `craft.py`), so the run's own step 0x13 sets
    `Draw_PassFlags = 0`, the message at 0x14 is closed by a press and step
    0x16 spawns the effect. Frames 640..744 every 4: black, the *Fin* panel
    bottom right revealed by the shade's edge from frame 676 to 712, the same
    brightness on each frame at 4:3, 16:9 and 21:9.

## 4a. Not verified

The owner's eye on either sweep in play; a quicksave taken mid-sweep (nothing
to carry by design: a mark lives one frame); the slide under
`BOF3X_ORIGINAL=Sprite_DrawOverlays` (the start-up line then says the slide is
off; not run).

## 5. The DIV-0041 amendment (drafted; the orchestrator numbers and folds it)

> **Amended 2026-10-21 (the wide-ring round, group SW,
> [`wide-sweeps.md`](wide-sweeps.md)): the two edge sweeps drawn mapped
> (IDEAS I38).** Two effects sweep across the picture and end a state at a
> screen-x bound ([`ultrawide-culls.md`](ultrawide-culls.md) rows 14 and 15):
> effect kind 0xD's slide (`EffectKind0D_*` `0x465F30..0x466050`, in from x
> 390 by 30 a frame, held at 160 for 30 frames, out to -70 where the record
> idles) and area 172's shade (`Area172_EffectA5Grow` `0x428240`, the bad
> ending's wipe that reveals the *Fin* panel; its edge grows by 10 a frame and
> above 400 the scenario's step goes on). In the wide modes they started or
> vanished inside the bands; moving their bounds would change the idle frame
> and the script's timing. *Original behaviour:* each drawn at its logic x.
> *New behaviour:* the logic x, the idle frame and the step are Capcom's; only
> the drawn x is mapped, by `c` columns a side - the slide `[160, 390]` onto
> `[160, 390 + c]` and `[-70, 160]` onto `[-70 - c, 160]`, the shade
> `[0, 220]` onto `[-c, 220 + c]` and `x + c` from 220 on (anchored right with
> its panel, so the reveal is on the original's frames). The slide's mapped
> float sits in the record's `+0x74` only for its draw in
> `Sprite_DrawOverlays` (Capcom's put back after); its `+0x2E` and the
> off-screen bit are never touched. Off until `Widescreen_ArmFills`, narrow,
> under `BOF3X_ORIGINAL=Widescreen`, and for the slide while
> `Sprite_DrawOverlays` or `Field_DrawFrame` is Capcom's. *Why:* the owner,
> 2026-10-15: keep the logic's x and draw it mapped, so each sweep starts and
> ends past the wide edge on the original's frames. *Verification:* a start-up
> property check (`Widescreen_CheckSweeps`, 17,277 checks at 0, 53 and 120
> columns); `--selftest` at 0 mismatches narrow, 16:9 and 21:9, with the
> fuzzes of `effect_1a`, `area_w4b` and `field_frame` among them; the state
> hash on the eleven routes at 16:9 and 21:9 with narrow's `Rand` on every
> frame and no page outside the picture set differing where the slide runs
> (`caughFish`, `masterAndManillo`: kind 0xD is the fishing minigame's
> *READY!* banner); both sweeps captured at all three widths (area 172 from a
> chapter-jump save). **Owed:** the owner's eye in play.

## 6. Open questions for the owner

1. **The shade's map** (section 2): anchored right from the panel's edge,
   keeping the *Fin* reveal frame-exact (built), or a plain stretch from the
   start as the brief's words read (the reveal 2..3 frames late at 16:9).
   Recommendation: keep the built map - the shade is only visible over the
   panel, and the panel is already anchored right.
2. **The slide's speed in the bands.** Mapped about the hold, the banner
   moves (230 + c) / 230 times as far a frame: 1.23x at 16:9, 1.52x at 21:9,
   the same kind of faster slide DIV-0041's SL amendment gave the menu boxes.
   Recommendation: accept; the alternative (more frames) is the logic change
   the owner turned down on 2026-10-15.
