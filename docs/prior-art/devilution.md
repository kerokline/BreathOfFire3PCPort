# Prior art: devilution and DevilutionX

**Status:** DRAFT (researched 2026-09-18)

Sources are linked inline. Everything here is from public repos, wikis, issue
threads and changelogs; nothing is from memory. Section 6 lists what I could not
confirm.

---

## 1. What it is, and how its shape compares to ours

[**Devilution**](https://github.com/diasurgical/devilution) is a
reverse-engineered reconstruction of the Diablo 1.09b source code: hand-written C
(in `.cpp` files) that, compiled with the *original* Microsoft toolchain,
produces functions byte-identical to Blizzard North's shipped `Diablo.exe`. Its
stated purpose is documentation and preservation: *"In
order to ensure that everything is preserved, Devilution keeps everything as it
was originally designed. This goes as far as bugs and badly written code in the
original game"*
([README](https://github.com/diasurgical/devilution/blob/master/README.md)).
[**DevilutionX**](https://github.com/diasurgical/devilutionX) forked that source
four months later and spent seven years turning it into a portable, SDL-based,
64-bit, twenty-platform game with widescreen, gamepads, translations, a mod API
and hundreds of deliberate gameplay changes.

Against our shape:

| | devilution / DevilutionX | This repo + `BreathOfFire3Recomp` |
|---|---|---|
| Archival artifact | Reconstructed **C source** that recompiles to the original bytes | Lifted **MIPS C** that runs the original PSX program |
| Living artifact | A fork of that source, same language, same repo lineage | A separate target binary (the PC port), reached by incremental decompilation |
| Relationship | Fork: living tracked archival by version, then stopped | Peers: no shared code, only shared *findings* |
| Archival invariant | "compiles to the same instructions" | "behaves like the hardware" |
| Living invariant | "plays like Diablo unless we decided otherwise" | "matches intent, deliberately chosen" |

Two structural differences cut in opposite directions. **In our favour:** their
name corpus came from a `.SYM` file accidentally left on the Japanese PlayStation
disc — a windfall. Ours comes from the kinship probe, which we manufactured.
**Against us:** devilution's archival artifact *is source in the same language as
the living fork*, so forking was a `git fork`. Our archival sibling is lifted
MIPS C for a different CPU. We can never fork it, only compare against it at
runtime — so the parts of their story about code flowing between the two repos do
not transfer, and the parts about *facts* flowing between them do.

---

## 2. What we can steal

### 2.1 Their symbol windfall is our kinship probe — and note what they did with it

Devilution's foundation was a `.SYM` debug file accidentally shipped on the
Japanese PlayStation release of Diablo, containing function names, addresses,
line numbers, source *file paths* and register-allocated local variable names
([BACKGROUND.md](https://github.com/diasurgical/devilutionX/blob/master/docs/BACKGROUND.md),
[CONTRIBUTING.md](https://github.com/diasurgical/devilution/blob/master/docs/CONTRIBUTING.md)).
A worked example in their contributing guide shows a PSX symbol entry carrying
`file = C:\diabpsx\SOURCE\DRLG_L1.CPP` alongside the PC address `0x40AE79` for
the same function — precisely the artifact PLAN §3 says we are synthesising by
matching. Their guide confirms what it is worth: they used the PSX file paths to
**recover the original file decomposition**, then split IDA's single dumped C
file into `.cpp` files along those boundaries (changelog,
[20 April 2018](https://github.com/diasurgical/devilution/blob/master/docs/CHANGELOG.md):
*"Split code from IDA's C file into separate CPP files"*). PLAN §3's proposal to
recover Capcom's file decomposition from global-block clustering is the same move,
and it is validated prior art rather than speculation.

Second-order lesson: they maintained **two naming schemes on purpose.** Devilution
used the PSX symbol names verbatim; the [sanctuary/notes](https://github.com/sanctuary/notes)
project used consistent file-prefixed names (`drlg_l1_load_dun`) and recorded
*both* in a comment header per function — PC address, PSX address, PSX mangled
signature. Their contributing guide reproduces one. That is a `symbols.toml` row
expressed as a docblock, and it is the shape our `psx` field already has.

### 2.2 "Binary identical" was a *milestone with a percentage*, and it took 18 months

This is the most useful number in the whole body of prior art, because it is a
calibrated estimate of the cost of the archival half of this kind of work. From
devilution's [CHANGELOG](https://github.com/diasurgical/devilution/blob/master/docs/CHANGELOG.md):

| Date | Milestone |
|---|---|
| 2018-01-15 | project conceived |
| 2018-03-13 | IDA database dumped to C |
| 2018-06-06 | public release 0.1.0 — playable, not matching |
| 2018-10-01 | compiler version confirmed via the Rich header |
| 2019-01-14 | **50%** of functions binary identical |
| 2019-03-19 | **60%**, and the built file is 968 bytes (0.13%) larger than the original |
| 2019-04-09 | "The last of the compiler flags are figured out"; **70%** |
| 2019-04-12 | **80%** |
| 2019-05-02 | **90%** |
| 2019-05-19 | **96%** |
| 2019-06-21 | **100%** |

Note the shape: 0→50% took eleven months, 50→100% took five, and the
compiler-flag work landed at the inflection point. Note also that a *playable*
build existed three months in, eighteen months before a *matching* one — the
ordering PLAN §5's phases already assume. The progress metric is worth copying: a
GitHub **milestone** whose open issues are the not-yet-matching functions, so
"percent done" is a query rather than a claim, with whole-file size delta as a
coarse second signal.

### 2.3 What "matching" meant in practice — with explicit tolerances

The [Cleaning Code](https://github.com/diasurgical/devilution/wiki/Cleaning-Code)
wiki page defines it operationally. A function counts as "bin exact" when the
disassembly matches **except** that jump targets into other functions may differ,
global memory addresses may differ within expected ranges, and switch statements
may have harmless trailing code. Commits are titled `FUNCTION_NAME bin exact`,
one branch per function.

Those three carve-outs are the whole trick: a strict byte compare is unusable
while the rest of the binary is still moving, because every address shifts when
any function's length changes. Defining equivalence *modulo relocation* is what
makes per-function progress possible before global progress. If we build an
equivalence checker for phase 3 (PLAN §5, "each replaced function gets an
equivalence test"), this is the specification to start from.

### 2.4 The tool: `devilution-comparer`

[devilution-comparer](https://github.com/diasurgical/devilution-comparer) is a
small Rust program. Given the original `.exe`, their freshly built `.exe`, and a
function name, it:

- reads the function's offset and size from **their** build's PDB (via Microsoft's
  `cvdump.exe`, because the `pdb` crate could not read pre-VC7 PDBs);
- looks up the original's offset in a checked-in `comparer-config.toml` mapping
  name → offset (+ optional size);
- disassembles both with Zydis **at the same base address**, so relative jumps
  line up textually;
- writes `orig.asm` and `compare.asm` for an ordinary `diff`;
- `-w` watches the PDB and regenerates on every rebuild.

Three design decisions to lift wholesale. **Normalise, don't ignore** — both
sides are disassembled at the same base so relative displacements print
identically; rebasing is cheaper than teaching a differ about relocation.
**Noise switches carry a warning** — `--no-mem-disp` (hide memory displacements
and indirect calls) and `--no-imms` are both documented as "use with caution",
and the contributing guide repeats that `--no-mem-disp` "can also hide valuable
details", i.e. a wrong stack variable or a wrong global. **The output is two text
files**, not a verdict or a similarity score: the tool does the alignment, the
human does the judgement.

For us the PDB half is free (we build our own reimplementations), the config-file
half is `symbols.toml`, and the disassembler half is capstone, which
`tools/pe_disasm.py` already wraps.

### 2.5 MSVC-era codegen: what actually bit them

Specifics, all from their own record:

- **The Rich header identified the toolchain exactly.** They dumped it and read
  off `VC++ 6.0 SP3–SP6 link 6.00.8447`, `VS97 (5.0) SP3 link 5.10.7303`, and
  `VC++ 6.0 SP5 Processor Pack`
  ([BACKGROUND.md](https://github.com/diasurgical/devilutionX/blob/master/docs/BACKGROUND.md),
  which cites [bytepointer's Rich header article](https://bytepointer.com/articles/the_microsoft_rich_header.htm)).
  **Do this on `BOF3.exe` in phase 1.** PLAN §1 currently records "MSVC 6.0" from
  the linker version; the Rich header will give service pack, Processor Pack
  presence, and the count of objects contributed by each tool. It is a ten-minute
  measurement that constrains every later codegen question.
- **Diablo was built with two different toolchains at once** — compiled with VC6,
  linked with the VC5 linker. Their build instructions say you must install VC5
  SP3 as well and link manually via a makefile because *"you cannot use the old
  linker right out of VC6"*. Expect the same kind of ugliness; do not assume one
  `cl.exe` version explains the whole image.
- **The VC6 Processor Pack is load-bearing for codegen**, flagged in their README
  as "**important for proper code generation!**" — i.e. the same nominal compiler
  version emits different instruction selection with and without it.
- **`/O1`, not `/O2`.** Deduced from inter-function padding — `/O2` left 11-byte
  `nop` runs between functions, `/O1` did not — plus instruction-selection
  differences such as `test` being emitted where the other level emitted `cmp`
  ([issue #111](https://github.com/diasurgical/devilution/issues/111)). The
  general technique is worth internalising: *optimisation level is inferable from
  alignment padding and from idiom choice, before you have matched a single
  function.*
- **C vs C++ was a real, reversed decision.** Devilution 0.5.0 records
  *"Code ported to C (can still be compiled as C++)"*
  ([PR #528](https://github.com/diasurgical/devilution/pull/528)); they reverted
  on 2019-04-15: *"Code is once again compiled as C++ as some parts
  appear to require despite the indications in Rich header."* The Rich header said
  C; the codegen said C++. Measurement beat metadata.
- **`/Og` global optimisation was not uniform across the image.** As recently as
  2024 they landed *"Remove global optimizations for `SHA1ProcessMessageBlock()`"*
  ([commit log](https://github.com/diasurgical/devilution/commits/master)) — a
  per-function flag override, five years after reaching 100%. Per-file and even
  per-function `#pragma optimize` in the original build is normal for this era.

### 2.6 The in-source ledger: `// BUGFIX:` vs `// TODO:`

Their [code style guide](https://github.com/diasurgical/devilution/wiki/Code-Style)
mandates: *mark Diablo bugs with `// BUGFIX:` and Devilution improvements with
`// TODO:`* — two markers for two kinds of divergence-from-ideal, enforced by
convention in the archival repo. Alongside it,
[devilution issue #64](https://github.com/diasurgical/devilution/issues/64) is a
long-lived "good first issue" tracker whose one job is to catalogue **bugs native
to the original game** found while reading the code, explicitly scoped to *new*
ones with the Lurker Lounge DSF buglist named as the baseline so they do not
re-report known defects (examples: a town-portal placement anomaly on quest
levels; a Diablo immunity check testing the wrong AI field).

This is the ecosystem's closest thing to `DIVERGENCE.md`, and the structural fact
worth copying is **which repo it lives in.** The catalogue of "what the original
actually did, including where it was wrong" sits in the *archival* repo, and the
living fork cites it: DevilutionX
[PR #6438](https://github.com/diasurgical/devilutionX/pull/6438) implements
"the strategy described in devilution/issues/64#issuecomment-1535561455", a
living-side change citing an archival-side finding seven years after the fork.
For us, that catalogue belongs on the sibling's side of the fence (CLAUDE.md
rule 6) and `DIVERGENCE.md` should cite rather than restate it.

### 2.7 How the fork actually worked

The mechanics are recoverable from release notes and repo metadata.

- devilution created 2018-04-02, devilutionX 2018-08-02 (GitHub API): the fork
  happened **two months after devilution's public release and ten months before
  it reached 100% matching.** The living fork did not wait.
- DevilutionX release notes 0.1.0 through 0.5.0 each open with *"Based on
  Devilution 0.5.0 / 0.5.8 / 0.6.0 / 0.9.6 / 0.10.0"*
  ([releases](https://github.com/diasurgical/devilutionX/releases)) — for roughly
  18 months the fork **pulled from upstream at every upstream release.** 0.10.0
  (June 2019) is the last named, and is exactly where devilution hit 100%. No
  post-1.0.0 release names a base version (all 22 checked via the API).
- devilution has cut no release since 0.10.0, yet is **not archived** and still
  receives commits: 2022–2025 is almost entirely `monster: add BUGFIX for mVar2`,
  `inv: add BUGFIX for CheckInvCut`, `lighting: add BUGFIX for MakeLightTable`
  ([commit log](https://github.com/diasurgical/devilution/commits/master)).

So: **no code flows back, findings do.** Once the archival repo achieved its
invariant it froze *as code* and stayed live *as documentation*, accumulating
annotations about original behaviour discovered downstream. That is the
equilibrium PLAN §6 predicts for us, reached by a project that did not plan for
it.

The timing lesson is sharper. Forking early meant DevilutionX spent 18 months
merging a codebase being *rewritten under it* — every "bin exact" commit upstream
rewrites a function the fork had already begun refactoring. Our situation
is better: we never merge, because the archival artifact is a different program.
Our coupling is a comparison harness, not a merge conflict.

### 2.8 The transition to "source you can actually change"

DevilutionX's path out of matching-land, in the order they did it:

1. **Replace the Windows DLL boundary first.** Diablo depended on `Storm.dll`,
   `DiabloUI.dll`, `SmackW32.dll` and `Standard.snp`; DevilutionX replaced them
   with SDL, libsodium and their own UI, which is why it had a Linux build within
   months. Same ordering as PLAN §5 phase 3, and our surface is smaller — PLAN §1
   counts 101 imports across 7 DLLs.
2. **Get off 32-bit late, not early.** DevilutionX 0.5.0 (Oct 2019):
   *"All builds are now 64bit (except for Windows and Raspberry Pi)"*; Windows
   only switched at 1.3.0 in Nov 2021. Two years from first portable build to
   64-bit everywhere.
3. **Globals are the long pole and are still not finished.**
   [Issue #2435, "Reduce use of globals"](https://github.com/diasurgical/devilutionX/issues/2435),
   opened 2021-07-20 and **still open in 2026**, is a checklist of ten global
   arrays (`Items`, `Objects`, `Players`, `Quests`, …) to pass by reference
   instead; five are ticked. The stated motivation is testability, not elegance.
   Five years, half done. PLAN §1 says ~2.8 MB of `BOF3.exe`'s `.data` is BSS —
   that is our version of this problem and it is bigger.
4. **Packed structs become an explicit serialisation layer.** Their
   [`test/pack_test.cpp`](https://github.com/diasurgical/devilutionX/blob/master/test/pack_test.cpp)
   (160 KB of test) exercises `ItemPack` / `PlayerPack` round-trips and carries
   explicit `Swap32LE`/`Swap64LE` helpers per field. Once you target big-endian
   consoles (their CI builds Amiga M68K, PS4, Xbox, 3DS, Vita, Switch), a struct
   that was a memory image becomes a format with an endianness, and every field
   has to be named to be swapped. Their changelog also records
   *"Correct networking support on big-endian systems"*. Our save format and
   `DAT/` containers will hit this the moment we leave x86.
5. **RNG becomes versioned infrastructure.** Their open
   [issue #2261](https://github.com/diasurgical/devilutionX/issues/2261) proposes
   "an alternate RNG and proposed API for versioning Random Number Generators",
   and merged work includes "Isolate shrine/quest pool RNG from global RNG state"
   and "Introduce xoshiro RNG to generate dungeon seeds". PR #6438 is worth
   reading in full: it reserves 10,000 values of RNG sequence per dungeon level
   to stop sequences overlapping, after *measuring* that level generation consumes
   1,500–6,000 values normally and up to ~480,000 in the layout retry loop. That
   is what "fix a design bug in a living fork" looks like when the original's
   behaviour is load-bearing for everything downstream.

### 2.9 Regression testing, which is the part most directly about our §6

DevilutionX did not keep a binary oracle, so they built behavioural ones:

- **Golden dungeon fixtures.** `test/drlg_l1_test.cpp` and friends call
  `LoadExpectedLevelData("diablo/1-2588.dun")`, generate level 1 with seed 2588,
  and assert the whole tile map *and* the resulting camera position for both entry
  directions; fixtures live in `test/fixtures/{diablo,hellfire,levels}`. A
  deterministic subsystem plus a captured expected output is a cheap, durable
  oracle — the same opportunity we have for encounter tables, damage formulas and
  script control-code handling (PLAN §6).
- **Timedemos.** They record input demos and replay them in CI
  (`test/fixtures/timedemo`; see
  [issue #2745](https://github.com/diasurgical/devilutionX/issues/2745), candid
  about the failure modes: alt+enter read as enter, real-time dialogs desyncing,
  level transitions firing early, players not walking the identical path after a
  transition). Input replay is a powerful regression net and *fragile against
  exactly the timing changes a renovation makes.*
- **The oracle constrains the fix.** PR
  [#6438](https://github.com/diasurgical/devilutionX/pull/6438) is titled "RNG
  fixes for dungeon generation **that don't break timedemo**", and defers half the
  fix because it would. Expect that tension; decide in advance which wins (our
  answer: ledger the change, re-record the demo).
- **Item corpus harvested by hand from the original game.**
  [Issue #1094, "Gather vanilla items for testing"](https://github.com/diasurgical/devilutionX/issues/1094),
  crowdsources save games containing items with each possible property,
  *"preferably verified in the original before submitted so that we know that the
  regeneration code works identical to the original."* They had to ask humans to
  play Diablo to build a test corpus. **We do not** — the archival sibling
  generates one on demand. This issue is the evidence for what PLAN §6's central
  claim is worth.

### 2.10 Game data without shipping game data

- Both READMEs state plainly that `DIABDAT.MPQ` is required, that none of it is
  provided, and that GOG is the legal route. Same posture as CLAUDE.md rule 1.
- They wrote **their own freely-licensed asset pack** so the engine can boot
  without the game: `devilutionx.mpq`, built with `smpq` at package time, holding
  fonts and UI elements, because *"DevilutionX requires some core assets to render
  UI elements and fonts even if game data is not available"*
  ([building.md](https://github.com/diasurgical/devilutionX/blob/master/docs/building.md)).
  Sources are Noto/Unifont under the SIL OFL and Pixabay sound effects, documented
  per file in [devilutionx-assets](https://github.com/diasurgical/devilutionx-assets).
  If you cannot ship the data, you still need enough non-data to render the error
  message saying so.
- **Tests skip rather than fail when assets are absent** — `pack_test.cpp` carries
  the constant `"MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test
  suite"`. A public CI that cannot legally hold the data still runs everything
  that does not need it. Worth designing our harness for from day one, since
  `analysis/` is gitignored for the same reason.
- They support **loose files in preference to the archive** (devilution 0.5.0:
  *"Assets can now be loaded directly from disk (no need for MPQ-files when
  modding)"*). Relevant to our `DAT/` parser (PLAN §8 step 3): make the loader
  prefer a loose file, and modding plus our own test fixtures fall out of one code
  path.

---

## 3. Pitfalls and things they'd do differently

- **They forked before the archival base was finished, and paid for it in
  merges.** 18 months of tracking a codebase that was being rewritten function by
  function underneath them. We cannot make this mistake in the same way, but the
  analogous error is available: treating the sibling's in-flight findings as
  settled and having to redo work when the disc corrects them.
- **A comment convention did not scale to a structured record.** `// BUGFIX:` is
  good in-place annotation, but the actual catalogue is a hand-maintained GitHub
  issue with a "last update" line and a numbered list, eight years on. It works
  because the archival side changes almost never; a *living* project generates
  divergences far faster, which is why our ledger is a reviewed file, not an issue.
- **Their "Summary of Changes" page is player-facing, not engineering.** The
  [wiki page](https://github.com/diasurgical/devilutionX/wiki/Summary-of-Changes-in-DevilutionX-from-Diablo)
  groups changes as Engine / UI / Gameplay, marks many "(off by default)", and
  cites chapters of *Jarulf's Guide* for the original behaviour — genuinely good
  practice, and their nearest thing to "what the original did / what we do now".
  But it does not record *why*, and it does not distinguish a deliberate change
  from a fix for a regression the port itself introduced. `docs/README.md` already
  forbids merging those two readers; this is what you get when you try.
- **Release notes carry the distinction the wiki doesn't.** DevilutionX 1.0.0
  separates "Bugfixes" (theirs) from "**Original Diablo bugs**" (the game's) as
  two headed lists — their most ledger-like habit, living in their least durable
  document.
- **Input-replay regression tests rot under exactly the changes you want to
  make.** See issue #2745's list. Budget for re-recording, and store the demo
  alongside the ledger entry that invalidated it.
- **Globals defeat testability for years.** Issue #2435 has been open since 2021.
  If we want per-function equivalence tests in phase 3, the BSS has to become
  addressable as data structures rather than addresses, and that work does not
  get easier by being deferred.
- **Matching is not a one-time achievement.** devilution reached 100% in June
  2019 and was still landing per-function optimisation-flag corrections in 2024.
  A "done" bar of this kind decays as understanding improves.

---

## 4. Where to go when blocked

| Our problem | Go read |
|---|---|
| "Which compiler, which flags built `BOF3.exe`?" | [BACKGROUND.md](https://github.com/diasurgical/devilutionX/blob/master/docs/BACKGROUND.md) §3–4 and [devilution#111](https://github.com/diasurgical/devilution/issues/111); the Rich header method is in [bytepointer's article](https://bytepointer.com/articles/the_microsoft_rich_header.htm) |
| "How do I prove my reimplementation matches?" (PLAN §5 phase 3 exit test) | [Cleaning Code](https://github.com/diasurgical/devilution/wiki/Cleaning-Code) for the tolerances; [devilution-comparer](https://github.com/diasurgical/devilution-comparer) for the tool shape |
| "How do I track percent-decompiled honestly?" | The [binary identical functions milestone](https://github.com/diasurgical/devilution/milestone/3) and the [devilution changelog](https://github.com/diasurgical/devilution/blob/master/docs/CHANGELOG.md) — issue-count-as-metric |
| "How do I carry PSX names onto the PC binary?" (PLAN §3) | [devilution CONTRIBUTING.md](https://github.com/diasurgical/devilution/blob/master/docs/CONTRIBUTING.md) — the `.SYM` walkthrough and the dual-naming docblock convention; [sanctuary/notes](https://github.com/sanctuary/notes) for the annotation format |
| "How do I recover Capcom's file decomposition?" (PLAN §3, `SHARED_SOURCE.md`) | devilution changelog 2018-04-20, and the `file = ...\DRLG_L1.CPP` example in CONTRIBUTING.md |
| "How do I regression-test a deterministic subsystem?" | [`test/drlg_l1_test.cpp`](https://github.com/diasurgical/devilutionX/blob/master/test/drlg_l1_test.cpp) + `test/fixtures/` |
| "How do I regression-test whole play sessions?" (PLAN §5 phase 2) | [devilutionX#2745](https://github.com/diasurgical/devilutionX/issues/2745), demo/timedemo machinery |
| "What breaks when I leave x86-32?" | [`test/pack_test.cpp`](https://github.com/diasurgical/devilutionX/blob/master/test/pack_test.cpp) (endian swaps per field), [devilutionX#2435](https://github.com/diasurgical/devilutionX/issues/2435) (globals), changelog entries for the 2019→2021 64-bit migration |
| "How do I change RNG-dependent behaviour without wrecking everything?" | [devilutionX#6438](https://github.com/diasurgical/devilutionX/pull/6438) and [#2261](https://github.com/diasurgical/devilutionX/issues/2261) |
| "How do I build and test without shipping game data?" (rule 1) | [devilutionx-assets](https://github.com/diasurgical/devilutionx-assets) and the `devilutionx.mpq` sections of [building.md](https://github.com/diasurgical/devilutionX/blob/master/docs/building.md) |
| "What does a living fork's change record look like, and what's missing from it?" | [Summary of Changes wiki](https://github.com/diasurgical/devilutionX/wiki/Summary-of-Changes-in-DevilutionX-from-Diablo) and the [1.0.0 release notes](https://github.com/diasurgical/devilutionX/releases) |

---

## 5. Licence, and what it means for us

**Both repos are under the [Sustainable Use License](https://github.com/diasurgical/devilution/blob/master/LICENSE.md)
v1.0** — identical text in
[devilution](https://github.com/diasurgical/devilution/blob/master/LICENSE.md)
and [devilutionX](https://github.com/diasurgical/devilutionX/blob/master/LICENSE.md),
and both READMEs restate it: *"The source code in this repository is for
non-commercial use only. If you use the source code you may not charge others for
access to it or any derivative work thereof."* GitHub reports the SPDX id as
`NOASSERTION` for both, i.e. it is not a recognised open-source licence.

What SUL v1.0 actually grants: use, copy, distribute, make available and prepare
derivative works, limited to your own internal business purposes or non-commercial
/ personal use; distribution must be free of charge and non-commercial; you may
not remove licensing or copyright notices; **anyone you give a copy to must also
get a copy of the SUL terms**; and modified copies must carry a prominent notice
saying you modified them.

For this repo:

- **Reading is unambiguously fine.** Documentation, wikis, issue threads and
  design decisions are facts about how they worked; this note reproduces none of
  their code.
- **Vendoring is not blocked in principle but is an entanglement, and rule 5 says
  don't.** SUL's non-commercial limitation sits comfortably beside PolyForm
  Noncommercial — neither permits commercial use — but they are *different*
  non-commercial licences with different notice and modification-marking duties.
  Any vendored file would have to keep its SUL text, be marked as modified, and be
  carved out of our licence in a `THIRD_PARTY.md`: real ongoing overhead for code
  we do not need.
- **Nothing here needs vendoring anyway.** What is valuable is technique — the
  normalise-and-diff comparer, the tolerance definition, the milestone metric, the
  fixture pattern — none of it copyrightable expression, and all of it something
  we would reimplement against our own binary under our own evidence rule.
- **Their asset pack is separately licensed** (SIL OFL fonts, Pixabay sounds) and
  is *not* SUL. If we ever need a freely-licensed font or placeholder sound, that
  repo is a curated, per-file documented starting point under permissive terms.

The practical summary: **read it all, cite it, copy no source.** Same posture
CLAUDE.md rule 5 already sets for `bof3ext`, for the same reason.

---

## 6. What I could not verify

- **Whether anyone involved regrets the fork or its timing.** An interview with
  DevilutionX maintainer AJenbo exists as an
  [audio recording on archive.org](https://archive.org/details/diablo-interview-ajenbo);
  I did not listen to it, so nothing from it is in this note. Their Discord is the
  primary discussion venue and is not web-indexed. **Treat §2.7's fork narrative
  as inferred from release notes and repo metadata, not as anyone's stated
  account** — including the claim that the fork stopped tracking upstream *because*
  devilution hit 100% in June 2019, which is my correlation, not their statement.
- **Whether any code ever flowed back from DevilutionX into devilution.** Findings
  demonstrably do (PR #6438 citing issue #64) and recent devilution commits are
  annotation-only, but I did not diff the trees or look for cherry-picks. "No code
  flows back" is an inference from the commit log.
- **The licence history.** Both repos are SUL v1.0 today; whether either was ever
  under different terms, and whether contributors were asked to re-license, is
  unchecked. It would matter only if we ever wanted to vendor.
- **How they handled x87 floating point.** PLAN §1 counts ~10,800 x87
  instructions in `BOF3.exe`. Diablo is largely fixed-point and I found no
  discussion of matching x87 codegen or of float determinism across platforms in
  DevilutionX. **This is probably our problem alone**, and the sibling repo's
  hardware-accuracy work is a better place to look than this one.
- **Whether devilution ever measured "percent of the *image*" rather than
  "percent of functions".** The 0.13% file-size delta at 60% of functions is the
  only whole-image number I found.
- **Anything about the Hellfire expansion's separate build** (VC5 vs VC6 per the
  first search result) beyond the toolchain mention; I did not pursue it because
  it has no analogue here.
