# D56: the encounter's on-screen test gets the PlayStation's lower bounds back

**Status:** MEASURED (2026-10-13, the owner away; agent d56, branch
`away/1013-d56` off `d0d9162a`). Every claim cites the run or the bytes it
came from; runs, logs and state-hash files are under the session scratchpad
`.../scratchpad/d56/` and the two capture folders under
`analysis/shots/d56_before`, `d56_after` (game-derived, rule 1). Not yet seen
by the owner in play.

The owner's decision of 2026-10-13: build D56's repair, a restoration of the
PC's lost guard (a port repair by the [psx-twin rule](psx-twin-check.md)).
Ledger entry DIV-0116 ([`DIVERGENCE.md`](DIVERGENCE.md)); known defect
[D56](known-defects.md).

## 1. The twin, read again

The JP disc's `BIN/ETC/GAME.EMI` (`python ../BreathOfFire3Recomp/tools/disc_ls.py
"disc/Breath of Fire III (Japan).cue" --extract BIN/ETC/GAME.EMI`, 237,568
bytes, LBA 61431), section 0 at `0x80196800` (`emi.py list`), disassembled
with capstone (MIPS32 little-endian) from `0x801C805C` to its `jr $ra` at
`0x801C8178`:

| Address | Bytes | Instruction | Meaning |
|---|---|---|---|
| `0x801C8078` | `ff00c630` | `andi $a2, $a2, 0xff` | size class, a byte |
| `0x801C8084` | `80930600` | `sll $s2, $a2, 0xe` | (size + 1) << 14, as the PC |
| `0x801C80C4` | `6f20070c` | `jal 0x801c81bc` | `Encounter_Project`'s twin, the point to `0x10($sp)` |
| `0x801C80CC` | `1000a297` | `lhu $v0, 0x10($sp)` | screen x, **unsigned** |
| `0x801C80D4` | `4001422c` | `sltiu $v0, $v0, 0x140` | x < 320 unsigned: 0..319 |
| `0x801C80D8` | `1d004010` | `beqz $v0, 0x801c8150` | else return 0 |
| `0x801C80E0` | `1200a297` | `lhu $v0, 0x12($sp)` | screen y, **unsigned** |
| `0x801C80E8` | `f000422c` | `sltiu $v0, $v0, 0xf0` | y < 240 unsigned: 0..239 |
| `0x801C813C` | `1200a287` | `lh $v0, 0x12($sp)` | the fifth point's y, signed |
| `0x801C8144..4C` | | `subu`, `slti 8`, `xori 1` | y - margin >= 8, as the PC |

So a corner whose projected x or y is a negative short (left of or above the
screen) is refused on the PlayStation. The PC's `0x5928F0` compares the same
two values as floats against 320.0 and 240.0 only (group BI's reading,
[`inventory_ops.md`](inventory_ops.md) 5); ours reproduced that. The rest of
the function - the corner walk, the fifth projection, the margin test - is
the PC's instruction for instruction.

Its callers are the PC's `Encounter_Place` (`0x591F30`): a scan of section 0
for `jal 0x801C805C` finds two sites. `0x801C72A4` passes a member's spot with
size 1 when its id (`0x80144F59 + m`) is 2 or 6 and margin `0x20`, and a
refusal ends the placement (`beqz` to `0x801C737C`); `0x801C7310` passes each
of eight enemy slots with its kind's two bytes (`0x800E40CA` / `40CB`), and a
refusal stores 0 to the slot's active byte and decrements the count at
`0x801CE050`, which at zero ends the placement. That is what the PC's
`Encounter_Place` does ([`field_hidden.cpp`](../src/game/field_hidden.cpp)).
**Verdict: the port's.** The doc's earlier wording ("against 319 / 239") is
the same test: `sltiu 0x140` admits 0..319.

## 2. The repair

`src/game/inventory_ops.cpp`, `Encounter_OnScreen`: after the two upper
compares, a corner with screen x < 0.0 or y < 0.0 returns 0 while
`g_on_screen_lower_bounds` is set. A NaN still passes (it fails no ordered
compare), as the upper tests let it. The flag is a byte of ours written by
`bof3::PatchBytes("EncounterOnScreenLowerBounds", ...)` in
`InventoryOps_Inject` after `inventory_ops::SelfTest`, so the fuzz, which
compares ours against a clone of Capcom's `0x5928F0`, sees the original's
bounds - the pattern DIV-0065 set (`BattleEquipLabelsRow`). The switches:

| Setting | `Encounter_OnScreen` |
|---|---|
| default | ours, the PlayStation's bounds |
| `BOF3X_ORIGINAL=EncounterOnScreenLowerBounds` | ours, the port's bounds (the patch logs `patch OFF`) |
| `BOF3X_ORIGINAL=Encounter_OnScreen` or `*` | Capcom's function |

After the self-test every corner below 0 is logged, with the flag on or off:
`D56 Encounter_OnScreen(x, z, size s, margin m) corner k at (sx, sy): refused
(DIV-0116)`, or `passed, the port's bounds`. The log is how section 3 knows
whether a route ever reached the test; it writes nothing the game reads.

