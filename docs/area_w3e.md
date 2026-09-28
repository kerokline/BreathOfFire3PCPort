# World 3, areas 136 and 139..142: the band `0x41EFE0..0x420800`

**Status:** IN PROGRESS (2026-09-28) - 52 functions ours
(`src/game/area_w3e.cpp`, shadow name `area_w3e`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 312,000 rounds (in this worktree); 343 controls planted, 342 refused by a count, 1 equivalent with its variant refused (section 4).
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
  `0x939A00`, `Field_Kind2Hold`, `Cond_ByteFE`, `Sprite_Kind2`, and the record a spawn slot of `0xFF` writes (`0x7E9160`) (32 regions,
  19,088 bytes).
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
each), 385,635 calls to the stand-ins, 0 mismatches, 19,088 bytes of state
(32 regions) and the log compared. Coverage: every callee each function can
reach was called - e.g. `Effect_Spawn` 30,000, `Field_ChangeArea` 79 (tail
19's state 12 at counter 3 `0x28`), `Flags_Toggle` 302 / 2,037 (tail 19,
area 139's hook), `MoveCmd_Move` 109 (area 140's handler 1 inside its
rectangle with nothing blocking), `Effect_FindFree` 120 (area 140's cell hook
past its flag and rectangle with `+4` set), `Field_ZoneCounterRoll` 285,
`Party_HealJoined` 863, `MoveCmd_MoveKind2` 2,192, the effect states
2,096 / 1,988 / 1,916, area 142's states 3,046 / 2,954.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4988 ours` (the `impl` count is 4,989: the off-by-one the round doc section 10 notes), 453 self-test lines, no mismatch (first run; it did not die silently).

## 4. Controls

Planted one at a time in `area_w3e.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w3e.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w3e`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all five runs). **343 planted, 342 refused by a count (exit 3), 1 equivalent** (R11: `Area140_Effect71Count` stores state 2 on either test, so skipping the frame test when the request is 5 changes nothing; its near variant R11b, keyed on request 4, refused), no hang, no fault. Every one of the 52 functions has at least one control of its own; a control in a helper shared across functions (`FindCell`, `SpawnByCharacter`, `CellsRow59`, `ChoiceTail19`, `ChoiceMessage`, `ArmTail`, `DriftStep`, `TimerOut`, `FlagRunsOff`, `TwoFree`, `PlacePair`, `PlaceOn`) is refused in each function that uses it within the first area run that meets it.

**The first run** (against the fuzz of commit `3af7e6a`) left five standing, all the fuzz's fault and all refused after strengthening it: F20 (`Area140_BlockedAhead`'s stand-in answered any non-zero byte, so `<= 1` looked like `== 0` too rarely - it now answers 0 or 1, as the function does: refused in 114 rounds); E15 (a spawn slot of `0xFF` wrote past the regions - `Effect_Objects + 0xFF << 7` is now a region: 22); U24 (`Field_Kind2Hold` seeded 0 or any, never 1: 41); Y5 (the event ops' object word `0x903850` was not logged per call - the ops' stand-ins now note it: 5,598..5,623); R11 (equivalent, above). An infrastructure slip in the first run (the symbols generator was fed a doubled block by a script, eleven controls failed to build) was caught, the tree restored, and those controls re-run.

