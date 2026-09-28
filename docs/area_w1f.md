# World 1, areas 68..69 and 71..75: the band `0x40CEF0..0x40EB90`

**Status:** IN PROGRESS (2026-09-28) - 59 functions ours
(`src/game/area_w1f.cpp`, shadow name `area_w1f`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 354,000 rounds (in this worktree); 374 controls planted, 371 refused by a count, 3 equivalent (each with a refused variant) (section 4).
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
each), 447,616 calls to the stand-ins, 0 mismatches, 17,291 bytes of
state (34 regions) and the log compared. Coverage: every callee each
function can reach was called, and every state-table entry - e.g. the glide
states 2,939..6,082 each, area 74's `MoveCmd_Move` 355 (the push past all
its gates), `Flags_Clear` 123 (the toggle's clear), `Inventory_CountUsed`
931, area 75's phases 1,148..1,227 each (the shared `ret` 1,211),
`Area75_ResetPresses` 428, `Field_ChangeArea` 425, `Sound_StreamDone` 138,
`Music_FadeOutStop` 158, `Msg_OpenScript` 331, `Area75_DrawWindow` 9,058,
`Gpu_GetClut` 24,000.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4613 ours`, 411 self-test lines, no
mismatch (in this worktree, first run).

## 4. Controls

Planted one at a time in `area_w1f.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w1f.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w1f`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all seven runs). **374 planted, 371 refused by a count (exit 3), 3 equivalent** (each with a near variant planted and refused), no hang, no fault. Every one of the 59 functions has at least one control of its own; a control in a shared helper lists every function it refused in.

A first pass (371 planted) left six standing. Three were the fuzz's fault and were refused after two fixes: the harness returns from a `kPhase` recorder before it runs the callee's `effect`, so the louder stand-ins listed on `Area75_ResetPresses`, `_PhaseRun`, `_OtherPress` and `_PlayerPress` never moved anything (F15, F45; now `kGarbage` with no arguments); `Gfx_CommitPrim`'s effect moved the packet cursor by exactly the size, so a cursor cached across it matched (N35; now one call in five moves it by another amount). The other three are equivalent (below). **For the harness doc:** a `kPhase` callee's `effect` is ignored.

The thinnest: F44 1 (Area75_TailPresses), E42 10 (Area75_FollowStep), N37 10 (Area75_DrawWindow), M14 15 (Area75_DrawCounters), F70 19 (Area75_TailPresses); the tail's re-reads after a call are the thin ones, reached when a louder stand-in moves the cell.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| H1 | `RunState (x5)` | the other state | Area68_GlideRunA 6000, Area68_GlideRunB 6000 |
| H2 | `SpawnAtMember (68 x6)` | z from +0x2E | Area68_SpawnKind4AtMember1 6000, Area68_SpawnKind4AtMember2 5999, Area68_SpawnKind1AtMember1 6000, Area68_SpawnKind1AtMember2 6000, Area68_SpawnKind3AtMember1 6000, Area68_SpawnKind3AtMember2 6000 |
| H3 | `SpawnAtMember (68 x6)` | none 0xFE | Area68_SpawnKind4AtMember1 2421, Area68_SpawnKind4AtMember2 2432, Area68_SpawnKind1AtMember1 2412, Area68_SpawnKind1AtMember2 2375, Area68_SpawnKind3AtMember1 2364, Area68_SpawnKind3AtMember2 2302 |
| H4 | `SpawnAtMember (68 x6)` | the other member made current | Area68_SpawnKind4AtMember1 5762, Area68_SpawnKind4AtMember2 5766, Area68_SpawnKind1AtMember1 5772, Area68_SpawnKind1AtMember2 5774, Area68_SpawnKind3AtMember1 5762, Area68_SpawnKind3AtMember2 5776 |
| H5 | `SpawnAtMember (68 x6)` | kinds index + 1 | Area68_SpawnKind4AtMember1 3951, Area68_SpawnKind4AtMember2 4018, Area68_SpawnKind1AtMember1 4412, Area68_SpawnKind1AtMember2 4410, Area68_SpawnKind3AtMember1 4354, Area68_SpawnKind3AtMember2 4435 |
| H6 | `SpawnAtMember (68 x6)` | the record, not Sprite_Current re-read | Area68_SpawnKind4AtMember1 156, Area68_SpawnKind4AtMember2 169, Area68_SpawnKind1AtMember1 147, Area68_SpawnKind1AtMember2 154, Area68_SpawnKind3AtMember1 152, Area68_SpawnKind3AtMember2 154 |
| H7 | `GlideBegin (68, 69)` | script word less 3 | Area68_GlideBegin10 6000 |
| H8 | `GlideBegin (68, 69)` | state 2 | Area68_GlideBegin10 6000 |
| H9 | `GlideStep (x3)` | state 1 at count 0 | Area68_GlideStepA 1201, Area68_GlideStepB 1163 |
| H10 | `GlideStep (x3)` | x step by count & 7 | equivalent: the three glide step tables repeat every 4 bytes (`FF 00 01 00`), so `& 7` reads what `& 0xF` reads; variant H10b (`& 0xE`) refused |
| H11 | `GlideStep (x3)` | x step << 12 | Area68_GlideStepA 2122, Area68_GlideStepB 2118 |
| H12 | `GlideStep (x3)` | z step not negated | Area68_GlideStepA 2122, Area68_GlideStepB 2118 |
| H13 | `GlideStep (x3)` | count less 2 | Area68_GlideStepA 4799, Area68_GlideStepB 4837 |
| H14 | `GlideStep (x3)` | script word less 4 | Area68_GlideStepA 4799, Area68_GlideStepB 4837 |
| H15 | `ObjectIndex (x5)` | divided by 0xA3 | Area71_SpawnEffect3C 2032 |
| P1 | `PlaceRandomObject (72, 73)` | below or at the weight | Area72_PlaceRandomObject 546 |
| P2 | `PlaceRandomObject (72, 73)` | Rand & 0x1F | Area72_PlaceRandomObject 2130 |
| P3 | `PlaceRandomObject (72, 73)` | the others' +1 cleared | Area72_PlaceRandomObject 6000 |
| P4 | `PlaceRandomObject (72, 73)` | seven objects cleared | Area72_PlaceRandomObject 5858 |
| P5 | `PlaceRandomObject (72, 73)` | z from the next pair | Area72_PlaceRandomObject 5998 |
| P6 | `PlaceRandomObject (72, 73)` | Rand & 3 | Area72_PlaceRandomObject 3004 |
| P7 | `PlaceRandomObject (72, 73)` | ground to +0x3C | Area72_PlaceRandomObject 6000 |
| P8 | `PlaceRandomObject (72, 73)` | edge bits less 4 | Area72_PlaceRandomObject 6000 |
| P9 | `PlaceRandomObject (72, 73)` | chosen 7 not placed | Area72_PlaceRandomObject 106 |
| P10 | `PlaceRandomObject (72, 73)` | elevation at (z, x) | Area72_PlaceRandomObject 5218 |
| P11 | `Area73_PlaceRandomObject` | area 72's cells | Area73_PlaceRandomObject 6000 |
| P12 | `Area72_PlaceRandomObject` | weights + 1 | Area72_PlaceRandomObject 2848 |
| A1 | `Area68_ChoiceMessageA` | counter 0x15 | Area68_ChoiceMessageA 926 |
| A2 | `Area68_ChoiceMessageA` | counter 4 | Area68_ChoiceMessageA 856 |
| A3 | `Area68_ChoiceMessageA` | the answer unsigned | Area68_ChoiceMessageA 1938 |
| A4 | `Area68_ChoiceMessageB` | table + 6 | Area68_ChoiceMessageB 5479 |
| A5 | `Area68_ChoiceMessageB` | counter 0xFE | Area68_ChoiceMessageB 922 |
| A6 | `Area68_ChoiceMessageB` | counter 1 at answer 0 | Area68_ChoiceMessageB 876 |
| A7 | `Area68_ChoiceAskCount` | above 0xF | Area68_ChoiceAskCount 124 |
| A8 | `Area68_ChoiceAskCount` | message 0x85 | Area68_ChoiceAskCount 259 |
| A9 | `Area68_ChoiceAskCount` | category 2 | Area68_ChoiceAskCount 931 |
| A10 | `Area68_ChoiceAnswer84` | none 0xFFFE | Area68_ChoiceAnswer84 858 |
| A11 | `Area68_ChoiceAnswer84` | mark 7 | Area68_ChoiceAnswer84 5142 |
| A12 | `Area68_SpawnKind4AtMember1` | kind 5 | Area68_SpawnKind4AtMember1 6000 |
| A13 | `Area68_SpawnKind4AtMember2` | the other table | Area68_SpawnKind4AtMember2 3201 |
| A14 | `Area68_SpawnKind1AtMember1` | kind 2 | Area68_SpawnKind1AtMember1 6000 |
| A15 | `Area68_SpawnKind1AtMember2` | member 1 | Area68_SpawnKind1AtMember2 6000 |
| A16 | `Area68_SpawnKind3AtMember1` | kind 4's table | Area68_SpawnKind3AtMember1 3214 |
| A17 | `Area68_SpawnKind3AtMember2` | kind 7 | Area68_SpawnKind3AtMember2 6000 |
| A18 | `Area68_GlideRunA` | table B | Area68_GlideRunA 2939 |
| A19 | `Area68_GlideRunB` | table A | Area68_GlideRunB 2979 |
| A20 | `Area68_GlideBegin10` | count 0x11 | Area68_GlideBegin10 6000 |
| A21 | `Area68_GlideStepA` | steps B | Area68_GlideStepA 2122 |
| A22 | `Area68_GlideStepB` | steps A | Area68_GlideStepB 2118 |
| A23 | `Area68_Trigger25` | +1 = 5 | Area68_Trigger25 6000 |
| A24 | `Area68_Trigger25` | +0x83 = 4 | Area68_Trigger25 6000 |
| A25 | `Area68_Trigger25` | word +0x8A = 1 | Area68_Trigger25 6000 |
| A26 | `Area68_Trigger25` | sub-kind 4 | Area68_Trigger25 6000 |
| A27 | `Area68_Trigger25` | counter 3 the index + 1 | Area68_Trigger25 6000 |
| A28 | `Area68_Trigger25` | the focus read before ScriptFlags_Set40 | Area68_Trigger25 2902 |
| A29 | `Area68_Trigger25` | answers 1 | Area68_Trigger25 6000 |
| A30 | `Area68_Trigger25` | tail kind 5 | Area68_Trigger25 6000 |
| B1 | `Area69_GlideBegin40` | count 0x41 | Area69_GlideBegin40 6000 |
| B2 | `Area69_GlideStep` | area 68's steps | equivalent: `Area69_GlideSteps` holds the same 16 bytes as `Area68_GlideStepsA`; variant B2b (the table + 1) refused |
| B3 | `Area69_GlideRun` | area 68's table | Area69_GlideRun 6000 |
| C1 | `Area71_SpawnEffect3C` | kind 0x3D | Area71_SpawnEffect3C 3983 |
| C2 | `Area71_SpawnEffect3C` | member + 1 | Area71_SpawnEffect3C 3983 |
| C3 | `Area71_SpawnEffect3C` | sound 0x208 | Area71_SpawnEffect3C 3983 |
| C4 | `Area71_SpawnEffect3C` | the member read before Effect_FindFree | Area71_SpawnEffect3C 1920 |
| C5 | `Area71_SpawnEffect25` | none not stored | Area71_SpawnEffect25 1970 |
| C6 | `Area71_SpawnEffect25` | kind 0x26 | Area71_SpawnEffect25 4020 |
| C7 | `Area71_SpawnEffect25` | +0 = 2 | Area71_SpawnEffect25 4020 |
| C8 | `Area71_SpawnEffect3C` | +0 = 3 | Area71_SpawnEffect3C 3983 |
| D1 | `Area74_ChoiceAsk92` | bit 0x10 | Area74_ChoiceAsk92 439 |
| D2 | `Area74_ChoiceAsk92` | message 0x91 | Area74_ChoiceAsk92 450 |
| D3 | `Area74_ChoiceAsk92` | mark 5 | Area74_ChoiceAsk92 445 |
| D4 | `Area74_ChoiceAnswer94` | none 0xFFFE | Area74_ChoiceAnswer94 910 |
| D5 | `Area74_ChoiceAnswer94` | message 0x95 | Area74_ChoiceAnswer94 5090 |
| D6 | `Area74_ChoiceAsk92` | message 0x96 for an answer | Area74_ChoiceAsk92 5105 |
| D7 | `Area74_PushToggleRow7` | bit 1 cleared too | Area74_PushToggleRow7 3006 |
| D8 | `Area74_PushToggleRow7` | pose 3 | Area74_PushToggleRow7 1378 |
| D9 | `Area74_PushToggleRow7` | direction & 0xF | Area74_PushToggleRow7 540 |
| D10 | `Area74_PushToggleRow7` | +0xA tested | Area74_PushToggleRow7 1053 |
| D11 | `Area74_PushToggleRow7` | direction 2 for 1 | Area74_PushToggleRow7 552 |
| D12 | `Area74_PushToggleRow7` | turned to direction + 1 | Area74_PushToggleRow7 1038 |
| D13 | `Area74_PushToggleRow7` | +0x87 = 4 | Area74_PushToggleRow7 355 |
| D14 | `Area74_PushToggleRow7` | the member not read again | Area74_PushToggleRow7 160 |
| D15 | `Area74_PushToggleRow7` | moved direction ^ 4 | Area74_PushToggleRow7 355 |
| D16 | `Area74_PushToggleRow7` | flag 7 tested | Area74_PushToggleRow7 189 |
| D17 | `Area74_PushToggleRow7` | flag 7 cleared | Area74_PushToggleRow7 123 |
| D18 | `Area74_PushToggleRow7` | flag 4 set | Area74_PushToggleRow7 66 |
| D19 | `Area74_PushToggleRow7` | the next row's flag 6 | Area74_PushToggleRow7 166 |
| D20 | `Area74_PushToggleRow7` | sound + 0x101 | Area74_PushToggleRow7 355 |
| D21 | `Area74_PushToggleRow7` | direction 1 toggles | Area74_PushToggleRow7 355 |
| D22 | `Area74_PushToggleRow7` | blocked test inverted | Area74_PushToggleRow7 1056 |
| D23 | `Area74_ClearMemberBit0` | bit 1 | Area74_ClearMemberBit0 4435 |
| D24 | `Area74_Trigger28` | sub-kind 8 | Area74_Trigger28 6000 |
| D25 | `Area74_Trigger28` | tail kind 5 | Area74_Trigger28 6000 |
| D26 | `Area74_Trigger28` | answers 1 | Area74_Trigger28 6000 |
| E1 | `Area75_DropIn0` | entry 1 | Area75_DropIn0 6000 |
| E2 | `Area75_ArmTail30` | kind 0x1F | Area75_ArmTail30 6000 |
| E3 | `Area75_ArmTail30` | state 1 | Area75_ArmTail30 6000 |
| E4 | `Area75_MarkObjectB` | object A's cell | Area75_MarkObjectB 5999 |
| E5 | `Area75_MarkObjectA` | index + 1 | Area75_MarkObjectA 6000 |
| E6 | `Area75_ShowEffects` | the other's +2 | Area75_ShowEffects 5993 |
| E7 | `Area75_ShowEffects` | the player's = 3 | Area75_ShowEffects 6000 |
| E8 | `Area75_PoseObject5` | object 6 | Area75_PoseObject5 6000 |
| E9 | `Area75_PoseObject5` | +0x83 = 6 | Area75_PoseObject5 6000 |
| E10 | `Area75_PoseObject5` | +0x84 = 3 | Area75_PoseObject5 6000 |
| E11 | `Area75_PoseObject5` | +1 = 5 | Area75_PoseObject5 6000 |
| E12 | `Area75_PoseObject5` | word +0x8A = 1 | Area75_PoseObject5 6000 |
| E13 | `Area75_SpawnFive` | four | Area75_SpawnFive 6000 |
| E14 | `Area75_SpawnFive` | x + 0x20000 | Area75_SpawnFive 5966 |
| E15 | `Area75_SpawnFive` | records of 0x10 | Area75_SpawnFive 5920 |
| E16 | `Area75_SpawnFive` | +0xB = i + 1 | Area75_SpawnFive 5961 |
| E17 | `Area75_SpawnFive` | the slot stored as a byte | Area75_SpawnFive 5976 |
| E18 | `Area75_SpawnFive` | none ends the loop | Area75_SpawnFive 4794 |
| E19 | `Area75_SpawnFive` | the active member not put back | Area75_SpawnFive 4976 |
| E20 | `Area75_SpawnFive` | Sprite_Current not put back | Area75_SpawnFive 4912 |
| E21 | `Area75_SpawnFive` | z from +0x3C | Area75_SpawnFive 5970 |
| E22 | `Area75_SpawnFive` | Sprite_Current read before the op | Area75_SpawnFive 5038 |
| E23 | `Area75_FlyRun` | the follow table | Area75_FlyRun 6000 |
| E24 | `Area75_FlyBegin` | height index + 1 | Area75_FlyBegin 4734 |
| E25 | `Area75_FlyBegin` | +0x2B = 4 | Area75_FlyBegin 6000 |
| E26 | `Area75_FlyBegin` | state 2 | Area75_FlyBegin 6000 |
| E27 | `Area75_FlyBegin` | script back 3 | Area75_FlyBegin 6000 |
| E28 | `Area75_FlyStep` | 0x80 or more | Area75_FlyStep 1493 |
| E29 | `Area75_FlyStep` | +0 = 1 | Area75_FlyStep 2966 |
| E30 | `Area75_FlyStep` | less 9 | Area75_FlyStep 3034 |
| E31 | `Area75_FlyStep` | +0x5E plus 0xF7 | Area75_FlyStep 3034 |
| E32 | `Area75_FlyStep` | +0x5F plus 0xF0 | Area75_FlyStep 3034 |
| E33 | `Area75_FlyStep` | x plus +0x10 | Area75_FlyStep 3034 |
| E34 | `Area75_FlyStep` | z plus +0xC | Area75_FlyStep 3034 |
| E35 | `Area75_FlyStep` | word +0x16 | Area75_FlyStep 3034 |
| E36 | `Area75_DrainEffects` | one frame in eight | Area75_DrainEffects 706 |
| E37 | `Area75_DrainEffects` | at 0x20 too | Area75_DrainEffects 796 |
| E38 | `Area75_DrainEffects` | the other slot ^ 1 second | Area75_DrainEffects 1043 |
| E39 | `Area75_FollowRun` | the fly table | Area75_FollowRun 6000 |
| E40 | `Area75_FollowBegin` | sound 0x201 | Area75_FollowBegin 6000 |
| E41 | `Area75_FollowBegin` | state 0 | Area75_FollowBegin 6000 |
| E42 | `Area75_FollowStep` | request above 1 | Area75_FollowStep 10 |
| E43 | `Area75_FollowStep` | state 1 | Area75_FollowStep 3014 |
| E44 | `Area75_FollowStep` | x set | Area75_FollowStep 2986 |
| E45 | `Area75_FollowKind2Z` | request 4 | Area75_FollowKind2Z 2205 |
| E46 | `Area75_FollowKind2Z` | sound 0x206 | Area75_FollowKind2Z 1445 |
| E47 | `Area75_FollowKind2Z` | script back 4 | Area75_FollowKind2Z 4555 |
| E48 | `Area75_ChoiceStart14` | message 0x1B | Area75_ChoiceStart14 5092 |
| E49 | `Area75_ChoiceStart14` | state 0x13 | Area75_ChoiceStart14 908 |
| E50 | `Area75_ChoiceStart14` | counter 0x16 | Area75_ChoiceStart14 908 |
| E51 | `Area75_ChoiceStart14` | message 0x1A for 0 | Area75_ChoiceStart14 908 |
| E52 | `Area75_ChoiceStartF` | message 0x17 | Area75_ChoiceStartF 5151 |
| E53 | `Area75_ChoiceStartF` | state 0xE | Area75_ChoiceStartF 849 |
| E54 | `Area75_ChoiceStartF` | timer 0x1F | Area75_ChoiceStartF 849 |
| E55 | `Area75_ChoiceStartF` | counter 0x13 | Area75_ChoiceStartF 849 |
| E56 | `Area75_ChoiceStartF` | message 0xFFFE | Area75_ChoiceStartF 849 |
| F1 | `Area75_TailPresses` | phase run below 8 | Area75_TailPresses 258 |
| F2 | `Area75_TailPresses` | phase run from 5 | Area75_TailPresses 226 |
| F3 | `Area75_TailPresses` | state 0 Transition_Start(2) | Area75_TailPresses 198 |
| F4 | `Area75_TailPresses` | state 1 pass flags 1 | Area75_TailPresses 128 |
| F5 | `Area75_TailPresses` | state 2 flag 0x42 | Area75_TailPresses 206 |
| F6 | `Area75_TailPresses` | state 2 area flags 0x83 | Area75_TailPresses 206 |
| F7 | `Area75_TailPresses` | state 2 music 0x59 | Area75_TailPresses 206 |
| F8 | `Area75_TailPresses` | state 2 row flag 2 | Area75_TailPresses 206 |
| F9 | `Area75_TailPresses` | state 2 counter 0x14 | Area75_TailPresses 131 |
| F10 | `Area75_TailPresses` | state 3 pass flags 0x1E | Area75_TailPresses 152 |
| F11 | `Area75_TailPresses` | state 4 counter 2 | Area75_TailPresses 164 |
| F12 | `Area75_TailPresses` | state 1 waits above 1 | Area75_TailPresses 24 |
| F13 | `Area75_TailPresses` | state 5 at 4 | Area75_TailPresses 171 |
| F14 | `Area75_TailPresses` | state 5 flag 0x10 | Area75_TailPresses 111 |
| F15 | `Area75_TailPresses` | state 5 object read before the reset | Area75_TailPresses 33 |
| F16 | `Area75_TailPresses` | state 5 +0x2A = 2 | Area75_TailPresses 155 |
| F17 | `Area75_TailPresses` | state 5 animation 3 | Area75_TailPresses 155 |
| F18 | `Area75_TailPresses` | state 6 at 0x578 too | Area75_TailPresses 58 |
| F19 | `Area75_TailPresses` | state 6 phase 1 | Area75_TailPresses 85 |
| F20 | `Area75_TailPresses` | state 7 at 7 | Area75_TailPresses 188 |
| F21 | `Area75_TailPresses` | state 7 move 2 | Area75_TailPresses 158 |
| F22 | `Area75_TailPresses` | state 8 the other counter | Area75_TailPresses 78 |
| F23 | `Area75_TailPresses` | state 8 counter + 2 | Area75_TailPresses 77 |
| F24 | `Area75_TailPresses` | state 9 at 8 | Area75_TailPresses 196 |
| F25 | `Area75_TailPresses` | state 9 message 0x18 | Area75_TailPresses 173 |
| F26 | `Area75_TailPresses` | state 9 flags read before the message | Area75_TailPresses 77 |
| F27 | `Area75_TailPresses` | state 9 flag 2 | Area75_TailPresses 135 |
| F28 | `Area75_TailPresses` | state 9 request 3 | Area75_TailPresses 166 |
| F29 | `Area75_TailPresses` | state 0xA waits on 3 | Area75_TailPresses 98 |
| F30 | `OpenMoveA (75 tail)` | row 1 | Area75_TailPresses 145 |
| F31 | `OpenMoveA (75 tail)` | +0x2A = 0 | Area75_TailPresses 145 |
| F32 | `OpenMoveA (75 tail)` | object B | Area75_TailPresses 73 |
| F33 | `Area75_TailPresses` | state 0xB below 0x96 | Area75_TailPresses 41 |
| F34 | `Area75_TailPresses` | state 0xB no count | Area75_TailPresses 102 |
| F35 | `Area75_TailPresses` | state 0xC at 0x47F | Area75_TailPresses 27 |
| F36 | `Area75_TailPresses` | state 0xD at 0xB | Area75_TailPresses 167 |
| F37 | `Area75_TailPresses` | state 0xD to 0xF | Area75_TailPresses 141 |
| F38 | `Area75_TailPresses` | state 0xF above 1 | Area75_TailPresses 20 |
| F39 | `Area75_TailPresses` | state 0xF back to 5 | Area75_TailPresses 154 |
| F40 | `Area75_TailPresses` | state 0x14 frames 0x77 | Area75_TailPresses 132 |
| F41 | `Area75_TailPresses` | state 0x14 object A twice | Area75_TailPresses 63 |
| F42 | `Area75_TailPresses` | state 0x14 object B +0x2A = 1 | Area75_TailPresses 132 |
| F43 | `Area75_TailPresses` | state 0x14 sound 0x202 | Area75_TailPresses 132 |
| F44 | `Area75_TailPresses` | state 0x14 object B read before the first animation | Area75_TailPresses 1 |
| F45 | `Area75_TailPresses` | state 0x15 not read again | Area75_TailPresses 40 |
| F46 | `Area75_TailPresses` | state 0x1E word + 2 | Area75_TailPresses 223 |
| F47 | `Area75_TailPresses` | state 0x1E leader word + 2 | Area75_TailPresses 223 |
| F48 | `Area75_TailPresses` | state 0x1E to 0x20 | Area75_TailPresses 223 |
| F49 | `Area75_TailPresses` | state 0x1F at 0x1B | Area75_TailPresses 165 |
| F50 | `Area75_TailPresses` | state 0x1F fade 0x21 | Area75_TailPresses 141 |
| F51 | `Area75_TailPresses` | state 0x1F Transition_Start(1) | Area75_TailPresses 141 |
| F52 | `Area75_TailPresses` | state 0x20 fade 0xB | Area75_TailPresses 158 |
| F53 | `Area75_TailPresses` | state 0x20 stream 7 | Area75_TailPresses 158 |
| F54 | `Area75_TailPresses` | state 0x20 message 0x1C | Area75_TailPresses 158 |
| F55 | `Area75_TailPresses` | state 0x20 pass flags 1 | Area75_TailPresses 158 |
| F56 | `Area75_TailPresses` | state 0x21 al only | Area75_TailPresses 22 |
| F57 | `Area75_TailPresses` | state 0x21 to 0x23 | Area75_TailPresses 108 |
| F58 | `Area75_TailPresses` | state 0x21 waits on 1 | Area75_TailPresses 102 |
| F59 | `Area75_TailPresses` | state 0x22 flag 0x40 | Area75_TailPresses 219 |
| F60 | `Area75_TailPresses` | state 0x22 z + 1 | Area75_TailPresses 219 |
| F61 | `Area75_TailPresses` | state 0x23 counter 0 | Area75_TailPresses 132 |
| F62 | `Area75_TailPresses` | state 0x23 kind 1 | Area75_TailPresses 133 |
| F63 | `Area75_TailPresses` | state 0x23 Transition_Start(0) | Area75_TailPresses 133 |
| F64 | `Area75_TailPresses` | state 0x23 ends on 6 | Area75_TailPresses 133 |
| F65 | `Area75_TailPresses` | state 0x10 a case | Area75_TailPresses 118 |
| F66 | `Area75_TailPresses` | the state unsigned & 0x3F | Area75_TailPresses 78 |
| F67 | `Area75_TailPresses` | state 0 to 8 | Area75_TailPresses 198 |
| F68 | `Area75_TailPresses` | state 3 waits on 1 | Area75_TailPresses 179 |
| F69 | `Area75_TailPresses` | state 4 does not wait | Area75_TailPresses 58 |
| F70 | `Area75_TailPresses` | state 0x20 waits above 0xFF | Area75_TailPresses 19 |
| F71 | `Area75_TailPresses` | state 0x23 does not wait | Area75_TailPresses 69 |
| F72 | `Area75_TailPresses` | state 6 no count | Area75_TailPresses 84 |
| F73 | `Area75_TailPresses` | state 0xC phase 2 | Area75_TailPresses 90 |
| G1 | `Area75_StepHook` | flag 3 | Area75_StepHook 6000 |
| G2 | `Area75_StepHook` | x at 0x1B0000 refused | Area75_StepHook 83 |
| G3 | `Area75_StepHook` | x unsigned | Area75_StepHook 135 |
| G4 | `Area75_StepHook` | five rows | Area75_StepHook 163 |
| G5 | `Area75_StepHook` | from 0x37 | Area75_StepHook 292 |
| G6 | `Area75_StepHook` | z high word as a byte | Area75_StepHook 161 |
| G7 | `Area75_StepHook` | counter 3 = 0xB | Area75_StepHook 477 |
| G8 | `Area75_StepHook` | answers 2 | Area75_StepHook 477 |
| G9 | `Area75_StepHook` | z from x | Area75_StepHook 477 |
| G10 | `Area75_Trigger41` | kind 0x2D | Area75_Trigger41 6000 |
| G11 | `Area75_Trigger41` | sub-kind 0xD | Area75_Trigger41 6000 |
| G12 | `Area75_Trigger41` | state 1 | Area75_Trigger41 6000 |
| G13 | `Area75_Trigger41` | answers 1 | Area75_Trigger41 6000 |
| K1 | `Area75_ResetPresses` | other counter 0x5DD | Area75_ResetPresses 6000 |
| K2 | `Area75_ResetPresses` | player counter 0x5DB | Area75_ResetPresses 6000 |
| K3 | `Area75_ResetPresses` | list 1 | Area75_ResetPresses 6000 |
| K4 | `Area75_ResetPresses` | position 1 | Area75_ResetPresses 6000 |
| K5 | `Area75_ResetPresses` | idle 1 | Area75_ResetPresses 6000 |
| K6 | `Area75_ResetPresses` | flags 8 | Area75_ResetPresses 6000 |
| K7 | `Area75_ResetPresses` | early 2 | Area75_ResetPresses 6000 |
| K8 | `Area75_PhaseRun` | no counters drawn | Area75_PhaseRun 6000 |
| K9 | `Area75_PhaseRun` | the next phase | Area75_PhaseRun 6000 |
| K10 | `Area75_PhaseDeal` | below 0x1F4 | Area75_PhaseDeal 1597 |
| K11 | `Area75_PhaseDeal` | animation + 3 | Area75_PhaseDeal 2187 |
| K12 | `Area75_PhaseDeal` | phase 3 at the end | Area75_PhaseDeal 2345 |
| K13 | `Area75_PhaseDeal` | early 1 | Area75_PhaseDeal 3655 |
| K14 | `Area75_PhaseDeal` | rows & 7 | Area75_PhaseDeal 1292 |
| K15 | `Area75_PhaseDeal` | position + 2 | Area75_PhaseDeal 1105 |
| K16 | `Area75_PhaseDeal` | past the count | Area75_PhaseDeal 954 |
| K17 | `Area75_PhaseDeal` | next list from the rows | Area75_PhaseDeal 1740 |
| K18 | `Area75_PhaseDeal` | position not stored at a new list | Area75_PhaseDeal 2318 |
| K19 | `Area75_PhaseDeal` | move not less 1 | Area75_PhaseDeal 3628 |
| K20 | `Area75_PhaseDeal` | frames + 1 | Area75_PhaseDeal 3655 |
| K21 | `Area75_PhaseDeal` | animation index transposed | Area75_PhaseDeal 2565 |
| K22 | `Area75_PhaseDeal` | sound 0x202 | Area75_PhaseDeal 3655 |
| K23 | `Area75_PhaseDeal` | phase 1 after a deal | Area75_PhaseDeal 3655 |
| K24 | `Area75_PhaseDeal` | +0x2A = 1 at the end | Area75_PhaseDeal 1722 |
| K25 | `Area75_PhaseDeal` | the next list's pointer | Area75_PhaseDeal 3286 |
| K26 | `Area75_PhaseDeal` | the list read before the row's Rand | Area75_PhaseDeal 52 |
| K27 | `Area75_PhasePlay` | move 0 presses reversed | Area75_PhasePlay 1453 |
| K28 | `Area75_PhasePlay` | move 1 early | Area75_PhasePlay 1472 |
| K29 | `Area75_PhasePlay` | move 2 without the player | Area75_PhasePlay 1557 |
| K30 | `Area75_PhasePlay` | others the player | Area75_PhasePlay 1518 |
| K31 | `Area75_PhasePlay` | phase 0 at the end | Area75_PhasePlay 47 |
| K32 | `Area75_PhasePlay` | frames 0x270F | Area75_PhasePlay 324 |
| K33 | `Area75_PhasePlay` | bit 4 for 8 | Area75_PhasePlay 2312 |
| K34 | `Area75_PhasePlay` | more than 2 | Area75_PhasePlay 111 |
| K35 | `Area75_PhasePlay` | idle at 0x1E | Area75_PhasePlay 88 |
| K36 | `Area75_PhasePlay` | gap 0xC9 | Area75_PhasePlay 55 |
| K37 | `Area75_PhasePlay` | phase 2 for 3 | Area75_PhasePlay 4166 |
| K38 | `Area75_PhasePlay` | flags read before the presses | Area75_PhasePlay 118 |
| K39 | `Area75_PhasePlay` | frames less 2 | Area75_PhasePlay 6000 |
| K40 | `Area75_PhaseMissed` | flag 2 | Area75_PhaseMissed 6000 |
| K41 | `Area75_PhaseMissed` | state 0x1F | Area75_PhaseMissed 6000 |
| K42 | `Area75_PhaseReached` | story flag 0x42 | Area75_PhaseReached 6000 |
| K43 | `Area75_PhaseReached` | chapter 0xB | Area75_PhaseReached 2456 |
| K44 | `Area75_PhaseReached` | row flag 1 | Area75_PhaseReached 1664 |
| K45 | `Area75_PhaseReached` | objects read before the flag | Area75_PhaseReached 142 |
| K46 | `Area75_PhaseReached` | B +0x83 = 5 | Area75_PhaseReached 1620 |
| K47 | `Area75_PhaseReached` | A +0x83 = 6 | Area75_PhaseReached 1664 |
| K48 | `Area75_PhaseReached` | Var7 4 | Area75_PhaseReached 1664 |
| K49 | `Area75_PhaseReached` | step 1 | Area75_PhaseReached 1664 |
| K50 | `Area75_PhaseReached` | drop-in 6 | Area75_PhaseReached 1664 |
| K51 | `Area75_PhaseReached` | counter 0x1F | Area75_PhaseReached 1664 |
| K52 | `Area75_PhaseReached` | kind 1 | Area75_PhaseReached 1664 |
| K53 | `Area75_PhaseReached` | B at A | Area75_PhaseReached 1620 |
| K54 | `Area75_PhaseReached` | A word 1 | Area75_PhaseReached 1664 |
| K55 | `Area75_PhaseReached` | B word 1 | Area75_PhaseReached 1620 |
| L1 | `Area75_OtherPress` | mod 14 | Area75_OtherPress 2699 |
| L2 | `Area75_OtherPress` | rows of 16 | Area75_OtherPress 2316 |
| L3 | `Area75_OtherPress` | sound 0x203 | Area75_OtherPress 3511 |
| L4 | `RandHalf (75 x2)` | & 1 for % 2 | equivalent: `rand()` never answers a negative value, so `% 2` and `& 1` agree (the harness's negative answers are multiples of 4); variant L4b (`% 3`) refused |
| L5 | `DropCounter (75 x2)` | less 5 | Area75_OtherPress 3503, Area75_PlayerPress 2959 |
| L6 | `Area75_OtherPress` | the player's counter | Area75_OtherPress 3511 |
| L7 | `Area75_OtherPress` | the player's effect | Area75_OtherPress 2630 |
| L8 | `Area75_OtherPress` | +0x4A = 2 | Area75_OtherPress 3511 |
| L9 | `Area75_OtherPress` | object B | Area75_OtherPress 3389 |
| L10 | `Area75_OtherPress` | the slot read before Rand | Area75_OtherPress 146 |
| L11 | `DrainEffect (75 x3)` | less 2 | Area75_OtherPress 3511, Area75_PlayerPress 2974, Area75_EarlyPress 2285 |
| L12 | `Area75_PlayerPress` | idle + 2 | Area75_PlayerPress 3026 |
| L13 | `Area75_PlayerPress` | idle 1 after a press | Area75_PlayerPress 2974 |
| L14 | `Area75_PlayerPress` | the other's counter | Area75_PlayerPress 2974 |
| L15 | `Area75_PlayerPress` | the other's effect | Area75_PlayerPress 2230 |
| L16 | `Area75_PlayerPress` | +0x4A = 2 | Area75_PlayerPress 2973 |
| L17 | `Pressed20 (75 x2)` | bit 0x10 | Area75_PlayerPress 2981, Area75_EarlyPress 2342 |
| L18 | `Area75_PlayerPress` | the slot read before Rand | Area75_PlayerPress 214 |
| L19 | `Area75_PlayerPress` | sound 0x201 | Area75_PlayerPress 2974 |
| L20 | `Area75_PlayerPress` | the leader + 1 record | Area75_PlayerPress 2853 |
| L21 | `Area75_EarlyPress` | bit 0x10 | Area75_EarlyPress 1516 |
| L22 | `Area75_EarlyPress` | early + 2 | Area75_EarlyPress 2285 |
| L23 | `Area75_EarlyPress` | +0x4A = 3 | Area75_EarlyPress 2285 |
| L24 | `Area75_EarlyPress` | the next record | Area75_EarlyPress 2170 |
| L25 | `Area75_EarlyPress` | the other's effect | Area75_EarlyPress 1724 |
| M1 | `Area75_DrawCounters` | blink by bit 8 | Area75_DrawCounters 588 |
| M2 | `Area75_DrawCounters` | blink flag 2 | Area75_DrawCounters 550 |
| M3 | `Area75_DrawCounters` | colour below 0x96 | Area75_DrawCounters 641 |
| M4 | `Area75_DrawCounters` | colour 3 | Area75_DrawCounters 2040 |
| M5 | `Area75_DrawCounters` | hidden by bit 2 | Area75_DrawCounters 2955 |
| M6 | `Area75_DrawCounters` | window x 0x19 | Area75_DrawCounters 4529 |
| M7 | `Area75_DrawCounters` | second window 0x12 high | Area75_DrawCounters 4529 |
| M8 | `Area75_DrawCounters` | tens | Area75_DrawCounters 4495 |
| M9 | `Area75_DrawCounters` | mod 10 | Area75_DrawCounters 4055 |
| M10 | `Area75_DrawCounters` | second / 99 | Area75_DrawCounters 1688 |
| M11 | `Area75_DrawCounters` | text x 0x1F | Area75_DrawCounters 3976 |
| M12 | `Area75_DrawCounters` | second text y 0x23 | Area75_DrawCounters 3976 |
| M13 | `Area75_DrawCounters` | five characters | Area75_DrawCounters 3976 |
| M14 | `Area75_DrawCounters` | the second counter read first | Area75_DrawCounters 15 |
| M15 | `Area75_DrawCounters` | the second text always | Area75_DrawCounters 553 |
| M16 | `Area75_DrawCounters` | the first counter the other's | Area75_DrawCounters 3805 |
| N1 | `Area75_DrawWindow` | shade 0xAD | Area75_DrawWindow 1983 |
| N2 | `Area75_DrawWindow` | texture page 0xE | Area75_DrawWindow 6000 |
| N3 | `Area75_DrawWindow` | CLUT x + 0x11 | Area75_DrawWindow 6000 |
| N4 | `Area75_DrawWindow` | rect y 0xF1 | Area75_DrawWindow 6000 |
| N5 | `Area75_DrawWindow` | rect w 0x11 | Area75_DrawWindow 6000 |
| N6 | `Area75_DrawWindow` | first draw mode dtd 0 | Area75_DrawWindow 6000 |
| N7 | `Area75_DrawWindow` | first commit 0xD | Area75_DrawWindow 6000 |
| N8 | `Area75_DrawWindow` | left v2 h - 1 | Area75_DrawWindow 6000 |
| N9 | `Area75_DrawWindow` | y + 1 for y + 2 | Area75_DrawWindow 6000 |
| N10 | `Area75_DrawWindow` | bottom less 2 | Area75_DrawWindow 6000 |
| N11 | `Area75_DrawWindow` | left x2 at x + 2 | Area75_DrawWindow 6000 |
| N12 | `Area75_DrawWindow` | left u1 3 | Area75_DrawWindow 6000 |
| N13 | `Area75_DrawWindow` | left u3 1 | Area75_DrawWindow 6000 |
| N14 | `Area75_DrawWindow` | left v0 1 | Area75_DrawWindow 6000 |
| N15 | `Area75_DrawWindow` | half by / 2 | Area75_DrawWindow 752 |
| N16 | `Area75_DrawWindow` | odd from w | Area75_DrawWindow 6000 |
| N17 | `Area75_DrawWindow` | middle x + 3 | Area75_DrawWindow 6000 |
| N18 | `Area75_DrawWindow` | middle-left u1 + 1 | Area75_DrawWindow 6000 |
| N19 | `Area75_DrawWindow` | right half without the odd pixel | Area75_DrawWindow 3216 |
| N20 | `Area75_DrawWindow` | right half u without odd | Area75_DrawWindow 3216 |
| N21 | `Area75_DrawWindow` | right edge x - 1 | Area75_DrawWindow 6000 |
| N22 | `Area75_DrawWindow` | right edge y2 less 2 | Area75_DrawWindow 6000 |
| N23 | `Area75_DrawWindow` | right v1 h + 1 | Area75_DrawWindow 6000 |
| N24 | `Area75_DrawWindow` | right v3 h - 3 | Area75_DrawWindow 6000 |
| N25 | `Area75_DrawWindow` | right y1 at y | Area75_DrawWindow 6000 |
| N26 | `Area75_DrawWindow` | right v1 3 | Area75_DrawWindow 6000 |
| N27 | `Area75_DrawWindow` | second rect w 0xFF | Area75_DrawWindow 6000 |
| N28 | `Area75_DrawWindow` | outline w - 3 | Area75_DrawWindow 6000 |
| N29 | `Area75_DrawWindow` | outline h - 5 | Area75_DrawWindow 6000 |
| N30 | `Area75_DrawWindow` | outline x + 3 | Area75_DrawWindow 6000 |
| N31 | `Area75_DrawWindow` | y whole | Area75_DrawWindow 1985 |
| N32 | `Area75_DrawWindow` | h + 1 whole | Area75_DrawWindow 1987 |
| N33 | `Area75_DrawWindow` | right edge texture page + 1 | Area75_DrawWindow 6000 |
| N34 | `Area75_DrawWindow` | second draw mode page 0x9C | Area75_DrawWindow 6000 |
| N35 | `Area75_DrawWindow` | middle cursor not read again | Area75_DrawWindow 1199 |
| N36 | `Area75_DrawWindow` | a left store before the setters | Area75_DrawWindow 5980 |
| N37 | `Area75_DrawWindow` | the flag whole for the page | Area75_DrawWindow 10 |
| N38 | `Area75_DrawWindow` | x whole | Area75_DrawWindow 2048 |
| N39 | `Area75_DrawWindow` | w + 1 whole | Area75_DrawWindow 2346 |
| H10b | `GlideStep (x3)` | x step by count & 0xE | Area68_GlideStepA 2677, Area68_GlideStepB 2719 |
| B2b | `Area69_GlideStep` | steps + 1 | Area69_GlideStep 4223 |
| L4b | `RandHalf (75 x2)` | % 3 for % 2 | Area75_OtherPress 2322, Area75_PlayerPress 2057 |

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

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D136 (reads and writes by an
unchecked byte or count), D138 (the bare-`ret` inits), D147 (uninitialised
bytes), D154 (dead branches) in [`known-defects.md`](known-defects.md).

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
