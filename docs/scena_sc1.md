# Scenario chapter 1's bank: the scenes, the object hook, the cell hook

**Status:** IN PROGRESS (2026-09-27) - stage A of two. Forty-two functions
written (`src/game/scena_sc1.cpp`, shadow name `scena_sc1`), read to the last
instruction, with their `symbols.toml` entries, the fuzz file against the
scenario harness's contract (`src/game/scena_sc1_fuzz.cpp`) and their
`entries_logic.txt` lines. **Not built, not registered, not fuzzed**: the
scenario harness (group SCH) had not landed. No divergence.

Group SC1 of round ten's first wave ([`takeover-queue-round10.md`](takeover-queue-round10.md) §1),
the band `0x539AD0..0x53DDA0`, on the plan of
[`takeover-queue-scenario.md`](takeover-queue-scenario.md). Every claim about
the binary is capstone over `bof3/BOF3.exe` (2026-09-27), by a scratch
recursive-descent lister built on `tools/magic_rows.py`, checked against
SCH's `tools/scenario_rows.py --unit SC1 --clones` (commit `222eb0f`, read
from a scratch copy, not merged).

## Stage A done - what stage B has to do

1. Merge `phase-3/capture-round-ten` at the harness's SHA. Check the fuzz
   file against the real `scenario_harness.h`: the stage A syntax check ran
   against a private copy of `magic_harness.h` with the names substituted
   and one addition the clone tool prints, `enum class Shape { ..., kSlot,
   kObject, kHook, ... }` with a `Clone::shape` field after `calm`. If SCH
   named the shapes differently, or gives the hooks their `(x, z)` itself,
   adjust the two rows that say a shape (`Scena01_ObjectHook`,
   `Scena01_CellHook`) and `Args` (section 4).
2. The fuzz file lists every callee (the group's listing stands over the
   standard set) and its own regions, some of which the harness may hold
   too; drop duplicates if SCH's `Run` refuses them. The two long `.data`
   tables are listed in parts of 16 (`magic_harness`'s limit); merge them if
   SCH raised it.
3. Register the module: `src/game/scena_sc1.cpp` and `scena_sc1_fuzz.cpp` at
   the end of `CMakeLists.txt`'s list, `ScenaSc1_Inject();` (and its header)
   at the end of `inject_all.cpp`.
4. Build; `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc1` to 0 mismatches,
   then `'*'`. Read the coverage line against section 4's expectations (every
   scene's steps, both answers of each hook, all 24 run entries and 18
   object entries through the swapped tables).
5. Plant and refuse the controls of section 5 with a script that plants,
   rebuilds, runs, restores and rebuilds; record each count "in this
   worktree".
6. Fill sections 4 and 5 with the counts; move the status to the result.

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

- `tools/scenario_rows.py --unit SC1` lists **`0x53B120` and `0x53D390`**:
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

## 4. The fuzz (written, not yet run)

`scena_sc1_fuzz.cpp`, one `scenario_harness::Group`:

- **Clones**: the 42, from `scenario_rows.py --unit SC1 --clones` less its
  two byte tables; call sites and jump tables as it printed them, checked
  against the reading. `ret_mask 0xFF` on `Scena01_CellHook` and
  `Scena01_Cell` (both answer in al). Call shapes: 21 state handlers (void),
  `Scena01_ObjectHook` (slot 1, one word), 17 object handlers (two words),
  `Scena01_PlaceEffect` (one word), `Scena01_CellHook` (slot 4, `(x, z)` ->
  al) and `Scena01_Cell` (a table entry tail-jumped with `(x, z)` -> al).
- **Callees**: all 32 listed. `Flags_Test`, `0x57C550` and
  `Sound_StreamDone` answer `kFlag`; `Effect_FindFree` a slot 0..0x13 or
  0xFF (`kByte` 0xFF..0x13); `0x56D800` 0, 1 or 0xFF.
- **Tables**: the four above, swapped for recorders (the two longer in parts
  of 16).
