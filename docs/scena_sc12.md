# Chapter 12's first block: 0x55E4E0..0x561DB0

**Status:** IN PROGRESS (2026-09-27) - 24 functions ours
(`src/game/scena_sc12.cpp`, shadow name `scena_sc12`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 192,000 rounds (8,000 a function); 65 of 66 negative controls refused, the other an equivalent mutant whose near variant is refused. Fuzz only:
no recorded route reaches chapter 12 (section 7).

Group SC12 of round ten's first wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3), in two
stages: stage A read and wrote everything against the harness's contract
before the harness existed; stage B merged it (`218eeec`), registered the
module, built, fuzzed and planted the controls.

## 1. The block, and chapter 12's roots

`tools/scenario_roots.py --chapter 12`: chapter 12's closure is 91 functions
from `0x4410B0` to `0x567A90`; this band is the first 24 of them. Its roots:

| Table | Entry | Target | Band |
|---|---|---|---|
| vtable `0x6616E0` (0x662C80 entry 12) | slot 0, the frame | `0x55E4E0` Scena12_Frame | SC12 |
| | slot 1, the object trigger | `0x561880` Scena12_ObjectTrigger | SC12 |
| | slot 2, the step hook | `0x561A30` Scena12_StepHook | SC12 |
| | slot 3, the arrive hook | `0x5619E0` Scena12_ArriveHook | SC12 |
| | slot 4, the cell hook | `0x561CE0` Scena12_CellHook | SC12 |
| call table A `0x65F814` | 10 entries | `0x51A030`..`0x51A570` | none in the bank: engine code in `Boot: field, map and sprites` |
| call table B `0x65F83C` | 5 entries | `0x5199A0`..`0x51A2F0` | the same |

All five slots are in this band; no call-table entry is in any scenario band.
From `0x561DB0` on the closure is group SC13's block (a later wave): the
state table's entry 0 (`0x5646B0`, state 0 - presumably the chapter's start,
as chapter 1's and 16's state 0 are; unread here) and everything the SC13
functions reach. Of the helpers outside the bank, `0x4410B0` is group SE's
(this wave; called here by raw address) and `0x508000` / `0x5080A0` are
reached only from SC13's block.

**The starts.** The three start lists (`pc_funcs`, `pc_hidden`, the walk's
`added_starts`) hold 19 starts in the band; the five vtable slots are in
none of them (`pc_hidden`'s `0x55D140` is a 0x55B9-byte run-on from
`0x55CC50` that covered the whole band), which is why the queue counted 19.
`tools/scenario_rows.py --unit SC12` counts 24 with the roots, 0x3824 bytes,
0 ours, 0 not walked; this reading agrees function for function, extent for
extent. Every start in the band is reached by the walk; none was found
inside another's extent and none is missing.

## 2. The tables

Five tables of the chapter's own, back to back after the vtable
(`symbols.toml` `[[data]]`):

