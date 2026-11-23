# The solo-character mode: the merchant and the masters show the champion alone

**Status:** BUILT (2026-11-22, branch `solo/shop-master` off `main`
`8e5d1ea0`). The owner's call, [`solo-character-round.md`](solo-character-round.md)
6l. Ledger: DIV-0228, amended 2026-11-22. Run on the two recorded routes with
the mode off and on, and played by the owner 2026-11-22 (section 4): both
screens work as intended. The camp's Skill Notes (section 6) were built
after that play and are **not played**.

## 1. What changes

While the champion fights (`solo::Solo_ChampionFights()`: the mode active, and
the champion joined or `SoloChampionIfPresent` off - the combat swap's own
test), two screens hold one member, the champion, whether they walk with the
team, sit in reserve or have not joined yet:

- **The merchant's buy screen** (`ShopTrade_*`): the stat panel beside the
  list, the member asked for after "equip it?", the equipment box and the
  equip itself.
- **The masters' member screens** (`MasterTalk_*`, `MasterJoin_*`,
  `MasterQuit_*`, `MasterGrant_*`): the panels, the pick for "be my
  apprentice" and "stop being my apprentice", the two tests that choose the
  master's line (every member a pupil; any member a pupil) and the skills a
  master grants at a visit.

A champion who has not joined has a record from New Game (every record is
built by `NewGame_InitCharacters`, and the mode's level-1 start is made on
it), so their figures and equipment are real.

Nothing else changes. The party lists, the walkers and `Field_MemberCount`
are not written, so the field, a master's scripted test of who leads
(`area_91.cpp:143` and the like) and the field menu see the team as before.

## 2. How the two screens hold their members (by reading the source)

Both keep a count and, per slot, a character record index.

| | The merchant | The masters |
|---|---|---|
| Count | `Party_Count(0)` `0x531BB0`: the entries of the first list `0x904062` | `Field_MemberCount` `0x929EC0` |
| Slot's record | `Character_IndexById` `0x66972C` `[0x904062[slot]]` | `ObjTrio[slot] +0x148`, the walker's record index |
| Where it is read | `shop_trade.cpp` `RecordOf`, `PartyCount` (the equip mask test, `Shop_Equip`, `Char_RecalcStats`, six loops over the panels); `shop_win.cpp` `ShopWin_MemberStatsRun` (window kind 5, the stat panel); `shop_win_2.cpp` `ShopWin_EquipRun` (kind 6, the equipment box) | `master_talk.cpp` `Panels`; `master_talk_2.cpp` `FirstMember`, `MemberAt`, `DrawMasterScreen`, the two pupil tests, the pick's cursor wrap; `master_quit.cpp` `MemberAt`, `DrawMasterScreen`, the two after-checks, `MasterGrant_Step2Next` |

Window kinds 5 and 6 of handler 7 are set up only by `shop_trade.cpp`
(`grep` for the handler's stores over `src/game`: the other handler-7 stores
are window kinds 7 to 10 of the same file), so the two drawers need no test
of which menu is up.

The masters' face comes from one table by character id with a fixed CLUT
(`MasterPanel_DrawFace` `0x586570`), not from the walkers' loaded art. The
shop panel's face draw was not read; section 4's captures show a champion
outside the team drawn correctly on both screens.

## 3. What is built

`src/game/solo/solo_screens.{h,cpp}`: `SoloScreens_Count(game_count)` and
`SoloScreens_Record(slot, game_record, who)`. Each read in section 2's table
passes the game's own value through one of them. With the champion shown
alone the count is 1 and slot 0 is the champion's record; a slot past 0 is a
Fatal naming the reader (the cursor of both screens starts at 0 and wraps on
the count, so none is asked). Otherwise the game's value comes back
unchanged. No state of ours, nothing for a quicksave to carry.

With `SoloCharacter` off, `Solo_ChampionFights()` reads nothing of the save
and answers no: the original's count and records.

**Side effects, named.**

- A walker who is not the champion cannot be equipped at a merchant or taken
  or released by a master while the screens show the champion. The field
  menu still equips them.
- A walker apprenticed before the mode was turned on is not offered that
  master's skills at a visit (the grant runs over the screen's members);
  their apprenticeship itself is untouched.
- The master's lines "all of you are my pupils" / "none of you is my pupil"
  follow the champion alone.

**Not changed.** The sell screen (it shows no member). The resistance shop
(`ShopResist_*`, `shop_resist.cpp`, which reads the walkers): not asked for
and not looked at in play. `SharedList_*`, the field menu, PartyForm.

## 4. Checks

All 2026-11-22, the 32-bit build of this branch, from the harness root
`D:\bof3x-runs\soloshops` (a launcher copy with no ini, a game folder with
DAT / BGM / SND junctioned, the recipe saves copied). Frames are the game's
own, written every 45 recipe frames, outside the tree.

| Run | Route | Environment | Result |
|---|---|---|---|
| `shop_off` | `shop` (save chapter 1) | the behaviours' default | done, rc 0 |
| `shop_nina` | `shop` | `BOF3X_ON=SoloCharacter BOF3X_SOLO=nina` | done, rc 0; stamped at the first area entry |
| `master_off` | `masterAndManillo` (save chapter 3) | default | done, rc 0 |
| `master_garr` | `masterAndManillo` | `BOF3X_ON=SoloCharacter BOF3X_SOLO=garr` | done, rc 0; stamped |

