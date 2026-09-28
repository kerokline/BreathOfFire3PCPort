# World 4, areas 168..172: the band `0x426560..0x428450`

**Status:** IN PROGRESS (2026-09-28) - 56 functions ours
(`src/game/area_w4b.cpp`, shadow name `area_w4b`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 336,000 rounds (in this worktree); 368 controls planted, 365 refused by a count, 3 equivalent with their variants refused (section 4).
Fuzz only: no recorded route reaches the band (section 8). No divergence;
the three dispatchers through area 172's state tables abort past the entries
that are code, where the original would jump into data (section 6).

Group AR4B of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
56 starts, none ours before, **56 taken**; no start dropped, none added
(section 7).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; area 168 has no record there, and
choices, hooks, tails, triggers, member frames and state tables have no
pairing).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the message box's answer byte
`0x7DEE67` in, the message word `0x7DEE48` read after), `kHandler` a `+0x3C`
handler (movement-script ops `03` / `DE`), `kInit` the `+0x40` init,
`kTail` a `Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a
step or arrive hook `(x, z)` answering in `al`, `kState` a state handler
reached through a table in the area's `.data`, `kCallee` a function called
directly (by the engine, by the group's own code, as an object trigger
`(object, 0x904030)` answering in `al`, or as an effect kind's handler).
"Armed" below is `ScriptFlags_Set40` then the tail kind and state written;
"ended" the kind and state cleared; "disarmed" `ScriptFlags_Clear40` then
ended.

### Area 168 (descriptor `0x63C900`: `+0x34` only; no PSX record)

Nine choice slots (`Area168_Choices`): 0 and 3 are one body, 2 is
`0x425C30` (area 167's block, group AR4A's), 8 is 0. No handlers, no init.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x426560` | `Area168_ChoiceFocusPair` | `0x5F` | choice 1 | kChoice | message `0xFFFF`; answer 3 arms engine tail kind 10 at state 5, sub-kind `0xFF`; then the focus object's (`0x903804`) dwords `+0x18` / `+0x1C` = the two bytes of `Area168_FocusPairs` by the answer (read again after the arming, signed, x 2) |
| `0x4265C0` | `Area168_ChoiceKeyItemC` | `0x41` | choices 0, 3 | kChoice | answer 0: key item `0xC` held - message 6 and `Cond_ByteFE` bit 1; not - message 7; other answers message `0xFFFF` |
| `0x426610` | `Area168_ChoiceKeyItemD` | `0x41` | choice 4 | kChoice | the same with key item `0xD` and bit 2 |
| `0x426660` | `Area168_ChoiceFlag8FArm59` | `0x38` | choice 5 | kChoice | message `0xFFFF`; answer 0: story flag `0x8F`, tail kind 59 armed at 0 |
| `0x4266A0` | `Area168_ChoiceArm59At10` | `0x26` | choice 6 | kChoice | message `0xFFFF`; answer 0: tail kind 59 armed at 10 |
| `0x4266D0` | `Area168_ChoiceMessage12` | `0x25` | choice 7 | kChoice | message `0x12` for answer 0, `0x13` for 1, `0x14` for any other |
| `0x426700` | `Area168_Tail59` | `0xE1` | tail kind 59 | kTail | by a 13-byte index table: 0 once `Field_Request` is not 2, state 1; 1 `Field_ChangeArea(0xA7, 0xB0000, 0x698000, 0x89)`, ended; 10 `Kind2_Place(0)`, 11; 11 counter 0 at 1 - sound `0x204`, story flag `0x91`, 12; 12 counter 0 at 2 - `ScriptFlags_Clear40`, counter 0 and the tail cleared; 2..9 and anything else nothing |

### Area 169 (descriptor `0x63C9F8`; PSX `0x801F3B58`)

Only an init in the descriptor; the rest hangs from the engine's tables.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4267F0` | `Area169_InitClearFlag74` | `0x19` | init (PSX `0x801F3524`) | kInit | `Cond_ByteFD` not 1: story flag `0x74` cleared |
| `0x426810` | `Area169_MembersFrame` | `0x1D7` | called by `0x46D7C3` (a gap of the tool) | kCallee | the member frame over `Area169_Rects` (below) |
| `0x4269F0` | `Area169_MemberRect` | `0x6A` | called by the frame (a gap) | kCallee | party record `member & 0xFF`'s x / z against the two rectangles' bytes `<< 16`, signed, inclusive: the index in `al`, or `0xFF` |
| `0x426A60` | `Area169_TailHealParty` | `0xB4` | tail kind 47 | kTail | four states (a jump table): 0 `Transition_Start(8)`; 1 the DA wait word 0 - sound `0x202`, `Party_HealJoined`, `Transition_Start(9)`; 2 the wait word 0 - message 1, `Field_Request` 2; 3 `Field_Request` not 2 - `ScriptFlags_Clear40`, story flag `0x74`, ended |
| `0x426B20` | `Area169_ArriveHook` | `0x35` | `Area_ArriveHook` (`0x56E57D`) | kHook | `Cond_ByteFD` 1 and flag `0x74` clear: tail kind 47 armed at 0, `al` 1; else `al` 0 |

