# Status

**Status:** IN PROGRESS (2026-09-18)

Where the project actually is, what is in flight, and what is blocked.
[`PLAN.md`](PLAN.md) says what we intend to do and why; this file says what is
true today. When they disagree, this one is right and `PLAN.md` needs updating.

## Where we are

**Scoping, with the load-bearing experiment run and passed.** No game code yet.

What is established:

- The two binaries are **compilations of one C source tree**, and the source's
  *file decomposition* survived into both ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)).
- **Name transfer works**, by two independent techniques — global block deltas
  and constant-table value search — on both a well-documented subsystem and a
  cold one ([`kinship-probe-text-engine.md`](kinship-probe-text-engine.md),
  [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md)).
- **Ghidra BSim works cross-ISA** and is a seed generator rather than an oracle
  ([`bsim-evaluation.md`](bsim-evaluation.md)).
- 12 functions, 7 global blocks and 4 data tables named in
  [`symbols.toml`](../symbols.toml), tiered.
- Four comparable projects surveyed for what they learned the hard way
  ([`prior-art/`](prior-art/)).

## The immediate order of work

### 0. Verify launch and stability

Nothing else is real until the game runs here and we know what "working" looks
like. Establish a baseline: does it launch, what does it do on this machine,
where are the known defects (the fullscreen fallback, resolution handling), and
**is there an attract/demo mode**. That last one matters out of proportion to its
size — TR1X used demo playback as a determinism oracle, and if BoF3 has one it is
the cheapest regression harness available to us
([`prior-art/tr1x.md`](prior-art/tr1x.md)).

### 1. PSX ↔ PC save file interchange — **priority 1 after step 0**

Convert a save between the PlayStation release and the PC port, in at least one
direction.

Chosen deliberately as the first real piece of work, for four reasons:

- **It is a visible win.** [`PLAN.md`](PLAN.md) §6 names the risk that phases 0–2
  deliver nothing a player can see and the project stalls on enthusiasm alone.
  This is something a person can actually use, early.
- **It tests the central finding on real data.** The persistent character record
  is 164 bytes in *both* binaries, and block-delta propagation says the fields
  should line up. A save converter either works or it exposes exactly where the
  model is wrong — on data, where errors are cheap and visible, rather than in
  reimplemented code where they are neither.
- **The sibling already did half of it** — `SAVE_IMPORT.md`, `tools/save_tool.py`,
  `tools/save_import.py`. Reference, not vendored.
- **It needs the `DAT/` container work anyway** ([`PLAN.md`](PLAN.md) §8 step 3),
  which is already critical path, and OpenRCT2's experience says formats gate
  everything ([`prior-art/README.md`](prior-art/README.md) §2).

### 2. The overlay corpus

**Demoted 2026-09-18, and worth less than believed.** Investigated in
[`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md):

- The corpus is **121 named overlay functions across 8 of 406 overlays**, not
  the 29,036 repeated across five documents. That figure had no artifact behind
  it. What the overlays really offer is **5,805 statically discovered function
  entries** — structure, not names — and a human role for 216 of 406 overlays.
- Indexing them is **actively risky**, not merely pending: it grows the BSim
  candidate pool ~17x and, done the way the sibling's tooling seeds the boot EXE
  into every overlay program, would flatten BSim's rarity scoring and silently
  degrade the boot-EXE ranks already published.
- The images are already extracted and complete on disk (406/406 verified), and
  the machine time is about an hour. Cheap to do, easy to do wrong.

This is a **PSX-side problem only**: overlays are a 2 MB-RAM technique, and the
PC port is one flat image with every function resident. It matters because the
name corpus is keyed by overlay and a bare PSX address is ambiguous without
knowing which was loaded.

Consequence: this drops below save interchange and phase 0. When it is done, it
must be on a database copy with the nine BSim pairs re-scored before and after.

### 3. Phase 0 proper

Loader, detour layer, build system. See [`PLAN.md`](PLAN.md) phase 0, now
informed by [`prior-art/tr1x.md`](prior-art/tr1x.md) (launcher-based injection,
bind each name once, an `enable` flag for free A/B) and
[`prior-art/openrct2.md`](prior-art/openrct2.md) (the interop layer in both
directions).

## Open decisions

- **How to CI an oracle that needs undistributable game data.** The prior-art
  survey's loudest lesson is that an oracle you do not run is not an oracle, so
  the differential harness has to be automated from day one — but CI cannot hold
  `BOF3.exe`, `DAT/` or the PSX disc. Being designed now rather than retrofitted.
- **Size floors for the matcher.** [`bsim-evaluation.md`](bsim-evaluation.md)
  shows tiny wrappers and very large functions are unreliable; nine pairs is too
  few to fit a cutoff.
- **Phase 3 subsystem order.** `PLAN.md` puts the platform layer first for
  portability; two completed projects removed it last
  ([`prior-art/README.md`](prior-art/README.md) §2).

## Outstanding obligations

- **Write findings back to the archival sibling.** We answered its open `0x0C`
  question from the PC side and established that `Rand` does not match between
  the binaries — which constrains *its* differential-testing plans too. The
  cross-reference relationship is reciprocal and we have taken without giving.
- **Contact TheRealBiggs** ([`PLAN.md`](PLAN.md) §8 step 1). Worth doing now
  specifically because there is finally something to offer rather than only
  questions.

## Known gaps in the record

- **No CLA**, and the DCO does not substitute for one
  ([`LICENSING.md`](LICENSING.md) §7).
- **No legal review** of the provenance, which `LICENSING.md` §5 says should
  happen before any commitment to a commercial timeline.
- **The exact MSVC product versions** behind `BOF3.exe`'s Rich header are
  unidentified — the public build-number tables consulted did not cover them
  ([`SHARED_SOURCE.md`](SHARED_SOURCE.md) §2).
- **`MsgBox_Step` returning a single BSim candidate** is unexplained and matters
  before relying on BSim for large battle functions.
