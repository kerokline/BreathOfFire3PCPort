# Group BSG: fights 29, 31, 32, 33, kinds 34..38 and 40, and the effect task F6

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave two. 53 functions of `0x43CDE0..0x43E535` ours (`src/game/boss_sg.cpp`,
shadow `boss_sg`), each read to its last instruction with capstone and fuzzed
through the boss harness ([`boss_harness.md`](boss_harness.md)), eleven
`Run`s, 0 mismatches; CONTROLS_SUMMARY (section 4). The first user of the
harness's `kTask` shape (F6). Fuzz only: no recorded route reaches a boss
fight.

Enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's area
records), not memory of the game. Which fight a set-up is comes from the
tool's rows; nothing here says what happens in a fight.

## 1. The units and their functions

`tools/boss_rows.py --unit <U> --clones` for the eleven units (2026-09-28):
K34 3, B29 2, K35 3, K36 3, K37 5, B31 3, K38 5, B32 3, B33 2, F6 12, K40 12 -
53 functions, none found inside another's extent, none missing, every extent
the tool gave the code's (`0x43E290`'s 0x2A5 included). No function is shared
with another unit. **A note on the tool's group column:** after wave one
merged, `analysis/boss_funcs.tsv` was regenerated over what is left and its
group column now reads `BSB` for these 53 (the cut renumbered over the
remainder); the brief's table and this doc call them BSG, and the set is the
same.

The sibling's per-image counts (`analysis/overlay_captures_all.json`,
`static_discovery_entry_pcs`) as the check on size: BOSS029 6 (K34 + B29 =
5 here), BOSS031 8 (K37 + B31 = 8), BOSS032 8 (K38 + B32 = 8), BOSS033 23
(BSA's K39 10 + B33 2 + F6 12 = 24 - so F6 is BOSS033's, Weretigr's image,
which its code confirms: section 1.3).

