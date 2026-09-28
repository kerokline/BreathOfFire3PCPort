# Scenario chapters 7 and 8: `Scena07_*` and `Scena08_*`

**Status:** IN PROGRESS (2026-09-27) - fifty-two functions ours
(`src/game/scena_sc7.cpp`, shadow name `scena_sc7`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 312,000 rounds (chapter 7's 27 functions with
`Cond_ByteFA` 7, chapter 8's 25 with 8); 57 of 58 negative controls refused
by a count (exit 3), the other an equivalent mutant with its near variant
refused. Fuzz only: no recorded route plays chapters 7 or 8 (section 8).

Group SC7 of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §6,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3).

## 1. The band and what is in it

`0x54F080..0x553B30`, chapter 7's code then chapter 8's, back to back (the
last function ends at `0x553B21`; chapter 9's `0x553B30` follows). The
walk's lists hold **66 starts**; `tools/scenario_rows.py --unit SC7
--clones` lists **52** and the reading agrees:

- **Fourteen are not functions** but cases of a switch pc_funcs / pc_hidden
  took for starts: `0x54F380` (case 1 of `Scena07_Scene1`'s table
  `0x54F40C`), `0x54F540`, `0x54F560`, `0x54F8A0` (cases 7, 8, 0x15 of
  `Scena07_Scene2`'s `0x54F96C`), `0x54FA40` (case 3 of `Scena07_Scene3`'s
  `0x54FB24`), `0x550950` (case 0 of `Scena07_Scene7`'s `0x550AEC`),
  `0x551BA0` (case 9 of `Scena08_Scene3`'s `0x551D8C`), `0x551E40` (case 2
  of `Scena08_Scene4`'s `0x551F08`), `0x551F60` (case 0 of
  `Scena08_Scene5`'s `0x552098`), `0x552400` (case 0 of `Scena08_Scene7`'s
  `0x552570`), `0x552770` (case 8 of `Scena08_Scene8`'s `0x552C30`),
  `0x552DD0` (case 3 of `Scena08_Scene9`'s `0x552EC8`), `0x553070` and
  `0x5534E0` (cases 0 and 0x14 of `Scena08_Scene11`'s `0x553560`). So
  `pairs_propagated.json`'s pairs of `0x551E40` (with `0x801FB8A0`) and
  `0x553070` (with `0x801FCBAC`, in another section) are a case's, not a
  function's - the `0x5455A0` kind of error HANDOFF item 9 names.
- **None is missing**: every `E8` target and every `.data` entry that
  points into the band is one of the 52 (a byte scan of `.text` for rel32
  calls into the band and of `.data` for dwords equal to a start,
  2026-09-27).
- Five scenes are **two-level switches** (a byte table after the jump
  table: `0x550B08`, `0x5520B4`, `0x552594`, `0x553034`, `0x5535BC`);
  the tool counts each byte table into its function's extent.

### 1.1 Which chapter's roots reach what

The plan said chapters 7 and 8 have no code of their own and run chapter
6's shared tail. **By reading they do have their own**, and nothing of
chapter 6 reaches this band:

- Chapter 7's vtable `Scena07_Hooks` `0x6611B8` names `0x54F080` (frame),
  `0x550B50`, `0x550D20`, `0x550F20`, `0x550FD0`; chapter 8's
  `Scena08_Hooks` `0x661368` names `0x551020`, `0x553690`, `0x553750`,
  `0x553810` and a 0 in slot 4. Every function of the band is reached from
  exactly one chapter's roots: `0x54F080..0x551014` (27) from chapter 7's,
  `0x551020..0x553B21` (25) from chapter 8's. Their call tables A and B
  point into the CALLS block only (`0x519AB0`, `0x519DE0`, `0x519E10`,
  `0x519E20` for 7; `0x519EA0..0x519FC0` for 8).
- `tools/scenario_roots.py --chapter 6` lists the whole band in chapter 6's
  closure, and `--chapter 7` lists chapter 8's: both are the walk reading a
  pointer table past its end. Chapter 6's `jmp [edx*4 + 0x6611A8]` (at
  `0x54EEF3`) has four entries and runs straight into `Scena07_Hooks`;
  chapter 7's cell-handler table `0x661364` (one entry) runs into
  `Scena08_Hooks`. The walk reads a `.data` table while its words are code
  pointers. No `E8`, `E9` or table entry of chapter 6 targets this band.
- **Inbound from outside**: one call, `0x55BBB1` (chapter 10's code, SC9b's
  band) calls `Scena07_PartyHas89State2` `0x550F80`. Two dwords in the band
  that read like pointers, `push 0x550000` (`0x54FFE0` step 0x27) and
  `mov [esi + 0x38], 0x550000` (`0x551590` step 2), are 16.16 coordinates.
- **Outbound into SC6's or SC9a's bands**: none. Every call leaves the band
  for the engine (named callees, or the raw addresses of section 6).

So the group's two `Run` calls are the two chapters' own, and nothing here
is a shared tail of chapter 6's.

## 2. Chapter 7

Row `0x903FC8` (`Cond_Flags + 8 * 7`). Tables named this round
(`[[data]]`): `Scena07_States` `0x6611CC` (3) running on into
`Scena07_Runs` `0x6611D8` (9, run 0 the bare `ret` `0x437CC0`),
`Scena07_TimedRecords` `0x6611FC` (12 six-byte records), `Scena07_Shake`
`0x661244` (4 s8), `Scena07_PlacedObjects` `0x661248` (14 event-op records
of 0x11 bytes), `Scena07_PlacedSeats` `0x661338` (14 bytes),
`Scena07_ObjectHandlers` `0x661348` (5), `Scena07_Cells` `0x66135C` (one
cell record), `Scena07_CellHandlers` `0x661364` (1).

| Function | Address | What it does |
|---|---|---|
| `Scena07_Frame` | `0x54F080` | slot 0: through `Scena07_States` on the s8 state |
| `Scena07_Start` | `0x54F090` | the row cleared, state 1 |
| `Scena07_EnterArea` | `0x54F0B0` | per area entered (0x35, 0x4A, 0x55, 0x69, 0xAF): drop-ins, map cells, the kind-2 sprite, a scene armed; state 2 |
| `Scena07_Run` | `0x54F330` | state 2: through `Scena07_Runs` on the run |
| `Scena07_Scene1`..`Scene8` | `0x54F340`, `0x54F420`, `0x54F9E0`, `0x54FB40`, `0x54FF40`, `0x54FFE0`, `0x550930`, `0x550B20` | runs 1..8, each a switch on the step (section 2.1) |
| `Scena07_TimedEffects` | `0x54FE90` | run 4's effects of kind 0x18 at the records whose frame is the timer |
| `Scena07_ShakeCamera` | `0x550890` | `Camera_ShiftY += 4 * Scena07_Shake[Frame_Counter & 3]` |
| `Scena07_PlaceObjects` | `0x5508C0` | 14 event objects through `EventOp_0x` |
| `Scena07_TakeEffect49` | `0x550CE0` | an effect of kind 0x49 for a member; answers the record (section 7) |
| `Scena07_ObjectHook` | `0x550B50` | slot 1: `Scena07_ObjectHandlers[object +0x86](object, row)` |
| `Scena07_Object0`..`4` | `0x550B70`..`0x550C90` | 0..2 a flag, a drop-in and the object to state 4; 3 and 4 run 7 (the two rewards), step 0x14 when chapter 6's flag 0x22 / 0x23 is set |
| `Scena07_StepHook` | `0x550D20` | slot 2: by area 0x4A / 0x52 / 0x55 / 0x69 / 0xAF |
| `Scena07_ArriveHook` | `0x550F20` | slot 3: area 0x67 |
| `Scena07_PartyHas89State2` | `0x550F80` | al 1 when a party member's `+0x89` is 2 |
| `Scena07_CellHook` | `0x550FD0` | slot 4: `0x56D800` over the one cell record, its handler |
| `Scena07_Cell0` | `0x551000` | run 6 at step 0xA |

### 2.1 The state machine

State 0 clears the row and goes to 1; state 1 (`Scena07_EnterArea`) arms
a run and goes to 2; state 2 dispatches the run. Each run is a switch on
the step byte whose every case waits on one thing (counter 0 `0x903848`
at a value, the timer, `Field_Request`, the wait word, a stream, a file),
does one thing and sets the next step; the last step clears the run and
the step. Runs 4 (`0x54FB40`) step 0xB and 6 (`0x54FFE0`) step 0x13 wait
for the step to be moved from outside (an event battle is started the step
before). Run 6's tail shakes the camera once past step 0x1F and again past
0x26; its cases leave three ways (return; the tail; one shake then the
tail's second test), kept case by case in ours. Run 7 is two item rewards
(object handlers 3 and 4 arm it): the item id is character record 7's
`+0x12` / `+0x15` + 1, category 1 / 2, and `Inventory_Add` is pushed a
fourth word it does not read.

## 3. Chapter 8

Row `0x903FD0`. Tables: `Scena08_States` `0x66137C` (3) running on into
`Scena08_Runs` `0x661388` (12), `Scena08_PairOps` `0x6613B8` (two 16-byte
event-op records), `Scena08_ObjectHandlers` `0x6613D8` (3), and
`Scena08_Kept` `0x6BC720` (16 bytes of `.data` the chapter keeps state in:
an area change to go back to, two sprite indices, two party-list bytes, an
effect slot).

| Function | Address | What it does |
|---|---|---|
| `Scena08_Frame` | `0x551020` | slot 0: through `Scena08_States` |
| `Scena08_Start` | `0x551030` | state 1, the row and the four counters cleared, `Scena08_PartyBitsToRecord0`, `Scena08_SwapKeyItem4`, story flag 0x43 |
| `Scena08_PartyBitsToRecord0` | `0x551060` | bit 0 of `+0xB` set on `CharacterRecords` 0..7, `0x533E50`, cleared again on 1..7 |
| `Scena08_SwapKeyItem4` | `0x553730` | every key-item byte 4 of the 32 at `0x904554` becomes 0xE |
| `Scena08_EnterArea` | `0x5510A0` | per area entered (0, 1, 2, 3, 0xC, 0xF, 0x12, 0x15, 0x20), by `Cond_ByteFD`; state 2 |
| `Scena08_Run` | `0x551580` | state 2: through `Scena08_Runs` |
| `Scena08_Scene1`..`Scene11` | `0x551590`, `0x5518E0`, `0x551980`, `0x551DE0`, `0x551F40`, `0x552120`, `0x5523E0`, `0x5525B0`, `0x552D50`, `0x552EF0`, `0x553050` | runs 1..11 |
| `Scena08_SpawnPair` | `0x552CA0` | two free sprites set up through `EventOp_6x` (run 8) |
| `Scena08_SetUpRecord4` | `0x553630` | character record 4: `+0xC += 10000`, four bytes, `Char_RecalcStats`, HP / AP healed (run 11) |
| `Scena08_ObjectHook` | `0x553690` | slot 1: `Scena08_ObjectHandlers[object +0x86](object, row)` |
| `Scena08_Object0`..`2` | `0x5536B0`, `0x5536E0`, `0x553700` | run 7 (timer 0x20 / step 0xA), run 10 with flag 0x13 |
| `Scena08_StepHook` | `0x553750` | slot 2: area 1 (runs 4 and 5), area 0x46 (a flag) |
| `Scena08_ArriveHook` | `0x553810` | slot 3: areas 1, 0xC (five exact points), 0xD, 0x23, 0x2B |

The same state machine as chapter 7's. The waits of this chapter decrement
the timer in two ways, both kept: `dec word` then the result
(`DecTimer`), and "store the decrement, test the old value"
(`PostDecTimer`), under which a timer of 0 wraps to 0xFFFF. Run 1's step 0
runs on into step 1's wait, run 5's step 1 into step 2's test. Runs 3, 6
and 11 wait at a step for the event battle they start to move it.

## 4. The fuzz

`BOF3X_SHADOW=scena_sc7`, `scena_sc7_fuzz.cpp`: two
`scenario_harness::Run` calls, one per chapter byte (SC3's pattern),
logged as `scena_sc7 (chapter 7)` / `(chapter 8)`.

- **the clones**: 52, `tools/scenario_rows.py --unit SC7 --clones`, names
  put in; 16 jump tables moved into the copies (the five byte tables stay
  in the original, read-only). **Shapes**: `kSlot` the two frames;
  `kState` the states, runs, scenes, helpers and the cell's handler;
  `kObject` the two object hooks; `kEntry` the eight object handlers
  (object, row) and `Scena07_TakeEffect49` (the member; its answer, a
  pointer, compared whole); `kHook` the five hooks (`ret_mask 0xFF`);
  `Scena07_PartyHas89State2` `kState` with `ret_mask 0xFF`;
- **callees** beyond the harness's 70: ours the standard set lists by
  address or not at all (`Field_StartEventBattle`, `EventOp_0x`,
  `Char_RecalcStats`); `Inventory_Add` (three bytes, a flag),
  `Item_NamePtr` (two bytes; its answer a pointer to 16 bytes of the
  harness's noise in a buffer of the fuzz's, compared) and
  `AreaMap_SetByte` (u16, u16, u8: `Scena08_EnterArea` leaves garbage above
  the u16 it pushes) at the widths they read; the seven nobody owns by
  address (`0x533E50`; `0x56D800` a byte 0 or 0xFF; `0x57CD90` 0..0x1D or
  0xFF; `0x587B80`; `0x591900`; `0x591BE0`; `0x498DE0`); ours the
  originals' E8 reach (`kPhase` the helpers without an answer;
  `Scena07_PartyHas89State2` a flag; `Scena07_TakeEffect49` answering one
  of the 20 effect records, since its callers write through it); and the
  eight object-handler stand-ins;
- **the object handler tables**: typed stand-ins written into each entry of
  `Scena07_ObjectHandlers` (5) and `Scena08_ObjectHandlers` (3) by the seed,
  each logging the object and the row against the handler's own address
  (SC0's `ObjectEntry`); the tables are regions, put back after the run;
- **the tables** swapped for handler recorders: `Scena07_States` + `Runs`
  as one window of 12, `Scena07_CellHandlers` (1), `Scena08_States` +
  `Runs` (15);
- **regions** beyond the standard 22 (38 in all, about 11,500 bytes):
  `0x903800..0x903807` (`Camera_ShiftY` and the sprite pointer
  `0x903804`), `CharacterRecords` 0..7, `Text_Records +0..+0x2F`,
  `0x904CD0`, `0x904EE0`, `Music_Track`, `Cond_ByteFE`, the message id
  `0x7DEE48`, `Field_ActiveMember`, `Scena08_Kept`, `0x929F10..13`,
  `0x9039F0..0x903A03`, `MoveScript_EffectState`, the key items, the name
  buffer, the chapter's object table;
- **the seed**: the state and run inside the swapped windows; the step at
  one of the scene's cases (read off ours by the scratch `stepcmp.py`) or
  one past; two in three rounds, a step that waits on a counter with the
  counter at its value or one either side; the timer at each constant a
  case compares, 0..2 for a scene, run 4 at a timed record's frame, run 6
  at 0x88; counter 0 at every value compared; counter 3 at 0..3 (a sprite
  index) or 0x11..0x13; the area at each tested; `Cond_ByteFD` 0..5; the
  request, wait word, message id, `Kind2Hold`; the member count, members'
  `+0x89` and member 0's kind and kept slot; the pointers and indices the
  code writes through kept inside the regions (`0x903804` and
  `Field_ActiveMember` at sprite records, `Scena08_Kept +0` below 30,
  `MoveScript_EffectState` bytes below 8); key-item bytes 4; the object's
  `+0x86` inside the table; for a hook, one of its hits (an exact point or
  a cell range, with the area it is tested in), the arguments following it;
- **the disturbance** (the group's part; from the hash only): counter 0 at
  compared values, the area, a byte of the effect records, `Cond_ByteFD`,
  the timer at 0 / 1 / 2 / 0x88 / 0xFFFF, the request, counter 3, the step
  below 0x28, `Kind2Hold`, the message id.

**Result** (2026-09-27, this worktree):

    shadow      scena_sc7 (chapter 7) self-test: 162000 rounds over 27 functions (6000 each), 359481 calls to the stand-ins,
                0 MISMATCHES; 11498 bytes of state (38 regions) and the stand-ins' log compared
    shadow      scena_sc7 (chapter 8) self-test: 150000 rounds over 25 functions (6000 each), 187408 calls to the stand-ins,
                0 MISMATCHES; 11490 bytes of state (38 regions) and the stand-ins' log compared

Every callee and handler the clones name is reached (the coverage lines);
the thinnest are `Music_LoadFile` 15, `Scena08_SpawnPair` 52,
`Scena08_SetUpRecord4` 59, `PartySet_LoadSecond` 5 (each behind one
step of a long scene). Counts depend on the build directory.
`BOF3X_SHADOW='*'`: exit 0.

## 5. The controls

Fifty-eight, planted one at a time by a script (the scratch `controls.py`,
not committed: each anchored on a unique string of `scena_sc7.cpp`;
replace, build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc7`, restore;
rebuild at the end), on the final fuzz, 2026-09-27. **57 of 58 refused** by
a count (exit 3), each in the function its plant touches; D88 is an
equivalent mutant. Every one of the 52 functions has at least one refused
plant. Counts are this worktree's.

| | Planted | Refused in (rounds of 6,000) |
|---|---|---|
| F7 | Scena07_Frame: state ^ 1 | Scena07_Frame 6,000 |
| S7 | Scena07_Start: row 0x903FD0 | Scena07_Start 6,000 |
| E7a | Scena07_EnterArea: Kind2 +0x3E 0xD00 | Scena07_EnterArea 175 |
| E7b | Scena07_EnterArea: Flags_Clear 7 | Scena07_EnterArea 1,817 |
| E7c | Scena07_EnterArea: area 0xAF run 2 | Scena07_EnterArea 31 |
| R7 | Scena07_Run: the next run (inside the window) | Scena07_Run 6,000 |
| C71 | Scena07_Scene1: counter 0x14 | Scena07_Scene1 3,275 |
| C72 | Scena07_Scene2 step 0xD: +9 0x31 | Scena07_Scene2 185 |
| C72b | Scena07_Scene2 step 5: facing 0x82 | Scena07_Scene2 9 |
| C73 | Scena07_Scene3 step 4: facing 0x84 | Scena07_Scene3 156 |
| C74 | Scena07_Scene4 step 0: +0x10 + 1 | Scena07_Scene4 61 |
| T7 | Scena07_TimedEffects: sound % 2 | Scena07_TimedEffects 1,502 |
| C75 | Scena07_Scene5: track 0x4E | Scena07_Scene5 472 |
| C76 | Scena07_Scene6 tail: past 0x20 | Scena07_Scene6 101 |
| C76b | Scena07_Scene6 step 0x16: +0x14 0xFFF00000 | Scena07_Scene6 110 |
| K7 | Scena07_ShakeCamera: << 1 | Scena07_ShakeCamera 3,016 |
| P7 | Scena07_PlaceObjects: state 6 | Scena07_PlaceObjects 6,000 |
| C77 | Scena07_Scene7: 12 bytes of the name | Scena07_Scene7 1,552 |
| C78 | Scena07_Scene8: step 1 | Scena07_Scene8 58 |
| O7h | Scena07_ObjectHook: the next handler | Scena07_ObjectHook 6,000 |
| O70 | Scena07_Object0: animation 4 | Scena07_Object0 6,000 |
| O71 | Scena07_Object1: key item 6 | Scena07_Object1 6,000 |
| O72 | Scena07_Object2: drop-in 8 | Scena07_Object2 6,000 |
| O73 | Scena07_Object3: step 1 | Scena07_Object3 2,065 |
| O74 | Scena07_Object4: step 0xB | Scena07_Object4 2,031 |
| X49 | Scena07_TakeEffect49: kind 0x48 | Scena07_TakeEffect49 5,716 |
| H7s | Scena07_StepHook: kind 4 not excluded | Scena07_StepHook 28 |
| H7a | Scena07_ArriveHook: z >= 0x200000 refused | Scena07_ArriveHook 508 |
| P89 | Scena07_PartyHas89State2: 3 | Scena07_PartyHas89State2 2,891 |
| CH7 | Scena07_CellHook: 0xFE | Scena07_CellHook 3,058 |
| CL7 | Scena07_Cell0: step 0xB | Scena07_Cell0 6,000 |
| F8 | Scena08_Frame: the next state (inside the window) | Scena08_Frame 6,000 |
| S8 | Scena08_Start: story flag 0x44 | Scena08_Start 6,000 |
| PB | Scena08_PartyBitsToRecord0: records 2..7 | Scena08_PartyBitsToRecord0 6,000 |
| E8a | Scena08_EnterArea: sprite 1 +0x83 0xF | Scena08_EnterArea 33 |
| E8b | Scena08_EnterArea: flag 0x14 as the source seems to mean | Scena08_EnterArea 12 |
| R8 | Scena08_Run: run ^ 1 | Scena08_Run 6,000 |
| D81 | Scena08_Scene1 step 6: +0x5C 0x65 | Scena08_Scene1 23 |
| D82 | Scena08_Scene2: 0x9039F4 = 4 | Scena08_Scene2 81 |
| D83 | Scena08_Scene3 step 0: << 2 | Scena08_Scene3 66 |
| D84 | Scena08_Scene4: 0xBB9 | Scena08_Scene4 1,196 |
| D85 | Scena08_Scene5: Cond_ByteFE 2 | Scena08_Scene5 566 |
| D86 | Scena08_Scene6 step 4: +9 0x5B | Scena08_Scene6 145 |
| D87 | Scena08_Scene7 step 0xF: +0x1C 4 | Scena08_Scene7 792 |
| D88 | Scena08_Scene8 step 0x1A: / 0xA0 | not refused: equivalent - for every pointer into the 30 Sprite_Objects records, / 0xA0 and / 0xA4 give the same index; D88b is its near variant |
| D88b | Scena08_Scene8 step 0x1A: the index + 1 | Scena08_Scene8 4 |
| SP | Scena08_SpawnPair: animation 0x37 | Scena08_SpawnPair 5,650 |
| D89 | Scena08_Scene9 step 7: facing 2 | Scena08_Scene9 38 |
| D8A | Scena08_Scene10: flag 0x15 | Scena08_Scene10 286 |
| D8B | Scena08_Scene11 step 7: \| 0x40 | Scena08_Scene11 33 |
| R4 | Scena08_SetUpRecord4: +0x14 0x22 | Scena08_SetUpRecord4 6,000 |
| O8h | Scena08_ObjectHook: the next handler | Scena08_ObjectHook 6,000 |
| O80 | Scena08_Object0: timer 0x21 | Scena08_Object0 6,000 |
| O81 | Scena08_Object1: step 0xB | Scena08_Object1 6,000 |
| O82 | Scena08_Object2: flag 0x12 | Scena08_Object2 6,000 |
| KI | Scena08_SwapKeyItem4: 0xD | Scena08_SwapKeyItem4 5,999 |
| H8s | Scena08_StepHook: z cells 0x2E..0x30 | Scena08_StepHook 319 |
| H8a | Scena08_ArriveHook: z 0xD8000 | Scena08_ArriveHook 59 |

The first run of the controls (on the fuzz before the hook hits carried
their areas) had four not refused: H7s (the step hook's kind-4 test: the
area 0x52 rectangle and kind 4 rarely met in one round) - refused after the
seed picked each hook hit together with its area; R7 and F8 crashed both
sides' copies in ours only (a plant of `^ 1` sends the run / state past its
swapped window into data - the plant was wrong, rewritten inside the
window); and D88, equivalent (a sprite pointer anywhere in the 30-record
pool divides to the same index by 0xA0 as by 0xA4), with D88b, its near
variant, refused. The thinnest are E8b and C72b (9..12 rounds), D88b (4).

