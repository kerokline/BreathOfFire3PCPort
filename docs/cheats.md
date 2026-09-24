# Cheats — EXP and zenny multipliers, and a steal that always lands

**Status:** BUILT + CONFIRMED IN GAME (2026-09-24, the recorded combat route,
watched by the owner).

The archival sibling ships three cheats as mods: EXP and zenny multiplier
sliders (its `docs/EXP_BOOST.md`) and a "steal always succeeds" patch (its
`docs/STEAL.md`). This is the same three as launcher settings here - a
"Cheats..." button on the settings dialog, beside "Play", with two sliders
and one switch. Ledger entries DIV-0045 and DIV-0046.

## 1. Where each one lives

| Setting | `bof3x.ini` | Environment | Default | Done by |
|---|---|---|---|---|
| EXP won, times | `cheat.exp=` 0..50 | `BOF3X_EXP=n` when not 1 | 1 | our `Battle_EnemyDefeated` `0x437470`, [`battle_flow.md`](battle_flow.md) |
| Zenny won, times | `cheat.zenny=` 0..50 | `BOF3X_ZENNY=n` when not 1 | 1 | the same |
| Steal always succeeds | `cheat.steal=` 0 / 1 | `BOF3X_STEAL=1` when on | off | two `PatchBytes` of one immediate each, `src/game/cheats.cpp` |

Unset, nothing here changes a byte or a number: every harness and oracle
run leaves the variables unset, and the `battle_flow` fuzz compares ours
against the original with the multipliers at 1. A value outside its range
is a `Fatal`, as a wrong `BOF3X_FILTER` is.

## 2. The multipliers

The sibling scales the fallen enemy's record before the original adds it to
the battle totals, because on the PlayStation the addition is overlay code
it cannot edit. Here the addition is ours: `Battle_EnemyDefeated` adds the
enemy's u16 EXP (`+0x96`) and zenny (`+0x94`) to the totals `0x904AEC` /
`0x904AF0` ([`battle_flow.md`](battle_flow.md) §2), so it multiplies the
yield on the way in and leaves the record alone. The results screen, the
per-member split and any level-ups read the totals, and follow; the
sibling's proof of the same source (its 84 EXP → 840, counter ticks the
same) is the reason to expect that, not a measurement made here.

0 grants nothing - the sibling verified a no-level-up run under it in play
(2026-09-13); not tried here yet. 50 is the DLL's ceiling, the sibling's
too.

## 3. The steal roll on the PC

The sibling found the roll inside each steal ability's own BMAGIC overlay
(Pilfer `MAGIC065.EMI`, Steal `MAGIC216.EMI`), the same routine twice. The
port compiled those overlays into the exe, so the routine is there twice
too. Found 2026-09-24 by searching `BOF3.exe` for the rate table
`[0, 1, 3, 6, 12, 16, 32, 32]` (two hits, `0x65AC20` and `0x65C204`) and the
code that indexes each (an immediate scan of `.text`; `pe_xref.py` misses
an indexed operand):

| Ability | State step | Rate table | `Rand` | `and eax, 0xFF` | Compare |
|---|---|---|---|---|---|
| Pilfer | `0x4B54F0` | `0x65AC20` | `0x4B5688` | `0x4B5690` | `cmp eax, edi` |
| Steal | `0x4F5140` | `0x65C204` | `0x4F51E6` | `0x4F51EE` | `cmp eax, esi` |

Read off the disassembly (`tools/pe_disasm.py`), the same terms in the same
order as the PSX's `0x801EEE74..0x801EF000` in the sibling's `STEAL.md`:
the attacker's agility from the party record (`0x802DE8` table, actor byte
`0x904B34`) less the enemy's (`0x93BA18`, target byte `0x904B44` - 3, 0x118
a record), nine compares to a tier 12 .. 4 (≥ 49, 29, 19, 9, −10, −20,
−30, −50), the enemy's steal level `+0x1A` (`0x93BA0A`) into the table,
`Rand` (`0x5B93D2`, the CRT's), the low byte against level × tier; on a
pass the enemy's drop-1 item `+0x18` (`0x93BA08`) into the inventory
(`0x590BB0`), message 0x38, item and level cleared - and 0x39 or 0x3A
(inventory full, nothing to steal) otherwise.

The cheat is the sibling's patch: the mask's immediate `0xFF` becomes `0`
at both sites, so the random byte is 0 and `0 < level × tier` passes
whenever the enemy's chance was above zero. `PatchBytes` checks the
immediate with the compare after it (`FF 00 00 00 3B C7` / `.. 3B C6`)
before writing. What does not change: an enemy at level 0 stays
unstealable, an enemy with nothing still says so, and the routine clears
the item on success, so a second attempt finds nothing.

Why a byte patch and not a takeover: the roll sits inside two state steps
of ~0x270 bytes each that are not yet ours, and the divergence is one
operand - the same case as DIV-0012's filter immediates.
`BOF3X_ORIGINAL=Cheat_StealAlways` leaves both sites alone.

## 4. Verification

- 2026-09-24, headless (`BOF3X_SELFTEST_ONLY=1`) with `BOF3X_STEAL=1
  BOF3X_EXP=10 BOF3X_ZENNY=3`: the log has `DIV-0045 EXP x10, zenny x3`,
  `patch ON Cheat_StealAlways 6 bytes at 0x004B5691` and `... 0x004F51EF`,
  `DIV-0046 steal always succeeds`; every inject and start-up self-test as
  before. `BOF3X_EXP=99` ends the process with `FATAL: BOF3X_EXP must be
  0..50`.
- The same day, the recorded combat route (`tools/recipes/combat_ab.txt`,
  [`input-script.md`](input-script.md)) under `BOF3X_STEAL=1 BOF3X_EXP=10
  BOF3X_ZENNY=3` against a clean run at the same window size
  (`analysis/shots/combat_cheats` / `combat_clean2`; the older `combat_a`
  is 640 x 480 and the window now opens at the owner's saved size): 43
  captures, 6 differ. Capture 960: "You couldn't steal anything!" clean,
  "You grabbed Marbles!" with the cheat; 1740..1860: the item list with
  Marbles as a sixth entry, 6/128 against 5/128; 2520..2580: the results'
  message a phase apart (the EXP counter ticks longer for a larger total).
  The owner watched the run: the Marbles steal, and the kill's 3 EXP shown
  as 30. Owed: the zenny line read off the results screen.
