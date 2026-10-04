# Group R3C: Effect_Handlers slots 50..85 and 87..111

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, on
the round branch's tip `7f116a2`. **61 functions ours**
(`src/game/rest_3c.cpp`; the cells and raw callees in
`src/game/rest_3c_callees.h`; prototypes from `symbols.toml`; shadow name
`rest_3c`): the cut's 60 rows for R3C (`analysis/round14_cut.tsv`) and
**one start no list had**, `0x44D8B0` (slot 91). Each read to its last
instruction with capstone and fuzzed through the boss harness's engine frame
([`boss_harness.md`](boss_harness.md) section 10), used unchanged: one `Run`,
366,000 rounds (6,000 a function), 0 mismatches. CONTROLS_SUMMARY
Fuzz-only: no recorded route enters any of the 61 (section 8).

The band is one thing: **61 consecutive slots of `Effect_Handlers`**
(`0x64E73C`, 130 entries, named in round twelve), the table
`Effect_ApplyResult` (ours, `battle_damage.cpp`) calls with no argument
through `0x64E540[ability]` (acting kind 4) or `0x64E72C[category][item]`.
Slot 86 is `BareRet` (`0x437CC0`, BH's); slots 50..85 and 87..111 are
this group's. A slot reads the actor `0x904B34` and the target `0x904B54`
(0..2 a member of `ObjTrio`, stride 0x14C; 3..10 the enemy object `t - 3`,
`0x93B960`, stride 0x128) and fills the result record `*0x904B60` (+4 the HP
delta, positive damage; +6 the AP delta; +8 flags), or changes a record
directly. **Every name is a hypothesis from the code's shape** (`symbols.toml`
status `hypothesis`): which ability or item reaches a slot was not traced,
and "heal", "inflict", "status" name what the arithmetic and the callees do,
not a play-tested fact. `analysis/pairs_propagated.json` pairs none of the 61
with a PSX address, so no sibling name was transferred.

## 1. What each function does

Every function's comment in `rest_3c.cpp` is the full read and each
`symbols.toml` evidence string cites its instructions. "R" is the result
record, "MissTail" R3D's `0x44FB30` (the effect's common tail: `0x904AA9 |=
0x20`, the target's `+0x130 |= 0x200`, its `+0x12C` bit 0 cleared),
"Resisted" R3D's `0x44F6A0(actor, target)` (al: the target resisted).

### 1.1 An ability by its number (slots 50..61, 83)

Each stores `0x904B35 = 4` and the word `0x904B80` = the ability, then tail-
jumps to one of R3B's bodies and answers its eax.

