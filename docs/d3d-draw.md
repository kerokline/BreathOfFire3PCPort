# The Direct3D draw: the ordering-table walk, its handlers and their helpers

**Status:** IN PROGRESS (2026-09-22 - group R of the fourth parallel round:
twelve functions ours, faithful, fuzzed and controlled headless; no
divergence; not yet run live - the batch after the merge is the first check.
Two latent defects written down, D27 and D28)

Everything the port draws under Direct3D goes through one walk of the PSX
ordering table, `Gfx_DrawOTag` `0x59EE50`, which hands each primitive to a
handler by its GPU code. This group took over the walk, the six handlers that
were still Capcom's (`POLY_FT4`, `POLY_GT4`, `LINE_F2`, `LINE_F4`, `TILE` and
the port's own cell sprite, code `0x84`), the four state helpers every handler
calls and the cell sprite's texture cache - `src/game/d3d_draw.cpp`,
`src/game/d3d_list.cpp`, fuzzed by `src/game/d3d_draw_fuzz.cpp` and
`src/game/d3d_list_fuzz.cpp` under `BOF3X_SHADOW=d3d_draw`. The glyph handler
is group N's ([`glyph-draw.md`](glyph-draw.md)), the three `SPRT` handlers are
DIV-0010's copies; both stay as they were.

Every claim about the binary below is from capstone over `bof3/BOF3.exe` on
2026-09-22 (the scratchpad's `groupR_dasm.py` / `groupR_calls.py`, the same
reads `tools/pe_disasm.py` gives) unless it says otherwise. Call counts are
the attract trace `analysis/calltrace/hidden_b/bof3x.callcounts.tsv`. None of
these functions has a PSX twin: the PlayStation drew primitives on its GPU.

## 1. The functions

| PC | name | true size | code | calls (hidden_b) |
|---|---|---|---|---|
| `0x59EE50` | `Gfx_DrawOTag` | `0x450` code (to `0x59F29F`), `0x6C8` with its tables (to `0x59F517`) | - | 10,860 |
| `0x59FBA0` | `D3d_PrimColor` | `0xE4` (to `0x59FC83`), `0xF4` with its table | - | 2,310,145 |
| `0x59FCA0` | `D3d_SetBlend` | `0xC9` (to `0x59FD68`), `0xDC` with its table | - | 2,305,069 |
| `0x59FD80` | `D3d_SetShadeMode` | `0x24` (to `0x59FDA3`) | - | not in the trace's list; every handler calls it |
| `0x59FFE0` | `D3d_BindTexture` | `0x97` (to `0x5A0076`) | - | 2,213,770 |
| `0x5A0C40` | `D3d_DrawPolyFT4` | `0x23D` (to `0x5A0E7C`) | `0x2C` | 2,183,660 |
| `0x5A14C0` | `D3d_DrawPolyGT4` | `0x2D4` (to `0x5A1793`) | `0x3C` | 1,692 |
| `0x5A17A0` | `D3d_DrawLineF2` | `0x10F` (to `0x5A18AE`) | `0x40` | 8,662 |
| `0x5A1D10` | `D3d_DrawLineF4` | `0x185` (to `0x5A1E94`) | `0x4C` | 1,894 |
| `0x5A20D0` | `D3d_DrawTile` | `0x14A` (to `0x5A2219`) | `0x60` | 2,320 |
| `0x5A2EB0` | `D3d_DrawCellSprite` | `0x2AA` (to `0x5A3159`) | `0x84` | 17,716 |
| `0x5A3160` | `D3d_CellTexture` | `0x144` (to `0x5A32A3`) | - | 17,716 |

Sizes are to the byte after the last instruction. The queue's sizes for the
handlers were right; its 208 for the draw was not (`pe_funcs.py` stopped
inside it). The codes are confirmed from the other side too: the libgpu
setters that are ours (`psx_gpu.cpp`) write them - `Gpu_SetPolyFT4` `0x2C`,
`Gpu_SetPolyGT4` `0x3C`, `Gpu_SetLineF2` `0x40`, `Gpu_SetLineF4` `0x4C`,
`Gpu_SetTile` `0x60`, `Gpu_SetCode84` `0x84` (a raw scan for `C6 4x 07 nn`,
one site each). Every one of the twelve is reached by the attract sequence,
so the live batch runs all of them.

