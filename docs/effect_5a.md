# Group E5A: kind 0x18's sub-kinds 1, 4..0xA, 0x1A and 0x1F

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..15),
wave five, from the round branch's tip `0834edf`. **55 functions ours**
(`src/game/effect_5a.cpp`, shadow name `effect_5a`): 51 of the cut table's 53
rows for E5A (`analysis/round13_cut.tsv`, the band `0x4FD2E0..0x4FF150`) - its
other two, `0x4FD350` and `0x4FD3E0`, were ours already
(`Gfx_DrawSkyGradient`, `Gfx_DrawSunsetGlow`, `area_backdrop.cpp`, DIV-0041) -
and four starts no list of the cut has: sub-kind 4's states 1 `0x4FD4C0` and
3 `0x4FD630`, sub-kind 0x1A's draw `0x4FDCC0` and sub-kind 7's top draw
`0x4FE640` (section 5). Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
220,000 rounds, 0 mismatches; 55 of 55 controls refused by a count. **Fuzz
only**: no recorded route's first-call trace enters any of them (section 9).

Every row is effect code, every tier `evidence`; no row is a `hypothesis` row
and none is not effect code. Kind 0x18 is one kind with many sub-kinds:
`EffectKind18_Start` (ours, `worldmap_area.md` section 6) copies the record's
`+0xB` into `+1`, and `EffectKind18_Run` jumps through `EffectKind18_States`
(`0x65406C`) by `+1`. Ten of its entries point into this band: eight are
**sub-state dispatchers** by `+2` through a table of their own (none bounded),
two (`[1]` and `[9]`) are a sub-kind's one state.

| Sub-kind (`EffectKind18_States[n]`) | Functions | What the code does |
|---|--:|---|
| 1 (`0x654070`) | 1 | the sunset sky by `Cond_ByteFE`: 1 - `Gfx_DrawSkyGradient(0x40, 0x5A, 0xFF)` and the step word `+0x2E` 0; 2 - the step up to 0xBF, the gradient `(c + 0x40, 0x5A, max(0xFF - 2c, 0))`, then `Gfx_DrawSunsetGlow` (area 23's cutscene, DIV-0041's trace) |
| 4 (`0x65407C`) | 6 | two textured panels at fixed map cells (`0x468000`, `0x478000` / `0x498000`), shut until flag 0x17 of `Cond_Flags` row 2, then turned apart about z by `+0xC` (0x20 a frame to 0x400), then one panel left at 0x400 |
| 5 (`0x654080`) | 6 | CLUT animation: 16-colour slots of two strip rows (`0x65DAF8` / `0x65DAFD` by `+0x36`) copied into slot 0 - start, idle, fade in, pulse, fade out - switched by a nibble test of the word `+0x3A` against the story-flag dword `0x904030` |
| 6 (`0x654084`) | 6 | a part set (`+0x36`, three) drawn from 8-byte part records, its height `+0x3E` one of four levels (`+0x3A`); each time its story flag (`+0x34`) is set it steps to the next level over 16 frames, clears the flag and rewrites the map's bytes and height bytes under its rectangle |
| 7 (`0x654088`) | 6 | a two-panel door (`+0x36` picks one of three) with a top piece: shut, its two cells of map row 0x29 blocked (0x50); on its flag it slides 0x100 over 32 frames and frees them |
| 8 (`0x65408C`) | 8 | the same panels on flags 5 and 6 of row 3: shaken eight frames, pushed, toppled through a turning draw (0x29 frames, sound 0x205), crashed under seven puffs, left as six floor tiles |
| 9 (`0x654090`) | 1 | three story flags (`0x65DE60`) as a pattern 1..8 (`0x4FEE70`): on a change `+2`, a height and a 3 x 4 block of map bytes (0x10 or 0); `Cond_ByteFE` by the pattern every frame |
| 0xA (`0x654094`) | 7 | a pair of doors of eight parts each: shut until `Cond_ByteFE`, swung 0x41 frames, held, slid apart 0x11 frames, open |
| 0x1A (`0x6540D4`) | 7 | a block (texture `0x21800133`, linked at `(0x5D0000, 0x340000)`) slid one cell out (`+0x38` to `0x350000`, `+0x3E` up 0x80) when `Cond_ByteFE` is set and back when it clears |
| 0x1F (`0x6540E8`) | 5 | CLUT slots of rows 3, 6 and 7: on story flag 7 brightened over six frames, then cycled by `+9 & 3` |
| (shared) | 1 | `Gfx_ClutStripCopy16` `0x4FDB70`: a 16-colour slot of `Gfx_ClutStrip` copied to another, the strip marked dirty - sub-kinds 5 and 0x1F, 16 call sites |

