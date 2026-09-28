# The tenth round's queue: the scenario banks and the area overlays, wave by wave

**Status:** IN PROGRESS (2026-09-28) - four waves merged, 1,426 functions, 3,510 -> 4,936 ours; **the scenario round is complete**; the area round has worlds 0 and 1 whole and world 2 to area 103

Round nine took every spell overlay through one harness
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)).
The owner asked on 2026-09-27 for round ten to open both queues the other
session planned - the scenario round ([`takeover-queue-scenario.md`](takeover-queue-scenario.md),
IDEAS I24) and the area round ([`takeover-queue-areas.md`](takeover-queue-areas.md),
IDEAS I25) - on the spell round's pattern: about eight Opus agents a wave,
one group each in a worktree, merged one at a time, so a wave fits one
usage window. The branch is `phase-3/capture-round-ten`, from `main`'s
`c47f521` (3,510 ours).

## 1. Wave one (staged 2026-09-27 afternoon, from `c47f521`)

Neither plan has its harness or its tool yet (each plan's "Before the first
cut"), so wave one carries them alongside the first scenario groups. The
round-nine lesson applies: **groups do not edit a harness** - one group
builds each, the rest write against its API.

| Group | What | Fns to take | Depends on |
|---|---|--:|---|
| SCH | the scenario harness `src/game/scenario_harness.*` from `magic_harness`, the clone-table tool for a chapter band, the 20 vtables and the call tables named, chapter 0 (SC0, `0x537F20..0x539AD0`) taken through it as the proof | 21 | - |
| ART | the area tool `tools/area_rows.py` (the seven root tables, the descriptors' data blocks, the walk, per-area blocks, the group cut), the two engine tables `0x662CE8` / `0x662F28` and the descriptor layout named; no functions taken | 0 | - |
| ARH | the area harness `src/game/area_harness.*` from `magic_harness` with the six area call shapes, proved on one small world 0 area; the cell hook `0x56E670` taken | ~5..8 | ART's tool replaces its hand-built clone table |
| SE | the nine engine-side helpers the chapters share (`0x4410B0`, `0x520000`, `0x524870`, `0x579D70`, `0x57A010`, `0x591CC0`, `0x508000`, `0x5080A0`, `0x519F70`; the plan said eight - the reading settles it) | 9 | SCH's harness for the fuzz |
| SC1 | chapter 1, `0x539AD0..0x53DDA0` (58 starts, 3 ours; 12 starts the walk did not reach) | 55 | SCH |
| SC3 | chapters 3 and 4, `0x5428C0..0x546390` | 54 | SCH |
| SC11 | chapter 11, `0x55C040..0x55E4E0` | 33 | SCH |
| SC12 | chapter 12's first block, `0x55E4E0..0x561DB0` (few functions, long ones) | 19 | SCH |

About 195 functions plus two harnesses and one tool. Counts are starts in
the band (`pc_funcs` + `pc_hidden` + the walk's added starts) not yet
`impl`; the plan's counts were the walk's closures, which cross bands
(chapter 12's closure runs to `0x567A90`, SC13's block). **A group owns the
functions whose address lies in its band**, as the spell round's did.

**Two stages.** SE and SC1..SC12 cannot fuzz before SCH merges. Stage A
(now): every group reads its functions to the last instruction, writes
ours, `symbols.toml`, its doc and its fuzz file against the harness's API
contract (the brief, `analysis/round10_wave1_brief.md`: `magic_harness`'s
API one for one under `scenario_harness` / `SH_`), commits, and reports
"ready for the harness". Stage B: after SCH merges, each group is resumed
with the harness's SHA, merges it, builds, runs its fuzz and controls, and
reports. ART and ARH are one stage; ARH's clone table is hand-built (as
round nine's group E) until ART's tool lands.

**What the wave leaves for wave two:** scenario SC5, SC6, SC7, SC9a
(plan §4 wave 2), and the area round's world 0 groups as ART's tool cuts
them.

## 2. For the coordinator

- Merge order: SCH first (the five groups' stage B waits on it), then ART,
  ARH, then SE, SC1, SC3, SC11, SC12 as they report. The merge script is
  round nine's (`merge_group.sh`, keep-both for `CMakeLists.txt`,
  `inject_all.cpp`, `symbols.toml`, `docs/README.md`; repeat the shared
  `[[func]]` header; `tomllib` parse, no duplicate `pc`), branch names
  `phase-3/round10-<group>`.
- After the wave: `analysis/consolidate_entries.py`; every `impl` start has
  an `entries_logic.txt` line; the frame hash is untouched unless a taken
  function is on the attract path (chapter 16 is; nothing in wave one is,
  by the plan - check the trace).
- The rebinding pass (raw-address calls into SE and across bands) at the
  round's end, as round nine's.

## 3. Wave one, stage A reported and the harnesses merged (2026-09-27 evening)

Every agent was cut once by the usage limit about twenty minutes in and
resumed with its worktree intact; SCH had already committed the tool, so
the five waiting groups built their clone tables from it (`git show
222eb0f:tools/scenario_rows.py`) instead of by hand.

**Merged, in order** (each: keep-both merge, `tomllib` no duplicate `pc`,
build, the group's shadow and `'*'` exit 0, `ledger_check` 0):

| Group | Merge | Taken | Controls | Doc |
|---|---|--:|---|---|
| ARH | `8f0172c` | 4: area 11's two handlers and init, `Area_CellHook` `0x56E670` | area 11: 18 of 20 refused by a count, 1 refused then hung (its variant refused), 1 equivalent with a refused variant; cell hook 6 of 6 | [`area_harness.md`](area_harness.md), [`area_011.md`](area_011.md) |
| ART | `d08471d` | 0 | - | [`area-rows.md`](area-rows.md); `takeover-queue-areas.md` §1a, §3 |
| SCH | `218eeec` | 19 (SC0) | 105 of 105 refused by a count | [`scenario_harness.md`](scenario_harness.md), [`scena_sc0.md`](scena_sc0.md) |

3,510 -> 3,533 ours.

**Stage A reports** (all five: ours written, `symbols.toml` with `impl`,
doc with a "Stage A done" section, fuzz file against the contract, syntax
check clean, `ledger_check` 0; nothing built or fuzzed; resumed for stage
B from `218eeec`):

| Group | Functions | Of the band's starts | Notes |
|---|--:|---|---|
| SE | 6 of 9 | - | `0x520000` is one of 18 copies of `Field_CellPickup` (a party-action round's), `0x508000` / `0x5080A0` belong to a 22-function object state machine behind `0x65E710`; the walk read `push 0x520000` / `0x508000` coordinates as code pointers. `EventObj_Face` and `EventOp_0x` gain `impl` in place. |
| SC1 | 42 | 13 are jump-table cases; `0x53A2C0` (run 1) was at the frontier because the catalogue mislabels it | the brief swapped two names: `0x539B20` is `Scena01_EnterArea`, `0x53D830` `Scena01_StepHook` |
| SC3 | 50 | 5 are jump-table cases (so `pairs_propagated.json`'s pair for `0x5455A0` is wrong); one start no list had, `0x544AC0`, the shared tail of four object handlers | 14 tables named |
| SC11 | 30 | 6 are switch cases | run table of 10 entries indexed to 0xE by triggers 9..13 (latent) |
| SC12 | 24 | 5 vtable slots hidden under a run-on `pc_hidden` start | chapter 12's state 0 is in SC13's block |

**What the wave found that the plan lacked:**

- **The call tables' block.** Chapters' call tables A and B point into
  engine code at `0x519890..0x51AC50`, outside every planned band: 99
  starts (`scenario_rows.py` lists it as unit `CALLS`), only SE's
  `Scena08_PartyJoin784` taken. A group for wave two.
- **Raw-address callees nobody owns**, reported by four groups:
  `0x532ED0`, `0x533E50`, `0x533E00`, `0x534DB0`, `0x537480`, `0x56D6F0`,
  `0x56D800`, `0x56FCA0`, `0x57C550`, `0x57C6B0`, `0x57CD90`, `0x587B80`,
  `0x590C90`, `0x591900`, `0x519FA0`. `0x5341C0` is named
  `Scenario_CallB` (SCH), not taken. Candidates for a small engine group
  beside CALLS.
- **The area round's numbers** (ART, `takeover-queue-areas.md` §1a): the
  band ends at `0x42D710`, not `0x430000` (BATE / BATTLE code after it);
  an eighth root table, `Field_ObjectTriggers` `0x662E20`; the cell-hook
  table `0x662F28` holds 28 records, not 100, and its entries are `(x, z)`
  hooks answering in `al`, not phases (ARH); `+0x38` is set in area 77;
  28 groups over 1,465 functions. World 0 for wave two:

  | Group | Areas | Band | Fns | Ours | To take |
  |---|---|---|--:|--:|--:|
  | AR0A | 0..5, 7..8, 10..13, 15 | `0x401000..0x401B80` | 51 | 0 | 51 |
  | AR0B | 16, 18..26 | `0x401B80..0x403400` | 61 | 0 | 61 |
  | AR0C | 27..29, 32..37 | `0x403400..0x4053B0` | 75 | 20 | 55 |

  No small world 0 area is on any recording; area 33 (the world-map
  route) is the one with a live check, 7 functions not yet ours.
- **Tool fixes folded in** (`207ef4e`): five byte tables of two-level
  switches listed as functions (SC1 found two), and the shape name
  `kEntry`.

## 4. Wave one merged (2026-09-27, 17:00..20:30)

All eight groups merged into `phase-3/capture-round-ten`, one at a time,
each with the build, the group's shadow and `BOF3X_SHADOW='*'` headless
(exit 0 every time), `tomllib` no duplicate `pc`, `ledger_check` 0
errors. **175 functions taken, 3,510 -> 3,685 ours.** Counts are each
group's in its own worktree (the harness's pointers into our DLL make the
call counts build-directory dependent, round9 doc section 6).

| Group | Merge | Taken | Rounds | Controls planted / refused | Not refused | Doc |
|---|---|--:|--:|---|---|---|
| ARH | `8f0172c` | 4 | - | area 11: 20 / 19; cell hook 6 / 6 | 1 equivalent, its variant refused | [`area_harness.md`](area_harness.md), [`area_011.md`](area_011.md) |
| ART | `d08471d` | 0 | - | - | - | [`area-rows.md`](area-rows.md) |
| SCH | `218eeec` | 19 | 76,000 | 105 / 105 | | [`scenario_harness.md`](scenario_harness.md), [`scena_sc0.md`](scena_sc0.md) |
| SE | `2c51995` | 6 | 12,000 | 32 / 30 | 2 equivalent (sign vs zero extension before `shl 16`; a byte read twice with no call between), variants refused | [`scena_se.md`](scena_se.md) |
| SC11 | `7715486` | 30 | 240,000 | 54 / 54 (52 by a count, 2 by a fault, the fault's near variant by a count) | | [`scena_sc11.md`](scena_sc11.md) |
| SC3 | `c0f08ba` | 50 | 100,000 | 76 / 75 | 1 equivalent (an unsigned index whose value is always 0..0x13 or 0xFF, returned before use), variant refused | [`scena_sc3.md`](scena_sc3.md) |
| SC1 | `0d3fd2c` | 42 | 252,000 | 100 / 98 | 2 equivalent (a store overwritten after the call; two commuting xors), variants refused | [`scena_sc1.md`](scena_sc1.md) |
| SC12 | `753dc2e` | 24 | 192,000 | 66 / 65 | 1 equivalent (an area re-read whose branches only return), variant refused | [`scena_sc12.md`](scena_sc12.md) |

Every group is fuzz-only: nothing in wave one is on the attract path
(chapter 0 is a new game's first chapter, not the demo's), so the frame
hash reference `r9_orig` stands. The live check per chapter is the recipe
save the plan names (scenario plan section 5); chapter 0 needs none.

**What the groups learned, for the next wave's brief:**

- **Controls not refused on the first run are the fuzz's fault first.**
  Four groups (SC1, SC3, SC11, SC12) had controls stand on a first run
  and refused every one by strengthening the fuzz: step-paired counter
  seeds, rectangle-paired hook arguments, an `effect` that moves the
  chapter's own cells (the harness's disturbance reaches a group cell
  about one call in 24), a `settle` for the area-entry state, typed
  stand-ins one per table entry, rounds raised to 6,000..8,000.
- **One `Group` sets one chapter byte**: a two-chapter group runs two
  `Run` calls under one shadow name (SC3).
- **A `.data` table whose handlers take arguments** wants a typed
  stand-in per entry, as `scena_sc0_fuzz.cpp`'s `ObjectEntry`; the
  handler recorder logs no arguments.
- **The harness's clone limit is 64 call sites**: `Scena12_Run4` (117)
  and `Scena12_Run8` (102) are copied by SC12's fuzz file itself and
  handed to the harness as a `jmp`. A later harness could raise the limit
  and take them back.
- **Not every start is a function**: 35 of the wave's listed starts were
  jump-table or switch cases (SC1 13, SC11 6, SC3 5, SCH 4, plus the tool's
  five byte tables), and 10 real functions were in no list (vtable slots
  under a run-on `pc_hidden` start, a shared tail). `scenario_rows.py`
  finds both kinds now.
- **A group `disturb` from `Next()`** made 403 false mismatches (SC3), as
  S30 did in round nine. It is in the brief; keep it there.
- **The aborts on an unchecked index are a real behavioural edge**: SC12's
  run dispatchers abort where movement-script op `F6` could write a run of
  10 or more and the original would run an object handler. Nothing
  measured reaches it; the owner's rule (no DIVERGENCE entry) stands
  unless a route does.

**Owed by the wave:** `known-defects.md` numbering for the groups' latent
defects (unchecked dispatch tables in every chapter; SC11's run table of
10 indexed to 0xE; SC1's effect-record clear at slot 0xFF and member write
past 8; SC3's effect record -1; SE's negative count in `EventOp_0x`; ARH's
header walk that never ends on a 0 step); the rebinding pass at the
round's end; the two `entries_logic.txt` extents SE corrected
(`0x520000` to `0x11F`, `0x5080A0` to `0x305`, applied by hand after
`consolidate_entries.py`); the fix for `pairs_propagated.json`'s pair of
`0x5455A0` (a jump-table case, SC3).

## 6. Wave two staged (2026-09-27 night, from the tip after `368b84f`)

Eight groups, one stage each (both harnesses exist). Counts are the
tools' at this tip (`scenario_rows.py`, `area_rows.py --groups`): starts
in the band less ours. The fifteen engine callees of section 3 wait for
wave three; the `Playthrough fixes` commit `368b84f` (the other session's)
is in the base. Brief `analysis/round10_wave2_brief.md`, group lines
`analysis/round10_wave2_groups.tsv` (gitignored).

| Group | What | Band | To take | Module |
|---|---|---|--:|---|
| CALLS | the chapter call tables' entries, engine-side, shared between chapters | `0x519890..0x51AC50` | 98 | `scena_calls` |
| SC5 | chapter 5 | `0x546390..0x54A910` | 35 | `scena_sc5` |
| SC6 | chapter 6 | `0x54A910..0x54F080` | 48 | `scena_sc6` |
| SC7 | chapters 7 and 8, with 6's shared tail | `0x54F080..0x553B30` | 52 | `scena_sc7` |
| SC9a | chapter 9's first block | `0x553B30..0x557170` | 22 | `scena_sc9a` |
| AR0A | world 0: areas 0..5, 7..8, 10..13, 15 | `0x401000..0x401B80` | 48 | `area_w0a` |
| AR0B | world 0: areas 16, 18..26 | `0x401B80..0x403400` | 61 | `area_w0b` |
| AR0C | world 0: areas 27..29, 32..37 (the combat and world-map routes enter 29 and 33) | `0x403400..0x4053B0` | 55 | `area_w0c` |

419 functions: over the ~350 a usage window held in round nine, so a cut
mid-wave is expected and resumable (commit early is in the brief).

## 5. Wave two (as listed before staging)

Scenario: **CALLS** (the call tables' block `0x519890..0x51AC50`, 99
starts, `scenario_rows.py --unit CALLS`), SC5, SC6, SC7, SC9a (plan
section 4); a small engine group for the fifteen raw-address callees
nobody owns (section 3). Area: **AR0A, AR0B, AR0C** (section 3's table;
area 33 has the world-map route as a live check). About eight groups
again; the scenario groups are one stage now that the harness exists.

## 7. Wave two merged (2026-09-27 night to 2026-09-28)

All eight groups merged into `phase-3/capture-round-ten`, one at a time,
each with the build, the group's shadow and `BOF3X_SHADOW='*'` headless
(exit 0), `tomllib` no duplicate `pc`, `ledger_check` 0 errors. **417
functions taken, 3,685 -> 4,102 ours.** No agent was cut this wave; the
first to report (CALLS) finished in half an hour, the last (SC5) in an
hour. Counts are each group's in its worktree.

| Group | Merge | Taken | Rounds | Controls planted / refused | Not refused | Doc |
|---|---|--:|--:|---|---|---|
| CALLS | `eed855c` | 98 | - | 106 / 106 | | [`scena_calls.md`](scena_calls.md) |
| SC9a | `c436bfa` | 22 | 132,000 | 86 / 86 | | [`scena_sc9a.md`](scena_sc9a.md) |
| AR0A | `2b09501` | 49 | 294,000 | 98 / 97 | 1 equivalent (two pose tables with identical bytes), variant refused | [`area_w0a.md`](area_w0a.md) |
| SC6 | `f58d38f` | 48 | 288,000 | 92 / 92 | | [`scena_sc6.md`](scena_sc6.md) |
| AR0B | `ba9ac60` | 61 | 256,000 | 94 / 94 | | [`area_w0b.md`](area_w0b.md) |
| AR0C | `d36a5ff` | 52 | 312,000 | 123 / 122 | 1 equivalent (sign vs zero extension before `<< 16`), variant refused | [`area_w0c.md`](area_w0c.md) |
| SC7 | `c13d392` | 52 | 312,000 | 58 / 57 | 1 equivalent (a pool pointer's index the same by 0xA0 or 0xA4), variant refused | [`scena_sc7.md`](scena_sc7.md) |
| SC5 | `8e83993` | 35 | 700,000 | 110 / 110 | | [`scena_sc5.md`](scena_sc5.md) |

**A scaffolding ceiling fell at SC5's merge**: `detour.cpp`'s
owned-function table held 4,096 and the merge made 4,102; the self-test
died with `FATAL: Scena05_Object06: more than 4096 injected functions`
after SC5's own fuzz had passed. Raised to 16,384 (`b5800c9`; about
10,200 real functions in the exe). The other session's `368b84f` had left
`DIVERGENCE.md`'s status line one short of DIV-0058; fixed at `8e4fcd7`
(SC6 fixed it too; the merge took one).

**Fuzz-only, again.** AR0C read the combat and world-map routes' traces:
they reach none of its 52 (only round seven's and eight's functions in
areas 29 and 33 are on them), so the route A/Bs would show only that
nothing else moved. Not run this wave; worth one run of
`validate_combat.sh` and the world-map A/B when the owner is away, since
the base now also carries `368b84f`'s menu and glyph changes.

**What the wave found:**

- **Chapters 6, 7 and 8 share no code.** The plan's "7 and 8 run 6's
  shared tail" was the walk over-reading chapter 6's jump table
  `0x6611A8` past its 4 entries into `Scena07_Hooks`, and chapter 7's
  cell table into `Scena08_Hooks` (SC6, SC7 independently). Each chapter
  has its own vtable, scenes and runs.
- **The CALLS block is party changes only**: 100 entries, every one a
  party change (join, drop, reorder, palette reload); two read the
  caller's `ecx` (the entry index) through a naked entry and overwrite
  the return address at a member count above 4 (ours aborts after doing
  what the original does).
- **`pairs_propagated.json` pairs jump-table cases** as functions:
  `0x5455A0` (SC3), `0x54AAD0` (nine PSX addresses, SC6), `0x551E40`,
  `0x553070` (SC7). One fix for the pairing tool, owed.
- **The area tool's two gaps** (AR0A, AR0C): its start rule wants a
  padding or `ret` byte before a start, so `0x401000` - the first byte of
  `.text`, area 0's choice 0 - is in no list; and its tail-kind scan reads
  immediates only, so a kind armed through a register (`mov al, 1 ... mov
  [0x9039F3], al`, area 27's choice 0 arming `0x403570`) is a gap. Both
  owed to `tools/area_rows.py`.
- **Area 16 is area 33's world-map code compiled again** with its own
  tables (a capstone compare differs only in jump targets and table
  addresses), plus one field hook of its own; the eleven world-map areas
  are eleven copies on the disc and mostly one here (DA), but not all.
- **Raw-address callees nobody owns**, now about twenty across the waves:
  section 3's fifteen plus `0x534030` (removes a member), `0x533E00`
  (reloads the members' palettes), `0x591BC0` / `0x591BE0` / `0x591B60`
  (money take, money give, inventory take), `0x498DE0` (the level-up
  routine, round nine's boundary), `0x591900`. A group for wave three.
- **Inbound calls from later blocks**: `0x55BBB1` (chapter 10, SC9b) into
  `Scena07_PartyHas89State2`; area 77's handler `0x40F090` into
  `Scena06_Leap` (the four starts chapter 6's walk missed). For the
  rebinding pass.

**Owed by the wave:** the defects' numbering (`known-defects.md`) for
both waves' latent defects; the rebinding pass; the two `area_rows.py`
fixes; the pairing tool's jump-table cases; the route A/Bs above.

## 9. Wave three staged (2026-09-28, from the tip after `481fe76`)

Eight groups, one stage each. Counts are the tools' at this tip. Brief
`analysis/round10_wave3_brief.md` (wave two's with sections 4 and 7
folded in, and a rule that an agent kills a hung self-test only by the
pid its launcher printed, since the owner may be playing from the same
`BOF3.exe`), group lines `analysis/round10_wave3_groups.tsv` (gitignored).

| Group | What | Band | To take | Module |
|---|---|---|--:|---|
| SC2 | chapter 2, both halves of the plan's split | `0x53DDA0..0x5428C0` | 71 | `scena_sc2` |
| SC9b | chapter 9's tail and chapter 10 | `0x557170..0x55C040` | 63 | `scena_sc9b` |
| SC13 | chapter 12's tail, chapters 13 and 14 | `0x561DB0..0x567DC0` | 51 | `scena_sc13` |
| SC15 | chapter 15 (with `0x537580`), SC16's leftover slot `0x56C080`, and a reading of chapters 17..19's block `0x56C130..0x56D5E0` (60 starts the walk never reached: taken if it is chapter code, reported if not) | `0x567DC0..0x56B2A0` | 29 (+ up to 60) | `scena_sc15` |
| SX | the engine callees nobody owns: `0x498DE0`, `0x532ED0`, `0x533E00`, `0x533E50`, `0x534030`, `0x534DB0`, `0x537480`, `0x56D6F0`, `0x56D800`, `0x56FCA0`, `0x57C550`, `0x57C6B0`, `0x57CD90`, `0x587B80`, `0x590C90`, `0x591900`, `0x591B60`, `0x591BC0`, `0x591BE0` | nineteen addresses | 19 | `scena_sx` |
| AR1A | world 1: areas 38..41 | `0x4053B0..0x406650` | 48 | `area_w1a` |
| AR1B | world 1: areas 42..47 (45 is a world-map area; the world-map route enters it) | `0x406650..0x408FF0` | 55 | `area_w1b` |
| AR1C | world 1: areas 48..52 | `0x408FF0..0x40AB00` | 57 | `area_w1c` |

393 functions, plus whatever chapters 17..19 turn out to hold.

## 8. Wave three (as listed before staging)

Scenario: SC2a + SC2b (chapter 2, 71 functions - one group or two),
SC9b (63), SC13 (51), SC15 with SC16's one leftover (29), and **SX**, the
engine callees nobody owns (about twenty, section 7; read each whole,
some may belong to larger engine units - decline those as SE did).
SC17 (chapters 17..19, 60 starts none of which the walk reached) wants
one reading before it is a group: the plan says stubs, the tool says 4.9
KiB of code. Area: world 1's first groups as `area_rows.py --groups` cuts
them (AR1A 48, AR1B 55 with the world-map route in area 45, then AR1C
on). About eight groups, ~400 functions; every group one stage.

## 10. Wave three merged (2026-09-28)

All eight groups merged, one at a time. From SX on, each merge commit was
built and self-tested in a **detached verification worktree** with its
own build directory (`<scratch>/verify`, `merge_group10v.sh`), because
the main checkout's DLL was held by a running game and the other session
had uncommitted work there; the main checkout's build is not touched by a
merge any more. **451 functions taken, 4,102 -> 4,554 ours.** Counts are
each group's in its worktree.

| Group | Merge | Taken | Rounds | Controls planted / refused | Not refused | Doc |
|---|---|--:|--:|---|---|---|
| SX | `f4dae2c` | 18 of 19 | 54,000 | 67 / 67 (66 by a count, 1 by a fault, its variant by a count) | | [`scena_sx.md`](scena_sx.md) |
| AR1B | `47e31db` | 55 | 254,000 | 166 / 165 | 1 equivalent (`| 1` sets bit 0 either way), variant refused | [`area_w1b.md`](area_w1b.md) |
| SC9b | `6c3f0ee` | 63 | 378,000 | 165 / 165 | | [`scena_sc9b.md`](scena_sc9b.md) |
| AR1C | `f4f73b5` | 57 | 342,000 | 185 / 185 | | [`area_w1c.md`](area_w1c.md) |
| SC15 | `f442722` | 89 | 516,000 | 96 / 96 (2 by a fault, variants by a count) | | [`scena_sc15.md`](scena_sc15.md) |
| AR1A | (after `f442722`) | 48 | 288,000 | 180 / 180 | | [`area_w1a.md`](area_w1a.md) |
| SC2 | (after AR1A) | 72 | 432,000 | 235 / 234 | 1 equivalent (an early return nothing after can tell from going on), variant refused | [`scena_sc2.md`](scena_sc2.md) |
| SC13 | `8c49f87` | 51 | 816,000 | 240 / 238 | 2 equivalent, variants refused | [`scena_sc13.md`](scena_sc13.md) |

**The scenario round is complete.** `scenario_rows.py` at `8c49f87`
lists 0 to take in every band: chapters 0..19 (SC0..SC17), the shared
helpers (SE; its three declined addresses are other units'), the call
tables' block (CALLS) and the engine callees (SX; `0x587B80` declined,
named `Sound_StopMusic`). Sixteen groups over three waves, all fuzz-only;
the live check per chapter (a recipe save played through the chapter
under both sides, scenario plan §5) is still owed and is the owner's to
record.

**What the wave found:**

- **Chapters 17..19 are the staff roll**, not stubs: 60 functions the
  walk never reached because the catalogue labels `0x56AD80..0x56D5DF`
  "Event script" by address range (SC15). Chapter 17's vtable points at
  them; the roll is a 351-line scroller with its own font, rays, fade and
  letterbox, then an end task back to chapter 15's area 0x8F, then
  `Boot_Task`. `Scena17_EndTask` never returns; the fuzz runs both sides
  under `__builtin_setjmp` with a `Task_Sleep` stand-in that longjmps.
- **`0x5646B0` is a shared state 0** (sets the state byte to 1) used by
  eight chapters' tables; it lies in chapter 14's block, so SC13 owns it
  as `ScenaShared_State0`.
- **A third world-map copy**: area 45 is area 16's code instruction for
  instruction (two constants differ: the plate's animation bank, the
  region label's cell). Areas 16, 33, 45 are read whole now; the other
  eight copies (65, 87, 88, 104, 115, 121, 151, 152) will be the same.
- **The tools' misses this wave**: `magic_rows._cmp_bound` misses a `cmp
  reg, reg` bound, so `scenario_rows.py` dropped a real function
  (`0x5413D0`, SC2) as a byte table and ran a neighbour's extent over it;
  `area_rows.py` gaps `0x4075D0` / `0x407940` / `0x4077F0` are area 44's,
  armed through a register (AR1B); `0x56FCA0`'s descent stopped at 0x10
  because the entry jumps over its own loop head (SX). Three fixes owed.
- **`pairs_propagated.json` pairs more jump-table cases**: `0x559AD0`
  (SC9b), `0x53DF10` twice (SC2).
- **An `args` hook that writes memory is lost** - the harness captures
  the state before the arguments; a plant that must change memory goes in
  `Seed` (SX). For the harness doc.
- **`BOF3X_SHADOW='*'` died silently once each for two agents** (AR1A,
  AR1C's merge check: exit 127, no Fatal) while a round-nine spell group
  was cloning, and passed on the re-run every time. Not understood; the
  owner's game and several agents' self-tests were running at once. Watch
  for a third.
- **Raw-address callees nobody owns, after SX**: `0x532FD0`, `0x591EC0`,
  `0x57C5A0`, `0x57C600` (a turn test beside `Camera_TurnToDegrees`),
  `0x57C160` (a story-flag toggle), `0x572620`, `0x57C8A0`, `0x469FE0`,
  `0x587860`, `0x587890`, `0x591920`, `0x5A7730`. A second engine group.
- **Inbound calls from engine and area code** for the rebinding pass:
  `0x46D79C` (effect kind 0x70) into `Area49_EffectFrame`; areas 16 and 33
  into `Area45_Record4Tick`; area 7 into `Area46_PlaceKind2At0`; area 77
  into `Scena06_Leap`.
- **The other session's commits ride on this branch** (`368b84f`,
  `f669cce`: DIV-0059..0062, four routes); each left `DIVERGENCE.md`'s
  status count behind, fixed at `8e4fcd7` and `a3c4fde`. The `inject:`
  count is one short of the `impl` count (4,554 against 4,555) since
  before wave two; unexplained, small, owed a look.

**Owed by the round so far:** the defects' numbering for all three waves;
the rebinding pass (every `SH_AT` / `AH_AT` into SE, SX, CALLS and the
chapter blocks; the harness's standard-set columns for `0x4410B0`,
`0x532ED0`, `0x57C6B0`, `0x56FCA0`, `0x56D6F0` move to `SH_OURS` at the
same time - switching one without the other breaks the raw calls, SX);
the three tool fixes; the pairing tool's cases; the route A/Bs when the
owner is away; the recipe saves per chapter.

## 11. Wave four (to stage)

Area only from here: world 1's remainder (areas 53..75, about 155
functions, three groups as `area_rows.py --groups` cuts them at the tip -
its letters shift as areas become ours), world 2's first groups (AR2A
50, AR2B 66 with the world-map route in area 88, AR2C 45, AR2D 53, AR2E
52 with area 104 on the world-map route, AR2F 53), and **SX2**, the twelve
engine callees above. About eight groups, ~400 functions; wave two's
brief with sections 7 and 10 folded in.

## 12. Wave four staged (2026-09-28 morning, from the tip after `1279927`)

Eight groups, one stage each, area only plus the second engine group.
Counts are `tools/area_rows.py --groups` at this tip (its world-1
letters shifted as areas 38..52 became ours: the tool's `AR1A` now spans
areas 38..64 with 47 to take, all past `0x40AB00`; the groups here are
named on from the merged `area_w1a`..`w1c` so the module names stay
distinct). Brief `analysis/round10_wave4_brief.md` (wave three's with
section 10 folded in), group lines `analysis/round10_wave4_groups.tsv`
(gitignored). Entries snapshot
`analysis/calltrace/entries_logic_0928_prewave10_4.txt` (6,182 lines).
AR2E (areas 104..106, 52, area 104 on the world-map route) and AR2F
(108, 110..113, 53) are listed for wave five with world 3's first groups,
to keep this wave near the 350 that fits one usage window.

| Group | What | Band | To take | Module |
|---|---|---|--:|---|
| AR1D | world 1: areas 53, 55..57, 59..64 | `0x40AB00..0x40B8C0` | 47 | `area_w1d` |
| AR1E | world 1: areas 65, 67 (65 is a world-map area, the fourth copy of 16 / 33 / 45) | `0x40B8C0..0x40CEF0` | 49 | `area_w1e` |
| AR1F | world 1: areas 68..69, 71..75 | `0x40CEF0..0x40EB90` | 59 | `area_w1f` |
| AR2A | world 2: areas 76..82, 84 | `0x40EB90..0x40F720` | 50 | `area_w2a` |
| AR2B | world 2: areas 85..88 (87 and 88 are world-map areas; the world-map route enters 88; `WorldMap_PinSprite` and `WorldMap_FrameWait` in the band are ours already) | `0x40F720..0x411F10` | 66 | `area_w2b` |
| AR2C | world 2: areas 90..92, 94 | `0x411F10..0x4135B0` | 45 | `area_w2c` |
| AR2D | world 2: areas 95..100, 103 | `0x4135B0..0x4146C0` | 53 | `area_w2d` |
| SX2 | the engine callees nobody owns after SX (section 10): `0x469FE0`, `0x532FD0`, `0x572620`, `0x57C160`, `0x57C5A0`, `0x57C600`, `0x57C8A0`, `0x587860`, `0x587890`, `0x591920`, `0x591EC0`, `0x5A7730` | twelve addresses | 12 | `scena_sx2` |

381 functions.

## 13. Wave four merged (2026-09-28, morning to early afternoon)

All eight groups merged, one at a time, each merge commit built and
self-tested in the detached verification worktree (`merge_group10v.sh`,
now with `one_grow.py`: see below). **382 functions taken, 4,554 -> 4,936
ours.** Counts are each group's in its worktree. World 1 is complete
(316 of 316, `area_rows.py --groups` at `ad4390d`); world 2 has 105 to
take (areas 104..106, 108, 110..113).

| Group | Merge | Taken | Rounds | Controls planted / refused | Not refused | Doc |
|---|---|--:|--:|---|---|---|
| SX2 | `52dfc7e` (fixed `9a30492`) | 13 of 12 listed (+ `0x57C650`) | 39,000 | 56 / 56 (54 by a count, 2 by a fault, their variants by a count) | | [`scena_sx2.md`](scena_sx2.md) |
| AR1E | `aff2f40` (fixed `8602ae2`) | 49 | 196,000 | 95 / 95 (93 by a count, 2 by a fault, variant by a count) | | [`area_w1e.md`](area_w1e.md) |
| AR2A | `9e57156` | 50 | 300,000 | 145 / 144 | 1 equivalent (a step table with period 4), variant refused | [`area_w2a.md`](area_w2a.md) |
| AR2B | `f9b7a1f` | 66 | 272,000 | 147 / 147 (146 by a count, 1 by a fault, variant by a count) | | [`area_w2b.md`](area_w2b.md) |
| AR1D | `3ee8864` | 47 | 282,000 | 136 / 135 | 1 equivalent (a zero-extension a `<< 16` drops), variants refused | [`area_w1d.md`](area_w1d.md) |
| AR2D | `1f4f25e` | 53 | 318,000 | 255 / 254 | 1 equivalent (a drift table with period 4), variant refused | [`area_w2d.md`](area_w2d.md) |
| AR2C | `d141ff2` | 45 | 270,000 | 176 / 175 | 1 equivalent (a product with the identity, either order), variant refused | [`area_w2c.md`](area_w2c.md) |
| AR1F | `ad4390d` | 59 | 354,000 | 374 / 371 | 3 equivalent (two period-4 step tables; `rand() % 2` against `& 1`), variants refused | [`area_w1f.md`](area_w1f.md) |

**What the wave found:**

- **`DrawPool_Grow` must stay last in `inject_all.cpp`** and wave three's
  SC13 merge had appended its inject after it (an agent noticed, SX2).
  Moved before it at `4c621b1`; but every wave-four branch forked before
  that, so keep-both doubled the `DrawPool_Grow` block on the SX2 and
  AR1E merges and the second call failed loudly (`FATAL: DrawPool: the 4
  bytes at 0x00486EBD are not the ones expected`), both self-tests exit
  3. Fixed by hand twice, then `one_grow.py` in the merge script keeps
  only the last block. A merge script for the next round wants the same
  step from the start.
- **SX2 took thirteen**: `0x57C650` (`Camera_TurnStepFB`, `0x57C5A0`'s
  twin, called only by `0x57C600`) was in no list. None declined:
  `0x587860` (`Sound_StopChannels`) is its own function beside
  `Sound_PauseAll` `0x587C30`. Two tables named: `BattleFormation_Offsets`
  `0x660B1C`, `Formation_SlotVectors` `0x6698B0`.
- **World-map copies four to six read** (65, 87, 88 against 45): the
  same code; the plate bank (`0x53`; `0x156` / `0x157`), the label cell
  (`0x803584`; `0x803580`), and the place hook's name sets (area 65 two
  six-byte sets over five rows, so `0x178` bytes and its last two call
  sites 4 bytes on; area 88 12-byte sets over 11 rows; area 45 three
  five-byte sets over four). `0x40C490` (`Area65_Record8Move`) is the
  record `+8` effect's state 2 shared by all ten world maps' tables;
  `0x40FC40` (`Area87_Init`) is areas 65 and 87's shared init. AR2B's
  ours is one body with a table struct per copy.
- **Capcom's own `jmp` over eleven `nop`s** opens areas 63, 64, 72 and
  73's inits: `CloneOriginal` refuses the entry as already patched, so
  the fuzz clones from the body `0x10` on; `BOF3_INJECT` has no such
  guard (AR1D, AR1F).
- **The harness's `kPhase` callee never runs its `effect`** (the recorder
  returns first): louder stand-ins were silently dead until switched to
  `kGarbage` (AR1F). For the harness doc.
- **`area_rows.py` misses**: it counts 7 entries in the state table at
  `0x614728` where the seventh dword is the handler array (AR2C); it reads
  s16 coordinate pairs in areas 6, 9, 24, 67, 122, 123 as pointers to
  `0x40FDE0` / `0x40FF20` (AR2B); `area_funcs.tsv` listed 43 of AR1E's 49
  (the six are shared handler bodies and a trigger the rows list). An
  agent's `area_rows.py` run rewrote the shared `analysis/area_funcs.tsv`
  with its worktree's symbols once (restored) - the tool writes where it
  reads; a `--no-write` is owed.
- **The brief's `+0x38` note was wrong** (AR1F): area 77's descriptor
  sets it (`0x60ACC8`, the colour matrix), not area 75's; the tool's
  roots line had it right.
- **`0x4139E0` is both `WorldMap_FieldHooks[11]`** (the "no world map"
  entry) and object trigger id 0 (AR2D).
- **Raw-address callees nobody owns, after SX2**: `0x454A80`, `0x455290`
  (release and start of `Field_Slots` scripts, AR2B); `0x4220D0` (AR3F's
  block, called by AR1D and AR2D). And SX2's inbound callers nobody owns:
  `0x4FEEB0`, `0x46BF80`, `0x46C100`, `0x482930`, `0x4703F0`, `0x4712E0`,
  `0x4849A0`, `0x432750`, `0x459720`, `0x464E40`.
- **Inbound calls for the rebinding pass**: SX2's twelve from AR2A (area
  77: `0x469FE0`, `0x57C160`), AR2B (area 86, the same two), AR2D (area
  99: `0x57C160`), and every merged caller in `scena_sx2.md` §3; engine
  `0x478649` into `Area85_ClutShift`; `Area42_TimerTail` (AR1B) into
  `Area75_DrawWindow` `0x40E750` at `0x406A9F` / `0x406CDB`; `Area_StepHook`
  (`event_ops.cpp` `kStepHandlers`) into `Area76_StepDisarmTail5`,
  `Area97_StepHook`, `Area100_StepHook`, `0x40B410`; `0x56E0F5` into
  `Area76_StepDisarmTail5`; the tables of other areas name `0x40C490`,
  `0x40CAB0`, `0x40CAC0`, `0x40CDE0`, `0x40CE10`, `0x40B2D0`, `0x40B2E0`,
  `0x40B4F0`, `0x40B590`, `Area80_ResetCameraShift`,
  `Area94_Counter1FromLeaderPose`, `Area68_ChoiceAnswer84`,
  `Area69_GlideBegin40`, `0x40D790` (read in place, no rebinding).
- **`Area53_Trigger42` answers whatever `ScriptFlags_Set40` left in
  eax**; ours of that is `void`, so the value was undefined before this
  group (AR1D) - a latent read of an undefined answer, for the defects.
- **`BOF3X_SHADOW='*'` did not die silently for any group this wave**
  (eight first runs, exit 0).
- **The `inject:` count stays one short of the `impl` count** (4,936
  against 4,937).
- **The other session staged round eleven** (the boss round,
  [`takeover-queue-bosses.md`](takeover-queue-bosses.md)) on this branch
  during the wave (`44348f7`, `154272f`).

**Owed by the round so far:** the defects' numbering for all four waves;
the rebinding pass (every `SH_AT` / `AH_AT` into SE, SX, SX2, CALLS, the
chapter blocks and the area blocks; the harness standard-set column
moves together with it); the tool fixes (section 10's three, plus the
state-table count, the coordinate-pair false positive and `--no-write`);
the harness doc's `kPhase` / `args` notes; the pairing tool's cases; the
route A/Bs when the owner is away; the recipe saves per chapter.

## 14. Wave five (to stage)

Area only: world 2's remainder (AR2E: areas 104..106, 52, area 104 on the
world-map route; AR2F: 108, 110..113, 53 - the tool's `AR2A` / `AR2B`
rows at `ad4390d`) and world 3's first groups as `area_rows.py --groups`
cuts them (AR3A 56 with world-map area 115 on the route, AR3B 54 with
world-map area 121, AR3C 56, AR3D 46 - area 135 alone, the largest area,
AR3E 52, AR3F 53 with `0x4220D0`, AR3G 56). Eight groups is about 430;
seven (through AR3E) is 369. Wave four's brief with section 13 folded
in; the merge script with `one_grow.py` from the start.

## 15. Wave five staged (2026-09-28 afternoon, from the tip after `62d069f`)

Eight groups, one stage each, area only. Counts are `tools/area_rows.py
--groups` at this tip (its world-2 rows are `AR2A` / `AR2B` now that
areas 76..103 are ours; the groups are named on from the merged
`area_w2a`..`w2d`). Brief `analysis/round10_wave5_brief.md` (wave four's
with section 13 folded in), group lines `analysis/round10_wave5_groups.tsv`
(gitignored). Entries snapshot
`analysis/calltrace/entries_logic_0928_prewave10_5.txt` (6,532 lines).
AR3G (areas 148..151, 56) waits for wave six with world 4.

| Group | What | Band | To take | Module |
|---|---|---|--:|---|
| AR2E | world 2: areas 104..106 (104 is a world-map area on the world-map route) | `0x4146C0..0x4168E0` | 52 | `area_w2e` |
| AR2F | world 2: areas 108, 110..113 | `0x4168E0..0x418BE0` | 53 | `area_w2f` |
| AR3A | world 3: areas 115..119 (115 is a world-map area on the world-map route; `WorldMapHud_Start` `0x419110` is ours already) | `0x418BE0..0x41A9D0` | 56 | `area_w3a` |
| AR3B | world 3: areas 120..121 (121 is a world-map area, 51 functions) | `0x41A9D0..0x41C890` | 54 | `area_w3b` |
| AR3C | world 3: areas 124..125, 127..128, 130..134 | `0x41C890..0x41DAD0` | 56 | `area_w3c` |
| AR3D | world 3: area 135 alone, the largest area | `0x41DAD0..0x41EFE0` | 46 | `area_w3d` |
| AR3E | world 3: areas 136, 139..142 | `0x41EFE0..0x420800` | 52 | `area_w3e` |
| AR3F | world 3: areas 143..146 (`0x4220D0`, called by AR1D and AR2D by raw address, is in this band) | `0x420800..0x4223A0` | 53 | `area_w3f` |

422 functions.
