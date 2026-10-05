# Group R2F: the field menu's Tactics and Config steps, window handlers 1 and 2

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, from the
round branch's tip `fa583bc`. **49 functions ours** (`src/game/rest_2f.cpp`,
`rest_2f.h`, `rest_2f_callees.h`, shadow name `rest_2f`): the cut's 48 rows for
R2F (`analysis/round14_cut.tsv`) and the one start in their spans no list had
(`0x596530`, band_rows' "code no list has": `Field_RunTaskRecords`' handler 1).
Each read to its last instruction with capstone and fuzzed through the scenario
harness's **field** mode (used unchanged): 294,000 rounds, **0 mismatches**;
139 of 144 controls refused, the other five equivalent mutants with a near variant refused (section 5); `BOF3X_SHADOW='*'` exit 0, narrow and with `BOF3X_WIDE=1`. Fifteen `.data` tables named. Fuzz only: no recorded
trace enters any of the 49 (section 9).

**What the band is** (by the code; the screens' names are
[`menu-screens.md`](menu-screens.md)'s, from the owner's walk of the menu; no
gameplay fact is stated from memory):

- **The Tactics screen** (`FieldMenu_States[5]`): `Tactics_Run` and its steps
  (`TacticsMenu_Steps`), the formation grid (`TacticsFormation_*`) and the
  party / reserve exchange (`TacticsMembers_*`).
- **The Config screen's three steps** (`FieldMenu_States[7]`, `ConfigMenu_*`:
  the open, a tail jump to R4F's `0x460CB0` machine, the close) and **state 8**
  (`FieldMenu_State8Run`: four bare rets).
- **The Ability screen's helpers** that R2E's states call by `E8`: two window
  set-ups and the ability list's compaction and two sorts by AP cost.
- **Window-record handlers 1 and 2** of `Field_RunTaskRecords` (their kind
  tables, five kinds each, three slide steps, the "SHISU" panel and a nine-row
  item list) and **window kind 1's** three helpers (`Window_Kind1Open` /
  `Window_Kind1Frame` call them: the choice list's layout, draw and hand).
- **Three helpers other modules called by address**: `Stat_AddClampedTo`
  (`battle_e5`), `AbilityList_CountSet` (`field_s`), `ItemTrade_TakeNeeds`
  (`field_e2`).

## 1. The functions

Extents from `tools/band_rows.py --byte-tables` / `--clones` (2026-10-04),
checked against the capstone read; the cut's sizes differ by padding except
where said.

