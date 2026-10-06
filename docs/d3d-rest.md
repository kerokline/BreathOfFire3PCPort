# The renderer's live remainder: the last Direct3D handlers, the walk's two other callees, and the VRAM read-back

**Status:** IN PROGRESS (2026-10-05 - the platform round's group PH, step 2
of [`platform-layers-plan.md`](platform-layers-plan.md) section 4: ten
functions ours in `src/game/d3d_rest.cpp`, each read to its last instruction,
fuzzed headless against copies of Capcom's under `BOF3X_SHADOW=d3d_rest`
(0 mismatches) with 39 negative controls, every one refused on the final
fuzz; faithful, no divergence; five latent defects of Capcom's written down
(section 4), one of them, TILE_1's single point, in a recorded route. Not yet through the state
hash or a live run - the coordinator's, after the merge.)

[`platform-read-pass.md`](platform-read-pass.md) section 2 found eight
starts of Capcom's renderer still alive under ours: the five Direct3D
primitive handlers of `Gfx_DrawOTag`'s second table that no earlier group
took, the two starts under POLY_FT3, and the two other callees of the walk.
This group took them, and the one row of the renderer's range the read pass
left unplaced, `0x59E930` (section 6, question 1). The models were
[`d3d-draw.md`](d3d-draw.md) (group R's six handlers and the vertex-block
fuzz) and [`battle_draw.md`](battle_draw.md) (the Gouraud handlers).

Every claim about the binary below is from capstone over `bof3/BOF3.exe` on
2026-10-05 (the scratchpad's `phdis.py`, the same reads `tools/pe_disasm.py`
gives; `scan.py` for raw immediates and E8 / E9 targets) unless it says
otherwise. None has a PSX twin: the PlayStation drew primitives on its GPU,
and its StoreImage is libgpu's, not this code.

## 1. The functions

| PC | name | true size | code | caller (E8 / E9 scan) |
|---|---|---|---|---|
| `0x59E930` | `Gfx_StoreImage` | `0x6C` (to `0x59E99B`) | - | four effect states of ours (section 6) |
| `0x59F520` | `D3d_SetAlphaModulate` | `0x53` (to `0x59F572`) | `0xF4` | `Gfx_DrawOTag` `0x59F23F` |
| `0x59F580` | `D3d_AfterDraw` | `0x2BF` (to `0x59F83E`) | - | `Gfx_DrawOTag`'s tail `jmp` `0x59F29A` |
| `0x59FA50` | `D3d_DrawPolyF3` | `0x14C` (to `0x59FB9B`) | `0x20` POLY_F3 | the walk, `0x59F0AA` |
| `0x59FDB0` | `D3d_DrawPolyFT3` | `0x226` (to `0x59FFD5`) | `0x24` POLY_FT3 | the walk, `0x59F0B8` |
| `0x5A0910` | `D3d_FlattenFT3` | `0x121` (to `0x5A0A30`) | - | `D3d_DrawPolyFT3` `0x59FDCB` |
| `0x5A0A40` | `D3d_PageTexel4` | `0x6A` (to `0x5A0AA9`) | - | `D3d_FlattenFT3` `0x5A093A` |
| `0x5A1050` | `D3d_DrawPolyGT3` | `0x239` (to `0x5A1288`) | `0x34` POLY_GT3 | the walk, `0x59F0F0` |
| `0x5A1EA0` | `D3d_DrawLineG4` | `0x22F` (to `0x5A20CE`) | `0x5C` LINE_G4 | the walk, `0x59F160` |
| `0x5A2220` | `D3d_DrawTile1` | `0xD5` (to `0x5A22F4`) | `0x68` TILE_1 | the walk, `0x59F17C` |

Sizes are to the byte after the last instruction; the read pass's catalogue
sizes include the padding to the next start (96 for `0x59F520`). **Every
start is a function**: `0x5A0910` and `0x5A0A40` are POLY_FT3's helpers - a
one-texel triangle's colour and the texel lookup under it - not variants of
the handler or second entries. Every jump is internal; the calls out are the
E8s the fuzz re-aims (section 7) and the COM calls below.

Windows / COM calls, all through the DirectX 6 objects that are ours since
DIV-0031 ([`render-backend.md`](render-backend.md) section 1), so ours calls
the same slots with the same arguments that Capcom's handler called:

| function | calls | the fuzz's stand-in |
|---|---|---|
| the five handlers | `IDirect3DDevice3::SetTexture` (`+0x98`, F3 / LINE_G4 / TILE_1 and a flattened FT3), `DrawPrimitive` (`+0x70`) | `d3d_fuzz.h`'s fake device (DrawPrimitive snapshots the vertices) |
| `D3d_SetAlphaModulate` | `SetTextureStageState` (`+0xA0`) | the same fake device |
| `D3d_AfterDraw` | `IDirectDrawSurface4::Lock` (`+0x64`), `Unlock` (`+0x80`), `Blt` (`+0x14`); `CreateSurface` and `QueryInterface` only inside `Dd_CreatePlainSurface` / `Dd_CreateTextureSurface` (ours, `tex_page.cpp`) | six fake surfaces of `d3d_rest_fuzz.cpp`'s own (two capture surfaces, two back buffers, two texture surfaces), every other slot a trap; the four creation helpers recorders |
| `D3d_FlattenFT3` | the CRT's `_ftol` `0x5B9550` three times | none: the copies call the CRT's, ours inlines its sequence (round toward zero in a copy of the control word, `fistp` to 64 bits, the low dword - `area_w3g.cpp`'s `Ftol`) |

## 2. The handlers

Common to all, as in [`d3d-draw.md`](d3d-draw.md) section 3: the vertices are
the `D3DTLVERTEX` at `D3d_Vertices` `0x7CA958`; `sx`, `sy` = `fld scale; fmul
x` (no half-pixel offset), `sz` the z by `mov`, `rhw = 0.1 / z` (`0x5C4610`);
the last call is `DrawPrimitive(type, 0x1C4, D3d_Vertices, n, 0)` and its
result is returned (the walk does not read it). Not kept, in all of them: the
colour helper's diffuse written into the caller's pushed `prim` slot, which
the walk pops unread. The E8 offsets below are what the fuzz re-aims.

- **`D3d_DrawPolyF3`** (`POLY_F3`, read to `+0x2B`): `D3d_PrimColor(r, g, b
  +4..+6, code +7, Gfx_DrawTpage & 0xFFFF, &diffuse, NULL)` (`+0x31`); three
  corners of `0xC` from `+8` (x, y, z), position and diffuse - specular,
  `tu`, `tv` left as the last draw left them; `SetTexture(0, NULL)`;
  `0x437CC0(0)` twice (`+0x101`, `+0x108`); `D3d_SetBlend(code,
  Gfx_DrawTpage re-read)` (`+0x120`); `D3d_SetShadeMode(1)` (`+0x127`);
  `D3DPT_TRIANGLELIST` (4) of 3.
- **`D3d_DrawPolyFT3`** (`POLY_FT3`, to `+0x37`): corners of `0x10` from `+8`:
  float x, y, z, byte u, v; CLUT word `+0x16`, tpage word `+0x26` - the
  layout `POLY_FT4` has, and the one the game's builders write (`magic_s22.cpp`'s
  `0x4C9290` sets `+0x26` from `Gpu_GetTPage` and `+0x16` from `Gpu_GetClut`
  after `0x5A7590`, libgpu `SetPolyFT3`). If the three (u, v) words `+0x14`,
  `+0x24`, `+0x34` are equal (16-bit compares; the second only if the first
  holds), **`D3d_FlattenFT3(prim, that word)`** first (`+0x1B`). Then
  `D3d_PrimColor(r, g, b, code, tpage, &diffuse, &specular)` (`+0x51`); three
  corners, position, diffuse, specular; **`0x7CA9C0` - the fourth vertex's
  `sz` - set to 0**, as `D3d_DrawPolyG3` does; flattened: `SetTexture(0,
  NULL)` and no texture coordinates; else `tu`, `tv` = `D3d_TexCoords[u]`,
  `[v]` through `fld` / `fstp` per corner and `D3d_BindTexture(tpage, CLUT)`
  (`+0x1B0`); `0x437CC0(tpage & 0x400)`, `(tpage & 0x800)` (`+0x1D6`,
  `+0x1E6`); the blend (`+0x1F8`); flat (`+0x1FF`); a list of 3. The tpage
  word is read afresh after every call.
- **`D3d_DrawPolyGT3`** (`POLY_GT3`, to `+0x3D`): `D3d_DrawPolyGT4` with
  three corners: corners of `0x14` from `+4` (r, g, b, pad, float x, y, z, u,
  v), CLUT `+0x16`, tpage `+0x2A`; three colour calls with a specular each
  (`+0x31`, `+0x5F`, `+0x8D`) into locals, all before any vertex; per corner
  position, pair, texel; `D3d_BindTexture` (`+0x1EA`); `0x437CC0(0)` twice;
  the blend; Gouraud (2); a list of 3.
- **`D3d_DrawLineG4`** (`LINE_G4`, to `+0x43`): `D3d_DrawLineG3`
  ([`battle_draw.md`](battle_draw.md)) with four corners: four colour calls
  (r, g, b at `+4 + i * 0x10`, the code and `Gfx_DrawTpage` re-read for each,
  null specular) into locals; four corners of `0x10` from `+8`; `SetTexture(0,
  NULL)`; `0x437CC0(0)`, `(1)`; the blend; Gouraud; `D3DPT_LINESTRIP` (3)
  of 4.
- **`D3d_DrawTile1`** (`TILE_1`, to `+0x13`): one colour (null specular); one
  vertex from `+8`; `SetTexture(0, NULL)`; `0x437CC0(1)` twice; the blend;
  flat; **`D3DPT_POINTLIST` (1) of 1** - see D-a in section 4.

## 3. POLY_FT3's two helpers

**`D3d_FlattenFT3(prim, uv)`** `0x5A0910`. A triangle whose three corners
share one (u, v) samples a single texel, so it is drawn untextured in that
texel's colour:

1. `pixel = D3d_PageTexel4(word +0xE, word +0x16, uv & 0xFFFF)` - see D-b.
2. The channels by `Gfx_PixelFormat` `0x7DED60`'s masks and shifts (the
   inverse of `Gfx_PackRgb`): green `((mask +0x14 & pixel) << shift +8) >>
   16`, blue (`+0x18`, `+0xC`), red (`+0x10`, `+4`); each shift by `cl` (mod
   32). For 1-5-5-5 (shifts 9, 14, 19) each 5-bit field lands at bits 3..7.
