# Scenario chapter 2's bank: 0x53DDA0..0x5428C0

**Status:** IN PROGRESS (2026-09-27) - seventy-two functions ours
(`src/game/scena_sc2.cpp`, shadow name `scena_sc2`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 432,000 rounds (6,000 a function); 234 of 235 negative
controls refused by a count (exit 3), the other an equivalent mutant whose
near variant is refused (section 5).
`BOF3X_SHADOW='*'` exits 0 (4,173 injects in this worktree). Fuzz only: no
recorded route plays chapter 2 (section 8). No divergence.

Group SC2 of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §9): the plan's
SC2a and SC2b ([`takeover-queue-scenario.md`](takeover-queue-scenario.md)
§3) as one group, one stage, against the harness as merged at `e4882fb`.
Every claim about the binary is capstone over `bof3/BOF3.exe`
(2026-09-27), by a scratch recursive-descent lister (every jump table
bounded at its `cmp`, every byte table at its bound), checked row by row
against `tools/scenario_rows.py --unit SC2a --clones` and `--unit SC2b
--clones`.

## 1. What is in the band

`tools/scenario_roots.py --chapter 2`: chapter 2's vtable `Scena02_Hooks`
`0x660E50` holds the frame `0x53DDA0`, the object hook `0x541D10`, the step
hook `0x5420C0`, `Scenario_NoHook` `0x539AC0` in slot 3 (ours since SCH)
and no cell hook. Its call tables are CALLS' (ours since wave two):
`Scena02_CallA` `0x65F678` holds `ScenaCall_Join4` `0x519990`,
`Scena02_CallB` `0x65F67C` holds `ScenaCall_Leave4` `0x5199A0` and
`Scena02_Leave43` `0x5199B0`; this band reaches them only through
`Scenario_CallA` / `Scenario_CallB`. The closure is 81 functions walked,
19,552 bytes, frontier 37. The sibling's `names/scenario_records.toml`
pairs the vtable with SCENA02's at `0x801FE274` (slot 0 `0x801F719C`, 1
`0x801FD084`, 2 `0x801FD780`; its slot 3 is `0x801FD6F4` where the PC's is
`Scenario_NoHook`) - hypotheses by position, not read here.

**The starts.** The two tool units list 71 starts in the band, none ours.
This reading takes **72**:

- **One added, `0x5413D0`** (`Scena02_Scene08Next`, 0x2E bytes): a real
  function, the shared tail of `Scena02_Scene08` steps 9..0xB (three `jmp`s
  to it). The tool dropped it: `drop_byte_tables` could not bound
  `Scena02_Scene1B`'s byte table (its `cmp` is against `ecx`, loaded with
  0x19 before), so it dropped every start within 0x100 bytes of the table
  and ran `0x5411E0`'s extent to the next start (0x220 instead of 0x1EE).
  Section 1.2.
- **`0x541AB0`, the start the walk did not reach** (`Scena02_StartRun07`,
  0x25 bytes): the twelve near helpers' shared tail, reached only by their
  tail `jmp`s, which the walk does not follow into a function it has not
  listed. The tool lists it (its "new start" rule), and it is chapter 2's.

Every other row agrees with the reading, extent for extent, except
`0x53F4D0` (`Scena02_Scene0B`): the tool's 0x230 runs over the padding after
its byte table; the function and table end at `0x53F6F0` (0x221), which is
the clone's size here.

