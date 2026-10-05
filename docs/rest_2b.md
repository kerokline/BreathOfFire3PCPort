# Group R2B: Effect_Spawn, two menu primitives, and game mode 8's step 8 screen

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, from the
round branch's tip `fa583bc`. **38 functions ours** (`src/game/rest_2b.cpp`,
`rest_2b.h`, `rest_2b_callees.h`, shadow name `rest_2b`): 36 of the cut's 39
rows for R2B (`analysis/round14_cut.tsv`) and two starts no list had
(`0x57E720`, `0x57EDF0`); the other three rows are jump-table cases (section
5). Each read to its last instruction with capstone and fuzzed through the
scenario harness's **field** mode: 152,000 rounds, **0 mismatches**; **106
controls, 103 refused**, the other three equivalent mutants each with a
refused near variant (section 6). `BOF3X_SHADOW='*'` exit 0, narrow and with
`BOF3X_WIDE=1` (section 4). Eight `.data` tables named. Fuzz only: no
recorded route or trace enters the band (section 9). **Two harness rows
were edited** (one token each, section 10): taking `Effect_Spawn` /
`Effect_SpawnAt` makes `Register` refuse their `THEIRS` rows, and no other
change lets `'*'` pass. **One latent defect is a ledger entry, DIV-0073**
(section 7, L1; entered 2026-10-04 and kept by the owner the same day).

**What the band is.** By the code, four things:

- **`Effect_Spawn` / `Effect_SpawnAt`** (`0x57CE10`, `0x57CE80`): the two
  spawners of an `Effect_Objects` record (kind 6 with two words, kind `0x19`
  with three dwords) that the movement-script ops and some sixty area
  handlers call - named in 2026-09-22 and bound here, the names verified.
