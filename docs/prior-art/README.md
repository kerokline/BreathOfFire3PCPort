# Prior art

**Status:** STABLE (surveyed 2026-09-18)

Four projects, researched because each has already hit a wall we are walking
toward. These are notes on *what they learned*, written to be useful when we are
stuck — not summaries of what the projects are.

| Note | Why it is here |
|---|---|
| [`openrct2.md`](openrct2.md) | The completed version of our exact architecture (PLAN §2C). Hybrid binary, x86, original executable eventually dropped. |
| [`devilution.md`](devilution.md) | Our archival/living split (PLAN §6), already played out — devilution reconstructed an MSVC-era x86 binary faithfully, DevilutionX forked it into a portable, feature-adding game. |
| [`tr1x.md`](tr1x.md) | Our phase 0. DLL injection into a running x86 Windows game, single-address redirection, and a project whose stated purpose is to *improve* the game. |
| [`diaphora.md`](diaphora.md) | Our phase 1. A mature implementation of cross-binary function matching, with years of accumulated knowledge about which signals are trustworthy. |

Each note states the project's licence and what it permits. The short version is
in §4 below and it is not encouraging.

---

## 1. The finding that repeats: nobody kept an oracle

This was not the question any of the notes was asked. It came back from three of
them anyway.

- **OpenRCT2** built a differential harness (testpaint) and let it rot — "hasn't
  compiled for years, and before that it produced incorrect results for years" —
  while it still forced their structs to stay `#pragma pack`ed. A dead oracle
  that constrains your code is worse than no oracle.
- **DevilutionX**'s change record is a player-facing wiki page that does not
  distinguish a deliberate change from a self-inflicted regression. The
  catalogue of what the original actually did lives in the *archival* repo, not
  theirs.
- **TR1X** did not verify equivalence at all — no byte-matching, no differ,
  just compile-run-playtest-review. The cost is legible: **262 + 182 changelog
  entries** tagged "regression from \<version\>".

Three living forks of a shipped game; three projects that lost the ability to
tell *we changed this* from *we broke this*. This is exactly the failure mode
[`DIVERGENCE.md`](../DIVERGENCE.md) was opened on day one to prevent, and
`CLAUDE.md` rule 2 is the rule that prevents it.

**We are better placed than any of them**, because a provably faithful
implementation of this game sits in the next directory. DevilutionX's open issue
crowdsourcing save games from humans playing vanilla Diablo — to build a test
corpus by hand — is the concrete price of not having that. We can generate ours
on demand.

The corollary is a warning, from OpenRCT2: **an oracle you do not run is not an
oracle.** Whatever differential harness phase 2/3 builds has to be in CI from
its first day or it will rot exactly the same way.

## 2. Corrections to our plan

**The cutover is early; the tail is long.** OpenRCT2 announced "RCT2.EXE is no
longer required" in Dec 2016 and deleted its last interop files in **March
2025**. OpenLoco merged its final "Remove interop" PR in **Nov 2025**, after
~103 interop PRs. PLAN §2C treats dropping the original binary as the endpoint;
it is closer to the midpoint.

**Native is not 64-bit.** TRX completed its hybrid transition and is *still*
`i686-w64-mingw32` on Windows today. PLAN phase 4 treats "drop the original
binary" and "reach portable 64-bit" as near-simultaneous. They are two projects.

**The platform layer went last, twice.** TR1 (~10 months) and TR2 (~8 months)
both finished with the platform layer as a final lump, not a taper. PLAN phase 3
orders platform work *first*, to buy portability fastest. That may still be
right for us — our platform surface is 101 imports across 7 DLLs, which is
unusually small — but it is the opposite of what two completed projects did, and
the reason deserves to be understood rather than assumed.

**Formats gate features.** Six years of OpenRCT2 feature requests — 255 trains,
999×999 maps — all closed on one day in 2021, when their own `.park` format
replaced the original's SV6. *After* the code was theirs. Supports PLAN §8 step 3
being critical path, and argues for pairing the `DAT/` reader with our own
container format rather than living inside Capcom's shape.

## 3. Techniques worth adopting

**Bind each function name once.** TR1X binds a name in a header to *either* an
address-cast macro *or* a real prototype. Decompiling a function therefore needs
zero edits to any caller. OpenRCT2's `RCT2_ADDRESS`/`RCT2_GLOBAL` macros converge
on the same shape from the other direction. Design this into phase 0; retrofitting
it means touching every call site twice.

**A four-state progress flag.** TR1X tracks `-` todo / `*` todo-but-we-already-
call-it / `x` unused / `+` done, in one plain text file grouped by *original
source file*, with generators emitting headers, an importer script and a treemap.
The `*` state is the interesting one: it separates "unknown" from "we depend on
this". Worth adding to `symbols.toml` alongside the evidence tier, which answers
a different question.

**Verifier-as-generator.** OpenRCT2's testpaint compared *structured call traces*
— paint-call sequence, support heights, tunnel geometry — not rendered pixels.
Then they inverted it and had the checker emit the C++ it was checking. Diffing
intent rather than output is the right shape for a project that intends to
diverge. (Their reported "80% → 99.999% in one day" is subsystem-scoped; the
technique is the point, not the number.)

