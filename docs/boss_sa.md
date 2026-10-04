# Group BSA: fights 1, 2, 3 and 39, and kinds 1, 2, 6, 7, 39 and 46

**Status:** MEASURED (2026-09-28) - round eleven
([`takeover-queue-bosses.md`](takeover-queue-bosses.md)), wave one, stage
B. 49 functions ours (`src/game/boss_sa.cpp`, shadow `boss_sa`), each read
to its last instruction with capstone and fuzzed through the boss harness
([`boss_harness.md`](boss_harness.md)) without edits to it: ten `Run`s,
0 mismatches; 139 controls planted, 139 refused, every one by a count (section 4). Fuzz only: no recorded route
reaches a boss fight.

The group's units (`tools/boss_rows.py --groups`, 2026-09-28): `K06`, `K07`,
`B01`, `K01`, `K39`, `K02`, `K46`, `B02`, `B03`, `B39` - the band from
`0x437A10` to `0x43828F`, and kind 39's ten at `0x43D3D0..0x43D662`, which
are in the group because kind 39's hook table shares a body with kind 1's
(`0x438030`). The group column of `analysis/boss_funcs.tsv` lists 49; all
49 were read, none was found inside another's extent or missing from the
list, and every extent the tool gave was right (each runs to its last
instruction; `0x437CF0`'s includes its seven-entry jump table).

The enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's
area records), not memory of the game:

| Unit | Root | The disc's | Functions |
|---|---|---|--:|
| `K06` | `BossKind_Table[6]` `0x437A10` | Gary, area 24 | 7 |
| `K07` | `BossKind_Table[7]` `0x437B70` | Mogu, area 24 | 7 |
| `B01` | `Boss_SetupTable[1]` `0x437CD0` | `BOSS001`, area 24 row 4 (kinds 6, 7) | 4 |
| `K01` | `BossKind_Table[1]` `0x437EE0` | Nue, area 23 | 8 (with `0x438030`) |
| `K02` | `BossKind_Table[2]` `0x438070` | Nue, area 22 | 4 |
| `K46` | `BossKind_Table[46]` `0x438100` | Sample 1, area 158 | 3 |
| `B02` | `Boss_SetupTable[2]` `0x438190` | `BOSS002` row 7 | 3 |
| `B03` | `Boss_SetupTable[3]` `0x438210` | `BOSS002` row 7 | 2 |
| `B39` | `Boss_SetupTable[39]` `0x438270` | `BOSS002` row 7 | 1 |
| `K39` | `BossKind_Table[39]` `0x43D3D0` | Weretigr, area 35 (fight 33's, `BOSS033`) | 10 |

**Settled 2026-09-30, fight 2 = Nue in area 23:** the owner's
`tools/recipes/cutsceneAndNue.txt` (area 23's cutscene into its Nue fight)
under a first-call trace with every entry armed (`BOF3X_CALLTRACE`,
`BOF3X_CALLTRACE_REACH=1`, `BOF3X_ORIGINAL='*'`) reaches `BossActor_Index`
from `0x437F9E` inside `BossNue_Dispatch` `0x437EE0` (kind 1) at frame 7042
and `BossActor_Clear` from `0x4381F5` inside `Boss02_Setup` `0x438190` at
frame 7180; neither `Boss03_Setup` nor `Boss39_Setup` ranges appear. So B02
is area 23's (kind 1, the first Nue the player meets), B03 area 22's (kind
2) and B39 Sample 1's, as reasoned below. *As it stood before:* the tool puts
all three on row 7 of areas 22, 23 and 158 (kinds 1, 2 and 46 precede them
in `BOSS002`), and none of the eleven functions of those three set-ups and
three kinds reads `0x904AAA`. What the code does say: fight 39 is chapter
15's register-passed id (the plan's section 5), and kind 46 (Sample 1) is
the area-158 kind, so fight 39 is the likeliest Sample 1 set-up; fights 2
and 3 are then Nue's, one each - which is which, the owner or a later reading
of the chapter's call sites settles. The names say `Boss02_*`, `Boss03_*`,
`Boss39_*` and nothing more. The sibling's
`analysis/overlay_captures_all.json` counts 23 roots for `BOSS001` (our
`B01`, `K06`, `K07`: 18 functions, the rest the shared helpers and table
entries they reach).

## 1. What each function does

`symbols.toml` gives each to the instruction. Every kind here is built on
round seven's generic enemy state (`EnemyOp_Steps`, [`enemy_ai_ops.md`](enemy_ai_ops.md)):
its dispatcher jumps through a twelve-entry table by `+1` whose entries are
the generic ones except where the kind has its own - state 0 (the kind's
entrance), 6 (an action table with `BossOp_Death` at 4), and 4, 9 or 11
where the kind scripts something. The tables' extents are the code's, not
the tool's (which reads to the next named address and so runs a kind's `+1`
table into its sub-tables).

### Kind 6 (Gary) and kind 7 (Mogu): fight 1