What an effect looks like in the game, and which area spawns each sub-kind,
is not settled here beyond the code: the spawners set `+0xB` from area data,
not from a function of the round (no `area_w*.cpp` stores these sub-kinds).
Sub-kind 1's area is DIV-0041's measurement, not a reading.

## 1. What each function does

Names are the reading's; `+n` is a byte of the `Sprite_Current` effect record
unless a width is given. Every state reads `Sprite_Current` again after each
call, as the original does (the disturbance moves it).

### 1.1 Sub-kind 1

- `EffectKind18_01_Sunset` `0x4FD2E0`: the table above. The step compare is
  signed (`cmp cx, 0xBF; jge`); the blue `0xFF - 2c` floored at 0 (`setle` /
  `dec` / `and`).

### 1.2 Sub-kind 4 (`EffectKind18_04_States` `0x65DAE8`, 4)

- `_Run` `0x4FD470`: `jmp [0x65DAE8 + +2 * 4]`.
- `_Start` `0x4FD490`: `Flags_Test(Cond_Flags row 2, 0x17)`: set - `+2` 3 and a
  tail jump to `_Open`; else `+2` up one and a tail jump to `_Shut`.
- `_Shut` `0x4FD4C0`: the flag set - `+2` up, the dword `+0xC` 0; the two
  panels at angles 0 and 0x800 (`_DrawPanel`), linked with dy 0 and -1.
- `_Swing` `0x4FD570`: `+0xC` at 0x400 or past (signed) - `+2` up; `+0xC` +
  0x20; the panels at `+0xC` and `0x800 - +0xC` (words).
- `_Open` `0x4FD630`: `Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0x95, 0)` linked
  at the first panel's cell (size 0xC), one panel at 0x400.
- `_DrawPanel` `0x4FD6A0` (cdecl `(point, angle, texture)`, answers the
  primitive, read by no caller): the matrix pushed, translated by
  `Gte_RotTrans(point)`, rotated by `(0, 0, angle)`, times `Camera_Matrix`;
  a POLY_FT4 of `(0, 0x80, -0x17E)`, `(0, 0, -0x17E)`, `(0, 0x80, 0)`,
  `(0, 0, 0)`; popped; `Prim_SetTexture(texture, prim, 1)`.

### 1.3 Sub-kind 5 (`EffectKind18_05_States` `0x65DB04`, 5)

"On" is `(((+0x3A sar 4) & (dword 0x904030 sar 4)) ^ +0x3A) & 0xF == 0`, the
word `+0x3A` signed. Row a is `0x65DAF8[+0x36]`, row b `0x65DAFD[+0x36]`
(five each); "slot s to slot 0" is `Gfx_ClutStripCopy16(row, s, row, 0)`.

- `_Start` `0x4FD800`: on - `+2` 3 (`_Pulse`), a's slot 9; else `+2` up
  (`_Idle`), a's slot 1; then b's slot 8.
- `_Idle` `0x4FD890`: a's slot 1, b's slot 8; on - `+9`, `+0xA` 0, `+2` up.
- `_FadeIn` `0x4FD910`: `+9 % 5 == 0` - b's slot `8 - +9 / 5`; `+9` up;
  `+0xA < 9` - a's slot `+0xA + 1`; `+0xA` up; `+9 > 0x1E` - both 0, `+2` up.
- `_Pulse` `0x4FD9D0`: `+9 & 3 == 0` - b's slot `0x65DB18[(+9 >> 2) % 6]`;
  a's slot 9; not on and `(+9 >> 2) % 6 == 0` - `+9`, `+0xA` 0, `+2` up and
  `+9` up (1); else `+9` up.
- `_FadeOut` `0x4FDAA0`: every fifth frame b's slot `+9 / 5 + 2`; `+9` up;
  `+0xA` even and below 9 - a's slot `0x65DB20[+0xA >> 1]`; `+0xA` up;
  `+9 > 0x1E` - both 0, `+2` 1.
- `Gfx_ClutStripCopy16` `0x4FDB70` (cdecl, four words): 8 dwords from
  `Gfx_ClutStrip + ((from_row << 4) + from_slot) << 5` to the same of the
  destination, first word first; `Gfx_ClutStripDirty` 1.

### 1.4 Sub-kind 0x1A (`EffectKind18_1A_States` `0x65DB28`, 5)

`_Start` `0x4FDBE0` (`+0x3E` 0x320, `+2` up), `_WaitOn` `0x4FDC00`
(`Cond_ByteFE` not 0: up), `_SlideOut` `0x4FDC20` (`+0x38` + 0x2000, `+0x3E`
+ 0x10, at exactly `0x350000` up), `_WaitOff` `0x4FDC60` (`Cond_ByteFE` 0:
up), `_SlideBack` `0x4FDC80` (- 0x2000, - 0x10, at `0x340000` `+2` 1); each
tail-jumps to `_Draw` `0x4FDCC0`: a POLY_FT4 about `((+0x34 sar 9) -
0x4000, (+0x38 sar 9) - 0x4000)` (x -0x40 / +0xC0, the second word -/+
0x30), the third word `+0x3E` -/+ 0x30.

