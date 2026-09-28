# World 3, areas 124..125, 127..128 and 130..134: the band `0x41C890..0x41DAD0`

**Status:** IN PROGRESS (2026-09-28) - 56 functions ours
(`src/game/area_w3c.cpp`, shadow name `area_w3c`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 336,000 rounds (in this worktree); CONTROLS_SUMMARY. Fuzz only:
no recorded route reaches the band (section 8). No divergence, no abort
added: the band has no dispatch through a `.data` table and no divide
(section 6).

Group AR3C of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
56 starts, none ours before, **56 taken**; no start dropped, none added
(section 7). Areas 126 and 129 have no code anywhere (their descriptors,
`0x626F00` and `0x627B08`, have no `+0x34`, `+0x3C` or `+0x40`).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, tails, triggers and the effect
state have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kState` a state handler
of `EffectKind18_States`, `kCallee` a function called directly (by the band's
own code or the engine's) or an object trigger `(object, 0x904030)` answering
in `al`. The three functions that are both a choice and a handler: the two
that write the message word are fuzzed as choices, `Area128_PlaceObject`
(which does not) as a handler.

### Areas 124 and 125 (descriptors `0x6265C8`, `0x626700`; PSX `0x801F2DE0`, `0x801F37F0`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41C890` | `Area124_PlaceRandomObject` | `0xCB` | 124 init (PSX `0x801F2D2C`) | kInit | a `jmp` over eleven `nop`s; `Rand() & 0x3F` walked down `Area124_Weights` (8), the first it falls below chosen (none: 8); field objects 0..7 but the chosen one get `+0` = 0; the chosen one at `Area124_Cells[Rand() & 7]` (bytes `<< 16`), `+0x3E` the ground there; `Field_EdgeBits` = the leader's dword `+0x134` less 5 either way |
| `0x41C960` | `Area125_PlaceRandomObject` | `0xCB` | 125 init (PSX `0x801F362C`) | kInit | the same over `Area125_Weights` / `Area125_Cells` |

Both are areas 72 and 73's init compiled again (`Area72_PlaceRandomObject`,
[`area_w1f.md`](area_w1f.md)): instruction for instruction the same, only
the two table addresses differ. Ours is one body over two tables. The two
areas' tables hold the same bytes, and the eight weights sum to 64, so with
`& 0x3F` a weight is always found: the "none" branch (only `Field_EdgeBits`
set, every one of the eight objects hidden) is unreachable with the image's
tables; it is kept.

### Area 127 (descriptor `0x626FE8`; PSX `0x801F2D10`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41CA30` | `Area127_ClearActive80` | `0x3E` | handler 0 (PSX `0x801F2C04`) | kHandler | `Field_ActiveMember`'s byte `+0x80` bit 0 cleared; the leader's `+0x89` at 5: story flag `0x32`, `MoveCmd_TestFB(0x4E, 0x33)`, then `Sprite_Current` (read after the calls) `+0` = 0 |

