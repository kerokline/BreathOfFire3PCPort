# The battle menu states: the attack pick and the item command

**Status:** IN PROGRESS (2026-09-25). Fifteen functions are ours
(`src/game/battle_menu_states.cpp`), each fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 15,000 rounds, 0
mismatches. 74 negative controls were planted, and all 74 were refused by a count.

This is group CI of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Every function is a
*faithful* replacement, so no `DIVERGENCE.md` entry is owed. No Capcom defect
was found that shows in game (section 5 has the latent ones).

## 1. Where they sit

All fifteen are pointer-reached and entered by a tail jump. The chain from
the battle's frame, each link a `jmp`:

1. `Battle_PhaseDispatch` (ours) calls phase 1, `0x42E990`;
2. `0x42E990` jumps through `0x64AE28` by the command step `0x904AA1`; step
   4 is `0x42EED0`;
3. `0x42EED0` calls `0x446A10`, then jumps through `0x64AE54` by the command
   byte `0x904AA2`: entry 2 (`0x64AE5C`) is `BattleItemCmd_Dispatch`, entry 3
   (`0x64AE60`) `BattleAttackCmd_Dispatch`;
4. those two jump through their state tables by `0x904AA3`, and two of the
   states through sub-state tables by `0x904AA4`.

So every function here returns straight to `Battle_PhaseDispatch`, takes
nothing, and its eax is thrown away: `void (void)`, no result compared. The
tables `0x64AE28` and `0x64AE54` are group CA's; the four below are this
group's, named in `symbols.toml`:

| Table | Entries | Dispatched by | Holds |
|---|--:|---|---|
| `BattleAttackCmd_States` `0x64E44C` | 4 | `0x447FD0`, byte `0x904AA3` | `0x447FE0` `0x448020` `0x4480E0` `0x448140` |
| `BattleItemCmd_States` `0x64E45C` | 10 | `0x448180`, byte `0x904AA3` | `0x448190` `0x448210` `0x448600` `0x448630` `0x4486C0` `0x448B80` `0x448C80` `0x4493E0` `0x449480` `0x4498F0` |
| `BattleItemCmd_OpenSteps` `0x64E484` | 2 | `0x448190`, dword `0x904AA4` & 0xFF | `0x4481B0` `0x4481E0` |
| `BattleItemCmd_TargetSteps` `0x64E48C` | 5 | `0x4486C0`, dword `0x904AA4` & 0xFF | `0x4486E0` `0x448780` `0x4488C0` `0x448A70` `0x448B40` |

The four are contiguous (21 dwords, `0x64E44C..0x64E49F`); `0x64E4A0` starts
`0x448B80`'s table (`0x448BA0` `0x448C00` `0x448A70` `0x448B40`), which is
not this group's. None of the dispatches has a bound, and ours has none
either: an index past a table reads the next one's, as the original's does
(the tables are in `.data`, so this is the same memory, not a stack as in
`Battle_PhaseDispatch`, whose abort is the other choice). Each count is the
extent to the next table and the states the handlers set, not a bound in
code.

The byte scan for each table address finds exactly one reference, the
dispatch's own `jmp` (2026-09-25).

## 2. What each does

Read by linear disassembly to the last instruction (capstone, 2026-09-25);
every jump is internal, every call is listed in the fuzz's tables. The full
reading of each is the comment above it in the source.

| Function | Entry | Bytes | Calls |
|---|---|--:|--:|
| `BattleAttackCmd_Dispatch` | `0x447FD0` | 0xE | table |
| `BattleAttackCmd_Begin` | `0x447FE0` | 0x38 | 2 |
| `BattleAttackCmd_PickEnemy` | `0x448020` | 0xBC | 7 |
| `BattleAttackCmd_Confirm` | `0x4480E0` | 0x59 | 1 |
| `BattleItemCmd_Dispatch` | `0x448180` | 0xE | table |
| `BattleItemCmd_OpenDispatch` | `0x448190` | 0x11 | table |
| `BattleItemCmd_QueuePrompt` | `0x4481B0` | 0x27 | 2 |
| `BattleItemCmd_OpenList` | `0x4481E0` | 0x22 | 1 |
| `BattleItemCmd_List` | `0x448210` | 0x3E2 | 11 |
| `BattleItemCmd_TargetKind` | `0x448630` | 0x85 | 1 |
| `BattleItemCmd_TargetDispatch` | `0x4486C0` | 0x11 | table |
| `BattleItemCmd_TargetBegin` | `0x4486E0` | 0x93 | 4 |
| `BattleItemCmd_PickEnemy` | `0x448780` | 0x135 | 11 |
| `BattleItemCmd_PickMember` | `0x4488C0` | 0x1A5 | 15 |
| `BattleItemCmd_Commit` | `0x448A70` | 0xCD | 2 |

