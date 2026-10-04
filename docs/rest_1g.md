# Group R1G: the fishing spot - the rest of the leader's state 9, and game mode 8's fish

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, on the
round branch's tip `ba2c3c3`. **45 functions ours** (`src/game/rest_1g.cpp`,
`rest_1g.h`, shadow name `rest_1g`): the cut's 43 rows for R1G
(`analysis/round14_cut.tsv`) and the two starts in their spans no list had
(`0x528BE0`, `0x52BF90`, band_rows' "code no list has"). Each read to its last
instruction with capstone and fuzzed through the scenario harness's **field**
mode (used unchanged): 270,000 rounds, **0 mismatches**; 103 of 103 controls
refused (section 6).
Six `.data` tables named. Fuzz only here; two recorded routes reach the fish
(section 9).

**What the band is.** Not the party sets' field actions: everything here is
the fishing spot's code ([`fishing-text.md`](fishing-text.md) places the
machine; the names below say what the code does, not what the player sees -
no gameplay fact is stated from memory). Two machines:

- **The leader's state 9** (`LeaderPanel_*`, E1E's in round thirteen,
  [`effect_1e.md`](effect_1e.md)): its stage 1 dispatcher and steps 0..4 and
  7, stage 9's step 4, stages 10 and 11 with their steps, the two pages of
  the menu E1F's `ChoiceMenu_Run` jumps to, and the eight helpers E1E called
  by address. `Sprite_Current` is the leader's `ObjTrio` record.
