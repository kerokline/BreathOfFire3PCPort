# Scenario chapters 3 and 4: `Scena03_*` and `Scena04_*`

**Status:** IN PROGRESS (2026-09-27) - fifty functions ours
(`src/game/scena_sc3.cpp`, shadow name `scena_sc3`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md),
merged at `218eeec`): 0 mismatches in 100,000 rounds (chapter 3's 36
functions with `Cond_ByteFA` 3, chapter 4's 14 with 4); 75 of 76 negative
controls refused by a count (exit 3), the other an equivalent mutant with
its near variant refused. Fuzz only: no recorded route plays chapters 3 or
4 (section 8).

Group SC3 of round ten's first wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3).

## Stage A and stage B

Stage A (commit `6514faf`, before the harness existed) read and wrote the
fifty, wrote the fuzz against the harness's API contract and syntax-checked
it against a renamed copy of `magic_harness.h`. Stage B merged the harness
(`218eeec`), registered the module (the end of `CMakeLists.txt`'s list and
of `inject_all.cpp`) and ported the fuzz to what the harness is: one Group
per chapter byte, each clone's shape in its field, the callee and region
lists trimmed to what the harness's 70 standard callees and 22 standard
regions lack, typed stand-ins in the object handler tables. Re-running
`tools/scenario_rows.py --unit SC3 --clones` at the tip prints no clones:
all fifty are `impl` now; the stage A table (`222eb0f`) stands (`207ef4e`
renamed one shape, `kEntry`, which this file uses).

## 1. The band and what is in it

`0x5428C0..0x546390`, chapter 3's code then chapter 4's, no gap but the
alignment filler before four jump tables (a capstone coverage walk of the
band, 2026-09-27). The walk's lists hold **54 starts** (`pc_hidden` 30,
`pc_funcs` 16, the scenario walk's added starts 8), every one reached from
the chapter tables (`tools/scenario_roots.py --chapter 3` / `4`). By
reading:

- **Five are not functions.** `0x5453D0`, `0x5455A0` and `0x545910` are
  cases 3, 12 and 29 of `Scena04_Scene1`'s jump table `0x545948`, and
  `0x545AA0` and `0x545F70` cases 4 and 19 of `Scena04_Scene2`'s
  `0x5460A8`: `pc_hidden.json` records their only references as `.text`
  words inside those tables (`pe_hidden.py`'s inline-jump-table blind spot,
  [`field-modes.md`](field-modes.md) §3). `pairs_propagated.json`'s pairing
  of `0x5455A0` with PSX `0x801F8B48` is therefore wrong.
- **One start no list has:** `0x544AC0`, the tail object handlers 1..4 jump
  to (`jmp 0x544AC0` from each), which `tools/scenario_rows.py` (SCH,
  `222eb0f`) makes a start. Taken as its own function,
  `Scena03_ObjectsAllFour`.

So **50 functions**, 0x3973 bytes, none ours before. The tool's clone table
and extents agree with this reading function for function.

Chapter 3's call tables (`Scena03_CallA` `0x65F684`: `0x519A00`,
`0x519A40`; `Scena03_CallB` `0x65F68C`: `0x519A50`) and chapter 4's
(`0x519A60`, `0x519AB0`; `0x437CC0`) point outside the band (party set-ups
in `Boot: field, map and sprites`: `PartySet_Load`, `Party_Join`); they are
nobody's this wave and none reads a stack argument.

What the chapters *are* in the story is not read here (no gameplay claims);
the code shows chapter 3 playing in areas 0x25..0x63 and chapter 4 in
0x28..0x41, with the scene numbers below.

## 2. Chapter 3

Tables (named here, `[[data]]`): `Scena03_States` `0x660F64` (3) running on
into `Scena03_Runs` `0x660F70` (9: run 0 the bare `ret` `0x437CC0`, runs
1..8 the scenes), `Scena03_Bob4` `0x660F94` (4 s8), `Scena03_Bob2`
`0x660F98` (2 s8), `Scena03_ObjectHandlers` `0x660F9C` (9; entry 6 the bare
`ret`), `Scena03_Cells` `0x660FC0` (one 5-byte record for `0x56D800`),
`Scena03_CellHandlers` `0x660FC8` (1). `Scena03_Hooks` `0x660F50` is SCH's.

