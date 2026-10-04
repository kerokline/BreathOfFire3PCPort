# Group R3F: the effect bands' leftovers between kinds 0x60 and 0xAB

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, from
the round branch's tip `7f116a2`. **50 functions ours** (`src/game/rest_3f.cpp`,
`rest_3f.h`, `rest_3f_callees.h`, shadow name `rest_3f`): the cut's 48 rows for
R3F (`analysis/round14_cut.tsv`, the band `0x480210..0x49259C`) and the two
starts in their spans no list had (`0x48F1E0`, `0x48F300`: kind 0x9E's states 2
and 4, band_rows' "code no list has"). None dropped: no start is a case or a
shared tail. Each read to its last instruction with capstone and fuzzed
through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8), used unchanged:
200,000 rounds, **0 mismatches**; STAR_RESULT. Controls: CONTROLS_RESULT
(section 5). **One divergence site**: `EffectKindAA_DrawFill` is a full-frame
fill and is drawn through DIV-0041's widened fill (section 2) - the
coordinator's amendment of DIV-0041. No new `.data` table: every state table
the functions sit in was named by round thirteen. **Fuzz only**: no recorded
route enters any of the 50 (section 9).

Every row is effect code. What round thirteen left here: the states of kinds
whose dispatchers E3A, E4E and E4F took, and the draws those states and their
neighbours call.