Not taken, recording stand-ins in the fuzz: the page-texture builders
`D3d_BuildPageTexture` `0x5A0080` and `D3d_RefreshPageTexture` `0x5A0510`,
the cell-texture builders `D3d_BuildCellTexture` `0x5A32B0` and
`D3d_RefreshCellTexture` `0x5A37D0` (DirectDraw surfaces, `Lock` / `Blt`),
`D3d_SetAlphaModulate` `0x59F520`, `D3d_AfterDraw` `0x59F580`, the other
fifteen Direct3D handlers and all twenty-one software ones, and `0x437CC0`
(a bare `ret`, left as it is - taking it would change nothing).

## 2. The walk and its two tables

`Gfx_DrawOTag(ot)` - one call a rendered frame, from WinMain `0x4FCE6F` with
`[0x937F84] + 0x8C`; the return is not read.

1. Nothing at all unless `Gfx_DrawEnable` `0x7DED17` (byte). The only
   instruction in `.text` that names it (a raw scan) is the walk's own
   draw-mode case below, so a draw mode with bit 0 clear
   turns off the *next* draw.
2. `D3d_AfterDrawRequest` `0x7CADEA` (u16) cleared if set.
3. `Gfx_RenderFlags` bit 0, read once: the software surfaces' table; else
   device `BeginScene`, the Direct3D table, `EndScene` after the walk.
4. The walk: while the link word at the current node is not -1, take it; a
   link with its top byte set is masked to 24 bits and the node it reaches is
   **not** drawn; a clean one is dispatched. The drawn node's link is read
   after its handler returns (a handler may rewrite it). This is
   [`known-defects.md`](known-defects.md) D4's walk.
5. Every `Font_TexCache` in-use word (`0x7C9F60`, stride `0x14`, 128) and every
   `D3d_CellTexCache` in-use word (`0x7CAE4C`, stride `0x28`, 128) zeroed.
6. If `D3d_AfterDrawRequest` is now set, a tail `jmp` to `D3d_AfterDraw`
   `0x59F580`, which reads no stack argument (its frame addresses nothing
   above its return address). A raw scan of `.text` finds `0x7CADEA` only in
   this function, so what sets it does so through another base; not found.

The dispatch: index = `(code & 0xFC) - 0x20`, unsigned; beyond `0xD0`
(software) or `0xD4` (Direct3D) nothing; else a byte index table gives the
entry of a jump table.

| code | entry | Direct3D (`0x59F3D8` / `0x59F440`) | software (`0x59F2A0` / `0x59F304`) |
|---|---|---|---|
| `0x20` POLY_F3 | 0 | `0x59FA50` | `0x5A3A60` |
| `0x24` POLY_FT3 | 1 | `0x59FDB0` | `0x5A3B60` |
| `0x28` POLY_F4 | 2 | `0x5A0AB0` | `0x5A3D70` |
| `0x2C` POLY_FT4 | 3 | **`D3d_DrawPolyFT4`** | `0x5A3E90` |
| `0x30` POLY_G3 | 4 | `0x5A0E80` | `0x5A3FB0` |
| `0x34` POLY_GT3 | 5 | `0x5A1050` | `0x5A40C0` |
| `0x38` POLY_G4 | 6 | `0x5A1290` | `0x5A41A0` |
| `0x3C` POLY_GT4 | 7 | **`D3d_DrawPolyGT4`** | `0x5A42E0` |
| `0x40` LINE_F2 | 8 | **`D3d_DrawLineF2`** | `0x437CC0` (ret) |
| `0x48` LINE_F3 | 9 | `0x5A1A00` | ret |
| `0x4C` LINE_F4 | 10 | **`D3d_DrawLineF4`** | ret |
| `0x50` LINE_G2 | 11 | `0x5A18B0` | ret |
| `0x58` LINE_G3 | 12 | `0x5A1B50` | ret |
| `0x5C` LINE_G4 | 13 | `0x5A1EA0` | ret |
| `0x60` TILE | 14 | **`D3d_DrawTile`** | `0x5A4400` |
| `0x64` SPRT | 15 | `D3d_DrawSprt` (DIV-0010) | `0x5A45B0` |
| `0x68` TILE_1 | 16 | `0x5A2220` | `0x5A4500` |
| `0x6C` glyph | 17 | `D3d_DrawGlyph` (group N) | `0x5A4900` |
| `0x74` SPRT_8 | 18 | `D3d_DrawSprt8` (DIV-0010) | `0x5A46E0` |
| `0x7C` SPRT_16 | 19 | `D3d_DrawSprt16` (DIV-0010) | `0x5A47F0` |
| `0x84` cell sprite | 20 | **`D3d_DrawCellSprite`** | `0x5A4C40` |
| `0xE0`, `0xE8` draw mode | 21 | `Gfx_DrawEnable` = byte +6 bit 0, `0x7DED16` = bit 1, `Gfx_DrawTpage` `0x7DED14` = word +4, and `Gfx_TexCacheKey` from a non-null `+8` | same |
| `0xEC` | 22 | `Gfx_MoveImage(prim + 8, +0x10, +0x14)` | same |
| `0xF0` | 23 | `Gfx_TexCacheKey` from `+8`, not checked for null | same |
| `0xF4` | 24 | `D3d_SetAlphaModulate(byte +8 ^ 1)` | none (out of range) |
| any other | 25 / 24 | none | none |

