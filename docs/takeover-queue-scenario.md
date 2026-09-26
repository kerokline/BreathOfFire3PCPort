# The scenario round: the banks wave by wave, on the spell round's pattern

**Status:** PROPOSED (2026-09-26) - a plan, not a queue. Listed as
[`IDEAS.md`](IDEAS.md) I23; nothing here is scheduled or cut. The
functions and groups come from [`scenario-roots.md`](scenario-roots.md)
(`tools/scenario_roots.py`); the method is the spell round's
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md),
[`magic_harness.md`](magic_harness.md)) with the differences section 2
names.

## 1. What is to be taken

The 638 functions the three chapter tables reach, less the 17 already ours
(the chapter 16 demo, `Scena01_Frame`, `Scena01_StepHook`,
`Scenario_CallA`, `Scenario_NoHook`): **about 620 functions, ~190 KiB**.
They lie in one band, `0x537F20..0x56C080`, in chapter order, which is what
makes whole-chapter groups possible - the same fact the spell round used
when `magic_rows.py` found each overlay's extent contiguous.

Outside the band, eight helpers the banks share with the field engine
(`0x4410B0`, called from 14 chapters; `0x520000`, `0x524870`, `0x579D70`,
`0x57A010`, `0x591CC0`, `0x508000`, `0x5080A0`, `0x519F70`) - the analogue of
the spell round's group L, and first for the same reason.

Not in this round: the 125 SCENA-labelled functions the world-map records,
the battle tables and the area handlers reach
([`scenario-roots.md`](scenario-roots.md) §5). They go with those rounds.

## 2. What a bank function is, to a harness

A spell function is a battle task's step with one frame of battle state as
its every input. A bank function is one of three shapes, and the harness
has to hold all three:

| Shape | Called by | Arguments | Reads |
|---|---|---|---|
| A vtable slot | `Field_ModeDispatch` (slot 0, every field frame), the object trigger (1), the step and arrive hooks (2, 3: `(x, z)`, answer in `al`), slot 4 | none, or two words | the chapter bytes `0x8034E0..E9` (`Cond_ByteFA`, the state `0x8034E2`, `0x8034E4` = `MoveScript_Var7`, `0x8034E5`), `Cond_Flags` `0x903F90..` and its row pointer `0x929ED0`, the story flags `0x904030`, `Field_State` / ObjTrio, `Field_Request`, `Game_AreaNumber`, `0x903848`, the wait word `0x66C810` |
| A call-table entry | `Scenario_CallA` / `B` from area handlers and event ops, arguments where the caller left them | 0..3 words, cdecl, the caller cleans up | the same, plus `Sprite_Current` |
| A state handler | its chapter's frame function through a table of tiny handlers (a `.data` pointer table, or a switch by `0x8034E5`) | none | the same |

The state-machine shape is the same in every chapter read so far: a frame
function switches on `0x8034E5` (or a byte table indexes a pointer table),
each case a handler of 16..300 bytes that tests a flag or a wait, calls two
or three engine functions, sets the next state, returns. Chapter 16's
`Scena16_*` ([`field-modes.md`](field-modes.md)) is the template; chapters
0, 4, 8, 11 and 15 look the same at a glance and are small.

**The recorder set** is the frontier the walk measured: 232 engine functions,
60 of them named - `Msg_OpenScript`, `Msg_OpenSystem`, `Field_ChangeArea`,
`Party_DropIn`, `Party_Join`, `PartySet_Load*`, `Flags_Test` / `Set` /
`Clear`, `Sprite_*`, `EventObj_*`, `Music_*`, `Transition_Start`, `Task_*`,
`Inventory_Add`, `Effect_SpawnAt`, `LoadDatFile`, `File_LoadDone`, `Rand` -
and 172 unnamed, 98 of them in `Boot: field, map and sprites`, 20 in the
event script. `analysis/scenario_roots.json` `frontier_functions` lists
them. A group calls out through the harness exactly as a spell group does
(`MH_CALL` by name, `MH_AT` by address, `Phase` for a table entry), and the
fold that made one spell harness of eleven groups' extensions is the
precedent for growing this one.

**The fuzz** randomises one frame of the state in the table above, sets
`Cond_ByteFA` to the group's chapter, and calls each root and each handler
under Capcom's and ours, comparing the state after and the recorders'
tapes. `Scenario_Start(n)` under the same harness covers the load path.
Controls as the spell round's: a mutant per function that the fuzz must
refuse.

## 3. The groups