| Kind | Functions | Reached through |
|---|--:|---|
| 0x60: a grey line from (0x198000, 0x198000) whose ends slide down x to 0x98000 | 3 | `EffectKind60_States` `0x65492C` [0..2] (E3A's `EffectKind60_Run`) |
| 0x5F's disc (R3E's states call it): semi-transparent black LINE_F2, a row each, round a projected point | 1 | E8 from R3E's `0x47FE30`, `0x47FEE0`, `0x480010`, `0x4800E0`, `0x480190` |
| 0x69's lines (E3B's part 2): a chain of LINE_G2 down a wave | 1 | E8 from `EffectKind69_Part2` |
| 0x9C's trail (E4E's states 1..6): two quads and up to eight dots | 1 | E8 from six `EffectKind9C_*` states |
| 0x9E: a white box outline that grows from the record's point, a flash of the plate, a bouncing wall and a textured plate until the chapter's count, the plate, the outline shrinking | 7 | `EffectKind9E_States` `0x6551BC` [0..5] (E4E's `EffectKind9E_Run`) |
| 0xA1 and 0xA3: a sprite drawn into VRAM by a borrowed effect record, read back, every pixel not 0 a particle (TILE_1) that blinks and flies | 10 | `EffectKindA1_States` `0x655220` [0..4] = `EffectKindA3_States` `0x655248` [0..4]; 0xA3's own [5..9] |
| 0xA2: a ring of 32 POLY_G4 between two radii | 5 | `EffectKindA2_States` `0x655238` [0..3]; the ring by E8 |
| 0xA7: a glow that grows, holds and shrinks | 5 | `EffectKindA7_States` `0x655274` [0..3] (E4E's `EffectKindA7_Run` calls them, then the glow) |
| 0xA8: sixteen red bars across the screen | 7 | `EffectKindA8_States` `0x655284` [0..2]; the bar helpers by E8 (the draw also by E4F's `EffectKindB0_StepBars`) |
| 0xA9: a half disc from the screen's bottom centre | 5 | `EffectKindA9_States` `0x655290` [0..3] (E4E's `EffectKindA9_Run` calls them, then the disc) |
| 0xAA: a red gradient over the frame that fades in and out | 4 | `EffectKindAA_States` `0x6552A0` [0..2] (E4F's `EffectKindAA_Run`); the fill by E8 (also by R3G's `0x4925E0`, `0x492620`) |
| 0xAB's state 1: the drops while `Draw_PassFlags` is set | 1 | `EffectKindAB_States` `0x6552BC` [1] (E4F's) |

The names are from what the code does: "line", "disc", "trail", "outline",
"plate", "ring", "glow", "bars", "fill" name the primitives each commits and
the cells it steps - not a play-tested fact. What these effects are in the
game, and where they appear, is the owner's to say (section 9). **The
spawners in our source**: E4E's doc found chapter 15's runs spawning kinds 0x9C,
0x9E (`Scena15_SpawnMarker`), 0xA0, 0xA8 and 0xA9; kinds 0x60, 0xA1..0xA3, 0xA7
and 0xAA have none known. **PSX twins** (`analysis/pairs_propagated.json`):
`0x480300` = `0x801F9F8C`, `0x4837B0` = `0x801D218C` (call-disputed),
`0x48ED80` = `0x801D1C1C`, `0x48F3D0` = `0x801F9020`, `0x490A80` =
`0x801D21B4`, each paired by its callers; none has a name in the sibling's
`names/*.toml` or `symbols.toml` (a grep of each address), so every name is
this group's.

## 1. What each function does

All are cdecl; the states are `void (void)` on `Sprite_Current` (an
`Effect_Objects` record); "+n" is a byte of the record unless a word or dword
is said. Every draw that opens with a draw mode calls `Gpu_GetTPage(0, abr, x,
y)` / `Gpu_SetDrawMode(cursor, 0, dtd, page, 0)` (the fifth word the leftover
of GetTPage's five pushes), and projects through EGT's `EffectGte_*` after
`EffectGte_LoadMapCamera`.

### 1.1 Kind 0x60 (E3A's dispatcher)

| PC | Name | What |
|---|---|---|
| `0x480210` | `EffectKind60_Start` (0) | the point `+0x34..` (0x198000, 0x198000, 0x4000000); the other end `+0xC..` the point plus 0x40000 in x; `+1` up |
| `0x480270` | `EffectKind60_SlideBoth` (1) | both ends' x down 0x20000; `EffectKind60_DrawLine(+0x34, +0xC)`; `+1` up once `+0x34` is 0x98000 |
| `0x4802C0` | `EffectKind60_SlideEnd` (2) | the other end's x down 0x20000; the line; `+1` up once `+0xC` is 0x98000 |

### 1.2 The draws other kinds call

- `EffectKind5F_DrawLineDisc` `0x480300` (`point`, unused, `wobble`, `dy`): a
  draw mode (page (0, 0, 0x3C0, 0x100), dtd 0) linked 0xC at the point (dy the
  fourth word, a byte to the callee); `o` the point projected; `r`
  `EffectGte_ProjectSize(point, {0x50, 0x50})`'s first word; an angle `Rand &
  0xFFF`. For each row `i` from `-r` to `r` (s16): `s = sqrt(r*r - i*i)`
  (Capcom's `0x5A7A90`, `fild; fsqrt; _ftol`), a semi-transparent black
  LINE_F2 from `sinA + b + o.x - s` to `sinB + s + b + o.x` at `y = o.y + i`,
  both at o's depth - `sinA`, `sinB` `(Math_Sin(angle) << 4) sar 12` (called
  twice), `b` the wobble's low word, negated every row; linked 0x20; the angle
  up `Rand & 0xFF`. **The second argument is not read** (R3E's callers push
  0x20).
- `EffectKind69_DrawLines` `0x4837B0` (void): in the relocated PSX scratchpad
  (`DamageScratch` +0, +4, +8) and `Scratch_Swap` the amplitude 0x40, the angle
  `((+0xB + 2) & 0xF) << 8`, a shade `Rand & 0x3F`, the count `+0xA`;
  `Prim_VertexScratch` `(0, Math_Sin(angle) * amplitude sar 12, 0)`. For `i`
  from 1 below the count (`Scratch_Swap` read again after every line): a draw
  mode (page 0xB5, dtd 1) committed 0xC to slot 2; a semi-transparent LINE_G2
  from the vertex projected (`Gte_RotTransPers`, `Gte_StoreDepthF`); the
  amplitude `0x80 +/- (Rand & 0x3F)` (`Rand & 1` chooses); the vertex moved to
  `(0, Math_Sin(((+0xB + i + 2) & 0xF) << 8) * amplitude sar 12, -i << 6)` and
  projected for the other end; shades `(shade, shade, 0x20)` and `(next, next,
  0x20)`, `next = Rand & 0x3F` kept; committed 0x24 to slot 2.
- `EffectKind9C_DrawTrail` `0x48ED80` (`from`, `to`, `shade`): a draw mode
  (page (0, 1, 0x2C0, 0x100), dtd 1) committed 0xC to slot 2; two
  semi-transparent POLY_G4 between `from` projected (a) and `to` projected (b),
  each committed 0x44 to slot 2 - `(a + (hx, hy), a, b - (hx, hy), b)` shaded
  `(black, (s, s, 0), black, (s, s, 0))` and `(a, a + (hx, hy), b, b - (hx,
  hy))` shaded `((0, s, 0), black, (0, s, 0), black)`, `hx` / `hy` the floats
  `0x5C41B8` / `0x5C41C0`, every vertex at its end's depth; then up to eight
  TILE_1 dots `(0, s, 0)` from `from + (Frame_Counter & 7) * d` stepping `8 d`,
  `d = (to - from) sar 6`, while the dot's y is not below `to`'s, each at its
  projection less `hx` in y (hx, not hy: as read), committed 0x14 to slot 2.

### 1.3 Kind 0x9E (E4E's dispatcher and plates)

The record's point `+0x34..`, a half-extent `+0xC..` and its speed `+0x18..`
(three longs each); the wall vector `0x6762A0` (three longs, nothing else in
the image reads or writes it - a raw scan).

| PC | Name | What |
|---|---|---|
| `0x48F090` | `EffectKind9E_Start` (0) | the extent 0; the speed (0x3000, 0, 0x200000), or (0, 0x3000, 0x200000) when `+6` bit 0; `+6` bit 3: the extent eight steps on and `+1` up three (to state 4), else `+9` = 8, `+1` up; unless `+0xB` bit 0, sound 0x207 (0x202 when `+6` bit 2) |
| `0x48F170` | `EffectKind9E_Grow` (1) | the extent up by the speed; `EffectKind9E_DrawOutline(+0x34, +0xC)`; `+9` down, at 0 `+1` up |
| `0x48F1E0` | `EffectKind9E_Flash` (2) | the outline and `EffectKind9E_DrawPlate`; the wall vector = -extent; `+9` = 0x80, `+1` up |
| `0x48F240` | `EffectKind9E_Hold` (3) | the wall vector's third long up `+0x20 sar 1`, negated when it equals `+0x14`; odd frames `EffectKind9E_DrawShadePlate`, then `_DrawTexPlate` with the point 0x4000 further along x (y when `+6` bit 0) and put back; even frames `_DrawWall(+0x34, 0x6762A0)`; `+1` up once the chapter's count `0x903848` equals `+7` |
| `0x48F300` | `EffectKind9E_Close` (4) | the plate; `+9` = 8, `+1` up; unless `+0xB` bit 1, sound 0x208 (0x203 when `+6` bit 2) |
| `0x48F360` | `EffectKind9E_Shrink` (5) | the extent down by the speed; the outline; `+9` down, at 0 `+1` up (to `Effect_StateRelease`) |
| `0x48F3D0` | `EffectKind9E_DrawOutline` (`a`, `b`) | four corners projected - `a + b`, a crossed corner `(a.x - b.x, a.y + b.y, a.z + b.z)` or `(a.x + b.x, a.y - b.y, a.z + b.z)` by `+6` bit 0, the other crossed corner at `a.z - b.z`, and `a - b` (`Sprite_Current` read after each projection); an opaque white LINE_F4 round the four linked 0x38 at `(a.x, a.y)`, and an opaque white LINE_F2 across linked 0x20 |

### 1.4 Kinds 0xA1 and 0xA3: a sprite broken into pixels

`EffectKindA1_States` and `EffectKindA3_States` share their first five
entries (kind 0xA1's states); kind 0xA3 has five more of its own. The record's
dword `+0x4C` points at a sprite record (`Sprite_Objects`). The cells
`0x6769B0` (the pixel cursor), `0x6769B4` (the particle cursor), `0x6769B8`
(the particle count, a word) and `0x6769BA` (the capture record, a byte) and the
particles at `0x931980` (0x14 each: `+0` shown this frame, `+1` a delay, `+2`
the 15-bit pixel, `+4` / `+8` x / y floats, `+0xC` the depth, `+0x10` /
`+0x11` the s8 step) are read and written by these ten functions only (a raw
scan of the image). The capture records `0x655208` (three of 8: s16 dx, dy,
width, height) are read in place; **both captures write record 1**, so records
0 and 2 are never used. The read-back buffer is `EffectKind30_Shards`.

| PC | Name | What |
|---|---|---|
| `0x490A80` | `EffectKindA1_CaptureSprite` (0) | `Effect_FindFree` (none: `+1` up only); the capture record 1; `Gfx_ClearRect(0x340, 0x100, 0x80, 0x100)`; the sprite's first 0x80 bytes copied into the free record (`rep movsd`), made kind 0x3A in state 2 with the capture record's dx / dy at `+0x2E` / `+0x30`; when its `+0x24` bit 0: the page `Gpu_GetTPage(0, 0, 0x340, 0x100)` at `+0x25`, `+0x26` 0x80; `+0x24 \|= 0x88`; with it current, when bit 0: `Sprite_SetAnimationAt(0x4D, 0x1C)` and `+0x2A` 0 (Sprite_Current read after) when its word `+0x2C` is 0, else `(0x50, 0x24)`; `Sprite_UpdateScreen`, `Effect_Release`; Sprite_Current put back, `+9` = 2, `+1` up |
| `0x490BB0` | `EffectKindA1_ReadBack` (1) | `+9` down; at 0 the rectangle (0x340, 0x100, width, height) read back by Capcom's `0x59E930` into `EffectKind30_Shards`, the cursors at it and at `0x931980`, the count 0, `+9` 0, `+1` up |
| `0x490C50` | `EffectKindA1_SplitPixels` (2) | the sprite's point projected; the rows `+9 * h / 2 .. (+9 + 1) * h / 2` walked by the pixel cursor, `width` pixels a row, every pixel not 0 a particle at the particle cursor: its pixel, `x = o.x - dx + column`, `y = o.y - dy + row`, the depth copied (`fld` / `fstp`), the count up; `+9` up, at 2 `+1` up (so two frames split the picture) |
| `0x490E20` | `EffectKindA1_AimPixels` (3) | every particle: `+0` from an order of eight 0s and eight 1s shuffled (sixteen swaps by two `Rand & 0xF`) every sixteenth; the step - `+0xB` 0: two of `(Rand & 0x1F) - 0x10`; else two of `Rand & 0xF`, the larger first, the second negated; then `+6` 0: a delay `d = (Rand & 0x1E) + 0xA` and the particle d steps back, else the delay `0x28 - (Rand & 0x1E)`; `+9` = 0x50, `+1` up, sound 0x20C |
| `0x490FA0` | `EffectKindA1_MovePixels` (4) | the sprite's byte `+0 \|= 0x40`; every particle shown this frame - `+6` 0: one step while its delay runs (the delay counts down); else it waits its delay out and then steps every frame - drawn as a TILE_1 at its x, y, depth in `(p << 3, (p >> 2) & 0xF8, (p >> 7) & 0xF8)` of the 15-bit pixel, committed 0x14 to slot 2; every particle's `+0` flipped (a blink); `+9` down, at 0 `+1` up (to `Effect_StateRelease`) |
| `0x491310` | `EffectKindA3_CaptureSprite` (5) | nothing until the chapter's count is 0x29; then the capture, the page always set and `Sprite_SetAnimation(7)` |
| `0x491410` | `EffectKindA3_ReadBack` (6) | `EffectKindA1_ReadBack`'s code, instruction for instruction |
| `0x4914B0` | `EffectKindA3_SplitPixels` (7) | `EffectKindA1_SplitPixels`' code, instruction for instruction |
| `0x491680` | `EffectKindA3_AimPixels` (8) | every particle the blink and the ordered step, a delay `d = (Rand & 0x1E) + 0xA` and the particle d steps **on** |
| `0x4917C0` | `EffectKindA3_MovePixels` (9) | the sprite's byte `+0 \|= 0x40`; every particle shown steps **back** while its delay runs, drawn; the blinks flipped; `+9` down, at 0 the count `0x903848` = 0x2B and `+1` up |

So kind 0xA1 (with `+6` 0) gathers the particles onto the sprite's picture and
kind 0xA3 scatters it and brings it back; with `+6` set kind 0xA1's particles
wait and then fly off for good. That reading is from the arithmetic only.

### 1.5 Kind 0xA2: a ring

| PC | Name | What |
|---|---|---|
| `0x4910F0` | `EffectKindA2_Start` (0) | the radii `+0xC`, `+0x10` and their speeds `+0x18`, `+0x1C` 0; `+9` = 0x14, `+1` up; sound 0x208 |
| `0x491140` | `EffectKindA2_Grow` (1) | the speeds up 0x200 / 0x100, the radii by them; `EffectKindA2_DrawRing(+0x34, +0xC, +0x10)`; `+9` down, at 0 `Effect_Release` (a tail `jmp`) |
| `0x4911C0` | `EffectKindA2_Rewind` (2) | the radii and speeds 0, twenty steps of 0x100 / 0x200 with the radii by them, the radii negated; `+9` = 0x14, `+1` up; sound 0x208 |
| `0x491270` | `EffectKindA2_Shrink` (3) | the ring; the radii up by the speeds, the speeds down 0x100 / 0x200; `+9` down, at 0 `Effect_Release` |
| `0x4918B0` | `EffectKindA2_DrawRing` (`point`, `inner`, `outer`) | a draw mode (page (0, 1, 0x2C0, 0x100), dtd 0) committed 0xC to slot 2; a ring of 32 semi-transparent POLY_G4 round the point in x and y, the corners `point + ((cos, sin) * radius sar 8)` for angles 0x80 by 0x80 (the first edge `point.x + (radius << 4)`), every corner at the point's z, projected; inner corners (0xFF, 0x7F, 0), outer black; each committed 0x44 to slot 2. `al` 0 (no caller reads it) |

### 1.6 Kinds 0xA7, 0xA8, 0xA9, 0xAA and 0xAB's state 1

| PC | Name | What |
|---|---|---|
| `0x491AE0` | `EffectKindA7_Start` (0) | the size `+0xC` 0, `+9` = 0x10, `+1` up |
| `0x491B00` | `EffectKindA7_Grow` (1) | the size up 0x10; `+9` down, at 0 `+9` = 0x20, `+1` up |
| `0x491B40` | `EffectKindA7_Hold` (2) | `+9` down, at 0 `+9` = 0x10, `+1` up |
| `0x491B70` | `EffectKindA7_Shrink` (3) | the size down 0x10; `+9` down, at 0 `Effect_Release` |
| `0x491E30` | `EffectKindA7_DrawGlow` (`point`, `size`, `colour`) | `EffectKindA0_DrawGlow`'s code (compared instruction by instruction: three immediates differ) with the draw mode and the 32 POLY_G3 committed to **slot 7** and the POLY_G3 **opaque** (`SetSemiTrans 0`) |
| `0x491BC0` | `EffectKindA8_Start` (0) | the word `+0x2E` = 0x12C (no state of the kind reads it), `+9` = 0; `EffectKindA8_ClearBars`; `+1` up |
| `0x491BF0` | `EffectKindA8_Bars` (1) | `+9` 0: `EffectKindA8_StartBar` and `+9` = `(Rand & 0xF) + 0x18`; else `+9` down; `EffectKindA8_StepBars(0xF0)`; `+1` up once the count is 0xE |
| `0x491C40` | `EffectKindA8_End` (2) | `EffectKindA8_StepBars(0xF0)`; none in use (`al` 0): `Effect_Release` (a tail `jmp`) |
| `0x491FF0` | `EffectKindA8_ClearBars` | the sixteen bars (6 bytes each at `EffectKind30_Shards`) out of use |
| `0x492010` | `EffectKindA8_StartBar` | the first bar out of use put in use (`+0` 1, `+2` 0) |
| `0x492030` | `EffectKindA8_StepBars` (`length`, its word read) | a draw mode (page (0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7; `EffectGte_LoadMapCamera`; each bar in use by its step `+2`: 0 - `+1` = `+3` = 4, `+4` = length, `+2` = 1; 1 - `+2` = 2; 2 - `+4` down 8, below 0 the bar out of use; drawn unless `+2` is 0. `al` 1 when any was in use |
| `0x4920F0` | `EffectKindA8_DrawBar` (`bar`) | two opaque POLY_G4 **across the screen, x 0..320** (`0x43A00000`): from `y = +4 - +3 - (Frame_Counter & 1)` black to `y = +4` red (0x80, 0, 0), then back to black at `+4 + (Frame_Counter & 1) + +3`; each committed 0x44 to slot 7 (the depths `Gpu_SetPolyG4`'s 0.01). E4F's `EffectKindB0_StepBars` calls it too |
| `0x491CA0` | `EffectKindA9_Start` (0) | the radii words `+0x2E`, `+0x30` 0, `+9` = 0xA, `+1` up |
| `0x491CD0` | `EffectKindA9_Grow` (1) | the radii up 0x15 / 9; `+9` down, at 0 `+9` = 0x5A, `+1` up |
| `0x491D10` | `EffectKindA9_Wait` (2) | once the count is 9: `+9` = 0x1E, `+1` up |
| `0x491D30` | `EffectKindA9_Shrink` (3) | the radii down 7 / 3; `+9` down, at 0 `Effect_Release` |
| `0x492260` | `EffectKindA9_DrawDisc` (`rx`, `ry`, `shade`) | a draw mode (page (0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7; 32 semi-transparent POLY_G3 fanned from the screen point (160, 240), the rim from the angle 0x800 by 0x40 to 0x1000 at `(160 + f + cos * rx sar 12, 240 + f + sin * ry sar 12)` (each a word; `f = (Frame_Counter & 1) << 2`), centre `(shade, shade >> 2, shade >> 2)`, rim black; each committed 0x34 to slot 7. Screen-centred: nothing for the wide picture |
| `0x491D90` | `EffectKindAA_Start` (0) | `+9` = 0x10, `+1` up |
| `0x491DB0` | `EffectKindAA_FadeIn` (1) | `EffectKindAA_DrawFill(+9 * 0xF0)` (the product's low byte: 0 at 0x10 up to 0xF0 at 1); `+9` down, at 0 `+9` = 0x10, `+1` up |
| `0x491DF0` | `EffectKindAA_FadeOut` (2) | `EffectKindAA_DrawFill` in 0xFF when `+9` is 0x10, else `+9 << 4`; `+9` down, at 0 `Effect_Release` |
| `0x492400` | `EffectKindAA_DrawFill` (`shade`, a byte) | a draw mode (page (0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7; two opaque POLY_G4 **over the whole frame** - rows 0..120 black to `(shade, 0, 0)`, rows 120..240 back to black; each committed 0x44 to slot 7. Section 2 |
| `0x492580` | `EffectKindAB_Drops` (1) | `Draw_PassFlags` 0: `+1` up; else `EffectKindAB_Emit` and a tail `jmp` to `EffectKindAB_MoveDrops` (E4F's) |

## 2. Divergence

**`EffectKindAA_DrawFill` (`0x492400`)** builds two POLY_G4 that cover `(0, 0)`
320 x 240 from the floats 320.0 / 120.0 / 240.0 - a full-frame fill of
DIV-0041 section 3c's class. As the addendum asks, ours draws its x from
`Widescreen_FillX()` to `320 + Widescreen_Fill()` (E5G's
`EffectKind18Sub36_Pulse` form): the original's 0 .. 320 when the picture is
narrow or the fills unarmed (every self-test runs before `Widescreen_ArmFills`,
so the fuzz compares the original's 320 x 240), (-53, 0) .. (373, 240) under
the wide picture. **The coordinator amends DIV-0041** (its site table:
`0x492450` - the 320.0 `mov ebp, 0x43A00000` - in `0x492400`, kind 0xAA's unit, a red vignette over the frame); no
ledger entry is added here. **Left at 320**: `EffectKindA8_DrawBar`'s bars are
full-width bands, not full-frame fills; under the wide picture they stop at the
old edges. Whether they should widen is the coordinator's and the owner's call
(the brief's rule names full-frame fills only).

Nothing else: `DIVERGENCE.md`, `cheats.cpp`, `widescreen.cpp`, `labels.cpp`
and `yes_no_layout.cpp` patch no byte inside the 50 (a scan of every six-digit
address in them against the extents).

## 3. Tables and cells

**No `.data` table named**: the states sit in tables round thirteen named
(`EffectKind60_States`, `EffectKind9E_States`, `EffectKindA1_States` ..
`EffectKindAB_States`; each count checked against what the states store into
`+1`), and no function of this group dispatches through a table. The data read
in place (raw in `rest_3f_callees.h`, not named in `symbols.toml`, their bytes
not copied here): the capture records `0x655208` (three of 8, room to
`EffectKindA1_States`; ours aborts past three), the trail's half-widths
`0x5C41B8` / `0x5C41C0` (.rdata floats). **Cells**: the wall vector `0x6762A0`
(12 bytes), the pixel cells `0x6769B0..0x6769BB`, the particles `0x931980..`,
the chapter's count `0x903848`, `DamageScratch` +0 / +4 / +8 and
`Scratch_Swap` (kind 0x69's lines), the bars at `EffectKind30_Shards`.

## 4. The fuzz (`rest_3f_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=rest_3f`, effect mode (`g.effect`; each clone its
own kind - 0x5F, 0x60, 0x69, 0x9C, 0x9E, 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9, 0xAA,
0xAB), 4,000 rounds a function (`BOF3X_R3F_ONLY=<name>` runs the clones whose
name holds it, `BOF3X_R3F_ROUNDS` the rounds). Shapes: 40 `kEffect` (the
states and the two bar helpers without arguments), 10 `kCall` (the draws with
arguments, `Args` handing them the record's own points as the callers do,
shades, sizes and lengths over random upper bytes); `EffectKindA2_DrawRing`
and `EffectKindA8_StepBars` answer in `al` (`ret_mask 0xFF`).

**Regions** beyond the effect standard ones: the wall vector `0x6762A0`
(0x10), the pixel cells `0x6769B0` (0xC), the read-back buffer from the end
of the standard `EffectKind30_Shards` region to `0x92EF80` (the largest
capture record's half window, 0x2D00 bytes, from a cursor up to 0x300 bytes
in), and the first 64 particles at `0x931980`.

**Seeds** (`Seed`): on all 20 records `+0x4C` a sprite record (the
disturbance may make any current before the capture, the split or the move
reads it) and `+9` at the countdowns' edges - 0..3 for the two splits, whose row
loop needs `+9 * h / 2 + h / 2` below 256 (section 7); the first four sprites'
`+0x24` bit 0 and word `+0x2C` 0 half the time each; `+6`, `+0xB` at the bits
the states test; the chapter's count 0x29, 0xE, 9, 0x2B, 0x28 or any. Per
function: kind 0x60's ends one step from 0x98000, at it and either side; kind
0x69's count `+0xA` 0..0x1F; the trail's far end below its near one half the
time (so the dots run); the wall vector's third long landing on `+0x14` half
the time and `+7` the count half the time; the captures' count 0x29; the
read-backs' and splits' capture record 0..2; **the split**: the pixel cursor up
to 0x300 bytes into the buffer, its largest window (0x2D00 bytes) zeroed and up
to 24 pixels planted, the particle cursor so that every particle made stays in
the 64; the aims' and moves' particle count 0..64; kind 0xA8's sixteen bars
half in use, steps 0..3 and any, positions at 0, 7..9, 0xF0, the sign edge;
`Draw_PassFlags` 0 half the time.

**Group disturbance** (`Disturb`, from the hash only): `+9` (0..3 under a
split), `+6`, `+0xB`, the chapter's count, `Draw_PassFlags`, the particle
count (under the aims and moves, inside 64), the capture record (0..2).

**Callees listed** beyond the harness's standard and effect-standard rows: the
group's own called by name - `EffectKindA8_ClearBars` / `_StartBar` `kPhase`,
`EffectKind9E_DrawOutline` and `EffectKindA2_DrawRing` with the record's
points logged and hashed (12 bytes), `EffectKindA8_StepBars` its word and
`kFlag`, `EffectKindA8_DrawBar` the bar (6 bytes), `EffectKindAA_DrawFill` its
byte (`FadeIn` pushes `eax` with `Sprite_Current`'s upper half and the
product's high byte above the shade); merged groups' draws by name -
`EffectKind60_DrawLine` (E3A), the four `EffectKind9E_Draw*` (E4E; the wall's
second point the cell `0x6762A0`, in this group's regions),
`EffectKindAB_Emit` / `_MoveDrops` (E4F) `kPhase`. **Re-listed**:
`Gfx_CommitPrim` and `MapView_LinkPrimAt` stop the packet cursor 0x90 short of
the buffer's end (E4E's form: the POLY_G4s write 0x44 bytes past the cursor,
past the harness's 0x40); `EffectGte_ProjectSize`'s radius bounded - 0, 1, 0x3F,
0x50, -1, -0x40, -0x8000, 0x10, 0x20 or below 0x60 - never a large positive
word, which would run `EffectKind5F_DrawLineDisc`'s rows by the tens of
thousands (the standard row answers any). **Not re-listed**: `Rand` (the
harness's), the square root `0x5A7A90` and the read-back `0x59E930` (the
effect-standard rows: the root's answer any int, the read-back hashing the
rectangle and filling four bytes of the buffer), `EffectGte_ProjectPoint`
(fractional floats), `Math_Cos` / `Math_Sin`.

**The result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_3f`,
exit 0): **200,000 rounds over 50 functions, 6,509,818 calls to the
stand-ins, 0 mismatches**; 36,748 bytes of state in 49 regions; 369
stand-ins. Every callee reached (coverage line): `EffectKind9E_DrawOutline`
12,000, `EffectKindA2_DrawRing` 8,000, `EffectKindA8_DrawBar` 32,061,
`EffectKind9E_DrawTexPlate` 2,061 / `_DrawWall` 1,939 (the frame's parity),
`Effect_FindFree` 6,319, `Sprite_SetAnimation` 1,707 / `_SetAnimationAt`
1,548, `0x59E930` 1,716, `0x5A7A90` 277,430, `Rand` 1,314,419. The first run
passed; the controls (section 5) are what show the fuzz sees each behaviour.

STAR_SECTION

## 5. Controls

CONTROLS_SECTION

## 6. What the cut and the tool said, settled

- **Extents**: `band_rows.py` (through the round's scratch wrapper
  `band14.py`: the tool itself stops at "settle: no fixpoint in 8 rounds") read
  the 50 at 9,255 bytes against the cut's 9,735: 32 differ by padding only; two
  by code - the cut's `0x48F170` (208) and `0x48F240` (288) ran over the starts
  `0x48F1E0` and `0x48F300`, which are `EffectKind9E_States[2]` and `[4]`, each
  its own function with its own `ret` (added here). Every extent checked by
  hand to its `ret` or tail `jmp`. No shared tail, no case, no second entry.
- **Hidden starts**: 36 in the cut, each an entry by address (a cell of a
  state table), not a case. Their recorded hosts: R3E's `0x47FBE0` (kind 0x60's
  three), this group's `0x48ED80` (kind 0x9E's four), E4E's
  `EffectKindA0_SwapLong` `0x490A30` (kinds 0xA1..0xA3's fourteen - its own
  extent is 0x23 bytes and ends at `0x490A52`, so ours contains none of them),
  this group's `0x4918B0` (kinds 0xA7..0xAA's fifteen) and `0x492400` (kind
  0xAB's state 1). None of the hosts contains another's code as a fall-through.
- **Same code twice**: `0x491410` / `0x490BB0` and `0x4914B0` / `0x490C50` are
  byte for byte the same code (compared instruction by instruction); each is
  its own entry of its own table, so each is taken as a function and ours
  shares the body (`ReadBack`, `SplitPixels`). `0x491E30` is
  `EffectKindA0_DrawGlow`'s code with three immediates changed (slot 7 twice,
  `SetSemiTrans 0`).
