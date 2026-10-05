# Group E6C: kind 0x18's sub-kinds 0x44 (frame and sky), 0x45, 0x51, 0x53, 0x55, 0x59, 0x5A, 0x5B, 0x66, and the map's corner height

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..16),
wave six, from the round branch's tip `c290566`. **50 functions ours**
(`src/game/effect_6c.cpp`, shadow name `effect_6c`): the cut table's 48 rows
for E6C (`analysis/round13_cut.tsv`, the band `0x510C90..0x5140C0`) less one
start that is **not a function** (`0x5124C0`, case 3 of
`EffectKind18Sub51_Wait`'s own switch - the cut's suspect, confirmed; section
5), plus three starts the band holds that no group listed: sub-kind 0x51's
third state `0x512510` ("code no list has") and the sub-kind dispatchers
`0x512B20` (0x59) and `0x513CD0` (0x5B), catalogued as "Table
EffectKind18_States" rows of no group (wave one's addendum: a dispatcher in
the band that no list holds is the band's). Each read to its last instruction
with capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
200,000 rounds, 0 mismatches; controls in section 6. **Fuzz only**: no
recorded route enters any of the 50 (section 9).

**One row is not effect code and is taken anyway** (its tier is `part5`,
catalogued, so it is the group's - the 2026-09-29 addendum):
`AreaMap_CornerHeight` `0x511C10`, the map's height at a 16.16 point from
`AreaMap_Corners`, called by area 189's `Area189_LeaderStart` and
`Area189_StepBegin` (ours, `area_w4e.cpp`'s `kHeightAt`). It does not read
`Sprite_Current`; it is fuzzed as a `kCall` answering `eax`. No `hypothesis`
row turned out not to be effect code (the two `hypothesis` rows, `0x5128B0` and
`0x513860`, are sub-kind dispatchers and are taken).

**Divergence:** DIV-0041's full-frame fill at `0x510E6C` (sub-kind 0x44's
red additive tile, `(0, 0)` 320 x 240) now draws through `Widescreen_FillX` /
`Widescreen_FillWidth`; no new ledger entry (the coordinator amends DIV-0041).
DIV-0062's six draw-item immediates in the band (`0x51242A`, `0x512607`,
`0x5139C2`, `0x513B9B`, `0x513BAC`, `0x513C41`) are read through
`draw_pool::Items()`, and the four functions holding them are added to
`draw_pool.cpp`'s `kOwnedUsers`. Nothing else diverges (section 2). **No
latent defect here reads memory the original never wrote** (nothing for a
new ledger entry); one is for the owner's eye: sub-kind 0x59's shade bytes
live in `.data` and are never restored (section 7).

| Sub-kind (`EffectKind18_States[n]`) | Functions | What the code does |
|---|--:|---|
| 0x44 (`[0x44]` `0x510C20`, E6B's dispatcher; its state 1 `0x510C80` calls the first and tail-jumps to the second) | 7 | the record kept on the leader's animation, projected at `Field_Kind2`'s point and put on `Sprite_DrawList`; Cond_ByteFE 1 sets its colour cells, 2 a red additive full-frame tile. Then a screen-space sky: bands and two rows of textured quads tinted by area 189's frame word `0x90405C` (CLUT rows by `Gfx_ClutAdjust`), up to six stars with twinkles, two layers of Gouraud bands, two glows placed by the camera's distance from two map points |
| 0x45 (`0x654180`) | 5 | waits for Cond_ByteFE 1; a ring of 32 textured quads round a fixed point opened outward 8 a frame; then eight rings spreading one after another; at the end `MoveCmd_TestFB(0xB, 0x22)`, Cond_ByteFE 2 and the record released |
| 0x51 (`0x6541B0`) | 4 | one of five rectangles of map cells (by `+0x36`): their records' bits 19..23 cleared (textures re-set); waits for a flag / Cond_ByteFE by the variant; the bits stepped up to 0x10 and the record released |
| 0x53 (`0x6541B8`) | 1 | the ground under the record once; then, while the camera's cell is within 20 of the record's, sixteen rings of sixteen spinning dots |
| 0x55 (`0x6541C0`) | 4 | 66 textured squares over the cells x 3..13, z 0x71..0x76 (a checkerboard); flag 0xE of `0x904000` fades them out and releases the record |
| 0x59 (`0x6541D0`) | 9 | a sixteen-column ring of eight two-quad bands with glows, waiting on Cond_ByteFE 1, 2, 3, its shades faded in a cascade through twelve `.data` bytes; a texture-page move (DR_MOVE) each frame |
| 0x5A (`0x6541D4`) | 9 | waits for Cond_ByteFE 2; map cells of rows 0x47.. rewritten in a sweep; story flag 0x80 set; two cycles of texture-page moves through four rows |
| 0x5B (`0x6541D8`) | 10 | three groups of the 49 textured pieces at `0x65EF98` (bytes: a cell, four corners, a texture word) slid apart, held, slid wide, lifted; story flag 0x81 set at the end, `MoveCmd_TestFB(0x30, 0x71)`; re-opened (flag cleared) on Cond_ByteFE 2 |
| 0x66 (`0x654204`) | 1 | while Cond_ByteFE is not 0, two layers of a 2 x 2 grid of 256-pixel textured quads scrolled and faded by `+9`; released when `+9` wraps |
| - | 1 | `AreaMap_CornerHeight` (above) |

What each looks like in the game is not stated here (the owner's to say);
the descriptions are what the code draws and tests. That sub-kind 0x44's sky
runs on area 189's frame word is what the code reads (the word
`Area189_StepArrive` advances and wraps at 0x3C0), not an observation. **No
spawner is found**: kind 0x18's sub-kind is `+0xB` at spawn
(`EffectKind18_Start`), set from the areas' effect lists, data not code.

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`, the seven dispatchers `evidence`). The cut's PSX twins (column
`note`, `psx 801F...`) are AREA-overlay copies (every overlay loads at
`0x801F...`): `../BreathOfFire3Recomp/names/` has no function name for any of
them (a grep of the 26 addresses finds only unrelated `area_records.toml`
entries of other overlays), so none is transferred.

## 1. What each function does

### 1.1 Sub-kind 0x44 (its dispatcher `0x510C20` is E6B's)

| PC | Name | What |
|---|---|---|
| `0x510C90` | `EffectKind18Sub44_Follow` | `Sprite_EnsureAnimation(the leader's +0x4B)` when the record's `+0x4B` differs, then `+0x4B` = it (read again); `Sprite_ScriptTick` when the leader's `+2` is 3; Cond_ByteFE 1: `+0x5D..+0x5F` = 0x7F, 0x80, 0x80 and Cond_ByteFE 0, else 0; `+0x32` 0x3200; the vertex `(((Field_Kind2X - 1) & 0xFFFFFF) + 1 >> 9) - 0x4000`, the same of Z, `-(the leader's +0x3E / 2)` projected (`Gte_RotTransPers`) into `MapView_ScreenXY`; `+0x74` 160.0f, `+0x2E` 0xA0; the leader's `+0x14` set: `+0x78` = the screen y (an `fld` / `fstp` copy), `+0x30` and the leader's `+0x30` = `_ftol` of it; else `+0x78` = the leader's `+0x30` (`fild`) and `+0x30` = it back; the record onto `Sprite_DrawList` while the count is below 40 (the writer `0x510E26` its symbol names); Cond_ByteFE 2: a draw mode (page 0xB5) at slot 3, a TILE `(0, 0)` 320 x 240 red `(0xFF, 0, 0)` semi-transparent, its depth (`Gte_StoreDepthF`), slot 3 (0x1C), Cond_ByteFE 0 - **DIV-0041's fill** (section 2) |
| `0x510EB0` | `EffectKind18Sub44_Draw` | hidden in `0x510C90`; section 1.2 |
| `0x5115A0` | `EffectKind18Sub44_DrawGlow(int level, int x)` | three FT4s over `(x .. x + 0x4E, 72 .. 102)`: one shaded `level` (page 0xD7, CLUT 0x7A00, uv `(0..0x4F, 0xC0..0xDF)`), two (page 0xB7, CLUTs `(0x1E6 + row) << 6`, u stepped 0x50) shaded by the vertex words of rows 0 and 1 times `level` / 128 (toward zero), each `EffectKind18Sub44_Commit(0x48)` |
| `0x511740` | `EffectKind18Sub44_DrawStars(int level)` | a draw mode (page 0xB5, dtd), commit 0xC; the shade `level * 255 / 128`; star 1 at x `((Cond_AngleFB + 0x200) & 0xFFF) * 10 >> 5`, y 45, its green / blue `(Rand & 0x7F) * level / 128`, twinkled; star 2 at `+ 0x400` and y `Field_Kind2Z's cell / 85 - 1`, star 3 at y `|0x1400 - z|^2 / 98304 + (x's high byte past 0xF00) (x - 0xF00) / 25 + 8` and x `(s16 angle - 0x200) * 10 >> 5 + (z - 0x1480) / 17` shaded `((Rand - 0x80) & 0xFF) * level / 128` - each drawn and twinkled only while y is in `[0, 90)`; three stars of size `(0xD00 - min(|z - 0x1380|, 0x400) - min(|x - 0x1480|, 0x900)) / 43 + 0x14` (at least 0x28; + 8 each) placed at `Math_Ratan2` of the clamped point `(x in [0xB00, 0x1E00], z in [0xF00, 0x1800])` - drawn while the size is at most 89. Each star's y is stored into the cursor's prim before its test, drawn or not |
| `0x511BB0` | `EffectKind18Sub44_Commit(unsigned size)` | `Gfx_CommitPrim`'s room test (`(Gfx_BufferIndex << 16) + 0x7F1BAC` above cursor + `size & 0xFF`) against another list: `Gpu_LinkPrim(the last pointer at 0x802B34 + 8 * Gfx_BufferIndex, cursor)`, that pointer = the cursor (read again), the cursor up; else nothing |
| `0x511D50` | `EffectKind18Sub44_DrawTwinkle(unsigned char *tile)` | four TILE_1s at the star's float point plus the s8 offsets `0x65EEC4` (four pairs), its colour `>> 2`, each committed (0x14); the star's prim read after each new tile is set up. Clears `ax` on return (no caller reads it; ours is `void`) |

### 1.2 The sky (`0x510EB0`)

A draw mode with a texture window `(0x20, 0, 0x20, 0x20)` (a rect allocated at
the cursor, the cursor up 8) at slot 7, a textured FT4 band `(0, 90)..(320,
122)` (uv `(0..0xFF, 0..0x40)`, page 0x95, CLUT 0x7900) at slot 7; a draw mode
with the window `(0, 0, 0x100, 0x100)` at slot 7; a draw mode (page 0xD5) and
a white TILE `(0, 50)` 320 x 40 through `EffectKind18Sub44_Commit`. Then by
area 189's frame word `w` (`0x90405C`'s low word): the vertex words
`0x9037A0..0x9037AC` become two colour rows - below 0x1B0 `(0, 0, 0)` /
`(0x80, ..)`; below 0x1E0 the ramp `n = (w - 0x1B0) / 3`: `(8n, ..)` / `((0x10 -
n) 8, ..)` and `Gfx_ClutAdjust(0, 6, 4n / 10 - 6, n / 8 - 2, 0)`; below 0x390
`(0x80, ..)` / `(0, ..)`; else `n = (w - 0x390) / 3`: row 0 `(0x80 - 12 max(n -
6, 0), (0x10 - n) 8, (0x10 - n) 8)`, row 1 `(8n, ..)` and
`Gfx_ClutAdjust(0, 6, the s8 triple n of 0x65EE94)`. Two rows of three FT4s
(columns 0x100 apart from `((Cond_AngleFB * 10) >> 5) & 0xFF - 0x100`, y 0..90,
page 0xB7, CLUT `(0x1E6 + row) << 6`, v `0x60 row .. + 0x5A`) coloured by the
rows; `EffectKind18Sub44_DrawStars(0x9037A8 as s16)` when it is not 0; two
layers (pages 0xB5, 0xD5, dtd) of four G4 bands between the words of `0x65EE80`
from y 90 down to the bytes of `0x65EE8C`, the top coloured by the rows, the
bottom black - **laid out from the cursor as it was after their draw mode, 0x44
apart, without reading it again**; the glow `DrawGlow(min(0xC0 - (|x - 0x1780| +
|z - 0x1380|) / 3, 0x80), ((angle - 0x800) & 0xFFF) - 0x400 ...)` while that is
above 0, and - flag 4 of `0x904000` set - `DrawGlow(min(0x100 - (|dx| + |z -
0xE00|) / 4, 0x80), ((Math_Ratan2(dx, -z) + angle - 0x400) & 0xFFF) - 0x200
...)`, `dx = x - 0x1780`. Every commit but the first three through
`EffectKind18Sub44_Commit`.

### 1.3 Sub-kind 0x45 (`EffectKind18Sub45_Run` `0x511DD0`)

| PC | Name | What |
|---|---|---|
| `0x511DD0` | `EffectKind18Sub45_Run` | `jmp [EffectKind18Sub45_States + +2 * 4]`, unbounded (hidden in `0x511D50`) |
| `0x511DF0` | `_Wait` (0) | Cond_ByteFE 1: `+0xB` 0, the radius `+0x3C` 0, `+2` up |
| `0x511E20` | `_Open` (1) | `+0x3C` up 8; `DrawRing(min(v, 0x100), max(v - 0x20, 0), 1)`; the inner past 0xFF: `+0x3C` 0, `+2` up |
| `0x511E80` | `_Spread` (2) | `+0x3C` up 8; eight rings between `v - 0x20k`, each kept to 0..0x100 (`v` itself to 0x100 only), halves 1, 0, 1, ...; the first past 0xFF with `+0xB` 0: `MoveCmd_TestFB(0xB, 0x22)`, `+0xB` 1; the last past 0xFF: Cond_ByteFE 2, `Effect_Release` |
| `0x512040` | `EffectKind18Sub45_DrawRing(int outer, int inner, int half)` | `MapView_ScreenXY` and the float after it = (-14912, -12096, -960); 32 FT4s, each 0x1000 of angle: the vertices `(_ftol(cx), _ftol(cy - sin(a / 32) r >> 13), _ftol(cos(a / 32) r >> 13 + cz))` at `outer` (0, 1) and `inner` (2, 3); `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, semi-transparency 0, grey 0x50, page `((half << 7) + 0x140) >> 6 & 0xF | 0x90`, CLUT 0x7940, the uv the same angles' products `>> 16` plus the half's pair at `0x65EED8`; `MapView_LinkPrimAt(+0x34, +0x38, 0, 0x48)` |