- **Two menu primitives the community band calls by address**: a grey
  horizontal line (`Menu_DrawGreyHLine`, `0x57CEF0`) and the window colour's
  outline with notched corners (`Menu_DrawOutlineNotched`, `0x57D520`,
  `Menu_DrawOutline`'s sibling).
- **`Field_ObjectTriggers[1]`** (`Field_TriggerCounterF0`, `0x56E040`).
- **Game mode 8's step 8 screen** (`Shisu_*`, 32 functions): `Mode8_Step8`
  (`0x517340`, FC3's) calls `Shisu_ModeDispatch` every frame. The cut paired
  two of its rows with PSX functions of the **SHISU** overlay (`0x801D1018`,
  `0x801D2308`; the sibling's `names/overlays.toml` names the overlay, not the
  functions), so the prefix is the overlay's file name, a label and not a
  claim about play. What it does, from the code only: on the menu block
  `0x929F00` (mode, state `+1`, step `+2`, timer `+4`) it counts four items
  of the inventory (`0x4D`, `0x23`, `0x24`, `0x56`), sets two model records up
  (`0x9398E0` "A", `0x939960` "B", laid out as the sprite records
  `Sprite_ObjectMatrix` reads, their models from the file the pointer
  `0x628C88` holds), lets the player pick a side and then put up to one of the
  first item and up to 20 of each of the others into four "given" counts, runs
  a show (both models square their turn, drop to the ground, flash and fade),
  up to eight times, and at the end takes the given items, scores them and
  hands the scenario its step (`0x8034E5` 5 with no show run, else `0x14`),
  the counter byte `0x90384B` (1 for a score of `0x32` or less, else 2) and a
  rank `0x903F6A` (0..5). What the screen is in the game is the owner's to
  say.

| Function | Address | Size | Reached by | What |
|---|---|--:|---|---|
| `Field_TriggerCounterF0` | `0x56E040` | 0xA | `Field_ObjectTriggers[1]` | the counter `0x903848` = `0xF0`; al 0 |
| `Effect_Spawn` | `0x57CE10` | 0x63 | 77 E8 sites (64 ours) | an effect record of kind 6: +6 kind, +0xC / +0x10 two s8, +0x2E / +0x30 two words; al the slot |
| `Effect_SpawnAt` | `0x57CE80` | 0x69 | 6 E8 sites | kind `0x19`, the dwords at +0x34 / +0x38 / +0x3C |
| `Menu_DrawGreyHLine` | `0x57CEF0` | 0x6F | R4D's `0x45D730`, `0x45E2C0` | LINE_F2 (x, y)..(x + w, y), grey `0x28` / `0x8C` |
| `Menu_DrawOutlineNotched` | `0x57D520` | 0x23A | R4B's `0x458D70` (4), R4E's (7) | the outline with corners cut by flag bits 4..7 |
| `Shisu_ModeDispatch` | `0x57DFF0` | 0x11 | `Mode8_Step8` | jmp through `Shisu_Modes` by `0x929F00` |
| `Shisu_Begin` | `0x57E010` | 0x5B | `Shisu_Modes[0]` | counts, zeroes, scale index, models; mode + 1 |
| `Shisu_OpenDispatch` | `0x57E070` | 0xE | `Shisu_Modes[1]` | jmp through `Shisu_OpenStates` by `0x929F01` |
| `Shisu_OpenFade` | `0x57E080` | 0x16 | `Shisu_OpenStates[0]` | `Transition_Start(3)`, the windows; state + 1 |
| `Shisu_OpenWait` | `0x57E0A0` | 0x35 | `Shisu_OpenStates[1]` | backdrop; wait word 0: message `0x2B`, mode + 1 |
| `Shisu_CloseDispatch` | `0x57E0E0` | 0xE | `Shisu_Modes[3]` | jmp through `Shisu_CloseStates` |
| `Shisu_CloseFade` | `0x57E0F0` | 0x21 | `Shisu_CloseStates[0]` | backdrop, `Transition_Start(2)`, windows closing; state + 1 |
| `Shisu_CloseWait` | `0x57E120` | 0x3B | `Shisu_CloseStates[1]` | backdrop while waiting; then mode + 1, windows 1..3 off |
| `Shisu_Result` | `0x57E160` | 0x125 | `Shisu_Modes[4]` | items taken, score, rank, scenario step; `Window_ResetAll`, `Game_Step` + 1 |
| `Shisu_PickDispatch` | `0x57E290` | 0x1D | `Shisu_Modes[2]` | backdrop, jmp through `Shisu_PickStates` |
| `Shisu_PickSide` | `0x57E2B0` | 0x1D8 | `Shisu_PickStates[0]` | the side, into the counts, a show, or leave; window 0, the models |
| `Shisu_PickCounts` | `0x57E490` | 0x28C | `Shisu_PickStates[1]` | the row 0..3 and the given counts (a 4-entry jump table); window 0, the models |
| `Shisu_PickShow` | `0x57E720` | 0x18 | `Shisu_PickStates[2]` | call through `Shisu_ShowSteps` by `0x929F02`; the models |
| `Shisu_ShowStart` | `0x57E740` | 0x13 | `Shisu_ShowSteps[0]` | both models state 2 |
| `Shisu_ShowDrop` | `0x57E760` | 0x25 | `Shisu_ShowSteps[1]` | both done: state 3 |
| `Shisu_ShowLanded` | `0x57E790` | 0x76 | `Shisu_ShowSteps[2]` | both done: state 4, colours kept, timer 0 |
| `Shisu_ShowFlash` | `0x57E810` | 0xAA | `Shisu_ShowSteps[3]` | colours floored at the timer, timer + 8 (sound `0x200` at `0x80`) |
| `Shisu_ShowFade` | `0x57E8C0` | 0xDA | `Shisu_ShowSteps[4]` | timer - 8; at 0 colours back, A state 5, B 1, rounds + 1, back to the pick |
| `Shisu_CountItems` | `0x57E9A0` | 0x44 | `Shisu_Begin` | four `Inventory_Count`s into `0x9399E0..E3` |
| `Shisu_SetupWindows` | `0x57E9F0` | 0xC4 | `Shisu_OpenFade` | `WindowRecords` 0..3's bytes and words |
| `Shisu_CloseWindows` | `0x57EAC0` | 0x19 | `Shisu_CloseFade` | window 0 off, 1..3's +3 = 1 |
| `Shisu_ScaleIndex` | `0x57EAE0` | 0x37 | `Shisu_Begin` | the byte `0x904101` to 9..`0x10` (al) |
| `Shisu_Score` | `0x57EB20` | 0x19F | `Shisu_Result` | four terms and the score `0x9399FC` |
| `Shisu_InitModels` | `0x57ECC0` | 0x108 | `Shisu_Begin` | both models at the kind-2 sprite's place, B 0x200 below the ground, A 0x200 above |
| `Shisu_ModelBDispatch` | `0x57EDD0` | 0xE | the three pick states | jmp through `Shisu_ModelBStates` by `0x939961` |
| `Shisu_ModelBTurn` | `0x57EDE0` | 0xC | `Shisu_ModelBStates[1]` | angle + 0x20, draw |
| `Shisu_ModelBDraw` | `0x57EDF0` | 0xF5 | four B states (jmp) | scale by the given counts, colour tinted, `Shisu_DrawModel`, colour back |
| `Shisu_DrawModel` | `0x57EEF0` | 0x377 | `Shisu_ModelBDraw`, R2C's `0x57F340` | a model record's quads as POLY_FT4s |
| `Shisu_ModelBSquare` | `0x57F270` | 0x2E | `Shisu_ModelBStates[2]` | turn on by 0x40 to a whole turn; done flag |
| `Shisu_ModelBDrop` | `0x57F2A0` | 0x6C | `Shisu_ModelBStates[3]` | y + 0x200000 to the ground; done flag |
| `Shisu_ModelBStill` | `0x57F310` | 0x5 | `Shisu_ModelBStates[4]` | draw |
| `Shisu_ModelADispatch` | `0x57F320` | 0xE | the three pick states (jmp) | jmp through `MasterFigure_States` by `0x9398E1` |
| `Shisu_ModelATurn` | `0x57F330` | 0xC | `MasterFigure_States[1]` | angle - 0x20, R2C's draw `0x57F340` |

## 1. The tables named

Each count is its reader's reach, read by hand (the tool's counts ran on into
the next table): the dispatchers bound nothing, and every writer of each
index stores a value inside the count.

| Table | At | Count | Read by | Notes |
|---|---|--:|---|---|
| `Shisu_ModelScales` | `0x663D7C` | 21 | `Shisu_ModelBDraw` [1..20]; R2C's `0x57F340` [9..0x10] | dwords, scale 0x1000 = 1 |
| `Shisu_Modes` | `0x663DD0` | 5 | `Shisu_ModeDispatch` | mode 4 never moves on itself |
| `Shisu_OpenStates` | `0x663DE4` | 2 | `Shisu_OpenDispatch` | |
| `Shisu_CloseStates` | `0x663DEC` | 2 | `Shisu_CloseDispatch` | |
| `Shisu_PickStates` | `0x663DF4` | 3 | `Shisu_PickDispatch` | |
| `Shisu_ShowSteps` | `0x663E00` | 5 | `Shisu_PickShow` (a `call`) | |
| `Shisu_ModelBStates` | `0x663E14` | 5 | `Shisu_ModelBDispatch` | entry 0 `BareRet` |
| `MasterFigure_States` | `0x663E28` | 6 | `Shisu_ModelADispatch` | entry 0 `BareRet`, 1 ours, 2..5 R2C's `0x57F420`, `0x57F450`, `0x57F4E0`, `0x57F4F0` |

The seven state tables lie back to back from `0x663DD0` to `0x663E3F`; the
tools read them as one run of code pointers (`0x663DD0`, "39 code entries").
`0x663E40` onwards is R2C's (`0x57F500` jumps through it).

## 2. Divergence

None in behaviour: every function is Capcom's to the byte it writes, with
two places where the original's bytes cannot be reproduced (section 7, L1
and L2) - **L1 is DIV-0073** (entered 2026-10-04, kept by the owner).

