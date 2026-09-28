# World 3, areas 143..146: the band `0x420800..0x4223A0`

**Status:** IN PROGRESS (2026-09-28) - 53 functions ours
(`src/game/area_w3f.cpp`, shadow name `area_w3f`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 318,000 rounds (in this worktree); CONTROLS_SUMMARY (section 4). Fuzz only:
no recorded route reaches the band (section 8). No divergence; the three
two-state dispatchers abort past their tables, where the original would jump
into data (section 6).

Group AR3F of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
53 starts, none ours before, **53 taken**; no start dropped, none added
(section 7). Area 147 has no code in the band (its descriptor `0x6342E8` has
no `+0x34`, `+0x3C` or `+0x40`).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`area_funcs.tsv`) and agrees with
the reading; the clone tables are `area_rows.py --clones`'s rows, each read
against the disassembly. What an area *is* in the story is not read here.
The PSX twins are the sibling's `names/area_records.toml` (descriptor
handlers and inits only; choices, hooks, tails, triggers and state tables
have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kTail` a `Field_ModeTailKinds` phase (by
the s8 `0x9039F3`), `kHook` a step or cell hook `(x, z)` answering in `al`,
`kState` a state handler reached through a table in `.data`, `kCallee` a
function called directly (by the group's own code, by other code, through an
engine table, or as an object trigger `(object, 0x904030)` answering in
`al`). A function that is both a choice and a handler is fuzzed by the shape
of its first root.

### Area 143 (descriptor `0x630C80`; PSX `0x801F52B8`)

Thirteen choices (`Area143_Choices` `0x630C48`) and four handlers
(`Area143_Handlers` `0x630C6C`); choices 9..12 are the four handlers. Choice
7 is `0x425C30` (world 4's block) and choice 8 `Area22_ArmTailOnYes`
(`0x403050`, AR0B's), both another group's; choice 10 and handler 1 are
`Area145_ClearTint`.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x420800` | `Area143_ChoiceAsk52` | `0x41` | choice 0 | kChoice | an answer: message `0x54` and the mark `0x9398CF` 6; 0: the dword `0x904650` equal to `0x3FFFF` (eighteen flags), message `0x52`; else `0x53` and the mark |
| `0x420850` | `Area143_ChoiceYesMark3` | `0x1A` | choice 1; also the tables of areas 3, 37, 41, 50, 55, 59, 61, 68, 74, 91, 98, 113, 116 | kChoice | an answer: the byte `0x9398D1` 3; message `0xFFFF` |
| `0x420870` | `Area143_ChoiceYesMark5` | `0x1A` | choice 2; the same fourteen areas | kChoice | the same with 5 |
| `0x420890` | `Area143_ChoiceAsk54` | `0x24` | choices 3, 4 | kChoice | an answer: message `0x54`, the mark 6; 0: `0xFFFF` |
| `0x4208C0` | `Area143_ChoiceMessage49` | `0x17` | choice 5 | kChoice | message `0x49`, `0x4A` for an answer |
| `0x4208E0` | `Area143_ChoiceRun8` | `0x55` | choice 6 | kChoice | an answer: message `0x4C`. 0: `ScriptFlags_Set40`; the message word (read after the call) `0x44`: `MoveScript_Var7` 8, the step `0x8034E5` 8, message `0x4B`, the focus object's word `+0x8A` + 1; else Var7 8, step `0xA`, message `0xFFFF` |
| `0x420940` | `Area143_ChangeArea70` | `0x1A` | handler 0 = choice 9 (PSX `0x801F3C10`); area 145 handler 2, area 146 handler 0 | kHandler | `Field_ChangeArea(0x70, 0xC8000, 0x580000, 0x81)` |
| `0x420960` | `Area143_MessageByMember` | `0x8B` | handler 2 = choice 11 (PSX `0x801F3C74`) | kHandler | for each of `Area143_MemberKeys`' four in turn, each of `Field_MemberCount`'s party records: the first whose `+0x89` is the key opens that key's message (`Area143_MemberMessages`), `Field_Request` 2 |
| `0x4209F0` | `Area143_SkipIfLeader89Is7` | `0x19` | handler 3 = choice 12 (PSX `0x801F3D38`); area 197 handler 9 | kHandler | the leader's `+0x89` 7: the script position + 3 |
| `0x420A10` | `Area143_StepHook` | `0x4B` | `Area_StepHook`'s case for area `0x8F` | kHook | `Cond_ByteFD` 1, x's high word `0x37..0x39`, z's `0x64..0x66`, the leader's pose 0, 7 or 6: counter 0 = 0, `Party_DropIn(0)`, al 1 |
| `0x420A60` | `Area143_Trigger50` | `0x2D` | object trigger 50 (a gap of the tool) | kCallee | `ScriptFlags_Set40`; tail kind 4 (engine) with sub-kind `0xF`; `Cond_ByteFE` (read after the call) 0: set to 1 and the tail's state 5; al 0 |
| `0x420A90` | `Area143_ClutShiftRight` | `0x6A` | called by chapter 12's code (`0x562027`, `0x5620BA`) | kCallee | the eleven CLUT rows 3..13 as loaded (`Gfx_ClutStripSource + 0x600`), each 5-bit channel shifted right by the argument (its low byte, as x86 masks the count), bit 15 kept, to the live strip `0x4000` on; `Gfx_ClutStripDirty` 1; al 0 |
| `0x420B00` | `Area143_EffectB3Run` | `0x12` | `Effect_KindHandlers[0xB3]` (a gap) | kCallee | `Area143_EffectStates` by the running record's `+1` |
| `0x420B20` | `Area143_EffectB3Cylinder` | `0x2B` | `Area143_EffectStates[1]` | kState | the record's point (x, z, y) to the stack, `Area143_DrawGlowCylinder` |
| `0x420B50` | `Area143_DrawGlowCylinder` | `0x27B` | called by `Area143_EffectB3Cylinder` | kCallee | the glow cylinder (below) |

