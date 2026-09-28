# Group BSI: fights 37, 38, 40, 42, 44, 45, 49, 50, 51 and 53, kinds 45, 47, 49, 51, 52, 56, 57, 58 and 60, and effect slot 7

**Status:** MEASURED (2026-09-28) - round eleven
([`takeover-queue-bosses.md`](takeover-queue-bosses.md)), wave two. 50
functions of `0x43ECC0..0x43F79B` ours (`src/game/boss_si.cpp`, shadow
`boss_si`), each read to its last instruction with capstone and fuzzed
through the boss harness ([`boss_harness.md`](boss_harness.md)) without
edits to it: twenty `Run`s, 0 mismatches; 138 controls planted, 135 refused by a count, 3 equivalent with their near variants refused (section 4). Fuzz only: no
recorded route reaches a boss fight.

The group's units (`tools/boss_rows.py --groups`, the cut of 2026-09-28):
`B37`, `K45`, `K51`, `B38`, `B44`, `K47`, `K60`, `B40`, `B53`, `K49`, `B42`,
`K52`, `K56`, `B45`, `B49`, `K57`, `B50`, `K58`, `B51`, `F7`. The group column
of `analysis/boss_funcs.tsv` listed 51; the 51st, `MagicFx_DoneAndFree`
`0x43FE80` (round nine's, `magic_engine.cpp`), was already ours and is
reached here as entry 3 of the Arwan task's stack table (`Phase(0x43FE80)`).
All 50 were read; none was found inside another's extent or missing from
the list, and every extent the tool gave was the code's. The gap
`0x43F3A2..0x43F480` between `B50` and `K58` is round nine's engine rows
(`0x43F3B0`, Paralyzer, and its neighbours), not the group's.

**The tool rewrites the main checkout's `analysis/boss_funcs.tsv`.** Running
`tools/boss_rows.py --unit ... --clones` with `--analysis` at the main
checkout (as the brief says) regenerated `boss_funcs.tsv` and
`boss_rows.tsv` from this worktree's `symbols.toml` (wave one's functions
ours), and its cut of what is left to take renames the groups: after the run
`0x43ECC0` reads group BSC, `0x43F5F0` BSD. The group list above is the one
read before the run (the brief's line agrees: 51, one ours). The coordinator
may want the cut frozen, or the tool pointed at a copy of `analysis/`.

The enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's
area records), not memory of the game:

| Unit | Root | The disc's | Functions |
|---|---|---|--:|
| `B37` | `Boss_SetupTable[37]` `0x43ECC0` | `BOSS037`, area 144 row 7 (kind 44, Elder - group BSH's) | 2 |
| `K45` | `BossKind_Table[45]` `0x43ED00` | Ammonite, area 134 | 3 |
| `K51` | `BossKind_Table[51]` `0x43EDC0` | Sample 6, area 162 | 3 |
| `B38` | `Boss_SetupTable[38]` `0x43EE50` | `BOSS038` row 7, area 134 (Ammonite) | 2 |
| `B44` | `Boss_SetupTable[44]` `0x43EF00` | `BOSS038` row 6, area 162 (Sample 6) | 1 |
| `K47` | `BossKind_Table[47]` `0x43EF20` | Sample 2, area 159 | 3 |
| `K60` | `BossKind_Table[60]` `0x43EFB0` | HugeSlug, area 119 | 3 |
| `B40` | `Boss_SetupTable[40]` `0x43F040` | `BOSS040` row 7, areas 119 and 159 | 1 |
| `B53` | `Boss_SetupTable[53]` `0x43F060` | `BOSS040` row 7, areas 119 and 159 | 2 |
| `K49` | `BossKind_Table[49]` `0x43F0A0` | Sample 4, area 161 | 3 |
| `B42` | `Boss_SetupTable[42]` `0x43F130` | `BOSS042` row 7, area 161 | 1 |
| `K52` | `BossKind_Table[52]` `0x43F150` | Sample 7, area 163 | 3 |
| `K56` | `BossKind_Table[56]` `0x43F1E0` | Manmo, area 196 | 3 |
| `B45` | `Boss_SetupTable[45]` `0x43F270` | `BOSS049` row 7, areas 163 and 196 | 1 |
| `B49` | `Boss_SetupTable[49]` `0x43F290` | `BOSS049` row 7, areas 163 and 196 | 2 |
| `K57` | `BossKind_Table[57]` `0x43F2D0` | Chimera, area 197 | 3 |
| `B50` | `Boss_SetupTable[50]` `0x43F360` | `BOSS050` row 7, area 197 | 2 |
| `K58` | `BossKind_Table[58]` `0x43F480` | Arwan, area 142 | 6 |
| `B51` | `Boss_SetupTable[51]` `0x43F5B0` | `BOSS051` row 7, area 142 | 2 |
| `F7` | `BattleBossFx_Dispatch` slot 7 `0x43F5F0` | Arwan's effect task (below) | 4 (+ `MagicFx_DoneAndFree`) |

**Which of fights 40 / 53 and 45 / 49 is which area is inferred, not
measured.** The tool puts both of each pair on the same row of two areas.
None of the group's code reads `0x904AAA`. What the code does say: set-ups
40, 42, 44 and 45 are four copies of one body that stores BH's
`BossHook_EndPickWay` as the end hook - the shape of set-ups 39 and 48, and
ids 39..48 are chapter 15's register-passed ids (the plan's section 7,
kinds 46..55 Sample 1..12); set-ups 53 and 49 have end hooks of their own
that set the chapter's step. So 40 is likeliest Sample 2's (area 159) and
53 HugeSlug's (area 119), 45 Sample 7's (area 163) and 49 Manmo's (area
196); the owner or a reading of the chapter's call sites
(`Field_StartEventBattle`'s pages: 53 from `0x555000`, 49 from `0x565000`)
settles it. Set-up 44 is `BOSS038` row 6 of area 162 by the tool, Sample 6's.

## 1. What each function does

`symbols.toml` gives each to the instruction.

### The nine kinds

Eight of the nine are the plain shape round seven's generic enemy gives a
boss kind: a dispatcher `jmp [Steps + 4 * Sprite_Current +1]` through a
twelve-entry table (the kind's entry at 0, `Port_DroppedCall` at 1 and 10,
`EnemyOp_Idle`, `EnemyOp_Wait`, `0x4365D0`, `0x436620`,
`EnemyOp_ActDispatch`, `0x437030`, `0x437180`, `0x437240`,
`EnemyOp_HitPose`), an entry that stores `0x939AD8`'s `+0xFC` (animation
bytes), `+0xF4` (the hook) and `+0xF8` (cue words) - `0x939AD8` read again
for each -, sets `0x939AD8 +0x114 |= 8` (the dword) and `Sprite_Current +1 =
2`, and tail-jumps to `Sprite_ScriptTick` (its `al` the answer); and a hook
`jmp [Hooks + 4 * (word & 0xFF)]` through three `BareRet`s. Ours of a
dispatcher aborts past its table and hands the entry the caller's word; ours
of a hook aborts past 3 and hands the entry the word whole.

