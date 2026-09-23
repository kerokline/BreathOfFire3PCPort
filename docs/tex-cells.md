# The glyph and cell textures, their unpackers and helpers

**Status:** IN PROGRESS (2026-09-23 - group U of the fifth parallel round:
eleven functions ours, faithful, fuzzed and controlled headless on group T's
fake DirectDraw, `src/game/ddraw_fuzz.*` (extended: colour fill, a bigger
log), and a fake device of its own; no divergence; not yet run live - the
batch after the merge is the first check. Two latent defects written down,
D31 and D32)

The field's sprites and the port's Chinese text are not drawn from VRAM the
way the PlayStation drew them: each is composed into a small Direct3D texture
first and cached. A glyph (24 x 24, 4 bits a texel) becomes a 32 x 32
texture in `Font_TexCache`; a sprite's cells (the pieces `Sprite_Draw` lays
out, up to 120 x 120 texels each) are composed onto the 320 x 256 staging
surface around its centre and the used rectangle copied into a texture in
`D3d_CellTexCache`. This group took over the three builders, the five
hand-written unpackers under them and three helpers -
`src/game/tex_cells.cpp`, fuzzed by `src/game/tex_cells_fuzz.cpp` under
`BOF3X_SHADOW=tex_cells`. Their callers are ours already:
`Font_GlyphTexture` ([`glyph-draw.md`](glyph-draw.md) §3) and
`D3d_CellTexture` ([`d3d-draw.md`](d3d-draw.md) §3).

Every claim about the binary below is from capstone over `bof3/BOF3.exe` on
2026-09-23 (the scratchpad's `udis.py`, a linear sweep - the same reads
`tools/pe_disasm.py` gives) and a raw E8 / dword scan of the image
(`callers.py`) unless it says otherwise. Call counts are the attract trace
`analysis/calltrace/hidden_b/bof3x.callcounts.tsv`. None has a PSX twin.

## 1. The functions

| PC | name | true size | calls (hidden_b) |
|---|---|---|---|
| `0x59F9B0` | `D3d_FitTextureSize` | `0x9A` (to `0x59FA49`) | 94, all from the cell build |
| `0x5A2CA0` | `Font_BuildGlyphTexture` | `0x1C9` (to `0x5A2E68`) | 6,005 |
| `0x5A2E70` | `Dd_ClearSurface` | `0x3C` (to `0x5A2EAB`) | 336 (build 94, refresh 241, set-up 1) |
| `0x5A32B0` | `D3d_BuildCellTexture` | `0x4DB` (to `0x5A378A`) | 94 |
| `0x5A3790` | `D3d_FreeCellTexture` | `0x3E` (to `0x5A37CD`) | 94 |
| `0x5A37D0` | `D3d_RefreshCellTexture` | `0x28A` (to `0x5A3A59`) | 241 |
| `0x5A9E1E` | `Font_UnpackGlyph` | `0x220` (to `0x5AA03D`) | 6,005 |
| `0x5AA03E` | `Cell_Unpack4` | `0x1A2` (to `0x5AA1DF`) | not reached |
| `0x5AA1E0` | `Cell_Unpack8` | `0x116` (to `0x5AA2F5`) | 1,570 |
| `0x5AA2F6` | `Cell_Unpack4Flip` | `0x1B8` (to `0x5AA4AD`) | not reached |
| `0x5AA4AE` | `Cell_Unpack8Flip` | `0x128` (to `0x5AA5D5`) | 110 |

Sizes are to the byte after the last instruction; each ends in its `ret`
with `nop` padding or the next function after it, and every jump of each is
inside it. `analysis/calltrace/entries.txt` already has all eleven at these
sizes (checked 2026-09-23), the two 4-bit unpackers included. The five
unpackers are hand-written assembly packed end to end (an `ebp` frame,
`lodsd`, `shld`, byte counters in `ch` / `cl`), cdecl-callable: they save
every register but `eax` and `edx`. Other callers, outside this group:
`D3d_FitTextureSize` from `D3d_AfterDraw` `0x59F603`, `Dd_ClearSurface` from
the Direct3D set-up (`0x5A55FA`, `0x5A56EF`), `D3d_FreeCellTexture` from the
Direct3D teardown `0x5A6760` (all 128 entries).