### 1.5 Sub-kind 0x1F (`EffectKind18_1F_States` `0x65DB3C`, 4)

`_Start` `0x4FDDF0` (`+9` 0; story flag 7 set - `+2` 3; else `(3, 0)` to
`(3, 3)`, `+2` up), `_Wait` `0x4FDE30` (flag 7: up), `_Brighten` `0x4FDE50`
(`(3, (+9 >> 1) + 5)` to `(3, 3)`, `(6, (+9 >> 1) + 0xB)` to `(6, 9)`; `+9`
past 5: up), `_Cycle` `0x4FDEB0` (by `+9 & 3`: `(3, 0x65DB4C[i])` to `(3, 3)`,
`(7, 0x65DB5C[i])` to `(6, 9)`, `(7, + 1)` to `(6, 0xA)`; `+9` up).

### 1.6 Sub-kind 6 (`EffectKind18_06_States` `0x65DB78`, 3)

- `_Start` `0x4FDF40`: the word `+0x34` the story flag `0x65DB84[+0x36]`, the
  word `+0x3E` the height `0x65DB88[+0x3A]`; `+2` up; `_SetMap`, `_Draw`.
- `_Wait` `0x4FDF80`: the story flag `+0x34` (its byte) set - `+9` 0, `+2`
  up, sounds 0x202 and 0x201; `_Draw`.
- `_Turn` `0x4FDFD0`: `+0x3E` + the s8 `0x65DB90[+0x3A]`; `+9` up; at 0x10 -
  the word `+0x3A` `(low byte + 1) & 3`, the flag cleared, `+2` down (back to
  `_Wait`); `_SetMap`, `_Draw`.
- `_SetMap` `0x4FE040`: over the rectangle `0x65DB6C[+0x36]` (x0, z0, x1,
  z1), `AreaMap_Bytes[w * z + x]` 0x21 (0 at the level `0xFE00`), `w` the
  low byte of `AreaMap_Header`; then every cell's height byte at
  `AreaMap_Header + 4 * AreaMap_HeightBase + x + w * z` =
  `(-(s16) AreaMap_Elevation(x << 16, z << 16) - 2 * +0x3E) / 32` (toward
  zero). The bounds are read again from the record each pass.
- `_Draw` `0x4FE1A0`: the parts `0x65DB9C[+0x36]` (first, end; end read again
  each pass) of the 8-byte records at `0x65DBE0`: byte 0 a dy class, byte 1 a
  shape row (12 bytes at `0x65DBA4`), bytes 2 and 3 the cell, `+4` the
  texture; a POLY_FT4 per part, linked at the cell with dy
  `0x65DB94[byte 0 * 2 + (+0x3E != 0xFE00)]`.

### 1.7 Sub-kind 7 (`EffectKind18_07_States` `0x65DDC4`, 4)

- `_Start` `0x4FE350`: `+0xB` = `+0x36` (the door, k); its flag
  (`Cond_Flags` row `0x65DDB4[k]`, bit `0x65DDB8[k]`) set - `+2` 3, the dword
  `+0x38` `0x65DDB0[k] + 2`; else `+2` up, `+0x38` `0x65DDB0[k]` and
  `AreaMap_SetByte(0x29, row, 0x50)`, `(0x29, row + 1, 0x50)`; then `+0x38 =
  (+0x38 << 7) - 0x4040`, `+0x34 = -0x2B80`; `_Draw`, `_DrawTop`.
- `_Wait` `0x4FE440`: the flag - `+9` 0, `+2` up; both draws.
- `_Slide` `0x4FE490`: `+9` up, `+0x38` + 8; at 0x20 - `+2` up, the two cells
  0; `_Draw` only (a tail jump).
- `_Draw` `0x4FE510` (also `[3]`, the end state): two POLY_FT4 stacked 0x80
  apart from `+0x38`, x `+0x34`, z -0x200 / -0x80, textures `0x65DDBC[i]`,
  linked one above.
- `_DrawTop` `0x4FE640`: one POLY_FT4 at x `+0x34 + 0x10`, `+0x38 + 0x80 ..
  0x100`, z -0x160 / -0xE0, texture `0xBD5000F9`.

### 1.8 Sub-kind 8 (`EffectKind18_08_States` `0x65DDD4`, 7)

- `_Start` `0x4FE770`: flag 6 of row 3 set - `+2` 6, `+0x34` -0x2940; else
  `+2` up, `+0x34` -0x2B80; `+0x38` -0x31C0, `+0x3E` -0x80; sub-kind 7's two
  draws.
