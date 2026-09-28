# World 1, areas 42..47: a timed round, two gates, the world map's third copy

**Status:** IN PROGRESS (2026-09-27) - 55 functions ours
(`src/game/area_w1b.cpp`, shadow name `area_w1b`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 258,000 rounds; CONTROLS_LINE (section 10). Fuzz only: the
world-map route reaches none of the 55 (section 9). No divergence.

Group AR1B of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 9): the band
`0x406650..0x408FF0`, whole areas as `tools/area_rows.py --groups` cut them.
What each area *is* in the story is not read here: the names come from what
the code does.

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` (group `AR1B`) lists 56 starts, one ours
(`WorldMap_DrawNeedle` `0x408530`, round seven's, which lies in area 45's
block). All 55 others are functions and all are taken: **no start dropped,
none added**. Every one was read to its last instruction with capstone (the
scratch `adis.py`); every gap between them is `nop` padding. The tool's
clone rows (`--unit AREA042..047 --clones`) match the reading call site for
call site, jump table for jump table; area 47's were printed after its two
functions were bound (the tool then shows them ours and moves area 47 to
AR1A's cut: its band is still AR1B's), so its rows were written by hand from
the disassembly, and area 45's are area 16's rows at area 45's addresses
(section 5).

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 42 | `0x5F6290` | `Area42_Handlers` (7, the `+0x34` choice table its tail), init, step hook, tail kind 7 | `0x406650..0x406F72` | 11 |
| 43 | `0x5F6A28` | `Area43_Handlers` (3), `Area43_States` (2), object trigger 58 | `0x406F80..0x407202` | 6 |
| 44 | `0x5F7030` | init, cell hook (`Area_CellHooks`), tail kinds 8 and 9 | `0x407210..0x407B38` | 6 |
| 45 | `0x5F74A0` (only a choice table, `+0x34` -> `0x41D270`, outside the band) | `WorldMap_Records` record 2 (`+0`, `+4`, `+8`, `+0xC`, `+0x10`), `WorldMap_FieldHooks` entry 2, six state tables in its data block | `0x407B40..0x408E01` | 26 (and `WorldMap_DrawNeedle`) |
| 46 | `0x5F7E58` | `Area46_Handlers` (1), step hook, tail kind 11, object trigger 54 | `0x408E10..0x408F5F` | 4 (one shared) |
| 47 | `0x5F8330` | `Area47_Handlers` (2) | `0x408F60..0x408FE7` | 2 |

**Jump tables inside functions** (the clones move them; the byte index
tables beside them are read at their original addresses, constants):
`Area42_TimerTail` (11 entries at `0x406DAC`), `Area44_GateTailA` (a byte
table of 21 at `0x4077CC`, six entries at `0x4077B4`), `Area44_GateTailB`
(`0x407B24`, `0x407B0C`), `Area46_DropTail` (12 at `0x408EA0`, five at
`0x408E8C`). Every other jump is internal.

**The tool's two known gaps** (round10 doc section 7), looked for:

- **A start with no padding or `ret` byte before it:** none in this band
  (every start follows `nop` padding or a `ret`).
- **A tail kind armed through a register:** yes - **`Area44_SwitchHook`**
  arms kinds 8 and 9 with `mov cl, [esi*5 + 0x5F7086]; mov [0x9039F3], cl`
  (the kind is the switch record's fifth byte). So the tool's "tail kind 8"
  (`0x4075D0`) and "tail kind 9" (`0x407940`), listed as gaps "reached by no
  area" and placed by address, are **area 44's**, reached through its cell
  hook; and `0x4077F0` (a gap too) is theirs, called by both. The same
  scan explains ART's note that slots 8 and 9 are "stored by code that
  computes the kind" ([`area-rows.md`](area-rows.md) section 6).

**Shared bodies** (keyed by address, taken once):

| PC | Name | Reached by |
|---|---|---|
| `0x408990` | `Area45_Record4Tick` | the record `+4` state 1 of ten world maps: areas 16, 33, 45, 65, 87, 88, 115, 121, 151, 152 (their record-4 state tables name it; AR0B's `Area16_Record4States` and AR0C's `WorldMap33_Record04States` read it raw) |
| `0x408F10` | `Area46_PlaceKind2At0` | handler 0 of areas 7, 14 and 46 (AR0A's `Area07_Handlers` names it raw) |

**Gap functions** (reached by no descriptor field of their own area):
`0x4071E0` (object trigger 58) and `0x408F20` (object trigger 54), placed by
address in areas 43 and 46; `0x4075D0`, `0x4077F0`, `0x407940` (area 44's,
above).

## 2. Area 42: a timed round

Area 42's handlers set four story flags against a countdown the mode tail
draws. Handler 0 starts it; handlers 2..5 each set one of flags `0x12..0x15`;
when all four are set before the time runs out, the time is shown, a rank
0..2 kept by the seconds taken, and a field object of that rank animated;
the step hook calls it all off at two rows of the map.

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x406650` | `Area42_ChoiceMessage` | `0x17` | choice 0, handler 6 | `0x801F3D90` | message `Area42_Messages[(s8) choice]` |
| `0x406670` | `Area42_StartTimer` | `0x78` | handler 0 | `0x801F3DBC` | `Field_ActiveMember +0x80` bit 0 cleared; when `MoveScript_EffectState[leader +0x89]` is 1 and flags `0xB` and `0x11` are clear: flag `0xB`, `Field_ScriptFlags` bit 5 (a byte `or`), tail kind 7 armed in state `0xA` with the countdown word `0x9039F6 = 0x384` (900 frames) |
| `0x4066F0` | `Area42_ClearMemberFlag` | `0x1D` | handler 1 | `0x801F3E80` | `Flags_Clear` of flag `Field_ActiveMember +0xA0` + `0x10` (a byte) |
| `0x406710`..`0x406860` | `Area42_Touch12`..`Touch15` | `0x68` each | handlers 2..5 | `0x801F3EB8`, `F6C`, `4020`, `40D4` | one shape, flags `0x12..0x15`: `+0x80` bit 0 cleared; when the effect state is 1 and the flag clear: the flag set, `Area42_CheckAll`, and unless flag `0xB` the member's script word `+0x8A` + 1 |
| `0x4068D0` | `Area42_CheckAll` | `0x159` | called by the four | (`0x801F4188`, area_pairs) | al 0 unless `0x12..0x15` set and `0x11` clear; then `ScriptFlags_Set40`, the tail to state 0 with `0x9039F5 = 0x3C` (the time shown 60 frames), `Area42_Rank` = 2 / 1 / 0 for the seconds' low byte above / at / below 5; the first field object with `+0` bit 0, `+6 == 9`, `+5` in `0x5C..0x5E` whose flag `0x5C + rank` of the bank `0x9040CC` is set gets `Sprite_SetAnimation(1)` (the rank re-read after each miss); al 1 |
| `0x406A30` | `Area42_TimerTail` | `0x3A8` | tail kind 7 | (`0x801F4A00`) | the switch below |
| `0x406DE0` | `Area42_StepHook` | `0xA1` | step hook | (`0x801F4AF4`) | `(x, z)` 16.16: `z == 0x308000` with x's column `0x7B..0x7C`, or `z == 0x368000` with `0x1D..0x1F`: flags `0x11`, `0xB`, `0x12..0x15` cleared, `Field_ScriptFlags` bit 5 cleared, the tail ended if kind 7 is armed; al 0 always |
| `0x406E90` | `Area42_Init` | `0xE3` | init | `0x801F5070` | `Cond_ByteFD` 0: the rank from the bank `0x9040CC` (the first of flags `0x5C..0x5E` clear, 2 when none; skipped once flag `0x11` is set), a free field object from `0x57CD90` to the word `0x903850`, `EventOp_9x(Area42_Placements + rank * 13)`; `Cond_ByteFD` 1: an effect of kind `0x28`, `+6 = 2`, `+0xC` / `+0x10` = `0x418000` / `0x108000`, `+0x18` / `+0x1C` = `0x4D8000` / `0x108000` |

