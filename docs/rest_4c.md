# Group R4C: the community band's two games with a stake

**Status:** MEASURED (2026-10-05) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave four, from the
round branch's tip `35ec19e`. **60 functions ours** (`src/game/rest_4c.cpp`,
declarations in `symbols.gen.h` from their `symbols.toml` signatures, the cells,
the image tables read in place and the callees nobody owns in
`src/game/rest_4c_callees.h`, shadow name `rest_4c`): the cut's 60 rows for R4C
(`analysis/round14_cut.tsv`), none added, none dropped. Each read to its last
instruction with capstone and fuzzed through the scenario harness's field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
360,000 rounds, 0 mismatches. **146 controls planted** one at a time: 144 refused by a count, the other two equivalent mutants whose near variants are refused (section 6). **Fuzz only**: no recorded route
enters the faerie village (section 9). No divergence; no full-frame fill.

The band is two of the four games R4B's game table `0x652A84` holds (the field
hook's state 2: R4B's `0x652A70[2]`, `0x457340`, jumps through `0x652A84` by
the byte `0x9039F5`), and the draw helpers both share:

| Entry | What the code does | Functions |
|---|---|--:|
| `0x652A84[0]` `CommuHiLo_Run` `0x459F20` | **The first game.** A stake of 1..100 zenny entered digit by digit (three digits, held to Party_Zenny); nine values, a shuffle of 1..9, dealt as cards (one in four deals shows one to three of values 1..8 at random); up to eight picks, each 0 or 1; then each card t = 1..8 turned in order and its pick compared with whether value t - 1 is not below value t; a wrong one loses the stake, all right pays the stake times the u16 factor `0x652CF2[t]` (t the last card turned) over 100; winnings of 10000 or more end the game, below it the next deal plays the winnings | 37 |
| `0x652A84[1]` `CommuHitBlow_Run` `0x45B770` | **The second game.** 500 zenny taken (unchecked); three distinct digits 1..9 drawn; up to eight guesses of three digits, each scored as digits in place (+3 of its record) and digits at another place (+4); three in place gives the item of `0x652D94[3 r + rank]` (r the guess, rank 0..2 by Rand), the eighth miss shows the secret | 21 |
| (both) | `Commu_*`: the money box and the stake box, their frame of menu pieces, a card, a piece, an underline, a random digit 1..n; `Commu_PushSubscreen`, which R4B's screens call | 2 of the above, and the helpers |

