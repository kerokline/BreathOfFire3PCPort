# The solo-character round: the two members screens (group MS)

**Status:** SUPERSEDED (2026-11-12, the owner, round doc 6f: the game's
screens stay unchanged, so the champion's mark and, before it, the scrolling
reserve were built, merged and taken out of `solo/round` the same day. The
code is on branch `solo/ms`: the mark at `0bf9b744`, the scrolling reserve at
`9b60b48d`. What stands: how a cleared save is drawn (section 5a) and the
original's defect at the fourth reserve member (section 2a, Capcom's,
unfixed, not numbered). Round doc:
[`solo-character-round.md`](solo-character-round.md).)

**Scope changed the same day (the owner, 2026-11-12): "lets by default have 3
walkers like the normal game."** The map party in the mode is the game's own
three; only combat changes. So the mode no longer produces four or more in
reserve, and the long, scrolling reserve list built for both screens (section
5, commit `9b60b48d`, with the cleared look on top of it at `0d7a20de`) was
removed: both screens are the original's code in every state, and only the
champion's mark remains, while `solo::Solo_Active()`. The finding of section 2
stands as a latent defect of Capcom's, unfixed (section 2a).

## 1. The answer in one paragraph

**Both members screens break from the fourth reserve member on** (Capcom's
latent defect, section 2): each keeps its reserve in three bytes followed by
a byte of its own, and its cursor places hold three rows. The original
reaches it only with four or more joined members outside a team of three,
which no measured save holds (section 3). With a two-walker map team the
mode would have reached it from chapter 9 on, so a scrolling reserve was
built; the owner then kept the game's three walkers, and it was removed.
What stays: the champion's panel on both screens carries the look the load
list gives a cleared save, the gold frame (section 5a), only while the mode
is active. The swap rules are the original's (section 4).

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

## 2a. The latent defect, for numbering (Capcom's; unfixed)

**Draft A.** "The members screens hold three reserve rows; a fourth breaks
both. Tactics' list `0x6BDFD4` has the refused byte `0x6BDFD7` as its
fourth byte, which `TacticsMembers_Build` sets to 0xFF after the list: the
fourth row shows record 0 (`Character_IndexById[0xFF]` reads 0 in the image)
and a swap with it does nothing. PartyForm's fourth byte is its own count
`0x6BC897`, written after the list: the fourth row shows id 4's record. Both
cursor tables hold three rows, so row 3's place is off screen, and row 3 is
drawn past line 240. From the fifth member the reserve flags (never read)
land on the list (`rest_2f.md` 7 item 2, `field_s.md` 10 L1/L2). Reached
only with four or more joined outside a team of three: no measured save holds
that; the chapter 14 desert holds five outside a team of one, and its members
choice is refused (`Field_InputFlags` 0). Shown live with a poke (section 6,
`off_members`, `off_swap`). A fix - a reserve of ours shown three rows at a
time with scrolling - was built and removed (commit `9b60b48d`)."

## 3. How large the reserve gets, and when

## 4. What a swap moves

Both screens move who walks: the first list (and the second) and the
reserve. The mode does not change them:

- **Two party rows**: only the order of the first list, so the lead.
- **Two reserve rows**: only the reserve's order.
- **A party row and a reserve row**: refused when either member has `+0xB`
  bit 1 (the party's asked first; the refused member is named on the help
  line), else the party member is replaced by the reserve member in both
  lists. Counts never change. Measured with the wide build and a two-walker
  experiment (section 6, `on_swap_nina`): a swap through a scrolled row
  replaced Nina in both lists; Ryu, marked, was refused.

## 5. What was built, and what was removed

**Built and kept** (this branch's tip):

- `src/game/solo/solo_screens.{h,cpp}`: `IsChampion(id)` (the mode active
  and the id's record the champion's), the mark's constants
  (`kChampionMark`: `kCleared`, or `kHand` - the game's pointing hand, kept
  for comparison; `kChampionClearEmblem`, off), `DrawCleared` and `InFront`
  (section 5a).
- **Tactics** (`field_ability_2.cpp`): `TacticsMembers_Pick` is the
  original's; while `solo::Solo_Active()` it calls `TacticsChampionMark`
  after placing its cursor records (and, with `kHand`, closes record 21 with
  the cursors on the way out).
- **PartyForm** (`party_form.cpp`): `PartyForm_Draw` is the original's; while
  `solo::Solo_Active()` it calls `PartyFormChampionMark` after its two boxes.
- Each finds the champion in the party column (rows below `Party_Count(0)`)
  or the reserve's (rows below the screen's count, to the three rows the
  cursor places hold) by `IsChampion`.

With the mode off both screens run the original's code with nothing added:
the only new statements are the two `if (solo::Solo_Active())` calls.