**`Area42_TimerTail`**, by the s8 state `0x9039F4`:

| State | Does |
|--:|---|
| 0 | while `0x9039F5` is not 0: the time drawn - window `0x40E750(0x7C, 0x2E, 0x48, 0x11, 0)`, `Crt_sprintf(0x904BA0, 0x5F6308, seconds, hundredths)` (seconds = word / 30, hundredths = word % 30 * 10 / 3), `Text_DrawAt(0x82, 0x30, 2 below ten seconds else 0, 5, 0x904BA0)` - and `0x9039F5` - 1; at 0, as 1 |
| 1 | `Field_Kind2X` / `Z` = cell `(0x15, 0x19)`, `MoveScript_F3Divisor` `0x40`, state 2 |
| 2 | once `Field_Kind2Hold` is 0: `MoveScript_F3Divisor` 0, an effect of kind `0x13` (`CameraTurn_Run`: `+0x64` the target -770, `+0x68` / `+0x6C` `Camera_Angles[1]` / `[2]`, `+9` 16 frames) at `Effect_FindFree`'s slot, kept in `0x9039F5`; state 3 |
| 3 | once that record's `+0` bit 0 is clear: flag `0x11`, state 9 |
| 4 | a turn to -682 (no hold test), state 5 |
| 5 | once it ends: `Field_Kind2X` / `Z` = the leader's position, divisor `0x40`, state 6 |
| 6 | once `Field_Kind2Hold` is 0: flag `0xB` cleared, `ScriptFlags_Clear40`, bit 5 cleared, divisor 0, the tail ended (`0x9039F3`, `0x9039F4`, the word `0x9039F6` zeroed) |
| 10 | the countdown: the time drawn with `Rand() & 3` added to the hundredths (after the window), its colour `(word's low byte >> 1) & 2` below ten seconds (blinking); the word - 1, or at 0 flags `0xB`, `0x12..0x15` cleared, bit 5 cleared, the tail ended |
| 7..9 | nothing (9 is where state 3 leaves it; the script moves it on) |

