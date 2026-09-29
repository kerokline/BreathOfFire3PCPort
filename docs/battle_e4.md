# Group BE4: damage, the pace and target helpers, the item command's states 5..9 and the escape

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3), wave one, stage B. All 56 functions `analysis/round12_cut.tsv` lists for
BE4 are ours (`src/game/battle_e4.cpp`, shadow name `battle_e4`), each read
to its last instruction with capstone and fuzzed through the boss harness as
an engine group ([`boss_harness.md`](boss_harness.md) section 10) without
edits to it: one `Run`, 60 clones (the 56 and four driven again through the
dispatcher that reaches them), 360,000 rounds, 0 mismatches. CONTROLS_LINE
Nine of the 56 are entered by the owner's `dragonTransform.txt` (section
11); the rest are fuzz-only.

The band tool (`tools/band_rows.py --group BE4`) and the cut agree: 56
functions, none missing from the list, none listed that is not a function.
Its extents are the code's; the cut's sizes are longer by the trailing
padding in 23 rows (`0x447F40`: 0x8C of code, the cut's 144 with the nops)
and never by code. The PSX twins below are `analysis/pairs_propagated.json`'s
hypotheses (tier in brackets); a sibling name was transferred only where the
sibling has one and the PC code reads the same (five: section 1 says which).

| Address | Name | Extent | Hidden | Shape |
|---|---|---|---|---|
| `0x444660` | `BattleWin_DimScreen` | 0x7D | | helper |
| `0x4446E0` | `BattleWin_DrawTileTint` | 0xC4 | | helper |
| `0x445600` | `Battle_EnemyOutpaces` | 0x3F | | helper, al |
| `0x4457F0` | `Battle_PrevTarget` | 0xB1 | | helper, al |
| `0x446110` | `Battle_HitOrMissParty` | 0x9E | | helper, ax |
| `0x4461B0` | `Battle_HitOrMissEnemy` | 0xFA | | helper, ax |
| `0x4463E0` | `Battle_PartyDefenceMean` | 0x4A | | helper, eax |
| `0x446540` | `Battle_SetHpChange` | 0xB7 | | helper |
| `0x446600` | `Battle_ReloadPartyRecords` | 0x43 | | helper |
| `0x446700` | `Battle_OrderPushFront` | 0x1B | | helper |
| `0x446720` | `AutoBattle_FillCommands` | 0x50 | | helper |
| `0x446770` | `Battle_TurnVectorC` | 0x41 | | helper |
| `0x4467C0` | `Battle_TurnVector18` | 0x41 | | helper |
| `0x446810` | `Battle_MemberReactRoll` | 0x98 | | helper, al |
| `0x446A80` | `Battle_WriteBackMember` | 0x7B | | helper |
| `0x446B00` | `Battle_PickEnemyTarget` | 0xAB | | helper |
| `0x446CB0` | `Battle_WakeRoll` | 0xD6 | | helper, al |
| `0x446D90` | `Battle_ReturnItem` | 0x45 | | helper, al |
| `0x446DE0` | `BattleEnd_EnterStep1` | 0x16 | | helper |
| `0x446E00` | `BattleEnd_EnterStep2` | 0x16 | | helper |
| `0x446E20` | `BattleEnd_EnterStep3` | 0x16 | | helper |
| `0x447F40` | `ItemMenu_SetupForMember` | 0x8C | | helper |
| `0x448BA0` | `BattleItemCmd_SideBegin` | 0x51 | in `0x447F40` | step (`0x904AA4`) |
| `0x448C00` | `BattleItemCmd_SidePick` | 0x7C | in `0x447F40` | step |
| `0x448CA0` | `BattleItemCmd_EquipMenu` | 0x138 | in `0x447F40` | step |
| `0x448DE0` | `BattleItemCmd_EquipSlotPick` | 0x1DD | in `0x447F40` | step |
| `0x448FC0` | `BattleItemCmd_EquipItemPick` | 0x210 | in `0x447F40` | step |
| `0x4491D0` | `BattleItemCmd_EquipUsePick` | 0x202 | in `0x447F40` | step |
| `0x4493E0` | `BattleItemCmd_EquipUseKind` | 0x9B | in `0x447F40` | step (`0x904AA3`) |
| `0x449480` | `BattleItemCmd_EquipTargetDispatch` | 0x11 | in `0x447F40` | dispatch, 5 |
| `0x4494A0` | `BattleItemCmd_EquipTargetBegin` | 0x93 | in `0x447F40` | step |
| `0x449540` | `BattleItemCmd_EquipPickEnemy` | 0x13C | in `0x447F40` | step |
| `0x449680` | `BattleItemCmd_EquipPickMember` | 0x1AC | in `0x447F40` | step |
| `0x449830` | `BattleItemCmd_EquipCommit` | 0x69 | in `0x447F40` | step |
| `0x4498A0` | `BattleItemCmd_EquipCancel` | 0x4A | in `0x447F40` | step |
| `0x4498F0` | `BattleItemCmd_EquipSideDispatch` | 0x11 | in `0x447F40` | dispatch, 4 |
| `0x449910` | `BattleItemCmd_EquipSideBegin` | 0x52 | in `0x447F40` | step |
| `0x449970` | `BattleItemCmd_EquipSidePick` | 0x83 | in `0x447F40` | step |
| `0x449A00` | `BattleEquip_Apply` | 0x1A3 | | helper |
| `0x449BB0` | `BattleEquip_Preview` | 0xBD | | helper |
| `0x449C70` | `BattleEquip_RemoveSlot` | 0x18C | | helper, al |
| `0x449E90` | `BattleEquip_OpenChange` | 0xC9 | | helper |
| `0x449F60` | `BattleEquip_OpenUse` | 0x73 | | helper |
| `0x44A010` | `Escape_Roll` | 0x113 | in `0x449FE0` | step (`0x904AA3`) |
| `0x44A130` | `Escape_FailDispatch` | 0x11 | in `0x449FE0` | dispatch, 4 |
| `0x44A150` | `Escape_Begin` | 0x18F | in `0x449FE0` | step |
| `0x44A2E0` | `Escape_StepBack` | 0x49 | in `0x449FE0` | step |
| `0x44A330` | `Escape_StepOn` | 0x49 | in `0x449FE0` | step |
| `0x44A380` | `Escape_Failed` | 0xCD | in `0x449FE0` | step |
| `0x44A450` | `Escape_WinDispatch` | 0x11 | in `0x449FE0` | dispatch, 3 |
| `0x44A470` | `Escape_Leave` | 0x73 | in `0x449FE0` | step |
| `0x44A4F0` | `Escape_End` | 0x30 | in `0x449FE0` | step |
| `0x44A520` | `Escape_Chance` | 0x96 | | helper, al |
| `0x44A910` | `Battle_MemberNameToText` | 0x41 | | helper |
| `0x44A960` | `Battle_EnemyNameToText` | 0x2A | | helper |
| `0x44AA90` | `BattleBanner_AddLine` | 0x39 | | helper |

**The hidden starts and their hosts.** The 16 in `0x447F40`'s catalogue
extent follow its 0x8C bytes of code; the nine in `0x449FE0`'s follow
`ItemMenu_FreeWindows`' 0x20 (ours since round eight, `battle_misc.cpp`,
which contains none of their code). Each is reached by address from a
`.data` table (section 4), so each is its own function. Between them sit
three starts of the catalogue's part 2 that no group owns this round and
this group calls or drives only through tables: `0x448B40` (the list item's
target pick cancelled), `0x448B80` and `0x448C80` (BattleItemCmd_States
entries 5 and 6: `jmp [0x64E4A0 / 0x64E4B0 + 4 x (dword 0x904AA4 & 0xFF)]`),
`0x448600` (entry 2) and `0x44A000` (Battle_MenuSteps entry 6, `jmp
[0x64E4FC + 4 x byte 0x904AA3]`). They are Capcom's, called raw by the fuzz's
`Clone::via` rows only.

