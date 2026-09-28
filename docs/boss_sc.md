# Group BSC: fights 11, 12, 14, 15, 16 and 46, and kinds 12..17 and 53

**Status:** MEASURED (2026-09-28) - round eleven
([`takeover-queue-bosses.md`](takeover-queue-bosses.md)), wave one, stage B.
All 53 functions of the group are ours (`src/game/boss_sc.cpp`, shadow name
`boss_sc`), each read to its last instruction with capstone and fuzzed
through the boss harness ([`boss_harness.md`](boss_harness.md)) without
edits to it: 13 `Run`s, 318,000 rounds, 0 mismatches. 145 controls planted, all refused by a count (one, C128b, an equivalent mutant the harness refuses for its own reason - section 6). Fuzz
only: no recorded route reaches a boss fight.

Enemy and fight names below are `tools/boss_rows.py --disc`'s (the US disc's
area records), not memory of the game; where the tool leaves a fight open
(B46) it stays open.

| Unit | Root | The tool's fight / enemy | Extent | Fns |
|---|---|---|---|--:|
| B11 | `Boss_SetupTable[11]` | id 11, `BOSS008`, area 27 row 4 (kinds 8..11: Torast, Kassen, Galtel, Doksen - the row's one not read) | `0x439410..0x439497` | 3 |
| K12 | `BossKind_Table[12]` | Amalgam, area 28 | `0x4396B0..0x439B06` | 11 |
| B12 | `Boss_SetupTable[12]` | id 12, `BOSS012`, Amalgam, area 28 row 7 | `0x4398E0..0x439920` | 2 |
| K13 | `BossKind_Table[13]` | Balio, areas 11 and 41 | `0x439B10..0x439C58` | 5 |
| K14 | `BossKind_Table[14]` | Sunder, areas 11 and 41 | `0x439C60..0x439DF3` | 5 |
| K17 | `BossKind_Table[17]` | Nina, area 41 | `0x439E00..0x439FD5` | 8 |
| B16 | `Boss_SetupTable[16]` | id 16, `BOSS013`, row 7, kinds 13, 14, 17 | `0x43A030..0x43A355` | 4 |
| K15 | `BossKind_Table[15]` | Rocky, area 26 | `0x43A360..0x43A3CF` | 3 |
| K53 | `BossKind_Table[53]` | Sample 8, area 164 | `0x43A3D0..0x43A45F` | 3 |
| B14 | `Boss_SetupTable[14]` | id 14, `BOSS014`, area 26 row 7, kinds 15 and 53 | `0x43A460..0x43A499` | 2 |
| B46 | `Boss_SetupTable[46]` | id 46, `BOSS046` (`BOSS014`'s image by the sibling's dedup), row 7, no kinds of its own - **left open** | `0x43A4B0..0x43A4CE` | 1 |
| K16 | `BossKind_Table[16]` | Pooch, area 26 | `0x43A4D0..0x43A53F` | 3 |
| B15 | `Boss_SetupTable[15]` | id 15, `BOSS015`, Pooch, area 26 row 6 | `0x43A540..0x43A588` | 3 |

The extents are `tools/boss_rows.py --unit <UNIT> --clones` (2026-09-28),
and `analysis/boss_funcs.tsv`'s group column BSC lists the same 53 starts.
Every row was read against the code: every start is a function (the three
of B11 carry the catalogue's "Not functions" label, which is wrong - they are
the set-up and its two hooks), none was found inside another's extent, none
is missing. The rows (`EventBattle_Records` `+2`) are read off the exe:
id 11's is 4, 14's and 46's 7. Fight 13 (`B13`, `0x438E30`) is BSB's; the
kinds 13, 14 and 17 are shared by fights 13 and 16 and branch on the fight
byte (`0x904AAA` compared with 0xD and 0x10 - section 1.3). B46: its end
hook is `BossHook_EndPickWay`, the one chapter 15's ten set-ups share, and
its record's file row is its own; with K53 (Sample 8, area 164) the second
kind of `BOSS014`'s image, Sample 8's fight is the likely reading - the code
does not settle it, so it stays open (plan section 7).

The sibling's image sizes agree in order with the units: `BOSS012` 1,752
bytes (Amalgam: 13 PC functions), `BOSS013` 3,812 (three kinds and two
set-ups), `BOSS014` 956 (two kinds, one set-up), `BOSS015` 492 (one kind,
one set-up) (`analysis/overlay_captures_all.json`).

## 1. What each function does

`symbols.toml` gives each to the instruction; in outline.

### 1.1 The set-ups and their hooks

Every set-up stores the three hooks and returns (`kSetup`). The end hooks
(`BattleHook_End`, called once as the way out is picked) store a byte of the
move-script counters (`0x903848`, `MoveScript_CounterTest`'s: the scene the
field runs next - what each value selects was not read) on the win and hand
the battle to the end phase.

| Address | Name | What |
|---|---|---|
| `0x439410` | `Boss11_Setup` | End `Boss11_End`, Exit `Boss11_Exit`, Event `BareRetZero` |
| `0x439430` | `Boss11_End` | won (`0x904AE8` bit 1): `0x903848 = 0x44`, `0x446DE0`; else `0x446E00` |
| `0x439450` | `Boss11_Exit` | `BossActor_ClearBit40(3)`, `Sprite_Current = BossActor_Find(3)`, bank 0x83, animation 0, the actor's `+0x58` / `+0x5A` = enemy 0's |
| `0x4398E0` | `Boss12_Setup` | End `Boss12_End`, Exit `BareRet`, Event `BareRetZero` |
| `0x439900` | `Boss12_End` | won: `0x903848 = 0x23`, `0x904AE8 |= 8` (the byte read once), `0x446DE0`; else `0x446E00` |
| `0x43A030` | `Boss16_Setup` | End `Boss16_End`, Exit `Boss16_Exit`, Event `Boss16_Event`; the wait count `0x675F00 = (Rand() & 2) + 8` |
| `0x43A060` | `Boss16_Event` | `al` 0 on every path, by the code's low byte - 2 (`BattleRoundEnd_NextRound`): unless `0x904AAD` bit 6 or a wait count left, the name banner (below), `Sprite_Current` = `0x939AD8` = enemy 2, `Field_MemberSprite(1, 1)`, `+0xFC` = `BossNina_Anims2`, `+0x29 = 4`, `Sprite_SetAnimation(+8 + 4)`, bit 6; 1: with the acting actor 0, the target's bit 6 and not `0x904AAD` bit 0, action 0 and line 0x27; 0: enemy 0's or 1's status bit 0x2000 sets `0x904AAD` bit 3 |
| `0x43A190` | `Boss16_End` | won: `0x904AEC` = enemy 1's word `+0x96` + enemy 0's, `0x903848 = 0x14`, `0x446DE0`; else (with bit 6: enemy 2 bank 0xF9, `+0x24 &= 0xFE`, `+0x2A = 1`, animation 1) `0x903848 = 0x50`, `0x446E20`; then `0x9039A2 &= 0xFF7F`, `0x904131 = 0x31` |
| `0x43A230` | `Boss16_Exit` | unless won: actors 2 (without bit 0), 1 and 0 found by tag and posed (banks 0xF9 / 0xEF / 0xEE; actor 2 animation 1 or `Sprite_SetAnimationAt(5, 4)` by bit 6; 1 and 0 given enemies 1 and 0's `+0x58` / `+0x5A`); then `Scenario_CallA(1)`, member 1's `+0 |= 0x40`, `0x92BF18` = 6, or 7 when `0x903848` (read after the call) is not 0x14 |
| `0x43A460` | `Boss14_Setup` | End `Boss14_End`, Exit `BossHook_ExitClearActor0`, Event `BareRetZero` |
| `0x43A480` | `Boss14_End` | won: `0x903848 = 0xC`, `0x446DE0`; else `0x446E00` |
| `0x43A4B0` | `Boss46_Setup` | End `BossHook_EndPickWay`, Exit `BareRet`, Event `BareRetZero` |
| `0x43A540` | `Boss15_Setup` | End `Boss15_End`, Exit `Boss15_Exit`, Event `BareRetZero` |
| `0x43A560` | `Boss15_End` | won: `0x903848 = 0x4B`, `0x446DE0`; else `0x446E00` |
| `0x43A580` | `Boss15_Exit` | `BossActor_Clear(1)` |

The name banner (`Boss16_Event` code 2 and `BossNina_HookHit`, the same
instructions twice): `Battle_OpenMsgWindow`; `Str_CopyN(Text_Records, the
live character record 0x903A70 + 0xA4 * byte 0x66972D, 8)` (the byte read
after the call); `BattleBanner_Add(2, 0, 0, 0x2D, Msg_SystemPtr(0x25))`.

### 1.2 The kinds' dispatchers and tables

Each kind's dispatcher is `jmp [table + 4 * Sprite_Current +1]` over its
twelve-entry `+1` table (`kDispatch`); each `+0xF4` hook is `jmp [table +
4 * (word & 0xFF)]` over three entries (`kEnemyHook`). The `+1` tables are
the generic enemy's (`EnemyOp_Idle`, `EnemyOp_Wait`, `0x4365D0`,
`0x436620`, `EnemyOp_ActDispatch`, `0x437030`, `0x437180`, `0x437240`,
`EnemyOp_HitPose`, `Port_DroppedCall` at 1 and 10) with the kind's entrance
at 0 - and Amalgam's own act table at 6, Nina's walk at 9.

**The jmp leaves the stack word above the return address to the entry**, and
two entries read it (`Port_DroppedCall`'s byte, a bare `ret`; the hook
entries their word). So ours of every dispatcher takes that word and hands
it on (`unsigned long Name(unsigned long through)`), and answers the entry's
`eax`. BH's `Dispatch` (`boss_h.cpp`) takes none: its tables hold no entry
that reads the word, so it is exact for them; a stage-B dispatcher whose
table holds `Port_DroppedCall` needs this shape (controls C8 and C74, the
word not handed on, are refused on states 1 and 10).

| Kind | Dispatcher | State 0 (the entrance) | Hook | Hook entries 0 (the action pick) / 1 (the hit) / 2 |
|---|---|---|---|---|
| 12 Amalgam | `0x4396B0` `BossAmalgam_Dispatch` | `0x4396D0` `BossAmalgam_Enter` | `0x4398D0` `BossAmalgam_Hook` | `BareRet` x3 |
| 13 Balio | `0x439B10` `BossBalio_Dispatch` | `0x439B30` `BossBalio_Enter` | `0x439BC0` `BossBalio_Hook` | `0x439BD0` `BossBalio_HookAct` / `0x439C40` `BossBalio_HookHit` / `BareRet` |
| 14 Sunder | `0x439C60` `BossSunder_Dispatch` | `0x439C80` `BossSunder_Enter` | `0x439D10` `BossSunder_Hook` | `0x439D20` `BossSunder_HookAct` / `0x439DB0` `BossSunder_HookHit` / `BareRet` |
| 17 Nina | `0x439E00` `BossNina_Dispatch` | `0x439E20` `BossNina_Enter` | `0x439EE0` `BossNina_Hook` | `0x439EF0` `BossNina_HookAct` / `0x439F50` `BossNina_HookHit` / `BareRet` |
| 15 Rocky | `0x43A360` `BossRocky_Dispatch` | `0x43A380` `BossRocky_Enter` | `0x43A3C0` `BossRocky_Hook` | `BareRet` x3 |
| 53 Sample 8 | `0x43A3D0` `BossSample8_Dispatch` | `0x43A3F0` `BossSample8_Enter` | `0x43A450` `BossSample8_Hook` | `BareRet` x3 |
| 16 Pooch | `0x43A4D0` `BossPooch_Dispatch` | `0x43A4F0` `BossPooch_Enter` | `0x43A530` `BossPooch_Hook` | `BareRet` x3 |

Every entrance stores `0x939AD8`'s `+0xFC` (the animation bytes
`BattleEnemy_SetAnimation` reads), `+0xF4` (the hook) and `+0xF8` (bytes whose
reader was not traced), in that order, and `+1 = 2`. Beyond that:
Amalgam's hands `0x455290` its sprite and `BossAmalgam_SlotScripts` (a
`Field_Slots` record, the answer not read) and tail-jumps to
`Sprite_ScriptTick`; Rocky's, Pooch's and Sample 8's tail-jump to it too
(Sample 8's with `0x939AD8 +0x114 |= 8`, and **Rocky's animation bytes**);
Balio's and Sunder's set, in fight 13 (`0x904AAA` 0xD), the words `+0xD0`,
`+0xB0` and HP `+0xA4` to 0xFFFF, then bank 0xEE / 0xEF, animation 0,
`+0x2A = 1`; Nina's sets `+8 = 1`.

