# The glyph draw, the vertex-block fuzz, and three display fixes

**Status:** IN PROGRESS (2026-09-22 - group N of the fourth parallel round:
two functions ours, fuzzed and controlled headless; DIV-0025, DIV-0026 and
DIV-0027 built, none yet seen in game - the batch after the merge is the
first live check)

Every glyph the port draws - Chinese or the overlay's Latin - reaches the
screen through one Direct3D handler, `D3d_DrawGlyph` `0x5A2900`, and the
texture cache under it, `Font_GlyphTexture` `0x5A2BC0`. Both are ours now
(`src/game/glyph_draw.cpp`), faithful, fuzzed against byte-copies of
Capcom's; the fuzz's harness (`src/game/d3d_fuzz.h`) is written for group R,
which takes the other Direct3D handlers. On top: D17's fix (DIV-0025), and
two layout patches the owner asked for with it - the Config screen's
controller names (DIV-0026) and the save screen's Yes / No (DIV-0027).

Every claim about the binary below is from capstone over `bof3/BOF3.exe`
(`tools/pe_disasm.py`) on 2026-09-22 unless it says otherwise.

## 1. The functions

| PC | name | true size | ours | what |
|---|---|---|---|---|
| `0x5A2900` | `D3d_DrawGlyph` | `0x2B4` (to `0x5A2BB3`) | yes | code-`0x6C` primitive -> four `D3DTLVERTEX` -> `DrawPrimitive` |
| `0x5A2BC0` | `Font_GlyphTexture` | `0xD7` (to `0x5A2C96`) | yes | find or build the glyph's texture in a 128-entry cache, `SetTexture` |
| `0x5A2CA0` | `Font_BuildGlyphTexture` | `0x1C9` (to `0x5A2E68`) | no | build one glyph texture through DirectDraw (§3) |
| `0x59FBA0` | `D3d_PrimColor` | `0xE4` (to `0x59FC83`) | no | PSX colour -> diffuse + specular (§4) |
| `0x59FCA0` | `D3d_SetBlend` | `0xC9` (to `0x59FD68`) | no | PSX semi-transparency -> three render states (§4) |
| `0x59FD80` | `D3d_SetShadeMode` | `0x24` (to `0x59FDA3`) | no | `SHADEMODE`, cached (§4) |
| `0x5747D0` | `Menu_YesNo` | `0xB2` (to `0x574881`) | no, patched | the Yes / No chooser (§7) |
| `0x5905D0` | `Menu_DrawHand` | `0x8C` (to `0x59065B`) | no | the pointing hand sprite (§7) |

The first two are the PC port's own: the PlayStation drew glyphs as GPU
primitives, so there is no PSX twin to read them against. Sizes are to the
byte after the last instruction; `pe_funcs.py` agrees for the two taken over
(692, 215).

**Who reaches `D3d_DrawGlyph`.** One call site, `0x59F1B4` in the draw
`0x59EE50`, entry 17 of its second jump table `0x59F3D8`. The byte table
`0x59F440` maps only `(code & 0xFC) - 0x20 = 0x4C` to entry 17, so codes
`0x6C`..`0x6F` and nothing else. Five functions build code `0x6C` (the
callers of `Gpu_SetCode6C`, `analysis/pc_funcs.json`): `Text_EmitGlyph`
`0x516D50` (every string), `0x4987E0` (under `MsgBox_Step`), `0x4FC420`
(six strings over an object), `0x469AD0` and `0x4B1520`. The first four were
checked to store a glyph index at `+0x16` (the last is 2,880 bytes and was
not read). **So every code-`0x6C` primitive is a glyph, and DIV-0025 applies
to all of them.** Nothing else in the exe reads the `1/32` double at
`0x5C4618`: its eight references (a raw scan of `.text`) are all inside
`D3d_DrawGlyph`. The sprite handlers' texture coordinates come from the
table `0x7CA9E0` (DIV-0010), not from this arithmetic.

`Font_GlyphTexture` has a second caller, the software renderer's glyph path
`0x5A4900` (first jump table, `0x59EFC9`); it runs our function too.

## 2. `D3d_DrawGlyph`, read

Straight-line; six relative calls, at `+0x2C`, `+0x25B`, `+0x262`, `+0x269`,
`+0x276`, `+0x27D`; two COM calls.

1. `D3d_PrimColor(r, g, b, code, 0, &diffuse, &specular)` - the bytes
   `+4..+7`, zero-extended.