| Function | Entry | Bytes | Shape | Does |
|---|---|--:|---|---|
| `Scena03_Frame` | `0x5428C0` | 0xE | kSlot 0 | `jmp [Scena03_States + s8 0x8034E2 * 4]` |
| `Scena03_Start` | `0x5428D0` | 0x12 | kState 0 | state 1; the chapter's flag row `0x903FA8` cleared |
| `Scena03_EnterArea` | `0x5428F0` | 0x3AC | kState 1 | by area 0x25 / 26 / 27 / 29 / 2D / 32 / 38 / 45 / 47 / 63, flags and `Cond_ByteFD`: pass flags, `AreaMap_SetByte`, counters, camera, drop-ins, `Scenario_CallA(0)` / `CallB(0)`, a scene armed; state 2 |
| `Scena03_Run` | `0x542CA0` | 0xE | kState 2 | `jmp [Scena03_Runs + s8 MoveScript_Var7 * 4]` |
| `Scena03_Scene1` | `0x542CB0` | 0x268 | kState, run 1 | 8 steps: three effects 0x14 on counter 0 = 0x28..0x2A, `0x590C90(0x97, ...)`, area 0x2D, effect 0x13, area 0x27 and run 3 |
| `Scena03_Scene2` | `0x542F20` | 0x40 | kState, run 2 | drop-in, counter 0 = 0xA; on 0x28 run 1 step 1 |
| `Scena03_Scene3` | `0x542F60` | 0x553 | kState, run 3 | two-level switch (byte table `0x543480`, 0x33 steps; jump table of 26): messages 1 and 0x32, area 0x29, flags 3..6, `0x532ED0` and `0x4410B0`, two streams |
| `Scena03_Scene4` | `0x5434C0` | 0x1CC | kState, run 4 | 11 steps: message 0x20, area 0x25 twice, flag 0x15 |
| `Scena03_Scene5` | `0x543690` | 0x454 | kState, run 5 | 18 steps: areas 0x2D and 0x32, messages 0xA / 0xB, three effects, flags 0x19 / 0x1A |
| `Scena03_Scene6` | `0x543AF0` | 0x548 | kState, run 6 | 29 steps: area 0x63, a stream, three views, `Cond_ByteFD` 3, flag 0x1C |
| `Scena03_Scene7` | `0x544040` | 0x2EC | kState, run 7 | 14 steps: two area 0x63 changes, effects 0x13 / 0x22 (its slot kept in `0x8034E3`), flag 0x1D |
| `Scena03_Scene8` | `0x544330` | 0x4F4 | kState, run 8 | 21 steps (entries 14 and 15 one case): the party bobbed and stand-ins spawned on a timer with sounds 0x201..0x206, effects 0x13 / 0x23 / 0x25, music 0x49 / 0x51, flags 0x1E and `0x904650` bit 6, tail `0x56D6F0` |
| `Scena03_SpawnAtMember` | `0x544830` | 0x154 | kCallee (member), al | a stand-in sprite at a party member (section 2.1) |
| `Scena03_BobParty4` / `2` | `0x544990` / `0x5449C0` | 0x22 each | kCallee | members 0 and 1 `+0x3E` += `Scena03_Bob4[timer & 3]` / `Bob2[timer & 1]` << 4 |
| `Scena03_ObjectHook` | `0x5449F0` | 0x1F | kObject | `Scena03_ObjectHandlers[object +0x86](object, [0x929ED0])`, unchecked |
| `Scena03_Object0` | `0x544A10` | 0x23 | kState (object, row) | flag 0x14 on the row given, run 4 |
| `Scena03_Object1..4` | `0x544A40..0x544AA0` | 0x14 each | kState (object, row), al | flag 0x10..0x13 on the row, tail `Scena03_ObjectsAllFour` |
| `Scena03_ObjectsAllFour` | `0x544AC0` | 0x26 | kCallee (a tail), al | the row's bits 0x10..0x13 all set (`0x903FAA & 0xF`): step 6, timer 0x1E, 1 |
| `Scena03_Object5` | `0x544AF0` | 0x4A | kState (object, row) | key item `0x591900(1)`, message 0x2F, stream 2 waited out with a loading frame and `Task_Sleep(1)` each wait, tail `Sound_ResumeAll` |
| `Scena03_Object7` / `8` | `0x544B40` / `0x544B60` | 0x16 / 0x1F | kState (object, row), al | run 6 / run 7 with 0x1E frames; 0 |
| `Scena03_StepHook` | `0x544B80` | 0xF3 | kHook slot 2, al | by area (byte table `0x544C34` from 0x25, jump table of 7): the six area tests, else 0 |
| `Scena03_StepArea25/29/32/33/45/63` | `0x544D50`, `0x544D00`, `0x544E30`, `0x544C80`, `0x544DA0`, `0x544E80` | 0x47, 0x50, 0x45, 0x7F, 0x8C, 0xBD | kCallee (x, z), al | a flag and a cell or exact coordinate: a run and step, often `Scena04_Message(id)`; area 0x33 sets the run and answers 0 |
| `Scena03_ArriveHook` | `0x544F40` | 0x20 | kHook slot 3, al | area 0x47: `Scena03_ArriveArea47`, else 0 |
| `Scena03_ArriveArea47` | `0x544F60` | 0x4D | kCallee (x, z), al | flag 0x1E clear, z 0x490000, x cell 0xE..0x11: run 8 step 2, 1 |
| `Scena03_CellHook` | `0x544FB0` | 0x2D | kHook slot 4, al | `0x56D800(Scena03_Cells, 1, x, z)`; negative 0xFF; else `Scena03_CellHandlers[answer]()`, 1 |
| `Scena03_Cell0` | `0x544FE0` | 0x2E | kState | message 0x19, run 7 with 0x1E frames |

