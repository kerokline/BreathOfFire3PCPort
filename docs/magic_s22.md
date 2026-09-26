# Group S22: MAGIC096..099 (Blizzard, Jolt, Lightning, Myollnir)

## Paused (2026-09-25)

- **Written, all 56 functions** (`src/game/magic_s22.cpp`, `BOF3_INJECT` for
  each; `symbols.toml` 56 `[[func]]` + 12 `[[data]]`; module at the end of
  `CMakeLists.txt` and `inject_all.cpp`). Builds clean.
- **Fuzz** (`src/game/magic_s22_fuzz.cpp`): 53 through `magic_harness::Run`
  (group callees, the nine `.data` handler tables, own regions: scratch
  `0x903850`/`0x9037A0`, `Gfx_PacketNext` + a packet buffer, tint records,
  CLUT row 26, MAGIC096 `.data`; own `Disturb`); three through an own
  recorder fuzz (`SelfTestOwn`: `BlizzardShard_PushMatrix`,
  `LightningBolt_PushMatrix`, `Blizzard_CenterOnTargets` - pointer vectors /
  divide-by-zero guard). **Own fuzz: 0 mismatches over 6,000 rounds.**
- **Harness run: segfaults** (exit 139) somewhere in the 53; a per-clone
  bisect (`scratchpad/s22/bisect.sh`, env `S22_FROM`/`S22_TO`) was running.
  The fuzz file carries **DEBUG** hooks (`g_base`, `S22_FROM`/`S22_TO`,
  `<cstdlib>`) to remove before finishing.
- **Controls: none planted yet.** `entries_logic.txt` lines not yet appended;
  doc body (functions, seeds, controls table, latent defects) not written.
- **Coordinator note:** do not edit `magic_harness.*`; HX is consolidating.
  Port onto HX's API later.
- **Next step:** rerun `bisect.sh 0 52` (each clone alone), find the
  faulting clone (likely a draw writing through an unaimed `Gfx_PacketNext`
  or a loop bound; or a 3-arg draw with random args), fix seed/ours, get
  `magic_s22` and `'*'` to exit 0, then controls, doc, entries_logic.

Latent defects found so far (for the coordinator to number):
`Blizzard_CenterOnTargets` divides by zero when every actor on the side is
out (ours aborts); every dispatcher's table index is unchecked (ours aborts);
`BlizzardShard_Launch`/`_Grow` index their `.data` tables by +4 unbounded;
`Jolt_Start`/`Lightning_Start` copy slot 0's position when no bolt was made.
