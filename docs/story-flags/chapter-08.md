# Story flags: chapter 8

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 8's row is `Cond_Flags + 0x40` (`0x903FD0`), save block `+0x5F0`. Source:
`src/game/scena_sc7.cpp` (the `Scena08_*` functions). The area code that reaches the
current row in this chapter's areas is also listed: `Area12_WaitFlagE`
(`area_w0a.cpp`), `Area21_ChoiceTrade` (`area_w0b.cpp`), `Area34_ChoiceTail5`
(`area_w0c.cpp`), and area 108's tail (`area_w2f.cpp`), which starts run 3. The
`Scena08_*` functions in `scena_se.cpp` and `scena_calls.cpp` (`Scena08_PartyJoin784`,
the call-table party entries) touch no flag.

The file's wrappers are `Flag(n)` and `SetFlag(n)` around `Bank()`
(`scena_sc7.cpp:115-117`). Several sites call `Flags_Set(Bank(), n)` directly. Event
battle names are from [`../boss-fights.md`](../boss-fights.md) (fights 32..34). "Zone"
is `Cond_ByteFD`.

## Start

**What starts the chapter.** `Scena07_Scene6` case `0x27` (`scena_sc7.cpp:1078-1089`)
calls `Field_SetStatus80` and changes to area `0x20` (`0x900000, 0x550000`). On that
entry `Area_Enter` (`mode_flow.cpp:548-556`) steps the chapter to 8 and calls
`Scenario_Start(8)`. That call zeroes the row's first dword (flags 0..31) and the
scenario bytes, and runs state 0.

**`Scena08_Start`** (state 0, `:1378-1385`):

- state 1;
- the row's first dword zeroed again (`:1380`);
- the four script counters `0x903848..0x90384B` zeroed;
- `Scena08_PartyBitsToRecord0`: bit 0 of `+0xB` set on the eight character records,
  the party refreshed, then the bit cleared on records 1..7, so record 0 alone keeps
  it;
- `Scena08_SwapKeyItem4`: every key-item byte 4 becomes `0xE`;
- event flag `0x43` set (`:1384`).

It starts no run. Flags 32..63 are not cleared (reading).