### Area 128 (descriptor `0x627A88`; PSX `0x801F3DA8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41CA70` | `Area128_ChoiceStep3` | `0x37` | choice 0 | kChoice | the answer read, message `0xFFFF`; answer 0: the focus object's word `+0x8A` = `0xB`, counter 0 = `0xF`, the step byte `0x8034E5` = 3; else the step 8 |
| `0x41CAB0` | `Area128_ChoiceArmTail56` | `0x30` | choice 1 | kChoice | answer 0: `ScriptFlags_Set40`, tail kind 56 at state `0xA`, message `0x49`; else message `0xFFFF` |
| `0x41CAE0` | `Area128_PlaceObject` | `0x56` | choice 2 = handler 0 (PSX `0x801F3030`) | kHandler | the running object's x `0x4E8000`, y 0, z `0x88000` when `Field_State +0x89` is 2 else `0x80000`, `+8` = 1 (`Sprite_Current` read again for every store) |
| `0x41CB40` | `Area128_TailLeave56` | `0x86` | tail kind 56 | kTail | state 0, request not 2: `ScriptFlags_Clear40`, `Party_DropIn(6)`, story flag `0x88`, the tail cleared; state `0xA`, request not 2: `ScriptFlags_Clear40`, story flag `0x89`, `Field_ChangeArea(0x79, 0x2A0000, 0x1A0000, 7)`, the tail cleared; any other state nothing |
| `0x41CBD0` | `Area128_Trigger61` | `0x21` | object trigger 61 (a gap of the tool) | kCallee | `ScriptFlags_Set40`, tail kind 56 at state 0, the object's word `+0x8A` + 1; al 0 |
| `0x41CC00` | `Area128_InitPatches` | `0xB5` | init (PSX `0x801F31D0`) | kInit | the previous area `0x79`: story flag `0x43` set, `0x42` cleared, the patch chain applied (area 94's walk); then the chapter (`Cond_ByteFA`, signed) above 10 and flag `0x43` set: cells (`0x1F`/`0x20`, `0x39`) `0xC0` and (`0x1F`/`0x20`, `0x3A`) `0xA1` |

Choice 3 = handler 1 is `0x4285D0`, another block's. The init's patch walk
is area 94's (`Area94_InitPatches`, [`area_w2c.md`](area_w2c.md)) with the
flags the other way round (94 sets `0x42` and clears `0x43`; 128 sets `0x43`
and clears `0x42`); ours shares the walk (`ApplyPatchChain`).

### Area 130 (descriptor `0x628C80`; PSX `0x801F500C`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41CCC0` | `Area130_ChoiceRunStep` | `0x4D` | choice 0 = handler 16 (PSX `0x801F35B0`) | kChoice | message `Area130_ChoiceMessages[s8 answer]`; answer 0: the four counters 0, the step `0xA`, `MoveScript_Var7` 6; answer 1: counter 0 = 0 |
| `0x41CD10` | `Area130_SpawnKind4AtMember0` | `0x42` | handler 0 (PSX `0x801F363C`) | kHandler | `Effect_Spawn(4, 0, Area130_EffectArgsA[list 0], record 0's +0x2E, +0x30)`, `Sprite_Current` made the record; not `0xFF`: its `+0xB` |
| `0x41CD60` | `Area130_SpawnKind3AtMember0` | `0x42` | handler 1 (PSX `0x801F36BC`) | kHandler | kind 3, record 0, args B |
| `0x41CDB0` | `Area130_DroppedCall1` | `0x9` | handler 3 (PSX `0x801F3760`) | kHandler | `Port_DroppedCall(1)` |
| `0x41CDC0` | `Area130_DroppedCallTrack83` | `0x12` | handler 5 (PSX `0x801F37B8`) | kHandler | `Port_DroppedCall(0)`, `Music_Track` = `0x83` |
| `0x41CDE0` | `Area130_SpawnKind3AtMember1` | `0x42` | handler 6 (PSX `0x801F37F0`) | kHandler | kind 3, record 1, args B |
| `0x41CE30` | `Area130_SpawnKind3AtMember2` | `0x46` | handler 7 (PSX `0x801F3870`) | kHandler | kind 3, record 2, args B |
| `0x41CE80` | `Area130_Effect78AtObject` | `0x4C` | handler 8 (PSX `0x801F38F0`) | kHandler | `Effect_FindFree`; a slot: `+0` = 1, `+5` = `0x78`, x / z / y the running object's |
| `0x41CED0` | `Area130_SpawnKind1AtMember1` | `0x42` | handler 9 (PSX `0x801F397C`) | kHandler | kind 1, record 1, args B |
| `0x41CF20` | `Area130_SpawnKind1AtMember2` | `0x46` | handler 10 (PSX `0x801F39FC`) | kHandler | kind 1, record 2, args B |
| `0x41CF70` | `Area130_SpawnKind4AtMember1` | `0x42` | handler 11 (PSX `0x801F3A7C`) | kHandler | kind 4, record 1, args C |
| `0x41CFC0` | `Area130_SpawnKind4AtMember2` | `0x46` | handler 12 (PSX `0x801F3AFC`) | kHandler | kind 4, record 2, args C |
| `0x41D010` | `Area130_SpawnKind5AtMember1` | `0x42` | handler 13 (PSX `0x801F3B7C`) | kHandler | kind 5, record 1, args C |
| `0x41D060` | `Area130_SpawnKind5AtMember2` | `0x46` | handler 14 (PSX `0x801F3BFC`) | kHandler | kind 5, record 2, args C |
| `0x41D0B0` | `Area130_GiveKeyItem10` | `0x9` | handler 15 (PSX `0x801F3C7C`) | kHandler | `KeyItem_Add(0xA)` |
| `0x41D0C0` | `Area130_Trigger65` | `0x16` | object trigger 65 (a gap) | kCallee | `ScriptFlags_Set40`, tail kind 63 with sub-kind 0; al 0 |
| `0x41D0E0` | `Area130_TailGiveItem` | `0x18C` | tail kind 63 (a gap) | kTail | by the step byte `0x8034E5` (its state): 0, request not 2 - by the byte `0x903F6A` (0..5, a six-entry jump table in `.text`; else none) one of six (category, item): the name copied into `Text_Records` (`strncpy(Text_Records, Item_NamePtr(c, i), 0x10)`) and `Inventory_Add(c, i, 1)`; then `Text_Records + 0x2F` = 0, message `0x49`, request 2, step 1. 1, request not 2: sound `0x106`, story flag `0x98`, `ScriptFlags_Clear40`, the tail kind and sub-kind 0 |
| `0x41D270` | `Area130_ChoiceTailState2` | `0x1A` | the choice 0 of the ten world-map areas, area 131's choice 1, area 187's choice 1, areas 33 and 121's handlers 2 / 3 | kChoice | answer 0: the tail state 2; message `0xFFFF` |

Handlers 2 and 4 are `Area67_MusicFade10` (`0x40CE10`) and
`Area39_FadeOutMusic` (`0x405750`), ours already (table entries, read in
place; no call to rebind). The six (category, item) pairs are code
immediates; the pick byte `0x903F6A` has no reader named in this repo. The
`Inventory_Add` call pushes a fourth word 0 that `Inventory_Add` (three
arguments, `symbols.toml`) never reads; ours passes three.
`Area130_ChoiceTailState2` lies in area 130's block but area 130's tables do
not name it: it is the choice every world-map area's "leave?" box commits
through ([`worldmap_area.md`](worldmap_area.md)); it is fuzzed under 130.

### Area 131 (descriptor `0x629910`; PSX `0x801F3CB8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41D290` | `Area131_ChoiceFocusStepA` | `0x6A` | choice 2 | kChoice | answer 0: `ScriptFlags_Set40`; counter 0 = 0, `MoveScript_Var7` 7, the step `0xA`; the focus object's `+1` = 4, `+0x84` = 2, `+0x83` = 4, word `+0x8A` = 0; message `0xC`. Else message `0xFFFF` |
| `0x41D300` | `Area131_ChoiceFocusStep0` | `0x5E` | choice 3 | kChoice | message `0xFFFF`; answer 0: the same scene at step 0 with `+0x83` = 2 |
| `0x41D360` | `Area131_CameraShiftYLess1E` | `0x10` | handler 4 (PSX `0x801F2E18`) | kHandler | `Camera_ShiftY` - `0x1E`, redraw |
| `0x41D370` | `Area131_CameraShiftYMore1E` | `0x10` | handler 5 (PSX `0x801F2E40`) | kHandler | `Camera_ShiftY` + `0x1E`, redraw |
| `0x41D380` | `Area131_Trigger15` | `0xA` | object trigger 15 (a gap) | kCallee | tail kind 34; al 0 |
| `0x41D390` | `Area131_TailLeave34` | `0x9C` | tail kind 34 (a gap) | kTail | by the s8 state (a four-entry jump table in `.text`; negative or above 3 nothing): 0 - `ScriptFlags_Set40`, message 2, request 2, the state (read after the call) + 1; 1, request not 2 - `Area131_DisarmTail` (a tail `jmp`); 2, request not 2 - state 3; 3 - `Field_ScriptFlags` ^ `0x16`, `Area131_DisarmTail`, the byte `0x904152` 0, story flag `0x77` cleared, `Field_ChangeArea(0x79, 0x1A0000, 0x350000, 3)` |
| `0x41D430` | `Area131_DisarmTail` | `0x17` | called by tail kind 34 and by the engine | kCallee | `ScriptFlags_Clear40`; the tail's kind, state and sub-kind 0 |

Choice 0 is `0x437CC0` (outside the band), choice 1
`Area130_ChoiceTailState2`. Handlers 0..3 are areas 7 and 49's camera shifts
(`Area07_CameraShiftYLess` / `More`, `Area49_ShiftCameraDown` / `Up`), 6 and
7 `0x42A4B0` and `0x41F320` (other blocks), 8 `Area94_Counter1FromLeaderPose`,
9 and 10 `Area67_Object0XDown` / `Up` (`0x40CDE0`, `0x40CAC0`): table
entries, read in place. The two choices' scene is one helper
(`FocusScene`): the focus pointer read once before the counters and again
for each later store, as the originals.

### Area 132 (descriptor `0x62A598`; PSX `0x801F4A44`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41D450` | `Area132_AnimByBit4` | `0x1F` | handler 0 (PSX `0x801F3A24`) | kHandler | the running object's `+8` bit 4: animation `0x40`, else `0x41` |
| `0x41D470` | `Area132_Effect73` | `0x57` | handler 1 (PSX `0x801F3A64`) | kHandler | an effect record of kind `0x73` at (`0x2A0000`, `0x1D0000`), `+1` = 0, `+0x3C` the ground `<< 16` |
| `0x41D4D0` | `Area132_Effect73Pair` | `0xD3` | handler 2 (PSX `0x801F3AEC`) | kHandler | two of kind `0x73` at (`0x2E0000` / `0x2F0000`, `0x1C0000`), `+1` = 1, `+0xB` 1 / 0; each `0x200` above the ground and x (read after the call) less `0x8000` |
| `0x41D5B0` | `Area132_Effect74` | `0x53` | handler 3 (PSX `0x801F3C1C`) | kHandler | one of kind `0x74` at (`0x2B0000`, `0x280000`) on the ground; `+1` not written |
| `0x41D610` | `Area132_ClearFE` | `0x8` | handler 4 (PSX `0x801F3CA0`) | kHandler | `Cond_ByteFE` = 0 |
| `0x41D620` | `Area132_EffectGradient` | `0xAA` | `EffectKind18_States` 95 (`0x6541E8`; a gap) | kState | `Cond_ByteFE` 0: `Effect_Release`. `Draw_PassFlags` bit 2: draw mode tpage `0x95` committed (slot 7, `0xC`); at `Gfx_PacketNext` (read again) a semi-transparency-off `POLY_G4` over (0, 0)..(320, 200), the top vertices (0, `0xC8`, `0xFF`) and the bottom (0, 0, `0x20`), committed (slot 7, `0x44`): a screen-wide gradient |

Handler 4 clears the byte the gradient state waits on: while `Cond_ByteFE`
is set the effect draws each frame; once it is 0 the record is released
(and, as the original, still drawn that frame). What spawns the effect
record whose sub-kind is 95 is not read here (`EffectKind18_Start` sets the
state from `+0xB`, [`worldmap_area.md`](worldmap_area.md) section 6).

### Area 133 (descriptor `0x62B5C0`; PSX `0x801F3DAC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41D6D0` | `Area133_ChoiceFocusPair` | `0x69` | choice 0 = handler 4 (PSX `0x801F2C1C`) | kChoice | the focus object's dwords `+0x18` / `+0x1C` = the byte pair `Area133_ChoicePairs[s8 answer]` (the focus pointer and the answer read again for the second), message `0xFFFF`; answer 2: the byte `0x929F0F` 0, `ScriptFlags_Set40`, `Draw_PassFlags` 0, the step `0x1E`, `MoveScript_Var7` 4 |
| `0x41D740` | `Area133_SpawnKind3AtMember0` | `0x42` | handler 0 (PSX `0x801F2CEC`) | kHandler | kind 3, record 0, `Area133_EffectArgs` |

Handlers 1..3 are `Area100_ShiftCameraDown4` / `Up4` and
`Area94_Counter1FromLeaderPose` (table entries).

### Area 134 (descriptor `0x62C120`; PSX `0x801F6374`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x41D790` | `Area134_SpawnKind4AtMember0` | `0x42` | handler 0 (PSX `0x801F5114`) | kHandler | kind 4, record 0, args B |
| `0x41D7E0` | `Area134_CameraShiftXMore2` | `0x12` | handler 1 (PSX `0x801F5194`) | kHandler | `Camera_ShiftX` + 2, redraw |
| `0x41D800` | `Area134_CameraShiftXLess2` | `0x10` | handler 2 (PSX `0x801F51BC`) | kHandler | `Camera_ShiftX` - 2, redraw |
| `0x41D810` | `Area134_CameraShiftXReset` | `0x11` | handler 3 (PSX `0x801F51E4`); area 8's choice 7 = handler 6 | kHandler | `Camera_ShiftX` 0, redraw |
| `0x41D830` | `Area134_SpawnKind1AtMember0` | `0x42` | handler 4 (PSX `0x801F5200`) | kHandler | kind 1, record 0, args A |
| `0x41D880` | `Area134_CameraShiftYMore8` | `0x10` | handler 5 (PSX `0x801F5280`) | kHandler | `Camera_ShiftY` + 8, redraw |
| `0x41D890` | `Area134_CameraShiftYLess8` | `0x10` | handler 6 (PSX `0x801F52A8`) | kHandler | `Camera_ShiftY` - 8, redraw |
| `0x41D8A0` | `Area134_SpawnKind3AtMember1` | `0x42` | handler 7 (PSX `0x801F52D0`) | kHandler | kind 3, record 1, args A |
| `0x41D8F0` | `Area134_SpawnKind3AtMember2` | `0x46` | handler 8 (PSX `0x801F5350`) | kHandler | kind 3, record 2, args A |
| `0x41D940` | `Area134_SpawnKind1AtMember1` | `0x42` | handler 10 (PSX `0x801F53EC`) | kHandler | kind 1, record 1, args A |
| `0x41D990` | `Area134_SpawnKind1AtMember2` | `0x46` | handler 11 (PSX `0x801F546C`) | kHandler | kind 1, record 2, args A |
| `0x41D9E0` | `Area134_Effect90AtObject` | `0x50` | handlers 12 **and** 13 (PSX `0x801F54EC`, `0x801F557C`) | kHandler | kind `0x90` at the running object, `+1` = 0 |
| `0x41DA30` | `Area134_Effect93AtObject` | `0x4C` | handler 14 (PSX `0x801F560C`) | kHandler | kind `0x93` at the running object |
| `0x41DA80` | `Area134_Effect99AtObject` | `0x4C` | handler 15 (PSX `0x801F5698`) | kHandler | kind `0x99` at the running object |

Handler 9 is `Area80_ResetCameraShift` (a table entry). **Handlers 12 and 13
name one body on the PC** where the PSX has two functions
(`names/area_records.toml`: `0x801F54EC` and `0x801F557C`); whether the PSX
pair differs, or the port's linker folded two identical bodies, is not read
here (the PSX bytes were not compared). It is the PC's table as it is; no
divergence from the PC is involved.

## 2. Ours

`src/game/area_w3c.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
one raw address of section 9). The group's own callee is called the same
way (`AH_CALL(Area131_DisarmTail)`), so the fuzz stands a recorder in for it
and every function is tested alone. Shapes that repeat are one helper: the
random placement (`PlaceRandomObject`, two inits), the party-list spawns
(`SpawnAtMember`, seventeen handlers - area 94's shape), the effect at the
running object (`EffectAtObject`, four), area 132's effects at a fixed cell
(`EffectAtCell`, three calls), the patch walk (`ApplyPatchChain`) and area
131's talk scene (`FocusScene`). Kept as the originals: every re-read after
a call (`Sprite_Current` for the spawns' `+0xB` and area 127's `+0`, the
tail state in tail kind 34's state 0, `Gfx_PacketNext` after the first
commit, the effect's x after `AreaMap_Elevation` in area 132's pair, the
focus pointer and the answer in area 133's choice), the order of every call
and of the stores the originals make around them, the signed reads (the
answers of areas 130 and 133, the tail state of kind 34, the chapter), the
sign extension of the ground, the 16-bit camera words and the object's
16-bit `+0x8A`.

## 3. The fuzz

`BOF3X_SHADOW=area_w3c` (`src/game/area_w3c_fuzz.cpp`): nine `Run`s under
the one shadow name, one per area with its `Group::area` (124, 125, 127,
128, 130, 131, 132, 133, 134), 6,000 rounds per function, the real
descriptors and tables in place. `Area130_ChoiceTailState2` runs under 130,
`Area131_DisarmTail` under 131.

- **The two inits** open with Capcom's own `jmp` over eleven `nop`s:
  `bof3::CloneOriginal` refuses that entry as already patched, so the clones
  are the bodies (`0x41C8A0`, `0x41C970`, `0x10` on, call sites `0x10` less
  than the tool's rows), as AR1F's areas 72 and 73. `BOF3_INJECT` patches the
  real entry.
- **Callees the group lists** (beyond the harness's standard set):
  `Effect_Spawn` (`kByte 0xFE..0x02`, the byte and words masked as pushed
  with stale high bits) and `Effect_FindFree` (`kByte 0xFF..0x03`), both
  moving `Sprite_Current` half the time; `ScriptFlags_Set40` / `Clear40`;
  `MoveCmd_TestFB` (`kFlag`); `KeyItem_Add`; `Port_DroppedCall` (a byte);
  `Item_NamePtr`; the CRT `strncpy` by its raw address (section 9);
  `Effect_Release`; `Gfx_CommitPrim`; the group's own `Area131_DisarmTail`
  (`kPhase`).
- **Louder stand-ins** (an `effect`, from `Noise` only): `Effect_Spawn`,
  `Effect_FindFree`, `Flags_Set` and `MoveCmd_TestFB` (area 127) move
  `Sprite_Current`; area 131's `ScriptFlags_Set40` moves the focus pointer
  and now and then the tail state, its `Msg_OpenScript` the tail state;
  area 128's `Flags_Set` / `Flags_Clear` move the chapter byte (read after
  the patch walk); `AreaMap_ApplyPatch` rewrites the entry's step (read
  again after the call); `AreaMap_Elevation` moves an effect record's x and
  answers a sign edge a fifth of the time; `Effect_Release` moves
  `Draw_PassFlags`; the primitive setters scribble the primitive
  (`Gpu_SetDrawMode` `0xC` bytes, `Gpu_SetPolyG4` `0x44`, `Gpu_SetSemiTrans`
  its code byte's bit 1) and `Gfx_CommitPrim` moves the packet cursor on by
  the size, kept inside the fuzz's own 1 KiB packet buffer. The first run
  drew those effects' picks with `AH_PICK`, which draws from the harness's
  `Next()`, and made 1,727 false mismatches in area 128 (the brief's rule
  for a group `disturb` applies to effects as well: they run on both passes).
- **Regions beyond the field frame:** all twenty `Effect_Objects` records
  (130, 132, 134), `Field_ActiveMember` (127), `Camera_ShiftX` .. the focus
  pointer `0x903800..0x903807` (128, 131, 133, 134), the previous area word
  (128), the pick byte `0x903F6A` and `Text_Records`' first `0x30` bytes
  (130), the packet cursor, the packet buffer, `Draw_PassFlags` and
  `Cond_ByteFE` (132), the byte `0x929F0F` and `Draw_PassFlags` (133).
- **Seeds:** the answer at every value a choice tests (0, 1, 2), its
  neighbours, the sign edge and above; the party list bytes inside the
  eight-entry tables two times in three; area 124 / 125's `Rand` hinted at
  each cumulative weight edge and the leader's `+0x134` around 5; area 127's
  leader `+0x89` at 5 and beside; area 128's `Field_State +0x89` at 2 and
  beside, its tail state at 0, `0xA` and beside, `Field_Request` 2 a third
  of the time, the previous area `0x79` two times in three (else beside, a
  high byte), the patch chain (AR2C's `SeedChain`: every step lands inside
  the chain or on a zero), the chapter at 10 / 11 and the sign edge; area
  130's tail step at 0, 1, 2 and above with the pick 0..5, 6 and above;
  area 131's tail state 0..4 and the sign edge; area 132's `+8` bit 4, the
  pass flags' bit 2, `Cond_ByteFE` 0 a third of the time; area 133's answer 2
  often, 0..5, and the sign edge.
- **The group's disturbance** (from the hash it is given): the focus
  pointer, the answer, the tail state, `Field_Request`, the step byte, the
  list bytes, the packet cursor, the pass flags, `Cond_ByteFE`.

**Result (in this worktree):** 336,000 rounds over the 56 functions (6,000
each), COUNTS_LINE, 0 mismatches. Coverage: every callee each function can
reach was called - e.g. `Effect_Spawn` 60,000 (area 130) / 36,000 (134),
`Port_DroppedCall` 12,000, `Item_NamePtr` / `strncpy` / `Inventory_Add` 457
each (tail kind 63's give), `Msg_OpenScript` 669 (130) and 639 (131),
`Field_ChangeArea` 534 (128) and 715 (131), `Party_DropIn` 518,
`Area131_DisarmTail` 1,107, `AreaMap_ApplyPatch` 16,361, `Effect_Release`
2,051, the gradient's draw 2,736.

STAR_LINE

## 4. Controls

CONTROLS_TEXT

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (23): `Area124_Cells`
`0x6265AC` (8 pairs), `Area124_Weights` `0x6265BC` (8), `Area125_Cells`
`0x6266E8`, `Area125_Weights` `0x6266F8`, `Area127_Handlers` `0x626FE0` (1),
`Area128_Choices` `0x627A74` (4), `Area128_Handlers` `0x627A7C` (2),
`Area130_EffectArgsA` / `B` / `C` `0x627B50` / `0x627B5C` / `0x627B68` (8
signed bytes each), `Area130_Handlers` `0x628C38` (17), `Area130_Choices`
`0x628C78` (1), `Area130_ChoiceMessages` `0x628CC4` (2 words),
`Area131_Choices` `0x629898` (4), `Area131_Handlers` `0x6298E4` (11),
`Area132_Handlers` `0x62A580` (5), `Area133_EffectArgs` `0x62A5E0` (8),
`Area133_Handlers` `0x62AF88` (5), `Area133_Choices` `0x62AF98` (1),
`Area133_ChoicePairs` `0x62B604` (6 byte pairs), `Area134_EffectArgsA` /
`B` `0x62B610` / `0x62B61C` (8 each), `Area134_Handlers` `0x62C0E0` (16). As
elsewhere, area 133's `+0x34` array is the tail of its `+0x3C` array, and
area 128's `+0x3C` the tail of its `+0x34`. The effect-argument tables lie
after the descriptor of the area before (the data-block rule of
[`area-rows.md`](area-rows.md) section 2); each is read by one area's code
only. The two jump tables (`0x41D254`, 6 entries; `0x41D41C`, 4) are in
`.text`, inside their functions' extents.

## 6. Latent defects

Described, not fixed (none is new in kind):

- **Unchecked reads that stay in `.data`, kept:** the party-list spawns
  index their argument tables by a list byte (a character id; 0..7 in play,
  a byte past reads the next table); `Area130_ChoiceRunStep` reads its
  message word and `Area133_ChoiceFocusPair` its byte pair by the signed
  answer (a negative answer reads before the table; the message box's
  answers are small).
- **Effect slots unchecked.** Every spawn writes the record at
  `Effect_Objects + slot << 7` for any slot but `0xFF`; `Effect_FindFree`
  answers only 0..19 or `0xFF` (its `symbols.toml` evidence), so nothing
  reaches past.
- **The inits' "none" branch** (every one of the eight objects hidden) is
  unreachable with the image's weights (they sum to 64 and the roll is
  `& 0x3F`); a table edited to sum lower would reach it. Kept.
- **`Area130_TailGiveItem` names an item by a byte nothing here writes**
  (`0x903F6A`); a value past 5 gives nothing but still opens message
  `0x49` with the previous contents of `Text_Records`.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 56 starts, extents, call sites,
  both in-function jump tables (`0x41D0E0`: 6 entries at `+0x174`;
  `0x41D390`: 4 at `+0x8C`), and the shape its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding (`nop` runs), read.
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x41CBD0`, `0x41D0C0`, `0x41D380` (object triggers 61, 65, 15),
  `0x41D0E0`, `0x41D390` (tail kinds 63 and 34, armed by immediates in
  triggers 65 and 15; tail kind 56 is also armed by the immediate in
  `Area128_ChoiceArmTail56`, which the tool read), `0x41D430` (the engine's
  five calls), `0x41D620` (`EffectKind18_States` 95).
- **The tool's register-armed tail kinds:** none in this band.
- **`area_rows.py` lists `0x41D270` under area 130's block as `shared`**:
  it is right that no area 130 table names it (section 1).

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 56). Every function is reached only in play: the choices
  when the area's message box asks (and `Area130_ChoiceTailState2` from
  every world map's), the handlers from the areas' movement scripts, the
  inits on entry, the tails once armed, the triggers by an object's trigger
  id, the gradient while an effect of kind `0x18` sub-kind 95 lives, and
  `Area131_DisarmTail` from the five engine phases. The world-map route
  enters area 115 and 121 (AR3A, AR3B), not this band; whether it commits
  the world map's "leave?" choice (`0x41D270`) is not measured here.

