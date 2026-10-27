# The bit-15 round: semi-transparent primitives blend only the texels with bit 15

**Status:** STABLE (2026-10-23: step 1, the census, done ([`stp-census.md`](stp-census.md)); step 2, bit 15
carried into the textures, built as DIV-0160, no pixel changed; step 3, the draw by texel, built, verified
(sections 3 and 5) and played by the owner. Left, unscheduled: the census on the routes it did not reach). Branch `render/stp-blend`. The defect is [`known-defects.md`](known-defects.md) D334.

## 1. What the round restores

On the PlayStation a semi-transparent textured primitive blends only the texels whose colour has bit 15 set; a
texel with it clear is drawn opaque, and a colour of 0 is transparent. The PC port drops bit 15 when it turns a
colour into a texel and blends every texel of such a primitive by its tpage's mode. Seen in play: the McNeil
rooftop's rope (area 28) at a quarter of its brightness. The census found one other family it changes, the text
on page 15 drawn additively with the text palettes (`7800` / `7807`). The PlayStation draws that text opaque too
(the twin's savestates; [`stp-census.md`](stp-census.md) 4).

The plan, agreed with the owner 2026-10-23 (HANDOFF item 00):

1. The census - done.
2. Carry bit 15 into the textures - built (section 2), no visible change.
3. Draw by texel: in a semi-transparent textured primitive, bit-15-clear texels opaque and bit-15 texels
   blended.
4. A switch (`StpTexels`, plus step 3's own), the fuzzes, A/B frames on the census routes, the owner's eye on
   the rope and some effects.

## 2. Step 2: the encoding

Only two functions turn a PlayStation colour into a texel: `Gfx_ConvertRow` (`0x59EBB0`), which converts a
CLUT row, and `Tex_Convert16` (`0x5A9D2F`), for a direct-colour page. Every palettized texture indexes
`Gfx_ConvertRow`'s output: the page textures (`Tex_Convert4` / `8`), the cell sprites (`Cell_Unpack*`) and the
glyphs (`Font_UnpackGlyph`). The two other callers of `Gfx_PackRgb` (`0x5AAA09`, `0x5AC1D5`) are MMX routines the
exe host never runs ([`exe-engine-own.md`](exe-engine-own.md) 4.1).

The page format is A1R5G5B5, opaque bits `0x8000` (`display_setup.cpp` record 0). One alpha bit, already used for
transparency, cannot hold a third state on its own. But Capcom's converters never write one combination: alpha
clear with a non-zero colour. A cell of 0 converts to 0, and every other cell gets the opaque bits, with a colour
that would round to 0 forced to 1 first. So, with DIV-0160:

| PlayStation colour | texel |
|---|---|
| 0 | 0 (transparent), as before |
| non-zero, bit 15 clear | the colour plus the alpha bit, as before |
| bit 15 set | the colour with the alpha bit clear; a colour that would round to 0 is forced to 1, as before |

Nothing changes in the 16-bit textures' format, size or colour, and no other writer of the texture surfaces
had to change.

**The backend** ([`render-backend.md`](render-backend.md)) reads alpha clear on a non-zero colour as a drawn
texel (alpha 255 in its B8G8R8A8 upload, as every drawn texel was). It also uploads a second texture beside it,
`R8_UNORM`, 255 where bit 15 is set (`GpuTexture::stp`, bound at `t2`), made on the first upload that carries
one. Keeping bit 15 out of the colour texture's alpha matters: under bilinear filtering the alpha is
interpolated, and a third alpha level would move the discard edge of every sprite. The plane is read at the
nearest texel.

`render_shim.cpp`'s `ReadPixel` (depth-converting blits) reads the encoding the same way. A same-depth blit
copies raw, so the stage-to-texture blit of `D3d_BuildPageTexture` keeps it.

**When it is on** (`render::StpTexels()`): the switch `StpTexels` armed and our backend running (`InitShim` has
run). It is armed in `TexPage_Inject`, after the `gfx_clut` and `tex_page` fuzzes, and only when DIV-0159's
`DirectOpaque` is. Without DIV-0159, a direct-colour texel is a colour with alpha clear, which this would read
as drawn. Under Capcom's DirectX 6 objects (`BOF3X_ORIGINAL=Display_Setup`), alpha clear means transparent to the
hardware, so the converters keep Capcom's rule there.

A CLUT row converted before `InitShim` keeps the old encoding until it is converted again. The display set-up
runs before the game draws, so none was seen.

**Diagnostic:** `BOF3X_STPVIEW=1` draws every texel the plane marks in magenta (`flags & 64` in the scene
shader).

## 3. Step 3: the draw by texel (built)

One pass, dual-source blending (core from feature level 10_0, the lowest the backend makes its device at). A
draw is drawn by texel when all of these hold: the switch `StpBlend` is on, its texture carries a plane (section
2), the blend is enabled, it is not a DIV-0125 grab draw, and its D3DBLEND pair is one `PSStp` can express
(`StpFactors`). That means a source factor that is one number for all channels (ZERO, ONE, SRCALPHA, INVSRCALPHA)
and a destination factor that reads only the source (those and SRCCOLOR, INVSRCCOLOR). Capcom's `D3d_SetBlend`
uses four pairs, all expressible: 5 / 6, 2 / 2, 1 / 4, 5 / 2. Such a draw runs `PSStp` under the blend state
`SRC1_ALPHA` / `SRC1_COLOR`. Its second output carries the source factor in alpha and the destination factor in
colour, per pixel:

- **A bit-15 texel, or a transparent one:** the draw's own factors, computed from the clamped source colour as
  the fixed blend computes them. It looks as before, but for rounding: under bilinear filtering 13 pixels of a
  frame of Amalgam's fight differ by one level.
- **Any other drawn texel:** `cover` and 1 - `cover`, opaque as the PlayStation draws it. `cover` is the
  texture's filtered alpha under alpha modulate, else 1, which is what an opaque draw's SRCALPHA reads at an
  edge. Under point filtering that is 1 and 0.

The texel is the nearest one to the pixel (the plane and the colour texture's alpha read by `Load`). A
transparent nearest texel keeps the draw's own blend. Without that, a bilinear edge (whose filtered alpha
passes the colour-key test) was drawn as an opaque texel and every sprite lost its soft edge. That was the first
build's fault; the bilinear A/B found it.

A non-semi-transparent draw is drawn by texel too: Capcom's opaque mode is 5 / 6 with alpha 1, so its factors
come out the same either way.

Not drawn by texel: a grab draw. The grab's texels are the frame's picture, and the plane holds the page's bit
15, not the picture's. What the PlayStation does with bit 15 of a picture moved into a texture page is not
measured. Nor is any other D3DBLEND pair, though none other was seen.

The first build of step 3 grew the draw's constant buffer to 64 bytes but made it at 48, so `PSStp` read the
blend pair as 0 / 0 (ONE / ONE) and the copy wrote past the mapped buffer: shadows and dark polygons vanished,
sprites turned additive, and the owner saw them flicker. The buffer is now made at `sizeof(DrawConstants)`.

## 4. Verification of step 2

Root `C:\Users\kerok\bof3x-runs\stp_census` (`ab.sh`, `ab_<route>_<on|off|view>`).

- **The start-up fuzzes.** `BOF3X_SHADOW=gfx_clut,tex_page` pass. A new check in the `gfx_clut` self-test runs
  `Gfx_ConvertRow` with the rule forced on against Capcom's copy over 600 rows: 74,752 cells with bit 15, 0
  mismatches (the expected value is Capcom's output less the opaque bits wherever the cell has bit 15). The full
  `BOF3X_SHADOW='*'` headless (an ini-less copy, `LS\`): exit 0, 805 self-test lines, no mismatch, no fatal.
- **No pixel changes.** A/B frames, `--host bof3`, `BOF3X_FILTER=point`, the build on against
  `BOF3X_ORIGINAL=StpTexels` (each log says `patch ON` / `OFF`). Every shot is identical:

  | recipe | shots | where |
  |---|---|---|
  | `ropeAndAmalgam` (`rope_ab.txt`) | 17 | the rope (frames 260..380), Amalgam's melt (7600..7780, a direct-colour page) |
  | `bossAndFlash` (`boss_ab.txt`) | 21 | every 250 frames: the boss fight and its flash |
  | `dragonTransform` (`dragon_ab.txt`) | 22 | every 200 frames: the transformation's effects |
  | `combat` (`combat_shots.txt`) | 13 | the load screen, battle text, numbers, the HP pop-up |

  This is point filtering only. Under bilinear the colour texture's bytes are the same by construction
  (section 2), so the sampling is the same.
- **The plane is right.** `BOF3X_STPVIEW=1` on `rope_ab` and `boss_ab` (sheet
  `analysis/shots/stp-view-1023/view_rope.png`, local only): the rope stays unmarked, as its palette has no
  bit 15. The melt's direct-colour page, the party's shadows, the HUD windows' dark backings and a darkening
  polygon in Amalgam's fight are marked. The HUD text and the damage numbers are not, as the census and the
  twin say. The view marks bit-15 texels in every textured draw, opaque primitives included, where the bit
  changes nothing (the stairs' edges on the rooftop). Every boss shot has marked pixels (2,330 to 35,456).

## 5. Verification of step 3

Root `C:\Users\kerok\bof3x-runs\stp_census` (`s3*.sh`, `ab_<route>_s3*`); the final shader throughout. Each A/B is
`--host bof3` against step 2's frames, which are Capcom's draw.

- **The rope.** `rope_ab`, point filtering: 15 of 17 shots are identical, and the two that differ (recipe frames
  300 and 340) change only the rope: 669 and 441 pixels. The rope's median goes from (20, 16, 6) to
  (81, 62, 25), against the twin's (72, 56, 24). The twin's figure is the handoff's, over its own pixel set
  under the sibling's renderer, so a tenth either way is within the method. The knot where it meets the roof
  shows. Amalgam's fight (13 shots, the melt, shadows, the windows) is identical.
- **The text.** `combat` 3 of 13 shots differ, `bossAndFlash` 3 of 21, `dragonTransform` 8 of 22. Every
  differing box is text on page 15: the load screen's slot labels, item counts, the EXP lines, AP and skill
  numbers, the HP pop-up. Each draws in its palette colour instead of brighter on its backing. Nothing else
  moves: no effect, shadow or sprite.
- **Off.** `BOF3X_ORIGINAL=StpBlend` on `rope_ab`: 17 of 17 shots identical to step 2.
- **Bilinear** (`BOF3X_FILTER=linear`, `rope_ab`, on against `StpBlend` off): the rope's two shots, and 13
  pixels off by one level in a frame of Amalgam's fight (the shader's factor against the fixed blend's
  rounding). The first build's hard sprite edges (section 3) are gone.
- **Self-tests:** `BOF3X_SHADOW=gfx_clut,tex_page,d3d_draw` headless, exit 0, `StpTexels` and `StpBlend` ON.
- **The owner** (2026-10-23, watching the runs): saw the first build's flicker, which section 3 explains and
  the fix removed; on the fixed build, the text "looks much closer to original now to my eye". Then a quick
  playthrough of the committed branch: "confirmed both the fixes" (the rope and the text).

Not reached: the routes the census still lacks ([`stp-census.md`](stp-census.md) 6.5; section 6 ran fourteen
more on 2026-10-27 and found the world map's button legend, mixed, drawn per texel as the PlayStation does); a draw with a D3DBLEND pair other
than Capcom's four; a grab draw's bit 15 on the PlayStation.
