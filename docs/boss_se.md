# Group BSE: fights 22..26, 30 and 48, kinds 28..32 and 55

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave one, stage B. 52 functions of `0x43B5B0..0x43E7A0` ours
(`src/game/boss_se.cpp`, shadow `boss_se`), each read to its last instruction
with capstone and fuzzed through the boss harness ([`boss_harness.md`](boss_harness.md)),
thirteen `Run`s, 0 mismatches; 145 controls planted, 141 refused by a count, 4 equivalent with their near variants refused (section 4). Fuzz only: no recorded
route reaches a boss fight.

Enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's area
records), not memory of the game. Which fight a set-up is comes from the
tool's rows and the code's `0x904AAA` compares; nothing here says what
happens in a fight.

## 1. The units and their functions

`tools/boss_rows.py --unit <U> --clones` for the thirteen units
(2026-09-28), 52 functions, every one in `analysis/boss_funcs.tsv`'s group
column as BSE. None found inside another's extent, none missing; every
extent the tool gave was the code's (`0x43C230`'s 0x13C includes its own
four-entry jump table at `+0x12C`). Two functions are shared:
`0x43B750` (B23 and B30's exit hook, both ours) and `0x43E790` (B26's and
B35's - BSH's set-up 35 stores it, so it is the earlier group's: ours).
`0x43B730`, B23's end hook, is B21's too and group BSD's: B23's set-up stores
it as a literal and nothing of ours calls it.

The sibling's per-image counts (`analysis/overlay_captures_all.json`,
`static_discovery_entry_pcs`) as the check on size: BOSS023 6 (B23 + K28 =
6 here), BOSS024 12 (B24, B48, K29, K55 = 10), BOSS025 36 (B25, B26, K30,
K31, K32 = 32), BOSS022 10 and BOSS030 11 (their kinds are BSD's and BSG's).

