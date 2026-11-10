# Story flags: chapter 5

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 5's row is `Cond_Flags + 0x28` (`0x903FB8`), save block `+0x5D8`. Main
source: `src/game/scena_sc5.cpp`. Its helpers (`scena_sc5.cpp:76-80`): `Bits()` is
the row at `Chapter_FlagRow`, `Flag(n)` / `Set(n)` / `Clr(n)` test, set and clear it,
and `Story()` is the event-flag bank `Cond_Flags + 0xA0`. Sites outside the file that
reach this row while chapter 5 runs are in `area_w1c.cpp` (areas 48..52),
`area_w2a.cpp` (area 78), `boss_sd.cpp` and `boss_h.cpp` (event battles 18..20 in
area 79), and `effect_3d.cpp` (effect kind 0x77). Each is cited in the table.

Run numbers below are the `Scena05_RunN` names (decimal). The code that starts a run
writes it in hex: run `0x10` is `Scena05_Run16`. Areas are given in hex as the code
has them, with decimal in brackets where it helps: 0x2D (45), 0x31 (49), 0x32 (50),
0x34 (52), 0x41 (65), 0x43 (67), 0x44 (68), 0x45 (69), 0x4E (78), 0x4F (79), 0x50
(80), 0x51 (81). "Sel" is `AreaW0b_PartySet & 0x7F` (`scena_sc5.cpp:60`), a party
selector with the values 1, 2 and 6. Event-battle names are from
[`../boss-fights.md`](../boss-fights.md).

## Start

**Who starts the chapter.** Chapter 4 ends in `Scena04_Scene1` (chapter 4's run 1),
step `0x16` (`scena_sc3.cpp:2102-2111`): it sets chapter 4's flag 12 (`0xC`) and
calls `Field_SetStatus80` (`0x56D6F0`, `scena_sx.cpp:320`), which sets bit 7 of
`Field_StatusBits`. Step `0x15` before it issued `ChangeArea(0x36, ...)`
(`scena_sc3.cpp:2097`). The chapter number moves on in `Area_Enter`
(`mode_flow.cpp:548-556`): on the next area entry with that bit set,
`Cond_ByteFA` goes up by one and `Scenario_Start(5)` runs. Which area that entry is
depends on what changes the area next; reading, not checked.

**What `Scenario_Start` does** (`title_states.cpp:404-424`). It stores the chapter
byte, zeroes the state bytes `0x8034E1..0x8034E9` (bit 7 of `Field_StatusBits` too),
points `Chapter_FlagRow` at the row, and writes 0 to **the row's first dword only**
(`SetLong`, `title_states.cpp:416`). So flags 0..31 are cleared at the chapter's
start; flags 32..63 keep whatever the row held. In a normal game the row is never
written before chapter 5, so they are 0 there too; reading, not checked.

**State 0.** Chapter 5's state 0 is `ScenaShared_State0` (`0x5646B0`,
`scena_sc13.cpp:1424`), shared with chapters 9, 10, 12, 14, 15, 18 and 19. It only
sets the state byte to 1. It sets no flag, places no party, starts no run and
plays no music. State 1 `Scena05_EnterArea` (`scena_sc5.cpp:541`) then runs the
per-area body `EnterAreaBody` (`scena_sc5.cpp:362-535`) and stores state 2.