2. Vertex `i` (from `0x7CA958 + 0x20 i`), corner `i` of the primitive
   (`+8 + 8i`): `sx` = s16 x times `D3d_ScaleX` `0x7C9F4C`, `sy` = s16 y times
   `D3d_ScaleY` `0x7C9F48` (fild, fmul by a float, fstp - exact before the one
   rounding, so the precision control does not matter); `sz` = `0x3F7D70A4`
   (0.99), `rhw` = `0x3DCCCCCD` (0.1), both immediates; the colour pair.
3. `tu`, `tv` = the byte `u`, `v` (`+0xC + 8i`, `+0xD + 8i`) shifted left one,
   fild, times the double at `0x5C4618` (1/32), fstp. No half-texel offset
   (D17).
4. `Font_GlyphTexture(u16 +0x16, u16 +0xE)` - glyph, CLUT, both
   zero-extended (the registers were cleared with `xor` before the `mov` of
   the word); its return is not read.
5. `0x437CC0(1)` twice - a bare `ret`.
6. `D3d_SetBlend(code, 0)` - the code byte re-read from the primitive.
7. `D3d_SetShadeMode(1)` - flat.
8. Device `+0x58` `SetRenderState(0x1B, 0)` - `ALPHABLENDENABLE` off, after
   `D3d_SetBlend` turned it on: glyphs draw unblended.
9. Device `+0x70` `DrawPrimitive(5, 0x1C4, 0x7CA958, 4, 0)` - a triangle
   strip of four `D3DFVF_TLVERTEX`. Its result is the function's.

The device pointer `0x7CC350` is re-read before each COM call; ours too. One
thing not kept: the original computes every fild's operand in its own
argument slot, so the caller's pushed `prim` comes back holding `2 * v` of
corner 3. The caller pops it unread (`add esp, 4` at `0x59F1B9`).

## 3. `Font_GlyphTexture`, read - and `Font_BuildGlyphTexture`

`Font_TexCache` `0x7C9F50`: 128 entries of `0x14` bytes - u16 glyph `+0`,
u16 CLUT `+2`, the CLUT row's generation `+4`, the DirectDraw surface `+8`,
the Direct3D texture `+0xC`, u16 "used this frame" `+0x10`. The draw
`0x59EE50` zeroes every `+0x10` after its `EndScene` (`0x59F267..0x59F27A`).

One pass over the 128: a hit is glyph and CLUT equal to the arguments -
`cmp` of the zero-extended word with the whole 32-bit argument, so an
argument with its high half set never hits - and `+4` equal to the dword at
`Gfx_ClutRows + (clut >> 6) * 8` (unsigned shift of the whole argument). On
the way, the first entry whose `+0x10` is 0. No hit and a free entry:
`Font_BuildGlyphTexture(free, glyph, clut)`, wrapped in device `EndScene`
(`+0x28`) and `BeginScene` (`+0x24`) unless `Gfx_RenderFlags` `0x6C3A4C`
bit 0. Then, unless bit 0, device `+0x98` `SetTexture(0, entry +0xC)`; the
entry's `+0x10` = 1 (a word); the index is returned. The flag byte is read
twice, before and after the build; ours too.