| Address | Name | Entries | Read by | Holds |
|---|---|--:|---|---|
| `0x6616F4` | `Scena12_States` | 3 | Scena12_Frame, `jmp [+ s8 0x8034E2 * 4]` | `0x5646B0` (SC13's), EnterArea, Run |
| `0x661700` | `Scena12_Runs` | 10 | Scena12_Run, `jmp [+ s8 MoveScript_Var7 * 4]` | ret, Run1, Run2, ret, Run4..Run9 |
| `0x661728` | `Scena12_Objects` | 15 | Scena12_ObjectTrigger, `call [+ object[0x86] * 4]` (object, bits) | ret for 0..5, 7, 8; Object06, 09..14 |
| `0x661764` | `Scena12_CellRecords` | 4 x 5 bytes | `0x56D800` for Scena12_CellHook | area, cell, run, facing (as `0x56D800` reads them) |
| `0x661778` | `Scena12_CellHooks` | 4 | Scena12_CellHook, `jmp [+ index * 4]` with (a, b) in place | CellTalk x3, CellDoor |

Chapter 13's vtable follows at `0x661788`. The originals index every table
unchecked (a signed byte for the state and the run); an index past a table
reads the next one - the state table's entry 3 is the run table's entry 0,
exactly as chapter 16's two tables overlap
([`field-modes.md`](field-modes.md) §1). **Ours aborts with a message** for
an index outside the table's own entries (negative included), the project's
rule for an index past a table (round nine §6, the owner: no DIVERGENCE
entry). No code in the block writes such an index; `MoveScript_Var7` is
also set by the movement script's op F6, so a script that stored a run of
10 or more would stop here where the original ran an object handler.

## 3. The functions

Call shapes: **slot** - a vtable slot, no arguments; **object** - slot 1,
the object; **hook** - a hook, (x, z) or (a, b), answering in al; **state**
- a handler reached through a chapter table, no arguments read.

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x55E4E0` | Scena12_Frame | 0xE | slot 0 | the state's handler through `Scena12_States` (tail jump) |
| `0x55E4F0` | Scena12_EnterArea | 0x58C | state 1 | by the area and counter 2: pass flags, script flags, the elevation, effects 0x76..0x7C, music loaded and waited for, call-table entries by the selector; ends the run in areas 0x10 / 0x2D / 0x41 / 0x57 / 0x73; state 2 on every exit |
| `0x55EA80` | Scena12_Run | 0xE | state 2 | the run's handler through `Scena12_Runs` (tail jump) |
| `0x55EA90` | Scena12_Run1 | 0x4B | state (run 1) | steps 0xA, 0xB: a wait on counter 1 |
| `0x55EAE0` | Scena12_Run2 | 0x4C | state (run 2) | steps 0x32 / 0x3C a message, 0x33 its end |
| `0x55EB30` | Scena12_Run4 | 0xA90 | state (run 4) | 17 cases over steps 0..0x27 (below) |
| `0x55F5C0` | Scena12_Run5 | 0x7A8 | state (run 5) | 22 cases over 0..0x2D |
| `0x55FD70` | Scena12_Run6 | 0x688 | state (run 6) | steps 0..0x16: a chain of area changes into 0xBC |
| `0x560400` | Scena12_Run7 | 0x4D4 | state (run 7) | 17 cases over 0..0x1E |
| `0x5608E0` | Scena12_Run8 | 0x884 | state (run 8) | 12 cases over 0..0x20 |
| `0x561170` | Scena12_Run9 | 0x708 | state (run 9) | 25 cases over 0..0x2F |
| `0x561880` | Scena12_ObjectTrigger | 0x1F | slot 1, object | `Scena12_Objects[object +0x86]` (object, bits) |
| `0x5618A0` | Scena12_Object06 | 0x23 | state (objects 6) | run 1, step 0 |
| `0x5618D0` | Scena12_Object09 | 0x25 | state (objects 9) | run 4, step 0x14 |
| `0x561900` | Scena12_Object10 | 0x25 | state (objects 10) | run 5, step 0x14 |
| `0x561930` | Scena12_Object11 | 0x2A | state (objects 11) | run 5, step 2, all counters 0 |
| `0x561960` | Scena12_Object12 | 0x23 | state (objects 12) | run 8, step 0 |
| `0x561990` | Scena12_Object13 | 0x25 | state (objects 13) | run 8, step 0x1E |
| `0x5619C0` | Scena12_Object14 | 0x14 | state (objects 14) | run 1, step 0xA |
| `0x5619E0` | Scena12_ArriveHook | 0x41 | slot 3, hook (x, z) -> al | area 0x79, flag 0, x at most 0x16.0: run 2, step 0x3C |
| `0x561A30` | Scena12_StepHook | 0x2B0 | slot 2, hook (x, z) -> al | areas 0x86 / 0x85 / 0x88 / 0xBC by flags and rectangles: runs 9 / 4 / 5 / 7 |
| `0x561CE0` | Scena12_CellHook | 0x2A | slot 4, hook (a, b) -> al | `0x56D800` over the cell records; negative 0xFF, else `Scena12_CellHooks` |
| `0x561D10` | Scena12_CellTalk | 0x3B | state (cell hooks 0..2) -> al | run 5, step 0 or 5 by the byte `0x802DC9`; 1 |
| `0x561D50` | Scena12_CellDoor | 0x5D | state (cell hook 3) -> al | flag 0x2A set: 0xFF; else run 7, `MoveCmd_TestFB` at the sprite, sound 0x103; 1 |

Every exit of every function is in `symbols.toml`'s evidence line with its
extent and jump tables. What the runs are in the story is not read here: the
areas and flags are numbers, and a scene's meaning is the owner's to say.

**The state machine.** Each run is a switch on the step byte `0x8034E5`
(MSVC's two-level switch where the cases are sparse: a byte table of the
step, then a jump table of the cases). Each case waits on something -
counter 0 (`0x903848`, the movement script's counters) at a value, the
request byte `Field_Request` not 2 (a message closed) or 0, the wait word
`0x66C810` at 0, an effect record's in-use byte - then does one thing
(an area change, a message, a party drop-in, a flag, an effect) and sets
the next step. A run ends with `ScriptFlags_Clear40` and the step and run 0.

**Recurring shapes, written once:**

- *The selector* `Cond_Flags + 0x19C` & 0x7F picks one of ten flags: 7..10
  the first four, 13..18 the other six, 11 and 12 none (a twelve-entry
  table whose entries 4 and 5 are empty). Run 4 (flags 0x1A..0x23) and runs
  7 / 8 (0x32..0x3B) set the picked one; run 4's step 0x1E and run 8's step
  0x14 also clear the other nine, in ascending order. Checked entry by entry
  with a symbolic trace of the twelve cases of each of the four tables.
  The same selector picks `Scenario_CallA` / `0x5341C0` entries in
  EnterArea (area 0x85) and run 5 (step 0x2C), and run 5's step 0x28 / 0x29
  area changes.
- *The effect record* `Effect_Objects` + slot * 0x80 from `Effect_FindFree`
  (the slot stored to `0x903850` first in most sites): +0 = 1, +5 the kind,
  sometimes +1; kind 0x13 (the camera turn) also +0x64 / +0x68 / +0x6C and
  +9 from `Camera_Angles`.
- *The music wait*: `Music_LoadFile(t)`, `Task_Sleep(1)` until
  `File_LoadDone`, `Music_Play(t, 8)`.
- *Run 6's hops*: counters, the step, the music byte `0x904CD0` = 0xFF,
  `Field_ChangeArea` into 0xBC, the music byte 0x90 (so the next
  `Music_Play` of the same track restarts it).

**As the originals have them** (each kept, and each said at its site):
run 5's step 2 moves to step 3 even when the request byte is 2 (the message
is then skipped); run 5's step 0x16 hands `Flags_Set` the literal
`0x904030` (the story flags) where every other flag call reads the pointer
at `0x929ED0`; run 5's step 0x29 reads the selector as a dword (the same
low byte); run 8's step 0x14 clears the pass flags before testing
`0x929F0F`; run 9's step 0x12 stores its own step again; the step hook's
area 0x88 test takes z's high word minus 7 as a 16-bit unsigned (7..11);
EnterArea reads the area once for each pair of its first six tests and
afresh for every later one.

## 4. The fuzz

`scena_sc12_fuzz.cpp`, through `scenario_harness` with `chapter = 12`:

- **The clones**: `tools/scenario_rows.py --unit SC12 --clones` at
  `218eeec` (run against that commit's `symbols.toml`, since the tool skips
  functions already ours), identical to stage A's reading extent for extent
  and call for call. Shapes: slot 0 `kSlot`; the states, runs and object
  handlers `kState`; slot 1 `kObject`; slots 2..4 and the two cell entries
  `kHook` (compared on al).
- **Two copies the group makes itself.** `Scena12_Run4` has 117 call sites
  and `Scena12_Run8` 102; the harness re-aims at most 64 a clone. The fuzz
  file copies both with `bof3::CloneOriginal`, every site re-aimed at a
  trampoline that calls the harness's recorder for that callee
  (`scenario_harness::StandIn`, on either pass - the same log entry,
  disturbance and answer a harness-aimed site gets), the jump tables
  relocated into the copy (`move_script::Relocate`); the harness is handed,
  as each one's original, a six-byte `jmp [copy]` of the fuzz's own, which
  it clones like a function without calls. Theirs is still Capcom's bytes.
  A later harness with a larger call limit can take both back.
- **Callees**: the harness's 70 standard ones, plus three the group lists:
  `Flags_Test` as a `kFlag` (the block tests al alone, so garbage above a 0
  must change nothing; the standard `kBool` would not show it), `0x533E50`,
  and `0x56D800` as a byte 0xFF or 0..3; and the log slots of the table
  stand-ins below. `Scenario_CallB` (named by SCH, not taken) is called by
  name; `0x4410B0` stays a raw address until SE merges.
- **Tables**: `Scena12_States` and `Scena12_Runs` swapped for recorders
  (their entries take no arguments). `Scena12_Objects` and
  `Scena12_CellHooks` pass arguments (the object and the flag row; the cell
  (a, b)) that a table recorder does not log, so the seed writes a typed
  stand-in into every entry - one per index, so a wrong index is a
  different log - and the two tables are regions the harness puts back.
- **Regions** beyond the harness's 22: the selector `0x90412C`,
  `Cond_ByteFE`, the music byte `0x904CD0`, the 0x2000 colours at
  `0x80F580`, the tile word `0x939A00`, `0x929F0C..` (the byte `0x929F0F`
  and `Field_Kind2Hold`), and the two tables. 30 regions, 26,472 bytes.
- **Seed**: the areas the block tests (EnterArea's paired with counter 2's
  cases), the counters' tested values, the request byte 0 / 2 / 6, the wait
  word 0, the selector 7..18 (bit 7 half the time), `Cond_ByteFD` 2 / 3,
  the byte `0x802DC9` 2..8, the input words, the party bytes, ObjTrio's two
  words at their bounds, an effect's in-use byte, each run's case steps and
  one past, each with counter 0 at the value it waits on two times in three
  (`kWaits`), the state 0..2, the run 0..9, the object's +0x86 0..14; the
  hooks' (x, z) at each bound and one either side, and half the time both
  at the bounds of one of the step hook's seven rectangles.
- **Disturbance** beyond the harness's: counters 1 and 2, the area (0x82..
  0x88, 0xBC), the selector, an effect's in-use byte, the kind-2 hold,
  `Cond_ByteFD`, the byte `0x802DC9`, a bit of the script flags' low byte
  (bits 3, 5 and 7 most often). A `settle` for EnterArea alone moves the area
  to 0x82..0x88 half the time after every disturbance, drawing on `Noise()`.

**Result, in this worktree** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc12`,
exit 0): 192,000 rounds, 184,091 calls to the stand-ins, **0 mismatches**.
Coverage, the calls the originals made: `Flags_Test` 21,919,
`ScriptFlags_Set40` 68,204, `Msg_OpenScript` 7,263, `Field_ChangeArea`
2,191, `Party_DropIn` 2,194, `Flags_Set` 1,765, `Flags_Clear` 622,
`Scenario_CallA` / `CallB` in the hundreds, `Effect_FindFree` ~1,000,
`Kind2_Place` ~3,600, `AreaMap_SetByte` ~150, `MoveCmd_TestFB` ~2,700,
`Sound_LoadStream` / `StreamDone` ~600 / ~300, `Music_LoadFile` ~110 with
`File_LoadDone` ~170 and `Task_Sleep` ~55 (the wait loop runs), `0x4410B0`
28, `0x532ED0` 22, `0x56D6F0` 71, `0x533E50` ~200, `0x56D800` 8,000, the
object and cell stand-ins 8,000 / ~6,400, and every state and run table
entry (`0x5646B0`, EnterArea, Run, the bare ret, Run1..Run9) about 800 each.
`BOF3X_SHADOW='*'`: exit 0.

