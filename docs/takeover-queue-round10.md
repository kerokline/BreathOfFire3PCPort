# The tenth round's queue: the scenario banks and the area overlays, wave by wave

**Status:** IN PROGRESS (2026-09-27) - wave one: SCH, ART, ARH merged (3,533 ours); SE, SC1, SC3, SC11, SC12 in stage B

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
