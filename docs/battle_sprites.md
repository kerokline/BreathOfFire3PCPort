# Battle actors, pop-ups, the clut map and the enemies' set-up and names

**Status:** IN PROGRESS (2026-09-24) - 32 functions ours (round seven, group
BG, [`takeover-queue-round7.md`](takeover-queue-round7.md)), fuzzed headless
against byte copies of Capcom's, 70 negative controls all refused; the self-test with `BOF3X_SHADOW='*'`
passes. **Not yet through the live check**: that runs centrally after the
merge (section 6).

Source: [`src/game/battle_sprites.cpp`](../src/game/battle_sprites.cpp),
[`battle_sprites_callees.h`](../src/game/battle_sprites_callees.h),
[`battle_sprites_fuzz.cpp`](../src/game/battle_sprites_fuzz.cpp). Shadow name
`battle_sprites`. Every function is a *faithful* replacement: no
`DIVERGENCE.md` entry is owed.

The group's name in the round doc was a guess from the named members
(`Sprite_SetTint`, `Tint_Release`); most of the group turned out to be the
battle engine's actor bookkeeping. **For the localisation work (section 3):
no function here draws the enemy name.** `Battle_CopyEnemyData` copies it,
and `Battle_OpenEnemyNames` opens the windows that show it.

## 1. The functions

