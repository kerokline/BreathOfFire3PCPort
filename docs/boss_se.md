# Group BSE: fights 22..26, 30 and 48, kinds 28..32 and 55

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave one, stage B. 52 functions of `0x43B5B0..0x43E7A0` ours
(`src/game/boss_se.cpp`, shadow `boss_se`), each read to its last instruction
with capstone and fuzzed through the boss harness ([`boss_harness.md`](boss_harness.md)),
thirteen `Run`s, 0 mismatches; CONTROLS_SUMMARY. Fuzz only: no recorded
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

RUN_TABLE

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

`BOF3X_SHADOW='*'` (every group of every harness, this worktree): STAR_RESULT

## 4. Controls

`python controls.py` (the group's scratch script): each control one textual
change to ours anchored on a string that occurs once, rebuild, the one `Run`
that holds the function, restore, rebuild at the end. A dispatcher's plant is
made in the shared `Dispatch` helper, keyed on the function's name (the next
entry of its table). CONTROLS_DETAIL

CONTROLS_TABLE

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
