# Group BE1: BATE's screens, the command menu's tail, the result and the loss screen

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3), wave one, stage B, group BE1. All 41 functions of the group are ours
(`src/game/battle_e1.cpp`, shadow name `battle_e1`): the 39 rows
`analysis/round12_cut.tsv` lists for BE1 and two starts the cut lacks
(`0x432430`, `0x432440`, section 2). Each was read to its last instruction
with capstone and fuzzed through the boss harness's engine frame
([`boss_harness.md`](boss_harness.md) section 10) without edits to it: one
`Run`, 246,000 rounds, 0 mismatches (this worktree). 62 negative controls planted, all refused by a count (section 6).
`BOF3X_SHADOW='*'` headless at this branch's tip: exit 0, 656 self-test lines, every harness's runs 0 mismatches (`boss_harness_eh` and `battle_e1` among them). Three of the functions are entered by the
owner's `dragonTransform` route (section 11); the rest are fuzz only.

Game facts below are what the code does; where a name says more (the
"loss screen", "equipment"), the evidence is the existing docs it cites,
not memory of the game.

| Part | What | Extent | Fns |
|---|---|---|--:|
| BATE state 3 | the tally: six counters counted up, character record 7's stat words raised | `0x42D7A0..0x42DBF9` | 6 |
| BATE state 2 | the equipment screen over record 7's two equipment bytes, and its dispatchers | `0x42DC00..0x42E0D2` | 10 |
| The command menu | `Battle_InputSteps[3]` and its three held steps; `Battle_MenuSteps[5]` (auto battle) | `0x42ED90..0x42F068` | 5 |
| Actions | kind 3's dispatcher and two steps; the random ability pick; the ability notice (`AfterSteps[1]`) | `0x42F5E0..0x42FF69` | 5 |
| The end and the result | the third way out's restore step; the EXP-share count; the level-up notice; the zenny bonus test; a character's level gain | `0x4315C0..0x43242F` | 5 |
| The loss screen | `0x904AE8` bit 0's way out: dispatcher, five steps, four draws | `0x432430..0x432B6A` | 10 |

## 1. What each function does

Addresses are Capcom's; each row's `[[func]]` in `symbols.toml` has the
extent and the full evidence.

### 1.1 BATE, state 3: the tally

BATE is game mode 9's overlay ([`area_w4f.md`](area_w4f.md): mode 9's frame
step `0x517330` calls `0x42D710`, `jmp [0x64ADAC + 4 * byte 0x929F00]`,
nobody's this round). `0x42D730` (state 0, Capcom's) sets the state to
`0x904C9F + 2` - 2 or 3 as far as the writers read
([`scena_sc6.md`](scena_sc6.md) sets `0x904C9F` with the request 7). State 3
is `0x42D760` by `0x929F01` (`0x64ADBC`: `0x42D770`, then two of ours) and
`0x42D770` by `0x929F02` (`0x64ADC8`: `0x42D780` Transition_Start(3), then
ours).

| Address | Name | What |
|---|---|---|
| `0x42D7A0` | `BattleExtra_TallyOpen` | the backdrop and the tally; once no transition runs (`MoveScript_WaitWordDA`), `BattleExtra_ApplyTally`, `0x929F08` = 0, next step |
| `0x42D7F0` | `BattleExtra_TallyCount` | counts `0x675E88[n]` up by `0x64ADD0[n]` to its target `0x675EA0[n]` (a press skips to it), then the next of six; after the sixth, a press gives `Transition_Start(2)` and the next step |
| `0x42D880` | `BattleExtra_TallyClose` | once the transition is done, state 1 (`0x42D750`: `Window_ResetAll`, `Game_Step + 1`); else draws |
| `0x42D8C0` | `BattleExtra_DrawTally(x, y)` | the window: box and border, title 0xE7, three rows of two labels (0xE8 + i, 0xEB + i) and two counters each (`Crt_sprintf` into `0x904BA0`, `Text_DrawFont12`) with a plus sign between, then 0xF0 and two 0xEF lines |
| `0x42DA60` | `BattleExtra_ApplyTally` | the counters' targets from `0x939A04` / `0x939A08` / `0x939A0C` (what fight 26's event hook sums, [`boss_se.md`](boss_se.md)): [0] and [3] the count, [1] and [2] the two sums, [4] `0x939A04 / 20`, [5] 2 / 4 / 6 by `0x939A08` against 0x32 / 0x50; record 7's words `+0x46` / `+0x26` raised by the count, `+0x40` / `+0x20` by [4] (`Stat_AddCap999`), `+0x44` / `+0x24` by [5] |
| `0x42DB40` | `BattleExtra_DrawPlus(x, y)` | two grey semi-transparent `LINE_F2`: a plus 10 wide at the s16 (x, y) |

`BattleExtra_DrawTally`'s row y is the original's `y + 32 * esi`, where
`esi`'s low half is the row (`movzx si, bl`) and its upper half is left
over from the row before (`y + 6` before the first): the same thing for the
callers' `(0x37, 0x37)`, kept exact for any y (control C6).

