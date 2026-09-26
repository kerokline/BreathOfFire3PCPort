# Spell group S19: MAGIC083 (Shield, row 94) and MAGIC086 (row 28)

**Status:** IN PROGRESS (2026-09-25) - paused; see below.

## Paused (2026-09-25)

- **Done:** all 43 functions of the two extents written (`src/game/magic_s19.cpp`),
  `symbols.toml` entries (43 `[[func]]`, 12 `[[data]]`), module in
  `CMakeLists.txt` and at the end of `inject_all.cpp`. Harness changes in
  `magic_harness.h/.cpp`: `Custom` recorders (`Run(g, customs, n)`, `Note`,
  `Salt`, `Stir`), the target-enemy disturbance guarded to target bytes
  0..10, the coverage line wrapped onto more lines.
- **Fuzzed:** `BOF3X_SHADOW=magic_s19` alone: exit 0, 86,000 rounds over 43
  functions, 0 mismatches, every recorder and handler reached (commit
  a84dd23).
- **Open:** `BOF3X_SHADOW='*'` **crashes** (exit 0xC0000005) inside
  magic_s19's run, after every other group passed: an access violation in a
  clone (eip in the 0x095D0000 copy - read the log's `cloned to` lines to map
  it; fault address 0x33432354 through edx, which looks like an unseeded
  pointer that the earlier groups' runs leave different from a lone run).
  Next step: re-add the scratch VEH (`scratchpad/s19/veh.py on`), map the
  eip to the clone, find which pointer the seed does not put inside
  (candidates: the owner's or the source's record read through by a draw,
  or `Gfx_PacketNext` read before the seed sets it), fix the seed, re-run
  `magic_s19` and `'*'`.
- **Controls:** none planted yet. Then: this doc in full (what each function
  does, the fuzz, the controls table, what nothing reached, defects), its
  row in `docs/README.md`, the 43 lines in the main checkout's
  `analysis/calltrace/entries_logic.txt`.

## Row 28's ability (for the report)

Row 28 (MAGIC086) is loaded by ids 0x56 and 0xBC. One id down, the
sibling's `names/magic.toml` has no English label (id 85, jp パリア);
`names/abilities.toml` gives id 85 the us name **Barrier**. Row 94
(MAGIC083) is ids 0x53 / 0xB9, Shield one id down - confirmed by its own
code: `Shield_Kind` (0x4C1710) rewrites an action id 0x136 to 0x53, where
MAGIC082's twin maps its ids to 0x52 / 0x54 / 0x55 (Protect, Speed, Might
one id down) - the five ids 0x52..0x56 are one family of effects. That
row 28 is Barrier rests on the shift holding here: likely, not measured.
