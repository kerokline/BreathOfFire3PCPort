# The hard-coded button tests through σ

**Status:** BUILT (2026-10-17, group HT of
[`button-unification-round.md`](button-unification-round.md) 3, branch
`buttons/ht`; group HT-2 added the owner's three of section 3 on
`buttons/ht2`, section 2.4). Headless self-tests pass, and so does the
stand-in check (section 4). Not played. RC's site routes are owed (section
7).
**Numbered at the round's end (2026-10-17):** ledger draft G (section 6) is DIV-0139; the
two defects of section 7 are known-defects D314 (the ram's text) and D315
(the well's tests).

The design is [`button-roles-by-release.md`](button-roles-by-release.md) 7.4:
a hard-coded test of Japan's bit `b` tests `σ(b)`. The API is SG's
([`button-sigma.md`](button-sigma.md) 2): `Sigma(bits)` maps each of the six
face and shoulder bits and passes the rest through. Tests that read a Config
word already follow `σ`, so they are left alone. Only constants change.

## 1. What changed

Every changed line has one of three shapes:

- **A mask:** `Input_Pressed & b` becomes `Input_Pressed & Sigma(b)`. This
  covers a held test, a pressed test, a byte read of the low byte, and the
  item lists' repeat masks `0xF00C` / `0x500C`. `Sigma` maps the shoulder bits
  `0x0C` and passes the d-pad through. The lists' later L1 / R1 tests on the
  repeat's answer, `& 4` and `& 8`, become `& Sigma(4)` and `& Sigma(8)`.
- **An exact compare:** the well and the chop compare the whole word,
  `Input_Pressed == 0x40`. That becomes `== Sigma(0x40)`, and the compare
  stays exact.
- **The fishing's stage 1** (`LeaderPanel_S1Buttons` `0x528A90`) switches on
  `Input_Pressed & 0x74` exactly: L1 opens the look, triangle the menu, circle
  the cast, cross the quit. Ours switches on
  `SigmaInverse(pressed & Sigma(0x74))`, so `0x74`'s four bits are taken one
  by one. The swapped button lands on Japan's case. Two buttons at once still
  land on no case, as in the original.

Each file gets one include, `input/sigma.h`, and a using-declaration after
its includes. Comments that name a bit still name Japan's bit, which is the
bit the original tests.

With the Config at Japan's defaults, `σ` is the identity and every test is
the original's. Under `BOF3X_ORIGINAL=ButtonSigma`, and before `sigma::Arm`
(so in every module's start-up fuzz), `Sigma` is the identity.

## 2. Every test changed

The file:line is on `buttons/ht`. Each address is the original function's
entry. Each bit is Japan's.

### 2.1 The fishing (the field minigames family)

| Original | Function | Bits | File:line |
|---|---|---|---|
| `0x528A90` | `LeaderPanel_S1Buttons` (stage 1: look, menu, cast, quit) | `0x74`: `0x04`, `0x10`, `0x20`, `0x40` | `rest_1g.cpp:233` |
| `0x52AF80` | `LeaderPanel_S10Look` (held) | `0x40` | `rest_1g.cpp:417` |
| `0x52B460` | `LeaderPanel_PressLatch` | `0xE020` (circle, with the d-pad passed through) | `rest_1g.cpp:655` |
| `0x528CD0` | `LeaderPanel_S1Choose` | `0x60`, `0x40` | `effect_1e.cpp:167`, `:168` |
| `0x529070` | `LeaderPanel_S2Steer` | `0x20` | `effect_1e.cpp:318` |
| `0x5291D0` | `LeaderPanel_S3Run` | `0x40` pressed, `0x20` held | `effect_1e.cpp:386`, `:445` |
| `0x5295F0` | `LeaderPanel_S4Run` (the reel) | `0x20` pressed x2, held x2 | `effect_1e.cpp:632`, `:640`, `:659`, `:660` |
| `0x529AE0` | `LeaderPanel_S4Count` | `0x20` | `effect_1e.cpp:758` |
| `0x529C70` | `LeaderPanel_S4Take` (the catch) | `0x60` | `effect_1e.cpp:805` |
| `0x529E70` | `LeaderPanel_S4ResultWait` | `0x60` | `effect_1e.cpp:861` |
| `0x529FA0` | `LeaderPanel_S4Again` | `0x60`, `0x20` | `effect_1e.cpp:926`, `:927` |
| `0x52A280` | `LeaderPanel_Leave` (the argument to `LeaderPanel_LeaveOnPress`) | `0x60` | `effect_1e.cpp:1023` |

The brief's "`0xC0`" is not a face test. The census's `0xC0` is
`test ah, 0xc0` in `LeaderPanel_S4Run` (`0x529671`, `0x529680`), which is
`0xC000`, the d-pad's left and down. The census's `0xA0` and `0x50` are the
same kind of test (`0xA000`, `0x5000`). None of them changes.

### 2.2 The field minigames

| Site | Original | Function | Bits | File:line |
|---|---|---|---|---|
| the rope pull (area 75) | `0x40E520`, `0x40E5A0` | `Area75_PlayerPress`, `Area75_EarlyPress`, through the helper `Pressed20` | `0x20` | `area_w1f.cpp:181` (callers `:1001`, `:1022`) |
| the crane (area 135) | `0x41DEA0`, `0x41DF00` | `Area135_HeldMove1`, `Area135_HeldMove7`, through the helper `HeldMove` (held) | `0x20` | `area_w3d.cpp:105` |
| | `0x41E630` | `Area135_Tail18`: held, pressed, held | `0x20` x3 | `area_w3d.cpp:640`, `:658`, `:661` |
| the well (area 68: effect kind `0x8C`, which `Scena11_Scene5` places in area `0x44`) | `0x48ACA0`, `0x48AD50` | `EffectKind8C_WaitA`, `_WaitB`, through the helper `Kind8CWait` | `== 0x40` | `effect_4b.cpp:1284` |
| | `0x48AD10`, `0x48B100` | `EffectKind8C_PressA` (`0x40`), `_CountPress` (`0x20`), through the helper `Kind8CHold` | `== Sigma(button)` | `effect_4b.cpp:1296` |
| | `0x48B070`, `0x48B090`, `0x48B140` | `EffectKind8C_Ready`, `_CountA`, `_CountB` | `== 0x20` | `effect_4b.cpp:1389`, `:1405`, `:1433` |
| the chop (area 8) | `0x53AC30` | `Scena01_Scene06` step `0x12`, after `Field_ChangeArea(8, ...)` | `!= 0x40` | `scena_sc1.cpp:573` |
| the antenna (the dial) | `0x563090` | `Scena13_Run5` step 2: the dial, turned by the d-pad's left and right, taken on circle | `0x20` | `scena_sc13.cpp:758` |
| the ending's skip | `0x56C9F0`, `0x56CA60` | `Scena17_Outro08`, `Scena17_Outro09` | `0x20` | `scena_sc15.cpp:2208`, `:2222` |

- **The well is kind `0x8C`.** `Scena11_Scene5` step 6 places kind `0x8C`
  and kind `0x8B` after `Field_ChangeArea(0x44, ...)` (`scena_sc11.cpp:666`),
  and `0x44` is area 68. The tests match the PC text's buttons: cross x3 then
  circle x4.
- **The dial is taken to be the antenna**, from its shape: a dial tuned by a
  tone and taken on circle, which matches area 111's one circle mention. The
  US release changed `SCENA13`'s tests by `P`
  ([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 3). Chapter 13's area
  changes do not name `0x6F`, so this is not confirmed by area. RC's
  antenna route settles it.
- **The ending's skip is not in the brief's list.** It is included because
  `SCENA17` is one of the sections whose tests the US changed by `P`
  ([`button-roles-by-release.md`](button-roles-by-release.md) 3). Leaving it
  on Japan's circle would leave a Western disc-only install skipping on a
  different button from the PlayStation US.

### 2.3 Everything else

| Site | Original | Function | Bits | File:line |
|---|---|---|---|---|
| the dock's parts menu (area 128) | `0x47F610` | `EffectKind5E_Pick`: triangle, cross, circle | `0x10`, `0x40`, `0x20` | `rest_3e.cpp:1004`, `:1010`, `:1015` |
| the item lists | `0x42DE50` | `BattleExtra_EquipListInput` | `0xF00C`, `4`, `8` | `battle_e1.cpp:432`, `:444`, `:459` |
| | `0x4474B0` | `BattleItem_Browse` | `0xF00C`, `4`, `8` | `battle_actor_copies.cpp:291`, `:319`, `:334` |
| | `0x448210` | `BattleItemCmd_List` | `0xF00C`, `4`, `8` | `battle_menu_states.cpp:264`, `:297`, `:312` |
| | `0x448FC0` | `BattleItemCmd_EquipItemPick` | `0xF00C`, `0x500C`, `4`, `8` | `battle_e4.cpp:799`, `:800`, `:815`, `:829` |
| | `0x450340` | `DragonCmd_SlotsCursor` | `0x500C`, `0x10` | `battle_e5.cpp:752`, `:768` |
| | `0x450BE0` | `DragonCmd_Slots2Cursor` | `0x500C` | `battle_e5.cpp:1023` |
| | `0x450FC0` | `DragonCmd_StoreCursor` | `0x500C` | `battle_e5.cpp:1119` |
| | `0x451290` | `DragonCmd_StoreSource` | `0x500C`, `0x10` | `battle_e5.cpp:1179`, `:1195` |
| | (the four `DragonCmd_*` lists) | `MoveCursor` | `4`, `8` | `battle_e5.cpp:492`, `:501` |
| | `0x45FE90` | `CommuItem_Choose` | `0xF00C`, `4`, `8` | `rest_4e.cpp:961`, `:983`, `:997` |
| | `0x5827F0` | `ShopTrade_SellList` | `0xF00C`, `4`, `8` | `shop_states2.cpp:837`, `:868`, `:886` |
| | `0x5833E0` | `ShopBrowse_Choose` | `0xF00C`, `4`, `8` | `rest_2c.cpp:571`, `:586`, `:600` |
| | `0x5849B0`, `0x584D30` | `SharedList_PickShared`, `SharedList_Arrange` | `0x500C` | `field_s.cpp:1494`, `:1603` |
| | (both) | `ScrollList` | `4`, `8` | `field_s.cpp:163`, `:178` |
| | `0x58CAE0` | `Equip_ChooseItem` | `0xF00C`, `4`, `8` | `field_s.cpp:1998`, `:2017`, `:2035` |
| | `0x58AD30` | `FieldMenuItems_List` | `0xF00C`, `4`, `8` | `rest_2d.cpp:1062`, `:1114`, `:1132` |
| | `0x58B1D0` | `FieldItems_ArrangeCategory` (the repeat only; it tests left / right) | `0xF00C` | `rest_2e.cpp:335` |
| | `0x58B4B0`, `0x58BA40`, `0x58EB00` | `FieldItems_ArrangeMove`, `FieldItems_ViewList32`, `FieldAbility_ViewBrowse` | `0x500C` | `rest_2e.cpp:449`, `:592`, `:1850` |
| | (those three) | `Scroll` | `4`, `8` | `rest_2e.cpp:197`, `:215` |
| | `0x58E420`, `0x58E7E0` | `FieldAbility_ArrangeMember` (`0xF00C`), `FieldAbility_ArrangeMove` (`0x500C`) | | `rest_2e.cpp:1628`, `:1728` |

- **The repeat masks change too.** A mask that only starts
  `Input_AutoRepeat`'s twelve-frame wait still decides which press starts it,
  so it moves with the shoulder buttons. Otherwise a swapped L1 would page
  without its first-press wait.
- **The dragon lists' triangle `0x10`** (`DragonCmd_SlotsCursor`, `_StoreSource`:
  the store) is a face constant in a list this group already edits, so it is
  included. It is not in the brief's list.
- **The name entry: nothing to change.** The PC cut its input step to
  `BareRetZero` ([`name-entry-restoration.md`](name-entry-restoration.md) 1).
  The renamer's surviving steps (`CommuName_*`, `rest_4d.cpp`) test words 7 / 8
  and the d-pad only. The tests apply when I34 restores the routine.
- **The ram (area 121): nothing to change.** `Area121_PushObject`
  (`area_w3b.cpp:895`) tests `Input_Held` against word 1, the Move line's
  word (`kButtonMap1` `0x903582`). So it already follows the Config. The same
  holds for `Area121_DrawHud`, `_StepOffObject` and `_MenuButton` (words 0, 1
  and 2) and for area 104's twin functions.

**Count:** 83 changed lines (`tools/sites.py` in the harness root, over
`src/game/`). By family:

| Family | Lines | Files |
|---|--:|--:|
| Field minigames and fishing | 32 | 8 |
| Everything else | 51 | 12 |

### 2.4 Group HT-2: the owner's three (2026-10-17)

The owner's decisions on section 3's table. The file:line is on
`buttons/ht2`.

| Original | Function | Bits | File:line |
|---|---|---|---|
| `0x497B30` | `MsgBox_StatePrint`: the held speed-up (`0x497DC6`) | `0x20` | `msgbox.cpp:563` |
| `0x45F830` | `CommuMusic_Browse`: play / stop, close | `0x20`, `0x40` | `rest_4e.cpp:707`, `:718` |
| `0x42EAD0` | `BattleMenu_CommandSelect`: anything held, then each of the six masks | `0xF00C`, `Sigma(mask)` | `battle_phases.cpp:508`, `:513` |

That is 5 changed lines in 3 files, with one include and one
using-declaration each where they were missing (`msgbox.cpp` and
`battle_phases.cpp`; `rest_4e.cpp` had HT's).

**The text's speed-up asks `Sigma(0x20)`, not word 0.** The original reads
`Input_Held`'s low byte into `cl` and does `test cl, 0x20` at `0x497DC6`
(capstone, 2026-10-17). Held, the delay comes from `0x658E94 + row`, and
otherwise from `0x658E95 + row`. Word 0 (`0x903580`) is the Speak line's
button with L2 and R2 ORed in (`0x23` at Japan's defaults,
[`button-sigma.md`](button-sigma.md) 3). A test of word 0 would let L2 or R2
speed the text, which the original never did. `Sigma(0x20)` is word 0's
button with L2 / R2 set aside ([`button-sigma.md`](button-sigma.md) 2), so it
is the Speak line's button and nothing else. The check tests this for every
release's defaults (section 4).

**The music room.** `0x45F8CA` reads `Input_Pressed`'s low byte and tests
`0x20` (play, or stop the track playing). It then tests `0x40` (close). Both
now ask `σ`, in the same order. The d-pad's `0xA000` (the row) is
unchanged.

**The battle command cross.** `Battle_CommandPadMasks` (`0x64AE3C`, six
halfwords, read in place 2026-10-17) holds the d-pad's up `0x1000`, down
`0x4000`, left `0x8000` and right `0x2000`, then L1 `0x04` and R1 `0x08`.
Entry `k` points the cross at command `k + 1`. Pressing it twice within 8
frames, or the confirm button, takes that command into `0x904AA2`.
`BattleMenu_ConfirmDispatch` (`0x42EED0`) then jumps through
`Battle_MenuSteps` (`0x64AE54`) by that byte:

| Entry | Button (Japan's) | Command | `Battle_MenuSteps` entry |
|---|---|---|---|
| 0 | up `0x1000` | 1 | `0x447430` `BattleItem_Dispatch` |
| 1 | down `0x4000` | 2 (greyed by the record's `+0x10` bit 1) | `0x448180` `BattleItemCmd_Dispatch` |
| 2 | left `0x8000` | 3 (greyed the same way) | `0x447FD0` `BattleAttackCmd_Dispatch` |
| 3 | right `0x2000` | 4 | `0x42EEF0` `Cmd_ConfirmDefend` |
| 4 | **L1 `0x04`** | 5 | `0x42EF50` `Cmd_AutoBattle`: round flags `0x904AA8 |= 0x10`, every member to state 2 (`symbols.toml`, round twelve BE1) |
| 5 | **R1 `0x08`** | 6 | `0x44A000`, the escape: a jump through `0x64E4FC` to `Escape_Roll`, `Escape_FailDispatch`, `Escape_WinDispatch` (round twelve BE4, [`battle_e4.md`](battle_e4.md)) |

So **R1 is the escape**, the Run the owner names, and **L1 is the auto
battle**. The two dispatcher names in rows 1 and 2 are the catalog's; their
in-game command names were not checked here.

Ours asks `held & Sigma(0xF00C)` and `Sigma(mask)` for each entry. The table
is read as it is and never written, so its bytes stay Capcom's. That is the
same as mapping the pressed and held words through `SigmaInverse` before the
lookup: `Sigma(mask) & held` is non-zero exactly when
`mask & SigmaInverse(held)` is. The d-pad passes through `σ`, so it stays
physical. Select (`0x100`) is a test of its own before the loop, and it is
unchanged. Cancel and confirm read words 8 and 7, which already follow the
Config.

With View and Change swapped, L1 escapes and R1 auto-battles. The escape
follows the View line's button (Japan's R1), and the auto battle follows the
Change line's (Japan's L1).

**`SuperCombo_ReadButton` is left** (the owner, 2026-10-17: it does its own
thing with every button).

**`Scena19_Run0` (`0x56D580`), for the owner's decision.** From the code
(capstone, 2026-10-17, `0x56D580..0x56D5B0`):

- It is scenario chapter 19's only run. `Scena19_Frame` (`0x56D550`) reaches
  it in state 2 through `Scena19_Run` (`0x56D570`, one entry). It runs every
  frame while chapter 19 is in that state.
- On `test byte [Input_Pressed], 8` (R1 pressed this frame) it takes a free
  `Effect_Objects` record (`Effect_FindFree` `0x589810`; none, nothing) and
  makes it kind `0x2F` (`+0 = 1`, `+5 = 0x2F`).
- Kind `0x2F` ([`effect_2b.md`](effect_2b.md)) plays sound effect `0x206` and
  draws three red, semi-transparent trails. They turn as they fall and
  narrow, for `0x40` frames, and then the record is released. The only other
  spawner is area 27's `Area27_SpawnEffect2F` (`0x4034F0`), one of that
  area's choice handlers (`Area27_Choices[5]`).
- Each R1 press spawns one more effect, while records are free. Nothing else
  in chapter 19's code reads the pad. Chapter 18's run is a bare `ret`.

What chapter 19 is in play, and what reaches it, was not traced. R1 is View
in Japan's terms. The PlayStation West's `P` does not move R1. If the owner
wants it unified, the change is `Input_Pressed & Sigma(8)`, one line in
`scena_sc15.cpp`.

## 3. The census

**`input_census.py`** (key-icons 1; the tool in the main checkout's
`analysis/shots/keyicons-1013/tools/`), run 2026-10-17 on `bof3/BOF3.exe`,
`analysis/pc_funcs.json` and this branch's `symbols.toml`:

- **121 catalog functions** read the pad or the nine words. **77** read the
  words, and **44** read the pad only. The 10-13 run counted 118; the catalog
  has grown since, and the three extra were not traced.
- The first test after each pad read, over all reads:

  | Test | Count | Test | Count |
  |---|--:|---|--:|
  | a register | 146 | `0x0` | 26 |
  | `0x20` | 23 | `0xF000` | 23 |
  | `0x5000` | 22 | `0xA000` | 16 |
  | `0xF00C` | 11 | `0x500C` | 11 |
  | `0x40` | 9 | `0xA0` | 9 |
  | `0x8` | 8 | `0xFFF` | 6 |
  | `0x10` | 6 | `0x80` | 4 |
  | `0x50`, `0x60` | 3 each | `0xC0`, `0x1E`, `0x1`, `0x3000` | 2 each |

  Ten more appear once each.
- `analysis/` is ignored (`.gitignore` line 62), so the TSV is not
  committed. It is at `C:\Users\kerok\b_ht\input_census.tsv`, outside the
  tree.

**The census's limits.** It names a function by its `pc_funcs.json` entry,
which can be wider than `symbols.toml`'s function. For example, its
`Field_ReadCellsRaised` row holds `LeaderPanel_S1Buttons`. It records only the
first test after each read. So two read-only scripts in the harness root
`C:\Users\kerok\b_ht\tools\` went per instruction:

- `input_sites.py` (key-icons 1's) over `0x401000..0x5C0000`: 762 reads of
  the pad or the words, 411 of them of the pad.
- `pad_taint.py` (ours): it follows each pad value through registers, stack
  slots and `Input_AutoRepeat`'s answer to every constant test with a bit in
  `0xFC`. It finds **72 such tests**.
  - 9 of them are not button tests: `0xFFF` / `0xFFFF` / `0x9FF` copy
    masks, and the rebind's own `and 0xFC`.
  - Every remaining one is either in section 2 or in the list below.
  - The script misses tests on a register reloaded after a call, such as the
    dock's second and third tests. The source read in section 2 is the
    authority; the script is its cross-check.

**Face or shoulder constants HT left** (outside its brief's list). The
owner decided each of them on 2026-10-17; group HT-2 (branch `buttons/ht2`)
built the three to change (section 2.4):

| Original | Function | Test | The owner's decision (2026-10-17) |
|---|---|---|---|
| `0x4613B0` | `ConfigScreen_Controller` (the rebind, `0x461459`) | `and 0xFC` | Left. It reads the physical button being bound, so it must stay physical. |
| `0x497B30` | `MsgBox_StatePrint` (`0x497DC6`) | `Input_Held & 0x20` | **Changed.** Holding the text's speed-up button tracks the Speak line: `Sigma(0x20)`. |
| `0x45F830` | `CommuMusic_Browse` | `0x20` play, `0x40` close | **Changed.** The faerie village's music room goes through `σ`, like the other tests. |
| `0x42EAD0` | `BattleMenu_CommandSelect`, the battle command cross | `held & 0xF00C`, then `Battle_CommandPadMasks` `0x64AE3C` | **Changed.** L1 / R1 track the Config through `σ`. The d-pad stays physical, and the table stays Capcom's bytes. |
| `0x49A960` | `SuperCombo_ReadButton` (`magic_s02.cpp`) | any of `0xF0`, then the whole word against the table byte `0x65A518` | **Left.** The owner: it does its own thing with every button. |
| `0x56D580` | `Scena19_Run0` | R1 `0x08` | **Left; no decision yet.** What it is, from the code, is in section 2.4. |

## 4. The stand-in check (`BOF3X_SHADOW=button_tests`)

The check is in `src/input/button_tests.{h,cpp}`. It runs once, after
`sigma::Arm` (`inject_all.cpp`), because it needs the live table. It has two
halves.

**1. The masks** (pure). The check holds a table of the 64 (function, mask)
pairs of section 2. Each pair is tested three ways:

- **Under each release's defaults** as stand-in words (`sigma::DefaultWords`),
  `Apply(table, mask)` must equal the release's own map. For Japan that is the
  original's mask. For the West it is `P`, which is the PlayStation US's own
  test. For PSP-EU it is `S`.
- **Under Japan's words with Speak <-> Action and View <-> Change**, the
  mask must equal the expected swap. The swap is written out by hand
  (circle <-> cross, L1 <-> R1), not computed through `σ`.
- **`LeaderPanel_S1Buttons`' four cases:** for every one of the 256 low bytes,
  the case taken equals the swapped press's case under Japan's buttons.

**2. Four of ours, run for real.** The four are `LeaderPanel_S1Buttons`,
`LeaderPanel_PressLatch`, `EffectKind8C_Ready` and `EffectKind8C_CountPress`:
the four with a path that calls nothing. For each of them:

1. The check writes stand-in words into `0x903580`, and `sigma::Rebound`
   builds the live table from them.
2. It runs the function with 26 presses (none, each of the 16 bits, and
   pairs) under Japan's words, then under the swap.
3. Each press under the swap must do exactly what the swapped press does
   under Japan's words. The test has teeth: under Japan's words circle and
   cross must differ, at `S1Buttons` and at kind `0x8C`.
4. The nine words and every cell touched are put back afterwards.

With `BOF3X_ORIGINAL=ButtonSigma` there is no live table, so the second half
is skipped and a line says so.

**Group HT-2's additions** (section 2.4):

- **The masks:** four more rows: the speed-up `0x20`, the music room's `0x20`
  and `0x40`, and the cross's `0xF00C`. Also `Battle_CommandPadMasks`' six
  masks, read in place from `0x64AE3C` (not copied into the check). Each is
  tested under the three releases' defaults and under the swap, as above.
- **Word 0:** under each release's defaults, `Sigma(0x20)` must equal word
  0's button with L2 / R2 set aside, and must hold neither L2 nor R2.
- **The cross, run for real:** `BattleMenu_CommandSelect` is the fifth live
  function. It is run with the press held and nothing pressed this frame,
  which is its path that calls nothing; select is taken off the held word.
  The result is the command the cross points at (`0x904AB4`), the tap bytes
  and the step bytes. Under Japan's words:
  - for each of the 26 presses, the command must be the original loop's
    answer over the table as it is (the lowest entry held, plus one; `0`
    with nothing of `0xF00C` held);
  - L1 and R1 must differ, and L1, R1 and up must each point at a command.

  Under the swap, the swapped press must do what Japan's press did, as for the
  other four. The cross's bytes (`0x904AA1..A2`, `0x904AA5..A6`, `0x904AB4`,
  `0x904AC3`) and `Input_Held` are put back with the rest.

The speed-up and the music room are checked by their masks only. Each calls
the text or draw code on every path, so neither runs headless here.

**Result** (the clean launcher copy `C:\Users\kerok\b_ht\L`, the PC install,
`--no-config`, `BOF3X_SELFTEST_ONLY=1`). The run's `BOF3X_SHADOW` was
`sigma`, `button_tests` and the 20 modules this branch edits:

> `button_tests: 512 masks under stand-in words (3 releases' defaults and the swap), 188 runs of 4 of ours under the live table (Japan's words and the swap); 0 MISMATCHES`

Every one of the 20 modules' fuzzes reports 0 mismatches. `sigma`'s own
self-test reports 2,211 tables and 0 mismatches. The log ends
`inject: 10087 ours, 0 left original` and `self-test only: done`. The full
`BOF3X_SHADOW='*'` run is in section 8.

## 5. Open question 4: PSP-EU's code at the well and the ram

The question: does PSP-EU's code at the well (area 68) and at the ram (area
121) follow `S` or `P`? DT-1 translated those two areas' PSP-EU text by `P`,
which is answer 3.

**Method.** The two `BOOT.BIN`s were read in place from an earlier session's
scratch extract, copied to `C:\Users\kerok\b_ht\psp\`. Those extracts came
from the existing disc images in `C:\Users\kerok\d303discs\`. Nothing was
written to an install. `tools/psp_pad.py` and `tools/psp_ram.py` in the
harness root are capstone scans that print addresses and immediates only.

- **The pad block** was found from the fishing's stage-1 mask, the one test
  whose JP and EU values `button-roles-by-release.md` 3 already pinned: JP
  `andi 0x74` at `0x0DF0D0`, EU `andi 0xE4` at `0x0DEDE8`. In both it is
  `lhu 4(base)`. The base is `0x0D0710` on JP and `0x0D1220` on EU (link-time
  offsets): Held at `+0`, Pressed at `+4`, as on the PC.
- **Every read of Pressed (230) and of Held (53)** was taken with the tests
  that follow it. The two builds have the same counts, so the reads were
  aligned by index and their tests compared.
- **The Config words** were found from the Held reads that are ANDed with a
  halfword global. They are at JP `0x0FF9D0..` and EU `0x1004E0..`: word 1 at
  `+2`, word 3 at `+6`, word 8 at `+0x10`.

**The well follows `P`.** The well's seven exact compares (JP `0x052788..`,
EU `0x05257C..`) are the only run of three and then four in either build:

| Build | First three compares | Last four compares |
|---|---|---|
| PSP-JP | `== 0x40` (cross) | `== 0x20` (circle) |
| PSP-EU | `== 0x10` (triangle) | `== 0x40` (cross) |

`P` takes cross to triangle and circle to cross. `S` would give circle and
cross. So the code is `P`'s, and PSP-EU's text there (triangle x2, cross x2)
is honest about its own code.

**The ram follows the Config.** The ram's push reads Held against word 1,
the Move line's word, on both builds. JP `0x24ECAC` reads `0x0FF9D2`, and EU
`0x24EAE8` reads `0x1004E2`, with a compare of `0x79` (area 121) nearby. Area
104's twin is at JP `0x243818` / EU `0x24364C`. Both read the word, as the PC
does (`Area121_PushObject`, section 2.3). So the ram takes whatever button
Move is on. At PSP-EU's defaults that is triangle (`S` of square). PSP-EU's
text names circle (`P` of square), so **the ram's text is wrong on the
PSP-EU disc itself**, against its own code at its own defaults.

**Answer 3 should stand for both areas.** In both areas the text is in the
West's terms:

- **The ram:** `P^-1` of PSP-EU's circle is Japan's square, the Move line's
  default. DT-2 draws it as `σ(square)`, the Move line's button now, which is
  the word the code reads. Translating by `S` would give `σ(cross)`, the
  Action line's button, which is wrong.
- **The well:** `P^-1` gives Japan's cross and circle. Those are exactly the
  bits our well now tests through `σ`.

So DT keeps answer 3. The owner's call is only whether to record the PSP-EU
disc's own ram text as a defect of that release. It is wrong there, and our
build draws it correctly.

**Two more readings from the same alignment:**

- PSP-EU's chop follows `S`: JP `!= 0x40` at `0x273D68`, EU `!= 0x20` at
  `0x273BA4`. That agrees with its text (area 8, circle x2).
- PSP-EU's dock follows `S`: JP `andi 0x10` at `0x041490`, EU `andi 0x80` at
  `0x041260`, beside the d-pad's `0x5000` / `0x1000`.

So PSP-EU's own code is `S` except at the well. On a PSP-EU-only install, our
well now tests `σ(cross)` / `σ(circle)`: circle and cross at PSP-EU's
defaults. The disc's own code tests triangle and cross. That is a deliberate
change (DIV-0139): the unified flow, with the icons the text draws, in place of
the one PSP-EU test that broke its release's map.

## 6. The ledger entry, DIV-0139: the hard-coded button tests through σ

- **Date:** 2026-10-17
- **Subsystem:** input: the fishing, the rope pull, the crane, the well, the
  chop, the antenna's dial, the ending's skip, the dock's parts menu, the
  item lists' L1 / R1, and (group HT-2) the text's held speed-up, the faerie
  village's music room and the battle command cross's L1 / R1. Every
  function and line is in section 2 (83 lines in 20 files of `src/game/`,
  and HT-2's 5 lines in 3 files, section 2.4).
- **Tier:** Intent. The owner, 2026-10-17: unify every button flow (answer 1,
  `button-roles-by-release.md` 8). Builds on DIV-0133's `σ`. The same day the
  owner decided section 3's remainder: the speed-up, the music room and the
  cross through `σ`; `SuperCombo_ReadButton` and the rebind left;
  `Scena19_Run0` open.
- **Original behaviour:** these tests compare the pad with Japan's buttons
  as constants: circle to cast, cross to quit, L1 / R1 to page, and so on.
  They ignore the Config. A player who swaps Speak with Action in the Config
  still casts the rod on circle, still pulls the rope and works the crane on
  circle, and still pages the item lists on L1 / R1 after moving View and
  Change. Holding circle speeds the text, circle plays a track in the music
  room and cross closes it, and in battle L1 points the cross at the auto
  battle and R1 at the escape (`Battle_CommandPadMasks` `0x64AE3C`). The
  same holds on the PlayStation, where the Western releases instead
  hard-code `P` of each face constant.
- **New behaviour:** each test asks `Sigma(b)` for Japan's bit `b`. That is the
  button the line whose Japan default is `b` is on now.
  - Exact compares stay exact.
  - The fishing's stage 1 takes `0x74`'s four bits one by one.
  - The speed-up is `Sigma(0x20)`: the Speak line's button, never L2 / R2,
    which word 0 also holds.
  - The battle command cross asks `Sigma` of each of its table's masks.
    The table stays Capcom's bytes, and the d-pad stays physical. The escape
    follows the View line's button and the auto battle the Change line's.
  - At Japan's defaults (every PC install) nothing changes.
  - A Western disc-only install starts at the West's Config (DIV-0134), so
    `σ` is `P` there and the tests are the PlayStation US's own.
  - On a PSP-EU-only install `σ` is `S`. That is PSP-EU's own except at the
    well, whose disc tests `P` (section 5); ours follows the Config there too.
- **Rationale:** with the icons (DIV-0135..0140) and the menus (DIV-0133), one
  table decides every button. The text names the button the code tests, on
  every install and after any rebind.
- **Also in the PSX version?** No. Every PlayStation release hard-codes these
  tests in its own terms: Japan's, `P` for the West, `S` for PSP-EU with the
  well left in `P`. This changes Capcom's design, not a port defect. HT-2's
  three were not read on the PlayStation discs. `P` and `S` do not move L1
  or R1, so the cross's shoulders are the same on every release.
- **Verification:**
  - `BOF3X_SHADOW=button_tests` (section 4): 512 masks and 188 live runs,
    0 mismatches; with HT-2's sites, 555 masks and 261 live runs of five
    functions, 0 mismatches.
  - The 20 edited modules' fuzzes, which run before `Arm` with `σ` the
    identity, report 0 mismatches.
  - RC's swapped site routes are owed (section 7).
- **Reversible?** `BOF3X_ORIGINAL=ButtonSigma`, DIV-0133's switch: with it,
  `Sigma` is the identity and every test is the original's. There is no
  separate switch. A swapped Config with DIV-0133 off but DIV-0139 on would split the
  flow, so the two are kept together.

## 7. What is not reached

- **No game was played or captured.** RC's site routes are the evidence still
  owed, identity and swapped:
  - the fishing (`caughFish`, area 30: stage 1's cast, the reel, the catch);
  - the rope pull (75);
  - the crane (135);
  - the well (68);
  - the chop (8);
  - the antenna (111), which also settles whether the dial is the antenna;
  - the dock (128);
  - the item lists (`fairyVillage`, `shop`, `menu_screens`: an L1 / R1 page
    turn);
  - the ram (121), which needs no change and proves the Config-word path.

  The ending's skip has no route in RC's list.
- **HT-2's routes are owed too:** a battle with View and Change swapped
  (L1 must point the cross at the escape); any dialogue held on the Speak
  line's button, and on L2 (which must not speed it); and the faerie
  village's music room (`fairyVillage`), with play on the Speak line's
  button.
- **`Scena19_Run0`** waits for the owner (section 2.4).
- **An open question on the cross's boxes.** The cross draws its command
  boxes by position, with no button glyph
  ([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.7). So with View
  and Change swapped, the box drawn for command 5 (L1's arm) is still the
  auto battle, but L1 now picks command 6, the escape. Whether
  the boxes should swap sides with the Config is the owner's call. Nothing
  here changes the drawing.
- **The PlayStation US's chop and well were not read.** They are exact
  compares (`lhu`, `li`, `bne`), which `relscan.py`'s pattern (an `andi`
  near a `lhu`) does not see. So they are not among its 44. Their text moved
  by `P` (`button-roles-by-release.md` 5). On PSP-EU the chop's code moved
  by `S` and the well's by `P` (section 5).
- **Section 3's six constants** were decided by the owner on 2026-10-17.
  Three are built (section 2.4), and the rest are left as the owner said.
- **Defects found** (numbered at the round's end: D314 and D315):
  - **D314, PSP-EU's ram text names the wrong button on its own disc.** It says
    circle; the code reads the Move word, which is triangle at PSP-EU's
    defaults (section 5). Our build draws it correctly through answer 3.
  - **D315, PSP-EU's well keeps the West's tests.** It tests triangle and cross
    where its own map `S` gives circle and cross. It is consistent with its
    own text (section 5), so it is a release quirk, not a text defect.
    DIV-0139 changes it on a PSP-EU install.

## 8. Verification

- **Build:** `cmake --preset i686 && cmake --build build` in the worktree. No
  errors, and no warning in any file this branch touches. The warnings
  present (`effect_4b.cpp`'s unused `Shards`, the fuzz files', SDL's) were
  there before.
- **Headless, narrow:** section 4.
- **Headless, everything:** `BOF3X_SHADOW='*'`, the same launcher copy. Exit through `self-test only: done`;
  `inject: 10087 ours, 0 left original`. All 1,081 self-test lines that report a count report
  `0 MISMATCHES`, `button_tests`' line among them as in section 4. No Defender event (1116 / 1117) names
  this group's files.
- **HT-2** (`buttons/ht2`, the clean copy `C:\Users\kerok\b_ht2\L`, the PC
  install, `--no-config`, `BOF3X_SELFTEST_ONLY=1`):
  - The build: no errors, and no new warning.
  - Narrow, with `BOF3X_SHADOW=sigma,button_tests,msgbox,rest_4e,battle_phases`:
    `button_tests: 555 masks under stand-in words (3 releases' defaults and the swap), 261 runs of 5 of ours under the live table (Japan's words and the swap); 0 MISMATCHES`.
    `msgbox` reported 44,000 rounds, `rest_4e` 288,000 rounds and
    `battle_phases` 18,000 rounds, each with 0 mismatches.
  - With `BOF3X_SHADOW='*'`: exit 0 through `self-test only: done`, and
    `inject: 10087 ours, 0 left original`. All 1,084 count lines report
    `0 MISMATCHES`, and none reports another count. No Defender event
    (1116 / 1117) names `b_ht2` or this worktree.