| Entry | Name | Bytes | Call shape | Does |
|---|---|--:|---|---|
| `0x53DDA0` | `Scena02_Frame` | 0xE | vtable slot 0 | `jmp [Scena02_States + s8 state * 4]` |
| `0x53DDB0` | `Scena02_Start` | 0x14 | state 0 | state 1; chapter 2's flag row dword `0x903FA0` and the four counters cleared |
| `0x53DDD0` | `Scena02_EnterArea` | 0x5FC | state 1 | by area: 0 the pass flags, flag 3; 3 call B(1), members 3 and 4 stripped, run 0x15, music 0xF; 5 by counter 2 (a jump table of 5); 0xB run 0x13; 0xD run 0x12; 0xF flag 0 and run 2; 0x12/0x16/0x17/0x1D/0x2D all cleared; 0x1A, 0x1B, 0x1C by counter 2; 0x5A music 0x4D; 0x10 music 0x11; state 2 |
| `0x53E3D0` | `Scena02_Run` | 0xE | state 2 | `jmp [Scena02_Runs + s8 MoveScript_Var7 * 4]` |
| `0x53E3E0` | `Scena02_Scene00` | 0x187 | run 0 | area 0x1A without flag 0x3F: the near helpers on sprites 0..9 by flags, then a tail `jmp` to a door helper |
| `0x53E570` | `Scena02_Scene02` | 0x314 | run 2 | area 0xF; flag 4; members 1 and 2, flags 5 and 6, area 0x1A; the choice 0x64 / 0x65; the stream; music 0x11, message 0xA |
| `0x53E890` | `Scena02_Scene04` | 0x16C | run 4 | run 2's first steps again at another place |
| `0x53EA00` | `Scena02_Scene05` | 0x45 | run 5 | message 0x3E |
| `0x53EA50` | `Scena02_Scene06` | 0x194 | run 6 | message 0x30 and its choice; area 0xF; the flags reset under flag 7 |
| `0x53EBF0` | `Scena02_Scene07` | 0x17C | run 7 | member 6, transition 0xD, message 0x16, area 0xF, the reset |
| `0x53ED70` | `Scena02_Scene08` | 0x3A4 | run 8 | Kind2, the party placed and event battle 0xE; member 0x12, area 0x1A; message 0x9A; step 8's three cells of the leader's (flags 9..0xB) and its end on counter 2 = 3 |
| `0x5413D0` | `Scena02_Scene08Next` | 0x2E | `Scena02_Scene08` steps 9..0xB's tail | counter 2 = 3: `ScriptFlags_Set40`; else back to step 8 with counter 2 + 1 |
| `0x53F120` | `Scena02_Scene09` | 0x21C | run 9 | member 0x18; two effects at the camera, the second's end waited for; member 0xA; music 0x20; call B(0), area 0x1B; flag 0x3F |
| `0x53F340` | `Scena02_Scene0A` | 0x18C | run 0xA | members 3, 4, 0x12; area 0x1B; the party placed and event battle 8; flags 0xF, 0x10 |
| `0x53F4D0` | `Scena02_Scene0B` | 0x221 | run 0xB | the choice 0x64 / 0x65; the shop (`Game_Mode` 7) on 0xC8; the stream; area 5 by the member count, music 7 |
| `0x53F700` | `Scena02_Scene0C` | 0xF4 | run 0xC | members 0x1A, 5, 0x15; event battle 9; flag 0x11 |
| `0x53F800` | `Scena02_Scene0D` | 0x1B8 | run 0xD | members 6, 0x16; two effects; event battle 0xA; flags 0x12, 0x13 |
| `0x53F9C0` | `Scena02_Scene0E` | 0x240 | run 0xE | members by the first member (`0x904062`); sound 0x204; event battle 0xB; two changes to area 0x1B; flags 0x17, 0x14 |
| `0x53FC00` | `Scena02_Scene0F` | 0x22C | run 0xF | transition 0xD; the stream; area 0x1B by the member count and first member, music 0x21; flag 0x18, member 0x11; the choice bit |
| `0x53FE30` | `Scena02_Scene10` | 0x2C3 | run 0x10 | four changes to area 0x1C; flags 0x15, 1; call A(0); two camera turns tested through `0x57C600` |
| `0x540100` | `Scena02_Scene11` | 0x408 | run 0x11 | messages 0x14, 0x29, 0x38; the stream 1; the camera shaken; event battle 0xC; flag 0x36; areas 0 and 5 |
| `0x540510` | `Scena02_Scene12` | 0x168 | run 0x12 | flag 0x1A, member 2; area 0xD; flags 0x1C, 0x38, 0x1B; music 0x10 |
| `0x540680` | `Scena02_Scene13` | 0x250 | run 0x13 | area 0xB; two effects; member 1; music 0xD; event battle 0xD; flag 0x1E, area 3 |
| `0x5408D0` | `Scena02_Scene15` | 0x158 | run 0x15 | music 0xF; the party restored, flag 0x1F; flag 0x20 and `Field_StatusBits` bit 7 |
| `0x540A30` | `Scena02_Scene17` | 0x40 | run 0x17 | `0x591BE0(0x32, 0)`, flag 0x23, sound 0x106 |
| `0x540A70` | `Scena02_Scene18` | 0x2D1 | run 0x18 | message 0x65; the payment `0x591BC0(0x32, 0)` on counter 0 = 8, messages 0x6D / 0x6C; flags 0x24, 0x25 or 0x3A |
| `0x540D50` | `Scena02_Scene19` | 0x14E | run 0x19 | flag 0x27 and `0x591B60(4, 2, 1)`; flag 0x26 and `Inventory_Add(4, 2, 1)`; message 0x73; the effect 0x91 |
| `0x540EA0` | `Scena02_Scene1A` | 0x334 | run 0x1A | sound 0x20B and 0x4B frames; members by the first member and by the leader's +8; effects 0x92, 0x99; event battle 0xF; flags 0x2F, 0x30, 0x32 |
| `0x5411E0` | `Scena02_Scene1B` | 0x1EE | run 0x1B | message 0x6F; the choice 0x5A..0x5D; the shop; the stream; area 0x1B |
| `0x541400`..`0x541840` | `Scena02_Near01`..`0C` | 0x5A..0x62 | `(n)`, from `Scena02_Scene00` | with `Cond_ByteFD` 0, the leader's (x, z) inside a rectangle around sprite n's: a tail `jmp` to `Scena02_StartRun07` |
| `0x5418A0` | `Scena02_DoorStart` | 0x11D | tail `jmp` from `Scena02_Scene00` | three cells of the leader's (first member 0 or 3): run 0x1A with counter 0 = 0x23 |
| `0x5419C0` | `Scena02_DoorSound` | 0xE1 | tail `jmp` from `Scena02_Scene00` | the same cells: sound 0x20B, run 0x1A at step 0x1E |
| `0x541AB0` | `Scena02_StartRun07` | 0x25 | the near helpers' tail | counter 0 = 0x1F, run 7 at step 0 |
| `0x541AE0` | `Scena02_Shake` | 0xAE | `(n)`, from `Scena02_Scene11` | `Camera_ShiftY` = n + `Camera_ShiftX` for two frames, the phase in counter 1 |
| `0x541B90` | `Scena02_PlaceEffect70` | 0xB3 | `(n)` | `Scena01_PlaceEffect` compiled again: a kind-6 effect at the leader, +6 = n + 0x70 |
| `0x541C50` | `Scena02_PlaceEffect68` | 0xB3 | `(n)` | the same as kind 0x19, +6 = n + 0x68 |
| `0x541D10` | `Scena02_ObjectHook` | 0x1D | vtable slot 1: `(object)` | `Scena02_ObjectHandlers[object +0x86](object, 0x903FA0)` |
| `0x541D30`..`0x542060` | `Scena02_Object01`..`1A` (22) | 0x8..0x3C | object handlers `(object, row)` | a run at a step (with `ScriptFlags_Set40`), a counter or a flag |
| `0x542080` | `Scena02_StripMember` | 0x3C | `(member)`, from `Scena02_EnterArea` | the ten bytes at +0x7E of the member's record handed to `0x590C90(b, 0, 1, 0)` and cleared |
| `0x5420C0` | `Scena02_StepHook` | 0x7F3 | vtable slot 2: `(x, z)` -> al | areas 3, 0xF, 0x1A, 0x1B, 0x1C: rectangles that start a scene, 1; else 0 |

Bytes are each body to its last byte, jump tables and byte tables
included. What each scene *is* in the story was not read: the numbers are
areas, messages, flags, members and music tracks, not places or characters.
A scene's shape is [`field-modes.md`](field-modes.md)'s and chapter 1's
([`scena_sc1.md`](scena_sc1.md) §1).

### 1.1 What the walk lists that is not a start

