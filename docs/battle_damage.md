# Damage, effects and affinities: the damage formula, the effect result, the turn order

**Status:** IN PROGRESS (2026-09-24) - eighteen functions ours
(`src/game/battle_damage.cpp`, shadow name `battle_damage`), each fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder;
99 negative controls, all 99 refused by a count. **Through the wave-2 batch** (2026-09-24, 1,020 ours - [`takeover-queue-round7.md`](takeover-queue-round7.md) "Result": the combat A/B 5 of 43 at 4..15 px, the tile-edge class); section 7 had it owed.

Group BE of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)). The battle engine
is the PSX's `BATTLE.EMI` overlays compiled into the exe; every function here
has a PSX twin, read side by side in the sibling's Ghidra export
(`../BreathOfFire3Recomp/analysis/ghidra/BATTLE_EMI3_801D0C00_decomp`,
`BATTLE_EMI15_80093800_decomp`) and in capstone MIPS from the overlay images
next to it. **No divergence**: every function is a faithful replacement and no
`DIVERGENCE.md` entry is owed. Four defects of Capcom's, all in the PSX too,
are in section 6 for the coordinator to number.

## 1. What is ours, and the extents

Bytes are each body to its last instruction and its jump table (capstone
recursive descent, 2026-09-23) - the figure for
`analysis/calltrace/entries_logic.txt`. Route calls are the combat route's
(`analysis/combat_catalog.md`).

