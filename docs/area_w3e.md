# World 3, areas 136 and 139..142: the band `0x41EFE0..0x420800`

**Status:** IN PROGRESS (2026-09-28) - 52 functions ours
(`src/game/area_w3e.cpp`, shadow name `area_w3e`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 312,000 rounds (in this worktree); CONTROLS_SUMMARY (section 4).
Fuzz only: no recorded route reaches the band (section 8). No divergence;
the four state-table dispatchers abort past the code their tables hold, where
the original would jump through data (section 6).

Group AR3E of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
52 starts, none ours before, **52 taken**; no start dropped, none added
(section 7). Areas 137 and 138 have no code in the band: their descriptors
(`0x62EB50`, `0x62EBF8`) have no `+0x34`, `+0x3C` or `+0x40`.

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, hooks, tails, triggers,
effect handlers and state tables have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`), as
in [`area_w2d.md`](area_w2d.md) section 1. Functions that are both a choice
and a handler (`+0x34[n + k] = +0x3C[n]`) are fuzzed as choices when they
write the message word, as handlers otherwise.

### Area 136 (descriptor `0x62EA30`; PSX `0x801F43B4`)

Fifteen choices and eleven handlers: choice `4 + n` is handler `n`. Choice
13 = handler 9 is `0x42A4B0` (another block's, PSX `0x801F3148`).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41EFE0` | `Area136_ChoiceTail19Sub0` | `0x2D` | choice 0 | kChoice | the answer read, message `0xFFFF`; answer 0: `ScriptFlags_Set40`, tail kind 19 at state 0, sub-kind 0 |
| `0x41F010` | `Area136_ChoiceTail19Sub1` | `0x2D` | choice 1 | kChoice | the same with sub-kind 1 |
| `0x41F040` | `Area136_ChoiceMessage2` | `0x23` | choice 2 | kChoice | the message word `Area136_ChoiceMessages2[answer]` (signed); answer 0: counter 0 = 6 |
| `0x41F070` | `Area136_ChoiceMessage3` | `0x23` | choice 3 | kChoice | the same with `Area136_ChoiceMessages3` |
| `0x41F0A0` | `Area136_MoveUntilLimit` | `0x53` | choice 4 = handler 0 (PSX `0x801F2D68`) | kHandler | the running object's z one direction step on (`Field_DirectionSteps[+8 & 7]`, dwords) at most `Area136_ZLimits[sub-kind] << 16` (unsigned): the script object's `+7` 1, `MoveCmd_Move(it, 5)`, the script position - 2 |
| `0x41F100` | `Area136_CellsRow51` | `0x25` | choice 5 = handler 1 (PSX `0x801F2E08`) | kHandler | cells (9, `0x59..0x5B`) `0x51` |
| `0x41F130` | `Area136_SpawnMember1Kind2` | `0x42` | choice 6 = handler 2 (PSX `0x801F2E50`) | kHandler | `Sprite_Current` = party record 1; `Effect_Spawn(2, 0, Area136_EffectByCharA[slot 1's character], its +0x2E, +0x30)`; a slot to `Sprite_Current +0xB` (read again) |
| `0x41F180` | `Area136_SpawnMember2Kind2` | `0x46` | choice 7 = handler 3 (PSX `0x801F2ED0`) | kHandler | the same for party record 2 and slot 2 |
| `0x41F1D0` | `Area136_SpawnMember1Kind1` | `0x42` | choice 8 = handler 4 (PSX `0x801F2F50`) | kHandler | record 1, kind 1, `Area136_EffectByCharB` |
| `0x41F220` | `Area136_CellsRow10` | `0x25` | choice 9 = handler 5 (PSX `0x801F2FD0`) | kHandler | cells (9, `0x59..0x5B`) `0x10` |
| `0x41F250` | `Area136_Counter0ByTile` | `0x25` | choice 10 = handler 6 (PSX `0x801F3018`) | kHandler | counter 0 = `0xA` when the low byte of the word `0x939A00` is `0x62..0x66`, else `0x1E` |
| `0x41F280` | `Area136_SpawnMember1Kind3` | `0x42` | choice 11 = handler 7 (PSX `0x801F3048`) | kHandler | record 1, kind 3, table B |
| `0x41F2D0` | `Area136_SpawnLeaderKind1` | `0x42` | choice 12 = handler 8 (PSX `0x801F30C8`) | kHandler | the leader's record, kind 1, table B **by party slot 1's character** (as read) |
| `0x41F320` | `Area136_ShiftCameraUp2` | `0x12` | choice 14 = handler 10 (PSX `0x801F3170`); area 119 choice 2 = handler 1, area 131 handler 7, area 188 choice 5 = handler 1 | kHandler | `Camera_ShiftY` + 2, `MapView_Redraw` 2 |
| `0x41F340` | `Area136_Tail19` | `0x19E` | tail kind 19 | kTail | fourteen states through a byte map and an eight-entry jump table (below) |
| `0x41F4E0` | `Area136_InitExtraObject` | `0x2F` | init (PSX `0x801F33BC`) | kInit | the byte `0x905E68` 1 and `Cond_ByteFD` 4: `Sprite_ObjectsExtra[0] +0x83` = 2; 4 and 1: = 4 |

Tail 19's states (byte map `0x41F4D0`, jump table `0x41F4B0`, both inside the
extent): 0 `KeyItem_Has(6)`: state 10 when held, else 1; 1 message `0x83`
(once `Field_Request` is not 2), state 2; 2 disarm (kind, state, sub-kind 0,
`ScriptFlags_Clear40`); 10 the flag row's flag `0x2E`, then message `0x82`,
`Flags_Toggle(story, 0x29)`, state `0xB`; 11 `Party_DropIn(0)`, state `0xC`;
12 at counter 3 = `0x28`, `Field_ChangeArea(0x88, ...)` - sub-kind 0 to
(`0x330000`, `0x470000`) with flags `0x81`, any other to (`0x80000`,
`0x220000`) with `0x82` - state `0xD`; 13 at counter 3 = 0, disarm and the
row flag `0x2E` cleared; 3..9 nothing (map entry 7, a `ret`). So choices 0 and
1 pick which of two destinations of area `0x88` the tail leaves for.