- **The cut's columns**: the unit hints (the tables) are right; the "Scenario
  effects (SCE1xEF overlays)" / "Scenario event banks (SCENA overlays)" labels
  of the eleven `pair` / `neighbour` rows are the catalog's PSX-overlay
  pairing, not what the PC code is: every one is effect-engine code (a draw a
  state calls, or a state).
- **The harness's rows** (`scenario_harness.cpp` `kEffectStd`, `FX_RAW`, keyed
  by address): `0x48ED80` (3 words, the points 12 read, a byte) - matches what
  `EffectKind9C_DrawTrail` reads; `0x4837B0` (no argument) - matches, though the
  row does not log the record `EffectKind69_DrawLines` reads (`+0xA`, `+0xB`);
  `0x491E30` (`{kAll, kAll, kU8}`) - the size is read as a word, so the row
  compares more than the callee reads (E4E re-listed it with the word); 
  `0x492260` (`{kU16, kU16, kU8}`) and `0x4920F0` (a record read to +6) - match.
  Keyed by address, every row still serves the raw callers after this merges
  (the brief's addendum); none lists a function of this group as Capcom's in a
  way that stops the self-test, so no row was flipped.

## 7. Latent defects (Capcom's, described, not fixed)

- **The particle pool has no bound** (kinds 0xA1 / 0xA3). The split makes a
  particle of 0x14 bytes for every pixel not 0 of the captured rectangle, from
  `0x931980` on, with nothing counting room: capture record 1 is 0x60 x 0x78,
  so up to 11,520 particles (0x38400 bytes) could be made, while
  `Sprite_Current` (`0x937F88`) lies 1,306 particles in and `.data` ends 2,427
  particles in. A sprite with more than 1,306 pixels not 0 in that rectangle
  overwrites `Sprite_Current` and what lies before it. How many pixels a real
  sprite gives is not measured (no route reaches these kinds); ours writes
  where the original writes.
- **The split's row loop can run forever**: the row is a byte, the bound
  `(h / 2) * (+9 + 1)` an int, so when the bound exceeds 255 the row wraps
  before reaching it. In play `+9` is 0 and 1 only (the read-back sets 0, the
  split counts to 2); ours aborts with a message where the bound passes 255.
- **Unchecked index**: the capture record byte `0x6769BA` indexes a table of
  three before `EffectKindA1_States`' code pointers; both captures write 1, so
  records 0 and 2 are never used (perhaps for other sprites once). Ours aborts
  past three. `Effect_FindFree`'s answer is trusted below 20 (ours aborts past).
- **`EffectKind5F_DrawLineDisc`'s second argument is never read** (the callers
  push 0x20): a parameter the code dropped, or a size the disc was meant to use
  in place of its own 0x50.
