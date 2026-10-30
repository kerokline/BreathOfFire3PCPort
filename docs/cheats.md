# Cheats — EXP and zenny multipliers, a steal that always lands, and every enemy's information shown

**Status:** BUILT + CONFIRMED IN GAME (2026-09-24, the recorded combat route,
watched by the owner). 2026-10-03: the cap 10 and the two boss hooks (§2a,
§2b) built and checked headless; the live run in §5 is owed. 2026-10-29: the
enemy-information switch (§6) built and checked headless; not yet played.

The archival sibling ships three cheats as mods: EXP and zenny multiplier
sliders (its `docs/EXP_BOOST.md`) and a "steal always succeeds" patch (its
`docs/STEAL.md`). This is the same three as launcher settings here - the
Cheats sub-page under Game Options, with two sliders and one switch - and
since 2026-10-29 a fourth of our own, every enemy's HP gauge and stats shown
(§6). Ledger entries DIV-0045, DIV-0046 and DIV-0187.

## 1. Where each one lives

| Setting | `bof3x.ini` | Environment | Default | Done by |
|---|---|---|---|---|
| EXP won, times | `cheat.exp=` 0..10 | `BOF3X_EXP=n` when not 1 | 1 | our `Battle_EnemyDefeated` `0x437470`, [`battle_flow.md`](battle_flow.md); and two boss hooks that write the EXP total themselves, `Boss16_End` `0x43A190` and `BossWeretigr_EndMove` `0x43D5A0` (§2a) |
| Zenny won, times | `cheat.zenny=` 0..10 | `BOF3X_ZENNY=n` when not 1 | 1 | our `Battle_EnemyDefeated` only (§2a: no other writer) |
| Steal always succeeds | `cheat.steal=` 0 / 1 | `BOF3X_STEAL=1` when on | off | two `PatchBytes` of one immediate each, `src/game/cheats.cpp` |
| Every enemy's HP bar and stats shown | `cheat.enemy_info=` 0 / 1 | `BOF3X_ENEMY_INFO=1` when on | off | our `Battle_SetupEnemy` `0x494320` and `Battle_PlaceBossActor` `0x494570` write the enemy's byte `+0x8F` (§6) |

Unset, nothing here changes a byte or a number: every harness and oracle
run leaves the variables unset, and the `battle_flow` fuzz compares ours
against the original with the multipliers at 1. A multiplier that is not a
whole number 0 or more is a `Fatal`, as a wrong `BOF3X_FILTER` is; one above
10 is clamped to 10 with a log line (§2b).

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
(2026-09-13); not tried here yet. 10 is the ceiling since 2026-10-03 (§2b);
it was 50, the sibling's.

### 2a. The EXP that does not come through `Battle_EnemyDefeated` (2026-10-03)

The owner, 2026-10-02: Balio and Sunder's second fight ignores
`cheat.exp=0`. An immediate scan of `.text` for the totals' addresses
(`0x904AEC`, `0x904AF0` as 32-bit operands; group CH's scratch
`scan.py`, capstone) finds every instruction that names them. Leaving out
the readers and the zeroing (`0x42E5xx` battle set-up, ours since round
eleven as `battle_phases.cpp`; `0x4319xx` / `0x431Exx..0x4320xx`, the result
screen, ours as `battle_result.cpp`), three places **write** the EXP total:

| Address | Function (ours since) | What it writes |
|---|---|---|
| `0x437482..` | `Battle_EnemyDefeated` (round seven, `battle_flow.cpp`) | `+=` the fallen enemy's `+0x96` - scaled since DIV-0045 |
| `0x43A1B3` | `Boss16_End` (round eleven, `boss_sc.cpp`) | **`=`** enemy 1's `+0x96` + enemy 0's, on a win (`0x904AE8` bit 1) |
| `0x43D618` | `BossWeretigr_EndMove` (round eleven, `boss_sa.cpp`) | `+=` the acting enemy's `+0x96`, with `0x904AE8 \|= 2` |

and one writes the zenny total, `Battle_EnemyDefeated` (`+0x94`), besides
the result screen's own half-again bonus, which reads the scaled total.

