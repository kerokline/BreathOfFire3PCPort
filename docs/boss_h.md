# Group BH: the boss band's 20 shared helpers, the spawn helpers, and the band's tables

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave one, stage A. 26 functions ours: the 20 helpers three or more boss
units share (`src/game/boss_h.cpp`, shadow `boss_h`) and the six spawn
helpers every set-up calls (`src/game/boss_spawn.cpp`, shadow `boss_spawn`),
each read to its last instruction with capstone and fuzzed through the boss
harness BH built ([`boss_harness.md`](boss_harness.md)), 0 mismatches;
88 controls planted, 86 refused, 2 equivalent with their near variants refused (section 4). Fuzz only: no recorded route reaches a boss fight (the
combat route is an ordinary encounter). Also named: `Boss_SetupTable`,
`BossKind_Table`, the kind-3 dispatcher `BattleBossFx_Dispatch`, the three
hooks, and the eight tables round seven called `EnemyOp_StepsB..F` /
`EnemyOp_ActSubsB..E`, re-attributed to their kinds (section 7).

Enemy and fight names below are `tools/boss_rows.py --disc`'s (the US disc's
area records), not memory of the game.

## 1. The 20 helpers

`tools/boss_rows.py --unit H` (2026-09-28): the functions three or more units
reach, 1,598 bytes. Every one is in the tool's group column as BH
(`analysis/boss_funcs.tsv`); none was found inside another's extent or
missing from the list; the tool's extents were right for all 20.

