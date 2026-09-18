# Prior art — Diaphora

**Status:** DRAFT (researched 2026-09-18)

## 1. What it is

[Diaphora](https://github.com/joxeankoret/diaphora) is Joxean Koret's open-source
program-diffing plugin for IDA Pro, first released in 2015 and maintained
continuously since: it exports each binary's functions into an SQLite database of
precomputed properties, then runs roughly fifty named *heuristics* in a fixed
order to pair functions between the two databases
([wiki: Heuristics and their explanation](https://github.com/joxeankoret/diaphora/wiki/Heuristics-and-their-explanation),
[Hex-Rays plugin focus](https://hex-rays.com/blog/plugin-focus-diaphora)).
Every heuristic carries a declared match quality — *Very good*, *Good*, *Poor*,
*Very poor*, *Unreliable* — and they run "in the specific order they are executed
(as the order really matters)", most reliable first, with each confirmed match
removed from the pool the later heuristics see
([wiki: Matching Strategy](https://github.com/joxeankoret/diaphora/wiki/Matching-Strategy)).

That is our phase-1 matcher (PLAN §3), built by someone who has been iterating on
it for eleven years and has publicly demoted or deleted the signals that turned
out to lie. We are not going to run it — it needs IDA (§5) — but its heuristic
table is the closest thing that exists to a *pre-tiered list of binary-matching
evidence*, which is exactly the artifact `docs/README.md`'s evidence tiers want
and which we would otherwise learn the expensive way.

## 2. What we can steal

Our situation is the hard corner of this problem — MIPS/Psy-Q 1997 against
x86-32/MSVC6 2001 — but also an unusually favourable one, because
[`SHARED_SOURCE.md`](../SHARED_SOURCE.md) establishes common descent rather than
mere similarity. Below, each family of Diaphora heuristic with a verdict for our
cross-ISA case.

### 2.1 Byte-, address- and mnemonic-level signals — **dead for us**

`Same RVA and hash`, `Same order and hash`, `Function hash` ("MD5 hash of the non
relative bytes"), `Bytes hash`, `Bytes sum`; and `Mnemonics and names`,
`Mnemonics small-primes-product` (a prime per mnemonic, multiplied, giving an
order-independent invariant — the [BinDiff manual](https://www.zynamics.com/bindiff/manual/)
calls the same idea *prime signature matching*), `Same address, nodes, edges and
mnemonics`, plus the microcode variants. These sit at the top of Diaphora's
default set, rated *Very good*, because in patch diffing most functions are
byte-identical.

None of them survives a change of ISA. Encodings share no bytes; the mnemonic
vocabularies are disjoint; the *counts* diverge systematically (one x86 `lea`
strength-reduction chain against several MIPS instructions, MSVC6's magic-number
division `0x51EB851F` + `sar 5` against whatever Psy-Q emitted); and the
RVA-based ones additionally assume comparable image layouts, which we do not have
(PSX overlays versus one flat 1.80 MB `.text`). Diaphora knows this: release 3.0
added "Do not run assembly based heuristics when diffing different CPU
architectures" ([releases](https://github.com/joxeankoret/diaphora/releases)).
That single line is the honest headline of this whole document — **the tool's own
highest-quality tier is switched off in our scenario.** (`Bytes sum` is rated
*Unreliable* even same-architecture.)

### 2.2 Graph and CFG structure — **survives, degraded; the workhorse**

`Same rare MD Index`, `Same MD Index and constants`, `Topological sort hash`
(Tarjan), `Nodes, edges, complexity, mnemonics, names, prototype`,
`Strongly connected components` (small-primes-product over the SCCs, restricted
to functions with more than 10 basic blocks), `Same graph`.

MD-Index is a hash of the control-flow graph built from topological order plus
in-degree and out-degree, derived from Dullien et al.'s work and shared with
BinDiff
([Quarkslab](https://diffing.quarkslab.com/qbindiff/doc/source/features.html);
[BinDiff manual](https://www.zynamics.com/bindiff/manual/)). Nothing in it reads
an opcode, so it is architecture-neutral *by construction* — this is the family
that actually crosses the ISA boundary. The degradation is real, though, and
predictable for our pair:

- **Branch lowering differs.** `MsgBox_Step`'s 23-arm dispatch is a jump table on
  both sides (text probe §3), so that one is safe — but a compiler may turn a
  small switch into a compare chain on one side and a table on the other, which
  changes node and edge counts outright.
- **Inlining changes the graph.** Our `Window_Task` case exactly: PC `0x595450`
  is 613 instructions against a much smaller PSX counterpart, and is
  `hypothesis` for that reason. MD-Index would not match that pair, and should
  not.
- **Rarity is computed per database.** `Same rare MD Index` fires when the index
  appears once or twice **in both databases**. Ours are 2,952 PC functions
  against a PSX corpus several times larger once overlays are indexed
  (estimated 12–13k Ghidra-discovered functions), so an index rare among 2,952
  is far more likely to be non-rare on the PSX side, and the heuristic silently
  under-fires in a way it does not for a normal two-version diff. Compute rarity
  against the *subset* being searched, not the whole PSX corpus.
  *(Corrected 2026-09-18: this paragraph originally said 30,062, a figure with
  no artifact behind it — see
  [`../overlay-transfer-feasibility.md`](../overlay-transfer-feasibility.md).
  The argument is unaffected; only the magnitude changes.)*

Practical note we should copy verbatim: the SCC heuristic is restricted to
functions with **more than 10 basic blocks** in the default set; the same
heuristic without that restriction is filed under *Unreliable*. The restriction
*is* the heuristic.

### 2.3 Constants — **strongest cross-ISA signal, and we already proved it**

`Same constants` ("the constants used by both functions and their order is the
same in both databases"), `Same MD Index and constants`, and `Same rare constant`
added in the 3.x line
([releases](https://github.com/joxeankoret/diaphora/releases)). Constants are
source-level facts: they survive the compiler, the calling convention and the
ISA, and they are what
[`kinship-probe-battle-engine.md`](../kinship-probe-battle-engine.md) §4–5
actually used — the `0xCD` clamp, the `0x118`/`0x128` strides, the eight-value
variance table that found `Battle_ScaleDamage` from nothing but its contents.
`Same rare constant` is precisely our "value-sequence search needs no seed"
anchor, arrived at independently. Two corrections our measurements force on it:

1. **Width-agnosticism.** The variance table is `u16` on PSX and `u32` on PC with
   identical values; a search keyed on encoded bytes or a fixed element width
   reports it absent, which is what our first attempt did. Diaphora collects
   constants as IDA typed them; for us a constant must compare as a *number*.
2. **Address constants must be excluded.** Much of both binaries' immediates are
   addresses (`0x80xxxxxx` versus `0x4xxxxx`/`0x9xxxxx`), guaranteed to differ
   and pure noise. Diaphora's `Same constants` need not care; ours must.

Verdict: build this first, rank a match by the *rarity* of the constants shared
(a function sharing `0xCD` and `0x3FFF` with its candidate is worth far more than
one sharing `0` and `1`), and exclude anything that looks like an address.

### 2.4 Switch structures — **excellent cross-ISA, underrated**

`Switch structures`: "same number of cases and cases values", filed under *slow
heuristics*, rated *Good*. Case *values* are source-level, like constants, and
the case *count* is a strong discriminator. Our text-engine probe matched
`MsgBox_Step` partly on exactly this: `cmp eax, 0x16 / ja / jmp [eax*4 +
0x497a70]`, a 23-entry table against the PSX switch on codes `0x00`–`0x16`. For a
game PLAN §1 calls "a function-pointer machine" with 259 scaled `[reg*4+imm]`
dispatch sites, a switch-shape index over both binaries is cheap and high-yield.
Rate it higher than Diaphora does — it is filed as slow only because it is rare
in the system-software corpora Diaphora is normally pointed at.

### 2.5 Pseudo-code heuristics — **the idea survives; the implementation cannot**

`Pseudo-code fuzzy hash` / `…hashes` (three fuzzy hashes from Koret's own
DeepToad), `Pseudo-code fuzzy AST hash`, `Partial pseudo-code fuzzy hash` (first
16 bytes), `Similar pseudo-code`, `Equal assembly or pseudo-code`, `Same cleaned
up assembly or pseudo-code` (ignoring autogenerated `sub_XXXX` names). This
family has the best theoretical claim to crossing an ISA boundary, because a
decompiler is a normaliser: two compilations of one C function should decompile
to structurally similar C regardless of the machine underneath. The AST-hash
variant is the strongest form, since it discards the textual surface entirely.

For us the tooling is the problem and the opportunity. Diaphora's version needs
the Hex-Rays decompiler on both sides, and a MIPS decompiler is a separate
purchase on top of IDA. But **we have the Ghidra decompiler for both ISAs
already**, and the archival sibling's pipeline is Ghidra-based. A fuzzy AST hash
over Ghidra's decompiled output for MIPS and x86 is a straightforwardly buildable
phase-1 experiment and is, as far as this research found, not something anyone
has published results for on a MIPS↔x86 same-source pair — a genuinely open lead
rather than a known-good technique. The `Same cleaned up …` variant carries one
rule worth adopting directly: strip autogenerated names before hashing, or you
are hashing the disassembler's arbitrary choices.

### 2.6 Name-, string- and import-based signals — **useless for us**

`Same name`, `Import names hash`, `Small names difference`, `Similar pseudo-code
and names`, BinDiff's `string references`. All assume shared symbol or string
material. We have neither: the PC side is stripped (2,952 unnamed functions), the
import surface is Win32/DirectX against the PSX's BIOS thunks (`Rand`
`0x8017ED4C` → CRT `rand` `0x5B93D2`), and PLAN §4a establishes that the Chinese
script is re-encoded and lives in repacked `DAT/` containers, so string-reference
matching has nothing to bite on. Skip the family — though once we *do* have PC
names, these become cheap self-consistency checks in the reverse direction.

### 2.7 Propagation — `Callgraph matches` and `Local affinity`

**`Callgraph matches`**: "recursively identifies callers and callees of
previously discovered matches", rated *Good*, and notably **not** a SQL query
like the rest — one of only two procedural heuristics, alongside `Brute-forcing`
([wiki: Matching Strategy](https://github.com/joxeankoret/diaphora/wiki/Matching-Strategy)).
BinDiff does the same more elaborately: after global matching it examines
"parents (callers) and children (callees) of each new match", re-runs the
drill-down steps on that reduced set, then matches functions called from matched
basic blocks, iterating "until no new matches emerge"
([BinDiff manual](https://www.zynamics.com/bindiff/manual/)).

The insight to take: **propagation is not a heuristic, it is a loop around the
heuristics.** A confirmed match removes two functions from the pool *and* shrinks
the candidate set for their neighbours from "everything" to "the neighbours of
the counterpart". Our block-delta propagation is the same move over the data
graph rather than the call graph, and the two are complementary: a delta names a
block of fields, the fields name the functions touching them, and the call graph
then predicts *those* functions' neighbours. Requiring both to agree beats
either alone.

**`Local affinity`**, added in the 3.x line to "find matches in functions gaps"
([releases](https://github.com/joxeankoret/diaphora/releases)), is the spatial
version: if *A* and *C* are matched and *B* lies between them in both images, *B*
is a strong candidate. That maps onto a prediction we have not tested and should.
[`SHARED_SOURCE.md`](../SHARED_SOURCE.md) §2 shows per-translation-unit *statics*
stayed contiguous in both binaries; if the same holds for per-translation-unit
*code*, emission order within one `.c` file's functions is preserved on both
sides and gap-filling works. A cheap, falsifiable experiment on the twelve pairs
we already have.

**Failure modes of propagation, stated honestly.** Both tools are greedy with no
backtracking — a pair, once taken, is not revisited. Consequences:

- **Error amplification.** One wrong seed propagates outward and its descendants
  look mutually corroborating, because they were derived from each other. Our
  probes are hand-seeded from the sibling's landmarks — safe at twelve pairs, not
  at twelve hundred.
- **Inlining breaks degree**, on one side only, and **the two compilers inline
  differently.** A callee inlined on one side removes an edge and changes both
  functions' in/out-degree (`Window_Task` again); Psy-Q and MSVC6 do not make the
  same decisions, so neighbourhoods are systematically not isomorphic. PLAN §3's
  "isomorphic up to inlining" is doing real work in that sentence.
- **Greedy local strategies generalise poorly.** The network-alignment literature
  levels this at BinDiff and Diaphora specifically: local greedy assignment gives
  "good solutions on simple cases but generalize[s] poorly on more difficult
  problem instances"
  ([arXiv 2112.15336](https://arxiv.org/pdf/2112.15336)).

The mitigation we can afford and they cannot: our tiering. A propagated name is a
`hypothesis` until a PC-side read confirms it, and `SHARED_SOURCE.md` §4 already
says a disassembly read is usually enough. Propagate freely, tier honestly, and
never let a `hypothesis` seed another `hypothesis` without a marked chain.

### 2.8 Compilation units — **the most valuable single idea here**

Diaphora 3.0 recovers *compilation units* at export time and diffs within them
([doc/articles/compilation_units.md](https://github.com/joxeankoret/diaphora/blob/master/doc/articles/compilation_units.md)).
It gets them two ways: from debug strings naming source files (IDA Magic
Strings), and — for stripped binaries — from **CodeCut's Local Function Affinity
algorithm**, which infers object-file boundaries from how densely functions call
their neighbours. It then adds three heuristics on top: same named CU + fuzzily
matched AST, same *unnamed* CU + matched AST, and same CU + sufficient
similarity. The stated purpose is false-positive suppression: comparing within
units "minimis[es] the risk of false positives because two functions that have
the same body appear in different parts of the binary — this is something more
common than what you might think."

This is the same finding as [`SHARED_SOURCE.md`](../SHARED_SOURCE.md) §2, reached
from the opposite side. We recovered translation-unit structure from **global
data block deltas**; CodeCut recovers it from **call affinity in the code**. Two
independent partitions of the same source-file decomposition, so they can be
cross-checked — and where they agree, the recovered `.c` boundary is far better
evidenced than either method alone. For a project whose deliverable is
maintainable source starting from Capcom's own file boundaries, that is not a
side benefit; it is the phase-1 output `SHARED_SOURCE.md` already asked for.
(CodeCut is a separate NSA-originated tool, not Diaphora's code; its licence was
not checked.)

### 2.9 The demoted tier — what Diaphora learned not to trust

A list of things *not* to build. *Unreliable*: `Bytes sum`; `Strongly connected
components` without the block-count restriction; `Loop count` for any looping
function; unrestricted `Strongly connected components SPP and names`. *Poor* or
*Very poor* among the experimental set: `Same nodes, edges and strongly connected
components`, `Similar small pseudo-code`, `Small pseudo-code fuzzy AST hash`,
`Equal small pseudo-code`, `Same low complexity, prototype and names`, `Same low
complexity and names`.

The pattern is unmistakable: **every demoted heuristic is one applied without a
size or complexity floor.** The same signal is *Good* with a floor and
*Unreliable* without. The release notes also record outright deletions —
`Call address sequence` removed as "old wrong and buggy", `Bytes hash and names`
and `Strongly connected components SPP and names` dropped
([releases](https://github.com/joxeankoret/diaphora/releases)).

## 3. Pitfalls, false positives, and the limits of automation

**Small functions are the dominant false-positive source, and the fix is a blunt
floor.** Diaphora ships two guards: an option documented as needed because "many
heuristics will cause false positives when comparing small functions (for
example, functions with less than 5 instructions)"
([wiki: export/diffing dialog fields](https://github.com/joxeankoret/diaphora/wiki/Explanation-of-every-single-field-in-the-export-diffing-dialog)),
and a 3.x change to "ignore functions with less than 3 basic blocks to remove
potential false positives"
([releases](https://github.com/joxeankoret/diaphora/releases)). Our PC corpus has
a median of 76 instructions and a p90 of 457
([text-engine probe](../kinship-probe-text-engine.md)), so a long tail sits under
any such floor. Adopt a floor, state it, and treat everything under it as
matchable only by propagation from a confirmed neighbour — never on its own
signal.

**"Best" does not mean correct.** Diaphora's `Relaxed ratio calculations` option
is explicit that "if only the size of a variable, or the type, or some other
small thing changed, it will still mark the match as 'Best' with a ratio of 1.0"
— and it is recommended exactly for *porting between versions*, our case. A 1.0
in that mode describes the tool's tolerance, not the code. BSim's docs make the
point more sharply: "you could end up bringing incorrect datatypes into a
program, even using BSim matches with 1.0 similarity"
([BSim tutorial](https://github.com/NationalSecurityAgency/ghidra/blob/master/GhidraDocs/GhidraClass/BSim/BSimTutorial_Evaluating_Matches.md)).

**A confidence score is not an evidence tier.** BinDiff's confidence is "the
average algorithm confidence (match quality) used to find a particular match
weighted by a sigmoid squashing function"
([BinDiff manual](https://www.zynamics.com/bindiff/manual/)) — a statement about
*which heuristic fired*, laundered into a number. Diaphora's quality labels are
the same information unlaundered, which is the better form. Our `evidence` /
`hypothesis` / `unnamed` tiering does a different job than either: it records
whether a *human verified it on the PC side*. Do not let a matcher write
`evidence`.

**The failure mode our corpus makes worse.** Diaphora's design assumes two
comparable programs. We have one flat 2,952-function PC image against a PSX side
split across a boot EXE and 406 separate overlay images
([battle-engine probe, Open](../kinship-probe-battle-engine.md)). A 1-vs-N
problem inflates the false-positive rate of every rarity-based heuristic, because
"rare" was defined over the wrong population (§2.2). Any tool we adopt or build
must be told which population rarity is measured against.

**The known-false region.** `SHARED_SOURCE.md` §4 is blunt that the platform
layer is *not* shared source, and the battle probe lists the exceptions — PSX
scratchpad `0x1F80xxxx` with no PC counterpart at any delta, BIOS thunks become
CRT calls, strides changed where a field was widened. A matcher assuming a
uniform mapping is not slightly wrong there; it is wrong with high confidence.

**What Koret himself says about limits.** Less than hoped. He calls the Hex-Rays
write-up "only the tip of the iceberg"
([Hex-Rays](https://hex-rays.com/blog/plugin-focus-diaphora)) and the
`Matching Strategy` wiki page ends in "(To be continued)". The real record of
hard-won knowledge is not an essay — it is the quality column of the heuristics
table and the deletions in the release notes. Read those as the primary source.

## 4. Where to go when blocked

- **[Heuristics and their explanation](https://github.com/joxeankoret/diaphora/wiki/Heuristics-and-their-explanation)**
  — the tiered list, in execution order. The single page to keep open while
  building our matcher. Raw markdown at
  `raw.githubusercontent.com/wiki/joxeankoret/diaphora/Heuristics-and-their-explanation.md`.
- **[BinDiff manual, "Function Matching Algorithms"](https://www.zynamics.com/bindiff/manual/)**
  — better *algorithmic* descriptions than Diaphora's docs, including MD-Index
  top-down/bottom-up, prime signatures, proximity MD index, call-sequence
  matching, and the propagation loop. Go here first when you need to know how a
  signal is computed, not just whether it is trusted.
- **[compilation_units.md](https://github.com/joxeankoret/diaphora/blob/master/doc/articles/compilation_units.md)**
  — for §2.8 and the pointer to CodeCut/LFA. **[Release
  notes](https://github.com/joxeankoret/diaphora/releases)** — the changelog is
  the archaeology: what was added, demoted, deleted as "wrong and buggy", and the
  cross-CPU gating line. **[How can I automate the diffing
  process?](https://github.com/joxeankoret/diaphora/wiki/How-can-I-automate-the-diffing-process%3F)**
  — the batch-mode envelope, if we ever get IDA access.
- **[BSim tutorial docs](https://github.com/NationalSecurityAgency/ghidra/blob/master/GhidraDocs/GhidraClass/BSim/README.md)**
  and [a practical walkthrough](https://www.pentestpartners.com/security-blog/fuzzy-matching-with-ghidra-bsim-a-guide/)
  — for the similarity-versus-confidence distinction our own scoring should copy.
  **[Issue #159](https://github.com/joxeankoret/diaphora/issues/159)** is the
  Ghidra port; subscribe rather than wait (§5). Koret's talks
  ([44CON](https://www.youtube.com/watch?v=wHRd39u02io),
  [BSidesLisbon 2015](https://www.youtube.com/watch?v=eAVfRxp99DM)) are the last
  resort; not reviewed here.

**Is it realistic for us to run it? No.** The export step requires IDA — "run IDA
in batch mode… `ida -A -B -S/path/to/diaphora.py your_binary`" — and only the
*diff* step is "pure Python and doesn't require IDA", operating on
already-exported SQLite databases. We have no IDA licence, and the Ghidra port
has been an open issue since 11 March 2019 ("this is going to be a long time
task"), milestoned to "Diaphora 4.0", with a second open issue (#297, 2024) on
the same subject. Treat Ghidra support as absent. The one technically open door —
a Ghidra exporter emitting Diaphora's SQLite schema, then the IDA-free diff step
— is a poor trade: weeks of work to obtain an engine whose top-quality tier is
switched off for cross-ISA diffing anyway (§2.1), leaving the graph, constant and
pseudo-code heuristics we can implement ourselves.

**How it compares for our specific job.** Honestly:

| | Diaphora | BinDiff | Ghidra BSim |
|---|---|---|---|
| Needs IDA | **Yes, to export** | No — [BinExport](https://github.com/google/binexport) has a Ghidra extension, shipped with BinDiff, described as beta | No |
| Licence | AGPL-3.0 | Apache-2.0 | Ghidra's licence (Apache-2.0) |
| Cross-ISA posture | assembly heuristics *explicitly disabled* cross-CPU | MD-Index/graph algorithms are ISA-neutral; byte and mnemonic ones are not | designed for it — the `nosize` database option excludes varnodes of 4 bytes and larger so differing integer sizes do not poison features |
| Shape of problem | 2 programs | 2 programs | **1 function against a database of many binaries** |
| Heuristic transparency | **best in class** — named, tiered, ordered | good — algorithm list with quality ranking | opaque by comparison: one feature-vector score plus confidence |

The last row decides it. Diaphora and BinDiff both assume a two-program diff; our
problem is one PC image against a corpus of PSX overlays, which is BSim's native
shape — an argument for continuing the BSim evaluation independent of anything
in this note.

The one hard cross-ISA number found: a WPI thesis,
[*Cross Architecture Function Matching*](https://digital.wpi.edu/downloads/8336h5032),
reports its own p-code matcher correctly matching 70% of cross-architecture
function pairs compiled from the same source, against **33% for BinDiff and 23%
for Ghidra Version Tracker**; Diaphora was not in that comparison. If those
figures are representative, the honest expectation for any off-the-shelf tool on
a MIPS↔x86 same-source pair is **a minority of functions**, and our hand-built
anchors (constants, data-block deltas, value-sequence search) are not a stopgap
before the real matcher — they are competitive with it. *(Unverified — see §6.)*

## 5. Licence, and what it means for us

**Diaphora is AGPL-3.0.** The README states: *"Since version 2.0, Diaphora is now
licensed under the GNU Affero GPL version 3 license."* Commercial licences are
sold separately for organisations that cannot accept AGPL
([README](https://github.com/joxeankoret/diaphora)).

- **We cannot vendor any of it.** AGPL is strong copyleft: a derivative work must
  be distributed under AGPL, and this repo is PolyForm Noncommercial, which is
  not AGPL-compatible in either direction. A harder bar than the `bof3ext`
  situation in PLAN §7 — there the issue is a *missing* licence that asking will
  probably resolve; here the licence is present, deliberate, and says no.
  **Rule 5 disposes of it cleanly: we vendor nothing, so nothing is gated — but
  if anyone later proposes lifting code from it, the answer is no, not "ask
  first".** Running it would have been fine (using a tool is not distributing a
  derivative); the blocker there is IDA, not the licence.
- **The heuristics are ideas, and ideas are not what a licence covers.** A
  matching technique is not protected by copyright; a particular expression of it
  in Python is. Everything in §2 was taken from prose documentation — the wiki,
  the release notes, the compilation-units article, the BinDiff manual — and most
  of the underlying techniques are independently published anyway: MD-Index
  traces to Dullien et al., small-primes-products to zynamics/BinDiff, LSH over
  feature vectors to BSim and the academic literature. **The practical rule:
  reimplement from the documentation, not from `diaphora_heuristics.py.`** That
  keeps provenance clean and is better engineering, since it forces us to
  re-derive rather than transliterate — the same posture rule 5 and the evidence
  rule already impose on `bof3ext`'s address corpus. No Diaphora source was read
  or quoted in producing this note.
- **Credit it.** If our matcher's tiering is recognisably Diaphora's, say so in
  the matcher's doc. Costs nothing and is plainly true.

## 6. What I could not verify

- **The WPI thesis figures (70% / 33% / 23%).** The PDF could not be text-
  extracted on this machine (no `pypdf`/`pdfminer`/poppler), so those numbers come
  from search-result summaries of it, not from reading it. The experimental setup
  — which architectures, which corpus, what counts as a match — is unread. Treat
  the numbers as indicative only until someone reads the document.
- **Whether Diaphora's cross-CPU gate covers the pseudo-code heuristics.** The
  release note says "assembly based heuristics". Whether graph-, constant- and
  pseudo-code-based heuristics still run in that mode is not stated anywhere I
  found, and it materially changes how useful the tool would be to us.
- **Exact heuristic count and the current roster.** The wiki page is summarised as
  "51 heuristics" but enumerates about 47, and the release notes record both
  additions (`Same rare constant`, `Local affinity`, microcode variants) and
  deletions, so the wiki may lag the code. Do not treat §2's list as the current
  shipping set without checking against a release. Likewise `Local affinity`'s
  actual algorithm: described only as finding "matches in functions gaps", with no
  published scoring or gap bound — §2.7's reading of it as address-order
  interpolation is an inference.
- **Any Koret writing specifically on the limits of automated matching.** Searched
  for and not found; `joxeankoret.com` was not reachable through the tooling here,
  and [diaphora.re](http://diaphora.re/) returned 403. His talks were not watched.
- **Diaphora's measured cross-architecture accuracy.** No published number found.
  The IEEE paper *Efficient Features for Function Matching in Multi-Architecture
  Binary Executables* uses Diaphora as a baseline, but the 82–83% figure visible
  in summaries appears to be a same-architecture kernel diff, and the paper is
  paywalled. **There is no verified answer to "how well does Diaphora do
  MIPS↔x86".** The strongest evidence remains indirect: the tool disables its own
  best heuristics in that mode.
- **CodeCut's licence**, and whether its LFA implementation is usable by us.
- **Whether per-translation-unit *code* contiguity holds** in our two binaries, as
  §2.7 speculates from the statics finding. Untested — a cheap experiment against
  the twelve pairs already in `symbols.toml`.
