# Group S17: Purify, Raise Dead / Resurrect, Leech Power

## Paused (2026-09-25)

Paused for the account's usage limit; resume from this worktree, branch
`phase-3/round9-s17`.

- **Done:** all 48 functions of MAGIC075 / 077 / 078 written
  (`src/game/magic_s17.cpp`), their `symbols.toml` entries (48 `[[func]]`
  with `impl`, 11 `[[data]]`), the fuzz group (`src/game/magic_s17_fuzz.cpp`),
  CMake and `inject_all.cpp` wiring. The build is clean.
- **Fuzzed:** `BOF3X_SHADOW=magic_s17`: 96,000 rounds over 48 functions,
  0 mismatches, exit 0 (5 s); `BOF3X_SHADOW='*'`: exit 0 (79 s), Steal's
  counts unchanged.
- **Harness changes** (shared file, the coordinator must know): `Callee::custom`
  (a group's own stand-in) with `Record` / `Stir` / `Noise` / `HashBytes`;
  `Clone::result_mask` (log a function's result) and `Clone::invoke` (call a
  function with arguments); `Group::settle` (after every disturbance); the
  log 1,024 -> 4,096 entries (compared only to its count); 32 -> 64 call
  sites a clone; the coverage line split at 900 characters; and a latent
  harness defect fixed - the target-enemy disturbance with a target of 2
  wrote over the owner pointer 0x93B940 (it crashed LeechOrb_DrawRings'
  fuzz), 0 and 1 below every region: now only for a target of 3 or more.
  `magic_steal_fuzz.cpp` got the new trailing initialisers.
- **Controls planted:** none yet.
- **Next step:** commit is WIP; plant the negative controls one at a time
  (a script under the scratchpad's `s17/`), then write the rest of this doc
  (what each function does, the fuzz and its seeds, the controls table, what
  nothing reached, latent defects), the `docs/README.md` index row, and the
  main checkout's `analysis/calltrace/entries_logic.txt` lines.