### Area 139 (descriptor `0x62ED78`; PSX `0x801F45A0`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41F510` | `Area139_FlagIfLeader89Is5` | `0x3E` | handler 0 (PSX `0x801F42F4`) | kHandler | `Field_ActiveMember +0x80` bit 0 cleared; the leader's `+0x89` 5: story flag `0x4D`, `MoveCmd_TestFB(4, 0xD)`, `Sprite_Current +0` = 0 |
| `0x41F550` | `Area139_CellHook` | `0x63` | `Area_CellHooks`, area `0x8B` | kHook | the first of `Area139_CellEntries` (2 of 4 bytes) whose x, z bytes are the arguments' low bytes and whose direction nibble is the leader's direction byte: `Flags_Toggle(story, its flag)`, sound `0x204`, al 1; none: al 0 |

### Area 140 (descriptor `0x62EF98`; PSX `0x801F4A28`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41F5C0` | `Area140_ChoiceArmTail41` | `0x34` | choice 0 = handler 2 (PSX `0x801F3D4C`) | kChoice | answer 0: message 4; 1: tail kind 41 armed at state 0, message `0xFFFF`; else message `0xFFFF` |
| `0x41F600` | `Area140_FlagIfLeader89Is567` | `0x46` | handler 0 (PSX `0x801F3DAC`) | kHandler | as area 139's handler with `+0x89` 5, 6 or 7, flag `0x54`, `MoveCmd_TestFB(2, 7)` |
| `0x41F650` | `Area140_FollowAndAct` | `0x132` | handler 1 (PSX `0x801F3E30`) | kHandler | story flag `0x66` clear: script + 4. Set: the running object takes the leader's direction; the cell ahead of it (`0x66971C` steps); with the leader stepping and its `+0x137` 1, unless `Area140_BlockedAhead` a move inside x `0x44..0x50`, z `6..0x10`, else `Sprite_FaceDirection`; then with the action button (`0x90358C`) in `Input_Pressed` the script object's bit `0x40`, `+7` bit 8, an animation and `+0x2A` from `Area140_ActPoses` by quadrant, and `Area140_CellHook` on the cell ahead; without, script + 4 |
| `0x41F790` | `Area140_BlockedAhead` | `0x2E` | called by handler 1 only | kCallee | `Field_ObjectBlockedAhead(Field_ActiveMember)` 0: al 0; else al 1 unless the map byte at (x, z) is `0x89` |
| `0x41F7C0` | `Area140_Tail41` | `0x1A0` | tail kind 41 | kTail | twelve states (below) |
| `0x41F960` | `Area140_StepHook` | `0x42` | `Area_StepHook`'s case for area `0x8C` | kHook | `Cond_ByteFD` 0, story flag `0x66`, x exactly `0x558000`, z's high word `0xA` or `0xB`: `ScriptFlags_Set40`, state 5 (the kind left: tail 41 idles in state 2); al 0 always |
| `0x41F9B0` | `Area140_CellHook` | `0x1AE` | `Area_CellHooks`, area `0x8C`; handler 1 | kHook | the matching one of `Area140_CellEntries` (8 of 6 bytes); its flag set: al 0; its rectangle (`Area140_CellRects[+5]`) holding any member's point one timed step on (`+0xC` / `+0x10` times `+9`, plus `+0x34` / `+0x38`): tail 41 armed at state `0xA`, al 0; else the flag set, `MoveCmd_TestFB` at the cell, with `+4` an effect of kind `0x71` at the cell holding the flag, sound `0x204`, al 1 |
| `0x41FB60` | `Area140_Trigger18` | `0xF` | object trigger 18 (a gap of the tool) | kCallee | `ScriptFlags_Set40`, state 5; al 0 |
| `0x41FB70` | `Area140_Effect71Run` | `0x12` | `Effect_KindHandlers[0x71]` (`0x655514`; a gap) | kCallee | `Area140_Effect71States` by `Sprite_Current[1]` (the running effect record) |
| `0x41FB90` | `Area140_Effect71Start` | `0x14` | `Area140_Effect71States[0]` | kState | `+9` = `0xD2`, `+1` = 1 |
| `0x41FBB0` | `Area140_Effect71Count` | `0x30` | `Area140_Effect71States[1]` | kState | `+9` - 1; at 0, or `Field_Request` 5: `+1` = 2 |
| `0x41FBE0` | `Area140_Effect71End` | `0x2F` | `Area140_Effect71States[2]` | kState | the flag in `+0xB` cleared, `MoveCmd_TestFB` at the record's cell, `Effect_Release` (a tail `jmp`) |