| Address | Name | What it does |
|---|---|---|
| `0x437A10` | `BossGary_Dispatch` | `jmp [BossGary_Steps + 4 * +1]` (12: `BossGary_Enter` at 0, `BossGary_ActDispatch` at 6, `BossGary_EndDispatch` at 11, the generic entries elsewhere, `Port_DroppedCall` at 1 and 10) |
| `0x437A30` | `BossGary_Enter` | state 0: `0x939AD8`'s `+0xFC` = `BossGary_Anims`, `+0xF4` = `BossGary_Hook`, `+0xF8` = `BossGary_Cues`; `+1` = 2; `Sprite_ScriptTick` |
| `0x437A70` | `BossGary_ActDispatch` | state 6: `jmp [BossGary_ActSubs + 4 * +2]` (6, BH's re-attributed table) |
| `0x437A90` | `BossGary_EndDispatch` | state 11: `jmp [BossGary_EndSteps + 4 * +2]` (3) |
| `0x437AB0` | `BossGary_EndStart` | `BattleEnemy_SetAnimation(4)`, `MoveCmd_TestFB(0x63, 0x14)`, `+2` up |
| `0x437AD0` | `BossGary_EndAwait` | when Mogu's count `0x904B7E` is 0: `+0x38` up by 0x8000, the ground word `+0x3E` from `AreaMap_Elevation`, bank 0x5F, `+0xFC` = `BossGary_AnimsEnd`, `+0x2A` = 0, animation 1, `0x904AE8 \|= 4`, `MoveCmd_TestFB(0x64, 0x14)`, `+2` up; every frame `Sprite_ScriptTick` |
| `0x437B60` | `BossGary_Hook` | `+0xF4`: `jmp [BossGary_Hooks + 4 * (word & 0xFF)]` (3, all `BareRet`) |
| `0x437B70` | `BossMogu_Dispatch` | as Gary's, `BossMogu_Steps` |
| `0x437B90` | `BossMogu_Enter` | as Gary's with `BossMogu_Anims`, `BossMogu_Hook`, `BossMogu_Cues` |
| `0x437BD0` | `BossMogu_ActDispatch` | `BossMogu_ActSubs` by `+2` |
| `0x437BF0` | `BossMogu_EndDispatch` | `BossMogu_EndSteps` by `+2` (3) |
| `0x437C10` | `BossMogu_EndStart` | `BattleEnemy_SetAnimation(4)`, the count `0x904B7E` = 0x3C, `+2` up |
| `0x437C30` | `BossMogu_EndCount` | the count down by one; at 0 Gary's step with `BossMogu_AnimsEnd` and animation 0 (no `0x904AE8`, no `MoveCmd_TestFB`), `+2` up; every frame `Sprite_ScriptTick` |
| `0x437CB0` | `BossMogu_Hook` | `BossMogu_Hooks` (3, all `BareRet`) |

State 11 is the generic table's `EnemyOp_HitPose`; both kinds replace it with
a scripted end in three steps, the third `BossOp_ScriptTick`. Mogu counts
0x3C frames in the battle word `0x904B7E`; Gary waits for that count to
reach 0; both then step `+0x38` by half a unit, take a new animation bank and
table and pose; Gary alone sets `0x904AE8` bit 2 - the bit fights 2 and 3's
end hooks test with bit 1 (`& 6`). What bit 2 means to the engine
(`0x904AE8` is the battle-end byte: bit 0 the loss, bit 1 the win) is not
read here; what enters state 11 is not read either.

### Fight 1's set-up and hooks

| Address | Name | What it does |
|---|---|---|
| `0x437CD0` | `Boss01_Setup` | `BattleHook_End` = `Boss01_End`, `BattleHook_Exit` = `Boss01_Exit`, `BattleHook_Event` = `Boss01_Event` |
| `0x437CF0` | `Boss01_Event` | by the phase code's low byte (a jump table of 7), `al` 0 always. 0: with `0x904AA8` bit 0x40 and target `0x904B44` 0, `0x904AAD \|= 1`. 1: with the actor `0x904B34` 0, the action kind `0x904B35` 4 and the action's id (`+2` of `[0x904B40]`) 0x78, enemies 0 and 1's `+0xA4` and `+0xB0` = 1 and the leader's `+0x134` bit 1 cleared. 5: with `0x904AAD` bit 0, the leader's action `+0x126` = 0x78, `+0x125` = 4, `0x904B35` = 4, its target `+0x124` = 0x40 and `0x904B44` = 0x40. 6: the leader's `+0x134` bit 1 set |
| `0x437DE0` | `Boss01_End` | with `0x904AE8` bit 0 (the loss) the chapter's run `0x8034E4` = 6 and step `0x8034E5` = 0x32, otherwise the step up by one; then `0x446E20` (the end phase's step 3) |
| `0x437E10` | `Boss01_Exit` | for the field actors tagged 6 and 7 and enemies 0 and 1: `BossActor_ClearBit40`, `BossActor_CopyFrom(tag, enemy, 1)` (the place), `Sprite_Current = BossActor_Find(tag)`, bank 0x5F, `+0x2A` = `+0x48` = 0, animation 1 (tag 6) or 0 (tag 7), the words `+0x58` / `+0x5A` the enemy's |

