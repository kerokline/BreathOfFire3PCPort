# Group E4D: effect kinds 0x91, 0x94..0x98, 0x9A and the screen tint

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave four, from the round branch's tip `9bebe7f`. **51 functions ours**
(`src/game/effect_4d.cpp`, shadow name `effect_4d`): the cut table's 51 rows
for E4D (`analysis/round13_cut.tsv`, the band `0x48C990..0x48DF70`), none
added, none dropped. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
204,000 rounds, 0 mismatches; 95 of 96 controls refused, the other an equivalent mutant whose near variant is refused. **Fuzz only**: no recorded
route enters any of the 51 (section 9). Every row is effect code (no
`hypothesis` row in the group; none left original).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x91: a full-screen tint brightened one step a frame over 0x3C frames, then message 0x86 opened; once it closes the chapter's step byte raised; the tint held (chapter 14's `Scena14_Run2` spawns it) | 5 | `Effect_KindHandlers[0x91]` (`0x655594`), `EffectKind91_States` `0x655100` (4) |
| the screen tint itself (`0x48CA90`), which kind 0x91 tail-jumps to and kinds 0x63 (E3B), 0x75 (E3C) and E4A's / E4C's states call | 1 | calls and tail jumps |
| 0x94: the tint faded in over 0x5A frames (blend 2) with the first six field sprites and the party members at `ObjTrio +0x89` 7 drawn again over it, held; or, spawned at `+1` 3 (`Scena14_Run6`), a white flash (blend 1) for four frames | 8 | `Effect_KindHandlers[0x94]` (`0x6555A0`), `EffectKind94_States` `0x655110` (5) |
| 0x95: a clock - the word `0x67629A` counted in ticks of 30 frames (40 with its bit 15) while no message, menu, mode 3 / 4 or party byte stops it; at 15 ticks the flag 0x40 set, the chapter's run 7 step 5, released | 4 | `Effect_KindHandlers[0x95]` (`0x6555A4`), `EffectKind95_States` `0x65512C` (3) |
| 0x96: a semi-transparent magenta quad (0, 0)..(320, 320), its level stepped through an eight-entry table every seventeen frames; never released by its own code | 3 | `Effect_KindHandlers[0x96]` (`0x6555A8`), `EffectKind96_States` `0x655138` (2) |
| 0x97: a grey ring of 32 triangles grown at the record's point; at the counter `0x903848` = 10 a burst of 32 debris triangles and rising spark rings, transitions 8 and 9, the chapter's step 0x14 (`Scena14_Run7` spawns it) | 15 | `Effect_KindHandlers[0x97]` (`0x6555AC`), `EffectKind97_States` `0x655140` (6) |
| 0x98: a grey full-screen flash rising over 0x20 frames, then a burst ring of 32 triangles at the record's point whose rim and then centre fade (`Scena14_EnterArea`'s and `Scena14_Run7`'s spawns) | 7 | `Effect_KindHandlers[0x98]` (`0x6555B0`), `EffectKind98_States` `0x655158` (4) |
| 0x9A: `Sprite_Objects` record 1 nudged 0x1000 along y or x (by `Rand & 7` through a table) for five frames and back, again until the counter `0x903848` is 0x28 | 9 | `Effect_KindHandlers[0x9A]` (`0x6555B8`), `EffectKind9A_States` `0x655170` (8) |

