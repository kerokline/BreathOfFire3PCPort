# World 2, areas 104..106: the world map's seventh copy, a leader controller, a countdown, two step hooks

**Status:** IN PROGRESS (2026-09-28) - 52 functions ours
(`src/game/area_w2e.cpp`, shadow name `area_w2e`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)): 0 mismatches in
270,600 rounds (five `Run`s, section 6); @@CONTROLS_SUMMARY@@ (section 9).
Fuzz only: no recorded route reaches any of the 52 (section 8). No
divergence.

Group AR2E of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15): the
band `0x4146C0..0x4168E0`, whole areas as `tools/area_rows.py --groups` cut
them (its `AR2A` row at this tip; only areas 104..106 are this group's, area
107 has no code). What each area *is* in the story is not read here: the
names come from what the code does.

## 1. The band, the areas, the function count

`tools/area_rows.py --unit AREA104..106 --clones` (run 2026-09-28 before
`symbols.toml` gained this group's entries) lists 53 starts, one ours:
`WorldMapHud_BoxWait` `0x414BB0` (round eight's, shared by the eleven world
maps, in area 104's block; called here by its table, not by name). **All 52
others are functions and all are taken: no start dropped, none added.** Every
one was read to its last instruction with capstone (the scratch `adis.py`);
every gap between them is `nop` padding. The tool's clone rows match the
reading call site for call site, and its three in-function jump tables
(`0x415BE0`, `0x4160E0`, `0x416600`) are right; its "gap" labels are two
functions reached only by calls (`0x4156C0`, the tail of area 104's
controller and of area 121's, and `0x4161F0`, the countdown's panel) - both
real functions, both taken.

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 104 | `0x61BB38` (no handlers, no choices; the init) | init; `WorldMap_Records` record 6 (`+0`, `+0xC`; `+0x10` is `0x41B730`, outside the band); `WorldMap_FieldHooks` entry 6; tail kind 40; object trigger 36; leader state 12 and effect kinds `0x5C` / `0x6A` by engine code (section 3) | `0x4146C0..0x41635C` | 43 (and the one ours) |
| 105 | `0x61BD70` | init, step hook, tail kind 61 | `0x416360..0x41643A` | 3 |
| 106 | `0x61C670` | three handlers (`Area106_Handlers` `0x61C664`), step hook, tail kind 36 | `0x416440..0x4168D5` | 6 |

**The tool's known gaps, looked for** (round10 doc sections 7, 10, 13): no
start without padding or a `ret` before it; the tail kinds armed in the band
are all by immediates (`0x28` by area 104's init, `0x3D` by area 105's step
hook, `0x24` by area 106's arming, `0x2C` by trigger 36 - engine code's), so
none is armed through a register. No s16 coordinate pair read as a pointer
here.

**PSX twins** (`names/area_records.toml`, `analysis/pairs_propagated.json`):
area 104's init `0x801F2E8C`, field hook `0x801F2F0C`, record `+0`
`0x801F2FB0`, `+0xC` `0x801F34E0` (its `+0x10`, `0x801F4238`, is `0x41B730`
on the PC, outside the band); area 105's init `0x801F32B8`; area 106's
handlers `0x801F38E8`, `0x801F3940`, `0x801F3A60`; by call,
`Area104_BuildMinimap` `0x801F6564` and `Area104_MinimapShade` `0x801F66C8`.
`pairs_propagated.json`'s four pairings of `0x415C60` are jump-table cases of
tail kind 40 (HANDOFF item 9's error), not functions.

## 2. Area 104: the world map's seventh copy

Area 104 is `WorldMap_Records` record 6 (`0x6539B8`, area byte `0x68`) and
`WorldMap_FieldHooks` entry 6. **Eighteen of its functions are area 87's
code** ([`area_w2b.md`](area_w2b.md) section 4, itself area 45's and area
16's) **instruction for instruction**: a capstone compare of the eighteen
pairs (the scratch `cmp.py`, addresses inside a function made relative) finds
only jump targets, calls to each copy's own functions and the table operands
differing. What differs from areas 87 and 88:

| | Areas 87 / 88 | Area 104 |
|---|---|---|
| Place hook (field hook) | names the place and its items (`0x174` / `0x178` bytes) | its own, `0x56` bytes: `Msg_OpenScript(4)` whatever the place |
| Plate state 0 (`PlateStart`, the bank) | in the band (`push 0x156` / `0x157`) | `0x41ACD0`, outside the band (area 121's block): no bank constant here |
| Region label cell | `0x803580` | `0x803580` (the same) |
| Record `+4` / `+8` / `+0x14` | the cell marker, the record-8 states, the drift's data | **null** (known-defects D66's note, [`worldmap_area.md`](worldmap_area.md) section 5) |
| Record `+0x10` (drift) | in the band | `0x41B730`, outside the band |

So the copies' "constants" are, for area 104, only its tables: the plate
animations `0x61B4F0` (one entry, place `0x65`, then a zero record), the
four state tables, the sprites and the buttons. **The second legend's key
search reads eight button entries** in every copy; in area 104 the seventh
and eighth are the first two dwords of `Area104_LeaderStates` (`0x415040`,
`0x4154E0`: masks `0x5040`, `0x54E0`, sprite `0x41`) - read in place, as the
original reads them. Ours is one body of code over a `WorldMapTables`
(`area_w2e_callees.h`), copied from AR2B's (the bodies live in each module's
anonymous namespace), with a named entry per address:

| PC | Name | Size | Root / caller |
|---|---|--:|---|
| `0x414700` | `Area104_PlaceMessage` | `0x56` | field hook entry 6: state 0 `ScriptFlags_Set40`, `Msg_OpenScript(4)`, + 1, `Field_Request` 2; state 1 once the request is not 2 `ScriptFlags_Clear40` and `0x9039F3..F5` zeroed |
| `0x414760` | `Area104_PlateRun` | `0xD6` | record 6 `+0` |
| `0x414840` / `0x414990` / `0x4149E0` / `0x414A40` | `_PlateShow` / `_PlateGrow` / `_PlateHold` / `_PlateShrink` | `0x142`, `0x41`, `0x58`, `0x50` | `Area104_PlateStates` 1..4 (`0x61BB7C`; 0 is `0x41ACD0`) |
| `0x414A90` / `0x414AB0` | `_HudRun` / `_HudFrame` | `0x12`, `0xA` | record 6 `+0xC` / `Area104_HudStates` 1 (`0x61BB90`) |
| `0x414AC0`, `0x414AE0`, `0x414B10`, `0x414B40` | `_FrameStep`, `_FrameSlideIn`, `_FrameHold`, `_FrameSlideOut` | `0x12`, `0x21`, `0x28`, `0x41` | `Area104_FrameStates` (`0x61BB98`; 0 `WorldMap_FrameWait`) |
| `0x414B90`, `0x414BF0`, `0x414C60`, `0x414CD0` | `_BoxStep`, `_BoxSlideIn`, `_BoxHold`, `_BoxSlideOut` | `0x12`, `0x62`, `0x6E`, `0x57` | `Area104_BoxStates` (`0x61BBA8`; 0 `WorldMapHud_BoxWait`) |
| `0x414D30` / `0x414F00` / `0x414FC0` | `_DrawFrame` / `_DrawSprite` / `_DrawHud` | `0x1C5`, `0xBC`, `0x58` | called by the frame and box states |

## 3. Area 104's own code

Engine code reaches three of its roots **by area number**, each with a twin
in area 121's block (group AR3B's band this wave):

| Engine site | What | Area 104 (`Game_AreaNumber` `0x68`) | Otherwise |
|---|---|---|---|
| `0x52FE90` (`0x660918[12]`, leader state 12) | the leader's frame | `jmp 0x415020` | `jmp 0x41B9D0` |
| `0x462B60` (`Effect_KindHandlers[0x5C]`) | effect kind `0x5C` | `jmp 0x415780` | `jmp 0x41C190` |
| `Effect_KindHandlers[0x6A]` (`0x6554F8`) | effect kind `0x6A` | `0x415D50` directly | (the same) |

**The leader controller** (leader state 12 in area 104):

| PC | Name | Size | What |
|---|---|--:|---|
| `0x415020` | `Area104_LeaderRun` | `0x17` | `Area104_LeaderStates` `0x61BC28` by the leader's `+2` (a call; two entries), then a tail jump to `Area104_LeaderCharge` |
| `0x415040` | `Area104_LeaderIdle` | `0x26B` | state 0, the leader at rest: out on `Area104_StartOnObject121`, `Field_ScriptFlags` bit 8, `Field_ScriptFlags2` bit 6, `Field_Request`, or a hold (`+0xA`). `Field_InputHeld` set clears `Field_State +0x136` unless the actor state of `+0x148` has bit 5. Out on `Field_LeaderCellEvent`, area 121's `0x41C0A0` (menu button), `Field_LeaderTalkTest`, `0x41C0E0`. The facing before area 121's turn keys `0x41C110` is kept (`b`); with no turn and the charge button (word `0x903582`) not held, or a full charge (`+0xB` `0x40`): `+8 = b`, `+0x137 = 0`, `Area104_StopMotion`, `+9 = 0`, `+2 = 0`. Else the new facing is limited to one step either side of `b` over the 8-step wrap (the s8 compares as the original makes them), `& 7`. `Field_LeaderStepTarget` 1 or `0xFF`: `+8 = b`, stop with pose 3; 0: `Field_LeaderPushObjects` answering is `Area104_ObjectAhead121`; else `Area104_PoseByCharge`, a charge pose with a turn keeps it standing; else `Field_JumpStart`, `Field_JumpCheckHeight`, `+9 - 1`, `Field_LeaderStepTick`, `+0x137 = 1`, `+2 = 1` |
| `0x4152B0` | `Area104_ObjectAhead121` | `0x133` | nothing unless `Game_AreaNumber` is `0x79` - **area 121's code, dead in area 104** (the controller runs only there). The object two direction steps ahead (`Sprite_ObjectAt(x, z, 1)`); with the charge button held and the object facing the leader, its `+0x80 |= 1`; else `Area104_TurnToFree` and a step |
| `0x4153F0` | `Area104_TurnToFree` | `0x69` | `(x, z, object)` -> al: `0xFF` answers 0; else `+8` = area 121's `0x41BE10(x, z, object)` (the octant from the object) and up to eight turns until the cell ahead is free (`Field_LeaderStepTarget` 0 and no object there): 1 |
| `0x415460` | `Area104_StartOnObject121` | `0x71` | al 0 unless the area is `0x79` (dead in area 104): the object at the leader's own place handed to `Area104_TurnToFree`; 1 starts a step |
| `0x4154E0` | `Area104_LeaderStep` | `0x157` | state 1: `+0x137 = 1`; `+9` counts the step out through `Field_LeaderStepTick`; then `Field_Bit20Tick`, `Field_FloorDamage`; with `Field_InputFlags` bit 0 the cell under the leader: `0xAF` `Field_ChangeArea(pending area, 0x903860, 0x90384C, 0x905B88)` (the area and flags pushed with stale bits above the word and byte read), `0x904EE0 = 0`, `0x937F98 = 0xC`; `0xC0` (asked again) `Area_LinkAt`. `Field_EdgeBits` + 1 (+ 2 facing 2 or 6), `Area104_StopMotion`, `Scenario_ArriveHook`, `Field_Bit80Tick`; a held key of (`0x903582 | 0x903580 | 0xF000`) is a tail jump to `Area104_LeaderIdle` |
| `0x415640` | `Area104_StopMotion` | `0x36` | `Field_State +0x128 = 3`, `+0xC`, `+0x10`, `+0x14` = 0, `MoveScript_F3Divisor` and `MoveScript_FAWord` 0 |
| `0x415680` | `Area104_PoseByCharge` | `0x3A` | `Field_State +0x128` = 4 with the charge button held and `+0xB` below `0x40`, else 3 |
| `0x4156C0` | `Area104_LeaderCharge` | `0xB8` | the charge: `+0xA` counts a hold down; held, `+0xB` rises to `0x40`, then `+0xA = 0x1E`; released it drains by 3 above `0x30`, 2 above `0x20`, else 1 (stepping: half, and the odd bit on odd frames); held to `0..0x40` as an s8 |

**Effect kind `0x5C`** (the companion; `Area104_Kind5CStates` `0x61BC30`):

| PC | Name | Size | What |
|---|---|--:|---|
| `0x415780` | `Area104_Kind5CRun` | `0x12` | the state table by `+1`, a tail jump (five entries; the fifth `0x41C5B0`, area 121's) |
| `0x4157A0` | `Area104_Kind5CStart` | `0xB1` | state 0: `+8` the leader's facing; `Sprite_InitFromEntry(Area_Descriptors[area] +8's pointer + 8)`; tint `0x80`; `+1 + 1`; `Area104_Kind5CFollow`; with key item `0xA` (in area `0x68`), four more kind-`0x5C` effects in state 3 with delays 0, 8, `0x10`, `0x18` |
| `0x415860` | `Area104_Kind5CFollow` | `0xDB` | at the leader's place and height, `Sprite_UpdateScreenA`; the gauge (`Draw_PassFlags & 0x1B`): area 121's frame `0x41C350(0xDC, 0x10, n)` and `Area104_DrawGauge` bars by `Field_StatusBits` bit 6 (both full), the leader's `+0xA` (blinking frame, back bar) or its `+0xB` (front bar `0x40 - charge`) |
| `0x415940` | `Area104_DrawGauge` | `0xC4` | `(width, bar)`: one `POLY_FT4`, x 228 .. 228 + (width & 0xFF), y 29..39, u / CLUT from a two-entry table on the stack by `bar & 0xFF`, `Gfx_CommitPrim(2, 0x48)` |
| `0x415A10` | `Area104_Kind5CTurn` | `0x54` | state 1: the companion's `+8` against the leader's facing through `Area104_Kind5CTurnStep` (then with `+8 ^ 4`); settled, the facing = `+8` and the follow |
| `0x415A70` | `Area104_Kind5CTurnStep` | `0xCB` | `(from, to)` -> al, the low bytes over the 8-step wrap: one step (`+0x14 = +-0x40`, `+8 +- 1`), the leader's facing, `+9 = 8`, `Area104_Kind5CSpin`, `+1 = 2`, al 1; more than two apart, or equal, al 0 |
| `0x415B40` | `Area104_Kind5CSpin` | `0x48` | state 2: `+0x6C += +0x14`, `& 0xFFF`; `+9 - 1`; at 0 back to state 1 and a tail jump to `_Kind5CTurn`, else to `_Kind5CFollow` |
| `0x415B90` | `Area104_Kind5CRise` | `0x4B` | state 3: `+0xA` counts out the delay; then the leader's place with `+0x3C + 0x800000`, area 121's `0x41C5B0`, `+1 + 1` |

**Tail kind 40, the countdown (kind `0x6A`), the minimap, trigger 36:**

| PC | Name | Size | What |
|---|---|--:|---|
| `0x415BE0` | `Area104_Tail40` | `0x16C` | tail kind 40 (armed by the init), by the s8 `0x9039F4` 0..3, each once `Field_Request` is 0: message 1; `Field_ChangeArea(0x79, 0x190000, 0x2D0000, 7)` and `Field_ScriptFlags &= ~0x140`; story flag `0x5B` (set it and message 2, or message 3); the countdown effect (kind `0x6A`, `+9 = 0x19`, at the leader's cell), `Music_FadeOutStop(0xA)`, `Music_Play(0x94, 8)` |
| `0x415D50` | `Area104_Kind6ACountdown` | `0x2CC` | `Field_Request` 5 releases it (and it still draws once); with the request 0 and the leader in state 12, `+0xA` frames (`0x1D` each second) and `+9` seconds count down; out: `Field_ChangeArea(0x79, 0x190000, 0x2D0000, 7)`, `Field_ScriptFlags &= ~0x40`, released. The draw: `Menu_DrawBox`, `Menu_DrawOutline`, `Crt_sprintf` into `0x904BA0` by the format `0x61BC74` (seconds, frames / 3, a jittered digit), `Text_DrawFont12` (colour 2 on blink frames under ten seconds), `Area104_DrawPanel`, a ring of 24 semi-transparent triangles pulsing round the leader's place on the panel |
| `0x4161F0` | `Area104_DrawPanel` | `0x14E` | one semi-transparent `POLY_FT4` (CLUT `0x7BC6`, tpage `0x1E`) and four `POLY_F3` from `Area104_PanelTris` `0x61BC44`; every eighth frame the live CLUT word `0x811446` `0x109F` / 8 and `Gfx_ClutStripDirty + 1` |
| `0x416020` | `Area104_BuildMinimap` | `0xB8` | at entry with key item `0xA`: a 4-bit image of the cells `z 0xD..0x51`, `x 5..0x54` into `Gfx_UnpackScratch` (header words `0x14`, `0x44`, then 69 rows of 40 bytes - one row more than the header says), queued for upload at `(0x380, 0x100)`, CLUT words `0x811440..0x811446`, `Gfx_ClutStripDirty + 1` |
| `0x4160E0` | `Area104_MinimapShade` | `0x10D` | `AreaMap_ByteAt(x, z)`: `0x00`, `0xAF`, `0xC0` 0; `0x40` 1; `0x10`, `0x50` 2; any other 3 (a byte table picks one of four cases; **the jump table lists them 0, 2, 1, 3** - the first reading had them in order and the fuzz refused it at once) |
| `0x416340` | `Area104_Trigger36` | `0x1D` | object trigger 36: `ScriptFlags_Set40`, tail kind `0x2C` (engine code) state 0 argument 9; al 0 |
| `0x4146C0` | `Area104_Init` | `0x3D` | `Field_ScriptFlags |= 0x140`; tail kind 40; without key item `0xA` state 0 and `Cond_ByteFE + 1`; with it the minimap and state 2 |

Read off the code, then: area 104 runs its own leader controller whose
button (word `0x903582`) charges a gauge the companion effect draws; tail
kind 40 opens a message on entry and, with key item `0xA`, starts a
25-second countdown drawn over a minimap marker, which sends the party to
area `0x79` (121) at `(0x19, 0x2D)` when it runs out. What that is in the
game is the owner's to say.

## 4. Area 105

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x416360` | `Area105_Tail61` | `0x45` | tail kind 61 | state 0: `Msg_OpenScript(1)`, `Field_Request = 2`, state 1; 1: once the request is not 2, `ScriptFlags_Clear40`, `0x9039F3` / `F4` = 0 |
| `0x4163B0` | `Area105_StepHook` | `0x49` | step hook (`0x56E122`) | `(x, z)` -> al: chapter (s8 `Cond_ByteFA`) `0xA` or more, or `Cond_ByteFD` set, al 0; the leader's facing stored at `0x903850`; facing 0..2, or z's high word (s16) at or below `0x1D`, al 0; else `ScriptFlags_Set40`, tail 61 state 0, al 1 |
| `0x416400` | `Area105_Init` | `0x3B` | init (PSX `0x801F32B8`) | from area `0x57`: story flag `0x4F` set when `Cond_ByteFD` is 0, then (read again) cleared when it is 1 |

## 5. Area 106

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x416440` | `Area106_Handler0` | `0x36` | handler 0 (PSX `0x801F38E8`) | `Field_ActiveMember +0x80` bit 0 cleared; `MoveScript_EffectState[leader +0x89]` 0: `Cond_ByteFE = 1` and the running object's `+0 = 0` |
| `0x416480` | `Area106_PlaceByFlags` | `0xCF` | handler 1 (`0x801F3940`) | the running object to `(0x128000, 0xB8000)` / `(0x128000, 0x238000)` / `(0x118000, 0x1F8000)` by story flags `0x51` / `0x4F`, with `+0x3E` `0x850` / `0x600` / `0x300` and the script word + 2, + 4 or not |
| `0x416550` | `Area106_PlaceByPair` | `0xAF` | handler 2 (`0x801F3A60`) | `k = flag 0x52 | flag 0x53 << 1` picks a cell of `Area106_Cells` `0x61C6B4` (`(x << 16) | 0x8000`); flag `0x4F`: `+0x3E = 0x850`, script + 8; else `0x280`, script + 2k |
| `0x416600` | `Area106_Tail36` | `0x161` | tail kind 36 | states 0, 2, `0xA`, `0xC`, `0xE`, `0x10`: `Party_DropIn(0..5)`, the tail cleared, story flags: 0 sets `0x51`, 2 clears it; `0xA` / `0xC` / `0xE` / `0x10` set `0x52` / `0x53` as 10, 01, 11, 00 |
| `0x416770` | `Area106_StepHook` | `0x144` | step hook (`0x56E132`) | with `Cond_ByteFD` 0, five cells by the flags arm tail 36 (states 0, 2, `0xC` / `0x10`, `0xE`, `0xA`), al 1: z exactly `0x248000` with x's high word `0x11..0x13` (`0x4F`, not `0x51`); z `0xA8000`, x `0x12..0x13` (`0x4F` and `0x51`); z `0x258000`, x `0x1A..0x1B` (`0x52`); x `0x268000`, z `0x24..0x25` (`0x53` only); x `0x188000`, z `0x15..0x16` (none) |
| `0x4168C0` | `Area106_ArmTail36` | `0x16` | called | `ScriptFlags_Set40`, tail kind 36 with the state byte |

So the five cells of area 106 cycle a party member in and out
(`Party_DropIn` entries 0..5) and step two flag pairs; the handlers place an
object by the same flags.

## 6. The fuzz

`BOF3X_SHADOW=area_w2e` (`src/game/area_w2e_fuzz.cpp`): five `Run`s under the
one shadow name, `Group::area` each area's number, the real descriptors and
tables in place. `BOF3X_AR2E_AREA=n` runs one group alone (the controls'
shortcut): `1040` area 104's world-map copy, `1041` its own code, `1042` its
minimap, `105`, `106`.

- **Area 104's world-map copy** (18 functions, 4,000 rounds each): area 87's
  group ([`area_w2b.md`](area_w2b.md) section 5) over area 104's tables - the
  four state tables as `DataTable`s, the packet buffer of the fuzz's, the
  plate animations a region put back to the exe's bytes two rounds in three
  with the place planted in the zero record (the search has no bound), the
  place `0x65` half the time; the draws' button words seeded with bits only
  the seventh or eighth entry has (area 104's code pointers). The tables'
  addresses the seeds and regions use are the fuzz's own literals, never
  `kWm104`'s.
- **Area 104's own code** (24 functions, 6,000 rounds each): regions the
  twenty effect records, the minimap's image, upload queue and CLUT words,
  `Input_Held`, four actor states, the cells the band writes outside the
  harness's regions; `Area104_LeaderStates` and `Area104_Kind5CStates` as
  `DataTable`s. Stand-ins louder or truer than the real callees where the
  caller reads after them: the early-out tests (`Field_LeaderCellEvent`,
  `Field_LeaderTalkTest`, area 121's two, `Area104_StartOnObject121`,
  `Scenario_ArriveHook`) answer 0 five calls in six so the later paths run;
  `Field_LeaderStepTarget` 0 half the time, 1, `0xFF` and another byte a
  sixth each; `Field_LeaderPushObjects` half and half; `Area104_PoseByCharge`
  writes `Field_State +0x128`; the turn test answers 0 two calls in three
  and moves both facings; `Flags_Test` answers the story bit as it stands
  three calls in four; `Effect_FindFree` a slot or none (never none for
  `Area104_Kind5CStart`, which does not test it); `Sprite_ObjectAt` none, a
  field object or an extra record (not none with the button held for
  `Area104_ObjectAhead121`); `AreaMap_ByteAt` the band's tested cells and
  their neighbours. `Field_ChangeArea`, `Area_LinkAt`, `Menu_DrawBox`,
  `Field_CellHasEvent` are listed with the masks of what they read (the
  originals push those arguments with stale upper bits). Seeds: the charge
  button held or not in `Input_Held`, the charge `+0xB` at `0, 1, 0x20,
  0x21, 0x30, 0x31, 0x3F..0x42, 0x7F, 0x80, 0xFF`, the hold `+0xA` at 0..2 and
  `0x1E`; the area `0x79` (area 121's paths) and its neighbours; the objects'
  facings on the leader's; the tail state 0..4 and its sign; the countdown's
  frames and seconds at their edges with the leader in state 12 or not;
  `Frame_Counter` at the blink and pulse edges; the gauge's bar 0 / 1 with
  bits above; the turn test's pairs at 0, 1, 2 and more steps apart.
- **Area 104's minimap** (600 rounds; each round calls the shade 5,520
  times): the upload count 0..19 at its edges.
- **Areas 105 and 106** (6,000 rounds a function): the chapter and
  `Cond_ByteFD` at their edges, the step hook's z high word at and beside
  `0x1D` and its sign; area 106's step seed picks one of the six flag cases
  and plants the four story flags for it, the arguments that case's cell
  (the exact word or one bit off in one axis, the high word in or beside its
  range in the other); the tail states 0..`0x11`, odd and negative.

**Result (in this worktree):** 0 mismatches in every run; rounds / calls to
the stand-ins: the world-map copy 72,000 / 142,543; area 104's own 144,000 /
1,387,359; its minimap 600 / 3,312,000; area 105 18,000 / 2,892; area 106
36,000 / 50,077. Coverage lines show every state table entry reached (the
plate states about 800 each, frame and box states about 1,000,
`Area104_LeaderStep` 2,966, the kind-`0x5C` states 1,173..3,766), area 121's
paths (`Area104_ObjectAhead121` 218 calls), the idle's pose and step
(`Area104_PoseByCharge` 216, `Field_JumpCheckHeight` 137), tail 36 armed 258
times. `BOF3X_SHADOW='*'`: exit 0, 452 self-test lines, no mismatch or Fatal,
`inject: 4988 ours` (all 52 of this group's injected; 4,936 before).

## 7. Latent defects (described, not fixed; the fuzz keeps inside them)

1. **Area 104's world-map copy** has areas 45 / 87's: the unbounded plate
   animation search (area 104's table is one entry and a zero record: a
   place other than `0x65` or 0 reads on into the data after it), the
   unchecked `.data` dispatches (ours aborts past each of the four tables,
   and past `Area104_LeaderStates`' two and `Area104_Kind5CStates`' five).
2. `Area104_Kind5CStart` spawns four effects without testing
   `Effect_FindFree`'s "none" (`0xFF`): a full effect pool writes
   `0xFF * 0x80` past `Effect_Objects`. `Area104_Tail40` tests none but
   sign-extends the slot. Ours aborts on a slot outside 0..19.
3. `Area104_ObjectAhead121` (area 121 only) indexes `Sprite_ObjectsExtra` by
   `Sprite_ObjectAt`'s answer less `0x1E` unchecked: "none" with the charge
   button held reads (and may write `+0x80`) `0xE1` records past the four.
   Ours aborts. Area 121's own copy (`0x41BEC0`'s family, AR3B's) is where it
   could matter.
4. `Area104_DrawGauge` indexes a two-entry table on its stack by `bar &
   0xFF`: a bar above 1 reads the return address. Callers pass 0 and 1;
   ours aborts on more.
5. `Area104_BuildMinimap` writes its upload into `Gfx_UploadQueue*` at
   `Gfx_UploadQueueCount` unchecked against the twenty slots; ours aborts.
   Its image is 69 rows where its header says `0x44` (68).
6. `Area104_Kind5CStart` reads `Area_Descriptors[Game_AreaNumber]` unchecked
   (ours aborts at 200 and above); in the game it runs only in area `0x68`.
7. Record 6's null `+4`, `+8`, `+0x14` (D66's note): not called by this
   band's code.

## 8. What reaches it, calls across groups

- **Reach - no recorded route reaches any of the 52.**
  `analysis/hidden_reached_worldmap.json` and the world-map route's
  call counts (`analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv`)
  hold no address of this band: the route plays area 33's copy, and
  `WorldMapHud_BoxWait` (round eight's) is reached there through area 33's
  table, not area 104's. The coordinator's world-map A/B can only show that
  nothing moved.
- **Cross-group raw-address calls** (`area_w2e_callees.h`): area 121's
  helpers in group **AR3B**'s band this wave - `0x41BE10` (the octant toward
  a point), `0x41C0A0` (menu button), `0x41C0E0` (a held button),
  `0x41C110` (turn keys), `0x41C350` (the gauge frame), `0x41C5B0` (kind
  `0x5C`'s state 4); and `0x5A7570` (an engine `POLY_F3` setter nobody owns,
  read 2026-09-28). The state tables also hold `0x41ACD0` (area 104's plate
  state 0, AR3B's) and `0x41B730` is record 6's `+0x10` (AR3B's); both are
  reached through tables, not called by this group's code. Named and ours:
  `WorldMapHud_Start`, `WorldMap_FrameWait`, `WorldMapHud_BoxWait` (in the
  tables), `WorldMap_PinSprite`, `WorldMap_DrawNeedle`, `Party_DropIn`,
  `Scenario_ArriveHook`, `Area_LinkAt`, `KeyItem_Has`, `Effect_FindFree`,
  `Menu_DrawBox`, the standard set; Capcom's `Crt_sprintf` and `Rand` by
  name.
- **Inbound from outside the band** (for the rebinding pass): engine
  `0x52FE9A` (`jmp`) into `Area104_LeaderRun`; engine `0x462B6A` (`jmp`) into
  `Area104_Kind5CRun`; `Effect_KindHandlers` `0x6554F8` names
  `Area104_Kind6ACountdown`; `Area_StepHook`'s calls `0x56E122` / `0x56E132`
  into `Area105_StepHook` / `Area106_StepHook`; **area 121's code** (AR3B's):
  `0x41B9E2` (`jmp`) into `Area104_LeaderCharge`, `0x41BADE` and `0x41C020`
  into `Area104_StopMotion`, `0x41BBEE` into `Area104_PoseByCharge`,
  `0x41C2E1`, `0x41C2F3`, `0x41C31E`, `0x41C339`, `0x41C342` into
  `Area104_DrawGauge`.
- `analysis/calltrace/entries_logic.txt`: 42 lines appended; ten were there
  with the same extent. Host lines with larger extents cover `0x414AC0`
  (`F0`, mine `12`), `0x414FC0` (`2EB`), `0x415460` (`1D7`), `0x415680`
  (`1D1`), `0x415940` (`124`), `0x415B40` (`4DC`), `0x4161F0` (`1B5`),
  `0x4163B0` (`3B5`), `0x4168C0` (`C45`): the smaller extents are appended
  beside them.

## 9. Controls

@@CONTROLS@@
