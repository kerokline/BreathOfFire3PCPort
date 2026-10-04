# Group E3C: effect kinds 0x6D, 0x6E, 0x6F, 0x72, 0x73, 0x74, 0x75 and the shared debris draw

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave three, from the round branch's tip `4409f85`. **51 functions ours**
(`src/game/effect_3c.cpp`, shadow name `effect_3c`): the cut table's 50 rows
for E3C (`analysis/round13_cut.tsv`, the band `0x484050..0x485C50`) and one
start no list of the cut has - kind 0x75's shared tail `0x485C60` (section 5).
Each read to its last instruction with capstone and fuzzed through the
scenario harness in effect mode ([`scenario_harness.md`](scenario_harness.md)
section 8) without edits to it: 204,000 rounds, 0 mismatches; 84 of 85 controls refused by a count, the other an equivalent mutant whose near variant is refused.
**Fuzz only**: no recorded route enters any of the 51 (section 9).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x6E: sixteen shards emitted one every fourth frame from the record's point, rising, growing and darkening (E3B's shard quad draws them); `Field_ScriptFlags` bit 6 held while any is live | 8 | `Effect_KindHandlers[0x6E]` (`0x655508`), `EffectKind6E_States` `0x654A44` (4) |
| 0x6D: thirty-two flat triangles thrown from one of two fixed boxes (`+6` picks the box and the pool block) and falling | 7 | `Effect_KindHandlers[0x6D]` (`0x655504`), `EffectKind6D_States` `0x654A54` (2) |
| 0x6F: a line of segments along z at x 0x5C (`+1` 1) or along x at z 0x44 (`+1` 2) by story flags 0x5C / 0x5D and `Cond_ByteFD`, each a `LINE_F2` and two shaded quads; party members on its four cells turned away | 8 | `Effect_KindHandlers[0x6F]` (`0x65550C`), `EffectKind6F_States` `0x654AA0` (4) |
| 0x72: thirty-two debris triangles - kind 0x1F's twin (E1C's `EffectKind1F_Start` / `_Debris`) | 3 | `Effect_KindHandlers[0x72]` (`0x655518`), `EffectKind72_States` `0x654AB8` (3) |
| the debris draw and set-up kinds 0x1E, 0x1F, 0x4B and 0x72 share | 2 | calls (E1C's and E2E's by address, kind 0x72's by name) |
| 0x73: thirty-two textured sparks - `+1` 0 emitted one every fourth count and risen (area 132's single), `+1` 1 sixteen at once spread by their angle (area 132's pair) | 13 | `Effect_KindHandlers[0x73]` (`0x65551C`), `EffectKind73_States` `0x654AC4` (2), each a sub-state dispatcher by `+2`: `EffectKind73A_States` `0x654ACC` (3), `EffectKind73B_States` `0x654AD8` (2) |
| 0x74: a column of textured quads turning round the record's point, rising over 0x40 frames, then narrowing (area 132's) | 5 | `Effect_KindHandlers[0x74]` (`0x655520`), `EffectKind74_States` `0x654AE0` (3) |
| 0x75: a full-screen tile brightened over fifteen frames (E4D's `0x48CA90`), the field objects at pose 6 redrawn over it | 5 | `Effect_KindHandlers[0x75]` (`0x655524`), `EffectKind75_States` `0x654AEC` (3) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the nine dispatchers `evidence`). "Shards", "triangles",
"line", "debris", "sparks", "column", "tile" name the code's shape - the
primitives it commits and the cells it steps - not a play-tested fact: where
the game shows these kinds and what they look like was not traced (section 9;
the owner's word, not this doc's). **Spawners found**: area 132
(`area_w3c.cpp`: `Area132_Effect73` kind 0x73 at `+1` 0, `Area132_Effect73Pair`
two of kind 0x73 at `+1` 1 with `+0xB` 1 / 0, `Area132_Effect74` kind 0x74) -
and a raw scan of the image for `mov byte [reg + 5], kind` finds only those
three. No spawner of kinds 0x6D, 0x6E, 0x6F, 0x72 or 0x75 was found by that
scan, by a scan of every `Effect_SpawnAt` call's pushed kind, or by a grep of
`src/game` and `docs`; they are likely stored by data (an event script's
effect op), not code.

Both `hypothesis` rows of the cut (`0x484F50`, kind 0x72's dispatcher, and
`0x485960`, the spark draw) are effect code: taken.

## 1. What each function does

Every function runs with `Sprite_Current` an `Effect_Objects` record
(`Effect_RunObjects`) or is a cdecl helper called from one; `+1` is the
kind's state (`+2` kind 0x73's sub-state), `+9` a frame count, `+0x34 /
+0x38 / +0x3C` the record's point (x, z, height as 16.16). Sprite_Current is
read again wherever the original reads `[0x937F88]` again.

### 1.1 Kind 0x6E (`EffectKind6E_Run` `0x484050`)

The only dispatcher of the band that **calls** its state (`call [T + +1 *
4]`) and then tail-jumps to the shard pass `EffectKind6E_DrawShards`, so the
shards are stepped and drawn after every state.

