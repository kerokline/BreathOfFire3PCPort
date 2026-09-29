# Round eleven's cleanup: the debts the boss round left

**Status:** IN PROGRESS (2026-09-28) - the list for a session of its own,
after round eleven's PR (#30, `c4b0d32`). Every item is owed by
[`takeover-queue-round11.md`](takeover-queue-round11.md) section 7 (sections
4 and 5 name the evidence); nothing here changes game behaviour, so no
DIVERGENCE entry is expected unless an item says so. Cross off items here
as they land; the round doc stays as the record of what was found.

**Landed 2026-09-28, from a cloud session without the game files** (branch
`claude/round-10-cleanup-handoff-qtwcrk`, restarted from `main` at
`c4b0d32`; verified by the i686 build (llvm-mingw 20260616),
`ledger_check.py` 0 errors, `gen_symbols.py`, `tables.py check`): item 1's
six fold-backs (section 1, each marked with whether it can move a fuzz
result), item 2's rebinding in full (195 constants in 28 files, the
round-ten form), item 3 in full (D162..D173), item 4's `--no-write`. **Left**:
the game-side run of every fold - `BOF3X_SHADOW='*'` headless at the tip is
the first thing with the game on hand, and section 1 says which groups' counts
may move and why - then items 5 and 6 (the owner's set-ups, the recipe saves,
the local branches and worktrees).

Round eleven took 531 functions in eleven groups over two waves (5,706 ->
6,237 ours), every group fuzz-only through `boss_harness`. Stage B ran while
the harness was frozen (no agent builds against a moving harness), so every
group carried its own copy of what the harness lacked; the price is
section 1.

## 1. Harness fold-backs

The six shapes of the round doc's section 5, folded into
`src/game/boss_harness.h` / `.cpp` on 2026-09-28. The groups' fuzz files now
name the harness's forms where they had copies; the copies were byte-identical
where a fold replaces them, so the draws (`Next()`, `Noise()`) come in the
same order and the fuzz is the same run. **What can move a count** is said
per item; a `'*'` run at the tip settles each.

