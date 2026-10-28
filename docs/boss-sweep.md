# The boss sweep: every boss's moves, by its own fight

**Status:** PROPOSED (2026-10-27 evening, the owner's ask before the staged
rounds: "update the spell/item harness to also test boss moves"; the owner's
suspicion that the kind injections of [`spell-sweep.md`](spell-sweep.md) 11
could swap the enemy kind and graphic in combat so moves like Nue Stomp render
from the right body. Section 1 answers that from the code; sections 2 to 4 are
the plan; the runs and findings will follow as sections. Section 5 is
step 1, the `battle ID` op and its proof, 2026-10-27 night.)

## 1. Why a kind swap cannot stage a boss, and what can

Read in our code (2026-10-27) and measured once in spell-sweep.md 11.2:

- A **random encounter** builds its enemies from the area's encounter rows
  (`Encounter_Rows`, `0x8C5580`) through `Battle_InitEnemies` (`0x4942C0`):
  each slot's sprite and animation bank come from the area's enemy data
  record and `ENEMYnnn.DAT`. Poking a boss kind into a row crashes in the
  sprite draw because a boss kind's animation bank is `0xFFFF`: **a boss has
  no battle sprite of its own**.
- An **event battle** (`EventBattle_Id`, `0x904AAA`, not 0) goes through
  `Battle_InitBossEncounter` (`0x4942A0`) and `Battle_PlaceBossActors`
  (`0x494500`, `rest_3g.cpp`): the fight's record `EventBattle_Records[id]`
  (`0x64DDEC`, 4 bytes: `+0` flags, `+1` formation, `+2` the area's encounter
  row, `+3` an asset index) names a row of the *loaded* area; for each slot of
  that row with a kind, `Battle_PlaceBossActor` looks for **the field actor
  tagged with the slot** (`BossActor_Find` `0x494920`: a `Sprite_Objects`
  record with type `+6 == 7` and `+0x9E == slot`), copies its first `0x80`
  bytes into the enemy record (so the boss's body in battle *is* its field
  sprite), then `Battle_CopyEnemyData(count, kind)` for its stats and lists.
  No tagged actor, no enemy.
- The field side is one call: `Field_StartEventBattle(id)` (`0x4410B0`,
  `scena_se.cpp`): sets the fight byte, the leader's state 5 (the encounter
  transition) and the flags byte from the record. 53 call sites in 14
  chapters' scenario code place the actors first (the scene), then call it.

So the injection that works is **not a kind poke but the event battle itself**:
stand in the boss's area with its tagged actors present and call
`Field_StartEventBattle(id)`. The sweep's enemy-side pokes (the base ability
list, the AI rows off, the party's HP 999) then apply unchanged, with the
boss as enemy 0.., and the forced cast plays on the boss's own body.

The 56 records (dumped from the PC exe at `0x64DDEC`; game data stays out of
the repo, the dump is in the harness root): every fight but 0 names row 4..7
of its area, formation 0..3. The fight ids whose set-ups the boss round named
(takeover-queue-bosses.md 1.2, the kinds table there) give id -> area.

## 2. What "boss moves" are, in two parts

1. **Enemy skills only a boss carries** - the seven of spell-sweep.md 11.3
   without a route: Head Cracker (Shroom, area 119), Holocaust and Venom
   (Myria, 198), Corona (Gaist, 120), Main Cannon (Ammonite, 134), Paralyzer
   (Chimera, 197), Eye Beam (Gazer, 77). Forced casts, the by-kind sweep's
   method, once the fight is staged.
2. **A boss's scripted behaviour** - what `BossKind_Table[kind]`'s
   dispatcher does by state (forms, phases, the Nue's leaving, hit hooks that
   put HP back to 1, end hooks): not in any ability list, so a forced cast
   never shows it. That wants the fight *played*: N rounds with the party
   invulnerable, a shot at every enemy pick and every state change, and -
   where a phase needs it - the boss's HP lowered by poke. Which bosses and
   how many rounds is the owner's call (section 4).

## 3. The plan

**Step 1 - the `battle ID` recipe op and its proof** (no decision needed).
`src/hook/input_script.cpp` gains `battle ID`: on the game thread, as `goto`
calls `Field_ChangeArea`, it calls `Field_StartEventBattle(ID)` and logs the
fight. Proof: the owner's `cutsceneAndNue` route reaches fight 2 (area 23)
by its own script; a copy cut *before* the script's call (the actors already
in the field - the mode peek copy of spell-sweep.md 11.2 finds the frame)
plus `battle 2` must give the same battle: the `boss encounter:` log line,
the prologue peeks (area, enemy 0's slot and kind), and the pictures compared
with `abc_compare.py` against the route's own fight. A second proof on a
fight whose actors are the area's own objects (the Amalgam, `ropeAndAmalgam`)
tells whether `goto` alone, without the scene, is enough there.

**Step 2 - reaching each boss's pre-fight state** (research, no game at
first). For every fight id: the chapter and scenario state whose code calls
`Field_StartEventBattle(id)` (the `scena_sc*.cpp` call sites), the area, and
whether the tagged actors are area objects or script-spawned. Then the
chapter-jump save method (`docs/ending-backdrop.md`: block bytes 0 and 2 the
chapter and state, checksum u16 at `+0x70`) crafted to the state *before*
the call, tried on the PC install, one fight at a time, the seven of section
2.1 first. What no crafted save reaches is listed for the owner's recording
(the owner's routes already reach fights 2, 12, 23/24, 32 and the Balio,
Sunder, dolphin, whelp fights).

