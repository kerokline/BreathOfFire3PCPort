# World 1, areas 38..41: the band `0x4053B0..0x406650`

**Status:** IN PROGRESS (2026-09-28) - 48 functions ours
(`src/game/area_w1a.cpp`, shadow name `area_w1a`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 288,000 rounds (in this worktree); CONTROLS_SUMMARY (section 4).
Fuzz only: no recorded route reaches the band (section 8). No divergence;
the two dispatchers through an area's state table abort past it (section 6).

Group AR1A of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 9). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
48 starts, none ours, 48 to take. **48 taken; none dropped, none added**
(section 7). Area 40 is the band's one large piece: a 4 x 4 floor puzzle
run by a mode tail, an arrive hook and five helpers.

Every function was read to its last instruction with capstone
(2026-09-28, the scratch `dz.py` over the band end to end); each extent
agrees with the tool's `analysis/area_funcs.tsv`; the clone tables are the
tool's `--clones` rows, each checked against the reading. What an area *is*
in the story is not read here. The PSX twins are the sibling's
`names/area_records.toml` (descriptor handlers and inits only; choices,
tails, hooks and triggers have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` an arrive hook
`(x, z)` answering in `al`, `kState` an entry of a state table in the area's
`.data`, `kCallee` a function the area's own code calls directly, or an
object trigger (`Field_ObjectTriggers` `0x662E20` by object `+0x86`, called
`(object, 0x904030)`, answering in `al`). A function that is both a choice
and a handler is fuzzed as a handler (none of them touches the message
word).

### Area 38 (descriptor `0x5F2208`; PSX `0x801F3D48`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4053B0` | `Area38_ClearCells` | `0x25` | handler 0 (PSX `0x801F2C04`) | kHandler | map bytes x `0xC`, z `0x1E`, `0x1C`, `0x1D` set to 0 |
| `0x4053E0` | `Area38_SetCellsC0` | `0x2E` | handler 1 (PSX `0x801F2C4C`) | kHandler | the same cells to `0xC0` |
| `0x405410` | `Area38_SpawnEffectMember1` | `0xBA` | handler 2 (PSX `0x801F2C94`) | kHandler | with the party-list byte `0x904063` 0: `Sprite_Current` made party record 1 (and left so), its `+0xB` the free effect slot; the effect record `+0` 1, `+5` 6, `+6` 3, `+0xC` 0, `+0x10` the signed `Area38_EffectBytes[list byte, read again]`, `+0x2E` / `+0x30` the record's words |
| `0x4054D0` | `Area38_SpawnEffectMember2` | `0xBE` | handler 3 (PSX `0x801F2E60`) | kHandler | the same for `0x904064` and party record 2 (the list byte read as a dword and masked, the same byte) |

No choices, no init.

### Area 39 (descriptor `0x5F3E60`; PSX `0x801F4BCC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x405590` | `Area39_Run` | `0x12` | handler 0 (PSX `0x801F2C04`) | kHandler | `Area39_States` by `Sprite_Current[4]` |
| `0x4055B0` | `Area39_DriftStart` | `0x22` | `Area39_States[0]` | kState | `+0xA` = `0x40`, state 1; `Field_ActiveMember`'s word `+0x8A` less 2 |
| `0x4055E0` | `Area39_DriftStep` | `0x62` | `Area39_States[1]` | kState | while `+0xA`: `+0x34` by the signed `Area39_DriftSteps[+0xA & 0xF] << 11`, `+0x38` by its negation `<< 11`, `+0xA` less 1, the member's word less 2; at 0 state 0 |
| `0x405650` | `Area39_SpawnEffect36` | `0x41` | handler 3 (PSX `0x801F2D54`) | kHandler | a free effect slot: `+0` 1, kind `0x36`, `+9` 0; none: the member's word `+0x8A` less 2 (the script waits) |
| `0x4056A0` | `Area39_ClearCells` | `0x1A` | handler 4 (PSX `0x801F2DE4`) | kHandler | cells (`0x43`, `0x36`), (`0x44`, `0x36`) 0 |
| `0x4056C0` | `Area39_SetCells51` | `0x1A` | handler 5 (PSX `0x801F2E1C`) | kHandler | cells (`0x58`, `0xD`), (`0x59`, `0xD`) `0x51` |
| `0x4056E0` | `Area39_SetCells50` | `0x1A` | handler 6 (PSX `0x801F2E54`) | kHandler | the same cells `0x50` |
| `0x405700` | `Area39_PlayMusicA3` | `0x10` | handler 10 (PSX `0x801F2EC4`) | kHandler | `Music_Play(0xA3, 8)` |
| `0x405710` | `Area39_SetScriptFlag8` | `0x8` | handler 11 (PSX `0x801F2EEC`) | kHandler | `Field_ScriptFlags` bit 3 set (a byte `or`) |
| `0x405720` | `Area39_SwapKeyItemE` | `0x21` | handler 12 (PSX `0x801F2F0C`) | kHandler | the first of the 32 key-item bytes `0x904554` holding `0xE` becomes 4; none: `0x591900(4)` (the key item 4 added) |
| `0x405750` | `Area39_FadeOutMusic` | `0x9` | handler 13 (PSX `0x801F2F68`); area 130's handler 4 | kHandler | `Music_FadeOutStop(0x1E)` |
| `0x405760` | `Area39_Counter1Not4` | `0x10` | handler 14 (PSX `0x801F2F94`) | kHandler | counter 1 `0x903849` = the first party-list byte `0x904062` is not 4 |

Handlers 1 and 2 are `0x413C80` / `0x413CA0` (group AR2D's block, shared
by areas 39, 41 and 99), 7 and 8 the bare `ret` `0x437CC0`, 9 `0x40B2D0`
(AR1D's, shared by four areas). No choices, no init.

**The shared body `0x405750`** is area 39's handler 13 and area 130's
handler 4: taken once, keyed by address, fuzzed under area 39.

### Area 40 (descriptor `0x5F4BD8`; PSX `0x801F491C`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x405770` | `Area40_PlaceObject` | `0x2F` | choice 8 = handler 0 (PSX `0x801F2C04`) | kHandler | a free field object's index (`0x57CD90`) to the scratch word `0x903850`; none (`0xFF`): the running script back 2 (the op again next frame); else `EventOp_0x(Area40_PlaceOp)` |
| `0x4057A0` | `Area40_NudgeObject` | `0x1B` | choice 9 = handler 1 (PSX `0x801F2C74`) | kHandler | with the leader's `+0x89` 2, the running object's `+0x34` up `0x8000` |
| `0x4057C0` | `Area40_ChoiceFlag5` | `0x42` | choice 0 | kChoice | message `0xFFFF`, the byte after `MoveScript_Var7` `0x1C` (answer 0) or `0x1E`; flag 5 of the chapter's row; `Field_ScriptFlags` bits 3, 7 and `Field_StatusBits` bit 6 cleared |
| `0x405810` | `Area40_ChoiceLeverB` | `0x31` | choice 1 | kChoice | message `0xFFFF`; answer 0: flag `0xB` of the row `0x903FB0`, `Area40_SetGate`, sound `0x203` |
| `0x405850` | `Area40_ChoiceLeverA` | `0x31` | choice 2 | kChoice | the same, flag `0xA` |
| `0x405890` | `Area40_ChoiceLever9` | `0x31` | choice 3 | kChoice | flag 9 |
| `0x4058D0` | `Area40_ChoiceLever8` | `0x31` | choice 4 | kChoice | flag 8 |
| `0x405910` | `Area40_SetGate` | `0x9B` | called by the four levers | kCallee | the four lever flags (the low nibble of `0x903FB1`) at 5: the 28 gate cells x `0x41..0x44` by z 3..9 set to 0, al 1; else `0xA4`, al 0 (the callers ignore al) |
| `0x4059B0` | `Area40_ChoiceTail16` | `0x5A` | choice 5 | kChoice | message `0xFFFF`; answer not 0: the tail off (`Field_ScriptFlags &= 0xEFF9`, kind and state 0). Answer 0: the leader's `+0x89` of 2 opens message `0x3C`; else, no tail armed, `ScriptFlags_Set40` and tail kind `0x10` at state 0 |
| `0x405A10` | `Area40_TailPuzzle` | `0xE8` | tail kind `0x10` | kTail | by the s8 state: 0 `ScriptFlags_Clear40`, story flag `0x27` clear: `Party_DropIn(4)`, `Field_ScriptFlags` bit 12, state 1 (set: state `0xA`); 1: counter 0 at `0x63`: bits 1, 2, counter 0 = 0, state 2; 2: with `Field_Request` 0 the held buttons to the scratch word and their top nibble remapped through `Area40_InputMap`, then `Area40_DrawGrid`. Every state then: unless area `0x28`, state not `0xA` and `Cond_ByteFD` 6, the tail off |
| `0x405B00` | `Area40_ArriveHook` | `0xE2` | `Area_ArriveHook`'s case for area 40 | kHook | with `Cond_ByteFD` 6, the leader's `+0x2B` 0, flag `0x27` clear and the puzzle's tail armed: `Area40_ClearGrid`; a cell (x, z high words) in the grid: lit (`Area40_TileLit`) switched off (`MoveCmd_TestFC`) or unlit on (`MoveCmd_TestFB`), `Area40_MarkBehind(x, z)`, sound `0x205`; outside: unless `Area40_PuzzleSolved`, all 16 grid cells off. al 0 always |
| `0x405BF0` | `Area40_TileLit` | `0x138` | called by the hook and `PuzzleSolved` | kCallee | is the map cell (x, z) lit? The view-ring cell `MoveCmd_TestFB` finds, its run of area-block records; the first of kind `0x23..0x25` whose patch entry is kind `0x8000` answers the entry's bit 0; else 0. Reads only |
| `0x405D30` | `Area40_PuzzleSolved` | `0xF1` | called by the hook | kCallee | each grid cell's `TileLit` against `Area40_Pattern`; any differing al 0. All equal: flag `0x27`, the four cells beside the grid opened (row `0x1C` `0xA1`, row `0x1D` `0xC0`), tail state `0xA`, `MoveCmd_TestFB(0x92, 0x1D)`, sound `0x103`, al 1 |
| `0x405E30` | `Area40_ClearGrid` | `0x42` | called by the hook | kCallee | the grid's 16 map bytes 0, row by row |
| `0x405E80` | `Area40_MarkBehind` | `0x4E` | called by the hook | kCallee | the cell one step behind the leader (direction `0x802D48 & 7` through the cell steps `0x66971C`), in the grid: map byte `0x50` |
| `0x405ED0` | `Area40_DrawGrid` | `0x1E4` | called by the tail in state 2 | kCallee | each grid cell with map byte `0x50` and a map item: a semi-transparent flat quad at `Gfx_PacketNext` (`Gpu_SetPolyF4`, `Gpu_SetSemiTrans(1)`), its corners the item's `DrawItems` floats (by `fld` / `fstp`), its colour a grey pulsing with the frame, `MapView_LinkPrimAt(x << 16, z << 16, 0, 0x38)` |
| `0x4060C0` | `Area40_Init` | `0x13B` | `+0x40` init (PSX `0x801F3A00`) | kInit | `Cond_ByteFD` not 1: the lever flags cleared, the gate closed. `Cond_ByteFD` 6 and flag `0x27` clear: the cells beside the grid closed. Entered from zone 6 (`0x905E68`): only with `Cond_ByteFD` 2, bit 12, flag `0x27` and counter 0 cleared; from zone 4: the pattern's cells lit (`MoveCmd_TestFB`) |

Choices 6 and 7 are `0x425C30` (AR4A's, seven areas) and `0x403050`
(`Area22_ArmTailOnYes`, ours since AR0B). The descriptor's handler array
is the tail of its choice table (as [`area-rows.md`](area-rows.md) section
2 found generally).

**The puzzle, as the code reads.** The grid is the 16 map cells x
`0x94..0x97` by z `0x20..0x23`. Each cell's "lit" state is not a byte of
area 40's: it is the patch entry of the cell's first `0x23..0x25` record in
the area block, which `MoveCmd_TestFB` switches on and `MoveCmd_TestFC` off
([`field-misc.md`](field-misc.md)). Arriving on a grid cell toggles it and
marks the cell behind the leader with map byte `0x50`; stepping off the
grid compares all 16 with `Area40_Pattern` and, on a match, sets story flag
`0x27`, opens four cells beside the grid and ends the tail (state `0xA`);
on no match every cell is switched off. The tail draws a pulsing quad on
every `0x50` cell each frame. The four levers (choices 1..4, flags 8..`0xB`
of the row `0x903FB0`) open the gate x `0x41..0x44`, z 3..9 when their
nibble is 5. What the puzzle is in the story is not read here.

### Area 41 (descriptor `0x5F5F10`; PSX `0x801F5634`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x406200` | `Area41_ChoiceDropIn12` | `0x36` | choice 0 | kChoice | message `0xFFFF`; answer 0: `Party_DropIn(1)`, counter 1 = `0xA`; else `Party_DropIn(2)`, `0x14` |
| `0x406240` | `Area41_ChoiceDropIn34` | `0x3E` | choice 1 | kChoice | answer 0: `Party_DropIn(3)`, counter 1 = `0x1E`; else `Party_DropIn(4)`, the byte the pointer `0x903804` names (read after the call) 0, counter 1 = `0x28` |
| `0x406280` | `Area41_ChoiceGiveItem` | `0x58` | choice 3 | kChoice | message `Area41_GiveMessages[s8 answer]`; answer 0 with flag 6 of the chapter's row clear: sound `0x106`, `Inventory_Add(0, 0x28, 1)`, the flag set (the row pointer read again) |
| `0x4062E0` | `Area41_ChoiceTrade` | `0xBC` | choice 4 | kChoice | answer 0 with items `0x49`, `0x3F`, `0x27`, `0x1D` of category 0 all held: each taken (`0x591B60(0, id, 1, 0)`), message `0x62`; any missing: `0x63` and the mark `0x9398CF` = 6. Answer not 0: `0x64` and the mark |
| `0x4063A0` | `Area41_ChoiceConfirm64` | `0x24` | choices 7, 8 | kChoice | answer not 0: message `0x64` and the mark; else `0xFFFF` |
| `0x4063D0` | `Area41_Run` | `0x12` | choice 12 = handler 2 (PSX `0x801F3E3C`) | kHandler | `Area41_States` by `Sprite_Current[4]` |
| `0x4063F0` | `Area41_TintStart` | `0x34` | `Area41_States[0]` | kState | `+0xC` = `0x4000`, `Area41_TintUp`, then `+0xA` = 7 and state 1 (`Sprite_Current` read again for each); the leader's word `+0x12E` less 2 |
| `0x406430` | `Area41_TintRise` | `0x3A` | `Area41_States[1]` | kState | `Area41_TintUp`; `+0xA` down, at 0 `+0xA` = 8 and state 2; the leader's word less 2 |
| `0x406470` | `Area41_Slide` | `0x46` | `Area41_States[2]` | kState | `+0x34` by `+0xC`; `+0xA` down, at 0 `+0xA` = 8 and state 3; the leader's word less 2 |
| `0x4064C0` | `Area41_TintFall` | `0x99` | `Area41_States[3]` | kState | `+0x34` by `+0xC`; the tint record's three bytes down 2; `+0xA` down, at 0 state 0 (and no more); else the leader's word less 2 |
| `0x406560` | `Area41_TintUp` | `0x64` | called by `TintStart`, `TintRise` | kCallee | `+0x34` by `+0xC`; `MoveScript_TintRecords[Field_State +0x149]` bytes `+2..+4` up 2 |
| `0x4065D0` | `Area41_ClearCells` | `0x1A` | choice 13 = handler 3 (PSX `0x801F41F8`) | kHandler | cells (`0x40`, `0x28`), (`0x40`, `0x27`) 0 |
| `0x4065F0` | `Area41_PlaceKind2` | `0x9` | choice 14 = handler 4 (PSX `0x801F4230`) | kHandler | `Kind2_Place(8)` |
| `0x406600` | `Area41_Trigger29` | `0x16` | object trigger 29 | kCallee | `ScriptFlags_Set40`, tail kind 4 with sub-kind `0xA`; al 0 |
| `0x406620` | `Area41_TriggerFlag` | `0x2D` | object triggers 59, 60 | kCallee | story flag `byte 0x5F5F2D + object +0x86` (`Area41_TriggerFlags` for 59, 60), sound `0x201`; al 0 |

Choices 2 (`0x425C30`, AR4A), 5 and 6 (`0x420850` / `0x420870`, AR3F),
9 (`0x403050`, `Area22_ArmTailOnYes`), 10 and 11 (`0x413C80` /
`0x413CA0`, AR2D; also the descriptor's handlers 0 and 1) are other
groups'. `0x4063A0` is choices 7 and 8: one function, taken once.

## 2. Ours

`src/game/area_w1a.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee; `AH_AT` for SX's three raw
addresses). Shapes that repeat are one helper with the function's
constants: area 38's two spawns (`SpawnForMember`), the lever choices
(`Lever`), the gate (`Gate`, shared by `Area40_SetGate` and `Area40_Init`),
the tail's way out (`TailOff`), area 41's tint (`TintBy`) and countdown
(`CountDown`). Kept as the originals: every re-read after a call
(`Sprite_Current`, the list byte in area 38, the tail state, `Cond_ByteFD`
and the entry zone in `Area40_Init`, `Field_StatusBits` after `Flags_Set`,
the row pointer in `Area41_ChoiceGiveItem`, the pointer `0x903804`,
`Gfx_PacketNext` per quad); the s8 reads of the tail state and the answer
where the original sign-extends and the byte reads where it does not; the
16-bit arithmetic on cell words (`Area40_ArriveHook`, `Area40_MarkBehind`,
`Area40_TileLit`); the corners copied through `fld` / `fstp` (a signalling
NaN comes out quiet, inline asm); `Area38_Spawn*` leaving `Sprite_Current`
on the party record.

**What the originals pass that ours does not reproduce bit for bit**, each
read only in 16 or 8 bits by the callee, so the same to it: the cell words
the hook, `PuzzleSolved`, `SetGate`, `ClearGrid` and `Init` push with stale
high halves (`AreaMap_SetByte` reads `s16` x and z and a byte; `MoveCmd_*`
and `TileLit` `short` x and z); `Area40_MarkBehind`'s z dword to
`AreaMap_SetByte`, whose high half is two bytes of its caller's frame; the
fourth word 0 `Area41_ChoiceGiveItem` pushes to `Inventory_Add`, which
takes three.

## 3. The fuzz

`BOF3X_SHADOW=area_w1a` (`src/game/area_w1a_fuzz.cpp`): four `Run`s under
the one shadow name, one per area with its `Group::area` (38..41), 6,000
rounds per function, the real descriptors and tables in place.

- **Callees the group lists:** `Effect_FindFree` as `kByte` `0xFF..0x03`
  (a slot inside the group's four effect records, or none);
  `ScriptFlags_Set40` / `Clear40`, `EventOp_0x`, `MoveCmd_TestFB` / `FC`
  (x, z logged in 16 bits), `Gpu_SetPolyF4`, `Kind2_Place` (ours, not in the
  standard set); `AreaMap_SetByte` again with the masks it reads (16, 16, 8
  bits); `AreaMap_ByteAt` (`0x50` three times in four), `MapView_ItemAt` (no
  item a third of the time, else items 1..3), `Inventory_Count` (0 a
  quarter of the time), `Gpu_SetSemiTrans` and `MapView_LinkPrimAt` (the
  packet's bit and cursor, as AR0B's); the group's own `Area40_SetGate`,
  `DrawGrid`, `ClearGrid` (`kPhase`), `TileLit` (the pattern's byte for a
  grid cell 31 times in 32, so the all-equal path runs about half the
  time), `MarkBehind`, `PuzzleSolved`, `Area41_TintUp` (`kPhase`); SX's
  `0x57CD90` (`kByte` `0xFF..0x1D`), `0x591900`, `0x591B60` by raw address.
- **Louder stand-ins** (an `effect`, from `Noise`, half the time or so):
  `Effect_FindFree` moves `Sprite_Current`, area 38's list byte and
  `Field_ActiveMember`; `Party_DropIn` the pointer `0x903804`, the script
  flags' bit 12, the tail state, `Cond_ByteFD`; `Flags_Test` `Cond_ByteFD`,
  the entry zone, the row pointer, the tail state; `Flags_Set`
  `Field_StatusBits`; `Flags_Clear` the entry zone; `AreaMap_SetByte`
  (one call in eight) `Cond_ByteFD`; `Inventory_Add` the row pointer;
  `0x57CD90` `MoveScript_Object`; `Area40_DrawGrid` the tail state, the
  area, `Cond_ByteFD` - each a cell its caller reads again after the call.
- **Data tables** (swapped for recorders): `Area39_States` (2),
  `Area41_States` (4).
- **Regions beyond the field frame** (35 regions, 23,795 bytes):
  `Effect_Objects` records 0..3, `Field_ActiveMember`, `MoveScript_Object`,
  the row pointer `0x929ED0`, the pointer `0x903804`, `0x9398CF`, the 32
  key-item bytes, `Input_Held` (as a dword), `Gfx_PacketNext` and the
  fuzz's own 512-byte packet buffer, `MapView_Origin`, `MapView_Column` ..
  `MapView_Row`, all of `MapView_Cells`, `DrawItems`' first eight halves,
  and 232 of `MoveScript_TintRecords` (with `Effect_Objects` after them
  every byte index lands in a region).
- **Every round:** `Field_ActiveMember` at one of the four party objects, a
  field object, a party record or the running object; `MoveScript_Object`
  and `0x903804` at a field object (or a party record); the row pointer at a
  `Cond_Flags` row; `Gfx_PacketNext` into the buffer.
- **Seeds:** the choice answer at 0 (twice as often), 1, 2, 3, `0x7F`,
  `0x80`, `0xFF`; area 38's list byte 0 two in three, else 1, 7, `0x80`,
  `0xFF`; the dispatchers' index inside its table (section 6);
  `Area39_DriftStep`'s `+0xA` at 0, 1, `0x10`, `0x11`, `0x40`; the key items
  with no `0xE` a third of the time, else one or two planted; the first
  party-list byte 4, 3, 5, `0x84`, 0; the leader's `+0x89` 2, 1, 3, `0x82`;
  the lever nibble 5 (half) with any high nibble, 4, 6, 7, `0xD`; the tail
  kind 0, `0x10`, 1; the tail state 0, 1, 2, 3, -1, `0xA`; counter 0
  `0x63` and either side, `0xE3`; `Field_Request` 0, 1, 2; the area `0x29`,
  `0x128`, `0x27` half the rounds; `Cond_ByteFD` 6, 5, 7, `0x86` (tail), 1,
  2, 6, 5, `0x86` (init); the entry zone 4, 6, 5, `0x84`; the hook half the
  rounds with every gate passing but at most one drawn to fail, its (x, z)
  on the grid, one edge just off (`0x93`, `0x98`, `0x194`, `0xFF94`; `0x1F`,
  `0x24`, `0x120`, `0xFF20`) or anywhere near; the leader's direction byte
  any value; `Gfx_BufferIndex` 0 / 1 half the rounds; area 41's `+0xA` 1,
  2, 0, 8 and the tint index 0..231; the triggers' object `+0x86` 59 or 60
  two in three.
- **`Area40_TileLit`'s own fuzz** plants what it walks: `MapView_Row` -1..
  `0x36` and `MapView_Column` -1..`0x1A` (the one wrap the code assumes),
  `AreaMap_CellBase` below `0x40`, `AreaMap_PatchBase` `0x600..0x63F`, four
  runs of 0..6 records at dword indexes `0x140`, `0x180`, `0x1C0`, `0x200`
  (kinds `0x23..0x25` or `0x22`, `0x26`, `0xA3`, `0x03`; steps of 1 or 2
  landing exactly on the run's last dword), their patch entries of kind
  `0x8000` half the time, and every ring cell 0 or one of the runs; the
  arguments a ring row and diagonal of one parity, inside two times in
  three, else at or past the edges (-1, `0x38`, `0x39`, `0x100`).
- **The group's disturbance** (from the hash it is given): the tail state,
  `Cond_ByteFD`, the entry zone, the area, `Field_StatusBits`, the row
  pointer, `0x903804`, `Field_ActiveMember`, a party-list byte,
  `Field_ScriptFlags`, `MoveScript_Object`, `Gfx_BufferIndex`.

**Result (in this worktree):** RESULT_LINE

`BOF3X_SHADOW='*'`: STAR_LINE

## 4. Controls

CONTROLS_TEXT

## 5. The tables named

In `symbols.toml` as `[[data]]` (15): `Area38_EffectBytes` `0x5F14E8`
(signed bytes by a party-list byte; 8 named), `Area38_Handlers`
`0x5F21F4` (4), `Area39_Handlers` `0x5F3E20` (15), `Area39_States`
`0x5F3EA4` (2), `Area39_DriftSteps` `0x5F3EAC` (16 signed bytes),
`Area40_Choices` `0x5F4BB0` (10), `Area40_Handlers` `0x5F4BD0` (2, the
choice table's tail), `Area40_Pattern` `0x5F4C1C` (16 bytes, row by z),
`Area40_PlaceOp` `0x5F4C2C` (`EventOp_0x`'s op), `Area40_InputMap`
`0x5F4C40` (16 bytes), `Area41_Choices` `0x5F5ED4` (15), `Area41_Handlers`
`0x5F5EFC` (5, the choice table's tail), `Area41_GiveMessages` `0x5F5F54`
(2 words), `Area41_States` `0x5F5F58` (4), `Area41_TriggerFlags`
`0x5F5F68` (2 bytes). Areas 38 and 39's tables sit before or after their
own descriptors as the tool's data-block rule says; 40 and 41's run on past
their descriptors (the PC's layout, [`area-rows.md`](area-rows.md)
section 2).

## 6. Latent defects and the aborts

Described, not fixed:

- **Unchecked state indexes.** `Area39_Run` (2 entries) and `Area41_Run`
  (4) jump through `[Sprite_Current[4] * 4 + table]`; past the table are
  `Area39_DriftSteps`' bytes and `Area41_TriggerFlags`' bytes read as code
  pointers. **Ours aborts with a message** past each table (the owner's
  rule, round9 doc section 6, no DIVERGENCE entry); the fuzz keeps the
  index inside. No route measured reaches either.
- **`Area40_TileLit`'s walk** has `MoveCmd_TestFB`'s shape and its hazard
  ([`field-misc.md`](field-misc.md)): a record step of 0, or one that steps
  past the run's last dword, walks on for ever (reads only: a hang, not a
  fault). Ours walks on as the original. It also assumes `MapView_Row` and
  `MapView_Column` keep the ring cell inside `MapView_Cells` after one wrap.
- **`Area40_DrawGrid`** indexes `DrawItems` (`0x24000` bytes) by
  `MapView_ItemAt`'s answer (up to `0xFFF`) unchecked: an item of `0x800`
  or more reads past it. Kept.
- **`Area41_ChoiceGiveItem`** takes its message from
  `Area41_GiveMessages[s8 answer]`, two words: an answer of 2 or more reads
  `Area41_States`' pointers as message ids, a negative one the descriptor.
  Kept (a read of the image; both sides read the same).
- **`Area38_Spawn*`** leave `Sprite_Current` on party record 1 / 2: the
  movement-script op that ran the handler then reads the party record's
  byte `+8` (op `DE`) where it would read its own object's. Kept; whether
  the scripts rely on it is not read.
- **`Area41_TintUp` / `TintFall`** index `MoveScript_TintRecords` by
  `Field_State +0x149` unchecked (as ops `C1` / `C2` do).

## 7. What the tool listed, against the reading

- **Dropped:** none. **Added:** none. Every gap between the band's 48
  functions is `nop` padding after a `ret` or the `jmp` of a dispatcher.
- **The two known gaps of the tool** (round10 doc section 7), looked for:
  every start in the band follows a `ret` or padding (the band opens at
  `0x4053B0` after area 37's `ret` at `0x4053A8`), so none is missed by the
  start rule; the band's two tail-kind stores (`0x4059D9` kind `0x10`,
  `0x406605` kind 4) are immediates, no kind is armed through a register,
  and `Field_ModeTailKinds` has one entry in the band (`0x10`,
  `Area40_TailPuzzle`).
- **Every row agrees** with the reading: start, extent, calls, and the
  shape its root gives; the tool's "reached by area 40 / 41" rows are the
  direct callees named in section 1, its "data 0x5F3EA4 / 0x5F5F58" rows
  the two state tables' entries.
- **Gap functions** (no descriptor names them): `0x406600` is
  `Field_ObjectTriggers` id 29 and `0x406620` ids 59 and 60 (the table
  `0x662E20`, by an object's `+0x86`). A byte scan of `.text` for `mov byte
  [reg + 0x86], imm8` finds no store of 29, 59 or 60, so the id comes from
  area data. Both sit in area 41's block and are attributed to it;
  `0x406620`'s flag table follows area 41's descriptor.

## 8. What reaches it

- **No recorded route**: the tool's live column (`hidden_reached_*.json`,
  the call traces) is empty for all 48, so the coordinator's route A/Bs
  after the merge check only that nothing else moved.
- **In play**: the handlers from the areas' movement scripts (ops `03` /
  `DE`), the choices when the area's message box asks, `Area40_Init` on
  every entry to area 40, its arrive hook on every arrival there, the tail
  once `Area40_ChoiceTail16` arms it, and the object triggers when an
  object with id 29, 59 or 60 is triggered.

## 9. Calls across groups

- **Raw addresses (group SX's this wave):** `0x57CD90` (a free
  `Sprite_Objects` index) by `Area40_PlaceObject`; `0x591900` (a key item
  added) by `Area39_SwapKeyItemE`; `0x591B60` (the inventory take) by
  `Area41_ChoiceTrade`. In `area_w1a_callees.h`.
- **Through the tables, not called:** `0x413C80`, `0x413CA0` (AR2D),
  `0x40B2D0` (AR1D), `0x425C30` (AR4A), `0x420850`, `0x420870` (AR3F),
  `0x437CC0` (the bare `ret`), `0x403050` (`Area22_ArmTailOnYes`, ours).
- **By name, ours:** `AreaMap_SetByte`, `AreaMap_ByteAt`, `Effect_FindFree`,
  `Music_Play`, `Music_FadeOutStop`, `EventOp_0x`, `Flags_Set` / `Test` /
  `Clear`, `Sound_PlayEffect`, `ScriptFlags_Set40` / `Clear40`,
  `Party_DropIn`, `MoveCmd_TestFB` / `FC`, `MapView_ItemAt`,
  `Gpu_SetPolyF4`, `Gpu_SetSemiTrans`, `MapView_LinkPrimAt`,
  `Inventory_Add`, `Inventory_Count`, `Kind2_Place`. No harness edit; no
  `AH_THEIRS` moved.
- `analysis/calltrace/entries_logic.txt`: 43 lines appended under a
  `# group AR1A` comment (five of the 48 were there already with the same
  extent: `0x405B00`, `0x405BF0`, `0x405D30`, `0x405E30`, `0x405E80`); the
  hosts `00405910 1E8`, `00405ED0 689` and `00406560 368` run over others
  (the consolidation keeps the smaller).