**The overrun, D-N1** ([`known-defects.md`](known-defects.md)): with all 128
entries used this frame and none a hit, the index stays 128. Entry 128 is
`0x7CA950..0x7CA963` - the eight bytes after the table and the first twelve
of the vertex block. `SetTexture` is handed vertex 0's `sy` as a texture
pointer and the in-use word lands on the low half of vertex 0's `sz`. Ours
does exactly that (the addresses are the original's arithmetic); the fuzz
seeds it (§5) and a control that clamps it is refused.

`Font_BuildGlyphTexture` `0x5A2CA0` (read to its `ret`, **not taken
over**): the 16 colours from `Gfx_ClutPixels(clut)`, the 288-byte glyph at
`Font_GlyphData + glyph * 0x120`; with flag bit 0 a 32 x 32 plain surface
(`0x59F860`, caps `0x840`) locked and the glyph unpacked into it by
`0x5A9E1E`; otherwise a 32 x 32 texture (`0x59F900`, caps `0x1800`; a colour
key through `+0x74` under flag bit `0x20`) filled by a `Blt` from the staging
surface `0x7CC344`, which is locked and unpacked the same way. On success it
stores glyph, CLUT and generation at `+0/+2/+4` and the surface and texture
at `+8/+0xC`. **Why not taken:** six callees, three of them DirectDraw
surface creation through the DirectDraw object `0x7CC334`, and four
surface methods (`Lock`, `Unlock`, `Blt`, `SetColorKey`); a fuzz of it needs a
fake surface with memory behind `Lock`, which this harness does not have.
The fuzz stands a recorder in for it that fills the entry the way its
success path does.

## 4. The three helpers, read (names for group R)

- **`D3d_PrimColor` `0x59FBA0`** `(r, g, b, code, mode, *diffuse, *specular)`.
  Alpha `0xFF` unless code bit 1; then by `(mode >> 5) & 3` through the table
  `0x59FC84`: `0x80`, `0xFF`, `0xFF`, `0x40`. Colour: bit 2 clear
  (untextured) - `r, g, b` as given; bits 2 and 0 (raw texture) - `0xFF`
  each; bit 2 alone (modulated) - each doubled and clamped to `0xFF`, the
  low byte of the doubled value going to the specular. `r`, `g`, `b` are
  compared as whole dwords. The specular is written only through a non-null
  pointer.
- **`D3d_SetBlend` `0x59FCA0`** `(code, mode)`: `SetRenderState` of
  `ALPHABLENDENABLE` (`0x1B`) 1, `SRCBLEND` (`0x13`), `DESTBLEND` (`0x14`).
  Code bit 1 clear: 5, 6. Set, by `(mode >> 5) & 3` through `0x59FD6C`:
  5, 6 / 2, 2 / 1, 4 / 5, 2 - the PSX's four semi-transparency modes.
- **`D3d_SetShadeMode` `0x59FD80`** `(mode)`: `SetRenderState(9, mode)`
  only when it differs from `D3d_ShadeModeCache` `0x6C3A44`, then caches it.

Each has 21 callers - every Direct3D handler. All three are Capcom's still.

## 5. The vertex-block fuzz (for group R)

`BOF3X_SHADOW=glyph_draw` (`src/game/glyph_draw_fuzz.cpp`) on the harness
`src/game/d3d_fuzz.h` / `.cpp`. What the harness gives:

- **`d3d_fuzz::DeviceSwap`** - for its lifetime `D3d_Device` holds a fake
  COM object. Its vtable has 64 slots; the ones `Arity()` in `d3d_fuzz.cpp`
  knows (`BeginScene` 9, `EndScene` 10, `SetRenderState` 22,
  `DrawPrimitive` 28, `SetTexture` 38, `SetTextureStageState` 40) record
  `(0x100 + slot, the arguments after this)` and return `ComResult(position)`,
  a function of the log position; any other slot calls `Fatal` naming it.
  **To cover a new method, add its slot and its argument count including
  `this` to `Arity()`** - stdcall, the callee pops them, so the count must be
  right.
- **`DrawPrimitive`'s recorder snapshots the vertices it is handed** (count
  x `0x20` bytes, up to `0x80`) at the moment of the call - a store after
  the draw shows even when the block ends up right.
- **`d3d_fuzz::Record(what, ...)`** for the user's own stand-ins (numbers
  below `0x100`), into `*d3d_fuzz::g_log`; **`SameLog(ours, theirs, why,
  rule)`** compares two logs call by call and snapshot by snapshot, with an
  optional rule for the snapshots (DIV-0025 uses one).
- `Seed` / `Next` / `Pick` - its own xorshift; the game's CRT is not up at
  start-up (HANDOFF traps).

How a handler is fuzzed with it (what `glyph_draw_fuzz.cpp` does): clone
Capcom's with every `E8` re-aimed at a recording stand-in (`CloneCall` with
`expected`); put ours on the same stand-ins through a `Callees` struct of
pointers (`glyph_draw_callees.h`); per round, random primitive, random
scales, random vertex block, `Capture` the state, run the copy, `Restore`,
run ours, compare the log, the snapshots, the vertex block, the return value.
**Stand-ins must write what the caller reads again:** `D3d_PrimColor`'s writes
the colour pair; `Font_GlyphTexture`'s sometimes writes the overrun's word on
vertex 0's `sz` and a new code byte into the primitive (the handler re-reads
`+7` after it) - that is what makes the order of the handler's stores and
reads against the call observable, and controls 16 and 22 below are
refused only because of it (their first refusals came in rounds 32 and 8).