## 5. Controls

`scratchpad/sc12/controls.py` (not committed; the pattern of
`magic_s31`'s): each mutant planted in `scena_sc12.cpp` on a unique anchor,
rebuilt, the self-test run, restored, and rebuilt at the end. Refused =
exit 3 on a mismatch.

| Id | Mutant | Result |
|---|---|---|
| F1 | frame: state 1 read as 2 | refused (exit 3) |
| F2 | run: run 4 read as 5 | refused (exit 3) |
| F3 | object trigger: the row not passed | refused (exit 3) |
| F4 | object trigger: index + 1 below 14 | refused (exit 3) |
| F5 | cell hook: count 3 | refused (exit 3) |
| F6 | cell hook: negative answers 0 | refused (exit 3) |
| F7 | cell hook: (b, a) | refused (exit 3) |
| H1 | arrive: x strictly below | refused (exit 3) |
| H2 | arrive: run 1 | refused (exit 3) |
| H3 | step: area 0x86 x lower bound +1 | refused (exit 3) |
| H4 | step: SetByte cell 0x2E, 0x2A value 0x71 | refused (exit 3) |
| H5 | step: ObjTrio +0x3C strict | refused (exit 3) |
| H6 | step: area 0x85 z upper bound | refused (exit 3) |
| H7 | step: area 0x88 z high word 7..12 | refused (exit 3) |
| H8 | step: area 0x88 x at least | refused (exit 3) |
| H9 | step: area 0xBC third rectangle step 5 | refused (exit 3) |
| H10 | step: 0xBC second rectangle z top | refused (exit 3) |
| E1 | cell talk: leader 4 | refused (exit 3) |
| E2 | cell talk: leader read before the call | refused (exit 3) |
| E3 | cell door: answer 1 when flagged | refused (exit 3) |
| E4 | cell door: x and z swapped | refused (exit 3) |
| E5 | cell door: counter 2 cleared too | refused (exit 3) |
| O1 | object 06: run 2 | refused (exit 3) |
| O2 | object 11: step 3 | refused (exit 3) |
| O3 | object 13: run 7 | refused (exit 3) |
| O4 | object 14: run before set40 | refused (exit 3) |
| A1 | enter: area 0x83 elevation 0x499 | refused (exit 3) |
| A2 | enter: 0x85 selector 16 CallB 3 | refused (exit 3) |
| A3 | enter: 0xBC case 6 no pass flags | refused (exit 3) |
| A4 | enter: area 0x57 dropped from the end list | refused (exit 3) |
| A5 | enter: area 0x86 z bound <= | refused (exit 3) |
| A6 | enter: no state 2 | refused (exit 3) |
| A7 | enter: area read once for 0x82/0x83 | **equivalent** (below) |
| A7b | enter: area read once for 0x83 / 0x84 | refused (exit 3) |
| A7c | enter: 0x79 path keeps its counters and the area unread (near A7) | refused (exit 3) |
| A8 | enter: music wait skipped | refused (exit 3) |
| R1 | run1: counter 0 = 0x33 | refused (exit 3) |
| R2 | run2: message 8 at 0x3C | refused (exit 3) |
| R3 | run4: grey divisor 4 | refused (exit 3) |
| R4 | run4: bit 15 dropped | refused (exit 3) |
| R5 | run4: step 0x1E sets without clearing | refused (exit 3) |
| R6 | run4: selector 11 picks | refused (exit 3) |
| R7 | run4: step 0x15 area 0x66 | refused (exit 3) |
| R8 | run5: step 2 only on request | refused (exit 3) |
| R9 | run5: story flag through the row | refused (exit 3) |
| R10 | run5: tile 0x67 in | refused (exit 3) |
| R11 | run5: step 0x2C selector 15 without CallB 4 | refused (exit 3) |
| R12 | run5: step 0x28 selector 17 flags 0x86 | refused (exit 3) |
| R13 | run6: hop music byte 0x8F after | refused (exit 3) |
| R14 | run6: effect +0x6C 0x201 | refused (exit 3) |
| R15 | run6: step 0xC script flags before the sound | refused (exit 3) |
| R16 | run6: step 4 effect byte of counter 2 | refused (exit 3) |
| R17 | run7: SetByte 0x1D, 3 value 0xA1 | refused (exit 3) |
| R18 | run7: step 0x14 counter 2 = 8 | refused (exit 3) |
| R19 | run7: step 0x14 base 0x33 | refused (exit 3) |
| R20 | run8: pass flags kept when not loaded | refused (exit 3) |
| R21 | run8: greet fd 3 message 0x19 for even | refused (exit 3) |
| R22 | run8: stream done not waited | refused (exit 3) |
| R23 | run8: step 1 counter 5 keeps counter 0 | refused (exit 3) |
| R24 | run9: message order 5 / 4 swapped | refused (exit 3) |
| R25 | run9: input 0x6000 not tested | refused (exit 3) |
| R26 | run9: placement kind 0x27 | refused (exit 3) |
| R27 | run9: member word + 0x38000 | refused (exit 3) |
| R28 | run9: step 0x2C x from angle Y | refused (exit 3) |
| R29 | run9: step 0x2F bit 80 after the change | refused (exit 3) |
| R30 | run9: step 0 member state 2 | refused (exit 3) |

**A7 is equivalent.** EnterArea's 0x79 path (counter 2 = 2) calls
`ScriptFlags_Clear40` and then zeroes the four counters before it reads the
area again. With counter 2 now 0, the 0x82 and 0x83 branches it could take
on a moved area only return; a mutant that keeps the stale 0x79 falls
through to tests that each read the area afresh, and with no call in
between none of them fires. Every input ends in the same state. The near
variant A7c, which also leaves the counters alone so that 0x83's counter-2
cases can run, is refused.

Earlier passes left mutants standing because the fuzz could not reach them,
and each was fixed in the fuzz before the final pass:
- H10, the top z of 0xBC's second rectangle: x and z were drawn
  independently, so they rarely landed at one rectangle's bounds together.
  The seed now puts both inside one rectangle half the time, and H8 (area
  0x88's x bound) came with it.
- E2, CellTalk's leader byte read before the call: nothing moved `0x802DC9`
  after a call. The group's disturbance now does.
- R12 and the per-run steps' actions: a step's action is reached only when
  counter 0 holds the value that step waits on. The seed now pairs each
  step with that value (the table `kWaits`).
- R15, run 6's script-flag store before the sound: the harness's flag
  disturbance rarely hit bit 3 at that step. The group's now flips bits 3,
  5 and 7, the ones this block sets.
- A7b, EnterArea's area read again after `MapView_SetElevation`: the
  group's disturbance runs only one time in sixteen of the harness's. A
  `settle` now moves the area to one of 0x82..0x88 half the time while
  EnterArea is fuzzed.

## 6. Cross-group calls

| Callee | What | Owner | How |
|---|---|---|---|
| `0x5341C0` `Scenario_CallB` | call table B's thunk (the shape of `Scenario_CallA`) | named by SCH, not taken | by name |
| `0x533E50` | a pass over the eight records at `0x903A70` and the party | nobody | raw |
| `0x532ED0` | (x, z, kind): a party placement before a battle start | nobody | raw |
| `0x56D6F0` | bit 7 of `0x8034E1` set | nobody | raw |
| `0x56D800` | the cell-record search (area, cell, facing) | nobody | raw |
| `0x4410B0` | (kind): a battle start's bytes | group SE, this wave | raw |
| `0x5646B0` | state 0 | group SC13, a later wave | through `Scena12_States`, read in place |
| `Flags_*`, `ScriptFlags_*`, `Msg_OpenScript`, `Field_ChangeArea`, `Scenario_CallA`, `Effect_FindFree`, `MapView_SetElevation`, `Music_*`, `File_LoadDone`, `Task_Sleep`, `Kind2_Place`, `Party_DropIn`, `Transition_Start`, `Sound_*`, `AreaMap_SetByte`, `MoveCmd_TestFB` | | ours (earlier rounds) | by name |

## 7. What nothing reached, and latent defects

No route has reached chapter 12 (fuzz only, as every scenario group so far).
No latent defect found: every index the block writes stays inside its table,
every loop is bounded, and the one wait (`File_LoadDone`) is the engine's
usual load wait. The table aborts of section 2 are the only departure, and
only for indices the block never writes.