What either game is in play, what the messages say and what the faerie village
shows are not stated here: the descriptions are what the code draws, tests and
writes (the names `HiLo` and `HitBlow` say what the code compares, nothing
more). Every message id is opened with `Msg_OpenScript`; the choice bytes
`0x939A3E..0x939A41` the states run on are the ones a message's choice writes
(area_w4c's `kChoiceByte3E`), so some transitions are the scripts' (section 7).

**PSX twins**: 56 of the 60 rows have one in a COMMU overlay
(`analysis/pairs_propagated.json`, one overlay section); the sibling names none
of them, so every name here is a hypothesis from the code (`symbols.toml`
status `evidence` for what was read; the twin's address is in each evidence
string). The four without: `0x459EE0`, `0x459F20`, `0x45B400`, `0x45B770`.

## 1. What each function does

The cells (`rest_4c_callees.h`): the phase `0x939A3E`, the step `0x939A3F`, the
state `0x939A40`, the again choice `0x939A41`; the block `0x675F78..0x675FC2`
- the stake / winnings `0x675F88` (s32), the count `0x675F8C` (s8: the stake's
digit cursor, then the picks; the second game's guess row), the cursor
`0x675F8D` (s8: the pick cursor, the shown-value bits, the prize rank), the turn
`0x675F8E`, the lost flag `0x675F8F`, the kept music `0x675F90`, the counter
`0x675F95` (slides and waits), the nine values `0x675F98`, the picks
`0x675FA1 + 1..8`, the guess records `0x675F9B + 5 r` (three digits, hits,
blows); the stake's digits `0x903850..0x903852` (ones, tens, hundreds). Every
`symbols.toml` evidence string has the function's whole reading; the source
comments have it again beside the code.

### 1.1 The helpers

| PC | Name | What |
|---|---|---|
| `0x459EE0` | `Commu_PushSubscreen` | `0x675F81` = `0x939A40`, word `0x675F78` = word `0x675F7A`; Sound `0x103`; `0x675F7A` = `0x3D`, `0x675F7C` = 0, `0x939A40` = 5. Called by R4B's `0x457DD0`, `0x457FE0`, `0x458240` |
| `0x45B0D0` | `Commu_RandDigit(top)` | `Rand & 0xF` until 1..top (low byte); al, Rand's upper bytes above |
| `0x45B2C0` | `Commu_DrawCard(x, y, d)` | draw mode `0x1E`; a 0x20 x 0x28 SPRT: `d` 0xFF the back, else u `(d & 7) << 5`, v `(d >> 3) * 0x28` (a byte) |
| `0x45B3A0` | `Commu_DrawUnderline(x, y)` | a LINE_F2 (x, y)..(x + 0xC, y), shade 0x80 |
| `0x45B400` | `Commu_DrawPiece(x, y, i)` | a SPRT whose u, v, w, h are `0x652D04[i]` (18 entries), word `0x7887` |
| `0x45B520` | `Commu_DrawFrame(x, y)` | draw mode `0x1D`; pieces 0..8 around a 0x70 x 0x2B box |
| `0x45B490` | `Commu_DrawZennyBox(x, y)` | `Menu_DrawBox(x + 1, y + 1, 0x6A, 0x28, 0x80, style)`, the frame, title `0x669F60`, Party_Zenny (`sprintf` `0x64ADDC`, 12-dot font), unit `0x66A31C` |
| `0x45B5F0` | `Commu_DrawStakeBox(x, y, n)` | the same with title `0x669F68` and the stake; `n` 2 / 3 / 4 right-aligns a stake below 10 / 100 / 1000 (formats `0x65306C`, `0x653074`, `0x65307C`) |
| `0x45AF60` | `CommuHiLo_DrawMarker(x, y)` | a POLY_F3 (`0x5A7570`) pointing down at (x, y), green blinking by `Frame_Counter` |
| `0x45AFF0` | `CommuHiLo_DrawRow(n, m, k)` | n cards at `0x10 + 32 i`: raised (0x64) / lowered (0x74) by pick i for 0 < i <= k, else 0x6C; value i below m, backs past it |
| `0x45B080` | `CommuHiLo_Shuffle` | `0x675F98..0x675FA0` a shuffle of 1..9 by `Commu_RandDigit(9)` |
| `0x45B1E0` | `CommuHiLo_DrawChoice(x, y, label, mode)` | a 0x2A x 0x14 box; mode 0 / 1 / 2: piece 0xA / 9 / 0xA and the label `0x669F54[label]` in colour 0 / 0 / 7 |
| `0x45B100` | `CommuHiLo_DrawChoices(x, y, sel)` | the three at x, x + 0x30, x + 0x60; their modes by sel (0: 1 2 0, 1: 2 1 0, 2: 0 0 2, 3: 2 2 0, 0xFF: 0 0 0, other: none) |

### 1.2 The first game (`CommuHiLo_*`)

| PC | Name | Table entry | What |
|---|---|---|---|
| `0x459F20` | `_Run` | `0x652A84[0]` | by the phase through `CommuHiLo_Phases` |
| `0x459F30` | `_OpenDispatch` | Phases[0] | by the state through `CommuHiLo_OpenStates` |
| `0x459F40` | `_OpenFade` | OpenStates[0] | the values 0xFF, Music_Track kept, `Music_FadeOutStop(10)`, state + 1 |
| `0x459F80` | `_OpenMusic` | OpenStates[1] | once `File_LoadDone`: `Music_Play(0x94, 8)`, state 0, phase + 1 |
| `0x459FB0` | `_PlayDispatch` | Phases[1] | by the state through `CommuHiLo_PlayStates` |
| `0x459FC0` | `_Intro` | PlayStates[0] | the two boxes slide in; message `0x71`; stake and count 0 |
| `0x45A070` | `_WaitIntro` | [1] | the boxes; state + 1 once no message is up |
| `0x45A0A0` | `_BetDispatch` | [2] | by the step through `CommuHiLo_BetSteps` |
| `0x45A0B0` | `_BetInput` | BetSteps[0] | the stake's digits from its low byte; the pad moves the digit cursor (0..2) and the digit (0..9); the stake held to 0..100 and Party_Zenny; confirm takes it from Party_Zenny (step + 1), a 0 stake or cancel backs out (step + 2) |
| `0x45A300` | `_BetSlide` | BetSteps[1] | the boxes slide to the top; state + 1 |
| `0x45A380` | `_BetBack` | BetSteps[2] | the boxes slide out; at 4 a tail jmp to `_Quit` |
| `0x45B370` | `_Quit` | (`_BetBack`'s jmp) | Sound `0x106`, message `0x72`, phase + 1 (the close), state 0 |
| `0x45A3E0` | `_DealDispatch` | [3] | by the step through `CommuHiLo_DealSteps` |
| `0x45A3F0` | `_DealStart` | DealSteps[0] | picks 0, the shuffle, counter 3, count 0 |
| `0x45A460` | `_DealCards` | DealSteps[1] | nine backs dealt one by one; at 9 the shown bits (one Rand in four: one to three bits) |
| `0x45A590` | `_DealHints` | DealSteps[2] | the shown values; message `0x73` |
| `0x45A610` | `_DealWait` | DealSteps[3] | the same until no message is up |
| `0x45A6A0` | `_DealChoices` | DealSteps[4] | the three choices slide in; count and cursor 0; state + 1 |
| `0x45A770` | `_Pick` | [4] | the pick cursor and the up to eight picks (section 1, the evidence) |
| `0x45A980` | `_PickClose` | [5] | the rest of the backs and the choices slide down; the turn 1 |
| `0x45AA40` | `_Reveal` | [6] | every 0x1E frames card t judged (pick t against value t - 1 >= value t, unsigned); a wrong one: Sound `0x107`, lost |
| `0x45AB30` | `_RevealPause` | [7] | 0x3C frames, then state + 1 |
| `0x45AB90` | `_Payout` | [8] | the values rise; lost: stake 0, message `0x74`; else stake = (factor[turn] * stake + 50) / 100, message `0x76` |
| `0x45AC70` | `_CheckWinnings` | [9] | 10000 or more: message `0x79`, state 10; else message `0x78`, state 3 (a new deal on the winnings) |
| `0x45ACE0` | `_CashOut` | [10] | `Zenny_Add(winnings, 0)`, message `0x75`, winnings 0; the state is not moved here |
| `0x45AD30` | `_Restart` | [11] | the boxes slide back; message `0x71`, state 1 (a new stake) |
| `0x45AE30` | `_Leave` | [12] | the boxes slide up; phase 2 |
| `0x45AF00` | `_Close` | Phases[2] | the music faded, then the kept track played; the field hook's tail state `0x9039F4` + 1 |

### 1.3 The second game (`CommuHitBlow_*`)

| PC | Name | Table entry | What |
|---|---|---|---|
| `0x45B770` | `_Run` | `0x652A84[1]` | by the phase through `CommuHitBlow_Phases` |
| `0x45B780` | `_OpenDispatch` | Phases[0] | by the state through `CommuHitBlow_OpenStates` |
| `0x45B790` | `_OpenFade` | OpenStates[0] | Music_Track kept, fade, state + 1 |
| `0x45B7C0` | `_OpenMusic` | [1] | `Music_Play(0x94, 8)`, message `0x7F` |
| `0x45B800` | `_OpenWait` | [2] | Sound `0x102`, counter 4, stake 0 |
| `0x45B840` | `_OpenSlide` | [3] | R4D's guess panel, the boxes and a back slide in; phase + 1 |
| `0x45B8F0` | `_PlayDispatch` | Phases[1] | by the state through `CommuHitBlow_PlayStates` |
| `0x45B900` | `_StartDispatch` | PlayStates[0] | by the step through `CommuHitBlow_StartSteps` |
| `0x45B910` | `_Start` | StartSteps[0] | Party_Zenny - 500 (unchecked), stake 500; three distinct digits; records 1; row 0 |
| `0x45B9D0` | `_StartSlide` | StartSteps[1] | three cards 1 rise, three backs slide |
| `0x45BAF0` | `_Prompt` | [1] | message `0x80` |
| `0x45BB60` | `_WaitPrompt` | [2] | cursor 0 once no message is up |
| `0x45BBD0` | `_Input` | [3] | the digit at the cursor down (below 1 -> 9) / up (above 9 -> 1), the cursor over 0..2; confirm: state + 1 |
| `0x45BD20` | `_Score` | [4] | hits and blows into the record; not three: both into the message's text (`sprintf` of `Area08_MessageFormat` into `Text_Records` and `0x904D00`), message `0x81`; three: the row + 1, the rank (Rand), the prize's name into `0x904D00`, message `0x82`, state 7 |
| `0x45BEE0` | `_NextGuess` | [5] | the next row (the last guess copied in), state 1; after the eighth, message `0x85` |
| `0x45BFA0` | `_Lost` | [6] | message `0x86`, state 8; the secret shown |
| `0x45C020` | `_Prize` | [7] | `Inventory_Add(category, item, 1)`; al 0: message `0x83`; state 6 |
| `0x45C0C0` | `_End` | [8] | step 0: wait; step 1: the last guess and the secret rise, then by `0x939A41` a new game (state 0) or step 2; step 2: everything slides out, phase 2 |
| `0x45C3A0` | `_Close` | Phases[2] | as `CommuHiLo_Close` |

## 2. Divergence

None: each function is a faithful replacement. `widescreen.cpp`, `cheats.cpp`,
`labels.cpp` and `DIVERGENCE.md` name no address in the band (a grep); no
full-frame fill is in it (every float the band writes is a small coordinate
converted from an integer; no `320.0` / `240.0` operand). No byte the band
reads that it never wrote reaches a draw or a decision (nothing for a ledger
entry).

## 3. The tables

**The state tables** (`symbols.toml` `[[data]]`): two runs of contiguous
tables, `0x652C8C..0x652CF3` and `0x652D4C..0x652D93`, each split by its
readers; every count is its reader's reach, checked by hand against what the
entries write and against where the next reader's table starts (`band_rows.py`
read the runs as 26, 23, 21, 8, 5 and 18, 15, 11, 2 code entries - the runs to
their ends, not the tables):

| Table | Count | Read by | Ends at |
|---|--:|---|---|
| `CommuHiLo_Phases` `0x652C8C` | 3 | `CommuHiLo_Run` by `0x939A3E` | `CommuHiLo_OpenStates` |
| `CommuHiLo_OpenStates` `0x652C98` | 2 | `_OpenDispatch` by `0x939A40` | `CommuHiLo_PlayStates` |
| `CommuHiLo_PlayStates` `0x652CA0` | 13 | `_PlayDispatch` by `0x939A40` | `CommuHiLo_BetSteps` |
| `CommuHiLo_BetSteps` `0x652CD4` | 3 | `_BetDispatch` by `0x939A3F` | `CommuHiLo_DealSteps` |
| `CommuHiLo_DealSteps` `0x652CE0` | 5 | `_DealDispatch` by `0x939A3F` | the payout factors (data) |
| `CommuHitBlow_Phases` `0x652D4C` | 3 | `CommuHitBlow_Run` by `0x939A3E` | `CommuHitBlow_OpenStates` |
| `CommuHitBlow_OpenStates` `0x652D58` | 4 | `_OpenDispatch` by `0x939A40` | `CommuHitBlow_PlayStates` |
| `CommuHitBlow_PlayStates` `0x652D68` | 9 | `_PlayDispatch` by `0x939A40` | `CommuHitBlow_StartSteps` |
| `CommuHitBlow_StartSteps` `0x652D8C` | 2 | `_StartDispatch` by `0x939A3F` | the prize table (data) |

**R4B's tables, not named here**: `0x652A70` (the field hook's five states by
`0x9039F4`, read by R4B's `0x4572F0`) and `0x652A84` (the four games by
`0x9039F5`, read by R4B's `0x457340`; entries 0 and 1 are this group's two
`_Run`s, 2 and 3 R4D's `0x45C8C0` and `0x45D040`). The `band_rows.py` line
"reached: .data cell 0x652a84 run 0x652a70[5]" is the run, not the table.

**The data read in place** (raw in `rest_4c_callees.h`, their bytes not copied
here): the payout factors, u16 at `0x652CF2 + 2 t` for t 1..8 (`0x652CF4..
0x652D03`; t 0 would read the high half of `CommuHiLo_DealSteps[4]`); the menu
pieces `0x652D04` (18 x 4 bytes, to `CommuHitBlow_Phases`; this group draws
0..0xA, R4D's `0x45C700` 0xB..0x11); the prizes `0x652D94` (24 pairs, item then
category, to code pointers at `0x652DC4`); the three choice labels `0x669F54`
(pointers); the titles, unit and formats named in the callees header.

## 4. The fuzz (`rest_4c_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=rest_4c`, field mode, **6,000 rounds a function**
(`BOF3X_R4C_ONLY=<name>` runs the clones whose name holds it). Shapes: the 47
states and dispatchers `kState`, the 13 helpers `kCall` (`Commu_RandDigit`
`ret_mask 0xFF`: its callers read al). The nine tables are `DataTable`s,
swapped for recorders on both sides. **Regions** beyond field mode's: the
choice bytes `0x939A30` + 0x20, the block `0x675F70` + 0x60, the tail state
`0x9039F0` + 8, `Text_Records` + 0x40 (to `0x904D20`). The stake's digits,
Party_Zenny, Music_Track, the style byte, `Frame_Counter`, the pad and the
confirm / cancel words are in the standard regions.

**Callees** (the group's listing first): the group's own helpers called by
`E8` / `E9` as recorders with the masks their callers' pushes need -
coordinates `0xFFFF` (every reader takes them as `movsx word` or hands them to
a callee that reads 16 bits; the callers push whole registers whose upper half
is whatever an earlier answer left), digits, pieces, labels, modes and counts
`0xFF`; `Commu_RandDigit` a `kByte` 1..9; R4D's `0x45C400`, `0x45C7D0`,
`0x45C850` by address with the same masks (their third and fourth arguments
are read as bytes, `0x45C447`, `0x45C7D0`, `0x45C7FB`, `0x45C850`); Capcom's
`0x5A7570` by address (it fills the primitive); re-listed: `Input_AutoRepeat`
(answers that take each branch, the pad's word a third of the time),
`Gpu_SetSprt` (its 0x1C bytes filled after the call), `Item_NamePtr` with
masks `0xFF, 0xFF` (its evidence: each argument's low byte; the caller pushes
eax / ecx over leftovers) answering into the text buffer. The rest are the
standard and field-standard rows. **Louder stand-ins**: every draw the states
call (the group's boxes, cards, rows, choices and R4D's three) moves one of the
group's cells one call in four (the disturbance below, from `Noise()`), so a
re-read missed after a draw is seen; so does `Sound_PlayEffect`'s (re-listed,
`0xFFFF`), since the stake and the money are read after sounds;
`CommuHiLo_Shuffle`'s recorder writes nine values.

**Seeds** (per function, after the harness's fill): the phase, state and step
at their compares (0..12), each dispatcher's byte below its table; the counter
at 0..6, 0x1E, 0x3C, 0xFF; the count inside the function's room (section 7:
0..2 for `_BetInput`, 0..7 for the second game but 0..8 for its `_Lost` and
`_End`, 0..8 for `_Pick` and `_Payout`, 0..9 otherwise) and its ends; half the
time `_DealCards` at the ninth card (count 8, counter 1) and `_End` at step 1
with the counter 3; the cursor 0..2 for the second game,
0..3, 0x80, 0xFF otherwise; the turn 1..8, half the time at the picks' end; the
lost flag, the again choice 0 / 1 / any; the stake at 0, 1, 9..11, 99..101,
127, 128, 999..1001, 9999..10001, -1, INT_MIN; Party_Zenny at 0, 9, 50, 99..101,
499..501, 0x8000, 0xFFFF, 0x10050; the digits 0..10, 0x80, 0xFF; the values and
records 1..9 two times in three; the picks 0 / 1; `_Score`'s guess the secret
or a rotation of it half the time (three hits, or three blows); `_Reveal`'s
pick t right half the time; `Field_Request` 2 or not; the pad, confirm and
cancel words. **Arguments** of the kCall helpers: a card 0xFF, 0..9, 0x3F or
any; a piece 0..17; a label 0..2 and a mode 0..3 or 0xFF; the selection 0..4,
0xFF or any; a row's count 0..12, shown 0..9, picks 0..8; a top 1..15.
**Disturbance** (`sh::DisturbCase(h, 11)`, every value from `h`): the counter,
the count (inside the function's room), the cursor, the turn (1..8), one of the
phase / step / state, the stake, Party_Zenny, a byte of `0x675F98..0x675FC2`,
a digit (-1..10), the lost flag or the again choice, the tail state.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_4c`,
exit 0): 360,000 rounds over 60 functions, 1,601,285 calls to the stand-ins,
**0 mismatches** (0 on the first run too, before the fuzz was made louder); 23,796 bytes of state in 42 regions; 305
stand-ins. Every entry of the nine tables reached (each handler recorder 414..
3,036 calls); `CommuHiLo_Quit` 471, `Item_NamePtr` 1,037, `Zenny_Add` 4,008,
`Inventory_Add` 4,003, `Rand` 28,795, `Commu_RandDigit` 173,045.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0, no
`MISMATCH` line but `0 MISMATCHES`, `inject: 9741 ours, 0 left original`;
`rest_4c` there 360,000 rounds, 1,599,797 calls, 0 mismatches. **With
`BOF3X_WIDE=1`**: `'*'` exit 0, the same counts, no mismatch. Neither run died
silently. `ledger_check`: 73 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 60 at 9,087 bytes against the cut's
  9,386 - 43 differ by padding only, none by code; each checked by hand to its
  `ret` or tail `jmp`. No code of the band is in no list.
- **Hidden starts**: 47, every one an entry by address - a cell of one of the
  nine tables or of R4B's `0x652A84` - except `0x45B370`, which is
  `CommuHiLo_BetBack`'s tail-jmp target (its own frame-free code with its own
  `ret`, after `Commu_DrawCard`'s `ret`): taken as a function, not a shared
  tail (one jmp reaches it). No start is a jump-table case; no `jmp [reg * 4 +
  table]` inside a function (the dispatchers are the functions).
- **Their hosts**: `0x459EE0`'s catalog extent `0x1074` and `0x45B5F0`'s
  `0xE04` run on over the states after their `ret`s (`0x459F1C`, `0x45B766`);
  `0x45B2C0`'s `0xDF` takes in `0x45B370`. None contains another's code as a
  fall-through.
- **The cut's columns**: the unit "COMMU" is right for all 60 (they are the
  overlays' twins); the label "Communication minigames" is the catalog's.
  `0x459EE0` (neighbour tier) and `0x45B400`, `0x45B770` (no twin) were read
  like the rest.
- **The harness's rows**: none of the 60 addresses has a row in
  `scenario_harness.cpp` or `boss_harness.cpp` (a grep).
- **Not refused callers' leftovers**: the callers push byte arguments as whole
  registers (`push eax` after `mov al, ...`) and coordinates computed on
  `movzx ax` with the upper half of eax left from an earlier answer; every
  recorder listed above masks what its callee reads. The R1E caution (a
  `(pointer & 0xFFFFFF00) | byte` emitted as the byte alone) was not met.

## 6. Controls

Planted one at a time by a scratch script (`controls.py` in the session's
`r4c/` scratch directory): each plant anchored on a unique string of
`rest_4c.cpp` (a two-part plant adds the earlier read it then uses), rebuilt,
run under `BOF3X_R4C_ONLY=<filter>`, the file restored and rebuilt at the end;
the committed file has no switch. The harness stops at the first differing
round, so the column is the round that refused it (0-based) in this worktree;
every refused run exited 3 on a `MISMATCH` line. **146 planted, 144 refused.**
Not refused: **62** and **63**, equivalent mutants - 62 caps the stake at 99
where the original caps above 100, and at exactly 100 the stake already stored
is 100; 63 drops the low word's sign extension of Party_Zenny, which is below
100 wherever it is taken (section 7). Their near variants **148** (the cap
stores 99) and **64** (Party_Zenny + 1) are refused. Controls 43..60 are one
per case of the group's disturbance, each missing a re-read after a call (the
case named in the column): all eleven cases are proved live.

**The first pass** (the fuzz as first committed) left five not refused that
are refused now, each the fuzz's: 52 (the stake re-read after the confirm
sound) and 59 (the again choice re-read after the end's draws) - made louder by
`Sound_PlayEffect`'s stand-in moving a cell one call in four and the end seeded
at step 1 with the counter at 3 half the time; 75 (the ninth card's hint
count) - the ninth card seeded half the time; 130 and 134 (a row of 8 at the
second game's loss and end) - the count's room was 0..7 for every second-game
function, now 0..8 for those two, which index no record by it. Control 35's
first plant made ours' shuffle loop for ever (the harness's run hung; the game
process this worktree's launcher started was killed by its pid); it was
replaced by the plant below, refused.

| # | Run (`_ONLY`) | Plant (ours changed to) | Refused at |
|--:|---|---|---|
| 1 | `CommuHiLo_Run` | `(0x459F20)", CommuHitBlow_Phases,` for `(0x459F20)", CommuHiLo_Phases,` | round 0 |
| 2 | `CommuHiLo_OpenDispatch` | `(0x459F30)", CommuHitBlow_StartSteps,` for `(0x459F30)", CommuHiLo_OpenStates,` | round 0 |
| 3 | `CommuHiLo_PlayDispatch` | `(0x459FB0)", CommuHiLo_PlayStates + 1,` for `(0x459FB0)", CommuHiLo_PlayStates,` | round 0 |
| 4 | `CommuHiLo_BetDispatch` | `(0x45A0A0)", CommuHiLo_Phases,` for `(0x45A0A0)", CommuHiLo_BetSteps,` | round 0 |
| 5 | `CommuHiLo_DealDispatch` | `(0x45A3E0)", CommuHitBlow_PlayStates,` for `(0x45A3E0)", CommuHiLo_DealSteps,` | round 0 |
| 6 | `CommuHitBlow_Run` | `(0x45B770)", CommuHiLo_BetSteps,` for `(0x45B770)", CommuHitBlow_Phases,` | round 0 |
| 7 | `CommuHitBlow_OpenDispatch` | `(0x45B780)", CommuHitBlow_OpenStates + 1,` for `(0x45B780)", CommuHitBlow_OpenStates,` | round 0 |
| 8 | `CommuHitBlow_PlayDispatch` | `(0x45B8F0)", CommuHitBlow_PlayStates + 1,` for `(0x45B8F0)", CommuHitBlow_PlayStates,` | round 0 |
| 9 | `CommuHitBlow_StartDispatch` | `(0x45B900)", CommuHiLo_OpenStates,` for `(0x45B900)", CommuHitBlow_StartSteps,` | round 0 |
| 10 | `Commu_PushSubscreen` | `B(at::kState) = 4;` for `B(at::kState) = 5;` | round 0 |
| 11 | `Commu_PushSubscreen` | `SetWord(At(at::kSub), 0x3E);` for `SetWord(At(at::kSub), 0x3D);` | round 0 |
| 12 | `Commu_RandDigit` | `static_cast<int>(v) <= limit` for `static_cast<int>(v) < limit` | round 4 |
| 13 | `Commu_RandDigit` | `if (static_cast` for `if (v != 0 && static_cast` | round 4 |
| 14 | `Commu_DrawCard` | `PutWord(p, 0x16, 0x7BC8);` for `PutWord(p, 0x16, 0x7BC9);` | round 0 |
| 15 | `Commu_DrawCard` | `((d >> 3) * 0x29)` for `((d >> 3) * 0x28)` | round 0 |
| 16 | `Commu_DrawCard` | `PutWord(p, 0x1A, 0x20);` for `PutWord(p, 0x1A, 0x28);` | round 0 |
| 17 | `Commu_DrawCard` | `SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, 0);` for `SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1E, 0);` | round 0 |
| 18 | `Commu_DrawUnderline` | `PutFloat(p, 0x14, x0 + 0xB);` for `PutFloat(p, 0x14, x0 + 0xC);` | round 0 |
| 19 | `Commu_DrawPiece` | `PutWord(p, 0x18, e[3]);` for `PutWord(p, 0x18, e[2]);` | round 0 |
| 20 | `Commu_DrawPiece` | `PutWord(p, 0x16, 0x7888);` for `PutWord(p, 0x16, 0x7887);` | round 0 |
| 21 | `Commu_DrawFrame` | `for (int i = 0; i < 5; ++i) Piece(x + 8 * i + 0x20, y, 1);` for `for (int i = 0; i < 6; ++i) Piece(x + 8 * i + 0x20, y, 1);` | round 0 |
| 22 | `Commu_DrawFrame` | `const int bottom = y + 0x22;` for `const int bottom = y + 0x23;` | round 0 |
| 23 | `Commu_DrawFrame` | `Piece(x + 8 * i + 8, bottom, 6)` for `Piece(x + 8 * i + 8, bottom, 7)` | round 0 |
| 24 | `Commu_DrawZennyBox` | `Text(at::kBetTitle)` for `Text(at::kZennyTitle)` | round 0 |
| 25 | `Commu_DrawZennyBox` | `(At(at::kFmtWide)), L(at::kBet))` for `(At(at::kFmtWide)), L(bof3::addr::Party_Zenny))` | round 0 |
| 26 | `Commu_DrawStakeBox` | `if (amount <= 10) format` for `if (amount < 10) format` | round 425 |
| 27 | `Commu_DrawStakeBox` | `left = x + 0x37` for `left = x + 0x36` | round 6 |
| 28 | `Commu_DrawStakeBox` | `if (amount < 999)` for `if (amount < 1000)` | round 291 |
| 29 | `CommuHiLo_DrawMarker` | `PutFloat(p, 0x24, y0 - 5);` for `PutFloat(p, 0x24, y0 - 6);` | round 0 |
| 30 | `CommuHiLo_DrawMarker` | `if ((frame & 4) == 0)` for `if ((frame & 8) == 0)` | round 1 |
| 31 | `CommuHiLo_DrawMarker` | `SH_CALL(Gfx_CommitPrim)(1, 0x28);` for `SH_CALL(Gfx_CommitPrim)(1, 0x2C);` | round 0 |
| 32 | `CommuHiLo_DrawRow` | `y = B(at::kPicks + i) != 0 ? 0x64 : 0x74;` for `y = B(at::kPicks + i) != 0 ? 0x74 : 0x64;` | round 0 |
| 33 | `CommuHiLo_DrawRow` | `if (i < k && k != 0 && i != 0)` for `if (i <= k && k != 0 && i != 0)` | round 0 |
| 34 | `CommuHiLo_DrawRow` | `Card(0x10 + 32 * i, y, 0xFE);` for `Card(0x10 + 32 * i, y, 0xFF);` | round 1 |
| 35 | `CommuHiLo_Shuffle` | `for (j = 0; j < i && B(at::kCards + j) != v + 1; ++j) {}` for `for (j = 0; j < i && B(at::kCards + j) != v; ++j) {}` | round 0 |
| 36 | `CommuHiLo_DrawChoice` | `Piece(x, y, m == 2 ? 9 : 0xA);` for `Piece(x, y, m == 1 ? 9 : 0xA);` | round 13 |
| 37 | `CommuHiLo_DrawChoice` | `m == 2 ? 6 : 0, 2, text` for `m == 2 ? 7 : 0, 2, text` | round 13 |
| 38 | `CommuHiLo_DrawChoice` | `if (m > 3) return;` for `if (m > 2) return;` | round 3 |
| 39 | `CommuHiLo_DrawChoices` | `case 1: a = 2, b = 2, c = 0; break;` for `case 1: a = 2, b = 1, c = 0; break;` | round 2 |
| 40 | `CommuHiLo_DrawChoices` | `case 0xFE: a = 0` for `case 0xFF: a = 0` | round 17 |
| 41 | `CommuHiLo_OpenFade` | `SetWord(At(at::kCards + 8), 0xFF00);` for `SetWord(At(at::kCards + 8), 0xFFFF);` | round 0 |
| 43 | `CommuHiLo_Intro` | `StakeBox(0x6E - 0x28 * n, 0x74, 1); if (B(at::kSlide) != 0) return;...` for `StakeBox(0x6E - 0x28 * B(at::kSlide), 0x74, 1); if (B(at::kSlide) !...` | round 0 (case 0: the counter) |
| 44 | `CommuHiLo_DealCards` | `++i;` for `count = B(at::kCount); ++i;` | round 0 (case 1: the count) |
| 45 | `CommuHiLo_Payout` | `} while (i <= static_cast<signed char>(count));` for `} while (i <= S8(at::kCount));` | round 0 (case 1: the count) |
| 46 | `CommuHiLo_DealHints` | `static void ShownValues() { const int bits = S8(at::kCursor); for (...` for `static void ShownValues() { for (int i = 0; i < 8; ++i) if (static_...` | round 17 (case 2: the cursor) |
| 47 | `CommuHiLo_Reveal` | `const auto turn = static_cast<unsigned char>(t + 1);` for `const auto turn = static_cast<unsigned char>(B(at::kTurn) + 1);` | round 208 (case 3: the turn) |
| 48 | `CommuHiLo_Payout` | `const int t = t0; Room("CommuHiLo_Payout` for `const int t = S8(at::kTurn); Room("CommuHiLo_Payout` (and the earlier read added) | round 500 (case 3: the turn) |
| 49 | `CommuHiLo_OpenMusic` | `const auto phase = static_cast<unsigned char>(B(at::kPhase) + 1); i...` for `if (SH_CALL(File_LoadDone)() == 0) return; SH_CALL(Music_Play)(0x94...` | round 297 (case 4: the phase) |
| 50 | `CommuHiLo_Intro` | `B(at::kStep) = 1; return; } const auto n = static_cast<unsigned cha...` for `B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1); return...` | round 81 (case 4: the step) |
| 51 | `CommuHiLo_WaitIntro` | `if (Request() != 2) B(at::kState) = static_cast<unsigned char>(s0 +...` for `if (Request() != 2) B(at::kState) = static_cast<unsigned char>(B(at...` (and the earlier read added) | round 31 (case 4: the state) |
| 52 | `CommuHiLo_BetInput` | `if (bet0 == 0) {` for `if (L(at::kBet) == 0) {` (and the earlier read added) | round 2451 (case 5: the stake) |
| 53 | `Commu_DrawStakeBox` | `const unsigned style = B(at::kStyle); const auto amount = static_ca...` for `const unsigned style = B(at::kStyle); SH_CALL(Menu_DrawBox)(x + 1, ...` | round 193 (case 5: the stake) |
| 54 | `Commu_DrawZennyBox` | `const U z0 = L(bof3::addr::Party_Zenny); SH_CALL(Text_DrawAt)(x + 0...` for `SH_CALL(Text_DrawAt)(x + 0x24, y + 7, 0, 3, Text(at::kZennyTitle));...` | round 747 (case 6: Party_Zenny) |
| 56 | `CommuHitBlow_End` | `Card(8 * (0x11 - m), 0x78 - 0xE * m, d1);` for `Card(8 * (0x11 - m), 0x78 - 0xE * m, B(rec + 1));` (and the earlier read added) | round 4654 (case 7: a block byte) |
| 57 | `CommuHiLo_BetInput` | `S8(at::kDigits + 1) * 10 + ((pressed & 0x5000) ? S8(at::kDigits) : ...` for `S8(at::kDigits + 1) * 10 + S8(at::kDigits);` | round 587 (case 8: a digit) |
| 58 | `CommuHiLo_Payout` | `if (lost0 != 0) {` for `if (B(at::kLost) != 0) {` (and the earlier read added) | round 992 (case 9: the lost flag) |
| 59 | `CommuHitBlow_End` | `if (again0 == 0) {` for `if (B(at::kAgain) == 0) {` (and the earlier read added) | round 72 (case 9: the again choice) |
| 60 | `CommuHiLo_Close` | `const unsigned char t0 = B(at::kTailState); SH_CALL(Music_Play)(L(a...` for `SH_CALL(Music_Play)(L(at::kMusic) & 0xFF, 8); B(at::kTailState) = s...` | round 1243 (case 10: the tail state) |
| 61 | `CommuHiLo_BetInput` | `if (static_cast<signed char>(c) >= 4) B(at::kCount) = 0;` for `if (static_cast<signed char>(c) >= 3) B(at::kCount) = 0;` | round 3 |
| 62 | `CommuHiLo_BetInput` | `if (v > 99) {` for `if (v > 100) {` | **not refused** (equivalent: at 100 the stake is already 100) |
| 63 | `CommuHiLo_BetInput` | `SetL(at::kBet, zenny);` for `SetL(at::kBet, static_cast<U>(static_cast<int>(static_cast<std::int...` | **not refused** (equivalent: zenny < 100 there) |
| 64 | `CommuHiLo_BetInput` | `SetL(at::kBet, static_cast<U>(static_cast<int>(static_cast<std::int...` for `SetL(at::kBet, static_cast<U>(static_cast<int>(static_cast<std::int...` | round 0 (near variant of 63) |
| 65 | `CommuHiLo_BetInput` | `const auto step = static_cast<unsigned char>(B(at::kStep) + 1);` for `const auto step = static_cast<unsigned char>(B(at::kStep) + 2);` | round 10 |
| 66 | `CommuHiLo_BetInput` | `SetL(bof3::addr::Party_Zenny, zenny + stake);` for `SetL(bof3::addr::Party_Zenny, zenny - stake);` | round 24 |
| 67 | `CommuHiLo_BetInput` | `if (B(at::kPhase) != 1 \|\| B(at::kState) != 3) return;` for `if (B(at::kPhase) != 1 \|\| B(at::kState) != 2) return;` | round 2 |
| 68 | `CommuHiLo_BetInput` | `SH_CALL(Commu_DrawUnderline)(0xBA - 12 * S8(at::kCount), 0x98);` for `SH_CALL(Commu_DrawUnderline)(0xBA - 12 * S8(at::kCount), 0x99);` | round 2 |
| 69 | `CommuHiLo_BetInput` | `if (static_cast<signed char>(d) > 8) B(at::kDigits + i) = 0;` for `if (static_cast<signed char>(d) > 9) B(at::kDigits + i) = 0;` | round 1165 |
| 70 | `CommuHiLo_BetSlide` | `ZennyBox(0x12 * n + 0x26, 0xD * n + 0x10);` for `ZennyBox(0x12 * n + 0x26, 0xE * n + 0x10);` | round 0 |
| 71 | `CommuHiLo_BetBack` | `if (B(at::kSlide) == 3) SH_CALL(CommuHiLo_Quit)();` for `if (B(at::kSlide) == 4) SH_CALL(CommuHiLo_Quit)();` | round 1 |
| 72 | `CommuHiLo_Quit` | `Sound(0x105); Message(0x72);` for `Sound(0x106); Message(0x72);` | round 0 |
| 73 | `CommuHiLo_DealStart` | `SetWord(At(at::kPicks + 10), 0);` for `SetWord(At(at::kPicks + 9), 0);` | round 2 |
| 74 | `CommuHiLo_DealStart` | `B(at::kSlide) = 2;` for `B(at::kSlide) = 3;` | round 2 |
| 75 | `CommuHiLo_DealCards` | `unsigned char left = r < 8 ? 1 : (r < 0xD ? 2 : 3);` for `unsigned char left = r < 8 ? 1 : (r < 0xE ? 2 : 3);` | round 12 |
| 76 | `CommuHiLo_DealCards` | `if ((Rnd() & 1) == 0) {` for `if ((Rnd() & 3) == 0) {` | round 13 |
| 77 | `CommuHiLo_DealCards` | `Card((2 * c - B(at::kSlide)) * 16, 0x6C, 0xFF);` for `Card((2 * c - B(at::kSlide) + 1) * 16, 0x6C, 0xFF);` | round 0 |
| 78 | `CommuHiLo_DealCards` | `if (dealt != 8) {` for `if (dealt != 9) {` | round 4 |
| 79 | `CommuHiLo_DealHints` | `Message(0x74);` for `Message(0x73);` | round 0 |
| 80 | `CommuHiLo_DealWait` | `ShownValues(); HiLoBoxes(); if (Request() == 1) return;` for `ShownValues(); HiLoBoxes(); if (Request() == 2) return;` | round 0 |
| 81 | `CommuHiLo_DealChoices` | `Choice(0xB8, 0x44 - 0x1E * B(at::kSlide), 2, 1);` for `Choice(0xB8, 0x44 - 0x1E * B(at::kSlide), 2, 0);` | round 0 |
| 82 | `CommuHiLo_Pick` | `selected = n < 1 ? 2 : (n >= 7 ? 3 : 0xFF);` for `selected = n < 1 ? 2 : (n >= 8 ? 3 : 0xFF);` | round 16 |
| 83 | `CommuHiLo_Pick` | `B(at::kPicks + i) = cursor;` for `B(at::kPicks + 1 + i) = cursor;` | round 62 |
| 84 | `CommuHiLo_Pick` | `if (n < 8) SH_CALL(CommuHiLo_DrawMarker)((n + 1) * 32, 0x6E);` for `if (n < 8) SH_CALL(CommuHiLo_DrawMarker)((n + 2) * 32, 0x6E);` | round 0 |
| 85 | `CommuHiLo_Pick` | `if (B(at::kCursor) == 2) B(at::kCursor) = 1;` for `if (B(at::kCursor) == 2) B(at::kCursor) = 0;` | round 586 |
| 86 | `CommuHiLo_Pick` | `B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) ^ 3);` for `B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) ^ 1);` | round 34 |
| 87 | `CommuHiLo_PickClose` | `B(at::kTurn) = 2;` for `B(at::kTurn) = 1;` | round 1 |
| 88 | `CommuHiLo_PickClose` | `for (++i; i < 8; ++i)` for `for (++i; i < 9; ++i)` | round 0 |
| 89 | `CommuHiLo_Reveal` | `B(at::kCards + t - 1) > B(at::kCards + t) ? 1 : 0;` for `B(at::kCards + t - 1) >= B(at::kCards + t) ? 1 : 0;` | round 614 |
| 90 | `CommuHiLo_Reveal` | `if (static_cast<signed char>(turn) == static_cast<signed char>(coun...` for `if (static_cast<signed char>(turn) == static_cast<signed char>(coun...` | round 172 |
| 91 | `CommuHiLo_Reveal` | `Sound(0x108);` for `Sound(0x107);` | round 2 |
| 92 | `CommuHiLo_Reveal` | `B(at::kLost) = 2;` for `B(at::kLost) = 1;` | round 2 |
| 93 | `CommuHiLo_RevealPause` | `Row(all, count, count);` for `Row(all, all, count);` | round 0 |
| 94 | `CommuHiLo_Payout` | `factor * L(at::kBet) + 0x31u` for `factor * L(at::kBet) + 0x32u` | round 332 |
| 95 | `CommuHiLo_Payout` | `SetL(at::kBet, static_cast<U>(product / 99));` for `SetL(at::kBet, static_cast<U>(product / 100));` | round 1 |
| 96 | `CommuHiLo_Payout` | `At(at::kPayouts + 2 * t + 2)` for `At(at::kPayouts + 2 * t)` | round 1 |
| 97 | `CommuHiLo_Payout` | `message = 0x75;` for `message = 0x74;` | round 11 |
| 98 | `CommuHiLo_CheckWinnings` | `> 10000` for `>= 10000` | round 6 |
| 99 | `CommuHiLo_CheckWinnings` | `B(at::kState) = 4;` for `B(at::kState) = 3;` | round 2 |
| 100 | `CommuHiLo_CashOut` | `SH_CALL(Zenny_Add)(L(at::kBet), 1);` for `SH_CALL(Zenny_Add)(L(at::kBet), 0);` | round 2 |
| 101 | `CommuHiLo_Restart` | `StakeBox(0xF * m + 0x6E, 0x74 - 0x18 * m, 0);` for `StakeBox(0xF * m + 0x6E, 0x74 - 0x19 * m, 0);` | round 2 |
| 102 | `CommuHiLo_Restart` | `B(at::kState) = 2; B(at::kStep) = 0;` for `B(at::kState) = 1; B(at::kStep) = 0;` | round 40 |
| 103 | `CommuHiLo_Leave` | `ZennyBox(0x26, 0x10 - 0x13 * n);` for `ZennyBox(0x26, 0x10 - 0x14 * n);` | round 2 |
| 104 | `CommuHiLo_Leave` | `B(at::kPhase) = 1; B(at::kState) = 0;` for `B(at::kPhase) = 2; B(at::kState) = 0;` | round 11 |
| 105 | `CommuHiLo_Close` | `SH_CALL(Music_Play)(L(at::kMusic) & 0x7F, 8);` for `SH_CALL(Music_Play)(L(at::kMusic) & 0xFF, 8);` | round 3 |
| 106 | `CommuHiLo_Close` | `if (Request() == 1) return; SH_CALL(Music_FadeOutStop)(10);` for `if (Request() == 2) return; SH_CALL(Music_FadeOutStop)(10);` | round 17 |
| 107 | `CommuHitBlow_OpenMusic` | `Message(0x7E);` for `Message(0x7F);` | round 0 |
| 108 | `CommuHitBlow_OpenWait` | `SetL(at::kBet, 1); B(at::kState) = state;` for `SetL(at::kBet, 0); B(at::kState) = state;` | round 2 |
| 109 | `CommuHitBlow_OpenSlide` | `GuessPanel(0x10 - 0x1D * n, 0x18, 0); ZennyBox(0x1E * B(at::kSlide)...` for `GuessPanel(0x10 - 0x1E * n, 0x18, 0); ZennyBox(0x1E * B(at::kSlide)...` | round 0 |
| 110 | `CommuHitBlow_Start` | `SetL(bof3::addr::Party_Zenny, zenny - 499u);` for `SetL(bof3::addr::Party_Zenny, zenny - 500u);` | round 1 |
| 111 | `CommuHitBlow_Start` | `std::memset(At(at::kRecords), 1, 39);` for `std::memset(At(at::kRecords), 1, 40);` | round 1 |
| 112 | `CommuHitBlow_Start` | `if (j < i - 1) continue;` for `if (j != i) continue;` | round 10 |
| 113 | `CommuHitBlow_StartSlide` | `Card(8 * (0x11 - n), 0x78 - 0xE * n, 2);` for `Card(8 * (0x11 - n), 0x78 - 0xE * n, 1);` | round 0 |
| 114 | `CommuHitBlow_Prompt` | `Message(0x81);` for `Message(0x80);` | round 0 |
| 115 | `CommuHitBlow_WaitPrompt` | `B(at::kCursor) = 1; B(at::kState) = state;` for `B(at::kCursor) = 0; B(at::kState) = state;` | round 2 |
| 116 | `CommuHitBlow_Input` | `if (static_cast<signed char>(d) < 0) B(at::kCards + index) = 9;` for `if (static_cast<signed char>(d) < 1) B(at::kCards + index) = 9;` | round 41 |
| 117 | `CommuHitBlow_Input` | `if (d > 8) B(at::kCards + index) = 1;` for `if (d > 9) B(at::kCards + index) = 1;` | round 38 |
| 118 | `CommuHitBlow_Input` | `const auto index = static_cast<unsigned char>(row * 5 + cursor + 4);` for `const auto index = static_cast<unsigned char>(row * 5 + cursor + 3);` | round 0 |
| 119 | `CommuHitBlow_Input` | `HitBlowPanels(0, 0, 0);` for `HitBlowPanels(0, 0, 1);` | round 0 |
| 120 | `CommuHitBlow_Score` | `if (rec[i] == B(at::kCards + j)) ++blows;` for `if (i != j && rec[i] == B(at::kCards + j)) ++blows;` | round 1 |
| 121 | `CommuHitBlow_Score` | `rec[3] = blows;` for `rec[3] = hits;` | round 0 |
| 122 | `CommuHitBlow_Score` | `else if (static_cast<signed char>(v) > 2) v = 1;` for `else if (static_cast<signed char>(v) > 2) v = 2;` | round 39 |
| 123 | `CommuHitBlow_Score` | `std::memcpy(At(at::kTextB), name, 15);` for `std::memcpy(At(at::kTextB), name, 16);` | round 10 |
| 124 | `CommuHitBlow_Score` | `SH_CALL(Crt_sprintf)(text_a, format, r);` for `SH_CALL(Crt_sprintf)(text_a, format, r + 1);` | round 10 |
| 125 | `CommuHitBlow_Score` | `SH_CALL(Item_NamePtr)(item, category)` for `SH_CALL(Item_NamePtr)(category, item)` | round 10 |
| 126 | `CommuHitBlow_Score` | `B(at::kState) = 6;` for `B(at::kState) = 7;` | round 10 |
| 127 | `CommuHitBlow_NextGuess` | `for (int j = 0; j < 2; ++j) rec[j] = rec[j - 5];` for `for (int j = 0; j < 3; ++j) rec[j] = rec[j - 5];` | round 2 |
| 128 | `CommuHitBlow_NextGuess` | `static_cast<unsigned char>(B(at::kCount) - 0), 0);` for `static_cast<unsigned char>(B(at::kCount) - 1), 0);` | round 2 |
| 129 | `CommuHitBlow_NextGuess` | `GuessPanel(0x10, 0x18, 0); GuessRow(0x68, 0x78, B(at::kCount), 0); ...` for `GuessPanel(0x10, 0x18, 1); GuessRow(0x68, 0x78, B(at::kCount), 0); ...` | round 0 |
| 130 | `CommuHitBlow_Lost` | `GuessRow(0x68, 0x78, row == 8 ? 8 : row, 0);` for `GuessRow(0x68, 0x78, row == 8 ? 7 : row, 0);` | round 0 |
| 131 | `CommuHitBlow_Lost` | `B(at::kState) = 9;` for `B(at::kState) = 8;` | round 2 |
| 132 | `CommuHitBlow_Prize` | `SH_CALL(Inventory_Add)(category, item, 2);` for `SH_CALL(Inventory_Add)(category, item, 1);` | round 2 |
| 133 | `CommuHitBlow_Prize` | `if (added != 0) {` for `if (added == 0) {` | round 2 |
| 134 | `CommuHitBlow_End` | `const unsigned char r = count == 8 ? 6 : count;` for `const unsigned char r = count == 8 ? 7 : count;` | round 0 |
| 135 | `CommuHitBlow_End` | `if (B(at::kSlide) != 4) return;` for `if (B(at::kSlide) != 5) return;` | round 30 |
| 136 | `CommuHitBlow_End` | `Card(0xA8 - 16 * B(at::kSlide), 0x40, B(at::kCards + 1));` for `Card(0xA8 - 16 * B(at::kSlide), 0x40, B(at::kCards + 2));` | round 0 |
| 137 | `CommuHitBlow_End` | `B(at::kSlide) = 0; B(at::kStep) = 3;` for `B(at::kSlide) = 0; B(at::kStep) = 2;` | round 6 |
| 138 | `CommuHitBlow_Close` | `B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 2);` for `B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);` | round 0 |
| 139 | `Commu_PushSubscreen` | `B(at::kSavedState) = state + 1;` for `B(at::kSavedState) = state;` | round 0 |
| 140 | `CommuHiLo_OpenFade` | `SetL(at::kCards + 4, 0xFFFFFF00u);` for `SetL(at::kCards + 4, 0xFFFFFFFFu);` | round 0 |
| 141 | `CommuHitBlow_OpenMusic` | `SH_CALL(Music_Play)(0x94, 9); Message(0x7F);` for `SH_CALL(Music_Play)(0x94, 8); Message(0x7F);` | round 0 |
| 142 | `CommuHiLo_Intro` | `B(at::kCount) = 0; B(at::kState) = state; B(at::kStep) = 1;` for `B(at::kCount) = 0; B(at::kState) = state; B(at::kStep) = 0;` | round 0 |
| 143 | `CommuHiLo_DrawChoice` | `L(at::kChoiceLabels + 4 * (2 - i))` for `L(at::kChoiceLabels + 4 * i)` | round 13 |
| 144 | `CommuHiLo_DrawMarker` | `((frame & 6) << 5) + 0x3E` for `((frame & 6) << 5) + 0x3F` | round 0 |
| 145 | `CommuHitBlow_OpenFade` | `B(at::kMusic) = static_cast<unsigned char>(Music_Track + 1);` for `B(at::kMusic) = Music_Track;` | round 0 |
| 146 | `CommuHitBlow_Score` | `if (B(at::kCards + j) == rec[2 - j]) ++hits;` for `if (B(at::kCards + j) == rec[j]) ++hits;` | round 0 |
| 147 | `CommuHitBlow_Prize` | `const int prize = S8(at::kCount) * 3 + (2 - S8(at::kCursor));` for `const int prize = S8(at::kCount) * 3 + S8(at::kCursor);` | round 2 |
| 148 | `CommuHiLo_BetInput` | `SetL(at::kBet, 99);` for `SetL(at::kBet, 100);` | round 12 (near variant of 62) |

## 7. Latent defects and the one policy (Capcom's, described, not fixed)

**The policy**: where the original indexes a table or a run of cells by a byte
it never bounds, ours aborts with a `Fatal` before the access - the nine
dispatchers past their tables; `Commu_DrawPiece` past 18; `CommuHiLo_DrawChoice`
past 3 labels; `CommuHiLo_DrawRow` a pick outside 1..8 or a value past 8;
`CommuHiLo_BetInput` a digit cursor outside 0..2 (where a digit is moved);
`CommuHiLo_Pick` a pick count below 0 (where a pick is stored);
`CommuHiLo_Reveal` and `_Payout` a turn outside 1..8, `_Payout` a value index
past 8; `CommuHitBlow_Input` a row outside 0..7 or a cursor outside 0..2 (where
a digit is moved); `_Score` a row outside 0..7, `_Score` and `_Prize` a prize
past 24; `_NextGuess` a new row outside 1..7 (where it copies); `_End` a row
past 7 (where it draws the last guess). `Commu_RandDigit(0)` never ends in the
original (no value is below 1): ours aborts before the loop. In ordinary play
every writer keeps these inside: the states step the bytes through their
tables, the count and the cursor wrap or stop at their ends, the turn starts at
1 and stops at the picks' count.

- **Transitions left to the messages**: `CommuHiLo_CashOut` (PlayStates[10])
  never moves the state, and `CommuHiLo_Restart` / `_Leave` (11, 12) are reached
  by no state's write; likewise the second game's `0x939A41`. The bytes are the
  message-choice bytes, so a choice in messages `0x75` / `0x78` / `0x79` and
  the second game's end presumably writes them; not read here (the scripts are
  data). An index past a table there would be the scripts', not the code's.
- **`CommuHitBlow_Start` takes 500 zenny unchecked**: Party_Zenny - 500 wraps
  below 0. Whether the script before it checks the money is not read.
- **`CommuHiLo_BetInput` reads the stake's low byte as signed**: a stake of
  128..255 would split into negative digits. Every path into it sets the stake
  to 0 first (`_Intro`, `_Restart`); `_CheckWinnings` sends larger winnings to
  the deal, not the bet. Unreachable as read.
- **The bet held to Party_Zenny takes its low word, signed** (`movsx ecx, cx`):
  equal to Party_Zenny wherever it is taken (it is below the stake, so below
  100) - control 63 is that equivalent mutant.
- **Payout factor 0** would be the high half of a code pointer; the turn is at
  least 1 there.
- **Their own argument slots**: `Commu_RandDigit` stores its draw, and
  `CommuHiLo_DrawMarker` and `Commu_DrawFrame` keep temporaries, in their own
  stack arguments (dead after the return; ours does not, and nothing reads
  them).

## 8. Calls across groups

**Outbound**: to R4D (wave four, called raw, `rest_4c_callees.h`): `0x45C400`
(15 sites), `0x45C7D0` (9), `0x45C850` (9) - 33. To Capcom's library
`0x5A7570` (one, raw; nobody owns it). Everything else by name, ours already:
`Sound_PlayEffect`, `Msg_OpenScript`, `Music_Play`, `Music_FadeOutStop`,
`File_LoadDone`, `Input_AutoRepeat`, `Menu_DrawBox`, `Menu_DrawHand`,
`Text_DrawAt`, `Text_DrawFont12`, `Gpu_SetDrawMode`, `Gpu_SetSprt`,
`Gpu_SetLineF2`, `Gfx_CommitPrim`, `Zenny_Add`, `Inventory_Add`,
`Item_NamePtr`; Capcom's `Rand`, `Crt_sprintf`.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `0x457DD0`, `0x457FE0`, `0x458240` | R4B | `Commu_PushSubscreen` `0x459EE0` (three `call`s) |
| `0x457340` through `0x652A84[0]`, `[1]` | R4B | `CommuHiLo_Run`, `CommuHitBlow_Run`, read in place |
| `0x45C700` | R4D | `Commu_DrawPiece` `0x45B400` (8 sites) |
| `0x45C7D0`, `0x45C850` | R4D | `Commu_DrawCard` `0x45B2C0` (3 sites) |

## 9. The live route

None: no recorded route enters the faerie village (the brief), and no
first-call trace under `analysis/calltrace` names any of the 60. **Fuzz only.**
No live run was made. A route through the village's two games (both outcomes
of each, the stake's cancel, the eighth miss) would let the coordinator's
state hash cover them; the owner records it once the band is ours.

## 10. The rebinding

`grep -rn -i` of the 60 addresses and the nine tables in `src/game`
(`band_rows.py --refs`): **no raw reference** in our files. Nothing rebound,
nothing left raw. For the coordinator: R4B's files will call `0x459EE0` and
hold `0x459F20` / `0x45B770` in its table's description, and R4D's call
`0x45B400` / `0x45B2C0` - raw until this merges.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-05): 47 lines, the read extents
of the hidden starts. Three existing lines carry a host's longer extent over
functions of this group and are left in place: `00459EE0 1074` (the function
is 0x3D; it covers 27 hidden starts), `0045B2C0 DF` (0xB0; covers `0x45B370`),
`0045B5F0 E04` (0x177; covers 19).