**Step 3 - `spell_sweep.py --fight ID`**: head = the save, settle, (`goto`),
`battle ID`, until mode 5, the HP pokes; then the enemy side's casts of the
ids the fight's kinds carry (`kind_census.json`); and `--play N` for section
2.2: N rounds of Defend with shots at each pick, HP pokes as options. The
report gains the fight, its kinds and the state bytes seen.

**Step 4 - the runs**: the PC install and the US-disc install with
`BOF3X_GAPWATCH`, pictures compared; findings to this doc, defects to
`known-defects.md`, any behaviour change to the ledger.

## 4. The owner's calls

1. Scope: section 2.1 only (the seven skills), or 2.2 too (played fights) -
   and if 2.2, which bosses first.
2. Whether the harness may kill a boss (phase transitions, end hooks) or
   holds every HP, as the spell sweep does.
3. Installs: PC + US-disc as the spell sweep, or the PSP install as well.

## 5. Step 1: the op and its proof (2026-10-27 night)

**The op.** `battle ID` (`src/hook/input_script.cpp`, [`input-script.md`](input-script.md)
2): on the game thread, at the latch, `Field_StartEventBattle(ID)` through
the original's entry - the way `goto` calls `Field_ChangeArea` - logging
`input battle id N in area A; recipe frame F`. ID is a byte; the step costs
no frame. A harness step, like `goto` and `door`.

**The runs.** The PC install (the main checkout's `bof3`, `--lang en-US`,
`BOF3X_OPT=none`, `BOF3X_LAYERING=0`, speed 4) through an ini-less launcher
copy of this branch's build. Root `C:\Users\kerok\bof3x-runs\bsw1027\op`:
the recipes, the scripts that made and read them (`tools\mk_measure.py`,
`mk_pair.py`, `cmp_logs.py`), one folder of frames and log a run, the
compares `cmp_*`. A trial is the route cut at frame C, `battle ID`, then the
route's own inputs from about the frame of its own call on (so the battle's
buttons land on the same battle frames), with the same named shots every 30
frames from the fight's start. Peeks cost no frame, so the reference is the
route as recorded. Read: the field mode `0x66C7E8`, the fight byte
`0x904AAA`, the leader's state `0x802D41`, the actors `Sprite_Objects`
(`0x7DEE80`, `0xA4` a record: `+6` the type, `+0x9E` the slot) and the
sweep's prologue peeks (`spell_sweep.py prologue_peeks`) 65 and 200 frames
into the fight.

### 5.1 The Nue (fight 2, area 23, `cutsceneAndNue`)