Effect kind `0xB3`'s state 0 is `Area59_EffectGround` (`0x40B4F0`, AR1D's:
the record's height from the ground, then state 1), as for areas 36, 59,
100's kinds.

**The glow cylinder** (`0x420B50` here, `0x4220D0` in area 146's block):
the map camera (`0x494060`); then sixteen quads about the point's (x, z), at
radius (`Math_Cos` / `Math_Sin` << 9) >> 4, each angle `0x100` on from the
last: two points projected by `0x494110` at each angle, (x, z, y) and (x, z,
y + `0x8000000`), the previous angle's pair and this one's the POLY_G4's four
vertices (the PC's `0x10`-byte vertex layout: colour, then the projected
vertex's three dwords); semi-transparent (abr 1 on tpage `0x3C0`); the
vertex grey `(Frame_Counter & 1) + 8 << 2` (`0x20` or `0x24`) on the
previous pair, and 4 less over quads 4..11 (4 more otherwise) on the new
pair, carried to the next quad; each quad linked with `MapView_LinkPrimAt`
(`0x44`) between a draw-mode packet with dtd 1 and one with dtd 0, all at the
new pair's (x, z). A capstone compare of the two copies: 205 instructions
each, 118 differ, every one a stack offset or a register choice (the second
copy keeps (x, z) in its vector where the first saves a copy); the calls,
their arguments and the primitive's stores are the same, in the same order.
Ours is one body (`GlowCylinder`) behind both names.

### Area 144 (descriptor `0x631770`; PSX `0x801F491C`)

Nine handlers (`Area144_Handlers` `0x631748`); its two choices
(`Area144_Choices` `0x631764`) are handlers 7 and 8.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x420DD0` | `Area144_SceneByRowFlags` | `0x1CA` | handler 0 (PSX `0x801F3818`) | kHandler | the chapter row's flags `0x17..0x1B`, the first clear, and the leader's `+0x89` pick the script object's `+3` (2; 3 / 4; 5 / 6 / 7; 8 / 9 / `0xA`; `0xC` / `0xD`), setting the row's flag on three of them; the script position stored `0xFFFE` every way out |
| `0x420FA0` | `Area144_WalkToZ1D8` | `0x55` | handler 1 (PSX `0x801F3A20`) | kHandler | d = (`0x1D8000` - z) >> 15; not 0: the script object's `+7` = |d|, the running object's direction 1 (d < 0) or 5, `MoveCmd_Move` that way |
| `0x421000` | `Area144_SkipIfMember3Is4` | `0x14` | handler 2 (PSX `0x801F3AB4`) | kHandler | the third party record's `+0x89` 4: the script position + 3 |
| `0x421020` | `Area144_SkipIfMember3Is2` | `0x14` | handler 3 (PSX `0x801F3AEC`) | kHandler | the same with 2 |
| `0x421040` | `Area144_WalkToZ1C0` | `0x46` | handler 4 (PSX `0x801F3B24`) | kHandler | d = (`0x1C0000` - z) >> 15; not 0: direction 1, `+7` = |d|, `MoveCmd_Move`; `Field_ScriptFlags` bit 8 either way |
| `0x421090` | `Area144_SpawnEffect85` | `0x5B` | handler 5 (PSX `0x801F3BAC`) | kHandler | an effect of kind `0x85`, `+6` the active member's index in `Sprite_Objects`; none: the script back 2 |
| `0x4210F0` | `Area144_MoveKind2Here` | `0x3E` | handler 6 (PSX `0x801F3C78`) | kHandler | the kind-2 object (`Sprite_Kind2`) to the running object's (x, z, y), its `+0x84` the script object's `+4`, `+0x87` 1; `MoveCmd_MoveKind2(3)` |
| `0x421130` | `Area144_ChoiceCounter1E` | `0x1C` | choice 0 = handler 7 (PSX `0x801F3CE4`) | kChoice | message `0xFFFF`; counter 0 `0x23` for an answer, else `0x1E` |
| `0x421150` | `Area144_ChoiceDropIn` | `0x3B` | choice 1 = handler 8 (PSX `0x801F3D14`) | kChoice | message `0xFFFF`; 0: counter 0 = 5, `Party_DropIn(6)`, Var7 7, step `0x14`; an answer: `Party_DropIn(5)` |

### Area 145 (descriptor `0x6336A0`; PSX `0x801F5C50`)

Twelve choices (`Area145_Choices` `0x633670`) and eleven handlers
(`Area145_Handlers` `0x633674`: choices 1..11 are the handlers). Handler 2 is
`Area143_ChangeArea70`, 3 `Area145_ClearTint`, 9 and 10 `Area57_StopMusic` /
`Area57_ResumeSound` (`0x40B2D0` / `0x40B2E0`, AR1D's, called by areas 39,
57 and 108 too). **Its init is `0x437CC0`, a bare `ret`** (the entry the
chapters' object tables use for "nothing"); the PSX descriptor has an init,
`0x801F5324` - what it does is not read here, and nothing in this band
stands in for it.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x421190` | `Area145_ChoiceRun3` | `0x2E` | choice 0 | kChoice | message `0xFFFF`; an answer: counter 0 = 8; 0: Var7 3, counter 0 `0xA`, step `0xA` |
| `0x4211C0` | `Area145_PartyRecord16` | `0x58` | handler 0 = choice 1 (PSX `0x801F3CE4`) | kHandler | i = the active member's index in `Sprite_ObjectsExtra` (a byte, to `0x903851`); `MoveScript_PartyRecords` record i: `+2` `0x10`, `+1` 0, dword `+0xC` `0x200 / +2` (`0x20`); the script object's bit 8 |
| `0x421220` | `Area145_RunDrop` | `0x12` | handler 1 = choice 2 (PSX `0x801F3DCC`) | kHandler | `Area145_DropStates` by the running object's `+4` |
| `0x421240` | `Area145_DropStart` | `0xAE` | `Area145_DropStates[0]` | kState | bit `0x40` cleared; word `+0x3E` the ground + `0x7D0`; `+0xC`, `+0x10`, `+0x14` 0, `+0x20` -8, state 1; animation `0x3B`; the script object's bit `0x40`, its `+1` = the leader record's party index x `0x14`; the script back 2 |
| `0x4212F0` | `Area145_DropFall` | `0x78` | `Area145_DropStates[1]` | kState | `+0x14` += `+0x20`; `Field_LeaderStepTick`; the ground above `+0x3E` (signed 16-bit): landed - `+0x3E` the ground, speeds 0, state 2, animation `0x39`, the script object's `+1` 4; else the script back 2 |
| `0x421370` | `Area145_MessageByMember1` | `0x8B` | handler 4 = choice 5 (PSX `0x801F403C`) | kHandler | `Area143_MessageByMember`'s body with its own keys and messages (`0x6336EC` / `0x6336F0`) |
| `0x421400` | `Area145_MessageByMember5` | `0x8B` | handler 5 = choice 6 (PSX `0x801F4100`) | kHandler | the same (`0x6336F8` / `0x6336FC`) |
| `0x421490` | `Area145_MessageByMemberB` | `0x8B` | handler 6 = choice 7 (PSX `0x801F41C4`) | kHandler | the same (`0x633704` / `0x633708`) |
| `0x421520` | `Area145_GiveKeyItemB` | `0x45` | handler 7 = choice 8 (PSX `0x801F4288`) | kHandler | key item `0xB`'s name (`Item_NamePtr(4, 0xB)`, 16 bytes) to `Text_Records`; `KeyItem_Add(0xB)`; `Msg_OpenSystem(2)`; `Field_Request` 2 |
| `0x421570` | `Area145_SkipIfKeyItemB` | `0x19` | handler 8 = choice 9 (PSX `0x801F4304`) | kHandler | key item `0xB` held: the script position + 3 |
| `0x421590` | `Area145_Trigger40` | `0x1D` | object trigger 40 (a gap) | kCallee | `ScriptFlags_Set40`; tail kind `0x2C` (engine), state 0, sub-kind `0xF`; al 0 |
| `0x4215B0` | `Area145_StepHook` | `0xA4` | `Area_StepHook`'s case for area `0x91` | kHook | `Cond_ByteFD` 0: z exactly `0x98000`, x's and the leader's high words `0x1B..0x1C`: `ScriptFlags_Set40`, tail kind 20 at state `0x19`, al 1. `Cond_ByteFD` 4: story flag `0x2D`, x's high word `9..0xB`, z's `0x62..0x64`, the pose 0, 7 or 6: counter 0 = 0, `Party_DropIn(3)`, al 1 |
| `0x421660` | `Area145_CellHook` | `0x6F` | `Area_CellHooks`' record for area `0x91` | kHook | the first of `Area145_CellRecords`' seven matching the cell's x and z bytes and the leader's pose: with `Cond_Flags` row 13's flag 9, `ScriptFlags_Set40`, tail kind 20 at the record's state (2, 4, 6 or `0xA`), al 1 |
| `0x4216D0` | `Area145_Tail20` | `0x44B` | tail kind 20 | kTail | the area's sequence (below) |
| `0x421B20` | `Area145_TrailStart` | `0x67` | called by `Area145_Tail20` | kCallee | the trail's first leg by the timer (below); al 0, 1 or 2 |
| `0x421B90` | `Area145_TrailLeg0` | `0xF5` | called by `Area145_TrailStart` | kCallee | a leg by extra object 0's quarter (below) |
| `0x421C90` | `Area145_TrailLeg1` | `0x11B` | called by `Area145_TrailLeg0` | kCallee | a leg by extra object k's quarter |
| `0x421DB0` | `Area145_TrailLeg2` | `0xEC` | called by `Area145_TrailLeg1` | kCallee | the last leg; al its record's `+2` at the end |
| `0x421EA0` | `Area145_SpawnTrail` | `0x68` | called by the four | kCallee | an effect of kind `0x3F` from (x0, z0) to (x1, z1), `+0x14` / `+0x20` `0x500000`; none: al 1 |
| `0x421F10` | `Area145_DrawGradient` | `0x9C` | `EffectKind18_States[96]` (`0x6541EC`; a gap) | kState | while `Cond_ByteFE`: a draw-mode packet (tpage `0x95`) and a full-screen POLY_G4, (0, 0)..(320, 240) as floats, colour (0, `0xC8`, `0xFF`) above and (0, 0, `0x20`) below, both committed to slot 7 |
| `0x421FB0` | `Area145_ClearTint` | `0x17` | handler 3 = choice 4 (PSX `0x801F4008`); handler 1 of areas 36, 59, 100, 112, 116, 146, area 143 choice 10 | kHandler | the running object's `+0x48` 0, `Sprite_ReleaseTint` |

**Tail kind 20** (`Area145_Tail20`) switches on the s8 state - 2 through a
byte table of 35 into a jump table of 18, both inside its extent (`+0x428`,
`+0x3E0`); the `ja` bounds the index, so every other state does nothing.
The cell hook arms it at 2, 4, 6 or `0xA`, the step hook at `0x19`:

- 2, 4, 6: counter 3 = the state, state 8; 8 waits for counter 3 to be 0
  (the scripts count it down) and disarms.
- `0xA`: story flag `0x2C`, sound `0x200`, `Kind2_Place(0)` and an effect of
  kind `0x13` beside the camera's angle (none free: the state stays, and the
  flag, sound and placement run again next frame); `0xB` waits for counter 3
  = `0x40` (timer 0, sound `0x204`); `0xC` runs `Area145_TrailStart` with the
  timer + 1 each frame: 1 goes on; 2 (or 0 with story flag `0x2D` set) sound
  `0x205` and state `0x14`; 0 with the flag clear sounds `0x205`, `0x207` and
  state `0x1E`. `0x14`: another kind `0x13` effect, `Field_Kind2X` / `Z` the
  leader's; `0x15` waits for `Field_Kind2Hold`, counter 3 0; `0x16` clears
  flag `0x2C` and disarms.
- `0x19`: the leader's `+1..+3` = 2, 3, 0, timer `0x17`; `0x1A` counts it
  down, then `Field_ChangeArea(0x91, 0x3B8000, 0x5D0000, 0x82)`, story flag
  `0x2E` (sound `0x20C` the first time) and disarms.
- `0x1E`: `Transition_Start(8)`, the kind-2 target (`0x308000`,
  `0x638000`); `0x1F` waits for `MoveScript_WaitWordDA`, `Transition_Start(9)`
  and an effect of kind `0x3E`; `0x20` waits for `Field_Kind2Hold`, timer
  `0x1E`; `0x21` counts it; `0x22` flag `0x2D`, `Field_ChangeArea(0x91,
  0xA0000, 0x630000, 0x80)`; `0x23` waits for the word, timer `0x5A`; `0x24`
  counts it, `Field_ChangeArea(0x91, 0x3E0000, 0x5F0000, 1)` and on to `0x16`.

**The trail** is a chain of effects of kind `0x3F` (each a segment from one
point to another) drawn a frame at a time: `Area145_TrailStart(t)` puts the
first segment from (`0x428000`, `0x638000`) back min(t, 5) cells in x; from
t 5 on `Area145_TrailLeg0` adds a segment along the leg that extra object
0's quarter turn (its `+0x6C` bits 9..10) picks in `Area145_TrailLegs0`, t -
5 cells long up to the leg's length; at the end of a leg with a next index k,
`Area145_TrailLeg1` continues from extra object k's position along the leg
its quarter picks (tables `1A` for object 1, `1B` for another), then
`Area145_TrailLeg2` (tables `2A` / `2B`, t - 15 cells) answers the leg's
last byte, 0 or 2. The sound `0x206` marks each leg's end. So the four extra
objects' facings route the trail, and its answer (2 at a dead end or past t
`0x37`, 0 at the right end) steers tail state `0xC`. Each leg's direction
steps by the engine's cell deltas (`0x66971C`, two signed bytes a direction).