Each code covers its four low values (`0x2C..0x2F` etc., the PSX's
semi-transparency and raw-texture bits). The case blocks do not lie in entry
order (software 15 / 16 and 17..19, Direct3D 15 / 16 and 17..19): the clone's
re-aimed sites are listed by entry in `d3d_list_fuzz.cpp`.

Ours calls every handler through the address the original calls, so the ones
that are ours elsewhere run as under Capcom's walk. Ours `Fatal`s on a byte
table entry the tables cannot hold (they are constant data).

## 3. The handlers

Common to all: the vertices are the four `D3DTLVERTEX` at `D3d_Vertices`
`0x7CA958`; positions are the primitive's floats times `D3d_ScaleX` /
`D3d_ScaleY` (`fld scale; fmul x; fstp`), no half-pixel offset; the last call is
`DrawPrimitive(type, 0x1C4, D3d_Vertices, n, 0)` and its result is returned
(the caller does not read it); every handler reads the primitive afresh after
each call it makes. Not kept, in all of them: the original lets the colour
helper write the diffuse into the caller's pushed `prim` slot, which the
caller pops unread (`add esp, 4` after every call site of the walk).

- **`D3d_DrawPolyFT4`** (`POLY_FT4`, 0x48 bytes): corners of `0x10` from +8:
  float x, y, z, byte u, v; CLUT word +0x16, tpage word +0x26.
  `D3d_PrimColor(r, g, b, code, tpage, &diffuse, &specular)`; per corner sx,
  sy, `sz = z` (a `mov`: bits kept), `rhw = 0.1 / z` (`0x5C4610`, x87), the
  colour pair, `tu, tv = D3d_TexCoords[u], [v]` (`0x7CA9E0`, through `fld` /
  `fstp`); `D3d_BindTexture(tpage, clut)`; `0x437CC0(tpage & 0x400)`,
  `0x437CC0(tpage & 0x800)`; `D3d_SetBlend(code, tpage)`;
  `D3d_SetShadeMode(1)`; a 4-vertex strip.
- **`D3d_DrawPolyGT4`** (`POLY_GT4`, 0x54 bytes): corners of `0x14` from +4:
  r, g, b, pad, float x, y, z, u, v; CLUT +0x16, tpage +0x2A. Four colour
  calls, one per corner, all before any vertex; then as FT4, `0x437CC0(0)`
  twice, `D3d_SetShadeMode(2)` (Gouraud).