The PSX pairs the catalogue gave (`0x80095070` for `0x447FD0`,
`0x80095314` for `0x448180`) were not read.

**Command 3, the attack (a hypothesis in the names).** State 0 targets
`Battle_DefaultTarget(3)` (the first enemy not out), shows banner message 2,
sets the pick flag `0x904AAF` and zeroes `Input_AutoRepeat`'s latch word
`0x7E01B8`. State 1: a cancel button goes to state 3, a confirm button to
state 2 (cancel tested first); otherwise right and left on the auto-repeat
move the target through `Battle_WrapIndex(enemy count 0x904AB2 + 2, 3,
target +/- 1)` and `Battle_DefaultTarget` / `0x4457F0` (the downward picker),
cue `0x101`. State 2 commits: cue `0x104`, action kind +1 = 0, flags +0xC |=
1, the member's state 2, `0x904AC3` + 1, back to step 1. State 3 (cancel,
`0x448140`) was never reached on the route. That kind 0 with an enemy target
is the plain attack is inferred, not checked in game.

**Command 2, the item command.** The command record `*0x939FA0` is built as
+0 target, +1 action kind (5), word +2 = category << 8 | item, +0xA the list
row.

- *Open* (state 0): `QueuePrompt` pushes system message `0x4000` into the
  battle message queue; `OpenList` sets kind 5 and calls
  `ItemMenu_SetupForParty`, which fills window record 16 from the saved
  `0x904605..7`.
