# Status

**Status:** IN PROGRESS (2026-09-19)

Where the project actually is, what is in flight, and what is blocked.
[`PLAN.md`](PLAN.md) says what we intend to do and why; this file says what is
true today. When they disagree, this one is right and `PLAN.md` needs updating.

## Where we are

**Scoping is over: the load-bearing experiment passed, the game runs on this
machine, and phase 0 is the next thing to build.** No game code yet.

What is established:

- The two binaries are **compilations of one C source tree**, and the source's
  *file decomposition* survived into both ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)).
- **Name transfer works**, by two independent techniques — global block deltas
  and constant-table value search — on both a well-documented subsystem and a
  cold one ([`kinship-probe-text-engine.md`](kinship-probe-text-engine.md),
  [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md)).
- **Ghidra BSim works cross-ISA** and is a seed generator rather than an oracle
  ([`bsim-evaluation.md`](bsim-evaluation.md)).
- **The `DAT/` container is parsed** — 742 of 742 files, unencrypted, and most
  non-audio sections are byte-identical to the JP disc's `.EMI`s (2,120 of
  2,680, full census; the 560 differences reduce to a handful of causes) ([`DAT_CONTAINER.md`](DAT_CONTAINER.md), `tools/dat.py`). Done
  ahead of step 0 at the owner's direction, 2026-09-19.
- **The media stack is surveyed, and the first divergence has shipped.**
  `BOF3.exe` is DirectDraw + `IDirect3D3`, DirectSound 1, DirectInput 3, MCI/VFW
  for FMV, and a statically-linked MP3 decoder. Exactly one thing was broken —
  `capcom.avi` is Indeo 5, which Windows has not decoded since XP — and it is
  now fixed by re-encoding to Cinepak, the ledger's first entry
  ([`media-stack-survey.md`](media-stack-survey.md),
  [`DIVERGENCE.md`](DIVERGENCE.md) DIV-0001). Audio needs nothing. The FMV path
  is fully read ([`replacing-mci.md`](replacing-mci.md)); the DirectDraw
  presentation layer is the long-term liability, is **not** read yet, and is
  [`IDEAS.md`](IDEAS.md) I8 against phase 3.
- 15 functions, 7 global blocks and 5 data tables named in
  [`symbols.toml`](../symbols.toml), tiered.
- Four comparable projects surveyed for what they learned the hard way
  ([`prior-art/`](prior-art/)).

## The immediate order of work

### 0. Verify launch and stability — **passed 2026-09-19, enough to proceed**

Owner-tested on this machine, 2026-09-19: the launcher starts the game, the
intro FMVs play, the start screen works, and play continues **into the first
area**. That is the gate this step existed for — the game runs here — so
**phase 0 proper is unblocked** and is now the next piece of building work.

It also settles what was an open worry: the legacy DirectDraw display path,
including the exclusive-fullscreen `SetDisplayMode(640, 480, 16)` that FMV
performs before the title screen, works on Windows 11 today. Replacing it is
[`IDEAS.md`](IDEAS.md) I8, deliberate phase-3 work, not an emergency.

What this test did **not** cover, carried forward rather than blocking:

- No written baseline of what "working" looks like beyond the first area — no
  battle, menu, save/load or long-session stability check.
- The known defects (fullscreen fallback, resolution handling) have not been
  reproduced and recorded.
- **Whether there is an attract/demo mode** is still unanswered. It matters out
  of proportion to its size: TR1X used demo playback as a determinism oracle,
  and if BoF3 has one it is the cheapest regression harness available to us
  ([`prior-art/tr1x.md`](prior-art/tr1x.md)). Check it the next time the game is
  left idle on the title screen.

### 1. Read the exe, starting with asset loading