### Set-ups (`Boss_SetupTable[id]`, `kSetup`) and their hooks

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43B5B0` | `Boss22_Setup` | 0x1F | id 22 (BOSS022, area 80 row 7; kind 27 Garr is BSD's): end `Boss22_End`, exit `Boss22_Exit`, event `BareRetZero` |
| `0x43B5D0` | `Boss22_End` | 0x35 | movement-script variable 3 `0x903848` = 0xC, `0x904AE8` \|= 8, `0x904AE5` &= 0xBF, `Music_Track` = 0x6A, `0x92BF18` = 3, then `0x446E20` (step 3) whatever the end |
| `0x43B610` | `Boss22_Exit` | 0x3B | `BossActor_ClearBit40(0)`, `Sprite_Current` = `BossActor_Find(0)`, bank 0xAA, `+0x2A` = `+0x48` = 0 (on the `Sprite_Current` of after the call), `Sprite_SetAnimation(6)` |
| `0x43B710` | `Boss23_Setup` | 0x1F | id 23 (BOSS023, area 67 row 7, kind 28): end `0x43B730` (BSD's), exit `BossHook_ExitClearActors012`, event `BareRetZero` |
| `0x43B750` | `BossHook_ExitClearActors012` | 0x19 | exit hook of 23 and 30: `BossActor_Clear` 0, 1, 2 |
| `0x43B870` | `Boss24_Setup` | 0x1F | id 24 (BOSS024, area 67 row 6): end `Boss24_End`, exit `Boss24_Exit`, event `BareRetZero` |
| `0x43B890` | `Boss24_End` | 0x28 | won (`0x904AE8` bit 1): variable 3 = 5, `0x904AE8` \|= 8, `Music_Track` = 0x44, step 1 (`0x446DE0`); else step 2 (`0x446E00`) |
| `0x43B8C0` | `Boss24_Exit` | 9 | `BossActor_Clear(3)` |
| `0x43B8D0` | `Boss48_Setup` | 0x1F | id 48 (BOSS024, area 166 row 7 - chapter 15's register-passed ids): end `BossHook_EndPickWay`, exit `BareRet`, event `BareRetZero` |
| `0x43BEA0` | `Boss25_Setup` | 0x56 | id 25 (BOSS025, area 92 row 7): party member 0's HP `+0x98` and AP `+0x9A` saved - **their low bytes** - in `0x675F05` / `0x675F04`, the two words set from `+0xA0` / `+0xA2`; end `Boss25_End`, exit `Boss25_Exit`, event `Boss25_Event`; `0x904AE4` = 3 |
| `0x43BF00` | `Boss25_Event` | 0x27D | the event hook (below) |
| `0x43C180` | `Boss25_End` | 0x29 | member 0's HP and AP words = the saved bytes, zero-extended; `0x8034E5` = 4; step 3 |
| `0x43C1B0` | `Boss25_Exit` | 0x4A | round-flag bit 1, `Transition_Start(4)`, then `Field_MemberCount` = 3 and the byte `0x939A02` bytes of `0x939A10` copied to the party list `0x904065` |
| `0x43C200` | `Boss26_Setup` | 0x30 | id 26 (BOSS025, area 92 row 7): end `Boss26_End`, exit `BossHook_ExitTransition4`, event `Boss26_Event`; the sums `0x939A08`, `0x939A04` and the count `0x939A0C` zeroed |
| `0x43C230` | `Boss26_Event` | 0x13C | the event hook (below) |
| `0x43C370` | `Boss26_End` | 0x105 | each party member with `+0` bit 0: `Battle_RemoveFromTurnOrder(+5)`, `Sprite_Current` = it, `Sprite_PoseFromSet(+8 + 0x1C` with `+0x90` bit 14, else `+ 4` - a byte`, 0x8C5D80, 0x1800)`; variable 3 = 4; step 3 |
| `0x43E790` | `BossHook_ExitTransition4` | 0x10 | exit hook of 26 and 35: round-flag bit 1, `Transition_Start(4)` |
| `0x43CF80` | `Boss30_Setup` | 0x1F | id 30 (BOSS030, area 103 row 7; kinds 35 and 36 are BSG's): end `Boss30_End`, exit `BossHook_ExitClearActors012`, event `BareRetZero` |
| `0x43CFA0` | `Boss30_End` | 0x2E | won: `0x8034E5` = 0xC, variable 3 up by one, step 1, **then** `Music_Track` = 0x5A; else step 2 |

**`Boss25_Event`**, by the code's low byte, `al` 0 on every path:
code 0 (an action phase's): the turn counter `0x904B90` = (`0x904AE2` >> 1)
+ 1, and by `0x904AE2`: 1 message 0x1B; 2 member 0's action `+0x125` = 1
and message 0x1C; 3 message 0x1D; 4 (unless round-flag bit 6) action 4 and
skill `+0x126` = 0x46; 5 message 0x1E; 6 (unless bit 6) message 0x1F, then
action 1, `+0xBA` = 0x64, enemy 0's HP = 1. Code 3 (`Battle_PhaseDispatch`'s)
walks the script bits `0x904AAD`: bit 5 clear in phase 1 - `BattleTask_Create(0, 8)`,
bit set (`0x904AAD` read again); bit 4 clear in phase 3 step 0 - enemy 0's
`+0x110` \|= 2, `+0x104` = 0, `+0x105` = 2, member 0's target `+0x124` = 3,
action 1, `+0xBC` = 0x64, `+0xBA` = 0, the turn order `0x904ACC` = 0, 3, 0,
3, 0, 3, 0 with `0x904AE3` = 7, bit set (on the value held); then the first
clear of bits 0, 1, 2 - message 0x19 / enemy 0 made `Sprite_Current` with
`BattleEnemy_SetAnimation(8)` / message 0x1A - and that bit set (read again
after the call); all set: at `0x904AE2` 4 with member 0's `+1` 6 and `+2` 1,
its words `+0x128` / `+0x12A` = 0 and `0x904B35` = 0xFF; at 4 or 6 with
`+1` 2 and `+2` 0, round-flag bit 6. Every message sets `0x802D20` = 2
first. Other codes: nothing.

**`Boss26_Event`**, by the code's low byte through a four-entry jump table,
`al` 0 always: 0 - enemy 0 acting (`0x904B34` 3) on a member (target 0..2)
whose s16 `+0x128` is above 0: added to `0x939A08`; otherwise, on target 3 or
a side (bits 6, 7): with enemy 0's `+0x110` bit 1 the count `0x939A0C` up
one, else enemy 0's s16 `+0x108`, if above 0, added to `0x939A04`. 1 - at
`0x904AE2` 1 with enemy 0's `+0x8E` 1: enemy 0's `+0x110` \|= 2, `+1` = 2,
`+2` = 0, `+0x105` = 2. 2 - at turn `0x904B90` = 0x15 (the dword),
`0x904AE8` \|= 4. 3 - as `Boss25_Event`'s task step. What reads the two
sums and the count was not traced.

### Kinds (`BossKind_Table[kind]`) and their tables

Each kind's dispatcher is `jmp [States + 4 * Sprite_Current +1]` (12 entries:
the kind's entry at 0, then the generic enemy states, `Port_DroppedCall` at
1 and 10); its entry stores `0x939AD8`'s `+0xFC` (animation bytes, read by
`BattleEnemy_SetAnimation`), `+0xF4` (the hook) and `+0xF8` (sound words)
and sets `+1` = 2; its hook is `jmp [Hooks + 4 * (word & 0xFF)]` (3 entries).

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43B650` | `BossBully_Dispatch` | 0x12 | kind 28 (Bully 1..3, area 67): `BossBully_States` (12) |
| `0x43B670` | `BossBully_Enter` | 0x8F | `+0xFC` `BossBully_AnimsB` at `+5` 3 else `_AnimsA`; `+0xF8` by `+5` (read again) 3 / 4 / other: `_SoundsA` / `B` / `C`; `+0xF4` `BossBully_Hook`; `+1` = 2; tail `Sprite_ScriptTick` (`al`) |
| `0x43B700` | `BossBully_Hook` | 0x10 | `BossBully_Hooks` (3, `BareRet` each) |
| `0x43B770` | `BossStallion_Dispatch` | 0x12 | kind 29 (Stallion, area 67) |
| `0x43B790` | `BossStallion_Enter` | 0x3D | `BossStallion_Anims`, `_Hook`, `_Sounds`; tail tick |
| `0x43B7D0` | `BossStallion_Hook` | 0x10 | `BareRet` x3 |
| `0x43B7E0` | `BossSample10_Dispatch` | 0x12 | kind 55 (Sample10..12, area 166) |
| `0x43B800` | `BossSample10_Enter` | 0x51 | Stallion's two byte tables, its own hook, `0x939AD8 +0x114` \|= 8; tail tick |
| `0x43B860` | `BossSample10_Hook` | 0x10 | `BareRet` x3 |
| `0x43B8F0` | `BossBeyd_Dispatch` | 0x12 | kind 30 (Beyd, area 92) |
| `0x43B910` | `BossBeyd_Enter` | 0xFE | its tables and hook, `+1` = 2, `Sprite_ScriptTick` **called** (answer dropped); in fight 0x1A only: `Battle_CopyEnemyData(0, EnemyData_FindByTag(0x59))`, `+0x8F` = 1, the kind `+0x100` = 0x1E, and HP `+0xA4` / `+0xD0` / `+0xB0` from the word `0x903F0C`, `+0xD4` / `+0xB4` from `0x903F10`, `+0xD6` / `+0xB6` from `0x903F12` (writer of those three not read) |
| `0x43BA10` | `BossBeyd_ActDispatch` | 0x12 | state 6: `BossBeyd_ActSubs` by `+2` (6: `EnemyOp_ActSubs` with the death at 4) |
| `0x43BA30` | `BossBeyd_Death` | 0x5F | `Sprite_EnsureAnimation(0xF)`, `+0x2A` = 0, `Sprite_ScriptTickOnce`, `Battle_EnemyDefeated`, `+0` &= 0xBF, states 3, 0, 0 (all on the `Sprite_Current` of after the calls); in fight 0x19 message 0x20 |
| `0x43BA90` | `BossBeyd_Hook` | 0x10 | `BossBeyd_HookPick`, `BareRet`, `BossBeyd_HookTick` |
| `0x43BAA0` | `BossBeyd_HookPick` | 0x27 | word 0: in fight 0x19, `0x904AE2` >= 1 clears `0x939AD8 +0x110` bit 1, and `0x904B35` = 1 |
| `0x43BAD0` | `BossBeyd_HookTick` | 0x2E | word 2: round-flag bit 1 and `MoveScript_WaitWordDA` 0 - `BossActor_ClearBit40(0)`, then the flags word &= 0xFFFD and `Draw_PassFlags` = 0 |
| `0x43BB00` | `BossBeyd2_Dispatch` | 0x12 | kind 31 (the second Beyd kind) |
| `0x43BB20` | `BossBeyd2_Enter` | 0xDC | kind 30's byte tables, its own hook, tick called, `+8` = 1, `+0x8F` = 1, the seven words - in every fight |
| `0x43BC00` | `BossBeyd2_ActDispatch` | 0x12 | `BossBeyd2_ActSubs` (6) |
| `0x43BC20` | `BossBeyd2_Death` | 0x47 | the death, animation 0xF, no message |
| `0x43BC70` | `BossBeyd2_Hook` | 0x10 | `BossBeyd2_HookTarget4`, `BareRet`, `BareRet` |
| `0x43BC80` | `BossBeyd2_HookTarget4` | 8 | word 0: the target `0x904B44` = 4 |
| `0x43BC90` | `BossZig_Dispatch` | 0x12 | kind 32 (Zig, area 92) |
| `0x43BCB0` | `BossZig_Enter` | 0x47 | its tables and hook, `+8` = 3, `+1` = 2; tail tick |
| `0x43BD00` | `BossZig_Idle` | 0x40 | state 2: `0x904AAD` bit 3 without bit 5 - `Sprite_EnsureAnimation(0xC)`; else `BattleEnemy_SetAnimation(8` with `+0x110` bit 1 `, else 0)`; `BattleEnemy_ScriptTick`; `+1` up one |
| `0x43BD40` | `BossZig_Step5Dispatch` | 0x12 | state 5: `BossZig_Step5Subs` by `+2` (2) |
| `0x43BD60` | `BossZig_Step5Count` | 0x3F | tick; `+9` down one; at 0 the sound `0x437450`(the first word of `+0xF8`) and `+2` up one |
| `0x43BDA0` | `BossZig_Step5Fire` | 0x4C | when the tick answers `al` not 0: `Battle_SetTargetFlag40(target)`; `Rand & 3` zero - round-flag bit 7, `0x904AAD` \|= 0x10, `BattleTask_Create(0, 2)`; round-flag bit 2; tail `0x4376A0` (the action's end: `+1` = 2, `+2` = 0) |
| `0x43BDF0` | `BossZig_ActDispatch` | 0x12 | `BossZig_ActSubs` (6) |
| `0x43BE10` | `BossZig_Death` | 0x47 | the death, animation 0xD |
| `0x43BE60` | `BossZig_Hook` | 0x10 | `BossZig_HookPick`, `BossZig_HookHit`, `BareRet` |
| `0x43BE70` | `BossZig_HookPick` | 0x19 | word 0: without `0x904AAD` bit 3 target = 3, with it `0x904B35` = 0 |
| `0x43BE90` | `BossZig_HookHit` | 8 | word 1: `0x904AAD` \|= 0x20 |

`BossBeyd_Enter` and `BossBeyd2_Enter` call `Sprite_ScriptTick` and do not
return its `al` (the other entries tail-jump to it): `BattleEnemy_RunAll`
reads no answer from a dispatcher, so ours returns nothing there
(`ret_mask` 0), and `al` for the four tail-jumping entries (`ret_mask 0xFF`,
the harness's rule).

### Named data (`symbols.toml`, 27 `[[data]]`)

Per kind: `_States` (12), `_Hooks` (3), `_ActSubs` (6) for 30..32,
`BossZig_Step5Subs` (2); the byte tables the entries store - `_Anims` (12
bytes, `+0xFC`) and `_Sounds` (4 words, `+0xF8`) for Bully (A, B / A, B, C),
Stallion (shared with kind 55), Beyd (shared by 30 and 31) and Zig. Counts
are the code's (the state values each dispatcher can reach, the next table),
not the tool's extents ("67 code entries" for kind 30's `+1` table is 12,
its `+2` table and hook table after).

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed (grepped DIVERGENCE.md and `cheats.cpp` for the 52 and their tables:
nothing). The dispatchers and hook tables abort with a `Fatal` past their
tables, where the original jumps through what follows (round9 doc section
6). Ours of a hook table passes the caller's word whole to its entry, as the
`jmp` does.

## 3. The fuzz

`BOF3X_SHADOW=boss_se`, `src/game/boss_se_fuzz.cpp`, one `Run` per fight and
kind (`BOF3X_BSE_RUN=<unit>` runs one); every shape by its root: set-ups
`kSetup` (**the first user of the shape**), end / exit / event hooks, the
dispatchers `kDispatch` with their state byte drawn below the table, entries
`kState`, the hooks and their entries `kEnemyHook`. Every kind's `+1`,
`+2`, step-5 and hook tables are `DataTable`s (hook tables with one word).

| Run | Fight, kind | Clones | Rounds | Calls | Result (this worktree) |
|---|---|---|--:|--:|---|
| `b22` | 22 | set-up, end, exit | 6,000 each | 30,000 | 0 mismatches |
| `k28` | 23, 28 | dispatcher, entry, hook | 6,000 | 18,000 | 0 |
| `b23` | 23 | set-up, exit | 4,000 | 12,000 | 0 |
| `b30` | 30 | set-up, end | 6,000 | 6,000 | 0 |
| `k29` | 24, 29 | dispatcher, entry, hook | 6,000 | 18,000 | 0 |
| `k55` | 48, 55 | dispatcher, entry, hook | 6,000 | 18,000 | 0 |
| `b24` | 24 | set-up, end, exit | 6,000 | 12,000 | 0 |
| `b48` | 48 | set-up | 4,000 | 0 (it calls nothing) | 0 |
| `k30` | 25, 30 | seven | 6,000 | 49,867 | 0 |
| `k31` | 25, 31 | six | 6,000 | 42,000 | 0 |
| `k32` | 25, 32 | eleven | 6,000 | 86,216 | 0 |
| `b25` | 25 | set-up, event, end, exit | 8,000 | 20,210 | 0 |
| `b26` | 26 | set-up, event, end, exit | 8,000 | 47,434 | 0 |

370,000 rounds in all. Coverage (the originals' calls, this worktree, the
first build): every entry of every `DataTable` (`phase 0x4363B0` ..
`phase 0x437420` about 500 each, the kinds' own entries likewise); `k30`:
`Battle_CopyEnemyData` / `EnemyData_FindByTag` 2,153, `Msg_OpenScript`
2,390, `BossActor_ClearBit40` 1,171; `k32`: `0x437450` 1,080, `0x4376A0` /
`Battle_SetTargetFlag40` / `Rand` 3,952, `BattleTask_Create` 1,280;
`b25`: `Msg_OpenScript` about 3,300, `BattleEnemy_SetAnimation` about
1,200, `BattleTask_Create` about 380; `b26`: `Sprite_PoseFromSet` /
`Battle_RemoveFromTurnOrder` about 15,500, `BattleTask_Create` about 400.
Counts move with the build directory; judge by 0 mismatches.

**`kSetup` works as documented**: the seven set-ups' stores land in the
compared hook cells and the logged read-back, and a set-up storing the
wrong hook was refused in every round it ran (C1, C18, C21, C35, C41, C123).
Nothing in the shape was missing. `kTask` is not used by this group.

**Callees added to the standard set** (group listings, registered first):
`Port_DroppedCall` with no argument (it sits in every `+1` table; Capcom's
dispatcher jumps to it on its caller's stack, ours calls it with none - a bare
`ret` whose "argument" is never read); `Battle_CopyEnemyData` and
`Sprite_PoseFromSet` with the id and the animation masked to a byte (the
callees read only that; the originals push a register whose upper bytes are a
callee's leftovers); `Battle_RemoveFromTurnOrder` louder than the real one
(its effect flips a party member's `+0x90` bit 14, `+8` or `+0` bit 0, which
`Boss26_End` reads after it); and the two engine functions nobody owns,
`0x437450` (one word) and `0x4376A0` (none).

**Seeds.** Set-up 25's code 3 by the step a round aims at (each script bit
before it set, it clear, the phase and step it tests) and code 0 by
`0x904AE2` 0..7, 4 and 6 twice as often; member 0's states at 2 / 6 and 0 /
1; the copy count at 0, 1..8, 0x10, 0xFF. Set-up 26: the actor 3, the target
0..3, 0x40, 0x80, 0xC0, the words at 0, 1, 0xFFFF, 0x7FFF, 0x8000; the turn
at 0x15, 0x14, 0x16, 0x115, 0x80000015; the members' `+0` bit 0 and `+8`
where the byte add wraps. The kinds: Bully's `+5` 3, 4 and others; the fight
byte at 0x19 / 0x1A and the neighbours for Beyd's three compares; the wait
word at 0, 1, 0x100, 0x8000; Zig's script bits (bits 3, 4, 5), `+0x110`
bit 1, `+9` at 1, 0, 2, 0x80, the `Rand` hint; for a `+2` dispatcher the
other state bytes inside the table so a wrong-byte plant lands on an entry.
The event and hook words carry garbage above their low byte half the time.
**`Disturb`** moves `0x904AAD`, the copy count, a member's `+0x91`, `0x904AE2`
and variable 3.

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the final build): exit 0, BSE's thirteen `Run`s with the figures above and every other group's 0 mismatches; it passed first time both times it was run (no silent death).

## 4. Controls

`python controls.py` (the group's scratch script): each control one textual
change to ours anchored on a string that occurs once, rebuild, the one `Run`
that holds the function, restore, rebuild at the end. A dispatcher's plant is
made in the shared `Dispatch` helper, keyed on the function's name (the next
entry of its table). Run twice; the table is the second run, on the final seeds. **145 controls planted, 141 refused by a count, 4 equivalent with their near variants refused (section 4).** The thinnest refusals: C90b 23, C126b 24, C125 43, C105 44, C113 58, C111 60, C61 66, C116 69 - the re-read plants (a cell read before a call, refused only when the disturbance moves it in that call) and set-up 25's last step.

| # | Function | Plant | Refused |
|---|---|---|---|
| C1 | `Boss22_Setup` | the exit hook the end hook | 6000 rounds |
| C2 | `Boss22_End` | variable 3 = 0xD | 5947 rounds |
| C3 | `Boss22_End` | 0x904AE5 bit 7 cleared too | 2972 rounds |
| C4 | `Boss22_End` | track 0x6B | 6000 rounds |
| C5 | `Boss22_End` | 0x92BF18 = 2 | 6000 rounds |
| C6 | `Boss22_End` | 0x904AE8 bit 2 for bit 3 | 4754 rounds |
| C7 | `Boss22_Exit` | bank 0xAB | 6000 rounds |
| C8 | `Boss22_Exit` | +0x2A on the Sprite_Current of before the call | 215 rounds |
| C9 | `Boss22_Exit` | bit 0x40 of actor 1 | 6000 rounds |
| C10 | `Boss22_Exit` | animation 7 | 6000 rounds |
| C11 | `BossBully_Dispatch` | the next entry | 6000 rounds |
| C12 | `BossBully_Enter` | +5 at 4 for the B animations | 1984 rounds |
| C13 | `BossBully_Enter` | +5 at 5 for sounds B | 1476 rounds |
| C14 | `BossBully_Enter` | Stallion's hook installed | 6000 rounds |
| C15 | `BossBully_Enter` | al | 1 | 3996 rounds |
| C16 | `BossBully_Enter` | state 3 | 5965 rounds |
| C17 | `BossBully_Hook` | the entry given the masked word | 2965 rounds |
| C18 | `Boss23_Setup` | BossHook_EndPickWay for 0x43B730 | 4000 rounds |
| C19 | `BossHook_ExitClearActors012` | actor 3 for 2 | 4000 rounds |
| C20 | `BossHook_ExitClearActors012` | 0 and 1 swapped | 4000 rounds |
| C21 | `Boss30_Setup` | event BareRet | 6000 rounds |
| C22 | `Boss30_End` | variable 3 up by two | 3013 rounds |
| C23 | `Boss30_End` | step 0xB | 2979 rounds |
| C24 | `Boss30_End` | track 0x5B | 3040 rounds |
| C25 | `Boss30_End` | the win by bit 0 | 3476 rounds |
| C26 | `BossStallion_Dispatch` | the next entry | 6000 rounds |
| C27 | `BossStallion_Enter` | +0xFC and +0xF8 swapped | 6000 rounds |
| C28 | `BossStallion_Enter` | +2 for +1 | 6000 rounds |
| C29 | `BossStallion_Hook` | the word's second byte flipped | 6000 rounds |
| C30 | `BossSample10_Dispatch` | the next entry | 6000 rounds |
| C31 | `BossSample10_Enter` | +0x114 bit 4 | 4606 rounds |
| C32 | `BossSample10_Enter` | Stallion's hook | 6000 rounds |
| C33 | `BossSample10_Enter` | Sprite_ScriptTickOnce | 6000 rounds |
| C34 | `BossSample10_Hook` | the word's second byte flipped | 6000 rounds |
| C35 | `Boss24_Setup` | set-up 22's exit hook | 6000 rounds |
| C36 | `Boss24_End` | variable 3 = 6 | 2976 rounds |
| C37 | `Boss24_End` | 0x904AE8 | 0xA | NOT REFUSED (equivalent: on the win path 0x904AE8 already has bit 1, so `| 0xA` is `| 8` (no input tells them apart); C37b (`| 0xC`) refused) |
| C37b | `Boss24_End` | 0x904AE8 | 0xC | 2378 rounds |
| C38 | `Boss24_End` | track 0x45 | 3002 rounds |
| C39 | `Boss24_End` | the win by bit 0 or 1 | 1433 rounds |
| C40 | `Boss24_Exit` | actor 4 | 6000 rounds |
| C41 | `Boss48_Setup` | exit BareRetZero | 4000 rounds |
| C42 | `BossBeyd_Dispatch` | the next entry | 6000 rounds |
| C43 | `BossBeyd_Enter` | fight 0x19 for 0x1A | 2674 rounds |
| C44 | `BossBeyd_Enter` | tag 0x5A | 2253 rounds |
| C45 | `BossBeyd_Enter` | kind 31 | 2253 rounds |
| C46 | `BossBeyd_Enter` | 0x939AD8 read before the copy | 104 rounds |
| C47 | `BossBeyd_Enter` | +0xB4 from 0x903F12 (CarryStats) | 2253 rounds |
| C48 | `BossBeyd_Enter` | slot 1 | 2253 rounds |
| C49 | `BossBeyd_ActDispatch` | the next entry | 6000 rounds |
| C50 | `BossBeyd_Death` | message 0x21 | 2373 rounds |
| C51 | `BossBeyd_Death` | fight 0x1A | 2682 rounds |
| C52 | `BossBeyd_Death` | +1 = 4 (Death) | 5985 rounds |
| C53 | `BossBeyd_Death` | +0x2A on the Sprite_Current of before the call (Death) | 241 rounds |
| C54 | `BossBeyd_Death` | 0x802D20 = 3 (Message) | 2373 rounds |
| C55 | `BossBeyd_Hook` | the word's second byte flipped | 6000 rounds |
| C56 | `BossBeyd_HookPick` | 0x904AE2 at 2 or above | 158 rounds |
| C57 | `BossBeyd_HookPick` | 0x904B35 = 2 | 2215 rounds |
| C58 | `BossBeyd_HookPick` | bit 2 cleared | 1400 rounds |
| C59 | `BossBeyd_HookPick` | fight 0x1A | 2442 rounds |
| C60 | `BossBeyd_HookTick` | the wait word's low byte only | 1236 rounds |
| C61 | `BossBeyd_HookTick` | the round flags read before the call | 66 rounds |
| C62 | `BossBeyd_HookTick` | Draw_PassFlags = 1 | 1164 rounds |
| C63 | `BossBeyd_HookTick` | the flags' high byte cleared | 1157 rounds |
| C64 | `BossBeyd2_Dispatch` | the next entry | 6000 rounds |
| C65 | `BossBeyd2_Enter` | +8 = 2 | 6000 rounds |
| C66 | `BossBeyd2_Enter` | +8 on the Sprite_Current of before the call | 232 rounds |
| C67 | `BossBeyd2_Enter` | kind 30's hook | 6000 rounds |
| C68 | `BossBeyd2_Enter` | +0x8F = 0 | 6000 rounds |
| C69 | `BossBeyd2_ActDispatch` | the next entry | 6000 rounds |
| C70 | `BossBeyd2_Death` | animation 0xE | 6000 rounds |
| C71 | `BossBeyd2_Hook` | the word's second byte flipped | 6000 rounds |
| C72 | `BossBeyd2_HookTarget4` | target 5 | 6000 rounds |
| C73 | `BossZig_Dispatch` | the next entry | 6000 rounds |
| C74 | `BossZig_Enter` | +8 = 2 | 5969 rounds |
| C75 | `BossZig_Enter` | al ^ 1 | 6000 rounds |
| C76 | `BossZig_Idle` | bit 4 for bit 5 | 1729 rounds |
| C77 | `BossZig_Idle` | +0x110 bit 2 | 2130 rounds |
| C78 | `BossZig_Idle` | Sprite_Current read before the tick | 218 rounds |
| C79 | `BossZig_Idle` | animation 0xB | 1645 rounds |
| C80 | `BossZig_Step5Dispatch` | the other entry | 6000 rounds |
| C81 | `BossZig_Step5Count` | at 1 | 1668 rounds |
| C82 | `BossZig_Step5Count` | the second sound word | 1097 rounds |
| C83 | `BossZig_Step5Count` | +2 up by two | 1097 rounds |
| C84 | `BossZig_Step5Count` | +9 on the Sprite_Current of before the tick | 215 rounds |
| C85 | `BossZig_Step5Fire` | Rand & 7 | 615 rounds |
| C86 | `BossZig_Step5Fire` | bit 6 | 929 rounds |
| C87 | `BossZig_Step5Fire` | task parameter 3 | 1352 rounds |
| C88 | `BossZig_Step5Fire` | bit 3 | 2827 rounds |
| C89 | `BossZig_Step5Fire` | the actor for the target | 3701 rounds |
| C90 | `BossZig_Step5Fire` | 0x904AAD read again after the flags store | NOT REFUSED (equivalent: 0x904AA8 and 0x904AAD are different bytes, so reading 0x904AAD again after the flags store reads the same value; C90b (read before the two calls) refused) |
| C90b | `BossZig_Step5Fire` | 0x904AAD read before the two calls | 23 rounds |
| C91 | `BossZig_ActDispatch` | the next entry | 6000 rounds |
| C92 | `BossZig_Death` | animation 0xC | 6000 rounds |
| C93 | `BossZig_Hook` | the word's second byte flipped | 6000 rounds |
| C94 | `BossZig_HookPick` | target 4 | 2583 rounds |
| C95 | `BossZig_HookPick` | 0x904B35 = 1 | 3417 rounds |
| C96 | `BossZig_HookHit` | bit 4 | 4519 rounds |
| C97 | `Boss25_Setup` | AP saved for HP | 7973 rounds |
| C98 | `Boss25_Setup` | 0x904AE4 = 2 | 8000 rounds |
| C99 | `Boss25_Setup` | AP from max HP | 8000 rounds |
| C100 | `Boss25_Setup` | HP's high byte saved | 7976 rounds |
| C101 | `Boss25_Event` | turn by >> 2 | 2250 rounds |
| C102 | `Boss25_Event` | message 0x1E at 3 | 250 rounds |
| C103 | `Boss25_Event` | skill 0x47 | 190 rounds |
| C104 | `Boss25_Event` | enemy 0's HP 2 | 220 rounds |
| C105 | `Boss25_Event` | 0x904AAD not read again after the task | 44 rounds |
| C106 | `Boss25_Event` | the order's last 3 | 212 rounds |
| C107 | `Boss25_Event` | the order's length 6 | 212 rounds |
| C108 | `Boss25_Event` | the step not tested | 102 rounds |
| C109 | `Boss25_Event` | bits 0 and 1 set | 655 rounds |
| C110 | `Boss25_Event` | enemy 1 | 855 rounds |
| C111 | `Boss25_Event` | 0x904B35 = 0xFE | 60 rounds |
| C112 | `Boss25_Event` | member state 3 | 96 rounds |
| C113 | `Boss25_Event` | round 5 | 58 rounds |
| C114 | `Boss25_Event` | al 1 on the last path | 128 rounds |
| C115 | `Boss25_Event` | +0xBC = 0x65 | 212 rounds |
| C116 | `Boss25_Event` | 0x802D20 = 3 at round 2 | 69 rounds |
| C117 | `Boss25_End` | AP from the saved HP | 7978 rounds |
| C118 | `Boss25_End` | step 5 | 7831 rounds |
| C119 | `Boss25_Exit` | Field_MemberCount = 2 | 8000 rounds |
| C120 | `Boss25_Exit` | the count read before the call | 70 rounds |
| C121 | `Boss25_Exit` | whole dwords only | 5198 rounds |
| C122 | `Boss26_Setup` | 0x939A04 = 1 | 8000 rounds |
| C123 | `Boss26_Setup` | set-up 25's event hook | 8000 rounds |
| C124 | `Boss26_Setup` | 0x939A0C = 1 | 8000 rounds |
| C125 | `Boss26_Event` | member 2 not summed | 43 rounds |
| C126 | `Boss26_Event` | 0 summed | NOT REFUSED (equivalent: a word of 0 adds 0 and returns, which is what the original's fall-through does for a member target; C126b (-1 summed) refused) |
| C126b | `Boss26_Event` | -1 summed | 24 rounds |
| C127 | `Boss26_Event` | bit 7 only | 123 rounds |
| C128 | `Boss26_Event` | count up by two | 326 rounds |
| C129 | `Boss26_Event` | negative summed | 122 rounds |
| C130 | `Boss26_Event` | enemy 0 state 3 | 104 rounds |
| C131 | `Boss26_Event` | 0x904AE2 1 or above | 163 rounds |
| C132 | `Boss26_Event` | the turn's low byte | 135 rounds |
| C133 | `Boss26_Event` | bits 4 and 5 | 188 rounds |
| C134 | `Boss26_Event` | enemy 0's flag bit 0 | 341 rounds |
| C135 | `Boss26_End` | +0x1D | 5522 rounds |
| C136 | `Boss26_End` | +0x90 read before the call | 1607 rounds |
| C137 | `Boss26_End` | bit 1 | 6985 rounds |
| C138 | `Boss26_End` | variable 3 = 5 | 7926 rounds |
| C139 | `Boss26_End` | the pose not wrapped to a byte (equivalent?) | NOT REFUSED (equivalent: Sprite_PoseFromSet reads the animation's low byte only (Sprite_SetFrameQueueUpload's frame & 0xFF, the +0x4B byte compare) and the recorder masks it so; C135 (+0x1D) refused) |
| C140 | `Boss26_End` | size 0x1000 | 7677 rounds |
| C141 | `BossHook_ExitTransition4` | round-flag bit 0 | 5735 rounds |
| C142 | `BossHook_ExitTransition4` | transition 5 | 8000 rounds |

## 5. What nothing reached

The fuzz reached every branch the controls planted in. What only a fight
would show: which fight is which in play (the tool's rows and the code's
`0x19` / `0x1A` compares are all this doc says), what the three words
`0x903F0C..0x903F12` hold when kind 30 or 31 enters, what reads set-up 26's
two sums and its count, what `0x802D20` = 2 selects for a message, and the
end hooks' hand-back to the chapter.

## 6. Latent defects (Capcom's, kept)

- **Set-up 25 saves member 0's HP and AP as bytes and restores them as
  words.** `Boss25_Setup` stores the low byte of `+0x98` and of `+0x9A`
  (`mov al, byte ptr [0x802DD8]` into `0x675F05`) and `Boss25_End` writes
  them back zero-extended (`movzx ax, byte ptr [0x675F05]`): a member-0 HP
  or AP above 255 comes back as its value mod 256 after the fight. Whether
  the fight's member 0 can have that much, and what the PSX did, were not
  read. Kept (a living-game fix candidate: save the words).
- **Kind 30's entry copies enemy data record 0xFF when no record carries tag
  0x59.** In fight 0x1A `BossBeyd_Enter` passes `EnemyData_FindByTag(0x59)`
  to `Battle_CopyEnemyData` untested; "none" is 0xFF, and the copy reads
  `0x8C55C8 + 0xFF * 0x8C`, far past the eight records. Whether the area's
  data always carries the tag is the data's, not read.
- **Set-up 25's exit copy is unbounded**: up to 255 bytes from `0x939A10`
  into the party list `0x904065` by the byte `0x939A02`; its writer was not
  read, so whether it stays within the list is not known.
- **The dispatchers and hook tables index unchecked** (the state byte, the
  word's low byte); ours aborts. Only the engine's words 0..2 reach the hooks.

## 7. Calls across groups

**Out of BSE, to code nobody owns** (raw, `boss_se_callees.h`): `0x437450`
(the enemy sound), `0x4376A0` (an enemy action's end), `0x446DE0` /
`0x446E00` / `0x446E20` (the end phase's steps; the harness's standard set).
Stored, not called: `0x43B730` (B21 / B23's end hook, **BSD's**). Everything
else BSE calls or stores is ours by name (`BossActor_*`, `BareRet`,
`BareRetZero`, `BossHook_EndPickWay`, the engine's).

**Into BSE from outside the group:** none of our code names a BSE address
(`grep` of `src/`). In Capcom's code: `Boss_SetupTable` entries 22..26, 30,
48 and `BossKind_Table` entries 28..32, 55 (the roots), and **`0x43E740`**
(set-up 35's, group BSH's) storing `0x43E790` (`BossHook_ExitTransition4`) as
its exit hook - an E8 / immediate scan of the image, 2026-09-28. Every other
reference is the group's own.

## 8. For `analysis/calltrace/entries_logic.txt`

The 52 extents of section 1 appended to the main checkout's file
(2026-09-28), none there before; they fix the host lines `0043B130 18BF`,
`0043C9F0 189F` (set-up 30's two) and `0043E290 1120`, which covered them.
