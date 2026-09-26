# Spell group S20: MAGIC087, MAGIC088, MAGIC092

**Status:** IN PROGRESS (2026-09-25) - paused for the usage limit; see below.

## Paused (2026-09-25)

- **Done:** all 51 functions of the three units are ours
  (`src/game/magic_s20.cpp`), with `symbols.toml` entries (evidence, `impl`)
  and 16 `[[data]]` entries (the two mote pools, the thirteen .data
  dispatch tables, MAGIC088's band phases); module wired at the end of
  `CMakeLists.txt` and `inject_all.cpp`.
- **Fuzzed:** all 51, through the harness (`src/game/magic_s20_fuzz.cpp`,
  shadow `magic_s20`): 102,000 rounds, 13.85 M stand-in calls,
  **0 mismatches**, 59,120 bytes of state in 19 regions, every stand-in and
  handler reached (coverage lines in `build/bof3x.log`). `BOF3X_SHADOW='*'`:
  exit 0 (84 s; Steal's figures unchanged).
- **Harness changes** (`magic_harness.cpp/.h`, for the coordinator to merge
  with care): log 1,024 -> 8,192 entries, copied and compared by the used
  prefix only; `LogPointee` (log what a callee's pointer argument points
  at - the stack vectors of `Gte_RotTrans` / `Gte_RotMatrix`, the vertices
  of `Gte_RotTransPers3/4`); `LogReturn` (compare a clone's al - the two
  pool allocs and `Magic088_Variant`); the coverage line continues over
  several lines; the enemy-record disturbance (cases 12/13) now only for a
  target 3..10 - a target of 2 made it write the owner pointer 0x93B940
  itself (a crash here), and a side bit 0x40 indexes past the image.
- **Controls:** none run yet. 118 are written, one per behaviour, in
  `C:/Users/kerok/AppData/Local/Temp/claude/C--Users-kerok-Documents-GitHub-BreathOfFire3PCPort/c6020f3e-435b-4b37-a18c-94d1c71f583a/scratchpad/s20/controls.py`
  (apply one, `cmake --build build`, self-test, restore; results appended to
  `controls.tsv` beside it). X3 (Sprite_SetTint's fifth argument) is
  expected not to be refused: the stand-ins log four arguments.
- **Next step:** `python <scratch>/s20/controls.py` (all, about an hour,
  run in the background), then write this doc properly (what each function
  does, the fuzz and seeds, the controls table, what nothing reached, the
  latent defects), add its row to `docs/README.md`, append the 51 lines to
  the main checkout's `analysis/calltrace/entries_logic.txt`, commit.

Latent defects noted so far (Capcom's, kept; for the coordinator to
number): the three stack tables and thirteen .data tables are unchecked;
`Magic087_ChildSpawn` and `Magic087_ChildRing` do not test the pool alloc's
0xFF (a full pool writes past it), where `Magic092_ChildSpawn` does;
`BattleTask_Create`'s index untested in the three starts and in
`Magic088_Start` / `_Apply`; `Magic088_Darken` / `_Lighten` index the 32
tint records by `Sprite_SetTint`'s answer unchecked (its 0xFF, no record
free, writes past them).
