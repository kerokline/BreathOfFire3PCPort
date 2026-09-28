# Round eleven: the boss round, staged and merged

**Status:** MEASURED (2026-09-28 evening) - the takeover is complete: 531
functions in eleven groups over two waves, 5,706 -> 6,237 ours, every group
0 mismatches and every control refused or an equivalent with a refused near
variant; fuzz-only, the live check per fight the owner's (section 6). The
plan and the cut are [`takeover-queue-bosses.md`](takeover-queue-bosses.md)
(IDEAS I26); the tool [`boss-rows.md`](boss-rows.md); the harness
[`boss_harness.md`](boss_harness.md). The round's debts are section 7.

## 1. Set-up (2026-09-28 afternoon)

Branch `phase-3/capture-round-eleven` from `main` at `6b70e71` (PR #29, the
round-ten cleanup). Before the first agent, the cleanup's owed game-side
checks ran at that tip and passed ([`round-10-cleanup.md`](round-10-cleanup.md)
status header). `tools/boss_rows.py --groups` re-run at `6b70e71` gave the
plan's cut function for function (`analysis/boss_funcs.tsv`; the copy
`boss_funcs_0928_4554.tsv` is the canonical one - see section 4's trap).

The routine was round ten's: a brief per group (`analysis/round11_wave1_brief.md`
the template; the filled ones in the session-`08306a9f` scratchpad as
`brief_<group>.md`), one Opus agent per group in a worktree reset onto the
wave's tip, headless self-tests only, `merge_group11.sh <group> <scratch>`
(`MOD=boss_<group>`; merges `--no-ff` in the main checkout, `keepboth.py`
and `one_grow.py` on the four both-appended files, then builds and runs
the group's shadow, `BOF3X_SHADOW='*'` and `ledger_check.py` in the
detached `verify/` worktree), one merge at a time.

**One change from the plan:** BH ran alone in stage A. The plan had BH and
BSA both building `boss_harness.*`; two agents editing the same new files
would only have conflicted, so BH built the harness, its doc and the
helpers (56 minutes), and BSA..BSE started from its merge.

## 2. Wave one (stage A 14:50..15:20, stage B 15:20..17:00)