### 1.4 Sub-kind 0x51 (`EffectKind18Sub51_Run` `0x512350`)

The rectangle is `(x0, z0, x1, z1)` of the five at `0x65EEDC` by the s16
variant `+0x36`, re-read from the current record at every bound test. A
cell's first record is the dword `AreaMap_Header + 4 (base + cell word + (height
* width + 1) / 2)`, the cell word at `AreaMap_Header + 2 (x + width z + 2
base)`, `base` the header's word +2 (the indexing `MapView_PlaceRuns` and
`DrawLayer_Open` use).

| PC | Name | What |
|---|---|---|
| `0x512350` | `EffectKind18Sub51_Run` | `jmp [EffectKind18Sub51_States + +2 * 4]`, unbounded (hidden in `0x512040`) |
| `0x512370` | `_Place` (0) | `+9` 0; each cell: its first record `& 0xFF07FFFF`; where `MapView_ItemAt(x, z)` answers an item, `Prim_SetTexture(record, DrawItems + item * 0x90, 2)` (DIV-0062's `0x51242A`); `+2` up |
| `0x512490` | `_Wait` (1) | `switch (+0x36)` through its own jump table `0x5124F8` (five cases, the code's extent includes the table): variant 0 flag 0x14 of `0x904000`, 1 Cond_ByteFE 1, 2..4 story flags 0x7A..0x7C - set: `+2` up; past 4 nothing. **Case 3 at `0x5124C0` is the cut's start**: it pushes 0x7B and falls into case 2's `Flags_Test` - not a function |
| `0x512510` | `_Fade` (2) | `+9` up; each cell: its first record's bits 19..23 = `min(+9, 0x10)`; an item's half for this buffer (`DrawItems + (Gfx_BufferIndex + 2 item) * 0x48`, DIV-0062's `0x512607`) textured (1); `+9` past 0x11: `Effect_Release` |

### 1.5 Sub-kinds 0x53 and 0x55

| PC | Name | What |
|---|---|---|
| `0x512660` | `EffectKind18Sub53_Run` | on `+2` = 0: `+0x3C` = `AreaMap_Elevation(+0x34, +0x38)` as an s16, `+2` up. While `|z cell - +0x3A| + |x cell - +0x36|` is at most 20 (`Field_Kind2`'s high words): an additive draw mode linked at the record (dy 1); `+9` up 2; sixteen rings `k` of sixteen TILE_1 dots shaded `0xFF - ((+9 - 0x10k) & 0xFF)` at angle `((Frame_Counter & 0xF) << 4) * spin[k & 1] + 0x100i` (`0x65EEFC`: +1, -1) round the record's point (`3 (sin >> 6) + (+0x34 >> 9) - 0x4000`, the same for z), lifted `-(+0x3C / 2) - 2 ((+9 - 0x10k) & 0xFF)`, projected (`Gte_RotTransPers`, `Gte_StoreDepthF`), linked (dy 1, 0x14); a draw mode closing it |
| `0x5128B0` | `EffectKind18Sub55_Run` | `jmp [EffectKind18Sub55_States + +2 * 4]`, unbounded; entry 0 is `WeretigerFx_Next` (`+2` on, magic_s14's) |
| `0x5128D0` | `EffectKind18Sub55_Wait` (1) | flag 0xE of `0x904000`: `+9` 0, `+2` up; `Draw(0x80, 0)` |
| `0x512910` | `EffectKind18Sub55_Fade` (2) | `+9` up; past 0xF `Effect_Release`; `Draw((0x10 - +9) 8, +9 * 8)` (`+9` read after the release) |
| `0x512960` | `EffectKind18Sub55_Draw(int shade, int lift)` | over cells x 3..13, z 0x71..0x76 with `x + z` even: the vertex `((x - 0x80) << 7, (((z - 0x80) << 6) - lift) << 1, (z - 0x77) << 4)` projected into `MapView_ScreenXY`; an FT4 square of half side `|3 - ((Frame_Counter / 3 + x + z) & 7)| + 0x18` about it, shaded `(shade / 2, shade, shade / 2)`, page 0x3B, CLUT 0x78CA, `MapView_LinkPrimAt(x << 16, z << 16, 1, 0x48)` |

### 1.6 Sub-kind 0x59 (`EffectKind18Sub59_Run` `0x512B20`)

| PC | Name | What |
|---|---|---|
| `0x512B20` | `EffectKind18Sub59_Run` | `jmp [EffectKind18Sub59_States + +2 * 4]`, unbounded (hidden in `0x512960`; in no group's list) |
| `0x512B40` | `_Start` (0) | `+9`, `+0xA` 0, `+2` up; `DrawRing(0)`; tail jump to `_Move` |
| `0x512B70` | `_Wait` (1) | Cond_ByteFE 1: `+2` up; `DrawRing(0)` (no move) |
| `0x512B90` | `_Rise` (2) | Cond_ByteFE 2: `+2` up; `+9` up to 0x20; `DrawRing(0x20 - +9)`; `+0xA` up 1, `& 0x3F`; tail jump to `_Move` |
| `0x512BF0` | `_Hold` (3) | Cond_ByteFE 3: `+9` 0, `+2` up; `DrawRing(0)`; `+0xA` up 2; tail jump |
| `0x512C30` | `_Fade` (4) | `+9` up; the cascade on `0x65EF0C..0x65EF17` (`.data`): the first non-zero of the first nine bytes down 2, and while each so stepped is at most 0x60 the next down 2 too (four at most); `DrawRing(min(+9, 0x60))`; `+0xA` up 4; `_Move` (a call); the ninth byte `0x65EF14` at 0: `+9` 0x60, `+2` up |
| `0x512CF0` | `_Close` (5) | `+9` down 4; at 0 `Effect_Release`; `DrawRing(+9)` |
| `0x512D30` | `EffectKind18Sub59_DrawRing(int glow)` | the record's point into `MapView_ScreenXY` as floats; sixteen columns `i` (angle `0x1000 i`, / 16) of eight rings `k`: a GT4 between the radii `0x65EF37 - k` and `0x65EF38 - k` bytes (`(sin >> 8) r` + the screen x, `_ftol`), lifted `-(height << 5)` (`0x65EF43 - k`, `0x65EF44 - k`), shaded by the fade bytes (`0x65EF13 - k`, `0x65EF14 - k`), page 0xD5, CLUT 0x7940, projected (`Gte_RotTransPers4`, `Gte_PrimDepths4_14`), linked (dy `0x65EF48[i]`, 0x54); a second GT4 given the first's screen points (copied dword by dword), page 0xB5, CLUT 0x7900, linked likewise. Then two glows a column: the outer point at `i`'s angle (+ 0x80) projected into `0x903838`, an FT4 square of half side `(|3 - ((i / 2 + Frame_Counter) & 7)| + 0x10) * 4500 / (Camera_Distance + 0x1194)` (+ 8 the second), shaded `(glow, glow, glow / 2)`, page 0x3B, linked (dy `0x65EF48[i] + 1`, 0x48) |
| `0x513410` | `EffectKind18Sub59_Move` | `Gpu_SetDrawMove(cursor, a stack rect (0x1A8, 0x140 - (+0xA >> 1), 0x10, 0x20), 0x198, 0x120)`, `Gfx_CommitPrim(2, 0x18)` |

### 1.7 Sub-kind 0x66

| PC | Name | What |
|---|---|---|
| `0x513470` | `EffectKind18Sub66_Run` | nothing while Cond_ByteFE is 0. Else `n = +9`, `e = 0x80 - |0x80 - n|`; two layers: a draw mode with a window `(0, 0, 0x40, 0x40)` (page 0x97, dtd) at slot 4, four FT4s `(-d - 0x80 + 0x100 i, d - 0x80 + 0x100 j)` 256 x 256 (page 0x7B, CLUT 0x78C0) shaded - layer 0 `d = -(+9 & 0x3F)`, `e / 2`; layer 1 `d = -2 (+9 & 0x1F)`, `e` when `|0x80 - n|` is past 0x40 else `((Math_Cos((n - 0x40) << 5) >> 7) + 0x60) / 2` - each committed (4, 0x48), a draw mode with the window `(0, 0, 0x100, 0x100)`. `+9` up 4; at 0 `Effect_Release` |

### 1.8 Sub-kind 0x5A (`EffectKind18Sub5A_Run` `0x513860`)

| PC | Name | What |
|---|---|---|
| `0x513860` | `EffectKind18Sub5A_Run` | `jmp [EffectKind18Sub5A_States + +2 * 4]`, unbounded (hidden in `0x513410`) |
| `0x513880` | `_Start` (0) | `+9` 0; story flag 0x80: `+2` = 5; else up |
| `0x5138C0` | `_Wait` (1) | Cond_ByteFE 2: `+2` up |
| `0x5138E0` | `_Animate` (2) | `SetTiles(+9 >> 3, +9 >> 2 & 1)`; `+9` up; at 0xD0 `+2` up |
| `0x513920` | `_Set` (3) | `Flags_Set(story 0x80)`; `+9` 0; `Move(0xC0, 0)`; cells (0x11, 0x61), (0x12, 0x61): the first record's low byte 0xE6 / 0xE7, its item textured (DIV-0062's `0x5139C2`); `+2` up |
| `0x5139F0` | `_Cycle` (4) | `+9` up; every sixth frame `+9` wraps to 0 at 0x18 and `Move(0x65EF70[+9 / 6], 0xC0)` |
| `0x513A60` | `_CycleOpen` (5) | the same through `0x65EF74`, y 0 (where `_Start` goes when flag 0x80 is set) |
| `0x513AD0` | `EffectKind18Sub5A_SetTiles(int step, int bank)` | row `step + 0x47`; `pick = step % 3 + 3 bank` into two six-byte tables the original builds on its stack (5, 4, 3, 0xB, 0xA, 9 / 0x30, 0x40, 0x50, 0xC0, 0xD0, 0xE0): the second record of cell (0xD, row) gets the first's byte, and its item's linked item (`DrawItems[item] + 0x7E & 0xFFF`) textures it (DIV-0062's `0x513B9B`, `0x513BAC`); cells 0xE..0x15: the first record `(x - 0xB) | the second's byte`, its item textured (`0x513C41`) |
| `0x513C70` | `EffectKind18Sub5A_Move(int x, int y)` | `Gpu_SetDrawMove(cursor, a stack rect (0x240, x + 0x100, 0x58, 0x30), 0x240, y + 0x100)`, `Gfx_CommitPrim(2, 0x18)` |