### Area 146 (descriptor `0x6340D0`; PSX `0x801F3ABC`)

Three handlers (`Area146_Handlers` `0x6340C0`): `Area143_ChangeArea70`,
`Area145_ClearTint`, `Area146_LeaderBit138OtSlot`. No choices, no init.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x421FD0` | `Area146_LeaderBit138OtSlot` | `0x23` | handler 2 (PSX `0x801F2C68`) | kHandler | the leader's `+0x138` bit 0 cleared; the running object's `+0x29` = `Draw_OtSlot` |
| `0x422000` | `Area146_StepHook` | `0x5E` | `Area_StepHook`'s case for area `0x92` | kHook | `Cond_ByteFD` 1, story flag `0x33`, x's high word `0x44..0x46`, z's `0x2A..0x2C`, the pose 0, 7 or 6: counter 0 = 0, `Party_DropIn(0)`, al 1 |
| `0x422060` | `Area146_Trigger32` | `0x1D` | object trigger 32 (a gap) | kCallee | `ScriptFlags_Set40`; tail kind `0x2C`, state 0, sub-kind 4; al 0 |
| `0x422080` | `Area146_EffectB4Run` | `0x12` | `Effect_KindHandlers[0xB4]` (a gap) | kCallee | `Area146_EffectStates` by the record's `+1` |
| `0x4220A0` | `Area146_EffectB4Cylinder` | `0x2B` | `Area146_EffectStates[1]` | kState | the record's point to `Area146_DrawGlowCylinder` |
| `0x4220D0` | `Area146_DrawGlowCylinder` | `0x27A` | called by `Area146_EffectB4Cylinder`, the effect states of areas 36, 59, 100, 112, 116 and engine code (section 9) | kCallee | the glow cylinder |
| `0x422350` | `Area146_ClearFlag46` | `0x17` | areas 148 (handler 1 = choice 3), 167 (handler 1 = choice 6) | kHandler | story flag `0x46` cleared, `Cond_ByteFE` 0 |
| `0x422370` | `Area146_ToExtraObject0` | `0x2C` | areas 148 (handler 2 = choice 4), 149 (handler 0), 167 (handler 8 = choice 13) | kHandler | the running object's x, z, y = extra object 0's |

The last two lie in area 146's block, but no table of area 146 names them
(the tool's `shared` rows); they are named after the block, as AR2D named
area 98's camera bodies.

## 2. Ours

`src/game/area_w3f.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
two raw addresses of section 9). The group's own callees are called the
same way (`AH_CALL(Area145_TrailLeg1)`, `AH_CALL(Area143_DrawGlowCylinder)`),
so the fuzz stands a recorder in for each and every function is tested
alone; the three state dispatchers read their `.data` tables in place and
call the entry, so the fuzz's `DataTable` swap stands recorders there.
Shapes that repeat are one helper: the glow cylinder (`GlowCylinder`, both
copies), the member search (`MessageByMember`, four copies), the two walks
(`StepsToZ`), the two legs 1 and 2 (`TrailLeg`), the step hooks' pose test
and drop-in, the object triggers' arming (`ArmTail`), the tail's timer and
its effect of kind `0x13` (`Kind13`). Kept as the originals: every re-read
after a call (`Sprite_Current` and `MoveScript_Object` in the drop's states,
the message word in `Area143_ChoiceRun8`, `Cond_ByteFE` in the trigger, the
active member after `Effect_FindFree`, the effect slot read back from
`DamageScratch`), the order of every call, the 16-bit compares of the hooks'
high words, the signed quotients (`(pointer - base) / 0xA4` and `/ 0x14C`
truncate toward 0), the 32-bit wrap of every coordinate sum, the byte index
area 145's handler 0 keeps, and the unchecked reads of section 6.

