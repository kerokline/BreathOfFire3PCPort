# A command's target choice and the battle's item window

**Status:** IN PROGRESS (2026-09-25). Sixteen functions are ours
(`src/game/battle_actor_copies.cpp`), group CH of the eighth round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Each is fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder:
16,000 rounds, 0 mismatches. 92 negative controls were planted: 90 were
refused, and two are changes that change nothing (section 4). Not yet
live-checked; the combat route reaches all sixteen.

Every function is a *faithful* replacement, so no `DIVERGENCE.md` entry is
owed. No defect of the original was found.

The file name is the round's. None of the sixteen copies an actor: they are
what `pe_funcs.py` folded into `Battle_SpawnActorCopies` `0x446FF0` and
`ItemMenu_CanUseSelected` `0x447840` (both ours since round seven,
[`battle_misc.md`](battle_misc.md)), because nothing calls them - they are
reached through `.data` tables.

## 1. Where they sit

`Battle_PhaseDispatch` (ours, [`battle_flow.md`](battle_flow.md)) calls phase
1, `0x42E990` (group CA's), which tail-jumps through the phase-1 table
`0x64AE28` by the byte `0x904AA1`. Entries 11 and 12 of that table
(`0x64AE54`, `0x64AE58`) are this group's two top stubs. Each dispatches on
`0x904AA3` (the step), and the step handlers below them on `0x904AA4` (the
pick state). Every one is `void (void)`: each is entered by a tail jump, so the
frame and the return are `Battle_PhaseDispatch`'s call's, and it discards eax.

```
0x64AE28[11]  BattleTarget_Dispatch 0x447110      by 0x904AA3 through BattleTarget_Steps 0x64E3EC
                0  BattleTarget_ShowPrompt 0x447120
                1  BattleTarget_PickDispatch 0x447140   by 0x904AA4 through BattleTarget_Picks 0x64E3F4
                     0  BattleTarget_Begin 0x447160
                     1  BattleTarget_PickEnemy 0x447190
                     2  0x447290 (the party's side; not reached, Capcom's)
                     3  BattleTarget_Confirm 0x447390
                     4  0x4473E0 (the cancel; not reached, Capcom's)
0x64AE28[12]  BattleItem_Dispatch 0x447430        by 0x904AA3 through BattleItem_Steps 0x64E408
                0  BattleItem_OpenDispatch 0x447440     by 0x904AA4 through BattleItem_OpenSteps 0x64E420
                     0  0x4481B0 (group CI's)
                     1  BattleItem_OpenWindow 0x447460
                1  BattleItem_Browse 0x4474B0
                2  0x447880 (the window closing; not reached, Capcom's)
                3  BattleItem_Choose 0x4478B0
                4  BattleItem_TargetDispatch 0x447940   by 0x904AA4 through BattleItem_TargetSteps 0x64E428
                     0  BattleItem_TargetBegin 0x447960
                     1  BattleItem_PickEnemy 0x4479E0
                     2  BattleItem_PickParty 0x447B10
                     3  BattleItem_Confirm 0x447CC0
                     4  0x447D30 (the cancel; not reached, Capcom's)
                5  0x447D70 (not reached, Capcom's)     through 0x64E43C: 0x447D90, 0x447DD0, BattleItem_Confirm, 0x447D30
```

## 2. What is ours

Each extent was read to its last instruction with capstone on 2026-09-25
(`pe_disasm.py`; the E8 offsets are listed in the fuzz). The table slot is
where the function's pointer sits. The `symbols.toml` `evidence` field of each
entry holds the full reading.

| Function | Entry | Bytes | Slot | What it does |
|---|---|--:|---|---|
| `BattleTarget_Dispatch` | `0x447110` | 0xE | `0x64AE54` | `jmp [0x64E3EC + 4 * byte 0x904AA3]` |
| `BattleTarget_ShowPrompt` | `0x447120` | 0x19 | `0x64E3EC` | `BattleBanner_SetMessage(0, 0)`, then step + 1 |
| `BattleTarget_PickDispatch` | `0x447140` | 0x11 | `0x64E3F0` | `jmp [0x64E3F4 + 4 * (dword 0x904AA4 & 0xFF)]` |
| `BattleTarget_Begin` | `0x447160` | 0x2F | `0x64E3F4` | the target starts on `Battle_DefaultTarget(3)`; pick state 1 |
| `BattleTarget_PickEnemy` | `0x447190` | 0xF2 | `0x64E3F8` | cancel / confirm, else up/down and left/right among the enemies |
| `BattleTarget_Confirm` | `0x447390` | 0x4D | `0x64E400` | the command committed; back to the command menu |
| `BattleItem_Dispatch` | `0x447430` | 0xE | `0x64AE58` | `jmp [0x64E408 + 4 * byte 0x904AA3]` |
| `BattleItem_OpenDispatch` | `0x447440` | 0x11 | `0x64E408` | `jmp [0x64E420 + 4 * (dword 0x904AA4 & 0xFF)]` |
| `BattleItem_OpenWindow` | `0x447460` | 0x4B | `0x64E424` | the item window set up for the acting actor |
| `BattleItem_Browse` | `0x4474B0` | 0x387 | `0x64E40C` | the item window's frame: pages, cursor, scroll, description, cancel, confirm |
| `BattleItem_Choose` | `0x4478B0` | 0x90 | `0x64E414` | the chosen item's target, by its record's flags |
| `BattleItem_TargetDispatch` | `0x447940` | 0x11 | `0x64E418` | `jmp [0x64E428 + 4 * (dword 0x904AA4 & 0xFF)]` |
| `BattleItem_TargetBegin` | `0x447960` | 0x7F | `0x64E428` | the item's target starts: enemies or party |
| `BattleItem_PickEnemy` | `0x4479E0` | 0x126 | `0x64E42C` | as `BattleTarget_PickEnemy`, the side switch only for items that allow it |
| `BattleItem_PickParty` | `0x447B10` | 0x1AC | `0x64E430` | the pick among the party |
| `BattleItem_Confirm` | `0x447CC0` | 0x69 | `0x64E434`, `0x64E444` | the item command committed; the windows freed |

The names are hypotheses from what the code does. The short version:

- **The five stubs** are `mov al, [step]; jmp [eax*4 + table]`. Ours read the
  table entry afresh from `.data` and call it. As the original has it, the
  index is not checked (known-defects.md D59). The tables sit back to back, so one past the end
  reads the next table's first entry; ours reads exactly what the original's
  `jmp` would.
- **The pick states** (`0x447190`, `0x4479E0`, `0x447B10`) share a head:
  `Input_Pressed` is read once and tested with `Field_CancelButtons` (pick
  state 4, the cancel) and then with `Field_ConfirmButtons` (3, the confirm).
  Otherwise `Input_AutoRepeat(pressed & 0xF000)` gives the d-pad:
  - `0x5000` switches sides (for an item, only when its record has `0x80`);
  - `0x2000` steps up through `Battle_DefaultTarget`, `0x8000` down through
    `0x4457F0`, each on `Battle_WrapIndex` of the target byte (read signed)
    ± 1: among actors 3 .. `0x904AB2` + 2 for the enemies, 0 .. `0x904AB0` -
    1 for the party. Both may run in one frame. Each plays cue `0x101`.
  - On the party's side, while `Battle_ReturnTrue` answers non-zero (it
    always does, [`battle_misc.md`](battle_misc.md)), the wrapped index's low
    byte is the target itself, with no skip of absent members.