`Boss16_End` is the end hook of `Boss_SetupTable[16]`, the second fight of
`BOSS013` (Balio, Sunder and Nina, kinds 13, 14, 17;
[`boss_sc.md`](boss_sc.md) §1.3). Balio's and Sunder's hit hooks put HP 0
back to 1 in any fight, so in fight 16 their EXP should never pass through
`Battle_EnemyDefeated` (read, not measured); the end hook writes the
battle's EXP outright from the two records, and because it stores rather
than adds, it replaces whatever `Battle_EnemyDefeated` had accumulated
either way. So under `cheat.exp=0` the second fight paid the full award:
the report, explained by the code. `BossWeretigr_EndMove`
(fight 33, Weretigr, [`boss_sa.md`](boss_sa.md)) is the same class: an
end-of-fight walk that adds the enemy's EXP itself.

Both now multiply by `Cheats_ExpMultiplier()` (DIV-0045, amended): the
second fight's sum times n, Weretigr's addend times n. Zenny needs nothing:
neither hook touches `0x904AF0`, so fight 16 pays only what
`Battle_EnemyDefeated` paid (scaled), as the original pays only that.

**The first fight** (`Boss_SetupTable[13]`, the owner's
`balioAndSunder_1.txt`): its end hook `Boss13_End` `0x43A000` writes no
total (`0x903848 = 0x32`, then `0x446DE0` or `0x446E20` by the win bit -
symbols.toml), and its set-up gives Balio and Sunder HP `0xFFFF` with the
same HP-0-to-1 hit hooks. So by the code the first fight has no EXP path
but `Battle_EnemyDefeated`, which honours the switch. Not measured: §5
names the run that would show it.

### 2b. The cap: 10 (2026-10-03)

