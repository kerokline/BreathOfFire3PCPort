# Round ten's cleanup: the debts the scenario and area rounds left

**Status:** IN PROGRESS (2026-09-28) - the list for a session of its own,
after round ten's PR. Every item is owed by
[`takeover-queue-round10.md`](takeover-queue-round10.md) (sections 10, 13,
16 and 19 name the evidence); nothing here changes game behaviour, so no
DIVERGENCE entry is expected unless an item says so. Cross off items
here as they land; the round doc stays as the record of what was found.

**Landed 2026-09-28, from a cloud session without the game files** (branch
`claude/round-10-cleanup-handoff-qtwcrk`; verified by the i686 build,
`ledger_check.py`, `gen_symbols.py` and `tables.py check` - the shadow
self-tests, the frame hash and the route A/Bs need `BOF3.exe` and are the
owner's to run at the tip): item 1's rebinding of every raw constant whose
target is ours (234 in 41 files, the round-nine form); item 2 in full
(D133..D161); item 4's `--no-write`; item 5 in full; item 7's merge-script
note in HANDOFF. **Left**, each marked below: item 1's `SH_CALL` form with
the harness rows moved, the world-map body shared once, and the linker
folds' calls by name where a fold's owner header has no prototype; item 3
whole; item 4's tool fixes that want the exe to test; item 6 whole; item
7's scratchpad copies.

Round ten took 2,195 functions in 47 groups over six waves (3,510 ->
5,706 ours), every group fuzz-only through `scenario_harness` and
`area_harness`. The waves were fast because each group bound only its
own band and called everything else by raw address; the price is the
list below.

## 1. The rebinding pass (first, it unblocks the rest)

Every `SH_AT` / `AH_AT` and `_callees.h` raw address whose target is ours
now becomes a call by name through the owner's header. The wave docs
list them per group ("cross-group raw-address calls" and "inbound calls"
in each `docs/scena_*.md` / `docs/area_*.md`); the round doc's sections
10, 13, 16 and 19 gather the cross-group ones. In particular:

- **The engine groups' callees**: SX's nineteen and SX2's thirteen are
  called raw from the chapter blocks (SC2, SC13, SC15, ...) and the area
  blocks (AR1B, AR1C, AR2A, AR2B, AR2D, ...). The harness standard-set
  columns for `0x4410B0`, `0x532ED0`, `0x57C6B0`, `0x56FCA0`, `0x56D6F0`
  move from `SH_THEIRS` / `AH_THEIRS` to `_OURS` **in the same commit**
  as the calls they gate; switching one without the other breaks the
  raw calls (section 10).
- **The linker's folds between areas**: areas 104 and 121 hold each
  other's code (section 16: AR2E calls `0x41BE10`, `0x41C0A0`,
  `0x41C0E0`, `0x41C110`, `0x41C350`, `0x41C5B0`, `0x41ACD0`, `0x41B730`
  raw; AR3B calls `0x415640`, `0x415680`, `0x4156C0`, `0x415940` raw);
  areas 151 and 152 (`Area152_PlateStart` `0x424BA0` is area 151's
  plate state 0; `Area152_Record8Spawn` `0x4253C0` is record-8 state 0
  for all ten world maps, section 19); `0x4220D0`
  (`Area146_DrawGlowCylinder`) from `area_w0c`, `area_w1d`, `area_w2d`
  and areas 112 / 116; `0x40E750` (`Area75_DrawWindow`) from
  `Area42_TimerTail`; `0x42C2D0` from area 191; `0x42BA90`
  (`Area191_TalkMessage`) and `0x42C0A0` from SC13 / SC15; SC13's
  `kArea141a..e` (`0x4204D0..0x420710`); `0x420A90`
  (`Area143_ClutShiftRight`) from `scena_sc13_callees.h`; `0x56FCA0`
  from AR3G (ours since SX).
