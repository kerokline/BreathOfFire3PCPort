# Chapter 6's bank: 0x54A910..0x54F080

**Status:** IN PROGRESS (2026-09-27) - 48 functions ours
(`src/game/scena_sc6.cpp`, shadow name `scena_sc6`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
**0 mismatches in 288,000 rounds** (6,000 a function); **92 negative
controls planted, 92 refused by a count** (at least one per function);
`BOF3X_SHADOW='*'` exit 0. Fuzz only: no route was played (section 7).

Group SC6 of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §6,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3), one stage:
read, written, fuzzed and controlled against the harness as merged.

## 1. The band, chapter 6's roots, and the starts

`tools/scenario_roots.py --chapter 6`: the vtable `Scena06_Hooks`
`0x6610E0` (`0x662C80` entry 6) and the two call tables:

| Table | Entry | Target | Owner |
|---|---|---|---|
| `Scena06_Hooks` | slot 0, the frame | `0x54A910` Scena06_Frame | SC6 |
| | slot 1, the object trigger | `0x54E440` Scena06_ObjectTrigger | SC6 |
| | slot 2, the step hook | `0x54EA80` Scena06_StepHook | SC6 |
| | slot 3, the arrive hook | `0x539AC0` Scenario_NoHook (shared) | ours already |
| | slot 4, the cell hook | `0x54EED0` Scena06_CellHook | SC6 |
| `Scena06_CallA` `0x65F6DC` | 10 entries | `0x519BF0`..`0x519D00`, `0x51A420` | group CALLS (this wave): raw, section 6 |
| `Scena06_CallB` `0x65F704` | 8 entries | `0x519BC0`..`0x519DC0` | the same |

The sibling's `names/scenario_records.toml` pairs chapter 6's PSX vtable
(`SCENA06.EMI`, table `0x801FE3C4`) slot by slot: slot 0 `0x801F7298`, 1
`0x801FCDD0`, 2 `0x801FD800`, 4 `0x801FDD14` - cited as twins by slot in
`symbols.toml`, no name taken from them. (The PSX slot 3 is `0x801FD294`
where the PC's is the shared `Scenario_NoHook`; not read here.)

**The starts.** `tools/scenario_rows.py --unit SC6`: 48 functions,
`0x464C` bytes, 0 ours, 4 not walked. This reading agrees function for
function, extent for extent, call for call and table for table (each read
to its last instruction with capstone, the jump tables and MSVC's byte
tables of the sparse switches dumped and followed). What the start lists
hold that is not a function: **eleven `pc_hidden` starts are switch cases**
inside the band's functions - `0x54AAD0` (Scena06_EnterArea +0x1A0),
`0x54BB70` / `0x54BDA0` (Run06 +0x90 / +0x2C0), `0x54BFA0` / `0x54C040` /
`0x54C120` / `0x54C1E0` / `0x54C260` (Run07 +0x80 / +0x120 / +0x200 /
+0x2C0 / +0x340), `0x54DC40` / `0x54DCF0` (Run15 +0x1B0 / +0x260) and
`0x54DEF0` (Run16 +0x20); the tool drops them, and so does this group.
`analysis/pairs_propagated.json` pairs `0x54AAD0` with nine PSX addresses:
a switch case, not a function (as SC3 found for `0x5455A0`). No start is
missing: every byte between the 48 extents is alignment padding.

**The four the walk did not reach** are one mechanism, not dead code:
`0x54E790` Scena06_Leap is called by E8 from **area 77's handler
`0x40F090`** (at `0x40F0E6`; an E8 scan of all of `.text`), an area
overlay's code outside every scenario root, and `0x54E7D0` / `0x54E8E0` /
`0x54E9F0` are the three entries of its table `Scena06_LeapPhases`
(`0x661188`, read by nothing else). Section 3 has what they do.

**What the walk's closure says about chapters 7 and 8 is an artefact.**
Chapter 6's closure (125 functions) runs on into group SC7's band
(`0x54F080`..`0x553810`) because the walk reads a `.data` pointer table
until the first dword that is not code: `Scena06_CellHandlers` (4 entries at
`0x6611A8`) is followed directly by `Scena07_Hooks` `0x6611B8`, and the walk
reads chapter 7's vtable and tables as more cell handlers. The code says
otherwise: no E8 / E9 of the band reaches `0x54F080..0x553B30`, none there
reaches this band, and no absolute reference to any of the 48 starts lies
outside chapter 6's own tables (a scan of every call, jump and dword of the
image). Nothing of this band is shared with chapters 7 and 8, and nothing of
theirs is called from here (section 6).

## 2. The tables

Chapter 6's own, back to back from `0x6610D8` (`symbols.toml` `[[data]]`,
named this round):