| PC | Name | Bytes | Reached from | What it does |
|---|---|--:|---|---|
| `0x58ED40` | `AbilityMenu_InitRecords` | 0xFC | E8 from `0x58D7D0` (R2E) | window records 11, 12, 16 set up and opened, 19 and 21 closed |
| `0x58EE40` | `AbilityList_SortBy` | 0x10 | E8 from `0x58E640` (R2E) | tail jump through `AbilityList_SortModes` by its argument byte |
| `0x58EE50` | `AbilityList_Compact` | 0x70 | `AbilityList_SortModes[0]`; E8 from the two sorts | nine bubble passes moving the empty ability slots to the end (`0x58BD50` swaps) |
| `0x58EEC0` | `AbilityList_SortByApDown` | 0x9A | `AbilityList_SortModes[1]` | compact, then by the AP cost byte (`Ability_Records + 0x12`), highest first |
| `0x58EF60` | `AbilityList_SortByApUp` | 0x9A | `AbilityList_SortModes[2]` | the same, lowest first |
| `0x58F000` | `AbilityMenu_InitRecord17` | 0x45 | E8 from `0x58EA70` (R2E) | record 17 (kind 0x14) opened at x 0x140 |
| `0x58F050` | `FieldMenu_FreeRecords13To18` | 0x21 | E8 from `0x58E9D0` (R2E) | records 13..18 freed |
| `0x58F080` | `Tactics_Run` | 0xE | `FieldMenu_States[5]` | through `TacticsMenu_Steps` by `0x929F01` |
| `0x58F090` | `Tactics_Open` | 0xDC | `TacticsMenu_Steps[0]` | records 11, 18..21 set up; backdrop; sound 0x102; step on |
| `0x58F170` | `Tactics_Top` | 0x177 | `TacticsMenu_Steps[1]` | the two choices (formation, members), the hand, confirm / cancel |
| `0x58F2F0` | `Tactics_Formation` | 0xE | `TacticsMenu_Steps[2]` | through `TacticsFormation_Steps` by `0x929F02` |
| `0x58F300` | `TacticsFormation_Enter` | 0x69 | `TacticsFormation_Steps[0]` (and `TacticsMenu_Steps[6]`) | the countdown; the party records' +0xB = 1 |
| `0x58F370` | `TacticsFormation_Pick` | 0x3EF | `TacticsFormation_Steps[1]` | the formation grid's cursor, pick, set and party swap |
| `0x58F760` | `TacticsFormation_Leave` | 0x69 | `TacticsFormation_Steps[2]` | the countdown; +0xB = 0; back to the top |
| `0x58F7D0` | `Tactics_Members` | 0xE | `TacticsMenu_Steps[3]` | through `TacticsMembers_Steps` by `0x929F02` |
| `0x58F7E0` | `TacticsMembers_Enter` | 0xA7 | `TacticsMembers_Steps[0]` | the countdown; +0xC = 1 for members whose record has +0xB bit 1 |
| `0x58F890` | `TacticsMembers_Pick` | 0x2D0 | `TacticsMembers_Steps[1]` | the two-column cursor, pick and exchange |
| `0x58FB60` | `TacticsMembers_Leave` | 0x69 | `TacticsMembers_Steps[2]` | the countdown; +0xC = 0; back to the top |
| `0x58FBD0` | `Tactics_Close` | 0x8D | `TacticsMenu_Steps[4]` | the countdown; records freed; the menu to its top bar |
| `0x58FC60` | `Tactics_OpenFormation` | 0xF1 | `TacticsMenu_Steps[5]` | Tactics entered at the formation (the party records placed, the grid built) |
| `0x58FD60` | `TacticsFormation_Build` | 0x185 | E8 from `Tactics_Top`, `Tactics_OpenFormation` | the 3 x 3 grid by the party count and `0x904061`'s bits; records 12.. for its cells |
| `0x58FEF0` | `TacticsMembers_Build` | 0x12F | E8 from `Tactics_Top` | the reserve list `0x6BDFD4` from `CharacterRecords`; record 12 |
| `0x590020` | `TacticsMembers_Swap` | 0x273 | E8 from `TacticsMembers_Pick` | the exchange of two cells; al 0xFF or the member that refuses |
| `0x5902A0` | `FieldMenu_FreeRecords12To18` | 0x26 | E8 from `Tactics_Close` | records 12..18 freed |
| `0x5902D0` | `FieldMenu_State8Run` | 0xE | `FieldMenu_States[8]` | through `FieldMenu_State8Steps` (four bare rets) |
| `0x5902E0` | `ConfigMenu_Run` | 0xE | `FieldMenu_States[7]` | through `ConfigMenu_Steps` by `0x929F01` |
| `0x5902F0` | `ConfigMenu_Open` | 0x41 | `ConfigMenu_Steps[0]` | step on; the top bar's cursor kept at `0x6BDFD8` |
| `0x590340` | `ConfigMenu_Body` | 5 | `ConfigMenu_Steps[1]` | `jmp 0x460CB0` (R4F's Config machine) |
| `0x590350` | `ConfigMenu_Close` | 0x95 | `ConfigMenu_Steps[2]` | the party records out; the menu to its top bar, the cursor restored |
| `0x590E80` | `Stat_AddClampedTo` | 0x54 | E8 from `Effect_DrainHp`, `Effect_DrainAp` | a u16 stat plus a s16 delta clamped to [0, cap]; answers the change |
| `0x591AC0` | `AbilityList_CountSet` | 0xA0 | E8 from `SharedList_DrawList` | the set bytes of one of a member's four ability lists; al |
| `0x594D90` | `ItemTrade_TakeNeeds` | 0x65 | E8 from `ItemTrade_Confirm` | up to three ingredients removed (`Inventory_Remove`) |
| `0x596090` | `Window_Kind1List` | 0x34 | E8 from `Window_Kind1Frame` | the choices through `Text_DrawImmediate` |
| `0x596120` | `Window_Kind1Cursor` | 0x2D | `Window_Kind1Frame`'s tail jump | the hand at the cursor's choice |
| `0x596330` | `Window_Kind1Layout` | 0x1F2 | E8 from `Window_Kind1Open` | the choices' lines counted; the window's size and the rows' y |
| `0x596530` | `Window_Handler1Kinds` | 0x12 | `Field_RunTaskRecords` handler 1 (imm32 at `0x59E245`) | through `Window_Handler1KindTable` by the record's +2 |
| `0x596550` | `Win1_TitleStrip` | 0x7C | `Window_Handler1KindTable[1]` | its state; a 0x118-wide title box; a `MessagePools` string |
| `0x5965D0` | `Win1_ButtonRow` | 0x34 | `Window_Handler1KindTable[2]` | its state; `Menu_DrawButtonRow` |
| `0x596610` | `Win1_ShisuPanel` | 0x20 | `Window_Handler1KindTable[3]` | its state; `Win1_DrawShisuPanel` |
| `0x596630` | `Win1_DrawShisuPanel` | 0x2AA | E8 from `Win1_ShisuPanel` | a box, four item rows, the title "SHISU", the frame |
| `0x5968E0` | `Window_Handler2Kinds` | 0x12 | `Field_RunTaskRecords` handler 2 (imm32 at `0x59E250`) | through `Window_Handler2KindTable` by +2 |
| `0x596900` | `Win2_ItemList` | 0x20 | `Window_Handler2KindTable[0]` | its state; `Win2_DrawItemList` |
| `0x596920` | `MenuSlide_LeftOff170` | 0x28 | four state tables | x - 0x20 a frame to the operand's -170 (DIV-0041 widens it) |
| `0x596950` | `MenuSlide_RightTo80` | 0x28 | `Win2_ItemListStates[2]` | x + 0x20 a frame to 0x50 |
| `0x596980` | `Win2_ItemPanel` | 0x20 | `Window_Handler2KindTable[2]` | its state; `Menu_DrawItemPanel` |
| `0x5969A0` | `Win2_EquipCompare` | 0x35 | `Window_Handler2KindTable[3]` | its state; `Menu_DrawEquipCompare(7, ...)` |
| `0x5969E0` | `Win2_TitleBox` | 0x74 | `Window_Handler2KindTable[4]` | its state; a title box; a system message |
| `0x596A60` | `MenuSlide_DownTo40` | 0x28 | `Win2_TitleBoxStates[3]` | y + 0x10 a frame to 0x28 |
| `0x596A90` | `Win2_DrawItemList` | 0x508 | E8 from `Win2_ItemList` | `Menu_DrawItemList`'s shape with nine rows; the title call DIV-0059 re-aims |

**Extents against the cut.** `0x591AC0`: the cut's 96 bytes stopped short -
the code runs to `0x591B4D` and its 4-entry jump table sits at `0x591B50`
(0xA0 in all). `0x596330`: the cut's 544 ran over `0x596530`, a function of its
own (the start the cut lacked); 0x1F2 including its jump table `0x596508` and
its ten-byte case table `0x596518`. No start was a jump-table case or a shared
tail; nothing was dropped or merged. The catalog's hosts in the cut's `host`
column are all R2F's own rows (contiguous functions, each reached on its own).

**The state tables named** (`[[data]]`, each count its reader's reach, read by
hand; every reader is unchecked and ours aborts past the count):

| Table | Count | Reader, by | Entries |
|---|--:|---|---|
| `AbilityList_SortModes` `0x667444` | 3 | `AbilityList_SortBy`, its argument byte | Compact, SortByApDown, SortByApUp (the formation's cursor places `0x667450` follow) |
| `TacticsMenu_Steps` `0x667498` | 7 | `Tactics_Run`, `0x929F01` | Open, Top, Formation, Members, Close, OpenFormation; 6 = `TacticsFormation_Enter` |
| `TacticsFormation_Steps` `0x6674B0` | 3 | `Tactics_Formation`, `0x929F02` | Enter, Pick, Leave |
| `TacticsMembers_Steps` `0x6674BC` | 3 | `Tactics_Members`, `0x929F02` | Enter, Pick, Leave (the members screen's places `0x6674C8` follow) |
| `FieldMenu_State8Steps` `0x6674F8` | 4 | `FieldMenu_State8Run`, `0x929F01` | four `BareRet` |
| `ConfigMenu_Steps` `0x66750C` | 3 | `ConfigMenu_Run`, `0x929F01` | Open, Body, Close |
| `Window_Handler1KindTable` `0x66AE40` | 4 | `Window_Handler1Kinds`, record +2 | `MenuWin_Hand`, TitleStrip, ButtonRow, ShisuPanel |
| `Win1_TitleStripStates` `0x66AE50` | 3 | `Win1_TitleStrip`, +3 | `BareRet`, `MenuSlide_UpOff`, `MenuSlide_DownTo16` |
| `Win1_ButtonRowStates` `0x66AE5C` | 3 | `Win1_ButtonRow`, +3 | `BareRet`, `MenuSlide_UpOff`, `MenuSlide_DownTo38` |
| `Win1_ShisuPanelStates` `0x66AE68` | 3 | `Win1_ShisuPanel`, +3 | `BareRet`, `MenuSlide_LeftOff`, `MenuSlide_RightTo17` |
| `Window_Handler2KindTable` `0x66AEC8` | 5 | `Window_Handler2Kinds`, +2 | ItemList, `MenuWin_Hand`, ItemPanel, EquipCompare, TitleBox |
| `Win2_ItemListStates` `0x66AEDC` | 3 | `Win2_ItemList`, +3 | `BareRet`, LeftOff170, RightTo80 |
| `Win2_ItemPanelStates` `0x66AEE8` | 3 | `Win2_ItemPanel`, +3 | `BareRet`, `MenuSlide_RightOff`, `0x59A610` (R2G's) |
| `Win2_EquipCompareStates` `0x66AEF4` | 3 | `Win2_EquipCompare`, +3 | `BareRet`, LeftOff170, `MenuSlide_RightTo17` |
| `Win2_TitleBoxStates` `0x66AF00` | 4 | `Win2_TitleBox`, +3 | `BareRet`, `MenuSlide_UpOff`, `MenuSlide_DownTo16`, DownTo40 |

`TacticsMenu_Steps` is laid out over the two sub-step tables: its entries
6..11 are `TacticsFormation_Steps` and `TacticsMembers_Steps`, the run of code
pointers going on to `0x6674C8`. Its count, 7, is the reach of the screen's
own writes of `0x929F01` (0..5 by its steps, and 6 by `TacticsFormation_Pick`
on its way out of a directly entered Tactics, the same frame it bumps
`Game_Step`). The fuzz swaps it as its first six entries, the seventh being
`TacticsFormation_Steps[0]`'s cell (two `DataTable`s may not overlap).

## 2. What they do

Each function's comment in `rest_2f.cpp` is the full read; the points that
matter here.

**The Tactics screen.** The steps keep the menu block's `0x929F01` (step),
`0x929F02` (sub-step) and `0x929F04` (a five-frame countdown each Enter /
Leave / Close waits on). Its own cells sit at `0x6BDFBC..0x6BDFD8`: the 3 x 3
grid of open cells, the formation set (column 1..2, row: `0x904060` = col +
2 row - 1), the cell picked (`0x7F` none), the grid cursor, the top's choice,
the party / reserve flags `0x6BDFCC`.., the two rows the last swap moved, the
reserve list `0x6BDFD4`.., the refused member `0x6BDFD7`, and Config's saved
top-bar cursor `0x6BDFD8`. The formation grid's column 0 is the party's rows;
columns 1..2 are six cells opened by `0x904061`'s bits up to a limit set by
the party count (1, 3, 6), each with a window record 12 + n. The members
screen is two columns (party 0, reserve 1) through `0x929F08..0x929F0D`.
`TacticsMembers_Swap` exchanges two party rows (their records' +0xC, the party
bytes `0x904062`.., and the "portrait" cells `0x9398C0 + 4 row`, `0x939880` /
`0x9398A0 + 6 row`, byte by byte), two reserve bytes, or a party and a
reserve member - refused (the answer) when either's `CharacterRecords` +0xB
has bit 1, else both party lists (`0x904062`.. and `0x904065`..) and the
reserve rewritten and the party row's portrait cells cleared.

**The Ability helpers.** The two sorts and the compaction are nine-pass
bubble sorts over the ten-byte list `Char_AbilityList(record 16 +0xA, +0xB, 0)`
returns, fetched afresh each pass; every swap is R2E's `0x58BD50`.

**Window kind 1.** `Window_Kind1Layout` walks the choices' text from
`0x7DEE50` (0 ends a choice and counts a line, 1 counts a line, codes 4 5 7 8 9
take a byte more, 2 3 6 none, a byte from 0x80 a byte more - the classes of its
own ten-byte case table) and lays out the current record: 0xDC wide, (last + 4)
2 + 13 lines high, x and y in sixteenths, the rows' y bytes `0x7DEE6F + n`.

**Handlers 1 and 2.** Each kind calls its state (a slide or a bare ret)
through its own table by +3, then draws; the slides step x or y and hold at a
bound with +3 = 0. `Win2_DrawItemList` is the field menu's item list's shape
(`Menu_DrawItemList`, [`menu-windows.md`](menu-windows.md)) with nine rows and
`Item_CanUse` by the member `0x904062[s8 0x929F06]`; `Win1_DrawShisuPanel`
draws four item rows whose ids are `0x66AE8C`'s and whose counts are the
record's +0x20 list, under the image string "SHISU".

**The three helpers.** `Stat_AddClampedTo` is `Stat_AddClamped` with the cap
an argument (callers read the answer's low word as a s16 amount; section 7).
`AbilityList_CountSet` is `Char_AbilityList`'s counting twin.
`ItemTrade_TakeNeeds` removes a trade's ingredients from category 0 (item byte
+ 0x38, count the s8 product of the count byte and `0x6BE08E`).

**Faithful in the details** (each named in the comment on ours): reads after
calls where the originals read them (the step after the sounds, the cursor
after `Input_AutoRepeat`, the category and item after the shadow row, the
current record after each state call); the upper halves the originals push
(`Window_Kind1List`'s y carries the text pointer's upper half,
`Window_Kind1Cursor`'s x the cursor's sign); the y of `Win2_DrawItemList`'s
rows from the scroll offset as a s8; the stack-slot reuse of
`Win2_DrawItemList` (the scroll offset in the record argument's own low byte)
and `Win1_DrawShisuPanel` (the dim flag in its argument's slot) kept as locals,
unobservable.

## 3. The patches inside these bodies

- **DIV-0059** re-aims `Win2_DrawItemList`'s title call `0x596D13` at
  `ListTitle_DrawAt` under a Latin overlay (`battle_draw.cpp`, `kTitleCallA`,
  `BOF3X_ORIGINAL=BattleListTitleCentre`). `Rest2F_Inject` reads the site's
  target after the fuzz and ours calls it (`g_title_call`), so the divergence
  survives the takeover under its existing name; the log line `rest_2f
  Win2_DrawItemList's title call (0x596D13) reaches ...` says which. Under an
  overlay the fuzz cannot copy the function (the site no longer reaches
  `Text_DrawAt`), so it leaves `Win2_DrawItemList` out and logs it; run the
  self-test without `BOF3X_LANG`.
- **DIV-0041** widens `MenuSlide_LeftOff170`'s bound, the imm32 of `mov ecx,
  -170` at `0x596926` (`widescreen.cpp` kSlides). Ours reads it from there, as
  `menu_lists.cpp` reads `MenuSlide_LeftOff`'s; `Rest2F_Inject` refuses an
  operand that is not `B9` then -170 or the widened bound.
- Nothing else: `DIVERGENCE.md`, `cheats.cpp`, `labels.cpp`, `widescreen.cpp`
  and every `src/game` file grepped for the 49 addresses and the band's call
  sites. No full-frame fill in the band.

## 4. The fuzz (`BOF3X_SHADOW=rest_2f`, `rest_2f_fuzz.cpp`)

The clone table is `band_rows.py --group R2F --clones --harness scenario`.
The field menu's states are `kMenu` (`menu_span` 3; each dispatcher's index
drawn below its own table by the seed), the window handlers, kinds, slides,
kind-1 helpers and `ItemTrade_TakeNeeds` `kState` with `0x905B84` one of the
22 window records (its +2 / +3 inside the function's tables), and the five
that take arguments `kCall` (`AbilityList_SortBy` a mode below 3,
`Stat_AddClampedTo` a scratch word, `AbilityList_CountSet` member / list /
current at their cases, the two draws a window record). Answers compared:
`TacticsMembers_Swap` and `AbilityList_CountSet` in al, `Stat_AddClampedTo`'s
low word. 6,000 rounds a function.

**Regions** beyond field mode's: `WindowRecords` (22 x 0x24), the message
cells `0x7DEE20` + 0x60, Tactics' cells `0x6BDFB0` + 0x30, `CharacterRecords`
past the style region to `Cond_Flags`, the inventory lists `0x904160` + 0x400,
the portrait cells `0x939870` + 0x70, the confirm / cancel words `0x903584`
+ 0x10, `Game_Mode` / `Game_Step`, the trade's cells `0x6BE080` + 0x10, and a
0x100-byte text buffer of the fuzz's own for kind 1's choices (zeros behind
it). **Seeds:** every record's bytes at their compares (x at the slides'
bounds and one either side, y likewise, the category 0..4, the top, cursor and
mark around each other, the +0x10 word's bits and countdown); the grid with
the cursor's cell open (the grid loops end on it), the pick 0x7F or a cell,
the set column 0..3; the members cursor and the held cell; the party ids and
the characters' +9 / +0xB flags; the buttons (confirm 0x20, cancel 0x40,
pressed among them and the four directions); the choice text of control
codes, glyphs and ends with the last index -1..6; `Stat_AddClampedTo`'s word
at the cap, 0, one either side and cap - delta.

**Stand-ins** beyond the field-standard set: the group's own seven called by
`E8` (`TacticsMembers_Swap` answering 0xFF..0x07, `TacticsFormation_Build`
resetting the cursor and pick its callers read after it); `Text_DrawImmediate`
(answers a few bytes on), `Menu_DrawButtonRow`, `Menu_DrawItemPanel`,
`Menu_DrawEquipCompare`, `Inventory_CountUsed` with the widths each reads;
`Crt_sprintf` re-listed with four words (the standard row does not log the
room) and the standard row's write of the print buffer; R4F's `0x460CB0`.
**Disturbance** (from the hash only): the step, sub-step, top choice and
countdown; the current record's x / y, category (a 4 stays a 4), top, +0x10
word and +9; `0x905B84` repointed to another record; the kind-1 last index;
a party byte; record 12's +0xA; the members rows.

**Result** (this worktree's build, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_2f`, exit 0):

    shadow      rest_2f self-test: 294000 rounds over 49 functions (6000 each), 1843398 calls to the stand-ins, 0 MISMATCHES; 27236 bytes of state (48 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'` (this worktree, at the final commit): exit 0, 1,026 lines of
`0 MISMATCHES` and none other; the same with `BOF3X_WIDE=1` (DIV-0041's bound
inside `MenuSlide_LeftOff170` widened, read back by ours). Neither run died
silently.

Every table entry, stand-in and own callee was reached (the coverage lines:
e.g. `TacticsMembers_Swap` 1,034 times as a callee, `0x58BD50` 322,462,
`Menu_DrawPiece` 726,000; the table handlers 818..19,379 each).

**What the fuzz found before any commit:** two of its own seeds - record 12's
+0xA is both the reserve's count and, for the item list, a category; and a
category turned from 4 to another under a call makes both sides read the null
count list (section 7). Both bound in the seed / disturbance.

## 5. Controls

144 planted bugs, one at a time, each through a scratch driver that plants it in `rest_2f.cpp` (anchored on a unique string), rebuilds, runs `BOF3X_SHADOW=rest_2f` with `BOF3X_R2F_ONLY` on the function, restores and rebuilds (`r2f/controls.py`, `control_list.py` in the session scratchpad). **139 refused by a count of mismatches, in the function planted; 5 not refused, each an equivalent mutant** (no input can tell it apart; the reason is in its row) **with a near variant refused.** The first run left three more unrefused (C34, C124, C137: the seeds had no record of kind 0xB, no x of 0x31, no change of the list's category under the shadow row) and C144 weak; the seeds and the disturbance were widened and every control re-run - the table is that last run, on the committed fuzz.

| # | Function | Planted | Refused in (rounds) |
|---|---|---|--:|
| 1 | `AbilityMenu_InitRecords` | record 16 x 0x141 | 6000 of 6000 |
| 2 | `AbilityMenu_InitRecords` | record 12 +0xB 0xFE | 6000 of 6000 |
| 3 | `AbilityList_SortBy` | mode 1 runs mode 2 | 1966 of 18000 |
| 4 | `AbilityList_Compact` | compaction swaps on a second byte above 1 | 7 of 6000 |
| 5 | `AbilityList_Compact` | the span not shortened after the first pass | 1119 of 6000 |
| 6 | `AbilityList_Compact` | Char_AbilityList member and type swapped | 5312 of 6000 |
| 7 | `SortByApDown` | down sort swaps on equal costs | 6000 of 6000 |
| 8 | `SortByApUp` | up sort swaps on equal costs | 5778 of 6000 |
| 9 | `AbilityMenu_InitRecord17` | record 17 y 0x3F | 6000 of 6000 |
| 10 | `FieldMenu_FreeRecords13To18` | record 18 kept | 4465 of 6000 |
| 11 | `Tactics_Run` | step 1 runs step 2 | 788 of 6000 |
| 12 | `Tactics_Open` | record 18 x 0x71 | 5998 of 12000 |
| 13 | `Tactics_Open` | the top choice 1 | 5980 of 12000 |
| 14 | `Tactics_Open` | the step read before the sound | 49 of 12000 |
| 15 | `Tactics_Top` | record 11 +0x10 0x20 + choice | 5996 of 6000 |
| 16 | `Tactics_Top` | the hand 0x31 a choice | 603 of 6000 |
| 17 | `Tactics_Top` | the toggle on 0x8000 only | 1516 of 6000 |
| 18 | `Tactics_Top` | the members allowed by bit 1 | 612 of 6000 |
| 19 | `Tactics_Top` | confirm steps on by choice + 2 | 1422 of 6000 |
| 20 | `Tactics_Top` | cancel steps on by 4 | 1006 of 6000 |
| 21 | `Tactics_Top` | record 18 +0xB 0xFE | 4578 of 6000 |
| 22 | `Tactics_Formation` | sub-step 1 runs 2 | 2019 of 6000 |
| 23 | `TacticsFormation_Enter` | the records +0xB = 2 | 1533 of 6000 |
| 24 | `TacticsFormation_Enter` | the party loop one further | 606 of 6000 |
| 25 | `TacticsFormation_Leave` | the countdown ends at 1 | 1004 of 6000 |
| 26 | `TacticsFormation_Pick` | the pick mark +2 | 3008 of 6000 |
| 27 | `TacticsFormation_Pick` | the set mark 6 off column 0 | 4039 of 6000 |
| 28 | `TacticsFormation_Pick` | record 11 +0x10 0x77 + cell | 4036 of 6000 |
| 29 | `TacticsFormation_Pick` | the swapped note takes the row twice | 869 of 6000 |
| 30 | `TacticsFormation_Pick` | the rows noted when one apart | 157 of 6000 |
| 31 | `TacticsFormation_Pick` | the swap arguments exchanged | 203 of 6000 |
| 32 | `TacticsFormation_Pick` | direct entry on 2 | 120 of 6000 |
| 33 | `TacticsFormation_Pick` | the formation written - 2 | **not refused** - equivalent: the formation byte is written again from the set at the function's end (near variant C39 refused) |
| 34 | `TacticsFormation_Pick` | records of kind 0xC sent out | 270 of 6000 |
| 35 | `TacticsFormation_Pick` | up on 0x2000 | 1415 of 6000 |
| 36 | `TacticsFormation_Pick` | the sound only on a column move | 1438 of 6000 |
| 37 | `TacticsFormation_Pick` | the columns move with a pick in column 1 | 1489 of 6000 |
| 38 | `TacticsFormation_Pick` | left on 0x4000 | 1163 of 6000 |
| 39 | `TacticsFormation_Pick` | the formation at the end without - 1 | 6000 of 6000 |
| 40 | `TacticsFormation_Leave` | the step back by 2 | 2001 of 6000 |
| 41 | `Tactics_Members` | sub-step 0 runs 1 | 1982 of 6000 |
| 42 | `TacticsMembers_Enter` | the member flag bit 0 | 857 of 6000 |
| 43 | `TacticsMembers_Enter` | the party byte at 2 rec[+0xB] | 534 of 6000 |
| 44 | `TacticsMembers_Pick` | the cursor mark from +5 | 5993 of 6000 |
| 45 | `TacticsMembers_Pick` | record 11 +0x10 0x62 | 2031 of 6000 |
| 46 | `TacticsMembers_Pick` | the name 9 bytes | 3968 of 6000 |
| 47 | `TacticsMembers_Pick` | the column on right only | 1216 of 6000 |
| 48 | `TacticsMembers_Pick` | the party row held one lower | **not refused** - equivalent: at the boundary the clamp writes the row it already holds (near variant C49 refused) |
| 49 | `TacticsMembers_Pick` | up wraps to count - 2 | 618 of 6000 |
| 50 | `TacticsMembers_Pick` | down wraps at the last row | 120 of 6000 |
| 51 | `TacticsMembers_Pick` | the sound only on a column change | 1738 of 6000 |
| 52 | `TacticsMembers_Pick` | the done sound 0x104 | 110 of 6000 |
| 53 | `TacticsMembers_Pick` | record 12 to state 2 | 501 of 6000 |
| 54 | `TacticsMembers_Leave` | the step back by 1 | 2001 of 6000 |
| 55 | `Tactics_Close` | the field menu state 2 | 2001 of 6000 |
| 56 | `Tactics_Close` | the party records to state 3 | 1486 of 6000 |
| 57 | `Tactics_OpenFormation` | the party rows 0x37 apart | 2260 of 6000 |
| 58 | `Tactics_OpenFormation` | record 11 y 0x11 | 5999 of 6000 |
| 59 | `TacticsFormation_Build` | the cells at x 0xF6 | 4304 of 6000 |
| 60 | `TacticsFormation_Build` | the limit inclusive | 1508 of 6000 |
| 61 | `TacticsFormation_Build` | two members limit 2 | 762 of 6000 |
| 62 | `TacticsFormation_Build` | the set column + 2 | 6000 of 6000 |
| 63 | `TacticsFormation_Build` | column 0 open to the count | 4509 of 6000 |
| 64 | `TacticsMembers_Build` | the reserve by bit 1 | 5951 of 6000 |
| 65 | `TacticsMembers_Build` | one reserve flag fewer | 4990 of 6000 |
| 66 | `TacticsMembers_Build` | record 12 y 0x3C | 6000 of 6000 |
| 67 | `TacticsMembers_Build` | the party flags cleared only below 2 | 1504 of 6000 |
| 68 | `TacticsMembers_Build` | the list written again after the flags (the overlap undone) | 1359 of 6000 |
| 69 | `TacticsMembers_Swap` | the +0xC bytes not exchanged | 818 of 6000 |
| 70 | `TacticsMembers_Swap` | the party swap arguments exchanged | 884 of 6000 |
| 71 | `TacticsMembers_Swap` | three portrait bytes | 883 of 6000 |
| 72 | `TacticsMembers_Swap` | five of the six-byte cells | 884 of 6000 |
| 73 | `TacticsMembers_Swap` | refusal by bit 2 | 1254 of 6000 |
| 74 | `TacticsMembers_Swap` | the reserve refusal answers the party member | 616 of 6000 |
| 75 | `TacticsMembers_Swap` | the copy not rewritten | 300 of 6000 |
| 76 | `TacticsMembers_Swap` | the reserve not rewritten | 406 of 6000 |
| 77 | `TacticsMembers_Swap` | the last portrait word at +3 | 1350 of 6000 |
| 78 | `FieldMenu_FreeRecords12` | record 12 kept | 4443 of 6000 |
| 79 | `FieldMenu_State8Run` | state 8 through the Config table (near variant of an equivalent mutant) | 6000 of 6000 |
| 80 | `FieldMenu_State8Run` | state 8 step 0 runs step 1 (all four are BareRet) | **not refused** - equivalent: the four entries are one bare ret (near variant C79 refused) |
| 81 | `ConfigMenu_Run` | step 0 runs 1 | 1982 of 6000 |
| 82 | `ConfigMenu_Open` | 0x929F03 = 3 | 6000 of 6000 |
| 83 | `ConfigMenu_Open` | the cursor not kept | 5972 of 6000 |
| 84 | `ConfigMenu_Body` | a sound for the machine | 6000 of 6000 |
| 85 | `ConfigMenu_Close` | the party x 0xFF6B | 4550 of 6000 |
| 86 | `ConfigMenu_Close` | the cursor not restored | 5228 of 6000 |
| 87 | `Stat_AddClampedTo` | the cap exclusive | **not refused** - equivalent: at sum = cap both give the stat cap and an answer whose low word is the delta (C89, C90 refused) |
| 88 | `Stat_AddClampedTo` | a fall to 0 clamped | **not refused** - equivalent: at sum = 0 both give 0 and an answer whose low word is the delta (C89, C90 refused) |
| 89 | `Stat_AddClampedTo` | at the cap the delta answered | 464 of 6000 |
| 90 | `Stat_AddClampedTo` | the fall answer one less | 1146 of 6000 |
| 91 | `AbilityList_CountSet` | list 2 at +0x75 | 8 of 6000 |
| 92 | `AbilityList_CountSet` | the other list 0x7F bytes | 2103 of 6000 |
| 93 | `AbilityList_CountSet` | the working record one on | 851 of 6000 |
| 94 | `ItemTrade_TakeNeeds` | the item + 0x39 | 5868 of 6000 |
| 95 | `ItemTrade_TakeNeeds` | the second kind by five bits | 1696 of 6000 |
| 96 | `ItemTrade_TakeNeeds` | the end mark 0xFE | 2723 of 6000 |
| 97 | `ItemTrade_TakeNeeds` | the multiplier unsigned | 2410 of 6000 |
| 98 | `Window_Kind1List` | the next choice's y | 4641 of 6000 |
| 99 | `Window_Kind1List` | the last choice not drawn | 3954 of 6000 |
| 100 | `Window_Kind1Cursor` | the hand + 5 | 6000 of 6000 |
| 101 | `Window_Kind1Layout` | a newline counts no line | 3524 of 6000 |
| 102 | `Window_Kind1Layout` | two-byte glyphs from 0x40 | 2669 of 6000 |
| 103 | `Window_Kind1Layout` | the width 0xDD | 6000 of 6000 |
| 104 | `Window_Kind1Layout` | the +0x1A half by shift | 682 of 6000 |
| 105 | `Window_Kind1Layout` | the rows 3 apart | 3954 of 6000 |
| 106 | `Window_Kind1Layout` | the +0x14 a quarter | 6000 of 6000 |
| 107 | `Window_Kind1Layout` | the height + 5 | 6000 of 6000 |
| 108 | `Window_Handler1Kinds` | kind 1 runs kind 2 | 1537 of 6000 |
| 109 | `Win1_TitleStrip` | the title box 0x117 wide | 6000 of 6000 |
| 110 | `Win1_TitleStrip` | the pool index unsigned | 5 of 6000 |
| 111 | `Win1_TitleStrip` | the record not re-read after the box | 28 of 6000 |
| 112 | `Win1_ButtonRow` | set and selection exchanged | 5268 of 6000 |
| 113 | `Win1_ShisuPanel` | the panel of the next record | 6000 of 6000 |
| 114 | `Win1_DrawShisuPanel` | an empty row colour 6 | 85 of 6000 |
| 115 | `Win1_DrawShisuPanel` | the cursor from +0xB | 4795 of 6000 |
| 116 | `Win1_DrawShisuPanel` | the cursor row raised 1 | 4783 of 6000 |
| 117 | `Win1_DrawShisuPanel` | the title 5 a byte | 6000 of 6000 |
| 118 | `Win1_DrawShisuPanel` | one right-edge piece fewer | 6000 of 6000 |
| 119 | `Win1_DrawShisuPanel` | corner piece 0x33 | 6000 of 6000 |
| 120 | `Window_Handler2Kinds` | kind 2 runs kind 3 | 1241 of 6000 |
| 121 | `Win2_ItemList` | the list of the next record | 6000 of 6000 |
| 122 | `MenuSlide_LeftOff170` | held at the bound too | 285 of 6000 |
| 123 | `MenuSlide_LeftOff170` | left 0x10 a frame | 4451 of 6000 |
| 124 | `MenuSlide_RightTo80` | held above 0x51 | 328 of 6000 |
| 125 | `MenuSlide_RightTo80` | right 0x10 a frame | 4002 of 6000 |
| 126 | `Win2_ItemPanel` | the panel of the next record | 6000 of 6000 |
| 127 | `Win2_EquipCompare` | no_preview from +0xC | 5507 of 6000 |
| 128 | `Win2_TitleBox` | the title box 0x14 high | 6000 of 6000 |
| 129 | `Win2_TitleBox` | the record not re-read after the message | 23 of 6000 |
| 130 | `MenuSlide_DownTo40` | held at 0x28 too | 541 of 6000 |
| 131 | `Win2_DrawItemList` | eight rows | 5979 of 6000 |
| 132 | `Win2_DrawItemList` | dim inverted | 3022 of 6000 |
| 133 | `Win2_DrawItemList` | the cursor colour 3 | 3524 of 6000 |
| 134 | `Win2_DrawItemList` | key items count 2 | 1208 of 6000 |
| 135 | `Win2_DrawItemList` | the marked row not raised | 2697 of 6000 |
| 136 | `Win2_DrawItemList` | the shadow skipped on the cursor colour | 4274 of 6000 |
| 137 | `Win2_DrawItemList` | the raised row's category not re-read | 5 of 6000 |
| 138 | `Win2_DrawItemList` | the title 7 a character | 6000 of 6000 |
| 139 | `Win2_DrawItemList` | the room 0x7F | 4792 of 6000 |
| 140 | `Win2_DrawItemList` | the countdown on bits 5..7 | 506 of 6000 |
| 141 | `Win2_DrawItemList` | the up arrow on bit 0 | 1660 of 6000 |
| 142 | `Win2_DrawItemList` | the scroll bar over 8 rows | 6000 of 6000 |
| 143 | `Win2_DrawItemList` | the scroll offset unsigned | 2983 of 6000 |
| 144 | `Win2_DrawItemList` | the top not re-read per row | 63 of 6000 |
| 145 | `Tactics_Top` | the step read before the cancel's two sounds | 28 of 6000 |
| 146 | `TacticsFormation_Leave` | the step read before the backdrop | 80 of 6000 |
| 147 | `TacticsFormation_Enter` | the countdown read before the backdrop | 89 of 6000 |
| 148 | `Win1_ButtonRow` | the selection +0xB read before the state call | 31 of 6000 |
| 149 | `Window_Kind1List` | the last index read once, before the draws | 30 of 6000 |
| 150 | `TacticsMembers_Pick` | the held row from the row read before the sound | 2 of 6000 |
| 151 | `TacticsMembers_Swap` | the held row not re-read after the party swap | 135 of 6000 |
| 152 | `TacticsMembers_Swap` | the row not re-read after the party swap | 108 of 6000 |

**Under the repaired disturbance (2026-10-05, round fourteen's review item 1).** Before `b9dfe34` the group's cases 0, 3, 6, 9 and 12 never ran (the step, the countdown, the current record's +0xB, the kind-1 last index, the members rows); rows 1..144 were refused by the other cases. The 144 were re-run on the repaired fuzz (a copy of the driver, `controls/rest_2f/` in session 8cb2a236's scratchpad, anchors converted for a CRLF checkout, none repaired): **139 refused, the same five equivalents not refused**; 38 counts moved, none to 0. The four weakest fell - C111 28 to 21, C129 23 to 12, C137 5 to 3, C144 63 to 45, each a record re-read that the live cases 8 and 14 now move a third less often. Rows 145..152 are new, on cells only the formerly dead cases moved (case 0: 145, 146; case 3: 147; case 6: 148; case 9: 149; case 12: 150..152). With those cases skipped (a scratch gate, not committed) 149 and 150 fall to 0 and 148 from 31 to 8; 145..147 keep 22, 69 and 73 (the harness's own case 11 moves the step and countdown bytes too). 151 and 152 were refused in 2 and 0 rounds by case 12 alone: the stand-in in the way is 0x58BD50, and the fuzz now re-lists it (`SwapEffect`: the swap as the field-standard row's `FxSwap`, and a quarter of the time the members row or held row moved to 0..4), since `TacticsMembers_Swap` reads both again after its party swap. On that fuzz the shadow is `294000 rounds over 49 functions (6000 each), 1842536 calls to the stand-ins, 0 MISMATCHES`, and all 152 were run again: 139 of 1..144 refused, the same five not, 43 counts moved by at most 51 (C96), none to 0; rows 145..152 above are that run. 150 stays at 2 rounds: its re-read is under a sound, which only case 12 reaches.

## 6. Cross-group calls, inbound calls, the rebinding

**Raw calls out of the group** (`rest_2f_callees.h`): `0x58BD50` (R2E's byte
swap; the field-standard row `"0x58BD50"` stands in for it), `0x460CB0`
(R4F's Config machine, `ConfigMenu_Body`'s tail jump). Everything else the 49
call is ours by name.

**Inbound calls from outside the group:** R2E's `0x58D7D0` -> `0x58ED40`,
`0x58E640` -> `0x58EE40`, `0x58E9D0` -> `0x58F050`, `0x58EA70` -> `0x58F000`
(Capcom's code until R2E merges; nothing to rebind on our side);
`Effect_DrainHp` / `Effect_DrainAp` -> `0x590E80`; `SharedList_DrawList` ->
`0x591AC0`; `ItemTrade_Confirm` -> `0x594D90`; `Window_Kind1Open` ->
`0x596330`; `Window_Kind1Frame` -> `0x596090`, `0x596120` (tail);
`Field_RunTaskRecords` -> `0x596530`, `0x5968E0`; the `.data` tables
`0x66B2E8` and `0x66B570` (other handlers') -> `0x596920`;
`FieldMenu_States[5]`, `[7]`, `[8]`.

**Rebound** (the round-ten form: the value unchanged, the fuzz keys stand):
`battle_e5_callees.h` `kStatAdd`, `field_e2_callees.h` `kTradeTake`,
`field_s_callees.h` `kAbilityListCount`, `window_task_callees.h`
`kListSetUp` / `kListDraw` / `kListCursorDraw` (and its include of
`symbols.gen.h`).

**Left raw, for the coordinator:**

- `window_task_callees.h` `kRecordHandlers[9]`: one line holding
  `0x596530` and `0x5968E0` beside R2G's / R2H's handlers (`0x598890`,
  `0x59B220`, `0x59CB00`) - rebinding it here would conflict with theirs.
- Harness rows (not edited, per the brief): `scenario_harness.cpp`
  `{"0x594D90", ...}` (no arguments, garbage - matches the read) and
  `{"0x591AC0", ..., {kU8, kU8, kU8}, kGarbage}` (the masks match; its comment
  says "list pointer" but the function answers a count in al - a kFlag-like
  answer would be louder); `boss_harness.cpp` `{"0x590E80", ..., {kAll, kAll,
  kAll}, kGarbage}` (the read: a pointer, the cap's low word, the delta's low
  word, an answer whose low word the callers read). All three key on the
  address, so they stand after the merge; they move to `_OURS` with the
  rebinding pass.
- Comments naming the addresses in `battle_e5.cpp`, `field_e2.cpp`,
  `field_s.cpp`, `menu_lists.cpp`, `window_task.cpp`, `window_task_fuzz.cpp`,
  `battle_windows.cpp`: prose, left.

## 7. Latent defects (described, not fixed)

1. **The grid loops never end on a column of three closed cells**
   (`TacticsFormation_Pick`): the row loop adds the step (0 without a press)
   and wraps until an open cell; with the cursor's own cell closed and no
   press, or the cursor's column all closed, it spins forever. Ours spins the
   same. Play keeps the cursor on open cells (`TacticsFormation_Build` opens
   column 0's party rows and the cursor starts at 0, 0) - not shown reachable.
2. **The reserve flags overlap the reserve list** (`TacticsMembers_Build`):
   the flags `0x6BDFCD + 2 j` for j below the reserve's count are written after
   the list `0x6BDFD4`.., so from the fifth reserve member on they land on list
   entries 1, 3, ...; and `0x6BDFD7` (the refused member, set to 0xFF last) is
   list entry 3, `0x6BDFD8` (Config's saved cursor) entry 4. Whether a reserve
   of four or more members exists in play is not read here. Ours writes the
   same bytes in the same order (control 68).
3. **The members screen's places run into the state tables**: rows past 2
   index `0x6674C8` past its six entries, into `FieldMenu_State8Steps` and
   `ConfigMenu_Steps` (code pointers read as x / y). Read in place, as the
   original.
4. **A category turned from 4 mid-draw reads a null list**
   (`Win2_DrawItemList`): the count list is taken once (`Inventory_CountLists[4]`
   is 0) and the category re-read per row; a 4 that became 0..3 under a call
   would read the null list. Nothing the draw calls writes the category; the
   fuzz's disturbance found it and keeps a 4 a 4 (as menu-windows.md's D40).
5. **Unchecked dispatchers**: all fifteen tables above (ours aborts with a
   message past each count). `FieldMenu_State8Steps` is four bare rets, so a
   state-8 step of 0..3 does nothing.
6. **`Stat_AddClampedTo`'s answer** has only its low word defined: in the
   clamped cases its upper half is the caller's `ecx`'s. Its callers read a
   s16 amount, so nothing depends on it; ours puts 0 there.

Nothing here needs a ledger entry: no read of memory never written reaches a
draw or a decision that ours does differently.

## 8. What nothing reached

- **Any of this live.** No trace in `analysis/calltrace` (179 call-count
  files) enters any of the 49, and the cut's reach column is empty for all 48
  rows. The menu walk (`tools/recipes/menu_screens.txt`) opens Tactics and
  Config and should reach `Tactics_*`, `ConfigMenu_*` and handlers 1 / 2; the
  camp route (`campingFishing.txt`, HANDOFF item 0000) enters window kind 1
  (`Window_Kind1Open` -> `Window_Kind1Layout`) - the coordinator's A/Bs after
  the merge are the check. The state hash will compare `0x6BDFBC..` and the
  window records frame for frame.
- **Within the functions:** the grid loops' endless cases and the null count
  list (section 7) are never seeded (both sides would hang or fault); an index
  past a table likewise.

## 9. Live coverage

None measured (section 8). Fuzz only.

## 10. For the batch check

Added to the main checkout's `analysis/calltrace/entries_logic.txt` (33
lines; 9 starts already had a line of the read extent). **Seven existing lines
carry a longer extent over functions of ours and were left** (the coordinator
splits them at the round's end): `0058EE50 1AA` (over `0x58EEC0`, `0x58EF60`;
read 0x70), `0058F050 D01` (over `0x58F080`..`0x58FC60`; read 0x21),
`005902A0 145` (over `0x5902D0`..`0x590350`; read 0x26), `00596090 40` (read
0x34), `00596330 300` (over `0x596530`..`0x596610`; read 0x1F2),
`00596630 458` (over `0x5968E0`..`0x596A60`; read 0x2AA), `00596A90 510`
(read 0x508).