The two camera turns and the effect records' fields are read off the code;
what the round *is* is not.

Tables: `Area42_Handlers` `0x5F6274` (7), `Area42_Messages` `0x5F62D4` (6
words), `Area42_Placements` `0x5F62E0` (three 13-byte event-op `9x`
records), the format `0x5F6308`, `Area42_Rank` `0x675A00` (a byte in
`.data`).

## 3. Area 43

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x406F80` | `Area43_ObjectRun` | `0x12` | handler 0 | `0x801F2C04` | `jmp [Area43_States + Sprite_Current[4] * 4]` |
| `0x406FA0` | `Area43_TintStart` | `0x77` | `Area43_States` 0 | (`0x801F2C48`) | `Field_ActiveMember +0x9F = Sprite_SetTint(Sprite_Current, 0, 0, 0, 1)`; `+0` bit 5; the tint bytes `+0x5D..+0x5F` = `0xC0`; `+0x5C = 1`, `+0x48 = 2`, `+4 = 1`; the member's script word `+0x8A` - 2 (the op runs again next frame) |
| `0x407020` | `Area43_TintSettle` | `0xA7` | ... 1 | (`0x801F2D0C`) | each tint byte not yet `0x80` less 2; `+0x40` / `+0x44` + `0x800`; all three at `0x80`: `Tint_Release(member +0x9F)`, `+0` bit 6, `+4 = 0`; else `+0x8A` - 2 |
| `0x4070D0` | `Area43_SpawnEffect37` | `0x68` | handler 1 | `0x801F2E28` | an effect of kind `0x37` at the running object, `0x80` higher (`+0x3C` + `0x800000`), word `+0x2C = 0x64`, `+9 = 1` |
| `0x407140` | `Area43_SetUpObject` | `0x92` | handler 2 | `0x801F2FBC` | `Sprite_ReleaseTint`, bank `0x1BD`, animation 0, scale `0xE000`, a new tint at `0x14` |
| `0x4071E0` | `Area43_Trigger58` | `0x23` | object trigger 58 | | flag `0x7F`, `MoveCmd_TestFB(0x44, 0xB)` (answer not read), sound `0x103` |

Tables: `Area43_Handlers` `0x5F6A18` (3), `Area43_States` `0x5F6A6C` (2).

## 4. Area 44: two gates, four switches

Two extra objects stand in two gates of map cells; four floor switches
(the cell hook) each move one gate and push the party member standing on
its object along with it. The gate byte `0x90384B` holds gate A in bits
0..1 and gate B in bits 4..5.

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x407210` | `Area44_Init` | `0x10B` | init (PSX `0x801F2CB0`) | `Cond_ByteFD` 0 or 2: the gate bytes zeroed, flags `0x16` / `0x17` cleared. 1: extra objects 0 and 1 at `Area44_Gate0[A]` / `Area44_Gate1[B]` (cell centres), `+0x3E = 0x80`, `+0x80` bit 7; `Area44_SetGates`. Other values: nothing |
| `0x407320` | `Area44_SetGates` | `0x22E` | called | the two gates as 24 `AreaMap_SetByte` calls: each gate position opens (0) one pair of cells two wide and closes (`0x10`) the other two; A over x `0x3C..0x41` rows `0xA..0xF`, B over x `0x3A..0x3F` rows `8..0xD` |
| `0x407550` | `Area44_SwitchHook` | `0x7C` | cell hook | the first of `Area44_Switches`' four records whose cell and facing (`& 0xF`, the leader's `+8`) match: `0x57C160` toggles its flag (`0x16` or `0x17`), sound `0x200`, `ScriptFlags_Set40`, tail kind = the record's (8 or 9) in state 0; al 1, else 0 |
| `0x4075D0` | `Area44_GateTailA` | `0x211` | tail kind 8 | flag `0x16`'s switch: moves gate A (state 0 -> 1 -> 10), then gate B (10 -> 11 -> 20) - each move `Area44_PushParty`, sound `0x201` (`0x20E` for the second of two), the gate byte updated, then waits for the object's slide (`+9`) and `Area44_SetGates`; state 20 ends the tail |
| `0x4077F0` | `Area44_PushParty` | `0x143` | called | `(direction, k)`: extra object k's step `Field_DirectionSteps[direction] >> 2`, 32 frames; every party member standing on it (`Sprite_FindNearby` answering `0x1E + k`) finishes its own step and takes the object's. `Sprite_Current` is left at the last member |
| `0x407940` | `Area44_GateTailB` | `0x1F9` | tail kind 9 | flag `0x17`'s switch: gate B first, gate A second, the same shape |

