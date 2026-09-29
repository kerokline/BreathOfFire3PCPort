# Round twelve: the field modes and the battle engine, staged and merged

**Status:** IN PROGRESS (2026-09-29) - wave one, the battle side, is merged
and measured: 316 functions in seven groups, 6,237 -> 6,553 ours, every
group 0 mismatches, every control refused or an equivalent with a refused
near variant, and **the first live A/Bs a takeover round has passed on its
own code** (section 5). Wave two, the field side, is staged (its harness is
merged) and not started. The plan and the cut are
[`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) (IDEAS
I27); the tool [`band-rows.md`](band-rows.md); the harnesses
[`boss_harness.md`](boss_harness.md) section 10 and
[`scenario_harness.md`](scenario_harness.md) section 7.

## 1. Set-up (2026-09-28 night)

Branch `phase-3/capture-round-twelve` from `main` at `430f34b` (PR #32).
`main` was rewritten that night: the branch was cut at `7e382c3`, which is
the same tree (one tree hash) under a different history, and moved to
`430f34b` before any agent had committed. `BOF3X_SHADOW='*'` headless
passed at both (exit 0, `inject: 6237 ours`, `ledger_check` 0 errors) -
which is also the game-side run round eleven's cleanup owed for its
harness folds ([`round-11-cleanup.md`](round-11-cleanup.md) section 1): no
fold moved a count into a mismatch. The counts themselves were not
compared with a run before the folds.

The cut regenerated at the base (the plan's section 8) is the plan's
table, function for function: 631 in 14 groups.

The routine was round eleven's, with the scripts in the
session-`6ae930a8` scratchpad: `brief_<group>.md`, `merge_group12.sh
<group> <scratch>` (`MOD=<module>`), `verify_tip.sh`, `keepboth.py`,
`one_grow.py`, and two resolvers this round added (section 4). The brief
template is `analysis/round12_wave1_brief.md` with
`analysis/round12_wave1_addendum.md`.

## 2. Stage A: two harnesses and a tool

Three agents in parallel on three disjoint sets of files.

| Group | Merge | What | Proof |
|---|---|---|---|
| RT | `a6e3da0` | `tools/band_rows.py`, [`band-rows.md`](band-rows.md): rows, extents read from the code, clones, raw references and cross-group edges for a group of the cut | `--groups` reproduces the plan's table (631, 418 hidden, 0 ours); three clones checked by hand; BSC's K12 clones agree with `boss_rows.py` line for line |
| EH | `642def1` | `boss_harness` widened to the battle runs: `at::kEngineBands`, `Group::engine`, four shapes (`kStep`, `kWindow`, `kMember`, `kHelper`), `state_cell`, 11 regions, 120 standard stand-ins | all 126 runs of the twelve boss shadows identical before and after; `boss_harness_eh`, 11 of Capcom's functions on both sides, 44,000 rounds, 0 mismatches, nine controls refused |
| FH | `8fbd6ee` | `scenario_harness` widened to the field runs: `Group::field`, five shapes (`kSprite`, `kMenu`, `kCursor`, `kScript`, `kCall`), nine regions, 174 standard entries | all 25 scenario self-test lines identical before and after; `scenario_harness_fh`, 13 field functions on both sides, 26,000 rounds, 0 mismatches, six controls refused |

**Two changes from the plan.** RT is not in the plan: rounds nine to
eleven each had a tool that printed a group's clone tables, and nothing
did for an address band. And EH wrote no fold-back: the six had landed in
PR #31 before the round began.

**What stage A found out about the cut:**

- **Eight cut starts are not functions**, each a jump-table case of the
  function before it: `0x452460` (BE6, of `0x4523C0`), `0x44B8D0` (BE5, of
  `0x44B870`), `0x56D240` (FE2), and `0x577600`, `0x5776A0`, `0x577B50`,
  `0x578550`, `0x578790` (FO; the owners of the last six are ours
  already).
- **Nineteen functions lie in the bands and in no list**: BE1 2, BE3 1,
  BE5 6, FE2 8, FO 1, FS 1; FH found two more reached from outside their
  host, `0x5254A0` (FC3) and `0x536EC0` (FE2). The coordinator's
  decision: a group takes what its band holds.
- **Nine functions the plan counts as entered live are in no group**
  (`0x598810`, `0x4468B0`, `0x446990`, `0x4469D0`, `0x446F20`, `0x446F50`,
  `0x446F80`, `0x44FB30`, `0x44FCE0`: catalog parts 2 and 7).
- **The catalog's sizes are a guess**: 381 of 631 differ from the code by
  trailing padding, 20 in code.
- 90 calls cross between groups; none forces a merge order.

## 3. Wave one: the battle side (staged 00:50 from `642def1`, merged 01:50..04:00)

| Group | Merge | Functions | Rounds | Controls | Note |
|---|---|--:|--:|---|---|
| BE7 | `3958180` | 31 | 186,000 | 23 of 25; 2 equivalent, one variant refused | the battle windows, the gene screen; 15 standard draw callees re-listed with narrower masks |
| BE1 | `dbdbd88` | 41 (39 + 2) | 246,000 | 62 of 62 | `0x432430`, `0x432440` (the loss path) were in no list; DIV-0020's patch untouched |
| BE5 | `046d47b` | 52 (47 - 1 + 6) | 312,000 | 32 of 32 | the Dragon command's run; `EnemyAI_CondElement` renamed from a provisional name |
| BE6 | `353abd2` | 39 (40 - 1) | 234,000 | 65 of 66; 1 near-equivalent, its variant refused in 4 rounds | the transformation; **L1 is the owner's decision** (section 6) |
| BE4 | `5909d54` | 56 | 360,000 | 48 of 50; 2 equivalent, both variants refused | `0x447F40` is a list set-up, not the target picker; 70 constants rebound in 42 files |
| BE2 | `894606f` | 48 | 288,000 | 61 of 61 | the action tasks; 16 hidden starts inside hosts already ours, none held by ours |
| BE3 | `979a567` | 49 (47 + 2) | 294,000 | 93 of 93 | `0x437230`, `0x441510` were in no list; `0x442420` is 0x7F, not 0x5C0 |

Wave tip `979a567`, **6,553 ours** (`inject: 6553 ours, 0 left
original`). Every merge: the group's shadow exit 0, `'*'` exit 0,
`ledger_check` 0 errors, in the detached `verify/` worktree. Totals: **316
functions, 389 controls planted, 384 refused**, 1,920,000 rounds, seven
docs (`battle_e1.md` .. `battle_e7.md`).

`BattleE3_Inject` and `BattleE4_Inject` must stay after
`BossHarnessEh_Inject` (its self-test copies `0x4457F0`, `0x441A10`,
`0x441A30` from the image); they do.

## 4. What the merges learned

- **Rebinding as a group's first step makes merge conflicts outside the
  four appended files.** Two groups name different constants of one
  callees header (BE4 on BE6: `boss_sc_callees.h`, `boss_sf_callees.h`;
  BE2: `battle_actions_callees.h`; BE3: `enemy_ai_ops_callees.h`), or
  different slots of one table (`battle_fx_tasks.cpp`'s twelve-entry
  dispatch list, BE2 on BE6). `rebind_resolve.py` takes, line by line, the
  side that names; `table_resolve.py` the same entry by entry. Both stop
  where the two sides name one constant differently. Every resolved name
  was read back against `symbols.toml`: the values are unchanged.
  `merge_group12.sh` runs the first; the second was run by hand once.
- **The standard callees' masks are too wide where Capcom pushes a whole
  register for a byte or a word**: BE7 re-listed 15, BE1 9, BE5 4, BE4 3,
  BE2 2 with narrower masks in their own fuzz files. Owed as a fold
  (section 7).

## 5. The live checks (2026-09-29, 02:09..04:16, at `979a567`)

From a launcher copy of the tip's build (`cheat.steal=0`, the scripts'
off-list as at round eleven's tip).

| Check | Result |
|---|---|
| Frame hash, attract, ours unfocused against `r9_orig` (`analysis/calltrace/r12w1_ours`) | 1 of 10,299 frames differ: frame 0, the set-up, as since `rb1` |
| Combat A/B (`analysis/shots/combat_r12w1_*`) | 5 of 43 identical, 15 pixels at most - round eight's baseline |
| **`whelpBoss.txt`, frame hash** (`analysis/calltrace/hash_whelp_*`) | original against original identical on 13,184 frames; **original against ours 1 of 13,183: frame 0** |
| **`dragonTransform.txt`, frame hash** (`analysis/calltrace/hash2_dragon_*`) | original against original identical on 4,332 frames; **original against ours 1 of 4,332: frame 0** |

**The recipe A/Bs are frame hashes, not pictures.** The original side's
shots are screen grabs (its `Gfx_Present` is Capcom's, and
`render::SaveFrame` needs our renderer), the display had gone to sleep,
and every grab was black. The hash needs no screen. Ours wrote its own
frames on both routes (73 and 109) with no Fatal; two contact sheets
(`analysis/shots/r12w1_dragon_ours_sheet.png`,
`r12w1_whelp_ours_sheet.png`) were looked at by the coordinator, which is
a look and not a comparison.

**The dragon route's first hash differed on eight frames** (2030..2065,
every fifth; same call counts). The detail trace of frames 2028..2068
gave the same 1,590 calls in the same order on both sides; the 150
records that differed were `Rand` called from `Sparkle_Launch`
(`0x4B9090`, ours since round seven), which had no `entries_logic.txt`
line, so the tracer on the original side did not know its callers as
owned. One line added (`004B9090 161`); the re-run is the table's row. It
is one of the 34 owned starts without a line that HANDOFF lists - the
first a route has entered.

**What the routes reach** (`BOF3X_CALLTRACE_REACH=1`, all original;
`analysis/calltrace/reach_dragon`, `reach_whelp`, each with
`bof3x.calltrace.by_module.tsv`):

| Route | Starts reached | Wave one's | Round 11 (boss) | Round 10 chapters | Round 10 areas | Round 9 spells |
|---|--:|--:|--:|--:|--:|--:|
| `dragonTransform.txt` | 1,159 | 99 (BE1 4, BE2 20, BE3 25, BE4 9, BE5 15, BE6 15, BE7 11) | 3 | 9 | 3 | 93 |
| `whelpBoss.txt` | 982 | 30 | 13 (`boss_sb` 10, `boss_spawn` 2, `boss_h` 1) | 24 (`scena_sc0` 11, `scena_sc1` 4, the engine groups 9) | 6 | 1 |

`whelpBoss.txt` is the owner's, recorded 2026-09-29 00:08 without a save
header; slot 0 had been written at 00:00 and was imported as `whelp_boss`.
The route reaching its fight and its last frame settles the pairing. It is
**the first live A/B rounds ten and eleven have**: 43 of their functions,
with the boss set-up and its kinds among them.

## 6. For the owner

- **BE6's L1** ([`battle_e6.md`](battle_e6.md) section 7):
  `DragonForm_PartyRecipe` reads two stack bytes it never wrote when fewer
  than two of the other members are in. Capcom's code goes on with what
  the stack held; ours aborts. The group reads it as reachable in ordinary
  play and suggests a ledgered fix (no pair: answer 0xFF). Left as the
  abort; a fix is a DIVERGENCE entry and the owner's word.
- **The recipes wave one still wants** (plan section 5): the other
  commands of the cross, a battle lost, an item's battle use, a formation
  change. 208 of wave one's 316 functions are entered by neither route (108 by one or both).
- **A pixel A/B of the two routes** wants the display awake for the
  original side's grabs.

## 7. Debts

1. **Harness folds**: the narrower masks of section 4 into
   `kEngineStandard`; `PreviewEffect` for `Equip_PreviewSet` (BE7); the
   louder pop-up stand-ins (BE5); `Rand`'s negative answers and a turning
   stand-in for `0x446770` (BE2); the standard rows `0x437230` and
   `0x441510`, now named (BE3).
2. **The rebinding left for the coordinator**: the raw calls between this
   wave's groups (RT's `--edges`; each group doc lists its own), and the
   harness files' raw routes (`boss_harness.cpp`, `boss_harness_eh.cpp`).
3. **Number the defects** in `known-defects.md` from the seven docs'
   "latent defects". The common classes: dispatchers indexing unchecked;
   `BattleTask_Create`'s 0xFF answer untested (D163's class, eleven more
   places); divisions by a count that can be 0. The singular ones:
   `EnemyAI_DedupMessages`' eight-dword stack buffer; `Escape_Roll`
   summing enemy objects 3..10; `EnemyOp_SlideStart`'s wrong field and
   step; `BattleFx_NextStatusIcon` never returning; `Battle_RecalcStats`
   clearing enemy bytes; `DragonCmd_Slots2Close` testing window 18;
   `BattleObj_Fall`'s unchecked character byte; BE6's L1.
4. **The 33 other owned starts without an `entries_logic.txt` line**: the
   dragon route showed what one costs. Audit them before the next route.
5. **Housekeeping**: the ten `phase-3/round12-*` branches and the agents'
   worktrees under `.claude/worktrees/` are merged and can go; the
   controls scripts live in the session-`6ae930a8` scratchpad
   (`<group>/`), a Temp folder.

## 8. Wave two: the field side

Staged, not started. FH is merged; the seven briefs want the wave-one
template's battle paragraphs swapped for the field's, `<TIP>` filled, and
the addendum's field half: FE2's eight and FO's and FS's one unlisted
functions, `0x5254A0` and `0x536EC0`, the six starts that are cases. FH's
merge order, callee first: FE2, FO, FE1, FC3, FS, FC2, FC1. FH's new
stand-ins have only passed start-up registration; they first run in these
groups' fuzzes. The field side has no live route (plan section 5).
