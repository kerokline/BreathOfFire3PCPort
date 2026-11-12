# The solo-character round: the two members screens (group MS)

**Status:** IN PROGRESS (verified 2026-11-12: built and measured on `solo/ms`, from the
round branch `solo/round` `10646687`; headless self-tests and captures in
section 6; not played, not merged). Round doc
[`solo-character-round.md`](solo-character-round.md) 4 "MS", 0a item 9 and
6a item 1 govern; the code is `src/game/solo/solo_screens.{h,cpp}`, the
Tactics members step in `src/game/field/field_ability_2.cpp` and `PartyForm`
in `src/game/field/party_form.cpp`.

## 1. The answer in one paragraph

**The mode reaches both screens' latent defects, from the fourth reserve
member on, and in ordinary play.** Each screen keeps its reserve in a list
of three bytes followed by a byte of its own, and its cursor places hold
three rows; a fourth member is written over that byte and has no place. In
the mode the map team is the hero and at most one other (6a item 1; FS's
rule: every "must stay" member and at most one unmarked), so six joined
members - every save of the corpus from chapter 9 to 15 - leave four in
reserve, and the chapter 14 desert, where the hero walks alone with five
joined, leaves five. What was built: both screens use a reserve list of ours
(room for all eight records) shown three rows at a time with scrolling, only
while `solo::Solo_Active()` (one line, `solo_screens::Wide()`, makes it
unconditional); and the game's pointing hand marks the champion's row on both.
Nothing else: the swap rules the original has already keep the mode's map
team in shape (section 4).

## 2. What each screen holds, and where a reserve of four breaks it

By reading `src/game/field/field_ability_2.cpp` (`TacticsMembers_Build`
`0x58FEF0`, `_Pick` `0x58F890`, `_Swap` `0x590020`), `src/game/field/
party_form.cpp` (`PartyForm_Setup` `0x580A60`, `_Choose` `0x580C20`,
`_Swap` `0x581590`, `_Draw` `0x581720`, `_DrawReserve` `0x581300`),
`src/game/menu/menu_list.cpp` (`MenuList_ReserveWinDraw` `0x59AA80`, the
Tactics reserve's window), the cell tables read from `bof3/BOF3.exe`, and
`tools/pe_xref.py --range` over the bytes around each list (2026-11-12).

| | Tactics members step | PartyForm |
|---|---|---|
| Reserve list | `0x6BDFD4`.. (record 12 `+0x20` points at it, `+0xA` its count) | `0x6BC894`.. (count `FieldS_ReserveCount` `0x6BC897`) |
| The list's 4th byte is | `Rest2f_Refused` `0x6BDFD7` (set 0xFF by Build after the list; the refused id or 0xFF after each swap) | its own count `0x6BC897` (written after the list) |
| The list's 5th byte is | `Rest2f_ConfigCursor` `0x6BDFD8` | `FieldS_PartyFormResult` `0x6BC898` (set 0xFF after the count) |
| Flag pairs (party, reserve) | `0x6BDFCC`.. three pairs; reserve flag j at `0x6BDFCD + 2j` | `0x6BC88C`.. three pairs; reserve flag j at `0x6BC88D + 2j` |
| Who reads the flags | nobody (pe_xref: two stores in Build, no load) | nobody (two stores in Setup, no load) |
| Cursor places | `0x6674C8`, 8 bytes a cell, `[col + 2 row]`, six cells (3 rows); then `FieldMenu_State8Steps` | `0x664078`, 6 bytes a cell, `[col + 2 row]`, six cells; then code pointers |
| Rows drawn | every row below the count, 0x34 apart from y 0x41 | the same, from y 0x41 |

What a reserve of **four** does (the defect starts here on both screens,
not at five):

- **Tactics:** the fourth id is written to `0x6BDFD7` and then overwritten
  with 0xFF by Build's last store, so the fourth row draws member 0xFF:
  `Character_IndexById[0xFF]` (`0x66982B`, past the 24-byte table) is 0 in
  the image, so record 0, the hero, a second time; a swap with that row
  puts id 0xFF into the party lists. After a refused swap the byte is the
  refused member's id (a duplicate). Its cursor place is cell 7, `0x667500`: x 0x7CC0, y 0x43,
  size index 0xC0 (`MenuList_CursorBoxSizes` has four) - off screen. The row
  itself is drawn at y 0xDD..0x10D, past the 240-line screen.
- **PartyForm:** the fourth id is written to `0x6BC897` and then overwritten
  with the count (4), so the fourth row is member id 4's record (a second
  copy of record 4 when record 4 is already in the list), and a swap with it
  swaps id 4. Its cursor cell 7 is `0x6640A2`: x 0x58, y 0xC20 - off
  screen.

