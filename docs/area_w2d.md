# World 2, areas 95..100 and 103: the band `0x4135B0..0x4146C0`

**Status:** IN PROGRESS (2026-09-28) - 53 functions ours
(`src/game/area_w2d.cpp`, shadow name `area_w2d`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 318,000 rounds (in this worktree); 255 controls planted, 254 refused by a count, 1 equivalent with its variant refused (section 4). Fuzz only:
no recorded route reaches the band (section 8). No divergence; the three
state-table dispatchers abort past their tables and the two divides abort
on a zero divisor, where the original would jump into data or fault
(section 6).

Group AR2D of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
53 starts, none ours before, **53 taken**; no start dropped, none added
(section 7). Areas 101 and 102 have no code in the band (their descriptors,
`0x61AFF8` and `0x61B170`, have no `+0x34`, `+0x3C` or `+0x40`); area 97's
descriptor has none either, and its code hangs from a step hook, a tail kind
and the world-map field-hook table.

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, hooks, tails, triggers and
state tables have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the message box's answer byte
`0x7DEE67` in, the message word `0x7DEE48` read after), `kHandler` a `+0x3C`
handler (movement-script ops `03` / `DE`), `kInit` the `+0x40` init,
`kTail` a `Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a
step hook `(x, z)` answering in `al`, `kState` a state handler reached
through a table in the area's `.data`, `kCallee` a function called directly
(by the group's own code, by an engine table, or as an object trigger
`(object, 0x904030)` answering in `al`). The four functions that are both a
choice and a handler write the message word, so they are fuzzed as choices.

### Areas 95 and 96 (descriptors `0x617588`, `0x617D48`; PSX `0x801F3E58`, `0x801F3DCC`)

On the PSX two files (`AREA095.EMI`, `AREA096.EMI`, sections of different
hashes); here one body of code: both descriptors' `+0x3C` name the same
eight handlers except entry 4, which is two copies byte for byte (one in
each block; both end in a `jmp` to the shared handler 3). The shared bodies
are taken once, keyed by address, named after the block that holds them
(the tool puts `0x4135B0..0x413857` in area 95's block and
`0x413860..0x413939` in area 96's). Only handler 1 tests the area.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4135B0` | `Area95_TurnAtCell` | `0x3E` | 95 handler 4 (PSX `0x801F351C`) | kHandler | at z word `0x3A` and x word `0x40` / `0x42`: direction 3 / 7 and `Area95_TurnAndMove` (a tail `jmp`); else the script position + 2 |
| `0x4135F0` | `Area95_WaitEffectAtCell` | `0x75` | 95, 96 handler 0 | kHandler | an effect record (of all twenty) live at cell (`0x41`, `0x3A`): the script back 2; none: the leader's `+0x137` set, back 2 and `ScriptFlags_Set40`; clear, `Party_DropIn(0)` |
| `0x413670` | `Area95_SpawnEffect37` | `0xD2` | 95, 96 handler 1 | kHandler | an effect of kind `0x37` at a fixed point, sound `0x202`, then story flag `0x20` in area 96, else `0x21` below chapter 7 (signed), else `0x22`; no slot: the active member's word `+0x8A` - 2 |
| `0x413750` | `Area95_LeapArc` | `0x7C` | 95, 96 handler 2 | kHandler | the script object moved 5; the ground `+9` steps ahead; `+0x14` = (ground - height + `0xE0`) / `+9` (idiv); `MoveScript_PartyRecords` record 0's `+4` = `0x40`, `+1` = `+9` |
| `0x4137D0` | `Area95_TurnAndMove` | `0x87` | 95, 96 handler 3; the tail of both handler 4s | kHandler | the script object's `+4` 1 and bit `0x40`, `+7` bit 8, `Sprite_SetAnimation(8 + direction)`, script `+7` 2; `MoveCmd_Move` to 3 when facing 7, else to 7, and that direction |
| `0x413860` | `Area96_TurnAtCell` | `0x3E` | 96 handler 4 (PSX `0x801F34AC`) | kHandler | `Area95_TurnAtCell`, area 96's copy |
| `0x4138A0` | `Area96_ToggleCell` | `0x2E` | 95, 96 handler 5 | kHandler | map cell (`0x41`, `0x39`) 0 becomes `0x10`, anything else 0 |
| `0x4138D0` | `Area96_FaceBack` | `0x17` | 95, 96 handler 6 | kHandler | direction 7 becomes 3, anything else 7 |
| `0x4138F0` | `Area96_SnapElevation` | `0x4A` | 95, 96 handler 7 | kHandler | the ground at the leader: `MapView_Elevation` already there, `0x20` lower; else set to it; `MapView_Redraw` + 2 |

### Area 97 (descriptor `0x617E58`: no tables)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x413940` | `Area97_TailWaitCounter3` | `0x40` | tail kind 13 | kTail | state 0: `ScriptFlags_Set40`, 1; state 1: counter 3 at 2: `ScriptFlags_Clear40`, counter 3 and the tail cleared |
| `0x413980` | `Area97_StepHook` | `0x51` | `Area_StepHook`'s case for area `0x61` | kHook | z at most `0x3A0000`, x's high word `0x30..0x32`, the leader's pose 0..2, key item 5 not held: tail kind 13 with counter 3 = 1, al 1 |
| `0x4139E0` | `Area97_FlagIfKeyItem5` | `0x20` | `WorldMap_FieldHooks[11]` (a gap of the tool) | kTail | key item 5 held: story flag `0x23`; al 0 |

`0x4139E0` is `WorldMap_FieldHooks`' entry for "no world map": tail kind 27
(`0x56DE30`) jumps through the table by `WorldMap_RecordIndex`, and index 11
is none of the eleven world maps ([`worldmap_area.md`](worldmap_area.md)).
The same dword is `Field_ObjectTriggers`' id 0 (the trigger reader indexes
`0x662E1C + id * 4` unchecked, section 6); it answers al 0, so it is fuzzed
as a tail with its answer compared. It lies in area 97's block; nothing
ties it to area 97 but its place.

### Area 98 (descriptor `0x6183B0`; PSX `0x801F64CC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x413A00` | `Area98_ChoiceFlag9` | `0x41` | choice 0 = handler 1 (PSX `0x801F59F4`) | kChoice | message `0xFFFF`, flag 9 of the bit array `0x904654`; then the answer (read after the call) 0: counter 1 = 0; else the chapter row's flag `0x1A`, counter 1 = 1 |
| `0x413A50` | `Area98_ChoiceAsk2F` | `0x24` | choices 3, 4 = handlers 4, 5 (PSX `0x801F5AC8`, `0x801F5B0C`) | kChoice | an answer: message `0x2F` and the mark `0x9398CF` 6; 0: message `0xFFFF` |
| `0x413A80` | `Area98_ChoiceArmTail45` | `0x26` | choice 5 = handler 6 (PSX `0x801F5B50`) | kChoice | message `0xFFFF`; answer 0: tail kind 45 armed |
| `0x413AB0` | `Area98_SpawnEffect37` | `0xD2` | handler 0 (PSX `0x801F5BA0`) | kHandler | an effect of kind `0x37` at the running object, its slot kept in the object's `+0xB`, sound `0x206` |
| `0x413B90` | `Area98_InitCells` | `0x21` | init (PSX `0x801F5DD0`) | kInit | cells (4..7, 5) set to `0x50` |
| `0x413BC0` | `Area98_TailChangeArea62` | `0x60` | tail kind 45 | kTail | state 0 and `Field_Request` not 2: the return point `0x904148` = the leader's position and the area; `Field_ChangeArea(0x62, ...)`; the tail cleared |
| `0x413C20` | `Area98_Trigger44` | `0x5D` | object trigger 44 (a gap of the tool) | kCallee | `ScriptFlags_Set40`; the focus object's `+1` 4, `+0x83` 0, word `+0x8A` 0; tail kind 4 (engine) with sub-kind 9 and counter 3 the focus object's index; al 0 |
| `0x413C80` | `Area98_CameraDistanceFF00` | `0x11` | area 99 handler 0 (PSX `0x801F2C04`); area 39 handler 1, area 41 choice 10 = handler 0 | kHandler | `Camera_Distance` `0xFF00`, `MapView_Redraw` 2 |
| `0x413CA0` | `Area98_CameraDistance0` | `0x11` | area 99 handler 1 (PSX `0x801F2C24`); area 39 handler 2, area 41 choice 11 = handler 1 | kHandler | `Camera_Distance` 0, `MapView_Redraw` 2 |

