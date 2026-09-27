# Group E: the engine-side rows of the effect table

**Status:** IN PROGRESS (2026-09-26) - 22 functions ours
(`src/game/magic_engine.cpp`, shadow name `magic_engine`), fuzzed headless
through the shared harness ([`magic_harness.md`](magic_harness.md)): 0
mismatches in 44,000 rounds; 100 of 102 negative controls refused by a count or a Fatal (exit 3), the two others equivalent mutants (section 5). Fuzz only: no recorded route
casts any of these rows (section 7). Both of TCRF's reports are kept as
Capcom wrote them: Paralyzer's unchecked read (a crash, reachable by
reading) and Head Cracker's unbounded waits (section 6 - by reading, the
target-state wait ends for ordinary targets; the reported freeze is not
explained).

Group E of round nine's second spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §3,
§4). No `DIVERGENCE.md` entry: every function is a faithful replacement,
except that a phase past a stack table aborts where the original calls
through its own stack (the precedent, [`magic_fx_reached.md`](magic_fx_reached.md) §3).

## 1. The rows and their functions

Five rows of `Magic_Rows` (`0x64C2B8`) have file `0xFFFF` and a handler in
the battle engine, outside the BMAGIC band, so `tools/magic_rows.py --unit`
does not cover them. Names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2), TCRF's where it names the id - all
hypotheses about what the player sees.

| Row(s) | Handler | Id (one down) | What it does, read |
|---|---|---|---|
| 0, 126 | `MagicHold_Task` `0x4378D0` | row 126: `0x7F`, TCRF's Assault | a 60-frame hold, then flag `0x40` on the target and the effect done |
| 108 | `RestoreForm_Task` `0x4525F0` | `0xD9`, Restore Form | the acting member put in state 6, sub-states 4 / 4; done when no actor is pending |
| 123 | `Paralyzer_Task` `0x43F3B0` | `0x8B`, TCRF's Paralyzer | the current enemy's cue, the caster's animation 2 to its end, flag `0x40` on the target |
| 128 | `HeadCracker_Task` `0x43FC80` | `0x80`, TCRF's Head Cracker | the caster's animation 2, sound `0x601`, then three rocks dropped on the target, each followed by a wait for the target to leave state 6 |