| Kind | Dispatcher | Entry | Hook | Its byte tables |
|---|---|---|---|---|
| 45 Ammonite | `BossAmmonite_Dispatch` `0x43ED00` | `BossAmmonite_Enter` `0x43ED20` (below) | `BossAmmonite_Hook` `0x43EDB0` | its own |
| 51 Sample 6 | `BossSample6_Dispatch` `0x43EDC0` | `BossSample6_Enter` `0x43EDE0` | `BossSample6_Hook` `0x43EE40` | kind 45's |
| 47 Sample 2 | `BossSample2_Dispatch` `0x43EF20` | `BossSample2_Enter` `0x43EF40` | `BossSample2_Hook` `0x43EFA0` | its own |
| 60 HugeSlug | `BossHugeSlug_Dispatch` `0x43EFB0` | `BossHugeSlug_Enter` `0x43EFD0` | `BossHugeSlug_Hook` `0x43F030` | kind 47's |
| 49 Sample 4 | `BossSample4_Dispatch` `0x43F0A0` | `BossSample4_Enter` `0x43F0C0` | `BossSample4_Hook` `0x43F120` | its own |
| 52 Sample 7 | `BossSample7_Dispatch` `0x43F150` | `BossSample7_Enter` `0x43F170` | `BossSample7_Hook` `0x43F1D0` | its own |
| 56 Manmo | `BossManmo_Dispatch` `0x43F1E0` | `BossManmo_Enter` `0x43F200` | `BossManmo_Hook` `0x43F260` | kind 52's |
| 57 Chimera | `BossChimera_Dispatch` `0x43F2D0` | `BossChimera_Enter` `0x43F2F0` | `BossChimera_Hook` `0x43F350` | its own (six cue words) |
| 58 Arwan | `BossArwan_Dispatch` `0x43F480` | `BossArwan_Enter` `0x43F4A0` | `BossArwan_Hook` `0x43F5A0` | its own |

**`BossAmmonite_Enter`** (`0x43ED20`) does the stores and `+1 = 2`, then
**calls** `Sprite_ScriptTick` and drops its answer; then, with bit 2 of the
chapter's flag bits (`Flags_Test([0x929ED0], 2)`, the pointer read after the
tick) set, `Battle_CopyEnemyData(slot, EnemyData_FindByTag(0x96))` where the
slot is the current enemy's `+5` less 3 as a byte, `0x939AD8` read after
`EnemyData_FindByTag`. So in some chapter state the Ammonite's data is
replaced by the area's enemy record tagged 0x96 (which state that is, and
what record, is the area's data, not read). It answers nothing (`eax` is a
callee's leftover; `BattleEnemy_RunAll` reads none): ours is `void`,
`ret_mask` 0.

**Kind 58 (Arwan)** differs at states 4 and 5:

| Address | Name | What it does |
|---|---|---|
| `0x43F500` | `BossArwan_State4Dispatch` | state 4 (the generic table's turn start `0x4365D0`): `jmp [BossArwan_State4Steps + 4 * +2]` (2: `BossArwan_State4Fx`, `BareRet`) |
| `0x43F520` | `BossArwan_State4Fx` | `+9` = the `+0x8A` byte of the area's enemy data record the current enemy's `+0xF0` indexes (`0x8C5652 + 0x8C * it`); slot n = `BattleTask_Create(3, 7)` - the kind-3 dispatcher's slot 7, **F7**; slot n's owner `+0x80` (`0x93A080 + 0x84 * n`) = `Sprite_Current` (read after the call); `+2` up by one (then `BareRet` waits) |
| `0x43F580` | `BossArwan_State5Await` | state 5: once `0x904AA8` has bit 2, `0x4376F0` and a tail jump to `0x4376A0` (the turn closed); otherwise nothing |

### Arwan's effect task: `BattleBossFx_Dispatch` slot 7

The only creator of task kind 3 with parameter 7 is `BossArwan_State4Fx`
(a scan of `.text` for `push 7; push 3` before `BattleTask_Create`), and the
dispatcher's address appears once, in `BattleBossFx_Dispatch`'s stack table.
`BattleTask_RunAll` runs it with `Sprite_Current` the slot and `0x93B940`
the owner (the enemy that created it).

| Address | Name | What it does |
|---|---|---|
| `0x43F5F0` | `BossArwanFx_Dispatch` | `call [esp + 4 * +1]` through a stack table of four: `BossArwanFx_Start`, `_Count`, `_Finish`, `MagicFx_DoneAndFree`; the entry's `eax` back |
| `0x43F630` | `BossArwanFx_Start` | `+1` up; `+9` = the `+0x8A` byte of the record **enemy 0's** `+0xF0` (`0x93BA50`) indexes; `+0xA` = 0x2D; on the owner (made `Sprite_Current`): `+0x24 \|= 0x80`, the words `+0x2E += 0x30`, `+0x30 -= 0x10`, `+0x29` = 2, `Sprite_SetAnimation(2)`; `Sprite_Current` put back |
| `0x43F6C0` | `BossArwanFx_Count` | `Sprite_ScriptTickOnce` on the owner; the task's `+9` counts down to 0, where `Sound_PlayEffect(0x601)` and `+9` = 0xFF (on the `Sprite_Current` of after the call), then holds; `+0xA` counts down, and at 0 the owner's `+1` = 5, `+2` = 0 (the owner read again for each), `Battle_SetTargetFlag40(0x904B44)` and the task's `+1` up (read after the call) |
| `0x43F750` | `BossArwanFx_Finish` | `Sprite_ScriptTickOnce` on the owner; when it answers `al` not 0, `Sprite_SetAnimation(0)`, `+0x24 &= 0x7F` and `+0x29` = 4 on the `Sprite_Current` of after the call, the task's `+1` up; `Sprite_Current` put back |
| `0x43FE80` | `MagicFx_DoneAndFree` (round nine's) | `0x904AA8 \|= 4`, `BattleTask_FreeCurrent` |

Read together (the code's order, not play): on its turn Arwan takes a count
from its enemy data, starts a task and waits; the task poses the Arwan
(`+0x24` bit 7, a nudge of `+0x2E` / `+0x30`, animation 2), plays cue
0x601 after that count, and after 0x2D frames moves the Arwan to state 5,
flags the target, and on the next pose's end resets the Arwan's pose
(animation 0) and frees itself with `0x904AA8` bit 2, which state 5 waits
for to close the turn.

### The set-ups and end hooks

Every set-up stores `BattleHook_Exit` = `BareRet` and `BattleHook_Event` =
`BareRetZero`; they differ in the end hook.

| Address | Name | End hook, and more |
|---|---|---|
| `0x43ECC0` | `Boss37_Setup` | `Boss37_End` |
| `0x43ECE0` | `Boss37_End` | won (`0x904AE8` bit 1): the chapter's step `0x8034E5` = 0x1C and `0x446DE0` (the end phase, step 1); else `0x446E00` (step 2) |
| `0x43EE50` | `Boss38_Setup` | `Boss38_End`; the target `0x904B44` = 0x40 (the enemies' side), `Sprite_Current` = enemy 7's object `0x93C178`, `MagicFx_CenterOnSide`; then (`Sprite_Current` read after) `MoveScript_F3Divisor` = 0x20, `Field_Kind2X` = the fight's centre `0x903780` + (`+0x34` - it) / 2, `Field_Kind2Z` = `0x903784` + (`+0x38` - it) / 2 - signed, halved toward zero: the field's kind-2 point midway between the fight's centre and the enemies'. `Sprite_Current` is left at enemy 7 |
| `0x43EED0` | `Boss38_End` | won: the movement script's variable 3 `0x903848` = 5, step 0x20, `Music_Track` = 0x77, step 1; else step 2 |
| `0x43EF00` | `Boss44_Setup` | `BossHook_EndPickWay` (BH's) |
| `0x43F040` | `Boss40_Setup` | `BossHook_EndPickWay` |
| `0x43F060` | `Boss53_Setup` | `Boss53_End` |
| `0x43F080` | `Boss53_End` | as `Boss37_End` with step 0x26 |
| `0x43F130` | `Boss42_Setup` | `BossHook_EndPickWay` |
| `0x43F270` | `Boss45_Setup` | `BossHook_EndPickWay` |
| `0x43F290` | `Boss49_Setup` | `Boss49_End` |
| `0x43F2B0` | `Boss49_End` | as `Boss37_End` with step 0xA |
| `0x43F360` | `Boss50_Setup` | `Boss50_End` |
| `0x43F380` | `Boss50_End` | won: step 0x10, **calls** `0x446DE0`, then `Music_Track` = 0x8C; else step 2 |
| `0x43F5B0` | `Boss51_Setup` | `Boss51_End` |
| `0x43F5D0` | `Boss51_End` | as `Boss37_End` with step 0xF |

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed (DIVERGENCE.md and `cheats.cpp` grepped for the 50 and their tables:
nothing). Where the original jumps or calls through a table by an unchecked
index (nine `+1` dispatchers, `BossArwan_State4Dispatch`, nine hooks,
`BossArwanFx_Dispatch`'s stack table) ours aborts with a `Fatal` naming the
function, and `BossArwan_State4Fx` aborts where the original would store an
owner past the 48 slots (section 6): the owner's rule for an unchecked index
(round9 doc section 6). Nothing reaches either. The dispatchers return the
entry's `eax` (the original's `jmp` / `call` passes it through).

## 3. The fuzz

`BOF3X_SHADOW=boss_si`, `src/game/boss_si_fuzz.cpp`: twenty `Run`s
(`BOF3X_BSI_RUN=k45|k51|k47|k60|k49|k52|k56|k57|k58|b37|b38|b44|b40|b53|b42|b45|b49|b50|b51|f7`
runs one), the harness unedited.

| Run | Fight, kind | Clones and shapes | Tables (`DataTable`) | Rounds | Result (this worktree) |
|---|---|---|---|--:|---|
| `k45` | 38, 45 | dispatcher `kDispatch` (12), entry `kState`, hook `kEnemyHook` | hooks (1 word), steps | 24,000 | 42,532 calls, 0 mismatches |
| `k51`, `k47`, `k60`, `k49`, `k52`, `k56`, `k57` | 44 / 40 / 53 / 42 / 45 / 49 / 50, the kind | the same three, the entry `ret_mask 0xFF` | hooks, steps | 18,000 each | 18,000 calls each, 0 mismatches |
| `k58` | 51, 58 | 2 dispatchers (12, and 2 by `+2`), entry, state 4 step 0, state 5, hook | hooks, steps, state-4 steps | 48,000 | 48,002 calls, 0 mismatches |
| `b37`, `b53`, `b49`, `b51` | the fight | `kSetup`, `kEnd` | - | 12,000 each | 6,000 calls each, 0 mismatches |
| `b38` | 38 | `kSetup` (with `MagicFx_CenterOnSide`), `kEnd` | - | 16,000 | 16,000 calls, 0 mismatches |
| `b50` | 50 | `kSetup`, `kEnd` | - | 12,000 | 6,000 calls, 0 mismatches |
| `b44`, `b40`, `b42`, `b45` | the fight | `kSetup` | - | 6,000 each | no calls (the three stores, read back), 0 mismatches |
| `f7` | 51, 58 | the dispatcher (its four stack-table immediates re-aimed) and its three steps, all `kTask` | - | 32,000 | 40,351 calls, 0 mismatches |

Coverage (the originals' calls, this worktree, the first build): every entry of every state
table about 450..700 times, `BareRet` through the hook tables every hook
round; `k45` `Flags_Test` 8,000, `EnemyData_FindByTag` /
`Battle_CopyEnemyData` 5,273; `k58` `BattleTask_Create` 8,000, `0x4376F0` /
`0x4376A0` 3,932; the ends `0x446DE0` / `0x446E00` about half each; `f7`
the four steps about 2,000 each through the dispatcher,
`Sprite_ScriptTickOnce` 16,000, `Sprite_SetAnimation` 13,388,
`Sound_PlayEffect` 1,362, `Battle_SetTargetFlag40` 1,787.

**First user of `kTask`.** The shape needed nothing more: `Sprite_Current`
and `0x93B8C4` a task slot, the owner `0x93B940` a harness record or a slot
(both in the compared state), the stack table's immediates re-aimed at
handler recorders (`Clone::imms`), and the step functions called as the
dispatcher calls them. Two things a later `kTask` group should know: the
harness's `Fix` draws the first four slots' `+1` below 3, so a four-step
table's step 3 is reached only by the group's seed (ours draws `+1` below 4
for the dispatcher); and the disturbance moves `Sprite_Current` among the
task slots and the owner among records and slots, which is what the steps'
re-reads need.

**What the group adds, in its own file** (nothing folded back is needed):
`Port_DroppedCall` with no argument and `Battle_CopyEnemyData` with byte
masks (as BSA and BSE); `0x4376F0` and `0x4376A0` with no argument (nobody's
engine code); `MagicFx_CenterOnSide` louder than the real one (its effect
writes `Sprite_Current +0x34` / `+0x38` half the time - an odd signed step
from the fight's centre or any dword - and moves the centre a quarter of
the time), because set-up 38 reads them after it. The regions: variable 3,
`Music_Track`, the fight's centre `0x903780` (8) and `Field_Kind2Z` /
`Field_Kind2X` `0x905E60` (8).

**Seeds.** Every state byte `+1..+4` but the clone's own dispatcher's drawn
inside the tables every round (below 12; below 2 for kind 58, whose state-4
table has two); the hook words 0..2 with garbage above the byte half the
time; the battle-end byte by its bits and garbage; set-up 38's enemy 7
place an odd signed step from the centre (both signs, the 32-bit ends) two
times in three; Ammonite's slot byte `+5` 3..10 and the wrap below 3; the
enemy data index `+0xF0` (the current enemy's for state 4, enemy 0's for
the task) 0..7 two times in three, any byte otherwise (the reads past the
eight records are the exe's `.data`, the same on both passes); Arwan's `+2`
at its wrap, `0x904AA8` bit 2 each way; the task's step 0..3, its count
`+9` at 0xFF, 0, 1, 2 and the wait `+0xA` at 0, 1, 2. **`Disturb`** moves
the chapter's flag bits pointer, the current enemy's `+5`, the running
sprite's `+9` / `+0xA`, the owner's `+1` / `+2` and the target.

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the final
build): exit 0 first time (no silent death), 910 `Run` totals, every one 0
mismatches, BSI's twenty among them.

## 4. Controls

`python controls.py` (the group's scratch script): each control one textual
change to ours anchored on a string that occurs once in `boss_si.cpp`, rebuild,
the one `Run` that holds the function (`BOF3X_BSI_RUN`), restore; the build is
redone at the end (and by the next plant's build). A plant in a shared helper
(`EnterStores`, `HookDispatch`, `EndStep`, `Setup`, `SetupOwnEnd`,
`SetupPickWay`) is listed under the function its run tests.

The first run (on the first seeds) was stopped after 18: the dispatchers'
"by +2" plants were refused only by a `Fatal` past the table, because the
seed drew the other state bytes inside the table two times in three; it now
draws them every round. The second run is the table below; the 31 plants
whose multi-line anchors missed (the checkout's CRLF) and P5 (first planted
as a one-entry table, a `Fatal`; now the `+1` table read by `+2`) were run
again on the same final seeds. **138 controls planted, 135 refused by a
count, 3 equivalent with their near variants refused.** The thinnest
refusals: Q16 32, Q19 32, Q23 44, Q30 159, H6 245, J3 254, P13 311 - the re-read plants (a cell read before a call, refused only when the
disturbance moves it in that call).

| # | Function | Plant | Refused (rounds) |
|---|---|---|---|
| A1 | `BossSample6_Dispatch` | by +2 | 5477 |
| A2 | `BossSample6_Enter` | Arwan's hook | 6000 |
| A3 | `BossSample6_Enter` | the cues a word on | 6000 |
| A4 | `BossSample6_Enter` | the animations a byte on | 6000 |
| A5 | `BossSample6_Hook` | the word bit 8 flipped | 6000 |
| B1 | `BossSample2_Dispatch` | by +2 | 5477 |
| B2 | `BossSample2_Enter` | Arwan's hook | 6000 |
| B3 | `BossSample2_Enter` | the cues a word on | 6000 |
| B4 | `BossSample2_Enter` | the animations a byte on | 6000 |
| B5 | `BossSample2_Hook` | the word bit 8 flipped | 6000 |
| C1 | `BossHugeSlug_Dispatch` | by +2 | 5477 |
| C2 | `BossHugeSlug_Enter` | Arwan's hook | 6000 |
| C3 | `BossHugeSlug_Enter` | the cues a word on | 6000 |
| C4 | `BossHugeSlug_Enter` | the animations a byte on | 6000 |
| C5 | `BossHugeSlug_Hook` | the word bit 8 flipped | 6000 |
| D1 | `BossSample4_Dispatch` | by +2 | 5477 |
| D2 | `BossSample4_Enter` | Arwan's hook | 6000 |
| D3 | `BossSample4_Enter` | the cues a word on | 6000 |
| D4 | `BossSample4_Enter` | the animations a byte on | 6000 |
| D5 | `BossSample4_Hook` | the word bit 8 flipped | 6000 |
| E1 | `BossSample7_Dispatch` | by +2 | 5477 |
| E2 | `BossSample7_Enter` | Arwan's hook | 6000 |
| E3 | `BossSample7_Enter` | the cues a word on | 6000 |
| E4 | `BossSample7_Enter` | the animations a byte on | 6000 |
| E5 | `BossSample7_Hook` | the word bit 8 flipped | 6000 |
| F1 | `BossManmo_Dispatch` | by +2 | 5477 |
| F2 | `BossManmo_Enter` | Arwan's hook | 6000 |
| F3 | `BossManmo_Enter` | the cues a word on | 6000 |
| F4 | `BossManmo_Enter` | the animations a byte on | 6000 |
| F5 | `BossManmo_Hook` | the word bit 8 flipped | 6000 |
| G1 | `BossChimera_Dispatch` | by +2 | 5477 |
| G2 | `BossChimera_Enter` | Arwan's hook | 6000 |
| G3 | `BossChimera_Enter` | the cues a word on | 6000 |
| G4 | `BossChimera_Enter` | the animations a byte on | 6000 |
| G5 | `BossChimera_Hook` | the word bit 8 flipped | 6000 |
| A6 | `BossSample6_Enter` | +1 = 3 (EnterStores) | 5961 |
| A7 | `BossSample6_Enter` | bit 4 of +0x114 (EnterStores) | 4559 |
| A8 | `BossSample6_Enter` | Sprite_Current's +0x114 (EnterStores) | 1302 |
| A9 | `BossSample6_Enter` | +0xF4 for +0xF8 (EnterStores) | 6000 |
| A10 | `BossSample6_Hook` | entry 2 as 0 (HookDispatch) | NOT REFUSED (equivalent: the hook table's three entries are one function, `BareRet`, so entry 2 run as entry 0 is the same call; A5 and H11 refused) |
| H1 | `BossAmmonite_Dispatch` | by +3 | 7172 |
| H2 | `BossAmmonite_Enter` | flag bit 3 | 8000 |
| H3 | `BossAmmonite_Enter` | tag 0x95 | 5311 |
| H4 | `BossAmmonite_Enter` | the slot less 2 | 5311 |
| H5 | `BossAmmonite_Enter` | Sprite_Current's +5 | 2219 |
| H6 | `BossAmmonite_Enter` | 0x939AD8 read before the call | 245 |
| H7 | `BossAmmonite_Enter` | the arguments swapped | 4917 |
| H8 | `BossAmmonite_Enter` | the test inverted | 8000 |
| H9 | `BossAmmonite_Enter` | no tick | 8000 |
| H10 | `BossAmmonite_Enter` | kind 51's hook | 8000 |
| H11 | `BossAmmonite_Hook` | the word masked | 3902 |
| H12 | `BossAmmonite_Enter` | the next flag byte | 8000 |
| I1 | `Boss37_Setup` | set-up 53's end | 6000 |
| I2 | `Boss37_Setup` | exit BareRetZero (SetupOwnEnd) | 6000 |
| I3 | `Boss37_End` | step 0x1D | 3096 |
| I4 | `Boss37_End` | bit 0 (EndStep) | 3102 |
| I5 | `Boss37_End` | step 2 on a win (EndStep) | 3168 |
| J1 | `Boss38_Setup` | target 0x41 | 7584 |
| J2 | `Boss38_Setup` | enemy 6 | 7715 |
| J3 | `Boss38_Setup` | Sprite_Current read before the call | 254 |
| J4 | `Boss38_Setup` | divisor 0x21 | 8000 |
| J5 | `Boss38_Setup` | x halved by a shift (down, not toward zero) | 2498 |
| J6 | `Boss38_Setup` | z from +0x3C | 8000 |
| J7 | `Boss38_Setup` | x into Kind2Z | 8000 |
| J8 | `Boss38_Setup` | set-up 37's end | 8000 |
| J9 | `Boss38_Setup` | centre x for z | 8000 |
| J10 | `Boss38_Setup` | x a unit on | 8000 |
| J11 | `Boss38_End` | variable 3 = 6 | 4251 |
| J12 | `Boss38_End` | step 0x21 | 4155 |
| J13 | `Boss38_End` | track 0x78 | 4251 |
| J14 | `Boss38_End` | bits 0 or 1 | 1641 |
| K1 | `Boss44_Setup` | event BareRet | 6000 |
| K2 | `Boss44_Setup` | set-up 51's end (SetupPickWay) | 6000 |
| K3 | `Boss40_Setup` | exit BareRetZero | 6000 |
| K4 | `Boss42_Setup` | end BareRet | 6000 |
| K5 | `Boss45_Setup` | event the end | 6000 |
| K6 | `Boss44_Setup` | end and exit swapped (Setup) | 6000 |
| L1 | `Boss53_Setup` | set-up 49's end | 6000 |
| L2 | `Boss53_End` | step 0x25 | 3096 |
| M1 | `Boss49_Setup` | end PickWay | 6000 |
| M2 | `Boss49_End` | step 0xB | 3096 |
| N1 | `Boss50_Setup` | set-up 51's end | 6000 |
| N2 | `Boss50_End` | step 0x11 | 3096 |
| N3 | `Boss50_End` | track 0x8D | 3168 |
| N4 | `Boss50_End` | the track before the call (equivalent) | NOT REFUSED (equivalent: `0x446DE0` writes `0x904AA0..0x904AA2`, not `Music_Track`, so the store's order against the call cannot be seen; N3 and N5 refused) |
| N5 | `Boss50_End` | the track on a loss too | 2823 |
| O1 | `Boss51_Setup` | set-up 37's end | 6000 |
| O2 | `Boss51_End` | step 0xE | 3096 |
| P1 | `BossArwan_Dispatch` | by +2 | 6998 |
| P2 | `BossArwan_Enter` | Chimera's hook | 8000 |
| P3 | `BossArwan_Enter` | Chimera's animations | 8000 |
| P4 | `BossArwan_State4Dispatch` | by +3 | 3940 |
| P5 | `BossArwan_State4Dispatch` | the +1 table | 8000 |
| P6 | `BossArwan_State4Fx` | kind 4 | 8000 |
| P7 | `BossArwan_State4Fx` | slot 6 of the dispatcher | 8000 |
| P8 | `BossArwan_State4Fx` | stride 0x80 | 7824 |
| P9 | `BossArwan_State4Fx` | +0x8B | 5350 |
| P10 | `BossArwan_State4Fx` | Sprite_Current's +0xF0 | 1572 |
| P11 | `BossArwan_State4Fx` | the owner 0x939AD8 | 2719 |
| P12 | `BossArwan_State4Fx` | +2 by two | 8000 |
| P13 | `BossArwan_State4Fx` | Sprite_Current read before the call | 311 |
| P14 | `BossArwan_State5Await` | bit 3 | 3938 |
| P15 | `BossArwan_State5Await` | the calls swapped | 4030 |
| P16 | `BossArwan_State5Await` | no chance | 4030 |
| P17 | `BossArwan_Hook` | the word bit 8 flipped | 8000 |
| P18 | `BossArwan_Dispatch` | the word + 1 handed on | NOT REFUSED (equivalent: no entry of a `+1` table reads a word - `Port_DroppedCall` is a bare `ret`, the others take none; P17 refused) |
| Q1 | `BossArwanFx_Dispatch` | steps 1 and 2 swapped | 4008 |
| Q2 | `BossArwanFx_Dispatch` | by +2 | 5998 |
| Q3 | `BossArwanFx_Dispatch` | step 3 the finish | 2062 |
| Q4 | `BossArwanFx_Start` | +1 by two | 7989 |
| Q5 | `BossArwanFx_Start` | enemy 1's +0xF0 | 5474 |
| Q6 | `BossArwanFx_Start` | the owner's +0xF0 | 5474 |
| Q7 | `BossArwanFx_Start` | +0xA 0x2C | 7991 |
| Q8 | `BossArwanFx_Start` | bit 6 | 6016 |
| Q9 | `BossArwanFx_Start` | +0x2E by 0x31 | 7958 |
| Q10 | `BossArwanFx_Start` | +0x30 up | 7966 |
| Q11 | `BossArwanFx_Start` | +0x29 = 3 | 8000 |
| Q12 | `BossArwanFx_Start` | animation 1 | 8000 |
| Q13 | `BossArwanFx_Start` | Sprite_Current not put back | 6995 |
| Q14 | `BossArwanFx_Count` | the tick on the task | 627 |
| Q15 | `BossArwanFx_Count` | cue 0x602 | 1337 |
| Q16 | `BossArwanFx_Count` | the task, not Sprite_Current read after the call | 32 |
| Q17 | `BossArwanFx_Count` | the stop at 0xFE | 2085 |
| Q18 | `BossArwanFx_Count` | down by two | 5282 |
| Q19 | `BossArwanFx_Count` | the wait on the task, not Sprite_Current read again | 32 |
| Q20 | `BossArwanFx_Count` | the owner to state 4 | 1798 |
| Q21 | `BossArwanFx_Count` | the owner +2 = 1 | 1791 |
| Q22 | `BossArwanFx_Count` | the target + 1 | 1805 |
| Q23 | `BossArwanFx_Count` | +1 on the Sprite_Current of before the call | 44 |
| Q24 | `BossArwanFx_Count` | the task to state 5, not the owner | 1605 |
| Q25 | `BossArwanFx_Count` | the wait held | 6195 |
| Q26 | `BossArwanFx_Finish` | animation 1 | 5360 |
| Q27 | `BossArwanFx_Finish` | bit 6 cleared | 4031 |
| Q28 | `BossArwanFx_Finish` | +0x29 = 5 | 5360 |
| Q29 | `BossArwanFx_Finish` | the test inverted | 8000 |
| Q30 | `BossArwanFx_Finish` | the owner, not Sprite_Current read after the call | 159 |
| Q31 | `BossArwanFx_Finish` | Sprite_Current not put back | 6946 |
| Q32 | `BossArwanFx_Count` | Sprite_Current left at the owner | 6926 |

## 5. What nothing reached

Every clone reached every branch the controls planted in. What only a fight
would show: every one of the ten set-ups and their ends, the Ammonite's
data swap (which chapter bit, which record tagged 0x96), Arwan's turn and
its task. Not reached: a dispatcher past its table and `BossArwan_State4Fx`
with no free slot (the harness's `BattleTask_Create` answers 0..47) - the
`Fatal`s.

## 6. Latent defects (Capcom's, kept or aborted)

- **`BossArwan_State4Fx` does not test `BattleTask_Create`'s answer.** With
  all 48 slots taken it answers 0xFF and the original stores the owner at
  `0x93A080 + 0xFF * 0x84` = `0x9423FC`, past the pool (which ends at
  `0x93B8C0`); the task never runs, so the Arwan waits in state 4 step 1
  (`BareRet`) for good. Ours aborts with a `Fatal` there. Whether a fight
  can fill the pool is not measured.
- **The task reads enemy 0's `+0xF0`, not its owner's.** `BossArwanFx_Start`
  takes the count `+9` from the record `0x93BA50` indexes - enemy 0's type -
  whatever enemy owns the task, while `BossArwan_State4Fx` takes the
  Arwan's own. The same when the Arwan is enemy 0 (not measured which slot
  it has); kept.
- **Both enemy-data reads trust `+0xF0`**: an index past the area's eight
  records reads `0x8C5652 + 0x8C * index`, up to `0x8CE1C6`, still inside
  `.data`; kept (a read).
- **`Boss38_Setup` leaves `Sprite_Current` at enemy 7's object** and relies
  on `MagicFx_CenterOnSide`, which divides by the count of enemies not out:
  with all eight out at set-up the original divides by zero (ours aborts
  there, round nine's `magic_lib.cpp`). At set-up time the fight's enemies
  are presumably in (not measured).
- **The dispatchers index unchecked** (ten by a state byte, nine hooks by the
  word's low byte, the task's stack table by `+1`: past 3 it calls through
  its caller's frame). Ours aborts.
- **The nine hook tables are three `BareRet`s each**: the hook does nothing
  for any of the nine kinds. Not a defect.

## 7. Named data

Thirty-one tables, `[[data]]` in `symbols.toml` (addresses and counts only;
their contents are the exe's). The counts are the code's (a dispatcher's
state values, the next table's address), not the tool's extents ("30 code
entries" for kind 45's `+1` table is its 12, its hooks' 3 and kind 51's
table).

| Address | Name | Count | What |
|---|---|--:|---|
| `0x64D94C` | `BossAmmonite_Anims` | 12 bytes | kinds 45 and 51's `+0xFC` |
| `0x64D958` | `BossAmmonite_Cues` | 4 words | kinds 45 and 51's `+0xF8` |
| `0x64D960` | `BossAmmonite_Steps` | 12 | kind 45's `+1` table |
| `0x64D990` | `BossAmmonite_Hooks` | 3 | `BareRet` x3 |
| `0x64D99C` | `BossSample6_Steps` | 12 | kind 51's |
| `0x64D9CC` | `BossSample6_Hooks` | 3 | `BareRet` x3 |
| `0x64D9D8` | `BossSample2_Anims` | 12 bytes | kinds 47 and 60's |
| `0x64D9E4` | `BossSample2_Cues` | 4 words | kinds 47 and 60's |
| `0x64D9EC` | `BossSample2_Steps` | 12 | kind 47's |
| `0x64DA1C` | `BossSample2_Hooks` | 3 | `BareRet` x3 |
| `0x64DA28` | `BossHugeSlug_Steps` | 12 | kind 60's |
| `0x64DA58` | `BossHugeSlug_Hooks` | 3 | `BareRet` x3 |
| `0x64DA64` | `BossSample4_Anims` | 12 bytes | kind 49's |
| `0x64DA70` | `BossSample4_Cues` | 4 words | kind 49's |
| `0x64DA78` | `BossSample4_Steps` | 12 | kind 49's |
| `0x64DAA8` | `BossSample4_Hooks` | 3 | `BareRet` x3 |
| `0x64DAB4` | `BossSample7_Anims` | 12 bytes | kinds 52 and 56's |
| `0x64DAC0` | `BossSample7_Cues` | 4 words | kinds 52 and 56's |
| `0x64DAC8` | `BossSample7_Steps` | 12 | kind 52's |
| `0x64DAF8` | `BossSample7_Hooks` | 3 | `BareRet` x3 |
| `0x64DB04` | `BossManmo_Steps` | 12 | kind 56's |
| `0x64DB34` | `BossManmo_Hooks` | 3 | `BareRet` x3 |
| `0x64DB40` | `BossChimera_Anims` | 12 bytes | kind 57's |
| `0x64DB4C` | `BossChimera_Cues` | 6 words | kind 57's |
| `0x64DB58` | `BossChimera_Steps` | 12 | kind 57's |
| `0x64DB88` | `BossChimera_Hooks` | 3 | `BareRet` x3 |
| `0x64DB94` | `BossArwan_Anims` | 12 bytes | kind 58's |
| `0x64DBA0` | `BossArwan_Cues` | 4 words | kind 58's |
| `0x64DBA8` | `BossArwan_Steps` | 12 | kind 58's (4 and 5 its own) |
| `0x64DBD8` | `BossArwan_State4Steps` | 2 | `BossArwan_State4Fx`, `BareRet` |
| `0x64DBE0` | `BossArwan_Hooks` | 3 | `BareRet` x3 |

The byte and word tables' sizes are the gap to the next table the code
names; how much of each the engine reads was not measured.

## 8. Calls across groups

**Out of the group, to code nobody owns** (raw, `boss_si_callees.h`): the
end phase's steps `0x446DE0` and `0x446E00`, `0x4376F0` and `0x4376A0` -
unnamed engine code, no group's this round. No raw call into another
wave-two group. Everything else is ours and called by name: BH's
`BossHook_EndPickWay`, `BareRet`, `BareRetZero` (stored as literals), round
seven's `EnemyOp_*` and `Port_DroppedCall` (table entries), round nine's
`MagicFx_DoneAndFree` (a stack-table entry, by its address) and
`MagicFx_CenterOnSide`, `Sprite_*`, `Battle_*`, `EnemyData_FindByTag`,
`Flags_Test`, `Sound_PlayEffect`, `BattleTask_Create`.

**Into the group from outside** (for the rebinding pass): none from our code
(`grep` of `src/` for the 50 addresses: only this group's files and the
`inject_all.cpp` comment). Capcom's references are the root tables and the
kinds' own: `Boss_SetupTable` 37, 38, 40, 42, 44, 45, 49, 50, 51, 53;
`BossKind_Table` 45, 47, 49, 51, 52, 56, 57, 58, 60; the state, sub- and
hook tables above; the stores of the hooks and `+0xF4` in the set-ups and
entries; and `BattleBossFx_Dispatch` `0x4357D0`'s stack table (Capcom's,
not ours) for `BossArwanFx_Dispatch`, whose own stack table names its three
steps. All reach ours through the entries' `jmp`s.

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 50 extents above, one
line each, none there before. The smaller extents fix the host line
`0043E290 1120`, which ran over the group to `0x43F3B0`.