- `_Wait` `0x4FE7E0` (flag 5: `+9` 0, up), `_Shake` `0x4FE810` (`+0x34`
  `(+9 & 1) * 8 - 0x2B80` for eight frames), `_WaitPush` `0x4FE860` (flag 6:
  `+0x34` + 0x40, up); each with both draws.
- `_Topple` `0x4FE8A0`: `_DrawTilted(0x65DDF0[+9] << 4)`; past 0x28 - sound
  0x205, `+0x3E` -0x88, up.
- `_Crash` `0x4FE920`: `0x65DE1C[+9]` not 0 - `_DrawTilted(it << 4)`, else
  `_DrawFallen`; seven puffs - a point spread by `+9` (`0x65DE2C` / `0x65DE3C`
  pairs) through `Gte_RotTransPers` into `MapView_ScreenXY` and
  `Gte_PrimDepthFlat4_10`, the corners `MapView_ScreenXY` -/+ `+9` as floats
  (x87 `fild` / `fsubr` / `fadd`, ours by the same instructions), texture
  `0xBA689124 - (+9 << 19)`, linked dy 6; past 0xD - up.
- `_DrawFallen` `0x4FEAF0` (also `[6]`): six floor tiles at the cells of
  `0x65DE4C`, z -0x88, textures `0x65DE58[k] - 0x42B00000`. Reads nothing of
  the record.
- `_DrawTilted` `0x4FEC20` (cdecl `(angle)`): the matrix at the record's
  `(+0x34, +0x38, +0x3E)` words rotated `(0, -angle, 0)`; sub-kind 7's two
  panels (z -0x180 / 0), linked dy 2; popped. Its argument's stack slot is
  reused as the 0x80 counter.

### 1.9 Sub-kind 9

- `EffectKind18_09_Pattern` `0x4FEDD0`: `0x4FEE70()` (1..8) not `+2` - `+2`
  = `0x4FEE70()` again, `+0x3E` the word `0x65DE62[+2]`, the map's bytes x
  0x42..0x44, z 3..6 = 0x10 (0 at `+0x3E` -0x80); then `Cond_ByteFE =
  0x65DE6F[+2]`.

### 1.10 Sub-kind 0xA (`EffectKind18_0A_States` `0x65DE78`, 5)

