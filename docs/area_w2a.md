# World 2, areas 76..82 and 84: the band `0x40EB90..0x40F720`

**Status:** IN PROGRESS (2026-09-28) - 50 functions ours
(`src/game/area_w2a.cpp`, shadow name `area_w2a`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 300,000 rounds (in this worktree); 145 controls planted, 144 refused by a count, 1 equivalent with its near variant refused (section 4).
Fuzz only: no recorded route reaches the band (section 8). No divergence;
two indexes past a table abort where the original would jump or read past
it (section 6).

Group AR2A of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 12). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
50 starts, none ours before, **50 taken**; no start dropped, none added
(section 7). Areas 76..82 and 84 have code here; area 83's descriptor
(`0x610D10`) names no choice, handler, init or hook, and no other root
reaches code of its own. None of the band's areas is a world-map area.

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`analysis/area_funcs.tsv`) and
agrees with the reading; the clone tables are capstone's call sites and the
two jump tables, checked against `area_rows.py --clones`'s rows (section 7).
What an area *is* in the story is not read here. The PSX twins are the
sibling's `names/area_records.toml` (descriptor handlers only for these
areas; choices, hooks and triggers have no pairing there, and area 76 has no
record).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the message box's answer byte
`0x7DEE67` in, the message word `0x7DEE48` read after), `kHandler` a `+0x3C`
handler (movement-script ops `03` / `DE`), `kState` an entry of an area's
own state table, `kHook` a step or cell hook `(x, z)` answering in `al`,
`kCallee` a function called directly (by the group's own code, or as an
object trigger `(object, 0x904030)` answering in `al`). A function that is
both a choice and a handler is fuzzed as a handler (none of them touches
the message word). Counters 0..3 are the movement script's `0x903848..0x90384B`.

### Area 76 (descriptor `0x60AC78`; no PSX record)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40EB90` | `Area76_StepDisarmTail5` | `0x1D` | `Area_StepHook`'s case for area `0x4C` | kHook | with the mode tail kind `0x9039F3` at 5 (engine code `0x56DA10`), the kind, its state `0x9039F4` and the byte `0x9039F1` cleared; al 0 |

