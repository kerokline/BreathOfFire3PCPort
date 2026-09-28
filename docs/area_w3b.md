# World 3, areas 120..121: three choices, a camera ramp, the world map's ninth copy, leader state 12 and effect kind 0x5C

**Status:** IN PROGRESS (2026-09-28) - 54 functions ours
(`src/game/area_w3b.cpp`, shadow name `area_w3b`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), five `Run` calls:
0 mismatches in 294,000 rounds; CONTROLS_SUMMARY (section 10). Fuzz only: no
recorded route reaches any of the 54 (section 8). No divergence.

Group AR3B of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15): the
band `0x41A9D0..0x41C890`, whole areas as `tools/area_rows.py --groups` cut
them. What each area *is* in the story is not read here: the names come
from what the code does.

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` (group `AR3B`) and the tool's `--unit AREA120 /
AREA121 --clones` list 54 starts, none ours. **All 54 are functions and all
are taken: no start dropped, none added.** Every one was read to its last
instruction with capstone (the scratch `nd.py`, `adis.py`); every gap between
them is `nop` padding. The tool's clone rows match the reading call site for
call site (tail `jmp`s included). One body inside a function: `0x41C2B0`,
reached only by `0x41C270`'s own tail `jmp` over padding (no other code or
data names it, an E8 / E9 and dword scan of the image), is taken inside
`Area121_Kind5CFollow` and not injected on its own.

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 120 | `0x623648` (a choice table only: no handlers, no init) | `Area120_Choices` `0x62363C` (3) | `0x41A9D0..0x41AA46` | 3 |
| 121 | `0x6245D8` (choice table `0x6245D4` -> `0x41D270`, AR3C's band; no init) | `Area121_Handlers` `0x6245C8` (3), `WorldMap_FieldHooks` entry 8, `WorldMap_Records` record 8, and in its data block the world map's six state tables, the leader-state and effect tables | `0x41AA50..0x41C88C` | 51 |

Areas 122 and 123 have no code in the band: `area_rows.py` names them only
through the s16 coordinate pairs it reads as pointers (round10 doc section
13); `--unit AREA122` / `AREA123` answer "no unit". Area 121's block is 51
functions because its overlay also carries two engine-reached machines and
two table entries of other roots (sections 5 and 6), not only its world map.

**The tool's counts, corrected by the reading:** it reads `0x624704` (the
leader states) as 7 entries and `0x62470C` (effect kind 0x5C's) as 5 - the
first runs into the second; the leader table has 2. Its world-map rows for
`0x41AB70` are the field-hook shape; the function is not area 87's
(section 4).

**Shared by address with area 104 (group AR2E's band):** the linker folded
identical code of areas 104 and 121 into one copy each, and the copies
landed in area 121's block. Area 104's data names `Area121_PlateStart`
(`0x61BB7C`, its plate state 0), `Area121_RingRise` (`0x61BC40`) and its
record 6 names `Area121_DrawDrift` (`0x6539C8`); area 104's code calls
`Area121_DirectionTo` (`0x415406`), `Area121_MenuButton` (`0x4150C4`),
`Area121_Request4Button` (`0x4150DE`), `Area121_TurnInput` (`0x4150F4`),
`Area121_GaugeSprite` (`0x4158C8`, `0x415902`, `0x415920`) and
`Area121_RingRise` (`0x415BC7`). The other way, area 121's code calls four
functions in area 104's block (section 8).

## 2. Area 120: three choices

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x41A9D0` | `Area120_ChoiceVar3A` | `0x1C` | choice 0 | no message (`0xFFFF`); movement-script variable 3 (`0x903848`) = `0x14` when the choice byte `0x7DEE67` is not 0, else `0xA` |
| `0x41A9F0` | `Area120_ChoiceVar3B` | `0x1C` | choice 1 | the same with `0x1E` / `0x28` |
| `0x41AA10` | `Area120_ChoiceStartRun13` | `0x37` | choice 2 | answer 0: no message, the word `+0x8A` of the record the pointer `0x903804` names + 1, run `0xD` (`MoveScript_Var7`) at step 8; else message `0x35` |

The sibling's `names/area_records.toml` has no record for area 120.

## 3. Area 121's handlers