- **The command** is the record `0x939FA0` points at: `+0` the target byte
  (an actor 0..10, or `0x40` / `0x80` / `0xC0` for a side or everyone), `+1` a
  kind byte (1 after a target choice, 4 after an item), `+2` the item id (a
  u16), `+0x10` flags. Bit 1 without bit 17 of the flags holds the item
  window on page 2. `0x939EC4` points at the acting actor's context: `+1` is
  set to 2 on a confirm, `+5` is the actor.
- **`BattleItem_Browse`** draws nothing itself. It places the cursor
  (`0x803410` / `0x803412`) from the cursor and scroll as they were before
  this frame's input. It then turns the page, moves the cursor (0..9 by a row,
  7 by a page, the scroll 0..3), and puts the description of the item under
  the cursor (`Msg_SystemPtr` of its record's `+6`) into the message queue
  entry before the write index. On a cancel or a confirm it keeps the page,
  scroll and cursor for the actor `0x8033AA` names (`0x9045FC` + 3n) and
  closes the window (`0x8033A3` = 1). A confirm that
  `ItemMenu_CanUseSelected` answers 1 plays cue `0x107` and stays open.
- **Item `0x97`** is special-cased on the confirm: the acting actor becomes
  its own target, window record 4's byte `+3` (`0x8031F3`) is set to 2, and
  `0x904AA2` = 7 with step 0 - it skips the target choice. Which item `0x97`
  is was not looked up.
- **`BattleItem_Choose`** reads the item record's flags byte (`0x65C4D8` + 24
  n): `0x40` a choice (step 4, or step 5 with `0x10`); `0x80` everyone
  (`0xC0`); `0x10` one side whole (`0x40` with `0x20`, else `0x80`); none the
  user itself. Everything but a choice goes straight to the confirm (step 4,
  pick state 3).