Every extent was read to its last instruction (capstone recursive descent,
every jump internal, no jump table; `tools/magic_rows.py`'s `clone_sites`
run over the same extents for the call sites and immediates). The queue's
list was right: **22 functions, 1,357 bytes** (the queue's "about 1,500"),
none found inside or missing.

| Function | Entry | Bytes | Role | Shared with |
|---|---|--:|---|---|
| `MagicHold_Task` | `0x4378D0` | 0x26 | rows 0 / 126: +1 through {`0x47FDA0`, `0x437900`} | |
| `Task_StartHold60` | `0x47FDA0` | 0x12 | +9 = 60, on | entry 0 of the `.data` table `0x654904` that `0x47FD80` (no group's) jumps through |
| `MagicHold_Countdown` | `0x437900` | 0x2D | +9 down; at 0 flag 40, done, free | |
| `RestoreForm_Task` | `0x4525F0` | 0x26 | row 108: +1 through {`0x452620`, `0x452660`} | |
| `RestoreForm_Start` | `0x452620` | 0x3B | the actor's party record +1..+4 = 6, 4, 4, 0 | |
| `RestoreForm_Wait` | `0x452660` | 0x17 | the pending word `0x904B82` 0: done, free | |
| `Paralyzer_Task` | `0x43F3B0` | 0x2E | row 123: +1 through three | |
| `Paralyzer_Start` | `0x43F3E0` | 0x4B | the cue through `0x939AD8` +0xF8, animation 2 on the caster | |
| `MagicFx_WaitOwnerAnim` | `0x43F430` | 0x2C | the caster's script until it reports its end | MAGIC081's stack table (`0x4BF8EE`) |
| `MagicFx_FlagTargetEnd` | `0x43F460` | 0x1A | flag 40 on the target, done, free | thirteen stack-table sites: MAGIC010, 015 (3), 018/019 (4), 020, 117 (2), 145, 146 |
| `HeadCracker_Task` | `0x43FC80` | 0x58 | row 128: +1 through ten | |
| `HeadCracker_Start` | `0x43FCE0` | 0x3E | animation 2 on the caster, +0xC = 8 | |
| `HeadCracker_Windup` | `0x43FD20` | 0x53 | +0xC down, at 0 sound `0x601`; the caster's script ticked | |
| `HeadCracker_WaitCaster` | `0x43FD80` | 0x35 | the caster's script until its end; +0xA = 0 | |
| `HeadCracker_Drop` | `0x43FDC0` | 0x3E | entries 3, 5, 7: a rock (kind 1, `0x5D`), +0xA up | |
| `HeadCracker_WaitTarget` | `0x43FE00` | 0x75 | entries 4, 6, 8: the rocks down, then the target out (to 9) or off state 6 | |
| `MagicFx_DoneAndFree` | `0x43FE80` | 0xC | entry 9: done, free | 21 more stack-table sites: `0x43F61A` (engine), MAGIC001 (2), 002, 015 (3), 018/019 (4), 020, 021, 054, 056, 103 (Simoon), 110 (Ragnarok), 111, 150, 161, 169 |
| `HeadCrackerRock_Task` | `0x43FE90` | 0x1A | `BattleMagicFx_Dispatch` entry `0x5D`: +1 through one | |
| `HeadCrackerRock_Run` | `0x43FEB0` | 0x43 | +2 through three; then the sprite ticked and placed | |
| `HeadCrackerRock_Start` | `0x43FF00` | 0x110 | bank `0x1D3` at the target's x / z, 0x800 up, falling | |
| `HeadCrackerRock_Fall` | `0x440010` | 0x4C | the fall, until below the ground + 0x180 | |
| `HeadCrackerRock_Land` | `0x440060` | 0x1B | flag 40 on the target, the parent's +0xA down, free | |

**`0x43FE80`, the first wave's raw address.** S23 (`kEnginePhase`) and S25
(`kDoneAndFree`) call it by address through `Phase()`, as their stack
tables hold it; it is `MagicFx_DoneAndFree` now: the effect-done bit
(`0x904AA8 |= 4`) and `BattleTask_FreeCurrent`, nothing else. Their
`Phase(0x43FE80)` keeps working unchanged (it reaches the `jmp` to ours);
naming the constant after the merge is theirs to do, not needed.

The three `MagicFx_*` and `Task_StartHold60` are shared bodies (the queue's
§1): taking them takes them for every table that holds them. None is ever
an `E8` / `E9` target (a scan of `.text`), only a stack-table or `.data`
entry, so the harness's address-keyed phases cover every caller.

## 2. What each does

The full account is above each function in `magic_engine.cpp` and in its
`symbols.toml` evidence. In short:

- **The dispatchers** (`MagicHold_Task`, `RestoreForm_Task`,
  `Paralyzer_Task`, `HeadCracker_Task`, `HeadCrackerRock_Task` by +1,
  `HeadCrackerRock_Run` by +2): each builds its table on its stack and
  calls the entry, unchecked. `HeadCracker_Task` loads `HeadCracker_Drop`
  and `HeadCracker_WaitTarget` into `ecx` / `eax` (`mov r32, imm32` at +0x4
  and +0x9) and stores each three times - two immediates the band tool's
  `[esp + k]` pattern does not see, added to the clone table by hand.
- **The caster.** Paralyzer and Head Cracker switch `Sprite_Current` to the
  running slot's owner (`[0x93B8C4] + 0x80`, the actor whose command
  started the effect) around each sprite call, and back.
- **Row 108** writes the acting actor's party record directly: state 6 with
  sub-states 4 and 4, which the member's state-6 handler `0x441A10` runs as
  entry 4 of `0x64E07C` (`0x442080`, not read here). The actor indexes the
  party records unchecked.
- **The rock** takes animation bank `0x1D3`, copies the target object's x
  (+0x34) and z (+0x38), rises 0x800 from **whatever height word +0x3E the
  slot held** (not reset by `BattleTask_Create`), and falls with a speed
  that grows by 16 a frame until it is below `AreaMap_Elevation(x, z)` +
  0x180 (the low word, signed); then flag 0x40 on the target and the
  parent's rock count down.

## 3. Calls to other units

| Address | What | Owner | How ours calls it |
|---|---|---|---|
| `0x437450` | `Sound_PlayEffect(id)` unless `id` is `0xFFFF` | nobody's (named by no group; `battle_items_callees.h`, `enemy_ai_ops_callees.h` call it by address) | `MH_AT`, a local constant |

Everything else is ours already and called by name: `BattleTask_Create`,
`BattleTask_FreeCurrent`, `Battle_SetTargetFlag40`, `Battle_ActorIsOut`,
`Sound_PlayEffect`, `Sprite_SetAnimation`, `Sprite_SetAnimationBank`,
`Sprite_ScriptTickOnce`, `Sprite_ScriptTick`, `Sprite_UpdateScreen`,
`AreaMap_Elevation`. No call into another second-wave group's unit.

No `[[data]]` entry: the group's tables are all built on the stack, and the
two `.data` cells it reads (`0x939AD8` the current enemy, `0x904B82` the
pending word) are other modules' (left as local constants, so no name
collides in the merge).