3. Each of the primitive's r, g, b times `1/128` (`0x5C4614`) times its
   channel, x87 - red `fild qword` of the channel, `fild` r, `fmul`, `fmulp`;
   green and blue `fild`, `fmul`, `fimul` - through `_ftol`, clamped at
   `0xFF` (unsigned `jbe`), **written back over the primitive**, each after the
   next byte is read. The PSX's texture modulation (`0x80` = 1.0).
4. The code byte `+7 &= 0xFB` - bit 2, textured, cleared: the primitive is a
   POLY_F3 (`0x20`) from now on (D-c).

`eax` at its return is a byte of the code; its caller does not read it.

**`D3d_PageTexel4(clut, page, uv)`** `0x5A0A40`. The 4-bit texel at (u, v) of
a texture page of `Gfx_VramShadow`, through a CLUT: the byte at
`Gfx_VramShadow + ((((page & 0x10) << 4) + (uv sar 8)) * 16 + (page & 0xF)) *
128 + ((uv sar 1) & 0x7F)` - page x in 64-cell units, page y 256 rows when
bit `0x10`; the nibble `(byte >> ((~uv & 1) * 4)) & 0xF` - **the high nibble
for an even u** (D-b); then `Gfx_ClutPixels(clut)` (ours, `gfx_clut.cpp`; the
E8 at `+0x48`), indexed by the nibble: a `u16` when `Gfx_PixelFormat`'s bytes
per texel (`0x7DED63`, read after the call) is 2, else a dword. Always 4-bit:
the tpage's colour mode is not looked at.

