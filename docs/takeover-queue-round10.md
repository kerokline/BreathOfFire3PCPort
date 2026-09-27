# The tenth round's queue: the scenario banks and the area overlays, wave by wave

**Status:** IN PROGRESS (2026-09-27) - wave one staged; nothing merged yet

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
