# Scenario chapter 11: its frame, eight scenes, fourteen triggers and two hooks

**Status:** IN PROGRESS (2026-09-27) - stage A: thirty functions written
(`src/game/scena_sc11.cpp`, shadow name `scena_sc11`), their `symbols.toml`
entries with `impl`, the fuzz file against the scenario harness's contract
and the `entries_logic.txt` lines; **not built, not fuzzed, no controls yet**
(the harness, group SCH's, had not merged). Syntax-checked against a private
copy of `magic_harness.h` renamed to `scenario_harness`.

Group SC11 of round ten's first wave ([`takeover-queue-round10.md`](takeover-queue-round10.md)
§1; the plan [`takeover-queue-scenario.md`](takeover-queue-scenario.md)).

## Stage A done - what stage B has to do

1. Merge `phase-3/capture-round-ten` at the harness's SHA. Hold
   `scena_sc11_fuzz.cpp` against the real `scenario_harness.h`: the call
   shape is written as `Clone::shape` of type `scenario_harness::Shape`
   (`kSlot`, `kObject`, `kHook`, `kCallEntry`) after `ret_mask` and `calm`,
   the form `tools/scenario_rows.py --clones` (222eb0f) prints - rename if the
   harness chose otherwise. `Group` is filled positionally up to `args`
   (`settle`, `phase_span` 0, `args`), as magic_harness's.
2. Drop from `kRegions` any region the harness's standard set already holds
   (the chapter bytes `0x8034E0..`, the counters, `Field_Request`, the wait
   word, `Game_AreaNumber`, `Sprite_Current` are likely in it), and from
   `kCallees` nothing: the group's listing stands over the standard one.
3. Register the module: `src/game/scena_sc11.cpp` and `scena_sc11_fuzz.cpp` at
   the end of `CMakeLists.txt`'s list (the `)` on the last line only), and
   `#include "game/scena_sc11.h"` / `ScenaSc11_Inject();` at the end of
   `src/hook/inject_all.cpp`.
4. Build; `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc11` to 0 mismatches,
   then `BOF3X_SHADOW='*'`; check the coverage line names every callee of
   section 5 and the three `.data` tables' handlers.
5. The controls of section 7, planted one at a time by a script (plant,
   rebuild, run, restore, rebuild), each anchored on a unique string; an
   equivalent recorded with its reason and a refused near variant.
6. `tools/ledger_check.py` 0 errors; this doc's status, section 4's counts and
   section 7's results; the `docs/README.md` row.

## 1. What chapter 11 is, by its code

Chapter 11's vtable is `0x661658` (`0x662C80[11]`; SCH names the vtables),
the sibling's `SCENA11.EMI` record `0x801FAEDC`
([`loader_records/SCENARIO.md`](../../BreathOfFire3Recomp/docs/loader_records/SCENARIO.md)):

| Slot | PC | PSX (the twin by slot, a hypothesis) | Ours |
|--:|---|---|---|
| 0 frame | `0x55C040` | `0x801F7288` | `Scena11_Frame` |
| 1 object trigger | `0x55E170` | `0x801FA5B8` | `Scena11_ObjectTrigger` |
| 2 step hook | `0x539AC0` (`Scenario_NoHook`, shared) | `0x801FAB64` | already ours |
| 3 arrive hook | `0x55E450` | `0x801FAA74` | `Scena11_ArriveHook` |
| 4 cell hook | `0x55E4D0` | `0x801FAB6C` | `Scena11_CellHook` |

Its call tables are not in the band: table A (`0x65F7DC`, nine entries) and
B (`0x65F800`, five) point into `0x5199A0..0x51A540` (Boot: field; the
seven read - B 0..3, A 0..2 - are party set-ups that read no argument), where the PSX's `subA` entries are in the
overlay itself (`0x801FAB74..`). The chapter reaches them through
`Scenario_CallA` and `0x5341C0`, with the entry number only.

