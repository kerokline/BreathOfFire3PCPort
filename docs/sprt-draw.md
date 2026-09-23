# The Direct3D sprite handlers, with DIV-0010 inside

**Status:** IN PROGRESS (2026-09-23 - group D of the sixth parallel round:
three functions ours, fuzzed headless against copies of Capcom's bytes and
against DIV-0010's old re-aimed copies, 32 negative controls refused; not yet
run in game - the batch after the merge is the first live check)

`D3d_DrawSprt` `0x5A2300`, `D3d_DrawSprt8` `0x5A2520` and `D3d_DrawSprt16`
`0x5A2710` draw the PSX's `SPRT`, `SPRT_8` and `SPRT_16` under Direct3D:
every menu numeral, the 8 x 8 font, the menu's panel pieces. Since
2026-09-20 they ran as byte-copies of Capcom's handlers with two operands
re-aimed each (`src/game/gfx_sprite_uv.cpp`), which carried DIV-0010, the
far texture edge that stopped the bottom row of every menu numeral being cut
off ([`known-defects.md`](known-defects.md) D1). They were counted as ours
without being reimplementations; `D3d_DrawSprt` was the last function the
attract sequence reaches in stage 1's scope that was not really ours
([`takeover-queue-round5.md`](takeover-queue-round5.md)). Now they are C++
(`src/game/sprt_draw.cpp`), faithful but for DIV-0010, which is built in and
switchable on its own. `gfx_sprite_uv.cpp` is gone.

Every claim about the binary below is from capstone over `bof3/BOF3.exe`
(`tools/pe_disasm.py`) on 2026-09-23 unless it says otherwise.

## 1. The functions

| Entry | Name | Bytes | Code | Reached from |
|---|---|--:|---|---|
| `0x5A2300` | `D3d_DrawSprt` | `0x211` | `0x64` SPRT | the draw `0x59EE50`'s second jump table `0x59F3D8` |
| `0x5A2520` | `D3d_DrawSprt8` | `0x1EB` | `0x74` SPRT_8 | the same |
| `0x5A2710` | `D3d_DrawSprt16` | `0x1EB` | `0x7C` SPRT_16 | the same |

All three straight-line, entry to `ret`, no jump. Six `E8` each - to
`D3d_PrimColor` `0x59FBA0`, `D3d_BindTexture` `0x59FFE0`, the bare `ret`
`0x437CC0` twice, `D3d_SetBlend` `0x59FCA0`, `D3d_SetShadeMode` `0x59FD80`
(all ours since round four, [`d3d-draw.md`](d3d-draw.md)) - at `+0x38 +0x1BB
+0x1C2 +0x1C9 +0x1E1 +0x1E8` in SPRT and `+0x35 +0x198 +0x19F +0x1A6 +0x1BE
+0x1C5` in the other two; one COM call, `DrawPrimitive`. `D3d_DrawSprt8` and
`D3d_DrawSprt16` are the same bytes but for two operands each.

## 2. Read

The primitive: r, g, b, code at `+4..+7`; float x `+8`, y `+0xC`, z `+0x10`;
u8 u `+0x14`, v `+0x15`; u16 CLUT `+0x16`; SPRT only, u16 w `+0x18`, h
`+0x1A`. (`Menu_DrawPiece` `0x57D860` builds a SPRT of `0x1C` bytes; the 8 x 8
font `0x517090` a SPRT_8.)

In order:

1. `D3d_PrimColor(r, g, b, code, Gfx_DrawTpage & 0xFFFF, &diffuse,
   &specular)`. The diffuse pointer is the handler's own argument slot - the
   caller's pushed `prim`, which it pops unread (SPRT's call is at `0x59F18A`,
   then `add esp, 4`); not kept, as in every other handler.
