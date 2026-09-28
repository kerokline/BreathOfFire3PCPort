# World 0's first areas: 0..5, 7, 8, 10, 12, 13 and 15 (`0x401000..0x401B80`)

**Status:** IN PROGRESS (2026-09-27) - 49 functions ours
(`src/game/area_w0a.cpp`, shadow name `area_w0a`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 294,000 rounds (6,000 a function); 98 controls planted, 97 refused by a count, 1 equivalent mutant with its near variant refused. Fuzz
only: no recorded route reaches any area of the band (section 6). No
divergence; two unchecked state dispatches abort past their table (section
7).

Group AR0A of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §6,
[`takeover-queue-areas.md`](takeover-queue-areas.md)): the band
`0x401000..0x401B80` of [`area-rows.md`](area-rows.md) §4, twelve areas'
blocks with area 11's three (wave one's, [`area_011.md`](area_011.md)) in
the middle. What each area *is* in the story is not read here; the
sibling's `names/areas.toml` gives the owner's place names (area 0
"MacNeil Village - Spring", 13 "Farm Fields - Spring", ...), quoted only as
that record.

## 1. The areas, their roots and blocks

`tools/area_rows.py --unit AREA<NNN>` for each area (2026-09-27, over a
scratch copy of `analysis/`), each descriptor's `+0x34` / `+0x3C` / `+0x40`
read again by a scratch script. Twins: the sibling's
`names/area_records.toml` pairs every handler and init (the PSX descriptor
`handler[k]` is the PC's `+0x3C[k]`); a choice handler has no record there,
and `analysis/pairs_propagated.json` pairs three of them by table
(`0x401000`, `0x4014E0`, `0x401860`). Entries outside the band are other
groups' (another world's shared bodies) and not read here.

| Area | Descriptor | `+0x34` choices | `+0x3C` handlers | `+0x40` init | Block | Ours |
|---|---|---|---|---|---|---|
| 0 | `0x5DB318` (`Area0_Descriptor`) | `Area00_Choices` `0x5DB304`, 4 | `Area00_Handlers` `0x5DB30C`, 2 (choices 2..3) | - | `0x401000..0x4010D0` | 4 |
| 1 | `0x5DC760` | `Area01_Choices` `0x5DC74C`, 4 (0, 1: `0x403050`, `0x425C30`) | `Area01_Handlers` `0x5DC754`, 2 (choices 2..3) | - | `0x4010D0..0x401150` | 2 |
| 2 | `0x5DD5C8` | `Area02_Choices` `0x5DD5C0`, 1 | - | - | `0x401150..0x401190` | 1 |
| 3 | `0x5DDB40` | `Area03_Choices` `0x5DDB2C`, 5 (= handlers 2..6) | `Area03_Handlers` `0x5DDB24`, 7 (3, 4: `0x420850`, `0x420870`) | - | `0x401190..0x401250` | 3 + the gap `0x401230` |
| 4 | `0x5DE8A0` | `Area04_Choices` `0x5DE898`, 1 | - | - | `0x401250..0x401290` | 1 |
| 5 | `0x5DF6A0` | `Area05_Choices` `0x5DF5EC`, 4 | - | - | `0x401290..0x401300` | 2 (+ 3 shared bodies areas 7 and 19 reach) |
| 7 | `0x5E0378` | - | `Area07_Handlers` `0x5E0350`, 9 (0, 5, 8: `0x408F10`, `0x40CE10`, `0x40F530`) | - | `0x401300..0x4014E0` | 3 |
| 8 | `0x5E1078` | `Area08_Choices` `0x5E1040`, 14 (= handlers from one before) | `Area08_Handlers` `0x5E1044`, 13 (6..9: `0x41D810`, `0x409960`, `0x409970`, `0x40F530`) | - | `0x4014E0..0x4016D0` | 10 (+ 2 shared bodies areas 5, 10, 15 reach) |
| 10 | `0x5E23A8` | `Area10_Choices` `0x5E2390`, 5 | `Area10_Handlers` `0x5E23A0`, 1 (choice 4) | - | `0x4016D0..0x401750` | 3 |
| 12 | `0x5E2AA8` | - | `Area12_Handlers` `0x5E2AA4`, 1 | - | `0x401840..0x401860` | 1 |
| 13 | `0x5E3FB8` | `Area13_Choices` `0x5E3FA0`, 6 (= handlers from one before) | `Area13_Handlers` `0x5E3FA4`, 5 | - | `0x401860..0x4019B0` | 6 |
| 15 | `0x5E5B80` | `Area15_Choices` `0x5E5B78`, 1 | `Area15_Handlers` `0x5E5B60`, 7 (1..3: `0x409970`, `0x409960`, `0x40F530`) | `0x4019B0` | `0x4019B0..0x401B80` | 7 |

