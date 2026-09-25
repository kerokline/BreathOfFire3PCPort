# The shop's buy and sell states, the shop's close, task 0's title loop

**Status:** IN PROGRESS (2026-09-25)

Group DG of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Twenty-four
functions are ours (`src/game/shop_states2.cpp`). Each is fuzzed headless
against a copy of Capcom's with every call re-aimed at a recorder: 24,000
rounds, 0 mismatches. 121 negative controls were planted: 116 were refused by a count, 4 by a crash of the self-test, and one was a change that changes nothing. Every function is a *faithful*
replacement, so no `DIVERGENCE.md` entry is owed. Two of the group's 26
addresses are left: they are switch cases of functions already ours
(section 6).

The shop's buy and sell states are the PSX `SHOP.EMI`'s state machine
compiled into the exe (the catalogue's label "Shop / inn / save point").
`pe_funcs.py` had folded them into the recorded start `0x581720`, because
nothing calls them: they are reached only through the tables at
`0x664118..0x66418C` in `.data`. The two title functions sit after
`Save_ReadSummaries` and were folded into it the same way; they have
nothing to do with the summaries - task 0 is created on `0x588E70`.

## 1. What is ours

Each extent was read to its last instruction with capstone (2026-09-25).
"Slot" is the table entry the function is reached through (section 2). The
PSX twins are the catalogue's pairing (`analysis/remaining_catalog.tsv`),
not read here.

| Function | Entry | Bytes | Slot | Calls out | PSX |
|---|---|--:|---|--:|---|
| `ShopTrade_Step` | `0x5818B0` | 0xE | `0x663E40` [2] (group DF's) | jump | |
| `ShopTrade_OpenStep` | `0x5818C0` | 0xE | `ShopTrade_States` [0] | jump | |
| `ShopTrade_Open` | `0x5818D0` | 0x9D | `ShopTrade_OpenSteps` [0] | 4 | `0x801D1154` |
| `ShopTrade_OpenWait` | `0x581970` | 0x3D | `ShopTrade_OpenSteps` [1] | 0 | |
| `ShopTrade_ChoiceStep` | `0x5819B0` | 0xE | `ShopTrade_States` [1] | jump | |
| `ShopTrade_Choice` | `0x5819C0` | 0x11C | `ShopTrade_ChoiceSteps` [0] | 5 | |
| `ShopTrade_BuyStep` | `0x581AE0` | 0xE | `ShopTrade_States` [2] | jump | |
| `ShopTrade_BuySetup` | `0x581AF0` | 0xE8 | `ShopTrade_BuySteps` [0] | 2 | |
| `ShopTrade_BuyList` | `0x581BE0` | 0x2ED | `ShopTrade_BuySteps` [1] | 15 | `0x801D1634` |
| `ShopTrade_BuyCount` | `0x581ED0` | 0x1B4 | `ShopTrade_BuySteps` [2] | 6 | `0x801D1A88` |
| `ShopTrade_BuyConfirm` | `0x582090` | 0x147 | `ShopTrade_BuySteps` [3] | 7 | `0x801D1D58` |
| `ShopTrade_BuyAskEquip` | `0x5821E0` | 0xDF | `ShopTrade_BuySteps` [4] | 5 | `0x801D1F74` |
| `ShopTrade_BuyMember` | `0x5822C0` | 0x205 | `ShopTrade_BuySteps` [5] | 11 | `0x801D20D8` |
| `ShopTrade_BuySlot` | `0x5824D0` | 0x1CD | `ShopTrade_BuySteps` [6] | 11 | `0x801D23EC` |
| `ShopTrade_BuyEquipped` | `0x5826A0` | 0xCE | `ShopTrade_BuySteps` [7] | 5 | `0x801D26B0` |
| `ShopTrade_SellStep` | `0x582770` | 0xE | `ShopTrade_States` [3] | jump | |
| `ShopTrade_SellSetup` | `0x582780` | 0x6B | `ShopTrade_SellSteps` [0], `ShopSell_SellSteps` [0] | 0 | |
| `ShopTrade_SellList` | `0x5827F0` | 0x366 | `..SellSteps` [1] (both) | 11 | |
| `ShopTrade_SellCount` | `0x582B60` | 0x192 | `..SellSteps` [2] (both) | 5 | |
| `ShopTrade_SellConfirm` | `0x582D00` | 0x177 | `..SellSteps` [3] (both) | 5 | |
| `ShopTrade_SellClose` | `0x582E80` | 0x24 | `..SellSteps` [4] (both) | 0 | |
| `ShopTrade_Close` | `0x584F70` | 0x1B | `ShopTrade_States` [4], `0x664254` [4] | 1 | `0x801E0E08` |
| `TitleTask_Run` | `0x588E70` | 0x3A | task 0's entry (`.text 0x462407`) | 4 + the mode table | |
| `TitleMode_Load` | `0x588EB0` | 0x44 | `TitleTask_Modes` [0] | 5 | |

The shop route reaches all 22 shop functions; every route reaches the two
title functions (task 0 starts the title on each of them).

What each function does is written above it in `shop_states2.cpp`, with
every read and store in the order the original makes it. The short version:

- **The state machine.** `0x929F01` is the shop's state, `0x929F02` the
  state's step, `0x929F04` a frame counter; each step returns and the field
  menu's frame calls `ShopTrade_Step` again. State 0 opens (the windows,
  the "sells equipment" byte `0x6BC8A4`, the price rate `0x6BC8B4`), 1 is
  the buy / sell choice (`0x6BC8AB`), 2 buys, 3 sells, 4 closes
  (`Window_ResetAll`, `0x929F00 = 1`).
- **Buying** (eight steps): the list (the shop's list at `[0x903844]`, a
  count and then category / item byte pairs), the count, "buy?", then - for
  equipment - "equip it?", the member, the slot, and the new figures. The
  price is `Shop_ScalePrice(Item_BasePrice & 0xFFFF, rate)`; the most that
  can be bought is the money divided by it, cut to 99 less what is held
  (carried plus worn, as bytes).
- **Selling** (five steps): four category tabs over the inventory's lists
  (`0x656B00` ids, `0x656B14` counts, 128 each), a top row and a row, a page
  of nine; the count; "sell?" - the money capped at 9,999,999 and an
  emptied stack's id cleared; then back to the choice.
- **The help line.** Every step puts a system message id in `0x803170`
  (the window record `0x803160`'s `+0x10`): `0x46` / `0x47` buy / sell,
  `0x48` / `0x4E` the count, `0x49` "buy?", `0x4A` "equip it?", `0x4B` on
  whom, `0x4C` which slot, `0x3A` the slot, `0x4D` equipped, `0x51` the
  sell count, `0x52` "sell?"; the list steps put `Item_Price`'s word there
  (section 7). The yes / no steps and the member / slot steps copy the item's
  name into text record 0 first (`TextRecord_Set(0, 0x10, Item_NamePtr)`).
  **None of these functions draws text**: the help window (`0x803160`) and
  the lists are drawn elsewhere, so there is nothing here for
  [`dialogue-localisation.md`](dialogue-localisation.md) section 6.
- **Task 0's title loop.** `TitleTask_Run` zeroes `Game_Mode` and
  `Game_Step`, clears the task's private words and the windows, then for
  ever sleeps a frame, calls `TitleTask_Modes[Game_Mode]` and runs the task
  records. Mode 0 (`TitleMode_Load`) loads DAT file `0x31D`, waits for it,
  restores the CLUT strip and moves to mode 1, `TitleFlow_Step` (group X's,
  [`save-menu.md`](save-menu.md)). The task leaves only through
  `Task_Restart` (`TitleFlow_EnterGame`).

**Cheats and divergences.** None patches these addresses:
`grep -rn` of every entry over `src/game/*.cpp` finds only
`title_states.cpp`'s `kMenuEntry = 0x588E70` (the task entry
`Title_StateMenu` creates, which is our `TitleTask_Run` now). DIV-0002 and
DIV-0040 (saves), DIV-0018 / DIV-0027 / DIV-0030 (menus) and DIV-0045 (the
zenny multiplier, applied at `Battle_EnemyDefeated`) touch none of them.

## 2. The tables named

All `[[data]]`, `ctype = "unsigned long"`, in `symbols.toml`. Every
dispatch is unchecked in the original.

| Name | At | Count | Indexed by | Through |
|---|---|--:|---|---|
| `ShopTrade_States` | `0x664118` | 5 | `0x929F01` | `ShopTrade_Step` |
| `ShopTrade_OpenSteps` | `0x66412C` | 2 | `0x929F02` | `ShopTrade_OpenStep` |
| `ShopTrade_ChoiceSteps` | `0x664134` | 1 | `0x929F02` | `ShopTrade_ChoiceStep` |
| `ShopTrade_BuySteps` | `0x664138` | 8 | `0x929F02` | `ShopTrade_BuyStep` |
| `ShopTrade_SellSteps` | `0x664158` | 5 | `0x929F02` | `ShopTrade_SellStep` |
| `ShopSell_States` | `0x66416C` | 4 | `0x929F01` | `0x582EB0` (not ours) |
| `ShopSell_SellSteps` | `0x66417C` | 5 | `0x929F02` | `0x582FF0` (not ours) |
| `TitleTask_Modes` | `0x667294` | 2 | `Game_Mode` | `TitleTask_Run` |

`0x66416C` and `0x66417C` belong to a second menu kind: `0x582EB0` is entry
8 of the field menus' table `0x663E40` (`0x663E60`), and its state 2
(`0x582FF0`) runs our five sell steps through the second copy of their
table. None of its own four states (`0x582EC0`, `0x582FB0`, `0x582FF0`,
`0x583000`) is in any round-8 group; the shop route does not reach them.

**Ours reads each table at run time**, as the original does, and jumps to
the entry (a `musttail` jump: the handler returns to our caller). The index
is not checked: past its table the next table's words are read, as in the
original. A null entry would be a call to 0 in the original; ours aborts
(rule 4). The only null reachable that way is `TitleTask_Modes` [2] - a
`Game_Mode` of 2 while task 0 runs the title, which the title flow never
sets.

## 3. Registers pushed whole

The originals push a byte loaded into `al` / `cl` / `dl` over whatever the
register held - a callee's return, a list pointer, the function's entry
`eax`. Ours pass the byte zero-extended. That is safe only because every
callee reads the byte, and nothing more, of each such argument:

| Callee | Reads |
|---|---|
| `Item_IconKind`, `Item_Price`, `Item_BasePrice`, `Item_NamePtr`, `Item_EquipMask` | category and item `& 0xFF` (char_stats.cpp, menu_windows.cpp) |
| `Inventory_Count`, `Inventory_Add` | category, item, count / flag `& 0xFF` |
| `Item_CanUse` | mode, category, item `& 0xFF`; the member `& 31` (a constant 0 here) |
| `Shop_Equip` | record, slot `& 0xFF`; the item goes whole to `0x591B60`, which reads its byte (`mov bl, [esp + 0xC]`), and to a byte store |
| `Shop_SellPrice` | kind, flag `& 0xFF`; the item whole to `Item_BasePrice` (byte) and `& 0xFF` |
| `Shop_ScalePrice` | the rate `& 0xFFFF` (the original pushes `edx` with `dx` = the rate over a list pointer) |
| `Party_Count`, `TextRecord_Set` | slot / length `& 0xFF` |

The returns are read as the originals read them: `Item_IconKind`'s and
`Party_Count`'s low byte, `Item_EquipMask`'s low byte against the member
bit, `Inventory_Count`'s word or byte, `Item_BasePrice`'s low word,
`Shop_ScalePrice`'s whole dword. The fuzz's recorders return garbage in
the bits the original ignores, and record only the bits the real callee
reads.

The word stores of window coordinates are computed by the originals in
32-bit registers whose upper halves are undefined (`movsx ax`, `mov cl`)
and stored as their low words; ours compute the same low words.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=shop_states2`
(`src/game/shop_states2_fuzz.cpp`). It makes 24 byte-copies, every `E8` out
of each extent re-aimed at a recorder (`CloneCall` with `expected`), and
for the time of the test points:

- the thirty dwords `0x664118..0x66418F` (all five step tables and the two
  after them) and `TitleTask_Modes`' two at recorders;
- the shop's list pointer `0x903844` 0x100 into a buffer of its own (rows
  -128..127, times two, stay inside);
- the ten inventory list pointers `0x656B00` / `0x656B14` 0x80 into
  buffers of their own (rows -128..255).

Everything is put back afterwards.

**Each round** starts from random bytes over the menu block
`0x929F00..0x929F1F`, the shop's bytes `0x6BC880..0x6BC8BF`, the window
records `0x803160..0x80347F`, the list pointer, the money and the party list
`0x904058..0x904067`, the input words, the pad map, `Game_Mode` /
`Game_Step`, the CLUT dirty byte, `0x66972C` and the list pointers - and the
three buffers. The pointers are put back on the buffers, categories 0..4,
the tab 0..3. Each branch's boundaries are seeded:

- the dispatch indices inside their tables two rounds in three, past them
  (inside the thirty swapped dwords) the third;
- the buttons: a pressed word hitting confirm, cancel, both, neither, or
  only its high half;
- counters 0, 1, 2, 5, 0xFF; counts 0, 1, 2, 9..11, 0x7F, 0x80, 0xFF
  against a most of 0, 1, 2, 10, 11, 20, 99, 0x80, 0xFF, 0x100 or anything;
- the money under 30,000 (a quotient near 0 and near 99) and within 3,000 of
  9,999,999; a stack whose count equals the count sold;
- the sell list's top 0, 1, 5, 8, 9, 10, 0x6E..0x70, 0x76..0x78 with rows
  on either side of the page, 0x7E..0x80 and 0xFF;
- the title loop cut after 1..4 sleeps; the file load done after 0..3 polls.

**The recorders are not quiet.** Each moves, three calls in four, one of
27 bytes the functions read again after a call - the state and step, the
counter, the member, the answer, the category, item, row, count, most,
price, money, the pressed dword, the list pointer (between two places), a
list byte, the tab, top and row, the scroll word, the windows' open byte,
the equipment byte, `Game_Mode`, the dirty byte, the slot cursor, a stack
count, the rate, the choice, Item_CanUse's mode. The sound recorder also
moves the state, the step or the choice in a quarter of its calls. Their returns are chosen at the edges
(icon kinds 5, 0xA, 0xB; held counts near 99; `Party_Count` 0..3 and 6
under garbage; `Input_AutoRepeat` one tested bit, or anything; a scaled
price of a few zenny, never 0 - as `Shop_ScalePrice`). And every recorder
logs a hash of all the bytes the functions store, so a store moved across a
call shows even when the callee does not read it.

The title loop never returns: the `Task_Sleep` recorder long-jumps out of
both copies after the seeded number of sleeps (the saved-register jump
`mode_flow_fuzz.cpp` uses).

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=shop_states2`,
exit 0:

    shop_states2 self-test: 24000 rounds over 24 functions (1000 each), 73010 calls to the stand-ins, 0 MISMATCHES
    coverage: buy list held 99+ 239, room cut 169, afforded 357; counts: double sound 1391, to 1 741,
      to the most 502; sell: money capped 258, stack emptied 95, page up 63, page down 64, scroll 113,
      tabs 607, not sellable 60; title loop cut 1000, load waited 726

The per-function coverage lines give the step moves each branch makes
(`BuyList` +0/+1 and +0/+0, `BuyCount` -1 / 0 / +1, `BuyConfirm` +1 / 0 /
-2, `BuyAskEquip` +1 / 0 / -3, `BuyMember` +1 / 0 / -4, `BuySlot` +1 / 0 /
-1, `BuyEquipped` 0 / -2 / -6, `SellList` +3 / +1 / 0, `SellCount` +1 / -1
/ 0, `SellConfirm` 0 / -2) and 6..16 distinct sound sequences for each
step that plays sounds.

With `BOF3X_SHADOW='*'` the run exited 0 with `inject: 1286 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`DG/controls.py` in the session
scratchpad) planted each one alone: replace, build, self-test, revert. The
table gives the rounds that refused each one, of 1,000 for its function.

| | Planted | Refused in |
|---|---|---|
| D1 | Step: the state table by the step byte | a crash (exit 0xC0000005): the wrong index reached a word past the swapped thirty |
| D2 | OpenStep: the table one entry on | a crash (exit 0xC0000005): the wrong index reached a word past the swapped thirty |
| D3 | BuyStep: the index one on | a crash (exit 0xC0000005): the wrong index reached a word past the swapped thirty |
| D4 | SellStep: by the state byte | a crash (exit 0xC0000005): the wrong index reached a word past the swapped thirty |
| D5 | ChoiceStep: always entry 0 (the index checked) | 310 |
| O1 | Open: the counter 5 | 1000 |
| O2 | Open: the step read before Shop_InitWindows | 17 |
| O3 | Open: the list pointer not re-read after the call | 7 |
| O4 | Open: icon kind 0xB counted as equipment | 162 |
| O5 | Open: the category not re-read after the call | 2 |
| O6 | Open: the icon kind a word | 281 |
| O7 | Open: the rate two bytes on | 1000 |
| W1 | OpenWait: 0x929F05 kept | 348 |
| W2 | OpenWait: 0x6BC8A9 cleared for 0x6BC8A8 | 350 |
| C1 | Choice: help 0x45 + choice | 1000 |
| C2 | Choice: the hand 36 apart | 692 |
| C3 | Choice: the flip sound 0x100 | 606 |
| C4 | Choice: the choice read before the sounds | 31 |
| C5 | Choice: cancel to state + 2 | 234 |
| C6 | Choice: cancel sets 0x8031AA for 0x8031AB | 234 |
| S1 | BuySetup: panels 56 apart | 418 |
| S2 | BuySetup: 0x803283 inverted | 1000 |
| S3 | BuySetup: the rate's high byte | 998 |
| S4 | BuySetup: panel +0xA the index + 1 | 599 |
| L1 | BuyList: panel +0xB the item | 879 |
| L2 | BuyList: the item not re-read after Item_IconKind | 98 |
| L3 | BuyList: the hand y + 0xB | 653 |
| L4 | BuyList: up wraps to the count | 98 |
| L5 | BuyList: the down wrap unsigned | 32 |
| L6 | BuyList: nothing re-read after the move sound | 12 |
| L7 | BuyList: 99 held is full | 1 |
| L8 | BuyList: held as a word | 139 |
| L9 | BuyList: the count 1 when the room is 0 | 15 |
| L10 | BuyList: the money not re-read for the test | 3 |
| L11 | BuyList: the most compared signed | 2 |
| L12 | BuyList: the rate a byte | 413 |
| L13 | BuyList: the base price not masked | 417 |
| L14 | BuyList: cancel to state 2 | 242 |
| L15 | BuyList: the second count equipped 2 | 417 |
| L16 | BuyList: the list pointer to 0x803478 | 653 |
| N1 | BuyCount: the help lines swapped | 1000 |
| N2 | StepCount: the second sound against the count after the first step | BuyCount 506, SellCount 491 |
| N3 | StepCount: the first low clamp unsigned | BuyCount 159, SellCount 157 |
| N4 | StepCount: the most not re-read after the sound | BuyCount 2, SellCount 10 |
| N5 | StepCount: nine up | BuyCount 112, SellCount 129 |
| N6 | StepCount: the most a dword | BuyCount 87, SellCount 92 |
| N7 | BuyCount: a count of 0 accepted | 67 |
| N8 | BuyCount: cancel back two | 247 |
| N9 | BuyCount: the window y + 0x2E | 1000 |
| B1 | BuyConfirm: the whole count dword paid for | 305 |
| B2 | BuyConfirm: one more added | 305 |
| B3 | BuyConfirm: category 0 offered to equip | 52 |
| B4 | BuyConfirm: the answer not set before equipping | 80 |
| B5 | BuyConfirm: no goes back one | 373 |
| B6 | YesNoFrame: the pressed dword not re-read after the flip | BuyConfirm 9, BuyAskEquip 11, SellConfirm 9 |
| B7 | YesNoFrame: the hand 32 apart | BuyConfirm 647, BuyAskEquip 656, SellConfirm 669 |
| B8 | YesNoFrame: only 0x8000 flips | BuyConfirm 186, BuyAskEquip 197, SellConfirm 217 |
| A1 | AskEquip: the member 1 | 264 |
| A2 | AskEquip: no goes back two | 364 |
| M1 | BuyMember: the hand from panel +8 | 738 |
| M2 | BuyMember: Party_Count after the repeat | 1000 |
| M3 | BuyMember: the record bit & 7 | 147 |
| M4 | BuyMember: the member read before Item_EquipMask | 4 |
| M5 | BuyMember: the panel marks swapped | 51 |
| M6 | BuyMember: the slot box names the next member | 57 |
| M7 | BuyMember: cancel back three | 253 |
| M8 | BuyMember: the counter 6 | 57 |
| M9 | BuyMember: the step read before the sound | 5 |
| T1 | BuySlot: the two-slot kind 4 | 242 |
| T2 | BuySlot: the slot cursor + kind | 406 |
| T3 | BuySlot: the record not re-read for Char_RecalcStats | 8 |
| T4 | BuySlot: the hand a row lower | 1000 |
| T5 | BuySlot: only 0x4000 flips the slot | 32 |
| T6 | BuySlot: cancel marks swapped | 213 |
| T7 | BuySlot: help 0x3B | 862 |
| E1 | BuyEquipped: back five | 37 |
| E2 | BuyEquipped: the hand kept when nothing is pressed | 539 |
| E3 | BuyEquipped: 0x80323B after the count | 457 |
| E4 | BuyEquipped: the equipped count | 460 |
| P1 | SellSetup: Item_CanUse's mode 2 | 1000 |
| P2 | SellSetup: +2 8 | 1000 |
| Q1 | SellList: five tabs | 47 |
| Q2 | SellList: left from tab 0 to 0 | 98 |
| Q3 | SellList: the up scroll at the top row too | 31 |
| Q4 | SellList: the row at most 0x7E | 15 |
| Q5 | SellList: the down scroll eight below | 14 |
| Q6 | SellList: page up of top 9 to 0 | 0 - changes nothing (below) |
| Q7 | SellList: page down to 0x77 above 0x6F | 7 |
| Q8 | SellList: the last page row 0x7E | 6 |
| Q9 | SellList: nothing re-read after the move sound | 5 |
| Q10 | SellList: the scroll word ignored | 377 |
| Q11 | SellList: Item_CanUse's mode from +9 | 175 |
| Q12 | SellList: the most from the id list | 114 |
| Q13 | SellList: the stack's row unsigned | 13 |
| Q14 | SellList: cancel counter 5 | 108 |
| Q15 | SellList: Item_Price with the arguments swapped | 992 |
| Q16 | SellList: the hand rows 12 apart | 595 |
| Q17 | SellList: the choice byte for Shop_SellPrice's flag | 114 |
| K1 | SellCount: help 0x50 | 1000 |
| K2 | SellCount: the window y + 0x13 | 1000 |
| K3 | SellCount: +0xD the count | 994 |
| K4 | SellCount: the row unsigned | 168 |
| K5 | SellCount: sound 0x100 | 824 |
| F1 | SellConfirm: capped at 9,999,998 | 258 |
| F2 | SellConfirm: the cap compared signed | 121 |
| F3 | SellConfirm: the id kept at 0 | 92 |
| F4 | SellConfirm: the stack's row unsigned | 45 |
| F5 | SellConfirm: the count a word | 2 |
| Z1 | SellClose: to state 2 | 339 |
| Z2 | SellClose: the hand kept | 995 |
| X1 | Close: the menu byte 2 | 327 |
| X2 | Close: the menu ended before Window_ResetAll | 326 |
| R1 | TitleTask_Run: Game_Step kept | 1000 |
| R2 | TitleTask_Run: sleeps of 2 | 1000 |
| R3 | TitleTask_Run: the task records before the mode | 764 |
| R4 | TitleTask_Run: the other mode | 764 |
| R5 | TitleTask_Run: Window_ResetAll first | 1000 |
| Y1 | TitleMode_Load: file 0x31C | 1000 |
| Y2 | TitleMode_Load: one sleep, no second test | 726 |
| Y3 | TitleMode_Load: the dirty byte + 2 | 1000 |
| Y4 | TitleMode_Load: the mode on before the restore | 1000 |

Q6 (`top <= 9` for `top < 9` in the page up) changes nothing: at a top of
9 both branches give row - 9 and top 0. D1..D4 make a dispatch read an
index far past its table (the step byte for the state byte, and so on); the
copy and ours then jump into whatever the thirty swapped dwords do not
cover, and the self-test process dies before it can count - a refusal, but
not a counted one.

A first run of the same 121, before the sound recorder moved the state,
step and choice (2026-09-25), let three through that the final fuzz
refuses: C4 (the choice read before the sounds), L7 (99 held counted as
full) and M9 (the step read before the sound).

## 5. What the original does that it should not

Described here, not numbered - the coordinator numbers after the merge.
None shows in play as far as the reads go, and all are kept.

- **A step of one plays its sound twice.** `ShopTrade_BuyCount` and
  `ShopTrade_SellCount` compare the count with the one they started with
  after the one-step clamp *and again* after the ten-step clamp, so a single
  up or down plays the cursor sound (`0x100` / `0x101`) twice in the frame.
  Two identical effects started together are unlikely to be heard as two.
- **Unchecked dispatch indices** (every table of section 2): a state or step
  byte out of range jumps through the next table, and far enough past
  through non-code. Nothing found sets one out of range.
- **`ShopTrade_BuyList` divides the money by the scaled price** with no
  test; `Shop_ScalePrice` answers 1 for 0, so it cannot fault. Ours aborts
  where it would.
- **`ShopTrade_BuyMember` marks the panels with a signed index** against
  `Party_Count`'s unsigned byte; with at most three members it cannot matter.

## 6. What is left, and what the fuzz did not reach

- **`0x5916B0` and `0x5917D0` are left.** They are switch cases, not
  functions: entries 1 of the four-entry jump tables `0x591708` and
  `0x591800` inside `Item_NamePtr` `0x591680` and `Item_EquipMask`
  `0x5917A0` (the armour cases: `lea eax, [eax*2 + 0x657D68]` and `mov al,
  [eax*2 + 0x657D78]`). The first-call trace saw them because it ran with
  every function Capcom's; both hosts are ours since round six
  (`char_stats.cpp`), whose code never jumps there, so they already went
  with their hosts (the round's rule, "What is different" item 2).
- Not reached by the fuzz: the null-entry aborts and `ShopTrade_BuyList`'s
  divide abort (no recorder answers 0, as the real callees cannot);
  `TitleTask_Run`'s real exit (`Task_Restart` from inside
  `TitleFlow_Step`, a recorder here); and the live data - every list here
  is a buffer. The live check is the shop route's A/B after the merge.
- `0x582EB0`'s menu (section 2) runs our sell steps and is on no route.

## 7. Other groups' addresses (said, not acted on)

- **`Item_Price` `0x591C20` (group W's) is not a price.** Its word goes to
  the help line's message word `0x803170` in both list steps, and the price
  the shop charges is `Item_BasePrice` `0x5749F0`'s (group Y's) through
  `Shop_ScalePrice`. It reads weapons `+0x18`, armour `+0x16`, accessories
  `+0x14`, key items `+0x10`, consumables `+0x12`: each record's help
  message. A rename is W's (or the coordinator's) to make.
- **`MoveScript_EffectState` `0x66972C` (24 bytes) is the character to
  record map** - `0x66972C[0x904062[member]]` picks the `0x903A70 + 0xA4 n`
  record in `ShopTrade_BuyMember`, `ShopTrade_BuySlot`, `0x581720`, and in
  `item_use.cpp`, `msgbox.cpp`, `battle_window_draw.cpp`,
  `inventory_ops.cpp` already. The name came from a single movement-script
  reader.
- **`0x584F70` is also entry 4 of `0x664254`**, the state table of the menu
  `0x584180` (entry 7 of `0x663E40`, jumped through at `0x584187`); the
  queue listed its pointer as `0x664264`, which is that entry. Its other
  states `0x584190`, `0x5841B0`, `0x584460`, `0x584BB0` are no group's.
- `0x581720` (no group's, the recorded start that swallowed these) ends at
  `0x5818AE` (0x18F bytes): the shop's frame draw, by its calls
  (`Menu_DrawBackdrop`, `Menu_DrawTitleBox`, `0x573560`, `Crt_sprintf`,
  `Text_DrawFont8`, `0x581300`, `Menu_DrawCursorBox`).

## 8. `entries_logic.txt`

Appended to the main checkout's `analysis/calltrace/entries_logic.txt`
(2026-09-25), under a `# --- ... group DG` header, sizes as read:

    005818B0 E      005818C0 E      005818D0 9D     00581970 3D
    005819B0 E      005819C0 11C    00581AE0 E      00581AF0 E8
    00581BE0 2ED    00581ED0 1B4    00582090 147    005821E0 DF
    005822C0 205    005824D0 1CD    005826A0 CE     00582770 E
    00582780 6B     005827F0 366    00582B60 192    00582D00 177
    00582E80 24     00584F70 1B     00588E70 3A     00588EB0 44

Overlaps for the coordinator: `00581720 18F6` (no group's) overlaps
`005818B0..00582EA3` - its body is `18F`; `00584120 E6B` (no group's)
overlaps `00584F70..00584F8A` - the last `ret` before `0x584F70` is at
`0x584F69`, so it is at most `E4A`. `00588DC0 B0` (`Save_ReadSummaries`)
already ends before `0x588E70`.
