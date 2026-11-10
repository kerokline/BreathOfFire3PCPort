# Story flags: chapter 15

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 15's row is `Cond_Flags + 0x78` (`0x904008`), save block `+0x628`.
Source: `src/game/scena_sc15.cpp` (the `Scena15_*` functions), with area code that
runs in this chapter's areas. Every `Flags_Set`, `Flags_Clear` and `Flags_Test` of
the file is in this page. The file's wrappers are `Test`, `Set`, `TestAt`, `SetAt`
and `ClearAt` (`scena_sc15.cpp:199-204`); `Row()` reads `Chapter_FlagRow` afresh.

All line numbers are `scena_sc15.cpp` unless a file is named.

## Start

**State 0 does nothing but move on.** `Scena15_States` entry 0 is
`ScenaShared_State0` (`scena_sc13.cpp:1424`), which sets the state byte to 1. The
same function is state 0 of eight chapters (`docs/scena_sc13.md` section 3). So
chapter 15 places no party, sets no area and sets no flag when it starts. The
first work is `Scena15_EnterArea` (state 1, line 345) for the area being entered.

**Who starts it.** `Area_Enter` (`mode_flow.cpp:548-556`) steps the chapter byte
by one when `Field_StatusBits` bit 7 is set, and calls `Scenario_Start` with the
result. Chapter 14 + 1 = 15. `Scenario_Start` (`title_states.cpp:404-423`) zeroes
only the row's first dword (line 416: `SetLong`), so it clears flags 0..31 and
leaves flags 32..63 as they were. It points `Chapter_FlagRow` at the row.

Which chapter 14 code sets bit 7 is not found. No `Scena14_*` function calls
`Field_SetStatus80` (`0x56D6F0`) or writes bit 7 (`grep` of `scena_sc13.cpp`). See
Unresolved.

**A second way in.** After the ending, `Scena17_EndReturn` (line 2693) writes 15
to the chapter byte directly, without `Scenario_Start`. The row is not zeroed
then. It clears flags 9..22 only (line 2685). See End.

## Flags

