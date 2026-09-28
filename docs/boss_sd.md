# Group BSD: fights 17..21 and kinds 18, 21..27 (chapter 5)

**Status:** MEASURED (2026-09-28) - round eleven
([`takeover-queue-bosses.md`](takeover-queue-bosses.md)), wave one, stage B.
52 functions ours (`src/game/boss_sd.cpp`, shadow `boss_sd`), each read to
its last instruction with capstone and fuzzed through the boss harness
([`boss_harness.md`](boss_harness.md)), one `Run` per unit: 0 mismatches in
312,000 rounds. 115 controls planted: 114 refused by a count, one equivalent with its near variant refused (section 5). Fuzz only: no recorded route reaches a boss
fight. No divergence.

Enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's area
records), not memory of the game; which fight a set-up is comes from the
tool's rows and the code, section 1.

## 1. The units

`analysis/boss_funcs.tsv`, group column `BSD`: 52 functions in 13 units, all
taken. **No start dropped, none added**: every start is a function, the
tool's extents are each function's last instruction (every gap `nop`
padding), and its clone rows match the reading call site for call site. Two
functions are shared: `0x43AB50` (kinds 24 and 26's hook tables) and
`0x43B730` (the end hook set-ups 21 and 23 both install; 23 is BSE's). The
tool's table extents read to the next named address, as
[`boss_harness.md`](boss_harness.md) section 6 says: "70 code entries" for
kind 21's `+1` table is 12; the counts below are the dispatchers' (every
kind's `+1` table is EnemyOp_Steps' twelve, every `+2` table
EnemyOp_ActSubs' six, every hook table three - the next table's address
follows each).

| Unit | Fight / enemy (the tool) | Functions | Sibling's image |
|---|---|--:|---|
| K18 | Mutant, area 52 | 4 | BOSS017 |
| B17 | id 17: Mutant, area 52 row 7 | 2 | BOSS017 |
| K21, K22, K23 | Claw, Cawer, Patrio, area 79 | 3 each | BOSS018 |
| K26 | Dodai 1 / Dodai 2, area 79 | 9 (with `0x43AB50`) | BOSS018 |
| B18 | id 18: Claw, Cawer, Patrio, Dodai 1 / 2, area 79 row 7 | 4 | BOSS018 |
| B19, B20 | ids 19, 20: rows 6 and 5 - the tool leaves their kinds open | 4 each | BOSS019, BOSS020 |
| K24, K25 | Emitai, Golem, area 81 | 5 (without `0x43AB50`), 3 | BOSS021 |
| B21 | id 21: Emitai, Golem, area 81 row 7 | 4 | BOSS021 |
| K27 | Garr, area 80 (its set-up 22 is BSE's) | 4 | BOSS022 |

**Set-ups 19 and 20** (plan section 7's open ones): the sibling's
`docs/loader_records/BOSS.md` section 5.3 finds `BOSS018 = BOSS019 =
BOSS020` by section md5 - one image, "a shared 3-boss build" - and the code
agrees: their hooks are set-up 18's instruction for instruction except the
tag of the actor the exit hook places (0, 1, 2) and the chapter step the end
hook hands back on the win (section 2.3). So 19 and 20 are rows 6 and 5 of
the fight whose row 7 is set-up 18 - area 79's by the image; the area is not
proven for rows 6 and 5 (the tool finds no kinds of their own), and which
moment of the story each is was not read. **The size check**: the sibling's
`BOSS018` has 37 roots (`analysis/overlay_captures_all.json`); the PC's
set of it is B18's 4 + B19's 4 + B20's 4 + kinds 21, 22, 23, 26's 18 + BH's
five that only these units reach (`BossOp_EnterTick`,
`BossHook_RetargetMember0`, `BossMap_SetCorners`, `Boss_SetByLeaderId`,
`BossMap_UpdateFromEnemies`) = 35.

## 2. What each function does

### 2.1 The kinds

Every kind is the generic enemy's state machine (`EnemyOp_Steps`,
[`enemy_ai_ops.md`](enemy_ai_ops.md)) with the kind's own entrance at step
0 and, for some, its own entries elsewhere. The dispatchers jump through the
kind's `.data` table by a state byte of `Sprite_Current`; the hooks through
a three-entry table by the low byte of their word (0 the action pick, 1 the
hit, 2 `BattleEnemy_RunAll`'s call).

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43A590` | `BossMutant_Dispatch` | 0x12 | `BossKind_Table[18]`: `jmp [BossMutant_Steps + 4 * +1]` |
| `0x43A5B0` | `BossMutant_Enter` | 0x3D | step 0: 0x939AD8's `+0xFC` = `0x64CE74`, `+0xF4` = `BossMutant_Hook`, `+0xF8` = `0x64CE98` (0x939AD8 read again for each), `+1` = 2, `jmp Sprite_ScriptTick` |
| `0x43A5F0` | `BossMutant_ActDispatch` | 0x12 | step 6: `jmp [BossMutant_ActSubs + 4 * +2]` (EnemyOp_ActSubs with `BossOp_Death` at 4) |
| `0x43A610` | `BossMutant_Hook` | 0x10 | `+0xF4`: `jmp [BossMutant_Hooks + 4 * (word & 0xFF)]` (three `BareRet`) |
| `0x43A660` / `0x43A770` / `0x43A840` | `BossClaw_Dispatch` / `BossCawer_Dispatch` / `BossPatrio_Dispatch` | 0x12 | kinds 21, 22, 23 by `+1` |
| `0x43A680` / `0x43A790` | `BossClaw_Enter` / `BossCawer_Enter` | 0x95 | step 0: `Flags_Test(chapter bits, 0x35 / 0x36)`: set - 0x939AD8's `+0xA4` and `+0xA6` = the words `0x939A1A` / `0x939A14`; clear - `Flags_Set` of it. Then the three stores, `+1` = 1, `Sprite_EnsureAnimation(2)`, `jmp Sprite_ScriptTick` (step 1 is BH's `BossOp_EnterTick`) |
| `0x43A860` | `BossPatrio_Enter` | 0x8B | the same with flag 0x37, `+1` = 2 and no animation call |
| `0x43A740` / `0x43A830` / `0x43A8F0` | `BossClaw_Hook` / `BossCawer_Hook` / `BossPatrio_Hook` | 0x10 | three tables of the same entries: BH's `BossHook_RetargetMember0`, `BareRet`, `BareRet` |
| `0x43A900` | `BossDodai_Dispatch` | 0x8B | `BossKind_Table[26]`, every frame: 0x939AD8's `+0xFC` by `Sprite_Current +5` and a flag - `+5 == 4`: flag 0x34 ? `0x64CF34` : `0x64CF28`; else flag 0x33 ? `0x64CF4C` : `0x64CF40` - then (Sprite_Current read again) by `+1` through `BossDodai_Steps` |
| `0x43A990` | `BossDodai_Enter` | 0xC0 | step 0: flag 0x1E, or (asked only when it is clear) 0x23: 0x939AD8's `+0xA4` = the word `0x939A18` (`+5 == 4`) or `0x939A16`. Then `+5 == 5`: `+8 |= 2`, `+0xFC` = `0x64CF40`; else `0x64CF28`; `+0xF4` = `BossDodai_Hook`, `+0xF8` = `0x64CF70`, `+1` = 2, `jmp Sprite_ScriptTick` |
| `0x43AA50` | `BossDodai_ActDispatch` | 0x12 | step 6 by `+2` |
| `0x43AA70` | `BossDodai_Death` | 0x88 | action 4: `Sprite_SetAnimation(4)`, `Battle_EnemyDefeated`, `BossMap_SetCorners(+5 == 4 ? 1 : 0, 2)`, enemy 0's and the leader's ground words `+0x3E` = `AreaMap_Elevation(+0x34, +0x38)`, then `+0 &= 0xBF`, states 3, 0, 0 |
| `0x43AB00` | `BossDodai_HitPoseDispatch` | 0x12 | step 11 (where `EnemyOp_Steps` has `EnemyOp_HitPose`): by `+2` through `BossDodai_HitPoseSubs` (3) |
| `0x43AB20` | `BossDodai_HitShake` | 0x1A | its entry 0: `Sprite_Current`'s dword `+0x38 += (Frame_Counter & 1) << 9` |
| `0x43AB40` | `BossDodai_Hook` | 0x10 | `BossHook_ActKindNone`, `BossDodai_HitSound`, `BareRet` |
| `0x43AB50` | `BossHook_ActKindNone` | 8 | hook entry 0 of kinds 24 and 26: the action kind `0x904B35` = 0 |
| `0x43AB60` | `BossDodai_HitSound` | 0x32 | hook entry 1 (the hit): with 0x939AD8's word `+0x108` above 0 (signed), `Sound_PlayById(Rand() & 1 ? 0x600 : 0x601)` |
| `0x43B280` / `0x43B370` / `0x43B4B0` | `BossEmitai_Dispatch` / `BossGolem_Dispatch` / `BossGarr_Dispatch` | 0x12 | kinds 24, 25, 27 by `+1` |
| `0x43B2A0` / `0x43B390` | `BossEmitai_Enter` / `BossGolem_Enter` | 0x3D | `BossMutant_Enter`'s shape with the kind's tables |
| `0x43B2E0` / `0x43B530` | `BossEmitai_ActDispatch` / `BossGarr_ActDispatch` | 0x12 | step 6 by `+2` |
| `0x43B300` | `BossEmitai_Death` | 0x5B | action 4: `Sprite_EnsureAnimation(4)`, `+0x2A` = 0, then `BossOp_Death`'s body (`Sprite_ScriptTickOnce`, `Battle_EnemyDefeated`, `+0x110 |= 0x1000`, `+0 &= 0xBF`, states 3, 0, 0) |
| `0x43B360` / `0x43B3D0` / `0x43B5A0` | `BossEmitai_Hook` / `BossGolem_Hook` / `BossGarr_Hook` | 0x10 | Emitai's `BossHook_ActKindNone`, `BareRet`, `BareRet`; Golem's and Garr's three `BareRet` |
| `0x43B4D0` | `BossGarr_Enter` | 0x57 | the three stores, `+1` = 1, `Sprite_SetAnimationBank(0x153)`, `+0x2A` = 0 (Sprite_Current read after), `Sprite_SetAnimation(0)`; step 1 is `BossOp_EnterTick` |

### 2.2 Set-ups 17 and 21

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43A620` | `Boss17_Setup` | 0x1F | `BattleHook_End` = `Boss17_End`, `_Exit` = BH's `BossHook_ExitActor0Bit40`, `_Event` = `BareRetZero` |
| `0x43A640` | `Boss17_End` | 0x1A | the win (`0x904AE8` bit 1): the field's move-script counter 0 (`0x903848`) = 0x14, `jmp 0x446DE0` (the end phase, step 1); else `jmp 0x446E00` (step 2) |
| `0x43B3E0` | `Boss21_Setup` | 0x1F | End `Boss21_End`, Exit `Boss21_Exit`, Event `Boss21_Event` |
| `0x43B400` | `Boss21_Event` | 0x71 | phase 3: every actor 0..10 `Battle_ActorIsOut` answers al 0 for gets bit 0x10 - members in their `+0x90`, enemies in their object's `+0x92`; al 0 |
| `0x43B480` | `Boss21_Exit` | 0x30 | `BossActor_ClearBit40(0)`; `Sprite_Current = BossActor_Find(0)`, its `+0x2A` = 0; `Sprite_SetAnimation(4)`; `BossActor_Clear(1)`, `BossActor_Clear(2)` |
| `0x43B730` | `Boss21_End` | 0x1A | `Boss17_End` with the counter 0xA; set-up 23 (BSE's, `0x43B710`) installs it too |

### 2.3 Set-ups 18, 19, 20 (one body, three copies)

| Address (18 / 19 / 20) | Names | Bytes | What it does |
|---|---|--:|---|
| `0x43ABA0` / `0x43AD60` / `0x43AF20` | `Boss18_Setup` .. | 0x1F | the three hooks below |
| `0x43ABC0` / `0x43AD80` / `0x43AF40` | `Boss18_Event` .. | 0x86 | phase 0: enemy 0's `+0x93` bit 0x40 sets `0x904AE8` bit 1; enemy 1's sets bit 1 of `0x904AAD` and `0x904AE8`; enemy 2's bit 0 of both; with `0x904AE8` not 0, `BossMap_UpdateFromEnemies`. Phase 5: `BossMap_UpdateFromEnemies`. al 0 |
| `0x43AC50` / `0x43AE10` / `0x43AFD0` | `Boss18_End` .. | 0xB2 / 0xB4 / 0xAF | the win: chapter step `0x8034E5` = 0x1A when `0x904AAD` bit 1, else 0x1B / 0x23 / 0x29; step 1. `0x904AAD` bit 0: step 2. Otherwise by flags 0x23 and 0x24: both - step 2; 0x23 alone - chapter step 0x3A, `Boss_SetByLeaderId`, step 3; neither - chapter step 0x1C, the same (set-up 20 stores the step before the call, 18 and 19 after). Then, on every path, the words `0x939A18` / `0x939A16` / `0x939A1A` / `0x939A14` = enemy 1's HP, enemy 2's, enemy 0's `+0xA4` and `+0xA6` |
| `0x43AD10` / `0x43AED0` / `0x43B080` | `Boss18_Exit` .. | 0x50 | unless enemy 0's `+0x93` bit 0x40: the actor tagged 0 / 1 / 2 takes enemy 0's place (`BossActor_CopyFrom(tag, enemy 0, 1)`) and loses bit 0x40; actors 3 and 4 take enemies 1 and 2's poses (what 0) and lose bit 0x40 |

The four words the end hooks keep are read back only by this fight's kinds
(a raw scan of the exe for the four addresses finds the end hooks' stores
and the entrances' loads, nothing else): kinds 21..23 restore their `+0xA4`
/ `+0xA6` from enemy 0's when their chapter flag (0x35..0x37) is set, kind
26 its `+0xA4` from enemy 1's or enemy 2's by its `+5` on flag 0x1E or 0x23.
What that means in the story - a fight resumed, a foe met again - is not
read here.

## 3. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original jumps through an index past its table (every
dispatcher and hook) or writes through a null (`Boss21_Exit`), ours aborts
with a `Fatal` naming the function (the owner's rule, round9 doc section 6);
nothing reaches it. Three things ours does in C that the original does in
registers, none observable:

- **The dispatchers forward one word.** The original `jmp`s leave the
  caller's `[esp + 4]` to the entry (`BattleEnemy_RunAll` pushes nothing, so
  it is its frame's word); ours takes that word as a parameter and passes it
  on, so an entry that reads it (`Port_DroppedCall`'s byte, a hook's code)
  sees the same value. The fuzz checks it (`Port_DroppedCall`'s recorder
  logs its byte; control D2).
- **`Boss21_Event` writes each actor into its own argument's low byte** and
  pushes that dword; ours passes the same dword without the write (the slot
  is the caller's pushed argument, dead after the return).
- **The event hooks answer in `al`**; ours clears eax. Every caller reads
  `al` (`ret_mask 0xFF`).

## 4. The fuzz

`BOF3X_SHADOW=boss_sd`, `src/game/boss_sd_fuzz.cpp`, one `Run` per unit
(`BOF3X_BSD_RUN=K18|B17|...` runs one), 6,000 rounds a function. The kinds'
`+1`, `+2` and hook tables are `DataTable`s (21 tables; the hook
tables with one argument word); the three BH functions the set-ups call
directly (`BossMap_UpdateFromEnemies`, `Boss_SetByLeaderId`,
`BossMap_SetCorners`) are the group's callees; the move-script counters
`0x903848..0x90384B` and the kept words `0x939A14..0x939A1B` are the group's
regions.

| Run | Fight, kind | Clones | Rounds | Calls (the originals', this worktree) | Result |
|---|---|--:|--:|--:|---|
| K18 | 17, 18 | 4 | 24,000 | 24,000 | 0 mismatches |
| B17 | 17 | 2 | 12,000 | 6,000 | 0 mismatches |
| K21 | 18, 21 | 3 | 18,000 | 31,972 | 0 mismatches |
| K22 | 18, 22 | 3 | 18,000 | 32,043 | 0 mismatches |
| K23 | 18, 23 | 3 | 18,000 | 26,033 | 0 mismatches |
| K26 | 18, 26 | 9 | 54,000 | 79,841 | 0 mismatches |
| K24 | 21, 24 | 5 | 30,000 | 42,000 | 0 mismatches |
| B18 | 18 | 4 | 24,000 | 42,712 | 0 mismatches |
| B19 | 19 | 4 | 24,000 | 42,691 | 0 mismatches |
| B20 | 20 | 4 | 24,000 | 42,581 | 0 mismatches |
| K25 | 21, 25 | 3 | 18,000 | 18,000 | 0 mismatches |
| B21 | 21 | 4 | 24,000 | 73,928 | 0 mismatches |
| K27 | 22, 27 | 4 | 24,000 | 30,000 | 0 mismatches |

312,000 rounds. **Coverage** (the originals' calls): every entry of every
`DataTable` (`phase 0x...` about 500 each for a step table's entries, about
1,000 for an action table's, 2,000..6,000 for the hook tables'), and every
callee - e.g. K26: `BossMap_SetCorners` 6,000, `AreaMap_Elevation` 12,000,
`Flags_Test` 14,019, `Rand` / `Sound_PlayById` 2,911, `Port_DroppedCall`
1,053; B18: `BossMap_UpdateFromEnemies` 3,885, `Boss_SetByLeaderId` 846,
`0x446DE0` 3,247, `0x446E00` 1,907, `0x446E20` 846, `BossActor_CopyFrom` /
`_ClearBit40` 14,732, `Flags_Test` 2,517; B21: `Battle_ActorIsOut` 37,928,
`BossActor_Find` 6,000, `BossActor_Clear` 12,000. Counts are this
worktree's (they move with the build directory; judge by 0 mismatches).

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the final
build): exit 0, 839 `Run`s, 0 mismatches; it passed first time (no silent
death).

**Seeds.** The dispatchers: the harness draws the state byte below its
table; the seed puts the other state bytes inside it too, so a dispatcher
reading the wrong byte lands on another entry (a count) rather than past
the table (a Fatal). `BossDodai_Dispatch` calls `Flags_Test` before it
dispatches, and the disturbance may point `Sprite_Current` at another
enemy: its seed puts every enemy's `+1` below 12, the Run's `phase_span`
is 12, and its `settle` puts every enemy's `+1` back below 12 after each
disturbance (the first run without them faulted on both sides, the original
first; the second, with only the seed and `phase_span`, had ours abort in a
round where the standard disturbance's "a byte of the current enemy's
record" wrote `+1` of the object `Sprite_Current` points at - that case does
not honour `phase_span`). `Boss_SetByLeaderId`'s stand-in is louder than the
real one: it also moves the chapter step `0x8034E5`, so set-up 20's order
(the step stored before the call, 18 and 19's after) is seen in every round
that reaches it (control D88: 17 rounds without, 765 with). The
hooks: the harness's word 0..2, with garbage above the byte half the time
(the entries receive the whole word). The event hooks: the codes they test
(0 and 5; 3 for set-up 21) half the time, garbage above the byte half the
time. The set-ups: `0x904AE8` and `0x904AAD` at 0..3, 0xFD, 0x82, 0xFE or any;
each enemy's `+0x93` with and without 0x40. Kind 26: `+5` at 4, 5 and
neighbours; the signed word `+0x108` at 0, 1, 0xFFFF, 0x7FFF, 0x8000; `+0x38`
near a carry. The kinds 21..23: the kept words at their ends. **The group's
disturbance** (`Disturb`) moves what the standard one does not and the
functions read after a call or must be seen not to: `0x904AAD`, the three
enemies' status bytes, enemies 0 and 1's HP, the kept words, the chapter's
flag bits pointer, `Sprite_Current +5`.

**The harness, for the coordinator.** BSD is the first user of `kSetup`: it
needed nothing more (a set-up's stores land in the compared battle bytes and
the logged hooks; a swapped hook is refused in every round, controls D9,
D10, D75..D77, D103). The shapes BSD used: `kSetup`, `kEnd`, `kExit`,
`kEvent`, `kDispatch`, `kState`, `kEnemyHook`; nothing added to the harness
in the group's file. One thing a stage-B group should know: a `kDispatch`
that calls out before it dispatches re-reads `Sprite_Current` after the
call, and the standard disturbance re-points it at another enemy whose
state byte the harness did not draw - seed every enemy's state byte, set
`phase_span`, and `settle` the state bytes, because the standard
disturbance's case 11 (a byte of the current enemy's record) writes `+1..+4`
of that enemy past `phase_span`. Worth folding into the harness (case 11
could skip `+1..+4`, or honour `phase_span` as the field case does).

## 5. Controls

`python controls.py` (the group's scratch script): each control one textual
change to `boss_sd.cpp` anchored on a string that occurs once, then rebuild,
run (`BOF3X_SHADOW=boss_sd`, `BOF3X_BSD_RUN` the unit), restore; one rebuild
at the end. The rounds column is the planted function's mismatched rounds
(of 6,000).

| # | Function | Plant | Refused |
|---|---|---|---|
| D1 | `BossMutant_Dispatch` | by +2 | 5378 rounds |
| D2 | `BossMutant_Dispatch` | the word not forwarded (0) | 1039 rounds |
| D3 | `BossMutant_Enter` | +0xFC and +0xF8 swapped | 6000 rounds |
| D4 | `BossMutant_Enter` | kind 21's hook stored | 6000 rounds |
| D5 | `BossMutant_Enter` | +1 = 1 | 5970 rounds |
| D6 | `BossMutant_Enter` | the tick's answer dropped | 4021 rounds |
| D7 | `BossMutant_ActDispatch` | by +3 | 4722 rounds |
| D8 | `BossMutant_Hook` | the word's low byte only forwarded | 2976 rounds |
| D9 | `Boss17_Setup` | exit hook BossHook_ExitClearActor0 | 6000 rounds |
| D10 | `Boss17_Setup` | end and exit swapped | 6000 rounds |
| D11 | `Boss17_End` | counter 0x15 | 3235 rounds |
| D12 | `Boss17_End` | the win by bit 0 | 3801 rounds |
| D13 | `Boss17_End` | counter 1 (0x903849) | 3235 rounds |
| D14 | `Boss21_End` | counter 0xB | 3277 rounds |
| D15 | `BossClaw_Enter` | flag 0x34 | 6000 rounds |
| D16 | `BossClaw_Enter` | animation 3 | 6000 rounds |
| D17 | `BossCawer_Enter` | +1 = 2 | 5940 rounds |
| D18 | `BossPatrio_Enter` | +0xF8 kind 22's | 6000 rounds |
| D19 | `BossClaw_Enter` | +0xA6 from 0x939A1A | 4016 rounds |
| D20 | `BossClaw_Enter` | 0x939AD8 read before the Flags_Test | 131 rounds |
| D21 | `BossCawer_Enter` | Flags_Clear for Flags_Set | 1984 rounds |
| D22 | `BossPatrio_Enter` | the kept words not restored | 4011 rounds |
| D23b | `BossClaw_Hook` | the next entry (mod 3) | 6000 rounds |
| D24 | `BossCawer_Hook` | the word's low half forwarded | 2965 rounds |
| D25 | `BossPatrio_Hook` | the word + 0x100 | 6000 rounds |
| D26c | `BossClaw_Dispatch` | by +3 | 5480 rounds |
| D27 | `BossCawer_Dispatch` | the word not forwarded | 451 rounds |
| D28 | `BossPatrio_Dispatch` | by +4 | 5439 rounds |
| D29 | `BossDodai_Dispatch` | +5 == 5 | 2084 rounds |
| D30 | `BossDodai_Dispatch` | flag 0x35 | 1037 rounds |
| D31 | `BossDodai_Dispatch` | the other slot's tables swapped | 4963 rounds |
| D32 | `BossDodai_Dispatch` | Sprite_Current of before the call | 225 rounds |
| D33 | `BossDodai_Enter` | flag 0x23 not asked | 2039 rounds |
| D34 | `BossDodai_Enter` | the kept HP swapped | 5305 rounds |
| D35 | `BossDodai_Enter` | +8 |= 1 | 728 rounds |
| D36 | `BossDodai_Enter` | +5 == 4 for the +0xFC | 1887 rounds |
| D37 | `BossDodai_Enter` | Sprite_Current of before the calls | 44 rounds |
| D38 | `BossDodai_Enter` | +0xF8 0x64CF74 | 6000 rounds |
| D39 | `BossDodai_ActDispatch` | by +3 | 4631 rounds |
| D40 | `BossDodai_Death` | value 1 | 6000 rounds |
| D41 | `BossDodai_Death` | cells swapped | 6000 rounds |
| D42 | `BossDodai_Death` | defeat before the animation | 6000 rounds |
| D43 | `BossDodai_Death` | enemy 0's x and z swapped | 6000 rounds |
| D44 | `BossDodai_Death` | the leader's ground a dword | 6000 rounds |
| D45 | `BossDodai_Death` | +1 = 4 | 6000 rounds |
| D46 | `BossDodai_Death` | +0 &= 0x3F | 3025 rounds |
| D47 | `BossDodai_HitPoseDispatch` | by +1 | 2653 rounds |
| D48b | `BossDodai_HitShake` | << 8 | 2978 rounds |
| D49b | `BossDodai_HitShake` | bit 1 of the frame | 2936 rounds |
| D50b | `BossDodai_Hook` | the next entry (mod 3) | 6000 rounds |
| D51 | `BossHook_ActKindNone` | kind 1 | 6000 rounds |
| D52 | `BossDodai_HitSound` | < 0 (0 sounds) | 656 rounds |
| D53 | `BossDodai_HitSound` | the word unsigned | 2361 rounds |
| D54 | `BossDodai_HitSound` | Rand's bit 1 | 1236 rounds |
| D55 | `BossDodai_HitSound` | the sounds swapped | 2983 rounds |
| D56 | `BossEmitai_Dispatch` | by +2 | 5426 rounds |
| D57 | `BossEmitai_Enter` | +1 = 3 | 5964 rounds |
| D58c | `BossEmitai_ActDispatch` | by +1 | 4733 rounds |
| D59 | `BossEmitai_Death` | +0x2A = 1 | 5999 rounds |
| D60 | `BossEmitai_Death` | bit 0x100 | 4491 rounds |
| D61 | `BossEmitai_Death` | animation 5 | 6000 rounds |
| D62 | `BossEmitai_Death` | Sprite_Current of before the call | 200 rounds |
| D63 | `BossEmitai_Death` | 0x939AD8 of before the calls | 309 rounds |
| D64b | `BossEmitai_Hook` | the next entry (mod 3) | 6000 rounds |
| D65 | `BossGolem_Dispatch` | the word + 1 forwarded | 955 rounds |
| D66 | `BossGolem_Enter` | the hook + 1 | 6000 rounds |
| D67 | `BossGolem_Hook` | the word's low byte only | 2965 rounds |
| D68 | `BossGarr_Dispatch` | by +2 | 5468 rounds |
| D69 | `BossGarr_Enter` | bank 0x152 | 6000 rounds |
| D70 | `BossGarr_Enter` | +0x2A before the bank | 187 rounds |
| D71 | `BossGarr_Enter` | animation 1 | 6000 rounds |
| D72 | `BossGarr_Enter` | +1 = 2 | 5940 rounds |
| D73c | `BossGarr_ActDispatch` | by +4 | 4681 rounds |
| D74b | `BossGarr_Hook` | entry 0 for every word (the three are BareRet) | NOT REFUSED - equivalent: the three entries of `BossGarr_Hooks` are `BareRet`, so which one runs is unobservable (and the lost check is only the past-3 abort); D74c, the same hook forwarding a changed word, refused |
| D74c | `BossGarr_Hook` | the word's low half forwarded | 2976 rounds |
| D75 | `Boss18_Setup` | event and exit swapped | 6000 rounds |
| D76 | `Boss19_Setup` | set-up 18's end hook | 6000 rounds |
| D77 | `Boss20_Setup` | set-up 18's exit hook | 6000 rounds |
| D78 | `Boss18_Event` | phase 4 for 5 | 2381 rounds |
| D79 | `Boss18_Event` | the whole word compared | 1913 rounds |
| D80 | `Boss18_Event` | enemy 0 by 0x2000 | 197 rounds |
| D81 | `Boss18_Event` | enemy 1 sets 0x904AAD bit 0 | 650 rounds |
| D82 | `Boss18_Event` | the update on the win bit only | 160 rounds |
| D83 | `Boss18_Event` | al 1 after phase 0 | 1925 rounds |
| D84 | `Boss19_Event` | phase ^ 5 | 2840 rounds |
| D85 | `Boss20_Event` | phase + 1 | 4294 rounds |
| D86 | `Boss18_End` | the win's steps swapped | 3283 rounds |
| D87 | `Boss19_End` | step 0x22 | 1469 rounds |
| D88 | `Boss20_End` | the step after the pick | 765 rounds |
| D89 | `Boss18_End` | the win's step by bit 0 | 2091 rounds |
| D90 | `Boss18_End` | step 2 by bit 1 | 1684 rounds |
| D91 | `Boss18_End` | flag 0x25 | 928 rounds |
| D92 | `Boss18_End` | 0x3A and 0x1C swapped | 767 rounds |
| D93 | `Boss18_End` | step 2 for step 3 | 784 rounds |
| D94 | `Boss18_End` | the kept HP words swapped | 6000 rounds |
| D95 | `Boss18_End` | +0xA6 from +0xA8 | 6000 rounds |
| D96 | `Boss18_End` | the words kept on the win only | 2594 rounds |
| D97 | `Boss18_Exit` | enemy 0 by 0x2000 | 3246 rounds |
| D98 | `Boss18_Exit` | enemy 0's pose, not place | 2635 rounds |
| D99 | `Boss18_Exit` | actor 4 from enemy 1 | 6000 rounds |
| D100 | `Boss18_Exit` | tag 1 | 2635 rounds |
| D101 | `Boss19_Exit` | tag 2 | 2635 rounds |
| D102 | `Boss20_Exit` | tag 0 | 2635 rounds |
| D103 | `Boss21_Setup` | exit and event swapped | 6000 rounds |
| D104 | `Boss21_Event` | phase 4 | 3869 rounds |
| D105 | `Boss21_Event` | members 0..1 | 3415 rounds |
| D106 | `Boss21_Event` | the members' bit 0x20 | 1970 rounds |
| D107 | `Boss21_Event` | the enemies' +0x93 | 3114 rounds |
| D108 | `Boss21_Event` | enemies 3..9 | 3415 rounds |
| D109 | `Boss21_Event` | al 1 after phase 3 | 3415 rounds |
| D110 | `Boss21_Exit` | Sprite_Current not set | 5344 rounds |
| D111 | `Boss21_Exit` | animation 5 | 6000 rounds |
| D112 | `Boss21_Exit` | actor 3 cleared | 6000 rounds |
| D113 | `Boss21_Exit` | +0x2B | 6000 rounds |
| D114 | `Boss21_Exit` | bit 0x40 of actor 1 | 6000 rounds |

Two runs, on the final seeds; the table is the second. Superseded plants of
the first run, not in the table: four hook plants that stepped the word to 3
(`code ^ 1`, `code ^ 2`) and three dispatcher plants that moved the table by
one entry but shrank its count - each hit the dispatcher's own past-the-table
`Fatal` (loud, but a Fatal proves less than a count), replaced by D23b,
D50b, D64b (the next entry mod 3), D74b / D74c, and D26c, D58c, D73c (a
different state byte); three plants that pointed a dispatcher at another
kind's table crashed on ours' side (that table's cells are not swapped in the
Run, so ours ran Capcom's entries for real) and were replaced the same way;
D48 / D49's anchor was not unique (D48b / D49b). The thinnest refusals are
the re-read plants (D20 131 rounds, D32 225, D37 44, D62 200, D63 309, D70
187: the disturbance moving `Sprite_Current` or `0x939AD8` in a call) and
D82 (160), D80 (197).

## 6. Latent defects (Capcom's, kept)

- **Every dispatcher and hook indexes its table unchecked**: a state byte
  past 12 / 6 / 3, or a hook word whose low byte is past 2, jumps through
  the dwords after the table (the next table's pointers, or the next kind's
  byte tables). Ours aborts. Nothing in the kinds' own code stores a state
  byte out of range; the generic `EnemyOp_*` entries they share set them.
- **`Boss21_Exit` writes through `BossActor_Find(0)` untested** (`+0x2A`),
  after `BossActor_ClearBit40(0)` has already written through the same
  search's answer untested - the spawn helpers' defect ([`boss_h.md`](boss_h.md)
  section 6) met once more in a caller. With no field actor tagged 0 the
  original faults; ours aborts naming the function. Whether area 81's scene
  can lack it is the area's data, not read.
- **`BossDodai_Dispatch` rewrites the enemy's `+0xFC` every frame** from a
  chapter flag, before the step it dispatches to reads it - not a defect,
  but the one kind in the group whose dispatcher does work of its own (the
  animation table follows the flag live).
- **The kept words are read without a check that they were written**: kinds
  21..23 and 26 restore HP from `0x939A14..0x939A1B` when their flag is set;
  only set-ups 18..20's end hooks write those words. Were a flag set without
  that hook having run in the same process, the kind would take whatever the
  words hold (zeros at start-up). Whether that can happen - where the flags
  are set, whether the words survive a load - was not read.

## 7. What nothing reached

Nothing of the 52 went unreached by the fuzz: the coverage lists every table
entry and every callee each clone calls. What only a fight would show: the
chapter steps the end hooks hand back (`0x1A`, `0x1B`, `0x23`, `0x29`,
`0x3A`, `0x1C`) and what the chapter does with them; the actors the exit
hooks place; the Dodai's shake and sounds; `Boss21_Event`'s bit 0x10 (what
reads it was not traced).

## 8. Calls across groups

**Out of BSD, raw** (`boss_sd_callees.h`): `0x446DE0`, `0x446E00`,
`0x446E20` (the end phase's steps; nobody owns them - the harness's standard
set lists them). Everything else BSD calls is ours and called by name: BH's
`BossMap_UpdateFromEnemies`, `Boss_SetByLeaderId`, `BossMap_SetCorners`,
`BossActor_*` and the engine's. BSD calls no function of another stage-B
group. The literals BSD stores that are BH's functions: `0x440820`
(`BossHook_ExitActor0Bit40`) and `0x43C9F0` (`BareRetZero`), set-up 17's
exit and event hooks; its tables' BH entries (`BossOp_EnterTick`,
`BossOp_Death`, `BossHook_RetargetMember0`, `BareRet`) are `.data` cells,
read as they are.

**Into BSD from outside the group** (for the rebinding pass):

- `Boss_SetupTable` entries 17..21 (`.data`, read by
  `Battle_InitBossEncounter`) and `BossKind_Table` entries 18, 21..27 (read
  by `BattleEnemy_RunAll`): cells, not code; they reach ours through the
  entries' `jmp`.
- **`0x43B730` (`Boss21_End`) from set-up 23** (`0x43B710`, group BSE's):
  it stores `BattleHook_End = 0x43B730` as a literal - the same address,
  nothing to rebind; BSE's fuzz should see that store, not call it.
- **Kind 27 (`BossGarr_*`) is set-up 22's kind** (BSE's): reached through
  `BossKind_Table[27]`, nothing to rebind.
- **BH's fuzz drives two of BSD's by raw address**: `boss_h_fuzz.cpp`'s
  `Clone::via` calls kind 21's dispatcher `0x43A660` (state 1, to reach
  `BossOp_EnterTick`) and kind 21's hook `0x43A740` (word 0, to reach
  `BossHook_RetargetMember0`), planting BH's clone in `BossClaw_Steps` /
  `BossClaw_Hooks`. BH's self-test runs before `BossSd_Inject`, so it drives
  Capcom's code; were the order to change, ours reads the same cell and
  reaches the planted clone the same way (the harness's `StandIn`). Nothing
  to rebind; the comments there call them "BSD's".
- Otherwise no code of ours names BSD's addresses (a grep of `src/` for
  `0x43A590..0x43B74A` outside `boss_sd*` finds only those two and BH's
  comment on `0x43AA70`).

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 52 extents as
section 2 gives them. None was there; two host lines covered them all
(`004399E0 16F0` and `0043B130 18BF`) and the smaller extents fix both.