**First trigger.** The first flag is set by the step hook in area 0x44:
`Scena05_StepHook` with flag 0 clear (`scena_sc5.cpp:234`) starts run 1. Run 1
shows message 5, sets flag 0 and changes into area 0x44 (`scena_sc5.cpp:555-562`).
That this is the first thing to happen in the chapter is a reading of the flag
order, not checked.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena05_Run1` case 1, `scena_sc5.cpp:561` | Message 5 closed; counter 2 = 1; `ChangeArea(0x44, 0x1D.0, 0x1E.0, 0x80)` | - | `Scena05_StepHook` area 0x44 (`:234`): the step trigger for run 1 fires once | progress |
| 1 | `Scena05_Run1` case 2, `:566` | Counter 0 reaches 0x2D after the change into 0x44; the run ends | - | `EnterAreaBody` area 0x44 (`:428`): with counter 2 = 1 and flag 1 clear, run 1 resumes at step 2 | progress |
| 2 | `Scena05_Object01`, `:296` | Object trigger entry 1, first time; starts run 21 | - | `Scena05_Object01` (`:295`): the object starts run 21 once | progress |
| 3 | not set in chapter 5's code | - | - | `EnterAreaBody` area 0x34, counter 2 = 1 (`:392`): flag 3 clear zeroes counter 0 | unknown |
| 5 | `Scena05_Run4` case 8, `:608` | After event battle 17 (Mutant, started at case 3, `:596`); counter 2 = 3; `ChangeArea(0x34, 0x85.0, 0xF.0, 0x85)` | - | `Scena05_StepHook` area 0x34 (`:220`): clear arms the step trigger for run 4 (the battle); set arms the trigger for run 4 step 0x14 (message 0xB) | progress |
| 6 | `Scena05_Run4` case 9, `:615` | Counter 0 reaches 8 in area 0x34; run 5 is made current at step 0 | - | `CellCue` (`:259`): the two cell cues of area 0x34 work only with flag 6 set | progress |
| 8 | `Scena05_Run5` case 0x21, `:710` | Message 0x13 closed (music 0x69); call A 0 | - | Nothing in the source read | progress |
| 9 | `Scena05_Run5` case 0x23, `:723` | After `ChangeArea(0x34, 0x85.0, 0x15.0, 0x89)` (counter 2 = 5); the run ends | - | `CellCue` (`:260`): set turns both cell cues off. `StepHook` area 0x31 (`:217`): arms run 7 (message 0x26). `StepHook` area 0x34 (`:222`): clear arms run 4 step 0x14. `EnterAreaBody` area 0x31 (`:371`) and area 0x34 (`:389`) | progress |
| 10 (`0xA`) | `Scena05_Run6`, `:740` | Counter 0 reaches 0x18; run 6 started by the first entry to area 0x31 after flag 9 (`EnterAreaBody`, `:371-387`, with drop-ins by Sel) | - | `EnterAreaBody` area 0x31 (`:371`): the entry set-up runs once | progress |
| 11 (`0xB`) | `Scena05_Object02`, `:302`; also `Scena05_Run8` case 0, `:767` | Object trigger entry 2 starts run 8 | - | `StepHook` area 0x31 (`:217`): set turns off the run 7 trigger | progress |
| 12 (`0xC`) | `Scena05_Run8` case 6, `:798` | Counter 0 reaches 0x28 after the stream and the hop into area 0x31; `ChangeArea(0x45, 0x1E.0, 0x19.0, 0x80)` | - | `EnterAreaBody` area 0x41 (`:413`): clear sets run 0 (a bare return), which cancels any run | progress |
| 13 (`0xD`) | not set in chapter 5's code | - | - | `EnterAreaBody` area 0x41 (`:416`): with 0xC set and 0xD clear, run 9 starts | unknown |
| 14 (`0xE`) | `Scena05_Run8` case 6, `:799` | With flag 12 | - | Nothing in the source read | progress |
| 15 (`0xF`) | `Scena05_Run8` case 6, `:800` | With flag 12 | - | Nothing in the source read | progress |
| 16 (`0x10`) | `Scena05_Run9` case 2, `:832` | Counter 0 reaches 8; `ChangeArea(0x2D, 0x3C.0, 0x2E.0, 0x83)`. Run 9 is started by area 0x45's entry (`:435-442`, music 1) or area 0x41's (`:416`) | - | `EnterAreaBody` area 0x45 (`:437`): run 9 starts once | progress |
| 17 (`0x11`) | `Scena05_Run9` case 0xA, `:844` | Counter 0 reaches 0xC; `ChangeArea(0x32, 0x8.0, 0x26.0, 0x80)` | - | Nothing in chapter 5's source read (see Unresolved for `effect_4b.cpp:1030`) | progress |
| 18 (`0x12`) | `Scena05_Run9` case 0xC, `:857` | Counter 0 reaches 0x1D in area 0x32 | - | Nothing in the source read | progress |
| 19 (`0x13`) | `Scena05_Run9` case 0xC, `:861` / `:866` | Same step, only when Sel is 1, 2 or 6; with the change into area 0x4E (counter 2 = 0, 2 or 3) | - | Nothing in the source read | progress |
| 20 (`0x14`) | `Scena05_Run10` case 4, `:902` | Counter 0 reaches 3; the run ends | - | `EnterAreaBody` area 0x4E, counter 2 = 1 or 4 (`:460`, `:478`): clear starts run 10 at step 4. `EffectKind77_Tick` (`effect_3d.cpp:319`): set makes each count of the tally play sound 0x20A | progress |
| 22 (`0x16`) | `Area78_ChoiceRowFlag16or17` answer 0, `area_w2a.cpp:293`; `Pick13` from `Scena05_Run13` case 0x12, `:985` / `:934` | A choice in area 0x4E (78). Run 13's re-pick sets 0x16 and clears 0x17, counter 2 = 5 | `Pick13` from `Scena05_Run13` case 5 (`:969` / `:935`) | `Scena05_Run10` case 1 (`:894`): clear gives counter 2 = 1 and the change into 0x4E with flags 0x83 | choice (with 23) |
| 23 (`0x17`) | `Area78_ChoiceRowFlag16or17` answer 1, `area_w2a.cpp:293`; `Pick13` from `Scena05_Run13` case 5, `:969` / `:934` | The same choice, other answer. Run 13's re-pick sets 0x17 and clears 0x16, counter 2 = 6 | `Pick13` from `Scena05_Run13` case 0x12 (`:985` / `:935`) | `Scena05_Run10` case 1 (`:889`): clear gives counter 2 = 4 and the change into 0x4E with flags 3. Counter 2 decides the drop-ins and the member flags on the next area 0x4E entry (`:459-488`) | choice (with 22) |
| 24 (`0x18`) | `Scena05_Run10` case 1, `:888` | Counter 0 reaches 0x23 a second time in area 0x4E | - | `EnterAreaBody` area 0x4E, counter 2 = 0, 2, 3 (`:450`, `:468`): clear starts run 10 with music 0x4A / 0x4B | progress |
| 25 (`0x19`) | `Scena05_Run10` case 4, `:901` | With flag 20 | - | Nothing in the source read | progress |
| 26 (`0x1A`) | `Scena05_Run10` case 4, `:903`; also `Scena05_Run14` case 4, `:1024` | With flag 20. Run 14 (started by `Area78_ChoiceResetScene` answer 0, `area_w2a.cpp:319`) sets it again at counter 0 0x14 | - | Nothing in the source read | progress |
| 29 (`0x1D`) | `Scena05_Run16` case 0x11, `:1237` | Run 16 starts from object trigger entry 5 (`:313`); after message 0x32 and the pair code (counter 3), the change into area 0x4F | - | `EnterAreaBody` area 0x4F (`:516`): clear zeroes the counters on entry | progress |
| 30 (`0x1E`) | `Scena05_Run16` case 0x1B, `:1312`; case 0x5F, `:1465` | 0x1B: after event battle 18 (step 0x18, `:1300`), when `Boss18_End` hands back step 0x1B (`boss_sd.cpp:424`). 0x5F: the run's end | - | `MemberMessage` (`:1072`) and `FlagBattle` (`:1091`): picks battle 19 or 20 and the message set. `BossDodai_Enter` (`boss_sd.cpp:347`): set keeps the HP saved from the last fight | progress |
| 31 (`0x1F`) | `Scena05_Run16` case 0x23, `:1337`; case 0x5F, `:1466` | After event battle 19 (`:1330`), when `Boss19_End` hands back step 0x23 (`boss_sd.cpp:437`); and at the run's end | - | `MemberMessage` (`:1075`), `FlagBattle` (`:1091`) | progress |
| 32 (`0x20`) | `Scena05_Run16` case 0x29, `:1355`; case 0x5F, `:1467` | After event battle 20 (`:1351`), when `Boss20_End` hands back step 0x29 (`boss_sd.cpp:451`); and at the run's end | - | Nothing in the source read | progress |
| 33 (`0x21`) | `Scena05_Run16` case 0x5B, `:1434` | Step 0x1A (drop-in 0x30) leads to 0x5A, then 0x5B; a transition 0xD follows | - | Nothing in the source read | progress |
| 34 (`0x22`) | `Scena05_Run16` case 0x29, `:1356`; case 0x5F, `:1468` | With flag 32; and at the run's end | - | Nothing in the source read | progress |
| 35 (`0x23`) | `Scena05_Run16` case 0x47, `:1419` | Step 0x1C (transition 4, then 0x46). A fight end hands back 0x1C when it is not a win, enemy 2 has not fallen and flag 0x23 is clear (`EndAreaFight`, `boss_sd.cpp:140-150`); then the change into area 0x4F by `kHall47` | - | `EndAreaFight` (`boss_sd.cpp:140`): set sends the next such end to step 0x3A instead. `BossDodai_Enter` (`boss_sd.cpp:348`) | optional |
| 36 (`0x24`) | `Scena05_Run16` case 0x65, `:1475` | Step 0x3A (transition 4, then 0x64). The second such fight end, with 0x23 set; change by `kHall65` | - | `EndAreaFight` (`boss_sd.cpp:141`): with 0x23 and 0x24 both set, the end takes the plain step 2 | optional |
| 37 (`0x25`) | `Scena05_Run16` case 0x44, `:1408` | Step 0x44, which nothing in the source read sets; the step stays at 0x44 | - | Nothing in the source read | unknown |
| 40 (`0x28`) | `Scena05_Run17` case 7, `:1527` | Run 17 starts from object trigger entry 6 (`:316`); after message 0x20 and counter 0 = 6; change into area 0x51 by Sel | - | Nothing in the source read | progress |
| 41 (`0x29`) | `Scena05_Run17` case 0xF, `:1571` | After the timed wait (steps 0xB..0xC, music 0x4E); change into area 0x51 | - | Nothing in the source read | progress |
| 42 (`0x2A`) | `Scena05_Run17` case 0x15, `:1587` | Counter 0 reaches 0x11; `ChangeArea(0x51, 0x5A.0, 0x6.0, 0x84)`. Event battle 21 (Emitai, Golem) follows at step 0x18 (`:1605`) | - | Nothing in the source read | progress |
| 43 (`0x2B`) | `Scena05_Run17` case 0x1D, `:1640` | After the change into area 0x4E (music 0x4F) and back to 0x51 | - | Nothing in the source read | progress |
| 44 (`0x2C`) | `Scena05_Run17` case 0x1C, `:1635` | Counter 0 reaches 1 in area 0x4E; `Kind2_Place(0)` | - | Nothing in the source read | progress |
| 45 (`0x2D`) | `Scena05_Run18` case 2, `:1676` | Run 18 starts from object trigger entry 7 (`:319`); `ChangeArea(0x50, 0x20.0, 0xA.0, 0x8E)` | - | Nothing in the source read | progress |
| 46 (`0x2E`) | `Scena05_Run18` case 3, `:1683` | Counter 0 reaches 7; change into area 0x50 by Sel; sound 0x208 | - | `EffectKind77_End` (`effect_3d.cpp:333`): set stops the tally's end from handing run 5 step 0x19 | progress |
| 47 (`0x2F`) | `Scena05_Run18` case 0xC, `:1748` | After event battle 22 (Garr, step 0xA, `:1736`); counter 0 reaches 0x14; transition 0xD | - | `EnterAreaBody` area 0x50 (`:522`): clear with counter 2 = 1 loads music 0x66 | progress |
| 48 (`0x30`) | `Scena05_Run20` case 2, `:1798` | Run 20 starts from the step hook in area 0x4E with 0x3D set and 0x32 clear (`:244`); the effect of `Scena05_SpawnMember(0x91)`; change into 0x4E | - | Nothing in the source read | progress |
| 49 (`0x31`) | `Scena05_Run20` case 3, `:1808` | Counter 0 reaches 0xA; change into area 0x4E | - | Nothing in the source read | progress |
| 50 (`0x32`) | `Scena05_Run20` case 6, `:1831` | Counter 0 reaches 5 after call A 3 or 2 by Sel; the run ends | - | `StepHook` area 0x4E (`:239`): set swaps the run 19 / run 20 triggers for the run 22 trigger (message 0x4C). `EnterAreaBody` area 0x2D (`:364`): set ends the chapter | progress |
| 51 (`0x33`) | `BossMap_UpdateFromEnemies`, `boss_h.cpp:355` | During event battles 18..20 (phase 5 and phase 0 of `EventAreaFight`, `boss_sd.cpp:169-186`): set when enemy 2's `+0x93` has bit 0x20 | `BossMap_UpdateFromEnemies`, `boss_h.cpp:356`, when that bit is clear | `BossDodai_Dispatch` (`boss_sd.cpp:335`): picks enemy 2's pose table | optional |
| 52 (`0x34`) | `BossMap_UpdateFromEnemies`, `boss_h.cpp:355` | The same, for enemy 1 | `boss_h.cpp:356` | `BossDodai_Dispatch` (`boss_sd.cpp:332`): picks enemy 1's pose table | optional |
| 53 (`0x35`) | `RestoreOrMark` from `BossClaw_Enter` (kind 21), `boss_sd.cpp:117` / `:281` | The enemy's first entrance in a fight in area 79 | - | `RestoreOrMark` (`boss_sd.cpp:113`): set restores the HP and AP kept from the last fight | once-only |
| 54 (`0x36`) | `RestoreOrMark` from `BossCawer_Enter` (kind 22), `boss_sd.cpp:298` | The same, kind 22 | - | The same | once-only |
| 55 (`0x37`) | `RestoreOrMark` from `BossPatrio_Enter` (kind 23), `boss_sd.cpp:316` | The same, kind 23 | - | The same | once-only |
| 58 (`0x3A`) | `Scena05_Run21` step 0, `:1850` | Run 21 (object entry 1, with flag 2): the message done; drop-in 3, `Kind2_Place(0)` | `Scena05_Run21` step 1, `:1856`, at counter 0 0x1C | `Area49_EffectFrame` (`area_w1c.cpp:447`, the fixed row `Cond_Flags + 0x28`): while set, area 49's effect leaves every member out | transient |
| 59 (`0x3B`) | `Scena05_Run16` case 0x5F, `:1469`; also `Area49_ChoiceMessageFlag3B` answer 0, `area_w1c.cpp:383` | The end of run 16. Area 49's choice 0 sets it in whatever chapter is current | - | Nothing in the source read | progress |
| 60 (`0x3C`) | `Scena05_Run17` case 0x19, `:1610` | Counter 0 reaches 0xA after event battle 21 (`Boss21_End` sets counter 0 to 0xA on a win, `boss_sd.cpp:551`) | - | Nothing in the source read | progress |
| 61 (`0x3D`) | `Scena05_Run18` case 0x10, `:1769` | Counter 0 reaches 8 after the change into area 0x50; `Party_HealJoined`; the run ends | - | `StepHook` area 0x4E (`:237`): arms the run 19, 20 and 22 triggers | progress |
| 62 (`0x3E`) | `EnterAreaBody` area 0x2D, `:365` | First entry to area 0x2D with flag 0x32 set; `Field_SetStatus80` follows | - | `EnterAreaBody` (`:364`): the end runs once | progress |
| 63 (`0x3F`) | `Area49_SetRowFlag3F`, `area_w1c.cpp:533` | Area 49's handler 0 (choice 2), in whatever chapter is current | `Scena05_Run8` case 6, `:801` | Nothing in the source read | transient |

Flags 4, 7, 21, 27, 28, 38, 39, 56 and 57 are not named anywhere in the source read:
nothing sets, clears or tests them while chapter 5 runs.

**Battle order, as the runs chain** (reading, not checked): event battle 17 in area
0x34 (run 4), then 18, 19 and 20 in area 0x4F (run 16, one after the other until one
is won by `0x904AAD` bit 1), then 21 in area 0x51 (run 17), then 22 in area 0x50
(run 18). Each fight's flag is set when the run reaches the step after the battle,
not when the battle starts.

## End

**Where the chapter ends.** `EnterAreaBody`, area 0x2D (45), `scena_sc5.cpp:364-366`:
on an entry with flag 0x32 set and flag 0x3E clear, it sets flag 0x3E and calls
`Field_SetStatus80` (`at::kSetBit80`, `scena_sc5_callees.h:91`). `Area_Enter` has
already passed its chapter test for that entry, so chapter 6 starts on the **next**
area entry (`mode_flow.cpp:548-556`). Flag 0x32 is set by `Scena05_Run20` case 6.
Reading, not checked.

**The path to the end** sets, in order: 0x3D (run 18), 0x30, 0x31, 0x32 (run 20),
0x3E. Run 19 (message 0x84) and run 22 (message 0x4C) are talk runs with no flags.

**The row as left** (reading):

- **Always set:** 0, 1, 2, 5, 6, 8..12, 14..20, 24..26, 29..34, 40..50, 59..62.
- **Alternatives:** 22 or 23. One is set by the area 78 choice. Run 13 can swap
  them, and it always clears the other.
- **Optional, by fight outcome:** 35 (0x23) and 36 (0x24), set by the non-win ends
  of the area 79 fights. 51 and 52 (0x33, 0x34) are left as the last area 79 fight
  left them. 53..55 mark which of kinds 21..23 entered a fight.
- **Cleared before the end:** 58 (0x3A, run 21) and 63 (0x3F, run 8).
- **Never set by the source read:** 3, 4, 7, 13, 21, 27, 28, 37..39, 56, 57.

**Against the saves** ([`observed-rows.md`](observed-rows.md)). Saves 13..17 (JP,
chapter 6) have chapter 5's row as 0..2 5..6 8..12 14..20 23..26 29..35 40..54
59..62. Saves 20..22 (US, chapters 14, 10 and 15) have the same with 55 added.

- Every "always set" flag is set in all of them. **Agree.**
- 23 is set and 22 is clear in all of them: every route took the answer-1 side, or
  run 13's case 5. **Agree** (one of the pair).
- 35 (0x23) is set and 36 (0x24) is clear in all of them: each route had one non-win
  end in area 79 and not two. **Agree**, as an optional flag.
- 51..54 are set in all of them. 55 is set only in the US saves. So kind 23's
  entrance ran on one route and not on the other. Which fight places kind 23 is in
  the battle data, not in the code. **Consistent**, not explained.
- 3, 4, 7, 13, 21, 22, 27, 28, 36..39, 56..58 and 63 are clear in all of them.
  **Agree.**
- Save 19 (JP, 12:48, inside chapter 5, area 90) has 0..2 5..6 8..9. That is run 1,
  object 1, run 4 and run 5 done and run 6 (flag 10, area 0x31's entry) not yet.
  **Agrees** with the order above.

No saved flag in chapter 5's row is unaccounted for by the source.

## Event flags used by this chapter

These are the event-flag bank `Cond_Flags + 0xA0` (save block `+0x650`), not the
chapter row.

| Event flag | Site | What |
|---:|---|---|
| 0x26 (38) | `Scena05_Cell0` / `CellCue`, `scena_sc5.cpp:261`, `:273` | Set by area 0x34's cell cue 0 (flag 6 set, 9 clear), which also moves run 5 to step 1 |
| 0x26 (38) | `EnterAreaBody` area 0x34, counter 2 = 4, `scena_sc5.cpp:400` | Cleared |
| 0x25 (37) | `Scena05_Cell1` / `CellCue`, `scena_sc5.cpp:261`, `:276` | Set by area 0x34's cell cue 1, which moves run 5 to step 0x19 |
| 0x25 (37) | `Area52_ClearFlag25`, `area_w1c.cpp:1000` | Cleared by area 52 (0x34)'s handler 5 |

Area 49 (0x31) code that runs when the chapter is in that area also uses the bank:
`Area49_SwapFlags2To3` clears 2 and sets 3 (`area_w1c.cpp:404-405`), and
`Area49_CellHook` tests 0x1C and toggles the flag named by the area's cell-switch
data (`area_w1c.cpp:524-525`). That code does not depend on the chapter.

Saves 13..22 hold neither 0x25 nor 0x26, which fits both being cleared again.

## Unresolved

- **Flags 3 and 13 (0xD)** are tested (`scena_sc5.cpp:392`, `:416`) but nothing in
  the source read sets them in chapter 5's row. An event script in the area data
  may set them through `Area_TestCondition`'s or an opcode's own row access; the
  area-data scripts were not searched. With 13 never set, every entry to area 0x41
  after flag 12 starts run 9 again. Needs a run.
- **Flag 37 (0x25)**: run 16 step 0x44 sets it, but no site in the source read
  stores step 0x44. The step may come from a battle end phase (`EndStep`'s
  `kEndOther` / `kEndThird`, `boss_sd.cpp:126`) or the data. Unknown.
- **Steps 0x19, 0x22, 0x28, 0x39 and 0x43 of run 16** are no-ops. After a fight end
  that takes the plain step 2 (`kEndOther`), what moves run 16 on is not in the
  source read.
- **Flags 53..55**: which of event battles 18..20 places kinds 21, 22 and 23 is in
  the battle data. Why the JP route left 55 clear is not settled.
- **Row flag 17 (0x11) and effect kind 0xA4**: `EffectKindA4_MarkSprites`
  (`effect_4b.cpp:1030`) tests flag 0x11 of the current row. Its record words come from `Scena15_RecordWord`, so it is most likely a
  chapter 15 effect. Not checked for chapter 5.
- **Effect kind 0x77** (`effect_3d.cpp:319`, `:333`) tests flags 0x14 and 0x2E of
  the current row and hands back run 5 step 0x19. Run 5 step 0x19 exists in chapter
  5, so this is read as a chapter 5 effect. Who spawns kind 0x77 was not found.
  Reading, not checked.
- **Object trigger entry 3** (`Scena05_Object03`, `:307`) starts run 12, which is a
  bare return (`scena_sc5.cpp:160-162`). It holds `ScriptFlags_Set40` with no run to
  clear it. No flag is involved; noted because it looks inert.
- **Area 0x41 with flag 12 clear** sets run 0 (a bare return, `:413-415`). This
  stops whatever run was current. Its purpose is not clear from the code.
- **`Area_TestCondition`** tests `Cond_Flags + 8 * (kind & 0x1F)` from area data, so
  area scripts can test chapter 5's row by number. Those tests are data, not code,
  and are not listed here.
- **The first area of the chapter** is set by whatever area change follows chapter
  4's run 1 step 0x16. Not established from the code.