2. The four `D3DTLVERTEX` at `D3d_Vertices` `0x7CA958`, in strip order (x, y),
   (x + w, y), (x, y + h), (x + w, y + h):
   - `sx = fld D3d_ScaleX; fmul x`, `sy` likewise; the far edges `fild w; fadd
     x; fmul scale` in SPRT (w zero-extended: `xor edx, edx; mov dx, [esi +
     0x18]`, then through a stack dword), `fld x; fadd 8.0 (0x5C41CC) / 16.0
     (0x5C41D0); fmul scale` in the others - the sum rounded to the control
     word's precision, not to float, before the multiply.
   - `sz`: `fld z; fst` to all four; `rhw`: `fld 0.1 (0x5C4610); fdiv z`, to
     all four.
   - the colour pair to all four.
   - texture: near `tu`, `tv` = `D3d_TexCoords[u]`, `[v]` (`0x7CA9E0`) through
     `fld` / `fstp`; far = `[0x7CA9DC + 4 * (u + w)]` - `tc[u + w - 1]` - in
     SPRT (the index `u8 + u16` in a register, the disp32 at `+0xD2` and
     `+0x130`), `[0x7CA9FC + 4u]` = `tc[u + 7]` and `[0x7CAA1C + 4u]` =
     `tc[u + 15]` in the others (disp32 at `+0xBD`, `+0x11D`). Unbounded:
     D-NEW-D.
   - the repeats (the second corner's `sy` and `tv`, the third's `sx` and `tu`,
     the fourth's `sx`, `tu`, `tv`) are either `fst`s of the same x87 register
     or `mov`s of the dword just stored: the same bits.
3. `D3d_BindTexture(Gfx_DrawTpage & 0xFFFF, CLUT)` - `Gfx_DrawTpage`
   `0x7DED14` read again; the return not read.
4. `0x437CC0(1)` twice.
5. `D3d_SetBlend(code, Gfx_DrawTpage & 0xFFFF)` - both read a third time,
   after the texture and the rets.
6. `D3d_SetShadeMode(1)` (flat).
7. `DrawPrimitive(TRIANGLESTRIP, D3DFVF_TLVERTEX, D3d_Vertices, 4, 0)` on the
   device at `0x7CC350`, read afresh; its result is the handler's.

Ours is one body for the three (`DrawSprite`), each x87 sequence written as
the original's instructions (`X87Mul`, `X87AddMul`, `X87IntAddMul`,
`X87Div`, `X87Pass`, as `d3d_draw.cpp` does): the project compiles with SSE,
and only the same instructions give the same bits under every control word.

## 3. DIV-0010, and its switches

The far edge is read at `g_far_base + 4 * j`, `j = u + w` (`u + 8`, `u + 16`
for the fixed sizes). `g_far_base` starts as Capcom's `0x7CA9DC`; at inject
`PatchBytes("SpriteFarEdge", &g_far_base, ...)` points it at `g_far`, our
table, `g_far[j] = (j - 1/30 + 0.012) / 256` - the value that puts the last
pixel on the last texel's centre for the 8 x 8 font at 2x
([`DIVERGENCE.md`](DIVERGENCE.md) DIV-0010, and the long comment at the top
of `sprt_draw.cpp`, which moved there from `gfx_sprite_uv.cpp`). This is the
same arithmetic as the re-aimed copies: their SPRT operand became `g_tc + 0`
(index `u + w`), their SPRT_8 and SPRT_16 operands `g_tc + 8` and `g_tc + 16`
(index `u`).

| `BOF3X_ORIGINAL=` | The handlers | Far edge |
|---|---|---|
| (none) | ours | DIV-0010 |
| `SpriteFarEdge` | ours | Capcom's, `tc[u + w - 1]` |
| `D3d_DrawSprt` (and / or `8`, `16`) | Capcom's, for each one named | Capcom's, for those |
| `*` | Capcom's | Capcom's |

The owner's names of 2026-09-20 keep their meaning: the handler named is
Capcom's own. `SpriteFarEdge` is the finer switch, as `GlyphTexelCentres`
is for DIV-0025.

**The table.** `g_far` has `0x10100` entries - one for every `u8 + u16` - where
the copies' had 1,024, read unchecked, so an SPRT with `u + w` of 1,024 or
more read our dll's memory after it. Below 1,024 the values are the copies'
exactly: the inset's bits are pinned by a `static_assert` (`0xBCAEC33F`) and
the first 1,024 entries by an FNV-1a hash (`0xF06E2C4B`), both computed
independently in numpy float32, checked at every start. The fuzz cannot see
the table's values - ours and the re-aimed copies read the same one - so the
pin is what says it is unchanged; control 31 below is refused by it.

## 4. The fuzz

