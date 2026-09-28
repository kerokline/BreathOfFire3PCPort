# World 1, areas 68..69 and 71..75: the band `0x40CEF0..0x40EB90`

**Status:** IN PROGRESS (2026-09-28) - 59 functions ours
(`src/game/area_w1f.cpp`, shadow name `area_w1f`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 354,000 rounds (in this worktree); CONTROLS_SUMMARY (section 4).
Fuzz only: no recorded route reaches the band (section 8). No divergence;
where the original jumps or calls through a pointer read past a table ours
aborts with a message (section 6).

Group AR1F of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12; the
tool's `--groups` prints it as its `AR1C` row, its world-1 letters having
shifted). The band is the tool's (`tools/area_rows.py`,
[`area-rows.md`](area-rows.md)): 59 starts, none ours before, **59 taken**;
no start dropped, none added (section 7). Area 70 has no code (its descriptor
names none); areas 72 and 73 have one function each.

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, hooks, tails and triggers have
no pairing there).

**The brief's `+0x38` note:** area 75's descriptor (`0x60AA98`) has `+0x38`
= 0, as 198 others do. The one descriptor that sets it is **area 77's**
(`0x60BB88` `+0x38` = `0x60ACC8`, a colour matrix in `.data`, never code:
[`area-rows.md`](area-rows.md) section 2, `area_rows.py`'s roots line "+0x38
set in areas [77]"). Area 77 is AR2A's band. Nothing in this band reads a
descriptor's `+0x38`.

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase, `kHook` a step hook `(x, z)` answering in `al`,
`kState` an entry of the area's own `.data` state table, `kCallee` a
function called directly (by the group's own code, by another area's, or as
an object trigger `(object, 0x904030)` answering in `al`). A function that
is both a choice and a handler is fuzzed as a choice when it writes the
message word, else as a handler.

### Area 68 (descriptor `0x607D88`; PSX `0x801F4D88`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40CEF0` | `Area68_ChoiceMessageA` | `0x2E` | choice 0 | kChoice | the message word `Area68_ChoiceMessages[s8 answer]`; answer 0: counter 0 = `0x14`; 1: counter 0 = 3 |
| `0x40CF20` | `Area68_ChoiceMessageB` | `0x2E` | choice 1 | kChoice | the table's second pair; answer 0: counter 0 = 0; 1: `0xFF` |
| `0x40CF50` | `Area68_ChoiceAskCount` | `0x43` | choice 2 | kChoice | answer not 0: message `0x84`, the mark `0x9398CF` 6; 0: `Inventory_CountUsed(1)` at `0xF` or more message `0x82`, else `0x83` and the mark |
| `0x40CFA0` | `Area68_ChoiceAnswer84` | `0x24` | choices 5, 6; **area 50's** choices 3, 4 | kChoice | answer not 0: message `0x84`, the mark; 0: `0xFFFF` |
| `0x40CFD0` | `Area68_SpawnKind4AtMember1` | `0x42` | handler 0 (PSX `0x801F3A18`) | kHandler | `Effect_Spawn(4, 0, Area68_EffectKinds4[party list 1], x, z)` at party record 1, which `Sprite_Current` is made; a slot to its `+0xB` |
| `0x40D020` | `Area68_SpawnKind4AtMember2` | `0x46` | handler 1 (PSX `0x801F3A98`) | kHandler | the same for record 2 (list 2 read as a dword `& 0xFF`) |
| `0x40D070` | `Area68_GlideRunA` | `0x12` | handler 2 (PSX `0x801F3B18`) | kHandler | `jmp` through `Area68_GlideStatesA` by `Sprite_Current[4]` |
| `0x40D090` | `Area68_GlideBegin10` | `0x22` | `Area68_GlideStatesA[0]`, `Area68_GlideStatesB[0]` | kState | the object's count `+0xA` = `0x10`, state 1; `Field_State`'s word `+0x12E` less 2 |
| `0x40D0C0` | `Area68_GlideStepA` | `0x62` | `Area68_GlideStatesA[1]` | kState | count 0: state 0; else x `+0x34` += `steps[count & 0xF] << 11`, z `+0x38` -= the same (signed bytes of `Area68_GlideStepsA`), the count less 1, `+0x12E` less 2 |
| `0x40D130` | `Area68_GlideRunB` | `0x12` | handler 3 (PSX `0x801F3C2C`) | kHandler | through `Area68_GlideStatesB` |
| `0x40D150` | `Area68_GlideStepB` | `0x62` | `Area68_GlideStatesB[1]` | kState | the glide step over `Area68_GlideStepsB` |
| `0x40D1C0` | `Area68_SpawnKind1AtMember1` | `0x42` | handler 4 (PSX `0x801F3D40`) | kHandler | kind 1 from `Area68_EffectKinds`, record 1 |
| `0x40D210` | `Area68_SpawnKind1AtMember2` | `0x46` | handler 5 (PSX `0x801F3DC0`) | kHandler | kind 1, record 2 |
| `0x40D260` | `Area68_SpawnKind3AtMember1` | `0x42` | handler 7 (PSX `0x801F3E48`) | kHandler | kind 3, record 1 |
| `0x40D2B0` | `Area68_SpawnKind3AtMember2` | `0x46` | handler 8 (PSX `0x801F3EC8`) | kHandler | kind 3, record 2 |
| `0x40D300` | `Area68_Trigger25` | `0x5E` | object trigger 25 | kCallee | `ScriptFlags_Set40`; the object at `0x903804` (re-read for each store) `+1` = 4, `+0x83` = 5, word `+0x8A` = 0; tail kind 4, sub-kind 3; counter 3 = the object's index; al 0 |

Choices 3 and 4 are `0x420850` / `0x420870` (another block's); handler 6 is
the shared `ret` `0x437CC0`. `Area68_ChoiceAnswer84` is the band's one shared
body with another area's table: area 50's choices 3 and 4 name it
([`area_w1c.md`](area_w1c.md) section 1 listed it as "other blocks'").

### Area 69 (descriptor `0x608E98`; PSX `0x801F3DAC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40D360` | `Area69_GlideRun` | `0x12` | handler 0 (PSX `0x801F2C04`) | kHandler | through `Area69_GlideStates` by `Sprite_Current[4]` |
| `0x40D380` | `Area69_GlideBegin40` | `0x22` | `Area69_GlideStates[0]`; **area 79's** state table `0x60EC80[0]` | kState | the glide begin with a count of `0x40` |
| `0x40D3B0` | `Area69_GlideStep` | `0x62` | `Area69_GlideStates[1]` | kState | the glide step over `Area69_GlideSteps` |

### Area 71 (descriptor `0x609358`; PSX `0x801F30FC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40D420` | `Area71_SpawnEffect3C` | `0x5A` | handler 0 (PSX `0x801F2C04`) | kHandler | `Effect_FindFree` to the byte `0x903850`; a slot: `+0` = 1, kind `+5` = `0x3C`, `+0xB` = the active member's index, `Sound_PlayEffect(0x207)` |
| `0x40D480` | `Area71_SpawnEffect25` | `0x2A` | handler 1 (PSX `0x801F2CF4`) | kHandler | the same slot, kind `0x25`, no member, no sound |

### Areas 72 and 73 (descriptors `0x609428`, `0x6094F8`; PSX `0x801F2DD4` for both)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40D4B0` | `Area72_PlaceRandomObject` | `0xCB` | init (PSX `0x801F2D2C`) | kInit | a `jmp` over eleven `nop`s; `Rand() & 0x3F` walked down `Area72_Weights`, the first weight it falls below chosen (none: 8); field objects 0..7 but the chosen one get `+0` = 0; the chosen one at `Area72_Cells[Rand() & 7]` (bytes << 16), its `+0x3E` the ground there; `Field_EdgeBits` = the leader's zone counter `0x802E74` less 5 either way |
| `0x40D580` | `Area73_PlaceRandomObject` | `0xCB` | init (the same PSX entry) | kInit | area 72's code over `Area73_Cells` / `Area73_Weights` |

The shipped weights sum to `0x40`, so with `Rand() & 0x3F` an object is
always chosen: the "none" path is dead with the image's tables (the fuzz
reaches it with random weights, section 3).

### Area 74 (descriptor `0x60A2C0`; PSX `0x801F3C48`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40D650` | `Area74_ChoiceAsk92` | `0x3E` | choice 0 = handler 2 (PSX `0x801F2C04`) | kChoice | answer not 0: message `0x94`, the mark; 0: story flag `0x365` (the byte `0x90409C` bit `0x20`) message `0x92`, else `0x93` and the mark |
| `0x40D690` | `Area74_ChoiceAnswer94` | `0x24` | choices 3, 4 = handlers 5, 6 (PSX `0x801F2CC0`, `0x801F2D04`: two bodies there) | kChoice | answer not 0: message `0x94`, the mark; 0: `0xFFFF` |
| `0x40D6C0` | `Area74_PushToggleRow7` | `0xCC` | handler 0 (PSX `0x801F2D48`) | kHandler | the active member's `+0x80` bit 0 cleared; leader `+0x89` 2, the object's `+9` 0 and the leader's direction (`+8 & 7`) 5 or 1: the object turned to it; unless `Field_ObjectBlockedAhead(active member)`: its `+0x87` = 3, `MoveCmd_Move(script object, direction)`; direction 5: Cond row 7 flag 6 set ? cleared : flag 5 set; direction 1: flag 6 set; `Sound_PlayEffect(leader dword +0x2C + 0x100)` |
| `0x40D790` | `Area74_ClearMemberBit0` | `0xD` | handler 1 (PSX `0x801F2E74`); `Area_ObjectHandler`'s fallback table `0x660CA0` entries 2, 4, 8 | kHandler | the active member's `+0x80` bit 0 cleared |
| `0x40D7A0` | `Area74_Trigger28` | `0x16` | object trigger 28 | kCallee | `ScriptFlags_Set40`, tail kind 4 with sub-kind 7; al 0 |

Choices 1, 2 (handlers 3, 4) are `0x420850` / `0x420870`.

### Area 75 (descriptor `0x60AA98`; PSX `0x801F5754`)

Choices 2..14 are handlers 0..12; handlers 8 and 9 are `0x55B8B0` /
`0x55B960` (engine block). **The descriptor's `+0x40` is the shared `ret`
`0x437CC0`** where the PSX pairs an init (`0x801F4FB4`), and the PSX lists 8
handlers where the PC has 13 - Capcom's port, read, not measured further.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40D7C0` | `Area75_DropIn0` | `0x9` | choice 2 = handler 0 (PSX `0x801F317C`) | kHandler | `Party_DropIn(0)` |
| `0x40D7D0` | `Area75_ArmTail30` | `0x14` | choice 3 = handler 1 (PSX `0x801F319C`) | kHandler | `ScriptFlags_Set40`, tail kind 30 at state 0 |
| `0x40D7F0` | `Area75_MarkObjectB` | `0x24` | choice 4 = handler 2 (PSX `0x801F31D0`) | kHandler | object B's index `0x93C351` = the active member's |
| `0x40D820` | `Area75_MarkObjectA` | `0x24` | choice 5 = handler 3 (PSX `0x801F321C`) | kHandler | object A's index `0x93C350` = the active member's |
| `0x40D850` | `Area75_ShowEffects` | `0x26` | choice 6 = handler 4 (PSX `0x801F3268`) | kHandler | the two effect records' `+1` = 2 |
| `0x40D880` | `Area75_PoseObject5` | `0x1F` | choice 7 = handler 5 (PSX `0x801F32C8`) | kHandler | field object 5's `+0x83` = 7, word `+0x8A` = 0, `+0x84` = 2, `+1` = 4 |
| `0x40D8A0` | `Area75_SpawnFive` | `0x80` | choice 8 = handler 6 (PSX `0x801F32FC`) | kHandler | five times `Sprite_FindFree` (to the word `0x903850`); a slot: `EventOp_0x(Area75_SpawnOps record i)`, the new `Sprite_Current` at the entry object's x + `0x30000`, z and `+0x3C`, its `+0xB` = i; `Sprite_Current` and `Field_ActiveMember` put back |
| `0x40D920` | `Area75_FlyRun` | `0x12` | choice 9 = handler 7 (PSX `0x801F33D0`) | kHandler | through `Area75_FlyStates` by `Sprite_Current[4]` |
| `0x40D940` | `Area75_FlyBegin` | `0x34` | `Area75_FlyStates[0]` | kState | dword `+0x14` = `Area75_FlyHeights[+0xB]`, `+0x2B` = 3, state 1, the script back 2 |
| `0x40D980` | `Area75_FlyStep` | `0x6B` | `Area75_FlyStates[1]` | kState | `+0x5D` at `0x80`: `+0` = 0; else `+0x5D` less 8, `+0x5E` / `+0x5F` plus `0xF8`, x += `+0xC`, z += `+0x10`, `+0x3E` += word `+0x14`, the script back 2 |
| `0x40D9F0` | `Area75_DrainEffects` | `0x64` | choice 12 = handler 10 | kHandler | one frame in four the effects' dwords `+0xC` less 1; counter 0 below `0x20`: the script back 2 |
| `0x40DA60` | `Area75_FollowRun` | `0x12` | choice 13 = handler 11 | kHandler | through `Area75_FollowStates` |
| `0x40DA80` | `Area75_FollowBegin` | `0x22` | `Area75_FollowStates[0]` | kState | `Sound_PlayById(0x200)`, the script back 2, state 1 |
| `0x40DAB0` | `Area75_FollowStep` | `0x2E` | `Area75_FollowStates[1]` | kState | `Field_Request` not 0: z = `Field_Kind2Z`, the script back 2; else state 0 |
| `0x40DAE0` | `Area75_FollowKind2Z` | `0x31` | choice 14 = handler 12 | kHandler | `Field_Request` 5: `Sound_PlayEffect(0x207)`; else z = `Field_Kind2Z`, the script back 2 |
| `0x40DB20` | `Area75_ChoiceStart14` | `0x2B` | choice 0 | kChoice | answer 0: message `0x19`, tail state `0x14`, counter 0 = `0x15`; else `0x1A` |
| `0x40DB50` | `Area75_ChoiceStartF` | `0x34` | choice 1 | kChoice | answer 0: message `0xFFFF`, tail state `0xF`, its timer `0x1E`, counter 0 = `0x14`; else `0x18` |
| `0x40DB90` | `Area75_TailPresses` | `0x590` | tail kind 30 | kTail | section 1.1 |
| `0x40E120` | `Area75_StepHook` | `0x3C` | `Area_StepHook`'s case for area 75 (`0x56E0E7`) | kHook | unless Cond row 10 flag 2, x at `0x1B0000` or less (signed) and z's high word `0x38..0x3B`: `ScriptFlags_Set40`, counter 3 = `0xA`, al 1; else al 0 |
| `0x40E160` | `Area75_Trigger41` | `0x1D` | object trigger 41 | kCallee | `ScriptFlags_Set40`, tail kind `0x2C` at state 0, sub-kind `0xC`; al 0 |
| `0x40E180` | `Area75_ResetPresses` | `0x32` | called by the tail (states 5, `0xD`, `0x14`) | kCallee | the phase, flags, both press counts, the list and its position 0; both counters `0x5DC` |
| `0x40E1C0` | `Area75_PhaseRun` | `0x16` | called by the tail's state `0x15`, tail-jumped to by its exit | kCallee | `call [Area75_Phases + phase * 4]`, then `jmp Area75_DrawCounters` |
| `0x40E1E0` | `Area75_PhaseDeal` | `0x14C` | `Area75_Phases[1]` | kState | section 1.1 |
| `0x40E330` | `Area75_PhasePlay` | `0x8D` | `Area75_Phases[2]` | kState | section 1.1 |
| `0x40E3C0` | `Area75_PhaseMissed` | `0x17` | `Area75_Phases[3]` | kState | Cond row 10 flag 1 set; tail state `0x1E` |
| `0x40E3E0` | `Area75_PhaseReached` | `0x92` | `Area75_Phases[4]` | kState | story flag `0x41` cleared; with `Cond_ByteFA` `0xA`: row 10 flag 0 set, tail kind and state 0, the run step 0, object B `+0x83` = 4, object A `+0x83` = 5 (words `+0x8A` 0), `MoveScript_Var7` 5, `Party_DropIn(5)`, counter 0 = `0x1E` |
| `0x40E480` | `Area75_OtherPress` | `0x96` | called by `Area75_PhasePlay` | kCallee | a press of the rhythm table: `Sound_PlayEffect(0x202)`, the other's counter less 4 + `Rand() % 2`, its effect's `+0xC` less 1, object A made current with `+0x4A` = 1, `jmp Sprite_ScriptTick` |
| `0x40E520` | `Area75_PlayerPress` | `0x74` | called by `Area75_PhasePlay` | kCallee | the button (`Input_Pressed` bit `0x20`): the same for the player's counter and effect with the leader's record; else the idle frames + 1 |
| `0x40E5A0` | `Area75_EarlyPress` | `0x48` | called by `Area75_PhasePlay` | kCallee | unless the flags' bit 8, the button: the leader's record ticked, the player's effect less 1, the presses out of turn + 1 |
| `0x40E5F0` | `Area75_DrawCounters` | `0x155` | `jmp`'d to by `Area75_PhaseRun` | kCallee | section 1.1 |
| `0x40E750` | `Area75_DrawWindow` | `0x43F` | called by `Area75_DrawCounters` and **area 42's** tail `0x406A30` | kCallee | section 1.1 |

### 1.1 Area 75's presses

Area 75's cells `0x93C340..0x93C353` (unnamed; no reader outside the band
was read) hold two counters (words, `0x5DC` at a reset) that come down by 4
or 5 a press: **the player's** `0x93C34A` by the button (`Input_Pressed` bit
`0x20`, read as a byte), **the other's** `0x93C34C` by a rhythm table. The
phase byte `0x93C340` indexes `Area75_Phases` (entry 0 the shared `ret`); the
move `0x93C343` (0 the other's turn, 1 the player's, 2 both, anything else
the player's presses counted out of turn) runs for the frames `0x93C344`.

- **`Area75_TailPresses`** (tail kind 30): the s8 state through the byte
  table `0x40E0FC` (36 entries) and the jump table `0x40E09C` (24) in the
  body. Every exit carries a state in `cl` - the new state, or the one it
  entered with when it waits - and when that state is 6..`0xE` the exit
  tail-jumps to `Area75_PhaseRun`. States 0..4: a fade out, story flag
  `0x41` and `Field_ChangeArea(0x4B, 0x170000, 0x190000, 0x82)`, the music
  byte `0x904CD0` = `0x58`, counter 0 `0x13` or 0 by Cond row 10 flag 1, a
  fade in, counter 0 = 1. 5..`0xD`: three rounds gated by counter 0 (3, 6, 9,
  `0xC`): the other's turn to below `0x579`, the player's to below `0x579`,
  message `0x17` then both until the counters are more than `0x96` apart and
  the other's is at `0x47E` or less. `0xF`: the timer down to 4. `0x14` /
  `0x15`: a round of both after a choice (frames `0x78`, sound `0x201`), then
  the phase run with the state read again. `0x1E`: object A's word `+0x8A`
  and the leader's `+0x12E` bumped. `0x1F..0x23`: a fade, the music faded and
  stopped, stream 6 loaded, message `0x1B`, a wait for the stream, flag
  `0x41` cleared, `Field_ChangeArea(0x4B, 0x198000, 0x1B8000, 0x83)`, a fade
  in, `ScriptFlags_Clear40`, the tail disarmed.
- **`Area75_PhaseDeal`** (phase 1): with the player's counter at `0x1F4` or
  less the round ends (phase 4); else a new move: the rhythm row from
  `Area75_Rows[Rand() & 0xF]`, the next pair of the current list (three lists
  in `Area75_Lists`, a new one from `Area75_NextList[Rand() & 0xF]` when one
  ends), object B's animation by (old move, new move) from
  `Area75_MoveAnims`, phase 2.
- **`Area75_PhasePlay`** (phase 2): the presses by the move, the frames
  down; at 0 a new deal (phase 1) unless the flags' bit 8 holds the move
  (frames `0x2710`); without bit 8, phase 3 on more than 3 presses out of
  turn, more than `0x1E` frames without a press in the player's turn, or the
  counters more than `0xC8` apart.
- **`Area75_DrawCounters`**: each counter in a window
  (`Area75_DrawWindow`), printed as `word / 100`, `word % 100` (bytes) by
  `Crt_sprintf` into `0x904BA0` and `Text_DrawAt(..., 6, ...)`, colour 2 when
  the counters are more than `0x96` apart; the flags' bit 1 hides both, bit 4
  blinks the text on `Frame_Counter & 0xC`.
- **`Area75_DrawWindow`** `(x, y, w, h, flag)` - area 42's timer draws with
  it too ([`area_w1b.md`](area_w1b.md), "window `0x40E750`"): a draw-mode
  primitive (its RECT in the 8 bytes before it), four semi-transparent FT4
  quads with float corners (the left edge 2 wide, the middle in two halves
  of `(w + 1 - 4) >> 1`, the right a pixel wider for an odd `w + 1`, the
  right edge 2 wide; the corners cut by 2 / 3 / 1 as the texture's), shade
  `0xFF` or `0xAC` and texture page `0x2F` or `0xF` by `flag`, the CLUT
  `Gpu_GetClut(flag * 32 + 0x10, 0x1E1)`; a second draw mode; then
  `Menu_DrawOutline(x + 2, y + 2, w - 4, h - 4, flag)`. x, y, `w + 1`, `h + 1`
  are taken as words for the corners and as dwords for the outline; the
  float constants are `0x5C41B8` / `BC` / `C0` (1, 3, 2).

## 2. Ours

`src/game/area_w1f.cpp`, calling out only through the harness
(`AH_CALL(name)` for every callee; no raw address is needed - every callee
of the band is named). The group's own callees are called the same way, so
the fuzz stands a recorder in for each. Shapes that repeat are one helper
with the function's constants: area 68's six spawns (`SpawnAtMember`), the
three areas' glides (`GlideBegin`, `GlideStep`), the five two-state
dispatchers (`RunState`), areas 72 / 73's init (`PlaceRandomObject`), area
75's round opening (`OpenMoveA`), press arithmetic (`RandHalf`,
`DropCounter`, `DrainEffect`) and the index of a record among the field
objects (`ObjectIndex`: the pointer less `Sprite_Objects` divided by `0xA4`
as a signed dword, as the originals' `imul` by `0x63E7063F` computes it).
Kept as the originals: every re-read after a call (`Sprite_Current`, the
active member, the focus object `0x903804`, the tail state after the phase
run and after `Sound_StreamDone`, object B after `Sprite_EnsureAnimation`,
the flags after `Msg_OpenScript`), the order of every call, the tail's
carried state, the byte and word widths of every store, `Rand() % 2` with
its sign, the unchecked `.data` reads of section 6, the window's float
corners (every value is an exact integer or half, so `float` arithmetic
matches the x87's).

**Areas 72 and 73's clones start at the body** (`0x40D4C0`, `0x40D590`):
each init's first instruction is a `jmp` over eleven `nop`s, which
`bof3::CloneOriginal` refuses as an entry that looks patched (`E9` with no
call site at offset 0). The clone is the body the `jmp` reaches, 0x10 bytes
on, its call sites 0x10 less than the tool's rows; ours is injected at the
entry as usual.

## 3. The fuzz

`BOF3X_SHADOW=area_w1f` (`src/game/area_w1f_fuzz.cpp`): seven `Run`s under
the one shadow name, one per area with its `Group::area` (68, 69, 71..75),
6,000 rounds per function, the real descriptors and tables in place, the
state tables `Area68_GlideStatesA` / `B`, `Area69_GlideStates`,
`Area75_FlyStates`, `Area75_FollowStates` and `Area75_Phases` as
`DataTable`s (their entries recorders while the fuzz runs).

- **Callees the group lists:** its own seven (`Area75_ResetPresses`,
  `_PhaseRun`, `_DrawCounters`, `_OtherPress`, `_PlayerPress`,
  `_EarlyPress` as `kPhase`; `_DrawWindow` with its five words);
  `ScriptFlags_Set40` / `Clear40`, `Effect_FindFree` (a slot of the group's
  four effect records or none, a third of the time none), `Sprite_FindFree`
  (0..29 or none), `EventOp_0x`, `Transition_Start`, `Music_FadeOut`,
  `Sound_LoadStream`, `Sound_StreamDone` (`kFlag`: the tail tests all of
  eax), `Menu_DrawOutline`, `Gfx_CommitPrim` (moves the packet cursor by the
  size, inside the fuzz's buffer), `Crt_sprintf` and `MoveCmd_Move`
  (Capcom's); standard ones listed again: `Text_DrawAt` with the colour
  masked to its byte (the original pushes stack bytes above it;
  `Text_DrawString` reads the low byte only), `Inventory_CountUsed`
  (answers around `0xF`), `Effect_Spawn` `kByte 0xFE..0x02`, the three GPU
  setters (writing the primitive's bytes, as area_w1b's do).
- **Louder stand-ins** (half the time, from `Noise`): `Sprite_EnsureAnimation`
  moves `Sprite_Current` and one of area 75's cells; `EventOp_0x` moves
  `Sprite_Current` and the active member; `Field_ObjectBlockedAhead` and
  `Effect_FindFree` the active member; `ScriptFlags_Set40` the focus object;
  `Msg_OpenScript` the flags byte; `Area75_ResetPresses` object A and the
  flags; `Area75_PhaseRun` the tail state (to `0x1E`, 0, 6, `0xE`, `0x15`,
  `0xF` - the real phases 3 and 4 set it); `Sprite_ScriptTick` the two press
  counts; `Rand`, `Sound_PlayEffect`, `Flags_Set` and the two press
  recorders one of area 75's cells (objects, effect slots, flags, move,
  list, position, counters - each kept inside what it indexes).
- **Regions beyond the field frame:** `Effect_Objects` records 0..3, the
  active member, script object and focus object pointers, the mark, area
  75's 20 bytes, `Input_Pressed`, `Draw_PassFlags`,
  `MoveScript_WaitWordDA`, the music byte, `Gfx_PacketNext` and the fuzz's
  512-byte packet buffer, areas 72 and 73's weights (34 regions, 17,291
  bytes).
- **Every round:** the active member at a field object, a party record or
  one of the four party objects; the script object at a field object or a
  party record; the focus object at a field object; object A / B 0..29, the
  effect slots 0..3, the list 0..2, the phase 0..4; the packet cursor into
  the buffer; the shipped weights of areas 72 / 73 two rounds in three (the
  harness's random bytes otherwise, which reach "none chosen").
- **Seeds:** the choice answer at each value tested, its sign edge and
  above; the two-state dispatchers' state 0 or 1 (2 or more aborts ours and
  faults the original: never seeded); the glide count at 0, 1, the table's
  nibble edges; the party list bytes 0..11; the trigger arguments (a field
  object, `0x904030`); area 74's push gates each passed four times in five,
  the flag byte `0x90409C` either way; area 75: `+0x5D` at `0x80`, counter 0
  at `0x20` and beside, `Field_Request` 0 / 5 and beside; the tail's state
  at every value the byte table reaches, the table's empty entries and
  above `0x23` / negative, with the cell that state waits on at its value
  two times in three and beside it otherwise (the wait word, counter 0 at 3,
  6, 9, `0xC`, `0x1A`, the counters at `0x578` / `0x47E` and one off, the gap
  at `0x96`, `Field_Request`, the timer); the deal's counter at `0x1F4` and
  beside, the previous move, the list position at its count less one; the
  play's move 0..3 and `0xFF`, frames 1 / 0 / 2, the flags' bit 8, the press
  counts and the gap at their thresholds; the rhythm row and frames on a
  press of the table half the time; the step hook's x at `0x1B0000` and
  beside (signed), z's high word `0x37..0x3C` and one with a high byte; the
  window's x / y words, w and h small (0..5, `0x10`..`0x55`, `0xFFFF`: the
  half-width goes negative) or any, flag 0, 1, 2 or any.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 0, the active member and script object pointers, one of area
  75's cells, a counter word, `Input_Pressed`, the wait word.

**Result (in this worktree):** 354,000 rounds over the 59 functions (6,000
each), COVERAGE_CALLS calls to the stand-ins, 0 mismatches, 17,291 bytes of
state (34 regions) and the log compared. Coverage: every callee each
function can reach was called - COVERAGE_LINE.

`BOF3X_SHADOW='*'`: STAR_RESULT.

## 4. Controls

CONTROLS_TEXT

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (32): `Area68_ChoiceMessages`
`0x607D64` (4 words), `Area68_Choices` `0x607D6C` (7), `Area68_Handlers`
`0x607D40` (9), `Area68_EffectKinds4` `0x607060` and `Area68_EffectKinds`
`0x60706C` (12 bytes each: the room between them, the index a party list
byte), `Area68_GlideStatesA` `0x607DCC` / `B` `0x607DE4` (2 each),
`Area68_GlideStepsA` `0x607DD4` / `B` `0x607DEC` (16 signed bytes each),
`Area69_Handlers` `0x608E90` (1), `Area69_GlideStates` `0x608EDC` (2),
`Area69_GlideSteps` `0x608EE4` (16), `Area71_Handlers` `0x609350` (2),
`Area72_Cells` `0x609410` / `Area73_Cells` `0x6094E0` (8 byte pairs each),
`Area72_Weights` `0x609420` / `Area73_Weights` `0x6094F0` (8 each),
`Area74_Handlers` `0x60A2A0` (7), `Area74_Choices` `0x60A2A8` (5),
`Area75_Choices` `0x60AA58` (15), `Area75_Handlers` `0x60AA60` (13),
`Area75_SpawnOps` `0x60AAE0` (5 records of `0x11`), `Area75_FlyStates`
`0x60AB38` (2), `Area75_FlyHeights` `0x60AB40` (8: the room before the next
table), `Area75_FollowStates` `0x60AB48` (2), `Area75_Phases` `0x60AB50`
(5), `Area75_Lists` `0x60AB88` (3 records of 8), `Area75_Rows` `0x60ABA0`
(16), `Area75_NextList` `0x60ABB0` (16), `Area75_MoveAnims` `0x60ABC0` (16:
the room), `Area75_Rhythm` `0x60ABD0` (two rows of 15: the rows
`Area75_Rows` holds), `Area75_CounterFormat` `0x60ABF0` (a `Crt_sprintf`
format; its length not measured). As in areas 48..51, a descriptor's `+0x3C`
array is the tail of its `+0x34` table in areas 74 and 75. The tail's byte
and jump tables (`0x40E0FC`, `0x40E09C`) are in `.text`, inside its extent.

## 6. Latent defects

Described, not fixed:

- **Pointers read past a table (ours aborts, the original faults or runs
  data):** the five two-state dispatchers jump through their table by
  `Sprite_Current[4]` unchecked (a state of 2 or more jumps through the
  bytes after it); `Area75_PhaseRun` calls `Area75_Phases` by the phase byte
  unchecked (5 entries); `Area75_PhaseDeal` follows `Area75_Lists` by the
  list byte unchecked (3 records; the count compare stays in `.data`, the
  pointer does not). In the game the states are 0 / 1, the phase 0..4 and
  the list 0..2 by every writer read; nothing else writes them.
- **Unchecked indexes into data (kept; the reads stay in `.data`):** the
  choice messages by the s8 answer; the effect-kind tables by a party list
  byte; `Area75_FlyHeights` by `+0xB`; `Area75_MoveAnims` by
  `old * 4 + new`; `Area75_Rhythm` by `row * 15 + frames % 15`.
- **Unchecked indexes that write:** area 75's object bytes `0x93C350` /
  `0x93C351` and effect bytes `0x93C34E` / `0x93C34F` index `Sprite_Objects`
  and `Effect_Objects` for writes with no bound; they come from
  `ObjectIndex(Field_ActiveMember)` (a byte of a truncating division: a
  member outside `Sprite_Objects` gives an index past its 30 records) and
  from whatever set the effect slots (no writer in the band).
- **`Area75_DrawCounters` pushes the colour as a dword whose upper bytes are
  its own uninitialised stack** (the byte `setle` wrote, then `dec` / `and`
  of the whole register): harmless, `Text_DrawString` reads the low byte.
- **`Area68_Trigger25` stores the focus object's index as a byte** of the
  truncating division, into counter 3: an object outside `Sprite_Objects`
  gives a meaningless count.
- **Areas 72 / 73's "none chosen" path is dead** with the shipped weights
  (they sum to `0x40`).

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 59 starts, extents, call sites, the
  tail's jump table (`0x40DB90`: 24 entries at `+0x50C`, the byte table at
  `+0x56C` read in place), and the shape its root gives.
- **Dropped:** none of the band's. The tool's dropped list names
  `0x40E000`: it is the tail's case for state `0x21`, inside
  `Area75_TailPresses` (rightly dropped). **Added:** none; every gap
  between the band's functions is padding or the tail's two tables.
- **`0x40DA60`'s note "7 code entries"** is the tool running from
  `Area75_FollowStates` into `Area75_Phases`, which follows it: the state
  byte is only ever 0 or 1 (`Area75_FollowBegin` / `Step`), so the table is
  2 entries.
- **Areas 72 / 73's entry `jmp`**: a start of the function as the tool says;
  the clone starts at the body (section 2).
- **The tool's gap roots:** `0x40D300`, `0x40D7A0`, `0x40E160` are object
  triggers 25, 28, 41 (`Field_ObjectTriggers`), in the blocks of areas 68,
  74 and 75 by address; `0x40E180`, `0x40E1C0`, `0x40E480`, `0x40E520`,
  `0x40E5A0`, `0x40E750` are reached only by area 75's own code (and area
  42's for `0x40E750`).

## 8. What reaches it

- **No recorded route reaches the band** (`analysis/hidden_reached_*.json`,
  `pc_hidden_reached.json`: none of `0x40CEF0..0x40EB90`; `area_funcs.tsv`'s
  live column is empty for all 59). Every function is reached only in play.
- Nothing in the band was reached by the fuzz's own seeds but not by the
  shipped tables except areas 72 / 73's "none chosen" (section 6).

## 9. Calls across groups

- **Raw addresses:** none. Every callee is named (ours: `Effect_FindFree`,
  `Sprite_FindFree`, `EventOp_0x`, `Transition_Start`, `Music_FadeOut`,
  `Music_FadeOutStop`, `Sound_LoadStream`, `Sound_StreamDone`,
  `Menu_DrawOutline`, `Gfx_CommitPrim`, `Gpu_*`, `Text_DrawAt`,
  `Inventory_CountUsed`, `Field_ObjectBlockedAhead`, `Field_ChangeArea`,
  `Sprite_EnsureAnimation`, `Sprite_ScriptTick`, `Msg_OpenScript`,
  `Party_DropIn`, `Flags_*`, `ScriptFlags_*`, `Sound_*`, `AreaMap_Elevation`;
  Capcom's: `Rand`, `Effect_Spawn`, `MoveCmd_Move`, `Crt_sprintf`). None of
  SX2's twelve is called. No harness edit; no `AH_THEIRS` moved.
- **Inbound, for the rebinding pass:** area 42's tail `0x406A30`
  (`Area42_TimerTail`, group AR1B, `area_w1b_callees.h`'s `kDrawWindow`)
  calls `0x40E750` (`Area75_DrawWindow`) at `0x406A9F` and `0x406CDB` by raw
  address. `Area_StepHook` calls `Area75_StepHook` at `0x56E0E7` (the hook
  switch, a root). Read in place (no caller to rebind): area 50's choices 3,
  4 (`0x5FCB74`, `0x5FCB78`) name `Area68_ChoiceAnswer84`; area 79's state
  table `0x60EC80` names `Area69_GlideBegin40`; `Area_ObjectHandler`'s
  fallback table `0x660CA0` names `Area74_ClearMemberBit0` three times;
  `Field_ModeTailKinds[30]` (`0x662D60`) and `Field_ObjectTriggers` 25, 28,
  41 (`0x662E80`, `0x662E8C`, `0x662EC0`) name the tail and the triggers.
- `analysis/calltrace/entries_logic.txt` (main checkout): 55 lines appended
  under a `# group AR1F` comment; four (`0x40E180`, `0x40E480`, `0x40E520`,
  `0x40E750`) were already there with the same extent; the hosts
  `0040C2B0 1ECD`, `0040E1C0 2B2` and `0040E5A0 1A5` run over others (the
  consolidation keeps the smaller).