`Area_StepHook` calls this case **with no arguments** (the one case of 38
that does; `event_ops.cpp`'s `kNoArgsCase`): it reads none. Ours takes none;
the harness calls it with two words, which cdecl leaves unread.

### Area 77 (descriptor `0x60BB88`; PSX `0x801F6AD0`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40EBB0` | `Area77_CellHook` | `0x92` | `Area_CellHooks` entry for area `0x4D` | kHook | Cond row 6's flag `0x2C` set: 0; a cell switch (x, z, the leader's pose) of `Area77_CellSwitches`' two: unless story flag `0x1C`, its flag toggled (`0x57C160`), sound `0x201`, `0x469FE0(0xF)`, al 1 |
| `0x40EC50` | `Area77_Trigger34` | `0x1D` | object trigger 34 | kCallee | `ScriptFlags_Set40`; tail kind `0x2C` (engine code `0x56DE50`), state 0, sub-kind 7; al 0 |
| `0x40EC70` | `Area77_ChoiceMessage` | `0x17` | choice 0 | kChoice | the message word from `Area77_ChoiceMessages` by the s8 answer |
| `0x40EC90` | `Area77_ChoiceCounter0` | `0x28` | choice 1 | kChoice | message `0xFFFF`; answer 0: counter 0 = 1, 1: 0 |
| `0x40ECC0` | `Area77_SetByteFE` | `0x13` | choice 2 = handler 0 (PSX `0x801F5A58`) | kHandler | `Cond_ByteFE` 1, sound `0x202` |
| `0x40ECE0` | `Area77_ClearByteFE` | `0x13` | handler 1 (PSX `0x801F5A84`) | kHandler | `Cond_ByteFE` 0, sound `0x202` |
| `0x40ED00` | `Area77_Spawn3AtMember0ListA` | `0x42` | handler 2 (PSX `0x801F5AAC`) | kHandler | `Effect_Spawn(3, 0, Area77_EffectKindsA[list 0], x, z)` at party record 0, which `Sprite_Current` is made (and left); the slot to its `+0xB` |
| `0x40ED50` | `Area77_Spawn4AtMember0ListB` | `0x42` | handler 3 (PSX `0x801F5B2C`) | kHandler | the same, kind 4, list B |
| `0x40EDA0` | `Area77_RemoveItem47` | `0x11` | handler 4 (PSX `0x801F5BAC`) | kHandler | `Inventory_Remove(1, 0x47, 1)` (a fourth word, 0, pushed and never read) |
| `0x40EDC0` | `Area77_SetFlag4` | `0x10` | handler 5 (PSX `0x801F5BD8`) | kHandler | story flag 4 set |
| `0x40EDD0` | `Area77_Spawn2AtMember0ListC` | `0x42` | handler 6 (PSX `0x801F5C00`) | kHandler | the spawn, kind 2, record 0, list C |
| `0x40EE20` | `Area77_Spawn3AtMember1ListD` | `0x42` | handler 7 (PSX `0x801F5C80`) | kHandler | kind 3, record 1, list D |
| `0x40EE70` | `Area77_Spawn3AtMember2ListE` | `0x46` | handler 8 (PSX `0x801F5D00`) | kHandler | kind 3, record 2, list E |
| `0x40EEC0` | `Area77_ShiftCameraDownSound` | `0x1B` | handler 9 (PSX `0x801F5D80`) | kHandler | `Camera_ShiftY` - `0x14`, redraw, sound `0x200` |
| `0x40EEE0` | `Area77_Spawn4AtMember1List0` | `0x42` | handler 12 (PSX `0x801F5E04`) | kHandler | kind 4, record 1, list 0 |
| `0x40EF30` | `Area77_Spawn4AtMember2List0` | `0x46` | handler 13 (PSX `0x801F5E84`) | kHandler | kind 4, record 2, list 0 |
| `0x40EF80` | `Area77_Spawn4AtMember0List0` | `0x42` | handler 14 (PSX `0x801F5F04`) | kHandler | kind 4, record 0, list 0 |
| `0x40EFD0` | `Area77_Spawn1AtMember0ListF` | `0x42` | handler 15 (PSX `0x801F5F84`) | kHandler | kind 1, record 0, list F |
| `0x40F020` | `Area77_Spawn3AtMember0ListG` | `0x42` | handler 16 (PSX `0x801F6004`) | kHandler | kind 3, record 0, list G |
| `0x40F070` | `Area77_SpawnEffect4A` | `0x9` | handler 17 (PSX `0x801F6084`) | kHandler | `Area77_SpawnEffectKind(0x4A)` |
| `0x40F080` | `Area77_SpawnEffect4C` | `0x9` | handler 18 (PSX `0x801F60A4`) | kHandler | `Area77_SpawnEffectKind(0x4C)` |
| `0x40F090` | `Area77_LeapStep` | `0xA9` | handler 19 (PSX `0x801F60C4`) | kHandler | one step of a leap: the script's operand byte picks a (dx, dz) pair of `Area77_Leaps`; `Scena06_Leap(the script object, dx, dz, 0x40, -0x400, 4, 0)`; not 0: the script's position back 2; else at operand 4 the object put on the ground and `Sprite_SetAnimation(0)`, the position on 1 |
| `0x40F140` | `Area77_SpawnEffectKind` | `0x2B` | called by handlers 17, 18 | kCallee | a free effect record (`Effect_FindFree`), `+0` 1, kind `+5` the argument's byte |

Choices 2..22 are handlers 0..20 (the `+0x3C` array is the choice table
plus 8). Handler 10 is `Area49_ShiftCameraUp` (AR1C's), handler 11
`Area80_ResetCameraShift` (this band's), handler 20 `0x425DB0` (another
block's). The descriptor's `+0x38` is the colour matrix ART found
([`area-rows.md`](area-rows.md) section 2), never code.

**The leap** (`Area77_LeapStep`): the operand byte is the byte two past the
script object's position word `+0xA` in the running area's `+0x10` script
`[object +3]` (`Area_Descriptors[Game_AreaNumber] +0x10`, the movement
script's own table, [`movement-script.md`](movement-script.md)); the pair is
`Area77_Leaps[operand * 2]`. The two dwords passed as dx and dz carry what
the original's registers held above the byte - dx the pair's offset
(`operand * 2`, bit 8 for an operand of `0x80` or more), dz the `+0x10`
table's own address - which `Scena06_Leap`'s phases read as s8 only; ours
passes the same dwords (control L3). `Scena06_Leap` (SC6's) is the leap's
phase machine; this is its one caller (E8 at `0x40F0E6`), called by name.

### Area 78 (descriptor `0x60CEF0`; PSX `0x801F4140`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40F170` | `Area78_ChoiceRowFlag16or17` | `0x49` | choice 0 | kChoice | message `0xFFFF`; answer 0: counter 0 `0x1E` and the chapter row's flag `0x16`; 1: counter 0 `0x1E` and flag `0x17` |
| `0x40F1C0` | `Area78_ChoiceCounter2Bor2C` | `0x28` | choices 3, 4 | kChoice | message `0xFFFF`; counter 0 `0x2B` / `0x2C` |
| `0x40F1F0` | `Area78_ChoiceCounterAor2` | `0x28` | choices 5, 6 | kChoice | message `0xFFFF`; counter 0 `0xA` / 2 |
| `0x40F220` | `Area78_ChoiceResetScene` | `0x5B` | choice 7 | kChoice | the message word from `Area78_ChoiceMessages`; answer 0: `ScriptFlags_Set40`, the four counters 0, `0x8034E5` and the word `0x8034E6` 0, `MoveScript_Var7` `0xE`; 1: counter 0 0 |
| `0x40F280` | `Area78_SpawnEffect44` | `0x28` | choice 8 = handler 0 (PSX `0x801F2E1C`); area 80's choice 6 = handler 3 (PSX `0x801F3D50`) | kHandler | a free effect record of kind `0x44` |

Choices 1 and 2 are `0x437CC0` (a shared `ret`, another block's).

### Area 79 (descriptor `0x60EC30`; PSX `0x801F4C2C`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40F2B0` | `Area79_ChoiceCounter1or14` | `0x28` | choices 0, 1 | kChoice | message `0xFFFF`; counter 0 1 / `0x14` |
| `0x40F2E0` | `Area79_ChoiceCounter3` | `0x5C` | choice 2 | kChoice | **the message word untouched**; an answer 0..5 (unsigned) through a jump table of six (`+0x44`): counter 3 = answer + 1 |
| `0x40F340` | `Area79_ChoiceMessageCounter3` | `0x6C` | choice 3 | kChoice | the message word from `Area79_ChoiceMessages` (read before the bound); an answer 0..5 through a jump table of six (`+0x54`): counter 3 = `0xB` + answer |
| `0x40F3B0` | `Area79_ChoiceCounter8or9` | `0x28` | choice 4 | kChoice | message `0xFFFF`; counter 0 8 / 9 |
| `0x40F3E0` | `Area79_ChoiceCounter5orA` | `0x28` | choice 5 | kChoice | message `0xFFFF`; counter 0 5 / `0xA` |
| `0x40F410` | `Area79_ObjectState` | `0x12` | handler 0 (PSX `0x801F2E24`) | kHandler | `jmp [Area79_States + Sprite_Current[4] * 4]` |
| `0x40F430` | `Area79_StateSlide` | `0x62` | `Area79_States[1]` | kState | with a count at `Sprite_Current +0xA`: x moved by `Area79_SlideSteps[count & 0xF] << 11`, z by minus that, the count less 1, `Field_State`'s word `+0x12E` less 2; a count of 0: state `+4` back to 0 |

`Area79_States[0]` is `0x40D380`, a body area 69's block holds (AR1F's band;
the tool's `shared` row, reached by areas 69 and 79).

### Area 80 (descriptor `0x60F308`; PSX `0x801F4450`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40F4A0` | `Area80_ChoiceMessage0` | `0x2E` | choice 0 | kChoice | the message word from the first pair of `Area80_ChoiceMessages`; counter 0 1 / `0xA` |
| `0x40F4D0` | `Area80_ChoiceMessage1` | `0x2E` | choice 1 | kChoice | the second pair, the same |
| `0x40F500` | `Area80_ChoiceMessage2` | `0x2E` | choice 2 | kChoice | the third pair, the same |
| `0x40F530` | `Area80_ResetCameraShift` | `0x11` | choice 5 = handler 2 (PSX `0x801F3D34`); also areas 7, 8, 15, 23, 49, 77, 81, 100, 119, 134, 188 | kHandler | `Camera_ShiftY` 0, redraw |

Choices 3..6 are handlers 0..3: `Area49_ShiftCameraDown`, `Area49_ShiftCameraUp`
(AR1C's), `Area80_ResetCameraShift`, `Area78_SpawnEffect44`.
`Area80_ResetCameraShift` is the band's **shared body**: twelve areas'
tables name it (the tool's `shared` row); taken once, here, keyed by address.

### Area 81 (descriptor `0x60FD90`; PSX `0x801F37B8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40F550` | `Area81_ChoiceCounter1or2` | `0x28` | choice 0 | kChoice | message `0xFFFF`; counter 0 1 / 2 |
| `0x40F580` | `Area81_ChoiceMessageCounter3or4` | `0x2E` | choice 1 | kChoice | the message word from `Area81_ChoiceMessages`; counter 0 3 / 4 |
| `0x40F5B0` | `Area81_SpawnEffect27AtObject` | `0x40` | handler 0 (PSX `0x801F2C94`) | kHandler | a free effect record of kind `0x27` at the running object's x and z |
| `0x40F5F0` | `Area81_ShiftCameraDown` | `0x10` | handler 1 (PSX `0x801F2D14`) | kHandler | `Camera_ShiftY` - `0xF`, redraw |
| `0x40F600` | `Area81_ShiftCameraUp` | `0x10` | handler 2 (PSX `0x801F2D3C`) | kHandler | `Camera_ShiftY` + `0xF`, redraw |

Handler 3 is `Area80_ResetCameraShift`.

### Area 82 (descriptor `0x610870`; PSX `0x801F62A4`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40F610` | `Area82_SkipByPose` | `0x24` | handler 0 (PSX `0x801F5590`) | kHandler | by `Field_State`'s `+0x89`: 8 nothing; 4 the script's position on `0xA`; else on `0x16` |
| `0x40F640` | `Area82_SetMargin5` | `0xD` | handler 1 (PSX `0x801F55F0`) | kHandler | the running object's dword `+0x70` 5 |
| `0x40F650` | `Area82_SkipByPose456` | `0x4D` | handler 2 (PSX `0x801F5604`) | kHandler | `Field_State +0x89` read three times: 5 on 3, 4 on 6, 6 on 9 |
| `0x40F6A0` | `Area82_OpenMessageByRowFlag4` | `0x38` | handler 3 (PSX `0x801F56E8`) | kHandler | the chapter row's flag 4: `Msg_OpenScript(8)`, else `(3)`; `Field_Request` 2 |

### Area 84 (descriptor `0x611218`; PSX `0x801F3130`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x40F6E0` | `Area84_SetFlag31At5` | `0x3E` | handler 0 (PSX `0x801F2C04`) | kHandler | the active member's `+0x80` bit 0 cleared; the leader's `+0x89` at 5: story flag `0x31`, `MoveCmd_TestFB(9, 0x10)` (answer unread), the running object's byte `+0` cleared |

## 2. Ours

`src/game/area_w2a.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
two raw addresses of section 9). The group's own callee
(`Area77_SpawnEffectKind`) is called the same way, so the fuzz stands a
recorder in for it and every function is tested alone. Shapes that repeat
are one helper with the function's constants: the ten spawns
(`SpawnAtMember(member, kind, list)`, area 49's shape), the three effect
records (`SpawnEffectRecord`), the choices (`ChoiceCounter0`,
`ChoiceMessageCounter0`, `CountersByAnswer`, `MessageAt`), the camera shifts
(`ShiftCamera`). Kept as the originals: every re-read after a call
(`Sprite_Current` after `Effect_Spawn`, `Effect_FindFree`,
`AreaMap_Elevation`, `MoveCmd_TestFB`; `MoveScript_Object` after
`Scena06_Leap` and `Sprite_SetAnimation`; the leader's pose after the row
flag's test), `Field_State +0x89` read three times, the order of every
call, the s8 answer (unsigned for area 79's two jump tables), the upper
bytes of the leap's two dwords, and the unchecked table reads of section 6.
`Area79_ObjectState` reads its table in place and calls the entry
directly (the harness's `DataTable` swaps it for the fuzz).

## 3. The fuzz

`BOF3X_SHADOW=area_w2a` (`src/game/area_w2a_fuzz.cpp`): eight `Run`s under
the one shadow name, one per area with its `Group::area` (76..82, 84),
6,000 rounds per function, the real descriptors and tables in place.

- **Callees the group lists:** `Effect_FindFree` `kByte 0xFF..0x03` (a slot
  inside the group's four effect records, or none); `Effect_Spawn` `kByte
  0xFE..0x02`; `Flags_Test` `kBool`; `Inventory_Remove`,
  `ScriptFlags_Set40`, `MoveCmd_TestFB` (words), `Scena06_Leap` (seven
  dwords compared whole), `AreaMap_Elevation`, `Sprite_SetAnimation`;
  `0x57C160`, `0x469FE0` by raw address; the group's own
  `Area77_SpawnEffectKind`. `Area79_States` is a `DataTable` of two (its
  entries recorders while area 79 runs). No harness edit; no `AH_THEIRS`
  moved.
- **Louder stand-ins** (an `effect`, half the time, from `Noise`): after
  each call whose caller reads a cell again, that cell moves -
  `Effect_Spawn`, `Effect_FindFree`, `AreaMap_Elevation`, `MoveCmd_TestFB`
  move `Sprite_Current`; `Scena06_Leap` and `Sprite_SetAnimation` move
  `MoveScript_Object`; `Flags_Test` moves the leader's pose a quarter of the
  time (area 77's cell hook reads it after the row flag's test; more would
  starve the switch's match).
- **Regions beyond the field frame:** `Effect_Objects` records 0..3, the
  active member and script object pointers, the chapter row pointer,
  `Camera_ShiftY`, `Cond_ByteFE` (27 regions, 16,799 bytes: with effect "record" `0xFF`'s first `0x40` bytes, where a helper that took `Effect_FindFree`'s none for a slot would write - control K3).
- **Every round:** `Field_ActiveMember` at one of the four party objects, a
  field object, a party record or the running object; `MoveScript_Object`
  at a field object or a party record.
- **Seeds:** the choice answer at 0, 1 (twice as often), 2..6, the sign
  edge and above (`0xFF`, `0x80`, `0x81`, `0x7F`); area 76's tail kind at 5
  and beside; area 77's cell switch drawn by the seed (a record of the
  table, read in place, exactly or with one of x, z, the pose off by one;
  the arguments follow the seed's draw, as AR1C's); the party list bytes
  0..7 for the spawns (else any byte, which reads the next lists); the
  leap's script `[object +3]` one of area 77's seven and its position word
  one whose operand byte is 4 half the time (a scan of the script's first
  `0x100` bytes), else any position in them or, one time in sixteen, any
  word; area 79's state `+4` 0 or 1, and the slide's count at 0, 1, 2,
  `0xF`, `0x10`, `0x11`, `0x80`, `0xFF` or any; area 82's `Field_State +0x89`
  at each value a handler tests and beside; area 84's leader `+0x89` at 5
  and beside.
- **The group's disturbance** (from the hash it is given): the tail kind (5
  or any), the active member and script object pointers, the leader's pose
  and `+0x89`, the answer byte, a party list byte, `Cond_ByteFE`.

**Result (in this worktree):** 300,000 rounds over the 50 functions (6,000
each), 164,077 calls to the stand-ins, 0 mismatches, 16,799 bytes of state
(27 regions) and the log compared. Coverage: every callee each function can
reach was called - area 77's cell switch toggles 166 (its sound and
`0x469FE0` the same), `Flags_Test` 6,490 there, `Effect_Spawn` 60,000 (ten
spawns), `Scena06_Leap` 6,000 with `AreaMap_Elevation` / `Sprite_SetAnimation`
1,006 (the operand-4 path after a 0 answer), `Area77_SpawnEffectKind`
12,000, area 79's two states 3,049 / 2,951, area 84's calls 1,608.

`BOF3X_SHADOW='*'`: exit 0, `inject: 4604 ours` (one short of the 4,605 `impl` lines, the gap the round doc records since before wave two), 413 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w2a.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w2a.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w2a`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all eight runs). **145 planted, 144 refused by a count (exit 3), 1 equivalent** (R9, its near variant R4 refused); no hang, no fault. K3 stood on the first run - with `Effect_FindFree` answering none the mutant writes into effect "record" `0xFF`, outside every region - and was refused (1,139 rounds) after the fuzz gained that record as a region; the rest were refused on the first run. Every one of the 50 functions has at least one control of its own or in a helper it runs; a control in a shared helper lists the functions it was refused in (a Fatal ends the self-test after the first area that differs, so a helper shared across areas lists the first area's). The thinnest is B11 (13 rounds): a pose byte of `0x10` or more whose low nibble matches, which only an unseeded pose reaches.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `Area76_StepDisarmTail5` | kind 4 | Area76_StepDisarmTail5 2078 |
| A2 | `Area76_StepDisarmTail5` | state kept | Area76_StepDisarmTail5 1394 |
| A3 | `Area76_StepDisarmTail5` | 0x9039F2 for 0x9039F1 | Area76_StepDisarmTail5 1399 |
| A4 | `Area76_StepDisarmTail5` | answers 1 | Area76_StepDisarmTail5 6000 |
| B1 | `Area77_CellHook` | row flag 0x2D | Area77_CellHook 6000 |
| B2 | `Area77_CellHook` | pose read before the row test | Area77_CellHook 163 |
| B3 | `Area77_CellHook` | z compared as x | Area77_CellHook 542 |
| B4 | `Area77_CellHook` | one record searched | Area77_CellHook 1488 |
| B5 | `Area77_CellHook` | flag 0x1D | Area77_CellHook 542 |
| B6 | `Area77_CellHook` | toggled flag from byte 2 | Area77_CellHook 182 |
| B7 | `Area77_CellHook` | sound 0x202 | Area77_CellHook 182 |
| B8 | `Area77_CellHook` | 0x469FE0(0xE) | Area77_CellHook 182 |
| B9 | `Area77_CellHook` | answers 0 after the toggle | Area77_CellHook 182 |
| B10 | `Area77_CellHook` | sound before the toggle | Area77_CellHook 182 |
| B11 | `Area77_CellHook` | pose masked to its low nibble | Area77_CellHook 13 |
| C1 | `Area77_Trigger34` | kind 0x2B | Area77_Trigger34 6000 |
| C2 | `Area77_Trigger34` | sub-kind 6 | Area77_Trigger34 6000 |
| C3 | `Area77_Trigger34` | state 1 | Area77_Trigger34 6000 |
| C4 | `Area77_Trigger34` | answers 1 | Area77_Trigger34 6000 |
| C5 | `Area77_Trigger34` | no ScriptFlags_Set40 | Area77_Trigger34 6000 |
| D1 | `Area77_ChoiceMessage` | table + 2 | Area77_ChoiceMessage 5942 |
| D2 | `MessageAt (x9)` | the answer unsigned | Area77_ChoiceMessage 1918 |
| D3 | `Area77_ChoiceCounter0` | answer 1 sets 2 | Area77_ChoiceCounter0 619 |
| D4 | `CountersByAnswer (x13)` | answer 2 for 1 | Area77_ChoiceCounter0 966 |
| D5 | `ChoiceCounter0 (x9)` | message 0xFFFE | Area77_ChoiceCounter0 6000 |
| D6 | `CountersByAnswer (x13)` | counter 1 for answer 0 | Area77_ChoiceCounter0 606 |
| D7 | `Answer (x18)` | the answer byte 0x7DEE66 | Area77_ChoiceMessage 5917, Area77_ChoiceCounter0 1262 |
| E1 | `Area77_SetByteFE` | value 2 | Area77_SetByteFE 5969 |
| E2 | `Area77_SetByteFE` | sound 0x203 | Area77_SetByteFE 6000 |
| E3 | `Area77_ClearByteFE` | value 1 | Area77_ClearByteFE 5960 |
| E4 | `Area77_ClearByteFE` | sound 0x200 | Area77_ClearByteFE 6000 |
| F1 | `SpawnAtMember (x10)` | x and z swapped | Area77_Spawn3AtMember0ListA 6000, Area77_Spawn4AtMember0ListB 6000, Area77_Spawn2AtMember0ListC 6000, Area77_Spawn3AtMember1ListD 5999, Area77_Spawn3AtMember2ListE 6000, Area77_Spawn4AtMember1List0 6000, Area77_Spawn4AtMember2List0 6000, Area77_Spawn4AtMember0List0 6000, Area77_Spawn1AtMember0ListF 6000, Area77_Spawn3AtMember0ListG 6000 |
| F2 | `SpawnAtMember (x10)` | the slot to +0xA | Area77_Spawn3AtMember0ListA 4785, Area77_Spawn4AtMember0ListB 4851, Area77_Spawn2AtMember0ListC 4798, Area77_Spawn3AtMember1ListD 4838, Area77_Spawn3AtMember2ListE 4817, Area77_Spawn4AtMember1List0 4802, Area77_Spawn4AtMember2List0 4813, Area77_Spawn4AtMember0List0 4837, Area77_Spawn1AtMember0ListF 4794, Area77_Spawn3AtMember0ListG 4845 |
| F3 | `SpawnAtMember (x10)` | Sprite_Current read before the call | Area77_Spawn3AtMember0ListA 2029, Area77_Spawn4AtMember0ListB 2116, Area77_Spawn2AtMember0ListC 2069, Area77_Spawn3AtMember1ListD 2156, Area77_Spawn3AtMember2ListE 2124, Area77_Spawn4AtMember1List0 2121, Area77_Spawn4AtMember2List0 2104, Area77_Spawn4AtMember0List0 2163, Area77_Spawn1AtMember0ListF 2089, Area77_Spawn3AtMember0ListG 2040 |
| F4 | `SpawnAtMember (x10)` | the next member's list byte | Area77_Spawn3AtMember0ListA 5190, Area77_Spawn4AtMember0ListB 5158, Area77_Spawn2AtMember0ListC 5178, Area77_Spawn3AtMember1ListD 5100, Area77_Spawn3AtMember2ListE 5374, Area77_Spawn4AtMember1List0 5141, Area77_Spawn4AtMember2List0 5124, Area77_Spawn4AtMember0List0 5166, Area77_Spawn1AtMember0ListF 5091, Area77_Spawn3AtMember0ListG 5130 |
| F5 | `SpawnAtMember (x10)` | none is 0xFE | Area77_Spawn3AtMember0ListA 2406, Area77_Spawn4AtMember0ListB 2354, Area77_Spawn2AtMember0ListC 2400, Area77_Spawn3AtMember1ListD 2370, Area77_Spawn3AtMember2ListE 2318, Area77_Spawn4AtMember1List0 2337, Area77_Spawn4AtMember2List0 2358, Area77_Spawn4AtMember0List0 2340, Area77_Spawn1AtMember0ListF 2410, Area77_Spawn3AtMember0ListG 2392 |
| F6 | `SpawnAtMember (x10)` | z from +0x32 | Area77_Spawn3AtMember0ListA 6000, Area77_Spawn4AtMember0ListB 6000, Area77_Spawn2AtMember0ListC 6000, Area77_Spawn3AtMember1ListD 6000, Area77_Spawn3AtMember2ListE 6000, Area77_Spawn4AtMember1List0 6000, Area77_Spawn4AtMember2List0 6000, Area77_Spawn4AtMember0List0 5999, Area77_Spawn1AtMember0ListF 6000, Area77_Spawn3AtMember0ListG 6000 |
| F7 | `Area77_Spawn3AtMember0ListA` | kind 2 | Area77_Spawn3AtMember0ListA 6000 |
| F8 | `Area77_Spawn4AtMember0ListB` | list A | Area77_Spawn4AtMember0ListB 5106 |
| F9 | `Area77_Spawn2AtMember0ListC` | member 1 | Area77_Spawn2AtMember0ListC 6000 |
| F10 | `Area77_Spawn3AtMember1ListD` | kind 4 | Area77_Spawn3AtMember1ListD 6000 |
| F11 | `Area77_Spawn3AtMember2ListE` | list D | Area77_Spawn3AtMember2ListE 2626 |
| F12 | `Area77_Spawn4AtMember1List0` | member 0 | Area77_Spawn4AtMember1List0 6000 |
| F13 | `Area77_Spawn4AtMember2List0` | kind 3 | Area77_Spawn4AtMember2List0 6000 |
| F14 | `Area77_Spawn4AtMember0List0` | list B | Area77_Spawn4AtMember0List0 2251 |
| F15 | `Area77_Spawn1AtMember0ListF` | kind 0 | Area77_Spawn1AtMember0ListF 6000 |
| F16 | `Area77_Spawn3AtMember0ListG` | list F | Area77_Spawn3AtMember0ListG 1712 |
| F17 | `Area77_Spawn3AtMember0ListG` | member 2 | Area77_Spawn3AtMember0ListG 6000 |
| G1 | `Area77_RemoveItem47` | item 0x48 | Area77_RemoveItem47 6000 |
| G2 | `Area77_RemoveItem47` | count 2 | Area77_RemoveItem47 6000 |
| G3 | `Area77_RemoveItem47` | category 0 | Area77_RemoveItem47 6000 |
| H1 | `Area77_SetFlag4` | flag 5 | Area77_SetFlag4 6000 |
| H2 | `Area77_SetFlag4` | the chapter row | Area77_SetFlag4 6000 |
| I1 | `Area77_ShiftCameraDownSound` | - 0x13 | Area77_ShiftCameraDownSound 6000 |
| I2 | `Area77_ShiftCameraDownSound` | sound 0x201 | Area77_ShiftCameraDownSound 6000 |
| I3 | `ShiftCamera (x3)` | redraw 1 | Area77_ShiftCameraDownSound 6000 |
| J1 | `Area77_SpawnEffect4A` | kind 0x4B | Area77_SpawnEffect4A 6000 |
| J2 | `Area77_SpawnEffect4C` | kind 0x4D | Area77_SpawnEffect4C 6000 |
| K1 | `SpawnEffectRecord (x3)` | kind + 1 | Area77_SpawnEffectKind 4775 |
| K2 | `SpawnEffectRecord (x3)` | +0 = 2 | Area77_SpawnEffectKind 4775 |
| K3 | `SpawnEffectRecord (x3)` | none is 0xFE | Area77_SpawnEffectKind 1139 |
| K4 | `SpawnEffectRecord (x3)` | record stride 0x7C | Area77_SpawnEffectKind 3503 |
| K5 | `Area77_SpawnEffectKind` | the kind's second byte | Area77_SpawnEffectKind 4752 |
| L1 | `Area77_LeapStep` | operand one byte early | Area77_LeapStep 5662 |
| L2 | `Area77_LeapStep` | dx from the pair's second byte | Area77_LeapStep 5833 |
| L3 | `Area77_LeapStep` | dz without the register's upper bytes | Area77_LeapStep 6000 |
| L4 | `Area77_LeapStep` | lift 0x41 | Area77_LeapStep 6000 |
| L5 | `Area77_LeapStep` | back 1 on a leap | Area77_LeapStep 4041 |
| L6 | `Area77_LeapStep` | operand 5 | Area77_LeapStep 1023 |
| L7 | `Area77_LeapStep` | Sprite_Current not read again after the elevation | Area77_LeapStep 469 |
| L8 | `Area77_LeapStep` | the script object not read again at the end | Area77_LeapStep 1090 |
| L9 | `Area77_LeapStep` | elevation at (z, x) | Area77_LeapStep 1014 |
| L10 | `Area77_LeapStep` | animation 1 | Area77_LeapStep 1014 |
| L11 | `Area77_LeapStep` | the position a byte | Area77_LeapStep 332 |
| L12 | `Area77_LeapStep` | goes on after a leap | Area77_LeapStep 4041 |
| L13 | `Area77_LeapStep` | gravity -0x3FF | Area77_LeapStep 6000 |
| L14 | `Area77_LeapStep` | script table + 0x14 | Area77_LeapStep 6000 |
| M1 | `Area78_ChoiceRowFlag16or17` | flag 0x18 | Area78_ChoiceRowFlag16or17 582 |
| M2 | `Area78_ChoiceRowFlag16or17` | counter 0x1F | Area78_ChoiceRowFlag16or17 1184 |
| M3 | `Area78_ChoiceRowFlag16or17` | answer 2 for 1 | Area78_ChoiceRowFlag16or17 873 |
| M4 | `Area78_ChoiceRowFlag16or17` | the story flags for the row | Area78_ChoiceRowFlag16or17 1184 |
| N1 | `Area78_ChoiceCounter2Bor2C` | 0x2D | Area78_ChoiceCounter2Bor2C 653 |
| N2 | `Area78_ChoiceCounterAor2` | 0xB | Area78_ChoiceCounterAor2 625 |
| O1 | `Area78_ChoiceResetScene` | Var7 0xD | Area78_ChoiceResetScene 672 |
| O2 | `Area78_ChoiceResetScene` | counter 3 kept | Area78_ChoiceResetScene 668 |
| O3 | `Area78_ChoiceResetScene` | the word 1 | Area78_ChoiceResetScene 672 |
| O4 | `Area78_ChoiceResetScene` | answer 1 sets 1 | Area78_ChoiceResetScene 622 |
| O5 | `Area78_ChoiceResetScene` | table + 2 | Area78_ChoiceResetScene 5902 |
| O6 | `Area78_ChoiceResetScene` | no ScriptFlags_Set40 | Area78_ChoiceResetScene 672 |
| O7 | `Area78_ChoiceResetScene` | the step byte kept | Area78_ChoiceResetScene 671 |
| P1 | `Area78_SpawnEffect44` | kind 0x45 | Area78_SpawnEffect44 4728 |
| Q1 | `Area79_ChoiceCounter1or14` | 0x15 | Area79_ChoiceCounter1or14 644 |
| Q2 | `Area79_ChoiceCounter3` | bound 6 | Area79_ChoiceCounter3 318 |
| Q3 | `Area79_ChoiceCounter3` | answer + 2 | Area79_ChoiceCounter3 2526 |
| Q4 | `Area79_ChoiceCounter3` | bound signed | Area79_ChoiceCounter3 1915 |
| Q5 | `Area79_ChoiceMessageCounter3` | 0xC + answer | Area79_ChoiceMessageCounter3 2464 |
| Q6 | `Area79_ChoiceMessageCounter3` | the message only inside the bound | Area79_ChoiceMessageCounter3 3536 |
| Q7 | `Area79_ChoiceMessageCounter3` | table + 2 | Area79_ChoiceMessageCounter3 3782 |
| Q8 | `Area79_ChoiceCounter8or9` | 0xA | Area79_ChoiceCounter8or9 607 |
| Q9 | `Area79_ChoiceCounter5orA` | 6 | Area79_ChoiceCounter5orA 571 |
| R1 | `Area79_ObjectState` | the other entry | Area79_ObjectState 6000 |
| R2 | `Area79_StateSlide` | x step << 12 | Area79_StateSlide 2358 |
| R3 | `Area79_StateSlide` | z not negated | Area79_StateSlide 2358 |
| R4 | `Area79_StateSlide` | the count & 0x1F | Area79_StateSlide 1713 |
| R5 | `Area79_StateSlide` | Field_State +0x12E less 1 | Area79_StateSlide 5122 |
| R6 | `Area79_StateSlide` | state 1 at a count of 0 | Area79_StateSlide 878 |
| R7 | `Area79_StateSlide` | count less 2 | Area79_StateSlide 5122 |
| R8 | `Area79_StateSlide` | z moved from x | Area79_StateSlide 5122 |
| R9 | `Area79_StateSlide` | the count & 0x7 (the table's halves are equal) | not refused: equivalent (the sixteen steps repeat with a period of four, so `& 0x7` reads the same bytes as `& 0xF`); R4, `& 0x1F`, refused |
| R10 | `Area79_StateSlide` | z step by the count less 1 | Area79_StateSlide 5122 |
| S1 | `Area80_ChoiceMessage0` | the second pair | Area80_ChoiceMessage0 3550 |
| S2 | `Area80_ChoiceMessage1` | 0xB | Area80_ChoiceMessage1 597 |
| S3 | `Area80_ChoiceMessage2` | 2 for answer 0 | Area80_ChoiceMessage2 640 |
| S4 | `ChoiceMessageCounter0 (x4)` | the message by answer + 1 | Area80_ChoiceMessage0 5909, Area80_ChoiceMessage1 5910, Area80_ChoiceMessage2 5911 |
| S5 | `Area80_ResetCameraShift` | shift 1 | Area80_ResetCameraShift 6000 |
| S6 | `Area80_ResetCameraShift` | redraw 3 | Area80_ResetCameraShift 6000 |
| T1 | `Area81_ChoiceCounter1or2` | 3 | Area81_ChoiceCounter1or2 601 |
| T2 | `Area81_ChoiceMessageCounter3or4` | 5 | Area81_ChoiceMessageCounter3or4 629 |
| T3 | `Area81_ChoiceMessageCounter3or4` | table + 2 | Area81_ChoiceMessageCounter3or4 5935 |
| T4 | `Area81_SpawnEffect27AtObject` | kind 0x28 | Area81_SpawnEffect27AtObject 4810 |
| T5 | `Area81_SpawnEffect27AtObject` | x from +0x38 | Area81_SpawnEffect27AtObject 4810 |
| T6 | `Area81_SpawnEffect27AtObject` | Sprite_Current read before Effect_FindFree | Area81_SpawnEffect27AtObject 2181 |
| T7 | `Area81_SpawnEffect27AtObject` | z to +0x3C | Area81_SpawnEffect27AtObject 4810 |
| T8 | `Area81_ShiftCameraDown` | - 0xE | Area81_ShiftCameraDown 6000 |
| T9 | `Area81_ShiftCameraUp` | + 0x10 | Area81_ShiftCameraUp 6000 |
| U1 | `Area82_SkipByPose` | stops at 7 | Area82_SkipByPose 915 |
| U2 | `Area82_SkipByPose` | 4 skips 0xB | Area82_SkipByPose 458 |
| U3 | `Area82_SkipByPose` | else 0x15 | Area82_SkipByPose 5060 |
| U4 | `Area82_SkipByPose` | the leader's +0x89 for Field_State's | Area82_SkipByPose 351 |
| U5 | `Area82_SetMargin5` | 6 | Area82_SetMargin5 6000 |
| U6 | `Area82_SetMargin5` | +0x6C | Area82_SetMargin5 6000 |
| U7 | `Area82_SkipByPose456` | 5 skips 4 | Area82_SkipByPose456 446 |
| U8 | `Area82_SkipByPose456` | 4 skips 7 | Area82_SkipByPose456 466 |
| U9 | `Area82_SkipByPose456` | 7 for 6 | Area82_SkipByPose456 897 |
| U10 | `Area82_OpenMessageByRowFlag4` | message 9 when set | Area82_OpenMessageByRowFlag4 4020 |
| U11 | `Area82_OpenMessageByRowFlag4` | message 4 when clear | Area82_OpenMessageByRowFlag4 1980 |
| U12 | `Area82_OpenMessageByRowFlag4` | Field_Request 3 | Area82_OpenMessageByRowFlag4 6000 |
| U13 | `Area82_OpenMessageByRowFlag4` | row flag 5 | Area82_OpenMessageByRowFlag4 6000 |
| V1 | `Area84_SetFlag31At5` | bit 1 | Area84_SetFlag31At5 4475 |
| V2 | `Area84_SetFlag31At5` | at 4 | Area84_SetFlag31At5 2390 |
| V3 | `Area84_SetFlag31At5` | flag 0x32 | Area84_SetFlag31At5 1578 |
| V4 | `Area84_SetFlag31At5` | TestFB(0x10, 9) | Area84_SetFlag31At5 1578 |
| V5 | `Area84_SetFlag31At5` | byte +0 set 1 | Area84_SetFlag31At5 1578 |
| V6 | `Area84_SetFlag31At5` | Sprite_Current read before the calls | Area84_SetFlag31At5 737 |
| V7 | `Area84_SetFlag31At5` | the member bit only at 5 | Area84_SetFlag31At5 2242 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (23): `Area77_Choices`
`0x60BB2C` (23), `Area77_Handlers` `0x60BB34` (21), `Area77_CellSwitches`
`0x60BBCC` (2 records of 4 bytes: x, z, pose, story flag),
`Area77_ChoiceMessages` `0x60BBD4` (6 words), `Area77_EffectKinds0`
`0x60ACC0` (8), `Area77_EffectKindsA` `0x60BBE0` (seven lists of 8, A..G,
consecutive), `Area77_Leaps` `0x60BC18` (byte pairs by the operand; the count
is not bounded by any reader), `Area78_Choices` `0x60CECC` (9),
`Area78_Handlers` `0x60CEEC` (1), `Area78_ChoiceMessages` `0x60CF34` (2),
`Area79_Choices` `0x60EC00` (6), `Area79_Handlers` `0x60EC28` (1),
`Area79_ChoiceMessages` `0x60EC74` (6), `Area79_States` `0x60EC80` (2),
`Area79_SlideSteps` `0x60EC88` (16 signed bytes), `Area80_Choices`
`0x60F2E8` (7), `Area80_Handlers` `0x60F2F4` (4), `Area80_ChoiceMessages`
`0x60F34C` (3 pairs), `Area81_Choices` `0x60FD68` (2), `Area81_Handlers`
`0x60FD80` (4), `Area81_ChoiceMessages` `0x60FDD4` (2), `Area82_Handlers`
`0x610860` (4), `Area84_Handlers` `0x611210` (1). A descriptor's `+0x3C`
array is the tail of its `+0x34` table in areas 77, 78 and 80 (as
[`area-rows.md`](area-rows.md) section 2 found generally). `0x60ACC0` lies
right after area 76's descriptor and is read only by area 77's code (area
77's data runs on before its descriptor, the tool's rule). The lists' counts
of 8 are by their spacing (a party list byte indexes them); lists F and G
hold the same eight bytes, and the eight slide steps repeat with a period
of four (so the index masks `& 0xF` and `& 0x7` read the same values: R9).

## 6. Latent defects

Described, not fixed:

- **Unchecked indexes into data** (kept: the reads stay in `.data`). Every
  message table by the s8 answer (areas 77, 78, 79, 80, 81; area 79's
  choice 3 reads before its bound); the effect-kind lists by a party list
  byte; `Area77_Leaps` by the script's operand byte; the running area's
  `+0x10` script table by the script object's `+3` and the script by its
  position word (the movement-script engine indexes them the same way; a
  script index past the table reads a dword that may not be a pointer, and
  both would fault on it).
- **Two indexes past a table abort in ours** (the project's rule, round9
  doc section 6; no DIVERGENCE entry): `Area79_ObjectState` jumps through
  `Area79_States[Sprite_Current +4]` of two - a state of 2 or more jumps
  through `Area79_SlideSteps`' bytes in the original, and ours aborts;
  `Area77_LeapStep` reads `Area_Descriptors[Game_AreaNumber]` - an area of
  200 or more reads past the table, and ours aborts (only area 77's table
  names the handler, so neither is reached in play by anything read).
- **`Area79_ChoiceCounter3` leaves the message word as it found it**: the
  other choices store `0xFFFF` or a word; `MsgBox_ChoiceCommit` reads it
  after ([`item-use.md`](item-use.md) section 5), so the message before the
  choice is what it sees. Faithful; what the commit then does is not read.
- **`Area79_StateSlide` writes `Field_State`'s word `+0x12E`**, whatever
  object is sliding; the fuzz compares it, the meaning of `+0x12E` is not
  read here.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count), D146 (the stale message
word) in [`known-defects.md`](known-defects.md).

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 50 starts, extents, call sites
  (capstone's E8 / E9 scan of each extent, the same as `area_rows.py
  --clones`' rows for every area, run against the base commit's
  `symbols.toml` since the worktree's marks the band ours), the two jump
  tables in `.text` (`0x40F2E0`: six entries at `+0x44`, the `jmp`'s disp32
  at `+0xF`; `0x40F340`: six at `+0x54`, disp32 at `+0x1E`), and the shape
  its root gives.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding or one of the two jump tables.
- **The tool's first gap** (a start with no padding or `ret` before it):
  `0x40F340` follows `0x40F2E0`'s jump table; it is in the tool's list.
- **The tool's second gap** (a tail kind armed through a register): none
  in this band; the two kinds armed here (`Area76_StepDisarmTail5` clears
  kind 5, `Area77_Trigger34` arms `0x2C`) are immediates, both engine
  phases (`0x56DA10`, `0x56DE50`), so no band function is left unrooted.
- **`0x40EC50` is the tool's gap function** attributed to area 77 by its
  block: `Field_ObjectTriggers` id 34 (slot 33 of `0x662E20`) names it; no
  immediate store of 34 was read.

## 8. What reaches it

- **No recorded route reaches the band** (`analysis/area_funcs.tsv`'s live
  column is empty for all 50). Every function is reached only in play: the
  choices when the area's message box asks, the handlers from its movement
  scripts (ops `03` / `DE`), the hooks on a step or a cell in the area, the
  trigger by an object's `+0x86`, the slide by area 79's object state.
- **Gap and data roots:** `0x40EC50` (an object trigger, above);
  `0x40F430` (`Area79_StateSlide`, the tool's data root `0x60EC84`: entry 1
  of `Area79_States`); `0x40F140` (`Area77_SpawnEffectKind`, only from
  handlers 17 and 18).

## 9. Calls across groups

- **Raw addresses nobody owns** (group SX2's this wave, section 12 of the
  round doc), in `area_w2a_callees.h`: `0x57C160` (the story-flag toggle,
  area 77's cell hook) and `0x469FE0` (an effect of kind 4 and story flag
  `0x1C`, the same hook).
- **By name, Capcom's:** `Effect_Spawn`.
- **By name, ours:** `Scena06_Leap` (SC6), `Inventory_Remove` (SX),
  `Flags_Test` / `Set`, `Sound_PlayEffect`, `ScriptFlags_Set40`,
  `Effect_FindFree`, `AreaMap_Elevation`, `Sprite_SetAnimation`,
  `Msg_OpenScript`, `MoveCmd_TestFB`.
- **Named by other areas' tables, not called:** `0x40D380` (area 69's
  block, AR1F's band) is `Area79_States[0]`; `0x425DB0` is area 77's handler
  20 and `0x437CC0` area 78's choices 1 and 2 (other blocks'); this band's
  `Area80_ResetCameraShift` is named by eleven other areas' tables and
  `Area78_SpawnEffect44` by area 80's; AR1C's `Area49_ShiftCameraUp` /
  `Down` by areas 77 and 80's. Tables are read in place by the engine: no
  caller to rebind.
- **Inbound calls from outside the band** (an E8 / E9 scan of `.text` for
  the 50 starts): one, `0x56E0F5` in `Area_StepHook` (`0x56E050`, ours in
  `event_ops.cpp`, which calls its 38 step cases by raw address from
  `kStepHandlers`, this one with no arguments) into
  `Area76_StepDisarmTail5`. In the game it lands on the `jmp` Inject put at
  `0x40EB90`; for the rebinding pass. The other two hits are the band's own
  calls into `0x40F140`. SC6's `Scena06_Leap` names this band's `0x40F090`
  as its one caller (the other direction).
- `analysis/calltrace/entries_logic.txt`: 50 lines appended (the tool's
  extents); two starts were there already as hosts running over others
  (`0040EB90 5A9`, `0040F140 899`) and get the smaller extent beside them
  (the consolidation keeps the smaller).