**Rounds.** `D3d_DrawGlyph`: 20,000 with the inset 0 (everything byte for
byte) and 20,000 with the DIV-0025 inset (everything byte for byte except
each `tu`, `tv`, which must be the copy's plus exactly `1/64`) - so the fix
is checked to change those eight floats and nothing else, and ours with the
fix off is checked to be Capcom's. Seeded: colours `0, 1, 0x7F, 0x80, 0x81,
0xFF`; codes `0x6C..0x6F` and others; coordinates `0, 1, -1, 0x7FFF,
-0x8000, ...`; texels `0, 1, 0xC, 0x18, 0x7F, 0x80, 0xFF`; scales 2.0, 1.0,
1.5, 3.0, 0.5, -2.0, 2.25 and random. `Font_GlyphTexture`: 40,000 rounds -
in the last run 20,295 hits, 7,369 builds with flag bit 0, 7,371 inside
`EndScene`/`BeginScene`, 4,965 overruns (all 128 used), 10,230 with a stale
hit (glyph and CLUT right, generation not) seeded, 4,885 with an argument's
high half set. Result: **0 mismatches** (with `BOF3X_SHADOW='*'` too, under
`BOF3X_LANG` unset, `en` and `original`).

**Negative controls: 39 planted, 39 refused**, each by a comparison (a
count, a call's arguments, a snapshot, the return value or the cache) -
none by a hang. Run by a script that edits `glyph_draw.cpp`, builds, runs
the headless self-test and restores:

| # | control | refused by |
|---|---|---|
| 0 | `sx` times `D3d_ScaleY` | snapshot |
| 1 | `sy` from `+0xC` | snapshot |
| 2 | x read unsigned | snapshot |
| 3 | `sz` one ulp off | snapshot |
| 4 | `sz` and `rhw` swapped | snapshot |
| 5 | diffuse and specular swapped | snapshot |
| 6 | `tu` from `v` | snapshot |
| 7 | `u` read signed | snapshot |
| 8 | `tv` not doubled | snapshot |
| 9 | the inset added twice to `tu` | snapshot (inset pass only) |
| 10 | corner 3's `tu`, `tv` not written | snapshot |
| 11 | colour mode 1 | call 0's arguments |
| 12 | no specular pointer | call 0's arguments |
| 13 | colour code from `+6` | call 0's arguments |
| 14 | lookup arguments swapped | call 1's arguments |
| 15 | one bare `ret` dropped | the call count |
| 16 | blend code read before the lookup | call 4's arguments |
| 17 | shade mode 0 | call 5's arguments |
| 18 | alpha blend left on | call 6's arguments |
| 19 | 3 vertices drawn | call 7's arguments |
| 20 | triangle list | call 7's arguments |
| 21 | returns 0 | the return value |
| 22 | `sz` stored after the lookup | snapshot |
| 23 | 127 entries walked | the call count |
| 24 | generation ignored | `SetTexture`'s argument |
| 25 | generation row `clut >> 5` | the call count |
| 26 | glyph compared as 16 bits | the call count (a wide argument) |
| 27 | the last free entry, not the first | the build's slot |
| 28 | in-use word read as a byte | the build's slot |
| 29 | render flag bit 1 for bit 0 | the call count |
| 30 | no `EndScene` | the call count |
| 31 | `BeginScene` before the build | call 1 |
| 32 | texture stage 1 | `SetTexture`'s arguments |
| 33 | texture from `+8` | `SetTexture`'s arguments |
| 34 | in-use stored as 2 | the cache |
| 35 | in-use high byte not written | the cache |
| 36 | build arguments swapped | the build's arguments |
| 37 | the overrun clamped to 127 (a D-N1 fix) | `SetTexture`'s argument |
| 38 | the free slot returned on a hit | the return value |

**What the fuzz does not reach:** the real `D3d_PrimColor`, `D3d_SetBlend`,
`D3d_SetShadeMode`, `Font_BuildGlyphTexture` and the real device - all
stand-ins; whether a changed device pointer between the two COM calls is
re-read (ours re-reads it, as the original does, but no stand-in changes it,
so no control could tell); the software renderer's caller `0x5A4900`; and
anything drawn - the harness sees vertices and calls, never pixels.

## 6. DIV-0025: texel centres

D17 ([`known-defects.md`](known-defects.md)): a 12-unit glyph quad is 24
pixels showing 24 texels, 1:1, and with no half-texel offset every pixel's
sample point (Direct3D 6: the pixel's integer coordinate) falls exactly on
the edge between two texels - point filtering picks one by the rounding of
each triangle's interpolator (the owner's "wobbly" English), bilinear blends
the two 50 / 50 (soft).

Ours: `tu = (2u + 0.5) / 32`, `tv = (2v + 0.5) / 32`, on every corner - both
the near and the far edge move by half a texel, so the scale stays 1:1 and
pixel `k` of the quad samples the centre of texel `k`. Every value is exact
in float. The screen positions are untouched (no half-pixel offset), as in
the original.

Where it is: `g_texel_inset` in `glyph_draw.cpp`, 0 until `GlyphDraw_Inject`
patches it to 0.5 through `PatchBytes` under the name **`GlyphTexelCentres`**.
So:

- `BOF3X_ORIGINAL=D3d_DrawGlyph` - Capcom's function, the defect back;
- `BOF3X_ORIGINAL=GlyphTexelCentres` - our function with Capcom's
  arithmetic, byte for byte (the fuzz's exact pass is this configuration);
- `BOF3X_ORIGINAL=*` - both.

A run comparing pixels against an all-original reference will differ on every
frame with text unless `GlyphTexelCentres` is listed; the frame hash and the
call counts are unaffected (the same calls, the same order).

Only at scale 2.0 is it exactly texel centres; the scale was 2.0 in every mode
seen. At another scale the quad is not 1:1 anyway and half a texel is still
closer to right than the edge.

## 7. DIV-0027: the Yes / No chooser

Found statically (a sub-agent's read, re-checked here instruction by
instruction). The save menu (inside `pe_funcs.py`'s `0x57F340` extent) draws
"OK to overwrite?" - system message `0x9F` - through `Text_DrawAt` at
`(0x1C, 0x16)` (`0x57FF96`) and calls **`Menu_YesNo` `0x5747D0`** at
`0x57FFBF`. The same chooser serves `0x57FD6D` ("Do you want to save?",
message `0x1D`), `0x58838B` ("Load game?", `0xB9`) and `0x583C42` ("Is this
what you want?", `0x6D`, a screen not identified).

`Menu_YesNo`:

    005747D0  6A 0F              push 0xF
    005747D2  E8 69 2F F2 FF     call Msg_SystemPtr          ; the options line
    005747D7..005747E3           Text_DrawAt(0x1C, 0x16, 0, 0xFF, line)
    005747E8  66 0F BE 05 0B 9F 92 00  movsx ax, byte [0x929F0B]  ; 1 Yes, 0 No
    005747F0  B9 FE 00 00 00     mov ecx, 0xFE
    005747F7  8D 04 C0           lea eax, [eax+eax*8]
    005747FC  C1 E0 02           shl eax, 2
    005747FF  2B C8              sub ecx, eax                ; x = 0xFE - 36 * sel
    00574802  E8 C9 BD 01 00     call Menu_DrawHand(x, 0x18, 0)

then the buttons: confirm returns 1 (sound `0x104` for Yes, `0x106` for No),
cancel sets No and returns 1, left / right flips the byte (sound `0x101`).
`Menu_DrawHand` `0x5905D0` draws a `0x18 x 0xC` sprite at `x - 0x16`; the
owner's measurements put its visible tip at `x - 1`.

**The words' x is the line's own spaces.** The system pool (`FIRST.DAT` and
`en.FIRST.DAT`, tag `0x4000`, parsed 2026-09-22): the Chinese line is 16
spaces, a one-code word, 2 spaces, a two-code word - at 12 units a character
its words start at 28 + 192 = 220 and 256, where the hand's stops 218 and
254 put its tip. The English line is 27 spaces, `Yes`, 1 space, `No` - at 8
units, 244 and 276: 25 units past the hand's tip on Yes, 22 on No. This
matches the owner's measurements to 2 px (hand on Yes 394-437 px, `Yes` at
490, hand on No 464-506, `No` at 552).

**Ours** (`src/game/yes_no_layout.cpp`), the owner's layout of
[`dialogue-localisation.md`](dialogue-localisation.md) §6 item 8:

| | left hand | Yes | right hand (visible) | No |
|---|---|---|---|---|
| today, units | 196-217 | 244-267 | 232-253 | 276-291 |
| ours, units | 196-217, kept | 220-243 | 252-273 | 276-291, kept |
| ours, px (x2) | 392-434 | 440-486 | 504-546 | 552-582 |
| owner's target, px | 394-437 | ~442-491 | ~501-546 | 552-584 |

- the line: `Msg_SystemPtr`'s call at `0x5747D2` re-aimed
  (`RetargetCall`) at `YesNo_Line`, which returns the pool's line with three
  spaces moved from its lead to its gap - shape-checked (spaces, a word,
  spaces, a word; else `Fatal`); `Yes` 3 units right of the left tip;
- the hand: `0x5747F0` `mov ecx, 0x112`, `0x5747F7` `imul eax, eax, 56`
  (`6B C0 38`), `0x5747FC` three `nop`s - stops 218 (kept) and 274, the right
  tip 3 units before `No`. The product's high half is garbage as in the
  original (`movsx ax`), and `Menu_DrawHand` keeps only the low 16 bits
  (`and ecx, 0xFFFF` at `0x590601`).

Only under a language overlay (`BOF3X_LANG` set and not `original`);
`BOF3X_ORIGINAL=YesNoLayout` leaves all four sites alone. All four prompts
change together. `BOF3X_SHADOW=yes_no_layout` checks the re-spacing on the
English line's shape and the Chinese line's (two-byte codes) at start-up.

**Not checked:** anything on screen - no recipe reaches a save point; the
owner judges a capture. The PSX US chooser (the recomp's hand tips at about
246 and 277 units) is unread.

## 8. DIV-0026: the Config controller panel, on paper

[`config-screen.md`](config-screen.md) §8 has the owner's report and the
cause. Built: DIV-0017's pair on `0x461AF0` - its width `lea eax,
[ecx+ecx*2]` / (`mov ecx, ebp`) / `shl eax, 1` at `0x461B36` becomes `len *
4` by one SIB byte (`8D 04 49` -> `8D 04 09`, `[ecx+ecx]`; the `mov` between
the two is left alone - the five-byte `89 C8 C1 E0 02` the queue proposed
would have overwritten it), and its `Text_DrawAt` call at `0x461B43` re-aimed
at `ConfigText_DrawSelected` (the dialogue font). Switch:
`ConfigController`; not under `BOF3X_LANG=original`.

**Where the names land.** The sub-panel `0x461A50(px, py)` draws its frame
(`Menu_DrawFrame`, DIV-0011) at `(px, py)`, `0x0C x 0x0F` cells of 8 - 96 x
120 units - and each row as `0x461AF0(px + 5, py + 6 + 18 i, name)` and
`0x461C00(px + 0x58, ...)`. `0x461AF0(x, ...)` draws a row box
`0x57CF60(x, y, 0x68, 0x10)`, the name ending at `x + 0x20` (panel
`px + 37`), and two separators at `x + 0x3F` and `x + 0x53` (panel `px + 68`
and `px + 88`); `0x461C00` puts the button's two strings at `px + 0x48` and
`px + 0x5C`. With DIV-0026 a name of n letters spans `px + 37 - 8n ..
px + 37`:

| name | letters | starts at | against the panel |
|---|---|---|---|
| Move, Menu, View | 4 | `px + 5` | inside the frame, but on its 8-unit edge strip |
| Speak | 5 | `px - 3` | 3 units outside the frame |
| Action, Change | 6 | `px - 11` | 11 units outside the frame |

(Check against the owner's screenshot: today's `len * 6` puts the 4-letter
names at `px - 11`, measured 315 px, so `px` is about 168 units; the frame's
left edge then sits near 337 px and "Change" will start near 315 px.)

**It clearly will not fit, so the anchor needs to move - proposed, not
built.** Between the frame's inner edge (`px + 8`) and the first separator
(`px + 68`) there are 60 units, room for 7 letters. The proposal: the name's
right edge from `x + 0x20` to `x + 0x3C` - the `add ecx, 0x20` at `0x461B3F`
(`83 C1 20` -> `83 C1 3C`) - right-aligned as the Chinese is, 3 units before
the separator: "Change" and "Action" at `px + 17`, the 4-letter names at
`px + 33`. The alternative is left alignment at `px + 9` (a different patch:
the subtraction would go). Which the PSX US build does is unread. The owner
judges off `tools/recipes/config_controller.txt` in the batch, then picks.

**Found on the way:** `ConfigText_Inject` and `MenuVerbs_Inject` apply their
layout patches whenever `BOF3X_LANG` is set, `original` included, while
`DatLoad_Inject` reads `original` as no overlay. So a run with
`BOF3X_LANG=original` (as `attract_run.py` pins it) gets DIV-0015's anchor and
DIV-0017's widths over Chinese text. Ours (DIV-0026, DIV-0027) make the
exception; the older ones were left as they are - a change of their
behaviour is not this group's.