## 3. The tables named

Five `[[data]]` entries in `symbols.toml`, `ctype = "unsigned long"`. The
count of each is where the next table starts, checked against the indices the
handlers store:

| Name | Address | Count | Dispatched by | Index |
|---|---|--:|---|---|
| `BattleTarget_Steps` | `0x64E3EC` | 2 | `0x447110` | `0x904AA3` |
| `BattleTarget_Picks` | `0x64E3F4` | 5 | `0x447140` | `0x904AA4` |
| `BattleItem_Steps` | `0x64E408` | 6 | `0x447430` | `0x904AA3` |
| `BattleItem_OpenSteps` | `0x64E420` | 2 | `0x447440` | `0x904AA4` |
| `BattleItem_TargetSteps` | `0x64E428` | 5 | `0x447940` | `0x904AA4` |

`0x64E43C` (four entries, `0x447D70`'s) is not named: `0x447D70` is in no
group. `0x64E44C` onwards is group CI's.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_actor_copies`
(`src/game/battle_actor_copies_fuzz.cpp`). It makes 16 byte-copies with every
E8 re-aimed at a recorder (`CloneCall` with `expected`). `BattleItem_Browse`'s
two E9 are jumps inside it and stay. The twenty entries of the five tables are
pointed at recorders for the run, so each stub reaches a recorder from its
copy and from ours alike, and put back afterwards.

**Each round** starts from random bytes over these regions (7,285 bytes):

- the phase bytes `0x904AA0..0x904ADF`;
- `Input_Pressed`, the two button masks, the repeat latch;
- the item window record 16 and its cursor fields `0x8033A0..0x80341F`,
  window record 4's first bytes, `0x929F06`;
- the message queue `0x93C2A0..0x93C33F`;
- the kept pages `0x9045FC` (3 bytes for each of 256 owners);
- the 256 item records `0x65C4D8` (constant data, put back after).

The two pointers are aimed into buffers of the fuzz's own: the command at one
of 16 offsets, the acting context at one of 16 others or, one round in four,
at the command itself (so the order of the `+1` stores shows). The list
`Char_AbilityList`'s recorder returns points into a 512-byte buffer, a quarter
of whose bytes are `0x97`. Each branch's boundaries are seeded:

- the buttons (none, the cancel set, the confirm set only, both, anything),
  and for `BattleItem_Browse` half the rounds one cancel bit and one confirm
  bit apart, one of them pressed;
- the d-pad bits `Input_AutoRepeat` returns, alone and together (22 values),
  with any upper half;
- the target byte at 0..4, 10, `0x7F`, `0x80`, `0x81`, `0xFE`, `0xFF`, and
  the counts at 0..5, 8, `0xFF`;
- the command flags (bit 1 alone, with bit 17, neither, all);
- the page at 0..4, `0x7F`, `0x80`, `0xFF`; the scroll at 0..4, 6..9, -1,
  -3..-5, `0x7FFF`, `0x8000`; the cursor at the scroll - 1, + 0, + 1, + 6,
  + 7, or at 0, 1, 8..10, `0x80`, `0xFF`;
- the stubs' indices inside their tables.

**The recorders are not quiet.** Each logs its arguments (the low byte where
the callee reads one: `Char_AbilityList`'s three, whose originals carry stale
upper bits; `Sound_PlayEffect`'s u16) and a hash of every byte the sixteen
store, so a store moved across a call shows. Half the time each moves a byte
its callers read again after it: the step after the prompt, the command
pointer after the target calls and the window set-up, the window's actor,
page or cursor after a cue, the cursor after the list. A general disturber
moves one of 22 watched bytes three calls in four. Callees that answer in al
return garbage in the upper bits.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    battle_actor_copies self-test: 16000 rounds over 16 functions (1000 each), 21769 calls to the stand-ins, 0 MISMATCHES
    coverage: table entries 20 of 20; paths per function 2 1 5 1 6 1 6 2 1 17 1 5 3 6 10 1; browse: cancelled 252,
      used 97, refused 95, item 0x97 27, page wrapped 47; calls: prompt 1000, default target 2371, previous target 191,
      wrap 495, repeat 1982, cues 4250, list 2703, message 887, can use 192, return true 698, free 1000

The same build with `BOF3X_SHADOW='*'` exited 0 with `inject: 1040 ours` and
no mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`ch_agent/controls.py` in the session
scratchpad) planted each one alone: replace, build, self-test, revert. The
table gives the rounds that refused each one, of 1,000 per function.

| | Planted | Refused in |
|---|---|---|
| D1 | TargetDispatch: indexed by `0x904AA4` | 797 |
| D2 | PickDispatch: indexed by `0x904AA3` | 824 |
| D3 | ItemDispatch: step 5 read as 0 | 166 |
| D4 | OpenDispatch: the entries swapped | 1000 |
| D5 | TargetDispatch: through `BattleItem_Steps` | 1000 |
| P1 | ShowPrompt: message 1 | 1000 |
| P2 | ShowPrompt: the step read before the call | 519 |
| B1 | Begin: `Battle_DefaultTarget(0)` | 1000 |
| B2 | Begin: the command pointer read before the call | 487 |
| B3 | Begin: `0x904AAF` = 0 | 1000 |
| B4 | Begin: the repeat latch kept | 1000 |
| H1 | pick head: confirm tested before cancel | 319 / 352 / 366 |
| H2 | pick head: the repeat mask `0xF00C` | 85 / 84 / 89 |
| H3 | pick head: cancel sets 3 | 418 / 475 / 471 |
| E1 | PickEnemy: the side switch on `0x4000` only | 67 |
| E2 | PickEnemy: the pick state counted after the cue | 215 |
| E3 | PickEnemy: no return after the side switch | 125 |
| A1 | enemies: the high bound count + 3 | 48 / 104 |
| A2 | enemies: `0x8000` through `Battle_DefaultTarget` | 52 / 91 |
| A3 | enemies: `0x8000` only when not `0x2000` | 24 / 44 |
| A4 | the target read unsigned | 40 / 77 / 81 |
| A5 | enemies: the command pointer not re-read after the calls | 33 / 69 |
| A6 | enemies: the low bound 2 going down | 52 / 91 |
| C1 | Confirm: the acting `+1` stored before the command `+1` | 212 |
| C2 | Confirm: the command pointer read before the cue | 156 |
| C3 | Confirm: `0x904AC3` not counted | 1000 |
| C4 | Confirm: phase 1 = 2 | 1000 |
| O1 | OpenWindow: bit 17 not tested | 234 |
| O2 | OpenWindow: the flags read before the call | 184 |
| O3 | OpenWindow: pick state 1 | 1000 |
| O4 | OpenWindow: the actor from `+4` | 999 |
| O5 | OpenWindow: `+1` = 4 after the call | 997 |
| W1 | Browse: runs unless `0x8033A3` is 1 | 113 |
| W2 | Browse: the cursor x + 8 | 887 |
| W3 | Browse: rows 12 apart | 882 |
| W4 | Browse: the repeat mask `0xF000` | 248 |
| W5 | Browse: the page held on bit 1 alone | 144 |
| W6 | Browse: a page left from 0 wraps to 0 | 57 |
| W7 | Browse: pages 0..4 | 28 |
| W8 | Browse: the page-left cue byte `0x31` | 213 |
| W9 | Browse: the page read before the cue | 24 |
| W10 | Browse: the scroll up at the scroll row too | 16 |
| W11 | Browse: the cursor down to 10 | 9 |
| W12 | Browse: the scroll down at scroll + 6 | 6 |
| W15 | Browse: the scroll unsigned in the row page up | 67 |
| W16 | Browse: the move cue on no move | 887 |
| W17 | Browse: the queue index read after the list | 25 |
| W18 | Browse: the message id from `+4` | 887 |
| W19 | Browse: the text not stored while scrolling | 310 |
| W20 | Browse: `Input_Pressed` not re-read after the calls | 39 |
| W21 | Browse: cancel sets step 2 | 211 |
| W22 | Browse: cancel leaves the queue byte | 252 |
| W23 | Browse: owners 4 bytes apart | 347 |
| W24 | Browse: can-use inverted | 192 |
| W25 | Browse: can-use tested on the whole eax | 97 |
| W26 | Browse: the actor read before the cue | 14 |
| W27 | Browse: item `0x96` | 27 |
| W28 | Browse: item `0x97`, `0x904AA2` = 6 | 27 |
| W29 | Browse: an item chosen goes to step 2 | 70 |
| W30 | Browse: window record 4 left at 1 | 27 |
| W31 | Browse: the cursor for the second list read before the call | 47 |
| W32 | Browse: the second list of another page | 97 |
| K1 | Choose: step 4 for both choices | 236 |
| K2 | Choose: everyone is `0x80` | 255 |
| K3 | Choose: the side bytes swapped | 140 |
| K4 | Choose: the user from `+4` | 124 |
| K5 | Choose: `0x80` tested before `0x40` | 241 |
| X1 | the item under the cursor: the cursor read before the call | Browse 423, Choose 380, PickEnemy 46, PickParty 54 |
| X2 | the item under the cursor: actor and page swapped | 884 / 996 / 367 / 350 |
| X3 | item records 16 bytes apart | Choose 761, TargetBegin 520, PickEnemy 99, PickParty 93 |
| T1 | TargetBegin: the enemies on `0x40` | 484 |
| T2 | TargetBegin: `Battle_ReturnTrue` inverted | 498 |
| T3 | TargetBegin: the party is pick state 1 | 498 |
| T4 | TargetBegin: the party starts on `Battle_DefaultTarget(3)` | 258 |
| T5 | TargetBegin: the id from the byte `+3` | 505 |
| T6 | TargetBegin: `Battle_ReturnTrue` on the whole eax | 258 |
| I1 | item PickEnemy: the side switch whatever the item | 102 |
| I2 | item PickEnemy: the item read before the input call | 1000 |
| Q1 | PickParty: the side switch counts up | 97 |
| Q2 | PickParty: the high bound the party count | 52 |
| Q3 | PickParty: `0x2000`'s `Battle_ReturnTrue` inverted | 101 |
| Q4 | PickParty: the low bound 1 going down | 51 |
| Q5 | PickParty: `0x8000` through `Battle_DefaultTarget` | 48 |
| Q6 | PickParty: the side switch through `Battle_DefaultTarget(0)` | 97 |
| Q7 | PickParty: no cue after `0x2000` | 101 |
| F1 | item Confirm: the queue entry at the write index | 1000 |
| F2 | item Confirm: the queue byte after `ItemMenu_FreeWindows` | 997 |
| F3 | item Confirm: `+1` = 1 | 814 |
| F4 | item Confirm: the phase bytes before `ItemMenu_FreeWindows` | 1000 |
| F5 | item Confirm: `0x904AAF` kept | 997 |
| W13 | Browse: a row page up below 8, not 7 | **not refused; no input can tell** |
| W14 | Browse: a row page down above -5, not -4 | **not refused; no input can tell** |

A shared helper's control is refused by each function that uses it (three
numbers for the pick head, four for the list read).