### 1.2 BATE, state 2: the equipment screen

`BattleExtra_EquipDispatch` is `0x64ADAC[2]`. The tables are nested in one
run of `.data` (`0x64ADAC..0x64AE07`); their counts come from the steps'
writes of the step bytes (section 5), not from the tool.

| Address | Name | What |
|---|---|---|
| `0x42DC00` | `BattleExtra_EquipDispatch` | `jmp [0x64ADE0 + 4 * 0x929F01]`, 3 |
| `0x42DC10` | `BattleExtra_EquipOpenDispatch` | `jmp [0x64ADEC + 4 * 0x929F02]`, 3 (`0x42D780`, then the two below) |
| `0x42DC20` | `BattleExtra_EquipOpen` | once no transition runs: sound 0x102, `0x42E0E0()`, the timer `0x929F04` = 4, next, and five bytes of `0x669CE8` over record 7's first five |
| `0x42DC80` | `BattleExtra_EquipFadeIn` | the timer down; at 0 `0x675EBE..C0` = 0, 0, 1, the next step |
| `0x42DCD0` | `BattleExtra_EquipRun` | `call [0x64ADF8 + 4 * 0x929F02]` (2), then the backdrop |
| `0x42DCF0` | `BattleExtra_EquipSlotInput` | two slots (window 1's cursor flipped by xor 3), their help line `Item_HelpMessage(1, record 7 +0x12)` / `(2, +0x15)` - the two equipment bytes ([`char-stats.md`](char-stats.md)); confirm opens the list, cancel closes the windows and leaves |
| `0x42DE50` | `BattleExtra_EquipListInput` | window 0's list: the hand, the help line, a cursor of 0..0x7F in pages of 9 with a scroll animation word; confirm with a count calls `0x42E250()` and goes back to the slots, without one sound 0x107; cancel goes back |
| `0x42E040` | `BattleExtra_EquipLeaveDispatch` | `jmp [0x64AE00 + 4 * 0x929F02]`, 2 |
| `0x42E050` | `BattleExtra_EquipFadeOut` | the timer down; at 0 `Transition_Start(2)`, next |
| `0x42E090` | `BattleExtra_EquipLeave` | once the transition is done, state 1 and five bytes of `0x669CE0` over record 7's first five |

`0x669CE0` is DIV-0020's whelp slot: that entry's check reads this
function's `push 0x669CE0` at `0x42E09D`, which the inject's five-byte `jmp`
at `0x42E090` leaves in place, and ours copies from the same slot - the
language overlay's name still flows. Not measured live.

### 1.3 The command menu's held steps and auto battle

| Address | Name | What |
|---|---|---|
| `0x42ED90` | `BattleHold_Dispatch` | `Battle_InputSteps[3]` (entered when `BattleMenu_CommandSelect` sees `Input_Held` bit 8): `jmp [0x64AE48 + 4 * 0x904AA2]`, 3 |
| `0x42EDA0` | `BattleHold_Shrink` | the menu member's cross size `0x904ABC[0x904AB4]` down by 2 to 0, then next; the label, the party status and the cross drawn |
| `0x42EE00` | `BattleHold_ShowAll` | while `Input_Held` keeps bit 8: the party status, BE4's `0x444660()`, all seven command labels with their icons; released, windows 1..3's `+3` = 1 and next |
| `0x42EEA0` | `BattleHold_Regrow` | the cross size up by 4 to 8, then `0x904AA1` = 2 (back to `BattleMenu_CommandSelect`) |
| `0x42EF50` | `Cmd_AutoBattle` | `Battle_MenuSteps[5]`: window 4's `+3` = 2; each entry of the order `0x904AB6[i]` (the s8 count `0x904AC3`) holding an item command (5) gives the item back (BE4's `0x446D90`); the members not out to state 2 (with `0x904B8E`, only those with `+0x134` bit 4); round flag 0x10, DAT 0xD1, steps 0 |

`Cmd_AutoBattle` is the PSX `Cmd_AutoBattle_Begin` `0x801D2598` by the
sibling's `names/functions.toml` (its wtrace and decompilation: the auto
flag, the pending item commands consumed, the living members to state 2,
the phase bytes 0) - read here and agreeing step for step.