From **five**: the reserve flags begin to land on the list (Tactics: flag 4
on entry 1, `0x6BDFD5`, written 1 = member id 1; PartyForm: flag 4 on entry
1 and flag 5 on the count itself, which becomes 1), as
[`rest_2f.md`](rest_2f.md) 7 item 2 and [`field_s.md`](field_s.md) 10 L1,
L2 describe. `rest_2f.md` 7 item 2 dates the overlap "from the fifth
member", which is right for the flags; the list's own fourth byte is the
earlier break.

## 3. How large the mode's reserve gets, and when

**The joined counts measured.** Every distinct slot file found on this
machine (54 under the game folders, backups and `bof3x-runs` roots; 67
recipe saves; read 2026-11-12 with a scratch script over the block at file
offset 0: records at `+0x90`, `+0xB` bit 0 joined, the lists at
`+0x682` / `+0x685`): at most **six** records joined at once, always records
0, 1, 2, 4, 5, 6 (the six main cast), in every save from chapter 9 to 15.
No save holds seven. With a team of three the original's reserve is then
three - its three rows.

**In the mode.** FS's map team is every "must stay" member and at most one
unmarked; the hero's mark is permanent (M7), so with two or more joined the
team is the hero and one other unless a scene marks a third. So:

| Joined | Map team | Reserve | Where measured |
|---|---|---|---|
| 6 (records 0, 1, 2, 4, 5, 6) | hero + one | **4** | every save of chapters 9..15 |
| 6, the hero walking alone | hero | **5** | `adult_ryu` (chapter 14, the first list `07 FF FF`), `desertToManmo`, `desertSummary` and the other chapter 14 desert saves |
| 7 (six + the whelp) | hero (+ one) | 5 or 6 | read, not measured: chapter 14's `Scena14_SaveLeaveAllPartyA` (`0x51A810`) sends the three walkers away with `Party_Remove`, which keeps their joined bits (`field_6.cpp:2205-2260`), and joins id 0xA (record 7) alone; `Scena14_Out10Party748` clears it again |

Records 3 and 7 (the two guests) join with few others: Teepo (record 3) in
chapters 1-2 beside records 0 and 4 (`scena_01.cpp:62`, `scena_call_5.cpp`),
the whelp in chapter 8's opening after `Scena08_PartyBitsToRecord0` has left
record 0 alone (`scena_08.cpp:46-61`), in chapter 14 as above, and in
chapter 16 (the attract demo, where the mode stands down).

**So the mode reaches the defect** whenever all six have joined and a
members screen opens with a reserve of four or more: from chapter 9 on.
Whether the original itself reaches it in the chapter 14 desert depends on
whether the members choice is allowed there (`Field_InputFlags` `0x905BA2`
bit 0; section 6).

## 4. What a swap moves in the mode

Both screens move who walks: the first list (and the second) and the
reserve. Read against the mode's shape:

