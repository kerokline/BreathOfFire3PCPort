# World 4, areas 188..191: the band `0x42A320..0x42BD60`

**Status:** IN PROGRESS (2026-09-28) - 51 functions ours
(`src/game/area_w4e.cpp`, shadow name `area_w4e`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area
with code: 0 mismatches in 306,000 rounds (in this worktree); CONTROLS_SUMMARY
(section 4). Fuzz only: no recorded route reaches the band (section 8). No
divergence; the two state dispatchers abort past their tables and area 189's
step aborts on a divisor of 0, where the original would jump into data or
fault (section 6).

Group AR4E of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)): 51
starts, none ours before, **51 taken**; no start dropped, none added (section
7). **Area 190 has no code**: its descriptor `0x647658` has no `+0x34`,
`+0x3C` or `+0x40`, and the PSX `BIN/WORLD04/AREA190.EMI` holds 4 code bytes
(the sibling's `analysis/area_pcs.json`); the fifteen functions the tool
files under area 190 are area 189's (section 7).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`area_funcs.tsv`) and agrees with
the reading; the clone tables are `area_rows.py --clones`'s rows, each read
against the disassembly. What an area *is* in the story is not read here.
The PSX twins are the sibling's `names/area_records.toml` (descriptor
handlers and inits); `analysis/pairs_propagated.json`'s pairings are cited in
`symbols.toml` as hypotheses where they are clear (every pairing in the band
lies in the section of the area named here: `c787edb8` AREA188, `b880f463`
AREA189, `1091d89f` AREA191).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a step hook
`(x, z)` answering in `al`, `kState` a state handler reached through a table
(the area's `.data`, `Field_LeaderStates` or `EffectKind18_States`),
`kCallee` a function called directly. A function that is both a choice and a
handler is fuzzed as a handler.

### Area 188 (descriptor `0x6474E0`; PSX `0x801F6848`)

Fourteen choices (`Area188_Choices` `0x64743C`) and ten handlers
(`Area188_Handlers` `0x64744C`: choices 4..13 are the handlers). Handler 1 =
choice 5 is `Area136_ShiftCameraUp2` (`0x41F320`, AR3E's) and handler 3 =
choice 7 `Area80_ResetCameraShift` (`0x40F530`, AR2A's): table entries read in
place, called by nothing here.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42A320` | `Area188_ChoiceTail43A` | `0x2D` | choice 0 | kChoice | message `0xFFFF`; answer 0: `ScriptFlags_Set40`, tail kind 43 at state 0, sub-kind 0 |
| `0x42A350` | `Area188_ChoiceTail43B` | `0x2D` | choice 1 | kChoice | the same with sub-kind 1 |
| `0x42A380` | `Area188_ChoiceByChapter` | `0x5A` | choice 2 | kChoice | message `0xFFFF`; `Cond_ByteFA` (s8) above 12: answer 0 arms tail kind 10 (engine `0x56DB80`) at state 0, sub-kind `0xFF`, **without** `ScriptFlags_Set40`; at most 12: answer (s8) 0 `ScriptFlags_Set40`, counter 0 = `0xA`; 1 the same with 5 |
| `0x42A3E0` | `Area188_ChoiceFocusPair` | `0x69` | choice 3 | kChoice | the focus object (`0x903804`) `+0x18` / `+0x1C` = `Area188_FocusPairs[answer]`'s two bytes as dwords (the answer s8, unchecked), message `0xFFFF`; `Cond_ByteFA` at most 12 and answer 1: the byte `0x929F0F` 0, `ScriptFlags_Set40`, step `0x14`, `MoveScript_Var7` 8 |
| `0x42A450` | `Area188_WalkWhileZUnder` | `0x53` | handler 0 = choice 4 (PSX `0x801F4840`) | kHandler | the first dword of the engine's direction record `0x6697B4 + (+8 & 7) * 8` plus the running object's z, unsigned, at most `Area188_ZLimits[the tail's sub-kind] << 16`: the script object's `+7` = 1, `MoveCmd_Move(script object, 5)`, the script back 2 |
| `0x42A4B0` | `Area188_ShiftCameraDown2` | `0x10` | handler 2 = choice 6 (PSX `0x801F4908`); area 119 choice 3 = handler 2, area 131 handler 6, area 136 choice 13 = handler 9 | kHandler | `Camera_ShiftY` - 2, `MapView_Redraw` 2 (`Area136_ShiftCameraUp2` the other way) |
| `0x42A4C0` | `Area188_PatchCellsA0` | `0x30` | handler 4 = choice 8 (PSX `0x801F494C`) | kHandler | `AreaMap_SetByte`: (0xF, 0x3C) and (0x10, 0x3C) to 0, (0xF, 0x3B) and (0x10, 0x3B) to `0x51` |
| `0x42A4F0` | `Area188_PatchCellsA1` | `0x3C` | handler 5 = choice 9 (PSX `0x801F49A4`) | kHandler | the same four cells to `0xC0`, `0xC0`, `0xA1`, `0xA1` |
| `0x42A530` | `Area188_PatchCellsB0` | `0x30` | handler 6 = choice 10 (PSX `0x801F49FC`) | kHandler | (0x1C, 3) and (0x1D, 3) to 0, (0x1F, 0x15) and (0x1F, 0x16) to `0x50` |
| `0x42A560` | `Area188_PatchCellsB1` | `0x3C` | handler 7 = choice 11 (PSX `0x801F4A54`) | kHandler | (0x1C, 3) and (0x1D, 3) to `0xC0`, (0x20, 0x15) and (0x20, 0x16) to `0xA1` |
| `0x42A5A0` | `Area188_ClearCellsC` | `0x46` | handler 8 = choice 12 (PSX `0x801F4AAC`) | kHandler | six cells to 0: (0x38, 0x47), (0x38, 0x48), (0x39, 0x47), (0x39, 0x48), (0x37, 0x3F), (0x37, 0x40) |
| `0x42A5F0` | `Area188_SpawnEffectB9` | `0x28` | handler 9 = choice 13 (PSX `0x801F4B24`) | kHandler | `Effect_FindFree`; not `0xFF`: the record's `+0` = 1, `+5` = `0xB9` (effect kind `0xB9`) |
| `0x42A620` | `Area188_Tail43` | `0x17A` | tail kind 43 | kTail | the sequence below |
| `0x42A7A0` | `Area188_Init` | `0x2F` | init (PSX `0x801F4D88`) | kInit | the byte `0x905E68` 1 with `Cond_ByteFD` 4: `Sprite_ObjectsExtra` record 0's `+0x83` = `0xC`; `0x905E68` 4 with `Cond_ByteFD` 1: `0xE` |

**Tail kind 43** switches on the s8 state through a byte table of 14
(`+0x16C`) into a jump table of 8 (`+0x14C`), both inside the extent; the
`ja` is unsigned, so a negative state and 3..9, past `0xD` do nothing. 0: state
`0xA` with key item 6, else 1. 1 (once `Field_Request` is not 2): message
`0x83`, request 2, state 2. 2 (once not 2): kind, state, sub-kind 0, then
`ScriptFlags_Clear40`. `0xA` (once not 2): message `0x82`, request 2, story
flag `0x69` toggled (`Flags_Toggle`), state `0xB`. `0xB` (once not 2):
`Party_DropIn(6)`, state `0xC`. `0xC` (once counter 3 is `0x28`):
`Field_ChangeArea(0xBC, 0x330000, 0x470000, 0x87)` for sub-kind 0, else
`(0xBC, 0x80000, 0x220000, 0x88)`; state `0xD`. `0xD` (once counter 3 is 0):
`ScriptFlags_Clear40`, then kind, state, sub-kind 0.

### Area 189 (descriptor `0x647558`; PSX `0x801F62AC`)

Two choices (`Area189_Choices` `0x647550`), no handlers, an init; and the
walk the area compiles as **leader state 13**: `Field_LeaderStates`
(`0x660918`) entry 13 is `0x42A8B0` itself (entry 12, `0x52FE90`, is areas
104 and 121's, [`area_w3b.md`](area_w3b.md) section 5). What puts the leader
in state 13 is not read here. Sprite_Current is the leader.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42A7D0` | `Area189_Init` | `0x1E` | init (PSX `0x801F4C38`) | kInit | story flag `0x82` set: `Draw_PassFlags` `0x1F` |
| `0x42A7F0` | `Area189_ChoiceTail50A` | `0x26` | choice 0 | kChoice | message `0xFFFF`; answer 0: `ScriptFlags_Set40`, tail kind 50 at state 0 |
| `0x42A820` | `Area189_ChoiceTail50B` | `0x26` | choice 1 | kChoice | the same at state 1 |
| `0x42A850` | `Area189_Tail50` | `0x5F` | tail kind 50 | kTail | once `Field_Request` is not 2: `ScriptFlags_Clear40`; `Field_StatusBits` bit 0 cleared; state not 0 `Field_ChangeArea(0x96, 0x180000, 0x300000, 1)`, else `(0xC1, 0x1A8000, 0x1C0000, 1)`; kind, state 0 |
| `0x42A8B0` | `Area189_LeaderRun` | `0x12` | `Field_LeaderStates[13]` (`0x66094C`; a gap of the tool) | kState | `Area189_LeaderStates` `0x6475A0` by `+2` (5; ours aborts past) |
| `0x42A8D0` | `Area189_LeaderStart` | `0x1A7` | leader state 0 | kState | the walk set up (below) |
| `0x42AA80` | `Area189_LeaderProject` | `0x102` | leader state 1 | kState | the kind-2 point projected (below) |
| `0x42AB90` | `Area189_LeaderControl` | `0x195` | leader state 2 (and `Area189_StepArrive`'s tail jump) | kState | the input (below) |
| `0x42AD30` | `Area189_LeaderStep` | `0x28` | leader state 3 | kState | `Field_State +0x137` = 1; `+9` at 0: a tail jump to `Area189_StepArrive`; else `+9 - 1` and a tail jump to `Area189_StepMove` |
| `0x42AD60` | `Area189_LeaderTurn` | `0x4D` | leader state 4 | kState | `Cond_AngleFB`'s low word = (it + the s8 `+0xB`) & `0xFFF`; equal to `((2 - +8) & 0xF) << 8`: `+0x137` = 0, `+2` = 2 |
| `0x42ADB0` | `Area189_StepBegin` | `0x7A` | called by the control | kCallee | `+9` = 8; `+0xC` / `+0x10` = `Area189_StepVectors[+8]` doubled (the byte unchecked); `+0x14` = ((the height `0x511C10` at the point `+9` steps on) << 16 - `+0x3C`) / `+9` (idiv; `+9` read after the call) |
| `0x42AE30` | `Area189_StepMove` | `0x4A` | called by the control and the step | kCallee | x += `+0xC`, `Field_Kind2X` = x; z += `+0x10`, `Field_Kind2Z` = z; `+0x3C` += `+0x14`; `MapView_Redraw` 2 |
| `0x42AE80` | `Area189_StepArrive` | `0x46C` | the step's tail jump | kCallee | the arrival (below) |
| `0x42B2F0` | `Area189_LeaderHalt` | `0x26` | called and tail-jumped to | kCallee | `Area189_ZeroSpeeds`; `+0x4B` = 0; `Field_State +0x137` = 0; `+2` = 2 |
| `0x42B320` | `Area189_ExitButton` | `0xD1` | called by the control | kCallee | `Input_Pressed` bit 11: the pending place `Area189_ExitPlaces[Cond_ByteFF 0]` (area, x, z bytes, the coordinates `<< 16`), flags 4; the leader's x, z and `Game_AreaNumber` saved (`0x904148` / `0x90414C` / `0x904150`), the byte `0x904152` + 1; the facing jittered; `Field_ChangeArea` from the pending cells; al 1. Else al 0 |
| `0x42B400` | `Area189_MenuButton` | `0x3A` | called by the control | kCallee | `Field_MenuButton` against `Input_Pressed` outside `Field_ScriptFlags` bit 6: `Sound_PlayEffect(0x105)`, `Field_Request` 1, `+2` = 2, al 1 (`Area121_MenuButton` sets `+2` to 0) |
| `0x42B440` | `Area189_ZeroSpeeds` | `0x1E` | called by three | kCallee | `+0xC`, `+0x10`, `+0x14` = 0 |
| `0x42B460` | `Area189_TurnInput` | `0x77` | called by the control | kCallee | `Input_Held` bit 12: `+0x4B` = 1, al 2; bit 13: `+0x4B` = 2, `+8 + 1`, `+0xB` = `0xF0`; else bit 15: `+0x4B` = 3, `+8 - 1`, `+0xB` = `0x10` (either: `+8 &= 0xF`, al 1); none al 0 |
| `0x42B4E0` | `Area189_RaiseByte1E` | `0x2C` | called by the arrival | kCallee | for seven `CharacterRecords` (`0xA4` each): the max-HP scale `+0x1E` ([`char-stats.md`](char-stats.md) section 2) below 9: + 1 and `Char_RecalcStats` |
| `0x42B510` | `Area189_DrainHp` | `0x3F` | called by the arrival | kCallee | for seven `CharacterRecords`: d = (base max HP `+0x40` * 2 + `0x32`) / 100; HP `+0x18` above d: - d; else 1 |

**The facings are sixteen**: `Area189_TurnInput` keeps `+8 & 0xF`,
`Area189_StepVectors` holds sixteen (x, z) dword pairs (a circle of radius
`0x1000`, `0x80` bytes to `0x647634`), and the angle `Cond_AngleFB` is
`0x100` a facing (`((2 - facing) & 0xF) << 8`).

**State 0** (`Area189_LeaderStart`): `Gte_SetGeomOffset(0xA0, 0xB9)`,
`Gte_SetGeomScreen(0x12C)`; `Camera_Angles[0]` `0xFD12`; the facing `+8` = the
byte `0x904153`; `Camera_Distance` `0xF280`; `MapView_Redraw` 4;
`Cond_AngleFB`'s low word `((2 - that byte, read again) & 0xF) << 8`;
`Field_Kind2X` / `Z` the leader's x / z; `+0x3C` = the height at (x, z)
(`0x511C10`, in ax) << 16; `Area189_ZeroSpeeds`; `+0x24`, `+0x4B` 0;
`Field_EdgeBits` 0. **The pace**: `(Rand() & 0x1F) + 0x96`, doubled when any
of `Party_Count(0)`'s members answers `Actor_EquipCount(member, 3, 0x15)`,
halved (arithmetic) when any answers `(member, 3, 0x14)`, doubled again with
`Cond_ByteFF` 0; the word `0x802E74` (the leader record's `+0x134`) = the
pace. Story flags `0x75`, `0x76`, `0x8B..0x8E` cleared; `+2 + 1`.

**State 1** (`Area189_LeaderProject`): `Prim_VertexScratch` = (x, z, y):
`Field_Kind2X` and `Z` each `((v - 1) & 0xFFFFFF) + 1 >> 9, - 0x4000`, and
`-(s16 +0x3E / 2)` (toward 0); `Gte_RotTransPers` of it to `MapView_ScreenXY`
(the call pushes a fourth argument, a local); `Gte_StoreDepthF(+0x7C)`;
`+0x78` = the screen point's y through `fld` / `fstp` (a signalling NaN comes
out quiet; ours is the two instructions); `+0x14` = 1. `Cond_Flags` row 14
(`0x904000`) bit 9 set and bit `0xA` clear: `Effect_Objects[0]` bit 6, `+2` =
3, `+0x10` = `-0x2000`, `+9` = `0x78`, the word `0x8034E6` = `0x3C`. Else `+2` =
2.

**State 2** (`Area189_LeaderControl`): nothing under `Field_ScriptFlags` bit
8, `Field_ScriptFlags2` bit 6, a `Field_Request`, or while
`Area189_ExitButton` or `Area189_MenuButton` answers. `Area189_TurnInput`: 0
- `+9` = 0 and the halt; 1 - `Field_State +0x137` = 1, `Cond_AngleFB`'s low
word = (it + the s8 `+0xB`) & `0xFFF`, `+2` = 4 (state 4 turns until the
angle meets the facing); 2 - `Area189_StepBegin`, then the point `+9` steps
on (x + `+0xC` * `+9`, z + `+0x10` * `+9`, 32-bit) by its blocks (the high
words with the low byte cleared, signed): x `0x1000..0x1400` with z from
`0x1800` - message 2; `Cond_Flags` row 14 bit 4 with x `0x1500..0x1900` and z
at most `0xE00` - message 3 (each with request 2 and the halt); else `+9 -
1`, `Area189_StepMove`, `+0x137` = 1, `+2` = 3. Any other answer: nothing.

**The arrival** (`Area189_StepArrive`, at the end of each step):
`Area189_ZeroSpeeds`; the frame word `0x90405C` + 1 and `Field_EdgeBits` + 1.
At `0x1E0`: the reserve `0x90405F` + 8 if not 0, `Field_StatusBits` bit 0
and `Cond_ByteFF` cleared, message 5, request 2. At `0x3C0` (read again): the
reserve + 8 if not 0, bit 0 and `Cond_ByteFF` set, the frame word 0, the
count `0x90405E` + 1, message 6, request 2. Then the reserve, if not 0,
counts down 3 (`Cond_ByteFF` 0) or 1 a step, not below 0; the bytes
`0x929EC1` / `0x9036D0` 0; at 0 message 1 and the halt. With no reserve, an
event after `(Rand() & 7) + 20 * Cond_ByteFF + 7` steps (`0x929EC1` counts
them): the fifth in a row (`0x9036D0` at 4) `Area189_RaiseByte1E`,
`Cond_ByteFE` 2, sound `0x10B`; the others `Area189_DrainHp`, `Cond_ByteFE` 1,
sound `0x108`. A request of 2: the halt. `Cond_Flags` row 14 bit 4 clear with
the x block `0x1600` or `0x1700` and the z block at most `0x1000`:
`Scena14_LeaveToC4` (SC13's) and the halt. `Scenario_ArriveHook(x, z)`
answering (all of eax): the halt. The blocks moved one inward at the edges (x
at most `0x900` or from `0x1F00`, z at most `0xD00` or from `0x1900`);
`Field_Kind2X` / `Z`; `MapView_Redraw` 2. Outside `Field_ScriptFlags` bit 5
and with `Field_EdgeBits` at the pace: the edge count 0; story flags `0x75`,
`0x76`, `0x8B`, `0x8C`, `0x8D`, `0x8E` for the blocks (`0xB00`, `0xF00`),
(`0x1500`, `0x1100`), (`0xC00`, `0x1400`), (`0x1000`, `0x1100`), (`0x1D00`,
`0x1700`), (`0x1400`, `0x1400`); area `0x7D` at (`0x190000`, `0x190000`),
flags 4, the facing jittered, `Field_ChangeArea`, the halt. Else under
`Field_ScriptFlags` bit 8 or `Field_ScriptFlags2` bit 6 the halt; a held
button (`Field_InputHeld` against the word `0x903580 | 0xB000`) goes straight
on (a tail jump to `Area189_LeaderControl`); else the halt.

**The facing jitter**: `(Area189_FacingJitter[Rand() & 3] + +8) & 0xF` to
`0x904153` (the jitter -1, 0, +1, 0), which state 0 takes as the facing.

### Area 190 (descriptor `0x647658`)

No choices, handlers, init or hook: no code.

### Area 191 (descriptor `0x647BC8`; PSX `0x801F418C`)

Nine choices (`Area191_Choices` `0x647BA4`) and three handlers
(`Area191_Handlers` `0x647BBC`: choices 6..8). Choice 4 is `0x42BDA0` (area
192's block, AR4F's), read in place. Area 192's descriptor names
`0x42B550`, `0x42BD30`, `0x42B5B0`, `0x42B610`.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42B550` | `Area191_ChoiceMessage68` | `0x18` | choice 0 (and area 192's) | kChoice | message `0x68` for answer 0, else `0xFFFF` |
| `0x42B570` | `Area191_ChoiceFocusPair` | `0x3C` | choice 2 | kChoice | the focus object's `+0x18` / `+0x1C` from `Area191_FocusPairs[answer]`, message `0xFFFF` (area 188's choice 3 without its tail) |
| `0x42B5B0` | `Area191_ChoiceMessage6C` | `0x18` | choice 3 (and area 192's) | kChoice | message `0x6C` for answer 0, else `0xFFFF` |
| `0x42B5D0` | `Area191_ChoiceTail53` | `0x3E` | choice 5 | kChoice | message `0xFFFF`; answer 1: `ScriptFlags_Set40`, tail kind 53 at state `0x1E`; 2: `ScriptFlags_Set40`, `MoveScript_Var7` 1, step `0x14` |
| `0x42B610` | `Area191_MemberNext` | `0x26` | handler 0 = choice 6 (PSX `0x801F30DC`; area 192's too) | kHandler | `ScriptFlags_Set40`; `Field_ActiveMember` (read after) `+0x80` bit 0 cleared, word `+0x8A` + 1 |
| `0x42B640` | `Area191_MemberNextRun` | `0x34` | handler 1 = choice 7 (PSX `0x801F3130`) | kHandler | the same, then `MoveScript_Var7` 2, step `0x19` |
| `0x42B680` | `Area191_RunScale` | `0x12` | handler 2 = choice 8 (PSX `0x801F31A0`) | kHandler | `Area191_ScaleStates` `0x647C18` by `+4` (3; ours aborts past) |
| `0x42B6A0` | `Area191_ScaleStart` | `0x6A` | scale state 0 | kState | `+0x48` = 2, `+0x40` / `+0x44` = `0x10000`, `+0xA` = `0xFF`, dword `+0x70` = 1, `+0x5D..+0x5F` = `0x50`, `0x3C`, 0, `+4` = 1, the script back 2 |
| `0x42B710` | `Area191_ScaleGrow` | `0x68` | scale state 1 | kState | x += `+0xC`; `+0x40`, `+0x44` + `0x2000`; `+0xA` - 1; its low two bits 0: `+4` = 2; `+0xA` 0: `+4` = 0, else the script back 2 |
| `0x42B780` | `Area191_ScaleShrink` | `0x68` | scale state 2 | kState | the same with `- 0x2000` and `+4` = 1 |
| `0x42B7F0` | `Area191_Tail53` | `0x1F7` | tail kind 53 | kTail | the sequence below |
| `0x42B9F0` | `Area191_StepHook` | `0x95` | `Area_StepHook`'s case for area `0xBF` (`0x56E2B2`) | kHook | the cells the step covers - (x, z) by their high words, the next in x when x's low word is not 0, in z when z's is not, the diagonal when both - counted where `AreaMap_ByteAt` answers `0xA6`; any: `ScriptFlags_Set40`, tail kind 53 at state 0, al 1 |
| `0x42BA90` | `Area191_TalkMessage` | `0x11B` | called by chapter code (SC13 `0x5677F3`, `0x567833`, `0x567873`, `0x5678B3`, `0x5678F3`; SC15 `0x56AEE1`; a gap of the tool) | kCallee | the message a member's talk opens, in ax (below) |
| `0x42BBB0` | `Area191_TalkMessageB` | `0x3B` | called by the talk (a gap) | kCallee | (a, who, b, c): a + `Area191_TalkStepsC[b]` for c below 5, else a + 8; the byte of `Area191_TalkMessagesC` by it; `who` not read |
| `0x42BBF0` | `Area191_Init` | `0xE4` | init (PSX `0x801F3964`) | kInit | with `0x42BC20`, its tail-jumped body (below) |
| `0x42BCE0` | `Area191_Kind18Flag77` | `0x45` | `EffectKind18_States[77]` (`0x6541A0`; a gap) | kState | story flag `0x77` set: the walk's frame word, `0x929EC1`, `0x9036D0` 0, the count `0x90405E` + 1, `Field_StatusBits` bit 0; a tail jump to `Effect_Release` |
| `0x42BD30` | `Area191_ChoiceRun1` | `0x26` | choice 1 (and area 192's) | kChoice | message `0xFFFF`; answer 0: `ScriptFlags_Set40`, `MoveScript_Var7` 1, step `0x14` |

**Tail kind 53** switches on the s8 state through a byte table of 31
(`+0x1D8`) into a jump table of 8 (`+0x1B8`): 0 - message `0x6D`, request 2,
state 1 (state 1 does nothing; what moves it on is not read here - area
191's choice 5 arms the kind at `0x1E`). 2 (once
`Field_Request` is not 2) - `Transition_Start(0)`, state 3. 3 (once
`MoveScript_WaitWordDA` is 0) - `Draw_PassFlags` 0, `Sound_StopMusic`,
`0x42C2D0` (area 192's block: each actor record with `+0xB` bit 0 gets its words
`+0x20` / `+0x22` at `+0x18` / `+0x1A` and `+0x10` 0, then each party
member's actor record is copied to its party record), `Sound_LoadStream(0)`,
state 4. 4 (once `Sound_StreamDone` answers, all of eax) - story flag `0x82`;
the walk's frame word, `0x929EC1`, `0x9036D0` 0; its count + 1;
`Field_StatusBits` bit 0; state `0xA`. `0xA` (once not 2) -
`ScriptFlags_Clear40`, `Field_ChangeArea(the saved area, x, z, 4)` (the three
area 189's exit button saved), `Field_ScriptFlags2` bit 6, the byte
`0x904152` 0, story flag `0x77` cleared, kind and state 0. `0x14` (once not 2)
- `ScriptFlags_Clear40`, kind and state 0. `0x1E` (once not 2) -
`ScriptFlags_Clear40`, `Field_ChangeArea(0x96, 0x180000, 0x300000, 1)`, the
same three, `Field_StatusBits` bit 0 cleared, kind and state 0. Every other
state (1, 5..9, `0xB..0x13`, `0x15..0x1D`, past `0x1E`, negative): nothing.

**The talk** (`Area191_TalkMessage(who)`): i = the index of `who`'s low byte
among `Area191_TalkKeys`' five (5 when none); a = 9i; b = the walk count
`0x90405E`, at most 8; c = the max-HP scale `+0x1E` of the actor record
`MoveScript_EffectState[who]` names (both indexes unchecked). With
`Cond_Flags` row 14 bit `0xA`: `Area191_TalkMessageB(a, who, b, c)`. Else a +
8 for c from 8, + 7 for c above 4 with b below 7, else +
`Area191_TalkSteps[b]`; the byte of `Area191_TalkMessagesB` (the chapter
row's flag 2) or `_A` by a, zero-extended to ax. The callers open that
message ([`scena_sc13.md`](scena_sc13.md), `scena_sc15.md`).

**The init** (`0x42BBF0` and the body `0x42BC20` it tail-jumps to, reached by
nothing else; the tool reads one function of `0xE4` and so does ours):
nothing with `Cond_ByteFA` `0xE` and the chapter row's flag 5, nor with story
flag `0x77`. Else a list k by actor record 0's max-HP scale (read first):
`Cond_Flags` row 14 bit `0xA` clear - 2 from 9, 1 from 5, else (the walk
count at least 5); set - 2 from 5, else (the count at least 5). Six times:
`Sprite_FindFree` to the word `DamageScratch`; not `0xFF`:
`EventOp_0x(Area191_ObjectLists[k] + 0x11 n)` (each list six 17-byte
records).

## 2. Ours

`src/game/area_w4e.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
two raw addresses of section 9). The group's own callees are called the same
way (`AH_CALL(Area189_StepBegin)`, `AH_CALL(Area191_TalkMessageB)`), so the
fuzz stands a recorder in for each and every function is tested alone; the
two state dispatchers read their `.data` tables in place and call the entry
(a tail `jmp` in the originals), so the fuzz's `DataTable` swap stands
recorders there. Shapes that repeat are one helper: the focus pair
(`FocusPair`, areas 188 and 191), the map patches (`SetCells`), the tail
arming and disarming, the run (`SetRun`), the scale's two ticks
(`ScaleTick`), the walk's pending change and facing jitter. Kept as the
originals: every re-read after a call (`Sprite_Current` after the height,
depth, `Rand`, flag and hook calls in area 189; `Field_ActiveMember` after
`ScriptFlags_Set40`; the step count after the height; the frame word after
message 5; `0x9036D0` after the sound; the saved place after
`ScriptFlags_Clear40`; the walk count and `Field_StatusBits` after the flag
calls), the order of every call, the signed 16-bit block compares, the
32-bit wrap of every coordinate sum and product, the word stores to
`Cond_AngleFB`'s low half, the float copy through the x87, and the unchecked
reads of section 6.

## 3. The fuzz

`BOF3X_SHADOW=area_w4e` (`src/game/area_w4e_fuzz.cpp`): three `Run`s under the
one shadow name, one per area with code, each with its `Group::area` (188,
189, 191), 6,000 rounds per function, the real descriptors and tables in
place.

- **Callees the group lists:** `ScriptFlags_Set40` / `Clear40`,
  `Msg_OpenScript`, `Sound_PlayEffect`, `Flags_Set` / `Clear` / `Toggle`,
  `KeyItem_Has`, `Field_ChangeArea` (the area masked to a word and the flags
  to a byte: the pushes before the calls at `0x42B2B6`, `0x42B3E3` and
  `0x42B8FB` carry stale bits above them), `MoveCmd_Move` (Capcom's), `Effect_FindFree` (`kByte
  0xFF..0x03`), `Party_Count` (`kByte 0..3`), `Actor_EquipCount` (the member
  a byte), `Gte_RotTransPers` (its vector by its six bytes, the local third
  argument masked, the screen point it writes), `Gte_StoreDepthF`,
  `Scenario_ArriveHook` (0, `0x100` or not 0), `Scena14_LeaveToC4`,
  `Char_RecalcStats`, `Transition_Start`, `Sound_StopMusic` (Capcom's),
  `Sound_LoadStream`, `Sound_StreamDone` (0, `0x100` or not 0),
  `AreaMap_ByteAt` (`0xA6` or beside it often), `Sprite_FindFree` (`kByte
  0xFF..0x1D`), `EventOp_0x`, `Effect_Release`, `0x511C10` and `0x42C2D0` by
  raw address, and the group's own called directly: `Area189_ZeroSpeeds`,
  `_StepBegin`, `_StepMove`, `_ExitButton` / `_MenuButton` (0 three times in
  four), `_TurnInput` (`kByte 0..3`), `Area191_TalkMessageB` (a, b, c by
  their low bytes, who not at all), and `Area189_LeaderHalt`, `_StepArrive`,
  `_LeaderControl`, `_RaiseByte1E`, `_DrainHp` as `kPhase` (logging the
  object they run for).
- **Louder stand-ins** (each part of the time, from `Noise`):
  `ScriptFlags_Set40` moves `Field_ActiveMember` and `Sprite_Current`;
  `ScriptFlags_Clear40` moves `Field_StatusBits`, the tail state and the
  saved place; `Msg_OpenScript` the walk's frame word (to `0x3C0` half the
  time); `Sound_PlayEffect` `0x9036D0` and `Sprite_Current`; `Flags_Set` /
  `Clear` `Sprite_Current`, the walk count and `Field_StatusBits`;
  `Scenario_ArriveHook`, `Gte_StoreDepthF`, `Area189_ZeroSpeeds`
  `Sprite_Current`; `Gte_RotTransPers` writes the screen point, its y a
  signalling NaN or its neighbours a quarter of the time; the height call
  moves `Sprite_Current` and its step count (kept above 0);
  `Area189_StepBegin` sets the count and small speeds; `Area189_StepMove`
  moves `Field_State`; `MoveCmd_Move` the script object; `Char_RecalcStats`
  another record's max-HP scale.
- **Data tables** swapped for recorders: `Area189_LeaderStates`,
  `Area191_ScaleStates`.
- **Regions beyond the field frame**, per area (24, 32 and 29 regions with
  the harness's twenty; 18,779, 20,132 and 17,543 bytes): all twenty
  `Effect_Objects` records (188, 189); `Camera_ShiftY` and the focus pointer;
  `0x929F0F`; the script object, active member and chapter row pointers;
  `Draw_PassFlags`; `Cond_ByteFF` .. `Input_Held` .. `Input_Pressed`;
  `Prim_VertexScratch`; `MapView_ScreenXY`; the button words to
  `Field_MenuButton`; `0x929EC1`, `0x9036D0`; `Cond_ByteFE`; `Camera_Angles`
  and `Cond_AngleFB`; eight `CharacterRecords`; `MoveScript_WaitWordDA`.
- **Seeds:** the answer at each value a choice tests, 1, 2, a negative byte;
  `Cond_ByteFA` 11..13 and signed edges; the tails at every state their tables
  name, their neighbours, the states that do nothing, negative bytes, with
  each state's wait (request 2, counter 3 at `0x28` / 0, the wait word 0) on
  and off; area 188's z at, below and above its limit less the direction
  dword, the sub-kind 0, 1 and past; the init's mode and `Cond_ByteFD` on
  and beside; area 189's state index in range; the kind-2 point's edges
  (`0x1000000` wrap, 0, -1); the y odd and negative; the control's point
  about both boxes' edges with small speeds; the turn's angle one step short
  of the facing's target or on it (with bits above `0xFFF`); the arrival's
  frame word at `0x1DF` / `0x3BF`, the reserve 0..4, the event counts on and
  beside their limits, the six marks, the exit blocks, the clamp edges, the
  edge count on the pace, half the rounds aimed at the walk's end; the exit
  and menu buttons pressed and not; `Input_Held`'s bits alone and together;
  the max-HP scale 7..10, HP on and beside the drain; the scale's count on
  its flips and at 1; the step hook's low words 0 or not; the talk's member
  among the keys (with stale bits above) and not, the walk count 0..10, the
  records' scale 4..9; the init's chapter `0xE` and not, record 0's scale 4,
  5, 8, 9, 10, the count 4..6.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3, the frame word, the reserve, `0x929EC1`, the wait word, the
  answer, `Cond_ByteFF`, the active member. A **settle** keeps area 189's
  step count above 0 while `Area189_StepBegin` runs (the original faults on a
  0 divisor, ours aborts; neither can be compared).

**Result (in this worktree):** 306,000 rounds over the 51 functions (6,000
each), 449,859 calls to the stand-ins (145,023 / 209,688 / 95,168), 0
mismatches. Coverage: every callee each function can reach was called - e.g.
`Area189_StepBegin` 389 (from the control), `Area189_TurnInput` 1,557, the
marks' `Flags_Set` 110, `Scena14_LeaveToC4` 62, `Area189_RaiseByte1E` 780 /
`_DrainHp` 283, `Field_ChangeArea` 107 (area 188) / 10,380 (189) / 660 (191),
`Sound_StopMusic` and `0x42C2D0` 167, `Area191_TalkMessageB` 3,975, every
leader and scale state about 1,150..2,050.

STAR_RESULT

## 4. Controls

CONTROLS_TEXT

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (20): `Area188_Choices`
`0x64743C` (14), `Area188_Handlers` `0x64744C` (10), `Area188_FocusPairs`
`0x647524` (12 bytes), `Area188_ZLimits` `0x647530` (2 bytes, read by the
sub-kind unchecked), `Area189_Choices` `0x647550` (2),
`Area189_FacingJitter` `0x64759C` (4 s8), `Area189_LeaderStates` `0x6475A0`
(5), `Area189_StepVectors` `0x6475B4` (32 dwords: sixteen (x, z) pairs),
`Area189_ExitPlaces` `0x647634` (2 x 3 bytes), `Area191_Choices`
`0x647BA4` (9), `Area191_Handlers` `0x647BBC` (3), `Area191_FocusPairs`
`0x647C0C` (12 bytes), `Area191_ScaleStates` `0x647C18` (3),
`Area191_TalkMessagesA` / `B` / `C` `0x647C24` / `0x647C54` / `0x647C9C` (48
bytes each), `Area191_TalkKeys` `0x647C84` (5), `Area191_TalkSteps`
`0x647C8C` / `Area191_TalkStepsC` `0x647CCC` (9 each), `Area191_ObjectLists`
`0x647E18` (3 pointers to six 17-byte `EventOp_0x` records each). As in the
other worlds, a descriptor's `+0x34` array is the tail of its `+0x3C` array
(areas 188, 191). The two tail kinds' tables are in `.text`, inside their
extents.

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area189_LeaderRun` (by `+2`) jumps through
  a five-entry table and `Area191_RunScale` (by `+4`) a three-entry one; an
  index past either jumps through the data after (`Area189_StepVectors`'
  first dword, `0xFFFFF4B0`; `Area191_TalkMessagesA`'s first bytes): not
  code. Ours aborts with a message there (the owner's rule: no DIVERGENCE
  entry). The states store only 0..4 and 0..2.
- **A divide by the step count.** `Area189_StepBegin` sets `+9` to 8, calls
  the height helper `0x511C10` (which writes nothing but its own arguments),
  then divides by `+9` read again: 8 in the game. Ours aborts on 0, which
  only a stand-in could cause.
- **The drain revives a record at 0 HP.** `Area189_DrainHp`'s floor (HP at
  most d becomes 1) turns an HP of 0 into 1 for each of the seven records it
  walks. Faithful; what the game does with that is not measured.
- **Reads past tables, kept.** `Area188_WalkWhileZUnder` indexes
  `Area188_ZLimits` by the tail's sub-kind, which area 188's choice 2 sets to
  `0xFF` (tail kind 10): a handler 0 run after that reads the byte `0x64762F`
  (inside `Area189_StepVectors`). `Area188_ChoiceFocusPair` /
  `Area191_ChoiceFocusPair` read before their pairs for a negative answer.
  `Area189_StepBegin` indexes the sixteen vectors by the whole facing byte
  (`Area189_TurnInput` keeps it below 16; state 0 takes it from `0x904153`,
  which the jitter keeps below 16). `Area191_TalkMessage` indexes
  `MoveScript_EffectState` and `CharacterRecords` by the member byte
  unchecked, and a member not among its keys reads offsets 45..53 of
  `Area191_TalkMessagesA` / `B`, running into the next table. Every one stays
  in mapped memory (the largest, record 255's `+0x1E` at `0x90DDEA`, inside
  `.data` to `0x93D6EC`); ours reads the same.
- **Tail kind 10 armed without `ScriptFlags_Set40`** by area 188's choice 2
  past chapter 12, where every other arming here sets it; what tail kind 10
  (engine) does with that is not read.
- **Tail kind 53's state 1 waits on nothing of its own**: nothing in the
  band moves the state from 1 but a re-arming (area 191's choice 5 at
  `0x1E`, the step hook at 0); what else does is not read.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 51 starts, extents, call sites, the
  two in-function jump tables (`0x42A620`: 8 entries at `+0x14C` through a
  byte table of 14 at `+0x16C`; `0x42B7F0`: 8 at `+0x1B8` through 31 at
  `+0x1D8`), and the shape each root gives.
- **Dropped:** none. **Added:** none. `0x42BC20` is a start of its own in
  the code (a `push ecx` prologue after `nop` padding), reached only by
  `Area191_Init`'s tail `jmp` at `0x42BC0D`; the tool reads the two as one
  function `0x42BBF0..0x42BCD4`, and so does ours (as AR3B read `0x41C2B0`
  inside `0x41C270`). `0x42B2F0` is its own function (called by the control
  and the arrival, which jumps to it at its end), as the tool has it.
- **Area 190's rows are area 189's.** The tool files `0x42A8D0..0x42B540`
  (fifteen functions and the shared `0x42B550`) under area 190 because their
  table `0x6475A0` lies between 189's descriptor and 190's, and no area's
  exclusive code reads it (only the gap `0x42A8B0` does). The PSX says 189:
  `AREA190.EMI` has 4 code bytes, `AREA189.EMI` 13,964, and
  `pairs_propagated.json` pairs all of them in section `b880f463`
  (AREA189). Named `Area189_*` here.
- **Gaps of the tool**, each read to its root: `0x42A8B0`
  (`Field_LeaderStates[13]`), `0x42BA90` / `0x42BBB0` (called by chapter
  code), `0x42BCE0` (`EffectKind18_States[77]`).
- **Tail kinds**: 43, 50 and 53 are armed by immediates in the band; tail
  kind 10 (engine `0x56DB80`) by area 188's choice 2. No register-armed kind.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 51). Each is reached only in play: the choices when the
  areas' message boxes ask, the handlers from their movement scripts, the
  inits on entry, area 191's step hook on a step there, the tail kinds once
  armed, leader state 13 while the leader is in it, the scale states from
  handler 2, `EffectKind18_States[77]` while such an effect lives, and the
  talk from the chapter objects' talk code (SC13's `Scena14_Object00..04`,
  SC15's `Scena15_ObjectTalk`), with the members 8, 4, 2, 5, 6.

## 9. Calls across groups

- **Raw addresses nobody owns**, in `area_w4e_callees.h`: `0x511C10` (the
  map height at (x, z), engine, no group's); `0x42C2D0` (area 192's block,
  **AR4F's** this wave: the rebinding pass names it).
- **By name, Capcom's:** `MoveCmd_Move`, `Sound_StopMusic`, `Rand`.
- **By name, ours:** `ScriptFlags_Set40` / `Clear40`, `Msg_OpenScript`,
  `Sound_PlayEffect`, `Flags_Set` / `Clear` / `Test` / `Toggle`,
  `KeyItem_Has`, `Field_ChangeArea`, `Effect_FindFree`, `Party_Count`,
  `Actor_EquipCount`, `Gte_SetGeomOffset`, `Gte_SetGeomScreen`,
  `Gte_RotTransPers`, `Gte_StoreDepthF`, `Scenario_ArriveHook`,
  `Scena14_LeaveToC4`, `Char_RecalcStats`, `Transition_Start`,
  `Sound_LoadStream`, `Sound_StreamDone`, `AreaMap_ByteAt`,
  `AreaMap_SetByte`, `Sprite_FindFree`, `EventOp_0x`, `Effect_Release`,
  `Party_DropIn`. No harness edit; no `AH_THEIRS` moved.
- **Table entries of other groups', read in place:** `Area136_ShiftCameraUp2`
  (`0x41F320`, AR3E) and `Area80_ResetCameraShift` (`0x40F530`, AR2A) are
  area 188's handlers 1 and 3; `0x42BDA0` (AR4F) is area 191's choice 4.
- **Inbound, for the rebinding pass:**
  - `0x42BA90` (`Area191_TalkMessage`) by raw address from
    `scena_sc13_fuzz.cpp` / `scena_sc13.cpp` (`0x5677F3`, `0x567833`,
    `0x567873`, `0x5678B3`, `0x5678F3`, as `at::kArea191`) and
    `scena_sc15*.cpp` (`0x56AEE1`).
  - `Area_StepHook` (`0x56E050`, ours in `event_ops.cpp`) calls `0x42B9F0`
    through its raw `kStepHandlers` table (case `0xBF`, `0x56E2B2`), now
    `Area191_StepHook`.
  - Read in place by tables, no caller to rebind: `Field_LeaderStates[13]`
    (`Area189_LeaderRun`), `Field_ModeTailKinds` 43, 50, 53,
    `EffectKind18_States[77]`; the choice and handler tables of areas 119,
    131, 136 (`0x42A4B0`) and 192 (`0x42B550`, `0x42B5B0`, `0x42B610`,
    `0x42BD30`).
- `analysis/calltrace/entries_logic.txt`: 42 lines appended under a
  `# group AR4E` comment (nine of the 51 were listed already at the same
  extent); the hosts `0042A1C0 BED`, `0042AE30 4BC`, `0042B510 4D7`,
  `0042BBB0 44B` ran over the band (the consolidation keeps the smaller).