"Zone" is `Cond_ByteFD`, the byte the area set-up switches on (the entrance used,
by the chapter 0 reading; not checked here). Event battle names come from
[`boss-fights.md`](../boss-fights.md), which lists fights 39..48 for
`Scena15_BattleSetup`'s battles 0..9 and fight 55 for event battle `0x37`.

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena15_EnterArea`, line 409 | First entry to area `0xAD` in zone 2. `Party_DropIn(3)` with it | - | Line 408: the drop-in runs once | once-only set-up |
| 1 | `Scena15_Run3` step `0xF`, line 701 | Area `0xAD`, zone 1, z above `0x470000` (`Scena15_StepHook`, line 1708). `Cond_ByteFE` 2, `Party_DropIn(9)` | - | `Scena15_StepHook` line 1708: the trigger fires once | once-only set-up |
| 2 | `Scena15_Run3` step `0x18`, line 728 | Started by `Scena15_ArriveHook` in area `0xAE` (line 1741). Marker, drop-in 1, message 3, then the change to area `0x58` | - | `Scena15_ArriveHook` 1741 (first of 2/4/5 still clear picks the next scene); `Scena15_EnterArea` 421 (area `0xAE`, with 3 clear: a marker); `Area191_TalkMessage` (`area_w4e.cpp:1057`) and `Area192_TalkMessage` (`area_w4f.cpp:346`) pick their message table on it, reached from `Scena15_ObjectTalk` (line 1649) | progress |
| 3 | `Scena15_Run3` step `0x1B`, line 751 | Back in area `0xAE` after area `0x58`, on counter 0 = 6 | - | `Scena15_EnterArea` 421: the marker of the 2-not-3 interval | progress |
| 4 | `Scena15_Run3` step `0x1F`, line 762 | Area `0xAE`, second rectangle of `Scena15_ArriveHook` (line 1749). Marker, `Party_DropIn(3)`, counter 0 = 2 | - | `Scena15_ArriveHook` 1749 | progress |
| 5 | `Scena15_Run3` step `0x29`, line 773 | Area `0xAE`, third rectangle (line 1757). `Party_DropIn(4)`, counter 0 = `0xC`, then the change to area `0x3A` | - | `Scena15_ArriveHook` 1757 | progress |
| 6 | `Scena15_Run3` step `0x2B`, line 786 | Back in area `0xAE` (step `0x2A`), counter 0 = 0 | - | Nothing in the source | progress |
| 7 | `Scena15_Run3` step `0x32`, line 790 | Started by `Scena15_Object05` (object table entry 5, line 1660). The change to area 2 | - | `Scena15_EnterArea` 347: area 2 with 8 clear, the pass flags 0 | unknown (see Unresolved) |
| 8 | `Scena15_Run3` step `0x34`, line 805 | Area 2, counter 0 = 2. The change back to area `0xAE` | - | `Scena15_EnterArea` 347 | unknown |
| 9 | `Scena15_Run4` step `0xC`, line 938 | Run 4 (started by `Scena15_StepHook` line 1720: area `0xAE`, zone 0, x exactly `0x418000`). Counter 0 = `0x2B` and file `0x303` in; the change to area `0xC4` | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 348: area `0x2D` with 10 clear, the camera distance `0x5DC` | progress within run 4; run 4's need unknown |
| 10 (`0xA`) | `Scena15_Run4` steps `0xF` and `0x10`, lines 959 and 967 | Step `0xF`: the change to area `0x98`. Step `0x10`: set again, the change back to `0xAE` | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 348 and 362 (area `0x98`, with 11 clear: camera) and 422 (area `0xAE`, with 11 clear: music `0xA2`) | progress within run 4 |
| 11 (`0xB`) | `Scena15_Run4` step `0x18`, line 1019 | Run 4's end, counter 0 = 0, after message `0x1E` and two effects | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 362, 422 | progress within run 4 |
| 12 (`0xC`) | `Scena15_Object08`, line 1685 | Object table entry 8 triggered. No member dropped in | `Scena17_EndReturn` 2685 | `Scena15_ArriveHook` 1767 | optional (one of five) |
| 13 (`0xD`) | `Scena15_Object06`, line 1681 | Object table entry 6. `Party_DropIn(0xB)` | `Scena17_EndReturn` 2685 | `Scena15_ArriveHook` 1767 | optional (one of five) |
| 14 (`0xE`) | `Scena15_Object07`, line 1683 | Object table entry 7. `Party_DropIn(0xC)` | `Scena17_EndReturn` 2685 | `Scena15_ArriveHook` 1767 | optional (one of five) |
| 15 (`0xF`) | `Scena15_Object10`, line 1689 | Object table entry 10. `Party_DropIn(0xD)` | `Scena17_EndReturn` 2685 | `Scena15_ArriveHook` 1767 | optional (one of five) |
| 16 (`0x10`) | `Scena15_Object09`, line 1687 | Object table entry 9. `Party_DropIn(0xE)` | `Scena17_EndReturn` 2685 | `Scena15_ArriveHook` 1767 | optional (one of five) |
| 17 (`0x11`) | `Scena15_Run5` step `0x1F`, line 1197 | Counter 0 = `0x2A`: the object whose record word is `0x308` gets script `0x24`. Then message `0x65`, call-table entry 1, area `0xC6` and run 6 | `Scena17_EndReturn` 2685 | Nothing in the source | choice (the run 5 branch that leads on; reading) |
| 18 (`0x12`) | `Scena15_Run5` step `0xE`, line 1117 | Run 5 from step 5. The change to area `0xAC`; then message `0x2B`, the colours faded to grey, message `0x2D`, effect `0xA5`, and on `Input_Held` bit `0x800` a restart at `Boot_Task` (line 1181) | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 402: area `0xAC`, the pass flags 0 | choice (the run 5 branch that ends in a restart; reading) |
| 19 (`0x13`) | `Scena15_Run6` step `0x16`, line 1418 | After event battle `0x37` (step `0xD`, line 1363; fight 55) and the random pause. The change to area `0xAE` | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 426 (area `0xAE`: with 20 clear, call-table entry 2 and `Party_DropIn(0x13)`) and 438 (area `0xC6`: effects `0xAB`, `0xB0`, the shake) | progress |
| 20 (`0x14`) | `Scena15_Run6` step `0x21`, line 1492 | After message `0xA`. The change to area `0xAE` | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 427, 444 | progress |
| 21 (`0x15`) | `Scena15_Run6` step `0x29`, line 1519 | After message `0x6B`. Event flag `0x58` with it; the change to area `0xAD` | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 412 (area `0xAD`, with 22 clear: effect `0xAF`, a marker, call-table entry 3, `Party_DropIn(0xA)`), 431, 444, 445 | progress |
| 22 (`0x16`) | `Scena15_Run6` step `0x31`, line 1549 | The camera distance past `0x780`. The change to area `0x95` | `Scena17_EndReturn` 2685 | `Scena15_EnterArea` 352 (area `0x95`: effect `0x31`, camera), 412, 445 | progress |
| 32 (`0x20`) | `Scena15_Run2` step 5, line 620 (reading) | Area `0x9E`, zone 0: battle 0 (fight 39) | - | `Scena15_EnterArea` 375 | once-only set-up |
| 33 (`0x21`) | `Scena15_EnterArea`, line 380 | Area `0x9E`, zone 1: the flag only | - | Line 379 | once-only set-up |
| 34 (`0x22`) | `Scena15_Run2` step 5 (reading) | Area `0x9F`, zone 0: battle 1 (fight 40) | - | Line 375 | once-only set-up |
| 35 (`0x23`) | `Scena15_EnterArea`, line 380 | Area `0x9F`, zone 1: the flag only | - | Line 379 | once-only set-up |
| 36 (`0x24`) | `Scena15_EnterArea`, line 376 | Area `0xA0`, zone 0: the flag only | - | Line 375 | once-only set-up |
| 37 (`0x25`) | `Scena15_Run2` step 5 (reading) | Area `0xA0`, zone 1: battle 2 (fight 41) | - | Line 379 | once-only set-up |
| 38 (`0x26`) | `Scena15_EnterArea`, line 376 | Area `0xA1`, zone 0: the flag only | - | Line 375 | once-only set-up |
| 39 (`0x27`) | `Scena15_Run2` step 5 (reading) | Area `0xA1`, zone 1: battle 3 (fight 42) | - | Line 379 | once-only set-up |
| 40 (`0x28`) | `Scena15_Run2` step 5 (reading) | Area `0xA2`, zone 0: battle 4 (fight 43) | - | Line 375 | once-only set-up |
| 41 (`0x29`) | `Scena15_Run2` step 5 (reading) | Area `0xA2`, zone 1: battle 5 (fight 44) | - | Line 379 | once-only set-up |
| 42 (`0x2A`) | `Scena15_Run2` step 5 (reading) | Area `0xA3`, zone 0: battle 6 (fight 45) | - | Line 375 | once-only set-up |
| 43 (`0x2B`) | `Scena15_EnterArea`, line 380 | Area `0xA3`, zone 1: the flag only | - | Line 379 | once-only set-up |
| 44 (`0x2C`) | `Scena15_Run2` step 5 (reading) | Area `0xA4`, zone 0: battle 7 (fight 46). Any other zone goes straight to state 2 (line 385) | - | Line 389 | once-only set-up |
| 45 (`0x2D`) | `Scena15_EnterArea`, line 392 | Area `0xA5`, zone 0: the flag only | - | Line 392 | once-only set-up |
| 46 (`0x2E`) | `Scena15_Run2` step 5 (reading) | Area `0xA5`, zone 1: battle 8 (fight 47) | - | Line 393 | once-only set-up |
| 47 (`0x2F`) | `Scena15_Run2` step 5 (reading) | Area `0xA6`, zone 0: battle 9 (fight 48). Any other zone goes straight to state 2 (line 396) | - | Line 400 | once-only set-up |

Flags 23..31 and 48..63 are not named anywhere in our source.

**How the battle flags are set.** The pairs table in `Scena15_EnterArea` (lines
369-383) gives, for each of areas `0x9E..0xA3` and zones 0 and 1, a flag and either
a battle or none. With no battle the flag is set at once. With a battle,
`Scena15_BattleSetup(n)` (line 459) starts run 2 and stores `n` in an effect's
`+0xB`. `Scena15_Run2` starts the battle `Scena15_Battles[n] +9` (line 612) and,
at step 5, sets the flag held in `Scena15_Battles[n] +8` (line 620). That the
`+8` byte holds the same flag the pairs table tests is a reading, not checked: the
table is in `.data`. As in chapter 0, a flag is set at the step after the battle
(step 5), not when the encounter starts. Whether a lost battle reaches step 5 is not
read.

**The order the main scenes take**, by the hooks (reading, not checked in play):

1. `Scena15_ArriveHook` (line 1737) works in area `0xAE` only. It runs the scene
   of the first of flags 2, 4, 5 that is still clear: run 3 at step `0x14`
   (sets 2 and 3), then `0x1E` (sets 4), then `0x28` (sets 5 and 6).
2. With 2, 4 and 5 all set, the spot x = `0x310000`, z cell `0x27..0x28` starts
   run 5: at step 5 when flags 12..16 are all set (line 1767), else at step 0.
   Step 0 only drops member set `0xF` in and ends the run (line 1056).
3. Run 5's step-5 branch sets 18 and ends in a restart at `Boot_Task`
   (lines 1060-1182). Its step `0x1E` branch sets 17 and goes on to area `0xC6`
   and run 6 (lines 1183-1232). No code of ours sets step `0x1E`; see Unresolved.
4. Run 6 sets 19..22 in order and ends the chapter (below).

## End

**`Scena15_Run6` step `0x36`** (line 1581) ends the chapter. It changes to area
`0xC7` at (`0xB60000`, `0x128000`) facing 7, clears `Field_ScriptFlags` bit
`0x0800`, ends the run and calls `Field_SetStatus80` (line 1587). On the area
entry `Area_Enter` steps the chapter byte to 16 and, because chapter 16 is
skipped (`mode_flow.cpp:551-554`), to 17. **Chapter 15 hands over to chapter 17.**
Chapter 16 is reached only by the title's demo (see `chapter-16.md`).

No flag is set at step `0x36`. The flags set on the run 6 path are 19, 20, 21, 22
(and event flag `0x58`).

**The other way out of run 5.** Step `0x19` (line 1178) restarts the task at
`Boot_Task` after the grey-out. That path sets 18 and does not change the chapter.

**The row as left** when chapter 17 starts (reading, not checked):

- Always set: 2, 3, 4, 5, 6 (the arrive-hook chain gates run 5), 17 (the branch to
  run 6), 19, 20, 21, 22. Flag 0 too, if any entry to area `0xAD` lands in zone 2
  (run 6 step `0x29` changes to area `0xAD`; whether that entry is in zone 2 is not
  read).
- Set when their runs ran: 1, 7, 8, 9, 10, 11. Whether those runs are on the
  required path is not read.
- Optional: 12..16, each by its own object. The five together switch run 5's
  start step.
- Alternatives: 17 and 18 are on the two branches of run 5. A row with 18 and no 17
  is followed by a restart, not by the ending.
- Optional, by area and zone: 32..47.
- Never cleared inside the chapter: nothing in chapter 15's own code clears a flag
  of its row.

**After the ending.** `Scena17_EndReturn` (line 2685) clears flags 9..22 of this
row and sets the chapter byte back to 15 (see `chapter-17.md`). The row then
holds 0..8 and 32..47 as they were, with 9..22 clear.

**Observed rows.** Only save 22 (US, 43:25,
[`observed-rows.md`](observed-rows.md)) is inside chapter 15. Its chapter 15 row is
empty, in area 154 (`0x9A`), which no chapter 15 code names. That fits a save
made before the first flag of the chapter (flag 0, area `0xAD`). Its row-14 flags
1, 2 and 3 are clear and event flags `0x77`, `0x81`, `0x8A`, `0x58` and `0x92`
are clear, so none of the runs below had run. No save past 43:25 exists, so nothing
in the table above is checked against a save.

## Chapter 14's row, written by chapter 15

`Scena15_Run1` and `Scena15_EnterArea` use `Cond_Flags + 0x70`, chapter 14's row,
by its fixed address:

| Row 14 flag | Site | What |
|---:|---|---|
| 1 | Set by `Scena15_Run1` step 1 (line 528), cleared at step 7 (line 562) | Set for the visit to area `0xBD`, cleared on leaving for area `0xC1`. Transient. Tested by `Scena15_EnterArea` line 436 (area `0xBD`: `Effect_Objects[0]` bit `0x40`) |
| 2 | Set by `Scena15_Run1` step `0x19` (line 575) | With row-14 flag 3 and event flag `0x8A`, before area `0xC1` facing `0x84` |
| 3 | Set by `Scena15_Run1` step `0x19` (line 576) | Tested by `Area193_Tail57` (`area_w4f.cpp:547`: message `0x25` or `0x20`) and cleared there (`area_w4f.cpp:563`). Transient |

`Scena15_Run1`'s steps `0x14` and `0x19` are alternatives: `0x14` clears event flag
`0x77` and goes to area `0xC1` facing `0x83`; `0x19` sets row-14 flags 2 and 3 and
goes facing `0x84`. Both set event flag `0x8A`. The code says field scripts set
these steps (comment at line 516); which one runs is not read. `Scena14_Run1`
(`scena_sc13.cpp:1703-1712`) has the same pair of event-flag writes at its own step
`0x19`.

## Event flags used by this chapter

Event flags are `Cond_Flags + 0xA0` (save block `+0x650`).

| Event flag | Site | What |
|---:|---|---|
| 119 (`0x77`) | Cleared by `Scena15_Run1` step `0x14`, line 571 | Also tested by `Area191_Init` / `Area192_Init` (`area_w4e.cpp:1080`, `area_w4f.cpp:373`) and cleared by `Area193_Tail57` (`area_w4f.cpp:517`, `534`) |
| 129 (`0x81`) | Tested by `Scena15_Run3` step 4 (line 665) and `Scena15_StepHook` (line 1713) | Run 3 step 4 waits for it; the step hook starts run 3 at step 0 only while it is clear. Set by `EffectKind18Sub5B_Lift` (`effect_6c.cpp:1652`); that this is the effect run 3 places is not checked |
| 138 (`0x8A`) | Set by `Scena15_Run1` steps `0x14` / `0x19`, line 580 | `Area193_Init` (`area_w4f.cpp:586`) tests it on entering area `0xC1` (193) and starts that area's tail at state `0x14`; `Area193_Tail57` clears it (`area_w4f.cpp:564`). Transient |
| 88 (`0x58`) | Set by `Scena15_Run6` step `0x29`, line 1520 | With flag 21, before area `0xAD` (173). `Area173_Init` tests it and `Area173_Tail38` sets and clears it (`area_w4c.cpp:313`, `259`, `279`) |
| 146 (`0x92`) | Set by `Scena17_EndReturn`, line 2684 (chapter 17's code) | See `chapter-17.md` |

Area code in this chapter's areas also reads fixed rows, whichever chapter is
current: `Area149_Init` (area `0x95`) tests row-14 flag 17 (`area_w3g.cpp:785`);
`Area172_ChoiceFlag12` (area `0xAC`) tests row-14 flag 18 (`area_w4b.cpp:1028`);
`Area189_Init` (area `0xBD`) tests event flag `0x82` (`area_w4e.cpp:347`).
`Area191_Init` tests the current row's flag 5 only when the chapter byte is 14
(`area_w4e.cpp:1078`), so not in chapter 15.

## Unresolved

- **Who starts chapter 15.** No C++ function of chapter 14 sets `Field_StatusBits`
  bit 7. The setter may be an event script in the area data, or code not yet
  found. A run from a late chapter 14 save would settle it.
- **`Scena15_Run2` line 620 sets a non-literal flag**, `Scena15_Battles[n] +8`.
  That it is the flag of the pairs table (32, 34, 37, 39..42, 44, 46, 47 for battles
  0..9) is a reading. Checking it needs the ten records of `.data` `0x6618C0`
  (bytes `+8`), read locally and not pasted.
- **`ObjectFlag(object, flag, member)` at line 1673** has a non-literal flag. Its
  five callers pass 12..16 as literals (lines 1681-1689); resolved above.
- **The extractor's `p.flag0` / `p.flag1`** (lines 375-380) are the pairs table's
  literals; resolved above.
- **Steps set outside this file.** Run 1 is started by no code of ours (no
  `MoveScript_Var7 = 1` in the file); the comment at line 516 says field scripts
  set steps `0x14` and `0x19`. Run 5 steps 7, 9, `0x17` and `0x1E`, run 6 step `0xF`
  and run 2 steps 2 and 4 are not set by this file either. Most likely event or field
  scripts and effects; not read. In particular, what starts run 5 at step `0x1E`
  decides which branch sets 17.
- **Whether flags 1, 7..11 are required.** Their triggers (a step rectangle in area
  `0xAD`, object entry 5, a spot in area `0xAE`) are in the area data. Whether a
  playthrough must pass them is not read.
- **What flags 12..16 mean in play.** Five objects (table entries 6..10), each with a
  member drop-in. What the objects are is in the area data.
- **Event scripts can test this row.** `EventScript_Run` starts its flag bank at the
  chapter byte (`event_script.cpp:111`), so area event scripts can test chapter 15's
  flags. Those tests are in the game data, not in our source.
- **Area code with an unattributed or current row** (`boss_h.cpp:355`,
  `boss_sd.cpp:113-141`) belongs to boss set-ups 18..20, which
  [`boss_sd.md`](../boss_sd.md) places at area 79. Not in this chapter's areas
  (reading). Whether event battle `0x37` (fight 55) has a set-up that writes the
  current row is not read.
- **Nothing here is checked in play** past the 43:25 save.
