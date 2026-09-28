# World 2, areas 90, 91, 92 and 94: the band `0x411F10..0x4135B0`

**Status:** IN PROGRESS (2026-09-28) - 45 functions ours
(`src/game/area_w2c.cpp`, shadow name `area_w2c`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 270,000 rounds (in this worktree); CONTROLS_SUMMARY. Fuzz
only: no recorded route reaches the band (section 8). No divergence; the
areas' unchecked reads stay in `.data` and are kept, and area 91's handler 0
aborts where the original would loop forever or jump through its
descriptor (section 6).

Group AR2C of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
45 starts, none ours before, **45 taken**; no start dropped, none added
(section 7). Areas 90, 91, 92 and 94 have code; **area 93's descriptor
(`0x614F30`) names none** (its `+0x34`, `+0x3C` and `+0x40` are null).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s
rows, each read against the disassembly. What an area *is* in the story is
not read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, triggers and the effect state
have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice (the answer byte `0x7DEE67` in, the message word
`0x7DEE48` read after), `kHandler` a `+0x3C` handler (movement-script ops
`03` / `DE`), `kInit` the `+0x40` init, `kState` a state handler reached
through a `.data` table (area 91's own, or the engine's
`EffectKind18_States`), `kCallee` a function called directly (by the
group's own code, or as an object trigger `(object, 0x904030)` answering
in `al`). A function that is both a choice and a handler (area 92's
`+0x3C` array is the tail of its `+0x34` table, area 94's `+0x34` table the
tail of its `+0x3C` array) is fuzzed by the shape the table order gives it
first; none of the handlers touches the message word, and every choice
leaves it.

### Area 90 (descriptor `0x614588`; PSX `0x801F40EC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x411F10` | `Area90_ChoiceFocusPair` | `0x3C` | choice 0 | kChoice | message `0xFFFF`; the talked-to object (`0x903804`) gets dwords `+0x18` / `+0x1C` from the byte pair of `Area90_FocusPairs` by the s8 answer; the pointer and the answer read again for the second |

Area 90's `+0x3C` is null and its `+0x40` is `0x437CC0`, another block's
shared `ret`; the PSX descriptor names an init of its own (`0x801F2C5C`).
Not read further here (a PC / PSX difference in the record, not a function
of this band).

### Area 91 (descriptor `0x614748`; PSX `0x801F3B60`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x411F50` | `Area91_ChoiceTakeItem59` | `0x58` | choice 0 | kChoice | answer not 0: message 7, the mark `0x9398CF` 6; 0: `Inventory_Count(0, 0x59, 0)` as a word not 0 - `Inventory_Remove(0, 0x59, 1)`, message 5; else message 6 and the mark |
| `0x411FB0` | `Area91_ChoiceMessage7` | `0x24` | choices 3 and 4 | kChoice | answer not 0: message 7 and the mark; 0: message `0xFFFF` |
| `0x411FE0` | `Area91_ObjectRun` | `0x12` | handler 0 (PSX `0x801F2D6C`) | kHandler | `jmp [Area91_States + Sprite_Current[4] * 4]`, unchecked (section 6) |
| `0x412000` | `Area91_State0CheckItem57` | `0x5C` | state 0 | kState | the first party list byte not 6 or 3: the active member's `+0x80` bit 0 cleared; else `Inventory_Count(0, 0x57, 0)` as a word 0: `Cond_ByteFE` 1, the bit cleared, sound `0x201`; not 0: `ScriptFlags_Set40`, state + 1 |
| `0x412060` | `Area91_State1Arm` | `0x32` | state 1 | kState | the leader's byte `+0x137` 0: sound `0x200`, `+0xA` = `+0xB` = 0, state + 1 |
| `0x4120A0` | `Area91_State2Grow` | `0x9A` | state 2 | kState | `+0xA` + 1; at `0x1E`: story flag `0x6B` clear - set it, `Party_DropIn(0)`; set - `Party_DropIn(1)`; `Sprite_Current` put back, `Field_Kind2X` / `Z` (`0x1F8000`, `0x1A0000`), `MoveScript_F3Divisor` `0x20`, state + 1; always the glow at `+0xA * 10` |
| `0x412140` | `Area91_DrawGlow` | `0x1C1` | called by states 2..5 | kCallee | a stack MATRIX (translation `Gte_RotTrans` of (`0xCFC0`, `0xCD00`, `0x80`), rotation of (0, 0, 0), times `Camera_Matrix`), a draw mode (tpage `0x3E`), then a fan of 32 semi-transparent `POLY_G3`s of radius the first argument's word, centre colour (c, c, 0) with c the second argument's byte, rim black, each linked at (`0x1F8000`, `0x1A0000`) |
| `0x412310` | `Area91_State3Place` | `0xBA` | state 3 | kState | `Field_Kind2Hold` 0: the object at (`0x1F8000`, `0x1A0000`), `+0x70` 0, word `+0x3E` `0xFF00`, bit 6 of `+0` cleared, state + 1; its CLUT row (`+0x27`) in `Gfx_ClutStrip` zeroed, `Gfx_ClutStripDirty` + 1, `+0xB` 0, `+0xA` 2; always the glow at `0x12C` |
| `0x4123D0` | `Area91_State4RevealClut` | `0x126` | state 4 | kState | `+0xA` - 1; the row's words `+0xB` and `0x1F - +0xB` copied back from `Gfx_ClutStripSource`, halved (`& 0x3DEF`) while `+0xA` is not 0; at `+0xA` 0 the next pair (`+0xA` 2), after 16 pairs `+0xA` `0x1E` and state + 1; always `Gfx_ClutStripDirty` + 1 and the glow at `0x12C` |
| `0x412500` | `Area91_State5Shrink` | `0x67` | state 5 | kState | `+0xA` - 1; at 0: counter 3 = 1, state 0, the active member's bit cleared and word `+0x8A` + 1, `ScriptFlags_Clear40`; always the glow at `+0xA * 10` |
| `0x412570` | `Area91_Trigger49` | `0x16` | object trigger 49 | kCallee | `ScriptFlags_Set40`, tail kind 4 (engine `0x56D930`) with sub-kind `0x10`; al 0 |
| `0x412590` | `Area91_EffectRings` | `0x30A` | `EffectKind18_States` 71 (`0x654188`) | kState | while `Cond_ByteFE`: three rings about the effect's screen point - 64 `LINE_F2`s on an ellipse and 16 orbiting `TILE_1`s each, radius `+9 - 0x20k`, colour `0x10k - +9 + 0xC8`; `+9` + 4 a frame; the last ring's colour 0 clears `Cond_ByteFE` |

The six states are one object's cycle, as read: state 0 checks the party
and item `0x57` (without it: `Cond_ByteFE` on, which the rings draw by);
state 1 waits on the leader's byte `+0x137`; state 2 grows the glow for 30
frames and drops the party in (entry 0 the first time, flag `0x6B`, entry 1
after); state 3 waits on `Field_Kind2Hold`, places the object and blanks
its CLUT row; state 4 restores that row from both ends two words every
second frame, the first frame of each pair at half brightness; state 5
shrinks the glow for 30 frames, ends the script's wait (counter 3) and
goes back to state 0. The rings are an effect of kind `0x18` whose
sub-kind is 71 (`EffectKind18_Start` sets `+1` from `+0xB`); what spawns it
is not in this band (no immediate 71 read here).

### Area 92 (descriptor `0x614E78`; PSX `0x801F416C`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4128A0` | `Area92_ChoiceRunStep` | `0x9F` | choice 0 | kChoice | message word by the s8 answer; answers 2 / 3 / 4: the scene start (below) with `MoveScript_Var7` 8 and step `0xA` / `0x1E` / `0x14` |
| `0x412940` | `Area92_ChoiceRun0` | `0x4A` | choice 1 | kChoice | message word; answer 0: Var7 8, step 0 |
| `0x412990` | `Area92_ChoiceRun28` | `0x4B` | choice 2 | kChoice | message word; answer 0: Var7 8, step `0x28` |
| `0x4129E0` | `Area92_ChoiceRunByFlag1B` | `0xC0` | choice 3 | kChoice | message word; answer 0: Var7 7, step `0x3C`; 1: the chapter row's flag `0x1B` clear - step `0x12`, set - `0x23` |
| `0x412AA0` | `Area92_SpawnKind4AtMember0` | `0xAD` | handler 0 = choice 4 (PSX `0x801F2E9C`) | kHandler | `Effect_Spawn`'s body written out: kind 4 at party record 0, the third byte `Area92_EffectArgs4` by list byte 0 (section 2) |
| `0x412B50` | `Area92_SpawnKind3AtMember0` | `0xAD` | handler 1 (PSX `0x801F304C`) | kHandler | kind 3, record 0, `Area92_EffectArgs3` |
| `0x412C00` | `Area92_SpawnKind3AtMember1` | `0xAD` | handler 2 (PSX `0x801F31FC`) | kHandler | kind 3, record 1 |
| `0x412CB0` | `Area92_SpawnKind3AtMember2` | `0xB1` | handler 3 (PSX `0x801F33AC`) | kHandler | kind 3, record 2 |
| `0x412D70` | `Area92_SpawnKind1AtMember0` | `0xAD` | handler 4 (PSX `0x801F355C`) | kHandler | kind 1, record 0, `Area92_EffectArgs1` |
| `0x412E20` | `Area92_SpawnKind1AtMember1` | `0xAD` | handler 5 (PSX `0x801F3708`) | kHandler | kind 1, record 1 |
| `0x412ED0` | `Area92_SpawnKind1AtMember2` | `0xB1` | handler 6 (PSX `0x801F38B4`) | kHandler | kind 1, record 2 |
| `0x412F90` | `Area92_PlayMusic3D` | `0xD` | handler 7 = choice 11 (PSX `0x801F3A60`) | kHandler | `Music_Play(0x3D, 8)` |
| `0x412FA0` | `Area92_Trigger37` | `0x1D` | object trigger 37 | kCallee | `ScriptFlags_Set40`, tail kind `0x2C` with state 0 and sub-kind `0xB`; al 0 |

**The scene start** the choices of areas 92 and 94 share: `ScriptFlags_Set40`,
the movement script's four counters (`0x903848..4B`) 0, the step byte after
`MoveScript_Var7` (`0x8034E5`) and `MoveScript_Var7` itself (areas 48, 49
and 52 write the same pair, [`area_w1c.md`](area_w1c.md)).

### Area 94 (descriptor `0x616D70`; PSX `0x801F52F8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x412FC0` | `Area94_ChoiceMessage0` | `0x17` | choice 0 = handler 13 (PSX `0x801F2C04`) | kChoice | message word by the s8 answer |
| `0x412FE0` | `Area94_ChoiceMessage1` | `0x17` | choice 1 = handler 14 (PSX `0x801F2C30`) | kChoice | the same, its second list |
| `0x413000` | `Area94_ChoiceSetFlag15` | `0x2B` | choice 2 = handler 15 (PSX `0x801F2C5C`) | kChoice | message word; answer 1: the row's flag `0x15` |
| `0x413030` | `Area94_ChoiceRunByFlags16` | `0xD3` | choice 3 = handler 16 (PSX `0x801F2CB4`) | kChoice | message word; answer 1: flag `0x16` clear - set it (the row read again), Var7 7, step 0; set - flag `0x1A` clear: step `0x19`, set: `0x32` |
| `0x413110` | `Area94_ChoiceRunByFlag20` | `0xB1` | choice 4 = handler 17 (PSX `0x801F2DC4`) | kChoice | message word; answer 1: Var7 7, step `0x32`; 2: flag `0x20` clear - Var7 9, step 0; set - step 2 |
| `0x4131D0` | `Area94_SpawnKind2AtMember0` | `0x42` | handler 0 (PSX `0x801F2ED0`) | kHandler | `Effect_Spawn(2, 0, Area94_EffectArgsA[list 0], record 0 +0x2E, +0x30)`, `Sprite_Current` made the record; a slot not `0xFF` to its `+0xB` |
| `0x413220` | `Area94_SpawnKind3AtMember1` | `0x42` | handler 1 (PSX `0x801F2F50`) | kHandler | kind 3, record 1, args A |
| `0x413270` | `Area94_SpawnKind3AtMember2` | `0x46` | handler 2 (PSX `0x801F2FD0`) | kHandler | kind 3, record 2, args A |
| `0x4132C0` | `Area94_SpawnKind4AtMember0` | `0x42` | handler 3 (PSX `0x801F3050`) | kHandler | kind 4, record 0, args B |
| `0x413310` | `Area94_ClearCells` | `0x25` | handler 4 (PSX `0x801F30D0`) | kHandler | map cells (`0x52`, `0x32..0x34`) 0 |
| `0x413340` | `Area94_SpawnKind1AtMember1` | `0x42` | handler 5 (PSX `0x801F3118`) | kHandler | kind 1, record 1, args C |
| `0x413390` | `Area94_SpawnKind1AtMember2` | `0x46` | handler 6 (PSX `0x801F3198`) | kHandler | kind 1, record 2, args C |
| `0x4133E0` | `Area94_Counter1FromLeaderPose` | `0xB` | handler 7 (PSX `0x801F3218`); also areas 131, 133 | kHandler | counter 1 = the leader's byte `+8` |
| `0x4133F0` | `Area94_SpawnKind3AtMember0` | `0x42` | handler 8 (PSX `0x801F3230`) | kHandler | kind 3, record 0, args D |
| `0x413440` | `Area94_SpawnKind4AtMember1` | `0x42` | handler 9 (PSX `0x801F32B0`) | kHandler | kind 4, record 1, args B |
| `0x413490` | `Area94_SpawnKind4AtMember2` | `0x46` | handler 10 (PSX `0x801F3330`) | kHandler | kind 4, record 2, args B |
| `0x4134E0` | `Area94_GiveKeyItem7` | `0x18` | handler 11 (PSX `0x801F33B0`) | kHandler | `KeyItem_Add(7)`, the row's flag `0x1E` (the row read after the call) |
| `0x413500` | `Area94_SpawnKind4AtMember0E` | `0x42` | handler 12 (PSX `0x801F33E0`) | kHandler | kind 4, record 0, args E |
| `0x413550` | `Area94_InitPatches` | `0x5E` | init (PSX `0x801F3460`) | kInit | the previous area (`0x802290`) `0x79`: story flag `0x42` set, `0x43` cleared, and the area block's patch chain from `AreaMap_PatchBase` applied (`AreaMap_ApplyPatch` per entry to a zero dword) |

`Area94_Counter1FromLeaderPose` is the band's one **shared body**: areas 131
(`+0x3C[8]`) and 133 (`+0x3C[3]`) name it too (the tool's `shared` row);
taken once, here, keyed by address. The five args tables A..E are eight
signed bytes by a party list byte; A, C and D hold the same eight bytes
(a byte past the eighth tells them apart).

## 2. Ours

`src/game/area_w2c.cpp`, calling out only through the harness
(`AH_CALL(name)` for every callee, ours or Capcom's; this group has **no
raw-address callee**). The group's own callee is called the same way
(`AH_CALL(Area91_DrawGlow)`), so the fuzz stands a recorder in for it and
each state is tested alone. Shapes that repeat are one helper with the
function's constants: the scene start (`RunStep`), area 92's seven spawns
(`SpawnInline`: `Effect_Spawn`'s body as `0x57CE10` has it - `+0` 1, `+5` 6,
`+6` the kind, `+0xC` 0, `+0x10` the signed third byte, words `+0x2E` /
`+0x30` - but storing the slot in the record's `+0xB` before the test for
none), area 94's ten (`SpawnAt`), the state dispatcher (`StateEntry`).
Kept as the originals: every re-read after a call (`Sprite_Current`, the
active member, the chapter row pointer, the patch entry's step, the frame
counter in the rings' tile loop, `Gfx_PacketNext` per primitive), the order
of every call, the stack MATRIX whose translation `Gte_RotTrans` writes in
place, the x87 arithmetic of the rings (`fild` / `fadd` / `fstp` at the
game's 53-bit precision: a double add rounded once to a float, as
`psx_gte_float.cpp` has it), the 16-bit test of `Inventory_Count`'s answer,
and the unchecked table reads of section 6.

## 3. The fuzz

`BOF3X_SHADOW=area_w2c` (`src/game/area_w2c_fuzz.cpp`): four `Run`s under
the one shadow name, one per area with its `Group::area` (90, 91, 92, 94),
6,000 rounds per function, the real descriptors and tables in place.

- **Area 91's draws.** The glow's matrix set-up and projection
  (`Gte_PushMatrix` / `PopMatrix`, `Gte_RotTrans`, `Gte_RotMatrix`,
  `Gte_MulMatrix0`, `Gte_SetRotMatrix` / `SetTransMatrix`,
  `Gte_RotTransPers3`, `Gte_PrimDepths3_10B`) run for real on both sides
  (`Answer::kThrough`): they read and write the stack MATRIX and the
  primitive, and `Camera_Matrix` and `Gte_Matrix` are regions of random
  bytes each round, so a changed vector or product moves the projected
  points. The rings' `Gte_RotTransPers` is a recorder (its vertex logged,
  a screen point written - a small whole number mostly, any float bits one
  time in seven) and `Gte_StoreDepthF` one (a float into the primitive).
  `Math_Sin` / `Math_Cos` answer garbage (both sides the same), so every
  `imul` / `sar` is tested on any 32 bits. The primitive setters scribble
  their primitive's bytes and `Gpu_SetSemiTrans` its bit, so a store made
  before the call shows; `MapView_LinkPrimAt` moves `Gfx_PacketNext` on by
  the size four times in five, inside the fuzz's own 4 KiB packet buffer.
- **Louder stand-ins** (half the time, from `Noise`): after
  `ScriptFlags_Set40` / `Clear40`, `Sound_PlayEffect`, `Party_DropIn`,
  `Effect_FindFree`, `Effect_Spawn` and `Math_Sin`, `Sprite_Current` moves;
  `Math_Cos` moves `Sprite_Current[9]` or the frame counter one time in
  eight each; `Flags_Test` (area 94) and `KeyItem_Add` move the chapter row
  pointer; `AreaMap_ApplyPatch` rewrites the entry's step (0..3) half the
  time; `Inventory_Count` answers a word whose low byte alone is 0 a third
  of the time.
- **Regions beyond the field frame:** the talked-to object pointer (area
  90); the active member pointer, `Cond_ByteFE`, the mark, `Field_Kind2Hold`,
  both CLUT strips with `0x200` bytes either side (`0x80B380 + 0x8400`),
  `Gfx_PacketNext` and the packet buffer, `Prim_VertexScratch`,
  `MapView_ScreenXY`, `Camera_Matrix`, `Gte_Matrix` (area 91; 31 regions,
  54,211 bytes); every effect record a slot byte can name
  (`Effect_Objects + 0xFF * 0x80`: a disturbed `+0xB` reaches past the four
  the stand-in answers) and the row pointer (area 92); the row pointer and
  the previous area (area 94).
- **The state table** `Area91_States` is a `DataTable` of six (the harness
  swaps its entries for recorders while `Area91_ObjectRun` runs).
- **Seeds:** the choice answer at each value a choice tests (0..5), its sign
  edge and above, 0 two times in three for area 91's; the party list bytes
  inside the eight-byte tables two times in three, any byte else (the reads
  past them stay in `.data`); `Area91_ObjectRun`'s state 0..5; state 0's
  list byte at 6 / 3 and beside; state 1's leader byte 0; state 2's `+0xA`
  at `0x1D` (the step to `0x1E`) and beside; state 3's `Field_Kind2Hold` 0;
  state 4's `+0xA` at 1 / 2 / 0 and `+0xB` at `0xF` (the step to `0x10`),
  0..`0x1F` and any; state 5's `+0xA` at 1; the glow's radius word at the
  values the states push (`0x12C`, tenths of 0..`0x1E`) or any, its colour
  `0xFF` or any; the rings' `Cond_ByteFE` 0 a quarter of the time and `+9`
  at each ring's radius edge (`0x1C`, `0x3C`, `0x5C` and beside, before the
  `+ 4`) and the last colour's 0 (`0xE4` and beside) and its wrap (`0xFC`);
  area 94's previous area at `0x79` two times in three and beside
  (`0x78`, `0x7A`, `0x179`, `0x7900`, 0); the patch chain - `AreaMap_PatchBase`
  at a dword of the block's first 7 KiB, then 0..24 dwords not 0 with steps
  0..3 (a zero now and then ends it early), then four zero dwords, so any
  step from any entry lands inside the chain or on a zero; the triggers'
  `(object, 0x904030)`.
- **The group's disturbance** (from the hash it is given): area 90 the
  talked-to object; area 91 `Cond_ByteFE`, the active member,
  `Field_Kind2Hold`, the packet cursor, the leader's byte, the list byte;
  areas 92 and 94 a list byte, the answer, the row pointer.

**Result (in this worktree):** 270,000 rounds over the 45 functions (6,000
each), 9,717,654 calls to the stand-ins, 0 mismatches. Coverage: every
callee each function can reach was called - e.g. `Inventory_Remove` 2,885,
`Party_DropIn` 1,130, `Area91_DrawGlow` 24,000 (from the four states),
`Gte_RotTransPers` 4,445, `Gpu_SetPolyG3` 192,000, `Gpu_SetLineF2` 853,440,
`Gpu_SetTile1` 213,360, each of the six states through the dispatcher
967..1,029, area 92's `Flags_Test` 682, area 94's `AreaMap_ApplyPatch`
16,148, `Flags_Clear` 4,018, `Effect_Spawn` 60,000.

STAR_RESULT

## 4. Controls

CONTROLS_SECTION

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (19): `Area90_Choices`
`0x614584` (1), `Area90_FocusPairs` `0x6145CC` (12 bytes: six pairs before
`0x6145D8`, a dword area 91's descriptor names), `Area91_Choices`
`0x614714` (5; entries 1 and 2 are `0x420850` / `0x420870`, another
block's), `Area91_States` `0x614728` (6), `Area91_Handlers` `0x614740` (1),
`Area91_RingTurns` `0x61478C` (3 signed bytes), `Area92_EffectArgs3`
`0x614790` and `Area92_EffectArgs1` `0x614798` (8 each; after area 91's
descriptor, read only by area 92's code - the tool's data-block rule
attributes them to 92), `Area92_Choices` `0x614E44` (12), `Area92_Handlers`
`0x614E54` (8), `Area92_ChoiceMessages` `0x614EBC` (12 words),
`Area92_EffectArgs4` `0x614ED4` (8), `Area94_EffectArgsA` / `B` / `C`
`0x614F78` / `0x614F80` / `0x614F88` (8 each; after area 93's descriptor,
read only by area 94's code), `Area94_Handlers` `0x616D24` (18),
`Area94_Choices` `0x616D58` (5, the array's tail), `Area94_ChoiceMessages`
`0x616DB4` (20 words), `Area94_EffectArgsD` / `E` `0x616DDC` / `0x616DE4`.

## 6. Latent defects

Described, not fixed; each is kept faithfully unless it would fault:

- **`Area91_ObjectRun` dispatches unchecked.** The state byte indexes
  `Area91_States` (six); the seventh dword is area 91's `+0x3C` array,
  whose one entry is `Area91_ObjectRun` itself - so a state of 6 jumps to
  itself forever (a hang), and 7 and above jump through the descriptor's
  words (null, `0x614600`, `0x614710`, `0x675B70` ...: a fault). Ours aborts
  with a message for any state past 5 (the round's rule for an index past
  its table; round9 doc section 6). Nothing read here writes a state past
  5 (the six states step 0 -> 1 -> ... -> 5 -> 0); a movement script's
  write to the object's `+4` is the only other way.
- **Unchecked indexes into data.** Every message table by the s8 answer
  (area 90's pairs, areas 92 and 94's words); the effect-argument tables by
  a party list byte (areas 92, 94). The reads stay in `.data`.
- **Area 91's CLUT rows are indexed by the object's bytes unchecked.** State
  4's `+0xB` counts 0..`0x10` in play; past `0x1F` the words it copies lie
  in the rows either side (below `Gfx_ClutStripSource` by up to 448 bytes
  at row 0), past the strip's end for the last rows. Kept; the fuzz's
  region covers every index the two bytes can make.
- **`Area94_InitPatches` walks with no bound**: a chain with no zero dword
  runs on through memory (as `Area11_DimBackdrop`'s header walk,
  [`area_011.md`](area_011.md)). Read, not run; the fuzz builds chains that
  end.
- **Area 92's spawns store the slot before the test**, so a full effect
  pool (`Effect_FindFree` `0xFF`) leaves `0xFF` in the party record's
  `+0xB`, where `Effect_Spawn`'s callers (area 94's, op `90..95`) leave the
  old slot. Whatever reads `+0xB` next (op `9F` waits on it) then sees
  "none". Not measured in play.
- **The rings' clamp to `0xFF` cannot fire**: the colour is at most
  `0x20 + 0xC8 = 0xE8`. Dead code in the original, kept.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 45 starts, extents, call sites, and
  the shape its root gives. **Dropped:** none. **Added:** none; every gap
  between the band's functions is padding.
- **The state table's count**: the tool's note on `0x411FE0` says "7 code
  entries" at `0x614728`; the seventh is `Area91_Handlers` (the handler
  itself), not a state (section 6). The `DataTable` is six.
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x412140` `Area91_DrawGlow` - called by states 2..5;
  `0x412570` `Area91_Trigger49` and `0x412FA0` `Area92_Trigger37` -
  `Field_ObjectTriggers` ids 49 and 37 (by an object's `+0x86` from area
  data); `0x412590` `Area91_EffectRings` - `EffectKind18_States` entry 71
  (`0x654188`), an engine table.
- **Tail kinds armed here**: kind 4 (engine `0x56D930`) by
  `Area91_Trigger49`, kind `0x2C` by `Area92_Trigger37`, both immediates
  (the tool's scan sees them); neither is a function of this band.

## 8. What reaches it

- **No recorded route reaches the band** (`analysis/hidden_reached_*.json`,
  `pc_hidden_reached.json`: no entry or host in `0x411F10..0x4135B0`;
  `area_funcs.tsv`'s live column is empty for all 45). Every function is
  reached only in play: the choices when the area's message box asks, the
  handlers from its movement scripts, the triggers by an object's id, the
  init on entry to area 94, the rings once an effect of kind `0x18` with
  sub-kind 71 runs.

## 9. Calls across groups

- **Raw addresses nobody owns:** none (the group's callees are all named).
- **By name, Capcom's:** `Effect_Spawn`.
- **By name, ours:** `Inventory_Count`, `Inventory_Remove`, `KeyItem_Add`
  (SX), `Party_DropIn`, `Sound_PlayEffect`, `Music_Play`,
  `ScriptFlags_Set40` / `Clear40`, `Flags_Test` / `Set` / `Clear`,
  `Effect_FindFree`, `AreaMap_SetByte`, `AreaMap_ApplyPatch`,
  `MapView_LinkPrimAt`, the `Gte_*`, `Gpu_*` and `Math_*` of section 3. No
  harness edit; no `AH_THEIRS` moved.
- **Inbound from outside the band:** `EffectKind18_States` entry 71
  (`0x654188`, engine `.data`) names `Area91_EffectRings`; areas 131 and 133's
  `+0x3C` tables name `Area94_Counter1FromLeaderPose`; `Field_ObjectTriggers`
  names the two triggers. All read in place by the engine; no caller to
  rebind.
- `analysis/calltrace/entries_logic.txt`: 45 lines appended under a
  `# group AR2C` comment; none of the 45 was there; the host `00412140 1840`
  covers 37 of them and runs on to `0x413980` (the consolidation keeps the
  smaller).
- **`inject_all.cpp`:** `AreaW2c_Inject` is placed before `DrawPool_Grow`,
  which says it must run after every module's self-test; `ScenaSc13_Inject`
  (merged in wave three) sits after it, for the coordinator.