### 1.3 Kinds 13, 14 and 17: fights 13 and 16

The three kinds serve both fights of `BOSS013` and read the fight byte:

- `BossBalio_HookAct` / `BossSunder_HookAct` (the action pick) act only in
  fight 16 (`0x904AAA` 0x10): by the fight's progress bits `0x904AAD` they
  set the enemy's action `0x904B35` (0 or 3), the window pass byte
  `0x802D20 = 2`, open a line (`Msg_OpenScript` 0x26 / 0x29 for Balio, 0x25 /
  0x28 / 0x2A for Sunder) and set the bit that shows it (bits 1, 5 / 2, 4;
  Sunder's 0x2A line on bit 5 sets none, so it repeats). The bits are read
  again after each call.
- `BossBalio_HookHit` and `BossSunder_HookHit` (the hit) put HP 0 back to 1,
  in any fight; Sunder's also, in fight 16 with the acting actor 8, the word
  `+0x108` 0 to 1.
- `BossNina_HookAct`: with `0x904AAD` bit 0, action 3; else while the wait
  count `0x675F00` (the set-up's 8 or 10) is not 0, action 0 and the count
  down; at 0, action 1, the target `0x904B44 = 4`, `0x904AE2` down by one and
  bit 0 set. `BossNina_HookHit`: the name banner, the wait count 0,
  `Field_MemberSprite(1, 1)`, `+0x29 = 4`, `+0xFC = BossNina_Anims2`,
  `Sprite_SetAnimation(+8 + 4)`, bit 6. `Boss16_Event`'s code 2 does the same
  with `Sprite_Current` and `0x939AD8` set to enemy 2 first.
- Nina's `+1` entry 9 is her own `BossNina_WalkDispatch` (by `+2` over
  `BossNina_WalkSteps`: `BossNina_WalkStart` - bank 0xF9, `+0x24 &= 0xFE`,
  animation 2, `+2` up - then `BossNina_WalkStep` - with `+0` bit 7 `+2` up,
  else the dword `+0x34` up by 0x8000 and two script ticks - then
  `0x4373C0`).

What the bits and lines are in play is not measured.

### 1.4 Amalgam's death

`BossAmalgam_Steps` 6 is `BossAmalgam_ActDispatch` (by `+2` over
`BossAmalgam_ActSubs`: `EnemyOp_ActSubs` with the death at 4, as kinds
8..11's), and entry 4 `BossAmalgam_DeathDispatch` (by `+3` over four steps):

| Address | Name | What |
|---|---|---|
| `0x439770` | `BossAmalgam_DeathTick` | `Sprite_ScriptTick`; done: `+3 = 1` |
| `0x439790` | `BossAmalgam_DeathStart` | round flags `0x904AA9 |= 4`; `Sprite_ScriptTickOnce`; `Gfx_ClearRect(0x340, 0x100, 0x80, 0x100)`; `Sound_PlayById(0x601)`; `+0x18` / `+0x1C` = the place (`+0x2E` / `+0x30` signed), the row `+0x20 = 0`, the place moved to (0x32, 0x41), `+0x24 |= 0x88`; `BossAmalgam_DrawSprite(row)`; `+3 = 2` |
| `0x439830` | `BossAmalgam_DeathMelt` | `BossAmalgam_DrawSprite(row)`, `BossAmalgam_DrawStreak(row)`, the row up by 2; at 0x56 `+3 = 3` |
| `0x439870` | `BossAmalgam_DeathEnd` | `0x454A80(Sprite_Current)` (its field slots released), `Battle_EnemyDefeated`, `+0x110 |= 0x1000`, `+0 &= 0xBF`, states 3, 0, 0 |
| `0x439930` | `BossAmalgam_DrawSprite` | a SPRT of the VRAM rect `(0x340, 0x100)`: 0x68 x 0x56 from row v, at `(+0x18 - 0x28, +0x20 + +0x1C - 0x3C)` |
| `0x4399E0` | `BossAmalgam_DrawStreak` | a POLY_FT4 stretching texture row v of the same rect from the screen's top (y 0) down to that y, 0x68 wide |

So the sprite is drawn from the cleared VRAM rect for 43 frames of the
melt (rows 0..0x54), its top row advancing by two each frame while the row at that height is stretched up
to the top of the screen; how it looks is not measured.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original indexes a table unchecked (the ten dispatchers, the
seven hook tables) or would write through a null actor (`Boss11_Exit`,
`Boss16_Exit`), ours aborts with a `Fatal` naming the function (the owner's
rule, round9 doc section 6); nothing reaches it. Two functions answer `eax`
as the original leaves it on a path the engine does not read
(`BossAmalgam_DeathTick`, `BossNina_WalkStep`: the sprite's address when
they store through it); ours answers the same.

## 3. Calls across groups

**Out of BSC, to code nobody owns** (raw, `boss_sc_callees.h`):
`0x446DE0`, `0x446E00`, `0x446E20` (the end phase's steps 1..3; in the
harness's standard set), `0x454A80` (the field slots released for an
object, read by AR2B) and `0x455290` (a field slot taken for an object and
a script, read here). Everything else is ours and called by name: BH's
spawn helpers (`BossActor_ClearBit40`, `_Find`, `_Clear`), the battle and
sprite engine. BH's `BareRet`, `BareRetZero`, `BossHook_ExitClearActor0` and
`BossHook_EndPickWay` are **stored** (hook literals), never called.

The table entries the kinds reach and nobody owns (handler recorders in the
fuzz): `0x4365D0`, `0x436620`, `0x436BC0`, `0x436F00`, `0x437030`,
`0x437180`, `0x437240`, `0x4373C0`.

**Into BSC from outside the group**: none of our code names any of the 53
(grep of `src/`, 2026-09-28). Capcom's reaches them through
`Boss_SetupTable`, `BossKind_Table`, the hook cells and the kinds' tables,
all through the entries' `jmp`.

## 4. Named data (`symbols.toml` `[[data]]`)

32 tables, all in `.data` `0x64CBC4..0x64CE73`: for each kind its `+1`
table (`Boss*_Steps`, 12), its `+0xF4` hook table (`Boss*_Hooks`, 3), its
`+0xFC` animation bytes (`Boss*_Anims`, 12; Nina's second `BossNina_Anims2`;
Sample 8 has none of its own - it stores `BossRocky_Anims`), its `+0xF8`
bytes (`Boss*_F8`, 8), Amalgam's `BossAmalgam_ActSubs` (6),
`BossAmalgam_DeathSteps` (4) and `BossAmalgam_SlotScripts` (4 pointers),
Nina's `BossNina_WalkSteps` (3). The extents are the code's: each table runs
to the next address the code names (the tool's "25 code entries" for
`0x64CC24` is its twelve, then the `+2` / `+3` / hook tables).

## 5. The fuzz

`BOF3X_SHADOW=boss_sc`, `src/game/boss_sc_fuzz.cpp`: one `Run` per unit
(`BOF3X_BSC_RUN=b11|k12|...` runs one), every function called directly in
its engine shape - `kSetup` (the first group to use it: it lacked nothing),
`kEnd`, `kExit`, `kEvent` (`ret_mask` 0xFF), `kDispatch` (the state byte
drawn below the table: `+1` below 12, Amalgam's `+2` below 6 and `+3` below
4, Nina's `+2` below 3), `kEnemyHook` (the dispatchers and their entries),
`kState` (`ret_mask` 0xFF where it tail-jumps to `Sprite_ScriptTick` or
answers the sprite), `kCallee` (the two draws). No `kTask` (the group has no
effect task) and no `Clone::via` (every function is reachable directly).
The kinds' tables are `DataTable`s; the group's callees beyond the standard
set are `0x455290`, `0x454A80` and the two draws (a byte each), and four
standard ones listed again with an effect louder than the real callee
(`Msg_OpenScript` moves `0x904AAD`, `Scenario_CallA` and `0x446DE0` the
scene byte half the time, `Battle_OpenMsgWindow` the banner's character:
the cells their callers read again after the call). Regions
beyond the battle frame: the move-script counters `0x903848`, `0x9039A2`,
`0x904131`, `0x92BF18`, `0x675F00`, `0x802D20`, `0x66972D`. 6,000 rounds
each.

| Run | Fight, kind | Clones | Rounds | Calls | Result |
|---|---|--:|--:|--:|---|
| `b11` | fight 11 | 3 | 18,000 | 30,000 | 0 mismatches |
| `k12` | fight 12, kind 12 | 11 | 66,000 | 156,000 | 0 mismatches |
| `b12` | fight 12 | 2 | 12,000 | 6,000 | 0 mismatches |
| `k13` | fight 16, kind 13 | 5 | 30,000 | 25,018 | 0 mismatches |
| `k14` | fight 16, kind 14 | 5 | 30,000 | 25,170 | 0 mismatches |
| `k17` | fight 16, kind 17 | 8 | 48,000 | 72,072 | 0 mismatches |
| `b16` | fight 16 | 4 | 24,000 | 54,242 | 0 mismatches |
| `k15` | fight 14, kind 15 | 3 | 18,000 | 18,000 | 0 mismatches |
| `k53` | fight 14, kind 53 | 3 | 18,000 | 18,000 | 0 mismatches |
| `b14` | fight 14 | 2 | 12,000 | 6,000 | 0 mismatches |
| `b46` | fight 46 | 1 | 6,000 | 0 | 0 mismatches |
| `k16` | fight 15, kind 16 | 3 | 18,000 | 18,000 | 0 mismatches |
| `b15` | fight 15 | 3 | 18,000 | 12,000 | 0 mismatches |

(This worktree's counts; they move with the build directory.)

**Seeds.** The win byte `0x904AE8` at 0..3 and bytes with bit 1 set or not;
the fight byte at 0xD, 0x10 and neighbours for the entrances and hooks that
compare it; `0x904AAD` a random subset of its low six bits (the action
hooks) or bit 0 / 3 / 6 combinations (Nina's hooks, fight 16's hooks); the
wait count at 0, 1, 2, 8, 0xA, 0x80, 0xFF, 0 half the time for the event
hook; the acting actor 0 or 8 and neighbours; the target with and without
bit 6; the two status bytes with and without 0x20; HP and `+0x108` at 0, 1,
2, 0xFFFF, 0x100; the scene byte 0x14 and neighbours; Amalgam's row at and
around 0x56 (0x52, 0x54..0x57, 0, -2 and with bits above); `+0` bit 7 at
both values; the dispatchers' other state bytes inside their tables (a
dispatcher reading the wrong byte then lands on an entry, a count, rather
than past its table). **Arguments**: a hook's word with garbage above the
byte half the time; the event code 0..2 two times in three; the draws' row
byte from the melt's rows (0..0x56 in twos) or any, garbage above.
**Disturbance** (the group's): `0x904AAD`, the scene byte (0x14 half the
time), the wait count, the banner's character, member 1's flags - what the
functions read again after a call.

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the final
build): exit 0, all 839 self-test lines 0 mismatches, BSC's 13 `Run`s among them with the counts above; it passed first time both times it was run (no silent death).

## 6. Controls

`python controls.py` (the group's scratch script): each control one textual
change to `boss_sc.cpp`, anchored on a string that occurs once, then
rebuild, run the unit (`BOF3X_BSC_RUN`), restore, rebuild. Every one of the
53 functions has at least one. Run three times; the table is the third run, on the final seeds. **145 planted, 145 refused by a count, none by a Fatal**, one of them (C128b) equivalent in the game. The first run found what the seeds lacked: four dispatcher plants reading the wrong state byte found it past the table (a Fatal, now counts: the other state bytes are seeded inside their tables), six plants that dispatched through another kind's table ran that kind's unswapped code and crashed (replaced by "the table one entry on"), and the re-read plants were thin (3 and 5 rounds for the action hooks' bits re-read after `Msg_OpenScript`): `Msg_OpenScript`, `Scenario_CallA`, `Battle_OpenMsgWindow` and `0x446DE0` got stand-ins that move the cell their caller reads again. The second run showed the scene stand-in overwrote the end hooks' own store and hid five scene-byte plants: it now moves the byte half the time. The thinnest refusals now: C69 (117 rounds), C104 (143 rounds), C103 (150 rounds), C101 (199 rounds), C80 (201 rounds), C71 (209 rounds).

| # | Function | Plant | Refused |
|---|---|---|---|
| C1 | `Boss11_Setup` | the event hook BareRet | 6000 rounds |
| C2 | `Boss11_End` | scene 0x45 | 1463 rounds |
| C3 | `Boss11_Exit` | the actor tagged 2 | 6000 rounds |
| C4 | `Boss11_Exit` | bank 0x84 | 6000 rounds |
| C5 | `Boss11_Exit` | the pose words swapped | 6000 rounds |
| C6 | `Boss11_Exit` | Sprite_Current read before the calls | 501 rounds |
| C7 | `BossAmalgam_Dispatch` | by +2 | 5442 rounds |
| C8 | `BossAmalgam_Dispatch` | the stack word not handed on | 967 rounds |
| C9 | `BossAmalgam_Enter` | +0xF8 four on | 6000 rounds |
| C10 | `BossAmalgam_Enter` | Balio's hook | 6000 rounds |
| C11 | `BossAmalgam_Enter` | the script one on | 6000 rounds |
| C12 | `BossAmalgam_Enter` | state 3 | 5956 rounds |
| C13 | `BossAmalgam_Enter` | Sprite_ScriptTickOnce | 6000 rounds |
| C14 | `BossAmalgam_ActDispatch` | by +3 | 4489 rounds |
| C15 | `BossAmalgam_DeathDispatch` | by +2 | 4446 rounds |
| C16 | `BossAmalgam_DeathTick` | step 2 | 4006 rounds |
| C17 | `BossAmalgam_DeathTick` | al 1 when not done | 1994 rounds |
| C18 | `BossAmalgam_DeathStart` | bit 3 | 4513 rounds |
| C19 | `BossAmalgam_DeathStart` | w and h swapped | 6000 rounds |
| C20 | `BossAmalgam_DeathStart` | x unsigned | 3000 rounds |
| C21 | `BossAmalgam_DeathStart` | +0x30 = 0x42 | 5967 rounds |
| C22 | `BossAmalgam_DeathStart` | +0x24 |= 0x80 | 3072 rounds |
| C23 | `BossAmalgam_DeathStart` | Sprite_Current not read after the call | 231 rounds |
| C24 | `BossAmalgam_DeathStart` | sound 0x602 | 6000 rounds |
| C25 | `BossAmalgam_DeathMelt` | up by 1 | 6000 rounds |
| C26 | `BossAmalgam_DeathMelt` | at 0x56 and above | 4318 rounds |
| C27 | `BossAmalgam_DeathMelt` | the row from +0x21 | 5547 rounds |
| C28 | `BossAmalgam_DeathMelt` | Sprite_Current read before the second draw | 224 rounds |
| C29 | `BossAmalgam_DeathEnd` | bit 8 | 4492 rounds |
| C30 | `BossAmalgam_DeathEnd` | +3 = 1 | 6000 rounds |
| C31 | `BossAmalgam_DeathEnd` | 0x939AD8 for the sprite | 1711 rounds |
| C32 | `BossAmalgam_Hook` | the word handed on masked | 2988 rounds |
| C33 | `BossAmalgam_DrawSprite` | x - 0x27 | 219 rounds |
| C34 | `BossAmalgam_DrawSprite` | h 0x55 | 6000 rounds |
| C35 | `BossAmalgam_DrawSprite` | size 0x18 | 6000 rounds |
| C36 | `BossAmalgam_DrawSprite` | dtd 1 | 6000 rounds |
| C37 | `BossAmalgam_DrawSprite` | y with +0x1C subtracted | 6000 rounds |
| C38 | `BossAmalgam_DrawSprite` | v halved | 5935 rounds |
| C39 | `BossAmalgam_DrawStreak` | the first tpage | 5999 rounds |
| C40 | `BossAmalgam_DrawStreak` | right + 0x48 | 1142 rounds |
| C41 | `BossAmalgam_DrawStreak` | u 0x67 at corner 3 | 6000 rounds |
| C42 | `BossAmalgam_DrawStreak` | size 0x40 | 6000 rounds |
| C43 | `BossAmalgam_DrawStreak` | corner 1 y 1 | 6000 rounds |
| C44 | `BossAmalgam_DrawStreak` | the packet read before the commit | 6000 rounds |
| C45 | `Boss12_Setup` | exit hook BossHook_ExitClearActor0 | 6000 rounds |
| C46 | `Boss12_End` | bit 2 | 2680 rounds |
| C47 | `Boss12_End` | scene 0x24 | 1493 rounds |
| C48 | `Boss12_End` | bit 0 for the win | 3535 rounds |
| C49 | `BossBalio_Dispatch` | the table one entry on | 6000 rounds |
| C50 | `BossBalio_Enter` | bank 0xEF | 6000 rounds |
| C51 | `BossBalio_Enter` | fight 16 for 13 (shared with Sunder) | 2022 rounds |
| C52 | `BossBalio_Enter` | no +0xB0 (shared with Sunder) | 1320 rounds |
| C53 | `BossBalio_Enter` | +0x2A = 2 (shared with Sunder) | 6000 rounds |
| C54 | `BossBalio_Hook` | Sunder's hook table | 6000 rounds |
| C55 | `BossBalio_HookAct` | line 0x27 | 537 rounds |
| C56 | `BossBalio_HookAct` | action 0 | 510 rounds |
| C57 | `BossBalio_HookAct` | bit 2 for bit 1 | 493 rounds |
| C58 | `BossBalio_HookAct` | the bits not re-read after the call | 242 rounds |
| C59 | `BossBalio_HookAct` | bit 4 set | 446 rounds |
| C60 | `BossBalio_HookAct` | fight 13 | 1252 rounds |
| C61 | `BossBalio_HookHit` | the floor 2 (shared with Sunder) | 1287 rounds |
| C62 | `BossSunder_Dispatch` | the table one entry on | 6000 rounds |
| C63 | `BossSunder_Enter` | bank 0xEE | 6000 rounds |
| C64 | `BossSunder_Enter` | Balio's +0xF8 | 6000 rounds |
| C65 | `BossSunder_Hook` | the word handed on masked | 2972 rounds |
| C66 | `BossSunder_HookAct` | line 0x28 first | 488 rounds |
| C67 | `BossSunder_HookAct` | bit 1 for bit 0 | 843 rounds |
| C68 | `BossSunder_HookAct` | action 0 | 509 rounds |
| C69 | `BossSunder_HookAct` | the bits not re-read after the call | 117 rounds |
| C70 | `BossSunder_HookAct` | bit 5 for bit 4 | 252 rounds |
| C71 | `BossSunder_HookHit` | actor 8 and above | 209 rounds |
| C72 | `BossSunder_HookHit` | +0x108 = 1 whatever it was | 307 rounds |
| C73 | `BossSunder_HookHit` | any fight | 261 rounds |
| C74 | `BossNina_Dispatch` | the stack word not handed on | 996 rounds |
| C75 | `BossNina_Enter` | +8 = 2 | 6000 rounds |
| C76 | `BossNina_Enter` | the second animation bytes | 6000 rounds |
| C77 | `BossNina_WalkDispatch` | by +3 | 4018 rounds |
| C78 | `BossNina_WalkStart` | animation 3 | 6000 rounds |
| C79 | `BossNina_WalkStart` | bit 1 cleared too | 2985 rounds |
| C80 | `BossNina_WalkStart` | Sprite_Current read before the call | 201 rounds |
| C81 | `BossNina_WalkStep` | bit 6 | 2955 rounds |
| C82 | `BossNina_WalkStep` | step 0x10000 | 2998 rounds |
| C83 | `BossNina_WalkStep` | one tick | 2999 rounds |
| C84 | `BossNina_WalkStep` | +3 up | 3001 rounds |
| C85 | `BossNina_Hook` | Balio's hook table | 6000 rounds |
| C86 | `BossNina_HookAct` | action 0 with bit 0 | 2577 rounds |
| C87 | `BossNina_HookAct` | down by 2 | 2766 rounds |
| C88 | `BossNina_HookAct` | target 5 | 657 rounds |
| C89 | `BossNina_HookAct` | 0x904AE2 up | 657 rounds |
| C90 | `BossNina_HookAct` | action 2 | 657 rounds |
| C91 | `BossNina_HookHit` | slot 2 | 6000 rounds |
| C92 | `BossNina_HookHit` | +0x29 = 5 | 6000 rounds |
| C93 | `BossNina_HookHit` | animation +8 + 3 | 6000 rounds |
| C94 | `BossNina_HookHit` | nine bytes (shared with Boss16_Event) | 6000 rounds |
| C95 | `BossNina_HookHit` | message 0x26 (shared) | 6000 rounds |
| C96 | `BossNina_HookHit` | the character read before the call (shared) | 5972 rounds |
| C97 | `BossNina_HookHit` | layer 1 (shared) | 6000 rounds |
| C98 | `Boss16_Setup` | Rand & 3 | 3051 rounds |
| C99 | `Boss16_Setup` | end and exit swapped | 6000 rounds |
| C100 | `Boss16_Event` | the whole word | 671 rounds |
| C101 | `Boss16_Event` | enemy 1's bit 0x4000 | 199 rounds |
| C102 | `Boss16_Event` | bit 2 | 907 rounds |
| C103 | `Boss16_Event` | target bit 7 | 150 rounds |
| C104 | `Boss16_Event` | action 3 | 143 rounds |
| C105 | `Boss16_Event` | the wait count not tested | 528 rounds |
| C106 | `Boss16_Event` | enemy 1 as the sprite | 509 rounds |
| C107 | `Boss16_Event` | Nina's first animation bytes | 535 rounds |
| C108 | `Boss16_Event` | al 1 for the other codes | 1102 rounds |
| C109 | `Boss16_End` | scene 0x15 | 1512 rounds |
| C110 | `Boss16_End` | enemy 2's word | 3040 rounds |
| C111 | `Boss16_End` | step 2 for 3 | 2960 rounds |
| C112 | `Boss16_End` | bit 6 cleared too | 2998 rounds |
| C113 | `Boss16_End` | 0x30 | 6000 rounds |
| C114 | `Boss16_End` | animation 2 | 1511 rounds |
| C115 | `Boss16_End` | scene 0x51 | 2938 rounds |
| C116 | `Boss16_Exit` | the two words swapped | 913 rounds |
| C117 | `Boss16_Exit` | bit 1 for bit 0 | 1507 rounds |
| C118 | `Boss16_Exit` | bit 5 for 6 | 950 rounds |
| C119 | `Boss16_Exit` | actor 1 bank 0xEE | 3016 rounds |
| C120 | `Boss16_Exit` | actor 1 from enemy 0's +0x58 | 2346 rounds |
| C121 | `Boss16_Exit` | 6 and 7 swapped | 6000 rounds |
| C122 | `Boss16_Exit` | the scene byte read before the call | 1480 rounds |
| C123 | `Boss16_Exit` | bit 5 | 4471 rounds |
| C124 | `Boss16_Exit` | bit 0 for the win | 3708 rounds |
| C125 | `BossRocky_Dispatch` | the table one entry on | 6000 rounds |
| C126 | `BossRocky_Enter` | Sample 8's +0xF8 | 6000 rounds |
| C127 | `BossRocky_Enter` | state 1 | 5972 rounds |
| C128 | `BossRocky_Hook` | the word handed on masked | 2959 rounds |
| C128b | `BossRocky_Hook` | Sample 8's hook table (all BareRet as Rocky's: equivalent?) | 6000 rounds - by the harness only: Sample 8's table is not swapped in the `k15` run, so its real `BareRet` runs where Rocky's recorder stood; in the game the two tables hold the same three `BareRet`s, so the mutant is **equivalent** (C128 is its near variant) |
| C129 | `BossSample8_Dispatch` | the table one entry on | 6000 rounds |
| C130 | `BossSample8_Enter` | bit 2 | 4519 rounds |
| C131 | `BossSample8_Enter` | Rocky's hook | 6000 rounds |
| C132 | `BossSample8_Hook` | the word handed on masked | 2959 rounds |
| C133 | `Boss14_Setup` | exit hook BossHook_ExitActor0Bit40 | 6000 rounds |
| C134 | `Boss14_End` | scene 0xD | 1493 rounds |
| C135 | `Boss14_End` | bit 0 for the win (shared) | 3535 rounds |
| C136 | `Boss14_End` | the scene stored after the call (shared) | 1517 rounds |
| C137 | `Boss46_Setup` | the event hook BareRet | 6000 rounds |
| C138 | `BossPooch_Dispatch` | the table one entry on | 6000 rounds |
| C139 | `BossPooch_Enter` | the two byte tables swapped | 6000 rounds |
| C140 | `BossPooch_Enter` | Sprite_ScriptTickOnce | 6000 rounds |
| C141 | `BossPooch_Hook` | the word handed on masked | 2959 rounds |
| C142 | `Boss15_Setup` | exit hook BossHook_ExitClearActor0 | 6000 rounds |
| C143 | `Boss15_End` | scene 0x4C | 1463 rounds |
| C144 | `Boss15_Exit` | tag 0 | 6000 rounds |

## 7. Latent defects (Capcom's, kept)

- **Every dispatcher and hook table indexes unchecked**: a state byte past
  12 (6, 4, 3 for the second-level tables) or a hook word whose low byte is
  past 2 jumps through the dword after the table. Ours aborts.
- **The exit hooks write through `BossActor_Find`'s answer untested**
  (`Boss11_Exit`, `Boss16_Exit`, three actors): a scene without the tagged
  actor faults near address 0. Each is preceded by the same tag's
  `BossActor_ClearBit40`, which faults (the original) or aborts (ours) first,
  so the second null write is unreachable; ours aborts there too.
- **Fight 16's code names enemy 2 by address.** `Boss16_Event` and
  `Boss16_End` set `Sprite_Current` (and `0x939AD8`) to the object
  `0x93BBB0` and pose it as Nina's, without checking the kind at that slot;
  the formation (row 7 of `BOSS013`'s area) decides whether that holds. Not
  read.
- **`BossAmalgam_Enter` drops `0x455290`'s "no slot free" answer** (al 0xFF):
  with all eight field slots in use the script is simply not started
  (`BossAmalgam_DeathEnd`'s release then finds nothing to free).
- **`BossAmalgam_DeathMelt` ends on `+0x20 == 0x56` exactly**, stepping by 2
  from `DeathStart`'s 0: an odd row (never stored by the code) would step
  past 0x56 and melt until the dword wraps.
- **`BossNina_HookAct` decrements `0x904AE2` without a floor** and
  `BossSunder_HookAct`'s 0x2A line on bit 5 sets no bit, so it opens again at
  every action pick while bit 5 stays set; what either does in play is not
  measured.
- **The hit hooks floor HP at 1 in every fight** (Balio's and Sunder's,
  `BossBalio_HookHit` / `BossSunder_HookHit`): neither kind's HP reaches 0
  through a hit; how their fights end is the end hook's win bit, which was
  not traced to its setter.

## 8. What nothing reached

Nothing of the 53 went unreached: every branch the controls planted in was
reached, and every entry of the seven kinds' tables was called (the
coverage lines). What only a fight would show: the scenes the end hooks
pick (`0x903848`), the lines and the banner, Amalgam's melt on screen,
whether enemy 2 is Nina in fight 16, and B46's fight.

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 53 extents of the
table above - 52 new lines; `00439930 A3` was already there, and
`004399E0 127` fixes the host line `004399E0 16F0` (with `004394A0 481`,
which BH's `004394A0 20F` and these lines now cover).