- **`D3d_DrawLineF2` / `D3d_DrawLineF4`**: corners of `0xC` from +8 (float
  x, y, z), two or four. The blend mode is `Gfx_DrawTpage & 0xFFFF`, read on
  entry for the colour and read again for the blend. The colour helper gets a
  **null specular pointer**: the vertices' specular, `tu` and `tv` are left as
  the previous draw left them (harmless: no texture, and `D3DFVF_TLVERTEX`'s
  specular is ignored with specular off - not checked). `SetTexture(0, NULL)`,
  `0x437CC0(0)`, `0x437CC0(1)`, blend, flat, `D3DPT_LINESTRIP` (3) of 2 or 4.
- **`D3d_DrawTile`** (`TILE`): float x +8, y +0xC, z +0x10, w +0x14, h +0x18.
  Corners (x, y), (x + w, y), (x, y + h), (x + w, y + h) - the far edges as
  `fld w; fadd x; fmul scale` - `sz` = z through `fld` / `fst` (a signalling
  NaN comes out quiet here and not in the other handlers), `rhw = 0.1 / z`,
  diffuse only; `SetTexture(0, NULL)`, `0x437CC0(1)` twice, blend from
  `Gfx_DrawTpage`, flat, a strip.
- **`D3d_DrawCellSprite`** (code `0x84`): float x +8, y +0xC, sprite scale x
  +0x10, y +0x14; u16 first cell +0x18, count +0x1A, CLUT +0x1C, flags +0x1E
  (the colour's and the blend's tpage; bit `0x400` flips x). The colour, then
  `index = D3d_CellTexture(first, count, clut, flags)`; from that entry of
  `D3d_CellTexCache` the extent (s16 left +8, right +0xA, top +0xC, bottom
  +0xE, in texels) gives the corners `(x + e * sx) * D3d_ScaleX` - or
  `(x - e * sx)` when flipped - and likewise y (never flipped); the texture
  coordinates are `0.5 / W` and `(used_w - 0.5) / W`, `0.5 / H` and
  `(used_h - 0.5) / H` (u16 +0, +2 the used size, +4, +6 the texture's) - see
  D28; `sz` 0.99 and `rhw` 0.1 as immediates; `0x437CC0(1)`, blend(code,
  flags), flat, a strip. The index is not range-checked (D27 can make it
  anything up to 0xFFFF).

**`D3d_CellTexture(first, count, clut, flags)`** - one pass over the 128
entries (`D3d_CellTexCache` `0x7CAE38`, `0x28` bytes: used w / h +0 / +2,
texture w / h +4 / +6, extent +8..+0xE, CLUT +0x10, count +0x12, used this
frame +0x14, use counter +0x16, the cells' checksum +0x18, the CLUT row's
generation +0x1C, surface +0x20, texture +0x24):

- first dword 0 (never built): `D3d_BuildCellTexture(i, first, count, clut,
  flags & 0x800)`, use it;
- a hit: count and CLUT words equal to the arguments (32-bit compares), and
  the checksum equal to the 32-bit sum of the `2 * count` dwords of
  `SpriteCell_Table + first * 8`; if its generation is not
  `Gfx_ClutRows[clut >> 6]`'s and it is not in use this frame,
  `D3d_RefreshCellTexture(i, first, count, clut)`; use it;
- on the way, the victim: not in use this frame and the smallest counter below
  `0xFFFF`, the first on a tie (signed `jle`);
- neither: build the victim - **D27** when there is none;
- use: counter += 1 (a word, wraps), in-use = 1, and unless
  `Gfx_RenderFlags` bit 0 (read after the build), `SetTexture(0, +0x24)`;
  return the index. The software path `0x5A4C40` is its other caller.

## 4. The helpers

- **`D3d_PrimColor(r, g, b, code, mode, *diffuse, *specular)`** - as group N
  read it ([`glyph-draw.md`](glyph-draw.md) §4), with two details: only the
  low byte of `code` is tested, and in the untextured case r, g and b are
  OR-ed in as whole dwords, unmasked. The doubling is 32-bit (`shl`), so
  `0x80000000` doubles to 0 and is not clamped. The diffuse is written before
  the specular (they may alias). 37 call sites; none reads `eax` after it
  (a scan of every site, the scratchpad's `groupR_callers.py`) - nor after
  `D3d_SetBlend` (21) or `D3d_SetShadeMode` (21), which ours return nothing
  from.
- **`D3d_SetBlend(code, mode)`** and **`D3d_SetShadeMode(mode)`** - as N read
  them; the shade cache `0x6C3A44` is written after the render-state call.
- **`D3d_BindTexture(tpage, clut)`** - page `tpage & 0x1F`, colour mode
  `(tpage >> 7) & 3`, `Gfx_TexCacheFind` (ours, `gfx_texcache.cpp`); a miss
  (slot `0x20`) builds between `EndScene` and `BeginScene` and sets what the
  builder returns; a hit sets the entry's `+0x14`, first refreshing a state-2
  entry outside the scene. The texture it sets on that path was read
  **before** the refresh - kept; harmless, because `D3d_RefreshPageTexture`
  re-fills the existing surfaces (`Lock` / `Unlock` / `Blt`) and never
  writes `+0x14` (its stores read 2026-09-22). Returns `SetTexture`'s result;
  the one caller that touches `eax` after it (`0x59FF65`) re-loads `ax` and
  masks it.

## 5. The fuzz

`BOF3X_SHADOW=d3d_draw`, one start-up run, about 4 seconds. Each function
against a byte-copy of Capcom's with every call re-aimed at a recording
stand-in (ours on the same stand-ins through `d3d_draw::g` / `d3d_list::g`),
the device the harness's fake (`d3d_fuzz.h`, unchanged: every COM method
these functions call was already covered). The three jump tables are moved
into their copies (`D3d_PrimColor`'s, `D3d_SetBlend`'s, and both of the
walk's). Compared each round: the call log with arguments, the vertices each
`DrawPrimitive` was handed, every byte of state either side could touch (the
vertex block, scales, `D3d_TexCoords`, `Gfx_DrawTpage`, the shade cache, the
render flag, `Gfx_TexCache`, `Gfx_ClutRows`, `SpriteCell_Table`,
`D3d_CellTexCache` and five entries past it, the ordering table), the return
value.

**The x87.** Every float operation of the handlers is the original's
instruction sequence in inline assembly (`X87Mul`, `X87Div`, `X87Pass`,
`X87AddMul`, `X87IntMulAddMul`, `X87DivInt`, `X87IntSubDivInt`), so ours
rounds as Capcom's does under any precision control; each handler round runs
under a control word picked from `0x027F` (the game's, measured -
[`psx-library-layer.md`](psx-library-layer.md)), `0x007F` (24-bit) and
`0x037F` (64-bit). This is not caution for its own sake: a plain SSE float
multiply for `sx` differed from Capcom's only under `0x007F` (a denormal
product rounded twice), first in round 213, and a plain divide for `rhw` in
round 3,556 - controls 21 and 22 below. The renderer's control word in game
has never been measured (Direct3D may set 24-bit precision on its thread);
ours does not depend on it.

**The stand-ins** write what the real callees write where the caller reads it
again - the colour pair; a page-cache entry's state and texture (the find),
its texture (the refresh); a whole cell-cache entry and texture (the build);
the render flag after a cell build - and, a quarter of the time, one byte of
the primitive, the vertex block, the scales, `Gfx_DrawTpage` or a texture
coordinate in use. That orders every read and store of a handler against
every call it makes. The walk's recorders cut or re-route the link of the
node they were handed, set the after-draw word, and flip the render flag or
the draw-enable byte (which the walk must not re-read).

**Rounds and coverage** (the last run): `D3d_PrimColor` 200,000; `D3d_SetBlend`
and `D3d_SetShadeMode` 20,000 together; `D3d_BindTexture` 20,000 (built
6,677, refreshed 7,002, hit 6,321); `D3d_CellTexture` 40,000 (empty built
6,071, hit 19,016, refreshed 2,794, stale but in use 8,329, victim built
6,045, D27 6,074); each of the six handlers 20,000 under the three control
words; `Gfx_DrawOTag` 30,000 (software 15,082, draw disabled 3,716,
after-draw 3,005, 82,625 calls out, every one of the 54 table indices
reached). Seeded: colours and colour dwords `0, 1, 0x7F, 0x80, 0x81, 0xFF,
0x100, 0x7FFFFFFF, 0x80000000, ...`; tpage words across the blend and
colour-mode bits; floats `0, -0, 1, 0.5, 160, -32, 0.01, 0.99, 1e30, FLT_MAX,
the smallest normal, denormals, +-inf, a quiet NaN, two signalling NaNs,
2^24 - 1, 1/3`; scales `2.0` and the others group N seeded plus a denormal
product, an overflowing one and one ulp above 1; texels `0, 1, 0x7F, 0x80,
0xFE, 0xFF`; cell-sprite sizes `0` (a division by zero), `1 ... 0xFFFF` and
extents `0x7FFF / 0x8000 / 0xFFFF`; cache counters `0, 1, 2, 0x7FFF, 0x8000,
0xFFFE, 0xFFFF` and a pattern with every free entry at `0xFFFF`. Result:
**0 mismatches**.

**Negative controls: 84 planted, by two scripts that edit the source, build,
run the headless self-test and restore** (the scratchpad's
`groupR_controls.py` and `groupR_controls2.py`). 82 were refused by a
comparison - a call's arguments, the call count, a snapshot, memory, the
return value - and none by a hang. The two that were not:

- *red clamped at `>= 0xFF`* (`D3d_PrimColor`): unobservable - a doubled
  value is even, so it is never `0xFF`. Replaced by `>= 0xFE`, refused.
- *the victim search's best starting at `0x10000`* (`D3d_CellTexture`): the
  fuzz was blind - it needs every free entry at the counter ceiling. The
  pattern was added and the control, planted again, was refused.

One more is worth saying: *the CLUT compared as 16 bits* was at first refused
by a fault as often as by a comparison - a wide CLUT (`0x10000 << 15`) sent
the changed function to `Gfx_ClutRows` far outside the image, where Capcom's
never reads. The wide values were narrowed to `0x10000 << 0..3` (still wide,
and the read stays inside the image) and the control, run again, was refused
by a comparison in round 47. All the controls were run a second time on the
final fuzz: the same verdicts.

| # | control | refused by |
|---|---|---|
| 0 | alpha of blend mode 0 `0x7F` | the colour |
| 1 | alpha index `(mode >> 4) & 3` | the colour |
| 2 | red clamped at `>= 0xFF` | not refused: unobservable (above) |
| 3 | green excess not masked | the specular |
| 4 | raw texture on bit 3 | the colour |
| 5 | untextured red masked to a byte | the colour |
| 6 | specular written before diffuse | the aliased pair |
| 7 | specular r and b swapped | the specular |
| 8 | blend mode 2 is 4, 1 | call 1's arguments |
| 9 | blend code tested as a dword | call 2's arguments |
| 10 | alpha blend enable 0 | call 0's arguments |
| 11 | shade cache not written | memory |
| 12 | colour mode `(tpage >> 7) & 1` | call 0's arguments |
| 13 | texture read after the refresh | `SetTexture`'s argument |
| 14 | no `BeginScene` after the build | the call count |
| 15 | state `>= 2` refreshed | the call count |
| 16 | page cache stride `0x14` | `SetTexture`'s argument |
| 17 | rhw numerator 1.0 | snapshot |
| 18 | poly `sz` through the x87 | snapshot (a signalling NaN) |
| 19 | `tu` copied, not through the x87 | snapshot (a signalling NaN) |
| 20 | `sx` times the y scale | snapshot |
| 21 | `sx` in SSE float | snapshot, under `0x007F` only |
| 22 | `rhw` in SSE float | snapshot, under `0x007F` only |
| 23 | FT4 first ret gets `& 0x800` | call 2's arguments |
| 24 | FT4 bind arguments swapped | call 1's arguments |
| 25 | FT4 texels from +0x15 | snapshot |
| 26 | FT4 colour mode from the CLUT word | call 0's arguments |
| 27 | FT4 tpage read once, before the calls | call 4's arguments |
| 28 | FT4 Gouraud | call 5's arguments |
| 29 | FT4 triangle list | call 6's arguments |
| 30 | FT4 diffuse and specular swapped | snapshot |
| 31 | FT4 texels written after the bind | snapshot |
| 32 | GT4 corner 3 coloured from corner 2 | call 3's arguments |
| 33 | GT4 code read once before the colours | call 3's arguments |
| 34 | GT4 second ret gets 1 | call 6's arguments |
| 35 | GT4 flat | call 8's arguments |
| 36 | lines: blend mode not re-read | call 4's arguments |
| 37 | lines: a specular pointer | call 0's arguments |
| 38 | lines: corner stride `0x10` | snapshot |
| 39 | lines: `SetTexture(0, 1)` | call 1's arguments |
| 40 | `LINE_F4` as a triangle strip | call 6's arguments |
| 41 | lines: `tu` written | snapshot |
| 42 | tile: right edge scaled term by term | snapshot |
| 43 | tile: `sz` copied, not through the x87 | snapshot |
| 44 | tile: blend mode masked to a byte | call 0's arguments |
| 45 | tile: corners 1 and 2 swapped | snapshot |
| 46 | tile: bottom from w | snapshot |
| 47 | cell: flip on bit 3 | snapshot |
| 48 | cell: flip read before the lookup | snapshot |
| 49 | cell: top flipped too | snapshot |
| 50 | cell: far u from the used height | snapshot |
| 51 | cell: left extent unsigned | snapshot |
| 52 | cell: rhw one ulp low | snapshot |
| 53 | cell: ret gets 0 | call 2's arguments |
| 54 | cell: near u `0.5 / used width` | snapshot |
| 55 | cell cache: victim starts at 0 (a D27 fix) | the build's slot |
| 56 | cell cache: last on a tie | the build's slot |
| 57 | cell cache: best starts at `0x10000` | not refused: blind (above); planted again as 68 |
| 58 | cell cache: checksum over `count * 4` bytes | call 0 |
| 59 | cell cache: refresh even when in use | call 0 |
| 60 | cell cache: render flag read before the build | the call count |
| 61 | cell cache: counter not incremented | memory |
| 62 | cell cache: empty on the first word | the call count |
| 63 | cell cache: build gets `flags & 0x400` | call 0's arguments |
| 64 | cell cache: CLUT compared as 16 bits | call 0 (after narrowing, above) |
| 65 | cell cache: generation row `clut >> 5` | the call count |
| 66 | cell cache: a stale hit refreshed only when in use | call 0 |
| 67 | red clamped at `>= 0xFE` | the colour |
| 68 | cell cache: best starts at `0x10000`, planted again | the build's slot |
| 69 | walk: a masked node dispatched too | the call count |
| 70 | walk: render flag re-read per node | the call count |
| 71 | walk: draw-enable not tested | memory |
| 72 | walk: after-draw word not cleared on entry | the call count |
| 73 | walk: no `EndScene` | the call count |
| 74 | walk: glyph in-use stride `0x10` | memory |
| 75 | walk: cell in-use words not cleared | memory |
| 76 | walk: Direct3D range `0xD0` | the call count |
| 77 | walk: alpha argument not inverted | call 1's arguments |
| 78 | walk: draw mode bit 2 to `0x7DED16` | memory |
| 79 | walk: texture key half copied | memory |
| 80 | walk: move-image x and y swapped | call 1's arguments |
| 81 | walk: after-draw unconditional | the call count |
| 82 | walk: the head node dispatched too | the call count |
| 83 | walk: draw-mode key copied only with a non-zero tpage | memory (after `tpage 0` was seeded; blind before) |

(Control 83 was not refused on its first run either - the tpage word was
never 0 - and was refused once seeded; counted with the 82.)

## 6. Defects found

**D27 and D28** in [`known-defects.md`](known-defects.md), both latent,
Capcom's, kept by ours:

- **D27 - the cell-texture cache's victim is `count` when no entry is free.**
  `D3d_CellTexture` keeps the victim's index in the stack slot of its
  `count` argument. When no entry qualifies - all 128 used this frame, or
  every entry not in use has its counter at `0xFFFF` - the index built into,
  marked and set is `count`: a cell sprite of `n` cells evicts entry `n`,
  which is in use this frame (its texture already handed to earlier draws of
  the frame), and a count of 128 or more writes past the table. The fuzz
  seeds both conditions; control 55 (the fix "victim starts at 0") is
  refused.
- **D28 - cell sprites map texel centre to texel centre: the last row and
  column get half their pixels.** The near texture coordinate is `0.5 / W`
  and the far `(used - 0.5) / W`, so a sprite drawn at its own size shows
  `used - 1` texels over `used` texels' width. At 2x with point sampling
  (Direct3D 6 samples at the pixel's integer coordinate), pixel `k` of a
  `used`-texel sprite samples `u = 0.5 + k (used - 1) / (2 used)`: for
  `used` = 16, 24, 32 every texel gets 2 pixels except the last (1) and one in
  the middle (3). It is D1's shape (DIV-0010's far-edge slip) on both edges
  of every cell sprite. `Gpu_SetCode84`'s one caller is `Sprite_Draw`
  `0x5935B0` (`0x59366A`), which fills the cells through `SpriteCell_Add`
  (`0x593763`): the field's sprites. Under the default bilinear filter it is a soft, one-texel
  squeeze instead. **Proposed, not built:** DIV-0010's rule - keep the near
  value, move the far one so the last pixel samples the last texel's centre:
  far = `0.5 + (used - 1) * N / (N - 1)` texels for an `N`-pixel span (at 2x,
  `N = 2 used`: 15.984 for 16, 31.992 for 32 - every texel exactly 2 pixels in
  the arithmetic above). It is exact only at the sprite's natural size; the
  owner decides, as for D17 and D1.

Not a defect, checked: `D3d_BindTexture`'s stale texture read (§4). Not
established: whether `POLY_FT4` / `POLY_GT4` have D1's slip - they read
`D3d_TexCoords[u]` per corner with no `w - 1` arithmetic of their own, so it
depends on the far `u` the game's builders put in the primitive (`u + w` or
`u + w - 1`), which nobody has read.