Tail 41's states (jump table `0x41F930`): 0 `ScriptFlags_Clear40`,
`Party_DropIn(0)`, `Field_ScriptFlags | 0x1270`, the leader's `+0x48` 1,
story flag `0x66`, state 1; 1 `Camera_Distance` up by `0x40` to `0x680`, then
state 2 (`MapView_Redraw` 2 each frame); 2 while `Field_Request` is 0 the word
`0x903850` = `Input_Held` and `Input_Held`'s high nibble replaced through
`Area140_ButtonRemap` (the pad's four high bits remapped for as long as the
tail sits here); 5 counter 3 = 1, state 6; 6 `Camera_Distance` down by `0x40`
to 0, then state 7 (`ScriptFlags_Set40`, redraw); 7 `ScriptFlags_Clear40`,
`Field_ScriptFlags & 0xED8F`, `+0x48` 0, flag `0x66` cleared,
`Field_ZoneCounterRoll(0)`, disarmed; 10 message 1, `Field_Request` 2, state
`0xB`; 11 once the message is gone, disarmed; 3, 4, 8, 9 nothing. The step
hook, trigger 18 and the cell hook's rectangle case move it to 5 or `0xA`;
the choice and state 0 arm it.

### Area 141 (descriptor `0x62F5A0`; PSX `0x801F4D50`)