## 4. Defects found, all latent, all kept

Described, never fixed (the brief's rule and [`known-defects.md`](known-defects.md)'s
model; not numbered there - the coordinator's). Ours keeps every one; a fix
would be a ledger entry, proposed in the report.

- **D-a - TILE_1 is one point at any scale.** `D3d_DrawTile1` draws a
  `D3DPT_POINTLIST` of one vertex. A PSX TILE_1 is one pixel of 320 x 240; at
  `D3d_ScaleX/Y` = 2 the port's frame has four pixels for it and the point
  lights one - a quarter of the area, at the scaled position's top-left. The
  backend draws points as points ([`render-backend.md`](render-backend.md)
  section 2), so it is the same under ours. **The one defect here a recorded
  route shows**: `Gpu_SetTile1` `0x5A7750` builds them for battle motes
  (`0x4E31C0`, `0x4E68B0` in `symbols.toml`) and for magic (`0x4ABB70`), and
  the `whelpBoss` route enters the handler. Proposed, not built: a TILE_1
  drawn as a `D3d_ScaleX` x `D3d_ScaleY` quad (a strip of four at (x, y),
  (x + 1, y), (x, y + 1), (x + 1, y + 1) scaled, as `D3d_DrawTile` draws a
  TILE of w = h = 1) - the owner's eye decides, as for D17 and D1; at the
  backend's other scales the gap grows with k. **Built 2026-10-06 as DIV-0077**
  at the owner's word, off a zoom of the `whelpBoss` route's frame 11880 (the
  dream scene's specks): on by default, `BOF3X_TILE1=0` the point.
- **D-b - a one-texel POLY_FT3 takes its colour from the wrong place, twice
  over.** `D3d_FlattenFT3` passes the words `+0xE` and `+0x16` as the CLUT and
  the tpage: the PSX's POLY_FT3 offsets (u0 v0 clut at `+0xC`, u1 v1 tpage at
  `+0x14`), not this port's float layout, in which `+0xE` is the high half of
  corner 0's float y and `+0x16` is the CLUT - the handler's own
  `D3d_BindTexture(+0x26, +0x16)` and the builders say so (section 2). So the
  page is read from the CLUT word and the palette from a float's high half.
  And `D3d_PageTexel4` takes the high nibble for an even u, where
  `Tex_Convert4` `0x5A9A59` - which builds every 4-bit page texture the other
  handlers bind - reads the low nibble first (`symbols.toml`, 2026-09-23): the
  neighbouring texel. It also assumes 4-bit whatever the tpage's mode. A
  triangle of one texel would come out a wrong flat colour; whether any game
  code builds one is not established (the one builder read, `0x4C9290`, gives
  its three corners different (u, v)).
- **D-c - the flattening rewrites the primitive.** r, g, b are replaced by the
  modulated colour and the code becomes `0x20`. Drawn again without being
  rebuilt (an ordering table kept across frames), the primitive is a POLY_F3 of
  the already-modulated colour - the same colour, untextured, so no visible
  change; noted because a later reader of the primitive (a fuzz, a capture)
  sees the code change.
- **D-d - `D3d_AfterDraw` leaves its capture surface locked when the back
  buffer will not lock**, and ignores the capture surface's pitch (it packs 320
  pixels a row, which is the pitch only when the surface's rows are exactly
  that wide). Unreachable: nothing requests the capture (section 5).