- **Engine code into area code**: `event_ops.cpp`'s `kStepHandlers` /
  `kArriveHandlers` hold raw addresses of step and arrive hooks now ours
  (every area group's "inbound" list); `Effect_KindHandlers` and
  `EffectKind18_States` entries; `Field_LeaderStates[12]` / `[13]` and
  effect kind `0x5C`'s split by area number (`0x52FE90`, `0x462B60`).
  Table entries read in place need no rebinding; `call` / `jmp` sites in
  ours do.
- **The world-map body**, one body of code nine times in Capcom's
  binary (areas 16, 33, 45, 65, 87, 88, 104, 115, 121, 151, 152 over
  their own tables, with a hook or draw of their own in 104, 121, 151
  and 152), is five copies in ours: AR2B's in an anonymous namespace,
  AR2E's, AR3A's, AR3B's, AR3G's on AR3A's. Share one body over a table
  struct per copy; the per-copy differences are listed in sections 13,
  16 and 19.

Verify by the build, `BOF3X_SHADOW='*'` headless, `ledger_check.py`,
and the frame hash (item 6).

**Done (2026-09-28, `a1f2d71`):** every raw constant in the scenario and
area groups' `_callees.h` headers and `.cpp` tables whose target has an
`impl` reads `bof3::addr::<Name>` inside the same `constexpr` / `SH_AT` /
`AH_AT` / table entry - 234 constants in 41 files, the round-nine form
(round9 doc section 12: the same value, so the fuzz files' stand-in keys
and the game's calls are unchanged; each file says so beside its include).
That covers the engine groups' callees, the area 104 / 121 and 151 / 152
folds, `0x4220D0`, `0x40E750`, `0x42C2D0`, `0x42BA90`, `0x42C0A0`,
`kArea141a..e`, `0x420A90` and `0x56FCA0` from AR3G. Left raw on purpose:
the fuzz files' `CallSite` / `Imm` / `kCallees` tables (the keys),
coordinates and bounds that equal a function's address (`0x4D8000`,
`0x558000`, `0x518000`, `0x401000`), the engine callees of item 3, and
`scenario_harness.cpp`'s five raw standard rows. **Left:** the `SH_CALL(Name)`
form and the standard-set rows moved to `SH_OURS` in one commit (they
change the fuzz's keys: `StandIn` resolves a raw key through the row's
`address` either way, but a named key with a raw row is a `Fatal` - so the
pair wants a `BOF3X_SHADOW='*'` run the cloud session cannot make); the
`kStepHandlers` / `kArriveHandlers` and table entries read in place (no
rebinding needed, as above); the world-map body shared once (a refactor of
five copies with no fuzz to prove it here; D143 names the copies).

## 2. The defects' numbering

Six waves describe latent defects in their docs and number none
(`known-defects.md` is the coordinator's). Number them, one entry per
class where a class repeats (the spell round's D89..D92 are the
precedent): the unchecked state dispatchers that ours aborts past (most
groups), unchecked `Effect_FindFree` "none" slots written as 0xFF
(AR2E, AR3B, AR3D, AR4C), reads by a signed choice or an unchecked list
byte, divides by a byte that can be 0 (AR2D, AR3D, AR4E), tables laid
back to back (AR3D, AR3E, AR4B), `Area53_Trigger42`'s undefined answer
(AR1D), and the ones for the owner:

- **Bare-`ret` inits on the PC** where the PSX has code: areas 145,
  148, 153, 154 all point at `0x437CC0` (PSX `0x801F5324`,
  `0x801F4274`, `0x801F2C5C`) - AR3F, AR3G, AR4A.
- **Area 198's handler 9 always plays sound effect 0** (AR4F).
- **The turn steps read the step as an s8** (steps of 12 degrees or
  more turn the other way, SX2).
- **A cue with bank 0 reads before `Sound_Banks`** (SX2).

Four places where wave docs contradict each other were noted for the
spell round (round9 doc section 12); read for the same here.

**Done (2026-09-28):** D133..D161 in [`known-defects.md`](known-defects.md)
- the classes collapsed to one entry each (D133 the dispatchers, D134 the
tables back to back, D135 `Effect_FindFree`'s none, D136 the unchecked
bytes and counts, D137 the divides, D143 the world-map copies' searches,
D144 the walks, D146, D147, D153, D154, D155, D161) and the owner's four
their own (D138 the bare-`ret` inits, now eight areas: 56, 75, 90, 108,
145, 148, 153, 154; D139 sound effect 0; D140 the s8 turn step; D141 the
bank-0 cue), with `Area53_Trigger42` D142 and the rest D145, D148..D152,
D156..D160. Every group doc's defects section names its numbers. **The
contradictions, written into the entries for one read of the code each:**
three dispatcher policies (abort outside the table: SC1, SC2, SC5, SC6,
SC9a, SC9b, SC12, SC13 and every area group but AR0B; abort only at a
non-code word: SC0, SC15; read in place: SC3, SC7, SC11 and AR0B's area 16
- D133); a record-255 write reproduced by AR3B and aborted by AR3D / AR4C
for the same shared body (D135); "overlap" meaning adjacency in SC0 / SC12
but not in SC1 (D134); AR3B's "eight tables" against its six plus three
(D133); `Area111_ArmTailAtLeaderCell`'s record-14 address `0x8028F0`
against the stride's `0x802900` (D136); area 16's buttons table with no
overread where 45 / 87 / 88's read eight (D134); the "none chosen" weights
read for areas 72 / 73 / 124 / 125 but not area 20 (D154).

## 3. A small engine group for the callees nobody owns

Raw after the round (each group's doc says who calls it): `0x454A80`,
`0x455290` (release and start of `Field_Slots` scripts), `0x455450` (a
field reset), `0x486D60` (a map set-up, `void (void)`), `0x5A7570` (a
POLY_F3 setter), `0x494060`, `0x494110`, `0x4941E0` (the map camera
set-up, a point-to-vertex projection, a screen size at a point - the
same helpers the spell round left raw), `0x46D710`, `0x46D770` (spawn
states), `0x511C10` (the map height), `0x441090` (a 16.16 round-up),
`0x5B9450` (the CRT `strncpy` - leave it), and SX2's inbound callers
`0x4FEEB0`, `0x46BF80`, `0x46C100`, `0x482930`, `0x4703F0`, `0x4712E0`,
`0x4849A0`, `0x432750`, `0x459720`, `0x464E40`. One group as SX / SX2
were (`docs/scena_sx2.md` the model); decline what belongs to a larger
unit.

**Left** (wants the binary to read): none of it can be done without
`BOF3.exe`.

## 4. The tools

`tools/area_rows.py` and `tools/scenario_rows.py` / `magic_rows`:

- `magic_rows._cmp_bound` misses a `cmp reg, reg` bound (`0x5413D0`
  dropped as a byte table, SC2; section 10).
- Tail kinds armed through a register (`mov byte [0x9039F3], reg`):
  area 44's `0x4075D0` / `0x407940` / `0x4077F0` were gaps (section 10).
- A descent that stops at 0x10 when the entry jumps over its own loop
  head (`0x56FCA0`, section 10).
- A handler array counted as a seventh state-table entry (`0x614728`,
  `0x624704`; sections 13, 16).
- s16 coordinate pairs read as pointers (`0x40FDE0` / `0x40FF20` in
  areas 6, 9, 24, 67, 122, 123; section 13).
- A body reached only by its own tail `jmp` split in two (`0x41C2B0`,
  section 16); a `jmp` with displacement 0 read as one function
  (`0x42D4D0` / `0x42D580`, section 19).
- `area_funcs.tsv` shorter than the rows (shared handler bodies,
  triggers; sections 13, 19).
- The tool writes `area_rows.tsv` / `area_funcs.tsv` into the
  `--analysis` directory it reads, from the running checkout's
  `symbols.toml`: a `--no-write` flag (section 13). **Done (2026-09-28,
  untested against the exe: the flag only skips the two writes).**
- `pairs_propagated.json` pairs jump-table cases as functions
  (`0x5455A0`, `0x54AAD0`, `0x551E40`, `0x553070`, `0x559AD0`,
  `0x53DF10`) and has HANDOFF item 9's swaps.

**Left:** every other fix here changes what the tools read off the exe,
and each wants a run against it to show the row it now finds.

## 5. The harness docs

For [`magic_harness.md`](magic_harness.md) /
[`scenario_harness.md`](scenario_harness.md) /
[`area_harness.md`](area_harness.md):

- An `args` hook that writes memory is lost - the harness captures the
  state before the arguments; a plant that must change memory goes in
  `Seed` (SX, section 10).
- A `kPhase` callee never runs its `effect` (the recorder returns
  first); a louder stand-in on a phase callee wants `kGarbage` (AR1F,
  section 13).
- Effects and every group callback draw from `Noise()` only, never
  `AH_PICK` / `Next()` (AR3C, 1,727 false mismatches; section 16; the
  `disturb` rule of section 4 generalised).
- `CloneOriginal` refuses an entry that opens with Capcom's own `jmp`
  over eleven `nop`s (areas 63, 64, 72, 73, 110, 124, 125, 192); clones
  start at the body `0x10` on; `BOF3_INJECT` has no such guard (section
  13).
- A clone with more than 64 call sites is copied by the fuzz file
  (SC12, SC5).
- `BOF3X_SHADOW='*'` died silently twice in wave three (exit 127, no
  Fatal) and never since; a note in SCAFFOLDING's self-test section.

**Done (2026-09-28, `3a50354`):** the first five in
[`magic_harness.md`](magic_harness.md) §5 (each checked against the
harness sources; the scenario and area docs point at them), the last in
[`SCAFFOLDING.md`](SCAFFOLDING.md) §2.

## 6. The live side

- **The frame hash**: `analysis/validate_round9_hash.sh` against
  `r9_orig`; nothing round ten took is on the attract path, so it
  should stand - confirm once at the tip.
- **The route A/Bs** (`validate_combat.sh`, `validate_shop.sh`, the
  world map's): one run at the tip when the owner is away.
- **The recipe saves per chapter** (scenario plan section 5) and the
  live check per area (the owner's): 200 areas, 20 chapters, none
  played under both sides beyond the world-map route (areas 33, 45,
  88, 104, 115) and the combat route (area 29).
- **The `inject:` count one short of the `impl` count** (5,706 against
  5,707, since before wave two): one read of `InjectReport`.

**Left:** all of it needs the game.

## 7. Housekeeping

- The controls scripts of every round-ten group live in session
  scratchpads, not in git: waves one to three in
  `.../0eefe2a8-ba23-4625-9434-7c4f87a1456f/scratchpad/<group>/`, waves
  four to six in `.../71e258cd-639f-4084-8bfa-60f9e4a9ffda/scratchpad/<group>/`
  (with `merge_group10v.sh`, `keepboth.py`, `one_grow.py`,
  `verify_tip.sh`, `closeout_wave*.py`). A Temp folder: copy them
  somewhere durable if they are to outlive a cleanup.
- The verification worktree `.../0eefe2a8.../scratchpad/verify` (its
  own build) can go once the next round has its own.
- `.claude/worktrees/agent-ab92022bdcdcb0640` (AR4A's, merged) is
  untracked by git but held open by a leftover process; delete it once
  that ends.
- The merge script for the next round wants `one_grow.py`'s step from
  the start: `DrawPool_Grow` is the last inject line, and keep-both
  doubles or misplaces it whenever a branch forked before a reorder
  (sections 13, 16).

**Left:** the copies and the deletions are on the owner's machine; the
merge-script note is in HANDOFF's routine.
