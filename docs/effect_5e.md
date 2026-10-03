# Group E5E: effect kind 0x18's sub-kinds 0x23..0x26, 0x39 and 0x3F

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave five, from the round branch's tip `0834edf`. **54 functions ours**
(`src/game/effect_5e.cpp`, shadow name `effect_5e`): the cut table's 51 rows
for E5E (`analysis/round13_cut.tsv`, the band `0x506A10..0x508BA0`) and three
starts no list had - the draw `0x506AB0` that three sub-states tail-jump to
(its own frame and `ret`, inside the tool's extent of `0x506A80`), and the
dispatchers `0x506BD0` and `0x507620` of `EffectKind18_States` 36 and 63
(the brief's addendum: a dispatcher in the band no list holds is the
group's). None dropped. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to
it: 216,000 rounds, 0 mismatches; CONTROLS_SUMMARY. **Fuzz only**: no
recorded route enters any of the 54 (section 9). Every row is effect code (no
`hypothesis`-tier row in the group; none left original). **DIV-0041's two
listed full-frame fills in this band, `0x507BDC` and `0x507CE3`, are
widened** (section 2); no new ledger entry (the coordinator amends DIV-0041).

All six are sub-kinds of effect kind 0x18: `Effect_KindHandlers[0x18]` is
`EffectKind18_Run` `0x46D830` (ours), which jumps through
`EffectKind18_States` by `+1`; `EffectKind18_Start` sets `+1` from the
record's `+0xB` (the sub-kind) and clears `+2`; each sub-kind's entry here is
a dispatcher by `+2` through a table of its own.

| Sub-kind (`EffectKind18_States`) | What the code draws and steps | Functions | Table |
|---|---|--:|---|
| 0x23 (35, `0x6540F8`) | a textured panel at the record's map point that waits for story flag 0x28 and then slides 4 a frame along x until `+0x30` reaches 0x100 (released); released at once when the flag is set at its start | 5 | `EffectKind18Sub23_States` `0x65E698` (3) |
| 0x24 (36, `0x6540FC`) | at the counter `0x903848` = 5 (and story row `0x903FE0` bit 4 set): four textured quads round the record's point (a spinning square: centre to each pair of neighbouring corners), three beats of 17 frames with sounds, seven shaded trails drawn away and four drawn out (the trail helper), then a spin that speeds up with random jitter of the cell (moving the counter 6 to 7) and a fade (the level `+0x30` down 2 a frame); the counter raised at the trails' end and at the release | 11 | `EffectKind18Sub24_States` `0x65E6AC` (8) |
| 0x3F (63, `0x654168`) | a scene on the counter's cues 0x26, 0x29, 0x32, 0x35, 0x37, 0x3A, 0x3B: CLUT row 4 dimmed to half and restored, a sky gradient over the frame that warms over 0x5B frames and cools over 0x79, dims again with the leader's shade bytes, a trail, sub-kind 0x24's spin and fade, and a white frame to end (CLUT row 4 at full, the leader's shade 0, released) | 19 | `EffectKind18Sub3F_States` `0x65E710` (16) |
| 0x25 (37, `0x654100`) | a lid of two leaves over a map cell (eight variants by `+0x36`): it opens (`+0x30` to 0x80, 0x10 a frame) once the leader stands at the cell, waits for the leader to leave, closes; sounds 0x200 / 0x201 when no message is up (`Field_Request` 0); a textured cap when the variant has one | 7 | `EffectKind18Sub25_States` `0x65E750` (5) |
| 0x26 (38, `0x654104`) | the same lid with five variants of its own (the texture word in `+0x20`, the leaves from `+0x3E - +0x2E`) | 6 | `EffectKind18Sub26_States` `0x65E7B0` (5) |
| 0x39 (57, `0x654150`) | three textured quads in two halves that part (`+0x30` to 0x80, 8 a frame) once story flag 0x5A is set (sound 0x208 when no message is up), then hold drawn as a third variant with `Cond_ByteFE` 1 | 6 | `EffectKind18Sub39_States` `0x65E810` (4) |

What each sub-kind looks like in the game, and where it plays, is not stated
here (the owner's to say); the descriptions are what the code draws. The
spawners: a scan of `.text` for `mov byte [r + 0xB], imm8` with these six
sub-kinds within 0x60 bytes of a `mov byte [r + 5], 0x18` finds none, and
none of our sources stores them; the sub-kind is presumably set from script
or table data (a hypothesis, not checked further). `0x508670` (sub-state 3
of sub-kinds 0x25 and 0x26) is also entry 3 of E5F's table `0x65E9A8`.

## 1. What each function does

All `cdecl`. "S" is `Sprite_Current` (an `Effect_Objects` record); ours reads
it again wherever the original reads `[0x937F88]` again after a call. `+n` is
a byte of S unless a width is given. Every table is read in place from the
image (nothing copied).

### 1.1 Sub-kind 0x23

| Address | Name | Size | What |
|---|---|--:|---|
| `0x506A10` | `EffectKind18Sub23_Run` | `0x12` | `jmp [EffectKind18Sub23_States + +2 * 4]`, unbounded |
| `0x506A30` | `EffectKind18Sub23_Start` | `0x30` | word `+0x30` = 0; `Flags_Test(0x904030, 0x28)` set: a tail jump to `Effect_Release`; else `+2` up, a tail jump to the draw |
| `0x506A60` | `EffectKind18Sub23_WaitFlag` | `0x20` | the flag set: `+2` up; the draw |
| `0x506A80` | `EffectKind18Sub23_Slide` | `0x21` | word `+0x30` up 4; at 0x100 or more (signed) `Effect_Release` - and the draw all the same |
| `0x506AB0` | `EffectKind18Sub23_Draw` | `0x118` | `Gpu_SetDrawMode(cursor, 0, 0, 0x95, 0)` linked at the record's point (`MapView_LinkPrimAt(+0x34, +0x38, 0, 0xC)`); a `POLY_FT4` at the cursor read after the link: `Prim_VertexScratch` x `+0x30 - 0x37C0` / `- 0x36C0`, y `0x16C0`, z 0x140 / 0x280; `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture(0x2378013D, prim, 1)`, linked at the point (0x48) |

### 1.2 Sub-kind 0x24

| Address | Name | Size | What |
|---|---|--:|---|
| `0x506BD0` | `EffectKind18Sub24_Run` | `0x12` | dispatcher by `+2`, unbounded (no list had it) |
| `0x506BF0` | `EffectKind18Sub24_Start` | `0xBE` | nothing until the counter byte `0x903848` is 5; then `Flags_Set(0x903FE0, 4)`; the record placed: `+0x34` / `+0x38` (dwords) = `((+0xC / +0x10) + 0x4B8000 / 0x568000) sar 9 - 0x4000`, `+0x3C` 0x3A0, word `+0x32` 0, `+0x14` 0x53020, word `+0x30` 0x80, `+0x10` and `+0xC` 0; `+0xB` 2, `+9` 0, sound 0x204, `+2` up; a tail jump to the draw |
| `0x506CB0`, `0x506CF0` | `_Beat1`, `_Beat2` | `0x32` | `+9` up; above 0x10: `+9` 0, the sound byte `+0xB` 1 / 2, `+2` up; the draw |
| `0x506D30` | `_Beat3` | `0x36` | `+9` up; above 0x10: `+9` 0, sound 0x201, `+2` up; the draw |
| `0x506D70` | `_Trails` | `0x155` | `Prim_VertexScratch` `0x9037A0` = (0xE5C0, 0xEB40, 0x160), its angles `0x9037A8` = (0x620, 0, (k * 32 + `Frame_Counter`) * 32); seven `EffectKind18Sub24_DrawTrail(+9 * 4, 0x1F, 0xE00 + 0x400 k, 0x18)`; from `+9` = 3 four more from the corners (the byte pairs `0x65E6A4`, x and z each `(3 b << 16 + 0x4B8000 / 0x568000) sar 9 - 0x4000`, y 0x3A0, angles (0x400, 0, ...)): `DrawTrail(0, 0x1E, 0x400 j, 0x10)`; the draw; `+9` up; above 7: `+2` up, sound 0x200 |
| `0x506ED0` | `_TrailsOut` | `0xEA` | the four corner trails `DrawTrail(+9 * 4 - 0x20, 0x1E, 0x400 j, 0x10)`; the draw; `+9` up; above 0xF: the counter up, `+9` 0, `+2` up |
| `0x506FC0` | `_Spin` | `0x11B` | the angle `+0x32` turned back by `+0x14 / 10000` and masked `& 0xFFD` (bit 1 cleared too); `+0x14 += (q - 10) * q`; on each change of the angle's bit 11 the sound byte `+0xB = (+0x32 sar 11) + 1`, `+0x2E` keeping the angle; while `+0x14` is 0xA6040..0xF9060 `+0xC` / `+0x10` = `Rand & 0x1000`; from 0xF9060 `Rand & 0x2000` and the counter 6 becomes 7; from 0x14E790 `+2` up; the draw |
| `0x5070E0` | `_Fade` | `0xAF` | the angle turned; `+0xC` / `+0x10` = `Rand & 0x2000`; the sound byte on a bit-11 change only while `+0x30` is above 0x40; `+0x30` down 2; at 0 the counter up and `Effect_Release`; the draw |
| `0x507190` | `_Draw` | `0x238` | `Gte_PushMatrix`; a matrix whose translation is `Gte_RotTrans` of the record's point (`+0x34`, `+0x38`, `+0x3C` as s16), its rotation `Gte_RotMatrix(0, 0, +0x32)` times `Camera_Matrix`, loaded; the sound byte `+0xB` set: sound `+0xB + 0x201`, `+0xB` 0, texture `0x1300900F`, else `0x1300980C`; four `POLY_FT4`s from (0, 0, -0x240) twice to the corners k and k + 1 (x, y `3 b << 7`), their level `+0x30` - above 0x40 less `Math_Cos(+0x32 + 0x400 k) sar 7` and 0x20 - in the texture word's bits 16.. with its low three bits cleared, `Gfx_CommitPrim(5, 0x48)`; `Gte_PopMatrix` |
| `0x5073D0` | `_DrawTrail(from, to, angle, scale)` | `0x244` | `Gte_PushMatrix`; a draw mode (tpage 0x35, dtd 1) committed (5, 0xC); a matrix of the scratch's angles turned by `Gte_RotMatrixZ(angle)`, times `Camera_Matrix`, its translation `0x9037A0` turned; for each i of from..to - 1 the segment from pair i to pair i + 1 of the 34 byte pairs at `0x65E6CC` (y the pair's second byte times `scale`, z its first times -48), drawn twice (j = 0, 2) as a semi-transparent `POLY_G4`: `Gte_RotTransPers3` (third point into `MapView_ScreenXY`), `Gte_StoreDepthF3`, the far edge the near one plus the float `0x5C41B8` less j (through the FPU), the depths copied, colour (0xC8, 0xB4, 0x0A) to black, committed (5, 0x44); a draw mode (dtd 0) committed (5, 0xC); `Gte_PopMatrix` |

### 1.3 Sub-kind 0x3F

| Address | Name | Size | What |
|---|---|--:|---|
| `0x507620` | `EffectKind18Sub3F_Run` | `0x12` | dispatcher by `+2`, unbounded (no list had it) |
| `0x507640` | `_Start` | `0x80` | placed as sub-kind 0x24's with 0x238000, 0x1B8000, `+0x3C` -0x4C0, `+0x30` 0x60; `+2` up |
| `0x5076C0` | `_WaitDim` | `0x33` | at the counter 0x26: `_ShadeClut(0x40)`, `MapView_BuildFlags` 0, `_DrawSky(-1)` (white), `+9` 0, `+2` up |
| `0x507700` | `_Undim` | `0x1D` | `_ShadeClut(0x80)`, `MapView_BuildFlags` 1, `+2` up |
| `0x507720` | `_SkyWarm` | `0x8E` | `MapView_BuildFlags` 0; the sky `(0x7C00 - (q << 10)) | (0x320 - (q << 5)) | (+9 / 17) | 0x10000000`, q = 27 `+9` / 100; `+9` up; above 0x5A: `+9` 0, `+2` up |
| `0x5077B0`, `0x507A50`, `0x507B90` | `_WaitCue29`, `_WaitCue37`, `_WaitCue3B` | `0x26` | `MapView_BuildFlags` 0, the sky `0x10001805`; at the counter's cue `+2` up |
| `0x5077E0` | `_SkyCool` | `0x8B` | the sky `(5 - r) | (q + 6) << 10 | (q | 0x800000) << 5`, q = 20 `+9` / 100, r = `+9` / 24; `+9` up; above 0x78: `MapView_BuildFlags` 1, `+2` up (`+9` kept) |
| `0x507870` | `_WaitCue32` | `0x28` | at 0x32: sound 0x208, `+9` 0, `+2` up; nothing drawn |
| `0x5078A0` | `_SkyDim` | `0xE0` | the warming sky; `_ShadeClut(0x80 - +9 / 3)`; `ObjTrio` record 0's `+0x5D..+0x5F` = `(+9 / 3 - +9) / 2`; `+9` up; above 0x5A: `+9` 0, `+2` up |
| `0x507980` | `_WaitCue35` | `0x31` | the sky held; at 0x35 `+2` up and sound 0x201 |
| `0x5079C0` | `_Trail` | `0x85` | the scratch `0x9037A0` = (0xD1C0, 0xCDC0, 0xFC20), angles (0, 0, `Frame_Counter` * 32); `DrawTrail(+9 * 4, 0x1F, 0, 0x18)`; the sky held; `+9` up; above 7 `+2` up |
| `0x507A80` | `_Flash` | `0x37` | the sky white; sounds 0x202 and 0x200; `+9` 0; `+2` up |
| `0x507AC0` | `_Spin` | `0x55` | the sky held; the angle turned; sub-kind 0x24's draw; at 0x3A `+2` up |
| `0x507B20` | `_SpinFade` | `0x69` | the sky held; the angle turned; `+0x30` down 6; the draw; `+9` up; above 0xF `+2` up |
| `0x507BC0` | `_WhiteOut` | `0x7E` | an opaque white `POLY_F4` over the frame (section 2) committed (4, 0x38); sound 0x202; `MapView_BuildFlags` 1; `_ShadeClut(0x80)`; the leader's shade 0; `Effect_Release` |
| `0x507C40` | `_ShadeClut(level)` | `0x6A` | the 0x100 colours of `Gfx_ClutStripSource + 0x800` (CLUT row 4 as loaded), each 5-bit channel times `level` sar 7, packed back **unclamped** (a level above 0x80 runs a channel into the next) with bit 15 kept, to `Gfx_ClutStrip + 0x800`; `Gfx_ClutStripDirty` 1 |
| `0x507CB0` | `_DrawSky(colours)` | `0xDF` | a draw mode (tpage 0x95, dtd 1) committed (6, 0xC); a `POLY_G4` over the frame (section 2), its top corners the 15-bit colour in bits 0..14 of the argument, the bottom ones bits 16..30 (each channel `<< 3`), committed (6, 0x44); a draw mode (dtd 0) committed (6, 0xC) |

### 1.4 Sub-kinds 0x25 and 0x26

| Address | Name | Size | What |
|---|---|--:|---|
| `0x507D90`, `0x5083B0` | `EffectKind18Sub25_Run`, `Sub26_Run` | `0x12` | dispatchers by `+2`, unbounded |
| `0x507DB0`, `0x5083D0` | `_Start` | `0x156`, `0x166` | `+8` = (word `+0x3A` == 0); the variant v = s16 `+0x36` picks the cell `+0x36` / `+0x3A` (byte pairs `0x65E784` / `0x65E7F0`), the height `+0x3E` (`0x65E764` / `0x65E7C4`), `+0x2E` (`0x65E774` / `0x65E7D0`), for 0x26 the texture word `+0x20` (`0x65E7DC`), the lid `+0x32` (bytes `0x65E794` / `0x65E7FC`); `+0x30` 0; `+2` up; the leader at the cell: `+0x30` 0x80, `+2` = 3; the draw (dy 0). **Eight variants for 0x25, five for 0x26**; past them ours aborts (section 6) |
| `0x507F10`, `0x508540` | `_WaitNear` | `0xE4` | the leader at the cell: sound 0x200 unless `Field_Request`, `+2` up; the draw (dy 0) |
| `0x508000`, `0x508630` | `_Open` | `0x3F` | word `+0x30` up 0x10; at 0x80 `+2` up; the draw, dy 0 with a lid (`+0x32`), else -1 |
| `0x508670` | `EffectKind18Sub25_WaitAway` | `0xBD` | `+2` up once the leader is away: more than 0x20000 across the cell's middle row, or along it more than 0x20000 from both the cell and the next; nothing drawn. Sub-state 3 of both and of E5F's `0x65E9A8` |
| `0x508040`, `0x508730` | `_Close` | `0x57` | `+0x30` down 0x10; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1; the draw |
| `0x5080A0` | `EffectKind18Sub25_Draw(dy)` | `0x305` | the cell's fixed edge `((+0x3A` or `+0x36) << 7) - 0x3FC0` (by `+8`) and the height `+0x3E - 0x180 .. +0x3E` in the scratch; two leaves (`POLY_FT4`), each moved by `+0x30` times the sign byte i of `0x65E79C` (1, -1), projected, texture `(+0x2E + i) | +8 << 21 | 0x14500000`, linked at the point with dy (0x48); with a lid, a cap over the cell at `+0x3E - 0x140`, its texture the dword `+0x32` of `0x65E79C`, linked with dy 0 |
| `0x508790` | `EffectKind18Sub26_Draw(dy)` | `0x2E8` | the same, the leaves from `+0x3E - +0x2E`, signs `0x65E804`, texture `(+0x20 + i) | +8 << 21`; the cap at the leaves' height, texture the dword `+0x32` of `0x65E804` |

"The leader at the cell": `ObjTrio` record 0's x, z (`0x802D74`, `0x802D78`).
With `+8` set the lid lies along x: the leader's z within 0x8000 of
`(+0x3A + 1) << 16 | 0x8000`, and its x within 0x10000 of `+0x36 << 16` or of
the next cell; with `+8` clear x and z swap. The distances are `cdq` /
`xor` / `sub` absolutes compared signed (`0x80000000` stays negative: ours
keeps that).

### 1.5 Sub-kind 0x39

| Address | Name | Size | What |
|---|---|--:|---|
| `0x508A80` | `EffectKind18Sub39_Run` | `0x12` | dispatcher by `+2`, unbounded |
| `0x508AA0` | `_Start` | `0x5F` | story flag 0x5A set: `+0x30` 0x80, `+2` = 3, `_Draw(0, -2)`; clear: `+0x30` 0, `+2` up, `_Draw(0, 1)`, `_Draw(1, 1)` |
| `0x508B00` | `_WaitFlag` | `0x47` | the flag set: sound 0x208 unless `Field_Request`, `+2` up; `_Draw(0, 1)`, `(1, 1)` |
| `0x508B50` | `_Open` | `0x30` | `+0x30` up 8; at 0x80 `+2` up; `_Draw(0, -1)`, `(1, 1)` |
| `0x508B80` | `_Hold` | `0x14` | `Cond_ByteFE` 1; `_Draw(2, 0)` |
| `0x508BA0` | `_Draw(variant, dy)` | `0x11C` | three `POLY_FT4`s from the vertices `0x65E824` (four (x, y, z) s16 each), x less `+0x30` times the sign byte `0x65E820 + (variant & 1)` plus `(variant & 1) << 7`, projected, texture the dword `quad + 3 * variant` of `0x65E86C` (nine; E5F's table `0x65E890` follows), linked at the point with dy |

## 2. Divergence: DIV-0041's two fills

`0x507BDC` (the `mov ecx, 0x43A00000` operand in `_WhiteOut`) and
`0x507CE3` (the same in `_DrawSky`) are the 320.0 of two full-frame
primitives `docs/widescreen.md` section 5 lists for E5E. Ours draws both
from `Widescreen_FillX()` to `Widescreen_FillX() + Widescreen_FillWidth()`
(x of the left corners and of the right ones; y 0 and 240 unchanged), as
`effect_4d.cpp` draws its tints. Until `Widescreen_ArmFills` has run - so
during every self-test - and whenever the picture is narrow that is
`0.0f` .. `320.0f`, the original's bytes exactly; under the wide picture
(-53) .. 373. The gradient keeps its two colour rows (the corners move, the
colours do not), which is `Gfx_DrawSkyGradient`'s rule. `Effect5E_Inject`
sits before `FishingText_Arm` / `Widescreen_ArmFills` in `inject_all.cpp`.
**No new ledger entry**: the coordinator amends DIV-0041's list. Nothing else
diverges.

## 3. The tables

The six sub-state tables, each the dispatcher's reach read by hand (the
dispatchers do not bound `+2`): the run of code pointers up to the data
that follows or the next table a dispatcher indexes, checked against what
the sub-states store into `+2`.

| Table | Count | Entries | What follows |
|---|--:|---|---|
| `EffectKind18Sub23_States` `0x65E698` | 3 | `_Start`, `_WaitFlag`, `_Slide` | sub-kind 0x24's corner bytes `0x65E6A4` |
| `EffectKind18Sub24_States` `0x65E6AC` | 8 | `_Start`, `_Beat1..3`, `_Trails`, `_TrailsOut`, `_Spin`, `_Fade` | the trail's byte pairs `0x65E6CC` |
| `EffectKind18Sub3F_States` `0x65E710` | 16 | `_Start` .. `_WhiteOut` | `EffectKind18Sub25_States` (the tool's run counted on to 21) |
| `EffectKind18Sub25_States` `0x65E750` | 5 | `_Start`, `_WaitNear`, `_Open`, `_WaitAway`, `_Close` | the variants' words `0x65E764` |
| `EffectKind18Sub26_States` `0x65E7B0` | 5 | `_Start`, `_WaitNear`, `_Open`, `EffectKind18Sub25_WaitAway`, `_Close` | the variants' words `0x65E7C4` |
| `EffectKind18Sub39_States` `0x65E810` | 4 | `_Start`, `_WaitFlag`, `_Open`, `_Hold` | the sign bytes `0x65E820` |

The data the code indexes (`effect_5e_callees.h` names each; lengths read
from where the next table starts): sub-kind 0x24's four corners `0x65E6A4`;
the trail's 34 byte pairs `0x65E6CC..0x65E70F`; sub-kind 0x25's eight
variants (`0x65E764` s16, `0x65E774` u16, `0x65E784` byte pairs, `0x65E794`
bytes) and five texture dwords `0x65E79C` (entry 0's two low bytes are the
leaves' signs 1 / -1; a lid indexes 1..4); sub-kind 0x26's five variants
(`0x65E7C4`, `0x65E7D0`, `0x65E7DC` dwords, `0x65E7F0`, `0x65E7FC`) and three
dwords `0x65E804` (the same double use, a lid 1..2); sub-kind 0x39's signs
`0x65E820`, 72 bytes of vertices `0x65E824` and nine texture dwords
`0x65E86C`. The float `0x5C41B8` (the trail's offset) is read in place.

## 4. The fuzz (`effect_5e_fuzz.cpp`)

Effect mode (`g.effect`, kinds {0x18}), 4,000 rounds a function. The clone
table is `band_rows.py --group E5E --clones --harness scenario`, every
extent the tool's but `0x506A80` (0x21: the tool's 0x148 runs on into the
draw it tail-jumps to) and the three added starts (`0x506AB0`'s calls are
the tool's for `0x506A80` past `+0x30`). Dispatchers, sub-states and the two
argument-less draws are `kEffect`, each dispatcher with `sub_span` its
table's count; the six helpers with arguments `kCall`.

- **Callees**: the group's own as recorders - the two argument-less draws
  `kPhase`; `_DrawTrail` four whole words, `_ShadeClut` and `_DrawSky` one,
  the lid draws one and sub-kind 0x39's two (whole words: every caller pushes
  constants or a register it cleared first), each lid / 0x39 draw logging
  `Sprite_Current` (`FxCurrent`). Everything else is the harness's standard
  and effect sets unchanged (none re-listed: `Gte_RotTransPers3` /
  `Gte_RotTransPers4` / `Gte_StoreDepthF3` fill their outs, which the trail
  reads and copies; `Rand` the harness's).
- **Tables**: the six above, swapped for recorders.
- **Regions** beyond the standard: CLUT row 4 as loaded
  (`Gfx_ClutStripSource + 0x800`, 0x200 bytes) and live (`Gfx_ClutStrip +
  0x800`). `ObjTrio` (the leader's point and shade), the counter, the flag
  rows, `MapView_BuildFlags`, `Cond_ByteFE`, `Gfx_ClutStripDirty` and the
  scratch are standard.
- **Seed** (every record the disturbance may move `Sprite_Current` to):
  `+9` at its compares' boundaries (2, 3, 7, 8, 0xF..0x11, 0x5A, 0x5B,
  0x78, 0x79), `+8` 0 / 1 / other, `+0xB` 0..2, the level `+0x30` round
  0x40, 0x80, 0x100 and its steps, the angle `+0x32` and `+0x2E` with bit 11
  the same or not, the speed `+0x14` round 0xA6040, 0xF9060, 0x14E790, the
  cell `+0x36` / `+0x3A`; the starts' variant inside its tables, the lid
  draws' `+0x32` 0..2 (ours aborts past a table); the leader's x and z at
  the near / away tests' boundaries round the current record's cell; the
  counter at every cue and either side; `Field_Request` 0 / 1 / 2; CLUT row
  4's source filled for `_ShadeClut`.
- **Arguments**: the trail inside the polyline (from 0..32, to 0..33, from
  at or above to drawing nothing, a negative from with a lower to), the
  angle and scale at the callers' values and random; the CLUT level at
  0x40, 0x80, 0, 0x100, negative; the sky's colours white, `0x10001805`,
  random; dy as the callers push it; the variant 0..2.
- **Disturbance** (the group's case, from the hash only): `+9` at its
  boundaries, `+0x30`, `+0x14` at 0xF9060, the counter at the cues, `+0xB`,
  `+0x32` (0..2 while a lid draw runs), `+0x2E`'s bit 11, the leader's
  point, `+8`.
- `BOF3X_E5E_ONLY=<name>,<name>,...` runs the clones it names exactly.

**Results in this worktree** (`BOF3X_SELFTEST_ONLY=1`): `effect_5e` 216,000
rounds over 54 functions, 1,006,945 calls to the stand-ins, 0 mismatches.
STAR_RESULTS

## 5. What the cut and the tool said, settled

- **`0x506A80`** (the cut's 336, the tool's 0x148): 0x21 bytes - it ends in
  a `jmp 0x506AB0`. `0x506AB0` has its own frame (`sub esp, 8; push esi`)
  and `ret`; three sub-states tail-jump to it (harness section 8.5 named it
  as one of the eight shared tails). Taken as `EffectKind18Sub23_Draw`.
- **`0x506BD0`, `0x507620`**: `EffectKind18_States` entries 36 and 63, each a
  dispatcher of the shape `mov ecx, [S]; mov al, [ecx + 2]; jmp [eax * 4 +
  T]`; the cut lists only their tables' states (its `unit_desc` names them
  `Fn_506BD0` / `Fn_507620`). Taken.
- The **hidden starts** inside E5D's host `0x506640` (`0x506A10..0x5070E0`)
  and inside our `0x5073D0`, `0x507CB0`, `0x5080A0`, `0x508790` hosts: each
  reached only by its `.data` cell; every host's real extent ends at its own
  `ret` before them (section 11).
- Every other extent differs from the cut's only by padding.
- **`0x508000`'s "push" reach** (`band_rows.py`: `push 0x5638ae in
  0x5633F0`, and `tools/scenario_rows.py`'s `SE_ADDRS` listing `0x508000` and
  `0x5080A0` as "engine-side helpers the chapters share"): `0x508000` is the
  immediate z of `Scena13_Run6`'s `ChangeArea(0x8F, 0x78000, 0x508000,
  0x88)` - a coordinate that happens to equal the address, not a reference;
  nothing calls either function from the chapters.

## 6. Where ours aborts (the original reads on)

No ledger entry (round 9 section 6): where Capcom's code would read past a
table, ours aborts with a message naming `docs/effect_5e.md section 6`.

| Function | When | What the original does |
|---|---|---|
| the six dispatchers | `+2` at or past the table's count | jumps through the dword after (data or the next table) |
| `EffectKind18Sub25_Start` / `Sub26_Start` | s16 `+0x36` below 0 or at / past 8 / 5 | reads the next tables' bytes as the variant |
| `EffectKind18Sub25_Draw` / `Sub26_Draw` | s16 `+0x32` (a lid) below 0 or past 4 / 2 | reads the sub-state table's code pointers (or what precedes) as a texture word |
| `EffectKind18Sub39_Draw` | `quad + 3 * variant` past 8 | reads E5F's table `0x65E890` as a texture word |
| `EffectKind18Sub24_DrawTrail` | a segment i below 0 or above 32 | reads the bytes before or after the 34 pairs as vertices |

None is reached in ordinary play as far as the code shows: the starts take
their variant from the spawner, the lids from those tables (0..4 / 0..2),
the 0x39 draws are called with 0..2, and the trails' `from` is `+9 * 4`
(`+9` 0..7 in state 4) or `+9 * 4 - 0x20` - which `_TrailsOut` only runs
with `+9` from 8 (state 4 leaves it at 8; it is not reset), so i starts at 0.

## 7. Latent defects (Capcom's, described, not fixed)

- **`_ShadeClut` does not clamp**: a level above 0x80 carries a channel past
  31 into the next field (red into green, green into blue, blue into bit 15).
  Its callers pass 0x40, 0x80 and `0x80 - +9 / 3` (+9 0..0x5A), never above
  0x80 - so not reached in play.
- **The angle mask `& 0xFFD`** in the spin (`0x506FC0`, `0x5070E0`,
  `0x507AC0`, `0x507B20`) clears bit 1 of the angle as well as bits 12..15:
  the spin's angle only takes every other step of 4096 when bit 1 would be
  set. Harmless; reproduced.
- **`_DrawTrail` hands `Gte_RotTransPers3` the global `MapView_ScreenXY` as
  its third out** and `Gte_StoreDepthF3` a local it never reads: the third
  projected point and depth are written and dropped (the quad is built from
  the first two). Reproduced (ours writes the same global).
- **`_TrailsOut` with `+9` below 8** (not reached: see section 6) would draw
  segments from before the polyline.
- `EffectKind18Sub25_WaitAway` (sub-state 3 of both lids) draws nothing:
  while it waits for the leader to leave, the open lid is not drawn at all.
  Not necessarily a defect - what the open lid should look like is the
  owner's to say; recorded because every other sub-state draws.

## 8. Calls across groups

**Out**: none - every callee is ours by name (`Flags_Test`, `Flags_Set`,
`Effect_Release`, `Sound_PlayEffect`, `Rand`, the `Gte_*` / `Gpu_*` /
`Prim_SetTexture` / `MapView_LinkPrimAt` / `Gfx_CommitPrim` primitives) or
the group's own; `band_rows.py --edges` lists no edge.

**In** from outside the group: `EffectKind18_Run` (ours, `worldmap_area.cpp`)
through `EffectKind18_States` 35, 36, 37, 38, 57, 63 (the cells, no code
reference to rebind); **E5F's sub-state table `0x65E9A8` entry 3 is
`0x508670`** (`EffectKind18Sub25_WaitAway`) - a cell, no code reference.

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract,
shop, worldmap, combat) are empty for all 54 rows, and no call trace under
`analysis/calltrace/` (the 314 files there, the owner's
`reach_balioAndSunder_1/_2`, `reach_bossAndFlash`, `reach_dragonGene` among
them) records an entry into `0x506A10..0x508CBB`. The group is **fuzz only**;
a route that shows any of these sub-kinds would let the coordinator's
frame-hash A/B cover it.

## 10. The rebinding

`grep -rn -i` of the 54 addresses and six tables in `src/game`
(`band_rows.py --refs`): one reference, `scena_sc13.cpp:966`'s `0x508000` -
a coordinate (section 5), **left raw**: it is not a reference. Nothing else
in our sources names these addresses; `tools/scenario_rows.py`'s `SE_ADDRS`
(`0x508000`, `0x5080A0`) is a tool's list, not ours to edit, and is noted
here for its owner. Nothing rebound.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): a comment and 51 lines,
the read extents of the 54 less three already listed right (`0x507190`,
`0x507C40`, `0x5080A0`); `0x5073D0`, `0x507CB0`, `0x508790`, `0x508BA0`
re-listed smaller than their host lines (`86E`, `3E7`, `404`, `48B`, each
spanning the hidden starts after its `ret`), and the hidden starts inside
E5D's `00506640 B4F` listed, which cut it.

## 12. Controls

CONTROLS_TABLE
