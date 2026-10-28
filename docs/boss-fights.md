# The event battles: who calls each fight, where, and from which scenario state

**Status:** MEASURED where a row says so (headless runs of 2026-10-27,
[`boss-sweep.md`](boss-sweep.md) "Step 2"), READ elsewhere (our
`scena_sc*.cpp`, cited by file and line). Step 2 of the boss sweep: the
table a `battle ID` recipe op and a crafted pre-fight save are built from.
No game data here: the fight records and the areas' rows stay in the
harness root; this page names fight ids, areas, chapters, runs and steps.

## 1. How the table was made

- **The call sites.** Every call of `Field_StartEventBattle` (`0x4410B0`)
  in `src/game/scena_sc*.cpp`, direct or through a chapter's helper
  (`Battle(id)` in `scena_sc2.cpp:101`, `scena_sc5.cpp:133`,
  `scena_sc6.cpp:117`; `EventBattle(id)` in `scena_sc0.cpp`; `kSeHelper`
  in `scena_sc1.cpp`; `LeaderState5` in `scena_sc3.cpp:101`;
  `BattleBytes()` = fight 38 in `scena_sc12.cpp:1329`; `FlagBattle` in
  `scena_sc5.cpp:1089`; the table `Scena15_Battles` `0x6618C0` in
  `scena_sc15.cpp:612`): 53 sites, every fight 1..55 called at least once
  (a script of the boss-sweep root, `py/sites.py`).
- **Chapter numbering.** The chapter is block byte 0 (`Cond_ByteFA`,
  `0x8034E0`); `Scenario_Hooks` (`0x662C80`) holds one hook table per
  chapter, and entry *c* of it is `ScenaNN_Hooks` with NN = *c* for every
  *c* in 0..19 (the twenty dwords read off the PC exe and matched to
  `symbols.toml`'s `ScenaNN_Hooks` addresses, 2026-10-27). So a function
  named `ScenaNN_*` is chapter NN's, whatever file holds it:
  `scena_sc3.cpp` holds chapters 3 and 4, `scena_sc7.cpp` 7 and 8,
  `scena_sc9a.cpp` 9, `scena_sc9b.cpp` 9's tail and 10, `scena_sc13.cpp` 13
  and 14, `scena_sc15.cpp` 15..19.
- **Run and step.** State 2 (`Run`) dispatches `ScenaNN_Runs[MoveScript_Var7]`
  (`0x8034E4`, block byte 4); the run switches on its step
  `0x8034E5` (block byte 5). Each run's index was checked against the
  chapter's `Runs` table in the exe (the function's `original` address).
  The step in the table is the `case` that calls the fight.