Gate A's positions 0 / 1 / 2 and B's: A 0 -> 1 (direction 3) or 1 -> 0 (7)
on flag `0x16`; B 0 -> 1 (1) or 1 -> 0 (5) on flag `0x16`; B 0 -> 2 (7) or 2
-> 0 (3) and A 0 -> 2 (5) or 2 -> 0 (1) on flag `0x17` (the directions
index `Field_DirectionSteps`).

Tables: `Area44_Gate0` `0x5F7074` (4 cells), `Area44_Gate1` `0x5F707C` (its
fourth cell is `Area44_Switches`' first record's x, z), `Area44_Switches`
`0x5F7082` (4 x 5 bytes).

## 5. Area 45: the world map's third copy

Area 45 is `WorldMap_Records` record 2 ([`worldmap_area.md`](worldmap_area.md)
section 5; `WORLD01/AREA045`, [`world-map.md`](world-map.md)). **Its code is
area 16's, instruction for instruction, over its own tables**: a capstone
compare of the 25 pairs (the scratch `cmp.py`: the same instruction counts
and mnemonics in every pair) finds only jump targets, the calls to its own
copies and the table operands differing, **and two operands more**:

- `Area45_PlateStart` `0x407DA0` pushes bank `0x3B` where area 16's (which
  is also area 33's state 0) pushes `0xF` - which is why area 45 has a plate
  start of its own;
