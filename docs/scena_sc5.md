# Chapter 5's bank: 0x546390..0x54A910

**Status:** IN PROGRESS (2026-09-27) - 35 functions ours
(`src/game/scena_sc5.cpp`, shadow name `scena_sc5`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 700,000 rounds (20,000 a function); 110 of 110 negative controls refused, every one by a count. Fuzz only:
no recorded route reaches chapter 5 (section 7).

Group SC5 of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §6,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3), one stage:
read, written, fuzzed and controlled against the harness as merged at
`3e410e7`.

## 1. The band, and chapter 5's roots

`tools/scenario_roots.py --chapter 5`: chapter 5's closure is 57 walked
starts, 17,888 bytes, frontier 42. Its roots:

| Table | Entry | Target | Band |
|---|---|---|---|
| vtable `Scena05_Hooks` `0x661020` (0x662C80 entry 5) | slot 0, the frame | `0x546390` Scena05_Frame | SC5 |
| | slot 1, the object trigger | `0x54A2A0` Scena05_ObjectTrigger | SC5 |
| | slot 2, the step hook | `0x54A440` Scena05_StepHook | SC5 |
| | slot 3, the arrive hook | `0x539AC0` Scenario_NoHook | ours already (SCH) |
| | slot 4, the cell hook | `0x54A800` Scena05_CellHook | SC5 |
| call table A `Scena05_CallA` `0x65F69C` | 11 entries | `0x519A40`..`0x51A420` | CALLS (this wave) |
| call table B `Scena05_CallB` `0x65F6C8` | 5 entries | `0x519A50`..`0x519BE0` | CALLS (this wave) |

