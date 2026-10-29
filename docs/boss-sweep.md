# The boss sweep: every boss's moves, by its own fight

**Status:** PROPOSED; steps 1 to 3 MEASURED, sections 5 to 8 (2026-10-27 evening, the owner's ask before the staged
rounds: "update the spell/item harness to also test boss moves"; the owner's
suspicion that the kind injections of [`spell-sweep.md`](spell-sweep.md) 11
could swap the enemy kind and graphic in combat so moves like Nue Stomp render
from the right body. Section 1 answers that from the code; sections 2 to 4 are
the plan. Section 5 is step 1, the `battle ID` op and its proof,
2026-10-27 night; section 6 step 2, the fight table and the crafted
pre-fight saves, 2026-10-27 night; section 7 step 3, the fights played to
their deaths on three installs, 2026-10-28; section 8 fight 35 from the
owner's route, 2026-10-29.)

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

## 4. The owner's calls (answered 2026-10-27 evening)

1. Scope: **both** - the seven boss-only skills (2.1) and the played fights
   with their scripted moves (2.2).
2. **The harness may kill a boss.** The owner: two of the recent visual
   glitches came from boss deaths - the D.Zombie's (fight 32,
   `minecartToZombieDragon`) and the Amalgam's (fight 12, `ropeAndAmalgam`)
   death effects were broken; the D.Zombie's is the blood / oil pool,
   which the owner thinks may be area logic rather than boss logic, so its
   check reads the field's effect objects after the kill as well as the
   battle's - so the end phases are wanted - every
   fight played through to its death animation, its end hook and its exit
   hook, with shots across them (section 3's `--play` mode drops the boss's
   HP by poke when the party's own hits are not enough, and keeps the party
   invulnerable).
3. Installs: PC, US-disc **and the PSP install** - the owner: it has slight
   differences sometimes.

So step 3's `--play` is the main mode, not an option: N rounds to the kill,
a shot at every enemy pick, every state-byte change (the enemy object's
+1..+4, docs/boss_harness.md section 4) and the end sequence, then the
forced casts of 2.1 on the fights that carry them, before the kill.

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
in every trial below - 1 is right: section 7.6), odds 18, flags `0xE0`.

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

## 6. Step 2: the pre-fight states

**Status:** MEASURED (2026-10-27 night, headless, the PC install, branch
`boss-sweep/prefight-saves` off `main` `fec0624c`, no game code changed).
The table of every fight - who calls it, the chapter, run and step, the
area, the actors - is [`boss-fights.md`](boss-fights.md); this section is
what crafted saves reached.

### 5.1 How