- `Area45_DrawHud` `0x4086D0` reads the region label's offset from the dword
  `0x803584`, area 16's from `0x803588`.

So [`area_w0b.md`](area_w0b.md) section 2 (and [`worldmap_area.md`](worldmap_area.md)
sections 2..4, [`world-map-hud.md`](world-map-hud.md)) describe area 45 with
these tables, and ours is area 16's code over them, with one difference of
policy: the six state dispatchers abort past their tables (section 8).

| Area 45 | Area 16's | Size |
|---|---|--:|
| `Area45_PlaceMessage` `0x407B40` | `0x401B80` | `0x174` |
| `Area45_PlateRun` / `Start` / `Show` / `Grow` / `Hold` / `Shrink` `0x407CC0..0x407FF0` | `0x401D00..0x402030` | `0xD6`, `0x4E`, `0x142`, `0x41`, `0x58`, `0x50` |
| `Area45_HudRun` / `HudFrame` `0x408040` / `0x408060` | `0x402080` / `0x4020A0` | `0x12`, `0xA` |
| `Area45_FrameStep` / `SlideIn` / `Hold` / `SlideOut` `0x408070..0x4080F0` | `0x4020B0..0x402130` | `0x12`, `0x21`, `0x28`, `0x41` |
| `Area45_BoxStep` / `SlideIn` / `Hold` / `SlideOut` `0x408140..0x408240` | `0x402180..0x402280` | `0x12`, `0x62`, `0x6E`, `0x57` |
| `Area45_DrawFrame` / `DrawSprite` `0x4082A0` / `0x408470` | `0x4022E0` / `0x4024B0` | `0x1C5`, `0xBC` |
| `WorldMap_DrawNeedle` `0x408530` (ours, round seven) | - | `0x196` |
| `Area45_DrawHud` `0x4086D0` | `0x402570` | `0x58` |
| `Area45_Record8Run` / `Record8Place` `0x408730` / `0x408750` | `0x4025D0` / `0x4025F0` | `0x12`, `0x154` |
| `Area45_Record4Run` / `Record4MarkCell` `0x4088B0` / `0x4088D0` | `0x402750` / `0x402770` | `0x12`, `0xB5` |
| `Area45_Record4Tick` `0x408990` (shared by ten maps) | - | `0xA` |
| `Area45_DrawDrift` `0x4089A0` | `0x402830` | `0x462` |

**`Area45_Record4Tick` `0x408990`**: `call Sprite_ScriptTick` (al not read),
`jmp Sprite_UpdateScreenSlot` - the record `+4` effect's running state (its
state 0 marks a cell a place, then this animates it).