What each kind looks like in the game is not stated here (the owner's to
say); the descriptions are what the code draws. The spawners: a scan of the
image for `Effect_Spawn` calls with these kinds pushed and `mov byte [r +
5], imm8` stores finds only chapter 14's code (`Scena14_EnterArea`,
`Scena14_Run7`; ours in `scena_sc13.cpp`, which also spawns kinds 0x91 and
0x94 through `Fx`); kinds 0x95, 0x96 and 0x9A have no spawner the scan
finds (an event-script operand, perhaps).

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`); no PSX twin gives a name (section 5).

## 1. What each function does

### 1.1 Kind 0x91 (`EffectKind91_Run` `0x48C990`) and the tint

| Address | Name | What |
|---|---|---|
| `0x48C990` | `EffectKind91_Run` | `jmp [EffectKind91_States + +1 * 4]`, unbounded (hidden in E4C's `0x48C7F0`) |
| `0x48C9B0` | `EffectKind91_Start` | the colour `+0x5D..+0x5F` 0, `+9` = 0x3C, `+1` = 1 |
| `0x48C9F0` | `EffectKind91_Brighten` | `+9` down; not 0: each colour byte up 1; at 0 `Msg_OpenScript(0x86)`, `Field_Request` 2, `+1` = 2. Tail jump to the tint |
| `0x48CA50` | `EffectKind91_WaitMessage` | once `Field_Request` is not 2: the step byte `0x8034E5` up 1, `+1` = 3. Tail jump to the tint |
| `0x48CA80` | `EffectKind91_Hold` | a tail jump to the tint (5 bytes) |
| `0x48CA90` | `Effect_DrawScreenTint` | a draw mode (`Gpu_GetTPage(0, 2, 0x3C0, 0)`, dtd 1) committed at slot 5 (0xC); a semi-transparent untextured TILE (0, 0) 320.0 x 240.0 coloured `Sprite_Current +0x5D..+0x5F` (each read afresh), slot 5 (0x1C) |

### 1.2 Kind 0x94 (`EffectKind94_Run` `0x48CB30`)

| Address | Name | What |
|---|---|---|
| `0x48CB30` | `EffectKind94_Run` | the dispatcher (hidden in `0x48CA90`) |
| `0x48CB50` | `EffectKind94_Start` | colour 0, `+9` = 0x5A, `+1` = 1 |
| `0x48CB90` | `EffectKind94_Fade` | `+9` down; not 0: each colour byte 0xFF - (`+9` * 0xFF / 0x5A) (the `imul 0xB60B60B7` / `sar 6` quotient's low byte); at 0 `+1` = 2. `Effect_DrawScreenTintMode(2)`, `EffectKind94_RedrawSprites` |
| `0x48CC10` | `EffectKind94_Hold` | the tint at blend 2, a tail jump to the redraw |
| `0x48CC20` | `EffectKind94_Flash` | colour 0xFF, the tint at blend 1, `+9` = 4, `+1` = 4 |
| `0x48CC60` | `EffectKind94_FlashOut` | `+9` down, at 0 `Effect_Release`; the tint at blend 1 |
| `0x48CC90` | `Effect_DrawScreenTintMode` | `0x48CA90` with the page's blend mode the argument's low byte |
| `0x48CD40` | `EffectKind94_RedrawSprites` | `Sprite_Current` kept; the first six `Sprite_Objects` records made current in turn, `+0x29` = 2, `Sprite_UpdateScreen`; then each party member below `Field_MemberCount` (re-read after each draw) whose `ObjTrio +0x89` is 7 the same; `Sprite_Current` put back |

### 1.3 Kind 0x95 (`EffectKind95_Run` `0x48CDD0`)

| Address | Name | What |
|---|---|---|
| `0x48CDD0` | `EffectKind95_Run` | the dispatcher (hidden in `0x48CD40`) |
| `0x48CDF0` | `EffectKind95_Start` | the clock word `0x67629A` 0, `+1` up |
| `0x48CE10` | `EffectKind95_Clock` | held while `Field_Request` is 2, `Game_Mode` 4, the byte `0x80310F` (`ObjTrio` record 2's `+0x137` by its address) 2, `Field_Request` 1, `Game_Mode` 3 or `Input_Pressed & Field_MenuButton`; else the high byte's bits 0..6 (frames) up 1, at 0x1E (0x28 with bit 7 set) 0 and the low byte (ticks) up 1; at 15 ticks `+1` up |
| `0x48CEA0` | `EffectKind95_Finish` | held as the clock but on `0x802E77` (`ObjTrio` record 0's `+0x137`); else `ScriptFlags_Set40`, step `0x8034E5` = 5, `MoveScript_Var7` = 7, a tail jump to `Effect_Release` |

### 1.4 Kind 0x96 (`EffectKind96_Run` `0x48CF00`)

| Address | Name | What |
|---|---|---|
| `0x48CF00` | `EffectKind96_Run` | the dispatcher (hidden in `0x48CD40`) |
| `0x48CF20` | `EffectKind96_Start` | `+9` 0, `+1` = 1, the level `0x676298` = 0xF, the index `0x676299` 0 |
| `0x48CF50` | `EffectKind96_Pulse` | `+9` up; when it was 0x10 or more the level = `EffectKind96_Shades[index]`, `+9` 0, the index up (0 after 7). A draw mode (`Gpu_GetTPage(2, 2, 0x140, 0x140)`, dtd 1) at slot 6 (0xC); a semi-transparent POLY_G4 (0, 0), (320, 0), (0, 320), (320, 320) (floats), every corner (level, 0, level), at slot 2 (0x44). No state after it |

### 1.5 Kind 0x97 (`EffectKind97_Run` `0x48D070`)

| Address | Name | What |
|---|---|---|
| `0x48D070` | `EffectKind97_Run` | the dispatcher (hidden in `0x48CD40`) |
| `0x48D090` | `EffectKind97_Start` | the ring size `+0x2E` 0, `+9` = 8, `+1` up |
| `0x48D0B0` | `EffectKind97_Grow` | `+0x2E` up 0x18; the ring (`EffectKind97_DrawRing(+0x34, +0x2E, 7)`); `+9` down, at 0 `+9` = 0x40 and `+1` up |
| `0x48D100` | `EffectKind97_WaitCue` | the ring; at the counter `0x903848` = 10: E3A's `EffectKind64_ClearSparks`, `Gte_PushMatrix`, the 32 debris at `0x92C040` set up, `Gte_PopMatrix`; `+6` 0, `+0x5D` 8, `+9` = 0x78, `+1` up |
| `0x48D180` | `EffectKind97_Burst` | `+6` up, a spark every fourth frame; the ring; the debris drawn; the sparks moved; `+0x5D` up while below 0x40 (signed); every debris shade `+0x2A` = `+0x5D` >> 1 (signed); `+9` down, at 0 `Transition_Start(8)`, `+1` up |
| `0x48D220` | `EffectKind97_Fade` | a spark every fourth frame; the debris; the sparks moved; once `MoveScript_WaitWordDA` is 0 `Transition_Start(9)`, `Draw_PassFlags` 0, `+1` up |
| `0x48D270` | `EffectKind97_End` | once `MoveScript_WaitWordDA` is 0 the step `0x8034E5` = 0x14, a tail jump to `Effect_Release` |
| `0x48D490` | `EffectKind97_DrawRing` | (point, size, shade): a draw mode (`Gpu_GetTPage(0, 1, 0x2C0, 0x100)`) at slot 2; the point and the size (its low s16 in both words) projected; radius r = width + `Frame_Counter & 1`; 32 semi-transparent POLY_G3s from the centre, each from the last rim point (first (x + r, y)) to the next at 0x80 steps (`Math_Cos` / `Math_Sin` * r >> 12 added through the FPU), the depth at all three, the centre 0x40 per bit 2 / 1 / 0 of the shade byte (its callers pass 7: grey), the rim black; slot 2 (0x34) |
| `0x48D650` | `EffectKind97_EmitSpark` | the first free of the eight 0x18-byte sparks at `EffectKind30_Shards`: in use, life 0x20, at the record's point, size 0x180, centre shade the record's `+0x5D`, rim 0 |
| `0x48D6A0` | `EffectKind97_MoveSparks` | `EffectGte_LoadMapCamera`; each live spark shrunk 0xC, aged (out of use at 0) and drawn - also the frame it dies; al 1 when any was live (both callers ignore it) |
| `0x48D6F0` | `EffectKind97_DrawSpark` | (spark): a draw mode (`Gpu_GetTPage(0, 1, 0x3C0, 0)`) at slot 1; the size projected at the spark's point (section 7), then the point; 32 POLY_G3s from the centre to rim points at a and a + 0x80 (four `Math_*` calls a triangle); centre `+0x16`, rim `+0x17`; slot 1 (0x34) |
| `0x48D860` | `EffectKind97_DebrisInit` | (debris): E3C's `EffectDebris_InitOne`'s twin with Cos / Sin of 0x18 (not 0x20), a y angle `(Rand & 0x7FF) - 0x400` (not `Rand & 0x3FF`) and scale `4 + Rand & 3` (not `8 + Rand & 7`) |
| `0x48D980` | `EffectKind97_DrawDebris` | a draw mode (`Gpu_GetTPage(0, 1, 0x380, 0x100)`) at slot 1; `EffectGte_LoadMapCamera`; each of the 32 debris drawn, its angle `+0x24` up 0x10 |
| `0x48D9F0` | `EffectKind97_DebrisDraw` | (debris): E3C's `EffectDebris_Draw` instruction for instruction (`cmp.sh`, a diff of the two disassemblies) but the first corner's colour (v, v, v), where that one has (v, v, v >> 1) |

### 1.6 Kind 0x98 (`EffectKind98_Run` `0x48D290`)

| Address | Name | What |
|---|---|---|
| `0x48D290` | `EffectKind98_Run` | the dispatcher (hidden in `0x48CD40`) |
| `0x48D2B0` | `EffectKind98_Start` | the level dword `+0xC` 0, `+9` = 0x20, `+1` up, `Draw_PassFlags` 0 |
| `0x48D2E0` | `EffectKind98_Flash` | `+0xC` up 8, `EffectKind98_DrawFlash(+0xC)`; `+9` down, at 0 the radius `+0x32` = 0x140, `+0xC` and `+0x10` 0x100, `+9` = 0x10, `+1` up, `Draw_PassFlags` 0x1F |
| `0x48D360` | `EffectKind98_Rise` | `Gte_PushMatrix`, `EffectGte_LoadMapCamera`, the record's point (copied to a local) projected into its own `+0x74..+0x7F`, `Gte_PopMatrix`; the rim level `+0x10` down 0x10; `EffectKind98_DrawBurst(+0x32, +0xC, +0x10)`; `+9` down, at 0 `+9` = 0x10, `+1` up |
| `0x48D3F0` | `EffectKind98_Fall` | the same projection; the centre level `+0xC` down 0x10, the radius down 0x14; the burst; `+9` down, at 0 `Effect_Release` |
| `0x48DBA0` | `EffectKind98_DrawFlash` | (level): the low s16 clamped to 0..0xFF; a draw mode (`Gpu_GetTPage(0, 1, 0x380, 0x100)`) at slot 1; a semi-transparent grey TILE (0, 0) 320 x 240, slot 1 (0x1C) |
| `0x48DC40` | `EffectKind98_DrawBurst` | (radius, centre, rim): the two levels clamped to bytes; the draw mode at slot 1; 32 POLY_G3s round `Sprite_Current`'s projected point `+0x74` / `+0x78` (re-read at each use), rim points at a and a + 0x80, the depth `+0x7C` copied through the FPU (`fld` / `fst` / `fst` / `fstp`); slot 1 (0x34) |

### 1.7 Kind 0x9A (`EffectKind9A_Run` `0x48DDE0`)

| Address | Name | What |
|---|---|---|
| `0x48DDE0` | `EffectKind9A_Run` | the dispatcher (hidden in `0x48DC40`) |
| `0x48DE00` | `EffectKind9A_Start` | `+9` 0, `+1` = `EffectKind9A_Branches[Rand & 7]` (1 or 4) |
| `0x48DE30` | `EffectKind9A_NudgeY` | `Sprite_Current` kept, `Sprite_Objects` record 1 (`0x7DEF24`) made current, its `+0x38` up 0x400 four times (`Sprite_Current` re-read), `+1` = 2, put back, `+9` = 4 |
| `0x48DE70` | `EffectKind9A_HoldY` | `+9` down; when it was 0 `+1` = 3 |
| `0x48DE90` | `EffectKind9A_BackY` | record 1's `+0x38` down 0x1000 the same way; `+1` = 7, `+9` 0 |
| `0x48DED0` | `EffectKind9A_NudgeX` | record 1's `+0x34` up 0x1000; `+1` = 5, `+9` = 4 |
| `0x48DF10` | `EffectKind9A_HoldX` | `+9` down; when it was 0 `+1` = 6 |
| `0x48DF30` | `EffectKind9A_BackX` | `+0x34` down 0x1000; `+1` = 7, `+9` 0 |
| `0x48DF70` | `EffectKind9A_Repeat` | at the counter `0x903848` = 0x28 a tail jump to `Effect_Release`, else `+1` = 0 |

## 2. Divergence

**DIV-0041 (widescreen), already ledgered - no new entry.** Three of the
group's functions are among the full-frame sites DIV-0041's 2026-09-30
amendment and [`widescreen.md`](widescreen.md) §5 left Capcom's until their
group took them: `0x48CB07` in `Effect_DrawScreenTint`, `0x48CD10` in
`Effect_DrawScreenTintMode` and `0x48DC19` in `EffectKind98_DrawFlash`. Ours
draws each TILE at `(Widescreen_FillX(), 0)` `Widescreen_FillWidth()` x 240,
as E2B's `EffectKind38_DrawTint` and E2D's tint do: `Widescreen_Fill()` is 0
until `Widescreen_ArmFills` has run and whenever the picture is narrow, so the
fuzz compares the original's (0, 0) 320 x 240 and narrow play is bit for bit
Capcom's. The coordinator owes `widescreen.md` §5's table and DIV-0041's "not
yet" list the update (three sites now ours); this group edits neither (other
wave-four groups hold rows of the same table).

**Not widened**: `EffectKind96_Pulse`'s POLY_G4 (0, 0)..(320, 320) - its 320s
come from a register (`mov eax, 0x43A00000`), not one of DIV-0041's listed
operands, and it is a quad, not the TILE fills §3c widens. It stays the
original's; whether it wants widening is the owner's word (DIV-0041 leaves
fills "the scan cannot see" to his eye).

Otherwise none: `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no
other address of the band nor its tables (checked 2026-10-03; no operand
patch inside the band). Where the original indexes past a table ours aborts
with a `Fatal` naming the function (the round-nine rule; nothing in the fuzz
reaches it): the seven dispatchers past their tables, `EffectKind96_Pulse`
past its eight levels, `EffectKind94_RedrawSprites` past `ObjTrio`'s three
records. The x87 is used as the original uses it (inline `fild` / `fadd` and
`fld` / `fst` / `fstp`), under the game's own control word.