| Function | What |
|---|---|
| `EffectKind6E_Start` `0x484070` (state 0) | `Field_ScriptFlags` bit 6 set (an `or byte`); E3B's `0x483C10` (the sixteen shards spread round the point); `+9` 0xB4, `+1` up |
| `EffectKind6E_Emit` `0x484090` (1) | `Frame_Counter & 3` 0: `EffectKind6E_FindShard` into the cursor `0x67626C` (null too), a found shard set up at the point with its rise `+0x1C` 0x200000; `+9` down, at 0 `+0xC` 0x200000, `+9` 0x40, `+1` up |
| `EffectKind6E_EmitSlower` `0x4840F0` (2) | the same, `+0xC` down 0x10000 before each new shard takes it as its rise; at 0 `+1` up |
| `EffectKind6E_Fade` `0x484150` (3) | the shard pass; none live: `Effect_Release` and bit 6 cleared (`and word`, 0xFFBF) |
| `EffectKind6E_ShardInit` `0x484170` (shard) | the 0x28-byte shard at the point: `+0` 1, `+1` 0, `+2` 0x20 (life), `+3` 0xC0 (shade), `+0x1C` 0, `+0x24` 0x80 (size), `+0x26` 2 |
| `EffectKind6E_FindShard` `0x4841C0` | the first of the sixteen shards of 0x28 at `EffectKind30_Shards` with `+0` 0, or null; eax |
| `EffectKind6E_DrawShards` `0x4841E0` | `EffectGte_LoadMapCamera`; a draw mode (`Gpu_GetTPage(0, 1, 0x3C0, 0)`, dtd 0) committed at slot 2; the sixteen stepped through the cursor `0x67626C`: each live one `+0xC += +0x1C`, `+0x24 += 8`, `+3 -= 6`, `+2` down (0 frees it), drawn by E3B's `0x483DA0`; al 1 when any was live |

### 1.2 Kind 0x6D (`EffectKind6D_Run` `0x4842A0`)

The pool is a block of 32 records of 0x28 at `0x92C780 + 0x500 * +6`
(`+6` unbounded: section 7); E1C's kind 0x1E keeps its debris at the same
`0x92C780`.

| Function | What |
|---|---|
| `EffectKind6D_Start` `0x4842C0` (0) | the block cleared, then set up; `+9` 0xFF, `+1` up |
| `EffectKind6D_Move` `0x4842E0` (1) | the particles moved; none live: a tail jump to `Effect_Release` |
| `EffectKind6D_ClearParticles` `0x4842F0` | `+0` 0 in the 32 |
| `EffectKind6D_MoveParticles` `0x484320` | `EffectGte_LoadMapCamera`; each live one: `+0x18 += +8`, `+0x1C += +0xC`, `+0x20 += +0x10`, `+0x10 -= 0x40000` (it falls), `+3 -= 4`, its shape `+4` `Rand & 7`, `+2` down (0 frees it), drawn; al 1 when any was live |
| `EffectKind6D_DrawParticle` `0x4843B0` (particle) | a draw mode linked at (`+0x18`, `+0x1C`); a flat triangle (`0x5A7570`, 0x2C bytes) at the cursor, semi-transparent: its first corner the point `+0x18` projected, the other two that corner plus the s16 row of `0x654A5C` by `+4` (eight rows of four; `fild` / `fadd`), the depth copied to all three through the FPU (`fld`, `fst`, `fstp`); colour (0, `+3`, `+3`); linked with dy -2 |
| `EffectKind6D_InitParticles` `0x4844C0` | each of the 32 from one of two boxes by `+6` (read again for each): `+6` 0 - x `0x120000 + (Rand & 0x1FF) << 8`, z `0x2E8000 + ..`, the speeds `+8` `(x - 0x130000) >> 4` less 0x800, `+0xC` `(z - 0x2F8000) >> 4`, `+0x20` `(0x228000 - x) << 7`; `+6` not 0 - x `0x78000 + ..`, z `0x318000 + ..`, `+8` `(x - 0x88000) >> 4`, `+0xC` -0x800, `+0x20` `(0x420000 - z) << 7`; both `+0x10` `-(Rand & 0xFF) << 5`; `+0` 1, `+1` 0, `+2` 0x18, `+3` 0x60, `+4` `Rand & 7` |

### 1.3 Kind 0x6F (`EffectKind6F_Run` `0x4845F0`)