## 3. The fuzz

`BOF3X_SHADOW=area_w3f` (`src/game/area_w3f_fuzz.cpp`): four `Run`s under
the one shadow name, one per area with its `Group::area` (143, 144, 145,
146), 6,000 rounds per function, the real descriptors and tables in place.
The shared bodies run under the area whose block holds them.

- **Callees the group lists:** `ScriptFlags_Set40` / `Clear40`,
  `Effect_FindFree` (`kByte 0xFF..0x03`: a slot of the first four records or
  none), `KeyItem_Has` / `Add`, `Kind2_Place`, `Transition_Start`,
  `MoveCmd_MoveKind2`, `MoveCmd_Move` (Capcom's), `Item_NamePtr` (answering
  a name buffer of the fuzz's own, filled), `MapView_GroundAt`,
  `Field_LeaderStepTick`, `Sprite_SetAnimation`, `Sound_PlayEffect`, the
  draw calls (`Gfx_CommitPrim`, `MapView_LinkPrimAt`, `Gpu_SetDrawMode`,
  `Gpu_SetPolyG4`, `Gpu_SetSemiTrans`, `Gpu_GetTPage`, `Math_Sin`,
  `Math_Cos`), `0x494060` and `0x494110` by raw address (the latter logging
  the 12 bytes of the vector it is handed and writing 12 bytes where the
  vertex goes), and the group's own called directly: the two cylinders (the
  point by its 12 bytes), the trails (`kByte 0..2`; the timer by its low
  word, the record by its low byte, which is all each reads).