Choices 1 and 2 (= handlers 2, 3) are `0x420850` and `0x420870`, another
block's. The two camera bodies lie in area 98's block but no table of area
98 names them: areas 39, 41 and 99 do (the tool's `shared` rows); their PSX
twins are area 99's handlers 0 and 1.

### Area 99 (descriptor `0x619BF8`; PSX `0x801F4944`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x413CC0` | `Area99_TurnSoundEffect1B` | `0x8A` | handler 2 (PSX `0x801F2C40`) | kHandler | an even direction turned one back; a sound by the object's word `+0x2C`; an animation by the direction; the leader's `+0x124` bit `0x40`; an effect of kind `0x1B`, its slot in the object's `+0xB` |
| `0x413D50` | `Area99_RunDrift` | `0x12` | handler 3 (PSX `0x801F2D68`) | kHandler | `Area99_DriftStates` by `Sprite_Current[4]` |
| `0x413D70` | `Area99_DriftStart` | `0x22` | `Area99_DriftStates[0]` | kState | `+0xA` = `0x48`, state 1; the active member's word `+0x8A` - 2 |
| `0x413DA0` | `Area99_DriftStep` | `0x45` | `Area99_DriftStates[1]` | kState | while `+0xA`: z moved by the negated step `Area99_DriftSteps[+0xA & 0xF] << 11`, `+0xA` - 1, the member's word - 2; at 0, state 0 |
| `0x413DF0` | `Area99_RunLeap` | `0x12` | handler 4 (PSX `0x801F2E58`) | kHandler | `Area99_LeapStates` by `Sprite_Current[4]` |
| `0x413E10` | `Area99_LeapStart` | `0xA1` | `Area99_LeapStates[0]` | kState | frames 16 / `Field_MoveSpeeds[3]`, a rise of `0x4200`, x and z steps `0x20000` and `-0x18000` over frames x 4; then `+0xA` `0xE`, state 1; the leader's word `+0x12E` - 2 |
| `0x413EC0` | `Area99_LeapStep` | `0x88` | `Area99_LeapStates[1]` | kState | a frame of the arc: x, z by the steps, the height by the rise >> 8, the rise less `0x700`; turning to fall, animation `0x51`; out of frames, state 0, else the leader's word - 2 |
| `0x413F50` | `Area99_ToggleRowFlag1F` | `0x11` | handler 5 (PSX `0x801F30A8`) | kHandler | the chapter row's flag `0x1F` toggled (`0x57C160`) |
| `0x413F70` | `Area99_InitCell` | `0x4D` | init (PSX `0x801F30D0`) | kInit | `Cond_ByteFD` 1: cell (`0x23`, `0x4F`) `0x51` when Cond row 3's flag `0x1C` is set and `0x1D` clear, else `0x50` |

Area 99's drift is area 39's (`Area39_DriftStep`, [`area_w1a.md`](area_w1a.md))
moving z alone and negated; its leap is chapter 6's leap phases' shape
([`scena_sc6.md`](scena_sc6.md)) with fixed steps.

### Area 100 (descriptor `0x61AAF0`; PSX `0x801F4F18`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x413FC0` | `Area100_ChangeAreaByFlags` | `0xA8` | handler 0 (PSX `0x801F34FC`) | kHandler | by story flags `0x94` and `0x93`: `Field_ChangeArea` to area `0x3B`, `0x24`, `0x74` or `0x70` |
| `0x414070` | `Area100_SetFlag45` | `0x10` | handler 2 (PSX `0x801F3608`) | kHandler | story flag `0x45` |
| `0x414080` | `Area100_ShiftCameraUp4` | `0x10` | handler 3 (PSX `0x801F3630`); area 49 choice 17 = handler 15, area 133 handler 2 | kHandler | `Camera_ShiftY` + 4, `MapView_Redraw` 2 |
| `0x414090` | `Area100_ShiftCameraDown4` | `0x10` | handler 4 (PSX `0x801F3658`); area 49 choice 16 = handler 14, area 133 handler 1 | kHandler | `Camera_ShiftY` - 4, `MapView_Redraw` 2 |
| `0x4140A0` | `Area100_SpawnEffect9B` | `0x49` | handler 6 (PSX `0x801F369C`) | kHandler | `Cond_ByteFE` 1, `MoveCmd_TestFB(0x4A, 0x2F)`, an effect of kind `0x9B` at a fixed point |
| `0x4140F0` | `Area100_SpawnEffectB7` | `0x49` | handler 7 (PSX `0x801F3724`) | kHandler | the same with kind `0xB7` |
| `0x414140` | `Area100_ChoiceFlags93` | `0xFC` | choice 0 = handler 8 (PSX `0x801F37AC`) | kChoice | the message word `Area100_ChoiceMessages[answer]`; by the answer, flags `0x93`, `0x94`, `0x33` set or cleared (answer 3 asks for key item 6; without it message 9); answer 4 nothing |
| `0x414240` | `Area100_TailEffect9B` | `0xF0` | tail kind 28 (a gap of the tool) | kTail | five states: `Kind2_Place(0)`; once counter 0 is 1, an effect of kind `0x9B` and a timer of `0x1E`; the timer out, counter 0 + 1; once counter 0 is 0, message 3; once the message is gone, the tail cleared |
| `0x414330` | `Area100_StepHook` | `0x5E` | `Area_StepHook`'s case for area `0x64` | kHook | `Cond_ByteFD` 3, story flag `0x33`, x's high word `0x45..0x47`, z's `0x33..0x35`, the leader's pose 0, 6 or 7: counter 0 = 0, `Party_DropIn(0)`, al 1 |
| `0x414390` | `Area100_Trigger16` | `0x5D` | object trigger 16 (a gap) | kCallee | story flag `0x45`, `MoveCmd_TestFB(0x4A, 0x1E)`, sound `0x109`, the four gate cells `0xA1`; al 0 |
| `0x4143F0` | `Area100_Trigger13` | `0x2E` | object trigger 13 (a gap) | kCallee | story flag `0x40`, `MoveCmd_TestFB(0x4A, 0x2F)`, tail kind 28 armed; al 0 |
| `0x414420` | `Area100_Trigger30` | `0x1D` | object trigger 30 (a gap) | kCallee | tail kind 44 (engine) with sub-kind 2; al 0 |
| `0x414440` | `Area100_InitCells` | `0x43` | init (PSX `0x801F3C2C`) | kInit | story flag `0x45` clear: the four gate cells (`0x49`, `0x1E..0x1F`), (`0x4D`, `0x33..0x34`) `0x50` |
| `0x414490` | `Area100_EffectB7Run` | `0x12` | `Effect_KindHandlers[0xB7]` (a gap) | kCallee | `Area100_EffectStates` by `Sprite_Current[1]` (the running effect record) |
| `0x4144B0` | `Area100_EffectB7Ring` | `0x2B` | `Area100_EffectStates[1]` | kState | the record's point (x, z, y) copied to the stack and handed to `0x4220D0` |

Handlers 1 and 5 are `0x421FB0` and `0x40F530`, other blocks'. The gate
cells are the four the init closes (`0x50`) and trigger 16 opens (`0xA1`)
with flag `0x45`, which the init tests. Effect kind `0xB7`, which handler 7
spawns, runs `Area100_EffectB7Run`: its state 0 is `0x40B4F0` (area 59's
block, group AR1D's this wave: the record's height from the ground, then
state 1), its state 1 `Area100_EffectB7Ring`.

### Area 103 (descriptor `0x61B4A0`; PSX `0x801F5D20`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4144E0` | `Area103_TalkByMember` | `0x51` | handler 0 (PSX `0x801F560C`) | kHandler | the script object's word `+8` = `Area103_FirstListedMember` + 2, `MoveScript_SetTurnTarget` on it, its bit `0x10` cleared, bit `0x20` by the running object's `+7` bit 4 |
| `0x414540` | `Area103_FirstListedMember` | `0x70` | called by handler 0 | kCallee | the first of `Area103_MemberKeys`' three that some member's `+0x89` holds, or `0xFF` |
| `0x4145B0` | `Area103_SkipIfLeader89` | `0x1A` | handler 1 (PSX `0x801F5738`) | kHandler | the leader's `+0x89` not 0: the script position + `0x17` |
| `0x4145D0` | `Area103_CountIfLeader89Zero` | `0x16` | handler 2 (PSX `0x801F5778`) | kHandler | the leader's `+0x89` 0: counter 0 + 1 |
| `0x4145F0` | `Area103_ShakeElevation` | `0x25` | handler 3 (PSX `0x801F57B4`) | kHandler | `MapView_Elevation` moved by `Area103_Shake[Frame_Counter & 3]` x 8, redraw |
| `0x414620` | `Area103_Ground` | `0x20` | handler 4 (PSX `0x801F57FC`) | kHandler | the running object's height = the ground at it |
| `0x414640` | `Area103_SpawnEffect4D` | `0x58` | handler 5 (PSX `0x801F5834`) | kHandler | an effect of kind `0x4D` at the running object; none: the script back 2 |
| `0x4146A0` | `Area103_Trigger35` | `0x1D` | object trigger 35 (a gap) | kCallee | tail kind 44 (engine) with sub-kind 8; al 0 |

The `+0x89` byte is `MoveScript_EffectState`'s index in AR1C's reading
(op `8C`); area 103 compares each member's with three constants. What the
three stand for is not read.

## 2. Ours

`src/game/area_w2d.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
two raw addresses of section 9). The group's own callees are called the
same way (`AH_CALL(Area95_TurnAndMove)`, `AH_CALL(Area103_FirstListedMember)`),
so the fuzz stands a recorder in for each and every function is tested
alone; the three state dispatchers read their `.data` tables in place and
call the entry (the original's `jmp [index * 4 + table]`), so the fuzz's
`DataTable` swap stands recorders there. Shapes that repeat are one helper:
the two handler-4 copies (`TurnAtCell`), area 100's effect at the fixed
point (`SpawnAt46`, three callers) and its test-and-spawn (`TestAndSpawn`),
its gate cells (`GateCells`), the tail arming and disarming (`ArmTail`,
`Disarm`). Kept as the originals: every re-read after a call
(`Sprite_Current`, `MoveScript_Object`, `Field_State`, the answer byte in
`Area98_ChoiceFlag9`, the effect slot read back from `+0xB` for every
store in `Area98_SpawnEffect37`), the order of every call, the 16-bit
compares of the hooks' high words and the signed compare of area 97's z,
the signed chapter compare, the sign extensions (ground, height, the drift
step, the shake step), the 16-bit timer, the 16-bit `+8` word of area 103
(none makes `0x101`), and the unchecked reads of section 6 that stay in
`.data`.

## 3. The fuzz

`BOF3X_SHADOW=area_w2d` (`src/game/area_w2d_fuzz.cpp`): seven `Run`s under
the one shadow name, one per area with its `Group::area` (95, 96, 97, 98,
99, 100, 103), 6,000 rounds per function, the real descriptors and tables in
place. The shared bodies of areas 95 and 96 run under 95 (the seed sets 96
for the one that tests the area), area 96's own copy under 96; the camera
bodies of area 98's block under 98, area 100's under 100.

- **Callees the group lists:** `AreaMap_SetByte` / `AreaMap_ByteAt` with
  masks of words x, z and a value byte (area 98's init pushes x as a dword
  whose high half is the previous call's answer; the callee reads a word);
  `Effect_FindFree` `kByte 0xFF..0x03` (a slot of the first four records,
  or none); `KeyItem_Has`, `MoveCmd_TestFB`, `MoveScript_SetTurnTarget`
  `kFlag`; `Kind2_Place`; `MoveCmd_Move` (Capcom's); `ScriptFlags_Set40` /
  `Clear40`; `0x57C160` and `0x4220D0` by raw address (the latter logging
  the 12 bytes of the point it is handed, not its stack address); the
  group's own `Area95_TurnAndMove` (`kPhase`) and
  `Area103_FirstListedMember` (`kByte 0xFF..0x02`).
- **Louder stand-ins** (an `effect`, half the time, from `Noise`):
  `AreaMap_Elevation` answers `MapView_Elevation`'s low word half the time
  (so `Area96_SnapElevation`'s "already there" branch is taken) and moves
  the running object; `Sound_PlayEffect` moves the running object, the
  chapter byte and the area number (read after it by
  `Area95_SpawnEffect37`); `Sprite_EnsureAnimation` and `Effect_FindFree`
  move the running object (and `Field_State`); `MoveCmd_Move`,
  `Sprite_SetAnimation` and `MoveScript_SetTurnTarget` move the script
  object and the running object. The harness's own disturbance moves
  `Sprite_Current` about one call in 24.
- **Data tables** swapped for recorders: `Area99_DriftStates`,
  `Area99_LeapStates` (area 99's run), `Area100_EffectStates` (area 100's).
- **Regions beyond the field frame:** all twenty `Effect_Objects` records,
  the active member, script object and chapter row pointers,
  `Camera_ShiftY` with the focus object pointer `0x903804` after it, the
  mark `0x9398CF`, `MoveScript_PartyRecords` record 0, `Field_MoveSpeeds[3]`,
  `Cond_ByteFE` (29 regions, 18,805 bytes).
- **Every round:** `Field_ActiveMember` at one of the four party objects, a
  field object, a party record or the running object; `MoveScript_Object`
  at a field object or a party record; the focus object at a field object
  or one of the four extra objects (an index past the thirty);
  `Field_MoveSpeeds[3]` 1..16.
- **The divisors** (section 6): a `settle` after every disturbance, and the
  louder stand-ins after they move the running object, keep
  `Area95_LeapArc`'s `+9` and `Field_MoveSpeeds[3]` off 0 (and the speed at
  16 or less), since the original faults there and ours aborts.
- **Seeds:** handler 4's cell (z word `0x3A` and beside, x words `0x40` /
  `0x42` and every neighbour, a high byte); the twenty effect records with
  at most one at area 95's cell, a quarter of the time one field off, the
  leader's `+0x137` 0 half the time; area 96 or not, the chapter at 6, 7, 8
  and the sign edge; `+9` at 1, 2, `0x10`, `0x80`, `0xFF` and the height's
  sign edges; the direction at 7 and its aliases (`0xF`, `0x87`); area 97's
  tail state (0, 1, 2, negative) with counter 3 at 2 and beside; its hook's
  x high word on or beside `0x30..0x32` with a high byte, z at and beside
  `0x3A0000`, negative; the pose at 0..3, 7 and with a high bit; the answer
  at every value a choice tests, its sign edge and above; area 98's tail
  state and `Field_Request`; area 99's state index (0 or 1: past its
  table the original jumps into data), its drift counter at 0, 1, `0x10`,
  `0x11`, its leap's rise on each side of 0 before and after the `0x700`
  step with the frames at 0..2; `Cond_ByteFD` at each value an init or hook
  tests and beside; area 100's tail at every state, its neighbours and the
  sign edge, with the cell that state waits on at its value two times in
  three (step-paired: counter 0, the timer at 1 and `0x101`,
  `Field_Request`); its hook's x and z high words on or beside their three
  with a high byte; the effect record as `Sprite_Current` with its state 0
  or 1; area 103's members' `+0x89` at each key and one past it, the
  member count 0 a tenth of the time, the leader's `+0x89` 0 half the time.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 0, counter 3, the timer (each at a value some state waits on, or
  any), the three pointers, the speed, the answer byte.

**Result (in this worktree):** 318,000 rounds over the 53 functions (6,000
each), 364,580 calls to the stand-ins, 0 mismatches, 18,805 bytes of state
(29 regions) and the log compared. Coverage: every callee each function can
reach was called - e.g. `Area95_TurnAndMove` 688 / 714 (the two handler-4
copies), `Party_DropIn` 2,605 (area 95) and 33 (area 100's hook, which needs
five conditions at once; its controls are refused in 9 rounds or more), `Field_ChangeArea` 1,694 (area 98's tail) and
6,000 (area 100), area 99's four states 2,881 / 3,119 / 2,973 / 3,027,
area 100's two effect states 2,995 / 3,005, `Msg_OpenScript` 525,
`Kind2_Place` 838, `KeyItem_Has` 404 (area 100's answer 3), `0x4220D0`
6,000.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4607 ours` (one below the 4,608 `impl` lines, the off-by-one the round doc section 10 notes from before wave two), 412 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w2d.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w2d.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w2d`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all seven runs). **255 planted, 254 refused by a count (exit 3), 1 equivalent** (D17: `Area99_DriftSteps` repeats every four bytes, so indexing by `+0xA & 7` reads the same step as `& 0xF`; its near variant D17b, `& 0xE`, refused), no hang, no fault. Four anchors of the first run matched a comment too (A50, E8, E9, F16) and were planted again on longer anchors. Every one of the 53 functions has at least one control of its own. A control in a helper shared across areas (`TurnAtCell`, `ArmTail`, `Disarm`, `StateEntry`) is refused in the first area's run, whose Fatal ends the self-test; A6b gives `Area96_TurnAtCell` one of its own. The thinnest: C3 (6 rounds; `Area98_ChoiceFlag9`'s answer read again after `Flags_Set`, moved only by the group's disturbance), D4 (7; a shift differs from the halving only for direction 0, which the turn never leaves - an even direction becomes odd - so it is reached only when the sound's stand-in or the disturbance moves the running object to one facing 0), and area 100's step hook (E45, E46, E50: 9..17; the hook needs five conditions at once); the rest need 27 rounds or more.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `TurnAtCell (95, 96)` | z word 0x3B | Area95_TurnAtCell 808 |
| A2 | `TurnAtCell (95, 96)` | x less 0x41 | Area95_TurnAtCell 1034 |
| A3 | `TurnAtCell (95, 96)` | direction 6 for 7 | Area95_TurnAtCell 323 |
| A4 | `TurnAtCell (95, 96)` | script + 3 | Area95_TurnAtCell 5312 |
| A5 | `TurnAtCell (95, 96)` | x 0x41 for 0x42 | Area95_TurnAtCell 503 |
| A6 | `TurnAtCell (95, 96)` | no turn-and-move | Area95_TurnAtCell 688 |
| A7 | `Area95_WaitEffectAtCell` | live bit 2 | Area95_WaitEffectAtCell 3843 |
| A8 | `Area95_WaitEffectAtCell` | z word 0x3B | Area95_WaitEffectAtCell 3433 |
| A9 | `Area95_WaitEffectAtCell` | 19 records searched | Area95_WaitEffectAtCell 5015 |
| A10 | `Area95_WaitEffectAtCell` | byte +0x138 | Area95_WaitEffectAtCell 2601 |
| A11 | `Area95_WaitEffectAtCell` | Party_DropIn(1) | Area95_WaitEffectAtCell 2605 |
| A12 | `Area95_WaitEffectAtCell` | found: script - 1 | Area95_WaitEffectAtCell 985 |
| A13 | `Area95_WaitEffectAtCell` | no Set40 | Area95_WaitEffectAtCell 2410 |
| A14 | `Area95_SpawnEffect37` | kind 0x38 | Area95_SpawnEffect37 4842 |
| A15 | `Area95_SpawnEffect37` | z 0x3C8001 | Area95_SpawnEffect37 4842 |
| A16 | `Area95_SpawnEffect37` | +7 0x13 | Area95_SpawnEffect37 4842 |
| A17 | `Area95_SpawnEffect37` | +0x29 7 | Area95_SpawnEffect37 4842 |
| A18 | `Area95_SpawnEffect37` | area 0x5F | Area95_SpawnEffect37 3398 |
| A19 | `Area95_SpawnEffect37` | chapter <= 7 | Area95_SpawnEffect37 481 |
| A20 | `Area95_SpawnEffect37` | chapter unsigned | Area95_SpawnEffect37 1284 |
| A21 | `Area95_SpawnEffect37` | sound 0x203 | Area95_SpawnEffect37 4842 |
| A22 | `Area95_SpawnEffect37` | word 0x59 | Area95_SpawnEffect37 4842 |
| A23 | `Area95_SpawnEffect37` | none: the leader's word | Area95_SpawnEffect37 1158 |
| A24 | `Area95_SpawnEffect37` | area read before the sound | Area95_SpawnEffect37 1067 |
| A25 | `Area95_SpawnEffect37` | +9 2 | Area95_SpawnEffect37 4842 |
| A26 | `Area95_LeapArc` | script +7 2 | Area95_LeapArc 6000 |
| A27 | `Area95_LeapArc` | move 4 | Area95_LeapArc 6000 |
| A28 | `Area95_LeapArc` | x step n + 1 | Area95_LeapArc 6000 |
| A29 | `Area95_LeapArc` | z by +0xC | Area95_LeapArc 6000 |
| A30 | `Area95_LeapArc` | + 0xE1 | Area95_LeapArc 600 |
| A31 | `Area95_LeapArc` | record word 0x41 | Area95_LeapArc 6000 |
| A32 | `Area95_LeapArc` | record +2 | Area95_LeapArc 6000 |
| A33 | `Area95_LeapArc` | object not read again | Area95_LeapArc 2626 |
| A34 | `Area95_LeapArc` | ground zero-extended | Area95_LeapArc 3055 |
| A35 | `Area95_TurnAndMove` | script +4 2 | Area95_TurnAndMove 5997 |
| A36 | `Area95_TurnAndMove` | bit 0x20 | Area95_TurnAndMove 4535 |
| A37 | `Area95_TurnAndMove` | +7 bit 4 | Area95_TurnAndMove 4103 |
| A38 | `Area95_TurnAndMove` | animation + 9 | Area95_TurnAndMove 6000 |
| A39 | `Area95_TurnAndMove` | facing 6 | Area95_TurnAndMove 3445 |
| A40 | `Area95_TurnAndMove` | script +7 3 | Area95_TurnAndMove 6000 |
| A41 | `Area95_TurnAndMove` | direction & 0xF | Area95_TurnAndMove 1535 |
| A42 | `Area95_TurnAndMove` | script object read before the animation | Area95_TurnAndMove 2720 |
| A43 | `Area96_ToggleCell` | read z 0x3A | Area96_ToggleCell 6000 |
| A44 | `Area96_ToggleCell` | set 0x11 | Area96_ToggleCell 2082 |
| A45 | `Area96_ToggleCell` | clear to 1 | Area96_ToggleCell 3918 |
| A46 | `Area96_FaceBack` | direction & 7 | Area96_FaceBack 1376 |
| A47 | `Area96_FaceBack` | 6 for 7 | Area96_FaceBack 4839 |
| A48 | `Area96_SnapElevation` | less 0x10 | Area96_SnapElevation 1585 |
| A49 | `Area96_SnapElevation` | ground zero-extended | Area96_SnapElevation 3024 |
| A51 | `Area96_SnapElevation` | (z, x) | Area96_SnapElevation 6000 |
| B1 | `Area97_TailWaitCounter3` | counter 3 | Area97_TailWaitCounter3 310 |
| B2 | `Area97_TailWaitCounter3` | state 2 | Area97_TailWaitCounter3 314 |
| B3 | `Area97_TailWaitCounter3` | on to 2 | Area97_TailWaitCounter3 983 |
| B4 | `Area97_TailWaitCounter3` | counter 3 kept | Area97_TailWaitCounter3 210 |
| B5 | `Area97_StepHook` | z >= | Area97_StepHook 97 |
| B6 | `Area97_StepHook` | z unsigned | Area97_StepHook 101 |
| B7 | `Area97_StepHook` | x less 0x31 | Area97_StepHook 212 |
| B8 | `Area97_StepHook` | x span 4 | Area97_StepHook 98 |
| B9 | `Area97_StepHook` | x as a byte | Area97_StepHook 181 |
| B10 | `Area97_StepHook` | pose 3 | Area97_StepHook 224 |
| B11 | `Area97_StepHook` | key item 6 | Area97_StepHook 326 |
| B12 | `Area97_StepHook` | counter 3 = 2 | Area97_StepHook 112 |
| B13 | `Area97_StepHook` | kind 0xE | Area97_StepHook 112 |
| B14 | `Area97_StepHook` | answers 2 | Area97_StepHook 112 |
| B15 | `Area97_StepHook` | state 1 | Area97_StepHook 112 |
| B16 | `Area97_FlagIfKeyItem5` | flag 0x24 | Area97_FlagIfKeyItem5 4009 |
| B17 | `Area97_FlagIfKeyItem5` | when not held | Area97_FlagIfKeyItem5 6000 |
| B18 | `Area97_FlagIfKeyItem5` | answers 1 | Area97_FlagIfKeyItem5 6000 |
| C1 | `Area98_ChoiceFlag9` | flag 10 | Area98_ChoiceFlag9 6000 |
| C2 | `Area98_ChoiceFlag9` | bank + 1 | Area98_ChoiceFlag9 6000 |
| C3 | `Area98_ChoiceFlag9` | answer read before the call | Area98_ChoiceFlag9 6 |
| C4 | `Area98_ChoiceFlag9` | row flag 0x1B | Area98_ChoiceFlag9 5612 |
| C5 | `Area98_ChoiceFlag9` | counter 1 = 2 | Area98_ChoiceFlag9 5612 |
| C6 | `Area98_ChoiceFlag9` | counter 0 cleared | Area98_ChoiceFlag9 388 |
| C7 | `Area98_ChoiceFlag9` | message 0xFFFE | Area98_ChoiceFlag9 5540 |
| C8 | `Area98_ChoiceAsk2F` | message 0x30 | Area98_ChoiceAsk2F 5589 |
| C9 | `Area98_ChoiceAsk2F` | mark 7 | Area98_ChoiceAsk2F 5589 |
| C10 | `Area98_ChoiceAsk2F` | else 0xFFFE | Area98_ChoiceAsk2F 411 |
| C11 | `Area98_ChoiceArmTail45` | kind 0x2E | Area98_ChoiceArmTail45 424 |
| C12 | `Area98_ChoiceArmTail45` | answer below 2 | Area98_ChoiceArmTail45 427 |
| C13 | `Area98_ChoiceArmTail45` | message 0xFFFE | Area98_ChoiceArmTail45 5981 |
| C14 | `ArmTail (98, 100 x3, 103)` | state 1 | Area98_ChoiceArmTail45 424 |
| C15 | `Area98_SpawnEffect37` | kind 0x38 | Area98_SpawnEffect37 4793 |
| C16 | `Area98_SpawnEffect37` | word 0x11C | Area98_SpawnEffect37 4793 |
| C17 | `Area98_SpawnEffect37` | +0x29 3 | Area98_SpawnEffect37 4793 |
| C18 | `Area98_SpawnEffect37` | y from +0x38 | Area98_SpawnEffect37 4793 |
| C19 | `Area98_SpawnEffect37` | sound 0x207 | Area98_SpawnEffect37 4793 |
| C20 | `Area98_SpawnEffect37` | +9 2 | Area98_SpawnEffect37 4793 |
| C21 | `Area98_SpawnEffect37` | none 0xFE | Area98_SpawnEffect37 1207 |
| C22 | `Area98_InitCells` | value 0x51 | Area98_InitCells 6000 |
| C23 | `Area98_InitCells` | three cells | Area98_InitCells 6000 |
| C24 | `Area98_InitCells` | row 6 | Area98_InitCells 6000 |
| C25 | `Area98_TailChangeArea62` | request 3 | Area98_TailChangeArea62 536 |
| C26 | `Area98_TailChangeArea62` | state 1 passes | Area98_TailChangeArea62 856 |
| C27 | `Area98_TailChangeArea62` | area 0x63 | Area98_TailChangeArea62 1694 |
| C28 | `Area98_TailChangeArea62` | flags 0 | Area98_TailChangeArea62 1694 |
| C29 | `Area98_TailChangeArea62` | area word + 0xA | Area98_TailChangeArea62 1694 |
| C30 | `Area98_TailChangeArea62` | x from z | Area98_TailChangeArea62 1694 |
| C31 | `Disarm (98, 100)` | kind 1 | Area98_TailChangeArea62 1694 |
| C32 | `Area98_Trigger44` | +1 5 | Area98_Trigger44 6000 |
| C33 | `Area98_Trigger44` | +0x84 | Area98_Trigger44 6000 |
| C34 | `Area98_Trigger44` | word 1 | Area98_Trigger44 6000 |
| C35 | `Area98_Trigger44` | sub-kind 8 | Area98_Trigger44 6000 |
| C36 | `Area98_Trigger44` | stride 0xA0 | Area98_Trigger44 1208 |
| C37 | `Area98_Trigger44` | kind 5 | Area98_Trigger44 6000 |
| C38 | `Area98_Trigger44` | answers 1 | Area98_Trigger44 6000 |
| C39 | `Area98_CameraDistanceFF00` | 0xFE00 | Area98_CameraDistanceFF00 6000 |
| C40 | `Area98_CameraDistanceFF00` | redraw 3 | Area98_CameraDistanceFF00 6000 |
| C41 | `Area98_CameraDistance0` | distance 1 | Area98_CameraDistance0 6000 |
| C42 | `Area98_CameraDistance0` | redraw 1 | Area98_CameraDistance0 6000 |
| D1 | `Area99_TurnSoundEffect1B` | bit 1 tested | Area99_TurnSoundEffect1B 2698 |
| D2 | `Area99_TurnSoundEffect1B` | turned forward | Area99_TurnSoundEffect1B 3177 |
| D3 | `Area99_TurnSoundEffect1B` | sound + 0x101 | Area99_TurnSoundEffect1B 6000 |
| D4 | `Area99_TurnSoundEffect1B` | halved by a shift | Area99_TurnSoundEffect1B 7 |
| D5 | `Area99_TurnSoundEffect1B` | animation 0x43 | Area99_TurnSoundEffect1B 6000 |
| D6 | `Area99_TurnSoundEffect1B` | leader bit 0x20 | Area99_TurnSoundEffect1B 4537 |
| D7 | `Area99_TurnSoundEffect1B` | kind 0x1C | Area99_TurnSoundEffect1B 4753 |
| D8 | `Area99_TurnSoundEffect1B` | object not read again after the sound | Area99_TurnSoundEffect1B 2559 |
| D9 | `Area99_TurnSoundEffect1B` | Field_State +0x125 | Area99_TurnSoundEffect1B 4526 |
| D10 | `Area99_RunDrift` | the leap table | Area99_RunDrift 6000 |
| D11 | `Area99_RunLeap` | the drift table | Area99_RunLeap 6000 |
| D12 | `StateEntry (99 x2, 100)` | index ^ 1 | Area99_RunDrift 6000, Area99_RunLeap 6000 |
| D13 | `Area99_DriftStart` | +0xA 0x47 | Area99_DriftStart 6000 |
| D14 | `Area99_DriftStart` | state 2 | Area99_DriftStart 6000 |
| D15 | `Area99_DriftStart` | the leader's word | Area99_DriftStart 6000 |
| D16 | `Area99_DriftStep` | state 1 at 0 | Area99_DriftStep 1040 |
| D17 | `Area99_DriftStep` | index & 7 | not refused: equivalent (the table repeats every four bytes, so & 7 and & 0xF read the same step); variant D17b refused |
| D18 | `Area99_DriftStep` | not negated | Area99_DriftStep 2467 |
| D19 | `Area99_DriftStep` | shift 10 | Area99_DriftStep 2467 |
| D20 | `Area99_DriftStep` | x moved | Area99_DriftStep 2467 |
| D21 | `Area99_DriftStep` | no member step | Area99_DriftStep 4960 |
| D22 | `Area99_LeapStart` | +0xB 4 | Area99_LeapStart 6000 |
| D23 | `Area99_LeapStart` | 17 / speed | Area99_LeapStart 629 |
| D24 | `Area99_LeapStart` | rise 0x4201 | Area99_LeapStart 6000 |
| D25 | `Area99_LeapStart` | x step 0x21000 | Area99_LeapStart 6000 |
| D26 | `Area99_LeapStart` | z step -0x17000 | Area99_LeapStart 6000 |
| D27 | `Area99_LeapStart` | frames 0xD | Area99_LeapStart 6000 |
| D28 | `Area99_LeapStart` | +9 1 | Area99_LeapStart 6000 |
| D29 | `Area99_LeapStart` | the member's word | Area99_LeapStart 6000 |
| D31 | `Area99_LeapStep` | x by +0x10 | Area99_LeapStep 6000 |
| D32 | `Area99_LeapStep` | rise >> 7 | Area99_LeapStep 4226 |
| D33 | `Area99_LeapStep` | gravity 0x680 | Area99_LeapStep 6000 |
| D34 | `Area99_LeapStep` | rise >= 0 | Area99_LeapStep 434 |
| D35 | `Area99_LeapStep` | falling <= 0 | Area99_LeapStep 472 |
| D36 | `Area99_LeapStep` | animation 0x52 | Area99_LeapStep 885 |
| D37 | `Area99_LeapStep` | ends below 2 | Area99_LeapStep 773 |
| D38 | `Area99_LeapStep` | frames - 2 | Area99_LeapStep 5994 |
| D39 | `Area99_ToggleRowFlag1F` | flag 0x1E | Area99_ToggleRowFlag1F 6000 |
| D40 | `Area99_InitCell` | Cond_ByteFD 2 | Area99_InitCell 2486 |
| D41 | `Area99_InitCell` | 0x1D set | Area99_InitCell 1147 |
| D42 | `Area99_InitCell` | value 0x52 | Area99_InitCell 359 |
| D43 | `Area99_InitCell` | row 4 | Area99_InitCell 1692 |
| D44 | `Area99_InitCell` | else z 0x50 | Area99_InitCell 1333 |
| E1 | `Area100_ChangeAreaByFlags` | area 0x25 | Area100_ChangeAreaByFlags 1341 |
| E2 | `Area100_ChangeAreaByFlags` | z 0x350000 | Area100_ChangeAreaByFlags 1323 |
| E3 | `Area100_ChangeAreaByFlags` | x 0xC8001 | Area100_ChangeAreaByFlags 2675 |
| E4 | `Area100_ChangeAreaByFlags` | flags 0x80 | Area100_ChangeAreaByFlags 661 |
| E5 | `Area100_ChangeAreaByFlags` | 0x93 as 1 | Area100_ChangeAreaByFlags 3998 |
| E6 | `Area100_ChangeAreaByFlags` | tests swapped | Area100_ChangeAreaByFlags 6000 |
| E7 | `Area100_SetFlag45` | flag 0x46 | Area100_SetFlag45 6000 |
| E10 | `Area100_ShiftCameraUp4` | redraw 3 | Area100_ShiftCameraUp4 6000 |
| E11 | `Area100_ShiftCameraDown4` | redraw 1 | Area100_ShiftCameraDown4 6000 |
| E12 | `TestAndSpawn (100 x2)` | Cond_ByteFE 2 | Area100_SpawnEffect9B 6000, Area100_SpawnEffectB7 6000 |
| E13 | `TestAndSpawn (100 x2)` | test z 0x30 | Area100_SpawnEffect9B 6000, Area100_SpawnEffectB7 6000 |
| E14 | `TestAndSpawn (100 x2)` | spawn before the test | Area100_SpawnEffect9B 6000, Area100_SpawnEffectB7 6000 |
| E15 | `SpawnAt46 (100 x3)` | kind + 1 | Area100_SpawnEffect9B 4798, Area100_SpawnEffectB7 4773, Area100_TailEffect9B 491 |
| E16 | `SpawnAt46 (100 x3)` | z 0x340001 | Area100_SpawnEffect9B 4798, Area100_SpawnEffectB7 4773, Area100_TailEffect9B 491 |
| E17 | `SpawnAt46 (100 x3)` | x 0x460001 | Area100_SpawnEffect9B 4798, Area100_SpawnEffectB7 4773, Area100_TailEffect9B 491 |
| E18 | `SpawnAt46 (100 x3)` | +0 2 | Area100_SpawnEffect9B 4798, Area100_SpawnEffectB7 4773, Area100_TailEffect9B 491 |
| E19 | `Area100_SpawnEffect9B` | kind 0x9C | Area100_SpawnEffect9B 4798 |
| E20 | `Area100_SpawnEffectB7` | kind 0xB8 | Area100_SpawnEffectB7 4773 |
| E21 | `Area100_ChoiceFlags93` | message by an unsigned answer | Area100_ChoiceFlags93 1993 |
| E22 | `Area100_ChoiceFlags93` | answer 1 sets 0x95 | Area100_ChoiceFlags93 419 |
| E23 | `Area100_ChoiceFlags93` | answer 2 clears 0x95 | Area100_ChoiceFlags93 403 |
| E24 | `Area100_ChoiceFlags93` | key item 7 | Area100_ChoiceFlags93 404 |
| E25 | `Area100_ChoiceFlags93` | message 10 | Area100_ChoiceFlags93 116 |
| E26 | `Area100_ChoiceFlags93` | answer 5 returns | Area100_ChoiceFlags93 795 |
| E27 | `Area100_ChoiceFlags93` | default sets 0x92 | Area100_ChoiceFlags93 4393 |
| E28 | `Area100_ChoiceFlags93` | messages + 2 | Area100_ChoiceFlags93 5164 |
| E29 | `Area100_ChoiceFlags93` | answer 3 clears 0x34 | Area100_ChoiceFlags93 125 |
| E30 | `Area100_TailEffect9B` | Kind2_Place(1) | Area100_TailEffect9B 838 |
| E31 | `Area100_TailEffect9B` | counter 0 2 | Area100_TailEffect9B 702 |
| E32 | `Area100_TailEffect9B` | timer 0x1F | Area100_TailEffect9B 603 |
| E33 | `Area100_TailEffect9B` | state 3 from 1 | Area100_TailEffect9B 603 |
| E34 | `Area100_TailEffect9B` | timer at 1 | Area100_TailEffect9B 77 |
| E35 | `Area100_TailEffect9B` | counter + 2 | Area100_TailEffect9B 590 |
| E36 | `Area100_TailEffect9B` | waits on counter 1 | Area100_TailEffect9B 618 |
| E37 | `Area100_TailEffect9B` | message 4 | Area100_TailEffect9B 525 |
| E38 | `Area100_TailEffect9B` | request 3 | Area100_TailEffect9B 525 |
| E39 | `Area100_TailEffect9B` | waits on request 3 | Area100_TailEffect9B 432 |
| E40 | `Area100_TailEffect9B` | state 2 from 0 | Area100_TailEffect9B 838 |
| E41 | `Area100_TailEffect9B` | timer a byte | Area100_TailEffect9B 209 |
| E42 | `Area100_TailEffect9B` | state 5 runs 4 | Area100_TailEffect9B 429 |
| E43 | `Area100_StepHook` | Cond_ByteFD 4 | Area100_StepHook 2400 |
| E44 | `Area100_StepHook` | flag 0x34 | Area100_StepHook 1600 |
| E45 | `Area100_StepHook` | x less 0x46 | Area100_StepHook 14 |
| E46 | `Area100_StepHook` | z span 2 | Area100_StepHook 9 |
| E47 | `Area100_StepHook` | pose 5 for 6 | Area100_StepHook 27 |
| E48 | `Area100_StepHook` | counter 0 = 1 | Area100_StepHook 32 |
| E49 | `Area100_StepHook` | answers 2 | Area100_StepHook 33 |
| E50 | `Area100_StepHook` | z as a byte | Area100_StepHook 17 |
| E51 | `Area100_Trigger16` | flag 0x44 | Area100_Trigger16 6000 |
| E52 | `Area100_Trigger16` | test x 0x4B | Area100_Trigger16 6000 |
| E53 | `Area100_Trigger16` | sound 0x10A | Area100_Trigger16 6000 |
| E54 | `Area100_Trigger16` | cells 0xA2 | Area100_Trigger16 6000 |
| E55 | `Area100_Trigger16` | answers 1 | Area100_Trigger16 6000 |
| E56 | `GateCells (100 x2)` | z 0x20 | Area100_Trigger16 6000, Area100_InitCells 2028 |
| E57 | `GateCells (100 x2)` | x 0x4E | Area100_Trigger16 6000, Area100_InitCells 2028 |
| E58 | `Area100_Trigger13` | flag 0x41 | Area100_Trigger13 6000 |
| E59 | `Area100_Trigger13` | kind 0x1D | Area100_Trigger13 6000 |
| E60 | `Area100_Trigger13` | answers 1 | Area100_Trigger13 6000 |
| E61 | `Area100_Trigger13` | test z 0x2E | Area100_Trigger13 6000 |
| E62 | `Area100_Trigger30` | kind 0x2D | Area100_Trigger30 6000 |
| E63 | `Area100_Trigger30` | sub-kind 3 | Area100_Trigger30 6000 |
| E64 | `Area100_Trigger30` | answers 1 | Area100_Trigger30 6000 |
| E65 | `Area100_InitCells` | when set | Area100_InitCells 6000 |
| E66 | `Area100_InitCells` | value 0x51 | Area100_InitCells 2028 |
| E67 | `Area100_EffectB7Run` | state ^ 1 | Area100_EffectB7Run 6000 |
| E68 | `Area100_EffectB7Ring` | z from +0x3C | Area100_EffectB7Ring 6000 |
| E69 | `Area100_EffectB7Ring` | y from +0x40 | Area100_EffectB7Ring 6000 |
| E70 | `Area100_EffectB7Ring` | x from +0x35 | Area100_EffectB7Ring 6000 |
| F1 | `Area103_TalkByMember` | row + 3 | Area103_TalkByMember 5997 |
| F2 | `Area103_TalkByMember` | bit 0x20 cleared first | Area103_TalkByMember 2931 |
| F3 | `Area103_TalkByMember` | +7 bit 8 | Area103_TalkByMember 2962 |
| F4 | `Area103_TalkByMember` | bit 0x40 set | Area103_TalkByMember 2915 |
| F5 | `Area103_TalkByMember` | script object not read again | Area103_TalkByMember 2056 |
| F6 | `Area103_TalkByMember` | word +8 a byte | Area103_TalkByMember 1506 |
| F7 | `Area103_FirstListedMember` | two keys | Area103_FirstListedMember 1419 |
| F8 | `Area103_FirstListedMember` | last member skipped | Area103_FirstListedMember 1762 |
| F9 | `Area103_FirstListedMember` | answers the member | Area103_FirstListedMember 2524 |
| F10 | `Area103_FirstListedMember` | none 0xFE | Area103_FirstListedMember 2461 |
| F11 | `Area103_FirstListedMember` | byte +0x88 | Area103_FirstListedMember 3562 |
| F12 | `Area103_SkipIfLeader89` | skip 0x18 | Area103_SkipIfLeader89 3032 |
| F13 | `Area103_SkipIfLeader89` | byte +0x88 | Area103_SkipIfLeader89 2962 |
| F14 | `Area103_CountIfLeader89Zero` | + 2 | Area103_CountIfLeader89Zero 3093 |
| F15 | `Area103_CountIfLeader89Zero` | at 1 | Area103_CountIfLeader89Zero 3111 |
| F17 | `Area103_ShakeElevation` | times 4 | Area103_ShakeElevation 3026 |
| F18 | `Area103_ShakeElevation` | redraw 3 | Area103_ShakeElevation 6000 |
| F19 | `Area103_ShakeElevation` | table unsigned | Area103_ShakeElevation 1482 |
| F20 | `Area103_Ground` | word +0x3C | Area103_Ground 6000 |
| F21 | `Area103_Ground` | (z, x) | Area103_Ground 6000 |
| F22 | `Area103_Ground` | object not read again | Area103_Ground 2700 |
| F23 | `Area103_SpawnEffect4D` | kind 0x4E | Area103_SpawnEffect4D 4827 |
| F24 | `Area103_SpawnEffect4D` | y from +0x38 | Area103_SpawnEffect4D 4827 |
| F25 | `Area103_SpawnEffect4D` | none: script - 1 | Area103_SpawnEffect4D 1173 |
| F26 | `Area103_SpawnEffect4D` | object read before the search | Area103_SpawnEffect4D 2160 |
| F27 | `Area103_Trigger35` | sub-kind 9 | Area103_Trigger35 6000 |
| F28 | `Area103_Trigger35` | answers 1 | Area103_Trigger35 6000 |
| F29 | `Area103_Trigger35` | kind 0x2B | Area103_Trigger35 6000 |
| A50 | `Area96_SnapElevation` | redraw + 1 | Area96_SnapElevation 6000 |
| D17b | `Area99_DriftStep` | index & 0xE (D17 variant) | Area99_DriftStep 2493 |
| E8 | `Area100_ShiftCameraUp4` | + 5 | Area100_ShiftCameraUp4 6000 |
| E9 | `Area100_ShiftCameraDown4` | - 3 | Area100_ShiftCameraDown4 6000 |
| F16 | `Area103_ShakeElevation` | frame & 1 | Area103_ShakeElevation 1482 |
| A6b | `Area96_TurnAtCell` | always the script step | Area96_TurnAtCell 714 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (15): `Area95_Handlers`
`0x617568` (8), `Area96_Handlers` `0x617D28` (8), `Area98_Handlers`
`0x618390` (7), `Area98_Choices` `0x618394` (6), `Area99_Handlers`
`0x619BDC` (6), `Area99_DriftStates` `0x619C3C` (2), `Area99_DriftSteps`
`0x619C44` (16 signed bytes), `Area99_LeapStates` `0x619C54` (2),
`Area100_Handlers` `0x61AAC8` (9), `Area100_Choices` `0x61AAE8` (1),
`Area100_ChoiceMessages` `0x61AB34` (5 words), `Area100_EffectStates`
`0x61AB40` (2), `Area103_Handlers` `0x61B488` (6), `Area103_MemberKeys`
`0x61B4E4` (3 bytes), `Area103_Shake` `0x61B4E8` (4 signed bytes). As in
world 1, a descriptor's `+0x34` array is the tail of its `+0x3C` array in
areas 98 and 100 ([`area-rows.md`](area-rows.md) section 2). The choice
and tail jump tables (`0x41422C`, 4 entries; `0x41431C`, 5) are in `.text`,
inside their functions' extents.

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area99_RunDrift` / `Area99_RunLeap` jump
  through a two-entry table by `Sprite_Current[4]`, and
  `Area100_EffectB7Run` by the effect record's `+1`; an index of 2 or more
  jumps through the bytes after the table (`Area99_DriftSteps`, the next
  table, area 100's data). Ours aborts with a message there (the owner's
  rule: no DIVERGENCE entry, round9 doc section 6). The band's own states
  store only 0 and 1; a larger index would need another writer of the
  byte, none read this round.
- **Divides by a byte that can be 0.** `Area95_LeapArc` divides by the
  running object's `+9` (a timed move's frames; 0 faults), and
  `Area99_LeapStart` divides 16 by `Field_MoveSpeeds[3]` and then by four
  times the quotient (a speed of 0, or above 16, faults). Ours aborts with a
  message at each. `Field_MoveSpeeds[3]` is non-zero in the image and no
  writer of it was found, so area 99's cannot fault in practice; area 95's
  depends on the object the script runs for.
