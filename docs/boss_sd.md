# Group BSD: fights 17..21 and kinds 18, 21..27 (chapter 5)

**Status:** MEASURED (2026-09-28) - round eleven
([`takeover-queue-bosses.md`](takeover-queue-bosses.md)), wave one, stage B.
52 functions ours (`src/game/boss_sd.cpp`, shadow `boss_sd`), each read to
its last instruction with capstone and fuzzed through the boss harness
([`boss_harness.md`](boss_harness.md)), one `Run` per unit: 0 mismatches in
@@ROUNDS@@ rounds. @@CONTROLS@@ Fuzz only: no recorded route reaches a boss
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

@@RUNS@@

**Seeds.** The dispatchers: the harness draws the state byte below its
table; the seed puts the other state bytes inside it too, so a dispatcher
reading the wrong byte lands on another entry (a count) rather than past
the table (a Fatal). `BossDodai_Dispatch` calls `Flags_Test` before it
dispatches, and the disturbance may point `Sprite_Current` at another
enemy: its seed puts every enemy's `+1` below 12 and the Run's
`phase_span` is 12 (the first run without it faulted on both sides, the
original first - the harness's section 6 trap, met through a re-read). The
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
state byte the harness did not draw - seed every enemy's state byte, and
set `phase_span`.

## 5. Controls

`python controls.py` (the group's scratch script): each control one textual
change to `boss_sd.cpp` anchored on a string that occurs once, then rebuild,
run (`BOF3X_SHADOW=boss_sd`, `BOF3X_BSD_RUN` the unit), restore; one rebuild
at the end. The rounds column is the planted function's mismatched rounds
(of 6,000).

@@CONTROLS_TABLE@@

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