- *The list* (state 1, `0x448210`), window record 16 (`0x8033A0`): +0xA the
  category (0..3), +0xB the top row, +0xC the cursor row; seven rows show
  (the paging is by 7, the top stops at `0x79`, the cursor at `0x7F`). Once
  a frame it places the hand (record 19) at the cursor row, clears the
  member's `+0x130` bit 14 (and, by the 16-bit mask, its upper half), reads
  the auto-repeat of `pressed & 0xF00C`: left/right change the category
  (request word +0x10 = `0x32` / `0x31`), up/down move the cursor (request
  +0x12 = `0xF0` / `0x10` when the list must scroll), L1/R1 page. It then
  sets the last-pushed queue entry's text to
  `Msg_SystemPtr(Item_Price(category, item))` - the item's help line. With no
  scroll pending: up on row 0 opens record 21 and goes to state 6; cancel
  saves the page and cursor, cue `0x106`, state 2 (`0x448600`, left
  Capcom's); confirm asks `Item_CanUse(record 16 +8, 0, category, item)` -
  refused, cue `0x107`; accepted, cue `0x103`, state 3.
- *The target kind* (state 3, `0x448630`), from the item's flag byte
  `0x591810(category, item)`: bit 0x40 goes to state 4 or, with 0x10, 5
  (`0x448B80`'s table, a side toggle by its look, not read); bit 0x80 is
  target `0xC0` straight to the commit; bit 0x10 is target `0x40` (with
  0x20) or `0x80`; otherwise the member itself. The three target values
  `0x40`, `0x80`, `0xC0` are group codes by their use here; what each means
  to the action is not read.
- *The picks* (state 4 through `0x64E48C`): `TargetBegin` starts on the
  first enemy (flag 0x20) or on the party - target 0 while
  `Battle_ReturnTrue` answers 1, which it always does in this port
  ([`battle_misc.md`](battle_misc.md) §1.5). `PickEnemy` is the attack's
  pick, plus up/down crossing to the party when flag 0x80 allows it.
  `PickMember` wraps within 0 .. party count - 1 (`0x904AB0`), the target
  taken as the wrap gives it while `Battle_ReturnTrue` answers 1, and crosses
  back with flag 0x80.
- *The commit* (sub-state 3): cue `0x104`, kind 5, row +0xA, the member's
  state 2, `0x904AC3` + 1, the last queue entry's +1 = 1, and **the item is
  spent at the choice**: its count one less, or at 1 (or 0) its id and count
  both zeroed. Then `ItemMenu_FreeWindows` and back to step 1.

**Text.** None of the fifteen draws text. Three pass a message on:
`BattleAttackCmd_Begin` sets banner message 2 (`BattleBanner_SetMessage`,
the table `0x669DE0` the English overlay can repoint), `QueuePrompt`
queues `Msg_SystemPtr(0x4000)` (system bank 1, message 0) and `List` sets a
queue entry's text to `Msg_SystemPtr(the item's word)`. All three are drawn
by the banner and queue readers, and all come from the system pool or the
banner table the localisation already serves; for
[`dialogue-localisation.md`](dialogue-localisation.md) §6 the addresses are
`0x4481B0` (`push 0x4000`) and `0x448436` (the help line's `Msg_SystemPtr`
call).

## 3. Found on the way (other groups' addresses: said, not acted on)

- **`Item_Price` (`0x591C20`, `char_stats.cpp`) is a message id here.** Its
  word goes straight to `Msg_SystemPtr` as the list's help line. For
  consumables it reads record +0x12, which `NameTable_Consumables` calls the
  "ref" word (+0x14 is the price); its own evidence already notes that it
  does not read the price. The name looks misfiled: "the item's help
  message" fits both readers (the shop shows one too).
- **`0x939EC4`** points at an ObjTrio member here (`+0x125`, `+0x130` are
  written, `+5` read), where `battle_misc_callees.h` calls it "the acting
  actor's context" (the 0x84-byte records at `0x93A000`). A member record
  fits this group's reads; the context reading was not checked.
- **Record 16's +0xB** is the list's top row for the party window
  (`ItemMenu_SetupForParty`); `battle_misc` names `0x8033AB` "the inventory
  page" from the actor window's use (`ItemMenu_SetupForActor`). Both may be
  true of their own windows.
- **`0x4481B0`** is also the dword at `0x64E420`, inside group CH's table
  `0x64E3EC..`; CH's dispatch reaches this group's function there.
- **The catalogue's `.rdata 0x5C7001` / `0x5C7041` for `0x4481B0`** are not
  references and not strings: they are unaligned hits of the bytes `B0 81 44
  00` inside two 16-float records at `0x5C7000` and `0x5C7040` (the float
  `0x4481B000` is 1037.5). Nothing formats there.
- **Neighbours left Capcom's:** `0x448140` (the attack's cancel) and
  `0x448600` (the list closed: step 1, then a tail jump to
  `ItemMenu_FreeWindows`) are in this group's tables but not in the round's
  queue - the route never reached them - so they stay original and are
  called through the tables like any other entry. `0x448B40`, `0x448B80`,
  `0x448BA0`, `0x448C00`, `0x448C80` and the table `0x64E4A0` are the same.
- **`analysis/calltrace/entries_logic.txt`** has `00447F40 1AB3`: the host
  function `0x447F40` (0x8C bytes, to `0x447FCB`) is listed as running on
  through all fifteen of these and past `0x4498F0`. The fifteen are now
  listed with their own extents (section 6); the merger should cut the host
  line to `8C` and judge what else in `0x447FD0..0x4499F3` wants a line.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_menu_states`
(`src/game/battle_menu_states_fuzz.cpp`). Fifteen byte-copies, every call
out re-aimed at a recorder with `CloneCall` and `expected`. The four tables'
21 entries are checked against what they hold and pointed at recorders for
the run (both sides dispatch through the same memory), then put back.

**Each round** starts from random bytes over these (3,248 bytes, and the
64 of the two command records):

- the step bytes `0x904AA0..0x904ACF`;
- the inventory's id and count lists and the saved list bytes
  (`0x904154..0x90465F`);
- window records 2..21 (`0x8031A0..0x80347F`);
- the message queue and its indices (`0x93C2A0..0x93C33F`);
- ObjTrio; the pointers `0x939FA0` and `0x939EC4`; the latch word; the
  pressed word; the confirm and cancel words;
- two command records of the fuzz's own, which `0x939FA0` points at.

Then the pointers are put back (the command at one of its two records, the
acting member at one of three, each member's actor byte 0..2) and the
category inside 0..3. Seeded per function: dispatch indices inside the 21
entries (and past their own table); the pad with neither, one or both of
confirm and cancel sharing its bits; targets at the wraps' edges (0..5, 9,
10, `0x7F`, `0x80`, `0xFE`, `0xFF`); enemy and party counts small; the list's
top at 0, 1, 6..8, `0x72`, `0x73`, `0x78..0x7A`, `0xFF`, the cursor at 0, at
the top's edges and at `0x7E..0x80`; the commit's count at 0, 1, 2, 3,
`0xFF`.

**The recorders are not quiet.** Three calls in four move one of 20 watched
bytes: the state and sub-state, the command pointer (to the other record)
and its target, the category, top and cursor, the pad words, the queue
index, the counts, record 16's x / y and requests, the member pointer, the
list's mode and done byte, and the inventory byte under the cursor. The
auto-repeat's recorder answers single direction bits mostly, pairs, nothing
and anything.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    battle_menu_states self-test: 15000 rounds over 15 functions (1000 each), 25415 calls to the stand-ins, 0 MISMATCHES
    coverage: table entries 21 of 21; default targets 2291, previous 346, wraps 872, banners 1000, repeats 2875,
      prompts 1000, set-ups 1000, frees 1000, help lines 852, use asks 181, flags 2711, return-trues 926; cues 100 491
      101 1581 103 125 104 2000 106 256 107 56; list: above 19, scroll 275, refused 56, confirmed 125, cancelled 256;
      kinds: by 0x40 274, state 5 230, the rest 496; picks crossed or left 1267; commit spent 775, cleared 225

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1039 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`ci_controls.py`, session scratchpad)
planted each one alone in `battle_menu_states.cpp`: replace, build,
self-test, revert. The table gives the rounds that refused each, of 1,000
(two numbers where the planted code is shared).