## 9. Calls across groups

- **Raw addresses nobody owns**, in `area_w3c_callees.h`: `0x5B9450`, the
  C runtime's `strncpy` (read: copies up to n bytes, a NUL ends the copy,
  the rest zeroed). A `Crt_` name is the rebinding pass's to give.
- **By name, Capcom's:** `Rand`, `Effect_Spawn`.
- **By name, ours:** `AreaMap_Elevation`, `AreaMap_ApplyPatch`,
  `AreaMap_SetByte`, `Flags_Set` / `Clear` / `Test`, `MoveCmd_TestFB`,
  `ScriptFlags_Set40` / `Clear40`, `Party_DropIn`, `Field_ChangeArea`,
  `Effect_FindFree`, `Effect_Release`, `Port_DroppedCall`, `KeyItem_Add`,
  `Item_NamePtr`, `Inventory_Add`, `Msg_OpenScript`, `Sound_PlayEffect`,
  `Sprite_SetAnimation`, `Gpu_SetDrawMode`, `Gpu_SetPolyG4`,
  `Gpu_SetSemiTrans`, `Gfx_CommitPrim`. No harness edit; no `AH_THEIRS`
  moved. The other areas' functions the brief names (`Area39_FadeOutMusic`,
  `0x40CAC0`, `0x40CDE0`, `0x40CE10`, `Area94_Counter1FromLeaderPose`) are
  entries of areas 130, 131 and 133's handler tables, not calls: nothing in
  the band calls them, so there is nothing to call by name.
- **Inbound** (for the rebinding pass): the engine calls `0x41D430`
  (`Area131_DisarmTail`) at `0x456E29`, `0x4570C9`, `0x457362`, `0x457582`,
  `0x4575CE` (after its own tail phases; `symbols.toml` bounds all five inside
  `Field_RunSlot` `0x455300`, which reads as several functions). Named by
  other areas' tables (read in place): `Area130_ChoiceTailState2` (areas 16,
  33, 45, 65, 87, 88, 115, 121, 131, 151, 152, 187), `Area134_CameraShiftXReset`
  (area 8). By engine tables: `Field_ModeTailKinds` 34, 56, 63;
  `Field_ObjectTriggers` 15, 61, 65; `EffectKind18_States` 95.
- `analysis/calltrace/entries_logic.txt`: 56 lines appended under a
  `# group AR3C` comment; every one was inside a host line (`0041C5B0 E7C`,
  `0041D430 11AC`) that runs over the band (the consolidation keeps the
  smaller; `0041D430` now has both sizes).
