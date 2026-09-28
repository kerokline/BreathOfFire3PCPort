# World 1, areas 48..52: the band `0x408FF0..0x40AB00`

**Status:** IN PROGRESS (2026-09-28) - 57 functions ours
(`src/game/area_w1c.cpp`, shadow name `area_w1c`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 342,000 rounds (in this worktree); 185 controls planted, 185 refused by a count (section 4). Fuzz only:
no recorded route reaches the band (section 8). No divergence; the areas'
unchecked reads stay in `.data` and are kept (section 6).

Group AR1C of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 9). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
57 starts, none ours before, **57 taken**; no start dropped, none added
(section 7). Areas 48..52 each have code; area 52 has no choice table.

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are `area_rows.py --clones`'s rows,
each read against the disassembly. What an area *is* in the story is not
read here. The PSX twins are the sibling's `names/area_records.toml`
(descriptor handlers and inits only; choices, hooks, tails and triggers have
no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the message box's answer byte
`0x7DEE67` in, the message word `0x7DEE48` read after), `kHandler` a `+0x3C`
handler (movement-script ops `03` / `DE`), `kInit` the `+0x40` init,
`kTail` a `Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a
step, arrive or cell hook `(x, z)` answering in `al`, `kCallee` a function
called directly (by the group's own code, by engine code, or as an object
trigger `(object, 0x904030)` answering in `al`). A function that is both a
choice and a handler is fuzzed as a handler (none of them touches the
message word).

### Area 48 (descriptor `0x5F98A8`; PSX `0x801F63B4`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x408FF0` | `Area48_ChoiceCounterBOr1` | `0x41` | choice 0 | kChoice | message 0xFFFF; answer bit 0 clear: counter 0 = `0xB`, sound `0x205`; set: counter 0 = 1, `ScriptFlags_Clear40`, `MoveScript_Var7` and its step 0 |
| `0x409040` | `Area48_ToggleFlagD` | `0x34` | choice 1 = handler 0 (PSX `0x801F4804`) | kHandler | the active member's `+0x80` bit 0 cleared; `MoveScript_EffectState[leader +0x89]` at 1: story flag `0xD` toggled (`0x57C160`) |
| `0x409080` | `Area48_BumpCount` | `0x40` | choice 2 = handler 1 (PSX `0x801F4864`) | kHandler | the same test; the count `0x92BEE7` + 1 below 8, sound `0x20D` |
| `0x4090C0` | `Area48_MoveScriptObject5` | `0xA3` | choice 3 = handler 2 (PSX `0x801F48DC`) | kHandler | direction 5; the script object's `+7` = count << 2; not 0: by `MoveScript_ObjectKind` 2 `MoveCmd_MoveKind2` (FA word 0, bit `0x40`), else `MoveCmd_Move` (bit `0x80`); `+0x14` 0, `+0x3E` `0x480` |
| `0x409170` | `Area48_MoveScriptObject1` | `0xA3` | choice 4 = handler 3 (PSX `0x801F49C8`) | kHandler | the same, direction 1 |
| `0x409220` | `Area48_PoseByCount` | `0x87` | choice 5 = handler 4 (PSX `0x801F4AB4`) | kHandler | the active member's `+0x83` by the count (2: 6, 6: 7, 8: 8, else 9), `+0x8A` `0xFFFE` |
| `0x4092B0` | `Area48_ResetCount` | `0x14` | choice 6 = handler 5 (PSX `0x801F4B40`) | kHandler | the count 0, the byte `0x803490` bit 0 set |
| `0x4092D0` | `Area48_TailDropIn` | `0x6B` | tail kind 3 | kTail | by the sub-kind: 1 `Party_DropIn` by the count (2: 6, 6: 7, 8: 1, else 8); 0 `Party_DropIn(0)` then the sub-kind read again; 2 `Party_DropIn(2)`; then disarmed |
| `0x409340` | `Area48_ArriveHook` | `0x8B` | `Area_ArriveHook`'s case for area 48 | kHook | three doorways (x, z high words as u16; z exactly `0x58000` / `0xA8000` also) each arm tail kind 3 with sub-kind 0 / 1 / 2 and `ScriptFlags_Set40`; al 0 |
| `0x4093D0` | `Area48_InitSound` | `0x15` | init (PSX `0x801F4D2C`) | kInit | `Cond_ByteFD` 5: sound `0x20C` |

The count `0x92BEE7` (five bytes after `MapView_ElevationOffset`) and the
byte `0x803490` are area 48's cells here; neither is named and no other
reader was read this round.