| Address | Name | Entries | Read by | Holds |
|---|---|--:|---|---|
| `0x6610D8` | `Scena06_MemberBytes` | 8 s8 | LeaderEffect, Run14 step 2, `[+ party byte]` | an effect's +0x10 by the party member's id |
| `0x6610F4` | `Scena06_States` | 3 | Frame, `jmp [+ s8 0x8034E2 * 4]` | Start, EnterArea, Run |
| `0x661100` | `Scena06_Runs` | 18 | Run, `jmp [+ s8 MoveScript_Var7 * 4]` | the bare ret `0x437CC0`, Run01..Run17 |
| `0x661148` | `Scena06_Objects` | 14 | ObjectTrigger, `call [+ object[0x86] * 4]` (object, bits) | the bare ret, Object01..13 |
| `0x661180` | `Scena06_GuestStats` | 8 bytes | GuestRecord | character record 7's HP, stats and two equipment bytes |
| `0x661188` | `Scena06_LeapPhases` | 3 | Leap, `call [+ Sprite_Current[4] * 4]` (seven words) | LeapStart, LeapAir, LeapLand |
| `0x661194` | `Scena06_Cells` | 4 x 5 bytes | `0x56D800` for CellHook | area, cell, direction, run (as `0x56D800` reads them) |
| `0x6611A8` | `Scena06_CellHandlers` | 4 | CellHook, `jmp [+ movsx al * 4]` with (x, z) in place | Cell0, the bare ret, Cell2, Cell3 |

`Scena07_Hooks` follows at `0x6611B8`. The originals index every table
unchecked (a signed byte for the state and the run); an index past a table
reads the next one. **Ours aborts with a message** for an index outside the
table's own entries, the project's rule (round nine §6, the owner: no
DIVERGENCE entry). No code of the band writes such an index. The bare ret
at `Scena06_CellHandlers[1]` answers what `0x56D800` left in eax - the
index found, 1 - and ours answers exactly that (`Scena06_CellHook`).

## 3. The functions

