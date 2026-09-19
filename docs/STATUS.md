# Status

**Status:** IN PROGRESS (2026-09-19)

Where the project actually is, what is in flight, and what is blocked.
[`PLAN.md`](PLAN.md) says what we intend to do and why; this file says what is
true today. When they disagree, this one is right and `PLAN.md` needs updating.

## Where we are

**Phase 0 is done: our code runs inside the game.** A launcher starts the
player's own `BOF3.exe` suspended and loads `bof3x.dll` into it; a five-byte
detour hands one original function at a time to a reimplementation; and
`symbols.toml` generates the header that makes taking over a function a
four-line change with no edits to its callers
([`SCAFFOLDING.md`](SCAFFOLDING.md)). The exit test passed 2026-09-19 with
`File_Read` `0x5A7470`, under llvm-mingw, in both directions of the A/B switch.
**Three functions of ~2,952 are ours** — `File_Read` and `File_Open`,
faithful, and
`Save_WriteFile`, which carries the project's first *code* divergence: a fix
for saves vanishing from the save menu ([`DIVERGENCE.md`](DIVERGENCE.md)
DIV-0002, [`save-files.md`](save-files.md)), found, traced, fixed and verified
in game on 2026-09-19.

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
- **The asset-loading path is partly read** ([`asset-loading-path.md`](asset-loading-path.md)):
  a 16-slot `FILE*` file layer at `0x5A7370`..`0x5A75F0`, and `LoadDatFile`'s
  four chunk kinds. Kind-0 data lands in **one arena at `0x803580 + tag`** — the
  port's repacked image of PSX RAM from `0x80010000`, which closes
  `DAT_CONTAINER.md`'s open question.
- **There is a regression oracle.** The port is deterministic from launch
  through its attract sequence — identical `Rand` call count, message index and
  area word at every one of 7,478 frames across fresh launches — because the
  seed is fixed at 1, the binary has no `srand`, game logic cannot reach a
  clock, and all of it runs on one thread inside a four-coroutine task system.
  Original-vs-ours already compares identical
  ([`attract-mode.md`](attract-mode.md)). Reach is two field scenes; no battle
  or menu yet.
- 35 functions, 8 global blocks and 11 data items named in
  [`symbols.toml`](../symbols.toml), tiered; 19 functions carry signatures and
  are callable from our code, 3 of them ours (counted 2026-09-19 from the
  file, not from memory).
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
- **There is an attract sequence — answered by the owner, 2026-09-19.** Left
  idle, the start screen plays through several areas with story text on screen.
  It is **not FMV**: on the PSX it ran through the overlays and the ordinary
  area code, so it is the real engine being driven, which is exactly the
  property that made TR1X's demos a determinism oracle
  ([`prior-art/tr1x.md`](prior-art/tr1x.md) §2.5). It is "not a full attract
  mode" — no recorded gameplay input is known — so what it can police is area
  load, scripting, text and rendering, not battle. **Not yet established:** what
  drives it (a script, a timer table, recorded input), whether it is
  deterministic run to run, and whether it touches `Rand`. That is
  [`IDEAS.md`](IDEAS.md) I6.

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

### 3. Phase 0 proper — **done 2026-09-19**

Launcher, detour layer, generated symbol header, one toolchain. Described in
[`SCAFFOLDING.md`](SCAFFOLDING.md); exit test in its §5. Numbered 3 for history.

What phase 0 deliberately did not build: register-argument thunks for
non-standard MSVC6 conventions (wait for the first real case), a progress
report, the Ghidra round-trip (phase 1), and any CI — there is still no
workflow that compiles `src/`, and the receipt policy below now has something
to bite on.

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
  ([`prior-art/README.md`](prior-art/README.md) §1). **As of 2026-09-19 `src/`
  exists**, so the "fail" half of this policy is no longer hypothetical, and the
  receipt below is the next piece of infrastructure owed.

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