Every scene is chapter 16's shape: a switch on the step `0x8034E5`, each
case waiting on one thing (a script counter `0x903848` / `0x903849`, the
timer `0x8034E6` counted down, `Field_Request` 2, the wait word, the last
message id `0x7DEE48`, `Sound_StreamDone`, `File_LoadDone`, `Game_Mode`),
doing one thing, setting the next step or ending the run (run 0, step 0).

### 2.1 `Scena03_SpawnAtMember`

A free `Sprite_Objects` record from `0x57CD90` (answers 0 in al if none),
made `Sprite_Current` and `Field_ActiveMember`; `EventObj_Reset`,
`Sprite_SetAnimationBank(0x67)`; at the member's `+0x34` + 0x8000 and
`+0x38` (the record's `+0x8C` / `+0x90` too, `+0x94` 0), on
`AreaMap_Elevation` there; state 4, `+2`, `+8`, `+6` 0, the record's
`+0x98`, `+0x9A`, `+0x88` 0 and `+0x83`, `+0x84` 2; flags 0x88 through
`EventObj_SetFlags` - **from a byte the original writes into its own
argument slot** (`mov byte [esp + 0x17], 0x88`, the argument's top byte),
ours from a local; `+0` bit 0x20, `+0x5C..0x5F` 1, 0, 0, 0; `EventObj_Face`;
1. `EventObj_Place`'s shape ([`event-ops.md`](event-ops.md)).

## 3. Chapter 4

Tables: `Scena04_States` `0x660FE4` (3) into `Scena04_Runs` `0x660FF0` (3:
the `ret`, `Scena04_Scene1`, `Scena04_Scene2`), `Scena04_Quake` `0x660FFC`
(4 s8), `Scena04_TremorSteps` `0x661000` (4 s8), `Scena04_ObjectHandlers`
`0x661004` (1), `Scena04_Cells` `0x661008` (one record),
`Scena04_CellHandlers` `0x661010` (1). Slot 3 of `Scena04_Hooks` is
`Scenario_NoHook`.

| Function | Entry | Bytes | Shape | Does |
|---|---|--:|---|---|
| `Scena04_Frame` | `0x545010` | 0xE | kSlot 0 | `jmp [Scena04_States + s8 state * 4]` |
| `Scena04_Start` | `0x545020` | 0x12 | kState 0 | state 1; the row `0x903FB0` cleared |
| `Scena04_EnterArea` | `0x545040` | 0x2E1 | kState 1 | areas 0x28 (music 0x3F, run 1 step 7, `CallA(1)`; by counter 1 the pass flags or `Kind2_Place(1)` / `ObjTrio_SetBit40`), 0x2A, 0x30 (`AreaMap_SetByte` twice), 0x36, 0x41 (the three members' `+0x24` bit 0x20 cleared, effects 0x13 and 0x2E); state 2 |
| `Scena04_Run` | `0x545330` | 0xE | kState 2 | `jmp [Scena04_Runs + s8 run * 4]` |
| `Scena04_Scene1` | `0x545340` | 0x684 | kState, run 1 | 31 steps: a quake (`MapView_Elevation` shaken by `Scena04_Quake` << 6), the camera pulled back (`timer * 9`), messages 0xD / 0xE, streams, areas 0x2A and 0x28 (three returns to 0x28 on the counters), flag 4, the `Game_Mode` 7 switch waited out to mode 2 |
| `Scena04_Scene2` | `0x5459D0` | 0x754 | kState, run 2 | 31 steps and one exit (section 3.1): camera and music 0x41, effects, areas 0x41 and 0x36, flags 7 and 0xC |
| `Scena04_Tremor` | `0x546130` | 0x27 | kCallee | `MapView_Elevation += 4 * Scena04_TremorSteps[(Frame_Counter >> 1) & 3]`, redraw |
| `Scena04_ObjectHook` | `0x546160` | 0x1F | kObject | `Scena04_ObjectHandlers[object +0x86](object, row)` |
| `Scena04_Object0` | `0x546180` | 0x12 | kState (object, row), al | flag 0 on the row; 0 |
| `Scena04_StepHook` | `0x5461A0` | 0x20 | kHook slot 2, al | area 0x28: `Scena04_StepArea28`, else 0 |
| `Scena04_StepArea28` | `0x5461C0` | 0x15F | kCallee (x, z), al | flags 2 / 3 / 7, cells, the leader's `+8` not 4..6 (stored to `0x903850`): flag 2, sound 0x204, run 1 step 1; or run 1 step 0 with message 2, 3 or 0x26 |
| `Scena04_Message` | `0x546320` | 0x20 | kCallee (id), al | `Msg_OpenScript(id)`, `0x9039A3 |= 1`, request 2; 1. Chapter 3's hooks call it too |
| `Scena04_CellHook` | `0x546340` | 0x2D | kHook slot 4, al | `0x56D800(Scena04_Cells, 1, x, z)`; `Scena04_CellHandlers[answer]()` |
| `Scena04_Cell0` | `0x546370` | 0x1D | kState | run 2 with 0xA frames |

### 3.1 `Scena04_Scene2`'s exit

Every case leaves through one tail: `sub al, 2 / cmp al, 0xB / jae ret /
jmp 0x546130`. What is in al there is the step the case just stored, or -
where a case stores none (a wait not over) - the step byte read at the top,
or (case 0x15 after its own tremor call, cases 0x1E and 0xE after a failed
wait) the step read again. So the tremor runs on every frame of steps 2..12
and after any case that moves to one of them. Ours tracks al as a local and
calls `Scena04_Tremor` at the end (`Scene2Exit`). `ebx` holds 0x1E through
the cases that do not clear it: step 7 sets step 0x1E from it, steps 8, 9,
0xC and 0x14 the timer 0x1E, step 0xF the effect's `+9`.

## 4. The fuzz

`BOF3X_SHADOW=scena_sc3`, `scena_sc3_fuzz.cpp`: **two** `scenario_harness::Run`
calls, because a `Group` writes one chapter into `Cond_ByteFA` (and points
`0x929ED0` at that chapter's flag row) every round - chapter 3's 36 with
`chapter = 3`, chapter 4's 14 with `chapter = 4` (logged as
`scena_sc3 (chapter 3)` / `(chapter 4)`). `Scena04_Message` runs with
chapter 4's byte; it reads none.

- **the clones**: fifty, `tools/scenario_rows.py --unit SC3 --clones` (SCH's
  tool at `222eb0f`), names put in; nine jump tables moved into the copies
  (the two byte tables stay in the original, read-only); `ret_mask 0xFF` on
  the 23 that answer in al. **Shapes** (the Clone's field): `kSlot` the two
  frames; `kState` the states, runs, scenes, bobs, the tremor, the four-bit
  tail and the two cell handlers; `kObject` the two object hooks; `kHook` the
  step, arrive and cell hooks and the eight area tests (`(x, z)`, al);
  `kEntry` the object handlers (object, row), `Scena03_SpawnAtMember` (the
  member) and `Scena04_Message` (the id);
- **callees** beyond the harness's 70: `EventObj_SetFlags` again (its
  pointer, into the original's own frame, logged as the byte it points at),
  the six nobody owns by address (`0x533E50`; `0x56D800` a byte 0 or 0xFF;
  `0x57CD90` 0..0x1D or 0xFF; `0x587B80`; `0x590C90`; `0x591900`), the
  fourteen of ours the originals' E8 / E9 reach (the area tests,
  `ObjectsAllFour`, `SpawnAtMember` and `Message` a flag; the bobs and the
  tremor `kPhase`), and the ten object handler stand-ins;
- **the object handler tables**: a `.data` table's handler recorder logs no
  arguments, and the object handlers take (object, row), so, as SC0's
  `ObjectEntry`, the seed writes a typed stand-in of the fuzz's own into each
  entry of `Scena03_ObjectHandlers` (9) and `Scena04_ObjectHandlers` (1),
  each logging the object and the row against the handler's own address
  (entry 6, the bare `ret` `0x437CC0`, is also `Scena03_Runs` entry 0, a
  handler recorder's: its stand-in logs against `0x5449F0` instead); the
  tables are regions, put back after the run;
- **the tables** swapped for handler recorders: `Scena03_States` + `Runs`
  as one window of 12, `Scena03_CellHandlers` (1), `Scena04_States` + `Runs`
  (6), `Scena04_CellHandlers` (1);
- **regions** beyond the standard 22: `0x904CD0`, `0x904EE0`,
  `Music_Track`, `Cond_ByteFE`, the message id `0x7DEE48`, `Game_Mode`,
  `0x929F00..0x929F13`, `MoveScript_EffectState`, `Field_ActiveMember`, and
  the object handler table (32 regions in all, about 10,050 bytes);
- **the seed**: the state and run inside the swapped windows; the step
  inside each scene's cases and one past; half the time a step that waits on
  a counter with the counter at the value its case compares, or one either
  side (the pairs read off the cases by capstone, `kWaits`); a scene's timer
  0..2 a third of the time; the timer, counter 0 and counter 1 at every
  constant compared; the area at every one tested (the entries' own half
  the time); `Cond_ByteFD` 0..7; the request, wait word, message ids,
  `Kind2Hold`, `Game_Mode` 2 half the time each; the row's four bits and the
  byte `0x905E68`; the leader's `+8` 3..7; the object's `+0x86` in the four
  records the `kObject` shape passes; run 3 at its track steps, run 6 at
  step 0x13 with the timer 0x32, run 8 at its shaking steps, chapter 4's
  run 2 at its timed steps and at step 0x15 before the tremor;
- **the arguments**: the eight area tests at each exact coordinate or cell
  range they test, on it, half a cell into it, or one cell either side
  (`kHits`); the object handlers a sprite record and a row; the member 0..2
  with garbage above; the message id any byte;
- **the disturbance** (the group's part; drawn from the hash only - a first
  version drew from the harness's `Next` and mismatched in 403 rounds, the
  passes seeing different disturbances): counter 1, the area, a byte of the
  effect records, `Field_StatusBits`, the camera distance, `Game_Mode`, the
  timer and counter 0 at compared values, the step 2..0x16, `Cond_ByteFD`;
  and for the function being fuzzed, half the time, the one cell it reads
  again after a call: area 0x63's `Cond_ByteFD`, run 6's timer, run 3's
  counter 0, chapter 4's run 2 step.

**Result** (2026-09-27, this worktree):

    shadow      scena_sc3 (chapter 3) self-test: 72000 rounds over 36 functions (2000 each), 93946 calls to the stand-ins,
                0 MISMATCHES; 10068 bytes of state (32 regions) and the stand-ins' log compared
    shadow      scena_sc3 (chapter 4) self-test: 28000 rounds over 14 functions (2000 each), 24415 calls to the stand-ins,
                0 MISMATCHES; 10036 bytes of state (32 regions) and the stand-ins' log compared

Coverage (the originals' calls): every callee and handler the clones name is
reached - e.g. chapter 3's `Flags_Test` 16,690, `Field_ChangeArea` 638,
`Scena04_Message` 1,323, the nine object handler stand-ins 203..242 each,
`0x4410B0` / `0x532ED0` 36, `Scenario_CallB` 7, each run 262..302 as a
phase; chapter 4's `Scena04_Tremor` 1,021, `File_LoadDone` 30,
`MapView_SetElevation` 33, each run 770..800. Counts vary a little with the
build directory. `BOF3X_SHADOW='*'`: exit 0 (338 self-test lines).

## 5. The controls

Seventy-six, planted one at a time by a script (the scratch `controls.py`,
not committed: each anchored on a unique string of `scena_sc3.cpp`; replace,
build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc3`, restore; rebuild at
the end), on the final fuzz, 2026-09-27. **75 of 76 refused** by a count
(exit 3), each in the function its plant touches; C12e is an equivalent
mutant. Counts are this worktree's.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| F1 | Scena03_Frame: state ^ 1 (inside the swapped window) | Scena03_Frame 2,000 |
| F2 | Scena04_Frame: indexed by the run | Scena04_Frame 1,441 |
| S1 | Scena03_Start: row 0x903FB0 | Scena03_Start 2,000 |
| S2 | Scena04_Start: state 2 | Scena04_Start 2,000 |
| E1 | Scena03_EnterArea: area 0x27 pass flags 0x1F | Scena03_EnterArea 57 |
| E2 | Scena03_EnterArea: area 0x2D counters cleared before Scenario_CallA | Scena03_EnterArea 3 |
| E3 | Scena03_EnterArea: area 0x38 on Cond_ByteFD 2 | Scena03_EnterArea 31 |
| E4 | Scena03_EnterArea: area 0x63 step 7 | Scena03_EnterArea 3 |
| E5 | Scena04_EnterArea: counter 1 == 2 places Kind2 | Scena04_EnterArea 16 |
| E6 | Scena04_EnterArea: area 0x30 counters kept | Scena04_EnterArea 18 |
| E7 | Scena04_EnterArea: member 2 +0x24 kept | Scena04_EnterArea 32 |
| E8 | Scena04_EnterArea: music 0x40 | Scena04_EnterArea 11 |
| C1 | Scena03_Scene1: effect kind 0x15 | Scena03_Scene1 353 |
| C2 | Scena03_Scene1 step 4: area track 0x30 | Scena03_Scene1 121 |
| C3 | Scena03_Scene2: counter 0 = 0xB | Scena03_Scene2 268 |
| C4 | Scena03_Scene3 step 3: timer tested before the decrement | Scena03_Scene3 4 |
| C5 | Scena03_Scene3 step 0xC: <= 0x14 | Scena03_Scene3 35 |
| C6 | Scena03_Scene3 step 0xF: counter read before Music_Play | Scena03_Scene3 7 |
| C20 | Scena03_Scene3 step 0xB: 0x532ED0 x and z swapped | Scena03_Scene3 36 |
| C7 | Scena03_Scene4 step 5: message 0x34 | Scena03_Scene4 5 |
| C17 | Scena03_Scene4 step 6: facing 0x83 | Scena03_Scene4 13 |
| C22 | Scena03_Scene4 step 7: counter + 2 | Scena03_Scene4 28 |
| C8 | Scena03_Scene5 step 0xB: Scenario_CallA(2) | Scena03_Scene5 23 |
| C9 | Scena03_Scene5 step 0xF: step 1 | Scena03_Scene5 66 |
| C21 | Scena03_Scene5 step 3: pending kind 2 | Scena03_Scene5 4 |
| C10 | Scena03_Scene6 step 0x13: the second compare of the first read | Scena03_Scene6 5 |
| C11 | Scena03_Scene6 step 0x1C: Cond_ByteFD 2 | Scena03_Scene6 59 |
| C18 | Scena03_Scene6 step 2: z 0x270000 | Scena03_Scene6 2 |
| C12 | Scena03_Scene7 step 6: effect kind 0x23 | Scena03_Scene7 80 |
| C12e | TakeKeptEffect: the slot indexed unsigned (equivalent: the answer is 0..0x13 or 0xFF, which returns first) | not refused: equivalent (the answer is 0..0x13 or 0xFF, and 0xFF returns first); C12 is its near variant |
| C19 | Scena03_Scene7 step 6: +0x3C 0x4000000 | Scena03_Scene7 80 |
| C13 | Scena03_Scene8 step 7: & 3 == 1 | Scena03_Scene8 81 |
| C14 | Scena03_Scene8 step 0xB: step stored after the sounds | Scena03_Scene8 1 |
| C15 | Scena03_Scene8 step 0xE: one stand-in | Scena03_Scene8 49 |
| C16 | Scena03_Scene8 step 0x14: no 0x56D6F0 | Scena03_Scene8 41 |
| P1 | Scena03_SpawnAtMember: x + 0x4000 | Scena03_SpawnAtMember 1,950 |
| P2 | Scena03_SpawnAtMember: Sprite_Current not read again after AreaMap_Elevation | Scena03_SpawnAtMember 214 |
| P3 | Scena03_SpawnAtMember: flags 0x80 | Scena03_SpawnAtMember 1,950 |
| B1 | Bob: << 3 | Scena03_BobParty4 1,057, Scena03_BobParty2 2,000 |
| B2 | Bob: member 2 for member 1 | Scena03_BobParty4 1,057, Scena03_BobParty2 2,000 |
| B3 | Scena03_BobParty2: Scena03_Bob4 | Scena03_BobParty2 959 |
| O1 | Scena03_ObjectHook: the next handler | Scena03_ObjectHook 2,000 |
| O2 | Scena03_Object3: flag 0x13 | Scena03_Object3 2,000 |
| O3 | Scena03_ObjectsAllFour: & 7 | Scena03_ObjectsAllFour 67 |
| O4 | Scena03_Object5: sleep 2 | Scena03_Object5 665 |
| O5 | Scena03_Object8: timer 0x1F | Scena03_Object8 2,000 |
| O6 | Scena04_Object0: answer 1 | Scena04_Object0 2,000 |
| H1 | Scena03_StepHook: area 0x29 to StepArea32 | Scena03_StepHook 67 |
| H2 | Scena03_StepArea33: flag 0 asked once | Scena03_StepArea33 486 |
| H3 | Scena03_StepArea29: z cells 7..9 | Scena03_StepArea29 55 |
| H4 | Scena03_StepArea25: z 0x410000 | Scena03_StepArea25 374 |
| H5 | Scena03_StepArea45: the third door dropped | Scena03_StepArea45 117 |
| H6 | Scena03_StepArea32: x cell 0x16 | Scena03_StepArea32 610 |
| H7 | Scena03_StepArea63: Cond_ByteFD read before the test | Scena03_StepArea63 3 |
| H8 | Scena03_ArriveHook: area 0x46 | Scena03_ArriveHook 78 |
| H9 | Scena03_ArriveArea47: x cells 0xE..0x10 | Scena03_ArriveArea47 76 |
| H10 | Scena03_CellHook: none answers 0xFE | Scena03_CellHook 982 |
| H11 | Scena04_StepArea28: the leader kind 3 refused | Scena04_StepArea28 20 |
| H12 | Scena04_StepArea28: message 4 | Scena04_StepArea28 30 |
| H13 | Scena04_Message: 0x9039A2 \|= 1 | Scena04_Message 1,469 |
| H14 | Scena04_CellHook: Scena03_Cells | Scena04_CellHook 2,000 |
| K1 | Scena04_Cell0: timer 0xB | Scena04_Cell0 2,000 |
| K2 | Scena03_Cell0: message 0x1A | Scena03_Cell0 2,000 |
| Q1 | Scena04_Scene1 step 6: << 5 | Scena04_Scene1 8 |
| Q2 | Scena04_Scene1 step 8: * 8 | Scena04_Scene1 18 |
| Q3 | Scena04_Scene1 step 0x1D: Game_Mode 3 | Scena04_Scene1 9 |
| Q4 | Scena04_Scene1 step 4: the effect indexed by 0x903850 | Scena04_Scene1 11 |
| Q5 | Scena04_Scene1 step 0x15: facing 1 | Scena04_Scene1 62 |
| R1 | Scena04_Scene2 exit: < 0xC | Scena04_Scene2 106 |
| R2 | Scena04_Scene2 step 7: step 0x1D | Scena04_Scene2 13 |
| R3 | Scena04_Scene2 step 9: the sign test on bit 14 | Scena04_Scene2 25 |
| R4 | Scena04_Scene2 step 0x15: al not read again after the tremor | Scena04_Scene2 4 |
| R5 | Scena04_Scene2 step 0x10: clamped to 0x3FF | Scena04_Scene2 46 |
| R6 | Scena04_Scene2 step 8: the effect x 0, not the yaw | Scena04_Scene2 15 |
| T1 | Scena04_Tremor: >> 2 | Scena04_Tremor 1,256 |
| T2 | Scena04_Tremor: * 2 | Scena04_Tremor 977 |

The thinnest are the re-reads and orders: C14 (1 round), E2 and E4 and H7 (3),
C4 and R4 (4), C10 (5). Each shows only when the disturbance moves exactly
the cell read again across exactly that call. The first run of the controls
had five not refused - E2, C4, C10, H7, R4 - each a re-read or an order no
input of that fuzz reached; the seeds and the per-function disturbance of
section 4 (a scene's timer at 0..2, the entries' areas, the cell each reads
again) were added for them, and all 76 were run again. A seventh, C12e, first
crashed both sides: the plant itself was wrong (a missing parenthesis made it
index `(pool + slot) << 7`); rewritten as meant it is equivalent.

## 6. Cross-group calls and callees

By address in `scena_sc3_callees.h` (nobody's this wave, or another
group's):

| Address | What (by reading) | Owner |
|---|---|---|
| `0x4410B0` | (u8): member 0's `+1` = 5, `+2..+4` 0, `0x904AAA` = the byte, `0x904AE5` from a table, `0x905BA5 |= 0x10` | group SE |
| `0x579D70` | `EventObj_Face` | group SE |
| `0x5341C0` | `Scenario_CallB` (named by SCH, not taken) | nobody (SCH names it) |
| `0x532ED0` | (x, z, u8): the party placed at a point | nobody |
| `0x533E50` | (): the party's records refreshed (`Char_RecalcStats`) | nobody |
| `0x56D6F0` | (): `Field_StatusBits |= 0x80` | nobody |
| `0x56D800` | (records, n, x, z): the 5-byte cell record matched, 0xFF none | nobody |
| `0x57CD90` | (): a free `Sprite_Objects` index 0..0x1D, 0xFF none | nobody |
| `0x587B80` | (): `jmp 0x5A6FF0`, the music buffer stopped | nobody |
| `0x590C90` | (u8 id, x, u8, y): called with 0x97 and the effect state's byte | nobody |
| `0x591900` | (u8): into the first free of 32 bytes at `0x904554` | nobody |

In the fuzz, `0x4410B0`, `0x532ED0`, `0x56D6F0`, `Scenario_CallB` and `EventObj_Face` are the harness's standard callees; the other six are this group's (section 4). By name (ours or Capcom's): `Flags_Test`, `Flags_Set`, `AreaMap_SetByte`,
`Party_DropIn`, `ScriptFlags_Set40` / `Clear40`, `ObjTrio_SetBit40` /
`ClearBit40`, `Scenario_CallA`, `MapView_SetElevation`, `Kind2_Place`,
`Music_FadeOutStop` / `Play` / `FadeOut` / `LoadFile`, `Sound_PlayEffect`
/ `LoadStream` / `StreamDone` / `ResumeAll`, `File_LoadDone`,
`Effect_FindFree`, `Field_ChangeArea`, `Msg_OpenScript`,
`Transition_Start`, `Field_LoadingFrame`, `Task_Sleep`, `Field_ViewReset`,
`AreaMap_SetupEntries`, `MoveCmd_TestFB`, `EventObj_Reset`,
`Sprite_SetAnimationBank`, `AreaMap_Elevation`, `EventObj_SetFlags`.

## 7. Latent defects (Capcom's, kept; for the coordinator to number)

- **`Scena04_Scene1` step 4 reads the effect before `Effect_Objects` when
  step 3 found no slot.** Step 3 keeps `Effect_FindFree`'s answer in the
  s8 `0x8034E3`; with no free slot that is 0xFF, and step 4 tests bit 0 of
  record -1 (`0x7E1160`, whatever precedes the pool) to decide whether to
  wait. By reading only: whether the pool is ever full there is not
  measured. `Scena03_Scene7` step 6 keeps its slot the same way but never
  reads it back.
- **The chapter tables are indexed unchecked**, as every chapter's: the
  state (s8), the run (s8), the object's `+0x86` (9 or 1 entries - an
  object of chapter 4 with `+0x86` above 0 calls through
  `Scena04_Cells`' bytes), and the cell answer. Ours reads them in place
  the same.
- **`Scena03_SpawnAtMember`'s member is unchecked** (callers pass 0 and 1).

No divergence and no ledger entry: every function is a faithful
replacement. `DIVERGENCE.md` and `cheats.cpp` name no address of the band
(grep 2026-09-27).

## 8. What reaches it

Nothing recorded: no route plays chapters 3 or 4 (the owner's recipe saves
are the live check, [`takeover-queue-scenario.md`](takeover-queue-scenario.md)
§5). The frame hash is untouched unless a taken function is on the attract
path; chapter 16's is the only chapter the attract cycle runs.

`analysis/calltrace/entries_logic.txt`: 42 lines appended under a
`group SC3` comment; eight of the fifty were listed with their right
extents already; six host lines (`00542080 27A9`, `005449C0 2B7`,
`00544E80 E0`, `00544F60 11C9`, `00546130 90`, `00546320 3E75`) ran over
their neighbours and are superseded by the smaller lines.
