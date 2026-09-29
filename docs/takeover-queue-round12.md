# Round twelve: the field modes and the battle engine, staged and merged

**Status:** MEASURED (2026-09-29) - the takeover is complete: **654
functions in fourteen groups over two waves, 6,237 -> 6,891 ours**, every
group 0 mismatches, every control refused or an equivalent with a refused
near variant (1,513 of 1,529 refused), and the tip live-checked on the
attract sequence and five recorded routes - frame hashes identical to the
original but frame 0, pictures at their baselines (sections 5 and 9). The
plan and the cut are
[`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) (IDEAS
I27); the tool [`band-rows.md`](band-rows.md); the harnesses
[`boss_harness.md`](boss_harness.md) section 10 and
[`scenario_harness.md`](scenario_harness.md) section 7. The debts are
section 7; round thirteen is [`takeover-queue-round13.md`](takeover-queue-round13.md).

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

- **BE6's L1, decided 2026-09-29** ([`battle_e6.md`](battle_e6.md)
  section 7, DIV-0063): `DragonForm_PartyRecipe` reads two stack bytes it
  never wrote when fewer than two of the other members are in. Capcom's
  code goes on with what the stack held; ours aborted as merged. The
  owner's account - the gene with one partner standing fails as the two
  failing pairs do, into the default dragon - makes it answer 0, the
  failing pairings' answer, and not the 0xFF the group suggested. **Owed:
  the owner's check in game**, once a save has the gene and a full party.
- **FS's reserve list** (`field_s.md` L2: a fourth entry garbles it) is **not reachable**: six characters at most, three in the party (the owner, 2026-09-29).
- **The Config screen under a language overlay** has not been seen in game through ours (`Config_DrawRowLabel`, FC1).
- **The recipes the round still wants** (plan section 5): the field side's - an event that gives zenny, one that costs HP, steering in a crowded town, a jump, the shop's equip and sell screens, a save point - and the battle side's: the other
  commands of the cross, a battle lost, an item's battle use, a formation
  change. 208 of wave one's 316 functions are entered by neither route (108 by one or both).

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
   `BattleObj_Fall`'s unchecked character byte; BE6's L1 (fixed, DIV-0063).
4. **The 33 other owned starts without an `entries_logic.txt` line**: the
   dragon route showed what one costs. Audit them before the next route.
5. **The pointer scan** of section 8 over all fourteen bands, and its two cases folded into `tools/band_rows.py`.
6. **Wave two's folds** into `scenario_harness`: the masks, `Zenny_Add`'s test, the confirm and cancel cells as a region, `Crt_sprintf` at three words, the GTE rows that log stack pointers; and wave two's defects with wave one's in item 3.
7. **Housekeeping**: the seventeen `phase-3/round12-*` branches and the agents'
   worktrees under `.claude/worktrees/` are merged and can go; the
   controls scripts live in the session-`6ae930a8` scratchpad
   (`<group>/`), a Temp folder.

## 8. Wave two: the field side (staged from `61be26e`, merged 06:58..08:35)

`61be26e` is wave one's tip with DIV-0063 (section 6). The briefs were
wave one's with the field paragraphs and what wave one had learned
(`analysis/round12_wave2_brief.md`); merges by `queue2.sh`, callee groups
first where they had reported.

| Group | Merge | Functions | Rounds | Controls | Note |
|---|---|--:|--:|---|---|
| FC2 | `b7faa70` | 44 (40 + 4) | 264,000 | 166 of 166 | four effect-pool kinds; three `Effect_KindHandlers` dispatchers and a state-table entry were in no list and the tool did not flag them |
| FS | `c63e350` | 53 (52 + 1) | 212,000 | 183 of 188; 5 equivalent, variants refused | `0x58CAE0` after `0x58C7A0` (0x338 bytes, not 1440); DIV-0011's retargeted call inside `0x581300` holds; the confirm and cancel cells were in no standard region |
| FE2 | `7fc5a32` | 51 (44 - 1 + 8) | 306,000 | 132 of 135 by a count, 2 by a crash of ours with variants by a count, 1 equivalent | DIV-0023 amended for four more functions; `SC11_THEIRS` -> `SC11_OURS` |
| FC3 | `7402249` | 63 (57 + 1 + 5) | 189,000 | 160 of 160 | five `FieldCore_State2Steps` dispatchers in no list; `Field_ObjectBestDirection` keeps the farthest step for an approach (the 09-22 reading corrected) |
| FE1 | `893036d` | 45 | 270,000 | 148 of 149; 1 equivalent, variant refused | 18 of the 45 have only Capcom's callers; the harness's `Zenny_Add` stand-in has its test backwards |
| FO | `78b2f2f` | 41 (44 - 6 + 3) | 82,000 | 103 of 105 (2 by a hang); 2 equivalent, variants refused | six cases of `MoveScript_Group*`; one `boss_harness.cpp` row `BH_THEIRS` -> `BH_OURS` (the coordinator accepted it: ownership only); DIV-0029 noted |
| FC1 | `0e51ec7` | 41 | 246,000 | 235 of 237; 2 equivalent, variants refused | effect-record kinds and `Config_DrawRowLabel`; DIV-0015 and DIV-0017 noted (their patches are read in place) |

Round tip `0e51ec7`, **6,891 ours** (`inject: 6891 ours, 0 left
original`); `d76f0d8` on top of it is round thirteen's plan, docs and
tools only. Every merge: the group's shadow exit 0, `'*'` exit 0,
`ledger_check` 0 errors. Wave two: **338 functions, 1,140 controls
planted, 1,129 refused**, 1,569,000 rounds, seven docs (`field_c1.md` ..
`field_s.md`). Every field group's inject is after
`ScenarioHarnessFh_Inject`, whose self-test copies functions of FS, FE2,
FC3, FE1 and FO from the image.

**What wave two found out:**

- **`tools/band_rows.py` misses starts reached only through a pointer in
  `.data`**: FC2's four and FC3's five dispatchers were printed as "0 not
  listed", and `0x578A40` (FO; found by the round-thirteen session, checked
  by the coordinator and by FO) as a function. A scan for dwords pointing
  into each band, and for a cut start whose only reference is a jump
  table's cell, is owed over all fourteen bands.
- **Seven field starts of the cut are jump-table cases**, not six:
  `0x578A40` is case 0 of `MoveScript_Group9`.
- **The plan's "the callers are ours" holds for the battle side only**:
  18 of FE1's 45 are called by Capcom's code alone.
- **FH's stand-ins met their first use** and every group re-listed some:
  masks too wide (FS about 25 callees), stack pointers logged by value,
  `Sprite_ObjectAt` answering outside its range, `Zenny_Add`'s test
  backwards.
- **A function taken stops a harness row that lists it as Capcom's**:
  FO's and FE2's `_THEIRS` rows. The next round's brief should say so.

## 9. The live checks at the round's tip (2026-09-29, 08:45..10:53)

From a launcher copy of `0e51ec7`'s build, as section 5's. The hashes ran
unfocused; the pictures with the display held on (`keep_display.py`).

| Check | Frame hash, original against ours (original against original identical in each) | Picture A/B |
|---|---|---|
| Attract, against `r9_orig` | frame 0 only, of 10,318 | - |
| `combat.txt` | frame 0 only, of 2,622 | 5 of 43 identical, 15 pixels at most - the baseline |
| `shop.txt` | frame 0 only, of 3,159 | 5 of 35 identical - the baseline; one frame 21,191 pixels, the save list's slot 0 (the recipe runner's swap) |
| `worldMapAndAreaTransition.txt` | frame 0 only, of 2,146 | 7 of 35 identical - the baseline; one frame 1,015 pixels, the compass needle (DIV-0044) |
| `dragonTransform.txt` | frame 0 only, of 4,330 | 5 of 73 identical, 248 pixels at most |
| `whelpBoss.txt` | frame 0 only, of 13,183 | 18 of 109 identical, 204 pixels at most |

The two recipes' first picture pass grabbed a browser window that covered
the game from about 10:01 (every original-side shot); the rows are the
second pass, the original side played again and compared with ours of the
first. The 248 pixels of the dragon route's `f04200` were not looked into.
The owner's saves were checked after the runs: slot 0 byte-identical to
its copy of the night before.

**Still fuzz-only**: what no route enters. The field side has no route of
its own (plan section 5); FC2's and FS's functions are entered by none of
the five.