The sibling's `names/scenario_records.toml` pairs the vtable with SCENA05's
at `0x801FD5C8` (slot 0 `0x801F7A78`, 1 `0x801FCA58`, 2 `0x801FCD68`, 3
`0x801FCD60`, 4 `0x801FD110`) - hypotheses by position, not read here. The
call tables' entries are reached only through `Scenario_CallA` (ours) and
`Scenario_CallB` (named, Capcom's): nothing in this band calls one directly.
Beyond the band the closure reaches state 0, `0x5646B0` (group SC13's block,
a later wave; chapter 12's state 0 is the same address), through
`Scena05_States` read in place.

**The starts.** `tools/scenario_rows.py --unit SC5` lists 35 starts in the
band, 0x45B0 bytes, none ours; this reading agrees function for function,
extent for extent (recursive descent with capstone, every jump table and
byte table bounded). None dropped, none added. The roots walk lists 18 more
"starts" inside the band - `0x5464F0`, `0x546910` (cases of EnterArea's two
jump tables), `0x546D80`, `0x546E30`, `0x546F90` (Run5's cases 1, 0x19,
0x23), `0x547170` (Run8's case 4), `0x5475D0`, `0x547690` (Run13's cases 0
and 4), `0x547940`, `0x547AC0`, `0x547C80`, `0x5480D0`, `0x548550`,
`0x5486B0`, `0x548990`, `0x548C50`, `0x548D30`, `0x548D90` (Run16's cases
and inner cases) - every one a jump-table target, none reached by a call;
the tool drops them. `entries_logic.txt` had `0x54A1A0` (0x37, right) and a
run-on `0x54A1E0` of 0x3838; SC5's extents are appended there.

## 2. The tables

The chapter's tables, back to back after the vtable (`symbols.toml`
`[[data]]`), plus one before it:

| Address | Name | Entries | Read by | Holds |
|---|---|--:|---|---|
| `0x661018` | `Scena05_MemberBytes` | 8 s8 | Scena05_SpawnMember, `movsx [+ byte 0x904062]` | a per-member byte stored as an effect's dword +0x10 |
| `0x661034` | `Scena05_States` | 3 | Scena05_Frame, `jmp [+ s8 0x8034E2 * 4]` | `0x5646B0` (SC13's), EnterArea, Run |
| `0x661040` | `Scena05_Runs` | 23 | Scena05_Run, `jmp [+ s8 MoveScript_Var7 * 4]` | ret for 0, 2, 3, 12, 15; Run1, Run4..Run11, Run13, Run14, Run16..Run22 |
| `0x66109C` | `Scena05_Objects` | 9 | Scena05_ObjectTrigger, `call [+ object[0x86] * 4]` (object, bits) | ret; Object01..07; Object04 again |
| `0x6610C0` | `Scena05_CellRecords` | 2 x 5 bytes | `0x56D800` for Scena05_CellHook | two records (as `0x56D800` reads them) |
| `0x6610CC` | `Scena05_CellHooks` | 2 | Scena05_CellHook, `jmp [+ index * 4]` with (a, b) in place | Cell0, Cell1 |

A zero dword and eight bytes (`0x6610D8`, the same eight as
`Scena05_MemberBytes`, not read by this band) follow, then `Scena06_Hooks`
`0x6610E0`. The originals index every table unchecked (a signed byte for
the state and the run); an index past a table reads the next one. **Ours
aborts with a message** for an index outside the table's own entries
(negative included), the project's rule for an index past a table (round
nine §6, the owner: no DIVERGENCE entry). No code in the band writes such an
index; `MoveScript_Var7` is also set by the movement script's op F6, so a
script storing a run of 23 or more would stop here where the original ran
an object handler. `Scena05_MemberBytes` is only read (a byte, not a jump):
ours reads the byte in place as the original does, whatever the member
byte (section 7).

## 3. The functions

Call shapes: **slot** - a vtable slot, no arguments; **object** - slot 1,
the object; **hook** - (x, z) or (a, b), answering in al; **state** - a
handler reached through a chapter table, no arguments read; **direct** - an
`E8` of the chapter's own.

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x546390` | Scena05_Frame | 0xE | slot 0 | the state's handler through `Scena05_States` (tail jump) |
| `0x5463A0` | Scena05_EnterArea | 0x708 | state 1 | by the area: 0x2D, 0x31 (run 6 by the selector), 0x34 (counter 2's five cases), 0x41 (runs 0 / 9), 0x44, 0x45 (music 1 loaded), 0x4E (counter 2's seven cases: music, runs 0xA / 0xD, call-table entries, drop-ins, bit 0 of `0x903DAF` / `0x903E53`), 0x4F, 0x50, 0x51; state 2 on every exit |
| `0x546AB0` | Scena05_Run | 0xE | state 2 | the run's handler through `Scena05_Runs` (tail jump) |
| `0x546AC0` | Scena05_Run1 | 0xB8 | state (run 1) | message 5; a change into 0x44; flag 1 at counter 0 0x2D |
| `0x546B80` | Scena05_Run4 | 0x1DA | state (run 4) | area 0x34: placement and event battle 0x11, drop-in, flags 5 / 6, message 0xB |
| `0x546D60` | Scena05_Run5 | 0x2D7 | state (run 5) | steps 1..6, 0x19..0x23: messages, drop-ins, area 0x34 changes, music, a transition, call A 0, flags 8 / 9 |
| `0x547040` | Scena05_Run6 | 0x4A | state (run 6) | flag 0xA at counter 0 0x18 |
| `0x547090` | Scena05_Run7 | 0x45 | state (run 7) | message 0x26 and its close |
| `0x5470E0` | Scena05_Run8 | 0x158 | state (run 8) | a transition, `0x533E50`, a stream, a change into 0x31, then into 0x45 with flags 0xC, 0xE, 0xF |
| `0x547240` | Scena05_Run9 | 0x1F0 | state (run 9) | music 1; changes into 0x2D, 0x32, 0x4E; flags 0x10..0x13 |
| `0x547430` | Scena05_Run10 | 0x126 | state (run 10) | flags 0x16..0x1A; changes into 0x4E |
| `0x547560` | Scena05_Run11 | 0x45 | state (run 11) | message 0 and its close |
| `0x5475B0` | Scena05_Run13 | 0x26F | state (run 13) | a choice after messages 0x20 / 0x1F by the selector, flags 0x16 / 0x17 |
| `0x547820` | Scena05_Run14 | 0xF4 | state (run 14) | messages 0x23..0x25, flag 0x1A |
| `0x547920` | Scena05_Run16 | 0x1BD0 | state (run 16) | 59 cases over steps 0..0x66 (below) |
| `0x5494F0` | Scena05_Run17 | 0x58C | state (run 17) | messages, a timed wait with two dropped PSX calls, battle 0x15, changes into 0x51 / 0x4E |
| `0x549A80` | Scena05_Run18 | 0x3D0 | state (run 18) | area 0x50's scene, battle 0x16, `0x533E50` |
| `0x549E50` | Scena05_Run19 | 0x48 | state (run 19) | message 0x84 and its close |
| `0x549EA0` | Scena05_Run20 | 0x230 | state (run 20) | Scena05_SpawnMember(0x91); changes into 0x4E; bit 0 of three bytes; call A |
| `0x54A0D0` | Scena05_Run21 | 0x78 | state (run 21) | flag 0x3A set and cleared around a drop-in |
| `0x54A150` | Scena05_Run22 | 0x45 | state (run 22) | message 0x4C and its close |
| `0x54A1A0` | Scena05_PartyCounter | 0x37 | direct (Run16) | counter 1 = 1 for a party byte 5, then 2 for a 6 |
| `0x54A1E0` | Scena05_SpawnMember | 0xB3 | direct (Run20), (kind) | an effect at ObjTrio record 0 (kind 6, +6 kind + 0x70, +0x10 the member's byte) |
| `0x54A2A0` | Scena05_ObjectTrigger | 0x1F | slot 1, object | `Scena05_Objects[object +0x86]` (object, bits) |
| `0x54A2C0` | Scena05_Object01 | 0x4D | state (object 1) | flag 2 clear: set it, run 0x15 |
| `0x54A310` | Scena05_Object02 | 0x38 | state (object 2) | flag 0xB, run 8 |
| `0x54A350` | Scena05_Object03 | 0x28 | state (object 3) | run 0xC |
| `0x54A380` | Scena05_Object04 | 0x28 | state (objects 4, 8) | run 0xD |
| `0x54A3B0` | Scena05_Object05 | 0x23 | state (object 5) | run 0x10, counter 0 kept |
| `0x54A3E0` | Scena05_Object06 | 0x23 | state (object 6) | run 0x11, counter 0 kept |
| `0x54A410` | Scena05_Object07 | 0x23 | state (object 7) | run 0x12, counter 0 kept |
| `0x54A440` | Scena05_StepHook | 0x3B7 | slot 2, hook (x, z) -> al | seven rectangles in areas 0x31, 0x34, 0x43, 0x44, 0x4E under flags: runs 7, 4, 0xB, 1, 0x13, 0x14, 0x16 |
| `0x54A800` | Scena05_CellHook | 0x2A | slot 4, hook (a, b) -> al | `0x56D800` over the two records; negative 0xFF, else `Scena05_CellHooks` |
| `0x54A830` | Scena05_Cell0 | 0x69 | state (cell hook 0) -> al | flags 6 set / 9 clear: story flag 0x26, sound 0x202, step 1; 1; else 0xFF |
| `0x54A8A0` | Scena05_Cell1 | 0x6B | state (cell hook 1) -> al | the same with story flag 0x25 and step 0x19 |

Every exit of every function is in `symbols.toml`'s evidence line with its
extent and tables. What the runs are in the story is not read here: the
areas, flags and messages are numbers, and a scene's meaning is the
owner's to say.

**The state machine.** As every chapter's: each run is a switch on the step
byte `0x8034E5` (MSVC's two-level switch where the cases are sparse). Each
case waits on something - counter 0 (`0x903848`) at a value, the request
byte `Field_Request` not 2 (a message closed) or 0, the wait word
`0x66C810` at 0, the message box's done bit (`0x7DEE44` & 2) - then does
one thing and sets the next step. A run ends with `ScriptFlags_Clear40`,
counters 1..3 (or all four) and the step and run 0.

**Run 16.** The chapter's longest (0x1BD0 bytes, 231 calls, 23 jump
tables): steps 5 / 6 set counter 3 (`0x90384B`) to a code 1..6 or 0xB..0x10
from the pair of party bytes `0x904065` / `0x904066`; later steps pick
messages, call-table B pairs and area-0x4F changes by that code, messages by
the first member `0x904062` (0..6), and event battles 0x12 / 0x13 / 0x14 at
the leader by flags 0x1E / 0x1F. Ours keeps each of its inner tables as a
table of the same entries (`kMsg12`, `kExit34`, `kHall47`, ...), each
checked entry by entry against the jump tables (`scratchpad/sc5/sw.py`).

**Recurring shapes, written once:** *the selector* `Cond_Flags + 0x19C` &
0x7F, 1 or 2 (6 in two places) picks a branch; *the music wait*
`Music_LoadFile(t)`, `Task_Sleep(1)` until `File_LoadDone` (EnterArea's
0x45 and 0x50 load without playing); *a battle at the leader*
`0x532ED0(ObjTrio +0x34, +0x38, n)` then `Field_StartEventBattle(n)`.

**As the originals have them** (each kept, and each said at its site):
Run9 reads counter 0 once on entry for steps 1 and 2 and step 1 falls into
step 2's test; Run16's step 0x44 sets flag 0x25 and never moves on; Run16's
step 0x36 / 0x40 message tables leave members 2..4 without a message
under two of the three flag states while step 0x1F's gives them 0x4B;
Run17's step 0xB plays sound 0x209 twice in one frame when the timer
reaches 0xC8 and `0x803150` is at most 8; Run17 pushes a 4 above the first
`Port_DroppedCall(0x10)` (a dropped PSX call's second argument, unread);
Scena05_Cell0 / Cell1 set a step without a run; the step hook tests
`x <= 0x86.8` twice in area 0x34; EnterArea and the step hook read the area
afresh for each test.

## 4. The fuzz

`scena_sc5_fuzz.cpp`, through `scenario_harness` with `chapter = 5`:

- **The clones**: `tools/scenario_rows.py --unit SC5 --clones` at
  `3e410e7`, identical to this reading extent for extent and call for call.
  Shapes: slot 0 `kSlot`; the states, runs, object handlers and the two
  direct helpers `kState`; slot 1 `kObject`; slots 2 and 4 and the two
  cell entries `kHook` (compared on al).
- **Two copies the group makes itself**, as SC12 did: `Scena05_EnterArea`
  has 65 call sites and `Scena05_Run16` 231; the harness re-aims at most 64
  a clone. The fuzz file copies both with `bof3::CloneOriginal`, every site
  re-aimed at a trampoline that calls the harness's recorder for that
  callee (`scenario_harness::StandIn`), the jump tables relocated
  (`move_script::Relocate`; the byte tables are read in place from the
  original, unchanged at start-up); the harness is handed a six-byte
  `jmp [copy]` as each one's original. No harness edit.
- **Callees**: the harness's 70 standard ones, plus: `Flags_Test` as a
  `kFlag` (the bank tests al alone); `Field_StartEventBattle` by name (group
  SE's, ours; the standard set lists `0x4410B0` by address, and the group's
  listing stands); `0x533E50`; `0x56D800` as a byte 0xFF, 0 or 1; the two
  direct helpers - `Scena05_PartyCounter` with an `effect` that sets counter
  1 from the party bytes as the function does (Run16's step 0x11 reads it
  back), `Scena05_SpawnMember` with its kind logged; the log slots of the
  table stand-ins below.
- **Tables**: `Scena05_States` and `Scena05_Runs` swapped for recorders.
  `Scena05_Objects` and `Scena05_CellHooks` pass arguments a table recorder
  does not log, so the seed writes a typed stand-in into every entry, one
  per index, and the two tables are regions.
- **Regions** beyond the harness's 22: the selector `0x90412C`, the music
  byte `0x904CD0`, the message box's flag word `0x7DEE44`, the bytes
  `0x903B1F`, `0x903DAF`, `0x903E53`, `0x803150`, `0x92BF10`, and the two
  tables. 32 regions, 10,054 bytes.
- **Seed**: the areas the bank tests, EnterArea's paired with counter 2's
  cases 0..7; the request byte 0 / 2 / 6; the wait word 0; the selector 1,
  2, 6 and others (bit 7 half the time); the done bit; counter 3's codes and
  their neighbours; the party bytes 0..7 and 0xFF, the pair 0, 1, 2, 5, 6;
  `0x803150` about 8; the timer about 0xC8; each run's case steps and one
  past, each with counter 0 at one of the values that step waits on two
  times in three (`kWaits`); for run 16 half the time one of its case
  steps, counter 3 one of the twelve codes two times in three, and at steps
  5 / 6 a pair of party bytes the step maps; the state 0..2, the run 0..22, the object's
  +0x86 0..8; the step hook's (x, z) half the time at the bounds of one of
  its eight rectangles (one either side, or the middle of the range) with
  the area set to the rectangle's.
- **Disturbance** beyond the harness's (drawing only from its hash):
  counters 0..3, the area, the selector, the done bit, a party byte, counter
  3's code, bit 0 of `0x903DAF` / `0x903E53`, `0x803150`, the pass flags.
  A `settle` (drawing on `Noise()`): for EnterArea and the step hook (both
  re-read the area after calls) the area moved to one they test half the
  time, and for EnterArea counter 0 as well; for run 16 the pass flags half
  the time (it stores them before or after its transitions).

**Result, in this worktree** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc5`,
exit 0): 700,000 rounds, 680,702 calls to the stand-ins, **0 mismatches**.
Coverage, the calls the originals made: `Flags_Test` 127,265, `Flags_Set`
97,653, `ScriptFlags_Set40` 136,601, `ScriptFlags_Clear40` 48,749,
`Msg_OpenScript` 44,790, `Field_ChangeArea` 25,830, `Party_DropIn` 10,881,
`Kind2_Place` 12,019, `Sound_PlayEffect` 13,974, `Music_Play` 7,848,
`Transition_Start` 5,820, `Flags_Clear` 7,603, `Scenario_CallA` 4,365,
`Scenario_CallB` 2,419, `Field_StartEventBattle` 3,109, `0x532ED0` 2,831,
`0x533E50` 1,945, `Port_DroppedCall` 1,222, `Sound_LoadStream` /
`StreamDone` 2,459 / 2,499, `Music_LoadFile` 358 with `File_LoadDone` 549
and `Task_Sleep` 191 (the wait loop runs), `0x56D6F0` 368,
`Scena05_PartyCounter` 157, `Scena05_SpawnMember` 2,559, `0x56D800`
20,000, the object and cell stand-ins 20,000 / 13,337, and every state and
run table entry about 830..6,800 each.
`BOF3X_SHADOW='*'`: exit 0 (this worktree, with every earlier group).