- **Enemies.** `tools/boss_rows.py --disc` (the US disc's area records name
  every kind; a set-up's kinds are the ones its image introduces) - its
  names, not memory of the game. Where an image serves several fights the
  area of the call site picks the enemy.
- **Actors.** No run creates a `Sprite_Objects` record of type 7 or writes
  a tag `+0x9E` before its battle step: the steps call `Party_DropIn`,
  `Kind2_Place`, `Party_PlaceForBattle` (`0x532ED0`) and effects
  (`Effect_FindFree`, a different array) only. The only sprite spawners in
  the scenario files are `Scena03_SpawnAtMember` (chapter 3 run 8),
  `Scena07_PlaceObjects` (`scena_sc7.cpp:735`, after fight 31),
  `Scena08_SpawnPair` (chapter 8 run 8), `Scena10_SpriteOp`
  (`scena_sc9b.cpp:2047`, chapter 10 run 3), `Scena13_SpawnPair*`
  (`scena_sc13.cpp:458`) and `Scena15_EventObjects` (`scena_sc15.cpp:1035`,
  run 4) - none on a fight's path. Tags are written by the areas' own event
  scripts (`EventOp_7x` stores the op's byte `+0x10` at `+0x9E`,
  `field_o.cpp:1141`). So **the tagged actors are the area's own objects**;
  measured for fights 28, 38, 39..48, 50, 54, 55 ([`boss-sweep.md`](boss-sweep.md)
  5). Fight 35 is the exception measured: area 120's two figures stand as
  type-1 objects until the scene has run (the same section).
- **Routes.** The `boss encounter: id N` lines of the logs under
  `C:\Users\kerok\bof3x-runs` (the owner's routes run since 2026-10-23).

## 2. The table

`ch` chapter, `run`/`step` the state at the call, area decimal (hex in the
code). "Start" is what the code does to begin the run (or the part of it
holding the fight); "route" names an owner's recipe whose log has the fight.

| Id | Enemy | Area | ch | run | step | Call | Start (read) | Route |
|--:|---|--:|--:|--:|--:|---|---|---|
| 1 | Gary, Mogu | 24 | 0 | 5 | 0xF | `scena_sc0.cpp:703` | `Scena00_Start` / run 4 step 0xA; last change step 0xA `(0x18, 0x630000, 0x110000, 3)` (`:663`) | `battle_commands` |
| 2 | Nue | 23 | 1 | 8 | 0x39 | `scena_sc1.cpp:787` | EnterArea with counter 2 = 7 (`mode_states.cpp:728`), set by the step hook's `ChangeArea(0x17, 0x180000, 0x180000, 0x82)` (`:832`) | `cutsceneAndNue` |
| 3 | Nue | 22 | 1 | 9 | 0x24 | `scena_sc1.cpp:1029` | EnterArea by flags 0xB..0x10 (`mode_states.cpp:666`); last change step 0x18 (`scena_sc1.cpp:963`) | - |
| 4 | Engineer, Foreman, Miner | 24 | 0 | 6 | 0xB | `scena_sc0.cpp:747` | step hook, area 24, x cell 0x14, zone 0 (`:1168`) | - |
| 5 | Engineer, Foreman, Miner | 24 | 0 | 6 | 0x15 | `scena_sc0.cpp:768` | `Scena00_Area18` on entry, zone 2, flag 8 clear (`:320`) | - |
| 6 | Engineer, Foreman, Miner | 24 | 0 | 6 | 0x1F | `scena_sc0.cpp:791` | the same, zone 4, flag 9 clear | - |
| 7 | Worker, Operator | 2 | 0 | 7 | 1 | `scena_sc0.cpp:827` | step hook, area 2, x cell 0x2B, flag 6 (`:1161`) | `whelpBoss` |
| 8 | Torast, Kassen, Galtel, Doksen | 27 | 2 | 0xA | 8 | `scena_sc2.cpp:1022` | step hook rectangle, flag 0x10 clear (`:2603`) | `bossAndFlash` |
| 9 | the same | 27 | 2 | 0xC | 2 | `scena_sc2.cpp:1150` | step hook, flag 0x11 clear (`:2610`) | - |
| 10 | the same | 27 | 2 | 0xD | 3 | `scena_sc2.cpp:1192` | step hook, flag 0x13 clear (`:2617`) | - |
| 11 | the same | 27 | 2 | 0xE | 4 | `scena_sc2.cpp:1245` | step hook, flag 0x14 clear (`:2627`) | - |
| 12 | Amalgam | 28 | 2 | 0x11 | 0x12 | `scena_sc2.cpp:1547` | step hook, flag 0x19 clear (`:2641`); flag 0x15 set | `ropeAndAmalgam` |
| 13 | Balio, Sunder | 11 | 2 | 0x13 | 0x10 | `scena_sc2.cpp:1717` | EnterArea, flag 0x1D clear (`:279`); last change step 0xF (`:1705`) | `balioAndSunder_1` |
| 14 | Rocky | 26 | 2 | 8 | 2 | `scena_sc2.cpp:812` | step hook, flag 0x32 set, 7 clear, `ChangeArea(0x1A, 0x70000, 0xD0000, 0x80)` (`:2572`) | - |
| 15 | Pooch | 26 | 2 | 0x1A | 0x15 | `scena_sc2.cpp:1995` | `StepHook1A`, flag 0x30 clear (`:2513`) | - |
| 16 | Balio, Sunder, Nina | 41 | 3 | 3 | 0xB | `scena_sc3.cpp:447` | `Scena03_StepArea29`, flag 7 clear (`:1413`) | `balioAndSunder_2` |
| 17 | Mutant | 52 | 5 | 4 | 3 | `scena_sc5.cpp:596` | step hook, flag 5 clear (`:221`); `ChangeArea(0x34, 0x3E0000, 0x270000, 0x84)` at step 0 | - |
| 18 | Claw, Cawer, Patrio, Dodai | 79 | 5 | 0x10 | 0x18 | `scena_sc5.cpp:1300` | object trigger 5 (`:313`); also `FlagBattle` at steps 0x38 / 0x42 with flag 0x1E clear (`:1089`, `:1385`, `:1405`) | - |
| 19 | the same image | 79 | 5 | 0x10 | 0x21 | `scena_sc5.cpp:1330` | the same run; `FlagBattle` with 0x1E set, 0x1F clear | - |
| 20 | the same image | 79 | 5 | 0x10 | 0x27 | `scena_sc5.cpp:1351` | the same run; `FlagBattle` with both set | - |
| 21 | Emitai, Golem | 81 | 5 | 0x11 | 0x18 | `scena_sc5.cpp:1605` | object trigger 6 (`:316`); last change step 0x16 (`:1591`) | - |
| 22 | Garr | 80 | 5 | 0x12 | 0xA | `scena_sc5.cpp:1736` | object trigger 7 (`:319`); last change step 6 (`:1705`) | - |
| 23 | Bully 1..3 | 67 | 6 | 0xC | 4 | `scena_sc6.cpp:1505` | step hook, area 67, flag 0 clear (`:2270`) | `stallion` |
| 24 | Stallion | 67 | 6 | 0xC | 0xE | `scena_sc6.cpp:1559` | the same run, after fight 23 | `stallion` |
| 25 | Beyd, Zig (image) | 92 (inferred) | 6 | 8 | 2 | `scena_sc6.cpp:1182` | no starter in chapter 6's code (a field script's op F6, not traced); the run re-enters 92 after the fight (`:1185`) | - |
| 26 | the same image | 92 (inferred) | 6 | 8 | 0xC | `scena_sc6.cpp:1208` | the same run | - |
| 27 | (image without kinds of its own) | 92 | 6 | 9 | 5 | `scena_sc6.cpp:1320` | not found in code; `Run09Enter` `ChangeArea(0x5C, 0x180000, 0x3D0000, 0x89)` (`:1277`) | - |
| 28 | Gazer | 77 | 6 | 0xE | 0x1E | `scena_sc6.cpp:1774` | step hook, area 77, flag 0x27 set, 0x2A clear (`:2273`) | owner's save `midEyeEncounter` (`boss-sweep.md` 5.2) |
| 29 | Dolphin | 175 | 7 | 2 | 0x1A | `scena_sc7.cpp:474` | step hook, area 175, flag 0 set, 1 clear (`:1303`) | `dolphinBoss` |
| 30 | Gisshan, Scylla, Charyb | 103 | 7 | 4 | 0xA | `scena_sc7.cpp:661` | arrive hook, area 103, flag 3 clear (`:1319`) | - |
| 31 | Garr | 85 | 7 | 6 | 0x12 | `scena_sc7.cpp:876` | `Scena07_Cell0` (`:1342`) / EnterArea / step hook (`:1279`) | - |
| 32 | D>Zombie | 108 | 8 | 3 | 0xC | `scena_sc7.cpp:1780` | no starter in chapter 8's code; area from the route | `minecartToZombieDragon` |
| 33 | Weretigr | 35 | 8 | 6 | 6 | `scena_sc7.cpp:1989` | arrive hook, area 35, flag 8 clear (`:2721`) | - |
| 34 | Mikba | 43 | 8 | 0xB | 0xA | `scena_sc7.cpp:2514` | arrive hook, area 43, flag 0x14 set, 0x15 clear (`:2731`) | - |
| 35 | Gaist, Torch | 120 | 10 | 0xD | 0x13 | `scena_sc9b.cpp:1998` | arrive hook, area 120 at (0x250000, 0x748000), zone 5, flag 0x3B set, 0x3C clear (`:2375`) | - |
| 36 | Angler | 75 (by the tool; the run returns to 75 after it, `:1788`) | 10 | 5 | 6 | `scena_sc9b.cpp:1783` | no starter in chapter 10's code (a field script) | - |
| 37 | Elder | 144 | 13 | 7 | 0x1A | `scena_sc13.cpp:1177` | EnterArea, area 144, zone 2, flag 0x17 clear (`:255`); step 0x14 from a field script | - |
| 38 | Ammonite | 134 | 12 | 9 | 0x16 (and 0x1F) | `scena_sc12.cpp:1440`, `:1450` | step hook, area 134, flag 1 clear (`:220`) | - |
| 39..48 | Sample 1..12 | 158..166 | 15 | 2 | 3 | `scena_sc15.cpp:612` | EnterArea of areas 158..166 by zone and flags 0x20..0x2F (`:366`..`:401`) through `Scena15_BattleSetup` (`:459`); battle index *n* = fight 39 + *n* | - |
| 49 | Manmo | 196 | 14 | 2 | 8 | `scena_sc13.cpp:1824` | `Scena14_LeaveToC4` from area 189's walk handler (`area_w4e.cpp:670`) | - |
| 50 | Chimera | 197 | 14 | 5 | 0xE | `scena_sc13.cpp:2269` | `Scena14_EnterArea`, area 197, arrival from zone 5, flag 0xF clear (`:1606`) | - |
| 51 | Arwan | 142 | 14 | 7 | 0xD | `scena_sc13.cpp:2689` | arrive hook, area 142, zone 3, flag 0x1F clear (`:2899`) | - |
| 52 | D>Lord | 172 | 14 | 7 | 0x18 | `scena_sc13.cpp:2753` | the same run after fight 51; `ChangeArea(0xAC, 0x208000, 0x268000, 0x89)` at step 0xF | - |
| 53 | HugeSlug | 119 | 9 | 8 | 0x24 | `scena_sc9a.cpp:1107` | EnterArea, area 119, zone 2, flag 0x3D set, 0x16 clear (`:333`) | - |
| 54 | Shroom | 119 | 9 | 9 | 3 | `scena_sc9a.cpp:1154` | step hook, area 119, flag 0x1E clear (`scena_sc9b.cpp:283`) | - |
| 55 | Myria | 198 | 15 | 6 | 0xD | `scena_sc15.cpp:1363` | run 5's step 0x24 `ChangeArea(0xC6, 0x160000, 0x120000, 0x80)` (`:1223`) | - |

Every scene's counter-0 waits (block `+0x20`, `0x903848`) are answered by
the area's field scripts (movement-script ops A0..AF,
`move_script.cpp:397`), which `Party_DropIn` usually starts; the
pre-fight steps and their waits are in the readers' notes in the harness
root (`C:\Users\kerok\bof3x-runs\bsw1027\saves\notes_partA.md`). Two traps every craft meets:

- **`Cond_ByteFD` (`0x8034F1`) is not in the save**: `Area_Enter` sets it
  from the arrival cell's zone (`Area_ZoneIdAt`). A craft chooses it by its
  x, z. Measured: (0x120000, 0x180000) is zone 2 in each of areas 158..166.
- **EnterArea runs on the load** and re-arms runs by flags and zone: a
  craft sets the flags that keep it from overwriting the saved run and step
  (the `cset` / `cclr` of `tools/chapter_jump.py`'s `bf*` crafts).