Extents by capstone, 2026-09-24 (a linear range dump and a scratch script
that lists every transfer leaving each extent and fails on any that is not a
direct call or tail jump; inline jump tables skipped by hand). The queue's 27
became 32: five were inside other entries' catalogue sizes, reached only by a
jump or after an inline table (`pe_hidden.py`'s known blind spot):

- `0x453FA0`'s 0x2BD ran on through `Battle_MemberOutAction` `0x454220`
  (after its jump table);
- `0x454DF0`'s 0x3AF through `ClutMap_FindFree` `0x454F30` and
  `ClutMap_FindOwner` `0x455140` (each after the previous one's table);
- `0x494280`'s 0x9A through `Battle_InitBossEncounter` `0x4942A0` and
  `Battle_InitEnemies` `0x4942C0` - `0x494280` itself is two tail jumps.

`0x494F00`'s 0x711 is really 0x13C (it ran on into `0x495040`, not ours).
PSX twins from `analysis/pairs_propagated.json`, read in the sibling's
captures of `BATTLE.EMI` section 15 (`4065db04`, the battle engine, load
`0x80093800`) and `GAME.EMI` (`9d00fd19`) with capstone MIPS. "Combat" is the
all-original trace of `tools/recipes/combat.txt`
(`analysis/calltrace/recipe_combat/bof3x.callcounts.tsv`), total calls.

| PC | bytes | PSX | name | combat | what it does |
|---|--:|---|---|--:|---|
| `0x452BF0` | 0x1D5 | `0x800A5EC0` | `Battle_RollPendingFlag` | 4 | for three kinds of Field_State +0x92, 1-in-12 per target (status class < 6) to set flag 0x100 |
| `0x452F70` | 0x151 | `0x800A6570` | `Battle_SetTargetFlags` | 1 | OR 16 bits into the target(s)' flags, state 0xB, `0x446FB0` |
| `0x4530D0` | 0xB9 | `0x800A6780` | `Battle_SetTargetFlag40` | 8 | flag 0x40 on the target(s) |
| `0x453190` | 0x7B | `0x800A690C` | `Battle_AnyFlagF0` | 817 | any present actor with flag bits 4..7 |
| `0x453210` | 0x87 | `0x800A69E4` | `Battle_ActionSuitsTarget` | 3 | the action against its target being out |
| `0x4532A0` | 0x5B | `0x800A6A9C` | `Battle_ItemSuitsTarget` | 1 | the same for an item |
| `0x4537A0` | 0x16C | - | `Battle_PickFlag8Member` | 3 | pick a party member (flag 8, state 0xA) |
| `0x453A10` | 0x73 | `0x800A76E0` | `Battle_MemberCoinFlip` | 9 | that member's test and a Rand coin |
| `0x453B10` | 0xE1 | `0x800A783C` | `Battle_SettleFlag8` | 8 | flag-8 members hand the action's item to `0x590C90` |
| `0x453C00` | 0x19C | `0x800A79AC` | `Formation_ApplyStatMods` | 1 | the formation's stat changes (PSX name) |
| `0x453DA0` | 0x103 | `0x800A7BC0` | `Battle_SetDamagePopup` | 8 | an amount's pop-up record |
| `0x453FA0` | 0x27C | `0x800A7FF0` | `Battle_MemberAutoTarget` | 10 | a party member's automatic target |
| `0x454220` | 0x3D | - | `Battle_MemberOutAction` (added) | (untraced) | the member's action is one of the four out actions |
| `0x454380` | 0x83 | `0x800A8764` | `Battle_PlayHitSound` | 5 | the hit sound (PSX name) |
| `0x454410` | 0x17E | `0x800A8840` | `Battle_SetHitPopup` | 5 | a hit pop-up at the sprite |
| `0x454CC0` | 0x9C | `0x8019751C` | `Sprite_SetTint` | 20 | a tint record and its clut cells |
| `0x454D60` | 0x56 | `0x80197670` | `Tint_Release` | 26 | release them |
| `0x454DF0` | 0x140 | `0x80197784` | `ClutMap_Mark` | 40 | claim or free a run of clut cells |
| `0x454F30` | 0x210 | `0x801978BC` | `ClutMap_FindFree` (added) | (untraced) | the first free aligned run; the clut slot |
| `0x455140` | 0x5F | `0x80197B20` | `ClutMap_FindOwner` (added) | (untraced) | an owner's first cell |
| `0x4551A0` | 0xA6 | `0x80197B9C` | `Sprite_SetClutStp` | 4 | semi-transparency bit on a sprite's clut |
| `0x494280` | 0x14 | `0x800A8AD4` | `Battle_InitEncounterKind` | 1 | boss or ordinary set-up (PSX name) |
| `0x4942A0` | 0x13 | `0x800A8B10` | `Battle_InitBossEncounter` (added) | 0 | `0x494500`, then the boss handler |
| `0x4942C0` | 0x5A | `0x800A8B60` | `Battle_InitEnemies` (added) | (jmp) | each used encounter record as an enemy |
| `0x494320` | 0x1D9 | - | `Battle_SetupEnemy` | 1 | an enemy's sprite and record |
| `0x4946C0` | 0x21C | `0x800A9148` | `Battle_CopyEnemyData` | 1 | the enemy data - **the name** - into the record |
| `0x494A80` | 0x3EE | `0x800A9894` | `Battle_OpenEnemyNames` | 1 | one name window per living enemy |
| `0x494EA0` | 0x2D | - | `Battle_EnemyKindFlag` | 1 | a bit of the set at 0x904068 |
| `0x494ED0` | 0x2D | - | `Battle_SetEnemyKindFlag` | 1 | set it (from `0x437470`, group BB) |
| `0x494F00` | 0x13C | - | `Battle_SetEnemyOffset` | 1 | enemy +0xF2 / +0xF3 by the layout |
| `0x588F00` | 0x13 | `0x8014D154` | `Sprite_UpdateScreenSlot` | 201 | +0x29 = Draw_OtSlot, then `Sprite_UpdateScreen` |
| `0x587900` | 0xC | - | `Sound_PlayById` | 1 | `Sound_PlayEffect` |

Bytes are each body to its last instruction, inline tables included.
Read side by side with the twin instruction for instruction: `0x452BF0`,
`0x453210`, `0x4532A0`, `0x4551A0`, `0x494A80` (whose PSX loop structure
settled the one doubt, below); the others against the twin's calls,
constants and shape. "No twin" means none in the pairs file.

**Actors** 0..2 are the party records at `ObjTrio` (0x802D40, 0x14C bytes),
3..10 the enemy records at 0x93B960 (0x128 bytes, index actor - 3; the first
0x80 bytes are the enemy's sprite object, `EnemyWorkingRecords` begins at
+0x80). Flags: party +0x130, enemy +0x110 (bit 0x100 the PSX's "pending
effect", ctx +0x124 in `BATTLE_RAM.md`). The target byte 0x904B44 is an actor
or a side: bit 0x80 the party, 0x40 the enemies. `0x4456C0` (group BB)
answers 1 when an actor is absent (+0 bit 0 clear) or dead (+0x91 / +0x93
bit 6).

**`Battle_MemberAutoTarget`** in full: +0x125 goes to 0x904B35. With +0x90
bit 5, +0x134 bits 14 / 16, or +0x134 bit 0: `0x452DD0(member)` 0 settles
at once (+0x125 = 1, 0x904B35 = 1); without +0x134 bit 0 a random target -
Rand & 7 >= 3 and 0x904AB1 not 1: `0x454310(member)`, else
`0x445730((Rand & 7) + 3)` - then the settle. Otherwise +0x134 bit 0 hands
over to `0x454290`, battle flag bit 4 to `0x446B00`, a side in +0x124 is the
target; by +0x125: 0 / 1 take +0x124 unless it is out (then `0x446B00`);
2 / 3 nothing; 4: +0x124 if not out or `Battle_MemberOutAction`, else an
enemy through `0x445730`, a party one through `0x454310` (or the member
itself when 0x904AB1 is 1) unless the action's flag bit 5 (`0x446B00`);
5: +0x124 if not out or `0x454260`, else by `0x591810`'s bit 5 and
0x904AB1 / 0x904AB3 among `0x454310`, `0x435C80`, `0x445730`; above 5
nothing.

**The clut map** is 4 rows x 16 cells at 0x7E06A0, each the owning tint
record's index or 0xFF. Kinds 0..4 take 1, 16 (a row, at cell 0), 2, 4 or 8
cells, aligned; `ClutMap_FindFree` answers the clut slot (0xC0 + row * 16 +
cell, 0x1C + row, 0xE0 + row * 8 + cell / 2, 0x70 + row * 4 + cell / 4,
0x38 + row * 2 + cell / 8), and both searches leave the row and cell in
`DamageScratch` bytes 0 and 1, which `ClutMap_Mark` then writes over.
`Sprite_SetClutStp` turns the slot back into a `Gfx_ClutStrip` row and
column with the tables 0x6528C4 / 0x6528CC / 0x6528D4 indexed by the kind
(+0x28).

## 2. Callees not ours

Called through raw addresses in `battle_sprites_callees.h`, never through a
`symbols.toml` name. What each is, as far as the calls show - said, not bound:

| address | whose | as typed / seen |
|---|---|---|
| `0x4456C0` | BB | `unsigned char (actor)`: 1 when absent or dead (read whole, above) |
| `0x435180` | BB | `unsigned char (0, n)`: a free 0x84-byte record at 0x93A000 (48 of them), 0xFF when none |
| `0x446FB0` | BF | `void (actor)`: sets bit `actor & 31` of the word 0x904B82 |
| `0x445730` | BE | `unsigned char (actor)`: an enemy pick |
| `0x590C90`, `0x591810`, `0x59E2D0` | nobody's | inventory put (a list at 0x904574, 128 slots); a class byte by kind (1..4, else a fifth table) and the id's LOW BYTE in every case (PSX `0x80166918`); claims window record n (0x803160 + n * 0x24, +0 = 1, +1 = kind) |
| `0x453910`, `0x453A90`, `0x453AC0` | nobody's | a second member test (not reached by the combat route); bit of an action id in the set 0x904088; the member's ten bytes +0xFE.. all set |
| `0x452DD0`, `0x454290`, `0x446B00`, `0x454260`, `0x454310`, `0x435C80` | nobody's | the auto-target helpers (above); `0x454260` starts by testing the member's action +0x126 for 0x0E |
| `0x494500` | nobody's | the boss encounter's common set-up |

The 24 boss handlers at 0x656954 read no arguments: each stores three
function pointers at 0x904B64..0x904B6C or returns (checked by reading all 24
heads), so `Battle_InitBossEncounter`'s tail jump becomes a call.

## 3. The enemy name (for the localisation work)

- **Where it comes from:** the enemy data record at
  **`0x8C55C8 + id * 0x8C`**, its first 12 bytes. `Battle_SetupEnemy` is given
  the id by `Battle_InitEnemies` from byte +1 of the encounter record at
  `0x939F00 + r * 12`.
- **Where it goes:** `Battle_CopyEnemyData` copies those 12 bytes to
  **`0x93B9E0 + slot * 0x128`** (`EnemyWorkingRecords` +0 - the 16-byte name
  field the port prepended), with the id's LOW BYTE indexing the data here
  where `Battle_SetupEnemy` uses 16 bits.
- **Who shows it:** `Battle_OpenEnemyNames` opens one kind-3 window per living
  enemy (`0x59E2D0(0xC - n, 3)`), record `0x803160 + (0xC - n) * 0x24`, with
  +0xA = the actor; the window-task handler for kind 3 (group BC's side)
  reads the name from the actor's record. This group draws no text.

## 4. The fuzz

`BOF3X_SHADOW=battle_sprites` (or `*`), at start-up, about 2 seconds:
**93,600 rounds, 3,000 per function (600 for `Sprite_SetClutStp`, whose
state holds the 128 KB clut strip), 141,916 stand-in calls, 0 mismatches.**
Every original byte-copied, every call out (84 sites, the group's calls to
each other included, and the two tail jumps) re-aimed at a numbered
recording stand-in; ours reach the same stand-ins through
`battle_sprites::g`; five inline jump tables relocated into the copies; the
boss table's 24 entries pointed at 24 numbered stand-ins for the duration;
Field_State, 0x904B40, 0x904B50 and 0x939AD8 pointed at buffers of the
fuzz's. Compared: the party records (and over-indexed ones, and the window
records), the globals 0x904000..0x904C00, the pop-up and enemy records, the
clut map and 96 tint records' worth, DamageScratch, Sprite_Current and
Gfx_ClutStripDirty, the enemy data and the encounter records where read, the
action table, the clut strip for `Sprite_SetClutStp`, the fuzz's buffers,
the answer at the width the original defines, and the stand-ins' log (each
argument at the width its callee reads).

Seeded boundaries: targets 0..10 and sides with random low bits; action ids
from the tested set (0x0E, 0x128, 0x4C, 0x4D, 0xB4, 0xB5) and their
neighbours; Field_State +0x92 kinds 0x1B / 0x3F / 0x44 and neighbours;
status classes 0, 5, 6, 7, 0xFF; flag bits 0, 14, 16 of +0x134; modes 0..6;
counts 0..4 / 0..8; boss 0 half the time; amounts 0, 1, -1, -2, 0x7FFF,
0x8000, 0x8001; clut maps with whole free rows, aligned holes, scattered
cells and none free; tint records all taken; banks 0x2CD / 0x2E1 and their
neighbours; eight living enemies of eight kinds (the walk past the list,
below). The stand-ins answer 0, 1 or any byte, `Rand` 0..0x7FFF as the CRT's
does, pop-up slots below 0x30, and disturb what callers read again: the
target byte, the acting actor, the counts, the action id, record bytes (the
members' +0x124 after `0x591810` in particular),
Sprite_Current and its +5 / +0x24 / +0x27 / +0x28, the member stats the
formation re-reads, the clut scratch and cells; the two clut searches set
DamageScratch as the real ones do, and the window claim writes its record.

**Bounded, not the functions:** a target re-read as an index stays an actor
(a side value re-read would index 0x40.. records past `.data`, and fault in
the original too); pop-up slots below 0x30 (0xFF, unchecked by
`Battle_SetDamagePopup`, lands past `.data`); enemy data ids below 0x40; a
clut kind of 0..4 for `Sprite_SetClutStp` (5 and up divide by zero in both).

`Battle_OpenEnemyNames` with eight different kinds walks its list of eight
into the stack above its frame - in the original, its return address, then
the caller's frame - and stops at the first word 0xFFFF. Ours reads (and
decrements) the same words: the detour is a jump, so its entry stack pointer
is the original's, and list entry 8 + k is the dword at entry esp + 4k
(`__builtin_frame_address(0) + 4`). The fuzz calls both from one call site
with a first argument of 0xFFFF, so the walk reads the return address and
stops; a control that stops ours at the list's end instead is refused
(section 5).

One read settled during the fuzz: in the layouts 0 / 1 measuring loop the PC
clears ecx before the first read only - the loop jumps back past the
`xor ecx, ecx` (to `0x494D09`) - so each kind's own count is used, as on the
PSX. A first reading had called it a PC defect; the fuzz refused ours at 505
of 3,000 rounds until it matched.

And one stand-in was too strict: `0x591810`'s second argument was
recorded as 16 bits, and `Battle_MemberAutoTarget` pushes a register whose
upper bytes a callee left (3 rounds of 3,000). Every case of `0x591810`
reads only its low byte (read whole), so the stand-in now records a byte.

## 5. Negative controls

Seventy planted bugs, one at a time, by a scratch driver (`build/bg/controls.py`,
not committed): each a text substitution in `battle_sprites.cpp`, rebuilt, run
headless under `BOF3X_SHADOW=battle_sprites`, the source restored after. **All
70 were refused by a count of mismatching rounds; none by a fault or a hang.**
Counts are rounds of that function (3,000; 600 for `Sprite_SetClutStp`), from
the final pass against the committed fuzz, 2026-09-24. At least one per
function (32 of 32).

| # | function | the planted bug | refused (rounds) |
|--:|---|---|--:|
| 0 | `Battle_RollPendingFlag` | kind 0x40 for 0x3F | 468 of 3000 |
| 1 | `Battle_RollPendingFlag` | party class > 6, not >= 6 | 22 of 3000 |
| 2 | `Battle_RollPendingFlag` | single enemy target not read again | 2 of 3000 |
| 3 | `Battle_RollPendingFlag` | Rand % 11 | 16 of 3000 |
| 4 | `Battle_SetTargetFlags` | bits & 0xFF | 1829 of 3000 |
| 5 | `Battle_SetTargetFlags` | 0xC0 does the party only | 465 of 3000 |
| 6 | `Battle_SetTargetFlags` | party state 0xA | 391 of 3000 |
| 7 | `Battle_SetTargetFlag40` | single party only below 2 | 145 of 3000 |
| 8 | `Battle_AnyFlagF0` | party bits 0x70 | 301 of 3000 |
| 9 | `Battle_ActionSuitsTarget` | flag bit 5 for 4 | 1489 of 3000 |
| 10 | `Battle_ActionSuitsTarget` | 0xB6 for 0xB5 | 150 of 3000 |
| 11 | `Battle_ItemSuitsTarget` | 0x129 for 0x128 | 262 of 3000 |
| 12 | `Battle_ItemSuitsTarget` | item_class of the low byte | 2999 of 3000 |
| 13 | `Battle_PickFlag8Member` | bit 8 picks the test | 1473 of 3000 |
| 14 | `Battle_PickFlag8Member` | flag bit 2 | 1987 of 3000 |
| 15 | `Battle_PickFlag8Member` | always the first that passed | 582 of 3000 |
| 16 | `Battle_MemberCoinFlip` | +0x125 for +0x124 | 884 of 3000 |
| 17 | `Battle_MemberCoinFlip` | one in four | 93 of 3000 |
| 18 | `Battle_SettleFlag8` | record +5 bit 0 | 521 of 3000 |
| 19 | `Battle_SettleFlag8` | member +0x147 | 525 of 3000 |
| 20 | `Battle_SettleFlag8` | action not read again for the bit | 40 of 3000 |
| 21 | `Formation_ApplyStatMods` | +0xC6 down by half | 503 of 3000 |
| 22 | `Formation_ApplyStatMods` | each keeps its own +0xC8 | 198 of 3000 |
| 23 | `Formation_ApplyStatMods` | member 2 up by a quarter | 457 of 3000 |
| 24 | `Battle_SetDamagePopup` | bit 3 for bit 2 | 363 of 3000 |
| 25 | `Battle_SetDamagePopup` | 5 for 3 | 40 of 3000 |
| 26 | `Battle_SetDamagePopup` | +0xB = 1 | 3000 of 3000 |
| 27 | `Battle_MemberAutoTarget` | status 0x10000 only | 73 of 3000 |
| 28 | `Battle_MemberAutoTarget` | roll of 4 and up | 9 of 3000 |
| 29 | `Battle_MemberAutoTarget` | mode 3 as 4 | 981 of 3000 |
| 30 | `Battle_MemberAutoTarget` | 0x445730 for 0x435C80 | 43 of 3000 |
| 31 | `Battle_PlayHitSound` | critical * 6 | 1451 of 3000 |
| 32 | `Battle_SetHitPopup` | enemy offset unsigned | 554 of 3000 |
| 33 | `Battle_SetHitPopup` | party +9 | 1911 of 3000 |
| 34 | `Battle_SetHitPopup` | block bit 2 | 421 of 3000 |
| 35 | `Sprite_SetTint` | head + 0x40 | 2392 of 3000 |
| 36 | `Sprite_SetTint` | +0x24 bit 3 | 69 of 3000 |
| 37 | `Sprite_SetTint` | +5 from +0x27 | 2378 of 3000 |
| 38 | `Tint_Release` | & 0xF7 | 449 of 3000 |
| 39 | `Tint_Release` | kind from +1 | 1144 of 3000 |
| 40 | `ClutMap_Mark` | kind 2 three cells | 339 of 3000 |
| 41 | `ClutMap_Mark` | release writes 0xFE | 1075 of 3000 |
| 42 | `ClutMap_FindFree` | a row of 15 | 115 of 3000 |
| 43 | `ClutMap_FindFree` | kind 3 slot + 0x71 | 397 of 3000 |
| 44 | `ClutMap_FindFree` | kind 4 aligned to 4 | 221 of 3000 |
| 45 | `ClutMap_FindOwner` | answers 1 | 2274 of 3000 |
| 46 | `Sprite_SetClutStp` | +0x24 bit 1 | 132 of 600 |
| 47 | `Sprite_SetClutStp` | the low byte | 600 of 600 |
| 48 | `Battle_InitEncounterKind` | boss above 1 | 56 of 3000 |
| 49 | `Battle_InitBossEncounter` | index read before the call | 29 of 3000 |
| 50 | `Battle_InitEnemies` | 0x904AB3 = n | 2979 of 3000 |
| 51 | `Battle_InitEnemies` | x and z swapped | 2987 of 3000 |
| 52 | `Battle_SetupEnemy` | +5 = slot + 2 | 2821 of 3000 |
| 53 | `Battle_SetupEnemy` | bank 0x2CE for 0x2CD | 484 of 3000 |
| 54 | `Battle_SetupEnemy` | layout ^ 3 | 2992 of 3000 |
| 55 | `Battle_SetupEnemy` | Sprite_Current not read again after AreaMap_Elevation | 156 of 3000 |
| 56 | `Battle_CopyEnemyData` | +0x12 from +0x86 | 2989 of 3000 |
| 57 | `Battle_CopyEnemyData` | 28 bytes of the block | 3000 of 3000 |
| 58 | `Battle_CopyEnemyData` | +0x70 = slot | 2948 of 3000 |
| 59 | `Battle_OpenEnemyNames` | sort ties swap | 1620 of 3000 |
| 60 | `Battle_OpenEnemyNames` | 0xC per kind | 2620 of 3000 |
| 61 | `Battle_OpenEnemyNames` | actor + 4 | 2620 of 3000 |
| 62 | `Battle_OpenEnemyNames` | measured for layout 0 only | 453 of 3000 |
| 63 | `Battle_OpenEnemyNames` | the walk stops at the list end | 204 of 3000 |
| 64 | `Battle_EnemyKindFlag` | bit & 15 | 755 of 3000 |
| 65 | `Battle_SetEnemyKindFlag` | word by k >> 4 | 2027 of 3000 |
| 66 | `Battle_SetEnemyOffset` | side 2 not negated | 497 of 3000 |
| 67 | `Battle_SetEnemyOffset` | side 3 does nothing | 511 of 3000 |
| 68 | `Sprite_UpdateScreenSlot` | +0x28 | 2997 of 3000 |
| 69 | `Sound_PlayById` | id + 1 | 3000 of 3000 |

**Read with the table:**

- Control 30 (`0x445730` for `0x435C80` when the member's re-read target is an
  enemy) was **not refused at first** (0 of 3,000): the path needs the
  member's +0x124 to change during the call to `0x591810`, and no stand-in did
  that. `0x591810`'s stand-in now rewrites the three members' +0x124 half the
  time (the real one does not, but the caller reads it again, and that read is
  the behaviour under test), and `Battle_MemberAutoTarget`'s rounds seed the
  plain path to modes 4 and 5; refused since.
- The thinnest refusals (#2 2, #3 16, #28 9 rounds) are re-reads and
  remainder tests on narrow paths; each is still a count, not zero. Rand's
  stand-in answers multiples of 12 and of 11 half the time for the `% 12`
  tests (control 3 went from 2 to its count here after that change).
- Controls 60 and 61 share a count: both change every window of every round
  that opens one.

## 6. For the batch check

Entries for `analysis/calltrace/entries_logic.txt` (owned ranges, sizes as
measured here; five replace wrong sizes - 0x453C00, 0x453FA0,
0x454DF0, 0x494280, 0x494F00 - and five are new: 0x454220, 0x454F30,
0x455140, 0x4942A0, 0x4942C0):

    00452BF0 1D5
    00452F70 151
    004530D0 B9
    00453190 7B
    00453210 87
    004532A0 5B
    004537A0 16C
    00453A10 73
    00453B10 E1
    00453C00 19C
    00453DA0 103
    00453FA0 27C
    00454220 3D
    00454380 83
    00454410 17E
    00454CC0 9C
    00454D60 56
    00454DF0 140
    00454F30 210
    00455140 5F
    004551A0 A6
    00494280 14
    004942A0 13
    004942C0 5A
    00494320 1D9
    004946C0 21C
    00494A80 3EE
    00494EA0 2D
    00494ED0 2D
    00494F00 13C
    00587900 C
    00588F00 13

The combat A/B (`analysis/validate_combat.sh`) reaches every function but
`Battle_InitBossEncounter` (an ordinary encounter) and, by the trace,
`0x453910` from `Battle_PickFlag8Member` (not ours). Untraced, so unknown:
`Battle_MemberOutAction` (only mode 4 of `Battle_MemberAutoTarget` calls it).
Fuzz only: the boss set-up, the eight-kinds walk, a full tint table or clut
map, formations other than the route's, most of `Battle_MemberAutoTarget`'s
modes. `Sprite_UpdateScreenSlot` (201 calls) and the tints run outside
battle too (the field's party screens and fades).

## 7. Latent defects kept (for docs/known-defects.md, the coordinator's)

No Capcom defect reached in play was found. Latent, kept as they are:

- `Battle_SetDamagePopup` and `Battle_SetHitPopup` do not check
  `0x435180`'s 0xFF (no free record): record 0xFF is 0x93A000 + 0x837C, past
  the end of `.data` - a write fault, if 48 pop-ups were ever live.
- `Battle_OpenEnemyNames` walks past its eight-entry list with eight
  different kinds alive (the PSX alike); it stops at the first 0xFFFF word on
  the caller's stack, and could decrement a caller word that happened to
  equal a kind.
- `Battle_SetupEnemy` indexes the enemy data by 16 bits of the id,
  `Battle_CopyEnemyData` by 8: ids above 0xFF would split between records.
- `ClutMap_Mark`'s release, when `ClutMap_FindOwner` finds nothing, still
  writes 0xFF over the cells DamageScratch last named.
- `Sprite_SetClutStp` divides by zero for a clut kind of 5 or more.
- Every actor index is unchecked (targets 11..0x3F index enemy records past
  `.data`).