- **`EffectKind9C_DrawTrail`'s dots** are lifted by `hx` (`0x5C41B8`, the
  x half-width) in y where the quads use `hy` (`0x5C41C0`) - perhaps a typo, as
  read.
- **`EffectKindA8_Start` sets the word `+0x2E` = 0x12C** that no state of kind
  0xA8 reads.

No read of memory the original never wrote reaches what is drawn: the
screen-space POLY_G3 / POLY_G4 of the bars, the disc and the fill leave their
depth words at `Gpu_SetPolyG3` / `_G4`'s 0.01 (`battle_items.cpp`,
`field_misc.cpp`). **Nothing needs a ledger entry** beyond DIV-0041's amendment
(section 2).

## 8. Calls across groups

**Outbound**: none raw. By name, merged groups': E3A's `EffectKind60_DrawLine`,
E4E's `EffectKind9E_DrawPlate` / `_DrawTexPlate` / `_DrawWall` /
`_DrawShadePlate`, E4F's `EffectKindAB_Emit` / `_MoveDrops`. Capcom's, raw in
`rest_3f_callees.h`: the square root `0x5A7A90` and the VRAM read-back
`0x59E930` (no group's: library layer and renderer). `Rand` by its macro.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `0x47FE30`, `0x47FEE0`, `0x480010`, `0x4800E0`, `0x480190` | R3E (this wave) | `EffectKind5F_DrawLineDisc` `0x480300`, raw in R3E's files until both merge |
| `0x4925E0`, `0x492620` | R3G (this wave) | `EffectKindAA_DrawFill` `0x492400`, raw in R3G's files |
| `EffectKind69_Part2` `0x483540` | E3B (ours) | `EffectKind69_DrawLines` - `effect_3b_callees.h` `kKind69Lines`, rebound |
| `EffectKind9C_Grow` .. `_Shrink` (six) | E4E (ours) | `EffectKind9C_DrawTrail` - `kTrail9C`, rebound |
| `EffectKindA7_Run` `0x491AA0`, `EffectKindA9_Run` `0x491C60` | E4E (ours) | `EffectKindA7_DrawGlow`, `EffectKindA9_DrawDisc` - `kGlowA7`, `kDiscA9`, rebound |
| `EffectKindB0_StepBars` `0x493370` | E4F (ours) | `EffectKindA8_DrawBar` - `kBarDraw`, rebound |
| the dispatchers `EffectKind60_Run`, `EffectKind9E_Run`, `EffectKindA1_Run`, `_A2_Run`, `_A3_Run`, `_A7_Run`, `_A8_Run`, `_A9_Run`, `EffectKindAA_Run`, `EffectKindAB_Run` | E3A, E4E, E4F (ours) | the 36 states and the two added, through their tables, read in place |

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 50 rows, and no first-call trace under
`analysis/calltrace` names any of them (a grep of every file: the only hits
are extent lists and three `inject OFF` lines of an older build's log, DLL
addresses that happen to share digits). **Fuzz only.** No live run was made
(the brief). Chapter 15 spawns kinds 0x9C, 0x9E, 0xA8 and 0xA9 (E4E's doc); a
recorded route through it would let the coordinator's frame-hash A/B cover
those four kinds' states here.

## 10. The rebinding

`grep -rn -i` of the 50 addresses in `src/game` (`band_rows.py --refs`: 54
references to 14 of them). Rebound, each on its own line, the value unchanged
so every fuzz key stands:

| File | Was | Now |
|---|---|---|
| `effect_3b_callees.h` | `kKind69Lines = 0x4837B0` | `bof3::addr::EffectKind69_DrawLines` |
| `effect_4e_callees.h` | `kTrail9C = 0x48ED80`, `kGlowA7 = 0x491E30`, `kDiscA9 = 0x492260` | `bof3::addr::EffectKind9C_DrawTrail`, `_A7_DrawGlow`, `_A9_DrawDisc` (and the header now includes `symbols.gen.h`) |
| `effect_4f_callees.h` | `kBarDraw = 0x4920F0` | `bof3::addr::EffectKindA8_DrawBar` |
| `effect_3b_fuzz.cpp` | the row `E3B_RAW(0x4837B0)` | `"EffectKind69_DrawLines", at::kKind69Lines, at::kKind69Lines` (the log's name; the key the address still) |
| `effect_4e_fuzz.cpp`, `effect_4f_fuzz.cpp` | the rows named `"0x491E30"`, `"0x4920F0"` | named `"EffectKindA7_DrawGlow"`, `"EffectKindA8_DrawBar"` |
| comments in `effect_3a.cpp` (2), `effect_3b.cpp`, `effect_3b_fuzz.cpp`, `effect_4e.cpp` (4), `effect_4e_fuzz.cpp`, `effect_4f.cpp` (4) | the addresses as the thing called | the names |

**Left raw**: the band tool's `CallSite` tables in the merged groups' fuzz
files (`{0x29, 0x4837B0}` and the like - the tool's output, which round
thirteen's cleanup left literal for every merged callee); the comments that say
where a state is hidden (`hidden in 0x4918B0`, `0x492400`, `0x48ED80`:
extents, still true); and **`scenario_harness.cpp`'s five `FX_RAW` rows**
(`0x48ED80`, `0x4837B0`, `0x491E30`, `0x492260`, `0x4920F0`) - a harness is
not this group's to edit: for the coordinator's fold. **For the coordinator**:
R3E's files call `0x480300` raw and R3G's `0x492400` (section 8); once each
merges beside this they bind to `EffectKind5F_DrawLineDisc` /
`EffectKindAA_DrawFill`.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04): 38 lines, the read extents
of the 36 hidden starts and the two added states. The host lines already there
are cut at the next listed entry and cover none of this group's starts
(`0047FBE0 1A0`, `0048ED80 2F0`, `00490A30 23`, `004918B0 1F0`, `00492400
110`); the twelve visible rows' lines (`00480300 1B7`, `004837B0 1C0`,
`0048ED80 2F0`, `0048F3D0 1F4`, `004918B0 1F0`, `00491E30 1B7` .. `00492400
110`) were there and are left.