- **Unchecked reads that stay in `.data`, kept:** `Area100_ChoiceFlags93`
  reads its message word by the signed answer (a negative answer reads
  before the table); `Area103_FirstListedMember` reads up to
  `Field_MemberCount` party records unbounded (1..3 in the field).
- **Effect slots unchecked.** Every spawn writes the record at
  `Effect_Objects + slot << 7` for any slot but `0xFF`;
  `Effect_FindFree` answers only 0..19 or `0xFF` (its `symbols.toml`
  evidence), so nothing reaches past.
- **`Area98_Trigger44`'s counter 3** is the focus object's index in
  `Sprite_Objects` as a byte: a focus object outside the thirty (one of the
  four extra objects, or the leader) gives an index past them. What tail
  kind 4 does with it is engine code, not read here.
- **`0x4139E0` doubles as `Field_ObjectTriggers` id 0**: the trigger reader
  indexes `0x662E1C` unchecked, and an object with trigger id 0 would run
  `Area97_FlagIfKeyItem5` (story flag `0x23` when key item 5 is held). Not
  measured in play.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D137 (divides by 0) in [`known-defects.md`](known-defects.md).

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 53 starts, extents, call sites,
  both in-function jump tables (`0x414140`: 4 entries at `+0xEC`;
  `0x414240`: 5 at `+0xDC`), and the shape its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding (`nop` runs, read byte by byte).
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x4139E0` (`WorldMap_FieldHooks[11]`), `0x413C20`,
  `0x414390`, `0x4143F0`, `0x414420`, `0x4146A0` (object triggers 44, 16,
  13, 30, 35), `0x414240` (tail kind 28, armed by an immediate in
  `Area100_Trigger13`), `0x414490` (effect kind `0xB7`'s handler). Each
  sits in the block of the area named.
- **The tool's register-armed tail kinds:** none in this band; every tail
  kind here (13, 28, 45, 4, 44) is armed by an immediate store.
- **`0x414540`** has no root of its own: only `Area103_TalkByMember` calls
  it.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 53). Every function is reached only in play: the choices
  when the area's message box asks, the handlers from its movement scripts,
  the step hooks on a step in the area, the tails once armed, the triggers
  by an object's trigger id, the inits on entry, area 100's effect states
  while an effect of kind `0xB7` lives, `0x4139E0` whenever tail kind 27
  runs outside a world map.

## 9. Calls across groups

- **Raw addresses nobody owns**, in `area_w2d_callees.h`: `0x57C160` (the
  story-flag toggle; group SX2's this wave) and `0x4220D0` (world 3's area
  code, group AR3F's; a later wave). Not a call but a table entry:
  `Area100_EffectStates[0]` is `0x40B4F0`, group AR1D's this wave.
- **By name, Capcom's:** `MoveCmd_Move`.
- **By name, ours:** `AreaMap_SetByte`, `AreaMap_ByteAt`,
  `AreaMap_Elevation`, `Effect_FindFree`, `Party_DropIn`,
  `Sound_PlayEffect`, `Sprite_SetAnimation`, `Sprite_EnsureAnimation`,
  `ScriptFlags_Set40` / `Clear40`, `Flags_Set` / `Clear` / `Test`,
  `KeyItem_Has`, `Kind2_Place`, `MoveCmd_TestFB`,
  `MoveScript_SetTurnTarget`, `Msg_OpenScript`, `Field_ChangeArea`. No
  harness edit; no `AH_THEIRS` moved.
- **Inbound** (for the rebinding pass): `Area_StepHook` (`0x56E050`, ours in
  `event_ops.cpp`) calls `0x413980` and `0x414330` through its raw
  `kStepHandlers` table (Capcom's copy at `0x56E102` / `0x56E112`), now
  `Area97_StepHook` and `Area100_StepHook`. Named by other areas' tables
  (read in place, no caller to rebind): `Area98_CameraDistanceFF00` /
  `Distance0` (areas 39, 41, 99), `Area100_ShiftCameraUp4` / `Down4`
  (areas 49, 133). By engine tables: `Field_ModeTailKinds` 13, 28, 45;
  `Field_ObjectTriggers` 13, 16, 30, 35, 44; `Effect_KindHandlers[0xB7]`;
  `WorldMap_FieldHooks[11]`.
- `analysis/calltrace/entries_logic.txt`: 53 lines appended under a
  `# group AR2D` comment; every one of the 53 was inside a host line
  (`00412140 1840`, `00413980 BB1`, `00414540 57A`) that runs over the band
  (the consolidation keeps the smaller).