`_Shut` `0x4FEF70` (doors at 0 / 0x800, x `0x858000` / `0x878000`, variants
0 / 1; `Cond_ByteFE` - up, `+9` 0), `_Swing` `0x4FEFC0` (angles `+9 << 4`,
`(+9 + 0x80) << 4`, variant + 0x10 past 0x30; past 0x40 up), `_Hold`
`0x4FF060` (0x400 / 0xC00, variants 0x10 / 0x11, five frames), `_Slide`
`0x4FF0B0` (x `(0x858 - +9) << 12`, `(+9 + 0x878) << 12`, 0x11 frames),
`_Open` `0x4FF120` (x `0x848000` / `0x888000`, the end state). `_DrawDoor`
`0x4FF150` (cdecl `(angle, x, variant)`): the matrix at `((x sar 9) - 0x4000,
0xC5C0, -0x80)` rotated `(0, 0, angle)`; eight parts, each drawn when its
texture (`0x65DEEC[i]`, by the variant's bit 0) is not 0: four vertices from
`0x65DE8C` (x, y `<< 7`, z `* -320`), the texture with
`(((angle sar 5) & 0x38) + 0x50) << 16` or'ed in when its bits 19..23 are 0,
linked at `(x, 0xB8000)` with dy `0x65DF2C[(variant sar 4) + 2 (variant & 1)
+ 4 i]`.

## 2. Divergence

None. Every function is a faithful replacement; where the original indexes
past a table ours aborts (section 7, the round-nine rule, no ledger entry).
No module patches bytes inside these 55: `DIVERGENCE.md`, `cheats.cpp` and
`widescreen.cpp` name none of them (DIV-0041 widens the two callees sub-kind 1
draws through, which are `area_backdrop.cpp`'s, not a site in this band). No
full-frame fill here.

## 3. The tables and the arguments

Eight `[[data]]` entries, each the dispatcher's own reach read by hand (the
next table a dispatcher indexes, or the first dword that is not code), checked
against what the states store into `+2`:

| Table | Count | Read by | Ends at |
|---|--:|---|---|
| `EffectKind18_04_States` `0x65DAE8` | 4 | `0x4FD470` | bytes at `0x65DAF8` (sub-kind 5's rows) |
| `EffectKind18_05_States` `0x65DB04` | 5 | `0x4FD7E0` | bytes at `0x65DB18` |
| `EffectKind18_1A_States` `0x65DB28` | 5 | `0x4FDBC0` | `0x65DB3C`, the next table |
| `EffectKind18_1F_States` `0x65DB3C` | 4 | `0x4FDDD0` | dwords 9, 0xA, .. at `0x65DB4C` |
| `EffectKind18_06_States` `0x65DB78` | 3 | `0x4FDF20` | bytes at `0x65DB84` |
| `EffectKind18_07_States` `0x65DDC4` | 4 | `0x4FE330` | `0x65DDD4`, the next table |
| `EffectKind18_08_States` `0x65DDD4` | 7 | `0x4FE750` | a null at `0x65DDF0` |
| `EffectKind18_0A_States` `0x65DE78` | 5 | `0x4FEF50` | bytes at `0x65DE8C` |

The brief's hint counted 9, 7, 7, 6, 6, 5, 5 and 4 (`0x65DDD4`, `0x65DB04`,
`0x65DE78`, `0x65DB28`, `0x65DB78`, `0x65DB3C`, `0x65DDC4`, `0x65DAE8`): the
cut's count runs on into the next table or over the dwords of bytes after it;
only `0x65DAE8`'s agreed. The byte and word tables the states index (section 1) are read in
place and not named: their extents, where a record byte indexes them, are in
`effect_5a_callees.h` and section 7. Arguments: `_DrawPanel` reads its point
(6 bytes) and the angle's word; `Gfx_ClutStripCopy16` four whole words;
`_DrawTilted` one word; `_DrawDoor` three words (the angle's low word for the
rotation and bits 8..10 for the texture page, x whole, the variant whole).
None answers what a caller reads (`_DrawPanel`'s primitive is ignored): no
`ret_mask`.

## 4. The fuzz (`effect_5a_fuzz.cpp`)

`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_5a`: **220,000 rounds over 55
functions (4,000 each), 1,476,857 calls to the stand-ins, 0 mismatches**;
32,948 bytes of state in 46 regions (in this worktree). 51 clones are
`kEffect` (kind 0x18, the dispatchers and states with `sub_span` their table's
length), four `kCall` (`_DrawPanel` with argument 0 a scratch pointer,
`Gfx_ClutStripCopy16`, `_DrawTilted`, `_DrawDoor`). Every entry of every table
was reached (the coverage line: 525..1,385 a handler).

- **Callees listed** (group's own by name, so the clones' `E8` / `E9` and ours'
  `SH_CALL` meet one recorder): `_Shut`, `_Open`, `_DrawPanel` (the point
  hashed 6 bytes - its pad is the caller's stale stack -, the angle's word:
  `0x4FD570` pushes `0x800 - +0xC` in `ax` over a stale `eax`),
  `Gfx_ClutStripCopy16`, the draws and `_SetMap` (`kPhase`), `_DrawTilted`,
  `_DrawDoor`; `Gfx_DrawSkyGradient` (bytes) and `Gfx_DrawSunsetGlow`
  (`kPhase`), ours of `area_backdrop.cpp` but in no standard set; **`0x4FEE70`**
  (no group's) answering a whole `eax` of 1..8 - a quarter of the time the
  current `+2` - over effect mode's `kFlag` row, which leaves garbage above
  `al` that `_Pattern`'s whole-word compare and table index cannot take;
  **`MapView_LinkPrimAt`** re-listed with its dy compared on the low byte (the
  real one reads a signed byte, `symbols.toml`) and the effect row's cursor
  advance: `_DrawDoor` pushes `dl` over what `Prim_SetTexture` left in `edx`,
  which mismatched 4,000 of 4,000 rounds under the standard `kAll` mask.
- **Region**: `Gfx_ClutStrip`'s first 16 rows (`0x80F580`, 0x2000): the rows
  sub-kinds 5 and 0x1F copy within (3, 5..0xE).
- **Seeds** (all 20 records, since the disturbance moves `Sprite_Current`
  among them after a call): `+0x36` below the indexed table's length (five
  rows for sub-kind 5, three sets for 6, three doors for 7), `+0x3A` 0..3 for
  sub-kind 6, `+0xB` 0..2, `+9` below the clone's span (0x24 for 5 and 7,
  0x10 for 0x1F and 8, 0x14 for 6, 0x29 / 0xE for `_Topple` / `_Crash`, 0x48
  for 0xA), `+0xA` 0..0xB, `+2` 0..9 for `_Pattern`. On the current record:
  `+9` and `+0xA` at their compares (0, 4, 5, 8, 0x10, 0x1E..0x20, 0x28,
  0x30, 0x31, 0x40, 0x41), the step word `+0x2E` about 0xBF, `+0xC` about
  0x400, `+0x38` at and beside `0x340000` / `0x350000`, `+0x3E` at `0xFE00`,
  `-0x80` and beside; `Cond_ByteFE` 0..3; `AreaMap_HeightBase` below 0x600
  (`_SetMap` writes `4 *` it past `AreaMap_Header`, which a random word puts
  outside the area block's 8 KiB, on both sides and outside the regions).
- **Arguments**: the CLUT copy's rows below 0xF and slots below 0x10; the
  door's variant one of `0, 1, 0x10, 0x11` (its callers') or `0x20, 0x30`.
- **Disturbance** (from its hash): `+9` inside the clone's span, `+0xA`,
  `Cond_ByteFE`, the nibble word `+0x3A` (not for sub-kind 6), `+0xC`, `+0x3E`.
- One seed bug found on the way, not a defect: the dword seed of `+0x38`
  overwrote sub-kind 6's level word `+0x3A`, and ours aborted on the index
  (the original read past `0x65DB88`): `+0x38` is now seeded only outside
  sub-kind 6.

**Every shadow** (in this worktree, at this branch's tip): `BOF3X_SHADOW='*'`
exit 0, `inject: 8158 ours, 0 left original` (8,103 + these 55), every
self-test line at 0 mismatches; again with `BOF3X_WIDE=1`, exit 0. Neither run
died silently. `tools/ledger_check.py`: 70 entries, 0 errors.

**Not re-listed, and why**: `Gte_RotTransPers` (effect mode's row fills
`MapView_ScreenXY` with fractional floats, which `_Crash`'s x87 corners read:
both sides compute the same); `Gte_RotTransPers4`, `Gte_RotTrans`,
`Gte_RotMatrix`, `Gte_MulMatrix0`, `Gte_SetRotMatrix`, `Gte_SetTransMatrix`
(their rows hash six-byte SVECTORs and 18-byte matrices, so the stale pads
compare nothing); `Flags_Test` / `Flags_Clear` (their `kU8` index masks
cover `_Wait`'s `push ecx` over `Sprite_Current`'s upper bytes and `_Turn`'s
`push eax` over the pointer's); `AreaMap_SetByte` (`kU16`, `kU16`, `kU8`).
`EffectGte_ProjectSize`, `Math_Cos`, `0x5A7A90` are not called here.

## 5. What the cut and the tool said, settled

- **Already ours**: `0x4FD350` (`Gfx_DrawSkyGradient`) and `0x4FD3E0`
  (`Gfx_DrawSunsetGlow`), `area_backdrop.cpp` (DIV-0041's sunset): not taken
  again; `EffectKind18_01_Sunset` calls them by name.
- **Added, a table entry the tool counted into its neighbour**: `0x4FD4C0`
  (`EffectKind18_04_States[1]`; `0x4FD490` tail-jumps to it; its own frame and
  `ret`). The tool's `0x4FD490` is `0xDE`; read, it is `0x29` (to the
  `jmp`).
- **Added, code no list has**: `0x4FD630` (`[3]` of the same table, after
  `0x4FD570`'s `ret`; the cut said `0x4FD570` was 301 bytes, it is `0xB8`) and
  `0x4FE640` (the top draw six states tail-jump to; the cut said `0x4FE510`
  was 576 bytes, it is `0x124`).
- **Added, a shared draw with its own frame**: `0x4FDCC0` (the tail of
  sub-kind 0x1A's five states, EKH's "second entry inside E5A's `0x4FDC80`",
  section 8.5); `0x4FDC80` is `0x32` (to its `jmp`), not the tool's `0x14C`.
- **Hosts**: the recorded extents of `Gfx_LinkOTags` (`0x4FD290`, Capcom's),
  `Gfx_DrawSkyGradient` (`entries_logic.txt` `004FD350 34D`) and the
  unlabelled `0x4FEE70` (`2D6`) swallowed rows of this group; none of their
  code contains ours (each ends at its own `ret`: `0x4FD2D9`, `0x4FD3DD`,
  `0x4FEEA4`). The smaller extents are appended (section 11).
- No `NOTFN` start, no jump-table case, no `hypothesis` row, no row that is not
  effect code. Every other extent agrees with the tool's (the cut's sizes
  include padding).
- **Code in the band in no group, not taken** (for the coordinator):
  `0x4FEE70` (64 bytes, an unlabelled catalog row: the three story flags of
  `0x65DE60` as bits, plus 1; called by `_Pattern` twice and by `0x4FEEB0`)
  and `0x4FEEB0` (160 bytes, `Area_CellHooks`' entry for area 0x6D, Capcom's:
  on cell (0x1C, 6) with the leader's `+8` 3 and story flag 0x1C clear, the
  pattern `% 6` rewritten into those three flags, `Effect_HoldFlag1C(0xF)`,
  sounds 0x206 and 0x202, `al` 1). `Gfx_LinkOTags` `0x4FD290` is named,
  Capcom's, a platform row.

## 6. Controls

`controls.py` in the session scratchpad (`e5a/`): a mutant planted in each
function's body (anchored on a string unique within it), the module rebuilt,
the self-test run on that clone alone (`BOF3X_E5A_ONLY`, 2,000 rounds), the
source restored and rebuilt. Batched - every mutant sits in a different
function and ours reaches its siblings only through the harness's recorders -
except `_07_Draw` and `_08_Wait`, whose names are prefixes of others', run in
batches of their own. **55 of 55 refused by a count** (exit 3):

| Function | Mutant | Rounds refused |
|---|---|--:|
| `_01_Sunset` | blue 0xFF -> 0xFE at Cond_ByteFE 1 | 398 |
| the eight `_Run` | entry 1 instead of entry 0 | 272..647 |
| `_04_Start` | `+2` 3 -> 2 | 1,296 |
| `_04_Shut` | `+0xC` 0 -> 1 | 1,268 |
| `_04_Swing` | `>= 0x400` -> `> 0x400` | 220 |
| `_04_Open` | angle 0x400 -> 0x401 | 2,000 |
| `_04_DrawPanel` | v0.z -0x17E -> -0x17D | 2,000 |
| `_05_Start` | slot 9 to slot 1 | 89 |
| `_05_Idle` | `+9` 0 -> 1 | 87 |
| `_05_FadeIn` | `8 - n / 5` -> `/ 6` | 299 |
| `_05_Pulse` | `% 6 == 0` -> `== 1` | 1,137 |
| `_05_FadeOut` | `+2` 1 -> 2 | 467 |
| `Gfx_ClutStripCopy16` | dirty 1 -> 2 | 2,000 |
| `_1A_Start` | 0x320 -> 0x321 | 1,973 |
| `_1A_WaitOn` / `_WaitOff` | the test inverted | 1,961 / 1,961 |
| `_1A_SlideOut` | 0x350000 -> 0x34E000 | 132 |
| `_1A_SlideBack` | `+2` 1 -> 2 | 139 |
| `_1A_Draw` | link x + 1 | 2,000 |
| `_1F_Start` | slot 0 -> 1 | 672 |
| `_1F_Wait` | the test inverted | 2,000 |
| `_1F_Brighten` | `> 5` -> `> 4` | 137 |
| `_1F_Cycle` | slot 0xA -> 0xB | 2,000 |
| `_06_Start` | `+2` + 2 | 1,916 |
| `_06_Wait` | sound 0x201 -> 0x200 | 1,347 |
| `_06_Turn` | level + 2 | 188 |
| `_06_SetMap` | 0x21 -> 0x20 | 1,658 |
| `_06_Draw` | the dy class inverted | 2,000 |
| `_07_Start` | `+0x34` + 1 | 2,000 |
| `_07_Wait` | `+9` 0 -> 1 | 1,328 |
| `_07_Slide` | `>= 0x20` -> `>= 0x1F` | 150 |
| `_07_Draw` | dy 1 -> 2 | 2,000 |
| `_07_DrawTop` | 0x40C0 -> 0x40C1 | 2,000 |
| `_08_Start` | `+0x38` + 1 | 2,000 |
| `_08_Wait` | flag 5 -> 4 | 2,000 |
| `_08_Shake` | `>= 8` -> `>= 7` | 6 |
| `_08_WaitPush` | + 0x41 | 1,328 |
| `_08_Topple` | `+0x3E` -0x88 -> -0x87 | 129 |
| `_08_Crash` | `fsubr` -> `fadd` for the y corner | 1,951 |
| `_08_DrawFallen` | z -0x88 -> -0x89 | 2,000 |
| `_08_DrawTilted` | the angle not negated | 2,000 |
| `_09_Pattern` | map byte 0x10 -> 0x11 | 1,011 |
| `_0A_Shut` | variant 1 -> 0 | 2,000 |
| `_0A_Swing` | `> 0x30` -> `>= 0x30` | 152 |
| `_0A_Hold` | `> 4` -> `> 3` | 3 |
| `_0A_Slide` | 0x858 -> 0x857 | 2,000 |
| `_0A_Open` | x + 1 | 2,000 |
| `_0A_DrawDoor` | z * -320 -> * -319 | 2,000 |

The low counts (`_08_Shake` 6, `_0A_Hold` 3) are single-value boundaries
(`+9` exactly 7 or 4 after its increment) the seed reaches rarely; each is
refused.

## 7. Latent defects (Capcom's, described, not fixed)

- **Unbounded dispatchers**: the eight `_Run`s jump through `+2` with no
  compare; ours aborts past the table's count.
- **Unbounded table indexes by a record byte**: `+0x36` into sub-kind 5's rows
  (5), sub-kind 6's flags, rectangles and part runs (3), `+0x3A` into the
  levels (4), `+0xB` into sub-kind 7's doors (3), `+9` into `_Topple`'s
  (0x29) and `_Crash`'s (0xE) tables, `+2` into `_Pattern`'s heights and
  `Cond_ByteFE` values (1..8 by `0x4FEE70`), the door variant into
  `0x65DF2C` (32): the original reads what follows; ours aborts with a
  message. In play each byte comes from the same tables or a bounded count
  (`+9` resets at the compare that ends the state), so none is reached.
- **`Gfx_ClutStripCopy16` is unbounded**: rows and slots past the strip copy
  outside it. `_05_FadeIn`'s slot `8 - +9 / 5` goes negative past `+9` 44, but
  the state resets `+9` past 0x1E. Ours aborts outside the strip.
- **`_SetMap` writes the height bytes at `AreaMap_Header + 4 *
  AreaMap_HeightBase`** with no bound on the base word: in play it is the
  area's (the rows after the header), so not a defect by itself; in the fuzz
  it is seeded below 0x600.
- **`_DrawDoor`'s dy carries stale bytes**: `mov dl, [table]` over the `edx`
  `Prim_SetTexture` returned with; `MapView_LinkPrimAt` reads only the signed
  byte, so nothing reads them (the fuzz compares the byte, section 4).
- **`_DrawPanel` passes its own `angle` slot** to `Gte_RotTrans` as the flag
  out (Capcom's third word), and `_DrawTilted` reuses its argument's slot as a
  counter: both stack-only, read by nobody after.
- **Stale SVECTOR pads**: the points `_DrawPanel`'s callers, `_DrawTilted` and
  `_DrawDoor` build leave the fourth short as stack; no GTE function of ours
  reads it (DIV-0023's pad, the effect rows hash six bytes).

No stale read reaches a result: nothing for the ledger.

## 8. Calls across groups

- **Into other groups of the round**: none (`band_rows.py --edges`: 0 edges).
- **Raw, to a function no group holds**: `0x4FEE70` (`effect_5a_callees.h`
  `kPattern`), `_Pattern`'s two calls.
- **Ours of earlier rounds, by name**: `Gfx_DrawSkyGradient`,
  `Gfx_DrawSunsetGlow` (`area_backdrop.cpp`), and the standard callees.
- **Inbound from outside the group**: only `.data` cells -
  `EffectKind18_States` entries 1, 4..10, 26 and 31 (`EffectKind18_Run`,
  ours) - and the eight sub-state tables. **EKH's self-test** row
  `"E5A 0x4FD470"` (`scenario_harness_ekh.cpp`) copies `0x4FD470`
  (`EffectKind18_04_Run`) with Capcom's on both sides: it runs before
  `Effect5A_Inject` patches the entry, so it is unaffected.

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract, shop,
worldmap, combat) are empty for every row but `0x4FD2E0`, whose numbers are
negative: its host `Gfx_LinkOTags`' reach, not its own. No first-call trace
under `analysis/calltrace` names any of the 55 (`entries_logic.txt` had only
the host lines; the `reach_*_1003` logs only the inject line of
`Gfx_DrawSkyGradient`). **The group is fuzz-only.** One inference, not a
trace: DIV-0041's detail call trace of area 23's sunset (the nue recipe) found
`0x4FD350` and `0x4FD3E0` drawing it, and their only caller is `0x4FD2E0` -
so sub-kind 1 ran on that recipe. A frame-hash A/B on that recipe would
cover `EffectKind18_01_Sunset`.

## 10. The rebinding

- Rebound (comments; the values unchanged): `area_backdrop.cpp`'s two notes
  naming "0x4FD2E0, Capcom's" now name `EffectKind18_01_Sunset`.
- Left raw: `scenario_harness.cpp`'s `kEffectRuns` bound `0x4FD2E0` (a band
  edge, not a reference) and its comment in `scenario_harness.h`;
  **`scenario_harness_ekh.cpp`'s row `"E5A 0x4FD470"`** and the
  `inject_all.cpp` comment beside EKH (a harness file: not mine to edit - for
  the coordinator, should the row want the name).
- `area_backdrop.cpp`'s note "a word at 0x937F88 + 0x2E" means
  `Sprite_Current`'s record `+0x2E` (the step word), not the cell
  `0x937FB6`: a wording for the coordinator, left as it is.

**For the harness's fold** (not edited here): `MapView_LinkPrimAt`'s effect
row could compare dy on its byte (as the real one reads it); `0x4FEE70`'s
`kEffectStd` row is `kFlag`, but its only callers compare and index by the
whole `eax` (1..8); `Gfx_DrawSkyGradient` / `Gfx_DrawSunsetGlow` are in no
standard set.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (54 lines; `004FEAF0 123` was there):
every function's extent as read, among them the smaller extents of the six
that had host-sized lines (`004FD6A0 131`, `004FDB70 47`, `004FE040 157`,
`004FE510 124`, `004FEC20 1AE`, `004FF150 1C8`; `0x4FEAF0`'s agreed). The
hosts `004FD350 34D` and `004FEE70 2D6` are cut by these lines at the next
listed entry.