| Slot | Address | Name | Ability | Then |
|--:|---|---|--:|---|
| 50 | `0x44D000` | `Effect_AsAbility60` | 0x60 | `jmp 0x44C040` (R = `Effect_SkillDamage(actor, target, the ability's power, 0)`) |
| 51 | `0x44D020` | `Effect_AsAbility68` | 0x68 | `jmp 0x44C040` |
| 52 | `0x44D040` | `Effect_AsAbility5B` | 0x5B | `jmp 0x44C040` |
| 53 | `0x44D060` | `Effect_AsAbility52RaiseStat1` | 0x52 | `0x44FBB0(1)` (unless resisted, R record byte +0x15 raised by the ability's power; `Battle_RecalcStats`); al |
| 54 | `0x44D080` | `Effect_AsAbility5ARaiseStat1` | 0x5A | `0x44FBB0(1)`; al |
| 55 | `0x44D0A0` | `Effect_AsAbility5C` | 0x5C | `jmp 0x44C040` |
| 56 | `0x44D0C0` | `Effect_AsAbility64` | 0x64 | `jmp 0x44C040` |
| 57 | `0x44D0E0` | `Effect_AsAbility57Inflict10` | 0x57 | `0x44FC60(0x10)` (MissTail, then unless resisted `0x44F1D0(target, 0x10)`); al |
| 58 | `0x44D100` | `Effect_AsAbility61` | 0x61 | `jmp 0x44C040` |
| 59 | `0x44D120` | `Effect_AsAbility46` | 0x46 | `jmp 0x44C120` (R +4 = `Effect_HealAmount(actor, target)`) |
| 60 | `0x44D140` | `Effect_AsAbility67` | 0x67 | `jmp 0x44C040` |
| 61 | `0x44D160` | `Effect_AsAbility56` | 0x56 | `jmp 0x44CF60` (MissTail, the target's +0x134 / +0x114 \|= 0x200) |
| 83 | `0x44D680` | `Effect_AsAbility63` | 0x63 | `jmp 0x44C040` |

### 1.2 Damage forms

| Slot | Address | Name | What |
|--:|---|---|---|
| 62 | `0x44D180` | `Effect_DoubleDamage` | `0x904AA9 \|= 0x20`, `0x939F86 = 0`; R +4 = `Battle_CalcDamage(actor, target, 0xFFFF)` << 1 (16 bits); byte `0x904660 = 5` |
| 63 | `0x44D1C0` | `Effect_QuarterDamage` | kind 4 with ability 0x9A: `0x904AA9 \|= 0x20`; `0x939F86 = 0`; R +4 = that damage / 4 (signed, truncated) |
| 64..66 | `0x44D220`, `0x44D240`, `0x44D260` | `Effect_ActorHpDamage3`, `_1`, `_2` | R +4 = R3D's `0x44FCE0(3 / 1 / 2)` (the actor's HP over the divisor, varied by `Rand` and the element); eax forwarded |
| 68 | `0x44D290` | `Effect_DamageInflict40` | the target's HP and status word read first; R +4 = `Battle_CalcDamage(actor, the target's dword, 2)`; the delta (read again) below the HP, status bit 6 clear, not 0: `0x44FCA0(0x40)` |
| 73 | `0x44D390` | `Effect_Attack80` | the attacker's power `0x939FE4` = the actor's +0xA4 / +0xB4, then x 80 / 100; R +4 = `Battle_CalcDamage(the actor's dword, target, 0xFFFF)` |
| 82 | `0x44D630` | `Effect_AsAbility66Half` | ability 0x66's power byte; kind 4, ability 0x66; R +4 = `Effect_SkillDamage(actor, target, power, 0)` / 2 |
| 88 | `0x44D770` | `Effect_AttackNoKill` | R +4 = `Battle_CalcDamage(...)`; when not below the target's HP (signed words), HP - 1 |
| 89 | `0x44D7E0` | `Effect_HalveTargetHp` | unless resisted, R +4 = the target's HP >> 1 |
| 91 | `0x44D8B0` | `Effect_DamageAllHp` | resisted: R +8 = 1; else R +4 = the target's HP |
| 92 | `0x44D920` | `Effect_RandomPowerAttack` | `0x939FE4` = the actor's power, then by `Rand & 3` halved / kept / x 1.5 / x 2 (16 bits); R +4 = `Battle_CalcDamage(...)` |
| 94 | `0x44DA70` | `Effect_SureHitHalfDamage` | the actor's status bit 3 held aside and cleared; `0x939FFC = 100`; R +4 = damage / 2; the actor (read again) gets the bit back |
| 99 | `0x44DCC0` | `Effect_ActorHpToDamage` | `0x904AA9 \|= 0x20`; R +4 = the actor's HP - 1, the actor's HP = 1 |
| 100 | `0x44DD60` | `Effect_DamageToOneHp` | unless resisted, R +4 = the target's HP - 1 (0 at HP 1) |
| 103 | `0x44DE20` | `Effect_MultiHit` | `0x904B96` hits (read again each pass): sum of `Battle_CalcDamage(...)` x f / 10, f the s8 `0x64E944[i]` (Capcom's .data, read in place) for i < 7, else 3; held to +-9999 |
| 104 | `0x44DEF0` | `Effect_SkillDamageLessDef` | `0x44C040`'s body, then less the target's +0xA6 / +0xB6, held at 0 |
| 105 | `0x44DF80` | `Effect_HalfDamageInflict20` | R +4 = damage / 2; not 0: `0x44FCA0(0x20)` |

### 1.3 Status, flags, the party's stats

| Slot | Address | Name | What |
|--:|---|---|---|
| 67, 69, 70 | `0x44D280`, `0x44D330`, `0x44D340` | `Effect_InflictStatus4`, `8`, `80` | `0x44FC60(4 / 8 / 0x80)`; al |
| 71 | `0x44D350` | `Effect_InflictStatus20` | MissTail, `0x44FCA0(0x20)`; al |
| 74 | `0x44D420` | `Effect_FlagActorPair` | `0x904AA9 \|= 0x40`; `0x904B8A` = the actor, `0x904B8B` = byte `0x904B44`; `jmp 0x44FB30` |
| 75 | `0x44D450` | `Effect_TargetRecalcParty` | MissTail; a member target only: `Char_RecalcStats(its character record)`, then `BattleParty_RecalcStats`' body after its first call (the record bytes into each member, `Formation_ApplyStatMods`, +0xC0 to +0xA0, `BattleForm_ApplyStats`, `Battle_RecalcStats` per member) |
| 76 | `0x44D580` | `Effect_Inflict40Heal20Ap4` | `0x44F1D0(target, 0x40)`; R +8 \|= 2, +4 = -20, +6 = -4 |
| 84 | `0x44D6A0` | `Effect_CureBFCHealMax` | `Battle_ClearStatus(target, 0xBFC)`, `jmp 0x44C170` (R +4 = -(a member's maximum HP)) |
| 87 | `0x44D6D0` | `Effect_EnemyRaiseAAAE` | MissTail; enemy (target - 3): +0xAA up 2 and +0xAE up 1, each once (marker bit 0 of +0xAB / +0xAF), each held to 7 |
| 90 | `0x44D850` | `Effect_Inflict800Unguarded` | `ObjTrio +0x134` at the target's stride bits 0/1: R +8 = 1. Else a member's +0x143 = 0; `0x44FCA0(0x800)` not 0: R +8 = 1; else MissTail |
| 93 | `0x44D9D0` | `Effect_TargetFlag2000` | resisted: R +8 = 1, `Battle_SetDamagePopup(0, target)`; else the target's +0x134 / +0x114 \|= 0x2000, `Battle_RecalcStats(target)`; both `jmp 0x44FB30` |
| 95 | `0x44DB40` | `Effect_EnemyDouble94And96` | MissTail; enemy (target - 3)'s +0x94 and +0x96 doubled, held to 0xFFFF |
| 96 | `0x44DBC0` | `Effect_DropFromTurnOrder` | R +4 = 0; resisted: R +8 = 1; else MissTail, `Battle_ReturnQueuedItem(target)`, `Battle_RemoveFromTurnOrder(target)` |
| 97 | `0x44DC10` | `Effect_FlushTurnOrder` | MissTail; `0x904B8E = 4`; `ObjTrio +0x134` at the target's stride \|= 0x10; turn-order slots `0x904AE2 - 1` .. `0x904AE3` inclusive not 0xFF: `Battle_ReturnQueuedItem(slot index)`, the slot = 0xFF |
| 98 | `0x44DCA0` | `Effect_RaiseStat4By10` | MissTail; R3D's `0x44F650(10, 4)` (R byte +0x18 + 10, held to -25..50); `Battle_RecalcStats(target)` |
| 101 | `0x44DDD0` | `Effect_HealAndCureBFC` | R +4 = `Effect_HealAmount(actor, target)`; `Battle_ClearStatus(target, 0xBFC)` |
| 106 | `0x44DFD0` | `Effect_ActorStepThenHpToOne` | a member actor: its character record's +0x1E below 5 - it and the member's +0x9E up one; `Char_RecalcStats`, the party loops of slot 75. An enemy actor with +0xB0 not 0xFFFF: maximum less a tenth of its data record's word +0x24 (when above it), +0xD0 less the same, HP held to the maximum. Then unless resisted R +4 = the target's HP - 1 |
| 107 | `0x44E260` | `Effect_ActorFlag8000` | MissTail; the actor's +0x130 / +0x110 \|= 0x8000 |
| 108, 109 | `0x44E2B0`, `0x44E330` | `Effect_ActorFlag40Count`, `Effect_ActorFlag80Count` | MissTail; the actor's +0x134 / +0x114 \|= 0x40 (0x80), its +0x144 / +0x124 (+0x145 / +0x125) up one below 2 |
| 110 | `0x44E3B0` | `Effect_ActorFlag100` | MissTail; the actor's +0x134 / +0x114 \|= 0x100 |
| 111 | `0x44E400` | `Effect_TargetClearBuffs` | MissTail; the target's +0x138 / +0x13C (+0x118 / +0x11C) = 0, +0x130 &= 0xFFFE7FFF, +0x134 &= 0xFFFBC43F; `Battle_RecalcStats(target)` |

### 1.4 Small constants (slots 72, 77..81, 85, 102)

`Effect_Heal5Ap1` (`0x44D360`: R +8 \|= 2, +4 = -5, +6 = -1),
`Effect_Heal1` / `Heal80` / `Heal240` / `Heal5` (`0x44D5C0`, `0x44D5D0`,
`0x44D5E0`, `0x44D6C0`: R +4 = -1 / -80 / -240 / -5), `Effect_RestoreAp5` /
`RestoreAp40` / `RestoreAp10` (`0x44D5F0`, `0x44D610`, `0x44DE00`: R +8 = 2,
+6 = -5 / -40 / -10).

**`eax` on return.** `Effect_ApplyResult` drops it. A slot that ends in a
tail jump or in `call; pop ecx; ret` leaves its callee's eax, and ours
answers it (23: the eleven tail jumps to R3B, `0x44D420`, `0x44D6A0`,
`0x44D9D0` and the three `0x44FCE0` calls whole - `ret_mask 0xFFFFFFFF`; the
seven status-helper calls `ret_mask 0xFF`, the helpers answer al); the other
38 are `void`.

## 2. Starts, extents, the cut

- **`0x44D8B0` is a function no list had** (slot 91, 0x6F bytes, between
  `0x44D850`'s `ret` at `0x44D8A8` and `0x44D920`); the cut's 208 bytes for
  `0x44D850` ran over it. R3D's `0x44EA70` also tail-jumps to it (at
  `0x44EA80`). Taken as `Effect_DamageAllHp`.
- **All 60 cut rows are hidden in `Effect_ApplyResult`'s catalogue extent**
  (`0x44B9F0`, 3,317 bytes in `entries.txt`). Ours of `Effect_ApplyResult`
  ends at `0x44BEC5` and calls every slot through the table cell, so each
  slot is a function of its own, reached by address (its cell).
- **The tool's extents are right for all 61**; the cut's sizes differ by
  trailing padding for 57 and by code for `0x44D850`. No start is a
  jump-table case or a shared tail: every tail jump leaves the band (to R3B's
  `0x44C040`, `0x44C120`, `0x44C170`, `0x44CF60` and R3D's `0x44FB30`), and
  no slot jumps into another. No code in the band is left untaken.
- **Tables:** none named. The only `.data` table these functions reach is
  `Effect_Handlers` itself (named, read by ours), and `0x64E944` (seven s8
  factors `Effect_MultiHit` reads with its own `i < 7` bound - a byte table,
  not a handler table, described in `rest_3c_callees.h`).
- No `DIVERGENCE.md` entry, `cheats.cpp` patch or `widescreen.cpp` operand
  names any of the 61 addresses (grepped); no harness row lists one
  (`boss_harness*.cpp`, `scenario_harness*.cpp`); none draws a full-frame fill.

## 3. Divergence and the aborts

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed, and nothing needs a ledger entry (no read of memory the original never
wrote reaches a result: section 7 lists what the original reads in the wrong
place, which ours reads identically). Ours aborts with a `Fatal` naming the
function where the original **writes** through an index past its records
(the owner's rule, round9 doc section 6): a member past the three
(`Effect_FlushTurnOrder`, the actor writers), an enemy index past 7
(`EnemyWrite`: the actor or target byte above 10, or below 3 in the two
enemy-only slots 87 and 95), the party count `0x904AB0` above 3 (slots 75
and 106's loops), the turn-order slot past 11 (slot 97). Reads by an actor,
target, character or ability byte stay unchecked, as in the rest of our
battle code.

## 4. The fuzz (`rest_3c_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=rest_3c` (`BOF3X_R3C_ONLY=<substring>` runs
the clones whose name holds it), `Group::engine`, 6,000 rounds a function,
every clone a `kStep`.

**The listing** beyond the engine set: ours not in it - `Effect_SkillDamage`
(masks `{0, 0xFF, 0xFFFF, 0xFF}`: `caster` unread, the target compared as a
byte, power `and ecx, 0xFFFF`, psi a byte), `Effect_HealAmount` (`{0,
0xFF}`), `Battle_RecalcStats` (`0xFF`: `cmp al, 2`, `and edi, 0xFF`, BE6's
own listing), `BattleForm_ApplyStats`; R3B's four and R3D's eight by address
(`rest_3c_callees.h`; `0x44F1D0`'s target masked to its byte - `cmp bl, 2`,
`and esi, 0xFF` - since slot 76 pushes it in eax over the caller's upper
bytes). **Louder stand-ins:** `Char_RecalcStats` notes and refills the
record's +0x12..+0x17 and +0x20..+0x3F the loop copies after it;
`Formation_ApplyStatMods` notes and refills one member's +0xC0 block the
second loop copies; `Battle_CalcDamage` (the standard masks) answers, two
times in three, 0, 1, or the target's HP - 1 / HP / HP + 1 instead of
garbage (the equality and zero branches of slots 68, 88, 105 and the
clamps). CALC_NOTE

**Regions** beyond the engine frame: `0x904654..0x904664` (slot 62's
`0x904660`) and `CharacterRecords` past the engine region to ten records.

**Seeds** (every round): the actor 0..10; the target 0..10, 0..2 for slot 97
and 3..10 for slots 87 and 95 (section 3); the kind 4 half the time; the
ability 0x9A or below 256; the party count 0..3; each member's character
below 10, HP at 0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, +0x134 bits 0/1 clear half
the time, the counts +0x144 / +0x145 0..3, status bits 3 and 6, the record's
+0x1E 0..6; each enemy's data index below 8, HP at the same ends, maximum
0xFFFF a quarter of the time or about the data word's tenth (0, 1, 9..11,
100, 1000), the data word at 0, 9, 10, 99, 100, 1000, 0xFFFF, +0xAA / +0xAE
0..9 with their markers, +0x94 / +0x96 at 0, 1, 0x7FFF, 0x8000, 0xFFFF, the
counts +0x124 / +0x125 0..3, status bits 3 and 6; the hit count 0..20; the
turn order's cursor 0..12, end 0..11 and slots 0xFF half the time.

**Disturbance** (the group's case, from the hash only): the target (inside
its slot's range), the actor, the party count, the turn order's end and a
slot, the hit count, a member's or an enemy's HP, a status word's bit 3, the
attacker's power word. The engine frame moves the result record's pointer.

RESULT_LINES

## 5. Controls

CONTROLS_TABLE

## 6. Calls across groups and the rebinding

**Out of R3C, raw** (`rest_3c_callees.h`, a recorder each in the fuzz): R3B
`0x44C040` (9 sites: slots 50..52, 55, 56, 58, 60, 83, all tail jumps),
`0x44C120` (slot 59), `0x44C170` (slot 84), `0x44CF60` (slot 61); R3D
`0x44FB30` (15 sites), `0x44F6A0` (6), `0x44FC60` (4), `0x44FCA0` (4),
`0x44FCE0` (3), `0x44FBB0` (2), `0x44F1D0` (1), `0x44F650` (1) - 11 and 36
sites, the cut's edges.

**Into R3C from outside:** `Effect_ApplyResult` through the
`Effect_Handlers` cells (read in place: no rebinding needed), and R3D's
`0x44EA70`, whose tail jump at `0x44EA80` reaches `Effect_DamageAllHp`
`0x44D8B0` (R3D's to rebind after both merge).

**The rebinding.** `tools/band_rows.py --refs` finds two raw references to
the group in `src/game`, both in one comment of `battle_e5.cpp` (line 381:
`BattleForm_ApplyStats`' callers `0x44D450`, `0x44DFD0`, `0x44E720`) - a
comment describing the original's calls by address, left raw on purpose as
earlier groups did. No harness row, `_callees.h` constant or fuzz key names
any of the 61, so nothing was rebound and no raw reference lies in another
group's file.

## 7. Latent defects (Capcom's, described, not fixed)

- **Slot 97 (`Effect_FlushTurnOrder`) writes `ObjTrio +0x134` at the
  target's stride with no enemy branch**: an enemy target (3..10) would OR
  0x10 into `0x8031B0 + 0x14C (t - 3)` - the window records and past them.
  Ours aborts. Its loop runs **through** `0x904AE3` inclusive, where
  `Battle_ReturnQueuedItem` stops below it: with all eleven actors in the
  order it reads the byte after the order (`0x904AD7`), and if that is not
  0xFF hands slot index 11 on and writes 0xFF there. Ours does the same (the
  byte is inside the compared battle bytes). It also hands
  `Battle_ReturnQueuedItem` the **slot index**, not the actor in the slot;
  that callee acts only for 0..2, so it returns the queued item of members
  0..2 by the order's first three positions. Kept as read.
- **Slots 87 and 95 have no member branch**: a member target makes slot 87's
  enemy index 253..255 (a write past the image's end, a fault) and slot
  95's -3..-1 (writes into the battle task slots below the enemies). Ours
  aborts on both.
- **Slot 90 reads `+0x134` at ObjTrio's stride for any target**: for an
  enemy target the miss is decided by bits 0 / 1 of a byte of the window
  records or past them (`0x8031B0 + 0x14C (t - 3)`), not of the enemy. Ours
  reads the same bytes, so it behaves identically; whether play reaches it
  depends on which ability or item uses slot 90 against an enemy, not traced.
- **Slot 106 writes `CharacterRecords[+0x148] +0x1E`** with the character
  byte unchecked, and reads the enemy data record by `+0xF0` unchecked (as
  BE3's `BattleObj_Fall`).
- **Slot 104 reads the power byte by the whole ability word** `0x904B80`
  unchecked, as `Effect_SkillDamage` does.
- **Unchecked writes past the records** (an enemy index past 7, a member past
  2, the party count past 3, the turn order past 11): ours aborts on each;
  the battle's actor and target bytes are 0..10 and its count at most 11
  (`Battle_BuildTurnOrder`), so play reaches none of these aborts.

## 8. The live route

The cut's `reach` column marks all 60 rows `+`: the host
(`Effect_ApplyResult`'s catalogue extent) was entered, an upper bound. The
traces that armed every hidden start show none of the 61:
`analysis/hidden_reached_combat.json` (the combat route) reached only slots
8 (`0x44C140`) and 31 (`0x44C990`) of `Effect_Handlers`;
`hidden_reached_shop.json` and `hidden_reached_worldmap.json` none in
`0x44C000..0x44E500`. The group is fuzz-only; with its lines in
`entries_logic.txt` the coordinator's live check will say if a route enters
one.

## 9. Self-tests and the entry list

SELFTESTS

**`analysis/calltrace/entries_logic.txt`** (main checkout, 2026-10-04): 61
lines appended, the code's extents of section 1 (none was there; no host
line covers them - `Effect_ApplyResult`'s line is `0044B9F0 750`).
