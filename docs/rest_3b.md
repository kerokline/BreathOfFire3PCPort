# Group R3B: the result screen's EXP, three percent clamps, the command menus' last steps, and 45 `Effect_Handlers` slots

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, from the
round branch's tip `7f116a2`. **64 functions ours** (`src/game/rest_3b.cpp`,
declarations in `symbols.gen.h` from their `symbols.toml` signatures, the cells
and the callees nobody owns in `src/game/rest_3b_callees.h`, shadow name
`rest_3b`): the cut's 60 rows for R3B (`analysis/round14_cut.tsv`) and four
`Effect_Handlers` slots no list had (section 2); each read to its last
instruction with capstone and fuzzed through the boss harness's engine frame
([`boss_harness.md`](boss_harness.md) section 10), used unchanged: 384,000
rounds, 0 mismatches. **116 controls planted** one at a time: 114 refused by a count, one an equivalent mutant (its near variant refused), one a `Fatal` rather than a count (replaced by a near variant, refused) - section 4. Six of the 64 were entered by recorded routes before
(section 8); the other 58 had no `entries_logic.txt` line until this group's,
so no trace says whether play reaches them: fuzz only here.

The band is three things in address order:

- **The result screen's EXP** (`0x4468B0..0x4469E5`): `BattleResult_AddExp`
  and `CharId_ToRosterIndex` (the sibling's names for their PSX twins,
  verified by reading), and the member test the first runs per slot;
- **three percent clamps** (`0x446F20..0x446FAD`), `(value * percent) / 100`
  within 0..999, 0..9999 and 0..100, which the stat rebuilds and the
  transformation call;
- **the command menus' steps no earlier group took** (`0x447290..0x44A00D`):
  the party side of the target pick, four cancels, two closing waits, the
  ability window's side pick (a table of its own, named here) and four
  dispatchers whose tables BE4 named;
- **45 slots of `Effect_Handlers`** (`0x44BED0..0x44CFF4`, slots 0..49 less 8
  and 31 - `battle_odds`' - and 22, 23, 38 - `battle_e5`'s).

The names say what the code does. Nothing here states what an ability, an
item, a status bit or a slot is in play: "the out bit" is the status bit
`Battle_ActorIsOut` tests (`0x4000` of a member's `+0x90` word / an enemy's
`+0x92`), "raise" names a slot that clears it and gives HP back, and a slot's
number is its index in `Effect_Handlers` (`0x64E73C`).

## 1. What each function does

### 1.1 The result screen's EXP and the percent clamps