## 7. What the self-test does not reach

- Pixels. The harness sees calls and vertices, never a surface. The real
  device, the real texture builders and the other 36 handlers are stand-ins.
- The x87 control word the renderer really runs under (ours matches under
  any; the fuzz checks three).
- `D3d_AfterDraw`, `D3d_SetAlphaModulate` and whoever sets
  `D3d_AfterDrawRequest` - unread.
- The walk's ordering table as the game builds it: the fuzz's tables are
  random, 1-15 nodes, links with and without a top byte.
- Whether ours is as fast: `D3d_DrawPolyFT4` runs 2.2 million times in the
  attract trace. Ours calls its helpers through a pointer table and does the
  float work in the same instructions; not measured.

**What would show a wrong one, live.** The renderer's range is excluded from
the frame hash, so the batch checks this group by A/B captures (`BOF3X_ORIGINAL`
with these twelve names against ours) and by eye. A wrong `D3d_DrawPolyFT4` /
`D3d_BindTexture` / `D3d_PrimColor` would show on every frame - the field's
map, menus' panels, every textured quad - as wrong colours, missing or
swapped textures, misplaced or torn geometry; `D3d_SetBlend` wrong as
semi-transparent effects gone opaque or too bright; `D3d_DrawCellSprite` /
`D3d_CellTexture` wrong as characters with the wrong frame, mirrored, or
flickering between textures; `D3d_DrawTile` as fades and flat boxes in the
wrong place or colour; lines (rare: 8,662 calls in the trace) as missing
lines; `Gfx_DrawOTag` wrong as whole primitives missing or drawn twice, or a
black frame. Any of these differs pixel for pixel from the original in a
capture of the same frame; the attract sequence runs every one of the
twelve (the counts in section 1).

## 8. Changes to shared code

None to `d3d_fuzz.h` / `.cpp` (N's harness is used as it is; `BOF3X_SHADOW=glyph_draw`
still passes). `symbols.toml`: one block at its end - the twelve, the six
stand-ins, `D3d_TexCoords`, `Gfx_DrawTpage`, `Gfx_DrawEnable`,
`D3d_AfterDrawRequest`, `D3d_CellTexCache`; N's three provisional helper
entries moved into it with `impl`.