**Area 45's tables** (`[[data]]`, in its data block after area 46's
descriptor - the tool moves its 20 code pointers to area 45; checked: each
is read only by area 45's code): `Area45_PlateAnims` `0x5F7098` (10),
`Area45_Cells` `0x5F70C0` (265 x 4), `Area45_PlaceMessages` `0x5F74E4` (11
rows), `Area45_NameSets` `0x5F7644` (3 x 5), `Area45_PlateStates` `0x5F7654`
(5), `Area45_HudStates` `0x5F7668` (2), `Area45_FrameStates` `0x5F7670` (4),
`Area45_BoxStates` `0x5F7680` (4), `Area45_Sprites` `0x5F7690` (22 x 4),
`Area45_Buttons` `0x5F76E8` (6 x 4; the second legend reads eight),
`Area45_Record8States` `0x5F7700` (3), `Area45_Directions` `0x5F770C` (4 x 2
words), `Area45_Record8Anims` `0x5F771C` (4 x 2), `Area45_Record4States`
`0x5F7724` (2), `Area45_DriftUV` `0x5F772C` (12). The record-8 table's first
and third entries are `0x4253C0` and `0x40C490` (other groups', shared by
the copies); the HUD, frame and box tables' state 0 entries are round
eight's shared `WorldMapHud_Start`, `WorldMap_FrameWait`,
`WorldMapHud_BoxWait`.

## 6. Area 46

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x408E10` | `Area46_DropTail` | `0x9C` | tail kind 11 | 0: state 1. 1: once `0x903849` is 2, `ScriptFlags_Clear40`, `0x903849 = 0`, the tail ended. 10: `Party_DropIn(1)`, state 11. 11: once `0x903849` is 0, `ScriptFlags_Clear40`, the tail ended |
| `0x408EB0` | `Area46_StepHook` | `0x55` | step hook | `z` at most `0x218000` (signed), x's column `0xE..0x10`, key item 4 not held: `ScriptFlags_Set40`, `0x903849 = 1`, tail kind 11 in state 10 when the s8 chapter byte `Cond_ByteFA` is 8 or more (the drop-in), else state 0; al 1, else al 0 |
| `0x408F10` | `Area46_PlaceKind2At0` | `0x9` | handler 0 of areas 7, 14, 46 | `Kind2_Place(0)` |
| `0x408F20` | `Area46_Trigger54` | `0x40` | object trigger 54 | flag `0x70`; flags `0x6D..0x70` all set: flag `0x71`; al 0 |

State 1's wait for `0x903849 == 2` and state 11's for 0 are the script's
to satisfy (nothing in this band writes 2). Tables: `Area46_Handlers`
`0x5F7E54` (1).

## 7. Area 47

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x408F60` | `Area47_PlaceAtStart` | `0x5C` | handler 0 | `0x801F38F4` | the running object and `Field_Kind2X` / `Z` at cell `(0x12, 0x24)`, `Field_ViewReset`, `+0x3E = 0x1220`, `MapView_SetElevation` of it, `Camera_Angles[0] = 0xFB76`, `[2] = 0xF8` |
| `0x408FC0` | `Area47_SpawnEffect3D` | `0x28` | handler 1 | `0x801F396C` | an effect of kind `0x3D` at `Effect_FindFree`'s slot |

`MapView_SetElevation`'s word is pushed with `Field_ViewReset`'s leftover
`edx` above it; the callee reads its low 16 bits only (its `movsx` and the
`* 2` into a word), so ours passes the word sign-extended and the fuzz
masks the argument to 16 bits. Tables: `Area47_Handlers` `0x5F8324` (2).

PSX twins throughout are `names/area_records.toml`'s descriptor slots and,
in parentheses, `analysis/area_pairs.json`'s pairs of non-descriptor roots
(hypotheses, not disassembled on the PSX side this round).

## 8. The fuzz

`BOF3X_SHADOW=area_w1b` (`src/game/area_w1b_fuzz.cpp`): six `Run` calls
under the one shadow name, `Group::area` each area's number, the real
descriptors and tables in place. Shapes as the roots say: `kChoice`,
`kHandler`, `kInit`, `kHook` (the step and cell hooks, `ret_mask` `0xFF`),
`kTail` (the four mode tails, their jump tables moved by the harness),
`kState` (area 43's and area 45's state handlers), `kCallee` (called
functions, the draws, the two object triggers). `BOF3X_AR1B_AREA=n` runs one
area's group alone (the controls script's shortcut).

