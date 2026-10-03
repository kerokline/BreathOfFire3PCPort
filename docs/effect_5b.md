# Group E5B: effect kind 0x18's sub-kinds 0x0B..0x0F, 0x13 and 0x4F

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..15),
wave five, from the round branch's tip `0834edf`. **50 functions ours**
(`src/game/effect_5b.cpp`, shadow name `effect_5b`): the cut table's 48 rows
for E5B (`analysis/round13_cut.tsv`, the band `0x4FF320..0x501430`) and two
starts no list of the cut has - sub-kind 0x13's states 1 `0x5003A0` and 3
`0x5004A0` (section 5). Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
200,000 rounds, 0 mismatches; 149 of 149 controls refused by a count
(section 6). **Fuzz only**: no
recorded route enters any of the 50 (section 9).

Every row is effect code, and every row is `evidence` tier: the seven
dispatchers are `EffectKind18_States` entries 11..15, 19 and 79 (kind 0x18's
`EffectKind18_Run` jumps through that table by `+1`), each a sub-kind with its
own table by `+2` - E2G's `EffectKind18Sub20_*` is the naming model. No PSX
twin of any row is in `analysis/pairs_propagated.json`; the names are the
code's.

| Sub-kind | Dispatcher | What it is, from the code |
|---|---|---|
| 0x0D | `EffectKind18Sub0D_Run` `0x4FF320` (`[13]`) | four VRAM columns animated by `Gpu_SetDrawMove` frames and a glow over a fixed cell; column 0 lit by story flag 0xD, the others by area 48's count `0x92BEE7` (area 48's handlers bump that count with the same sound 0x20D, [`area_w1c.md`](area_w1c.md)) |
| 0x0B | `EffectKind18Sub0B_Run` `0x4FF970` (`[11]`) | eighteen ground tiles at fixed cells, raised with story flag 0x11 and faded out; it leaves the field hook byte `0x9039F4` = 4 |
| 0x0C | `EffectKind18Sub0C_Run` `0x4FFBB0` (`[12]`) | sub-kind 0x0D's columns again at another VRAM x, each lit by its own story flag (`0x65DFC4`), no glow |
| 0x0E | `EffectKind18Sub0E_Run` `0x4FFE40` (`[14]`) | a gate at cells (1, 21)..(1, 22), closed by row 9's flag 0xC, lowered and raised as the leader comes and goes or `Cond_ByteFE` says |
| 0x13 | `EffectKind18Sub13_Run` `0x500300` (`[19]`) | a wall panel of four variants whose face is chosen by row 9's flags, shown, hidden, flashed, with a flickering line |
| 0x0F | `EffectKind18Sub0F_Run` `0x500930` (`[15]`) | gates of eight variants (cells and heights from `0x65E058` / `0x65E038`), their two cells `0x52` (passable?) or `0xA1` in `AreaMap_Bytes`, opened by `Cond_ByteFE` against the record's `+0xA` or by the leader |
| 0x4F | `EffectKind18Sub4F_Run` `0x5011A0` (`[79]`) | the same gates opened by `Cond_ByteFE` only; its table shares sub-kind 0x0F's `_Lower` and `_Raise` |

What a gate's cell values mean to the map is not read here (`0x52` and `0xA1`
are written, never tested by this code); which areas spawn these sub-kinds is
not established (no area group's source stores these sub-kind numbers into
`+0xB` of an effect record by code - they come from the areas' data); what
the effects look like in play is the owner's to say.

## 1. What each function does

Every state runs with `Sprite_Current` an `Effect_Objects` record (+5 0x18,
+1 the sub-kind, +2 the sub-state). Words: `+0x36` / `+0x3A` the cell or the
variant, `+0x34` / `+0x38` the point (16.16), `+0x30` a gate's lowering, `+9`
a frame count, `+0xA` the `Cond_ByteFE` value that opens a gate, `+0xB` a flag
index or variant, `+8` a gate's axis.

### 1.1 Sub-kind 0x0D (`EffectKind18Sub0D_States` `0x65DF4C`, 6)

| PC | Name | What |
|---|---|---|
| `0x4FF320` | `_Run` | `jmp [_States + +2 * 4]`, unbounded |
| `0x4FF340` | `_Start` | +9 0; column 0 with story flag 0xD: frame 3, +2 3; else the column's frame 0, +2 1 |
| `0x4FF3A0` | `_Wait` | column 0 and flag 0xD, or another column and the count not 0: sound 0x20D, +9 0, +2 up |
| `0x4FF400` | `_Open` | frame `0x65DF64[+9 >> 1]`; +9 up, past 4: +2 up; the glow |
| `0x4FF460` | `_Idle` | the condition gone: +9 0, +2 up; +9 up, past 0x40: +9 0, odd `Rand`: +2 5; the glow |
| `0x4FF4F0` | `_Shut` | frame `0x65DF68[+9 / 3]`; past 3: +2 1 |
| `0x4FF540` | `_Flash` | frame `0x65DF6C[+9 >> 1]`; past 6: +9 0, +2 3; the glow |
| `0x4FF5A0` | `_MoveFrame(column, frame)` | `Gpu_SetDrawMove` of the 8 x 0x78 rect at ((frame + 1) * 8 + column * 64 + 0x240, 0x160) onto (column * 64 + 0x240, 0x160), slot 6 |
| `0x4FF610` | `_DrawGlow(column)` | the point (`0x65DF70` / `74` cells, `-0x65DF78 * 8` high) through `Gte_RotTransPers`; culled outside -60..380 x -150..300 or at depth 0; a diamond of half-width `0x65DF7C * 1125 / depth` across and `(|sin(Camera_Angles) * s >> 13| + s) * 1125 / depth` up and down; four semi-transparent `POLY_G3` fans, centre colour from column 0's 0x50 and `(count + 1) * column << 4`, rims black; draw modes (tpage 0xB5) before and after |

`_DrawGlow` is x87: the screen point's `_ftol`s, `fild` / `fadd` and `fild` /
`fsubr` of the half-widths, the depth float copied by `fld` / `fst` / `fstp`
(a signalling NaN comes out quiet) and the screen point by `mov`. Ours does
the same operations in inline assembly (`FldFtol`, `FiAddFtol`, `FldFiSubFtol`,
`FpuCopy2`): one rounding each, the CRT's truncating `_ftol`. Its cull reads
the four `.rdata` floats Capcom's reads (`0x5C4210` -60, `0x5C420C` 380,
`0x5C4200` -150, `0x5C41FC` 300) - the fcomp operand at `0x4FF6A3` is one of
the sprite culls [`widescreen.md`](widescreen.md) section 3b lists; it stays
Capcom's width (section 8).

