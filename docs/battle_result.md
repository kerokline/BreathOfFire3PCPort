# The battle result

**Status:** IN PROGRESS (2026-09-25). Seventeen functions are ours
(`src/game/battle_result.cpp`). Each is fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 17,000 rounds, 0
mismatches. 72 negative controls were planted: 69 were refused by a count, and three were
changes that change nothing (section 4).

This is group CD of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Every function is
a *faithful* replacement, so no `DIVERGENCE.md` entry is owed. The model was
round seven's [`battle_flow.md`](battle_flow.md).

The code is the PSX `BATL_END.EMI` overlay compiled into the exe. The
sibling names four of its PSX twins from its Ghidra decomp
(`../BreathOfFire3Recomp/names/functions.toml`, overlay `18ce968c...`,
BATL_END): `BattleResult_ExpTick` 0x801EEF58, `BattleResult_Setup`
0x801EF390, `BattleResult_ZennyTick` 0x801EF810 and
`BattleResult_AwardDrops` 0x801EF874. Their decomp descriptions match the
PC bodies step for step; the names are taken from them. The PSX code itself
was not read this round (evidence tier: the PC disassembly, the sibling's
description cited).

## 1. The shape: three step tables and two window states

The result sits under `Battle_PhaseDispatch`'s sixth handler `0x4311E0`
(by `0x904AA1`, table `0x64AF44`), its entry 1 `0x4314B0` (by `0x904AA2`,
table `0x64AF64`) and that table's entry 2 `0x431910` - all group CC's
stubs (read 2026-09-25). There the byte `0x904AA3` picks an entry of the three-entry `.data`
table `0x64AFA0` (dispatched by CC's stub `0x431910`), and each entry
dispatches again on the byte `0x904AA4`:

| `0x904AA3` | Entry | Dispatch | Table (ours, `[[data]]`) | Steps |
|--:|---|---|---|---|
| 0 | `0x431920` (CC's) | `jmp [0x64AFAC + i*4]` | `BattleResult_ExpSteps` 5 | split, window, wait, tick, done |
| 1 | `BattleResult_LevelUpStep` `0x431B60` | `jmp [0x64AFC0 + i*4]` | `BattleResult_LevelUpSteps` 2 | search, announcement (`0x431C10`, left) |
| 2 | `BattleResult_RewardStep` `0x431D50` | `jmp [0x64AFC8 + i*4]` | `BattleResult_RewardSteps` 4 | setup, wait, tick, award |

Every handler takes nothing and returns nothing, so the stubs' tail jumps
are plain calls in ours. Ours reads the `.data` dword when the step runs
and, as the original, does not check the index (0..255 all land inside
`.data`; past the end is the next table).

The result window is window slot 1 (`0x803184`, 36-byte records from
`0x803160`) of kind 4, whose handler is `0x597F60` (group CM's). Its stack
table's slots 4 and 5 are ours:

- `BattleResultWin_ExpState` `0x598570` (state 4, set by
  `BattleResult_OpenExpWindow`) and `BattleResultWin_ZennyState`
  `0x5986C0` (state 5, set by `BattleResult_Setup`) each build a two-entry
  table on their stack and call it by the record's byte `+3`:
  `BattleResultWin_NextStep` `0x5986F0` (the open frame: `+3` one on), then
  the draw. An index past 1 would call through the stack; ours aborts.
- The drop window is slot `0x15`, state 1 of the same kind: `0x5984B0`,
  **no group's** (section 6).

## 2. What each function does

Written in full above each function in `battle_result.cpp`, and in each
`evidence` field. In short (`Input_Held` `0x7E1BE8`, `Input_Pressed`
`0x7E1BEC`, `File_LoadDone` always 1 on the PC):

| Function | Entry | Bytes | PSX (paired) | What |
|---|---|--:|---|---|
| `BattleResult_SplitExp` | `0x431940` | 0x6E | `0x801EECD4` | EXP total `0x904AEC` = ceil(total / `0x4319B0`()), 0 at a zero total or count; `"%d"` into text record 0; `0x93B8E4` = `Msg_SystemPtr(5)`; `PartySet_Select(0x90412C & 0x7F, 0)` |
| `BattleResult_OpenExpWindow` | `0x431A20` | 0x68 | `0x801EEE50` | `Window_Alloc(1, 4)`, state 4, step 0, height count * 16 + 4; tick step `0x904B70` = 1 below 30, else total / 30 (a word) |
| `BattleResult_ExpWaitHeld` | `0x431A90` | 0x11 | `0x801EEEE4` | on when a button is held |
| `BattleResult_ExpTick` | `0x431AB0` | 0x75 | `0x801EEF18` (the sibling's `ExpTick` `0x801EEF58`) | held: all at once; else `0x4468B0(step)` while total - step > 0 (signed), then the rest; total 0, on |
| `BattleResult_ExpDone` | `0x431B30` | 0x24 | `0x801EEFD0` | `0x904AA3` on, `0x904AA4` and `0x904AA5` 0 |
| `BattleResult_LevelUpStep` | `0x431B60` | 0x11 | `0x801EF01C` | the stub, table `0x64AFC0` |
| `BattleResult_FindLevelUp` | `0x431B80` | 0x8D | `0x801EF058` | from slot `0x904AA5`: roster index `0x4469D0(id)` to `0x904AA7`; when `0x432170(index, 0)` answers a non-zero word, `0x904AE8 |= 0x20` and on (to `0x431C10`); none left: `0x904AA3` on |
| `BattleResult_RewardStep` | `0x431D50` | 0x11 | `0x801EF310` | the stub, table `0x64AFC8` |
| `BattleResult_Setup` | `0x431D70` | 0x26B | `0x801EF34C` (the sibling's `Setup` `0x801EF390`) | after a held button (and a new press after a level-up): `0x498DE0` per slot that levels; zenny x 1.5 when `0x431FE0`(); the window re-taken in state 5, `"%d"` of the zenny, `Msg_SystemPtr(6)`, tick step; the drop window and the drop sort; `Snd_LoadBankFile(0x904EFC + 3)` |
| `BattleResult_ZennyWaitHeld` | `0x432050` | 0x1E | `0x801EF780` | on when held, `0x904AA5` 0 |
| `BattleResult_ZennyTick` | `0x432070` | 0x79 | `0x801EF7B8` (the sibling's `ZennyTick` `0x801EF810`) | `ExpTick`'s shape through `Zenny_Add(n, 0)` `0x591BE0` |
| `BattleResult_AwardDrops` | `0x4320F0` | 0x74 | `0x801EF874` (the sibling's `AwardDrops`) | `Inventory_Add(cat, item, count, 0)` per drop; mode bytes `0x904AA1..A4` = 4, 0, 0, 0 |
| `BattleResultWin_ExpState` | `0x598570` | 0x26 | `0x801F065C` | window state 4's two steps |
| `BattleResultWin_DrawExp` | `0x5985A0` | 0x11A | `0x801F06D4` | the frame `0x5982D0(0x14, 0x28, 0x118, h)`; per slot the name, then at level 99 `Msg_SystemPtr(0x41)`, else `"%2d"` of level + 1, `"%6d"` of `0x598810(slot)` and `Msg_SystemPtr(0x15)` |
| `BattleResultWin_ZennyState` | `0x5986C0` | 0x26 | none | window state 5's two steps |
| `BattleResultWin_NextStep` | `0x5986F0` | 0x9 | none | the record's `+3` one on |
| `BattleResultWin_DrawZenny` | `0x598700` | 0x4A | none | the medium box, `"%7d"` of the party's zenny `0x904058`, the text `0x66A31C` |

The extents are the disassembly's, to the last instruction (capstone,
2026-09-25); the round's list gave upper bounds that ran into the padding.
All seventeen are reached by the combat route (the round's trace).

**Faithful details worth knowing:**

- **The drop sort** is an exchange sort (entry i against each later j,
  swapped when greater, equal items kept in order) of the u16 item words
  `0x904AF4` with the counts `0x904B14`, done with xor swaps. The count
  `0x904AE7` is re-read after each swap. Ours repeats the stores in the
  original's order, so a count past 16 - which the original would sort
  into the counts and beyond - comes out byte for byte the same.
- **Window slot 1 is freed and re-taken** by `BattleResult_Setup`: byte
  `+0` = 0 before `Window_Alloc(1, 4)`, state and step after. The fuzz's
  `Window_Alloc` stand-in behaves like the real one, which is what makes
  that order observable (controls U5, U6).
- **Register upper halves.** The original's frame height and line y in
  `BattleResultWin_DrawExp`, the party set for `PartySet_Select` and the
  drop's item and count for `Inventory_Add` carry the upper bits of
  whatever register was loaded. Every callee reads those arguments as a
  byte or a word (read 2026-09-25: `0x5982D0` hands them to
  `BattleWin_DrawQuadF4` / `DrawLine*`, which `movsx word` or byte them;
  `Text_DrawAt`, `Text_DrawFont12`, `PartySet_Select`, `Inventory_Add`
  are ours and mask), so ours passes them zero-extended; the fuzz's
  stand-ins record only the bits the callees read.
- **`Inventory_Add` is pushed four arguments** (the PSX's), the fourth 0;
  ours reads three. The four are passed through a raw-address pointer.

## 3. DIV-0045 (the EXP and zenny multipliers)

Unchanged and still working: the multiplication happens in
`Battle_EnemyDefeated` (`battle_flow.cpp`), when an enemy's yield is added
to `0x904AEC` / `0x904AF0`. Everything here reads those totals afterwards,
so the split, the ticks, the 1.5 x bonus and the level-ups all follow the
multiplied numbers. Nothing in this file patches or knows about the cheat.

One interaction, reported, not changed: the tick step is stored as a
**word** of total / 30. A share or zenny total of 1,966,080 or more
(30 x 65,536) wraps it - to a small step (a very slow count) or, for
totals within 30 of a multiple of 1,966,080, to **0**, which never ends the
tick. The original's yields cannot reach that (eight enemies of at most
65,535 each is 524,280 before the split), but DIV-0045's x50 makes it
arithmetically reachable for zenny and for large EXP. It is not a hang in
practice: holding any button through the tick (as `ExpWaitHeld` /
`ZennyWaitHeld` require to get there) takes the whole amount at once.
The coordinator may want a DIVERGENCE decision on whether to clamp the
step; this group changed nothing.

## 4. The fuzz and its negative controls

`BOF3X_SHADOW=battle_result` (`src/game/battle_result_fuzz.cpp`): 17
byte-copies, every call re-aimed at a recorder (`CloneCall` with
`expected`, 41 sites); the two window states' stack-table immediates
checked and re-aimed in the copies; the `.data` entries of `0x64AFC0` (2)
and `0x64AFC8` (4) checked against the original's and swapped for
recorders, and put back.

**Each round** starts from random bytes over (4.5 KB): the battle's
globals `0x904AA0..0x904F00` (the result's bytes, the drop list, the tick
step, the text records, `0x904EFC`), the party zenny, `0x90412C`,
`0x93B8E4`, `Input_Held` / `Input_Pressed`, eight party records from
`0x802DC0`, window records 0..0x15 and `0x905B84` (pointed at one of
them). Seeded boundaries: totals of 0, 1, 29..31, 59..61, the step and one
either side of it, and the step plus 2^31 (the signed test); step dwords
with upper bits; held and pressed on and off; the level-up flag; drop counts
0, 1, 2, 15, 16, 17 and 0x20 with equal items; levels 98 and 99; 14..16
party slots so that the line's y byte wraps; the window step 0 / 1.

**The recorders are not quiet.** Each moves a byte its caller reads again
after the call: the EXP total after `0x4319B0` and `0x4468B0`, the zenny
after `Zenny_Add`, `Msg_SystemPtr` and `BattleWin_DrawMediumBox`, the tick
step, the slot `0x904AA5` after `0x4469D0`, the party count after
`0x432170`, the frame and the name draw, the drop count after
`Window_Alloc` and `Inventory_Add`, a party record's id and level, the
party set after `Msg_SystemPtr`, the step byte after `PartySet_Select`.
`Window_Alloc`'s takes a free slot as the real one does. `sprintf`'s writes
a text of the value, and the draws record a hash of the text they are
given. A general disturber moves one of 22 watched bytes three calls in
four.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    battle_result self-test: 17000 rounds over 17 functions (1000 each), 55105 calls to the stand-ins, 0 MISMATCHES
    coverage: table entries 10 of 10; split zero 564 shared 436; ticks whole 1182 step 541 rest 277; level-up
      found 192 none 771, levelled 337; setups 568 (bonus 562, drop window 531, sorted 511, past 16 102);
      awards 3217; party-set 1000, sound banks 568, frames 1000, lines at 99 or messages 3695, y wrapped 75,
      zenny lines 1000

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1041 ours`, 241
self-tests logged and no mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`controls.py`, session scratchpad) planted
each one alone: replace, build, self-test, revert. The count is the rounds
that refused it, of 1,000.

| | Planted | Refused in |
|---|---|---|
| S1 | SplitExp: rounded down | 264 |
| S2 | SplitExp: the count asked for a total of 0 too | 536 |
| S3 | SplitExp: the total read before the count | 170 |
| S4 | SplitExp: the party set read before Msg_SystemPtr | 508 |
| S5 | SplitExp: the set masked 0xFF | 476 |
| S6 | SplitExp: the share stored after the sprintf | 44 |
| S7 | SplitExp: message 4 | 1000 |
| S8 | SplitExp: PartySet_Select mode 1 | 1000 |
| O1 | OpenExpWindow: height count * 16 | 1000 |
| O2 | OpenExpWindow: the count read before Window_Alloc | 431 |
| O3 | TickStep: 0 for 1..29 (no floor of 1) | OpenExpWindow 207, Setup 80 |
| O4 | OpenExpWindow: state 5 | 1000 |
| O5 | TickStep: stored as a dword | OpenExpWindow 1000, Setup 568 |
| W1 | ExpWaitHeld: Input_Pressed | 515 |
| W2 | ExpWaitHeld: the held byte | 258 |
| T1 | ExpTick: File_LoadDone asked first | 266 |
| T2 | ExpTick: an unsigned comparison | 126 |
| T3 | ExpTick: the step a dword | 239 |
| T4 | ExpTick: share and step not re-read | 159 |
| T5 | ExpTick: the step byte read before the call | 27 |
| T6 | ExpTick: >= 0 | 22 |
| D1 | ExpDone: 0x904AA5 kept | 549 |
| D2 | ExpDone: no File_LoadDone gate | 332 |
| F1 | FindLevelUp: the slot not re-read after 0x4469D0 | 217 |
| F2 | FindLevelUp: flag 0x10 | 195 |
| F3 | FindLevelUp: the search goes on after a find | 218 |
| F4 | FindLevelUp: 0x432170(index, 1) | 467 |
| F5 | FindLevelUp: the id byte +8 | 465 |
| F6 | FindLevelUp: the answer tested as a byte | 118 |
| F7 | FindLevelUp: 0x904AA3 not advanced at the end | 782 |
| L1 | RunStep: the entry index ^ 1 | LevelUpStep 1000, RewardStep 1000 |
| U1 | Setup: the Input_Pressed gate left out | 265 |
| U2 | Setup: the Input_Held gate left out | 118 |
| U3 | Setup: 0x498DE0 given the first index | 257 |
| U4 | Setup: the bonus a quarter | 230 |
| U5 | Setup: slot 1 freed after Window_Alloc | 567 |
| U6 | Setup: state and step set before Window_Alloc | 563 |
| U7 | Setup: the step from the total before Msg_SystemPtr | 218 |
| U8 | Setup: the drop window 12 per pair | 428 |
| U9 | Setup: the drop count read before its Window_Alloc | 261 |
| U10 | Setup: sorted descending | 473 |
| U11 | Setup: equal items swapped | 264 |
| U12 | Setup: the counts not moved | 434 |
| U13 | Setup: the count not re-read after a swap | **not refused; no input could tell** - no call and no store inside the sort can move `0x904AE7` |
| U14 | Setup: the bank + 2 | 568 |
| U15 | Setup: the drop window kind 3 | 531 |
| U16 | Setup: the swap on the original values (a plain swap) | **not refused; no input could tell** - items i and j never overlap, so a plain swap stores the same two words |
| Z1 | ZennyWaitHeld: 0x904AA5 kept | 390 |
| Y1 | ZennyTick: Zenny_Add(step, 1) | 218 |
| Y2 | ZennyTick: an unsigned comparison | 124 |
| Y3 | ZennyTick: the step byte read before the call | 36 |
| Y4 | ZennyTick: total and step not re-read | 102 |
| A1 | AwardDrops: category and item swapped | 605 |
| A2 | AwardDrops: the count read once | 507 |
| A3 | AwardDrops: mode 3 | 681 |
| A4 | AwardDrops: no File_LoadDone gate | 319 |
| E1 | ExpState: the entries swapped | 1000 |
| E2 | ZennyState: the EXP state's table | 1000 |
| N1 | NextStep: into +2 | 1000 |
| X1 | DrawExp: lines 15 apart | 655 |
| X2 | DrawExp: the cap at 98 | 494 |
| X3 | DrawExp: the level, not the next | 844 |
| X4 | DrawExp: 0x598810 before the level line | 844 |
| X5 | DrawExp: the count read once | 668 |
| X6 | DrawExp: the frame height from +8 | 995 |
| X7 | DrawExp: six characters of the name | 921 |
| X8 | DrawExp: the level read before the name | 109 |
| X9 | DrawExp: y not wrapped at a byte | 75 |
| X10 | DrawExp: the frame before the record is read (0x905B84 read after) | **not refused; no input could tell** - the height is read before the frame call either way (an argument) |
| M1 | DrawZenny: the zenny read before the box | 515 |
| M2 | DrawZenny: the amount at 0x71 | 1000 |
| M3 | DrawZenny: the unit at 0xC5 | 1000 |

**The thinnest:** T6 (22), T5 (27), Y3 (36), S6 (44), X9 (75), O3 (80).

**Three controls were blind in the first run** and are refused now: O3
(planted first as "1 up to 30", which is the same function - 30 / 30 is 1 -
and replaced by a real change), F6 (0, then 118, once `0x432170`'s
stand-in answered non-zero words with a zero low byte) and X9 (0, then 75,
once the stand-ins' moves of the party count stopped cutting the 14..16
slot lists short). U13, U16 and X10 are not counted.

**What the fuzz cannot see:**

- anything the callees really do (`0x4468B0`, `0x432170`, `0x498DE0`,
  `0x5982D0`, `0x598810` are unread beyond their arguments);
- the order of stores with no call between them;
- the register upper halves of section 2 (recorded as the callees read
  them);
- the level-up announcement `0x431C10` and the drop window's state
  `0x5984B0`, which are not ours.

## 5. Found on the way: what the result screen does with an item-0 drop

This answers `battle_flow.md` section 8's open question about its section 5
defect (a merged drop appends an extra `(item 0, count 1)` entry). Read
2026-09-25:

- `BattleResult_Setup` counts it: the drop window's height grows by 13 for
  every second entry, and the sort puts the word 0 first.
- `0x5984B0` (the drop window's draw, no group's) skips an entry whose
  word is 0 (`test cx, cx; je` at `0x5984F4`) but places every entry by its
  index (`bl`), so the real drops shift one place and **the first slot is
  left empty**.
- `BattleResult_AwardDrops` passes it on; `Inventory_Add` (ours,
  `char_stats.cpp`) returns 0 for item 0 and adds nothing.

So the defect is visible - an empty first slot and possibly one extra row
in the drop window - and harmless to the inventory. For
[`known-defects.md`](known-defects.md); the coordinator numbers it with
battle_flow's. Kept, as the original has it.

## 6. For the merger

**Left original, in our tables but not in the queue:**

- `0x431C10` (0x136 bytes, entry 1 of `BattleResult_LevelUpSteps`): the
  level-up announcement (`0x44A910`, `0x432170` per stat, `"%d"` into text
  record 1, `Msg_SystemPtr(7)`, window slot 1 state 0 with its height). No
  route reached it (no level-up on the combat route), so it has no live
  check; it goes with the next route that levels someone.
- `0x5984B0` (0xB2 bytes, state 1 of window kind 4): the drop list.

**Other groups' and no group's addresses**, called raw from
`battle_result_callees.h` and not bound:

- `0x4319B0` (the count of eligible members) and `0x431FE0` (the 1.5 x
  zenny test: a slot not out with 7 in byte `+0x16` or `+0x17` of its
  `0x802DC0` record - what 7 is was not read) are in BATL_END's range and
  in **no group**.
- `0x4468B0` (the sibling's `BattleResult_AddExp`), `0x4469D0`
  (`CharId_ToRosterIndex`), `0x432170`, `0x498DE0`, `0x591BE0`
  (`Zenny_Add` by its body), `0x5982D0`, `0x598810`: no group.
- The table `0x64AFA0` (3 entries, by `0x904AA3`) spans CC's `0x431920`
  and our two stubs, and is dispatched by CC's `0x431910`: **not named
  here**, left to CC (or the merger) so that it is not named twice.

**`analysis/calltrace/entries_logic.txt`**: the seventeen lines were
appended (2026-09-25, with a comment line). Two existing lines are too long
and overlap ours; the merger should correct them:

| Line now | Should be |
|---|---|
| `004319B0 62B` | `004319B0 61` (ret at `0x431A10`) |
| `005982D0 47A` | `005982D0 1D8` (ret at `0x5984A7`); `005984B0 B2` then needs a line of its own |

`00432170 679` was not checked.

**For the batch check:** the combat A/B reaches all seventeen. The
level-up paths, a drop list,
the 1.5 x zenny and the counts past 16 are fuzz only (no drop lands on the
combat route, [`battle_flow.md`](battle_flow.md) section 6).
