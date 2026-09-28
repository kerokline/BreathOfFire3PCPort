# World 3, areas 115..119: the world map's eighth copy, a Zenny gate, two party conveyors, twelve spawns

**Status:** IN PROGRESS (2026-09-28) - 56 functions ours
(`src/game/area_w3a.cpp`, shadow name `area_w3a`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 270,000 rounds; 197 controls planted, 194 refused by a count, 2 by a fault (their near variants by a count) and 1 equivalent under its area (its variant under the twin area refused) (section 9). Fuzz only: no
recorded route reaches any of the 56 (section 8). No divergence.

Group AR3A of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15): the
band `0x418BE0..0x41A9D0`, whole areas as `tools/area_rows.py --groups` cut
them. What each area *is* in the story is not read here: the names come
from what the code does. ("Conveyor" in the title is shorthand for what
areas 117 and 118's member frame does to party members standing in a
rectangle, section 4; the code does not say what the rectangle is.)

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` (group `AR3A`) and the tool's `--unit AREA115..119
--clones` list 57 starts, one ours: `WorldMapHud_Start` `0x419110` (round
eight's), in area 115's block and the state 0 of all eleven world maps' HUD
tables. **All 56 others are functions and all are taken: no start dropped,
none added.** Every one was read to its last instruction with capstone (the
scratch `adis.py`); every gap between them, and the band's tail after
`0x41A9C8`, is `nop` padding (the scratch `gaps.py`). The tool's clone rows
match the reading call site for call site; one function has a jump table
(`Area117_ChoiceMessage2`, five entries at `0x41A080` inside its `0x54`
bytes, a clone `JumpTable`).

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 115 | `0x620220` (only a choice table, `+0x34` -> `0x620218`, whose one entry `0x41D270` is group AR3C's; no init, as area 88's) | `WorldMap_Records` record 7 (`0x6539D4`: `+0`, `+4`, `+8`, `+0xC`, `+0x10`), `WorldMap_FieldHooks` entry 7, six state tables in its data block | `0x418BE0..0x419D21` | 24 (and `WorldMapHud_Start`) |
| 116 | `0x620620` | choice 0, handler 0, `Area_StepHook`'s case, object trigger 26, `Effect_KindHandlers[0xB8]` and its state table | `0x419D30..0x419E6A` | 6 |
| 117 | `0x6215B0` | handlers 0..6 (4..6 also choices 0..2), object trigger 33, the cell hook (`Area_CellHooks`' area `0x75`), engine code `0x46D7A9` | `0x419E70..0x41A3BC` | 11 |
| 118 | `0x621DF8` | handler 0, the cell hook (area `0x76`), engine code `0x46D7B6` | `0x41A3C0..0x41A72C` | 4 |
| 119 | `0x622B18` | choice 0, handlers 0 and 4..11 (= choices 1 and 5..12) | `0x41A730..0x41A9C7` | 10 |

**The tool's gaps**, each read and placed: `0x419E00` and `0x419FB0` are
`Field_ObjectTriggers` ids 26 and 33 (their areas by block); `0x419E20` is
`Effect_KindHandlers[0xB8]` (`0x655630`), area 116's effect; `0x41A0A0` and
`0x41A410` are called by engine code (effect kind `0x70`'s handler
`0x46D780`, section 4); `0x41A2D0` and `0x41A640` are their rectangle
searches ("reached by no area" in the tool's words: they are called, not
tabled). No tail kind is armed through a register in the band (the three
`0x9039F3` stores are immediates: kinds 4, `0x2C`, `0xA`).

**Shared bodies:** `0x419D80` (`Area116_ToArea100`) is handler 0 of areas
36, 59 and 116 (the tool's "shared" row); it lies in 116's block, so it is
this group's. The band's own tables also name functions of other groups,
called only through those tables and not by this code: `Area61_ChoiceMark4`
`0x40B590` (area 116's choices 3 and 4), `Area59_EffectGround` `0x40B4F0`
(area 116's effect state 0), `Area22_ArmTailOnYes` `0x403050` (area 117's
handler 7 / choice 3), `Area80_ResetCameraShift` `0x40F530` (area 119's
handler 3), `Area65_Record8Move` `0x40C490` and `Area45_Record4Tick`
`0x408990` (area 115's record tables, as every world map's), and in other
bands `0x420850`, `0x420870`, `0x421FB0` (AR3F), `0x41F320` (AR3E),
`0x41D270` (AR3C), `0x42A4B0` and `0x4253C0` (later bands).

## 2. Area 115: the world map's eighth copy

Area 115 is `WorldMap_Records` record 7 (area byte `0x73`) and
`WorldMap_FieldHooks` entry 7 ([`worldmap_area.md`](worldmap_area.md)
section 5). **Its code is area 88's** ([`area_w2b.md`](area_w2b.md)
section 4, itself area 45's and area 16's) **instruction for instruction
over its own tables**: a capstone compare of the 24 pairs (the scratch
`cmp.py --b88 115`: instruction by instruction, addresses inside a function
made relative) finds only jump targets, calls to each copy's own functions
and table operands differing, and these operands more:

| Function | Area 88 | Area 115 |
|---|---|---|
| `PlateStart` | `push 0x157` (the plate's animation bank) | `push 0x1C7` |
| `PlaceMessage` | name sets of 12 bytes (`lea ecx, [ebp + ebp*2]; lea edi, [ecx*4 + ...]`, `add eax, 0xC`), text rows to `0x904E40` (eleven) | **sets of 6 bytes (`lea edi, [ecx*2 + 0x620305]`, `add eax, 6`), text rows to `0x904D80` (five)** - `0x178` bytes, the same length |
| `DrawHud` | label offset from the dword `0x803580` | the same |

So area 115 is area 88's copy with area 65's name-set width (six bytes:
five items and the id) and two sets where 87 and 88 have three. The rest
is described once, in the docs above. **Ours is AR2B's world-map body
copied** (`area_w2b.cpp`'s anonymous namespace, `PlaceMessage` ..
`DrawDrift`, text for text) over a `WorldMapTables` for area 115
(`area_w3a_callees.h`, the same struct), with a named entry per address:
this wave's other world-map groups (AR2E's area 104, AR3B's 121) meet the
same choice, and one shared body for every copy is a refactor for the
rebinding pass, not a behaviour. The copy's names are `Area115_*` as the
merged copies':

| Function | Area 88 | Area 115 | Size |
|---|---|---|--:|
| `PlaceMessage` (field hook) | `0x410D90` | `0x418BE0` | `0x178` |
| `PlateRun` (record `+0`) | `0x410F10` | `0x418D60` | `0xD6` |
| `PlateStart` / `Show` / `Grow` / `Hold` / `Shrink` (plate states 0..4) | `0x410FF0..` | `0x418E40`, `0x418EA0`, `0x418FF0`, `0x419040`, `0x4190A0` | `0x51`, `0x142`, `0x41`, `0x58`, `0x50` |
| `HudRun` (record `+0xC`) / `HudFrame` | `0x4112C0` / `0x4112E0` | `0x4190F0` / `0x419130` | `0x12`, `0xA` |
| `FrameStep` / `SlideIn` / `Hold` / `SlideOut` | `0x4112F0..` | `0x419140`, `0x419160`, `0x419190`, `0x4191C0` | `0x12`, `0x21`, `0x28`, `0x41` |
| `BoxStep` / `SlideIn` / `Hold` / `SlideOut` | `0x4113F0..` | `0x419210`, `0x419230`, `0x4192A0`, `0x419310` | `0x12`, `0x62`, `0x6E`, `0x57` |
| `DrawFrame` / `DrawSprite` / `DrawHud` | `0x411550` / `0x411720` / `0x4117E0` | `0x419370` / `0x419540` / `0x419600` | `0x1C5`, `0xBC`, `0x58` |
| `Record8Run` (record `+8`) / `Record8Place` | `0x411840` / `0x411860` | `0x419660` / `0x419680` | `0x12`, `0x154` |
| `Record4Run` (record `+4`) / `Record4MarkCell` | `0x4119C0` / `0x4119E0` | `0x4197E0` / `0x419800` | `0x12`, `0xB5` |
| `DrawDrift` (record `+0x10`) | `0x411AA0` | `0x4198C0` | `0x462` |

Its state tables' entries are the other copies' shared ones: HUD state 0
`WorldMapHud_Start`, frame state 0 `WorldMap_FrameWait`, box state 0
`WorldMapHud_BoxWait`, record-8 states 0 and 2 `0x4253C0` and
`Area65_Record8Move`, record-4 state 1 `Area45_Record4Tick`.

**The tables** (`[[data]]`, each in the area's data block):

| Table | Address | What |
|---|---|---|
| `Area115_PlateAnims` | `0x620148` | (u16 place, u8 animation, u8) x 5 |
| `Area115_Cells` | `0x62015C` | (x, z, -, id) x 2, then a zero record (record 7's `+0x14`) |
| `Area115_PlaceMessages` | `0x620264` | 5 rows of `0x20` |
| `Area115_NameSets` | `0x620304` | (id, 5 items) x 2 |
| `Area115_PlateStates` / `HudStates` / `FrameStates` / `BoxStates` | `0x620310` / `0x620324` / `0x62032C` / `0x62033C` | 5 / 2 / 4 / 4 |
| `Area115_Sprites` / `Buttons` | `0x62034C` / `0x6203A4` | 22 x 4 / 6 x 4 (the second legend reads eight) |
| `Area115_Record8States` / `Directions` / `Record8Anims` / `Record4States` | `0x6203BC` / `0x6203C8` / `0x6203D8` / `0x6203E0` | 3 / 4 x 4 / 4 x 2 / 2 |
| `Area115_DriftUV` | `0x6203E8` | u, v, size (4 each) |
| `Area115_Choices` | `0x620218` | the descriptor's `+0x34`, one entry |

The PSX record of `AREA115.EMI` lists seven hooks
(`names/area_records.toml`); they are not paired to the PC copy here (the
other world-map groups did not pair theirs either).

## 3. Area 116: a Zenny gate, a change of area, a drop-in step

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x419D30` | `Area116_ChoicePay10000` | `0x4C` | choice 0 (`Area116_Choices` `0x620608`) | the cursor `0x7DEE67` not 0: message 4 and the byte `0x9398CF` 6. Else `Party_Zenny` 10,000 or more (unsigned): message 2 and 10,000 taken; less: message 3 and `0x9398CF` 6 |
| `0x419D80` | `Area116_ToArea100` | `0x1A` | handler 0 of areas 116, 36 and 59 (PSX `0x801F2D54`) | `Field_ChangeArea(100, 0x460000, 0x340000, 0x81)` |
| `0x419DA0` | `Area116_StepHook` | `0x5E` | `Area_StepHook`'s case (`event_ops.cpp` `kStepHandlers`) | `Area100_StepHook` `0x414330` with two constants moved (a capstone compare: `Cond_ByteFD` 2 where it has 3, x from `0x46` where it has `0x45`): `Cond_ByteFD` 2, story flag `0x33`, x's high word `0x46..0x48` and z's `0x33..0x35` (16-bit compares), the leader's pose 0, 7 or 6: counter 0 = 0, `Party_DropIn(0)`, al 1; else al 0 |
| `0x419E00` | `Area116_Trigger26` | `0x16` | `Field_ObjectTriggers` id 26 | `ScriptFlags_Set40`; tail kind 4 with argument 8 (the state byte `0x9039F4` not written); al 0 |
| `0x419E20` | `Area116_EffectB8Run` | `0x12` | `Effect_KindHandlers[0xB8]` | `Area116_EffectStates` (`0x620664`: `Area59_EffectGround`, `Area116_EffectB8Ring`) by `Sprite_Current[1]` |
| `0x419E40` | `Area116_EffectB8Ring` | `0x2B` | effect state 1 | `Area100_EffectB7Ring` `0x4144B0` instruction for instruction: the record's `+0x34`, `+0x38`, `+0x3C` copied to a 16-byte stack block and handed to `0x4220D0` (group AR3F's) |