## 5. Controls

`scratchpad/sc5/controls.py` (not committed; the pattern of `magic_s31`'s
and SC12's): each mutant planted in `scena_sc5.cpp` on a unique anchor,
rebuilt, the self-test run, restored, and rebuilt at the end. Refused =
exit 3 on a mismatch.

| Id | Mutant | Result |
|---|---|---|
| F1 | frame: state 1 read as 2 | refused (exit 3) |
| F2 | run: run 4 read as 5 | refused (exit 3) |
| F3 | object trigger: the row not passed | refused (exit 3) |
| F4 | object trigger: index + 1 below 8 | refused (exit 3) |
| F5 | cell hook: count 1 | refused (exit 3) |
| F6 | cell hook: negative answers 0 | refused (exit 3) |
| F7 | cell hook: (b, a) | refused (exit 3) |
| H1 | step: 0x31 flag 0xB not tested | refused (exit 3) |
| H2 | step: 0x31 z top 0x257FFF | refused (exit 3) |
| H3 | step: 0x34 x bottom + 1 | refused (exit 3) |
| H4 | step: 0x34 z 0x18.8 dropped | refused (exit 3) |
| H5 | step: 0x43 run 0xC | refused (exit 3) |
| H6 | step: 0x44 flag 1 | refused (exit 3) |
| H7 | step: 0x4E x 0x28.8 + 1 | refused (exit 3) |
| H8 | step: 0x4E flag 0x32 set run 0x15 | refused (exit 3) |
| H9 | step: 0x34 second step 0x15 | refused (exit 3) |
| H10 | step: 0x4E second z bottom + 1 | refused (exit 3) |
| H11 | step: area read once (0x31 then 0x34) | refused (exit 3) |
| C1 | cell0: story flag 0x27 | refused (exit 3) |
| C2 | cell: flag 9 not tested | refused (exit 3) |
| C3 | cell: counter 2 kept | refused (exit 3) |
| C4 | cell1: step 0x18 | refused (exit 3) |
| C5 | cell: counters after the sound | refused (exit 3) |
| O1 | object01: flag 3 tested | refused (exit 3) |
| O2 | object02: run 9 | refused (exit 3) |
| O3 | object03: run 0xB | refused (exit 3) |
| O4 | object04: counter 0 kept | refused (exit 3) |
| O5 | object05: counter 0 cleared | refused (exit 3) |
| O6 | object06: run 0x10 | refused (exit 3) |
| O7 | object07: run 0x13 | refused (exit 3) |
| O8 | objects: run set after Set40 swapped | refused (exit 3) |
| P1 | party counter: 6 -> 7 | refused (exit 3) |
| P2 | party counter: two bytes for 5 | refused (exit 3) |
| S1 | spawn: kind + 0x71 | refused (exit 3) |
| S2 | spawn: member byte unsigned | refused (exit 3) |
| S3 | spawn: sprite read before the call | refused (exit 3) |
| S4 | spawn: +0x30 from +0x32 | refused (exit 3) |
| S5 | spawn: +5 kind 7 | refused (exit 3) |
| A1 | enter: 0x2D flag 0x3F | refused (exit 3) |
| A2 | enter: 0x31 selector 6 call A 7 | refused (exit 3) |
| A3 | enter: 0x34 case 4 counter 0 after the clear | refused (exit 3) |
| A4 | enter: 0x41 run 8 | refused (exit 3) |
| A5 | enter: 0x44 counter 2 test 2 | refused (exit 3) |
| A6 | enter: 0x45 music 2 | refused (exit 3) |
| A7 | enter: 0x4E case 0 mask 0xFD | refused (exit 3) |
| A8 | enter: 0x4E case 5 drop-in 0xE | refused (exit 3) |
| A9 | enter: 0x4E case 6 call A 4 | refused (exit 3) |
| A10 | enter: 0x50 music 0x67 | refused (exit 3) |
| A11 | enter: 0x51 kind 0x16 | refused (exit 3) |
| A12 | enter: no state 2 | refused (exit 3) |
| A13 | enter: 0x4E above 7 ends | refused (exit 3) |
| A14 | enter: 0x4E case 1 sound before the stores | refused (exit 3) |
| A15 | enter: 0x34 case 1 flag 4 | refused (exit 3) |
| A16 | enter: 0x31 selector read after the stores (unmasked) | refused (exit 3) |
| R1 | run1: change area 0x45 | refused (exit 3) |
| R2 | run4: step 2 place z 0x210000 | refused (exit 3) |
| R3 | run4: step 9 script 0x10 | refused (exit 3) |
| R4 | run5: step 0x20 frames 8 | refused (exit 3) |
| R5 | run5: step 0x22 request 1 | refused (exit 3) |
| R6 | run6: counter 0x19 | refused (exit 3) |
| R7 | run7: message 0x27 | refused (exit 3) |
| R8 | run8: fade 0x1F | refused (exit 3) |
| R9 | run8: clear 0x3E | refused (exit 3) |
| R10 | run9: step 1 no fallthrough | refused (exit 3) |
| R11 | run9: selector 6 counter 4 | refused (exit 3) |
| R12 | run10: flag 0x15 | refused (exit 3) |
| R13 | run11: message 1 | refused (exit 3) |
| R14 | run13: choice 0x2D | refused (exit 3) |
| R15 | run13: step 5 flags swapped | refused (exit 3) |
| R16 | run14: message 0x26 | refused (exit 3) |
| R17 | run13: selector 1 step 0xE | refused (exit 3) |
| R18 | run14: counter 0xA step 3 | refused (exit 3) |
| R19 | run16: step 2 at 0x15 | refused (exit 3) |
| R20 | run16: pair (1, 5) code 3 | refused (exit 3) |
| R21 | run16: step 0x11 base 0x88 | refused (exit 3) |
| R22 | run16: step 0x12 code 0x10 message 0x35 | refused (exit 3) |
| R23 | run16: step 0x15 (4, 3) | refused (exit 3) |
| R24 | run16: member 5 message 0x3F | refused (exit 3) |
| R25 | run16: step 0x34 code 4 pass before | refused (exit 3) |
| R26 | run16: step 0x3E code 0xF flags 0x8C | refused (exit 3) |
| R27 | run16: hall 47 code 6 0x9E | refused (exit 3) |
| R28 | run16: hall 65 code 0xE (4, 1) | refused (exit 3) |
| R29 | run16: step 0x5D call A 3 | refused (exit 3) |
| R30 | run16: step 0x5F flag 0x3C | refused (exit 3) |
| R31 | run16: member message 6 0x4B | refused (exit 3) |
| R32 | run16: flag battle 0x13 and 0x14 swapped | refused (exit 3) |
| R33 | run16: step 0x44 flag 0x26 | refused (exit 3) |
| R34 | run16: party gate member 5 0x8B | refused (exit 3) |
| R35 | run16: step 3 counters kept | refused (exit 3) |
| R36 | run16: step 0x12 sound after the message | refused (exit 3) |
| R37 | run16: step 0x36 message without request byte | refused (exit 3) |
| R38 | run16: step 0x5A counter 0 kept | refused (exit 3) |
| R39 | run17: timer strict | refused (exit 3) |
| R40 | run17: level at most 7 | refused (exit 3) |
| R41 | run17: one dropped call | refused (exit 3) |
| R42 | run17: step 0x16 counter 2 3 | refused (exit 3) |
| R43 | run17: step 7 selector 2 flags 0x81 | refused (exit 3) |
| R44 | run17: level unsigned | refused (exit 3) |
| R45 | run18: selector 2 call B (3, 1) | refused (exit 3) |
| R46 | run18: step 0 counter 2 | refused (exit 3) |
| R47 | run18: battle 0x17 | refused (exit 3) |
| R48 | run19: message 0x85 | refused (exit 3) |
| R49 | run20: 0x903B1F bit 1 | refused (exit 3) |
| R50 | run20: step 2 script 0x10 | refused (exit 3) |
| R51 | run21: Kind2_Place(1) | refused (exit 3) |
| R52 | run22: message 0x4D | refused (exit 3) |
| R53 | run5: step 3 counter 0x1F | refused (exit 3) |
| R54 | run10: step 4 counters after Clear40 | refused (exit 3) |
| R55 | run18: step 0x10 script xor 0x10 | refused (exit 3) |
| R56 | run9: step 0xC selector 1 music 0xFE | refused (exit 3) |

Three stood on the first pass (8,000 rounds a function; 107 of 110 refused),
each fixed in the fuzz, not in ours:
- A3, EnterArea's area-0x34 case 4 storing counter 0 before
  `Flags_Clear(0x904030, 0x26)`: nothing moved counter 0 across that call.
  EnterArea's `settle` now moves counter 0 as well as the area.
- R20, a wrong code for the party pair (1, 5) at run 16's step 5: the pair
  was drawn byte by byte and landed on a mapped one about once in fifty
  rounds of an already rare step. The seed now picks run 16's case steps
  half the time, counter 3 among the twelve codes, and at steps 5 / 6 a
  mapped pair.
- R25, the pass flags stored before rather than after one transition of
  step 0x34: the pass flags were never disturbed across
  `Transition_Start`. The group's disturbance now moves them, and a
  `settle` for run 16 half the time. Rounds raised to 20,000 a function.
The final pass (above) ran every control against the final fuzz.

## 6. Cross-group calls

| Callee | What | Owner | How |
|---|---|---|---|
| `0x5341C0` `Scenario_CallB` | call table B's thunk | named by SCH, not taken | by name |
| `0x4410B0` `Field_StartEventBattle` | (kind): an event battle's set-up | group SE (wave one), ours | by name |
| `0x532ED0` | (x, z, kind): a party placement before a battle | nobody | raw |
| `0x533E50` | a pass over the eight records at `0x903A70` and the party | nobody | raw |
| `0x56D6F0` | bit 7 of `0x8034E1` set | nobody | raw |
| `0x56D800` | the cell-record search (records, count, a, b) | nobody | raw |
| `0x5646B0` | state 0 | group SC13, a later wave | through `Scena05_States`, read in place |
| `Scena05_CallA` / `_CallB` entries `0x519A40`..`0x51A420` | the call tables' entries | group CALLS, this wave | through `Scenario_CallA` / `Scenario_CallB` only |
| `Flags_*`, `ScriptFlags_*`, `Msg_OpenScript`, `Field_ChangeArea`, `Scenario_CallA`, `Party_DropIn`, `Kind2_Place`, `Transition_Start`, `Effect_FindFree`, `Port_DroppedCall`, `Music_*`, `Sound_*`, `File_LoadDone`, `Task_Sleep` | | ours (earlier rounds) | by name |

No call leaves the band into another scenario group's band (SC6's
`0x54A910..` included).

## 7. What nothing reached, and latent defects

No route has reached chapter 5 (fuzz only, as every scenario group so far).
Latent, described and kept:

- **`Scena05_SpawnMember` indexes `Scena05_MemberBytes` by the first
  member byte unchecked.** The table is eight bytes; a member byte of 8 or
  more reads on into `Scena05_Hooks` and the tables after it (still data,
  no fault). Ours reads the same byte in place. Nothing measured puts a
  member above 8 there.
- **Run16's step 0x44 never leaves**: it sets flag 0x25 every frame and
  keeps the step. No code in the band stores step 0x44 - like steps 0x1B,
  0x1C, 0x23, 0x29 and 0x3A, it is an entry written from outside (the
  movement script's step and run ops), so whatever wrote it presumably
  moves it on; not a defect on this reading, only unexplained here.
- The table aborts of section 2 are the only departure, and only for
  indices the bank never writes.
