# Group R3A: BATE's root, the battle's end steps, five kind-0 effect tasks, and their neighbours

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, from
the round branch's tip `7f116a2`. **45 functions ours**
(`src/game/rest_3a.cpp`, shadow name `rest_3a`): the cut table's 44 rows for
R3A (`analysis/round14_cut.tsv`, the band `0x404180..0x437820`) and one start
no list had (`0x4041B0`, section 5); none dropped. Each read to its last
instruction with capstone and fuzzed through the boss harness without edits
to it - two `Run`s (section 4): 270,000 rounds, 0 mismatches (this
worktree). CONTROLS_SUMMARY Reached by a recorded route: `0x435A20` (the
round-13 `nue` hash route), `0x404180` before `WorldMap_FrameStep` was ours
(section 9); the rest fuzz only. No divergence; no full-frame fill (the
flash and the tint draw through BE4's `BattleWin_DrawTileRgb` /
`_DrawTileTint`, ours, whose fills are theirs); no ledger entry owed.

Game facts below are what the code does. Where a name says "loss" it follows
[`battle_turn_steps.md`](battle_turn_steps.md) and
[`battle_e1.md`](battle_e1.md) (`0x904AE8` bit 0's way out); "flash",
"tint", "reform" and "restore" are words for what the tasks draw and write,
not claims about what a player sees (the owner's to say).

| Part | What | Fns |
|---|---|--:|
| Area 33's world-map frame | `WorldMap33_FrameStates[1..3]`, the case blocks our `WorldMap_FrameStep` carries inline | 3 |
| BATE (game mode 9) | the root by `0x929F00`, its start and way out, the tally's two sub-dispatchers, the transition step; the equipment screen's windows, commit and frame refresh | 9 |
| The battle's end | `BattleEnd_Steps[2]` (the loss) and `[3]` with their steps, `BattleEnd_ExitSteps[3]` | 7 |
| Kind-0 effect tasks | slots 2 (flash), 4 (tint), 5 (an animation of bank 0x18), 11 (a member shrunk, a DAT loaded, grown back), 12 (the restore's first three states); the actor watch's states 1 and 4 | 22 |
| Kind 3 | `BattleBossFx_Dispatch` | 1 |
| Engine helpers | `BattleEnemy_SetAnimation` on an enemy by battle index (two forms); New Game's records | 3 |

## 1. What each function does

Addresses are Capcom's; each row's `[[func]]` in `symbols.toml` has the
extent and the evidence.

### 1.1 Area 33's world-map frame (`WorldMap33_FrameStates`, `0x5EF5F8`)

`WorldMap_FrameStep` (`0x404160`, ours since round seven) jumps through the
table by `Sprite_Current +2`; ours carries the three case blocks inline and
does not read the table, so these three are reached in our game by nothing -
taken because the table holds them by address (the brief's rule).

| PC | Name | What |
|---|---|---|
| `0x404180` | `WorldMap33_FrameSlideIn` | word `+0x2E` up 0x10; at 0x10 or more (signed) `+2` up; a tail `jmp` to `0x4041B0` |
| `0x4041B0` | `WorldMap33_FrameShown` | the mode byte `0x9045FA` 2 makes `+2` = 3; `WorldMap_DrawFrame(0x10, +0x2E)` |
| `0x4041E0` | `WorldMap33_FrameSlideOut` | `+0x2E` down 0x10; at -0x30 or less `+2` = 0; unless the mode byte is 2, `+2` = 1; the frame drawn |

The y is pushed in a whole register with a stale high half (`edx` /
`eax`); `WorldMap_DrawFrame` reads its low word (world_map.cpp), so ours
passes the word sign-extended and the fuzz compares the low word.

### 1.2 BATE: the root, and the equipment screen's helpers

| PC | Name | What |
|---|---|---|
| `0x42D710` | `BattleExtra_Dispatch` | `Mode8_Step5`'s call: `jmp [BattleExtra_States + 4 * (dword 0x929F00 & 0xFF)]`, 4 |
| `0x42D730` | `BattleExtra_Start` | state 0: `0x929F00` = `0x904C9F` + 2, `0x929F01` = `0x929F02` = 0 |
| `0x42D750` | `BattleExtra_Leave` | state 1: `Window_ResetAll`, `Game_Step` + 1 |
| `0x42D760` | `BattleExtra_TallyDispatch` | state 3: `jmp [BattleExtra_TallySteps + 4 * 0x929F01]`, 3 |
| `0x42D770` | `BattleExtra_TallyOpenDispatch` | `jmp [BattleExtra_TallyOpenSteps + 4 * 0x929F02]`, 2 |
| `0x42D780` | `BattleExtra_OpenTransition` | `Transition_Start(3)`, `0x929F02` + 1 (the first step of both the tally's and the equipment screen's opening) |
| `0x42E0E0` | `BattleExtra_EquipSetupWindows` | window records 0..4 set up as the screen's (in use, kind / state bytes, positions, cursors 0, counts; records 1 and 2's `+0x20` = `0x675EB8`); record 0's `+0xC` = the first of the party list's three entries `0x904062` that is 0 or 7 (left when none is) |
| `0x42E250` | `BattleExtra_EquipCommit` | for each equipment byte `+0x12..+0x17` of character record 7, a chosen byte `0x675EB8[i]` not 0 and not the one held: `Inventory_Remove(0x64AE20[i], chosen, 1)`, `Inventory_Add(0x64AE20[i], held, 1)`, the chosen byte into the record; `Char_RecalcStats(record 7)` |
| `0x42E2F0` | `BattleExtra_EquipRefresh` | window 13's `+0xD` = `Item_EquipMask(window 0's +8, +0xD)` bit 0 clear; record 7's `+0x14, +0x12, +0x13, +0x15, +0x17` into `0x675EBA, B8, B9, BB, BC`; `0x675EB8[window 1's +0xA]` = window 0's `+0xD`; al `+0x17` (unread) |

`BattleExtra_Start` sets 2 (the equipment screen, BE1's) or 3 (the tally)
as far as the writers of `0x904C9F` read ([`battle_e1.md`](battle_e1.md)
section 1.1). Two observations, not defects: `EquipRefresh` copies `+0x17`
into the fifth chosen byte where `EquipCommit` pairs that byte with `+0x16`,
and never copies `+0x16`; the screen's two slots are `0x675EB8[0]` and
`[3]` (`+0x12` and `+0x15`, BE1's slot cursor flipped by `xor 3`), so the
other four only matter if a cursor reaches them.

### 1.3 The battle's end

| PC | Name | What |
|---|---|---|
| `0x431540` | `BattleEnd_LossDispatch` | `BattleEnd_Steps[2]`: `jmp [BattleEnd_LossSteps + 4 * 0x904AA2]`, 3 |
| `0x431550` | `BattleEnd_LossBanner` | unless `0x904AE5` bit 0x40: `Music_FadeOutStop(10)`, `Music_Play(0xA6, 10)`; `Battle_OpenMsgWindow`; `BattleBanner_Set(0, 2, 0, 0, 0xFF, Msg_SystemPtr(0x11))`; `0x904AA2` + 1 (the win's `BattleEnd_WinBegin` plays 0xA5) |
| `0x4315A0` | `BattleEnd_LossAwaitLoad` | `0x904AA2` + 1 once `File_LoadDone`'s eax is not 0; then `BattleLoss_Dispatch` (BE1) |
| `0x4315B0` | `BattleEnd_RestoreDispatch` | `BattleEnd_Steps[3]`: `jmp [BattleEnd_RestoreSteps + 4 * 0x904AA2]`, 5 |
| `0x431710` | `BattleEnd_RestoreLoadBank` | once `File_LoadDone`: `Snd_LoadBankFile(Game_AreaNumber + 3)`, `0x904AA2` + 1 |
| `0x431740` | `BattleEnd_RestoreAwaitBank` | once `File_LoadDone`: `0x904AA1` = 4 (`BattleEnd_ExitStep`), `0x904AA2` = 0 |
| `0x4318F0` | `BattleEnd_ExitAwaitFade` | `BattleEnd_ExitSteps[3]`: once `MoveScript_WaitWordDA` is 0, `Draw_PassFlags` = 0, `0x904AA2` - 1 |

`BattleEnd_Steps[3]`'s steps are `BattleEnd_TasksBegin`,
`BattleEnd_StartMemberTask`, BE1's `BattleEnd_AwaitRestore`, then the two
here; who sets `0x904AA1` = 3 was not found among the engine's writers
(`BattleEnd_AwaitMemberTasks` sets 1 or 2): an event battle's hook, perhaps.

### 1.4 Kind-0 effect tasks (`BattleFx_Dispatch`'s slots) and the actor watch

Each dispatcher calls through a table it builds on its stack, unchecked.
Two pointers are read, as in [`battle_fx_tasks.md`](battle_fx_tasks.md):
the running slot `0x93B8C4` and `Sprite_Current` (the same slot in the game,
re-read where the original re-reads); the slot's owner is `0x93B940`.

| PC | Name | What |
|---|---|---|
| `0x432F90` | `BattleFxFlash_Dispatch` | slot 2: by the slot's `+1`: `_Banner`, `_Rise`, `_Fall` |
| `0x432FC0` | `BattleFxFlash_Banner` | `BattleBanner_Add(1, 0, 0, 0x1E, [0x669DFC] for a party actor 0x904B34, [0x669E00] for an enemy)`; window 4's `+3` = 1; the slot's `+0xC` = 0x100, `+0x10` = 0, `+9` = 2, `+1` up |
| `0x433020` | `BattleFxFlash_Rise` | `Camera_Distance` - word `+0xC`; a draw mode (`Gpu_GetTPage(0, 0, 0x3C0, 0)`, committed 1, 0xC); `BattleWin_DrawTileRgb(0, 0, 0xB, colour, 1)` - word `+0x10` x 0x421 for a party actor (the same five bits in r, g, b), `<< 10` for an enemy (red); `+0x10` + 8; `MapView_Redraw` = 2; `+9` down, at 0 `+1` up and `+9` = 2 |
| `0x4330E0` | `BattleFxFlash_Fall` | the same with `Camera_Distance` + `+0xC` and `+0x10` - 8; at `+9` 0 a tail `jmp` to `BattleTask_FreeCurrent` |
| `0x4332B0` | `BattleFxTint_Dispatch` | slot 4: by the slot's `+1`: `BattleFx_StepReset`, `_Brighten`, `_Hold` |
| `0x433300` | `BattleFxTint_Brighten` | `Sprite_Current +9` into the three bytes `0x903850..52`; `BattleWin_DrawTileTint(0, 0, 0xB)`; `+9` + 1, at 0xFF `+1` up |
| `0x433350` | `BattleFxTint_Hold` | the same, held |
| `0x433380` | `BattleFxAnim_Dispatch` | slot 5: by `Sprite_Current +1`: `_Start`, `_Run`, `_Linger`; then `Sprite_QueueOverlay` while `+0` bit 0 |
| `0x4333C0` | `BattleFxAnim_Start` | `Sprite_SetAnimationBank(0x18)`; `+0x24` = 0x80, `+0x2A` = 0; animation 0 when the action's ability (word `+2` of `0x904B40`'s record) has bit 1 in its `Ability_Records` byte `+0x15`, else 2; `+1` up |
| `0x433410` | `BattleFxAnim_Run` | once `Sprite_ScriptTick`'s al is not 0: `+1` up, `+9` = 0x10 |
| `0x433430` | `BattleFxAnim_Linger` | `Sprite_ScriptTickOnce`; `+9` - 1; at 0 the round flags `|= 0x20` and a tail `jmp` to `BattleTask_FreeCurrent` |
| `0x433550` | `BattleFx_WatchIconStart` | `BattleFx_ActorWatch`'s state 1: `+0xB` = 0; `i` = `BattleFx_NextStatusIcon`; unless `i` is 0xFF or `0x64B048[i]` is 0xFF: `+0xB` = `i`, the icon sprite's fields (`+0x29` 2, `+0x25` 5, `+0x26` 0xF0, `+0x24` 0x80, `+0x27` 0xFF, `+0x28` 0, word `+0x2C` 1, `+0x2B` 0, `+0x48` 0, `+0x44` / `+0x40` 0x10000, `+0xC` 0, `+0x18` 0x666, `+0xA` 0x3C), `BattleFx_PlaceOverOwner`, `Sprite_SetAnimation(0x64B048[i])`, `+1` + 2 (to BE2's `BattleFx_WatchIcon`) |
| `0x433790` | `BattleFx_WatchRecheck` | state 4: with the round flags' bit 2, the owner's actor `+5`'s status (a member's `+0x90`, an enemy's `+0x92`): any of 0x58, `+1` - 1 (back to state 3); else `+1` = 0 |
| `0x433970` | `BattleFxReform_Dispatch` | slot 11: by `+1`: `_Begin`, `_Shrink`, `_LoadDat`, `_Reload`, `_Grow` |
| `0x4339B0` | `BattleFxReform_Begin` | the owner's `+0x48` = 2; `+1` up |
| `0x4339D0` | `BattleFxReform_Shrink` | state 1 of slots 11 **and 12**: the owner's u32 `+0x40` - 0x2000 while not 0; at 0 its `+0 |= 0x40`, `+1` up |
| `0x433A00` | `BattleFxReform_LoadDat` | `LoadDatFile` 0x2E8 / 0x2EA (the owner's word `+0x2C` 0; `+8` 0 or 1 / other) or 0x2E9 / 0x2EB (`+0x2C` not 0); `+1` up |
| `0x433A50` | `BattleFxReform_Reload` | once `File_LoadDone`: the owner made `Sprite_Current` and its member (`+5`) `Field_State`; `Sprite_SetAnimation(+8 + 4)`, `Sprite_ReleaseTint(member)`, `Sprite_LoadPalette(0x80D380 + 0x40 * +5, 0)`, `Battle_StatusTint(member +0x90)`, `Sprite_SetClutStp`; `Field_State +0x134` bit 0 cleared; `BattleParty_RecalcStats`; `Sprite_Current` back, its `+1` up |
| `0x433B20` | `BattleFxReform_Grow` | the owner's `+0` bit 0x40 cleared; `+0x40` + 0x2000 to 0x10000; there `+0x48` = 0, the member's `+0x130` bit 0x2000 cleared, a tail `jmp` to `BattleTask_FreeCurrent` (BE2's `BattleFx_RestoreFade` without the `+0x134` mask) |
| `0x433B80` | `BattleFxRestore_Dispatch` | slot 12: by `+1`: `_Begin`, `BattleFxReform_Shrink`, `_LoadDat`, BE2's `BattleFx_RestoreParty`, `BattleFx_RestoreFade` |
| `0x433BC0` | `BattleFxRestore_Begin` | the owner's member gets `+0x130` bit 0x2000; the owner's `+0x48` = 2; `+1` up |
| `0x433C00` | `BattleFxRestore_LoadDat` | with the round flags' bit 15: `Battle_BackupFlagged` not 0 - for the party set `0x90412C` 7, 0xD, 0xE, 0xF (read again for each) `LoadDatFile` 0x122..0x125 (the owner's `+8` 0 or 1) or 0x126..0x129; 0 - `set + 0xFC` or `set + 0x10F`; without bit 15 the u16 of the set's entry of the twenty at `[0x64EA84]` (`+8` 0 or 1) or `[0x64EA88]`; `+1` up |

BE2's doc ties slot 12 to Accession's single-member form (round flags
0x8000); slot 11's DAT files 0x2E8..0x2EB and its member re-posed from its
own `+8` are what the code shows - what it is in play was not established.

### 1.5 Kind 3, the enemy helpers, New Game

| PC | Name | What |
|---|---|---|
| `0x4357D0` | `BattleBossFx_Dispatch` | `BattleTask_RunAll`'s kind 3: by the slot's `+5` through `BareRet`, `Magic002Ball_Task`, `BossGazerFx_Dispatch`, `BossAnglerFx_Dispatch`, `BossDLordFx_Dispatch`, `BossMyriaFx_Dispatch`, `BossWeretigrFx_Task`, `BossArwanFx_Dispatch` (named in round eleven, taken here). The entry is called with the table under its return address, so its stack words are the table's: three of the entries are dispatchers that hand their first word on; ours passes the eight words |
| `0x435A20` | `BattleEnemy_SetAnimationAs(actor, animation)` | enemy object `0x93B960 + 0x128 * ((actor & 0xFF) - 3)` made `Sprite_Current` and `0x939AD8` for `BattleEnemy_SetAnimation(animation)` (the whole word), both put back. Callers `Howling_Start`, `BattleActor_SetAnimation` |
| `0x435A70` | `BattleEnemy_SetAnimationOf(actor, animation)` | the same with `0x939AD8` alone; twelve callers in the magic effects |
| `0x437820` | `NewGame_InitCharacters` | `TitleFlow_NewGame`'s call: the seven `Char_DefaultRecords` over `CharacterRecords` 0..6 (`rep movsd`), each followed by `(level << 4) / 0x64B8B0[2n] + 0x64B8B1[2n]` (a signed `idiv`), the low byte stored at `+0x4E`, `+0x1C`, `+0x2E` (the old evidence's offsets were wrong, corrected); then `0x64B80C` over the record `Char_WhelpSlot` names |

PSX twins (`analysis/pairs_propagated.json`): `0x435A20` is
`0x801E247C`, `0x435A70` is `0x801E2500` (both call-anchored). The
sibling's `names/functions.toml` calls `0x801E2500`
`Examine_EnemyMove_Tick`, a callstack-diff hypothesis; it does not fit what
the code does (an animation set on an enemy by index) and is not
transferred. No other row has a pair.

## 2. Divergence

None: each function is a faithful replacement. `DIVERGENCE.md`,
`cheats.cpp` and `widescreen.cpp` patch no byte inside the 45 (a grep of
every address). DIV-0020 reads two instructions inside
`NewGame_InitCharacters` (`0x437834`, `0x437891`) before writing the
default names into the records it copies: the inject's five-byte `jmp` at
`0x437820` leaves both in place, and ours copies from the same records, so
the overlay's names still flow (not measured live, as BE1's `0x42E09D`).
No full-frame fill: the flash and the tint call BE4's tile draws (ours).

## 3. The tables (`symbols.toml` `[[data]]`)

Each count is the reader's reach, read by hand: the run of code pointers to
the next table, against what the steps write.

| Table | Count | Reader | Entries |
|---|--:|---|---|
| `BattleExtra_States` `0x64ADAC` | 4 | `BattleExtra_Dispatch` by `0x929F00` | `_Start`, `_Leave`, BE1's `_EquipDispatch`, `_TallyDispatch`; `BattleExtra_TallySteps` follows |
| `BattleExtra_TallySteps` `0x64ADBC` | 3 | `BattleExtra_TallyDispatch` by `0x929F01` | `_TallyOpenDispatch`, BE1's `_TallyCount`, `_TallyClose` |
| `BattleExtra_TallyOpenSteps` `0x64ADC8` | 2 | `BattleExtra_TallyOpenDispatch` by `0x929F02` | `_OpenTransition`, BE1's `_TallyOpen`; the tally's step bytes (data) follow |
| `BattleEnd_LossSteps` `0x64AF70` | 3 | `BattleEnd_LossDispatch` by `0x904AA2` | `_LossBanner`, `_LossAwaitLoad`, BE1's `BattleLoss_Dispatch` |
| `BattleEnd_RestoreSteps` `0x64AF7C` | 5 | `BattleEnd_RestoreDispatch` by `0x904AA2` | `BattleEnd_TasksBegin`, `_StartMemberTask`, BE1's `_AwaitRestore`, `_RestoreLoadBank`, `_RestoreAwaitBank`; `BattleEnd_ExitSteps` follows |

`band_rows.py` counted 9 code entries for `0x64ADAC` and 31 / 28 for
`0x64AF70` / `0x64AF7C` (it runs on into the next tables). Read in place,
not named (no copy here): the categories `0x64AE20` (six bytes), the
banner strings `0x669DFC` / `0x669E00`, the status icons' animations
`0x64B048` (16), the restore's file tables at `[0x64EA84]` / `[0x64EA88]`
(twenty u16 each), the divisor pairs `0x64B8B0`.

## 4. The fuzz (`rest_3a_fuzz.cpp`)

Two `boss_harness::Run`s under `BOF3X_SHADOW=rest_3a`, 6,000 rounds a
function (`BOF3X_R3A_ONLY=<hex address>` runs one clone):

- **`rest_3a.outside`** (no engine frame) for the nine below the engine's
  runs - the harness's `kEngineBands` start at `0x42D7A0`, and an engine
  group's clone outside them is a Fatal. The three world-map states and
  BATE's three steps `kState`; BATE's three dispatchers `kDispatch` with
  `state_cell` `0x929F00` / `01` / `02` drawn below 4 / 3 / 2; the three
  tables `DataTable`s. Regions: BATE's mode bytes, `0x904C9F`, `Game_Step`,
  `0x9045FA`. Callees listed: `WorldMap_DrawFrame` (the y at its low word),
  `WorldMap33_FrameShown` (`kPhase`, `0x404180`'s tail), `Window_ResetAll`.
- **`rest_3a`** (`Group::engine`) for the 36 in the runs: the slot
  dispatchers and states `kTask` (the dispatchers' `states` 3 / 5 / 8 at
  `+1` / `+5`), the BattleEnd steps `kStep`, its two dispatchers
  `kDispatch` on `0x904AA2`, the equipment helpers, the enemy helpers and
  New Game `kHelper` (`ret_mask 0xFF` on `BattleExtra_EquipRefresh`, the
  only one that answers). Its stack tables' immediates are re-aimed by the
  harness; ours calls each entry by the address the original stores
  (`Phase`). Callees re-listed at what they read: `BattleWin_DrawTileRgb`
  (x, y low words, size and abe a byte, the colour's bits 0..14 - the
  original pushes it with a left-over high half), `BattleWin_DrawTileTint`,
  `Music_FadeOutStop` / `_Play`, `Snd_LoadBankFile`,
  `BattleFx_NextStatusIcon` (`kByte` 0xFF..0x0F, BE2's range),
  `BattleFx_PlaceOverOwner`, `BattleParty_RecalcStats`,
  `Battle_BackupFlagged` (`kFlag`), `Battle_StatusTint` and
  `BattleEnemy_SetAnimation` whole (both sides push the whole word).
  Regions beyond the engine frame's: the six chosen bytes `0x675EB8`,
  character records 1..7 past the frame's, the party list `0x904060`,
  `Game_AreaNumber`, `MoveScript_WaitWordDA`, `Draw_PassFlags`,
  `Camera_Distance` .. the tint bytes `0x903850`, `MapView_Redraw`, the
  party set `0x90412C`, `Char_WhelpSlot` (37 regions, 37,136 bytes).

**Seeds** (after the harness's fill): the party set 7 / 0xD / 0xE / 0xF two
times in three, else below 20; the party list 0, 7 or other members; each
chosen byte 0, the held one or another; window 1's cursor 0 / 3 or below 6;
`0x904AE5` bit 0x40 both ways; the wait word 0 half the time; the running
slot's `+1` below 3 and `+5` below 8 (the stack tables); the actor 0..4 and
10; the counters `+9` at 0, 1, 2, 0xFE, 0xFF; the ability id below 228;
the round flags' bits 2 and 15 both ways; the owner a party member with its
own index at `+5` half the time (else `+5` 0..2), 0..10 for the recheck;
its scale `+0x40` at 0, 0x2000, 0xE000, 0x10000, 0xFFFFE000, any; `+8` 0..3,
`+0x2C` 0 half the time; `Char_WhelpSlot` 7 or 0..7; the world map's y
around 0x10 / -0x30 and the 0x8000 wrap, the mode byte 2 or not. **Args**:
the enemy helpers' actor 3..10 with garbage above half the time.
**Disturbance** (the group's, from the hash only): `0x904AA2`, a chosen
byte, an equipment byte of record 7, the party set (below 20), the slot's
`+9` / `+0xA` and word `+0x10`, the owner's `+8`, `0x904AE5`,
`Game_AreaNumber`, `Sprite_Current +9`; outside: `0x929F02`, the mode
byte, the world map's y and state.

**Two clones are `calm`** (no recorder moves anything while they run):
`BattleExtra_EquipRefresh` and `BattleFxReform_Reload`. The engine
disturbance writes a random byte into the current window record (window 1's
cursor, read after `Item_EquipMask`) and re-points the owner `0x93B940`
(read after `File_LoadDone`) at a record whose `+5` is any byte; past the
six chosen bytes and past the three members the original writes on through
memory outside the compared state, where ours aborts (section 7). Their
re-reads after the call are therefore not disturbed; each is read once in
the original, so no stale value can hide there except the one read.

**Result** (in this worktree, exit 0):

    shadow      rest_3a.outside self-test: 54000 rounds over 9 functions (6000 each), 48000 calls to the stand-ins, 0 MISMATCHES; 33996 bytes of state (20 regions) and the stand-ins' log compared
    shadow      rest_3a self-test: 216000 rounds over 36 functions (6000 each), 297440 calls to the stand-ins, 0 MISMATCHES; 37136 bytes of state (37 regions) and the stand-ins' log compared

Every listed callee and every table and stack-table entry was reached (the
coverage lines: `Sprite_QueueOverlay` 3,019, `BattleFx_PlaceOverOwner`
1,041, the eight kind-3 entries 731..765 each the rarest). The first run of
the engine group stopped on `BattleFxReform_Reload`'s abort (section 4's
calm); no mismatch on any run.

STAR_RESULTS

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 44 (3,596 bytes against the cut's
  3,819: 36 differ by padding only, none by code); each checked by hand to
  its `ret` or tail `jmp`. Its `0x404180` (0x58) ran over `0x4041B0`'s body
  after the tail `jmp`: the function is 0x21 bytes.
- **`0x4041B0` is added**: `WorldMap33_FrameStates[2]` and `0x404180`'s
  tail-jump target, with its own `ret` - an entry by address, in no group's
  list (inside `WorldMap_FrameStep`'s catalogue extent, which is why the
  tool did not flag it). No start dropped: none is a jump-table case or a
  shared tail reached by a conditional jump.
- **Hidden starts**: 37 of the 44, each reached by a `.data` table or a
  stack table's immediate. Hosts already ours: `WorldMap_FrameStep`
  (`0x404160`, its three case blocks inline - section 1.1),
  `Battle_ActorSkipped` (`0x431030`, ours 0x60 bytes: holds none of the
  seven), `BattleFx_RollingDigits` (`0x432F10`, ours: holds none of the
  22), `BattleTask_ClearAll` (`0x435260`: not `0x4357D0`). The others' hosts
  (`0x42D710`) are this group's.
- **The cut's columns**: `0x4357D0` and `0x437820` were named (round eleven
  and 2026-09-21) and are bound in place; the `hypothesis` units
  (`battle_e1`, `battle_items`, `magic_s01`, `save_menu`) are the callers'
  modules, not what the functions are.
- **The harness's rows**: `boss_harness.cpp`'s engine set lists `0x42E0E0`,
  `0x42E250` (garbage) and `0x42E2F0` (`kFlag`) by address and
  `scenario_harness.cpp` lists `0x42D710` (FC3's) - recorders keyed by the
  address, which ours keeps: none breaks, and each matches what was read
  (no words; `0x42E2F0` answers a byte in al). Not edited.

## 6. Controls

CONTROLS_TABLE

## 7. Latent defects and unchecked indexes (Capcom's, described)

Ours reproduces each but aborts where the original writes or jumps through
memory it does not own (round9 doc section 6).

1. **Every dispatcher is unchecked** (`BattleExtra_Dispatch` and its two,
   the two BattleEnd ones, the five kind-0 slots' stack tables,
   `BattleBossFx_Dispatch`): a byte past the table jumps or calls through
   the next table or the caller's frame. Ours aborts. Every writer read
   keeps the bytes inside (the steps' + 1, the resets).
2. **`BattleExtra_EquipRefresh` writes `0x675EB8[window 1's +0xA]`
   unchecked**: a cursor past the six chosen bytes writes on into
   `0x675EBE..` (the bytes `BattleExtra_EquipFadeIn` sets) and beyond. The
   screen's cursor is 0 or 3. Ours aborts at 6 or more.
3. **The member index of slots 11 and 12 is the owner's `+5`, unchecked**:
   `BattleFxReform_Reload` sets `Field_State` and passes the member's
   record and palette slot to callees that write them, `_Grow` and
   `BattleFxRestore_Begin` write the member's `+0x130` - past 2 into the
   window records and on. Ours aborts on the index of a write; the
   `Battle_StatusTint` read and `BattleFx_WatchRecheck`'s enemy read
   (`+5` - 3, past 7) are kept (BE2's rule for the watch).
4. **`BattleFxRestore_LoadDat` indexes the twenty-entry file tables by the
   party set unchecked**: a set of 20 or more (`PartySet_Select`'s mode 0
   stores `set | 0x80`) reads the next table's words as a DAT file number;
   the backup branch would likewise load `set + 0xFC` past its files. Ours
   aborts on the table read; the other branch is kept. Whether a battle
   can reach this state with bit 7 set was not established.
5. **`BattleFx_WatchIconStart` indexes `0x64B048` (16 entries) by
   `BattleFx_NextStatusIcon`'s answer**, which is 0..15 by its code (BE2);
   ours aborts past 15.
6. **`BattleFxAnim_Start` reads `Ability_Records` by the action's u16
   ability id unchecked** (kept: a read, as `battle_flow.cpp`'s).
7. **`BattleEnemy_SetAnimationAs` / `_Of` take any actor byte**: below 3
   the record is below the enemies, past 10 past them, and
   `BattleEnemy_SetAnimation` writes it. Every caller read passes an enemy.
   Ours aborts outside 3..10.
8. **`NewGame_InitCharacters`**: a divisor of 0 in `0x64B8B0` would fault
   (the table has none); a `Char_WhelpSlot` past 7 writes past the eight
   records into `Cond_Flags`. Ours aborts on both.
9. **`BattleFxReform_Shrink` / `_Grow` step the scale by 0x2000 to exactly 0
   / 0x10000**: a scale that is not a multiple of 0x2000 never meets the
   end and wraps past it. The writers of `+0x40` were not all read (BE2's
   restore sets 0, `BattleFx_WatchIconStart` 0x10000). Kept.

## 8. Calls across groups

**Outbound**: none raw. By name, already ours: BE1's `BattleLoss_Dispatch`
and BATE's steps (table entries), BE2's `BattleFx_NextStatusIcon`,
`_PlaceOverOwner`, `Battle_BackupFlagged`, `BattleFx_RestoreParty`,
`_RestoreFade`, BE3's `BattleParty_RecalcStats`, BE4's
`BattleWin_DrawTileTint`, the boss round's kind-3 entries, and the
engine's.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `Mode8_Step5` `0x517330` | ours (field_c3) | `BattleExtra_Dispatch` (`kMenuDispatchA`, rebound) |
| BE1's `BattleExtra_EquipOpen`, `_EquipListInput` | ours | `_EquipSetupWindows`, `_EquipCommit`, `_EquipRefresh` (rebound) |
| `BattleEnd_Step` `0x4311E0` through `BattleEnd_Steps[2]`, `[3]`; `BattleEnd_ExitStep` through `BattleEnd_ExitSteps[3]` | ours (battle_turn_steps) | the two dispatchers, `BattleEnd_ExitAwaitFade` (read in place) |
| `BattleFx_Dispatch` (stack table) | ours (battle_fx_tasks) | slots 2, 4, 5, 11, 12 (its `kOriginals` rebound) |
| `BattleFx_ActorWatch` (stack table) | ours | `BattleFx_WatchIconStart`, `_WatchRecheck` (rebound) |
| `BattleTask_RunAll` (stack table) | ours (battle_flow) | `BattleBossFx_Dispatch` |
| `Howling_Start`, `BattleActor_SetAnimation`; the magic effects (twelve sites) | ours (battle_items, magic_s01/05/06/10) | `BattleEnemy_SetAnimationAs` / `_Of` (constants rebound) |
| `TitleFlow_NewGame` `0x5880E0` | ours (save_menu) | `NewGame_InitCharacters` |
| `WorldMap_FrameStep` | ours | the three world-map states: not called (inline) |

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`): world map
`0x404180` 1 and `0x4041E0` -564 (the host entered); combat -72 for the
seven BattleEnd rows and -98 for the 22 kind-0 rows (their hosts entered,
an upper bound), -1 for `0x4357D0`; nothing for BATE, the equipment
helpers, the enemy helpers and New Game. The traces under
`analysis/calltrace` (a grep of every file but the extent lists): `0x404180`
in `hidden_worldmap`'s first-call trace (from `0x404155`, when
`WorldMap_FrameStep` was Capcom's - ours no longer jumps through the table);
`0x435A20` called twice in round thirteen's `hash_r13_nue_*` call counts
(the nue route: the coordinator's frame-hash A/B covers it); `0x437820` "not
reached" in `reach_dragon` / `reach_whelp`. **Everything else is fuzz
only.** No live run was made (the brief). A route through BATE (game mode 9),
a lost battle, or an Accession-form battle would reach the rest.

## 10. The rebinding

`grep -rn -i` of the 45 addresses in `src/game` (`band_rows.py --refs`: 132
references to 24 of them). Rebound, each on its own line, the value
unchanged (the fuzz keys stand):

| File | Reference | Now |
|---|---|---|
| `battle_e1_callees.h` | `kEquipOpenHelper` / `kEquipConfirmHelper` / `kEquipFrameHelper` | `bof3::addr::BattleExtra_EquipSetupWindows` / `_EquipCommit` / `_EquipRefresh`; `kMode`'s and `kEquipOpenSteps`' comments name `BattleExtra_Dispatch`, `BattleExtra_States`, `BattleExtra_OpenTransition` |
| `field_c3_callees.h` | `kMenuDispatchA = 0x42D710` | `bof3::addr::BattleExtra_Dispatch` |
| `battle_items_callees.h` | `kEnemyAnimation = 0x435A20` | `bof3::addr::BattleEnemy_SetAnimationAs` |
| `magic_s10.cpp` | `kEnemyAnimCurrent` / `kEnemyAnim` | `BattleEnemy_SetAnimationAs` / `_Of` |
| `magic_s01.cpp`, `magic_s05.cpp`, `magic_s06.cpp` | `kEnemyAnimation = 0x435A70` | `bof3::addr::BattleEnemy_SetAnimationOf` |
| `battle_fx_tasks.cpp` | `kOriginals`' `H(0x432F90)`, `H(0x4332B0)`, `H(0x433380)`, `H(0x433970)`, `H(0x433B80)`, `H(0x433550)`, `H(0x433790)` | `H(bof3::addr::...)` |
| `battle_turn_steps.cpp` | two comments | the names beside the addresses |
| `battle_flow.cpp:116`, `battle_obj_states.cpp:17`, `battle_windows.cpp:138` | "the battle frame 0x42E2F0" | `Battle_Frame 0x42E370` - those three comments named `Battle_Frame` by a stale catalogue start; `0x42E2F0` is `BattleExtra_EquipRefresh` |

**Left raw**: the fuzz files' keys and call sites (`battle_e1_fuzz.cpp`,
`battle_turn_steps_fuzz.cpp`, `battle_fx_tasks_fuzz.cpp`,
`battle_flow_fuzz.cpp`, `battle_items_fuzz.cpp`, `field_c3_fuzz.cpp`,
`save_menu_fuzz.cpp`, `magic_s01/05/06/10_fuzz.cpp` - round ten's rule);
the harnesses' rows (`boss_harness.cpp` 806..808, `scenario_harness.cpp`
725, `boss_harness.h` / `scenario_harness.h` comments - not a group's to
edit); `area_w4f.*`'s `0x42D710` (a band's end, a coordinate); the other
comments that name an address beside what it does (`battle_e1.cpp`,
`battle_e2.cpp`, `battle_fx_tasks*.{cpp,h}`, `battle_flow*`,
`battle_items*`, `magic_s*.cpp`, `char_names.cpp`, `world_map_callees.h`).
No reference in a file another group of this wave is known to write.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04): a comment line and 41
extents (`0x42E0E0`, `0x42E250`, `0x435A20`, `0x435A70` were listed at the
extent read). Smaller extents added under longer lines left in place:
`0x42D710` (0x11 under 0x90), `0x42E2F0` (0x72 under 0x80), `0x437820`
(0x8B under 0x90). Host lines over these left for the coordinator:
`00404160 D0` (`WorldMap_FrameStep`, ours 0xC1, over the three world-map
states) and `00432F10 280` (`BattleFx_RollingDigits`, over `0x432F90..
0x433190`).
