# World 1, areas 53, 55..57 and 59..64: the band `0x40AB00..0x40B8C0`

**Status:** IN PROGRESS (2026-09-28) - 47 functions ours
(`src/game/area_w1d.cpp`, shadow name `area_w1d`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 282,000 rounds (in this worktree); 136 controls planted, 135 refused by a count, 1 equivalent with its near variants refused
(section 4). Fuzz only: no recorded route reaches the band (section 8). No
divergence; the two state dispatchers abort past their tables (section 6).

Group AR1D of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
47 starts past `0x40AB00` (the tool's `AR1A` row at this tip spans areas
38..64; 38..52 were ours already), none ours before, **47 taken**; no start
dropped, none added (section 7). Areas 54 and 58 have no code in the band
(their descriptors' `+0x34`, `+0x3C` and `+0x40` are 0).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s
rows, each read against the disassembly. What an area *is* in the story is
not read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, hooks, tails, triggers and
effect states have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a step hook
`(x, z)` answering in `al`, `kState` an entry of a state table the area's
own dispatcher jumps through, `kCallee` an object trigger `(object,
0x904030)` answering in `al` (`Field_ObjectTriggers` ids are 1-based: id N
is the dword at `0x662E1C + 4N`). Area 57's choice 0 is also its handler 4
and area 56's choice 15 its handler 0; each is fuzzed by the shape whose
read-after matters (the choice writes the message word; the dispatcher does
not).

The pointer `0x903804` (the "focus object": `area_w1a`'s name, the active
member of the last talk in `object_kinds.cpp`) is read by area 53's choice
and area 59's trigger 24; the mark `0x9398CF = 6` is set beside a message
by areas 55, 59 and 61's choices, as areas 37 and 50's.

### Area 53 (descriptor `0x5FEA98`; PSX `0x801F35F8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40AB00` | `Area53_ChoiceFocusPair` | `0x3C` | choice 0 | kChoice | message 0xFFFF; the focus object's dwords `+0x18` / `+0x1C` = `Area53_ChoicePairs[s8 answer]` bytes 0 / 1 (pointer and answer read again for the second) |
| `0x40AB40` | `Area53_Trigger42` | `0x1B` | object trigger 42 (in 53's block by address) | kCallee | `ScriptFlags_Set40`; tail kind `0x2C` (engine code `0x56DE50`), state 0, sub-kind `0xA`; **no `al` of its own**: eax is `ScriptFlags_Set40`'s, passed on |

Choice 1, handler 0 and the init are `0x437CC0`, a shared `ret` of another
block. The PSX overlay has an init (`0x801F2C5C`) and a handler
(`0x801F2C64`) the PC port did not keep.

### Area 55 (descriptor `0x6007D8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40AB60` | `Area55_ChoiceTakeItem8` | `0x58` | choice 0 | kChoice | answer 0: `Inventory_Count(0, 8, 0)`'s word not 0: `Inventory_Remove(0, 8, 1)` (a fourth push of 0), message `0x22`; 0: message `0x23` and the mark; another answer: message `0x24` and the mark |
| `0x40ABC0` | `Area55_ChoiceMark24` | `0x24` | choices 3, 4 | kChoice | an answer: message `0x24` and the mark; 0: message 0xFFFF |
| `0x40ABF0` | `Area55_Trigger23` | `0x16` | object trigger 23 | kCallee | `ScriptFlags_Set40`, tail kind 4 (engine `0x56D930`) with sub-kind 2; al 0 |

Choices 1 and 2 are `0x420850` / `0x420870` (another block's; areas 59 and
61 name them too). No handler, no init.

### Area 56 (descriptor `0x600BA0`; PSX `0x801F3854`)

"Pose 2/7" below is: the leader's record `+1 = 2`, `+2 = 7`, `+3 = 0`, then
`MoveCmd_TestFB(the leader's words +0x36, +0x3A)` (the high words of x and
z, read before the stores; the answer is not read).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40AC10` | `Area56_ChoicePoseFlag8` | `0x6B` | choice 0 | kChoice | message 0xFFFF; answer 0: pose 2/7; answer (read again) 1: `Flags_Set(Cond_Flags row 3, 8)`; answer (read again) not 2: sound `0x208` |
| `0x40AC80` | `Area56_ChoicePoseFlag9` | `0x6B` | choice 1 | kChoice | the same, flag 9 |
| `0x40ACF0` | `Area56_ChoicePoseFlagA` | `0x6B` | choice 2 | kChoice | flag `0xA` |
| `0x40AD60` | `Area56_ChoicePoseFlagB` | `0x6B` | choice 3 | kChoice | flag `0xB`, **the other way round**: pose on 1, flag on 0 |
| `0x40ADD0` | `Area56_ChoicePoseFlagC` | `0x6B` | choice 4 | kChoice | flag `0xC` |
| `0x40AE40` | `Area56_ChoicePoseFlagD` | `0x6B` | choice 5 | kChoice | flag `0xD`, the other way round |
| `0x40AEB0` | `Area56_ChoicePoseFlagE` | `0x6B` | choice 6 | kChoice | flag `0xE` |
| `0x40AF20` | `Area56_ChoiceMessage9` | `0x18` | choice 7 | kChoice | message 9 on answer 0, else 0xFFFF (`neg` / `sbb` / `and` / `add`) |
| `0x40AF40` | `Area56_ChoiceMessageA` | `0x18` | choice 8 | kChoice | `0xA` on 0 |
| `0x40AF60` | `Area56_ChoiceMessageB` | `0x18` | choice 9 | kChoice | `0xB` on 0 |
| `0x40AF80` | `Area56_ChoiceMessageC` | `0x1A` | choice 10 | kChoice | `0xC` on 2 |
| `0x40AFA0` | `Area56_ChoiceMessageD` | `0x1A` | choice 11 | kChoice | `0xD` on 1 |
| `0x40AFC0` | `Area56_ChoiceMessageE` | `0x1A` | choice 12 | kChoice | `0xE` on 1 |
| `0x40AFE0` | `Area56_ChoicePoseOn1` | `0x3F` | choice 13 | kChoice | message 0xFFFF; answer 1: pose 2/7 |
| `0x40B020` | `Area56_ChoicePoseIfAllFlags` | `0x58` | choice 14 | kChoice | message 0xFFFF; answer 0: sound `0x208`, then with row 3's flags 8..0xE all set (`0x903FA9 & 0x7F`, read after the sound) pose 2/7 |
| `0x40B080` | `Area56_FallRun` | `0x12` | choice 15 = handler 0 (PSX `0x801F3284`) | kHandler | `jmp [Sprite_Current[4] * 4 + Area56_FallStates]` |
| `0x40B0A0` | `Area56_FallBegin` | `0x72` | `Area56_FallStates[0]` | kState | the object's `+0` bit `0x40` cleared; height `+0x3E` = `MapView_GroundAt(x, z) + 0x7D0`; `+0xC`, `+0x10`, `+0x14` 0; step `+0x20` -8; state 1; `Field_State` word `+0x12E` - 2 |
| `0x40B120` | `Area56_FallStep` | `0x90` | `Area56_FallStates[1]` | kState | `+0x14 += +0x20`; `Field_LeaderStepTick`; the ground above the height (signed words): height = ground, `+0x14` and `+0x20` 0, state 2; then with `+5` 0 and bit 3 of neither flag word's low byte, `MapView_SetElevation(height)`; `Field_State +0x12E` - 2 |
| `0x40B1B0` | `Area56_FallEnd` | `0x13` | `Area56_FallStates[2]` | kState | `Field_ScriptFlags2` bit `0x200` cleared, state 0 |
| `0x40B1D0` | `Area56_Trigger52` | `0x40` | object trigger 52 (in 56's block) | kCallee | story flag `0x6E` set; flags `0x6D..0x70` all set: flag `0x71` too; al 0 (area 34's trigger 51 with `0x6E`) |

Area 56's init is `0x437CC0` (the PSX has one, `0x801F3464`). The fall is
a gravity step: the height starts `0x7D0` above the ground and the rise
`+0x14` falls by 8 a frame until the ground meets the height.

### Area 57 (descriptor `0x602928`; PSX `0x801F4AB8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40B210` | `Area57_ChoiceMessage` | `0x17` | choice 0 = handler 4 (PSX `0x801F2C04`) | kChoice | message = `Area57_ChoiceMessages[s8 answer]` |
| `0x40B230` | `Area57_SpawnKind2AtLeader` | `0x42` | handler 0 (PSX `0x801F2C30`) | kHandler | `Sprite_Current` = the leader's record (left so); `Effect_Spawn(2, 0, Area57_EffectKinds2[0x904062], leader +0x2E, +0x30)`; not 0xFF: to `Sprite_Current[0xB]` (read again) |
| `0x40B280` | `Area57_SpawnKind4AtLeader` | `0x42` | handler 1 (PSX `0x801F2CB0`) | kHandler | kind 4, `Area57_EffectKinds4` |
| `0x40B2D0` | `Area57_StopMusic` | `0x5` | handler 2 (PSX `0x801F2D30`); also area 39 h9, area 108 c16 / h13, area 145 c10 / h9 | kHandler | `jmp Sound_StopMusic` |
| `0x40B2E0` | `Area57_ResumeSound` | `0x5` | handler 3 (PSX `0x801F2D58`); also area 108 c17 / h14, area 145 c11 / h10 | kHandler | `jmp Sound_ResumeAll` |
| `0x40B2F0` | `Area57_Trigger53` | `0x40` | object trigger 53 (in 57's block) | kCallee | as `Area56_Trigger52` with flag `0x6F` first |

The two jump thunks are **shared bodies**: area 57's block holds them and
areas 39, 108 and 145's tables name them (the tool's `shared` rows); taken
once, here, keyed by address. On the PSX each area has its own
(`0x801F2D30` / `0x801F2D58` are area 57's).

### Area 59 (descriptor `0x603C98`; PSX `0x801F37E0`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40B330` | `Area59_ChoiceMessage2or4` | `0x24` | choice 0 | kChoice | answer 0: message 2; else message 4 and the mark |
| `0x40B360` | `Area59_ClearCells` | `0x46` | handler 2 (PSX `0x801F2D90`) | kHandler | `AreaMap_SetByte` cells x `0x47..0x49` of rows 8 and 9 to 0 |
| `0x40B3B0` | `Area59_SetCells` | `0x58` | handler 3 (PSX `0x801F2E08`) | kHandler | the same cells, row 8 `0xC0`, row 9 `0xA1` |
| `0x40B410` | `Area59_StepHook` | `0x5E` | `Area_StepHook`'s case for area 59 | kHook | `Cond_ByteFD` 2, story flag `0x33`, the step's high words x `0x46..0x48`, z `0x29..0x2B`, the leader's pose 0, 7 or 6: counter 0 = 0, `Party_DropIn(0)`, al 1; else al 0 - `Area36_StepHook` with z `0x29` for `0x23` |
| `0x40B470` | `Area59_Trigger24` | `0x5D` | object trigger 24 | kCallee | `ScriptFlags_Set40`; the focus object (read afresh each time) `+1 = 4`, `+0x83 = 0`, word `+0x8A = 0`; tail kind 4 with sub-kind 5 and counter 3 = the focus object's index `(pointer - Sprite_Objects) / 0xA4` (signed, truncated toward 0; the low byte); al 0 |
| `0x40B4D0` | `Area59_EffectRun` | `0x12` | `Effect_KindHandlers` entry `0xB6` (the dword `0x655628`; in 59's block) | kState | `jmp [Sprite_Current[1] * 4 + Area59_EffectStates]` |
| `0x40B4F0` | `Area59_EffectGround` | `0x2D` | `Area59_EffectStates[0]`; also entry 0 of areas 36, 100, 112, 116, 143, 146's effect tables | kState | `+0x3C` = `(s16)AreaMap_Elevation(x, z) << 16`; `+1` one on (`Sprite_Current` read again after the call) |
| `0x40B520` | `Area59_EffectStep` | `0x2B` | `Area59_EffectStates[1]` | kState | x, z, y dwords to a 16-byte stack frame, `0x4220D0(&frame)` - `Area36_EffectStep` instruction for instruction |

Handlers 0 and 1 are `0x419D80` / `0x421FB0`, choices 1, 2 `0x420850` /
`0x420870` (other blocks'); choices 3 and 4 are `Area61_ChoiceMark4`.
Area 59 is area 36's effect and step hook again, with its own z band: the
seventh area to name `0x40B4F0`.

### Areas 60, 61, 62 (descriptors `0x603DE0`, `0x6040B8`, `0x6041E0`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40B550` | `Area60_InitScriptFlag2000` | `0x8` | area 60 init (PSX `0x801F3310`) | kInit | `0x9039A3 |= 0x20`: `Field_ScriptFlags` bit `0x2000` |
| `0x40B560` | `Area61_ChoiceClearZenny` | `0x2E` | area 61 choice 0 | kChoice | answer 0: `Party_Zenny` (the u32 `0x904058`) 0 and message 2; else message 3 and the mark |
| `0x40B590` | `Area61_ChoiceMark4` | `0x24` | area 61 choices 3, 4; also areas 59, 113, 116's choices 3, 4 | kChoice | an answer: message 4 and the mark; 0: message 0xFFFF |
| `0x40B5C0` | `Area61_Trigger21` | `0x16` | object trigger 21 | kCallee | `ScriptFlags_Set40`, tail kind 4 with sub-kind 1; al 0 |
| `0x40B5E0` | `Area62_ArmTailGiveItem` | `0x3D` | area 62 handler 0 (PSX `0x801F2C04`) | kHandler | the active member's `+0x80` bit 0 cleared; the leader's `+0x89` at 6: `ScriptFlags_Set40`, tail kind `0x2A` at state 0, the active member's (read again) word `+0x8A` + 1 |
| `0x40B620` | `Area62_TailGiveItem59` | `0xF3` | tail kind `0x2A` | kTail | by the s8 state (a `sub` / `dec` chain, no table): 0 counter 0 = `0xA`, sound `0x10D`, state 1; 1 once counter 0 is `0xB`: item (0, `0x59`)'s 16-byte name record (`Item_NamePtr`) to `Text_Records`, `Inventory_Add(0, 0x59, 1)` - added: sound `0x106`, `Msg_OpenSystem(2)`, story flag `0x68`; not: `Msg_OpenSystem(3)` - `Field_Request` 2, state 2; 2 once `Field_Request` leaves 2: `ScriptFlags_Clear40`, counter 0, kind and state 0 |

### Areas 63 and 64 (descriptors `0x6042B0`, `0x604380`; one PSX descriptor address `0x801F2DD4`, two overlays)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40B720` | `Area63_InitPlaceObject` | `0xCB` | area 63 init (PSX `0x801F2D2C`) | kInit | a 5-byte `jmp` over eleven `nop`s (as shipped); `Rand & 0x3F` walked through `Area63_Weights` (the first weight above what is left; none: 8); byte `+0` of field objects 0..7 but that one cleared; the one (below 8) put at `Area63_Positions[Rand & 7]` (bytes << 16), `+0x3E` the elevation there; `Field_EdgeBits` = the leader's dword `+0x134` - 5 |
| `0x40B7F0` | `Area64_InitPlaceObject` | `0xCB` | area 64 init (PSX `0x801F2D2C`) | kInit | the same over area 64's tables (the same bytes) |

The weights are 8 bytes summing to 64, so with the image's tables one of
the eight objects is always kept; the Japanese PSX overlay `AREA063.EMI`
(`analysis/emi-jp/`) holds the same 24 bytes in the same order (the
positions, then the weights). The fuzz draws other weights (section 3), so
the "none" path is exercised though the data never takes it.

## 2. Ours

`src/game/area_w1d.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for
`0x4220D0`). Shapes that repeat are one helper with the function's
constants: area 56's seven pose-and-flag choices (`ChoicePoseFlag`) and
their pose (`LeaderPose27`), area 57's two spawns (`SpawnAtLeader`), area
59's two cell writers (`SetCells59`), the two flag-gathering triggers
(`FlagsGather`), the kind-4 triggers (`Tail4`), the two placement inits
(`PlaceOneObject`), the two dispatchers (`StateEntry`, as `area_w0c`'s).
Kept as the originals: every re-read after a call (`Sprite_Current`, the
answer byte, the focus pointer, `Field_ActiveMember`, row 3's byte), the
order of each store against the call beside it (area 62's counter before
the sound), the signed word compare of the fall and the signed division of
trigger 24, the 16-bit compares of the step hook's high words, the
unchecked `.data` reads by an answer or a list byte (section 6).

**`Area53_Trigger42` passes on `ScriptFlags_Set40`'s eax.** The original
calls `ScriptFlags_Set40` and returns without setting `al`; its caller
`0x56E020` returns that eax to `Field_ObjectTrigger` `0x56D6B0`, which
returns it too (neither reads it; its callers were not read). Capcom's
`ScriptFlags_Set40` leaves `Field_StatusBits | 0x40` in `al`; ours (group
C's, declared `void`) leaves whatever its compiled code does. Ours calls
it through a pointer typed to return `unsigned` and returns that: exactly
what the original returns with either `ScriptFlags_Set40` in place. The
fuzz compares it (`ret_mask 0xFF`).

**The two thunks** call `Sound_StopMusic` / `Sound_ResumeAll` from C++,
not by a tail `jmp`: `ecx` at the call is ours, not the op dispatcher's.
`Sound_StopMusic` (SX's reading) reads the caller's `ecx` only as the
status local when `GetStatus` fails, which is garbage in the original too;
chapter 8's `0x587B80` call (SC7) is the same kind of C++ call.

## 3. The fuzz

`BOF3X_SHADOW=area_w1d` (`src/game/area_w1d_fuzz.cpp`): ten `Run`s under
the one shadow name, one per area with its `Group::area` (53, 55, 56, 57,
59..64), 6,000 rounds per function, the real descriptors and tables in
place but area 56 and 59's state tables (`DataTable`s: their entries are
recorders while the fuzz runs) and areas 63 / 64's weights (regions).

- **Areas 63 and 64's inits are cloned from `0x40B730` / `0x40B800`**, the
  target of their entry `jmp`: `CloneOriginal` refuses an entry `E9` that
  is not a named call site (it reads it as a patch), and the body from the
  target is what every call runs. Offsets in the clone table are from
  there.
- **Callees the group lists:** `ScriptFlags_Set40` (moves the active member
  and the focus pointer, half the time each), `MoveCmd_TestFB` and
  `Flags_Set` (move the answer byte), `Sound_PlayEffect` (moves counter 0
  and row 3's byte), `MapView_GroundAt` (moves `Sprite_Current`, and a
  third of the time answers the object's height less one, equal or one
  more - the fall's signed word compare), `Field_LeaderStepTick`,
  `AreaMap_Elevation`, `Effect_Spawn` (move `Sprite_Current`; `Effect_Spawn`
  `kByte 0xFE..0x02`), `MapView_SetElevation` masked to its word (it uses
  only the low 16 bits), `Sound_StopMusic` (Capcom's), `Inventory_Count`
  (ax 0 a third of the time and a zero low byte under a non-zero high
  byte another third), `Inventory_Remove`, `Item_NamePtr` (answers a
  16-byte buffer of noise, the same on both passes), `0x4220D0` by raw
  address, logged by the three dwords its pointer holds.
- **Regions beyond the field frame:** `Effect_Objects` records 0..3 (area
  59's effect runs in one half the time), the focus pointer `0x903804`,
  `Field_ActiveMember`, the mark `0x9398CF`, `Text_Records +0..+0xF`, areas
  63 and 64's weights (27 regions, 16,761 bytes).
- **Every round:** `Field_ActiveMember` at a party object, a field object,
  a party record or the running object; the focus pointer at a field object,
  an extra object, a party record, or a byte of the message cells just
  below `Sprite_Objects` (a negative offset: the division truncates toward
  0).
- **Seeds:** the choice answer at each value a handler tests, a negative
  byte and above; row 3's byte all seven flags set (with and without bit 7),
  one clear, or any; the fall's state index 0..2 (always: an index past 2
  aborts ours and jumps to data in the clone), its height at the signed
  word's edges, `+5` 0 and the two flag words' bit 3 clear half the time;
  area 57's list byte 0..7; the step hook as area 36's (`Cond_ByteFD`,
  pose, flag `0x33`, each failing alone; the high words on and one off each
  edge of x `0x46..0x48`, z `0x29..0x2B`, with a high byte); the effect
  index 0..1; area 62's `+0x89` at 6 and beside; the tail's state at 0, 1,
  2, 3, -1, `0x80`, `0x7F` with the cell it waits on at its value two times
  in three (step-paired); the weights the image's two times in three, else
  all zero, one weight of 64, or noise; object triggers called `(a field
  object, 0x904030)`.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 0 (at `0xA..0xC` or any), counter 3, the answer byte, the active
  member and focus pointers, row 3's byte.

**Result (in this worktree):** 282,000 rounds over the 47 functions (6,000 each), 296,106 calls to the stand-ins, 0 mismatches, 16,761 bytes of state (27 regions) and the log compared. Coverage: every callee each function can reach was called - e.g. `MoveCmd_TestFB` 5,259, `MapView_SetElevation` 578, the fall's three states 1,954 / 2,067 / 1,979 and the effect's two 3,016 / 2,984 through their dispatchers, `Inventory_Remove` 412, `Party_DropIn` (the step hook's hit) 369, the tail's `Item_NamePtr` / `Inventory_Add` / `Msg_OpenSystem` 846 each, `Rand` 10,968 / 10,953 (the second draw on the placing path, about 4,960 per init), `0x4220D0` 6,000.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4601 ours`, 414 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w1d.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w1d.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w1d`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all ten runs). **136 planted, 135 refused by a count (exit 3)**, no hang, no fault; **one equivalent**. Every one of the 47 functions has at least one control of its own; a control in a shared helper lists every function it refused in.

**The first run** (135 planted) left two standing. B5 (`Inventory_Count`'s word tested as a byte) was the fuzz's: its stand-in almost never answered a zero low byte under a non-zero high byte; it does a third of the time now (215 rounds). E24 is **equivalent**: the ground's word zero-extended instead of sign-extended before `<< 16` - the shift drops every bit the extension sets, so no input tells them apart (AR0C's same finding); its near variants E23 (`<< 15`, 6,000) and E28 (the low byte sign-extended, 5,979) are refused. C21 (six flags for seven) was refused in 11 rounds: `Sound_PlayEffect`'s stand-in rewrote row 3's byte to `0x7F` / `0xFF` mostly; it draws each one-flag-clear value now (35 rounds, the thinnest). The table is the second run, with every control against the final fuzz.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `Area53_ChoiceFocusPair` | the second byte from offset 0 | 5747 |
| A2 | `BySignedAnswer (53, 57)` | the answer unsigned | Area53_ChoiceFocusPair 1891 |
| A3 | `Area53_ChoiceFocusPair` | message 0xFFFE | 6000 |
| A4 | `Area53_ChoiceFocusPair` | +0x14 for +0x18 | 6000 |
| A5 | `Area53_Trigger42` | tail kind 0x2D | 6000 |
| A6 | `Area53_Trigger42` | answers 0 | 5984 |
| A7 | `Area53_Trigger42` | sub-kind 0xB | 6000 |
| B1 | `Area55_ChoiceTakeItem8` | counts item 9 | 611 |
| B2 | `Area55_ChoiceTakeItem8` | removes 2 | 412 |
| B3 | `Area55_ChoiceTakeItem8` | message 0x21 | 412 |
| B4 | `Area55_ChoiceTakeItem8` | the count test inverted | 611 |
| B5 | `Area55_ChoiceTakeItem8` | the count as a byte | 215 |
| B6 | `Area55_ChoiceMark24` | message 0x25 | 5394 |
| B7 | `MessageMarked (55, 59, 61)` | mark 7 | Area55_ChoiceTakeItem8 5588, Area55_ChoiceMark24 5394 |
| B8 | `Area55_Trigger23` | sub-kind 3 | 6000 |
| B9 | `Tail4 (55, 61)` | kind 5 | Area55_Trigger23 6000 |
| C1 | `ChoicePoseFlag (56 x7)` | the answer not read again for the flag | Area56_ChoicePoseFlag8 75, Area56_ChoicePoseFlag9 74, Area56_ChoicePoseFlagA 82, Area56_ChoicePoseFlagB 86, Area56_ChoicePoseFlagC 86, Area56_ChoicePoseFlagD 79, Area56_ChoicePoseFlagE 78 |
| C2 | `ChoicePoseFlag (56 x7)` | no sound on 3 | Area56_ChoicePoseFlag8 1223, Area56_ChoicePoseFlag9 1264, Area56_ChoicePoseFlagA 1267, Area56_ChoicePoseFlagB 1308, Area56_ChoicePoseFlagC 1265, Area56_ChoicePoseFlagD 1274, Area56_ChoicePoseFlagE 1283 |
| C3 | `ChoicePoseFlag (56 x7)` | Cond row 4 | Area56_ChoicePoseFlag8 692, Area56_ChoicePoseFlag9 671, Area56_ChoicePoseFlagA 683, Area56_ChoicePoseFlagB 707, Area56_ChoicePoseFlagC 683, Area56_ChoicePoseFlagD 678, Area56_ChoicePoseFlagE 705 |
| C4 | `LeaderPose27 (56 x9)` | +2 = 6 | Area56_ChoicePoseFlag8 617, Area56_ChoicePoseFlag9 652, Area56_ChoicePoseFlagA 582, Area56_ChoicePoseFlagB 644, Area56_ChoicePoseFlagC 651, Area56_ChoicePoseFlagD 639, Area56_ChoicePoseFlagE 616, Area56_ChoicePoseOn1 604, Area56_ChoicePoseIfAllFlags 244 |
| C5 | `LeaderPose27 (56 x9)` | x and z swapped | Area56_ChoicePoseFlag8 620, Area56_ChoicePoseFlag9 653, Area56_ChoicePoseFlagA 583, Area56_ChoicePoseFlagB 646, Area56_ChoicePoseFlagC 652, Area56_ChoicePoseFlagD 640, Area56_ChoicePoseFlagE 617, Area56_ChoicePoseOn1 604, Area56_ChoicePoseIfAllFlags 244 |
| C6 | `LeaderPose27 (56 x9)` | x from +0x34 | Area56_ChoicePoseFlag8 620, Area56_ChoicePoseFlag9 653, Area56_ChoicePoseFlagA 583, Area56_ChoicePoseFlagB 646, Area56_ChoicePoseFlagC 652, Area56_ChoicePoseFlagD 640, Area56_ChoicePoseFlagE 617, Area56_ChoicePoseOn1 604, Area56_ChoicePoseIfAllFlags 244 |
| C7 | `Area56_ChoicePoseFlag8` | flag 7 | 692 |
| C8 | `Area56_ChoicePoseFlag9` | the other way round | 1250 |
| C9 | `Area56_ChoicePoseFlagA` | the flag on 2 | 1391 |
| C10 | `Area56_ChoicePoseFlagB` | the other way round | 1267 |
| C11 | `Area56_ChoicePoseFlagC` | flag 0xD | 683 |
| C12 | `Area56_ChoicePoseFlagD` | flag 0xC | 678 |
| C13 | `Area56_ChoicePoseFlagE` | the flag on 0 | 1081 |
| C14 | `Area56_ChoiceMessage9` | on 1 | 1266 |
| C15 | `Area56_ChoiceMessageA` | message 0xB | 646 |
| C16 | `Area56_ChoiceMessageB` | none 0xFFFE | 5368 |
| C17 | `Area56_ChoiceMessageC` | on 2 or more | 4139 |
| C18 | `Area56_ChoiceMessageD` | on any answer | 4735 |
| C19 | `Area56_ChoiceMessageE` | message 0xF | 611 |
| C20 | `Area56_ChoicePoseOn1` | on any answer | 4761 |
| C21 | `Area56_ChoicePoseIfAllFlags` | six flags | 35 |
| C22 | `Area56_ChoicePoseIfAllFlags` | the row byte +2 | 247 |
| C23 | `Area56_ChoicePoseIfAllFlags` | sound 0x209 | 614 |
| C24 | `Area56_ChoicePoseIfAllFlags` | the answer tested after the sound | 5386 |
| C25 | `Area56_FallRun` | state 2 run as 0 | 1979 |
| C26 | `Area56_FallBegin` | bit 0x80 cleared | 4552 |
| C27 | `Area56_FallBegin` | + 0x7CF | 6000 |
| C28 | `Area56_FallBegin` | step -7 | 6000 |
| C29 | `Area56_FallBegin` | Sprite_Current not read again after the ground | 2711 |
| C30 | `Area56_FallBegin` | state 2 | 6000 |
| C31 | `FieldStatePace (56 x2)` | less 3 | Area56_FallBegin 6000, Area56_FallStep 6000 |
| C32 | `Area56_FallStep` | ground at the height lands | 664 |
| C33 | `Area56_FallStep` | an unsigned compare | 2065 |
| C34 | `Area56_FallStep` | state 3 | 2731 |
| C35 | `Area56_FallStep` | bit 4 for 8 | 562 |
| C36 | `Area56_FallStep` | +6 for +5 | 580 |
| C37 | `Area56_FallStep` | Sprite_Current not read again after the step tick | 2630 |
| C38 | `Area56_FallStep` | rise not added | 5034 |
| C39 | `Area56_FallStep` | +0x20 kept on landing | 2733 |
| C40 | `Area56_FallEnd` | bit 0x400 | 4541 |
| C41 | `Area56_FallEnd` | state 1 | 6000 |
| C42 | `FlagsGather (56, 57)` | flags 0x6D..0x6F | Area56_Trigger52 1873 |
| C43 | `FlagsGather (56, 57)` | flag 0x72 | Area56_Trigger52 1253 |
| C44 | `Area56_Trigger52` | flag 0x6D | 6000 |
| D1 | `Area57_ChoiceMessage` | the next word | 5928 |
| D2 | `SpawnAtLeader (57 x2)` | x and z swapped | Area57_SpawnKind2AtLeader 6000, Area57_SpawnKind4AtLeader 6000 |
| D3 | `SpawnAtLeader (57 x2)` | the slot to the leader, not Sprite_Current | Area57_SpawnKind2AtLeader 2077, Area57_SpawnKind4AtLeader 2125 |
| D4 | `SpawnAtLeader (57 x2)` | none 0xFE | Area57_SpawnKind2AtLeader 2352, Area57_SpawnKind4AtLeader 2427 |
| D5 | `SpawnAtLeader (57 x2)` | the list byte 0x904063 | Area57_SpawnKind2AtLeader 5603, Area57_SpawnKind4AtLeader 5549 |
| D6 | `SpawnAtLeader (57 x2)` | the leader not made Sprite_Current | Area57_SpawnKind2AtLeader 2521, Area57_SpawnKind4AtLeader 2588 |
| D7 | `Area57_SpawnKind2AtLeader` | kind 3 | 6000 |
| D8 | `Area57_SpawnKind4AtLeader` | the kind-2 table | 5320 |
| D9 | `Area57_StopMusic` | resumes instead | 6000 |
| D10 | `Area57_ResumeSound` | twice | 6000 |
| D11 | `Area57_Trigger53` | flag 0x70 | 6000 |
| E1 | `Area59_ChoiceMessage2or4` | message 3 for 2 | 618 |
| E2 | `Area59_ChoiceMessage2or4` | the test inverted | 6000 |
| E3 | `Area59_ClearCells` | value 1 in row 9 | 6000 |
| E4 | `Area59_SetCells` | 0xC1 | 6000 |
| E5 | `SetCells59 (59 x2)` | row 9 to 0x48 | Area59_ClearCells 6000, Area59_SetCells 6000 |
| E6 | `SetCells59 (59 x2)` | row 0xA for 9 | Area59_ClearCells 6000, Area59_SetCells 6000 |
| E7 | `Area59_StepHook` | z from 0x2A | 184 |
| E8 | `Area59_StepHook` | x span 4 | 80 |
| E9 | `Area59_StepHook` | pose 5 for 6 | 163 |
| E10 | `Area59_StepHook` | Cond_ByteFD 3 | 3492 |
| E11 | `Area59_StepHook` | flag 0x34 | 2724 |
| E12 | `Area59_StepHook` | answers 2 | 369 |
| E13 | `Area59_StepHook` | x high word as a byte | 40 |
| E14 | `Area59_StepHook` | counter 0 = 1 | 369 |
| E15 | `Area59_Trigger24` | +1 = 5 | 6000 |
| E16 | `Area59_Trigger24` | +0x84 | 6000 |
| E17 | `Area59_Trigger24` | word +0x8A = 1 | 6000 |
| E18 | `Area59_Trigger24` | an unsigned division | 1180 |
| E19 | `Area59_Trigger24` | a floor division | 1180 |
| E20 | `Area59_Trigger24` | sub-kind 6 | 6000 |
| E21 | `Area59_Trigger24` | the pointer read once, before ScriptFlags_Set40 | 2953 |
| E22 | `Area59_EffectRun` | the entries swapped | 6000 |
| E23 | `Area59_EffectGround` | << 15 | 6000 |
| E24 | `Area59_EffectGround` | zero-extended | **not refused: equivalent** (above) |
| E25 | `Area59_EffectGround` | +1 by 2 | 6000 |
| E26 | `Area59_EffectGround` | Sprite_Current not read again | 2882 |
| E28 | `Area59_EffectGround` | the ground's low byte sign-extended (E24's near variant) | 5979 |
| E27 | `Area59_EffectStep` | y and z swapped | 6000 |
| F1 | `Area60_InitScriptFlag2000` | 0x40 | 4456 |
| F2 | `Area60_InitScriptFlag2000` | set, not or-ed | 5953 |
| G1 | `Area61_ChoiceClearZenny` | message 5 | 627 |
| G2 | `Area61_ChoiceClearZenny` | Zenny 1 | 627 |
| G3 | `Area61_ChoiceClearZenny` | message 4 | 5373 |
| G4 | `Area61_ChoiceMark4` | message 5 | 5378 |
| G5 | `Area61_ChoiceMark4` | none 0xFFFE | 622 |
| G6 | `Area61_Trigger21` | sub-kind 0 | 6000 |
| H1 | `Area62_ArmTailGiveItem` | bit 0 kept | 3036 |
| H2 | `Area62_ArmTailGiveItem` | +0x89 at 5 | 1895 |
| H3 | `Area62_ArmTailGiveItem` | kind 0x2B | 1282 |
| H4 | `Area62_ArmTailGiveItem` | the member read before ScriptFlags_Set40 | 577 |
| H5 | `Area62_ArmTailGiveItem` | +0x8A by 2 | 1282 |
| H6 | `Area62_TailGiveItem59` | counter 0xB at state 0 | 613 |
| H7 | `Area62_TailGiveItem59` | the counter stored after the sound | 581 |
| H8 | `Area62_TailGiveItem59` | waits on 0xC | 943 |
| H9 | `Area62_TailGiveItem59` | item 0x5A | 846 |
| H10 | `Area62_TailGiveItem59` | 12 bytes of the name | 846 |
| H11 | `Area62_TailGiveItem59` | adds 2 | 846 |
| H12 | `Area62_TailGiveItem59` | the two system messages swapped | 314 |
| H13 | `Area62_TailGiveItem59` | flag 0x69 | 532 |
| H14 | `Area62_TailGiveItem59` | sound 0x107 | 532 |
| H15 | `Area62_TailGiveItem59` | Field_Request 3 | 846 |
| H16 | `Area62_TailGiveItem59` | state 3 after the item | 846 |
| H17 | `Area62_TailGiveItem59` | waits on Field_Request 1 | 548 |
| H18 | `Area62_TailGiveItem59` | counter 0 kept at the end | 760 |
| H19 | `Area62_TailGiveItem59` | state 0 sound 0x10E | 1198 |
| I1 | `PlaceOneObject (63, 64)` | Rand & 0x1F | Area63_InitPlaceObject 1508 |
| I2 | `PlaceOneObject (63, 64)` | the weight inclusive | Area63_InitPlaceObject 419 |
| I3 | `PlaceOneObject (63, 64)` | seven objects | Area63_InitPlaceObject 5811 |
| I4 | `PlaceOneObject (63, 64)` | Rand & 3 | Area63_InitPlaceObject 2464 |
| I5 | `PlaceOneObject (63, 64)` | z from the pair's x byte | Area63_InitPlaceObject 4354 |
| I6 | `PlaceOneObject (63, 64)` | none placed clears none | Area63_InitPlaceObject 1032 |
| I7 | `PlaceOneObject (63, 64)` | EdgeBits less 4 | Area63_InitPlaceObject 6000 |
| I8 | `PlaceOneObject (63, 64)` | the height a byte | Area63_InitPlaceObject 4955 |
| I9 | `Area63_InitPlaceObject` | area 64 weights | 4232 |
| I10 | `Area64_InitPlaceObject` | the positions a pair on | 4953 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (20): `Area53_Choices`
`0x5FEA8C` (2), `Area53_ChoicePairs` `0x5FEADC` (16 bytes: the bytes before
the next item, not a bound the code checks), `Area55_Choices` `0x6007C0`
(5), `Area56_Choices` `0x600B4C` (16), `Area56_Handlers` `0x600B88` (1),
`Area56_FallStates` `0x600BE4` (3, right after the descriptor),
`Area57_Handlers` `0x60210C` (5), `Area57_Choices` `0x60211C` (1),
`Area57_ChoiceMessages` `0x60296C` (6 words), `Area57_EffectKinds2`
`0x602978` / `Area57_EffectKinds4` `0x602980` (8 bytes each, by analogy
with `Area49_EffectKinds`: the index is a party list byte),
`Area59_Handlers` `0x603C58` (4), `Area59_Choices` `0x603C78` (5),
`Area59_EffectStates` `0x603CDC` (2, right after the descriptor),
`Area61_Choices` `0x6040A0` (5), `Area62_Handlers` `0x6041D8` (1),
`Area63_Positions` `0x604298` (16) / `Area63_Weights` `0x6042A8` (8),
`Area64_Positions` `0x604368` / `Area64_Weights` `0x604378`. As ART found
generally, a descriptor's `+0x3C` array sits beside its `+0x34` table; a
dispatcher's state table directly follows its descriptor (`+0x44`).

## 6. Latent defects

Described, not fixed:

- **Unchecked dispatch.** `Area56_FallRun` jumps through
  `Area56_FallStates[Sprite_Current[4]]` and `Area59_EffectRun` through
  `Area59_EffectStates[Sprite_Current[1]]` with no bound; past 3 (area 56)
  the next dword is data, past 2 (area 59) it is 0. Ours aborts with a
  message there (the owner's rule for an unchecked index, round9 doc
  section 6); nothing measured reaches it. (`Area59_EffectGround` steps
  `+1` from 0 to 1 and `Area59_EffectStep` leaves it, so the effect stays
  in state 1 unless `0x4220D0` or another writer moves it - not read.)
- **Unchecked indexes into data** (kept: the reads stay in `.data`):
  `Area53_ChoiceFocusPair` and `Area57_ChoiceMessage` by the s8 answer (a
  negative answer reads before the table); `Area57_Spawn*AtLeader` by a
  party list byte.
- **`Area59_Trigger24` divides any pointer** by `0xA4` into counter 3: a
  focus pointer that is not a field object gives a meaningless index (its
  low byte), as chapter 8's runs do (SC7).
- **`Area53_Trigger42` answers whatever `ScriptFlags_Set40` left in eax**
  (section 2); nothing read here uses it.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 47 starts, extents, call sites,
  the shape its root gives. No jump table in `.text` (area 62's tail
  dispatches by a `sub` / `dec` chain); the two dispatchers jump through
  `.data` (the tool's `+0xB note` rows).
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding.
- **The tool's gaps:** none in this band. Every tail kind the band arms is
  an immediate store (`0x2C`, 4, `0x2A`); kind `0x2A` is
  `Area62_TailGiveItem59`, kinds `0x2C` and 4 are engine code (`0x56DE50`,
  `0x56D930`).
- **Two entries start with a `jmp`**: `0x40B720` / `0x40B7F0` (section 3).

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column
  is empty for all 47). Every function is reached only in play: the choices
  when the area's message box asks, the handlers from its movement scripts,
  the step hook on a step in area 59, the tail once armed, the inits on
  entry, the triggers by an object's `+0x86`, the effect by its kind.
- **Gap functions** (reached by no area table in the tool's walk): the six
  object triggers (ids 21, 23, 24, 42, 52, 53, by an object's `+0x86` from
  area data; no immediate store of those ids read) and `Area59_EffectRun`
  (`Effect_KindHandlers` entry `0xB6`; no immediate store of kind `0xB6`
  read). Each sits in the block of the area named and is attributed to it
  by address.

## 9. Calls across groups

- **Raw address nobody owns:** `0x4220D0` (group AR3F's block, area 146's
  shared body; `area_w0c` calls it the same way), in `area_w1d_callees.h`.
  No SX2 address is called.
- **By name, Capcom's:** `Effect_Spawn`, `Sound_StopMusic`,
  `Sound_ResumeAll`, `Rand`.
- **By name, ours:** `ScriptFlags_Set40` / `Clear40`, `Flags_Set` /
  `Test`, `Sound_PlayEffect`, `MoveCmd_TestFB`, `MapView_GroundAt`,
  `MapView_SetElevation`, `Field_LeaderStepTick`, `AreaMap_Elevation`,
  `AreaMap_SetByte`, `Party_DropIn`, `Inventory_Count` / `Remove` / `Add`,
  `Item_NamePtr`, `Msg_OpenSystem`. No harness edit; no `AH_THEIRS` moved.
- **Inbound, for the rebinding pass:** none by a call instruction. The
  engine reaches the band only through tables read in place
  (`Area_Descriptors`, `Field_ObjectTriggers`, `Effect_KindHandlers`
  `0x655628`, `Field_ModeTailKinds`, and `event_ops.cpp`'s
  `kStepHandlers`, which names `0x40B410` by address and reaches ours
  through the inject `jmp`). The shared bodies `Area57_StopMusic` /
  `Area57_ResumeSound` (areas 39, 108, 145), `Area59_EffectGround` (areas
  36, 100, 112, 116, 143, 146) and `Area61_ChoiceMark4` (areas 59, 113,
  116) are named by other areas' tables, not called.
- `analysis/calltrace/entries_logic.txt`: 47 lines appended under a `#
  group AR1D` comment; `0x40B410` had a host line (`9DA`) the
  consolidation will cut to `0x5E`.