The choice table's entries 1 and 2 (`0x420850`, `0x420870`) and handler 1
(`0x421FB0`) are group AR3F's band; entries 3 and 4 are
`Area61_ChoiceMark4`. What spawns effect kind `0xB8` is not in this band.

## 4. Areas 117 and 118: the member frame, its rectangle, a floor switch

Areas 117 and 118 share three bodies over their own tables (a capstone
compare: `0x41A0A0` / `0x41A410`, `0x41A2D0` / `0x41A640`, `0x41A340` /
`0x41A6B0` differ only in the table operands). Ours is one body each over a
`TwinTables` (`area_w3a_callees.h`). The member frame is also
`Area49_EffectFrame` `0x409480`'s shape ([`area_w1c.md`](area_w1c.md),
group AR1C), less its first two story-flag tests and its `Field_Request` 5
branch, and over one rectangle where area 49 has seven zones.

| PC (117 / 118) | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x41A0A0` / `0x41A410` | `Area117_MembersFrame` / `Area118_MembersFrame` | `0x223` | engine: `0x46D780` (effect kind `0x70`'s handler, the one that calls `Area49_EffectFrame` for area `0x31`) calls them at `0x46D7A9` / `0x46D7B6` when `Game_AreaNumber` is `0x75` / `0x76`; `Sprite_Current` the effect record | `Field_ScriptFlags` bit 13 cleared; for each member m below `Field_MemberCount` (read again after each member), with b = 1 << (m & 31) and b8 its low byte: `Field_State` and `Sprite_Current` = record m, the copy's `MemberRect(m)`. Outside: a member marked in the effect's `+0xB` (b8) is unmarked there and in `Field_ScriptFlags2`. Inside, unmarked and b in neither flag word: `Field_State +0x128` = 2, `+9` = 0, marked in both, `+8` = the rectangle's facing (^ 4 while its story flag is set), `Sprite_EnsureAnimation(+8)`. Inside and marked: `+9` 0 `Field_JumpSetUp` (and `Field_JumpCamera` for member 0), else the facing (^ 4 by the flag) and, when `+8` is not it, `+8` = it, `+0xC`, `+0x10`, `+0x14` negated, `+9` = 8 - `+9`; then `Field_LeaderStepTick`, `+9` - 1, `Sprite_ScriptTick`, bit 13 set. After: `Field_Request` 5 clears bit 13; `Sprite_Current` put back (`Field_State` left at the last member's record) |
| `0x41A2D0` / `0x41A640` | `Area117_MemberRect` / `Area118_MemberRect` | `0x6A` | called by the frame | party record (member & 0xFF)'s `+0x34` / `+0x38` against each `(x0, z0, x1, z1)` (cell bytes << 16, signed, inclusive): its index, or `0xFF` - one record each |
| `0x41A340` / `0x41A6B0` | `Area117_SwitchHook` / `Area118_SwitchHook` | `0x7D` | `Area_CellHooks`' area `0x75` / `0x76` | `(x, z)`: the first `(x, z, facing, flag)` record whose bytes are the arguments' and facing the leader's `+8`; none (the index at the count) al 0; story flag `0x1C` set al 0; else `Flags_Toggle` of its flag, `Effect_HoldFlag1C(0xF)`, `Sound_PlayEffect(0x203)`, al 1 |

Both areas' rectangle and switch carry the same story flag, `0x85`: the
switch toggles it, and while it is set the frame turns members the other
way (`^ 4`). What the flag is in the story is the scripts' to say.

Area 117's other functions:

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x419FB0` | `Area117_Trigger33` | `0x1D` | `Field_ObjectTriggers` id 33 (PSX `0x801F3F70` by the gap) | `ScriptFlags_Set40`; tail kind `0x2C` (engine `0x56DE50`) armed, state 0, sub-kind 5; al 0 |
| `0x419FD0` | `Area117_ChoiceArmTailA` | `0x28` | choice 0, handler 4 (PSX `0x801F3FB0`) | message `0xFFFF`; the cursor 0 arms tail kind `0xA` (state 0, argument `0x1C`) |
| `0x41A000` | `Area117_ChoiceMessage1` | `0x32` | choice 1, handler 5 (PSX `0x801F3FF0`) | the message `Area117_Messages1[the cursor, s8]` (unchecked: a negative cursor reads before the table); a cursor 0..4: counter 0 = `0xA`, `Sound_PlayEffect(0x205)` |
| `0x41A040` | `Area117_ChoiceMessage2` | `0x54` | choice 2, handler 6 (PSX `0x801F4048`) | the message `Area117_Messages2[the cursor, s8]`; for a cursor 0..4 (unsigned) by the five-entry jump table: 2 counter 0 = `0x14`; 0, 1, 3, 4 counter 0 = `0xA` and `Sound_PlayEffect(0x205)` |