Six choices (choice `2 + n` is handler `n`; choice 1 is `0x403050`, world
0's), four handlers, two state tables, the init, tail kind 52, the cell hook,
trigger 57, the cell-run painter, and five placers chapter 14's code calls.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41FC10` | `Area141_ShadeOff` | `0x3F` | choice 2 = handler 0 (PSX `0x801F3588`) | kHandler | `Sprite_Current +0` bit `0x20` cleared, `+0x5F..+0x5C` 0, `Sprite_ReleaseTint` |
| `0x41FC50` | `Area141_MoveToX45` | `0x57` | choice 3 = handler 1 (PSX `0x801F35E4`) | kHandler | d = (x - `0x450000`) `sar` 15; not 0: the script object's `+7` = \|d\|, direction 7, `MoveCmd_MoveKind2` for `Sprite_Kind2`, else `MoveCmd_Move` |
| `0x41FCB0` | `Area141_RunShadeDown` | `0x12` | choice 4 = handler 2 (PSX `0x801F3688`) | kHandler | `Area141_ShadeDownStates` by `+4` (four code entries reachable, section 6) |
| `0x41FCD0` | `Area141_ShadeDownStart` | `0x42` | `Area141_ShadeDownStates[0]` | kState | bit `0x20`, `+0x5F..+0x5D` `0xC4`, `+0x5C` 1, the step, `+4` 1 |
| `0x41FD20` | `Area141_ShadeDownStep` | `0x89` | `Area141_ShadeDownStates[1]`; called by the start | kState | x, z by `+0xC` / `+0x10`; each shade byte not `0x80` less 2; all at `0x80`: bit `0x40`, `+4` 0; else script - 2 |
| `0x41FDB0` | `Area141_RunShadeUp` | `0x12` | choice 5 = handler 3 (PSX `0x801F3838`) | kHandler | `Area141_ShadeUpStates` by `+4` |
| `0x41FDD0` | `Area141_ShadeUpStart` | `0x4E` | `Area141_ShadeUpStates[0]` | kState | bit `0x20` set, `0x40` cleared, `+0x5C` 1, shades `0x80`, the step, `+4` 1 |
| `0x41FE20` | `Area141_ShadeUpStep` | `0xB2` | `Area141_ShadeUpStates[1]`; called by the start | kState | x, z moved; each shade below `0xC0` (signed) plus 4; all `0xC0`: bit `0x20`, `+0x5C..+0x5F` and `+4` 0; else the active member's word `+0x8A` - 2 |
| `0x41FEE0` | `Area141_ChoiceFlag7D` | `0x55` | choice 0 | kChoice | story flag `0x7D`; answer (read after) 0: `Kind2_Place(3)`, run 8 off, tail kind 52 at `0x32`, message 5; else message 6 |
| `0x41FF40` | `Area141_InitCells` | `0x111` | init (PSX `0x801F3B04`) | kInit | by `Cond_ByteFD` (read three times): 0 runs 0, 1 on; 1 runs 2..7 in pairs, on while flags `0x7A` / `0x7B` / `0x7C` are clear; 2 run 8 on |
| `0x420060` | `Area141_Tail52` | `0x2FC` | tail kind 52 | kTail | 52 states through a byte map and a fifteen-entry jump table (below) |
| `0x420360` | `Area141_CellHook` | `0x5C` | `Area_CellHooks`, area `0x8D` | kHook | the matching one of `Area141_CellEntries` (2 of 4 bytes): tail 52 at the entry's state, al 1; none al 0 |
| `0x4203C0` | `Area141_Trigger57` | `0x69` | object trigger 57 (a gap) | kCallee | by the object's word `+0x88`: `0xC000` runs 0, 1 off; `0xC001` `Party_HealJoined`, tail 52 at 0; `0xC002` tail 52 at `0xA`; al 0 |
| `0x420430` | `Area141_PaintCells` | `0x9A` | called by the choice, init, tail, trigger | kCallee | `(run, on)`: `run[2] & 0x7F` cells from (`run[0]`, `run[1]`) along x (bit `0x80`) or z, each `run[3]` when `on`'s low byte is not 0, else `run[4]` |
| `0x4204D0` | `Area141_PlacePairAnimated` | `0xAA` | chapter 14's `0x567145` (a gap) | kCallee | two free objects (the first let go if there is no second), `EventOp_6x` on scripts `0x62F630` / `0x62F640` with `0x903850` the index, animations `0x6B`, `0x58` |
| `0x420580` | `Area141_PlaceOneAnimated` | `0x4A` | `0x567179` | kCallee | one object, `EventOp_6x` on `0x62F650`, animation 3 |
| `0x4205D0` | `Area141_PlacePair6x` | `0x94` | `0x5648B2` | kCallee | two objects, `EventOp_6x` on `0x62F660` / `0x62F670` |
| `0x420670` | `Area141_PlacePair0x` | `0x94` | `0x5671AC` | kCallee | two objects, `EventOp_0x` on `0x62F680` / `0x62F691` |
| `0x420710` | `Area141_PlaceOne0x` | `0x41` | `0x5671E0` | kCallee | one object, `EventOp_0x` on `0x62F6A8` |

Tail 52's states (byte map `0x420328` of `0x34`, jump table `0x4202EC` of
15): 0 once `Field_Request` is not 2, `Party_DropIn(5)` and disarmed; `0xA`
message 3, `Field_ScriptFlags | 7`, the timer 0, `0xB`; `0xB` waits for the
message, `0xC`; `0xC` the word timer + 1 to `0x1C2` (then `0xD`) or a button
pressed (then `0x14`); `0xD` message 4, `Kind2_Place(2)`, `0xE`; `0xE` at
counter 0 = 1 flag `0x7A`, runs 2 and 3 off, a timer of `0x3C`, `0xF`; `0xF`
the timer out: `Field_Kind2X` / `Z` = the leader's position, `0x10`; `0x10`
once `Field_Kind2Hold` is 0, the message box's flag bit `0x80`, `0x14`;
`0x14` disarmed with `Field_ScriptFlags & 0xFFF8`; `0x1E` / `0x28` flag
`0x7B` / `0x7C` (already set: `0x14`), runs 4, 5 / 6, 7 off, the timer, `0x1F`
/ `0x29`; `0x1F`, `0x29` the timer out: `0x14`; `0x32` at counter 0 = 1
`Cond_ByteFE` 1, the timer, `0x33`; `0x33` the timer out: disarmed, counter
0 + 1. Every other state value nothing.