## 4. The fuzz

`BOF3X_SHADOW=magic_engine` (`src/game/magic_engine_fuzz.cpp`): the clone
table built by hand (above), 2,000 rounds per function.

- **Callees beyond the standard set.** The five sprite callees are custom
  stand-ins that log `Sprite_Current` with their argument - they act on the
  current sprite, and these functions switch it to the caster and back
  around them, so a switch missed shows (controls P4, W3, U5).
  `Sprite_ScriptTickOnce` / `Sprite_ScriptTick` answer a flag (0 a third of
  the time) from the recorders' salted stream. `0x437450` is logged as its
  low word. `AreaMap_Elevation` answers, a third of the time, a ground one
  above, at or one below the current rock's height - 0x180 (the landing
  bound). `Battle_ActorIsOut` answers from its own stream and moves the
  target a quarter of the time (next paragraph).
- **Regions beyond the standard ones**: `0x939AD8` (4 bytes), `0x904B82`
  (2), and a 16-byte cue table of the fuzz's own. Every round the current
  enemy points at an enemy record or one of the harness's sprite records,
  whose +0xF8 points into the cue table.
- **Seeds**: each dispatcher's phase inside its table (+1 of 0..9 for Head
  Cracker, 0 for the rock's one-entry table); the hold counter at 0, 1, 2
  and 0xFF; the pending word 0 or a single bit; cue words whose +4 is
  0xFFFF, wraps, or is 0; the wind-up counter at 1, 0, 2, 0x80000000, -1,
  0x10000; the rock count 0 two in three; every actor's state 6 half the
  time; the rock's target at 0..4 and 10, its height near the sign bit.
- **A harness blind spot, worked round in the group's file** (no harness
  edit): a `kFlag` recorder's answer and its disturbance come from the same
  hash, so it answers 0 exactly when it disturbed nothing
  (`magic_harness.cpp`: `Disturb` returns at `h % 3 == 0`, `Answering`
  gives 0 at `h % 3 == 0`). A caller that re-reads a cell after a "no"
  answer never meets a moved cell: control T3 (the target not re-read after
  `Battle_ActorIsOut`) was **not refused** in the first run. The group's
  `effect` on `Battle_ActorIsOut` answers from `Noise()` instead and moves
  the target itself; T3 is now refused. Other groups' `kFlag` callees read
  again after a 0 answer have the same blind spot - for the coordinator.

In this worktree: 44,000 rounds over 22 functions, **47,832** calls to the
stand-ins, **0 mismatches**, 11,382 bytes of state (11 regions). Coverage
(calls the originals made): Sprite_SetAnimation 6000,
Sprite_SetAnimationBank 2000, Sprite_ScriptTickOnce 6000, Sprite_ScriptTick
983, Sprite_UpdateScreen 983, Battle_ActorIsOut 1354, AreaMap_Elevation
2000, 0x437450 2000, BattleTask_Create 2000, BattleTask_FreeCurrent 7553,
Battle_SetTargetFlag40 4581, Sound_PlayEffect 378, and every phase of the
six tables (`0x43FEB0` 2000, the others 197..1014). `BOF3X_SHADOW='*'`:
see section 8.

## 5. Controls

Planted one at a time in `magic_engine.cpp` by a script that plants
(anchored on a string that must occur exactly once), rebuilds, runs the
self-test, restores and rebuilds; the baseline passed after the last
restore. Counts are rounds of 2,000 for the function, in this worktree.