The reference: the scenario's call runs in frame 4234 (the fight byte 2 and
the leader's state 5 first read at 4235), mode 5 at 4236, `boss encounter:
id 2 (0x02) -> set-up 0x00438190` at 4293, the field back at 6750, then the
post-fight scene (messages, the save prompt). (Spell-sweep 11.2's cut 4300
is after mode 5; it worked because its `until` passes at once.) The tagged
actor - record 1, type 7, `+0x9E` 0 - is first in the field at **frame
3872** (absent at 3871, read every frame): the window is **3872..4234**.

| Cut | State at the cut | `boss encounter` | Prologue (count, enemy 0 slot, odds, flags, HP) | Battle frames differing | After the fight |
|---|---|---|---|---|---|
| 4220, resumed at 4234 | mode 2 (the scene's last message closed at 4205) | the same line, set-up `0x438190` | identical at +65 and +200 (1 enemy, slot 2, odds 18, flags `0xE0`) | **0 of 84** (4240..6731); the state peeks identical from the fight's first frame to its 2575th; `randlog` identical frame for frame 4235..6969 | the field, **no post-fight scene** (mode 2 to the end); 23 frames differ, all after 6750 |
| 4220, resumed at 4235 | the same | the same | identical | 13 of 84, the first rounds only: the Nue's idle animation and the status bar's slide one frame apart | as above |
| 4000 | mode 2, before the scene's last moves | the same | identical; `randlog` identical through the fight | 107 of 107: staged where the camera stood at 4000 - the party off screen, the Nue facing the other way | the field, no scene, the zone track 255 |
| 3880 | mode 1 (the actor's arrival) | the same | identical | 84 of 84: the transition waits out mode 1, and the random numbers are off from the fight's first frame | - |
| 3800 | no tagged actor (inside the house) | the same | **0 enemies** | - | **ours crashes 416 frames later**: an access violation reading `0x0000000C` in `Battle_SetHitPopup` (`battle_sprites.cpp`), called from `BattleObj_HitReceive` (`battle_e3.cpp:521`), `BattleParty_RunStates`, `Battle_Frame`; mode 5 step 5, the route's buttons pressed in an empty fight. A state the game never makes - the harness's misuse, as spell-sweep 11.2's boss kind in an encounter row. Not changed. |

The op's call lands at the start of a frame and the script's in the middle
of one, so the transition after the op can take a frame more or less than
after the script (the Nue: one more - the 4220 trial is aligned when the
route resumes at 4234, the frame before the script's call; the Amalgam, below,
two). Comparisons are aligned on the fight's start.

### 5.2 The Amalgam (fight 12, area 28, `ropeAndAmalgam`)

The reference: the save loads into area 28 at about frame 262; at 418 the
route walks into the area's link to its own other part (the pending area 28,
x `0x270000`, z `0x568000`, flags 3 - the door sweep's arrival 28.3), mode 1
at 453, and at **454** record 8 (type 7, `+0x9E` 0) and record 9 (type 9)
appear with that arrival. The scenario's call runs in frame 4902, mode 5 at
4904, `boss encounter: id 12 (0x0C) -> set-up 0x004398E0` at about 4961.
Prologue: 1 enemy, enemy 0 slot **1** (`+0xF0`; spell-sweep 11.4's table
gives the Amalgam slot 0, but this peek reads 1 in the route's own fight and
in every trial below), odds 18, flags `0xE0`.

| Trial | Tagged actor at the call | Result |
|---|---|---|
| cut 4901, resumed at 4901 | yes | 0 of 119 frames differ, `randlog` and every peek identical - but the scenario's own call follows a frame later, so this checks the op only against itself |
| cut 4901, resumed at 4902 | yes | 3 of 119 differ, the first rounds: the one-frame offset of 5.1 |
| cut 4700 (mode 2, 203 frames early) | yes | the prologue identical (1 enemy, slot 1, HP as the reference's at +65 and +200); **119 of 119 differ**: a full-screen white the scene had up at 4700 stays over the whole fight |
| cut 300 (area 28, before the link), `battle 12` | no | `boss encounter` logged, **0 enemies**, mode 5 for 670 frames with no buttons pressed, no crash |
| cut 300, `goto 28 0x410000 0x2A8000 7` (arrival 28.2), 150 frames, `battle 12` | no (the reload placed none) | 0 enemies, as above |
| cut 300, `goto 28 0x270000 0x568000 3` (arrival 28.3), 150 frames, `battle 12` | **yes**: record 8 type 7 slot 0 and record 9 type 9, from the reload alone | 1 enemy, slot 1, odds 18, flags `0xE0`, HP as the reference's: **the Amalgam's fight from `goto` alone**, no scene. Staged where the camera stands at the arrival: the Amalgam and the party off screen, the command cross working |

So **the Amalgam's actor is an object of area 28's 28.3 part**, placed by
the area load in this save's scenario state; the Nue's is placed by its
scene (at 3872, inside the scene's own transitions).

### 5.3 What the op needs, measured

1. **The tagged actors in the field.** Without them the set-up runs, finds
   no actor and leaves 0 enemies (fight 12: an empty fight that holds;
   fight 2 with the route's buttons: ours crashes in the hit popup). Step 3
   should peek the actors (`+6 == 7`) before `battle` and refuse a fight
   with none.
2. **The fight's logic does not need the scene.** From every cut with the
   actor present, the set-up line, the enemy count, slot, odds, flags and
   HP were the reference's, and the random numbers matched through the
   fight where they were compared (the Nue from 4000 and 4220).
3. **The picture does need the scene** where the scene moves the camera or
   the party, or has a screen effect up: the fight is staged on the field
   as it stands. A cast drawn on an off-screen boss is the staging's fault,
   not the cast's; step 3's head should cut after the scene's last move
   (the Nue: after 4205), and its pictures be read with that in mind.
4. **After the fight** the scenario does not go on as after its own call
   (the Nue: no post-fight scene): a `battle` run is a fight, not a route.

A trial recipe, the Nue's (the route's frames 0..4219 in place of the
`...`):

```
# save nue
...                                  # the route to frame 4219
peek 0x904AAA u8 ebid_before_battle  # 0: the scenario has not called it
battle 2
until 0x66C7E8 u16 == 5 timeout 600
```