- **Game mode 8's fish** (`Fish_*`): `Fish_Spawn` (called once by game mode
  8's entry `0x496440`, R3G's) fills `Sprite_Objects` from the spot's counts;
  `Fish_RunAll` (called by `GameMode8_Frame`, E1F's) runs every record in use
  through `Fish_States` by its `+1`, with `Sprite_Current` **and**
  `Field_ActiveMember` the record.

| Function | Entry | Bytes | Reached by | What |
|---|---|--:|---|---|
| `LeaderPanel_S1` | `0x5289A0` | 0x12 | `LeaderPanel_Stages[1]` | jmp through `LeaderPanel_Stage1Steps` by `+3` (11) |
| `LeaderPanel_S1Begin` | `0x5289C0` | 0xA2 | Stage1Steps[0] | the record pointers, sprite index 0xFF, `+0xC..+0x23` and `+9` cleared, record 3's mode 0 / 6 by the two item bytes, the animation by `Field_State +0x89`, `+3` up |
| `LeaderPanel_S1Wait` | `0x528A70` | 0x12 | [1] | record 3 at 6: `+3` up |
| `LeaderPanel_S1Buttons` | `0x528A90` | 0x14D | [2] | a byte-indexed switch on `Input_Pressed & 0x74` exactly: 0x04 stage 10, 0x10 stage 9, 0x40 `+3` up, 0x20 stage 2 (or a first-time warning through `Inventory_Holds38To4DAt99`, or stage 9 when an item byte is 0) |
| `LeaderPanel_S1Idle` | `0x528BE0` | 0x35 | [3] and [7] | records 3 idle and 1 at 1: `+9` = 4, sound 0x102, `+3` up; tail `FieldPanel_DrawShade` |
| `LeaderPanel_S1Box3In` | `0x528C20` | 0xA8 | [4] | box 3 slid by `+9` (x from the pointer's upper half, as E1E's `PtrHi`), the shade, the prompt box and message |
| `ChoiceMenu_DataPage` | `0x52ADA0` | 0xE0 | `ChoiceMenu_Choices[1]` | the title; record 6 at 8: 0x2000 / 0x8000 turn the page word `+0x3C` from `+0x3E` +/- 1 over 23 pages; cancel back |
| `ChoiceMenu_RulePage` | `0x52AE80` | 0xAC | [2] | the title; record 6 at 0xE, `+8` clear: cancel back, 0x4000 / 0x1000 turn over 6 pages |
| `LeaderPanel_S9Back` | `0x52AF30` | 0x29 | Stage9Steps[4] | record 6 idle: record 1 up, stage 1 step 0 |
| `LeaderPanel_S10` | `0x52AF60` | 0x12 | Stages[10] | jmp through `LeaderPanel_Stage10Steps` (2) |
| `LeaderPanel_S10Look` | `0x52AF80` | 0x12A | Stage10Steps[0] | `Field_Kind2Z` moved 0x8000 a frame (divisor 0x20) until held 0x40 or at 0x150000 with record 3 idle (divisor 0x40, z 0x3C0000, `+3` = 1); five F2 lines on frames with bit 2 |
| `LeaderPanel_S10End` | `0x52B0B0` | 0x29 | [1] | `Field_Kind2Hold` clear: record 1 up, stage 1 |
| `LeaderPanel_S11` | `0x52B0E0` | 0x12 | Stages[11] | jmp through `LeaderPanel_Stage11Steps` (4) |
| `LeaderPanel_S11FadeOut` | `0x52B100` | 0x1C | Stage11Steps[0] | record 3 idle: `Transition_Start(0)` |
| `LeaderPanel_S11Switch` | `0x52B120` | 0x36 | [1] | wait word clear: `0x93985C..F`, `Draw_PassFlags` cleared, `Game_Step` = 3 |
| `LeaderPanel_S11FadeIn` | `0x52B160` | 0x1A | [2] | `Transition_Start(1)`, `Draw_PassFlags` 0x1F |
| `LeaderPanel_S11End` | `0x52B180` | 0x2A | [3] | wait word clear: record 1 up, stage 1 |
| `LeaderPanel_PoseSound` | `0x52B1B0` | 0x48 | E8 (S3Run, S4Run) | sound 0x203 / 0x202 / 0x205 for pose 0 / 1 / 2 when it differs from the pose before |
| `LeaderPanel_UseItemEnd` | `0x52B200` | 0x42 | Stage7Steps[0], UseItem's tail | animation 9, sound 0x204, record 3 = 7, record 4 up and its `+6` = 3, `+3` up, tail `Sprite_ScriptTick` |
| `LeaderPanel_SetRecords` | `0x52B250` | 0x43 | E8 (S1Begin) | `0x939A24` / `0x939A20` from the item bytes `0x90412E` / `0x904130` (20- and 10-byte records at `0x66A528` / `0x66A4E8`) |
| `LeaderPanel_Effect3Mode` | `0x52B2A0` | 0x34 | E8 (6 sites) | record 3's `+6` = mode; at state 0, 4 or 6 also `+7` = 1 and state up |
| `LeaderPanel_HoldTest` | `0x52B2E0` | 0x4A | E8 (S3Run) | `0x903850` and `+0xA` by bit 0x4000 pressed / held |
| `LeaderPanel_LeaveOnPress` | `0x52B330` | 0x35 | E8 (Leave) | (pressed & buttons) & 0xFFFF: `Transition_Start(2)`, stage 8, al 1 |
| `LeaderPanel_EffectsStep` | `0x52B370` | 0xE2 | E8 (S3Run) | `LeaderPanel_PressLatch`, then four button sequences (`0x66A4C8`) stepped; a completed one sets record 0's `+7` / `+0xA` from the 20-byte record |
| `LeaderPanel_PressLatch` | `0x52B460` | 0x1E | E8 (EffectsStep) | `0x6BC708` = `0x6BC717`; `0x6BC717` = pressed & 0xE020 != 0 |
| `Fish_Spawn` | `0x52B480` | 0x23D | E8 (`0x496440`, R3G) | the records from the spot's counts; size, strength, growth by `Rand` (PSX twin `0x801DE218`) |
| `Fish_RunAll` | `0x52B6C0` | 0x84 | E8 (`GameMode8_Frame`) | each record in use through `Fish_States` by `+1`; `+0x5D..+0x5F` by the depth |
| `Fish_Begin` | `0x52B750` | 0x192 | `Fish_States[0]` | bank, depth, x / z / height from `Rand`, kept off the ground; state 1 |
| `Fish_Swim` | `0x52B8F0` | 0x230 | [1] | the lure in reach: state 3; else a nibble (state 2) or a turn and a climb |
| `Fish_Settle` | `0x52BB20` | 0xAF | [2] | the nibble's animation done: back to state 1 |
| `Fish_Approach` | `0x52BBD0` | 0x3B4 | [3] | toward record 0 (the eight ways), a turn by the level (a four-entry jump table), the climb; close and a bite: hooked (state 4, the leader to stage 4) |
| `Fish_Hooked` | `0x52BF90` | 0x828 | [4] | the pull (section 1.1) |
| `Fish_S5` | `0x52C7C0` | 0x12 | [5] | jmp through `Fish_S5Steps` by `+2` (2) |
| `Fish_S5Center` | `0x52C7E0` | 0x44 | S5Steps[0] | steps toward the half cell, `+9` = 8 |
| `Fish_S5Move` | `0x52C830` | 0x72 | [1] | moved; at 0 snapped, state 1; tail `Sprite_ScriptTick` |
| `Fish_S6` | `0x52C8B0` | 0x12 | [6] | jmp through `Fish_S6Steps` by `+2` (3) |
| `Fish_S6Begin` | `0x52C8D0` | 0x65 | S6Steps[0] | `+0x3C` dword 0, sound 0x206, bank `+0x12`, animation 0 |
| `Fish_S6Wait` | `0x52C940` | 0x21 | [1] | frame word `+0x58` at the kind's (`0x660364`) |
| `Fish_S6Release` | `0x52C970` | 0x12 | [2] | the leader's step above 9: the record freed |
| `Fish_AdjustStrength` | `0x52C990` | 0x4F | E8 (Hooked, 2) | `Field_ActiveMember +0x9A` += a signed byte, kept in 0..`+0x98` |
| `Fish_Chance` | `0x52C9E0` | 0x9E | E8 (Hooked, 2) | odds 1 / 2 / 4 / 8 / 0xE by the tension's distance from an edge; al = odds > `Rand & 0xF` |
| `Fish_Step` | `0x52CA80` | 0x40 | E8 (4) | while `+9`: moved by `+0xC` / `+0x10` / `+0x14` |
| `Fish_Heading` | `0x52CAC0` | 0x176 | E8 (2) | the steps from `MoveCmd_F9Steps` by `+8`, turned at x and the kind's z band; `+8` re-derived |
| `Fish_LureInReach` | `0x52CC40` | 0x8C | E8 (2) | record 0 at 4, nothing picked, the leader at stage 3: kind 0x16 on nibble 4, or `Sprite_PointInReach` |
| `Fish_LureClose` | `0x52CCD0` | 0x77 | E8 (Approach) | within 0x8000 in x and z and 0x400 in height of record 0 |

### 1.1 `Fish_Hooked`

The window is record 5's `+0x30` byte -/+ 4 * (the 10-byte record's `+3` + 3)
(bytes). z at 0x8000 or less lands the fish (record 5 `+0x10` = -7, record 0
idle, the leader to stage 6). A jump (`+4` = 3) waits on
`Sprite_ScriptTick`, then past frame 0x14 moves the tension `+0xA` by `+0x10`.
Otherwise, with record 4 idle, the tension above (below) the window for the
10-byte record's `+8` (`+9`) frames gives `Fish_Chance` a go: with record 5's
`+0x10` negative and the fish moving, a snap (stage 6); below, a break (stage
7). With `+9` counted out a new move: the tier by the strength `+0x9A`
against `+0x98` (below a quarter 3, a half 2, three quarters 1, else 0), the
kind's four chance bytes at `Fish_Kinds + 4 * tier`, each against `Rand &
0xF`: a jump (above -0x40 and tension positive; strength -5), a run (`+0x10`
from `Rand` and the kind's `+0x1D`, `+0x81` by the tension's gap), a rest, or
a drift; `+9` = (`Rand & 3` + 1) * (8 - tier). Else `+9` down and the strength
worn by `+0x81` on every 16th frame or every other. Then the tension clamped
to -0x30..0x30, the divisor, and the fish pulled: tension above the level and
moving, toward z 0 by 0x1000 (x turned at 0x60000 / 0x140000, the camera's
`Field_Kind2Z` following above 0x150000); below it with record 5 negative,
away, x drawn toward 0xF0000 by `(0xF0000 - x) >> 4 / (0x3E - z's cell)`
clamped to 0x1000.

## 2. Divergence

None. `DIVERGENCE.md`, `cheats.cpp`, `widescreen.cpp` and `fishing_text.cpp`
name no address in `0x5289A0..0x52CD46` nor the tables `0x66022C..0x66033F`,
`0x660350..0x660363` (grep, 2026-10-04); DIV-0069's patches are in the kind
0xF draws and the text tables, not here. `LeaderPanel_S10Look`'s lines are 3D
F2 lines, not a full-frame fill (nothing for DIV-0041).

## 3. Arguments and answers

- `LeaderPanel_Effect3Mode` reads `[esp+4]`'s byte; `LeaderPanel_LeaveOnPress`
  the dword, of which `and eax, edx; test ax, ax` keeps 16 bits; al 0 / 1
  (`ret_mask` 0xFF). `Fish_AdjustStrength` reads `movsx cx, byte [esp+4]`;
  `Fish_Chance` `[esp+4] & 0xFF` and `imul byte [esp+8]`, and **writes both
  argument slots** (the caller's stack, never read again); its eax is 0 / 1
  whole. `Fish_LureInReach` / `Fish_LureClose` answer al (callers `test al,
  al`). The rest are `void`; the tail-jmp `Sprite_ScriptTick` answers of
  `LeaderPanel_UseItemEnd`, `Fish_S5Move` and `Fish_S6Wait` are no caller's.
- **Leftovers above what callees read**, handed on as the callee reads them:
  `Fish_Hooked` hands `Fish_Chance` a stack dword whose upper three bytes are
  uninitialised (low byte the edge); the banks are pushed as `cx` / `ax` over
  a callee's leftovers (`Sprite_SetAnimationBank` reads a word); the
  animations as `al` / `dl` over the `Sprite_Current` pointer or leftovers (a
  byte); `Fish_LureInReach` pushes the height as `dx` over its caller's `edx`
  (`Sprite_PointInReach` reads a short); `LeaderPanel_S1Box3In`'s box and text
  y are `movzx` over a callee's leftovers (the draws read 16 bits). Its box x
  is the `Sprite_Current` pointer's upper half above `+9`, multiplied whole:
  ours computes the same 32 bits (E1E's `PtrHi`).
- `LeaderPanel_S10Look` calls `Gte_RotTransPers` with four arguments; ours
  (three) has no flag argument, which the original never reads
  (`psx_gte_transform.cpp`).
- **Same-expression ordering** (round thirteen's E3B trap): every `Rand` is
  taken into a local before the cell read after it.

## 4. The fuzz (`rest_1g_fuzz.cpp`)

45 clones, 6,000 rounds each, field mode (`g.field`), **not effect mode**
(neither machine runs on an effect record). 41 `kSprite`, four `kCall`
(`Effect3Mode`, `LeaveOnPress`, `AdjustStrength`, `Chance`); `ret_mask` 0xFF
on `LeaveOnPress`, `Chance`, `LureInReach`, `LureClose`. `sprite_span` 7
(`Fish_States`' length: `Fish_RunAll` reads the next record's `+1` after a
call). The six tables are `DataTable`s (their cells swapped for recorders).
`BOF3X_R1G_ONLY=<name>` runs the clones whose name contains it.

**The callees** (all the group's own listing): the nine of the group called
by `E8` (`SetRecords`, `Effect3Mode`, `PressLatch`, `LureInReach`,
`LureClose`, `Step`, `Heading`, `AdjustStrength`, `Chance`) and eighteen of
ours with the width each reads (section 3); `Rand` is the standard `kRand`.
**Louder stand-ins:** `AreaMap_Elevation` answers at the record's height give
or take one two times in three; `Fish_Heading` logs the `+8` it is handed
and half the time writes a new one (0..8: its callers read it after); `LeaderPanel_PressLatch` writes `0x6BC717` 0..2;
`Gfx_CommitPrim` moves the packet cursor on 0x20 (each line its own packet).
`Gte_RotTransPers` hashes its vertex (6 bytes on the stack) and logs the
packet's sxy pointer.

**Regions** beyond field mode's: `0x939A00` + 0x30 (the sprite index and the
record pointers), `0x6BC700` + 0x20 (the latch, the sequences, the nibble,
the poses), `Game_Mode` / `Game_Step`, `Field_ActiveMember`, `0x93985C` + 4.
23,688 bytes, 43 regions, 286 stand-ins.

**The seed** (every round): the leader's three `ObjTrio` records and the four
sprite records' bytes at their compares (fish: `+0` with and without bits 5 and
7, kind 0..22 or 0x15 / 0x16, direction 0..7 or past, `+9` 0 often, tension
`+0xA` at -0x31..0x31's edges, the steps at +/- 0x400 / 0x800 / 0x1000 and the
32-bit limits, x at 0x60000 / 0x80000 / 0x140000 / 0xF0000 +/- 1, z at 0x8000,
0x110000, 0x150000, 0x1A0000, 0x320000, 0x3C0000, 0x3E0000 +/- 1, height at 0,
-0x20, -0x40 +/- 1, -0x200, the frame word near 0x14, the strength `+0x9A` at
its tier boundaries against `+0x98`, `+0x81` signed); records 4..29 in use or
not with `+1` below 7; `Sprite_Current` the leader two times in three for the
leader's functions, a sprite record for the fish, `Field_ActiveMember` the same
record three times in four; the dispatchers' index below its table; the
record pointers into the scratch buffers with `+3`, `+8`, `+9`, `+0xE`, `+0xF`
at their compares; effect record 0 at state 4 / 5 and within / at / past
0x8000 and 0x400 of the fish; records 1, 3, 4, 5, 6 at the states the steps
test; the item bytes 0 / at their bases; the pose bytes equal half the time;
the spot (`0x905B88`) one whose counts fit 30 records; the pad on each bit the
steps test; `Rand`'s hint on the bits the fish test.

**The disturbance** (the group's case, from its hash only): the fish's `+9`,
`+0xA`, `+8`, height, `+4`, `+0x10`, kind (0..22); the member's strength
words; record 0's state; the sprite index; the leader's stage; the nibble;
the press latch; record 5's frame and level; record 4's state;
`Field_Kind2Hold`; the 20-byte record's `+0xE` / `+0xF`.

**Result** (this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_1g`, exit
0): 270,000 rounds, 608,028 calls to the stand-ins, **0 mismatches** (the first
run, 4,000 rounds before `SeedFor`, passed too). Every table entry reached
(handler recorders 493..18,387 calls each); the thinnest callees
`Fish_AdjustStrength` 360, `Inventory_Holds38To4DAt99` 516, `Fish_Chance`
1,337 calls. **`SeedFor`** adds, two times in three, each function's joint
conditions: `Fish_Swim` at rest at height 0 with no nibble; the lure tests
with record 0 at 4, nothing picked, record 4 idle, the leader at stage 3 and
record 0's `+7` within -2..4 of the kind's level; `Fish_Hooked` with record 4
idle, `+4` 0..3, record 5's level at the tension's gap bounds and the height
at -0x40 +/- 1; `Fish_AdjustStrength`'s strength at the delta's negation and a
negative `+0x98`. `Frame_Counter`'s low nibble 0, 8, 1 or 4 half the time.

**Under `'*'`** (this worktree, `BOF3X_SHADOW='*'`, after the rebinding):
exit 0, `inject: 8700 ours, 0 left original`, 1,019 self-test lines of 0
mismatches and none other (among them `rest_1g`, 608,236 calls, and
`effect_1e` / `effect_1f`, whose constants were rebound); the same with
`BOF3X_WIDE=1`: exit 0, 1,019, 8,700 ours. Each passed on its first run.
`tools/ledger_check.py`: 72 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Two starts no list had**, both functions of their own reached by table
  cells: `0x528BE0` (Stage1Steps[3] and [7]; after `0x528A90`'s jump tables)
  and `0x52BF90` (`Fish_States[4]`; after `0x52BBD0`'s jump table). Taken.
- **Extents**: band_rows' read sizes are the code's; the cut's are the
  catalogue's with padding, except `0x52BBD0` (cut 3,056, read 0x3B4: the
  catalogue ran on over `0x52BF90`) and the catalogue's `0x52B1B0` 0x92 and
  `0x52B6C0` 0x12C2 (hosts swallowing `0x52B200` and the fish states).
- **No case, no shared tail, nothing dropped.** The hidden starts are entries
  by address (table cells, a tail jmp for `0x52B200`); `0x528A90` and
  `0x52BBD0` carry their own jump tables inside their extents (clone
  `JumpTable`s); the byte table `0x528BA0` is read in place.
- **The labels**: "LeaderPanel_Stage1Steps", "ChoiceMenu_Choices" were right;
  "Boot: field, map and sprites" and the `0x66030C` / `0x660314` units are
  stages 10 and 11; the `hypothesis` row `0x52B480` is the fish spawn.

## 6. Controls

`r1g/controls.py` (scratch): each plant replaces a string that occurs once in
`rest_1g.cpp`, rebuilds, runs the self-test on the clones whose name contains
the filter, restores and rebuilds.
**103 planted, 103 refused** (every one exit 3, by a count of mismatching
rounds; a dispatcher's filter also runs its steps, hence "of" a larger total).
Every function has at least one. The first run (4,000 rounds, before the
per-function seeds of `SeedFor`) left two unrefused: C56 (the rise's bound,
needing a height at `-0x100 L - 0x20` with the level's bits) and C91 (the
strength kept from 1, which differs only for a strength of exactly 0 with a
negative `+0x98`); the seeds now plant both, and `Fish_Heading`'s stand-in logs
the direction it is handed (it had overwritten it, hiding `Fish_Approach`'s
eight ways: C59 went 7 -> 599). **The thinnest**: C56 (1 round), C76 and C77
(9), C15 (11), C71 and C74 (17) - each a single-value boundary behind several
conditions in `Fish_Hooked` / `Fish_Swim` or the rule page's wrap.

| # | Function | Plant | Refused (rounds of 6,000) |
|---|---|---|--:|
| C01 | `LeaderPanel_S1` | the step index + 1 (mod 11) | 6,000 of 84,000 |
| C02 | `LeaderPanel_S1Begin` | the sprite index 0xFE | 5,958 |
| C03 | `LeaderPanel_S1Begin` | record 3's mode 5 for 6 | 2,114 |
| C04 | `LeaderPanel_S1Wait` | record 3 at 7 | 2,010 |
| C05 | `LeaderPanel_S1Buttons` | the mask 0x75 | 35 |
| C06 | `LeaderPanel_S1Buttons` | the warning 2 | 127 |
| C07 | `LeaderPanel_S1Buttons` | stage 11 for 10 | 773 |
| C08 | `LeaderPanel_S1Idle` | record 1 at 2 | 821 |
| C09 | `LeaderPanel_S1Idle` | +9 = 5 | 821 |
| C10 | `LeaderPanel_S1Box3In` | the pointer's upper half dropped | 6,000 |
| C11 | `LeaderPanel_S1Box3In` | the message one lower | 6,000 |
| C12 | `ChoiceMenu_DataPage` | 22 pages | 57 |
| C13 | `ChoiceMenu_DataPage` | the wrap down to 0x15 | 48 |
| C14 | `ChoiceMenu_DataPage` | +0x3E + 1 read from +0x3C | 263 |
| C15 | `ChoiceMenu_RulePage` | seven pages | 11 |
| C16 | `ChoiceMenu_RulePage` | +8 = 0xFE | 85 |
| C17 | `LeaderPanel_S9Back` | record 6 at 1 | 839 |
| C18 | `LeaderPanel_S10` | the step index ^ 1 | 6,000 of 18,000 |
| C19 | `LeaderPanel_S10Look` | the camera 0x7000 nearer | 3,039 |
| C20 | `LeaderPanel_S10Look` | frames with bit 3 | 2,287 |
| C21 | `LeaderPanel_S10Look` | the far end 0xCE00 | 1,505 |
| C22 | `LeaderPanel_S10Look` | a primitive of 0x1C | 1,505 |
| C23 | `LeaderPanel_S10End` | Field_Kind2Hold 1 holds | 1,997 |
| C24 | `LeaderPanel_S11` | the step index + 1 (mod 4) | 6,000 of 30,000 |
| C25 | `LeaderPanel_S11FadeOut` | transition 3 | 2,024 |
| C26 | `LeaderPanel_S11Switch` | Game_Step 4 | 4,026 |
| C27 | `LeaderPanel_S11Switch` | 0x93985F left | 4,004 |
| C28 | `LeaderPanel_S11FadeIn` | pass flags 0x1E | 6,000 |
| C29 | `LeaderPanel_S11End` | the wait word 1 | 4,026 |
| C30 | `LeaderPanel_PoseSound` | pose 2's sound 0x206 | 459 |
| C31 | `LeaderPanel_PoseSound` | no test against the pose before | 2,170 |
| C32 | `LeaderPanel_UseItemEnd` | record 4's +6 = 2 | 6,000 |
| C33 | `LeaderPanel_UseItemEnd` | record 4 read before the calls | 38 |
| C34 | `LeaderPanel_SetRecords` | the 20-byte base 0x1B | 4,859 |
| C35 | `LeaderPanel_SetRecords` | the 10-byte stride 12 | 3,607 |
| C36 | `LeaderPanel_Effect3Mode` | state 5 for 4 | 960 |
| C37 | `LeaderPanel_HoldTest` | +0xA = 7 | 943 |
| C38 | `LeaderPanel_HoldTest` | held bit 0x2000 | 2,022 |
| C39 | `LeaderPanel_LeaveOnPress` | the low byte only | 256 |
| C40 | `LeaderPanel_LeaveOnPress` | stage 9 | 1,901 |
| C41 | `LeaderPanel_EffectsStep` | frames 0xB | 2,023 |
| C42 | `LeaderPanel_EffectsStep` | 4 frames left still steps | 238 |
| C43 | `LeaderPanel_EffectsStep` | +0xA from the record's +0x11 | 64 |
| C44 | `LeaderPanel_EffectsStep` | the depth not carried | 46 |
| C45 | `LeaderPanel_PressLatch` | the mask 0xE000 | 1,472 |
| C46 | `Fish_Spawn` | +0 = 0x20 | 6,000 |
| C47 | `Fish_Spawn` | kind 0x14 not grown | 65 |
| C48 | `Fish_Spawn` | the growth bound M / 8 | 68 |
| C49 | `Fish_Spawn` | the strength / 9 | 6,000 |
| C50 | `Fish_RunAll` | the shade from -0x1F8 | 881 |
| C51 | `Fish_RunAll` | Sprite_Current not re-read after the call | 3,426 |
| C52 | `Fish_Begin` | +0x29 = 6 | 6,000 |
| C53 | `Fish_Begin` | x from 0x15 cells | 6,000 |
| C54 | `Fish_Begin` | a zero height -0x1F | 236 |
| C55 | `Fish_Swim` | a nibble on bits 0..2 clear | 89 |
| C56 | `Fish_Swim` | the rise from 0x21 | 1 |
| C57 | `Fish_Swim` | stop on bit 7 | 503 |
| C58 | `Fish_Settle` | state 0 | 4,007 |
| C59 | `Fish_Approach` | dx < 0, dz > 0 as 5 | 599 |
| C60 | `Fish_Approach` | the level clamped at 2 | 717 |
| C61 | `Fish_Approach` | the leader to stage 5 | 975 |
| C62 | `Fish_Approach` | the double one time in two | 67 |
| C63 | `Fish_Approach` | level 0's turn on 0xC0 | 32 |
| C64 | `Fish_Hooked` | landed with -8 | 506 |
| C65 | `Fish_Hooked` | the jump's frame from 0x14 | 55 |
| C66 | `Fish_Hooked` | the high count from +9 | 122 |
| C67 | `Fish_Hooked` | the break to stage 8 | 865 |
| C68 | `Fish_Hooked` | tier 1 at three quarters | 215 |
| C69 | `Fish_Hooked` | a jump from -0x40 | 43 |
| C70 | `Fish_Hooked` | the jump wears 4 | 123 |
| C71 | `Fish_Hooked` | +0x81 0xFD from -0x2F | 17 |
| C72 | `Fish_Hooked` | the run clamped at 4 | 44 |
| C73 | `Fish_Hooked` | frames by 9 - tier | 1,751 |
| C74 | `Fish_Hooked` | wear every 8th frame | 17 |
| C75 | `Fish_Hooked` | reeled 0x800 | 373 |
| C76 | `Fish_Hooked` | the left bank 0x60001 | 9 |
| C77 | `Fish_Hooked` | direction 2 from 0x800 | 9 |
| C78 | `Fish_Hooked` | the far line 0x3F | 593 |
| C79 | `Fish_Hooked` | +4's direction 4 | 578 |
| C80 | `Fish_Hooked` | the divisor 1 | 1,259 |
| C81 | `Fish_Hooked` | the snap's chance at the low edge | 87 |
| C82 | `Fish_S5` | the step index ^ 1 | 6,000 of 18,000 |
| C83 | `Fish_S5Center` | an arithmetic shift | 3,858 |
| C84 | `Fish_S5Move` | x's low word & 0xC000 | 357 |
| C85 | `Fish_S5Move` | state 2 | 802 |
| C86 | `Fish_S6` | the step index + 1 (mod 3) | 6,000 of 24,000 |
| C87 | `Fish_S6Begin` | only +0x3C's word cleared | 5,242 |
| C88 | `Fish_S6Wait` | the next kind's frame | 796 |
| C89 | `Fish_S6Release` | freed above 8 | 1,242 |
| C90 | `Fish_AdjustStrength` | the delta unsigned | 1,575 |
| C91 | `Fish_AdjustStrength` | kept from 1 | 894 |
| C92 | `Fish_Chance` | odds 9 under | 123 |
| C93 | `Fish_Chance` | a tie wins | 487 |
| C94 | `Fish_Chance` | the band's first odds 2 | 258 |
| C95 | `Fish_Step` | the climb from +0x16 | 2,716 |
| C96 | `Fish_Heading` | direction 7 doubled | 1,001 |
| C97 | `Fish_Heading` | the left turn below 0x80000 | 199 |
| C98 | `Fish_Heading` | small from below 0x400 | 961 |
| C99 | `Fish_Heading` | the top band from 0x1B0000 | 37 |
| D01 | `Fish_LureInReach` | margin + 4 | 3,923 |
| D02 | `Fish_LureInReach` | nibble 5 | 312 |
| D03 | `Fish_LureClose` | x within 0x8000 inclusive | 152 |
| D04 | `Fish_LureClose` | the height within 0x401 | 132 |

## 7. Latent defects and ranges (Capcom's, described, not fixed)

- **The dispatchers are unbounded**: `LeaderPanel_S1` / `_S10` / `_S11` by
  `+3`, `Fish_RunAll` by each record's `+1`, `Fish_S5` / `_S6` by `+2` - the
  original jumps through the dword after each table. Ours aborts with a
  message. Not seen in play.
- **`Fish_Spawn`'s record index is unbounded**: the spot's counts summed
  over the 23 kinds fill records from 0 up; past 30 the original writes
  `0x7E01B8..`. Ours aborts. The shipped rows read for spots 0..15 sum to 17..25.
  Which spot bytes play gives `0x905B88` is not measured.
- **`Fish_Spawn` divides by the kind's top size** (`Fish_Kinds +0x1F`) and
  halves by it in a loop: a kind with 0 faults, with 1 never ends. The 23
  shipped kinds are 20..240; the kind is re-read from `Sprite_Current` after
  `Rand`, so only a `Sprite_Current` moved mid-spawn could meet another. Ours
  aborts on 0.
- **The kind and the direction index image tables unmasked** (`Fish_Kinds`
  by `+6`, `0x660340` / `MoveCmd_F9Steps` by `+8`, `0x660364` by `+6`): read
  in place, as R0A's direction (no fault inside the image).
- **`ChoiceMenu_DataPage` turns from `+0x3E`, writes `+0x3C`**; the rule page
  turns `+0x3C` itself. Reproduced; which is the shown page is the record's
  (E1F's) business.
- **`Fish_S5Center`'s logical shift**: `((v & 0xFFFF8000) - v) >> 3` with
  `shr`, so any fraction gives a step near 0x1FFFF000; eight such steps wrap
  back to the half cell. Reproduced.
- **`Fish_Chance` writes its caller's argument slots** (harmless: never read).
- **`LeaderPanel_SetRecords`** gives a pointer before its table for an item
  byte below 0x1C / 0x2E (nothing read there by it; the readers then read
  whatever lies before). Reproduced.

Nothing here needs a ledger entry: no original read of never-written memory
reaches a draw or a decision (the uninitialised upper bytes of `Fish_Chance`'s
edge and of `LeaderPanel_S10Look`'s vertex pad are never read by the callees).

## 8. Calls across groups, inbound

**Outbound**: none to another group of the round (`band_rows.py --edges`);
every callee is ours or the group's own.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls | How |
|---|---|---|---|
| `0x496440` (game mode 8's entry) | R3G (wave three) | `Fish_Spawn` | E8 |
| `GameMode8_Frame` | E1F (ours) | `Fish_RunAll` | `SH_AT(at::kMode8Panel)`, rebound |
| `LeaderPanel_S3Run`, `_S4Run`, `_S4Music`, `_S5Wait`, `_S6Wait`, `_S7Wait`, `_Leave`, `_UseItem` | E1E (ours) | `PoseSound`, `Effect3Mode`, `HoldTest`, `EffectsStep`, `LeaveOnPress`, `UseItemEnd` | `SH_AT` of `effect_1e_callees.h`, rebound |
| `ChoiceMenu_Run` | E1F | the two pages | `ChoiceMenu_Choices` |
| `Field_LeaderStates[9]` `0x528880` | R1F | the three dispatchers | `LeaderPanel_Stages` |

**Harness rows that list functions of this group by address**
(`scenario_harness.cpp`, the effect-standard rows of round thirteen):
`0x52B2A0` (`{kU8}`), `0x52B1B0`, `0x52B2E0`, `0x52B370`, `0x52B200` (no
arguments), `0x52B330` (`{kAll}`, `kGarbage`), `0x52B6C0`. They key on the
address, and every caller of ours calls by the address (the constants keep
their values), so none breaks. Each matches the reading but `0x52B330`: it
answers al 0 / 1 (the row answers garbage) and reads 16 bits of its argument
(the row compares all 32) - stricter, not wrong; for the coordinator's fold.
`effect_1e_fuzz.cpp`'s six `E_RAW` rows likewise key on the address and stay.

## 9. The live route

The catalogue's reach columns are empty for all 45. HANDOFF (the catch
route's reach, 2026-10-03) records that the owner's
`tools/recipes/campingFishing.txt` and `caughFish.txt` enter "the same 16
fishing functions" in `0x52B1B0..0x52CCD0` (the catalogue's listed starts
there; the fish states hidden in `0x52B6C0`'s extent run under it); no trace
file of either is under `analysis/calltrace`. The stages 1, 10, 11 and the
two pages: no trace names them. Here: fuzz only. **The coordinator's check**
is `caughFish.txt`'s `randlog` (the `Rand` count per frame must not move: ours
calls `Rand` exactly where the original does, in its order) and the frame
hash / state hash on both routes.

## 10. The rebinding

`band_rows.py --refs --group R1G` and `grep -rn -i` of the 45 over
`src/game`: 55 raw references to 12 functions.

| File | Change |
|---|---|
| `effect_1e_callees.h` | `kPoseSound`, `kUseItemEnd`, `kEffect3Mode`, `kHoldTest`, `kEffectsStep`, `kLeaveOnPress` = `bof3::addr::LeaderPanel_...` (values unchanged) |
| `effect_1f_callees.h` | `kMode8Panel = bof3::addr::Fish_RunAll` (value unchanged; the header now includes `symbols.gen.h`) |

**Left raw, on purpose**: `effect_1e_fuzz.cpp`'s `CallSite` tables and its six
`E_RAW` callee rows, `effect_1f_fuzz.cpp`'s `kCalls52CF00` (the disassembly's
targets; the rows key on the address); `scenario_harness.cpp`'s seven
`FX_RAW` rows (a harness: not this group's to edit; section 8); comments in
`effect_1e.cpp`, `field_e1.cpp`, `effect_1e_callees.h` describing the
round-thirteen state. No file of another group of this round refers to the
45.

## 11. For `analysis/calltrace/entries_logic.txt`

32 lines appended to the main checkout's file (2026-10-04): the 30 starts it
lacked and the smaller extents `0052B1B0 48` and `0052B6C0 84` beside the
catalogue's host lines (`92`, `12C2`). The other 13 were already listed with
the extents read here.