**The member frame** (`0x46D780` calls it every field frame in the area,
`0x46D7C3`) is area 117's `Area117_MembersFrame` (AR3A,
[`area_w3a.md`](area_w3a.md)) without its story-flag turn of the facing and
over five-byte rectangles `(x0, z0, x1, z1, facing)`: `Field_ScriptFlags`
bit 13 cleared; per member `m` below `Field_MemberCount` (read again after
each member) `Field_State` and `Sprite_Current` become record `m`, the
search runs; outside every rectangle a member marked in the caller's `+0xB`
is unmarked there and in `Field_ScriptFlags2`; inside, unmarked and in
neither flag word, it is marked, `+0x128` = 2, `+9` = 0, `+8` = the
facing and `Sprite_EnsureAnimation`; inside and marked, `+9` 0 runs
`Field_JumpSetUp` (and `Field_JumpCamera` for member 0), else the facing is
turned when it differs (`+0xC`, `+0x10`, `+0x14` negated, `+9 = 8 - +9`);
then `Field_LeaderStepTick`, `+9 - 1`, `Sprite_ScriptTick` and bit 13 set.
`Field_Request` 5 clears bit 13 again; `Sprite_Current` is put back
(`Field_State` is not). The bits: `b = 1 << (m & 31)` for the flag words,
its low byte for `+0xB` (the original's 8-bit shift).

### Area 170 (descriptor `0x63D5F8`; PSX `0x801F6474`)

Six choices (`Area170_Choices`), whose 4 and 5 are also the two handlers
(`Area170_Handlers` starts at choice 4).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x426B60` | `Area170_ChoiceArm37At5` | `0x26` | choice 0 | kChoice | message `0xFFFF`; answer 0: tail kind 37 armed at 5 |
| `0x426B90` | `Area170_ChoiceArm37At15` | `0x26` | choice 1 | kChoice | the same at 15 |
| `0x426BC0` | `Area170_ChoiceArm37At25` | `0x26` | choice 2 | kChoice | the same at 25 |
| `0x426BF0` | `Area170_ChoiceState52Or55` | `0x1C` | choice 3 | kChoice | message `0xFFFF`; the tail's state 52 for answer 0, 55 for any other (the kind not written) |
| `0x426C10` | `Area170_ScriptFlagsSet1010` | `0xA` | handler 0 = choice 4 (PSX `0x801F4A7C`) | kHandler | `Field_ScriptFlags` \|= `0x1010` |
| `0x426C20` | `Area170_ScriptFlagsClear1010` | `0xA` | handler 1 = choice 5 (PSX `0x801F4A9C`) | kHandler | `Field_ScriptFlags` &= `0xEFEF` |
| `0x426C30` | `Area170_Init` | `0x52` | init (PSX `0x801F4ABC`) | kInit | tail kind 51 becomes 37; the engine's `0x486D60`; entry zone (`0x905E68`) 2 with `Cond_ByteFD` 1 plays sound `0x20C` (zone 2 with another value returns); the zone read again 3 with `Cond_ByteFD` 2 plays it |
| `0x426C90` | `Area170_Tail37` | `0x5DD` | tail kind 37 | kTail | 61 states by a byte table into 25 cases (below) |
| `0x427270` | `Area170_StepHook` | `0x200` | `Area_StepHook` (`0x56E1C2`) | kHook | below |
| `0x427470` | `Area170_Trigger19` | `0x25` | object trigger 19 (a gap) | kCallee | tail kind 37 armed at 40, story flag `0x7E` cleared, `al` 1 |
| `0x4274A0` | `Area170_Trigger20` | `0x12` | object trigger 20 (a gap) | kCallee | story flag `0x67`, `al` 0 |
| `0x4274C0` | `Area170_Trigger56` | `0x80` | object trigger 56 (a gap) | kCallee | flag `0x79` clear: `Inventory_Add(1, 0x4B, 1)` - added: flag `0x79`, `Sprite_SetAnimation(1)`, message `0x22`, sound `0x106`; not: system message 3; flag set: system message 1; then `Party_DropIn(0xD)`, `Sprite_Current` put back, `Field_Request` 2, `al` 0 |

**Tail kind 37.** Six drop-in pairs: states 0, 5, 10, 15, 20, 25 run
`Party_DropIn` of 0, 1, 4, 5, 8, 9 and step on; states 1, 6, 11, 16, 21, 26
wait for counter 3 at `0x24`, `0x24`, `0x44`, `0x44`, `0x64`, `0x64`, then set
story flag `0x55`, `0x55`, `0x56`, `0x56`, `0x57`, `0x57` and
`Field_ChangeArea(0xAA, x, z, flags)` (six points, flags `0x82`, `0x83`,
`0x86`, `0x87`, `0x8A`, `0x8B`), state 30. States 29 and 30 wait for counter
3 at 0: 29 sets `Field_ScriptFlags` bits `0x1010`, clears flag `0x57`, state
53; 30 clears flags `0x55..0x57` and ends. State 40: an effect of kind
`0x7D` (words `+0x2E` / `+0x30` `0x64`, `+6` the message word's low byte less
`0xB`, read after the slot search), the timer 0, state 41. State 41: the
timer 1 - the glide divisor from `Field_MoveSpeeds[2]`, `Field_Kind2X` /
`Z` a fixed point, an effect of kind `0x13` (`+0x64` 0, `+0x68` the camera
yaw, `+0x6C` 0, `+9` `0x70`; its slot kept in the scratch byte `0x903850`),
state 42; then (either way) the timer `0xFF` - state 45. State 42 waits for
`Field_Kind2Hold` 0 (timer `0x1E`, 43); 43 counts the timer down (`0x1E`,
44); 44 counts it down, then the divisor from `Field_MoveSpeeds[3]`,
`Field_Kind2X` / `Z` the leader's position and an effect of kind `0x13`
(`+0x64` `-0x2AA`, `+0x6C` `0x200`, `+9` `0x38`), state 45; 45 waits for
the hold 0 and disarms. State 50: message `0xA`, `Field_Request` 2, state
51 - which no case handles, so the tail idles there until something else
writes the state (choice 3 writes 52 or 55). State 52: `ScriptFlags_Clear40`,
`Party_DropIn(0xC)`, 53. State 53, while `Field_Request` is 0 and with no
exit of its own: the held input word is kept in the scratch word and its top
nibble replaced by `Area170_InputSwap[nibble]` (the swap is read, not named
here). State 55: `Field_Request` not 2 disarms. State 60:
`ScriptFlags_Clear40`, the words `+0x12E` of party records 1 and 2 + 1,
`Field_ScriptFlags` &= `0xEFEF`, ended.

**The step hook.** `Cond_ByteFD` 4: z exactly `0x708000` or `0x738000`
with x's high word 3..5, or x exactly `0x28000` or `0x58000` with z's high
word `0x71..0x73`: tail kind 37 armed at 0, `al` 1. `Cond_ByteFD` 5: z
`0x8E8000` / `0x918000` with x's high word 9..11, or x `0x88000` /
`0xB8000` with z's `0x8F..0x91`: armed at 10; the same z lines with x's
`0x10..0x12`, or x `0xF8000` / `0x128000` with z's `0x8F..0x91`: armed at
20, story flag `0x7E` set and `Field_ScriptFlags` &= `0xEFEF`; else (flag
`0x7E` clear) z exactly `0x900000` with x exactly `0x208000` and the
leader's pose 7: armed at 50; x exactly `0x218000` with pose 3: kind 37 and
state 60 written **without** `ScriptFlags_Set40`, `al` 0. The windows are
16-bit compares of the high word (`sub; cmp r16, 3; jb`).

### Area 171 (descriptor `0x63D808`; PSX `0x801F4144`)

Three choices (`Area171_Choices`), whose 1 and 2 are the two handlers.
Its init, member frame, rectangle search, tail and arrive hook are area
169's code (the init and the arrive hook with `Cond_ByteFD` 3 for 1 and
tail kind 48 for 47; the frame and the search over `Area171_Rects`
instruction for instruction; the tail with more states).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x427540` | `Area171_ChoiceArmTail10` | `0x2D` | choice 0 | kChoice | message `0xFFFF`; answer 0 arms engine tail kind 10 at 5, sub-kind `0xFF` |
| `0x427570` | `Area171_Leader89Gate` | `0x90` | handler 0 = choice 1 (PSX `0x801F3558`) | kHandler | the active member's `+0x80` bit 0 cleared; the leader's `+0x89` 5: unless the byte `0x903DB6` is `0x4B` nothing more, else story flag `0x73`, map cells (2, `0x21..0x22`) `0xC0` and (3, `0x21..0x22`) 0, the active member's word `+0x8A` + 1; then the leader's `+0x89` (read again) 7: tail kind 48 armed at 10 |
| `0x427600` | `Area171_SpawnEffect92` | `0xA1` | handler 1 = choice 2 (PSX `0x801F3650`) | kHandler | an effect of kind `0x92` (`+0x30` `0xFFE2`, `+0x29` 6, `+0xB` the active member's index among `Sprite_Objects`); then the four cells of handler 0 set to `0x89` |
| `0x4276B0` | `Area171_InitClearFlag74` | `0x19` | init (PSX `0x801F3764`) | kInit | `Cond_ByteFD` not 3: story flag `0x74` cleared |
| `0x4276D0` | `Area171_MembersFrame` | `0x1D7` | called by `0x46D7D0` (a gap) | kCallee | area 169's frame over `Area171_Rects` |
| `0x4278B0` | `Area171_MemberRect` | `0x6A` | called by the frame (a gap) | kCallee | area 169's search over `Area171_Rects` |
| `0x427920` | `Area171_TailHealParty` | `0x15E` | tail kind 48 | kTail | by a 22-byte index table: 0 `Cond_ByteFD` 3 - `Transition_Start(8)`, 1; else disarmed; 1..3 area 169's states; 10 the leader's `+0x137` 0 - `ScriptFlags_Clear40`, `Party_DropIn(0)`, ended; 20 message 2, `Field_Request` 2, 21; 21 `Field_Request` not 2 disarms |
| `0x427A80` | `Area171_StepHook` | `0xCC` | `Area_StepHook` (`0x56E1D2`) | kHook | `Cond_ByteFD` 2 and flag `0x73` clear: the scratch byte = 0, + 1 for each map byte `0x89` at the cell of (x, z), and by the fractions at x + 1, z + 1 and the diagonal; any counted: tail kind 48 armed at 20. `al` 0 always |
| `0x427B50` | `Area171_ArriveHook` | `0x35` | `Area_ArriveHook` (`0x56E593`) | kHook | `Cond_ByteFD` 3 and flag `0x74` clear: tail kind 48 armed at 0, `al` 1 |
| `0x427B90` | `Area171_Trigger55` | `0x20` | object trigger 55 (a gap) | kCallee | `Cond_ByteFE` 1, `MoveCmd_TestFB(0x1C, 0x4A)`, sound `0x109`, `al` 0 |

The `+0xB` index is `(Field_ActiveMember - Sprite_Objects) / 0xA4`, signed
and truncated (the compiler's multiply by `0x63E7063F`, `sar 6`, plus the
sign bit), read after the slot search.

### Area 172 (descriptor `0x63EED0`; PSX `0x801F672C`)

Nine choices (`Area172_Choices`): choice 0 its own, choices 1..8 the eight
handlers (`Area172_Handlers` starts one dword in). Two state machines and
effect kind `0xA5`'s states sit in `.data` right after the descriptor
(section 5).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x427BB0` | `Area172_ClearFlag4E` | `0x10` | handler 0 = choice 1 (PSX `0x801F4324`) | kHandler | story flag `0x4E` cleared |
| `0x427BC0` | `Area172_SkipIfMember89Is4` | `0x48` | handler 1 = choice 2 (PSX `0x801F434C`) | kHandler | a member below `Field_MemberCount` (read once) whose `+0x89` is 4: the script position + 3 |
| `0x427C10` | `Area172_RunFall` | `0x12` | handler 2 = choice 3 (PSX `0x801F43C8`) | kHandler | `Area172_FallStates` by `Sprite_Current[4]` (section 6) |
| `0x427C30` | `Area172_FallStart` | `0x7D` | `Area172_FallStates[0]` | kState | `+0` bit `0x40` cleared, `0x20` set; the tint `+0x5C` 1, `+0x5D..+0x5F` `0x80`; `+0xC`, `+0x10`, `+0x14` 0, `+0x20` -8; state 1; the script position - 2 |
| `0x427CB0` | `Area172_FallStep` | `0x139` | `Area172_FallStates[1]` | kState | `+0x14 += +0x20`, `Field_LeaderStepTick`; the ground (`MapView_GroundAt`) above the height word `+0x3E` (signed): landed - bit `0x20` and the tint cleared, the height the ground, `+0x14` / `+0x20` 0, state 2, `Sprite_SetAnimation(0x39)`, the script object's `+1` = 4; else the script position - 2. Then the field record's tint record (`MoveScript_TintRecords[Field_State[0x149]]`) bytes `+2..+4` - 2; `+0x5D` not `0x40`: `+0x5D..+0x5F` + 4; `Sprite_Kind2 + 0x3C` = the object's `+0x3C`, `MapView_SetElevation(+0x3E)` |
| `0x427DF0` | `Area172_RunSlide` | `0x12` | handler 3 = choice 4 (PSX `0x801F471C`) | kHandler | `Area172_SlideStates` by `Sprite_Current[4]` |
| `0x427E10` | `Area172_SlideStart` | `0x4E` | `Area172_SlideStates[0]` | kState | bit `0x20` set, `0x40` cleared, the tint 1 / `0x80`; `Area172_SlideStep` once (a direct call); state 1 |
| `0x427E60` | `Area172_SlideStep` | `0xB2` | `Area172_SlideStates[1]` | kState | x += `+0xC`, z += `+0x10`; each of `+0x5D..+0x5F` below `0xC0` as a signed byte + 2; all three `0xC0`: bit `0x20`, the tint and the state cleared; else the active member's word `+0x8A` - 2 |
| `0x427F20` | `Area172_Drift` | `0x77` | handler 4 = choice 5 (PSX `0x801F491C`) | kHandler | counter 0 below 7: x += `Area172_DriftSteps[+0xA & 0xF] << 11`, z -= it, `+0xA` - 1, the field record's word `+0x12E` - 2; else x and z `& 0xFFFF8000` |
| `0x427FA0` | `Area172_SpawnEffect92` | `0x65` | handler 5 = choice 6 (PSX `0x801F49DC`) | kHandler | area 171's effect with `+0x30` 0 and `+0x29` 4, no cells |
| `0x428010` | `Area172_TintOn` | `0x33` | handler 6 = choice 7 (PSX `0x801F4AAC`) | kHandler | bit `0x20`, the tint 1 / `0xC0` |
| `0x428050` | `Area172_TintOff` | `0x33` | handler 7 = choice 8 (PSX `0x801F4AF4`) | kHandler | bit `0x20` and the tint cleared |
| `0x428090` | `Area172_ChoiceFlag12` | `0x48` | choice 0 | kChoice | message `0xFFFF`; answer 0: `ScriptFlags_Set40`, then Cond row 14's (`0x904000`) flag `0x12` set - tail kind 35 at 5; clear - `MoveScript_Var7` 6 and the byte after it 0 |
| `0x4280E0` | `Area172_Tail35` | `0xBC` | tail kind 35 | kTail | 0 `Party_DropIn(1)`, 1; 1 counter 3 at `0x14` - flag `0x4E`, `Field_ChangeArea(0xAC, 0xA0000, 0x50000, 0x83)`, ended; 5 `Party_DropIn(0)`, 6; 6 the same to (`0x260000`, `0x360000`, `0x82`); 2..4 nothing |
| `0x4281A0` | `Area172_StepHook` | `0x4A` | `Area_StepHook` (`0x56E1E2`) | kHook | `Cond_ByteFD` 0, x exactly `0x248000`, z's high word `0x35..0x37`, the leader's pose 6, 7 or 0: tail kind 35 armed at 0, `al` 1 |
| `0x4281F0` | `Area172_InitCell` | `0xF` | init (PSX `0x801F4D58`) | kInit | map cell (9, 3) `0x50` |
| `0x428200` | `Area172_EffectA5Run` | `0x12` | `Effect_KindHandlers[0xA5]` (`0x6555E4`; a gap) | kCallee | `Area172_EffectStates` by `Sprite_Current[1]` |
| `0x428220` | `Area172_EffectA5Start` | `0x1F` | `Area172_EffectStates[0]` | kState | the record's word `+0x2E` and dword `+0xC` 0, state 1 |
| `0x428240` | `Area172_EffectA5Grow` | `0x50` | `Area172_EffectStates[1]` | kState | `+0x2E` + 10; above `0x190` (signed): the byte after `MoveScript_Var7` + 1, state 2; `Area172_DrawPanel(0xDC, 0xC8)`, `Area172_DrawShade(+0x2E, 0xC8)` (the word read again) |
| `0x428290` | `Area172_EffectA5Hold` | `0x13` | `Area172_EffectStates[2]` | kState | `Area172_DrawPanel(0xDC, 0xC8)` |
| `0x4282B0` | `Area172_DrawPanel` | `0xBA` | called by states 1 and 2 | kCallee | a `POLY_FT4` at `Gfx_PacketNext`: (x, y)..(x + `0x40`, y + `0x20`) from the arguments' low words (signed, `fild` / `fst` floats), uv 0..`0x40` x 0..`0x20`, `Gpu_GetClut(0, 0x1EB)`, `Gpu_GetTPage(1, 1, 0x300, 0x100)`, grey `0x80`; `Gfx_CommitPrim(1, 0x48)` |
| `0x428370` | `Area172_DrawShade` | `0xD8` | called by state 1 | kCallee | `Gpu_SetDrawMode(prim, 0, 1, Gpu_GetTPage(0, 2, 0x3C0, 0), 0)` (the fifth word is a push the tpage call left on the stack), `Gfx_CommitPrim(1, 0xC)`; a `POLY_G4` at `Gfx_PacketNext` read again: (x - `0x40`, y) black, (320, y) white, (x, y + `0x30`) grey `0x40`, (320, y + `0x30`) white, semi-transparent, shade-tex 0; `Gfx_CommitPrim(1, 0x44)` |

## 2. Ours

`src/game/area_w4b.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
one raw address of section 9 and for the member frames' rectangle searches,
called through each copy's own address as the originals do). The group's
own callees are called the same way (`AH_CALL(Area172_SlideStep)`,
`AH_CALL(Area172_DrawPanel)`, ...), so the fuzz stands a recorder in for each
and every function is fuzzed alone; the three state dispatchers read their
`.data` tables in place and call the entry (the original's `jmp [index * 4
+ table]`), so the fuzz's `DataTable` swap stands recorders there. Shapes
that repeat are one body: the member frame and the rectangle search (areas
169 and 171, over an `at::Rects` each), the heal states (the two tails'
states 1..3), the choice that arms a tail (`ChoiceArm`), the key-item
choice, the effect of kind `0x92` (areas 171 and 172) and of kind `0x13`
(area 170's states 41 and 44), area 170's drop-ins and area changes. Kept as
the originals: every re-read after a call (`Sprite_Current`, `Field_State`,
`Field_ActiveMember`, `MoveScript_Object`, the answer byte after
`ScriptFlags_Set40` in area 168's choice 1, the message word and the camera
yaw after the slot search, the entry zone after the sound in area 170's
init, the leader's `+0x89` after the map writes in area 171's gate, the
scratch byte area 171's step hook counts in memory), the order of every
call and every store around a call, the signed and 16-bit compares, the
floats of the two draws (`static_cast<float>` of an int, the same single
rounding as `fild` / `fst`), and the unchecked reads that stay in `.data`
(section 6).

## 3. The fuzz

`BOF3X_SHADOW=area_w4b` (`src/game/area_w4b_fuzz.cpp`): five `Run`s under
the one shadow name, one per area with its `Group::area`, 6,000 rounds per
function (336,000 rounds), the real descriptors and tables in place.

- **Callees the group lists** (registered before the standard set): every
  callee with its argument masks (`AreaMap_SetByte` / `AreaMap_ByteAt` /
  `MoveCmd_TestFB` words, `MapView_SetElevation` a word - the original
  pushes the height word with garbage above it, and the callee keeps the low
  word and uses 15 bits of the difference), `Effect_FindFree` `kByte
  0xFF..0x03` (a slot of the first four records or none), the rectangle
  searches `kByte 0xFF..0x01`, the flag and key tests `kFlag`, the engine's
  `0x486D60` by its address, the draws' GPU helpers, and the group's own
  slide step and draws.
- **Louder stand-ins** (an `effect`, each part of the time, from `Noise`
  only): `ScriptFlags_Set40` moves the answer byte, the focus pointer, the
  sub-kind and the tail; `ScriptFlags_Clear40` counter 0 and the tail; the
  flag writers `Field_ScriptFlags` and the tail; the area change, drop-in,
  transition, heal and message calls `Field_Request`, `Sprite_Current` and
  the tail; `Effect_FindFree` the active member, the message word, the
  camera yaw and the scratch byte; the sound and `0x486D60` the entry zone,
  `Cond_ByteFD`, `Sprite_Current` and the tail kind; `AreaMap_SetByte` the
  leader's `+0x89` and the active member; `AreaMap_ByteAt` answers `0x89`
  half the time and moves the scratch byte; the rectangle searches
  `Field_State`, `Field_ScriptFlags2` and `Sprite_Current`;
  `Sprite_ScriptTick` `Field_ScriptFlags`; `MapView_GroundAt` answers the
  object's height word or one either side half the time; the GPU setters
  scribble the primitive and `Gfx_CommitPrim` logs it and moves the packet
  pointer on (AR3F's pattern). The harness's own disturbance moves a group
  cell about one call in 24 (`Disturb`: the tail state, counters 0 and 3,
  the timer, the wait word, the hold, the entry zone, the scratch byte, the
  leader's `+0x89`, the tail kind, the answer, the three object pointers -
  from its hash only).
- **Data tables** swapped for recorders: `Area172_FallStates` over four
  entries (it covers `Area172_SlideStates`, which it runs into) and
  `Area172_EffectStates` (three).
- **Regions beyond the field frame** (33 regions, 22,611 bytes):
  `MoveScript_TintRecords` through `Sprite_Kind2` to all twenty
  `Effect_Objects` records as one run (`0x7E0700..0x7E1BE0`: the tint index
  is a byte), `Input_Held`, `Gfx_PacketNext` and a packet buffer of the
  fuzz's own, the active member, script object and focus pointers, the gate
  byte `0x903DB6`, the DA wait word, `Field_Kind2Hold`, `Camera_Angles`,
  `Field_MoveSpeeds`, `Cond_ByteFE`.
- **Every round:** the active member at a record (where an area writes
  through it) or at any value near `Sprite_Objects`, below it too (where only
  its distance is read, the two effects of kind `0x92`); the script object
  and the focus object at records; the packet pointer into the buffer.
- **Seeds:** each choice's answer (each tested value, its neighbours, a
  negative byte); each tail's state (every case, its neighbours, a state
  on no case, negatives) with the cell that state waits on paired to it two
  times in three (counter 0 at 1 / 2, counter 3 at `0x24` / `0x44` / `0x64`
  / 0 / `0x14`, the timer at 1 / `0xFF`, the wait word 0, `Field_Request`
  2 or not, the hold 0, `Cond_ByteFD` 3, the leader's `+0x137` 0); the step
  hooks' (x, z) on each exact line or one either side with the other high
  word in, beside or far from its window; area 171's fractions 0 or not;
  `Cond_ByteFD`, the leader's pose and `+0x89`, the gate byte and the
  entry zone at each tested value and beside; the party records around the
  rectangles' edges (read from the tables at run time), `+8` on the facing,
  `+9` 0; area 172's tint bytes on both sides of `0xC0` and at `0x40`, its
  effect word on both sides of `0x190`, counter 0 on both sides of 7, the
  members' `+0x89` at 4; the dispatchers' state bytes inside the entries
  that are code.

Coverage in this worktree (calls the originals made, first run): area 168
`KeyItem_Has` 1,432, `Field_ChangeArea` 506, `Kind2_Place` 514, the counter
waits' sound 354 and clear 338; area 169 `Area169_MemberRect` 11,918,
`Field_JumpSetUp` 1,476, `Sprite_EnsureAnimation` 976, `Party_HealJoined`
476; area 170 `Field_ChangeArea` 490, `Party_DropIn` 6,821,
`Inventory_Add` 1,991, `Effect_FindFree` 252 (states 40, 41, 44),
`0x486D60` 6,000; area 171 `Area171_MemberRect` 12,004, `AreaMap_ByteAt`
1,718, `AreaMap_SetByte` 25,288, `Party_DropIn` 230; area 172 the fall's
two states 1,533 / 1,467, the slide start 4,484 (both dispatchers),
`Area172_SlideStep` 10,516, the effect's three states 1,972 / 1,993 /
2,035, the draws' every GPU call 6,000 or more, `Field_ChangeArea` 771.

`BOF3X_SHADOW=area_w4b`: exit 0, five runs of 42,000 / 30,000 / 72,000 /
60,000 / 132,000 rounds, 0 mismatches (22,611 bytes, 33 regions).
`BOF3X_SHADOW='*'`: exit 0 on the first run, `inject: 5414 ours` (one below
the 5,415 `impl` lines, the off-by-one the round doc notes from before wave
two), 492 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w4b.cpp` by a script (the scratch
`controls_run.py`, not committed) that plants on an anchor it checks is
unique, rebuilds, checks `area_w4b.cpp` recompiled, runs
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w4b`, restores; after the last it
rebuilt, and the clean self-test passed (exit 0, 0 mismatches in all five
runs). **368 planted, 365 refused by a count (exit 3), 3 equivalent, each
with a near variant refused**; no hang, no fault. Every one of the 56
functions has controls of its own. A control in a body shared by two areas
(the member frame and search, the heal states) is planted once and refused
in the first area's run, whose Fatal ends the self-test; D24 / D25 give area
171's copies controls of their own (area 169's rectangles).

**The first run** (366) left seven standing. Four were the fuzz's: B10 (x's
sign bit masked: no seeded x had the sign bit over an x inside a rectangle -
the seed now plants one), D39 / D40 (area 171's fractions masked to `0xFFFE`
/ `0x7FFF`: no seeded fraction was exactly 1 or `0x8000` - the arguments now
take those), E38 (the slide start's step called before the tint stores: the
slide step's stand-in wrote nothing - it now writes the object's `+0`,
`+4` and tint bytes, as the real callee does). Re-run with the strengthened
fuzz, all four refused, and the 55 other controls of the functions those
seeds touch were re-run and refused again. **Three are equivalent** - no input
can tell them apart:

- C49: `SpawnEffect13` indexes the effect record by the slot it just stored
  in the scratch byte rather than by the byte read back; no call lies
  between the store and the read. Its near variant C49b (the byte stored as
  slot + 1) refused.
- D13: on a key-byte mismatch `Area171_Leader89Gate` goes on to the
  `+0x89 == 7` test instead of returning; the byte is 5 there and no call
  lies between, so the test always fails. Its near variant D13b (a mismatch
  arms the tail) refused.
- E49: `Area172_DriftSteps` repeats every four bytes, so `+0xA & 3` reads
  the same step as `& 0xF`. Its near variant E49b (`& 0xE`) refused.

The thinnest refusals: C56 (1 round of 6,000: state 45 waiting on a hold of
exactly 1, which the seed makes only among its odd values), C14 (3: the
entry zone read again after the sound - only the sound's louder stand-in
moves it), and area 170's step-hook lines (C71..C86, 5..10: each needs a
16.16 word exact and the other high word in a three-cell window at once);
the rest need 11 rounds or more. The counts are this worktree's (the
harness's pointers into our DLL make them build-directory dependent), from
the run that decided each control.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `Area168_ChoiceFocusPair` | answer 2 arms tail 10, not 3 | Area168_ChoiceFocusPair 1132 |
| A2 | `Area168_ChoiceFocusPair` | the first index from the answer read before ScriptFlags_Set40 | Area168_ChoiceFocusPair 268 |
| A3 | `Area168_ChoiceFocusPair` | the first index zero-extended | Area168_ChoiceFocusPair 1992 |
| A4 | `Area168_ChoiceFocusPair` | +0x1C from the pair first byte | Area168_ChoiceFocusPair 5090 |
| A5 | `Area168_ChoiceFocusPair` | no message store | Area168_ChoiceFocusPair 2035 |
| A6 | `Area168_ChoiceFocusPair` | tail 10 sub-kind 0xFE | Area168_ChoiceFocusPair 758 |
| A7 | `Area168_ChoiceFocusPair` | +0x1C written as +0x18 | Area168_ChoiceFocusPair 6000 |
| A8 | `Area168_ChoiceKeyItemC` | bit 3 not 1 | Area168_ChoiceKeyItemC 231 |
| A9 | `Area168_ChoiceKeyItemC` | key item 0xB | Area168_ChoiceKeyItemC 698 |
| A10 | `Area168_ChoiceKeyItemD` | bit 6 not 2 | Area168_ChoiceKeyItemD 245 |
| A11 | `Area168_ChoiceKeyItemD` | key item 0xE | Area168_ChoiceKeyItemD 734 |
| A12 | `Area168_ChoiceKeyItemC` | message 5 for held | Area168_ChoiceKeyItemC 467; Area168_ChoiceKeyItemD 497 |
| A13 | `Area168_ChoiceKeyItemC` | message 8 for not held | Area168_ChoiceKeyItemC 231; Area168_ChoiceKeyItemD 237 |
| A14 | `Area168_ChoiceKeyItemC` | message 0xFFFE for another answer | Area168_ChoiceKeyItemC 5302; Area168_ChoiceKeyItemD 5266 |
| A15 | `Area168_ChoiceKeyItemC` | KeyItem_Has tested on bit 0 | Area168_ChoiceKeyItemC 226; Area168_ChoiceKeyItemD 244 |
| A16 | `Area168_ChoiceFlag8FArm59` | story flag 0x8E | Area168_ChoiceFlag8FArm59 703 |
| A17 | `Area168_ChoiceFlag8FArm59` | tail 59 at state 1 | Area168_ChoiceFlag8FArm59 703 |
| A18 | `Area168_ChoiceFlag8FArm59` | the tail armed before the flag | Area168_ChoiceFlag8FArm59 703 |
| A19 | `Area168_ChoiceArm59At10` | state 11 | Area168_ChoiceArm59At10 735 |
| A20 | `Area168_ChoiceArm59At10` | ChoiceArm on answers 0 and 1 | Area168_ChoiceArm59At10 396 |
| A21 | `Area168_ChoiceMessage12` | message 0x11 for answer 0 | Area168_ChoiceMessage12 731 |
| A22 | `Area168_ChoiceMessage12` | 0x13 for answer 2 | Area168_ChoiceMessage12 768 |
| A23 | `Area168_Tail59` | state 0 waits on Field_Request 3 | Area168_Tail59 303 |
| A24 | `Area168_Tail59` | change-area flags 0x88 | Area168_Tail59 506 |
| A25 | `Area168_Tail59` | Kind2_Place(1) | Area168_Tail59 514 |
| A26 | `Area168_Tail59` | state 11 waits on counter 0 at 2 | Area168_Tail59 397 |
| A27 | `Area168_Tail59` | story flag 0x92 | Area168_Tail59 354 |
| A28 | `Area168_Tail59` | counter 0 not cleared | Area168_Tail59 337 |
| A29 | `Area168_Tail59` | state 13 runs state 12 | Area168_Tail59 38 |
| A30 | `Area168_Tail59` | the tail ended before the area change | Area168_Tail59 374 |
| A31 | `Area168_Tail59` | sound 0x205 | Area168_Tail59 354 |
| B1 | `Area169_InitClearFlag74` | Cond_ByteFD 2 | Area169_InitClearFlag74 2039 |
| B2 | `Area169_InitClearFlag74` | flag 0x75 | Area169_InitClearFlag74 4656 |
| B3 | `Area169_MemberRect` | x1 exclusive | Area169_MemberRect 106 |
| B4 | `Area169_MemberRect` | z1 exclusive | Area169_MemberRect 135 |
| B5 | `Area169_MemberRect` | x0 exclusive | Area169_MemberRect 87 |
| B6 | `Area169_MemberRect` | z0 from x0 | Area169_MemberRect 472 |
| B7 | `Area169_MemberRect` | the member masked to 9 bits | Area169_MemberRect 260 |
| B8 | `Area169_MemberRect` | the index xor 1 | Area169_MemberRect 935 |
| B9 | `Area169_MemberRect` | none answers 0xFE | Area169_MemberRect 5065 |
| B10 | `Area169_MemberRect` | unsigned compare on x | Area169_MemberRect 317 |
| B11 | `Area169_MembersFrame` | bit 13 and bit 0 cleared at the start | Area169_MembersFrame 2200 |
| B12 | `Area169_MembersFrame` | bit8 only for members 0 and 1 | Area169_MembersFrame 1108 |
| B13 | `Area169_MembersFrame` | unmark clears bit 7 too | Area169_MembersFrame 447 |
| B14 | `Area169_MembersFrame` | unmark clears the next bit of ScriptFlags2 | Area169_MembersFrame 1027 |
| B15 | `Area169_MembersFrame` | Field_ScriptFlags not tested | Area169_MembersFrame 929 |
| B16 | `Area169_MembersFrame` | +0x128 = 3 | Area169_MembersFrame 963 |
| B17 | `Area169_MembersFrame` | the field record from Sprite_Current | Area169_MembersFrame 593 |
| B18 | `Area169_MembersFrame` | +9 = 1 on marking | Area169_MembersFrame 953 |
| B19 | `Area169_MembersFrame` | facing from byte +3 | Area169_MembersFrame 963 |
| B20 | `Area169_MembersFrame` | EnsureAnimation(+9) | Area169_MembersFrame 963 |
| B21 | `Area169_MembersFrame` | the jump camera for member 1 | Area169_MembersFrame 1195 |
| B22 | `Area169_MembersFrame` | turn also for member 2 on the facing | Area169_MembersFrame 69 |
| B23 | `Area169_MembersFrame` | +9 = 7 - +9 | Area169_MembersFrame 1861 |
| B24 | `Area169_MembersFrame` | +0x10 not negated | Area169_MembersFrame 1886 |
| B25 | `Area169_MembersFrame` | +9 - 2 | Area169_MembersFrame 3145 |
| B26 | `Area169_MembersFrame` | bit 14 set | Area169_MembersFrame 2602 |
| B27 | `Area169_MembersFrame` | the count read once | Area169_MembersFrame 470 |
| B28 | `Area169_MembersFrame` | Field_Request 4 clears bit 13 | Area169_MembersFrame 769 |
| B29 | `Area169_MembersFrame` | Sprite_Current not put back | Area169_MembersFrame 5075 |
| B30 | `Area169_MembersFrame` | script tick before the step tick | Area169_MembersFrame 3222 |
| B31 | `Area169_MembersFrame` | Sprite_Current not read again for +9 (the rect search moves it) | Area169_MembersFrame 1550 |
| B32 | `Area169_MembersFrame` | +0xC not negated | Area169_MembersFrame 1874 |
| B33 | `Area169_MembersFrame` | Field_ScriptFlags2 bit not set on marking | Area169_MembersFrame 708 |
| B34 | `Area169_TailHealParty` | Transition_Start(7) in state 0 | Area169_TailHealParty 647 |
| B35 | `Area169_TailHealParty` | HealStates: sound 0x203 | Area169_TailHealParty 476 |
| B36 | `Area169_TailHealParty` | HealStates: Transition_Start(10) | Area169_TailHealParty 476 |
| B37 | `Area169_TailHealParty` | HealStates: message 2 | Area169_TailHealParty 457 |
| B38 | `Area169_TailHealParty` | HealStates: state 1 waits on the wait word 1 | Area169_TailHealParty 556 |
| B39 | `Area169_TailHealParty` | HealStates: state 2 waits on the wait word 1 | Area169_TailHealParty 523 |
| B40 | `Area169_TailHealParty` | HealStates: Field_Request 1 | Area169_TailHealParty 457 |
| B41 | `Area169_TailHealParty` | HealStates: flag 0x75 | Area169_TailHealParty 316 |
| B42 | `Area169_TailHealParty` | HealStates: no Party_HealJoined | Area169_TailHealParty 476 |
| B43 | `Area169_TailHealParty` | HealStates: state 3 waits on Field_Request 1 | Area169_TailHealParty 437 |
| B44 | `Area169_TailHealParty` | state 4 runs state 3 | Area169_TailHealParty 177 |
| B45 | `Area169_ArriveHook` | Cond_ByteFD 2 | Area169_ArriveHook 2055 |
| B46 | `Area169_ArriveHook` | flag 0x75 | Area169_ArriveHook 1409 |
| B47 | `Area169_ArriveHook` | tail 47 at 1 | Area169_ArriveHook 467 |
| B48 | `Area169_ArriveHook` | answers 2 | Area169_ArriveHook 467 |
| C1 | `Area170_ChoiceArm37At5` | state 4 | Area170_ChoiceArm37At5 780 |
| C2 | `Area170_ChoiceArm37At15` | state 14 | Area170_ChoiceArm37At15 735 |
| C3 | `Area170_ChoiceArm37At25` | state 24 | Area170_ChoiceArm37At25 747 |
| C3b | `Area170_ChoiceArm37At25` | kind 0x24 | Area170_ChoiceArm37At25 747 |
| C4 | `Area170_ChoiceState52Or55` | 55 only above 1 | Area170_ChoiceState52Or55 364 |
| C5 | `Area170_ChoiceState52Or55` | 54 for others | Area170_ChoiceState52Or55 5244 |
| C5b | `Area170_ChoiceState52Or55` | no message | Area170_ChoiceState52Or55 2020 |
| C6 | `Area170_ScriptFlagsSet1010` | \| 0x1011 | Area170_ScriptFlagsSet1010 2924 |
| C7 | `Area170_ScriptFlagsClear1010` | & 0xEFEE | Area170_ScriptFlagsClear1010 3038 |
| C8 | `Area170_Init` | kind 0x32 becomes 37 | Area170_Init 926 |
| C9 | `Area170_Init` | kind 51 becomes 0x24 | Area170_Init 620 |
| C10 | `Area170_Init` | no map set-up | Area170_Init 6000 |
| C11 | `Area170_Init` | zone 3 test with Cond_ByteFD 2 or more | Area170_Init 591 |
| C12 | `Area170_Init` | first sound 0x20D | Area170_Init 397 |
| C13 | `Area170_Init` | zone 2 with Cond_ByteFD 0 | Area170_Init 80 |
| C14 | `Area170_Init` | the zone read once | Area170_Init 3 |
| C15 | `Area170_Tail37` | state 0 to 2 | Area170_Tail37 110 |
| C16 | `Area170_Tail37` | state 1 flags 0x83 | Area170_Tail37 81 |
| C17 | `Area170_Tail37` | state 5 drop-in 2 | Area170_Tail37 116 |
| C18 | `Area170_Tail37` | state 6 z | Area170_Tail37 93 |
| C19 | `Area170_Tail37` | state 10 to 12 | Area170_Tail37 119 |
| C20 | `Area170_Tail37` | state 11 counter 0x45 | Area170_Tail37 88 |
| C21 | `Area170_Tail37` | state 15 drop-in 6 | Area170_Tail37 110 |
| C22 | `Area170_Tail37` | state 16 flag 0x57 | Area170_Tail37 85 |
| C23 | `Area170_Tail37` | state 20 to 22 | Area170_Tail37 126 |
| C24 | `Area170_Tail37` | state 21 x | Area170_Tail37 78 |
| C25 | `Area170_Tail37` | state 25 to 27 | Area170_Tail37 127 |
| C26 | `Area170_Tail37` | state 26 counter 0x63 | Area170_Tail37 90 |
| C27 | `Area170_Tail37` | area 0xAB | Area170_Tail37 490 |
| C28 | `Area170_Tail37` | state 31 after the change | Area170_Tail37 490 |
| C29 | `Area170_Tail37` | state written before the change | Area170_Tail37 250 |
| C30 | `Area170_Tail37` | state 29 sets 0x1000 | Area170_Tail37 23 |
| C31 | `Area170_Tail37` | state 29 to 54 | Area170_Tail37 89 |
| C32 | `Area170_Tail37` | state 30 clears 0x54 | Area170_Tail37 84 |
| C33 | `Area170_Tail37` | state 30 counter 1 | Area170_Tail37 98 |
| C34 | `Area170_Tail37` | effect kind 0x7C | Area170_Tail37 111 |
| C35 | `Area170_Tail37` | message - 0xA | Area170_Tail37 111 |
| C36 | `Area170_Tail37` | +0x2E 0x65 | Area170_Tail37 111 |
| C37 | `Area170_Tail37` | state 40 timer 1 | Area170_Tail37 136 |
| C38 | `Area170_Tail37` | state 40 message read before the search | Area170_Tail37 47 |
| C39 | `Area170_Tail37` | state 41 timer 2 | Area170_Tail37 47 |
| C40 | `Area170_Tail37` | state 41 divisor from speed 3 | Area170_Tail37 42 |
| C41 | `Area170_Tail37` | state 41 Kind2X | Area170_Tail37 42 |
| C42 | `Area170_Tail37` | state 41 effect +9 0x71 | Area170_Tail37 39 |
| C43 | `Area170_Tail37` | state 41 timer 0xFE | Area170_Tail37 54 |
| C44 | `Area170_Tail37` | SpawnEffect13: no scratch store | Area170_Tail37 116 |
| C45 | `Area170_Tail37` | SpawnEffect13: yaw zero-extended | Area170_Tail37 56 |
| C46 | `Area170_Tail37` | SpawnEffect13: +0x6C + 1 | Area170_Tail37 104 |
| C47 | `Area170_Tail37` | SpawnEffect13: kind 0x14 | Area170_Tail37 104 |
| C48 | `Area170_Tail37` | SpawnEffect13: yaw read before the search | Area170_Tail37 54 |
| C49 | `Area170_Tail37` | SpawnEffect13: the record by the slot, not the scratch read back | STANDS  |
| C50 | `Area170_Tail37` | state 42 timer 0x1D | Area170_Tail37 95 |
| C51 | `Area170_Tail37` | state 43 to 43 | Area170_Tail37 81 |
| C52 | `Area170_Tail37` | state 44 +9 0x39 | Area170_Tail37 65 |
| C53 | `Area170_Tail37` | state 44 z from x | Area170_Tail37 74 |
| C54 | `Area170_Tail37` | state 44 divisor << 2 | Area170_Tail37 74 |
| C55 | `Area170_Tail37` | state 44 timer - 2 | Area170_Tail37 113 |
| C56 | `Area170_Tail37` | state 45 on hold 1 | Area170_Tail37 1 |
| C57 | `Area170_Tail37` | state 50 message 0xB | Area170_Tail37 120 |
| C58 | `Area170_Tail37` | state 50 to 52 | Area170_Tail37 120 |
| C59 | `Area170_Tail37` | state 52 drop-in 0xB | Area170_Tail37 113 |
| C60 | `Area170_Tail37` | state 53 on Field_Request 1 | Area170_Tail37 97 |
| C61 | `Area170_Tail37` | state 53 scratch + 1 | Area170_Tail37 82 |
| C62 | `Area170_Tail37` | state 53 nibble & 7 | Area170_Tail37 22 |
| C63 | `Area170_Tail37` | state 53 low bits & 0x7FF | Area170_Tail37 44 |
| C64 | `Area170_Tail37` | state 55 on Field_Request 3 | Area170_Tail37 92 |
| C65 | `Area170_Tail37` | state 60 record 2 + 2 | Area170_Tail37 118 |
| C66 | `Area170_Tail37` | state 60 & 0xEFFF | Area170_Tail37 49 |
| C67 | `Area170_Tail37` | state 51 runs state 50 | Area170_Tail37 140 |
| C68 | `Area170_Tail37` | state 60 without ScriptFlags_Clear40 | Area170_Tail37 118 |
| C69 | `Area170_StepHook` | Cond_ByteFD 3 for the first set | Area170_StepHook 77 |
| C70 | `Area170_StepHook` | x window from 4 | Area170_StepHook 15 |
| C71 | `Area170_StepHook` | x line 0x68000 | Area170_StepHook 10 |
| C72 | `Area170_StepHook` | x window from 8 | Area170_StepHook 9 |
| C73 | `Area170_StepHook` | x line 0xB8001 | Area170_StepHook 10 |
| C74 | `Area170_StepHook` | z line 0x8E8001 for the 0x10 window | Area170_StepHook 8 |
| C75 | `Area170_StepHook` | x line 0x128001 | Area170_StepHook 5 |
| C76 | `Area170_StepHook` | flag 0x7F set | Area170_StepHook 26 |
| C77 | `Area170_StepHook` | & 0xEFEE | Area170_StepHook 14 |
| C78 | `Area170_StepHook` | flag 0x7D tested | Area170_StepHook 1091 |
| C79 | `Area170_StepHook` | pose 6 | Area170_StepHook 8 |
| C80 | `Area170_StepHook` | state 49 | Area170_StepHook 7 |
| C81 | `Area170_StepHook` | pose 2 | Area170_StepHook 10 |
| C82 | `Area170_StepHook` | state 59 | Area170_StepHook 6 |
| C83 | `Area170_StepHook` | In3 < 4 | Area170_StepHook 22 |
| C84 | `Area170_StepHook` | first path answers 2 | Area170_StepHook 25 |
| C85 | `Area170_StepHook` | second path at state 1 | Area170_StepHook 26 |
| C86 | `Area170_StepHook` | state 60 path through ScriptFlags_Set40 | Area170_StepHook 6 |
| C87 | `Area170_StepHook` | state 10 path at 11 | Area170_StepHook 34 |
| C88 | `Area170_StepHook` | the high word signed (>> 17 dropped bit) | Area171_StepHook 245 |
| C89 | `Area170_Trigger19` | state 41 | Area170_Trigger19 3002 |
| C90 | `Area170_Trigger19` | answers 0 | Area170_Trigger19 6000 |
| C91 | `Area170_Trigger19` | flag cleared before arming | Area170_Trigger19 6000 |
| C92 | `Area170_Trigger20` | flag 0x66 | Area170_Trigger20 6000 |
| C93 | `Area170_Trigger20` | answers 1 | Area170_Trigger20 6000 |
| C94 | `Area170_Trigger56` | flag 0x78 tested | Area170_Trigger56 6000 |
| C95 | `Area170_Trigger56` | count 2 | Area170_Trigger56 1991 |
| C96 | `Area170_Trigger56` | animation 2 | Area170_Trigger56 1329 |
| C97 | `Area170_Trigger56` | message 0x23 | Area170_Trigger56 1329 |
| C98 | `Area170_Trigger56` | sound 0x107 | Area170_Trigger56 1329 |
| C99 | `Area170_Trigger56` | system message 4 | Area170_Trigger56 662 |
| C100 | `Area170_Trigger56` | system message 2 | Area170_Trigger56 4009 |
| C101 | `Area170_Trigger56` | drop-in 0xE | Area170_Trigger56 6000 |
| C102 | `Area170_Trigger56` | Sprite_Current not put back | Area170_Trigger56 4214 |
| C103 | `Area170_Trigger56` | Field_Request 3 | Area170_Trigger56 6000 |
| C104 | `Area170_Trigger56` | flag 0x7A set | Area170_Trigger56 1329 |
| D1 | `Area171_ChoiceArmTail10` | answer 1 | Area171_ChoiceArmTail10 1102 |
| D2 | `Area171_Leader89Gate` | +0x80 & 0xFC | Area171_Leader89Gate 2988 |
| D3 | `Area171_Leader89Gate` | +0x89 4 | Area171_Leader89Gate 474 |
| D4 | `Area171_Leader89Gate` | key 0x4A | Area171_Leader89Gate 500 |
| D5 | `Area171_Leader89Gate` | flag 0x72 | Area171_Leader89Gate 322 |
| D6 | `Area171_Leader89Gate` | cell 0xC1 | Area171_Leader89Gate 322 |
| D7 | `Area171_Leader89Gate` | cell (3, 0x20) | Area171_Leader89Gate 322 |
| D8 | `Area171_Leader89Gate` | word + 2 | Area171_Leader89Gate 322 |
| D9 | `Area171_Leader89Gate` | the member not read again | Area171_Leader89Gate 270 |
| D10 | `Area171_Leader89Gate` | +0x89 6 arms | Area171_Leader89Gate 1908 |
| D11 | `Area171_Leader89Gate` | state 11 | Area171_Leader89Gate 1287 |
| D12 | `Area171_Leader89Gate` | +0x89 read once | Area171_Leader89Gate 125 |
| D13 | `Area171_Leader89Gate` | the key mismatch goes on to the 7 test | STANDS  |
| D14 | `Area171_SpawnEffect92` | +0x30 0xFFE3 | Area171_SpawnEffect92 4823 |
| D15 | `Area171_SpawnEffect92` | +0x29 7 | Area171_SpawnEffect92 4823 |
| D16 | `Area171_SpawnEffect92` | cell 0x88 | Area171_SpawnEffect92 6000 |
| D17 | `Area171_SpawnEffect92` | SpawnEffect92: kind 0x93 | Area171_SpawnEffect92 4823 |
| D18 | `Area171_SpawnEffect92` | SpawnEffect92: unsigned divide | Area171_SpawnEffect92 134 |
| D19 | `Area171_SpawnEffect92` | SpawnEffect92: floor divide | Area171_SpawnEffect92 98 |
| D20 | `Area171_SpawnEffect92` | SpawnEffect92: the member read before the search | Area171_SpawnEffect92 2277 |
| D21 | `Area171_SpawnEffect92` | SpawnEffect92: +0x2E 1 | Area171_SpawnEffect92 4823 |
| D22 | `Area171_SpawnEffect92` | the first cell before the spawn | Area171_SpawnEffect92 6000 |
| D23 | `Area171_InitClearFlag74` | Cond_ByteFD 2 | Area171_InitClearFlag74 2385 |
| D24 | `Area171_MembersFrame` | area 169's rectangles | Area171_MembersFrame 6000 |
| D25 | `Area171_MemberRect` | area 169's rectangles | Area171_MemberRect 162 |
| D26 | `Area171_TailHealParty` | state 0 on Cond_ByteFD 2 | Area171_TailHealParty 272 |
| D27 | `Area171_TailHealParty` | state 0 Transition_Start(7) | Area171_TailHealParty 242 |
| D28 | `Area171_TailHealParty` | state 10 on +0x137 1 | Area171_TailHealParty 263 |
| D29 | `Area171_TailHealParty` | state 10 drop-in 1 | Area171_TailHealParty 230 |
| D30 | `Area171_TailHealParty` | state 20 message 3 | Area171_TailHealParty 363 |
| D31 | `Area171_TailHealParty` | state 20 to 22 | Area171_TailHealParty 363 |
| D32 | `Area171_TailHealParty` | state 21 on Field_Request 1 | Area171_TailHealParty 194 |
| D33 | `Area171_TailHealParty` | state 2 not run | Area171_TailHealParty 220 |
| D34 | `Area171_TailHealParty` | state 0 disarm without Clear40 | Area171_TailHealParty 110 |
| D35 | `Area171_StepHook` | Cond_ByteFD 1 | Area171_StepHook 2358 |
| D36 | `Area171_StepHook` | flag 0x74 | Area171_StepHook 1548 |
| D37 | `Area171_StepHook` | no scratch store | Area171_StepHook 214 |
| D38 | `Area171_StepHook` | byte 0x88 | Area171_StepHook 135 |
| D39 | `Area171_StepHook` | x fraction & 0xFFFE | Area171_StepHook 35 |
| D40 | `Area171_StepHook` | z fraction & 0x7FFF | Area171_StepHook 25 |
| D41 | `Area171_StepHook` | x + 2 | Area171_StepHook 433 |
| D42 | `Area171_StepHook` | z + 2 | Area171_StepHook 425 |
| D43 | `Area171_StepHook` | the diagonal on either fraction | Area171_StepHook 158 |
| D44 | `Area171_StepHook` | state 21 | Area171_StepHook 485 |
| D45 | `Area171_StepHook` | answers 1 when armed | Area171_StepHook 485 |
| D46 | `Area171_StepHook` | the count kept in a local, not the scratch byte | Area171_StepHook 28 |
| D47 | `Area171_ArriveHook` | Cond_ByteFD 2 | Area171_ArriveHook 2455 |
| D48 | `Area171_ArriveHook` | state 1 | Area171_ArriveHook 507 |
| D49 | `Area171_ArriveHook` | flag 0x75 | Area171_ArriveHook 1637 |
| D50 | `Area171_Trigger55` | Cond_ByteFE 2 | Area171_Trigger55 6000 |
| D51 | `Area171_Trigger55` | arguments swapped | Area171_Trigger55 6000 |
| D52 | `Area171_Trigger55` | sound 0x108 | Area171_Trigger55 6000 |
| D53 | `Area171_Trigger55` | answers 1 | Area171_Trigger55 6000 |
| E1 | `Area172_ClearFlag4E` | flag 0x4F | Area172_ClearFlag4E 6000 |
| E2 | `Area172_SkipIfMember89Is4` | +0x89 3 | Area172_SkipIfMember89Is4 2270 |
| E3 | `Area172_SkipIfMember89Is4` | position + 4 | Area172_SkipIfMember89Is4 1758 |
| E4 | `Area172_SkipIfMember89Is4` | one member fewer | Area172_SkipIfMember89Is4 827 |
| E5 | `Area172_RunFall` | the state xor 1 | Area172_RunFall 6000 |
| E6 | `Area172_RunSlide` | the state xor 1 | Area172_RunSlide 6000 |
| E7 | `Area172_EffectA5Run` | the state & 1 | Area172_EffectA5Run 2035 |
| E8 | `Area172_FallStart` | & 0xBE | Area172_FallStart 2954 |
| E9 | `Area172_FallStart` | +0x20 -7 | Area172_FallStart 6000 |
| E10 | `Area172_FallStart` | +0x5E 0x81 | Area172_FallStart 6000 |
| E11 | `Area172_FallStart` | state 2 | Area172_FallStart 6000 |
| E12 | `Area172_FallStart` | position - 3 | Area172_FallStart 6000 |
| E13 | `Area172_FallStart` | +0x10 kept | Area172_FallStart 6000 |
| E14 | `Area172_FallStep` | +0x14 -= +0x20 | Area172_FallStep 5144 |
| E15 | `Area172_FallStep` | Sprite_Current not read again before the ground | Area172_FallStep 2708 |
| E16 | `Area172_FallStep` | lands on equal | Area172_FallStep 1023 |
| E17 | `Area172_FallStep` | unsigned compare | Area172_FallStep 1538 |
| E18 | `Area172_FallStep` | height ground + 1 | Area172_FallStep 2499 |
| E19 | `Area172_FallStep` | state 3 | Area172_FallStep 2483 |
| E20 | `Area172_FallStep` | animation 0x38 | Area172_FallStep 2499 |
| E21 | `Area172_FallStep` | script object +1 = 5 | Area172_FallStep 2499 |
| E22 | `Area172_FallStep` | position - 1 | Area172_FallStep 3500 |
| E23 | `Area172_FallStep` | tint index & 0x7F | Area172_FallStep 2985 |
| E24 | `Area172_FallStep` | tint +3 - 3 | Area172_FallStep 5971 |
| E25 | `Area172_FallStep` | tint stops at 0x44 | Area172_FallStep 572 |
| E26 | `Area172_FallStep` | +0x5F + 3 | Area172_FallStep 5619 |
| E27 | `Area172_FallStep` | camera height from +0x38 | Area172_FallStep 6000 |
| E28 | `Area172_FallStep` | elevation from +0x3C | Area172_FallStep 6000 |
| E29 | `Area172_FallStep` | Sprite_Current not read again after the landing | Area172_FallStep 1161 |
| E30 | `Area172_FallStep` | the tint record from Sprite_Current | Area172_FallStep 4998 |
| E31 | `Area172_FallStep` | landing & 0xDE | Area172_FallStep 1231 |
| E32 | `Area172_FallStep` | landing +0x20 1 | Area172_FallStep 2499 |
| E33 | `Area172_FallStep` | the script object read before the animation | Area172_FallStep 1147 |
| E34 | `Area172_SlideStart` | \| 0x21 | Area172_SlideStart 1533 |
| E35 | `Area172_SlideStart` | +0x5D 0x7F | Area172_SlideStart 3025 |
| E36 | `Area172_SlideStart` | the state through the old pointer | Area172_SlideStart 2679 |
| E37 | `Area172_SlideStart` | state 2 | Area172_SlideStart 6000 |
| E38 | `Area172_SlideStart` | the step before the tint | Area172_SlideStart 2975 |
| E39 | `Area172_SlideStep` | x by +0x10 | Area172_SlideStep 6000 |
| E40 | `Area172_SlideStep` | tint to 0xC0 inclusive | Area172_SlideStep 2473 |
| E41 | `Area172_SlideStep` | tint + 1 | Area172_SlideStep 4241 |
| E42 | `Area172_SlideStep` | unsigned tint compare | Area172_SlideStep 4256 |
| E43 | `Area172_SlideStep` | +0x5F not tested | Area172_SlideStep 250 |
| E44 | `Area172_SlideStep` | ends at state 1 | Area172_SlideStep 99 |
| E45 | `Area172_SlideStep` | member word - 1 | Area172_SlideStep 5901 |
| E46 | `Area172_SlideStep` | +0x5C left 1 | Area172_SlideStep 99 |
| E47 | `Area172_SlideStep` | bit 0x20 kept at the end | Area172_SlideStep 40 |
| E48 | `Area172_Drift` | counter below 6 | Area172_Drift 702 |
| E49 | `Area172_Drift` | x step index & 3 (period 4: expected equivalent) | STANDS  |
| E49b | `Area172_Drift` | x step index & 0xE (near variant) | Area172_Drift 690 |
| E50 | `Area172_Drift` | x step << 10 | Area172_Drift 690 |
| E51 | `Area172_Drift` | z not negated | Area172_Drift 690 |
| E52 | `Area172_Drift` | +0xA - 2 | Area172_Drift 1427 |
| E53 | `Area172_Drift` | leader word - 1 | Area172_Drift 1427 |
| E54 | `Area172_Drift` | snap & 0xFFFF0000 | Area172_Drift 2266 |
| E55 | `Area172_Drift` | step unsigned | Area172_Drift 321 |
| E56 | `Area172_Drift` | z step index & 0xE | Area172_Drift 737 |
| E57 | `Area172_SpawnEffect92` | +0x29 5 | Area172_SpawnEffect92 4838 |
| E58 | `Area172_SpawnEffect92` | +0x30 1 | Area172_SpawnEffect92 4838 |
| E59 | `Area172_TintOn` | +0x5E 0xBF | Area172_TintOn 6000 |
| E60 | `Area172_TintOn` | \| 0x60 | Area172_TintOn 3003 |
| E61 | `Area172_TintOn` | +0x5C 2 | Area172_TintOn 6000 |
| E62 | `Area172_TintOff` | & 0x9F | Area172_TintOff 2961 |
| E63 | `Area172_TintOff` | +0x5E 1 | Area172_TintOff 6000 |
| E64 | `Area172_ChoiceFlag12` | flag 0x13 | Area172_ChoiceFlag12 685 |
| E65 | `Area172_ChoiceFlag12` | the story bank | Area172_ChoiceFlag12 685 |
| E66 | `Area172_ChoiceFlag12` | state 6 | Area172_ChoiceFlag12 444 |
| E67 | `Area172_ChoiceFlag12` | Var7 7 | Area172_ChoiceFlag12 241 |
| E68 | `Area172_ChoiceFlag12` | Var7b 1 | Area172_ChoiceFlag12 241 |
| E69 | `Area172_ChoiceFlag12` | no ScriptFlags_Set40 | Area172_ChoiceFlag12 685 |
| E70 | `Area172_ChoiceFlag12` | answers 0 and 1 | Area172_ChoiceFlag12 412 |
| E71 | `Area172_ChoiceFlag12` | kind 0x22 | Area172_ChoiceFlag12 444 |
| E72 | `Area172_Tail35` | state 0 drop-in 2 | Area172_Tail35 593 |
| E73 | `Area172_Tail35` | state 1 counter 0x15 | Area172_Tail35 455 |
| E74 | `Area172_Tail35` | state 1 flags 0x84 | Area172_Tail35 386 |
| E75 | `Area172_Tail35` | state 5 to 7 | Area172_Tail35 564 |
| E76 | `Area172_Tail35` | state 6 x | Area172_Tail35 385 |
| E77 | `Area172_Tail35` | state 6 flag 0x4F | Area172_Tail35 385 |
| E78 | `Area172_Tail35` | state 4 runs state 5 | Area172_Tail35 299 |
| E79 | `Area172_Tail35` | state 6 counter not tested | Area172_Tail35 191 |
| E80 | `Area172_StepHook` | Cond_ByteFD 1 too | Area172_StepHook 43 |
| E81 | `Area172_StepHook` | x 0x248001 | Area172_StepHook 111 |
| E82 | `Area172_StepHook` | window from 0x36 | Area172_StepHook 53 |
| E83 | `Area172_StepHook` | pose 0 dropped | Area172_StepHook 20 |
| E84 | `Area172_StepHook` | state 1 | Area172_StepHook 75 |
| E85 | `Area172_StepHook` | pose 5 added | Area172_StepHook 24 |
| E86 | `Area172_InitCell` | cell (9, 4) | Area172_InitCell 6000 |
| E87 | `Area172_InitCell` | value 0x51 | Area172_InitCell 6000 |
| E88 | `Area172_EffectA5Start` | +0x2E 1 | Area172_EffectA5Start 6000 |
| E89 | `Area172_EffectA5Start` | +0x10 cleared not +0xC | Area172_EffectA5Start 6000 |
| E90 | `Area172_EffectA5Start` | state 2 | Area172_EffectA5Start 6000 |
| E91 | `Area172_EffectA5Grow` | + 11 | Area172_EffectA5Grow 5977 |
| E92 | `Area172_EffectA5Grow` | >= 0x190 | Area172_EffectA5Grow 454 |
| E93 | `Area172_EffectA5Grow` | unsigned compare | Area172_EffectA5Grow 1537 |
| E94 | `Area172_EffectA5Grow` | Var7b + 2 | Area172_EffectA5Grow 2491 |
| E95 | `Area172_EffectA5Grow` | state 3 | Area172_EffectA5Grow 2469 |
| E96 | `Area172_EffectA5Grow` | panel x 0xDD | Area172_EffectA5Grow 6000 |
| E97 | `Area172_EffectA5Grow` | the width not read again | Area172_EffectA5Grow 3103 |
| E98 | `Area172_EffectA5Grow` | shade y 0xC9 | Area172_EffectA5Grow 6000 |
| E99 | `Area172_EffectA5Hold` | panel y 0xC7 | Area172_EffectA5Hold 6000 |
| E100 | `Area172_DrawPanel` | x0 + 1 | Area172_DrawPanel 6000 |
| E101 | `Area172_DrawPanel` | u1 0x3F | Area172_DrawPanel 6000 |
| E102 | `Area172_DrawPanel` | v2 0x21 | Area172_DrawPanel 6000 |
| E103 | `Area172_DrawPanel` | x1 + 0x3F | Area172_DrawPanel 6000 |
| E104 | `Area172_DrawPanel` | y2 + 0x21 | Area172_DrawPanel 6000 |
| E105 | `Area172_DrawPanel` | clut y 0x1EC | Area172_DrawPanel 6000 |
| E106 | `Area172_DrawPanel` | tpage x 0x340 | Area172_DrawPanel 6000 |
| E107 | `Area172_DrawPanel` | g 0x81 | Area172_DrawPanel 6000 |
| E108 | `Area172_DrawPanel` | commit 0x4C | Area172_DrawPanel 6000 |
| E109 | `Area172_DrawPanel` | x zero-extended | Area172_DrawPanel 3025 |
| E110 | `Area172_DrawPanel` | u2 before the prim setter | Area172_DrawPanel 5969 |
| E111 | `Area172_DrawPanel` | y3 + 0x1F | Area172_DrawPanel 6000 |
| E112 | `Area172_DrawShade` | tpage & 0xFFF | Area172_DrawShade 5631 |
| E113 | `Area172_DrawShade` | tw 1 | Area172_DrawShade 6000 |
| E114 | `Area172_DrawShade` | commit 0xD | Area172_DrawShade 6000 |
| E115 | `Area172_DrawShade` | the packet pointer not read again | Area172_DrawShade 6000 |
| E116 | `Area172_DrawShade` | x3 320 + 1 bit | Area172_DrawShade 6000 |
| E117 | `Area172_DrawShade` | x0 - 0x3F | Area172_DrawShade 6000 |
| E118 | `Area172_DrawShade` | x2 + 1 | Area172_DrawShade 6000 |
| E119 | `Area172_DrawShade` | y3 + 0x31 | Area172_DrawShade 6000 |
| E120 | `Area172_DrawShade` | r1 0xFE | Area172_DrawShade 6000 |
| E121 | `Area172_DrawShade` | b2 0x41 | Area172_DrawShade 6000 |
| E122 | `Area172_DrawShade` | not semi-transparent | Area172_DrawShade 6000 |
| E123 | `Area172_DrawShade` | shade-tex 1 | Area172_DrawShade 6000 |
| E124 | `Area172_DrawShade` | commit 0x40 | Area172_DrawShade 6000 |
| E125 | `Area172_DrawShade` | b0 before the prim setter | Area172_DrawShade 5972 |
| E126 | `Area172_DrawShade` | y zero-extended | Area172_DrawShade 3007 |
| E127 | `Area172_DrawShade` | dtd 0 | Area172_DrawShade 6000 |
| C49b | `Area170_Tail37` | SpawnEffect13: the scratch byte stored as slot + 1 (read back) | Area170_Tail37 133 |
| D13b | `Area171_Leader89Gate` | a key mismatch arms tail 48 at 10 | Area171_Leader89Gate 883 |

## 5. The tables named

| Address | Name | What |
|---|---|---|
| `0x63C8DC` | `Area168_Choices` | area 168's nine choice slots |
| `0x63C944` | `Area168_FocusPairs` | byte pairs by the choice answer (signed, x 2) |
| `0x63CA3C` | `Area169_Rects` | two five-byte rectangles (x0, z0, x1, z1, facing) |
| `0x63D5DC` | `Area170_Choices` | six choice slots; 4 and 5 are the handlers |
| `0x63D5EC` | `Area170_Handlers` | two handlers |
| `0x63D63C` | `Area170_InputSwap` | 16 bytes: the held word's top nibble replaced by its entry |
| `0x63D7FC` | `Area171_Choices` | three choice slots; 1 and 2 are the handlers |
| `0x63D800` | `Area171_Handlers` | two handlers |
| `0x63D84C` | `Area171_Rects` | area 169's layout |
| `0x63EEAC` | `Area172_Choices` | nine choice slots; 1..8 are the handlers |
| `0x63EEB0` | `Area172_Handlers` | eight handlers |
| `0x63EF14` | `Area172_FallStates` | two states; the slide's two follow at once |
| `0x63EF1C` | `Area172_SlideStates` | two states; the drift steps follow |
| `0x63EF24` | `Area172_DriftSteps` | 16 signed steps (they repeat every four bytes) |
| `0x63EF34` | `Area172_EffectStates` | effect kind `0xA5`'s three states; area 173's data block (`0x63EF40`, its descriptor's `+0x20`) follows |

The state tables start right after area 172's descriptor (`0x63EED0`, its
`+0x40` the init at `0x63EF10`), which is why the tool's walk found them
among area 173's pointers in descriptor order (the round doc's note of 7
pointers moved into area 172).

## 6. Latent defects

Described, not fixed; ours reproduces each faithfully, or aborts where the
original would jump into data.

- **Area 172's slide dispatcher at state 2.** `Area172_FallStep` sets the
  running object's `+4` to 2 when it lands. `Area172_RunFall` then reaches
  `Area172_SlideStates[0]` through the run-on (its table is back to back with
  the slide's, so states 2 and 3 are the slide's code - kept, ours reaches
  them too). But `Area172_RunSlide` on an object at state 2 (a script that
  runs handler 3 on an object handler 2 dropped) reads the first four drift
  steps as a code address, which lies outside `.text`, and faults; ours
  aborts with a message. Nothing measured reaches it.
- **`Area172_RunFall` past 4 and `Area172_EffectA5Run` past 3** read the
  drift steps, or area 173's first data dword, as a code address outside
  `.text` the same way; ours aborts. The fall and slide states write 0..2, the effect states 1 and
  2; a state byte written elsewhere (the movement script's `+4`, an effect
  record's `+1`) is not.
- **Area 170's tail state 51** (after message `0xA`) has no case: the tail
  idles until another writer (area 170's choice 3) moves the state. And
  state 53 has no exit of its own: it swaps the held input's top nibble
  every frame `Field_Request` is 0 until choice 3 writes 52 or 55. Both read
  as intended waits on a message's choice, not as defects; recorded because
  the code alone does not say so.
- **Area 170's step hook, x `0x218000`**, writes tail kind 37 and state 60
  without `ScriptFlags_Set40` and answers `al` 0, where every other path
  arms through `ScriptFlags_Set40` and answers 1. State 60 itself calls
  `ScriptFlags_Clear40`.
- **Area 170's trigger 56** pushes a fourth word (0) to `Inventory_Add`,
  which takes three; harmless under cdecl.
- **Area 168's choice 1** indexes `Area168_FocusPairs` by the signed answer
  unchecked (it stays in `.data` for any byte).
- **Area 172's choice 0** calls `ScriptFlags_Set40` on answer 0 but arms
  tail kind 35 only when Cond row 14's flag `0x12` is set; on the other path
  it writes `MoveScript_Var7` and leaves the script flag to whoever clears
  it next. Whether the movement script does is not read here.

## 7. What the tool listed, against the reading

The tool's 56 starts are 56 functions: no jump-table or switch case among
them, no start missing. Each clone row agrees with the reading (extents,
every `E8` / `E9`, the five in-function jump tables: area 168's two-level
13-byte / six-entry, area 169's four, area 170's two-level 61-byte /
26-entry, area 171's two-level 22-byte / eight-entry, area 172's seven).
The `.data` notes agree: `0x63EF14` four code entries (the fall's two and
the slide's two), `0x63EF1C` two, `0x63EF34` three. The gaps the tool
reached by name - the member frames (`0x46D7C3`, `0x46D7D0`), their
searches, the four object triggers and effect kind `0xA5`'s handler - are
real functions; the byte-by-byte E8/E9 scan's one hit into the band from
outside it that is not a call (`0x41ED1A` into `0x426D1F`) is the
immediate of a `sub ecx, 0x8000`, not an instruction.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 56). Every function is reached only in play: the choices
  when the area's message box asks, the handlers from its movement scripts,
  the step and arrive hooks on a step or an arrival in the area, the tails
  once armed, the triggers by an object's trigger id, the inits on entry,
  the member frames every field frame in areas 169 and 171, effect kind
  `0xA5`'s states while such an effect lives. Fuzz only; the live check is
  the owner's, when a save in these areas exists.

## 9. Calls across groups

- **Raw-address callee nobody owns:** `0x486D60` (engine code called only by
  `Area170_Init`: a `void (void)` map set-up over tables of the `0x63CAxx`
  block - story flag `0x7E`, map bytes, map items; not read further here).
  No call into another group's band.
- **Inbound calls from outside the band**, for the rebinding pass:
  `0x46D7C3` / `0x46D7D0` (`0x46D780`, the per-area member frames) into
  `Area169_MembersFrame` / `Area171_MembersFrame`; `Area_StepHook`'s
  `kStepHandlers` in `event_ops.cpp` (`0x427270`, `0x427A80`, `0x4281A0`;
  the original's `0x56E1C2`, `0x56E1D2`, `0x56E1E2`) and `kArriveHandlers`
  (`0x426B20`, `0x427B50`; `0x56E57D`, `0x56E593`). Read in place, no
  rebinding: `Field_ModeTailKinds` entries 35, 37, 47, 48, 59
  (`0x662D74`, `0x662D7C`, `0x662DA4`, `0x662DA8`, `0x662DD4`),
  `Field_ObjectTriggers` ids 19, 20, 55, 56, `Effect_KindHandlers[0xA5]`
  (`0x6555E4`), and the descriptors' own arrays.
- **Named by another group's table:** area 168's choice 2 is `0x425C30`
  (area 167's block, AR4A's this wave), read in place.
