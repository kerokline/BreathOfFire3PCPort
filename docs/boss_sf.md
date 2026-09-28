# Group BSF: fights 27 and 28, kinds 33 and 62 (Gazer, Myria), fight 26's count and the Gazer's effect task

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave two. 54 functions of `0x43C480..0x4406D7` ours (`src/game/boss_sf.cpp`,
shadow `boss_sf`), each read to its last instruction with capstone and
fuzzed through the boss harness ([`boss_harness.md`](boss_harness.md)), six
`Run`s, 0 mismatches; 197 controls planted, 197 refused by a count (section 5). The group's other ten
functions (the Paralyzer and Head Cracker rows, `Magic_Rows` 123 and 128)
were ours already; nothing here calls them. Fuzz only: no recorded route
reaches a boss fight.

Enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's area
records), not memory of the game. Which fight a set-up is comes from the
tool's rows; nothing here says what happens in a fight.

## 1. The units

`tools/boss_rows.py --unit <U> --clones` for the six units (2026-09-28), 54
functions to take, every one in `analysis/boss_funcs.tsv`'s group column as
BSF. None found inside another's extent, none missing; every extent the
tool gave was the code's. The tool's table counts were too long, as wave one
found (the "29 code entries" of kind 33's `+1` table is 12, then its own
`+2` tables); the counts below are the code's.