What the attract sequence reaches (the per-caller counts): only the Direct3D
paths (the trace's callers are return addresses; the call sites are given
here): every glyph build unpacks through the stage (the call at `0x5A2DFB`,
6,005); every cell build is of a never-built entry (`0x5A3233`, 94 - never
the victim's call at `0x5A3259`) and clears the stage (`0x5A3334`, 94 -
never the backdrop); only 8-bit cells, plain (1,570) and mirrored (110). **No 4-bit cell, no software
surface, no backdrop.**

`0x5A32B0` also calls `0x5B9550`, the CRT's `_ftol`: `fnstcw`, round toward
zero in a copy of the control word, `fistp` to 64 bits, `fldcw` back (13
instructions, `0x5B9550..0x5B9576`) - plain x87, no CRT state, so the fuzz's
copy keeps calling it before the CRT is up.

## 2. The glyph

`Font_TexCache` `0x7C9F50` (`symbols.toml`): 128 entries of `0x14` - u16
glyph `+0`, u16 CLUT `+2`, the CLUT row's generation `+4`, the surface `+8`,
the texture `+0xC`, u16 used this frame `+0x10`.

**`Font_BuildGlyphTexture(slot, glyph, clut)`** (`0x5A2CA0`):

1. `Gfx_ClutPixels(clut)` - first, before anything else is read; then the
   glyph at `Font_GlyphData + glyph * 0x120` (the pointer read after that
   call, `0x5A2CD5`), `Dd_InitSurfaceDesc`.
2. **Software surfaces** (`Gfx_RenderFlags` bit 0): if `+8` is 0,
   `Dd_CreatePlainSurface(0x20, 0x20, &+8, 0)` (a failure returns),
   `Dd_ClearSurface` of it, `+0xC = 0`. `Lock` the entry's surface
   (`DDLOCK_WAIT`), `Font_UnpackGlyph(lpSurface, glyph, palette, lPitch)`,
   `Unlock`.
3. **Direct3D**: if `+8` is 0, `Dd_CreateTextureSurface(0x20, 0x20, &+8,
   &+0xC, 0)` (a failure returns) and, under flag bit 5 (re-read),
   `SetColorKey(DDCKEY_SRCBLT, {0, 0})`. `Lock` the staging surface, unpack
   into it, `Unlock`, `Blt` its `{0, 0, 24, 24}` to the same rectangle of
   `+8` (`DDBLT_WAIT`).
4. A failed `Lock` returns with the entry's key words unwritten. Otherwise
   `+0` the glyph's low 16 bits, `+2` the CLUT's, `+4` the dword at
   `Gfx_ClutRows + (clut sar 6) * 8` - signed, on the whole argument.