Call shapes: **slot** - a vtable slot, no arguments; **object** - slot 1,
the object; **hook** - (x, z) answering in al; **state** - reached through
a chapter table or by E8, no arguments read; **entry** - words on the
stack (the leap's seven, LeaderEffect's one).

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x54A910` | Scena06_Frame | 0xE | slot 0 | the state's handler through `Scena06_States` (tail jump) |
| `0x54A920` | Scena06_Start | 0xD | state 0 | Scena06_GuestRecord, state 1 |
| `0x54A930` | Scena06_EnterArea | 0x4A0 | state 1 | by the area (0x27, 0x2E, 0x35, 0x4C, 0x57, 0x5C, 0x5E, each read afresh) and counter 2: members' record bits, call-table entries by the selector, run 0x11 / 6, Scena06_PartyCalls, `0x56D6F0`, `0x532ED0`, the camera distance, pass flags; areas 0x2D / 0x41 / 0x57 / 0x10 clear the counters; state 2 on every exit |
| `0x54ADD0` | Scena06_Run | 0xE | state 2 | the run's handler through `Scena06_Runs` (tail jump) |
| `0x54ADE0` | Scena06_Run01 | 0x1A4 | run 1 | 7 cases: into area 0x27 twice, a 0xB4-frame count-down on the word `0x8034E6`, flags 3 / 0x3E |
| `0x54AF90` | Scena06_Run02 | 0x2C0 | run 2 | 11 cases: drop-ins by ObjTrio +8, message 0x42, two effects of kind 0x13, flags 4 / 5 / 7 |
| `0x54B250` | Scena06_Run03 | 0x468 | run 3 | 14 cases: areas 0x2F, 0x35, 0x2D, effects of kind 0x31 / 0x13, music 0x27 / 0x24, sounds, flags 9..0xB, `0x591900(4)`, two members' record bits, `0x533E50` |
| `0x54B6C0` | Scena06_Run04 | 0x2F4 | run 4 | 16 cases: messages 0xD / 0x1A / 0x29 / 0x13, the kind-2 focus, area 0x5E, flags 0xD..0x10 |
| `0x54B9C0` | Scena06_Run05 | 0x120 | run 5 | 6 cases: areas 0x35 / 0x57, a stream waited on, flag 0x11 |
| `0x54BAE0` | Scena06_Run06 | 0x440 | run 6 | 19 cases: area 0x5E three times, message 0x38, a camera pull (Camera_Distance -0x40 a frame for 0x12 frames), effects, zenny `0x591BE0(0x3E8, 0)`, flags 0x10, 0x12..0x14 |
| `0x54BF20` | Scena06_Run07 | 0x55F | run 7 | 34 steps (29 handlers): areas 0x5C / 0x57, the request 7 with the byte `0x904C9F`, character record 7's two equipment bytes tested, zenny, flags 0x15, 0x17, 0x1A, 0x1B |
| `0x54C480` | Scena06_Run08 | 0x482 | run 8 | 22 steps (19 handlers): step 0 sorts the three ObjTrio records by +0x89 and resets the second party list; two event battles (`0x532ED0` + `Field_StartEventBattle` 0x19 / 0x1A), areas, requests 6 / 7, flag 0x1C |
| `0x54C910` | Scena06_Run09 | 0x364 | run 9 | 15 cases: area 0x5C and back, event battle 0x1B, flags 0x1D / 0x1F / 0x20, messages 0x2B / 0x29 |
| `0x54CC80` | Scena06_Run10 | 0x100 | run 10 | 6 cases: drop-ins 7 / 8, `0x587B80`, a stream, `Sound_ResumeAll`, flag 0x24 |
| `0x54CD80` | Scena06_Run11 | 0x178 | run 11 | 7 cases: messages 8 / 9 / 0xA by flags 0x27 / 0x25, flag 0x25 |
| `0x54CF00` | Scena06_Run12 | 0x3A4 | run 12 | 19 cases: LeaderEffect(0x91), event battles 0x17 / 0x18, `Party_AddToLists(2)` and the bytes `0x7DEE44` / `0x929F0F` / `0x929F10`, flags 0 / 1, calls by the selector, area 0x43 |
| `0x54D2B0` | Scena06_Run13 | 0x23C | run 13 | 9 cases: the map byte (0x5D, 0x2E), an effect of kind 0x45, flags 0x25..0x27, 0x2D and the story flag 0x2F, music 0x5B |
| `0x54D4F0` | Scena06_Run14 | 0x524 | run 14 | 14 cases: per-member lines, per-member effects of kind 6, LeaderEffect(0x93), MemberLines, event battle 0x1C, flags 0x28..0x2A |
| `0x54DA20` | Scena06_MemberLines | 0x6A | state (Run14's E8) | messages 0x16 / 0x15 / 0x14 for each party byte 2 / 1 / 5; request 2, step 0xC |
| `0x54DA90` | Scena06_Run15 | 0x43F | run 15 | 17 cases: messages, an effect of kind 0x4B, `Cond_ByteFE` 1, the story flag 7, a chain of kind-0x13 effects through counter 3, music 0x5C and a stream, flag 0x2C, `Inventory_Add(0, 0x57, 1)` |
| `0x54DED0` | Scena06_Run16 | 0x1EE | run 16 | 6 cases: character record 7's weapon and armour bytes + 1 given as items, their names to `Text_Records`, messages 0x94 / 0x95, flags 0x22 / 0x23 |
| `0x54E0C0` | Scena06_Run17 | 0x224 | run 17 | 10 cases: areas 0x2E / 0x3C / 0x57, two effects, flag 0x3D |
| `0x54E2F0` | Scena06_LeaderEffect | 0xB3 | entry (one word) | an effect of kind 6 on the lead's object (+6 = the word's byte + 0x70) |
| `0x54E3B0` | Scena06_PartyCalls | 0x8C | state (EnterArea's E8) | call B / A pairs by the selector 0, 2, 3, 4, 6 |
| `0x54E440` | Scena06_ObjectTrigger | 0x1F | slot 1, object | `Scena06_Objects[object +0x86]` (object, bits) |
| `0x54E460`..`0x54E6C0` | Scena06_Object01..13 | 0x11..0x3E | state (objects 1..13) | each starts a run from a step (runs 1, 2, 4, 6, 0xA, 0xE, 0x10; entry 3 once by flag 5, entry 5 flag 0xC only, 12 / 13 by flags 0x22 / 0x23) |
| `0x54E700` | Scena06_GuestRecord | 0x84 | state (Start's E8) | character record 7 from `Scena06_GuestStats` |
| `0x54E790` | Scena06_Leap | 0x39 | entry (7 words) -> al | the phase's handler through `Scena06_LeapPhases` |
| `0x54E7D0` | Scena06_LeapStart | 0x10A | entry -> al | the arc's set-up: steps, frames a step (16 / speed), per-frame x / z, the lift |
| `0x54E8E0` | Scena06_LeapAir | 0x110 | entry -> al | a frame of the arc: move, gravity, the ground under it, the animation at the top |
| `0x54E9F0` | Scena06_LeapLand | 0x89 | entry -> al | falls to the ground and ends (phase 0, al 0) |
| `0x54EA80` | Scena06_StepHook | 0x44B | slot 2, hook | areas 0x27 / 0x39 / 0x43 / 0x4D / 0x5E, by flags and rectangles: runs 2, 3, 5, 0xC, 0xE, 4, 0xA |
| `0x54EED0` | Scena06_CellHook | 0x2A | slot 4, hook | `0x56D800` over `Scena06_Cells`; negative 0xFF, else `Scena06_CellHandlers` |
| `0x54EF00` | Scena06_Cell0 | 0x72 | cell entry 0 -> al | run 0xB from step 0 or 5 by `Inventory_Count(1, 0x47, 0)` |
| `0x54EF80` | Scena06_Cell2 | 0x3E | cell entry 2 -> al | flag 0x27 clear: run 0xD |
| `0x54EFC0` | Scena06_Cell3 | 0xBB | cell entry 3 -> al | run 0xF from step 0, 5 or 0x1E by flag 0x27, the story flags 5 / 6 and flag 0x2C |

Every function's extent, jump tables and call count are in its
`symbols.toml` evidence line. What the runs are in the story is not read
here: the areas, flags, messages and battles are numbers, and a scene's
meaning is the owner's to say.

**The runs** are the shape [`scena_sc12.md`](scena_sc12.md) §3 describes: a
switch on the step byte `0x8034E5` (MSVC's two-level switch where the cases
are sparse), each case waiting on counter 0 at a value, the request byte
(not 2, or 0 after a request 7 / 6), the wait word, a stream, the kind-2
hold or an effect's in-use byte, then doing one thing and setting the next
step; a run ends with `ScriptFlags_Clear40` and the step and run 0 (some
with the counters). The effect records are `Effect_Objects` + slot x 0x80
(slot from `Effect_FindFree`, stored to `0x903850` before it is tested in
all but runs 13 and 15's one-byte spawns, which keep it local).

**The leap.** Area 77's handler (`0x40F090`) calls Scena06_Leap once a frame
with (the movement-script object, dx and dz as signed bytes from a pair at
`0x60BC18`, a lift 0x40, a gravity -0x400, an animation 4, a flag 0) and
steps its script when the answer is 0. Phase 0 sets the current sprite's
per-frame x / z steps from the object's speed (`Field_MoveSpeeds[object
+4]`: 16 / speed frames a step, the larger of |dx|, |dz| steps) and its
vertical velocity `+0x14` = lift << 8; phase 1 moves it a frame, adds the
gravity, keeps it above the ground (`AreaMap_Elevation` + 0x240) while
falling, sets the animation at the top of the arc, and at the end of its
steps calls `MoveCmd_OpDB` and goes to phase 2; phase 2 lets it fall to the
ground and answers 0. Why area 77 borrows chapter 6's code is not read here.

**As the originals have them** (each kept, and each said at its site in
`scena_sc6.cpp`): Run01's timer is decremented every frame of step 4,
wrapping to 0xFFFF past 0, and passes only at 0; Run07 steps 0x01, 0x1A
and 0x32 read `Field_StatusBits` before their other stores and write it
back after; Run11's step 0 stores step 1 before its flag test; Run12's
step 0xC falls through to step 0xD's test; Run13, Run15 and Cell3 hand
the flag calls the literal `0x904030` (the story flags) where the other
calls read the pointer at `0x929ED0`; Run16 reads record 7's
byte again for `Item_NamePtr` after `Inventory_Add`; Scena06_StepHook's
area 0x27 second rectangle starts run 3 and goes on to the next area's
test rather than answering; its area 0x5E answers 0 when flag 0x1E is clear
and the position misses (the flag 0x24 test is only for 0x1E set);
Scena06_LeapLand writes the height through the sprite pointer it read
before `AreaMap_Elevation` and the velocity through the one after;
Scena06_LeapStart writes the speed and the step count over two of its own
argument slots (Scena06_Leap's copies, unread by anyone). Where the original pushes a fourth word
0 to `Inventory_Add` (read by nobody), ours passes three.

## 4. The fuzz

`scena_sc6_fuzz.cpp`, through `scenario_harness` with `chapter = 6`:

- **The clones**: `tools/scenario_rows.py --unit SC6 --clones` at `3e410e7`,
  identical to this reading. Shapes: slot 0 `kSlot`; the states, runs,
  object handlers and the three helpers called by E8 without arguments
  `kState`; slot 1 `kObject`; the step and cell hooks and the three cell
  entries `kHook` (al compared); the leap, its phases and LeaderEffect
  `kEntry` (the leap's al compared: `ret_mask 0xFF`). No clone has more
  than 34 call sites; nothing `REFUSED`.
- **Callees**: the harness's standard set, plus the group's listing (which
  stands): `Flags_Test` as a `kFlag` (tested on al alone); the group's own
  called by E8 - `Scena06_GuestRecord`, `Scena06_PartyCalls`,
  `Scena06_MemberLines` as `kPhase` (they log the chapter bytes they run
  with), `Scena06_LeaderEffect` with its argument's byte; group SE's
  `Field_StartEventBattle` and `Party_AddToLists` by name (the standard set
  lists `0x4410B0` by address); `Inventory_Add` (the three low bytes, al a
  `kFlag`), `Inventory_Count` (a `kBool`: its u16 tested whole),
  `Item_NamePtr` (the two low bytes; its answer a 16-byte buffer of the
  fuzz's own filled from the recorders' stream); `MoveCmd_OpDB` (Capcom's);
  by address `0x533E50`, `0x56D800` (a byte 0xFF or 0..3), `0x591900`,
  `0x591BE0` and `0x587B80`; and the log slots of the typed stand-ins below.
  `Sprite_EnsureAnimation` is the standard one (its whole dword logged): ours
  passes the animation's dword as the original pushes it.
- **Tables**: `Scena06_States` and `Scena06_Runs` swapped for recorders
  (their entries take no arguments). `Scena06_Objects` (object, bits),
  `Scena06_CellHandlers` ((x, z), al) and `Scena06_LeapPhases` (seven
  words, al) pass what a table recorder does not log, so the seed writes a
  stand-in of the entry's own type into each entry, one per index (a wrong
  index is a different log), and the three tables are regions the harness
  puts back. Half the time `Scena06_CellHandlers[1]` is Capcom's bare ret
  instead, so that its answer (0x56D800's al) is compared.
- **Regions** beyond the harness's 22: the character records `0x903A70`
  (0x520: record 7 and the members' +0xB), `MoveScript_EffectState`
  (0x18), the selector `0x90412C`, the byte `0x904C9F`, `0x904CD0..0x904D0F`
  (the music byte and the two `Text_Records` names), `Cond_ByteFE`,
  `0x929F0C..` (the load bytes `0x929F0F` / `0x929F10` and
  `Field_Kind2Hold`), `0x7DEE44`, `0x939A00..0x939B0F` (run 8's copy of the
  party list), the three tables above, `Scena06_GuestStats` and
  `Scena06_MemberBytes` (Capcom's constant bytes, random here so that each
  byte's use is seen - control B2 stood until they were), and the leap's
  object. 37 regions, 11,794 bytes.
- **Seed**: `MoveScript_EffectState` 0..7 (every record inside the eight);
  the areas the band tests; counter 0 at the value a step waits on (the
  table `kWaits`, read off ours: every step that waits on counter 0, paired
  two times in three), counters 1..3 (counter 3 0..19 or 0x10..0x12 for
  run 6's pull), an effect's in-use byte, the request 0 / 2 / 7 / 6, the
  wait word 0, the hold and the load bytes 0, the selector 0..7 (bit 7
  half the time), the party bytes 0 / 1 / 2 / 5 / other, record 7's two
  equipment bytes 0 / 1 / 0xFF, ObjTrio +8 2..5, the timer 0..2; per role:
  the state 0..2, the run 0..17, EnterArea's areas with counter 2's cases,
  the object's +0x86 0..13, each run's case steps and one or two past (any
  step one time in eight), run 7's equipment bytes both set or not, run 8's
  three +0x89 among four values (ties included), run 12's `0x7DEE44`; the
  leap's object speed index 0..7 (1..5 for LeapAir: a 0 speed divides by
  zero on both sides, section 6), the sprites' phase, frame count, step
  count, velocity and height at their boundaries.
- **Arguments**: the object for slot 1; the step hook's (x, z) at each of its
  eight rectangles' bounds and one either side (two times in three); the leap
  functions' object, dx / dz at 0, 1, 0x7F, 0x80, 0xFF, the gravity, an
  animation of 0xFF half the time, the flag byte 0 half the time;
  LeaderEffect's 0x91 / 0x93 or anything.
- **Disturbance** beyond the harness's, from the hash given: counter 0 to a
  waited value, the request, the area, the selector, a party byte, one of
  record 7's equipment bytes, the hold or a load byte, an effect's in-use
  byte, `0x7DEE44`, counter 3. A `settle` (from `Noise()`): for EnterArea
  and the step hook, half the time the area moved to one they test (both
  read it again after calls); for run 16, half the time one of record 7's
  bytes moved (it reads it again after `Inventory_Add`; control B22 was
  refused in 1 round of 6,000 before this, 101 after).

**Result, in this worktree** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc6`,
exit 0): 288,000 rounds over 48 functions, 322,685 calls to the stand-ins,
**0 mismatches**, 11,794 bytes of state (37 regions) compared. Coverage, the
calls the originals made: `Flags_Test` 59,831, `ScriptFlags_Set40` 82,283,
`Flags_Set` 24,685, `Msg_OpenScript` 12,760, `AreaMap_Elevation` 14,886,
`Field_ChangeArea` 7,677, `Party_DropIn` 8,342, `Effect_FindFree` 9,727,
`ScriptFlags_Clear40` 11,388, `Scenario_CallA` / `CallB` ~4,000 each,
`Transition_Start` 3,233, `Kind2_Place` 2,959, `Sound_PlayEffect` 3,095,
`Flags_Clear` 2,838, `0x533E50` 2,566, `Music_Play` 2,048,
`Music_FadeOutStop` 1,795, `Sound_StreamDone` 1,656, `AreaMap_SetByte`
1,515, `Field_StartEventBattle` 1,111, `Inventory_Add` 1,067, `0x532ED0`
1,019, `Sound_LoadStream` 943, `Msg_OpenSystem` 633, `Item_NamePtr` 596,
`Sprite_EnsureAnimation` 464, `Sound_ResumeAll` 417, `MoveCmd_OpDB` 410,
`0x587B80` 400, `0x591BE0` 264, `0x591900` 210, `0x56D6F0` 206,
`Party_AddToLists` 136, `Inventory_Count` and `0x56D800` 6,000 each; the
three E8 helpers 6,000 / 359 / 357 and LeaderEffect 493; the object, cell
and leap stand-ins 6,000 / 4,197 / 12,000 (two log entries a call); and
every state and run table entry (Start, EnterArea, Run ~2,000 each, the
bare ret and Run01..Run17 ~290..370 each). `BOF3X_SHADOW='*'`: exit 0, every
group's self-test 0 mismatches.