**Built and removed** (commit `9b60b48d`; the cleared look on top of it at
`0d7a20de`): a reserve list of ours with room for eight, shown three rows at
a time with scrolling, on both screens, behind `solo_screens::Wide()`
(`Solo_Active()`; one line to make it unconditional), with its self-test
(179 checks). It was measured working (section 6, `on_members_nina`,
`on_pf_nina`, `on_swap_nina`). Removed when the owner kept the game's three
walkers: the mode no longer produces four in reserve.

## 5a. The champion in the cleared save's look (the owner, 2026-11-12)

The owner, on the hand: "What about the 'cleared' look of a post-endgame
save? Can we highlight the character like that?"

**What the cleared look is, by the code.** The save summary's byte `+0x14`
(block `+0xCB4`, event flag 146; `docs/save-editor-spec.md`) is read by one
drawer, `Menu_DrawSaveSlot` `0x576960` (`src/game/menu/menu.cpp:1464-1498`;
`records.h` `SaveSummary::marked`). With it set, two things differ from an
ordinary slot and nothing else:

1. The slot's frame pieces (`0x663584`, 72 records: the border, the name
   box, the LV and TIME labels) go to `Menu_DrawPieces` with **flags 2**
   instead of 0 (`menu.cpp:1491`, `:1495`). `Menu_DrawPiece`
   (`src/game/menu/menu_2.cpp:495-516`) passes flags bit 1 to its CLUT: (0x80,
   0x1E2) in place of (0xB0, 0x1E1). Same pieces, same places, another
   palette: the frame turns gold.
2. Piece **0x3D** is drawn at (x + 0x32, y + 0x16) with flags 0
   (`menu.cpp:1496`): rectangle (u 0xC8, v 0x30, 27 x 13) of `0x663B94`, the
   "CLEAR" emblem, right of the level (x + 0x1F, y + 0x18).