No area of the band has a step, arrive or cell hook, a mode-tail slot
(`0x662CE8` holds no band address; the band arms kinds 4 and 5 by storing
`0x9039F3`), or a world-map record. Areas 6, 9 and 14 have no block: 6
has no code field, 9 none, 14's one handler is `0x408F10` (another band).
Area 11's block `0x401750..0x401840` is wave one's.

**Shared bodies** (taken once, keyed by address, in the `Run` of the first
area named):

| PC | Block | Reached by |
|---|---|---|
| `0x4012D0`, `0x4012E0`, `0x4012F0` | 5 | area 7 handlers 1..3, area 19 handlers 2..4 (AR0B's area) |
| `0x4014C0`, `0x4014D0` | 7 | area 7 handlers 6, 7, area 131 handlers 0, 1 |
| `0x401640` | 8 | area 8 handler 12 / choice 13, area 3 handler 0 |
| `0x401670` | 8 | areas 5, 10, 15 choice 0, area 15 handler 6 |
| `0x4016A0` | 8 | areas 5, 10 choice 1 |

**The starts, against the tool.** The tool lists 51 starts in the band, 3
ours: every one agrees with the reading (extent to the last instruction,
no jump out, no jump table). **Added: `0x401000`** (area 0's choice 0,
`0x32` bytes), in none of `pc_funcs.json`, `pc_hidden.json` or the tool's
list: the tool's start test (`_looks_like_start`) wants the byte before a
start to be padding or a `ret`, and `0x401000` is the first byte of
`.text`. Its descriptor entry names it, so it is area 0's. **Dropped:
none.** The gap **`0x401230`** is `Field_ObjectTriggers` entry 22 (called
by `0x56E020` with the object and `0x904030`): no descriptor names it and
nothing else reaches it; it lies in area 3's block by address.

## 2. What each function does

`symbols.toml` has the evidence for each (`[[func]]`, `impl`
`src/game/area_w0a.cpp`). Variables are the movement script's
(`MoveScript_Variable`: 3 = `0x903848`, 5 = `0x90384A`, 6 = `0x90384B`);
"the row" is the choice box's cursor `0x7DEE67`, as s8 where a table is
indexed, as a byte where only 0 is tested; "no new message" is the message
word `0x7DEE48` = `0xFFFF` ([`item-use.md`](item-use.md) §5).

| PC | Name | Area / root | What |
|---|---|---|---|
| `0x401000` | `Area00_ChoiceVars3And6` | 0 choice 0 | no new message; row 0: variables 3 and 6 `0x14`, row 1: both `0x19` |
| `0x401040` | `Area00_ChoiceVar3` | 0 choice 1 | no new message; row 0: variable 3 `0x14`, row 1: `0x19` |
| `0x401070` | `Area00_FaceAsActiveMember` | 0 handler 0 | `Sprite_Current[8]` = `Field_ActiveMember[0x85]`; `Sprite_FaceDirection` of it |
| `0x4010A0` | `Area00_MoveFacing` | 0 handler 1 | `MoveCmd_Move(MoveScript_Object, Sprite_Current[8])`; the script object's `+7` = 2 (read after) |
| `0x4010D0` | `Area01_ActiveMemberToVar6` | 1 handler 0 | variable 6 = (`Field_ActiveMember` - `Sprite_Objects`) / `0xA4`, signed |
| `0x401100` | `Area01_SpawnEffectB` | 1 handler 1 | variable 5 above 6 and `Frame_Counter & 7` 0: a free effect record, kind `0xB` (`+0` 1, `+5`, `+0x1C` 5, `+7` 3); the slot kept at `0x903850` |
| `0x401150` | `Area02_ChoiceArmTail` | 2 choice 0 | no new message; row 0 / 1: `0x9039F5` 5 / 4, then tail kind 5 and story flag `0x1E`; another row nothing |
| `0x401190` | `Area03_ChoiceMessage42` | 3 choice 0, handler 2 | row 0: message `0x42`; else message `0x44` and the byte `0x9398CF` 6 |
| `0x4011C0` | `Area03_ChoiceMessage44` | 3 choices 3, 4, handlers 5, 6 | a row not 0: message `0x44`, `0x9398CF` 6; row 0 no new message |
| `0x4011F0` | `Area03_SetCells` | 3 handler 1 | four map cells (`0x54`/`0x55`, `0x1C`/`0x1D`) to `0xC0` / `0xA1` |
| `0x401230` | `Area03_Trigger22` | object trigger 22 (gap) | `ScriptFlags_Set40`; tail kind 4 with `0x9039F5` 0; `al` 0 |
| `0x401250` | `Area04_ChoiceArmTail` | 4 choice 0 | as area 2's with 0 / 1 and story flag `0x1F` |
| `0x401290` | `Area05_ChoiceMessageA` | 5 choice 2 | the message `Area05_MessagesA[row]` |
| `0x4012B0` | `Area05_ChoiceMessageB` | 5 choice 3 | the message `Area05_MessagesB[row]` |
| `0x4012D0` / `E0` / `F0` | `Area07_PlaceKind2At1` / `2` / `3` | 7 handlers 1..3 (19's 2..4) | `Kind2_Place(1 / 2 / 3)` |
| `0x401300` | `Area07_SetCells` | 7 handler 4 | 38 map cells set (0 on twelve, `0x50` on the twelve beside them, 0 on fourteen more) |
| `0x4014C0` / `D0` | `Area07_CameraShiftYLess` / `More` | 7 handlers 6 / 7 (131's 0 / 1) | `Camera_ShiftY` -/+ `0xA`, `MapView_Redraw` 2 |
| `0x4014E0` | `Area08_ChoiceMessageVar3` | 8 choice 0 | the message `Area08_ChoiceMessages[row]`; row 0: variable 3 1, row 1: 0 |
| `0x401510` | `Area08_SetCells` | 8 handler 0 | three map cells |
| `0x401540` | `Area08_OpenMessage54` | 8 handler 1 | `Crt_sprintf(Text_Records, Area08_MessageFormat, variable 6 & 0x7F)`, `Msg_OpenScript(0x54)`, `Field_Request` 2 |
| `0x401570` | `Area08_OpenMessage56` | 8 handler 2 | the same with message `0x56`, then `Music_Play(0xA, 8)` |
| `0x4015B0` | `Area08_SpawnEffect54` | 8 handler 3 | an effect record of kind `0x54` whose `+0xB` is `Field_ActiveMember`'s index (as `0x4010D0`'s, read after the call) |
| `0x401600` / `10` | `Area08_CameraShiftXMore` / `Less` | 8 handlers 4 / 5 | `Camera_ShiftX` +/- `0x14`, `MapView_Redraw` 2 |
| `0x401620` / `30` | `Area08_Object6XMore` / `Less` | 8 handlers 10 / 11 | field object 6's x (dword `0x7DF28C`) +/- `0x800` |
| `0x401640` | `Area08_ClearCells` | 8 handler 12 (3's 0) | the four cells `Area03_SetCells` sets, to 0 |
| `0x401670` | `Area05_ChoiceVar3At64` | 5 / 10 / 15 choice 0, 15 handler 6 | no new message; row 0 / 1: variable 3 `0x64` / `0x65` |
| `0x4016A0` | `Area05_ChoiceVar3AtC8` | 5 / 10 choice 1 | the same with `0xC8` / `0xC9` |
| `0x4016D0` / `F0` | `Area10_ChoiceMessageA` / `B` | 10 choices 2 / 3 | the message `Area10_MessagesA / B[row]` |
| `0x401710` | `Area10_SpawnEffect1C` | 10 handler 0 | an effect record of kind `0x1C` at `Sprite_Current`'s x and z (read after the call) |
| `0x401840` | `Area12_WaitFlagE` | 12 handler 0 | until flag `0xE` of the bank at `[0x929ED0]`, the script position 2 back (the op runs again) |
| `0x401860` | `Area13_ChoiceMessageVar3` | 13 choice 0 | the message `Area13_ChoiceMessages[row]`; row 0 / 1: variable 3 `0xA` / `0x14` |
| `0x401890` | `Area13_ClearCells` | 13 handler 0 | four map cells to 0 |
| `0x4018C0` | `Area13_SkipUnlessLeader8` | 13 handler 1 | the leader's `+0x89` not 8: the script position `0xC` on |
| `0x4018E0` | `Area13_WalkToX` | 13 handler 2 | direction 3 or 7 toward x `0x1C8000` (by the sign of the 32-bit difference), steps \|d\| / `0x8000` to `+7`, `MoveCmd_Move` when any |
| `0x401940` | `Area13_WalkToZ` | 13 handler 3 | direction 1, steps \|`0x20000` - z\| / `0x8000`, `MoveCmd_Move` when any |
| `0x401990` | `Area13_TurnDirection` | 13 handler 4 | `Sprite_Current[8]` one on, of eight |
| `0x4019B0` | `Area15_SetZones` | 15 init | the zone byte of the area's zone records 1 and 2 (`Area_ZoneAt`'s lists `0x668D80`) 2 with `Field_StatusBits` bit 0, else 0 |
| `0x401A00` | `Area15_PassFlags1F` | 15 handler 0 | `Draw_PassFlags` `0x1F` |
| `0x401A10` / `0x401AF0` | `Area15_RunStatesA` / `B` | 15 handlers 4 / 5 | `jmp [Area15_StatesA / B + Sprite_Current[4] * 4]` |
| `0x401A30` / `0x401B10` | `Area15_StateA0` / `B0` | state 0 of each table (`kState`) | a pose pair by frame parity (`Sprite_EnsureAnimation`, `+0x2A`), a countdown `Rand & 7` to `+0xA`, state 1, the op again |
| `0x401AA0` | `Area15_StateCount` | state 1 of both tables | the countdown down (state 0 at its end); with the scene timer `0x8034E6` at 0, state and countdown 0; else the op again |

**Reading notes.**

- The choice handlers write the message word before they branch (the
  compiler kept `sub eax, 0` for the flags across the `mov word`); ours
  writes it first too.
- `0x401150` loads `cl = 5` before the branch and stores `cl` to the tail
  kind on both paths: the kind is 5 for both rows, only `0x9039F5` differs.
- The effect handlers keep their `Effect_FindFree` slot at `0x903850` and
  read it back as a dword masked to a byte (`0x401100`, `0x4015B0`);
  `0x401710` keeps it on the stack. The record index is unchecked
  (`Effect_FindFree` answers 0..19 or `0xFF`).
- `0x4018E0` tests `jns` after the `sub`: the sign of the wrapped 32-bit
  difference, not a signed compare. Its step count is `cdq / xor / sub`
  (so `0x80000000` stays itself) then a signed divide by `0x8000`.
- `0x401A30` stores the parity into the stack slot its `push ecx` made
  and reads the whole dword back, masked to a byte: the parity.
- Area 15's two pose tables hold the same four bytes (read, not quoted),
  so `Area15_StateA0` and `_StateB0` differ only by address.

## 3. The fuzz

`BOF3X_SHADOW=area_w0a` (`src/game/area_w0a_fuzz.cpp`): twelve `Run`s, one
per area (`Group::area`), 6,000 rounds per function, the clone rows the
tool's with `0x401000` added and each function's shape by its first root:
`kChoice` for a handler that writes the message word, `kHandler` for the
rest of the `+0x34` / `+0x3C` entries, `kInit` for `0x4019B0`, `kState` for
area 15's three state handlers (cloned and called directly), `kCallee` for
the trigger `0x401230` (`ret_mask 0xFF`, arguments the object and
`0x904030` through `Group::args`). Area 15's `Area15_StatesA` / `B` are
`DataTable`s (two entries each): the dispatchers' `jmp` reaches recorders.

**Callees the standard set lacks** (`kCallees`): `MoveCmd_Move` (Capcom's,
`{all, u8}`), `Effect_FindFree` (ours; `kByte` with an `effect` answering
`0xFF` a quarter of the time, else a slot 0..19), `Kind2_Place` (ours),
`ScriptFlags_Set40` (ours), `Crt_sprintf` (Capcom's; the destination, the
format's address and the value logged, nothing written).

**Regions** beyond the standard 20 (29 in all, 18,834 bytes):
`Field_ActiveMember` `0x9035A4`, `MoveScript_Object` `0x929E80`, the flag
bank's pointer `0x929ED0`, `Camera_ShiftX` / `Y` `0x903800`, the byte
`0x9398CF`, `Draw_PassFlags`, `Effect_Objects` (20 x `0x80`), area 15's
zone records 0..2 and another area's (the first whose list does not
overlap 15's, found at start-up).

**Seeds**, every round: `Field_ActiveMember` and `MoveScript_Object` at a
party object (`Sprite_ObjectsExtra`), a field object or a party record;
the flag bank inside `Cond_Flags`; the row two rounds in three from 0, 1,
2, 3, `0xFF`, `0x80`, `0x7F`. Per function: `0x4010D0` / `0x4015B0` any
pointer half the time and `Sprite_Objects` + 0, `0xA3`, `0xA4`, `0xA5`,
`0x1478` (the divide's edges); `0x401100` variable 5 at 5, 6, 7, 0, `0xFF`,
`0x80`, `0x7F` and `Frame_Counter`'s low bits 0 or one bit; `0x4018C0` the
leader's `+0x89` 8 or beside; `0x4018E0` / `0x401940` the coordinate at
the target less 0, +-1, +-`0x7FFF..0x8001`, `0x7F8000`, `0x800000`,
`0x80000000`, `0x7FFFFFFF`, `0x80000001`, +-`0x10000`; `0x4019B0`
`Game_AreaNumber` 15 or the other area; the dispatchers' state 0 or 1;
`0x401AA0` the countdown 0, 1, 2, `0xFF`, `0x80` and the timer 0, 1,
`0xFFFF`, `0x100`. **Disturbance** (`disturb`, from the hash only):
`Field_ActiveMember`, `MoveScript_Object`, the tail bytes `0x9039F3` /
`0x9039F5`, the slot byte `0x903850`, the timer, a byte of the effect
records or of area 15's zone records, the camera shifts, `0x9398CF`.

**Result (in this worktree):** 294,000 rounds over 49 functions, 0
mismatches in every `Run`; coverage `MoveCmd_Move` 14,546,
`Sprite_FaceDirection` 6,000, `Effect_FindFree` 14,260, `Flags_Set` 3,652,
`ScriptFlags_Set40` 6,000, `AreaMap_SetByte` 318,000, `Kind2_Place`
18,000, `Crt_sprintf` 12,000, `Msg_OpenScript` 12,000, `Music_Play` 6,000,
`Flags_Test` 6,000, `Rand` 12,000, `Sprite_EnsureAnimation` 12,000, the
state recorders `0x401A30` 2,935, `0x401AA0` 6,078, `0x401B10` 2,987.
`BOF3X_SHADOW='*'`: exit 0, 337 self-test lines, every one 0 mismatches.

## 4. Controls

Planted one at a time in `area_w0a.cpp` by a script (the scratch `controls.py`, not committed) that plants (each anchor checked unique), rebuilds, checks the file recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w0a`, restores; after the last it restored, rebuilt and ran the clean self-test (exit 0, 0 mismatches). **98 planted, 97 refused by a count (exit 3), 1 equivalent (C90) with its near variant refused.** Every function has at least one; a plant in a shared helper (`ObjectIndex`, `Steps`, the area 15 pose) counts in every function using it, and the process stops at the first `Run` that fails, so C08 shows only area 1's count.

| # | planted | refused: rounds mismatched (of 6,000 for the function) |
|---|---|---|
| C01 | Area00_ChoiceVars3And6: row 1 sets variable 6 to 0x18 | Area00_ChoiceVars3And6 897 |
| C02 | Area00_ChoiceVars3And6: row 0 sets variable 5 for 3 | Area00_ChoiceVars3And6 886 |
| C03 | Area00_ChoiceVar3: the 0x19 on row 2 | Area00_ChoiceVar3 1334 |
| C05 | Area00_MoveFacing: +7 = 3 | Area00_MoveFacing 6000 |
| C06 | Area00_MoveFacing: MoveScript_Object not read again after the call | Area00_MoveFacing 19 |
| C07 | Area01_ActiveMemberToVar6: the pointer one byte on | Area01_ActiveMemberToVar6 332 |
| C08 | ObjectIndex: an unsigned divide | Area01_ActiveMemberToVar6 1498 |
| C09 | Area01_SpawnEffectB: variable 5 < 6 returns (6 spawns) | Area01_SpawnEffectB 314 |
| C10 | Area01_SpawnEffectB: Frame_Counter & 3 | Area01_SpawnEffectB 481 |
| C11 | Area01_SpawnEffectB: the dword +0x1C 6 | Area01_SpawnEffectB 1676 |
| C12 | Area01_SpawnEffectB: +7 = 4 | Area01_SpawnEffectB 1676 |
| C13 | Area01_SpawnEffectB: the slot not kept at 0x903850 | Area01_SpawnEffectB 2249 |
| C14 | Area01_SpawnEffectB: slot 0x13 taken for none | Area01_SpawnEffectB 78 |
| C15 | Area02_ChoiceArmTail: row 1 sets 3 | Area02_ChoiceArmTail 932 |
| C16 | Area02_ChoiceArmTail: flag 0x1D | Area02_ChoiceArmTail 1828 |
| C17 | Area02_ChoiceArmTail: the tail kind stored after Flags_Set | Area02_ChoiceArmTail 6 |
| C18 | Area03_ChoiceMessage42: message 0x43 | Area03_ChoiceMessage42 914 |
| C19 | Area03_ChoiceMessage42: the row tested as s8 <= 0 | Area03_ChoiceMessage42 1889 |
| C20 | Area03_ChoiceMessage44: the byte 0x9398CF 7 | Area03_ChoiceMessage44 5099 |
| C21 | Area03_ChoiceMessage44: row 0 message 0xFFFE | Area03_ChoiceMessage44 901 |
| C22 | Area03_SetCells: the last cell 0xA0 | Area03_SetCells 6000 |
| C23 | Area03_Trigger22: tail kind 3 | Area03_Trigger22 6000 |
| C24 | Area03_Trigger22: answers 1 | Area03_Trigger22 6000 |
| C25 | Area03_Trigger22: the stores before ScriptFlags_Set40 | Area03_Trigger22 49 |
| C26 | Area04_ChoiceArmTail: flag 0x1E | Area04_ChoiceArmTail 1824 |
| C27 | Area04_ChoiceArmTail: row 0 sets 2 | Area04_ChoiceArmTail 925 |
| C28 | Area05_ChoiceMessageA: table B | Area05_ChoiceMessageA 5780 |
| C29 | Area05_ChoiceMessageB: the row unsigned | Area05_ChoiceMessageB 1868 |
| C30 | Area05_ChoiceVar3At64: row 1 sets 0x66 | Area05_ChoiceVar3At64 956 |
| C31 | Area05_ChoiceVar3AtC8: row 0 sets 0xC7 | Area05_ChoiceVar3AtC8 883 |
| C32 | Area07_PlaceKind2At1: Kind2_Place(0) | Area07_PlaceKind2At1 6000 |
| C33 | Area07_PlaceKind2At2: Kind2_Place(1) | Area07_PlaceKind2At2 6000 |
| C34 | Area07_PlaceKind2At3: Kind2_Place(4) | Area07_PlaceKind2At3 6000 |
| C35 | Area07_SetCells: the last cell z 0x1A | Area07_SetCells 6000 |
| C36 | Area07_SetCells: 37 cells | Area07_SetCells 6000 |
| C37 | Area07_CameraShiftYLess: 0xB less | Area07_CameraShiftYLess 6000 |
| C38 | Area07_CameraShiftYMore: MapView_Redraw 1 | Area07_CameraShiftYMore 6000 |
| C39 | Area07_CameraShiftYMore: 9 more | Area07_CameraShiftYMore 6000 |
| C40 | Area08_ChoiceMessageVar3: row 0 sets 2 | Area08_ChoiceMessageVar3 903 |
| C41 | Area08_ChoiceMessageVar3: the table by row & 1 | Area08_ChoiceMessageVar3 4214 |
| C42 | Area08_SetCells: the last value 0x50 | Area08_SetCells 6000 |
| C43 | Area08_OpenMessage54: message 0x55 | Area08_OpenMessage54 6000 |
| C44 | Area08_OpenMessage54: Field_Request before Msg_OpenScript | Area08_OpenMessage54 237 |
| C45 | Area08_OpenMessage54: variable 6 & 0xFF | Area08_OpenMessage54 2976 |
| C46 | Area08_OpenMessage56: Music_Play(0xA, 9) | Area08_OpenMessage56 6000 |
| C47 | Area08_OpenMessage56: Field_Request after Music_Play | Area08_OpenMessage56 224 |
| C48 | Area08_OpenMessage56: message 0x54 | Area08_OpenMessage56 6000 |
| C49 | Area08_SpawnEffect54: kind 0x55 | Area08_SpawnEffect54 4486 |
| C50 | Area08_SpawnEffect54: Field_ActiveMember read before the call | Area08_SpawnEffect54 18 |
| C51 | Area08_SpawnEffect54: +0xB at +0xC | Area08_SpawnEffect54 4486 |
| C52 | Area08_SpawnEffect54: slot 0x13 taken for none | Area08_SpawnEffect54 215 |
| C53 | Area08_CameraShiftXMore: 0x15 more | Area08_CameraShiftXMore 6000 |
| C54 | Area08_CameraShiftXLess: 0x13 less | Area08_CameraShiftXLess 6000 |
| C55 | Area08_Object6XMore: 0x801 more | Area08_Object6XMore 6000 |
| C56 | Area08_Object6XLess: 0x700 less | Area08_Object6XLess 6000 |
| C57 | Area08_ClearCells: the last cell z 0x1E | Area08_ClearCells 6000 |
| C58 | Area10_ChoiceMessageA: the row + 1 | Area10_ChoiceMessageA 5941 |
| C59 | Area10_ChoiceMessageB: table A | Area10_ChoiceMessageB 5608 |
| C60 | Area10_SpawnEffect1C: kind 0x1D | Area10_SpawnEffect1C 4485 |
| C61 | Area10_SpawnEffect1C: +0x38 from +0x34 | Area10_SpawnEffect1C 4485 |
| C62 | Area10_SpawnEffect1C: Sprite_Current read before the call | Area10_SpawnEffect1C 145 |
| C63 | Area10_SpawnEffect1C: slot 0x13 taken for none | Area10_SpawnEffect1C 224 |
| C64 | Area12_WaitFlagE: flag 0xF | Area12_WaitFlagE 6000 |
| C65 | Area12_WaitFlagE: the test negated | Area12_WaitFlagE 6000 |
| C66 | Area12_WaitFlagE: MoveScript_Object read before the call | Area12_WaitFlagE 12 |
| C67 | Area13_ChoiceMessageVar3: row 1 sets 0x15 | Area13_ChoiceMessageVar3 891 |
| C68 | Area13_ClearCells: the last cell z 1 | Area13_ClearCells 6000 |
| C69 | Area13_SkipUnlessLeader8: >= 8 | Area13_SkipUnlessLeader8 4065 |
| C70 | Area13_SkipUnlessLeader8: 0xB on | Area13_SkipUnlessLeader8 4768 |
| C71 | Area13_WalkToX: direction 3 only above 0 | Area13_WalkToX 252 |
| C72 | Area13_WalkToX: to 0x1C8001 | Area13_WalkToX 1458 |
| C73 | Steps: no absolute value | Area13_WalkToX 1984, Area13_WalkToZ 2044 |
| C74 | Steps: (magnitude + 1) / 0x8000 | Area13_WalkToX 1195, Area13_WalkToZ 1230 |
| C75 | Area13_WalkToZ: direction 2 | Area13_WalkToZ 6000 |
| C76 | Area13_WalkToZ: to 0x20001 | Area13_WalkToZ 1246 |
| C77 | Area13_WalkToZ: MoveCmd_Move with no steps too | Area13_WalkToZ 1737 |
| C78 | Area13_TurnDirection: & 0xF | Area13_TurnDirection 3007 |
| C79 | Area15_SetZones: bit 1 | Area15_SetZones 2964 |
| C80 | Area15_SetZones: record 2 at +0x13 | Area15_SetZones 6000 |
| C81 | Area15_SetZones: area 15 always | Area15_SetZones 2914 |
| C82 | Area15_PassFlags1F: 0x1E | Area15_PassFlags1F 6000 |
| C83 | Area15_RunStatesA: table B | Area15_RunStatesA 2935 |
| C84 | Area15_RunStatesB: table A | Area15_RunStatesB 2987 |
| C85 | Area15 pose: parity bit 0 | Area15_StateA0 2994, Area15_StateB0 2936 |
| C86 | Area15 pose: Rand & 3 | Area15_StateA0 2978, Area15_StateB0 2974 |
| C87 | Area15 pose: +0x2A from the pair's first byte | Area15_StateA0 2999, Area15_StateB0 2985 |
| C88 | Area15 pose: state 2 | Area15_StateA0 6000, Area15_StateB0 6000 |
| C89 | Area15 pose: parity read after Rand | Area15_StateA0 133, Area15_StateB0 130 |
| C90 | Area15_StateA0: the poses of table B | **0 - equivalent**: Area15_PosesA and Area15_PosesB hold the same four bytes, so no input tells the two tables apart; near variant C90b refused |
| C90b | Area15_StateA0: the poses one byte on (near C90) | Area15_StateA0 6000 |
| C91 | Area15_StateB0: the poses one byte on | Area15_StateB0 6000 |
| C92 | Area15_StateCount: state 0 at a count of 1 | Area15_StateCount 1229 |
| C93 | Area15_StateCount: the timer's low byte only | Area15_StateCount 602 |
| C94 | Area15_StateCount: the countdown 1 at the timer's end | Area15_StateCount 1159 |
| C95 | Area15_StateCount: the script position 4 back | Area15_StateCount 4841 |
| C96 | Area03_Trigger22: the byte 0x9039F5 1 | Area03_Trigger22 6000 |
| C97 | Area00_ChoiceVars3And6: no new message not written | Area00_ChoiceVars3And6 1997 |
| C04 | Area00_FaceAsActiveMember: the member byte +0x84 | Area00_FaceAsActiveMember 5977 |

## 5. Named data

`symbols.toml` `[[data]]`, each with its evidence: every area's choice
table and handler array (`Area00_Choices` .. `Area15_Handlers`, eighteen;
a descriptor's `+0x34` is often its `+0x3C` from one or two entries before,
so the entries overlap), the choice handlers' message tables
(`Area05_MessagesA` / `B`, `Area08_ChoiceMessages`, `Area10_MessagesA` /
`B`, `Area13_ChoiceMessages`, u16, their counts the space to the next
table), `Area08_MessageFormat` (an address only: its bytes are not read or
quoted), and area 15's `Area15_StatesA` / `B` and `Area15_PosesA` / `B`.
The engine table `0x668D80` (`Area_ZoneAt`'s per-area lists) is a constant
in `area_w0a_callees.h`, not named here.

## 6. What reaches it, and what nothing reached

- **Reach:** no route. No band address is in `hidden_reached_*.json`,
  `pc_hidden_reached.json` or any `calltrace/*/bof3x.callcounts.tsv` (the
  tool's "Live" column is empty for the group). The handlers run when an
  area's scripts use op `03` / `DE`, the choice handlers when its message
  box commits a choice, area 15's init on entry, the trigger when an object
  with `+0x86` 22 is triggered. A recorded walk of world 0 is the live check
  ([`takeover-queue-areas.md`](takeover-queue-areas.md) §5); the owner has
  none yet.
- **What the fuzz does not reach:** the engine around each root (the
  commit's read of the message word, op `DE`'s read of `Sprite_Current[8]`
  are logged, not run); what the callees do (recorders); a state index of 2
  or more in area 15 (the original jumps into data, ours aborts: never
  seeded).

## 7. Latent defects (Capcom's, described, not fixed)

- **Area 15's state dispatch is unchecked.** `0x401A10` / `0x401AF0` jump
  through a two-entry table by `Sprite_Current[4]`; a state of 2 or more
  jumps to the bytes after it (the pose table, then the next table). Ours
  calls `bof3::Fatal` instead (the project's rule for an index past its
  table, round9 doc section 6; no DIVERGENCE entry). Nothing in the band
  stores a state other than 0 and 1.
- **The choice tables are indexed by the row unchecked**: a row outside
  the table reads the neighbouring words of `.data` as a message. Kept.
- **An index from the wrong base, candidate.** `0x4010D0` and `0x4015B0`
  divide `Field_ActiveMember - Sprite_Objects` by `0xA4`. `Field_ActiveMember`'s
  own evidence has it point into `Sprite_ObjectsExtra` (`0x802000`), which
  is not a whole number of records after `Sprite_Objects` on the PC
  (`0x23180` / `0xA4` = 876.5): such a pointer gives `0x6C` plus the slot,
  not an object index. Not checked against the PSX twins (`0x801F2C88`,
  `0x801F443C`), where the objects may be contiguous; kept as read.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count) in
[`known-defects.md`](known-defects.md).

## 8. Calls across groups

None by raw address. Every callee is named: ours (`AreaMap_SetByte`,
`Sprite_FaceDirection`, `Effect_FindFree`, `Kind2_Place`,
`ScriptFlags_Set40`, `Flags_Set`, `Flags_Test`, `Msg_OpenScript`,
`Music_Play`, `Sprite_EnsureAnimation`) or Capcom's and unowned this wave
(`MoveCmd_Move` `0x578C10`, `Crt_sprintf` `0x5B9380`, `Rand`). The
descriptor entries that point into other bands (section 1) are table
entries, not calls. `analysis/calltrace/entries_logic.txt`: one line per
function, the extents of section 2 (49 appended).

`tools/ledger_check.py` in this worktree: 0 errors from this group; its one
error is the base's (`DIVERGENCE.md`'s status line says 57 entries, the
ledger has DIV-0058, from `368b84f`), left for the coordinator.