`BOF3X_SHADOW=sprt_draw` (`src/game/sprt_draw_fuzz.cpp`) on the vertex-block
harness `src/game/d3d_fuzz.h` ([`glyph-draw.md`](glyph-draw.md) §5). Two
copies of each handler are made from Capcom's bytes before
`SprtDraw_Inject`'s `BOF3_INJECT`s - nothing else patches the three, so the
module's place at the end of `inject_all.cpp` is safe: Capcom's as they are,
and Capcom's with both far operands re-aimed at `g_far` exactly as
`gfx_sprite_uv.cpp` did (the `D9 04 <sib>` before each disp32 and its old
value checked). Every `E8` of both goes to a recording stand-in, the same
stand-ins ours gets through `sprt_draw::g`; the device is the harness's fake.

Three passes per handler, 20,000 rounds each, under a control word picked per
round from `0x027F` (the game's), `0x007F` and `0x037F`:

1. ours with the base at `0x7CA9DC` (DIV-0010 off) against Capcom's copy,
   byte for byte - **the faithful half**;
2. ours with the base at `g_far` (DIV-0010 on) against the re-aimed copy,
   byte for byte - **today's behaviour kept**;
3. ours with DIV-0010 on against Capcom's copy: every byte equal but the four
   far coordinates (the second and fourth corners' `tu`, the third and
   fourth's `tv`) - **the divergence touches those and nothing else**. They
   differed in 20,000 of 20,000 rounds of each handler.

Compared each round: the log of calls with their arguments, the vertices
handed to `DrawPrimitive` at the moment of the call, the return value, the
primitive, and every byte of state either side could touch - the vertex
block, the 8 bytes after it, `D3d_TexCoords` and the `0x800` bytes after it,
the scales, `Gfx_DrawTpage`, `g_far[0..511]`.

Seeded: u and v `0, 1, 7, 8, 0xF, 0x10, 0x7F, 0x80, 0xF0, 0xF7, 0xF8, 0xFE,
0xFF`; w and h from `0, 1, 2, 7, 8, 9, 0xC, 0xF, 0x10, 0x11, 0x20, 0x80, 0xFF,
0x100, 0x101, 0x1FF, 0x7FFF, 0x8000, 0x8001, 0xFFFF`, or so that `u + w` lands
within 2 of 256, or anywhere below 768, or random, and `u = w = 0` one round
in 32; x, y, z from zeros, denormals, infinities, quiet and signalling NaNs,
`2^24`, `2^23 + 1`, `1 + ulp` and others; scales 2.0, 1.0, 1.5, 3.0, 0.5,
-2.0, 2.25, tiny, huge and random; both tables seven entries in eight their
game values (so an index off by one shows) and one in eight any bits, NaNs
among them. The stand-ins write the colour pair, and a quarter of the time
one byte of the primitive, the vertex block, a scale, `Gfx_DrawTpage`, a
texture coordinate or a far entry - so every read is ordered against every
call.

**Last run** (`BOF3X_SHADOW=sprt_draw`, and inside `BOF3X_SHADOW='*'` with
all 128 self-test lines passing): 0 mismatches. Of SPRT's 60,000 rounds,
39,742 had `u + w` past 256 (D-NEW-D's reads), 1,932 had `u + w = 0` (the
gap dword before the table), 9,664 a `w` of `0x8000` or more; SPRT_8
7,453 past 256, SPRT_16 14,837. Also passing with
`BOF3X_ORIGINAL=SpriteFarEdge`, with
`BOF3X_ORIGINAL=D3d_DrawSprt,D3d_DrawSprt8,D3d_DrawSprt16` (inject line:
591 ours, 3 left original) and with `BOF3X_ORIGINAL=*` (the fuzz runs before
the switches, so it checks the same either way).

**Negative controls: 32 planted, 32 refused**, each by a comparison - none by
a hang, none unobservable. Planted one at a time by a script that edits
`sprt_draw.cpp`, builds, runs the headless self-test and restores:

| # | control | refused by | rounds refused |
|---|---|---|--:|
| 0 | far `u` index one short | snapshot | 114,128 |
| 1 | Capcom's far base one entry on (`tc[u + w]`) | snapshot (pass 1 only) | 58,264 |
| 2 | far base ignores the switch (always Capcom's) | snapshot (pass 2 only) | 59,999 |
| 3 | far `v` index from `u` | snapshot | 32,660 |
| 4 | `w` read signed | snapshot | 6,639 |
| 5 | right edge times the y scale | snapshot | 52,298 |
| 6 | bottom edge from x | snapshot | 51,709 |
| 7 | SPRT's `w + x` rounded to float before the multiply | snapshot | 1,545 |
| 8 | SPRT_8's `x + 8` rounded to float before the multiply | snapshot | 2,776 |
| 9 | SPRT_8 uses 16.0 | snapshot | 51,575 |
| 10 | SPRT_16's far extent 15 | snapshot (passes 1, 2) | 40,000 |
| 11 | second corner's `tv` the far one | snapshot | 179,584 |
| 12 | fourth corner's `sx` the left edge | snapshot | 116,301 |
| 13 | `sz` a plain copy, not `fld` / `fst` | snapshot (signalling NaN z) | 7,429 |
| 14 | near `tu` a plain copy, not `fld` / `fstp` | snapshot | 907 |
| 15 | far `tu` a plain copy | snapshot | 534 |
| 16 | `rhw` divided in C++ (SSE) | snapshot (cw `0x007F` only: a denormal quotient rounded twice on the x87) | 50 |
| 17 | `rhw` numerator 1.0 | snapshot | 150,767 |
| 18 | diffuse and specular swapped | snapshot | 180,000 |
| 19 | colour mode masked to a byte | call 0's arguments | 134,432 |
| 20 | no specular pointer | call 0's arguments | 180,000 |
| 21 | texture tpage read before the colour helper | call 1's arguments | 5,614 |
| 22 | blend code read before the texture bind | call 4's arguments | 1,785 |
| 23 | blend mode read before the texture bind | call 4's arguments | 21,465 |
| 24 | CLUT from `+0x14` | call 1's arguments | 179,804 |
| 25 | one bare `ret` dropped | the call count | 180,000 |
| 26 | shade mode 2 | call 5's arguments | 180,000 |
| 27 | 3 vertices drawn | call 6's arguments | 180,000 |
| 28 | returns 0 | the return value | 180,000 |
| 29 | x read before the colour helper | snapshot | 2,170 |
| 30 | the x scale read before the colour helper | snapshot | 2,236 |
| 31 | DIV-0010's inset 0.012 only (the `static_assert` removed) | the table's hash, at start-up | - |

The quiet ones are worth reading: 16 shows only under the 24-bit control
word, 14 and 15 only when the table holds a signalling NaN, 7 and 8 only when
the sum is inexact in float - the seeds are what make them visible.

## 5. Defect

**D-NEW-D** ([`known-defects.md`](known-defects.md), to be numbered at the
merge): the far index `u + w - 1` (`u + 7`, `u + 15`) is not bounded, so a
sprite whose texels run past column 255 of its page reads its far coordinate
from whatever follows `D3d_TexCoords`. Latent, Capcom's, kept with DIV-0010
off; with it on, our table continues the line instead.

## 6. Not checked

- **Nothing in game yet.** The attract sequence reaches `D3d_DrawSprt`
  ([`takeover-queue-round5.md`](takeover-queue-round5.md); its calls were
  never counted - the copy was unarmed in the tracer);
  `D3d_DrawSprt8` and `D3d_DrawSprt16` are unreached by it and need the menu
  (`tools/recipes/menu_screens.txt`, the numerals of D1). The batch after the
  merge should run the oracle and the frame hash with the three in the trace
  list (`entries_logic.txt` must name them with their sizes, `0x211`,
  `0x1EB`, `0x1EB`, now that they are real functions of ours - HANDOFF
  "Pick up here" 1), and a menu capture A/B of `SpriteFarEdge` on and off
  with the screen uncovered.
- The pixels: like every Direct3D handler, the product is a draw call, and
  only its arguments and vertices are compared ([`IDEAS.md`](IDEAS.md) I14).
- Aliasing of the primitive with the vertex block: the original's stores and
  loads interleave, ours reads every input before it stores; they differ only
  if a primitive lay inside `D3d_Vertices`, which the packet pool never puts
  there. Not fuzzed.
- `D-NEW-D` in game: whether any sprite has `u + w` past 256.