### Area 142 (descriptor `0x62FC98`; PSX `0x801F3B38`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x420760` | `Area142_RunWait` | `0x12` | handler 0 (PSX `0x801F3414`) | kHandler | `Area142_States` by `Sprite_Current[4]` |
| `0x420780` | `Area142_WaitRequest3` | `0x34` | `Area142_States[0]` | kState | counter 0 at 1: nothing; else `Field_Request` 3: bit `0x40`, `+4` 1; script - 2 |
| `0x4207C0` | `Area142_WaitRequestEnd` | `0x3C` | `Area142_States[1]` | kState | `Field_Request` not 3: bit `0x40` cleared, `Sprite_SetAnimation(+8)`, `+4` 0 (read again); script - 2 |

Area 142's descriptor `+0x44` holds `0x420780`, the address of its state 0;
the other areas' `+0x44` hold small values (`0xBFFFF`, `0x70008`), so this is
read as a coincidence of the value, not a pointer (nothing here reads `+0x44`).

## 2. Ours

`src/game/area_w3e.cpp`, calling out only through the harness (`AH_CALL`
for every callee; the band calls no address nobody owns). The group's own
callees are called the same way (`Area140_BlockedAhead`,
`Area140_CellHook`, the two shade steps, `Area141_PaintCells`), so each
function is fuzzed alone; the four dispatchers read their `.data` tables in
place and call the entry. Shapes that repeat are one helper: the three cell
hooks' search (`FindCell`), the five spawns by character, the two cell rows,
choices 0/1 and 2/3, the tail arming, the shade drift, tail 52's flag states
and timer, the placers' free-object pairs. Kept as the originals: every
re-read of `Sprite_Current`, `MoveScript_Object` and `Cond_ByteFD` after a
call, the answer read after the flag in `Area141_ChoiceFlag7D`, the order of
every call and store (`Field_Request` stored between the message and the
toggle in tail 19's state 10), the 16-bit compares, the signed shade compare,
the signed tail states (a negative state does nothing), `on` read as a byte.

## 3. The fuzz

`BOF3X_SHADOW=area_w3e` (`src/game/area_w3e_fuzz.cpp`): five `Run`s under the
one shadow name, one per area with its `Group::area` (136, 139, 140, 141,
142), 6,000 rounds per function, the real descriptors and tables in place.
The five placers run under area 141 (their block's).

- **Callees the group lists:** `AreaMap_SetByte` (words x, z, a value byte:
  the painter pushes the high halves of other registers), `AreaMap_ByteAt`
  (`0x89` a third of the time); `Effect_Spawn` (Capcom's) and
  `Effect_FindFree` `kByte 0xFF..0x03`; `Sprite_FindFree` `kByte 0xFF..0x1D`;
  `EventOp_6x` (Capcom's), `EventOp_0x`; `Flags_Toggle`; `Flags_Test` (`kBool`);
  `MoveCmd_TestFB`, `KeyItem_Has`, `Kind2_Place`, `Party_HealJoined`,
  `Field_ZoneCounterRoll`, `Effect_Release`, `ScriptFlags_Set40` /
  `Clear40`, `MoveCmd_Move` (Capcom's), `MoveCmd_MoveKind2`,
  `Sprite_FaceDirection`, `Sprite_SetAnimation`; the group's own
  `Area140_BlockedAhead` (`kFlag`, words), `Area140_CellHook` (bytes, the
  answer unread), the two shade steps (`kPhase`), `Area141_PaintCells`.
- **Louder stand-ins** (half the time, from `Noise`): `Effect_Spawn`,
  `MoveCmd_MoveKind2`, `Sprite_FaceDirection` and `Flags_Test` move
  `Sprite_Current`; `MoveCmd_Move` and `Sprite_SetAnimation` move the script
  object and `Sprite_Current` (the callers read both again).
- **Data tables** swapped for recorders: `Area140_Effect71States` (3),
  `Area141_ShadeDownStates` with the shade-up table after it (4, one run of
  code entries), `Area142_States` (2).
- **Regions beyond the field frame:** all twenty `Effect_Objects` records,
  the active member, script object and flag row pointers, `Camera_ShiftY`,
  `Input_Held` / `Input_Pressed`, the button word `0x90358C`, the tile word
  `0x939A00`, `Field_Kind2Hold`, `Cond_ByteFE`, `Sprite_Kind2` (31 regions,
  18,960 bytes).
- **Seeds:** the answer at every value a choice tests, its neighbours and the
  sign edge; area 136's z ahead on, beside and far from the limit of sub-kind
  0 or 1; party slots 1 and 2 a character 0..11; the tile's low byte at
  `0x61, 0x62, 0x64, 0x66, 0x67`; tail 19's every state, its neighbours and
  negatives with `Field_Request`, counter 3 at `0x28` / 0 and beside, sub-kind
  0, 1 or other; the init's zone and `Cond_ByteFD` at each tested value; the
  leader's `+0x89` at each tested value and beside; each cell hook called on
  one of its entries' cells with the leader's direction its nibble, one field
  off (x, z, a high nibble on the direction) a quarter of the time; area 140's
  rectangles with a member's timed step landing on, beside or away from them;
  handler 1's cell ahead on and beside the rectangle's edges for each
  direction, the leader stepping or not, `+0x137` 1 or not, the action button
  in `Input_Pressed` half the time; tail 41's states with `Camera_Distance`
  at 0, `0x40`, `0x640`, `0x680`, `0x6C0`; the step hook's x at and beside
  `0x558000`, z's high word `9..0xC` and with a high byte; the effect record as
  `Sprite_Current` with its state 0..2, its frame count 0, 1, 2; x around
  `0x450000` on each side of a `sar 15` boundary with `Sprite_Current` at
  `Sprite_Kind2` half the time; the shade bytes at and beside `0x80` / `0xC0`;
  tail 52's every state, neighbours and negatives with the timer at 0, 1, 2,
  `0x1C0..0x1C2`, `0xFFFF`, a button or none, counter 0 at 1 or not,
  `Field_Kind2Hold`; trigger 57's word at `0xC000..0xC003` and beside; the
  painter on each of the nine runs and on a run of any count and direction,
  `on` 0, 1, `0x100` (a high byte over 0), anything; area 142's counter 0 and
  `Field_Request` at 3 and beside.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 0, counter 3, the timer, the two pointers, the answer, the
  sub-kind, `Input_Pressed`.

**Result (in this worktree):** 312,000 rounds over the 52 functions (6,000
each), 387,598 calls to the stand-ins, 0 mismatches, 18,960 bytes of state
(31 regions) and the log compared. Coverage: every callee each function can
reach was called - e.g. `Effect_Spawn` 30,000, `Field_ChangeArea` 85 (tail
19's state 12 at counter 3 `0x28`), `Flags_Toggle` 329 / 1,991 (tail 19,
area 139's hook), `MoveCmd_Move` 56 (area 140's handler 1 inside its
rectangle with nothing blocking), `Effect_FindFree` 135 (area 140's cell hook
past its flag and rectangle with `+4` set), `Field_ZoneCounterRoll` 236,
`Party_HealJoined` 916, `MoveCmd_MoveKind2` 2,240, the effect states
1,939 / 2,083 / 1,978, area 142's states 2,962 / 3,038.

`BOF3X_SHADOW='*'`: STAR_RESULT.

## 4. Controls

CONTROLS_TEXT

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (16): `Area136_ChoiceMessages2`
`0x62EA74` (2 words), `Area136_ChoiceMessages3` `0x62EA78` (2 words),
`Area136_ZLimits` `0x62EA7C` (bytes by sub-kind), `Area136_EffectByCharA`
`0x62DAA0` and `Area136_EffectByCharB` `0x62DAAC` (bytes by character, in
`.data` before area 136's descriptor), `Area139_CellEntries` `0x62EDBC` (2 x
4), `Area140_ActPoses` `0x62EFDC` (4 pairs), `Area140_ButtonRemap` `0x62EFE4`
(16), `Area140_CellEntries` `0x62EFF8` (8 x 6), `Area140_CellRects`
`0x62F028` (4 word pairs), `Area140_Effect71States` `0x62F038` (3),
`Area141_CellRuns` `0x62F5E8` (9 x 5), `Area141_ShadeDownStates` `0x62F618`
(2), `Area141_ShadeUpStates` `0x62F620` (2), `Area141_CellEntries` `0x62F628`
(2 x 4), `Area142_States` `0x62FCDC` (2). The five event scripts the placers
hand on (`0x62F630`..`0x62F6A8`) are named by address in
`area_w3e_callees.h` only; the tail jump tables and byte maps are in `.text`,
inside their functions' extents.

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area140_Effect71Run` jumps through a
  three-entry table by the effect record's `+1` (a zero dword follows: index
  3 jumps to 0), `Area142_RunWait` through two by `+4` (a zero follows),
  `Area141_RunShadeUp` through two (area 141's cell entries follow, read as an
  address). `Area141_RunShadeDown`'s table is followed by the shade-up table,
  so its indices 2 and 3 run `Area141_ShadeUpStart` / `Step` - code, kept
  faithfully - and 4 and on read the cell entries. Ours aborts with a message
  past the code each table reaches (the owner's rule: no DIVERGENCE entry,
  round9 doc section 6). The band's own states store only 0 and 1 in `+4`
  (both shade machines share the byte) and 0..2 in the effect's `+1`; a
  larger index needs another writer, none read this round.
- **Unchecked reads that stay in `.data`, kept:** area 136's message words by
  the signed answer; `Area136_ZLimits` by the sub-kind byte; the two effect
  tables by a character byte; the cell steps `0x66971C` by the direction byte
  times 2; `Area140_CellRects` by the entry's rectangle byte.
- **`Area136_SpawnLeaderKind1` reads party slot 1's character** for the
  leader's record where its siblings read the slot of the record they use;
  the leader's own slot is `0x904062`. Kept as read; whether Capcom meant
  slot 0 is not known (the PSX twin `0x801F30C8` was not compared).
- **Effect and object slots unchecked**, as in [`area_w2d.md`](area_w2d.md):
  every spawn writes the record at `Effect_Objects + slot << 7`, the placers
  `Sprite_Objects + slot * 0xA4`, for any slot but `0xFF`; the finders answer
  only valid slots or `0xFF`.
- **Tail 41 remaps the pad while it idles in state 2**: every frame with
  `Field_Request` 0, `Input_Held`'s four high bits go through
  `Area140_ButtonRemap` (the unmapped word kept in `0x903850`). Behaviour, not
  a defect; noted because any divergence touching input would meet it.
- **Invisible, noted:** `Area140_CellHook` reuses its argument slots on the
  caller's stack for the member count and a predicted x (no caller reads them
  back); `Area140_StepHook` reads z's high word as a dword at `[esp + 0xA]`
  that runs into the caller's frame (only the low word used);
  `Area140_FollowAndAct` passes x and z in registers whose high halves are the
  caller's (both callees read only the low word or byte);
  `Area136_Counter0ByTile` stores counter 0 twice for a tile above `0x66`.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 52 starts, extents, call sites, the
  three in-function jump tables (`0x41F340`: 8 entries at `+0x170` with a
  14-byte map at `+0x190`; `0x41F7C0`: 12 at `+0x170`; `0x420060`: 15 at
  `+0x28C` with a `0x34`-byte map at `+0x2C8`), and the shape its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's functions
  is padding.
- **Gaps of the tool**, each read to its root: `0x41FB60`, `0x4203C0`
  (object triggers 18 and 57), `0x41FB70` (`Effect_KindHandlers[0x71]`, the
  kind area 140's cell hook spawns), and the five placers `0x4204D0`..
  `0x420710` (called from chapter 14's code). `0x41F790` and `0x420430` have no
  root of their own (called by area 140's handler 1; by area 141's code).
- **The tool counts 4 code entries at `0x62F618`**: the two shade tables sit
  back to back; its note is right that the shade-down dispatcher can reach
  all four (section 6).
- **Register-armed tail kinds:** none in this band; kinds 19, 41 and 52 are
  armed by immediates (tail 52's state from a table byte in the cell hook).

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 52). Every function is reached only in play: the choices when
  the area's message box asks, the handlers from its movement scripts, the
  hooks on a step or a talk at a cell, the tails once armed, the triggers by
  an object's trigger id, the inits on entry, effect kind `0x71` while one
  lives, and the placers from chapter 14's runs.

## 9. Calls across groups

- **Raw addresses nobody owns:** none. Every callee is named: Capcom's
  `Effect_Spawn`, `EventOp_6x`, `MoveCmd_Move`; ours `AreaMap_SetByte`,
  `AreaMap_ByteAt`, `Effect_FindFree`, `Effect_Release`, `Sprite_FindFree`,
  `EventOp_0x`, `Flags_Set` / `Clear` / `Test` / `Toggle`,
  `MoveCmd_TestFB`, `MoveCmd_MoveKind2`, `KeyItem_Has`, `Kind2_Place`,
  `Party_HealJoined`, `Party_DropIn`, `Field_ZoneCounterRoll`,
  `Field_ChangeArea`, `Field_ObjectBlockedAhead`, `Msg_OpenScript`,
  `Sound_PlayEffect`, `Sprite_FaceDirection`, `Sprite_SetAnimation`,
  `Sprite_SetAnimationAt`, `Sprite_ReleaseTint`, `ScriptFlags_Set40` /
  `Clear40`. No harness edit; no `AH_THEIRS` moved.
- **Inbound** (for the rebinding pass): chapter 14's code (group SC13) calls
  the five placers by raw address - `Scena14_EnterArea`'s `0x5648B2` into
  `Area141_PlacePair6x`, `Scena14_Run7`'s `0x567145`, `0x567179`,
  `0x5671AC`, `0x5671E0` into `Area141_PlacePairAnimated`,
  `Area141_PlaceOneAnimated`, `Area141_PlacePair0x`, `Area141_PlaceOne0x`
  (`scena_sc13_callees.h` `kArea141a..e`, SC13's fuzz stands recorders in).
  `Area_StepHook` (`event_ops.cpp` `kStepHandlers`) calls `0x41F960`, now
  `Area140_StepHook`. Read in place, no caller to rebind:
  `Area_CellHooks`' pairs for areas `0x8B`, `0x8C`, `0x8D`;
  `Field_ModeTailKinds` 19, 41, 52; `Field_ObjectTriggers` 18, 57;
  `Effect_KindHandlers[0x71]`; areas 119, 131 and 188's tables name
  `Area136_ShiftCameraUp2`.
- `analysis/calltrace/entries_logic.txt`: 47 lines appended under a
  `# group AR3E` comment (the other 5 were there with the same extent); every
  function also lies inside the host line `0041EFA0 7E2` or a larger
  same-start line (`0041F790 212`, `0041F9B0 362`, `0041FD20 FE`, `0041FE20
  609`, `00420710 2F9`), which the consolidation replaces by the smaller.