### 1.9 Sub-kind 0x5B (`EffectKind18Sub5B_Run` `0x513CD0`)

The states set the x offset `0x903828` and the lift `0x90382C` (floats)
before each of the three `DrawPart` calls.

| PC | Name | What |
|---|---|---|
| `0x513CD0` | `EffectKind18Sub5B_Run` | `jmp [EffectKind18Sub5B_States + +2 * 4]`, unbounded (hidden in `0x513C70`; in no group's list) |
| `0x513CF0` | `_Start` (0) | story flag 0x81: `+2` = 6; else Cond_ByteFE 3, `+2` up |
| `0x513D20` | `_Wait` (1) | Cond_ByteFE 2: offsets 0, parts 0..2, `Sound_PlayEffect(0x205)`, `+2` up |
| `0x513D70` | `_SlideOut` (2) | parts 0 / 1 / 2 at `-(+9 << 3)` / `+9 << 3` / 0 (lift 0); `+9` up; past 8: `+9` 0, `+2` up |
| `0x513E00` | `_Hold` (3) | parts at -64 / 64 / 0; `+9` up; past 0x1E: sound 0x206, `+9` 8, `+2` up |
| `0x513E80` | `_SlideWide` (4) | `_SlideOut`'s draw; past 0x30: `+9` 0, `+2` up |
| `0x513F10` | `_Lift` (5) | parts at -384 / 384 / 0, part 2 lifted `+9 << 3`; `+9` up; past 0x30: `Flags_Set(story 0x81)`, `MoveCmd_TestFB(0x30, 0x71)`, `Effect_Release` |
| `0x513FB0` | `_Reopen` (6) | Cond_ByteFE 2: `Flags_Clear(story 0x81)`, sound 0x206, parts at -384 / 384 / 0 lifted 384, `+9` 0x60, `+2` up |
| `0x514030` | `_Close` (7) | parts at `-(+9 << 2)` / `+9 << 2` / 0 lifted 384; `+9` down to 0 |
| `0x5140C0` | `EffectKind18Sub5B_DrawPart(int part)` | the pieces `[first, end)` of the byte pair `0x65F36C[2 part]` (end re-read each time) of the 49 at `0x65EF98` (0x14 each): the cell's `(c << 7) - 0x4040` into `MapView_ScreenXY` as floats; the four corners `_ftol(x << 6 + offset + screen x)`, `_ftol(z << 6 + screen y)`, `_ftol((6 - y) << 6 + lift)`; `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture(+0x10, 1)`; `MapView_LinkPrimAt(_ftol((x << 16) + offset * 0x5C4250's float), z << 16, 0, 0x48)` |

### 1.10 `AreaMap_CornerHeight` `0x511C10`

The cell `xi = (x + 0x8000) >> 16 & 0xFF`, `zi` likewise; the block byte of the
256-byte map `0x65ED78` at `(xi >> 4 & 0xF) | (zi & 0xF0)`; the corner dword of
`AreaMap_Corners` at `((block & 0xF0) | (zi & 0xF)) * 0x60 + ((block & 0xF) <<
4 | (xi & 0xF))`, its bytes `c0..c3` signed. With `fx = (x - 0x8000) & 0xFFFF`,
`fz` likewise: `fx + fz <= 0x10000` -> `c0 + (c1 - c0) fx / 0x10000 + (c2 - c0)
fz / 0x10000`; else `c3 - (c3 - c2)(0x10000 - fx) / 0x10000 - (c3 - c1)(0x10000
- fz) / 0x10000`, each divide toward zero; answers that `<< 5` in `eax`.

## 2. Divergence

**DIV-0041's fill** (`0x510E6C`, the `mov [esi + 0x14], 320.0f` of sub-kind
0x44's red tile): `EffectKind18Sub44_Follow` draws its TILE at
`Widescreen_FillX()`, `Widescreen_FillWidth()` wide (`effect_4d.cpp`'s shape),
which is the original's `(0, 0)` 320 x 240 until `Widescreen_ArmFills` has run
and whenever the picture is narrow. The fuzz runs before the arm
(`Effect6C_Inject` sits before `FishingText_Arm` / `Widescreen_ArmFills` in
`inject_all.cpp`), so it compares the original's 320. **No new ledger entry**:
the coordinator amends DIV-0041's list. **Not widened, for the coordinator**:
the sky's 320-wide bands at `0x510F12` (an FT4 `(0, 90)..(320, 122)`) and
`0x510FE5` (a TILE `(0, 50)` 320 x 40) are full-width, not full-frame; and
the sky's other quads sit at x from the camera's angle, 0x100 apart.

**DIV-0062** (the draw-item pool doubled): the six immediates in the band
`draw_pool.cpp`'s `kSites` re-aims at inject are inside
`EffectKind18Sub51_Place` (`0x51242A`), `_Fade` (`0x512607`),
`EffectKind18Sub5A_Set` (`0x5139C2`) and `_SetTiles` (`0x513B9B`, `0x513BAC`,
`0x513C41`). Ours reads the array through `draw_pool::Items()` and checks an
index against `draw_pool::Count()`; the four are added to `kOwnedUsers`
(`BOF3X_ORIGINAL` on any of them keeps the original's 1,024 items, as for the
earlier users). No new entry: DIV-0062 already lists the sites.

Otherwise no divergence: each function is a faithful replacement.
`widescreen.cpp`, `cheats.cpp` and `DIVERGENCE.md` patch no other byte inside
the band (a grep of every address; `AreaMapBD_BuildView`'s patched operands at
`0x51097E..0x5109D2` lie below it).

## 3. The tables

**The sub-state tables** (`symbols.toml` `[[data]]`): each the run of code
pointers its dispatcher indexes, to the data that follows it, checked by hand
against what the states store into `+2` (the tool reads the same counts):

| Table | Count | Entries | Followed by |
|---|--:|---|---|
| `EffectKind18Sub45_States` `0x65EECC` | 3 | `_Wait`, `_Open`, `_Spread` | `0x65EED8`, the ring's uv pairs |
| `EffectKind18Sub51_States` `0x65EEF0` | 3 | `_Place`, `_Wait`, `_Fade` | `0x65EEFC`, sub-kind 0x53's spins |
| `EffectKind18Sub55_States` `0x65EF00` | 3 | `WeretigerFx_Next`, `_Wait`, `_Fade` | `0x65EF0C`, sub-kind 0x59's shade bytes |
| `EffectKind18Sub59_States` `0x65EF18` | 6 | `_Start` .. `_Close` | `0x65EF30`, the ring's radii |
| `EffectKind18Sub5A_States` `0x65EF58` | 6 | `_Start` .. `_CycleOpen` | `0x65EF70`, the moves' rows |
| `EffectKind18Sub5B_States` `0x65EF78` | 8 | `_Start` .. `_Close` | `0x65EF98`, the 49 pieces |

The states keep `+2` inside each: 0x45 0..2 up; 0x51 0..2 up; 0x55's three
step up and release; 0x59 0..5 up; 0x5A 0..4 up or 5 from `_Start`; 0x5B 0..7
up or 6 from `_Start`.

**The data read in place** (raw in `effect_6c_callees.h`, not named in
`symbols.toml`; their bytes are not copied here): the block map `0x65ED78`
(256); the sky's x words `0x65EE80` (five), y bytes `0x65EE8C` (five), tints
`0x65EE94` (sixteen s8 triples, to the twinkle pairs at `0x65EEC4`); the
ring's uv `0x65EED8` (two pairs); sub-kind 0x51's rectangles `0x65EEDC`
(five); 0x53's spins `0x65EEFC`; **0x59's shades `0x65EF0C` (twelve bytes the
fade writes)**, radii `0x65EF30`, heights `0x65EF3C`, rows `0x65EF48`
(sixteen); 0x5A's rows `0x65EF70` / `0x65EF74`; 0x5B's pieces `0x65EF98` and
ranges `0x65F36C` (three pairs); the float constants `0x5C41DC` (0.0),
`0x5C4248` (89.0), `0x5C424C` (90.0), `0x5C4250`. `SetTiles`' two six-byte
tables are immediates the original stores on its stack - code constants.

## 4. The fuzz (`effect_6c_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_6c`, effect mode (`g.effect`; kind 0x18
for every clone), 4,000 rounds a function (`BOF3X_E6C_ONLY=<name>` runs the
clones whose name holds it). Shapes: 39 `kEffect` (the seven dispatchers'
`sub_span` their table's length), 11 `kCall` (`DrawTwinkle`'s tile an
`Arg::kScratch` buffer), `AreaMap_CornerHeight` with `ret_mask` `0xFFFFFFFF`
(it answers `eax`; nothing else answers). `EffectKind18Sub51_Wait`'s clone
carries its jump table (`{0x12, 0x68, 5}`). The six state tables are
`DataTable`s, swapped for recorders on both sides.

**Regions** beyond effect mode's standard ones: `Sprite_DrawList` (40
pointers), the list's last pointers `0x802B34` (two buffers), 64 draw items
`0x905E80` (`MapView_ItemAt`'s stand-in answers 1..0x3F), and sub-kind 0x59's
twelve `.data` shade bytes `0x65EF0C`.

**Callees**: the standard and effect-standard rows for the `Gpu_*`, `Gte_*`,
`Math_*`, `Rand`, `Flags_*`, `MoveCmd_TestFB`, `MapView_ItemAt`,
`Prim_SetTexture`, `AreaMap_Elevation`, `Gfx_*`, `Sprite_EnsureAnimation`,
`Sprite_ScriptTick`, `Sound_PlayEffect`, `Effect_Release`; **one re-listed**:
`MapView_LinkPrimAt` with its dy compared on the low byte (the real one reads a
signed byte; `DrawRing` pushes `cl` / `al` / `dl` over whatever the register
held), the cursor moved as effect mode's row moves it. `EffectGte_ProjectSize`
and the square root are not called here. **The group's own** called directly,
listed by name: `EffectKind18Sub44_Commit` (its size a byte; it moves the
cursor while 0x160 bytes stay after it - the sky lays four bands from the
cursor without re-reading it), `DrawStars` / `DrawGlow` (logging the colour
rows `0x9037A0..0x9037AF` they read), `DrawTwinkle` (hashing 16 bytes of the
star), the ring draws, `Sub55_Draw`, `Sub5A_SetTiles` / `_Move` (each logging
`Sprite_Current`, its `+8..+0xB` and point), `Sub59_Move` (`kPhase`) and
`Sub5B_DrawPart` (also logging the two offsets the states set before it).

**Seeds** (after the harness's per-round fill): on all 20 records the variant
`+0x36` 0..4 (the disturbance moves `Sprite_Current` among them, and sub-kind
0x51 re-reads it) and `+9` at its bounds (0, 3..5, 8..9, 0xF..0x12, 0x17,
0x18, 0x1E..0x21, 0x30, 0x31, 0x5F..0x61, 0x7F..0x81, 0xBF, 0xC0, 0xCF, 0xD0,
0xFC, 0xFF, any); Cond_ByteFE 0..3 or any; `Gfx_BufferIndex` 0 / 1 and the two
last pointers into the scratch buffer half the time; the frame word at its
bands' edges inside its wrap; the camera's cell about each glow's point,
`DrawStars`' clamps, or sub-kind 0x53's record (within and past 20);
`Camera_Distance` at -0x1193 / -0x1195 and never -0x1194 (section 7); the
draw-list count at 0x27..0x29. Per function: for the cell writers a width and
height 1..16, a base and every cell word 0..0x3F (all inside the area block's 8
KiB) and the 64 items' link words below 0x40; for sub-kind 0x59 the shade
bytes at the cascade's steps (0, 2, 0x60..0x64, 0x80, the first nine zero up to
a random point); sub-kind 0x53's `+2` 0 half the time; `Follow`'s leader in
state 3, moving or not, the record on its animation or not; the commit's
cursor at the real pool's room test (E5C's form) half the time.
**Arguments**: the levels, radii and shades at their callers' values;
`CornerHeight`'s point at a cell whose block keeps the corner dword inside the
area block (the block map read from the image at run time) and a fraction at
the triangles' edge; `SetTiles` step 0..0x1F, bank 0 / 1; `DrawPart` 0..2.
**Disturbance** (the group's, from the hash only): `+9`, `+0xA`, Cond_ByteFE,
the variant (inside its five), a camera cell word, the frame word (inside its
wrap), `Camera_Distance` (never -0x1194).

**The fuzz's own finding** (fixed in ours before the run below): the harness's
disturbance can leave the cursor less than a prim past the last, and
`EffectKind18Sub59_DrawRing` copies the first GT4's screen points into the
second dword by dword - a forward copy that reads what it has just written
when the two overlap. Ours copied with `memmove` and mismatched in 842 rounds;
it now copies in the original's order (the last dword read before two uv bytes
are written).

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_6c`,
exit 0): 200,000 rounds over 50 functions, 18,023,900 calls to the stand-ins,
**0 mismatches**; 34,160 bytes of state in 49 regions. Every entry of the six
tables reached (each handler recorder about 450..1,390 calls); `DrawGlow`
about 700 calls from the sky, `DrawStars` 2,714, `DrawTwinkle` 6,819,
`Gfx_ClutAdjust` about 2,100, `Gpu_LinkPrim` about 800 (the commit's room
test passed), `AreaMap_Elevation` about 2,000, `Sprite_ScriptTick` about
2,000, `Math_Ratan2` about 11,600 (counts in this worktree).

