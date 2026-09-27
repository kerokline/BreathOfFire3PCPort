# Scenario chapter 0: the scenario harness's proof

**Status:** IN PROGRESS (2026-09-27) - nineteen functions ours
(`src/game/scena_sc0.cpp`, shadow name `scena_sc0`), fuzzed headless through
the scenario harness ([`scenario_harness.md`](scenario_harness.md)): 0
mismatches in 76,000 rounds; 105 of 105 negative controls refused by a count (exit 3). Fuzz only: no recorded route
plays chapter 0 yet (section 8).

Round ten group SCH ([`takeover-queue-round10.md`](takeover-queue-round10.md)
§1), the first scenario group ([`takeover-queue-scenario.md`](takeover-queue-scenario.md)
§3, SC0).

## 1. The band

`0x537F20..0x539AD0`, `tools/scenario_rows.py --unit SC0`: 20 functions
after the descent, `Scenario_NoHook` (`0x539AC0`, ours since round eight)
among them. The start lists had 22 in the band; four are cases of the runs'
jump tables (`0x538620`, `0x538820`, `0x538950`, `0x538A60`, all in
`pc_hidden.json`: a jump table's entries are pointers to code), and two
roots of the chapter's vtable are in no list (`0x539A10` slot 1, `0x539A30`
slot 2). Nothing found outside the band belongs to it; the walk's closure
of chapter 0 runs on into `0x4410B0` (SE's), `Scena16_PartyReset`
(`0x519890`, ours: the call table's one entry) and `Scenario_CallA`.

What chapter 0 is in the story is not stated here: the code shows a
chapter start in area 0x18, set-ups for areas 1, 2, 4, 0x18, 0x19 and 0x1F,
four event battles started by index (1, 4..7), and a steering sequence on
the pad. Which scenes those are is the owner's to say.

## 2. The tables

Named this round in `symbols.toml`:

| Table | At | Entries | Read by |
|---|---|--:|---|
| `Scena00_Hooks` | `0x660CD0` | 5 | the engine: slot 0 `Scena00_Frame`, 1 `Scena00_ObjectHook`, 2 `Scena00_StepHook`, 3 `Scenario_NoHook`, 4 none (0) |
| `Scena00_CallA` | `0x65F664` | 1 | `Scenario_CallA`: `Scena16_PartyReset` |
| `Scena00_States` | `0x660CE4` | 3 | `Scena00_Frame` on the s8 `0x8034E2`, unchecked |
| `Scena00_Runs` | `0x660CF0` | 12 | `Scena00_Run` on the s8 `MoveScript_Var7`, unchecked; runs 0 and 1 a bare `ret` |
| `Scena00_Shake` | `0x660D20` | 16 s8 | `Scena00_Run5` |
| `Scena00_StartSteps` | `0x660D30` | 8 | `Scena00_Run6`'s step 0 by `Cond_ByteFD`, unchecked |
| `Scena00_InputAnims` | `0x660D38` | 16 pairs | `Scena00_Run11` by `Input_Pressed >> 12` |
| `Scena00_ObjectHooks` | `0x660D58` | 1 | `Scena00_ObjectHook` by the object's `+0x86`, unchecked |

`Scena00_States` and `Scena00_Runs` are one run of words, as chapter 16's
([`field-modes.md`](field-modes.md)): a state of 3..14 runs a run handler.

## 3. The functions

| Function | Entry | Bytes | Shape | Does |
|---|---|--:|---|---|
| `Scena00_Frame` | `0x537F20` | 0xE | slot 0 | `jmp` through `Scena00_States` on the state |
| `Scena00_Start` | `0x537F30` | 0xA2 | state 0 | call-table entry 0, area 0x18 at (0x630000, 0xC0000) facing 3 and the same pending, script flag 0x40, 18 dwords at `0x904608` to -1, run 5, the load wait, the counter 0, state 1 |
| `Scena00_EnterArea` | `0x537FE0` | 0x1F3 | state 1 | the set-ups of areas 1, 4, 0x19, 0x1F (effects, camera, flags, members), `Scena00_Area02` and `_Area18` between, state 2 |
| `Scena00_Area02` | `0x5381E0` | 0x116 | direct | area 2: once, elevation, the kind-2 object, a kind-0x13 effect, ObjTrio 0's position; else once, a member and the kind-2 object's position |
| `Scena00_Area18` | `0x538300` | 0x1EC | direct | area 0x18 by `Cond_ByteFD` (a five-case table): music 3 loaded and waited for, the camera, a member, an effect; or two CLUT rows copied and a member; or run 6 / 8 |
| `Scena00_Run` | `0x5384F0` | 0xE | state 2 | `jmp` through `Scena00_Runs` on the run |
| `Scena00_Run2` | `0x538500` | 0x20C | run 2 | 8 steps: script messages 0x10 and 0x11, music 6, transitions, area 4, an effect, area 0x1F and a fade, run 3 |
| `Scena00_Run3` | `0x538710` | 0x158 | run 3 | 4 steps: a transition and an effect, the view test at focus 0x4400 (the view moved back 0x1E0000, then the view shift), music 2 and area 2, run 4 |
| `Scena00_Run4` | `0x538870` | 0x298 | run 4 | 11 steps: messages 1..4 on the counter, two camera pulls, two effects, a transition, a fade, area 0x18, run 5 |
| `Scena00_Run5` | `0x538B10` | 0x478 | run 5 | 18 steps: flags 0..3, a pull, sprite shakes, an effect of kind 0x1E on the ground, transitions, area 0x18 again, music 3, event battle 1, a member; `Rand` picks a shake bit every 16th frame on steps 4..6 |
| `Scena00_Run6` | `0x538F90` | 0x213 | run 6 | a two-level switch (51 steps into 11 cases): three members in, event battles 4, 5, 6 at ObjTrio 0's position, area 0x1F |
| `Scena00_Run7` | `0x5391B0` | 0x24C | run 7 | 9 steps: a member, event battle 7, message 0xE, the camera set with `Field_ViewReset`, an effect, a pull, music 4, area 0x1F |
| `Scena00_Run8` | `0x539400` | 0x2F | run 8 | flag 6, a member, run 0 |
| `Scena00_Run9` | `0x539430` | 0x76 | run 9 | a wait on `0x90384A` 0x71 and a timer, area 1, sound 0x206, run 10 |
| `Scena00_Run10` | `0x5394B0` | 0x11C | run 10 | 4 steps: the camera turned (`0x57C6B0`), random sprite coordinates, area 1, area 0x19, run 11 |
| `Scena00_Run11` | `0x5395D0` | 0x380 | run 11 | 10 steps: the view moved on at focus 0x5000; transitions; the pad steers (`Scena00_Steer`) with sprite 0 animated by direction; a random wobble; area 9 and `0x56D6F0` |
| `Scena00_Steer` | `0x539950` | 0xBA | direct | the area's corner words (`Area_Descriptors[area] +0xC + 0x12`) and two sprite words moved by the pad, sound 0x204 |
| `Scena00_ObjectHook` | `0x539A10` | 0x1F | slot 1 | `Scena00_ObjectHooks[object +0x86](object, the flag row)` |
| `Scena00_StepHook` | `0x539A30` | 0x81 | slot 2 | `(x, z)`, only x's cell read: area 2 cell 0x2B with flag 6 starts run 7; area 0x18, `Cond_ByteFD` 0, cell 0x14, not flag 7 starts run 6 and answers 1 |

Every one is a faithful replacement, read to its last instruction; each
function's comment in `scena_sc0.cpp` says what of the original's order it
keeps (the flag row read afresh for each call; `0x90384A` read again after
`Effect_FindFree` in `Scena00_EnterArea`; `Frame_Counter` read again after
`Rand` in `Scena00_Run5`; `Game_AreaNumber` read again after the flag test
in `Scena00_StepHook`; `Field_Request` stored between two calls where the
original stores it; the whole dword `Sprite_EnsureAnimation` is pushed
with, in `Scena00_Run11`'s wobble the corner pointer's upper three bytes
above the animation).

## 4. The fuzz

`BOF3X_SHADOW=scena_sc0`, `scena_sc0_fuzz.cpp`, one `scenario_harness::Run`
with `chapter` 0:

- **the clones**: nineteen, the clone table as `scenario_rows.py --unit SC0
  --clones` printed it (nine jump tables moved into the copies, 143 call
  sites re-aimed; `Scena00_Run6`'s byte table is read from the original);
  shapes: two slots, a hook (its `al` compared), an object, fifteen states;
- **callees beyond the standard set**: the three functions of the group's
  own called directly (`Scena00_Area02`, `_Area18`, `_Steer`, `kPhase`), and
  `Scena00_ObjectHooks`' entry - a stand-in of the entry's own type written
  into the table by the seed, logging the object and the row it is passed
  (a table's handler recorder logs no arguments);
- **the `.data` tables**: `Scena00_States` (3), `Scena00_Runs` (12), swapped
  for recorders;
- **regions** beyond the standard ones (340 bytes): the 18 dwords at
  `0x904608`, the CLUT rows `0x80BC00` and `0x80FC00`, `0x904EE0`,
  `0x904CD0`, `MapView_Origin`, `Area_Descriptors[0x19]` and a descriptor
  block and corner words of the fuzz's own it points at;
- **the seed** by role: the six areas the chapter tests, `Cond_ByteFD`
  0..4, each run's steps and one past its table (run 6's eleven case steps
  by name), each counter value a step compares with (`0x903848`,
  `0x90384A`, `0x90384B`, `0x903849`), the wait word 0, `Field_Request` 2,
  the timer at 0, 1, 2, 0x21, 0x7B, 0x7C, the focus at 0x4400 / 0x5000,
  `Frame_Counter` on a 32-frame boundary, the pad's direction bits, ObjTrio
  0's `+1` at 2; the state 0..14 for the frame, the run 0..11 for the run
  dispatch, the object's `+0x86` 0; area 0x19 with the fake descriptor for
  the steering and run 11; a hook's cell 0x2B or 0x14;
- **the disturbance** of the chapter's own cells: `0x90384A`, `0x90384B`,
  `0x903849`, ObjTrio 0's `+1` (half the time to a value a step compares
  with); for `Scena00_EnterArea` `0x90384A` to 0 or 3 (area 1 reads it
  again after `Effect_FindFree`), for runs 2 and 4 `Field_Request` off 2
  (their message steps store it between two calls);
- **4,000 rounds** a function.

**Result** (2026-09-27, this worktree):

    shadow      scena_sc0 self-test: 76000 rounds over 19 functions (4000 each), 68403 calls to the stand-ins,
                0 MISMATCHES; 10336 bytes of state (32 regions) and the stand-ins' log compared

Coverage (the originals' calls): every callee and handler the clones name is
reached - e.g. `File_LoadDone` 6,111, `Field_ChangeArea` 4,592,
`Msg_OpenScript` 4,558, `Flags_Test` 1,636, `0x57C6B0` 1,094, the event
battle (`0x4410B0`, `0x532ED0`) 37 each, `Music_LoadFile` 28 - the thinnest
paths, a step reached only on one counter value after a timer. `BOF3X_SHADOW='*'`: exit 0.

## 5. The controls

105 plants, each put in `scena_sc0.cpp` one at a time by a script (the scratch `controls.py`, not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc0` and restored; after the last it restored, rebuilt and ran the clean self-test (exit 0, 0 mismatches). **105 of 105 refused**, every one by exit 3 with a count only in the function the plant touches (this worktree's counts).

The set ran three times. The first (2,000 rounds a function) left E1 (area 1's count not read again after `Effect_FindFree`), R5b (`Frame_Counter` not read again after `Rand`) and R11m (the steering's last count) standing. The second, after seeds for those three and the counters' disturbance to the values the steps compare with, left R2d and R4f (`Field_Request` stored after the second call instead of between the two: the harness's disturbance writes 2 there two times in three, the value both sides store) and R5m (its step reached about once in 2,000). The fuzz as committed moves `Field_Request` off 2 across those steps' calls, seeds the counters five times in six and runs 4,000 rounds a function; the table is its run. No equivalent mutant was planted. The thinnest: R2e (3), E8, R5k, R5m (6), B4 (7) - each a step reached only on one counter value.

| | Function | Planted | Refused in (rounds of 4,000) |
|---|---|---|--:|
| F1 | `Scena00_Frame` | `t<signed char>(B(kState))` -> `t<signed char>(B(kState) == 1 ? 0 : B(kState))` | 259 |
| F2 | `Scena00_Run` | `ast<signed char>(B(kRun))` -> `ast<signed char>(B(kRun) == 5 ? 6 : B(kRun))` | 311 |
| S1 | `Scena00_Start` | `x18, 0x630000, 0xC0000, 3);` -> `x18, 0x630000, 0xC0000, 4);` | 4,000 |
| S2 | `Scena00_Start` | `a < 0x904650` -> `a < 0x90464C` | 4,000 |
| S3 | `Scena00_Start` | `SetL(kCounter, 0); / B(k` -> `SetL(kCounter, 1); / B(k` | 4,000 |
| S4 | `Scena00_Start` | `B(kPending + 3) = 3;` -> `B(kPending + 3) = 2;` | 4,000 |
| S5 | `Scena00_Start` | `Run(5); / Step(0); / WaitLoad();` -> `WaitLoad(); / Run(5); / Step(0);` | 652 |
| E1 | `Scena00_EnterArea` | `t unsigned char again = B(kByte4A);` -> `t unsigned char again = n;` | 16 |
| E2 | `Scena00_EnterArea` | `W(kAngles + 4) + 0xFEABu` -> `W(kAngles + 4) + 0xFEAAu` | 961 |
| E3 | `Scena00_EnterArea` | `SetL(e + 0x1C, 2);` -> `SetL(e + 0x1C, 3);` | 929 |
| E4 | `Scena00_EnterArea` | `if (B(kByteFD) == 2) { / B(` -> `if (B(kByteFD) == 3) { / B(` | 69 |
| E5 | `Scena00_EnterArea` | `SetW(kDistance, 0x100);` -> `SetW(kDistance, 0x101);` | 226 |
| E6 | `Scena00_EnterArea` | `Run(9);` -> `Run(8);` | 144 |
| E7 | `Scena00_EnterArea` | `B(0x8021ED) = 7;` -> `B(0x8021ED) = 6;` | 1,036 |
| E8 | `Scena00_EnterArea` | `SH_CALL(Sound_PlayEffect)(0x200); / Run(0xB);` -> `Run(0xB); / SH_CALL(Sound_PlayEffect)(0x200);` | 6 |
| A1 | `Scena00_Area02` | `Angles, W(kAngles) + 0xAAu);` -> `Angles, W(kAngles) + 0xABu);` | 131 |
| A2 | `Scena00_Area02` | `S(kAngles + 4), 0x40);` -> `S(kAngles + 4), 0x41);` | 130 |
| A3 | `Scena00_Area02` | `SetL(kKind2X, 0x260000);` -> `SetL(kKind2X, 0x260001);` | 98 |
| A4 | `Scena00_Area02` | `LL(Flags_Test)(Row(), 0xA) != 0) ` -> `LL(Flags_Test)(Row(), 0xB) != 0) ` | 290 |
| B1 | `Scena00_Area18` | `ALL(Flags_Test)(row, 6) != 0) ret` -> `ALL(Flags_Test)(row, 6) == 0) ret` | 54 |
| B2 | `Scena00_Area18` | `W(0x80FC20 + i)` -> `W(0x80FC22 + i)` | 27 |
| B3 | `Scena00_Area18` | `kAngles + 2), 0x200, 0x90);` -> `kAngles + 2), 0x200, 0x91);` | 26 |
| B4 | `Scena00_Area18` | `SH_CALL(Party_DropIn)(1); / ret` -> `SH_CALL(Party_DropIn)(2); / ret` | 7 |
| B5 | `Scena00_Area18` | `Run(6); / Set` -> `Run(7); / Set` | 35 |
| B6 | `Scena00_Area18` | `SetW(kAngles + 4, 0x350);` -> `SetW(kAngles + 4, 0x351);` | 28 |
| R2a | `Scena00_Run2` | `SH_CALL(Music_Play)(6, 8);` -> `SH_CALL(Music_Play)(6, 9);` | 2,035 |
| R2b | `Scena00_Run2` | `if (W(kTimer) < 0x7C) Timer` -> `if (W(kTimer) < 0x7B) Timer` | 11 |
| R2c | `Scena00_Run2` | `if (B(kByte4B) != 0x54) return` -> `if (B(kByte4B) != 0x55) return` | 23 |
| R2d | `Scena00_Run2` | `sg_OpenScript)(0x10); / B(kRequest) = 2; / SH_CALL(Music_Play)(6, 8);` -> `sg_OpenScript)(0x10); / SH_CALL(Music_Play)(6, 8); / B(kRequest) = 2;` | 106 |
| R2e | `Scena00_Run2` | `Kind13(S(kAngles) + 0x180,` -> `Kind13(S(kAngles) + 0x181,` | 3 |
| R2f | `Scena00_Run2` | `B(0x937F98) = 0xFF;` -> `B(0x937F98) = 0xFE;` | 22 |
| R3a | `Scena00_Run3` | `if (B(kByte4A) != 0x30) return` -> `if (B(kByte4A) != 0x31) return` | 29 |
| R3b | `Scena00_Run3` | `SetL(kFocusZ, 0x6200);` -> `SetL(kFocusZ, 0x6201);` | 115 |
| R3c | `Scena00_Run3` | `W(0x7E068A) - 0x1Eu` -> `W(0x7E068A) - 0x1Fu` | 115 |
| R3d | `Scena00_Run3` | `_CALL(Music_Play)(2, 0x20);` -> `_CALL(Music_Play)(2, 0x21);` | 43 |
| R4a | `Scena00_Run4` | `* 0x57943u` -> `* 0x57944u` | 14 |
| R4b | `Scena00_Run4` | `* 13u) << 14` -> `* 12u) << 14` | 74 |
| R4c | `Scena00_Run4` | `_cast<int32_t>(0xFFFFFCB6u)` -> `_cast<int32_t>(0xFFFFFCB7u)` | 8 |
| R4d | `Scena00_Run4` | `if (B(kCounter) != 0xB) return` -> `if (B(kCounter) != 0xC) return` | 12 |
| R4e | `Scena00_Run4` | `L(Music_FadeOutStop)(0x20);` -> `L(Music_FadeOutStop)(0x21);` | 2,082 |
| R4f | `Scena00_Run4` | `L(Msg_OpenScript)(4); / B(kRequest) = 2; / SH_CALL(Music_FadeOutStop)(0x20);` -> `L(Msg_OpenScript)(4); / SH_CALL(Music_FadeOutStop)(0x20); / B(kRequest) = 2;` | 135 |
| R5a | `Scena00_Run5` | `gned char bit = count < 8 ?` -> `gned char bit = count < 2 ?` | 503 |
| R5b | `Scena00_Run5` | `frame = Frame_Counter; / }` -> `}` | 39 |
| R5c | `Scena00_Run5` | `if (bit > 1) B(kByt` -> `if (bit > 2) B(kByt` | 496 |
| R5d | `Scena00_Run5` | `c_cast<uint32_t>(t) << 19) >> 16` -> `c_cast<uint32_t>(t) << 18) >> 16` | 175 |
| R5e | `Scena00_Run5` | `x80203E, W(0x80203E) + d); / B(k` -> `x80203E, W(0x80203E) + d + 1); / B(k` | 502 |
| R5f | `Scena00_Run5` | `e[5] = 0x1E;` -> `e[5] = 0x1F;` | 57 |
| R5g | `Scena00_Run5` | `ic_cast<short>(y))) << 16` -> `ic_cast<short>(y))) << 15` | 57 |
| R5h | `Scena00_Run5` | `if (t != 0x20) return` -> `if (t != 0x21) return` | 8 |
| R5i | `Scena00_Run5` | `B(0x904EE0) = 0xFF;` -> `B(0x904EE0) = 0xFE;` | 10 |
| R5j | `Scena00_Run5` | `SetL(0x7DF000, z - d);` -> `SetL(0x7DF000, z + d);` | 29 |
| R5k | `Scena00_Run5` | `PlaceParty(x, z, 1);` -> `PlaceParty(z, x, 1);` | 6 |
| R5l | `Scena00_Run5` | `SH_CALL(Party_DropIn)(6);` -> `SH_CALL(Party_DropIn)(7);` | 117 |
| R5m | `Scena00_Run5` | `Timer(0x3C); / Ste` -> `Timer(0x3D); / Ste` | 6 |
| R5n | `Scena00_Run5` | `if (B(kCounter) != 4) return; / if (SH_CALL(File_LoadDone)() == 0) return` -> `if (SH_CALL(File_LoadDone)() == 0) return; / if (B(kCounter) != 4) return` | 89 |
| R6a | `Scena00_Run6` | `kStartSteps + B(kByteFD)));` -> `kStartSteps + B(kByteFD) + 1));` | 150 |
| R6b | `Scena00_Run6` | `SH_CALL(Party_DropIn)(3);` -> `SH_CALL(Party_DropIn)(4);` | 182 |
| R6c | `Scena00_Run6` | `PlaceParty(x, z, 5);` -> `PlaceParty(x, z, 9);` | 8 |
| R6d | `Scena00_Run6` | `SH_CALL(ScriptFlags_Clear40)(); / B(kCounter) = static_cast<unsigned char>(B(kCounter) + 1);` -> `B(kCounter) = static_cast<unsigned char>(B(kCounter) + 1); / SH_CALL(ScriptFlags_Clear40)();` | 13 |
| R6e | `Scena00_Run6` | `B(0x904CD0) = 4;` -> `B(0x904CD0) = 5;` | 188 |
| R6f | `Scena00_Run6` | `if (B(kObjTrio + 1) == 2) return` -> `if (B(kObjTrio + 1) == 3) return` | 84 |
| R6g | `Scena00_Run6` | `if (step > 0x32) return` -> `if (step > 0x31) return` | 188 |
| R7a | `Scena00_Run7` | `B(kCounter) = 0xA;` -> `B(kCounter) = 0xB;` | 259 |
| R7b | `Scena00_Run7` | `W(kScriptFlags) \| 0x180u` -> `W(kScriptFlags) \| 0x100u` | 162 |
| R7c | `Scena00_Run7` | `SetW(kAngles, 0xFCF6);` -> `SetW(kAngles, 0xFCF7);` | 133 |
| R7d | `Scena00_Run7` | `if (W(kWait) != 0) return; / Timer(W(kTimer) - 1u);` -> `Timer(W(kTimer) - 1u); / if (W(kWait) != 0) return;` | 135 |
| R7e | `Scena00_Run7` | `W(kDistance) - 0x24u` -> `W(kDistance) - 0x23u` | 249 |
| R7f | `Scena00_Run7` | `SH_CALL(Music_Play)(4, 8);` -> `SH_CALL(Music_Play)(4, 7);` | 10 |
| R7g | `Scena00_Run7` | `_cast<int32_t>(0xFFFFFD56u)` -> `_cast<int32_t>(0xFFFFFD57u)` | 13 |
| R8a | `Scena00_Run8` | `_CALL(Flags_Set)(Row(), 6);` -> `_CALL(Flags_Set)(Row(), 5);` | 946 |
| R8b | `Scena00_Run8` | `SH_CALL(Party_DropIn)(2);` -> `SH_CALL(Party_DropIn)(3);` | 946 |
| R9a | `Scena00_Run9` | `if (B(kByte4A) != 0x71) return` -> `if (B(kByte4A) != 0x70) return` | 42 |
| R9b | `Scena00_Run9` | `L(Sound_PlayEffect)(0x206);` -> `L(Sound_PlayEffect)(0x207);` | 82 |
| R9c | `Scena00_Run9` | `Run(0xA); / }` -> `Run(0x9); / }` | 82 |
| R10a | `Scena00_Run10` | `if (TurnCamera(0x2D, 0xA) !` -> `if (TurnCamera(0x2E, 0xA) !` | 564 |
| R10b | `Scena00_Run10` | `SH_CALL(Rand)() & 7) + 0x69` -> `SH_CALL(Rand)() & 3) + 0x69` | 182 |
| R10c | `Scena00_Run10` | `W(kScriptFlags) & 0xFF7Fu` -> `W(kScriptFlags) & 0xFF7Eu` | 14 |
| R10d | `Scena00_Run10` | `Step(3); / B(kByte4A) = 4;` -> `Step(3); / B(kByte4A) = 5;` | 184 |
| R10e | `Scena00_Run10` | `if (B(kByte4A) & 1) {` -> `if (B(kByte4A) & 2) {` | 228 |
| R11a | `Scena00_Run11` | `char>(SH_CALL(Rand)() & 7);` -> `char>(SH_CALL(Rand)() & 6);` | 1,028 |
| R11b | `Scena00_Run11` | `if (B(kByte4A) < 0x14 &&` -> `if (B(kByte4A) <= 0x14 &&` | 57 |
| R11c | `Scena00_Run11` | `L(0x8020DC, s2 + 0x200000);` -> `L(0x8020DC, s2 + 0x200001);` | 302 |
| R11d | `Scena00_Run11` | `L(Sound_PlayEffect)(0x203);` -> `L(Sound_PlayEffect)(0x202);` | 15 |
| R11e | `Scena00_Run11` | `if ((pressed & 0xF0) == 0 ` -> `if ((pressed & 0x70) == 0 ` | 13 |
| R11f | `Scena00_Run11` | `heck && B(kCount2) > 0x1E)` -> `heck && B(kCount2) > 0x1D)` | 159 |
| R11g | `Scena00_Run11` | `int32_t>(W(kInput)) >> 12) << 1;` -> `int32_t>(W(kInput)) >> 13) << 1;` | 77 |
| R11h | `Scena00_Run11` | `SetL(0x8021AC, S(p) >= 0 ? 0x4` -> `SetL(0x8021AC, S(p) > 0 ? 0x4` | 15 |
| R11i | `Scena00_Run11` | `EnsureAnimation((p & 0xFFFFFF00u) \| animatio` -> `EnsureAnimation(animatio` | 57 |
| R11j | `Scena00_Run11` | `ast<unsigned>((r & 7) - 3));` -> `ast<unsigned>((r & 7) - 2));` | 114 |
| R11k | `Scena00_Run11` | `W(kScriptFlags) & 0xFFB7u` -> `W(kScriptFlags) & 0xFFBFu` | 27 |
| R11l | `Scena00_Run11` | `Status80();` -> `(removed)` | 56 |
| R11m | `Scena00_Run11` | `if (c > 0x14) {` -> `if (c > 0x13) {` | 953 |
| R11n | `Scena00_Run11` | `Sprite_Current = Sprite_Objects; / B(0x7DEE` -> `B(0x7DEE` | 45 |
| St1 | `Scena00_Steer` | `if (B(kInput + 1) & 0x20) {` -> `if (B(kInput + 1) & 0x08) {` | 1,231 |
| St2 | `Scena00_Steer` | `021AC, L(0x8021AC) - 0x40);` -> `021AC, L(0x8021AC) - 0x41);` | 1,212 |
| St3 | `Scena00_Steer` | `SetW(L(kCorner) + 2, 0);` -> `SetW(L(kCorner) + 2, 1);` | 4,000 |
| St4 | `Scena00_Steer` | `L(Sound_PlayEffect)(0x204); / } /` -> `L(Sound_PlayEffect)(0x205); / } /` | 4,000 |
| O1 | `Scena00_ObjectHook` | `fn(object, row);` -> `fn(object, row + 1);` | 4,000 |
| O2 | `Scena00_ObjectHook` | `fn(object, row);` -> `fn(row, object);` | 4,000 |
| H1 | `Scena00_StepHook` | `if (cell != 0x2B) return` -> `if (cell != 0x2A) return` | 168 |
| H2 | `Scena00_StepHook` | `Run(7); / Ste` -> `Run(8); / Ste` | 113 |
| H3 | `Scena00_StepHook` | `if (B(kByteFD) != 0) return` -> `if (B(kByteFD) != 1) return` | 39 |
| H4 | `Scena00_StepHook` | `Step(0); / return 1; / }` -> `Step(0); / return 2; / }` | 13 |
| H5 | `Scena00_StepHook` | `t<std::uint32_t>(x) >> 16` -> `t<std::uint32_t>(x) >> 15` | 190 |
| H6 | `Scena00_StepHook` | `L(Flags_Test)(Row(), 7) != 0) ret` -> `L(Flags_Test)(Row(), 7) == 0) ret` | 21 |

## 6. Cross-group calls

| Address | What | Owner |
|---|---|---|
| `0x4410B0` | an event battle's set-up by index (`0x904AAA`, `0x802D41` = 5) | SE (round ten) |
| `0x532ED0` | the party placed at (x, z) for event battle n (`0x903780` / `84`) | nobody (before the bank) |
| `0x57C6B0` | the camera turned toward an s16 angle at an s8 speed, `al` 1 while turning | nobody (engine) |
| `0x56FCA0` | the view shift after a focus test (PSX `0x80155154`) | nobody (engine) |
| `0x56D6F0` | `Field_StatusBits |= 0x80` | nobody (engine) |

All by raw address (`scena_sc0_callees.h`), in the harness's standard set;
round ten's rebinding pass names them once taken.

## 7. Latent defects (Capcom's, kept)

For the coordinator to number; nothing is fixed.

- **The three `.data` dispatches are unchecked.** A state of 15 or more (or
  negative), a run of 12 or more, an object `+0x86` above 0 read the words
  after their tables - `0x100FF` .. or 0 - and jump there. Ours aborts with
  a message where the word read is not code (a state of 3..14 is a run
  handler, and ours runs it as the original does). Nothing sets them there.
- **`Scena00_StartSteps` is indexed by `Cond_ByteFD` unchecked**: above 7 it
  reads `Scena00_InputAnims` as a step. A byte read; faithful.
- **`Scena00_Steer` and run 11's wobble index `Area_Descriptors` by
  `Game_AreaNumber` unchecked** (200 entries) and write through the block's
  `+0xC`. Ours aborts past 199.
- **`Scena00_Run5`'s `Rand % 3` as a shift count**: a negative `Rand` (the
  CRT's never is) would shift the byte out to 0. Faithful.
- **Run 11's step 5 leaves the timer at 0xFFFF** when the counter `0x903849`
  passes 0x1E: it sets the timer 0 and then decrements it, so the step
  changes to 6 by the other test only. Faithful.

## 8. What reaches it

Nothing recorded: no route plays chapter 0 (a new game), and the attract
cycle runs chapter 16 only. The live check is a new game played through the
chapter under original and ours with the frame hash compared
([`takeover-queue-scenario.md`](takeover-queue-scenario.md) §5: chapter 0
needs no save). `Scena00_Frame` and `Scena00_StepHook` are called by the
engine every field frame and step of the chapter; nothing in the attract
path reaches them, so the frame hash is untouched.

No divergence and no ledger entry: every function is a faithful
replacement; where the original would jump through a table to what is not
code, or index `Area_Descriptors` past its end, ours aborts (the round-nine
precedent; no DIVERGENCE entry).

`entries_logic.txt`: 18 lines appended to the main checkout's copy under a
`group SCH` comment (`0x5381E0 116` was listed already); the host lines
`00537ED0 303` and `00538300 17B1` run over the chapter and were not edited.
