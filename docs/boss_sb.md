# Group BSB: fights 4..10 and 13, enemy kinds 3, 4, 5 and 8..11

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave one, stage B. 52 functions ours (`src/game/boss_sb.cpp`, shadow
`boss_sb`), each read to its last instruction with capstone and fuzzed
through the boss harness ([`boss_harness.md`](boss_harness.md)), 15 `Run`s,
0 mismatches; 124 controls planted, 124 refused by a count, none by a `Fatal`, none equivalent. Fuzz only: no recorded route reaches a boss
fight. The first group to use the harness's `kSetup` shape (section 3).

Enemy and fight names below are `tools/boss_rows.py --disc`'s (the US disc's
area records, 2026-09-28), not memory of the game. Which fight a set-up is
comes from the tool's rows; nothing here says what happens in a fight beyond
what the code does.

## 1. The units and the functions

`tools/boss_rows.py --unit <U> --clones` for the fifteen units, and
`analysis/boss_funcs.tsv`'s group column: 52 functions, all BSB's, none
ours before. Every clone row was read against the disassembly; the tool's
extents were right for all 52, and no start was dropped or added. Its "N code
entries" notes on the dispatchers read each table to the next named address
(kind 8's "72" is 12, then BH's three tables and the hook table): the counts
below are the code's.

| Unit | What (the tool's names) | Functions | The PSX file (sibling's `BOSS.md`) |
|---|---|--:|---|
| K03 | kind 3: Engineer, Foreman, Miner - one script (area 24) | 7 | BOSS004 |
| B04, B05, B06 | set-ups 4, 5, 6: area 24 rows 7, 6, 5, kind 3 | 4 + 4 + 3 (`0x4389E0` is B04's and B06's end hook, taken once) | BOSS004 |
| K04, K05 | kinds 4 and 5: Worker (areas 2, 32), Operator (area 2) | 3 + 4 | BOSS007 |
| B07 | set-up 7: row 7, kinds 4 and 5 | 4 (`0x438E30`, its exit hook, is B13's too) | BOSS007 |
| B13 | set-up 13: row 7 (the tool's kinds 13, 14, 17 - Balio, Sunder, Nina - are BSC's) | 2 | BOSS013 |
| K08..K11 | kinds 8..11: Torast, Kassen, Galtel, Doksen (area 27) | 3 each | BOSS008 |
| B08, B09, B10 | set-ups 8, 9, 10: area 27 rows 7, 6, 5 | 3 each | BOSS008 |

The sibling's per-image root counts (`analysis/overlay_captures_all.json`)
are the check on size only; BOSS004's three set-ups and one kind are 18
functions here, BOSS008's four kinds and three of its four set-ups 21 (the
fourth, B11, is BSC's; the death chain the kinds share is BH's).

### 1.1 Kind 3 (`BossEngineer_*`)