### 1.1 Set-ups (`Boss_SetupTable[id]`, `kSetup`) and their hooks

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43CE50` | `Boss29_Setup` | 0x1F | id 29 (BOSS029, area 175 row 7, kind 34 Dolphin): end `Boss29_End`, exit `BossHook_ExitClearActor0` (BH's), event `BareRetZero` |
| `0x43CE70` | `Boss29_End` | 0x22 | won (`0x904AE8` bit 1): `0x8034E5` = 0x1C, `0x446DE0` (step 1) **called**, then `Music_Track` = 0x62; else a tail jump to `0x446E00` (step 2) |
| `0x43D0C0` | `Boss31_Setup` | 0x1F | id 31 (BOSS031, area 85 row 7, kind 37 Garr): end `Boss31_End`, exit `Boss31_Exit`, event `BareRetZero` |
| `0x43D0E0` | `Boss31_End` | 0x21 | won: `0x8034E5` = 0x14, step 1; else `0x904AE5` &= 0xBF (the battle's music not kept), step 2 |
| `0x43D110` | `Boss31_Exit` | 0x2C | `BossActor_ClearBit40(0)`; `BossActor_Find(0)` stored in `Field_ActiveMember` **and** `Sprite_Current`; `Sprite_SetAnimation(1)`; `+0x2A` = 1 |
| `0x43D250` | `Boss32_Setup` | 0x1F | id 32 (BOSS032, area 108 row 7, kind 38 D>Zombie): end `Boss32_End`, exit `Boss32_Exit`, event `BareRetZero` |
| `0x43D270` | `Boss32_End` | 0x12A | won: each party member with `+0` bit 0 - `Battle_RemoveFromTurnOrder(+5)`, `Sprite_Current` = it, `Sprite_PoseFromSet(+8 + 0x1C` with `+0x90` bit 14, else `+ 4` - a byte`, 0x8C5D80, 0x1800)` (BSE's `Boss26_End` loop, unrolled); then `Music_Track` = 0x6B, `0x8034E5` = 0xE, `0x904AE8` \|= 8, step 1. Not won: step 2 and nothing else |
| `0x43D3A0` | `Boss32_Exit` | 0x23 | as `Boss31_Exit` with animation 0 and no `+0x2A` |
| `0x43D670` | `Boss33_Setup` | 0x1F | id 33 (BOSS033, area 35 row 7, kind 39 Weretigr - BSA's): end `Boss33_End`, exit `BareRet`, event `BareRetZero` |
| `0x43D690` | `Boss33_End` | 0x3A | `0x904AE8` & 6 (bit 1 or bit 2): `BossActor_Clear(0)`, then `Field_Kind2X` / `Field_Kind2Z` (`0x905E64` / `0x905E60`) = party member 0's `+0x34` / `+0x38` (read after the call), `0x8034E5` = 8, step 1; else step 2 |

Set-up 29's end hook is the one here that calls step 1 and returns to store
the track after it; the others tail-jump. `Boss33_End` is the only one that
takes bit 2 of `0x904AE8` for a win as well as bit 1.

### 1.2 Kinds (`BossKind_Table[kind]`) and their tables

Each kind's dispatcher is `jmp [States + 4 * Sprite_Current +1]` (12 entries:
the kind's entry at 0, then the generic enemy states, `Port_DroppedCall` at 1
and 10, the kind's own action dispatcher or `EnemyOp_ActDispatch` at 6); its
entry stores `0x939AD8`'s `+0xFC` (animation bytes), `+0xF4` (the hook) and
`+0xF8` (sound words), sets `+1` = 2 and tail-jumps to `Sprite_ScriptTick`;
its hook is `jmp [Hooks + 4 * (word & 0xFF)]` (3 entries, `BareRet` each for
all six kinds).

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43CDE0` | `BossDolphin_Dispatch` | 0x12 | kind 34 (Dolphin, area 175): `BossDolphin_States` (12) |
| `0x43CE00` | `BossDolphin_Enter` | 0x3D | `BossDolphin_Anims`, `_Hook`, `_Sounds`; `+1` = 2; tail tick (`al`) |
| `0x43CE40` | `BossDolphin_Hook` | 0x10 | `BossDolphin_Hooks` (3) |
| `0x43CEA0` | `BossGisshan_Dispatch` | 0x12 | kind 35 (Gisshan, area 103; fight 30, set-up BSE's) |
| `0x43CEC0` | `BossGisshan_Enter` | 0x3D | its tables and hook |
| `0x43CF00` | `BossGisshan_Hook` | 0x10 | `BareRet` x3 |
| `0x43CF10` | `BossScylla_Dispatch` | 0x12 | kind 36 (the tool prints "Charyb, Scylla": one kind, two names; area 103) |
| `0x43CF30` | `BossScylla_Enter` | 0x3D | its hook; its two byte tables lie in kind 35's header (`0x64D4D4`, `0x64D4E8`) |
| `0x43CF70` | `BossScylla_Hook` | 0x10 | `BareRet` x3 |
| `0x43CFD0` | `BossGarr2_Dispatch` | 0x12 | kind 37 (Garr, area 85 - the second Garr kind; kind 27 is BSD's `BossGarr_*`) |
| `0x43CFF0` | `BossGarr2_Enter` | 0x3D | its tables and hook |
| `0x43D030` | `BossGarr2_ActDispatch` | 0x12 | state 6: `BossGarr2_ActSubs` by `+2` (6: the generic action entries, its death at 4) |
| `0x43D050` | `BossGarr2_Death` | 0x5C | `Sprite_EnsureAnimation(1)`, `+0x2A` = 1, `Sprite_ScriptTickOnce`, `Battle_EnemyDefeated`, `0x939AD8 +0x110` \|= 0x1000, `+0` &= 0xBF, `+1..+3` = 3, 0, 0 (the `Sprite_Current` of after the calls) |
| `0x43D0B0` | `BossGarr2_Hook` | 0x10 | `BareRet` x3 |
| `0x43D140` | `BossDZombie_Dispatch` | 0x12 | kind 38 (D>Zombie, area 108) |
| `0x43D160` | `BossDZombie_Enter` | 0x51 | its tables and hook, then `0x939AD8 +0x114` \|= 8 |
| `0x43D1C0` | `BossDZombie_ActDispatch` | 0x12 | `BossDZombie_ActSubs` (6) |
| `0x43D1E0` | `BossDZombie_Death` | 0x52 | as kind 37's with animation 0 and no `+0x2A` |
| `0x43D240` | `BossDZombie_Hook` | 0x10 | `BareRet` x3 |
| `0x43DBF0` | `BossMikba_Dispatch` | 0x12 | kind 40 (Mikba, area 43; fights 34 and 41, set-ups BSH's) |
| `0x43DC10` | `BossMikba_Enter` | 0x3D | its tables and hook |
| `0x43DC50` | `BossMikba_ActDispatch` | 0x12 | `BossMikba_ActSubs` (6: the death dispatcher at 4) |
| `0x43DC70` | `BossMikba_DeathDispatch` | 0x12 | `BossMikba_DeathSteps` by **`+3`** (7) |
| `0x43DC90` | `BossMikba_DeathWait` | 0x27 | step 0: `Sprite_ScriptTick` `al` not 0 - `+9` = 0x40, `+0xA` = 0x20, `+3` = 1 |
| `0x43DCC0` | `BossMikba_DeathFlash` | 0x66 | step 1: `+9` equal to `+0xA` - `Sound_PlayEffect(0x601)` once (`0x904AAD` bit 2, read again after the call and set), `BattleWin_DrawTileRgb(0, 0, 0xB, 0xFFFF, 1)`, `+0xA` halved; else `+9` down by one; `+9` at 0: `+3` up. The flashes come closer together each time: 0x20 frames, 0x10, 8, ... |
| `0x43DD30` | `BossMikba_DeathOpen` | 0xAC | step 2: `Sprite_ScriptTickOnce`; `Gfx_ClearRect(0x340, 0x100, 0xC0, 0x100)`; `+0x18` / `+0x1C` = the s16 `+0x2E` / `+0x30`, `+0x20` = 0, `+0xC` = 0xBE, `+0x10` = 0x68, the words `+0x2E` = 0x64, `+0x30` = 0x50, `+0xA` = 1, `+0x24` \|= 0x88; `Sound_PlayEffect(0x602)`; `BossMikba_DrawQuad(+9)`; `+3` up |
| `0x43DDE0` | `BossMikba_DeathSpread` | 0x7B | step 3: `+0x20` += `+0xA`; the s32 `+0xC` above 0x28 down (and on an odd `Frame_Counter` `+0x1C` up), `+0x10` above 0x28 down; `BossMikba_DrawQuad(+0x20)`; `Frame_Counter & 7`: `+0xA` up; `+0xA` at 0x40: `+3` up |
| `0x43DE60` | `BossMikba_DeathLoad` | 0x3E | step 4: `Sprite_SetAnimationBank(0x1C1)`, `+0x2A` = 1, `Sprite_SetAnimation(8)`, `+0x24` &= 0x77, `Sprite_ScriptTick`, `LoadDatFile(0xD0)`, `+3` up |
| `0x43DEA0` | `BossMikba_DeathEnd` | 0x3E | step 5: `Sprite_ScriptTick`; `File_LoadDone` (its whole `eax`) not 0 - `Battle_EnemyDefeated`, `+0` &= 0xBF, `+1..+3` = 6, 4, 6 (entry 6 of the death table: `BossOp_ScriptTick` from then on) |
| `0x43DEE0` | `BossMikba_Hook` | 0x10 | `BareRet` x3 |
| `0x43E290` | `BossMikba_DrawQuad` | 0x2A5 | the death's quad, called by steps 2 and 3 with a turn (below) |

**`BossMikba_DrawQuad(turn)`**: a draw-mode primitive at `Gfx_PacketNext`
(`Gpu_SetDrawMode(it, 0, 0, Gpu_GetTPage(2, 0, 0x340, 0x100) & 0xFFFF, 0)`,
`Gfx_CommitPrim(1, 0xC)`) - the page the death's `Gfx_ClearRect` cleared;
`Gte_PushMatrix`; the rotation of the SVECTOR (0, 0, turn << 4)
(`Gte_RotMatrixYXZ` into a stack matrix, `Gte_SetRotMatrix`), a zero
translation (`Gte_TransMatrix`, `Gte_SetTransMatrix`); then a PolyFT4 at
`Gfx_PacketNext` (read again): tpage word `+0x26` = the same page, colour
0x80 x3, four corners (x, y) = (-/+ `+0xC` / 2, -/+ `+0x10` / 2) (s32 halved
toward zero, then a short) in the order (-,-), (+,-), (-,+), (+,+), each
through `Gte_RotTrans`, the vertex floats x = `Sprite_Current +0x18` + out x,
y = `+0x1C` + out y - 0x28 (the `Sprite_Current` of after each call), uv
(0, 0), (0xBF, 0), (0, 0x68), (0xBF, 0x68); `Gpu_SetPolyFT4`,
`Gfx_CommitPrim(1, 0x48)`, `Gte_PopMatrix`. Two quirks of the original,
kept: the corners' SVECTOR is the rotation's SVECTOR reused on the stack, so
its z still holds the turn (harmless: a rotation about z alone leaves x and y
independent of z, and the draw reads only x and y); and `Gte_RotTrans` is
pushed a third argument (the flag pointer) that ours, taking two, drops.
`DeathOpen` pushes its turn as `movzx cx, byte [+9]` with the upper half of
`ecx` still the `Sprite_Current` pointer's; the draw reads the low twelve
bits only (it builds the short `turn << 4`), so ours passes the byte.

### 1.3 F6: Weretigr's turn (`BattleBossFx_Dispatch` slot 6, `kTask`)

BSA's `BossWeretigr_State4Fx` (`0x43D450`) makes Weretigr's turn a task:
`BattleTask_Create(3, 6)`, the enemy object's first 0x80 bytes copied in,
kind 3, `+5` = 6 - so `BattleTask_RunAll` runs it through
`BattleBossFx_Dispatch`'s slot 6, `0x43D6D0`. `Sprite_Current` is the task
slot throughout; `0x93B940` its owner (the enemy).

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43D6D0` | `BossWeretigrFx_Task` | 0x12 | `jmp [BossWeretigrFx_Steps + 4 * +1]` (2: the turn, the trail) |
| `0x43D6F0` | `BossWeretigrFx_Main` | 0x23 | `call [BossWeretigrFx_MainSteps + 4 * +2]` (7), then, the `Sprite_Current` of after with `+0` not 0, a tail jump to `Sprite_UpdateScreen` |
| `0x43D720` | `BossWeretigrFx_Begin` | 0x2E | step 0: the goal height `+0x20` = `+0x3C` + 0x4000000; `Sprite_SetAnimation(2)`; `+0xB` = 0x10 (the trail count); `+2` up |
| `0x43D750` | `BossWeretigrFx_AwaitPose` | 0x1C | step 1: `Sprite_ScriptTickOnce` `al` not 0 - `Sprite_SetAnimation(3)`, `+2` up |
| `0x43D770` | `BossWeretigrFx_Rise` | 0xB8 | step 2: while the s32 `+0x3C` is below `+0x20`, `+0x3C` += 0x1000000; then once: the facing f = `0x904AAC & 0xFF` (with the target `0x904B44` a member, below 3: `+8` ^= 2 and f ^= 2), `+0x34` = `Field_Kind2X` + the signed byte at `0x64E4F4 + 2 f`, `+0x38` = `Field_Kind2Z` + the one after it, `Sprite_SetAnimation(4)`, `+2` up. Both ways a tail jump to `Sprite_ScriptTickOnce` |
| `0x43D830` | `BossWeretigrFx_Strike` | 0x15F | step 3: a trail - `BattleTask_Create(3, 6)`, unless 0xFF: the task's 0x80 bytes copied into the new slot (`rep movsd`), its `+0` &= 0xBF, kind 3, `+5` = 6, `+1` = 1 (the trail), `+2..+4` = 0, owner `+0x80` = the task, `+0xB` = the task's `+0xB`, and the task's `+0xB` down unless 0; then `MagicFx_StepToward(target, 0x50)` toward the target's object (a member 0..2's `ObjTrio` record, else `0x93B960 + (target - 3) * 0x128`), the target read again for `MagicFx_NearSprite3D(it, 0x10000)` - near: `Sprite_SetAnimation(5)`, `Battle_SetTargetFlag40` (the target read again), `+2` up; last `Sprite_ScriptTickOnce` |
| `0x43D990` | `BossWeretigrFx_Leap` | 0xC7 | step 4: `Sprite_ScriptTickOnce` `al` not 0 - the leap back to the acting enemy (`0x904B34`): `+0xC` / `+0x10` / `+0x18` = (its `+0x34` / `+0x38` / `+0x3C` - the task's) / 16 (signed, toward zero), `+0x14` = 0x40, `+0x20` = -8, `+2` up, a tail jump to `Sprite_ScriptTickOnce` |
| `0x43DA60` | `BossWeretigrFx_Fly` | 0x60 | step 5: `+0x34` += `+0xC`, `+0x38` += `+0x10`, the word `+0x3E` += the word `+0x14`, then the dword `+0x3C` += `+0x18` (it holds that word), `+0x14` += `+0x20` (the fall); at `+0x14` = -0x40 `+2` up; tail `Sprite_ScriptTickOnce` |
| `0x43DAC0` | `BossWeretigrFx_Finish` | 0x4E | step 6: the acting enemy's `+0` &= 0xBF (its object shown again); `Sprite_SetAnimation(0)` on the owner (`Sprite_Current` = `0x93B940`, put back after); `BattleTask_FreeCurrent` |
| `0x43DB10` | `BossWeretigrFx_Trail` | 0x23 | `+1` = 1, the trail: `call [BossWeretigrFx_TrailSteps + 4 * +2]` (2), then `Sprite_UpdateScreen` as `_Main` |
| `0x43DB40` | `BossWeretigrFx_TrailBegin` | 0x57 | trail step 0: `+0` \|= 0x20, `+0x5C` = 3, the shade `+0x5D` / `+0x5F` / `+0x5E` = the low byte of `+0xB * 0xF0`, `+9` = 8, `+2` up; tail `Sprite_ScriptTickOnce` |
| `0x43DBA0` | `BossWeretigrFx_TrailFade` | 0x4B | trail step 1: the shade again; `Sprite_ScriptTickOnce`; `+9` down (the `Sprite_Current` of after) - at 0 before it, a tail jump to `BattleTask_FreeCurrent`. **Shared with engine code** (section 7) |

The tool's "11 code entries" for `0x64D6A0` are the three tables in a row:
`BossWeretigrFx_Steps` (2), `_MainSteps` (7), `_TrailSteps` (2).

### 1.4 Named data (`symbols.toml`, 31 `[[data]]`)

Per kind: `_Anims` (12 bytes, `+0xFC`), `_Sounds` (4 words, `+0xF8`),
`_States` (12), `_Hooks` (3), and `_ActSubs` (6) for 37, 38 and 40;
`BossMikba_DeathSteps` (7); F6's three step tables. Counts are the code's
(the state values each dispatcher can reach, the next table), not the
tool's extents ("15 code entries" for kind 34's `+1` table is 12 and its
hook table; "43" for kind 40's runs through all four of its tables). The
signed byte pairs at `0x64E4F4` F6's rise reads are **not** named: engine code
outside the band reads them too (`0x44A18E`, `0x44A302`, `0x44A352`,
`0x452756` - an immediate scan of the image), so they are the engine's to
name; `boss_sg_callees.h` keeps the address.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed (grepped DIVERGENCE.md and `cheats.cpp` for the 53 and their tables:
nothing). The dispatchers, F6's two step callers and the hook tables abort
with a `Fatal` past their tables, where the original jumps or calls through
what follows (round9 doc section 6). The dispatchers hand the caller's stack
word on to their entry and return its `eax` (the `jmp` leaves both in place;
`Port_DroppedCall` in the `+1` tables logs that word's byte); the hooks pass
the word whole.

## 3. The fuzz

`BOF3X_SHADOW=boss_sg`, `src/game/boss_sg_fuzz.cpp`, one `Run` per unit
(`BOF3X_BSG_RUN=<unit>` runs one); every shape by its root: set-ups `kSetup`,
end / exit hooks, the kinds' dispatchers `kDispatch` with their state byte
drawn below their table, entries and death steps `kState`, hooks
`kEnemyHook`, F6's twelve `kTask`, `BossMikba_DrawQuad` `kCallee`. Every
kind's `+1`, `+2`, `+3` and hook tables and F6's three are `DataTable`s
(hook tables with one word, listed first).

RUNS_TABLE

**`kTask` works as documented** - the first user of the shape: `Sprite_Current`
is one of the first four task slots, `0x93B8C4` the same two times in three,
the owner `0x93B940` a task slot or a harness record; a trail copied into a
slot the create stand-in names lands in the compared 48 slots, and a
`Sprite_Current` put back wrongly is refused (G93). What the shape lacks,
added in the group's file: the state bytes `+1` / `+2` a task's dispatchers
read are drawn by `Fix` for the four slots as 0..2 only, past F6's 2-entry
tables; the seed puts the byte the function under test reads inside its
table and the others inside theirs (`OtherStates`), every round.

**Callees added to the standard set** (group listings, registered first):
`BattleTask_Create` answering 0xFF ("none") a third of the time (F6's strike
tests it; the standard listing answers 0..47 only); `Battle_RemoveFromTurnOrder`
louder than the real one (BSE's effect: set-up 32's loop reads the member's
`+0x90` and `+8` after it, and the next member's `+0`); `Sprite_PoseFromSet`
with its pose masked to a byte (BSE's listing); `BossActor_Clear` louder than
the real one (it also moves a byte of party member 0's `+0x34` / `+0x38`,
which set-up 33's end hook reads after it); `BossMikba_DrawQuad` (the group's
own, one word masked 0xFFF); and the five GTE calls of the quad with their
stack pointers masked and their vectors by their bytes (`Gte_RotMatrixYXZ`
the angles, 6; `Gte_SetRotMatrix` the matrix, 18; `Gte_TransMatrix` the
vector, 12; `Gte_SetTransMatrix` the translation, 12, noted; `Gte_RotTrans`
the corner, 6), each writing a result from its inputs and the noise where
the real one writes (`magic_s01_fuzz.cpp`'s shape) so the floats the draw
computes are compared. `Port_DroppedCall` keeps the standard listing (one
byte): the dispatchers hand it the harness's word on both passes.

**Seeds.** The dispatchers: the other state bytes inside the dispatcher's own
table, every round. Kind 40: the flash's `+9` and `+0xA` equal half the time,
at 1, 0, 2, 0x20, 0x40, 0x80, 0xFF, `0x904AAD` bit 2 set and clear; the
spread's `+0xC` / `+0x10` at 0x27..0x2A, 0, -1 and the signed ends, `+0xA` at
0x3E..0x41, 0xFF, 0, 1; the quad's `+0xC` / `+0x10` odd, negative and at the
signed ends (the halving). F6: the rise's height against its goal (equal, one
either side, the signed ends), the target at 0..4, 10, the facing 0..3, 0x80,
0xFF; the strike's target and `+0xB` at 0, 1, 0x10, 0xFF; the leap's and the
finish's acting enemy 3..10 two times in three (0..2 otherwise, still inside
the compared state); the flight's fall `+0x20` with `+0x14` one fall short of
-0x40 two times in three; the fade's `+9` at 0, 1, 2, 0x80, 0xFF. The set-ups:
`0x904AE8` at its bits one at a time and together, `0x904AE5` at 0x40, 0,
0xFF, 0xBF; set-up 32's members' `+0` bit 0 and `+8` where the byte add wraps.
**`Disturb`** moves `0x904AAD`, member 0's `+0x34` / `+0x38`, a member's
`+0x91` / `+0` / `+8`, `Sprite_Current`'s `+0xC` / `+0x10` / `+0x18` /
`+0x1C`, the facing and `Field_Kind2X` / `Z`; F6's `settle` STAR_SETTLE.

STAR_LINE

## 4. Controls

`python controls.py` (the group's scratch script, BSE's shape): each control
one textual change to ours anchored on a string that occurs once, rebuild,
the one `Run` that holds the function, restore, rebuild at the end. A
dispatcher's plant is made in the shared `Dispatch` / `TaskSteps` helper,
keyed on the function's name (the next entry of its table); a shared
helper's plant (`Idle`, `Fallen`, `ActorZero`, `Shade`) is counted against
the function whose `Run` it ran in. CONTROLS_SUMMARY.

CONTROLS_TABLE

## 5. What nothing reached

The fuzz reached every branch the controls planted in. What only a fight
would show: which fight is which in play (the tool's rows are all this doc
says); what `Field_Kind2X` / `Z` hold in battle when F6's rise places
Weretigr beside them (set-up 33's end hook writes them from member 0's
position after the fight; what writes them before it was not read); what
the facing `0x904AAC` holds beyond 0..3; the chapter hand-back of the four
end hooks; the page `0x340, 0x100` the Mikba death clears and draws.

## 6. Latent defects (Capcom's, kept)

- **F6 indexes by the battle's bytes unchecked.** The rise reads the byte
  pair at `0x64E4F4 + 2 * (0x904AAC & 0xFF)` - four pairs; a facing past 3
  reads whatever `.data` follows (a read only, never a fault). The strike,
  leap and finish index enemy objects by `target - 3` and `actor - 3`: with
  a member (0..2) as the actor, the finish's `&= 0xBF` would land in the task
  slots before the enemy objects. In the fight the actor is Weretigr itself
  (an enemy, 3..10), so neither is expected to happen. Ours keeps both reads
  and the write as they are (not a `Fatal`: none indexes past a table ours
  owns).
- **Set-up 32's end hook wraps the pose as a byte**, as BSE's `Boss26_End`
  (a member `+8` above 0xE3 poses from the start of the set).
- **The dispatchers and hook tables index unchecked** (the state byte, the
  word's low byte); ours aborts. Only the engine's words 0..2 reach the hooks.

## 7. Calls across groups

**Out of BSG, to code nobody owns** (raw, `boss_sg_callees.h`): `0x446DE0` /
`0x446E00` (the end phase's steps; the harness's standard set). Stored, not
called: `BossHook_ExitClearActor0`, `BareRet`, `BareRetZero` (BH's, by name).
Table entries, not called by address: the generic enemy states `0x4365D0`,
`0x436620`, `0x436BC0`, `0x436F00`, `0x437030`, `0x437180`, `0x437240` (nobody's)
and `EnemyOp_*`, `Port_DroppedCall`, `BossOp_ScriptTick` (ours). Everything
else is ours by name.

**Into BSG from outside the group** (an E8 / E9 / immediate scan of the image,
2026-09-28): the roots - `Boss_SetupTable` entries 29, 31, 32, 33,
`BossKind_Table` entries 34..38 and 40, `BattleBossFx_Dispatch`'s immediate
`0x43D6D0` at `0x435812`; and **one engine table**: `0x64F0F8`, entry 1 of the
step table `0x64F0F4` that the unnamed engine effect task `0x452B60`
(`BattleFx_Dispatch`'s slot 18, nobody's) calls through by `+1`, holds
`0x43DBA0` - `BossWeretigrFx_TrailFade` is that task's fade step too (its
step 0, `0x452B90`, is `BossWeretigrFx_TrailBegin` with `+1` stepped instead
of `+2`). For the rebinding pass: when `0x452B60` is taken, its table's
second entry is ours. Every other reference is the group's own.

## 8. For `analysis/calltrace/entries_logic.txt`

The 53 extents of section 1 appended to the main checkout's file
(2026-09-28), none there before; `0043E290 2A5` fixes the host line
`0043E290 1120`, which covered it.