Handler 7 / choice 3 is `Area22_ArmTailOnYes` (group AR0B's).

## 5. The spawns (areas 117..119) and area 119

Twelve handlers of one shape, area 11's `Area11_SpawnEffect`
([`area_011.md`](area_011.md)): party record m's words `+0x30` (z) and
`+0x2E` (x), the member's id from the first party list (`0x904062 + m`),
`Sprite_Current` made record m, `Effect_Spawn(kind, 0, table[id], x, z)`
(the table indexed unchecked; every id byte stays in `.data`), an answer
other than `0xFF` to `Sprite_Current[0xB]` (read again after the call).
Ours is one `SpawnAtMember(m, table, kind)`.

| PC | Name | Size | Root (PSX) | m | kind | table |
|---|---|--:|---|--:|--:|---|
| `0x419E70` | `Area117_Spawn3AtMember2` | `0x46` | 117 handler 0 (`0x801F3D70`) | 2 | 3 | `Area117_SpawnKinds3` `0x620670` |
| `0x419EC0` | `Area117_Spawn4AtMember2` | `0x46` | 117 handler 1 (`0x801F3DF0`) | 2 | 4 | `Area117_SpawnKinds45` `0x62067C` |
| `0x419F10` | `Area117_Spawn5AtMember2` | `0x46` | 117 handler 2 (`0x801F3E70`) | 2 | 5 | `Area117_SpawnKinds45` |
| `0x419F60` | `Area117_Spawn2AtMember2` | `0x46` | 117 handler 3 (`0x801F3EF0`) | 2 | 2 | `Area117_SpawnKinds2` `0x620688` |
| `0x41A3C0` | `Area118_Spawn1AtLeader` | `0x42` | 118 handler 0 (`0x801F4B88`) | 0 | 1 | `Area118_SpawnKinds` `0x621610` |
| `0x41A730` | `Area119_Spawn1AtLeader` | `0x42` | 119 handler 0 / choice 1 (`0x801F4138`) | 0 | 1 | `Area119_SpawnKindsA` `0x621E40` |
| `0x41A780` | `Area119_Spawn4AtLeader` | `0x42` | handler 4 / choice 5 (`0x801F4224`) | 0 | 4 | `Area119_SpawnKindsA` |
| `0x41A7D0` | `Area119_Spawn1AtLeaderB` | `0x42` | handler 5 / choice 6 (`0x801F42A4`) | 0 | 1 | `Area119_SpawnKindsB` `0x621E4C` |
| `0x41A820` | `Area119_Spawn1AtMember1` | `0x42` | handler 6 / choice 7 (`0x801F4324`) | 1 | 1 | `Area119_SpawnKindsB` |
| `0x41A870` | `Area119_Spawn1AtMember2` | `0x46` | handler 7 / choice 8 (`0x801F43A4`) | 2 | 1 | `Area119_SpawnKindsB` |
| `0x41A8C0` | `Area119_Spawn3AtMember2` | `0x46` | handler 8 / choice 9 (`0x801F4424`) | 2 | 3 | `Area119_SpawnKindsB` |
| `0x41A910` | `Area119_Spawn4AtMember2` | `0x46` | handler 9 / choice 10 (`0x801F44A4`) | 2 | 4 | `Area119_SpawnKindsA` |

(The member-2 copies read the list byte as `dword [0x904064] & 0xFF`, the
others as a byte: the same value.) The kind is `Effect_Spawn`'s first
argument, the movement-script ops `90..95`'s op - `0x90`
(`symbols.toml`).

Area 119's other three:

| PC | Name | Size | Root (PSX) | What |
|---|---|--:|---|---|
| `0x41A960` | `Area119_SetRowFlag3C` | `0x1B` | handler 10 / choice 11 (`0x801F4524`) | `Flags_Set(the chapter's row [0x929ED0], 0x3C)`, `Sound_PlayEffect(0x203)` |
| `0x41A980` | `Area119_ClearRowFlag3C` | `0x1B` | handler 11 / choice 12 (`0x801F4554`) | the same flag cleared |
| `0x41A9A0` | `Area119_ChoiceCounter0` | `0x28` | choice 0 (`0x801F4584`, the table's anchor) | message `0xFFFF`; the cursor (s8) 0: counter 0 = `0xA`, 1: `0x14` |

Its handler table (`Area119_Handlers` `0x622AE8`, the choice table
`0x622AE4` one entry before) names `0x41F320` (AR3E's band), `0x42A4B0` (a
later band) and `Area80_ResetCameraShift` as handlers 1..3. The PSX twins
above are `analysis/area_pairs.json`'s pairs by table slot (the section
hashes match `names/area_records.toml`); `pairs_propagated.json` agrees
for every one it lists.

## 6. The fuzz

`BOF3X_SHADOW=area_w3a` (`src/game/area_w3a_fuzz.cpp`): five `Run` calls
under the one shadow name, `Group::area` each area's number, the real
descriptors and tables in place. `BOF3X_AR3A_AREA=n` runs one area's group
alone (the controls script's shortcut).

- **Area 115** (4,000 rounds a function): area 88's group from
  `area_w2b_fuzz.cpp` (itself area 45's) copied over area 115's tables,
  with the name-set count made a field (`Fz::sets`, 2 here; AR2B's code
  assumed three). The six state tables as `DataTable`s, the packet buffer,
  items and names of the fuzz's, the copy's plate, cell (with its zero
  record), place, name-set, direction and drift tables as regions, put
  back to the exe's bytes two rounds in three; the text rows' region
  `0xA0` bytes; the fuzz's own literal table addresses (`kFz115`), so a
  wrong constant in ours shows.
- **Area 116** (6,000): the cursor 0 mostly, 1, `0xFF`, `0x80`, `0x7F`;
  `Party_Zenny` at 10,000 and one either side, the sign's and 32 bits'
  edges, or anything; the step hook's `(x, z)` 16.16 words whose high
  words are at and about the box (`0x46..0x48`, `0x33..0x35`, one past each
  end, `+0x10000`, `+0x8000`) with any low word, `Cond_ByteFD` 2 mostly and
  1, 3, `0x82`, 0, the leader's pose 0 / 7 / 6 and 1, 5, 8, `0x80`, `0x87`;
  the triggers' `(object, 0x904030)`; the effect state table as a
  `DataTable`, `Sprite_Current[1]` 0 or 1; `0x4220D0` a recorder that logs
  the three dwords its pointer names; the byte `0x9398CF` a region.
- **Areas 117 and 118** (6,000 each, one group body, the area's
  `FzTwin`): the spawns' party-list ids 0..11 mostly, else any byte; the
  choices' cursors at every case, their edges and the sign's; the member
  frame with the effect's mask none / one / several / every bit, the three
  members' bits in `Field_ScriptFlags` / `Field_ScriptFlags2` mostly clear
  (bit 13 too), each record's `+9` 0 / 1 / 7 / 8 / `0xFF` and `+8` the
  rectangle's facing, its `^ 4`, or any direction, `Field_Request` 5 / 0 /
  2 / 4, `Field_MemberCount` 0..3; the rectangle search stood in by a
  recorder answering 0 or `0xFF`; `MemberRect` itself with its member drawn
  by the seed (0..2, or 3 with bits above - an `args` hook's writes are
  lost) and that record's x and z at each rectangle edge, a cell's
  half-step either side, or the rectangle's middle; the switch's `(x, z)`
  the record's bytes two rounds in three (with bits above, one off, bit 7
  flipped) and the leader facing its way. The group's `disturb` flips a
  member's bit in either flag word (or bit 13), a record's `+9` or `+8`,
  or `Field_Request`.
- **Area 119** (4,000): the spawns' ids; the cursor; the row pointer
  `0x929ED0` a region.

Listed callees beyond the standard set: `Effect_Spawn` (`kByte 0xFE..0x02`,
so "none" `0xFF` is answered), `ScriptFlags_Set40` / `Clear40`, the two
`MemberRect`s by address (`kByte 0xFF..0x00`), `Field_JumpSetUp`,
`Field_JumpCamera`, `Flags_Toggle` (the index masked to a byte: it is
pushed with `Flags_Test`'s answer above it), `Effect_HoldFlag1C`,
`0x4220D0`, and area 115's copy callees (AR2B's list).

**Result (in this worktree):** 0 mismatches in every run; rounds / calls
to the stand-ins: area 115 100,000 / 501,942; 116 36,000 / 25,824; 117
66,000 / 56,496; 118 24,000 / 28,392; 119 40,000 / 44,000. Every
state-table entry reached (area 115's plate states 771..824 each, frame
and box states about 1,000 and the frame hold 5,050, record-8 about 1,330,
record-4 about 2,000; area 116's effect states 2,963 / 3,037); area 116's
`Party_DropIn` 62, `Flags_Test` 1,762; area 117's `Field_JumpSetUp` 519,
`Field_JumpCamera` 279, `Flags_Toggle` 226, `Sprite_EnsureAnimation`
1,356, `Field_LeaderStepTick` 2,421; area 118's 531 / 283 / 246 / 1,303 /
2,518. `BOF3X_SHADOW='*'`: exit 0 on the first run, 453 self-test lines, no mismatch or Fatal, `inject: 4992 ours` (all 56 of this group's injected; 4,936 before).

## 7. Latent defects (described, not fixed; the fuzz keeps inside them)

1. Area 115 has area 45's and 88's: the unbounded plate and cell searches
   (a place in no plate entry reads on through `.data`; a leader's cell in
   no cell record stops at the zero record only for cell (0, 0), else
   reads on through the descriptor's data), a found cell whose id is in
   neither name set reads a third set from `Area115_PlateStates`' bytes
   (code addresses as item ids), the unchecked record and table indexes,
   and the unchecked `.data` dispatches (ours aborts past each of the six
   tables, and past `Area116_EffectStates`' two, as AR0C's..AR2D's do).
2. The member frames walk records to `Field_MemberCount` unchecked
   against `ObjTrio`'s three (a count above 3 would write `+0x128`, `+9`,
   `+8` past them); their bit b8 is 0 for a member index of 8 and more (the
   8-bit shift), so such a member would never be marked. `MemberRect`
   indexes records by member & 0xFF unchecked. The field keeps the count
   at 1..3: none of it is reached.
3. The frames leave `Field_State` at the last member's record (they put
   back `Sprite_Current` only), as `Area49_EffectFrame` does.
4. The spawns index their tables by a member's id unchecked, and area
   117's two message choices index their tables by the s8 cursor (a
   negative cursor reads before the table): every read stays in `.data`.
5. `Area116_EffectB8Ring` hands `0x4220D0` a 16-byte stack block whose
   fourth dword it never writes (a stale word; the callee reads three -
   ours passes 0), as `Area100_EffectB7Ring`.

## 8. What reaches it, calls across groups

- **Reach - no recorded route reaches any of the 56.** The world-map
  route's traces (`analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv`,
  `hidden_worldmap/bof3x.log`, `analysis/hidden_reached_worldmap.json`)
  and every other `analysis/calltrace` trace name no call into the band
  (the scratch `reach.py`; only the `inject` line of `WorldMapHud_Start`
  appears): the route plays area 33's copy. The
  tool's "live" mark on area 115 is the shared `WorldMapHud_Start`'s. The
  coordinator's world-map A/B can only show that nothing moved.
- **Cross-group raw-address call:** `0x4220D0` (group AR3F's this wave),
  from `Area116_EffectB8Ring` (`area_w3a_callees.h`). Named and ours:
  `Field_ChangeArea`, `Flags_Test`, `Flags_Set`, `Flags_Clear`,
  `Flags_Toggle`, `Effect_HoldFlag1C` (SX2's), `Party_DropIn`,
  `ScriptFlags_Set40` / `Clear40`, `Field_JumpSetUp`, `Field_JumpCamera`,
  `Field_LeaderStepTick`, `Sprite_ScriptTick`, `Sprite_EnsureAnimation`,
  `Sound_PlayEffect`, `WorldMap_PinSprite`, `WorldMap_DrawNeedle`,
  `WorldMap_RecordIndex`, `Item_NamePtr`, the standard set. Capcom's:
  `Effect_Spawn`.
- **Inbound from outside the band** (for the rebinding pass): engine
  `0x46D7A9` / `0x46D7B6` (in `0x46D780`) into `Area117_MembersFrame` /
  `Area118_MembersFrame`; `event_ops.cpp`'s `kStepHandlers` names
  `0x419DA0` (`Area116_StepHook`) by address; areas 36's and 59's
  descriptors name `Area116_ToArea100`; `Effect_KindHandlers[0xB8]` names
  `Area116_EffectB8Run`; `Field_ObjectTriggers` 26 and 33, `Area_CellHooks`
  (areas `0x75`, `0x76`), `WorldMap_Records[7]` and `WorldMap_FieldHooks[7]`
  name the rest (read in place).
- `analysis/calltrace/entries_logic.txt`: 52 lines appended; `00419370
  1C5`, `00419540 BC`, `0041A0A0 223`, `0041A410 223` were there. Host lines
  with larger extents cover `0x419140` (`227`, mine `12`), `0x419600`
  (`79A`, mine `58`), `0x419DA0` (`2F9`, mine `5E`), `0x41A2D0` (`132`, mine
  `6A`), `0x41A640` (`96A`, mine `6A`): the smaller extents are appended
  beside them.

## 9. Controls

Planted one at a time in `area_w3a.cpp` (and, for the table constants,
`area_w3a_callees.h`) by a script (the scratch `controls.py`, not
committed): each anchored on a string the file holds once; plant, rebuild
(checking `area_w3a.cpp` recompiled), run the area's group alone
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w3a BOF3X_AR3A_AREA=n`), restore;
after the last, a rebuild and a clean full run (exit 0, 0 mismatches in all
five runs). The world-map body's controls (W) are AR2B's C series, the same
text, planted under area 115; the twins' shared body is planted under area
117. **197 planted, 194 refused by a count** (exit 3), at least one per
function, on the first run of the set; **2 refused by a fault**, each with
a near variant refused by a count:

- X9 (area 115's plate table moved on an entry): the unbounded search then
  reads on through `.data` and faults, as the original's would with a
  place in no entry; W58 (the animation read from the entry's pad byte) is
  refused by a count.
- E28 (area 116's effect state table moved on an entry): the second entry
  becomes the table's terminating 0 and the dispatch calls it; E25 (the
  other entry dispatched) is refused by a count.

**1 equivalent**: F63 (the switch hook comparing (z, x) instead of (x, z))
cannot be told apart in area 117, whose one switch record is at (0x22,
0x22); the same plant under area 118 (record (0x0B, 0x46)), F63b, is
refused by a count. Rounds are of a function's 4,000 (areas 115 and 119) or 6,000
(areas 116..118); in this worktree.

| # | Area | planted | refused in rounds |
|---|--:|---|---|
| W1 | 115 | PlaceMessage: state 2 handled as 1 | Area115_PlaceMessage 720 |
| W2 | 115 | PlaceMessage: waits on 3 | Area115_PlaceMessage 259 |
| W3 | 115 | PlaceMessage: 0x9039F5 kept | Area115_PlaceMessage 488 |
| W4 | 115 | PlaceMessage: 0xA0 a place | Area115_PlaceMessage 297 |
| W5 | 115 | PlaceMessage: Cond_ByteFA zero-extended | Area115_PlaceMessage 65 |
| W6 | 115 | PlaceMessage: x not read again | Area115_PlaceMessage 3 |
| W9 | 115 | PlaceMessage: 0xFE ends | Area115_PlaceMessage 292 |
| W10 | 115 | PlaceMessage: unseen name | Area115_PlaceMessage 628 |
| W11 | 115 | PlaceMessage: key item 0x17 | Area115_PlaceMessage 193 |
| W12 | 115 | PlaceMessage: name + 0x37 | Area115_PlaceMessage 744 |
| W13 | 115 | PlaceMessage: set + 0x17 | Area115_PlaceMessage 800 |
| W14 | 115 | PlaceMessage: request 3 | Area115_PlaceMessage 1001 |
| W15 | 115 | PlateRun: 0xA0 kind 3 | Area115_PlateRun 314 |
| W16 | 115 | PlateRun: bit 11 | Area115_PlateRun 1291 |
| W17 | 115 | PlateRun: the next state | Area115_PlateRun 4000 |
| W18 | 115 | PlateStart: bank + 1 | Area115_PlateStart 4000 |
| W19 | 115 | PlateStart: +0x29 4 | Area115_PlateStart 4000 |
| W20 | 115 | PlateShow: kind 2 animation 2 | Area115_PlateShow 322 |
| W21 | 115 | PlateShow: +9 7 | Area115_PlateShow 2011 |
| W22 | 115 | PlateShow: +0x44 | Area115_PlateShow 2025 |
| W23 | 115 | PlateGrow: step 0x1000 | Area115_PlateGrow 4000 |
| W24 | 115 | PlateHold: request 4 | Area115_PlateHold 392 |
| W25 | 115 | PlateHold: Game_Mode 2 holds | Area115_PlateHold 795 |
| W26 | 115 | PlateShrink: released on 4 | Area115_PlateShrink 277 |
| W27 | 115 | PlateShrink: back to 2 | Area115_PlateShrink 607 |
| W31 | 115 | FrameSlideIn: above 0x10 | Area115_FrameSlideIn 359 |
| W32 | 115 | FrameHold: mode 3 | Area115_FrameHold 658 |
| W33 | 115 | FrameSlideOut: below -0x30 | Area115_FrameSlideOut 59 |
| W35 | 115 | BoxSlideIn: below 0xC8 | Area115_BoxSlideIn 127 |
| W36 | 115 | BoxHold: 0x59 frames | Area115_BoxHold 263 |
| W37 | 115 | BoxSlideOut: above 0xF0 | Area115_BoxSlideOut 329 |
| W38 | 115 | BoxLeaves: bit 9 | Area115_BoxSlideIn 983; Area115_BoxHold 865 |
| W39 | 115 | DrawFrame: second key over seven | Area115_DrawFrame 1445 |
| W40 | 115 | DrawFrame: legend 3 y + 0x17 | Area115_DrawFrame 2518 |
| W41 | 115 | DrawFrame: party set & 0xFF | Area115_DrawFrame 53 |
| W42 | 115 | DrawSprite: CLUT 0x7B81 | Area115_DrawSprite 4000 |
| W43 | 115 | DrawSprite: semi by & 0x7F | Area115_DrawSprite 3 |
| W44 | 115 | DrawHud: cap at x + 0x7F | Area115_DrawHud 2938 |
| W45 | 115 | DrawHud: label & 0xFFF | Area115_DrawHud 2765 |
| W46 | 115 | Record8Place: bank 0x47 | Area115_Record8Place 4000 |
| W47 | 115 | Record8Place: first nudge sar 12 | Area115_Record8Place 1840 |
| W48 | 115 | Record8Place: up for +6 2 | Area115_Record8Place 1545 |
| W49 | 115 | Record4MarkCell: cell 0xA1 | Area115_Record4MarkCell 2755 |
| W50 | 115 | Record4MarkCell: released on 8 | Area115_Record4MarkCell 1115 |
| W51 | 115 | Record4MarkCell: bank 0x204 | Area115_Record4MarkCell 2755 |
| W52 | 115 | DrawDrift: wrap to -7 | Area115_DrawDrift 1663 |
| W53 | 115 | DrawDrift: x within 24 | Area115_DrawDrift 516 |
| W54 | 115 | DrawDrift: CLUT 0x78CC | Area115_DrawDrift 860 |
| W55 | 115 | DrawDrift: commit 0x44 | Area115_DrawDrift 1701 |
| W56 | 115 | DrawDrift: size from the u table | Area115_DrawDrift 859 |
| W57 | 115 | DrawDrift: +0x38 by b << 9 | Area115_DrawDrift 3078 |
| W58 | 115 | PlateShow: the animation from the pad byte | Area115_PlateShow 1026 |
| W59 | 115 | PlateGrow: no pin | Area115_PlateGrow 4000 |
| X1 | 115 | HudRun: the other entry | Area115_HudRun 4000 |
| X2 | 115 | HudFrame: the box first | Area115_HudFrame 4000 |
| X3 | 115 | FrameStep: the next entry | Area115_FrameStep 4000 |
| X4 | 115 | BoxStep: the next entry | Area115_BoxStep 4000 |
| X5 | 115 | Record8Run: the next entry | Area115_Record8Run 4000 |
| X6 | 115 | Record4Run: the other entry | Area115_Record4Run 4000 |
| X7 | 115 | kWm115: area 88's bank | Area115_PlateStart 4000 |
| X8 | 115 | kWm115: four place rows | Area115_PlaceMessage 61 |
| X9 | 115 | kWm115: plate animations from the second | a fault (exit 0xC0000005) |
| X10 | 115 | kWm115: sprites from the second | Area115_DrawSprite 3992 |
| X11 | 115 | kWm115: drift size from the next | Area115_DrawDrift 764 |
| X12 | 115 | kWm115: four text rows | Area115_PlaceMessage 513 |
| X13 | 115 | kWm115: name sets of five | Area115_PlaceMessage 446 |
| X14 | 115 | kWm115: one name set | Area115_PlaceMessage 96 |
| X15 | 115 | kWm115: directions from the second | Area115_Record8Place 3992 |
| X16 | 115 | kWm115: cells from the second | Area115_PlaceMessage 149; Area115_Record4MarkCell 2391 |
| X17 | 115 | kWm115: record-8 animations from the second | Area115_Record8Place 3960 |
| X18 | 115 | kWm115: buttons from the second | Area115_DrawFrame 1195 |
| X19 | 115 | kWm115: area 45's label cell | Area115_DrawHud 2938 |
| E1 | 116 | Pay: a negative cursor pays | Area116_ChoicePay10000 2192 |
| E2 | 116 | Pay: message 5 for no | Area116_ChoicePay10000 4244 |
| E3 | 116 | Pay: 0x9398CF 7 for no | Area116_ChoicePay10000 4244 |
| E4 | 116 | Pay: exactly 10,000 refused | Area116_ChoicePay10000 233 |
| E5 | 116 | Pay: a signed compare | Area116_ChoicePay10000 651 |
| E6 | 116 | Pay: 1,000 taken | Area116_ChoicePay10000 1603 |
| E7 | 116 | Pay: message 3 for paid | Area116_ChoicePay10000 1603 |
| E8 | 116 | Pay: 0x9398CF kept when short | Area116_ChoicePay10000 153 |
| E9 | 116 | ToArea100: area 101 | Area116_ToArea100 6000 |
| E10 | 116 | ToArea100: flags 0x80 | Area116_ToArea100 6000 |
| E11 | 116 | ToArea100: x and z swapped | Area116_ToArea100 6000 |
| E12 | 116 | StepHook: Cond_ByteFD 3 | Area116_StepHook 2381 |
| E13 | 116 | StepHook: flag 0x34 | Area116_StepHook 1807 |
| E14 | 116 | StepHook: x from 0x45 | Area116_StepHook 27 |
| E15 | 116 | StepHook: z four cells | Area116_StepHook 21 |
| E16 | 116 | StepHook: pose 5 for 6 | Area116_StepHook 38 |
| E17 | 116 | StepHook: counter 1 | Area116_StepHook 73 |
| E18 | 116 | StepHook: entry 1 | Area116_StepHook 73 |
| E19 | 116 | StepHook: al 2 | Area116_StepHook 73 |
| E20 | 116 | StepHook: x's low word | Area116_StepHook 73 |
| E21 | 116 | Trigger26: kind 5 | Area116_Trigger26 6000 |
| E22 | 116 | Trigger26: argument 9 | Area116_Trigger26 6000 |
| E23 | 116 | Trigger26: the state zeroed too | Area116_Trigger26 5978 |
| E24 | 116 | Trigger26: al 1 | Area116_Trigger26 6000 |
| E25 | 116 | EffectB8Run: the other entry | Area116_EffectB8Run 6000 |
| E26 | 116 | EffectB8Ring: x and z swapped | Area116_EffectB8Ring 6000 |
| E27 | 116 | EffectB8Ring: y from +0x40 | Area116_EffectB8Ring 6000 |
| E28 | 116 | kA116EffectStates: from the second | a fault (exit 0xC0000005) |
| F1 | 117 | Spawn3: kind 2 | Area117_Spawn3AtMember2 6000 |
| F2 | 117 | Spawn4: table C | Area117_Spawn4AtMember2 2050 |
| F3 | 117 | Spawn5: member 1 | Area117_Spawn5AtMember2 6000 |
| F4 | 117 | Spawn2: kind 3 | Area117_Spawn2AtMember2 6000 |
| F5 | 117 | Spawn: a 1 | Area117_Spawn3AtMember2 6000; Area117_Spawn4AtMember2 6000; Area117_Spawn5AtMember2 6000; Area117_Spawn2AtMember2 6000 |
| F6 | 117 | Spawn: none stored too | Area117_Spawn3AtMember2 1208; Area117_Spawn4AtMember2 1246; Area117_Spawn5AtMember2 1208; Area117_Spawn2AtMember2 1177 |
| F7 | 117 | Spawn: Sprite_Current not read again | Area117_Spawn3AtMember2 159; Area117_Spawn4AtMember2 162; Area117_Spawn5AtMember2 157; Area117_Spawn2AtMember2 160 |
| F8 | 117 | Spawn: x and z swapped | Area117_Spawn3AtMember2 6000; Area117_Spawn4AtMember2 6000; Area117_Spawn5AtMember2 6000; Area117_Spawn2AtMember2 6000 |
| F9 | 117 | Spawn: Sprite_Current not set | Area117_Spawn3AtMember2 4732; Area117_Spawn4AtMember2 4770; Area117_Spawn5AtMember2 4778; Area117_Spawn2AtMember2 4820 |
| F10 | 117 | Spawn: the next member's id | Area117_Spawn3AtMember2 5125; Area117_Spawn4AtMember2 5056; Area117_Spawn5AtMember2 5082; Area117_Spawn2AtMember2 5116 |
| F11 | 117 | Spawn: +0xC | Area117_Spawn3AtMember2 4790; Area117_Spawn4AtMember2 4751; Area117_Spawn5AtMember2 4785; Area117_Spawn2AtMember2 4820 |
| F12 | 117 | Trigger33: sub-kind 6 | Area117_Trigger33 6000 |
| F13 | 117 | Trigger33: the state not written | Area117_Trigger33 5971 |
| F14 | 117 | Trigger33: kind 0x2D | Area117_Trigger33 6000 |
| F15 | 117 | ArmTailA: argument 0x1D | Area117_ChoiceArmTailA 651 |
| F16 | 117 | ArmTailA: the message kept | Area117_ChoiceArmTailA 2027 |
| F17 | 117 | ArmTailA: kind 0xB | Area117_ChoiceArmTailA 651 |
| F18 | 117 | ArmTailA: cursor 1 arms | Area117_ChoiceArmTailA 1329 |
| F19 | 117 | Message1: cursor 5 counts | Area117_ChoiceMessage1 359 |
| F20 | 117 | Message1: the cursor zero-extended | Area117_ChoiceMessage1 1634 |
| F21 | 117 | Message1: sound 0x204 | Area117_ChoiceMessage1 2410 |
| F22 | 117 | Message1: counter 0xB | Area117_ChoiceMessage1 2409 |
| F23 | 117 | Message2: case 3 the odd one | Area117_ChoiceMessage2 702 |
| F24 | 117 | Message2: 0x15 | Area117_ChoiceMessage2 353 |
| F25 | 117 | Message2: a signed bound | Area117_ChoiceMessage2 1608 |
| F26 | 117 | Message2: table 1 | Area117_ChoiceMessage2 3282 |
| F27 | 117 | Frame: bit 13 not cleared first | Area117_MembersFrame 582 |
| F29 | 117 | Frame: out of it, Field_ScriptFlags2 kept | Area117_MembersFrame 715 |
| F30 | 117 | Frame: out of it, unmarked unconditionally | Area117_MembersFrame 776 |
| F31 | 117 | Frame: Field_ScriptFlags not tested | Area117_MembersFrame 553 |
| F32 | 117 | Frame: +0x127 | Area117_MembersFrame 1160 |
| F33 | 117 | Frame: +0x128 through Sprite_Current | Area117_MembersFrame 96 |
| F34 | 117 | Frame: +9 not zeroed | Area117_MembersFrame 939 |
| F35 | 117 | Frame: facing from the flag byte | Area117_MembersFrame 1153 |
| F36 | 117 | Frame: reversed by ^ 2 | Area117_MembersFrame 813 |
| F37 | 117 | Frame: animation +9 | Area117_MembersFrame 1160 |
| F38 | 117 | Frame: the jump camera for every member | Area117_MembersFrame 241 |
| F39 | 117 | Frame: +0x14 not negated | Area117_MembersFrame 1308 |
| F40 | 117 | Frame: +9 = 7 - +9 | Area117_MembersFrame 1291 |
| F41 | 117 | Frame: turned when facing already | Area117_MembersFrame 1643 |
| F42 | 117 | Frame: +9 + 1 | Area117_MembersFrame 1990 |
| F43 | 117 | Frame: bit 14 set | Area117_MembersFrame 1884 |
| F44 | 117 | Frame: request 4 | Area117_MembersFrame 521 |
| F45 | 117 | Frame: Sprite_Current not put back | Area117_MembersFrame 4380 |
| F46 | 117 | Frame: the count not read again | Area117_MembersFrame 413 |
| F47 | 117 | Frame: two members at most | Area117_MembersFrame 1761 |
| F48 | 117 | Frame: the other member asked | Area117_MembersFrame 5248 |
| F49 | 117 | Frame: turning, the flag from the facing byte | Area117_MembersFrame 1643 |
| F50 | 117 | Frame: in it, Field_ScriptFlags2 not marked | Area117_MembersFrame 1160 |
| F51 | 117 | Rect: x1 exclusive | Area117_MemberRect 247 |
| F52 | 117 | Rect: z0 exclusive | Area117_MemberRect 84 |
| F53 | 117 | Rect: x unsigned | Area117_MemberRect 324 |
| F54 | 117 | Rect: none 0xFE | Area117_MemberRect 4223 |
| F55 | 117 | Rect: z from +0x3C | Area117_MemberRect 1777 |
| F56 | 117 | Rect: z1 exclusive | Area117_MemberRect 117 |
| F57 | 117 | Switch: facing not tested | Area117_SwitchHook 328 |
| F58 | 117 | Switch: flag 0x1D | Area117_SwitchHook 646 |
| F59 | 117 | Switch: toggles the facing byte | Area117_SwitchHook 215 |
| F60 | 117 | Switch: hold 0xE | Area117_SwitchHook 215 |
| F61 | 117 | Switch: sound 0x204 | Area117_SwitchHook 215 |
| F62 | 117 | Switch: al 2 | Area117_SwitchHook 215 |
| F63 | 117 | Switch: (z, x) compared | equivalent under area 117 (its one switch is at (0x22, 0x22)); F63b refused |
| F64 | 117 | Switch: none goes on | Area117_SwitchHook 5354 |
| F65 | 117 | kTw117: the rectangle 6 bytes early | Area117_MembersFrame 2605; Area117_MemberRect 1777 |
| F66 | 117 | kTw117: the switch 4 bytes early | Area117_SwitchHook 646 |
| F67 | 117 | kTw117: area 118's MemberRect | Area117_MembersFrame 5248 |
| F68 | 117 | kSpawnTable117A + 1 | Area117_Spawn3AtMember2 4354 |
| F69 | 117 | kA117Messages1 + 2 | Area117_ChoiceMessage1 3461 |
| F70 | 117 | kA117Messages2 + 2 | Area117_ChoiceMessage2 3833 |
| F71 | 117 | Frame: the next member's mark | Area117_MembersFrame 4002 |
| G1 | 118 | Spawn118: kind 2 | Area118_Spawn1AtLeader 6000 |
| G2 | 118 | Spawn118: member 1 | Area118_Spawn1AtLeader 6000 |
| G3 | 118 | kTw118: the rectangle 6 bytes early | Area118_MembersFrame 2579; Area118_MemberRect 1796 |
| G4 | 118 | kTw118: the switch 4 bytes early | Area118_SwitchHook 696 |
| G5 | 118 | MembersFrame118: area 117's tables | Area118_MembersFrame 5286 |
| G6 | 118 | SwitchHook118: area 117's switch | Area118_SwitchHook 696 |
| G7 | 118 | MemberRect118: area 117's rectangle | Area118_MemberRect 1795 |
| G8 | 118 | kSpawnTable118 + 1 | Area118_Spawn1AtLeader 4556 |
| H1 | 119 | Spawn1AtLeader: kind 4 | Area119_Spawn1AtLeader 4000 |
| H2 | 119 | Spawn4AtLeader: table B | Area119_Spawn4AtLeader 2487 |
| H3 | 119 | Spawn1AtLeaderB: table A | Area119_Spawn1AtLeaderB 2465 |
| H4 | 119 | Spawn1AtMember1: the leader | Area119_Spawn1AtMember1 4000 |
| H5 | 119 | Spawn1AtMember2: member 1 | Area119_Spawn1AtMember2 4000 |
| H6 | 119 | Spawn3AtMember2: kind 1 | Area119_Spawn3AtMember2 4000 |
| H7 | 119 | Spawn4AtMember2: table B | Area119_Spawn4AtMember2 2491 |
| H8 | 119 | SetRowFlag3C: 0x3D | Area119_SetRowFlag3C 4000 |
| H9 | 119 | SetRowFlag3C: the story flags | Area119_SetRowFlag3C 4000 |
| H10 | 119 | SetRowFlag3C: sound 0x202 | Area119_SetRowFlag3C 4000 |
| H11 | 119 | ClearRowFlag3C: sets | Area119_ClearRowFlag3C 4000 |
| H12 | 119 | ClearRowFlag3C: 0x3B | Area119_ClearRowFlag3C 4000 |
| H13 | 119 | Choice0: 0x15 | Area119_ChoiceCounter0 421 |
| H14 | 119 | Choice0: cursor 2 | Area119_ChoiceCounter0 651 |
| H15 | 119 | Choice0: the message kept | Area119_ChoiceCounter0 1320 |
| H16 | 119 | kFlagRow + 4 | Area119_SetRowFlag3C 4000; Area119_ClearRowFlag3C 4000 |
| H17 | 119 | kSpawnTable119B + 1 | Area119_Spawn1AtLeaderB 3235; Area119_Spawn1AtMember1 3197; Area119_Spawn1AtMember2 3197; Area119_Spawn3AtMember2 3215 |
| H18 | 119 | Choice0: 0xB | Area119_ChoiceCounter0 427 |
| F63b | 118 | Switch: (z, x) compared, under area 118's (0x0B, 0x46) | Area118_SwitchHook 696 |