Set 2026-09-19. The `DAT/` work ([`DAT_CONTAINER.md`](DAT_CONTAINER.md)) left
concrete anchors — `LoadDatFile` `0x454590`, the filename table, the `SND\`/`BGM\`
name strings — in a subsystem whose inputs and outputs we can now parse and
diff, which also makes it the natural first replacement target for phase 0.
Steps are in [`HANDOFF.md`](HANDOFF.md). Overlaps phase 0 and does not gate it.

**Save file interchange was priority 1 here until 2026-09-19.** It is now
[`IDEAS.md`](IDEAS.md) I1: still the first visible win to go for, but only once
the game is up and running, since a converted save cannot be verified without
loading it. Step 0 has now cleared that gate; it is available to pick up
whenever a visible win is wanted.

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

Consequence: this drops below exe reading and phase 0. When it is done, it
must be on a database copy with the nine BSim pairs re-scored before and after.

### 3. Phase 0 proper — **unblocked, next**

Unblocked by step 0 on 2026-09-19. Numbered 3 for history; in practice it runs
now, alongside step 1, with a function from the asset-loading path as the
exit-test target.

Loader, detour layer, build system. See [`PLAN.md`](PLAN.md) phase 0, now
informed by [`prior-art/tr1x.md`](prior-art/tr1x.md) (launcher-based injection,
bind each name once, an `enable` flag for free A/B) and
[`prior-art/openrct2.md`](prior-art/openrct2.md) (the interop layer in both
directions).

## A stated goal worth recording now

**Selectable localisations, built from the original releases' own assets.**
Japanese, English, German, French and Chinese all shipped officially. This is a
phase-5 target, not near-term work, but two findings make it worth writing down
while the groundwork is being laid:

- **There is no machinery to inherit.** Each PlayStation language was a separate
  compiled build on its own SKU — no language subdirectories, no language
  strings in any executable, and the regional executables are not
  address-compatible with each other
  ([`regional-builds.md`](../../BreathOfFire3Recomp/docs/regional-builds.md)).
  Language switching is something this project would *build*, and it is
  therefore a divergence in the ledger sense, not a port of existing behaviour.
- **The asset side is enumerable and already surveyed.** Exactly **37 image
  sections** carry language, and 36 of 37 hash as four distinct images across
  the five releases — JP, US+EU sharing, France, Germany. The glyph atlas is a
  32 KB *texture* (not code), duplicated into every module that draws menus;
  plus the ending/kanji font and ~11 areas with text baked into scenery art.
  Every one of those sections is readable from a donor disc.

The design constraint that follows: **the player supplies the discs, the engine
reads the sections.** Shipping extracted glyph atlases would be distributing
Capcom's assets and would break the engine/data split that
[`LICENSING.md`](LICENSING.md) §3 says is not negotiable. Same model as
DevilutionX and OpenRCT2.

[`fixtures.toml`](../fixtures.toml) already carries all five PSX SKUs for this
reason — four as eventual localisation donors, Europe/English catalogued only so
an unrecognised disc can be named rather than guessed at.

## Open decisions

- **How to CI an oracle that needs undistributable game data.** *Decided
  2026-09-18, partially.* Contributors are expected to supply their own copies
  — having both sides locally for side-by-side comparison is the workflow
  anyway, so validation is a formalisation of it rather than new infrastructure.
  The shared contract is hashes, not artifacts: [`fixtures.toml`](../fixtures.toml)
  catalogues known builds and `tools/verify_fixtures.py` checks a local install
  against it, so two people can confirm like-for-like without anyone hosting a
  runner. That also removes the self-hosted-runner question and its fork-PR
  attack surface entirely.

  **Staleness policy, decided 2026-09-18: warn always, fail when the diff
  touches `src/`.** A documentation or tooling change should not be blocked on a
  differential run it cannot affect; a change to the game code should not merge
  on an assertion nobody re-checked. The warning is the part that matters — it
  is what stops the harness dying quietly the way OpenRCT2's did
  ([`prior-art/README.md`](prior-art/README.md) §1).

  **Receipt shape** (to build when there is something to verify, not before):
  a committed file recording the git SHA it ran against, the date, the
  `fixtures.toml` build ids on both sides, what was covered, the result, and the
  hashes of the vectors used. CI validates structure and freshness only — it
  never needs a byte of game data. Deliberately *not* built yet: there is no
  `src/` and no harness, and speculative verification infrastructure is exactly
  what rots.
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
