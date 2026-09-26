# Group S23: Cyclone, Typhoon, Quake, Simoon (MAGIC100..103)

## Paused (2026-09-25)

**Status:** IN PROGRESS - paused for the usage limit.

- **Done:** all 51 functions of MAGIC100..103 written in
  `src/game/magic_s23.cpp` (callees' raw addresses in
  `magic_s23_callees.h`), `symbols.toml` entries (51 `[[func]]` with `impl`,
  13 `[[data]]` tables), module at the end of `inject_all.cpp` and
  `CMakeLists.txt`.
- **Fuzzed:** `magic_s23_fuzz.cpp` through the harness; last run
  (`BOF3X_SHADOW=magic_s23`, before the debug instrumentation was removed):
  102,000 rounds over 51 functions, 0 mismatches, exit 0.
- **Harness changes (coordinator must know):** `magic_harness.h/.cpp` gain
  `Answer::kBool` (eax exactly 0 or 1) and `Answer::kThrough` (the callee is
  called for real on both sides - used for the GTE / GPU library and
  Math_Sin / _Cos / _Ratan2, whose outputs go through stack pointers); and
  the standard disturbance of "the target enemy's record" (cases 12/13) now
  runs only for a target of 3..10 - for a target of 2 it could write a random
  byte into the owner pointer 0x93B940, which case 11 then wrote through (an
  access violation reached in Quake_Heave's round ~7,273).
- **Controls planted:** none yet.
- **Next step:** rebuild (debug code was removed after the last run; the
  fuzz file's raw-callee names were just changed to "0x446770" etc.), rerun
  `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s23` and `'*'`; then plant one
  negative control per behaviour (scratchpad script), write the rest of this
  doc (per-function table, fuzz and seeds, controls table, latent defects:
  Quake_Heave's divide by zero at a facing byte >= 4, Quake_End turning an
  original 0x28 code into 0, FxFunnel_Wait's offset overwritten, unbounded
  tables FxSpiral_Turns / _Tilts / SimoonDust_Offsets / Quake_FacingOffsets,
  BattleTask_Create's 0xFF unchecked), add the README index row, append the
  51 lines to the main checkout's `analysis/calltrace/entries_logic.txt`.