- **Area 42** (6,000 rounds a function): regions `Field_ActiveMember`
  (seeded to a party record or one of the first four field objects),
  `MoveScript_EffectState`, `Area42_Rank`, `Field_Kind2Hold`,
  `Camera_Angles`, `Effect_Objects`' first eight records. Listed: the
  window `0x40E750`, `0x57CD90` (`kByte 0xFF..0x1D`), `Crt_sprintf`,
  `EventOp_9x` (its 13-byte operand dereferenced), `Area42_CheckAll`
  (`kPhase` for the handlers), `Effect_FindFree`, `ScriptFlags_Set40` /
  `_Clear40`. Seeds: the leader's effect index inside the table and its
  entry 1; the countdown at every boundary (0, 1, 29, 30, 150, 179, 180,
  299, 300, `0x384`, 7,679, 7,680 - the seconds' low byte wrapping - and
  `0xFFFF`); the tail state at each case, 7..9 and past the table;
  `0x9039F5` a slot of the fuzz's records with `+0` bit 0 set or clear;
  the rank 0..2; `Cond_ByteFD` 0, 1, 2; field objects qualifying for
  `CheckAll` (`+0` bit 0, `+6` 9, `+5` `0x5C..0x5E`, and near misses); the
  step hook's `(x, z)` on and beside its two rows and columns, with bits
  above. `disturb` moves the rank, `0x9039F5`, the countdown word, the tail
  kind and state, `Cond_ByteFD` and `Field_ActiveMember`.
- **Area 43** (4,000): `Area43_States` as a `DataTable`; `Tint_Release`,
  `MoveCmd_TestFB`, `Effect_FindFree` listed; the state byte `+4` 0 or 1;
  the tint bytes at `0x80`, `0x82` and near (half the time all three at
  `0x80` / `0x82`, the release); `disturb` moves `Field_ActiveMember`.
- **Area 44** (6,000): `Area44_SetGates` (`kPhase`) and `Area44_PushParty`
  listed for the tails; `AreaMap_SetByte` with x masked to 16 bits (pushed
  with the caller's `esi` / `edi` high half), the toggle `0x57C160`,
  `Sprite_FindNearby` answering `0x1C..0x21` (so `0x1E` / `0x1F`, a member on
  extra object 0 or 1, a third of the time). Seeds: the gate byte any;
  `Cond_ByteFD` 0..3, `0xFF`; the cell hook aimed at one of the four switch
  records (the leader's facing matching two rounds in three, bits above the
  cell bytes); the tails' states 0, 1, 10, 11, 20 and past; the extra
  objects' `+9` 0; `PushParty`'s arguments the directions and objects in
  range with bits above. `disturb` moves the gate byte, the tail state and
  `Field_MemberCount`.
- **Area 45** (4,000): area 16's group ([`area_w0b.md`](area_w0b.md)
  section 11) with area 45's tables - the six state tables as `DataTable`s,
  the packet buffer, items and names of the fuzz's, the tables put back to
  the exe's bytes two rounds in three, `settle` keeping `+1` inside the
  plate table and re-planting the leader's cell record; the place rows 11,
  the plate places 10, the cell records 265; plus `Area45_Record4Tick`.
- **Area 46** (4,000): `KeyItem_Has`, `Kind2_Place` listed; `0x903849` 0..3;
  the tail states; the step hook's z at `0x218000` and one either side, a
  negative, and its columns; `Cond_ByteFA` 7, 8, 9, 0, `0x80`, `0x7F`
  (`disturb` moves it: read after the calls).
- **Area 47** (4,000): `MapView_SetElevation` masked to 16 bits, `Camera_Angles`
  and the effect records as regions.