## 5. Controls

`scratchpad/sc6/controls.py` (not committed): a batch of mutants planted in
`scena_sc6.cpp`, each on a unique anchor (the script refuses an anchor found
other than once), rebuilt, the self-test run, then restored and rebuilt.
**A batch holds at most one mutant per function**, and the harness counts
mismatching rounds per function, so each count is that mutant's alone: the
functions are fuzzed separately and a clone reaches the group's other
functions only through recorders. Batch A is one mutant in each of the 48,
B a second in 34, C a third in 9, D a fourth in EnterArea. All 92 refused by
a count in the final pass (the fuzz as committed); the rounds are of 6,000.

| Id | Function | Mutant | Result |
|---|---|---|---|
| A1 | Frame | state read as state + 1 (mod 3) | refused (6000 rounds) |
| A2 | Start | state 2 instead of 1 | refused (6000 rounds) |
| A3 | GuestRecord | ATK base +0x44 from byte 3 | refused (5974 rounds) |
| A4 | PartyCalls | selector 4: call A 2 | refused (762 rounds) |
| A5 | EnterArea | area 0x5E: Camera_Distance 0x481 | refused (36 rounds) |
| A6 | Run | run 5 read as 6 | refused (328 rounds) |
| A7 | Run01 | step 3: timer 0xB5 | refused (415 rounds) |
| A8 | Run02 | step 0x10: effect life 0x3D | refused (240 rounds) |
| A9 | Run03 | step 3: effect +0xC 0x701 | refused (197 rounds) |
| A10 | Run04 | step 2: MoveScript_F3Divisor 0x21 | refused (194 rounds) |
| A11 | Run05 | step 3: stream 1 | refused (355 rounds) |
| A12 | Run06 | step 6: 0x11 frames | refused (36 rounds) |
| A13 | Run07 | step 6: the equipment test inverted | refused (137 rounds) |
| A14 | Run08 | step 0: sort on >= | refused (84 rounds) |
| A15 | Run09 | Run09Enter: counter 2 = 5 | refused (409 rounds) |
| A16 | Run10 | step 2: counter 0 = 0xA | refused (417 rounds) |
| A17 | Run11 | step 0: messages 8 / 0xA swapped | refused (565 rounds) |
| A18 | LeaderEffect | +6 = arg + 0x71 | refused (5715 rounds) |
| A19 | Run12 | step 0xB: 0x929F10 = 2 | refused (269 rounds) |
| A20 | Run13 | step 3: 4 / 6 swapped | refused (513 rounds) |
| A21 | MemberLines | members 1 and 5 swapped | refused (2808 rounds) |
| A22 | Run14 | step 2: effect +6 = 4 | refused (68 rounds) |
| A23 | Run15 | step 0xC: effect x -0x32F | refused (194 rounds) |
| A24 | Run16 | step 1: none -> counter 0 0x21 | refused (156 rounds) |
| A25 | Run17 | step 4: effect z 0xF9 | refused (292 rounds) |
| A26 | ObjectTrigger | index 3 read as 4 | refused (430 rounds) |
| A27 | Object01 | run 2 | refused (6000 rounds) |
| A28 | Object02 | step 1 | refused (6000 rounds) |
| A29 | Object03 | step 0xB | refused (2051 rounds) |
| A30 | Object04 | flag 5 | refused (6000 rounds) |
| A31 | Object05 | flag 0xD | refused (6000 rounds) |
| A32 | Object06 | run 5 | refused (6000 rounds) |
| A33 | Object07 | step 0x15 | refused (6000 rounds) |
| A34 | Object08 | counter 0 = 2 | refused (6000 rounds) |
| A35 | Object09 | run 7 | refused (6000 rounds) |
| A36 | Object10 | run 0xB | refused (6000 rounds) |
| A37 | Object11 | run 0xF | refused (6000 rounds) |
| A38 | Object12 | clear: step 1 | refused (2039 rounds) |
| A39 | Object13 | clear: step 0xB | refused (2052 rounds) |
| A40 | Leap | phase + 1 (mod 3) | refused (6000 rounds) |
| A41 | LeapStart | sprite +9 = 1 | refused (3770 rounds) |
| A42 | LeapAir | the top test strict (< 0) | refused (71 rounds) |
| A43 | LeapLand | ground + 0x241 | refused (2886 rounds) |
| A44 | StepHook | area 0x27 first rectangle: counter 0 0xB | refused (36 rounds) |
| A45 | CellHook | bare ret: found ^ 1 | refused (592 rounds) |
| A46 | Cell0 | step 6 when items | refused (3928 rounds) |
| A47 | Cell2 | run 0xC | refused (1955 rounds) |
| A48 | Cell3 | step 0x1F | refused (1249 rounds) |
| B1 | Start | Scena06_GuestRecord not called | refused (6000 rounds) |
| B2 | GuestRecord | weapon/armour bytes: +0x15 from byte 6 | refused (5976 rounds) |
| B3 | PartyCalls | selector 6: call A 1 | refused (794 rounds) |
| B4 | EnterArea | area 0x57, other counter 2: counters kept | refused (570 rounds) |
| B5 | Run01 | step 4: the timer at 1 passes too | refused (90 rounds) |
| B6 | Run02 | step 0xA: lead 4 drops in 7 | refused (48 rounds) |
| B7 | Run03 | step 0xE: member record +0xB |= 2 | refused (121 rounds) |
| B8 | Run04 | step 0x1F: the counters cleared too | refused (246 rounds) |
| B9 | Run05 | step 2: step stored before the calls | refused (39 rounds) |
| B10 | Run06 | step 0x15: Kind2X from +0x38 | refused (191 rounds) |
| B11 | Run07 | step 0x14: step stored after the calls | refused (16 rounds) |
| B12 | Run08 | step 0: list reset byte 2 0xFE | refused (219 rounds) |
| B13 | Run09 | step 0x10: counter 0 0x15 | refused (227 rounds) |
| B14 | Run10 | step 2: Sound_ResumeAll dropped | refused (417 rounds) |
| B15 | Run11 | step 6: counter 0 kept on the end | refused (155 rounds) |
| B16 | LeaderEffect | party byte 0x904063 for the index | refused (5229 rounds) |
| B17 | Run12 | step 0xC: no fall-through to 0xD | refused (39 rounds) |
| B18 | Run13 | step 7: flag 0x2F of the row, not the story | refused (519 rounds) |
| B19 | MemberLines | step 0xD | refused (6000 rounds) |
| B20 | Run14 | step 2: members 0 and 1 | refused (95 rounds) |
| B21 | Run15 | Cond_ByteFE = 2 | refused (212 rounds) |
| B22 | Run16 | the name id not read again after Inventory_Add | refused (101 rounds) |
| B23 | Run17 | step 7: or instead of xor | refused (197 rounds) |
| B24 | ObjectTrigger | bits + 8 passed | refused (6000 rounds) |
| B25 | Object12 | counters cleared after Flags_Test | refused (282 rounds) |
| B26 | Leap | dx and dz swapped | refused (5794 rounds) |
| B27 | LeapStart | x division floored | refused (463 rounds) |
| B28 | LeapAir | Sprite_Current not read again after AreaMap_Elevation | refused (113 rounds) |
| B29 | LeapLand | height written to the new sprite | refused (96 rounds) |
| B30 | StepHook | area 0x27 second rectangle answers 1 at once | refused (12 rounds) |
| B31 | CellHook | none answers 0xFE | refused (1211 rounds) |
| B32 | Cell0 | Inventory_Count item 0x48 | refused (6000 rounds) |
| B33 | Cell2 | answer 2 | refused (6000 rounds) |
| B34 | Cell3 | story flag 7 for 6 | refused (2687 rounds) |
| C1 | EnterArea | area 0x2E, selector 6: call A 8 | refused (15 rounds) |
| C2 | Run06 | step 6: redraw 3 | refused (177 rounds) |
| C3 | Run07 | step 0x3C: 0x904C9F = 1 | refused (100 rounds) |
| C4 | Run08 | step 0: one byte fewer copied | refused (219 rounds) |
| C5 | Run09 | Run09Leave: flag 0x1D set | refused (510 rounds) |
| C6 | Run12 | step 0x11, selector 3: call B 5 | refused (23 rounds) |
| C7 | Run16 | step 1: 12 bytes of the name | refused (596 rounds) |
| C8 | LeapAir | frames 17 / speed | refused (166 rounds) |
| C9 | StepHook | area 0x5E: flag 0x1E clear falls through to 0x24 | refused (380 rounds) |
| D1 | EnterArea | area 0x10 dropped from the end list | refused (109 rounds) |