## 6. Cross-group calls and callees

By address in `scena_sc7_callees.h` (nobody's this wave):

| Address | What (by reading) | Owner |
|---|---|---|
| `0x532ED0` | (x, z, u8): the party placed for an event battle | nobody (harness standard) |
| `0x533E50` | (): the party's records refreshed | nobody |
| `0x56D6F0` | (): `Field_StatusBits |= 0x80` | nobody (harness standard) |
| `0x56D800` | (records, n, x, z): the cell record matched, negative none | nobody |
| `0x57CD90` | (): a free `Sprite_Objects` index 0..0x1D, 0xFF none | nobody |
| `0x587B80` | (): the music buffer stopped | nobody |
| `0x591900` | (u8): into the first free of 32 key-item bytes at `0x904554` | nobody |
| `0x591BE0` | (0xBB8, 0): chapter 8 run 4, once (not read) | nobody |
| `0x498DE0` | (4): chapter 8's record-4 set-up, before its stats (not read) | nobody |

None of these is in another wave-two group's band; this band calls nothing
in SC6's (`0x54A910..0x54F080`) or SC9a's (`0x553B30..`), and neither calls
into it. The one inbound call from another band is chapter 10's
`0x55BBB1` to `Scena07_PartyHas89State2` (SC9b's band, not this wave).
The call tables' entries (CALLS, this wave) are reached only through
`Scenario_CallA` / `Scenario_CallB`, by name. Called by name (ours or
Capcom's): `Scenario_CallA` / `CallB`, `Field_StartEventBattle`,
`EventOp_0x`, `EventOp_6x`, `Char_RecalcStats`, `Char_HealHp` / `Ap`,
`Inventory_Add`, `Item_NamePtr`, `PartySet_LoadFirst` / `Second`,
`Flags_Test` / `Set` / `Clear`, `ScriptFlags_Set40` / `Clear40`,
`Party_DropIn`, `Field_ChangeArea`, `Field_ViewReset`, `Kind2_Place`,
`AreaMap_SetByte`, `AreaMap_Elevation`, `MapView_SetElevation`,
`Effect_FindFree`, `Sprite_SetAnimationAt`, `Msg_OpenScript` /
`OpenSystem`, `Transition_Start`, `Sound_PlayEffect` / `LoadStream` /
`StreamDone` / `ResumeAll`, `Music_Play` / `FadeOut` / `FadeOutStop` /
`LoadFile`, `LoadDatFile`, `File_LoadDone`, `Field_LoadingFrame`,
`Task_Sleep`.

## 7. Latent defects (Capcom's, kept; for the coordinator to number)

- **`Scena08_EnterArea` sets the wrong flag in area 0x15.** Where the
  scene of run 10 is armed (flags 0xF and 0x13 set, 0x14 not), the code
  pushes `0x14`, then `dl` = the low byte of the row pointer, then the row,
  and calls `Flags_Set` - which takes two arguments. So it sets flag
  (row pointer & 0xFF) of the row: with chapter 8's row `0x903FD0`, flag
  0xD0, bit 0 of `0x903FEA` (chapter 11's row, its flag 0x10), and never
  0x14. Run 10 sets flag 0x14 itself at its end (step 0xC), so the scene
  does not repeat once finished; left unfinished, it arms again on the next
  entry. Kept (control E8b shows the fuzz tells it apart). The PSX twin
  (`0x801FA3E4`, a hypothesis) is not read here.
- **`Scena07_TakeEffect49` answers a record past the pool when none is
  free**, and `Scena07_Scene6` writes a byte and nine dwords through it
  (`0x7E9160 + 6..+0x23`, `+0x34..+0x3F`, past `Effect_Objects`' 20 records). Kept; the
  fuzz's stand-in answers only real records.
- **Indices unchecked into pools**: `Scena08_Scene8` step 0x1B clears
  `Sprite_Objects [Scena08_Kept +0]` and step 0x1A computes that index from
  the pointer `0x903804` (a signed division, any pointer); `Scena08_Scene1`
  / `Scene11` index `CharacterRecords` by `MoveScript_EffectState` bytes;
  `Scena07_Scene4` and `Scena08_Scene6` / `8` / `10` test the effect record
  of a kept slot that may be 0xFF (a read past the pool); `Scena08_Scene4`
  reads the sprite counter 3 names. The chapter tables are indexed
  unchecked as every chapter's (state, run, object `+0x86`, cell answer).

No divergence and no ledger entry: every function is a faithful
replacement. `DIVERGENCE.md` and `cheats.cpp` name no address of the band
(grep 2026-09-27).

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D148 (the wrong flag) in
[`known-defects.md`](known-defects.md).

## 8. What reaches it

Nothing recorded: no route plays chapters 7 or 8 (the owner's recipe saves
are the live check, [`takeover-queue-scenario.md`](takeover-queue-scenario.md)
§5). Chapter 16 is the only chapter the attract cycle runs, so the frame
hash is untouched.

`analysis/calltrace/entries_logic.txt`: 52 lines appended under a
`group SC7` comment; six host lines (`005508C0 414`, `00550CE0 298`,
`00550F80 DD`, `00551060 1C40`, `00553630 FB`, `00553730 4E1F`) ran over
their neighbours and are superseded by the smaller lines.