## 1. What each function does

### 1.1 Screen tiles

- **`BattleWin_DimScreen`** (PSX `0x801D98D0` [gap44]; BE1's `0x42EE00`): the
  draw mode for the page (0x3C0, 0) - `Gpu_GetTPage(0, 2, 0x3C0, 0)` and
  `Gpu_SetDrawMode(packet, 0, 0, page, 0)`, whose fifth word is the 0 the
  compiler left on the stack from the first call's five pushes - then a TILE
  at `Gfx_PacketNext` read again: (0, 0), 960.0 x 240.0, grey 0x28,
  semi-transparent mode 1.
- **`BattleWin_DrawTileTint`** (PSX `0x801D997C` [gap44]; `0x433300`,
  `0x433350`, nobody's): the same head, then `BattleWin_DrawTile`'s tile
  (s16 x, s16 y, size byte into `0x64E268`) in the colour of the three bytes
  `0x903850..0x903852`.

### 1.2 Turn and damage helpers

- **`Battle_EnemyOutpaces(actor, average, highest)`** (PSX `0x801DB3AC`
  [gap74]; `Battle_MarkFasterSide`): `Battle_PartyOutpaces`' enemy twin -
  al 1 when the enemy object's u16 `+0xB8` is at least twice the average's
  low word and not below the highest's.
- **`Battle_PrevTarget(actor)`** (PSX `0x801DB6BC` [gap74]; nine target
  picks): `Battle_DefaultTarget`'s downward twin, EH's worked example 1
  ([`boss_harness.md`](boss_harness.md) 10.3), the placeholder name replaced.
  The comparisons are signed bytes: a byte of 0x80..0xFF searches from 10
  down through 0, 0xFF, 0xFE ... to just above itself.
- **`Battle_HitOrMissParty` / `_HitOrMissEnemy(amount, attacker, target)`**
  (no twin; `Battle_CalcDamage`'s two calls, [`battle_damage.md`](battle_damage.md)):
  ax the amount or 0. With round flag 0x80 the amount. A miss: the
  attacker's status bit 8 (a member's `+0x90`, an enemy's `+0x92`) with
  `Rand` bit 1; on a member, 0x939F9B not below `Rand() % 100`; on an enemy,
  the target's `+0x90` bit 4 with `Rand` bit 1, or - for a member attacking
  without `+0x134` bit 0x100 - the low byte of 0x939FFC not above `Rand() %
  100`. A miss clears the target's bit 0x10 (`+0x12C` member, `+0x10C`
  enemy). Ours answers the amount's low word zero-extended; the caller reads
  the word ([`battle_damage.md`](battle_damage.md)).
- **`Battle_PartyDefenceMean()`** (no twin; `Battle_BaseDamage`): the
  members' u16 `+0xA6` summed to 16 bits over the count, divided by it, plus
  u16 0x939F86, halved toward 0. The original adds each word into `esi`
  without clearing its upper half (the caller's), which the `and eax,
  0xFFFF` after the loop makes unobservable.
- **`Battle_SetHpChange(actor)`** (PSX `0x801DCEC0` [call-anchored];
  `BattleStep_Status80`): with status 0x80 the HP change word (`+0x128` a
  member, `+0x108` an enemy) is `((hp * 10) / 10 + 5) / 10` - a tenth,
  rounded - of HP `+0x98` / `+0xA4`.
- **`Battle_ReloadPartyRecords()`** (PSX `0x801DD054`, **the sibling's name**
  `Battle_ReloadPartyRecords`, read the same; `BattleEnd_Finish`): each
  member's `+0x80` from its character record (`+0x148`), 0x29 dwords.
- **`Battle_OrderPushFront(actor)`** (no twin; `Boss34_Event`): the turn
  order's front `0x904AE2` one down, the actor put there (`0x904ACC + it`).
- **`AutoBattle_FillCommands()`** (PSX `0x801DD264`, **the sibling's name**,
  read the same; `Battle_RoundStart`): from the count of commands chosen
  (`0x904AC3`, signed, below 3) the members by entry order (`0x904AB6`) up to
  a 0xFF, each given command 3 (`+0x124`) with `+0x125` = 1, `0x904AAE` the
  last; the count stored after each.
- **`Battle_TurnVectorC` / `_TurnVector18(record)`** (PSX `0x801DD318`
  [call-anchored], `0x801DD394` [gap31]): a record's pair of dwords
  (`+0xC / +0x10`, `+0x18 / +0x1C`) turned by its direction byte `+8`: 1
  (x, z) to (-z, x), 2 to (-x, -z), 3 to (z, -x). The first has 73 callers
  (the spells' tasks, BE2 / BE3's action code).
- **`Battle_MemberReactRoll()`** (PSX `0x801DD410` [call-anchored]; BE3's
  `0x441D80`, which sets round flag 0x40 on a 1): the flag already set -
  cleared, 0. Otherwise 0 for `Field_State`'s `+0x130` bit 0, an acting
  actor below 3, `+0x90 & 0x4864`, action kind 4 with ability 0xA1, or
  `0x904B8E` without `+0x134` bit 0x10; 1 for `+0x130` bit 15; else `+0xB9`
  above `Rand() % 100`. What the reaction is in play was not read.
- **`Battle_WriteBackMember(member)`** (PSX `0x801DD874`, **the sibling's
  name**, read the same; `Battle_WriteBackParty` for 0, 1, 2): a present
  member's `+0x98`, `+0x9A`, `+0x9C` into its character record's `+0x18`,
  `+0x1A`, `+0x1C`, its `+0x90` kept to 0x60A0 and copied to the record's
  `+0x10`.
- **`Battle_PickEnemyTarget()`** (PSX `0x801DD940` [gap44];
  `Battle_MemberAutoTarget`'s four calls, `battle_sprites.cpp`'s "no target"):
  `0x904B44` = among enemies not out, those with the highest `+0x98`, the one
  with the lowest HP (the last of equals); 0 if none.
- **`Battle_WakeRoll(actor)`** (PSX `0x801DDC7C` [gap44]; the four wake
  steps): the status counter (`+0x12D` / `+0x10D`): 0 at 0; 1 when 75 (from 3
  on) or `0x64E3E8`'s byte for 1 and 2 is not below `Rand() % 100`; else 1
  when (200 - `0x64E3E0`'s byte by `+0xB6` / `+0xC6`) x 50 is not below
  `Rand() % 10000`.
- **`Battle_ReturnItem(slot, item)`** (PSX `0x801DDE44` [call]; seven callers,
  `Escape_Begin` among them): an item word given back to its inventory slot -
  al 1 when the count there is already 99, else one more and the id set, al 0.
- **`BattleEnd_EnterStep1` / `2` / `3`** (no twin; round eleven's raw
  `0x446DE0` / `0x446E00` / `0x446E20`, the boss end hooks' tail jumps):
  phase 5 at `BattleEnd_Steps`' step 1 (the win), 2 (the other way out) or 3,
  `0x904AA2` = 0. `Escape_End` makes the same step 3 itself.

### 1.3 The item command's states 5..9: a side, and the equipment window

`BattleItemCmd_Dispatch` (ours, [`battle_menu_states.md`](battle_menu_states.md))
runs state `0x904AA3` through `BattleItemCmd_States`; round eight took states
0..4. States 5..9 are this group's and Capcom's part-2 dispatchers:

| State | Entry | By `0x904AA4` through | Entries |
|---|---|---|---|
| 5 | `0x448B80` (unowned) | `BattleItemCmd_SideSteps` `0x64E4A0` | `SideBegin`, `SidePick`, `BattleItemCmd_Commit` (ours), `0x448B40` (unowned) |
| 6 | `0x448C80` (unowned) | `BattleItemCmd_EquipSteps` `0x64E4B0` | `EquipMenu`, `EquipSlotPick`, `EquipItemPick`, `EquipUsePick` |
| 7 | `EquipUseKind` | - | - |
| 8 | `EquipTargetDispatch` | `BattleItemCmd_EquipTargetSteps` `0x64E4C0` | `EquipTargetBegin`, `EquipPickEnemy`, `EquipPickMember`, `EquipCommit`, `EquipCancel` |
| 9 | `EquipSideDispatch` | `BattleItemCmd_EquipSideSteps` `0x64E4D4` | `EquipSideBegin`, `EquipSidePick`, `EquipCommit`, `EquipCancel` |

The counts are the code's: the entries set the sub-states 0..3, 0..3, 0..4,
0..3 (the tool reads `0x64E4C0` as nine entries, running on into
`0x64E4D4`). State 5 is the list item whose flag byte (`0x591810`) has bit
0x10: `SideBegin` targets a whole side (bit 0x20 the enemies 0x40, else the
party 0x80), `SidePick` flips it with any direction when bit 0x80 allows.
State 6 is entered from the list (round eight's `BattleItemCmd_List`: "up
at row 0 opens record 21 and state 6"). "Equip" in the names is a reading of
the code, not of the game: the six bytes the window shows are a character
record's `+0x12..+0x17` ([`char-stats.md`](char-stats.md) section 2 calls
them the equipment bytes), and the window takes one out of the inventory and
puts the old one back.

- **`EquipMenu`** (PSX `0x8009632C` [callers]): system message 0x2F / 0x31
  by record 21's option `+0xA` into the last queue entry, the hand beside
  the option; 0xA000 flips the option; cancel or down (0x4000) back to the
  list; confirm: option 0 `BattleEquip_OpenUse` and sub-state 3 (`EquipUsePick`),
  option 1 `BattleEquip_OpenChange` and sub-state 1.
- **`EquipSlotPick`** (PSX `0x80096530` [callers]): the six slots (record
  18, cursor `+0xA` wrapped 0..5), message 0x30 / 0x32; cancel restores the
  list; button 0x10 takes the slot's item off (`BattleEquip_RemoveSlot`, cue
  0x103, or 0x107 when it could not); confirm picks the slot and opens the
  candidates (sub-state 2).
- **`EquipItemPick`** (PSX `0x800967F8` [callers]): the candidates (record
  17: cursor `+0xB`, top `+0xA`, pages of 7, capped at 0x7F / 0x79, scroll
  requests in word `+0x10`), the candidate's help line queued,
  `BattleEquip_Preview` each frame; confirm with an item puts it on
  (`BattleEquip_Apply`); cancel or a change back to the slots.
- **`EquipUsePick`** (PSX `0x80096B3C` [gap86]): the six slots for use;
  confirm with record 18's word `+0x10` set: the member's `+0x130` bit
  0x4000, its `+0x126` and the command's word `+2` that word, the help line
  queued, state 7; without it cue 0x107.
- **`EquipUseKind`** (state 7, PSX `0x80096E48` [gap86]): round eight's
  `BattleItemCmd_TargetKind` for the member's `+0x126` item instead of the list
  item - bit 0x40 to state 8 or 9 at sub-state 0; bit 0x80 every target,
  state 8 sub-state 3 (the commit); else a side or the member itself.
- **`EquipTargetBegin`** (PSX `0x80096F94`, the sibling's hypothesis name
  `Item_TargetSetup`, read the same), **`EquipPickEnemy`**,
  **`EquipPickMember`**, **`EquipSideBegin`**, **`EquipSidePick`**
  ([gap86]): round eight's `BattleItemCmd_TargetBegin`, `_PickEnemy`,
  `_PickMember` and this group's `SideBegin` / `SidePick`, on the member's
  item word.
- **`EquipCommit`** (PSX `0x80097430` [gap86]): `BattleItemCmd_Commit`
  without spending an item: cue 0x104, command `+1` = 5, the member's `+1` =
  2, one more command chosen, the queue entry's `+1` = 1,
  `ItemMenu_FreeWindows`, back to step 1.
- **`EquipCancel`** (PSX `0x800974E0` [gap86]): as `0x448B40` for the list
  item: cue 0x106, the member's name in the banner, the list back at
  `+3` = 2 and x 0xFF38, `0x904AA1..4` = 4, 2, 1, 0.
- **`BattleEquip_OpenChange` / `_OpenUse`** (PSX `0x80098004`, `0x80098128`
  [gap89]): records 18 (and 17 for a change) set up, `0x8033F4` the member,
  record 18's dword `+0x20` the preview bytes `0x675F18`.
- **`BattleEquip_Preview`** (PSX `0x80097AB0` [call-anchored]): record 18's
  `+0xD` = 1 when `Item_EquipMask(category, candidate)` has no bit for the
  character (`1 << index`, an 8-bit shift); the six preview bytes the
  character's, the one at the slot cursor the candidate.
- **`BattleEquip_Apply`** (PSX `0x8009770C` [callers]): each preview byte
  set and different from the character's: `Inventory_Remove(0x64E4E4[slot],
  new, 1)`, `Inventory_Add(.., old, 1)`, the slot set. Then
  `Char_RecalcStats`, and the members refreshed: `+0x92..+0x97` and the eight
  dwords `+0xC0` from their records, `Formation_ApplyStatMods`, `+0xC0` into
  `+0xA0` for the present, BE5's `0x44FDE0`, BE6's `0x453300` per member.
- **`BattleEquip_RemoveSlot`** (PSX `0x80097BE0` [gap89]): the slot at the
  cursor taken off (not slot 0, not an empty one): `Inventory_Add(0x64E4EC[slot],
  item, 1)`, the slot cleared, the same refresh; al 1.
- **`ItemMenu_SetupForMember`** (PSX `0x80094FB0` [call-anchored]; BE5's
  four Dragon-run steps `0x450200`, `0x450680`, `0x450A70`, `0x450EF0`):
  record 16 as a list with the menu actor's `+5` as its category byte, the
  hand off. The plan's "target picker `0x447F40`" is this; it picks nothing.

### 1.4 The escape

`0x44A000` (Battle_MenuSteps entry 6, unowned) runs `0x904AA3` through
`Escape_States` `0x64E4FC`: 0 `Escape_Roll`, 1 `Escape_FailDispatch`
(`Escape_FailSteps` `0x64E508`: `Escape_Begin`, `_StepBack`, `_StepOn`,
`_Failed`), 2 `Escape_WinDispatch` (`Escape_WinSteps` `0x64E518`:
`Escape_Begin`, `_Leave`, `_End`). The paths' names are the code's ends:
path 1 ends in `Escape_Failed` (system message 0xF, back to phase 3), path
2 in `Escape_End` (phase 5, step 3); the sibling's names for four of them
(`Escape_Roll`, `Escape_Begin`, `Escape_Failed`, `Escape_Chance`) say the
same, and were transferred after reading each.

- **`Escape_Roll`**: the attempts `0x904AE6` counted; an event battle
  (`0x904AAA` set) takes path 1 (state + 1); from the third attempt path 2;
  else `Escape_Chance(party mean +0xA8 - enemies' mean +0xB8)` against
  `Rand() & 0x3F`: path 2 when the roll is not above it, else path 1 (path 2
  with `0x904AE4` == 1).
- **`Escape_Chance(diff)`**: row `(diff + 0x20) / 16`: below 0 0x1C, above 5
  0x38, else `0x64E524`'s; + 8 on the second attempt; + 8 once for a member
  not out with `+0x91` bit 0x20; at most 0x40.
- **`Escape_Begin`**: banner of the text at `0x669E08`; window 4's `+3`;
  `MoveScript_F3Divisor` 0x20; the field kind-2 x / z high words stepped by
  `0x64E4F4`'s pair for `0x904AAC`; on path 2 `MoveScript_FAWord` = the
  elevation there less `MapView_Elevation`, >> 5 (16-bit), else 0;
  `LoadDatFile(0xD1)`; the members with a queued item command (5) given
  their items back (`Battle_ReturnItem`); the members not out turned (`+8 ^
  2`) with their action bytes cleared.
- **`Escape_StepBack` / `_StepOn`** (path 1): unless `Field_Kind2Hold`, the
  divisor 0x80 / 0x40 and the step taken back / again.
- **`Escape_Failed`**: the turn order rebuilt, the members turned back, their
  queued items returned and their turns removed, system message 0xF, round
  flag 8, phase 3.
- **`Escape_Leave`**: the elevation set, every enemy's tint released,
  `0x494E70`, system message 0x10. **`Escape_End`**: once `0x904AE9` is 0,
  phase 5 at step 3.

### 1.5 Text

- **`Battle_MemberNameToText(member)`** (PSX `0x801DE964`) and
  **`Battle_EnemyNameToText(actor)`** (PSX `0x801DE9D4` [gap20]): 8 bytes of
  the member's character record (by `0x66972C[+0x89]`), 12 of the enemy's
  working record, into `Text_Records[0]` (`Str_CopyN`).
- **`BattleBanner_AddLine(row, column)`** (PSX `0x801DEB80`; BE3's
  `0x4424A0`): `BattleBanner_Add(2, 0, 0, 0x2D, Msg_SystemPtr(u16
  0x64E52C[row x 10 + column]))`.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original divides by a count that can be 0
(`Battle_PartyDefenceMean` by the party count, `Escape_Roll` by `0x904AB3`
and `0x904AB1`), indexes its six stack pointers past the sixth
(`BattleEquip_RemoveSlot`) or jumps through a table past its end (the four
dispatchers), ours aborts with a `Fatal` naming the function (the owner's
rule, round9 doc section 6). Two words ours passes differ in bits no callee
reads: `BattleEquip_Apply` / `_RemoveSlot` push a fourth word 0 to
`Inventory_Add` / `_Remove`, which take three (ours passes three), and
`BattleEquip_RemoveSlot`'s word to `0x453300` has an uninitialised stack
dword above its low byte (ours passes the byte; `0x453300` reads the byte
and hands the word to `0x453560`, whose first act is `and eax, 0xFF`, read
2026-09-29).

## 3. Calls across groups

**Out of BE4, raw** (`battle_e4_callees.h`): BE5's `0x44FDE0` and BE6's
`0x453300` (from `BattleEquip_Apply` and `_RemoveSlot`; the band tool's
edges, [`boss_harness.md`](boss_harness.md) 10.6); nobody's `0x591810` (the
item flag byte) and `0x494E70` (the enemies' state bytes zeroed), both
standard rows by address. Everything else is ours and called by name.

**Into BE4 from outside the group** (the rebinding pass's list):

| Caller | Callee | Caller's owner |
|---|---|---|
| `0x42EE00` | `BattleWin_DimScreen` | BE1 |
| `0x42EF50` | `Battle_ReturnItem` | BE1 |
| `0x42FE20`, `0x431C10` | `Battle_MemberNameToText` | BE1 |
| `0x436290`, `0x436BE0` | `Battle_TurnVectorC` | BE2 |
| `0x437260` | `Battle_TurnVector18` | BE3 |
| `0x441D80` | `Battle_MemberReactRoll` | BE3 |
| `0x441ED0` | `Battle_TurnVectorC` | BE3 |
| `0x4424A0` | `Battle_MemberNameToText`, `BattleBanner_AddLine` | BE3 |
| `0x450200`, `0x450680`, `0x450A70`, `0x450EF0` | `ItemMenu_SetupForMember` | BE5 |
| `0x433300`, `0x433350` | `BattleWin_DrawTileTint` | nobody (Capcom's) |
| `0x447290` | `Battle_PrevTarget` | nobody (Capcom's, unnamed) |
| `0x448B80`, `0x448C80`, `0x44A000` | the state tables' entries | nobody (part 2) |
| ours (section 9) | 19 of the 56 by constant | rebound |

The BE groups' callers are in files those groups are writing this wave:
they call BE4 raw, and the coordinator rebinds after the merges.

## 4. Named data (`symbols.toml` `[[data]]`)

Seven tables, counted from the code (the entries' own stores): 
`BattleItemCmd_SideSteps` `0x64E4A0` (4), `BattleItemCmd_EquipSteps`
`0x64E4B0` (4), `BattleItemCmd_EquipTargetSteps` `0x64E4C0` (5),
`BattleItemCmd_EquipSideSteps` `0x64E4D4` (4), `Escape_States` `0x64E4FC`
(3), `Escape_FailSteps` `0x64E508` (4), `Escape_WinSteps` `0x64E518` (3).
The byte tables between them (`0x64E4E4` and `0x64E4EC`, the six slots'
inventory categories; `0x64E4F4`, the escape's s8 steps; `0x64E524`, the
chance row; `0x64E52C`, the banner lines) are read in place and named in
`battle_e4_callees.h` only.

## 5. The fuzz

`BOF3X_SHADOW=battle_e4`, `src/game/battle_e4_fuzz.cpp`: one `Run`,
`Group::engine`, 6,000 rounds a clone (`BOF3X_BE4_RUN=<hex base>,...` runs
some alone). The clone rows are the band tool's (`--clones`), each read
against the disassembly.

- **Shapes.** 31 `kHelper` (the directly called ones; `ret_mask` 0xFF for
  al, 0xFFFF for the two hit rolls, 0xFFFFFFFF for the defence mean); 21
  `kStep` with their state byte drawn before the seed (`0x904AA4` below 8;
  `0x904AA3` below 10 for `EquipUseKind`, below 4 for `Escape_Roll`); four
  `kDispatch` by `state_cell 0x904AA4` below their tables' lengths, over
  the four `DataTable`s. Four entries run a second time through the
  dispatcher the game reaches them by (`Clone::via`, `Via::state_cell`):
  `SideBegin` through `0x448B80`, `EquipSlotPick` through `0x448C80`,
  `EquipUseKind` through ours `BattleItemCmd_Dispatch` (`0x64E45C` entry
  7), `Escape_Roll` through `0x44A000`.
- **Callees beyond the standard set**: the group's own functions its others
  call directly (`BattleEquip_OpenUse`, `_OpenChange`, `_RemoveSlot` (flag),
  `_Preview`, `_Apply`, `Battle_PrevTarget` (a byte 0xFF..10),
  `Battle_ReturnItem` (slot a byte, item a short), `Escape_Chance` (a byte
  0..0x40)); BE5's `0x44FDE0`; BE6's `0x453300` (a byte); and three standard
  callees listed again with narrower masks - `Inventory_Add` and
  `_Remove` (all three words a byte: the originals push the category and the
  items with the upper bytes of whatever the register held, a pointer or the
  last callee's answer, and both callees read the low bytes -
  `char_stats.cpp`, `scena_sx.cpp`) and `Battle_ReturnQueuedItem` (a byte:
  `Escape_Failed` pushes a local whose upper bytes are the caller's `ecx`;
  the callee reads the low byte, `battle_setup.cpp`). Their standard rows
  (`kAll`) would compare that garbage.
- **Regions beyond the engine frame**: the character records 1..7
  (`0x903B24..0x903F90`), `0x904060..` (the members' character bytes),
  `0x66972C` (0x18), the preview bytes, the tint, `MoveScript_FAWord`,
  `Field_Kind2Z` / `X`, the auto-repeat latch, the inventory's id and count
  lists (`0x904154..0x9045FC`), and `0x93C320..0x93C6E0` (the message
  queue's tail, and what `Escape_Roll` reads past the eight enemy objects).
- **Seeds** (`Seed`, every round): the indices the functions go through kept
  inside what the regions hold - each member's `+5` (its party index, which
  the menu actor's code indexes the party by), `+0x148` / `+0x89` (0..7),
  `0x904065..67` and `0x66972C[0..7]` (0..7), record 18's slot cursor (0..5)
  and member (0..2), record 16's category (0..4); the party count 0..3 (1..3
  for the defence mean), both escape divisors at least 1; the buttons -
  cancel and confirm two distinct single bits, `Input_Pressed` one of the
  branches' values (0, either button, 0x10, the four directions, 0xA000, the
  page bits 4 / 8, garbage); the windows' bytes at their ends (the option,
  the `+3` bytes, record 17's top and cursor at 0, 1, 6..8, 0x72, 0x73, 0x78,
  0x79, 0x7E..0x80, record 18's word `+0x10` zero half the time). Per
  function: the round flags 0x80 / 0x40 and the percentages at 0, 1, 49, 50,
  98..100, 0xFF (the hit rolls, the reaction roll with its other five exits);
  the entry order and count (`AutoBattle_FillCommands`); the turn-order
  front 1..0x20 (it writes `0x904ACC` + front - 1); the direction byte of both
  harness records (the vector turns); small `+0x98` and HP words so the
  enemy target pick has ties; the status counters 0..4, 0xFF; counts of 99
  in the lists (`Battle_ReturnItem`); preview bytes none / equal / other
  (`Apply`); an empty slot and slot 0 (`RemoveSlot`); any character byte a
  third of the time for `Preview` (it only reads the record: the mask's bit
  past 7); the fight byte 0 half the time and the attempts 0..3, 0xFF
  (`Escape_Roll`); the second attempt (`Escape_Chance`); path 2 and the
  command-5 members (`Escape_Begin`); `Field_Kind2Hold` and `0x904AE9` 0 two
  times in three.
- **Args**: actors inside their sides where the originals index by them
  (garbage above the byte half the time); the pace's average and highest at
  the enemy's `+0xB8` / 2 and `+0xB8` +- 1; a harness record for the vector
  turns; an item word with category 0..3; `Escape_Chance`'s difference at
  each row's edges (-0x21, -0x20, -0x11, -0x10, -1, 0, 0xF, 0x10, 0x3F, 0x40,
  0x50, 0x5F, 0x60, 0x70, 0x7FFFFFFF); the banner line's row 0..7 and column
  0..9 mostly.
- **`Settle`** (after every disturbance): the cells a function reads again
  after a call put back inside their tables - record 18's cursor and member,
  record 16's category, each member's `+5` (the engine disturbance moves
  bytes of the current window record and of `Sprite_Current`, which may be a
  member). **`Disturb`** (case 14): a member's `+5` or item word, record
  18's word `+0x10`, record 17's item, record 21's option, the party count,
  `Field_Kind2Hold`.

In this worktree:

    shadow      battle_e4 self-test: 360000 rounds over 60 functions (6000 each), 716922 calls to the stand-ins, 0 MISMATCHES; 39304 bytes of state (37 regions) and the stand-ins' log compared

Every callee listed was called by the originals (the coverage lines), every
table entry reached (the `phase 0x...` counts: each of the four tables'
entries between 1,156 and 3,457 calls).

## 6. Controls

CONTROLS_TABLE

## 7. Latent defects (Capcom's, kept)

- **`Escape_Roll` sums the wrong enemies' `+0xB8`.** It asks
  `Battle_ActorIsOut` of actors 3..10 but reads the word at `0x93BD90 + k x
  0x128` - object `actor`, not `actor - 3`: objects 3..10, of which 8..10
  lie past the eight enemy objects (`0x93C2A0..`, the message queue and the
  memory after it). The enemies' mean the roll compares with is therefore
  that of objects 3..7 and three words that are no enemy's, over the enemies
  left. Measured by reading only; what the words hold in a battle is not.
- **Unchecked divisions**: `Battle_PartyDefenceMean` by the party count,
  `Escape_Roll` by `0x904AB3` (enemies left) and `0x904AB1`. A zero faults
  the original (ours aborts); whether play can reach one was not measured.
- **`BattleEquip_RemoveSlot` indexes six stack pointers by the slot cursor
  unchecked** and `BattleEquip_Preview` writes `0x675F18 + cursor`: every
  writer of the cursor keeps it 0..5 (`EquipSlotPick`, `EquipUsePick`, the
  two opens), so neither is reachable from these functions alone.
- **`Battle_PickEnemyTarget` keeps the highest `+0x98` as a byte** and then
  compares each enemy's word with it: an enemy whose `+0x98` is 0x100 or more
  never matches, and a word above 0xFF can lower the byte (0x100 is kept as
  0). Harmless while the words stay below 0x100.
- **`BattleEquip_Preview`'s bit is an 8-bit shift**: a character index of 8
  or more always reads "cannot equip".
- **`Battle_ReturnItem` indexes the list pointers by the category unchecked**:
  category 4's count pointer is 0 (a write near address 0); callers pass the
  item words they stored.
- **`Battle_OrderPushFront` has no floor**: at `0x904AE2` = 0 it writes
  `0x904ACC + 0xFF`, past the battle bytes.
- **`AutoBattle_FillCommands` and `Battle_PrevTarget` compare signed bytes**:
  a count or actor of 0x80 or more is negative to them (a fill loop from a
  negative index; a search from 10 down through 0xFF).
- **The four dispatchers are unchecked** (`jmp [table + 4 x byte]`); ours
  aborts past each table.
- **`Escape_Begin` gives back the items of all three records** whose
  command is 5, whatever the party count.

## 8. What nothing reached

Every function and every table entry was reached by the fuzz, and every
branch a control was planted in. What only play would show: which item
commands reach states 5..9 (a whole-side item, the equipment window), the
escape's paths on screen, the reaction roll's meaning, and the words
`Escape_Roll` reads past the enemies.

## 9. The rebinding

Every raw constant in our source naming one of the 56 now reads
`bof3::addr::<Name>`, the value unchanged (the round-ten form: the fuzz
files' keys and the game's calls are the same; each file says so beside its
include): **70 constants in 42 files** - the boss groups' `kEndWin` /
`kEndOther` / `kEndThird` (`boss_h`, `boss_sa`..`boss_sj` callees headers:
`BattleEnd_EnterStep1..3`), `boss_sh`'s `kOrderFront`, the 23 spell files'
`kTurnOffset` / `kTurnByFacing` (`Battle_TurnVectorC`),
`battle_damage.cpp`'s three (`Battle_HitOrMissParty`, `_HitOrMissEnemy`,
`Battle_PartyDefenceMean`), `battle_setup_callees.h`'s six,
`battle_turn_steps_callees.h`'s two, `battle_phases_callees.h`'s two,
`battle_actor_copies_callees.h` / `battle_menu_states_callees.h`'s
`kPrevTarget`, `battle_fx_tasks_callees.h`'s `kAfterAreaScript`,
`battle_sprites.cpp`'s `Battle_PickEnemyTarget`.

**Left raw, on purpose**: the fuzz files' `CallSite` rows and stand-in key
tables (the addresses the disassembly shows, checked by `CloneOriginal`);
`boss_harness.cpp`'s three standard rows `0x446DE0` / `0x446E00` /
`0x446E20` (not this group's file: the harness's `BH_OURS` form and the boss
files' `BH_CALL` form change the fuzz's keys together, the coordinator's
fold); `boss_harness_eh.cpp`'s `0x4457F0` (EH's self-test runs Capcom's
bytes on both sides). **For the coordinator**: BE1, BE2, BE3 and BE5 call
BE4's functions raw from the files they are writing (section 3).

## 10. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): the 25 hidden starts'
extents of the table above, and `00449FE0 20` (`ItemMenu_FreeWindows`' own
extent; the line `00449FE0 540` ran over `0x44A010..0x44A51F`). The 31
directly called functions already had lines with these extents (`00447F40
90` with the padding).

## 11. The live route

`tools/recipes/dragonTransform.txt` (the owner's, 2026-09-28) enters nine of
the 56 (`analysis/calltrace/reach_dragon/reach_dragon_new.txt`, recipe
frame and caller): `ItemMenu_SetupForMember` (1,678, BE5's `0x450510`),
`Battle_PartyDefenceMean` and `Battle_HitOrMissParty` (1,749, the damage
chain), `Battle_MemberReactRoll` (1,769), `Battle_EnemyOutpaces` (2,141),
`Battle_HitOrMissEnemy` (2,419), `Battle_MemberNameToText` (4,149, the
result screen), `Battle_WriteBackMember` (4,192) and
`Battle_ReloadPartyRecords` (4,194). The plan's section 5 row for BE4 lists
six more (`0x4468B0`, `0x446990`, `0x4469D0`, `0x446F20`, `0x446F50`,
`0x446F80`): part 2 / 7 rows no group owns this round, not counted here.
The coordinator's live check after the wave runs the route.

## 12. For the coordinator: what the harness lacked

Nothing it could not do through the group's own listings; three things a
fold would make standard:

- `Inventory_Add` / `Inventory_Remove`'s standard masks (`kAll` for the
  category and count) compare bits the callees do not read: BE4 lists both
  with byte masks, as any engine caller that pushes a byte from a partly
  written register will need.
- `Battle_ReturnQueuedItem`'s standard mask (`kAll`) likewise (the callee
  reads the low byte).
- The raw standard rows `0x446DE0` / `0x446E00` / `0x446E20` can become
  `BH_OURS(BattleEnd_EnterStep1..3)` together with the boss files' calls by
  name, in one commit and a `'*'` run (round-10-cleanup's form).