| Function | Entry | Bytes | PSX twin | Route calls | Section |
|---|---|--:|---|--:|---|
| `Battle_BuildEntryOrder` | `0x444F40` | 0x197 | `0x801DA7A4` (EMI#3) | 4 | 4 |
| `Battle_BuildTurnOrder` | `0x4450E0` | 0x2D7 | `Battle_BuildTurnOrder` `0x801DAAB4` | 4 | 4 |
| `Battle_LevelClass` | `0x445640` | 0x3D | `Battle_LevelClass` `0x801DB3FC` | 12 | 4 |
| `Battle_ClearCommands` | `0x445680` | 0x31 | `Battle_ClearCommands` `0x801DB45C` | 7 | 4 |
| `Battle_DefaultTarget` | `0x445730` | 0xB3 | `0x801DB594` | 11 | 4 |
| `Battle_ActorCanCommand` | `0x4458B0` | 0xCE | `0x801DB80C` | 24 | 4 |
| `Battle_ActorCanAct` | `0x445980` | 0xB0 | `Battle_ActorCanAct` `0x801DB9AC` | 78 | 4 |
| `Battle_ApplyDamage` | `0x445A30` | 0x2B1 | `Battle_ApplyDamage` `0x801DBB40` | 4 | 2 |
| `Battle_CalcDamage` | `0x445CF0` | 0x41B | `Battle_CalcDamage` `0x801DC00C` | 5 | 2 |
| `Battle_BaseDamage` | `0x4462B0` | 0x12A | `Battle_BaseDamage` `0x801DCAA0` | 5 | 2 |
| `Battle_ScaleDamage` | `0x446430` | 0x101 | `Battle_ScaleDamage` `0x801DCD18` | 5 | 2 |
| `Battle_RemoveFromTurnOrder` | `0x446650` | 0xA9 | `Battle_RemoveFromTurnOrder` `0x801DD114` | 1 | 4 |
| `EnemyAI_ChooseActions` | `0x44AE90` | 0x3A4 | `EnemyAI_ChooseActions` `0x80098F8C` (EMI#15) | 4 | 5 |
| `EnemyAI_RowDone` | `0x44B2C0` | 0x13 | `EnemyAI_RowDone` `0x80099830` | 4 | 5 |
| `Effect_ApplyResult` | `0x44B9F0` | 0x4D6 | `Effect_ApplyResult` `0x8009A160` | 4 | 3 |
| `Effect_SkillDamage` | `0x44ED10` | 0x166 | `0x8009F820` | 1 | 3 |
| `Battle_ElementAffinity` | `0x44EE80` | 0x1AF | `Battle_ElementAffinity` `0x8009FA78` | 1 | 2 |
| `Effect_HealAmount` | `0x44F130` | 0x9A | `0x800A0080` | 1 | 3 |

**For `entries_logic.txt`** two sizes there are wrong: `0044AE90 3A9` is
`3A4` (the table ends at `0x44B233`, padding follows), and **`0044B9F0 3317`
is `4D6`** - the old extent swallows the 130 pointer-reached effect handlers
after it (`0x44BF70` on), which would count them as owned. The other sixteen
are listed with the right sizes.

The four already named (`Battle_ApplyDamage`, `Battle_ScaleDamage`,
`Effect_ApplyResult`, `Battle_ElementAffinity`) keep their names; their
entries moved to this group's block at the end of `symbols.toml`.

Several originals use their own argument slots as scratch (`0x4458B0`,
`0x445980`, `0x445730`, `0x446650`, `0x445CF0`, `0x44F130`); every caller
pops its arguments or pushes fresh ones (E8 scan), so ours do not reproduce
that. Where an original pushes a dword whose upper bytes are stale stack (the
actor loops, the AI's row counter), the callee reads only the low byte - read
for `0x4456C0`, `0x44B2E0`, `0x44B240`, `0x44F4B0`, `EnemyAI_RowDone`,
`Battle_LevelClass` - and the recorders compare that byte.

## 2. The damage formula

An attack is `Battle_ApplyDamage(attacker, target)`. Actors are bytes: 0..2
the party (working record `0x802D50 + 0x14C` x i), 3..10 the enemies
(`0x93B9E0 + 0x128` x (i - 3)). Before the chain runs, the caller has copied
the attacker's ATK to `0x939FE4` and the target's DEF to `0x939F86` (the PSX
`0x801EC27C` / `0x801EC25E`).

**Base** (`Battle_BaseDamage`, mode 0):

- a party attacker, or an enemy hitting an enemy: `ATK - DEF`, not below 0;
- an enemy hitting the party: `ATK - D`, not below 0, where `D` is
  `0x4463E0`'s defence (the party's mean DEF with the target's, halved - read,
  not ours), then plus the enemy's word `+0x18` x 0.2 or 0.3 (`(2 + Rand % 2)
  / 10`, in 8.8 fixed point);
- then `+ (Rand & 1)`.

**Scale** (`Battle_ScaleDamage`), in 8.8 fixed point with `s = base << 8`:

1. x `205/256` (the multiplier `0x100 - (s / 256000) << 17 >> 8` floored at
   `0xCD`; it is `0xCD` for any base below 256,000);
2. x the variance `{218, 230, 243, 256, 269, 282, 294, 307}[Rand & 7] / 256`
   - 0.85 to 1.20;
3. if the damage scratch (`DamageScratch` `0x903850`, the weapon's element
   byte) has any of bits 0..4: x `Battle_ElementAffinity(target, scratch) /
   100` - the sum over those bits (fire, ice, lightning, earth, wind) of the
   s16 `0x64E95C[class]`, the target's five class bytes (party `+0x9F`, enemy
   `+0x3F`), `{300, 200, 100, 75, 50, 25, 0, -100, -1, 150, ...}` percent;
4. if it has bit 5 (holy): x `{300, 200, 200, 150, 125, 100, 50, 0}[holy
   class] / 100` (party `+0xA4`, enemy `+0x44`);
5. rounded half up out of 8.8.

So an ordinary hit is `(ATK - DEF + 0 or 1) x 0.80 x 0.85..1.20`, then the
element and holy percentages.

**Adjust** (`Battle_CalcDamage(attacker, target, 0xFFFF)`), around the base
and scale:

1. The scratch gets the party attacker's weapon element (`NameTable_Weapons`
   `+0x13`), an enemy's 0.
2. A charged attacker (second flags bit 6): the party's ATK becomes ATK +
   charge x ATK / 2; **an enemy's becomes charge x ATK / 2** (section 6); the
   charge is spent.
3. Base and scale as above.
4. A party member hitting an enemy: x2 for weapons `0x13`, `0x17`, `0x33`
   against enemy family 4 (byte `+0x0D`), and x2 for `0x16`, `0x35`, `0x40`
   against family 1.
5. If the target's status byte has none of `0x64`, the to-hit roll decides
   (`0x446110` on the party, `0x4461B0` on an enemy; the sibling's
   `Battle_HitCheck_*`: 0 on a miss). With `0x40` or `0x20` there is no roll:
   that status is cured through `0x44F4B0` and the hit does at least 1; with
   `4`, at least 1.
6. Target flags bit 1: x `{50, 50, 50, 50, 60, 60, 60, 70}[Rand % 8] / 100`.
7. `0x904AA8` bit 7: x2, plus a quarter of the base without the defence
   (`Battle_BaseDamage` mode 1).
8. A quarter off on a party target when `0x904060` is 2, and a quarter off
   for status `0x80`.
9. At least 1 with status `0x60` or `0x904AA8` bit 7; 0 against flag
   `0x10000`; clamped to +-9999.

**Apply** (`Battle_ApplyDamage`): the target's shown byte = `0x11`; with the
target's flag bit 8 (an enemy's only while its HP is at most `0x7FFF`) the
amount is the whole HP, a message is shown (`0x44A650(1, 0, 0, 0x1E, ...)`)
and `0x8031F3` set - an instant kill. Damage below HP is taken off; otherwise
HP is 0, and a party member whose overkill is below its byte `+0x8C` (with
second flags bits 0..1 clear) gets flag bit 2. A heal (0 or less) is capped
at max HP. An enemy with HP `0xFFFF` takes nothing. Last, a party attacker
whose weapon kind `+0x138` is 5 inflicts `0x40` / `8` / `0x20` for weapons
`'J'` / `'L'` / `'O'` through `0x44F1D0`, unless `0x44FA70` says the target
resisted. Which weapons and statuses those are is not read here (game facts
are the owner's).

## 3. The effects

`Effect_ApplyResult` (no argument on the PC; the target is the byte
`0x904B54`) is the ability and item path:

1. the target's shown byte `|= 0x31`;
2. unless ability `0xA3` (kind 4), a charged actor (`0x904B34`, second flags
   bit 7) multiplies the power word `0x939FEA` by charge + 1;
3. one of 130 handlers `0x64E73C[i]`: `i = 0x64E540[ability]` for kind 4, else
   `0x64E72C[category][item]` from the item command at `*0x904B40 + 2`;
4. the handler leaves an HP delta at `+4` and an AP delta at `+6` of the
   result record `*0x904B60` (positive is damage); flag `0x10000` zeroes both;
   each is clamped to +-9999 and applied like `Battle_ApplyDamage`'s (the kill
   flag and overkill test for the party, heals capped with shown bits `|5` /
   `|0xA`, `0xFFFF` immune on enemies).

Two amounts the handlers call:

- `Effect_SkillDamage(caster, target, power, psi)`: `(INT + 100) x power`
  (INT the caster's `0x939FEA`), x `(100 - the target's INT / 5, at least 50)
  / 100`, x the element affinity / 100 when the ability has an element mask
  (`NameTable_Abilities` `+4`, nine bits) - `Battle_ElementAffinity`, or
  `0x44F030` when `psi` is set - x `{85..120}[Rand & 7] / 10000`; a quarter
  off against the party when `0x904060` is 2; halved by the target's second
  flags bit 9; 0 against flag `0x10000`.
- `Effect_HealAmount(caster, target)`: `power byte (+3) x (INT + 100)` x
  `{-300, -200, -100, 0, 50, 100, 200, 400}[holy class] / -10000`: a heal
  (negative) for holy classes 4..7, damage for 0..2. The PSX divides by 10000
  and negates; the numbers are the same.

## 4. The turn order

- `Battle_ActorCanAct(actor)`: present (bit 0 of the object's first byte),
  no status bit of `0x4944` (party) / `0x4144` (enemy), with `0x904B8E` set
  second flags bit 4, and `0x904AE4` not naming its side (2 party, 1 enemies)
  nor 3. `Battle_ActorCanCommand` adds status `0x20` and second flags
  `0x14001` (party) / `0x4000` (enemy).
- `Battle_BuildEntryOrder`: the party members who can command, by agility
  (`+0x98`), highest first, into `0x904AB6` (count `0x904AC3`) - the order
  commands are entered in.
- `Battle_BuildTurnOrder`: party value = agility + bonus x percent / 100,
  the bonus 4 / 2 / 1 for an ability of order kind 0 / 1 or 3 / an item, the
  percent `{100, 125, 150, 200, 250}` by level class; enemy value = agility
  `+0x38` + a signed jitter from sixteen per level class by `Rand & 15`.
  Sorted highest first into `0x904ACC` (count `0x904AE3`, cursor
  `0x904AE2`). `Battle_LevelClass(level, row)`: the first threshold above the
  level, row 0 (enemies) 16 36 64 99, row 1 (party) 8 16 32 48 99, else 6.
- `Battle_ClearCommands`: every command byte and flags dword zeroed.
  `Battle_RemoveFromTurnOrder(actor)`: its command cleared and each of its
  places in the order set to `0xFF`. `Battle_DefaultTarget(actor)`: the first
  actor of the same side, from it upward then from the side's first, that
  `0x4456C0` does not rule out; `0xFF` if none.

Both order builders walk a side first for a sum and a highest agility that
nothing reads - the PSX computes and drops the same; the calls are kept.

## 5. The enemies' AI conditions

`EnemyAI_ChooseActions`: for each enemy not ruled out, rows 0..3 of its
script (`0x8C5600 + 0x8C` x byte `+0xF0`; the PSX's scripts are `0x88`
apart), byte 0 a condition. A holding condition fires a row not yet done
(`0x44B3A0`, then marked done through `0x44B2E0`); a failing one unmarks it.
The conditions are listed in `battle_damage.cpp` and the symbols entry: HP /
AP against a quarter, half or fifth of their maxima, another enemy with
status `0x2000`, counts of enemies (`0x904AB1..0x904AB3`), the turn counter
(`0x904B90` == 2 / 10, even / odd, multiple of 3), party member 0's level
against the enemy's by more than 5, `0x44B240`, `0x904B97`. Then
`0x44B920()`. `EnemyAI_RowDone(enemy, row)` is bit `row` of byte `+0xF1`.

## 6. Defects of Capcom's (for `known-defects.md`; all in the PSX too)

1. **An enemy's AP heal is compared with its max HP** and only then clamped
   to max AP (`Effect_ApplyResult`, `0x44BE91`; the PSX `0x801EB640` against
   `0x801EB642`). An enemy whose max HP exceeds its max AP can be healed to
   AP above its maximum, up to max HP - 1. Latent: needs an AP heal on an
   enemy. Control E73 (the "fix") is refused, so ours keeps it.
2. **Level class 6 over-reads both turn-order tables.** A level of 99 is
   class 6 (both rows end in 99, 99), past the five percentages and the four
   jitter rows. A level-99 party member's bonus is x516 % (the jitter
   table's first two bytes as a word) instead of a real entry; a level-99
   enemy's jitter comes from the damage variance table's bytes. Latent until
   level 99.
3. **An enemy's charge replaces its attack** where the party's adds to it
   (`Battle_CalcDamage`, `add` against `mov` at `0x445DB3` / `0x445DFD`; the
   PSX the same). A charge of 2 leaves an enemy's attack unchanged. Whether
   intended is not known; a candidate.
4. **Dead arithmetic in `Battle_BaseDamage`**: the divisor index `(x / -5)`
   is computed after `x` is floored at 0, so it is always 0 and the divisor
   always 10; eight of the table's nine entries (`0x64E380`) are unreachable.
   No player-visible effect.

Also latent, not defects of behaviour: `Effect_ApplyResult` does not bound
the item category (a category above 3 reads the handler table as byte
lists); `Battle_CalcDamage` indexes its stack by `Rand % 8` signed, which
the CRT's `rand` never makes negative (ours aborts loudly if it were).

## 7. The fuzz, and the controls

`BOF3X_SHADOW=battle_damage`, at start-up: eighteen byte-copies
(`battle_damage_fuzz.cpp`, `kClones`), every call out re-aimed at a recorder
(`bof3::CloneCall` with the callee each site was read to call), the AI's
jump table (29 entries) relocated in its copy, the effect handler table
`0x64E73C` swapped for 130 recording stand-ins and the category table
`0x64E72C` for byte lists of our own, both put back after. 2,000 rounds per
function: random bytes over the party objects (`0x802D40`, `0x4C0`),
`DamageScratch`, the battle state (`0x904000`, `0xC00`), the turn work and
damage stats (`0x939A80`, `0x580`), the enemies (`0x93B960`, `0xBA0`, room
for actor 11), AI scripts 0..15 and the result records; then the function's
boundaries seeded (levels at every threshold; HP / AP / maxima equal and
either side; single status and flag bits of each mask; weapons and families
of the doubling lists; overkill against the survive byte; ATK against DEF
equal and either side; bases around 1,000 and 256,000; the charge bits; AI
HP at a quarter / half and AP at a fifth / half, the counter at 2 / 10 /
multiples of 3, the level difference at 5 / 6; handler deltas at 0, +-1,
+-9999, +-10000, the target's HP and AP +-1). Theirs, then from the same
state ours; the regions, the answer at the original's width and the
recorders' log compared. The stand-ins scribble on the cells their callers
read again (the actors' HP, maxima, flags, status, weapon and family, the
scratch - toggled by the affinity stand-in, which `Battle_ScaleDamage`
re-reads - the battle flags, the turn work's value words, the script byte
and counters); the handlers may move the result pointer and change the
target. `Rand` answers only 0..0x7FFF, as the CRT's does.

Result (2026-09-24):

    shadow      battle_damage self-test: 36000 rounds over 18 functions (2000 each), 100129 calls to the stand-ins, 0 MISMATCHES
    shadow      battle_damage coverage: calls can-act 27296, can-command 5710, level class 6731, calc 2000, base 2785, scale 2000, element 1780, row done 5531, is-out 20988, message 570, resisted 625, inflict 315, cure 1323, hit 202 / 525, party DEF 453, psi 341, rand 12824, AI apply 1074, set done 2795, rows left 261, finish 2000, handlers 2000; CalcDamage clamped 434; ...

`BOF3X_SHADOW='*'`: exit 0, no `Fatal`, every self-test 0 MISMATCHES, 818
ours.

**Ninety-nine negative controls**, planted one at a time by a script (not
committed: apply, build, run `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=battle_damage`, restore), each build's output read. All 99
refused by a count (exit 3), each only in the function it touches (E12 in
both sorters, which share the sort). Rounds refused out of 2,000:

| | Planted | Rounds |
|---|---|--:|
| E1-E3 | LevelClass: `<=`; none found 5; row stride 5 | 324, 396, 823 |
| E4-E8 | CanAct: status 0x4940; enemy 0x4104; side 3 ignored; enemy side 2; gate bit 0x20 | 35, 26, 50, 68, 277 |
| E9-E11 | CanCommand: flags 0x14000; enemy 0x400; status 0x4944 | 30, 33, 30 |
| E12 | sorts swap equal values | 38 + 26 |
| E13-E16 | EntryOrder: enemies not walked; no second ask; list end kept; unsigned sort | 1994, 1323, 1971, 235 |
| E17-E25 | TurnOrder: kind 2; item bonus 2; party row 0; product not 16-bit; Rand & 7; jitter unsigned; cursor kept; slot word read early; list 10 bytes | 37, 46, 1265, 182, 1015, 572, 1991, 186, 1989 |
| E26 | ClearCommands: seven enemies | 2000 |
| E27-E28 | DefaultTarget: 10 not scanned; party wraps to itself | 120, 11 |
| E29-E30, E99 | Remove: flags2 bit 3; enemy bits 0..1; first place only | 389, 765, 1539 |
| E31-E37 | Scale: floor 0xCC; Rand & 3; scratch not re-read; round above 0x80; unsigned /100; 256256; holy byte - 1 | 1087, 740, 417, 6, 485, 212, 551 |
| E38-E43 | Base: DEF in mode 1; factor 3; no floor; Rand & 3; party DEF whole; enemy-on-enemy as party | 545, 941, 206, 1045, 176, 553 |
| E44-E58 | Calc: enemy element 1; charge set; family 3; weapon 0x41; mask 0x60; roll 75; mode-1 quarter floored; cut at 1; no 0x80 cut; immunity 0x20000; clamp 9998; min 1 with 0x40 only; or 0x60; cure byte + 1; 16-bit amount to the roll | 508, 411, 123, 39, 263, 60, 79, 89, 229, 172, 210, 12, 162, 477, 105 |
| E59-E66 | Apply: shown 0x10; overkill `<=`; flags2 bit 0 only; enemy bound 0x8000; heal `<=`; L inflicts 0x10; 0x8031F3 not set; flags read before the message | 1014, 29, 27, 15, 13, 108, 221, 4 |
| E67-E68 | Affinity: enemy block + 1; four elements | 914, 819 |
| E69-E77 | Result: 0xA4; no + 1; immunity keeps AP; heal keeps bit 4; **the AP defect fixed**; AP clamp -9998; target not re-read; category from the low byte; party HP `>=` | 135, 86, 511, 220, 90, 248, 207, 1023, 14 |
| E78-E83 | Skill: floor 49; psi / element swapped; mask 8 bits; cut by the argument; halving bit 8; signed first division | 752, 654, 125, 162, 207, 103 |
| E84-E85 | Heal: / 10000; party holy byte - 1 | 1310, 685 |
| E86-E98 | AI: 0xB `<`; 0xD / 4; 0xE no out test; 0x15 at 5; 0x1B % 4; 0x26 args swapped; 0x27 non-zero; three rows; no unmark; 0x16 as 0xB; counter byte for 0x13; script byte read once; RowDone & 7 | 29, 23, 216, 43, 105, 190, 99, 960, 1425, 201, 13, 12, 368 |

The thinnest: E66 (4 - a stand-in must change the flags dword during the
message call), E34 (6 - a result with low byte exactly `0x80`), E28 (11),
E55 (12, 0 before the flag roll was seeded to take a 1 back to 0), E97 (12),
E96 (13), E63 (13), E77 (14). In the first pass E20 was not refused and E24
refused once, until the level-class stand-in was made to rewrite the turn
work's value words it is followed by; E33 was 4 until the affinity stand-in
toggled the scratch's holy bit.

## 8. What the route reaches

The combat route enters all eighteen (the counts in section 1): a round's
entry and turn order, the enemies' AI, attacks (`Battle_ApplyDamage` 4) and
abilities (`Effect_ApplyResult` 4, one `Effect_SkillDamage`, one
`Effect_HealAmount`, one `Battle_ElementAffinity`). Which branches ran is not
measured - the charge, the instant kill, the weapon element rolls, the
family doublings, the AP paths, most AI conditions and the level-99 class
are fuzz only as far as the route says. For the batch: all eighteen on the
`--original` list, and on the trace list with the sizes in section 1.

## 9. Found on the way

- **`Battle_PsiStatusDeathAffinity` `0x44F030` is the twin of `0x8009FE88`,
  not `0x8009FD08`.** `Effect_SkillDamage`'s PSX twin `0x8009F820` calls
  `0x8009FE88` where the PC calls `0x44F030`, and `0x44F030` indexes
  `0x64E97C` - `0x30` past the variance table `0x64E94C`, as `0x8009FE88`'s
  `0x800B189C` is past `0x800B186C`. The near-duplicate `0x44F770` indexes
  `0x64E96C` (PSX `0x800B188C`, `0x8009FD08`'s) and is called by `0x44FA70`,
  the PC `0x800A0D78`, which calls `0x8009FD08`. Not renamed (not ours).
- `analysis/pairs_propagated.json` crosses two pairs: `0x445A30` with
  `0x8009A158` and `0x44B9F0` with `0x801DBB40`; the bodies say `0x801DBB40`
  and `0x8009A160`.
- The PSX damage variance table is **u32**, not u16 as the
  `Battle_DamageVarianceTable` note says: the overlay bytes at `0x801EAF50`
  read `DA 00 00 00 E6 00 00 00`, and `FUN_801dcd18` reads it as `int`.
- Other groups' callees, typed from their call sites and first
  instructions: `0x4456C0(actor)` (group BB) answers al 1 when the actor is
  absent or has status bit `0x40` of the high byte - "out"; `0x44A650` (BF)
  takes five arguments; `0x44F4B0(id byte, status)` (BA) reads the low byte.
  Unnamed and unowned: `0x446110` / `0x4461B0` (the to-hit rolls, PSX
  `0x801DC704` / `0x801DC85C`), `0x4463E0` (the party's mean DEF, PSX
  `0x801DCC78`), `0x44FA70` / `0x44F1D0` (the weapon element roll and
  inflict, PSX `0x800A0D78` / `0x800A0170`), `0x44B240`, `0x44B2E0`
  (`EnemyAI_SetRowDone`, PSX `0x80099844`), `0x44B3A0`
  (`EnemyAI_ApplyAction`, PSX `0x80099954`), `0x44B920`.