**W13 and W14 change nothing.** At scroll 7 both branches of the row page up
leave the cursor 7 lower and the scroll 0; at scroll -4 both branches of the
row page down leave the cursor 7 higher (3 - `0xFC` is 7 in a byte) and the
scroll 3. They are not counted.

**The first run's thin ones:** W26 (1), W31 (1), A5 (7 / 3), W12 (4), W9 (7),
W10 (9), O2 (25), P2 (29) and B2 (32). Each rose once
the recorder it depended on moved the byte its caller reads again, or once
the cursor was seeded relative to the scroll.

**The thinnest now:** W12 (6), W11 (9), W26 (14), W10 (16).

**What the fuzz cannot see:**

- anything the callees really do, and what the seven entries of the tables
  that are not ours do;
- the order of stores with no call between them, except the two `+1` stores
  (the acting context aimed at the command a quarter of the time);
- an index past a table's end (seeded inside the tables);
- the stale upper bits of `Char_AbilityList`'s three arguments (the recorder
  keeps the low bytes, which is what ours reads).

## 5. Found on the way

- **Seven more functions are in these tables** and in no group of the round:
  `0x447290`, `0x4473E0`, `0x447880`, `0x447D30`, `0x447D70`, `0x447D90`,
  `0x447DD0`. The combat route does not reach them: the party's side of a
  plain target choice, the three cancels, the item window's closing step, and
  step 5 (an item whose flags have `0x40` and `0x10`) with its table
  `0x64E43C`. Each is as small as its neighbours here and reads the same
  globals. They are left Capcom's; they are the next route's (a cancel, an
  item on the party).