| Id | Planted | Result |
|---|---|---|
| H1 | MagicHold_Task: the table swapped | refused, 2000 rounds |
| H2 | Task_StartHold60: 59 frames | refused, 2000 rounds |
| H3 | Task_StartHold60: +2 on, not +1 | refused, 2000 rounds |
| H4 | MagicHold_Countdown: fires at 1 | refused, 282 rounds |
| H5 | MagicHold_Countdown: down by 2 | refused, 1419 rounds |
| H6 | MagicHold_Countdown: the actor flagged, not the target | refused, 519 rounds |
| H7 | MagicHold_Countdown: no free | refused, 581 rounds |
| D1 | SetDone: bit 3, not 2 | refused, 3994 rounds |
| R1 | RestoreForm_Task: the table swapped | refused, 2000 rounds |
| R2 | RestoreForm_Start: state 5 | refused, 2000 rounds |
| R3 | RestoreForm_Start: +2 = 3 | refused, 2000 rounds |
| R4 | RestoreForm_Start: +3 = 3 | refused, 2000 rounds |
| R5 | RestoreForm_Start: +4 = 1 | refused, 2000 rounds |
| R6 | RestoreForm_Start: the target member, not the actor | refused, 1616 rounds |
| R7 | RestoreForm_Start: +2 on, not +1 | refused, 2000 rounds |
| R8 | RestoreForm_Wait: the pending word low byte only | refused, 370 rounds |
| R9 | RestoreForm_Wait: no done bit | refused, 461 rounds |
| P1 | Paralyzer_Task: entries 1 and 2 swapped | refused, 1309 rounds |
| P2 | Paralyzer_Start: cue + 3 | refused, 2000 rounds |
| P3 | Paralyzer_Start: the cue masked to a byte | refused, 1411 rounds |
| P4 | Paralyzer_Start: the caster not made current | refused, 1694 rounds |
| P5 | Paralyzer_Start: animation 3 | refused, 2000 rounds |
| P6 | Paralyzer_Start: Sprite_Current not put back | refused, 1727 rounds |
| P7 | Paralyzer_Start: the second cue | refused, 1925 rounds |
| P8 | Paralyzer_Start: the two calls swapped | refused, 2000 rounds |
| W1 | WaitOwnerAnim: +2 on | refused, 1345 rounds |
| W2 | WaitOwnerAnim: the answer inverted | refused, 2000 rounds |
| W3 | WaitOwnerAnim: the caster not made current | refused, 1750 rounds |
| W4 | WaitOwnerAnim: Sprite_Current re-read for the phase | refused, 1178 rounds |
| F1 | FlagTargetEnd: the actor flagged | refused, 1811 rounds |
| F2 | FlagTargetEnd: no free | refused, 2000 rounds |
| K1 | HeadCracker_Task: entry 9 WaitTarget | refused, 201 rounds |
| K2 | HeadCracker_Task: Start and Windup swapped | refused, 461 rounds |
| K3 | HeadCracker_Task: a nine-entry table | Fatal (exit 3): HeadCracker_Task: phase 9, past the 9-entry table |
| K4 | HeadCracker_Task: entry 5 WaitTarget (the second rock skipped) | refused, 198 rounds |
| S1 | HeadCracker_Start: +0xC = 7 | refused, 2000 rounds |
| S2 | HeadCracker_Start: animation 1 | refused, 2000 rounds |
| S3 | HeadCracker_Start: the phase on through the saved slot (equivalent: Sprite_Current was just put back to it) | **NOT refused** |
| S4 | HeadCracker_Start: the phase on through the caster (S3's near variant) | refused, 1767 rounds |
| U1 | Windup: down by 2 | refused, 2000 rounds |
| U2 | Windup: fires at or below 0 | refused, 730 rounds |
| U3 | Windup: sound 0x600 | refused, 378 rounds |
| U4 | Windup: Sprite_Current not re-read after the sound | refused, 10 rounds |
| U5 | Windup: the tick not on the caster | refused, 1752 rounds |
| U6 | Windup: the tick only while counting | refused, 368 rounds |
| C1 | WaitCaster: +0xA = 1 | refused, 1316 rounds |
| C2 | WaitCaster: +2 on | refused, 1316 rounds |
| C3 | WaitCaster: the answer inverted | refused, 2000 rounds |
| C4 | WaitCaster: Sprite_Current not put back | refused, 1724 rounds |
| Dr1 | Drop: parameter 0x5C | refused, 2000 rounds |
| Dr2 | Drop: kind 3 | refused, 2000 rounds |
| Dr3 | Drop: the rock owned by the running slot 0x93B8C4 | refused, 801 rounds |
| Dr4 | Drop: a stride of 0x80 | refused, 1953 rounds |
| Dr5 | Drop: no child counted | refused, 2000 rounds |
| Dr6 | Drop: Sprite_Current read before the call | refused, 68 rounds |
| T1 | WaitTarget: +0xB, not +0xA | refused, 1353 rounds |
| T2 | WaitTarget: out goes to 8 | refused, 456 rounds |
| T3 | WaitTarget: the target not re-read after ActorIsOut | refused, 102 rounds |
| T4 | WaitTarget: 3 as a member | refused, 37 rounds |
| T5 | WaitTarget: waits on 6 and above | refused, 345 rounds |
| T6 | WaitTarget: the next enemy | refused, 317 rounds |
| T7 | WaitTarget: the out test inverted | refused, 1354 rounds |
| T8 | WaitTarget: state byte +2 | refused, 435 rounds |
| T9 | kReacting 7 (WaitTarget and RestoreForm_Start) | refused, 2478 rounds |
| X1 | DoneAndFree: no done bit | refused, 952 rounds |
| RT1 | Rock_Task: by +2 | Fatal (exit 3): HeadCrackerRock_Task: phase 2, past the 1-entry table |
| RR1 | Rock_Run: Fall and Land swapped | refused, 1342 rounds |
| RR2 | Rock_Run: bit 1, not 0 | refused, 1032 rounds |
| RR3 | Rock_Run: UpdateScreen before ScriptTick | refused, 983 rounds |
| RR4 | Rock_Run: by +1 | refused, 1330 rounds |
| RR5 | Rock_Run: Sprite_Current tested before the dispatch | refused, 29 rounds |
| RS1 | Rock_Start: bank 0x1D2 | refused, 2000 rounds |
| RS2 | Rock_Start: 2 as an enemy | refused, 486 rounds |
| RS3 | Rock_Start: a member's +0x30 | refused, 943 rounds |
| RS4 | Rock_Start: an enemy's +0x3C | refused, 1057 rounds |
| RS5 | Rock_Start: up 0x400 | refused, 2000 rounds |
| RS6 | Rock_Start: step -15 | refused, 2000 rounds |
| RS7 | Rock_Start: +0x5C kept | refused, 1994 rounds |
| RS8 | Rock_Start: +0x2A kept | refused, 1994 rounds |
| RS9 | Rock_Start: +0x29 = 1 | refused, 2000 rounds |
| RS10 | Rock_Start: animation 2 | refused, 2000 rounds |
| RS11 | Rock_Start: Sprite_Current not re-read after the animation | refused, 58 rounds |
| RS12 | Rock_Start: speed 1 | refused, 2000 rounds |
| RS13 | Rock_Start: +0x48 kept | refused, 1989 rounds |
| RS14 | Rock_Start: +0x5E kept | refused, 1988 rounds |
| RS15 | Rock_Start: Sprite_Current read before the bank call | refused, 52 rounds |
| RS16 | Rock_Start: the target not re-read for +0x38 (equivalent: no call between the reads) | **NOT refused** |
| RS17 | Rock_Start: the target read before the bank call (RS16's near variant) | refused, 79 rounds |
| RF1 | Rock_Fall: the speed's high word | refused, 2000 rounds |
| RF2 | Rock_Fall: the step subtracted | refused, 2000 rounds |
| RF3 | Rock_Fall: + 0x17F | refused, 154 rounds |
| RF4 | Rock_Fall: at or below | refused, 175 rounds |
| RF5 | Rock_Fall: the whole eax | refused, 991 rounds |
| RF6 | Rock_Fall: x and z swapped | refused, 2000 rounds |
| RF7 | Rock_Fall: Sprite_Current not re-read | refused, 46 rounds |
| RF8 | Rock_Fall: the speed stepped first | refused, 2000 rounds |
| RF9 | Rock_Fall: unsigned height | refused, 606 rounds |
| RL1 | Rock_Land: owner +0xA down 2 | refused, 1983 rounds |
| RL2 | Rock_Land: owner read before the call | refused, 71 rounds |
| RL3 | Rock_Land: the running slot's owner cell | refused, 1634 rounds |
| RL4 | Rock_Land: no free | refused, 2000 rounds |
| SO1 | SlotOwner: Sprite_Current's +0x80, not 0x93B8C4's | refused, 3051 rounds |

**Not refused, both equivalent** (no input can tell them apart):

- **S3**: `HeadCracker_Start` bumps +1 through the saved slot instead of
  `Sprite_Current` read again - the line before put `Sprite_Current` back
  to that slot, with no call between. Its near variant S4 (the caster's +1)
  is refused.
- **RS16**: `HeadCrackerRock_Start` reads the target once for x and z
  instead of again for z - no call between the two reads. Its near variant
  RS17 (the target read before the bank call) is refused.

**Re-run 2026-09-26 on the kFlag-fixed harness
([`magic_harness.md`](magic_harness.md) section 8): 9 controls in the
affected functions, 9 refused.** `HeadCracker_WaitTarget` is this group's
one clone that reaches a `kFlag` / `kBool` answer (`Battle_ActorIsOut`);
T1..T9 are every control planted in it (T9, `kReacting` 7, also touches
`RestoreForm_Start`), rebuilt from the table. Each was refused in exactly the
rounds it was before - the group's `effect` on `Battle_ActorIsOut` already
answered from `Noise()`, not the harness's draw: the thinnest, **T4** at 37
and **T3** at 102. Skipped: K1 and K4 (plants in `HeadCracker_Task`'s table,
whose clone calls the phases' recorders, not `WaitTarget`). Clean self-test
after the restore: 0 mismatches, exit 0. No fuzz change.

## 6. Latent defects (Capcom's, kept)

Numbered D89 (E3), D90, D96 (E4), D102 (E1), D103 (E2, E6) and D124 (E5) in [`known-defects.md`](known-defects.md).

Described, not fixed: a fix is a divergence for the owner to choose. Not
numbered in `known-defects.md` (the coordinator's).

**E1 - Paralyzer reads the current enemy's cue table unchecked (TCRF: "crashes
the game").** `Paralyzer_Start` reads `word [[0x939AD8] + 0xF8]`. By reading:

- `0x939AD8` is the enemy object `BattleEnemy_RunAll` ran last (the highest
  live enemy slot; the task runs after it in the battle frame), not the
  caster.
- **Nothing but the event-battle set-ups writes an enemy's +0xF8**: a
  byte-pattern scan of `.text` for every `mov dword [r32 + 0xF8], ...`
  (decoded to confirm) finds 62 stores, all immediates of cue tables
  (`0x64C7A0..0x64DCD4`) at `0x437A55..0x4400D0`, plus four in the CRT;
  every other reference to `0x93BA58` (enemy 0's +0xF8) is a read
  (`0x4FC09E`, the library's sound, behind the event test). The ordinary
  enemy set-up `0x494570` copies +0..+0x7F and `0x4946C0` fills fields of
  +0x80..+0x125 from `0x8C55C8..`, never +0xF8.
- The enemy records lie in `.data`'s zero-filled tail (the raw data ends at
  `0x676000`), so +0xF8 is 0 at start.

So in a session where no event battle has yet written that enemy slot, the
first Paralyzer reads a word at address 0: an access violation, **the
crash**. After an event battle has written the slot, the pointer stays
(nothing clears it) and Paralyzer plays that boss's cue instead. Ours reads
through the same pointer the same way, unchecked, so it faults where
Capcom's does. Not measured: a block clear through a pointer computed
elsewhere would not show in these scans; the check is one read of
`0x93BA58` (+ 0x128 n) in an ordinary battle, or the owner casting it.

**E2 - Head Cracker's waits have no limit (TCRF: "freezes the game").**
Three waits, none bounded:

1. `HeadCracker_WaitCaster`: the caster's animation 2 until
   `Sprite_ScriptTickOnce` answers 1. By `sprite_anim.cpp` it answers 1 at
   the script's end *or at any jump* (a loop), so it ends for any script
   with an end or a loop. Not a freeze by reading.
2. `HeadCracker_WaitTarget`, the rocks: +0xA counts rocks created and not
   landed. A `BattleTask_Create` with no free slot answers `0xFF`, which
   `HeadCracker_Drop` does not test: it writes the owner at `0x9423FC`,
   past the image's end (`0x93F000`), and counts a rock that never lands -
   **a freeze** (if the write does not fault first). Needs all 48 task
   slots taken.
3. `HeadCracker_WaitTarget`, the reaction: while the target's +1 is 6. **Who
   moves it off 6**, read for this doc:
   - `BattleAction_EffectWait` (`0x42FC60`, ours) runs while the effect
     does and sets +1 = 6, +2..+4 = 0 on every actor flagged `0x40` whose
     +1 is not 6 - which each rock's landing flags.
   - **An enemy** in state 0 (`+0x100` = 0; `EnemyOp_Steps` entry 6):
     `EnemyOp_ActBegin` (+2 = 1), then `EnemyOp_HitDispatch` by +3:
     `EnemyOp_ReceiveAction`, `EnemyOp_WaitAnimOnce`, `EnemyOp_HitEnd` -
     which clears `+0x110` bits 4..6 and 9 and sets the steps to 2, 0, 0:
     **off 6**. A miss goes by +2 = 3 through `0x436BC0`'s table, whose
     last entry is `EnemyOp_HitEnd` again. A kill sets status `0x4000`,
     which `Battle_ActorIsOut` reports, so the wait takes its out branch
     first ([`enemy_ai_ops.md`](enemy_ai_ops.md) §3).
   - **A member**: state 6 is `0x441A10` by +2 through `0x64E07C` (entry 0
     `0x441A30`: +2 = 1; entry 1 `0x441A70` by +3 through `0x64E094`:
     `0x441A90` the action received, ..., `0x441D80` the end, which sets
     +1 = 2 at `0x441E8E` - off 6 - or +1 = 6 for a member now out, whom
     `Battle_ActorIsOut` reports).

   So **by reading, the reaction wait ends** for a state-0 enemy and for a
   member. It does not for: an enemy in another state (`+0x100` not 0: the
   boss and event tables `EnemyOp_StepsB..F` and the hooks `0x437CC0`
   installs - B and C reuse the same hit sequence, D..F were not read); or
   a target byte with a side bit (`0x40` / `0x80`), which
   `HeadCracker_WaitTarget` and `Battle_ActorIsOut` index as an enemy past
   the eight records (`0x93B961 + 0x128 × 0x3D` for `0x40`, past the
   image) - whatever lies there decides, or it faults.

**TCRF's freeze is therefore not explained by this reading** for its case
(the skill hacked into a player's list, cast on an ordinary enemy: a member
caster, a state-0 target). The remaining candidates are the side-bit target
(if the player's list makes the ability target a side) and an enemy in a
non-zero state. The check is live: cast it with the cheat
([`cheats.md`](cheats.md), DIV-0045) with a watch on `0x904B44`, the
caster's and target's +1, and the task's +1 / +0xA.

**Smaller ones, kept:**

- **E3** - every dispatcher's phase is unchecked (the originals call
  through their own stack past the table; ours aborts - the precedent).
  `HeadCrackerRock_Task` has a one-entry table by +1.
- **E4** - `RestoreForm_Start` indexes the party records by the acting
  actor unchecked: an enemy actor (3..10) writes +1..+4 of whatever lies
  past the three members (inside the image, so no fault).
- **E5** - the rock starts 0x800 above the height word its slot's previous
  tenant left (`BattleTask_Create` does not clear +0x3E).
- **E6** - `HeadCrackerRock_Start` and `HeadCracker_WaitTarget` index the
  target's object unchecked (the side bits, as E2).

## 7. What nothing reached

No recorded route casts any of these rows (the queue's §5); nothing here
has run in the game under ours. The live checks are the owner's: Assault
(row 126) and Restore Form (108) with a save that has them or DIV-0045's
cheat; Paralyzer and Head Cracker only with the cheat, and Paralyzer
**crashes the original** by the reading above - ours reproduces it. The
fuzz never feeds a null cue pointer or a side-bit target (both fault on
both sides alike).

## 8. Self-tests and the ledger

- `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_engine`: exit 0.
- `BOF3X_SHADOW='*'`: exit 0, every group's self-test line 0 mismatches, no
  Fatal (2026-09-26, this worktree).
- `python tools/ledger_check.py`: 0 errors.
- `analysis/calltrace/entries_logic.txt` (main checkout): 22 lines
  appended, each function's own extent; `0x47FBE0 718` and `0x4525B0 637`
  are host lines covering `0x47FDA0` and `0x4525F0..0x452676`, to be cut by
  the consolidation.