**Result (in this worktree):** 0 mismatches in every run; rounds / calls to
the stand-ins: area 42 66,000 / 55,693; 43 24,000 / 40,086; 44 36,000 /
164,024; 45 104,000 / 509,800; 46 16,000 / 19,666; 47 8,000 / 12,000 (the
first run; the six runs share the harness's random stream).
`BOF3X_SHADOW='*'`: STAR_LINE.

## 9. What reaches it, defects, calls across groups

- **Reach - the world-map route reaches none of the 55.** The route's
  traces (`analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv`,
  `hidden_worldmap/bof3x.calltrace.tsv`, `analysis/hidden_reached_worldmap.json`)
  hold one function of this band, `WorldMap_DrawNeedle` `0x408530`, called
  562 times from `0x40454D` - area 33's frame draw: the needle is one body
  for all eleven maps and lies in area 45's block, which is why the tool's
  "live" column names area 45. The route plays area 33's map (Yraall), not
  area 45's. `Area45_Record4Tick` `0x408990` is also area 33's record-4 state
  1, but the route's hidden trace never enters it (the record `+4` effect
  did not reach its state 1). **For the coordinator's world-map A/B after
  the merge:** it can only show that nothing moved; of AR1B's functions the
  one on area 33's path is `Area45_Record4Tick`, and only if the record-4
  effect runs. No other recorded route (attract, combat, shop) reaches the
  band.
- **Latent defects (described, not fixed; the fuzz keeps inside them):**
  1. `Area42_ChoiceMessage` indexes its messages by the signed choice byte,
     unchecked; `Area42_Init` indexes its three placement records by
     `Area42_Rank`, unchecked (both writers keep it 0..2).
  2. `Area42_TimerTail` states 3 and 5 test the effect record `0x9039F5`
     names, unchecked: states 2 and 4 store `Effect_FindFree`'s `0xFF`
     ("none") there and go on, and the test then reads the byte at
     `Effect_Objects + 0x7F80`, past the 20 records - the tail waits on
     whatever that byte's bit 0 is.
  3. `Area44_PushParty` indexes `Field_DirectionSteps` and
     `Sprite_ObjectsExtra` unchecked (its callers pass constants in range;
     ours aborts past them).
  4. Area 45 has area 16's: the unbounded cell and plate searches (D74's
     shape), the name set 3 reading the plate table's bytes, the unchecked
     record indexes, the unchecked `.data` dispatches (ours aborts past
     each of the six tables, as AR0C's does - AR0B's area 16 reads on).
  5. `Area43_ObjectRun` dispatches by `Sprite_Current[4]` unchecked (ours
     aborts past its two entries).
- **Cross-group raw-address calls:** `0x40E750` (area 75's block, group
  AR1F, not this wave: a window draw), `0x57CD90` (group SX this wave: a free
  field object), `0x57C160` (engine code nobody owns: the flag toggle).
  Tables name `0x4253C0` (AR4A), `0x40C490` (AR1E) and round eight's shared
  HUD states - reached through the tables, not called. Named and ours:
  `WorldMap_DrawNeedle`, `EventOp_9x`, `Party_DropIn`, `KeyItem_Has`,
  `Kind2_Place`, `MoveCmd_TestFB`, `Tint_Release`, `Sprite_FindNearby`,
  `Field_ViewReset` and the standard set. Capcom's by name: `Rand`,
  `Crt_sprintf`.
- **Inbound:** area 16's and area 33's record-4 tables name
  `Area45_Record4Tick` `0x408990` raw, AR0A's `Area07_Handlers` names
  `Area46_PlaceKind2At0` `0x408F10` raw - for the rebinding pass.
- `analysis/calltrace/entries_logic.txt`: 53 lines appended (`004082A0 1C5`
  and `00408470 BC` were there); `004068D0 159`, `00406DE0 A1`,
  `00407320 22E`, `004077F0 143`, `00408070 12`, `004086D0 58`,
  `00408EB0 55` are smaller than host lines already there.

## 10. Controls

CONTROLS_SECTION
