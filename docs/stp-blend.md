# The bit-15 round: semi-transparent primitives blend only the texels with bit 15

**Status:** IN PROGRESS (2026-10-23: step 1, the census, done ([`stp-census.md`](stp-census.md)); step 2, bit 15
carried into the textures, built as DIV-0160 and changing no pixel; step 3, the draw by texel, next). Branch
`render/stp-blend`. The defect is [`known-defects.md`](known-defects.md) D334.

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

## 3. Step 3: the draw (not built)

To decide: two passes (the primitive's opaque texels with the blend off, then its bit-15 texels with the blend
on, each discarding the other), or one pass with dual-source blending (the shader outputs a per-pixel blend
factor). The census found no primitive mixing the two kinds, so a per-primitive choice would match on its routes,
but it would not be the PlayStation's rule.

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