| Unit | Root | What it is (the tool's rows) | Functions |
|---|---|---|--:|
| B27 | `Boss_SetupTable[27]` `0x43C480` | `BOSS027`, row 6: one of the five set-ups the tool leaves open (no kinds of its own; plan section 7). Its hooks touch enemies 0 and 1 and field actors 1 and 2; which fight it is was not settled from the code (it compares no fight byte) | 4 |
| FB8 | `BattleFx_Dispatch` slot 8 (`0x4352F2`'s immediate) | fight 26's count: `Boss26_Event` (group BSE's) creates it with `BattleTask_Create(0, 8)` | 3 |
| K33 | `BossKind_Table[33]` `0x43C810` | Gazer (area 77) | 9 (one, `0x43C9A0`, shared with K62) |
| B28 | `Boss_SetupTable[28]` `0x43C9D0` | `BOSS028`, area 77 row 7: the Gazer's fight | 2 |
| F2 | `BattleBossFx_Dispatch` slot 2 (`0x4357F2`'s immediate) | the Gazer's effect task: its state 4 creates it with `BattleTask_Create(3, 2)` | 9 |
| K62 | `BossKind_Table[62]` `0x440080` | Myria (area 198); its set-up 55 is group BSJ's | 27 (plus `0x43C9A0`) |

**The sibling's per-image counts** (`analysis/overlay_captures_all.json`,
`static_discovery_entry_pcs`) as the check on size: BOSS028 22 (B28, K33 and
F2 are 20 here); BOSS055 55 (K62's 28 here, the rest B55 and the slot-5
task F5, group BSJ's); BOSS027 and fight 26's BOSS025 image (FB8's 3 with
BSE's 32 against 36) as the tool gives them - BOSS027 has no capture.
**The shared body**: `0x43C9A0`, kind 33's state 5 step 1, is also entry 1
of Myria's state-5 table; Gazer's image and Myria's are two PSX files, and
the PC compiled the body once (under the Gazer's, the first in address
order).

## 2. What each function does

### Set-up 27 (B27)

| Address | Name | Bytes | Shape | What it does |
|---|---|--:|---|---|
| `0x43C480` | `Boss27_Setup` | 0x1F | kSetup | Boss_SetupTable[27] (BOSS027, row 6: one of the five set-ups tools/boss_rows.py leaves open - no kinds of its own): stores BattleHook_End = 0x43C5C0, _Exit = 0x43C6F0, _Event = 0x43C4A0; ret. |
| `0x43C4A0` | `Boss27_Event` | 0x119 | kEvent, al | set-up 27's event hook by the code's low byte, al 0 always: 0 - enemy 0's or enemy 1's +0x92 bit 0x4000 sets 0x904AAD bit 0 / 1 and 0x904AE8 bit 2; else with 0x904AE8 0 and 0x904AAD bit 3 clear, the actor before the turn order's cursor (0x904ACB + 0x904AE2) a member whose +0x130 & 3 is 0 - 0x904AE8 = 4, 0x904AAD \|= 4; then 0x904AAD bit 5 clears bits 5 and 3. 2 - with 0x904AAD bit 4 and not 0x904AE8 bit 2: bit 4 cleared, Sprite_Current = enemy 1, Sprite_EnsureAnimation(0xC), 0x904AAD \|= 8. |
| `0x43C5C0` | `Boss27_End` | 0x124 | kEnd | set-up 27's end hook: Boss26_End's party loop (Battle_RemoveFromTurnOrder, Sprite_PoseFromSet by +0x90 bit 14) for each member with +0 bit 0; then by 0x904AAD bits 0 / 1 / 2 the chapter step 0x8034E5 = 0xF / 7 / 0x14 in turn; jmp 0x446E20. |
| `0x43C6F0` | `Boss27_Exit` | 0x47 | kExit | set-up 27's exit hook: with 0x904AAD bit 0 BossActor_CopyFrom(1, enemy 0, 0); BossActor_ClearBit40(1); with bit 1 BossActor_CopyFrom(2, enemy 1, 0); BossActor_ClearBit40(2). |

### Fight 26's count (FB8)

| Address | Name | Bytes | Shape | What it does |
|---|---|--:|---|---|
| `0x43C740` | `Boss26Fx_Dispatch` | 0x12 | kTask | BattleFx_Dispatch's slot 8 (the immediate at 0x4352F2), the task Boss26_Event creates with BattleTask_Create(0, 8): jmp [Boss26Fx_States + 4 * Sprite_Current +1], unchecked (ours aborts past 2); the entry gets the caller's stack. |
| `0x43C760` | `Boss26Fx_Wait` | 0x1B | kTask | Boss26Fx_States 0: with the byte 0x803433 and 0x904AE9 bit 1 clear, Sprite_Current +1 up by one. |
| `0x43C780` | `Boss26Fx_DrawCount` | 0x8F | kTask | Boss26Fx_States 1: BattleWin_DrawMediumBox(0x6B, 0x10); Text_DrawAt(0x7A, 0x12, 0, 4, [0x669D20]); Crt_sprintf(0x904D00, Boss26Fx_CountFormat, 0x15 - the turn counter 0x904B90); Text_DrawFont12(0x92, 0x12, 4, 0x904D00); Text_DrawAt(0xAA, 0x12, 0, 4, [0x669D24]); with 0x803433 or 0x904AE9 bit 1 +1 down by one; at phase 5 jmp BattleTask_FreeCurrent. Fight 26's turns-to-0x15 count (Boss26_Event sets 0x904AE8 bit 2 at turn 0x15). |

### Kind 33, the Gazer (K33)

| Address | Name | Bytes | Shape | What it does |
|---|---|--:|---|---|
| `0x43C810` | `BossGazer_Dispatch` | 0x12 | kDispatch `+1` below 12 | BossKind_Table[33] (Gazer, area 77): jmp [BossGazer_States + 4 * Sprite_Current +1], unchecked (ours aborts past 12); the entry gets the caller's stack. |
| `0x43C830` | `BossGazer_Enter` | 0x3D | kState, al | BossGazer_States 0: 0x939AD8's +0xFC = BossGazer_Anims, +0xF4 = BossGazer_Hook, +0xF8 = BossGazer_Sounds; Sprite_Current +1 = 2; jmp Sprite_ScriptTick (al the answer). |
| `0x43C870` | `BossGazer_State4Dispatch` | 0x12 | kDispatch `+2` below 3 | BossGazer_States 4 (the generic table's turn start 0x4365D0): jmp [BossGazer_State4Steps + 4 * +2], unchecked (ours aborts past 3). |
| `0x43C890` | `BossGazer_State4Start` | 0x3B | kState | BossGazer_State4Steps 0: +9 = the area's enemy data record (0x8C55C8 + 0x8C * the enemy's +0xF0) +0x8A; BattleEnemy_SetAnimation(2); +2 up by one. |
| `0x43C8D0` | `BossGazer_State4Wait` | 0x77 | kState | BossGazer_State4Steps 1: +9 down by one, at 0 0x437450(the enemy's +0xF8 first word); when BattleEnemy_ScriptTick answers, BattleTask_Create(3, 2) (the Gazer's effect task, F2) with +1 = 0 and owner +0x80 = Sprite_Current, +2 up by one (the slot unchecked: ours aborts on 0xFF). |
| `0x43C950` | `BossGazer_State5Dispatch` | 0x12 | kDispatch `+2` below 2 | BossGazer_States 5: jmp [BossGazer_State5Steps + 4 * +2], unchecked (ours aborts past 2). |
| `0x43C970` | `BossGazer_State5Start` | 0x24 | kState | BossGazer_State5Steps 0: Sprite_SetAnimation(2); 0x904B44 = 0x80; Battle_SetTargetFlag40(0x80); +2 up by one. |
| `0x43C9A0` | `BossGazer_State5Close` | 0x1B | kState | BossGazer_State5Steps 1 and BossMyria_State5Steps 1 (tools/boss_rows.py: K33 and K62): when BattleEnemy_ScriptTick answers, 0x904AA8 \|= 4, 0x4376F0, jmp 0x4376A0 (the action's end). |
| `0x43C9C0` | `BossGazer_Hook` | 0x10 | kEnemyHook | kind 33's +0xF4 hook: jmp [BossGazer_Hooks + 4 * (word & 0xFF)], unchecked (ours aborts past 3); the entry gets the caller's word. |

### Set-up 28 (B28)

| Address | Name | Bytes | Shape | What it does |
|---|---|--:|---|---|
| `0x43C9D0` | `Boss28_Setup` | 0x1F | kSetup | Boss_SetupTable[28] (BOSS028, area 77 row 7, kind 33 Gazer - tools/boss_rows.py --disc): stores BattleHook_End = Boss28_End, _Exit = BossHook_ExitClearActor0, _Event = BareRetZero; ret. |
| `0x43CA00` | `Boss28_End` | 0x1A | kEnd | set-up 28's end hook: with 0x904AE8 bit 1 (the win) movement-script variable 3 (0x903848) = 0x1E and jmp 0x446DE0, else jmp 0x446E00. |

### The Gazer's effect task (F2)

| Address | Name | Bytes | Shape | What it does |
|---|---|--:|---|---|
| `0x43CA20` | `BossGazerFx_Dispatch` | 0x12 | kTask | BattleBossFx_Dispatch's slot 2 (the immediate at 0x4357F2), created by BossGazer_State4Wait and BossGazerFx_BounceStart with BattleTask_Create(3, 2): jmp [BossGazerFx_States + 4 * Sprite_Current +1], unchecked (ours aborts past 2). |
| `0x43CA40` | `BossGazerFx_BounceDispatch` | 0x23 | kTask | BossGazerFx_States 0: call [BossGazerFx_BounceSteps + 4 * +2] (unchecked; ours aborts past 4), then with Sprite_Current bit 0 jmp Sprite_UpdateScreen. |
| `0x43CA70` | `BossGazerFx_BounceStart` | 0xD4 | kTask | BossGazerFx_BounceSteps 0: +0x24 / +0x2A / +0x48 = 0, +0x29 = 2, Sprite_SetAnimationBank(0x15C), Sprite_SetAnimation(4); from the owner 0x93B940 the word +0x36 + 7, the dword +0x38, the word +0x3E + 0x1000; +0x14 = 0, +0x20 = -0x20, +9 = 6; BattleTask_Create(3, 2) at +1 = 1 owned by this slot; +2 up by one. |
| `0x43CB50` | `BossGazerFx_Bounce` | 0xAA | kTask | BossGazerFx_BounceSteps 1: +0x3E += +0x14, +0x14 += +0x20; once the signed +0x3E reaches AreaMap_Elevation(+0x34, +0x38) + 0x200: +0x3E = the ground + 0x200, +0x14 = 0, +0x20 = 0x40, Sound_PlayById(0x600), then +9 down and +2 up by one, or at +9 0 +2 up by two. |
| `0x43CC00` | `BossGazerFx_BounceBack` | 0x89 | kTask | BossGazerFx_BounceSteps 2: the same move; once the signed +0x3E is above the ground + 0x400: +0x3E = the ground + 0x400, +0x14 = 0, +0x20 = -0x40, +2 down by one. |
| `0x43CC90` | `BossGazerFx_Leave` | 0x61 | kTask | BossGazerFx_BounceSteps 3: the same move; once the signed +0x3E is above the ground + 0x1000: the owner's +1 = 5 and +2 = 0 (the Gazer's state 5), jmp BattleTask_FreeCurrent. |
| `0x43CD00` | `BossGazerFx_MarkDispatch` | 0x23 | kTask | BossGazerFx_States 1: call [BossGazerFx_MarkSteps + 4 * +2] (unchecked; ours aborts past 3), then with Sprite_Current bit 0 jmp Sprite_UpdateScreen. |
| `0x43CD30` | `BossGazerFx_MarkStart` | 0x82 | kTask | BossGazerFx_MarkSteps 0: +0x24 / +0x2A / +0x48 = 0, +0x29 = 5, Sprite_SetAnimationBank(0x15C), Sprite_SetAnimation(6); the owner's +0x34 / +0x38; +0x3E = AreaMap_Elevation there; +2 up by one. |
| `0x43CDC0` | `BossGazerFx_MarkWait` | 0x17 | kTask, al | BossGazerFx_MarkSteps 1: with the owner's +0 bit 0 clear +2 up by one (BossGazerFx_MarkSteps 2 is BattleFx_FreeTask); jmp Sprite_ScriptTick (al the answer). |

### Kind 62, Myria (K62)

| Address | Name | Bytes | Shape | What it does |
|---|---|--:|---|---|
| `0x440080` | `BossMyria_Dispatch` | 0x12 | kDispatch `+1` below 12 | BossKind_Table[62] (Myria, area 198; its set-up 55): jmp [BossMyria_States + 4 * Sprite_Current +1], unchecked (ours aborts past 12); the entry gets the caller's stack. |
| `0x4400A0` | `BossMyria_Enter` | 0x9A | kState, al | BossMyria_States 0: the word 0x904B7E = 0; 0x939AD8's +0xFC = BossMyria_Anims, +0xF4 = BossMyria_Hook, +0xF8 = BossMyria_Sounds, +0x114 \|= 8, +0x29 = 6; BattleEnemy_SetAnimation(0); BossMyria_SpawnFx(0..2, 6); 0x455290(Sprite_Current, 0x64DD04); +1 = 2; jmp Sprite_ScriptTick (al the answer). |
| `0x440140` | `BossMyria_IdleDispatch` | 0x12 | kDispatch `+2` below 2 | BossMyria_States 2 (the generic idle's slot): jmp [BossMyria_IdleSteps + 4 * +2], unchecked (ours aborts past 2). |
| `0x440160` | `BossMyria_IdleStart` | 0x2A | kState | BossMyria_IdleSteps 0: +0xB = 1, the word 0x904B7E = 0, BattleEnemy_SetAnimation(0), BattleEnemy_ScriptTick (al not read), +2 up by one. |
| `0x440190` | `BossMyria_IdleEnd` | 0x26 | kState | BossMyria_IdleSteps 1: +0xB = 0, BattleEnemy_ScriptTick (al not read), +1 up by one, +2 = 0. |
| `0x4401C0` | `BossMyria_State4Dispatch` | 0x12 | kDispatch `+2` below 2 | BossMyria_States 4: jmp [BossMyria_State4Steps + 4 * +2], unchecked (ours aborts past 2). |
| `0x4401E0` | `BossMyria_State4Start` | 0x3F | kState | BossMyria_State4Steps 0: BossMyria_SpawnFxAndWait(4, 2, 2); +9 = the enemy data record's +0x8A; +2 up by one. |
| `0x440220` | `BossMyria_State4Wait` | 0x3A | kState | BossMyria_State4Steps 1: +9 down by one, at 0 0x437450(the enemy's +0xF8 first word); +0xB = 0. |
| `0x440260` | `BossMyria_State5Dispatch` | 0x12 | kDispatch `+2` below 2 | BossMyria_States 5: jmp [BossMyria_State5Steps + 4 * +2], unchecked (ours aborts past 2). |
| `0x440280` | `BossMyria_State5Start` | 0x17 | kState | BossMyria_State5Steps 0: Battle_SetTargetFlag40(0x904B44); +2 up by one. |
| `0x4402A0` | `BossMyria_ActDispatch` | 0x12 | kDispatch `+2` below 6 | BossMyria_States 6: jmp [BossMyria_ActSubs + 4 * +2], unchecked (ours aborts past 6). |
| `0x4402C0` | `BossMyria_ActPick` | 0x46 | kState | BossMyria_ActSubs 0: unless 0x904B35 is 4 with bit 2 clear in the ability record byte 0x65C4D8 + 24 * 0x904B80, BossMyria_SpawnFxAndWait(5, 4, 4); +2 = 1. |
| `0x440310` | `BossMyria_Death` | 0x57 | kState | BossMyria_ActSubs 4: Sprite_ScriptTickOnce, Battle_EnemyDefeated, 0x454A80(Sprite_Current); 0x939AD8's +0x110 \|= 0x1000; +0 &= 0xBF, +1 = 2, +2 = 0, +3 = 0. |
| `0x440370` | `BossMyria_State7Dispatch` | 0x12 | kDispatch `+2` below 3 | BossMyria_States 7: jmp [BossMyria_State7Steps + 4 * +2], unchecked (ours aborts past 3). |
| `0x440390` | `BossMyria_State7Start` | 0x104 | kState, al | BossMyria_State7Steps 0: nothing until File_LoadDone's eax is not 0; by 0x939AD8's +0x105 (4 in an event battle: Battle_LoadSoundByKey(kind + 0x20, 0x904B8D), answered 0x904B7E = 0 else +9 = 0x1E; not 4: +9 = 0); ability record byte 0x65C4DD + 24 * id bit 3: +1 up; else +9 = the enemy data record's +0x8B and BossMyria_SpawnFxAndWait / BattleEnemy_SetAnimation by the id (0x3A, 0x81, 0x82, other), +2 up; jmp BattleEnemy_ScriptTickOnce. |
| `0x4404A0` | `BossMyria_State7Count` | 0x4A | kState, al | BossMyria_State7Steps 1: +0xB = 0; +9 down to 0, then with 0x904B7E 3 or 5 Sound_PlayEffect(0x602) and +2 up by one; jmp BattleEnemy_ScriptTickOnce. |
| `0x4404F0` | `BossMyria_State7End` | 0x20 | kState | BossMyria_State7Steps 2: when BattleEnemy_ScriptTickOnce answers, +1 up by one, +2 = 0. |
| `0x440510` | `BossMyria_State8Dispatch` | 0x29 | kDispatch `+2` below 5 | BossMyria_States 8: call [BossMyria_State8Steps + 4 * +2] (unchecked; ours aborts past 5); then with +1 still 8 and +2 at most 2 jmp BossMyria_State8Check. |
| `0x440540` | `BossMyria_State8Tick` | 0xE | kState | BossMyria_State8Steps 0: BattleEnemy_ScriptTickOnce, +2 up by one. |
| `0x440550` | `BossMyria_State8Cost` | 0x3E | kState | BossMyria_State8Steps 1: BattleEnemy_ScriptTickOnce; with 0x939AD8's +0x105 4 its word +0xA6 -= the byte 0x904B88; 0x904AA9 \|= 8; +0x105 = 0; +2 up by one. |
| `0x440590` | `BossMyria_State8TickUnless` | 0x1D | kState, al | BossMyria_State8Steps 2: without bit 3 in the ability record byte 0x65C4DD + 24 * 0x904B80, jmp BattleEnemy_ScriptTickOnce. |
| `0x4405B0` | `BossMyria_State8Clear` | 0xA | kState | BossMyria_State8Steps 3: +0xB = 0. |
| `0x4405C0` | `BossMyria_State8Close` | 0x19 | kState | BossMyria_State8Steps 4: 0x4376A0, +1 = 2, +2 = 0. |
| `0x4405E0` | `BossMyria_State8Check` | 0x36 | kState | BossMyria_State8Dispatch's tail: with 0x904AA8 bit 2, the ability 0x904B80 at 0x81: +0xB = 1, the word 0x904B7E = 8, +2 = 3; any other: jmp 0x4376A0. |
| `0x440620` | `BossMyria_Hook` | 0x10 | kEnemyHook | kind 62's +0xF4 hook: jmp [BossMyria_Hooks + 4 * (word & 0xFF)], unchecked (ours aborts past 3); the entry gets the caller's word. |
| `0x440630` | `BossMyria_SpawnFxAndWait` | 0x29 | kCallee | cdecl, three words: BossMyria_SpawnFx(state, part); Sprite_Current +0xB = 1; the word 0x904B7E = wait's low byte. |
| `0x440660` | `BossMyria_SpawnFx` | 0x77 | kCallee | cdecl, two words: BattleTask_Create(3, 5) (BattleBossFx_Dispatch's slot 5, F5); the slot's first 0x80 bytes from enemy 0's object, +1 = state, +2 = 0, +6 = 3, +5 = 5, +9 = 0, +0x80 = enemy 0, +0x29 = part (the slot unchecked: ours aborts on 0xFF). |

### Named data (`symbols.toml` `[[data]]`)

| Address | Name | Count | What |
|---|---|--:|---|
| `0x64D3E4` | `Boss26Fx_States` | 2 | Boss26Fx_Dispatch's table by +1: Boss26Fx_Wait, Boss26Fx_DrawCount. |
| `0x64D3EC` | `Boss26Fx_CountFormat` | 4 | Boss26Fx_DrawCount's Crt_sprintf format (a two-digit number). |
| `0x64D3F0` | `BossGazer_Anims` | 12 | kind 33's +0xFC animation bytes (BossGazer_Enter; read by BattleEnemy_SetAnimation). |
| `0x64D3FC` | `BossGazer_Sounds` | 4 | kind 33's +0xF8 sound words (0x437450 plays the first; 0xFFFF none). |
| `0x64D404` | `BossGazer_States` | 12 | BossGazer_Dispatch's table by +1: BossGazer_Enter, Port_DroppedCall, the generic 2 and 3, BossGazer_State4Dispatch, BossGazer_State5Dispatch, the generic 6..9, Port_DroppedCall, EnemyOp_HitPose. |
| `0x64D434` | `BossGazer_State4Steps` | 3 | BossGazer_State4Dispatch's table by +2: BossGazer_State4Start, BossGazer_State4Wait, BareRet. |
| `0x64D440` | `BossGazer_State5Steps` | 2 | BossGazer_State5Dispatch's table by +2: BossGazer_State5Start, BossGazer_State5Close. |
| `0x64D448` | `BossGazer_Hooks` | 3 | BossGazer_Hook's table: three BareRet. |
| `0x64D454` | `BossGazerFx_States` | 2 | BossGazerFx_Dispatch's table by +1: BossGazerFx_BounceDispatch, BossGazerFx_MarkDispatch. |
| `0x64D45C` | `BossGazerFx_BounceSteps` | 4 | BossGazerFx_BounceDispatch's call table by +2: BossGazerFx_BounceStart, _Bounce, _BounceBack, _Leave. |
| `0x64D46C` | `BossGazerFx_MarkSteps` | 3 | BossGazerFx_MarkDispatch's call table by +2: BossGazerFx_MarkStart, BossGazerFx_MarkWait, BattleFx_FreeTask (the third reached by MarkWait's +2 up; tools/boss_rows.py's '3 code entries'). |
| `0x64DCC8` | `BossMyria_Anims` | 12 | kind 62's +0xFC animation bytes (BossMyria_Enter; read by BattleEnemy_SetAnimation). |
| `0x64DCD4` | `BossMyria_Sounds` | 4 | kind 62's +0xF8 sound words (0x437450 plays the first; 0xFFFF none). |
| `0x64DD44` | `BossMyria_States` | 12 | BossMyria_Dispatch's table by +1: BossMyria_Enter, Port_DroppedCall, BossMyria_IdleDispatch, EnemyOp_Wait, BossMyria_State4Dispatch, _State5Dispatch, _ActDispatch, _State7Dispatch, _State8Dispatch, the generic 9, Port_DroppedCall, EnemyOp_HitPose. |
| `0x64DD74` | `BossMyria_IdleSteps` | 2 | BossMyria_IdleDispatch's table by +2: BossMyria_IdleStart, BossMyria_IdleEnd. |
| `0x64DD7C` | `BossMyria_State4Steps` | 2 | BossMyria_State4Dispatch's table by +2: BossMyria_State4Start, BossMyria_State4Wait. |
| `0x64DD84` | `BossMyria_State5Steps` | 2 | BossMyria_State5Dispatch's table by +2: BossMyria_State5Start, BossGazer_State5Close. |
| `0x64DD8C` | `BossMyria_ActSubs` | 6 | BossMyria_ActDispatch's table by +2: EnemyOp_ActSubs with BossMyria_ActPick at 0 and BossMyria_Death at 4. |
| `0x64DDA4` | `BossMyria_State7Steps` | 3 | BossMyria_State7Dispatch's table by +2: BossMyria_State7Start, _State7Count, _State7End. |
| `0x64DDB0` | `BossMyria_State8Steps` | 5 | BossMyria_State8Dispatch's call table by +2: BossMyria_State8Tick, _State8Cost, _State8TickUnless, _State8Clear, _State8Close. |
| `0x64DDC4` | `BossMyria_Hooks` | 3 | BossMyria_Hook's table: three BareRet. |

## 3. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original indexes past a table (the twelve `jmp` / `call`
dispatchers by a state byte, the two hooks by the word's low byte) or writes
past the task pool (the three `BattleTask_Create` callers on its 0xFF, all
48 slots taken), ours aborts with a `Fatal` naming the function (the owner's
rule, round9 doc section 6); nothing reaches it. Three differences no caller
can see:

- `Boss27_Event` keeps the actor byte it reads in its own argument slot
  (`mov [esp + 4], al` at `0x43C56F`, the compiler reusing the parameter as a
  local); no caller reads its pushed word back (every caller is ours and
  pops it).
- The `jmp` dispatchers take the caller's stack word and hand it to the
  entry, returning the entry's eax, as the original's `jmp` leaves both in
  place (wave one's lesson 1); the `call` dispatchers (`0x43CA40`,
  `0x43CD00`, `0x440510`) call their entries with no word, as the original
  does (their entries read none).
- `BossMyria_State8TickUnless` answers the original's eax on both paths:
  `BattleEnemy_ScriptTickOnce`'s, or `3 * id` (its index arithmetic, left in
  eax); nothing reads it.

## 4. The fuzz

`BOF3X_SHADOW=boss_sf`, `src/game/boss_sf_fuzz.cpp`: six `Run`s
(`BOF3X_BSF_RUN=B27|FB8|K33|B28|F2|K62` runs one), 6,000 rounds a function,
the harness unedited.

| Run | Fight, kind | Clones and shapes | Tables (`DataTable`) | Rounds | Result (this worktree) |
|---|---|---|---|--:|---|
| `B27` | 27 | `kSetup`, `kEvent`, `kEnd`, `kExit` | - | 24,000 | 40,855 calls, 0 mismatches |
| `FB8` | 26 | 3 `kTask` (the dispatcher's `+1` seeded below 2) | `Boss26Fx_States` | 18,000 | 37,999 calls, 0 mismatches |
| `K33` | 28, 33 | 3 dispatchers `kDispatch` (states 12 / 3 / 2), 5 states, the hook `kEnemyHook` | `BossGazer_Hooks` (1 word, first), `_States`, `_State4Steps`, `_State5Steps` | 54,000 | 73,058 calls, 0 mismatches |
| `B28` | 28 | `kSetup`, `kEnd` | - | 12,000 | 6,000 calls, 0 mismatches |
| `F2` | 28 | 9 `kTask` (the three dispatchers' bytes seeded inside their tables) | `BossGazerFx_States`, `_BounceSteps`, `_MarkSteps` | 54,000 | 93,198 calls, 0 mismatches |
| `K62` | 55, 62 | 7 dispatchers (states 12 / 2 / 2 / 2 / 6 / 3 / 5), 16 states, the hook, 2 `kCallee` (the spawn helpers) | `BossMyria_Hooks` (1 word, first), the `+1` table and six sub-tables | 162,000 | 209,905 calls, 0 mismatches |

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the final
build): exit 0 first time (no silent death), 602 self-test lines, 0
mismatches, BSF's six `Run`s among them.

**First user of `kTask`.** The shape needed nothing more: `Sprite_Current` a
task slot, `0x93B8C4` the same two times in three, the owner `0x93B940` a
harness record or a slot. What the group adds in its own file (a candidate
harness default, nothing folded back is needed): a `kTask` dispatcher has no
`Clone::states` draw (that is `kDispatch`'s), so the seed puts the state
bytes inside the tables itself.

**Beyond the standard set** (`kCallees`): `Port_DroppedCall` with no words
(BSA's lesson); `Sprite_PoseFromSet` (`{0xFF, all, all}`, BSE's) and
`Battle_RemoveFromTurnOrder` with BSE's louder effect (a member's `+0x91` /
`+8` / `+0` moved); **four callees louder than the real ones for set-up 27**:
`Sprite_PoseFromSet`, `Sprite_EnsureAnimation`, `BossActor_CopyFrom` and
`BossActor_ClearBit40` flip a bit of the script byte `0x904AAD` (which
`Boss27_Event`, `_End` and `_Exit` read again after them; the last keeps the
standard stand-in's field-object flip); `Crt_sprintf` with the three words FB8
pushes (the standard four would log the original's stack above them - it
differs between the copy and ours); `AreaMap_Elevation` answering, a third of
the time, the running sprite's `+0x3E` less 0x200 / 0x400 / 0x1000, give or
take one (the Gazer effect's three compares' boundaries), garbage in the high
word; `Battle_LoadSoundByKey` reading its two bytes (the original pushes
registers with leftovers above them); the five engine functions nobody owns
with their arities; Myria's spawn helpers (`{0xFF, 0xFF}`, `{0xFF, 0xFF,
0xFF}`) and state 8's check (`kPhase`), called directly. Regions: the window
record byte `0x803433` (`0x803430`, 4) and movement-script variable 3.

**Seeds.** Every kind dispatcher's other state bytes inside its table
(`OtherStates`); the enemy's `+0xF0` (its enemy data record) 0..7 two times in
three; `+9` at 0, 1, 2, 6, 0x1E, 0x80, 0xFF. Set-up 27: the battle-end byte 0
half the time, else single bits (0 and 2) or others; `0x904AAD` bits 0..5
singly and paired, bit 3 clear half the time; the two enemies' `+0x92` bit
0x4000 clear five times in six; the turn cursor 0..11 (any a third) and the
actor before it a member two times in three; the members' `+0x130` low bits
clear half the time; the event code 0 six times and 2 three times in
seventeen, garbage above the byte half the time. FB8: the window byte and
`0x904AE9` bit 1 each clear half the time, phase 5 a third, the turn counter
0..0x17. F2: the height `+0x3E` at 0, 0x7FFF, 0x8000, 0xFFFF and the three
spans; the velocity dwords at their stores' values and the signed extremes;
the owner's bit 0 either way. Myria: the ability id 0x3A / 0x81 / 0x82 three
times in eight, else ids whose record bit 2 (`0x65C4D8`) or bit 3
(`0x65C4DD`) is set or clear as a coin says (read in place), else any; the
acting kind 4 half the time; the wait word 0, 3, 5, 8 and neighbours; round
flag bit 2 half the time; the enemy's `+0x105` 4 half the time; the fight
byte 0 a sixth of the time (state 7's event-battle test); state 8's `+1` at 8
two times in three. **`disturb`** moves what the 54 read again after a call:
`0x904AAD`, a member's `+0x91` bit 6, the ability id, the acting kind,
Myria's wait word, the window byte, `0x904AE9` bit 1, the current enemy's
`+0x105`, an enemy data record's `+0x8A` / `+0x8B`.

Coverage (the originals' calls, this worktree): `B27` `Sprite_PoseFromSet`
and `Battle_RemoveFromTurnOrder` 9,000 each, `BossActor_ClearBit40` 12,000,
`BossActor_CopyFrom` 4,600, `Sprite_EnsureAnimation` 366, `0x446E20` 6,000;
`FB8` the five draws 30,000, `BattleTask_FreeCurrent` 1,968, both states;
`K33` every entry of the four tables, `BattleTask_Create` 4,007,
`0x437450` 1,071, `0x4376A0` / `0x4376F0` 3,907; `F2` `AreaMap_Elevation`
29,000, `BattleTask_Create` 6,000, `BattleTask_FreeCurrent` 1,936,
`BattleFx_FreeTask` 2,023, `Sound_PlayById` 2,400, every step; `K62`
`BossMyria_SpawnFx` 24,000, `_SpawnFxAndWait` 14,036, `_State8Check` 2,275,
`Battle_LoadSoundByKey` 1,981, `Sound_PlayEffect` 75, every entry of the
eight tables.

## 5. Controls

`controls.py` (the group's scratch script): each control one textual change to ours (or two made together, F23 and F158), anchored on a string that occurs once in `boss_sf.cpp`, then rebuild, run the one `Run` (`BOF3X_BSF_RUN`) that fuzzes the function, restore, rebuild. Run twice; the table is the second run, on the final seeds. **197 planted, 197 refused, every one by a count** (the rounds column is the planted function's mismatched rounds; a plant in a shared helper - `Move`, `Height`, `EnemyDataByte`, `AbilityByte` - is refused in every function that calls it, listed after). No control went unrefused and none was refused only by a `Fatal`. The thinnest are F164 (`Sprite_Current` not read after `Sound_PlayEffect` in `BossMyria_State7Count`: 4 rounds - the sound is reached only with `+9` at 0 and the wait word at 3 or 5, and the disturbance must then move `Sprite_Current`), F158 (19), F165 (37), F5 (40) and F14 (49): Boss27_Event's deep path and Myria's state 7 are narrow; the first pass had F6 at 9 rounds and F14 at 3 before the set-up 27 seeds were sharpened (the battle-end byte 0 half the time, a member actor two times in three).

| # | Function | Plant | Refused |
|---|---|---|---|
| F1 | `Boss27_Setup` | the exit hook the end hook | 6000 rounds |
| F2 | `Boss27_Event` | enemy 0's bit 0x8000 | 1140 rounds |
| F3 | `Boss27_Event` | enemy 1 sets bit 2 | 317 rounds |
| F4 | `Boss27_Event` | 0x904AE8 = 5 | 279 rounds |
| F5 | `Boss27_Event` | an enemy actor counts | 40 rounds |
| F6 | `Boss27_Event` | +0x130 bit 0 only | 66 rounds |
| F7 | `Boss27_Event` | the actor at the cursor | 255 rounds |
| F8 | `Boss27_Event` | bit 3 kept | 163 rounds |
| F9 | `Boss27_Event` | 0x904AAD not read again after the call | 284 rounds |
| F10 | `Boss27_Event` | enemy 0 animated | 324 rounds |
| F11 | `Boss27_Event` | animation 0xD | 331 rounds |
| F12 | `Boss27_Event` | code 3 for 2 | 428 rounds |
| F13 | `Boss27_Event` | al 1 for other codes | 2377 rounds |
| F14 | `Boss27_Event` | 0x904AE8 bit 0 ignored | 49 rounds |
| F15 | `Boss27_Event` | bit 4 for 3 | 135 rounds |
| F16 | `Boss27_End` | step 0xE | 751 rounds |
| F17 | `Boss27_End` | step 7 by bit 3 | 1602 rounds |
| F18 | `Boss27_End` | bits 1 and 2 in the other order | 1102 rounds |
| F19 | `Boss27_End` | pose + 0x1B | 3428 rounds |
| F20 | `Boss27_End` | +0x90 read before the call | 983 rounds |
| F21 | `Boss27_End` | two members | 2953 rounds |
| F22 | `Boss27_End` | step 2's way out | 6000 rounds |
| F23 | `Boss27_End` | 0x904AAD read before the loop | 1877 rounds |
| F24 | `Boss27_Exit` | the place for the pose | 2215 rounds |
| F25 | `Boss27_Exit` | actor 2 from enemy 0 | 2437 rounds |
| F26 | `Boss27_Exit` | actor 1 twice | 6000 rounds |
| F27 | `Boss27_Exit` | bit 0 for 1 | 2626 rounds |
| F28 | `Boss27_Exit` | 0x904AAD read once | 923 rounds |
| F29 | `Boss26Fx_Dispatch` | the next entry | 6000 rounds |
| F30 | `Boss26Fx_Wait` | 0x904AE9 bit 0 | 1511 rounds |
| F31 | `Boss26Fx_Wait` | +1 up by two | 1525 rounds |
| F32 | `Boss26Fx_DrawCount` | 0x14 - the turn | 6000 rounds |
| F33 | `Boss26Fx_DrawCount` | the box at y 0x11 | 6000 rounds |
| F34 | `Boss26Fx_DrawCount` | the first text twice | 6000 rounds |
| F35 | `Boss26Fx_DrawCount` | the count at x 0x93 | 6000 rounds |
| F36 | `Boss26Fx_DrawCount` | freed at phase 4 | 2400 rounds |
| F37 | `Boss26Fx_DrawCount` | both conditions | 3051 rounds |
| F38 | `BossGazer_Dispatch` | the next entry | 6000 rounds |
| F39 | `BossGazer_Enter` | Myria's hook installed | 6000 rounds |
| F40 | `BossGazer_Enter` | +0xFC the sounds | 6000 rounds |
| F41 | `BossGazer_Enter` | state 3 | 5967 rounds |
| F42 | `BossGazer_Enter` | Sprite_ScriptTickOnce | 6000 rounds |
| F43 | `BossGazer_State4Dispatch` | the next entry | 6000 rounds |
| F44 | `BossGazer_State4Start` | the record's +0x8B | 3981 rounds |
| F45 | `BossGazer_State4Start` | animation 3 | 6000 rounds |
| F46 | `BossGazer_State4Start` | +9 stored after the call | 427 rounds |
| F47 | `BossGazer_State4Wait` | the sound at 1 | 1579 rounds |
| F48 | `BossGazer_State4Wait` | task slot 3 | 3998 rounds |
| F49 | `BossGazer_State4Wait` | the task at state 1 | 3998 rounds |
| F50 | `BossGazer_State4Wait` | Sprite_Current read before the create | 152 rounds |
| F51 | `BossGazer_State4Wait` | the second sound word (EnemySound) | 1077 rounds |
| F52 | `BossGazer_State5Dispatch` | the next entry | 6000 rounds |
| F53 | `BossGazer_State5Start` | target 0x81 | 5751 rounds |
| F54 | `BossGazer_State5Start` | flag call with 0x40 | 6000 rounds |
| F55 | `BossGazer_State5Start` | animation 3 | 6000 rounds |
| F56 | `BossGazer_State5Close` | round flag bit 3 | 2740 rounds |
| F57 | `BossGazer_State5Close` | the two calls swapped | 3967 rounds |
| F58 | `BossGazer_State5Close` | the flag after 0x4376F0 | 69 rounds |
| F59 | `BossGazer_Hook` | the word's second byte flipped | 6000 rounds |
| F60 | `BossGazer_State4Wait` | slot stride 0x80 (TaskSlot) | 3920 rounds |
| F61 | `Boss28_Setup` | event BareRet | 6000 rounds |
| F62 | `Boss28_End` | variable 3 = 0x1F | 3005 rounds |
| F63 | `Boss28_End` | the win by bit 0 | 3749 rounds |
| F64 | `Boss28_End` | step 3 for the win | 3005 rounds |
| F65 | `BossGazerFx_Dispatch` | the next entry | 6000 rounds |
| F66 | `BossGazerFx_BounceDispatch` | the next entry | 6000 rounds |
| F67 | `BossGazerFx_BounceDispatch` | bit 1 for the draw | 2953 rounds |
| F68 | `BossGazerFx_BounceDispatch` | Sprite_Current read before the step | 102 rounds |
| F69 | `BossGazerFx_BounceStart` | +0x29 = 3 | 6000 rounds |
| F70 | `BossGazerFx_BounceStart` | x + 8 | 6000 rounds |
| F71 | `BossGazerFx_BounceStart` | height + 0x800 | 6000 rounds |
| F72 | `BossGazerFx_BounceStart` | velocity step -0x40 | 6000 rounds |
| F73 | `BossGazerFx_BounceStart` | five bounces | 5979 rounds |
| F74 | `BossGazerFx_BounceStart` | the second task at state 0 | 6000 rounds |
| F75 | `BossGazerFx_BounceStart` | bank 0x15D | 6000 rounds |
| F76 | `BossGazerFx_BounceStart` | Sprite_Current read before the create | 198 rounds |
| F77 | `BossGazerFx_BounceStart` | +0x38 from the owner's +0x34 | 6000 rounds |
| F78 | `BossGazerFx_Bounce` | ground + 0x201 | 242 rounds |
| F79 | `BossGazerFx_Bounce` | > for >= | 242 rounds |
| F80 | `BossGazerFx_Bounce` | velocity step 0x41 | 2351 rounds |
| F81 | `BossGazerFx_Bounce` | sound 0x601 | 2351 rounds |
| F82 | `BossGazerFx_Bounce` | +2 up by three at the end | 176 rounds |
| F83 | `BossGazerFx_Bounce` | Sprite_Current read before the sound | 78 rounds |
| F84 | `BossGazerFx_Bounce` | set to the ground + 0x1FF | 2351 rounds |
| F85 | `BossGazerFx_Bounce` | the ground unsigned (Move) | 2460 rounds (also BossGazerFx_BounceBack 2010, BossGazerFx_Leave 1544) |
| F86 | `BossGazerFx_Bounce` | velocity before position (Move) | 4408 rounds (also BossGazerFx_BounceBack 4048, BossGazerFx_Leave 5407) |
| F87 | `BossGazerFx_Bounce` | the height unsigned (Height) | 1645 rounds (also BossGazerFx_BounceBack 2000, BossGazerFx_Leave 2311) |
| F88 | `BossGazerFx_Bounce` | x and z swapped in the second ask | 2351 rounds |
| F89 | `BossGazerFx_BounceBack` | ground + 0x3FF | 244 rounds |
| F90 | `BossGazerFx_BounceBack` | < for <= | 244 rounds |
| F91 | `BossGazerFx_BounceBack` | velocity step -0x20 | 2782 rounds |
| F92 | `BossGazerFx_BounceBack` | +2 down by two | 2782 rounds |
| F93 | `BossGazerFx_BounceBack` | set to the ground + 0x200 | 2782 rounds |
| F94 | `BossGazerFx_Leave` | ground + 0xFFF | 176 rounds |
| F95 | `BossGazerFx_Leave` | the owner's state 4 | 1983 rounds |
| F96 | `BossGazerFx_Leave` | the owner's +2 = 1 | 1982 rounds |
| F97 | `BossGazerFx_Leave` | its own +1 for the owner's | 1762 rounds |
| F98 | `BossGazerFx_Leave` | < for <= | 176 rounds |
| F99 | `BossGazerFx_MarkDispatch` | the next entry | 6000 rounds |
| F100 | `BossGazerFx_MarkDispatch` | Sprite_Current read before the step | 103 rounds |
| F101 | `BossGazerFx_MarkStart` | +0x29 = 6 | 6000 rounds |
| F102 | `BossGazerFx_MarkStart` | animation 7 | 6000 rounds |
| F103 | `BossGazerFx_MarkStart` | +0x38 from the owner's +0x34 | 6000 rounds |
| F104 | `BossGazerFx_MarkStart` | the ground + 1 | 6000 rounds |
| F105 | `BossGazerFx_MarkStart` | +0x49 for +0x48 | 6000 rounds |
| F106 | `BossGazerFx_MarkWait` | the owner's bit 1 | 3053 rounds |
| F107 | `BossGazerFx_MarkWait` | Sprite_ScriptTickOnce | 6000 rounds |
| F108 | `BossMyria_Dispatch` | the next entry | 6000 rounds |
| F109 | `BossMyria_Enter` | +0x114 bit 4 | 4533 rounds |
| F110 | `BossMyria_Enter` | +0x29 = 7 | 5996 rounds |
| F111 | `BossMyria_Enter` | the third spawn at state 3 | 6000 rounds |
| F112 | `BossMyria_Enter` | the first spawn's part 5 | 6000 rounds |
| F113 | `BossMyria_Enter` | Sprite_Current read before the calls | 839 rounds |
| F114 | `BossMyria_Enter` | the script + 4 | 6000 rounds |
| F115 | `BossMyria_Enter` | the Gazer's hook installed | 6000 rounds |
| F116 | `BossMyria_Enter` | the wait word 1 | 5844 rounds |
| F117 | `BossMyria_IdleDispatch` | the next entry | 6000 rounds |
| F118 | `BossMyria_IdleStart` | +0xB = 2 | 5941 rounds |
| F119 | `BossMyria_IdleStart` | animation 1 | 6000 rounds |
| F120 | `BossMyria_IdleStart` | Sprite_Current read before the tick | 207 rounds |
| F121 | `BossMyria_IdleEnd` | +1 up by two | 6000 rounds |
| F122 | `BossMyria_IdleEnd` | +0xB = 1 | 5968 rounds |
| F123 | `BossMyria_State4Dispatch` | the next entry | 6000 rounds |
| F124 | `BossMyria_State4Start` | wait 3 | 6000 rounds |
| F125 | `BossMyria_State4Start` | the record's +0x8B | 3828 rounds |
| F126 | `BossMyria_State4Start` | record stride 0x8B (EnemyDataByte) | 3348 rounds (also BossMyria_State7Start 2262) |
| F127 | `BossMyria_State4Wait` | +0xB = 1 | 6000 rounds |
| F128 | `BossMyria_State4Wait` | tested before the count | 1562 rounds |
| F129 | `BossMyria_State5Dispatch` | the next entry | 6000 rounds |
| F130 | `BossMyria_State5Start` | the byte after the target | 5974 rounds |
| F131 | `BossMyria_State5Start` | +2 = 1 | 5981 rounds |
| F132 | `BossMyria_ActDispatch` | the next entry | 6000 rounds |
| F133 | `BossMyria_ActPick` | kind 3 | 2352 rounds |
| F134 | `BossMyria_ActPick` | bit 3 | 1664 rounds |
| F135 | `BossMyria_ActPick` | wait 5 | 3952 rounds |
| F136 | `BossMyria_ActPick` | +2 = 2 | 6000 rounds |
| F137 | `BossMyria_ActPick` | record stride 23 (AbilityByte) | 1433 rounds (also BossMyria_State7Start 1131, BossMyria_State8TickUnless 1385) |
| F138 | `BossMyria_Death` | +0x110 bit 11 | 4501 rounds |
| F139 | `BossMyria_Death` | +1 = 3 | 6000 rounds |
| F140 | `BossMyria_Death` | +3 = 1 | 6000 rounds |
| F141 | `BossMyria_Death` | the current enemy released | 2304 rounds |
| F142 | `BossMyria_Death` | the two calls swapped | 6000 rounds |
| F143 | `BossMyria_State7Dispatch` | the next entry | 6000 rounds |
| F144 | `BossMyria_State7Start` | File_LoadDone's al only | 1007 rounds |
| F145 | `BossMyria_State7Start` | +0x105 at 5 | 2517 rounds |
| F146 | `BossMyria_State7Start` | kind + 0x21 | 2068 rounds |
| F147 | `BossMyria_State7Start` | the set 0x904B8C | 2062 rounds |
| F148 | `BossMyria_State7Start` | the answer inverted | 1901 rounds |
| F149 | `BossMyria_State7Start` | +9 = 0x1F | 153 rounds |
| F150 | `BossMyria_State7Start` | +9 = 1 when not 4 | 456 rounds |
| F151 | `BossMyria_State7Start` | the event-battle test dropped | 405 rounds |
| F152 | `BossMyria_State7Start` | bit 2 for 3 | 2374 rounds |
| F153 | `BossMyria_State7Start` | the record's +0x8A | 2599 rounds |
| F154 | `BossMyria_State7Start` | id 0x3B for 0x3A | 641 rounds |
| F155 | `BossMyria_State7Start` | 0x81's wait 7 | 639 rounds |
| F156 | `BossMyria_State7Start` | 0x82's animation 0xA | 646 rounds |
| F157 | `BossMyria_State7Start` | the default's wait 4 | 2179 rounds |
| F158 | `BossMyria_State7Start` | the id read before the calls | 19 rounds |
| F159 | `BossMyria_State7Start` | BattleEnemy_ScriptTick for Once | 4094 rounds |
| F160 | `BossMyria_State7Start` | al 1 while loading | 978 rounds |
| F161 | `BossMyria_State7Count` | +0xB = 1 | 5961 rounds |
| F162 | `BossMyria_State7Count` | wait 4 for 5 | 78 rounds |
| F163 | `BossMyria_State7Count` | sound 0x603 | 77 rounds |
| F164 | `BossMyria_State7Count` | Sprite_Current not read after the sound | 4 rounds |
| F165 | `BossMyria_State7Count` | the wait's low byte | 37 rounds |
| F166 | `BossMyria_State7End` | +1 up by two | 4036 rounds |
| F167 | `BossMyria_State7End` | +2 = 1 | 4036 rounds |
| F168 | `BossMyria_State8Dispatch` | the next entry | 6000 rounds |
| F169 | `BossMyria_State8Dispatch` | +1 at 7 | 2230 rounds |
| F170 | `BossMyria_State8Dispatch` | +2 at most 1 | 746 rounds |
| F171 | `BossMyria_State8Dispatch` | Sprite_Current read before the step | 98 rounds |
| F172 | `BossMyria_State8Tick` | +2 up by two | 6000 rounds |
| F173 | `BossMyria_State8Cost` | the cost from 0x904B89 | 2901 rounds |
| F174 | `BossMyria_State8Cost` | HP +0xA4 for +0xA6 | 2897 rounds |
| F175 | `BossMyria_State8Cost` | 0x904AA9 bit 2 | 4464 rounds |
| F176 | `BossMyria_State8Cost` | 0x939AD8 read before the tick | 210 rounds |
| F177 | `BossMyria_State8Cost` | +0x105 = 1 | 6000 rounds |
| F178 | `BossMyria_State8TickUnless` | bit 2 | 2867 rounds |
| F179 | `BossMyria_State8TickUnless` | al 2 * id | 1179 rounds |
| F180 | `BossMyria_State8Clear` | +0xC | 6000 rounds |
| F181 | `BossMyria_State8Close` | +1 = 3 | 6000 rounds |
| F182 | `BossMyria_State8Close` | the states set before the call | 265 rounds |
| F183 | `BossMyria_State8Check` | round flag bit 3 | 3037 rounds |
| F184 | `BossMyria_State8Check` | id 0x82 | 771 rounds |
| F185 | `BossMyria_State8Check` | wait 9 | 382 rounds |
| F186 | `BossMyria_State8Check` | +2 = 2 | 382 rounds |
| F187 | `BossMyria_Hook` | the word's second byte flipped | 6000 rounds |
| F188 | `BossMyria_SpawnFxAndWait` | the whole wait word | 2982 rounds |
| F189 | `BossMyria_SpawnFxAndWait` | +0xC | 6000 rounds |
| F190 | `BossMyria_SpawnFxAndWait` | the words swapped | 5773 rounds |
| F191 | `BossMyria_SpawnFx` | slot 6's task | 6000 rounds |
| F192 | `BossMyria_SpawnFx` | 0x7C bytes copied | 6000 rounds |
| F193 | `BossMyria_SpawnFx` | +6 = 2 | 6000 rounds |
| F194 | `BossMyria_SpawnFx` | +9 = 1 | 6000 rounds |
| F195 | `BossMyria_SpawnFx` | owned by enemy 1 | 6000 rounds |
| F196 | `BossMyria_SpawnFx` | +0x29 the state | 5788 rounds |
| F197 | `BossMyria_SpawnFx` | +1 set before the copy | 5976 rounds |

## 6. Latent defects (Capcom's, kept)

- **The three task spawns trust `BattleTask_Create`.** `BossGazer_State4Wait`,
  `BossGazerFx_BounceStart` and `BossMyria_SpawnFx` write the slot it
  answers without a test; with all 48 slots taken it answers 0xFF and the
  original writes at `0x93A000 + 0xFF * 0x84` = `0x9423FC`, past the pool
  (`BossMyria_SpawnFx` copies 0x80 bytes there). Myria's entrance creates
  three at once and every action pick one more. Ours aborts with a `Fatal`;
  whether a fight can fill the pool is not measured (BSA's
  `BossWeretigr_State4Fx` is the same defect).
- **`BossMyria_SpawnFx` copies enemy 0's object and names enemy 0 the owner**,
  whichever enemy runs it. Kept; in the game Myria is presumably enemy 0
  (area 198's row 7 - not measured).
- **The ability records are indexed by the whole word `0x904B80`** (`0x65C4D8
  + 24 * id` and `0x65C4DD + 24 * id`, `BossMyria_ActPick`, `_State7Start`,
  `_State8TickUnless`, `_State8Check`): an id past the table reads whatever
  `.data` follows. The ids come from the battle's action; kept.
- **`Boss26Fx_DrawCount` prints `0x15 - turn` unclamped**: past turn 0x15 the
  count goes negative. `Boss26_Event` (BSE's) sets `0x904AE8` bit 2 at turn
  0x15, which presumably ends the fight before that (not measured).
- **`Boss27_Event` reads the actor at `0x904ACB + 0x904AE2` unchecked** (the
  turn order one before its cursor); a cursor past the order reads the
  battle bytes after it. Only an actor 0..2 is acted on.
- **The dispatchers index unchecked** (twelve by a state byte, two hooks by
  the word's low byte). Ours aborts.
- **`BossMyria_State4Wait` never moves Myria on**: its step counts `+9` down
  and plays the sound at 0, and nothing in it changes `+1` or `+2`. What
  moves Myria out of state 4 is outside the group (the slot-5 tasks
  `BossMyria_SpawnFx` creates are group BSJ's, and name enemy 0 their
  owner); not a defect in itself, noted for BSJ's reading.

## 7. What nothing reached

Every function and every branch the controls planted in was reached. What
only a fight would show: which fight set-up 27 is (the tool leaves it open;
its hooks name no fight byte), the count's look and its texts (the two
`.data` text pointers were not followed), the Gazer effect's bounce as
seen, whether the pool can fill (section 6), and what the slot-5 tasks
Myria's entrance and actions create do (group BSJ's F5).

## 8. Calls across groups

**Out of the group, to code nobody owns** (raw, `boss_sf_callees.h`):
`0x437450` (an enemy state's sound, one word), `0x4376A0` (an enemy action's
end), `0x4376F0` (the round-flag bit 7 chance), `0x454A80` (the Field_Slots
release), `0x455290` (a Field_Slots record started), and the standard set's
`0x446DE0` / `0x446E00` / `0x446E20`. No other group's function is called;
BH's are called by name (`BossActor_CopyFrom`, `BossActor_ClearBit40`) or
stored as literals (`BossHook_ExitClearActor0`, `BareRetZero`), and
`BareRet` sits in the kinds' tables.

**Tasks created for another group**: `BossMyria_SpawnFx` creates
`BattleBossFx_Dispatch`'s slot-5 task (F5, group BSJ's) with
`BattleTask_Create(3, 5)` - no call into BSJ's code, but BSJ's F5 reads what
this seeds (`+1` the state 0..6, `+0x29` the part, the copy of enemy 0).

**Into the group from outside** (for the rebinding pass - our code naming
these by raw address): `0x43C740` (`Boss26Fx_Dispatch`) in
`battle_fx_tasks.cpp`'s `BattleFx_Dispatch` stack table (`H(0x43C740)`),
`battle_fx_tasks_callees.h`'s comment and `battle_fx_tasks_fuzz.cpp`'s call
site `{0x52, 0x43C740}`. `0x43CA20` (`BossGazerFx_Dispatch`) is held by
`BattleBossFx_Dispatch` `0x4357D0` (named, not ours). The two set-ups and
the two kinds are reached through `Boss_SetupTable` / `BossKind_Table`
(`.data`), which reach ours through the entries' `jmp`s.

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): 53 new lines, one a
function (the extents of section 2). `00440630 29` was already there and
exact; `00440660 77` fixes the host line `00440660 258`.
