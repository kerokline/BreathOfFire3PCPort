# World 3, area 135: the band `0x41DAD0..0x41EFE0`

**Status:** IN PROGRESS (2026-09-28) - 46 functions ours
(`src/game/area_w3d.cpp`, shadow name `area_w3d`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` for area
135: 0 mismatches in 276,000 rounds (in this worktree); 334 controls planted, 326 refused by a count, 7 by a fault, 1 equivalent (its variant refused)
(section 4). Fuzz only: no recorded route reaches the band (section 5). No
divergence; where the original jumps through a pointer past what its
dispatcher reaches, divides by a byte that can be 0, or writes through an
index past the 30 field objects or the 20 effect records, ours aborts with a
message (section 6).

Group AR3D of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)): one
area, 46 starts, none ours before, **46 taken**; no start dropped, none added
(section 7). Area 135 is the largest area by functions of its own.

Every function was read to its last instruction with capstone (2026-09-28);
each extent is the tool's (`analysis/area_funcs.tsv`) and agrees with the
reading; the clone table is `area_rows.py --unit AREA135 --clones`'s rows, each
read against the disassembly. What the area *is* in the story is not read
here. The PSX twins are the sibling's `names/area_records.toml` (descriptor
`0x801F6A68`: the init and handlers 0..19; choices, hooks, the tail, the
state entries and the helpers have no pairing there, and
`analysis/pairs_propagated.json` pairs none of the band).

## 1. The functions

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase, `kHook` a hook `(x, z)` answering in `al`,
`kState` an entry of one of the area's `.data` state tables, `kCallee` a
function the area's own code calls directly.

**Descriptor `0x62D940`.** `+0x34` = `0x62D8DC` (24 choices), `+0x3C` =
`0x62D8EC` (20 handlers): the choice array starts four dwords before the
handler array, so **choices 4..23 are handlers 0..19**. None of those twenty
writes the message word, so each is fuzzed as a handler. `+0x38` is 0; `+0x40`
the init. The step hook is `Area_StepHook`'s case for area 135 (a `call` at
`0x56E162`), the cell hook the `(0x87, 0x41E580)` pair at `0x662F5C` of
`Area_CellHook`'s table, the tail `Field_ModeTailKinds[18]` (`0x662D30`).

"The marker" below is `Sprite_ObjectsExtra[3]` (`0x8021EC`); its words `+0x36`
/ `+0x3A` are its cell. "Script back 2" is `MoveScript_Object`'s u16 `+0xA`
less 2 (the op runs again next frame).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41DAD0` | `Area135_ChoiceStartTail` | `0x26` | choice 0 | kChoice | message `0xFFFF`; answer 0: `ScriptFlags_Set40`, tail kind `0x12` at state 0 |
| `0x41DB00` | `Area135_ChoiceCount` | `0x3C` | choice 1 | kChoice | message `0x84`; the tail's sub byte `0x9039F5` = answer + 1, printed into `Text_Records` by area 8's format (`Crt_sprintf`); the marker's `+0x83` = the answer (read again) + 4 |
| `0x41DB40` | `Area135_ChoiceStartTail14` | `0x63` | choice 2 | kChoice | s8 answer 0 / 1: `ScriptFlags_Set40`, tail kind `0x12` at state `0x14`, `Effect_FindFree` to the byte `0x675CC0` (the none `0xFF` stored too); a slot: `+0` = 1, kind `+5` = `0x66`, `+6` = the answer. Message `0xFFFF` either way |
| `0x41DBB0` | `Area135_ChoiceFlag37` | `0x2B` | choice 3 | kChoice | message `0xFFFF`; answer 0 sets story flag `0x37`, else clears it |
| `0x41DBE0` | `Area135_ClearFlagC` | `0x17` | choice 4 = handler 0 (PSX `0x801F40F8`) | kHandler | story flag `0xC` cleared, the sub byte 0 |
| `0x41DC00` | `Area135_RiseInZone4` | `0x22` | handler 1 (PSX `0x801F4128`) | kHandler | `Cond_ByteFD` 4: the object's word `+0x3E` += word `+0x14`, script back 2 |
| `0x41DC30` | `Area135_RiseElsewhere` | `0x22` | handler 2 (PSX `0x801F4178`) | kHandler | the same outside zone 4 |
| `0x41DC60` | `Area135_LeaveByExit` | `0x7E` | handler 3 (PSX `0x801F41C8`) | kHandler | `Field_ChangeArea(0x87, x << 16, z << 16, 0x81)` from the byte pair `[script object +3 - 6]` of `Area135_ExitsZone4` (zone 4) or `Area135_Exits`; `Sprite_Kind2 +0x80` \|= 8 |
| `0x41DCE0` | `Area135_MarkerRoute` | `0x11A` | handler 4 (PSX `0x801F4270`) | kHandler | section 1.2 |
| `0x41DE00` | `Area135_ToMarker` | `0x53` | handler 5 (PSX `0x801F4464`) | kHandler | the object `Sprite_Kind2`: `Field_Kind2X` / `Z` = the marker's x / z, F3 divisor `0x20`; else its x, z, y dwords = the marker's |
| `0x41DE60` | `Area135_FaceLeaderDir` | `0x40` | handler 6 (PSX `0x801F44E0`) | kHandler | `Field_State` not party record 0 (its index `(p - ObjTrio) / 0x14C` signed, as a byte): `+8` = the leader's direction, `Sprite_SetAnimation(+8)` |
| `0x41DEA0` | `Area135_HeldMove1` | `0x53` | handler 7 (PSX `0x801F456C`) | kHandler | word `+0x3A` at 7 or more (signed) and `Input_Held` bit `0x20`: `+8` = 1, the script object's `+7` = 2, `MoveCmd_Move`, `+0x14` = 0, script back 2 |
| `0x41DF00` | `Area135_HeldMove7` | `0x53` | handler 8 (PSX `0x801F460C`) | kHandler | the same on word `+0x36` at 6, direction 7 |
| `0x41DF60` | `Area135_FallToFloor` | `0xD7` | handler 9 (PSX `0x801F46AC`) | kHandler | section 1.3 |
| `0x41E040` | `Area135_Fall780` | `0xC` | handler 10 (PSX `0x801F4828`) | kHandler | `Area135_FallTo(0x780)` |
| `0x41E050` | `Area135_StampCellsRun` | `0x5` | handler 11 (PSX `0x801F4848`) | kHandler | `jmp Area135_StampCells` |
| `0x41E060` | `Area135_RestoreCellsRun` | `0x5` | handler 12 (PSX `0x801F4868`) | kHandler | `jmp Area135_RestoreCells` |
| `0x41E070` | `Area135_RestoreOnRequest5` | `0x1A` | handler 13 (PSX `0x801F4888`) | kHandler | `Field_Request` 5: `jmp Area135_RestoreCells`; else script back 2 |
| `0x41E090` | `Area135_JumpByOccupied` | `0x79` | handler 14 (PSX `0x801F48DC`) | kHandler | over the four `Area135_JumpCells` (cell `<< 16 \| 0x8000`) the object stands on exactly, `Area135_ObjectAtCell` there: an object stops the count (the object read again after a none); the script moves on 3 x the count (12 when none stopped it) |
| `0x41E110` | `Area135_QueueRun` | `0x12` | handler 15 (PSX `0x801F49A8`) | kHandler | `jmp [Area135_QueueStates + +4 * 4]` |
| `0x41E130` | `Area135_QueueStepZ` | `0xA3` | `Area135_QueueStates[0]` | kState | n = `Area135_CountQueue`; `+8` = 5; the script object's `+7` = `((n << 17) - z + 0xC8000) >> 15` (arithmetic, a byte); negative: negated and `+8 ^= 4`; not 0: `MoveCmd_Move`, `+0x14` = 0; state + 1, script back 2 |
| `0x41E1E0` | `Area135_QueueStepX` | `0x7C` | `Area135_QueueStates[1]` | kState | n = `Area135_CountQueue`; at 4 nothing; else `+8` = 3, `+7` = `((Area135_QueueCells[n] << 16 \| 0x8000) - x) >> 15`; not 0: `MoveCmd_Move`, `+0x14` = 0; state 0 |
| `0x41E260` | `Area135_WalkRun` | `0x12` | handler 16 (PSX `0x801F4BD4`) | kHandler | `jmp [Area135_WalkStates + +4 * 4]` (section 1.4: 13 reached) |
| `0x41E280` | `Area135_WalkToX` | `0x88` | `Area135_WalkStates[0]` | kState | `+8` = 7; `+7` = `(x - 0x1C8000) >> 15`; negative: `+8 ^= 4`, negated; `MoveCmd_Move`, `+0x14` = 0, state + 1, script back 2 |
| `0x41E310` | `Area135_WalkToZ` | `0x51` | `Area135_WalkStates[1]` | kState | `+8` = 5; `+7` = `(0x148000 - z) >> 15`; `MoveCmd_Move`, `+0x14` = 0, state 0 |
| `0x41E370` | `Area135_LiftRun` | `0x12` | handler 17 (PSX `0x801F4D88`) | kHandler | `jmp [Area135_LiftStates + +4 * 4]` (11 reached) |
| `0x41E390` | `Area135_LiftUp` | `0x2D` | `Area135_LiftStates[0]`, `[3]`, `[4]`, `[7]` | kState | `MapView_Elevation` += 4, `MapView_Redraw` = 2, state + 1, script back 2 |
| `0x41E3C0` | `Area135_LiftDown` | `0x2D` | `Area135_LiftStates[1]`, `[2]`, `[5]`, `[6]` | kState | the same, -= 4 |
| `0x41E3F0` | `Area135_LiftDone` | `0x2A` | `Area135_LiftStates[8]` | kState | at x `0x168000` and z `0x128000` the script on by 1; state 0 |
| `0x41E420` | `Area135_SpawnRun` | `0x12` | handler 18 (PSX `0x801F4ED4`) | kHandler | `jmp [Area135_SpawnStates + +4 * 4]` |
| `0x41E440` | `Area135_SpawnCopy` | `0x93` | `Area135_SpawnStates[0]` | kState | script back 2; `Sprite_FindFree` to the word `0x903850`; a slot: `Area135_RestoreCells`, `EventOp_9x(Area135_SpawnOps + 13 x sub)`, the new `Sprite_Current` given the old one's x, z, y, the old one current again, `0x46D710`, `+0xA` = `+9`, `+9` = 0, state + 1 |
| `0x41E4E0` | `Area135_SpawnCountdown` | `0x47` | `Area135_SpawnStates[1]` | kState | script back 2; `+0xA` less 1; it was 0: `+0` = 0, state 0; else `+9` = `+0xA`, `0x46D770`, `+9` = 0 |
| `0x41E530` | `Area135_SpawnKind1AtLeader` | `0x42` | handler 19 (PSX `0x801F50B0`) | kHandler | `Sprite_Current` = the leader; `Effect_Spawn(1, 0, Area135_EffectKinds[party list 1], the leader's words +0x2E / +0x30)`; a slot to `+0xB` |
| `0x41E580` | `Area135_CellHook` | `0x5C` | the cell hook (`0x662F5C`) | kHook | the first of `Area135_CellEntries`' two whose x and z bytes are the cell's and whose direction nibble equals the leader's direction byte (the whole byte): `ScriptFlags_Set40`, tail state the entry's, kind `0x12`, al 1; none: al 0 |
| `0x41E5E0` | `Area135_StepHook` | `0x4A` | `Area_StepHook` (`0x56E162`) | kHook | zone 4, 6 or 7, story flag `0xC` and `Area135_NearMarker(x, z)`: `Party_DropIn(0)`, al 1; else al 0 |
| `0x41E630` | `Area135_Tail18` | `0x504` | tail kind 18 | kTail | section 1.1 |
| `0x41EB40` | `Area135_Init` | `0x99` | `+0x40` init (PSX `0x801F5A44`) | kInit | zone 4: entered from zone 1 (`0x905E68`): `EventOp_Bx(Area135_InitOp)`, flag `0xC` cleared, the zone read again, the sub byte 0; `0x8034B0 \|= 1`. The zone (as last read) 4, 6 or 7: the marker's `+0` = 1. Zone 0: `Effect_FindFree`, a slot `+0` = 1, kind `+5` = `0x53`, `+0x18` = 0, `+0x1C` = 6; counter 3 = 0 |
| `0x41EBE0` | `Area135_FallTo` | `0x54` | called by handlers 9, 10 | kCallee | `(height)`: d = s16(height - word `+0x3E`); `+9` = `(\|d / 128\| << 4)` as a byte (C's truncating division); `+0x14` = d / `+9` (`idiv`); `+0x10`, `+0xC` = 0 |
| `0x41EC40` | `Area135_ObjectAtCell` | `0x68` | called by handlers 9, 14, `Area135_CountQueue` | kCallee | `(x, z)`: the first field object 0..29 in use (`+0` bit 0), not `Sprite_Current` (read at entry), kind `+6` = `0xA`, at exactly (x, z): al its index; else `0xFF` |
| `0x41ECB0` | `Area135_CountQueue` | `0x3B` | called by the queue states | kCallee | `Area135_ObjectAtCell(0x208000, 0xC8000 + n << 17)` for n 0..3 until one is empty: al n (0..4) |
| `0x41ECF0` | `Area135_StampCells` | `0xEF` | handler 11's `jmp` | kCallee | `Field_Request` 5: script back 2; else the 2 x 2 cells under (x - `0x8000`, z - `0x8000`): `+0x20` = 0, then for each `AreaMap_SetHeight(cell, 0x10)`, its `AreaMap_ByteAt` or'd into `+0x20` (bytes 0 / 2 for row z, 1 / 3 for z + 1; the object read again before each), `AreaMap_SetByte(cell, 0x11)` |
| `0x41EDE0` | `Area135_RestoreCells` | `0x86` | handlers 12, 13, `Area135_SpawnCopy` | kCallee | the 2 x 2 cells at the object's own (x, z) - no half-cell offset: `AreaMap_SetHeight(cell, 0)`, `AreaMap_SetByte(cell, +0x20 >> 0 / 16 / 8 / 24)` (arithmetic, the object read again) |
| `0x41EE70` | `Area135_NearMarker` | `0x71` | called by the step hook | kCallee | `(x, z)`: r = (the object's `+0x70` byte + the marker's + 2) `<< 15`; \|x - `+9` x step `+0xC` - x\| < r and the same for z (the marker's; signed, \|`INT_MIN`\| stays negative): al 1, else 0 |
| `0x41EEF0` | `Area135_PartyInBox` | `0x61` | called by the tail's state 5 | kCallee | a party record below `Field_MemberCount` with cell x word `+0x36` `0x2C` / `0x2D` and z word `+0x3A` `0x30..0x37` (signed): al 1, else 0 |
| `0x41EF60` | `Area135_Kind2ToObject0` | `0x3E` | called by the tail | kCallee | `(divisor)`: `Field_Kind2X` / `Z` = `Sprite_ObjectsExtra[0]`'s x / z; a divisor word not 0 also `MoveScript_F3Divisor` |
| `0x41EFA0` | `Area135_OpenMessage` | `0x31` | called by the tail | kCallee | `(id)`: `Msg_OpenScript(id byte)`; `0x905B84` = `MessagePools`; `Window_FreeCurrent`; `MessagePools +4 / +6` = `0x3C0` / `0xAA0` |

### 1.1 The tail, kind 18

`Area135_Tail18` runs by the s8 tail state `0x9039F4` through the byte table
`0x41EB14` (32 entries, states 0..`0x1F`) and the jump table `0x41EAC8` (19
entries) in its own extent; a state below 0 or above `0x1F`, and states 2..4,
7..9, `0xC..0x13`, return at once. Three entries arm it: choice 0 (state 0),
choice 2 (state `0x14`) and the cell hook (the entry's state: 5 or `0xA` in the
shipped table). The disarm (`0x41E6AF`) is `ScriptFlags_Clear40`, kind and
state 0.

- **0, 1** (choice 0): unless `Field_Request` is 2, `KeyItem_Add(6)`, message
  `0x81`, `Field_Request` = 2, `Sound_StopMusic`, `Sound_LoadStream(2)`; then,
  once `Sound_StreamDone` (all of eax) and the message closed,
  `Sound_ResumeAll` and the disarm.
- **5, 6** (a cell hook entry): `Area135_PartyInBox`: message `0x87`, state
  `0xB`; else story flag `0x2A` toggled (`Flags_Toggle`), sound `0x203`, the
  word timer `0x9039F6` = `0xA`, then counted down to the disarm.
- **`0xA`, `0xB`** (the other entry): story flag `0xC` set: the sub byte
  printed into `Text_Records` by area 8's format and message `0x84`; clear:
  message `0x83` and the flag set. Then the message closed: the disarm.
- **`0x14`..`0x1F`** (choice 2): the camera (the kind-2 object) glides to
  `Sprite_ObjectsExtra[0]` at `Field_MoveSpeeds[3] << 3`, then five steps,
  each through `Area135_OpenMessage` and the effect in `0x675CC0` (its `+1`
  2, 3, 2, 3, 4, 5): `0x15` waits for `Field_Kind2Hold`, message `0x8B`;
  `0x16` the button held (`Input_Held` bit `0x20`), `0x8C`, sound `0x209`,
  counter 3 = `0xA`; `0x17` counter 3 at `0xB` (the movement script moves
  it), `0x8D`; `0x18` a press (`Input_Pressed`); `0x19` held, `0x8E`, sound
  `0x209`, counter 3 = `0xC`; `0x1A` counter 3 at `0xD`; `0x1B` at `0x10`: the
  field object in the sub byte (`Area135_FallToFloor`'s find) given `+1` = 7,
  `+0x18` = 0, `+0x1C` = 5, `+0x8A` += 3 - none (`0xFF`): message `0x8F`,
  state `0x1E`; `0x1C` at `0x11`: bit 4 of `0x62D883` set; `0x1D` at `0x12`:
  the object `+1` = 4, `+0x24` bit 5 cleared, `+0x8A` += 1, the bit cleared,
  message `0x8F`; `0x1E` at `0x15`: the camera to the leader's x / z at the
  same speed, the effect's `+1` = 5; `0x1F` the glide done: disarmed, counter
  3 and the sub byte 0. States `0x17`, `0x1A`, `0x1B`, `0x1D`, `0x1E` call
  `Area135_Kind2ToObject0(0)` first (the camera kept on object 0 without
  changing its speed).

`0x62D883` is byte 3 of the record the descriptor's `+8` points at
(`0x62D880`); what the bit does is not read.

### 1.2 The marker's routes (handler 4)

`Area135_Routes` (`0x62D994`) holds sixteen 7-byte records: the marker's
cell (x, z), then a target (x half flag, x cell, z half flag, z cell) and a
direction. Every record whose cell is the marker's is applied - the loop does
not stop at a match. A target cell `0xFF` means "none": the script back 4,
and for the leader counter 3 = 8. Otherwise the object running the script
(`+0x24` bit 5 cleared through the pointer last read, `+2` = 0) is sent with
`MoveCmd_OpF7(x, z, 0x10, MoveScript_Object)`, the script object's `+7` = 1,
its direction `+8` the record's; for the leader the kind-2 object follows
(`Field_Kind2X` / `Z`, F3 divisor `0x20`).

### 1.3 The fall (handler 9)

The highest ground (`MapView_GroundAt`, compared as s16, from `0xFFFF8000`)
over a 4 x 2 grid from (x - `0x18000`, z - `0x8000`) in steps of `0x10000`,
`MapView_HeightScale` 1 before each read (x read again for each row);
`Area135_ObjectAtCell` at the object's (x, z) goes to the sub byte; with an
object there the fall is to `AreaMap_Elevation + 0x280`, else to the highest +
`0x180`, through `Area135_FallTo`, which sets the fall's frame count `+9` and
step `+0x14`.

### 1.4 The state tables overlap

The four dispatchers' tables are contiguous in `.data`: `Area135_QueueStates`
`0x62DA0C` (2), `Area135_QueueCells` (4 bytes, data), `Area135_WalkStates`
`0x62DA18` (2), `Area135_LiftStates` `0x62DA20` (9), `Area135_SpawnStates`
`0x62DA44` (2), then a zero. Each dispatcher indexes its table unchecked, so
the walk's state 2..12 run the lift's and the spawn's handlers and the lift's
9..10 the spawn's - 13 and 11 code pointers, which is what the tool's rows
count ("13 code entries", "11 code entries"). Ours reads the same: each
dispatcher is given the count of code pointers its original reaches
(`kArea135*Reach`) and aborts past it (the next dword is a zero or data).
Only states 0..1 of the walk and 0..8 of the lift are set by their own
handlers; the rest are reached only with a state byte the area's code does
not write.

## 2. Ours

`src/game/area_w3d.cpp`, calling out only through the harness (`AH_CALL(name)`
for every named callee, `AH_AT` for the two engine callees nobody owns,
`0x46D710` and `0x46D770`). The group's own callees are called the same way,
so the fuzz stands a recorder in for each. Shapes that repeat are one helper:
the two rises (`Rise`), the two held moves (`HeldMove`), the lift's steps
(`Lift`), the four dispatchers (`RunState`), the tail's disarm and speed
(`Disarm`, `Speed8`). Kept as the originals: every re-read of
`Sprite_Current` and `MoveScript_Object` after a call (handler 4's "pointer
last read", the queue states after `MoveCmd_Move`, the spawn after
`EventOp_9x` and `0x46D710`, the cell stamp's pointer taken before
`AreaMap_ByteAt` and the dword read after it), the tail's reads after
`Area135_OpenMessage` (the slot, then the speed), the zone read again after
the init's `EventOp_Bx` / `Flags_Clear`, the byte and word widths of every
store (the word `0x903850`, the word timer), the s16 compares, the arithmetic
shifts, C's truncating division in `Area135_FallTo`, the cell arguments'
low words (the originals push registers whose upper halves are stack or
pointer bits; every callee reads the low word), `AreaMap_SetByte`'s value as
the arithmetic shift of `+0x20` (the callee reads the byte).

## 3. The fuzz

`BOF3X_SHADOW=area_w3d` (`src/game/area_w3d_fuzz.cpp`): one `Run` with
`Group::area` 135, 6,000 rounds per function, the real descriptor and tables
in place, the four state tables as `DataTable`s - listed without overlap
(`0x62DA0C` 2, `0x62DA18` 2, `0x62DA20` 9, `0x62DA44` 2), so each entry is
swapped once and a dispatcher reading on past its own table reads the next
table's recorders, as the original reads the next table's handlers.

- **Callees the group lists:** its own nine (`Area135_FallTo` word,
  `_ObjectAtCell` `kByte 0xFF..0x1D`, `_CountQueue` `kByte 0..5`,
  `_StampCells` `kPhase`, `_RestoreCells`, `_NearMarker` / `_PartyInBox`
  `kFlag`, `_Kind2ToObject0` word, `_OpenMessage` byte); the raw `0x46D710`,
  `0x46D770`; `ScriptFlags_Set40` / `Clear40`, `Effect_FindFree` (a slot of
  the 20 or none, a third of the time none), `Sprite_FindFree` (0..29 or
  none), `EventOp_9x`, `EventOp_Bx`, `MoveCmd_OpF7` (the speed a byte),
  `MoveCmd_Move` and `Crt_sprintf` and `Sound_StopMusic` (Capcom's),
  `KeyItem_Add`, `Sound_LoadStream`, `Sound_StreamDone` (`kFlag`: all of eax
  tested), `Flags_Toggle`, `Window_FreeCurrent`, `AreaMap_SetHeight` (x, z
  words, the value a byte); standard ones listed again: `AreaMap_ByteAt`,
  `AreaMap_SetByte` (the value a byte), `MapView_GroundAt`, `Flags_Clear`,
  `Effect_Spawn` (`kByte 0xFE..0x02`).
- **Louder stand-ins** (half the time, from `Noise`): every callee its caller
  reads `Sprite_Current` or `MoveScript_Object` again after moves one or both
  (`Area135_ObjectAtCell`, `_CountQueue`, `_RestoreCells`, the two engine
  callees, `EventOp_9x`, both `MoveCmd_*`, the three map writers,
  `MapView_GroundAt`, `Effect_Spawn`); `AreaMap_ByteAt` also scribbles the
  object's `+0x20` (the stamp's `or` reads it after the call); `Crt_sprintf`
  moves the choice answer (choice 1 reads it again); `EventOp_Bx` and
  `Flags_Clear` move `Cond_ByteFD` (the init reads it again);
  `Sound_StreamDone` moves `Field_Request`; `Area135_Kind2ToObject0` moves
  counter 3 (to a value a tail state tests, half the time) and
  `Field_Kind2Hold`; `Area135_OpenMessage` moves the effect slot (inside the
  20) and the speed byte.
- **Regions beyond the field frame:** the 20 effect records, the script
  object pointer, the effect slot, `Sprite_Kind2`'s record, the input words,
  `MapView_HeightScale`, `Field_Kind2Hold`, the speed byte, the map-flags
  byte `0x62D883`, the init's byte `0x8034B0`, the window pool words, and
  `Area135_Routes` (32 regions with the harness's, 19,064 bytes).
- **Every round:** the script object at a field object, a party record or the
  running object itself; the effect slot 0..19; the sub byte 0..29;
  `Cond_ByteFD` among 0, 4, 6, 7, 1, 5, 3, `0x84` two rounds in three; the
  shipped `Area135_Routes` put back two rounds in three (the harness's random
  bytes otherwise). The shipped table has `0xFF` in both target cells or in
  neither, and no no-target route shares its cell with a later route, so with
  it alone two of handler 4's paths cannot be told apart (controls C2, C12).
- **Seeds:** each choice's answers (0, 1, 2, `0xFF`, `0x80`, `0x7F`, ...); the
  zone for the rises, the step hook and the init (with the entry zone 1 or
  not); the exit index 6..9; the marker's cell at a route's (read from the
  table in place) two rounds in three, one off otherwise, the x word with a
  high byte one time in four, and half the time that route given one target
  cell `0xFF` and a later route given its cell; the running object
  as `Sprite_Kind2` half the time for handler 5; the held moves' words at 5,
  6, 7, 8, `0x8000`, `0x7FFF` with the button; `Field_Request` 5 and beside;
  the running object on a jump cell (exactly, or half a cell off); each
  dispatcher's state inside what it reaches; the queue's z at a row's centre
  and 0, `0x4000`, `0x7FFF`, `0x8000` either side, the walk's and the queue
  row's x near their targets; the lift's end cell and one off; the spawn's
  sub byte 0..3 or any, its count 0, 1, 2, `0x80`, `0xFF`; party list 1 0..8;
  the cell hook's (x, z) at an entry's bytes (upper bytes garbage half the
  time) with the leader's direction the entry's nibble or with high bits;
  every tail state 0..`0x1F` (the empty ones too), negative and above, with
  the cell each waits on at its value two times in three and beside it
  otherwise (`Field_Request` 2, the timer 0 / 1 / `0x100`, the hold, the
  input bits, counter 3 at `0xB`, `0xD`, `0x10`, `0x11`, `0x12`, `0x15`), the
  sub byte `0xFF` half the time at `0x1B`; `Area135_FallTo`'s height at a
  difference whose count is not 0 (1..255 x 128 plus 0..127, either sign, a
  count of 0 aborts ours and faults the original: never seeded); cells some
  objects stand on for `Area135_ObjectAtCell` (the running object among them
  half the time), at the given (x, z) or half a cell / a row off; the
  marker's `+9`, steps and the point at its reach and one inside / outside
  either way; half the time one object alone at the cell (object 29 half of
  those); `MapView_GroundAt` for handler 9 by a mode the seed picks per round
  (garbage; every ground `0x8000`, so the highest keeps its start; or near
  the s16 edges); for handler 14 every record the running object can become
  (the party records, the first four field objects) on a jump cell half the
  time, so a stand-in that moves it lands on another; the party's cell words at `0x2C`, `0x2D`, `0x2B`, `0x2E` and z
  `0x2F`, `0x30`, `0x37`, `0x38`, with 0..4 members.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3, the script object, the sub byte (0..29), the effect slot (0..19),
  `Cond_ByteFD`, the input words, the hold, the marker's cell words.

**Result (in this worktree):** 276,000 rounds over the 46 functions (6,000
each), 354,886 calls to the stand-ins, 0 mismatches, 19,064 bytes of state
(32 regions) and the log compared. Coverage: every callee each function can
reach was called, and every state-table entry (`0x41E130` 3,035, `0x41E1E0`
2,965, `0x41E280` 489, `0x41E310` 439 - the walk's own two, since its states
2..12 run the lift's and spawn's - `0x41E390` 3,971, `0x41E3C0` 4,010,
`0x41E3F0` 993, `0x41E440` 4,097, `0x41E4E0` 4,001); the tail's rarest
callees `KeyItem_Add` / `Sound_StopMusic` / `Sound_LoadStream` 175 each,
`Flags_Toggle` 93, `Sound_ResumeAll` 114, `Sound_StreamDone` 243,
`Area135_PartyInBox` 241, `Area135_OpenMessage` 721.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4982 ours`, 448 self-test lines, no
mismatch (in this worktree; run twice, before and after the fuzz was strengthened, exit 0 both times, never a silent death).

## 4. Controls

Planted one at a time in `area_w3d.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w3d.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w3d`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches). **334 planted, 333 refused (exit 3) - 326 by a count, 7 by a fault (ours aborting on a boundary value the mutant produces; each such function has others refused by a count) - and 1 equivalent** (with a near variant planted and refused by a count). No hang. Every one of the 46 functions has at least one control of its own; a control in a shared helper lists every function it refused in.

A first pass (333 planted) left seven standing. Six were the fuzz's fault and were refused after it was strengthened: `Area135_Routes` was read only as shipped, where no route has one target cell `0xFF` without the other and no no-target route shares a cell with a later one (C2, C12: the table is a region now, the shipped bytes put back two rounds in three, the seed plants both cases); the marker's cell word was never seeded with a high byte (C16); `MapView_GroundAt`'s garbage answers never made all eight grounds `0x8000` (F2: a per-round answer mode); no record the running object could be moved to stood on a later jump cell (G4); object 29 was rarely the only match (W5). The seventh, I1, is equivalent.

The thinnest: G4 8 (Area135_JumpByOccupied), K8 9 (Area135_WalkToX), AA9 15 (Area135_NearMarker), T6 26 (Area135_Tail18), T68 27 (Area135_Tail18), T13 30 (Area135_Tail18).

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| H1 | `ScriptStep (x16)` | the script word at +0xB | Area135_RiseInZone4 1521, Area135_RiseElsewhere 4450, Area135_MarkerRoute 703, Area135_HeldMove1 2161, Area135_HeldMove7 2852, Area135_RestoreOnRequest5 3636, Area135_JumpByOccupied 5115, Area135_QueueStepZ 6000, Area135_WalkToX 6000, Area135_LiftUp 6000, Area135_LiftDown 6000, Area135_LiftDone 1244, Area135_SpawnCopy 5985, Area135_SpawnCountdown 6000, Area135_StampCells 2416 |
| H2 | `RunState (x4)` | odd states run the even entry | Area135_QueueRun 3026, Area135_WalkRun 2758, Area135_LiftRun 2726, Area135_SpawnRun 3015 |
| H3 | `Rise (x2)` | the rise from +0x16 | Area135_RiseInZone4 1521, Area135_RiseElsewhere 4450 |
| H4 | `Rise (x2)` | zone 5 | Area135_RiseInZone4 2300, Area135_RiseElsewhere 2302 |
| H5 | `Rise (x2)` | script back 4 | Area135_RiseInZone4 1521, Area135_RiseElsewhere 4450 |
| H6 | `HeldMove (x2)` | at the threshold refused | Area135_HeldMove1 716, Area135_HeldMove7 736 |
| H7 | `HeldMove (x2)` | the word unsigned | Area135_HeldMove1 734, Area135_HeldMove7 683 |
| H8 | `HeldMove (x2)` | button bit 0x10 | Area135_HeldMove1 1286, Area135_HeldMove7 1689 |
| H9 | `HeldMove (x2)` | script object +7 = 3 | Area135_HeldMove1 2161, Area135_HeldMove7 2852 |
| H10 | `HeldMove (x2)` | +0x14 through the pointer read before MoveCmd_Move | Area135_HeldMove1 986, Area135_HeldMove7 1316 |
| H11 | `HeldMove (x2)` | +0x18 cleared | Area135_HeldMove1 2161, Area135_HeldMove7 2852 |
| H12 | `Lift (x2)` | elevation + 1 | Area135_LiftUp 6000, Area135_LiftDown 6000 |
| H13 | `Lift (x2)` | redraw 3 | Area135_LiftUp 6000, Area135_LiftDown 6000 |
| H14 | `Lift (x2)` | state + 2 | Area135_LiftUp 6000, Area135_LiftDown 6000 |
| H15 | `Disarm (tail)` | tail kind 1 | Area135_Tail18 482 |
| H16 | `Speed8 (tail)` | the speed << 2 | Area135_Tail18 408 |
| H17 | `ObjectAt (tail)` | object 29 aborts | by a fault: ours aborts on object 29 (`ObjectAt`); H19 in the same helper refused by a count |
| H18 | `EffectAt (x4)` | effect record + 1 | Area135_ChoiceStartTail14 1085, Area135_Tail18 770, Area135_Init 561 |
| H19 | `ObjectAt (tail)` | object record + 2 | Area135_Tail18 125 |
| A1 | `Area135_ChoiceStartTail` | message 0xFFFE | Area135_ChoiceStartTail 5966 |
| A2 | `Area135_ChoiceStartTail` | state 1 | Area135_ChoiceStartTail 845 |
| A3 | `Area135_ChoiceStartTail` | answer 1 | Area135_ChoiceStartTail 1601 |
| A4 | `Area135_ChoiceStartTail` | tail kind 0x13 | Area135_ChoiceStartTail 845 |
| A5 | `Area135_ChoiceCount` | count + 2 | Area135_ChoiceCount 6000 |
| A6 | `Area135_ChoiceCount` | message 0x85 | Area135_ChoiceCount 5732 |
| A7 | `Area135_ChoiceCount` | marker +0x83 answer + 5 | Area135_ChoiceCount 6000 |
| A8 | `Area135_ChoiceCount` | the answer not read again after Crt_sprintf | Area135_ChoiceCount 2972 |
| A9 | `Area135_ChoiceCount` | printed count - 1 | Area135_ChoiceCount 6000 |
| A10 | `Area135_ChoiceCount` | sub byte count ^ 1 | Area135_ChoiceCount 5976 |
| A11 | `Area135_ChoiceStartTail14` | answer 2 not 1 | Area135_ChoiceStartTail14 1192 |
| A12 | `Area135_ChoiceStartTail14` | state 0x15 | Area135_ChoiceStartTail14 1621 |
| A13 | `Area135_ChoiceStartTail14` | kind 0x67 | Area135_ChoiceStartTail14 1085 |
| A14 | `Area135_ChoiceStartTail14` | +6 = 1 | Area135_ChoiceStartTail14 569 |
| A15 | `Area135_ChoiceStartTail14` | none not stored | Area135_ChoiceStartTail14 542 |
| A16 | `Area135_ChoiceStartTail14` | +0 = 2 | Area135_ChoiceStartTail14 1085 |
| A17 | `Area135_ChoiceStartTail14` | message 0xFFFD | Area135_ChoiceStartTail14 6000 |
| A18 | `Area135_ChoiceStartTail14` | kind 0x11 | Area135_ChoiceStartTail14 1627 |
| A19 | `Area135_ChoiceFlag37` | flag 0x36 set | Area135_ChoiceFlag37 807 |
| A20 | `Area135_ChoiceFlag37` | set on not 0 | Area135_ChoiceFlag37 6000 |
| A21 | `Area135_ChoiceFlag37` | flag 0x38 cleared | Area135_ChoiceFlag37 5193 |
| B1 | `Area135_ClearFlagC` | flag 0xD | Area135_ClearFlagC 6000 |
| B2 | `Area135_ClearFlagC` | sub 1 | Area135_ClearFlagC 6000 |
| B3 | `Area135_RiseElsewhere` | in zone 4 | Area135_RiseElsewhere 6000 |
| B4 | `Area135_LeaveByExit` | exit by +3 - 5 | Area135_LeaveByExit 5958 |
| B5 | `Area135_LeaveByExit` | tables swapped | Area135_LeaveByExit 4851 |
| B6 | `Area135_LeaveByExit` | z from the x byte | Area135_LeaveByExit 4844 |
| B7 | `Area135_LeaveByExit` | area 0x88 | Area135_LeaveByExit 6000 |
| B8 | `Area135_LeaveByExit` | flags 0x80 | Area135_LeaveByExit 6000 |
| B9 | `Area135_LeaveByExit` | bit 2 | Area135_LeaveByExit 4536 |
| B10 | `Area135_LeaveByExit` | x << 15 | Area135_LeaveByExit 4552 |
| C1 | `Area135_MarkerRoute` | z cell from byte 2 | Area135_MarkerRoute 1986 |
| C2 | `Area135_MarkerRoute` | z target not tested | Area135_MarkerRoute 198 |
| C3 | `Area135_MarkerRoute` | script back 2 | Area135_MarkerRoute 702 |
| C4 | `Area135_MarkerRoute` | counter 3 = 9 | Area135_MarkerRoute 116 |
| C5 | `Area135_MarkerRoute` | x half 0x4000 | Area135_MarkerRoute 1416 |
| C6 | `Area135_MarkerRoute` | z half flag from x | Area135_MarkerRoute 643 |
| C7 | `Area135_MarkerRoute` | bit 4 cleared | Area135_MarkerRoute 1163 |
| C8 | `Area135_MarkerRoute` | F7 speed 0x11 | Area135_MarkerRoute 1528 |
| C9 | `Area135_MarkerRoute` | +7 = 2 | Area135_MarkerRoute 1528 |
| C10 | `Area135_MarkerRoute` | direction from byte 5 | Area135_MarkerRoute 1526 |
| C11 | `Area135_MarkerRoute` | divisor 0x21 | Area135_MarkerRoute 246 |
| C12 | `Area135_MarkerRoute` | stops at a no-target route | Area135_MarkerRoute 268 |
| C13 | `Area135_MarkerRoute` | +3 = 0 | Area135_MarkerRoute 1528 |
| C14 | `Area135_MarkerRoute` | +8 through the pointer read before MoveCmd_OpF7 | Area135_MarkerRoute 705 |
| C15 | `Area135_MarkerRoute` | kind-2 x / z swapped | Area135_MarkerRoute 246 |
| C16 | `Area135_MarkerRoute` | the cell x as a byte | Area135_MarkerRoute 672 |
| D1 | `Area135_ToMarker` | Sprite_Kind2 + 4 | Area135_ToMarker 2984 |
| D2 | `Area135_ToMarker` | divisor 0x10 | Area135_ToMarker 2984 |
| D3 | `Area135_ToMarker` | x / z swapped | Area135_ToMarker 2984 |
| D4 | `Area135_ToMarker` | y from z | Area135_ToMarker 3016 |
| D5 | `Area135_ToMarker` | x + 1 | Area135_ToMarker 3016 |
| D6 | `Area135_FaceLeaderDir` | record 1 skipped | Area135_FaceLeaderDir 5023 |
| D7 | `Area135_FaceLeaderDir` | direction from +9 | Area135_FaceLeaderDir 1991 |
| D8 | `Area135_FaceLeaderDir` | index less 1 | Area135_FaceLeaderDir 5023 |
| D9 | `Area135_FaceLeaderDir` | animation +9 | Area135_FaceLeaderDir 1991 |
| E1 | `Area135_HeldMove1` | direction 2 | Area135_HeldMove1 2161 |
| E2 | `Area135_HeldMove7` | threshold 7 | Area135_HeldMove7 736 |
| E3 | `Area135_HeldMove1` | word +0x36 | Area135_HeldMove1 2499 |
| F1 | `Area135_FallToFloor` | z - 0x10000 | Area135_FallToFloor 6000 |
| F2 | `Area135_FallToFloor` | highest from 0x8001 | Area135_FallToFloor 111 |
| F3 | `Area135_FallToFloor` | x - 0x10000 | Area135_FallToFloor 6000 |
| F4 | `Area135_FallToFloor` | three columns | Area135_FallToFloor 6000 |
| F5 | `Area135_FallToFloor` | unsigned dwords | Area135_FallToFloor 113 |
| F6 | `Area135_FallToFloor` | height scale 2 | Area135_FallToFloor 6000 |
| F7 | `Area135_FallToFloor` | x step 0x8000 | Area135_FallToFloor 6000 |
| F8 | `Area135_FallToFloor` | z step 0x20000 | Area135_FallToFloor 6000 |
| F9 | `Area135_FallToFloor` | sub found + 1 | Area135_FallToFloor 5950 |
| F10 | `Area135_FallToFloor` | top + 0x281 | Area135_FallToFloor 5776 |
| F11 | `Area135_FallToFloor` | best + 0x100 | Area135_FallToFloor 224 |
| F12 | `Area135_FallToFloor` | one row | Area135_FallToFloor 6000 |
| F13 | `Area135_FallToFloor` | found / none swapped | Area135_FallToFloor 6000 |
| F14 | `Area135_Fall780` | 0x781 | Area135_Fall780 6000 |
| F15 | `Area135_StampCellsRun` | restores | Area135_StampCellsRun 6000 |
| F16 | `Area135_RestoreCellsRun` | stamps | Area135_RestoreCellsRun 6000 |
| F17 | `Area135_RestoreOnRequest5` | request 4 | Area135_RestoreOnRequest5 3584 |
| F18 | `Area135_RestoreOnRequest5` | script back 3 | Area135_RestoreOnRequest5 3636 |
| G1 | `Area135_JumpByOccupied` | x half 0x4000 | Area135_JumpByOccupied 3598 |
| G2 | `Area135_JumpByOccupied` | z from the x byte | Area135_JumpByOccupied 3598 |
| G3 | `Area135_JumpByOccupied` | stops at none | Area135_JumpByOccupied 3598 |
| G4 | `Area135_JumpByOccupied` | the running object not read again | Area135_JumpByOccupied 8 |
| G5 | `Area135_JumpByOccupied` | 2 x the cells | Area135_JumpByOccupied 5115 |
| G6 | `Area135_JumpByOccupied` | three cells | Area135_JumpByOccupied 3382 |
| G7 | `Area135_JumpByOccupied` | z off by 0x8000 | Area135_JumpByOccupied 3598 |
| R1 | `Area135_QueueRun` | the spawn's table | Area135_QueueRun 6000 |
| R2 | `Area135_SpawnRun` | the queue's table | Area135_SpawnRun 6000 |
| R3 | `Area135_LiftRun` | the walk's table | Area135_LiftRun 6000 |
| R4 | `Area135_WalkRun` | reach 12 (state 12 aborts ours) | by a fault: ours aborts on the walk's state 12; R1..R3 refused by a count |
| R5 | `Area135_LiftRun` | reach 10 (state 10 aborts ours) | by a fault: ours aborts on the lift's state 10; R1..R3 refused by a count |
| I1 | `Area135_QueueStepZ` | the count as a word | equivalent: the stored byte is bits 15..22 of `(n << 17) + ...`, so only n's low six bits reach it; variant I1b (`& 3`) refused |
| I2 | `Area135_QueueStepZ` | direction 1 | Area135_QueueStepZ 6000 |
| I3 | `Area135_QueueStepZ` | base 0xC0000 | Area135_QueueStepZ 6000 |
| I4 | `Area135_QueueStepZ` | >> 14 | Area135_QueueStepZ 5842 |
| I5 | `Area135_QueueStepZ` | zero taken as negative | Area135_QueueStepZ 182 |
| I6 | `Area135_QueueStepZ` | ^ 2 | Area135_QueueStepZ 3219 |
| I7 | `Area135_QueueStepZ` | not negated, complemented | Area135_QueueStepZ 3219 |
| I8 | `Area135_QueueStepZ` | state + 2 | Area135_QueueStepZ 6000 |
| I9 | `Area135_QueueStepZ` | moves on a step of 0 | Area135_QueueStepZ 182 |
| I10 | `Area135_QueueStepZ` | +0x14 of the script object | Area135_QueueStepZ 4552 |
| J1 | `Area135_QueueStepX` | 5 stops too | Area135_QueueStepX 962 |
| J2 | `Area135_QueueStepX` | direction 4 | Area135_QueueStepX 4994 |
| J3 | `Area135_QueueStepX` | the next cell | Area135_QueueStepX 2997 |
| J4 | `Area135_QueueStepX` | >> 16 | Area135_QueueStepX 4037 |
| J5 | `Area135_QueueStepX` | state 1 | Area135_QueueStepX 4994 |
| J6 | `Area135_QueueStepX` | half 0x4000 | Area135_QueueStepX 2746 |
| K1 | `Area135_WalkToX` | direction 6 | Area135_WalkToX 6000 |
| K2 | `Area135_WalkToX` | target 0x1C0000 | Area135_WalkToX 6000 |
| K3 | `Area135_WalkToX` | ^ 1 | Area135_WalkToX 2290 |
| K4 | `Area135_WalkToX` | negated less 1 | Area135_WalkToX 2290 |
| K5 | `Area135_WalkToZ` | direction 4 | Area135_WalkToZ 6000 |
| K6 | `Area135_WalkToZ` | target 0x140000 | Area135_WalkToZ 6000 |
| K7 | `Area135_WalkToZ` | state 2 | Area135_WalkToZ 6000 |
| K8 | `Area135_WalkToX` | negative from 0x81 | Area135_WalkToX 9 |
| L1 | `Area135_LiftUp` | +8 | Area135_LiftUp 6000 |
| L2 | `Area135_LiftDown` | -8 | Area135_LiftDown 6000 |
| L3 | `Area135_LiftDone` | x 0x168001 | Area135_LiftDone 1427 |
| L4 | `Area135_LiftDone` | z at or past | Area135_LiftDone 856 |
| L5 | `Area135_LiftDone` | on by 2 | Area135_LiftDone 1244 |
| L6 | `Area135_LiftDone` | state 1 | Area135_LiftDone 6000 |
| M1 | `Area135_SpawnCopy` | the slot as a byte | Area135_SpawnCopy 5972 |
| M2 | `Area135_SpawnCopy` | none 0xFE | Area135_SpawnCopy 1998 |
| M3 | `Area135_SpawnCopy` | records of 12 | Area135_SpawnCopy 3337 |
| M4 | `Area135_SpawnCopy` | z from x | Area135_SpawnCopy 4002 |
| M5 | `Area135_SpawnCopy` | y from +0x40 | Area135_SpawnCopy 4002 |
| M6 | `Area135_SpawnCopy` | the old object not made current again | Area135_SpawnCopy 919 |
| M7 | `Area135_SpawnCopy` | +0xA from +8 | Area135_SpawnCopy 3986 |
| M8 | `Area135_SpawnCopy` | +9 = 1 | Area135_SpawnCopy 4002 |
| M9 | `Area135_SpawnCopy` | the object read before the restore | Area135_SpawnCopy 1785 |
| M10 | `Area135_SpawnCopy` | script back 4 | Area135_SpawnCopy 5278 |
| M11 | `Area135_SpawnCopy` | state + 2 | Area135_SpawnCopy 4002 |
| N1 | `Area135_SpawnCountdown` | less 2 | Area135_SpawnCountdown 5988 |
| N2 | `Area135_SpawnCountdown` | at 1 | Area135_SpawnCountdown 2156 |
| N3 | `Area135_SpawnCountdown` | +0 = 1 | Area135_SpawnCountdown 1574 |
| N4 | `Area135_SpawnCountdown` | +9 the old count | Area135_SpawnCountdown 1932 |
| N5 | `Area135_SpawnCountdown` | +9 through the pointer read before 0x46D770 | Area135_SpawnCountdown 1948 |
| N6 | `Area135_SpawnCountdown` | state 1 | Area135_SpawnCountdown 1574 |
| N7 | `Area135_SpawnCountdown` | script back 1 | Area135_SpawnCountdown 5994 |
| O1 | `Area135_SpawnKind1AtLeader` | kind 2 | Area135_SpawnKind1AtLeader 6000 |
| O2 | `Area135_SpawnKind1AtLeader` | record 1 made current | Area135_SpawnKind1AtLeader 3169 |
| O3 | `Area135_SpawnKind1AtLeader` | list 0 | Area135_SpawnKind1AtLeader 5253 |
| O4 | `Area135_SpawnKind1AtLeader` | none stored | Area135_SpawnKind1AtLeader 1162 |
| O5 | `Area135_SpawnKind1AtLeader` | x / z swapped | Area135_SpawnKind1AtLeader 6000 |
| O6 | `Area135_SpawnKind1AtLeader` | kinds + 1 | Area135_SpawnKind1AtLeader 4941 |
| P1 | `Area135_CellHook` | the direction byte masked | Area135_CellHook 294 |
| P2 | `Area135_CellHook` | the "none" test at 3 | Area135_CellHook 4188 |
| P3 | `Area135_CellHook` | state from byte 2 | Area135_CellHook 1812 |
| P4 | `Area135_CellHook` | kind 0x13 | Area135_CellHook 1812 |
| P5 | `Area135_CellHook` | x / z swapped | Area135_CellHook 1812 |
| P6 | `Area135_CellHook` | answers 2 | Area135_CellHook 1812 |
| P7 | `Area135_CellHook` | z high byte | Area135_CellHook 1811 |
| Q1 | `Area135_StepHook` | zone 7 not | Area135_StepHook 704 |
| Q2 | `Area135_StepHook` | flag 0xD | Area135_StepHook 2993 |
| Q3 | `Area135_StepHook` | drop-in 1 | Area135_StepHook 1323 |
| Q4 | `Area135_StepHook` | x / z swapped | Area135_StepHook 1989 |
| Q5 | `Area135_StepHook` | answers 3 | Area135_StepHook 1323 |
| Q6 | `Area135_StepHook` | zone 5 not 6 | Area135_StepHook 1525 |
| T1 | `Area135_Tail18` | key item 7 | Area135_Tail18 161 |
| T2 | `Area135_Tail18` | message 0x82 | Area135_Tail18 161 |
| T3 | `Area135_Tail18` | stream 3 | Area135_Tail18 161 |
| T4 | `Area135_Tail18` | state 2 | Area135_Tail18 161 |
| T5 | `Area135_Tail18` | request 3 waits | Area135_Tail18 125 |
| T6 | `Area135_Tail18` | the answer as a byte | Area135_Tail18 26 |
| T7 | `Area135_Tail18` | request 3 waits (state 1) | Area135_Tail18 91 |
| T8 | `Area135_Tail18` | message 0x88 | Area135_Tail18 159 |
| T9 | `Area135_Tail18` | flag 0x2B | Area135_Tail18 81 |
| T10 | `Area135_Tail18` | sound 0x204 | Area135_Tail18 81 |
| T11 | `Area135_Tail18` | timer 0xB | Area135_Tail18 81 |
| T12 | `Area135_Tail18` | state 7 | Area135_Tail18 81 |
| T13 | `Area135_Tail18` | the timer as a byte | Area135_Tail18 30 |
| T14 | `Area135_Tail18` | timer less 2 | Area135_Tail18 87 |
| T15 | `Area135_Tail18` | flag 0xB | Area135_Tail18 239 |
| T16 | `Area135_Tail18` | message 0x85 | Area135_Tail18 157 |
| T17 | `Area135_Tail18` | message 0x86 | Area135_Tail18 82 |
| T18 | `Area135_Tail18` | flag 0xD set | Area135_Tail18 82 |
| T19 | `Area135_Tail18` | printed sub + 1 | Area135_Tail18 157 |
| T20 | `Area135_Tail18` | state 0xC | Area135_Tail18 239 |
| T21 | `Area135_Tail18` | request 1 waits | Area135_Tail18 131 |
| T22 | `Area135_Tail18` | divisor + 1 | Area135_Tail18 174 |
| T23 | `Area135_Tail18` | FA word 1 | Area135_Tail18 174 |
| T24 | `Area135_Tail18` | state 0x16 | Area135_Tail18 174 |
| T25 | `Area135_Tail18` | hold 1 passes | Area135_Tail18 45 |
| T26 | `Area135_Tail18` | message 0x8A | Area135_Tail18 155 |
| T27 | `Area135_Tail18` | effect state 3 | Area135_Tail18 155 |
| T28 | `Area135_Tail18` | the speed read before the message | Area135_Tail18 83 |
| T29 | `Area135_Tail18` | the slot read before the message | Area135_Tail18 72 |
| T30 | `Area135_Tail18` | button 0x40 | Area135_Tail18 110 |
| T31 | `Area135_Tail18` | message 0x8E for 0x8C | Area135_Tail18 163 |
| T32 | `Area135_Tail18` | effect state 2 (0x16) | Area135_Tail18 163 |
| T33 | `Area135_Tail18` | counter 0xB | Area135_Tail18 163 |
| T34 | `Area135_Tail18` | state 0x18 | Area135_Tail18 163 |
| T35 | `Area135_Tail18` | waits for 0xC | Area135_Tail18 114 |
| T36 | `Area135_Tail18` | message 0x8C for 0x8D | Area135_Tail18 99 |
| T37 | `Area135_Tail18` | effect state 3 (0x17) | Area135_Tail18 99 |
| T38 | `Area135_Tail18` | glide divisor 1 | Area135_Tail18 254 |
| T39 | `Area135_Tail18` | press bit 0x10 | Area135_Tail18 124 |
| T40 | `Area135_Tail18` | pressed, not held | Area135_Tail18 141 |
| T41 | `Area135_Tail18` | counter 0xD | Area135_Tail18 176 |
| T42 | `Area135_Tail18` | effect state 4 (0x19) | Area135_Tail18 176 |
| T43 | `Area135_Tail18` | state 0x1B | Area135_Tail18 176 |
| T44 | `Area135_Tail18` | waits for 0xE | Area135_Tail18 105 |
| T45 | `Area135_Tail18` | state 0x1C | Area135_Tail18 97 |
| T46 | `Area135_Tail18` | effect state 5 (0x1A) | Area135_Tail18 97 |
| T47 | `Area135_Tail18` | waits for 0x11 | Area135_Tail18 135 |
| T48 | `Area135_Tail18` | none 0xFE (0xFF aborts ours) | by a fault: `0xFF` now reaches the object write and ours aborts; T54 (the none path) refused by a count |
| T49 | `Area135_Tail18` | state 0x1D on none | Area135_Tail18 60 |
| T50 | `Area135_Tail18` | +1 = 6 | Area135_Tail18 41 |
| T51 | `Area135_Tail18` | +0x18 = 1 | Area135_Tail18 41 |
| T52 | `Area135_Tail18` | +0x1C = 4 | Area135_Tail18 41 |
| T53 | `Area135_Tail18` | +0x8A + 2 | Area135_Tail18 41 |
| T54 | `Area135_Tail18` | message 0x8E on none | Area135_Tail18 60 |
| T55 | `Area135_Tail18` | waits for 0x12 (0x1C) | Area135_Tail18 193 |
| T56 | `Area135_Tail18` | map flag 0x20 | Area135_Tail18 123 |
| T57 | `Area135_Tail18` | state 0x1E (0x1C) | Area135_Tail18 165 |
| T58 | `Area135_Tail18` | waits for 0x13 (0x1D) | Area135_Tail18 92 |
| T59 | `Area135_Tail18` | +1 = 5 | Area135_Tail18 84 |
| T60 | `Area135_Tail18` | bit 4 cleared | Area135_Tail18 66 |
| T61 | `Area135_Tail18` | +0x8A + 2 (0x1D) | Area135_Tail18 84 |
| T62 | `Area135_Tail18` | flag 0x20 cleared | Area135_Tail18 58 |
| T63 | `Area135_Tail18` | message 0x90 (0x1D) | Area135_Tail18 84 |
| T64 | `Area135_Tail18` | waits for 0x16 | Area135_Tail18 88 |
| T65 | `Area135_Tail18` | x from the leader z | Area135_Tail18 80 |
| T66 | `Area135_Tail18` | state 0x1E (0x1E) | Area135_Tail18 80 |
| T67 | `Area135_Tail18` | effect state 6 | Area135_Tail18 80 |
| T68 | `Area135_Tail18` | hold 1 passes (0x1F) | Area135_Tail18 27 |
| T69 | `Area135_Tail18` | counter 1 | Area135_Tail18 163 |
| T70 | `Area135_Tail18` | sub 1 | Area135_Tail18 163 |
| T71 | `Area135_Tail18` | request 1 waits (0x14) | Area135_Tail18 132 |
| T72 | `Area135_Tail18` | no glide call (0x1A) | Area135_Tail18 245 |
| T73 | `Area135_Tail18` | no glide call (0x1E) | Area135_Tail18 220 |
| T74 | `Area135_Tail18` | state 2 runs state 0 | Area135_Tail18 135 |
| T75 | `Area135_Tail18` | state 7 runs state 6 | Area135_Tail18 134 |
| T76 | `Area135_Tail18` | state 0x13 runs 0x14 | Area135_Tail18 102 |
| T77 | `Area135_Tail18` | z + 1 | Area135_Tail18 80 |
| T78 | `Area135_Tail18` | divisor + 8 (0x1E) | Area135_Tail18 80 |
| T79 | `Area135_Tail18` | no glide call (0x1B) | Area135_Tail18 229 |
| T80 | `Area135_Tail18` | no glide call (0x1D) | Area135_Tail18 219 |
| T81 | `Area135_Tail18` | no glide call (0x17) | Area135_Tail18 254 |
| T82 | `Area135_Tail18` | no wait (0xB) | Area135_Tail18 82 |
| U1 | `Area135_Init` | from zone 2 | Area135_Init 939 |
| U2 | `Area135_Init` | the op + 1 | Area135_Init 750 |
| U3 | `Area135_Init` | flag 0xB | Area135_Init 750 |
| U4 | `Area135_Init` | the zone not read again | Area135_Init 268 |
| U5 | `Area135_Init` | bit 1 | Area135_Init 1135 |
| U6 | `Area135_Init` | zone 5 not 6 | Area135_Init 1635 |
| U7 | `Area135_Init` | marker 2 | Area135_Init 2731 |
| U8 | `Area135_Init` | zone 1 | Area135_Init 1659 |
| U9 | `Area135_Init` | kind 0x54 | Area135_Init 561 |
| U10 | `Area135_Init` | +0x1C = 7 | Area135_Init 561 |
| U11 | `Area135_Init` | +0x18 = 1 | Area135_Init 561 |
| U12 | `Area135_Init` | counter 1 | Area135_Init 839 |
| U13 | `Area135_Init` | sub 1 | Area135_Init 750 |
| U14 | `Area135_Init` | +0 = 2 | Area135_Init 561 |
| U15 | `Area135_Init` | bit 0 set in every zone | Area135_Init 2289 |
| V1 | `Area135_FallTo` | floor, not truncate | by a fault: the floored quotient makes a count of 0 that ours aborts on; V2, V3, V4, V6 refused by a count |
| V2 | `Area135_FallTo` | << 3 | Area135_FallTo 6000 |
| V3 | `Area135_FallTo` | divisor + 1 | Area135_FallTo 4223 |
| V4 | `Area135_FallTo` | +0xC = 1 | Area135_FallTo 6000 |
| V5 | `Area135_FallTo` | against +0x3C | by a fault: against `+0x3C` the seeded height gives counts of 0, ours aborts; V2, V3, V4, V6 refused by a count |
| V6 | `Area135_FallTo` | +0x10 = 1 | Area135_FallTo 6000 |
| V7 | `Area135_FallTo` | the difference not taken as s16 | by a fault: without the s16 the difference gives counts of 0, ours aborts; V2, V3, V4, V6 refused by a count |
| W1 | `Area135_ObjectAtCell` | bit 1 | Area135_ObjectAtCell 1013 |
| W2 | `Area135_ObjectAtCell` | itself not skipped | Area135_ObjectAtCell 78 |
| W3 | `Area135_ObjectAtCell` | kind 0xB | Area135_ObjectAtCell 1907 |
| W4 | `Area135_ObjectAtCell` | z at +0x3C | Area135_ObjectAtCell 1862 |
| W5 | `Area135_ObjectAtCell` | 29 objects | Area135_ObjectAtCell 703 |
| W6 | `Area135_ObjectAtCell` | none 0xFE | Area135_ObjectAtCell 4138 |
| W7 | `Area135_ObjectAtCell` | x by the cell only | Area135_ObjectAtCell 72 |
| X1 | `Area135_CountQueue` | three cells | Area135_CountQueue 5429 |
| X2 | `Area135_CountQueue` | x 0x200000 | Area135_CountQueue 6000 |
| X3 | `Area135_CountQueue` | rows << 16 | Area135_CountQueue 5814 |
| X4 | `Area135_CountQueue` | stops at the first occupied | Area135_CountQueue 6000 |
| Y1 | `Area135_StampCells` | request 4 | Area135_StampCells 3642 |
| Y2 | `Area135_StampCells` | x - 0x4000 | Area135_StampCells 907 |
| Y3 | `Area135_StampCells` | z - 0x10000 | Area135_StampCells 1818 |
| Y4 | `Area135_StampCells` | +0x20 = 1 | Area135_StampCells 1699 |
| Y5 | `Area135_StampCells` | height 0x11 | Area135_StampCells 3584 |
| Y6 | `Area135_StampCells` | shift + 4 | Area135_StampCells 3441 |
| Y7 | `Area135_StampCells` | byte 0x12 | Area135_StampCells 3584 |
| Y8 | `Area135_StampCells` | shift + 8 for x + 1 | Area135_StampCells 3572 |
| Y9 | `Area135_StampCells` | rows 4 bits apart | Area135_StampCells 3558 |
| Y10 | `Area135_StampCells` | the object not read again for x + 1 | Area135_StampCells 3361 |
| Y11 | `Area135_StampCells` | +0x20 read before AreaMap_ByteAt | Area135_StampCells 2346 |
| Y12 | `Area135_StampCells` | x + 1 height at x | Area135_StampCells 3584 |
| Y13 | `Area135_StampCells` | rows 2 apart | Area135_StampCells 3584 |
| Z1 | `Area135_RestoreCells` | x - 0x8000 | Area135_RestoreCells 2931 |
| Z2 | `Area135_RestoreCells` | height 1 | Area135_RestoreCells 6000 |
| Z3 | `Area135_RestoreCells` | shift + 1 | Area135_RestoreCells 5999 |
| Z4 | `Area135_RestoreCells` | shift + 24 | Area135_RestoreCells 6000 |
| Z5 | `Area135_RestoreCells` | the object read at entry | Area135_RestoreCells 5335 |
| Z6 | `Area135_RestoreCells` | rows from z + 1 | Area135_RestoreCells 6000 |
| Z7 | `Area135_RestoreCells` | x + 1 height a row down | Area135_RestoreCells 6000 |
| AA1 | `Area135_NearMarker` | reach + 3 | Area135_NearMarker 727 |
| AA2 | `Area135_NearMarker` | << 14 | Area135_NearMarker 783 |
| AA3 | `Area135_NearMarker` | x at the reach passes | Area135_NearMarker 299 |
| AA4 | `Area135_NearMarker` | z at the reach passes | Area135_NearMarker 302 |
| AA5 | `Area135_NearMarker` | x by the z step | Area135_NearMarker 170 |
| AA6 | `Area135_NearMarker` | z plus the marker | Area135_NearMarker 1020 |
| AA7 | `Area135_NearMarker` | |dx| one less for negatives | Area135_NearMarker 285 |
| AA8 | `Area135_NearMarker` | +0x70 & 0x7F | Area135_NearMarker 370 |
| AA9 | `Area135_NearMarker` | +9 & 0x7F | Area135_NearMarker 15 |
| AB1 | `Area135_PartyInBox` | x 0x2E not 0x2D | Area135_PartyInBox 900 |
| AB2 | `Area135_PartyInBox` | z from 0x31 | Area135_PartyInBox 350 |
| AB3 | `Area135_PartyInBox` | z to 0x36 | Area135_PartyInBox 301 |
| AB4 | `Area135_PartyInBox` | one member fewer | Area135_PartyInBox 318 |
| AB5 | `Area135_PartyInBox` | z from x | Area135_PartyInBox 983 |
| AB6 | `Area135_PartyInBox` | z unsigned | Area135_PartyInBox 735 |
| AC1 | `Area135_Kind2ToObject0` | the whole dword tested | Area135_Kind2ToObject0 3003 |
| AC2 | `Area135_Kind2ToObject0` | divisor + 1 | Area135_Kind2ToObject0 2997 |
| AC3 | `Area135_Kind2ToObject0` | x / z swapped at 0 | Area135_Kind2ToObject0 3003 |
| AC4 | `Area135_Kind2ToObject0` | x / z swapped | Area135_Kind2ToObject0 2997 |
| AD1 | `Area135_OpenMessage` | id & 0x7F | Area135_OpenMessage 2983 |
| AD2 | `Area135_OpenMessage` | pools + 4 | Area135_OpenMessage 6000 |
| AD3 | `Area135_OpenMessage` | 0x3C1 | Area135_OpenMessage 6000 |
| AD4 | `Area135_OpenMessage` | 0xAA1 | Area135_OpenMessage 6000 |
| I1b | `Area135_QueueStepZ` | the count & 3 (variant of I1) | Area135_QueueStepZ 1999 |

## 5. What nothing reached

No recorded route enters area 135 (the owner's routes are world 0 to 2); the
band is fuzz only. The live check is the owner's.

## 6. Aborts and latent defects

Where the original would fault or write outside what it indexes, ours aborts
with a message naming the function and the value (round nine section 6: no
ledger entry). None of these is reached by the area's own code as read;
each is a latent defect of Capcom's, described, not fixed.

- **`Area135_FallTo` divides by its frame count** `+9` = `(|d / 128| << 4)` as
  a byte (`idiv ebx` at `0x41EC18`). It is 0 whenever the height is within
  127 of the object's `+0x3E`, or `|d / 128|` is a multiple of 16 (`|d|`
  within 127 above a multiple of 2,048). Handler 10 falls to `0x780`
  and handler 9 to a ground or an object top, so an object already at (or
  within 127 of) that height faults the original with a divide error. Ours
  aborts.
- **The tail writes through the effect slot `0x675CC0` unchecked** (states
  `0x15`, `0x16`, `0x17`, `0x19`, `0x1A`, `0x1E`: `Effect_Objects + slot *
  0x80 + 1`), and choice 2 stores `Effect_FindFree`'s none (`0xFF`) into that
  byte before arming the tail at `0x14`: with no free effect record the
  original's tail writes `0x7E9161` (record 255's `+1`, past the 20 records)
  in each of those six states. Ours aborts at a slot of 20 or more.
- **The tail's states `0x1B` and `0x1D` write the field object in the sub
  byte** (`Sprite_Objects + sub * 0xA4`), `0x1B` after testing only for
  `0xFF`, `0x1D` not at all; the sub byte is `Area135_FallToFloor`'s find
  (0..29 or `0xFF`) - but choice 1 and the init also write it (the answer + 1,
  0). An index of 30 or more (or `0xFF` at `0x1D`) writes past the field
  objects in the original; ours aborts.
- **The dispatchers index their tables unchecked** (section 1.4): past the
  code pointers they reach the original jumps through a zero or a data word.
  Ours aborts.
- **Reads kept unchecked** (they stay inside `.data` or mapped memory):
  `Area135_LeaveByExit`'s exit index, `Area135_QueueStepX`'s count,
  `Area135_SpawnCopy`'s record index, handler 19's effect kind by party list
  1, and `Area135_PartyInBox` past the three party records when
  `Field_MemberCount` is above 3.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D137 (divides by 0), D158 (nibble against facing) in
[`known-defects.md`](known-defects.md).

## 7. Starts, the tool and the tables

46 starts, 46 functions: every start the tool lists is a function entered at
its first byte; none dropped, none added. The tool's rows list the choice and
handler arrays as the descriptor gives them (choices 4..23 the handlers'
bodies), the four state tables as `data_tables` entries with the counts
section 1.4 explains, and `0x41E630`'s extent with its byte and jump tables
(`0x504`; the code ends at `0x41EAC6`). `0x62DA14` (`Area135_QueueCells`, four
bytes between two tables) is data, correctly not a start. No `jmp` over
`nop`s opens any function here; handlers 11 and 12 (`0x41E050`, `0x41E060`)
are a bare `jmp` each, which `CloneOriginal` accepts because the clone lists
that `E9` as a call site at offset 0 (it refuses an `E9` entry it has no call
site for).

Named `.data` (symbols.toml `[[data]]`): `Area135_ExitsZone4`, `_Exits`,
`_Routes`, `_JumpCells`, `_QueueStates`, `_QueueCells`, `_WalkStates`,
`_LiftStates`, `_SpawnStates`, `_SpawnOps`, `_CellEntries`, `_InitOp`,
`_EffectKinds` (`0x62C168`, before the descriptor; its length is not
measured).

## 8. Calls across groups

- **Outbound, raw (nobody owns):** `0x46D710` (copies the object's script
  from `+0x50` / `+0x54` to `0x8C5D80`, calls `0x46C200` and `0x46C430`,
  `+9` = `0x10`) and `0x46D770` (`0x46C310`, then `jmp 0x46C4B0`), both from
  the spawn states. No call into another group's band; `0x4220D0` is not
  called.
- **Outbound, named (ours already):** `ScriptFlags_Set40` / `Clear40`,
  `Flags_Set` / `Clear` / `Test` / `Toggle`, `Effect_FindFree`,
  `Sprite_FindFree`, `EventOp_9x`, `EventOp_Bx`, `MoveCmd_OpF7`,
  `Field_ChangeArea`, `Sprite_SetAnimation`, `MapView_GroundAt`,
  `AreaMap_Elevation`, `AreaMap_SetHeight` (whose comment already names area
  135), `AreaMap_ByteAt`, `AreaMap_SetByte`, `Party_DropIn`, `KeyItem_Add`,
  `Msg_OpenScript`, `Sound_LoadStream`, `Sound_StreamDone`,
  `Sound_PlayEffect`, `Window_FreeCurrent`; Capcom's: `Crt_sprintf`,
  `MoveCmd_Move`, `Effect_Spawn`, `Sound_StopMusic`, `Sound_ResumeAll`.
- **Inbound, for the rebinding pass:** `Area_StepHook`'s case at `0x56E162`
  (ours, `event_ops.cpp` `kStepHandlers`, by the raw `0x41E5E0`) into
  `Area135_StepHook`; the cell hook table `0x662F5C` into `Area135_CellHook`
  (read in place by `Area_CellHook`); `Field_ModeTailKinds[18]` `0x662D30`
  into `Area135_Tail18`; the descriptor's arrays. No other reference into the
  band exists in the image (a scan of every `E8` / `E9` and every dword,
  2026-09-28).