`tools/chapter_jump.py` gained the `bf*` crafts (`list` names them, `make`
writes them into `--out`; the saves are game data and stay in the harness
root `C:\Users\kerok\bof3x-runs\bsw1027\saves\crafted`). Each is a donor
recipe save with the chapter, state 2, the run and step before the
scene's battle step, the area and position, the counters (block `+0x20`)
and the chapter flags that keep the load's EnterArea from re-arming
another run. The runs: a launcher copy with no ini, a game folder of the
root's own (junctions to the main checkout's `bof3\DAT`, `BGM`, `SND`),
`BOF3X_OPT=none`, `en-US`, speed 4, `BOF3X_RECIPE_SAVES` naming the
crafts' folder. The probe recipe (`tools/recipes/bossfight_probe.txt`,
`--save bfNN` to choose the craft; generated): ending17's load opening,
then every 10 recipe frames the field mode `0x66C7E8`, `Game_AreaNumber`,
`EventBattle_Id` `0x904AAA`, run, step, counter 0, `Cond_ByteFD` and the
block's area word; every 300 frames the 30 `Sprite_Objects` records' type
`+6` and tag `+0x9E`; at frame 688 the loaded area's encounter rows 4..7
(which slots have a kind); circle held 2 frames every 40 frames (the
scenes' message boxes wait for it); a shot every 120 frames. The exact
frame of a fight came from a second recipe peeking the fight byte and the
mode every frame with the same presses. The runs are deterministic: the
10-frame and the 1-frame probes agree.

### 5.2 The seven boss-only skills' fights

| Fight | Enemy | Craft (donor; state) | Result | Fight byte / mode 5 at recipe frame | Tagged actors (index, tag) at 688 |
|--:|---|---|---|---|---|
| 28 | Gazer | none needed: the owner's save `midEyeEncounter` is chapter 6 run 0xE **step 0x1E** in area 77, the battle step itself | **the scene starts the fight**; the Gazer drawn | 440 / 441 | (1, 0); row 7 slot 0 |
| 55 | Myria | `bf55b` (`ending17`; ch 15 run 6 step 0xB, counter 0 = 0xF, area 198 at (0x170000, 0x120000)) | **the scene starts the fight** on its own timers; Myria drawn | 479 / 480 | (5, 0) |
| 55 | Myria | `bf55` (`ending17`; run 6 step 0 in area 198, as `cs198`) | Myria stands; the scene stalls at step 1 waiting for counter 0 = 5 (it reaches 2; 4,600 frames) | - | (5, 0): ready for `battle 55` |
| 54 | Shroom | `bf54b` (`radiationRoom`; ch 9 run 9 **step 3**, counter 0 = 0x28, area 119 at (0xA0000, 0x28000), flag 0x16 set, 0x1E clear) | **the scene starts the fight**; the enemy listed in the target box, not in the camera's view in the shots | 440 / 441 | (1, 1); row 6 slot 1 |
| 54 | Shroom | `bf54` (the same at step 0) | the scene runs (the party dropped in, the lab) and stalls at step 2 waiting for counter 0 = 0x25 (it reaches 4) | - | (1, 1): ready for `battle 54` |
| 38 | Ammonite | `bf38c` (`radiationRoom`; ch 12 run 9 **step 0x14**, counter 0 = 0x17, area 134 at (0x348000, 0x258000), flags 1 and 0x2F set) | **the scene starts the fight** after the field's own 0x18 / 0x19 bumps; two enemies listed; the camera faces the sea, the Ammonite not in view | 784 / 785 | (0, 0), (1, 1); row 7 slots 0..3 have kinds |
| 38 | Ammonite | `bf38` (step 0xA, counter 0 = 0) | the scene runs steps 0xA..0x11 and stalls at 0x12 (a held button with counter 1 not 0xFF, or counter 2 = 0x80) | - | (0, 0), (1, 1) |
| 50 | Chimera | `bf50c` (`adult_ryu`; ch 14 run 5 **step 0xB**, counter 0 = 0x1E, area 197 at (0xB8000, 0x728000), flag 0xD set, 0xF clear) | **the scene starts the fight** on its timers; the Chimera drawn; adult Ryu alone (the donor's party) | 572 / 573 | (0, 0); row 7 slot 0 |
| 50 | Chimera | `bf50` (step 0xA) | stalls at step 0xA waiting for counter 0 = 0x1C (it stays 0: the entry's `DropIn(2)` for an arrival from zone 5 is not run for a craft) | - | (0, 0): ready for `battle 50` |
| 35 | Gaist, Torch | `bf35` (`radiationRoom`; ch 10 run 0xD step 0xF, area 120 at (0x250000, 0x748000), zone 5, flags 0x39..0x3B set, 0x3C / 0x3D clear) | stalls at step 0x10 waiting for counter 0 = 0xE (it stays 0xA); the Gaist and the frog stand in the field as **type-1** objects with tag 0: **no type-7 actor at all**. The same with the `ending17` donor's party | - | none |
| 35 | Gaist, Torch | `bf35b` (step 0x13, counter 0 = 0x13) | the scene calls fight 35 at frame 448 (`boss encounter: id 35`), then **crashes** in the battle's set-up, mode 5 step 5: `0xC0000005` reading `0x0000000C` at `bof3x.dll+0x202795` (dump in the root's `L1`): a battle with no tagged actor, a state the game never makes - the harness's, not a defect | - | none |

So **five of the six carrier fights start headless from a crafted (or the
owner's) save with no `battle` op at all** - 28, 38, 50, 54, 55: the scene
calls `Field_StartEventBattle` itself. For a forced cast from a state
before the scene (the `battle` op), `bf54`, `bf38`, `bf50` and `bf55`
stand the tagged actors. **Fight 35 (Gaist; Corona) is not reached**: its
actors become type 7 somewhere in the scene's steps 0x10..0x12, which the
crafted state does not complete (area 120's scripts never bump counter 0
past 0xA) - the owner's recording, or a read of area 120's event script.

### 5.3 The cheap ones: chapter 15's ten sample fights (39..48)

`Scena15_EnterArea` starts each sample area's battle on entry, by the zone
the party arrives in and the area's flags (`scena_sc15.cpp:366..401`). So a
save in the area at a link's arrival (the recipe op `links` read the
image's lists) in the right zone, chapter 15 flags 0x20..0x2F clear, starts
it: the run waits for any held button, then the fight. All ten reached,
each enemy drawn (`sheet_samples.png` in the root):

| Fight | Craft | Area | Arrival (link): x, z, flags | Zone | Actors (tags) |
|--:|---|--:|---|--:|---|
| 39 | `bf39` | 158 | 157.0: 0x98000, 0x1A0000, 1 | 0 | 0, 1, 2 |
| 40 | `bf40` | 159 | 157.6: 0x48000, 0x1A0000, 1 | 0 | 0, 1 |
| 41 | `bf41` | 160 | 157.11: 0x1E8000, 0xC0000, 1 | 1 | 0, 1 |
| 42 | `bf42` | 161 | 157.16: 0x1F0000, 0x48000, 3 | 1 | 0 |
| 43 | `bf43` | 162 | 157.18: 0xE0000, 0x108000, 7 | 0 | 0 |
| 44 | `bf44` | 162 | 157.20: 0x1D0000, 0xC8000, 3 | 1 | 2, 3 |
| 45 | `bf45` | 163 | 157.23: 0x48000, 0x180000, 1 | 0 | 0 |
| 46 | `bf46` | 164 | 157.27: 0x48000, 0x180000, 1 | 0 | 0..4 |
| 47 | `bf47` | 165 | 165.1: 0x268000, 0x30000, 5 | 1 | 0 |
| 48 | `bf48` | 166 | 164.3: 0x88000, 0x1A0000, 1 | 0 | 0, 1, 2 |

Fight byte / mode 5 at recipe frames 563 / 564 (`bf39`, `bf47` measured
frame by frame; the others at 568 by the 10-frame probe). The donor is
`adult_ryu`: adult Ryu alone against each. The generic position (0x120000,
0x180000) is zone 2 in all nine areas: no battle.

### 5.4 What no crafted save reaches yet (for the owner's recordings)

- **Fight 35** (Gaist, Torch, area 120; Corona): section 6.2; reached by the owner's route `geist` since 2026-10-29 (section 8).
- **Not tried, no owner's route**: 3 (the owner's `nue2` save is chapter 1
  run 9 step 0x11 in area 22 and stalls there with no type-7 actor; `nue`,
  run 8 step 0 in area 23, stalls the same), 4, 5, 6 (zone-dependent:
  `Cond_ByteFD` is not in the save, the arrival cell picks it), 9, 10, 11,
  14, 15, 17..22, 25..27, 30, 31, 33, 34, 36, 37, 49, 51, 52, 53. Each has
  its state in [`boss-fights.md`](boss-fights.md); 25..27 and 36 (and 32)
  have no starter in chapter code (a field script's op F6), so their crafts
  would start at the run's first step in the area.
- **Reached by an owner's route already**: 1 (`battle_commands`), 2
  (`cutsceneAndNue`), 7 (`whelpBoss`), 8 (`bossAndFlash`), 12
  (`ropeAndAmalgam`), 13 / 16 (`balioAndSunder_1` / `_2`), 23 / 24
  (`stallion`), 29 (`dolphinBoss`), 32 (`minecartToZombieDragon`); 28 by the
  owner's save `midEyeEncounter`.
## 7. Step 3: the fights played (2026-10-28)

**Status:** MEASURED (2026-10-28, branch `boss-sweep/play` off
`boss-sweep/battle-op`; the PC install, a US-PSX-only install and a
PSP-EU-only install). Root `C:\Users\kerok\bof3x-runs\bsw1027\play`: the
runners (`ss.sh` the PC install, `ssd.sh psxus|pspeu` a disc-only install
with the gap watch, `fights.sh` / `fights2.sh` / `casts.sh` the fight
lists), one folder of frames and a log a run under `pc\`, `us\`, `psp\`,
`results_fight.tsv` in each, the compares under `cmp\`, the contact sheets
`end_*.png` / `sheet_*.png`, the scripts in `tools\` (`endsheet.py`,
`align.py`, `cmp_all.py`).

### 7.1 The harness

**Recipe ops** (`src/hook/input_script.cpp`, [`input-script.md`](input-script.md) 2;
harness steps and instrumentation, no ledger entry):

- `battle ID` now logs the tagged field actors it finds (`Sprite_Objects`
  type `+6` 7, the slot `+0x9E`: `tagged actors 8:slot0`) and **refuses**
  when there are none - the recipe FAILS before the call, so the empty
  fight of 5.1 (which crashes ours once buttons are pressed into it) is no
  longer reachable by mistake. `battle ID any` stages it anyway.
- `place X Z ID`: `Party_PlaceForBattle` (`0x532ED0`) through the
  original's entry, as a scene places the members before its battle call.
- `view X Z`: `Field_Kind2X` / `Z` and `Field_ViewReset` (`0x56F670`) through
  the original's entry, as a scene frames its battle (chapter 14's run at
  step 0x19 does exactly this before its `Party_PlaceForBattle`).
- `watch ADDR LEN NAME [shot]`: a range compared at every new frame, each
  change logged with the bytes before and after; with `shot` the frame is
  written (`wFFFFFF.bmp`) and every frame drawn while the watch is on. Its
  reads hold the gap watch's guards off (as the state hash's do), and a
  page the gap watch keeps parked is read as `peek` reads it - the first
  disc runs read `0x66C7E8` as `EEEE` until that (commits `01ca409a`,
  `97a21b02`).
- `every N`: timed frames (`tFFFFFF.bmp`) from a recipe step, as
  `BOF3X_SHOT_EVERY` from the start.

**`spell_sweep.py run --fight ID`** (`tools/spell_sweep.py`):

- **Head.** The route to `--cut`; or to `--field-cut` and `goto` (`--goto
  AREA:X:Z:FLAGS`) for a fight whose actor is the area's object; then
  `--pre-poke` (scene state the staging lacks), `--camera X:Z` (`view`),
  `--place X:Z` (`place`), the 30 actors' flags / type / slot peeked, the
  watches set (mode, fight byte, phase / step, enemy count and enemies
  left, the end byte `0x904AE8`, the hooks `0x904B64..6F`, the fight's
  centre `0x903780`, the camera's focus, the area, each enemy's state
  bytes `+1..+4`, HP, pick `+0x105` and kind `+0x100`, and the 20
  `Effect_Objects` `0x7E11E0` records' first six bytes), `battle ID`,
  until mode 5, the party's HP / max HP / DEF 999, the prologue peeks 65
  frames in (they now come after the set-up: section 7.6).
- **`--ids`**: the enemy side's forced casts (section 11's pokes) on enemy 0.
- **`--play N`**: N rounds of Defend (one `seek` for however many crosses),
  the party's pokes at every cross; a shot at every phase / step change,
  every change of an enemy's state bytes, every pick and every change of
  the end byte and the counts.
- **`--kill`**: at the next cross `every 3` (timed frames from here to the
  end), the HP of the `--kill-enemies` records poked to `--kill-hp` (1),
  and the confirm alone at each cross - command 0, the attack, on the
  cursor's default target - one seek through the round; then confirms
  every 12 frames until the field mode is 2 again, through the death, the
  end and exit hooks and the result screen; the end byte, the hooks, the
  area, the fight byte and the 20 effect records peeked after the return;
  `--after` frames more in the field (`--after-confirm`: a confirm every
  30 for the area's own scene). `--mortal` / `--to-end` for a fight the
  game ends with a loss: no party pokes, the confirm until the field.
- **`--reference`**: the route's own fight instead - the route frame for
  frame (watches cost no frame), the same watches from `--watch-from`,
  timed frames over `--every-from` .. `--every-to`.
- **The report.** `results_fight.tsv`: the fight, the set-up line, the
  tagged actors, the kinds, the state bytes each enemy went through, the
  frames of the call, mode 5, the win (`0x904AE8` bit 1) or the loss
  (bit 0), the last enemy gone, the hooks, the return, the effect records
  that changed after it, the shots. `spell_sweep.py fightlog LOG --fight
  N` reads a log again; `spell_sweep.py selftest` checks the route
  expander, the fight recipe's order and the log reader without a game.

**What the staging needs, measured beyond 5.3:**

1. **The cut is the frame three before the fight byte's first read** (the
   scene's own call is then still to come): 5.1's Nue and 5.2's Amalgam
   agree, and every routed fight below was staged so. One trap: the
   Stallion's second fight cut at 10572 sat in a talk (mode 4) - the op
   ran, and the battle never began (`until` mode 5 timed out); at 10574,
   after the talk, it did.
2. **`goto` staging needs the scene's view and the actor shown.** The
   Amalgam from cut 300 + `goto` (5.2's last row) was fought off screen;
   `place` alone did not change the picture (the camera stays at the
   arrival). A **poke** of `Field_Kind2X` / `Z` alone left the view
   stepping toward the new focus and the battle's start waiting - mode 5
   for 1,440 frames with no set-up line (trial 1). `view` rebuilds the view
   at once, as the scenes do; with it and `place`, the framing is the
   route's. The Amalgam was then fought **invisible** until its death: the
   area's actor record 8 stands with `+0` = `0x41` (bit 0x40, not drawn)
   where the route's scene has made it `0x01` by its call, and the battle
   copies the actor's first 0x80 bytes into enemy 0. `--pre-poke
   "0x7DF3A0 u8 1"` (record 8's `+0`) shows it.
3. **After the fight the scenario does not resume** (5.3.4 again): the
   D.Zombie's post-fight scene and the departure of section 7.5 are only in
   the route's own run.

### 7.2 The framing proof: the Amalgam

The staged fight (`goto 28 0x270000 0x568000 3`, `--pre-poke`, `view
0x190000 0x5B0000` - the reference's `Field_Kind2X` / `Z` at its call -
`place 0x1C0000 0x5B0000 12` - the scene's own call, `scena_sc2.cpp`
run 0x11 step 0x11) against the route's own fight, frames renamed by their
offset from each fight's first mode-5 frame (`align.py`) and compared with
`abc_compare.py` (`cmp\out_a_t5`): the camera, the floor, the walls, the
party's three places and the cross are the route's to the pixel; what
differs is the boss's idle pose (the animation's phase: 9.3 % of the frame
at +57, the box `0,8 .. 136,108` around it), the bystander at the top left
(the scene's: lying in the route, standing in the staging) and the HP
digits (the party's 999 pokes). Before the `view` the boss and party were
off screen (`sheet_t2_death.png`); before the `pre-poke`, the boss
invisible through the rounds (`sheet_t4b.png`).

### 7.3 The fights on the PC install

The routes cut three frames before their own call (the staged rounds
Defend, so the fights run longer than the owner's); `--play 3 --kill`
unless the row says otherwise. Fights 28..55 start from step 2's crafted
saves (the coordinator's note; the probe `bossfight_probe.txt` of branch
`boss-sweep/prefight-saves` with its `# save` line changed, copies in the
root's `s2\`, the saves in its `saves\`), the op three frames before
their scene's call (38: the scene's own call, `--scene`, since frame 781
sat in a talk). Frames are recipe frames. Hooks: end /
exit / event as the set-up stored them - the same in the route's own
fight for all ten (the references' `hooks` watch).

| Fight | Boss (kinds) | Staging | Mode 5 | End | Return | Hooks | Enemy 0's state bytes (first byte.second) | What the frames show |
|--:|---|---|--:|---|--:|---|---|---|
| 2 | Nue (1) | `cutsceneAndNue` cut 4233 | 4237 | win 5447 (the Nue's "Escape"; no enemy defeated) | 5574 | `0x4381E0` / `0x437CC0` / `0x4381B0` | 2, 3, 5.0-1, 8.0-2, 3.1, 6.1.0-2, 9.1.0.0-3 | the Nue flees as in the route (`end_pc_nue.png`); no post-fight scene |
| 7 | Worker, Operator (4, 5) | `whelpBoss` cut 711, `--play 0 --to-end` | 715 | neither bit: the set-up's script ends it at 959 | 959 | `0x438DD0` / `0x438E30` / `0x438C50` | 2, 3, 3.1, 5.0-1 | a scripted fight of one exchange; `--play 2` ran out of rounds (the seek gave up) |
| 8 | kind 8, one enemy (the image of Torast, Kassen, Galtel, Doksen: boss-fights.md) | `bossAndFlash` cut 1747, kill 0..3 | 1751 | win 2699 | 2786 | `0x439260` / `0x439280` / `0x43C9F0` | 2, 3, 7.1-2, 8.0-2, 5.0-1, 3.1, 6.1.0-2, 6.4.0.0-3, 6.4.1 | the boss's death; effect records 1 and 2 change after the return, as in the route's own run |
| 12 | Amalgam (12) | section 7.2 | 521 | win 2006 | 2098 | `0x439900` / `0x437CC0` / `0x43C9F0` | 2, 3, 5.0-1, 7.1-2, 8.0-2, 3.1, 6.1.0-2, **6.4.0-3** | **the stretch and the melt** (section 7.4) |
| 13 | Balio, Sunder (13, 14) | `balioAndSunder_1` cut 2259, `--mortal --to-end` | 2263 | **loss** 3551 (HP `0xFFFF`, the hit hooks) | 3599 | `0x43A000` / `0x438E30` / `0x43C9F0` | 2, 3, 7.1-2, 8.0-2, 5.0-1, 3.1, 6.1.0-2 | Lightning on the party, the loss and the scene after it (`end_pc_balio13.png`) |
| 16 | Balio, Sunder, Nina (13, 14, 17) | `balioAndSunder_2` cut 17194, kill 0..2, 3 rounds | 17198 | win 20226 (Sunder's "Escape") | 20271 | `0x43A190` / `0x43A230` / `0x43A060` | 2, 3, 7.1-2, 8.0-2, 5.0-1, 3.1, 6.1.0-2, 6.3.0-4, 9.1-2 | the HP-1 hooks hold; the set-up's event hook ends it with the escape (`end_pc_balio16.png`) |
| 23 | Bully 1..3 (28) | `stallion` cut 1939, kill 0..2 | 1942 | win 3749 | 3816 | `0x43B730` / `0x43B750` / `0x43C9F0` | 2, 3, 5.0-1, 3.1, 6.1.0-2, 6.4.0-3 | the three fall one by one (`end_pc_stallion23.png`) |
| 24 | Stallion (29) | `stallion` cut 10574 | 10577 | win 11396 | 11466 | `0x43B890` / `0x43B8C0` / `0x43C9F0` | 2, 3, 7.1-2, 8.0-2, 6.1.0-2, 3.1, 6.4.0-3 | the death the owner's route never reached (it ends mid-fight) (`end_s24.png`) |
| 29 | Dolphin (34) | `dolphinBoss` cut 9955 | 9958 | win 10917 | 10992 | `0x43CE70` / `0x43A4A0` / `0x43C9F0` | 2, 3, 5.0-1, 7.1-2, 8.0-2, 3.1, 6.1, 6.3.0-4, 6.1.1-2, 6.4.0-3 | the dolphin's sink (`end_pc_dolphin29.png`) |
| 32 | D.Zombie (38) | `minecartToZombieDragon` cut 5230, `--after-confirm` | 5233 | win 6125 | 6195 | `0x43D270` / `0x43D3A0` / `0x43C9F0` | 2, 3, 8.0-2, 3.1, 6.1.0-2, 6.4 | section 7.5 |
| 28 | Gazer (33) | step 2's `midEyeEncounter` probe cut 437 | 442 | win 1553 | 1974 | `0x43CA00` / `0x43A4A0` / `0x43C9F0` | 2, 3, 4.1-2, 5.0-1, 8.0-2, 3.1, 6.1.0-2, 6.4.0-3 | the camera as the scene leaves it: the Gazer at the top left, the party off screen (`end_pc_gazer28.png`) |
| 38 | Ammonite (45, two) | step 2's `bf38c` probe, `--scene` cut 800 (the scene's own call at 865) | 866 | win 2386 | 2636 | `0x43EED0` / `0x437CC0` / `0x43C9F0` | 2, 3, 7.1-2, 8.0-2, 6.1.0-2, 3.1, 6.3.0-4, 6.4.0-3 | the camera faces the sea (step 2's note), the enemies out of view; the result screen as usual |
| 50 | Chimera (57) | step 2's `bf50c` probe cut 569, `--play 0 --confirm cross` | 572 | win 945 | 1933 | `0x43F380` / `0x437CC0` / `0x43C9F0` | (kill at the first cross) | adult Ryu alone (the donor's party); the donor save confirms with cross, and with circle the kill's seek gave up at the cross; with `--play 3` the fight was **lost** at 739 (the lone member disabled: no cross is left) |
| 54 | Shroom (61) | step 2's `bf54b` probe cut 437 | 442 | win 1443 | 1499 | `0x43FC60` / `0x437CC0` / `0x43C9F0` | 2, 3, 5.0-1, 7.1-2, 8.0-2, 3.1, 6.1.0-2, 6.4.0-3 | the enemy out of the camera's view (step 2's note) |
| 55 | Myria (62) | step 2's `bf55b` probe cut 476 | 479 | win 1561 | 1589 | `0x440700` / `0x440820` / `0x43C9F0` | 2, 2.1, 3, 7.1-2, 8.0-2, 6.1.0-2, 3.1, 6.4 | Myria above the stage; her death one frame of `6.4`, the field after it (`end_pc_myria55.png`) |

`6.4.*` is the common death the sweep sees most (state 6 the hit, step 4
its ending); the Amalgam's `6.4.2` holds 43 frames, its melt (D317's
`_DeathMelt`). The fights whose bosses carry hit hooks or event hooks
(2, 7, 13, 16) end by the set-up's script, not by the kill.

### 7.4 The Amalgam's death: the melt shows (D317, DIV-0142 / DIV-0159)

The route's own death (`ref_amalgam2`, frames 7600..7950 every 3) and the
staged one (`amalgam_t5`) aligned on the first frame of state `6.4`
(route 7668, staged 2060): the hit, then from +2 the body **stretches into
vertical streaks, red and green, and sinks into a column** that thins to a
line and is gone at +46 (`sheet_death_a.png`: +0, +2, +12, +24, +36, +45),
"You defeated a powerful enemy!" at +60 in both. The streaks are the page
`(0x340, 0x100)` drawn by `BossAmalgam_DrawStreak` - the picture DIV-0142
(the capture into VRAM) and DIV-0159 (the direct-colour texels' alpha)
restored together; D317's "scoots back and freezes" and D333's empty melt
are not seen. The staged and the route's frames differ only where the
fights differ (Ryu fallen in the owner's fight, the HP digits, the damage
popup's number, the bystander).

### 7.5 The D.Zombie, the pool, and the departure after the fight

- **In the fight** the red pool under the dragon is part of the field
  behind the battle: drawn in the route's fight and the staged one alike,
  to the pixel apart from the actors (`pool_pair.png`, `end_dz1.png`). The
  death is one frame of state `6.4` and the win; nothing is drawn for the
  dragon's death in battle - the dragon stays on its pool.
- **After the return** the staged run stands in the field with the pool and
  the dragon until the end (`end_dz3.png`, 1,100 frames of confirms): the
  scenario does not resume (7.1.3). **The departure is the route's alone**
  (`ref_dzombie2`, the route with timed frames 5560..6400): the talk
  (mode 4 at 5732, 5905), then at 6044 the scene's effect record 0 takes
  **kind 0x51** (`010B00000051`: in use, state 0xB, `_RingClose` - [`effect_2f.md`](effect_2f.md) 1.3) and the
  frame shows - from 6045 - the dragon and the pool gone, then a disc of
  the field picture filling the screen at 6048..6054 and closing to a point
  by 6066 (`dz_fx.png`), released at 6064; the field without them from
  6069.
- So the "blood / oil pool" the owner reports is **kind 0x51** - an effect
  the chapter-8 scene spawns in the field after the battle has returned,
  not the boss's code: the owner's hypothesis ("area logic after the
  return") stands. It is the effect DIV-0125's amendment of 2026-10-25
  (issue #142) fixed: its column's two grabs into one page, "the puddle
  that spreads into the pool and the circle that closes on the way out".
  In these frames **the circle is whole** - both halves, no straight left
  side, no seam - so what DIV-0125's amendment describes is what plays now.
  Whether the disc's picture inside the circle should be sharper than the
  soft, blurred field it shows here is not settled by these frames: the
  owner's PlayStation frames (named in DIV-0125's amendment) are the
  reference for that, not read here. The puddle's spread is earlier in
  the route (frames 2760..3360 of `minecartToZombieDragon`, DIV-0125's
  verification frames), before the fight - not in this sweep's window.

### 7.6 The slot question (step 1's discrepancy) settled

Enemy 0's `+0xF0` is the area slot `Battle_CopyEnemyData` was given, and
is written by the set-up, 57 frames after mode 5. Section 11.4 of
[`spell-sweep.md`](spell-sweep.md) took its prologue peeks at the first
frame of mode 5 (the `until` passes at the route's own call), **before the
set-up** - its logs read `enemies = 0` there for the Amalgam and the
Stallion - so its slots are what the previous fight left: the Amalgam
**1** (not 0), the D.Zombie **2** (not 3) and the Stallion **0** (not 1),
read 65 frames in on every install here, and the census's own carriers
agree (`kind_census.json`: Amalgam 28 / 1, D>Zombie 108 / 2, Stallion 67 /
0). spell-sweep.md 11.4's table and its parenthesis want those three
corrected; its casts are unaffected (the casts poke the ability list, not
the slot). Noted for its owner; this doc's 5.2 already reads 1.

### 7.7 Three installs

The same recipes on a **US-PSX-only** install (`abc_run.py prepare --only
psx-us`, `en-US`, the gap watch on, `gap,unread,language`) and a
**PSP-EU-only** install (`--only psp-eu`, `en-150`), both built from this
branch's tree into the root's `abc\` (a scratch installs file naming
`D:\bof3x-runs\discs`), run through the root's launcher copy with
`BOF3X_EXEIMAGE` and `BOF3X_CACHE` naming the cache. **Every fight reached
the same recipe frames on all three installs** - the call, mode 5, the win
or the loss, the return, to the frame (the `results_fight.tsv` rows of
`pc\`, `us\` / `psxus\` and `psp\` / `pspeu\`). The pictures, every frame
the runs wrote (the watch shots and the timed frames, matched by recipe
frame; `cmp_all.py`, `abc_compare.py`'s tolerance 4 / 16 pixels):

| Fight | Frames | PC - US | PC - PSP | US - PSP |
|---|--:|--:|--:|--:|
| 2 Nue | 518 | 0 | 0 | 0 |
| 7 Worker, Operator | 211 | 0 | 0 | 0 |
| 8 kind 8 | 350 | 0 | 0 | 0 |
| 12 Amalgam | 389 | 0 | 0 | 0 |
| 13 Balio, Sunder | 570 | 0 | 0 | 0 |
| 16 Balio, Sunder, Nina | 1,322 | 0 | 0 | 0 |
| 23 Bullies | 606 | 0 | 0 | 0 |
| 24 Stallion | 332 | 0 | **201** | **201** |
| 29 Dolphin | 391 | 0 | 0 | 0 |
| 32 D.Zombie | 549 | 0 | 0 | 0 |
| 32, the route's own run (the departure) | 309 | 0 | 2 | 2 |
| 28 Gazer | 443 | 1 | 1 | 0 |
| 38 Ammonite | 568 | 1 | 1 | 0 |
| 50 Chimera | 593 | 172 | 172 | 0 |
| 54 Shroom | 314 | 1 | 224 | 223 |
| 55 Myria | 400 | 1 | 1 | 0 |

(Fight 7's 21 frames on one side only are the PC root's first, failed
attempt's, left in the folder.) Every difference, read:

- **Fight 24, PSP: the Stallion is brown and blue** where the PC and the
  US disc draw it pale (`stallion_psp.png`, the box round the boss on
  every frame until it falls). The PSP release's recolour -
  [`psp-stallion.md`](psp-stallion.md): two 16-colour rows of area 67,
  data only - which a PSP-only base draws as its own (DIV-0103's area art).
  `recipes/abc-known.toml`'s `psp-area-art` rule covers mode 2 only, so
  the comparer calls these frames unexplained; a battle-mode twin of the
  rule would explain them. Not a defect.
- **Fight 54, PSP: two small patches of the lab's wall** (0.5 % of the
  frame, `shroom_psp.png`) - the PSP's own area pages in battle, the same
  class as fight 24.
- **The D.Zombie route, PSP: two frames of the ability window's L1 / R1
  page icons** (`dz_psp.png`) - the PSP layer's button glyphs (the button
  round's class).
- **Fights 28, 38, 50, 54, 55, PC against both disc installs: `f00388`**,
  the probe's shot of the load screen - the PC's game folder lists the
  owner's other slots, the disc installs' folders hold none. The harness's.
- **Fight 50, PC against both: the party's name box** (`chim.png`): the
  crafted save's donor (`adult_ryu`) names Ryu with a glyph the PC's font
  draws and the disc installs' fonts leave blank - the save's, the class
  of D326. The harness's.

So **no boss death, end hook, exit hook or return differs between the PC
install, the US disc and the PSP disc** in these fights beyond the PSP's
own colours and pages. The D.Zombie's departure (section 7.5) is the same
picture on all three.

**The gap watch** over the US-disc runs (`gapwatch.py report` over
`gw_psxus`, 26 runs - the names of the runs, the reruns writing over the first): 11 of 236 ranges read in some
run, 8 before any write - every one the class the spell sweep's runs list:
the empty save slot (`Encounter_SlotChance+0x1C`), `Battle_CommandLabels`
and `Menu_Verbs+0xF4` (the parked-page blind spot), `Equip_SlotIcons+0xC0`
.. `+0xF8` (the `stallion` route's field menu), and the Config rows and
`Menu_Verbs` / `Effect1b_ToggleText+0x60` the `balioAndSunder_2` route's
own Config visit reads ([`install-gap-reads.md`](install-gap-reads.md)).
The PSP-EU-only runs (`gw_pspeu`, 26 runs, 217 ranges) read the same
eleven. **No `gap` row is read by a boss's death, a hook or a return.**

### 7.8 The carried skills, cast by their own bosses

`--ids` on each fight (the enemy side's pokes on enemy 0, its odds and
flags from the prologue's peeks), then the kill, on all three installs:

| Id | Skill | Boss (fight) | Picked (`+0x106`) | Notes |
|--:|---|---|---|---|
| 1, 9 | Nue Stomp, Chlorine | Nue (2) | 1, 9 | |
| 17 | Astral Warp | Amalgam (12) | 17 | |
| 57, 81 | Bone Dance, RottenBreath | D.Zombie (32) | 57, 81 | |
| 86, 116 | Barrier, UtmostAttack | Stallion (24) | 86, **97 (Jolt)** then, with the AI rows off, 86, 116 | the Stallion's AI rows are live in a staged fight (the by-kind sweep's `--kind` switches them off in the area's data; the fight mode does not): with them, it picked Jolt over the poked list. `--pre-poke` of the four rows' conditions (`0x8C5600 + 16 r` = `0x63`, area 67's slot 0) makes the pick the list |
| 128 | Head Cracker | Shroom (54) | 128 | **section 2.1's skill** |
| 129, 130 | Holocaust, Venom | Myria (55) | 129, 130 | **2.1's** |
| 138 | Main Cannon | Ammonite (38) | 138 | **2.1's** |
| 139 | Paralyzer | Chimera (50) | 139 | **2.1's**; the donor's lone member paralysed, "Your party was wiped out" - the loss, no kill (a party of one, the craft's) |
| 150 | Eye Beam | Gazer (28) | **27 (Hypnotize)**, then with the AI rows off (`0x8C5718 + 16 r`, area 77's slot 2) 150 | **2.1's** |

**Six of section 2.1's seven boss-only skills are cast by their own
bosses** on the PC, the US disc and the PSP disc (`casts_boss.png`; Holocaust's beams from its fifth shot, `holo.png`);
Corona (Gaist, fight 35) was not reached then; it is cast by its own boss on the PC since 2026-10-29 (section 8). The pictures across the installs:
0 frames differ for the routed casts but the PSP's Stallion colours (207
frames, as 7.7) and for the crafted ones but the load screen and the
name box (7.7). Several of the crafted fights frame the enemies out of
view (the scene's camera: step 2's notes), so a cast's own art on the
boss is not always in the shot - Head Cracker and Main Cannon among them.


### 7.9 For the coordinator

- No new defect is established by these frames: every death and return
  played on all three installs as on the PC, and the two the owner named
  are what the fixes (DIV-0142 / DIV-0159 for the Amalgam, DIV-0125's
  amendment for the D.Zombie's kind 0x51) describe. Nothing here is a
  known-defects.md entry.
- The D.Zombie's departure circle (7.5): the owner's eye on `dz_fx.png`
  against their PlayStation frames, if the report was about the picture
  *inside* the circle rather than its shape.
- spell-sweep.md 11.4: the three slots of 7.6.
- `--goto` staging leaves area actors hidden until the scene shows them
  (7.1.2); each new `goto`-staged fight wants its actors' `+0` read off the
  route's peeks (`s*_flags_before`) and poked.
- `recipes/abc-known.toml`: `psp-area-art` keys on mode 2; the PSP's own
  palettes and pages show in battle too (fights 24 and 54, 7.7). A mode-5
  twin of the rule is a one-line call for whoever owns the file.
- The fight mode leaves the boss's AI rows live (7.8): forced casts on a
  boss with AI rows want `--pre-poke` of its rows; an `--ai-off` option on
  the slot would make that one switch.

### 7.10 Not reached

- **Fight 35** (Gaist; Corona): the coordinator's note - no type-7 actor, skipped. Reached 2026-10-29 by the owner's route (section 8).
- **Chimera's kill** (fight 50): the cast of Paralyzer wipes out the donor's
  one-member party; the kill runs only without the cast.
- The post-fight scenes of staged fights (7.1.3) - the route's own run is
  the reference for those (done for the D.Zombie).
- Fight 7's own rounds: the set-up's script ends the fight after one
  exchange, so it is played `--to-end` without a kill.

### 7.11 The recipe for one fight

```
python tools/spell_sweep.py run --route tools/recipes/minecartToZombieDragon.txt --fight 32 --cut 5230 \
    --enemies 3 --play 3 --kill --after 1200 --after-confirm \
    --root <root> --launcher <ini-less launcher dir>\bof3x-launcher.exe --game <the game folder> \
    --lang en-US --speed 4 --env BOF3X_OPT=none --env BOF3X_LAYERING=0
python tools/spell_sweep.py run --route tools/recipes/ropeAndAmalgam.txt --fight 12 --cut 300 --field-cut 300 \
    --goto 28:0x270000:0x568000:3 --pre-poke "0x7DF3A0 u8 1" --camera 0x190000:0x5B0000 --place 0x1C0000:0x5B0000 \
    --enemies 1 --play 3 --kill ...
python tools/spell_sweep.py run --route R --fight N --reference --cut C --watch-from F --every-from A --every-to B ...
python tools/spell_sweep.py fightlog <root>\bof3x_<tag>.log --fight N
```

The cut: the frame three before the route's `fight` watch first changes
(a `--reference` run gives it). A fight a talk starts (the last text box's confirm runs into the
battle) is cut at its first mode-5 frame with `--scene` instead (section 8). A disc-only install as spell-sweep.md 10:
`--game`, `BOF3X_EXEIMAGE` and `BOF3X_CACHE` naming its cache,
`BOF3X_RECIPE_SAVES` the main checkout's saves.

## 8. Fight 35, the Gaist (2026-10-29)

ScrubOasis reported trouble in the Gaist fight, so the owner played to it.
Section 6.2's crafts stalled because the scene is started by walking and
talking in area 120, the cave town, not by any one run step:

- `Scena10_StepHook` starts chapter 10's run 0xD at step 0 on the town path
  (zone 0, x 0x58000, z within 4 cells of 0x11; flag 0x37 clear; a member of
  kind 2), and the run takes the party into the forge room.
- Scenario counter 0 is moved by area 120's own choice handlers
  (`Area120_ChoiceVar3A` / `_ChoiceVar3B` write `0x903848`), and choice 2
  (`Area120_ChoiceStartRun13`) sets run 0xD at step 8. Step 0x10's wait on
  counter 0 = 0xE is the talks' to satisfy, which a craft never does.

The owner started from the craft `bf35v` (`tools/chapter_jump.py`: area
120's town at that cell, chapter-10 flags 0x37..0x3D clear, adult Ryu and
Garr), played up to the room, saved, and recorded the route `geist`
(`tools/recipes/geist.txt`, save `geist`, en-US, the point filter). The
fight is adult Ryu alone (the frames).

**The crafts' own lesson:** a craft that sets the party must set the party
set too (block +0x74C, `AreaW0b_PartySet`, with its 0x80 bit): the load
brings in the sprites of that row of the set table at
`MoveScript_EffectArg + 0xC` (row 7: Garr, Rei, adult Ryu; row 0xB: Garr,
adult Ryu, ...), and a member outside the row runs `Field_MemberSprite`
off the bank table (a crash reading `0x041D1130` in the load). With the
row right, a party of adult Ryu, Garr and Rei in that order still crashed
the first field draw (Rei's record had no frame, `+0x54` 0); Rei second
loads (`bf35tx`). Not looked into further.

**The route, measured** (`--reference`, PC install `game\game`, the main
build of 2026-10-29): fight byte and mode 5 at recipe frame 1468, inside a
106-frame hold of circle from 1381 - the last text box's confirm runs
into the battle; kinds 41 (the Gaist, 2500 HP) and 42 (two Torches, 300
HP each); end / exit / event hooks `0x43E770` / `0x43E790` / `0x43E750`; won
at 4931, the field again at 5062.

**The fight mode.** A cut before 1468 sits in that text (mode 4, the
battle never comes), so fight 35 is staged with `--scene --cut 1468`: the
route kept to the battle's first frame. Two harness fixes came with it
(`tools/spell_sweep.py`): `route_steps` keeps a hold the cut falls in,
shortened (it dropped it, and the text box waited forever); and
`fight_summary` takes the watches' first values for the call and the
start when the run is set at mode 5 already. Played: three rounds of
Defend and the kill - start 1468, win 3499, field again 3579, the same
three hooks as the route's own fight.

**Corona.** `--ids 137 --play 1 --kill`: the Gaist picked 137 (`+0x106`
0x89 at 1584), the "Corona" box and the cast, then a Torch's Flare, the
kill and the return (win 3071, field 3149). **Seven of section 2.1's seven
boss-only skills are now cast by their own bosses** - Corona on the PC
install only so far; the US-disc and PSP runs of 7.7 are not done for it.

```
python tools/spell_sweep.py run --route tools/recipes/geist.txt --fight 35 --cut 1468 --scene \
    --enemies 3 --play 3 --kill --after 1200 --root <root> --launcher <launcher dir>\bof3x-launcher.exe \
    --game <the game folder> --lang en-US --speed 4 --env BOF3X_FILTER=point
```

### 8.1 The route on the disc installs, and the ash (issue #163)

The same route, `--reference --cut 7868` (frames 4931 / 5062 are the PC's
win and return):

| Install | Win | Field again | Note |
|---|--:|--:|---|
| PSX-US alone (`abc_run.py prepare --only psx-us`) | 4931 | 5062 | as the PC |
| PSP-EU alone (`--only psp-eu`, `en-150`) | 4931 | 5062 | as the PC |
| US-PSX + EU-PSP (`D:\bof3x-runs\i130`, the reporter's shape) | 6228 | 6524 | **the fight runs differently** with the same inputs - its PSP data layers, not wide (6228 with wide off too) |

None crashed. Issue #163 (ScrubOasis, the crash after the fight) is
D217's kind 0x6B overrun, read on a build of the reporter's commit: see
[`known-defects.md`](known-defects.md) D217, which holds the measurements
(600 particles everywhere with this route, the PlayStation included) and
why: the ash effect captures `Sprite_Objects` record 2, here a Torch's
freed battle-tagged record behind the right torch, so the ash falls there
(seen in wide) while the Gaist switches to his heap pose - the
PlayStation does the same. A let's play shows the crumble on the Gaist,
so record 2 depends on the history, and `geist` descends from a crafted
chapter-14-donor save. Two harness notes from the day: the default
`bof3\` install's en-US overlay files are stale against the current
build (`labels chunk tag is 0x18`), so runs used the play install; and
the shot frames are at recipe frames 6766..6900, the ash's count set at
6772.