| Group | Merge | Functions | Controls | Note |
|---|---|--:|---|---|
| BH | `79dafce` | 26 (20 helpers + the 6 spawn helpers as `boss_spawn`) | 86 of 88 refused; 2 equivalent (H6, H17) with variants refused | the harness; `Boss_SetupTable`, `BossKind_Table`, `BattleBossFx_Dispatch`, the three hooks named; `EnemyOp_*B..F` re-attributed to kinds 6, 7, 1, 2, 46; `0x437CC0` `BareRet`, `0x43C9F0` `BareRetZero` |
| BSD | `e4b9673` | 52 | 114 of 115; 1 equivalent (D74b, Garr's all-`BareRet` hook table) | first `kSetup` user; set-ups 19 and 20 are copies of 18's hooks |
| BSB | `8bda604` | 52 | 124 of 124 | 8 first-run plants crashed both passes (another kind's table swapped in) and were replaced |
| BSA | `68999ff` | 49 | 139 of 139 | first run: 13 refused only by a Fatal until the other state bytes were seeded |
| BSC | `c34997c` | 53 | 145 of 145 | dispatchers forward the caller's stack word (C8, C74 prove it) |
| BSE | `5d295d7` | 52 | 141 of 145; 4 equivalent with variants refused | set-up 25's byte-sized HP/AP save described |

Wave one tip `5d295d7`, **5,990 ours**. Every merge: the group's shadow
exit 0, `'*'` exit 0, `ledger_check` 0 errors.

## 3. Wave two (staged 17:05 from `5d295d7`, merged 17:40..19:00)

The briefs carried wave one's six lessons (section 5) and the corrected
addresses (section 4).

| Group | Merge | Functions | Controls | Note |
|---|---|--:|---|---|
| BSI | `68b1c16` | 50 | 135 of 138; 3 equivalent with variants refused | first `kTask` user (Arwan's task) |
| BSH | `027eaf9` | 46 | 126 of 127; H115 refused only by its own Fatal, H115b by a count | `0x43EC10` `BattleFx_ScriptUntilDone`, held raw by three spell groups |
| BSJ | `d393465` | 44 | 127 of 128; 1 equivalent (D9) with variant refused | `0x44103A` is a branch inside `0x440EF0`, not a function |
| BSG | `8c24ff9` | 53 | 145 of 148; G73 refused only by a crash (G73b by a count), 2 equivalent | the trail's fade step is entry 1 of the unowned engine task `0x452B60` |
| BSF | `639ea9c` | 54 | 197 of 197 | kind 62 (Myria, 27 functions) and its slot-5 spawn |

Round tip `639ea9c`, **6,237 ours** (`inject: 6237 ours, 0 left original`);
65 commits, 68 files, 27,137 lines since `6b70e71`. The entry list
consolidated after the round: 7,485 entries, the same 34 owned starts
without a line as after round nine (the wall-clock exclusions and the
audited 25).

Totals: **531 functions** (the plan's 525 plus the six spawn helpers),
**1,494 controls**, 12 docs, about 250 `[[data]]` tables named.

## 4. What the round found out about the plan

- **`0x904B64` is not a per-frame hook.** `0x431464` lies inside
  `BattleEnd_AwaitMemberTasks` and calls it once as the battle's way out is
  picked. The hooks are `BattleHook_End` (`0x904B64`), `BattleHook_Exit`
  (`0x904B68`), `BattleHook_Event` (`0x904B6C`) (BH).
- **The enemy objects are at `0x93B960`** (stride `0x128`); the plan's
  `0x93B9E0` is their working-record view, object + `0x80` (BH).
- **The tool's table counts are too long** ("70 code entries" is 12, 6 or
  3): every `+1` table has 12 entries, every hook table 3; each group took
  the count from the code (all).
- **The tool's cut is exact otherwise**: no group found a start inside
  another function's extent or missing from its list, except BSJ's
  `0x44103A` (a branch of `0x440EF0`, whose clone row grew to four calls).
- **`tools/boss_rows.py --analysis <main checkout>` overwrites
  `analysis/boss_funcs.tsv` and `boss_rows.tsv`** from the current
  `symbols.toml`, which recuts the group column once functions are ours
  (BSI, BSF, BSG saw it). The canonical cut is `boss_funcs_0928_4554.tsv`
  (byte-identical to the tool's output at `4a15d61` and at `6b70e71`);
  restored twice during wave two. Give the tool a scratch `--analysis`
  copy, or teach it `--no-write` as `area_rows.py` has.
- **Set-ups whose fight the code does not settle**: 2 and 3 (Nue's area
  22 or 23, BSA), 46 (Sample 8's by its row, BSC), 27 (BSF), 40 / 53 and
  45 / 49 (BSI, by the end-hook shape and the chapter-15 id range), 41 /
  43 / 47 (BSH, Samples 3, 5, 9 by "id n is kind n+7"), 19 / 20 (BSD, rows 6
  and 5 of the area-79 fight by the shared image). None of these units'
  code reads `0x904AAA`; the owner's play settles them.

## 5. What the groups learned (carried into wave two's briefs)

1. A kind's dispatcher takes the caller's stack word, hands it to the
   table entry and returns the entry's eax: the original `jmp` leaves both
   in place, and `Port_DroppedCall` sits in the `+1` tables and logs that
   word (BSB, BSC first; every group after).
2. Seed the dispatchers' other state bytes inside their tables every
   round, or hook plants run past their table and refuse only by a Fatal
   (BSA's 13, BSI's first 18).
3. A stand-in made louder by overwriting a cell wipes a store the caller
   made before the call (BSH's `0x446DE0`: two controls unrefused until it
   logged the chapter step before moving it).
4. The standard disturbance's case 11 writes the state bytes `+1..+4`
   without honouring `phase_span` (a real Fatal in BSD's kind 26; `settle`
   around it).
5. `kTask` has no state draw: `Fix` puts a slot's `+1` / `+2` below 3, so a
   four-step or seven-step task, or a 2-entry table, wants the group's
   seed to draw them (BSI, BSJ, BSG); and when the owner is
   `Sprite_Current` itself, the task's own state bytes must be written
   after the owner's (BSJ's first fault).
6. `Port_DroppedCall` listed with 0 arguments; `DataTable` order matters
   when one address (`BareRet`) sits in tables with different `nargs`
   (BSA).

## 6. The live check

None yet: every group is fuzz-only, as the plan said. The check per fight
is a recipe save before it, played under original and ours with the frame
hash compared; the owner records them. What the fuzz cannot see is listed
in the plan's section 5 and each group doc's "what nothing reached".

After the last merge the attract-mode frame hash and the combat route's
A/B were re-run at the tip (section 8, filled in when they finish): the
boss band is off the attract path, and the spawn helpers are reached only
by the event-battle set-ups, so both are expected at their baselines.

## 7. Debts (the round's cleanup)

1. **Harness fold-backs** - every stage-B and wave-two group worked around
   these in its own fuzz file; fold them into `boss_harness.*` now that no
   agent builds against it, and re-run `'*'`: the dispatcher shape
   forwarding the stack word (section 5.1); the disturbance's case 11
   honouring `phase_span` (5.4); a state draw for `kTask` slots (5.5);
   `Port_DroppedCall` with 0 arguments and a documented `DataTable` order
   (5.6); the louder stand-ins for `Msg_OpenScript`, `Scenario_CallA`,
   `Battle_OpenMsgWindow`, `0x446DE0` (BSC, BSH's form),
   `Battle_RemoveFromTurnOrder`, `BattleTask_Create` answering 0xFF (BSE,
   BSG).
2. **The rebinding pass** (the round-ten form, values unchanged): inbound
   raw references to now-ours addresses - `0x437CC0` (`BareRet`) in about
   20 of our files and `0x4357D0` in `battle_flow` (`docs/boss_h.md`
   section 9); `0x43EC10` in `magic_s09`, `magic_s15`, `magic_s31`;
   `0x43C740` in `battle_fx_tasks.cpp` and its fuzz; the `boss_h_fuzz.cpp`
   `via` addresses `0x43A660`, `0x43A740` (BSD's), `0x438E50`, `0x4390F0`,
   `0x439160`, `0x4391D0`, `0x4390E0` (BSB's); `battle_flow_callees.h`'s
   comment on `0x438450`; literals stored across groups (`0x43B730` in
   BSE, `0x43E790` in BSH, kind 40's tables in BSH). Outbound raw calls
   are all to code nobody owns: `0x446700`, `0x446DE0`, `0x446E00`,
   `0x446E20`, `0x437450`, `0x4376A0`, `0x4376F0`, `0x454A80`, `0x455290`,
   `0x441090` - a small engine group (with `0x452B60`, the trail task) for
   a later round.
3. **Number the defects** in `known-defects.md` from the group docs'
   "latent defects": the common classes (dispatchers and hook tables
   indexing unchecked; `BattleTask_Create`'s 0xFF answer untested in
   seven places; exit hooks writing through `BossActor_Find`'s null; the
   `0x904B40` reads) collapsed to one entry each as round nine's were; the
   singular ones per group (set-up 25's HP/AP bytes, BSE - **a fix
   candidate for the owner**; Torch's `+0xFC` in never-written `.data`;
   Myria's ten-byte pose tables by a word; `SpawnFx` naming enemy 0;
   set-ups 8..10 posing every actor from enemy 0's words - **the owner
   decides whether that is intended**; `BossTorast_DrawRing` drawing
   twice; kind 30 copying record 0xFF; set-up 25's unbounded party-list
   copy; F6's facing table shared with engine code).
4. **`tools/boss_rows.py --no-write`** (section 4).
5. **The owner's open set-ups** (section 4's last item) and the recipe
   saves per fight.
6. **Housekeeping**: the eleven `phase-3/round11-*` branches and the
   agents' worktrees under `.claude/worktrees/` are merged and can go;
   the controls scripts live in the session-`08306a9f` scratchpad
   (`<group>/`), a Temp folder.

## 8. The tip's live checks

_Filled in by the coordinator after the runs._