Read together (the code's facts, not play): phase 1 takes an action of kind
4 with id 0x78 by the first member and puts both enemies at 1 HP; phase 0
arms `0x904AAD` bit 0 when round flag 0x40 is up with no target; phase 5,
once armed, queues that same action (0x78, kind 4) for the leader against
target 0x40. Whether that is a scripted attack, and what 0x40 targets, is
the engine's to say (not read). The exit hook hands the two enemies' places
and poses to the field actors tagged 6 and 7.

### Kind 1 (Nue, area 23), kind 2 (Nue, area 22), kind 46 (Sample 1)

| Address | Name | What it does |
|---|---|---|
| `0x437EE0` | `BossNue_Dispatch` | `BossNue_Steps` by `+1` (12; `BossNue_EndDispatch` at 9, `EnemyOp_ActDispatch` at 6) |
| `0x437F00` | `BossNue_Enter` | `+0xFC` `BossNue_Anims`, `+0xF4` `BossNue_Hook`, `+0xF8` `BossNue_Cues`, `+1` = 2, `Sprite_ScriptTick` |
| `0x437F40` | `BossNue_EndDispatch` | state 9: `BossNue_EndSteps` by `+2` (2) |
| `0x437F60` | `BossNue_EndPose` | `Sprite_SetAnimation(6)`, `+2` up |
| `0x437F80` | `BossNue_EndMove` | the end walk: `Sprite_ScriptTickOnce`; `MoveCmd_OpE9` on the field actor tagged 0 (`0x7DEF00 + BossActor_Index(0) * 0xA4`: its object + 0x80) with `(0, -14, 0x40, 0xFC00, 0xFF, 0)`; while it answers `al` not 0, nothing more; then `Battle_ClearActorBit(+5)`, `0x904AE8 \|= 2` (the win) and `Effect_Release` |
| `0x437FF0` | `BossNue_Hook` | `BossNue_Hooks` by the word (3: `BossNue_HookPick`, `BossNue_HookHit`, `BareRet`) |
| `0x438000` | `BossNue_HookPick` | the hook's call at the action pick (word 0): with HP `0xFFFF`, the action kind `0x904B35` = 3, `+4` = 0, `Sound_PlayById(0x602)` |
| `0x438030` | `BossNue_HookHit` | the hook's call at the hit (word 1; also kind 39's): HP less the signed pending damage `+0x108` at or below a quarter of `+0xB0` - `0x904AAD \|= 1` and HP = `0xFFFF` |
| `0x438070` | `BossNue2_Dispatch` | `BossNue2_Steps` by `+1` (12; the generic table but for 0 and 6) |
| `0x438090` | `BossNue2_Enter` | kind 1's `BossNue_Anims` and `BossNue_Cues`, its own hook `BossNue2_Hook` |
| `0x4380D0` | `BossNue2_ActDispatch` | `BossNue2_ActSubs` by `+2` (6) |
| `0x4380F0` | `BossNue2_Hook` | `BossNue2_Hooks` (3, all `BareRet`) |
| `0x438100` | `BossSample1_Dispatch` | `BossSample1_Steps` by `+1` (12; the generic table but for 0) |
| `0x438120` | `BossSample1_Enter` | `BossNue_Anims`, `BossSample1_Hook`, `BossSample1_Cues`, and `0x939AD8`'s `+0x114 \|= 8` |
| `0x438180` | `BossSample1_Hook` | `BossSample1_Hooks` (3, all `BareRet`) |

So of the two Nue kinds, kind 1 alone scripts: at a quarter of its HP the
hit marks it (`0xFFFF`, and `0x904AAD` bit 0), the next action pick turns
its action to kind 3 with cue 0x602, and state 9 walks the field actor tagged
0 until `MoveCmd_OpE9` is done, then ends the battle as a win. Note that the
pick sets `+4` = 0 and `MoveCmd_OpE9` dispatches by `Sprite_Current +4`
(its `symbols.toml` evidence), so the walk runs in `MoveCmd_OpE9`'s mode 0 -
by the code's order, not measured.

### Fights 2, 3 and 39 (`BOSS002`)