No other reader was found: in the source `summary[0x14]` appears only at
`menu.cpp:1491` and `:1496`, and `pe_xref.py` finds one absolute reference to
the live byte `0x904694` (a store in `Save_BuildBlock` `0x5806F0`) and none to
`0x905BD4` (slot 0's copy); the title and status screens were not found to
read it. The reference capture, the owner's cleared save copied into
MS's own game folder as slot 1 beside `adult_ryu` in slot 0:
`D:\bof3x-runs\solo1112\MS\shots\cleared_ref\load_list.png`.

**Carried to the members screens.** The party column's panel is
`Menu_DrawMemberStatus` `0x573560` (`menu.cpp:241-293`), which ends with its
own frame pieces `Menu_MemberPieces` `0x6632C8` (46 pieces, flags 0) and then
its EXP bar. So on the champion's panel: those pieces again with flags 2 - the
cleared slot's palette, exactly as the slot gets it - and the EXP bar again
over them (the panel's last draw). That is the cleared treatment, faithful.
What does not carry:

- **The reserve rows have no piece frame.** `MenuList_ReserveWinDraw`
  (Tactics) and `PartyForm_DrawReserve` draw a `Menu_DrawBox` box and
  `Menu_DrawTile16` tags in a fixed CLUT (0x7800), so there is no frame to
  recolour. Laying the member frame over a reserve row was tried and
  captured: its opaque name box and label pieces cover the row's level and HP
  (the boxes coincide - both 0x7D x 0x30 - but the contents are laid out
  differently). Built instead, the closest honest version: **the champion's
  reserve row is drawn as a member status panel** (`Menu_DrawMemberStatus`
  at the box less (3, 3), the panel's own geometry) with the gold frame. One
  side effect named: that call sets or clears the record's `+0x10` bit 13 (HP
  below a quarter) from its HP, as it does for every member it draws; for a
  reserve champion the original's screen would not make that write.
- **The "CLEAR" emblem is off** (`kChampionClearEmblem`, one constant). On
  the slot it sits in free space right of a two-digit level; the member
  panel's level is "%3d" at x + 0x16 with the HP line 8 below, so the 13-high
  emblem at the slot's distance (x + 0x2F, y + 0x15) covers the top of the HP
  line, and its word says "cleared", not "champion".
- **Draw order.** On Tactics the panels are window records drawn by
  `Field_RunTaskRecords` after the members step runs, so anything the step
  draws in slot 1 lies under them (the green box of the first try).
  `solo_screens::InFront` points slot 1's ordering-table tail at slot 0's for
  the draws and puts both back (the save and restore the field's draw passes
  make of `Gfx_OtPointers`); slot 0 is drawn after slot 1, measured by the
  captures (the gold frame shows over the panels). The cursor and the pick
  boxes are drawn again in front when they sit on the champion, so the cursor
  stays visible over the gold frame. PartyForm draws its panels itself, the
  same helper used for one path.

The hand stays one constant away: `solo_screens::kChampionMark =
ChampionMark::kHand` (`src/game/solo/solo_screens.h`).

## 6. Checks and captures

All 2026-11-12, from the harness root `D:\bof3x-runs\solo1112\MS\root` (a
launcher copy with no ini, a game folder with DAT / BGM / SND junctioned, the
recipe saves copied); captures under `D:\bof3x-runs\solo1112\MS\shots` (the
game's own frames, outside the tree).

**The tip (the mark alone).**

- **Build** (i686, llvm-mingw): exit 0, no warning in the touched files.
  `tools/ledger_check.py`: 229 entries, 0 errors; `tools/ledger_switches.py
  --check`: 0 errors.
- **Self-tests, headless**, on this tip's build: hosted `BOF3X_SELFTEST_ONLY=1
  BOF3X_SHADOW='*'` exit 0, every one of the 916 `MISMATCHES` lines 0
  (`rest_2f` and `field_s`, the fuzz of both screens' bodies, among them),
  `solo self-test: 132 checks passed`; the exe host exit 0, the same solo
  line. (The removed reserve model's own self-test went with it.)
- **Mode off is the original's code:** the only statements added to either
  screen are `if (solo::Solo_Active()) ...Mark()` (and, with `kHand`, one
  close of record 21 under the same test). Captured:
  `cleared_mark\off_tactics` shows the screen unmarked.

**The captures of the tip** - three walkers, from the recipe save `issue190`
(chapter 11; party Ryu, Nina, Momo; reserve Garr, Rei, Peco), recipes
`members3.txt` and `partyform3.txt` in `D:\bof3x-runs\solo1112\MS\shots\recipes`.
**Two caveats.** (1) Each needs a poke, so none is a player's route: the
members choice is refused in that area (`Field_InputFlags` reads 6, bit 0
clear: `poke 0x905BA2 u8 1`), and PartyForm is opened by `poke 0x66C7D8 u8 6`
(`Field_Request`, as `Party_Join` sets it). (2) The branch still holds the
round's field half (FS's two-walker fold, which the round branch is
reverting), so the capture binary was built with `solo_field.cpp`'s
`ModeOn()` answering false - a local edit, not committed, restored before the
commit - which is what the revert does to the field.

| Capture (`D:\bof3x-runs\solo1112\MS\shots\cleared_mark\...`) | What |
|---|---|
| `tactics_ryu\members_*.png` | the champion is the hero, first in the party (his panel also dimmed by his own "must stay" mark, as the original draws it) |
| `tactics_nina\members_*.png` | the champion in the party's second row: her panel in the gold frame |
| `tactics_rei\members_*.png` | the champion in reserve: his row drawn as a member panel in the gold frame |
| `partyform_ryu`, `partyform_nina`, `partyform_rei` `\pf_*.png` | the same on PartyForm |
| `off_tactics\members_*.png` | mode off: unmarked |
| `wide_*` | the removed wide build with the cleared look (adult_ryu, five in reserve, scrolling) |
| `..\cleared_ref\load_list.png` | the reference: the owner's cleared save (a copy, slot 1) under `adult_ryu` (slot 0) on the load list |

**The removed wide build's runs** (`adult_ryu`, chapter 14, the hero alone,
six joined; recipes `members.txt`, `partyform.txt`, `swap.txt`; the same
pokes, and `swap.txt` also Nina into both lists for a map team of two):

| Run (`D:\bof3x-runs\solo1112\MS\shots\...`) | What |
|---|---|
| `off_members` (mode off) | the original with five in reserve: the window `08 01 04 FF` for the ids `08 02 04 05 06`; Nina twice (flag 4 over Garr's id), Ryu as a fourth row hanging off the bottom (id 0xFF, record 0), the cursor off screen on rows 3 and 4 |
| `off_swap` (mode off) | four in reserve, `02 04 05 FF`: Peco lost, the fourth row Ryu; a swap with it answers 0xFF and changes nothing |
| `on_members_nina`, `on_pf_nina`, `on_members_ryu` | the scrolling reserve (`08 02 04` -> `02 04 05` -> `04 05 06`), with the hand mark |
| `on_swap_nina` | two walkers, four in reserve: Nina swapped with Peco on scrolled row 3 (both lists `07 06 FF`, reserve `02 04 05 08`); Ryu refused (`0x6BDFD7` = 7) |

No recorded route opens the members step or PartyForm. For the owner to
record (`BOF3X_RECORD`): from a save where the members choice is allowed, the
field menu, Tactics, the members choice, both columns; and a scene that opens
PartyForm (chapter 6's or 8's `Request(6)`).

## 7. Not done, and open

- Not played; not run on a recorded route (none reaches the screens).
- The cleared look is the owner's to judge by eye; the hand
  (`kChampionMark = kHand`) and the emblem (`kChampionClearEmblem`) are one
  constant each.
- A champion in reserve is shown as a member panel, not as the reserve row
  it is (section 5a): the reserve rows have no frame to recolour.
- The latent defect (section 2a) stays as the original has it.
- Chapter 14's whelp scene (seven joined, section 3) was not run.
- The 64-bit build was not built here.