- **Cases**: `0x53DF10` (`Scena02_EnterArea`'s jump table, case 0),
  `0x53E5B0` and `0x53E600` (`Scena02_Scene02`'s byte-table cases 1 and 3).
  `analysis/pairs_propagated.json` pairs `0x53DF10` with two PSX addresses
  ("callers", `0x801D509C` and `0x801F7450`): both wrong, it is a case. Its
  pair of `0x53DDD0` with `0x801F71FC` is a plausible hypothesis (state 1).
- **`0x557170`**: `Scena02_ObjectHandlers` entry 0xA, in group SC9b's block
  (this wave). Reached only through the table; the fuzz stands a typed
  stand-in in for it and ours calls through the table as the original does.
  Not taken.
- **`0x558000`**: a coordinate (`cmp esi, 0x558000` in `Scena02_StepHook`'s
  area 3 rectangle) that happens to be a `.text` address, as chapter 1's
  was ([`scena_sc1.md`](scena_sc1.md) §1.2).

### 1.2 The byte tables (the brief's three addresses)

The tool dropped three starts as byte-table starts; the reading places
them:

| Tool's start | What it is |
|---|---|
| `0x53F6E0` | inside `Scena02_Scene0BSteps` `0x53F6D8` (25 bytes, read by `Scena02_Scene0B`: `movzx edx, byte [step + 0x53F6D8]`, bound `cmp eax, ecx` with `ecx` 0x18) |
| `0x5413C0` | inside `Scena02_Scene1BSteps` `0x5413B4` (26 bytes, read by `Scena02_Scene1B`, bound 0x19 through `ecx`) |
| `0x5413D0` | **code**: `Scena02_Scene08Next`, taken above - the table ends at `0x5413CD`, two `nop`s, then the function |

Both tables are `[[data]]` entries under their true starts. The band's
other five byte tables (`0x53E860` of `Scena02_Scene02`, `0x5400DC` of
`Scena02_Scene10`, `0x540D2C` of `Scena02_Scene18`, `0x540E8C` of
`Scena02_Scene19`, `0x541194` of `Scena02_Scene1A`) are bounded by an
immediate `cmp` and the tool read them right; they are counted in their
functions' extents and not named.

**Owed to the tool** (not fixed here): `magic_rows._cmp_bound` reads only
`cmp r, imm`; a `cmp r, r2` after `mov r2, imm` leaves the byte table
unbounded and `drop_byte_tables` drops real code within 0x100 bytes.

## 2. The tables named

`symbols.toml` `[[data]]`:

| Table | Count | Read by | Entries |
|---|--:|---|---|
| `Scena02_EffectX` `0x660E48` | 8 s8 | the place helpers, by the byte `0x904062` | a member's s8 into the effect's +0x10 (chapter 1's `0x660D60` twin) |
| `Scena02_States` `0x660E64` | 3 | `Scena02_Frame` | `Scena02_Start`, `Scena02_EnterArea`, `Scena02_Run` |
| `Scena02_Runs` `0x660E70` | 28 | `Scena02_Run` | `0x437CC0` (a bare ret) at runs 1, 3, 0x14, 0x16; the 24 scenes at the others |
| `Scena02_ObjectHandlers` `0x660EE0` | 27 | `Scena02_ObjectHook` | `0x437CC0` at 0, 3, 0x12; `0x557170` (SC9b's block) at 0xA; `0x541E00` at 6 and 8; `Scena02_Object01`..`1A` |
| `Scena02_Scene0BSteps` `0x53F6D8` | 25 bytes | `Scena02_Scene0B` | the step's case index |
| `Scena02_Scene1BSteps` `0x5413B4` | 26 bytes | `Scena02_Scene1B` | the step's case index |

The states and runs tables sit back to back after the vtable; the object
table runs to a 0 dword before `Scena03_Hooks` `0x660F50`.

## 3. How ours is written

Every call out is `SH_CALL(name)` or, for a callee nobody owns,
`SH_AT(type, address)` with the address in `scena_sc2_callees.h`. The three
dispatchers read their tables in place and call the entry; each **aborts
past its table** where the original would jump through whatever lies
beyond (the spell round's rule; no ledger entry): `Scena02_Frame` past 3,
`Scena02_Run` past 28, `Scena02_ObjectHook` past 27.

Every store is where the original makes it relative to the calls around it
(the counters cleared before `ScriptFlags_Clear40` in some ends and after it
in others: `EndKeep0` and `ClearThenEnd`), and every byte the original reads
again after a call is read again (the leader's cell x in the door helpers,
the first member after `Flags_Set` in `Scena02_Scene0E`, `Sprite_Current`
in the place helpers). Where the original stores `al` that a test has just
shown is 0 (`Scena02_Scene10` step 0x12's counter 3, three stores in
`Scena02_StepHook`), ours writes the constant and says so. The step hook's
area 0x1A tests after its two area changes are one helper, `StepHook1A`,
whose sentinel 0xFE means "on to area 0x1B", as the original's `jmp` to
`0x542633` does; the two routes into the `0x250000..0x278000` rectangle
(through `x` 0x350000 / 0x358000's `z` compares, or not) come to the same
tests and are written once.

## 4. The fuzz

`scena_sc2_fuzz.cpp`, one `scenario_harness::Group` with `chapter = 2`:

- **Clones**: the 71 rows of the two units' clone tables (at `e4882fb`,
  against the symbols before this group) with `Scena02_Scene1B`'s extent
  0x1EE and `Scena02_Scene0B`'s 0x221, and `Scena02_Scene08Next` added (its
  two sites by hand: `jmp ScriptFlags_Set40` at +9, `call
  ScriptFlags_Clear40` at +0xE). No clone has more than 46 call sites (the
  harness's limit is 64). Shapes: `kSlot` the frame; `kState` the start, the
  area entry, the run dispatcher, the scenes, `Scena02_Scene08Next`, the
  door helpers and `Scena02_StartRun07`; `kObject` the object hook; `kEntry`
  the near helpers, the shake, the place helpers, `Scena02_StripMember` and
  the 22 object handlers; `kHook` with `ret_mask 0xFF` the step hook.
- **Callees**: the harness's standard set, and the group's listing over
  it: `Flags_Test` as a `kFlag` (the bank tests `al` alone); the standard
  busiest callees with their masks and an `effect`, `Move`, that half the
  time moves one of the chapter's cells (the group's `Disturb`);
  `0x590C90` with an effect that notes the eight member records;
  `Field_StartEventBattle` by name (SE's); the raw callees `0x533E50`,
  `0x587B80`, `0x590C90`, `0x591B60`, `0x591BC0` (`kFlag`), `0x591BE0`,
  `0x57C600` (`kFlag`); the bank's own called directly - the thirteen
  helpers with an argument by it, the door helpers, `Scena02_StartRun07`
  and `Scena02_Scene08Next` as phases.
- **Handler tables**: `Scena02_States` (3) and `Scena02_Runs` (28) are
  swapped for handler recorders. `Scena02_ObjectHandlers` (27) holds
  handlers that take arguments: each distinct entry but the bare ret (23,
  `0x557170` among them) is listed as a callee with a typed stand-in,
  `ObjectEntry<i>`, which logs the object and the row.
- **Regions** beyond the 22 standard ones: `Camera_ShiftX` / `Y`, the shop
  bytes and `Field_Kind2Hold` (`0x929F00`, 0x14), `Game_Mode`, the choice
  bits `0x7DEE44`, `0x904CD0`, the word `0x802290`, and the eight member
  records `0x903A70` (7 regions, 29 in all, 11,341 bytes).
- **Seed**: the state 0..2, each of the four sprite records' +0x86 an object
  handler index; the area at every value the bank tests (half the time);
  counter 0 at the 62 values a step compares it with, counters 1..3 at
  theirs; the message box, the wait word, `Field_Kind2Hold`, the member
  count, the first member (0, 3, 4, 1), `Cond_ByteFD`, the choice bit,
  `Field_StatusBits` bit 0, the word `0x802290`, the byte `0x802FC3`; the
  leader's cell words, +0x4B and +8 at the door and `Scena02_Scene08`
  cells. Per scene, a third of the rounds a (step, counter 0) pair one of
  its steps waits for, a third a step of its cases, a third any byte; the
  counter-3 waits of `Scena02_Scene08`, `10` and `1A`; `Scena02_Scene09`
  step 3's effect record, its first byte 0 or 1. For the step hook two
  rounds in three one of 19 rectangles: its area, and (in `Args`) `x` and
  `z` at its bounds and one past them. For a near helper, the sprite 0..9
  and the leader at one of its rectangle's four bounds or one past.
- **Disturbance** (the group's, from its hash only): counter 0 (to a waited
  value half the time), any counter, counter 3 and 2 to their waited values,
  `Field_Kind2Hold`, the member count, the choice bit, the first member, the
  leader's cell (a whole door cell half the time, and for the door helpers
  half of every disturbance: they read its x again after each call), the
  step. The first build drew three of its
  choices through `SH_PICK` - the harness's `Next()` - and made 13,600 false
  mismatches, SC3's trap again; every choice is from the hash now.

**Result** (2026-09-27, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc2`,
exit 0, in this worktree): 432,000 rounds, 410,506 calls to the stand-ins
(the counts move with the build directory), **0 mismatches**. Coverage, the
originals' side: every one of the 28 runs' handlers (180..246 each; the bare
ret 1,486), the 3 states (1,944..2,068), the 23 object handler stand-ins
(199..460), and every callee the bank calls: `ScriptFlags_Set40` 104,524,
`Flags_Test` 63,050, `0x590C90` 59,781, `Flags_Set` 31,878,
`ScriptFlags_Clear40` 24,238, `Sound_PlayEffect` 17,121, `Effect_FindFree`
13,485, `Scena02_StartRun07` 12,584, `Party_DropIn` 8,027,
`Field_ChangeArea` 7,250, `Flags_Clear` 5,544, `Transition_Start` 4,833,
`Msg_OpenScript` 4,115, `Kind2_Place` 2,838, `Field_StartEventBattle`
2,487, `0x532ED0` 2,470, `Music_FadeOutStop` 2,120, `0x591BE0` 2,054,
`Music_Play` 1,666, `0x533E50` 1,442, the near helpers 462..1,379 each,
`Scena02_Scene08Next` 1,172, `0x591BC0` 936, `Scena02_DoorSound` 917,
`Sound_LoadStream` 846, `0x56D6F0` 677, `Sound_StreamDone` 585,
`Scena02_DoorStart` 462, `Scena02_PlaceEffect70` 362, `Scenario_CallB` 346,
`File_LoadDone` 329, `Inventory_Add` 277, `Scena02_Shake` 260, `0x591B60`
243, `Music_LoadFile` 219, `0x57C600` 184, `Scena02_StripMember` 138,
`0x587B80` 130, `Sound_ResumeAll` 113, `Task_Sleep` 110, `Scenario_CallA`
99, `Scena02_PlaceEffect68` 74, `MoveCmd_TestFB` 47.

## 5. Controls

A scratch script (`controls.py`, not committed) plants each change alone by
a text substitution in `scena_sc2.cpp`, scoped to one function's body and
unique there, rebuilds, runs the self-test, restores, and rebuilds at the
end. The count is the rounds of the function that mismatched (of 6,000), in
this worktree; every refused control ended with exit 3 by comparison, none
by a fault or a hang.

**235 controls, 234 refused.** The one not refused is an equivalent mutant:

- **AE5** (`Scena02_DoorStart`: the leader on x 0x46 failing its other
  tests goes on instead of returning): the only test after it wants x 0x44,
  and nothing between re-reads x, so no input tells them apart. Its near
  variant **AE5b** (going on with x read again, less 2) is refused in 6.

Ten of the first run's controls stood (E8, I6, I9, the door helpers'
AE1..AE4, AF1, AF4, and AK3; M1 first missed its anchor) and were refused by
strengthening the fuzz, not the plants: the area entry's area drawn evenly
over the ones it tests, with `Cond_ByteFD` at 0 / 1 / 2; `Scena02_Scene08`
step 8 seeded on its three cells at the `z` bound and one past; the door
helpers seeded on the door cells with one field off a third of the time,
and their disturbance putting the leader on a door cell half the time (they
read the cell's x again after each call); `0x590C90`'s stand-in made louder
than a plain recorder - it notes the eight member records, so
`Scena02_StripMember`'s clear moved before the call (AK3) shows. Q1 ended
once with the launcher's exit -1 (no self-test line; a transient of the
run) and was refused on its re-run (153).

The thinnest refusals, where one boundary meets one disturbance: AM1 (1
round), AM8 (2), AM7 (3), AL4 and AL7 (4) - the step hook's tests behind
three to five flag answers - and E8 (4), area 0xD's `Cond_ByteFD` 1. A later seed change can drop them;
re-run them after any.

| Id | Function | Plant | Rounds |
|---|---|---|--:|
| F1 | `Scena02_Frame` | state 1 read as 2 | Frame 1944 |
| S1 | `Scena02_Start` | state 2 | Start 6000 |
| S2 | `Scena02_Start` | row dword +4 | Start 6000 |
| E1 | `EnterAreaBody` | area 0 flag 0x36 | EnterArea 341 |
| E2 | `EnterAreaBody` | strip member 5 | EnterArea 69 |
| E3 | `EnterAreaBody` | run 0x16 | EnterArea 60 |
| E4 | `EnterAreaBody` | music 0xE | EnterArea 67 |
| E5 | `EnterAreaBody` | script xor 6 | EnterArea 43 |
| E6 | `EnterAreaBody` | area 5 step 4 | EnterArea 33 |
| E7 | `EnterAreaBody` | area 0xB step 0xB | EnterArea 106 |
| E8 | `EnterAreaBody` | Cond_ByteFD 2 | EnterArea 4 |
| E9 | `EnterAreaBody` | status | 3 | EnterArea 61 |
| E10 | `EnterAreaBody` | area 0x2C | EnterArea 343 |
| E11 | `EnterAreaBody` | area 0x1A flag 0xD | EnterArea 51 |
| E12 | `EnterAreaBody` | TestFB swapped | EnterArea 47 |
| E13 | `EnterAreaBody` | music play 0x4C | EnterArea 63 |
| E14 | `EnterAreaBody` | extra word 0x5B | EnterArea 89 |
| E15 | `EnterAreaBody` | area 0x1B case 4 counter 1 | EnterArea 31 |
| E16 | `EnterAreaBody` | area 0x10 status read after | EnterArea 178 |
| E17 | `Scena02_EnterArea` | state 1 | EnterArea 6000 |
| R1 | `Scena02_Run` | run ^ 1 | Run 6000 |
| A1 | `Scena02_Scene00` | near02 on 1 | Scene00 895 |
| A2 | `Scena02_Scene00` | near08 for 09 | Scene00 1379 |
| A3 | `Scena02_Scene00` | or -> and | Scene00 1379 |
| A4 | `Scena02_Scene00` | flag 0x2E inverted | Scene00 1379 |
| A5 | `Scena02_Scene00` | flag 0x35 | Scene00 916 |
| B1 | `Scena02_Scene02` | script or 4 | Scene02 169 |
| B2 | `Scena02_Scene02` | xor 0xA0 | Scene02 347 |
| B3 | `Scena02_Scene02` | member count 2 | Scene02 136 |
| B4 | `Scena02_Scene02` | counter 0 cleared after the transition | Scene02 27 |
| B5 | `Scena02_Scene02` | pass flags after the stream test | Scene02 27 |
| B6 | `Scena02_Scene02` | message 0xB | Scene02 88 |
| B7 | `Scena02_Scene02` | member 2 at step 5 | Scene02 130 |
| C1 | `Scena02_Scene04` | area x | Scene04 257 |
| C2 | `Scena02_Scene04` | frame byte 2 | Scene04 634 |
| C3 | `Scena02_Scene04` | step 9 | Scene04 246 |
| D1 | `Scena02_Scene05` | message 0x3F | Scene05 1308 |
| D2 | `Scena02_Scene05` | request 1 | Scene05 586 |
| G1 | `Scena02_Scene06` | choice step 3 | Scene06 771 |
| G2 | `Scena02_Scene06` | area byte 0x12 | Scene06 311 |
| G3 | `Scena02_Scene06` | counter 1 = 2 | Scene06 183 |
| G4 | `ResetFlags7` | flag 0x34 cleared | Scene06 185, Scene07 240 |
| G5 | `Scena02_Scene06` | flag 0x25 | Scene06 278 |
| H1 | `Scena02_Scene07` | counter 2 after the drop-in | Scene07 24 |
| H2 | `Scena02_Scene07` | message 0x17 | Scene07 218 |
| H3 | `Scena02_Scene07` | area flags 0x87 | Scene07 271 |
| I1 | `Scena02_Scene08` | place kind 0xD | Scene08 80 |
| I2 | `Scena02_Scene08` | battle 0xF | Scene08 89 |
| I3 | `Scena02_Scene08` | counter 2 = 2 | Scene08 57 |
| I4 | `Scena02_Scene08` | run 7 | Scene08 62 |
| I5 | `Scena02_Scene08` | lead member 1 | Scene08 2594 |
| I6 | `Scena02_Scene08` | second cell z 7 | Scene08 76 |
| I7 | `Scena02_Scene08` | counter 3 = 7 | Scene08 398 |
| I8 | `Scena02_Scene08` | counter 0 kept | Scene08 553 |
| I9 | `Scene08Cell` | z bound strict | Scene08 187 |
| I10 | `Scene08Cell` | +8 = 2 | Scene08 288 |
| J1 | `Scena02_Scene08Next` | counter 2 = 4 | Scene08Next 1517 |
| J2 | `Scena02_Scene08Next` | counter 2 + 2 | Scene08Next 4483 |
| J3 | `Scena02_Scene08Next` | step 9 | Scene08Next 4483 |
| K1 | `Scena02_Scene09` | far 0x2C7 | Scene09 117 |
| K2 | `Scena02_Scene09` | counter 3 = 0 | Scene09 251 |
| K3 | `Scena02_Scene09` | angle 2 | Scene09 261 |
| K4 | `Scena02_Scene09` | record stride 0x40 | Scene09 1381 |
| K5 | `Scena02_Scene09` | music 0x21 | Scene09 235 |
| K6 | `Scena02_Scene09` | call B 1 | Scene09 277 |
| K7 | `Scena02_Scene09` | counter 2 = 2 | Scene09 118 |
| L1 | `Scena02_Scene0A` | counter 2 = 5 | Scene0A 310 |
| L2 | `Scena02_Scene0A` | flag 0xE | Scene0A 282 |
| L3 | `Scena02_Scene0A` | place x z swapped | Scene0A 312 |
| L4 | `Scena02_Scene0A` | battle 9 | Scene0A 277 |
| M1 | `Scene0BChoice` | shop step 4 | Scene0B 386 |
| M2 | `Scene0BChoiceEnd` | counter 3 for 2 | Scene0B 625 |
| M3 | `Scene0BEnd` | all inverted | Scene0B 510 |
| M4 | `Scena02_Scene0B` | member count 2 | Scene0B 73 |
| M5 | `Scena02_Scene0B` | transition 0xE | Scene0B 246 |
| M6 | `Scena02_Scene0B` | request 2 | Scene0B 165 |
| M7 | `OpenShop` | shop byte C 0 | Scene0B 386, Scene1B 443 |
| N1 | `Scena02_Scene0C` | place kind 8 | Scene0C 535 |
| N2 | `Scena02_Scene0C` | member 0x16 | Scene0C 487 |
| N3 | `Scena02_Scene0C` | flag 0x12 | Scene0C 518 |
| N4 | `ClearThenEnd` | counter 1 before the call | Scene0A 10, Scene0C 7, Scene0D 11, Scene19 7 |
| O1 | `Scena02_Scene0D` | effect x -0x2BB | Scene0D 254 |
| O2 | `Scena02_Scene0D` | life 0x15 | Scene0D 275 |
| O3 | `Scena02_Scene0D` | flag 0x14 | Scene0D 542 |
| P1 | `Scena02_Scene0E` | members swapped | Scene0E 153 |
| P2 | `Scena02_Scene0E` | sound 0x205 | Scene0E 285 |
| P3 | `Scena02_Scene0E` | area byte 0x23 | Scene0E 310 |
| P4 | `Scena02_Scene0E` | step 9 | Scene0E 247 |
| P5 | `Scena02_Scene0E` | place z | Scene0E 276 |
| Q1 | `Scena02_Scene0F` | flags swapped | Scene0F 153 |
| Q2 | `Scena02_Scene0F` | choice inverted | Scene0F 335 |
| Q3 | `Scena02_Scene0F` | flags 0x8F | Scene0F 35 |
| Q4 | `Scena02_Scene0F` | xor 0x18 | Scene0F 344 |
| Q5 | `Scena02_Scene0F` | counter 2 = 3 | Scene0F 321 |
| T1 | `Scena02_Scene10` | member byte 6 | Scene10 35 |
| T2 | `Scena02_Scene10` | turn b 3 | Scene10 87 |
| T3 | `Scena02_Scene10` | five frames | Scene10 1528 |
| T4 | `Scena02_Scene10` | turn b 2 | Scene10 97 |
| T5 | `Scena02_Scene10` | transition 2 | Scene10 113 |
| T6 | `Scena02_Scene10` | call A 1 | Scene10 99 |
| T7 | `Scena02_Scene10` | area flags 0x85 | Scene10 110 |
| U1 | `Scena02_Scene11` | music 0x26 | Scene11 97 |
| U2 | `Scena02_Scene11` | stream 0 | Scene11 157 |
| U3 | `Scena02_Scene11` | shake 0xB | Scene11 260 |
| U4 | `Scena02_Scene11` | shift x cleared | Scene11 150 |
| U5 | `Scena02_Scene11` | counter 0 = 0x1D | Scene11 189 |
| U6 | `Scena02_Scene11` | status ^ 2 | Scene11 86 |
| U7 | `Scena02_Scene11` | flag 0x37 | Scene11 71 |
| U8 | `Scena02_Scene11` | area byte 2 | Scene11 71 |
| V1 | `Scena02_Scene12` | flag 0x19 | Scene12 654 |
| V2 | `Scena02_Scene12` | Cond_ByteFD 1 | Scene12 167 |
| V3 | `Scena02_Scene12` | area flags 0x82 | Scene12 272 |
| V4 | `Scena02_Scene12` | counter 2 for 1 | Scene12 637 |
| W1 | `Scena02_Scene13` | life 0xB3 | Scene13 118 |
| W2 | `Scena02_Scene13` | area byte 0xE | Scene13 379 |
| W3 | `Scena02_Scene13` | area 4 | Scene13 383 |
| W4 | `Scena02_Scene13` | counter 0 after Kind2 | Scene13 8 |
| X1 | `Scena02_Scene15` | kind2 hold 2 | Scene15 221 |
| X2 | `Scena02_Scene15` | script or 0x40 | Scene15 334 |
| X3 | `Scena02_Scene15` | bit 80 call swapped | Scene15 677 |
| X4 | `Scena02_Scene15` | transition 6 | Scene15 650 |
| Y1 | `Scena02_Scene17` | money 0x33 | Scene17 2054 |
| Y2 | `Scena02_Scene17` | step 1 too | Scene17 1980 |
| Z1 | `Scene18Pay` | counter 0 = 8 | Scene18 422 |
| Z2 | `Scene18Pay` | message 0x6E | Scene18 293 |
| Z3 | `Scene18Other` | answer 0xE | Scene18 892 |
| Z4 | `Scene18Refused` | counter 1 = 0 | Scene18 417 |
| Z5 | `Scena02_Scene18` | member 0x1A | Scene18 31 |
| Z6 | `Scena02_Scene18` | refused step 4 | Scene18 118 |
| Z7 | `Scena02_Scene18` | step 0x14 refused 0xC | Scene18 73 |
| AA1 | `Scena02_Scene19` | take count 2 | Scene19 243 |
| AA2 | `Scena02_Scene19` | item 3 | Scene19 277 |
| AA3 | `Scena02_Scene19` | effect 0x92 | Scene19 212 |
| AA4 | `Scena02_Scene19` | counter 0 = 0x1A | Scene19 212 |
| AB1 | `Scena02_Scene1A` | count to 0x4A | Scene1A 531 |
| AB2 | `Scena02_Scene1A` | count + 2 | Scene1A 1603 |
| AB3 | `Scena02_Scene1A` | leader 7 member 0x14 | Scene1A 8 |
| AB4 | `Scena02_Scene1A` | fifteen frames | Scene1A 253 |
| AB5 | `Scena02_Scene1A` | effect 0x98 | Scene1A 74 |
| AB6 | `Scena02_Scene1A` | place z | Scene1A 165 |
| AB7 | `Scena02_Scene1A` | kind2 hold 2 | Scene1A 61 |
| AC1 | `Scena02_Scene1B` | answer 0x5C | Scene1B 166 |
| AC2 | `Scena02_Scene1B` | end keeps counter 0 | Scene1B 225 |
| AC3 | `Scena02_Scene1B` | no resume | Scene1B 113 |
| AC4 | `Scena02_Scene1B` | area flags 0x98 | Scene1B 170 |
| AC5 | `Scena02_Scene1B` | request 2 | Scene1B 86 |
| AC6 | `Scena02_Scene1B` | restore for stop | Scene1B 130 |
| AD1 | `Near` | Cond_ByteFD 1 | Near01 1181, Near02 1234, Near03 1194, Near04 1217, Near05 1196, Near06 1210, Near07 1212, Near08 1175, Near09 1201, Near0A 1200, Near0B 1225, Near0C 1193 |
| AD2 | `Near` | z hi strict | Near01 246, Near02 270, Near03 252, Near04 280, Near05 273, Near06 245, Near07 247, Near08 243, Near09 262, Near0A 252, Near0B 265, Near0C 257 |
| AD3 | `Near` | stride 0xA0 | Near01 904, Near02 963, Near03 925, Near04 919, Near05 949, Near06 963, Near07 948, Near08 910, Near09 960, Near0A 942, Near0B 979, Near0C 947 |
| AD4 | `Near` | x lo strict | Near01 250, Near02 257, Near03 253, Near04 270, Near05 249, Near06 270, Near07 269, Near08 264, Near09 279, Near0A 264, Near0B 262, Near0C 263 |
| AN01 | `Scena02_Near01` | x hi | Near01 270 |
| AN02 | `Scena02_Near02` | x lo | Near02 257 |
| AN03 | `Scena02_Near03` | z lo | Near03 265 |
| AN04 | `Scena02_Near04` | z hi | Near04 280 |
| AN05 | `Scena02_Near05` | x hi | Near05 262 |
| AN06 | `Scena02_Near06` | z lo | Near06 272 |
| AN07 | `Scena02_Near07` | x lo | Near07 269 |
| AN08 | `Scena02_Near08` | z hi | Near08 243 |
| AN09 | `Scena02_Near09` | z hi | Near09 262 |
| AN0A | `Scena02_Near0A` | x hi | Near0A 262 |
| AN0B | `Scena02_Near0B` | z hi | Near0B 265 |
| AN0C | `Scena02_Near0C` | x lo | Near0C 263 |
| AE1 | `Scena02_DoorStart` | lead 4 | DoorStart 1374 |
| AE2 | `Scena02_DoorStart` | first cell +8 = 2 | DoorStart 700 |
| AE3 | `Scena02_DoorStart` | x not re-read after the second | DoorStart 21 |
| AE4 | `DoorStart` | counter 0 = 0x24 | DoorStart 1388 |
| AE5 | `Scena02_DoorStart` | x 0x46 failing does not end | **0** (equivalent, below) |
| AE5b | `Scena02_DoorStart` | near AE5: x 0x46 failing goes on with x - 2 | DoorStart 6 |
| AF1 | `Scena02_DoorSound` | third step 0x1F | DoorSound 463 |
| AF2 | `Scena02_DoorSound` | second +8 = 6 | DoorSound 392 |
| AF3 | `AtDoor` | +8 not tested | DoorStart 87, DoorSound 111 |
| AF4 | `Scena02_DoorSound` | first x not re-read | DoorSound 73 |
| AG1 | `Scena02_StartRun07` | counter 0 = 0x20 | StartRun07 6000 |
| AG2 | `Scena02_StartRun07` | run 6 | StartRun07 6000 |
| AH1 | `Scena02_Shake` | phase 2 | Shake 501 |
| AH2 | `Scena02_Shake` | counter 3 kept | Shake 441 |
| AH3 | `Scena02_Shake` | n - shift | Shake 1486 |
| AH4 | `Scena02_Shake` | second frame 3 | Shake 991 |
| AI1 | `Scena02_PlaceEffect70` | n + 0x71 | PlaceEffect70 5730 |
| AI2 | `Scena02_PlaceEffect68` | kind 0x18 | PlaceEffect68 5723 |
| AI3 | `PlaceEffect` | table unsigned | PlaceEffect70 1064, PlaceEffect68 1095 |
| AI4 | `PlaceEffect` | +0x30 from +0x2E | PlaceEffect70 5730, PlaceEffect68 5723 |
| AI5 | `PlaceEffect` | current not read back | PlaceEffect70 233, PlaceEffect68 230 |
| AJ1 | `Scena02_ObjectHook` | row 3 | ObjectHook 5353 |
| AJ2 | `Scena02_ObjectHook` | index +0x87 | ObjectHook 5721 |
| OB01 | `Scena02_Object01` | step 6 | Object01 6000 |
| OB02 | `Scena02_Object02` | run 0xC | Object02 6000 |
| OB04 | `Scena02_Object04` | xor 0x26 | Object04 2064 |
| OB05 | `Scena02_Object05` | flag 6 | Object05 6000 |
| OB06 | `Scena02_Object06` | counter 1 = 2 | Object06 6000 |
| OB07 | `Scena02_Object07` | counter 3 | Object07 5962 |
| OB09 | `Scena02_Object09` | flag 0xE | Object09 6000 |
| OB0B | `Scena02_Object0B` | counter 0 = 2 | Object0B 6000 |
| OB0C | `Scena02_Object0C` | step 1 | Object0C 6000 |
| OB0D | `Scena02_Object0D` | step 4 | Object0D 6000 |
| OB0E | `Scena02_Object0E` | counter 0 = 0x15 | Object0E 6000 |
| OB0F | `Scena02_Object0F` | run 0xE | Object0F 6000 |
| OB10 | `Scena02_Object10` | sound 0x107 | Object10 6000 |
| OB11 | `Scena02_Object11` | counter 3 kept | Object11 5640 |
| OB13 | `Scena02_Object13` | step 0x1D | Object13 6000 |
| OB14 | `Scena02_Object14` | run 0x1A | Object14 6000 |
| OB15 | `Scena02_Object15` | no Set40 | Object15 6000 |
| OB16 | `Scena02_Object16` | run 0x18 | Object16 6000 |
| OB17 | `Scena02_Object17` | step 0xB | Object17 5432 |
| OB18 | `Scena02_Object18` | flag 0x30 | Object18 6000 |
| OB19 | `Scena02_Object19` | step 0x18 | Object19 6000 |
| OB1A | `Scena02_Object1A` | step 6 | Object1A 6000 |
| AK1 | `Scena02_StripMember` | count 2 | StripMember 6000 |
| AK2 | `Scena02_StripMember` | nine bytes | StripMember 5983 |
| AK3 | `Scena02_StripMember` | cleared before the call | StripMember 6000 |
| AL1 | `Scena02_StepHook` | area 3 x lo | StepHook 8 |
| AL2 | `Scena02_StepHook` | z >= 0x510000 | StepHook 10 |
| AL3 | `Scena02_StepHook` | z 0x80001 | StepHook 16 |
| AL4 | `Scena02_StepHook` | area byte 0x1E | StepHook 4 |
| AL5 | `Scena02_StepHook` | high word 5 | StepHook 16 |
| AL6 | `Scena02_StepHook` | member count 3 | StepHook 656 |
| AL7 | `Scena02_StepHook` | z hi 0x418001 | StepHook 4 |
| AL8 | `Scena02_StepHook` | x 0x208001 | StepHook 37 |
| AL9 | `Scena02_StepHook` | x 0x238001 | StepHook 14 |
| AL10 | `Scena02_StepHook` | counter 0 = 0x32 | StepHook 15 |
| AL11 | `Scena02_StepHook` | pass flags 1 | StepHook 16 |
| AL12 | `Scena02_StepHook` | no Set40 at area 0x1B first | StepHook 68 |
| AM1 | `StepHook1A` | step 7 | StepHook 1 |
| AM2 | `StepHook1A` | counter 0 = 0xA1 | StepHook 22 |
| AM3 | `StepHook1A` | z hi 0x4D7FFF | StepHook 25 |
| AM4 | `StepHook1A` | x hi 0x278001 | StepHook 26 |
| AM5 | `StepHook1A` | step 6 | StepHook 28 |
| AM6 | `StepHook1A` | step 0x15 | StepHook 17 |
| AM7 | `StepHook1A` | flag 0x25 inverted | StepHook 3 |
| AM8 | `StepHook1A` | member 0xC | StepHook 2 |
| AM9 | `StepHook1A` | counter 1 = 2 | StepHook 11 |
| AM10 | `StepHook1A` | counter 0 = 2 | StepHook 6 |
| AM11 | `StepHook1A` | flag 0x2F | StepHook 1797 |
| AM12 | `StepHook1A` | flag 0x24 inverted | StepHook 1735 |

## 6. Latent defects (described, not fixed)

- **The three dispatch tables are unchecked** (section 3); ours aborts.
- **`Scena02_StripMember`'s member index is unchecked**: it writes ten
  bytes at `0x903AEE + 0xA4 * member`; its one caller passes 3 and 4, so
  nothing reaches past the eight records (`Cond_Flags` begins at
  `0x903F90`), but an index of 8 or more would clear bytes in the flags and
  beyond. Ours writes where the original does.
- **`Scena02_Scene09` step 3 waits on `Effect_Objects[counter 3]`**, the
  slot step 2 stored in counter 3; a script counter op in between moves it
  to any of 256 records (a read past the 20, no fault), chapter 1's defect
  again ([`scena_sc1.md`](scena_sc1.md) §6).
- **The place helpers read `Scena02_EffectX` unbounded** by the byte
  `0x904062`; past its 8 bytes lies `Scena02_Hooks` (readable, no fault).
- **Two step-hook starts skip `ScriptFlags_Set40`**: area 0x1B's run 0xA
  from the rectangle at `z` 0x1C8000 (member count 2, flag 0xF clear) and
  run 0xF at `x` 0x208000 (flag 0x18 clear) set the run and answer 1 without
  it, where every other start sets it. Faithful; whether the scenes then
  run with the player's control is not measured.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D136
(reads and writes by an unchecked byte or count), D153 (hooks without the
script flag) in [`known-defects.md`](known-defects.md).

## 7. Calls across groups (raw addresses, for the round's rebinding)

| Address | What | Owner |
|---|---|---|
| `0x532ED0` | the party placed for an event battle `(x, z, kind)` | SX, this wave |
| `0x533E50` | the members' records rebuilt | SX |
| `0x56D6F0` | `Field_StatusBits` \|= 0x80 | SX |
| `0x587B80` | the music stopped (a `jmp` to the sound layer) | SX |
| `0x590C90` | an item handed back `(item, 0, 1, 0)` | SX |
| `0x591B60` | an item taken `(4, 2, 1, 0)` | SX |
| `0x591BC0` | the money take `(0x32, 0)`, `al` | SX |
| `0x591BE0` | the money give `(0x32, 0)` | SX |
| `0x57C600` | a turn test `(s16, s8)`, `al` (`0x57C650` on the two scaled; `0x57C550`'s sibling) | nobody's - not in SX's list |
| `0x557170` | `Scena02_ObjectHandlers` entry 0xA (a table entry, not a call) | SC9b, this wave |

The rest are named and called by name: `Field_StartEventBattle` (SE's),
`Scenario_CallA`, `Scenario_CallB`, `Field_ChangeArea`, `Flags_*`,
`Music_*`, `Party_DropIn`, `Kind2_Place`, `Msg_OpenScript`,
`MoveCmd_TestFB`, `Inventory_Add`, `Sound_ResumeAll`, ... No call enters
SC3's band.

## 8. What nothing reached

Fuzz only, like every scenario group until a chapter-2 route exists
([`takeover-queue-scenario.md`](takeover-queue-scenario.md) §5). The object
handlers are reached only when a field object's trigger sets +0x86, the
step hook only in areas 3, 0xF, 0x1A, 0x1B and 0x1C; chapter 2 is not on
the attract path (chapter 16 is), so the frame hash is untouched by this
group.