| Address | Name | What it does |
|---|---|---|
| `0x438190` | `Boss02_Setup` | end `Boss02_End`, exit `BareRet`, event `Boss02_Event` |
| `0x4381B0` | `Boss02_Event` | at phase code 0 (low byte) with `0x904AE8` bit 1: the window records' pass `0x802D20` = 2 and `Msg_OpenScript(0x19)`; `al` 0 |
| `0x4381E0` | `Boss02_End` | without `0x904AE8` bits 1 and 2: `0x446E00` (step 2); with either: `BossActor_Clear(0)`, MoveScript counter `0x903848` = 0x24, `Music_Track` = 0x1B, `0x446DE0` (step 1) |
| `0x438210` | `Boss03_Setup` | end `Boss03_End`, exit `BossHook_ExitActor0Bit40`, event `BareRetZero` |
| `0x438230` | `Boss03_End` | as `Boss02_End` with `BossActor_CopyFrom(0, enemy 0, 0)` (the pose), 0x6D, track 0x18 |
| `0x438270` | `Boss39_Setup` | end `BossHook_EndPickWay`, exit `BareRet`, event `BareRetZero` (all BH's) |

### Kind 39 (Weretigr, area 35; fight 33)

| Address | Name | What it does |
|---|---|---|
| `0x43D3D0` | `BossWeretigr_Dispatch` | `BossWeretigr_Steps` by `+1` (12: `BossWeretigr_Enter` at 0, `BossWeretigr_State4Dispatch` at 4, `BareRet` at 5, `EnemyOp_ActDispatch` at 6) |
| `0x43D3F0` | `BossWeretigr_Enter` | `BossWeretigr_Anims`, `BossWeretigr_Hook`, `BossWeretigr_Cues`, `+1` = 2, `Sprite_ScriptTick` |
| `0x43D430` | `BossWeretigr_State4Dispatch` | state 4 (the generic table's turn start `0x4365D0`): `BossWeretigr_State4Steps` by `+2` (5) |
| `0x43D450` | `BossWeretigr_State4Fx` | slot n = `BattleTask_Create(3, 6)` (`BattleBossFx_Dispatch`'s slot 6, group BSG's effect task F6); into it the first 0x80 bytes of the acting actor's enemy object (`0x93B960 + (0x904B34 - 3) * 0x128`, `rep movsd`), kind 3, `+5` = 6, `+1..+4` = 0, `+0xB` = n, owner `+0x80` = `Sprite_Current`; then `+9` = n, `+0xA` = 0x10, `+0 \|= 0x40`, `+2` up |
| `0x43D500` | `BossWeretigr_State4Cue` | `+0xA` down; at 0 the cue at `[0x939AD8 +0xF8]` and that cue + 4 through `0x437450`, `+2` up |
| `0x43D560` | `BossWeretigr_State4End` | once `+0` has lost bit 0x40: `0x904AA8 \|= 4`, `0x4376F0`, `0x4376A0` (the turn closed: `+1` = 2, `+2` = 0) |
| `0x43D580` | `BossWeretigr_EndPose` | `Sprite_SetAnimation(0)`, `+2` up |
| `0x43D5A0` | `BossWeretigr_EndMove` | kind 1's end walk, then `0x904AE8 \|= 2` and the experience `0x904AEC +=` the enemy's `+0x96` (what `Battle_EnemyDefeated` adds), `Effect_Release` |
| `0x43D630` | `BossWeretigr_Hook` | `BossWeretigr_Hooks` (3: `BossWeretigr_HookPick`, `BossNue_HookHit`, `BareRet`) |
| `0x43D640` | `BossWeretigr_HookPick` | as `BossNue_HookPick` without the cue |

Weretigr's turn (state 4) is an effect task of its own - a copy of the
acting enemy's object handed to the kind-3 dispatcher's slot 6 - two cues
16 frames on, and the turn closed once the task clears the enemy's bit 0x40
(by the step's order; the task is BSG's). Steps 3 and 4 are its end,
reached when something sets `+2` to 3 (nothing in this group does - not
read further).

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original jumps through a table by an unchecked index (the
twelve state and sub-table dispatchers, the six hook dispatchers) ours aborts
with a `Fatal` naming the function, and `BossWeretigr_State4Fx` aborts where
the original would copy into a slot past the 48 (section 6); the owner's rule
for an unchecked index (round9 doc section 6). Nothing reaches either. The
dispatchers return the handler's `eax` (the original's `jmp` passes it
through); `EnemyRunAll` does not read it. `0x437450` gets the cue word with
its upper half 0 where the original pushes whatever was in `eax`'s upper half
(`Sprite_Current`'s, then `0x437450`'s own answer): `0x437450` compares `ax`
and `Sound_PlayEffect` reads 16 bits, so the two are the same.

## 3. The fuzz

`BOF3X_SHADOW=boss_sa`, `src/game/boss_sa_fuzz.cpp`: ten `Run`s
(`BOF3X_BSA_RUN=k6|k7|b1|k1|k2|k46|b2|b3|b39|k39` runs one), 6,000 rounds a
function, the harness unedited.

| Run | Fight, kind | Clones and shapes | Tables (`DataTable`) | Rounds | Result (this worktree) |
|---|---|---|---|--:|---|
| `k6` | 1, 6 | 3 dispatchers `kDispatch` (states 12 / 6 / 3), 3 states, the hook `kEnemyHook` | `BossGary_Hooks` (1 word), `_Steps`, `_ActSubs`, `_EndSteps` | 42,000 | 57,344 calls, 0 mismatches |
| `k7` | 1, 7 | the same shape | Mogu's four | 42,000 | 48,948 calls, 0 mismatches |
| `b1` | 1 | `kSetup`, `kEvent` (the jump table moved into the copy), `kEnd`, `kExit` | - | 24,000 | 66,000 calls, 0 mismatches |
| `k1` | 3, 1 | 2 dispatchers, 3 states, the hook and its two entries `kEnemyHook` | `BossNue_Hooks` (1 word), `_Steps`, `_EndSteps` | 48,000 | 53,446 calls, 0 mismatches |
| `k2` | 2, 2 | 2 dispatchers, state 0, the hook | kind 2's three | 24,000 | 24,000 calls, 0 mismatches |
| `k46` | 39, 46 | the dispatcher, state 0, the hook | kind 46's two | 18,000 | 18,000 calls, 0 mismatches |
| `b2` | 2 | `kSetup`, `kEvent`, `kEnd` | - | 18,000 | 10,916 calls, 0 mismatches |
| `b3` | 3 | `kSetup`, `kEnd` | - | 12,000 | 9,856 calls, 0 mismatches |
| `b39` | 39 | `kSetup` | - | 6,000 | no calls (the three stores, read back), 0 mismatches |
| `k39` | 33, 39 | 2 dispatchers, 6 states, the hook and its entry | `BossWeretigr_Steps`, `_State4Steps`, `_Hooks` (1 word) | 60,000 | 69,586 calls, 0 mismatches |

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the final
build): exit 0, first time (no silent death).

**First users of `kSetup`.** The shape needed nothing more: a set-up's three
stores land in the compared battle bytes and in the hooks the harness logs
after the call; `b39`'s only function makes no call and is compared on those
alone. (`kTask` is not used by this group: kind 39's effect task is BSG's.)

**What the group adds to the harness's use, in its own file** (nothing folded
back is needed, but the coordinator may want these as harness defaults):