| PC | Name | Size | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x41AA50` | `Area121_CameraRun` | `0x12` | handler 0 | `0x801F2C3C` | `Area121_CameraStates` `0x62461C` by `Sprite_Current +4` (2; ours aborts past) |
| `0x41AA70` | `Area121_CameraFar` | `0x1E` | camera state 0 | `0x801F2C80` | `Camera_Distance` = `0xFBDC`, `+4` = 1, the script word - 2 (the op runs again) |
| `0x41AA90` | `Area121_CameraApproach` | `0x40` | camera state 1 | `0x801F2CBC` | while `Camera_Distance` (s16) is below `0x5DC`: + `0x40` a frame, the op runs again; then `0x5DC` and `+4` = 0; `MapView_Redraw` = 2 either way |
| `0x41AAD0` | `Area121_StartRun4` | `0x68` | handler 1 | `0x801F2D30` | `Field_ActiveMember +0x80` bit 0 cleared, `+0x83` = 0, word `+0x8A` = 0; `Sound_PlayEffect(0x217)`, `ScriptFlags_Set40`; variables 3..6 and the step = 0, run 4 |
| `0x41AB40` | `Area121_SpawnEffectBA` | `0x28` | handler 2 | `0x801F2DC0` | `Effect_FindFree`; a slot gets `+0` = 1, kind `+5` = `0xBA` |

Twins from `names/area_records.toml` (handlers 0..2) and
`analysis/pairs_propagated.json` (the camera states, gap pairing; read
against the PC code, the shapes agree). The PSX descriptor lists a fourth
handler (`0x801F2C08`) the PC descriptor does not have.

## 4. Area 121's world map: the ninth copy, and what differs

Area 121 is `WorldMap_Records` record 8 (`0x6539F0`) and `WorldMap_FieldHooks`
entry 8 (`0x662E10`). A capstone compare of its 25 world-map functions
against area 87's ([`area_w2b.md`](area_w2b.md) section 4; the scratch
`cmp121.py`: instruction by instruction, addresses inside a function made
relative) finds **22 of the 25 the same** but for jump targets, calls to its
own copies and table operands, and three that differ:

| Function | Area 87 | Area 121 |
|---|---|---|
| field hook (`PlaceMessage`) | `0x174` bytes: place rows by `(row * 16 + Cond_ByteFA)` on a `0xA1` cell, else a cell record's name set (3 sets of (id, 4 items)) to four `Text_Records` rows | **`0x74` bytes, another body**: the index of the place `0x937F82` in `Area121_Places` (four u16), `Msg_OpenScript(index + 1)` (5 when none); no cell, no name set, no `AreaMap_ByteAt`. State 1 is area 87's |
| `PlateStart` | bank `0x156` | bank **`0x158`** (area 104's plate state 0 is this same body) |
| `DrawDrift` | `0x462` bytes: one square about the object and a grid of `MapView_ItemHalfAt` quads with u / v from `DriftUV` | **`0x295` bytes, another body** (area 104's record 6 names it too): two squares only - a flat one of half-side `(4 - b) << 8` at height 0 (texture `(b - 2) | 0xBB28A100`, slot 5), then area 87's square at height `-0x300` (`(b - 2) | 0xBB509100`, slot 4); no map items, no UV table |
| `DrawHud` | label cell `0x803580` | `0x803580` - the same as areas 87 and 88 (area 45 `0x803584`, area 16 `0x803588`) |

So the "name-set layout" that moved area 88's hook call sites does not exist
here: area 121 has no name sets, no place-message rows and no cell search;
its only cell table is `Record4MarkCell`'s. Ours is area 87's body copied
(the functions AR2B wrote in `area_w2b.cpp`, which are in an anonymous
namespace there) over a `WorldMapTables` for area 121
(`area_w3b_callees.h`), with area 121's own field hook and drift. Three
copies of that body now exist (areas 87/88, 104, 121: AR2B, AR2E, AR3B); one
shared body is a later tidy-up, not a behaviour question.

| Function | Area 87 | Area 121 | Size |
|---|---|---|--:|
| `PlaceMessage` (field hook) | `0x40FC60` | `0x41AB70` | `0x174` / `0x74` |
| `PlateRun` (record `+0`) | `0x40FDE0` | `0x41ABF0` | `0xD6` |
| `PlateStart` / `Show` / `Grow` / `Hold` / `Shrink` | `0x40FEC0..` | `0x41ACD0`, `0x41AD30`, `0x41AE80`, `0x41AED0`, `0x41AF30` | `0x51`, `0x142`, `0x41`, `0x58`, `0x50` |
| `HudRun` (record `+0xC`) / `HudFrame` | `0x410170` / `0x410190` | `0x41AF80` / `0x41AFA0` | `0x12`, `0xA` |
| `FrameStep` / `SlideIn` / `Hold` / `SlideOut` | `0x4101A0..` | `0x41AFB0`, `0x41AFD0`, `0x41B000`, `0x41B030` | `0x12`, `0x21`, `0x28`, `0x41` |
| `BoxStep` / `SlideIn` / `Hold` / `SlideOut` | `0x410270..` | `0x41B080`, `0x41B0A0`, `0x41B110`, `0x41B180` | `0x12`, `0x62`, `0x6E`, `0x57` |
| `DrawFrame` / `DrawSprite` / `DrawHud` | `0x4103D0` / `0x4105A0` / `0x410660` | `0x41B1E0` / `0x41B3B0` / `0x41B470` | `0x1C5`, `0xBC`, `0x58` |
| `Record8Run` (record `+8`) / `Record8Place` | `0x4106C0` / `0x4106E0` | `0x41B4D0` / `0x41B4F0` | `0x12`, `0x154` |
| `Record4Run` (record `+4`) / `Record4MarkCell` | `0x410840` / `0x410860` | `0x41B650` / `0x41B670` | `0x12`, `0xB5` |
| `DrawDrift` (record `+0x10`) | `0x410920` | `0x41B730` | `0x462` / `0x295` |

The state tables' shared entries are the other copies': `WorldMapHud_Start`
`0x419110` (AR3A's band, ours), `WorldMap_FrameWait` `0x411310`,
`WorldMapHud_BoxWait` `0x414BB0`, `0x4253C0` and `Area65_Record8Move`
`0x40C490`, `Area45_Record4Tick` `0x408990` - none called by ours.

**The tables** (`[[data]]`; the plate animations and the cell record sit
after area 120's descriptor, the rest in area 121's data block after its own
descriptor):

| Table | At | What |
|---|---|---|
| `Area121_Places` | `0x624624` | 4 u16 places (the field hook's) |
| `Area121_PlateAnims` | `0x623690` | (u16 place, u8 animation, u8) x 4, searched with no bound |
| `Area121_Cells` | `0x6236A0` | (x, z, -, -) x 1, then zeros: `Record4MarkCell` by `+0xB`, unchecked |
| `Area121_PlateStates` / `HudStates` / `FrameStates` / `BoxStates` | `0x62462C` / `0x624640` / `0x624648` / `0x624658` | 5 / 2 / 4 / 4 |
| `Area121_Sprites` / `Buttons` | `0x624668` / `0x6246C0` | 22 x 4 / 6 (read to 8) |
| `Area121_Record8States` / `Directions` / `Record8Anims` / `Record4States` | `0x6246D8` / `0x6246E4` / `0x6246F4` / `0x6246FC` | 3 / 4 x 4 / 4 x 2 / 2 |

## 5. Leader state 12 (`Field_LeaderStates[12]`)

`Field_LeaderStates` `0x660918` entry 12 is `0x52FE90`: `cmp Game_AreaNumber,
0x68; jne; jmp 0x415020; jmp 0x41B9D0` - area 104 runs its own copy (AR2E's),
**every other area this one**. What sets the leader's state to 12 is not
read here; the helpers test area `0x79` (121) themselves, so in any other
area the push and step-off halves are inert. Sprite_Current is the leader.

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x41B9D0` | `Area121_LeaderRun` | `0x17` | `0x52FE9F` (jmp) | `Area121_LeaderStates` `0x624704` by `+2` (2; ours aborts past), called; then a tail jump to `0x4156C0` (area 104's block: `+0xA` counted down, a charge on `+0xB` by the push button) |
| `0x41B9F0` | `Area121_LeaderControl` | `0x26B` | leader state 0 | early out on `Area121_StepOffObject`, `Field_ScriptFlags` bit 8, `Field_ScriptFlags2` bit 6, `Field_Request`, `+0xA`; with any `Field_InputHeld`, `Field_State +0x136` = 0 unless `Field_ActorStates[Field_State +0x148]` bit 5; early out on `Field_LeaderCellEvent`, `Area121_MenuButton`, `Field_LeaderTalkTest`, `Area121_Request4Button`. The old facing, `Area121_TurnInput`, the push button (word `0x903582` & `Input_Held`): neither, or `+0xB` at `0x40` - halt (`+8` back, `+0x137` = 0, `0x415640`, `+9` = `+2` = 0). The button holds the facing; a new facing more than a step away turns one step toward it (signed compares, around the wrap by 8); `+8 &= 7`. `Field_LeaderStepTarget` 1 / `0xFF`: blocked (`+0x128` = 3); 0: `Field_LeaderPushObjects` then `Area121_PushObject`, else `0x415680` (pace 3 or 4) and at pace 4 a facing change is undone, else the step (`Field_JumpStart`, `Field_JumpCheckHeight`, `+9 - 1`, `Field_LeaderStepTick`, `+0x137` = `+2` = 1) |
| `0x41BC60` | `Area121_PushObject` | `0x133` | called | area `0x79` only: object k two `Field_DirectionSteps[+8]` steps ahead (`Sprite_ObjectAt(x, z, 1)`); the push button held and k facing the leader's way: k's `+0x80 |= 1`; else `Area121_StepAround(x, z, k)` answering: the step |
| `0x41BDA0` | `Area121_StepAround` | `0x69` | called | k `0xFF`: al 0; `+8` = `Area121_DirectionTo(x, z, k)`, then up to eight turns until `Field_LeaderStepTarget` 0 and `Sprite_ObjectAt(x, z, 0)` none: al 1 |
| `0x41BE10` | `Area121_DirectionTo` | `0xA2` | called (and area 104's `0x415406`) | the facing from object k to (x, z) by the signs of dx, dz: (-,-) 0, (-,+) 6, (-,0) 7, (+,+) 4, (+,-) 2, (+,0) 3, (0,+) 5, else 1 |
| `0x41BEC0` | `Area121_StepOffObject` | `0x71` | called | area `0x79` only: the object on the leader's own point; `Area121_StepAround` answering: the step, al 1 |
| `0x41BF40` | `Area121_LeaderStep` | `0x157` | leader state 1 | `+0x137` = 1; `+9` counts down through `Field_LeaderStepTick`. At 0: `Field_Bit20Tick`, `Field_FloorDamage`; with `Field_InputFlags` bit 0 the cell under the leader - `0xAF`: `Field_ChangeArea(place, 0x903860, 0x90384C, 0x905B88)`, `0x904EE0` = 0, `0x937F98` = `0xC`, done; `0xC0`: `Area_LinkAt`. `Field_EdgeBits` + 1 (+ 2 on facings 2 and 6); `0x415640`; `Scenario_ArriveHook` answering (all of eax): `+2` = 0. Else `Field_Bit80Tick`, and a held direction or button goes straight on to `Area121_LeaderControl`; else `+0x137` = `+2` = 0 |
| `0x41C0A0` | `Area121_MenuButton` | `0x3A` | called (and `0x4150C4`) | `Field_MenuButton` pressed outside flag bit 6: `Sound_PlayEffect(0x105)`, `Field_Request` = 1, `+2` = 0, al 1 |
| `0x41C0E0` | `Area121_Request4Button` | `0x30` | called (and `0x4150DE`) | outside flag bit 13, word `0x903586` held: `Field_Request` = 4, `+2` = 0, al 1 |
| `0x41C110` | `Area121_TurnInput` | `0x72` | called (and `0x4150F4`) | `Input_Held` bit 14 `+8 ^= 4`, bit 13 `+ 1`, bit 15 `- 1`, bit 12 kept; al 1 and `+8 &= 7`; none al 0 |

## 6. Effect kind 0x5C, an effect-kind-0x18 state, an object trigger

`Effect_KindHandlers[0x5C]` (`0x6554C0`) is `0x462B60`: the same area-104
split as the leader's, to `0x415780` there and `0x41C190` elsewhere.

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x41C190` | `Area121_Kind5CRun` | `0x12` | `0x462B6F` (jmp) | `Area121_Kind5CStates` `0x62470C` by `+1` (5; ours aborts past) |
| `0x41C1B0` | `Area121_Kind5CStart` | `0xB1` | state 0 | `+8` = the leader's facing; `Sprite_InitFromEntry(Area_Descriptors[area] +8's dword + 8)`; `+0x5D..+0x5F` = `0x80`; `+1 + 1`; `Area121_Kind5CFollow`. Unless area `0x68` without key item `0xA`: four `Effect_FindFree` slots become kind `0x5C` at state 3 with delays 0, 8, `0x10`, `0x18` (the rings) |
| `0x41C270` | `Area121_Kind5CFollow` | `0xDB` | called | the leader's `+0x34`, `+0x38`, `+0x3E`; `Sprite_UpdateScreenA`; then (`0x41C2B0`, this function's tail) under `Draw_PassFlags & 0x1B` a gauge: `Area121_GaugeSprite(0xDC, 0x10, n)` and `0x415940` rows - `Field_StatusBits` bit 6: n 0, rows (0x40, 0), (0x40, 1); the leader's `+0xA` 0: n 0, rows (0x40, 0), (0x40 - the leader's `+0xB`, 1); else n 1 / 2 blinking on `Frame_Counter` bit 1, row (0x40, 0) |
| `0x41C350` | `Area121_GaugeSprite` | `0x87` | called (and area 104's three) | a draw-mode primitive and a SPRT `0x50 x 0x20` at (x, y), v `(n << 5) + 0x78`, CLUT `0x7B80`, slot 2 |
| `0x41C3E0` | `Area121_Kind5CFace` | `0x54` | state 1 | while not facing the leader's way: `Area121_Kind5CTurnStep` from either side; neither turning: the leader's facing = its own; `Area121_Kind5CFollow` |
| `0x41C440` | `Area121_Kind5CTurnStep` | `0xCB` | called | facings a, b within 2 (around the wrap by 8) and not equal: `+0x14` = -/+`0x40`, `+8` one step toward b, the leader's facing = `+8`, `+9` = 8, `Area121_Kind5CTurn`, `+1` = 2, al 1 |
| `0x41C510` | `Area121_Kind5CTurn` | `0x48` | state 2 | the angle `+0x6C += +0x14`, `& 0xFFF`; `+9 - 1`: at 0 back to state 1 (`Area121_Kind5CFace`), else `Area121_Kind5CFollow` |
| `0x41C560` | `Area121_RingWait` | `0x4B` | state 3 | `+0xA` counts down; at 0 the ring takes the leader's position `+ 0x800000` up, `Area121_RingRise`, state 4 |
| `0x41C5B0` | `Area121_RingRise` | `0x1DB` | state 4 (and area 104's) | `+0x3E += 0x10`, above `0x280` back to state 3; under `0x1B` a GTE-transformed semi-transparent diamond of radius `(h >> 7) * 10 + 0x10` at the ring (`MapView_LinkPrimAt`) |
| `0x41C790` | `Area121_DrawTopBand` | `0xDD` | `EffectKind18_States` entry 70 (`0x654184`) | with `Cond_ByteFE` 1 or 2 and pass bit 2: an opaque POLY_G4 across the top of the screen, height `Area121_BandHeights[Cond_ByteFE]` (`0x624720`), blue at the top edge, white at the bottom, slot 7 |
| `0x41C870` | `Area121_Trigger38` | `0x1D` | `Field_ObjectTriggers` id 38 (`0x662EB4`) | `ScriptFlags_Set40`; tail kind `0x9039F3` = `0x2C` (`0x56DE50`, the world map's field-hook runner), state 0, arg `0xE`; al 0 |

## 7. Latent defects (described, not fixed)

The owner's rule (round9 doc section 6): ours aborts where the original
would fault; a silent read or write past a table into mapped memory is
reproduced as the original makes it and described here.

1. **`Area121_PushObject` indexes `Sprite_ObjectsExtra` by `Sprite_ObjectAt`'s
   "none" unchecked.** With the push button held and no object two steps
   ahead (`0xFF`), it reads "extra record `0xE1`" (`0x80B0A4`, inside `.bss`
   before `Gfx_ClutStripSource`) and, when that byte's low three bits equal
   the leader's facing, sets bit 0 of `0x80B124`. Reachable in area 121's
   leader state 12 whenever a push meets no object two steps on. Ours makes
   the same read and write (no abort: the original does not fault here).
2. **`Area121_Kind5CStart` writes an effect record from `Effect_FindFree`
   unchecked**: with no free slot (`0xFF`) the four stores land in "record
   255" (`0x7E9160`). Reproduced (S09's breath pool is the precedent).
   Ours aborts only on an area number past `Area_Descriptors`' 200 or a null
   descriptor, which the original would read through.
3. **`Field_DirectionSteps` by the whole facing byte** (`Area121_PushObject`)
   and `Field_ActorStates` by `Field_State +0x148` (`Area121_LeaderControl`):
   unchecked reads, reproduced (as `event_ops.cpp`'s leader code does).
4. **The world map's**: the unbounded plate search (`Area121_PlateAnims` has
   four entries), `Area121_Cells` by `+0xB` unchecked (one record, then
   zeros and the next data), the unchecked `.data` dispatches (ours aborts
   past each of the eight tables) - area 45's set, [`area_w1b.md`](area_w1b.md).
5. **Area 104's check inside area 121's code**: `Area121_Kind5CStart` tests
   area `0x68` for the key item, but `0x462B60` never sends area 104 here -
   dead in this copy (the folded body is shared; area 104's own `0x415780`
   is AR2E's).

## 8. What reaches it, calls across groups

- **Reach - no recorded route reaches any of the 54.** A scan of every
  `analysis/calltrace/*/*.tsv` for addresses in `0x41A9D0..0x41C890` finds
  none (the world-map route plays area 33's copy; leader state 12 and effect
  kind 0x5C do not appear). Fuzz only.
- **Cross-group raw-address calls** (`area_w3b_callees.h`): `0x415640`,
  `0x415680`, `0x4156C0`, `0x415940` - area 104's block, **group AR2E's**
  this wave (folded bodies area 121's code calls). Named and ours:
  `Field_Leader*`, `Field_Jump*`, `Field_Bit*Tick`, `Field_FloorDamage`,
  `Field_ChangeArea`, `Area_LinkAt`, `Scenario_ArriveHook`,
  `Sprite_ObjectAt`, `Sprite_InitFromEntry`, `Sprite_UpdateScreenA`,
  `KeyItem_Has`, `Effect_FindFree`, `ScriptFlags_Set40` / `_Clear40`,
  `WorldMap_DrawNeedle`, `WorldMap_PinSprite`, `WorldMap_RecordIndex`, the
  `Gte_*` / `Gpu_*` set, `MapView_LinkPrimAt`, `Prim_SetTexture`,
  `Msg_OpenScript`, `Text_DrawAt`. Nothing of `0x4220D0` (AR3F's).
- **Inbound from outside the band** (for the rebinding pass): engine
  `0x52FE9F` (jmp, `Field_LeaderStates[12]`) into `Area121_LeaderRun`;
  engine `0x462B6F` (jmp, `Effect_KindHandlers[0x5C]`) into
  `Area121_Kind5CRun`; `EffectKind18_States[70]` names `Area121_DrawTopBand`;
  `Field_ObjectTriggers` id 38 names `Area121_Trigger38`; area 104's code
  and data (AR2E) into seven functions (section 1).
- `analysis/calltrace/entries_logic.txt`: 45 lines appended; nine extents
  were there already (`0x41B1E0 1C5`, `0x41B3B0 BC`, `0x41BC60 133`,
  `0x41BDA0 69`, `0x41BE10 A2`, `0x41C0A0 3A`, `0x41C0E0 30`, `0x41C270 DB`,
  `0x41C440 CB`). Host lines with larger extents cover `0x41A9D0..0x41AFAA`
  (`0041A640 96A`), `0x41AFB0` (`227`), `0x41B470` (`7EB`), `0x41BEC0`
  (`1D7`), `0x41C110` (`151`), `0x41C350` (`E4`), `0x41C510` (`9B`),
  `0x41C5B0` (`E7C`): the smaller extents are appended beside them.

## 9. The fuzz

`BOF3X_SHADOW=area_w3b` (`src/game/area_w3b_fuzz.cpp`): five `Run` calls
under the one shadow name, each with `Group::area` its area (120, then 121
four times), the real descriptors and tables in place.
`BOF3X_AR3B_GROUP=n` runs one alone (120, 121, 1210 the world map, 1211
the leader, 1212 the effects; the controls script's shortcut).

- **Area 120** (6,000 rounds a function): the choice byte 0, 1, 2, `0x80`,
  `0xFF` or any; the pointer `0x903804` a party record or field object (a
  region of the group's), its word `+0x8A` at the wrap.
- **Area 121's handlers** (6,000): `+4` 0 / 1 for the dispatcher; the
  camera distance at `0x5DC`, one step short, the sign's edges;
  `MoveScript_Object` and `Field_ActiveMember` pointers seeded and moved by
  the disturbance; `Effect_Objects` a region, `Effect_FindFree` `kByte
  0xFF..0x13`.
- **The world map** (4,000): area 87's group ([`area_w2b.md`](area_w2b.md)
  section 5) over area 121's tables - the six state tables as
  `DataTable`s, a packet buffer of the fuzz's, the place list, plate
  animations, the cell record and the direction / animation tables as
  regions (put back to the exe's bytes two rounds in three, from the fuzz's
  own literal addresses); the place one of the four (or of the plate
  animations') or not; `settle` keeps `+1` inside the plate table and the
  place in the last animation entry (the search has no bound).
- **The leader** (8,000): the area `0x79` two rounds in three (else `0x78`,
  `0x7A`, `0x179`, `0x68`); the facing a direction or any byte; the push
  button word and `Input_Held` sharing a bit half the time. The seed picks
  which of the five early-out stand-ins answers "yes" this round (or none,
  half the time), so half the rounds reach the walk; in the control
  state's rounds `Field_LeaderStepTarget` answers 0 two times in three, the
  pusher half the time, and `0x415680`'s stand-in writes the pace 3 or 4 it
  leaves; `Area121_TurnInput`'s stand-in turns `+8` by 0, +-1..3, 4 or to
  anything (the caller reads it after). The step-around / direction
  arguments are points about an object's position (below, at, above in
  each axis). Regions: `Field_ActorStates`' first four records, "extra record
  `0xE1`" (defect 1), the button map, `Input_Held` / `_Pressed`,
  `0x904EE0`, `0x937F98`. `Field_ChangeArea` masked `(u16, all, all, u8)`
  and `Area_LinkAt` `(u8, u8)` (the pushes carry stale bits above);
  `Scenario_ArriveHook` answers 0, `0x100` (al 0, eax not) or any.
- **The effects** (6,000): the leader's and the object's facings; the
  gauge's status bit, blink byte and charge; the turn's `+9`, `+0x14`,
  `+0x6C`; the rings' delay and height at `0x270`..`0x281` and the sign;
  `Cond_ByteFE` 0..3 and `0xFF`; `Area121_BandHeights` a region;
  `Effect_FindFree` answering `0xFF` (record 255 is a region, defect 2). The
  ring's matrix chain (`Gte_PushMatrix` .. `Gte_RotTransPers4`,
  `Gte_PrimDepths4_10`) runs for real on both sides (`kThrough`: its
  pointers are stack locals), so what it computes shows in the packet.

**Result (in this worktree):** 0 mismatches in every run; rounds / calls to
the stand-ins in the `'*'` run: area 120 18,000 / 0; handlers 30,000 /
24,000; world map 100,000 / 196,773; leader 80,000 / 169,895; effects 66,000
/ 127,090. Every state-table entry reached (the plate states about 800 each,
frame and box states about 1,000, record-8 about 1,300, the camera states
about 3,000, the leader's step state 3,924, the effect's states 1,180..);
`0x415680` 589 calls, `Field_JumpCheckHeight` 452, `Field_ChangeArea` 323,
`Area_LinkAt` 287. `BOF3X_SHADOW='*'`: exit 0, 452 self-test lines, no
mismatch or Fatal, `inject: 4990 ours` (all 54 injected; 4,936 before).

## 10. Controls

CONTROLS_TEXT
