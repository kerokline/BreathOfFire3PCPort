# Group S21: MAGIC093, 094, 095 (Inferno, Frost, Iceblast read one id down)

**Status:** IN PROGRESS (2026-09-25). Paused before the negative controls
were done.

## Paused (2026-09-25)

- **Done:** all 48 functions of MAGIC093 (`0x4C6300..0x4C742A`, row 103),
  MAGIC094 (`0x4C7430..0x4C7CCE`, row 20) and MAGIC095
  (`0x4C7CD0..0x4C8D34`, row 22) are ours in `src/game/magic_s21.cpp`, each
  read to its last instruction. `symbols.toml` has 48 `[[func]]` entries with
  `impl`, plus 9 `[[data]]` entries: seven `.data` handler tables
  (`0x65B604..0x65B64C`), `FlamePool` `0x6948D8` and `Iceblast_Jitter`
  `0x695958`. The module is at the end of `inject_all.cpp` and CMakeLists.
- **Fuzzed:** `BOF3X_SHADOW=magic_s21` (`src/game/magic_s21_fuzz.cpp`)
  ran 96,000 rounds (2,000 per function) with 0 mismatches and exit 0. Every
  handler in every table is covered. `magic_steal` still gives its documented
  counts after the harness changes.
- **Harness changes (additive, the coordinator must review them):**
  - a `Callee::effect` hook, so a callee's recorder can log what it reads
    through pointer arguments and fill what it writes (the GTE and GPU
    calls), with recorders now taking up to ten arguments;
  - `LogValue`, `LogBytes`, `FillBytes` and `Salted` added for that hook;
  - `Clone::answer_bytes`, which compares a function's answer
    (`FlamePool_Alloc`);
  - `Group::phase_span`;
  - the target-enemy disturbance now only writes when the target is 3..10.
    Before, a party target hit `0x93B940` (the owner cell) and crashed the
    fuzz;
  - the coverage line is split across several log lines.
- **Controls:** the script is written
  (`scratchpad/s21/controls.py`, about 130 planted bugs). Only T1 has run:
  it swaps `Inferno_Task`'s table entries and was refused in 2,000 rounds.
- **Next step:** run the controls script from the worktree
  (`python <scratchpad>/s21/controls.py`). It writes `controls.json` and
  restores the source on exit. Then run `BOF3X_SHADOW='*'`, write the rest of
  this doc (what each function does, the fuzz, the controls table, latent
  defects), add its row to `docs/README.md`, and append 48 lines to the main
  checkout's `analysis/calltrace/entries_logic.txt`.

Latent defects noted so far, for the coordinator to number:

- `Inferno_TargetCentre` (`0x4C7320`) divides by the count of live targets,
  so it faults (#DE) when all of them are out. Ours aborts with a Fatal.
- `Inferno_Start` uses `FlamePool_Alloc`'s answer without checking it. With
  the pool full, the answer is 0xFF and the write lands past the pool.
- The three stack tables are not bounds-checked; ours aborts past the end.
- `IceShard_Draw` indexes the jitter bytes by `+0xB >> 1` without a bound.