The shape is the plan's plain state machine (as `Scena16_*`,
[`field-modes.md`](field-modes.md)): the frame jumps through
`Scena11_States` on the s8 `0x8034E2` (0 start, 1 area set-up, 2 run); the
run jumps through `Scena11_Runs` on `MoveScript_Var7` to a scene; a scene is
a switch on the u8 step `0x8034E5` whose cases wait on the script counter
bytes `0x903848..0x90384B` (which the area's event scripts set), the message
request `Field_Request`, the wait word `0x66C810` or a sound stream, call two
or three engine functions and set the next step; a scene's last step clears
the counters, the step and `MoveScript_Var7`. The object trigger and the
arrive hook start scenes. What the scenes are in the story is not read here:
the code names areas (`0x65`, `0x79`, `0x82`, `0x83`, `0x84`, `0x88`, `0x44`,
`0x9C`), items (category 0: `0x23`, `0x24`, `0x4D`, `0x56`), script and
system messages and flags, not events.

**The band's starts.** `0x55C040..0x55E4E0` holds 33 listed starts
(`pc_funcs`, `pc_hidden`, the walk's added starts) and three roots in no list
(slots 1, 3, 4): 36. **Thirty are functions; six are cases** of two
switches that `pe_hidden.py` read as starts from their `.text` jump tables:
`0x55C390` (case 0 of `Scena11_Scene1`), `0x55CD10`, `0x55CF50`,
`0x55CFC0`, `0x55D100`, `0x55D140` (cases 0, 9, 12, 18, 20 of
`Scena11_Scene5`). `tools/scenario_rows.py --unit SC11` (222eb0f) drops the
same six and lists the same thirty with the same extents as the group's own
descent. The walk reached all 36. Nothing in the band was ours; nothing is
missing from it (the next function, `0x55E4E0`, is chapter 12's slot 0).

## 2. The functions

Extents by recursive descent, jump tables included. Shape: the call shape
the fuzz's clone is marked with.

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x55C040` | `Scena11_Frame` | 0xE | vtable slot 0 | `jmp [Scena11_States + s8 0x8034E2 * 4]`, unchecked |
| `0x55C050` | `Scena11_Start` | 0xF | state handler (States 0) | pass flags 0x1F, state 1 |
| `0x55C060` | `Scena11_EnterArea` | 0x2F4 | state handler (States 1) | the set-up by area and counter 2 (below); state 2 |
| `0x55C360` | `Scena11_Run` | 0xE | state handler (States 2) | `jmp [Scena11_Runs + s8 MoveScript_Var7 * 4]`, unchecked |
| `0x55C370` | `Scena11_Scene1` | 0x3DD | state handler (Runs 1) | steps 0..15, 0x32, 0x33, 0x3C (below) |
| `0x55C750` | `Scena11_Scene2` | 0x73 | state handler (Runs 2) | drop-in 0; at counter 0 3 flag 5, end |
| `0x55C7D0` | `Scena11_Scene3` | 0x1A5 | state handler (Runs 3) | drop-ins 4 / 5, flags 6 / 9; step 10 the four-item check |
| `0x55C980` | `Scena11_Scene4` | 0x2C8 | state handler (Runs 4) | an effect rolled from `Scena11_EffectRolls`; items 0x23 (9) and 0x56 (1) |
| `0x55CC50` | `Scena11_EffectAnimate` | 0x97 | scene 4's E8, (slot) | an effect record made the current sprite and animated |
| `0x55CCF0` | `Scena11_Scene5` | 0x583 | state handler (Runs 5) | 24 cases over steps 0..0x2A |
| `0x55D280` | `Scena11_Scene6` | 0x475 | state handler (Runs 6) | 26 cases over steps 0..0x30 |
| `0x55D700` | `Scena11_Scene8` | 0x8F4 | state handler (Runs 8) | 40 steps; effects placed and waited on |
| `0x55E000` | `Scena11_Scene9` | 0x168 | state handler (Runs 9) | drop-ins 9 / 10, flag 0x13; area 0x79 |
| `0x55E170` | `Scena11_ObjectTrigger` | 0x1F | vtable slot 1, (object) | `call [Scena11_Triggers + object[0x86] * 4](object, flags)`, unchecked |
| `0x55E190` | `Scena11_Trigger01` | 0x48 | state handler (Triggers 1) | once (flag 5): scene 2 |
| `0x55E1E0`..`0x55E420` | `Scena11_Trigger02`..`14` | 0x23..0x2A | state handlers (Triggers 2..14) | `ScriptFlags_Set40`, counters 1..3 0, a scene from a step |
| `0x55E450` | `Scena11_ArriveHook` | 0x80 | vtable slot 3, hook (x, z), al | area 0x79, x at most 0x160000: scene 1 at step 0x3C / 0 / 0x32, al 1; else al 0 |
| `0x55E4D0` | `Scena11_CellHook` | 0x3 | vtable slot 4, hook (x, z), al | al 0xFF |

The triggers' scenes and steps (`MoveScript_Var7`, step): 2 (3, 0), 3 (5, 0),
4 (3, 0xA), 5 (6, 0), 6 (7, 0), 7 (9, 0), 8 (9, 5), 9 (0xA, 0x14),
10 (0xB, 0x14), 11 (0xB, 2, counter 0 cleared too), 12 (0xE, 0), 13 (0xE,
0x1E), 14 (5, 0x28).

**`Scena11_EnterArea`.** In area 0x65 by counter 2: 1 pass flags 0x1F, 2
pass flags 0 (each then state 2 and return), 0 - with flag 0xC set and 0xE
clear - `ScriptFlags_Set40`, the counters, pass flags and step 0,
`MoveScript_Var7` 8 (scene 8), and the call-table entries by the byte
`0x90412C & 0x7F`: 7 B1 B0 A0, 8 B1 A1, 9 B1 B2 A0, 10 B1 A2, 13 B0 A1, 14
B0 B2 A0, 15 B0 A2, 16 B2 A1, 18 B2 A2 (B is `0x5341C0`, A
`Scenario_CallA`). Then by the area, read afresh: 0x79 (counter 2 1: pass
0), 0x82 (1: pass 0x1F), 0x83 (1: elevation 0x498 and on; 2: script flags
`| 0x16`, pass 0x1F; 3: pass 0x1F), 0x84 (1: script flags `| 0x16`, pass 0;
2: pass 0x1F), 0x88 (1: `0x5341C0(3)`; 2: elevation 0x200; 10: an effect of
kind 0x77, `+1` 1), and 0x2D, 0x41, 0x57, 0x10, 0x73 clear the counters, the
step and `MoveScript_Var7`. State 2.

**The scenes** in outline (the code comments in `scena_sc11.cpp` are per
step): scene 1 changes area to 0x83 and 0x9C and back, sets flags 1, 2, 0,
4, and its steps 0x32 / 0x3C open script messages 6 / 7 (the arrive hook
starts it at 0, 0x32 or 0x3C); scene 3's step 10 asks `Inventory_Count` for
items 0x23, 0x24, 0x56 and 0x4D and goes to step 15 only with all four;
scene 4 gives item 0x23 (nine) or shows its name when the bag is full, and
item 0x56 (one); scene 5 enters area 0x44 four ways, plays streams 6 and 5,
gives item 0x24 (four); scene 6 enters area 0x82 several ways and plays
streams 7, 6, 2 and music track 0x83; scene 8 (started by the area set-up in
area 0x65) moves through areas 0x65, 0x84 and 0x83 placing effects of kinds
0x13 and 0x31 at `(x, Camera_Angles[1], z)` and waiting for each to end
(counter 3 holds its slot); scene 9 ends in area 0x79.

## 3. The tables

| Table | Named | Entries | Read by |
|---|---|--:|---|
| `0x661648` | `Scena11_EffectRolls` | 16 bytes | `Scena11_Scene4`, by `Rand & 15` |
| `0x66166C` | `Scena11_States` | 3 | `Scena11_Frame` |
| `0x661678` | `Scena11_Runs` | 10 (0 and 7 a bare ret, `0x437CC0`) | `Scena11_Run` |
| `0x6616A0` | `Scena11_Triggers` | 15 (0 a bare ret) | `Scena11_ObjectTrigger` |

As chapter 16's, the state table sits right after the vtable, and the run
table right after it. The switch tables in `.text` (after each scene) are
not named; each is bounded by its `cmp` (Scene1 `0x3C` through a 61-byte
index table, Scene3 `0x10` through 17, Scene4 `0x32` through 51 and a second
of 6 on the roll, Scene5 `0x2A` through 43, Scene6 `0x30` through 49, Scene8
`0x27` direct, Scene9 9 direct, EnterArea `0xB` on the party byte).

## 4. The fuzz

`BOF3X_SHADOW=scena_sc11`, `scena_sc11_fuzz.cpp`, one `scenario_harness::Run`:

- **the clones**: the thirty, as `scenario_rows.py --clones` printed them,
  each marked with its call shape;
- **the callees**, all listed by the group: the flag bits (`Flags_Test` a
  flag, `Flags_Set`, `ScriptFlags_Set40` / `Clear40`), the call tables
  (`Scenario_CallA`, `0x5341C0`, the entry byte), `Field_ChangeArea`,
  `Party_DropIn`, `Transition_Start`, `MapView_SetElevation`, `Kind2_Place`,
  `MoveCmd_TestFB`, `Port_DroppedCall`, `0x534DB0`, `0x537480`, `0x56D6F0`,
  `Effect_FindFree` (a slot 0..0x13 or 0xFF), `Sprite_SetAnimationBank`,
  `Sprite_SetAnimation`, `Scena11_EffectAnimate` (ours, called by its E8),
  `Rand`, `Msg_OpenScript`, `Msg_OpenSystem`, `Inventory_Count` (a bool: the
  callers test ax whole), `Inventory_Add` (a flag), `Item_NamePtr` (answers a
  16-byte buffer of the fuzz's own, filled from the stream), the sound and
  music calls (`Sound_StreamDone` a bool: eax tested whole);
- **the `.data` tables** swapped for recorders: `Scena11_States`,
  `Scena11_Runs`, `Scena11_Triggers` (a `MoveScript_Var7` of 10..24 reaches a
  trigger's recorder through the run table, as in the game);
- **regions**: the chapter bytes, the camera and counters, the effect
  records, `ObjTrio`, `Text_Records` and the bytes around it, the flags and
  bytes the scenes write (`kRegions`);
- **the seed**: the flag pointer at a buffer of the fuzz's own, the state
  0..2, `MoveScript_Var7` the scenes and 10, 11, 14 (or 0..24), counter 0 the
  values waited on, counter 2 the set-up's values, counter 3 a slot with its
  record live or not, the request 0 / 2 / 8, the wait word 0, the areas each
  test names, the party byte 6..19 (with its top bit), member 0's `+8` (0, 6,
  7, others) and `+0x89` (6, 7, others), each scene's steps (every case and
  the one past its bound), the object's `+0x86` 0..14, the hook's x at
  0x160000 either side;
- **the disturbance**: a counter byte, the request, the wait word,
  `Field_Kind2Hold`, the live byte of counter 3's effect; `settle` keeps
  counter 3 inside the twenty records.

Counts: stage B.

## 5. Cross-group calls

By raw address in `scena_sc11_callees.h` (nobody owns them this wave):
`0x5341C0` (call table B's thunk, `Scenario_CallA`'s twin), `0x534DB0`,
`0x537480` (below SC0's band), `0x587B80` (a jmp into the sound layer),
`0x56D6F0` (past the bank: `Field_StatusBits |= 0x80`). Every other callee
is ours already and called by name. No SE helper is called.

## 6. Latent defects (Capcom's, described, kept)

- **`Scena11_Runs` is ten entries and the triggers set
  `MoveScript_Var7` past it.** Triggers 9 (0xA), 10 and 11 (0xB), 12 and 13
  (0xE) leave a value that `Scena11_Run` reads out of `Scena11_Triggers`:
  0xA a bare ret, 0xB `Scena11_Trigger01` (flag 5 clear: scene 2 and flag 5;
  set: nothing), 0xE `Scena11_Trigger04` (scene 3 at step 0xA, the item
  check). Whether the area's event scripts consume those values before the
  next frame is not measured; the PSX was not compared. Ours reads the table
  in place, as the original.
- **`Scena11_ObjectTrigger` indexes its table by the object's `+0x86`,
  unchecked**: 15 reads the 0 at `0x6616DC` (a call to address 0), 16 and on
  chapter 12's vtable. Ours reads in place (the `.data` dispatch precedent).
- **Two steps run on into the next step's test** (a missing `break`, by the
  shape): `Scena11_Scene1` step 11 with `Field_Kind2Hold` set runs step 12's
  counter test, `Scena11_Scene8` step 7 with its effect still live runs step
  8's. Both only test and set the next step; kept.
- **`Scena11_Scene5` step 9 stores its effect slot into member 0's `+0xB`**
  (through `Sprite_Current`, set to `ObjTrio` first) and on a full pool
  leaves 0xFF there; what member 0's `+0xB` is was not read.
- **A full effect pool leaves most steps where they are** (they retry each
  frame); `Scena11_Scene8` step 14 goes on regardless, and
  `Scena11_Scene4` step 0 takes a new effect every frame its roll is one the
  switch does not act on (every value in the shipped table is one it acts
  on).

For the coordinator to number; nothing is fixed here.

## 7. The controls (stage B)

Planned, one per behaviour, each to be refused by a count:

| | Plant | In |
|---|---|---|
| F1 | the frame through `Scena11_Runs` | `Scena11_Frame` |
| F2 | the start's state 2 | `Scena11_Start` |
| E1 | area 0x65 counter 2 1 / 2 pass flags swapped | `Scena11_EnterArea` |
| E2 | flag 0xE tested for set | `Scena11_EnterArea` |
| E3 | party byte `& 0xFF` | `Scena11_EnterArea` |
| E4 | entry 13's `CallB(0)` as `CallB(1)` | `Scena11_EnterArea` |
| E5 | area 0x83 counter 1 returns before the 0x84 test | `Scena11_EnterArea` |
| E6 | area 0x88 counter 10 effect kind 0x78 | `Scena11_EnterArea` |
| E7 | area 0x73 dropped from the clearing list | `Scena11_EnterArea` |
| E8 | the area not read afresh after the case-0 calls | `Scena11_EnterArea` |
| R1 | the run through `Scena11_States` | `Scena11_Run` |
| S1 | scene 1 step 0 member 0's 6 drops in 1 | `Scena11_Scene1` |
| S2 | scene 1 step 3 area x 0x60000 | `Scena11_Scene1` |
| S3 | scene 1 step 11 without the fall-through | `Scena11_Scene1` |
| S4 | scene 1 step 15 script flags `^ 0x10` | `Scena11_Scene1` |
| S5 | scene 1 step 0x33 Var7 kept | `Scena11_Scene1` |
| S6 | scene 2 step 1 counter 0 cleared | `Scena11_Scene2` |
| S7 | scene 3 step 10 item 0x4E | `Scena11_Scene3` |
| S8 | scene 3 step 12 counter 0 kept | `Scena11_Scene3` |
| S9 | scene 4 roll `& 7` | `Scena11_Scene4` |
| S10 | scene 4 slot not read back after Rand | `Scena11_Scene4` |
| S11 | scene 4 full bag: script, not system, message | `Scena11_Scene4` |
| S12 | `EffectAnimate` +0x20 -7 | `Scena11_EffectAnimate` |
| S13 | `EffectAnimate` Sprite_Current read once | `Scena11_EffectAnimate` |
| S14 | scene 5 step 0x2A as step 4 without the 0x14 branch | `Scena11_Scene5` |
| S15 | scene 5 step 9 slot byte read once before the call | `Scena11_Scene5` |
| S16 | scene 5 step 6 bit 0x20 | `Scena11_Scene5` |
| S17 | scene 5 step 0x28 counter 0 0xFE | `Scena11_Scene5` |
| S18 | scene 6 step 20 counter 3 tested for 2 | `Scena11_Scene6` |
| S19 | scene 6 step 35 arguments swapped | `Scena11_Scene6` |
| S20 | scene 6 step 10 Set40 after the request test | `Scena11_Scene6` |
| S21 | scene 8 step 1 distance - 0x400 | `Scena11_Scene8` |
| S22 | scene 8 step 3 +0xC not cleared | `Scena11_Scene8` |
| S23 | scene 8 step 7 without the fall-through | `Scena11_Scene8` |
| S24 | scene 8 step 14 stops on a full pool | `Scena11_Scene8` |
| S25 | scene 8 angle 2, not 1 | `Scena11_Scene8` |
| S26 | scene 9 step 9 script flags `^ 0x14` | `Scena11_Scene9` |
| T1 | the trigger index `+0x85` | `Scena11_ObjectTrigger` |
| T2 | trigger 1 flag 6 | `Scena11_Trigger01` |
| T3 | trigger 4 step 0xB | `Scena11_Trigger04` |
| T4 | trigger 11 counter 0 kept | `Scena11_Trigger11` |
| H1 | the hook's x `>=` | `Scena11_ArriveHook` |
| H2 | the hook's member byte 7 dropped | `Scena11_ArriveHook` |
| H3 | slot 4 answers 0xFE | `Scena11_CellHook` |

## 8. What reaches it

Nothing recorded: no route plays chapter 11 (`Cond_ByteFA` 11). The live
check is a recipe save at the chapter's start
([`takeover-queue-scenario.md`](takeover-queue-scenario.md) §5), played under
original and ours with the frame hash compared. Chapter 11 is not on the
attract path, so the frame hash reference does not move.

`analysis/calltrace/entries_logic.txt`: 30 lines under a `group SC11`
comment; `0x55CC50` was listed as a host at `5AA9` (`pc_funcs`' size) and is
re-listed at its own `97` (the comment says the new line supersedes it).

No divergence and no ledger entry: every function is a faithful replacement.
