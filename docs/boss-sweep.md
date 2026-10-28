# The boss sweep: every boss's moves, by its own fight

**Status:** PROPOSED; step 2 MEASURED, section 5 (2026-10-27 evening, the owner's ask before the staged
rounds: "update the spell/item harness to also test boss moves"; the owner's
suspicion that the kind injections of [`spell-sweep.md`](spell-sweep.md) 11
could swap the enemy kind and graphic in combat so moves like Nue Stomp render
from the right body. Section 1 answers that from the code; sections 2 to 4 are
the plan; the runs and findings will follow as sections.)

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

## 5. Step 2: the pre-fight states

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

- **Fight 35** (Gaist, Torch, area 120; Corona): section 5.2.
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