### 1.2 Sub-kind 0x0B (`EffectKind18Sub0B_States` `0x65DF80`, 3)

| PC | Name | What |
|---|---|---|
| `0x4FF970` | `_Run` | the dispatcher |
| `0x4FF990` | `_Start` | story flag 0x11: sound 0x202, +0x3C -0x180, +0xB 0x80, +9 0, +2 up, tiles at -0x180 |
| `0x4FF9F0` | `_Rise` | tiles at +0x3C (colour 0x80); +0x3C + 4; above -0x100: +2 up |
| `0x4FFA30` | `_Fade` | tiles at +0x3C, colour `(+0xB & 0xF8) << 16`; +0x3C + 4, +0xB - 4; above -0x80: `0x9039F4` = 4, tail `Effect_Release` |
| `0x4FFA90` | `_DrawTiles(height, colour)` | eighteen `POLY_FT4` cell squares from the triples (x, z, dy) at `0x65DF8C`, y the height's low word, `Prim_SetTexture(colour \| 0xB600B100)`, each linked at its cell with its dy |

### 1.3 Sub-kind 0x0C (`EffectKind18Sub0C_States` `0x65DFCC`, 6)

The same machine as 0x0D without the glow: `_Start` / `_Wait` / `_Idle` test
the column's story flag `0x65DFC4[+0x36]` (sound 0x201); `_Open` / `_Shut` /
`_Flash` take their frames from `0x65DFE4` / `E8` / `EC`; `_MoveFrame(frame)`
`0x4FFDD0` moves the rect at ((frame + 1) * 8 + 0x1C0, 0x160) onto
(`0x65DFF0[+0x36]` + 0x1C0, 0x160).

### 1.4 Sub-kind 0x0E (`EffectKind18Sub0E_States` `0x65DFF8`, 6)

| PC | Name | What |
|---|---|---|
| `0x4FFE40` | `_Run` | the dispatcher |
| `0x4FFE60` | `_Start` | +0x30 0; row 9 flag 0xC clear: cells (1, 21), (1, 22) `0x50`, +2 5; set: the cells `0xA1`, +2 up, the leader within (0x8000, 0x10000) of (0x28000, 0x150000 or 0x160000): +0x30 0xFF00, +2 3; the draw |
| `0x4FFF80` | `_Wait` | `Cond_ByteFE` not 0, or the leader there: sound 0x200 (`Field_Request` 0), +2 2 - each on its own; the draw |
| `0x500020` | `_Lower` | +0x30 - 0x20, at -0x100: +2 up; tail draw |
| `0x500040` | `_Open` | the leader beyond 0x18000: +2 4; `Cond_ByteFE`: +2 3; tail draw |
| `0x5000B0` | `_Raise` | +0x30 + 0x20; below 0: tail draw; else `_Wait` (draws), +2 2: done; else +2 1, sound 0x201, the draw again |
| `0x500100` | `_Closed` | row 9 flag 0xC: the cells `0xA1`, +2 1; tail draw |
| `0x500160` | `_DrawGate` | under `Draw_PassFlags` bit 2: a draw mode committed, a `POLY_FT4` at x -0x3F40 from z +0x30 - 0x34C0 to - 0x35C0, 0x40 and 0x180 above half the ground's elevation (`AreaMap_Elevation`, four calls), `Prim_SetTexture(0x25500123)`, slot 6 |

### 1.5 Sub-kind 0x13 (`EffectKind18Sub13_States` `0x65E018`, 5)