Neither champion is joined at its save's point in the story, so both runs
are the "not joined yet" case.

- **The merchant.** 6 of the 70 frames differ between `shop_off` and
  `shop_nina` (`f01620` to `f01845`), all inside the member panels' box. Off:
  three stat panels. On: one, the champion's, and the equipment box is the
  champion's (`f01755`). The other 64 frames are pixel-identical, the field
  after the shop among them.
- **The master.** 5 of the 122 frames differ between `master_off` and
  `master_garr` (`f03960`, `f04005`, `f04815`, `f05445`, `f05490`): the
  member panel shows the champion in place of the lead walker, and the
  master's line names the champion as the new pupil (`f04005`). The other
  117 are pixel-identical.
- **Self-tests.** headless, from the ini-less launcher copy, on the final
  build. Hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`: 922 `MISMATCHES`
  lines, every one 0 (`rest_2c`, `rest_2d`, `shop_states2` and
  `menu_draw_helpers`, the fuzz of the changed files, among them), `solo
  self-test: 289 checks passed`, "self-test only: done". The exe host
  (`BOF3X_SELFTEST_ONLY=1 BOF3X_EXEFILE=1`): the same solo line, `solo battle
  self-test: 39 checks passed`, exit 0. **The first hosted run failed and was
  right to**: `rest_2c` differed in 241 rounds, all `MasterTalk_PanelsIn` /
  `_PanelsOut`, with the switch off. `Panels` reads the walker's record byte
  once for `MasterPanel_DrawMember` and again for `MasterPanel_DrawStats`;
  the first edit read it once. Fixed (two reads), and the run above is after
  the fix.
- `tools/ledger_check.py`, `tools/ledger_switches.py --check`: 245 entries, 0 errors each;
  `tools/ledger_registry.py --check` and `tools/data_census.py --strict`
  pass (the two changed helpers are kept in the shape the census keys by).

- **The owner's play** (2026-11-22, this branch's build in `game\`,
  `on=SoloCharacter`, champion Rei). The owner's words: "Both masters and
  shops work as intended"; "The stop being my apprentice check works as
  well". The skill grant: first reported as working only with the champion
  in the team, then withdrawn - "that was my mistake - I had tried to edit a
  save, and hadn't set the level high enough. The skill grant does seem to
  be working". On the camp's list: "Masters can't be reached from
  the camp list unless the ultimate edition mod (or something like it) is
  turned on - normally they just show who is apprenticed to that master, but
  I believe it only shows if they are currently in the team (so for instance,
  rei does not currently show as under banyan)".

## 5. Not done, and limits found in play

- **The camp's master list names a master's pupils from the team only**, so
  a champion outside the team is not named under their master (the owner's
  belief from play, not read in the source). Not changed: the list was not
  asked for. Open, the owner's call.
- The masters reached from the camp's list (DIV-0223): they are reached only
  with a mod that turns that on (the owner); not run.
- The recorded routes were made with a team: on the mode-on runs the inputs
  meant for a three-member pick land on a one-member pick. They reach the
  screens and leave them; they are not a player's route through them.
- A champion who walks with the team, one in reserve, "stop being my
  apprentice" and the skill grant: not reached by a recorded route; played
  by the owner (section 4).
- The 64-bit and Linux builds: not built here.
- The state-hash off-proof was not run: with the switch off every changed
  read returns the value the original read, and the hosted fuzz groups of
  these files compare ours with Capcom's code.
- The Skill Notes of section 6: no route run, not played.

## 6. The camp's Skill Notes open on the champion (2026-11-22, round doc 6m)

The owner's play of the two screens found that a champion the story has
taken out of the team could not move skills: the Skill Notes list the joined
records only.

**How the screen holds its members (by reading the source).**
`SharedList_Setup` `0x584F90` (`shared_list.cpp`) writes the records whose
`+0xB` bit 0 ("joined") is set, of the eight in record order, to the eight
bytes at `0x6BC8BC` and their count to `FieldS_JoinedCount` `0x6BC8C5`.
`SharedList_Menu`, `SharedList_UseItem` and `SharedList_PickMember` open on
entry 0 and step left and right through the count. Unlike the merchant and
the masters, the list is not the team: it is every joined member.

**What is built.** `SoloScreens_ChampionFirst(list, count)`
(`solo_screens.cpp`), called by `SharedList_Setup` on the list it has just
made, before it is stored: while the champion fights
(`Solo_ChampionFights()`), the champion's record first, joined or not, then
the other joined records in their order; the new count. Otherwise the list
is stored as the original stores it. The other members are kept because a
skill goes to the notes only from its holder's slots.

This one is a write, not a read-side view: the list at `0x6BC8BC` holds the
new order. It is the screen's own scratch, rebuilt at every opening and read
by nothing else (`grep kJoined src`: `shared_list.cpp` and its fuzz).

**Checks.** `solo self-test`: six cases of the order (a joined champion, one
not joined, one already first, an empty list, the full eight, eight with no
room). Hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'` with the switches
off: 922 `MISMATCHES` lines, every one 0 (`field_s`, the
group of `SharedList_Setup`, among them), `solo self-test: 295 checks
passed`, "self-test only: done". `tools/data_census.py --strict` passes (the list keeps
its one recorded write in `SharedList_Setup`). **No route run and not
played.**