**Every shadow** (this worktree, no `bof3x.ini`, at the final fuzz):
`BOF3X_SHADOW='*'` exit 0, 713 self-test lines, no `MISMATCH:` line,
`inject: 8499 ours, 0 left original` (8,449 + 50); `effect_6c` there 200,000
rounds, 18,087,233 calls, 0 mismatches. **With `BOF3X_WIDE=1`**: `'*'` exit 0,
713 self-test lines, no mismatch, the same counts. An earlier `'*'` narrow
(before the controls' seed fixes) also exited 0 with 713 lines. Neither run
died silently. `tools/ledger_check.py`: 72 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **`0x5124C0`** (the cut's suspect): `band_rows.py` reads it as "a case of
  `0x512490`'s own switch, no address reference" and so does the code -
  `EffectKind18Sub51_Wait`'s `jmp [eax*4 + 0x5124F8]` has five cases
  (`0x5124A6`, `0x5124AF`, `0x5124BC`, `0x5124C0`, `0x5124DC`); case 3 pushes
  0x7B and joins case 2 at `0x5124C2`. Not a function: taken inside
  `EffectKind18Sub51_Wait`, whose extent (`0x7C`) includes the table.
- **Added**: `0x512510` (the tool's "code no list has", `EffectKind18Sub51_States[2]`),
  `0x512B20` and `0x513CD0` (the dispatchers `EffectKind18_States[0x59]` /
  `[0x5B]`, the catalog's part-2 rows "Table EffectKind18_States" in no group).
  Every `EffectKind18_States` cell pointing into the band is now ours or E6B's
  (`[0x44]` `0x510C20`).
- **Extents**: the tool's 49 (13,076 bytes against the cut's 13,629: 31
  differ by padding only, `0x512490` by its jump table and `0x5124C0`'s 416
  was the rest of the host), each checked by hand to its `ret` or tail `jmp`.
  No tail draw shared by `jmp` without its own frame; `EffectKind18Sub59_Move`
  is tail-jumped to by three states and called by one, its own frame and
  `ret` - taken whole as a function.
- **Hidden starts**: 35 of the cut's; each an entry by address (a table cell,
  a call, a jump) inside the catalog's hosts `0x510C90`, `0x511D50`,
  `0x512040`, `0x512960`, `0x513410`, `0x513C70`, whose recorded extents span
  them; none of the hosts contains another's code as a fall-through.
- **The cut's columns**: the `unit_desc` hints (`Fn_513A60`, `Fn_513CD0`, ...)
  name the right tables; "X:Capcom's raw" for `0x510C90` / `0x510EB0` means
  E6B's `0x510C80`; "X:W4 Fn_510EB0" the sky's callees.

## 6. Controls

Planted one at a time by a scratch script (`controls.py`: each plant
anchored on a unique string of `effect_6c.cpp`, rebuilt, run under
`BOF3X_E6C_ONLY=<filter>`, the file restored and rebuilt at the end; the
committed file has no switch). The harness stops counting at the first
differing round only in its log, so the column is the first round that refused
it (0-based) in this worktree; every refused run exited 3 on a `MISMATCH`
line. **195 planted: 191 refused by a count, 3 equivalent mutants each with a
refused near variant, 1 stopped by ours' own abort with a refused near
variant.** Controls 1..191 ran under the first seeds; six were not refused
there, and the fuzz was mended for two of them:

- **67** (`_Open`'s inner `< 0xFF`) and **153** (`_Animate`'s end at 0xCF):
  the fuzz's fault - the radius `+0x3C` was random 32 bits and `+9` never
  0xCE. The seeds now put `+0x3C` about 0xF8, 0x118 and 0x1F8 and `+9` at
  0xCE; both refused (rounds 4 and 5).
- **23** (`Follow`'s quiet copy of the screen y made a plain one):
  equivalent under the harness - the y is always the `Gte_RotTransPers`
  stand-in's whole-number float, never a signalling NaN, the one value the two
  copies differ on. Near variant **192** (the copy's low bit flipped) refused.
- **32** (the third star's `/ 98304` as `/ 98303`): equivalent - the two
  divides differ only for `|0x1400 - z|` of 3,723 or more, where y is at least
  148 and the star is never drawn, and its stored y is overwritten at the same
  cursor by the next star's size before anything reads it. The seeds now pin
  such z (and the second star's y at 89..91, through `settle`, for `DrawStars`'
  rounds); near variant **195** (`/ 65536`) refused.
- **165** (the link word `& 0x7FF` for `& 0xFFF`): equivalent - they differ
  only for a link of 0x800 or more, past the 1,024 items the pool has during
  the self-test, where ours aborts. The link seeds now run to 0x3FF; near
  variant **194** (`& 0x1FF`) refused.
- **51** (the sky's third band ending at 0x38F): **stopped by ours' abort** -
  the frame word 0x38F then reaches the last band with `n` = `(0x38F - 0x390) /
  3`, past the sixteen tints, where the original reads on. Near variant
  **193** (`0x391`) refused.

The other 185 of the first run were not re-run under the final seeds (the
seeds only add values).

| # | Run (`_ONLY`) | Plant | For | Refused at |
|--:|---|---|---|---|
| 1 | `CornerHeight` | `const U xi = Sar(ux + 0x7FFFu, 16) & 0xFF` | `const U xi = Sar(ux + 0x8000u, 16) & 0xFF` | round 9 |
| 2 | `CornerHeight` | `* 0x61 + (((block & 0xF) << 4)` | `* 0x60 + (((block & 0xF) << 4)` | round 0 |
| 3 | `CornerHeight` | `if (I(fz + fx) < 0x10000) {` | `if (I(fz + fx) <= 0x10000) {` | round 7 |
| 4 | `CornerHeight` | `Sar(static_cast<U>(c2 - c0) * fz, 16)` | `DivP2(static_cast<U>(c2 - c0) * fz, 16)` | round 0 |
| 5 | `CornerHeight` | `h = static_cast<U>(c3) - from_z + from_x;` | `h = static_cast<U>(c3) - from_z - from_x;` | round 4 |
| 6 | `CornerHeight` | `return static_cast<long>(h << 4);` | `return static_cast<long>(h << 5);` | round 0 |
| 7 | `CornerHeight` | `const std::int32_t c1 = static_cast<unsigned char>((corners >> 8) &...` | `const std::int32_t c1 = static_cast<signed char>((corners >> 8) & 0...` | round 3 |
| 8 | `Sub44_Commit` | `if (limit < AddressOf(next) + bytes) return;` | `if (limit <= AddressOf(next) + bytes) return;` | round 8 |
| 9 | `Sub44_Commit` | `SetUL(at::kLayerTails + 8u * Gfx_BufferIndex, AddressOf(next));` | `SetUL(at::kLayerTails + 8u * Gfx_BufferIndex, AddressOf(now));` | round 18 |
| 10 | `Sub44_Commit` | `const U bytes = size & 0x7Fu;` | `const U bytes = size & 0xFFu;` | round 33 |
| 11 | `Sub44_Follow` | `` | `        S()[0x4B] = *At(at::kLeaderAnimation); ` | round 5 |
| 12 | `Sub44_Follow` | `if (*At(at::kLeaderState) == 2) SH_CALL` | `if (*At(at::kLeaderState) == 3) SH_CALL` | round 0 |
| 13 | `Sub44_Follow` | `        S()[0x5E] = 0x80;         S()[0x5F] = 0x81;         Cond_By...` | `        S()[0x5E] = 0x80;         S()[0x5F] = 0x80;         Cond_By...` | round 3 |
| 14 | `Sub44_Follow` | `SetWord(S() + 0x32, 0x3201);` | `SetWord(S() + 0x32, 0x3200);` | round 0 |
| 15 | `Sub44_Follow` | `SetV(4, 0u - Sar(static_cast<U>(S16(at::kLeaderLift)), 1));` | `SetV(4, 0u - DivP2(static_cast<U>(S16(at::kLeaderLift)), 1));` | round 1 |
| 16 | `Sub44_Follow` | `SetV(2, Sar(((static_cast<U>(Field_Kind2Z) - 1u) & 0xFFFFFFu) + 0u,...` | `SetV(2, Sar(((static_cast<U>(Field_Kind2Z) - 1u) & 0xFFFFFFu) + 1u,...` | round 512 |
| 17 | `Sub44_Follow` | `SetUL(S() + 0x74, 0x43210000u);` | `SetUL(S() + 0x74, 0x43200000u);` | round 0 |
| 18 | `Sub44_Follow` | `        SetWord(At(at::kLeaderDepth), Ftol(D(at::kScreenY)) + 1u);` | `        SetWord(At(at::kLeaderDepth), Ftol(D(at::kScreenY)));` | round 4 |
| 19 | `Sub44_Follow` | `SetD(S() + 0x78, Fi(static_cast<U>(Word(At(at::kLeaderDepth)))));` | `SetD(S() + 0x78, Fi(static_cast<U>(S16(at::kLeaderDepth))));` | round 1 |
| 20 | `Sub44_Follow` | `if (count < 0x27) {` | `if (count < 0x28) {` | round 0 |
| 21 | `Sub44_Follow` | `        prim[4] = 0xFE;         prim[5] = 0;` | `        prim[4] = 0xFF;         prim[5] = 0;` | round 4 |
| 22 | `Sub44_Follow` | `SH_CALL(Gfx_CommitPrim)(3, 0x18);` | `SH_CALL(Gfx_CommitPrim)(3, 0x1C);` | round 4 |
| 23 | `Sub44_Follow` | `SetUL(S() + 0x78, UL(at::kScreenY));` | `CopyQuiet(S() + 0x78, At(at::kScreenY));` | **equivalent** (section 6) |
| 24 | `Sub44_DrawTwinkle` | `p[5] = static_cast<unsigned char>(tile[5] >> 1);` | `p[5] = static_cast<unsigned char>(tile[5] >> 2);` | round 0 |
| 25 | `Sub44_DrawTwinkle` | `S8(at::kTwinkle + 2 * i)` | `S8(at::kTwinkle + 2 * i + 1)` | round 1 |
| 26 | `Sub44_DrawTwinkle` | `for (unsigned i = 0; i < 3; ++i) {         unsigned char* const p =...` | `for (unsigned i = 0; i < 4; ++i) {         unsigned char* const p =...` | round 0 |
| 27 | `Sub44_DrawStars` | `DivP2(level * 0xFEu, 7)` | `DivP2(level * 0xFFu, 7)` | round 1 |
| 28 | `Sub44_DrawStars` | `SetUL(p + 0xC, 0x42300000u);` | `SetUL(p + 0xC, 0x42340000u);` | round 0 |
| 29 | `Sub44_DrawStars` | `S16(at::kKind2ZHigh) / 84 - 1` | `S16(at::kKind2ZHigh) / 85 - 1` | round 2 |
| 30 | `Sub44_DrawStars` | `if (!(y < F(at::kZero)) && y <= F(at::kNinety)) {` | `if (!(y < F(at::kZero)) && y < F(at::kNinety)) {` | round 1073 |
| 31 | `Sub44_DrawStars` | `>= 0xE00 ? 1u : 0u;` | `>= 0xF00 ? 1u : 0u;` | round 15 |
| 32 | `Sub44_DrawStars` | `I(dz * dz) / 98303)` | `I(dz * dz) / 98304)` | **equivalent** (section 6) |
| 33 | `Sub44_DrawStars` | `- 0x1480) / 16);` | `- 0x1480) / 17);` | round 0 |
| 34 | `Sub44_DrawStars` | `p[c] = static_cast<unsigned char>(DivP2(((r + 0x80u) & 0x7F) * leve...` | `p[c] = static_cast<unsigned char>(DivP2(((r - 0x80u) & 0xFF) * leve...` | round 0 |
| 35 | `Sub44_DrawStars` | `I(0xD00u - az - ax) / 42 + 0x14` | `I(0xD00u - az - ax) / 43 + 0x14` | round 0 |
| 36 | `Sub44_DrawStars` | `if (fs >= F(at::kEightyNine)) continue;` | `if (fs > F(at::kEightyNine)) continue;` | round 81 |
| 37 | `Sub44_DrawStars` | `(x > 0x1E01 ? 0x1E01 : x)` | `(x > 0x1E00 ? 0x1E00 : x)` | round 37 |
| 38 | `Sub44_DrawStars` | `(r + Cond_AngleFB + 0x632u) & 0xFFF) - back` | `(r + Cond_AngleFB + 0x633u) & 0xFFF) - back` | round 0 |
| 39 | `Sub44_DrawStars` | `SH_CALL(Math_Ratan2)(static_cast<float>(cx - 0x1480), static_cast<f...` | `SH_CALL(Math_Ratan2)(static_cast<float>(cz - 0x1380), static_cast<f...` | round 0 |
| 40 | `Sub44_DrawGlow` | `right = static_cast<float>(Fi(x + 0x4Fu));` | `right = static_cast<float>(Fi(x + 0x4Eu));` | round 0 |
| 41 | `Sub44_DrawGlow` | `SetWord(p + 0x26, 0xD6);` | `SetWord(p + 0x26, 0xD7);` | round 0 |
| 42 | `Sub44_DrawGlow` | `SetWord(p + 0x16, (row + 0x1E7) << 6);` | `SetWord(p + 0x16, (row + 0x1E6) << 6);` | round 0 |
| 43 | `Sub44_DrawGlow` | `S16(VertexAddress(8 * row + 2 * c))) * level, 6)` | `S16(VertexAddress(8 * row + 2 * c))) * level, 7)` | round 0 |
| 44 | `Sub44_DrawGlow` | `static_cast<unsigned char>(row * 0x40);` | `static_cast<unsigned char>(row * 0x50);` | round 0 |
| 45 | `Sub44_Draw` | `unsigned char* const window = Rect(0x20, 0, 0x20, 0x10);` | `unsigned char* const window = Rect(0x20, 0, 0x20, 0x20);` | round 0 |
| 46 | `Sub44_Draw` | `SH_CALL(Gpu_SetShadeTex)(p, 0);` | `SH_CALL(Gpu_SetShadeTex)(p, 1);` | round 0 |
| 47 | `Sub44_Draw` | `SetUL(p + 0x2C, 0x42F00000u);  // 122.0f` | `SetUL(p + 0x2C, 0x42F40000u);  // 122.0f` | round 0 |
| 48 | `Sub44_Draw` | `SetUL(p + 0xC, 0x42400000u);   // 50.0f` | `SetUL(p + 0xC, 0x42480000u);   // 50.0f` | round 0 |
| 49 | `Sub44_Draw` | `if (clock < 0x1B1) {` | `if (clock < 0x1B0) {` | round 24 |
| 50 | `Sub44_Draw` | `I(n * 4) / 10 - 6, I(n) / 8 - 1, 0);` | `I(n * 4) / 10 - 6, I(n) / 8 - 2, 0);` | round 8 |
| 51 | `Sub44_Draw` | `} else if (clock < 0x38F) {` | `} else if (clock < 0x390) {` | **stopped by ours' abort** (section 6) |
| 52 | `Sub44_Draw` | `SetV(0, 0x80u - past * 8u);` | `SetV(0, 0x80u - past * 12u);` | round 4 |
| 53 | `Sub44_Draw` | `S8(at::kSkyTints + 3 * n + 2),` | `S8(at::kSkyTints + 3 * n + 1),` | round 12 |
| 54 | `Sub44_Draw` | `const U past = FloorZero(n - 5);` | `const U past = FloorZero(n - 6);` | round 4 |
| 55 | `Sub44_Draw` | `SetD(p + 8, Fi((AngleX(Cond_AngleFB) & 0x7F) + column - 0x100u));` | `SetD(p + 8, Fi((AngleX(Cond_AngleFB) & 0xFF) + column - 0x100u));` | round 4 |
| 56 | `Sub44_Draw` | `v1 = static_cast<unsigned char>(v0 + 0x5B);` | `v1 = static_cast<unsigned char>(v0 + 0x5A);` | round 0 |
| 57 | `Sub44_Draw` | `SetD(p + 0x3C, Fi(*At(at::kSkyYs + m)));` | `SetD(p + 0x3C, Fi(*At(at::kSkyYs + m + 1)));` | round 0 |
| 58 | `Sub44_Draw` | `            RowColour(p + 0x14, row ^ 1);` | `            RowColour(p + 0x14, row);` | round 0 |
| 59 | `Sub44_Draw` | `U level = 0xC1u - static_cast<U>(I(near) / 3);` | `U level = 0xC0u - static_cast<U>(I(near) / 3);` | round 4 |
| 60 | `Sub44_Draw` | `U level = 0x100u - DivP2(Abs(dz) + Abs(dx), 1);` | `U level = 0x100u - DivP2(Abs(dz) + Abs(dx), 2);` | round 12 |
| 61 | `Sub44_Draw` | `static_cast<float>(I(dx)), static_cast<float>(z)` | `static_cast<float>(I(dx)), static_cast<float>(-z)` | round 12 |
| 62 | `Sub44_Draw` | `if (SH_CALL(Flags_Test)(At(at::kFlagRow), 5) == 0) return;` | `if (SH_CALL(Flags_Test)(At(at::kFlagRow), 4) == 0) return;` | round 0 |
| 63 | `Sub44_Draw` | `unsigned char* const p = Gfx_PacketNext;` | `unsigned char* const p = first + 0x44 * m;` | round 0 |
| 64 | `Sub45_Run` | `Dispatch("EffectKind18Sub45_Run", E6C_TABLE(EffectKind18Sub51_State...` | `Dispatch("EffectKind18Sub45_Run", E6C_TABLE(EffectKind18Sub45_State...` | round 0 |
| 65 | `Sub45_Wait` | `    S()[0xB] = 1;     SetUL(S() + 0x3C, 0);` | `    S()[0xB] = 0;     SetUL(S() + 0x3C, 0);` | round 3 |
| 66 | `Sub45_Open` | `const U inner = Ring(v, 0x18, false);` | `const U inner = Ring(v, 0x20, false);` | round 0 |
| 67 | `Sub45_Open` | `if (I(inner) < 0xFF) return;` | `if (I(inner) <= 0xFF) return;` | round 4 |
| 68 | `Sub45_Spread` | `SH_CALL(EffectKind18Sub45_DrawRing)(I(r[k]), I(r[k + 1]), (k & 1) ?...` | `SH_CALL(EffectKind18Sub45_DrawRing)(I(r[k]), I(r[k + 1]), (k & 1) ?...` | round 0 |
| 69 | `Sub45_Spread` | `if (cap && I(r) > 0xFF) return 0xFF;` | `if (cap && I(r) > 0xFF) return 0x100;` | round 0 |
| 70 | `Sub45_Spread` | `SH_CALL(MoveCmd_TestFB)(0xB, 0x21);` | `SH_CALL(MoveCmd_TestFB)(0xB, 0x22);` | round 845 |
| 71 | `Sub45_Spread` | `        Cond_ByteFE = 3;         SH_CALL(Effect_Release)();` | `        Cond_ByteFE = 2;         SH_CALL(Effect_Release)();` | round 0 |
| 72 | `Sub45_DrawRing` | `SetUL(at::kScreenY, 0xC63C0000u);` | `SetUL(at::kScreenY, 0xC63D0000u);` | round 0 |
| 73 | `Sub45_DrawRing` | `SetV(8 * v + 2, Ftol(D(at::kScreenY) + Fi(sine)));` | `SetV(8 * v + 2, Ftol(D(at::kScreenY) - Fi(sine)));` | round 0 |
| 74 | `Sub45_DrawRing` | `radius, 12);             SetV(8 * v + 4` | `radius, 13);             SetV(8 * v + 4` | round 0 |
| 75 | `Sub45_DrawRing` | `(Sar((half << 7) + 0x100u, 6) & 0xF) \| 0x90u` | `(Sar((half << 7) + 0x140u, 6) & 0xF) \| 0x90u` | round 0 |
| 76 | `Sub45_DrawRing` | `p[c.at] = static_cast<unsigned char>(Sar(sine * c.radius, 16) + uv[...` | `p[c.at] = static_cast<unsigned char>(Sar(sine * c.radius, 16) + uv[...` | round 0 |
| 77 | `Sub45_DrawRing` | `{0x34, q1, inner}` | `{0x34, q0, inner}` | round 0 |
| 78 | `Sub45_DrawRing` | `UL(s + 0x38), 1, 0x48);     } }` | `UL(s + 0x38), 0, 0x48);     } }` | round 0 |
| 79 | `Sub45_DrawRing` | `SH_CALL(Gpu_SetSemiTrans)(p, 1);         p[6] = 0x50;` | `SH_CALL(Gpu_SetSemiTrans)(p, 0);         p[6] = 0x50;` | round 0 |
| 80 | `Sub51_Run` | `E6C_TABLE(EffectKind18Sub45_States)); }` | `E6C_TABLE(EffectKind18Sub51_States)); }` | round 0 |
| 81 | `Sub51_Place` | `SetUL(record, UL(record) & 0xFF0FFFFFu);` | `SetUL(record, UL(record) & 0xFF07FFFFu);` | round 0 |
| 82 | `Sub51_Place` | `Item("EffectKind18Sub51_Place", item), 1);` | `Item("EffectKind18Sub51_Place", item), 2);` | round 0 |
| 83 | `Sub51_Place` | `for (U z = Rect51(who, 1); I(z) < I(Rect51(who, 3)); ++z)` | `for (U z = Rect51(who, 1); I(z) <= I(Rect51(who, 3)); ++z)` | round 3 |
| 84 | `Sub51_Place` | `const U half = (height * width) / 2;` | `const U half = (height * width + 1) / 2;` | round 6 |
| 85 | `Sub51_Place` | `return At(at::kAreaHeader + 4 * (word + half + run));` | `return At(at::kAreaHeader + 4 * (base + word + half + run));` | round 0 |
| 86 | `Sub51_Wait` | `if (v > 3) return;` | `if (v > 4) return;` | round 9 |
| 87 | `Sub51_Wait` | `SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x79 + (v - 2))` | `SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x7A + (v - 2))` | round 4 |
| 88 | `Sub51_Wait` | `v == 0 ? SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x14)` | `v == 0 ? SH_CALL(Flags_Test)(At(at::kFlagRow), 0x14)` | round 0 |
| 89 | `Sub51_Wait` | `if (Cond_ByteFE == 2) s[2]` | `if (Cond_ByteFE == 1) s[2]` | round 3 |
| 90 | `Sub51_Fade` | `const U step = S()[9] > 0xF ? 0xFu : S()[9];` | `const U step = S()[9] > 0x10 ? 0x10u : S()[9];` | round 1 |
| 91 | `Sub51_Fade` | `\| (step << 18));` | `\| (step << 19));` | round 1 |
| 92 | `Sub51_Fade` | `const U half = item * 2;` | `const U half = Gfx_BufferIndex + item * 2;` | round 1 |
| 93 | `Sub51_Fade` | `if (S()[9] > 0x10) SH_CALL(Effect_Release)();` | `if (S()[9] > 0x11) SH_CALL(Effect_Release)();` | round 35 |
| 94 | `Sub53_Run` | `if (I(near) >= 0x14) return;` | `if (I(near) > 0x14) return;` | round 213 |
| 95 | `Sub53_Run` | `S()[9] = static_cast<unsigned char>(S()[9] + 1);` | `S()[9] = static_cast<unsigned char>(S()[9] + 2);` | round 21 |
| 96 | `Sub53_Run` | `0xFEu - ((S()[9] - ring) & 0xFF)` | `0xFFu - ((S()[9] - ring) & 0xFF)` | round 21 |
| 97 | `Sub53_Run` | `SetV(0, Sar(sine, 6) * 3 + Sar(UL(S() + 0x34), 8) - 0x4000u);` | `SetV(0, Sar(sine, 6) * 3 + Sar(UL(S() + 0x34), 9) - 0x4000u);` | round 21 |
| 98 | `Sub53_Run` | `SetV(4, (0u - DivP2(UL(s + 0x3C), 1)) - lift);` | `SetV(4, (0u - DivP2(UL(s + 0x3C), 1)) - (lift << 1));` | round 21 |
| 99 | `Sub53_Run` | `SetUL(S() + 0x3C, static_cast<U>(h));` | `SetUL(S() + 0x3C, static_cast<U>(static_cast<std::int16_t>(static_c...` | round 0 |
| 100 | `Sub53_Run` | `UL(s + 0x38), 2, 0x14);` | `UL(s + 0x38), 1, 0x14);` | round 21 |
| 101 | `Sub53_Run` | `const std::int32_t spin = S8(at::kSpin53 + ((k + 1) & 1));` | `const std::int32_t spin = S8(at::kSpin53 + (k & 1));` | round 21 |
| 102 | `Sub55_Run` | `E6C_TABLE(EffectKind18Sub51_States));` | `E6C_TABLE(EffectKind18Sub55_States));` | round 0 |
| 103 | `Sub55_Wait` | `SH_CALL(EffectKind18Sub55_Draw)(0x7F, 0);` | `SH_CALL(EffectKind18Sub55_Draw)(0x80, 0);` | round 0 |
| 104 | `Sub55_Wait` | `Flags_Test)(At(at::kFlagRow), 0xF)` | `Flags_Test)(At(at::kFlagRow), 0xE)` | round 0 |
| 105 | `Sub55_Fade` | `if (S()[9] > 0x10) SH_CALL(Effect_Release)();` | `if (S()[9] > 0xF) SH_CALL(Effect_Release)();` | round 69 |
| 106 | `Sub55_Fade` | `SH_CALL(EffectKind18Sub55_Draw)(I((0x10u - n) << 3), I(n * 4));` | `SH_CALL(EffectKind18Sub55_Draw)(I((0x10u - n) << 3), I(n * 8));` | round 1 |
| 107 | `Sub55_Draw` | `if (((x + z) & 1) == 0) continue;` | `if (((x + z) & 1) != 0) continue;` | round 0 |
| 108 | `Sub55_Draw` | `SetV(2, (((z - 0x80u) << 6) + lift) << 1);` | `SetV(2, (((z - 0x80u) << 6) - lift) << 1);` | round 0 |
| 109 | `Sub55_Draw` | `Abs(3u - (((Frame_Counter / 3) + x + z) & 7)) + 0x17` | `Abs(3u - (((Frame_Counter / 3) + x + z) & 7)) + 0x18` | round 0 |
| 110 | `Sub55_Draw` | `SetD(p + 0x2C, (sy - h) + h);` | `SetD(p + 0x2C, (sy - h) + side);` | round 0 |
| 111 | `Sub55_Draw` | `DivP2(shade << 5, 7)` | `DivP2(shade << 6, 7)` | round 2 |
| 112 | `Sub55_Draw` | `SH_CALL(MapView_LinkPrimAt)(z << 16, x << 16, 1, 0x48);` | `SH_CALL(MapView_LinkPrimAt)(x << 16, z << 16, 1, 0x48);` | round 0 |
| 113 | `Sub59_Run` | `E6C_TABLE(EffectKind18Sub5A_States));` | `E6C_TABLE(EffectKind18Sub59_States));` | round 0 |
| 114 | `Sub59_Move` | `SetWord(rect + 2, 0x140u - (S()[0xA] >> 2));` | `SetWord(rect + 2, 0x140u - (S()[0xA] >> 1));` | round 0 |
| 115 | `Sub59_Move` | `SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x198, 0x121);` | `SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x198, 0x120);` | round 0 |
| 116 | `Sub59_Start` | `    S()[0xA] = 1;     S()[2] = static_cast<unsigned char>(S()[2] + ...` | `    S()[0xA] = 0;     S()[2] = static_cast<unsigned char>(S()[2] + ...` | round 0 |
| 117 | `Sub59_Wait` | `if (Cond_ByteFE == 1) S()[2] = static_cast<unsigned char>(S()[2] + ...` | `if (Cond_ByteFE == 1) S()[2] = static_cast<unsigned char>(S()[2] + ...` | round 0 |
| 118 | `Sub59_Rise` | `if (S()[9] < 0x1F) S()[9]` | `if (S()[9] < 0x20) S()[9]` | round 53 |
| 119 | `Sub59_Rise` | `SH_CALL(EffectKind18Sub59_DrawRing)(I(0x20u + S()[9]));` | `SH_CALL(EffectKind18Sub59_DrawRing)(I(0x20u - S()[9]));` | round 0 |
| 120 | `Sub59_Hold` | `Scroll59(3);` | `Scroll59(2);` | round 0 |
| 121 | `Sub59_Fade` | `if (n == 3 \|\| shades[e + n] >= 0x60) break;` | `if (n == 3 \|\| shades[e + n] > 0x60) break;` | round 1 |
| 122 | `Sub59_Fade` | `shades[e + n] = static_cast<unsigned char>(shades[e + n] - 1);` | `shades[e + n] = static_cast<unsigned char>(shades[e + n] - 2);` | round 0 |
| 123 | `Sub59_Fade` | `for (unsigned e = 0; e < 8; ++e) {` | `for (unsigned e = 0; e < 9; ++e) {` | round 4 |
| 124 | `Sub59_Fade` | `if (*At(at::kShades59 + 7) != 0) return;` | `if (*At(at::kShades59 + 8) != 0) return;` | round 1 |
| 125 | `Sub59_Fade` | `const U glow = S()[9] > 0x5F ? 0x5Fu : S()[9];` | `const U glow = S()[9] > 0x60 ? 0x60u : S()[9];` | round 5 |
| 126 | `Sub59_Close` | `S()[9] = static_cast<unsigned char>(S()[9] - 2);` | `S()[9] = static_cast<unsigned char>(S()[9] - 4);` | round 0 |
| 127 | `Sub59_DrawRing` | `const U b0 = DivP2(angle, 4), b1 = DivP2(angle + 0x800u, 4);` | `const U b0 = DivP2(angle, 4), b1 = DivP2(angle + 0x1000u, 4);` | round 0 |
| 128 | `Sub59_DrawRing` | `{0x10, b0, at::kRadii59 + 8 + ki, at::kHeights59 + 7 + ki},` | `{0x10, b0, at::kRadii59 + 8 + ki, at::kHeights59 + 8 + ki},` | round 0 |
| 129 | `Sub59_DrawRing` | `SetV(c.at + 4, 0u - (static_cast<U>(*At(c.height)) << 4));` | `SetV(c.at + 4, 0u - (static_cast<U>(*At(c.height)) << 5));` | round 0 |
| 130 | `Sub59_DrawRing` | `q[0x3D] = 0x3E;` | `q[0x3D] = 0x3F;` | round 0 |
| 131 | `Sub59_DrawRing` | `dress(p, 0xD5, 0x7900);` | `dress(p, 0xD5, 0x7940);` | round 0 |
| 132 | `Sub59_DrawRing` | `far = *At(at::kShades59 + 7 + ki);` | `far = *At(at::kShades59 + 8 + ki);` | round 0 |
| 133 | `Sub59_DrawRing` | `I((Abs(3u - ((half + Frame_Counter) & 7)) + 0x10) * 4500u) / diviso...` | `I((Abs(3u - ((half + Frame_Counter) & 7)) + 0x10) * 4500u) / diviso...` | round 0 |
| 134 | `Sub59_DrawRing` | `const U half = i;` | `const U half = DivP2(i, 1);` | round 0 |
| 135 | `Sub59_DrawRing` | `p[6] = bright;` | `p[6] = blue;` | round 0 |
| 136 | `Sub59_DrawRing` | `UL(s + 0x38), I(row), 0x48);` | `UL(s + 0x38), I((row + 1) & 0xFF), 0x48);` | round 0 |
| 137 | `Sub59_DrawRing` | `for (U back = 0; I(back) < 0x10; back += 8, e += 0x40) {` | `for (U back = 0; I(back) < 0x10; back += 8, e += 0x80) {` | round 0 |
| 138 | `Sub59_DrawRing` | `                SetUL(q + 0x4C, last + 1u);` | `                SetUL(q + 0x4C, last);` | round 0 |
| 139 | `Sub59_DrawRing` | `SetD(p + 0x1C, D(at::kGlowXY) - h);` | `SetD(p + 0x1C, D(at::kGlowY) - h);` | round 0 |
| 140 | `Sub66_Run` | `shades[0] = edge;` | `shades[0] = Sar(edge, 1);` | round 2 |
| 141 | `Sub66_Run` | `if (I(distance) >= 0x40) {` | `if (I(distance) > 0x40) {` | round 6 |
| 142 | `Sub66_Run` | `Sar(Sar(cosine, 7) + 0x50u, 1)` | `Sar(Sar(cosine, 7) + 0x60u, 1)` | round 6 |
| 143 | `Sub66_Run` | `const U offsets[2] = {0u - (m & 0x3F), 0u - ((m & 0x3F) << 1)};` | `const U offsets[2] = {0u - (m & 0x3F), 0u - ((m & 0x1F) << 1)};` | round 3 |
| 144 | `Sub66_Run` | `const U xs[3] = {0u - d - 0x80u, 0u - d + 0x80u, 0u - d + 0x170u};` | `const U xs[3] = {0u - d - 0x80u, 0u - d + 0x80u, 0u - d + 0x180u};` | round 2 |
| 145 | `Sub66_Run` | `SetWord(p + 0x26, 0x7A);` | `SetWord(p + 0x26, 0x7B);` | round 2 |
| 146 | `Sub66_Run` | `unsigned char* const whole = Rect(0, 0, 0x100, 0xFF);` | `unsigned char* const whole = Rect(0, 0, 0x100, 0x100);` | round 2 |
| 147 | `Sub66_Run` | `S()[9] = static_cast<unsigned char>(S()[9] + 3);     if (S()[9] == ...` | `S()[9] = static_cast<unsigned char>(S()[9] + 4);     if (S()[9] == ...` | round 2 |
| 148 | `Sub66_Run` | `if (Cond_ByteFE == 1) return;` | `if (Cond_ByteFE == 0) return;` | round 0 |
| 149 | `Sub5A_Run` | `E6C_TABLE(EffectKind18Sub59_States));` | `E6C_TABLE(EffectKind18Sub5A_States));` | round 0 |
| 150 | `Sub5A_Start` | `        S()[2] = 4;` | `        S()[2] = 5;` | round 0 |
| 151 | `Sub5A_Wait` | `if (Cond_ByteFE == 3) S()[2] = static_cast<unsigned char>(S()[2] + ...` | `if (Cond_ByteFE == 2) S()[2] = static_cast<unsigned char>(S()[2] + ...` | round 4 |
| 152 | `Sub5A_Animate` | `SH_CALL(EffectKind18Sub5A_SetTiles)(I(n >> 3), I((n >> 1) & 1));` | `SH_CALL(EffectKind18Sub5A_SetTiles)(I(n >> 3), I((n >> 2) & 1));` | round 10 |
| 153 | `Sub5A_Animate` | `if (S()[9] >= 0xCF)` | `if (S()[9] >= 0xD0)` | round 5 |
| 154 | `Sub5A_Set` | `SH_CALL(EffectKind18Sub5A_Move)(0xC0, 1);` | `SH_CALL(EffectKind18Sub5A_Move)(0xC0, 0);` | round 0 |
| 155 | `Sub5A_Set` | `CellRecord(e + 0x11, 0x60, 0);` | `CellRecord(e + 0x11, 0x61, 0);` | round 0 |
| 156 | `Sub5A_Set` | `\| (e + 0xE5));` | `\| (e + 0xE6));` | round 0 |
| 157 | `Sub5A_Set` | `SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x81);` | `SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x80);` | round 0 |
| 158 | `Sub5A_Cycle` | `s[9] = static_cast<unsigned char>(n < 0x12 ? n : 0);` | `s[9] = static_cast<unsigned char>(n < 0x18 ? n : 0);` | round 2 |
| 159 | `Sub5A_Cycle` | `if (n % 4 != 0) return;` | `if (n % 6 != 0) return;` | round 2 |
| 160 | `Sub5A_Cycle` | `Cycle5A(at::kSlide5A, 0xB0); }` | `Cycle5A(at::kSlide5A, 0xC0); }` | round 0 |
| 161 | `Sub5A_CycleOpen` | `Cycle5A(at::kSlide5A, 0); }` | `Cycle5A(at::kSlide5B, 0); }` | round 0 |
| 162 | `Sub5A_SetTiles` | `static const unsigned char kFirst[6] = {5, 4, 3, 0xB, 0xA, 8};` | `static const unsigned char kFirst[6] = {5, 4, 3, 0xB, 0xA, 9};` | round 3 |
| 163 | `Sub5A_SetTiles` | `const U z = step + 0x48;` | `const U z = step + 0x47;` | round 0 |
| 164 | `Sub5A_SetTiles` | `unsigned char* const record = CellRecord(0xD, z, 0);` | `unsigned char* const record = CellRecord(0xD, z, 1);` | round 0 |
| 165 | `Sub5A_SetTiles` | `Word(Item("EffectKind18Sub5A_SetTiles", item) + at::kItemLink) & 0x...` | `Word(Item("EffectKind18Sub5A_SetTiles", item) + at::kItemLink) & 0x...` | **equivalent** (section 6) |
| 166 | `Sub5A_SetTiles` | `\| (e + 2) \| mark);` | `\| (e + 3) \| mark);` | round 0 |
| 167 | `Sub5A_SetTiles` | `const U pick = static_cast<U>(I(step) % 3) + bank * 2;` | `const U pick = static_cast<U>(I(step) % 3) + bank * 3;` | round 0 |
| 168 | `Sub5A_Move` | `SetWord(rect + 2, static_cast<U>(x_word) + 0xF0u);` | `SetWord(rect + 2, static_cast<U>(x_word) + 0x100u);` | round 0 |
| 169 | `Sub5A_Move` | `SetWord(rect + 4, 0x50);` | `SetWord(rect + 4, 0x58);` | round 0 |
| 170 | `Sub5A_Move` | `rect, 0x240, static_cast<U>(y_word) + 0x101u);` | `rect, 0x240, static_cast<U>(y_word) + 0x100u);` | round 0 |
| 171 | `Sub5B_Run` | `E6C_TABLE(EffectKind18Sub5A_States));` | `E6C_TABLE(EffectKind18Sub5B_States));` | round 0 |
| 172 | `Sub5B_Start` | `        S()[2] = 7;` | `        S()[2] = 6;` | round 0 |
| 173 | `Sub5B_Start` | `    Cond_ByteFE = 2; ` | `    Cond_ByteFE = 3; ` | round 2 |
| 174 | `Sub5B_Wait` | `static_cast<unsigned short>(at::kSoundStep5B));` | `static_cast<unsigned short>(at::kSoundOpen5B));` | round 14 |
| 175 | `Sub5B_SlideOut` | `SlideOut(void) { Slide5B(9); }` | `SlideOut(void) { Slide5B(8); }` | round 3 |
| 176 | `Sub5B_SlideOut` | `    OffsetX(true, 3);     Part(0);     OffsetX(true, 3);` | `    OffsetX(true, 3);     Part(0);     OffsetX(false, 3);` | round 0 |
| 177 | `Sub5B_SlideWide` | `Slide5B(0x2F); }` | `Slide5B(0x30); }` | round 1173 |
| 178 | `Sub5B_Hold` | `SetOffset(0xC2700000u, 0);   // -64.0f` | `SetOffset(0xC2800000u, 0);   // -64.0f` | round 0 |
| 179 | `Sub5B_Hold` | `    S()[9] = 9; ` | `    S()[9] = 8; ` | round 1 |
| 180 | `Sub5B_Lift` | `SetD(at::kOffsetY, Fi(static_cast<U>(S()[9]) << 2));` | `SetD(at::kOffsetY, Fi(static_cast<U>(S()[9]) << 3));` | round 0 |
| 181 | `Sub5B_Lift` | `SH_CALL(MoveCmd_TestFB)(0x30, 0x70);` | `SH_CALL(MoveCmd_TestFB)(0x30, 0x71);` | round 1 |
| 182 | `Sub5B_Reopen` | `    Part(2);     S()[9] = 0x5F; ` | `    Part(2);     S()[9] = 0x60; ` | round 14 |
| 183 | `Sub5B_Reopen` | `SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x80);` | `SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x81);` | round 14 |
| 184 | `Sub5B_Close` | `s[9] = static_cast<unsigned char>(s[9] - 1);` | `if (s[9] != 0) s[9] = static_cast<unsigned char>(s[9] - 1);` | round 49 |
| 185 | `Sub5B_Close` | `    OffsetX(true, 2);     Part(0);     OffsetX(false, 3);` | `    OffsetX(true, 2);     Part(0);     OffsetX(false, 2);` | round 0 |
| 186 | `Sub5B_DrawPart` | `Ftol(Fi(static_cast<U>(c[0]) << 6) + D(AddressOf(MapView_ScreenXY)))` | `Ftol((Fi(static_cast<U>(c[0]) << 6) + D(at::kOffsetX)) + D(AddressO...` | round 1 |
| 187 | `Sub5B_DrawPart` | `SetV(8 * v + 4, Ftol(Fi((7u - c[2]) << 6) + D(at::kOffsetY)));` | `SetV(8 * v + 4, Ftol(Fi((6u - c[2]) << 6) + D(at::kOffsetY)));` | round 1 |
| 188 | `Sub5B_DrawPart` | `D(at::kOffsetX) * 2.0);` | `D(at::kOffsetX) * D(at::kPieceScale));` | round 1 |
| 189 | `Sub5B_DrawPart` | `SH_CALL(Prim_SetTexture)(UL(r + 0xC), p, 1);` | `SH_CALL(Prim_SetTexture)(UL(r + 0x10), p, 1);` | round 0 |
| 190 | `Sub5B_DrawPart` | `I(j) <= I(*At(at::kParts5B + 2 * part + 1))` | `I(j) < I(*At(at::kParts5B + 2 * part + 1))` | round 0 |
| 191 | `Sub5B_DrawPart` | `SetD(at::kScreenY, Fi((static_cast<U>(r[1]) << 7) - 0x4000u));     ...` | `SetD(at::kScreenY, Fi((static_cast<U>(r[1]) << 7) - 0x4040u));     ...` | round 0 |
| 192 | `Sub44_Follow` | `SetUL(S() + 0x78, UL(at::kScreenY) ^ 1u);` | `CopyQuiet(S() + 0x78, At(at::kScreenY));` | round 0 |
| 193 | `Sub44_Draw` | `} else if (clock < 0x391) {` | `} else if (clock < 0x390) {` | round 76 |
| 194 | `Sub5A_SetTiles` | `at::kItemLink) & 0x1FF;` | `at::kItemLink) & 0xFFF;` | round 34 |
| 195 | `Sub44_DrawStars` | `I(dz * dz) / 65536)` | `I(dz * dz) / 98304)` | round 6 |

## 7. Latent defects (Capcom's, described, not fixed)

- **Sub-kind 0x59's shades are `.data`, never restored.** (D226) The fade writes the
  twelve bytes `0x65EF0C..0x65EF17` of the image (initially 0x80 each) down to
  0 and nothing in the executable writes them back (a scan of `.text` for the
  addresses finds only this band's reads and the fade's writes). On the
  PlayStation the twin's table lived in an AREA overlay, reloaded with the
  area; on the PC it is resident, so a second showing of sub-kind 0x59 in one
  session starts faded: `_Fade` sees `0x65EF14` already 0 and moves straight
  on, the ring drawn dark. **For the owner's eye** - it changes what is drawn
  the second time; ours does as the original (no entry from the group).
- **Unchecked indexes** (D200): the seven dispatchers do not bound `+2`; sub-kind
  0x51's variant `+0x36` indexes five rectangles unchecked (its `_Wait` bounds
  its own switch); `DrawRing`'s half indexes two uv pairs (callers 0 / 1);
  `SetTiles`' `step % 3 + 3 bank` six stack bytes (callers keep it 0..5; a
  negative step reads the frame); `DrawPart`'s part three ranges (callers
  0..2); the sky's tints sixteen triples by `(w - 0x390) / 3` - past 0x3BF the
  original reads the twinkle offsets, which `Area189_StepArrive`'s wrap at
  0x3C0 keeps from happening. Ours aborts past any.
- **A divide by zero** (D207): `DrawRing`'s glows divide by `Camera_Distance +
  0x1194`, zero at `Camera_Distance` -0x1194 (an integer divide fault in the
  original; ours aborts with a message). Whether play reaches that distance is
  not measured; no recorded route shows sub-kind 0x59.
- **The draw-item indexes** (D200): `MapView_ItemAt` answers up to 0xFFF and the
  link word `& 0xFFF` likewise; the original indexes `DrawItems` unchecked
  (1,024 items, 2,048 under DIV-0062). Ours aborts past `draw_pool::Count()`.
- **The sky's bands past a full pool** (D209): the four G4 bands are laid at the
  cursor + `0x44 m` without reading it again; when the commit's room test
  fails (the pool nearly full) they are written past the cursor and up to
  0x110 bytes past the pool's room margin, unlinked. Harmless unless the pool
  is that full; ours writes the same bytes.
- **Stale writes** (D213): `DrawStars` stores each star's y into the cursor's prim
  before testing it, drawn or not; sub-kind 0x45's ring overwrites
  `MapView_ScreenXY` and `0x903828` (the camera cells other code reads) with
  its fixed point every call.
- **Never ends by its own code** (D202): sub-kind 0x53 (one state, no release);
  sub-kind 0x5A's cycles; sub-kind 0x44's states (E6B's dispatcher's).

## 8. Calls across groups

**Outbound**: none raw (`band_rows.py --edges`: no call from E6C into
another group). By name, already ours: every callee in section 4;
`EffectKind18Sub55_States[0]` is magic_s14's `WeretigerFx_Next`, read in place
(swapped for a recorder in the fuzz).

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `0x510C80` (`call` at `0x510C80`, `jmp` at `0x510C85`) | E6B (wave six) | `EffectKind18Sub44_Follow` `0x510C90`, `EffectKind18Sub44_Draw` `0x510EB0` - E6B's files, raw until this merges (E6B merges after E6C) |
| `Area189_LeaderStart` `0x42A8D0`, `Area189_StepBegin` `0x42ADB0` | ours (`area_w4e`) | `AreaMap_CornerHeight` `0x511C10` through `kHeightAt` - rebound (section 10) |
| `EffectKind18_Run` through `EffectKind18_States` `[0x45]`, `[0x51]`, `[0x53]`, `[0x55]`, `[0x59]`, `[0x5A]`, `[0x5B]`, `[0x66]` | ours | the dispatchers and the two one-state sub-kinds, read in place |

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 49 band rows, and no first-call trace under
`analysis/calltrace` names any of the 50 (a grep of every file: the hits are
the extent lists `entries*.txt` and dll addresses in three `bof3x.log`s that
happen to share the low digits). **Fuzz only.** No live run was made (the
brief). Sub-kind 0x44 reads area 189's frame word and `AreaMap_CornerHeight`
is area 189's: a recorded walk of that area (whose effect list would have to
spawn sub-kind 0x44) would let the coordinator's frame-hash A/B cover them.

## 10. The rebinding

`grep -rn -i` of the 50 addresses and the six tables in `src/game`
(`band_rows.py --refs`: 7 references, all to `0x511C10`, all `area_w4e`'s):

- **Rebound**: `area_w4e_callees.h`'s `kHeightAt` now reads
  `bof3::addr::AreaMap_CornerHeight` (the value unchanged, so the area
  harness's stand-in keyed on it stands; the line alone changed).
- **Left raw, and why**: `area_w4e_fuzz.cpp`'s two `CallSite` targets
  `0x511C10` (what the clones' call sites hold, checked against the
  disassembly - every group's call-site tables are raw) and its stand-in row
  `HeightAt_511C10` (keyed on `kHeightAt`); the comments in `area_w4e.cpp` /
  `_callees.h` / `_fuzz.cpp` that name the address.
- **For the coordinator**: E6B's raw call and jump to `0x510C90` / `0x510EB0`
  (E6B's files, this wave).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 44 lines, the read extents
of the 50 (six already there exactly). The host lines `00510C90 90B`,
`00511D50 2EE`, `00512040 914`, `00512960 3C2`, `00513410 6B6`, `00513C70
444` and `005140C0 421` (which runs on into E6D's band past `0x51426B`) are
left in place beside the smaller extents (`215`, `7A`, `303`, `1C0`, `5B`,
`53`, `1AC`) for the consolidation to cut.
