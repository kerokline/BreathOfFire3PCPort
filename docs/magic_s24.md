# Spell group S24: MAGIC104, MAGIC105, MAGIC106

**Status:** IN PROGRESS (2026-09-25) - paused for the usage limit; see below.

## Paused (2026-09-25)

- **Done:** all 47 functions written in `src/game/magic_s24.cpp` (MAGIC104
  19, MAGIC105 16, MAGIC106 12; 10,068 bytes, the brief's count), injected by
  `MagicS24_Inject` (end of `inject_all.cpp`, `CMakeLists.txt`);
  `symbols.toml` has 47 `[[func]]` (impl) and 16 `[[data]]` entries.
- **Fuzzed:** `magic_s24_fuzz.cpp`, `BOF3X_SHADOW=magic_s24`: 94,000 rounds
  over 47 functions, **0 mismatches**, exit 0 (last run before the pause).
  Not yet run: `BOF3X_SHADOW='*'`.
- **Harness changes made (for the coordinator, additive):** `kLog` 1024 ->
  4096 (MAGIC104's funnel makes ~2,600 calls); `Callee::deref[4]` (hash the
  pointee of a stack-built GTE input); `Answer::kPhase` (a group's own
  function called directly, logged like a handler, `masks[0]` an optional
  watched cell); `kByte` with `lo > hi` wraps through 0xFF; Disturb case
  12/13 no longer writes a target >10's "enemy" (past the image) nor into
  0x93B8C0..0x93B95F (target 2's "enemy" tore the owner pointer, then case
  11 dereferenced it: a crash); case 9/10 writes phase bytes +1/+2 as 0/1
  (0x4D12A0 dispatches after two calls).
- **Temporary debug to remove before the final commit:** every line tagged
  `S24DBG-TEMP` in `src/game/magic_harness.cpp` (an include and two logs)
  and in `src/game/magic_s24_fuzz.cpp` (windows.h, log.h, the vectored
  handler `S24Veh` and its registration).
- **Controls:** none planted yet.
- **Next step:** remove the S24DBG-TEMP lines; rebuild; rerun
  `BOF3X_SHADOW=magic_s24` and `'*'`; plant controls one at a time (a
  script under the scratchpad `s24/`), one or two per function; write the
  rest of this doc (functions, fuzz and seeds, controls table, what nothing
  reached, latent defects: unchecked stack/.data dispatch indices,
  Fx106_Start's unchecked 0xFF allocation writing past the spark pool,
  Fx105_OrbStart / DrawOrb unbounded +4 table reads, Fx105_ChildPhases
  entry 1 = data), the `docs/README.md` row, and the 47 lines for the main
  checkout's `analysis/calltrace/entries_logic.txt`.
