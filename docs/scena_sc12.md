# Chapter 12's first block: 0x55E4E0..0x561DB0

**Status:** IN PROGRESS (2026-09-27) - stage A: 24 functions read to the
last instruction and written (`src/game/scena_sc12.cpp`, shadow name
`scena_sc12`), the fuzz written against the scenario harness's contract
(`src/game/scena_sc12_fuzz.cpp`), neither built nor run yet: the module is
not registered in `CMakeLists.txt` / `inject_all.cpp` until SCH's
`scenario_harness` merges. No controls planted yet.

Group SC12 of round ten's first wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3).

## Stage A done - what stage B has to do

Stage A (this commit) holds: ours, `symbols.toml` (24 `[[func]]` with `impl`,
5 `[[data]]`), `scena_sc12_callees.h`, the fuzz file, this doc, the
`entries_logic.txt` lines (main checkout, 24 lines). Both `.cpp` files pass
`-fsyntax-only` (llvm-mingw, `-m32 -std=c++20 -Wall -Wextra`) against a
private copy of `magic_harness.h` renamed to `scenario_harness` / `SH_`, with
one addition the clone tool prints: `enum class Shape { kState, kSlot,
kObject, kHook, kCall }` and `Clone::shape` after `calm`.

Stage B, after SCH's harness merges:

1. Merge `phase-3/capture-round-ten` at the harness's SHA; add
   `src/game/scena_sc12.cpp` and `scena_sc12_fuzz.cpp` at the end of the
   `CMakeLists.txt` list and `ScenaSc12_Inject();` (with its include) at the
   end of `inject_all.cpp`.
2. Build against the real `scenario_harness.h`. Expect to adjust: the name
   and enumerators of the call shape (the fuzz uses
   `sh::Shape::kSlot / kObject / kHook`, as `tools/scenario_rows.py`
   prints), how a `kObject` / `kHook` clone gets its arguments (the fuzz
   gives them through `Group::args`: the object pointer, (x, z) at the
   hooks' bounds), and any group region the harness already holds as a
   standard one (the fuzz lists every cell it touches, section 4; drop the
   overlaps if the harness refuses them). Drop any callee the standard set
   records the same way.
3. Run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc12` to 0 mismatches, then
   `'*'`. Read the coverage line against section 4's expectation: every
   run's steps, the four data tables' entries, both `kBool` callees'
   loops.
4. Plant and refuse the controls of section 5 (a script that plants,
   rebuilds, runs, restores and rebuilds; anchors on unique strings).
5. Fill sections 4 and 5 with counts ("in this worktree"), replace this
   section and the status header.

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

## 4. The fuzz (stage B)

Written, not run. `scena_sc12_fuzz.cpp`: the clone table (the tool's, with
each clone's shape in its comment and `Clone::shape` on the roots and the
cell entries; `ret_mask 0xFF` on the three hooks and the two cell
entries), 29 callees listed by the group (the flags, `ScriptFlags_*`, the
messages, the area change, `Scenario_CallA`, `Effect_FindFree` as a byte
0xFF or 0..0x13, `File_LoadDone` / `Sound_StreamDone` as bools, the six raw
addresses, `0x56D800` as a byte 0xFF or 0..3), the four chapter tables as
`DataTable`s, 25 regions (every cell of the callees header, the
0x2000-colour CLUT, the 20 effect records, ObjTrio record 0, a sprite and an
object of the fuzz's own). The seed puts each compare's values in: the
areas the code tests, the counters' tested values, the request byte 0 / 2
/ 6, the wait word 0, the selector 7..18, `Cond_ByteFD` 2 / 3, the byte
`0x802DC9` 2..8, each run's case steps and one past, the state 0..2, the
run 0..9, the object's +0x86 0..14, the hooks' (x, z) at each bound and one
either side. The disturbance moves the counters, the request byte, the wait
word, the area, the selector, the step, an effect's in-use byte and the
kind-2 hold.

Counts: to come.

## 5. Controls (stage B)

To plant, one per function at least, each refused by a count: the frame's
table base, the run's signedness; EnterArea's area 0x83 elevation, the
0x85 selector case, the 0xBC case 6 fallthrough, the end-of-run area list;
per run a counter value, a step value, a flag number, an area-change flag
byte, and one selector entry; run 4's grey divisor and bit-15 keep; run 5's
step 2 unconditional step 3; run 6's music byte after a hop; run 8's pass
clear before the `0x929F0F` test; run 9's message order (2, 8, 5, 4); the
step hook's inclusive bounds (one each side) and the 16-bit z test; the
arrive hook's signed compare; the cell hook's negative test; the cell
entries' answers; each object handler's run.

## 6. Cross-group calls

| Callee | What | Owner | How |
|---|---|---|---|
| `0x5341C0` | call table B's thunk (the shape of `Scenario_CallA`) | nobody yet | raw address |
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
