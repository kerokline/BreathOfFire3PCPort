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

`src/game/solo/solo_battle.{h,cpp}` (new) and six calls in
`src/game/core/transition.cpp`. Every call is inert with `SoloCharacter` off:
the first asks `solo::Solo_ChampionFights()`, which with the switch off
answers no and reads nothing of the game; the others act only on a save the
first one made.

| Where | Call | What |
|---|---|---|
| `GameMode_Start` `0x495840`, first | `SoloBattle_Drop` | a save that never reached step 6 (a loss restarts the boot task, M3) is dropped, one log line |
| `GameMode5_Turn` `0x495EC0` (step 1), after the party has turned and the approach, before the step's `PartySet_Select` | `SoloBattle_Arm(0x904AE5)` | when the champion fights and the second list is not already the champion alone: the second list `0x904065..67`, `Field_MemberCount` `0x929EC0`, the formation `0x904060` and the party set `0x90412C` saved; the set changed to a row holding the champion when the current one does not (section 2) |
| `GameMode5_Load` `0x495FD0` (step 3), before `Field_PartyLoad(1)` | `SoloBattle_WriteParty` | the second list `(champion's id, 0xFF, 0xFF)`, the count 1, the formation 0 |
| `GameMode5_Place` `0x496130` (step 6), on entry | `SoloBattle_Restore` | each saved byte but the set back where the live byte still holds what was written; the set decided, not yet written; the leader steer below |
| `GameMode5_Place`, after `Party_PlaceAtSlots` | `SoloBattle_AfterPlaceAtSlots` | the member count held at 1 till step 7 when it stands above 1 (section 9) |
| `GameMode5_Leave` `0x496150` (step 7), in its branch, before `Field_PartyLoad(0)` | `SoloBattle_RestoreAtStep7` | the held count back; the set back, its field file loaded again when the row changed (section 9) |
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
    `ChooseRow` picks into `0x90412C` (section 11: among the rows holding the
    champion, the one keeping the most of the current row's ids; first built
    as `PartySet_Find(id, 0xFF, 0xFF)`'s first match) (no bit 7, as the
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
  - When the set goes back and the row differs, the saved row's field file
    is loaded the same way, `PartySet_Select(row, 0)` and the wait, then the
    saved byte is written whole - at step 7, right before
    `Field_PartyLoad(0)` (section 9; first built at step 6, which crashed). The mode-3 file (`row + 0x2D5`) is
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

## 9. The crash after a battle whose party set changed (2026-11-13)

**Symptom** (the coordinator's merged-round runs, then reproduced here). On
the `combat` save with champion Garr (set row 5 changed to row 0 for the
battle, restore mask `0x3F`), intermittently, a few frames after the line
"battle over, the party put back": an access violation in `d3d11.dll`
(`+0x7514F`, reading `0x8842884A`, `eax 88428842`) or `nvwgf2um.dll`, or
`Present failed, HRESULT 0x887A0005` (DXGI_ERROR_DEVICE_REMOVED). The two
bof3x.dll frames on the d3d11 crash's stack map (`llvm-nm` of the round's
dll) into `render::SweepReleased` (`0x10258100`, the loop that releases a
released surface's GPU objects, `src/render/render_d3d11.cpp`) and the
render fiber: it released a `GpuTexture` whose bytes read as 16-bit pixel
words. **Every** Garr run, crashed or not, logs 30 `DIV-0142 capture ... a
frame of no pieces (anim 5, frame +0xA0FD / +0xDAFF)` lines from about
eight frames after the restore: two party objects drawn from frame records
that are not frames. The coordinator's two crash logs carry the same 30.

**Rates before the fix** (`combat_exp`, exe host, `--speed 4`, 8 runs
each, `scratch/loop.sh`): champion Garr 1 failed (the device removed, 3
frames after the restore), 7 to their end, **30 garbage frames in all 7**;
champion Rei (set kept, mask `0x1F`) 1 crashed (`nvwgf2um.dll+0x3A76B5`,
mode 5 step 7, 11 frames after the restore), 0 garbage frames; mode off 0 of
8, 0 garbage frames. So it needs the mode, and not only the set change.

**Cause** (read, and measured by a diagnostic build that logged ObjTrio's
second and third records at steps 1, 3 and 6). From step 6 on, the count was
back at 3 while ObjTrio's second and third records were still the walkers'
records from before the battle: `Field_PartyLoad(1)` built only the
champion and cleared the others' flags (`Party_ClearAll`), and between step
3 and step 6 nothing else in them changed (the diagnostic: two bytes, the
flags). `Party_ScriptTicks` (`GameMode5_Place`, and every frame of step 7
until its branch runs) ticks the count's records, and the members' draw
(`Party_UpdateScreens` `0x531B60`, `field_6.cpp`) skips only bit 0x40, not a
cleared bit 0 - so both stale records were ticked and drawn for the frames
up to `Field_PartyLoad(0)`. Their sprite pointers (`+0x4C` animation data,
`0x835628` / `0x825D58`, and the script and frame pointers from it) hold
offsets into the set file loaded before the battle. The battle replaced that
file and the battle's end loaded the battle row's field file at the same
place (`BattleEnd_AwaitRestore` / `BattleResult_SplitExp`, `PartySet_Select(set,
0)`): for Garr that is row 0's, not row 5's, so the offsets land on other
characters' data - the garbage frames, and through
`Sprite_SetFrameQueueUpload` garbage upload records for
`Gfx_FlushUploadQueue`, whose rectangles are not bounded (known defect D4's
family; Capcom's code kept as it is). **That the heap damage comes through
those uploads is an inference** from the pixel-word bytes in the freed
`GpuTexture` and from the garbage frames always preceding the crash; the
dump was not walked to the writer. The original never meets it: every member
it ticks after a battle was rebuilt for that battle (`Field_PartyLoad(1)`
with the full count), and chapter 6's own one-member fight keeps the set, so
its stale records still match the file (the Rei case: same file, stale but
valid pointers; why one Rei run in eight crashed is not settled beyond the
same stale-record window).

A first fix, the set's file reload moved from step 6 to step 7, left all 30
garbage frames: the reload at step 6 was not the cause.

**Fix.** After `Party_PlaceAtSlots` (which needs the full count only to give
the walkers' records their x and z at the battle spots), the count is held
at 1 - only the champion, the one record the battle built, is ticked and
drawn - and at step 7, in `GameMode5_Leave`'s branch right before
`Field_PartyLoad(0)`, the count goes back and a changed set goes back with
its field file loaded (no frame between the load and the rebuild, as
`PartyForm_Reload` does: `PartySet_Load`, then `Field_PartyLoad(0)`). The
hold applies whenever the count stands above 1 after the restore, also when
an exit hook put back its own count (`Boss25_Exit`'s 3): those records were
not built for this battle either. Both are carried by the quicksave record
(`SOLOCB` version 2).

**Rates after the fix** (the same loop, the same build settings): champion
Garr 0 of 9 failed (one probe run plus eight), **0 garbage frames in all
nine**; champion Rei 0 of 8; `quick_combat` with Garr (the coordinator's
second crash route, F6 / F7 mid-battle) 0 of 4, 0 garbage frames. The
walkers come back with their own sprites (`runs/fix_garr_3/f00780.png`).
Logs: `D:\bof3x-runs\solo1112\CB\root\runs\` (`pre_*`, `fix_*`, `fixq_*`,
oot
uns\` (`pre_*`, `fix_*`, `fixq_*`,
`diag_*`; `loop_summary.txt` one line a run). Eight runs bound a rate of
about one in eight only loosely; the garbage-frame count, 30 in every
pre-fix Garr run and 0 after, is the deterministic half of the evidence.

## 10. The Beyd fights stay the original's (the owner, round doc 6g)

The owner: "the beyd fight is a minigame, not a true boss - I would turn off
the logic for those 2 fights (the beyd fight, and the beyd vs zigg fight)".
**The code has three Beyd fights**, one sequence in chapter 6, area 92.
`Boss_SetupTable` `0x656954` is indexed by the event-battle byte `0x904AAA`
(its `symbols.toml` evidence), so fight *n*'s set-up is `Boss<n>_Setup`
(`src/game/boss/boss_25.cpp`):

| Fight | Started at | Party | What the code makes of it |
|---|---|---|---|
| 25 (`0x19`) | `Scena06_Run08` step 2 (`src/game/scenario/scena_06_3.cpp`, `Battle(0x19)`), after step 0's `SortParty` | **record 0 alone**: `SortParty` writes the second list `(0, 0xFF, 0xFF)`, id 0 = record 0 in chapter 6 | `Boss25_Setup` saves member 0's HP / AP and fills them; `Boss25_Event` scripts member 0's actions by turn (an action 4 with skill `0x46`, at last enemy 0's HP 1); `Boss25_End` puts the HP / AP back; `Boss25_Exit` restores the count 3 and the list. Enemy: Beyd (kind 30; `BossBeyd_Death` says message `0x20` in fight `0x19` only) |
| 26 (`0x1A`) | the same run, step `0xC` (`Battle(0x1A)`) | the three `Boss25_Exit` restored | `BossBeyd_Enter`, in fight `0x1A` only, rebuilds Beyd from tag `0x59` with HP from `0x903F0C..` (the writer not read); `Boss26_Event` sums the blows enemy 0 deals the members and the blows it takes (or counts them while enemy 0's `+0x110` bit 1 is set) and ends the fight at turn `0x15` |
| 27 (`0x1B`) | `Scena06_Run09` step 5 (`Battle(0x1B)`) | the field's party | Beyd against Zig: `BossBeyd2_HookTarget4` aims Beyd at slot 4 (enemy 1), `BossZig_HookPick` aims Zig at 3 (enemy 0), and enemy 1's animation `0xC` in `Boss27_Event` is Zig's (`BossZig_Idle`); `Boss27_Event` ends the fight when enemy 0 or enemy 1 falls, **or when a party member takes a turn**; `Boss27_End` picks the run's next step (`0xF`, 7 or `0x14`) by which |

So "the Beyd vs Zig fight" is 27, and "the Beyd fight" is the bout of run
8, which is **two** battles, 25 and 26 (one run, one area, the second
started three steps after the first). Built: **all three stand down**, one
line each in `kStandDowns` (`src/game/solo/solo_battle.cpp`) with the
reason beside it; if the owner meant only one of 25 and 26, removing a line
is the whole change. That 26 is part of the minigame is this doc's reading
of the code, not the owner's word.

**Built.** (The table is `kFightRules` since section 11; these three are
its `kStandDown` rows.) `SoloBattle_Arm` reads `EventBattle_Id` (`0x904AAA`, set by
`Field_StartEventBattle` before mode 5, 0 for a random encounter, read at
step 1) and, for a fight in `kStandDowns`, logs one line naming it and arms
nothing. Every later hook acts only on what Arm saved: `SoloBattle_WriteParty`
on `g_save.armed`, `SoloBattle_Restore` on `g_save.armed`,
`SoloBattle_AfterPlaceAtSlots` and `SoloBattle_RestoreAtStep7` on the step-7
record only `Restore` fills, `SoloBattle_Drop` on either. So chapter 6's own
one-member party (`SortParty`, `Boss25_Exit`) is never met by a swap of ours.

**Run** (the `combat` save, champion Garr, the fight staged with the input
script's `battle N any`, `runs/beyd_0x19` .. `beyd_0x1B`): each logs, at
battle 4, `DIV-0228 SoloCharacter: battle 4 is event battle 0x19, chapter
6's Beyd bout, part one ...: fought by the game's own party (the owner,
round doc 6g)` (and the same for `0x1A`, `0x1B`), and no arm line; the
recipes ran to their end. These are staged, empty fights in area 29, not
the real scenes: the real fights want a chapter 6 save at area 92 before run
8 (fight 25), before run 8 step `0xC` (26) and before run 9 step 5 (27).

**The real scene of fight 25** (2026-11-13, `tools/solo_boss_sweep.py`, RT's
craft `bfs25` from `preBeyd`, chapter 6 run 8 step 0, area 92; root
`D:\bof3x-runs\solo1112\CB\sweep`): champions Garr and Rei both log
`battle 260 is event battle 0x19, chapter 6's Beyd bout, part one ...:
fought by the game's own party` and arm nothing. Both runs end "started,
hung (no return to the field)" - as RT's **plain** runs of the same craft
do (`RT\root\cx\boss_results.md`): the scripted bout waits for inputs the
harness does not give. Same result as plain.

## 11. Set-ups that read the party by id, by slot or from the set (2026-11-13)

**The defect** (group RT's find, reproduced here): fight 16 crashed every
time with champion Rei, Momo or Peco - `0xC0000005 at bof3x.dll+0x10D613`,
mode 5 step 5, in `Sprite_LoadPalette` under `Field_MemberSprite` under
`Boss16_Event`. `Boss16_Event` code 2 (`src/game/boss/boss_nina.cpp`) and
`BossNina_HookHit` (kind 17's hook, the same file) call
`Field_MemberSprite(1, 1)`: they dress enemy 2 as character id 1 from the
loaded party set, whose column for an id it lacks is 3, off the row
(section 3). Before the battle the party is record 0 alone in row 0 `(0, 1,
2)`, which holds id 1; `PartySet_Find(id, 0xFF, 0xFF)`'s first match for 4,
5, 6 was a row without id 1 (rows `0x0D`, `0x10`, `0x12` in RT's logs).

**The audit** (read; `grep` over `src/game/battle`, `boss`, `magic`,
`effect` for `Field_MemberSprite`, the set row `0x669750` / `AreaW0b_PartySet`,
`ObjTrio[1]` / `[2]`, `Member(1)` / `(2)`, the second list, `+0x148` and
`+0x89` compares, and the boss files' `ObjTrio` uses), each with what the
mode's battle of one or changed set does to it:

| Where | Fights | What it reads | In the mode |
|---|---|---|---|
| `Boss16_Event` code 2, `BossNina_HookHit` | 16 | id 1's sprite from the loaded row | **crash** when the row lacks id 1. Rule `kNeedsIdInRow 1` |
| `Boss34_Setup` / `Boss34_Event` (`boss_mikba.cpp`) | 34 (Mikba, chapter 8) | the member whose record (`+0x148`) is `Character_IndexById[4]`, record 4; with none the pick byte `0x675F08` keeps an older value and the event hook still puts "the picked member" first in the order and gives it ability `0x40` | a champion other than record 4 is not that member (a stale index into the off records: inference, hang or worse). Rule `kNeedsRecord 4` |
| `Boss_SetByLeaderId` (`boss_nina.cpp`), from set-ups 18..20's `EndAreaFight` on the third way out | 18, 19, 20 (chapter 5) | ObjTrio's first record's id - the leader in the original, **the champion** in the mode - into `0x92BF18`, which `Party_PlacesByList` hands `Party_DropIn` after the battle | the post-battle placement entry is the champion's (ids 0, 1, 5, 6) or left as it was (2, 4) instead of the leader's. No crash found; **not built**, an owner call |
| `Boss25_*` (member 0), `Boss26_End` / `Boss27_End` (members by flags) | 25..27 | member 0 / the flagged members | stood down (section 10) |
| `Boss01` / `04` / `07` (member 0's forced action) | 1, 4, 7 | member 0 | chapter 0: the mode stands down there |
| `Boss32_End`, `Boss52_End`, `Boss55_End` | 32, 52, 55 | each member with `+0` bit 0 | the champion alone: fine |
| `Boss16_Exit` | 16 | `ObjTrio[1].flags |= 0x40` after `Scenario_CallA(1)` | a stale record's flag; step 7's `Field_PartyLoad(0)` rebuilds it. Harmless |
| `Accession_ApplyB` (`magic/accession.cpp`) | any (the acting member's single form) | `Field_MemberSprite(row[0], 0)` - the loaded row's first id | the row is always one holding the champion; `row[0]` is in it, so no crash; with a changed row the form may wear another character (cosmetic, inference; also the original's behaviour in rows whose first id is not the caster's) |
| `Kaiser` / `BattleFxRestore_LoadDat` | the dragon revert | the set byte (rows 7, `0xD`, `0xE`, `0xF` have files of their own) | follows the row the battle loaded, as the original does |
| `battle_menu_win.cpp`, `battle_item_cmd.cpp` | any | `0x904065[member slot]` | the second list we wrote, slot 0 the champion: consistent |
| `battle_loss.cpp` `LossMember(1)` / `(2)`, `Sprite_ReleaseTint(Member(1/2))` | the game over | guarded by the count / a tint release | fine |
| `effect_kind_54`, `_82`, `_84`, `_ae` | field scenes (member 1's animation, member 2's x) | ObjTrio records 1 and 2 | field-time, the party the game's own (the mode's field half is out of the round) |

**What is built.**

- **The row choice** (`ChooseRow`): when the current row lacks the champion,
  the row holding the champion that keeps the most of the current row's ids
  (the lowest row on a tie), in place of `PartySet_Find`'s first match. It
  writes nothing until chosen (`PartySet_Find` left its ids at
  `0x903850..`).
- **`kFightRules`** (one line a fight, the reason beside it), the Beyd
  fights' table grown three kinds: `kStandDown` (25, 26, 27);
  `kNeedsIdInRow` - the row must hold the champion and the id, chosen the
  same way, else the game's own party fights (16, id 1); `kNeedsRecord` -
  the game's own party fights unless the champion is that record (34,
  record 4). Each stand-down is one log line naming the fight.
- **Fight 16 by champion** (chapter 3: ids 0, 1, 2, 4, 5, 6 are records 0,
  1, 2, 4, 5, 6; rows holding id 1 are 0, 1, 2): Ryu - already alone; Nina,
  Garr - row 0 kept; Momo - row 1 `(0, 1, 5)`; Peco - row 2 `(0, 1, 6)`;
  **Rei - no row holds 4 beside 1: stands down.**

**Run, fight 16** (`tools/solo_boss_sweep.py run --fights 16 --champions
ryu nina garr rei momo peco --mark-at-cut`, the full `balioAndSunder_2`
route to its cut; root `D:\bof3x-runs\solo1112\CB\sweep`): plain and all
six "started, ended (win, enemies defeated)", no crash. Ryu: `already has
the champion (id 0x00) alone`. Nina, Garr: armed, set kept. Momo, Peco:
`set row changed`, then at step 7 `set 80 (put back, its field file
loaded)`. Rei: the stand-down line. After the fight the game itself puts
Nina in the party (`Boss16_Exit`'s `Scenario_CallA(1)`): the restore leaves
those bytes to it (mask `0x15` / `0x35`, count 2 the game's), as the
per-byte rule means.

**Fight 34 is argued from the code**: RT's craft `bfs34` (chapter 8, area
43, "record 4 at 0 HP") does not start the fight, plain or solo, in RT's
runs and here. The owner would need a save in area 43 just before Mikba.

**A save near fight 16** (the owner's time-saver): the route's own slot-6
save is written at its end, after the fight (the recipe's save-write check,
2026-10-03), so it cannot stand before it. The six-champion run above used
the full route at speed 4 (about 3 minutes a run). A save the owner makes in
area 41 before walking down the steps (chapter 3, before
`Scena03_StepArea29` starts fight 16) would cut a run to well under a
minute.

## 12. Chapter 5's one-on-one series: a champion's lost bout is a game over (the owner, round doc 6h)

The owner: "that fight series is interesting in that it's a series on 1-1
fights. For that, if a solo character looses, it passes to the next
character in the team - in this case, we can progress straight to game over
if that's possible."

**The series, read** (`Scena05_Run16`'s steps in `src/game/scenario/scena_05_2.cpp`,
run 0x10; set-ups 18..20 in `src/game/boss/boss_nina.cpp`; the call tables
in [`scena_calls.md`](scena_calls.md)):

- **The party** is Ryu (id 0) and two of Nina / Momo (1, 5) or Nina / Peco
  (1, 6). Steps 5 / 6 (`PairCode`) turn the second list's first two ids into
  counter 3 (1..6 for `{0, 1, 5}`, `0xB..0x10` for `{0, 1, 6}`), the order the
  members fight in.
- **One on one:** step 0x15's `GateB` runs two call-table B entries by
  counter 3 - two of `ScenaCall_Leave1` / `_Leave5` / `_Leave6` /
  `Scena05_Leave0` - so the first member is left alone; step 0x18 calls
  fight 18 (`0x12`).
- **The bouts:** fight 18, then (flag `0x1E` set at step 0x1B) fight 19 at
  step 0x21, then (flag `0x1F`, step 0x23) fight 20 at step 0x27; a rematch
  after a loss goes through `FlagBattle` (steps 0x38 / 0x42), which picks
  18, 19 or 20 by flags `0x1E` / `0x1F`.
- **The end hook** `EndAreaFight` (`Boss18_End` (0x1A, 0x1B), `Boss19_End`
  (0x1A, 0x23), `Boss20_End` (0x1A, 0x29)): a win (`0x904AE8` bit 1, set by
  `EventAreaFight` when enemy 0 or 1 falls) hands the chapter its next step
  and takes the end phase's step 1; enemy 2 fallen (`0x904AAD` bit 0) takes
  step 2; **otherwise - the member lost -** by the chapter flags: `0x23` and
  `0x24` both set, step 2; `0x23` alone, the chapter step 0x3A; neither, step
  0x1C; each of the last two with `Boss_SetByLeaderId` and step 3 (back to
  the field).
- **After a first loss** (step 0x1C, then 0x47): flag `0x23` set,
  `CodeHall(kHall47)` joins the others back (call-table A, `ScenaCall_Join1` /
  `_Join5` / `_Join6` / `Scena05_Join0`), `CodeExit(kExit34)` leaves the
  **next** member alone, and step 0x38's `FlagBattle` refights the same bout.
  **After a second** (step 0x3A, then 0x65): flag `0x24` set, `kHall65` /
  `kExit3E`, the third member, step 0x42. **After a third** (`0x23` and
  `0x24` set): the end phase's **step 2** - `BattleEnd_EnterStep2`, the loss
  screen (solo-character-reads.md M3): the original's own GAME OVER.

**What the mode did there before this change:** the scene leaves one member
alone, so the champion fought every bout - armed when the scene's member is
someone else, "already alone" when it is the champion - and a loss took the
continue path above: the next member called in (who was then replaced by the
champion again at the next bout), and `Boss_SetByLeaderId` reading member 0's
id, the champion's (ledger draft H).

**Built** (the owner's rule): `EndAreaFight`, in the "member lost" branch
only, asks `solo::SoloBattle_ChampionFightsThis()` - the mode asks for the
champion (`Solo_ChampionFights()`, which reads nothing with the switch off),
the battle's member count `0x904AB0` is 1 and ObjTrio's first record holds the
champion's record - and when it says yes takes the end phase's step 2, the
path the original takes when its last member loses, with one log line. The
win, the enemy-2 branch and every bout the champion did not fight are the
original's. Files: `src/game/boss/boss_nina.cpp` (`EndAreaFight`, one branch,
the `solo_battle.h` include), `src/game/solo/solo_battle.{h,cpp}`
(`SoloBattle_ChampionFightsThis`). `Boss_SetByLeaderId` is untouched: in the
mode its one call site (this branch's continue path) is no longer reached
after a champion's bout.

**Run** (a craft of this group's, `cbs18`: `PreEmitai`'s block at chapter 5
run 0x10 step 0x15, area 79 flags `0x8E`, party `0, 1, 5` in set row 1,
counter 3 = 1, flags `0x1E`, `0x1F`, `0x23`, `0x24` clear; recipes
`D:\bof3x-runs\solo1112\CB\scratch\zz_cb_s18_*.txt`, not in the tree). The
craft's scene stops at step 0x16 with counter 0 at 0x15 (the area's script
does not advance it from a load), so the recipe pokes counter 0 to 0x16 and
0x1B and confirms the messages. Champion Garr: step 0x15's `GateB` left Ryu
alone; at fight 18 `battle 194 fought by the champion alone (record 2 ...)
set row changed`; with Garr's HP poked to 1 and Attack confirmed each turn,
Claw downed him: `DIV-0228 SoloCharacter: the champion lost a bout of
chapter 5's one-on-one series (event battle 0x12): game over, as when the
last member loses`, the loss screen, the title (`DIV-0064: 1 of 1 parts
title`), and at the next game's start `battle 194's saved party dropped at
GameMode_Start`. **The win is argued from the code only:** the same craft
with enemy 0's KO bit poked stops, **plain and solo alike**, on `Fatal:
BossActor_CopyFrom: no field actor carries tag 3` in `ExitAreaFight` - the
craft skips the steps that place the spectator actors. A save the owner
records in chapter 5 in area 79 before the series' first bout (before
"Take your places please!") would prove the win and the rest of the series.

## 13. Fight 34 (Mikba) trials (the owner, round doc 6i; 2026-11-13) - superseded by section 14

> The trial switch below is removed; section 14 is what is built.

Not a change of the default: fight 34 still stands down unless the champion
is record 4, and record 4 at 0 HP still stands down. The trials run behind
`BOF3X_CB_TRIAL34` (read once in `solo_battle.cpp`, marked TRIAL ONLY, not a
behaviour, not ledgered; unset = nothing): `A` lifts fight 34's
`kNeedsRecord 4` rule, `B` revives and heals record 4 at the battle's start
when it is the champion (HP 1 and bit `0x4000` cleared as `ItemUse_Revive`
does, then HP to the maximum and bit `0x2000` cleared as a full
`Char_HealHp` does). Under either letter, `SoloBattle_WriteParty` also sets
`+0x148` of ObjTrio's records 1 and 2 to `0xFF` for fight 34 (step 7's
`Field_PartyLoad(0)` rebuilds them).

**Why that last part.** `Boss34_Setup` scans ObjTrio's three records by
`+0x148` with no flag test, the last match winning. In a battle of one the
records 1 and 2 are the walkers' stale records (`Field_PartyLoad(1)` only
clears their flags), and with the owner's party `07 04 08` record 1 is Rei's:
the first runs picked member 1, an absent member, and the forced turn never
came (its `+0x91` bit `0x20` test read the stale status `0x60`).

**Runs** (the owner's route `mikbaFight` and save, through RT's sweep with
`--mark-at-cut`, cut 3545, three played rounds with the sweep's defend
command and party pokes, then the kill; root `D:\bof3x-runs\solo1112\CB\sweep`,
wrapper `D:\bof3x-runs\solo1112\CB\scratch\cb_mikba.py`, extra watches of the
pick byte `0x675F08`, `0x904AAD`, the acting actor / kind `0x904B34`, member
0's `+0x124..+0x127`, record 4's status and HP). Frames under
`D:\bof3x-runs\solo1112\CB\sweep\boss\f134_<side>\`.

- **Plain:** pick `00 -> 02` (Rei, member 2); the forced turn at 3816 (the
  sweep's party pokes revive the downed Rei each round - so plain's own run
  differs from the owner's play there); win at 4859; record 4 after: level
  19, EXP 10090, HP 1, status 0; back on the field at 4920.
- **Trial A, Nina:** `TRIAL A: fight 34's rule lifted: record 1 fights it
  alone; the pick byte 0x675F08 holds 0 before the set-up`; nothing matches
  record 4, so the pick keeps its 0 - **the champion** - and at 3792..3794
  `Boss34_Event` puts her first and forces action 4 skill `0x40` on her
  (member 0's bytes `00 04 40 00`): **Weretiger** on Nina (`w003797.png`), then
  `0xC0000005` in `Sprite_LoadPalette`, under `PaletteImage`,
  `WeretigerCopy_Spawn`, `WeretigerCopy_Run`, `Battle_Frame` (llvm-nm of the
  build): Rei's form drawn for a member who is not Rei. **Crash, every
  time the forced turn comes.**
- **Trial B, Rei:** `TRIAL B: record 4 revived and healed for fight 34: HP 0
  -> 106, status 0x6000 -> 0x0000`; the pick `0` (Rei); round 0 the
  player's defend (action 2) taken, then at 3778 the forced turn, action 4
  skill `0x40` - **Weretiger** (`w003781.png`), Rei in the tiger form from
  then on (`w004140.png`, `t004692.png`); rounds 1, 2 and the kill round:
  actor kind 1 and action 1 (attack) each round although the sweep pressed
  defend - **not the player's to command**, as the owner said; the win at
  4866 (`battle_end` `02`, `0A`, `2A` as plain); the restore (mask `0x1F`);
  record 4 after: **level 21, EXP 13090, HP 106, status 0** (plain: level 19,
  EXP 10090, HP 1); back on the field at 4947 with the party of three
  (`t004980.png`); the sweep's comparison: ending, party after and the scene
  after the return the same as plain.

**What would make each sound** (not built):

- **A:** the forced turn must not land on a champion who is not record 4.
  The smallest sound form: in the mode, `Boss34_Setup` (or our step 3, as
  the trial does for records 1 and 2) leaves no pick when no member is record
  4, and `Boss34_Event` / `Boss34_End` / `GiveAbility40` skip the picked
  member when there is none - which means changes in `boss_mikba.cpp`'s
  three functions behind the mode's test. With that, record 4 is not in the
  battle: plain's write-back (record 4 HP 1, status cleared at 4916..4918) does
  not happen, so record 4 would stay at HP 0 / status `0x6000` on the field
  unless the restore writes what plain's end leaves (HP 1, status 0); what
  reads it before the second prompt is not traced. And `GiveAbility40`'s
  first call gives ability `0x40` to record 4's own list on a win in plain;
  to keep the story, that call stays.
- **B:** as run it is sound by these runs - no crash, the scene after as
  plain - with the stale-record clearing (the pick must be member 0); the
  differences are record 4's level 21 / EXP 13090 / HP 106 against plain's
  19 / 10090 / 1 (he fought and won alone, plain's Rei took no EXP), and that
  the player does not command him after the forced Weretiger turn.

## 14. Fight 34 (Mikba) as built: the owner's decision (round doc 6j, 2026-11-13)

The owner: "for other champions for that one fight, let's leave rei in the
team. So the team becomes rei (dead), champion. That's still a single
character combat, and will let the game logic progress. If a person revives
rei, it's against the spirit of the challenge, but technically they could".

**Built** (`src/game/solo/solo_battle.cpp`; `boss_mikba.cpp` untouched; the
trial switch `BOF3X_CB_TRIAL34` and fight 34's stand-down rule removed):

- `kFightRules` row `{0x22, kWithRecord, 4}`.
- **Champion record 4:** revived and healed at step 1 when at 0 HP or KO
  (HP 1 and bit `0x4000` cleared as `ItemUse_Revive`, then HP to the
  effective maximum and bit `0x2000` cleared as a full `Char_HealHp`), then
  alone as any champion. This replaces 6d's 0 HP stand-down for this fight
  only; every other fight keeps it.
- **Any other champion:** the second list `(champion, record 4's id byte, FF)`,
  count 2, formation 0; record 4 as the fight has him (the scene's HP 0,
  status `0x6000`), nothing of ours revives him or stops the player from
  doing so. **Slot order: the champion first.** The fight's code finds record
  4 by `+0x148` wherever he sits (`Boss34_Setup`, `Boss34_Event` cases 5 and
  1, `GiveAbility40`'s second call by the pick, the Weretiger tasks by the
  actor), and its end hook loops the flagged members; slot 0 is what other
  code treats as the lead (`Party_PlaceAtSlots`' leader search and this
  module's `LeaderMissed`), so the living champion holds it.
- **The row** must hold both ids (each member's sprite is its column in the
  row, section 3): `ChooseRow` with record 4's id as the needed id. In chapter
  8 every champion id shares a row with id 4 (7: rows 7, 13, 14, 15; 8: 15;
  2: 7; 5: 13; 6: 14); a champion with none would stand down with a line, not
  built further (no case reaches it).
- **The stale records** (0a item 8's family, the audit of 2026-11-13): in
  `SoloBattle_WriteParty`, for every battle, the records past the battle's
  count get `+0x148 = 0xFF` (`Character_IndexById` holds 0..7, so 0xFF names
  no record), their old bytes kept in the save and put back at
  `SoloBattle_Restore` where the byte still reads `0xFF`. `+0x89` is not
  touched (`Party_PlaceAtSlots` and `LeaderMissed` read the ids). Step 7's
  `Field_PartyLoad(0)` rebuilds `+0x148` for the field's members
  (`field_6.cpp`, `m->char_index = actor` for each `i < Field_MemberCount`);
  the put-back covers a record past the field's count. `Field_Request == 6`
  at step 3 (which would skip `Party_ClearAll`) is a Fatal: mode 5 enters on
  request 3 and no step before 3 writes it (read: `GameMode_Field`,
  `GameMode5_Turn` / `_ToPlaces` / their callees).
- The quicksave record `SOLOCB` is version 3 (the stale bytes).

**Runs** (the owner's `mikbaFight` route and save, RT's sweep, `BOF3X_SOLO`
from the start, `mortal` so the harness does not refill HP, two played rounds
with the defend command, then the sweep's kill; wrapper
`D:\bof3x-runs\solo1112\CB\scratch\cb_mikba.py` entry 134; root
`D:\bof3x-runs\solo1112\CB\sweep`, logs and frames in `boss\f134_<side>`):

| Side | Party in battle | Ended | Turns taken (actor) | Record 4 in battle | After the return vs plain | Garbage frames |
|---|---|---|---|---|---|---|
| plain | `7 8 4`, count 3 | win | members and enemies | (the scene's) | - | 0 |
| ryu | `7 4`, count 2 | win | 00 and enemies only | HP 0, `0x6000` to the end | same (ending, party, scene) | 0 |
| nina | `8 4`, count 2 | win | 00 and enemies only | the same | same | 0 |
| garr | `2 4`, count 2 | win | 00 and enemies only | the same | same | 0 |
| rei | `4`, count 1, revived HP 106 | win | 00 (the forced Weretiger turn at 3778, then the game's own attacks) and enemies | - | same | 0 |
| momo | `5 4`, count 2 | win | 00 and enemies only | HP 0, `0x6000` to the end | same | 0 |
| peco | `6 4`, count 2 | win | 00 and enemies only | the same | same | 0 |

For the non-Rei champions record 4 ends the fight as plain's does: HP 1,
status `0x2000` (`BattleEnd_Finish`'s fallen member), the record's level and
EXP unchanged; for champion Rei: level 21, EXP 13090, HP 106, status 0. The
"after the return" comparison covers the sweep's 1500 frames of confirms after
the return; the route's second party prompt (frame 19128 in RT's straight
replay) is not reached by the sweep on any side, plain included.

- **Rei revived mid-fight** (Nina, the sweep's every-round HP pokes plus a
  poke clearing the KO byte, `cb_revive_nina.txt`): Rei back at the fight's
  start, a commanded defend, then the forced turn - Weretiger - at 3808, then
  his own attacks each round while Nina takes the player's commands
  (`cbrev_nina\w004254.png`: Rei as the tiger beside Nina). Win, no crash, 0
  garbage frames. With the HP poke alone (no KO clear, entry 135) the forced
  turn fires on the still-downed Rei and nothing breaks either.
- **Quicksave mid-fight** (Garr, row changed): F6 at 3646, F7 at 3819
  (`quicksave F7 BISLPS10.DAT restored ... 5 participants`), the fight on to
  a win, the restore mask `0x3F` with the set's field file loaded at step 7.

The machine was shared (the owner may have been using it); every run used
`--no-front`, and the runs replayed to their ends with no desync sign.

## 15. The two fights the story has the party lose stand down (the owner, round doc 6k, 2026-11-14)

The owner: "there are 2 fights that are 'must lose' - the first balio and
sunder fight, and the first garr fight (the chapter 5 ending fight). We should
probably disable the solo character for those fights, as they're not meant to
be won anyway".

**Built:** two `kStandDown` rows in `kFightRules`, `0x0D` (fight 13, chapter 2,
area 11) and `0x16` (fight 22, chapter 5, area 80), the numbers of
`boss-fights.md`'s rows; the same path as the Beyd fights (section 10). The
self-test checks both rows and that fights 12, 14, 21 and 23 have none (37
checks).

**Run** (2026-11-14, `tools/solo_boss_sweep.py`, root
`D:\bof3x-runs\solo1112\round\ml`, `--fights 13 --champions nina garr
--mark-at-cut`): plain, and the mark set to records 1 and 2. All three end in
the loss, the battle's party the field's three in each (count 3), the story on
to area 3 in chapter 2 as plain; each champion run logs the stand-down line
naming event battle `0x0D`.

**Not run:** fight 22. No save reaches it (`solo-character-proof.md` 6d); its
stand-down rests on the row, the self-test and fight 13's run of the same
path. **That fight 22 is a fight the story has the party lose is the owner's
statement, not read from the code here.**