An entry that already has a surface reuses it as it is - not cleared, not
colour-keyed again (a surface made under the software flag and reused under
Direct3D is Blt into, never locked). The glyph is 24 x 24 in a 32 x 32
texture: on the software path the rest of a new surface is the clear's 0; on
the Direct3D path it is whatever the managed texture starts with (what the draw
samples of the 32 x 32 is group N's, [`glyph-draw.md`](glyph-draw.md)). Every surface pointer is
re-read from the entry or from `Dd_StageSurface` at each use; ours too.

**`Font_UnpackGlyph(dst, glyph, palette, pitch)`** (`0x5A9E1E`): 24 rows of
24 texels, 12 contiguous bytes a row, each dword eight texels low nibble
first through the 16-entry palette, **every** texel written (no
transparency here - a texel whose palette entry is 0 is what the colour key
`{0, 0}` keys out), rows
`pitch` apart. 16-bit entries, or 32-bit when `Gfx_PixelFormat` byte `+3` is
4 (read once). Its four-texel tails (`0x5A9EEB`, `0x5A9FE7`) are dead code:
the column counter starts at 24 and steps 8.

## 3. The cell texture

`D3d_CellTexCache` `0x7CAE38`, 128 entries of `0x28` - as group R laid it
out ([`d3d-draw.md`](d3d-draw.md) §3), now with who writes what:

| offset | what | written by |
|---|---|---|
| `+0`, `+2` | used width, height (u16) | build (`right - left`, `bottom - top`; the texture's width and a scaled height when the texture is narrower) |
| `+4`, `+6` | the texture's width, height | build (Direct3D: the fitted size; software: the used size rounded up to 4) |
| `+8..+0xE` | extent: s16 left, right, top, bottom, texels from the sprite's origin | build |
| `+0x10`, `+0x12` | CLUT, cell count (u16) | build |
| `+0x14`, `+0x16` | used this frame, use counter | `D3d_CellTexture`, the draw walk |
| `+0x18` | checksum: the 32-bit sum of the cells' dwords | build |
| `+0x1C` | the CLUT row's generation | build, refresh |
| `+0x20`, `+0x24` | the surface, its `IDirect3DTexture2` (0 under the software surfaces) | build (through the helpers); `D3d_FreeCellTexture` releases and zeroes |

**The cells.** `SpriteCell_Table` `0x6BEA18` holds 8-byte records
(`SpriteCell_Add`, `symbols.toml`): s16 x `+0`, s8 y `+2`, a size byte
`+3` (w = low nibble x 8, h = high nibble x 8), the tpage word `+4`, u `+6`,
v `+7`. Each is unpacked from the VRAM shadow at the page the tpage names
(column `(tpage & 0xF) << 7` bytes, row block `(tpage & 0x10) << 4`) plus v
rows and u bytes - u halved for 4-bit - by the tpage's colour bits `0x180`:
0 is 4-bit, **anything else 8-bit** (a 15-bit cell would be read as 8-bit),
and its bit `0x200` picks the mirrored unpacker. It goes to `(x + 160, y +
128)` of the staging surface - the stage's centre is the sprite's origin -
at the pitch and depth (byte `0x7DED63`, re-read per record) of the moment.
Nothing bounds it: D32.

**`D3d_BuildCellTexture(slot, first, count, clut, backdrop)`** (`0x5A32B0`),
`backdrop` being `D3d_CellTexture`'s flags `& 0x800`:

1. The canvas. Unless `Gfx_RenderFlags` bit 0 (read once, on entry), with
   `backdrop` non-zero and a surface at `Dd_CellBackdrop` `0x6BEA00`: `Blt`
   of that surface's `{0, 0, 320, 240}` to the stage's `{0, 8, 320, 248}`.
   Otherwise `Dd_ClearSurface(stage)`. What the backdrop surface holds is not
   read (only the teardown `0x5A661A` also names it).
2. `Lock` the stage (a failure returns, the entry untouched);
   `Gfx_ClutPixels(clut)`; the records from `first` while `(int) first <
   (int) (first + count)` - a signed test, so a sum that wraps negative draws
   nothing; each one's fields are read before its unpacker runs, and after
   it the extent grows to hold `(x, y)..(x + w, y + h)` (signed; from `(0, 0)`,
   so it always holds the origin) and the checksum adds the record's two
   dwords, read then. `Unlock`.
3. `D3d_FreeCellTexture(slot)`, then the entry: the extent, the used size,
   the count and CLUT words, the checksum, and the generation of
   `Gfx_ClutRows + (clut shr 6) * 8` - **unsigned** here, where the glyph
   shifted signed.
4. The flag byte re-read. **Software**: `Dd_CreatePlainSurface(used w, used
   h, &+0x20, 0)` (a failure returns), `Blt` of the stage's `{left + 160,
   top + 128, right + 160, bottom + 128}` to `{0, 0, used w, used h}`, `+4` /
   `+6` = the used size (re-read) `+ 3 & 0xFFFC` in 16 bits. **Direct3D**:
   device `EndScene`; `D3d_FitTextureSize` of the used size (re-read);
   `Dd_CreateTextureSurface` of the fitted size into `+0x20` / `+0x24` - a
   failure returns **without `BeginScene`** (D31); if the fitted width is
   below the used width (signed), the used height becomes `fild tw; fidiv
   uw; fimul uh` through `_ftol` (x87, at the control word's precision) and
   the used width the texture's, both stored, and the `Blt`'s destination
   becomes `{0, 0, those}` - a stretch; the same `Blt` into `+0x20`; `+4` /
   `+6` the fitted size; device `BeginScene`. Only the width is tested: a
   texture fitted shorter than the used height gets a destination taller than
   itself, which `Blt` refuses.

The entry is written before the texture exists, so a failure after step 3
leaves it looking built (D31).

**`D3d_RefreshCellTexture(slot, first, count, clut)`** (`0x5A37D0`), for a
hit whose CLUT row's generation moved: `Dd_ClearSurface(stage)` - **always**,
never the backdrop, whatever the build drew on - `Lock`, `Gfx_ClutPixels`,
the records drawn as the build draws them but with no extent or checksum,
`Unlock`, `+0x1C` the new generation; then the build's `Blt` from the
entry's stored extent (s16, `+ 160` / `+ 128`, read after that store) to
`{0, 0, +0, +2}` into `+0x20`: at once under the software flag (read after
the `Unlock`), else between device `EndScene` and `BeginScene` with `+0x20`
read after the `EndScene`. `+0x20` is never tested (D31). The cells are
re-read from `SpriteCell_Table`, which is only right because the caller hit
on their checksum this frame.

**`D3d_FreeCellTexture(slot)`** (`0x5A3790`): `Release` (`+8`) of `+0x24`
if set, then of `+0x20` (read after) if set; the 0x28 bytes zeroed. No bound.

**`Dd_ClearSurface(surface)`** (`0x5A2E70`): a zeroed `DDBLTFX` (0x64 bytes,
`dwSize` 0x64, `dwFillColor` 0), `Blt(NULL, NULL, NULL, DDBLT_COLORFILL |
DDBLT_WAIT, &fx)`; returns the `HRESULT`, which no caller reads.

**`D3d_FitTextureSize(w, h, &out_w, &out_h)`** (`0x59F9B0`), from
**`D3d_DeviceDesc`** `0x7CC238` - a DirectX 6 `D3DDEVICEDESC`, by the offsets
it reads (a hypothesis in `symbols.toml`; its filler is not read): the low
byte of the triangle caps' `dwTextureCaps` (`0x7CC2BC`, read once). Bit 1
(`D3DPTEXTURECAPS_POW2`): each side is the device's minimum
(`dwMinTextureWidth` / `Height` at `0x7CC2E4` / `0x7CC2E8`, 1 for 0) doubled
until it is not below the side - signed compares, so a side above
`0x40000000` never ends and a negative one keeps the minimum. Else the sides
as given. Each then clamped **unsigned** to the maximum (`0x7CC2EC` /
`0x7CC2F0`), and under bit 5 (`SQUAREONLY`) with the two unequal, both made
the larger, **signed**. `*out_w` is written before `*out_h`.

## 4. The unpackers

`Cell_Unpack4` / `Cell_Unpack8` / `Cell_Unpack4Flip` / `Cell_Unpack8Flip`
`(dst, src, palette, w, h, pitch)`: h rows of w texels, source rows 0x800
bytes apart, destination rows `pitch` apart; 4-bit texels low nibble first,
8-bit low byte first, each through the palette, and **written only when the
entry is non-zero** - 0 is transparent, the destination keeps what the clear
or the backdrop put there (tested on the 16-bit entry, or the whole 32-bit
one). The mirrored ones start at the row's last dword, `src + (w shr 1) - 4`
or `src + w - 4`, and walk back, each dword's texels high first. The texel
count steps 8 or 4 and stops only at exactly 0; the row count is a `dec` /
`jne`: a size nibble of 0 (w or h 0) runs the original away through memory,
and ours does the same arithmetic. `SpriteCell_Add` packs the byte from the
piece's w and h (`symbols.toml`), so only a piece under 8 texels would (or
one 128 or more wide, whose `w >> 3` carries into the height nibble).

## 5. What changed in shared code

- `src/game/ddraw_fuzz.*` (group T's fake): `Blt` records its `DDBLTFX` by
  content (an FNV-1a hash of its 0x64 bytes, 0 for none - it was the
  pointer, a stack address that differs between Capcom's copy and ours) and
  performs `DDBLT_COLORFILL`, refusing an `fx` whose `dwSize` is not 0x64 as
  DirectDraw does; the log holds 128 calls (was 40 - a cell texture of 100
  cells makes about 110); `TextureOf(surface)` hands the fuzz a surface's
  texture without recording a call. `tex_page`'s fuzz passes no `fx`: its
  self-test passes with the coverage counts [`tex-page.md`](tex-page.md) §6
  gives, unchanged.
- `CMakeLists.txt`, `inject_all.cpp` (`TexCells_Inject` last), and
  `symbols.toml` (one block at its end: the eleven, `D3d_DeviceDesc`,
  `Dd_CellBackdrop`; the three existing entries moved into it with `impl`,
  `D3d_BuildCellTexture`'s last parameter renamed `backdrop` - it was
  `flip`, and the mirroring is per cell, bit `0x200`).

## 6. The fuzz and the controls

`BOF3X_SHADOW=tex_cells`, one start-up run. Clones: the five unpackers as
one block (`0x5A9E1E..0x5AA5D6`, no calls); `D3d_FitTextureSize` alone;
`Font_BuildGlyphTexture` with `Dd_ClearSurface` after it as one block (the
build's call of the clear stays inside); both cell builders with
`D3d_FreeCellTexture` between them as one block (the build's call of the
free stays inside). The builders' calls are re-aimed: the unpackers at
recorders that log the call (where it writes, as surface and offset; where
it reads, as an offset into VRAM or the glyph tables; which palette; w, h,
pitch) and run the unpacker block's copy, `Gfx_ClutPixels` at a stand-in,
the clear and the fit at their copies; the surface helpers (ours, group T)
and `_ftol` where the original called. Ours runs on recorders that log alike
and run ours - only when its call matches Capcom's at the same place (T's
guard) - and calls our helpers by name. So a builder round compares
Capcom's whole tree against ours. The device at `D3d_Device` is a fake of
the fuzz's own whose `BeginScene` / `EndScene` record into the same log (any
other method ends the run naming it); `Dd_DirectDraw` holds T's fake.

Compared each round: the log (every COM call with its arguments, the scene
calls, every unpacker call, every CLUT lookup), each `CreateSurface`'s
descriptor, every byte of every fake surface's buffer, every byte of state
either side could touch (`Font_TexCache` and entry 128 over the vertex block,
`D3d_CellTexCache` with the entries past it and `D3d_DeviceDesc`,
`SpriteCell_Table` and the dwords before it, `Gfx_ClutRows`,
`Gfx_RenderFlags`, the pixel formats, the DirectDraw globals and
`Font_GlyphData`), and the return. The unpackers and the fit are also fuzzed
directly on buffers, the clear and the free on the fakes.

The disturbances: after a quarter of the calls a fake method, a recorder or
the CLUT stand-in flips `Gfx_RenderFlags` bit 0 or 5, moves
`Dd_StageSurface` to the other stage, sets the depth byte to 2 or 4, rewrites
the round's CLUT row generation; for the glyph, moves `Font_GlyphData` to the
other glyph table, the entry's surface (to 0 only during `Gfx_ClutPixels`,
before the test) and texture; for the cells, rewrites a record of the round,
the entry's surface, texture, used size and extent, or the device's caps -
always to values the original can take. What a disturbance draws comes from
a generator seeded by the call's hash, so both passes are disturbed alike
(the first run drew from the round's generator and every Direct3D build
differed - the fuzz's bug, found by its first mismatch).

**Rounds and coverage** (the last run): the unpackers 6,000 (1,200 each; 439
glyph and 1,627 cell rounds at 4 bytes a texel); `D3d_FitTextureSize`
20,000 (8,160 square-only); `Dd_ClearSurface` 1,000; `D3d_FreeCellTexture`
2,000; `Font_BuildGlyphTexture` 4,000 (software 2,047, Direct3D 1,953,
surface made 1,994, reused 2,006, colour-keyed 490, slot 128 281,
`CreateSurface` failed 337, `Lock` failed 1,224); `D3d_BuildCellTexture`
5,000 (software 2,488, Direct3D 2,512, backdrop 1,153; 0 cells 520, 1 1,311,
2..24 2,671, 100 cells 498, a first before the table or wrapping 1,023,
pieces spilling past the stage 294; 40,241 mirrored and 60,390 8-bit cells;
scaled 970, 686 of the rounds with a precision-sensitive size seeded (below),
`CreateSurface` failed 673, `Lock` failed 992) under the control
words `0x027F`, `0x007F` and `0x037F`; `D3d_RefreshCellTexture` 4,000
(software 2,002, Direct3D 1,998, 0 cells 416, 1 1,033, 100 cells 381, `Lock`
failed 801). Seeded: the depth byte 2, 4, 3, 0 and random; pitches from 4 to
2,048, odd ones and negative ones (the unpackers); widths 8..248 (4-bit),
4..252 (8-bit), heights 1..128; palettes a third 0, some 0 only in one half;
glyphs 0, 1 and the last two of each 64-glyph table; CLUTs 0, 1, `0x3F`,
`0x40`, `0x7FC0`, `0xFFFF`, `0x10000`, `0x1FFC0` and, for the glyph,
`0xFFFFFFC0`, `0xFFFFF000`, `0xFFFF0000` (the signed shift); slots 0, 1,
126, 127, 128 (and 129 for the cells: entries past the table, over
`D3d_DeviceDesc`); first 0, the last possible, `0xFFFFFFFE` / `0xFFFFFFFF`
(records -2 and -1, before the table) and `0x7FFFFFFF` with 1 (the sum wraps
negative); counts 0, 1, 100; size nibbles 1 and 15; caps bits 1 and 5 and
random bytes; minima 0..16; maxima 8..`0x800`, `0xFFFFFFFF` and random 1..400
(so the width is clamped below the extent - the scaled path); sides up to
`0x40000000` and negative ones for the fit, and aliased outputs. Result:
**0 mismatches**, and every other module's self-test still passes
(`BOF3X_SHADOW='*'`, 594 ours).

**The x87's precision, seeded.** The scaled height (`fild` / `fidiv` /
`fimul`, then `_ftol`) rounds at the control word's precision, and ours does
it in inline assembly for that reason. Random extents almost never tell a
wrong precision apart: about one (texture width, used width, used height)
triple in 2,400 truncates differently at 24 bits than in doubles (numpy's
float32 over every width below 420 and height below 300, the scratchpad,
2026-09-23; the 64-bit triples found by exact rational arithmetic), and the first
run of the controls let "the scaled height in C++ doubles" through (the
compiler does `double` in SSE2, at 53 bits whatever the control word). So the
fake device's `EndScene` - just before the build reads the used size and fits
the texture - sets, a third of the time, a used size and a maximum texture
width from 18 such triples at 24 bits (under `0x007F`) or 21 at 64 bits
(under `0x027F` and `0x037F`). The control is refused since.

Two guards, T's: the fuzz **stops at the first mismatch** (a `Fatal` naming
it), and our pass's recorders run an unpacker only when the call they just
logged is the one Capcom's copy made at the same place.

**Negative controls: 55 planted, one at a time, by a script that edits
`tex_cells.cpp`, builds, runs the headless self-test with
`BOF3X_SHADOW=tex_cells` and restores** (the scratchpad's `controls.py`; run
twice, the second time on the final fuzz). 51 were refused by a comparison -
the pixels, a call's arguments, the call count, a descriptor, memory - and
none by a hang; one was refused by a fault; three were changes that change
nothing; and one planted as such (53) was refused:

- **22, the glyph's generation row shifted unsigned**, faults: the two shifts
  differ only for a CLUT argument of `0x80000000` or more (seeded:
  `0xFFFFFFC0`, `0xFFFFF000`, `0xFFFF0000`), where the unsigned one reads
  `0x6C2A40 + 0x1FFFFFF8` and beyond, outside the image, before anything can
  be compared. Capcom's reads inside it. As T's control 27 was: refused, by the
  fault (`0xC0000005`), not by a count.
- **51, 52, 54, changes that change nothing**, not refused, as expected: the
  glyph's `+0` stored as a dword (its upper half is `+2`, stored next with no
  call between); the cell build's generation row shifted signed (differs only
  for a CLUT of `0x80000000` or more, where Capcom's faults - section 7);
  `D3d_FitTextureSize` squaring when the sides are already equal.
- **53 was planted as one and is not**: scaling also when the fitted width
  equals the used width computes the same size, but the scaled path builds the
  `Blt`'s destination from the used size re-read after `EndScene`, where the
  unscaled one uses the size read before it - the disturbance at `EndScene`
  tells them apart. Refused by `Blt`'s destination; the claim "changes
  nothing" was wrong, and the doc says so rather than dropping the control.

| # | control | refused by |
|---|---|---|
| 0 | glyph unpack: high nibble first | the pixels |
| 1 | glyph unpack: 23 rows | the pixels |
| 2 | glyph unpack: 32-bit on a depth byte `>= 4` | the pixels (depth byte 174) |
| 3 | glyph unpack: zero entries transparent | the pixels |
| 4 | cell unpack: 16-bit transparency tested on 32 bits | the pixels |
| 5 | cell unpack: 32-bit transparency tested on 16 bits | the pixels |
| 6 | cell unpack: 4-bit mirrored start at `w / 4` | the pixels |
| 7 | cell unpack: mirrored keeps the texel order | the pixels |
| 8 | cell unpack: rows `0x400` apart | the pixels |
| 9 | cell unpack: every texel written | the pixels |
| 10 | fit: maximum width clamp signed | the size |
| 11 | fit: square takes the smaller side | the size |
| 12 | fit: POW2 on bit 2 | the size |
| 13 | fit: `*out_h` stored first | the size (aliased outputs) |
| 14 | fit: minimum height from the width's | the size |
| 15 | clear: fill colour 1 | `Blt`'s `DDBLTFX` |
| 16 | clear: no `DDBLT_WAIT` | `Blt`'s flags |
| 17 | free: surface released first | call 0 |
| 18 | free: `0x24` bytes zeroed | memory |
| 19 | glyph: `Font_GlyphData` read before `Gfx_ClutPixels` | the unpacker's source |
| 20 | glyph: `+0xC` not zeroed (software) | memory |
| 21 | glyph: colour key on flag bit 4 | the call count |
| 22 | glyph: generation row unsigned | a fault (above) |
| 23 | glyph: `Blt` of 32 x 32 | `Blt`'s rectangle |
| 24 | glyph: glyphs `0x100` bytes apart | the unpacker's source |
| 25 | glyph: an existing surface cleared too (software) | the call count |
| 26 | glyph: `Blt` surface read before the `Unlock` | `Blt`'s surface |
| 27 | cells: loop test unsigned | the call count |
| 28 | cells: extent not from the origin | `Blt`'s rectangle |
| 29 | cells: checksum of one dword | memory |
| 30 | cells: extent from x re-read after the unpack | `Blt`'s rectangle |
| 31 | cells: x offset `0x9F` | the unpacker's destination |
| 32 | cells: 4-bit u not halved | the unpacker's source |
| 33 | cells: mode on bit 8 only | the unpacker called |
| 34 | cells: mirror on bit 10 | the unpacker called |
| 35 | cells: page row `shl 3` | the unpacker's source |
| 36 | cells: depth byte read once, on entry | the unpacker's destination |
| 37 | build: count word stored as the CLUT | memory |
| 38 | build: flag byte not re-read | the call count |
| 39 | build: software size rounded down | memory |
| 40 | build: `BeginScene` after a failed create (the D31 fix) | the call count |
| 41 | build: scaled height in C++ doubles | `Blt`'s rectangle (after the seeding above) |
| 42 | build: backdrop to y 0 | `Blt`'s rectangle |
| 43 | build: backdrop under the software surfaces too | call 0 |
| 44 | build: entry freed after the stores | `Blt`'s rectangle |
| 45 | build: stage read once for `Lock` and `Unlock` | `Unlock`'s surface |
| 46 | build: fit handed the size read before `EndScene` | the descriptor |
| 47 | refresh: surface read before `EndScene` | `Blt`'s surface |
| 48 | refresh: extent read unsigned | `Blt`'s source rectangle |
| 49 | refresh: software on flag bit 1 | the call count |
| 50 | refresh: the stage not cleared | the call count |
| 51 | glyph: `+0` stored as a dword | nothing, as expected |
| 52 | build: generation row signed | nothing, as expected |
| 53 | build: scale also when the widths are equal | `Blt`'s rectangle (above) |
| 54 | fit: square also when the sides are equal | nothing, as expected |

## 7. What none of it reached

- **The real DirectDraw and Direct3D.** The fake answers as the
  documentation says, `Blt` copies only unstretched same-depth rectangles
  (the scaled path's stretch is compared as a call, not as pixels), and the
  fake device only records its scene calls. The live batch is the check:
  every glyph and every field sprite of the attract sequence is drawn from a
  texture these functions built.
- **4-bit cells, the software surfaces, the backdrop, the victim build and
  the scaled texture**: the fuzz runs all of them; the attract sequence none
  (section 1).
- **The game's real cell layouts** (D32) and texture caps: whether any sprite
  has a piece outside the stage, and whether the display's device ever fits
  a texture narrower than a sprite, are unmeasured - logging the extent and
  `D3d_FitTextureSize`'s answer at each build would say.
- **The generation shift of the cell builders** (`shr`) differs from a
  signed one only for a CLUT argument of `0x80000000` or more, where the
  original reads outside the image and faults; the callers pass u16. A
  control that makes it signed is a change the fuzz cannot observe safely.
- `_ftol` is Capcom's on both sides of the scaled rounds; ours is its
  instruction sequence in inline assembly, checked against it under three
  precisions.

**What would show a wrong one, live.** The renderer is excluded from the
frame hash, so the batch checks this group by A/B captures (`BOF3X_ORIGINAL`
with these eleven names against ours). A wrong glyph build or unpacker shows
as wrong or garbled text - every message box of the attract sequence; a wrong
cell build as sprites cut, shifted, mirrored the wrong way, holed (a
transparency test inverted) or in the wrong palette; a wrong refresh as a
sprite that keeps its old colours after a palette change; a wrong fit or
free as sprites missing (no texture) or a leak that nothing shows.
