# The solo-character round: a champion who fights alone

**Status:** STAGED (2026-11-12, from `main` `01a67500`; nothing built, nothing
launched. The design is IDEAS I56's (the owner, 2026-11-03), issue #223; the
owner's four answers of 2026-11-12 are in section 6; M1 is run, section 2.)

The owner (2026-11-03): a "solo character" mode for solo challenges. One
chosen character, the **champion**, fights every battle alone. On the map the
player walks as any joined character: the members screen holds the five others
in reserve and the L swap brings any of them out, so a section that wants a
given character still works. The reason, from the issue: solo challenges are
fun, and knocking your own friends out at the start of every fight is not.

## 0. The shape in one paragraph

Two teams where the game has one. The **map team** is the party as the field
knows it (the first list `0x904062`, the ObjTrio records at `0x802D40`,
`Field_MemberCount`): one walker, plus any member the game marks "must stay".
The **combat team** is ours: the champion. Before a battle loads, in
`GameMode5_Script` (`0x4960D0`, `src/game/core/transition.cpp`) before it
reads `Party_Count(1)`, the map team's records and both lists go to a buffer
of ours and the champion is built into record 0 the way `Field_PartyLoad`
(`0x533CE0`) builds a member. The battle then runs untouched, with one member
from its first frame. After the battle has fully ended, after the game's own
reverts, the champion's results go back to `CharacterRecords` and the map team
comes back. The mode is a launcher setting, off by default; with it off the
game is bit for bit what it is today, and the state hash says so.

What it is not: the dragon takeover reused (IDEAS I56, "Two shortcuts
considered and set aside"), or the second party list used as the combat team.
The takeover is the pattern and the checklist of what a restore rebuilds, not
the mechanism.

## 1. What is already known

IDEAS I56 holds the reads of 2026-11-03 (the two lists, the battle's members
being the field's records, the L swap's two-member test, the party sets, the
members screen's three reserve rows, the scripts' party builders, the dragon
takeover's save and restore, the EXP split). They are not restated here. Where
each function lives today (2026-11-12, `main` `01a67500`):

| Function | pc | File |
|---|---|---|
| `GameMode5_Script` | `0x4960D0` | `src/game/core/transition.cpp` |
| `Field_LeaderSwapTest`, `Party_CanSwap`, `Field_SwapState`, `Field_SwapExchange` | `0x5302C0`, `0x530320`, -, `0x52EE70` | `src/game/field/field_4.cpp` |
| `Party_Join`, `Party_Remove`, `Field_PartyLoad`, `Party_PlaceForBattle` | -, -, `0x533CE0`, - | `src/game/field/field_6.cpp` |
| `Party_AddToLists` | - | `src/game/menu/char.cpp` |
| `TacticsMembers_Swap` | `0x590020` | `src/game/field/field_ability_2.cpp` |
| `PartySet_Load`, `_LoadFirst`, `_LoadSecond`, `PartySet_Find` | `0x5367E0`, `0x536850`, `0x536890`, `0x5368F0` | `src/game/field/mode_11.cpp` |
| `BattleEnd_StartMemberTask` | `0x431220` | `src/game/battle/battle_action.cpp` |
| `BattleFx_RestoreParty` | `0x433DA0` | `src/game/battle/battle_fx.cpp` |
| `BattleResult_SplitExp` | `0x431940` | `src/game/battle/battle_result.cpp` |
| `Accession_ApplyB` | `0x4EB350` | `src/game/magic/accession.cpp` |

(A dash is a pc this pass did not look up; `symbols.toml` has it.)

Every function above is ours (the game's code is all ours since PR #43), so
the round edits source; it patches no bytes.

**The switch mechanism exists.** A change that is off unless asked for is a
*behaviour* (`src/hook/behaviour.h`, [`mods.md`](mods.md) 11): a ledger entry
with `Default: off` whose `Switch:` is the behaviour's name, asked in code as
`BehaviourOn(behaviours::Id::<Name>)`, turned on by `BOF3X_ON` (the launcher's
`on=`). `tools/ledger_switches.py` generates the enum from the calls in
`src/`, and `tools/ledger_check.py` fails a name the ledger lacks. A value
(which character) is not a behaviour; the launcher's ini already carries
values to the game as environment names (`cheat.exp` to `BOF3X_EXP`,
`src/launcher/config.cpp`).

## 2. Group M: the measurements, read-only, first

One agent, no `src/` change, a findings doc `docs/solo-character-reads.md`.
M1 was run at staging; M2 to M5 are I56's remaining four; M6 to M9 are what
the owner's answers and this staging added.

### M1 as run (2026-11-12): the party-set rows

`python` over `bof3/BOF3.exe` at `0x669750`, 19 rows of 3 character ids (the
row after the nineteenth is `00 00 00`, not a row):

| Rows | Ids in the rows | Reading |
|---|---|---|
| 0..6 | `(0,1,2) (0,1,5) (0,1,6) (0,2,5) (0,2,6) (0,3,4) (0,5,6)` | every row holds id 0 |
| 7..18 | `(2,4,7) (2,5,7) (2,6,7) (2,7,8) (2,7,A) (3,4,9) (4,5,7) (4,6,7) (4,7,8) (5,6,7) (5,7,8) (6,7,8)` | every row but `(3,4,9)` holds id 7 |

- **No row holds one character alone, and none needs to.** `PartySet_Find`
  (`mode_11.cpp`) counts the ids that are not `0xFF` and returns at the first
  match of the last counted id: with one id it returns the first row that
  holds it, searched column by column. Every id 0..`0xA` is in some row, so a
  team of one never reaches `PartySet_ErrorLoop`.
- **Every pair among ids 2, 4, 5, 6, 7, 8 shares a row** (the ten pairs of
  `{2,4,5,6,8}` each sit in a row with 7; each of the five with 7 as well).
  So if the map's party set is chosen to hold the champion beside the walker,
  the battle's set is already loaded: **no file load at the battle's start or
  end.** `PartySet_Find` keeps the current set when it holds every id asked
  for, so the choice sticks.
- In rows 0..6 every pair that includes id 0 shares a row with it except
  pairs across `{1,2,5,6}` and `{3,4}`; a pair without id 0 shares one only
  when the row happens to hold both. What those ids are is M6's.

Which character each id is, and which ids belong to which part of the story,
is not asserted here (M6).

### M2 to M9, to run

| | Question | Why the round needs it |
|---|---|---|
| M2 | Where a battle's HP, AP, status and EXP reach `CharacterRecords`, and when. Whether a reserve member's record is touched by a battle at all. | CB's write-back copies exactly these fields at exactly this point; the EXP question (section 6, open 1) waits on the reserve half. |
| M3 | Every way out of a battle: win, flee, game over, an event battle the script continues, the dragon revert's path, a battle that chains into another. | CB's restore runs on each, once. |
| M4 | Whether event battles (`0x904AAA`, `EventBattle_Records`) enter through `GameMode5_Script`, and whether any battle's script reads a member by slot or by character id. | A fight that expects a member finds the champion or it does not; the boss sweep finds the rest (stage C). |
| M5 | A quicksave (DIV-0145) taken while the combat team is in place: is CB's buffer inside the snapshot's span. | The buffer must be image state or a snapshot participant, or a quick-load mid-battle loses the map team. |
| M6 | The character ids: which id is which character, which ids the story uses when, and how the game records "joined" (a member in the lists or the reserve list `0x6BDFD4`, a `CharacterRecords` byte, a flag). | "Champion present" (section 3) is a test over this; the launcher's list of champions is this table's names. |
| M7 | The "must stay" mark (`CharacterRecords +0xB` bit 1, bit 0): every writer, every clearer, and whether it survives a save. | FS keeps a marked member walking (the owner's answer 4); a mark never cleared would keep them for good. |
| M8 | How to tell "a script changed the party" from the outside: the writers of the lists under `0x531F90` (393 call sites) and the 98 `Scenario_CallA` entries, and the first frame control returns to the player. | FS folds the map team back to the walker at that frame, not inside the script. |
| M9 | What `Save_BuildBlock` writes of the party (the lists, the reserve, the ObjTrio records or only ids) and what a load rebuilds from it. | Section 6, open 2: what a save made in the mode holds. |

M's hand-back is the findings doc and one line per question on what the
groups below must change in their brief. A measurement that contradicts a
premise here is a correction box at the top of the section it corrects, as
[`quicksave-round.md`](quicksave-round.md) 1 carries SC's.

## 3. The rules of the mode (the owner's answers applied)

- **Champion present** means the champion has joined and the story has not
  taken them away: the champion is in a party list or the reserve (M6 gives
  the exact test).
- **Champion present: the champion fights alone**, whatever the map team is,
  a script-forced party included.
- **Champion not present: the mode stands down whole.** The party is the
  game's own, map and combat, as with the mode off: the three-member party,
  the original L swap and members screen. *(The owner's answer was "the
  game's own party" fights; that the map half also stands down is this doc's
  reading, since a combat party of three cannot be drawn from a map team of
  one. Section 6, open 3.)* When the champion joins or returns, the mode
  takes over at the first frame of player control (M8's frame).
- **Champion always** (a second switch, off by default, the owner's "optional
  flag"): the champion fights every battle from the moment they have first
  joined, also while the story has them away. Built after the base rule
  works, by CB, only if M2 and M6 show the champion's record can be fought
  from while they are away; otherwise it is reported, not stubbed (rule 4).
- **Must-stay members walk too.** A member the game marks "must stay" stays
  on the map team beside the walker while the mark is set; they fight only if
  they are the champion.
- **The champion is chosen in the launcher** for now. The New Game screen
  waits for I34's rebuild of that sequence (section 8).

## 4. Groups

Agents are the `worker` type, each in its own worktree under
`D:\bof3x-runs\solo1112\<group>\wt` reset onto the round branch's tip, harness
root `D:\bof3x-runs\solo1112\<group>\root`, a clean launcher directory for
self-tests, `git commit -s`, headless self-tests only, a hand-back report.
Agents number nothing in the ledger or `known-defects.md`.

### Stage A (after M; SC alone, LA beside it)

**SC, the core** (`src/game/solo/solo.{h,cpp}`, new; merges first).
- The behaviour `SoloCharacter` and the champion's id from `BOF3X_SOLO`
  (unset or empty: no champion, the behaviour does nothing and says so in the
  log; a value that names no character stops the game with a sentence naming
  it, as an unknown `BOF3X_ON` name does). The second behaviour
  `SoloChampionAlways`.
- The questions the other groups ask, as functions with no side effect:
  `Solo_Active()` (the behaviour on, a champion set, the champion present),
  `Solo_Champion()`, `Solo_ChampionPresent()` (M6's test),
  `Solo_MustStay(id)` (M7's mark).
- A self-test of the decision table on state of its own (no game state), in
  the pattern of `Behaviour_SelfTest`.
- The ledger entry's draft (lettered A), `Default: off`.

**LA, the launcher** (`src/launcher/` only).
- Ini keys and what they become: the mode's check box adds `SoloCharacter` to
  `on=`; `solo.champion=<name>` becomes `BOF3X_SOLO`; a second check box for
  `SoloChampionAlways`, shown greyed while the mode is off (the config shows
  every option, greyed by source or state).
- The page text in ASD-STE100, drafts for the owner in the hand-back. The
  champion list's names are the launcher's own labels in the launcher's
  language, not strings read from the game.
- `tools/ledger_registry.py` and the presets regenerate after the
  coordinator numbers the entry; LA leaves a note of what to run.

### Stage B (after SC merges; four groups in parallel)

**CB, the combat half** (`src/game/solo/solo_battle.cpp`, new;
`src/game/core/transition.cpp`; the battle-end sites M3 names).
- *Before the battle loads*: when `Solo_Active()` and the champion is not
  already the only member, save the three ObjTrio records, both lists, the
  member count and the party set byte to the buffer (M5 says where it lives);
  build the champion into record 0 as `Field_PartyLoad` does; lists and count
  to one member. Not `0x939AE0`, not `0x904B8F`, not round flag bit 15: those
  are the dragon takeover's and a champion who takes a form uses them.
- *The party set*: none loaded when the current set holds the champion
  (FS keeps it so, M1). If it does not (a script's set), the load the game's
  own path makes, waited out as `PartySet_Load` waits.
- *After the battle has fully ended*, on every way out of M3, after
  `BattleFx_RestoreParty`: the champion's results to `CharacterRecords`
  (M2's fields), then the map team's records, lists, count and set back, and
  what `BattleFx_RestoreParty` and its slot-17 tasks rebuild for a member
  (windows, palette, tint, the spot on the ground) rebuilt for the map team.
- A game over or a load from inside a battle must not leave the buffer
  "armed": the buffer is cleared wherever the game's own state is replaced.
- `SoloChampionAlways`, last, on the terms of section 3.
- Checks that mean done: with the behaviour off, the battle routes' state
  hashes equal the reference; with it on, a recorded battle route won by the
  champion alone with a different walker, the walker back on the field after
  it with the pre-battle records (a peek of the three records before and
  after, equal but for nothing); a flee; a game over; a dragon form taken and
  reverted by a champion; a quicksave and quick-load mid-battle (M5).

**FS, the field half** (`src/game/solo/solo_field.cpp`, new;
`src/game/field/field_4.cpp`, `field_6.cpp`).
- *The L swap with one walker*: `Field_LeaderSwapTest` passes with one member
  when `Solo_Active()` and another character has joined; the swap replaces
  the walker with the next joined character in a fixed order
  (`Field_SwapExchange`'s reload: the sprite, the actor record from
  `Character_IndexById`, the list byte), with the party set chosen to hold
  the champion as well (M1). The cost of a set load mid-walk is measured and
  reported in frames; if the game's own swap states cannot wait it out, FS
  reports that instead of hiding it.
- *The fold*: at M8's frame after a script changed the party, the map team
  becomes the walker plus the must-stay members, the rest to the reserve,
  both lists kept holding the same members as every original writer keeps
  them.
- *Must-stay members* follow as members 1 and 2 do today.
- *Entering and leaving the mode* mid-game (the champion joins, returns, or
  is taken away): the same fold, or the game's own party left alone.
- Checks: off, the field routes' state hashes equal; on, a route that swaps
  through every joined character and enters a door, a scripted scene that
  builds a party and returns control, an area that wants a given leader.

**MS, the members screen** (`src/game/field/field_ability_2.cpp` and the
screen's files in [`rest_2f.md`](rest_2f.md); no other group's files).
- Five reserve rows. The two latent defects of `rest_2f.md` 7 (items 2 and 3:
  the reserve flags `0x6BDFCD + 2j` landing on the reserve list from the
  fifth member; rows past 2 reading their places past `0x6674C8`'s six
  entries) are answered first: tables of ours with room, the layout drawn for
  five. Whether the two fixes are unconditional or the mode's is section 6,
  open 2; MS builds them so either answer is a one-line change.
- In the mode the screen moves the walker, not the fighter; the champion is
  marked in the list (how is a capture for the owner, their layout words
  applied literally).
- Checks: off, `menu_screens` and the members routes equal; on, captures of
  the screen at one walker with one to five in reserve.

**RT, the routes and the proof batch** (`tools/recipes/`, `tools/`; no
`src/`).
- Solo variants of the battle routes (`combat`, `combatGroup`, `combat_exp`,
  `dragonTransform`, `gameover`, a quick-combat route) and a field route for
  the swap; a route that needs a recording is listed for the owner
  (`BOF3X_RECORD`), not invented.
- The boss sweep's pre-fight saves ([`boss-sweep.md`](boss-sweep.md) 6) run
  with a team of one: a harness mode that sets `BOF3X_SOLO` per character and
  reports, per fight, started / ended / hung / crashed. A fight that needs
  its members is a finding for CB or a line in the entry's "what stays the
  original's", not a silent skip.
- The off-proof script: the ten state-hash routes with the behaviour off
  against fresh references recorded at the round's base (the `orig4` pairs
  are stale, HANDOFF item 14b), on a quiet, hands-off machine.

## 5. Stage C: the proof, then the owner's play

The coordinator, unattended: merge order SC, LA, then CB, FS, MS in that
order, RT any time; after each merge the build, `gen_symbols.py`,
`ledger_check.py`, self-test `'*'`. Then RT's batch: the off-proof (0 routes
differ), the solo routes, the boss sweep with a team of one. Then captures
for the owner by chat (the members screen with five in reserve, the launcher
page), and the owner's play: a random battle with a walker who is not the
champion, an L swap through everyone, a boss, a scripted party, a save and a
load, the mode turned off on that save.

Numbering by the coordinator at the merges: the entry from the first free
DIV (the one after DIV-0227 at staging; check `main`), the members screen's defects from
the first free D (`D417` at staging; `D416` is parked).

## 6. The owner's calls

Answered 2026-11-12 (by the question box, the owner's words in quotes):

1. **A script-forced party: who fights?** "Lets start it as 'champion if
   present', but maybe leave an optional flag for 'champion always mode'."
   Section 3's first rule and `SoloChampionAlways`.
2. **Before the champion has joined, or while the story has them away:** "The
   game's own party."
3. **Where the champion is chosen:** a launcher setting now; the New Game
   screen later with I34.
4. **A "must stay" member:** they walk too, beside the walker, and fight only
   if they are the champion.

Open (none blocks M or stage A):

1. **EXP** (held over by the owner on 2026-11-03 until the combat half is
   settled). As built by this design the champion takes the whole total, as a
   party does today when the other two are out; whether the reserve should
   get any follows M2's read of what the original gives a reserve member.
2. **A save made in the mode.** It holds one walker and up to five in
   reserve, which the original members screen mishandles (the two latent
   defects) when the mode is then turned off, and which a save converted to a
   PlayStation card would carry to real hardware. Two ways: (a) the members
   screen's fixes are unconditional and the save holds the party as it is;
   (b) the save block is written with the game's own shape (the map team
   filled to three from the reserve, in the block only) and folded back on
   load in the mode. Recommended: (b) if M9 shows the block holds ids and not
   records, else (a). The owner's word after M9.
3. **The mode standing down whole** when the champion is not present
   (section 3; this doc's reading of answer 2).
4. **The champion marked on the members screen**, and the launcher page's
   text: captures and drafts at stage B's hand-back.

## 7. Not in this round

- The champion screen at New Game (I34's sequence).
- A mode recorded in the save, or a title-screen badge for a solo save.
- A combat team of two, or a champion per chapter.
- Rebalancing (enemy stats, drops) for a party of one.
- The PlayStation side: the mode is this port's; no disc-patch form of it.

## 8. The brief, per group

Each brief names: the goal; the files it owns and must not touch; the docs to
read first (IDEAS I56, this doc, `docs/solo-character-reads.md` once M has
it, [`rest_2f.md`](rest_2f.md) 2 and 7, [`battle_setup.md`](battle_setup.md),
[`battle_e1.md`](battle_e1.md) for the ways out, [`field-event.md`](field-event.md)
for the party-set loader, [`scena_calls.md`](scena_calls.md),
[`state-hash.md`](state-hash.md) 6, [`mods.md`](mods.md) 11 for a behaviour,
[`quicksave-round.md`](quicksave-round.md) for the snapshot's span); its
worktree and harness root; the checks that mean done; the hand-back report's
shape (what is saved, built and restored and where; what was measured; the
drafts lettered; captures' paths outside the tree). Rules 1, 2, 4 and 7
restated; game facts come from a measurement or the owner, never from what a
model remembers of the game. A usage cut resumes with a message naming the
next step; only the hand-back report means merge.

## 9. Where things are

- This doc and IDEAS I56 on `docs/solo-character-round` (2026-11-12).
- The round branch, when launched: `solo/round`, off `main` after this doc
  merges.
- Scratch: `D:\bof3x-runs\solo1112` (nothing there yet).