| Function | What |
|---|---|
| `EffectKind6F_Start` `0x484610` (0) | `Cond_ByteFD` 2 and story flag 0x5C clear: `+1` 1; else `Cond_ByteFD` 3 and flag 0x5D set: `+1` 2; else `+1` 3 |
| `EffectKind6F_AlongZ` `0x484670` (1) | `+6` 0; the line at x 0x5C0000 (`+0xC`, `+0x18`) from z 0x98000 (`+0x10`); its ground `AreaMap_Elevation(+0xC, (+0x1C + +0x10) / 2)` to `+0x3E`; `EffectKind6F_PushParty(0)`; two steps of 0x20000, each the record's point put at the step's middle and drawn at the ground + 0x100 and + 0x200 (`EffectKind6F_DrawSegment(+0xC, +0x18)` twice, then `BareRet` with the two points); flag 0x5C set: sound 0x206, `+1` 3 |
| `EffectKind6F_AlongX` `0x4847D0` (2) | the same along x at z 0x440000 from x 0x3D8000, `EffectKind6F_PushParty(1)`; flag 0x5D clear: sound 0x206, `+1` 3 |
| `EffectKind6F_Watch` `0x484930` (3) | `Cond_ByteFD` 2 with flag 0x5C clear, then (tested too) `Cond_ByteFD` 3 with flag 0x5D set: sound 0x206, `+1` 0 |
| `EffectKind6F_PushParty` `0x4849A0` (byte) | 0: the four z cells of `0x654AB0` at x 0x5C, 1: the four x cells of `0x654AB4` at z 0x44; each put in the record's `+0x34` / `+0x38` and `Party_MemberAt(+0x34, +0x38, 0)`; a member there, with `Field_ScriptFlags2 & 0x1400` and `Field_Request` 0, faces away (`ObjTrio +8`: 7 or 3 by x, 1 or 5 by z) and `Member_SetState2_8(member, 2)` - the original keeps each answer in its argument slot's low byte and hands the whole slot on |
| `EffectKind6F_DrawSegment` `0x484B10` (a, b) | a draw mode (dtd 1) linked at the record's point; `EffectGte_LoadMapCamera`; a `LINE_F2` from a to b projected, opaque, kind 0x6F's colour (the one row at `0x654A9C` by `+6`), linked; the screen angle `Math_Ratan2(dy, dx)` (each delta through `_ftol`, then a float); each end's size `EffectGte_ProjectSize(end, {0x40, 0}, out)` with `Frame_Counter & 1` (read after each) added to out's dword; `EffectKind6F_DrawRibbon(the four screen words through _ftol, the sizes, the angle + 0x400 and + 0xC00)`; a closing draw mode (dtd 0) linked |
| `EffectKind6F_DrawRibbon` `0x484D00` (8 words) | two `POLY_G4`s at the cursor (read at entry), semi-transparent, sharing the edge a-b in the colour, the far corners black at `Math_Cos` / `Math_Sin` times the radius `>> 12` off the ends (the first at ta and tb + 0x800, the second, a copy of the first's 0x44 bytes, at ta + 0x800 and tb); each linked (1, 0x44). Every argument read as its low s16 |

### 1.4 Kind 0x72 (`EffectKind72_Run` `0x484F50`) and the debris

Kind 0x72's two states are byte-for-byte the shape of E1C's kind 0x1F
(`EffectKind1F_Start` `0x46EEB0`, `EffectKind1F_Debris` `0x46EEE0`), its third
`Effect_StateRelease`.

| Function | What |
|---|---|
| `EffectKind72_Start` `0x484F70` (0) | the 32 debris records of 0x2C at `EffectKind30_Shards` set up (`EffectDebris_InitOne`); `+9` 0, `+1` up |
| `EffectKind72_Debris` `0x484FA0` (1) | a draw mode (page (0x380, 0x100)) committed at slot 1; `EffectGte_LoadMapCamera`; each drawn, its angle `+0x24` up 0x10, its shade `+0x2A` up 0x20 while `+9` (`Sprite_Current` read after the draw) is below 4, else down 2; the last read record's `+9` up; above 0x44 (read again) `+1` up |
| `EffectDebris_Draw` `0x485030` (debris) | a `POLY_G3` at the cursor, semi-transparent: the point `+0` projected; the two edges `+0x10` / `+0x18` (three s16) turned by `+0x24` (`(Math_Cos * ex - Math_Sin * ey) >> 8`, `(Math_Sin * ex + Math_Cos * ey) >> 8`), scaled by `+0x28`, the height `<< 12` times the scale, added to the point and projected; colour (v, v, v >> 1) at the first corner, v the shade clamped to 0..0xFF, black at the others; `Gfx_CommitPrim(1, 0x34)` |
| `EffectDebris_InitOne` `0x4851E0` (debris) | the point the record's; three angles `Rand & 0xFFF`, `& 0x3FF`, `& 0xFFF`; the edges (`Math_Cos`, `Math_Sin` of 0x20 and of -0x20) turned in place by a matrix of the angles (`EffectGte_SetDiagonalOne`, `Gte_RotMatrixX`, `_Y` by the second negated, `_Z`; `0x5A7C70` for each edge); the scale `+0x28` 8 + `Rand & 7`; `+0x20`, `+0x22`, `+0x24`, `+0x2A` 0. E1C's `EffectKind1E_DebrisInitOne` is the same with 0x10, 10 + `Rand & 3` and a `Gte_PushMatrix` / `PopMatrix` pair round the turns |

### 1.5 Kind 0x73 (`EffectKind73_Run` `0x4852F0`)

`+1` is the variant (area 132 writes it), each a sub-state dispatcher by `+2`
(`EffectKind73_RunA` `0x485310`, `EffectKind73_RunB` `0x4853B0`). The sparks
are 32 records of 0x18 at `EffectKind30_Shards`, stepped through the cursor
`0x676274`.

| Function | What |
|---|---|
| `EffectKind73A_Start` `0x485330` (A 0) | the sparks cleared; `+9` 0x40, `+2` up, sound 0x20C |
| `EffectKind73A_Emit` `0x485360` (A 1) | `+9 & 3` 0: a free spark set up at the point; the sparks moved (answer unread); `+9` down, at 0 `+2` up |
| `EffectKind73A_Fade` `0x4853A0` (A 2), `EffectKind73B_Fade` `0x485430` (B 1) | the sparks moved; none live: a tail jump to `Effect_Release` |
| `EffectKind73B_Burst` `0x4853D0` (B 0) | `+0xB` set (the pair's first): the sparks cleared; sixteen free sparks set up, each `+0x14` 0x80 and its angle `+0x17` its number 0..15 (a number is spent when none is free); sound 0x206, `+2` up |
| `EffectKind73_SparkInit` `0x4857C0` (spark) | at the point: `+0` 1, `+1` 0, `+2` 0x20, `+3` 0x40, `+0x14` 0x80, `+0x16` 7 |
| `EffectKind73_FindSpark` `0x485810` | the first free of the 32, or null; the cursor left at it or past the last |
| `EffectKind73_ClearSparks` `0x485840` | `+0` 0 in the 32, through the cursor |
| `EffectKind73_MoveSparks` `0x485870` | `EffectGte_LoadMapCamera`; each live one `+4 += 0x2000`; the record's `+1` 0: `+0x14 += 8`, else `+4 += Math_Cos(+0x17 << 8)`, `+0xC += Math_Sin(..) << 8`; `+3 -= 2`, `+2` down (0 frees it), drawn; al 1 when any was live |
| `EffectKind73_DrawSpark` `0x485960` (spark) | a draw mode (abr 2) linked at (`+4`, `+8`); a `POLY_FT4`, semi-transparent, centred on the point `+4` projected, its size `+0x14` scaled at its depth (`EffectGte_ProjectSize` with size and out one local); corners `x - (w >> 1)` and `+ w` on the x87 (`fild`, `fsubr`, `fiadd`), the depth by `mov`; u 0xE0 / 0xFF, v 0x30 / 0x4F, `Gpu_GetClut(0xA0, 0x1E3)`, `Gpu_GetTPage(0, 2, 0x2C0, 0x100)`; colour `+3` in each channel whose bit of `+0x16` is set (4 red, 2 green, 1 blue); linked (3, 0x48) |

### 1.6 Kind 0x74 (`EffectKind74_Run` `0x485440`)

| Function | What |
|---|---|
| `EffectKind74_Start` `0x485460` (0) | the foot `+0xC` / `+0x10` / `+0x14` the point, the top `+0x1C` the foot's height, the angle `+0x20` 0, the width `+0x24` 0x1000000; `+9` 0x40, `+1` up, sound 0x207 |
| `EffectKind74_Rise` `0x4854D0` (1) | the top up 0x1000000, no higher than the foot + 0x8000000; the angle turned 0xFE00; drawn; `+9` down, at 0 `+9` 0x10 and `+1` up |
| `EffectKind74_Fade` `0x485530` (2) | `+9` down; at 0 a tail jump to `Effect_Release`; else turned, the width down 0x100000, a tail jump to the draw |
| `EffectKind74_Draw` `0x485570` | `EffectGte_LoadMapCamera`; from the foot's height to the top + 0x1000000 in steps of 0x100000: a point at the angle (up 0x100 a step) `32 * (Math_Cos, Math_Sin)` round the foot, two heights (the step less the width, no lower than the foot; the step, no higher than the top - read afresh each time) projected, a `POLY_FT4` (opaque, grey 0x80, `Gpu_GetTPage(1, 0, 0x1C0, 0x100)`, `Gpu_GetClut(0, 0x1E5)`) from the previous step's pair to this one's, linked at the point (2, 0x48) |

### 1.7 Kind 0x75 (`EffectKind75_Run` `0x485BA0`)

| Function | What |
|---|---|
| `EffectKind75_Start` `0x485BC0` (0) | the tile's colour `+0x5D..+0x5F` 0; `+9` 0xF, `+1` 1 |
| `EffectKind75_Brighten` `0x485C00` (1) | `+9` down; not 0: each colour byte up 7; at 0 `+1` 2. E4D's `0x48CA90` (a full-screen semi-transparent tile of those bytes) and a tail jump to the mark |
| `EffectKind75_Hold` `0x485C50` (2) | the tile and the tail jump |
| `EffectKind75_MarkSprites` `0x485C60` | `Sprite_Current` kept; each of the thirty `Sprite_Objects` records made current, and one in use (`+0` bit 0) at pose 6 (`+6`) given `+0x29` 5 and redrawn (`Sprite_UpdateScreen`); `Sprite_Current` put back |

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-10-03). Where the original indexes past a
table ours aborts with a `Fatal` naming the function (the round-nine rule;
nothing in the fuzz reaches it): the nine dispatchers past their tables,
`EffectKind6D_DrawParticle` past its eight shapes, kind 0x6F's colour past its
one row (`EffectKind6F_DrawSegment`, `_DrawRibbon`), and
`EffectKind6F_PushParty` past `ObjTrio`'s three records. No path of the
game reaches any of them as far as the band's code shows: every writer of
`+1` / `+2` steps it inside its table, `+4` is `Rand & 7`, kind 0x6F writes
`+6` 0 before drawing and `Party_MemberAt` answers a member or 0xFF.

The x87 is used as the original uses it (inline `fild` / `fadd` / `fsubr` /
`fiadd` / `fld`-`fst`-`fstp`, and `_ftol`'s truncation through a 64-bit
`fistp` with the control word put back), under the game's own control word.

## 3. The tables

**The state tables** (`symbols.toml` `[[data]]`): each the table's own length
to the next table a dispatcher indexes, or to the first dword that is not code
- checked by a raw scan of the image for every cell address (scratch
`cellscan.py`: every one of the 32 table-reached functions is named by exactly
one `.data` cell, its own table's or `Effect_KindHandlers`') and against what
the states store into `+1` / `+2`:

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind6E_States` `0x654A44` | 4 | `0x654A54`, kind 0x6D's (the tool's run starts at E3B's `0x6549EC`) |
| `EffectKind6D_States` `0x654A54` | 2 | `0x654A5C`, kind 0x6D's shapes (data) |
| `EffectKind6F_States` `0x654AA0` | 4 | `0x654AB0`, the cell bytes (data) |
| `EffectKind72_States` `0x654AB8` | 3 | `0x654AC4`, kind 0x73's (the tool's run says 16); its third `Effect_StateRelease` |
| `EffectKind73_States` `0x654AC4` | 2 | `0x654ACC`, `EffectKind73A_States` (the tool's run says 13) |
| `EffectKind73A_States` `0x654ACC` | 3 | `0x654AD8`, `EffectKind73B_States` (by `+2`) |
| `EffectKind73B_States` `0x654AD8` | 2 | `0x654AE0`, kind 0x74's (by `+2`) |
| `EffectKind74_States` `0x654AE0` | 3 | `0x654AEC`, kind 0x75's |
| `EffectKind75_States` `0x654AEC` | 3 | `0x654AF8`, not code |

**The data the band reads in place** (raw constants in
`effect_3c_callees.h`, not named): kind 0x6D's shapes `0x654A5C` (eight rows
of four s16), kind 0x6F's colour `0x654A9C` (one row of three bytes - a second
row would read the low bytes of `EffectKind6F_States[0]`), its cells
`0x654AB0` / `0x654AB4` (four bytes each).

**The cells**: the shard cursor `0x67626C` (E3B's kind 0x6C writes it too -
`0x483C10`, `0x483D30..`, `0x483FA0`, `0x484000` - so neither group names it;
the coordinator's), the spark cursor `0x676274` (only this band's code), kind
0x6D's pool `0x92C780` (E1C's kind 0x1E debris shares it).

## 4. The fuzz (`effect_3c_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_3c`, effect mode (`g.effect`; kinds
0x6D, 0x6E, 0x6F, 0x72..0x75, each clone its own), 4,000 rounds a function
(`BOF3X_E3C_ONLY=<name>` runs the clones whose name holds it). Shapes: 41
`kEffect` (the nine dispatchers' `state_span` - or kind 0x73's sub-dispatchers'
`sub_span` - their table's length, section 3; kind 0x73's states and
`EffectKind73_MoveSparks` with `state_span` 2, the variant), ten `kCall` (the
seven helpers with arguments, the two finders, `EffectKind73_ClearSparks`).
`ret_mask` 0xFF on the three that answer in al (`EffectKind6E_DrawShards`,
`EffectKind6D_MoveParticles`, `EffectKind73_MoveSparks`: every caller tests al
only), 0xFFFFFFFF on the two that answer a pointer (`EffectKind6E_FindShard`,
`EffectKind73_FindSpark`). The nine tables are `DataTable`s, swapped for
recorders on both sides. **Regions** beyond effect mode's standard ones: the
two cursors `0x67626C` and `0x676274` (4 each) and kind 0x6D's two pool blocks
`0x92C780` (0xA00). Everything else the band touches is standard: the
records, `Sprite_Current`, `EffectKind30_Shards` (`0x92BF80`, 0x644 - the
shards' 0x280, the sparks' 0x300, the debris' 0x580), `Field_ScriptFlags`,
`Field_ScriptFlags2`, `Field_Request`, `Cond_ByteFD`, `Frame_Counter`, `ObjTrio`
(`+8`), `Sprite_Objects` (`+0`, `+6`, `+0x29`), the packet buffer.

**Callees**: the effect-standard rows for `Effect_Release` (clears `+0..+4`),
`Flags_Test`, `Sound_PlayEffect`, `Rand`, `AreaMap_Elevation`,
`Party_MemberAt` (0..2 or 0xFF), `Member_SetState2_8` (writes what the real one
writes), `BareRet`, `Math_Sin` / `Cos` (never 0 or -1), `Math_Ratan2`,
`EffectGte_LoadMapCamera` / `_ProjectPoint` (the point hashed, out three
fractional floats) / `_ProjectSize` (the point and the size's first word
hashed, out two s16) / `_SetDiagonalOne`, `Gte_RotMatrixX` / `Y` / `Z`,
`0x5A7C70`, `0x5A7570`, `Sprite_UpdateScreen` (`Sprite_Current` and its 0x80
bytes logged), `MapView_LinkPrimAt` (the cursor moved two times in three),
`Gfx_CommitPrim`, the `Gpu_*`; `_ftol` called for real on both sides.
**Re-listed**: `EffectGte_ProjectPoint`, out filled as E2A's `FillFloat` does -
fractional floats, and one time in eight a NaN (quiet or signalling) or a value
past `_ftol`'s range: the standard row's floats never reach the integer
indefinite nor the signalling NaN the particle draw's FPU copy turns quiet
(control 21 is refused by it). `EffectGte_ProjectSize` is not re-listed: the
size `EffectKind6F_DrawSegment` hands it has its second word written ({0x40,
0}) and the spark draw's two words are equal, so the standard first-word hash
loses nothing. **The group's own**
called directly, listed by name: the three movers as `kFlag` (al read), the
two finders answering the first free record or null (a quarter of the time
null; the spark finder also leaves the cursor where the real one does), the
three set-ups filling what they write (0x28, 0x2C, 0x18), the four draws
handed a record with that record hashed (0x28, 0x2C, 0x18; the segment's two
points 12 bytes each), `EffectKind6F_PushParty` at a byte, the ribbon's eight
words at s16, the clears, the particle set-up, the column draw and the sprite
mark as `kPhase`. **Other groups'** (by address): E3B's `0x483C10` logging
`Sprite_Current` and the point it reads, E3B's `0x483DA0` the shard hashed,
E4D's `0x48CA90` `Sprite_Current` and the dword `+0x5C` (its colour bytes).

**Seeds** (per function, after the harness's per-round fill): on all 20
records `+9` at 0..5, 0x44, 0x45, 0xFF or random, and `+6` below 2 for kind
0x6D's functions (the block) and 0 for kind 0x6F's (the colour row); the
pools' `+0` free a third of the time (all used in a fifth of the rounds) and
the lives `+2` at 0, 1, 2, 0x20; `Frame_Counter`'s low bits 0 half the time;
`Cond_ByteFD` 2, 3 or other; `Field_ScriptFlags2`'s bits 0x1400 and
`Field_Request` cleared half the time each; kind 0x6D's shapes `+4` below 8;
the debris shade at -1, 0, 1, 0xFF, 0x100, 0x7FFF, 0x8000; kind 0x72's `+9`
at 3, 4, 0x43..0x45; kind 0x73's `+0xB` 0 half the time; kind 0x74's top at
and round its cap; for the column draw every record's foot, top and width a
few steps round one height (kept off the signed wrap - section 7), since the
loop reads the current record's bounds again after each call; the field
objects' `+0` and `+6` at the mark's tests. **Arguments**: each helper handed a
record of its own pool (a shard, a particle of either block, a debris, a
spark), `PushParty`'s byte 0, 1, 2 or random over random upper bytes, the
segment's two points the record's `+0xC` and `+0x18` (as its callers), the
ribbon's eight words random. **Disturbance** (the group's, from the hash
only): `+9`, the two cursors moved to another record of their pools (the
shard and spark passes read them again after each draw), `Cond_ByteFD`, kind
0x74's top a few steps above the foot, the colour bytes `+0x5D..+0x5F`, `+0xC`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_3c`,
exit 0): 204,000 rounds over 51 functions, 2,111,758 calls to the stand-ins,
**0 mismatches**; 27,324 bytes of state in 48 regions; 404 stand-ins. Every
entry of the nine tables reached (each handler recorder 959..2,048 calls,
`Effect_StateRelease` 1,359); `Effect_Release` 5,901, `Party_MemberAt` 10,712,
`Member_SetState2_8` 1,815, E3B's quad 47,063, `Rand` 621,702.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
696 self-test lines, no `MISMATCH` line, `inject: 7621 ours, 0 left
original`; `effect_3c` there 204,000 rounds, 2,112,821 calls, 0 mismatches.
**With `BOF3X_WIDE=1`**: `'*'` exit 0, 696 self-test lines, no `MISMATCH`
line. Run three times (once before the `EffectGte_ProjectPoint` re-listing,
twice after it); none died silently.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 50 to the byte (6,880 bytes against the
  cut's 7,125: 31 differ by padding only, none by code), with one exception
  settled by hand: `0x485C50`'s extent 0x51 runs on through its `jmp
  0x485C60` into the code after it, which `0x485C00` also tail-jumps to (the
  tool's clone table lists `0x485C60` as "Capcom's raw" for it, and
  [`scenario_harness.md`](scenario_harness.md) 8.5 names it among the shared
  tails). **`0x485C60` is taken as a function of its own**
  (`EffectKind75_MarkSprites`, 0x41 bytes: its own `push ebp` .. `ret`; the
  wave-two addendum's rule for a draw several states tail-jump to);
  `EffectKind75_Hold`'s clone ends at its `jmp` (0xA).
- **Hidden starts**: 32, each an entry by address - a cell of
  `Effect_KindHandlers` or of a kind's table - not a case or a shared tail.
  Their recorded hosts: E3B's `0x483DA0` (kind 0x6E's five), and this band's
  `0x4841E0`, `0x4844C0`, `0x484D00`, `0x4851E0`, `0x485960`, whose catalog
  extents span the hidden starts after their `ret` (section 11). None of the
  hosts contains our code as a fall-through.
- **The `hypothesis` rows** (`0x484F50`, `0x485960`): kind 0x72's dispatcher and
  the spark draw - effect code, taken.
- **Not in the band, not taken**: nothing - the tool lists no code of the band
  that no list has; `0x485CB0` (after the band) is not E3C's.
- **The cut's `unit` / `label` columns**: right for this band.

## 6. Controls

Planted one at a time by a scratch script (`controls.py`: each plant anchored on a
unique string of `effect_3c.cpp`, rebuilt, run under `BOF3X_E3C_ONLY=<filter>`,
the file restored and rebuilt at the end; the committed file has no switch).
Counts are rounds refused of 4,000 per function run, in this worktree; every
refused run exited 3. **84 of 85 refused by a count**; the one not refused
(62) is an equivalent mutant - when the top equals the cap, `>` and `>=` both
leave it at the cap - and its near variant (85: `>=` storing the cap + 1) is
refused. Control 11 is refused in one round only: the finder's sixteenth
record is the answer only when the fifteen before it are all in use (a fifth
of the rounds seed the pool full, and the sixteenth is then free a third of
the time - most rounds leave an earlier record free); refused, so left.

| # | Run (`_ONLY`) | Plant | Refused |
|--:|---|---|---|
| 1 | `EffectKind6E_Run` | the dispatch through kind 0x6F's table | 4000 |
| 2 | `EffectKind6E_Run` | the tail draw dropped | 4000 |
| 3 | `EffectKind6E_Start` | +9 0xB5 | 4000 |
| 4 | `EffectKind6E_Start` | bit 5 set | 2981 |
| 5 | `EffectKind6E_Emit` | the rise 0x200001 | 1459 |
| 6 | `EffectKind6E_Emit` | +0xC 0x200001 | 400 |
| 7 | `EffectKind6E_EmitSlower` | the rise down 0x20000 | 1447 |
| 8 | `EffectKind6E_EmitSlower` | the count not stepped | 4000 |
| 9 | `EffectKind6E_Fade` | bit 0 cleared too | 682 |
| 10 | `EffectKind6E_ShardInit` | +0x26 3 | 4000 |
| 11 | `EffectKind6E_FindShard` | fifteen looked at | 1 |
| 12 | `EffectKind6E_DrawShards` | shade -5 | 4000 |
| 13 | `EffectKind6E_DrawShards` | the cursor not read again after the quad | 262 |
| 14 | `EffectKind6D_Run` | through kind 0x6E's table | 4000 |
| 15 | `EffectKind6D_Start` | +9 0xFE | 4000 |
| 16 | `EffectKind6D_Move` | the release test inverted | 4000 |
| 17 | `EffectKind6D_ClearParticles` | +0 = 1 | 4000 |
| 18 | `EffectKind6D_MoveParticles` | gravity 0x50000 | 4000 |
| 19 | `EffectKind6D_MoveParticles` | shape & 3 | 4000 |
| 20 | `EffectKind6D_DrawParticle` | the fourth offset the third | 3515 |
| 21 | `EffectKind6D_DrawParticle` | the depth copied by mov (no NaN quieting) | 117 |
| 22 | `EffectKind6D_InitParticles` | shade 0x61 | 4000 |
| 23 | `EffectKind6D_InitParticles` | box 1 +0xC -0x1000 | 3802 |
| 24 | `EffectKind6D_InitParticles` | box 0 always | 3802 |
| 25 | `EffectKind6F_Run` | through kind 0x6E's table | 4000 |
| 26 | `EffectKind6F_Start` | state 3 for 2 | 759 |
| 27 | `EffectKind6F_AlongZ` | z from 0x99000 | 4000 |
| 28 | `EffectKind6F_AlongZ` | pushed along x | 4000 |
| 29 | `EffectKind6F_AlongX` | x from 0x3D9000 | 4000 |
| 30 | `EffectKind6F_AlongX` | flag 0x5C | 4000 |
| 31 | `EffectKind6F_Watch` | sound 0x207 | 760 |
| 32 | `EffectKind6F_PushParty` | facing 2 | 260 |
| 33 | `EffectKind6F_PushParty` | facing 4 | 237 |
| 34 | `EffectKind6F_PushParty` | bit 10 not tested | 150 |
| 35 | `EffectKind6F_PushParty` | state 3 | 739 |
| 36 | `EffectKind6F_DrawSegment` | frame & 3 | 1257 |
| 37 | `EffectKind6F_DrawSegment` | angle + 0x401 | 4000 |
| 38 | `EffectKind6F_DrawSegment` | dtd 0 | 4000 |
| 39 | `EffectKind6F_DrawRibbon` | angle + 0x801 | 4000 |
| 40 | `EffectKind6F_DrawRibbon` | 0x40 bytes copied | 4000 |
| 41 | `EffectKind6F_DrawRibbon` | the b radius a's | 4000 |
| 42 | `EffectKind72_Run` | through kind 0x74's table | 4000 |
| 43 | `EffectKind72_Start` | +9 1 | 4000 |
| 44 | `EffectKind72_Debris` | >= 0x44 | 127 |
| 45 | `EffectKind72_Debris` | below 5 | 911 |
| 46 | `EffectDebris_Draw` | blue v >> 2 | 1729 |
| 47 | `EffectDebris_Draw` | the clamp at 0x100 | 486 |
| 48 | `EffectDebris_Draw` | the turn's sign | 4000 |
| 49 | `EffectDebris_InitOne` | scale 9 + | 4000 |
| 50 | `EffectDebris_InitOne` | Math_Sin(0x10) | 4000 |
| 51 | `EffectKind73_Run` | through 73B's table | 4000 |
| 52 | `EffectKind73_RunA` | through kind 0x72's table | 4000 |
| 53 | `EffectKind73A_Start` | +9 0x41 | 3978 |
| 54 | `EffectKind73A_Emit` | every eighth | 861 |
| 55 | `EffectKind73A_Fade` | the test inverted | 4000 |
| 56 | `EffectKind73_RunB` | through kind 0x6D's table | 4000 |
| 57 | `EffectKind73B_Burst` | angle n + 1 | 3200 |
| 58 | `EffectKind73B_Burst` | +0xB inverted | 4000 |
| 59 | `EffectKind73B_Fade` | the test inverted | 4000 |
| 60 | `EffectKind74_Run` | through kind 0x75's table | 4000 |
| 61 | `EffectKind74_Start` | width + 1 | 4000 |
| 62 | `EffectKind74_Rise` | cap at >= | **not refused** (exit 0, equivalent) |
| 63 | `EffectKind74_Rise` | turn 0xFF00 | 4000 |
| 64 | `EffectKind74_Fade` | width - 0xFFFFF | 3595 |
| 65 | `EffectKind74_Draw` | step 0x100001 | 3082 |
| 66 | `EffectKind74_Draw` | v 0xE1 | 3813 |
| 67 | `EffectKind74_Draw` | the floor + 1 | 3992 |
| 68 | `EffectKind73_SparkInit` | colour bits 6 | 4000 |
| 69 | `EffectKind73_FindSpark` | the cursor off by one | 2939 |
| 70 | `EffectKind73_ClearSparks` | thirty-one cleared | 4000 |
| 71 | `EffectKind73_MoveSparks` | shade -3 | 4000 |
| 72 | `EffectKind73_MoveSparks` | spread << 7 | 2959 |
| 73 | `EffectKind73_MoveSparks` | the cursor not read again after the draw | 820 |
| 74 | `EffectKind73_DrawSpark` | v 0x4E | 4000 |
| 75 | `EffectKind73_DrawSpark` | green on bit 2 | 1970 |
| 76 | `EffectKind73_DrawSpark` | the last corner + w | 3455 |
| 77 | `EffectKind75_Run` | through kind 0x72's table | 4000 |
| 78 | `EffectKind75_Start` | +9 0xE | 4000 |
| 79 | `EffectKind75_Brighten` | blue + 6 | 3589 |
| 80 | `EffectKind75_Brighten` | state 1 kept | 405 |
| 81 | `EffectKind75_Hold` | the tile dropped | 4000 |
| 82 | `EffectKind75_MarkSprites` | +0x29 4 | 3995 |
| 83 | `EffectKind75_MarkSprites` | pose 6 or more | 3996 |
| 84 | `EffectKind75_MarkSprites` | Sprite_Current not put back | 3998 |
| 85 | `EffectKind74_Rise` | cap at >=, the cap + 1 (62's near variant) | 2363 |

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x6E steps its shards twice in its last state.** (D205, D204) Its dispatcher calls
  the state and then always tail-jumps to the shard pass; state 3
  (`EffectKind6E_Fade`) itself runs the pass first (to learn whether any is
  live). So in state 3 every live shard rises, grows, darkens and ages twice a
  frame and is drawn twice, and on the frame the record is released the pass
  still runs once more from the freed record. Possibly intended (a faster
  fade); ours does the same.
- **Kind 0x6F samples the ground at a stale point on its first frame.** (D214)
  `_AlongZ` reads `+0x1C` (and `_AlongX` `+0x18`) for the elevation's
  coordinate before writing it, so the first frame's ground comes from what
  the spawn left there; from the second frame on it is the line's middle (the
  previous frame's loop leaves the end there).
- **Unchecked indexes** (D200): kind 0x6D's block `0x92C780 + 0x500 * +6` (a byte;
  no spawner found to say how many blocks there are - the next pool the image
  uses, `0x47A130`'s at `0x92D1DC`, starts inside the third block), kind 0x6D's
  shape `+4` into eight rows, kind 0x6F's colour `+6` into one row, and
  `Party_MemberAt`'s answer into `ObjTrio`. In the band's own code every one is
  kept in range; ours aborts past any of them.
- **The nine dispatchers do not bound their state bytes** (D200); every writer of
  `+1` / `+2` in the band steps it inside its table, and kind 0x73's `+1` is
  the spawner's (area 132 writes 0 and 1).
- **The column's step count is unbounded** (D208): `EffectKind74_Draw` steps
  0x100000 at a time from the foot to the top + 0x1000000 with signed
  compares; the states keep the top within 0x8000000 of the foot (at most 0x90
  steps), but a foot near the signed limit would run the loop past the wrap
  (up to 4,096 steps of 0x48 bytes each).
- **Leftovers** (D213): `_AlongZ` / `_AlongX` call `BareRet` (a bare `ret`) with the
  line's two points pushed; kind 0x6D's start clears the block it then sets up
  in full; `EffectKind6F_PushParty` hands `Member_SetState2_8` its whole
  argument slot (the callers push 0 or 1, so the upper bytes are 0).

## 8. Calls across groups

**Outbound, raw** (`band_rows.py --edges`; `SH_AT`, listed in the fuzz):
E3B's `0x483C10` (kind 0x6E's start: the shards spread) and `0x483DA0` (the
shard quad, 16 a frame); E4D's `0x48CA90` (kind 0x75's tile). E3B merges
before this group (the round's order), E4D after. By address and nobody's:
`0x5A7C70` (the matrix turn) and `0x5A7570` (the flat triangle) - both
effect-standard rows. By name, already ours: EGT's four
(`EffectGte_LoadMapCamera`, `_ProjectPoint`, `_ProjectSize`,
`_SetDiagonalOne`), `Effect_Release`, `Flags_Test`, `Sound_PlayEffect`,
`AreaMap_Elevation`, `Party_MemberAt`, `Member_SetState2_8`, `BareRet`,
`Sprite_UpdateScreen`, `MapView_LinkPrimAt`, `Gfx_CommitPrim`, the `Gpu_*` /
`Gte_*` / `Math_*` primitives, and FC1's `Effect_StateRelease` through kind
0x72's table.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `EffectKind1E_DebrisDraw` `0x46EBA0` (`0x46EBEE`), `EffectKind1F_Debris` `0x46EEE0` (`0x46EF2E`) | E1C (ours) | `EffectDebris_Draw` - by `effect_1c_callees.h` `kDebrisDraw`, rebound |
| `EffectKind1F_Start` `0x46EEB0` (`0x46EEBD`) | E1C (ours) | `EffectDebris_InitOne` - `kDebrisInit`, rebound |
| `EffectKind4B_Scatter` `0x47AAF0` (`0x47AB43`) | E2E (ours) | `EffectDebris_Draw` - `effect_2e_callees.h` `kDebrisDraw`, rebound |
| `0x48BB40` (`0x48BB40`) | E4C (wave four) | `EffectKind6E_FindShard` `0x4841C0` - E4C calls it by name once this merges |
| `scenario_harness_ekh.cpp` | EKH's self-test | copies `0x4857C0` (`EffectKind73_SparkInit`) in place as its "kCall handed a record" proof - a harness file, left raw for the coordinator |

`Effect_RunObjects` reaches the seven kind dispatchers through
`Effect_KindHandlers` (read in place). No other table of the image holds an
address of the band (`cellscan.py`).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 50 rows of the band, and no first-call trace under
`analysis/calltrace` names any of the 51 addresses (a grep of every file but
the entries lists). **Fuzz only.** No live run was made (the brief). Which
scene shows which kind is the owner's to say; a recorded walk through area
132 would let the coordinator's frame-hash A/B cover kinds 0x73 and 0x74.

## 10. The rebinding

`grep -rn -i` of the 51 addresses and the nine tables in `src/game`
(`band_rows.py --refs` found 21 references to 5 functions). **Rebound** (the
value unchanged, so the fuzz keys stand; the one line each):
`effect_1c_callees.h` `kDebrisDraw = bof3::addr::EffectDebris_Draw` and
`kDebrisInit = bof3::addr::EffectDebris_InitOne`; `effect_2e_callees.h`
`kDebrisDraw = bof3::addr::EffectDebris_Draw` (with the
`bof3/symbols.gen.h` include it lacked). **Left raw**: E1C's and E2E's fuzz
rows keep their labels `"0x485030 (E3C)"` / `"0x4851E0 (E3C)"` and key on the
constants (the log's text, not a reference); the comments in `effect_1c.cpp`,
`effect_2e.cpp` and `scena_sx2.cpp` that cite `0x485030`, `0x4851E0`,
`0x4849A0` by address (comments, still right); `scenario_harness_ekh.cpp`'s
`0x4857C0` (a harness file: the coordinator's).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 38 lines, the read extents
of the 51 less thirteen already listed right. They correct five host lines
left in place: `004841E0 10F` (the code is 0xB6), `004844C0 4D6` (0x12B),
`00484D00 32F` (0x24E), `004851E0 385` (0x10C) and `00485960 9AE` (0x240) -
each old line spans the hidden starts after its host's `ret`.
