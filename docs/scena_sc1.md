# Scenario chapter 1's bank: the scenes, the object hook, the cell hook

**Status:** IN PROGRESS (2026-09-27) - forty-two functions ours
(`src/game/scena_sc1.cpp`, shadow name `scena_sc1`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 252,000 rounds (6,000 a function); 98 of 100 negative
controls refused by a count (exit 3), the other two equivalent mutants whose
near variants are refused (section 5). `BOF3X_SHADOW='*'` exits 0 (3,574
injects in this worktree). Fuzz only: no recorded route plays chapter 1
(section 8). No divergence.

Group SC1 of round ten's first wave ([`takeover-queue-round10.md`](takeover-queue-round10.md) §1),
the band `0x539AD0..0x53DDA0`, on the plan of
[`takeover-queue-scenario.md`](takeover-queue-scenario.md). Every claim about
the binary is capstone over `bof3/BOF3.exe` (2026-09-27), by a scratch
recursive-descent lister built on `tools/magic_rows.py`, checked against
SCH's `tools/scenario_rows.py --unit SC1 --clones`. Written in two stages:
stage A (the reading, ours, the symbols) before the harness landed, stage B
(the fuzz and the controls) on it, merged at `218eeec`.

## 1. What is in the band

58 starts lie in the band (`pc_funcs` + `pc_hidden` + the scenario walk's
added starts). Three were ours already, since round eight's group DB
([`mode_states.md`](mode_states.md) §1.5): `0x539AD0` `Scena01_Frame`,
`0x539B20` `Scena01_EnterArea` and `0x53D830` `Scena01_StepHook` (the brief
named `0x539B20` the step hook; it is the area entry). **Thirteen are not
functions**: jump-table cases of the function before them, which
`pe_hidden.py` cut at a case (the blind spot [`field-modes.md`](field-modes.md) §3
describes). That leaves **42 functions, all taken**:

| Entry | Name | Bytes | Call shape | Does |
|---|---|--:|---|---|
| `0x539AE0` | `Scena01_Start` | 0x39 | state handler, `Scena01_States` 0 | `Scenario_CallA(0)`; the change to area 9 at (`0x570000`, `0x40000`) facing 7; `0x903F98` and the four counters cleared; state 1 |
| `0x53A2B0` | `Scena01_Run` | 0xE | state handler, `Scena01_States` 2 | `jmp [Scena01_Runs + run * 4]`, the run the s8 `MoveScript_Var7` |
| `0x53A2C0` | `Scena01_Scene01` | 0xB3 | run 1 | three steps: area 9, `Kind2_Place(0)`, flag 0 and area 0xA |
| `0x53A380` | `Scena01_Scene02` | 0x260 | run 2 | steps 0..0xB: area 0xA's four changes by counter 0, music 0xE, transitions 0xF / 0x10 with sound 0x201, the last effect cleared, flag 1 |
| `0x53A5E0` | `Scena01_Scene03` | 0x4AC | run 3 | steps 0..0x16: the camera out and back over 0x50 frames, a pointing hand at (0xD2, 0x3C) until a button, Kind2 walked to x `0x210000` / `0x160000`, four kind-0x13 effects, music 0x14 / 0x13, the last effect's end waited for, flags 2, 3 |
| `0x53AA90` | `Scena01_Scene05` | 0x191 | run 5 | message 7, or (the leader's +0x89 = 4) member 0 dropped in and message 8; sound 0x103; the change to area 8 with `0x904CD0` = 0xB; flag 4 and `Inventory_Add(0, 0x22, 1)` |
| `0x53AC30` | `Scena01_Scene06` | 0x4FF | run 6 | started by the message box's choice (`0x7DEE48` = 2, `0x7DEE44` bit 1) and story flag 4 (`0x90410C`); flags 5..7; transition 4; `Scenario_CallB(0)` and area 8; a loop where the leader plays animation 0x44 on button 0x40 and 5 when it ends; two effects (kinds 0x15, 0x55); the second member's turn |
| `0x53B130` | `Scena01_Scene08` | 0x4B9 | run 8 | the leader on cells x 0x1B / 0x1C (z up to 0x23) starts it: member 6, `Cond_ByteFE`; `0x57C550` tests; timers of 0x6E / 0xB4 frames with sounds 0x200 / 0x207; flag 9 and area 0x17; the stream loaded, 0x78 frames, sound 0x204, message 0x13; the party restored, music 0x1D; the party placed, `0x4410B0(2)`; flag 0xA |
| `0x53B5F0` | `Scena01_Scene09` | 0x78C | run 9 | steps 0..0x2D: messages 1, 2, 0xC; eight map cells closed and opened (`AreaMap_SetByte`); members 2..5, 9, 0xA dropped in; flags 0xC, 0xF, 0x10, 0x12, 0x1D, 0x3F; `Sound_PlayById(0x1206)` at cells 0x3D..0x3F; area 0x16's changes; music 0x19; the party placed, `0x4410B0(3)`; the leader at the exit starts run 0xA |
| `0x53BD80` | `Scena01_Scene0A` | 0x294 | run 0xA | message 0x18; flag 0x10 cleared, member 8; sounds 0x205 / 0x206 and area 0x17 by flags 0x12, 0x14; Kind2 moves; area 5 with `0x904CD0` = 0xFF |
| `0x53C020` | `Scena01_Scene0B` | 0x30C | run 0xB | flags 0x19..0x1C; areas 7 and 0xE; two effects at the camera; message 0x20; member 1; the camera in over 0x2E frames |
| `0x53C330` | `Scena01_Scene0C` | 0x26C | run 0xC | area 5; transitions 0xE, 0xD; music 8; an effect; `0x57C550`; message 4; flags 0x15, 0x16; the party restored |
| `0x53C5A0` | `Scena01_Scene0D` | 0x480 | run 0xD | area 0 with `0x904CD0` = 1; the camera in over 0x14 frames; music 0; flags 0x3A..0x3D; two effects by `Scena01_PlaceEffect` (0x92, 0x93); message 0x36 and the choice bit |
| `0x53CA20` | `Scena01_Scene0E` | 0x18C | run 0xE | music 0x11, messages 0x10 / 0x1C, flag 0x3E, areas 0xD and 0xF, `Field_ViewReset`, then the tail jump to `0x56D6F0` (`Field_StatusBits` bit 7) |
| `0x53CBB0` | `Scena01_Scene0F` | 0x128 | run 0xF | counter 0 = 0x64 / 0x65; the party restored; the stream played and waited for; music 0x1B, message 0xA |
| `0x53CCE0` | `Scena01_Scene11` | 0x2B1 | run 0x11 | counter 0 = 0x64 / 0x65 / 0xC8 (`Game_Mode` 7, object `0xFE`: the shop by [`mode_states.md`](mode_states.md)'s reading of mode 7) / 0xC9; the stream; area 5 by the member count; music 7 |
| `0x53CFA0` | `Scena01_Scene12` | 0x63 | run 0x12 | music 0x1A, member 0xB; then run 9 at step 0x2D |
| `0x53D010` | `Scena01_Scene14` | 0x53 | run 0x14 | message 0x18, the end |
| `0x53D070` | `Scena01_Scene15` | 0x45 | run 0x15 | message 0x34, then run 6 |
| `0x53D0C0` | `Scena01_Scene16` | 0x45 | run 0x16 | message 0x34, the end |
| `0x53D110` | `Scena01_Scene17` | 0x291 | run 0x17 | run 0x11's shape twice over; area 0xA by the member count (two: (`0x500000`, `0x50000`), three: (`0x510000`, `0x80000`)) |
| `0x53D3B0` | `Scena01_PlaceEffect` | 0xB3 | called by `Scena01_Scene0D` with one word | `Sprite_Current` = the leader; an effect slot into its +0xB; the record live as kind 6, +6 = n + 0x70, +0x10 from the s8 table `0x660D60` by `0x904062`, the leader's +0x2E / +0x30 |
| `0x53D470` | `Scena01_ObjectHook` | 0x1D | vtable slot 1: `(object)`, no answer read | `Scena01_ObjectHandlers[object +0x86](object, 0x903F98)` |
| `0x53D490` | `Scena01_Object01` | 0x3E | object handler 1 | once (flag 0x14): run 0x12 |
| `0x53D4D0`..`0x53D530` | `Scena01_Object02`..`05` | 0x1E..0x20 | object handlers 2..5 | run 0x17 at step 0, 5, 2; run 0x11 at step 2 |
| `0x53D550` | `Scena01_Object06` | 0x7D | object handler 6 | counter 0 = 0xA; the object's state 4, +0x8A / +0x87 0, +0x83 0xA; the turn; run 0xD at step 0xA |
| `0x53D5D0` | `Scena01_Object07` | 0x7A | object handler 7 | as 6 but +0x8A kept; counters cleared; run 0xD at step 0xD |
| `0x53D650`, `0x53D6E0` | `Scena01_Object08`, `09` | 0x83, 0x71 | object handlers 8, 9 | +0x83 0xB; the turn; run 0xD at step 0x10 (9 clears only counter 3) |
| `0x53D760`..`0x53D790` | `Scena01_Object0A`..`0D` | 8 each | object handlers 0xA..0xD | counter 0 = 0x64, 0x66, 0x67, 0x68 |
| `0x53D7A0` | `Scena01_Object0E` | 0x2A | object handler 0xE | counters cleared, run 9 at step 0x28 |
| `0x53D7D0`, `0x53D7F0` | `Scena01_Object0F`, `10` | 0x1E, 0x20 | object handlers 0xF, 0x10 | run 0x11 at step 0, 5 |
| `0x53D810` | `Scena01_Object11` | 0x14 | object handler 0x11 | run 0xB at step 0xF |
| `0x53DD20` | `Scena01_CellHook` | 0x2A | vtable slot 4: `(x, z)` -> al | `0x56D800(Scena01_Cells, 2, x, z)`; none, 0xFF; else a tail jump through `Scena01_CellHandlers` with (x, z) |
| `0x53DD50` | `Scena01_Cell` | 0x42 | `Scena01_CellHandlers` 0 and 1, (x, z) -> al | flag 3 set, flag 4 clear: run 5 from step 0, al 1; else al 0xFF |

Bytes are each body to its last byte, jump tables and the byte table of a
two-level switch included (`Scena01_Scene06` 0x4FF and `Scena01_Scene17`
0x291 with theirs; the clone copies stop before them, 0x4E4 / 0x278, since a
byte table is read in place). What each scene *is* in the story was not read:
the numbers are areas, messages, flags and music tracks, not places or
characters.

A scene's shape is [`field-modes.md`](field-modes.md)'s: a switch on the
step byte `0x8034E5` - through a jump table, or a byte table into one, or a
`sub`/`dec` chain for two or three steps - each step waiting on counter 0
(`0x903848`, which the field scripts' counter ops move), the message box
(`Field_Request` 2 while it is up), the wait word `MoveScript_WaitWordDA`,
`Field_Kind2Hold`, a flag, the timer `0x8034E6` or the leader's position,
then doing one thing and setting the next step; the last step clears the
counters and sets the run and step to 0.

### 1.1 The twelve starts the chapter walk did not reach

The scenario walk ([`scenario-roots.md`](scenario-roots.md)) reached 46 of
the 58; the other twelve:

- **`0x53A2C0` is chapter 1's**: `Scena01_Runs` entry 1, `Scena01_Scene01`.
  The walk did reach it, but the catalogue labels it `Boot: top-level modes
  and tasks`, which is not a label the walk expands, so it went to the
  frontier (`scenario_roots.json` `frontier_functions`). A state handler
  through a table, mislabelled.
- **Eleven are not functions**, cases of jump tables of the function before
  them: `0x539CC0` (`Scena01_EnterArea`, already said by
  [`mode_states.md`](mode_states.md) §7), `0x53B150`, `0x53B210`,
  `0x53B450`, `0x53B480`, `0x53B4A0` (`Scena01_Scene08`), `0x53B960`,
  `0x53BC70` (`Scena01_Scene09`), `0x53BE30` (`Scena01_Scene0A`),
  `0x53C230` (`Scena01_Scene0B`), `0x53C470` (`Scena01_Scene0C`).

Two more of the 58 were reached but are cases too, which the walk listed as
starts: `0x539ED0` (`Scena01_EnterArea`) and `0x53AAB0`
(`Scena01_Scene05`'s case 0; `pairs_propagated.json`'s five "callers" twins
for it are wrong for that reason). None of the 58 belongs to another
chapter.

### 1.2 What the tools list that is not code

- `tools/scenario_rows.py --unit SC1` listed **`0x53B120` and `0x53D390`** at `222eb0f` (corrected at `207ef4e`):
  the byte tables after `Scena01_Scene06`'s and `Scena01_Scene17`'s jump
  tables (27 and 25 bytes of case indexes), decoded as code by the
  uncovered-code pass of `magic_rows.discover`. Dropped from the clone table.
- The chapter walk's closure lists **`0x520000`, `0x524870` and
  `0x558000`**: `0x520000` and `0x558000` are coordinates (`push 0x520000`
  in `Scena01_Scene02` step 0, `cmp edi, 0x558000` / `0x520000` in
  `Scena01_StepHook`) that happen to be `.text` addresses, and `0x524870` is
  reached from `0x520000`. Chapter 1's code calls neither; of the shared
  helpers only **`0x4410B0`** is called (by `Scena01_Scene08` and `09`).

## 2. The tables named

`symbols.toml` `[[data]]`:

| Table | Count | Read by | Entries |
|---|--:|---|---|
| `Scena01_Runs` `0x660D88` | 24 | `Scena01_Run` on the s8 `MoveScript_Var7` | `0x437CC0` (a bare ret) at runs 0, 4, 7, 0x10, 0x13; `Scena01_Scene01..17` |
| `Scena01_ObjectHandlers` `0x660DE8` | 18 | `Scena01_ObjectHook` on the object's +0x86 | `0x437CC0`, `Scena01_Object01..11` |
| `Scena01_Cells` `0x660E30` | 10 bytes | `0x56D800`, from `Scena01_CellHook` | two 5-byte cell records |
| `Scena01_CellHandlers` `0x660E3C` | 2 | `Scena01_CellHook` on `0x56D800`'s answer | `Scena01_Cell` twice |

`Scena01_Runs` follows `Scena01_States` (`0x660D7C`, 3) directly; unlike
chapter 16's the two do not overlap. The s8 table `0x660D60` (8 bytes before
`Scena01_Hooks`) is read by `Scena01_PlaceEffect`; it is left unnamed (one
reader, not a pointer table).

## 3. How ours is written

Every call out is `SH_CALL(name)` or, for a callee nobody owns,
`SH_AT(type, address)` with the address in `scena_sc1_callees.h`. The
dispatchers read their tables in place, as the originals do, and call the
entry; each **aborts past its table** where the original would call through
whatever lies beyond (the spell round's rule; no ledger entry):

- `Scena01_Run` past 24 (a negative run reaches `Scena01_States`' entries,
  including `Scena01_Run` itself);
- `Scena01_ObjectHook` past 18 (0x56D6D0 writes 0xFF to +0x86 after each
  call, so a trigger with none set would index entry 255);
- `Scena01_CellHook` past 2 (0x56D800 answers 0, 1 or 0xFF; the original
  jumps through any al of 0..0x7F).

Every store is where the original makes it relative to the calls around it
(a recorder disturbs what the caller reads again), and every byte the
original reads again after a call is read again. Where the original's value
is a register left over from the switch (`ebx` = 0x1A in
`Scena01_Scene06` steps 0xC and 0x19, 0x2D in `Scena01_Scene09` steps 0x27
and 0x29, `ecx` = 9 in `Scena01_Scene0E` step 8, 0x18 in `Scena01_Scene17`),
ours writes the constant and says so.

## 4. The fuzz

`scena_sc1_fuzz.cpp`, one `scenario_harness::Group` with `chapter = 1`:

- **Clones**: the 42 rows of `scenario_rows.py --unit SC1 --clones` (run
  against the symbols before this group took them; since `207ef4e` the tool
  no longer lists the two byte tables), call sites and jump tables as it
  printed them, checked against the reading. Shapes: `kState` for the start,
  the run dispatcher and the 19 scenes; `kObject` for `Scena01_ObjectHook`;
  `kEntry` for the 17 object handlers (the fuzz's `Args` passes a sprite
  record and `0x903F98`) and `Scena01_PlaceEffect` (n, 0x92 / 0x93 half the
  time); `kHook` with `ret_mask 0xFF` for `Scena01_CellHook` and
  `Scena01_Cell`.
- **Callees**: the harness's 70 standard ones, and the group's listing over
  them: `Menu_DrawHand`, `Scena01_PlaceEffect` (ours, called directly by
  `Scena01_Scene0D`), `0x533E50`, `0x57C550` (`kFlag`), `0x56D800` (a byte
  0xFF..0x01). Eleven standard callees are listed again with their standard
  masks and an `effect`, `Move`, that half the time moves one of the
  chapter's cells (the group's `Disturb`): the harness's own disturbance
  reaches the group's cells one call in 24, and the stores the scenes make
  around a call (the step, `Cond_ByteFE`, `0x904CD0`, the effect slot) and
  the counters they read again after one needed more (controls B1, D2, F3,
  G4, K3).
- **Handler tables**: `Scena01_Runs` (24) is swapped for handler recorders.
  `Scena01_ObjectHandlers` (18) and `Scena01_CellHandlers` (2) hold handlers
  that take arguments, which a handler recorder does not log: each entry but
  the bare ret is listed as a callee with a typed stand-in of the fuzz's own
  (`ObjectEntry<i>` logs the object and the row, `CellEntry` x and z and
  answers from `Noise`), which the harness then puts in the table.
- **Regions** beyond the 22 standard ones: `0x929F00..13` (the shop bytes,
  `Field_Kind2Hold`), record 0xFF's first byte of `Effect_Objects`
  (`0x7E9160`, section 6), `0x803157`, `Cond_ByteFE`, `Game_Mode`, the choice
  bytes `0x7DEE44..4B`, `0x904CD0`, `0x92BF17`, `0x669730`, and the eight
  member records `0x903A70` (10 regions, 32 in all, 11,357 bytes).
- **Seed**: the effect slot 0..0x13 or 0xFF; `0x669730` 0..7; each of the
  four sprite records' +0x86 an object handler index; `Sprite_Current` at a
  party object half the time. Then per function: a third of the rounds a
  (step, counter 0) pair one of its steps waits for (105 pairs over the 19
  scenes), a third a step of its cases, a third any byte; the waits'
  boundaries (counter 0 at the 45 values a step compares it with, counter 3
  at the zooms' and choices' 0x12..0x14, 0x19, 0x2C..0x2D, 0x4E..0x4F, bit
  7; the timer at 0x6C..0x6D and 0xB2..0xB3; the message box, the wait word,
  `Field_Kind2Hold`, `Input_Pressed` 0 / 0x40, the member count, the choice
  bits, `Cond_ByteFD` 4); the leader's cell words and position at every
  bound `Scena01_Scene08` step 0 and `Scena01_Scene09` steps 3, 0x1A and
  0x2D test; `Scena01_Scene0D`'s choice steps with counter 3 at 0x13 / 0x14
  / 0x19; the run 0..23 for `Scena01_Run`.
- **Disturbance** (the group's, from its hash only): counter 0 (to a waited
  value half the time; for `Scena01_Scene09` 0x32 or 0x46, its steps' second
  tests), any counter, `Field_Kind2Hold`, the effect slot, the member count,
  `Input_Pressed`, counter 3 (0x19 for `Scena01_Scene0D`, whose choice steps
  read it again), `Cond_ByteFE`, `0x904CD0`, the step. `settle` keeps the
  effect slot and the member index in range.

**Result** (2026-09-27, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc1`,
exit 0, in this worktree): 252,000 rounds, 227,269..227,525 calls to the
stand-ins (the counts move with the build directory), **0 mismatches**.
Coverage, the originals' side: every one of the 24 runs' handlers and 17
object handlers (302..358 each), the cell handler 3,967 times, and every
callee the bank calls: `Field_ChangeArea` 13,700, `Msg_OpenScript` 5,379,
`Flags_Test` 16,684, `Flags_Set` 10,367, `Flags_Clear` 201,
`Transition_Start` 5,022, `Party_DropIn` 3,266, `Kind2_Place` 3,704,
`Music_Play` 2,641, `Music_FadeOutStop` 3,025, `Sound_PlayEffect` 1,192,
`Sound_PlayById` 239, `Sound_LoadStream` 1,005, `Sound_StreamDone` 581,
`Effect_FindFree` 8,635, `Inventory_Add` 1,975, `AreaMap_SetByte` 652,
`Menu_DrawHand` 191, `Sprite_SetAnimation` 85, `Field_ViewReset` 137,
`Scenario_CallB` 195, `Scena01_PlaceEffect` 303, `0x533E50` 1,517,
`0x57C550` 587, `0x4410B0` 182, `0x532ED0` 151, `0x56D6F0` 981.
`BOF3X_SHADOW='*'`: exit 0, 3,574 injects.

## 5. Controls

A scratch script (`controls.py`, not committed) plants each change alone by
a unique text substitution in `scena_sc1.cpp`, rebuilds, runs the self-test,
restores, and rebuilds at the end. The count is the rounds of the function
that mismatched (of 6,000), in this worktree; every refused control ended
with exit 3 by comparison, none by a fault or a hang.

**100 controls, 98 refused.** The two not refused are equivalent mutants:

- **G9** (`Scena01_Scene09` step 0x1E without `Counter(0) = 0x64` before
  sound 0x202): the store is overwritten by `0x65` after the call, and no
  recorder reads counter 0, so no input tells them apart. Its near variant
  **G9b** (0x66 after the call) is refused in 28.
- **I3** (`Scena01_Scene0B` step 0x13 with the two script-flag xors joined
  before `Flags_Set`): xors commute, and every disturbance of
  `Field_ScriptFlags` (the harness's) is an xor, which commutes too. Its near
  variant **I3b** (the second xor 0x40) is refused in 293.

The thinnest refusals, where one boundary meets one disturbance: F4 (the
timer limit 0xB3) and G4 (step 0xB's second test skipped) in 2 rounds; C1,
D2, I1 and K1 in 6. Each was not refused on an earlier seed and was brought
in by the seed or the disturbance above; a later seed change can drop them
again, so re-run them after any.

| Id | Function | Plant | Rounds |
|---|---|---|--:|
| S1 | `Scena01_Start` | area 9 -> 8 | 6000 |
| S2 | `Scena01_Start` | state 1 -> 2 | 6000 |
| S3 | `Scena01_Start` | start word kept | 6000 |
| R1 | `Scena01_Run` | run + 1 | 5762 |
| A1 | `Scena01_Scene01` | counter 0 = 0x1E -> 0x1F | 998 |
| A2 | `Scena01_Scene01` | area 0xA z | 987 |
| B1 | `Scena01_Scene02` | step 4 effect cleared before the transition | 11 |
| B2 | `Scena01_Scene02` | script flags 0x60 -> 0x40 | 72 |
| B3 | `Scena01_Scene02` | counter 2 = 7 -> 6 | 236 |
| C1 | `Scena01_Scene03` | zoom 0x50 -> 0x4F | 6 |
| C2 | `Scena01_Scene03` | hand x | 191 |
| C3 | `Scena01_Scene03` | effect x | 193 |
| C4 | `Scena01_Scene03` | waited effect record by counter 2 | 46 |
| C5 | `Scena01_Scene03` | Kind2Z from +0x34 | 205 |
| D1 | `Scena01_Scene05` | leader +0x89 test on 5 | 135 |
| D2 | `Scena01_Scene05` | area byte stored before the change | 6 |
| D3 | `Scena01_Scene05` | inventory count 2 | 1975 |
| E1 | `Scena01_Scene06` | choice word 3 | 54 |
| E2 | `Scena01_Scene06` | step 0xC bound 0x19 | 168 |
| E3 | `Scena01_Scene06` | member record stride 0xA0 | 127 |
| E4 | `Scena01_Scene06` | button 0x20 | 22 |
| E5 | `Scena01_Scene06` | animation end read after the stores | 63 |
| E6 | `Scena01_Scene06` | second member bit 0x20 | 124 |
| E7 | `Scena01_Scene06` | step 0x19 -> 0x1B | 104 |
| F1 | `Scena01_Scene08` | z word bound 0x24 | 409 |
| F2 | `Scena01_Scene08` | second place never | 574 |
| F3 | `Scena01_Scene08` | Cond_ByteFE before the drop-in | 136 |
| F4 | `Scena01_Scene08` | timer limit 0xB3 | 2 |
| F5 | `Scena01_Scene08` | no fall-through into the wait | 50 |
| F6 | `Scena01_Scene08` | angle test b | 45 |
| F7 | `Scena01_Scene08` | party place facing 3 | 97 |
| F8 | `Scena01_Scene08` | SE helper 3 | 125 |
| G1 | `Scena01_Scene09` | gate cell z 0x21 | 72 |
| G2 | `Scena01_Scene09` | door cell value 0xA0 | 91 |
| G3 | `Scena01_Scene09` | x word 0x5E missing | 17 |
| G4 | `Scena01_Scene09` | step 0xB second test skipped | 2 |
| G5 | `Scena01_Scene09` | exit z bound exclusive | 12 |
| G6 | `Scena01_Scene09` | sound cells to 0x3E | 70 |
| G7 | `Scena01_Scene09` | flag 0x1D seen: 0x87 | 31 |
| G8 | `Scena01_Scene09` | step 0x27 -> 0x2C | 43 |
| G9 | `Scena01_Scene09` | counter 0 = 0x64 not stored | **0** (equivalent, below) |
| G9b | `Scena01_Scene09` | near G9: counter 0 = 0x66 after the sound | 28 |
| I3b | `Scena01_Scene0B` | near I3: the second xor 0x40 | 293 |
| H1 | `Scena01_Scene0A` | flag 0x14 answer swapped | 211 |
| H2 | `Scena01_Scene0A` | area byte 0xFE | 325 |
| H3 | `Scena01_Scene0A` | counter 0 = 0xC | 134 |
| I1 | `Scena01_Scene0B` | zoom 0x2F | 6 |
| I2 | `Scena01_Scene0B` | effect far 0x31F | 376 |
| I3 | `Scena01_Scene0B` | xor 0x60 before the flag | **0** (equivalent, below) |
| J1 | `Scena01_Scene0C` | effect life 0x14 | 386 |
| J2 | `Scena01_Scene0C` | flag 0x16 -> 0x17 | 411 |
| J3 | `Scena01_Scene0C` | restore skipped | 147 |
| K1 | `Scena01_Scene0D` | zoom end at 0x15 | 6 |
| K2 | `Scena01_Scene0D` | effect 0x93 -> 0x94 | 146 |
| K3 | `Scena01_Scene0D` | choice counter read once | 63 |
| K4 | `Scena01_Scene0D` | area byte 2 | 60 |
| L1 | `Scena01_Scene0E` | status bit 2 | 104 |
| L2 | `Scena01_Scene0E` | no tail call | 981 |
| L3 | `Scena01_Scene0E` | counter 0 = 8 -> 9 | 156 |
| M1 | `Scena01_Scene0F` | stream check before the pass flags | 78 |
| M2 | `Scena01_Scene0F` | music 0x1C | 195 |
| N1 | `Scena01_Scene11` | flags by count 3 swapped | 254 |
| N2 | `Scena01_Scene11` | shop object 0xFF | 635 |
| N3 | `Scena01_Scene11` | 0x65 end keeps counter 2 | 913 |
| O1 | `Scena01_Scene12` | step 0x2C | 1979 |
| P1 | `Scena01_Scene14` | message 0x19 | 1304 |
| P2 | `Scena01_Scene15` | run 7 | 1116 |
| P3 | `Scena01_Scene16` | run kept | 1357 |
| Q1 | `Scena01_Scene17` | two members x 0x510000 | 41 |
| Q2 | `Scena01_Scene17` | counter 2 = 3 | 305 |
| Q3 | `Scena01_Scene17` | step 0x17 not shared | 45 |
| T1 | `Scena01_PlaceEffect` | kind + 0x71 | 5705 |
| T2 | `Scena01_PlaceEffect` | word +0x30 from +0x2E | 5705 |
| T3 | `Scena01_PlaceEffect` | table unsigned | 2585 |
| U1 | `Scena01_ObjectHook` | row 0x903F9C | 5674 |
| U2 | `Scena01_ObjectHook` | index +0x87 | 5681 |
| V1 | `Scena01_Object01` | flag 0x15 | 6000 |
| V2 | `Scena01_Object02` | step 1 | 6000 |
| V3 | `Scena01_Object03` | run 0x16 | 6000 |
| V4 | `Scena01_Object04` | step 3 | 6000 |
| V5 | `Scena01_Object05` | run 0x12 | 6000 |
| V6 | `Scena01_Object06` | counter 0 = 0xB | 5718 |
| V7 | `Scena01_Object07` | +0x8A cleared too | 6000 |
| V8 | `Scena01_Object08` | pose 0xC | 6000 |
| V9 | `Scena01_Object09` | counter 2 cleared too | 5246 |
| VA | `Scena01_Object0A` | 0x65 | 6000 |
| VB | `Scena01_Object0B` | 0x65 | 6000 |
| VC | `Scena01_Object0C` | 0x66 | 6000 |
| VD | `Scena01_Object0D` | 0x69 | 6000 |
| VE | `Scena01_Object0E` | step 0x29 | 6000 |
| VF | `Scena01_Object0F` | step 1 | 6000 |
| VG | `Scena01_Object10` | step 4 | 6000 |
| VH | `Scena01_Object11` | run 0xC | 6000 |
| W1 | `Objects 6..9 (Turn)` | turn only when not facing | 3851 |
| W2 | `Objects 6..9 (Turn)` | bit 3 on the kept pointer | 646 |
| X1 | `Scena01_CellHook` | none answers 0 | 2033 |
| X2 | `Scena01_CellHook` | records 3 | 6000 |
| X3 | `Scena01_CellHook` | x and z swapped | 3967 |
| Y1 | `Scena01_Cell` | answer 2 | 1303 |
| Y2 | `Scena01_Cell` | flag 4 not tested | 4019 |

## 6. Latent defects (described, not fixed)

- **`Scena01_Scene02` step 4 clears byte 0 of the effect record in slot
  `0x903850` without testing for 0xFF.** The slot is whatever the last
  `Effect_FindFree` stored there (chapter 1's own scenes, the area entry's
  `PlaceEffect`, or any other writer of that scratch cell); after one that
  found no free slot, the store lands at `0x7E9160`, 0x7F80 bytes past the
  first record and outside the 20. Ours stores there too.
- **`Scena01_Scene06` step 0xF clears bit 0 of the member record
  `0x903A7B + 0xA4 * byte[0x669730]`** unbounded; the eight records end at
  `Cond_Flags` `0x903F90`, so an index of 8 and up writes into the flags.
- **`Scena01_Scene03` step 0x15 waits on `Effect_Objects[counter 3]`**, the
  slot step 0x14 stored in counter 3; a script counter op in between moves it
  to any of 256 records (a read, no fault).
- **The object handlers 6..9's facing test is dead**: `setne dl; xor edx, 4`
  is 4 or 5, never 0, so the `je` past the turn is never taken and the
  current sprite always turns to the leader's facing ^ 4, whatever it faced.
- **The three dispatch tables are unchecked** (section 3); ours aborts.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D154 (dead branches) in
[`known-defects.md`](known-defects.md).

## 7. Calls across groups (raw addresses, for the round's rebinding)

| Address | What | Owner |
|---|---|---|
| `0x4410B0` | one byte argument; `Scena01_Scene08` (2), `09` (3) | SE, this wave |
| `0x532ED0` | every member placed at (x, z) facing n | nobody |
| `0x533E50` | the members' records rebuilt (`Char_RecalcStats`, the copies into `ObjTrio`) | nobody |
| `0x57C550` | a test of two scaled words through `0x57C5A0`, al | nobody |
| `0x56D6F0` | `Field_StatusBits` \|= 0x80 | nobody |
| `0x56D800` | the index of the record matching (area, x, z) or 0xFF | nobody |

The rest are named and called by name (`Field_ChangeArea`, `Flags_*`,
`Music_*`, `Party_DropIn`, `Kind2_Place`, `Msg_OpenScript`, `Scenario_CallB`, which SCH named, ...), ours or
Capcom's.

## 8. What nothing reached

Fuzz only, like every scenario group until a chapter-1 route exists
([`takeover-queue-scenario.md`](takeover-queue-scenario.md) §5). The object
handlers are reached only when a field object's trigger sets +0x86, and the
cell hook only on the two cells of `Scena01_Cells`; neither is on the
attract path. Chapter 1 is not on the attract path at all (chapter 16 is),
so the frame hash is untouched by this group.