| PC | Name | What |
|---|---|---|
| `0x500300` | `_Run` | the dispatcher |
| `0x500320` | `_Start` | +0xB = the byte +0x36 (the variant); +0x34 / +0x38 the variant's cell (`0x65E02C`); row 9's flag `0x65E010[+0xB]`: +2 up and on into `_Shown`; clear: +2 3, tail `_Hidden` |
| `0x5003A0` | `_Shown` | the flag clear: +9 9, +2 up; the panel with face `+0xB + 4 * flag(0x65E014[+0xB]) + 1` at 0x40; tail `_DrawLine` |
| `0x500420` | `_Fall` | +9 - 1, at 0: +2 up; the bare panel (face 0) and the face panel at `+9 * 8`; tail `_DrawLine` |
| `0x5004A0` | `_Hidden` | the flag set: +9 9, +2 up; the bare panel |
| `0x5004F0` | `_Rise` | +9 - 1, at 0: +2 1; a draw mode linked; the face panel; a grey `POLY_F4` (+9 * 0x1E) over the panel's vertices; `_DrawLine` |
| `0x500610` | `_DrawPanel(texture, height)` | a standing `POLY_FT4` at x +0x36 * 0x80 - 0x3FC0 between z +0x3A * 0x80 + height - 0x4008 and - height - 0x3FF8, heads 0x48 - height and feet height + 0x40 off half the elevation, `Prim_SetTexture(texture + 0x275000F3)`, linked at the point |
| `0x5007B0` | `_DrawLine` | a `LINE_F2` grey 0x80 on odd frames between `Prim_VertexScratch`'s first two vertices, lifted `2 * (Frame_Counter % 48) + 4`; `Gte_RotTransPers3` (its third vertex into `MapView_ScreenXY`), `Gte_StoreDepthF3`; draw modes around it |

### 1.6 Sub-kinds 0x0F (`0x65E068`, 6) and 0x4F (`0x65E080`, 5): the gates