- **The draws:** `Gfx_PacketNext` points into a packet buffer of the fuzz's
  own (a region), and the link and commit stand-ins log each primitive's
  bytes before moving the pointer on by the size, as the real ones do - the
  cylinder builds 48 primitives at the one pointer, so without the log only
  the last would be compared. The primitive setters scribble the bytes they
  would write, so a store made before the setter where the original makes it
  after shows.
- **Louder stand-ins** (each part of the time, from `Noise`):
  `ScriptFlags_Set40` moves the message word (0x44 or beside it),
  `Cond_ByteFE` and the running object (area 143's choice 6 and trigger 50
  read the first two after it); `Effect_FindFree` moves the active member
  (area 144's spawn reads it after); `MapView_GroundAt` answers the running
  object's word `+0x3E` or one either side and moves the object (the drop's
  compare, and both states' re-reads); `Field_LeaderStepTick` moves the
  running object; `Sprite_SetAnimation` and `MoveCmd_Move` move the script
  object and the running object. The harness's own disturbance moves
  `Sprite_Current` and `Frame_Counter` now and then.
- **Data tables** swapped for recorders: `Area143_EffectStates`,
  `Area145_DropStates`, `Area146_EffectStates`.
- **Regions beyond the field frame**, per area (the harness holds 40 with
  its own twenty): all twenty `Effect_Objects` records; the script object,
  focus object, active member and chapter row pointers; the marks
  `0x9398CF..D1`; the dword `0x904650`; `Cond_ByteFE`; the kind-2 object;
  `MoveScript_PartyRecords` records 0..5; `Text_Records`' first 16 bytes;
  `Field_Kind2Hold`; `Camera_Angles`; `MoveScript_WaitWordDA`;
  `Draw_OtSlot`; `Gfx_PacketNext`, the packet buffer, the name buffer, a
  point; for area 143 the eleven CLUT rows it reads and the eleven it writes
  (31, 25, 32 and 27 regions; 34,160, 18,944, 23,014 and 22,886 bytes).
- **Seeds:** the answer at each value a choice tests, 1, a negative byte;
  `0x904650` at `0x3FFFF` and one bit either side (above bit 17 and bit 31
  too); the message word at `0x44` and beside it; each member's `+0x89` a key
  of the function's table (or one past) with the member count 0 a tenth of
  the time; the leader's `+0x89` at each value the scene handler tests; the
  third record's `+0x89` at 4, 2 and beside; `Cond_ByteFD` at each value a
  hook tests and beside; the pose at 0, 7, 6 and beside; each hook's high
  words in, on the edge of and past their spans with a high byte (the
  compares are 16-bit), area 145's z at exactly `0x98000` and beside, the
  leader's high word the same; the cell hook's x and z bytes a record's (or
  one past) with any higher bytes and the pose the record's nibble or with a
  high bit; the walks' z a whole step from the target, 0, +-1, `+-0x8000`
  and far; `Field_ActiveMember` on and between the records of
  `Sprite_Objects` / `Sprite_ObjectsExtra` and just below the first (the
  quotient truncates toward 0); `Field_State` the same about `ObjTrio` for
  the drop's party index; the effect record as `Sprite_Current` with state 0
  or 1; the tail at every state its tables name, their neighbours, the
  states that do nothing, negative bytes, with the cell each state waits on
  at its value two times in three (counter 3 0 or `0x40`, the timer 0 or
  beside, `Field_Kind2Hold`, the wait word); the trails' timer at, below and
  past each leg's length with any high word; the record at 1, 2, 0, 3 and a
  high byte; the CLUT shift at 0..5, `0x1F..0x21`, `0x101` and any.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3 (0 or `0x40` half the time), the timer, the three pointers,
  `Field_Kind2Hold`, the wait word, `Cond_ByteFE`, the answer.

