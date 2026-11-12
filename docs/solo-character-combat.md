# The solo-character round: group CB, the combat half

**Status:** IN PROGRESS (2026-11-12, branch `solo/cb` off `solo/round`
`10646687`). Built and self-tested; the battle routes run in the mode are
section 6. Not played by the owner. The design is
[`solo-character-round.md`](solo-character-round.md) 0a items 1-5, 4 "CB" and
6a item 2 ("champion always" first); the reads it rests on are
[`solo-character-reads.md`](solo-character-reads.md) M2..M5. Ledger text for
DIV-0228's amendment is in the hand-back, not in the ledger.

"Read" below means the function's body in `src/` was opened (ours are
faithful replacements of Capcom's). An inference is marked as one.

## 1. What is built

`src/game/solo/solo_battle.{h,cpp}` (new) and four calls in
`src/game/core/transition.cpp`. Every call is inert with `SoloCharacter` off:
the first asks `solo::Solo_ChampionFights()`, which with the switch off
answers no and reads nothing of the game; the others act only on a save the
first one made.

| Where | Call | What |
|---|---|---|
| `GameMode_Start` `0x495840`, first | `SoloBattle_Drop` | a save that never reached step 6 (a loss restarts the boot task, M3) is dropped, one log line |
| `GameMode5_Turn` `0x495EC0` (step 1), after the party has turned and the approach, before the step's `PartySet_Select` | `SoloBattle_Arm(0x904AE5)` | when the champion fights and the second list is not already the champion alone: the second list `0x904065..67`, `Field_MemberCount` `0x929EC0`, the formation `0x904060` and the party set `0x90412C` saved; the set changed to a row holding the champion when the current one does not (section 2) |
| `GameMode5_Load` `0x495FD0` (step 3), before `Field_PartyLoad(1)` | `SoloBattle_WriteParty` | the second list `(champion's id, 0xFF, 0xFF)`, the count 1, the formation 0 |
| `GameMode5_Place` `0x496130` (step 6), on entry | `SoloBattle_Restore` | each saved byte back where the live byte still holds what was written; a changed set's field file loaded again; the leader steer below |
| `TransitionFromModeRest_Inject` | `SoloBattle_Inject` | the self-test (exe host `BOF3X_SELFTEST_ONLY`, hosted `BOF3X_SHADOW=solo` or `*`) and the quicksave participant `SOLOCB` |

The first list `0x904062` is never written by the mode (as chapter 6's
`SortParty` leaves it). Not used: the dragon takeover's storage (`0x939AE0`,
`0x904B8F`, round flag bit 15) and chapter 6's (`0x939A02`, `0x939A10`).

## 2. Why the write is at step 3 and not step 1

The round doc put the whole write before step 1. Read, step 2 rules that
out: `Encounter_PartyToPlaces` `0x532860` (`src/game/field/field_6.cpp`)
walks each of `Field_MemberCount`'s walkers to the spot of **its id's place
in the second list**, and an id not found there goes to the fourth spot
(`while (k < 3 && Cond_Flags[0xd5 + k] != id) ++k;` then `PosX(k)`).
Written at step 1, the count would be 1 and the walker leading ObjTrio's
first record - absent from a list of the champion alone - would be moved to
the fourth entry of `0x7E06E0`, which `Encounter_PlaceMember` never writes
for a party of three (a stale spot), until step 3's `Field_PartyLoad(1)`
replaces the record with the champion and `Encounter_PartyAtPlaces` puts it
at spot 0. Chapter 6 never meets this: `SortParty` sorts ObjTrio by id first,
so the one id left in its list is ObjTrio's first.

