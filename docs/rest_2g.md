# Group R2G: window code of record handlers 4, 5 and 6

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, on the
round branch's tip `fa583bc`. **48 functions ours** (`src/game/rest_2g.cpp`,
`rest_2g.h`, `rest_2g_callees.h`, shadow name `rest_2g`): the cut's 48 rows
for R2G (`analysis/round14_cut.tsv`), no start dropped, none added (the band
tool's "code no list has" is 0). Each read to its last instruction with
capstone and fuzzed through the scenario harness's **field** mode (used
unchanged): 192,000 rounds, **0 mismatches**, narrow and under
`BOF3X_WIDE=1`; 91 of 91 controls refused (section 5). Thirteen `.data`
tables named. Fuzz only here: one function is entered by the traced routes
(section 8).

**What the band is.** Not "boot: text, windows and menus" in general: every
row is a handler, kind or state of a window record (`WindowRecords`
`0x803160`, 22 of `0x24`) that `Field_RunTaskRecords` runs with `0x905B84`
naming it ([`window-task.md`](window-task.md) section 4), or a helper of one:

- **Record handler 4** (`Window_Handler4Kinds`, the battle result): its kind
  0 - the level-up window's one state - and the EXP a party slot still needs,
  which the result's EXP window prints.
- **Record handler 5** (the gene windows, BE7's names): its kind dispatch and
  kind 0, the grid: its open state and its two slides (BE7 took the grid's
  draw and kinds 1..4, [`battle_e7.md`](battle_e7.md)).
- **Record handler 6** (`MenuList_Run`, DH's): kinds 5..13 and 15..19
  (DH took 0..4, [`menu_lists.md`](menu_lists.md)) - each runs its state from
  its own `.data` table by the record's `+3`, then draws a panel of ours at
  the record's `(+4, +6)`; and 26 of the slide states those tables (and DH's,
  and other handlers') hold.

No PSX twin of any of the 48 is in `analysis/pairs_propagated.json`; the
names come from the code and the tables that reach it. What each screen is
in play is not stated here: the panels' own docs ([`field_o.md`](field_o.md),
[`menu-windows.md`](menu-windows.md)) name what they draw.

## 1. The functions

Extents are read to the last `ret` (the cut's sizes are the catalog's and
run on into the padding; no extent differs from the tool's by code).

### 1.1 Handlers 4 and 5

| PC | Bytes | Name | Reached by | What it does |
|---|--:|---|---|---|
| `0x597FA0` | 0x1A | `BattleResultWin_LevelUpStates` | `Window_Handler4Kinds`' stack table [0] | `call [esp + 4 +3]` over a stack table of one: `BattleResultWin_DrawLevelUp` |
| `0x598810` | 0x79 | `BattleResultWin_ExpToNext` | E8 from `BattleResultWin_DrawExp` | (slot) the id `ObjTrio[slot] +0x89` made a roster index `r` by `0x4469D0`; the first word of `Char_ExpTable` rows `0..level` of `r` summed, less the record's EXP `+0xC`; 0 when below |
| `0x598890` | 0x3E | `Window_Handler5Kinds` | `Field_RunTaskRecords`' handler table [5] | `call [esp + 4 +2]` over five: `GeneWin_GridStates`, BE7's `GeneWin_ChoiceStates`, `_ListStates`, `_List2States`, `_List3States` |
| `0x5988D0` | 0x2E | `GeneWin_GridStates` | handler 5's stack [0] | `call [esp + 4 +3]` over three: `GeneWin_GridOpen`, `_GridSlideIn`, `_GridSlideOut` |
| `0x598900` | 0xA7 | `GeneWin_GridOpen` | its stack [0] | `+8 = 0x18`, `+9 = 0x11`; the low byte of the eighteen words `0x939820..` cleared; a battle task `BattleTask_Create(0, 9)` per non-zero cell of `0x939A60[18]`, its `+0xB` and `+0x4B` the cell index; one `BattleTask_Create(0, 0xA)`; `+3 + 1` |
| `0x5989B0` | 0x3F | `GeneWin_GridSlideIn` | its stack [1] | `GeneWin_DrawGrid`; from the right (`+0xA` 0) x steps -0x20 while above 0x42, else is 0x42; from the left +0x20 while below, else 0x42 |
| `0x5989F0` | 0x3D | `GeneWin_GridSlideOut` | its stack [2] | `GeneWin_DrawGrid`; right: at x >= 0x142 the window is freed (`jmp Window_FreeCurrent`), else +0x20; left: at x <= -0xBE freed, else -0x20 |

### 1.2 Handler 6's kinds

| PC | Bytes | Name | `MenuList_Kinds` | State table | Then |
|---|--:|---|--:|---|---|
| `0x59A030` | 0x3E | `MenuList_StatsPanel` | 5 | `MenuList_StatsStates` `0x66B044` (7) | `Menu_DrawStatsPanel(+4, +6, the record of party slot +0xA)` |
| `0x59A190` | 0x3E | `MenuList_EquipPanel` | 6 | `MenuList_EquipStates` `0x66B060` (3) | `Menu_DrawEquipPanel` likewise |
| `0x59A200` | 0x40 | `MenuList_ExpPanel` | 7 | `MenuList_ExpStates` `0x66B06C` (3) | `Menu_DrawExpPanel(..., 0)` |
| `0x59A270` | 0x40 | `MenuList_NextLevelPanel` | 8 | `MenuList_NextLevelStates` `0x66B078` (3) | `Menu_DrawExpPanel(..., 1)` |
| `0x59A2E0` | 0xB6 | `MenuList_WideTitleBox` | 9 | `MenuList_WideTitleStates` `0x66B084` (3) | `Menu_DrawTitleBox(+4, +6, 0x118, 0x13, style)`; word `+0x10` not 0: its system text at `(+4 + 7, +6 + 3)`; `+0x10` 0x1A or 0x31: system text 0xF there too |
| `0x59A400` | 0x37 | `MenuList_CursorBox` | 10 | none | `Menu_DrawCursorBox(+4, +6, MenuList_CursorBoxSizes[+0xA], +0xB, 6)` |
| `0x59A440` | 0x6B | `MenuList_IconWheel` | 11 | `MenuList_IconWheelStates` `0x66B0A0` (3) | `Menu_DrawIconWheel(+4, +6, +0xA, +0xC, s16 +0x10)`; with `+0xC` set, `+0x10 += 0x40` and a cursor box 0x44 x 0x34 at `(+4 + 4, +6)` |
| `0x59A560` | 0x20 | `MenuList_ItemList` | 12 | `MenuList_ItemListStates` `0x66B0AC` (5) | `Menu_DrawItemList(record)` |
| `0x59A640` | 0x34 | `MenuList_ButtonRow` | 13 | `MenuList_ButtonRowStates` `0x66B0C0` (3) | `Menu_DrawButtonRow(+4, +6, +0xA, +0xB, 0)` |
| `0x59A6B0` | 0xD | `MenuList_LabelList` | 15 | none | `MenuList_DrawLabelList(record)` |
| `0x59A6C0` | 0x21D | `MenuList_DrawLabelList` | (E8 from kind 15) | | a box, a title and up to n lines of text from the rows `0x66B158` (6 bytes: n, then a text index a line) and the pointers `0x66B12C`, the lines `+0xC` and `+0xB` raised; their pieces |
| `0x59A8E0` | 0x47 | `MenuList_EquipCompare` | 16 | `MenuList_EquipCompareStates` `0x66B184` (5) | `Menu_DrawEquipCompare(the record of party slot +0xC, +4, +6, +0x20, +0xD, record)` |
| `0x59A9C0` | 0x20 | `MenuList_AbilityPanel` | 17 | `MenuList_AbilityStates` `0x66B198` (5) | `Menu_DrawAbilityPanel(record)` |
| `0x59AA10` | 0x20 | `MenuList_ItemPanel` | 18 | `MenuList_ItemPanelStates` `0x66B1AC` (5) | `Menu_DrawItemPanel(record)` |
| `0x59AA30` | 0x20 | `MenuList_ReservePanel` | 19 | `MenuList_ReserveStates` `0x66B1C0` (3) | `0x59AA80(record)` (R2H's, the reserve list) |

"The record of party slot k" is `0x66972C[0x904062[k]]` (the
character-to-record byte DH's doc section 8 describes). Every kind re-reads
`0x905B84` after its state, as the original. Kind 14 is DI's `0x59CB20`
(handler 7's code), kind 20 R2H's `0x59ADE0`.

### 1.3 The slide states

Each moves the record's x (`+4`) by 0x20 or y (`+6`) by 0x10, re-reads the
record, and when the word (signed) is past its bound holds it there and sets
`+3` to 0. "Ge" / "Le" in a name: the original holds at the bound itself too
(`jl` / `jg` over the store where the others have `jle` / `jge`).

| PC | Name | Move | Held when | Tables |
|---|---|---|---|---|
| `0x599C30` | `MenuList_SlideRightTo182` | x +0x20 | > 0xB6 | `MenuList_PanelStates[1]` |
| `0x599C60` | `MenuList_SlideLeftTo92` | x -0x20 | < 0x5C | `MenuList_PanelStates[3]` |
| `0x599C90` | `MenuList_SlideRightTo92` | x +0x20 | > 0x5C | `MenuList_PanelStates[4]` |
| `0x599CC0` | `MenuList_SlideLeftOff300` | x -0x20 | <= -300 (DIV-0041: -353) | `MenuList_PanelStates[7]` |
| `0x599CF0` | `MenuList_SlideRightTo17Ge` | x +0x20 | >= 0x11 | `MenuList_PanelStates[8]` |
| `0x599D20` | `MenuList_SlideLeftTo182` | x -0x20 | < 0xB6 | `MenuList_PanelStates[10]` |
| `0x599D90` | `MenuList_SlideLeftTo200` | x -0x20 | < 0xC8 | `MenuList_MoneyStates[2]` |
| `0x599DF0` | `MenuList_SlideLeftOff100` | x -0x20 | < -100 (DIV-0041: -153) | `MenuList_TimeStates[1]` |
| `0x599E20` | `MenuList_SlideRightTo16` | x +0x20 | > 0x10 | `MenuList_TimeStates[2]` |
| `0x599F70` | `MenuList_SlideDownTo42` | y +0x10 | > 0x2A | `MenuList_IconStates[2]` |
| `0x59A070` | `MenuList_SlideLeftBesidePanel` | x -0x20 | < s16 `0x8031F4` + 0x78 (record 4's x, an int) | `MenuList_StatsStates[2]` |
| `0x59A0B0` | `MenuList_SlideUpTo62Le` | y -0x10 | <= 0x3E | `MenuList_PanelStates[5]`, `MenuList_StatsStates[3]` |
| `0x59A0E0` | `MenuList_SlideDownToPanelRow` | y +0x10 | >= 0x3E + 0x36 `+0xA` (the member panels' rows) | `MenuList_PanelStates[6]`, `MenuList_StatsStates[4]` |
| `0x59A130` | `MenuList_SlideLeftOff180` | x -0x20 | <= -180 (DIV-0041: -233) | `MenuList_StatsStates[5]` |
| `0x59A160` | `MenuList_SlideRightTo137Ge` | x +0x20 | >= 0x89 | `MenuList_StatsStates[6]` |
| `0x59A1D0` | `MenuList_SlideLeftTo166` | x -0x20 | < 0xA6 | `MenuList_EquipStates[2]` |
| `0x59A240` | `MenuList_SlideRightTo45` | x +0x20 | > 0x2D | `MenuList_ExpStates[2]`, `MenuList_NextLevelStates[2]` |
| `0x59A2B0` | `MenuList_SlideLeftOff110` | x -0x20 | < -110 (DIV-0041: -163) | `MenuList_ExpStates[1]`, `MenuList_NextLevelStates[1]` |
| `0x59A4B0` | `MenuList_SlideRightOffColumn` | x +0x20 | > 0x140 + 75 (`+0xB` & 1) | `MenuList_IconWheelStates[1]` |
| `0x59A510` | `MenuList_SlideLeftToColumn` | x -0x20 | < 75 ((`+0xB` & 1) + 2) | `MenuList_IconWheelStates[2]` |
| `0x59A610` | `MenuList_SlideLeftTo152` | x -0x20 | < 0x98 | `MenuList_ItemListStates[4]`, `MenuList_ItemPanelStates[4]`, `0x66AEF0`, `0x66B588` |
| `0x59A930` | `MenuList_SlideLeftTo165` | x -0x20 | < 0xA5 | `MenuList_EquipCompareStates[2]` |
| `0x59A960` | `MenuList_SlideLeftTo17` | x -0x20 | < 0x11 | `MenuList_PanelStates[2]`, `MenuList_EquipCompareStates[3]`, `MenuList_AbilityStates[4]`, `0x66B2F8`, `0x66B390` |
| `0x59A990` | `MenuList_SlideRightTo165` | x +0x20 | > 0xA5 | `MenuList_EquipCompareStates[4]` |
| `0x59A9E0` | `MenuList_SlideRightTo150` | x +0x20 | > 0x96 | `MenuList_AbilityStates[3]`, `0x66B2FC` |
| `0x59AA50` | `MenuList_SlideLeftTo150` | x -0x20 | < 0x96 | `MenuList_AbilityStates[2]`, `MenuList_ReserveStates[2]`, `0x66B1D4`, `0x66B2E0`, `0x66B2F4` |

The tables named here are `[[data]]` entries in `symbols.toml` (twelve state
tables and `MenuList_CursorBoxSizes` `0x66B090`, four word pairs). Each
state table's count is the run to the next table: the readers do not bound
`+3`; the run of code pointers goes on through the following tables, which
ours does not follow (section 7).

## 2. Divergence

None owed: every function is a faithful replacement. **DIV-0041** has six
patch sites inside these bodies (`widescreen.cpp` `kSlides`): the imm32 of
`mov ecx` at `0x599CC6`, `0x599DF6`, `0x59A136`, `0x59A2B6` and the imm16 of
`cmp cx` at `0x598A08`, `0x598A1C`. Ours reads each bound from its operand on
every call (as `menu_lists` and `battle_e7` do), and `Rest2G_Inject`
refuses to start unless each operand is preceded by its instruction's bytes
and holds the original bound or the widened one. No other patch: `grep` of
`DIVERGENCE.md`, `cheats.cpp`, `labels.cpp`, `widescreen.cpp` and every
`src/game` file for the 48 addresses and their sites (2026-10-04). No
full-frame fill in the band.

**One ledger text to correct (the coordinator's):** DIV-0027's closing note
names "`MenuList_TitleBox` for help `0x1A` / `0x31`" among the help-line
choosers that draw message `0xF`. DH's `MenuList_TitleBox` `0x599FA0` has no
such test; the function that draws message `0xF` for `0x1A` / `0x31` is this
group's `MenuList_WideTitleBox` `0x59A2E0`. Nothing of it is patched.

## 3. Arguments, answers and upper halves

Every call goes through the harness (`SH_CALL` for ours, `SH_AT` by address
for the two callees no group holds yet). Where the original pushes a whole
register, what lies above the value:

- **Exact, so ours builds it** and the fuzz compares all 32 bits:
  `0x4469D0`'s argument (the id under `(5 slot)`'s upper bytes, the register
  the record offset was built in); `Menu_DrawIconWheel`'s x and kind (under
  the s16 angle's sign-extension); `Menu_DrawButtonRow`'s x (under the record
  pointer's upper half); `Menu_DrawEquipCompare`'s x (under `+0x20`'s upper
  half) and record index (under x's upper three bytes); the panels' x, y and
  record in kinds 5..8 (zero-extended). R1E's toolchain caution (a value
  built as `(p & 0xFFFFFF00) | byte` emitted as the byte) was **not met**:
  `MenuList_EquipCompare`'s record index is such a value and its control EC1
  (the upper bytes dropped) is refused in every round.
- **Garbage, so the stand-in compares what the callee reads**: the y of the
  title box, cursor box, button row and equipment compare, the title box's and
  the label list's colour, the cursor box's blink, the button row's set and
  selection, the icon wheel's lit bits, `Text_DrawAt`'s x and y,
  `Msg_SystemPtr`'s id (each the low word or byte, as `field_o.cpp` /
  `menu_windows.cpp` read them).
- **Answers**: `BattleResultWin_ExpToNext` answers the whole `eax` (its caller
  prints it with `%6d`): `ret_mask` 0xFFFFFFFF. `0x4469D0` answers in `al`;
  the original stores it over its own argument slot and reads it back masked.
  `BattleTask_Create`'s answer is read as a byte.

## 4. The fuzz (`rest_2g_fuzz.cpp`)

`BOF3X_SHADOW=rest_2g`, field mode, 4,000 rounds a function; `BOF3X_R2G_ONLY`
picks clones by name (the controls). The clone table is `band_rows.py
--clones --harness scenario`'s, names given: 46 `kState` (void, no
arguments), `BattleResultWin_ExpToNext` and `MenuList_DrawLabelList` `kCall`.

- **Regions** beyond the field ones: `WindowRecords` (22 x 0x24), the
  character records past the style region (`0x903A94..0x903F90`), the grid's
  words `0x939820` and cells `0x939A60`, the battle task pool `0x93A000` (48
  x 0x84). `0x905B84` lies in the field region `0x905B70..`.
- **Seed**: `0x905B84` one of the 22 records; that record's `+3` below the
  function's table (its stack table's length for the stack dispatchers, 1
  for the level-up kind), `+2` below 5 for handler 5; `+0xA..+0xD` at
  {0..4, 0xFF, random}; `+0x10` at 0, 0x1A, 0x31, their neighbours or random;
  record 4's x at 0x62, 0x10, 0x7F80, 0x8000 or random; the party bytes, the
  grid cells (a third filled), the battle party's ids, eight records' levels
  (1, 10, 50, 98, 99, 0, 0xFF, random) and EXP; a slide's word one step
  before its bound within two (the grid's slides, which compare before they
  move, around the bound itself), or random one round in six. The `args`:
  `ExpToNext` a slot of 0..3 or any byte under random upper bytes,
  `DrawLabelList` the current record (three in four) or another.
- **Tables**: the twelve state tables, swapped for stand-ins; every handler
  address in them is listed as a callee with an effect (**louder than the
  harness's handler recorder**: the kinds read the record after their state),
  and so are `GeneWin_DrawGrid`, `Menu_DrawTitleBox`, `Menu_DrawIconWheel`,
  `Menu_DrawBox`, `Menu_DrawPieces`, `Text_DrawAt`, `Msg_SystemPtr` (its answer
  into the text buffer, as the field set's) and `BattleTask_Create` (also a
  grid cell flipped): each moves from the noise - `0x905B84` repointed, a
  field of the current record, a party byte, the style byte or a grid cell.
  `0x4469D0`'s stand-in answers 0..7 under garbage and moves that record's
  level and EXP. The group's `disturb` moves the same from the hash alone.
- **Stack immediates**: `0x597FA0`'s, `0x598890`'s and `0x5988D0`'s (nine)
  re-aimed at handler recorders; ours calls the same addresses through
  `Phase`.

Result in this worktree (2026-10-04, `fa583bc` + this group):

    rest_2g self-test: 192000 rounds over 48 functions (4000 each), 469625 calls to the stand-ins, 0 MISMATCHES;
      32056 bytes of state (43 regions) and the stand-ins' log compared

The same under `BOF3X_WIDE=1` (469,621 calls), 0 mismatches. Coverage: every
callee and every table handler was reached - `BattleTask_Create` 30,537,
`Window_FreeCurrent` 1,670 (the grid freed), `Msg_SystemPtr` 4,143 (the help
text's second call included), `Text_DrawAt` 103,945, `Menu_DrawPieces`
193,924, the 23 table handlers 542..13,183 each, the nine stack handlers
773..4,000. `'*'` and `'*'` wide: section 9.

## 5. Controls

`r2g/controls.py` (scratch): each plant replaces a string that occurs once in
`rest_2g.cpp`, rebuilds, runs the self-test on the clones whose name contains
the filter (`BOF3X_WIDE=1` for the literal-bound plants), restores and
rebuilds. **91 planted, 91 refused** (exit 3, by a count of mismatching
rounds of 4,000). Every function has at least one. The first run left one
unrefused, U1: the grid slide-out's seed put x a step before the bound, but
the slide-out compares before it moves - the fuzz's fault; the grid slides
are now seeded around the bound itself (U1 0 -> 106, I1 3 -> 58, U3 3 ->
297, U4 2 -> 385). **The thinnest**: I1 (58), U1 (106), U2 (158), E5 (204).

| # | Function | Plant | Refused |
|---|---|---|--:|
| L1 | `BattleResultWin_LevelUpStates` | the state called twice | 4,000 |
| E1 | `BattleResultWin_ExpToNext` | level rows, not level + 1 | 2,465 |
| E2 | | the argument without `(5 slot)`'s upper bytes | 402 |
| E3 | | the EXP table stride 0x310 | 1,769 |
| E4 | | the id from `+0x88` | 3,514 |
| E5 | | a level read before the call | 204 |
| W1 | `Window_Handler5Kinds` | the kind from `+3` | 3,162 |
| W2 | | kinds 1 and 2 swapped | 1,628 |
| G1 | `GeneWin_GridStates` | slide in and out swapped | 2,707 |
| O1 | `GeneWin_GridOpen` | rows 0x12 | 4,000 |
| O2 | | the words cleared at stride 1 | 4,000 |
| O3 | | `+0x4B` not written | 3,998 |
| O4 | | the last task's parameter 9 | 4,000 |
| O5 | | the record read before the last task | 800 |
| O6 | | the cells read once, before the calls | 3,076 |
| I1 | `GeneWin_GridSlideIn` | from the right at 0x42 too | 58 |
| I2 | | the record read before the draw | 814 |
| I3 | | from the left by 0x10 | 1,926 |
| U1 | `GeneWin_GridSlideOut` | freed above the right bound only | 106 |
| U2 | | the literal 0x142 (wide) | 158 |
| U3 | | freed below the left bound only | 297 |
| U4 | | the literal -0xBE (wide) | 385 |
| S01 | `MenuList_SlideRightTo182` | held at the bound too | 945 |
| SL1 | | (the shared `Slide`) `+3` left at the bound | 1,495 |
| S02 | `MenuList_SlideLeftTo92` | bound 0x5D | 2,496 |
| S03 | `MenuList_SlideRightTo92` | held at the bound too | 945 |
| S04 | `MenuList_SlideLeftOff300` | held below only | 945 |
| S04w | | the literal -300 (wide) | 3,620 |
| S05 | `MenuList_SlideRightTo17Ge` | held above only | 945 |
| S06 | `MenuList_SlideLeftTo182` | bound 0xB5 | 1,550 |
| S07 | `MenuList_SlideLeftTo200` | held at the bound too | 945 |
| S08 | `MenuList_SlideLeftOff100` | held at the bound too | 945 |
| S08w | | the literal -100 (wide) | 3,622 |
| S09 | `MenuList_SlideRightTo16` | by 0x10 | 3,541 of 8,000 (the filter also runs `...To165`) |
| S10 | `MenuList_SlideDownTo42` | x, not y | 4,000 |
| BP1 | `MenuList_SlideLeftBesidePanel` | beside + 0x77 | 484 |
| BP2 | | record 4's x unsigned | 840 |
| S11 | `MenuList_SlideUpTo62Le` | held below only | 945 |
| PR1 | `MenuList_SlideDownToPanelRow` | rows 0x35 apart | 2,371 |
| PR2 | | held above only | 945 |
| S12 | `MenuList_SlideLeftOff180` | held below only | 945 |
| S12w | | the literal -180 (wide) | 3,622 |
| S13 | `MenuList_SlideRightTo137Ge` | bound 0x8A | 2,450 |
| S14 | `MenuList_SlideLeftTo166` | held at the bound too | 945 |
| S15 | `MenuList_SlideRightTo45` | bound 0x2C | 2,458 |
| S16 | `MenuList_SlideLeftOff110` | held at the bound too | 945 |
| S16w | | the literal -110 (wide) | 3,622 |
| RC1 | `MenuList_SlideRightOffColumn` | the column 74 | 1,163 |
| RC2 | | `+0xB & 3` | 741 |
| LC1 | `MenuList_SlideLeftToColumn` | the columns from 1 | 1,551 |
| S17 | `MenuList_SlideLeftTo152` | bound 0x99 | 2,497 |
| S18 | `MenuList_SlideLeftTo165` | held at the bound too | 945 |
| S19 | `MenuList_SlideLeftTo17` | bound 0x10 | 1,544 |
| S20 | `MenuList_SlideRightTo165` | held at the bound too | 945 |
| S21 | `MenuList_SlideRightTo150` | bound 0x97 | 1,504 |
| S22 | `MenuList_SlideLeftTo150` | by 0x21 | 2,448 |
| K5a | `MenuList_StatsPanel` | the party slot from `+0xB` | 2,975 |
| K5b | | the record read before the state | 785 |
| K6 | `MenuList_EquipPanel` | x and y swapped | 3,999 |
| K7 | `MenuList_ExpPanel` | next 1 | 4,000 |
| K8 | `MenuList_NextLevelPanel` | the character id, not its record | 2,238 |
| T1 | `MenuList_WideTitleBox` | help 0x30, not 0x31 | 390 |
| T2 | | the record not re-read after the text pointer | 796 |
| T3 | | the box 0x117 wide | 4,000 |
| T4 | | the help text at y + 4 | 496 |
| CB1 | `MenuList_CursorBox` | the height from the width | 3,994 |
| CB2 | | the blink from `+0xC` | 3,521 |
| Q1 | `MenuList_IconWheel` | the kind without the angle above it | 1,376 |
| Q2 | | the angle by 0x20 | 3,409 |
| Q3 | | the record not re-read after the wheel | 857 |
| Q4 | | x without the angle above it | 684 |
| IL1 | `MenuList_ItemList` | the record read before the state | 785 |
| BR1 | `MenuList_ButtonRow` | x without the pointer above it | 4,000 |
| BR2 | | set and selection swapped | 3,598 |
| LL1 | `MenuList_LabelList` | record 0, not the current | 3,813 |
| D1 | `MenuList_DrawLabelList` | the box 0x18 over the lines | 4,000 |
| D2 | | the `+0xC` line raised in colour 0 | 1,547 |
| D3 | | `+0xA` not re-read after a line | 1,124 |
| D4 | | the second pieces at y + 0x1F | 3,786 |
| D5 | | one line more | 3,031 |
| D6 | | the closing pieces at + 0x10 | 4,000 |
| D7 | | the `+0xB` line raised in colour 2 | 1,195 |
| D8 | | the text index from the row's count byte | 3,779 |
| D9 | | x read once | 1,552 |
| EC1 | `MenuList_EquipCompare` | the record index without x above it | 4,000 |
| EC2 | | x without the set above it | 4,000 |
| EC3 | | the party slot `+0xB` | 2,967 |
| EC4 | | no_preview from `+0xC` | 3,617 |
| AP1 | `MenuList_AbilityPanel` | the record read before the state | 785 |
| IP1 | `MenuList_ItemPanel` | the state from the ability table | 3,195 |
| RP1 | `MenuList_ReservePanel` | the state from the equip table | 1,318 |

**Equivalent mutants, not planted**: inside a slide no call is made, so the
order of its record re-reads cannot be told apart (nothing can move the
pointer between them); the ExpToNext roster index masked `& 7` instead of
`& 0xFF` (the callee answers 0..7 by its reading: the byte at
`0x66972C + id`, 7 made 0); the title box's style byte read before or after
`0x905B84` (no call between).

## 6. What the fuzz did not reach

- The callees themselves: every one is a recorder. The panels' draws are
  their groups' (FO's `field_o`, `menu_windows`, BE7's grid, R2H's reserve
  list); the live check shows them.
- A state, kind or slot past its table, a task slot past the pool: ours aborts
  (section 7); never drawn.
- `MenuList_CursorBoxSizes` past four pairs and the label rows / texts past
  their tables: seeded at small indices and random bytes (read in place by
  both, so equal by construction).

## 7. Latent defects and ranges (Capcom's, described, not fixed)

- **The stack dispatchers are unbounded**: `BattleResultWin_LevelUpStates`
  (one entry: a `+3` of 1 calls its own return address, more the caller's
  stack), `Window_Handler5Kinds` by `+2` (five), `GeneWin_GridStates` by `+3`
  (three). Ours aborts with a message.
- **The twelve `.data` state tables are unbounded**: past a table the
  original runs the next kind's states (the code pointers continue to
  `0x66B08C` and `0x66B1D4`), then data. Ours aborts past the table's own
  count. Which values play writes to `+3` is the screens' code (wave two's
  other groups); not seen past a table.
- **`GeneWin_GridOpen` writes past the task pool** when `BattleTask_Create`
  answers 0xFF (no slot free): `+0xB` / `+0x4B` of `0x93A000 + 0xFF * 0x84`.
  Up to 19 tasks are made per open; ours aborts there, as BE7's three list
  opens do ([`battle_e7.md`](battle_e7.md) section 7). Not seen in play.
- **`GeneWin_GridSlideIn` steps past 0x42**: from the right, x > 0x42 moves
  0x20 even when that lands below 0x42 (and from the left above it); the next
  frame sets 0x42. A one-frame overshoot when the start is not 0x42 plus a
  multiple of 0x20. Reproduced.
- **`MenuList_CursorBox` reads `MenuList_CursorBoxSizes` by an unmasked
  byte**: past four pairs it reads the next table's code pointers as sizes.
  `MenuList_DrawLabelList` likewise reads its rows (`0x66B158 + 6 +0xA`) and
  text pointers (`0x66B12C` by a byte) unbounded; a pointer past the table
  is followed by `Text_DrawAt`. Read in place by ours (inside the image).
- **`BattleResultWin_ExpToNext` at level 99** would sum 100 rows, the last the
  next roster index's first; its one caller prints the level-99 message
  instead and does not call it. Reproduced; and the roster index is
  `0x4469D0`'s answer (0..7 by its reading), so the table is never left.
- **`BattleResultWin_ExpToNext` writes its caller's argument slot** (the
  roster index stored over the slot byte): harmless, cdecl's slot is the
  callee's.

**Nothing needs a ledger entry**: no read of never-written memory reaches a
draw or a decision (the garbage above the pushed bytes and words is never
read by the callees, section 3).

## 8. Calls across groups; the live coverage

**Outbound to groups of this round, raw** (`rest_2g_callees.h`):

| Address | Owner | Called by |
|---|---|---|
| `0x4469D0` | R3B (wave three; `CharId_ToRosterIndex` in `battle_result_callees.h`) | `BattleResultWin_ExpToNext` |
| `0x59AA80` | R2H (this wave) | `MenuList_ReservePanel` |

Everything else called is ours, by name.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls | How |
|---|---|---|---|
| `Window_Handler4Kinds` | `window_kinds` (ours) | `BattleResultWin_LevelUpStates` | `kResultKinds[0]`, rebound |
| `BattleResultWin_DrawExp` | `battle_result` (ours) | `BattleResultWin_ExpToNext` | `kExpToNext`, rebound |
| `Field_RunTaskRecords` | `window_task` (ours) | `Window_Handler5Kinds` | `kRecordHandlers[5]`, rebound |
| `MenuList_Run` | `menu_lists` (ours) | kinds 5..13, 15..19 | `MenuList_Kinds` `0x66AFA8..0x66AFE0` |
| `MenuList_MemberPanel`, `_MoneyBox`, `_TimeBox`, `_TopBarIcons` | `menu_lists` | ten slides | `MenuList_PanelStates`, `_MoneyStates`, `_TimeStates`, `_IconStates` |
| handlers 2, 7, 8 and kind 20's readers | R2H / ours | `MenuList_SlideLeftTo152`, `_LeftTo17`, `_RightTo150`, `_LeftTo150` | `0x66AEF0`, `0x66B1D4`, `0x66B2E0..0x66B2FC`, `0x66B390`, `0x66B588` |

No harness row (`scenario_harness*.cpp`, `boss_harness*.cpp`) lists any of
the 48. Other modules' fuzzes that key on these addresses
(`battle_result_fuzz`, `window_kinds_fuzz`, `window_task_fuzz`,
`menu_lists_fuzz`'s table swaps, `battle_e7_fuzz`) keep their values.

**Live coverage.** The catalogue's reach column marks the hidden starts
whose host was entered (an upper bound), not the starts. In
`analysis/calltrace` only `0x598810` is named by a traced run: the battle
routes' reach (`reach_balioAndSunder_2_1003`, `reach_bossAndFlash_1003`) and
round thirteen's `hash_r13_nue_*` - the result's EXP window. The field menu
(`menu_screens.txt`), the shop (`shop.txt`) and the camp
(`campingFishing.txt`) reach handler 6's kinds by `MenuList_Kinds`, but no
trace names a hidden start there. **Fuzz only here**; the coordinator's A/Bs
of those routes and a battle's result after the merge are the live check.

## 9. Self-tests

In this worktree's build (2026-10-04):

- `BOF3X_SHADOW=rest_2g`: exit 0, 0 mismatches (section 4); `inject: 9037
  ours` (8,989 + 48). The same with `BOF3X_WIDE=1`.
- `BOF3X_SHADOW='*'` and `'*'` with `BOF3X_WIDE=1`: see the addendum below.

## 10. The rebinding

`band_rows.py --refs --group R2G` and `grep -rn -i` of the 48 over
`src/game`: 20 raw references to 5 functions, three of them values.

| File | Change |
|---|---|
| `battle_result_callees.h` | `kExpToNext = bof3::addr::BattleResultWin_ExpToNext` (value unchanged) |
| `window_kinds_callees.h` | `kResultKinds[0] = bof3::addr::BattleResultWin_LevelUpStates` (value unchanged) |
| `window_task_callees.h` | `kRecordHandlers[5] = bof3::addr::Window_Handler5Kinds` (value unchanged; the header now includes `symbols.gen.h`) |

Left raw: the comments that cite the addresses (`battle_e7.cpp`,
`battle_result.cpp`, `window_kinds.cpp`, `battle_e7.h`, `battle_e7_fuzz.cpp`,
`battle_result_fuzz.cpp`; `window_kinds_callees.h`' note above
`kResultKinds` still says `0x597FA0` "is in no group") - prose, not
references; none in a file another group of this round writes.

## 11. For `analysis/calltrace/entries_logic.txt`

46 lines appended to the main checkout's file (`# round 14 group R2G`). Two
starts already have a line carrying a host's longer extent, left for the
coordinator's split: `00598810 21D` (covers `0x598810..0x598A2C`: ours
`0x598810` 0x79 and the five hidden starts `0x598890..0x5989F0`) and
`0059A6C0 3B8` (covers `0x59A6C0..0x59AA77`: ours `0x59A6C0` 0x21D and the
nine hidden starts `0x59A8E0..0x59AA50`).