- **D-e - `Gfx_StoreImage` checks no bounds**: a rectangle outside the
  1024 x 512 shadow reads past it, and a negative w is a copy of about 4 GB
  (`shr ecx, 2` of a negative byte count). Its four callers pass
  `(0x340, 0x100, w, h)` of an effect's frame.
- **The far texture edge (DIV-0010, D1; d3d-draw.md's D27 / D28 class).**
  `D3d_DrawPolyFT3` and `D3d_DrawPolyGT3` read `D3d_TexCoords[u]` per corner
  with no `w - 1` arithmetic of their own, exactly as `POLY_FT4` / `POLY_GT4`
  do ([`d3d-draw.md`](d3d-draw.md) section 6's last paragraph): whether D1's
  slip appears depends on the far u the builder puts in the primitive
  (`0x4C9290` puts `u + 0x3F` for a 64-texel strip, the last texel's index),
  not on these handlers. Not established; nothing changed. Whether DIV-0010
  extends to them is the owner's decision. No cell texture here, so D27 /
  D28 do not arise.

## 5. `D3d_AfterDraw`: a capture nothing asks for

Read to its last instruction (it agrees with
[`display-setup.md`](display-setup.md) section 7's reading, which this
completes): under `Gfx_RenderFlags` bit 0 nothing. On first use - the capture
surface `D3d_CaptureSurface` `0x7CADF8` null - the scale `0x7CADF4` = 1.0,
the used size words `0x7CADEC` / `0x7CADEE` = 320 / 240;
`Dd_CreatePlainSurface(0x140, 0x140, &D3d_CaptureSurface, 2)` (the screen's
format); `D3d_FitTextureSize(0x140, 0xF0, 0x7CADF0, 0x7CADF2)` - dwords that
overlap, so the height's store clears the width's high half and writes the
float's low half (0 for any size below 65,536: harmless);
`Dd_CreateTextureSurface(the two low words, &D3d_CaptureTexSurface,
&D3d_CaptureTexture, 2)`; a texture narrower than 320 makes the used width
its width, the scale width x 0.003125 (`fild`, `fmul`, `fstp`) and the used
height `_ftol(240 x scale)`. Either creation failing returns.

Every capture: the steps `_ftol(ScaleX x 65536.0)` and `_ftol(ScaleY x
65536.0)`; one surface description (`Dd_InitSurfaceDesc`), locked through the
capture surface (its pixels kept) and then through `DDraw_BackBuffer` (its
pixels and pitch read from it); 240 rows x 320 nearest samples of the back
buffer at `(x step >> 16, y step >> 16)` (row `sar`, then `imul` by the
pitch) packed into the capture surface, 16-bit when the screen's bytes per
pixel (`0x7DEDE3`, read after the locks) is 2, else 32; `Unlock` of the back
buffer, then of the capture surface; `Blt` of the capture surface's (0, 0,
320, 240) onto the texture surface's (0, 0, used w, used h), `DDBLT_WAIT`;
`D3d_CaptureReady` `0x7CADE8` = 1, whatever `Blt` answered.

**Nothing requests it and nothing uses it.** `D3d_AfterDrawRequest`
`0x7CADEA` appears in no instruction but `Gfx_DrawOTag`'s three (a raw scan,
2026-09-22 and again today); the ready word and the texture `0x7CAE00` only
here, in `Display_Setup` (the zeroing) and in the teardown (`0x5A6651`,
`0x5A6660`, `0x5A666F`, the three `Release`s); no route of the read pass
entered `D3d_AfterDraw`. (display-setup.md section 7 guesses the codes
`0xF4..0xF7` would request it; they do not - the walk hands them to
`D3d_SetAlphaModulate`.) **Under the backend it would end the game**: its
second `Lock` is of the back surface, which `render_shim.cpp`'s
`Surface_Lock` refuses with a `Fatal` ("Lock of the back surface - not
built") - under Capcom's handler today as under ours. Taken whole all the
same: the rule is whole or loud, and the fuzz measures it against Capcom's on
fake surfaces.

## 6. The open questions

**1. `0x59E930`: `Gfx_StoreImage`, taken.** 108 bytes, read to its last
instruction: the rectangle (s16 x, y, w, h) of `Gfx_VramShadow` copied row
by row (`0x800` bytes apart) to a buffer, packed, `w * 2` bytes a row (`rep
movsd` then `rep movsb`); nothing when s16 h is not above 0; h re-read through
the rectangle's pointer after every row (a rectangle the copy overwrites
changes the count; the fuzz seeds it), x, y, w once. It returns `w * 2`, which
none of its callers reads. It is `Gfx_LoadImage` `0x59EA70`'s inverse - libgpu
StoreImage's PC counterpart - so **by nature the PSX library layer, filed as
renderer by its address**. Its callers are four effect states, all ours:
`0x475EC0` (`EffectKind6B_States[1]`), `0x480730` (`EffectKind61_States[1]`),
`0x490BB0` (`EffectKindA1_ReadBack`) and `0x491410`
(`EffectKindA3_ReadBack`, the same code), each reading
the rectangle at (`0x340`, `0x100`) back into a particle buffer - and ten call
sites in our DLL, which are those bodies' and their fuzzes' constants. No
recorded route enters it (the catalogue's reach columns; the read pass's
traces). A leaf of the size of the setters the PSX-library group takes, reached
by game code of ours: taken here, with the handlers.

**2. Can ours run with the software render flag set? No - the flag cannot be
set under ours.** Measured by reading, headless:

- The only instruction in `.text` that sets `Gfx_RenderFlags` bit 0 is
  `or al, 1` at `0x5A530F` in Capcom's `Display_Setup` `0x5A5160`, when the
  device index (`Cfg_RenderMode`, `renderer=` in `BOF3.CFG`) is 0 (a raw scan
  for `0x6C3A4C`: 38 instructions, every write inside `0x5A5160..0x5A5BB4`,
  the set-up helper `0x5A60E0` - bit `0x20` - and none elsewhere).
- Ours (`display_setup.cpp`, DIV-0031) writes the flags 0, then 2, then `|=
  0x200` and possibly `|= 0x20`; never bit 0. Every device index takes the
  hardware path. No other code of ours writes the flag (`grep 6C3A4C src`).
- At the self-test's start-up the flag is the image's zero, and nothing sets
  bit 0 before or after.

So the software surfaces' table (`0x5A3A60..0x5A4C40`, 21 entries) and its
callees - the 32 starts the read pass listed - are reached **only under
`BOF3X_ORIGINAL=Display_Setup` with `renderer=0`**, which is Capcom's
DirectDraw path end to end: original-only, gone at the cutover, and the first
table in `d3d_list.cpp`'s `kOriginals` the one line to retire then. Nothing
here needs a live run; if the coordinator wants one, `renderer=0` with ours
should draw exactly as `renderer=1` (the device record 0's name "Software
Render" still shows in F7's overlay - `display_setup.cpp` keeps it for the
cycling) - which DIV-0031's checks already cover.

## 7. The fuzz

`BOF3X_SHADOW=d3d_rest`, one start-up run, the whole headless run 3 to 4
seconds (the controls' timings). Each
function against a byte-copy of Capcom's with every call re-aimed at a
recording stand-in (ours on the same stand-ins through `d3d_rest::g`); the
device the harness's fake (`d3d_fuzz.h`, unchanged - `SetTextureStageState`'s
slot was already covered). POLY_FT3's copy calls a copy of `D3d_FlattenFT3`,
which calls a copy of `D3d_PageTexel4`, so the chain is compared whole; the
CRT's `_ftol` is called by the copies as the original calls it. Compared each
round: the call log with arguments, the vertices each `DrawPrimitive` was
handed, the primitive, every byte of state either side could touch (the vertex
block, scales, `D3d_TexCoords`, `Gfx_DrawTpage`, `Gfx_PixelFormat`'s first
record, `D3d_AlphaOpCache`, the capture block `0x7CADE8..0x7CAE03`, the render
flag, `DDraw_BackBuffer`, the screen's bytes per pixel, the palette the stand-in
hands out), the return value; for `D3d_AfterDraw` the 300 KB capture buffer by a
hash; for `Gfx_StoreImage` its 64 KB destination.

**The x87.** Every float operation is the original's sequence in inline
assembly, and each round runs under a control word picked from `0x027F` (the
game's), `0x007F` and `0x037F`, as `d3d-draw.md` section 5 explains.

**The stand-ins** write what the real callees write where the caller reads it
again - the colour pair; a palette (`Gfx_ClutPixels`, a pointer into the
fuzz's own, at a seeded offset); a surface description's pixels and pitch (the
fake `Lock`), a surface and a texture (the creation helpers), the two
overlapping size dwords (`D3d_FitTextureSize`) - and, a quarter of the time, a
byte of what the caller reads after the call: the primitive, the vertex block,
the scales, `Gfx_DrawTpage`, a texture coordinate, `Gfx_PixelFormat`, the
palette; for the capture the used-size words, the screen's bytes per pixel,
the scales (seeded values only: the back buffer is sized for them) and which
surface each of the three globals names.

**Rounds and coverage** (the last run): each of the five handlers 20,000 under
the three control words (POLY_FT3 one texel 10,024); `D3d_FlattenFT3` /
`D3d_PageTexel4` on their own 40,000; `D3d_SetAlphaModulate` 20,000;
`D3d_AfterDraw` 3,000 (render flag set 469, a creation failed 244, first use
with a narrow texture 282 and a wide one 357, a lock failed 475, copied 16-bit
649 and 32-bit 977); `Gfx_StoreImage` 20,000 (nothing copied 3,083, an odd
width 9,861, the rectangle overwritten by its own copy 2,606). Seeded: colours `0, 1,
0x7F, 0x80, 0x81, 0xFF`; the code byte `0x20` / `0x24` with its low bits;
tpage and CLUT words across the blend and colour-mode bits; the floats and
scales of `d3d-draw.md` section 5; texels `0, 1, 0x7F, 0x80, 0xFE, 0xFF`;
POLY_FT3's three (u, v) words equal half the time, two of three now and then;
`Gfx_PixelFormat` as 1-5-5-5 (the owner's machine), 5-6-5, X-8-8-8 with 32-bit
texels, or random, its bytes per texel 2, 4 or anything; palettes random or of
`0x00` / `0xFF` bytes (the clamp); `D3d_SetAlphaModulate`'s argument `0, 1,
0x100, 0x80000000, ~0` and the cache `0, 1, ~0` or random; for the capture the
render flag's bit 0, first use or not, creations and locks failing, the fitted
sizes `0x140, 0x100, 0x200, 0x13F, 0x80, 0x40, 0, 0xFFFF, 0x10100, 0xF0,
0x141, 1` or random, scales `0.5 .. 3` among them steps one below a whole
16.16 boundary (1 - ulp, 2 - ulp, 1.5 - ulp, 4/3: control 8 below), pitches `0x280 .. 0xA04`, the bytes per pixel 2, 4
or anything; for the read-back x `0 .. 0x3FF` (`0x3FF`, `0x340` seeded), y
`0 .. 0x1DF`, w `0 .. 0x1FF` (odd widths: the `rep movsb` tail), h `0 ..
0x20` and `0`, `-1`, `-2`, `-0x8000`, the rectangle inside the rows it copies
with the h it will be overwritten by planted in the shadow. Result: **0
mismatches**.

**Negative controls: 39 planted** by the scratchpad's `ph_controls.py`, which
edits `d3d_rest.cpp` on a unique anchor, rebuilds, runs the headless
self-test, reads the verdict, restores and rebuilds. On the first run 38 were
refused; control 8 (*the capture's x step one high*) was not - every seeded
scale gave a step that is a multiple of `0x4000`, so 320 extra units never
moved a sample across a pixel. Scales one step below a 16.16 boundary were
added (section 7's seeds) and **all 39, run again on the final fuzz, were
refused** by a comparison, none by a fault or a hang. Two of them are the
fixes D-b would want (23, 27): both are refused, so the fuzz would see either
fix as the divergence it is.

| # | control | refused by |
|---|---|---|
| 0 | store: h read once | the copy (the rectangle overwritten) |
| 1 | store: row stride `0x7FE` | the copy |
| 2 | store: the tail `bytes & 1` | the copy |
| 3 | store: h tested unsigned | the copy (h = -2) |
| 4 | store: returns w | the return value |
| 5 | alpha: off is SELECTARG1 (2) | call 0's arguments |
| 6 | alpha: the cache 2 when on | memory `0x66B720` |
| 7 | alpha: `on` tested as a byte | the call count |
| 8 | capture: x step + 1 | not refused at first (blind); refused after the seeds: the captured pixels |
| 9 | capture: the two unlocks swapped | call 6 (which surface) |
| 10 | capture: destination width 0x140 | the Blt's rectangle |
| 11 | capture: scale width / 256 | the Blt's rectangle |
| 12 | capture: bytes per pixel read before the locks | the captured pixels |
| 13 | capture: no return on a failed second lock | the call count |
| 14 | capture: the back buffer pointer read once | call 6 (which surface) |
| 15 | F3: a triangle strip | DrawPrimitive's type |
| 16 | F3: corner stride `0x10` | the vertices handed to DrawPrimitive |
| 17 | F3: blend mode as a byte | the blend's arguments |
| 18 | FT3: flattened on two equal corners | the call count |
| 19 | FT3: vertex 3's z not zeroed | memory `0x7CA9C0` |
| 20 | FT3: bind arguments swapped | the bind's arguments |
| 21 | FT3: flattened but bound | call 2 |
| 22 | FT3: tpage read once after the bind | the blend's arguments |
| 23 | flatten: the port's offsets (a D-b fix) | `Gfx_ClutPixels`' argument |
| 24 | flatten: red clamped at `0xFE` | the colour call |
| 25 | flatten: code bit 0 cleared, not 2 | the colour call |
| 26 | flatten: green by the blue shift | the colour call |
| 27 | texel: low nibble for an even u (a D-b fix) | the colour call |
| 28 | texel: bytes per texel read before the call | the colour call (round 3,396) |
| 29 | texel: page y by bit `0x8` | the colour call |
| 30 | GT3: corner 2 coloured from corner 1 | the third colour call |
| 31 | GT3: flat | the shade call |
| 32 | GT3: bind's tpage from `+0x26` | the bind's arguments |
| 33 | LINE_G4: three corners | DrawPrimitive's count |
| 34 | LINE_G4: a specular pointer | call 0 |
| 35 | LINE_G4: second `0x437CC0` gets 0 | call 6 |
| 36 | TILE_1: two points | DrawPrimitive's count |
| 37 | TILE_1: `sz` through the x87 | the vertices (a signalling NaN) |
| 38 | TILE_1: first `0x437CC0` gets 0 | call 2 |

Not planted, because nothing can see them: the order in which
`D3d_FlattenFT3` reads one colour byte and stores the one before (the bytes do
not alias), the flatten's products in SSE rather than x87 (r x channel / 128
is exact in 24 bits for every byte and 16-bit channel), and the capture's row
`sar` as a `shr` (section 8).

## 8. What nothing reaches, live coverage, and the rebinding

**Live coverage** (the read pass's traces, `analysis/calltrace/platform_1005`):
`D3d_DrawTile1` runs in the **`whelpBoss`** route (battle motes, D-a) and
the state hash on that route checks it - but the hash skips the renderer's
range, so a wrong TILE_1 shows as a capture difference, not a hash one. No
recorded route enters the other nine: POLY_F3 (`Gpu_SetPolyF3` `0x5A7570`,
the triangles of effects and magic no route casts), POLY_FT3 (`0x5A7590`, the
same), POLY_GT3 (no `mov byte [reg + 7], 0x34` anywhere in `.text`: whether
anything builds one is not established), LINE_G4 (`0x5A76F0`, magic trails),
`D3d_SetAlphaModulate` (no route's ordering table holds a code `0xF4`),
`D3d_AfterDraw` (nothing requests it, section 5), `Gfx_StoreImage` (four effect
states no route reaches). For those the fuzz is the whole evidence.

**What the self-test does not reach:** pixels (the harness sees calls,
vertices and memory); the real device and surfaces; `D3d_AfterDraw` against
the backend's own `Lock` (which refuses the back buffer - section 5); a row
step whose accumulator passes `2^31` (where `sar` and `shr` part: it would need
a scale above 136 and a back buffer of gigabytes, so the fuzz cannot show a
`shr` - by reading only); `Gfx_StoreImage` with a negative w or a rectangle
outside the shadow (D-e: the copy would fault or run for gigabytes).

**What would show a wrong one, live:** `D3d_DrawTile1` in `whelpBoss` - the
motes missing, mis-coloured or misplaced in a capture against
`BOF3X_ORIGINAL=D3d_DrawTile1`. The rest only where a route someday builds
their primitive.

**The rebinding.** Every raw reference in `src/game` and `src/hook` to the
ten (`grep -rn -i "0x<address>"`): `Gfx_DrawOTag`'s Direct3D table in
`d3d_list.cpp` now reads `H(bof3::addr::D3d_DrawPolyF3)` and the other four,
and its `kOriginals` the two callees as `bof3::orig::D3d_SetAlphaModulate` /
`D3d_AfterDraw` (a table of originals: `BOF3X_ORIGINAL=<name>` restores
Capcom's for the walk); `d3d_list_fuzz.cpp`'s expected targets for the seven
sites; `kStoreImage` in `effect_2c_callees.h` and `effect_3a_callees.h`
`= bof3::addr::Gfx_StoreImage` (the value unchanged: the scenario harness keys
its stand-in on it, so the bodies keep calling by address); the four
`CallSite`s and two labels in `effect_2c_fuzz.cpp`, `effect_3a_fuzz.cpp`,
`rest_3f_fuzz.cpp`; the scenario harness's row. **Left:** `kStoreImage` in
`rest_3f_callees.h` - the line next to `kSqrt` `0x5A7A90`, which the PSX
library group rebinds tonight; the coordinator's. Comments naming the
addresses are left as they are.

## 9. For `analysis/calltrace/entries_logic.txt`

Owned ranges, `start size` as that file has them. `0059E930 6C` is there
already with the right size; the other nine are not:

```
0059E930 6C
0059F520 53
0059F580 2BF
0059FA50 14C
0059FDB0 226
005A0910 121
005A0A40 6A
005A1050 239
005A1EA0 22F
005A2220 D5
```

## 10. Changes to shared code

`d3d_fuzz.h` / `.cpp` unchanged. `symbols.toml`: the eight new `[[func]]`
entries after `D3d_DrawTile`; `D3d_SetAlphaModulate` and `D3d_AfterDraw` given
`impl` and their full reading; `[[data]]` `D3d_AlphaOpCache` `0x66B720`,
`D3d_CaptureReady` `0x7CADE8`, `D3d_CaptureSurface` `0x7CADF8`,
`D3d_CaptureTexSurface` `0x7CADFC`, `D3d_CaptureTexture` `0x7CAE00`.
`inject_all.cpp`: `D3dRest_Inject()` after `Rest4B_Inject()`.