## 3. The tables

**The state tables** (`symbols.toml` `[[data]]`): the tables sit end to end
from `0x655100` (the dword before is not code), each its own length to the
next table or the data a state reads - checked against what the states store
into `+1` (`band_rows.py`'s runs continue across them: 9, 5, 15, 12, 10, 4, 26):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind91_States` `0x655100` | 4 | `0x655110`, kind 0x94's |
| `EffectKind94_States` `0x655110` | 5 | `0x655124`, `EffectKind96_Shades` (state 3 is set only by the spawner, `Scena14_Run6`) |
| `EffectKind95_States` `0x65512C` | 3 | `0x655138`, kind 0x96's |
| `EffectKind96_States` `0x655138` | 2 | `0x655140`, kind 0x97's (state 1 never advances) |
| `EffectKind97_States` `0x655140` | 6 | `0x655158`, kind 0x98's |
| `EffectKind98_States` `0x655158` | 4 | `0x655168`, `EffectKind9A_Branches` |
| `EffectKind9A_States` `0x655170` | 8 | `0x655190`, kind 0x9B's (`Effect_KindHandlers[0x9B]` `0x48DF90`, E4E's band) |

**The data read in place**, also named: `EffectKind96_Shades` `0x655124`
(eight bytes, the levels kind 0x96 steps through) and `EffectKind9A_Branches`
`0x655168` (eight bytes, each 1 or 4: the y or x nudge). Their bytes are not
copied here.

