# Group S25: four spell overlays (MAGIC107..110)

## Paused (2026-09-25)

Paused for the account's usage limit; resume from this worktree, branch
`phase-3/round9-s25`.

- **Done:** all 56 functions of MAGIC107..110 (`0x4D2D80..0x4D610B`) written
  in `src/game/magic_s25.cpp` (read to the last instruction; names
  `SpellSleep_*`, `SpellConfuse_*`, `SpellDepress_*`, `SpellRagnarok_*` -
  labels read one id down, hypotheses), `symbols.toml` entries (56 `[[func]]`
  with `impl`, 11 `[[data]]`: the six .data phase tables, the corners, the
  spark offsets, the three vortex rings), CMakeLists and `inject_all.cpp`
  (at the end, after `MagicSteal_Inject`).
- **Fuzzed:** `src/game/magic_s25_fuzz.cpp`; `BOF3X_SHADOW=magic_s25`
  self-test last run: exit 0, 112,000 rounds over 56 functions, **0
  mismatches**, 22,113 bytes of state (20 regions). `BOF3X_SHADOW='*'` not
  yet run.
- **Harness changes** (`magic_harness.h/.cpp`, additive, to be reported to
  the coordinator): stubs take ten argument slots; `SetCallHook` (a group
  hook before / after each recorder's disturbance), `LogValue`, `LogBytes`;
  `Answer::kBool`; the log 32,768 entries, only the used entries
  captured / compared; the harness's Disturb case 12/13 guarded for side
  targets (0x40 / 0x80 would have written outside the enemy records).
- **Not yet:** the coverage-line split in `magic_harness.cpp` (an edit was
  drafted, not applied: the line is truncated at 900 characters); no
  negative control planted yet; the rest of this doc; the
  `entries_logic.txt` lines (main checkout); the `docs/README.md` index row.
- **Next step:** rebuild, run `BOF3X_SHADOW=magic_s25` then `'*'`; plant
  controls one at a time (a script under the scratchpad's `s25/`), one or
  more per behaviour of each function; write sections 1..7 of this doc;
  append the 56 `entries_logic.txt` lines; add the README row; commit.