| Address | Name | What (by the code) |
|---|---|---|
| `0x4468B0` | `BattleResult_AddExp(exp)` | for each party slot below `0x904AB0` (read again each pass) that `BattleResult_MemberTakesExp` answers for: the slot's character `+0x89` to a roster index by `CharId_ToRosterIndex`, called again for each use; that `CharacterRecords` record's EXP dword `+0xC` plus `exp` below 9,999,999 (unsigned) adds `exp` to the record the second call names, else the third call's record is set to 9,999,999. eax `exp` on both paths. PSX twin `0x801DD564` (the sibling's `BattleResult_AddExp`) |
| `0x446990` | `BattleResult_MemberTakesExp(slot)` | al 0 when `Battle_ActorIsOut(slot)` (the word pushed whole: `BattleResult_AddExp`'s byte counter stored over its caller's `ecx` slot, the upper bytes leftovers), else 1 when party record `slot & 0xFF`'s dword `+0x134` has bit 10 clear |
| `0x4469D0` | `CharId_ToRosterIndex(id)` | the byte `0x66972C[id & 0xFF]` (the table `symbols.toml` calls `MoveScript_EffectState`), 7 answered as 0; eax the byte zero-extended, which `BattleResult_FindLevelUp` passes on whole. PSX twin `0x801DD774` (the sibling's name) |
| `0x446F20` | `Stat_PercentCap999(value, percent)` | the product in 32 bits (wrapping), divided by 100 signed (`imul 0x51EB851F`, `sar 5`, plus the sign bit); above 999 999, below 0 zero. PSX `0x801DE074` (unnamed there) |
| `0x446F50` | `Stat_PercentCap9999` | the same within 0..9999 (PSX `0x801DE0C0`) |
| `0x446F80` | `Stat_PercentCap100` | the same within 0..100 (PSX `0x801DE10C`) |

### 1.2 The command menus' steps

The cells are [`battle_menu_states.md`](battle_menu_states.md)'s: `0x904AA1`
the phase's step, `AA2` the command, `AA3` its step, `AA4` the sub-state,
`0x904AAF` the pick flag, `0x939EC4` the menu actor, `0x939FA0` its command
record (`+0` the target, `+2` the word).

| Address | Name | Reached by | What |
|---|---|---|---|
| `0x447290` | `BattleTarget_PickParty` | `BattleTarget_Picks[2]` | `BattleTarget_PickEnemy`'s twin on the party: cancel sub-state 4, confirm 3; else `Input_AutoRepeat(Input_Pressed & 0xF000)`: `0x5000` back to the enemies (`Battle_DefaultTarget(3)` to `+0`, sub-state one down, cue 0x101) and done; else `0x2000` `Battle_DefaultTarget(Battle_WrapIndex(0x904AB0 - 1, 0, target + 1))` and `0x8000` `Battle_PrevTarget(Battle_WrapIndex(.., target - 1))`, each into `+0` (the record read again) with cue 0x101; both may run |
| `0x4473E0` | `BattleTarget_Cancel` | `BattleTarget_Picks[4]` | cue 0x106, window records 3's and 2's `+3` = 1, `BattleBanner_ShowName(the menu actor)`, the pick flag 0, step 2, `AA2..AA4` 0 |
| `0x448140` | `BattleAttackCmd_Cancel` | `BattleAttackCmd_States[3]` | the same, `AA4` left as it is |
| `0x447880` | `BattleItem_CloseWait` | `BattleItem_Steps[2]` | once window record 16's `+3` (`0x8033A3`) is 0: step 1, `AA2` = `AA3` = 0, tail `jmp ItemMenu_FreeWindows` |
| `0x448600` | `BattleItemCmd_CloseWait` | `BattleItemCmd_States[2]` | the same bytes |
| `0x447D30` | `BattleItem_TargetCancel` | `BattleItem_TargetSteps[4]`, `BattleItem_SideSteps[3]` | cue 0x106, the actor's name, the pick flag 0, step 4, `AA2` 1, `AA3` 0, `AA4` 1 |
| `0x448B40` | `BattleItemCmd_TargetCancel` | `BattleItemCmd_TargetSteps[4]`, `BattleItemCmd_SideSteps[3]` | the same with `AA2` 2 |
| `0x447D70` | `BattleItem_SideDispatch` | `BattleItem_Steps[5]` | `jmp [0x64E43C + 4 * (dword 0x904AA4 & 0xFF)]` - `BattleItem_SideSteps` (named here, 4) |
| `0x447D90` | `BattleItem_SideBegin` | `BattleItem_SideSteps[0]` | `+0` = 0x40 when byte `+0` of the command word `+2`'s `NameTable_Abilities` row has 0x20, else 0x80; `AA4` 1, the repeat latch `0x7E01B8` 0, the pick flag 1 (`BattleItemCmd_SideBegin`'s twin, on the ability rows) |
| `0x447DD0` | `BattleItem_SidePick` | `BattleItem_SideSteps[1]` | cancel 3, confirm 2; else a direction `Input_AutoRepeat` lets through (`0xF000`), with bit 0x80 of byte `+0` of the row of `Char_AbilityList(byte 0x929F06, byte 0x8033AB, 1)[dword 0x8033AC & 0xFF]`: `+0 ^= 0xC0`, cue 0x101 |
| `0x448B80` | `BattleItemCmd_SideDispatch` | `BattleItemCmd_States[5]` | through `BattleItemCmd_SideSteps` (BE4's, 4) by `AA4` |
| `0x448C80` | `BattleItemCmd_EquipDispatch` | `BattleItemCmd_States[6]` | through `BattleItemCmd_EquipSteps` (BE4's, 4) by `AA4` |
| `0x44A000` | `Escape_Dispatch` | `Battle_MenuSteps[6]` (`0x64AE6C`) | through `Escape_States` (BE4's, 3) by the byte `AA3`. PSX twin `0x8009823C` (unnamed there) |

### 1.3 The `Effect_Handlers` slots

`Effect_ApplyResult` calls `[0x64E73C + 4 * i]` with no argument and loads
eax afresh after (`0x44BB2A`): every slot is `void (void)`. "The actor" is the
byte `0x904B34`, "the target" `0x904B54`, "the result" `*0x904B60` (+4 the HP
delta, positive is damage; +6 the AP delta; +8 flags), read again after
every call. A member is the party record `0x802D40 + 0x14C n`, an enemy the
object `0x93B960 + 0x128 (n - 3)`; below 3 is a member unless said. "Half a
hit" is `Battle_CalcDamage(actor, target, 0xFFFF) / 2` (signed, toward 0).
The R3D helpers (`rest_3b_callees.h`): `0x44FB30` the miss tail;
`0x44FBB0(n)` a stat byte of the result moved by the ability's `+3`;
`0x44FC60(s)` the miss tail then status `s` unless resisted; `0x44FCA0(s)`
status `s` unless resisted; `0x44FCE0(d)` a share of the actor's HP.

| Slot | Address | Name | What |
|--:|---|---|---|
| 0 | `0x44BED0` | `EffectSlot00_Miss` | `jmp 0x44FB30` |
| 1 | `0x44BEE0` | `EffectSlot01_VariedHit` | the power word `0x939FE4` = the actor's `+0xA4` / `+0xB4`; `Rand & 3`: 0 halves it, 3 adds its half (16 bits); the delta `Battle_CalcDamage(actor, target, 0xFFFF)` |
| 2 | `0x44BF70` | `EffectSlot02_HitInflict4` | the delta `Battle_CalcDamage(.., 4)`; then, signed below the target's HP (`+0x98` / `+0xA4`, the target read again) and not 0, `0x44FCA0(4)` |
| 3 | `0x44BFF0` | `EffectSlot03_HalfHitInflict20` | half a hit; not 0, `0x44FCA0(0x20)` |
| 4 | `0x44C040` | `EffectSlot04_SkillPower` | not in the cut. `Effect_SkillDamage(actor, target, the ability's power byte NameTable_Abilities +3, 0)`. Slots 44, 45, 46, 49 and R3C's `0x44D000`, `0x44D020`, `0x44D040`, `0x44D0A0`, `0x44D0C0`, `0x44D100`, `0x44D140`, `0x44D680` set the ability and `jmp` here |
| 5 | `0x44C080` | `EffectSlot05_Clear80` | `Battle_ClearStatus(target, 0x80)`, tail `jmp 0x44FB30` |
| 6 | `0x44C0A0` | `EffectSlot06_HpThirdHit` | the actor's **party** record (by the actor byte, either side) `+0x89` 0x0A: `0x44FB30`, delta 9999; else `0x44FCE0(3)` less the target's **enemy** object `+0xB6` (either side); a negative delta then 0 (section 5) |
| 7 | `0x44C120` | `EffectSlot07_Heal` | not in the cut. `Effect_HealAmount(actor, target)`; R3C's `0x44D120` jumps here |
| 9 | `0x44C150` | `EffectSlot09_Heal40` | delta -40 |
| 10 | `0x44C160` | `EffectSlot10_Heal100` | delta -100 |
| 11 | `0x44C170` | `EffectSlot11_HealFull` | not in the cut. A member: -(`+0xA0`); an enemy: `+0xB0` of 0xFFFF gives 0xFFFF and `+0x10C |= 4` (the target read again), else -(`+0xB0`). R3C's `0x44D6A0` jumps here |
| 12 | `0x44C1F0` | `EffectSlot12_Heal5Clear68` | delta -5; `(Rand & 0x7F) <= 0x26`: `Battle_ClearStatus(target, 0x68)` |
| 13 | `0x44C220` | `EffectSlot13_Clear80` | slot 5's bytes |
| 14..16 | `0x44C240`, `0x44C260`, `0x44C280` | `EffectSlot14_Clear8`, `15_Clear100`, `16_ClearBFC` | `Battle_ClearStatus(target, 8 / 0x100 / 0xBFC)`, tail `jmp 0x44FB30` |
| 17 | `0x44C2A0` | `EffectSlot17_RaiseDown` | the target's status word has the out bit: delta 0xFFFF, `Battle_ClearStatus(target, 0x4000)`, then `0x904AB1` (a member) or `0x904AB3` one up; else delta 0 |
| 18 | `0x44C330` | `EffectSlot18_HalfHitInflict80` | half a hit; not 0, `0x44FCA0(0x80)` |
| 19, 24, 25, 37 | `0x44C380`, `0x44C7C0`, `0x44C7D0`, `0x44CC90` | `EffectSlot19_StatMod0`, `24_StatMod1`, `25_StatMod2`, `37_StatMod3` | `0x44FBB0(0 / 1 / 2 / 3)` |
| 20, 21 | `0x44C390`, `0x44C3B0` | `EffectSlot20_ApHeal20`, `21_ApHeal100` | the result's `+8` = 2, the AP delta -20 / -100 |
| 26, 27, 43 | `0x44C7E0`, `0x44C7F0`, `0x44CEF0` | `EffectSlot26_Inflict40`, `27_Inflict20`, `43_Inflict10` | `0x44FC60(0x40 / 0x20 / 0x10)` |
| 28 | `0x44C800` | `EffectSlot28_RaiseQuarter` | a member target only; its status word read, then `Rand & 3` not 0 and the out bit: delta -(maximum) >> 2 (the target read again, either side's maximum), `0x904AB1` / `AB3` one up, `Battle_ClearStatus(target, 0x4000)`; else the result's `+8` = 1 |
| 29 | `0x44C8E0` | `EffectSlot29_RaiseFull` | a member target only; `+0x91` bit 0x40: delta -10000, `0x904AB1` one up (stored before the call), `Battle_ClearStatus(target, 0x4000)`; else delta 0 |
| 30 | `0x44C940` | `EffectSlot30_HalfHitInflict8` | half a hit; not 0, `0x44FCA0(8)` |
| 32 | `0x44C9C0` | `EffectSlot32_HalfHitPlus1` | `0x904AA9 |= 0x20`; delta `(Battle_CalcDamage(..) sar 1) + 1` (16 bits) |
| 33 | `0x44C9F0` | `EffectSlot33_HitDropTurn` | `0x904AA9 |= 0x20`; the power word = the actor's `+0xA4` / `+0xB4` plus a quarter; the delta; not 0 and `0x44F6A0(actor, target)` al 0: `Battle_ReturnQueuedItem(target)`, `Battle_RemoveFromTurnOrder(target)` (each re-reading it) |
| 34 | `0x44CAB0` | `EffectSlot34_HitIgnore8` | the actor's status bit 8 kept and cleared; `0x904AA8 |= 0x80`, `0x939FFC` = 100; the delta; the bit put back into the status word of the actor read again |
| 35 | `0x44CB90` | `EffectSlot35_StatSumHit` | `0x904AA9 |= 0x20`; the actor's stat (a member's `CharacterRecords` `+0x44` by `0x66972C[+0x89]`, an enemy's `+0xD4`); `0x939FFC` = min(word `0x939FE8` / 2 + 30, 100); `0x939FE4` = 0, `Stat_AddClamped(0x939FE4, word 0x939FE6 + the stat)`; the delta |
| 36 | `0x44CC60` | `EffectSlot36_Skill20` | `Effect_SkillDamage(actor, target, 0x14, 0)` |
| 39 | `0x44CD00` | `EffectSlot39_ElementHit` | `Battle_CalcDamage(actor, target, the ability's NameTable_Abilities +4 word & 0x1FF)` |
| 40 | `0x44CD40` | `EffectSlot40_StatAAHit` | the power word = the actor's `+0xAA` / `+0xBA`; the delta (element 0xFFFF) |
| 41 | `0x44CDB0` | `EffectSlot41_StatSumHit2` | slot 35 with the addend word `0x939FE8` for `0x939FE6` |
| 42 | `0x44CE80` | `EffectSlot42_DoubleHit20` | the power word = the actor's `+0xA4` / `+0xB4` doubled; `Battle_CalcDamage(.., 0x20)` |
| 44, 45, 46, 49 | `0x44CF00`, `0x44CF20`, `0x44CF40`, `0x44CFE0` | `EffectSlot44_Skill66`, `45_Skill65`, `46_Skill62`, `49_Skill5D` | `0x904B35` = 4, the ability word `0x904B80` = 0x66 / 0x65 / 0x62 / 0x5D, tail `jmp` slot 4 |
| 47 | `0x44CF60` | `EffectSlot47_MissMark200` | not in the cut. `0x44FB30`; then by the **actor's** side the **target's** second flags `|= 0x200` (section 5). R3C's `0x44D160` jumps here |
| 48 | `0x44CFC0` | `EffectSlot48_StatMod0Skill55` | `0x904B35` = 4, the ability 0x55, `0x44FBB0(0)` |

## 2. The extents, the starts

`tools/band_rows.py --group R3B` (through the scratch wrapper `band14.py`):

- **Four starts the cut does not list**, each an `Effect_Handlers` slot with
  its own `ret` and reached by its cell and by R3C's tail `jmp`s: `0x44C040`
  (slot 4), `0x44C120` (7), `0x44C170` (11), `0x44CF60` (47). The cut's
  extents for `0x44BFF0` (144), `0x44C0A0` (160), `0x44C160` (144) and
  `0x44CF40` (128) ran over them. Taken; none is a shared tail (each is
  entered by a cell of the table, and the R3C handlers reach it by a whole
  `jmp` to its first byte).
- **No start dropped**: every one of the 60 is a function ending in its own
  `ret` or tail `jmp`, reached by a `.data` cell or a call (section 1).
- **The hidden starts' hosts are ours and do not contain them**:
  `Battle_SpawnActorCopies`, `ItemMenu_CanUseSelected`,
  `ItemMenu_SetupForMember`, `ItemMenu_FreeWindows` and `Effect_ApplyResult`
  each end before (their sources dispatch through the tables), so each start
  is a function of its own, reached by its table cell.
- **The cut's sizes are the catalogue's**; 51 differ from the code, 47 by
  trailing padding only. The extents in `symbols.toml` and
  `entries_logic.txt` are the code's.
- **No code in the band is left untaken**: the band tool's spans hold the 64
  and padding.

## 3. The fuzz (`rest_3b_fuzz.cpp`)

One `boss_harness::Run` with `Group::engine`, 64 clones, 6,000 rounds each.
Shapes: `kHelper` for the EXP helpers and the clamps (`ret_mask` 0xFFFFFFFF on
`BattleResult_AddExp`, `CharId_ToRosterIndex` and the three clamps - their
callers read eax whole; 0xFF on `BattleResult_MemberTakesExp`, read as al);
`kStep` for the menus' ten steps and the 45 slots; `kDispatch` with
`state_cell` `0x904AA4` (three) or `0x904AA3` (`Escape_Dispatch`) and the
table's own length (4, 4, 4, 3), the four tables `DataTable`s.
`BOF3X_R3B_ONLY=<substring>` runs the clones whose name holds it.

**The listing** beyond the engine set: the group's own three called directly
(`BattleResult_MemberTakesExp` and `CharId_ToRosterIndex` with their byte
masks - `and esi, 0xFF` / `and eax, 0xFF` at their entries - and
`EffectSlot04_SkillPower`, the tail of four slots); `Effect_SkillDamage`
(target and psi bytes, power's word, the caster unread - `battle_damage.cpp`)
and `Effect_HealAmount` (target byte, caster unread); BE4's
`Battle_PrevTarget` (its word whole, an actor 0..10 or 0xFF);
`Char_AbilityList` (the member's and the type's bytes - `al` / `cl` loaded
over leftovers); R3D's four by address (`0x44FBB0`, `0x44FC60`, `0x44FCA0`
flags, `0x44FCE0` garbage; one pushed immediate each).

**Louder stand-ins** (each found by a control the first pass left unrefused,
section 4): `Battle_CalcDamage` answers, a third of the time, the target's HP
less 1, 0 or plus 1 (slot 2's compare at its boundary); `0x44FCE0` the target
enemy object's `+0xB6` less 1, 0 or plus 1 (slot 6's clamp at -1 / 0);
`Rand` keeps the engine set's 15 bits and moves the target half the time
(noted first: slots 1, 12 and 28 read it again after); `Battle_ClearStatus`
notes the two raise counters `0x904AB1` / `0x904AB3` as it is called (slots
17, 28, 29 count one up around the call); `Battle_ActorIsOut` answers
exactly 0 or 1 (`kBool`, as the real one: the standard `kFlag`'s "not 0"
always has bit 4, so `al & 0xFE` could not be told from `al & 0xFF`). And
`CharId_ToRosterIndex`'s moves a member's character
byte `+0x89` half the time (noted first) - `BattleResult_AddExp` reads it again
for each of its calls; `BattleResult_MemberTakesExp`'s moves the count
`0x904AB0` (0..3, noted first) - the loop reads it again each pass;
`Char_AbilityList`'s answers a list inside the harness's text buffer (a
compared region), so the row of 0..255 indexes compared bytes.

**Regions** beyond the engine frame: `CharacterRecords` past the frame's
`0x903B24` (records 1..7), the repeat latch `0x7E01B8`, and
`0x803478..0x8034E0` / `0x8034F0..0x803B70` - party records 3..10 by an
actor or target byte, which slots 6 and 47 and `BattleResult_MemberTakesExp`'s
word index (section 5).

**Seeds** (every round): the actor and the target `0..10` (past 10 an enemy
object runs off the image's end `0x93F000`, a fault on both sides); each
member's character 0..23 or 0x0A (the roster table's 24 entries; slot 6's
test), HP at its maximum less 0..2 or 0 / 1 / 0xFFFF, maxima 1..9999, the out
bit half the time, `+0x134` bit 10 half; each enemy's HP, maximum (0xFFFF and
0xFFFE among them) and out bit; the ability among the ones the slots set;
`0x939FE8` at the hit rate's bound (139..142); the party count 0..3. For the
menus: the keys (single-bit confirm / cancel masks; the pressed word one of
them, a direction, both, none or anything), the command's `+0` at the
sides' ends and signed, its word `+2` inside the ability rows, record 16's
closing byte 0 / 1, its page and row. `BattleResult_AddExp`: every record's
EXP at 9,999,999 less 0..1000 or anywhere below. The helpers' words: the
byte arguments with garbage above them half the time; the clamps' products
at 0, 99..101, 999 / 1000, 9999 / 10000, the signed ends and -100.

**Disturbance** (the group's case, from the hash only): the target and the
actor (0..10), the party count, a member's character byte, a member's and an
enemy's out bit, the command's target byte, `0x939FE8`. The engine frame's
moves the result record, the menu actor and its record, and the step bytes.

Results (this worktree, 2026-10-04; counts depend on the build directory):

    shadow      rest_3b self-test: 384000 rounds over 64 functions (6000 each), 425457 calls to the stand-ins, 0 MISMATCHES; 38856 bytes of state (31 regions) and the stand-ins' log compared

## 4. Controls

`BOF3X_R3B_ONLY=<clone>` with one change planted in ours at a time (scratch
`controls.py`: plant on a unique anchor, rebuild, run, restore; one rebuild
at the end), this worktree, 2026-10-04, at the tip of section 3. **116
planted; 114 refused by a count.** The first pass (before the louder
stand-ins of section 3) left five more unrefused - 7, 40, 50, 76, 79: each
the fuzz's fault (a boundary no answer reached, a target never moved after
`Rand`, a counter store the stand-in could not see, `Battle_ActorIsOut`'s
answer never 1); all five are refused now.

- **89 is an equivalent mutant**: `half + 30 >= 100` for `> 100` stores 100
  at the bound either way (the byte computed there is 100 too). Its near
  variant 116 (`> 0x65`) is refused.
- **29 is a `Fatal`, not a count**: dispatching by `0x904AA3` instead of
  `0x904AA4` reads a byte the harness does not draw below the table, and ours
  aborts past 4 (section 5, L3) - a crash is not a refusal. Its replacement
  115 (the wrong table) is refused in every round.

| n | Clone | Planted | Refused in |
|--:|---|---|--:|
| 1 | `BattleResult_AddExp` | AddExp: the sum below or at the cap (<=) | 175 of 6,000 |
| 2 | `BattleResult_AddExp` | AddExp: the cap stored 9999998 | 2,571 of 6,000 |
| 3 | `BattleResult_AddExp` | AddExp: answers exp + 1 | 6,000 of 6,000 |
| 4 | `BattleResult_AddExp` | AddExp: the count read once, not each pass | 1,845 of 6,000 |
| 5 | `BattleResult_AddExp` | AddExp: the character byte not read again for the second call | 266 of 6,000 |
| 6 | `BattleResult_MemberTakesExp` | MemberTakesExp: bit 9, not 10 | 1,003 of 6,000 |
| 7 | `BattleResult_MemberTakesExp` | MemberTakesExp: the out test's al & 0xFE | 1,974 of 6,000 |
| 8 | `CharId_ToRosterIndex` | CharId: 6 answered as 0 | 464 of 6,000 |
| 9 | `CharId_ToRosterIndex` | CharId: the id & 0x7F | 775 of 6,000 |
| 10 | `Stat_PercentCap` | Percent: divided by 99 | 1,495 of 6,000 |
| 11 | `Stat_PercentCap999` | PercentCap999: capped at 998 | 2,079 of 6,000 |
| 12 | `Stat_PercentCap100` | Percent: -1 kept (q < -1) | 158 of 6,000 |
| 13 | `BattleTarget_PickParty` | PickParty: 0x4000 back to the enemies, not 0x5000 | 778 of 6,000 |
| 14 | `BattleTarget_PickParty` | PickParty: the sub-state two down | 2,281 of 6,000 |
| 15 | `BattleTarget_PickParty` | PickParty: 0x2000 moves by 2 | 33 of 6,000 |
| 16 | `BattleTarget_PickParty` | PickParty: 0x8000 through Battle_DefaultTarget | 386 of 6,000 |
| 17 | `BattleTarget_PickParty` | PickParty: back to Battle_DefaultTarget(0) | 2,282 of 6,000 |
| 18 | `BattleTarget_PickParty` | PickParty: confirm before cancel | 1,064 of 6,000 |
| 19 | `BattleTarget_Cancel` | Cancel: the sub-state 1 | 6,000 of 6,000 |
| 20 | `BattleTarget_Cancel` | CancelHead: record 3's +3 = 2 | 6,000 of 6,000 |
| 21 | `BattleAttackCmd_Cancel` | AttackCmd_Cancel: the sub-state zeroed too | 5,977 of 6,000 |
| 22 | `BattleItem_CloseWait` | CloseWait: waits only past 1 | 1,271 of 6,000 |
| 23 | `BattleItemCmd_CloseWait` | CloseWait: step 2 | 2,774 of 6,000 |
| 24 | `BattleItem_TargetCancel` | Item TargetCancel: the command 2 | 6,000 of 6,000 |
| 25 | `BattleItemCmd_TargetCancel` | ItemCmd TargetCancel: the command 1 | 6,000 of 6,000 |
| 26 | `BattleItem_SideDispatch` | SideDispatch: through BattleItemCmd_SideSteps | 6,000 of 6,000 |
| 27 | `BattleItemCmd_EquipDispatch` | EquipDispatch: through the side steps | 6,000 of 6,000 |
| 28 | `Escape_Dispatch` | Escape_Dispatch: through the equip steps | 6,000 of 6,000 |
| 29 | `BattleItemCmd_SideDispatch` | ItemCmd SideDispatch: by 0x904AA3 | a Fatal, not a count (section 4) |
| 30 | `BattleItem_SideBegin` | SideBegin: bit 0x40 picks the enemies | 1,528 of 6,000 |
| 31 | `BattleItem_SideBegin` | SideBegin: the latch 1 | 6,000 of 6,000 |
| 32 | `BattleItem_SidePick` | SidePick: bit 0x40 flips | 756 of 6,000 |
| 33 | `BattleItem_SidePick` | SidePick: directions 0x7000 | 192 of 6,000 |
| 34 | `BattleItem_SidePick` | SidePick: the row & 0x7F | 581 of 6,000 |
| 35 | `BattleItem_SidePick` | SidePick: flips 0x80 only | 838 of 6,000 |
| 36 | `EffectSlot00_Miss` | Slot 0: 0x44FBB0(0) for the miss tail | 6,000 of 6,000 |
| 37 | `EffectSlot01_VariedHit` | Slot 1: roll 2 adds the half | 2,519 of 6,000 |
| 38 | `EffectSlot01_VariedHit` | Slot 1: a quarter added | 1,511 of 6,000 |
| 39 | `EffectSlot01_VariedHit` | Slot 1: the actor's +0xA6 | 1,591 of 6,000 |
| 40 | `EffectSlot02_HitInflict4` | Slot 2: below or at the HP | 363 of 6,000 |
| 41 | `EffectSlot02_HitInflict4` | Slot 2: element 5 | 6,000 of 6,000 |
| 42 | `EffectSlot02_HitInflict4` | Slot 2: the delta unsigned against the HP | 1,910 of 6,000 |
| 43 | `EffectSlot03_HalfHitInflict20` | HalfHit: shifted (rounds down) | 1,373 of 6,000 |
| 44 | `EffectSlot03_HalfHitInflict20` | Slot 3: status 0x21 | 5,371 of 6,000 |
| 45 | `EffectSlot04_SkillPower` | Slot 4: the power byte +2 | 3,804 of 6,000 |
| 46 | `EffectSlot05_Clear80` | Slot 5: mask 0x81 | 6,000 of 6,000 |
| 47 | `EffectSlot14_Clear8` | ClearThenMiss: no miss tail | 6,000 of 6,000 |
| 48 | `EffectSlot06_HpThirdHit` | Slot 6: character 0x0B | 873 of 6,000 |
| 49 | `EffectSlot06_HpThirdHit` | Slot 6: 9998 | 819 of 6,000 |
| 50 | `EffectSlot06_HpThirdHit` | Slot 6: -1 kept | 519 of 6,000 |
| 51 | `EffectSlot06_HpThirdHit` | Slot 6: the enemy's +0xB4 | 3,793 of 6,000 |
| 52 | `EffectSlot06_HpThirdHit` | Slot 6: the target not read again after 0x44FCE0 | 12 of 6,000 |
| 53 | `EffectSlot07_Heal` | Slot 7: the actor and target swapped | 5,426 of 6,000 |
| 54 | `EffectSlot09_Heal40` | Slot 9: -39 | 6,000 of 6,000 |
| 55 | `EffectSlot11_HealFull` | Slot 11: 0xFFFE the special maximum | 754 of 6,000 |
| 56 | `EffectSlot11_HealFull` | Slot 11: +0x10C |= 8 | 272 of 6,000 |
| 57 | `EffectSlot11_HealFull` | Slot 11: a member's +0x9E | 1,665 of 6,000 |
| 58 | `EffectSlot11_HealFull` | Slot 11: the target not read again for the mark | 277 of 6,000 |
| 59 | `EffectSlot12_Heal5Clear68` | Slot 12: below 0x26 | 23 of 6,000 |
| 60 | `EffectSlot12_Heal5Clear68` | Slot 12: mask 0x69 | 1,795 of 6,000 |
| 61 | `EffectSlot15_Clear100` | Slot 15: mask 0x101 | 6,000 of 6,000 |
| 62 | `EffectSlot16_ClearBFC` | Slot 16: mask 0xBFD | 6,000 of 6,000 |
| 63 | `EffectSlot17_RaiseDown` | Slot 17: bit 0x2000 | 2,990 of 6,000 |
| 64 | `EffectSlot17_RaiseDown` | Slot 17: target 2 counts the enemies | 280 of 6,000 |
| 65 | `EffectSlot17_RaiseDown` | Slot 17: the delta 0xFFFE | 2,956 of 6,000 |
| 66 | `EffectSlot18_HalfHitInflict80` | Slot 18: status 0x40 | 5,371 of 6,000 |
| 67 | `EffectSlot19_StatMod0` | Slot 19: 0x44FBB0(4) | 6,000 of 6,000 |
| 68 | `EffectSlot20_ApHeal20` | Slot 20: +8 = 3 | 6,000 of 6,000 |
| 69 | `EffectSlot21_ApHeal100` | Slot 21: the HP delta, not the AP | 6,000 of 6,000 |
| 70 | `EffectSlot25_StatMod2` | Slot 25: 0x44FBB0(1) | 6,000 of 6,000 |
| 71 | `EffectSlot26_Inflict40` | Slot 26: through 0x44FCA0 | 6,000 of 6,000 |
| 72 | `EffectSlot28_RaiseQuarter` | Slot 28: Rand & 1 | 140 of 6,000 |
| 73 | `EffectSlot28_RaiseQuarter` | Slot 28: a member's half | 270 of 6,000 |
| 74 | `EffectSlot28_RaiseQuarter` | Slot 28: an enemy target 3 taken | 569 of 6,000 |
| 75 | `EffectSlot28_RaiseQuarter` | Slot 28: the status read after Rand | 241 of 6,000 |
| 76 | `EffectSlot28_RaiseQuarter` | Slot 28: the quarter rounded toward 0 | 168 of 6,000 |
| 77 | `EffectSlot29_RaiseFull` | Slot 29: -9999 | 783 of 6,000 |
| 78 | `EffectSlot29_RaiseFull` | Slot 29: +0x91 bit 0x80 | 862 of 6,000 |
| 79 | `EffectSlot29_RaiseFull` | Slot 29: the count stored after the call | 783 of 6,000 |
| 80 | `EffectSlot30_HalfHitInflict8` | Slot 30: status 0x10 | 5,371 of 6,000 |
| 81 | `EffectSlot32_HalfHitPlus1` | Slot 32: plus 2 | 6,000 of 6,000 |
| 82 | `EffectSlot32_HalfHitPlus1` | Slot 32: the round flag 0x10 | 4,478 of 6,000 |
| 83 | `EffectSlot33_HitDropTurn` | Slot 33: a half added | 6,000 of 6,000 |
| 84 | `EffectSlot33_HitDropTurn` | Slot 33: the resist test inverted | 5,742 of 6,000 |
| 85 | `EffectSlot33_HitDropTurn` | Slot 33: the target not read again for the turn order | 22 of 6,000 |
| 86 | `EffectSlot34_HitIgnore8` | Slot 34: bits 8 and 4 cleared | 3,019 of 6,000 |
| 87 | `EffectSlot34_HitIgnore8` | Slot 34: the actor not read again for the put-back | 121 of 6,000 |
| 88 | `EffectSlot34_HitIgnore8` | Slot 34: the hit rate 99 | 6,000 of 6,000 |
| 89 | `EffectSlot35_StatSumHit` | StatSumHit: 100 at the bound (>=) | not refused: equivalent (section 4) |
| 90 | `EffectSlot35_StatSumHit` | StatSumHit: a member's record +0x46 | 1,591 of 6,000 |
| 91 | `EffectSlot41_StatSumHit2` | StatSumHit: an enemy's +0xD6 | 4,409 of 6,000 |
| 92 | `EffectSlot41_StatSumHit2` | Slot 41: the addend 0x939FE6 | 6,000 of 6,000 |
| 93 | `EffectSlot35_StatSumHit` | StatSumHit: the power word 1 before the add | 6,000 of 6,000 |
| 94 | `EffectSlot35_StatSumHit` | StatSumHit: the round flag 0x10 | 4,478 of 6,000 |
| 95 | `EffectSlot36_Skill20` | Slot 36: power 0x15 | 6,000 of 6,000 |
| 96 | `EffectSlot39_ElementHit` | Slot 39: the element & 0xFF | 84 of 6,000 |
| 97 | `EffectSlot40_StatAAHit` | Slot 40: a member's +0xAC | 1,591 of 6,000 |
| 98 | `EffectSlot42_DoubleHit20` | Slot 42: quadrupled | 6,000 of 6,000 |
| 99 | `EffectSlot42_DoubleHit20` | Slot 42: element 0x21 | 6,000 of 6,000 |
| 100 | `EffectSlot43_Inflict10` | Slot 43: status 0x11 | 6,000 of 6,000 |
| 101 | `EffectSlot44_Skill66` | Slot 44: ability 0x67 | 6,000 of 6,000 |
| 102 | `EffectSlot46_Skill62` | AsAbility: the acting kind 5 | 6,000 of 6,000 |
| 103 | `EffectSlot47_MissMark200` | Slot 47: by the target's side (the latent defect fixed) | 1,849 of 6,000 |
| 104 | `EffectSlot47_MissMark200` | Slot 47: bit 0x100 | 4,488 of 6,000 |
| 105 | `EffectSlot48_StatMod0Skill55` | Slot 48: ability 0x56 | 6,000 of 6,000 |
| 106 | `EffectSlot49_Skill5D` | Slot 49: ability 0x5E | 6,000 of 6,000 |
| 107 | `EffectSlot39_ElementHit` | Hit: the actor and target swapped | 5,426 of 6,000 |
| 108 | `EffectSlot10_Heal100` | Slot 10: -99 | 6,000 of 6,000 |
| 109 | `EffectSlot13_Clear80` | Slot 13: mask 0x40 | 6,000 of 6,000 |
| 110 | `EffectSlot24_StatMod1` | Slot 24: 0x44FBB0(2) | 6,000 of 6,000 |
| 111 | `EffectSlot27_Inflict20` | Slot 27: status 0x40 | 6,000 of 6,000 |
| 112 | `EffectSlot37_StatMod3` | Slot 37: 0x44FBB0(2) | 6,000 of 6,000 |
| 113 | `EffectSlot45_Skill65` | Slot 45: ability 0x64 | 6,000 of 6,000 |
| 114 | `BattleResult_MemberTakesExp` | MemberTakesExp: answers 1 for a member out (near variant of 7) | 4,045 of 6,000 |
| 115 | `BattleItemCmd_SideDispatch` | ItemCmd SideDispatch: through BattleItem_SideSteps (for 29) | 6,000 of 6,000 |
| 116 | `EffectSlot35_StatSumHit` | StatSumHit: capped above 0x65 (> 0x65; near variant of 89) | 573 of 6,000 |


## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **L1 - slot 47 picks the side by the actor and indexes by the target.**
  `EffectSlot47_MissMark200` (also R3C's `0x44D160`'s tail) tests `0x904B34
  <= 2` and then sets bit 0x200 of the **target's** record on that side: a
  member acting on an enemy (target 3..10) sets the bit in "party record 3..10"
  - `0x802D40 + 0x14C t + 0x134`, which lands in the window records
  (`0x803258`, `0x8033A4` for 3, 4: window records 6's `+0x20` and 16's
  `+4`, its x word) and past them; an
  enemy acting on a member sets it in "enemy object -3..-1", inside the task
  slots (`0x93B6BC`..). Ours writes exactly where the original does. Whether
  play reaches the mismatched case depends on which actions use slot 47 or
  `0x44D160`, which was not read. **Not a stale read**: no ledger entry is
  owed by the brief's rule, but if play reaches it the owner may want one.
- **L2 - slot 6 reads the actor's party record and the target's enemy object
  whichever side each is on**: a member target gives `Enemy(t)` below the
  enemies (the task slots), an enemy actor `Party(3..10)` (the window
  records). Read-only; ours reads the same bytes.
- **L3 - the dispatchers index their tables unchecked.** Each step keeps its
  byte inside its table; ours aborts past 4 / 4 / 4 / 3.
- **L4 - `BattleResult_AddExp` runs to the count byte `0x904AB0` with no
  bound**: past 3 it reads party records beyond the party (the window
  records) and their characters through `0x66972C` past its 24 entries, and
  writes EXP into whatever record that names. The fuzz keeps the count at
  0..3; the party is three at most.
- **L5 - the actor / target bytes are unchecked everywhere**: past 10 an enemy
  object runs off the image (a fault on both sides). Every caller writes them
  below 11 by the engine's own code ([`battle_e5.md`](battle_e5.md) section 7).

## 6. Calls across groups

Out of this group, raw in `rest_3b_callees.h` until the owner merges (all
R3D's, which merges first by the round's order):

| Callee | Called by |
|---|---|
| `0x44FB30` | slots 0, 5, 13..16 (tail `jmp`), 6, 47 |
| `0x44FBB0` | slots 19, 24, 25, 37, 48 |
| `0x44FC60` | slots 26, 27, 43 |
| `0x44FCA0` | slots 2, 3, 18, 30 |
| `0x44FCE0` | slot 6 |
| `0x44F6A0` | slot 33 (nobody's; the engine set's row) |

Into this group from outside it (inbound, for the rebinding pass):

| Caller | Owner | Callee |
|---|---|---|
| `BattleResult_ExpTick` | ours (battle_result) | `BattleResult_AddExp` (rebound, section 7) |
| `BattleResult_FindLevelUp`, `BattleResult_Setup` | ours (battle_result) | `CharId_ToRosterIndex` (rebound) |
| `BattleResultWin_ExpToNext` | ours (R2G) | `CharId_ToRosterIndex` (rebound) |
| `DragonForm_Mix`, `DragonForm_ApplyRecipe`, `Battle_RecalcStats`, `Battle_RecalcMemberStats` | ours (battle_e6) | the three clamps (rebound) |
| `0x44D000`, `0x44D020`, `0x44D040`, `0x44D0A0`, `0x44D0C0`, `0x44D100`, `0x44D140`, `0x44D680` | R3C (this wave) | tail `jmp` `EffectSlot04_SkillPower` |
| `0x44D120`, `0x44D6A0`, `0x44D160` | R3C | tail `jmp` `EffectSlot07_Heal`, `EffectSlot11_HealFull`, `EffectSlot47_MissMark200` |
| `Effect_ApplyResult` | ours (battle_damage) | the 45 slots, through `Effect_Handlers` |
| the menus' dispatchers | ours (battle_actor_copies, battle_menu_states, battle_phases' `Battle_MenuSteps` reader) | the steps, through their tables |

## 7. Named data, and the rebinding

**Named** (`symbols.toml` `[[data]]`): `BattleItem_SideSteps` `0x64E43C`
(4: `BattleItem_SideBegin`, `_SidePick`, `BattleItem_Confirm` `0x447CC0`,
`BattleItem_TargetCancel`; read by `BattleItem_SideDispatch`, the entries set
1..3; `BattleAttackCmd_States` starts at `0x64E44C`). The other three tables
my dispatchers read (`BattleItemCmd_SideSteps`, `BattleItemCmd_EquipSteps`,
`Escape_States`) were named by BE4 and are left as they are.

**Rebound** in the round-ten form (the value unchanged, so the fuzz keys
stand): `battle_result_callees.h`'s `kAddExp` and `kRosterIndex`,
`rest_2g_callees.h`'s `kRosterIndex`, `battle_e6_callees.h`'s
`kPercent999` / `9999` / `100` -> `bof3::addr::<name>`; and four comments in
`battle_actor_copies.cpp` that called these steps "not ours".

**Left raw on purpose**: the fuzz files' keys and rows -
`battle_result_fuzz.cpp`'s `case kAddExp` / call sites,
`battle_e6_fuzz.cpp`'s `CallSite` rows, `rest_2g_fuzz.cpp`'s `{0x1E,
0x4469D0}` and its listing, `battle_e4_fuzz.cpp`'s three via dispatchers
(`0x448B80`, `0x448C80`, `0x44A000`), `battle_menu_states_fuzz.cpp`'s and
`battle_phases_fuzz.cpp`'s table entries - and comments that describe the
original's calls by address (`battle_e6.cpp`, `battle_result.cpp`,
`battle_e4.cpp`, `battle_menu_states.cpp`, `battle_phases.cpp`). **The harness's
rows** `0x446F20` / `0x446F50` / `0x446F80` in `boss_harness.cpp`'s
`kEngineStandard` (`kThrough`, key = address) stay valid and were not edited:
`Register` accepts a key equal to its address inside `.text`, and both sides
run the real code at that address (ours after the inject) - for the
coordinator's fold, they can become `BH_OURS` rows. No raw reference to an R3B
function lies in a file another group of this round writes.

## 8. The live route

The six non-hidden helpers had `entries_logic.txt` lines before (from round
twelve's plan): `BattleResult_AddExp`, `BattleResult_MemberTakesExp` and
`CharId_ToRosterIndex` are entered in `reach_balioAndSunder_2_1003`,
`reach_bossAndFlash_1003`, `reach_dragon` and the `hash_r13_nue_*` call counts;
the three clamps in the dragon routes' and others' call counts (`hash_dragon_*`,
`hash2_dragon_*`, 23 files for `0x446F20`). The other 58 had no line, so no
trace under `analysis/calltrace` names them (a scan of every file for each
address, 2026-10-04); for the hidden ones the catalogue's `reach` is their
host's. Their 58 lines are added now; the coordinator's live check (the state
hash) will say which a route enters.

## 9. Self-tests and the entry list

Headless, this worktree, 2026-10-04 (counts depend on the build directory):

- `BOF3X_SHADOW=rest_3b`: exit 0, the line of section 3.
- `BOF3X_SHADOW='*'` at the group's first build: exit 0, 733 self-test lines,
  every differential one 0 mismatches, no Fatal; again at the final build,
  narrow: exit 0, 733 self-test lines, no differential line with a mismatch, no Fatal.
- `BOF3X_SHADOW='*'` with `BOF3X_WIDE=1` at the final build: exit 0, 733
  self-test lines, no differential line with a mismatch, no Fatal; rest_3b's
  line 424,868 calls, 0 mismatches.
- `tools/ledger_check.py`: 73 ledger entries, 0 errors.
- Neither `'*'` run died silently.

`analysis/calltrace/entries_logic.txt` (the main checkout's): 58 lines
appended with the code's extents; six were there with the same extents. The
host lines that cover functions of this group, left for the coordinator's
split: `00447840 70` (`ItemMenu_CanUseSelected`, over `BattleItem_CloseWait`)
and `0044B9F0 750` (`Effect_ApplyResult`, over slots 0..7).