`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'` from an ini-less copy
(`scratchpad/d56/L`, the player's install): exit 0; `inventory_ops self-test:
50000 rounds over 25 functions ... 0 MISMATCHES`; `patch ON
EncounterOnScreenLowerBounds 1 bytes` after it; `inject: 10087 ours, 0 left
original by BOF3X_ORIGINAL` (`scratchpad/d56/selftest.log`). Again with the
final build, the log line in (`L5`, `selftest2.log`): the same, exit 0, and
no `D56` line during the fuzz (the log is armed with the flag, after it).

## 3. The cost, measured

The record's worry: every encounter placement can change from the first
battle on, so every battle route and state-hash reference after it would need
re-recording. Measured by running each battle route twice from one build -
ours, and ours with `--original EncounterOnScreenLowerBounds` - in the hosted
build (`BOF3X_ORIGINAL` is hosted-only, so both sides run hosted), no language
overlay, `BOF3X_OPT=none BOF3X_LAYERING=0`, `BOF3X_STATEHASH` with
`tools/statehash_skip.txt` (173 ranges), from ini-less launcher copies in the
scratchpad (`runab.sh`, `runab3.sh`, `runab4.sh`; tags `a` .. `e`; tag `a`
with the first build, before the log line, the rest with the final one -
the log is the only difference). The compare is the state hash on every
tick (`statehash.py diff on off`), the recipes' own shots pixel for pixel
where a route has them (`cmpframes.py`), and from tag `b` on the D56 log
lines on each side: with the flag off, a "passed, the port's bounds" line is
exactly a placement the repair would change, and no line means none.

A run whose runlog says `slot 0: the lock is pid N's now, not ours` had
another agent's run take slot 0 while ours was loading: three such runs
(`whelpBoss_on_b`, `gameover_off_c`, `combatGroup_on_b`) differed from their
own repeats on every page from tick 2 or ~170 (the loaded machine's slot
race, not the build) and are set aside; each route has a clean pair below.

| Route | Clean pair (tag) | State hash, on against off | Shots | D56 lines (on / off) |
|---|---|---|---|---|
| combat | a, b | a: sound page `0x7DE000` on 9 ticks, `0x7E1000` at the last tick (2,621); b: sound, `D3d_Scale*`, VRAM-shadow pages | none recorded | 0 / 0 (b) |
| combat_exp | a, b | VRAM shadow, sound, `0x937000` at ticks 2..5 (the load); sound later | 51 / 51 identical (a and b) | 0 / 0 (b) |
| gameover | a, e | a: **identical on all 3,073 ticks**; e: sound page on one tick | 3 / 3 identical (a and e) | 0 / 0 (e) |
| whelpBoss | d | sound page on 8 ticks, VRAM shadow and `0x937000` on two | none recorded | 0 / 0 (d, b's off) |
| bossAndFlash | b | sound, VRAM shadow, `0x937000` on ticks 2,233..4,278 | none recorded | 0 / 0 (b) |
| balioAndSunder_1 | a, b | a: VRAM shadow, sound, `0x937000`; b: **identical on all 6,273 ticks** | none recorded | 0 / 0 (b) |
| balioAndSunder_2 | c | sound, `D3d_Scale*`, VRAM shadow, `0x937000` on 4 ticks | none recorded | 0 / 0 (c) |
| cutsceneAndNue | a, b | VRAM shadow, sound, `0x937000` | 5 / 5 identical (a and b) | 0 / 0 (b) |
| combatGroup | a, e | a: ticks 2..3 only; e: **identical on all 577 ticks** | none recorded | 0 / 0 (e) |

The pages that differ are the noise class the state-hash doc and
`abc-sweep-1012.md` 6 name for a loaded machine - the sound page `0x7DE000`,
the VRAM shadow (`0x7B2000..0x7C8000`), `Gfx_CurrentEnv`'s page `0x937000`,
`D3d_Scale*` - and none is the placement's: no encounter slot, party spot or
random-draw page differs on any route. With the flag off **no run logged a
single corner below 0**, so no route meets a placement the repair changes;
with it on, none was refused. **Per route: no placement differed.**

Which routes reach the placement at all: `combat`, `combat_exp` and
`gameover` (the `combat` save, a few steps, a random encounter; the crafted
run of section 4, which is `combat.txt`, logs the test for that encounter).
The boss routes' fights are event battles; whether they pass through
`Encounter_Place` is not established and does not change the answer.
`combatGroup` has no `# save` line: played with `fairyVillage` (the
install's usual slot 0, `abc-sweep-1012.md` 7) it ends at frame 622 with
no draw after the load's (`randlog`), so it reaches no encounter as run.

Against Capcom's side (`statehash.py check cn_R_orig2.sh cn_R_orig2b.sh`,
ours with the flag on): **combat** - VRAM shadow and `0x937000` at tick 3,
the sound page on two ticks: clean. **whelpBoss** - VRAM shadow, sound and
`0x937000` only: clean. **cutsceneAndNue** - besides those, `0x905000`
(`CameraTurn_Steps` ...) from tick 136 on 7,225 ticks and `0x904000` /
`0x665000` / `0x6BD000` (`Music_Track`, `Music_LoadedTrack`, `Music_File*`)
over ticks 204..1,032. Not this change: on and off are identical on those
pages, the flag acts only on a corner below 0 and none was logged, and an
exe-host run (tag `x`) shows the same; the 2026-10-10 run
`cn_cutsceneAndNue_cu1010x` was identical to Capcom's on every tick, so a
change merged since then moved them (section 6).

## 4. A capture that shows it

No route reaches the case (section 3), so it was crafted: `combat.txt` with
shots every 20 frames from 300 to 600 (`recipe_shots.py`) and `poke 0x903800
u16 0xFE00` (`Camera_ShiftX`, -512) on recipe frames 378..385, just before
the route's encounter (its first random draws after the walk are at frames
384 / 385, `randlog`). Zero-frame `poke` lines shift nothing
(`input_script.cpp`'s `Kind::Poke` advances without a frame).

- **Before** (`--original EncounterOnScreenLowerBounds`,
  `analysis/shots/d56_before`): at frame 382 `Encounter_OnScreen(1638400,
  2097152, size 0, margin 40)` has all four corners at x -40.0 .. -61.5, each
  "passed, the port's bounds"; frame 500 shows the fight against the purple
  Gooey - its name box at the top left - with the Gooey nowhere on screen.
- **After** (the default, `analysis/shots/d56_after`): the same corner is
  "refused" at frame 382 and that encounter does not start; at frame 385 the
  next try's enemies at x -23.0 and -60.6 are refused and one is left; frame
  500 shows a fight against one winged enemy, on screen.

Shifts of -256 (`0xFF00`) and +512 (`0x0200`) put no corner below 0; -1024
and -2048 did (three refusals each, `trials_shx.out`). Turning the camera
instead (`Camera_Angles[2]`, `0x929ECC`, eight values from 0 to 0x700 over the
same frames) put none below 0 (`trials.out`): the placement's own jitter and
the aim keep a normal view's fight on screen.

Each folder has `README.md`, the recipe, the run's log and a contact sheet.

## 5. What would need re-recording

**None.** The state-hash references in `analysis/statehash/`
(`cn_<route>_orig2`, `_orig2b`) are Capcom's side, recorded under
`BOF3X_ORIGINAL`, which runs Capcom's `Encounter_OnScreen`: this change
cannot move them, so they stand whatever a route does. What could move is
ours *against* them, and only from an encounter the repair changes; no
recorded route has one (section 3), so every `statehash.py check` reads as it
did before this change. The recorded battle routes (`tools/recipes/*.txt`)
replay the same frames on and off, so none needs recording again either.

Should a route recorded later meet such a placement, its run logs `D56 ...
refused` at the encounter and `statehash.py check` will report the
encounter's pages from that tick: an expected difference, the ledger's, not
a regression. Comparing it against our own side with the flag off is the
check (`--original EncounterOnScreenLowerBounds`, the hosted build), as
section 3 does.

## 6. Not reached

- The owner's eye in play (owner-review).
- An encounter in the wild that trips the test: none of the routes does, and
  the case was found only by moving the camera. Where in the game a normal
  view can place an enemy off the left (narrow maps, area 4's walls) is not
  measured; the D56 log line now names it whenever a run meets one.
- The PSX's own frame of such an encounter (an emulator capture) - the twin's
  bytes settle the question without it.
- **Found on the way, not this change's (for the coordinator):** today's
  build against Capcom's `cn_cutsceneAndNue_orig2` pair differs on `0x905000`
  (`CameraTurn_Steps`, `Field_StepCount`, `CameraTurn_Angles`, ...) from tick
  136 to the end and on the music pages (`Music_Track`, `Music_LoadedTrack`,
  `Music_File*`) over ticks 204..1,032, in both hosts, where the 2026-10-10
  run `cu1010x` was identical. The music pages may be the event-battle music
  fix (PR #54) or the field-music entries; the camera page is not explained
  here. A `BOF3X_STATEHASH_DUMP` at tick 136 on both sides and
  `statehash.py bytes` would name the bytes.
  **Settled** ([`nue-regression-1013.md`](nue-regression-1013.md)): no
  regression - `0x905000` was the pair gone stale on the save list (slot 6
  rewritten 07:10; re-recorded), the music pages DIV-0092; with it off, ours
  is identical to the new pair on every tick.