**Equivalence with declared tolerances.** devilution's "binary exact" explicitly
permits differing jump targets into other functions, global addresses, and switch
trailing code — equivalence *modulo relocation*. That definition is what makes
per-function progress possible at all. Their comparer emits two `.asm` files for
an ordinary diff and **never produces a verdict**; a human reads it.

**A free A/B switch.** TR1X's detour carries an `enable` flag that swaps the
patch direction, giving per-function fallback to original behaviour for nothing.
That is a regression-testing primitive, and phase 0 should have it.

**Era presets, not just toggles.** TR1X gates each impactful change behind
`fix_*`/`enable_*`/`*_mode`, then ships **presets that restore an era wholesale**
(`tr1-pc`, `tr1-ps1`). [`DIVERGENCE.md`](../DIVERGENCE.md)'s entry format has a
"Reversible?" field; a preset concept would make it far more useful to a player
and to us.

**Mark the original's bugs inline.** OpenRCT2 tags `(original bug)` in its
changelog; TR1X carries `(OG bug)` and `(regression from X)` plus the gating
setting path; devilution uses `// BUGFIX:` vs `// TODO:` in source. All three are
cheap. None of them records, in a queryable form, *what the original actually
did* — which is the half our ledger adds.

## 4. Licences: only one thing here is vendorable

| Project | Licence | For us |
|---|---|---|
| OpenRCT2 | GPL-3.0-or-later | **Read only.** Not merely unwise — PolyForm Noncommercial's field-of-use restriction is an "additional restriction" GPLv3 §7 forbids, so there is no notice-retention workaround of the kind MIT would allow. |
| TRX / TR1X | GPL-3.0 | **Read only**, same reasoning. Explicitly including line-by-line translation. |
| Diaphora | AGPL-3.0 | **Read only.** (Running it would have been fine; the blocker is that export needs IDA, which we do not have.) |
| devilution / DevilutionX | Sustainable Use License v1.0 (not OSI) | Non-commercial, so no conflict of *purpose* with PolyForm NC — but its own notice and modified-copy-marking duties. Vendoring means SUL text retained, "modified" notice, and a `THIRD_PARTY.md` entry. |
| OpenLoco | MIT | **The only vendorable reference in the survey.** |

Techniques are not copyrightable. Every note in this directory was written to be
implementable from its description, without reference to the original source —
which is the same posture `CLAUDE.md` rule 5 already sets for `bof3ext`.

## 5. What this says about phase 1

[`diaphora.md`](diaphora.md) settles a question we were about to spend time on.
Diaphora's own 3.0 release notes say **"Do not run assembly based heuristics when
diffing different CPU architectures"** — its entire highest-quality tier is
switched off in precisely our scenario. So it is not a candidate tool for us; it
is a *pre-tiered specification* of which signals survive a change of ISA, written
by someone who learned it the hard way.

What survives, and it corroborates our own probes: **constants** (independently
the same idea as our value-sequence anchor), **switch structures** (our 23-arm
`MsgBox_Step` match), and **graph/MD-index** measures. What dies: anything
touching instruction encoding, plus names, strings and imports.

Two traps it flags for us specifically:

- **Rarity is computed per database.** Our 2,952-vs-30,062 function asymmetry
  makes "rare" heuristics silently under-fire. Any rarity measure we build must
  be normalised across both sides, not within one.
- **Every demoted heuristic in Diaphora is a good signal applied without a size
  or complexity floor.** The same signal is rated "good" with a floor and
  "unreliable" without. Our delta propagation needs floors before it scales.

And one independent argument for BSim that was not on our list: Diaphora and
BinDiff both assume a **two-program diff**. Our shape is one PC image against a
*corpus* of PSX overlays — 1-vs-N, which is BSim's native shape.

**BSim was then measured** ([`../bsim-evaluation.md`](../bsim-evaluation.md)),
and Diaphora's warning transferred to it intact: BSim's two worst results were a
tiny wrapper whose true match was *out-ranked by a false one*, and a very large
function whose single wrong answer carried the highest significance score in the
run. Good signals without a size floor, exactly as documented. The floor has to
come from us.

## 6. A third route to the translation units

[`SHARED_SOURCE.md`](../SHARED_SOURCE.md) §2 argues the source's file
decomposition survived into both binaries, recoverable by clustering globals into
blocks. Two independent confirmations arrived the same day:

- **Diaphora 3.0 recovers translation-unit boundaries from code**, via CodeCut's
  Local Function Affinity, and diffs *within* them to suppress false positives.
  That is the same decomposition reached from the code side rather than the data
  side — two partitions of one source tree that can cross-check each other.
- **The Rich header gives a count.** `BOF3.exe`'s is intact:
  `python tools/pe_rich.py` reports 1,009 object contributions, of which 110 are
  import thunks and 16 unmarked — so roughly **883 real object files** went into
  the link. If clustering yields wildly more or fewer than that, the clustering
  is wrong.

A cheap untested prediction also falls out of Diaphora's gap-filling: if per-TU
*code* stayed contiguous the way per-TU *statics* demonstrably did, then
address-order interpolation between confirmed pairs should work. Worth one
experiment in phase 1.