Named for the first of the three enemies the tool prints for it.

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x438290` | `BossEngineer_Dispatch` | 0x12 | `BossKind_Table[3]`: `jmp [BossEngineer_Steps + 4 * Sprite_Current +1]`, 12 entries |
| `0x4382B0` | `BossEngineer_Enter` | 0x3D | state 0: the enemy's (`0x939AD8`'s) `+0xFC` = `BossEngineer_Anims`, `+0xF4` = `BossEngineer_Hook`, `+0xF8` = `BossEngineer_Cues`; `+1` = 2; `jmp Sprite_ScriptTick` |
| `0x4382F0` | `BossEngineer_ActDispatch` | 0x12 | state 6: by `+2` through `BossEngineer_ActSubs` (6: `EnemyOp_ActSubs` with `BossOp_Death` at 4) |
| `0x438310` | `BossEngineer_HitDispatch` | 0x12 | state 11 (where the generic table has `EnemyOp_HitPose`): by `+2` through `BossEngineer_HitSteps` (3) |
| `0x438330` | `BossEngineer_HitStart` | 0x1C | hit step 0: `BattleEnemy_SetAnimation(4)`, then `+9` = 0x3C and `+2` up |
| `0x438350` | `BossEngineer_HitStep` | 0xC9 | hit step 1: `+9` down; at 0: bank 0x5F; in fight 5 (`0x904AAA`) the flip `+0x2A` = 0 and z `+0x38` += 0x18000, else `+0x2A` = 1 and x `+0x34` += 0x18000; the ground word `+0x3E` = `AreaMap_Elevation(x, z)`; the enemy's `+0xFC` = `BossEngineer_AnimsHit`; animation 1; `0x904AE8 |= 4`; `0x904AAD |= 1 << (+5 & 0x1F)` as a byte (0 from 8 up: `shl dl, cl`); `+2` up. Every path tail-jumps to `Sprite_ScriptTick`. Hit step 2 is `BossOp_ScriptTick` (BH's) |
| `0x438420` | `BossEngineer_Hook` | 0x10 | the `+0xF4` hook: `jmp [BossEngineer_Hooks + 4 * (word & 0xFF)]`, 3 entries, all `BareRet` |

So kind 3's hit, where other kinds take `EnemyOp_HitPose`, is a pose, a wait
of 0x3C frames, and a step of 1.5 units away along one axis that sets
`0x904AE8` bit 2 and the enemy's slot bit (`+5`) in `0x904AAD` - the code's
facts; what the player sees is not read here.

### 1.2 Set-ups 4, 5, 6 (`Boss04_*`, `Boss05_*`, `Boss06_*`)

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x438430` | `Boss04_Setup` | 0x1F | `BattleHook_End` = `Boss04_End`, `_Exit` = `Boss04_Exit`, `_Event` = `Boss04_Event` |
| `0x438450` | `Boss04_Event` | 0x104 | a jump table of 7 by the phase code's low byte (past 6: nothing), al 0 on every path. 0: with round-flag bit 0x40 and the target 0, `0x904AAD |= 1`. 1: when actor 0 gives command kind 4 (`0x904B35`) with id 0x78 (the word `+2` of the command `[0x904B40]`), enemies 0 and 1's words `+0xA4` (HP) and `+0xB0` = 1 and the leader's `+0x134` bit 1 cleared. 3: the leader's HP 0 clears that bit. 5: with `0x904AAD` bit 0, the leader's command (`+0x124` target 0x40, `+0x125` kind 4, `+0x126` id 0x78) and the battle's kind and target the same. 6: the bit set |
| `0x438560` | `Boss04_Exit` | 0xE3 | the loss (`0x904AE8` bit 0): `BossActor_ClearBit40` of actors 0 and 1. Otherwise each actor (0, 1) posed from its enemy (0, 1): `ClearBit40`, `CopyFrom(tag, enemy, 1)` (the place), `Sprite_Current = BossActor_Find(tag)`, bank 0x5F, `+0x48` = 0, flip `+0x2A` = 1, animation 1, the enemy's words `+0x58` / `+0x5A` |
| `0x4389E0` | `Boss04_End` | 0x34 | set-ups 4 and 6: the loss - `0x92BF18` = 7, `0x904AE5 |= 0x80`, `0x446E20`, then the chapter's step `0x8034E5` = 0x32; otherwise the step up by one and `0x446E20` |
| `0x438650` | `Boss05_Setup` | 0x1F | the three hooks |
| `0x438670` | `Boss05_Event` | 0x104 | byte for byte `Boss04_Event` (its own jump table) |
| `0x438780` | `Boss05_End` | 0x34 | `Boss04_End` with `0x92BF18` = 8 |
| `0x4387C0` | `Boss05_Exit` | 0xE5 | `Boss04_Exit` with actors 2 and 3 and the flip 0 |
| `0x4388B0` | `Boss06_Setup` | 0x1F | the hooks, the end hook `Boss04_End` |
| `0x4388D0` | `Boss06_Event` | 0x104 | byte for byte `Boss04_Event` |
| `0x438A20` | `Boss06_Exit` | 0x10E | `Boss04_Exit` with actors 4 and 5, flip 1, then actor 5's `+0x34..+0x3C` = enemy 1's (not on the loss's path) |

The three event hooks are one body in ours (`Kind3Event`), each entry a
line; the controls plant in each entry and in the shared body (section 4).

### 1.3 Kinds 4 and 5 (`BossWorker_*`, `BossOperator_*`)

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x438B30` | `BossWorker_Dispatch` | 0x12 | `BossKind_Table[4]`: by `+1` through `BossWorker_Steps` (12: its state 0, then the generic `EnemyOp_Steps` entries) |
| `0x438B50` | `BossWorker_Enter` | 0x3D | state 0: `+0xFC` / `+0xF4` / `+0xF8` = `BossWorker_Anims`, `BossWorker_Hook`, `BossWorker_Cues`; `+1` = 2; tick |
| `0x438B90` | `BossWorker_Hook` | 0x10 | the hook through `BossWorker_Hooks` (3, all `BareRet`) |
| `0x438BA0` | `BossOperator_Dispatch` | 0x12 | `BossKind_Table[5]`: through `BossOperator_Steps` (12) |
| `0x438BC0` | `BossOperator_Enter` | 0x47 | state 0: its tables, `Sprite_Current +8` = 0, `+1` = 2; tick |
| `0x438C10` | `BossOperator_Hook` | 0x10 | the hook through `BossOperator_Hooks` (3: `BareRet`, `BossOperator_HookHit`, `BareRet`) |
| `0x438C20` | `BossOperator_HookHit` | 0xC | hook entry 1 (called with 1, the hit `0x4367EE`): the word `+4` of the target block `[0x904B50]` = 0; the word argument not read |

### 1.4 Set-ups 7 and 13 (`Boss07_*`, `Boss13_*`)

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x438C30` | `Boss07_Setup` | 0x1F | the three hooks |
| `0x438C50` | `Boss07_Event` | 0x174 | a jump table of 7: 0, 3, 5, 6 as `Boss04_Event`'s; 1 - on the same command test, `0x904AE8 |= 4`, the leader's bit 1 cleared, `Str_CopyN(0x904EA0, 0x65D008, 0x10)`, `BattleBanner_Add(1, 1, 0, 0x1E, 0x904EA0)`, `0x8031F3` = 1, `Sprite_Current` = the leader, `Sprite_SetAnimation(0x2C)`, then the phase `0x904AA0` = 4, `0x904AA5` = 0x3C, the step `0x904AA1` = 2; 2 - with `0x904AE8` bit 2, `0x904AA5` down by one and al 0xFF while it is not 0 (`BattleRoundEnd_NextRound` holds the round: the only hook of the group answering other than 0); 4 nothing |
| `0x438DD0` | `Boss07_End` | 0x52 | the loss - the chapter's step 0x32, `0x92BF18` = 6, the chapter's run `0x8034E4` = 6; otherwise the step up by one, `0x904AE8 |= 8`, `Draw_PassFlags` 0, `0x92BF18` = 2, `Music_Track` 4. Then `0x446E20` |
| `0x438E30` | `Boss07_Exit` | 0x12 | set-ups 7 and 13: `BossActor_ClearBit40(0)`, `(1)` |
| `0x439FE0` | `Boss13_Setup` | 0x1F | End `Boss13_End`, Exit `Boss07_Exit`, Event `BareRetZero` |
| `0x43A000` | `Boss13_End` | 0x22 | the movement script's variable 3 (`0x903848`) = 0x32; the win (`0x904AE8` bit 1, read before) - `0x446DE0`; otherwise `0x446E20` then `0x92BF18` = 3 |

The text copied at `0x65D008` is 16 bytes of `.data`; what it says was not
read (it would be game data).

### 1.5 Kinds 8..11 (`BossTorast_*`, `BossKassen_*`, `BossGaltel_*`, `BossDoksen_*`)

| Kind | Dispatcher (`BossKind_Table[k]`) | State 0 | `+0xF4` hook | `+1` table | Hook table |
|---|---|---|---|---|---|
| 8 Torast | `0x438E50` `BossTorast_Dispatch` | `0x438E70` `BossTorast_Enter` | `0x4390E0` `BossTorast_Hook` | `BossTorast_Steps` `0x64CAA4` | `BossTorast_Hooks` `0x64CB04` |
| 9 Kassen | `0x4390F0` | `0x439110` | `0x439150` | `0x64CB10` | `0x64CB40` |
| 10 Galtel | `0x439160` | `0x439180` | `0x4391C0` | `0x64CB4C` | `0x64CB7C` |
| 11 Doksen | `0x4391D0` | `0x4391F0` | `0x439230` | `0x64CB88` | `0x64CBB8` |

Each dispatcher is 0x12 bytes by `+1` through its 12-entry table; the four
tables differ only in entry 0 (the kind's state 0) and share entry 6,
`BossTorast_ActDispatch` (BH's, the shared death). Each state 0 is 0x3D
bytes and stores the same two byte tables, `BossTorast_Anims` (`+0xFC`) and
`BossTorast_Cues` (`+0xF8`), and its own hook; each hook is 0x10 bytes
through three `BareRet`s.

### 1.6 Set-ups 8, 9, 10 (`Boss08_*`..`Boss10_*`)

| Address | Name | What it does |
|---|---|---|
| `0x439240`, `0x4392D0`, `0x439370` | `Boss08_Setup`, `Boss09_Setup`, `Boss10_Setup` | the hooks; Event `BareRetZero` |
| `0x439260`, `0x4392F0`, `0x439390` | `Boss08_End` .. `Boss10_End` (0x1A) | the win: the movement script's variable 3 = 0x11 / 0x20 / 0x30 and `0x446DE0`; otherwise `0x446E00` |
| `0x439280`, `0x439310`, `0x4393B0` | `Boss08_Exit` (0x48), `Boss09_Exit`, `Boss10_Exit` (0x52) | actor 0 / 1 / 2: `ClearBit40`, `Find` into `Sprite_Current`, bank 0x83, (9 and 10: flip `+0x2A` = 1), animation 0, then enemy 0's words `+0x58` / `+0x5A` - enemy 0's in all three |

### 1.7 The tables named (`[[data]]`)

The pointer tables each dispatcher jumps through (counts from the code: the
next table's address), and the byte tables each state 0 stores: `+0xFC` the
animation table `BattleEnemy_SetAnimation` reads (battle_flow.md), `+0xF8`
the cue bytes (enemy_ai_ops.md). `BossEngineer_Anims` / `_AnimsHit` /
`_Cues` / `_Steps` / `_ActSubs` / `_HitSteps` / `_Hooks` (`0x64C964..0x64C9E3`),
`BossWorker_Anims` / `_Cues` / `_Steps` / `_Hooks`, `BossOperator_Anims` /
`_Cues` / `_Steps` / `_Hooks` (`0x64C9E4..0x64CA83`, interleaved: the two
kinds' byte tables lie side by side), `BossTorast_Anims` / `_Cues`
(`0x64CA90`, `0x64CA9C`: what [`boss_h.md`](boss_h.md) section 7 called kind
8's flag header), and each of kinds 8..11's `_Steps` and `_Hooks`. 25 in
all. Round seven's naming did not reach these tables (its renames stop at
`0x64C928`).

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed (none of the 52 addresses is in DIVERGENCE.md or `cheats.cpp`). Two
things ours does that the original expresses differently, neither a change
of behaviour:

- **A dispatcher passes its word on.** The original's `jmp [table + 4 *
  state]` leaves its caller's stack in place, so an entry that reads an
  argument (`Port_DroppedCall`, the hooks' entries) sees the caller's word.
  Ours takes that word and hands it on, and answers the entry's eax (the
  original's `jmp` answers whatever the entry answers).
- **Past a table ours aborts** with a `Fatal` naming the function, where the
  original jumps through the dword after (round9 doc section 6); nothing
  reached it. Likewise where an exit hook's `BossActor_Find` answers null
  (section 6).

## 3. The fuzz

`BOF3X_SHADOW=boss_sb`, `src/game/boss_sb_fuzz.cpp`, fifteen `Run`s of
8,000 rounds a function (`BOF3X_BSB_RUN=k03|k04|k05|k08|k09|k10|k11|b04|b05|b06|b07|b13|b08|b09|b10`
runs one):

| Run | Fight, kind | Clones (shape) | `.data` tables swapped | Result (this worktree) |
|---|---|---|---|---|
| `k03` | 5, 3 | `BossEngineer_Dispatch` (`kDispatch`, `+1` < 12), `_Enter`, `_ActDispatch` (`+2` < 6), `_HitDispatch` (`+2` < 3), `_HitStart`, `_HitStep`, `_Hook` | `BossEngineer_Steps`, `_ActSubs`, `_HitSteps`, `_Hooks` (one word) | 56,000 rounds, 63,752 calls, 0 mismatches |
| `k04` | 7, 4 | `BossWorker_Dispatch`, `_Enter`, `_Hook` | `BossWorker_Steps`, `_Hooks` | 24,000 rounds, 24,000 calls, 0 |
| `k05` | 7, 5 | `BossOperator_Dispatch`, `_Enter`, `_Hook`, `_HookHit` (`kEnemyHook`) | `BossOperator_Steps`, `_Hooks` | 32,000, 24,000, 0 |
| `k08`..`k11` | 8, 8..11 | each kind's `_Dispatch`, `_Enter`, `_Hook` | each kind's `_Steps`, `_Hooks` | 24,000 rounds, 24,000 calls, 0 mismatches each |
| `b04` | 4 | `Boss04_Setup` (`kSetup`), `_Event` (`kEvent`, its jump table moved into the copy), `_Exit` (`kExit`), `_End` (`kEnd`) | | 32,000, 56,128, 0 |
| `b05` | 5 | `Boss05_Setup`, `_Event`, `_End`, `_Exit` | | 32,000, 56,120, 0 |
| `b06` | 6 | `Boss06_Setup`, `_Event`, `_Exit` | | 24,000, 48,424, 0 |
| `b07` | 7 | `Boss07_Setup`, `_Event`, `_End`, `_Exit` | | 32,000, 24,741, 0 |
| `b13` | 13 | `Boss13_Setup`, `_End` | | 16,000, 8,000, 0 |
| `b08`, `b09`, `b10` | 8, 9, 10 | each set-up's `_Setup`, `_End`, `_Exit` | | 24,000 rounds, 40,000 calls, 0 mismatches each |

416,000 rounds in all. Kinds 8..11 run with fight 8 (their code does not read the fight byte; which of set-ups
8..11 spawns which kind is the data's, [`takeover-queue-bosses.md`](takeover-queue-bosses.md) section 7); kind 3 with
fight 5 (its step compares the byte with 5); kinds 4 and 5 with fight 7.

Every dispatcher is `kDispatch` with its state byte drawn below its table
(`+1` below 12; kind 3's `+2` below 6 and 3); every state 0 and kind 3's hit
steps `kState` (`ret_mask 0xFF` where they tail-jump to `Sprite_ScriptTick`,
0 for `BossEngineer_HitStart`, which answers nothing its callers read);
every hook `kEnemyHook`; the set-ups `kSetup`, their hooks `kEvent` / `kEnd`
/ `kExit`. The dispatchers are listed with `ret_mask 0xFF`: ours answers the
entry's eax as the original's `jmp` does.

**Seeds.** Kind 3's step: the count `+9` at 1 (it reaches 0), 0 (it wraps),
2, 0x3D or any; the fight byte 5 half the time (4, 6 and others otherwise;
the run's fight is 5, so the harness's own fight disturbance lands on both
sides of the compare); the shift count `+5` at 0..8, 0x1F, 0x20, 0x21, 0x27,
0xFF, 0xE3. Kind 5's hit: the target block `[0x904B50]` pointed into one of
the harness's records at any even offset. The hooks' word 0..2 with garbage
above the byte half the time (the hooks read its low byte and hand the whole
word on). The event hooks: the phase code 0..6 two times in three (7..255
otherwise), garbage above the byte half the time; the actor 0 two times in
three; the command kind 4 two times in three; the command `[0x904B40]`
pointed into one of the harness's records with its word `+2` 0x78 two times
in three (0x77, 0x79, 0x178, 0xF8, 0x7078, 0 or any otherwise); round-flag
bit 0x40, `0x904AAD` bit 0 and `0x904AE8` bit 2 each half the time; the
target 0 half the time; the leader's HP 0 half the time; the countdown
`0x904AA5` at 1, 0, 2, 0x3C, 0x80, 0xFF or any. The end and exit hooks:
`0x904AE8` at its low bits' combinations and 0xFC..0xFF; the chapter's step
at 0xFF (the wrap), 0, 1, 0x31, 0x32. The group's `disturb` moves what the
exit hooks and kind 3's step read after a call: enemy 0 and 1's pose words,
enemy 1's place, and the fight byte (5 or 4..6).

Regions beyond the standard ones: `0x92BF18`, `0x903848`, `0x8031F3`,
`Draw_PassFlags`, `Music_Track` (a byte each, written by the end hooks and
`Boss07_Event`). `0x904B40` / `0x904B50` are battle bytes (in the standard
region); the seeds point them into the harness's records, which are compared.

**The harness's `kSetup`** (first used here) needed nothing more: a set-up's
three stores land in the compared battle bytes and in the hooks the harness
logs after the call, and a swapped literal is refused on the first round
(S1..S8, 8,000 rounds each). `kEnd`, `kExit` and `kEvent` were BH's.

**Coverage** (the originals' calls, this worktree): kind 3: every entry of its four tables (`phase 0x4382B0` 688 .. `BareRet` 8,000 through the hook), `BattleEnemy_SetAnimation` 8,000, `AreaMap_Elevation` / `Sprite_SetAnimation` / `Sprite_SetAnimationBank` 2,584 (the step's zero path), `Sprite_ScriptTick` 16,000, `Port_DroppedCall` 1,284; each of kinds 4, 5, 8..11: every entry of its `+1` table (about 650 each) and of its hook table (`BossOperator_HookHit` 2,639 through the hook); set-ups 4..6: `0x446E20` 8,000, the spawn helpers about 8,000 each (the exit hooks' pose path) and `BossActor_ClearBit40` 16,000; set-up 7: `BattleBanner_Add` / `Str_CopyN` 247 (the banner), `0x446E20` 8,000; set-up 13: `0x446DE0` 3,117, `0x446E20` 4,883; set-ups 8..10: `0x446DE0` about 3,100, `0x446E00` about 4,900, the exit hooks' calls 8,000 each. Counts move with the build directory; judge by 0 mismatches and controls refused.

`BOF3X_SHADOW='*'` (every group of every harness): exit 0 on the final build, every earlier group's fuzz passing, BSB's fifteen `Run`s with 0 mismatches; it passed first time (no silent death).

## 4. Controls

`controls.py` (the group's scratch script): each control one textual change
to `boss_sb.cpp` (anchored on strings that occur once), then rebuild, run
the one `BOF3X_BSB_RUN` it belongs to, restore, and at the end rebuild.
Run twice; the table is the second run, on the final seeds. **124 planted, 124 refused**, every one by a count (the planted function's mismatched rounds), at least one in each of the 52 functions (52 names in the table). On the first run eight plants that swapped a dispatcher's table for another kind's crashed both passes (the other table's entries are not recorders in that run, so the real functions ran on the fuzz's garbage): the fuzz's fault, not a refusal. They were replaced by "the table one entry on" (K1, K5, W1, O1, T1, T4, T7, T10), whose entries are all recorders of the run. The thinnest refusals: E8 (27), E12 (57), N4 (78), E5 (95), F1 (109), E4 (133) - the event hooks' phase 1 needs the phase code, the actor, the command kind and the command id together, and the re-read plants (N4, X21, K9, K15) need the disturbance to move the cell in the call.

| # | Function | Plant | Refused |
|---|---|---|---|
| K1 | `BossEngineer_Dispatch` | the table one entry on | 8000 rounds |
| K2 | `BossEngineer_Dispatch` | the word not passed on | 1284 rounds |
| K3 | `BossEngineer_Enter` | the hook of kind 4 | 8000 rounds |
| K4 | `BossEngineer_Enter` | the hit animation table | 8000 rounds |
| K5 | `BossEngineer_ActDispatch` | the table one entry on | 8000 rounds |
| K6 | `BossEngineer_HitDispatch` | the table one entry on | 8000 rounds |
| K7 | `BossEngineer_HitStart` | animation 5 | 8000 rounds |
| K8 | `BossEngineer_HitStart` | the wait 0x3D | 8000 rounds |
| K9 | `BossEngineer_HitStart` | Sprite_Current read before the call | 316 rounds |
| K10 | `BossEngineer_HitStep` | fight 4 for 5 | 1597 rounds |
| K11 | `BossEngineer_HitStep` | the step 0x10000 | 2584 rounds |
| K12 | `BossEngineer_HitStep` | the shift count masked to 3 bits | 318 rounds |
| K13 | `BossEngineer_HitStep` | 0x904AE8 bit 3 | 1899 rounds |
| K14 | `BossEngineer_HitStep` | the first animation table | 2584 rounds |
| K15 | `BossEngineer_HitStep` | Sprite_Current of before the calls for +5 / +2 | 188 rounds |
| K16 | `BossEngineer_HitStep` | the flip inverted | 2583 rounds |
| K17 | `BossEngineer_HitStep` | the elevation of (z, x) | 2584 rounds |
| K18 | `BossEngineer_HitStep` | no tick on the count path | 5416 rounds |
| K19 | `BossEngineer_Hook` | the word's low byte passed on | 4021 rounds |
| W1 | `BossWorker_Dispatch` | the table one entry on | 8000 rounds |
| W2 | `BossWorker_Enter` | kind 5's hook | 8000 rounds |
| W3 | `BossWorker_Enter` | kind 5's cue table | 8000 rounds |
| W4 | `BossWorker_Hook` | the word's low byte passed on | 4066 rounds |
| O1 | `BossOperator_Dispatch` | the table one entry on | 8000 rounds |
| O2 | `BossOperator_Enter` | +8 = 1 | 7948 rounds |
| O3 | `BossOperator_Enter` | kind 4's hook | 8000 rounds |
| O4 | `BossOperator_Enter` | state 3 | 7954 rounds |
| O5 | `BossOperator_Hook` | the word's low byte passed on | 4064 rounds |
| O6 | `BossOperator_HookHit` | the word +6 | 8000 rounds |
| O7 | `BossOperator_HookHit` | the source block 0x904B4C | 7941 rounds |
| T1 | `BossTorast_Dispatch` | the table one entry on | 8000 rounds |
| T2 | `BossTorast_Enter` | kind 9's hook | 8000 rounds |
| T3 | `BossTorast_Hook` | the word's low byte passed on | 4066 rounds |
| T4 | `BossKassen_Dispatch` | the table one entry on | 8000 rounds |
| T5 | `BossKassen_Enter` | kind 10's hook | 8000 rounds |
| T6 | `BossKassen_Hook` | the word's low byte passed on | 4066 rounds |
| T7 | `BossGaltel_Dispatch` | the table one entry on | 8000 rounds |
| T8 | `BossGaltel_Enter` | the cue table as the animation table | 8000 rounds |
| T9 | `BossGaltel_Hook` | the word's low byte passed on | 4066 rounds |
| T10 | `BossDoksen_Dispatch` | the table one entry on | 8000 rounds |
| T11 | `BossDoksen_Enter` | kind 8's hook | 8000 rounds |
| T12 | `BossDoksen_Hook` | the word's low byte passed on | 4066 rounds |
| S1 | `Boss04_Setup` | set-up 5's event hook | 8000 rounds |
| S2 | `Boss05_Setup` | set-up 4's end hook | 8000 rounds |
| S3 | `Boss06_Setup` | set-up 4's exit hook | 8000 rounds |
| E1 | `Boss04_Event` | the phase code plus one | 2313 rounds |
| E2 | `Boss05_Event` | the phase code plus one | 2318 rounds |
| E3 | `Boss06_Event` | the phase code plus one | 2327 rounds |
| E4 | `Boss04_Event` | phase 0 sets bit 1 | 133 rounds |
| E5 | `Boss04_Event` | round flag bit 0x20 | 95 rounds |
| E6 | `Boss04_Event` | command kind 5 | 291 rounds |
| E7 | `Boss04_Event` | command id 0x79 | 275 rounds |
| E8 | `Boss04_Event` | the id byte, not the word | 27 rounds |
| E9 | `Boss05_Event` | enemy 1's +0xB0 = 2 | 260 rounds |
| E10 | `Boss05_Event` | enemy 0's HP 0 | 260 rounds |
| E11 | `Boss06_Event` | phase 1 keeps the leader bit | 141 rounds |
| E12 | `Boss06_Event` | the leader down at HP 1 | 57 rounds |
| E13 | `Boss04_Event` | phase 5 target 0x41 | 335 rounds |
| E14 | `Boss05_Event` | phase 5 without its test | 183 rounds |
| E15 | `Boss06_Event` | phase 6 sets bit 2 | 574 rounds |
| E16 | `Boss04_Event` | the phase code masked to 3 bits | 557 rounds |
| E17 | `Boss04_Event` | phase 3 as phase 4 | 400 rounds |
| E18 | `Boss04_Event` | the actor 1 | 295 rounds |
| N1 | `Boss04_End` | 0x92BF18 = 6 | 3936 rounds |
| N2 | `Boss05_End` | 0x92BF18 = 9 | 4022 rounds |
| N3 | `Boss04_End` | 0x904AE5 |= 0x40 | 2994 rounds |
| N4 | `Boss05_End` | the step written before the call | 78 rounds |
| N5 | `Boss04_End` | the step up by 2 | 3973 rounds |
| N6 | `Boss05_End` | bit 1 for the loss | 3947 rounds |
| N7 | `Boss07_End` | the run 7 | 3951 rounds |
| N8 | `Boss07_End` | 0x904AE8 |= 0x18 | 2291 rounds |
| N9 | `Boss07_End` | Music_Track 5 | 3978 rounds |
| N10 | `Boss07_End` | Draw_PassFlags 1 | 3978 rounds |
| N11 | `Boss07_End` | 0x92BF18 = 3 on the other way | 3978 rounds |
| N12 | `Boss07_End` | the step 0x33 on the loss | 3944 rounds |
| N13 | `Boss13_End` | variable 3 = 0x31 | 8000 rounds |
| N14 | `Boss13_End` | 0x92BF18 = 4 | 4878 rounds |
| N15 | `Boss13_End` | the win the other way out | 3122 rounds |
| N16 | `Boss13_End` | bit 0 for the win | 4042 rounds |
| N17 | `Boss08_End` | variable 3 = 0x12 | 3184 rounds |
| N18 | `Boss09_End` | variable 3 = 0x21 | 3184 rounds |
| N19 | `Boss10_End` | variable 3 = 0x31 | 3184 rounds |
| N20 | `Boss08_End` | bit 0 for the win | 4060 rounds |
| N21 | `Boss09_End` | the step 3 on the other way | 4816 rounds |
| X1 | `Boss04_Exit` | the flip 0 | 3928 rounds |
| X2 | `Boss05_Exit` | tags 3 and 4 | 8000 rounds |
| X3 | `Boss04_Exit` | bank 0x5E | 3928 rounds |
| X4 | `Boss05_Exit` | the pose copied (mode 0) | 4047 rounds |
| X5 | `Boss04_Exit` | +0x48 = 1 | 3921 rounds |
| X6 | `Boss05_Exit` | the actor of Find kept past the calls | 580 rounds |
| X7 | `Boss04_Exit` | +0x5A from +0x58 | 3928 rounds |
| X8 | `Boss05_Exit` | the loss clears one actor twice | 3953 rounds |
| X9 | `Boss06_Exit` | actor 5 the flip 0 | 3966 rounds |
| X10 | `Boss06_Exit` | z from y | 3966 rounds |
| X11 | `Boss06_Exit` | the loss clears actor 4 twice | 4034 rounds |
| X12 | `Boss06_Exit` | enemy 0's place | 3966 rounds |
| X13 | `Boss06_Exit` | actor 5 from enemy 0 | 3966 rounds |
| X14 | `Boss07_Exit` | actor 2 | 8000 rounds |
| X15 | `Boss08_Exit` | the flip set | 7971 rounds |
| X16 | `Boss09_Exit` | no flip | 7971 rounds |
| X17 | `Boss10_Exit` | actor 3 | 8000 rounds |
| X18 | `Boss08_Exit` | bank 0x84 | 8000 rounds |
| X19 | `Boss09_Exit` | animation 1 | 8000 rounds |
| X20 | `Boss10_Exit` | enemy 1's words | 8000 rounds |
| X21 | `Boss09_Exit` | the flip before the bank | 348 rounds |
| S4 | `Boss07_Setup` | set-up 13's end hook | 8000 rounds |
| S5 | `Boss13_Setup` | the event hook BareRet | 8000 rounds |
| S6 | `Boss08_Setup` | set-up 9's exit hook | 8000 rounds |
| S7 | `Boss09_Setup` | set-up 10's end hook | 8000 rounds |
| S8 | `Boss10_Setup` | set-up 8's exit hook | 8000 rounds |
| F1 | `Boss07_Event` | 0x904AE8 |= 6 | 109 rounds |
| F2 | `Boss07_Event` | 0x11 bytes copied | 260 rounds |
| F3 | `Boss07_Event` | the banner timer 0x1F | 260 rounds |
| F4 | `Boss07_Event` | 0x8031F3 = 2 | 260 rounds |
| F5 | `Boss07_Event` | animation 0x2D | 260 rounds |
| F6 | `Boss07_Event` | the countdown 0x3B | 260 rounds |
| F7 | `Boss07_Event` | al 0xFE | 306 rounds |
| F8 | `Boss07_Event` | phase 2 by bit 3 | 364 rounds |
| F9 | `Boss07_Event` | member 1 the sprite | 247 rounds |
| F10 | `Boss07_Event` | phase 5 | 260 rounds |
| F11 | `Boss07_Event` | phase 4 does phase 6 | 423 rounds |
| F12 | `Boss07_Event` | the step 1 | 260 rounds |
| F13 | `Boss07_Event` | the countdown not stored | 377 rounds |
| F14 | `Boss07_Event` | the leader bit kept on phase 1 | 140 rounds |

## 5. What nothing reached

Nothing of the 52 went unreached by the fuzz: every branch the controls
planted in was taken in thousands of rounds but `Boss07_Event`'s phase 1
(the banner; 247 rounds of 8,000 - the command test's three conditions and
the phase code together) and kind 3's hit step's zero path (about a third of
its rounds). What only a fight would show: which enemy runs which kind's
script, what the banner at `0x65D008` says, what the end hooks' chapter step
and `0x92BF18` / `0x903848` values select in the field code that reads them,
and whether a scene can lack an actor its exit hook finds by tag
([`boss_h.md`](boss_h.md) section 6).

## 6. Latent defects (Capcom's, kept)

- **The dispatchers index unchecked**: a state byte past 12 (the kinds'
  `+1`), 6 or 3 (kind 3's `+2`), or a hook word past 2 jumps through the
  dword after the table - the next table's first pointer, or a byte table's
  bytes. Ours aborts. Every table of the group is followed directly by
  another table, so a stray state would run another table's handler rather
  than fault at once.
- **The exit hooks write through `BossActor_Find`'s answer untested**
  (`Sprite_Current = Find(tag)` then stores at `+0x48`, `+0x2A`, `+0x58`,
  `+0x5A`, `+0x34..+0x3C`): with no actor carrying the tag the original
  writes near address 0 - BH's spawn writers' defect ([`boss_h.md`](boss_h.md)
  section 6) in the set-ups' own code (it faults first inside
  `Sprite_SetAnimationBank`, which reads `Sprite_Current`). Ours aborts with
  a `Fatal` naming the hook and the tag (`FindActor`), as BH's spawn writers
  do; the stand-in never answers null, so the path is not fuzzed.
- **Kind 3's shift** `1 << (+5 & 0x1F)` into a byte: a slot `+5` of 8 or more
  sets no bit in `0x904AAD`. Enemy slots are 0..7, so only a corrupt `+5`
  reaches it; kept.
- **The event hooks read the command `[0x904B40]` without a test**: phase 1
  dereferences it whenever the actor is 0 and the kind 4. Whether the
  engine always sets it before phase 1 was not read; a null or stale
  pointer there would fault or read garbage. Ours reads it as the original
  does.
- **`Boss08_Exit` .. `Boss10_Exit` copy enemy 0's pose words onto actors 0,
  1 and 2 alike** (`0x93B9B8` / `0x93B9BA` in all three), where the set-ups
  4..6 pose each actor from its own enemy. Kept as read; whether that is
  intended (enemy 0 may be each fight's only enemy) is the formation data's,
  not read here - a question for the owner, not a defect until answered.

## 7. Calls across groups

**Out of BSB, to code nobody owns** (raw, `boss_sb_callees.h`): `0x446DE0`,
`0x446E00`, `0x446E20` (the end phase's steps; the harness's standard set
lists them). Everything else BSB calls is ours by name: BH's `BossActor_*`,
the engine's `Sprite_*`, `BattleEnemy_SetAnimation`, `AreaMap_Elevation`,
`Str_CopyN`, `BattleBanner_Add`. No call reaches another stage-B group's
function; the tables hold BH's (`BossOp_Death`, `BossOp_ScriptTick`,
`BareRet`, `BossTorast_ActDispatch`) and engine entries, reached as read.

**Into BSB from outside the group** (for the rebinding pass):

- Capcom's `.data`: `Boss_SetupTable[4..10, 13]` and `BossKind_Table[3, 4,
  5, 8..11]` hold the set-ups and dispatchers; the kinds' own tables hold
  their states and hooks; all reach ours through each function's `jmp`.
- The hooks each set-up and state 0 stores are literals in ours (the
  original's immediates).
- `boss_h_fuzz.cpp` drives kinds 8..11's dispatchers (`0x438E50`, `0x4390F0`,
  `0x439160`, `0x4391D0`, its `kKindDispatch`) and kind 8's hook dispatcher
  (`0x4390E0`, the `BareRet` via) by raw address as `Clone::via`
  dispatchers. They run before `BossSb_Inject` (BH's module injects and
  tests first), so they reach Capcom's; after the rebinding pass they could
  name `BossTorast_Dispatch` .. `BossDoksen_Dispatch` and `BossTorast_Hook`.
  Ours of each dispatcher reaches a planted cell the same way (it reads the
  cell and calls it), so the via works either way.
- `battle_flow_callees.h`'s comment on `kEventHook` names `0x438450`
  (`Boss04_Event`) as an example; a comment only.

## 8. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 52 extents as the
tables above give them, under a comment line. None was there before; two
(`0x439FE0`, `0x43A000`) fall inside the host line `004399E0 16F0`, which
their smaller extents now cut.
