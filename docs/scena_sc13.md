# Chapters 13 and 14: 0x561DB0..0x567DC0

**Status:** IN PROGRESS (2026-09-28) - 51 functions ours
(`src/game/scena_sc13.cpp`, shadow name `scena_sc13`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)),
two runs (chapter 13's 23 with `Cond_ByteFA` 13, chapter 14's 28 with 14):
0 mismatches in 816,000 rounds (16,000 a function); 238 of 240 negative controls refused by a count, the other two equivalent mutants whose near variants are refused. Fuzz only:
no recorded route reaches chapters 13 or 14 (section 8).

Group SC13 of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §9,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3). The brief
called the band "chapter 12's tail and chapters 13 and 14"; read to its
last instruction, the band holds chapter 13's code from its first byte and
chapter 14's, plus one function eight chapters share as their state 0
(section 3). Nothing of chapter 12 but that shared function lies here:
chapter 12's block ends at `0x561DB0` exactly, where chapter 13's frame
begins.

## 1. The band, the starts, and the roots

`tools/scenario_rows.py --unit SC13` (at `e4882fb`): 51 functions, 0x5EDE
bytes, 0 ours, 1 not walked. This reading (capstone recursive descent of
each start, `scratchpad/sc13/sc13dis.py`, not committed) agrees start for
start and extent for extent, call site for call site, with one
qualification: the tool counts `Scena13_Run4`'s byte table at `0x563070`
(0x20 bytes) into its extent (0x3A0) where a plain descent stops at the
jump table's end (0x380) - the tool is right, the byte table is the
function's. No start was dropped, none added; no start is a jump-table or
switch case, and no function lies inside another's extent.

**The one start the walk did not reach, `0x5658B0`** (`Scena14_LeaveToC4`,
0x5A bytes): a whole function (a `ret`-ended body between
`Scena14_Shake` and `Scena14_Run3`) that no table, call or stored pointer
names - a byte search of the whole image for its address finds nothing,
and no chapter's state, run or object table holds it. It clears bit 0 of
two party objects' `+0x24`, leaves for area 0xC4 and starts run 2 at step
0. Taken like the rest (it is in the band), fuzzed like a state handler;
unreachable in the game as far as the image shows.

**The roots** (`tools/scenario_roots.py --chapter 13` / `14`):

| Chapter | Table | Entry | Target |
|---|---|---|---|
| 13 | `Scena13_Hooks` `0x661788` | slot 0, the frame | `0x561DB0` Scena13_Frame |
| | | slot 1, the object trigger | `0x564120` Scena13_ObjectTrigger |
| | | slot 2, the step hook | `0x5641C0` Scena13_StepHook |
| | | slot 3, the arrive hook | `0x5645D0` Scena13_ArriveHook |
| | | slot 4 | 0 (none) |
| | call table A `0x65F850` | 8 entries | `0x51A5A0`..`0x51A770`, ours (CALLS) |
| | call table B `0x65F870` | 1 entry | `0x437CC0`, the bare `ret` |
| 14 | `Scena14_Hooks` `0x661820` | slot 0 | `0x5646A0` Scena14_Frame |
| | | slot 1 | `0x5677B0` Scena14_ObjectTrigger |
| | | slot 2 | `0x567970` Scena14_StepHook |
| | | slot 3 | `0x567A90` Scena14_ArriveHook |
| | | slot 4 | 0 (none) |
| | call table A `0x65F874` | 13 entries | CALLS's, ours |
| | call table B `0x65F8A8` | 1 entry | `0x51AAF0` Scena14_RecountLeaveAll, ours |

**PSX twins, by table position only (not read):** the sibling's
`names/scenario_records.toml` gives SCENA13's vtable entries 0..3 as
`0x801F7D14`, `0x801FB350`, `0x801FB464`, `0x801FBA34` and SCENA14's as
`0x801F6FD8`, `0x801FB37C`, `0x801FB648`, `0x801FB84C` - hypotheses for
the eight slot functions above, not evidence; no name was taken from them.

The walk's chapter 13 closure (75 functions) runs on into chapter 14's
block and past `0x567A90`: the lesson of wave two - it reads chapter 13's
tables as far as the next non-code dword. By the tables' own bounds
(section 2) the chapters share nothing but `0x5646B0`.

## 2. The tables

Chapter 13's tables follow its vtable, chapter 14's its own (`symbols.toml`
`[[data]]`), and each chapter keeps its effect slot in a byte of its own:

| Address | Name | Entries | Read by | Holds |
|---|---|--:|---|---|
| `0x66179C` | `Scena13_States` | 3 | Scena13_Frame, `jmp [+ s8 0x8034E2 * 4]` | Start, EnterArea, Run |
| `0x6617A8` | `Scena13_Runs` | 9 | Scena13_Run, `jmp [+ s8 MoveScript_Var7 * 4]` | ret, Run1..Run8 (then a 0 dword) |
| `0x6617D0` | `Scena13_EventOps` | 4 x 16 bytes | Scena13_SpawnPairA / B, pushed to `EventOp_6x` | four event-op records |
| `0x661810` | `Scena13_Objects` | 4 | Scena13_ObjectTrigger, `call [+ object[0x86] * 4]` (object, bits) | Object00..03 |
| `0x661834` | `Scena14_States` | 3 | Scena14_Frame | ScenaShared_State0, EnterArea, Run |
| `0x661840` | `Scena14_Runs` | 10 | Scena14_Run | ret, Run1..Run7, ret, ret |
| `0x661868` | `Scena14_ShakeOffsets` | 4 signed bytes | Scena14_Shake by `Frame_Counter & 3` | 0, 1, 0, -1 |
| `0x66186C` | `Scena14_TalkWho` | 2 x 4 bytes | Scena14_TalkByMember (from Run6) | member bytes (ObjTrio `+0x89`) |
| `0x661874` | `Scena14_TalkLines` | 2 x 4 words | the same | the message each member byte picks |
| `0x661884` | `Scena14_Objects` | 8 | Scena14_ObjectTrigger (object, bits) | Object00..07 (then a 0 dword, `Scena15_Hooks`) |
| `0x6BC738` | `Scena13_Slot` | byte | chapter 13's scenes | the effect slot taken and waited on |
| `0x6BC73C` | `Scena14_Slot` | byte | chapter 14's | the same |