Whole chapters where a chapter's code stands alone, merged where
consecutive chapters share a block, about 40..60 functions a group, in
address order. Counts are the walk's; a group's real count is settled when
it is cut, as the spell groups' were.

| Group | Chapters | Band | Fns | Bytes | Notes |
|---|---|---|--:|--:|---|
| SE | shared engine-side helpers | eight addresses outside the band | 8 | ~1,700 | first, as L was |
| SC0 | 0 | `0x537F20..0x539A30` | 23 | 7,392 | the opening; the new game's first chapter |
| SC1 | 1 | `0x539AD0..0x53DD50` | 46 | 13,088 | `Scena01_Frame` and `Scena01_StepHook` already ours; `Scena01_Hooks`, `Scena01_States` named |
| SC2a | 2 (first half) | `0x53DDA0..0x53FFFF` | ~36 | ~9,600 | 73 functions in all; split at the address midpoint |
| SC2b | 2 (second half) | `0x540000..0x5420C0` | ~37 | ~9,600 | |
| SC3 | 3, 4 | `0x5428C0..0x546370` | 54 | 15,024 | 4 is 19 functions; merged with 3 |
| SC5 | 5 | `0x546390..0x54A8A0` | 53 | 17,792 | |
| SC6 | 6 | `0x54A910..0x54EFC0` | 55 | 17,568 | |
| SC7 | 7, 8 (with 6's shared tail) | `0x54F080..0x553810` | 66 | 19,120 | 7 and 8 have no code of their own; all of it is here |
| SC9a | 9 | `0x553B30..0x557150` | 41 | 13,920 | |
| SC9b | 9 (tail), 10 | `0x557170..0x55BE70` | 60 | 19,712 | |
| SC11 | 11 | `0x55C040..0x55E4D0` | 36 | 9,616 | |
| SC12 | 12 | `0x55E4E0..0x561D50` | 24 | 16,176 | few functions, long ones |
| SC13 | 13, 14 (with 12's shared tail) | `0x561DB0..0x567A90` | 63 | 24,480 | |
| SC15 | 15 | `0x567DC0..0x56AB10`, `0x537580` | 19 | 12,256 | |
| - | 16 | `0x56B2A0..0x56C080` | 1 | 32 | done in title-states; slot 1 `0x56C080` left, folds into SC15 |
| - | 17, 18, 19 | stubs | 0 | | `Scenario_NoHook` and one-entry tables |

Sixteen groups, ~620 functions.

## 4. The waves

Three waves of five or six groups, ~200 functions each; the small
state-machine chapters first so the harness's shape is settled on the easy
ones, as Steal settled the spell harness.

| Wave | Groups | Fns | Why this order |
|---|---|--:|---|
| 1 | SE, SC0, SC1, SC3, SC11, SC12 | ~190 | the shared helpers; the chapters under 55 functions with the plain state-machine shape; SC1 already half-named |
| 2 | SC5, SC6, SC7, SC9a | ~215 | the middle of the game, contiguous, `0x546390..0x557150` |
| 3 | SC2a, SC2b, SC9b, SC13, SC15 | ~215 | the two split chapters, the shared tails, the end |

Each wave: agents in worktrees, one group each, headless self-tests, merged
one at a time, the round doc per group as the spell round writes them
([`magic_s16.md`](magic_s16.md) is the shape). Rate-limit cuts resume.

## 5. The live check

Fuzz-only until a route exists, as every spell group is. The live check per
group is **a recipe save at the chapter's start** played through the
chapter under original and ours with the frame hash compared - the
recorded routes of [`input-script.md`](input-script.md) and
`tools/recipe_saves.py`. The owner records them; the order above lets the
saves arrive after the fuzz, not before it. Chapter 0 needs no save (a new
game). Chapter 16 is checked already (the attract cycle).

What the fuzz cannot see and the live check would: a pointer stored at run
time and called later, and any handler a chapter reaches only through code
outside the chapter tables ([`scenario-roots.md`](scenario-roots.md) §6).

## 6. Before the first cut

1. **Read one small chapter whole** (SC0 or SC11) to confirm the three
   shapes in section 2 and list the state bytes a harness must randomise.
   Half a day; it fixes the harness's design.
2. **Name the tables**: the 20 vtables `0x660CD0..0x662C58`, the call tables
   at `0x65F668..`, and the per-chapter state tables, in `symbols.toml`, so
   the groups can refer to slots and entries by name (`Scena01_Hooks` and
   `Scena01_States` are the precedent).
3. **The harness**, `src/game/scenario_harness.*`, from `magic_harness` with
   the state table of section 2 and the three call shapes; proved on SC0.