- **Two party rows** (the original's code, unchanged): only the order of the
  first list changes, so the lead; both lists keep the same members.
- **Two reserve rows**: only the reserve's order.
- **A party row and a reserve row**: refused when either member has `+0xB`
  bit 1 (the party's asked first; the refused member is named on the help
  line), else the party member is replaced by the reserve member in both
  lists. The hero's mark is permanent, so the hero can never be swapped out;
  a scene's mark keeps its member the same way; a marked member in reserve
  cannot come in. So the map team after a swap is again the marked members
  and at most one unmarked - FS's rule - with the same count and both lists
  holding the same members. Nothing for FS's fold to repair.

The original does exactly this; the mode adds nothing here.

## 5. What was built

- `src/game/solo/solo_screens.{h,cpp}`: the switch `Wide()` (the mode
  active; `return true;` makes the long reserve unconditional), the
  champion's test `IsChampion(id)`, the reserve model `Reserve` (eight ids,
  the count, the first row shown) with `Fill`, `Window`, `Shown`, `Whole`,
  `ShownRow`, `Hold` and `Move` (the original's wrap, the view scrolled to
  keep the row shown), and its self-test.
- **Tactics** (`field_ability_2.cpp`): `TacticsMembers_Build` with `Wide()`
  runs `TacticsMembersBuildWide` and latches `g_tactics.wide`; while it is
  set `TacticsMembers_Pick` runs `TacticsMembersPickWide` and
  `TacticsMembers_Swap` runs `TacticsMembersSwapWide` for any swap with a
  reserve row. `0x6BDFD4`.. holds the rows shown and record 12's `+0xA`
  their number, so `MenuList_ReserveWinDraw` is untouched; in the reserve
  column the cursor's and the pick's row bytes hold the row of the whole
  list; a picked row scrolled out of sight is kept, unmarked.
- **PartyForm** (`party_form.cpp`): the same, through `PartyForm_Setup`
  (latch), `_Choose`, `_Swap` and `_Draw`; `_DrawReserve` and
  `_DrawSliding` untouched.