| | Planted | Refused in |
|---|---|---|
| A1 | AttackDispatch: the item table | Dispatch 1000 |
| A2 | AttackDispatch: by the sub-state | Dispatch 834 |
| I1 | ItemDispatch: by the sub-state | Dispatch 913 |
| O1 | OpenDispatch: by the state | OpenDispatch 859 |
| T1 | TargetDispatch: the open table | TargetDispatch 1000 |
| B1 | Begin: Battle_DefaultTarget(0) | Begin 1000 |
| B2 | Begin: the banner (2, 1) | Begin 1000 |
| B3 | Begin: the state read before the banner | Begin 490 |
| B4 | Begin: the latch not zeroed | Begin 1000 |
| B5 | Begin: the command pointer read before the call | Begin 263 |
| P1 | AttackPick: confirm tested first | PickEnemy 206 |
| P2 | AttackPick: the pad masked 0xF00C | PickEnemy 498 |
| P3 | StepEnemyTarget: the right wrap low 2 | PickEnemy 199, PickEnemy 135 |
| P4 | StepEnemyTarget: the left wrap high count + 3 | PickEnemy 191, PickEnemy 123 |
| P5 | StepEnemyTarget: left through Battle_DefaultTarget | PickEnemy 191, PickEnemy 123 |
| P6 | StepEnemyTarget: left only without right | PickEnemy 91, PickEnemy 53 |
| P7 | StepEnemyTarget: the target unsigned | PickEnemy 66, PickEnemy 53 |
| P8 | StepEnemyTarget: the target stored through the pointer of before the call | PickEnemy 66, PickEnemy 36 |
| C1 | AttackConfirm: +0xC /= 2 | Confirm 754 |
| C2 | AttackConfirm: the count not raised | Confirm 1000 |
| C3 | AttackConfirm: action kind 1 | Confirm 1000 |
| Q1 | QueuePrompt: message 0x4001 | QueuePrompt 1000 |
| Q2 | QueuePrompt: pushed (1, 0xFF) | QueuePrompt 1000 |
| Q3 | QueuePrompt: the sub-state not moved | QueuePrompt 1000 |
| N1 | OpenList: action kind 4 | OpenList 1000 |
| N2 | OpenList: the state read before the set-up | OpenList 515 |
| L1 | List: done tested on +2 | List 847 |
| L2 | List: the hand at x + 8 | List 852 |
| L3 | List: rows 12 apart | List 852 |
| L4 | List: +0x130 &= 0xFFFFBFFF | List 852 |
| L5 | List: the pad masked 0xF000 | List 635 |
| L6 | List: right wraps at 3 | List 24 |
| L7 | List: requests 0x31 and 0x32 swapped | List 181 |
| L8 | List: the category read before the left cue | List 54 |
| L9 | List: up below row 0 wraps | List 32 |
| L10 | List: down to 0x80 | List 7 |
| L11 | List: the down scroll at top + 6 | List 5 |
| L12 | List: L1 pages of 8 | List 32 |
| L13 | List: R1 top capped at 0x78 | List 12 |
| L14 | List: no cue for a move | List 472 |
| L15 | List: the help line to the entry at the write index | List 852, Commit 1000 |
| L16 | List: Item_Price with the arguments swapped | List 850 |
| L17 | List: the scroll request not tested | List 197 |
| L18 | List: up at the top to state 5 | List 19 |
| L19 | List: up at the top whatever the row before | List 11 |
| L20 | List: the cancel saves top and cursor swapped | List 230 |
| L21 | List: the cancel leaves +0x125 | List 256 |
| L22 | List: the confirm word item << 8 | category |
| L23 | List: Item_CanUse mode 2 | List 181 |
| L24 | List: the confirm to state 4 | List 125 |
| L25 | List: the pad not re-read after the calls | List 25 |
| K1 | TargetKind: by 0x40 always state 4 | TargetKind 230 |
| K2 | TargetKind: all is 0x80 | TargetKind 232 |
| K3 | TargetKind: 0x40 and 0x80 swapped | TargetKind 128 |
| K4 | TargetKind: self from +4 | TargetKind 135 |
| G1 | TargetBegin: the enemy side by bit 0x10 | TargetBegin 481 |
| G2 | TargetBegin: Battle_ReturnTrue inverted | TargetBegin 512 |
| G3 | TargetBegin: the pick flag left for the party | TargetBegin 510 |
| E1 | ItemPick: crossing on up only | PickEnemy 125 |
| E2 | ItemPick: the sub-state read after the cue | PickEnemy 99 |
| E3 | ItemPick: the crossing goes on to left and right | PickEnemy 107 |
| M1 | PickMember: the party wrap to the count | PickMember 171 |
| M2 | PickMember: the party count as a byte of 0x904AB1 | PickMember 170 |
| M3 | PickMember: the crossing raises the sub-state | PickMember 88 |
| M4 | PickMember: left through Battle_DefaultTarget | PickMember 32 |
| M5 | PickMember: right low 3 | PickMember 77 |
| M6 | PickMember: Battle_ReturnTrue asked after the reads | PickMember 38 |
| X1 | Commit: action kind 4 | Commit 1000 |
| X2 | Commit: spent to nothing at 2 | Commit 124 |
| X3 | Commit: the id kept | Commit 227 |
| X4 | Commit: the queue entry not marked | Commit 995 |
| X5 | Commit: to the command step 0 | Commit 1000 |
| X6 | Commit: the row not stored | Commit 991 |
| X7 | Commit: the windows freed before the item is spent | Commit 87 |