| PC | Name | What |
|---|---|---|
| `0x500930` | `EffectKind18Sub0F_Run` | the dispatcher |
| `0x500950` | `EffectKind18Sub0F_Start` | from the variant v = +0x36: +0xA = byte +0x3A + 1, +0xB = `0x65E034[+0x3A]`, the cell `0x65E058[v]`, +8 = (v == 0), +0x3E / +0x32 / +0x2E from `0x65E038[v]` / `0x65E048[v]` and the elevation, +0x30 0; row 9 flag +0xB clear: the two cells `0x52`, +2 5; set: `0xA1`, +2 up, the leader at the gate: +0x30 0xFF00, +2 3; drawn flat |
| `0x500BD0` | `EffectKind18Sub0F_Wait` | `Cond_ByteFD` 0 and `Cond_ByteFE == +0xA`, or the leader at the gate: sound 0x200, +2 2, each on its own; drawn on the ground |
| `0x500CF0` | `EffectKind18Sub0F_Lower` | +0x30 - 0x20, at -0x100: +2 up; drawn flat (both tables' entry 2) |
| `0x500D20` | `EffectKind18Sub0F_Open` | unless `Cond_ByteFD` 0 and `Cond_ByteFE == +0xA`: the leader two cells off: +2 4. **No draw** |
| `0x500E00` | `EffectKind18Sub0F_Raise` | +0x30 + 0x20; below 0: drawn flat; else `_Wait`, +2 2: done; else sound 0x201, +2 1, drawn flat (both tables' entry 4) |
| `0x500E50` | `EffectKind18Sub0F_Closed` | row 9 flag +0xB: the cells `0xA1`, +2 1; drawn on the ground |
| `0x500EF0` | `EffectKind18Sub0F_DrawGate(ground)` | a standing `POLY_FT4` over the cell, along x with +8 set and along z without, lowered by +0x30; feet `-(elevation / 2) - +0x3E` with ground, else +0x32, heads 0x140 above; `Prim_SetTexture((+8 * 41) << 16 \| (s16) +0x2E \| 0x26500000)` |
| `0x5011A0` | `EffectKind18Sub4F_Run` | the dispatcher |
| `0x5011C0` | `EffectKind18Sub4F_Start` | `_Start`'s set-up but +0xA = 8 - v; cells (x, z), (x, z + 1) `0x52`, +2 up; the leader there (always the along-z test): `0xA1`, +0x30 0xFF00, +2 3. No flag test |
| `0x5013A0` | `EffectKind18Sub4F_Wait` | `Cond_ByteFE == +0xA`: sound 0x200, the cells `0xA1`, +2 2 |
| `0x501430` | `EffectKind18Sub4F_Open` | unless `Cond_ByteFE == +0xA`: the leader two cells off: the cells `0x52`, +2 4 |

"The leader at the gate" (`GateNear` in ours): with +8 set, the leader's z
within `across` of the centre of the row after the gate's
(`((z + 1) << 16) | 0x8000`) and its x within `along` of the cell's corner or
the next; with +8 clear the axes swap. `_Start` / `_Wait` use (0x8000,
0x10000), `_Open` (0x20000, 0x20000); every distance is the original's
`cdq / xor / sub` absolute value compared signed.

## 2. Divergence

None. Every function is a faithful replacement; no `DIVERGENCE.md` entry.
Where the original jumps through a sub-state table past its end, or reads one
of its byte arrays past the array's extent by a byte of the record, ours
aborts with a message (the round-nine rule; section 7).

## 3. The tables and the arguments

**The sub-state tables** (`symbols.toml` `[[data]]`), each the table's own
length to the next table a dispatcher indexes or to the first dword that is
not code, checked by hand against what the states store into `+2`:

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind18Sub0D_States` `0x65DF4C` | 6 | bytes (`0x65DF64`) |
| `EffectKind18Sub0B_States` `0x65DF80` | 3 | the tile triples `0x65DF8C` (the tool said 3) |
| `EffectKind18Sub0C_States` `0x65DFCC` | 6 | bytes (`0x65DFE4`) |
| `EffectKind18Sub0E_States` `0x65DFF8` | 6 | bytes (`0x65E010`) |
| `EffectKind18Sub13_States` `0x65E018` | 5 | bytes (`0x65E02C`); the tool said 5 but listed four states - entry 1 `0x5003A0` sat inside its extent of `_Start` |
| `EffectKind18Sub0F_States` `0x65E068` | 6 | `0x65E080`, sub-kind 0x4F's (the tool's run of eleven counted it in) |
| `EffectKind18Sub4F_States` `0x65E080` | 5 | a data dword `0x65E094` |

**The byte arrays** are read in place by a record byte (`effect_5b_callees.h`
names each): back to back, so each one's extent is to the next array -
`0x65DF64` / `68` / `6C` (frames, 4 each), `0x65DF70` / `74` / `78` / `7C` (the
glow's x, z, y, size by column, 4 each), the tile triples `0x65DF8C..0x65DFC1`
(18), `0x65DFC4` (8 flags), `0x65DFE4` / `E8` / `EC` (4 each), `0x65DFF0` (8
columns), `0x65E010` / `14` (4 each), `0x65E02C` (4 pairs), `0x65E034` (4),
`0x65E038` / `48` (8 words each), `0x65E058` (8 pairs). None is copied into
the source.

**Arguments**: `_DrawTiles` pushes each tile's dy for `MapView_LinkPrimAt` as
`mov dl, byte` over whatever `Prim_SetTexture` left in `edx`; the callee reads
a signed byte (symbols.toml), so the fuzz re-lists it with byte masks for dy
and size (section 4). `_DrawTiles`' own height argument is read as `bp` (its
low word): its row masks it to 16 bits. `Flags_Test`'s index is pushed as a
register whose upper bytes are a pointer's (the standard row's `kU8` is what
it reads). `_Shown`, `_Fall` and `_Rise` push `_DrawPanel`'s second argument
before `Flags_Test`'s two and leave it on the stack across the test. Every
other argument is a whole value.

**The extra pushes**: `_DrawGlow` hands `Gte_RotTransPers` a fourth pointer,
`_DrawTiles` / `_DrawGate`s / `_DrawPanel` / `_Rise` hand `Gte_RotTransPers4`
a tenth and `_DrawLine` hands `Gte_RotTransPers3` an eighth - all beyond the
declared parameters (symbols.toml), unread by the callees; ours passes the
declared ones.

## 4. The fuzz (`effect_5b_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_5b`, effect mode (`g.effect`, kind 0x18),
4,000 rounds a function (`BOF3X_E5B_ROUNDS`), `BOF3X_E5B_ONLY=<name>` for one.
The clone table is `band_rows.py --clones`, every extent checked against the
disassembly; `_Start` of sub-kind 0x13 cut at `0x5003A0` (its `jmp` there at
+0x67 a call site), `_Shown` and `_Hidden` clones of their own. Shapes: the
seven dispatchers kEffect with `sub_span` their table's count (the tables
swapped for recorders on both sides), the 37 states and the two void draws
kEffect, the six helpers with arguments kCall.

**Callees**: the group's own twelve called directly, by name - `_MoveFrame`
(2) and `_DrawGlow`, `_DrawTiles` (the height at 16 bits), `_DrawPanel`,
`EffectKind18Sub0F_DrawGate` as kGarbage with their arguments; `_Wait` (0x0E
and 0x0F), `_DrawGate` (0x0E), `_Shown`, `_Hidden`, `_DrawLine` as kPhase.
**Re-listed**: `Gte_RotTransPers` (`FxProjectGlow`: the glow's screen point
about the cull's four bounds with fractions, at and a quarter past each bound,
one time in sixteen a NaN or an infinity; the depth answer 0, small either
sign, middling or garbage - the effect-standard row fills whole floats and a
garbage depth that rounds every half-width to 0); `Gte_StoreDepthF`
(`FxDepth`: fractional depths and, one time in eight, a NaN or an infinity,
so the FPU copy's quietening shows); `MapView_LinkPrimAt`
(`FxLink`: the effect row's cursor move, the dy and size compared as bytes,
section 3). Everything else is the standard and effect-standard rows:
`Flags_Test` kBool, `Rand` kRand, `AreaMap_Elevation` garbage (only its low
word is read), `Effect_Release` clearing bytes 0..4, the draw primitives,
`Math_Sin`, `_ftol` called for real on both sides (kThrough).

**Regions** beyond effect mode's: area 48's count `0x92BEE7` (1) and the tail
state byte `0x9039F4` (4). Everything else read is standard: the effect
records, `Cond_Flags` (the story flags `0x904030`, row 9 `0x903FD8`), the
leader `ObjTrio`, `Camera_Angles`, `Draw_PassFlags`, `Field_Request`,
`Cond_ByteFD`, `Cond_ByteFE`, `Frame_Counter`, `Prim_VertexScratch`,
`MapView_ScreenXY`, the area block and `AreaMap_Bytes`.

**Seeds** (`Seed(k)`, on all 20 records, since the disturbance moves
`Sprite_Current` among them): sub-kind 0x0D's column `+0x36` below 4 (its glow
reads four arrays by it), 0x0C's below 8, a gate start's variant below 8 and
flag index `+0x3A` below 4, sub-kind 0x13's `+0x36` / `+0xB` below 4, the
other records' cell words below 0x48 / 0x1D (the cell writes stay inside the
area block, the map width `AreaMap_Header[0]` drawn at most 0x40); `+9` below
8 for the frame states and at 0x3F..0x41 for `_Idle`; `+8` 0 or 1; `+0x30` at
0xFF00, 0xFF1F..0xFF21, 0xFFDF, 0xFFE0, 0, 0x20, 0x7FFF; `+0x3C` at -0x180,
-0x104..-0x100, -0x84..-0x80; the leader about the record's gate (both axes)
or sub-kind 0x0E's fixed gate at offsets 0, ±0x8000, 0x10000, 0x20000,
0x30000 and one past each; `Field_Request`, `Cond_ByteFD`, the count 0 a
third of the time, `Cond_ByteFE` 0 or the record's `+0xA` a third of the time;
`Draw_PassFlags` bit 2 two times in three. **Arguments** (`Args`): the glow's
column below 4; `_MoveFrame`'s column small or wild (it only shifts), frames
0..4 or a byte; the tiles' height at the -0x180 / -0x100 / -0x80 marks; the
panel's face and height; the gate's ground 0, 1 or garbage.

**Disturbance** (from its hash only): `+9` below 8, `+0x30` 0xFF00 or a word,
`+0xB` below 4, `Cond_ByteFE` the record's `+0xA` or a byte, the count 0 or a
byte, the leader's x moved by ±0x8000.

**Result** (this worktree, `effect_5b_st1.log` in the scratch):
200,000 rounds over 50 functions, 944,140 calls to the stand-ins, **0
mismatches**; 24,761 bytes of state in 47 regions (the final fuzz,
`st2.log`). Every table entry reached (each sub-state's recorder 625..1,539
times); `Gte_RotTransPers` 4,000, of which the glow drew past the cull 1,970
times (`Math_Sin` 3,940 = two a glow); `Effect_Release` 749; `Rand` 2,025.

## 5. What the cut and the tool said, settled

- **`0x5003A0` is a function** (sub-kind 0x13's state 1): table entry
  `0x65E01C` names it and `_Start`'s `jmp` at `0x500387` enters it. The tool
  counted it into `0x500320`'s extent (0xF7); `_Start` is 0x80 here, `_Shown`
  0x77. Added.
- **`0x5004A0` is a function** (state 3): table entry `0x65E024` and
  `_Start`'s `jmp` at `0x500396`. The cut's 208 bytes for `0x500420` ran over
  it ("code no list has", the tool's flag); `0x500420` is 0x71. Added.
- **No start is a case or a shared tail.** The `jmp`s into `_DrawGate`
  (`0x500160`) and `_DrawLine` (`0x5007B0`) reach functions with their own
  frame and `ret`, called as well - taken whole, each its own clone.
- **The tool's "reached by" cells for `0x500020`, `0x500040`, `0x5000B0`,
  `0x500100` and `0x500300`** (23, 25, 6, 1 and 1 cells in `0x5DC40C..0x668250`
  beyond their table entries) are data dwords whose value happens to read as
  the address (pairs of halfwords such as (0x20, 0x50)); none sits in a table
  any code indexes. Each function is reached by its sub-state table entry.
- **Extents**: the tool's agree with the code for all 48; the cut's sizes are
  the catalog's guesses with padding (35 differ by padding only).
- **Hosts**: the cut names E5A's `0x4FF150` as the host of the first seven
  hidden rows; nothing of ours contains their code (E5A's function ends before
  `0x4FF320`), so each is taken as its own function.

## 6. Controls

Planted in `effect_5b.cpp` one at a time by a script (scratch `controls.py`:
a unique anchor replaced, rebuild, run under `BOF3X_E5B_ONLY=<filter>`,
restore, rebuild; never with a commit in between). Counts are rounds refused
(the round number of the first mismatch is what the Fatal names; the run
stops there) of 4,000 per function run, in this worktree; every refused run
exited 3. Each dispatcher swapped onto another table of at least its length.

**149 planted, 149 refused by a count; no equivalent mutant.** The first run
refused 144; the five it did not were the fuzz's fault and are refused since
(the counts below are the second run's):

- 26 (`_DrawGlow` culling depth 1 too) and 28 (the depth float copied by `mov`
  instead of through the FPU): the projection's stand-in answered depth ±1 too
  rarely, and the effect row of `Gte_StoreDepthF` writes no signalling NaN.
  `FxProjectGlow` now answers ±1 a sixth of the time; `Gte_StoreDepthF` is
  re-listed (`FxDepth`: fractions, one time in eight a NaN, signalling or
  quiet, or an infinity).
- 77 (sub-kind 0x0E's `_Open` at 0x17FFF): the leader's offsets had no
  0x18000 boundary; added (±0x18000 and one past).
- 115, 118 (a gate start's near test narrowed): the start replaces the
  record's words by the variant's cell before testing, so the leader seeded
  about the old words was never near; for the two starts the leader is now
  seeded about `_Cells[v]`.

The other 144 ran under the first fuzz; the change only adds stand-in
behaviour and seeds, and the full self-test after it is 0 mismatches.

| # | Run (`_ONLY`) | Plant | Refused (rounds of 4,000) |
|--:|---|---|---|
| 1 | `Sub0D_Run` | 0x0C table | 4000 |
| 2 | `Sub0D_Start` | frame 2 | 1031 |
| 3 | `Sub0D_Start` | +2 2 | 2969 |
| 4 | `Sub0D_Start` | flag 0xE | 1545 |
| 5 | `Sub0D_Wait` | sound 0x20C | 2664 |
| 6 | `Sub0D_Wait` | count != 1 | 840 |
| 7 | `Sub0D_Open` | flash frames | 4000 |
| 8 | `Sub0D_Open` | past 5 | 523 |
| 9 | `Sub0D_Open` | column not read again | 110 |
| 10 | `Sub0D_Idle` | >= 0x40 | 358 |
| 11 | `Sub0D_Idle` | Rand & 2 | 447 |
| 12 | `Sub0D_Idle` | count test reversed | 2455 |
| 13 | `Sub0D_Idle` | flag clear kept | 514 |
| 14 | `Sub0D_Shut` | / 2 | 495 |
| 15 | `Sub0D_Shut` | +2 2 | 2474 |
| 16 | `Sub0D_Flash` | +9 kept | 957 |
| 17 | `Sub0D_Flash` | open frames | 4000 |
| 18 | `Sub0D_MoveFrame` | to + 8 | 4000 |
| 19 | `Sub0D_MoveFrame` | frame + 2 | 4000 |
| 20 | `Sub0D_MoveFrame` | h 0x70 | 4000 |
| 21 | `Sub0D_DrawGlow` | y << 2 | 2021 |
| 22 | `Sub0D_DrawGlow` | 1124 | 88 |
| 23 | `Sub0D_DrawGlow` | across + 1 | 1993 |
| 24 | `Sub0D_DrawGlow` | >= right | 166 |
| 25 | `Sub0D_DrawGlow` | NaN kept at the top | 74 |
| 26 | `Sub0D_DrawGlow` | depth 1 culled | 221 (first run: not refused - the fuzz's fault, section 6's note) |
| 27 | `Sub0D_DrawGlow` | centre 0x51 | 529 |
| 28 | `Sub0D_DrawGlow` | depth copied by mov | 355 (first run: not refused - the fuzz's fault, section 6's note) |
| 29 | `Sub0D_DrawGlow` | x for y | 1988 |
| 30 | `Sub0D_DrawGlow` | green + 2 | 1464 |
| 31 | `Sub0D_DrawGlow` | size 0x30 | 1993 |
| 32 | `Sub0D_DrawGlow` | up added | 777 |
| 33 | `Sub0D_DrawGlow` | >> 12 | 788 |
| 34 | `Sub0B_Run` | 0x0E table | 4000 |
| 35 | `Sub0B_Start` | flag 0x12 | 4000 |
| 36 | `Sub0B_Start` | -0x17F | 2643 |
| 37 | `Sub0B_Start` | +0xB 0x81 | 2618 |
| 38 | `Sub0B_Start` | drawn at -0x181 | 2643 |
| 39 | `Sub0B_Rise` | >= -0x100 | 495 |
| 40 | `Sub0B_Rise` | colour 0x81 | 4000 |
| 41 | `Sub0B_Fade` | & 0xFC | 2006 |
| 42 | `Sub0B_Fade` | tail state 5 | 737 |
| 43 | `Sub0B_Fade` | < -0x80 | 524 |
| 44 | `Sub0B_Fade` | +0xB - 3 | 3994 |
| 45 | `Sub0B_DrawTiles` | texture + 1 | 3571 |
| 46 | `Sub0B_DrawTiles` | z - 0x40C1 | 4000 |
| 47 | `Sub0B_DrawTiles` | dy + 1 | 4000 |
| 48 | `Sub0B_DrawTiles` | y + 1 | 4000 |
| 49 | `Sub0C_Run` | 0x0D table | 4000 |
| 50 | `Sub0C_Start` | frame 2 | 2702 |
| 51 | `Sub0C_Start` | next flag | 3184 |
| 52 | `Sub0C_Start` | +2 0 | 1298 |
| 53 | `Sub0C_Wait` | sound 0x202 | 2702 |
| 54 | `Sub0C_Wait` | test reversed | 4000 |
| 55 | `Sub0C_Open` | +9 kept | 1985 |
| 56 | `Sub0C_Open` | flash frames | 4000 |
| 57 | `Sub0C_Idle` | < 0x40 | 377 |
| 58 | `Sub0C_Idle` | +2 4 | 489 |
| 59 | `Sub0C_Shut` | +2 0 | 2474 |
| 60 | `Sub0C_Shut` | open frames | 2464 |
| 61 | `Sub0C_Flash` | +2 2 | 957 |
| 62 | `Sub0C_MoveFrame` | rect + 8 | 4000 |
| 63 | `Sub0C_MoveFrame` | to + 1 | 4000 |
| 64 | `Sub0E_Run` | 0x0F table | 4000 |
| 65 | `Sub0E_Start` | cells 0x51 | 1357 |
| 66 | `Sub0E_Start` | z within 0xFFFF | 37 |
| 67 | `Sub0E_Start` | >= x | 104 |
| 68 | `Sub0E_Start` | 0xFF01 | 150 |
| 69 | `Sub0E_Start` | cell (2, 22) | 4000 |
| 70 | `Sub0E_Wait` | ByteFE != 1 | 652 |
| 71 | `Sub0E_Wait` | sound 0x201 | 75 |
| 72 | `Sub0E_Wait` | z 0x170000 | 26 |
| 73 | `Sub0E_Lower` | < -0x100 | 386 |
| 74 | `Sub0E_Lower` | - 0x21 | 3962 |
| 75 | `Sub0E_Open` | +2 5 | 567 |
| 76 | `Sub0E_Open` | +2 2 | 3326 |
| 77 | `Sub0E_Open` | z within 0x17FFF | 7 (first run: not refused - the fuzz's fault, section 6's note) |
| 78 | `Sub0E_Raise` | <= 0 | 407 |
| 79 | `Sub0E_Raise` | +2 0 | 1414 |
| 80 | `Sub0E_Raise` | sound 0x202 | 469 |
| 81 | `Sub0E_Raise` | record not read again | 61 |
| 82 | `Sub0E_Closed` | +2 2 | 2643 |
| 83 | `Sub0E_Closed` | flag 0xD | 4000 |
| 84 | `Sub0E_DrawGate` | x + 1 | 3335 |
| 85 | `Sub0E_DrawGate` | z - 0x34C1 | 3335 |
| 86 | `Sub0E_DrawGate` | head 0x181 | 3335 |
| 87 | `Sub0E_DrawGate` | texture + 1 | 3335 |
| 88 | `Sub0E_DrawGate` | x 0x18001 | 3335 |
| 89 | `Sub13_Run` | 0x4F table | 4000 |
| 90 | `Sub13_Start` | z << 15 | 4000 |
| 91 | `Sub13_Start` | +2 4 | 1300 |
| 92 | `Sub13_Start` | the high byte | 2973 |
| 93 | `Sub13_Shown` | +9 8 | 1365 |
| 94 | `Sub13_Shown` | face + 2 | 4000 |
| 95 | `Sub13_Fall` | << 2 | 3315 |
| 96 | `Sub13_Fall` | 0x41 | 4000 |
| 97 | `Sub13_Hidden` | face 1 | 4000 |
| 98 | `Sub13_Hidden` | faces flag | 4000 |
| 99 | `Sub13_Rise` | * 0x1F | 3336 |
| 100 | `Sub13_Rise` | +2 2 | 684 |
| 101 | `Sub13_Rise` | size 0x3C | 4000 |
| 102 | `Sub13_DrawPanel` | texture + 1 | 4000 |
| 103 | `Sub13_DrawPanel` | head 0x49 | 4000 |
| 104 | `Sub13_DrawPanel` | + h | 3227 |
| 105 | `Sub13_DrawPanel` | e1 for e2 | 4000 |
| 106 | `Sub13_DrawLine` | << 6 | 2292 |
| 107 | `Sub13_DrawLine` | % 0x31 | 3911 |
| 108 | `Sub13_DrawLine` | size 0x24 | 4000 |
| 109 | `Sub13_DrawLine` | dtd 0 | 4000 |
| 110 | `Sub0F_Run` | 0x0E table | 4000 |
| 111 | `Sub0F_Start` | +0xA + 2 | 4000 |
| 112 | `Sub0F_Start` | v == 1 | 2577 |
| 113 | `Sub0F_Start` | + half | 3999 |
| 114 | `Sub0F_Start` | cells 0x53 | 1355 |
| 115 | `Sub0F_Start` | along 0x8000 | 33 (first run: not refused - the fuzz's fault, section 6's note) |
| 116 | `Sub0F_Start` | heights for textures | 3085 |
| 117 | `Sub0F_Start` | z + 2 | 2233 |
| 118 | `Sub0F_Start` | row | 0x7FFF | 13 (first run: not refused - the fuzz's fault, section 6's note) |
| 119 | `Sub0F_Wait` | column | 0x8001 | 50 |
| 120 | `Sub0F_Wait` | second cell 0x20000 | 50 |
| 121 | `Sub0F_Wait` | +0xB | 215 |
| 122 | `Sub0F_Wait` | ground 2 | 4000 |
| 123 | `Sub0F_Lower` | <= -0xFF | 376 |
| 124 | `Sub0F_Open` | +2 3 | 2835 |
| 125 | `Sub0F_Open` | ByteFD 1 | 167 |
| 126 | `Sub0F_Open` | along 0x1FFFF | 86 |
| 127 | `Sub0F_Raise` | +2 2 | 1414 |
| 128 | `Sub0F_Raise` | +2 before the sound | 25 |
| 129 | `Sub0F_Raise` | + 0x1F | 3953 |
| 130 | `Sub0F_Closed` | +2 0 | 2643 |
| 131 | `Sub0F_DrawGate` | x - 0x3F41 | 2038 |
| 132 | `Sub0F_DrawGate` | z - 0x4041 | 1271 |
| 133 | `Sub0F_DrawGate` | * 40 | 995 |
| 134 | `Sub0F_DrawGate` | +0x2E not sign-extended | 1680 |
| 135 | `Sub0F_DrawGate` | head 0x141 | 2198 |
| 136 | `Sub0F_DrawGate` | flat head 0x13F | 1111 |
| 137 | `Sub0F_DrawGate` | halved toward minus infinity | 1471 |
| 138 | `Sub4F_Run` | 0x13 table | 4000 |
| 139 | `Sub4F_Start` | 9 - v | 4000 |
| 140 | `Sub4F_Start` | cells 0x53 | 3997 |
| 141 | `Sub4F_Start` | the other axis | 7 |
| 142 | `Sub4F_Start` | z + 2 | 3 |
| 143 | `Sub4F_Wait` | != | 4000 |
| 144 | `Sub4F_Wait` | +2 3 | 685 |
| 145 | `Sub4F_Wait` | second cell (x, z) | 680 |
| 146 | `Sub4F_Open` | along 0x1FFFF | 78 |
| 147 | `Sub4F_Open` | cell 0x51 | 2459 |
| 148 | `Sub4F_Open` | +2 5 | 2471 |
| 149 | `Sub4F_Open` | +0xB | 530 |

## 7. Latent defects (Capcom's, described, not fixed)

- **The seven dispatchers do not bound `+2`**: a sub-state past the table
  jumps through the dword after it - the next table, bytes or a data dword.
  Ours aborts with a message. Every state stores only in-table values.
- **Byte arrays read by a record byte, unchecked**: sub-kind 0x0D's glow by
  the column `+0x36` (four arrays of 4), its frames by `+9 >> 1` and `+9 / 3`
  (4 each; `+9` stays below 8 in play), sub-kind 0x0C's flag by `+0x36` (8)
  and its VRAM column by `+0x36` (8), sub-kind 0x13's by `+0xB` (the byte
  `+0x36` copied, 4), the gates' by the variant `+0x36` (8) and `+0x3A` (4).
  Past an extent the original reads the next array; ours aborts. Ordinary
  play reaches none of these unless a spawner writes an out-of-range variant.
- **Cell writes unchecked**: the gates write `AreaMap_Bytes + z * width + x`
  (and the neighbour) from the record's words, with no bound against the
  map; sub-kind 0x0E writes the fixed cells (1, 21) and (1, 22) whatever the
  map's size. Faithful in ours (no bound either).
- **Sub-kind 0x4F's variant 0 crosses its axes**: `_Start` sets +8 = (v ==
  0) as 0x0F's does, so variant 0's gate is drawn along x, but 0x4F's cell
  writes ((x, z), (x, z + 1)) and its leader tests always take the along-z
  shape. A variant-0 gate of this sub-kind would mark and test cells across
  the quad it draws. Whether any area spawns 0x4F with variant 0 is not known.
- **Two draws in one frame**: sub-kind 0x0E's `_Raise` and the gates' `_Raise`
  call `_Wait` (which draws the gate) and, when it did not re-trigger, draw
  it again - two identical quads committed that frame.
- **The gates' `_Open` draws nothing** (0x0F state 3): while a gate stands
  open its quad is not drawn at all (it is drawn lowered by +0x30 in `_Lower`
  and `_Raise`). By design or not is the owner's to see.
- **`_DrawGlow` divides by the projection's depth** after testing it for 0:
  safe.

## 8. Calls across groups

**Outbound, raw**: none (`band_rows.py --edges`: 0 edges). Every callee is
ours already and called by name: `Flags_Test`, `Sound_PlayEffect`, `Rand`,
`Effect_Release`, `AreaMap_Elevation`, `Math_Sin`, `Gfx_CommitPrim`,
`MapView_LinkPrimAt`, `Prim_SetTexture`, `Gpu_SetDrawMove`, `Gpu_SetDrawMode`,
`Gpu_SetPolyFT4` / `F4` / `G3`, `Gpu_SetLineF2`, `Gpu_SetSemiTrans`,
`Gpu_SetShadeTex`, `Gte_RotTransPers` / `3` / `4`, `Gte_PrimDepths4_10` /
`4_0C`, `Gte_StoreDepthF` / `F3`; the CRT's `_ftol` inline.

**Inbound from outside the group**: `EffectKind18_Run` (`0x46D830`, ours)
reaches the seven dispatchers through `EffectKind18_States` entries 11..15, 19
and 79 (read in place). No code of ours or Capcom's calls any of the 50
directly. `scenario_harness_ekh.cpp` copies `0x500D20` (`EffectKind18Sub0F_Open`)
by address as its kind-0x18 sub-state proof (section 10).

**Widescreen** (DIV-0041): `_DrawGlow`'s cull at `0x4FF6A3` reads `[-60,
380]` from `.rdata` as Capcom's does - [`widescreen.md`](widescreen.md) section
3b lists it among the sprite culls with 7 px of margin. Ours reads the same
floats; widening it (as `MapCell_DrawAnimated`'s was) is the coordinator's
and the owner's, under DIV-0041, not done here. No full-frame fill is in the
band; `widescreen.cpp` patches nothing inside it.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 48 rows. No file under `analysis/calltrace/` names
any of the 50 but the entry lists (every file grepped for the addresses,
today's `reach_*_1003` traces among them). **Fuzz only.** No live run was
made (the brief). Sub-kind 0x0D is area 48's by its count and sound
([`area_w1c.md`](area_w1c.md)); a recorded walk there would let the
coordinator's frame-hash A/B cover it. Which areas show the others is the
owner's to say.

## 10. The rebinding

`grep -rn -i` of the 50 addresses and the seven tables in `src/game`
(`band_rows.py --refs`: seven references to three functions, all in
`scenario_harness_ekh.cpp`):

- **Left raw**: `scenario_harness_ekh.cpp`'s `{"E5B 0x500D20", 0x500D20, ...,
  InPlace(0x500D20), ...}` and its comments naming `0x500930` / `0x5011A0`.
  The row runs Capcom's code in place as the harness's proof (before this
  group injects); its base must be Capcom's address, and the round's earlier
  groups left their rows there by address too (section 13's debts). For the
  coordinator's rebinding pass: the value would read
  `bof3::addr::EffectKind18Sub0F_Open`.
- **Rebound**: none needed - no code of ours calls or tables an address of
  the band.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 48 lines, the read extents
of the 50 less two already listed with the same extent (`004FF5A0 69`,
`00500610 19D`). Eight host lines there are longer than the function they
start (`004FF610 475`, `004FFA90 334`, `004FFDD0 1A8`, `004FFF80 1DD`,
`00500160 4A4`, `005007B0 413`, `00500BD0 313`, `00500EF0 954` - each ran on
over the hidden starts after it); the read extent of each was added beside it
for the consolidation.