The thinnest: F10 (2 rounds; `MoveCmd_Move` given the direction read before `Area140_BlockedAhead`, which differs only when that stand-in moved `Sprite_Current` and the rectangle test still passes), N9 (6; tail 19's request stored after the toggle, seen only when the toggle's stand-in disturbance moves the request), Q9 (11; the choice's answer read before the flag call), E5 / F8 (13), N12 (16); the rest need 19 rounds or more.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| H1 | `FindCell (139, 140, 141)` | pose masked to a nibble | Area139_CellHook 62 |
| H2 | `FindCell (139, 140, 141)` | x + 1 | Area139_CellHook 2037 |
| H3 | `FindCell (139, 140, 141)` | last entry not searched | Area139_CellHook 1029 |
| H3b | `FindCell (139, 140, 141)` | z high byte read | Area139_CellHook 2028 |
| H4 | `SpawnByCharacter (136 x5)` | x word +0x2C | Area136_SpawnMember1Kind2 6000, Area136_SpawnMember2Kind2 6000, Area136_SpawnMember1Kind1 5999 |
| H5 | `SpawnByCharacter (136 x5)` | slot stored without re-reading Sprite_Current | Area136_SpawnMember1Kind2 2084, Area136_SpawnMember2Kind2 2071, Area136_SpawnMember1Kind1 2099 |
| H6 | `SpawnByCharacter (136 x5)` | none 0xFE | Area136_SpawnMember1Kind2 1218, Area136_SpawnMember2Kind2 1194, Area136_SpawnMember1Kind1 1197 |
| H7 | `SpawnByCharacter (136 x5)` | table + 1 | Area136_SpawnMember1Kind2 4476, Area136_SpawnMember2Kind2 4499, Area136_SpawnMember1Kind1 4497 |
| H8 | `SpawnByCharacter (136 x5)` | z word +0x32 | Area136_SpawnMember1Kind2 5999, Area136_SpawnMember2Kind2 6000, Area136_SpawnMember1Kind1 6000 |
| H9 | `CellsRow59 (136 x2)` | middle value + 1 | Area136_CellsRow51 6000, Area136_CellsRow10 6000 |
| H10 | `CellsRow59 (136 x2)` | first z 0x58 | Area136_CellsRow51 6000, Area136_CellsRow10 6000 |
| H11 | `CellsRow59 (136 x2)` | last x 8 | Area136_CellsRow51 6000, Area136_CellsRow10 6000 |
| H12 | `ChoiceTail19 (136 x2)` | answer below 2 | Area136_ChoiceTail19Sub0 499, Area136_ChoiceTail19Sub1 491 |
| H13 | `ChoiceTail19 (136 x2)` | message 0xFFFE | Area136_ChoiceTail19Sub0 5953, Area136_ChoiceTail19Sub1 5956 |
| H14 | `ChoiceTail19 (136 x2)` | kind 0x14 | Area136_ChoiceTail19Sub0 1067, Area136_ChoiceTail19Sub1 1011 |
| H15 | `ChoiceTail19 (136 x2)` | sub-kind 0 always | Area136_ChoiceTail19Sub1 1011 |
| H16 | `ChoiceTail19 (136 x2)` | state 1 | Area136_ChoiceTail19Sub0 1067, Area136_ChoiceTail19Sub1 1011 |
| H17 | `ChoiceMessage (136 x2)` | answer unsigned | Area136_ChoiceMessage2 1989, Area136_ChoiceMessage3 1954 |
| H18 | `ChoiceMessage (136 x2)` | counter 0 = 7 | Area136_ChoiceMessage2 1026, Area136_ChoiceMessage3 978 |
| H19 | `ChoiceMessage (136 x2)` | word + 1 | Area136_ChoiceMessage2 5897, Area136_ChoiceMessage3 5889 |
| H20 | `Area136_ChoiceMessage3` | the table of choice 2 | Area136_ChoiceMessage3 3792 |
| H21 | `Area136_ChoiceTail19Sub1` | sub-kind 2 | Area136_ChoiceTail19Sub1 1011 |
| M1 | `Area136_MoveUntilLimit` | step x for z | Area136_MoveUntilLimit 1094 |
| M2 | `Area136_MoveUntilLimit` | >= for > | Area136_MoveUntilLimit 1313 |
| M3 | `Area136_MoveUntilLimit` | script +7 2 | Area136_MoveUntilLimit 2490 |
| M4 | `Area136_MoveUntilLimit` | move 4 | Area136_MoveUntilLimit 2490 |
| M5 | `Area136_MoveUntilLimit` | script - 1 | Area136_MoveUntilLimit 2490 |
| M6 | `Area136_MoveUntilLimit` | limit << 15 | Area136_MoveUntilLimit 2312 |
| M7 | `Area136_MoveUntilLimit` | signed compare | Area136_MoveUntilLimit 1224 |
| RW51 | `Area136_CellsRow51` | value 0x50 | Area136_CellsRow51 6000 |
| RW10 | `Area136_CellsRow10` | value 0x11 | Area136_CellsRow10 6000 |
| SP1 | `Area136_SpawnMember1Kind2` | kind 3 | Area136_SpawnMember1Kind2 6000 |
| SP2 | `Area136_SpawnMember2Kind2` | table B | Area136_SpawnMember2Kind2 2195 |
| SP3 | `Area136_SpawnMember2Kind2` | record 1 | Area136_SpawnMember2Kind2 6000 |
| SP4 | `Area136_SpawnMember1Kind1` | party slot 2 | Area136_SpawnMember1Kind1 4658 |
| SP5 | `Area136_SpawnMember1Kind3` | kind 4 | Area136_SpawnMember1Kind3 6000 |
| SP6 | `Area136_SpawnLeaderKind1` | party slot 0 | Area136_SpawnLeaderKind1 5155 |
| T1 | `Area136_Counter0ByTile` | above 0x62 | Area136_Counter0ByTile 716 |
| T2 | `Area136_Counter0ByTile` | below 0x66 | Area136_Counter0ByTile 654 |
| T3 | `Area136_Counter0ByTile` | else 0x1F | Area136_Counter0ByTile 3966 |
| T4 | `Area136_Counter0ByTile` | nine bits | Area136_Counter0ByTile 1037 |
| T5 | `Area136_Counter0ByTile` | 0xB inside | Area136_Counter0ByTile 2034 |
| C1 | `Area136_ShiftCameraUp2` | + 3 | Area136_ShiftCameraUp2 6000 |
| C2 | `Area136_ShiftCameraUp2` | redraw 1 | Area136_ShiftCameraUp2 6000 |
| N1 | `Area136_Tail19` | not held: 2 | Area136_Tail19 127 |
| N2 | `Area136_Tail19` | key item 7 | Area136_Tail19 403 |
| N3 | `Area136_Tail19` | message 0x84 | Area136_Tail19 353 |
| N4 | `Area136_Tail19` | state 3 after 1 | Area136_Tail19 353 |
| N5 | `Area136_Tail19` | sub-kind kept | Area136_Tail19 246 |
| N6 | `Area136_Tail19` | row flag 0x2F | Area136_Tail19 376 |
| N7 | `Area136_Tail19` | toggle 0x2A | Area136_Tail19 302 |
| N8 | `Area136_Tail19` | state 0xC after 10 | Area136_Tail19 302 |
| N9 | `Area136_Tail19` | request stored after the toggle | Area136_Tail19 6 |
| N10 | `Area136_Tail19` | drop-in 1 | Area136_Tail19 360 |
| N11 | `Area136_Tail19` | counter 3 0x29 | Area136_Tail19 106 |
| N12 | `Area136_Tail19` | x and z swapped | Area136_Tail19 16 |
| N13 | `Area136_Tail19` | sub-kind not 1 | Area136_Tail19 38 |
| N14 | `Area136_Tail19` | flags 0x83 | Area136_Tail19 63 |
| N15 | `Area136_Tail19` | row flag 0x2D cleared | Area136_Tail19 70 |
| N16 | `Area136_Tail19` | counter 3 0x28 ends too | Area136_Tail19 71 |
| N17 | `Area136_Tail19` | state 3 runs 2 | Area136_Tail19 150 |
| N18 | `Area136_Tail19` | state masked 0x7F | Area136_Tail19 433 |
| N19 | `Area136_Tail19` | state 0xE after 12 | Area136_Tail19 79 |
| N20 | `Area136_Tail19` | kind kept at the end | Area136_Tail19 70 |
| I1 | `Area136_InitExtraObject` | zone 2 | Area136_InitExtraObject 244 |
| I2 | `Area136_InitExtraObject` | value 3 | Area136_InitExtraObject 171 |
| I3 | `Area136_InitExtraObject` | FD 2 | Area136_InitExtraObject 185 |
| I4 | `Area136_InitExtraObject` | value 5 | Area136_InitExtraObject 183 |
| I5 | `Area136_InitExtraObject` | FD 5 | Area136_InitExtraObject 265 |
| A1 | `Area139_FlagIfLeader89Is5` | +0x89 6 | Area139_FlagIfLeader89Is5 2039 |
| A2 | `Area139_FlagIfLeader89Is5` | flag 0x4E | Area139_FlagIfLeader89Is5 1389 |
| A3 | `Area139_FlagIfLeader89Is5` | test z 0xE | Area139_FlagIfLeader89Is5 1389 |
| A4 | `Area139_FlagIfLeader89Is5` | +0 1 | Area139_FlagIfLeader89Is5 1389 |
| A5 | `Area139_FlagIfLeader89Is5` | bits 0, 1 cleared | Area139_FlagIfLeader89Is5 3024 |
| A6 | `Area139_CellHook` | the direction byte as the flag | Area139_CellHook 2037 |
| A7 | `Area139_CellHook` | sound 0x205 | Area139_CellHook 2037 |
| A8 | `Area139_CellHook` | none answers 1 | Area139_CellHook 3963 |
| A9 | `Area139_CellHook` | answers 2 | Area139_CellHook 2037 |
| A10 | `Area139_CellHook` | stride 5 | Area139_CellHook 1029 |
| B1 | `Area140_ChoiceArmTail41` | message 5 | Area140_ChoiceArmTail41 988 |
| B2 | `Area140_ChoiceArmTail41` | state 1 | Area140_ChoiceArmTail41 537 |
| B3 | `Area140_ChoiceArmTail41` | answer 1 or more | Area140_ChoiceArmTail41 4475 |
| B4 | `Area140_ChoiceArmTail41` | message 0xFFFE | Area140_ChoiceArmTail41 5012 |
| B5 | `ArmTail (140 x2, 141)` | kind + 1 | Area140_ChoiceArmTail41 537, Area140_CellHook 46 |
| B6 | `Area140_FlagIfLeader89Is567` | not 7 | Area140_FlagIfLeader89Is567 601 |
| B7 | `Area140_FlagIfLeader89Is567` | 4 for 5 | Area140_FlagIfLeader89Is567 1116 |
| B8 | `Area140_FlagIfLeader89Is567` | flag 0x55 | Area140_FlagIfLeader89Is567 1713 |
| B9 | `Area140_FlagIfLeader89Is567` | test z 8 | Area140_FlagIfLeader89Is567 1713 |
| B10 | `Area140_FlagIfLeader89Is567` | bit 1 cleared | Area140_FlagIfLeader89Is567 4478 |
| F1 | `Area140_FollowAndAct` | flag 0x67 | Area140_FollowAndAct 6000 |
| F2 | `Area140_FollowAndAct` | clear: script + 3 | Area140_FollowAndAct 1999 |
| F3 | `Area140_FollowAndAct` | direction & 7 | Area140_FollowAndAct 1264 |
| F4 | `Area140_FollowAndAct` | x by the z step | Area140_FollowAndAct 2060 |
| F5 | `Area140_FollowAndAct` | x span 0xC | Area140_FollowAndAct 40 |
| F6 | `Area140_FollowAndAct` | z span 0xC | Area140_FollowAndAct 21 |
| F7 | `Area140_FollowAndAct` | +0x137 not 0 | Area140_FollowAndAct 1599 |
| F8 | `Area140_FollowAndAct` | steps above 1 | Area140_FollowAndAct 13 |
| F9 | `Area140_FollowAndAct` | script +7 3 | Area140_FollowAndAct 109 |
| F10 | `Area140_FollowAndAct` | move by the direction read before | Area140_FollowAndAct 2 |
| F11 | `Area140_FollowAndAct` | face + 1 | Area140_FollowAndAct 3236 |
| F12 | `Area140_FollowAndAct` | quadrant >> 2 | Area140_FollowAndAct 2007 |
| F13 | `Area140_FollowAndAct` | Input_Held | Area140_FollowAndAct 1380 |
| F14 | `Area140_FollowAndAct` | script bit 0x20 | Area140_FollowAndAct 1995 |
| F15 | `Area140_FollowAndAct` | +7 bit 4 | Area140_FollowAndAct 2002 |
| F16 | `Area140_FollowAndAct` | animation from the second byte | Area140_FollowAndAct 2661 |
| F17 | `Area140_FollowAndAct` | +0x2B | Area140_FollowAndAct 2661 |
| F18 | `Area140_FollowAndAct` | hook (z, x) | Area140_FollowAndAct 2656 |
| F19 | `Area140_FollowAndAct` | not pressed: script + 5 | Area140_FollowAndAct 1340 |
| F20 | `Area140_FollowAndAct` | blocked test ignored | Area140_FollowAndAct 114 |
| F21 | `Area140_FollowAndAct` | z step + 1 | Area140_FollowAndAct 2925 |
| K1 | `Area140_BlockedAhead` | byte 0x88 | Area140_BlockedAhead 1311 |
| K2 | `Area140_BlockedAhead` | Field_State | Area140_BlockedAhead 5223 |
| K3 | `Area140_BlockedAhead` | answers 2 | Area140_BlockedAhead 2678 |
| K4 | `Area140_BlockedAhead` | (z, x) | Area140_BlockedAhead 3968 |
| L1 | `Area140_Tail41` | drop-in 1 | Area140_Tail41 255 |
| L2 | `Area140_Tail41` | | 0x1271 | Area140_Tail41 148 |
| L3 | `Area140_Tail41` | leader +0x48 2 | Area140_Tail41 255 |
| L4 | `Area140_Tail41` | flag 0x67 | Area140_Tail41 255 |
| L5 | `Area140_Tail41` | state 2 after 0 | Area140_Tail41 255 |
| L6 | `Area140_Tail41` | far 0x6C0 | Area140_Tail41 177 |
| L7 | `Area140_Tail41` | + 0x20 | Area140_Tail41 350 |
| L8 | `Area140_Tail41` | state 3 after 1 | Area140_Tail41 133 |
| L9 | `Area140_Tail41` | request 1 too | Area140_Tail41 20 |
| L10 | `Area140_Tail41` | saved + 1 | Area140_Tail41 266 |
| L11 | `Area140_Tail41` | remap << 11 | Area140_Tail41 127 |
| L12 | `Area140_Tail41` | low 11 bits kept | Area140_Tail41 138 |
| L13 | `Area140_Tail41` | counter 3 = 2 | Area140_Tail41 248 |
| L14 | `Area140_Tail41` | state 7 after 5 | Area140_Tail41 248 |
| L15 | `Area140_Tail41` | at or below 0 | Area140_Tail41 99 |
| L16 | `Area140_Tail41` | - 0x20 | Area140_Tail41 398 |
| L17 | `Area140_Tail41` | redraw 3 | Area140_Tail41 489 |
| L18 | `Area140_Tail41` | & 0xED8E | Area140_Tail41 151 |
| L19 | `Area140_Tail41` | leader +0x48 1 at the end | Area140_Tail41 285 |
| L20 | `Area140_Tail41` | clear 0x65 | Area140_Tail41 285 |
| L21 | `Area140_Tail41` | roll keep 1 | Area140_Tail41 285 |
| L22 | `Area140_Tail41` | message 2 | Area140_Tail41 268 |
| L23 | `Area140_Tail41` | state 0xC after 10 | Area140_Tail41 268 |
| L24 | `Area140_Tail41` | waits on request 3 | Area140_Tail41 38 |
| L25 | `Area140_Tail41` | state 3 runs 5 | Area140_Tail41 266 |
| L26 | `Area140_Tail41` | request 3 for 2 | Area140_Tail41 268 |
| S1 | `Area140_StepHook` | FD 1 too | Area140_StepHook 974 |
| S2 | `Area140_StepHook` | flag 0x67 | Area140_StepHook 1942 |
| S3 | `Area140_StepHook` | x high word only | Area140_StepHook 160 |
| S4 | `Area140_StepHook` | z span 3 | Area140_StepHook 38 |
| S5 | `Area140_StepHook` | state 6 | Area140_StepHook 162 |
| S6 | `Area140_StepHook` | answers 1 | Area140_StepHook 162 |
| S7 | `Area140_StepHook` | z >> 15 | Area140_StepHook 162 |
| S8 | `Area140_StepHook` | no Set40 | Area140_StepHook 162 |
| E1 | `Area140_CellHook` | the effect byte tested | Area140_CellHook 2046 |
| E2 | `Area140_CellHook` | rectangle from +4 | Area140_CellHook 27 |
| E3 | `Area140_CellHook` | last member skipped | Area140_CellHook 19 |
| E4 | `Area140_CellHook` | x by the z step | Area140_CellHook 33 |
| E5 | `Area140_CellHook` | z span 3 | Area140_CellHook 13 |
| E6 | `Area140_CellHook` | x span 1 | Area140_CellHook 30 |
| E7 | `Area140_CellHook` | state 0xB | Area140_CellHook 46 |
| E8 | `Area140_CellHook` | rectangle answers 1 | Area140_CellHook 46 |
| E9 | `Area140_CellHook` | flag + 1 set | Area140_CellHook 628 |
| E10 | `Area140_CellHook` | test (z, x) | Area140_CellHook 628 |
| E11 | `Area140_CellHook` | effect when +4 is 0 | Area140_CellHook 628 |
| E12 | `Area140_CellHook` | kind 0x72 | Area140_CellHook 98 |
| E13 | `Area140_CellHook` | x << 15 | Area140_CellHook 98 |
| E14 | `Area140_CellHook` | +0xB from +4 | Area140_CellHook 98 |
| E15 | `Area140_CellHook` | none 0xFE | Area140_CellHook 22 |
| E16 | `Area140_CellHook` | sound 0x205 | Area140_CellHook 628 |
| E17 | `Area140_CellHook` | rectangle x and z swapped | Area140_CellHook 46 |
| E18 | `Area140_CellHook` | rectangle index 1 skipped | Area140_CellHook 19 |
| E19 | `Area140_CellHook` | none answers 1 | Area140_CellHook 3954 |
| E20 | `Area140_CellHook` | effect +0 2 | Area140_CellHook 98 |
| E21 | `Area140_CellHook` | z << 17 | Area140_CellHook 98 |
| E22 | `Area140_CellHook` | answers 2 | Area140_CellHook 628 |
| G1 | `Area140_Trigger18` | state 6 | Area140_Trigger18 6000 |
| G2 | `Area140_Trigger18` | answers 1 | Area140_Trigger18 6000 |
| G3 | `Area140_Trigger18` | Clear40 | Area140_Trigger18 6000 |
| R1 | `Area140_Effect71Run` | state + 1 | Area140_Effect71Run 6000 |
| R2 | `Area140_Effect71Start` | frames 0xD3 | Area140_Effect71Start 6000 |
| R3 | `Area140_Effect71Start` | state 2 | Area140_Effect71Start 6000 |
| R4 | `Area140_Effect71Count` | - 2 | Area140_Effect71Count 6000 |
| R5 | `Area140_Effect71Count` | ends at 1 | Area140_Effect71Count 2072 |
| R6 | `Area140_Effect71Count` | request 4 | Area140_Effect71Count 1483 |
| R7 | `Area140_Effect71Count` | request: state 3 | Area140_Effect71Count 1030 |
| R8 | `Area140_Effect71End` | flag from +0xA | Area140_Effect71End 5982 |
| R9 | `Area140_Effect71End` | (z, x) | Area140_Effect71End 6000 |
| R10 | `Area140_Effect71End` | no release | Area140_Effect71End 6000 |
| R11 | `Area140_Effect71Count` | the frame test skipped when the request is 5 | not refused: equivalent (with the request at 5 both tests store 2, so skipping one changes nothing); variant R11b refused |
| R11b | `Area140_Effect71Count` | ends at 0 only when the request is not 4 (R11 variant) | Area140_Effect71Count 272 |
| P1 | `Area141_ShadeOff` | bit 0x10 | Area141_ShadeOff 4484 |
| P2 | `Area141_ShadeOff` | +0x5C 1 | Area141_ShadeOff 5999 |
| P3 | `Area141_ShadeOff` | release + 1 | Area141_ShadeOff 6000 |
| P3b | `Area141_ShadeOff` | +0x5E kept | Area141_ShadeOff 5983 |
| P4 | `Area141_MoveToX45` | >> 16 | Area141_MoveToX45 3411 |
| P5 | `Area141_MoveToX45` | 0x460000 | Area141_MoveToX45 5523 |
| P6 | `Area141_MoveToX45` | not absolute | Area141_MoveToX45 2495 |
| P7 | `Area141_MoveToX45` | direction 6 | Area141_MoveToX45 4443 |
| P8 | `Area141_MoveToX45` | kind 2 test inverted | Area141_MoveToX45 4443 |
| P9 | `Area141_MoveToX45` | kind 2 direction + 1 | Area141_MoveToX45 2192 |
| P9b | `Area141_MoveToX45` | logical shift | Area141_MoveToX45 2495 |
| P10 | `Area141_RunShadeDown` | state + 1 | Area141_RunShadeDown 6000 |
| P11 | `Area141_RunShadeUp` | state ^ 1 | Area141_RunShadeUp 6000 |
| P11b | `Area141_RunShadeUp` | the shade-down table | Area141_RunShadeUp 6000 |
| P12 | `Area141_ShadeDownStart` | 0xC5 | Area141_ShadeDownStart 5999 |
| P13 | `Area141_ShadeDownStart` | +0x5C 2 | Area141_ShadeDownStart 6000 |
| P14 | `Area141_ShadeDownStart` | bit 0x10 | Area141_ShadeDownStart 4528 |
| P15 | `Area141_ShadeDownStart` | state 2 | Area141_ShadeDownStart 6000 |
| P15b | `Area141_ShadeDownStart` | no step | Area141_ShadeDownStart 6000 |
| P16 | `Area141_ShadeDownStep` | - 1 | Area141_ShadeDownStep 5943 |
| P17 | `Area141_ShadeDownStep` | stops at 0x81 | Area141_ShadeDownStep 4217 |
| P18 | `Area141_ShadeDownStep` | bit 0x20 | Area141_ShadeDownStep 159 |
| P19 | `Area141_ShadeDownStep` | state 1 at the end | Area141_ShadeDownStep 204 |
| P20 | `Area141_ShadeDownStep` | script - 3 | Area141_ShadeDownStep 5796 |
| P21 | `DriftStep (141 x2)` | x by +0x10 | Area141_ShadeDownStep 6000, Area141_ShadeUpStep 6000 |
| P22 | `DriftStep (141 x2)` | z by +0x14 | Area141_ShadeDownStep 6000, Area141_ShadeUpStep 6000 |
| P23 | `Area141_ShadeDownStep` | +0x5E not tested | Area141_ShadeDownStep 415 |
| P24 | `Area141_ShadeUpStart` | & 0x3F | Area141_ShadeUpStart 2979 |
| P25 | `Area141_ShadeUpStart` | 0x81 | Area141_ShadeUpStart 6000 |
| P26 | `Area141_ShadeUpStart` | +0x5D 0x7F | Area141_ShadeUpStart 6000 |
| P27 | `Area141_ShadeUpStart` | state 2 | Area141_ShadeUpStart 6000 |
| P27b | `Area141_ShadeUpStart` | +0x5C 0 | Area141_ShadeUpStart 5999 |
| P28 | `Area141_ShadeUpStep` | + 3 | Area141_ShadeUpStep 4216 |
| P29 | `Area141_ShadeUpStep` | unsigned compare | Area141_ShadeUpStep 3513 |
| P30 | `Area141_ShadeUpStep` | 0xC0 or above ends | Area141_ShadeUpStep 122 |
| P31 | `Area141_ShadeUpStep` | & 0xCF | Area141_ShadeUpStep 49 |
| P32 | `Area141_ShadeUpStep` | state 1 at the end | Area141_ShadeUpStep 93 |
| P33 | `Area141_ShadeUpStep` | member word +0x88 | Area141_ShadeUpStep 5907 |
| P34 | `Area141_ShadeUpStep` | +0x5F kept at the end | Area141_ShadeUpStep 93 |
| Q1 | `Area141_ChoiceFlag7D` | flag 0x7E | Area141_ChoiceFlag7D 6000 |
| Q2 | `Area141_ChoiceFlag7D` | answer below 2 | Area141_ChoiceFlag7D 545 |
| Q3 | `Area141_ChoiceFlag7D` | message 7 | Area141_ChoiceFlag7D 4989 |
| Q4 | `Area141_ChoiceFlag7D` | kind 2 entry 4 | Area141_ChoiceFlag7D 1011 |
| Q5 | `Area141_ChoiceFlag7D` | run 7 | Area141_ChoiceFlag7D 1011 |
| Q6 | `Area141_ChoiceFlag7D` | state 0x33 | Area141_ChoiceFlag7D 1011 |
| Q7 | `Area141_ChoiceFlag7D` | message 4 | Area141_ChoiceFlag7D 1011 |
| Q8 | `Area141_ChoiceFlag7D` | kind 0x35 | Area141_ChoiceFlag7D 1011 |
| Q9 | `Area141_ChoiceFlag7D` | answer read before the flag | Area141_ChoiceFlag7D 11 |
| Q9b | `Area141_ChoiceFlag7D` | run 8 on | Area141_ChoiceFlag7D 1011 |
| Q10 | `Area141_InitCells` | FD 3 for 0 | Area141_InitCells 1572 |
| Q11 | `Area141_InitCells` | run 1 off | Area141_InitCells 1049 |
| Q12 | `Area141_InitCells` | flag 0x7A with runs 3, 4 | Area141_InitCells 964 |
| Q13 | `Area141_InitCells` | flag 0x7D | Area141_InitCells 964 |
| Q14 | `Area141_InitCells` | runs 5, 6 | Area141_InitCells 964 |
| Q15 | `Area141_InitCells` | run 8 off | Area141_InitCells 1026 |
| Q16 | `Area141_InitCells` | FD 3 for 2 | Area141_InitCells 1549 |
| Q17 | `Area141_InitCells` | on 2 | Area141_InitCells 681 |
| Q18 | `Area141_InitCells` | FD 2 for 1 | Area141_InitCells 1990 |
| Q19 | `Area141_InitCells` | run 0 twice | Area141_InitCells 1049 |
| U1 | `Area141_Tail52` | drop-in 6 | Area141_Tail52 242 |
| U2 | `Area141_Tail52` | waits on request 3 | Area141_Tail52 98 |
| U3 | `Area141_Tail52` | message 4 for 3 | Area141_Tail52 324 |
| U4 | `Area141_Tail52` | | 6 | Area141_Tail52 153 |
| U5 | `Area141_Tail52` | timer 1 | Area141_Tail52 324 |
| U6 | `Area141_Tail52` | state 0xC after 0xA | Area141_Tail52 324 |
| U7 | `Area141_Tail52` | state 0xD after 0xB | Area141_Tail52 271 |
| U8 | `Area141_Tail52` | timer + 2 | Area141_Tail52 289 |
| U9 | `Area141_Tail52` | > 0x1C2 | Area141_Tail52 21 |
| U10 | `Area141_Tail52` | state 0xE on the time-out | Area141_Tail52 125 |
| U11 | `Area141_Tail52` | Input_Held | Area141_Tail52 53 |
| U12 | `Area141_Tail52` | message 5 | Area141_Tail52 234 |
| U13 | `Area141_Tail52` | kind 2 entry 3 | Area141_Tail52 234 |
| U14 | `Area141_Tail52` | state 0xF after 0xD | Area141_Tail52 234 |
| U15 | `Area141_Tail52` | counter 0 at 2 | Area141_Tail52 139 |
| U16 | `Area141_Tail52` | flag 0x7B | Area141_Tail52 89 |
| U17 | `Area141_Tail52` | run 3 on | Area141_Tail52 89 |
| U18 | `Area141_Tail52` | state 0x10 after 0xE | Area141_Tail52 89 |
| U19 | `TimerOut (141 x4)` | timer - 2 | Area141_Tail52 1260 |
| U20 | `TimerOut (141 x4)` | out at 1 | Area141_Tail52 93 |
| U21 | `Area141_Tail52` | x from z | Area141_Tail52 36 |
| U22 | `Area141_Tail52` | z from +0x3C | Area141_Tail52 36 |
| U23 | `Area141_Tail52` | state 0x11 after 0xF | Area141_Tail52 36 |
| U24 | `Area141_Tail52` | hold 1 passes | Area141_Tail52 41 |
| U25 | `Area141_Tail52` | message flag 0x40 | Area141_Tail52 60 |
| U26 | `Area141_Tail52` | & 0xFFF0 | Area141_Tail52 133 |
| U27 | `Area141_Tail52` | kind 1 at 0x14 | Area141_Tail52 296 |
| U28 | `FlagRunsOff (141 x2)` | set already: 0x15 | Area141_Tail52 437 |
| U29 | `FlagRunsOff (141 x2)` | flag + 1 set | Area141_Tail52 165 |
| U30 | `FlagRunsOff (141 x2)` | second run on | Area141_Tail52 165 |
| U31 | `FlagRunsOff (141 x2)` | timer 0x3D | Area141_Tail52 165 |
| U32 | `Area141_Tail52` | state 0x20 after 0x1E | Area141_Tail52 88 |
| U33 | `Area141_Tail52` | state 0x2A after 0x28 | Area141_Tail52 77 |
| U34 | `Area141_Tail52` | 0x2A waits, not 0x29 | Area141_Tail52 329 |
| U35 | `Area141_Tail52` | Cond_ByteFE 2 | Area141_Tail52 80 |
| U36 | `Area141_Tail52` | timer 0x3D at 0x32 | Area141_Tail52 80 |
| U37 | `Area141_Tail52` | state 0x34 after 0x32 | Area141_Tail52 80 |
| U38 | `Area141_Tail52` | counter 0 + 2 | Area141_Tail52 50 |
| U39 | `Area141_Tail52` | kind 1 at 0x33 | Area141_Tail52 50 |
| U40 | `Area141_Tail52` | 0x32 waits on counter 0 at 0 | Area141_Tail52 120 |
| U41 | `Area141_Tail52` | state 0x11 runs 0x10 | Area141_Tail52 45 |
| U42 | `Area141_Tail52` | state 0x80 + 0xA runs 0xA | Area141_Tail52 133 |
| U43 | `Area141_Tail52` | request 3 at 0xD | Area141_Tail52 223 |
| U44 | `Area141_Tail52` | no button test | Area141_Tail52 111 |
| U45 | `Area141_Tail52` | 0xB waits on request 3 | Area141_Tail52 84 |
| V1 | `Area141_CellHook` | the direction byte as the state | Area141_CellHook 1980 |
| V2 | `Area141_CellHook` | kind 0x35 | Area141_CellHook 1980 |
| V3 | `Area141_CellHook` | answers 2 | Area141_CellHook 1980 |
| V4 | `Area141_CellHook` | none answers 1 | Area141_CellHook 4020 |
| V5 | `Area141_CellHook` | no Set40 | Area141_CellHook 1980 |
| W1 | `Area141_Trigger57` | 0xBFFF for 0xC000 | Area141_Trigger57 1298 |
| W2 | `Area141_Trigger57` | run 1 on | Area141_Trigger57 845 |
| W3 | `Area141_Trigger57` | no heal | Area141_Trigger57 863 |
| W4 | `Area141_Trigger57` | state 1 at 0xC001 | Area141_Trigger57 863 |
| W5 | `Area141_Trigger57` | state 0xB at 0xC002 | Area141_Trigger57 867 |
| W6 | `Area141_Trigger57` | answers 1 otherwise | Area141_Trigger57 3425 |
| W7 | `Area141_Trigger57` | word +0x86 | Area141_Trigger57 2575 |
| W8 | `Area141_Trigger57` | kind 0x35 at 0xC002 | Area141_Trigger57 867 |
| X1 | `Area141_PaintCells` | count & 0x3F | Area141_PaintCells 245 |
| X2 | `Area141_PaintCells` | direction bit 0x40 | Area141_PaintCells 3214 |
| X3 | `Area141_PaintCells` | on read whole | Area141_PaintCells 1504 |
| X4 | `Area141_PaintCells` | off value is the on value | Area141_PaintCells 2999 |
| X5 | `Area141_PaintCells` | x from 1 | Area141_PaintCells 3236 |
| X6 | `Area141_PaintCells` | z run along x | Area141_PaintCells 2645 |
| X7 | `Area141_PaintCells` | one more cell | Area141_PaintCells 6000 |
| Y1 | `TwoFree (141 x3)` | the first kept | Area141_PlacePairAnimated 172, Area141_PlacePair6x 195, Area141_PlacePair0x 193 |
| Y2 | `TwoFree (141 x3)` | second none 0xFE | Area141_PlacePairAnimated 172, Area141_PlacePair6x 195, Area141_PlacePair0x 193 |
| Y3 | `TwoFree (141 x3)` | first none 0xFE | Area141_PlacePairAnimated 199, Area141_PlacePair6x 206, Area141_PlacePair0x 178 |
| Y4 | `TwoFree (141 x3)` | first marked 2 | Area141_PlacePairAnimated 5422, Area141_PlacePair6x 5437, Area141_PlacePair0x 5446 |
| Y5 | `PlacePair (141 x3)` | first index + 1 | Area141_PlacePairAnimated 5619, Area141_PlacePair6x 5598, Area141_PlacePair0x 5623 |
| Y6 | `PlacePair (141 x3)` | second marked 2 | Area141_PlacePairAnimated 5625, Area141_PlacePair6x 5598, Area141_PlacePair0x 5629 |
| Y7 | `PlacePair (141 x1)` | first animation start 1 | Area141_PlacePairAnimated 5629 |
| Y8 | `PlacePair (141 x3)` | second index is the first | Area141_PlacePairAnimated 5422, Area141_PlacePair6x 5436, Area141_PlacePair0x 5443 |
| Y9 | `Area141_PlacePairAnimated` | animation 0x59 | Area141_PlacePairAnimated 5629 |
| Y10 | `Area141_PlacePairAnimated` | scripts swapped | Area141_PlacePairAnimated 5629 |
| Y11 | `Area141_PlaceOneAnimated` | animation 4 | Area141_PlaceOneAnimated 5822 |
| Y12 | `PlaceOn (141 x2)` | marked 3 | Area141_PlaceOneAnimated 5821, Area141_PlaceOne0x 5828 |
| Y13 | `PlaceOn (141 x2)` | index + 1 | Area141_PlaceOneAnimated 5819, Area141_PlaceOne0x 5827 |
| Y14 | `Area141_PlacePair6x` | op 0x for 6x | Area141_PlacePair6x 5599 |
| Y15 | `Area141_PlacePair0x` | second script + 1 | Area141_PlacePair0x 5629 |
| Y16 | `Area141_PlaceOne0x` | op 6x | Area141_PlaceOne0x 5829 |
| Y17 | `Area141_PlaceOneAnimated` | none 0xFE | Area141_PlaceOneAnimated 178 |
| Y18 | `Area141_PlaceOne0x` | script + 1 | Area141_PlaceOne0x 5829 |
| Y19 | `Area141_PlaceOneAnimated` | script 0x62F660 | Area141_PlaceOneAnimated 5822 |
| Y20 | `Area141_PlacePair6x` | first script + 1 | Area141_PlacePair6x 5599 |
| Y21 | `Area141_PlacePair0x` | op 6x | Area141_PlacePair0x 5629 |
| Y22 | `PlacePair (141 x1)` | second animation start 1 | Area141_PlacePairAnimated 5629 |
| Z1 | `Area142_RunWait` | state ^ 1 | Area142_RunWait 6000 |
| Z2 | `Area142_WaitRequest3` | counter 0 at 2 | Area142_WaitRequest3 1988 |
| Z3 | `Area142_WaitRequest3` | request 2 | Area142_WaitRequest3 2024 |
| Z4 | `Area142_WaitRequest3` | bit 0x20 | Area142_WaitRequest3 1043 |
| Z5 | `Area142_WaitRequest3` | state 2 | Area142_WaitRequest3 1393 |
| Z6 | `Area142_WaitRequestEnd` | request 2 | Area142_WaitRequestEnd 2478 |
| Z7 | `Area142_WaitRequestEnd` | & 0x3F | Area142_WaitRequestEnd 2205 |
| Z8 | `Area142_WaitRequestEnd` | animation from +7 | Area142_WaitRequestEnd 4307 |
| Z9 | `Area142_WaitRequestEnd` | state 1 kept | Area142_WaitRequestEnd 4330 |
| Z10 | `Area142_WaitRequestEnd` | script - 1 | Area142_WaitRequestEnd 6000 |
| Z11 | `Area142_WaitRequest3` | script - 3 | Area142_WaitRequest3 5018 |
| Z12 | `Area142_WaitRequestEnd` | state 0 without re-reading after the call | Area142_WaitRequestEnd 1911 |


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

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count), D147 (uninitialised bytes),
D159 (party slot 1) in [`known-defects.md`](known-defects.md).

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