- **`0x4481B0`, group CI's, is `BattleItem_OpenSteps` entry 0**: it pushes
  system message `0x4000` onto the message queue and counts `0x904AA4` on.
  The round's table lists `.data 0x64E420` among its pointers; the table is
  named here, as its dispatcher `0x447440` is this group's. CI calls through
  it.
- **`0x4457F0`** is unnamed and in no group: `Battle_DefaultTarget`'s
  downward twin, reached from the pick states' `0x8000`. It reads the
  argument's low byte and pushes the whole slot on to `Battle_ActorIsOut`.
- **`ItemMenu_CanUseSelected`'s answer reads inverted** (group BF's name,
  round seven). Its one caller, `BattleItem_Browse` (`0x447776`), plays cue
  `0x107` and keeps the window open when it answers 1, and goes on with the
  item when it answers 0. So "1" means the item cannot be used here, unless
  cue `0x107` is not a refusal. Said here, not acted on.
- **`Battle_ReturnTrue` makes two paths dead.** `BattleItem_TargetBegin`'s
  `Battle_DefaultTarget(0)` and `BattleItem_PickParty`'s
  `Battle_DefaultTarget` / `0x4457F0` branches run only when it answers 0,
  which it never does. The fuzz reaches them anyway.
- **`0x8031F3`** is window record 4's byte `+3` (`0x803160` + 4 × `0x24` + 3),
  the same field as the item window's `0x8033A3`. `battle_damage_callees.h`
  names it `kInstantKill` ("set with the flag-0x100 kill"). Item `0x97` sets
  it to 2 here. One of the two readings is off; not looked into.
- **`BattleItem_Browse` places the cursor before it moves it**, so the
  cursor's screen position lags its index by one frame. That is how the
  original has it, and it is not observable at 60 frames a second.

**For `analysis/calltrace/entries_logic.txt`** (not in the repository): the
sixteen lines were added on 2026-09-25 with the extents above. Two existing
lines still run over them. `00446FF0 847` should be `00446FF0 11D`, and
`00447840 616` should be `00447840 3C` (the sizes `battle_misc.md` measured).
The merger should cut both.

## 6. For the batch check

The combat A/B (`analysis/validate_combat.sh`) reaches all sixteen: the
round's first-call trace of the combat route entered each of them. What it
does not reach stays Capcom's (section 5). `BOF3X_ORIGINAL` with all sixteen
names adds 346 characters to the list.