**All 74 were refused by a count** (exit 3). **In the first run** three
dispatch controls (A2, I1, O1: the index from the other step byte) crashed
instead, the wrong byte running past the swapped entries into Capcom's code;
the fuzz now seeds the other step byte inside the tables too. Seven were thin
until the recorders were given the side effect their caller reads back (the
command pointer after the pickers, `Battle_WrapIndex` and
`Battle_ReturnTrue`; the state after the banner and the set-up; the category
and sub-state after a cue): P8 went from 5 / 2 to 66 / 36, B3 34 to 490, B5
26 to 263, N2 34 to 515, L8 7 to 54, E2 7 to 99, M6 4 to 38.

**The thinnest now:** L11 (5), L10 (7), L19 (11), L13 (12) - each needs
a cursor or a top at one edge and one direction bit together.

**What the fuzz cannot see:**

- anything the callees really do, and what the states it hands on to
  (`0x448140`, `0x448600`, `0x448B40`.., the queue's reader) do;
- the order of stores with no call between them;
- the upper bits of the arguments the original passes to `0x591810`,
  `Item_Price` and `Item_CanUse` (the caller's registers; all three read the
  low byte, and the recorders record only it).

## 5. Latent, as the original has it

- The four dispatches have no bound (section 1).
- The list's category is not checked before `0x656B00[category]`; left and
  right keep it inside 0..3, and `ItemMenu_SetupForParty` restores what the
  list saved. At category 4 the commit would write through `0x656B14[4]`,
  which is 0.
- The item is spent when the command is chosen, not when it is used; what
  happens to it if the turn never comes (the battle ends first) was not
  read. This is the port's order; whether the PSX does the same was not
  checked.

## 6. For the batch check

The combat A/B (`analysis/validate_combat.sh`) reaches all fifteen (the
first-call trace of 2026-09-25). Added to
`analysis/calltrace/entries_logic.txt` (not committed; `/analysis/` is
ignored), with the extents read:

    00447FD0 E
    00447FE0 38
    00448020 BC
    004480E0 59
    00448180 E
    00448190 11
    004481B0 27
    004481E0 22
    00448210 3E2
    00448630 85
    004486C0 11
    004486E0 93
    00448780 135
    004488C0 1A5
    00448A70 CD

## 7. Open

- What the target codes `0x40`, `0x80`, `0xC0` mean to the action.
- What states 5 and 6 of the item command (`0x448B80`, `0x448C80`) and the
  queue's reader do.
- Whether command 3 is the attack (the owner's eye on a fight).