**Controls that stood on an earlier pass, and what fixed the fuzz:**

- **B2** (GuestRecord: the armour byte from table byte 6 instead of 7):
  `Scena06_GuestStats`' bytes 6 and 7 are equal in the exe, so with the
  table as shipped no input tells them apart - an equivalent mutant of the
  data as it stands. The fuzz now makes the table a region filled at random
  each round (ours reads it in place, as the original does), and B2 is
  refused in 5,976 rounds. `Scena06_MemberBytes` was made random with it.
- **B22** (run 16: the item id not read again after `Inventory_Add`):
  refused in 1 round - the harness's disturbance reaches record 7's bytes
  rarely. The run-16 `settle` above moves them half the time: 101 rounds.
- **A14** (run 8's sort on `>=`): 2 rounds with random +0x89 bytes (a tie
  is 1 in 256); run 8's seed now draws the three from four values: 84.

## 6. Cross-group calls, and latent defects

| Callee / caller | What | Owner | How |
|---|---|---|---|
| `0x533E50` | a pass over the eight records at `0x903A70` and the party | nobody (round10 §3) | raw (`SH_AT`) |
| `0x532ED0` | (x, z, kind): an event battle's party placement | nobody | raw |
| `0x56D6F0` | `Field_StatusBits` bit 7 set | nobody | raw |
| `0x56D800` | the cell-record search | nobody | raw |
| `0x591900` | (id): the id into the first free byte of the 32 at `0x904554`, al 1 / 0 | nobody (round10 §3) | raw |
| `0x591BE0` | (amount, flag): the zenny `0x904058` += amount, capped | nobody | raw |
| `0x587B80` | a jmp into the sound layer (`0x5A6FF0`) | nobody (round10 §3) | raw |
| `0x4410B0` `Field_StartEventBattle`, `0x591CC0` `Party_AddToLists` | | group SE (ours) | by name |
| `Scenario_CallB` `0x5341C0`, `MoveCmd_OpDB` `0x57CD40`, `Sound_ResumeAll` `0x587B90` | | named, Capcom's | by name |
| `Scena06_CallA` entries 0, 1, 2, 3, 6, 7, 8, 9 (`0x519BF0`, `0x519C20`, `0x519C50`, `0x519C80`, `0x519CC0`, `0x519CE0`, `0x519CC0`, `0x519D00`) and `Scena06_CallB` 0..7 (`0x519D30`, `0x519D50`, `0x519D70`, `0x519D90`, `0x519BD0`, `0x519DA0`, `0x519BC0`, `0x519DC0`) | the chapter's call-table entries | group CALLS (this wave) | by index through `Scenario_CallA` / `Scenario_CallB`, never by address; entries 4 and 5 of call table A are not reached from this band |
| area 77's handler `0x40F090` -> `Scena06_Leap` | E8 at `0x40F0E6` | group AR2A (a later wave) calls into this band | for the rebinding pass: that caller becomes a call by name |
| group SC7's band `0x54F080..0x553B30` | - | SC7 (this wave) | **no call either way** (section 1) |