The originals index every table unchecked (a signed byte for the state and
the run, `object +0x86` for the objects); an index past a table reads the
next one. **Ours aborts with a message** for an index outside the table's
own entries (negative included) - the project's rule for an index past a
table (round nine §6; the owner: no DIVERGENCE entry). No code in the band
writes such an index: the runs chapter 13 starts are 1..8 and chapter 14's
2..7. `MoveScript_Var7` is also set by the movement script's op F6, so a
script storing run 9 in chapter 13 (or 10 in 14) would stop here where the
original ran the next table's entry - the same edge as SC12's.

## 3. `0x5646B0`, the shared state 0

`ScenaShared_State0` is `mov byte [0x8034E2], 1 / ret`: state 0 of eight
chapters. A byte search of the image for its address finds exactly eight
references, all state tables' entry 0: chapters 5 (`0x661034`), 9
(`0x6613FC`), 10 (`0x661588`), 12 (`0x6616F4`), 14 (`0x661834`), 15
(`0x661924`), 18 (`0x662C44`) and 19 (`0x662C6C`). Its address lies in
chapter 14's block, between Scena14_Frame and Scena14_EnterArea, so it is
chapter 14's by position and this group's by band; the other seven tables
reach the same bytes (one copy of an identical body kept for all eight,
presumably the linker's folding - the image does not say). Chapters 13, 16
and the rest have a state 0 of their own (chapter 13's `Scena13_Start`
does work: flags, the party pass, a call-table entry). Fuzzed under
chapter 14; it reads no chapter byte, so the chapter does not matter.

## 4. The functions

Shapes: **slot** - vtable slot 0, no arguments; **object** - slot 1 or an
object-table entry, the object; **hook** - slots 2 and 3, (x, z) 16.16
answering in al; **state** - a handler reached through a chapter table, or
called directly, no arguments; **entry** - a helper of the chapter's own
called with arguments (the harness's `kEntry`).

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x561DB0` | Scena13_Frame | 0xE | slot 0 | `Scena13_States[state]` (tail jump) |
| `0x561DC0` | Scena13_Start | 0x3B | state 0 | Set40, pass flags 0x1F, the party pass `0x533E50`, CallA(0), story flag 0x43 set and 0x42 cleared (the literal `0x904030`), state 1 |
| `0x561E00` | Scena13_EnterArea | 0x47E | state 1 | by area (0x56, 0x58, 0x70, 0x7E, 0x8F, 0x90, 0x91, 0xC2) and `Cond_ByteFD`: flags, drop-ins, call-table A entries, camera angles and distance, camera effects, area code `0x420A90`, runs 1 / 3 / 4 / 7, the event-object pairs in 0xC2; three tests jump straight to the end; every exit state 2 |
| `0x562280` | Scena13_Run | 0xE | state 2 | `Scena13_Runs[run]` (tail jump) |
| `0x562290` | Scena13_Run1 | 0x170 | state (run 1) | steps 0..5: a camera effect, a transition, area 0x7E, back |
| `0x562400` | Scena13_Run2 | 0x2F8 | state (run 2) | steps 0..0xB: three trips within 0xC2, flags 2..5, a stream waited for, area 0x97 |
| `0x562700` | Scena13_SpawnPairA | 0x94 | state (called) | two free sprite records (`0x57CD90`), each given an event-op record by `EventOp_6x` with its slot in `0x903850` |
| `0x5627A0` | Scena13_SpawnPairB | 0x94 | state (called) | the same with records 2, 3 |
| `0x562840` | Scena13_Run3 | 0x4A9 | state (run 3) | 16 cases over 0..0x18 (byte table `0x562CD0`) |
| `0x562CF0` | Scena13_Run4 | 0x3A0 | state (run 4) | 17 cases over 0..0x1F (byte table `0x563070`) |
| `0x563090` | Scena13_Run5 | 0x300 | state (run 5) | the dial (below) |
| `0x563390` | Scena13_ToneLevels | 0x5C | entry (angle) | two voices' levels (`0x587890`) from `Math_Sin` of the dial |
| `0x5633F0` | Scena13_Run6 | 0x624 | state (run 6) | 22 cases over 0..0x1E |
| `0x563A20` | Scena13_Run7 | 0x4F8 | state (run 7) | four captions, then a battle |
| `0x563F20` | Scena13_Caption | 0xB9 | entry (index, seconds) | a caption drawn with the CLUT strip faded in and out |
| `0x563FE0` | Scena13_Run8 | 0x140 | state (run 8) | steps 0, 4..6, 8, 0xA, 0xB |
| `0x564120` | Scena13_ObjectTrigger | 0x1F | slot 1, object | `Scena13_Objects[object +0x86]` (object, bits) |
| `0x564140` | Scena13_Object00 | 0xC | object | the object's word `+0x8A` + 1 |
| `0x564150` | Scena13_Object01 | 0x14 | object | run 3, step 5 |
| `0x564170` | Scena13_Object02 | 0x14 | object | run 6, step 0x19 |
| `0x564190` | Scena13_Object03 | 0x2B | object | flag 0x1C, Set40, bytes `0x9039F3..F5` = 0x2C, 0, 0x11 |
| `0x5641C0` | Scena13_StepHook | 0x40B | slot 2, hook | areas 0x56..0x9B by rectangles and flags (below) |
| `0x5645D0` | Scena13_ArriveHook | 0xC8 | slot 3, hook | area 0x8F run 8; area 0x91 a flag and `MoveCmd_TestFB` |
| `0x5646A0` | Scena14_Frame | 0xE | slot 0 | `Scena14_States[state]` |
| `0x5646B0` | ScenaShared_State0 | 0x8 | state 0 | state 1 (section 3) |
| `0x5646C0` | Scena14_EnterArea | 0x741 | state 1 | by area (0xB..0xC5, 14 of them): flags, drop-ins, party placements, camera effects, the grey CLUT in 0xB, runs 2 / 5 / 7; area 0xAC at FD not 0 jumps to the end; every exit state 2 |
| `0x564E10` | Scena14_GreyClut | 0x70 | state (called) | the 0x2000 CLUT colours greyed (the mean of the channels), dirty 1 |
| `0x564E80` | Scena14_SpawnEffect | 0x67 | entry (6 words) -> al | a camera effect by `Effect_FindFree` to `Scena14_Slot` |
| `0x564EF0` | Scena14_Run | 0xE | state 2 | `Scena14_Runs[run]` |
| `0x564F00` | Scena14_Run1 | 0x336 | state (run 1) | a three-question choice with `Menu_DrawHand` (below) |
| `0x565240` | Scena14_Run2 | 0x634 | state (run 2) | 25 cases over 0..0x24 |
| `0x565880` | Scena14_Shake | 0x28 | entry (amount) | `Camera_ShiftY` += `Scena14_ShakeOffsets[frame & 3]` * amount |
| `0x5658B0` | Scena14_LeaveToC4 | 0x5A | state (reached by nothing) | section 1 |
| `0x565910` | Scena14_Run3 | 0x2D8 | state (run 3) | steps 0..0xC within area 0x96 |
| `0x565BF0` | Scena14_Run4 | 0x410 | state (run 4) | 17 cases over 0..0x18 |
| `0x566000` | Scena14_ScrollView | 0x83 | state (called) | the view moved east by 0x14 cells when `MapView_FocusX` is exactly 0x77FF |
| `0x566090` | Scena14_Run5 | 0x3CA | state (run 5) | 18 cases over 0..0x21 (byte table `0x566438`) |
| `0x566460` | Scena14_Run6 | 0xBE0 | state (run 6) | 74-entry jump table over 0..0x49 (below) |
| `0x567040` | Scena14_TalkByMember | 0x93 | entry (who, lines) -> ax | the line for the first listed member present |
| `0x5670E0` | Scena14_Run7 | 0x6CC | state (run 7) | 25 cases over 0..0x20 |
| `0x5677B0` | Scena14_ObjectTrigger | 0x1F | slot 1, object | `Scena14_Objects[object +0x86]` |
| `0x5677D0`..`0x5678D0` | Scena14_Object00..04 | 0x39 each | object | a line the area code picks (`0x42C0A0` in area 0xC0, else `0x42BA90`) for 8, 4, 2, 5, 6 |
| `0x567910` | Scena14_Object05 | 0x12 | object | run 5, step 5 |
| `0x567930` | Scena14_Object06 | 0x1F | object | run 5, step 0x1E, `+0x8A` + 1 |
| `0x567950` | Scena14_Object07 | 0x1F | object | run 7, step 0x1E, `+0x8A` + 1 |
| `0x567970` | Scena14_StepHook | 0x117 | slot 2, hook | area 0x94 a rectangle; area 0xBF the cell byte 0xA6 under the step |
| `0x567A90` | Scena14_ArriveHook | 0x32F | slot 3, hook | rectangles in 0x8D, 0x8E, 0x94, 0xAC, 0xBF |

Every function's evidence line in `symbols.toml` has its extent, tables
and call count. What the scenes are in the story is not read here: the
areas and flags are numbers, and a scene's meaning is the owner's to say.

**The state machines** are chapter 12's (see [`scena_sc12.md`](scena_sc12.md)
§3): a switch on the step byte `0x8034E5` (MSVC's two-level switch where the
cases are sparse), each case waiting on counter 0 (`0x903848`) or counter 3
(`0x90384B`) at a value, on `Field_Request`, on the wait word `0x66C810`, on
the word timer `0x8034E6` counted down, or on an effect record's in-use bit,
then doing one thing and storing the next step.

**Recurring shapes, written once:**

- *The camera effect*: `Effect_FindFree` to the chapter's slot byte; for a
  slot, `+0` = 1, `+5` the kind (0x13, 0x31), the dwords `+0x64` / `+0x68` /
  `+0x6C` (an angle, `Camera_Angles +2`, a distance or `Cond_AngleFB`) and
  the life byte `+9`; kind 0x31 also `+0xC` = 0. The next step waits for
  `+0` bit 0 to clear. Chapter 14 does it through `Scena14_SpawnEffect`,
  whose al its callers test.
- *The field effect at a cell* (chapter 14, kinds 0x97 / 0x98): `+0x34` /
  `+0x38` = (0x1C8000, 0x268000) and `+0x3C` = (`AreaMap_Elevation` there
  as a signed word + 0x80) << 16.
- *A visit* (chapter 14's run 6): `Field_ChangeArea`, then the bytes
  `0x904EE0`, `0x937F98` and the music byte `0x904CD0` 0xFF; on the next
  steps the grey CLUT and a transition in, a hold, a fade out, a message.
  The four repeated handlers compute the next step as the step byte + 1,
  read afresh after their call, and the jump table holds each at up to
  seven steps.
- *Run 5's dial* (chapter 13): the dword at `Sprite_ObjectsExtra` record 1
  `+0x6C` turns by 0x10 while `Input_Held` bit 0x2000 or 0x8000 is held,
  `Scena13_ToneLevels` following it (two sound voices' levels,
  `|Math_Sin((a - 0x4C0) >> 1)| >> 5` and the same of `a - 0xCC0`), until
  `Input_Pressed` bit 0x20; 0x4A0..0x4E0 (the low 12 bits) passes.
- *Run 7's captions* (chapter 13): `Scena13_Caption(index, seconds)` each
  frame while the timer counts down from 30 x seconds: the caption drawn
  with `Text_DrawAt` from the offsets at `0x803580`, the CLUT strip faded in
  over the first 0x20 frames (`ClutStrip_FadeTo(clock)`), restored at 0x20,
  faded out over the last 0x20, restored at the end.
- *Chapter 14's run 7, step 0x17*: records 1..7 of the eight at `0x903A70`
  with `+0xB` bit 0 get `+0x1C` = `+0x2E`, `+0x18` = `+0x20`, `+0x1A` =
  `+0x22`, `+0x10` = 0; then each member after the leader has the 0xA4
  bytes of the record `MoveScript_EffectState[its party byte]` names copied
  to its ObjTrio `+0x80` - a party restored before a battle.

**As the originals have them** (each kept, and each said at its site):
chapter 13's run 4 holds the switch bound 0x1F in `ebx`, and step 0x1E
stores that `ebx` to `Draw_PassFlags` and to the step (so 0x1F, not 1 and
0x1E + 1); chapter 14's run 4 likewise holds 0x18, which step 0xB compares
counter 3 with and step 0x17 stores as the step; chapter 13's run 7, step
0x16 waits for the timer at 0 without counting it down (step 0x15 left it
0x1E; the movement script may count it, nothing in this band does);
chapter 13's step hook, in area 0x90, stores the facing byte it last
loaded into `al` to `0x903850`; chapter 14's `Scena14_EnterArea` in area
0xB stores `al` - the 0 its last `Flags_Test` answered - to
`Draw_PassFlags`; chapter 14's arrive hook starts runs in areas 0x8D, 0x8E
and 0xAC and still answers 0; `Scena14_TalkByMember` answers the line, and
its caller ignores it; `0x591900` reads no argument but is pushed one.

## 5. The fuzz

`scena_sc13_fuzz.cpp`, through `scenario_harness`, two `Run` calls under
the one shadow name (SC3's pattern), chapter 13's with `g.chapter = 13`
and chapter 14's with 14:

- **The clones**: `tools/scenario_rows.py --unit SC13 --clones` at
  `e4882fb`, identical to the reading. Shapes: the frames `kSlot`; states,
  runs, `ScenaShared_State0`, the called helpers without arguments and
  `Scena14_LeaveToC4` `kState`; the object triggers and the twelve object
  handlers `kObject` (the object pointer is an argument some read); the
  four hooks `kHook` (al); `Scena13_ToneLevels`, `Scena13_Caption`,
  `Scena14_SpawnEffect` (al), `Scena14_Shake` and `Scena14_TalkByMember`
  (ax, `ret_mask` 0xFFFF) `kEntry` with arguments set by `Args`.
- **One copy the group makes itself.** `Scena14_EnterArea` has 87 call
  sites; the harness re-aims at most 64 a clone. The fuzz file copies it
  with `bof3::CloneOriginal`, every site re-aimed at a trampoline that
  calls the harness's recorder for that callee (`scenario_harness::StandIn`,
  on either pass), and hands the harness a six-byte `jmp [copy]` as its
  original (SC12's pattern for Run4 / Run8). No harness edit.
- **Callees**: the harness's 70 standard ones, plus the group's list:
  `Flags_Test` as a `kFlag` (tested on al alone), `Field_StartEventBattle`
  by name (ours, SE's - the standard set lists it by raw address),
  `Menu_DrawHand`, `ClutStrip_FadeTo` masked to a byte (it clamps to the
  byte level; the callers leave garbage above), `Math_Sin` as garbage (so
  the absolute value, shift and mask see every sign), `AreaMap_ByteAt`
  with an `effect` answering 0xA6 half the time (the byte chapter 14's step
  hook counts), `EventOp_6x` with an `effect` logging the slot word
  `0x903850` the caller set; the group's own helpers by name
  (`kPhase` for the argument-less ones); and by raw address the callees
  nobody owns (section 7).
- **Tables**: the four state and run tables swapped for recorders; the two
  object tables pass arguments a table recorder does not log, so the seed
  writes a typed stand-in into every entry (one per index) and the tables
  are regions (SC0's `ObjectEntry`).
- **Regions** beyond the standard 22 (12 for chapter 13, 15 for 14): the
  slot bytes, the word `0x802290`, `MapView_ElevationOffset`, the music
  bytes `0x904131..0x904153`, `0x904CD0`, `0x904EE0`, `Cond_ByteFE`,
  `0x9039F3..F5`, `0x903800` (`Camera_ShiftY` and the object pointer
  `0x903804`), the caption offsets and index; for 14 the CLUT strip
  (0x4000 bytes), `MapView_Origin`, the eight records at `0x903A70`, the
  record index `0x669734`, `0x929F10`, `Draw_OtSlot`, `Draw_SortOnX`; and
  the two object tables.
- **Seed**, every round: the member count 0..3 (run 7 of chapter 14 copies
  a record per member into ObjTrio), the slot bytes 0..0x13, the record
  index 0..7 and the object pointer `0x903804` at a sprite record - each of
  which the originals write through; then, most of the time each, the
  areas and `Cond_ByteFD` values the chapter tests, the request byte 0 / 1
  / 2 / 6, the wait word 0, the timer at 0, 1, 2 and the pull-out's bounds,
  counter 3 at its tested values, the members' `+0x89` and the leader's
  facing, the word `0x802290`, the input words, the dial, `0x905E68` 5,
  `MapView_FocusX` 0x77FF. Per role: each run's case steps and one past
  with the counter each step waits on two times in three (`kWaits`); the
  state 0..2, the run inside its table, `object +0x86` inside its table;
  the caption's timer at the clock's edges (0, 0x20, total - 0x20, total,
  one either side); the hooks one of their rectangles with its area and
  `Cond_ByteFD` and x, z at each edge (whole x at the bound and one either
  side, cells at the range's ends and one outside, fractions 0 half the
  time).
- **Disturbance** beyond the harness's, from its hash only: counter 3, the
  area, `Cond_ByteFD`, a slot byte, an effect's in-use byte, a member's
  `+0x89`, the facing, script-flag bits 3 and 4, the member count, the word
  `0x802290`, the word `0x903850`, `Field_StatusBits` bit 0. The group's
  disturbance reaches a group cell only about one call in 24, so a
  `settle` (after every disturbance, drawing on `Noise()`) also flips a
  bit of `Field_StatusBits` and moves the object pointer `0x903804` a
  quarter of the time each, and, for the functions that read the area
  again after their calls (the enter-area states and the hooks), moves the
  area, `Cond_ByteFD` and the facing half the time each - chapter 13's
  step hook half of those times between areas 0x8F (FD 1) and 0x90 (FD 2)
  together, the pair whose facing load and store sit on either side of a
  `Flags_Test`.

**Result, in this worktree** (`BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=scena_sc13`, exit 0, at `cf0ea5a`): chapter 13, 368,000
rounds, 513,825 calls to the stand-ins, **0 mismatches**; chapter 14,
448,000 rounds, 491,458 calls, **0 mismatches**. Every callee of the group's list and
every table entry is reached (coverage in the log); the thinnest:
`Scenario_CallB` 54 (area 0xBF of
chapter 14's enter-area, behind three flags), `0x4205D0` 68, `Field_ViewReset`
113 and `Music_FadeOut` 130 in chapter 14, `0x420A90` 159 in chapter 13;
every other callee a few hundred times or more, every state and run table
entry about 1,500..5,400 times. `BOF3X_SHADOW='*'`: exit 0.

## 6. Controls

`scratchpad/sc13/controls.py` (not committed; SC5's pattern): each mutant
planted in `scena_sc13.cpp` on a unique anchor, rebuilt, the self-test run,
restored, and rebuilt at the end. Refused = exit 3 on a mismatch.

| Id | Mutant | Result |
|---|---|---|
| F1 | frame13: state 1 read as 2 | refused (exit 3) |
| F2 | run13: run 3 read as 4 | refused (exit 3) |
| F3 | objtrig13: the row not passed | refused (exit 3) |
| F4 | objtrig13: index + 1 below 3 | refused (exit 3) |
| S1 | start13: story flag 0x44 | refused (exit 3) |
| S2 | start13: state 2 | refused (exit 3) |
| E1 | enter13: 0x56 word 0x71 | refused (exit 3) |
| E2 | enter13: 0x58 angle 0xFD2B | refused (exit 3) |
| E3 | enter13: 0x70 FD 3 | refused (exit 3) |
| E4 | enter13: 0x7E run 2 | refused (exit 3) |
| E5 | enter13: 0x8F life 0xFE | refused (exit 3) |
| E6 | enter13: 0x8F area code arg 2 | refused (exit 3) |
| E7 | enter13: 0x90 fade 0x11 | refused (exit 3) |
| E8 | enter13: 0x91 FD 4 drop-in 7 | refused (exit 3) |
| E9 | enter13: 0xC2 distance 0xF201 | refused (exit 3) |
| E10 | enter13: 0xC2 flag 5 for 4 | refused (exit 3) |
| E11 | enter13: state 1 at the end | refused (exit 3) |
| E12 | enter13: 0x56 second test FD 1 | refused (exit 3) |
| E13 | enter13: 0x90 FD read once (before the flag) | refused (exit 3) |
| P1 | pair: the first record kept when the second fails | refused (exit 3) |
| P2 | pair: slot word b for a | refused (exit 3) |
| P3 | pairB: records 3 / 2 | refused (exit 3) |
| R1a | run1: counter 9 | refused (exit 3) |
| R1b | run1: area x 0x4E8000 | refused (exit 3) |
| R1c | run1: bit 3 cleared after Clear40 | refused (exit 3) |
| R2a | run2: counter 4 | refused (exit 3) |
| R2b | run2: kind 0x76 | refused (exit 3) |
| R2c | run2: stream 1 | refused (exit 3) |
| R2d | run2: transition 0xA | refused (exit 3) |
| R2e | run2: flags 0x82 | refused (exit 3) |
| R3a | run3: step 0 z 0x298 | refused (exit 3) |
| R3b | run3: step 1 offset 0xF000 | refused (exit 3) |
| R3c | run3: 0x591920 arg 0xC | refused (exit 3) |
| R3d | run3: request set before the message | refused (exit 3) |
| R3e | run3: track 0x5D | refused (exit 3) |
| R3f | run3: timer 0x45 | refused (exit 3) |
| R3g | run3: elevation (z, x) | refused (exit 3) |
| R3h | run3: step 0xD request tested before the effect | refused (exit 3) |
| R4a | run4: step 0x1E pass 0 | refused (exit 3) |
| R4b | run4: step 0x1E step 0 | refused (exit 3) |
| R4c | run4: spawn arg 6 | refused (exit 3) |
| R4d | run4: effect arg of the next member | refused (exit 3) |
| R4e | run4: request after the fade | refused (exit 3) |
| R4f | run4: flags 0x80 | refused (exit 3) |
| R4g | run4: offset 0xFE00 | refused (exit 3) |
| T1 | tones: no absolute value | refused (exit 3) |
| T2 | tones: shift 4 | refused (exit 3) |
| T3 | tones: 0xCB0 | refused (exit 3) |
| T4 | tones: angle not masked | refused (exit 3) |
| R5a | run5: held bit 0x1000 | refused (exit 3) |
| R5b | run5: turn 0x20 | refused (exit 3) |
| R5c | run5: bound 0x4A1 | refused (exit 3) |
| R5d | run5: bound 0x4DF | refused (exit 3) |
| R5e | run5: pressed 0x10 | refused (exit 3) |
| R5f | run5: effect +0x3C | refused (exit 3) |
| R5g | run5: counter read before Music_Play | refused (exit 3) |
| R5h | run5: step 1 dial 0x7FF | refused (exit 3) |
| R5i | run5: slot word 0x7FF | refused (exit 3) |
| R6a | run6: effect x from angle Y | refused (exit 3) |
| R6b | run6: counter 0xF | refused (exit 3) |
| R6c | run6: step 6 z 0x200 | refused (exit 3) |
| R6d | run6: script bit 0x20 | refused (exit 3) |
| R6e | run6: object byte 1 | refused (exit 3) |
| R6f | run6: z to +0x3C | refused (exit 3) |
| R6g | run6: bit 4 cleared before Clear40 | refused (exit 3) |
| R6h | run6: clear 0x22 | refused (exit 3) |
| R6i | run6: step 0x1A pass kept | refused (exit 3) |
| R6j | run6: object pointer read once | refused (exit 3) |
| C1 | caption: 31 frames a second | refused (exit 3) |
| C2 | caption: past the total only | refused (exit 3) |
| C3 | caption: fade-in bound 0x1F | refused (exit 3) |
| C4 | caption: text + 0 | refused (exit 3) |
| C5 | caption: last fade strict | refused (exit 3) |
| C6 | caption: the clock not re-read | refused (exit 3) |
| R7a | run7: caption 6 for 4 seconds | refused (exit 3) |
| R7b | run7: effect x -0x2EB | refused (exit 3) |
| R7c | run7: timer 0x12B | refused (exit 3) |
| R7d | run7: step 9 pass kept | refused (exit 3) |
| R7e | run7: distance + 0x48 | refused (exit 3) |
| R7f | run7: step 0x16 at 1 | refused (exit 3) |
| R7g | run7: kind 0x26 | refused (exit 3) |
| R7h | run7: counter after the drop-in | refused (exit 3) |
| R7i | run7: step 8 transition first | refused (exit 3) |
| R8a | run8: record 0 +0x137 | refused (exit 3) |
| R8b | run8: message 0x4F | refused (exit 3) |
| R8c | run8: no 0x56D6F0 | refused (exit 3) |
| R8d | run8: counter 0x29 | refused (exit 3) |
| O1 | object00: +0x88 | refused (exit 3) |
| O2 | object01: step 6 | refused (exit 3) |
| O3 | object02: run 7 | refused (exit 3) |
| O4 | object03: 0x12 | refused (exit 3) |
| H1 | step13: 0x56 z range 3 | refused (exit 3) |
| H2 | step13: 0x70 facing 5 | refused (exit 3) |
| H3 | step13: 0x7F x 0x38001 | refused (exit 3) |
| H4 | step13: 0x8F run 8 step 6 | refused (exit 3) |
| H5 | step13: 0x90 facing not stored | refused (exit 3) |
| H6 | step13: 0x90 drop-in 1 for others | refused (exit 3) |
| H7 | step13: 0x90 leader 6 | refused (exit 3) |
| H8 | step13: 0x91 story flag 0x2E | refused (exit 3) |
| H9 | step13: 0x97 z range 8 | refused (exit 3) |
| H10 | step13: 0x9B x 0x28001 | refused (exit 3) |
| H11 | step13: facing not loaded outside 0x70 | refused (exit 3) |
| H12 | step13: 0x8F FD 1 reload dropped | refused (exit 3) |
| A1 | arrive13: x 0x2F | refused (exit 3) |
| A2 | arrive13: z 0x6B | refused (exit 3) |
| A3 | arrive13: TestFB x 0x41 | refused (exit 3) |
| A4 | arrive13: 0x91 answers 1 | refused (exit 3) |
| G1 | frame14: state 1 read as 2 | refused (exit 3) |
| G2 | run14: run 2 read as 3 | refused (exit 3) |
| G3 | objtrig14: the row not passed | refused (exit 3) |
| G4 | state0: state 2 | refused (exit 3) |
| GC1 | grey: divisor 4 | refused (exit 3) |
| GC2 | grey: bit 15 dropped | refused (exit 3) |
| GC3 | grey: dirty 2 | refused (exit 3) |
| SE1 | spawn14: z zero-extended | refused (exit 3) |
| SE2 | spawn14: +0xC life | refused (exit 3) |
| SE3 | spawn14: answers 2 | refused (exit 3) |
| SE4 | spawn14: chapter 13s slot cell | refused (exit 3) |
| X1 | enter14: 0xB pass 1 | refused (exit 3) |
| X2 | enter14: 0x2B kind 0x23 | refused (exit 3) |
| X3 | enter14: 0x8D life 0xF1 | refused (exit 3) |
| X4 | enter14: 0x8D flag 0x19 | refused (exit 3) |
| X5 | enter14: 0x8E step 5 | refused (exit 3) |
| X6 | enter14: 0x96 FE 2 | refused (exit 3) |
| X7 | enter14: 0x96 timer 0x3D | refused (exit 3) |
| X8 | enter14: 0xAC exit at FD 1 | refused (exit 3) |
| X9 | enter14: 0xAC y + 1 | refused (exit 3) |
| X10 | enter14: 0xAC kind 0x99 | refused (exit 3) |
| X11 | enter14: 0xBD bit 0x20 | refused (exit 3) |
| X12 | enter14: 0xBF clear 4 | refused (exit 3) |
| X13 | enter14: 0xC5 byte 4 | refused (exit 3) |
| X14 | enter14: state 1 at the end | refused (exit 3) |
| X15 | enter14: 0xBF CallB 1 | refused (exit 3) |
| U1 | run1_14: music byte + 1 | refused (exit 3) |
| U2 | run1_14: status bits read before the change | refused (exit 3) |
| U3 | run1_14: hand y 0x29 | refused (exit 3) |
| U4 | run1_14: z 0x401 | refused (exit 3) |
| U5 | run1_14: clear 2 | refused (exit 3) |
| U6 | run1_14: story 0x78 | refused (exit 3) |
| U7 | run1_14: 0x904152 = 1 | refused (exit 3) |
| V1 | run2_14: divisor 239 | refused (exit 3) |
| V2 | run2_14: bound 0xB5 | refused (exit 3) |
| V3 | run2_14: divisor 181 | refused (exit 3) |
| V4 | run2_14: kind 0x24 | refused (exit 3) |
| V5 | run2_14: step 3 at 1 | refused (exit 3) |
| V6 | run2_14: record +0x1E 8 | refused (exit 3) |
| V7 | run2_14: two of the item | refused (exit 3) |
| V8 | run2_14: step 0x23 + 2 | refused (exit 3) |
| V9 | run2_14: 0x904153 = 3 | refused (exit 3) |
| V10 | run2_14: run 4 at the end | refused (exit 3) |
| V11 | run2_14: step 1 distance from 0xF1 | refused (exit 3) |
| L1 | leave: record 2 +0x25 | refused (exit 3) |
| L2 | leave: run 3 | refused (exit 3) |
| L3 | leave: flags 0x81 | refused (exit 3) |
| L4 | leave: status bits read before Set40 | refused (exit 3) |
| W1 | run3_14: entry byte 5 | refused (exit 3) |
| W2 | run3_14: record +0x1D | refused (exit 3) |
| W3 | run3_14: counter cleared after the flag | refused (exit 3) |
| W4 | run3_14: counter 0x1C | refused (exit 3) |
| W5 | run3_14: counter before the transition | refused (exit 3) |
| SV1 | scroll: focus 0x7800 | refused (exit 3) |
| SV2 | scroll: origin + 0x15 | refused (exit 3) |
| SV3 | scroll: extra + 0x130000 | refused (exit 3) |
| SV4 | scroll: elevation at the old x | refused (exit 3) |
| SV5 | scroll: SetElevation h + 1 | refused (exit 3) |
| SK1 | shake: index & 1 | refused (exit 3) |
| SK2 | shake: amount 9 bits | refused (exit 3) |
| SK3 | shake: redraw 3 | refused (exit 3) |
| Y1 | run4_14: counter 3 0x19 | refused (exit 3) |
| Y2 | run4_14: effect +1 = 3 | refused (exit 3) |
| Y3 | run4_14: divisor >> 1 | refused (exit 3) |
| Y4 | run4_14: step 0x19 | refused (exit 3) |
| Y5 | run4_14: counter 3 + 2 | refused (exit 3) |
| Y6 | run4_14: counter 3 = 0x21 | refused (exit 3) |
| Y7 | run4_14: step 0x12 request 1 | refused (exit 3) |
| Y8 | run4_14: x 0xFCBD | refused (exit 3) |
| Y9 | run4_14: step 0x10 slot read before the message | refused (exit 3) |
| Z1 | run5_14: counter 3 | refused (exit 3) |
| Z2 | run5_14: distance + 0x59 | refused (exit 3) |
| Z3 | run5_14: step 0xD no shake while counting | refused (exit 3) |
| Z4 | run5_14: stream 3 | refused (exit 3) |
| Z5 | run5_14: bit 3 cleared after the flag | refused (exit 3) |
| Z6 | run5_14: battle 0x33 | refused (exit 3) |
| Z7 | run5_14: step 0xC effect kind 0x8F | refused (exit 3) |
| TB1 | talk: one member more | refused (exit 3) |
| TB2 | talk: none answers 0xFFFE | refused (exit 3) |
| TB3 | talk: three bytes | refused (exit 3) |
| TB4 | talk: request not set | refused (exit 3) |
| Q1 | run6_14: second lines first | refused (exit 3) |
| Q2 | run6_14: counter after the talk | refused (exit 3) |
| Q3 | run6_14: member 5 | refused (exit 3) |
| Q4 | run6_14: slot word not stepped | refused (exit 3) |
| Q5 | run6_14: step 0x10 does not fall through | refused (exit 3) |
| Q6 | run6_14: grey step read before the calls | refused (exit 3) |
| Q7 | run6_14: hold 0x3B | refused (exit 3) |
| Q8 | run6_14: 0x55 z 0x430000 | refused (exit 3) |
| Q9 | run6_14: step 0x40 record 0 | refused (exit 3) |
| Q10 | run6_14: music byte 9 | refused (exit 3) |
| Q11 | run6_14: sprite mode 0xF | refused (exit 3) |
| Q12 | run6_14: frames 9 | refused (exit 3) |
| Q13 | run6_14: +1 = 4 | refused (exit 3) |
| Q14 | run6_14: fade step read before the call | refused (exit 3) |
| N1 | run7_14: step 1 area code 0x420580 | refused (exit 3) |
| N2 | run7_14: fade 5 | refused (exit 3) |
| N3 | run7_14: pass 0x1E | refused (exit 3) |
| N4 | run7_14: view byte 2 | refused (exit 3) |
| N5 | records: +0x2D | refused (exit 3) |
| N6 | records: n < 1 | **equivalent** (below) |
| N6b | records: n <= 2 (near N6) | refused (exit 3) |
| N13b | cell effect: elevation a signed byte (near N13) | refused (exit 3) |
| N7 | records: party list + 0 | refused (exit 3) |
| N8 | records: from record 2 | refused (exit 3) |
| N9 | run7_14: slot 5 | refused (exit 3) |
| N10 | run7_14: counter after the flag | refused (exit 3) |
| N11 | run7_14: kind 0x96 | refused (exit 3) |
| N12 | cell effect: + 0x81 | refused (exit 3) |
| N13 | cell effect: elevation unsigned | **equivalent** (below) |
| N14 | records: slot word n stored before | refused (exit 3) |
| J1 | objects14: area 0xC1 | refused (exit 3) |
| J2 | object02_14: who 3 | refused (exit 3) |
| J3 | object05_14: step 6 | refused (exit 3) |
| J4 | object06_14: step 0x1D | refused (exit 3) |
| J5 | object07_14: +0x8C | refused (exit 3) |
| J6 | objects14: request not set | refused (exit 3) |
| K1 | step14: x 0x738001 | refused (exit 3) |
| K2 | step14: byte 0xA7 | refused (exit 3) |
| K3 | step14: second cell (x + 1, z + 1) | refused (exit 3) |
| K4 | step14: z fraction from x | refused (exit 3) |
| K5 | step14: step 0x15 | refused (exit 3) |
| K6 | step14: FD 3 | refused (exit 3) |
| I1 | arrive14: bound 0x4F8001 | refused (exit 3) |
| I2 | arrive14: z range 7 | refused (exit 3) |
| I3 | arrive14: z bound 0x1D8001 | refused (exit 3) |
| I4 | arrive14: x16 0x14 | refused (exit 3) |
| I5 | arrive14: z16 0x1D | refused (exit 3) |
| I6 | arrive14: 0xAC run 5 | refused (exit 3) |
| I7 | arrive14: flag 8 for 9 | refused (exit 3) |
| I8 | arrive14: story 0x7B | refused (exit 3) |
| I9 | arrive14: fade 9 | refused (exit 3) |
| I10 | arrive14: 0x94 answers 0 | refused (exit 3) |

**N6 is equivalent.** Chapter 14's run 7 step 0x17 stores 1 to the byte
`0x903850`, returns when the member count is at most 1, and otherwise
stores the count there and copies a record per member after the leader.
The mutant (`n < 1`) differs only at a count of exactly 1, where it goes on
to store the count - 1, the same byte - and to a copy loop of no passes.
Every input ends in the same state. Its near variant N6b (`n <= 2`, which
skips the copy for two members) is refused.

**N13 is equivalent.** The field effect's `+0x3C` is (the elevation's low
word + 0x80) << 16; reading that word unsigned instead of signed changes
the sum by 0x10000, which the shift by 16 pushes out of the dword. No
input can tell them apart. Its near variant N13b (the elevation as a signed
byte) is refused.

**How the fuzz got here.** The first full pass (at `37dba3d`) left eight of
238 standing: E13 (`Cond_ByteFD` re-read after `Flags_Set` in area 0x90),
R6j (the object pointer `0x903804` re-read after `AreaMap_Elevation`), H12
(the step hook's facing re-loaded after a flag test in 0x8F and stored in
0x90), U2 and L4 (`Field_StatusBits` read after an area change or
`ScriptFlags_Set40`), Q6 (an exit of -1 once, refused by a count on its
own re-run), and the equivalent N6 and N13. Each was the fuzz's fault: the
group's disturbance reaches its cells about one call in 24. The `settle`
now moves the status bits (any bit - the functions set or clear bit 0
afterwards, so moving bit 0 alone hid U2 and L4), the object pointer, and
for the re-reading functions `Cond_ByteFD` and the facing; the step hook
got a rectangle testing area 0x90's cell in 0x8F and a settle that moves
0x8F / FD 1 and 0x90 / FD 2 together. A second full pass (at `6e0b7a3`)
refused all of those and left H7 (area 0x90's drop-in by who leads, a
four-flag path) and Y9 (the slot byte re-read after `Msg_OpenScript`):
area 0x90's rectangle is now weighted three times with the leader byte
seeded to the values it tests, and the settle moves the chapter's slot
byte a quarter of the time. The final pass, above, is at `cf0ea5a` (the
committed fuzz): 238 of 240 refused, every one by a count (exit 3), none
by a hang, and the two equivalents.

## 7. Cross-group calls

| Callee | What | Owner | How |
|---|---|---|---|
| `0x533E50` | a pass over the eight records at `0x903A70` and the party | SX (this wave) | raw |
| `0x532ED0` | (x, z, kind): the party placed for an event battle | SX | raw |
| `0x56D6F0` | `Field_StatusBits` \|= 0x80 | SX | raw |
| `0x56FCA0` | the view shift | SX | raw |
| `0x57CD90` | the first free `Sprite_Objects` record of 30, al (0xFF none) | SX | raw |
| `0x587B80` | `jmp 0x5A6FF0` (the sound layer), no arguments | SX | raw |
| `0x591900` | the first 0 byte's index among the 32 at `0x904554`, al; reads no argument | SX | raw |
| `0x587890` | (id, level): a sound voice's level | nobody | raw |
| `0x591920` | (byte): its index among the 32 at `0x904554` | nobody | raw |
| `0x420A90` | (shift): the CLUT copy at `0x80BB80` dimmed into `0x80FB80` | area 143 (AR3F, a later wave) | raw |
| `0x4204D0`, `0x420580`, `0x4205D0`, `0x420670`, `0x420710` | an area's event objects set up (free sprite records, `EventOp_6x`) | area 141 (AR3E) | raw |
| `0x42BA90`, `0x42C0A0` | (who): a message id by who, from a five-entry table | areas 191 / 192 (AR4E / AR4F) | raw |
| `0x5341C0` `Scenario_CallB` | call table B's thunk | named by SCH, not taken | by name |
| `Field_StartEventBattle` `0x4410B0` | an event battle's start | SE, ours | by name |
| `Flags_*`, `ScriptFlags_*`, `Msg_OpenScript`, `Field_ChangeArea`, `Scenario_CallA`, `Party_DropIn`, `Effect_FindFree`, `Transition_Start`, `Music_*`, `Sound_*`, `File_LoadDone`, `Task_Sleep`, `Kind2_Place`, `AreaMap_*`, `MapView_SetElevation`, `MoveCmd_TestFB`, `Field_ViewReset`, `Text_DrawAt`, `ClutStrip_*`, `Menu_DrawHand`, `Math_Sin`, `Inventory_Add` | | ours (earlier rounds) | by name |
| `EventOp_6x`, `Effect_SpawnAt`, `Sound_ResumeAll` | | Capcom's, named | by name |

No harness edit: `Field_StartEventBattle` needed no standard-set change
(listed in the group's callees by name); nothing moved from `SH_THEIRS`.

## 8. What nothing reached, latent defects

No route has reached chapters 13 or 14 (fuzz only, as every scenario group
so far). `Scena14_LeaveToC4` is reached by nothing in the image (section 1).

Latent, described and kept (none fixed):

- **Index-driven writes the originals do not bound**, each safe for the
  values the game stores and each a wild write for others: chapter 14's
  run 7 step 0x17 copies 0xA4 bytes into ObjTrio for every member after the
  leader below `Field_MemberCount` (a count above 3 writes past the three
  ObjTrio records); run 2 step 0x11 and run 3 step 9 write the record
  `0x903A70 + [0x669734] * 0xA4` (a byte above 7 is past the eight
  records); run 4 step 0x10 writes `+1` of the effect record `Scena14_Slot`
  names without checking it (0xFF - `Effect_FindFree`'s "none" - lands
  0x7F80 past `Effect_Objects`); chapter 13's run 6 step 0x19 writes a byte
  through the pointer `0x903804`. The fuzz keeps these inside their tables
  (section 5); ours writes the same places.
- **Waits that can stall**: chapter 13's run 7 step 0x16 (the timer never
  counted down in this band); the steps that wait on an effect record's
  in-use bit read the record the slot byte names even when the take failed
  (0xFF), a record past `Effect_Objects`.
- The table aborts of section 2 are the only departure from the originals,
  and only for indices the band never writes.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D155 (steps with no exit) in
[`known-defects.md`](known-defects.md).