**Result (in this worktree):** 318,000 rounds over the 53 functions (6,000
each), 2,793,6xx calls to the stand-ins, 0 mismatches. COVERAGE_LINE

`BOF3X_SHADOW='*'`: STAR_LINE

## 4. Controls

CONTROLS_SECTION

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (21): `Area143_Choices`
`0x630C48` (13), `Area143_Handlers` `0x630C6C` (4), `Area143_MemberKeys`
`0x630CC4` (4 bytes), `Area143_MemberMessages` `0x630CC8` (4 words),
`Area143_EffectStates` `0x630CD0` (2), `Area144_Handlers` `0x631748` (9),
`Area144_Choices` `0x631764` (2), `Area145_Choices` `0x633670` (12),
`Area145_Handlers` `0x633674` (11), `Area145_DropStates` `0x6336E4` (2),
`Area145_MemberKeys1` / `5` / `B` `0x6336EC` / `0x6336F8` / `0x633704` (4
bytes each, their four message words after each), `Area145_CellRecords`
`0x633710` (7 x 4 bytes), `Area145_TrailLegs0` `0x63372C`, `TrailLegs1A` /
`1B` `0x63373C` / `0x63374C`, `TrailLegs2A` / `2B` `0x63375C` /
`0x63376C` (4 x 4 bytes each), `Area146_Handlers` `0x6340C0` (3),
`Area146_EffectStates` `0x634114` (2). As in world 2, a descriptor's `+0x34`
array is the tail of (or overlaps) its `+0x3C` array (areas 143, 144, 145).
Tail kind 20's two tables are in `.text`, inside its extent.

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area143_EffectB3Run`, `Area146_EffectB4Run`
  (by the effect record's `+1`) and `Area145_RunDrop` (by the object's `+4`)
  jump through two-entry tables; an index of 2 or more jumps through the
  dword after the table (`0x8F3818`, 0 and `0x4020805`: not code; area 146's is a null pointer).
  Ours aborts with a message there (the owner's rule: no DIVERGENCE entry).
  The drop's own states store 1 and 2 into `+4`, and 2 (landed) is past its
  table: what keeps `Area145_RunDrop` from running at state 2 is the script,
  which calls handler 1 again only while a state steps it back (the
  position - 2); not measured in play.
- **Records past their tables, kept.** The member search reads up to
  `Field_MemberCount` party records (not bounded by three); area 145's
  handler 0 writes `MoveScript_PartyRecords` record i for any byte i the
  active member's distance gives (records 6 and 7 lie over `Cond_ByteFA` ..
  `0x8034FF`, beyond them the write runs on through `0x80447F`); area 144's
  spawn stores the member's index as a byte. None faults; ours does the same.
- **Effect slots unchecked.** Every spawn writes the record at
  `Effect_Objects + slot << 7` for any slot but `0xFF`; `Effect_FindFree`
  answers only 0..19 or `0xFF`, so nothing reaches past.
- **Tail state `0xA` repeats itself when no effect slot is free**: story
  flag `0x2C`, sound `0x200` and `Kind2_Place(0)` run again every frame
  until one frees (the state moves on only with a slot). Faithful.
- **Object trigger 50 arms tail kind 4 without its state when
  `Cond_ByteFE` is already set**: the state byte is left as the last tail
  left it. What tail kind 4 (engine) does with a stale state is not read.
- **Area 145's init is a bare `ret` on the PC** where the PSX has one
  (`0x801F5324`, not read here): either the port moved its work elsewhere or
  dropped it. For the owner's attention; nothing measured.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 53 starts, extents, call sites,
  the one in-function jump table (`0x4216D0`: 18 entries at `+0x3E0`,
  through a byte table of 35 at `+0x428`), and the shape its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding (`nop` runs).
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x420A60`, `0x421590`, `0x422060` (object triggers 50, 40, 32),
  `0x420A90` (called by chapter 12's code only), `0x420B00` / `0x422080`
  (`Effect_KindHandlers` `0xB3` / `0xB4`, whose second states are area 143's
  and 146's data roots), `0x421F10` (`EffectKind18_States[96]`). Each sits in
  the block of the area named.
- **The tool's register-armed tail kinds:** none in this band; tail kind 20
  is armed by immediates (the step hook) and by a byte read from the cell
  records (the cell hook arms the kind by immediate, the state from the
  table). The tool's `tail_kinds` column says 10 for area 143: kind 10 is
  `0x56DB80` (engine), armed by both of the other blocks' choices area 143
  names (7 `0x425C30` and 8 `Area22_ArmTailOnYes`), not by this band.
- **`0x421B20..0x421EA0`** have no root of their own: only tail kind 20's
  chain calls them.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 53). Every function is reached only in play: the choices
  when the area's message box asks, the handlers from its movement scripts,
  the step and cell hooks on a step in the area, tail kind 20 once armed,
  the triggers by an object's trigger id, the effect kinds `0xB3` / `0xB4`
  and state 96 of kind `0x18` while such an effect lives, the CLUT shift
  from chapter 12's scene, `0x4220D0` from the effect states that call it.

## 9. Calls across groups

- **Raw addresses nobody owns**, in `area_w3f_callees.h`: `0x494060` (the
  map camera) and `0x494110` (a point projected to a vertex) - engine, no
  group's (magic_c2 and magic_s32 call them raw too).
- **By name, Capcom's:** `MoveCmd_Move`.
- **By name, ours:** `ScriptFlags_Set40` / `Clear40`, `Effect_FindFree`,
  `KeyItem_Has` / `Add`, `Kind2_Place`, `Transition_Start`,
  `MoveCmd_MoveKind2`, `Item_NamePtr`, `MapView_GroundAt`,
  `Field_LeaderStepTick`, `Sprite_SetAnimation`, `Sprite_ReleaseTint`,
  `Sound_PlayEffect`, `Party_DropIn`, `Msg_OpenScript`, `Msg_OpenSystem`,
  `Field_ChangeArea`, `Flags_Set` / `Clear` / `Test`, `Gfx_CommitPrim`,
  `MapView_LinkPrimAt`, `Gpu_SetDrawMode`, `Gpu_SetPolyG4`,
  `Gpu_SetSemiTrans`, `Gpu_GetTPage`, `Math_Sin`, `Math_Cos`. No harness
  edit; no `AH_THEIRS` moved.
- **Table entries of other groups', read in place:** `Area59_EffectGround`
  (`0x40B4F0`, AR1D) is state 0 of kinds `0xB3` and `0xB4`;
  `Area57_StopMusic` / `Area57_ResumeSound` (AR1D) are area 145's handlers 9
  and 10; `0x425C30` (world 4, not taken) and `Area22_ArmTailOnYes` (AR0B)
  are area 143's choices 7 and 8.
- **Inbound, for the rebinding pass:**
  - `0x4220D0` (`Area146_DrawGlowCylinder`) is called by raw address from
    `area_w0c.cpp` (area 36's effect state, `0x405022`), `area_w1d.cpp`
    (area 59's, `0x40B542`), `area_w2d.cpp` (area 100's, `0x4144D2`), from
    areas 112 and 116's states (AR2F's and AR3A's this wave) and from engine
    effect states at `0x475CE2` and `0x47EF32` (the rel32 calls; the
    engine's stay as they are).
  - `0x420A90` (`Area143_ClutShiftRight`) by raw address from
    `scena_sc13_callees.h` / `scena_sc13.cpp` (chapter 12's code at
    `0x562027`, `0x5620BA`).
  - `Area_StepHook` (`0x56E050`, ours in `event_ops.cpp`) calls `0x420A10`,
    `0x4215B0`, `0x422000` through its raw `kStepHandlers` table, now
    `Area143_StepHook`, `Area145_StepHook`, `Area146_StepHook`.
  - Read in place by tables, no caller to rebind: `Area_CellHooks` (area
    `0x91`: `Area145_CellHook`), `Field_ModeTailKinds[20]`,
    `Field_ObjectTriggers` 32, 40, 50, `Effect_KindHandlers` `0xB3`, `0xB4`,
    `EffectKind18_States[96]`; the choice and handler tables of areas 3,
    36, 37, 41, 50, 55, 59, 61, 68, 74, 91, 98, 100, 112, 113, 116, 148,
    149, 167 and 197 name `0x420850`, `0x420870`, `0x421FB0`, `0x4209F0`,
    `0x422350`, `0x422370`.
- `analysis/calltrace/entries_logic.txt`: 49 lines appended under a
  `# group AR3F` comment (four of the 53 were listed already at the same
  extent); the hosts `00420A10 7D`, `00420A90 BB`, `00420B50 A5D`,
  `004215B0 56C`, `00421EA0 153`, `00422000 CB`, `004220D0 8E6` ran over the
  band (the consolidation keeps the smaller).