1. **The dispatcher shape (5.1)** - ours forwards the caller's stack word to
   the table entry and answers its eax. That is a property of each group's
   ours, not of the harness; the harness's part is the standard
   `Port_DroppedCall` listing at one argument, which is what refuses a
   dropped word when it sits in a `+1` table (BSJ's D2, BSC's C8 / C74 were
   refused on it). **Written into `Clone::states`' comment as the
   contract.** Section 7's "Port_DroppedCall listed with 0 arguments" is
   the override BSA and BSE need because their ours do not forward (BSA's
   `boss_sa.cpp` dispatchers call the entry with no word, BSE's are void);
   BSF and BSI carry the same override with an ours that does forward -
   dropping theirs would add the compare (their doc marks the control
   "equivalent"), and wants a run. Left as they are; a shared
   `DispatchEntry` helper in ours would be a change to game code and is
   not this session's.
2. **`OtherStates` (5.2)** - the dispatched byte's neighbours drawn inside
   their tables. Two overloads exported from the harness (`+1..+4` but
   `drawn` below one bound; `+1..+3` but `at`, a bound each); BSD, BSF, BSG,
   BSH (the first) and BSC, BSJ (the second) use them by `using`. The same
   draws: **no count moves.** BSA's `SeedStates` (by clone index), BSI's
   (every shape, `g_small`), BSE's and BH's (`Often()`, ad hoc) are their
   own shapes and stay.
3. **The louder stand-ins (5.3)** - six effects exported, five of them the
   standard set's now:
   - `TurnOrderEffect` for `Battle_RemoveFromTurnOrder`: the five identical
     copies (BSE, BSF, BSG, BSJ's `RemoveEffect`, BSH's `PartyEffect` - BSH
     lists it for `Sprite_PoseFromSet` too) replaced; every caller listed
     it already, so **no count moves**.
   - `EndWinEffect` for `0x446DE0`, BSH's form: `Note` the chapter step,
     then move it (BSC's form moved the move counter instead and stays
     BSC's own listing, `MoveCounterEffect`). **Louder for BH, BSA, BSB,
     BSD, BSE, BSF, BSG, BSI and BSJ**, which had the silent standard: one
     more log entry per call, the step moved after it, and `Noise()`'s salt
     advanced - their counts move; a mismatch there would be a real
     re-read of the step across the call, not a regression.
   - `ScriptBitsEffect` for `Msg_OpenScript` (BSC's `BitsEffect`,
     `0x904AAD` half the time): **louder for BSA and BSE**, the other
     callers.
   - `MoveCounterEffect` for `Scenario_CallA` and `BannerCharEffect` for
     `Battle_OpenMsgWindow` (BSC's `SceneEffect`, `CharEffect`): their cells
     `0x903848` and `0x66972D` are group regions, so the effects draw
     nothing outside one (no `Noise()` call either) - **only BSC, which lists
     the regions, is affected, and identically**.
   - `CreateMayFail` for `BattleTask_Create` answering 0xFF a third of the
     time (BSG's `CreateEffect`): **opt-in**, BSG's listing names it; seven
     originals index by the answer untested (D163) and would write past the
     pool on both sides. BSH's `SlotEffect` (slots 45..47) is a different
     check and stays.
   The rule - an effect that overwrites a cell the caller may have stored
   first must `Note()` the old value - is in `Callee`'s comment.
4. **The disturbance's case 11 (5.4)** honours `phase_span` as cases 9 and
   10 do: a state byte `+1..+4` of the current enemy's record stays below
   it. No extra draw, so groups with `phase_span` 0 (all but BSD's K26 and
   BSJ's f4 / f5) are unchanged; **BSD K26's `+2..+4` and BSJ f4 / f5's
   enemy bytes now come out below the span** - their counts may move, and
   BSD's `SettleK26` (which put `+1` back below 12 after every disturbance)
   is now redundant for the current enemy and still covers the other seven.
5. **A state draw for `kTask` (5.5)**: `Clone::states` is honoured for a
   `kTask` too, drawn **after** the group's seed so the task's own byte wins
   when its owner is `Sprite_Current` itself (BSJ's first fault). Every
   existing `kTask` row has `states` 0, so **nothing moves**; the groups'
   own task seeds (BSF's FB8 / F2, BSG's F6, BSH's F3, BSI's F7, BSJ's
   `SeedTask`) stay.
6. **`DataTable` order (5.6)**: the rule is written at `DataTable` (the
   first-listed table's `nargs` stands; a `Callee`'s wins over a table's),
   and `RegisterHandler` logs each conflict once (`handler 0x... in tables
   with nargs 1 and 0: the first stands`) - log only, so **nothing moves**;
   it will name BSA's k39, BSF's K33 and BSI's k58, which rely on the order.
   Not made a `Fatal`: those three runs want the order as it is.

## 2. The rebinding pass

**Done (2026-09-28)**, the round-ten form ([`round-10-cleanup.md`](round-10-cleanup.md)
section 1): every raw constant whose target has a name in `symbols.toml`
reads `bof3::addr::<Name>` inside the same `constexpr` / table entry / call,
the values unchanged so the fuzz keys stand. 195 constants in 28 files:

- `0x437CC0` (`BareRet`) in the `kRetOnly` / `kBareRet` / `kNop` /
  `kNothing` constants of `battle_draw`, `battle_misc`, `battle_win_states`,
  `d3d_draw`, `d3d_list`, `field_misc`, `glyph_draw`, `msgbox`,
  `scena_sc6`, `sprt_draw`, `window_kinds`' `_callees.h`, `magic_s14.cpp`,
  `boss_sa_callees.h`, `boss_sc.cpp`'s two `StoreHooks`, and both stack
  tables of `battle_fx_tasks.cpp` (with `Boss26Fx_Dispatch` `0x43C740`,
  slot 8); the same headers' other named targets while there (the round-seven
  skill helpers in `battle_draw_callees.h`, the banner kinds, the battle
  windows' state tables, `msgbox`'s and `window_kinds`' callees).
- `0x4357D0` (`BattleBossFx_Dispatch`, named, not taken) in `battle_flow.cpp`'s
  task table - as the generated pointer macro, since a named-not-ours
  function has no `bof3::addr` constant - with the other three tables'
  entries and `battle_flow_callees.h`'s three cross-group callees;
  `kEventHook`'s comment there corrected (the hook is stored by the boss
  set-ups, not "set by 0x437CC0").
- `0x43EC10` (`BattleFx_ScriptUntilDone`) in `magic_s09`, `magic_s15`,
  `magic_s31`.
- The boss groups' own: `boss_sb.cpp`, `boss_sc.cpp`, `boss_sd.cpp`,
  `boss_sj.cpp`'s hook and table stores (`Setup`, `StoreHooks`,
  `StoreKindTables`, `PointEnemy`, `RestoreOrMark`, `Enter`, `kSteps`),
  `boss_sa` / `boss_sd` / `boss_se` / `boss_sh` / `boss_si_callees.h`'s
  constants (`0x43B730` `Boss21_End` in BSE, `0x43E790`
  `BossHook_ExitTransition4` in BSH among them); `boss_h_fuzz.cpp`'s `via`
  dispatchers (`kKindDispatch`, `BossClaw_Dispatch`, `BossGary_ActDispatch`,
  `BossClaw_Hook`, `BossTorast_Hook`) - the clones' own addresses, the
  keys, stay raw.

Left raw, on purpose: the fuzz files' `CallSite` / `Imm` / `kCallees` keys;
`battle_fx_tasks.cpp`'s entries that are not ours; the engine callees no
unit owns (`0x446700`, `0x446DE0`, `0x446E00`, `0x446E20`, `0x437450`,
`0x4376A0`, `0x4376F0`, `0x454A80`, `0x455290`, `0x441090`, and `0x452B60`
the trail task) - **a small engine group for a later round**, as
[`round-10-cleanup.md`](round-10-cleanup.md) section 3 proposes for the
scenario and area rounds' leftovers. Kind 40's byte tables in BSH
(`kMikbaAnims`, `kMikbaSounds`) are `.data`, read in place, and are not
functions: nothing to rebind.

## 3. The defects' numbering

**Done (2026-09-28):** D162..D173 in [`known-defects.md`](known-defects.md).
The common classes collapsed to one entry each as round nine's were:
D162 the dispatchers, hook tables and stack tables indexing unchecked
(every group; ours aborts), D163 `BattleTask_Create`'s 0xFF untested in
seven places (ours aborts), D164 the exit hooks and spawn writers through
`BossActor_Find`'s null (ours aborts), D165 the reads by an unchecked index
or actor kept as read (Torast's colours, F6's facing pairs and `actor - 3`,
the task spawns copying by the acting actor or naming enemy 0, the ability
records by the whole word, `Boss27_Event`'s cursor, the `+0xF0` enemy-data
reads, `Boss34_Event`'s party byte, the `0x904B40` reads, fight 16's enemy
2 by address). The singular ones their own: **D166 set-up 25's HP / AP
bytes - the fix candidate for the owner**; **D167 set-ups 8..10 posing every
actor from enemy 0's words - the owner decides whether intended**; D168
Torch's `+0xFC` in never-written `.data` (a PSX comparison owed); D169
`BossTorast_DrawRing` twice; D170 Myria's ten-byte pose table by a word
(ours aborts at 10); D171 kind 30 copying record 0xFF; D172 set-up 25's
unbounded party-list copy; D173 the small slips read once (kind 3's shift,
Amalgam's dropped slot and melt bound, Nina's and Sunder's hooks, the kept
words of kinds 21..23 / 26, `Boss26Fx_DrawCount`, the byte-wrapped poses,
`Boss38_Setup`'s division).

## 4. The tools

**Done (2026-09-28):** `tools/boss_rows.py --no-write` - a read-only run that
sends both TSVs to `os.devnull` and says so, as `area_rows.py`'s does
([`boss-rows.md`](boss-rows.md) section 1); the canonical cut stays
`analysis/boss_funcs_0928_4554.tsv`. The tool's table counts (round doc
section 4, "70 code entries" is 12) are a reading of the exe and want it to
test a fix: left, with round ten's tool fixes.

## 5. The live side

Owed at the tip, the owner's:

- `BOF3X_SHADOW='*'` headless (exit 0): section 1 names the groups whose
  counts may move (BH and every group but BSC and BSH for the louder
  `0x446DE0`; BSA and BSE for `Msg_OpenScript`; BSD K26 and BSJ f4 / f5
  for the disturbance) and why. A mismatch under a louder stand-in is a
  finding about a re-read across the call, to write into the group's doc.
- `ledger_check.py`, the frame hash against `r9_orig` (frame 0 only, as at
  `639ea9c`), the combat A/B at its baseline (round doc section 8).
- The owner's open set-ups (round doc section 4's last item) and the recipe
  saves per fight; then the live check per fight.

## 6. Housekeeping

The eleven `phase-3/round11-*` branches are gone from `origin` (none listed
2026-09-28 evening); the agents' worktrees under `.claude/worktrees/` and the
controls scripts in the session-`08306a9f` scratchpad (`<group>/`, a Temp
folder) are on the owner's machine - copy the scripts somewhere durable if
they are to outlive a cleanup, as round nine's and ten's were told.