- **Regions**: the scenario bytes `0x8034E0..F3`; the camera distance, the
  counters and the effect slot; `Field_ScriptFlags`; `0x929EC0..1F` (member
  count, camera angles, the flag bank's pointer, the shop bytes,
  `Field_Kind2Hold`); `Effect_Objects` (20 records) and record 0xFF's first
  byte; the three members' records; `Sprite_Current`, `Field_State`,
  `Field_Kind2X` / `Z`, `MapView_Redraw`, `Draw_PassFlags`, `Input_Pressed`,
  `Field_Request`, `Game_Mode`, the wait word, the choice bytes, the member
  records `0x903A70` (8 of 0xA4) and the smaller cells each scene writes; an
  object of the fuzz's own.
- **Seed**: `Sprite_Current` / `Field_State` at a member's record, the
  effect slot 0..0x13 or 0xFF, `0x669730` 0..7; two in three counter 0 at
  one of the 45 values a step compares it with, counter 3 at the zoom and
  choice boundaries (0x13, 0x14, 0x19, 0x2D, 0x4F, bit 7), the message box,
  the wait word, `Field_Kind2Hold`, `Input_Pressed` 0 / 0x40, the member
  count 2 / 3, the timer at 0 / 1 / 0x6D / 0xB3, `Cond_ByteFD` 4, the choice
  bits; the leader's cell words and position at every bound
  `Scena01_Scene08` and `09` test; the step at one of the function's cases
  two in three; the run 0..23 for `Scena01_Run`, the object's +0x86 0..17.
- **Disturbance** (the group's, from its hash): the step, a counter, the
  message box, the wait word, `Sprite_Current`, `Field_Kind2Hold`, the effect
  slot, the member count. `settle` keeps the effect slot, `Sprite_Current`
  and the member index valid.

Counts: stage B.

## 5. Controls (stage B)

Planned, at least one per function, each a single substitution in
`scena_sc1.cpp`: a step's compare value moved by one (a counter, the timer
limit, a cell bound), a flag number, an area change's argument, a store moved
across the call after it (the step before `Field_ChangeArea` in
`Scena01_Scene05` step 0xA, `Cond_ByteFE` after `Party_DropIn` in
`Scena01_Scene08`), a re-read turned into a kept value (`Scena01_Scene0D`
steps 0xF / 0x12, `Scena01_Scene11` / `17` counter 0 after the transition),
the effect record's fields, the object handlers' +0x83 and step, the cell
handler's answer, and the three bounds checks (which the seed keeps inside,
so each is an equivalent mutant to record as such, with a near variant
planted instead).

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

## 7. Calls across groups (raw addresses, for the round's rebinding)

| Address | What | Owner |
|---|---|---|
| `0x4410B0` | one byte argument; `Scena01_Scene08` (2), `09` (3) | SE, this wave |
| `0x532ED0` | every member placed at (x, z) facing n | nobody |
| `0x533E50` | the members' records rebuilt (`Char_RecalcStats`, the copies into `ObjTrio`) | nobody |
| `0x5341C0` | the chapter's call table B, entry n (`Scenario_CallA`'s twin) | nobody (SCH names the call tables) |
| `0x57C550` | a test of two scaled words through `0x57C5A0`, al | nobody |
| `0x56D6F0` | `Field_StatusBits` \|= 0x80 | nobody |
| `0x56D800` | the index of the record matching (area, x, z) or 0xFF | nobody |

The rest are named and called by name (`Field_ChangeArea`, `Flags_*`,
`Music_*`, `Party_DropIn`, `Kind2_Place`, `Msg_OpenScript`, ...), ours or
Capcom's.

## 8. What nothing reached

Fuzz only, like every scenario group until a chapter-1 route exists
([`takeover-queue-scenario.md`](takeover-queue-scenario.md) §5). The object
handlers are reached only when a field object's trigger sets +0x86, and the
cell hook only on the two cells of `Scena01_Cells`; neither is on the
attract path. Chapter 1 is not on the attract path at all (chapter 16 is),
so the frame hash is untouched by this group.