- **The champion's mark**: the game's pointing hand (`Menu_DrawHand`, the
  hand of the Tactics top and every menu's choices), its tip at the
  champion's cursor place plus (0x12, 0): inside the panel's top-left
  corner, pointing at the name. On Tactics it is window record 21 - the
  hand `Tactics_Open` sets up (handler 6, kind 0xE), closed in the members
  step and set afresh by `Tactics_Top` every frame there - opened by
  `TacticsMembersPickWide` and closed with the two cursors on the way out;
  on PartyForm `PartyFormDrawWide` calls `Menu_DrawHand` after its boxes.
  Only while `Solo_Active()`. A first try, a steady green
  `Menu_DrawCursorBox` drawn from the Tactics step, was hidden: the step
  runs before `Field_RunTaskRecords` draws the panels over it (captured
  2026-11-12), so on Tactics a mark must be a window record.

With the mode off every edited function takes the original's body: the
first statement of each asks the latch or `Wide()`, both false.

## 6. Checks and captures

All 2026-11-12, on the branch's build, from the harness root
`D:\bof3x-runs\solo1112\MS\root` (a launcher copy with no ini, a game folder
with DAT / BGM / SND junctioned, the recipe saves copied); captures under
`D:\bof3x-runs\solo1112\MS\shots` (the game's own frames, outside the tree).

- **Build** (i686, llvm-mingw): exit 0, no warning in the touched files.
  `tools/ledger_check.py`: 229 entries, 0 errors; `tools/ledger_switches.py
  --check`: 0 errors.
- **Self-tests, headless:** hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`:
  exit 0, every one of the 916 `MISMATCHES` lines 0, among them `rest_2f
  self-test: 294000 rounds over 49 functions ... 0 MISMATCHES` and `field_s
  self-test: 212000 rounds over 53 functions ... 0 MISMATCHES` (the fuzz of
  both screens' original bodies), `solo self-test: 48 checks passed`,
  `solo_screens self-test: 179 checks passed`. Exe host: exit 0, the same two
  solo lines.
- **Mode off takes the original's path:** every edited function first asks
  `Wide()` (Build, Setup) or the latch it set (Pick, Swap, Choose, Draw), and
  `Wide()` is `Solo_Active()`, false with `SoloCharacter` off. Measured: the
  mode-off runs log no `solo members screen` line (the wide builds log one),
  and show the original's bytes - the window `08 01 04 FF` with five in
  reserve (the ids `08 02 04 05 06`: flag 4 on entry 1, the refused byte on
  entry 3), `02 04 05 FF` with four. The shadow fuzz passing is the same
  answer for the bodies.

**The recipes** (in `D:\bof3x-runs\solo1112\MS\shots\recipes`, not in the
tree): `members.txt` opens Tactics as `tools/recipes/menu_screens.txt` does,
from the `adult_ryu` recipe save (chapter 14, the hero alone, six joined), by
conditions (`until` / `seek`), then the members choice and the reserve column
walked down. **Each needs a poke, so none is a player's route:** the members
choice is refused in that area (`Field_InputFlags` `0x905BA2` reads 0), so
`poke 0x905BA2 u8 1`; `partyform.txt` opens PartyForm by `poke 0x66C7D8 u8 6`
(`Field_Request`, as `Party_Join` sets it); `swap.txt` also pokes Nina (id 8)
into both lists' second byte for a map team of two (the field menu had set up
one party panel, so the second panel is not drawn: the poke's artifact). No
recorded route opens the members step or PartyForm. For the owner to record
(`BOF3X_RECORD`): from a save of chapter 9..15 where the members choice is
allowed, with all six joined, the field menu, Tactics, the members choice,
the reserve column down to its last row and back; and a scene that opens
PartyForm (chapter 6's or 8's `Request(6)`) - with the mode on, the map team
of two.

**What the captures show:**

| Run | Shots | What |
|---|---|---|
| `off_members` (mode off) | `members_party`, `_row0`, `_row2`, `_row3`, `_row4`, `_wrap` | the original with five in reserve: Nina twice (flag 4 over Garr's id), Ryu as a fourth row hanging off the bottom (id 0xFF, record 0), the cursor off screen on rows 3 and 4 |
| `off_swap` (mode off) | `sw_picked`, ... | four in reserve: Peco lost, the fourth row Ryu again; a swap with it answers 0xFF and changes nothing |
| `on_members_nina` | the same six | five in reserve shown three at a time: `08 02 04` -> `02 04 05` (row 3) -> `04 05 06` (row 4), wrap to row 0; the hand on Nina while her row is shown |
| `on_members_ryu` | the same six | the hand on the party panel (it covers the member-number badge) |
| `on_pf_nina` | `pf_party`, `pf_row0`, `pf_row3`, `pf_row4`, `pf_back` | PartyForm: the same scroll, the hand on Nina |
| `on_swap_nina` | `sw_start`, `sw_picked`, `sw_swapped`, `sw_refused` | a map team of two (Ryu, Nina), four in reserve: Nina swapped with Peco on reserve row 3 (scrolled) - both lists `07 06 FF`, the reserve `02 04 05 08`; then Ryu picked against a reserve row: refused (`0x6BDFD7` = 7, his id; lists unchanged) |

## 7. Not done, and open

- Not played; not run on a recorded route (none reaches the screens).
- No scroll indicator: nothing on screen says a fourth row exists below
  until the cursor reaches it. The game draws no arrows on these screens;
  an indicator would be new layout (the owner's call).
- The hand's place is a proposal for the owner's eye; on the party panel it
  covers the "1" badge. The alternative offered: a steady box in another
  colour round the champion's panel (`Menu_DrawCursorBox` sides 2, green),
  which PartyForm can draw as built but Tactics only from a draw after
  `Field_RunTaskRecords` (a file of another group's).
- Both defects stay as the original has them with the mode off (round doc 6
  open 2's recommendation). The mode-off reach: no measured save holds four
  outside a team of three, and in the one place the original holds five
  (the chapter 14 desert) the members choice is refused. Chapter 14's
  whelp scene (seven joined, section 3) was not run.
- The 64-bit build was not built here.