### Area 49 (descriptor `0x5FBBF8`; PSX `0x801F5C90`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4093F0` | `Area49_ChoiceMessageFlag3B` | `0x2C` | choice 0 | kChoice | the message word from `Area49_ChoiceMessages` by the s8 answer; answer 0: the chapter row's flag `0x3B` |
| `0x409420` | `Area49_ChoiceMessage` | `0x17` | choice 1 | kChoice | the message word from `Area49_ChoiceMessages + 4` by the s8 answer |
| `0x409440` | `Area49_StepHook` | `0x1B` | `Area_StepHook`'s case for area 49 | kHook | x exactly `0x4D8000` and `Cond_ByteFD` 0: `Area49_SwapFlags2To3`; al 0 |
| `0x409460` | `Area49_SwapFlags2To3` | `0x1C` | called by the step hook and the effect frame | kCallee | story flag 2 cleared, 3 set |
| `0x409480` | `Area49_EffectFrame` | `0x2D6` | engine call `0x46D79C` (effect kind `0x70`'s handler `0x46D780`, area `0x31`) | kCallee | the party walked through `Area49_Zones`: a member entering a zone is turned to its direction and marked in the effect's mask and `Field_ScriptFlags2`; a marked member jumps (`Field_JumpSetUp`, the camera for the leader) or steps on, turning when the zone's direction changes; Field_Request 5 on another area: `Area49_SwapFlags2To3` |
| `0x409760` | `Area49_MemberZone` | `0x6A` | called by the effect frame | kCallee | the first of the seven zones holding member m's position, or `0xFF` |
| `0x4097D0` | `Area49_CellHook` | `0x7D` | `Area_CellHooks` entry for area `0x31` | kHook | a cell switch (x, z, pose) of nine: unless story flag `0x1C`, its flag toggled, `0x469FE0(0xF)`, sound `0x202`, al 1 |
| `0x409850` | `Area49_SetRowFlag3F` | `0x11` | choice 2 = handler 0 (PSX `0x801F3288`) | kHandler | the chapter row's flag `0x3F` |
| `0x409870` | `Area49_ClearCells` | `0x46` | handler 1 (PSX `0x801F32B0`) | kHandler | cells x `0x29..0x2B` of rows `0x5B`, `0x5C` zeroed |
| `0x4098C0` | `Area49_SetCells` | `0x58` | handler 2 (PSX `0x801F3328`) | kHandler | the same cells, `0xC0` / `0xA1` |
| `0x409920` | `Area49_Counter1If4` | `0x11` | handler 3 (PSX `0x801F33A0`) | kHandler | `0x904063` at 4: counter 1 = 1 |
| `0x409940` | `Area49_Counter2If4` | `0x11` | handler 4 (PSX `0x801F33C4`) | kHandler | `0x904064` at 4: counter 2 = 1 |
| `0x409960` | `Area49_ShiftCameraUp` | `0x10` | handler 5 (PSX `0x801F33E8`); also areas 8, 15, 26, 77, 80, 131 | kHandler | `Camera_ShiftY` + `0x14`, redraw |
| `0x409970` | `Area49_ShiftCameraDown` | `0x10` | handler 6 (PSX `0x801F3410`); also areas 8, 15, 26, 80, 131 | kHandler | `Camera_ShiftY` - `0x14`, redraw |
| `0x409980` | `Area49_SpawnAtMember0` | `0x42` | handler 8 (PSX `0x801F3454`) | kHandler | `Effect_Spawn(4, 0, Area49_EffectKinds[list 0], x, z)` at party record 0, which `Sprite_Current` is made (and left); the slot to its `+0xB` |
| `0x4099D0` | `Area49_SpawnAtMember1` | `0x42` | handler 9 (PSX `0x801F34D4`) | kHandler | the same for record 1 |
| `0x409A20` | `Area49_SpawnAtMember2` | `0x46` | handler 10 (PSX `0x801F3554`) | kHandler | the same for record 2 |
| `0x409A70` | `Area49_ClearCellsB` | `0x1A` | handler 11 (PSX `0x801F35D4`) | kHandler | cells (`0xB`, `0x55`), (`0xC`, `0x55`) zeroed |
| `0x409A90` | `Area49_SpawnEffect6C` | `0x89` | handler 12 (PSX `0x801F360C`) | kHandler | leader `+0x89` 5: an effect of kind `0x6C`, sound `0x209`, the two cells `0xC0`, counter 0 = 0, `MoveScript_Var7` 4 step `0x1E`; always the active member's bit 0 cleared |
| `0x409B20` | `Area49_SpawnKind3AtMember2` | `0x46` | handler 17 (PSX `0x801F376C`) | kHandler | `Area49_SpawnAtMember2` with kind 3 |
| `0x409B70` | `Area49_ClearCellsC` | `0x5F` | handler 18 (PSX `0x801F37EC`) | kHandler | cells x `0x4D`, `0x4E` of rows `0x22..0x25` zeroed |
| `0x409BD0` | `Area49_RunStep5` | `0x3B` | handler 19 (PSX `0x801F3884`) | kHandler | the active member's bit 0 cleared; leader `+0x89` 6: `ScriptFlags_Set40`, `MoveScript_Var7` 4 step 5 |
| `0x409C10` | `Area49_RunStep6` | `0x3B` | handler 20 (PSX `0x801F3908`) | kHandler | the same, step 6 |

Choices 2..22 are handlers 0..20. Choices 9 and 18 (`0x40F530`), 15
(`0x437CC0`, a shared `ret`), 16 (`0x414090`) and 17 (`0x414080`) are other
blocks' bodies. `Area49_ShiftCameraUp` / `Down` are the two **shared
bodies** of the band: area 49's block holds them and six other areas' tables
name them (the tool's `shared` rows); taken once, here, keyed by address.
The nine cell switches' story flags are 2 and 3 - the flags
`Area49_SwapFlags2To3` moves.

### Area 50 (descriptor `0x5FCD48`; PSX `0x801F3EAC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x409C50` | `Area50_ChoiceAsk82` | `0x3E` | choice 0 | kChoice | answer not 0: message `0x84` and the mark `0x9398CF` 6; 0: the byte `0x9045FB` at `0x1E` or more message `0x82`, else `0x83` and the mark |
| `0x409C90` | `Area50_SetCells` | `0x30` | handler 0 (PSX `0x801F2D48`) | kHandler | cells (8, `0x25..0x28`) set to `0x10` |
| `0x409CC0` | `Area50_Trigger43` | `0x14` | object trigger 43 | kCallee | `ScriptFlags_Set40`, tail kind 4 with sub-kind 4 (a register store); al 0 |

Choices 1..4 are `0x420850`, `0x420870` and `0x40CFA0` (twice), other blocks'.

### Area 51 (descriptor `0x5FD450`; PSX `0x801F3474`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x409CE0` | `Area51_MemberAtObject` | `0x46` | choice 1 = handler 0 (PSX `0x801F2C04`) | kHandler | `Party_MemberAt` the running object (margin its `+0x70`); a member, with `Field_ScriptFlags2` bit `0x1000` clear and `Field_Request` 0: `0x57C8A0(member, 1)` |
| `0x409D30` | `Area51_PlaceAtMember` | `0x45` | choice 2 = handler 1 (PSX `0x801F2C7C`) | kHandler | the running object put at the position of the object ten records before the active member, its `+0x3E` the ground there less `0x40` |
| `0x409D80` | `Area51_ChoiceMessage` | `0x15` | choice 0 | kChoice | message 1 for an answer, `0x14` for 0 |
| `0x409DA0` | `Area51_TintBackdrop` | `0x79` | init (PSX `0x801F2D00`) | kInit | `Area11_DimBackdrop`'s walk with its own colour `0x04214E73`, under Cond row 14's flags `0x13` set, `0x14` clear |

### Area 52 (descriptor `0x5FE080`; PSX `0x801F50A4`)

A block of 2 x 2 map cells the running object pushes: the object keeps the
map bytes it covers in `+0x18` (row z) and `+0x1C` (row z + 1), the low
byte column x and the next column x + 1; covered cells are `0x11` in the map
and `0x10` in the second byte layer (`0x572620`), and a cell left gets its
byte back and a layer of 0. x and z are the high words of the object's
position less `0x8000`.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x409E20` | `Area52_StashBlock` | `0xF5` | handler 0 (PSX `0x801F3280`) | kHandler | `Field_Request` 5: the script back 2; else the block under the object taken |
| `0x409F20` | `Area52_ShiftBlockZ` | `0x14D` | handler 1 (PSX `0x801F3414`) | kHandler | `Area52_BlockCell`; one row on in z (row z given back, row z + 2 taken), direction 5 |
| `0x40A070` | `Area52_ResetBlock` | `0x154` | handler 2 (PSX `0x801F35A0`) | kHandler | at exactly (`0x118000`, `0x298000`): the block given back and taken again at z `0x78000` |
| `0x40A1D0` | `Area52_MoveBlockA` | `0x8B` | handler 3 (PSX `0x801F37A8`) | kHandler | `Area52_BlockCell`; counter 3 `0xA` (x not `0x688000`): `Area52_DropColumn`, script + 6; `0x14`: `Area52_ShiftBlockX`, at x `0x858000` script + 3 and tail state `0x2D` |
| `0x40A260` | `Area52_MoveBlockB` | `0x8B` | handler 4 (PSX `0x801F3894`) | kHandler | the same, stop `0x668000` |
| `0x40A2F0` | `Area52_BlockCell` | `0x58` | called by handlers 1, 3, 4, 8 | kCallee | the block's (x, z) through two pointers; `Field_Request` 5: `Area52_RestoreBlock`; counter 3 at 0: script back 2, al 0; else al 1 |
| `0x40A350` | `Area52_ShiftBlockX` | `0xF6` | called by handlers 3, 4 | kCallee | one column on in x (column x given back, x + 2 taken), direction 3 |
| `0x40A450` | `Area52_DropColumn` | `0x64` | called by handlers 3, 4 | kCallee | column x + 1 given back from the high bytes, both halves << 8 |
| `0x40A4C0` | `Area52_RestoreBlock` | `0x6C` | called by `Area52_BlockCell` | kCallee | the whole block given back |
| `0x40A530` | `Area52_CellHook` | `0x90` | `Area_CellHooks` entry for area `0x34` | kHook | a cell switch (x, z, pose nibble) of four: `ScriptFlags_Set40`, tail kind 15 at the switch's state; a member in `Area52_Rects`: state `0xA`; else its flag toggled and sound `0x202`; al 1 |
| `0x40A5C0` | `Area52_PartyInRect` | `0xA4` | called by the cell hook | kCallee | 1 when a member stands in rectangle [the tail state not 0] |
| `0x40A670` | `Area52_TailBlock` | `0x2D0` | tail kind 15 | kTail | three timed scenes (states 0, `0x14`, `0x28`: counter 3, extra object 0 shown, a timer, then its flag `0x24` / `0x26` / `0x25` cleared and the object hidden); `0xA` message `0x39`; `0x2D..0x2F` the second effect, then cells (`0x86..0x87`, `0x11..0x12`) and on to `0x29` |
| `0x40A940` | `Area52_SpawnEffect48BJump` | `0x5` | called by the tail's state `0x2E` | kCallee | `jmp 0x40A9C0` |
| `0x40A950` | `Area52_ClearFlag25` | `0x10` | handler 5 (PSX `0x801F4268`) | kHandler | story flag `0x25` cleared |
| `0x40A960` | `Area52_SpawnEffect48A` | `0x57` | handler 6 (PSX `0x801F4290`) | kHandler | an effect of kind `0x48` at (`0x880000`, `0x110000`) on the ground, `+0xB` 8 |
| `0x40A9C0` | `Area52_SpawnEffect48B` | `0x57` | handler 7 (PSX `0x801F431C`) | kHandler | the same at (`0x890000`, `0x118000`), `+0xB` `0x10` |
| `0x40AA20` | `Area52_ShiftBlockXBack` | `0xD3` | handler 8 (PSX `0x801F43B0`) | kHandler | `Area52_BlockCell` (answer unread); column x - 1 taken into the low bytes (after `Area52_DropColumn` made room), direction 7 |

## 2. Ours

`src/game/area_w1c.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
four raw addresses of section 9). The group's own callees are called the
same way (`AH_CALL(Area52_BlockCell)` ...), so the fuzz stands a recorder in
for each and every function is tested alone. Shapes that repeat are one
helper with the function's constants: area 48's two script-object moves
(`MoveScriptObject`) and its count switch (`ByCount`), area 49's four
spawns (`SpawnAtMember`) and two run steps (`RunStep`), area 52's column
take / give-back (`StashColumn`, `RestoreColumn`), its two block movers
(`MoveBlock`), its two effect spawns (`SpawnEffect48`) and the tail's timed
scenes (`BeginTimed`, `CountDownBelow4`, `CountDownToZero`, `EndTimed`).
Kept as the originals: every re-read after a call (`Sprite_Current`,
`MoveScript_Object`, `Field_ActiveMember`, the sub-kind, counter 3, the
object's x), a pointer taken before a call and written through after it
(area 52's `+0x18` / `+0x1C`), the order of every call, the 16-bit compares
of the hooks' words, the byte masks of the effect frame's member bits (bits
past 7 never set), `Area52_ShiftBlockXBack`'s unread answer, and the unchecked
table reads of section 6.

## 3. The fuzz

`BOF3X_SHADOW=area_w1c` (`src/game/area_w1c_fuzz.cpp`): five `Run`s under
the one shadow name, one per area with its `Group::area` (48..52), 6,000
rounds per function, the real descriptors and tables in place.

- **Callees the group lists:** `AreaMap_SetByte` and `0x572620` with masks
  of words x, z and a value byte (the originals push dwords whose upper
  halves are stack garbage; both callees read `movsx` words and a byte);
  `Effect_FindFree` `kByte 0xFF..0x03` (a slot inside the group's four
  effect records, or none); `Effect_Spawn`, `Party_MemberAt` `kByte
  0xFE..0x02`; `MoveScript_ObjectKind` answering a whole small int a caller
  compares with 2; `MoveCmd_Move` (Capcom's), `MoveCmd_MoveKind2`,
  `ScriptFlags_Set40` / `Clear40`, `Field_JumpSetUp` / `Camera`;
  `0x57C160`, `0x57C8A0`, `0x469FE0` by raw address; the group's own
  (`Area49_SwapFlags2To3`, `Area49_MemberZone` `kByte 0xFF..0x06`,
  `Area52_BlockCell`, `Area52_ShiftBlockX` / `DropColumn` / `RestoreBlock`
  logging their words' high halves, `Area52_PartyInRect`,
  `Area52_SpawnEffect48B` / `...Jump`).
- **Louder stand-ins** (an `effect`, half the time, from `Noise`): after
  each call whose caller reads a cell again, that cell moves -
  `Party_DropIn` the sub-kind, `MoveCmd_Move` / `MoveKind2` the script
  object, `Sound_PlayEffect` the active member, `AreaMap_ByteAt` /
  `AreaMap_Elevation` the running object, `Area52_BlockCell` counter 3 (and
  it writes the object's x and z less `0x8000`, or noise, through its two
  pointers), `Area52_ShiftBlockX` the object's x (to `0x858000` half of
  those).
- **Regions beyond the field frame:** `Effect_Objects` records 0..3, the
  active member and script object pointers, the chapter row pointer, the
  mark `0x9398CF`, area 48's two bytes, `0x9045FB`, `Camera_ShiftY`,
  `MoveScript_EffectState` (24 bytes), area 52's extra objects (16 bytes of
  `.data`), and two dwords of the fuzz's own that `Area52_BlockCell`'s clone
  writes through (32 regions, 16,786 bytes).
- **Every round:** `Field_ActiveMember` at one of the four party objects, a
  field object, a party record or the running object; `MoveScript_Object`
  at a field object or a party record.
- **Seeds:** the choice answer at each value a handler tests (0..3), its
  sign edge and above; area 48's count at each value its switches test and
  the byte edges (`0x3F`, `0x40`, `0x41`, `0xC0`, `0xFF`: the `<< 2` wraps);
  `MoveScript_EffectState[leader +0x89]` 1 half the time; the sub-kind
  0..3, `0xFF`; the arrive hook's words at each doorway, on it and one step
  off each edge, with a high byte (`0x117`: the compares are 16-bit), z
  exactly `0x58000` / `0xA8000`; the step hook's x at `0x4D8000` and beside;
  the cell switches drawn by the seed (a record of the table, read in place,
  exactly or with one of x, z, the pose off by one - the seed sets the
  pose, the arguments follow, since the harness draws arguments after it
  has captured the round's state); the effect frame with `Sprite_Current` an
  effect record, `Field_Request` 5 and the pending area equal or not, the
  members' `+9` at 0 half the time, the member count 0 a tenth of the time;
  each member placed on or beside each edge of a zone; area 50's byte at
  `0x1E` and beside; area 51's flag bit `0x1000`, `Field_Request`, the
  active member ten records into the field objects, the header chain (area
  11's builder: 0..10 entries of kinds around `0x81`, steps 1..3); area 52's
  object exactly at the reset point or one word off, counter 3 at `0xA` /
  `0x14` and beside, the stops and `0x858000`; the rectangles' edges for
  each member; the tail's state at every value the byte table reaches, its
  neighbours and out of range, with the cell that state waits on (the timer
  at 1..4 or at 1, `Field_Request`, counter 3 at `0x1E` / `0x28`) set to its
  value two times in three (step-paired).
- **The group's disturbance** (from the hash it is given): the tail state
  and sub-kind, counter 3 (at a value some state waits on, or any), the
  active member and script object pointers, area 48's count, the timer, the
  answer byte.

**Result (in this worktree):** 342,000 rounds over the 57 functions (6,000
each), 690,785 calls to the stand-ins, 0 mismatches, 16,786 bytes of
state (32 regions) and the log compared. Coverage: every callee each function can reach was called - e.g. the cell switches' toggles 659 (area 49) / 677 (area 52), `Area52_PartyInRect` 1,951, the tail's `Msg_OpenScript` 297 and its jump thunk 194, `Area49_MemberZone` 932, `Field_JumpSetUp` 182, `Sprite_EnsureAnimation` 245, `Party_DropIn` 2,117, `MoveCmd_MoveKind2` 1,645, `0x57C8A0` 926, `Area52_ShiftBlockX` 1,548 / `DropColumn` 1,342 / `RestoreBlock` 1,012.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4158 ours`, 384 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w1c.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w1c.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w1c`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all five runs). **185 planted, 185 refused by a count (exit 3)**, no hang, no fault, none equivalent. Three anchors of the first run matched twice (B40, E4, E6) and were planted again on longer anchors. Every one of the 57 functions has at least one control of its own; a control in a shared helper lists every function it refused in. The thinnest is B25 (4 rounds): the effect frame's re-read of `Sprite_Current` after the zone's flag test, reached only through the harness's own disturbance (no louder stand-in moves `Sprite_Current` after `Flags_Test`); the rest need 51 rounds or more.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `Area48_ChoiceCounterBOr1` | bit 1 tested for bit 0 | 2382 |
| A2 | `Area48_ChoiceCounterBOr1` | counter 0xC | 2732 |
| A3 | `Area48_ChoiceCounterBOr1` | MoveScript_Var7 1 | 3264 |
| A4 | `Area48_ToggleFlagD` | flag 0xE | 963 |
| A5 | `Area48_ToggleFlagD` | state 2 for 1 | 1516 |
| A6 | `Area48_BumpCount` | up to 9 | 52 |
| A7 | `Area48_BumpCount` | sound 0x20E | 989 |
| A8 | `Area48_BumpCount` | member bit kept | 3025 |
| A9 | `MoveScriptObject (48 x2)` | count << 1 | Area48_MoveScriptObject5 5714, Area48_MoveScriptObject1 5719 |
| A10 | `MoveScriptObject (48 x2)` | kind 3 for 2 | Area48_MoveScriptObject5 1670, Area48_MoveScriptObject1 1672 |
| A11 | `MoveScriptObject (48 x2)` | bit 0x20 for 0x40 | Area48_MoveScriptObject5 608, Area48_MoveScriptObject1 614 |
| A12 | `MoveScriptObject (48 x2)` | bit 0x40 for 0x80 | Area48_MoveScriptObject5 3187, Area48_MoveScriptObject1 3216 |
| A13 | `MoveScriptObject (48 x2)` | word 0x481 | Area48_MoveScriptObject5 6000, Area48_MoveScriptObject1 6000 |
| A14 | `MoveScriptObject (48 x2)` | script object read before MoveCmd_Move | Area48_MoveScriptObject5 1465, Area48_MoveScriptObject1 1411 |
| A15 | `MoveScriptObject (48 x2)` | the count tested, not the stored step | Area48_MoveScriptObject5 618, Area48_MoveScriptObject1 571 |
| A16 | `Area48_MoveScriptObject5` | direction 6 | 5980 |
| A17 | `Area48_MoveScriptObject1` | direction 2 | 5974 |
| A18 | `Area48_PoseByCount` | other 8 | 5036 |
| A19 | `ByCount (48 x2)` | case 7 for 6 | Area48_PoseByCount 638, Area48_TailDropIn 61 |
| A20 | `Area48_PoseByCount` | word 0xFFFF | 6000 |
| A21 | `Area48_ResetCount` | bit 1 | 4500 |
| A22 | `Area48_ResetCount` | count 1 | 6000 |
| A23 | `Area48_TailDropIn` | other entry 7 | 584 |
| A24 | `Area48_TailDropIn` | sub-kind not read again | 94 |
| A25 | `Area48_TailDropIn` | Party_DropIn(3) for 2 | 778 |
| A26 | `Area48_TailDropIn` | sub-kind not cleared | 5375 |
| A27 | `Area48_ArriveHook` | x 0x18 | 1021 |
| A28 | `Area48_ArriveHook` | z span 3 | 60 |
| A29 | `Area48_ArriveHook` | z 0x58001 | 90 |
| A30 | `Area48_ArriveHook` | z high word 0xC | 184 |
| A31 | `Area48_ArriveHook` | sub-kind 3 for 2 | 164 |
| A32 | `Area48_ArriveHook` | tail kind 4 | 1334 |
| A33 | `Area48_ArriveHook` | x high word as a byte | 204 |
| A34 | `Area48_ArriveHook` | answers 1 | 6000 |
| A35 | `Area48_InitSound` | Cond_ByteFD 4 | 1995 |
| B1 | `Area49_ChoiceMessageFlag3B` | flag 0x3C | 477 |
| B2 | `Area49_ChoiceMessageFlag3B` | the answer unsigned | 2276 |
| B3 | `Area49_ChoiceMessage` | table + 6 | 5946 |
| B4 | `Area49_StepHook` | x 0x4D8001 | 674 |
| B5 | `Area49_StepHook` | Cond_ByteFD 0 or 1 | 216 |
| B6 | `Area49_StepHook` | answers 1 | 6000 |
| B7 | `Area49_SwapFlags2To3` | flag 1 cleared | 6000 |
| B8 | `Area49_SwapFlags2To3` | flag 4 set | 6000 |
| B9 | `Area49_EffectFrame` | the area test inverted | 1327 |
| B10 | `Area49_EffectFrame` | flags & 0xEFFF | 3563 |
| B11 | `Area49_EffectFrame` | row 9 flag 0x26 | 4465 |
| B12 | `Area49_EffectFrame` | row 5 flag 0x3B | 2351 |
| B13 | `Area49_EffectFrame` | Field_State +0x128 = 3 | 240 |
| B14 | `Area49_EffectFrame` | direction from byte 5 | 237 |
| B15 | `Area49_EffectFrame` | turned by 2 | 169 |
| B16 | `Area49_EffectFrame` | camera for member 1 | 155 |
| B17 | `Area49_EffectFrame` | +9 = 7 - +9 | 199 |
| B18 | `Area49_EffectFrame` | +0x10 not negated | 201 |
| B19 | `Area49_EffectFrame` | bit 0x1000 set | 371 |
| B20 | `Area49_EffectFrame` | the count not read again | 291 |
| B21 | `Area49_EffectFrame` | the last and on Field_Request not 5 | 383 |
| B22 | `Area49_EffectFrame` | Sprite_Current not put back | 4465 |
| B23 | `Area49_EffectFrame` | the next bit cleared in flags2 | 1289 |
| B24 | `Area49_EffectFrame` | Field_ScriptFlags not tested | 83 |
| B25 | `Area49_EffectFrame` | Sprite_Current re-read skipped before the turn | 4 |
| B26 | `Area49_MemberZone` | x1 exclusive | 264 |
| B27 | `Area49_MemberZone` | z0 from byte 0 | 474 |
| B28 | `Area49_MemberZone` | none 0xFE | 5220 |
| B29 | `Area49_MemberZone` | x0 exclusive | 260 |
| B30 | `Area49_CellHook` | pose from byte 3 | 2115 |
| B31 | `Area49_CellHook` | flag 0x1D | 2021 |
| B32 | `Area49_CellHook` | toggled flag from byte 2 | 659 |
| B33 | `Area49_CellHook` | 0x469FE0(0xE) | 659 |
| B34 | `Area49_CellHook` | eight records | 3979 |
| B35 | `Area49_SetRowFlag3F` | flag 0x3E | 6000 |
| B36 | `Area49_ClearCells` | x to 0x2A | 6000 |
| B37 | `Area49_SetCells` | row 0x5C 0xA0 | 6000 |
| B38 | `Area49_Counter1If4` | list byte 3 | 3006 |
| B39 | `Area49_Counter2If4` | counter 2 = 2 | 3029 |
| B41 | `Area49_ShiftCameraDown` | redraw 1 | 6000 |
| B42 | `SpawnAtMember (49 x4)` | x and z swapped | Area49_SpawnAtMember0 6000, Area49_SpawnAtMember1 6000, Area49_SpawnAtMember2 6000, Area49_SpawnKind3AtMember2 6000 |
| B43 | `SpawnAtMember (49 x4)` | the slot to +0xA | Area49_SpawnAtMember0 4795, Area49_SpawnAtMember1 4870, Area49_SpawnAtMember2 4803, Area49_SpawnKind3AtMember2 4810 |
| B44 | `SpawnAtMember (49 x4)` | list 0 for every member | Area49_SpawnAtMember1 4873, Area49_SpawnAtMember2 4907, Area49_SpawnKind3AtMember2 4815 |
| B45 | `SpawnAtMember (49 x4)` | Sprite_Current read before the call | Area49_SpawnAtMember0 166, Area49_SpawnAtMember1 176, Area49_SpawnAtMember2 156, Area49_SpawnKind3AtMember2 169 |
| B46 | `Area49_SpawnAtMember0` | kind 5 | 6000 |
| B47 | `Area49_SpawnAtMember1` | kind 3 | 6000 |
| B48 | `Area49_SpawnAtMember2` | kind 5 | 6000 |
| B49 | `Area49_ClearCellsB` | x 0xD | 6000 |
| B50 | `Area49_SpawnEffect6C` | kind 0x6D | 935 |
| B51 | `Area49_SpawnEffect6C` | pose 6 | 1775 |
| B52 | `Area49_SpawnEffect6C` | step 0x1F | 1180 |
| B53 | `Area49_SpawnEffect6C` | member read before the sound | 388 |
| B54 | `Area49_SpawnKind3AtMember2` | kind 4 | 6000 |
| B55 | `Area49_ClearCellsC` | z to 0x24 | 6000 |
| B56 | `RunStep (49 x2)` | pose 5 | Area49_RunStep5 1863, Area49_RunStep6 1860 |
| B57 | `RunStep (49 x2)` | Var7 3 | Area49_RunStep5 1261, Area49_RunStep6 1250 |
| B58 | `Area49_RunStep5` | step 4 | 1261 |
| B59 | `Area49_RunStep6` | step 7 | 1250 |
| C1 | `Area50_ChoiceAsk82` | > 0x1E | 51 |
| C2 | `Area50_ChoiceAsk82` | message 0x85 | 5522 |
| C3 | `Area50_ChoiceAsk82` | mark 7 | 98 |
| C4 | `Area50_SetCells` | value 0x11 | 6000 |
| C5 | `Area50_Trigger43` | sub-kind 5 | 6000 |
| C6 | `Area50_Trigger43` | answers 1 | 6000 |
| D1 | `Area51_MemberAtObject` | bit 0x800 | 627 |
| D2 | `Area51_MemberAtObject` | state 2 | 926 |
| D3 | `Area51_MemberAtObject` | margin +0x6C | 6000 |
| D4 | `Area51_MemberAtObject` | Field_Request 1 passes | 482 |
| D5 | `Area51_PlaceAtMember` | nine records back | 4336 |
| D6 | `Area51_PlaceAtMember` | less 0x3F | 6000 |
| D7 | `Area51_PlaceAtMember` | elevation at (z, x) | 4316 |
| D8 | `Area51_PlaceAtMember` | Sprite_Current not read again after the call | 2674 |
| D9 | `Area51_ChoiceMessage` | message 0x15 | 439 |
| D10 | `Area51_TintBackdrop` | flag 0x12 | 6000 |
| D11 | `Area51_TintBackdrop` | colour + 0xC off by one | 868 |
| D12 | `Area51_TintBackdrop` | kind 0x80 | 1001 |
| E1 | `Area52_StashBlock` | script position back 3 | 748 |
| E2 | `Area52_StashBlock` | z less 0x10000 | 2701 |
| E3 | `Area52_StashBlock` | +0x1C not cleared | 5252 |
| E5 | `StashColumn (52 x2)` | map 0x12 on row z + 1 | Area52_StashBlock 5252, Area52_ResetBlock 2007 |
| E7 | `RestoreColumn (52 x3)` | row z + 1 shifted one more | Area52_ResetBlock 2007, Area52_DropColumn 5972, Area52_RestoreBlock 6000 |
| E8 | `RestoreColumn (52 x3)` | layer 1 | Area52_ResetBlock 2007, Area52_DropColumn 6000, Area52_RestoreBlock 6000 |
| E9 | `Area52_ShiftBlockZ` | z + 1 | 3998 |
| E10 | `Area52_ShiftBlockZ` | +0x18 kept | 3998 |
| E11 | `Area52_ShiftBlockZ` | direction 6 | 3974 |
| E12 | `Area52_ShiftBlockZ` | byte +0x19 | 3978 |
| E13 | `Area52_ResetBlock` | x 0x118001 | 2337 |
| E14 | `Area52_ResetBlock` | z 0x88000 | 2007 |
| E15 | `Area52_ResetBlock` | row 0x2A given back first | 2007 |
| E16 | `Area52_MoveBlockA` | stop 0x688001 | 266 |
| E17 | `Area52_MoveBlockB` | stop 0x668001 | 264 |
| E18 | `MoveBlock (52 x2)` | counter 0xB | Area52_MoveBlockA 1236, Area52_MoveBlockB 1222 |
| E19 | `MoveBlock (52 x2)` | position + 5 | Area52_MoveBlockA 668, Area52_MoveBlockB 674 |
| E20 | `MoveBlock (52 x2)` | state 0x2C | Area52_MoveBlockA 264, Area52_MoveBlockB 241 |
| E21 | `MoveBlock (52 x2)` | x read before Area52_ShiftBlockX | Area52_MoveBlockA 200, Area52_MoveBlockB 184 |
| E22 | `MoveBlock (52 x2)` | counter read before Area52_BlockCell | Area52_MoveBlockA 1031, Area52_MoveBlockB 1054 |
| E23 | `MoveBlock (52 x2)` | x and z swapped to Area52_DropColumn | Area52_MoveBlockA 668, Area52_MoveBlockB 674 |
| E24 | `Area52_BlockCell` | x less 0x8001 | 6000 |
| E25 | `Area52_BlockCell` | z a row on to Area52_RestoreBlock | 1012 |
| E26 | `Area52_BlockCell` | answers 2 | 4989 |
| E27 | `Area52_BlockCell` | position back 1 | 1011 |
| E28 | `Area52_BlockCell` | Field_Request 4 | 1977 |
| E29 | `Area52_ShiftBlockX` | column x + 1 | 6000 |
| E30 | `Area52_ShiftBlockX` | direction 4 | 5970 |
| E31 | `Area52_ShiftBlockX` | +0x1C shifted 7 | 6000 |
| E32 | `Area52_ShiftBlockX` | +0x18 shifted logically | 2963 |
| E33 | `Area52_DropColumn` | column x + 2 | 6000 |
| E34 | `Area52_DropColumn` | +0x1C << 7 | 6000 |
| E35 | `Area52_RestoreBlock` | second column at shift 4 | 6000 |
| E36 | `Area52_CellHook` | the leader pose masked instead | 322 |
| E37 | `Area52_CellHook` | tail kind 0xE | 1951 |
| E38 | `Area52_CellHook` | state from byte 3 | 664 |
| E39 | `Area52_CellHook` | state 0xB in a rectangle | 1274 |
| E40 | `Area52_CellHook` | answers 0 after the toggle | 677 |
| E41 | `Area52_CellHook` | three records | 4049 |
| E42 | `Area52_PartyInRect` | the rectangle inverted | 1641 |
| E43 | `Area52_PartyInRect` | width inclusive | 443 |
| E44 | `Area52_PartyInRect` | z from +0x38 | 1641 |
| E45 | `Area52_PartyInRect` | no members answers 1 | 623 |
| E46 | `Area52_TailBlock` | counter 2 at state 0 | 274 |
| E47 | `Area52_TailBlock` | state 0x14 to 0x16 | 304 |
| E48 | `Area52_TailBlock` | counter 0x15 at state 0x28 | 275 |
| E49 | `BeginTimed (52 tail)` | timer 0x11 | Area52_TailBlock 853 |
| E50 | `BeginTimed (52 tail)` | extra object byte 1 | Area52_TailBlock 853 |
| E51 | `CountDownBelow4 (52 tail)` | below 5 | Area52_TailBlock 78 |
| E52 | `EndTimed (52 tail)` | extra object 0xFE | Area52_TailBlock 602 |
| E53 | `Area52_TailBlock` | flag 0x23 | 193 |
| E54 | `Area52_TailBlock` | flag 0x27 | 198 |
| E55 | `Area52_TailBlock` | flag 0x24 at 0x2A | 211 |
| E56 | `Area52_TailBlock` | message 0x3A | 297 |
| E57 | `Area52_TailBlock` | waits on Field_Request 3 | 166 |
| E58 | `Area52_TailBlock` | counter 0x1F at 0x2E | 218 |
| E59 | `Area52_TailBlock` | map 0x11 at (0x87, 0x12) | 183 |
| E60 | `Area52_TailBlock` | timer 0xB at 0x2F | 183 |
| E61 | `Area52_TailBlock` | counter 1 at 0x2D | 292 |
| E62 | `Area52_TailBlock` | state 0x2B counted down as 0x2A | 152 |
| E63 | `Area52_SpawnEffect48BJump` | called twice | 6000 |
| E64 | `Area52_ClearFlag25` | flag 0x26 | 6000 |
| E65 | `Area52_SpawnEffect48A` | +0xB = 9 | 4790 |
| E66 | `Area52_SpawnEffect48B` | z 0x118001 | 4826 |
| E67 | `SpawnEffect48 (52 x2)` | kind 0x49 | Area52_SpawnEffect48A 4790, Area52_SpawnEffect48B 4826 |
| E68 | `SpawnEffect48 (52 x2)` | elevation << 9 | Area52_SpawnEffect48A 4790, Area52_SpawnEffect48B 4826 |
| E69 | `SpawnEffect48 (52 x2)` | elevation zero-extended | Area52_SpawnEffect48A 2345, Area52_SpawnEffect48B 2378 |
| E70 | `Area52_ShiftBlockXBack` | column x + 1 | 6000 |
| E71 | `Area52_ShiftBlockXBack` | direction 8 | 5966 |
| E72 | `Area52_ShiftBlockXBack` | row z + 1 in the high byte | 4013 |
| E73 | `Area52_ShiftBlockXBack` | the answer tested | 1949 |
| B40 | `Area49_ShiftCameraUp` | + 0x15 | 6000 |
| E4 | `StashColumn (52 x2)` | layer 0x11 | Area52_StashBlock 5252, Area52_ResetBlock 2007 |
| E6 | `StashColumn (52 x2)` | the object read after the map read | Area52_StashBlock 2618, Area52_ResetBlock 1000 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (16): `Area48_Choices`
`0x5F988C` (7), `Area48_Handlers` `0x5F9890` (6), `Area49_EffectKinds`
`0x5F98F0` (8 bytes, by analogy with `Area11_EffectKinds`: the index is a
party list byte), `Area49_Zones` `0x5FBB70` (7 records of 6 bytes),
`Area49_Choices` `0x5FBB9C` (23), `Area49_Handlers` `0x5FBBA4` (21),
`Area49_ChoiceMessages` `0x5FBC3C` (4 words), `Area49_CellSwitches`
`0x5FBC44` (9 records of 4 bytes), `Area50_Choices` `0x5FCB68` (5),
`Area50_Handlers` `0x5FCD40` (1), `Area51_Choices` `0x5FD440` (3),
`Area51_Handlers` `0x5FD444` (2), `Area52_ExtraObjects` `0x5FE048` (16
bytes: the descriptor's `+0x08`, two entries of 8), `Area52_Handlers`
`0x5FE058` (9), `Area52_Rects` `0x5FE0E8` (2 records of 4 bytes),
`Area52_CellSwitches` `0x5FE0F0` (4 records of 5 bytes). A descriptor's
`+0x3C` array is the tail of its `+0x34` table in areas 48, 49 and 51 (as
[`area-rows.md`](area-rows.md) section 2 found generally). `0x5F98F0` lies
after area 48's descriptor and is read only by area 49's code (the tool's
data-block rule moves it to 49). Area 52's descriptor `+0x08` points into
its own data (`0x5FE048`), not the `0x675xxx` pool ART's census found for
the others. The tail's byte table (`0x40A910`, `0x30` bytes) and jump table
(`0x40A8D4`, 15 entries) are in `.text`, inside the function's extent.

## 6. Latent defects

Described, not fixed; each is kept faithfully (the reads stay inside
`.data`, so none can fault):

- **Unchecked indexes into data.** `Area49_ChoiceMessageFlag3B` /
  `Area49_ChoiceMessage` index `Area49_ChoiceMessages` by the s8 answer
  (a negative answer reads before it); `Area48_ToggleFlagD` /
  `Area48_BumpCount` index the 24-byte `MoveScript_EffectState` by the
  leader's `+0x89` byte; `Area49_SpawnAtMember*` index
  `Area49_EffectKinds` by a party list byte.
- **`Area51_TintBackdrop`'s header walk never ends** on an entry of another
  kind with a step byte of 0, as `Area11_DimBackdrop`'s (ARH): read, not
  run; the fuzz builds steps of 1..3.
- **`Area52_TailBlock` writes the area's own `.data`**: byte `+1` of the
  descriptor's first extra-object entry (`0x5FE049`) is set 0 when a timed
  scene begins and `0xFF` when it ends. On the PSX the overlay is reloaded
  with the area; here the image keeps the value until the next write, so
  whatever reads that entry on a later entry to area 52 sees the last
  scene's value. Not measured in play.
- **`Area52_ShiftBlockXBack` ignores `Area52_BlockCell`'s answer**: with
  counter 3 at 0 the helper backs the script up by 2 and answers 0, and the
  handler takes the column and moves the object anyway (the other three
  callers return). What the script does then is not read.
- **`Area49_EffectFrame` leaves `Field_State`** at the last member's record
  (it puts `Sprite_Current` back, not `Field_State`); its member bits are
  byte masks, so a member count above 8 would never mark or clear the rest
  (the count is 1..3 in the field).

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D136 (reads and writes by an unchecked byte or count),
D144 (the unbounded walks), D145 (`.data` kept across visits), D161 (small
slips) in [`known-defects.md`](known-defects.md).

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 57 starts, extents, call sites,
  the tail's jump table (`0x40A670`: 15 entries at `+0x264`, the byte table
  at `+0x2A0` read in place), and the shape its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding (or the tail's two tables).
- **The tool's first gap** (a start with no padding or `ret` before it):
  none in this band; `0x40A940`, after the tail's byte table, is in the
  tool's list (from `pc_funcs.json`).
- **The tool's second gap** (a tail kind armed through a register):
  `Area50_Trigger43` arms kind 4 by `mov al, 4 ... mov [0x9039F3], al`;
  kind 4 is engine code (`0x56D930`), so no function of this band is left
  unrooted by it, but the tool's list of arming sites misses this one.
- **`0x40A940` is a 5-byte function of its own**, a `jmp` to handler 7,
  reached only from the tail's state `0x2E`; taken as
  `Area52_SpawnEffect48BJump`, calling `Area52_SpawnEffect48B` by name.

## 8. What reaches it

- **No recorded route reaches the band** (`analysis/hidden_reached_*.json`,
  `pc_hidden_reached.json`: none of `0x408FF0..0x40AB00`; `area_funcs.tsv`'s
  live column is empty for all 57). Every function is reached only in play:
  the choices when the area's message box asks, the handlers from its
  movement scripts (ops `03` / `DE`), the hooks on a step, an arrival or a
  cell in the area, the tails once armed, the init on entry.
- **Gap functions** (reached by no area table in the tool's walk):
  `0x409480` `Area49_EffectFrame` - engine code `0x46D780`, effect kind
  `0x70`'s handler (`Effect_KindHandlers` + `0x1C0`, the dword at
  `0x655510`), calls it when `Game_AreaNumber` is `0x31` (the same switch
  calls `0x41A0A0`, `0x41A410`, `0x426810`, `0x4276D0` for areas `0x75`,
  `0x76`, `0xA9`, `0xAB`); `0x409760` `Area49_MemberZone` - only from
  `0x409480`; `0x409CC0` `Area50_Trigger43` - `Field_ObjectTriggers` id 43
  (by an object's `+0x86` from area data; no immediate store of 43 read).
  Each sits in the block of the area named and is attributed to it.

## 9. Calls across groups

- **Raw addresses nobody owns** (engine; section 3 of the round doc lists
  such callees for SX; these four are not on SX's list), in
  `area_w1c_callees.h`: `0x57C160` (a story-flag toggle, beside `Flags_Set`
  / `Flags_Clear`), `0x572620` (the area block's second byte layer, beside
  `AreaMap_SetByte`), `0x57C8A0` (a party record's state bytes), `0x469FE0`
  (an effect of kind 4 and story flag `0x1C`).
- **By name, Capcom's:** `MoveCmd_Move`, `Effect_Spawn`.
- **By name, ours:** `AreaMap_SetByte`, `AreaMap_ByteAt`,
  `AreaMap_Elevation`, `Effect_FindFree`, `Party_MemberAt`,
  `Party_DropIn`, `Sound_PlayEffect`, `ScriptFlags_Set40` / `Clear40`,
  `Flags_Set` / `Clear` / `Test`, `Msg_OpenScript`, `MoveScript_ObjectKind`,
  `MoveCmd_MoveKind2`, `Field_JumpSetUp`, `Field_JumpCamera`,
  `Field_LeaderStepTick`, `Sprite_ScriptTick`, `Sprite_EnsureAnimation`. No
  harness edit; no `AH_THEIRS` moved.
- **Inbound:** `0x46D79C` (engine, unnamed) calls `Area49_EffectFrame` -
  for the rebinding pass. `Area49_ShiftCameraUp` / `Down` are named by
  areas 8, 15, 26, 77, 80 and 131's tables (read in place by the engine; no
  caller to rebind).
- `analysis/calltrace/entries_logic.txt`: 51 lines appended under a
  `# group AR1C` comment; six (`0x409440`, `0x409460`, `0x409480`,
  `0x40A2F0`, `0x40A350`, `0x40A450`) were already there with the same
  extent; the hosts `00409340 F7`, `00409760 B8B`, `0040A4C0 100` and
  `0040A5C0 E48` run over others (the consolidation keeps the smaller).
