# Chapter 9's first block: 0x553B30..0x557170

**Status:** IN PROGRESS (2026-09-27) - 22 functions ours
(`src/game/scena_sc9a.cpp`, shadow name `scena_sc9a`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 132,000 rounds (6,000 a function); 86 of 86 negative controls refused, at least one per function. Fuzz only:
no recorded route reaches chapter 9 (section 7).

Group SC9a of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §6,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3), one stage:
read, written, fuzzed and controlled against the harness as merged in
`3e410e7`.

## 1. The block, and chapter 9's roots

`tools/scenario_roots.py --chapter 9`: chapter 9's closure is 107 functions,
from `0x520000` to `0x5646B0`; this band is its first 22 (its own; the
closure also reaches `0x550F80`, in SC7's band, from `0x55BBB1`, and the
immediate `0x520000` pushed at `0x55BE2B` - both in SC9b's block, not read
here). Its roots, and which block each lies in:

| Table | Entry | Target | Block |
|---|---|---|---|
| vtable `0x6613E8` (0x662C80 entry 9) | slot 0, the frame | `0x553B30` Scena09_Frame | **SC9a** |
| | slot 1, the object trigger | `0x5570D0` Scena09_ObjectTrigger | **SC9a** |
| | slot 2, the step hook | `0x557270` | SC9b |
| | slot 3, the arrive hook | `0x539AC0` Scenario_NoHook | ours (SC1's) |
| | slot 4, the cell hook | `0x557A20` | SC9b |
| `Scena09_States` `0x6613FC` | 0 | `0x5646B0` (chapter 12's state 0 too) | SC13 |
| | 1, 2 | EnterArea, Run | **SC9a** |
| `Scena09_Runs` `0x661408` | 1..11, 14..16 | the runs | **SC9a** |
| `Scena09_Objects` `0x661450` | 1..4 | Object01..04 | **SC9a** |
| | 5..11, 13..15 | `0x55BAC0`, `0x55BAE0`, `0x557170`..`0x5571F0`, `0x557210`..`0x557250` | SC9b |
| call table A `0x65F754` | 14 entries | `0x519FD0`..`0x51A260` | CALLS (this wave) |
| call table B `0x65F78C` | 10 entries | `0x5199A0`, `0x51A290`, `0x519D90`, `0x51A2B0`..`0x51A300`, `0x519BC0`, `0x519BD0`, `0x51A360` | CALLS |

Nothing in this band calls into SC9b's block directly: the only ways in are
the tables, read in place. The sibling's `scenario_records.toml` pairs the
SCENA09 vtable's slots 0 and 1 with `0x801F78D0` and `0x801FC5E4` by table
position (a hypothesis, not read); `pairs_propagated.json`'s two pairs in the
band (`0x553E50`, `0x554850`) are jump-table cases, not functions.

**The starts.** `tools/scenario_rows.py --unit SC9a` lists 22, 0x35B8
bytes, 0 ours, 0 not walked; this reading agrees function for function,
extent for extent (every gap between one function's end and the next start
is `0x90` padding). The three start lists hold 40 addresses in the band:

- **19 dropped**, all `pc_hidden` starts that are switch cases of the
  functions below: `0x553E50` (EnterArea's second table), `0x5545C0`,
  `0x554760` (Run3), `0x554810`, `0x554840`, `0x554850`, `0x554B90` (Run4),
  `0x554D60`, `0x555170`, `0x555220` (Run6), `0x555CD0`, `0x555D00` (Run9),
  `0x556190`, `0x556560`, `0x556630`, `0x556940`, `0x5569A0` (Run15),
  `0x556C60`, `0x556E90` (Run11).
- **1 added**: `0x5570D0`, the object trigger (slot 1), in no list; the tool
  adds it from the vtable.

Every function is reached only through a table (an E8 / E9 scan of `.text`
finds no direct call into the band, and each start's address occurs once in
the image, in its table).

## 2. The tables

Three tables of the chapter's own, back to back after the vtable
(`symbols.toml` `[[data]]`), and one cell:

| Address | Name | Entries | Read by | Holds |
|---|---|--:|---|---|
| `0x6613FC` | `Scena09_States` | 3 | Scena09_Frame, `jmp [+ s8 0x8034E2 * 4]` | `0x5646B0` (SC13's), EnterArea, Run |
| `0x661408` | `Scena09_Runs` | 17 | Scena09_Run, `jmp [+ s8 MoveScript_Var7 * 4]` | ret (0, 12, 13), Run1..Run11, Run14..Run16 |
| `0x661450` | `Scena09_Objects` | 16 | Scena09_ObjectTrigger, `call [+ object[0x86] * 4]` (object, bits) | ret (0, 12), Object01..04, SC9b's ten |
| `0x6BC730` | `Scena09_Slot11` | 1 byte | Scena09_Run11 | its effect slot (every other run uses `0x903850`) |

Between the run and object tables sits a dword `0xFF000100` at `0x66144C`,
read by nothing in this band; bytes follow the object table at `0x661490`
(not read here). The runs are not in address order: Run14..Run16
(`0x555E40`, `0x555F40`, `0x556BC0`) lie before Run10 and Run11 (`0x556C10`,
`0x556C40`).

The originals index every table unchecked (a signed byte for the state and
the run); an index past a table reads the next one - state 3 is the run
table's entry 0 (a ret), run 17 reads `0x66144C` (not code), object 16 reads
`0x661490` (bytes). **Ours aborts with a message** for an index outside the
table's own entries (negative included), the project's rule for an index
past a table (round nine §6, the owner: no DIVERGENCE entry). No code in the
block writes such an index: the runs it starts are 1, 3, 4, 5, 7, 8, 10 and
15, the objects are the object's own byte. `MoveScript_Var7` is also set by
the movement script's op F6, so a script that stored a run of 17 or more
would stop here where the original jumped through data.

## 3. The functions

Call shapes: **slot** - a vtable slot, no arguments; **object** - slot 1,
the object; **state** - a handler reached through a chapter table, no
arguments read.

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x553B30` | Scena09_Frame | 0xE | slot 0 | the state's handler through `Scena09_States` (tail jump) |
| `0x553B40` | Scena09_EnterArea | 0x728 | state 1 | by the area (below); every exit stores state 2 |
| `0x554270` | Scena09_Run | 0xE | state 2 | the run's handler through `Scena09_Runs` (tail jump) |
| `0x554280` | Scena09_Run1 | 0x7D | state (run 1) | steps 0..2: flag 0, an area change into 0x45, flag 1 on counter 0x11 |
| `0x554300` | Scena09_Run2 | 0x1C8 | state (run 2) | 8 cases over 0..0xB: a drop-in behind `Field_ScriptFlags2 & 7`, the hold, a stream and music 0x10 |
| `0x5544D0` | Scena09_Run3 | 0x320 | state (run 3) | 13 cases over 0..0xD: two camera effects, a camera sweep, message 0x14, the stream, `Scenario_CallA(5)` |
| `0x5547F0` | Scena09_Run4 | 0x477 | state (run 4) | 18 cases over 0..0x22: flags 0x26 / 0x27 by ObjTrio +0x3C, a camera sweep, area changes into 0x31 / 0x75 |
| `0x554C70` | Scena09_Run5 | 0xC4 | state (run 5) | steps 0, 1, 5, 6: a drop-in and the waits |
| `0x554D40` | Scena09_Run6 | 0x88A | state (run 6) | 48 cases over 0..0x69: messages, hops between areas 0x75 and 0x76 behind camera effects, `Cond_ByteFE` 1..4 |
| `0x5555D0` | Scena09_Run7 | 0xF8 | state (run 7) | steps 0, 1, 5, 6: the menu button not pressed, an area change into 0x76 |
| `0x5556D0` | Scena09_Run8 | 0x4A1 | state (run 8) | 23 cases over 0..0x28: messages, an event battle (0x35) with its placement, per-member effects and messages |
| `0x555B80` | Scena09_Run9 | 0x2BC | state (run 9) | 14 cases over 0..0x1F: an event battle (0x36), an area change into 0x77 with the music byte |
| `0x556C10` | Scena09_Run10 | 0x2B | state (run 10) | step 0: `Party_DropIn(1)` and the run's end without `ScriptFlags_Clear40` |
| `0x556C40` | Scena09_Run11 | 0x490 | state (run 11) | 17 cases over 0..0x23: timed camera effects on its own slot cell, a kind 0x2B effect on the ground, the view reset |
| `0x555E40` | Scena09_Run14 | 0xFC | state (run 14) | steps 0..4: areas 0x35 then 0x41 around a stream |
| `0x555F40` | Scena09_Run15 | 0xC77 | state (run 15) | 66 cases over 0..0x96, in scenes of ten steps (below) |
| `0x556BC0` | Scena09_Run16 | 0x4E | state (run 16) | steps 0, 1: a drop-in, flag 0x24 |
| `0x5570D0` | Scena09_ObjectTrigger | 0x1F | slot 1, object | `Scena09_Objects[object +0x86]` (object, bits) |
| `0x5570F0` | Scena09_Object01 | 0x19 | state (objects 1) | run 5, step 0, counter 0 0 |
| `0x557110` | Scena09_Object02 | 0x19 | state (objects 2) | run 5, step 5, counter 0 0 |
| `0x557130` | Scena09_Object03 | 0x14 | state (objects 3) | run 4, step 5 |
| `0x557150` | Scena09_Object04 | 0x14 | state (objects 4) | run 10, step 0 |

Every exit of every function is in `symbols.toml`'s evidence line with its
extent and jump tables. What the runs are in the story is not read here: the
areas and flags are numbers, and a scene's meaning is the owner's to say.

**EnterArea** tests the area afresh before each block (every test reads
`Game_AreaNumber` again, after the calls of the block before): 0x25 (flags
0x2E / 0x2F: run 0xF step 0xA, `Scenario_CallB` / `CallA` entries by the
selector `Cond_Flags +0x19C & 0x7F`), 0x27 (the kind-2 sprite; run 0xF step
0x46), 0x29 (run 0xF steps 0x3C / 0x6E), 0x35 (the kind-2 sprite), 0x37
(`Camera_Distance + 0xA80`, run 3, entries), 0x45 (run 1, entries), 0x31
(the kind-2 sprite and entries, run 4, `Camera_Distance + 0xB80`, flag 0x11
cleared), 0x64, 0x76 and 0x77 (each ends the function early unless counter
2 is 2 / `Cond_ByteFD` 4 / 2; run 0xF step 0x78, run 7 with effects 0x96 /
0x95 by ObjTrio +0x3C, run 8 step 0x1E); then areas 0x2D, 0x41, 0x57, 0x10,
0x73 zero the four counters.

**The state machine** is chapter 12's ([`scena_sc12.md`](scena_sc12.md) §3):
each run a switch on the step byte `0x8034E5` (MSVC's two-level switch
where the cases are sparse), each case waiting on counter 0 (`0x903848`) at
a value, the request byte, the wait word, the stream (`Sound_StreamDone`),
the hold or a timer, then doing one thing and setting the next step; a run
ends with `ScriptFlags_Clear40` and the step and run 0, counters 1..3 (and
sometimes 0) zeroed.

**Recurring shapes, written once:**

- *The camera effect* (`Place13`): `Effect_FindFree` to `0x903850` (run 11:
  to `Scena09_Slot11`), and in the record the byte index read back from that
  cell: +0 = 1, +5 = 0x13, +0x64 / +0x68 / +0x6C (x, `Camera_Angles +2`, z),
  +9 a life.
- *The camera sweep* (`Sweep`): counter 3 + 1, `Camera_Distance += d`,
  `MapView_Redraw` 2; at a limit, Redraw 2, the distance set, counter 3 0.
  Run 3 (-0x100 a step to 0xD, then 0xFD80), run 4 (-0x5C to 0x20, then 0),
  run 15 (+0x20 to 0x14 then 0x280; -0x10 to 0x28 then 0; +0x80 to 0xA then
  0x500; -0x80 to 0xA then 0).
- *The literal rows*: `Flags_Set` / `Flags_Clear` handed `0x903FC0` (row 6,
  flag 0x3E: EnterArea, run 15) or `0x904030` (the story flags: run 15's
  0x45, 0x40, 0x33) instead of the pointer at `0x929ED0`.
- *Run 15's scenes*: steps 0..1, 0xA..0xC, 0xF..0x10, 0x14..0x16, 0x1E,
  0x28..0x29, 0x32..0x34, 0x3C..0x3E, 0x46..0x5E (a long scene with the four
  sweeps), 0x64..0x65, 0x6E..0x72, 0x78..0x7F, 0x82..0x83, 0x8C..0x96 - each
  a short chain ending in a flag and the run's end.

**As the originals have them** (each kept, and each said at its site):
run 3's step 4 stores its effect slot to counter 3 and step 5 then counts
the sweep up from it, so the sweep is shorter by the slot's number (its end
state, `Camera_Distance` 0xFD80, is the same); run 6's steps 0x22 / 0x2C
wait on the effect record counter 3 names (set to the slot two steps before);
run 8's step 0x21 leaves `Sprite_Current` at the last member it matched and
reads it again after `Effect_FindFree`; run 10 ends without
`ScriptFlags_Clear40`; EnterArea's 0x45 and 0x31 blocks read the selector as
a dword (the same low byte); run 11 counts its timer down with a word
decrement (a timer of 0 wraps).

## 4. The fuzz

`scena_sc9a_fuzz.cpp`, through `scenario_harness` with `chapter = 9`:

- **The clones**: `tools/scenario_rows.py --unit SC9a --clones` at
  `3e410e7`, identical to this reading extent for extent and call for call.
  Shapes: slot 0 `kSlot`; the states, runs and object handlers `kState`;
  slot 1 `kObject`.
- **One copy the group makes itself.** `Scena09_Run15` has 73 call sites;
  the harness re-aims at most 64 a clone (EnterArea has exactly 64). The fuzz
  file copies it with `bof3::CloneOriginal`, every site re-aimed at a
  trampoline that calls the harness's recorder for that callee
  (`scenario_harness::StandIn` - the same log entry, disturbance and answer a
  harness-aimed site gets), the jump table relocated into the copy; the
  harness is handed, as its original, a six-byte `jmp [copy]` of the fuzz's
  own - SC12's `Scena12_Run4` model. Theirs is still Capcom's bytes. No
  harness edit.
- **Callees**: the harness's 70 standard ones, plus four the group lists:
  `Flags_Test` as a `kFlag` (the block tests al alone), `Field_StartEventBattle`
  (group SE's, ours: called by name, recorded on the id's byte), `0x533E50`,
  and the log slot of the object stand-ins. `Scenario_CallB` (named by SCH,
  not taken) and `Sound_ResumeAll` are called by name.
- **Tables**: `Scena09_States` and `Scena09_Runs` swapped for recorders
  (their entries take no arguments). `Scena09_Objects` passes (object, bits),
  which a table recorder does not log, so the seed writes a typed stand-in
  into every entry - one per index, so a wrong index is a different log - and
  the table is a region the harness puts back.
- **Regions** beyond the harness's 22: the selector `0x90412C`,
  `Cond_ByteFE`, the music byte `0x904CD0`, `Field_ScriptFlags2`,
  `Field_MenuButton`, `Field_Kind2Hold`, `Scena09_Slot11`, the object table.
  30 regions, 10,075 bytes.
- **Seed**: the areas EnterArea tests (and counter 2 at 1 / 2 for it), the
  counters 1..3 (counter 3 one below each sweep's limit, at it, or an effect
  slot), the request byte 0 / 2 / 6, the wait word 0, the selector 7..18
  (bit 7 half the time), `Cond_ByteFD` 0 / 1 / 2 / 4, the hold, the low bits of
  `Field_ScriptFlags2`, the two members' +0x137 bytes, the input and menu
  words, the party list's kinds (7, 2, 4, 8, 5), ObjTrio +0x3C at both of its
  bounds and one either side, the timer 0 / 1 / 2 / 0x5F, run 11's slot and
  the in-use byte of the records counter 3 and that slot name; each run's
  case steps and one past, each with counter 0 at the value it waits on (or
  one off) two times in three (`kWaits`); the state 0..2, the run 0..16, the
  object's +0x86 0..15.
- **Disturbance** beyond the harness's: counters 1..3, the area, the
  selector, an effect's in-use byte, the hold, `Cond_ByteFD`, a bit of the
  script flags' low byte (8, 0x80, 0x40, 2, 4), run 11's slot, the members'
  bytes, the party list, `Field_ScriptFlags2`'s low bits, the input and menu
  words, ObjTrio +0x3C. A `settle` for EnterArea alone moves the area,
  counter 2, `Cond_ByteFD` and ObjTrio +0x3C half the time each after every
  disturbance, drawing on `Noise()` - the cells it reads again after calls;
  for run 15 it moves counter 2 half the time (control R15g, section 5).

**Result, in this worktree** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc9a`,
exit 0): 132,000 rounds, 103,680 calls to the stand-ins, **0 mismatches**.
Coverage, the calls the originals made: `Flags_Test` 8,083,
`ScriptFlags_Set40` 24,576, `Party_DropIn` 10,071, `Flags_Set` 9,365,
`ScriptFlags_Clear40` 8,595, `Field_ChangeArea` 6,855, `Msg_OpenScript`
4,217, `Transition_Start` 2,203, `Effect_FindFree` 2,118, `Kind2_Place`
2,076, `Sound_StreamDone` 1,590, `Sound_LoadStream` 1,240, `Sound_PlayEffect`
637, `0x533E50` 541, `Music_Play` 480, `Music_FadeOutStop` 461,
`Scenario_CallA` 444, `Flags_Clear` 403, `0x532ED0` 363, `0x56D6F0` 297,
`MoveCmd_TestFB` 275, `Field_StartEventBattle` 260, `Scenario_CallB` 192,
`Field_ViewReset` 147, `AreaMap_Elevation` 140, `Sound_ResumeAll` 51 (through
the copy's trampolines), the object stand-ins 6,000, and every state and run
table entry (`0x5646B0`, EnterArea, Run, the bare ret, the fourteen runs)
300..2,000 each. `BOF3X_SHADOW='*'`: exit 0 (every earlier group of both harnesses, then this one: 0 mismatches).

## 5. Controls

`scratchpad/sc9a/controls.py` (not committed; SC12's pattern): each mutant
planted in `scena_sc9a.cpp` on a unique anchor, rebuilt, the self-test run,
restored, and rebuilt at the end. Refused = exit 3 on a mismatch.

| Id | Mutant | Result |
|---|---|---|
| F1 | frame: state 1 read as 2 | refused (exit 3) |
| F2 | run: run 4 read as 5 | refused (exit 3) |
| F3 | object trigger: the row not passed | refused (exit 3) |
| F4 | object trigger: index + 1 below 15 | refused (exit 3) |
| A1 | enter: 0x25 selector 9 CallB 7 | refused (exit 3) |
| A2 | enter: 0x25 row-6 flag 0x3D | refused (exit 3) |
| A3 | enter: 0x27 Cond_ByteFD 1 | refused (exit 3) |
| A4 | enter: 0x29 counter 2 at 2 | refused (exit 3) |
| A5 | enter: 0x37 camera 0xA00 | refused (exit 3) |
| A6 | enter: 0x37 selector 13 CallA 3 | refused (exit 3) |
| A7 | enter: 0x45 selector 15 CallA 2 | refused (exit 3) |
| A8 | enter: 0x31 flag 0x12 cleared | refused (exit 3) |
| A9 | enter: 0x64 goes on without counter 2 = 2 | refused (exit 3) |
| A10 | enter: 0x76 second effect strict | refused (exit 3) |
| A11 | enter: 0x77 step 0x1F | refused (exit 3) |
| A12 | enter: area 0x57 dropped from the end list | refused (exit 3) |
| A13 | enter: state 1 | refused (exit 3) |
| A14 | enter: 0x35 Kind2 3 | refused (exit 3) |
| A15 | enter: 0x76 tested on the area read at entry | refused (exit 3) |
| R1a | run1: area flags 0x87 | refused (exit 3) |
| R1b | run1: step 2 waits on 0x12 | refused (exit 3) |
| R2a | run2: ScriptFlags2 & 3 | refused (exit 3) |
| R2b | run2: Music_Play frames 9 | refused (exit 3) |
| R2c | run2: step 7 clears flag 0x24 | refused (exit 3) |
| R3a | run3: step 3 x -0x253 | refused (exit 3) |
| R3b | run3: step 4 slot not to counter 3 | refused (exit 3) |
| R3c | run3: sweep limit 0xE | refused (exit 3) |
| R3d | run3: step 9 request before the message | refused (exit 3) |
| R4a | run4: step 5 bound <= | refused (exit 3) |
| R4b | run4: counter 1 swapped | refused (exit 3) |
| R4c | run4: sweep delta -0x5B | refused (exit 3) |
| R4d | run4: step 0x10 branch 2 flags 0x8B | refused (exit 3) |
| R4e | run4: step 0x22 hold not tested | refused (exit 3) |
| R4f | run4: script mask 0xFFBE | refused (exit 3) |
| R5a | run5: drop-in 0xB | refused (exit 3) |
| R5b | run5: step 6 keeps counter 0 | refused (exit 3) |
| R6a | run6: step 5 message 0xD | refused (exit 3) |
| R6b | run6: land waits on counter 2 | refused (exit 3) |
| R6c | run6: hop z 0x38D | refused (exit 3) |
| R6d | run6: step 0x65 no party pass | refused (exit 3) |
| R6e | run6: step 0x67 pass flags after the stream | refused (exit 3) |
| R6f | run6: step 0x38 flag 0x1E | refused (exit 3) |
| R6g | run6: step 0xA keeps counter 0 | refused (exit 3) |
| R6h | run6: step 0x33 count 0x14 to 0x36 | refused (exit 3) |
| R7a | run7: the menu button not masked | refused (exit 3) |
| R7b | run7: member byte 3 | refused (exit 3) |
| R7c | run7: step 6 request not tested | refused (exit 3) |
| R8a | run8: kinds 4 / 2 swapped | refused (exit 3) |
| R8b | run8: Sprite_Current not read again | refused (exit 3) |
| R8c | run8: counter 0 = 5 | refused (exit 3) |
| R8d | run8: effect +6 = 3 | refused (exit 3) |
| R8e | run8: event battle 0x36 | refused (exit 3) |
| R8f | run8: F3 divisor 0x21 | refused (exit 3) |
| R8g | run8: member kind 6 | refused (exit 3) |
| R9a | run9: effect kind 0x9B | refused (exit 3) |
| R9b | run9: music byte 0x60 | refused (exit 3) |
| R9c | run9: step 0x15 other counts keep the run | refused (exit 3) |
| R9d | run9: placement kind 0x37 | refused (exit 3) |
| R10a | run10: drop-in 2 | refused (exit 3) |
| R10b | run10: request 6 waited | refused (exit 3) |
| R11a | run11: timer less 2 | refused (exit 3) |
| R11b | run11: ground shifted 15 | refused (exit 3) |
| R11c | run11: step 7 tests bit 1 | refused (exit 3) |
| R11d | run11: slot to 0x903850 | refused (exit 3) |
| R11e | run11: step 4 life 0x29 | refused (exit 3) |
| R11f | run11: step 0x1E no bit 80 | refused (exit 3) |
| R11g | run11: step 0xD no view reset | refused (exit 3) |
| R11h | run11: step 2 ground from (z, x) | refused (exit 3) |
| R14a | run14: script xor 0xC | refused (exit 3) |
| R14b | run14: area flags 5 | refused (exit 3) |
| R15a | run15: sweep 0x50 limit 0x27 | refused (exit 3) |
| R15b | run15: story flag 0x46 | refused (exit 3) |
| R15c | run15: no Sound_ResumeAll | refused (exit 3) |
| R15d | run15: request 5 | refused (exit 3) |
| R15e | run15: step 0x33 flag 0x34 | refused (exit 3) |
| R15f | run15: script flags | 0x80 | refused (exit 3) |
| R15g | run15: step 0x5E counter 2 before the flag | refused (exit 3) (second pass; stood on the first - below) |
| R15h | run15: step 0x78 member byte 2 | refused (exit 3) |
| R15i | run15: CallB 8 | refused (exit 3) |
| R15j | run15: music byte 0x86 | refused (exit 3) |
| R16a | run16: flag 0x25 | refused (exit 3) |
| R16b | run16: counter 4 | refused (exit 3) |
| O1 | object 1: run 4 | refused (exit 3) |
| O2 | object 2: step 4 | refused (exit 3) |
| O3 | object 3: run 3 | refused (exit 3) |
| O4 | object 4: run 0xB | refused (exit 3) |

86 of 86 refused, every one by a count (exit 3 on a mismatch), at least one
per function. One stood on the first pass and was fixed in the fuzz, as wave
one's groups found: **R15g**, run 15's step 0x5E storing counter 2 before
the first `Flags_Set` instead of between the two calls - nothing moved
counter 2 after a call while run 15 ran (the group's disturbance reaches its
cells one call in 24). A `settle` for run 15 now moves counter 2 half the
time after each disturbance; the run-15 controls R15a..R15j were re-run on
the stronger fuzz and all refused.

## 6. Cross-group calls

| Callee | What | Owner | How |
|---|---|---|---|
| `0x5341C0` `Scenario_CallB` | call table B's thunk | named by SCH, not taken | by name |
| `0x4410B0` `Field_StartEventBattle` | (id): an event battle's set-up | group SE (wave one), ours | by name |
| `0x533E50` | a pass over the eight records at `0x903A70` and the party | nobody | raw |
| `0x532ED0` | (x, z, kind): a party placement before an event battle | nobody | raw |
| `0x56D6F0` | bit 7 of `0x8034E1` set | nobody | raw |
| `0x5646B0` | state 0 | group SC13, a later wave | through `Scena09_States`, read in place |
| `0x55BAC0`, `0x55BAE0`, `0x557170`..`0x557250` | objects 5..11, 13..15 | group SC9b, a later wave | through `Scena09_Objects`, read in place |
| `Sound_ResumeAll` | | Capcom's, named | by name |
| `Flags_*`, `ScriptFlags_*`, `Msg_OpenScript`, `Field_ChangeArea`, `Scenario_CallA`, `Party_DropIn`, `Kind2_Place`, `Transition_Start`, `Effect_FindFree`, `Sound_*`, `Music_*`, `MoveCmd_TestFB`, `AreaMap_Elevation`, `Field_ViewReset` | | ours (earlier rounds) | by name |

The call-table entries chapter 9's code reaches through `Scenario_CallA` /
`CallB` are group CALLS's this wave; ours reaches them only through those
two thunks.

## 7. What nothing reached, and latent defects

No route has reached chapter 9 (fuzz only, as every scenario group so far).
Every state and run table entry and every callee the block calls is reached
by the fuzz (the coverage above), and the 86 controls, one or more per
function, are all refused by a count.

Latent, described and kept (never fixed):

- **The unchecked tables** (section 2): a state of 3 runs the run table's
  entry 0 (a ret); a run of 17 or more, or an object byte of 16 or more,
  jumps through data. Ours aborts there instead. Nothing in the block writes
  such an index.
- **Run 6's effect wait** (steps 0x22 / 0x2C) indexes `Effect_Objects` by
  counter 3 unchecked; the step before stores the slot there, but counter 3
  is a movement-script counter, and a script that moved it past 19 before
  the wait would read past the 20 records (a read only; both sides the same).
- **Run 11's step 7** indexes `Effect_Objects` by `Scena09_Slot11` without
  testing it for `0xFF`: only a failed `Effect_FindFree` in step 6 would leave
  it so, and step 6 does not advance on one.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count) in [`known-defects.md`](known-defects.md).