- `Port_DroppedCall` listed again with no arguments: it sits in every kind's
  state table (entries 1 and 10), and reached through a dispatcher's `jmp` it
  "reads" the stack word of the dispatcher's caller, which differs between
  the copy (the harness's `a[0]`) and ours (its own frame). It is a bare
  `ret`; the standard listing's one byte argument would log that garbage.
- The order of a group's `DataTable`s matters where one address is in two
  tables with different `nargs` (`BareRet` in a hook table and in a state
  table): the first registration stands. Hook tables first for kinds 6, 7,
  1, 2, 46 (`BareRet` logs the hook's word); kind 39's state table first
  (`BareRet` is its state 5, reached by a `jmp` with no word of its own).
- `0x437450` (the cue player, one word, mask 0xFFFF), `0x4376A0`, `0x4376F0`
  (none) listed with their arities (the standard set leaves them to the
  group that calls them).

**Seeds.** Every dispatcher's other state bytes drawn inside its table
(`SeedStates`, every round), so that a dispatcher reading the wrong byte
lands on another entry. Kind 39's `disturb` rewrites the cue word at the
current enemy's `+0xF8` a third of the time (every enemy's `+0xF8` is the
same harness record, so moving `0x939AD8` alone cannot show a re-read). The hook word: 0..2 with garbage above the byte half the time (the
dispatchers index by the low byte, the entries get the whole word; the hook
tables' `BareRet` logs it). HP: `0xFFFF` / `0xFFFE` / 0 for the picks; for the
hit, the maximum small or any, the signed damage -100..499 or any, and HP
set so that HP - damage is the quarter, one above or one below. Mogu's count
`0x904B7E`: 1 (its end), 0 (the wrap), 2, 0x3C, 0x100, `0xFFFF`, any; Gary's
0 (its step) and the same. The battle-end byte: the bits each hook tests, one
at a time and together, and garbage. `Boss01_Event`: round flag 0x40, target
0, actor 0, kind 4 (and 3, 5, 0x84), `[0x904B40]` a record in the compared
state with `+2` = 0x78 (and 0x77, 0x79, 0x178, 0), `0x904AAD` bit 0; the
phase code 0..6, 7..255 an eighth of the time, garbage above the byte half
the time. `Boss02_Event`: phase code 0 a third of the time. Weretigr: the
acting actor 3..10 two times in three (the harness draws 0..2), `+0xA` at 1,
0, 2, 0x10, 0xFF, the experience at its ends.

## 4. Controls

`python controls.py` (the group's scratch script): each control one textual
change to ours, anchored on a string that occurs once in `boss_sa.cpp`, then
rebuild, run (`BOF3X_SHADOW=boss_sa`, `BOF3X_BSA_RUN` its run), restore,
rebuild. Run twice; the table is the second run, on the final seeds. **139
planted, 139 refused, every one by a count** (the planted function's
mismatched rounds; a plant in a shared helper - `EndStep`, `EndWalk`,
`PickSpent`, `HookDispatch`, `EndByBits`, `SetHooks` - is listed under the
function its run tests).

The first run refused 125 by a count, 13 by a `Fatal` and left one: the
dispatchers read by the wrong state byte (A1, A6, A7, B1, B3, B4, D1, D3, E1,
E3, F1, J1, J3) found it past their tables in the rounds where the seed left
it random, and the hook plants "the word + 1" (A20, E4, F4, J25) ran word 2
past a three-entry table. The seed now draws every other state byte inside
the dispatcher's table every round (the recorders log all four, and a byte
past a table is a `Fatal` on both sides), and the hook plants take the next
entry (A20, J25) or flip bit 8 of the word (E4, F4). J16 (the cue word read
once) went unrefused: every enemy's `+0xF8` is the same harness record, so
the standard disturbance moving `0x939AD8` never changed the word; the
group's `disturb` now rewrites the word at the current enemy's `+0xF8`. The
thinnest refusals are the re-read plants (A16 86, B6, A9, D5: 195..464
rounds) and `Boss01_Event`'s phase-1 plants (C4..C7, C14: 7..51 rounds - the
branch needs the actor 0, kind 4 and the id 0x78 at once).

A20 ("the next entry" of Gary's hook table) is equivalent in the game - all
three entries are `BareRet`, which reads nothing - and refused here only
because the hook table's recorder logs the word it is handed; its near
variants A21 (the entry handed the index) and B11 (bit 8 of the word
flipped) are refused on what the entries receive.

| # | Function | Plant | Refused (rounds of 6,000) |
|---|---|---|--:|
| A1 | `BossGary_Dispatch` | by +3 | 5424 |
| A2 | `BossGary_Enter` | the end animations | 6000 |
| A3 | `BossGary_Enter` | Mogu's hook | 6000 |
| A4 | `BossGary_Enter` | state 3 (every Enter) | 5965 |
| A5 | `BossGary_Enter` | the cues a word on (every Enter) | 6000 |
| A6 | `BossGary_ActDispatch` | by +3 | 4667 |
| A7 | `BossGary_EndDispatch` | by +1 | 4052 |
| A8 | `BossGary_EndStart` | x and z swapped | 6000 |
| A9 | `BossGary_EndStart` | Sprite_Current read before the calls | 412 |
| A10 | `BossGary_EndStart` | animation 2 | 6000 |
| A11 | `BossGary_EndAwait` | the count at 1 as 0 | 325 |
| A12 | `BossGary_EndAwait` | bit 1 for bit 2 | 1657 |
| A13 | `BossGary_EndAwait` | x 0x63 | 2293 |
| A14 | `BossGary_EndAwait` | a quarter step (EndStep) | 2293 |
| A15 | `BossGary_EndAwait` | x and z swapped (EndStep) | 2293 |
| A16 | `BossGary_EndAwait` | Sprite_Current read before the call (EndStep) | 85 |
| A17 | `BossGary_EndAwait` | +0x2B (EndStep) | 2293 |
| A18 | `BossGary_EndAwait` | bank 0x5E (EndStep) | 2293 |
| A19 | `BossGary_EndAwait` | no step | 2282 |
| A20 | `BossGary_Hook` | the next entry | 6000 |
| A21 | `BossGary_Hook` | the entry gets the index (HookDispatch) | 3017 |
| B1 | `BossMogu_Dispatch` | by +2 | 5397 |
| B2 | `BossMogu_Enter` | Gary's cues | 6000 |
| B3 | `BossMogu_ActDispatch` | by +1 | 4662 |
| B4 | `BossMogu_EndDispatch` | by +4 | 3959 |
| B5 | `BossMogu_EndStart` | count 0x3B | 6000 |
| B6 | `BossMogu_EndStart` | Sprite_Current read before the call | 214 |
| B7 | `BossMogu_EndCount` | at 1 as at 0 | 342 |
| B8 | `BossMogu_EndCount` | animation 1 | 2297 |
| B9 | `BossMogu_EndCount` | a byte count | 2662 |
| B10 | `BossMogu_EndCount` | the first animations | 2297 |
| B11 | `BossMogu_Hook` | the word bit 8 flipped | 6000 |
| C1 | `Boss01_Setup` | end and exit swapped | 6000 |
| C2 | `Boss01_Event` | phase 0: bit 1 | 166 |
| C3 | `Boss01_Event` | phase 0: or | 172 |
| C4 | `Boss01_Event` | phase 1: id 0x79 | 44 |
| C5 | `Boss01_Event` | phase 1: +0xB2 | 32 |
| C6 | `Boss01_Event` | phase 1: bit 0 cleared | 26 |
| C7 | `Boss01_Event` | phase 1: actor 1 too | 7 |
| C8 | `Boss01_Event` | phase 5: id 0x77 | 406 |
| C9 | `Boss01_Event` | phase 5: target 0x41 | 406 |
| C10 | `Boss01_Event` | phase 6: bit 2 | 567 |
| C11 | `Boss01_Event` | al 1 at phase 3 | 767 |
| C12 | `Boss01_Event` | phase 4 for 5 | 800 |
| C13 | `Boss01_Event` | the code by 16 bits | 449 |
| C14 | `Boss01_Event` | phase 1: kind 5 | 51 |
| C15 | `Boss01_End` | run 7 | 2954 |
| C16 | `Boss01_End` | bit 1 for bit 0 | 3630 |
| C17 | `Boss01_End` | step 2 | 6000 |
| C18 | `Boss01_End` | step 0x33 | 2965 |
| C19 | `Boss01_Exit` | animation 1 for tag 7 | 6000 |
| C20 | `Boss01_Exit` | the pose, not the place | 6000 |
| C21 | `Boss01_Exit` | +0x49 | 6000 |
| C22 | `Boss01_Exit` | +0x5A from +0x58 | 6000 |
| C23 | `Boss01_Exit` | bank 0x60 | 6000 |
| C24 | `Boss01_Exit` | tag 6 twice | 6000 |
| C25 | `Boss01_Exit` | the actor found again | 6000 |
| D1 | `BossNue_Dispatch` | by +3 | 5393 |
| D2 | `BossNue_Enter` | kind 2's hook | 6000 |
| D3 | `BossNue_EndDispatch` | by +1 | 2970 |
| D4 | `BossNue_EndPose` | animation 7 | 6000 |
| D5 | `BossNue_EndPose` | Sprite_Current read before the call | 215 |
| D6 | `BossNue_EndMove` | f 1 (EndWalk) | 6000 |
| D7 | `BossNue_EndMove` | al signed (EndWalk) | 1973 |
| D8 | `BossNue_EndMove` | tag 1 (EndWalk) | 6000 |
| D9 | `BossNue_EndMove` | stride 0xA0 (EndWalk) | 5809 |
| D10 | `BossNue_EndMove` | +4 (EndWalk) | 1965 |
| D11 | `BossNue_EndMove` | the loss bit | 1427 |
| D12 | `BossNue_EndMove` | Sprite_ScriptTick (EndWalk) | 6000 |
| D13 | `BossNue_EndMove` | b -13 (EndWalk) | 6000 |
| D14 | `BossNue_Hook` | the hit as the pick (HookDispatch) | 2013 |
| D15 | `BossNue_HookPick` | cue 0x601 | 1538 |
| D16 | `BossNue_HookPick` | kind 4 (PickSpent) | 1538 |
| D17 | `BossNue_HookPick` | +3 (PickSpent) | 1538 |
| D18 | `BossNue_HookPick` | 0xFFFE too (PickSpent) | 472 |
| D19 | `BossNue_HookHit` | the damage unsigned | 1567 |
| D20 | `BossNue_HookHit` | a half | 1227 |
| D21 | `BossNue_HookHit` | below, not at or below | 1003 |
| D22 | `BossNue_HookHit` | HP 0xFFFE | 2465 |
| D23 | `BossNue_HookHit` | bits 0 and 1 | 1209 |
| D24 | `BossNue_HookHit` | HP at +0xA6 | 2367 |
| D25 | `BossNue_Hook` | the word masked | 3004 |
| E1 | `BossNue2_Dispatch` | by +2 | 5418 |
| E2 | `BossNue2_Enter` | Sample 1's cues | 6000 |
| E3 | `BossNue2_ActDispatch` | by +1 | 4682 |
| E4 | `BossNue2_Hook` | the word bit 8 flipped | 6000 |
| E5 | `BossNue2_Enter` | the animations a byte on | 6000 |
| F1 | `BossSample1_Dispatch` | by +2 | 5415 |
| F2 | `BossSample1_Enter` | bit 4 | 4561 |
| F3 | `BossSample1_Enter` | kind 2's hook | 6000 |
| F4 | `BossSample1_Hook` | the word bit 8 flipped | 6000 |
| F5 | `BossSample1_Enter` | Sprite_Current's +0x114 | 1351 |
| G1 | `Boss02_Setup` | exit BareRetZero | 6000 |
| G2 | `Boss02_Event` | the code by 16 bits | 595 |
| G3 | `Boss02_Event` | pass 1 | 1171 |
| G4 | `Boss02_Event` | message 0x1A | 1171 |
| G5 | `Boss02_Event` | bits 1 or 2 | 418 |
| G6 | `Boss02_End` | bit 1 only (EndByBits) | 1111 |
| G7 | `Boss02_End` | track 0x1C | 3779 |
| G8 | `Boss02_End` | tag 1 | 3779 |
| G9 | `Boss02_End` | no clear (EndByBits) | 3779 |
| G10 | `Boss02_End` | step 3 (EndByBits) | 3779 |
| G11 | `Boss02_End` | counter 0x25 | 3779 |
| G12 | `Boss02_Event` | al 0xFF | 6000 |
| H1 | `Boss03_Setup` | event BareRet | 6000 |
| H2 | `Boss03_End` | the place | 3837 |
| H3 | `Boss03_End` | track 0x19 | 3837 |
| H4 | `Boss03_End` | enemy 1 | 3837 |
| H5 | `Boss03_End` | counter 0x6C | 3837 |
| I1 | `Boss39_Setup` | exit BareRetZero | 6000 |
| I2 | `Boss39_Setup` | the end hook not stored (SetHooks) | 6000 |
| I3 | `Boss39_Setup` | fight 1's end | 6000 |
| J1 | `BossWeretigr_Dispatch` | by +3 | 5426 |
| J2 | `BossWeretigr_Enter` | the cues a word on | 6000 |
| J3 | `BossWeretigr_State4Dispatch` | by +3 | 4797 |
| J4 | `BossWeretigr_State4Fx` | parameter 5 | 6000 |
| J5 | `BossWeretigr_State4Fx` | the actor less 2 | 6000 |
| J6 | `BossWeretigr_State4Fx` | a dword short | 6000 |
| J7 | `BossWeretigr_State4Fx` | slot 7 | 6000 |
| J8 | `BossWeretigr_State4Fx` | +0xC | 6000 |
| J9 | `BossWeretigr_State4Fx` | count 0x11 | 6000 |
| J10 | `BossWeretigr_State4Fx` | bit 0x20 | 4496 |
| J11 | `BossWeretigr_State4Fx` | the owner 0x939AD8 | 2032 |
| J12 | `BossWeretigr_State4Fx` | +2 = 1 | 6000 |
| J13 | `BossWeretigr_State4Fx` | the slot masked to 5 bits | 1982 |
| J14 | `BossWeretigr_State4Cue` | down by 2 | 6000 |
| J15 | `BossWeretigr_State4Cue` | the second + 2 | 1362 |
| J16 | `BossWeretigr_State4Cue` | the cue read once | 24 |
| J17 | `BossWeretigr_State4Cue` | the step before the cues | 1349 |
| J18 | `BossWeretigr_State4End` | bit 0x20 | 3056 |
| J19 | `BossWeretigr_State4End` | bit 3 | 3102 |
| J20 | `BossWeretigr_State4End` | the calls swapped | 4470 |
| J21 | `BossWeretigr_EndPose` | animation 1 | 6000 |
| J22 | `BossWeretigr_EndMove` | a byte of experience | 2021 |
| J23 | `BossWeretigr_EndMove` | +0x94 (the gold) | 2025 |
| J24 | `BossWeretigr_EndMove` | bits 0 and 1 | 960 |
| J25 | `BossWeretigr_Hook` | the next entry | 6000 |
| J26 | `BossWeretigr_HookPick` | Nue's cue too | 1475 |
| J27 | `BossWeretigr_EndMove` | a tick more | 2025 |

## 5. What nothing reached

Every clone reached every branch the controls planted in. What only a fight
would show: the scripted end of fight 1 (Gary and Mogu's state 11 and what
enters it), fight 1's scripted attack (phases 1 and 5), Nue's walk and
win, Weretigr's effect task and its end, and the chapter hand-back
`Boss01_End` writes on a loss (`0x8034E4` = 6, `0x8034E5` = 0x32). Also not
reached: a dispatcher past its table and `BossWeretigr_State4Fx` with no free
slot (the harness's `BattleTask_Create` answers 0..47) - the two `Fatal`s.

## 6. Latent defects (Capcom's, kept or aborted)

- **`BossWeretigr_State4Fx` does not test `BattleTask_Create`'s answer.**
  With all 48 slots taken it answers 0xFF, and the original writes 0x84
  bytes at `0x93A000 + 0xFF * 0x84` = `0x9423FC`, past the pool (which ends
  at `0x93B8C0`), and stores 0xFF in `+9`. Ours aborts with a `Fatal` there
  (an index past a table). Whether a fight can fill the pool is not
  measured.
- **It copies the acting actor's object, not `Sprite_Current`'s**: `(0x904B34
  - 3) * 0x128` from the enemies' base. For a member (0..2) that is 0x128..
  0x378 bytes before the enemies, inside the task pool. Kept (ours copies the
  same bytes); in the game the state runs on the enemy's own turn, so the two
  are presumably the same enemy (not measured).
- **The end walk trusts `BossActor_Index(0)`**: with no field actor tagged 0
  it answers 0xFF, and `MoveCmd_OpE9` is handed `0x7DEF00 + 0xFF * 0xA4` =
  `0x80925C`, far past the 30 field objects. Kept (the pointer is only passed
  on); the scene's data decides whether it can happen.
- **`Boss01_Event` reads `[0x904B40]` without a test** at phase 1 (once the
  actor, the kind and 0x904B35 match). The action phase sets the pointer
  before it calls the hook; kept.
- **The dispatchers index unchecked** (twelve by a state byte, six hooks by
  the word's low byte). Ours aborts.
- **Kinds 6, 7, 2 and 46's hook tables are three `BareRet`s**, and kind 1
  and 39's third entry is one: the hook's per-frame call (word 2, from
  `EnemyRunAll`) does nothing in all six kinds. Not a defect; noted because
  the hook exists only for kinds 1 and 39's first two entries.

## 7. Named data

Twenty-two tables, `[[data]]` in `symbols.toml` (addresses and sizes only;
their contents are the exe's):

| Address | Name | Count | What |
|---|---|--:|---|
| `0x64C770` | `BossGary_Anims` | 12 bytes | kind 6's `+0xFC` (the animation bytes `BattleEnemy_SetAnimation` reads) |
| `0x64C77C` | `BossGary_AnimsEnd` | 12 bytes | kind 6's `+0xFC` after its end step |
| `0x64C788` | `BossMogu_Anims` | 12 bytes | kind 7's |
| `0x64C794` | `BossMogu_AnimsEnd` | 12 bytes | kind 7's after its end step |
| `0x64C7A0` | `BossGary_Cues` | 4 words | kind 6's `+0xF8` (cue words) |
| `0x64C7A8` | `BossMogu_Cues` | 4 words | kind 7's |
| `0x64C7F8` | `BossGary_EndSteps` | 3 | `BossGary_EndStart`, `_EndAwait`, `BossOp_ScriptTick` |
| `0x64C804` | `BossGary_Hooks` | 3 | `BareRet` x3 |
| `0x64C858` | `BossMogu_EndSteps` | 3 | `BossMogu_EndStart`, `_EndCount`, `BossOp_ScriptTick` |
| `0x64C864` | `BossMogu_Hooks` | 3 | `BareRet` x3 |
| `0x64C870` | `BossNue_Anims` | 16 bytes | kinds 1, 2 and 46's `+0xFC` |
| `0x64C880` | `BossNue_Cues` | 4 words | kinds 1 and 2's `+0xF8` |
| `0x64C888` | `BossSample1_Cues` | 4 words | kind 46's `+0xF8` |
| `0x64C8C0` | `BossNue_EndSteps` | 2 | `BossNue_EndPose`, `_EndMove` |
| `0x64C8C8` | `BossNue_Hooks` | 3 | `BossNue_HookPick`, `_HookHit`, `BareRet` |
| `0x64C91C` | `BossNue2_Hooks` | 3 | `BareRet` x3 |
| `0x64C958` | `BossSample1_Hooks` | 3 | `BareRet` x3 |
| `0x64D638` | `BossWeretigr_Anims` | 12 bytes | kind 39's `+0xFC` |
| `0x64D644` | `BossWeretigr_Cues` | 6 words | kind 39's `+0xF8` |
| `0x64D650` | `BossWeretigr_Steps` | 12 | kind 39's state table |
| `0x64D680` | `BossWeretigr_State4Steps` | 5 | its state 4's steps |
| `0x64D694` | `BossWeretigr_Hooks` | 3 | `BossWeretigr_HookPick`, `BossNue_HookHit`, `BareRet` |

BH named the kinds' `+1` and action tables (`BossGary_Steps` ...
`BossSample1_Steps`, [`boss_h.md`](boss_h.md) section 7); their counts (12, 6)
are right by the code. The plan's "kind 1: `0x64C890`, `0x64C8C0`,
`0x64C8C8`" are `BossNue_Steps`, `BossNue_EndSteps`, `BossNue_Hooks`. The
byte and word tables' sizes are the gap to the next table the code names;
how much of each the engine reads was not measured.

## 8. Calls across groups

**Out of the group, to code nobody owns** (raw, `boss_sa_callees.h`):
`0x437450` (the cue player), `0x4376A0` (the turn closed), `0x4376F0` (a
chance of `0x904AA8` bit 7 and a task), and the end phase's steps `0x446DE0`,
`0x446E00`, `0x446E20` - all unnamed engine code, no group's this round.
Everything else is ours and called by name: BH's spawn helpers and hooks
(stored as literals: `BareRet`, `BareRetZero`, `BossHook_EndPickWay`,
`BossHook_ExitActor0Bit40`), round seven's `EnemyOp_*` (table entries),
`Sprite_*`, `Battle_*`, `Msg_OpenScript`, `Sound_PlayById`, `Effect_Release`,
`AreaMap_Elevation`, `BattleTask_Create`; Capcom's `MoveCmd_OpE9` and
`MoveCmd_TestFB` by their macros. No raw call into another stage-B group:
the effect task `BossWeretigr_State4Fx` creates (the kind-3 dispatcher's slot
6) is BSG's, reached through `BattleTask_RunAll`, not called.

**Into the group from outside** (for the rebinding pass): none from our code
(`grep` of `src/` for the 49 addresses: only `battle_flow_callees.h`'s
comment naming `0x437CF0`, and `boss_spawn.cpp`'s naming `0x437E10`). Capcom's
references are the root tables and the kinds' own: `Boss_SetupTable` 1, 2,
3, 39; `BossKind_Table` 1, 2, 6, 7, 39, 46; the state, sub- and hook tables
above; the stores of the hooks and `+0xF4` in the set-ups and state 0s. All
reach ours through the entries' `jmp`s. (`0x00438000` also occurs as a dword
in unrelated code and data - `.text 0x54EDA6`, `0x5577FA`, `0x5669AE`,
`.rdata 0x5C4995`, `.data 0x5F5F7F`, `0x5F7EBB`, `0x600853`, unaligned or
constants; none is a reference.)

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 49 extents above, one
line each, none there before. The smaller extents fix two hosts that ran
over the band: `004379D0 2F0` (over kinds 6 and 7) and `0043C9F0 189F` (over
kind 39).