| Address | Name | Bytes | Reached by (units) | What it does |
|---|---|--:|---|---|
| `0x437CA0` | `BossOp_ScriptTick` | 5 | K03, K06, K07, K40, K08..K11 | `jmp Sprite_ScriptTick` - a state entry; its `al` the answer |
| `0x437CC0` | `BareRet` | 1 | 81 units; 455 raw references engine-wide | `ret`: `Boss_SetupTable[0]`, every empty hook, the effect dispatchers' slot 0, table fillers in battle, field and scenario code |
| `0x438EB0` | `BossTorast_ActDispatch` | 0x12 | K08..K11 | entry 6 of kinds 8..11's `+1` tables: `jmp [BossTorast_ActSubs + 4 * +2]` |
| `0x438ED0` | `BossTorast_DeathDispatch` | 0x12 | (through the above) | `BossTorast_ActSubs` 4: `jmp [BossTorast_DeathSubs + 4 * +3]` |
| `0x438EF0` | `BossTorast_DeathFxDispatch` | 0x12 | | `BossTorast_DeathSubs` 0: `jmp [BossTorast_DeathFxSteps + 4 * +4]` |
| `0x438F10` | `BossTorast_DeathFxStart` | 0x2A | | step 0: `+9` and `+0xA` = 0x40, `Sound_PlayEffect(0x601)`, `+4` up |
| `0x438F40` | `BossTorast_DeathFxFlash` | 0x86 | | step 1: at `+9 == +0xA` a screen tile in the kind's colour (`BattleWin_DrawTileRgb(0, 0, 0xB, colour, 1)`) and `+0xA` halved, else `+9` down; `+4` up at `+9 == 0` - flashes at 0x40, 0x20, 0x10 ... frames |
| `0x438FD0` | `BossTorast_DeathFxRingGrow` | 0x55 | | step 2: the ring at radius `+9`; `+9` up by 4 to 0x30, then `+0x48 = 2`, the dwords `+0x40` / `+0x44` = 0x10000, `Sound_PlayEffect(0x602)`, `+4` up |
| `0x439030` | `BossTorast_DeathFxRingShrink` | 0xAE | | step 3: the ring; `+9` down by 4 and `+0x40` / `+0x44` down by 0x2000 (not below 0); at 0: `+0x48 = 0`, `Sprite_SetAnimationBank(0x83)`, `Sprite_SetAnimation(0)`, `Battle_EnemyDefeated`, states `+1..+4` = 6, 4, +1, 0 |
| `0x4394A0` | `BossTorast_DrawRing` | 0x20F | | 64 Gouraud triangles round `Sprite_Current (+0x2E, +0x30)`: the centre white, the rim in the kind's colour, semi-transparent (section 6: two laps) |
| `0x43A4A0` | `BossHook_ExitClearActor0` | 9 | B14, B28, B29 | exit hook: `BossActor_Clear(0)` |
| `0x43A720` | `BossOp_EnterTick` | 0x13 | K21, K22, K27 | state 1: `Sprite_ScriptTickOnce`; answered, `+1 = 2` |
| `0x43A750` | `BossHook_RetargetMember0` | 0x11 | K21, K22, K23 | `+0xF4` hook entry 0 (the action pick's call): the leader's target `+0x124` 4 makes the battle's target `0x904B44` 5 |
| `0x43B0D0` | `BossMap_SetCorners` | 0x57 | K26 (and `0x43B180`) | a 2 x 2 block of `AreaMap_Corners` set to one dword (`BossMap_CornerCells` [cell], `BossMap_CornerValues` [value]) |
| `0x43B130` | `Boss_SetByLeaderId` | 0x50 | B18, B19, B20 | the byte `0x92BF18` = 0x2C / 0x2D / 0x2E / 0x2F by the leader's character id 0 / 1 / 5 / 6 (a jump table of 7) |
| `0x43B180` | `BossMap_UpdateFromEnemies` | 0xFE | B18, B19, B20 | enemies 1 and 2: by `+0x93` bits 0x40 / 0x20, a chapter flag (0x34 / 0x33) set or cleared, an animation (3 / 2), a map block (`BossMap_SetCorners`); the ground words of enemy 0 and the leader; `0x939B1C` |
| `0x43B550` | `BossOp_Death` | 0x48 | K02, K03, K06, K07, K18, K27 | `+2` entry 4 (the death): `Sprite_ScriptTickOnce`, `Battle_EnemyDefeated`, `+0x110 |= 0x1000`, `+0 &= 0xBF`, states 3, 0, 0 |
| `0x43C9F0` | `BareRetZero` | 3 | 39 set-ups' event hooks; field tables | `xor al, al; ret` |
| `0x43EB60` | `BossHook_EndPickWay` | 0x1A | B39..B48 (chapter 15's) | end hook: the win (`0x904AE8` bit 1) - chapter step `0x8034E5 = 5`, `0x446DE0`; else `0x446E00` |
| `0x440820` | `BossHook_ExitActor0Bit40` | 9 | B03, B17, B55 | exit hook: `BossActor_ClearBit40(0)` |

`BossTorast_*` is kinds 8..11's shared death (Torast, Kassen, Galtel,
Doksen: area 27, set-ups 8..11, one kind each): the four kinds' `+1`
tables differ only in entry 0 (each kind's entrance) and share entries
1..11, so their death chain below entry 6 is one body of code, named for the
first kind. The colours are the one thing per kind: `BossTorast_Colours`
(three bytes a kind, indexed by the enemy's kind byte).

The two fillers are named for what they are rather than for a boss:
`0x437CC0` and `0x43C9F0` sit in the boss band, but engine code calls and
stores both (section 9).

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original reads past a table (the three dispatchers, the two
indexes of `BossMap_SetCorners`) or writes through a null pointer (the spawn
writers, section 5), ours aborts with a `Fatal` naming the function (the
owner's rule, round9 doc section 6); nothing reaches it. `BareRetZero`
clears `al` where the original clears only `al` and ours clears eax: every
caller reads `al` (`ret_mask 0xFF`, as every function answering in `al`).

## 3. The fuzz

`BOF3X_SHADOW=boss_h`, `src/game/boss_h_fuzz.cpp`, three `Run`s
(`BOF3X_BH_RUN=torast|ops|hooks` runs one):

| Run | Clones | Shape and how driven | Rounds | Result (this worktree) |
|---|---|---|--:|---|
| `torast`, fight 8 | `BossTorast_ActDispatch` four times (through each of kinds 8..11's dispatchers `0x438E50`, `0x4390F0`, `0x439160`, `0x4391D0` at state 6), `_DeathDispatch` (through `0x438EB0` at `+2 = 4`), `_DeathFxDispatch` (through `0x438ED0` at `+3 = 0`), `BossOp_ScriptTick` (through `0x438ED0` at `+3 = 1`), the four steps (through `0x438EF0` at `+4 = 0..3`), `_DrawRing` (`kCallee`) | the dispatchers `kDispatch` with their state byte drawn below 6 / 2 / 4; the three shared tables `DataTable`s | 6,000 each | 72,000 rounds, **2,003,623** calls, 0 mismatches |
| `ops`, fight 18 | `BossOp_EnterTick` (through kind 21's `0x43A660` at state 1), `BossOp_Death` (through kind 6's `0x437A70` at `+2 = 4`), `BossHook_RetargetMember0` (through kind 21's hook table `0x43A740`, word 0), `BareRet` (through kind 8's hook table `0x4390E0`, word 0), `BossMap_SetCorners`, `Boss_SetByLeaderId`, `BossMap_UpdateFromEnemies` (`kCallee`) | the map block `0x8CB580..+0x4400` and `0x92BF18` as regions | 6,000 each | 42,000 rounds, 50,952 calls, 0 mismatches |
| `hooks` | `BareRetZero` (`kEvent`), `BossHook_EndPickWay` (`kEnd`), the two exit hooks (`kExit`) | | 4,000 each | 16,000 rounds, 12,000 calls, 0 mismatches |

**Seeds.** The chain: the current enemy's kind 8..11 two times in three;
the Flash's count and interval equal half the time, from the halving
sequence and its ends; the ring's `+9` at 0x30, 0x2C, 0x34, 0, 4 and the
wrap 0xB0; the shrink's `+0x40` / `+0x44` at 0, 1, 0x1FFF, 0x2000, 0x2001,
0x10000 and the signed extremes; the ring's radius 0..0x30 in fours with
garbage above the byte. The ops: the leader's target 4 and neighbours; the
leader's id 0..7 and any; the enemies' `+0x93` with each of 0x40 / 0x20 and
both; the grid width small, the area's range, or any (the region covers any
width). The hooks: `0x904AE8` 0..3 and bytes with bit 1 set or not.
`DisturbOps` moves what the map helpers read again after a call: the two
status bytes, the grid width, the leader's flags and target, and the flag
bits pointer `0x929ED0`.

**One harness artefact met** ([`boss_harness.md`](boss_harness.md) section
6): `BossTorast_DeathFxFlash` reads its colour at `0x64CA6C + 3 * kind`, and
for kinds 46 and 47 that is its own via cell `0x64CAF8`, which holds the copy
on one pass and ours on the other: 8 mismatched rounds of 6,000 on the first
run, all with the colour argument the first difference. The seed keeps those
two kinds out for that clone; in the game the cell holds `0x438F40` on both
sides and the read is the same.

Coverage (the originals' calls, `torast`): `BossTorast_DrawRing` 12,000,
`Battle_EnemyDefeated` 646, `BattleWin_DrawTileRgb` 3,120,
`Sound_PlayEffect` 6,591, `Sprite_SetAnimation` / `Sprite_SetAnimationBank`
646, `Sprite_ScriptTick` 6,000, `Gfx_CommitPrim` 390,000, `Gpu_SetPolyG3` /
`Gpu_SetSemiTrans` / `Math_Sin` / `Math_Cos` 384,000, `Gpu_SetDrawMode` /
`Gpu_GetTPage` 6,000, and every entry of the three tables (`phase 0x436700`
7,871 ... `phase 0x439030` 1,505). `ops`: `BossMap_SetCorners` 7,072,
`Battle_EnemyDefeated` 6,000, `AreaMap_Elevation` 12,000, `Flags_Set` 4,011,
`Flags_Clear` 3,061, `Sprite_EnsureAnimation` 7,072,
`Sprite_ScriptTickOnce` 12,000. `hooks`: `0x446DE0` 2,006, `0x446E00`
1,994, `BossActor_ClearBit40` 4,000, `BossActor_Clear` 4,000. Counts are this
worktree's (they move with the build directory; judge by 0 mismatches).

## 4. Controls

`python controls.py` (the group's scratch script): each control one textual change to ours, anchored on a string that occurs once, then rebuild, run (`BOF3X_SHADOW=boss_h` with `BOF3X_BH_RUN` its run, or `boss_spawn`), restore, rebuild. Run twice; the table is the second run, on the final seeds. **88 planted, 86 refused**: 83 by a count (the rounds column is the planted function's mismatched rounds), three by a `Fatal` (H36, H37, H37b: a dispatcher reading the wrong state byte finds it past its table in the rounds where the seed leaves that byte random, and aborts - loud, but a Fatal proves less than a count); **two equivalent**, each with a near variant refused: H6 (`BossActor_ClearBit40(0x100)`: the helper reads the tag's low byte, so 0x100 is 0 - no input can tell them apart; H6b refused) and H17 (`BossMap_SetCorners` with i and j swapped writes the same four cells with the same dword - the set is symmetric; H17b, the diagonal only, refused). The thinnest refusals are the re-read plants (H11, H14, H40, H44, H58: 83..357 rounds, the disturbance moving `Sprite_Current` or `0x939AD8` in a call) and S6 (the "none" answer, 1,323 rounds once the seed took the tag away in a third of the rounds: 12 before).

| # | Function | Plant | Refused |
|---|---|---|---|
| S1 | `EnemyData_FindByTag` | the tag masked to 7 bits | 880 rounds |
| S2 | `EnemyData_FindByTag` | seven records | 110 rounds |
| S3 | `EnemyData_FindByTag` | none answers 0xFE | 1786 rounds |
| S4 | `BossActor_Find` | the next object | 2636 rounds |
| S5 | `BossActor_Find` | the tag at +0x9D (Find and Index) | 2671 rounds |
| S6 | `BossActor_Index` | none answers 0 | 1323 rounds |
| S7 | `BossActor_ClearBit40` | bit 0x80 cleared too | 2002 rounds |
| S8 | `BossActor_CopyFrom` | mode 2 copies the pose | 412 rounds |
| S9 | `BossActor_CopyFrom` | the place without +0x3C | 862 rounds |
| S10 | `BossActor_CopyFrom` | bit 0x40 of +7 | 725 rounds |
| S11 | `BossActor_CopyFrom` | +0x5A from +0x58 | 965 rounds |
| S12 | `BossActor_CopyFrom` | the whole word as the mode | 887 rounds |
| S13 | `BossActor_Clear` | four bytes | 3986 rounds |
| H1 | `BareRetZero` | al 0xFF | 4000 rounds |
| H2 | `BossHook_EndPickWay` | bit 0 for the win | 2434 rounds |
| H3 | `BossHook_EndPickWay` | step 6 | 1906 rounds |
| H4 | `BossHook_EndPickWay` | the other way out for the win | 1941 rounds |
| H5 | `BossHook_ExitClearActor0` | tag 1 | 4000 rounds |
| H6 | `BossHook_ExitActor0Bit40` | tag 0x100 (the word logged masked: equivalent?) | NOT REFUSED |
| H6b | `BossHook_ExitActor0Bit40` | the other spawn helper | 4000 rounds |
| H7 | `BareRet` | a store | 5981 rounds |
| H8 | `BossHook_RetargetMember0` | 4 and above | 3054 rounds |
| H9 | `BossHook_RetargetMember0` | target 4 | 1339 rounds |
| H10 | `BossOp_EnterTick` | state 3 | 4086 rounds |
| H11 | `BossOp_EnterTick` | Sprite_Current read before the call | 140 rounds |
| H12 | `BossOp_EnterTick` | Sprite_ScriptTick | 6000 rounds |
| H13 | `BossOp_Death` | bit 8 | 4525 rounds |
| H14 | `BossOp_Death` | 0x939AD8 read before the calls | 357 rounds |
| H15 | `BossOp_Death` | +3 = 1 | 6000 rounds |
| H16 | `BossOp_Death` | no tick | 6000 rounds |
| H17 | `BossMap_SetCorners` | i and j swapped | NOT REFUSED |
| H17b | `BossMap_SetCorners` | the diagonal only | 5535 rounds |
| H18 | `BossMap_SetCorners` | the value by the cell | 3949 rounds |
| H19 | `BossMap_SetCorners` | the height byte | 5987 rounds |
| H20 | `BossMap_SetCorners` | a row down | 5535 rounds |
| H21 | `Boss_SetByLeaderId` | id 5 as 6 | 530 rounds |
| H22 | `Boss_SetByLeaderId` | id 2 as 1 | 510 rounds |
| H23 | `BossMap_UpdateFromEnemies` | the status byte +0x92 | 5206 rounds |
| H24 | `BossMap_UpdateFromEnemies` | flag 0x35 | 3474 rounds |
| H25 | `BossMap_UpdateFromEnemies` | the animations swapped | 4973 rounds |
| H26 | `BossMap_UpdateFromEnemies` | enemy 2 the cell 1 | 3538 rounds |
| H27 | `BossMap_UpdateFromEnemies` | enemy 2 by enemy 1's status | 3922 rounds |
| H28 | `BossMap_UpdateFromEnemies` | the flag bits pointer read once, before the loop | 38 rounds |
| H29 | `BossMap_UpdateFromEnemies` | bit 2 | 2926 rounds |
| H30 | `BossMap_UpdateFromEnemies` | the x for the y | 2970 rounds |
| H31 | `BossMap_UpdateFromEnemies` | a dword stored | 6000 rounds |
| H32 | `BossMap_UpdateFromEnemies` | x and z swapped | 6000 rounds |
| H33 | `BossMap_UpdateFromEnemies` | bit 0x80 for 0x40 | 4506 rounds |
| H34 | `BossOp_ScriptTick` | Sprite_ScriptTickOnce | 6000 rounds |
| H35 | `BossOp_ScriptTick` | al 1 | 6000 rounds |
| H36 | `BossTorast_ActDispatch` | by +3 | a Fatal: BossTorast_ActDispatch: state byte +3 is 18, past the 6 entries of 0x64CAD4 |
| H37 | `BossTorast_DeathDispatch` | by +4 | a Fatal: BossTorast_DeathDispatch: state byte +4 is 119, past the 2 entries of 0x64CAEC |
| H37b | `BossTorast_DeathDispatch` | by +2 (the via planted 4: a Fatal) | a Fatal: BossTorast_DeathDispatch: state byte +2 is 4, past the 2 entries of 0x64CAEC |
| H38 | `BossTorast_DeathFxDispatch` | by +3 | 4495 rounds |
| H39 | `BossTorast_DeathFxStart` | interval 0x41 | 5971 rounds |
| H40 | `BossTorast_DeathFxStart` | Sprite_Current read before the call | 237 rounds |
| H41 | `BossTorast_DeathFxStart` | sound 0x611 | 6000 rounds |
| H42 | `BossTorast_DeathFxFlash` | red shifted one more | 2796 rounds |
| H43 | `BossTorast_DeathFxFlash` | blue shifted one less | 2779 rounds |
| H44 | `BossTorast_DeathFxFlash` | the interval on the Sprite_Current of before the call | 111 rounds |
| H45 | `BossTorast_DeathFxFlash` | down by 2 | 2903 rounds |
| H46 | `BossTorast_DeathFxFlash` | next step at 1 | 422 rounds |
| H47 | `BossTorast_DeathFxFlash` | size 0xC | 3097 rounds |
| H48 | `BossTorast_DeathFxFlash` | the next kind | 3065 rounds |
| H49 | `BossTorast_DeathFxRingGrow` | at 0x2C | 1088 rounds |
| H50 | `BossTorast_DeathFxRingGrow` | up by 3 | 5457 rounds |
| H51 | `BossTorast_DeathFxRingGrow` | +0x48 = 1 | 539 rounds |
| H52 | `BossTorast_DeathFxRingGrow` | +0x44 = 0x8000 | 543 rounds |
| H53 | `BossTorast_DeathFxRingGrow` | Sprite_Current read before the ring | 208 rounds |
| H54 | `BossTorast_DeathFxRingShrink` | 0 counts as above | 604 rounds |
| H55 | `BossTorast_DeathFxRingShrink` | down by 0x1000 | 4369 rounds |
| H56 | `BossTorast_DeathFxRingShrink` | state 5 | 673 rounds |
| H57 | `BossTorast_DeathFxRingShrink` | +3 = 1 | 670 rounds |
| H58 | `BossTorast_DeathFxRingShrink` | Sprite_Current not read after the calls | 83 rounds |
| H59 | `BossTorast_DeathFxRingShrink` | bank 0x84 | 673 rounds |
| H60 | `BossTorast_DeathFxRingShrink` | down by 3 | 5327 rounds |
| H61 | `BossTorast_DrawRing` | sar 11 | 5687 rounds |
| H62 | `BossTorast_DrawRing` | the mask 0x3F | 5687 rounds |
| H63 | `BossTorast_DrawRing` | angle step 0x40 | 6000 rounds |
| H64 | `BossTorast_DrawRing` | red 0xFE at the centre | 6000 rounds |
| H65 | `BossTorast_DrawRing` | corner 2 red from green | 5999 rounds |
| H66 | `BossTorast_DrawRing` | x 0x380 | 6000 rounds |
| H67 | `BossTorast_DrawRing` | size 0x30 | 6000 rounds |
| H68 | `BossTorast_DrawRing` | the packet pointer read once | 6000 rounds |
| H69 | `BossTorast_DrawRing` | opaque | 6000 rounds |
| H70 | `BossTorast_DrawRing` | point i's x for its y | 5687 rounds |
| H71 | `BossTorast_DrawRing` | the tpage masked to 12 bits | 5602 rounds |
| H72 | `BossTorast_DrawRing` | the centre x unsigned | 5776 rounds |

## 5. The spawn helpers (`boss_spawn`)

The plan (§1.3, §8 step 3) left the six engine functions every set-up calls
with a tag to BH: take them as a small engine group, or stand them in.
**Taken**, as module `boss_spawn`: they are small (0x19E bytes), read in an
hour, called 57 times by the band, and every stage-B group needs their
behaviour pinned (a stand-in answering a pointer has to know what the
pointer is) - with them ours, the standard set lists them by name with
typed stand-ins, and stage B calls them by name.

A field actor is one of the 30 field objects (`Sprite_Objects`
`0x7DEE80`, stride 0xA4) whose type byte `+6` is 7; its tag is `+0x9E`. The
battle keeps the field's objects, and a set-up's hooks find the actors of
the scene by tag (Boss01's exit hook `0x437E10` copies enemies 0 and 1's
places and poses onto actors 6 and 7).

| Address | Name | Bytes | Calls from the band | What it does |
|---|---|--:|--:|---|
| `0x4948E0` | `EnemyData_FindByTag` | 0x3A | 2 units | the index 0..7 of the first of the area's enemy data records (`0x8C55C8 + i * 0x8C`) whose `+0xC` is the tag, or 0xFF (`al`; the caller's upper eax kept) |
| `0x494920` | `BossActor_Find` | 0x5D | 15 | the first field actor with the tag, or null |
| `0x494980` | `BossActor_Index` | 0x42 | 2 | its index 0..29, or 0xFF (`al`) |
| `0x4949D0` | `BossActor_ClearBit40` | 0x15 | 22 | the actor's `+0 &= 0xBF` |
| `0x4949F0` | `BossActor_CopyFrom` | 0x66 | 9 | by `what` - 0: `from`'s pose (`+0x4B`, `+0x50`, `+0x54`, `+0x58`, `+0x5A`) and `+7 |= 0x20`; 1: `from`'s place (`+0x34`, `+0x38`, `+0x3C`); else nothing |
| `0x494A60` | `BossActor_Clear` | 0x1E | 7 | the actor's bytes `+0..+4` zeroed |

`BOF3X_SHADOW=boss_spawn`, `src/game/boss_spawn_fuzz.cpp`: one `Run`, six
`kCallee` clones, 4,000 rounds each; the seed picks a tag (0, 1, 2, 6, 7,
0x10, 0xFF or any) and plants it in some of the objects with type 7, in some
with another type, and a neighbour tag in others (and in the enemy records'
`+0xC` for `EnemyData_FindByTag`); `args` hands the tag with garbage above
the byte, `from` an enemy, an object or a party record, `what` 0, 1, 2,
0x100, 0x101, 0xFF or any. 24,000 rounds, 12,000 calls, 0 mismatches
(coverage: `BossActor_Find` 12,000 - the three writers' call). The three
writers' `BossActor_Find` is the standard stand-in (a field object 0..3,
never null), so their null fault (section 6) is not reached.

## 6. Latent defects (Capcom's, kept)

- **The spawn writers write through a null pointer.** `BossActor_ClearBit40`,
  `BossActor_CopyFrom` (for `what` 0 and 1) and `BossActor_Clear` use
  `BossActor_Find`'s answer without a test; with no field actor carrying the
  tag the original writes near address 0 and faults. 22, 9 and 7 units of
  the band call them, with constant tags where read (the exit hooks' 0,
  Boss01's 6 and 7); whether a scene can lack its actor is the area's data,
  not read. Ours aborts with a
  message naming the function and the tag. The one caller outside the band,
  `0x494570` (`0x494500`'s encounter set-up), tests the answer.
- **The dispatchers and `BossMap_SetCorners` index unchecked**: a state byte
  past 6 / 2 / 4, or a cell past 1 / a value past 2, jumps or reads through
  what follows. Ours aborts.
- **`BossTorast_DrawRing` draws its ring twice.** 64 points at `i * 0x80`
  are two laps of the 4096-step circle, and the rim's second corner is point
  `(i + 1) & 0x1F`, so triangles 32..63 are triangles 0..31 again, each
  semi-transparent: the ring is blended twice. Kept (it is the look the
  original gives); a lap of 32 would halve the cost.
- **The kind's colour is read by the kind unchecked**: a kind outside 8..11
  reading `BossTorast_Colours` gets neighbouring `.data` (other tables'
  bytes) - only kinds 8..11 reach these steps, through their own tables.

## 7. Named data, and the tables re-attributed

| Address | Name | Count | What |
|---|---|--:|---|
| `0x656954` | `Boss_SetupTable` | 56 | the set-ups by fight id (`Battle_InitBossEncounter`'s `jmp`); entry 0 `BareRet` |
| `0x64B088` | `BossKind_Table` | 63 | the kinds' dispatchers by the enemy's kind `+0x100` (`BattleEnemy_RunAll` in an event battle); `BattleEnemy_States` one entry on - the same dwords, named twice on purpose (the plan's step 2) |
| `0x904B64` | `BattleHook_End` | | the end hook ([`boss_harness.md`](boss_harness.md) section 1: the plan's "per-frame script" corrected) |
| `0x904B68` | `BattleHook_Exit` | | the exit hook |
| `0x904B6C` | `BattleHook_Event` | | the event hook, with a phase code |
| `0x64CA84` | `BossTorast_Colours` | 12 | kinds 8..11's (r, g, b) |
| `0x64CAD4` | `BossTorast_ActSubs` | 6 | `EnemyOp_ActSubs` with kinds 8..11's death at 4 |
| `0x64CAEC` | `BossTorast_DeathSubs` | 2 | `BossTorast_DeathFxDispatch`, `BossOp_ScriptTick` |
| `0x64CAF4` | `BossTorast_DeathFxSteps` | 4 | the four steps |
| `0x64D090` | `BossMap_CornerCells` | 4 bytes | two (x, y) corners |
| `0x64D094` | `BossMap_CornerValues` | 3 | three corner dwords |

and the function `BattleBossFx_Dispatch` `0x4357D0` (named, not taken:
`BattleTask_RunAll`'s kind 3, an eight-entry stack table whose slots 2..7
are the band's effect tasks F2..F7).

**Round seven's `EnemyOp_StepsB..F` / `EnemyOp_ActSubsB..E`** were read as
the generic enemy state's; each is the table of one kind's own dispatcher,
the dispatcher an entry of `BossKind_Table` (read 2026-09-28: the `jmp` of
each, and its `BossKind_Table` index). Renamed, the old names kept in each
evidence string:

| Address | Old | New | The kind (the tool's name) | Its dispatcher |
|---|---|---|---|---|
| `0x64C7B0` | `EnemyOp_StepsB` | `BossGary_Steps` | 6 (Gary) | `BossKind_Table[6]` `0x437A10`, by `+1` |
| `0x64C7E0` | `EnemyOp_ActSubsB` | `BossGary_ActSubs` | 6 | its state 6 `0x437A70`, by `+2` |
| `0x64C810` | `EnemyOp_StepsC` | `BossMogu_Steps` | 7 (Mogu) | `[7]` `0x437B70` |
| `0x64C840` | `EnemyOp_ActSubsC` | `BossMogu_ActSubs` | 7 | `0x437BD0` |
| `0x64C890` | `EnemyOp_StepsD` | `BossNue_Steps` | 1 (Nue, area 23) | `[1]` `0x437EE0` |
| `0x64C8D4` | `EnemyOp_StepsE` | `BossNue2_Steps` | 2 (Nue, area 22) | `[2]` `0x438070` |
| `0x64C904` | `EnemyOp_ActSubsE` | `BossNue2_ActSubs` | 2 | `0x4380D0` |
| `0x64C928` | `EnemyOp_StepsF` | `BossSample1_Steps` | 46 (Sample 1) | `[46]` `0x438100` |

(There was no `EnemyOp_ActSubsD`.) Kind 1's other tables the plan names
(`0x64C8C0`, `0x64C8C8`) and the kinds' `+0xF8` / `+0xFC` byte tables are
BSA's to name with the kinds. Kind 8's state 0 shows what the "flag header"
before a kind's pointer table is: its `+0xFC` and `+0xF8` byte tables
(`0x438E70` stores `0x64CA90` and `0x64CA9C`, the bytes before `0x64CAA4`).

## 8. What nothing reached

Nothing of the 26 went unreached by the fuzz: every clone's rounds reached
every branch the controls planted in (section 4). What only a fight would
show: the ring's look, the flash's timing, the end hook's hand-back to the
chapter (`0x8034E5 = 5`), and whether a scene can lack its tagged actor.

## 9. Calls across groups

**Out of BH, to code nobody owns** (raw, `boss_h_callees.h`): `0x446DE0`
and `0x446E00` (the end phase's steps 1 and 2; the harness's standard set
lists them, with `0x446E20`). Everything else BH calls is ours.

**Into BH from outside the group** (for the round's rebinding pass - our
code naming these by raw address):

- `0x437CC0` (`BareRet`): the `kRetOnly` / `kBareRet` / `kNop` constants of
  `battle_draw_callees.h`, `battle_misc_callees.h`,
  `battle_win_states_callees.h`, `d3d_draw_callees.h`, `d3d_list_callees.h`,
  `field_misc_callees.h`, `glyph_draw_callees.h`, `msgbox_callees.h`,
  `scena_sc6_callees.h`, `sprt_draw_callees.h`, `window_kinds_callees.h`;
  `magic_s14.cpp`'s `kNothing`; `battle_fx_tasks.cpp`'s two stack tables;
  the fuzz sites of `battle_draw_fuzz.cpp`, `field_misc_fuzz.cpp`,
  `msgbox_fuzz.cpp`, `menu_draw_helpers_fuzz.cpp`,
  `battle_win_states_fuzz.cpp`, `battle_fx_tasks_fuzz.cpp`,
  `magic_s14_fuzz.cpp`. Capcom's code calls it directly (rel32) from
  `0x45D64A`.., `0x484776`, `0x4848D7`, `0x59EF2F`..`0x5A3119` and stores it
  in hundreds of `.data` slots: all reach ours through the entry's `jmp`.
- `0x4357D0` (`BattleBossFx_Dispatch`, named, not taken): `battle_flow.cpp`'s
  task table and `battle_flow_callees.h`, `battle_flow_fuzz.cpp`.
- `0x43C9F0` (`BareRetZero`): comments only (`field_modes.cpp`,
  `scena_sc15.cpp`); Capcom's scenario tables hold it (`0x6619F0`..).
- The spawn helpers: none of ours; Capcom's `0x494570` calls
  `BossActor_Find`.

**Stage B calls BH by name** once BH merges (the standard set already does).

## 10. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 20 helpers' extents
as the table in section 1 gives them - 19 new lines; `0043B0D0 57` was
already there, and the smaller extents fix three host lines
(`004394A0 481`, `0043B130 18BF`, `0043C9F0 189F`). The six spawn helpers'
lines were already exact.