**Latent defects** (Capcom's, described, kept; ours aborts where the
original would jump into data or fault):

- **Every dispatch table is indexed unchecked**: the state and run (signed
  bytes), the object's +0x86, the cell search's answer (0..0x7F), the
  sprite's leap phase. An index past a table jumps through the next table
  or data (section 2). No code of the band writes one; `MoveScript_Var7` is
  also written by the movement script (op F6), as SC12 notes.
- **Scena06_LeapStart indexes `Field_MoveSpeeds` (6 bytes) by the object's
  +4 unchecked.** Index 6 or 7 reads a 0 (the leap ends at once, al 0);
  index 8 reads 0x40, so 16 / speed frames a step is 0 and the division by
  frames x steps is **a divide by zero** (a fault in the original; ours
  aborts with a message). Area 77 passes the movement-script object, whose
  +4 the leap reads as the speed index.
- **Scena06_LeapAir divides 16 by the speed again at every new step**, with
  no test: a speed of 0 there (the object's +4 changed to 0, 6 or 7 during
  the leap) is a divide by zero; ours aborts.
- **`Scena06_MemberBytes` (8 bytes) is indexed by a party byte
  unchecked**: a member id above 7 reads `Scena06_Hooks`' first bytes.
- **Run 8's step 0 copies `Field_MemberCount` bytes** (a byte, unbounded)
  from the second party list `0x904065` to `0x939A10`: above 3 it copies the
  flag bytes that follow the list.
- **Run 16 gives record 7's equipment byte + 1**: a byte of 0xFF gives item
  0, which `Inventory_Add` refuses, and the run ends as if the inventory
  were full.
- The members' records (`CharacterRecords` + 0xA4 x
  `MoveScript_EffectState[k]`) are indexed unchecked, as SE found for
  `Party_AddToLists`.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count), D137 (divides by 0), D161
(small slips) in [`known-defects.md`](known-defects.md).

## 7. What nothing reached

No route was played (headless self-tests only, per the wave's brief), and
which recorded route, if any, reaches chapter 6 was not checked; the leap is
reached only through area 77. The live check per chapter is the recipe save the scenario
plan names (§5). The fuzz reached every standard and group callee the
originals call and every table entry; the four cell-search answers and its
0xFF, and both of the leap's landing branches. What it cannot show: what a
scene looks like (the recorded route's), and the real `Scenario_CallA` /
`CallB` entries (recorders here; group CALLS takes them).
