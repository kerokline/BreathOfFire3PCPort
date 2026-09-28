# World 2, areas 108 and 110..113: the band `0x4168E0..0x418BE0`

**Status:** IN PROGRESS (2026-09-28) - 53 functions ours
(`src/game/area_w2f.cpp`, shadow name `area_w2f`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 424,000 rounds (in this worktree); 348 controls planted, 346 refused by a count, 2 by a hang with near variants refused by a count
(section 4). Fuzz only: no recorded route reaches the band (section 8). No
divergence; the two state-table dispatchers abort past their tables, area
111's handler 8 aborts where its dword index would read outside the image
and its handler 5 aborts where its last write would land outside the grid,
where the original would jump into data, fault or write past its table
(section 6).

Group AR2F of round ten's fifth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 15; the
tool's `--groups` prints it as its `AR2B` row at `2c9c92f`). The band is the
tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)): 53 starts,
none ours before, **53 taken**; no start dropped, none added (section 7).
Area 109 has no code (its descriptor `0x61E738` has no `+0x34`, `+0x3C` or
`+0x40`). Areas 108 and 113 also name bodies of other areas that are ours
already (section 1); they are called or read by name, not taken again.

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, hooks, tails, triggers and
state tables have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the message box's answer byte
`0x7DEE67` in, the message word `0x7DEE48` read after), `kHandler` a `+0x3C`
handler (movement-script ops `03` / `DE`), `kInit` the `+0x40` init,
`kTail` a `Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a
step, arrive or cell hook `(x, z)` answering in `al`, `kState` a state
handler reached through a table in the area's `.data`, `kCallee` a function
called directly (by the group's own code, by an engine table, or as an
object trigger `(object, 0x904030)` answering in `al`). A function that is
both a choice and a handler is fuzzed as a choice when it writes the
message word, else as a handler.

### Area 108 (descriptor `0x61E598`; PSX `0x801F6328`)

`+0x34` has 19 entries and `+0x3C` 16: choices 3..18 are handlers 0..15.
Choice 2 is area 22's `Area22_ArmTailOnYes`; handler 12 (choice 15) is area
67's `Area67_SetBit24`, handlers 13 and 14 (choices 16, 17) area 57's
`Area57_StopMusic` / `Area57_ResumeSound`. **The descriptor's `+0x40` is the
shared `ret` `0x437CC0`** where the PSX lists an init (`0x801F56E0`) -
Capcom's port, read, not measured further.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4168E0` | `Area108_ChoiceArmTail5` | `0x42` | choice 0 | kChoice | the message word `0xFFFF`; by the s8 answer: 0 - sub-kind 3, story flag `0x1E`; 1 - sub-kind 2, flag `0x1F`; then tail kind 5. Any other answer: nothing more |
| `0x416930` | `Area108_ChoiceArmTailA` | `0x28` | choice 1 | kChoice | `0xFFFF`; answer 0: tail kind `0xA`, state 0, sub-kind 4 |
| `0x416960` | `Area108_ClearCounter1Bits0` | `0x8` | handler 0 (PSX `0x801F43A4`) | kHandler | counter 1 (`0x903849`) `&= 0xFC` |
| `0x416970` | `Area108_ClearCounter1Bits2` | `0x8` | handler 1 (PSX `0x801F43C0`) | kHandler | `&= 0xF3` |
| `0x416980` | `Area108_ClearCounter1Bits4` | `0x8` | handler 2 (PSX `0x801F43DC`) | kHandler | `&= 0xCF` |
| `0x416990` | `Area108_ClearCounter1Bits6` | `0x8` | handler 3 (PSX `0x801F43F8`) | kHandler | `&= 0x3F` |
| `0x4169A0` | `Area108_PlaceScene` | `0x3C0` | handler 4 (PSX `0x801F4414`) | kHandler | section 1.1 |
| `0x416D60` | `Area108_PlaceAtCellA` | `0x71` | handler 5 (PSX `0x801F4918`) | kHandler | the running object's words `+0x36` / `+0x34` / `+0x3A` / `+0x38` from `Area108_PlaceCellsA[extra object 1's +0x83 - 1]` (an x byte, `0x8000` when a flag byte is set, a z byte, the same) |
| `0x416DE0` | `Area108_PlaceAtCellB` | `0x71` | handler 6 (PSX `0x801F4998`) | kHandler | the same over `Area108_PlaceCellsB` |
| `0x416E60` | `Area108_ScriptByFlags1E` | `0x5E` | handler 7 (PSX `0x801F4A18`) | kHandler | story flag `0x1E`: the active member's script `+0x83` = `0x12`; else flag `0x1F`: `0x11`; its word `+0x8A` = `0xFFFE` either way |
| `0x416EC0` | `Area108_FlagIfEffectState5` | `0x46` | handler 8 (PSX `0x801F4A98`) | kHandler | the active member's `+0x80` bit 0 cleared; `MoveScript_EffectState[the leader's +0x89]` at 5: story flag `0x30`, `MoveCmd_TestFB(1, 0xB)`, the running object's `+0` = 0 |
| `0x416F10` | `Area108_FadeRun` | `0x12` | handler 9 (PSX `0x801F4B14`) | kHandler | `jmp [Area108_FadeStates + Sprite_Current[4] * 4]` |
| `0x416F30` | `Area108_FadeBegin` | `0x7A` | `Area108_FadeStates[0]` | kState | `Sprite_SetTint(the running object, 0, 0, 0, 1)` to the active member's `+0x9F`; `+0` bit `0x20` set and `0x40` cleared, `+0x5C` = 1, `+0x5F` / `+0x5E` / `+0x5D` = `0x80`, state `+4` = 1; the member's script position `- 2` |
| `0x416FB0` | `Area108_FadeStep` | `0xA8` | `Area108_FadeStates[1]` | kState | each of `+0x5D`, `+0x5E`, `+0x5F` below `0xD0` (signed bytes) `+ 1`; all three at `0xD0`: `Tint_Release(the member's +0x9F)`, `+0` bit `0x20` cleared, `+0x5C..+0x5F` and the state 0; else the member's position `- 2` |
| `0x417060` | `Area108_FindEffectSlot` | `0x26` | handler 10 (PSX `0x801F4D60`) | kHandler | `Effect_FindFree` to the running object's `+0xB`; none: the script position `- 2` |
| `0x417090` | `Area108_SpawnEffect51` | `0x6B` | handler 11 (PSX `0x801F4DC8`) | kHandler | the same; a slot: an effect of kind `0x51` at (`0x158000`, `0x418000`), `+1` = `0xA`, `+0x3C` = its ground `<< 16` |
| `0x417100` | `Area108_CellsIfRequest5` | `0x20` | handler 15 (PSX `0x801F4F0C`) | kHandler | `Field_Request` 5: `Area108_SetCells(0)`; else the script position `- 2` |
| `0x417120` | `Area108_CellHook` | `0xE1` | `Area_CellHooks` record for area `0x6C` (`0x662F34`) | kHook | the first of `Area108_HookCells` whose x and z bytes are the cell's and whose direction (`& 0xF`) is the leader's **whole** pose byte; none, or story flag `0x1C` set: al 0. Else the entry's flag toggled (`Flags_Toggle`), `Sound_PlayEffect(0x206)`, and with a free effect slot flag `0x1C` set and an effect of kind `0x18` at the cell (`+0xB` = `0x1B`, `+1` = 0); al 1 |
| `0x417210` | `Area108_TailPlace` | `0x2F4` | tail kind 12 | kTail | section 1.1 |
| `0x417510` | `Area108_SetCells` | `0x64` | called by handlers 4 and 15 | kCallee | `(on)`: each of `Area108_CellWords`' four (x, z) words - on: `AreaMap_SetHeight(x, z, 0xE)`, `AreaMap_SetByte(x, z, 0x11)`; off: both 0 |

### 1.1 Area 108's scene

**`Area108_PlaceScene`** (handler 4): the active member's `+0x80` bit 0 is
cleared first; unless the leader's member id (`+0x89`) is 2, nothing more.
`Area108_Places` holds five (x, z, direction) byte triples; the first whose
direction is the leader's (`+8 & 7`) and whose x and z are extra object 1's
high words (`+0x36`, `+0x3A`) picks the scene through the jump table
`0x416D4C` (5 entries, inside the extent); none: nothing more. Each scene
calls `Party_DropIn` with a byte mask of the other members (records 1..count
- 1, bit i for record i + 1, the originals' 8-bit `shl`) standing in a strip
beside the place - scene 0: x above `0x27`, z `0x53..0x54`; 1: x below
`0x39`, z `0x53..0x54`; 2: z above `0x3A`, x `0x33..0x34`; 3: x above
`0x21`, z `0x41..0x42`; 4: z below `0x41`, x `0x1C..0x1D` (signed 16-bit
bounds, unsigned 16-bit strips) - then sets the member's script (`+0x83`,
`+0x8A` = 0): scene 0 by story flags `0x18` / `0x19` / `0x1A` unset to 1 / 2
(with `Area108_SetCells(1)`) / 3, else 4; scene 1: 5; scene 2: 6 with
`Area108_SetCells(0)`; scene 3: 7; scene 4: 8, or 9 with flag `0x1B`. Then
`MoveScript_PartyRecords` record 1's bit 0, `ScriptFlags_Set40`, tail kind
`0xC`, state 0, timer `0xF`. The mask's argument is pushed as a dword whose
upper three bytes are the original's stack; `Party_DropIn` reads the byte.

**`Area108_TailPlace`** (tail kind 12, the jump table `0x4174DC`, 10
entries, by the s8 state; unsigned above 9 nothing):

- 0: the timer down to 0: `Field_Kind2X` / `Z` = extra object 1's x / z,
  `MoveScript_F3Divisor` = `Field_MoveSpeeds[3] << 3`, the next state.
- 1: `Field_Kind2Hold` 0: `Kind2_Place(extra object 1's +0x83 - 1)`,
  `Sprite_Kind2 +0x84` = 4, counter 3 = 1, the state (read again after the
  call) `+ 1`.
- 2: counter 3 at 2: counter 3 = 0; extra object 1's `+0x83` at 9: state 4;
  else the divisor from `Field_MoveSpeeds[4]`, `Field_Kind2X` / `Z` the
  leader's, the next state.
- 3: `Kind2Hold` 0: `MapView_SetElevation(AreaMap_Elevation(Kind2X,
  Kind2Z))`, state and kind 0, then `ScriptFlags_Clear40`.
- 4: counter 3 at 4: an effect of kind `0x35` at (`0x218000`, `0x2F8000`)
  with its ground (sign-extended, **not** shifted as handler 11's is) if a
  slot is free; `Area108_Model`'s flags byte `& 0xF1 | 0x41`; the timer
  `0x1E`; state 5.
- 5: the timer down to 0: story flag `0x1D`, `MoveCmd_TestFB(0x20, 0x2F)`,
  state 9 (the `ebx` the phase entered with), extra object 0's `+0` = 0, the
  timer `0x1E`. Else each of `Area108_Model`'s records (its s8 count, read
  again each pass; a count of 0 or below none) has its four corners' x, y
  and z words moved by its velocity words `>> 8` (arithmetic), and extra
  object 0's `+0x5D`, `+0x5F`, `+0x5E` less 4.
- 6..8: nothing. 9: the timer down to 0: `MoveScript_Var7` = 3, the state,
  the kind and the run's step `0x8034E5` 0.

`Area108_Model` (`0x61E508`, the descriptor's `+0x08`): an s8 count (`0x21`
in the image), two bytes, a flags byte, and a pointer (`0x61D2E4` in the
image) to `count` records of `0x28` bytes - four corners of three s16 each
at `+0..+0x17`, a velocity of three s16 at `+0x18..+0x1D`. Who draws it is
not read here.

### Area 110 (descriptor `0x61E808`; PSX `0x801F2DD4`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x417580` | `Area110_PlaceRandomObject` | `0xCB` | init (PSX `0x801F2D2C`, area 72 / 73's PSX entry too) | kInit | a `jmp` over eleven `nop`s to `0x417590`; **area 72's code** (`Area72_PlaceRandomObject`, [`area_w1f.md`](area_w1f.md)), instruction for instruction, over `Area110_Cells` / `Area110_Weights`: `Rand() & 0x3F` walked down eight weights, field objects 0..7 but the chosen one `+0` = 0, the chosen one at `Area110_Cells[Rand() & 7]` with its ground, `Field_EdgeBits` = the leader's zone counter `- 5` |

Compared byte for byte against `0x40D4B0`: only the two tables' addresses
and the `.text` targets differ. The shipped weights sum to `0x40`, so the
"none" path is dead with the image's table, as area 72's.

### Area 111 (descriptor `0x61FB38`; PSX `0x801F5B84`)

`+0x34` has 11 entries, `+0x3C` 9: choices 2..10 are handlers 0..8.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x417650` | `Area111_ChoiceMessage2` | `0x17` | choice 0 | kChoice | the message word 3 for an answer, else 2 |
| `0x417670` | `Area111_ChoiceStartVar7` | `0x26` | choice 1 | kChoice | `0xFFFF`; answer 0: `ScriptFlags_Set40`, `MoveScript_Var7` = 5, the run's step 0 |
| `0x4176A0` | `Area111_CopyExtra6C` | `0xB` | handler 0 (PSX `0x801F3448`) | kHandler | extra object 0's dword `+0x6C` = extra object 1's |
| `0x4176B0` | `Area111_SlideMark` | `0xD7` | handler 1 (PSX `0x801F3460`) | kHandler | section 1.2 |
| `0x417790` | `Area111_AttachAtLeaderCell` | `0xF8` | handler 2 (PSX `0x801F35F4`) | kHandler | section 1.2 |
| `0x417890` | `Area111_Counter3Bit` | `0x13` | handler 3 (PSX `0x801F3768`) | kHandler | counter 3 = the byte `0x10 <<` the running object's dword `+0x18` (an 8-bit `shl`, the count masked to 5 bits) |
| `0x4178B0` | `Area111_WaitMemberBit` | `0x3D` | handler 4 (PSX `0x801F3790`) | kHandler | unless counter 3 is `0x10 <<` the active member's index among the extra objects (`(member - 0x802000) / 0xA4`, signed; a dword `shl`), the script position `- 2` |
| `0x4178F0` | `Area111_PlaceMemberInGrid` | `0xF9` | handler 5 (PSX `0x801F3808`) | kHandler | section 1.2 |
| `0x4179F0` | `Area111_AttachAhead` | `0x13E` | handler 6 (PSX `0x801F39E0`) | kHandler | section 1.2 |
| `0x417B30` | `Area111_SlideAhead` | `0xD9` | handler 7 (PSX `0x801F3BC0`) | kHandler | section 1.2 |
| `0x417C10` | `Area111_StepToExtra` | `0xD3` | handler 8 (PSX `0x801F3D3C`) | kHandler | section 1.2 |
| `0x417CF0` | `Area111_TailGate` | `0x1E0` | tail kind 17 | kTail | section 1.2 |
| `0x417ED0` | `Area111_ArriveHook` | `0x25F` | `Area_ArriveHook`'s case for area 111 (`0x56E53B`) | kHook | section 1.2 |
| `0x418130` | `Area111_ArmTailAtLeaderCell` | `0xAC` | called by the arrive hook | kCallee | `(state)`: the grid's nibble at the leader's cell `- 1` to the sub-kind, tail kind `0x11`, the state `state`, extra object (that byte)'s `+8` = `state`, `ScriptFlags_Set40` |
| `0x4181E0` | `Area111_MarkCells` | `0x52` | called by handlers 1, 5 and the init | kCallee | `(x, z, value)`: the grid cell's four map bytes (`2x + 0xB..0xC`, `2z + 0xB1..0xB2`) set to `value` |
| `0x418240` | `Area111_Init` | `0x10C` | init (PSX `0x801F46A8`) | kInit | with `Cond_ByteFD` 0 story flag `0x28` cleared; `Cond_ByteFD` (read again) 1: every grid cell marked with its byte of `Area111_CellBytes` (rows of z, columns of x), `Area111_Grid` copied from `Area111_GridStart`, the gate's map bytes (`0x11..0x12`, `0xAD`) = `0x50` and (`..`, `0xAE`) = 0 without flag `0x28`, `0xA1` / `0xC0` with it; `Cond_ByteFD` 5: the map bytes (`0xA..0xB`, 8) = `0x50` |

### 1.2 Area 111's grid

`Area111_Grid` (`0x675C00`, 28 bytes at the very end of the image's
initialized `.data`) is 7 x 7 nibbles, four bytes a row: cell (col, row) is
the byte `(col >> 1) + row * 4`, its low nibble for an odd column. A grid
cell covers two by two map cells from x `0xB`, z `0xB1`. The init copies
`Area111_GridStart`; the image's start holds 0, 1, 2, 3 and `0xF` nibbles.
The handlers' cell arithmetic differs in rounding, faithfully:

- **`Area111_SlideMark`** (handler 1): the running object's cell ((x high
  word - `0xB`) `>> 1`, the same for z less `0xB1`: rounding down) marked
  `0x10`; then from it, by the object's direction (the cell steps
  `0x66971C`, the direction re-read after the call), cells stepped while
  inside the grid (16-bit tests, 0..6) and their nibble 0, the count `+ 4`
  each (a byte). A count: the script object's `+7` = count `+ 1`. The
  object's dword `+0x14` = 0, the cell it **stopped at** (outside the grid
  or blocked) marked 0, `Sound_PlayEffect(0x203)`.
- **`Area111_AttachAtLeaderCell`** (handler 2): the nibble at the leader's
  cell (halved toward 0, then taken as bytes: unchecked) `- 1`; `0xFF` (an
  empty cell): nothing. Else `MoveCmd_Attach(the script object, 7, that,
  Field_State's party slot + 1, 0x10)` with the script position word kept
  across the call; the running object's direction = extra object that's
  `+8 & 7`, and `Sprite_Kind2 +8` the same.
- **`Area111_PlaceMemberInGrid`** (handler 5): the key = the active
  member's extra index `+ 1`; the running object's cell (halved toward 0)
  marked 0; every nibble of the grid equal to the key cleared (each byte is
  seen twice, once for each of its columns, its low nibble tested first);
  the key or'd into the object's cell's nibble; `Sound_PlayEffect(0x204)`.
- **`Area111_AttachAhead`** (handler 6): extra object k's cell (k the
  running object's byte `+0x18`; halved toward 0), stepped once by the
  running object's direction (bytes); outside 0..6 (unsigned): nothing; its
  nibble `- 1` at 4 or more: nothing. Else `MoveCmd_Attach(the script
  object, 7, block, slot + 1, 0xC)` keeping the script position, extra
  object block's `+8` = the running object's direction `& 7`, the script
  position `- 6`.
- **`Area111_SlideAhead`** (handler 7): from the running object's cell (the
  16-bit high word less `0xB` / `0xB1`, shifted right once, as bytes) cells
  stepped while inside (signed bytes 0..6) and their nibble not `0xF`, the
  count `+ 4` each. A count: the script object's `+7` = count `+ 1`,
  `MoveCmd_MoveKind2(the object's direction byte)`, `MoveScript_FAWord` 0,
  the script object's `+0` bit `0x40`, the running object's `+0x14` = 0 and
  word `+0x3E` = `0xFB00`.
- **`Area111_StepToExtra`** (handler 8): extra object k's x and z (k the
  running object's **dword** `+0x18`), their high words stepped by twice the
  cell step of the object's direction; the ground there less the object's
  word `+0x3E` (16 bits, the object read again after the call). With the
  script object's `+4` set: the object's `+9` = `0x20` and its dwords
  `+0xC`, `+0x10`, `+0x14` = the x, z and height differences `/ 32`
  (signed, toward 0).
- **`Area111_TailGate`** (tail kind 17): the s8 state `- 1` through the
  byte table `0x417EB8` (24 entries) to the jump table `0x417E98` (8):
  states 1, 3, 5, 7 - `Party_DropIn(0)`, the timer and the state `0xA`;
  `0xA` - the timer down to 0: kind, state and sub-kind 0, then
  `ScriptFlags_Clear40`; `0x14` - `MoveCmd_TestFB(0x11, 0xB7)`, the timer
  `0x1E`, `0x15`; `0x15` - the timer: `Field_Kind2X` / `Z` = (`0x118000`,
  `0xAF0000`), the divisor from `Field_MoveSpeeds[4]`, `MoveScript_FAWord`
  0, `0x16`; `0x16` - `Kind2Hold` 0: `MoveCmd_TestFB(0x11, 0xAD)`, the gate's
  four map bytes `0xA1` / `0xC0`, `Sound_PlayEffect(0x205)`, story flag
  `0x28`, the timer `0x1E`, `0x17`; `0x17` - the timer: Kind2 X / Z the
  leader's, the divisor, FAWord 0, `0x18`; `0x18` - `Kind2Hold` 0: the
  timer 1, `0xA`. Any other state: nothing.
- **`Area111_ArriveHook`**: with `Cond_ByteFD` 0 story flag `0x28` cleared;
  unless `Cond_ByteFD` (read again) is 1, al 0. The cell in half cells (x
  `>> 15`, z `>> 15`, 16 bits): on the grid's edge x `0x16` with z in
  `0x162..0x17C`, arriving with an x step of +1:
  `Area111_ArmTailAtLeaderCell(3)`; x `0x30` stepping -1: 7; z `0x162`
  stepping +1 (x in `0x16..0x30`): 5; z `0x17C` stepping -1: 1; the inner
  gates x `0x20` (-1) / `0x26` (+1) over z `0x16E..0x170`: 7 / 3, z `0x16C`
  (-1) / `0x172` (+1) over x `0x22..0x24`: 1 / 5. Else the cell (x high
  word `0x11..0x12`, z `0xB7..0xB8`) with flag `0x28` clear:
  `ScriptFlags_Set40`, tail kind `0x11`, state `0x14`. **al 0 always.** The
  direction is the running object's (`Sprite_Current +8`).

### Area 112 (descriptor `0x61FF30`; PSX `0x801F48B8`)

`+0x3C` has 6 entries, `+0x34` 3: choices 0..2 are handlers 3..5. Handler 1
is `0x421FB0` (world 3's block, group AR3F's band); no code here names it.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x418350` | `Area112_ChangeAreaByFlags` | `0xB0` | handler 0 (PSX `0x801F3558`) | kHandler | `Field_ChangeArea` by story flags `0x34` (2) and `0x35` (1): 1 - (`0x8F`, `0x388000`, `0x650000`, `0x81`); 2 - (`0x92`, `0x458000`, `0x2B0000`, `0x81`); 3 - (`0x64`, `0x468000`, `0x340000`, `0x81`); neither - (`0x91`, `0xA8000`, `0x630000`, `0x84`) |
| `0x418400` | `Area112_TalkByMember` | `0x8B` | handler 2 (PSX `0x801F366C`) | kHandler | the first of `Area112_MemberKeys` that some member has as its `+0x89` (the count read once): `Msg_OpenScript(Area112_Messages[that])`, `Field_Request` 2; none: nothing |
| `0x418490` | `Area112_ChoiceFlags34` | `0xD8` | choice 0 = handler 3 (PSX `0x801F3730`) | kChoice | the message word the s8 answer `+ 3`; story flag `0x33` set; by the answer (read again after the call; the jump table `0x418558`, 4): 1 - `0x34` cleared, `0x35` set; 2 - `0x34` set, `0x35` cleared; 3 - both set; 4 - the message `0xFFFF`; any other - both cleared. Then without Cond row 13's flag `0x10`: the message 2 and flag `0x33` cleared |
| `0x418570` | `Area112_ChoiceMessage13` | `0x17` | choice 1 = handler 4 (PSX `0x801F3870`) | kChoice | the message `0x14` for an answer, else `0x13` |
| `0x418590` | `Area112_ChoiceClearFlags` | `0x81` | choice 2 = handler 5 (PSX `0x801F3898`) | kChoice | `0xFFFF`; answer 0: story flags `0x3C..0x3F`, `0x38..0x3B` cleared, `Sound_PlayEffect(0x200)` |
| `0x418620` | `Area112_StepHook` | `0x71` | `Area_StepHook`'s case for area 112 (`0x56E142`) | kHook | `Cond_ByteFD` 3, story flag `0x33`, Cond row 13's flag `0x10`, the x high word `0xB..0xD` and z `0x57..0x59` (16 bits), the leader's pose byte 0, 7 or 6: counter 0 = 0, `Party_DropIn(0)`, al 1; else al 0 |
| `0x4186A0` | `Area112_CellHook` | `0x119` | `Area_CellHooks` record for area `0x70` (`0x662FCC`) | kHook | the first of `Area112_HookCells` whose x and z bytes are the cell's while the leader's pose byte is not 3; none, story flag `0x1C`, or the entry's flag: al 0. `Area112_MemberNearBoxes`: `ScriptFlags_Set40`, tail kind `0xA`, state `0xA`, sub-kind `0x15`, al 0. Else each set bit of the entry's bits byte - bits 0..3 clear, 4..7 set `Area112_HookFlags[bit & 3]` - then the entry's flag set, `Sound_PlayEffect(0x200)`, `Effect_HoldFlag1C(0xF)`, al 1 |
| `0x4187C0` | `Area112_MemberNearBoxes` | `0x9D` | called by the cell hook | kCallee | al 1 when some member's next position (`+0x34 + +0xC * +9`, `+0x38 + +0x10 * +9`, wrapping) has its high words within one of `Area112_Boxes` (\|d\| below the half size; the absolute value as `cdq / xor / sub`); else al 0 |
| `0x418860` | `Area112_EffectRun` | `0x12` | `Effect_KindHandlers[0xB2]` (`0x655618`) | kCallee | `jmp [Area112_EffectStates + Sprite_Current[1] * 4]` (the running effect record) |
| `0x418880` | `Area112_EffectRing` | `0x2B` | `Area112_EffectStates[1]` | kState | **`Area100_EffectB7Ring`'s code** byte for byte but the call's displacement: the record's point (`+0x34`, `+0x38`, `+0x3C`) copied to the stack and handed to `0x4220D0` |

`Area112_EffectStates[0]` is area 59's `Area59_EffectGround` (`0x40B4F0`,
group AR1D's, ours).

### Area 113 (descriptor `0x620010`; no PSX records)

`+0x34` has 5 entries: choices 1 and 2 are `0x420850` / `0x420870` (group
AR3F's band), 3 and 4 area 61's `Area61_ChoiceMark4`. No handlers, no init.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4188B0` | `Area113_ChoiceAsk` | `0x3F` | choice 0 | kChoice | an answer: the message 4 and the mark `0x9398CF` = 6; answer 0: the byte `0x9045F4` at 6 or more the message 2, else 3 and the mark 6 |
| `0x4188F0` | `Area113_Trigger27` | `0x16` | object trigger 27 | kCallee | `ScriptFlags_Set40`, tail kind 4, sub-kind 6 (the state not written); al 0 |
| `0x418910` | `Area113_Trigger39` | `0x1D` | object trigger 39 | kCallee | `ScriptFlags_Set40`, tail kind `0x2C`, state 0, sub-kind `0xD`; al 0 |
| `0x418930` | `Area113_Trigger63` | `0x23` | object trigger 63 | kCallee | `ScriptFlags_Set40`, tail kind `0x3E`, state 0, the object's word `+0x88` = 5; al 0 |
| `0x418960` | `Area113_TailReward` | `0xE0` | tail kind 62 (armed by trigger 63) | kTail | the s8 state through the byte table `0x418A34` (12) to the jump table `0x418A1C` (6), unsigned above `0xB` nothing: 0 - the state = `Area113_Reward()`; 1 - `Field_Request` not 2: `ScriptFlags_Clear40`, kind and state 0; 5 - the same after `Sound_PlayEffect(0x106)`; `0xA` - request not 2: `Sound_StopMusic`, `Sound_LoadStream(2)`, state `0xB`; `0xB` - the stream done and request not 2: `Sound_ResumeAll`, `ScriptFlags_Clear40`, kind and state 0, the focus object's (`0x903804`, read after the call) word `+0x88` = 9 |
| `0x418A40` | `Area113_Reward` | `0x192` | called by the tail | kCallee | by the byte `0x9045F4`: `0xA` - flag `0x96` set: message 5, answer 1; else `Inventory_Add(3, 0x2D, 3)`, flag `0x96`, message 2, answer 5. `0xB` - flag `0x97` set: 5 / 1; else item `0x33` (1) and flag `0x97`, then flag `0x96` set: message 6, else item `0x2D` (3), flag `0x96`, message 3; answer 5. `0xC` - key item `0xF` held: 5 / 1; else `KeyItem_Add(0xF)`, then flag `0x97` set: 8; else item `0x33` and flag `0x97`, then flag `0x96` set: 7, else item `0x2D`, flag `0x96`, 4; answer `0xA`. Any other: message 1, answer 1. Then `Msg_OpenScript(message)`, `Field_Request` 2; al the answer |

The originals push a fourth word, 0, to `Inventory_Add`, whose signature
(ours, [`symbols.toml`]) takes three; ours passes three.

## 2. Ours

`src/game/area_w2f.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
one raw address of section 9). The group's own callees are called the same
way (`AH_CALL(Area108_SetCells)`, `AH_CALL(Area111_MarkCells)`,
`AH_CALL(Area111_ArmTailAtLeaderCell)`, `AH_CALL(Area112_MemberNearBoxes)`,
`AH_CALL(Area113_Reward)`), so the fuzz stands a recorder in for each and
every function is tested alone; the two state dispatchers read their
`.data` tables in place and call the entry, so the fuzz's `DataTable` swap
stands recorders there. Shapes that repeat are one helper: the place tables
(`PlaceAtCell`), the member strips (`MemberMask`), area 110's draw
(`PlaceRandomObject`, area 72's), area 111's nibble (`Nibble`), leader cell
(`LeaderCell`), party slot (`FieldStateSlot`) and extra index
(`ActiveMemberExtra`), and area 111's arrive edges (`Edge`). Kept as the
originals: every re-read after a call (the running object, the active
member, the script object and the focus object, the tail state after
`Kind2_Place`, the answer after `Flags_Set`, `Cond_ByteFD` after
`Flags_Clear`), the order of every call and store, each function's own
rounding of a cell (down or toward 0), the 8-bit and 32-bit shift counts
masked to 5 bits, the 16-bit and 8-bit bound tests, the sign extensions,
the unchecked reads of section 6 that stay in `.data`.

## 3. The fuzz

`BOF3X_SHADOW=area_w2f` (`src/game/area_w2f_fuzz.cpp`): five `Run`s under
the one shadow name, one per area with its `Group::area` (108, 110, 111,
112, 113), 8,000 rounds per function, the real descriptors and tables in
place. Area 110's clone starts at the body `0x417590`: its entry's `jmp`
over eleven `nop`s is refused by `bof3::CloneOriginal` as a patched entry,
as areas 63, 64, 72 and 73's were; its call sites are the tool's less
`0x10`.

- **Callees the group lists:** `AreaMap_SetByte` / `AreaMap_SetHeight` with
  masks of words x, z and a value byte (area 108's helper pushes registers
  loaded as words; area 111's handler 1 passes dwords whose upper halves are
  the original's registers); `Party_DropIn` a byte (area 108's scene pushes a
  dword whose upper bytes are its stack); `Flags_Toggle`; `Effect_FindFree`
  `kByte 0xFF..0x03`; `Sprite_SetTint` `kFlag`; `Tint_Release`;
  `Kind2_Place`; `ScriptFlags_Set40` / `Clear40`; `MoveCmd_TestFB` `kFlag`;
  `MoveCmd_Attach` (the object whole, four bytes); `MoveCmd_MoveKind2`;
  `Effect_HoldFlag1C`; `KeyItem_Has` / `KeyItem_Add` `kFlag`;
  `Sound_LoadStream`, `Sound_StreamDone` `kFlag` (tested whole),
  `Sound_StopMusic` (Capcom's); `0x4220D0` by raw address, logging the 12
  bytes of the point it is handed, not its stack address; the group's own
  `Area108_SetCells` (a byte), `Area111_MarkCells` (two words and a byte),
  `Area111_ArmTailAtLeaderCell` (a byte), `Area112_MemberNearBoxes`
  (`kFlag`), `Area113_Reward`.
- **Louder stand-ins** (an `effect`, half the time, from `Noise`), because a
  caller reads a cell again after the call: `Effect_FindFree`,
  `Tint_Release`, `MoveCmd_TestFB` and `Area111_MarkCells` move the running
  object; `Party_DropIn`, `Sprite_SetTint` and `Area108_SetCells` the
  active member (and the running object); `MoveCmd_Attach` and
  `MoveCmd_MoveKind2` the script object, its position word and the running
  object; `AreaMap_Elevation` the running and the script objects;
  `Kind2_Place` the tail state and counter 3; `Flags_Set` the answer byte;
  `ScriptFlags_Clear40` the focus object. The harness's own disturbance
  moves a group cell about one call in 24.
- **Data tables** swapped for recorders: `Area108_FadeStates`,
  `Area112_EffectStates`.
- **Regions beyond the field frame** (38 with the harness's, 24,330 bytes):
  all twenty `Effect_Objects` records, `Sprite_Kind2`, the active member,
  script object and focus object pointers, `MoveScript_PartyRecords`
  records 0..1, `Field_Kind2Hold`, `Field_MoveSpeeds[3..4]`,
  `Area108_Model` and **the fuzz's own point buffer** (`0x1440` bytes: the
  round points `Area108_Model`'s pointer into it, since the image's points
  at 33 records of the area's `.data` and a count can reach 127),
  `MoveScript_EffectState`, the mark, the byte `0x9045F4`, area 110's cells
  and weights, area 111's grid (and the four bytes after it), its grid start
  and cell bytes.
- **Every round:** the active member at one of the four extra objects (half
  the time), a field object, a party record or the running object; the
  script object at a field object or a party record; the focus object at a
  field object; `Area108_Model`'s pointer into the point buffer.
- **Seeds:** area 108's answers at every value a choice tests, the sign
  edge and above; its scene's leader id 2 or beside, extra object 1 at one
  of the five places or one field off, the leader's direction with high
  bits, the other members' high words on and beside each strip's bounds, a
  member count of 0, 9 or 10 a thirty-sixth of the time; the place index
  1..9, 0 and `0xA`; `MoveScript_EffectState` at the leader's id 5 half the
  time; the fade bytes at `0xCF` / `0xD0` / `0xD1` and their sign edge, all
  three at `0xD0` half the time; `Field_Request` 5 and beside; the cell
  hook's cell one of the four or one off (bytes, anything above), the pose 7
  (the image's direction) or with a high nibble; the tail at each state and
  beyond, its timer at 1, 2, 0 and `0x101`, `Kind2Hold`, counter 3 at 2 and
  4 and beside, extra object 1's `+0x83` at 9 and beside, the model's count
  at `0x21`, 0..2, 5, `0x7F`, `0x80`, `0xFF`. Area 110's image weights a third
  of the time, weights of 0..9 a third (the walk passes the eighth: "none"),
  else random; the image's cells two times in three. Area 111's grid
  nibbles 0 with a set probability per handler, else 1..4, 5 or `0xF`; the
  running object's and the leader's cells inside and one outside each way,
  the direction with high bits; handler 5's object always inside the grid
  (its write's byte; x words down to `0xA`, which halves to 0); handler 6's
  extra index 0..3 or 4, 5, `0xFF`; handler 8's dword index 0..3 or 4, 5,
  `0x10`, the script object's `+4`; the shift dwords 0..4, 7, 8, 31..33 and
  a high bit; counter 3 at the member's bit or beside; the tail at each
  state and beyond; the arrive hook's cell on one of the eight edges (the
  edge coordinate or one off, the other inside its span or one past either
  end) or the final cell's high words, `Cond_ByteFD` 1 and beside; the
  helper's leader on a block nibble 1..4. Area 112's member ids at each key
  and beside, a member count of 0; the step hook's high words on and beside
  their three, `Cond_ByteFD` 3 and beside, the pose at 0 / 7 / 6 and
  others; the cell hook's cell one of four or one off, the pose 3 or not;
  the boxes' members at each box's centre, half size and one inside; the
  effect record as the running object. Area 113's `0x9045F4` at 5, 6, 7 and
  at `0xA..0xC` and beside; the tail's states and `Field_Request` 2 or not.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3, the three pointers, the timer, the answer, `Kind2Hold`.

**Result (in this worktree):** 424,000 rounds over the 53 functions (8,000
each), 495,681 calls to the stand-ins, 0 mismatches, 24,330 bytes of state
(38 regions) and the log compared. Coverage (calls the originals made,
per area): area 108 - `Party_DropIn` 1,605 (the scene), `Area108_SetCells`
2,607, `Flags_Toggle` 315 (the cell hook's toggle), `Kind2_Place` 290,
`MapView_SetElevation` 266, the fade states 4,114 / 3,886; area 110 -
`Rand` 14,825 and `AreaMap_Elevation` 6,825 (the "none" path in the other
1,175); area 111 - `Area111_MarkCells` 109,750, `MoveCmd_Attach`
6,479, `MoveCmd_MoveKind2` 4,407, `Area111_ArmTailAtLeaderCell` 382 (the
arrive hook's eight edges), `Party_DropIn` 1,510, `Flags_Set` 429 (the
tail's state `0x16`); area 112 - `Field_ChangeArea` 8,000, `Msg_OpenScript`
4,206, `Area112_MemberNearBoxes` 347, `Effect_HoldFlag1C` 105 (the cell
hook's last path), `Party_DropIn` 86 (the step hook, six conditions at
once), the effect states 3,972 / 4,028, `0x4220D0` 8,000; area 113 -
`Area113_Reward` 1,000, `Inventory_Add` 1,143, `KeyItem_Add` 415,
`Sound_LoadStream` 827, `Sound_ResumeAll` 646.

`BOF3X_SHADOW='*'`: exit 0 on the first run, `inject: 4989 ours` (one below
the 4,990 `impl` lines, the off-by-one the round doc section 10 notes from
before wave two), 452 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w2f.cpp` by a script (the scratch
`controls.py`, not committed) that plants on an anchor it checks is unique,
rebuilds, checks `area_w2f.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=area_w2f`, restores; after the last it rebuilt and ran the clean
self-test (exit 0, 0 mismatches in all five runs). **348 controls planted, 346 refused by a count, 2 by a hang with near variants refused by a count**; no fault,
no equivalent. Every one of the 53 functions has at least one control of its
own; a control in a helper shared by two functions (`PlaceAtCell`,
`DecTimer`, `LeaderCell`, `Nibble`, `FieldStateSlot`, `ActiveMemberExtra`) is
refused in the first function's run, whose Fatal ends the self-test.

Three were not refused on the first run, each the fuzz's fault first:

- **B8** (area 110's walk over seven weights, not eight) differs only when
  the roll passes the first seven weights; the image's weights sum to
  `0x40` and random ones are rarely that small. The seed now fills the
  weights with 0..9 a third of the time; all ten of area 110's controls
  were re-run on it, B8 refused in 1,175 rounds.
- **C18** and **C51** (area 111's slides stepping z by the x step) refused
  by a hang: for a direction whose x step is 0 the stepped cell never moves
  and ours loops (the launcher's own pid killed after 900 s). Their near
  variants **C18b** / **C51b** (the z step negated) terminate and are
  refused by a count.

The thinnest by a count: A18 (1), A21 (1), C87 (1), A20 (2), C32 (2), A22 (5), C90 (12), A93 (13); the rest
need 60 rounds or more. A18..A22 move one bound of a place's strip, which
shows only when the leader is member 2 at that place, extra object 1 on it
and another member on the moved bound at once; C87 and C32 need the arrive
hook's last cell of an edge, and a member index of 16..31 among the extra
objects. Refused, though thinly: a stronger seed would pair them.

| # | Function | Planted | Refused in rounds (of 8,000 per function) |
|---|---|---|---|
| A1 | `Area108_ChoiceArmTail5` | sub-kind 4 for 3 | Area108_ChoiceArmTail5 530 |
| A2 | `Area108_ChoiceArmTail5` | flag 0x1D for 0x1E | Area108_ChoiceArmTail5 530 |
| A3 | `Area108_ChoiceArmTail5` | answer 2 for 1 | Area108_ChoiceArmTail5 1140 |
| A4 | `Area108_ChoiceArmTail5` | tail kind 6 | Area108_ChoiceArmTail5 1092 |
| A5 | `Area108_ChoiceArmTail5` | message 0xFFFE | Area108_ChoiceArmTail5 7956 |
| A6 | `Area108_ChoiceArmTailA` | sub-kind 5 | Area108_ChoiceArmTailA 555 |
| A7 | `Area108_ChoiceArmTailA` | kind 0xB | Area108_ChoiceArmTailA 555 |
| A8 | `Area108_ChoiceArmTailA` | answer above 1 returns | Area108_ChoiceArmTailA 584 |
| A9 | `Area108_ClearCounter1Bits0` | & 0xFD | Area108_ClearCounter1Bits0 3984 |
| A10 | `Area108_ClearCounter1Bits2` | & 0xF7 | Area108_ClearCounter1Bits2 4022 |
| A11 | `Area108_ClearCounter1Bits4` | & 0xDF | Area108_ClearCounter1Bits4 4057 |
| A12 | `Area108_ClearCounter1Bits6` | & 0x7F | Area108_ClearCounter1Bits6 4018 |
| A13 | `Area108_PlaceScene` | member & 0xFC | Area108_PlaceScene 3974 |
| A14 | `Area108_PlaceScene` | member id 3 | Area108_PlaceScene 1855 |
| A15 | `Area108_PlaceScene` | direction & 0xF | Area108_PlaceScene 387 |
| A16 | `Area108_PlaceScene` | z not compared | Area108_PlaceScene 235 |
| A17 | `Area108_PlaceScene` | 4 places | Area108_PlaceScene 2031 |
| A18 | `Area108_PlaceScene` | place 0 x >= 0x27 | Area108_PlaceScene 1 |
| A19 | `Area108_PlaceScene` | place 1 z from 0x52 | Area108_PlaceScene 20 |
| A20 | `Area108_PlaceScene` | place 2 z > 0x3B | Area108_PlaceScene 2 |
| A21 | `Area108_PlaceScene` | place 3 x > 0x22 | Area108_PlaceScene 1 |
| A22 | `Area108_PlaceScene` | place 4 x from 0x1D | Area108_PlaceScene 5 |
| A23 | `Area108_PlaceScene` | strips 3 wide | Area108_PlaceScene 50 |
| A24 | `Area108_PlaceScene` | records 0..count-2 | Area108_PlaceScene 77 |
| A25 | `Area108_PlaceScene` | bit i + 1 | Area108_PlaceScene 77 |
| A26 | `Area108_PlaceScene` | place 0 SetCells(0) | Area108_PlaceScene 77 |
| A27 | `Area108_PlaceScene` | script 4 for 3 | Area108_PlaceScene 46 |
| A28 | `Area108_PlaceScene` | flag 0x1A for 0x19 | Area108_PlaceScene 198 |
| A29 | `Area108_PlaceScene` | script 6 for 5 | Area108_PlaceScene 282 |
| A30 | `Area108_PlaceScene` | place 2 no SetCells | Area108_PlaceScene 333 |
| A31 | `Area108_PlaceScene` | place 4 scripts swapped | Area108_PlaceScene 342 |
| A32 | `Area108_PlaceScene` | record 1 bit 1 | Area108_PlaceScene 1186 |
| A33 | `Area108_PlaceScene` | tail kind 0xD | Area108_PlaceScene 1605 |
| A34 | `Area108_PlaceScene` | timer 0xE | Area108_PlaceScene 1605 |
| A35 | `Area108_PlaceScene` | script position 1 | Area108_PlaceScene 1605 |
| A36 | `Area108_PlaceScene` | place 1 script before DropIn | Area108_PlaceScene 122 |
| A37 | `PlaceAtCell (108)` | index not less 1 | Area108_PlaceAtCellA 10991 |
| A38 | `PlaceAtCell (108)` | x half 1 for 0 | Area108_PlaceAtCellA 8409 |
| A39 | `PlaceAtCell (108)` | z the x byte | Area108_PlaceAtCellA 15022 |
| A40 | `PlaceAtCell (108)` | z half by the x flag | Area108_PlaceAtCellB 12418 |
| A41 | `Area108_PlaceAtCellB` | table B + 4 | Area108_PlaceAtCellB 5767 |
| A42 | `Area108_ScriptByFlags1E` | script 0x13 | Area108_ScriptByFlags1E 5353 |
| A43 | `Area108_ScriptByFlags1E` | flag 0x1E twice | Area108_ScriptByFlags1E 2647 |
| A44 | `Area108_ScriptByFlags1E` | position 0xFFFC | Area108_ScriptByFlags1E 8000 |
| A45 | `Area108_FlagIfEffectState5` | state 4 | Area108_FlagIfEffectState5 1815 |
| A46 | `Area108_FlagIfEffectState5` | flag 0x31 | Area108_FlagIfEffectState5 1161 |
| A47 | `Area108_FlagIfEffectState5` | TestFB (0xB, 1) | Area108_FlagIfEffectState5 1161 |
| A48 | `Area108_FlagIfEffectState5` | object +0 = 1 | Area108_FlagIfEffectState5 1161 |
| A49 | `Area108_FlagIfEffectState5` | member bit kept | Area108_FlagIfEffectState5 3991 |
| A50 | `Area108_FadeRun` | state ^ 1 | Area108_FadeRun 8000 |
| A51 | `Area108_FadeBegin` | tint a 2 | Area108_FadeBegin 8000 |
| A52 | `Area108_FadeBegin` | tint + 1 kept | Area108_FadeBegin 8000 |
| A53 | `Area108_FadeBegin` | & 0x3F | Area108_FadeBegin 4049 |
| A54 | `Area108_FadeBegin` | +0x5C 2 | Area108_FadeBegin 8000 |
| A55 | `Area108_FadeBegin` | +0x5E 0x81 | Area108_FadeBegin 8000 |
| A56 | `Area108_FadeBegin` | state 2 | Area108_FadeBegin 8000 |
| A57 | `Area108_FadeBegin` | member position - 4 | Area108_FadeBegin 8000 |
| A58 | `Area108_FadeStep` | unsigned compare | Area108_FadeStep 2915 |
| A59 | `Area108_FadeStep` | +0x5F not stepped | Area108_FadeStep 2230 |
| A60 | `Area108_FadeStep` | +0x5F not tested | Area108_FadeStep 135 |
| A61 | `Area108_FadeStep` | release +0x9E | Area108_FadeStep 4027 |
| A62 | `Area108_FadeStep` | & 0xCF | Area108_FadeStep 2002 |
| A63 | `Area108_FadeStep` | +0x5E 1 | Area108_FadeStep 4038 |
| A64 | `Area108_FadeStep` | state through the object read before | Area108_FadeStep 1758 |
| A65 | `Area108_FindEffectSlot` | none 0xFE | Area108_FindEffectSlot 1580 |
| A66 | `Area108_FindEffectSlot` | store to the object read before | Area108_FindEffectSlot 3509 |
| A67 | `Area108_SpawnEffect51` | z 0x418001 | Area108_SpawnEffect51 6408 |
| A68 | `Area108_SpawnEffect51` | kind 0x52 | Area108_SpawnEffect51 6408 |
| A69 | `Area108_SpawnEffect51` | +1 0xB | Area108_SpawnEffect51 6408 |
| A70 | `Area108_SpawnEffect51` | ground << 15 | Area108_SpawnEffect51 6408 |
| A71 | `Area108_SpawnEffect51` | none: script - 4 | Area108_SpawnEffect51 1592 |
| A72 | `Area108_CellsIfRequest5` | request 4 | Area108_CellsIfRequest5 3267 |
| A73 | `Area108_CellsIfRequest5` | SetCells(1) | Area108_CellsIfRequest5 2197 |
| A74 | `Area108_CellHook` | direction & 7 | Area108_CellHook 840 |
| A75 | `Area108_CellHook` | z not compared | Area108_CellHook 273 |
| A76 | `Area108_CellHook` | none answers 2 | Area108_CellHook 7033 |
| A77 | `Area108_CellHook` | flag 0x1D | Area108_CellHook 967 |
| A78 | `Area108_CellHook` | toggles the direction byte | Area108_CellHook 315 |
| A79 | `Area108_CellHook` | sound 0x207 | Area108_CellHook 315 |
| A80 | `Area108_CellHook` | kind 0x19 | Area108_CellHook 246 |
| A81 | `Area108_CellHook` | +0xB 0x1C | Area108_CellHook 246 |
| A82 | `Area108_CellHook` | z word the x byte | Area108_CellHook 246 |
| A83 | `Area108_CellHook` | +1 = 1 | Area108_CellHook 246 |
| A84 | `Area108_CellHook` | answers 2 | Area108_CellHook 315 |
| A85 | `Area108_TailPlace` | state 8 for 9 | Area108_TailPlace 854 |
| A86 | `Area108_TailPlace` | state 0 X from z | Area108_TailPlace 224 |
| A87 | `Area108_TailPlace` | divisor << 2 | Area108_TailPlace 224 |
| A88 | `Area108_TailPlace` | Kind2_Place without - 1 | Area108_TailPlace 290 |
| A89 | `Area108_TailPlace` | Kind2 +0x84 5 | Area108_TailPlace 290 |
| A90 | `Area108_TailPlace` | counter 3 = 2 | Area108_TailPlace 290 |
| A91 | `Area108_TailPlace` | state not read again | Area108_TailPlace 139 |
| A92 | `Area108_TailPlace` | script 8 for 9 | Area108_TailPlace 31 |
| A93 | `Area108_TailPlace` | state 5 for 4 | Area108_TailPlace 13 |
| A94 | `Area108_TailPlace` | state 2 divisor from speed 3 | Area108_TailPlace 104 |
| A95 | `Area108_TailPlace` | state 2 Z the leader x | Area108_TailPlace 106 |
| A96 | `Area108_TailPlace` | elevation (Z, X) | Area108_TailPlace 266 |
| A97 | `Area108_TailPlace` | state 3 kind 1 | Area108_TailPlace 266 |
| A98 | `Area108_TailPlace` | counter 3 at 3 | Area108_TailPlace 185 |
| A99 | `Area108_TailPlace` | kind 0x36 | Area108_TailPlace 77 |
| A100 | `Area108_TailPlace` | ground zero-extended | Area108_TailPlace 44 |
| A101 | `Area108_TailPlace` | flags & 0xF3 | Area108_TailPlace 46 |
| A102 | `Area108_TailPlace` | timer 0x1F | Area108_TailPlace 102 |
| A103 | `Area108_TailPlace` | state 8 after the scene | Area108_TailPlace 246 |
| A104 | `Area108_TailPlace` | TestFB (0x20, 0x2E) | Area108_TailPlace 246 |
| A105 | `Area108_TailPlace` | extra 1 hidden | Area108_TailPlace 246 |
| A106 | `Area108_TailPlace` | count unsigned | Area108_TailPlace 212 |
| A107 | `Area108_TailPlace` | records from + 4 | Area108_TailPlace 321 |
| A108 | `Area108_TailPlace` | vy >> 7 | Area108_TailPlace 320 |
| A109 | `Area108_TailPlace` | vx unsigned shift | Area108_TailPlace 280 |
| A110 | `Area108_TailPlace` | three corners in z | Area108_TailPlace 321 |
| A111 | `Area108_TailPlace` | count + 1 records | Area108_TailPlace 321 |
| A112 | `Area108_TailPlace` | +0x5F less 3 | Area108_TailPlace 581 |
| A113 | `Area108_TailPlace` | Var7 4 | Area108_TailPlace 224 |
| A114 | `Area108_TailPlace` | step 1 | Area108_TailPlace 224 |
| A115 | `DecTimer (108, 111)` | a byte timer | Area108_TailPlace 1515 |
| A116 | `Area108_SetCells` | height 0xF | Area108_SetCells 6220 |
| A117 | `Area108_SetCells` | byte 0x10 | Area108_SetCells 6220 |
| A118 | `Area108_SetCells` | z + 1 | Area108_SetCells 8000 |
| A119 | `Area108_SetCells` | three cells | Area108_SetCells 8000 |
| B1 | `Area110_PlaceRandomObject` | roll & 0x1F | Area110_PlaceRandomObject 2808 |
| B2 | `Area110_PlaceRandomObject` | roll <= weight | Area110_PlaceRandomObject 717 |
| B3 | `Area110_PlaceRandomObject` | others +0 = 1 | Area110_PlaceRandomObject 8000 |
| B4 | `Area110_PlaceRandomObject` | pick & 3 | Area110_PlaceRandomObject 3363 |
| B5 | `Area110_PlaceRandomObject` | z << 15 | Area110_PlaceRandomObject 6812 |
| B6 | `Area110_PlaceRandomObject` | ground + 1 | Area110_PlaceRandomObject 6825 |
| B7 | `Area110_PlaceRandomObject` | zone - 4 | Area110_PlaceRandomObject 8000 |
| B8 | `Area110_PlaceRandomObject` | seven weights | Area110_PlaceRandomObject 1175 |
| B9 | `Area110_PlaceRandomObject` | cells + 2 | Area110_PlaceRandomObject 6825 |
| B10 | `Area110_PlaceRandomObject` | seven objects cleared | Area110_PlaceRandomObject 7708 |
| C1 | `Area111_ChoiceMessage2` | message 4 | Area111_ChoiceMessage2 7430 |
| C2 | `Area111_ChoiceStartVar7` | Var7 6 | Area111_ChoiceStartVar7 539 |
| C3 | `Area111_ChoiceStartVar7` | step 1 | Area111_ChoiceStartVar7 539 |
| C4 | `Area111_ChoiceStartVar7` | answer 1 | Area111_ChoiceStartVar7 1080 |
| C5 | `Area111_CopyExtra6C` | +0x68 | Area111_CopyExtra6C 8000 |
| C6 | `Area111_SlideMark` | z halved toward 0 | Area111_SlideMark 434 |
| C7 | `Area111_SlideMark` | x - 0xA | Area111_SlideMark 4076 |
| C8 | `Area111_SlideMark` | mark 0x11 | Area111_SlideMark 8000 |
| C9 | `Area111_SlideMark` | direction not read again | Area111_SlideMark 2946 |
| C10 | `Area111_SlideMark` | x up to 5 | Area111_SlideMark 1072 |
| C11 | `Area111_SlideMark` | z from 1 | Area111_SlideMark 649 |
| C12 | `Area111_SlideMark` | stops at nibble not 1 | Area111_SlideMark 3332 |
| C13 | `Area111_SlideMark` | count + 3 | Area111_SlideMark 3090 |
| C14 | `Area111_SlideMark` | +7 count + 2 | Area111_SlideMark 3090 |
| C15 | `Area111_SlideMark` | +0x10 cleared | Area111_SlideMark 8000 |
| C16 | `Area111_SlideMark` | stop cell 1 | Area111_SlideMark 8000 |
| C17 | `Area111_SlideMark` | sound 0x202 | Area111_SlideMark 8000 |
| C18 | `Area111_SlideMark` | z stepped by the x step | a hang (the stepped cell never moves for a direction without that step); variant below refused by a count |
| C19 | `LeaderCell (111)` | x rounded down | Area111_AttachAtLeaderCell 97 |
| C20 | `Nibble (111)` | nibbles swapped | Area111_AttachAtLeaderCell 20662 |
| C21 | `Area111_AttachAtLeaderCell` | empty 0xFE | Area111_AttachAtLeaderCell 2854 |
| C22 | `Area111_AttachAtLeaderCell` | attach t 0x11 | Area111_AttachAtLeaderCell 5146 |
| C23 | `FieldStateSlot (111)` | slot + 2 | Area111_AttachAtLeaderCell 6479 |
| C24 | `Area111_AttachAtLeaderCell` | position + 1 | Area111_AttachAtLeaderCell 5146 |
| C25 | `Area111_AttachAtLeaderCell` | direction & 3 | Area111_AttachAtLeaderCell 1321 |
| C26 | `Area111_AttachAtLeaderCell` | Kind2 +9 | Area111_AttachAtLeaderCell 5146 |
| C27 | `Area111_AttachAtLeaderCell` | script object not read again | Area111_AttachAtLeaderCell 2319 |
| C28 | `Area111_Counter3Bit` | shift & 7 | Area111_Counter3Bit 733 |
| C29 | `Area111_Counter3Bit` | 0x20 << | Area111_Counter3Bit 5098 |
| C30 | `Area111_WaitMemberBit` | 0x08 << | Area111_WaitMemberBit 3177 |
| C31 | `ActiveMemberExtra (111)` | stride 0xA0 | Area111_PlaceMemberInGrid 1249 |
| C32 | `Area111_WaitMemberBit` | shift & 15 | Area111_WaitMemberBit 2 |
| C33 | `Area111_PlaceMemberInGrid` | key + 2 | Area111_PlaceMemberInGrid 7741 |
| C34 | `Area111_PlaceMemberInGrid` | mark 1 | Area111_PlaceMemberInGrid 8000 |
| C35 | `Area111_PlaceMemberInGrid` | clears & 0xE0 | Area111_PlaceMemberInGrid 3777 |
| C36 | `Area111_PlaceMemberInGrid` | high nibble key + 1 | Area111_PlaceMemberInGrid 6612 |
| C37 | `Area111_PlaceMemberInGrid` | six columns | Area111_PlaceMemberInGrid 4630 |
| C38 | `Area111_PlaceMemberInGrid` | nibble swapped | Area111_PlaceMemberInGrid 7626 |
| C39 | `Area111_PlaceMemberInGrid` | sound 0x205 | Area111_PlaceMemberInGrid 8000 |
| C40 | `Area111_PlaceMemberInGrid` | six rows | Area111_PlaceMemberInGrid 3357 |
| C41 | `Area111_AttachAhead` | k from +0x19 | Area111_AttachAhead 1329 |
| C42 | `Area111_AttachAhead` | x rounded down | Area111_AttachAhead 79 |
| C43 | `Area111_AttachAhead` | rows 0..5 | Area111_AttachAhead 182 |
| C44 | `Area111_AttachAhead` | blocks 0..2 | Area111_AttachAhead 333 |
| C45 | `Area111_AttachAhead` | mode 6 | Area111_AttachAhead 1333 |
| C46 | `Area111_AttachAhead` | direction not read again | Area111_AttachAhead 526 |
| C47 | `Area111_AttachAhead` | script - 4 | Area111_AttachAhead 1333 |
| C48 | `Area111_AttachAhead` | row stepped by the x step | Area111_AttachAhead 1620 |
| C49 | `Area111_SlideAhead` | x - 0xA | Area111_SlideAhead 1947 |
| C50 | `Area111_SlideAhead` | cols up to 5 | Area111_SlideAhead 1368 |
| C51 | `Area111_SlideAhead` | row by the x step | a hang (the stepped cell never moves for a direction without that step); variant below refused by a count |
| C52 | `Area111_SlideAhead` | stops at 0xE | Area111_SlideAhead 1287 |
| C53 | `Area111_SlideAhead` | +7 count + 2 | Area111_SlideAhead 4407 |
| C54 | `Area111_SlideAhead` | direction & 7 | Area111_SlideAhead 2837 |
| C55 | `Area111_SlideAhead` | bit 0x20 | Area111_SlideAhead 3268 |
| C56 | `Area111_SlideAhead` | +0x3E 0xFA00 | Area111_SlideAhead 4407 |
| C57 | `Area111_SlideAhead` | FAWord 1 | Area111_SlideAhead 4407 |
| C58 | `Area111_SlideAhead` | count 4 returns | Area111_SlideAhead 4721 |
| C59 | `Area111_StepToExtra` | k + 1 | Area111_StepToExtra 5340 |
| C60 | `Area111_StepToExtra` | x step once | Area111_StepToExtra 6015 |
| C61 | `Area111_StepToExtra` | dz + 1 | Area111_StepToExtra 202 |
| C62 | `Area111_StepToExtra` | script +5 | Area111_StepToExtra 1468 |
| C63 | `Area111_StepToExtra` | +9 0x21 | Area111_StepToExtra 6554 |
| C64 | `Area111_StepToExtra` | dx rounded down | Area111_StepToExtra 3116 |
| C65 | `Area111_StepToExtra` | dy / 16 | Area111_StepToExtra 6549 |
| C66 | `Area111_StepToExtra` | height through the object read before | Area111_StepToExtra 2882 |
| C67 | `Area111_TailGate` | state 7 not a drop-in | Area111_TailGate 377 |
| C68 | `Area111_TailGate` | DropIn(1) | Area111_TailGate 1510 |
| C69 | `Area111_TailGate` | timer 0xB | Area111_TailGate 1510 |
| C70 | `Area111_TailGate` | sub-kind 1 | Area111_TailGate 203 |
| C71 | `Area111_TailGate` | TestFB (0x11, 0xB6) | Area111_TailGate 367 |
| C72 | `Area111_TailGate` | state 0x16 for 0x15 | Area111_TailGate 367 |
| C73 | `Area111_TailGate` | X 0x118001 | Area111_TailGate 217 |
| C74 | `Area111_TailGate` | Z 0xAE0000 | Area111_TailGate 217 |
| C75 | `Area111_TailGate` | byte 0xA0 | Area111_TailGate 429 |
| C76 | `Area111_TailGate` | flag 0x29 | Area111_TailGate 429 |
| C77 | `Area111_TailGate` | state 0x19 | Area111_TailGate 177 |
| C78 | `Area111_TailGate` | timer 2 | Area111_TailGate 237 |
| C79 | `Area111_TailGate` | 0x17 divisor from speed 3 | Area111_TailGate 177 |
| C80 | `Area111_TailGate` | 0x16 hold inverted | Area111_TailGate 1156 |
| C81 | `Area111_TailGate` | 0x18 hold not tested | Area111_TailGate 506 |
| C82 | `Area111_TailGate` | 0x15 FAWord 1 | Area111_TailGate 217 |
| C83 | `Area111_ArriveHook` | clears 0x29 | Area111_ArriveHook 715 |
| C84 | `Area111_ArriveHook` | Cond_ByteFD 2 | Area111_ArriveHook 729 |
| C85 | `Area111_ArriveHook` | x >> 16 | Area111_ArriveHook 392 |
| C86 | `Area111_ArriveHook` | edge 0x16 arms 4 | Area111_ArriveHook 57 |
| C87 | `Area111_ArriveHook` | edge 0x30 span 0x1A | Area111_ArriveHook 1 |
| C88 | `Area111_ArriveHook` | edge 0x162 by the x step | Area111_ArriveHook 54 |
| C89 | `Area111_ArriveHook` | edge 0x17C arms 2 | Area111_ArriveHook 37 |
| C90 | `Area111_ArriveHook` | gate 0x20 span 2 | Area111_ArriveHook 12 |
| C91 | `Area111_ArriveHook` | gate 0x26 from 0x27 | Area111_ArriveHook 85 |
| C92 | `Area111_ArriveHook` | gate 0x16C from x 0x23 | Area111_ArriveHook 46 |
| C93 | `Area111_ArriveHook` | gate 0x172 arms 7 | Area111_ArriveHook 50 |
| C94 | `Area111_ArriveHook` | final z from 0xB6 | Area111_ArriveHook 250 |
| C95 | `Area111_ArriveHook` | final state 0x15 | Area111_ArriveHook 57 |
| C96 | `Area111_ArriveHook` | edges from + step | Area111_ArriveHook 712 |
| C97 | `Area111_ArriveHook` | span inclusive | Area111_ArriveHook 93 |
| C98 | `Area111_ArriveHook` | answers 1 | Area111_ArriveHook 1922 |
| C99 | `Area111_ArriveHook` | direction & 3 | Area111_ArriveHook 346 |
| C100 | `Area111_ArmTailAtLeaderCell` | sub-kind block + 1 | Area111_ArmTailAtLeaderCell 8000 |
| C101 | `Area111_ArmTailAtLeaderCell` | kind 0x12 | Area111_ArmTailAtLeaderCell 8000 |
| C102 | `Area111_ArmTailAtLeaderCell` | extra +9 | Area111_ArmTailAtLeaderCell 8000 |
| C103 | `Area111_ArmTailAtLeaderCell` | state + 1 | Area111_ArmTailAtLeaderCell 7963 |
| C104 | `Area111_MarkCells` | x1 + 0xD | Area111_MarkCells 8000 |
| C105 | `Area111_MarkCells` | z0 + 0xB0 | Area111_MarkCells 8000 |
| C106 | `Area111_MarkCells` | (x1, z1) twice | Area111_MarkCells 8000 |
| C107 | `Area111_MarkCells` | z1 + 0xB3 | Area111_MarkCells 8000 |
| C108 | `Area111_Init` | clears 0x27 | Area111_Init 903 |
| C109 | `Area111_Init` | Cond_ByteFD 2 | Area111_Init 2642 |
| C110 | `Area111_Init` | rows and columns swapped | Area111_Init 1750 |
| C111 | `Area111_Init` | 27 grid bytes | Area111_Init 1737 |
| C112 | `Area111_Init` | gate 0x51 | Area111_Init 606 |
| C113 | `Area111_Init` | gate low 1 | Area111_Init 606 |
| C114 | `Area111_Init` | open gate 0xC1 | Area111_Init 1144 |
| C115 | `Area111_Init` | Cond_ByteFD 4 | Area111_Init 933 |
| C116 | `Area111_Init` | (0xB, 8) 0x51 | Area111_Init 925 |
| C117 | `Area111_Init` | flag inverted | Area111_Init 1750 |
| D1 | `Area112_ChangeAreaByFlags` | flag 0x36 | Area112_ChangeAreaByFlags 8000 |
| D2 | `Area112_ChangeAreaByFlags` | 1: flags 0x80 | Area112_ChangeAreaByFlags 1730 |
| D3 | `Area112_ChangeAreaByFlags` | 2: area 0x93 | Area112_ChangeAreaByFlags 1783 |
| D4 | `Area112_ChangeAreaByFlags` | 3: x + 1 | Area112_ChangeAreaByFlags 3557 |
| D5 | `Area112_ChangeAreaByFlags` | 0: flags 0x83 | Area112_ChangeAreaByFlags 930 |
| D6 | `Area112_ChangeAreaByFlags` | high 3 | Area112_ChangeAreaByFlags 1783 |
| D7 | `Area112_TalkByMember` | member +0x88 | Area112_TalkByMember 4224 |
| D8 | `Area112_TalkByMember` | the next message | Area112_TalkByMember 4206 |
| D9 | `Area112_TalkByMember` | request 3 | Area112_TalkByMember 4206 |
| D10 | `Area112_TalkByMember` | three keys | Area112_TalkByMember 852 |
| D11 | `Area112_TalkByMember` | one member fewer | Area112_TalkByMember 2180 |
| D12 | `Area112_ChoiceFlags34` | answer unsigned | Area112_ChoiceFlags34 1605 |
| D13 | `Area112_ChoiceFlags34` | flag 0x32 | Area112_ChoiceFlags34 8000 |
| D14 | `Area112_ChoiceFlags34` | answer not read again | Area112_ChoiceFlags34 2352 |
| D15 | `Area112_ChoiceFlags34` | 1: sets 0x36 | Area112_ChoiceFlags34 676 |
| D16 | `Area112_ChoiceFlags34` | 2: clears 0x36 | Area112_ChoiceFlags34 684 |
| D17 | `Area112_ChoiceFlags34` | 4: 0xFFFE | Area112_ChoiceFlags34 472 |
| D18 | `Area112_ChoiceFlags34` | row 13 flag 0x20 | Area112_ChoiceFlags34 8000 |
| D19 | `Area112_ChoiceFlags34` | message 3 | Area112_ChoiceFlags34 2510 |
| D20 | `Area112_ChoiceFlags34` | other: 0x35 kept | Area112_ChoiceFlags34 5160 |
| D21 | `Area112_ChoiceFlags34` | answer 5 for 3 | Area112_ChoiceFlags34 1400 |
| D22 | `Area112_ChoiceMessage13` | message 0x15 | Area112_ChoiceMessage13 7412 |
| D23 | `Area112_ChoiceClearFlags` | 0x3F kept | Area112_ChoiceClearFlags 583 |
| D24 | `Area112_ChoiceClearFlags` | 0x38 kept | Area112_ChoiceClearFlags 583 |
| D25 | `Area112_ChoiceClearFlags` | sound 0x201 | Area112_ChoiceClearFlags 583 |
| D26 | `Area112_ChoiceClearFlags` | message 0xFFFE | Area112_ChoiceClearFlags 7828 |
| D27 | `Area112_StepHook` | Cond_ByteFD 2 | Area112_StepHook 3576 |
| D28 | `Area112_StepHook` | flag 0x34 | Area112_StepHook 2661 |
| D29 | `Area112_StepHook` | row 14 | Area112_StepHook 1781 |
| D30 | `Area112_StepHook` | x two wide | Area112_StepHook 20 |
| D31 | `Area112_StepHook` | z from 0x58 | Area112_StepHook 59 |
| D32 | `Area112_StepHook` | pose 5 for 6 | Area112_StepHook 47 |
| D33 | `Area112_StepHook` | counter 0 = 1 | Area112_StepHook 86 |
| D34 | `Area112_StepHook` | answers 2 | Area112_StepHook 86 |
| D35 | `Area112_StepHook` | x >> 15 | Area112_StepHook 86 |
| D36 | `Area112_CellHook` | pose 2 blocks | Area112_CellHook 652 |
| D37 | `Area112_CellHook` | z not compared | Area112_CellHook 1022 |
| D38 | `Area112_CellHook` | none answers 3 | Area112_CellHook 4950 |
| D39 | `Area112_CellHook` | flag 0x1D | Area112_CellHook 3050 |
| D40 | `Area112_CellHook` | entry flag the bits byte | Area112_CellHook 1023 |
| D41 | `Area112_CellHook` | sub-kind 0x16 | Area112_CellHook 242 |
| D42 | `Area112_CellHook` | near answers 1 | Area112_CellHook 242 |
| D43 | `Area112_CellHook` | bits inverted | Area112_CellHook 105 |
| D44 | `Area112_CellHook` | flags by bit & 1 | Area112_CellHook 79 |
| D45 | `Area112_CellHook` | bit 4 clears | Area112_CellHook 58 |
| D46 | `Area112_CellHook` | sets the bits byte | Area112_CellHook 105 |
| D47 | `Area112_CellHook` | hold 0x10 | Area112_CellHook 105 |
| D48 | `Area112_CellHook` | answers 0 | Area112_CellHook 105 |
| D49 | `Area112_CellHook` | state 0xB | Area112_CellHook 242 |
| D50 | `Area112_MemberNearBoxes` | frames + 1 | Area112_MemberNearBoxes 565 |
| D51 | `Area112_MemberNearBoxes` | z >> 15 | Area112_MemberNearBoxes 1198 |
| D52 | `Area112_MemberNearBoxes` | x inclusive | Area112_MemberNearBoxes 995 |
| D53 | `Area112_MemberNearBoxes` | z by the width | Area112_MemberNearBoxes 2834 |
| D54 | `Area112_MemberNearBoxes` | three boxes | Area112_MemberNearBoxes 262 |
| D55 | `Area112_MemberNearBoxes` | from member 1 | Area112_MemberNearBoxes 558 |
| D56 | `Area112_MemberNearBoxes` | z step the x step | Area112_MemberNearBoxes 651 |
| D57 | `Area112_MemberNearBoxes` | absolute value off by one | Area112_MemberNearBoxes 1353 |
| D58 | `Area112_EffectRun` | state ^ 1 | Area112_EffectRun 8000 |
| D59 | `Area112_EffectRing` | y from +0x40 | Area112_EffectRing 8000 |
| D60 | `Area112_EffectRing` | x from +0x30 | Area112_EffectRing 8000 |
| E1 | `Area113_ChoiceAsk` | answer message 5 | Area113_ChoiceAsk 7466 |
| E2 | `Area113_ChoiceAsk` | byte at 7 | Area113_ChoiceAsk 75 |
| E3 | `Area113_ChoiceAsk` | mark 7 | Area113_ChoiceAsk 115 |
| E4 | `Area113_ChoiceAsk` | answer above 1 | Area113_ChoiceAsk 546 |
| E5 | `Area113_Trigger27` | sub-kind 7 | Area113_Trigger27 8000 |
| E6 | `Area113_Trigger27` | kind 5 | Area113_Trigger27 8000 |
| E7 | `Area113_Trigger27` | answers 1 | Area113_Trigger27 8000 |
| E8 | `Area113_Trigger39` | kind 0x2D | Area113_Trigger39 8000 |
| E9 | `Area113_Trigger39` | sub-kind 0xC | Area113_Trigger39 8000 |
| E10 | `Area113_Trigger39` | state 1 | Area113_Trigger39 8000 |
| E11 | `Area113_Trigger63` | kind 0x3F | Area113_Trigger63 8000 |
| E12 | `Area113_Trigger63` | +0x88 6 | Area113_Trigger63 8000 |
| E13 | `Area113_Trigger63` | answers 1 | Area113_Trigger63 8000 |
| E14 | `Area113_Trigger63` | state 2 | Area113_Trigger63 8000 |
| E15 | `Area113_TailReward` | state answer + 1 | Area113_TailReward 1000 |
| E16 | `Area113_TailReward` | 1: request 3 | Area113_TailReward 246 |
| E17 | `Area113_TailReward` | sound 0x107 | Area113_TailReward 806 |
| E18 | `Area113_TailReward` | state 9 for 0xA | Area113_TailReward 1207 |
| E19 | `Area113_TailReward` | stream 3 | Area113_TailReward 827 |
| E20 | `Area113_TailReward` | state 0xC | Area113_TailReward 827 |
| E21 | `Area113_TailReward` | stream test inverted | Area113_TailReward 763 |
| E22 | `Area113_TailReward` | +0x88 8 | Area113_TailReward 646 |
| E23 | `Area113_TailReward` | focus read before Clear40 | Area113_TailReward 309 |
| E24 | `Area113_TailReward` | 5: kind 1 | Area113_TailReward 806 |
| E25 | `Area113_TailReward` | no ResumeAll | Area113_TailReward 646 |
| E26 | `Area113_TailReward` | no StopMusic | Area113_TailReward 827 |
| E27 | `Area113_Reward` | case 0xD for 0xA | Area113_Reward 1779 |
| E28 | `Area113_Reward` | count 2 | Area113_Reward 388 |
| E29 | `Area113_Reward` | answer 6 | Area113_Reward 388 |
| E30 | `Area113_Reward` | message 7 for 6 | Area113_Reward 256 |
| E31 | `Area113_Reward` | message 2 for 3 | Area113_Reward 146 |
| E32 | `Area113_Reward` | key item 0xE | Area113_Reward 1239 |
| E33 | `Area113_Reward` | adds 0x10 | Area113_Reward 415 |
| E34 | `Area113_Reward` | answer 0xB | Area113_Reward 415 |
| E35 | `Area113_Reward` | message 6 for 7 | Area113_Reward 97 |
| E36 | `Area113_Reward` | message 5 for 4 | Area113_Reward 55 |
| E37 | `Area113_Reward` | request 1 | Area113_Reward 8000 |
| E38 | `Area113_Reward` | default message 0 | Area113_Reward 4313 |
| E39 | `Area113_Reward` | 0xB tests 0x96 first | Area113_Reward 1217 |
| E40 | `Area113_Reward` | byte & 0xFE | Area113_Reward 1765 |
| C18b | `Area111_SlideMark` | z stepped back (the z step negated) | Area111_SlideMark 4589 |
| C51b | `Area111_SlideAhead` | row stepped back (the z step negated) | Area111_SlideAhead 3761 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (25): `Area108_Choices`
`0x61E54C` (19), `Area108_Handlers` `0x61E558` (16), `Area108_Model`
`0x61E508` (8 bytes), `Area108_Places` `0x61E5DC` (15 bytes),
`Area108_PlaceCellsA` `0x61E5EC` (36), `Area108_PlaceCellsB` `0x61E610`
(36), `Area108_FadeStates` `0x61E634` (2), `Area108_HookCells` `0x61E63C`
(16 bytes), `Area108_CellWords` `0x61E64C` (8 words), `Area110_Cells`
`0x61E7F0` (16), `Area110_Weights` `0x61E800` (8), `Area111_Choices`
`0x61FB08` (11), `Area111_Handlers` `0x61FB10` (9), `Area111_CellBytes`
`0x61FB7C` (49), `Area111_GridStart` `0x61FBB0` (28), `Area111_Grid`
`0x675C00` (28), `Area112_Handlers` `0x61FF14` (6), `Area112_Choices`
`0x61FF20` (3), `Area112_MemberKeys` `0x61FF74` (4), `Area112_Messages`
`0x61FF78` (4 words), `Area112_HookCells` `0x61FF80` (16),
`Area112_HookFlags` `0x61FFA0` (4), `Area112_Boxes` `0x61FFA4` (16),
`Area112_EffectStates` `0x61FFB4` (2), `Area113_Choices` `0x61FFF8` (5). As
in the earlier areas, a descriptor's `+0x3C` array is the tail of its
`+0x34` array (108, 111) or the reverse (112). The five in-function tables
(`0x416D4C` 5, `0x4174DC` 10, `0x417E98` 8 with its byte table `0x417EB8`
24, `0x418558` 4, `0x418A1C` 6 with `0x418A34` 12) are in `.text`, inside
their functions' extents.

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area108_FadeRun` jumps through a two-entry
  table by `Sprite_Current[4]`, `Area112_EffectRun` by the effect record's
  `+1`; an index of 2 or more jumps through the bytes after the table. Ours
  aborts with a message there (the owner's rule: no DIVERGENCE entry,
  round9 doc section 6). The band's own states store only 0 and 1.
- **`Area111_PlaceMemberInGrid` writes by an unchecked cell.** The key is
  or'd into the byte `0x675C00 + x / 2 + z * 4` for the running object's
  cell, which nothing bounds: an object off the grid writes the bytes
  around it (area 112's and 113's descriptor records follow at
  `0x675C20`). Ours aborts where the byte would lie outside the 28-byte
  grid.
- **`Area111_StepToExtra` indexes the extra objects by a dword** (the
  running object's `+0x18`), reading `0x802034 + k * 0xA4` for any k; ours
  aborts where that record would lie outside the image's `.data` (the
  original would fault or read the heap). Its sibling handler 6 reads the
  same records by a byte (inside `.data`, kept).
- **`Area111_ArmTailAtLeaderCell` writes by the nibble.** A leader cell of
  nibble 0 makes the block `0xFF`, so the sub-kind is `0xFF` and extra
  record `0xFF`'s `+8` (`0x80C364`) is written; a wall (`0xF`) writes record
  14's (`0x8028F0`). Both lie inside `.data`: kept. Not measured in play.
- **`Area111_SlideMark` marks the cell it stopped at**, which is outside
  the grid or a blocked cell - its two map calls mark the start cell `0x10`
  and the stop cell 0, never the cell the object ends in. What the script
  does next is not read.
- **Unchecked reads that stay in `.data`, kept:** the grid read at the
  leader's cell as bytes (`Area111_AttachAtLeaderCell`,
  `Area111_ArmTailAtLeaderCell`: up to `0x675C00 + 0x7F + 0x3FC`); the place
  tables by extra object 1's `+0x83 - 1` (`Area108_PlaceAtCellA` / `B`; a 0
  reads entry `0xFF`); `MoveScript_EffectState` (24 bytes) by the leader's
  `+0x89` byte (`Area108_FlagIfEffectState5`); party records past the
  member count's three (`Area108_PlaceScene`, `Area112_TalkByMember`,
  `Area112_MemberNearBoxes`; 1..3 in the field).
- **`Area108_TailPlace` writes the area's own `.data`**: `Area108_Model`'s
  flags byte and its 33 records' corners are moved by the scene. On the PSX
  the overlay is reloaded with the area; here the image keeps the moved
  points until something rewrites them, so a second visit would start from
  the last scene's positions. Not measured in play.
- **`Area108_PlaceScene`'s drop-in mask** has no bit past the eighth
  member (an 8-bit shift), and its argument's upper bytes are the stack;
  `Party_DropIn` reads the byte (1..3 members in the field).
- **Effect slots unchecked.** Every spawn writes the record at
  `Effect_Objects + slot << 7` for any slot but `0xFF`; `Effect_FindFree`
  answers only 0..19 or `0xFF`, so nothing reaches past.
- **`Area111_ArriveHook` answers 0 on every path**, even when it arms the
  tail; what `Area_ArriveHook`'s caller does with the answer is not read
  here.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count), D138 (the bare-`ret` inits),
D145 (`.data` kept across visits), D147 (uninitialised bytes), D153 (hooks
without the script flag), D161 (small slips) in
[`known-defects.md`](known-defects.md).

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 53 starts, extents, call sites,
  the five in-function jump tables, and the shape its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding (`nop` runs, read byte by byte).
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x4188F0`, `0x418910`, `0x418930` (object triggers 27, 39, 63),
  `0x418960` (tail kind 62, armed by an immediate in `Area113_Trigger63`),
  `0x418A40` (called only by that tail), `0x418860`
  (`Effect_KindHandlers[0xB2]`). Each sits in the block of the area named;
  the tool's rows put them under AREA113 and AREA112.
- **The tool's register-armed tail kinds:** none in this band; every tail
  kind armed here (4, 5, `0xA`, `0xC`, `0x11`, `0x2C`, `0x3E`) is an
  immediate store.
- **`0x417510`, `0x418130`, `0x4181E0`, `0x4187C0`** have no root of their
  own: only their areas' functions call them.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 53). Every function is reached only in play: the choices
  when the area's message box asks, the handlers from its movement scripts,
  the hooks on a step, an arrival or a talk at a cell in the area, the
  tails once armed, the triggers by an object's trigger id, the inits on
  entry, area 112's effect states while an effect of kind `0xB2` lives.

## 9. Calls across groups

- **Raw address nobody owns**, in `area_w2f_callees.h`: `0x4220D0` (world
  3's area code, group AR3F's band this wave). Not calls but table entries
  of other bands: area 112's handler 1 `0x421FB0` and area 113's choices 1,
  2 `0x420850` / `0x420870` (AR3F's band), read in place by the engine.
- **By name, Capcom's:** `Rand`, `Sound_StopMusic`, `Sound_ResumeAll`.
- **By name, ours:** `AreaMap_SetByte`, `AreaMap_SetHeight`,
  `AreaMap_Elevation`, `MapView_SetElevation`, `Effect_FindFree`,
  `Effect_HoldFlag1C`, `Party_DropIn`, `Sound_PlayEffect`,
  `Sound_LoadStream`, `Sound_StreamDone`, `Sprite_SetTint`, `Tint_Release`,
  `ScriptFlags_Set40` / `Clear40`, `Flags_Set` / `Clear` / `Test` /
  `Toggle`, `KeyItem_Has`, `KeyItem_Add`, `Inventory_Add`, `Kind2_Place`,
  `MoveCmd_TestFB`, `MoveCmd_Attach`, `MoveCmd_MoveKind2`,
  `Msg_OpenScript`, `Field_ChangeArea`. No harness edit; no `AH_THEIRS`
  moved.
- **Shared bodies already ours, named by these areas' tables** (read in
  place, not taken again): `Area22_ArmTailOnYes`, `Area67_SetBit24`,
  `Area57_StopMusic`, `Area57_ResumeSound` (area 108), `Area61_ChoiceMark4`
  (area 113), `Area59_EffectGround` (`Area112_EffectStates[0]`).
- **Inbound** (for the rebinding pass): `Area_StepHook` (ours in
  `event_ops.cpp`) calls `0x418620` through its raw `kStepHandlers` table
  (Capcom's copy at `0x56E142`), now `Area112_StepHook`; `Area_ArriveHook`
  calls `0x417ED0` through `kArriveHandlers` (Capcom's at `0x56E53B`), now
  `Area111_ArriveHook`. By engine tables: `Field_ModeTailKinds` 12, 17, 62;
  `Field_ObjectTriggers` 27, 39, 63; `Area_CellHooks` (areas `0x6C`,
  `0x70`); `Effect_KindHandlers[0xB2]`.
- `analysis/calltrace/entries_logic.txt`: 52 lines appended under a
  `# group AR2F` comment (the 53rd, `00418130 AC`, was already there
  exactly); the other host lines that ran over the band (`00417510 9C0`,
  `004181E0 431`, `00418620 199`, `004187C0 280`) share a start with one of
  ours and are longer (the consolidation keeps the smaller).
- **`inject_all.cpp`:** `AreaW2f_Inject()` before `DrawPool_Grow()`. At
  `2c9c92f` wave four's `AreaW1f_Inject()` sits *after* `DrawPool_Grow()`
  (the AR1F merge appended it there); left for the coordinator.