**First area.** `Scena08_EnterArea` then runs for area `0x20`. In zone 5 with flag 0
clear (`:1494-1501`) it arms run 1 at step 0 with a `0x96`-frame timer. In zone 1
(`:1502-1511`) it sets up drop-ins only. That the change at `0x900000, 0x550000`
lands in zone 5 is a reading, not checked. `Scena08_Scene1` step 0 counts down the
timer that only the zone 5 branch sets.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena08_Scene1` case 7, `:1610` | Run 1: on counter 0 = `0x16`, the change to area `0x20` (`0x640000, 0x120000`) | - | `Scena08_EnterArea:1494` (run 1 plays once); `:1503` (zone 1 set-up before and after) | progress |
| 1 | `Scena08_Scene1` case `0xB`, `:1634` | Run 1: on counter 0 = `0x1D`, the change to area `0x20` zone 5, script flags `\| 0x20` | - | `Scena08_EnterArea:1506` (zone 1: scenario call A 1, drop-in 2 while clear) | progress |
| 2 | `Scena08_EnterArea`, `:1515` | First entry to area `0x20` in zone 0: scenario call A 2, drop-in 3, script flags `& 0xFFDF` | - | `Scena08_EnterArea:1512` (once) | once-only set-up |
| 3 | `Scena08_Scene2` case 2, `:1667` | Run 2: on counter 3 = `0x12`, a `0x10`-frame wait, then three bytes of `0x9039F0` set. Run 2 is started by `Area34_ChoiceTail5` (`area_w0c.cpp:525-542`) | - | `Area34_ChoiceTail5:537` (run 2 is started only while clear) | unknown |
| 4 | `Scena08_Scene3` case `0x12`, `:1816` | Run 3 ends, after event battle `0x20` (fight 32, D>Zombie). Genes 3 and `0x10` are set at the same step (`:1814-1815`). The special-interest section below has the details | - | **Nothing in source tests chapter 8's flag 4.** Only `Area82_OpenMessageByRowFlag4` (`area_w2a.cpp:481`) tests flag 4 of whatever row is current | progress |
| 5 | `Scena08_Scene4` case 1, `:1842` | Run 4 at step 0: counter 0 = `0xA`, drop-in 1; flag 5 when counter 0 returns to 0. Armed by the arrive hook in area 1 (x cells `0x1C..0x1D`, z cells `0x2B..0x2C`) while flag 5 is clear (`:2677-2682`) | - | `Scena08_ArriveHook:2677` (once) | progress |
| 6 | `Scena08_Scene4` case `0xA`, `:1865` | Run 4 at step `0xA`: on counter 0 = `0xA`, `0x591BE0(0xBB8, 0)` (the call `area_w0b.cpp:801` names the money give) and the change to area 1. Armed by `Scena08_EnterArea` in area 1 zone 2 while flag 6 is clear (`:1409-1424`) | - | `Scena08_EnterArea:1409` (that set-up once); `:1426` (area 2: an effect `0x13` with flag 6 set and 7 clear); `Scena08_StepHook:2647` (area 1: run 4 step 5, message `0x24`, while clear; after it, run 5) | progress |
| 7 | `Scena08_Scene5` case 4, `:1919` | Run 5: the change to area 2, then area `0x35` twice; on counter 0 = 8 the party refreshed, flag 7 and the change to area `0x73`. Armed by the step hook in area 1 with flag 6 set (`:2653-2658`) | - | `Scena08_StepHook:2653` (once); `Scena08_EnterArea:1426` | progress |
| 8 | `Scena08_Scene6` case 8, `:1995` | Run 6 after event battle `0x21` (fight 33, Weretigr, `:1989`). Armed by the arrive hook in area `0x23` (`:2721-2729`) | - | `Scena08_ArriveHook:2721` (once) | progress |
| 9 | `Scena08_Scene7` case 1, `:2019` | Run 7 at step 0 (`Scena08_Object0`, object handler 0): a `0x20`-frame wait, drop-in 0, flag 9 when counter 0 is 0 | - | Nothing in source tests it | unknown |
| 10 [0xA] | `Scena08_EnterArea`, `:1399` | First entry to area 0 in zone 0: scenario call A 3, drop-in 5 | - | `Scena08_EnterArea:1398` (once) | once-only set-up |
| 11 [0xB] | `Scena08_Scene7` case `0xE`, `:2050` | Run 7 at step `0xA` (`Scena08_Object1`): the change to area 0, a fade out, the party refreshed, stream 0, a fade in, flag `0xB` | - | `Scena08_EnterArea:1435` (area `0xC`: run 8 needs `0xB`); `Scena08_ArriveHook:2683` (the area `0xC` block needs `0xB`) | progress |
| 12 [0xC] | `Scena08_Scene8` case 7, `:2149` | Run 8 at step 0, armed by `Scena08_EnterArea` in area `0xC` with `0xB` set and `0xC` clear (`:1435-1446`): the change to area `0xB`, message `0x27`, music `0xC`, then the change back to area `0xC` at the kept point | - | `Scena08_EnterArea:1435` (once) | progress |
| 13 [0xD] | `Scena08_Scene8` case `0x18`, `:2248` | Run 8 steps `0x14..0x18`, armed by the arrive hook in area `0xC` at `(0x70000, 0x130000)` (`:2707-2712`) | - | `Scena08_ArriveHook:2683` (the area `0xC` block ends) | progress |
| 14 [0xE] | `Scena08_Scene8` case `0x1B`, `:2272` | Run 8: after message `0xE`, on counter 0 = 0, the kept sprite freed, scenario call A 2, the party-list bytes `0x904065`/`0x904066` put back, member 1 placed | - | `Scena08_EnterArea:1431` (area 3: scenario call A 3 and member 1's bit `0x40` while clear); `:1447` (area `0xF`: run 9 needs `0xE`); `Area12_WaitFlagE` (`area_w0a.cpp:411-415`: area 12's handler 0 repeats until it is set) | progress |
| 15 [0xF] | `Scena08_Scene9` case 3, `:2321` | Run 9 at step 0 (armed in area `0xF`, `:1447-1454`): the kind-2 sprite placed, waits; on counter 0 = 0 the change to area `0xD` | - | `Scena08_EnterArea:1447` (once); `:1464` (the area `0x15` zone 0 block needs it); `Scena08_ArriveHook:2715` (area `0xD`: run 9 step 5 needs it) | progress |
| 16 [0x10] | `Scena08_Scene9` case 7, `:2345` | Run 9 at step 5 (arrive hook, area `0xD`, `:2715-2720`): drop-in 5, an effect `0x5F`, sound `0x203`, the change to area `0xF` | - | `Scena08_ArriveHook:2715` (once) | progress |
| 17 [0x11] | `Scena08_EnterArea`, `:1472` | First entry to area `0x15` in zone 0 with `0xF` set: music file `0x74`, drop-in 0, the kind-2 sprite placed | - | `Scena08_EnterArea:1465` (once) | once-only set-up |
| 18 [0x12] | `Area21_ChoiceTrade`, `area_w0b.cpp:826` (the current row) | Area 21 (`0x15`): choice 0 with item 3 of category 0 held. A sum of money given, the item taken, then flag `0x12` | - | Nothing in source tests it | optional |
| 19 [0x13] | `Scena08_Object2`, `:2635` | Object handler 2: run 10 at step 0, a drop-in 1, and the run over | - | `Scena08_EnterArea:1474` (area `0x15`: with `0x13` set and `0x14` clear, run 10 is armed at step `0xA`) | progress |
| 20 [0x14] | `Scena08_Scene10` case `0xC`, `:2383` | Run 10 at step `0xA`: two effects `0x13`, then flag `0x14` | - | `Scena08_EnterArea:1474` (run 10 once finished); `Scena08_ArriveHook:2731` (area `0x2B`: run 11 needs it) | progress |
| 21 [0x15] | `Scena08_Scene11` case `0xC`, `:2522` | Run 11 after event battle `0x22` (fight 34, Mikba, `:2514`). Armed by the arrive hook in area `0x2B` (`:2731-2737`) | - | `Scena08_ArriveHook:2731` (once) | progress |
| 22 [0x16] | `Scena08_StepHook`, `:2661` | Area `0x46`, x `0x388000`, z cells `0x2E..0x31`. It starts no scene | - | Nothing in source tests it | unknown |
| 23 [0x17] | **Not set in source** | - | - | `Scena08_EnterArea:1455` (area `0x12`: while clear, 11 map cells `(0x1A + i, 0x18)` set to `0x10`) | unknown |
| 24..26 [0x18..0x1A] | `Scena08_ArriveHook`, `:2694`, `:2699`, `:2704` | Area `0xC` with `0xB` set and `0xD` clear, at three exact points (x `0x1B0000`/`0x160000`/`0x110000`), once each, with sounds `0x201..0x203` | - | `Scena08_ArriveHook:2693`, `:2698`, `:2703` (each once) | optional |
| 208 [0xD0] | `Scena08_EnterArea`, `:1484` | Arming run 10 in area `0x15`. The flag number is the row pointer's low byte (`0xD0`), not `0x14`: D148 ([`../scena_sc7.md`](../scena_sc7.md) section 7). It sets bit 0 of `0x903FEA`, chapter 11's flag 16 | - | - | unknown |

Run 2 needs a word. `Area34_ChoiceTail5` reads the choice answer. With 0 or 1 it arms
tail kind 5 and sets event flag `0x1E`. Then, for any answer but 2, in chapter 8 with
flag 3 clear, it disarms the tail and starts run 2 (`area_w0c.cpp:525-542`). The data
decides when that choice runs.

## Scena08_Scene3: run 3, event battle 0x20, flag 4 and the genes

Asked by the owner: what starts the run, what flag 4 gates, and whether a gene bit
decides if the scene plays.

### What starts run 3

**No `Scena08_*` function starts run 3.** The run byte is set by area 108's tail,
`Area108_TailPlace` (`area_w2f.cpp:475-570`), state 9: after a `0x1E`-frame wait it
sets `MoveScript_Var7 = 3` and step 0, and disarms the tail (`:561-567`). The chain:

1. `Area108_PlaceScene` (`area_w2f.cpp:254-303`, area 108's handler `+0x3C[4]`, run
   from the area's event script). The leader must be member id 2 (`+0x89`), facing
   one of `Area108_Places` at extra object 1's position. At place 0, event flags
   `0x18..0x1A` pick member scripts 1..4. At place 4, event flag `0x1B` picks
   script 9, else 8. Then it arms tail kind `0xC` at state 0.
2. `Area108_TailPlace` states 0..2 move the kind-2 sprite. At state 2, extra object
   1's script `+0x83` = 9 sends it to state 4; any other script goes to state 3,
   which disarms the tail with no run.
3. State 4: an effect `0x35` and the model's flags. State 5: after `0x1E` frames,
   **event flag `0x1D`** set and `MoveCmd_TestFB(0x20, 0x2F)`. State 9: run 3.

So run 3 starts only from place 4 with event flag `0x1B` set. Event flags `0x18..0x1B`
are toggled by `Area108_CellHook` (`area_w2f.cpp:426-452`). It toggles the flag its
cell entry names (`Area108_HookCells`, data), unless event flag `0x1C` is set, and
then sets `0x1C`. `Area_Enter` clears event flag `0x1C` on every area entry
(`mode_flow.cpp:560`).

**No chapter flag gates the start**, and nothing tests `Cond_ByteFA` on this path.
Neither function tests a chapter row. The tail sets the run byte in any chapter;
that it runs only in chapter 8 depends on the data (reading). The run's area agrees:
`Area108_SpawnEffect51` (`area_w2f.cpp:391-408`) places effect `0x51` at
`(0x158000, 0x418000)`, the point `Scena08_Scene3` step 2 uses. `boss-fights.md`
also places fight 32 in area 108.

### What run 3 does

These are the steps of `scena_sc7.cpp:1686-1825`:

1. Step 0: drop-in 6.
2. Steps 1..2: the kind-2 sprite placed; effect `0x51`.
3. Step 4: the party placed for battle `0x20`.
4. Step 8: sound `0x20A`.
5. Step `0xA`: message `0x12`.
6. Step `0xC` (`:1780`): `Field_StartEventBattle(0x20)`.
7. Step `0xD`: waits for the battle to move the step (set outside this file;
   reading).
8. Step `0xE`: DAT 1 loaded, drop-in 7.
9. Step `0xF`: effect `0x63`.
10. Step `0x10`: music `0x6D`.
11. **Step `0x12`** (`:1812-1821`): on counter 0 = 0,
    `Flags_Set(Gene_Bits, 3)`, `Flags_Set(Gene_Bits, 0x10)`, chapter flag 4, script
    flags `& 0xFFF7`, `ScriptFlags_Clear40`, and the run over.

The genes and flag 4 are set only after the battle and the scene after it.

### What flag 4 gates

**Nothing in our source tests chapter 8's flag 4.** The search covered:

- every `Flag(...)` and `Flags_Test` in the `Scena08_*` functions, literal and not
  literal. The only non-literal sites in the file are `ObjectReward`, which tests
  chapter 6's row, and the D148 set;
- every use of `Cond_Flags + 0x40` in `src/game`: only `Scena08_Start:1380`, the
  clear;
- every current-row site in the area, effect, boss and field files (the
  `tools/story_flags.py census` list, read one by one). The only one that tests flag
  4 is `Area82_OpenMessageByRowFlag4` (`area_w2a.cpp:481`): area 82's handler 3
  opens message 8 when set, else 3. Chapter 8's code never enters area `0x52`, so
  this matters only if area 82's script runs that handler during chapter 8
  (reading).

The data can still test it, by two routes:

- **Event-script condition 2** (`EventCond_Flag`, `event_script.cpp:706-708`) tests a
  flag of the row `EventScript_FlagBank` names. By default that is the current
  chapter (`event_script.cpp:111`, `:137`).
- **Map-cell conditions** (`Area_TestCondition`, `map_cells.cpp:81-97`): code
  `0x0804` tests row 8's flag 4 in any chapter.

Area 108's own script is the likely candidate to test flag 4, to stop the scene
re-arming after it has played. Our source shows no such test: the code alone
lets the place-4 path start run 3 again (reading, not checked).

### Do the gene bits decide whether the scene plays

**No code path reads `Gene_Bits` on the way to run 3.** The only readers of
`Gene_Bits` in `src/game` are:

- `Area143_ChoiceAsk52` (`area_w3f.cpp:297`), all 18 genes = `0x3FFFF`;
- the battle gene window (`battle_e5.cpp:625`);
- the gene menu list (`rest_2h.cpp:257`);
- event-script condition 16, `EventCond_Gene` (`field_o.cpp:1317`), which data
  scripts can use.

`Area108_PlaceScene`, `Area108_TailPlace`, `Area108_CellHook` and `Scena08_Scene3`
read no gene bit.

### Answers

- **Gene bit `0x10` already set when the scene is reached.** In source, the scene
  starts and plays the same way. Step `0x12` ORs bits 3 and `0x10` in again
  (`Flags_Set`, `event_script.cpp:679-682`), which changes nothing for `0x10`.
  The only thing that could differ is an area 108 event script that tests gene
  `0x10` with condition 16, which is data we have not read. With bit `0x10` set
  early, the gene window and the menu list show it early, and area 143's
  all-genes test counts it.
- **Flag 4 set ahead of time.** In source, nothing breaks: no code reads it, and
  run 3 is started by area 108's tail, gated by event flags `0x1B`, `0x1C` and
  the member scripts, not by flag 4. The scene would still play when reached and
  set flag 4 again. What a game-data script does with flag 4 (condition 2 in area
  108 or later areas, or a map-cell code `0x0804`) is not known. If area 108's
  script uses it to skip the scene, setting it early would skip the battle and the
  gene grant together. A run, or a read of area 108's event script, is owed.

## End

**The chapter ends at `Scena08_Scene11` case `0x15`** (`:2588-2594`). After run 11
has changed to area `0x2B` (case `0xE`, `:2534`) and played its scenes, it waits for
the fade, ends the run and calls `Field_SetStatus80` (`:2591`). It changes no area at
that step, so chapter 9 starts at the next area entry, when the player leaves
(reading, `mode_flow.cpp:548-556`). Flag 21 is set earlier on the same run (case
`0xC`, after event battle `0x22`).

**The row as left** (reading):

- Set on the way through: 0, 1, 3, 4, 5, 6, 7, 8, 11..16, 19, 20, 21. Flag 3 needs
  `Area34_ChoiceTail5`, flag 19 an object trigger; both are in the game data's
  hands, but flag 19 leads to 20, which gates run 11.
- Once-only set-ups: 2, 10, 17 (set on the first entry to areas `0x20`, 0 and
  `0x15`).
- Optional: 18 (the trade in area 21), 24..26 (three points in area `0xC`).
- Kind not established: 9 (object handler 0), 22 (a point in area `0x46`).
- Never set in source: 23.
- Cleared: none. No `Flags_Clear` of chapter 8's row exists.
- Alternatives: none.
- Outside the row: D148 sets chapter 11's flag 16 (bit 0 of `0x903FEA`) when run 10
  is armed. Chapter 11's own start clears its row's first dword (`Scenario_Start`),
  so the stray bit should not survive into chapter 11 (reading).

**Against the saves.** Saves 20, 21 and 22 of
[`observed-rows.md`](observed-rows.md) hold chapter 8 row **0..22, 24..26**, the same
row, so they count as one data point.

- Every flag the source sets is set, 9, 18, 22 and 24..26 included: agree. This play
  took every optional one.
- 23 clear: agrees with "never set in source".
- 18 set: agrees with `Area21_ChoiceTrade` writing the current row while chapter 8
  is current. No other site sets 18 in chapter 8.
- Event flags 24..27 and 29 (`0x18..0x1B`, `0x1D`) are set and 28 (`0x1C`) is clear
  in the same saves. That agrees with the area 108 reading: `0x1B` and `0x1D` on the
  way to run 3, and `0x1C` cleared on every area entry.

## Event flags used by this chapter

| Event flag | Site | What |
|---:|---|---|
| 67 [0x43] | `Scena08_Start:1384` | Set at the chapter's start. Set in saves 20..22 |

No other `Scena08_*` function sets, clears or tests an event flag. On the path to run
3, area code uses event flags `0x18..0x1D` (24..29): `0x18..0x1B` toggled by
`Area108_CellHook` and read by `Area108_PlaceScene`; `0x1C` the cell hook's latch,
cleared by `Area_Enter`; `0x1D` set by `Area108_TailPlace` state 5. `Area34_ChoiceTail5`
sets `0x1E` (30) on answer 0, and `Area108_ChoiceArmTail5` sets `0x1E` or `0x1F` (31).
Event flags 30 and 31 are both clear in saves 20..22, so neither choice took answer 0
or 1 in that play, or something cleared them later (unknown).

## Unresolved

- **What re-arms or stops run 3.** No source test of flag 4 or event flag `0x1D` stops
  `Area108_PlaceScene` from starting run 3 again. The guard, if any, is in area 108's
  event script (condition 2 or 12). Read the script or check in play.
- **Data tests of flag 4 and gene `0x10`.** Condition 2 (current row), condition 16
  (genes) and map-cell code `0x0804` can test them from data anywhere. Not read.
- **Steps set from outside.** `Scena08_Scene3` step `0xD` -> `0xE`, `Scena08_Scene6`
  step 7 -> 8 and `Scena08_Scene11` step `0xB` -> `0xC` follow the event battles. This
  file never sets them. Flags 4, 8 and 21 depend on them.
- **Flag 23** is tested in area `0x12` and never set in source. A data script with an
  event op that sets the current row could set it. Not searched.
- **Flags 9 and 22** have no test in source. Kind unknown.
- **The first area entry's zone.** That the change to area `0x20` at
  `(0x900000, 0x550000)` lands in zone 5, and so arms run 1, is a reading.
- **Flag 208 (D148).** That its bit lands on `0x903FEA` assumes the row pointer's low
  byte is `0xD0`, true while the image sits at its own base. `Low32(bank) & 0xFF`
  (`:1484`) takes the pointer as built. Not checked on the relocatable build.
- **`Area82_OpenMessageByRowFlag4`** in chapter 8: whether area 82's handler 3 runs
  during chapter 8 is in the data.