The owner, 2026-10-02: 50 is humorously large for this game; stop at 10.
The launcher's sliders run 0..10 with a tick at each step. An ini saved
while the cap was 50 may hold more (the owner's own had `cheat.zenny=50`):
the launcher reads any whole number up to 1,000,000 and passes it on, the
DLL clamps it to 10 and logs `DIV-0045    BOF3X_ZENNY=50 is above the cap,
10 used`, and the Cheats dialog shows the slider at 10 and saves 10 when
OK'd. A value set by hand in the environment is treated the same. Not a
`Fatal`: the owner should not be locked out of the game by their own file.
Text that is not a number, or a negative one, is still a `Fatal`.

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
- 2026-10-03 (fix wave, group CH), headless, `BOF3X_SELFTEST_ONLY=1`:
  `BOF3X_EXP=0 BOF3X_ZENNY=50` logs `DIV-0045    BOF3X_ZENNY=50 is above
  the cap, 10 used` and `EXP x0, zenny x10`, and starts; `BOF3X_EXP=abc` and
  `BOF3X_ZENNY=-1` are `FATAL: ... must be 0..10`. With the variables unset
  the `boss_sc`, `boss_sa` and `battle_flow` fuzz pass, 0 mismatches. With
  `BOF3X_EXP=0` the `boss_sc` fuzz mismatches in `Boss16_End` only (3,033
  of 24,000 rounds) and `boss_sa` in `BossWeretigr_EndMove` only (2,007 of
  6,000), the first differing byte `0x904AA0 + 0x4C` = `0x904AEC` each time:
  the multiplier now reaches both stores and changes nothing else those
  fuzz groups cover. `BOF3X_SHADOW='*'` with the variables unset: exit 0, 997 fuzz summaries all at 0
  mismatches, 7,687 ours (narrow; nothing here is widened by DIV-0041).

## 5. For the coordinator's live check (owed, 2026-10-03)

Headless only in the fix wave, so the in-game half is prepared here, not
run. Two questions: does the second fight now pay 0 EXP under
`cheat.exp=0`, and does the first fight (which, by §2a, has no other EXP
path) pay 0 too.

**The settings.** The brief says the owner recorded both recipes with
`cheat.exp=0`, `cheat.zenny=50`, `cheat.steal=1`; the main checkout's
`build/bof3x.ini` read `exp=1 zenny=1 steal=1` on 2026-10-03, and the
`recipes_1003` / `reach_*_1003` runs' logs have the DIV-0046 line and no
DIV-0045 line, so they ran with both multipliers at 1. Pass the recording's
values in the environment, which the launcher never overrides. On this
build `BOF3X_ZENNY=50` becomes 10 (§2b): a smaller purse could only change
the replay if the route spends money, which is not known (a guess - the
owner would know).

**The recipe copies.** The group's scratch script `recipe_peeks.py`
(`<session scratchpad>/fixwave/ch/`; copies already made there as
`balioAndSunder_1_peeks.txt` and `balioAndSunder_2_peeks.txt`) inserts,
every 30 frames, `peek` lines for the EXP total `0x904AEC` (u32), the zenny
total `0x904AF0` (u32), the battle-end bits `0x904AE8` (u8; bit 1 = won)
and the fight id `0x904AAA` (u8; `0x0D` the first fight, `0x10` the
second). A peek takes no frame (`input_script.cpp`, `Kind::Peek`) and a
split `wait`/`hold` plays the same frames, so the copy is the recording
frame for frame (6,327 and 21,834 frames, both checked). For pictures as
well, run `tools/recipe_shots.py` on the owner's recipe first and
`recipe_peeks.py` on its output (it passes `shot NAME 1` frames through).

**The runs**, from the main checkout, with this branch's build:

    python tools/input_run.py <scratch>/fixwave/ch/balioAndSunder_2_peeks.txt --out analysis/shots/fix1003_ch/bs2 --lang en --no-front --env BOF3X_FILTER=point --env BOF3X_EXP=0 --env BOF3X_ZENNY=50 --env BOF3X_STEAL=1
    python tools/input_run.py <scratch>/fixwave/ch/balioAndSunder_2_peeks.txt --out analysis/shots/fix1003_ch/bs2_orig --lang en --no-front --original Boss16_End --env BOF3X_FILTER=point --env BOF3X_EXP=0 --env BOF3X_ZENNY=50 --env BOF3X_STEAL=1
    python tools/input_run.py <scratch>/fixwave/ch/balioAndSunder_1_peeks.txt --out analysis/shots/fix1003_ch/bs1 --lang en --no-front --env BOF3X_FILTER=point --env BOF3X_EXP=0 --env BOF3X_ZENNY=50 --env BOF3X_STEAL=1

(the `# save` header comes through the copy). Read the `input       peek`
lines out of `build/bof3x.log` after each, and check it also has
`DIV-0045    BOF3X_ZENNY=50 is above the cap, 10 used`.

**What right and wrong look like.**

- Second fight, ours: once `fight` reads `0x10` and `battle_end` gains bit
  1 (`& 2`), `exp_total` stays 0 through the result screen. Wrong: a
  non-zero `exp_total` after the win.
- Second fight, `--original Boss16_End` (Capcom's store): `exp_total`
  jumps to the two enemies' sum at the win - the owner's report reproduced,
  the before half of the A/B.
- First fight: `fight` `0x0D`; `exp_total` 0 throughout. Whether that fight
  ends in a win at all (`battle_end & 2`) is worth reading off too - if bit
  1 never sets, the first fight pays no EXP in any setting, cheat or not.
- `zenny_total`: whatever the kills paid, times 10 now (it was times 50);
  in the second fight probably 0 throughout (the same hit hooks, so no
  `Battle_EnemyDefeated` for Balio or Sunder - read, not measured).

**Frames to shoot.** The result screen's frame is not known before the
first run. From the second fight's peeks take the frame F where
`battle_end` first has bit 1; a `recipe_shots.py --every 30` copy shot over
F..F+600 shows the EXP line (`BattleResult_SplitExp` first turns the total
into a member's share and the counter ticks it down, so a right picture
reads 0 and a wrong one the share). For the owner: one picture of the
second fight's result screen on ours under `cheat.exp=0`.

## 6. Every enemy's HP gauge and Identify stats (DIV-0187, 2026-10-29)

A player asked for every enemy's HP bar shown instead of hidden. The owner
named the lever: the enemy's "discovered" byte, which also opens Identify's
panel and the bosses' gauges.

**What the byte is.** Each enemy record's byte `+0x8F` (`records.h`
`BattleEnemy::kind_flag`). Three readers, all ours:

| Reader | Reads it as |
|---|---|
| `EnemyGauge` in `BattleWin_DrawTargetEnemy` `0x443B10` and `BattleWin_DrawEnemyStatus` `0x443D90` (`battle_windows.cpp`) | 1: the HP bar; else the ":" glyph of value 0xFFFF |
| `Identify_DrawEnemy` `0x4B11F0` (`magic_s12.cpp`) | not 0: the six element glyphs, EXP and zenny; else question marks |
| `Identify_Roll` (`magic_s12.cpp`) | not 0 and the roll hits: `Identify_MarkSeen`; else the cast fails |

Two writers, both ours: `Battle_SetupEnemy` `0x494320` copies
`Battle_EnemyKindFlag(kind)` `0x494EA0` - bit `kind` of the set at `0x904068`
(the save's flag block, `Cond_Flags + 0xD8`), which `Battle_EnemyDefeated`
`0x437470` sets through `Battle_SetEnemyKindFlag` `0x494ED0` (its only
caller) when one of that kind falls. So the byte reads "a kind the party has
beaten before", and a kind shows its gauge and stats from its second
meeting. `Battle_PlaceBossActor` `0x494570` writes 0 for an event battle's
actors: a boss never shows either, except Beyd's two fights, whose enter
hooks write 1 (`boss_se.md`).

**The switch.** `Cheats_Inject` reads `BOF3X_ENEMY_INFO` (0 or 1, unset is
off, anything else a `Fatal`) into `Cheats_EnemyInfoShown()`. With it on,
`Battle_SetupEnemy` writes 1 after reading the kind's bit (the read stays,
as the original's does), and `Battle_PlaceBossActor` writes 1 instead of 0.
Nothing writes the bit set: the save still records what the party beat, and
with the switch off the game is where it would have been. Each module's
inject checks that its writer is ours when the switch is on
(`BattleSprites_Inject`, `Rest3G_Inject`): a `BOF3X_ORIGINAL` or `BOF3X_OFF`
that leaves Capcom's writer in place is a `Fatal`, never a cheat half on.

Why per spawn and not the bit set: setting every kind's bit would rewrite
the player's save at the first battle, lose the kill record for nothing, and
still miss the bosses, whose writer ignores the set.

**Verification.** 2026-10-29, headless (`BOF3X_SELFTEST_ONLY=1`, a clean
copy of the four launcher files under `C:\Users\kerok\bof3x-runs\ei1029`):

- unset, `BOF3X_SHADOW='*'`: exit 0, every fuzz group at 0 mismatches;
- `BOF3X_ENEMY_INFO=1 BOF3X_SHADOW=battle_sprites`: the log line `DIV-0187
  every enemy shows its HP gauge and its Identify stats, bosses too`, and
  the fuzz differs in `Battle_SetupEnemy` alone, 2,237 of 3,000 rounds (the
  rounds whose kind bit was 0), call logs 5 / 5 (the bit is still read), 31
  other functions at 0;
- `BOF3X_ENEMY_INFO=1 BOF3X_SHADOW=rest_3g`: differs in
  `Battle_PlaceBossActor` alone, the first differing byte each time `0x93B960
  + 0x128 n + 0x8F` - the record's `+0x8F`, nothing else;
- `BOF3X_ENEMY_INFO=2`: `FATAL: BOF3X_ENEMY_INFO must be 0 or 1`;
- `BOF3X_ENEMY_INFO=1 BOF3X_ORIGINAL=Battle_SetupEnemy` and
  `=Battle_PlaceBossActor`: `FATAL: BOF3X_ENEMY_INFO=1 needs <writer> ours,
  and it is disabled`.

Owed: the owner's eye - an ordinary fight with a kind never beaten (the bar
under the name from turn one) and a boss (the bar, and Identify's numbers);
the Cheats sub-page's new group, 176 dialog units high now.

## 7. The minigames cannot be failed (DIV-0188, 2026-10-30)

The owner's call, with the groundwork laid for every minigame: one switch,
`BOF3X_NO_FAIL=1` (`cheat.no_fail=1`, the "You cannot fail a minigame" box
on the Cheats sub-tab's Page 2), read by `Cheats_Inject` into
`Cheats_MinigameNoFail()` (0 or 1, unset is off, anything else a `Fatal`).

**Why per minigame.** The engine has no "minigame" state. Each minigame is a
state of its own subsystem, armed by its area's or chapter's own code, and
judges the player's attempt in a step of its own
([`button-tests.md`](button-tests.md) 2.2):

| Minigame | Where it lives | Judged in | Under the switch |
|---|---|---|---|
| the rope pull (area 75) | `Field_ModeTailKinds[30]`, armed by `Area75_ArmTail30` | `Area75_PhasePlay` `0x40E330`: more than three presses out of turn, more than 0x1E idle frames or the counters more than 0xC8 apart send the pull to `Area75_PhaseMissed` | **yes**: the judgement skipped, the pull runs until the player's counter reaches the mark |
| the lighthouse valve (area 77, chapter 6) | effect kind `0x45`, the oscilloscope gauge, spawned by `Scena06`'s run (`scena_sc6.cpp` 1626), which then steps on its row's flag 0x26: set, "The main valve is open!"; clear, "Whoops! The boiler stopped" | `EffectKind45_Aim` `0x477370` (`effect_2d.cpp`): a confirm press sets flag 0x26 only with the trace's offset at 8 and its phase within 0x1C..0x24; `EffectKind45_Tune` `0x477250`, the state before it: a confirm press ends the gauge with the flag as it was (clear) - too soon | **yes**: a confirm press sets the flag in either state; cancel stays a cancel |
| the chop (area 8, chapter 1) | effect kind `0x55`, the thirty-second clock and the count, spawned with kind `0x15` by `Scena01_Scene06` step 0x11; step 0x14 reads the cue byte `0x90384A`: bit 7 the win (step 0x19), clear the second attempt (step 0x15) | `EffectKind55_Finish` `0x47EB30` (`effect_2g.cpp`): at the clock's end bit 7 of the count `0x90384B` always, of the cue only at a count of 16 or more | **yes**: the cue's bit 7 at any count |
| the well (area 68) | effect kind `0x8C` | not read yet | no route yet |
| the crane (area 135), the antenna dial, the ram (area 121), the dock's parts menu (area 128) | the button round's sites | not read yet | no route yet |

The lighthouse is not among the button round's seven sites: its gauge reads
the confirm and cancel button sets (`Field_ConfirmButtons`, `Field_CancelButtons`),
not a hard-coded bit, so the Config swap never touched it. Finding it was a
replay of the owner's recording with `watch` and `peek` lines (the area word
`0x904EFC` 0x4D, the live effect slots' kinds): no tail kind changes in the
area, and the only effect alive through the meter is kind `0x45`.

A minigame joins the switch when a recorded route reaches it (the owner
records the start with `BOF3X_RECORD`, [`button-routes.md`](button-routes.md)
5), its judgement is read, and a variant of the route that loses without the
switch is seen to go on with it. The module that owns a guarded function
checks at inject that the function is ours when the switch is on
(`AreaW1f_Inject`, `Effect2D_Inject`): a `BOF3X_ORIGINAL` or `BOF3X_OFF` that
leaves Capcom's judgement in place is a `Fatal`, never a cheat half on.

**The rope pull.** `Area75_PhasePlay` runs the move's presses, counts the
move's frames down, and - unless the press flags' bit 8 - judges: more than
three early presses (`0x93C347`), more than 0x1E idle frames (`0x93C348`) or
`CounterGap()` over 0xC8 set the phase byte `0x93C340` to 3, and
`Area75_PhaseMissed` ends the attempt (Cond row 10 flag 1, tail state 0x1E).
With the switch on the function returns before that judgement. The moves,
the rhythm table, the two counters, the sounds and the other side's pulling
are as they were; `Area75_PhaseDeal` still ends the pull at the mark
(the player's counter at 0x1F4 or less, phase 4).

**Verification.** 2026-10-30, headless (`BOF3X_SELFTEST_ONLY=1`, a clean
copy of the four launcher files under `C:\Users\kerok\bof3x-runs\rope1030\T`):

- `BOF3X_NO_FAIL=1 BOF3X_SHADOW=area_w1f`: the log line `DIV-0188 the
  minigames cannot be failed (BOF3X_NO_FAIL)`, and the fuzz differs in
  `Area75_PhasePlay` alone, 3,490 of 6,000 rounds, the first differing byte
  each time `0x93C340` (the phase byte), the group's 30 other functions at 0;
- `BOF3X_NO_FAIL=2`: `FATAL: BOF3X_NO_FAIL must be 0 or 1`;
- `BOF3X_NO_FAIL=1 BOF3X_ORIGINAL=Area75_PhasePlay`: `FATAL: BOF3X_NO_FAIL=1
  needs Area75_PhasePlay ours, and it is disabled`;
- `BOF3X_NO_FAIL=1 BOF3X_SHADOW=effect_2d`: differs in `EffectKind45_Tune`
  (3,179 of 6,000 rounds) and `EffectKind45_Aim` (4,794) alone, the group's
  52 other functions at 0; `BOF3X_ORIGINAL=EffectKind45_Aim` with the switch:
  `FATAL: BOF3X_NO_FAIL=1 needs EffectKind45_Tune and EffectKind45_Aim ours,
  and one is disabled`;
- unset, `BOF3X_SHADOW='*'`: the rope pull's build, 766 groups, 0 mismatches,
  exit 0 (about 25 minutes); the build with both minigames, again 766 groups, 0 mismatches, exit 0.

Live, the same day, from the owner's two recordings (`tools/recipes/ropeMiniGame.txt`,
`lighthousePowerMiniGame.txt`, their saves in `tools/recipe_saves/`), replayed on
a clean launcher copy at x8 / x4 with shots (`C:\Users\kerok\bof3x-runs\rope1030`,
the sheets `sheet_idle_*.png`, `sheet_l_*.png`):

- the rope pull, the player's first turn of presses (recipe frames
  1,634..1,762) made idle (`rope_idle.txt`): unset, "You failed..." at frame
  1,740 and the foreman's "if you feel like trying again"; with the switch the
  pull runs on - the other side pulls ahead, the counters turn red past 0x96
  apart, the player's later turns pull back - no loss through frame 3,300;
- the lighthouse, the valve press (frame 817) moved 40 frames early and 40
  frames late (`light_early.txt`, `light_late.txt`): unset, "Whoops! The boiler
  stopped" both ways; with the switch, "The main valve is open!" both ways;
- the recordings as made (a pull won, a valve opened) replay the same with the
  switch unset, under the owner's ini and under none.

**The chop** (2026-10-30, afternoon; the owner's recording
`tools/recipes/bunyanHouseChoppingMiniGame.txt`, its save in
`tools/recipe_saves/`). The recording replayed with the scenario counters
watched: area 8 from recipe frame 251, the chop's cue `0x90384A` first set at
11,495, a hit about every thirty frames, the count at 0x15 (21) by 12,370, the
clock's end at 12,396 - the cue's bit 7 and the count's together, so the
judgement is `EffectKind55_Finish`, not the scene (the docs' earlier guess of
`Scena01_Scene06` step 0x12 was the swing). Under the switch the function
sets the cue's bit 7 at any count. Verification (`C:\Users\kerok\bof3x-runs\rope1030`):

- `BOF3X_NO_FAIL=1 BOF3X_SHADOW=effect_2g`: differs in `EffectKind55_Finish`
  alone (746 of its 4,000 rounds), the group's other 57 functions at 0; unset,
  232,000 rounds over 58 functions, 0 mismatches, exit 0; `BOF3X_NO_FAIL=1 BOF3X_ORIGINAL=EffectKind55_Finish`: `FATAL:
  BOF3X_NO_FAIL=1 needs EffectKind55_Finish ours, and it is disabled`;
- live, every pad word of the chop's window (11,495..12,396) cleared
  (`chop_nopress.txt`, the sheets `sheet_chop_np_*.png`): unset, the count's bit 7 alone at 12,396, the cue's clear, Bunyan's "Humph! Only 0...?", "I'm afraid you haven't learned your lesson yet...", "Try again" and a second thirty seconds from 12,848;
  with the switch, both bits at 12,396, Bunyan's "Hmmm... 0", "Not bad...", Teepo's "There! We worked, didn't we" and the scene's end;
- the recording as made (21 hits, the chop won) replays the same with the
  switch unset.

The count's one-glyph word after the number, which the owner saw during the
same recording, is a localisation defect, not a cheat's business: DIV-0064's
group 25 (the same afternoon) draws the chop's count box as the disc does -
the West's narrower box and the count alone, the Japanese build's glyph only
under its own layer. The owner's install picks it up when the launcher
rebuilds the stale `loc` step at its next setup prompt.

The launcher: the cheats on two pages since this change (the owner's call,
so the page does not crowd) - the Cheats sub-tab carries a third row of
buttons, Page 1 and Page 2 (the owner's word over a second Cheats sub-tab,
the same afternoon): Page 1 holds the battle rewards and stealing, Page 2
the enemy information (DIV-0187) and the minigames; either page's Defaults
resets every cheat. The sub-tab buttons are placed per tab, each as wide as
that tab's longest label, and the third row likewise under them; the row
shows only over a sub-tab with more than one page (`config_dialog.cpp`
`SubPage::sheet`, `IDC_SHEET0`). `tools/launcher_host/dialog_test.py` checks
the rows and both pages (`cheats_page1.png`, `cheats_page2.png`) and passes
as on `main` (its two mod-zip failures are `main`'s too, 2026-10-30).

Owed: the owner's eye on the two pages, on a pull, a valve or a chop played
with the box on, and on the chop's count box under `en-US` once the layer is
rebuilt. Next under the switch: the well (effect kind `0x8C`), when a
recording reaches it.