The held steps push window 1's and 2's words in whole registers whose upper
halves are left over (for `BattleWin_DrawCommandCross`'s y, `0x0090` from
the label argument's address). The callees keep the low halves in effect
(`Menu_DrawIcon` masks x and y to 16 bits); ours passes the words, as
`battle_phases.cpp` does, and the fuzz compares the low halves (section 5).

### 1.4 Action kind 3, the random pick, the ability notice

| Address | Name | What |
|---|---|---|
| `0x42F5E0` | `BattleAction_Kind3Dispatch` | `BattleAction_KindSteps[3]`: `jmp [0x64AEB4 + 4 * 0x904AA3]`, 5 (the two below, then `BattleAction_AbilitySteps`' three: the tables are one run) |
| `0x42F5F0` | `BattleAction_Kind3Banner` | `Battle_ClearActingFlags`, the banner of the string `[0x669E08]`, window 4's `+3` = 1, `Battle_SetActorBit`, the acting sprite's state 9, next |
| `0x42F640` | `BattleAction_Kind3Wait` | once `0x904B82` (the effects still running) is 0: round flag 4, `0x904AA1` = 3, both sub-steps 0 |
| `0x42F9D0` | `BattleAction_PickRandomAbility` | `BattleAction_AbilityCommit`'s pick for the actions 0x24 / 0x25 / 0x8C: an id from `0x64AEC8[Rand() & 0x1F]` (0x24) or `0x64AEE8[...]` (0x25, 0x8C), any other action gives 0; stored to `0x904B80` and the action record's `+2`; al the target by the id's flags (a side, the actor itself, or `0x452EB0()` / `0x452F10()`) |
| `0x42FE20` | `BattleAction_AbilityNotice` | `BattleAction_AfterSteps[1]`: from the party slot after `0x904AA5`, the first member with `+0x130` bit 3 (cleared): with the action ability's flag 0x200 its name (BE4's `0x44A910`) and the acting enemy's ability record's first 16 bytes into `Text_Records + 0x20`, message 0x24 queued; else message 0x23; none left, `0x904AA2 + 1` |

### 1.5 The end and the result

| Address | Name | What |
|---|---|---|
| `0x4315C0` | `BattleEnd_AwaitRestore` | `0x64AF7C[2]` (the third way out's steps under `0x4315B0`, nobody's): `BattleEnd_AwaitMemberTasks`' head (waits on `+0x130` bit 0x2000, a step back while `0x904AA5` is short of the count), then `Port_DroppedCall(0)`, each member's status cleared (`& 0xBF5F`) and tint released, each present member re-posed (`Sprite_AnimFromSet(+0x4B, +0x58 - 2, 0x8C5D80, 0x1800)`), `PartySet_Select(0x90412C & 0x7F, 0)` |
| `0x4319B0` | `BattleResult_CountExpShares` | al: the slots not out whose `+0x134` lacks 0x400 - `BattleResult_SplitExp`'s divisor |
| `0x431C10` | `BattleResult_LevelUpNotice` | `BattleResult_LevelUpSteps[1]`: on a press, the name, the old and new levels (`0x904B72` / `0x904B74`, the new one through `Crt_sprintf` into `Text_Records + 0x20`), message 7 to `0x93B8E4`, a window of 0x10 rows per stat that grows (and per non-zero byte `+6` / `+7` of the old level's `Char_ExpTable` row), `Window_Alloc(1, 4)`, back to the search |
| `0x431FE0` | `BattleResult_ZennyBonus` | al 1 when a slot not out holds 7 in `+0x96` or `+0x97` |
| `0x432170` | `Char_LevelUpGain(roster, what)` | ax: the level the record's EXP reaches (the roster's 99 `Char_ExpTable` rows summed, the first to exceed it, else 99); `what` 0 the levels over the record's `+0xA`, 1..6 one stat's growth over those levels (bytes `+2`, `+3`, the nibbles of `+4` and `+5`, each plus the record's s8 `+0x89..+0x8E`; 0 if negative); else 0. A 7-entry jump table inside the extent |

### 1.6 The loss screen

`BattleEnd_Steps[2]` is the way out `0x904AE8` bit 0 picks
([`battle_turn_steps.md`](battle_turn_steps.md) section 1; the harness's
reading: bit 0 the loss, bit 1 the win); its table `0x64AF70` by
`0x904AA2` ends in `BattleLoss_Dispatch`. The last step restarts the task
at `Boot_Task`.

| Address | Name | What |
|---|---|---|
| `0x432430` | `BattleLoss_Dispatch` | `jmp [0x64AFD8 + 4 * 0x904AA3]`, 5 |
| `0x432440` | `BattleLoss_FadeOut` | `Transition_Start(0x13)`, `Gfx_ClutStripCopyRow(0x1A)`, next |
| `0x432460` | `BattleLoss_ResetParty` | once no transition runs: window 0 off; each present member (by the count, re-read) re-posed (`Sprite_EnsureAnimation(+8 + 0x1C)`, `+0x29` = 1, `+0x24 |= 0x80`, `+0x2E` / `+0x30` from words in the rows past `BattleLoss_Steps`' pointers) and its tint released; the enemies' state bytes (`0x494E70`), `BattleTask_ClearAll`, `Transition_Start(0xE)` |
| `0x4325F0` | `BattleLoss_Show` | black, the panels; once the transition is done the bar at 0x2C, next |
| `0x432630` | `BattleLoss_BarGrow` | black, panels, caption; the bar grown by 4 to 0xF4 and drawn; on any held button both CLUT runs (`0x812980`, `0x811380`, 0x100 words) greyed, `Gfx_ClutStripDirty` = 1, `Transition_Start(0xD)` |
| `0x432750` | `BattleLoss_Restart` | black, panels, caption; once the transition is done the three tints released, `Window_ResetAll`, the battle bytes 0, `0x494E70`, `BattleTask_ClearAll`, `Sound_StopChannels`, `Task_Restart(Boot_Task)` (which does not return in the game; its recorder does) |
| `0x4327F0` | `BattleLoss_DrawPanels(shade)` | three `SPRT` of page (0x340, 0x100), CLUT (0, 0x1FA) |
| `0x432930` | `BattleLoss_DrawBlack` | a black `TILE` over 320 x 240 |
| `0x4329A0` | `BattleLoss_DrawCaption(shade)` | one `SPRT` 0xA8 x 0x18 at (76, 48) |
| `0x432A30` | `BattleLoss_DrawBar(width)` | a black `TILE` at (w + 0x20, 48) and a semi-transparent `POLY_G4` black to white from (w, 48) |

## 2. The cut against the code

`tools/band_rows.py --group BE1` (2026-09-29): 39 functions of the cut and
2 it does not list; 30 extents differ from the cut's by trailing padding
only, one by code.

- **`0x432430` and `0x432440` are not in the cut** (code no start list has,
  inside `0x432170`'s catalogue extent): `0x432430` is reached by
  `0x64AF70[2]`, `0x432440` by `0x64AFD8[0]`. Both taken here.
- **`0x432170`'s extent is 0x2C0 (to its jump table's end, `0x43242F`), not
  the cut's 752**: the cut ran over the two unlisted starts and the loss
  steps; the tool is right.
- **Hidden starts.** 28 of the 39 are hidden in a catalogue extent. Two of
  the hosts are ours: `Battle_PhaseDispatch` `0x42E400` (hosting
  `0x42ED90..0x42F640`) and `Battle_ActorSkipped` `0x431030` (hosting
  `0x4315C0`); neither holds the hidden code (ours of them are 0x70 and
  0x60 bytes, and `entries_logic.txt` lists them so), and every hidden start
  here is reached by address through a `.data` table - each is its own
  function. The other hosts (`0x42D710`, `0x42DB40`, `0x42F9D0`,
  `0x4319B0`, `0x432170`) are this group's or nobody's; section 12 lists the
  extents added.
- Every start of the table was read against the code: all 41 are functions
  (no case body, no data).

## 3. Divergence

None: every function is a faithful replacement. The dispatchers abort past
their tables where the original jumps through whatever follows (round9 doc
section 6's rule; seven: the six `jmp` dispatchers and
`BattleExtra_EquipRun`'s `call`). No `DIVERGENCE.md` entry is owed.
`DIVERGENCE.md` and `src/game/cheats.cpp` hold no patch inside the 41 (one
read, DIV-0020's of `0x42E09D`, section 1.2).

## 4. Calls across groups, and inbound calls

**Out, to BE4 (raw, `battle_e1_callees.h`, listed in the fuzz):**
`0x444660` (from `BattleHold_ShowAll`), `0x446D90` (`Cmd_AutoBattle`),
`0x44A910` (`BattleAction_AbilityNotice`, `BattleResult_LevelUpNotice`) -
the four edges of boss_harness.md 10.6.

**Out, nobody's this round (the engine standard set's recorders):**
`0x42E0E0`, `0x42E250`, `0x42E2F0` (BATE's, catalogue part 7), `0x452EB0`,
`0x452F10`, `0x494E70`, `0x5B9450` (the CRT's `memcpy`); and `0x42D780`, an
entry of `BattleExtra_EquipOpenSteps` (Capcom's, nobody's).

**Inbound, from outside the group:**

| Function | From | How |
|---|---|---|
| `BattleExtra_TallyOpen`, `_TallyCount`, `_TallyClose` | `0x42D770` / `0x42D760` (Capcom's, nobody's) | `.data` `0x64ADC8[1]`, `0x64ADBC[1..2]` |
| `BattleExtra_EquipDispatch` | `0x42D710` (Capcom's) | `0x64ADAC[2]` |
| `BattleHold_Dispatch` | `BattleInput_Dispatch` (ours) | `Battle_InputSteps[3]` |
| `Cmd_AutoBattle` | `BattleMenu_ConfirmDispatch` (ours) | `Battle_MenuSteps[5]` |
| `BattleAction_Kind3Dispatch` | `BattleAction_KindStep` (ours) | `BattleAction_KindSteps[3]` |
| `BattleAction_PickRandomAbility` | `BattleAction_AbilityCommit` (ours) | a call through `battle_actions`' `kPickTarget` (rebound) |
| `BattleAction_AbilityNotice` | `BattleAction_AfterStep` (ours) | `BattleAction_AfterSteps[1]` |
| `BattleEnd_AwaitRestore` | `0x4315B0` (Capcom's) | `0x64AF7C[2]` |
| `BattleResult_CountExpShares` | `BattleResult_SplitExp` (ours) | `kCountMembers` (rebound) |
| `BattleResult_LevelUpNotice` | `BattleResult_LevelUpStep` (ours) | `BattleResult_LevelUpSteps[1]` |
| `BattleResult_ZennyBonus` | `BattleResult_Setup` (ours) | `kZennyBonus` (rebound) |
| `Char_LevelUpGain` | `BattleResult_FindLevelUp`, `BattleResult_Setup` (ours); **BE7's `0x597FC0`** (12 calls) | `kLevelUpPending` (rebound); BE7 calls it raw until the merge |
| `BattleLoss_Dispatch` | `0x431540` (Capcom's) | `0x64AF70[2]` |

## 5. The fuzz

`src/game/battle_e1_fuzz.cpp`: one `Run`, `Group::engine`, 6,000 rounds a
function. The clone rows are `band_rows.py --clones`', each read against
the code.

**Shapes.** The steps `kStep` (21); the six `jmp` dispatchers `kDispatch`
with the absolute step byte (`state_cell` `0x929F01`, `0x929F02`,
`0x904AA2`, `0x904AA3`) drawn below their tables' counts;
`BattleExtra_EquipRun` (a `call` through its table) `kStep` with the same
draw of `0x929F02` below 2; the helpers and draws `kHelper` (12), with
`ret_mask 0xFF` on the three that answer in al
(`BattleAction_PickRandomAbility`, `BattleResult_CountExpShares`,
`BattleResult_ZennyBonus`) and `0xFFFF` on `Char_LevelUpGain`.

**Tables (`DataTable`, counts from the code):** `0x64ADE0` 3, `0x64ADEC` 3,
`0x64ADF8` 2, `0x64AE00` 2 (the steps' writes of `0x929F01` / `0x929F02`:
0 from `0x42D730`, + 1 at each step's end, - 1 from the list back to the
slots, 0 and + 1 from the slot input's cancel); `0x64AE48` 3
(`BattleHold_Regrow` resets); `0x64AEB4` 5 (the sixth dword is not code);
`0x64AFD8` 5 (then a zero dword).

**Callees the group lists** (beside the engine standard set):

- its own called directly: `BattleExtra_DrawTally` (two whole words),
  `BattleExtra_ApplyTally`, `BattleExtra_DrawPlus` (two words),
  `Char_LevelUpGain` (two bytes, `kFlag`: its caller tests ax against 0),
  the four loss draws (a byte, none, a byte, a word);
- BE4's three by address (above);
- **nine standard callees at what the callee reads**, because the
  originals push a byte or a word in a whole register whose upper half is
  left over (the entry's `ecx` in a local slot, a previous answer's `eax`,
  `edx` from the caller): `Menu_DrawBackdrop` (a byte - ours of it masks
  `kind & 0xFF`), `Stat_AddClamped` / `Stat_AddCap999` (the delta's word),
  `BattleWin_DrawPartyStatus` / `_DrawCommandCross` (two words),
  `BattleWin_DrawCommandLabel` (a byte), `Menu_DrawIcon` (a byte, two
  words, three bytes), `Battle_ClearStatus` (the actor's byte),
  `Sprite_AnimFromSet` (a byte and a word), `PartySet_Select` (two bytes).
  The harness's rows for these are whole words; a fold after the wave could
  take these masks (section 13).

**Regions** (after the engine frame's 27): the sprintf buffer `0x904BA0`
(0x20; also where `0x904ABC + 0x904AB4` lands past the battle bytes), the
tally's counters `0x675E88..0x675EC3`, `MoveScript_WaitWordDA`, character
records 1..7 past the engine frame's (`0x903B24..0x903F8F`), `0x939A04..0F`,
`Text_Records` (0x40), the two CLUT runs (0x200 each), `0x90412C`, and the
random picks' id tables `0x64AEC8..0x64AF07` (drawn, so every flags branch
of the pick is reached, not only the ids the disc's table holds): 37
regions, 38,343 bytes.

**Seeds:** every round the party count 1..3 (0..3 a third of the time -
the loops index `ObjTrio` by it; above 3 is described, section 7) and the
tally row never negative. Per function: the row and the target at the
step's reach, the press; `0x939A08` at 0x31..0x33 / 0x4F..0x51; the timers
at 1 and around; the slot cursor 0 / 3 / 1 / 2; the list's scroll and cursor
at 0, 1, 8, 9, the page bounds 0x6E / 0x76 / 0x77 / 0x7E / 0x7F and
scroll + 8 / + 9, the scroll word 0, the count 0; the confirm and cancel
words against `Input_Pressed` (none, either, both, any); the menu member
and its cross size at 0..9 and 0xFF; `Input_Held` bit 8 both ways; the
entry order and its item commands, the count at 0..3, 5, 0x80, 0xFF, the
auto test; `0x904B82` at 0; the action word 0x24 / 0x25 / 0x8C / others and
the actor on both sides; the notice's message byte, slot, members' bit 3
and the acting enemy 3..10 (0, 2 sometimes); the members' busy bit and the
cursor at the count; `+0x96` / `+0x97` = 7 and `+0x134` bit 10; the
records' levels (0, 1, 2, 10, 40, 97..100) and EXP (small, large, any);
the loss bar at 0xF0..0xF5, 0x2C, 0, 0xFFFF, `Input_Held` 0.

**Args:** the tally's `(x, y)` `(0x37, 0x37)` two times in three, else any;
`Char_LevelUpGain`'s roster 0..7 and `what` 0..6 two times in three (any
byte else), garbage above the byte half the time; the draws' shade and
width.

**Disturbance** (from the hash only): BATE's step bytes, the tally's
targets [3..5], the backdrop's kind, windows 0 and 1's cursor bytes, the
scroll word, `Input_Pressed` / `Input_Held`, the roster (0..7), the old
level, the loss bar, the action word, the party count (0..3) - every cell a
function reads again after a call.

**Result (this worktree):**

    shadow      battle_e1 self-test: 246000 rounds over 41 functions (6000 each), 1072742 calls to the stand-ins, 0 MISMATCHES; 38343 bytes of state (37 regions) and the stand-ins' log compared

Every listed callee and every table entry was reached (the coverage line: `0x446D90` 678, `BattleQueue_Push` 720, `0x42E250` 545, `0x452EB0` 786, `0x452F10` 609 the rarest). Two first runs were not clean, both the fuzz's or the reading's: `Cmd_AutoBattle` walks `0x904AB6[i]` (the loop re-enters after the `xor eax, eax`, so the index is the counter - a first reading had it always entry 0), and `BattleAction_AbilityNotice` faulted on both sides for an actor past 0x30 (section 7.3), now seeded 0..10.

## 6. Negative controls

`be1_controls.py` (a scratch script, not committed; run from `build/`): each control
plants one change in `battle_e1.cpp` anchored on a unique string,
rebuilds, runs that clone alone (`BOF3X_BE1_ONLY=<address>`), restores and
rebuilds. Counts are rounds of 6,000 that mismatched, in this worktree.

| n | Clone | Planted | Refused (rounds of 6,000) |
|---|---|---|--:|
| C1 | `0x42D7A0` | TallyOpen: 0x929F08 = 1, not 0 | 2959 |
| C2 | `0x42D7F0` | TallyCount: jae for ja (the count at its target waits) | 203 |
| C3 | `0x42D7F0` | TallyCount: five rows, not six | 544 |
| C4 | `0x42D880` | TallyClose: state 2, not 1 | 2959 |
| C5 | `0x42D8C0` | DrawTally: the last line 1 higher | 6000 |
| C6 | `0x42D8C0` | DrawTally: the row without esi's left-over upper half | 2018 |
| C7 | `0x42DA60` | ApplyTally: below-or-equal 0x32 | 563 |
| C8 | `0x42DA60` | ApplyTally: target [3] not re-read after the call | 4 |
| C9 | `0x42DB40` | DrawPlus: the upright 1 longer | 6000 |
| C10 | `0x42DC00` | EquipDispatch: entries 0 and 2 swapped | 4012 |
| C11 | `0x42DC10` | EquipOpenDispatch: entries 0 and 2 swapped | 4012 |
| C12 | `0x42DC20` | EquipOpen: the timer 3 | 2959 |
| C13 | `0x42DC80` | EquipFadeIn: 0x675EC0 = 2 | 1594 |
| C14 | `0x42DCD0` | EquipRun: the backdrop before the step | 6000 |
| C15 | `0x42DCF0` | EquipSlotInput: xor 2 | 4472 |
| C16 | `0x42DCF0` | EquipSlotInput: message 0xDD | 6000 |
| C17 | `0x42DCF0` | EquipSlotInput: the cancel timer 6 | 1182 |
| C18 | `0x42DE50` | EquipListInput: the cursor stops at 0x7E | 95 |
| C19 | `0x42DE50` | EquipListInput: the page-on bound 0x6F | 19 |
| C20 | `0x42DE50` | EquipListInput: the count from +0xC | 509 |
| C21 | `0x42E040` | EquipLeaveDispatch: entries swapped | 6000 |
| C22 | `0x42E050` | EquipFadeOut: Transition_Start(3) | 1594 |
| C23 | `0x42E090` | EquipLeave: four bytes copied | 2948 |
| C24 | `0x42ED90` | BattleHold_Dispatch: entries 0 and 2 swapped | 4012 |
| C25 | `0x42EDA0` | Shrink: by 1 | 5540 |
| C26 | `0x42EE00` | ShowAll: the icons 1 right | 6000 |
| C27 | `0x42EE00` | ShowAll: the held bit 0x200 | 2991 |
| C28 | `0x42EEA0` | Regrow: to 9 | 470 |
| C29 | `0x42EF50` | Cmd_AutoBattle: command 4 given back | 826 |
| C30 | `0x42EF50` | Cmd_AutoBattle: +0x134 bit 5 | 832 |
| C31 | `0x42EF50` | Cmd_AutoBattle: round flag 0x20 | 4295 |
| C32 | `0x42F5E0` | Kind3Dispatch: the table reversed | 4774 |
| C33 | `0x42F5F0` | Kind3Banner: state 8 | 6000 |
| C34 | `0x42F640` | Kind3Wait: round flag 8 | 2265 |
| C35 | `0x42F9D0` | PickRandomAbility: party side 0xC0 | 46 |
| C36 | `0x42F9D0` | PickRandomAbility: 0x8D for 0x8C | 1042 |
| C37 | `0x42F9D0` | PickRandomAbility: the party side's targets swapped | 527 |
| C38 | `0x42FE20` | AbilityNotice: flag 0x100 | 14 |
| C39 | `0x42FE20` | AbilityNotice: the slot, not slot - 1 | 11 |
| C40 | `0x4315C0` | AwaitRestore: status mask 0xBF7F | 1778 |
| C41 | `0x4315C0` | AwaitRestore: position - 1 | 1756 |
| C42 | `0x4319B0` | CountExpShares: bit 0x800 | 1550 |
| C43 | `0x431C10` | LevelUpNotice: rows + 5 | 4023 |
| C44 | `0x431C10` | LevelUpNotice: byte +5 for +7 | 2149 |
| C45 | `0x431FE0` | ZennyBonus: 6 in +0x97 | 855 |
| C46 | `0x432170` | Char_LevelUpGain: 98 the top | 282 |
| C47 | `0x432170` | Char_LevelUpGain: stat 4 the high nibble | 104 |
| C48 | `0x432170` | Char_LevelUpGain: a negative sum 1 | 353 |
| C49 | `0x432170` | Char_LevelUpGain: jge for jg | 161 |
| C50 | `0x432430` | BattleLoss_Dispatch: the table reversed | 4774 |
| C51 | `0x432440` | FadeOut: row 0x1B | 6000 |
| C52 | `0x432460` | ResetParty: the position words 2 on | 2713 |
| C53 | `0x432460` | ResetParty: member 2 at count 2 | 1783 |
| C54 | `0x4325F0` | Show: the bar 0x2D | 2959 |
| C55 | `0x432630` | BarGrow: the bound 0xF3 | 553 |
| C56 | `0x432630` | BarGrow: the grey / 4 | 2986 |
| C57 | `0x432750` | Restart: 0x904AE6 for 0x904AE5 | 2959 |
| C58 | `0x4327F0` | DrawPanels: the third 0x41 high | 6000 |
| C59 | `0x432930` | DrawBlack: 241 high | 6000 |
| C60 | `0x4329A0` | DrawCaption: 0x19 high | 6000 |
| C61 | `0x432A30` | DrawBar: x1 w + 0x21 | 6000 |
| C62 | `0x432A30` | DrawBar: the width's 15 bits | 1015 |

All 62 refused by a count (exit 3, the mismatch on the planted function only); none equivalent. The smallest counts are the narrowest branches: C19 (the page-on bound, 19), C38 / C39 (the notice's learning path, 14 / 11), C8 (the target re-read after `Stat_AddClamped`, 4: the disturbance moves it in about one round in 1,500).

## 7. Latent defects and unchecked indexes (Capcom's, kept)

Described, not fixed; ours reproduces each (the dispatchers excepted).

1. **The dispatchers are unchecked** (all seven): a step byte past its
   table jumps through the next table's dwords (they are nested:
   `0x64ADE0` runs into `0x64ADEC`, `0x64AEB4` into
   `BattleAction_AbilitySteps`, `0x64AFD8` into a zero dword). Ours aborts
   past each table (section 3). Unreachable: every writer of the step bytes
   is read (section 5's counts).
2. **`BattleExtra_TallyCount` indexes its three tables by a signed byte**
   (`movsx`): below 0 it reads `0x64ADD0 - n` and writes `0x675E88 - 4n`
   in `.data`. Unreachable: `0x929F08` is set to 0 and only counted up. Not
   drawn.
3. **`BattleAction_AbilityNotice` reads the acting enemy's `+0x106` at
   `0x93BA66 + (0x904B34 - 3) * 0x128` unchecked**: a party actor reads the
   task slots before the enemies; an actor past 0x30 reads past the image
   and faults (it did, both sides, before the seed kept the actor to 0..10).
   The ability id then indexes `Ability_Records` unchecked (the 24-byte
   rows run into the rest of `.data` for large ids). In a battle the actor
   is the enemy that acted.
4. **The party loops index `ObjTrio` by the count `0x904AB0` and the entry
   order** unchecked: a count above 3 writes `+1` / reads `+0x134` past the
   three records (into `WindowRecords` at `0x803160`); an enemy id in the
   entry order (3..10) is read as a member (`Cmd_AutoBattle`). Neither
   drawn; the count is the party's.
5. **`Char_LevelUpGain`'s roster byte is unchecked** (any byte reads a
   record past the eight and 99 rows past the table's), and the EXP
   compare is signed: an EXP of `0x80000000` or more gives level 1.
6. **`BattleResult_LevelUpNotice` counts the rows from the OLD level's
   `Char_ExpTable` row** (bytes `+6` / `+7` of row `0x904B72`), not the new
   one's; what those two bytes are was not read. An observation, not a
   fault.

## 8. What nothing reached

- **The game.** Every function is fuzz only here; the owner's
  `dragonTransform` route enters three (section 11), which the
  coordinator's live check runs. BATE (mode 9) and the loss screen are
  reached by no recorded route.
- **Task_Restart's no-return**: the recorder returns, so the fuzz compares
  what `BattleLoss_Restart` does before it, not that nothing follows.
- **The disc's id tables as they are**: the fuzz draws `0x64AEC8..` (section
  5); with the disc's values only some of the pick's flags branches occur.
- **The upper halves the originals push** (sections 1.3, 5): compared only
  as the callees read them.
- Sections 7.2..7.5's paths (not drawn, on purpose).

## 9. Named data (`symbols.toml` `[[data]]`)

`BattleExtra_EquipSteps` `0x64ADE0` (3), `BattleExtra_EquipOpenSteps`
`0x64ADEC` (3), `BattleExtra_EquipRunSteps` `0x64ADF8` (2),
`BattleExtra_EquipLeaveSteps` `0x64AE00` (2), `BattleHold_Steps` `0x64AE48`
(3), `BattleAction_Kind3Steps` `0x64AEB4` (5), `BattleLoss_Steps`
`0x64AFD8` (5). BATE's state table `0x64ADAC` and its two sub-tables
`0x64ADBC` / `0x64ADC8` are read by nobody's code (`0x42D710`, `0x42D760`,
`0x42D770`) and are not named here.

## 10. The rebinding

Every raw reference to the 41 in `src/game` (`band_rows.py --refs`: 76
references to 15 of them), and what became of it:

| File | Reference | Now |
|---|---|---|
| `battle_actions_callees.h` | `kPickTarget = 0x42F9D0` | `bof3::addr::BattleAction_PickRandomAbility` (the value unchanged: the fuzz's keys stand); its comments name `0x42F5E0`, `0x42FE20` |
| `battle_result_callees.h` | `kCountMembers`, `kZennyBonus`, `kLevelUpPending` | `bof3::addr::BattleResult_CountExpShares`, `_ZennyBonus`, `Char_LevelUpGain` (values unchanged) |
| `battle_actions.cpp`, `battle_phases.cpp`, `battle_result.cpp`, `scena_sx2.cpp` | comments | the names beside the addresses |
| `battle_phases_fuzz.cpp` (`kInputTargets`, `kMenuTargets`), `battle_actions_fuzz.cpp` (`case 0x42F9D0`, `kAbilityCommitCalls`), `battle_result_fuzz.cpp` (`kLevelUpHad`) | the fuzz files' keys and call sites | **left raw** (round ten's rule: a fuzz file's keys) |
| `boss_harness.h` (`kEngineBands`' `0x42D7A0`), `boss_harness.cpp` (a comment), `boss_harness_eh.cpp` (EH's self-test copies `0x42D8C0`, `0x42F5E0`, `0x42F5F0`, `0x42F640` and lists `0x42DB40` - Capcom's code on both sides by design) | the harness | **left raw**: the harness is not a group's to edit, and a band bound is a coordinate |

No reference in a file another group of this wave is writing.

## 11. The live route's coverage

`tools/recipes/dragonTransform.txt` (the owner's, 2026-09-28) enters three
of the 41 ([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md)
section 5; `analysis/calltrace/reach_dragon/reach_dragon_new.txt` rows
4098..4188, the caller column there is the catalogue extent's, not the
caller): `BattleResult_CountExpShares` `0x4319B0`, `Char_LevelUpGain`
`0x432170`, `BattleResult_ZennyBonus` `0x431FE0` - the result screen. The
coordinator's live check after the wave runs them; the other 38 are fuzz
only.

## 12. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (a comment line, then 36 lines): every
one of the 41 at the extent read, but the five already listed at that
extent (`0x42D8C0`, `0x42DA60`, `0x4327F0`, `0x432930`, `0x4329A0`). Smaller
extents for the listed hosts `0x42DB40` (0xBA, was 0x593), `0x42F9D0` (0xD3,
0xE0), `0x4319B0` (0x61, 0x70), `0x431FE0` (0x68, 0x70), `0x432170` (0x2C0,
0x679), `0x432A30` (0x13B, 0x140); `0x42D710 1AA` stays (nobody's) with its
three hidden starts listed after it.

## 13. For the coordinator: what the harness lacked

Nothing that stopped the group. For a fold after the wave: the nine
standard rows of section 5 at the masks their callees read (every engine
group that pushes a byte in a whole register meets them); a `kDispatch`
through a `call` (not a `jmp`) is a `kStep` with `state_cell` here, which
works but is not the documented shape.
