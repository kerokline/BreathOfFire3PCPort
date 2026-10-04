# Group E4F: effect kinds 0xAA..0xB1, 0xB9 and 0xBA

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..11),
wave four, from the round branch's tip `9bebe7f`. **49 functions ours**
(`src/game/effect_4f.cpp`, shadow name `effect_4f`): the cut table's 45 rows
for E4F (`analysis/round13_cut.tsv`, the band `0x491D70..0x493F70`), one the
band holds that no list has (`0x492AF0`, inside the cut's `0x492AA0` extent),
and three unplaced rows of the band that serve its own kinds (`0x492530`,
`0x492750`, `0x492CF0`; section 9). Each read to its last instruction with
capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
196,000 rounds, 0 mismatches; 55 of 55 controls refused (section 5). **No divergence**:
every function is a faithful replacement (since round thirteen's end kind
0xAF's tile is DIV-0041's widened fill, section 2). One latent defect kept as the
original has it (`EffectKindAD_DrawArc`'s second vertex depth, section 7).
**Fuzz only**: no recorded route enters any of the 49 (section 10). Every row
of the cut is effect code, the seven `hypothesis` rows too: none left original.

| Kind | Functions | Reached through |
|---|--:|---|
| 0xAA: a dispatcher only (its three states are catalog part 6) | 1 | `Effect_KindHandlers[0xAA]` (`0x6555F8`), `EffectKindAA_States` `0x6552A0` (3) |
| 0xAB: kind 0x81's drops (E3D's) from eight sources - red or grey dots and 3 x 3 tiles linked into the map | 6 | `Effect_KindHandlers[0xAB]` (`0x6555FC`), `EffectKindAB_States` `0x6552BC` (3) |
| 0xAC: a dispatcher only (states part 6; area 198's `Area198_Shake` spawns it) | 1 | `Effect_KindHandlers[0xAC]` (`0x655600`), `EffectKindAC_States` `0x6552C8` (3) |
| 0xAD: a dispatcher, `Effect_StateNext`, and the arc its state 1 (part 6) draws | 3 | `Effect_KindHandlers[0xAD]` (`0x655604`), `EffectKindAD_States` `0x6552D4` (5) |
| 0xAE: an ellipse of 16 triangles that sinks, widens, narrows and shrinks away | 7 | `Effect_KindHandlers[0xAE]` (`0x655608`), `EffectKindAE_States` `0x6552E8` (5) |
| 0xAF: a red full-screen tile brightened and faded, again while `Draw_PassFlags` is set | 5 | `Effect_KindHandlers[0xAF]` (`0x65560C`), `EffectKindAF_States` `0x6552FC` (3) |
| 0xB0: sixteen bars started one by one (catalog part 6 draws them) | 6 | `Effect_KindHandlers[0xB0]` (`0x655610`), `EffectKindB0_States` `0x655308` (3) |
| 0xB1: the camera swayed - `Camera_Angles[1]` between -0xF0 and 0xF0 | 4 | `Effect_KindHandlers[0xB1]` (`0x655614`), `EffectKindB1_States` `0x655314` (3, called) |
| 0xB9: kind 0x64 (E3A's) again - a glow at the leader, shards, sparks, a trail (area 188 spawns it) | 13 | `Effect_KindHandlers[0xB9]` (`0x655634`), `EffectKindB9_States` `0x655320` (9) |
| 0xBA: a dispatcher, its start, and the line its state 1 (part 6) draws (area 121 spawns it) | 3 | `Effect_KindHandlers[0xBA]` (`0x655638`), `EffectKindBA_States` `0x655344` (3) |

The kind numbers are `Effect_KindHandlers` indices (decimal 170..177, 185,
186 in the cut's `unit_desc`). The spawners of ours that store these kinds
into `+5`: `Area198_Shake` (`area_w4f.cpp`, 0xAC), `Area188_SpawnEffectB9`
(`area_w4e.cpp`, 0xB9), `Area121_SpawnEffectBA` (`area_w3b.cpp`, 0xBA). The
others are spawned by code that is not ours (the cut's `unit_desc` names no
area; catalog part 6, "Scenario effects (SCE1xEF overlays)", holds the
neighbouring states). What each effect looks like in the game is not stated
here: the descriptions are of the primitives the code builds.

## 1. What each function does

Every state handler runs with `Sprite_Current` an `Effect_Objects` record; the
record's bytes are `+1` the state, `+3` a shade, `+6` a frame, `+9` a count,
`+0xC..` and `+0x34..` points, `+0x2E..` words, `+0x64..` a point. "+9 down,
at 0" is: `+9` decremented, `Sprite_Current` read again, tested.

### 1.1 The dispatchers

`EffectKindAA_Run` `0x491D70`, `_AB_` `0x492510`, `_AC_` `0x4925A0`, `_AD_`
`0x492660`, `_AE_` `0x492760`, `_AF_` `0x492980`, `_B0_` `0x492A50`, `_B9_`
`0x493550`, `_BA_` `0x493E10`: `mov ecx, [Sprite_Current]; xor eax, eax; mov
al, [ecx + 1]; jmp [eax * 4 + T]`, unbounded. `EffectKindB1_Run` `0x493430`
**calls** through its table (`call [eax * 4 + 0x655314]`) and then sets
`MapView_Redraw` to 2. Ours reads the entry in place (the fuzz swaps the
cells) and aborts past the table (section 6).

### 1.2 Kind 0xAB: kind 0x81's drops, eight sources

A copy of E3D's kind 0x81 ([`effect_3d.md`](effect_3d.md)) with eight
sources where 0x81 has sixteen, its own cells, a different drop draw and no
moving count. Its pools: 256 drops of 0x18 at `0x92BF80` (over
`EffectKind30_Shards`, kind 0x81's pool), eight sources of 0x14 at `0x92D780`.

| Function | What |
|---|---|
| `EffectKindAB_Start` `0x492530` (state 0, hidden in `0x492400`) | the record's point `+0x34..` the leader's (`ObjTrio +0x34..`, `Sprite_Current` read for each); `EffectKind81_ClearDrops`; `EffectKindAB_PlaceSources`; `+9 = 0x78`; `+1` up |
| `EffectKindAB_MoveDrops` `0x492AF0` (reached by a tail `jmp` from state 1 `0x492580`, part 6) | `EffectGte_LoadMapCamera`; each drop in use: speed `+0x14` 0x20000 more, height `+0xC` moved by it, blink `+3` flipped, drawn, life `+2` down - at 0 `+0` and `+1` cleared. `al` 1 when any was in use. `EffectKind81_MoveDrops` without the count at `0x67627C` |
| `EffectKindAB_DrawDrop` `0x492B60` (drop) | a draw mode (page `Gpu_GetTPage(0, 1, 0x3C0, 0)`, dtd 0) **linked** into the map at the drop's x, z (`MapView_LinkPrimAt(x, z, 0, 0xC)`); a semi-transparent `TILE_1` at its projection, `(0x80, 0x80, 0x80)` with `+3` set else `(0x80, 0, 0)`, linked (0x14); a semi-transparent 3 x 3 tile one up and left (the float 1.0 at `0x5C41B8` subtracted), `(0x20, 0x20, 0x20)` or `(0x20, 0, 0)`, linked (0x1C). Neither tile's `+0x10` is written. Kind 0x81's draws only on the blink and in black and 8 |
| `EffectKindAB_PlaceSources` `0x492C80` | `EffectKind81_PlaceSources` for eight: `+0` 1, `+1` 0, `+2` `(Rand & 0xF) - 0x10`, `+3` `Rand & 0x1F`, `+4` / `+8` the s8 cell pair of `EffectKindAB_SourceCells` `0x6552AC` `<< 16`, `+0xC` `AreaMap_Elevation`'s low word `<< 16` |
| `EffectKindAB_Emit` `0x492CF0` (called by state 1) | `EffectKind81_Emit` for eight: each source on, its wait `+3` down; at 0 two drops from `EffectKind81_FindFreeDrop` (life `0x20 + (Rand & 0x1F)`, x / z the source's less 0x10000 plus Rand's byte `<< 8`, its height, speed 0, blink `Rand & 1`) and the wait `Rand & 7`. `al` 1 when any source was on |

State 1 (`0x492580`, part 6): `Draw_PassFlags` 0 - `+1` up; else
`EffectKindAB_Emit` and a tail `jmp` to `EffectKindAB_MoveDrops`. State 2:
`Effect_StateRelease`.

### 1.3 Kind 0xAD

`Effect_StateNext` `0x492750` (hidden in `0x492400`; catalog part 6): `+1`
up. It is `EffectKindAD_States[2]` and `[3]`, and two cells of E3A's
`EffectKind60_States` too.

`EffectKindAD_DrawArc` `0x492DC0` (a, b, inner, outer; state 1 `0x492710`
calls it with `+0x34`, `+0xC`, `+0x5D`, `+0x5E`): a draw mode (page
`(0, 1, 0x380, 0x100)`, dtd 1, slot 1); about `a` and about `b` a point at
`(cos, sin) x 20` (`((v * 5) << 6) sar 4`) from the angle 0xFE00, each
projected; then 16 semi-transparent `POLY_G4`, the angle 0x80 more each (to
0x600): the last two projections and the new two, `(inner x 3)` at a's,
`(outer x 3)` at b's, committed 0x44 to slot 1. The second vertex's depth
`+0x20` is b's projection's **y** (section 7).

### 1.4 Kind 0xAE: the ellipse

| Function | What |
|---|---|
| `EffectKindAE_Sink` `0x492880` (state 1) | the height `+0x3C` down 0x1000000; drawn; +9 down, at 0 `+9 = 4`, `+1` up |
| `EffectKindAE_Widen` `0x4928C0` (2) | the words `+0x2E` up 0x40, `+0x30` down 0x40; drawn; at 0 `+9 = 2`, up |
| `EffectKindAE_Narrow` `0x492900` (3) | `+0x2E` down 0x40, `+0x30` up 0x40; drawn; at 0 `+9 = 0x10`, up |
| `EffectKindAE_Shrink` `0x492940` (4) | both down 0x10; drawn; at 0 a tail `jmp` to `Effect_Release` |
| `EffectKindAE_Draw` `0x493010` | a draw mode (page `(0, 1, 0x380, 0x100)`, dtd 1, slot 1); the record's `+0x34` point copied to a local; `Effect_DrawEllipse(local, +0x2E, +0x30, +0x32, 0xC0, 0)` - the three words pushed as whole registers |
| `Effect_DrawEllipse` `0x493090` (point, w, h, angle, shade, rim) | `EffectGte_LoadMapCamera`; the point projected (o) and `{w, h}` through `EffectGte_ProjectSize` to the radii, each one more on odd frames (`Frame_Counter & 1`, f); 16 semi-transparent `POLY_G3` round o: the rim at the ellipse `((cos a * rx) sar 12, (sin a * ry) sar 12)` (each an s16) turned by `angle & 0xFFFF` (`(cos t x - sin t y) sar 12`, `(sin t x + cos t y) sar 12`, each `fild` + o's float), `a` from 0 by 0x100; the centre `(shade, shade, f ? shade : 0)`, the rim `(rim, rim, f ? rim : 0)`; committed 0x34 to slot 1. E1C's `EffectKind20_Draw` calls it too |

State 0 (`0x492780`) is part 6.

### 1.5 Kind 0xAF: the red screen

| Function | What |
|---|---|
| `EffectKindAF_Start` `0x4929A0` | `+9 = 0`, `+6 = 0x10`, `+1` up |
| `EffectKindAF_FadeIn` `0x4929C0` | `+9` up 0x10; wrapped to 0: drawn at 0xFF and `+1` up; else drawn at `+9` (pushed in `eax`, the record pointer's upper bytes above `al`) |
| `EffectKindAF_FadeOut` `0x492A00` | `+9` down 0x10; drawn at `+9` while `Draw_PassFlags` is set; at `+9` 0: `Draw_PassFlags` 0 - a tail `jmp` to `Effect_Release`; else `+1` down (state 1 again) |
| `EffectKindAF_DrawScreen` `0x4932E0` (shade) | a semi-transparent tile (`Gpu_SetTile`) at (0, 0), 320.0 by 240.0, `(shade, 0, 0)`, committed 0x1C to slot 1 (`+0x10` not written). The 320.0 (`0x493308`) is one of the sites DIV-0041 names: **widened at round thirteen's end** (2026-10-03), `(Widescreen_FillX(), 0)` `Widescreen_FillWidth()` x 240, Capcom's floats while the fills are unarmed or the picture narrow |

### 1.6 Kind 0xB0: the bars

Sixteen bars of 6 at `0x92D820` (`+0` in use, `+1` / `+3` 4, `+2` a state,
`+4` a word), after kind 0xAB's sources.

| Function | What |
|---|---|
| `EffectKindB0_Start` `0x492A70` (0) | the word `+0x2E = 0x12C`, `+9 = 0`; the bars cleared; `+1` up |
| `EffectKindB0_Spawn` `0x492AA0` (1) | `+9` 0: a bar started and `+9 = (Rand & 0xF) + 0x18`; else `+9` down; `Draw_PassFlags` set: `EffectKindB0_StepBars(0xF0)` (its `al` unread) and return while it stays set; `+1` up (to `Effect_StateRelease`) |
| `EffectKindB0_ClearBars` `0x493330` | the sixteen `+0 = 0` |
| `EffectKindB0_NewBar` `0x493350` | the first free: `+0 = 1`, `+2 = 0`; none, nothing |
| `EffectKindB0_StepBars` `0x493370` (length word) | a draw mode (page `(0, 1, 0x380, 0x100)`, dtd 1, **slot 7**); `EffectGte_LoadMapCamera`; each bar in use by `+2`: 0 - `+1 = 4`, `+3 = 4`, `+4 = length`, `+2 = 1`; 1 - `+2 = 2`; 2 - `+4` down 8, below 0 the bar out of use; then drawn by `0x4920F0` (part 6, two `POLY_G4` from `+3` and `+4`) unless `+2` is 0 (a bar just put out of use is drawn once more). `al` 1 when any was in use |

### 1.7 Kind 0xB1: the camera's sway

| Function | What |
|---|---|
| `EffectKindB1_Start` `0x493450` (0) | `CameraTurn_Angles` = `Camera_Angles` (each s16 `<< 16`), `CameraTurn_Steps`' three 0, `MapView_Redraw = +9` (the dispatcher sets 2 after), `+1 = 1` |
| `EffectKindB1_Sway` `0x4934B0` (1) | `Camera_Angles[1]` (`0x929ECA`) up 4; `MapView_SetElevation(it + 0xF0)` (a word in `ax`); above 0xF0 (s16, read again): `Sound_PlayEffect(0x203)` unless `Field_Request`, `+1 = 2` |
| `EffectKindB1_SwayBack` `0x493500` (2) | down 4; the same; below -0xF0: the sound, `+1 = 1` |

It never ends by itself (no state releases it).

### 1.8 Kind 0xB9: kind 0x64 again

`EffectKindB9_States`' entry 0 is E3A's `EffectKind64_Start` itself; the
rest are kind 0x64's states ([`effect_3a.md`](effect_3a.md) section 1.4)
re-linked, with these differences: the glow is E4E's `0x4901D0` (a copy of
`EffectKind64_DrawGlow`), the sounds are 0x214 / 0x215 / 0x216, `_Fly` sets
`+9 = 0x10` (0x64's 0x80), `_FlyWait` waits for the chapter's count 3 (0x64's
0x18) and then counts `+9` down (a sound at 0x10), and state 8 is
`Effect_StateRelease` (0x64's `_Fade`).

| Function | What |
|---|---|
| `EffectKindB9_Grow` `0x493570` (1), `_Hold` `0x4935D0` (2), `_Rise` `0x493610` (3) | the glow grows by 0x20 (`+9 = 0x80` after, sound 0x214), holds (`+9 = 0x20`), rises 0xC0000 and shrinks by 6 - at 0 the sparks cleared (E3A's `EffectKind64_ClearSparks`), `+0x34..` = `+0x64..`, the 16 shards started (E3A's `EffectKind64_InitShard`), sound 0x215 |
| `_Burst` `0x4936D0` (4), `_Launch` `0x493750` (5), `_Fly` `0x493790` (6), `_FlyWait` `0x493850` (7) | `+6` up and a spark on every fourth; shards and sparks drawn; the trail from `+0x18` toward `+0x34` (speeds `+0xC` / `+0x10`) as kind 0x64's |
| `EffectKindB9_DrawTrail` `0x4938E0` | `EffectKind64_DrawTrail`'s instructions but one store order (the dots' copy) |
| `EffectKindB9_SpawnSpark` `0x493B50` | the first free of the eight sparks of 0x18 at `0x92BF80`: `+0` 1, `+1` 0, `+2` 0x10, `+4..` the record's `+0x34..`, `+0x14` 0x100, `+0x16` 0x40, `+0x17` 0. **Kind 0x64's states 4..7 call it too** |
| `EffectKindB9_StepSparks` `0x493BA0` | `EffectKind64_StepSparks`' instructions (draws through E3A's `EffectKind64_DrawSpark`); `al` 1 when any was in use |
| `EffectKindB9_DrawShards` `0x493BF0` | `EffectKind64_DrawShards`' instructions, drawing through `EffectKindB9_DrawShard` |
| `EffectKindB9_DrawShard` `0x493C60` (shard) | a semi-transparent `POLY_G3`: the shard's point projected; its edges `+0x10` / `+0x18` (s16 x, y, z) turned by the angle `+0x24` (`(cos e.x - sin e.y) sar 8`, `(sin e.x + cos e.y) sar 8`) and scaled by `+0x28`, `e.z << 12` scaled, added to the point and projected; the point `(c, c, c >> 1)` for c the word `+0x2A` clamped to 0..0xFF, the corners black; committed 0x34 to slot 2. **`EffectKind64_DrawShards` calls it too** |

### 1.9 Kind 0xBA

`EffectKindBA_Start` `0x493E30` (0): the word `+0x2E = 0`, `+1` up,
`Sound_PlayEffect(0x215)`. State 1 (`0x493E50`, part 6) sets `+0x34..` and
`+0xC..` round `Sprite_ObjectsExtra` record 1's point (`0x8020D8..`) and draws
`EffectKindBA_DrawLine` `0x493F70` (from, to, shade): a draw mode (page `(0,
1, 0x3C0, 0)`, dtd 1, slot 1); `EffectGte_LoadMapCamera`; a semi-transparent
`LINE_F2` between the two projections, `(shade, shade, 0)`, committed 0x20 to
slot 1.

## 2. Divergence

None of this group's own: every function is the original's behaviour; no
entry of its own. `src/game/cheats.cpp` names none of the 49;
`src/game/widescreen.cpp` patches no operand inside them. DIV-0041 lists
`0x493308` (`EffectKindAF_DrawScreen`'s 320.0) among its full-frame sites,
and since round thirteen's end (2026-10-03) ours draws it as
`Effect_DrawScreenTint` does: `(Widescreen_FillX(), 0)`
`Widescreen_FillWidth()` x 240, which is the original's `(0, 0)` 320 x 240
until `Widescreen_ArmFills` has run (the fuzz compares that) and whenever the
picture is narrow (DIV-0041's round's-end amendment).

## 3. Tables and cells named

`symbols.toml` `[[data]]`, each read by hand: the tables sit back to back from
`0x6552BC` to `Effect_KindHandlers` `0x655350`, and a raw scan of the image for
every cell address `0x6552A0..0x65534F` finds each table's start named by its
dispatcher only (and `0x6552AD`, the source cells' second byte, by
`EffectKindAB_PlaceSources`).

| Table | Count | Entries |
|---|--:|---|
| `EffectKindAA_States` `0x6552A0` | 3 | `0x491D90`, `0x491DB0`, `0x491DF0` (part 6); bytes follow |
| `EffectKindAB_SourceCells` `0x6552AC` | 16 bytes | eight s8 pairs (x, z), read in place |
| `EffectKindAB_States` `0x6552BC` | 3 | `_Start`, `0x492580`, `Effect_StateRelease` |
| `EffectKindAC_States` `0x6552C8` | 3 | `0x4925C0`, `0x4925E0`, `0x492620` |
| `EffectKindAD_States` `0x6552D4` | 5 | `0x492680`, `0x492710`, `Effect_StateNext` x 2, `Effect_StateRelease` |
| `EffectKindAE_States` `0x6552E8` | 5 | `0x492780`, `_Sink`, `_Widen`, `_Narrow`, `_Shrink` |
| `EffectKindAF_States` `0x6552FC` | 3 | `_Start`, `_FadeIn`, `_FadeOut` |
| `EffectKindB0_States` `0x655308` | 3 | `_Start`, `_Spawn`, `Effect_StateRelease` |
| `EffectKindB1_States` `0x655314` | 3 | `_Start`, `_Sway`, `_SwayBack` (called) |
| `EffectKindB9_States` `0x655320` | 9 | `EffectKind64_Start`, `_Grow` .. `_FlyWait`, `Effect_StateRelease` |
| `EffectKindBA_States` `0x655344` | 3 | `_Start`, `0x493E50`, `Effect_StateRelease` |

The tool's counts ran on into the next table (224, 221, ... for
`0x6552BC`..); the counts above are each dispatcher's own reach.

## 4. The fuzz (`effect_4f_fuzz.cpp`)

`BOF3X_SHADOW=effect_4f`; `BOF3X_E4F_ONLY=<name>` runs the clones whose name
contains it, `BOF3X_E4F_ROUNDS` the rounds (4,000 each by default). Effect
mode, the kinds `{0xAA..0xB1, 0xB9, 0xBA}`, each clone its own kind; the ten
dispatchers `kEffect` with `state_span` their table's count and the ten tables
`DataTable`s (swapped for recorders on both sides); every state and no-argument
helper `kEffect`; the eight helpers with arguments `kCall` (`Args`: the
record's points as the states pass them, a drop, a shard, the ellipse's point
in a record, its w / h and the bars' length over leftover upper bytes).

**Callees listed** beyond the standard sets: the group's own called directly
(`kPhase` the void ones; `EffectKindB9_StepSparks` and `_StepBars` `kFlag`;
the draws with their argument masks: `Effect_DrawEllipse` `{0, k16, k16, k16,
k8, k8}` - its point is a local, so its 12 bytes are hashed and the address
not logged -, `EffectKindAF_DrawScreen` `{k8}`, `_StepBars` `{k16}`,
`_DrawDrop` / `_DrawShard` / `_DrawTrail` the pool record or the record's
points, hashed); E3D's `EffectKind81_ClearDrops` and `EffectKind81_FindFreeDrop`
(answering the first free drop or null, a quarter of the time null, as
E3D's fuzz); E3A's `EffectKind64_ClearSparks`, `_InitShard`, `_DrawSpark`;
raw `0x4901D0` (E4E's glow: the point, a word, a byte) and `0x4920F0` (part
6's bar draw, the bar read to `+5`). **One standard row re-listed**:
`EffectGte_ProjectSize` hashes both words of the size, because
`Effect_DrawEllipse` writes both (w and h).

**Masks, cited**: the callers push whole registers for bytes and words -
`EffectKindAF_FadeIn` / `_FadeOut` push `eax` / `ecx` with the shade in the
low byte over the record pointer's upper bytes, and `EffectKindAF_DrawScreen`
reads `mov cl, [esp + 0x14]`; kind 0xB9's states push `ecx` with `cx` the size
(`0x4901D0` reads `mov ax, [esp + 0x60]`); `EffectKindAE_Draw` pushes `edx` /
`ecx` / `edx` with the words in the low halves (`Effect_DrawEllipse` reads
`mov cx, [esp + 0x3c]`, `mov dx, [esp + 0x40]` and `and edi, 0xffff`);
`EffectKindB1_Sway` pushes `eax` with `ax` the elevation (the standard
`MapView_SetElevation` row is `kU16`).

**Region** beyond the standard ones: `0x92C5C4..0x92D880`, the rest of kind
0xAB's 256 drops, its eight sources and kind 0xB0's sixteen bars.

**Seeds** (each round, after the random fill): `+9` at 0, 1, 2, 0x10, 0x11,
0xF0, 0xFF; `+6` at 0..4 (the spark tick's `& 3`); `Draw_PassFlags` 0 or set;
the chapter's count 0x903848 at 3 and beside it. Per function: the drops'
in-use, life 1 and the blink; the sources' on and waits 0 / 1, the drop pool
full a quarter of the time; the bars' states 0..3 and words at 0, 7, 8, 9,
0x8000; `Camera_Angles[1]` at 0xEC..0xF1 and 0xFF0F..0xFF14 with
`Field_Request` 0 or set; `+9` at 0xF0 / 0xE0 / 0xEF (fade in) and 0x10 /
0x20 / 0x11 (fade out); the sparks' in-use and life 1; the shards' shade word
at 0, 0xFF, 0x100, 0xFFFF, 0x7FFF, 0x8000; the trail's `+0x34` behind `+0x18`
on x half the time so the dots run.

**Disturbance** (from its hash only): `+9`, `+6`, `+3`, `Draw_PassFlags`
(read again after `EffectKindB0_StepBars` and `EffectKindAF_DrawScreen`), the
chapter's count (read after the trail), `Camera_Angles[1]` (read again after
`MapView_SetElevation`), a drop's life, a bar's word.

**Result, in this worktree**: 196,000 rounds over 49 functions, 3,116,240
calls to the stand-ins, **0 mismatches**, 29,552 bytes of state (46 regions)
and the stand-ins' log compared, first run.

`BOF3X_SHADOW='*'` after the rebinding (section 11): exit 0, 683 self-test
summary lines, every one 0 mismatches; again with `BOF3X_WIDE=1`: the same.
No silent death.

## 5. Controls

A script (the session scratchpad's `e4f/controls.py`) plants each in
`effect_4f.cpp`, rebuilds, runs the one clone (`BOF3X_E4F_ONLY`, 4,000
rounds), restores and rebuilds. **55 planted, 55 refused**: 54 by a
mismatch count, one (48, a dispatcher's count one short) by ours' abort past
its table (section 6). Every function has at least one; each dispatcher has
its table swapped for another of its length (41..49, 52). Controls 50 and 51
read memory once where the original reads it again after a call
(`Camera_Angles[1]` after `MapView_SetElevation`, `Draw_PassFlags` after
`EffectKindB0_StepBars`): refused only by the group's disturbance, in 7 and
3 rounds. Control 8 is the defect of section 7 "fixed". Counts in this
worktree.

| # | Function | Plant | Refused |
|--:|---|---|---|
| 1 | `EffectKindAE_Sink` | `S()[9] = 4;` -> `S()[9] = 5;` | 859 rounds |
| 2 | `EffectKindAF_FadeOut` | `cast<unsigned char>(s[1] - 1)` -> `cast<unsigned char>(s[1] + 1)` | 756 rounds |
| 3 | `EffectKindB0_Spawn` | `(r & 0xFu) + 0x18u` -> `(r & 0xFu) + 0x17u` | 1589 rounds |
| 4 | `EffectKindAB_MoveDrops` | `UL(d + 0x14) + 0x20000` -> `UL(d + 0x14) + 0x21000` | 4000 rounds |
| 5 | `EffectKindAB_DrawDrop` | `p[4] = 0x80; p[5] = 0;` -> `p[4] = 0x80; p[5] = 1;` | 1349 rounds |
| 6 | `EffectKindAB_PlaceSources` | `ast<unsigned char>(r & 0x1Fu)` -> `ast<unsigned char>(r & 0x0Fu)` | 3914 rounds |
| 7 | `EffectKindAB_Emit` | `SH_CALL(Rand)()) & 7u);` -> `SH_CALL(Rand)()) & 3u);` | 2991 rounds |
| 8 | `EffectKindAD_DrawArc` | `SetUL(p + 0x20, UL(o2 + 4));` -> `SetUL(p + 0x20, UL(o2 + 8));` | 4000 rounds |
| 9 | `Effect_DrawEllipse` | `p[6] = odd != 0 ? c0 : 0;` -> `p[6] = c0;` | 1727 rounds |
| 10 | `EffectKindAF_DrawScreen` | `0x43700000u` -> `0x43700001u` | 4000 rounds |
| 11 | `EffectKindB0_StepBars` | `Word(b + 4) + 0xFFF8u` -> `Word(b + 4) + 0xFFF9u` | 3761 rounds |
| 12 | `EffectKindB1_Sway` | `Word(At(at::kTilt))) <= 0x` -> `Word(At(at::kTilt))) < 0x` | 346 rounds |
| 13 | `EffectKindB1_Start` | `MapView_Redraw = s[9];` -> `MapView_Redraw = s[8];` | 3992 rounds |
| 14 | `EffectKindB9_FlyWait` | `if (B(at::kCounter) != 3) r` -> `if (B(at::kCounter) != 4) r` | 2122 rounds |
| 15 | `EffectKindB9_DrawShard` | `cast<unsigned char>(c >> 1);` -> `cast<unsigned char>(c >> 2);` | 2472 rounds |
| 16 | `EffectKindB9_SpawnSpark` | `k[0x16] = 0x40;` -> `k[0x16] = 0x41;` | 3982 rounds |
| 17 | `EffectKindB9_DrawTrail` | `UL(q + 4) - 0x4000u` -> `UL(q + 4) - 0x4001u` | 2691 rounds |
| 18 | `EffectKindB1_Run` | `MapView_Redraw = 2;` -> `MapView_Redraw = 3;` | 4000 rounds |
| 19 | `EffectKindBA_DrawLine` | `p[6] = 0; S` -> `p[6] = 1; S` | 4000 rounds |
| 20 | `EffectKindB9_Rise` | `LL(Sound_PlayEffect)(0x215); ` -> `LL(Sound_PlayEffect)(0x216); ` | 859 rounds |
| 21 | `Effect_StateNext` | `ext(void) { NextState(); }` -> `ext(void) { NextState(); NextState(); }` | 4000 rounds |
| 22 | `EffectKindAB_Start` | `S()[9] = 0x78;` -> `S()[9] = 0x79;` | 4000 rounds |
| 23 | `EffectKindB0_NewBar` | `b[2] = 0; r` -> `b[2] = 1; r` | 4000 rounds |
| 24 | `EffectKindB9_StepSparks` | `4, Word(k + 0x14) + 0xFFF0u);` -> `4, Word(k + 0x14) + 0xFFF1u);` | 3990 rounds |
| 25 | `EffectKindAE_Draw` | `Word(s + 0x32), 0xC0, 0` -> `Word(s + 0x32), 0xC1, 0` | 4000 rounds |
| 26 | `EffectKindB9_Fly` | `S()[9] = 0x10; ` -> `S()[9] = 0x80; ` | 724 rounds |
| 27 | `EffectKindAF_FadeIn` | `ectKindAF_DrawScreen)(0xFF);` -> `ectKindAF_DrawScreen)(0xFE);` | 1319 rounds |
| 28 | `EffectKindB9_Grow` | `LL(Sound_PlayEffect)(0x214);` -> `LL(Sound_PlayEffect)(0x213);` | 859 rounds |
| 29 | `EffectKindB9_DrawShards` | ` Word(shard + 0x24) + 0x10u);` -> ` Word(shard + 0x24) + 0x11u);` | 4000 rounds |
| 30 | `EffectKindBA_Start` | `SetWord(S() + 0x2E, 0); ` -> `SetWord(S() + 0x2E, 1); ` | 4000 rounds |
| 31 | `EffectKindB9_Burst` | `tUL(S() + 0x10, 0xFFFFFC00u);` -> `tUL(S() + 0x10, 0xFFFFFC01u);` | 809 rounds |
| 32 | `EffectKindAE_Shrink` | ` Word(S() + 0x30) + 0xFFF0u);` -> ` Word(S() + 0x30) + 0xFFF1u);` | 4000 rounds |
| 33 | `EffectKindB9_Hold` | `S()[9] = 0x20; N` -> `S()[9] = 0x21; N` | 859 rounds |
| 34 | `EffectKindB9_Launch` | `epSparks)(); S()[9] = 0x20;` -> `epSparks)(); S()[9] = 0x21;` | 4000 rounds |
| 35 | `EffectKindAE_Widen` | `S()[9] = 2;` -> `S()[9] = 3;` | 859 rounds |
| 36 | `EffectKindAE_Narrow` | `0, Word(S() + 0x30) + 0x40u);` -> `0, Word(S() + 0x30) + 0x41u);` | 4000 rounds |
| 37 | `EffectKindAF_Start` | `S()[6] = 0x10;` -> `S()[6] = 0x11;` | 4000 rounds |
| 38 | `EffectKindB0_Start` | `SetWord(S() + 0x2E, 0x12C);` -> `SetWord(S() + 0x2E, 0x12D);` | 4000 rounds |
| 39 | `EffectKindB0_ClearBars` | `++i) Bar(i)[0] =` -> `++i) Bar(i)[1] =` | 4000 rounds |
| 40 | `EffectKindB1_SwayBack` | `>= -0` -> `> -0` | 352 rounds |
| 41 | `EffectKindAA_Run` | `AddressOf(EffectKindAA_St` -> `AddressOf(EffectKindAC_St` | 4000 rounds |
| 42 | `EffectKindAB_Run` | `AddressOf(EffectKindAB_St` -> `AddressOf(EffectKindAF_St` | 4000 rounds |
| 43 | `EffectKindAC_Run` | `AddressOf(EffectKindAC_St` -> `AddressOf(EffectKindAB_St` | 4000 rounds |
| 44 | `EffectKindAD_Run` | `AddressOf(EffectKindAD_St` -> `AddressOf(EffectKindAE_St` | 4000 rounds |
| 45 | `EffectKindAE_Run` | `AddressOf(EffectKindAE_St` -> `AddressOf(EffectKindAD_St` | 4000 rounds |
| 46 | `EffectKindAF_Run` | `AddressOf(EffectKindAF_St` -> `AddressOf(EffectKindB0_St` | 4000 rounds |
| 47 | `EffectKindB0_Run` | `AddressOf(EffectKindB0_St` -> `AddressOf(EffectKindBA_St` | 2661 rounds |
| 48 | `EffectKindB9_Run` | `EffectKindB9_States_count` -> `EffectKindB9_States_count - 1` | abort: EffectKindB9_Run: state byte +1 is 8, past the 8 entries of  |
| 49 | `EffectKindBA_Run` | `AddressOf(EffectKindBA_St` -> `AddressOf(EffectKindB0_St` | 2661 rounds |
| 50 | `EffectKindB1_Sway` | `tatic_cast<std::int16_t>(Word(At(at::kTilt))) <` -> `tatic_cast<std::int16_t>(tilt) <` | 7 rounds |
| 51 | `EffectKindB0_Spawn` | `if (Draw_PassFlags != 0) ret` -> `ret` | 3 rounds |
| 52 | `EffectKindB1_Run` | `un", AddressOf(EffectKindB1_St` -> `un", AddressOf(EffectKindAF_St` | 4000 rounds |
| 53 | `EffectKindB9_FlyWait` | `if (S()[9] == 0x10) S` -> `if (S()[9] == 0x11) S` | 495 rounds |
| 54 | `EffectKindAB_DrawDrop` | ` 4), UL(drop + 8), 0, 0x1C);` -> ` 4), UL(drop + 8), 0, 0x1D);` | 4000 rounds |
| 55 | `EffectKindAD_DrawArc` | `rcOffset(int v) { return Sar((static_cast<U>(v) * 5u) << 6, 4); }` -> `rcOffset(int v) { return static_cast<U>(v) * 20u; }` | 4000 rounds |

## 6. Aborts

Where the original jumps through a state table past its end, ours aborts with
a message naming the table, the byte and this section: the ten dispatchers,
`EffectKindB1_Run`'s call too. The original would jump (or call) through the
dword after - the next kind's table, or for `EffectKindAA_States` its source
cells' bytes, or for `EffectKindBA_States` `Effect_KindHandlers[0]`. Ordinary
play reaches none: every state stores `+1` within its table (the last states
release or loop back). No other index or divide is unbounded: the pools'
loops are counted (256, 8, 16), the source cells are read for eight.

## 7. Latent defects (Capcom's, described, not fixed)

- **`EffectKindAD_DrawArc` `0x492DC0`: the second vertex's depth is a y.** (D223)
  Each `POLY_G4`'s `+0x20` (vertex 1's depth) is loaded from b's projection's
  `+4` (its y) - `mov eax, [esp + 0x2c]` / `mov ecx, [esp + 0x2c]` the same
  cell twice - where vertex 3's (`+0x40`) is its depth `+8`. The nearest
  sensible value is the depth (`+8`). Ours copies the y as the original
  (control 8 plants the depth and is refused). What the renderer does with the
  depth of a semi-transparent quad (its sort, its perspective) decides whether
  it shows; nothing here measured it.
- **`EffectKindB0_StepBars` draws a bar once more after putting it out of
  use** (D204) (state 2, its word below 0: `+0 = 0`, then `+2` is still 2, so it is
  drawn). Probably intended (the last frame of the bar); described only.
- **`EffectKindAF_DrawScreen`'s tile was 320 wide** (D238) under DIV-0041's wide
  picture; widened at round thirteen's end (section 2).
- **`EffectKindB1` never ends** (D202): no state of its table releases the record;
  whatever spawns it must release it (not ours to see).

## 8. Calls across groups

**Out of the group**: E4E's `0x4901D0` (kind 0xB9's glow, three sites) - raw,
`effect_4f_callees.h` `kGlowDraw`, until E4E merges; catalog part 6's
`0x4920F0` (the bar draw) raw, `kBarDraw`. By name: E3A's
`EffectKind64_ClearSparks`, `_InitShard`, `_DrawSpark`; E3D's
`EffectKind81_ClearDrops`, `_FindFreeDrop`; EGT's four; the standard set.

**Into the group** (for the rebinding pass): E3A's `EffectKind64_Burst`,
`_Launch`, `_Fly`, `_FlyWait` call `EffectKindB9_SpawnSpark` `0x493B50`; E3A's
`EffectKind64_DrawShards` calls `EffectKindB9_DrawShard` `0x493C60`; E1C's
`EffectKind20_Draw` `0x46F230` calls `Effect_DrawEllipse` `0x493090`; the
tables `EffectKind60_States` (E3A) hold `Effect_StateNext` `0x492750` twice.
From catalog part 6 (nobody's): `0x492580` tail-jumps to
`EffectKindAB_MoveDrops` and calls `EffectKindAB_Emit`; `0x492710` calls
`EffectKindAD_DrawArc`; `0x493E50` calls `EffectKindBA_DrawLine`.

## 9. What the cut and the tool said, settled

- **Added**: `0x492AF0` (`EffectKindAB_MoveDrops`) - the tool's "code no list
  has" inside the cut's `0x492AA0` row (whose 191 bytes run over it): its own
  frame and `ret`, reached by `0x492580`'s tail `jmp`. A function, taken.
- **Added, unplaced rows of the band** (the wave-three addendum's list): `0x492530`
  (kind 0xAB's state 0, part 6), `0x492750` (`Effect_StateNext`, part 6) and
  `0x492CF0` (kind 0xAB's emitter, part 7) - each serves this group's kinds.
- **Left, catalog part 6 rows in the band** (no group; for the coordinator):
  `0x491D90`, `0x491DB0`, `0x491DF0` (kind 0xAA's states), `0x491E30`,
  `0x491FF0`, `0x492010`, `0x492030`, `0x4920F0` (the bar draw kind 0xB0's
  `_StepBars` calls), `0x492260`, `0x492400`, `0x492580` (kind 0xAB's state
  1), `0x4925C0`, `0x4925E0`, `0x492620` (kind 0xAC's), `0x492680`,
  `0x492710` (kind 0xAD's states 0, 1), `0x492780` (kind 0xAE's state 0),
  `0x493E50` (kind 0xBA's state 1). They read and write the same records and
  pools; a later round taking part 6 inherits the tables named here.
- **Extents**: the tool's stand (read again by hand); the cut's sizes are the
  catalog's padding-inclusive guesses (30 differ by padding only; `0x492AA0`
  by the added function).
- **The cut's `unit_desc`** named the kinds right (in decimal); its labels
  ("Table Effect_KindHandlers") are the dispatchers; no row was a case, a
  shared tail or a second entry.
- **`hypothesis` rows** (`0x492510`, `0x492660`, `0x492760`, `0x492A50`,
  `0x492C80`, `0x492DC0`, `0x493550`, `0x493E10`, `0x493F70`): all effect code,
  taken.

## 10. The live route

The catalog's reach columns (attract, shop, worldmap, combat) are empty for
every row of the band, and no trace under `analysis/calltrace` (the `ab*`
runs, `all_ab.callcounts.tsv`) names any of the 49: **fuzz only**. A route
that shows them would walk area 188 (kind 0xB9), area 121 (0xBA) or area 198
(0xAC); the coordinator's frame-hash A/B covers one when the owner records it.

## 11. The rebinding

Raw references to this group's functions in our source (`band_rows.py
--refs`: 16 in four files), rebound in the round-ten form (the value
unchanged, so every fuzz key stands): `effect_3a_callees.h` `kSparkSpawn =
bof3::addr::EffectKindB9_SpawnSpark` and `kShardDraw =
bof3::addr::EffectKindB9_DrawShard`; `effect_1c_callees.h` `kCone =
bof3::addr::Effect_DrawEllipse`. Left raw, by design: the `CallSite` tables of
`effect_3a_fuzz.cpp` / `effect_1c_fuzz.cpp` (the clones' disassembly, raw by
convention) and their callee rows (keyed on the constants above, the same
values); comments naming the addresses.

## 12. For `analysis/calltrace/entries_logic.txt`

Appended in the main checkout (35 lines): every hidden start's extent, and
`00493370 B7`, `00493C60 1AD` beside the catalog's host lines `00493370 564`
and `00493C60 307`, which run over the hidden starts after them. The other
fourteen (`0x492B60` .. `0x493F70`) were already there with the extents read
here.