**The cells** (raw in `effect_4d_callees.h`, not named): kind 0x96's level
`0x676298` and index `0x676299`, kind 0x95's clock word `0x67629A` (only this
band's code touches the three - a scan of the image for the addresses as
operands finds no other); the bytes `0x802E77` / `0x80310F` inside `ObjTrio`;
the debris pool `0x92C040` (32 of 0x2C, after the eight sparks of 0x18 at
`EffectKind30_Shards`).

## 4. The fuzz (`effect_4d_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_4d`, effect mode (`g.effect`; kinds 0x91,
0x94..0x98, 0x9A, each clone its own, the two tints any), 4,000 rounds a
function (`BOF3X_E4D_ONLY=<name>` runs the clones whose name holds it).
Shapes: 44 `kEffect` (the seven dispatchers' `state_span` their table's
length), seven `kCall` (`Effect_DrawScreenTintMode`, the ring, spark and
debris helpers, the flash and burst draws). `ret_mask` 0xFF on
`EffectKind97_MoveSparks` (al; its callers ignore it). The seven tables are
`DataTable`s, swapped for recorders on both sides. **Regions** beyond effect
mode's standard ones: `0x676298` (4: kind 0x96's level and index, kind 0x95's
clock). Everything else the band touches is standard: the records,
`Sprite_Current`, `Sprite_Objects` (record 1's `+0x34` / `+0x38`, `+0x29`),
`ObjTrio` (`+0x29`, `+0x89`, `+0x137`), `Field_MemberCount`,
`EffectKind30_Shards` (the sparks and the debris), the chapter bytes, the
counter `0x903848`, `Field_Request`, `Game_Mode`, `Field_MenuButton`,
`Input_Pressed`, `MoveScript_WaitWordDA`, `Draw_PassFlags`, `Frame_Counter`,
the packet buffer.

**Callees**: the standard and effect-standard rows for `Effect_Release`,
`Msg_OpenScript`, `ScriptFlags_Set40`, `Transition_Start`, `Rand`,
`Math_Sin` / `Cos` (never 0 or -1), `EffectGte_LoadMapCamera` /
`_ProjectSize` (the point and the size's first word hashed, out two s16) /
`_SetDiagonalOne`, `Gte_PushMatrix` / `PopMatrix`, `Gte_RotMatrixX` / `Y` /
`Z`, `0x5A7C70`, `Sprite_UpdateScreen` (`Sprite_Current` and its 0x80 bytes
logged), `Gfx_CommitPrim`, the `Gpu_*`. **Re-listed**:
`EffectGte_ProjectPoint`, out filled as E3C's row does - fractional floats and
one time in eight a NaN (quiet or signalling) or an extreme: the ring, spark
and debris draws copy the projected depth with `mov`, and only a NaN tells
that from an FPU copy (controls 56 and 66). `EffectGte_ProjectSize` is not
re-listed: the ring writes both words of its size (equal), and the spark
draw's second word is the original's unwritten stack whose projection nobody
reads (section 7) - the standard first-word hash loses nothing. **The group's
own** called directly, listed by name: the two tints logging
`Sprite_Current` and its dword `+0x5C` (the colour), the tint's blend at a
byte; the redraw, the spark emitter and the debris pass as `kPhase`; the
mover as `kFlag`; the ring at (the point hashed 12 bytes, the size at s16,
the shade at a byte - its callers push a register whose upper half is left
over), the spark and debris draws handed a record hashed (0x18, 0x2C), the
debris set-up its record, the flash at s16, the burst at three s16 logging the
projected point `+0x74..+0x7F` it reads. **Others' by name**: E3A's
`EffectKind64_ClearSparks` (`kPhase`; nothing of it read after).

**Seeds** (per function, after the harness's per-round fill): on all 20
records `+9` at 0, 1, 2, 0xF, 0x10, 0x11, 0x5A, 0xFF or random, `+6` at its
fourth-frame boundary, `+0x5D` at 0x3F, 0x40, 0x7F, 0x80, the burst's
projected point (fractions, a NaN quiet or signalling a sixth of the time),
the levels `+0xC`, `+0x10`, `+0x32` at the clamps (-1, 0, 0xFF, 0x100,
0x7FFF, 0x8000, ...); `Field_MemberCount` 0..3 (ours aborts past three);
kind 0x96's index 0..7 (ours aborts past); the counter `0x903848` at 10 or
0x28 half the time, `MoveScript_WaitWordDA` 0 half the time; `Field_Request`
2 half the time for kind 0x91's wait; `ObjTrio +0x89` 7 often; kind 0x95's
holds one at a time (or none a third of the time), its clock's frames at
0x1D / 0x1E / 0x27 / 0x28 and ticks at 0xE / 0xF, either mode; the sparks'
`+0` free a third of the time and lives at 1; the debris shades at the
clamps. **Arguments**: the ring's point the record's `+0x34` (as its callers),
its shade 7 mostly; a spark or a debris record of its pool; the flash and
burst levels at the clamps over random upper halves. **Disturbance** (the
group's, from the hash only): `+9`, `+6`, `+0x5D` round 0x40, the counter at
its cues, kind 0x96's index inside its table, kind 0x95's clock, a word of the
projected point.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_4d`,
exit 0): 204,000 rounds over 51 functions, 3,083,928 calls to the stand-ins,
**0 mismatches**; 24,760 bytes of state in 46 regions; 401 stand-ins. Every
entry of the seven tables reached (each handler recorder 477..2,026 calls);
`Effect_Release` 4,891, `Msg_OpenScript` 436, `Transition_Start` 2,459,
`EffectKind64_ClearSparks` 915, `Sprite_UpdateScreen` 26,390.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
700 self-test lines, no `MISMATCH` line but the "0 MISMATCHES" counts,
`inject: 7838 ours, 0 left original`; `effect_4d` there 204,000 rounds,
3,087,037 calls, 0 mismatches. **With `BOF3X_WIDE=1`**: `'*'` exit 0, 700
self-test lines, no mismatch, the same counts. Neither run died silently.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 51 (5,208 bytes against the cut's
  5,526: 39 differ by padding only, none by code); each extent checked by hand
  to its `ret` or tail `jmp`. No shared tail, no case of a switch, no second
  entry among them.
- **Hidden starts**: 39, each an entry by address - a cell of
  `Effect_KindHandlers` or of a kind's state table - not a case. Their
  recorded hosts: E4C's `0x48C7F0` (kind 0x91's five), and this band's
  `0x48CA90`, `0x48CD40`, `0x48DC40`, whose catalog extents (`entries_logic`
  `1F7`, `741`, `460`) span the hidden starts after their `ret` (section 11).
  None of the hosts contains another's code as a fall-through: `0x48CA90`
  (ours) ends at `0x48CB2C`.
- **The kind dispatchers**: every `Effect_KindHandlers` cell pointing into the
  band is one of the seven rows (`[0x91]`, `[0x94]`..`[0x98]`, `[0x9A]`);
  `[0x92]` (`0x46A3E0`, ours), `[0x93]` (`0x48BFB0`) and `[0x99]` (`0x48C0E0`)
  point outside it (E4C's band). The tool lists no code of the band no list
  has.
- **PSX twins**: `0x48D3F0` (`0x801F3D24`, call-disputed) and `0x48DC40`
  (`0x801F4138`, call-anchored; the sibling's `names/area_records.toml` lists
  `0x801F4138` only as `AREA119.EMI`'s `handler[0]` entry). Both are AREA
  overlay copies: no name to transfer, none held.
- **The cut's `unit` / `label` columns**: kind numbers right; the two
  "Area overlays" labels (`0x48D3F0`, `0x48DC40`, `0x48DE00..`) are kind
  0x98's and 0x9A's code, resident on the PC.

## 6. Controls

Planted one at a time by a scratch script (`controls.py`: each plant anchored on a
unique string of `effect_4d.cpp`, rebuilt, run under `BOF3X_E4D_ONLY=<filter>`,
the file restored and rebuilt at the end; the committed file has no switch).
The harness stops at the first differing round, so the column is the round
that refused it (0-based) in this worktree; every refused run exited 3 on a
`MISMATCH` line. **95 of 96 refused**; the one not refused (82) is an
equivalent mutant: `EffectKind9A_Branches`' second four bytes repeat its first
four, so `Rand & 3` picks what `Rand & 7` picks - its near variant (96, `Rand &
6`) is refused. Two anchors first matched twice (16, 28) and were re-planted
on longer anchors; 46 first swapped in a shorter table, which ours' own bound
refused by a `Fatal` rather than a count, and was re-planted with kind 0x9A's
eight-entry table. The FPU-against-`mov` controls (56, 66, 76) are refused by
the NaNs the re-listed `EffectGte_ProjectPoint` and the burst's seeded point
put in.

| # | Run (`_ONLY`) | Plant | Refused at |
|--:|---|---|---|
| 1 | `EffectKind91_Run` | `EffectKind94_States` for `EffectKind91_States` | round 0 |
| 2 | `EffectKind91_Start` | `0x3D` for `0x3C` | round 0 |
| 3 | `EffectKind91_Brighten` | `2` for `1` | round 0 |
| 4 | `EffectKind91_Brighten` | `0x87` for `0x86` | round 28 |
| 5 | `EffectKind91_WaitMessage` | `2` for `3` | round 0 |
| 6 | `EffectKind91_Hold` | `Effect_DrawScreenTintMode)(2` for `Effect_DrawScreenTint)(` | round 0 |
| 7 | `Effect_DrawScreenTint` | `1` for `2` | round 0 |
| 8 | `Effect_DrawScreenTint` | `0x5F` for `0x5E` | round 0 |
| 9 | `Effect_DrawScreenTint` | `0x43710000u` for `0x43700000u` | round 0 |
| 10 | `Effect_DrawScreenTint` | `1` for `0` | round 0 |
| 11 | `EffectKind94_Start` | `0x59` for `0x5A` | round 0 |
| 12 | `EffectKind94_Fade` | `0x5Bu` for `0x5Au` | round 1 |
| 13 | `EffectKind94_Fade` | `3` for `2` | round 28 |
| 14 | `EffectKind94_Hold` | `1` for `2` | round 0 |
| 15 | `EffectKind94_Flash` | `5` for `4` | round 0 |
| 16 | `EffectKind94_FlashOut` | `!CountDown` for `CountDown` | round 0 |
| 17 | `Effect_DrawScreenTintMode` | `0x7Fu` for `0xFFu` | round 0 |
| 18 | `EffectKind94_RedrawSprites` | `3` for `2` | round 0 |
| 19 | `EffectKind94_RedrawSprites` | `6` for `7` | round 0 |
| 20 | `EffectKind94_RedrawSprites` | `5` for `6` | round 0 |
| 21 | `EffectKind95_Run` | `EffectKind96_States` for `EffectKind95_States` | round 0 |
| 22 | `EffectKind95_Start` | `1` for `0` | round 0 |
| 23 | `EffectKind95_Clock` | `0x1Fu` for `0x1Eu` | round 28 |
| 24 | `EffectKind95_Clock` | `0x27u` for `0x28u` | round 1883 |
| 25 | `EffectKind95_Clock` | `0x10` for `0xF` | round 27 |
| 26 | `EffectKind95_Clock` | `2` for `3` | round 37 |
| 27 | `EffectKind95_Clock` | `if (request == 1) return true;` dropped | round 1 |
| 28 | `EffectKind95_Finish` | `6` for `7` | round 0 |
| 29 | `EffectKind95_Finish` | `kTrio2Mode` for `kTrio0Mode` | round 18 |
| 30 | `EffectKind96_Start` | `0xE` for `0xF` | round 0 |
| 31 | `EffectKind96_Pulse` | `6` for `7` | round 12 |
| 32 | `EffectKind96_Pulse` | `level` for `0` | round 0 |
| 33 | `EffectKind96_Pulse` | `0x11` for `0x10` | round 20 |
| 34 | `EffectKind96_Pulse` | `0x43700000u` for `k320` | round 0 |
| 35 | `EffectKind96_Run` | `EffectKind95_States` for `EffectKind96_States` | round 0 |
| 36 | `EffectKind97_Start` | `9` for `8` | round 0 |
| 37 | `EffectKind97_Grow` | `0x19u` for `0x18u` | round 0 |
| 38 | `EffectKind97_Grow` | `0x41` for `0x40` | round 28 |
| 39 | `EffectKind97_WaitCue` | `9` for `8` | round 0 |
| 40 | `EffectKind97_WaitCue` | `SH_CALL(EffectKind64_ClearSparks)();` dropped | round 0 |
| 41 | `EffectKind97_Burst` | `shade <= 0x40` for `shade < 0x40` | round 4 |
| 42 | `EffectKind97_Burst` | `2` for `1` | round 0 |
| 43 | `EffectKind97_Burst` | `7` for `8` | round 6 |
| 44 | `EffectKind97_Fade` | `1` for `0` | round 0 |
| 45 | `EffectKind97_End` | `0x15` for `0x14` | round 0 |
| 46 | `EffectKind97_Run` | `EffectKind9A_States` for `EffectKind97_States` | round 0 |
| 47 | `EffectKind98_Start` | `0x21` for `0x20` | round 0 |
| 48 | `EffectKind98_Flash` | `9u` for `8u` | round 0 |
| 49 | `EffectKind98_Flash` | `0x1E` for `0x1F` | round 28 |
| 50 | `EffectKind98_Rise` | `0x11u` for `0x10u` | round 0 |
| 51 | `EffectKind98_Fall` | `0xFFEDu` for `0xFFECu` | round 0 |
| 52 | `EffectKind98_Rise` | `0x10), Word(s + 0xC` for `0xC), Word(s + 0x10` | round 0 |
| 53 | `EffectKind97_DrawRing` | `r + 1` for `r` | round 0 |
| 54 | `EffectKind97_DrawRing` | `0x40` for `0x80` | round 0 |
| 55 | `EffectKind97_DrawRing` | `5` for `6` | round 0 |
| 56 | `EffectKind97_DrawRing` | the depth copied through the FPU (`FldCopy3`) for the three `mov`s | round 5 |
| 57 | `EffectKind97_EmitSpark` | `0x181` for `0x180` | round 0 |
| 58 | `EffectKind97_EmitSpark` | `!= 1` for `== 0` | round 0 |
| 59 | `EffectKind97_EmitSpark` | `0x5E` for `0x5D` | round 0 |
| 60 | `EffectKind97_MoveSparks` | `0xFFF5u` for `0xFFF4u` | round 0 |
| 61 | `EffectKind97_MoveSparks` | `1` for `0` | round 0 |
| 62 | `EffectKind97_MoveSparks` | `1` for `any` | round 343 |
| 63 | `EffectKind97_DrawSpark` | the size word + 1 | round 0 |
| 64 | `EffectKind97_DrawSpark` | `centre` for `rim` | round 0 |
| 65 | `EffectKind97_DrawSpark` | `2` for `1` | round 0 |
| 66 | `EffectKind97_DrawSpark` | the depth copied through the FPU for the three `mov`s | round 0 |
| 67 | `EffectKind97_DebrisInit` | `5u` for `4u` | round 0 |
| 68 | `EffectKind97_DebrisInit` | `0x19` for `0x18` | round 0 |
| 69 | `EffectKind97_DebrisInit` | `0x3FFu` for `0x400u` | round 0 |
| 70 | `EffectKind97_DrawDebris` | `0x11u` for `0x10u` | round 0 |
| 71 | `EffectKind97_DrawDebris` | `2` for `1` | round 0 |
| 72 | `EffectKind97_DebrisDraw` | `static_cast<unsigned char>(v >> 1)` for `v` | round 1 |
| 73 | `EffectKind97_DebrisDraw` | `11` for `12` | round 0 |
| 74 | `EffectKind98_DrawFlash` | `int32_t` for `int16_t` | round 0 |
| 75 | `EffectKind98_DrawFlash` | `static_cast<unsigned char>(v + 1)` for `v` | round 0 |
| 76 | `EffectKind98_DrawBurst` | the depth copied by three `mov`s for the FPU copy | round 0 |
| 77 | `EffectKind98_DrawBurst` | `rim` for `centre` | round 0 |
| 78 | `EffectKind98_DrawBurst` | `0x81` for `0x80` | round 0 |
| 79 | `EffectKind98_DrawBurst` | `0x74` for `0x78` | round 0 |
| 80 | `EffectKind9A_Run` | `EffectKind97_States), EffectKind97_States_count` for `EffectKind9A_States), EffectKind9A_States_count` | round 0 |
| 81 | `EffectKind9A_Start` | `1` for `0` | round 0 |
| 82 | `EffectKind9A_Start` | `3u` for `7u` | **not refused**: equivalent (below) |
| 83 | `EffectKind9A_NudgeY` | `5` for `4` | round 0 |
| 84 | `EffectKind9A_NudgeY` | `3` for `4` | round 0 |
| 85 | `EffectKind9A_HoldY` | `2` for `3` | round 5 |
| 86 | `EffectKind9A_BackY` | `0x34` for `0x38` | round 0 |
| 87 | `EffectKind9A_NudgeX` | `3` for `4` | round 0 |
| 88 | `EffectKind9A_HoldX` | `5` for `6` | round 5 |
| 89 | `EffectKind9A_HoldX` | `s[9]` for `v` | round 5 |
| 90 | `EffectKind9A_BackX` | `6` for `7` | round 0 |
| 91 | `EffectKind9A_Repeat` | `1` for `0` | round 0 |
| 92 | `EffectKind9A_NudgeY` | `Sprite_Current = kept;` dropped | round 0 |
| 93 | `EffectKind94_RedrawSprites` | `Sprite_Current = saved;` dropped | round 0 |
| 94 | `EffectKind98_Run` | `EffectKind97_States` for `EffectKind98_States` | round 0 |
| 95 | `EffectKind94_Run` | `EffectKind91_States), EffectKind91_States_count` for `EffectKind94_States), EffectKind94_States_count` | round 0 |
| 96 | `EffectKind9A_Start` | `6u` for `7u` | round 3 |

## 7. Latent defects (Capcom's, described, not fixed)

- **The spark draw projects a size whose second word is never written.** (D213)
  `EffectKind97_DrawSpark` writes one s16 of its two-word size local
  (`mov [esp + 0x38], cx`) before `EffectGte_ProjectSize`, which also reads
  the second and writes `out[1]` from it; the draw keeps only `out[0]` (the
  dword's add of `Frame_Counter & 1` is truncated back to its low s16). So the
  stale word changes nothing drawn; ours hands the first word in both (a round
  size), which is the nearest sensible value and draws the same. No
  `DIVERGENCE.md` entry is needed for a value nobody reads; named here for the
  coordinator.
- **Kind 0x96 never ends.** (D202) `EffectKind96_Pulse` is its last state and never
  advances `+1` nor releases; its quad covers 320 x 320 - 80 rows below the
  240-row frame (harmless, clipped). No spawner of it is found (the
  table at the top).
- **Unchecked indexes** (D200): the seven dispatchers do not bound `+1` (every writer
  in the band keeps it in its table; kind 0x94's state 3 is the spawner's);
  kind 0x96's index (kept 0..7 by its own wrap); `Field_MemberCount` into
  `ObjTrio` (three records). Ours aborts past any.
- **Kind 0x91 writes `Field_Request` 2 itself** (D238) after `Msg_OpenScript`, and
  kind 0x95's two states test different party bytes (`0x80310F`, record 2's
  `+0x137`, in the clock; `0x802E77`, record 0's, in the finish) - as read,
  perhaps deliberate.
- **The debris pass draws all 32 every frame** (D214) whether set up or not:
  `EffectKind97_Fade` draws them after `_Burst`; nothing clears the pool.

## 8. Calls across groups

**Outbound**: none raw. By name, already ours: E3A's
`EffectKind64_ClearSparks` (the one `--edges` call into another group of the
round; E3A is merged), EGT's four, `Effect_Release`, `Msg_OpenScript`,
`ScriptFlags_Set40`, `Transition_Start`, `Sprite_UpdateScreen`, `Rand`,
`Gfx_CommitPrim`, the `Gpu_*` / `Gte_*` / `Math_*` primitives. By address and
nobody's: `0x5A7C70` (the library's matrix turn; an effect-standard row).

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `EffectKind63_Shrink` `0x482550` (`0x48257D`), `_WaitCue` `0x4825D0` (`0x4825EC`), `_FadeOut` `0x482600` (`0x48263E`) | E3B (ours) | `Effect_DrawScreenTint` - `effect_3b_callees.h` `kScreenTint`, rebound |
| `EffectKind75_Brighten` `0x485C00` (`0x485C3E`), `_Hold` `0x485C50` (`0x485C50`) | E3C (ours) | `Effect_DrawScreenTint` - `effect_3c_callees.h` `kScreenTile`, rebound |
| `0x488A50`, `0x488A80`, `0x488AE0`, `0x488B70` | E4A (wave four) | `Effect_DrawScreenTint` - E4A's files, the coordinator's |
| `0x48B280`, `0x48B290` (tail jumps) | E4C (wave four) | `Effect_DrawScreenTint` - E4C's files, the coordinator's |

`Effect_RunObjects` reaches the seven dispatchers through
`Effect_KindHandlers` (read in place).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 51 rows, and no first-call trace under
`analysis/calltrace` names any of the 51 addresses (a grep of every trace;
the only hits are hash columns). **Fuzz only.** No live run was made (the
brief). Chapter 14's scenes (`Scena14_Run2`, `_Run6`, `_Run7`, `_EnterArea`) spawn
kinds 0x91, 0x94, 0x97 and 0x98; a recorded route through them would let
the coordinator's frame-hash A/B cover those kinds, and the screen tint
whichever of kinds 0x63 / 0x75 the route shows.

## 10. The rebinding

`grep -rn -i` of the 51 addresses and the seven tables in `src/game`
(`band_rows.py --refs`: 13 references, all to `0x48CA90`). **Rebound** (the
value unchanged, so the fuzz keys stand; the one line each, with the
`bof3/symbols.gen.h` include the headers lacked):
`effect_3b_callees.h` `kScreenTint = bof3::addr::Effect_DrawScreenTint` and
`effect_3c_callees.h` `kScreenTile = bof3::addr::Effect_DrawScreenTint`.
**Left raw**: E3B's and E3C's fuzz files (their `CallSite` tables and the
callee rows `E3B_RAW(0x48CA90)` / `"0x48CA90 (E4D)"` - keys and labels);
the comments in `effect_3b.cpp`, `effect_3c.cpp` and `effect_3b_fuzz.cpp`
that cite `0x48CA90` by address (still right).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 42 lines, the read extents
of the 51 less nine already listed right (`0x48CC90`, `0x48D490`..`0x48DBA0`).
Three correct host lines left in place: `0048CA90 1F7` (the code is 0x9D),
`0048CD40 741` (0x89) and `0048DC40 460` (0x195) - each old line spans the
hidden starts after its host's `ret`.