## 3. Arguments and answers

- `Effect_Spawn` / `Effect_SpawnAt` keep the 2026-09-22 signatures (bytes,
  words, longs); the fuzz hands them random words and compares, so the
  narrow parameters are proved read as the original reads them. al answers
  (ret_mask `0xFF`): `Effect_FindFree`'s slot, or `0xFF`.
- `Field_TriggerCounterF0(object, flags)`, al 0; neither argument read.
- `Menu_DrawGreyHLine(int x, int y, unsigned w, unsigned bright)`: x and y
  read as s16, w as u16, bright's byte. `Menu_DrawOutlineNotched(int x, int
  y, int w, int h, int flags)` as `Menu_DrawOutline`.
- `Shisu_ScaleIndex` answers in al (ret_mask `0xFF`); `Shisu_DrawModel(record)`
  takes the model record. Everything else is `void(void)`.
- **The original writes into its own argument slots** - `Menu_DrawGreyHLine`
  uses two as `fild` scratch, `Menu_DrawOutlineNotched` keeps two
  intermediate coordinates in arg 1's and arg 4's, `Shisu_DrawModel` stores
  the caller's `Sprite_Current` in its argument slot. Every caller (read:
  the community sites listed in section 8, `0x57EEBF`, R2C's `0x57F3EF`)
  pops them unread; ours keeps those values in locals.

## 4. The fuzz (`rest_2b_fuzz.cpp`)

`scenario_harness::Run` in field mode, 4,000 rounds a function, the screen's
states and steps `kMenu` (`menu_span` 2, the dispatchers' indexes seeded
below their own tables), the models' states `kState`, the rest `kCall`.
`BOF3X_R2B_ONLY=<name>` runs the clones whose name contains it.

**Regions beyond field mode's**: the two models and the screen's cells
`0x9398E0` + `0x120`, the kept colours `0x6BC878` + 8, `WindowRecords`
(22 x `0x24`), the rank byte (`0x903F68` + 4), `Game_Mode` / `Game_Step`,
`Prim_VertexScratch`, the model file pointer `0x628C88`, and the window
colours of styles 0..7 (`0x80B7A8` + `0x200`; the style byte seeded below 8).
25,272 bytes of state in 46 regions.

**Seeds**: every menu byte near its bounds (the mode 0..4 and `0xFF`, the
state 0..3 and `0xFF`, the step 0..4 and `0xFF`, the timer at 0, 8, `0x78`,
`0x80`, `0x88`, `0xF0`, `0xF8`); both models' states inside their tables
(their dispatchers are called from three functions); the four counts at 0, 1,
2; the given counts at 0, 1, 2 and `0x13..0x15`; the rounds at 0..3, 7..9;
the side at 0, 1 and past; the row at 0..4 and `0xFF`; the scale index and
the level byte at each of `Shisu_ScaleIndex`'s seven thresholds and one
below; model B's colour at `0xFF - 3 g2` and `3 g2`, give or take one; the
angle at whole turns and one off; each model's header (count 0..4, so the
quads stay inside the harness's scratch; flags with and without `0x40`,
`0x80`) and four quads - the first four `Sprite_Objects` records get the
same pointers, since the disturbance may move `Sprite_Current` onto one
inside `Shisu_DrawModel`; the pad's pressed, confirm and cancel words at the
values the branches test; the wait word 0 or not.

**Stand-ins** (the group's listing; masks by what each callee reads):
`Input_AutoRepeat` answers each branch's bit (`0xA000`, `0x1000`, `0x4000`,
none) or the pressed word; `Shisu_Score` writes the score at `Shisu_Result`'s
thresholds +/- 1; `AreaMap_Elevation` answers at model B's y's high word +/- 1;
`Gfx_CommitPrim` moves the packet cursor by its size; `Sprite_ObjectMatrix`
fills the whole matrix (stack, logged by bytes), `Gte_ScaleMatrix` changes
it in place, `Light_ObjectDirection` fills its three shorts,
`Gte_RotTransPers4` writes the four screen points as small whole floats,
`Gte_PrimDepths4_10` the four depths, `Gte_VectorNormalS` its out,
`Gte_NormalColor` copies the colour in over the out (as shipped); the stack
matrices are logged by their bytes (masks 0), `Gte_SetMatrix2`'s by its
first row only (L1). `Menu_DrawLine` compares the coordinates' low words,
the colour bytes and abr & 3 (the original passes dwords with leftovers above
them, section 3). `Inventory_Remove` compares each argument's byte.

**Disturbance** (from the hash): the mode, state, step, timer, rounds,
side, row, a given count, model B's y and angle, `Game_Step`, the file
pointer (between two valid places), the kind-2 sprite's x or z - each a cell
a function reads again after a call.

**Result** (this worktree, the committed fuzz): `BOF3X_SHADOW=rest_2b`, exit
0: 152,000 rounds over 38 functions, 310,617 calls to the stand-ins, **0
mismatches**; every callee listed was reached, every entry of the seven
tables (the handler recorders' phase counts about 600..2,000 each),
`Gte_ScaleMatrix` 2,054, the lit path (`Gte_NormalColor`) 2,911, the
committed quads 2,593 of 7,409. The harness names each clone "outside the
field runs" (it does not refuse them; the band lies between FS's runs).

**`BOF3X_SHADOW='*'`** (this worktree, the committed tree): exit 0, 725
self-test lines, none with a mismatch (`rest_2b`'s 310,296 calls there - the
stream depends on what ran before). With `BOF3X_WIDE=1`: exit 0, 725 self-test lines, none
with a mismatch. `tools/ledger_check.py`: 0 errors.
The first two `'*'` runs stopped at `Register` on rows naming
`Effect_Spawn` / `_SpawnAt` as Capcom's (section 10); neither died silently.

## 5. What the cut and the tool said, settled

- **Three rows are jump-table cases, not functions** (each `pop ebx` or
  falls into its host's shared `ret`, and is reached only through its host's
  table in `.text`): `0x551E40` is case 2 of `Scena08_Scene4` `0x551DE0`'s
  table `0x551F08`; `0x553E50` is case 8 of the table `0x554244` in
  `Scena09_EnterArea` `0x553B40`; `0x559AD0` is case 10 of `Scena10_Run5`
  `0x559970`'s table `0x559AF0`. All three hosts are ours and contain the
  code (round ten's SC7, SC9A, SC9B declined the same starts:
  [`scena_sc7.md`](scena_sc7.md), [`scena_sc9a.md`](scena_sc9a.md),
  [`scena_sc9b.md`](scena_sc9b.md)). The tool's "hidden in" hosts for them
  (`0x551060`, `0x553730`, `0x559930`) were the nearest starts, not the
  switches; the cut's sizes (288, 1072, 6784) were the catalogue's guesses.
- **Two starts no list had**: `0x57E720` (the tool's "code no list has" after
  `0x57E490`'s jump table: `Shisu_PickStates[2]`) and `0x57EDF0` (inside
  `0x57EDE0`'s tool extent of `0x105`: a frame and `ret` of its own, the
  `jmp` target of four states - taken as `Shisu_ModelBDraw`, and
  `0x57EDE0` cut to its `0xC`).
- Every other extent is the tool's (21 differ from the cut by padding only;
  `0x57E490`'s `0x28C` includes its jump table `0x57E70C`).
- The cut's labels ("Boot: field, map and sprites", "minigames, master",
  `area_w3c`) were hints; the `hypothesis` rows `0x57CE10`, `0x57CE80`,
  `0x57CEF0`, `0x57D520`, `0x57DFF0` are all functions.

## 6. Controls

106 plants, one or more a function, each in `rest_2b.cpp` by a unique
anchor: the scratch script `controls.py` (session `309e3952` scratchpad,
`r2b/`) plants, rebuilds, runs `BOF3X_SHADOW=rest_2b` with `BOF3X_R2B_ONLY`
naming the function, restores and rebuilds at the end (the tree clean
after). **103 refused by a count** (exit 3, the harness's Fatal), **3 not,
each an equivalent mutant** with a near variant planted and refused (C72 /
C102, C75 / C103, C82 / C104). Every count is from the last run, at the
committed fuzz.

**What the controls fixed in the fuzz** (each first not refused, then
refused once the fuzz could see it): C28 (the wait word never seeded at 1),
C94 (the angle never seeded with 0x800 alone), and C104 / C105 / C106 -
`Shisu_ModelBDraw`'s scale and tint exist only while `Shisu_DrawModel` runs
(the colour is put back after it), so the draw's stand-in hashes the whole
record now. Before the first control run C86 and C96 were predicted blind
and fixed (the winding stand-in answers 0, 1, -1 and a high half with ax 0;
model B's y seeded on whole units).

| Control | Run on | Plant | Rounds refused |
|---|---|---|--:|
| C1 | `Field_TriggerCounterF0` | counter 0xF1 | 4000 |
| C2 | `Field_TriggerCounterF0` | al 1 | 4000 |
| C3 | `Effect_Spawn` | kind 7 | 3812 |
| C4 | `Effect_Spawn` | z from x | 3812 |
| C5 | `Effect_Spawn` | +0x10 from a | 3808 |
| C6 | `Effect_SpawnAt` | kind 0x18 | 3805 |
| C7 | `Effect_SpawnAt` | z from y | 3805 |
| C8 | `Effect_SpawnAt` | al slot ^ 1 | 3805 |
| C9 | `Menu_DrawGreyHLine` | shades swapped | 4000 |
| C10 | `Menu_DrawGreyHLine` | x1 + 1 | 4000 |
| C11 | `Menu_DrawGreyHLine` | w signed | 2028 |
| C12 | `Menu_DrawOutlineNotched` | abr swapped | 4000 |
| C13 | `Menu_DrawOutlineNotched` | gate by n3 | 1126 |
| C14 | `Menu_DrawOutlineNotched` | blue field | 3873 |
| C15 | `Menu_DrawOutlineNotched` | row stride | 3466 |
| C16 | `Shisu_ModeDispatch` | next mode | 4000 |
| C17 | `Shisu_Begin` | level + 1 | 4000 |
| C18 | `Shisu_Begin` | mode read before the calls | 39 |
| C19 | `Shisu_Begin` | rounds 1 | 3977 |
| C20 | `Shisu_OpenDispatch` | next state | 4000 |
| C21 | `Shisu_OpenFade` | transition 2 | 4000 |
| C22 | `Shisu_OpenFade` | calls swapped | 4000 |
| C23 | `Shisu_OpenWait` | message 0x2C | 2001 |
| C24 | `Shisu_OpenWait` | wait test | 1014 |
| C25 | `Shisu_CloseDispatch` | next state | 4000 |
| C26 | `Shisu_CloseFade` | transition 3 | 4000 |
| C27 | `Shisu_CloseWait` | window 3 byte 1 | 1987 |
| C28 | `Shisu_CloseWait` | wait test | 1040 |
| C29 | `Shisu_Result` | step 6 | 840 |
| C30 | `Shisu_Result` | rank 2 bound | 131 |
| C31 | `Shisu_Result` | low bound | 127 |
| C32 | `Shisu_Result` | items swapped | 3120 |
| C33 | `Shisu_Result` | rank 5 bound | 126 |
| C34 | `Shisu_PickDispatch` | next state | 4000 |
| C35 | `Shisu_PickDispatch` | state read before the backdrop | 43 |
| C36 | `Shisu_PickSide` | flip mask | 479 |
| C37 | `Shisu_PickSide` | cursor 2 | 661 |
| C38 | `Shisu_PickSide` | eight bound | 22 |
| C39 | `Shisu_PickSide` | side-1 confirm dropped | 167 |
| C40 | `Shisu_PickSide` | window place | 2669 |
| C41 | `Shisu_Pick` | stack factor | 6236 |
| C42 | `Shisu_PickSide` | and to or | 6 |
| C43 | `Shisu_PickCounts` | top bound | 237 |
| C44 | `Shisu_PickCounts` | bottom bound | 101 |
| C45 | `Shisu_PickCounts` | row 0 cap | 9 |
| C46 | `Shisu_PickCounts` | row 2 prerequisite | 27 |
| C47 | `Shisu_PickCounts` | row 1 model | 14 |
| C48 | `Shisu_PickCounts` | row height | 4000 |
| C49 | `Shisu_PickCounts` | any-button message dropped | 2926 |
| C50 | `Shisu_PickCounts` | row 3 cap | 3 |
| C51 | `Shisu_PickShow` | next step | 4000 |
| C52 | `Shisu_PickShow` | models swapped | 4000 |
| C53 | `Shisu_ShowStart` | A state 3 | 4000 |
| C54 | `Shisu_ShowDrop` | or to and | 1486 |
| C55 | `Shisu_ShowLanded` | saved from B | 2268 |
| C56 | `Shisu_ShowLanded` | timer 1 | 2276 |
| C57 | `Shisu_ShowFlash` | sound at 0x88 | 890 |
| C58 | `Shisu_ShowFlash` | timer 0xF0 | 471 |
| C59 | `Shisu_ShowFlash` | timer not re-read | 6 |
| C60 | `Shisu_ShowFlash` | A red from green | 2686 |
| C61 | `Shisu_ShowFade` | step 4 | 4000 |
| C62 | `Shisu_ShowFade` | A state 4 | 462 |
| C63 | `Shisu_ShowFade` | state - 1 | 462 |
| C64 | `Shisu_ShowFade` | B red from green | 461 |
| C65 | `Shisu_CountItems` | item 0x57 | 4000 |
| C66 | `Shisu_SetupWindows` | word 0xFF89 | 4000 |
| C67 | `Shisu_SetupWindows` | pointer to the given | 4000 |
| C68 | `Shisu_CloseWindows` | window 2 byte 2 | 4000 |
| C69 | `Shisu_ScaleIndex` | 0x2D bound | 222 |
| C70 | `Shisu_ScaleIndex` | 0x44 bound | 260 |
| C71 | `Shisu_Score` | A numerator | 1147 |
| C72 | `Shisu_Score` | ramp edge | **0 - equivalent: at den = edge the quotient is exactly 100 (num = 100 edge), so q and 200 - q agree; near variant C102 refused** |
| C73 | `Shisu_Score` | C base | 1320 |
| C74 | `Shisu_Score` | part by g2 | 449 |
| C75 | `Shisu_Score` | unsigned division | **0 - equivalent: the total is never negative (every term floored at 0, the products far below 2^31), so signed and unsigned division agree; near variant C103 refused** |
| C76 | `Shisu_InitModels` | file not re-read | 3 |
| C77 | `Shisu_InitModels` | B below 0x100 | 3985 |
| C78 | `Shisu_InitModels` | B blue 0x81 | 4000 |
| C79 | `Shisu_ModelBDispatch` | next state | 4000 |
| C80 | `Shisu_ModelBTurn` | turn 0x40 | 3990 |
| C81 | `Shisu_ModelBDraw` | clamp 0x13 | 562 |
| C82 | `Shisu_ModelBDraw` | blue bound | **0 - equivalent: at blue = 3 g2 the lowered byte is 0 either way; near variant C104 refused** |
| C83 | `Shisu_ModelBDraw` | green restored from red | 3443 |
| C84 | `Shisu_ModelBDraw` | scale index - 1 | 4000 |
| C85 | `Shisu_DrawModel` | mode 3 | 1094 |
| C86 | `Shisu_DrawModel` | winding bound | 1736 |
| C87 | `Shisu_DrawModel` | y offset from x | 3474 |
| C88 | `Shisu_DrawModel` | green from red | 2176 |
| C89 | `Shisu_DrawModel` | CLUT row | 3474 |
| C90 | `Shisu_DrawModel` | scale test | 4000 |
| C91 | `Shisu_DrawModel` | Sprite_Current not put back | 3357 |
| C92 | `Shisu_DrawModel` | quad stride | 2363 |
| C93 | `Shisu_ModelBSquare` | mask | 1963 |
| C94 | `Shisu_ModelBSquare` | whole-turn test | 813 |
| C95 | `Shisu_ModelBDrop` | step 0x100000 | 3352 |
| C96 | `Shisu_ModelBDrop` | ground bound | 446 |
| C97 | `Shisu_ModelBStill` | no draw | 4000 |
| C98 | `Shisu_ModelADispatch` | next state | 4000 |
| C99 | `Shisu_ModelATurn` | turn the other way | 4000 |
| C100 | `Shisu_ModelBDrop` | x, z swapped | 4000 |
| C101 | `Shisu_DrawModel` | winding read as a dword | 2350 |
| C102 | `Shisu_Score` | ramp edge + 1 (C72 near variant) | 268 |
| C103 | `Shisu_Score` | divisor 99 (C75 near variant) | 727 |
| C104 | `Shisu_ModelBDraw` | blue bound + 1 (C82 near variant) | 692 |
| C105 | `Shisu_ModelBDraw` | red lift + 1 | 2141 |
| C106 | `Shisu_ModelBDraw` | scale + 1 during the draw | 4000 |

## 7. Latent defects and ranges (Capcom's, described, not fixed)

- **L1 - DIV-0073 (entered 2026-10-04; the owner kept it the same day):
  `Shisu_DrawModel` loads 26 bytes of stale stack into `Gte_Matrix2`.** Its
  light matrix is a local (`[esp + 0x7C]`) of which `Light_ObjectDirection`
  writes the first three shorts; `Gte_SetMatrix2` then copies all 32 bytes
  into `Gte_Matrix2` (`.data`). Rows 1 and 2 and the translation are
  whatever the stack held. `Gte_Matrix2` is read only by `Gte_NormalColor`,
  whose result the shipped code throws away (the colour in is copied over
  it, [`psx_gte_transform.cpp`](../src/game/psx_gte_transform.cpp)), so
  nothing drawn or decided depends on it - but the bytes land in `.data`,
  where the state hash ([`state-hash.md`](state-hash.md)) compares them.
  **Ours zeroes the 26 bytes** (the light row is the function's own); the
  fuzz compares the first row only. A state-hash difference at `Gte_Matrix2`
  after this screen draws is this. Sprite_AddDrawRecords, the field's
  sibling, hands the global `Light_Matrix` and has no such read.
- **L2 - stale bytes the callee masks**: `Menu_DrawOutlineNotched` builds
  colour and abr dwords from bytes stored into otherwise unwritten locals,
  and coordinates from dwords spanning two notch words (one of them past the
  four words' array); `Menu_DrawLine` reads only their low byte / low word /
  low two bits, so the lines are exact. Ours passes clean values.
- **Unbounded dispatchers** (seven): each `jmp` / `call [table + byte * 4]`
  is unchecked; ours aborts with a message past the counts of section 1.
  Every writer stores inside them, so play reaches none; a stray byte would
  jump through the next table (or, past `MasterFigure_States`, R2C's).
- **`Effect_Spawn` / `_SpawnAt`** write record `slot` for any answer but
  `0xFF`; `Effect_FindFree` answers 0..19 or `0xFF`, so ours aborts past 19
  (never reached).
- **`Shisu_DrawModel`'s count** is the header's s8 widened to a u16: a
  negative count draws some 65,000 quads past the packet pool, with no room
  check (`Sprite_AddDrawRecords` has one). Ours does the same.
- **`Shisu_PickCounts`' row**: a row past 3 (`0xFF` after backing out,
  then the tail writes `0xFF` into the windows' cursor bytes) is skipped by
  the original's `ja`; ours the same.
- **`Shisu_ModelBDraw`'s colour tint** uses 8-bit sums (`imul` of a byte):
  with `g2` past 85 the lift `3 g2` wraps; the saturation tests use the full
  value, so the byte stored can be below the colour it lifted. `g2` is at
  most 20 in play.
- **Division**: `Shisu_Score`'s three `idiv`s are guarded against 0 divisors
  and never overflow (divisors positive); ours divides the same way.

## 8. Calls across groups, inbound

Raw calls out (`rest_2b_callees.h`, the round's rebinding turns them into
names): `0x57F340` (R2C, wave two: model A's draw - `Shisu_ModelATurn`'s
tail jmp; `MasterFigure_States[2..5]` are R2C's too, read in place) and
`0x4941B0` (R3G, wave three: the winding test, from `Shisu_DrawModel`).

Inbound from outside the group (for the rebinding pass):

| Caller | Owner | Calls | How |
|---|---|---|---|
| `Mode8_Step8` `0x517340` | FC3 (ours) | `Shisu_ModeDispatch` | `SH_AT(at::kMenuDispatchB)`, rebound to the name (section 10) |
| `0x57F340` | R2C (wave two) | `Shisu_DrawModel` | E8 `0x57F3EF` |
| `0x458D70` | R4B (wave four) | `Menu_DrawOutlineNotched` x 4 | E8 |
| `0x45FA80`, `0x45FE90`, `0x4601D0`, `0x460270` | R4E (wave four) | `Menu_DrawOutlineNotched` x 7 | E8 |
| `0x45D730`, `0x45E2C0` | R4D (wave four) | `Menu_DrawGreyHLine` | E8 |
| 64 functions of the area blocks, `move_groups.cpp`, `Scena13_Run4` | ours | `Effect_Spawn`, `Effect_SpawnAt` | by name (`AH_CALL` / `SH_CALL` / the movement groups' table) |
| `0x578A40` (a case of `MoveScript_Group9`, ours) | ours | `Effect_Spawn` | inside `MoveScript_Group9` |
| `Field_ObjectTriggerByKind` | FE2 (ours) | `Field_TriggerCounterF0` | `Field_ObjectTriggers[1]`, read in place |

## 9. The live route

The catalogue's reach columns are empty for all 39 rows, and no file under
`analysis/calltrace` (the attract traces, `all_ab.callcounts.tsv`,
`trace_a`) names any of the 38. The camp route
(`tools/recipes/campingFishing.txt`) enters other groups' rows of this wave
(HANDOFF item 0000), none of these. `Effect_Spawn` is reached by the area
handlers in play, but no recorded trace shows it. **Fuzz only.** The
coordinator's state hash after the merge covers whatever its routes enter;
L1 is the one `.data` difference to expect, and only once the screen draws.

## 10. The rebinding

`band_rows.py --refs --group R2B` and `grep -rn -i` of the 38 over
`src/game`: 66 raw references to four functions, and twelve rows that name
`Effect_Spawn` / `Effect_SpawnAt` as Capcom's (the `THEIRS` macros and two
written out), which `Register` refuses once the names are ours.

| File | Change |
|---|---|
| `field_c3_callees.h` | `kMenuDispatchB = bof3::addr::Shisu_ModeDispatch` (value unchanged) |
| `scenario_harness.cpp` | `kStandard`'s `SH_THEIRS(Effect_SpawnAt)` -> `SH_OURS(Effect_SpawnAt)` - **a harness row, edited** (below) |
| `area_harness.cpp` | `kStandard`'s `AH_THEIRS(Effect_Spawn)` -> `AH_OURS(Effect_Spawn)` - **a harness row, edited** |
| `area_w1c`, `w1d`, `w1f`, `w2a`, `w2c`, `w3c`, `w3d`, `w3e` `_fuzz.cpp` | each group's `Wxx_THEIRS(Effect_Spawn)` row -> `Wxx_OURS(Effect_Spawn)` |
| `area_011_fuzz.cpp`, `area_w3a_fuzz.cpp` | the row `{"Effect_Spawn", KeyOf(Effect_Spawn), KeyOf(Effect_Spawn), ...}` -> `{"Effect_Spawn", ::bof3::addr::Effect_Spawn, KeyOf(Effect_Spawn), ...}` (hand-agnostic; the first `'*'` run stopped at it) |
| `area_w1e_fuzz.cpp` | its two rows `{"Effect_Spawn", 0x57CE10, 0x57CE10, ...}`, `{"Effect_SpawnAt", 0x57CE80, 0x57CE80, ...}` -> `bof3::addr::...`, `KeyOf(&::...)` (area 67's handlers call both by name; the area harness's standard set has no `Effect_SpawnAt` row to resolve the named key through, and the second `'*'` run stopped there) |

**Why two harness rows moved.** `SH_THEIRS(name)` / `AH_THEIRS(name)` key a
row on the name's pointer and require it to lie in `.text`; once the name is
ours that pointer is our DLL's, and `Register` stops every group of the
harness with "callee ... is not Capcom's code" - the `'*'` run cannot pass
with the two functions bound and the rows unchanged. The round-ten form
([`round-10-cleanup.md`](round-10-cleanup.md) section 1: the column moves
"in the same commit" as the take) is the only change that keeps both; each
is one token, the row's masks and answers untouched. `FIELD_THEIRS(Effect_Spawn,
0x57CE10)` is hand-agnostic and stays.

**Left raw, on purpose**: the fuzz files' `CallSite` tables naming `0x57CE10`
/ `0x57CE80` (`area_011`, `area_w0b`, `w1c`..`w3e`, `scena_sc13`; the
disassembly's targets), `area_w0b_fuzz.cpp`'s `{"Effect_Spawn", 0x57CE10,
0x57CE10, ...}` row (keyed on the address; `StandIn` resolves the named key
through the area harness's standard row, now `AH_OURS`),
`move_groups.cpp`'s `case 0x57CE10` / `0x57CE80` and its `kCalls9` (the
movement groups' call-site keys), `scenario_harness.cpp`'s field row for
`0x57DFF0` (keyed on the address; a harness), comments in `scena_sc7_fuzz.cpp`
and `area_*`. No file of another group of this round refers to the 38.

**Harness rows that list functions of this group**: `scenario_harness.cpp`'s
`SH_THEIRS(Effect_SpawnAt)` (moved, above; its masks `{kU8, kU8, kU8, kAll,
kAll, kAll}` and answer `0xFF..0x13` match the read), `FIELD_THEIRS(Effect_Spawn,
0x57CE10)` (`{kU8, kU8, kU8, kU16, kU16}`, `kFlag` - matches; the answer
could be `kByte 0xFF..0x13` like `Effect_SpawnAt`'s, stricter, not wrong),
and the field row `0x57DFF0` (no arguments, `kGarbage`: matches - it answers
nothing). `area_harness.cpp`'s `AH_THEIRS(Effect_Spawn)` (moved; matches).

## 11. For `analysis/calltrace/entries_logic.txt`

28 lines appended to the main checkout's file (2026-10-04): the 28 starts it
lacked or listed with a longer host extent; the other ten were already
listed with the extents read here. Left for the round's end (a host's
longer extent over functions of this group): `0057DFF0 9AA` (over the 22
hidden starts `0x57E010..0x57E8C0`), `0057EDD0 115` (over `0x57EDE0`,
`0x57EDF0`), `0057EEF0 44C` (over `0x57F270..0x57F330`), `0056E020 2A` (over
`0x56E040`).