So step 1 saves and changes only the set (the step's select must load the
champion's row), and step 3 writes the list, the count and the formation
just before `Field_PartyLoad(1)` reads them. Steps 1 and 2 only read those
bytes (`GameMode5_Turn`, `GameMode5_ToPlaces` and their `Encounter_` callees,
read); `SoloBattle_WriteParty` checks they still hold step 1's values and
stops the game with a sentence if not. The formation `0x904060` is read
between them by nothing (its readers are `Encounter_PlaceParty`, before mode
5, and `Party_PlacesByList`, step 7). Seen from the screen (inference): the
other walkers fade to their spots as in the original and vanish at step 3,
when `Field_PartyLoad(1)`'s `Party_ClearAll` turns their records off.

## 3. The party set (round doc M1, verified)

- **`PartySet_Select` `0x536AC0` takes a row, not a list**
  (`src/game/field/mode_11.cpp`): it records the row in `0x90412C` (bit 7
  for mode 0, untouched for mode 3) and loads `row + 0x2C2 / 0xFC / 0x10F /
  0x2D5` for mode 0..3. The one-id question is `PartySet_Find` `0x5368F0`'s:
  with `(id, 0xFF, 0xFF)` it keeps the current row when that holds the id,
  else counts one id, sorts it first and returns the first row whose cell
  matches, column by column. Every champion id (0, 1, 2, 4, 5, 6, 7, 8, and
  record 0's 9) is in some row (M1's table), so it never reaches
  `PartySet_ErrorLoop`.
- **Why the row must hold the champion:** `Field_MemberSprite` `0x533BA0`
  (its `symbols.toml` evidence; `Field_PartyLoad` calls it per member) sets
  a member's `+0x2C` to its column in the current row, 3 if absent, and the
  sprite bank pointer from it. A champion outside the row would be drawn from
  the fourth bank entry.
- **The battle file replaces the field file**: the battle's end reloads the
  current row's mode-0 file (`BattleEnd_AwaitRestore` `0x4315C0`,
  `BattleResult_SplitExp` `0x431940`, both `PartySet_Select(set, 0)`). So:
  - Arm, when the current row lacks the champion, writes the row
    `PartySet_Find(id, 0xFF, 0xFF)` gives into `0x90412C` (no bit 7, as the
    step's mode 1 / 2 select records it); the step's own select then loads
    that row's battle file and step 2 waits for it (`File_LoadDone`).
  - An event battle with `0x904AE5` bit 4 skips that select, and so draws
    from whatever set file is loaded - by the reload above, the field's.
    Arm then loads the new row's field file itself, `PartySet_Select(row,
    0)`, waited out as `PartySet_Load` waits (`File_LoadDone`, a frame's
    `Task_Sleep`). This branch is read, not run: no route reaches an event
    battle with bit 4 and a set lacking the champion. (The inference that
    such a battle draws from the field file is from the reload, not from a
    file read.)
  - Restore, when it puts the set back and the row differs, loads the saved
    row's field file the same way, `PartySet_Select(row, 0)` and the wait,
    then writes the saved byte whole. The mode-3 file (`row + 0x2D5`) is
    never touched: no battle path loads it, so the field's stays.
- On the PC `File_LoadDone` answers 1 at once (`LoadMenu_Apply`'s comment,
  `src/game/core/sound.cpp`), so the waits cost no frame there.

## 4. Nothing to write back: 0a item 3 verified

Read, for a champion who is not on the first list:

- `Field_PartyLoad(1)` (`field_6.cpp`) builds member 0 from the **second**
  list's id: `+0x148 = Character_IndexById[id]`, the record copied whole to
  `+0x80` (so `+0x89` is the record's own `+9`). The first list is not read.
- HP, AP, `+0x1C`, status: `Battle_WriteBackMember` `0x446A80` writes to
  `CharRecord(member->char_index)`, the `+0x148` above
  (`src/game/battle/battle_3.cpp:2134-2147`).
- EXP: `BattleResult_AddExp` `0x4468B0` credits `CharId_ToRosterIndex(slot
  +0x89)` (`battle_3.cpp:2055-2079`): `Character_IndexById` of the
  champion's id, the champion's record (7 would map to 0; record 7 is never
  a champion).
- Levels: `BattleResult_Setup` `0x431D70` levels
  `roster_index(member +0x89)`, the same mapping
  (`src/game/battle/battle_result.cpp`, its loop over the party count).
- The fallen: `BattleEnd_Finish` by `+0x148`.

So every result reaches the champion's record whatever the first list holds.
The run logs the record at both ends (section 6).

## 5. The leader after the battle

`Party_PlaceAtSlots` `0x532C10` (step 6, `field_6.cpp`) places the restored
count's ObjTrio records at the battle spots and looks for the leader as the
record whose `+0x89` is the first list's first id; with none it reads the
record after the last (past ObjTrio for a count of 3), and sets the camera's
point `Field_Kind2X / Z` from it. In a battle of one, ObjTrio's first record
is the champion and the others are the walkers' records `Field_PartyLoad(1)`
turned off, so a walker leader other than the champion is not found and the
camera point would come from a stale record. When that would happen,
`GameMode5_Place` holds the first list's first byte at the champion's id for
that one call and puts it back after: the champion stood at the first battle
spot, where a three-member battle's leader stands. Step 7's
`Field_PartyLoad(0)` then rebuilds the walkers from the untouched first list
and `Party_PlacesByList` puts each at the spot of its place in the (restored)
second list. With a party of three and the champion in it, a record of the
leader's id may still be among the turned-off records and is found first;
the steer then does not apply.

## 6. Runs

The i686 build of `solo/cb`, the exe host, launched from a launcher copy
with no ini (`D:\bof3x-runs\solo1112\CB\root\L`) on a game copy whose
`DAT`, `BGM` and `SND` are junctions (`...\CB\root\game`), through
`tools/input_run.py --no-front --slot0-shared --lang en-US`, with
`BOF3X_ON=SoloCharacter BOF3X_SOLO=<name>`. Logs and frames in
`D:\bof3x-runs\solo1112\CB\root\runs\<run>\`. Other agents were building
on the machine.

The recipe save `combat` (chapter 1): first list `00 04 03`, second list
`00 03 04`, count 3, formation 0, set `0x85` (row 5, ids 0, 3, 4); records
0, 3, 4 joined, records 1, 2, 5, 6 not (their New Game defaults). The save
`adult_ryu` (chapter 14): both lists `07 FF FF`.

| Run | Recipe, champion | Arm | Restore | Route |
|---|---|---|---|---|
| self-test, exe host | `BOF3X_SELFTEST_ONLY=1` | - | - | `solo self-test: 48 checks passed`, `solo battle self-test: 21 checks passed`, `participant 'SOLOCB' registered` |
| self-test, hosted | `BOF3X_SHADOW=*` | - | - | the same three lines; no `MISMATCH` line other than "0 MISMATCHES" |
| A | combat_exp, rei (joined, not the leader) | battle 4: `00 03 04` count 3 set `85` saved, set kept | mask `0x1F`: `00 03 04` count 3 formation 0 set `85`, first list `00 04 03` | to its end (frame 3764). Rei alone on screen (`f00600.png`); the three walkers back on the field after (`f01560.png`). Record 4 EXP 90 -> 99 |
| B | combat_exp, garr (**never joined**, the gate) | the set changed: row 5 lacks id 2, `PartySet_Find` gave row 0; battle file `BPLD247` | mask `0x3F`; the battle's end loaded `PL247`, the restore `PL034` (row 5's field file); set `85` | to its end. Garr alone, from the default record (`f00600.png`); the walkers back with their own sprites (`f00900.png`). Record 2 EXP 3000 -> 3009, AP 7 -> 6, joined still 0. The battle ended sooner (frame 748), so the route ran desynced from there |
| C | gameover, rei | battle 4 armed | none (the loss) | to its end at the title. No `GameMode_Start` follows in the route, so the drop's log line is not seen |
| D | quick_combat, rei (F6 at 901, F7 at 1201, mid-battle) | battle 4 armed | after the quick-load, mask `0x1F`, the party as before | three runs: two to their end (frame 2920, `Frame_Counter` 2620), **one crashed** after the restore line, at mode 5 step 7, in `nvwgf2um.dll` (the NVIDIA driver) reading `0xB545B545`. The same route with the mode off ran to its end once. Read as noise (two clean repeats, the fault in the driver, the machine loaded), not settled |
| E | dragonTransform, ryu | `already has the champion (id 0x07) alone; nothing to change` | - | to its end: the save's party is Ryu alone, so the mode changes nothing; the dragon form is the original's run |
| F | combat, garr with `SoloChampionIfPresent` | no arm (Garr not joined: the game's own party fights) | - | to its end, in step with the recording |
| G | combat_exp, ryu (the leader) | battle 4 armed | not reached | desynced: Ryu alone at level 1 is slower, and the recipe ends at a command menu in the battle (`exp7.png`) |

Not run, for want of a route: a **flee** (no recorded route flees outside
the intro, where the mode stands down); a **dragon form taken and reverted
by a champion who is not already alone**; **chapter 6's own one-member
fight** (set-up 25) with the mode on; an **event battle with `0x904AE5` bit
4** whose set lacks the champion; the **drop at `GameMode_Start`** after a
loss (needs a route that loads or starts a game after the game over). For
the owner to record with `BOF3X_RECORD`: a random battle that flees; a fight
where Ryu transforms with a party of three (champion Ryu); the chapter 6
boss with a champion other than Ryu; a game over followed by a load.

## 7. The gate: a fight from a record whose joined bit is clear

The default records (`0x64B390 + 0xA4 k`, read from `BOF3.exe` 2026-11-12;
`NewGame_InitCharacters` copies them into records 0..6) hold a whole
character: records 1, 2, 4, 5, 6 have `+0xB` 0, a level (5, 13, 5, 10, 1),
EXP in that level's window, HP and AP equal to their maximums (27 / 33, 99 /
7, 42 / 12, 52 / 40, 40 / 8), status 0, weapons and armour (record 6's
weapon and armour 0), no master, the base and effective stat blocks, and
non-zero bytes in `+0x60..` for records 0 to 5 (the skill lists by
`solo-character-level1.md`; not read again here). Each id is in some
party-set row (M1). So a champion the story has not introduced can be fought
from, and run B did: Garr's default record fought and won, took the EXP, and
stayed "not joined".

A record the story took away holds whatever the story left. The one state
found that cannot be fought from is **HP 0**: chapter 8's `Scena08_Scene11`
gives the record of id 4 HP 0 and status `0x6000`
([`save-block/range-b-characters.md`](save-block/range-b-characters.md) 4),
and a battle whose only member is down would open lost (inference). The
stated rule: **with the champion's record at 0 HP, that battle is fought by
the game's own party**, with one log line. Not run (no route reaches it).

## 8. What is not done

- The routes of section 6 that need a recording.
- The off-proof (the ten state-hash routes), the coordinator's on a quiet
  machine. Reading: with the switch off `SoloBattle_Arm` returns at
  `Solo_ChampionFights()`, which reads nothing then; the other three calls
  return at the module's own flag, which only Arm sets (a quicksave record
  can set it too, and only one taken in a battle of one).
- The 64-bit build.
- The owner's play.
