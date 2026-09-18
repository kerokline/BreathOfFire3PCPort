# Prior art: OpenRCT2

**Status:** DRAFT (researched 2026-09-18)

Researched from public sources only — GitHub API (issues, PRs, tags, trees, commit
history), the project wiki, `distribution/changelog.txt`, release bodies, and the
OpenLoco blog. No OpenRCT2 source is reproduced here; mechanisms are described,
not copied. Claims are cited; §6 lists what I could not verify.

---

## 1. What it is, and how its shape compares to ours

OpenRCT2 is an open-source re-implementation of *RollerCoaster Tycoon 2*, started
by Ted "IntelOrca" John on 2014-04-01 and still under active development twelve
years later ([repo metadata](https://github.com/OpenRCT2/OpenRCT2); 750+ pages of
contributors via the API). It began as an incremental decompilation: a DLL of
newly-written C was injected into a patched `rct2.exe`, so that new code and
original machine code ran in the same address space and the game stayed playable
while functions were replaced one at a time
([IntelOrca, issue #1, 2014-04-14](https://github.com/OpenRCT2/OpenRCT2/issues/1)).
Today it is a standalone, cross-platform, 64-bit C++ program that needs only the
original game's *data* — and its stated 1.0 criterion is getting rid of that too
([Roadmap wiki](https://github.com/OpenRCT2/OpenRCT2/wiki/Roadmap)).

**Same architecture as PLAN §2C, almost exactly.** Hybrid binary, in-process
interop, one function at a time, playable at every commit, original binary
eventually dropped. This is the completed version of our journey and the single
most relevant prior art we have.

**Same invariant as ours, too** — OpenRCT2 is a *living* project, not an archival
one. It fixes original-game bugs, adds multiplayer, raises limits, and reintroduces
RCT1 features
([Changes to original game](https://github.com/OpenRCT2/OpenRCT2/wiki/Changes-to-original-game)).
So its divergence-management practices are directly comparable to ours, which makes
their weakness (§3) instructive rather than academic.

**Three differences that matter.** (a) RCT2 was written by Chris Sawyer in hand-written
x86 assembly, not compiler output — IntelOrca specifically declined an offer of
Hex-Rays output because "the game being written in assembly and therefore not
following typical compiler conventions"
([issue #1](https://github.com/OpenRCT2/OpenRCT2/issues/1)). `BOF3.exe` is MSVC 6.0
output with ordinary conventions, so decompiler assistance should work *far* better
for us than it would have for them. (b) They had no second compilation of the same
source to mine for names; we have the PSX side (PLAN §3). (c) They had ~750
contributors; we do not.

---

## 2. What we can steal

### 2.1 The hybrid mechanism, in both directions

Two separate problems, two separate mechanisms, and they got them both.

**Us → original: `addresses.h`.** A header of macros over the original image.
`RCT2_ADDRESS(addr, type)` yields a typed pointer to an original global;
`RCT2_GLOBAL(addr, type)` dereferences one. Calls into original code came in three
flavours: plain `RCT2_CALLFUNC_1`…`_6` (cast the address to a function pointer, for
anything with a normal stack signature), and `RCT2_CALLFUNC_X` / `RCT2_CALLPROC_X`,
which take a `registers` struct — a union exposing EAX/EBX/ECX/EDX/ESI/EDI/EBP and
their 16- and 8-bit subdivisions — load those registers, call, and return the flags
([`src/addresses.h` at v0.0.3](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/v0.0.3/src/addresses.h)).
Their [IDA wiki page](https://github.com/OpenRCT2/OpenRCT2/wiki/Decompiling-Tips-IDA)
tells contributors which variant to use: `RCT2_CALLPROC_EBPSAFE` when the callee
preserves registers, `_X` when it reads uninitialised registers as arguments, and
`RCT2_CALLFUNC_X` when it returns values in registers.

**Original → us: `hook.c`.** At runtime they `VirtualAllocEx`/`mmap` an
executable page, emit a fixed-size (140-byte) trampoline that spills all GPRs into
a global `registers` struct, calls the C function with a pointer to that struct,
restores the registers from it, and reconstructs the flags with `sahf` from the
return value's low byte; then they patch a 5-byte `E9` jump at the original address.
Capacity was hardcoded at 1,000 hooks
([`src/openrct2/rct2/hook.c` at v0.0.7](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/v0.0.7/src/openrct2/rct2/hook.c)).

For us: this is the shape of PLAN phase 0, and it confirms the phase-0 estimate
("a few hundred lines over MinHook"). The register-struct-as-ABI idea is the part
worth copying as a *design*: it makes non-standard calling conventions expressible
in ordinary C without inline assembly at every call site, which matters because our
binary is MSVC 6 output and will have `__fastcall`, `__thiscall` and
register-clobbering leaf functions all over it.

**One warning from their thread.** When a contributor proposed patching functions
with `WriteProcessMemory` and testing by hand, IntelOrca's objection was that it
only works "if you used the original registers for arguments to the function. Even
if there are no arguments, you still need to make sure you don't corrupt any
registers used by the caller"
([issue #360, 2014](https://github.com/OpenRCT2/OpenRCT2/issues/360)). Register
preservation is not an optimisation detail in a hybrid build; it is the correctness
boundary.

### 2.2 The data segment is the thing you carry across, not the code

The best-documented piece of the whole hybrid period. For 64-bit builds
janisozaur explained the arrangement: they extract the original `.data` segment
from the binary and map it "to memory at some known, arbitrary location. Whatever
is trying to use original's data, will simply get redirected to the new location…
All the integrated stuff comes from our own `.data` segment"
([issue #4198](https://github.com/OpenRCT2/OpenRCT2/issues/4198)). The address had
to stay inside the 32-bit range (`0x8a4000`) because original-format pointers still
lived in it.

And the hard limit, from IntelOrca in the same thread, on whether any
not-yet-decompiled original function could be used in a 64-bit build: "no they
can't be used, they are x86 instructions expecting 32-bit addresses. It just
wouldn't work."

**This is the finding that should be written into PLAN §4 (the "phase-4 accelerator").**
Data crosses the 32→64 boundary; *code does not*. You cannot have a partly-64-bit
build. Either every executing instruction is yours, or the process stays 32-bit.
That is an argument in favour of the crude-lifter accelerator rather than against
it — but it means the lifter has to cover **everything still reachable**, not just
the tail, on the day you flip.

### 2.3 The verifier that became a generator — the single best idea here

`test/testpaint` compared their new C++ track-drawing code against the original
routine, per ride type, track piece, rotation, and sequence. Notably it did **not**
diff pixels: it intercepted the paint calls and compared (i) the sequence and
arguments of paint calls, (ii) segment support heights, (iii) general support height
and slope, (iv) side and vertical tunnel geometry
([`test/testpaint` at v0.2.0](https://github.com/OpenRCT2/OpenRCT2/tree/v0.2.0/test/testpaint)).
A structured call trace is dramatically more diagnosable than an image diff and far
more tractable than register-level equivalence.

Then marijnvdwerf noticed that a checker precise enough to *validate* the original's
behaviour is precise enough to *emit* it. From duncanspumpkin's OpenLoco write-up:
"he realised we could also just make the verification function output out a new C++
version of the original function… For OpenRCT2, this meant we suddenly went from
~80% completion to 99.999% completion in one day!"
([OpenLoco v24.02, Feb 2024](https://openloco.io/news/2024/02/openloco-v24.02.html)).
The generator landed as
[PR #4567, 2016-10-09](https://github.com/OpenRCT2/OpenRCT2/pull/4567) — IntelOrca:
"Extended testpaint to generate the paint code for a roller coaster. This will save
us lots and lots and lots of time… And yes, all tests pass!" — producing ~10k lines
for the largest coaster, with stations, on-ride photos and some support functions
still done by hand. OpenLoco later reused it to replace ~400 hand-written functions
with two 10-line generators.

**For us:** BoF3 has exactly this shape of subsystem — battle action tables, script
opcode handlers, the PSX-packet draw path. Anything that is a large family of
near-identical table-driven functions is a candidate. Design the equivalence harness
(phase 3) so that "emit the C" is a short step from "check the C", not a rewrite.

### 2.4 Order of work

De-facto order, from the [v0.0.4 readme](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/v0.0.4/readme.md)
(Sept 2015): windowing/UI, then park and track file formats, then ride logic — with
map rendering, peep behaviour and vehicles explicitly still outstanding. That is
*loosely-coupled and eyeball-verifiable first, tightly-coupled simulation last*, and
testpaint exists precisely because the last category could not be eyeballed. PLAN
§3's ordering (platform layer → graphics → menus → battle → field/script) is the
same instinct and is corroborated.

One detail worth copying: **they patched the original executable's imports before
they replaced its code.** A 2015-08-18 commit is titled "patch exe to remove
DirectDraw, DirectInput and DirectPlay dependencies"
([commit history for `openrct2.exe`](https://github.com/OpenRCT2/OpenRCT2/commits/develop/openrct2.exe)).
Given our import surface is DDRAW/DSOUND/DINPUT/WINMM across 101 symbols (PLAN §1),
detaching the original binary from DirectX early — before the graphics code is ours
— is a cheap, separable win.

### 2.5 The replay system — their regression oracle, post-decompilation

Console commands record every game action to a `.sv6r` file and replay it; the
pass/fail signal is a **sprite checksum**, and PRs are tested by running a set of
replays against the new build
([Replay System wiki](https://github.com/OpenRCT2/OpenRCT2/wiki/Replay-System)).
Two documented caveats we should plan around: replays are architecture-dependent
(x86 and x64 produce different checksums, so only x64 is checked in CI), and an
*intentional* behaviour change requires re-recording and re-normalising the replay.
That second one is the trap — the golden file is regenerated by the very change it
is meant to police, so the harness cannot distinguish "we changed this" from "we
broke this" on its own. Our answer to that is `DIVERGENCE.md`; theirs is nothing.

This is also the shape our differential testing against the archival sibling
(PLAN §6) should take: a deterministic input script plus a state checksum, not a
frame diff.

### 2.6 Formats: the part they got wrong, and it cost six years

OpenRCT2 kept writing RCT2's `SV6` save format long after the code was theirs. The
replacement `.park` format was implemented in
[PR #10664](https://github.com/OpenRCT2/OpenRCT2/pull/10664) (merged 2021-11-21) and
shipped in [v0.4.0, 2022-04-25](https://github.com/OpenRCT2/OpenRCT2/releases/tag/v0.4.0),
whose release notes lead with "New save format with increased limits". Look at what
closed on that single day: 255 trains per ride (issue #714, opened 2015-01-21),
999×999 maps (#4933, opened 2016-12-28), mix-and-match path surfaces (#2253, opened
2015-11-08), cheats saved with the park (#3517, opened 2016-05-07). **Six years of
wanted changes were blocked by the container format, not by the code.**

Old `SV4`/`SV6` files still load (import-only); new saves are `.park`. The importer
is a translation layer with its own fidelity decisions, not a passthrough — e.g.
v0.4.0's "when importing SV6 files, the RCT1 land types are only added when they
were actually used". Forward compatibility is weak: opening a newer `.park` in an
older build reports generic corruption rather than the version message the code
contains ([issue #22229](https://github.com/OpenRCT2/OpenRCT2/issues/22229)).

Objects went the same way: binary `DAT` → JSON `.parkobj`
([PR #7310](https://github.com/OpenRCT2/OpenRCT2/pull/7310), merged 2018-04-08),
splitting the object's *definition* (JSON, in their own
[objects repo](https://github.com/OpenRCT2/objects)) from its *graphics* (still
imported from the original install). Eight years on both formats are still live in
the same lookup path, and a `DAT` shadows the newer `.parkobj` with the same id
([issue #18606](https://github.com/OpenRCT2/OpenRCT2/issues/18606)).

**Directly actionable for us:** PLAN §8 step 3 makes the `DAT/` container parser the
critical path. This is the evidence that it is the right call, and that the parser
should be paired early with *our own* save and container format, not just a reader
for Capcom's.

---

## 3. Pitfalls and things they'd do differently

**The tail after the cutover is longer than the cutover.** v0.0.5 (2016-12-27)
announced, in one sentence, "This is the first fully implemented version of
OpenRCT2. RCT2.EXE is no longer required." But the emulated globals block survived
another year — "Remove RCT2 interop" is
[PR #6772](https://github.com/OpenRCT2/OpenRCT2/pull/6772), merged 2017-12-04 — and
stray interop files were still being deleted in
[PR #23904](https://github.com/OpenRCT2/OpenRCT2/pull/23904) on 2025-03-01:
"These files were used back when OpenRCT2 could call into the original executable.
That hasn't been the case for several years now, so time to go." **Nine years of
residue.** OpenLoco, which declares the reimplementation complete, merged its
*final* "Remove interop" PR on 2025-11-27 after ~103 interop-related issues and PRs
([OpenLoco issue search](https://github.com/OpenLoco/OpenLoco/issues?q=interop)).

**The oracle outlives its usefulness and then blocks you.** Because testpaint still
called original code, the in-memory `Ride` struct had to stay `#pragma pack`ed and
byte-identical to RCT2's long after the game itself was standalone — the discussion
in [PR #6265](https://github.com/OpenRCT2/OpenRCT2/pull/6265) (2017) is explicitly
"TestPaint… does need `gRideList`, so `Ride` will have to stay `pack`ed for now."
And when it was finally removed, Gymnasiast's reason was blunt: "TestPaint served us
well, but it hasn't compiled for years, and before that it produced incorrect
results for years" ([PR #17333](https://github.com/OpenRCT2/OpenRCT2/pull/17333),
2022-06-05). An unmaintained differential harness is worse than none: it holds your
data structures hostage while silently lying.

**Globals that overlapped stop overlapping.** A lovely, very specific cutover bug:
under `RCT2_GLOBAL` two variables aliased the same original memory; compiled as real
C variables in the standalone build they did not, and macOS 64-bit rendered a black
screen. "I'm sure there are other issues like this still in the code, but this is
the one I found" ([PR #4463](https://github.com/OpenRCT2/OpenRCT2/pull/4463)). Expect
a class of bugs at our cutover that exists *only* because the original memory map was
doing work nobody documented.

**Their divergence record is a four-word tag and an unmaintained wiki page.** The tag
works: every fix to a pre-existing RCT2 bug is marked `(original bug)` in
[`distribution/changelog.txt`](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/develop/distribution/changelog.txt)
— e.g. v0.4.18's "Fix: [#1122] Trains spawned on a cable lift hill will fall down and
crash (original bug)." Cheap, and it survives because it rides on a file that has to
be edited anyway. **Worth adopting verbatim as a `DIVERGENCE.md` companion.** The
wiki page is the counter-example:
[Found bugs and limitations in RCT2](https://github.com/OpenRCT2/OpenRCT2/wiki/Found-bugs-and-limitations-in-RCT2)
catalogues roughly eight original bugs and records the disposition of *one*. That is
exactly the retrofit cost CLAUDE.md rule 2 predicts, observed in the wild.

**They committed a patched copy of the game's executable.** `openrct2.exe`
(6,750,208 bytes) is in the repository's initial commit of 2014-04-01 and was only
deleted on 2022-06-07, days after testpaint was removed. We must not do this
(CLAUDE.md rule 1) — but note what it implies operationally: their whole build for
eight years assumed a specific patched binary was *present*. Our equivalent has to
be a local, gitignored artifact plus a reproducible patch script, and that needs to
be designed in phase 0 rather than discovered later.

**Nothing was written down.** There is no decompilation-completion announcement, no
`.park` design document, no tracking issue for interop removal, and no forum post
for the v0.0.5 standalone release. The rationale for every major transition survives
only in PR descriptions. Our `docs/` convention and the evidence rule are the
correction to this, and this note is evidence that the correction is worth its cost.

**No retrospective exists.** Searched hard: no interview, AMA, conference talk,
podcast or postmortem by any core developer. The blog is release announcements. The
closest thing to a reflective document in this whole family of projects is the
OpenLoco v24.02 post. Treat §3 as *inferred* from artifacts, not as their own stated
regrets.

---

## 4. Where to go when blocked

| Our problem | Read this |
|---|---|
| Phase 0: designing the detour/interop layer | [`src/addresses.h` @ v0.0.3](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/v0.0.3/src/addresses.h) and [`src/openrct2/rct2/hook.c` @ v0.0.7](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/v0.0.7/src/openrct2/rct2/hook.c) — read for shape only; GPL, see §5 |
| Which call macro for a weird calling convention | [Decompiling Tips IDA](https://github.com/OpenRCT2/OpenRCT2/wiki/Decompiling-Tips-IDA) |
| "How do I know my C matches the original?" | [`test/testpaint` @ v0.2.0](https://github.com/OpenRCT2/OpenRCT2/tree/v0.2.0/test/testpaint), then [issue #360](https://github.com/OpenRCT2/OpenRCT2/issues/360) for what they did *before* they had it |
| The verifier-as-generator idea (PLAN §2B accelerator, cheaper version) | [OpenLoco v24.02 post](https://openloco.io/news/2024/02/openloco-v24.02.html) and [PR #4567](https://github.com/OpenRCT2/OpenRCT2/pull/4567) |
| PLAN §4 phase 4: can we go 64-bit before decompilation finishes? | [issue #4198](https://github.com/OpenRCT2/OpenRCT2/issues/4198) — the `.data`-segment relocation and the "code cannot cross" limit |
| Cutover bugs from the original memory map | [PR #4463](https://github.com/OpenRCT2/OpenRCT2/pull/4463), [PR #6265](https://github.com/OpenRCT2/OpenRCT2/pull/6265) |
| When and how to delete the interop layer | [PR #6627](https://github.com/OpenRCT2/OpenRCT2/pull/6627) → [PR #6772](https://github.com/OpenRCT2/OpenRCT2/pull/6772) → [PR #23904](https://github.com/OpenRCT2/OpenRCT2/pull/23904), and OpenLoco's ~103 interop PRs |
| Regression testing a living fork (PLAN §6) | [Replay System wiki](https://github.com/OpenRCT2/OpenRCT2/wiki/Replay-System) |
| Divergence bookkeeping (CLAUDE.md rule 2) | [`distribution/changelog.txt`](https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/develop/distribution/changelog.txt), grep `(original bug)` |
| PLAN §8 step 3: container format design | [PR #10664 (.park)](https://github.com/OpenRCT2/OpenRCT2/pull/10664), [PR #7310 (JSON objects)](https://github.com/OpenRCT2/OpenRCT2/pull/7310), [v0.4.0 notes](https://github.com/OpenRCT2/OpenRCT2/releases/tag/v0.4.0) |
| What "done" looks like | [Roadmap wiki](https://github.com/OpenRCT2/OpenRCT2/wiki/Roadmap) and [OpenGraphics](https://github.com/OpenRCT2/OpenGraphics) |

**The endgame, since the brief asks.** "100% decompiled" was not an ending and was
never even announced. Twelve years in, OpenRCT2 is at v0.5 and its **1.0 criterion
is "RCT2 not required (open graphics available)"**
([Roadmap](https://github.com/OpenRCT2/OpenRCT2/wiki/Roadmap)). Everything a player
cares about — multiplayer, the JavaScript plugin API (v0.3.0, 2020), the new save
format (v0.4.0, 2022), raised limits — happened *after* the code was theirs, and the
last remaining dependency is **data**, not code: `g1.dat` graphics, sounds, objects.
[OpenGraphics](https://github.com/OpenRCT2/OpenGraphics) exists to replace them and
is unfinished, blocked on the peep model and on sprite tooling; the engine-side
[asset pack manager](https://github.com/OpenRCT2/OpenRCT2/pull/18050) (v0.4.2, 2022)
shipped with audio packs only. Our PLAN §5 "renovation proper" is the right place to
put the value, and the honest read of OpenRCT2 is that phase 5 is not a phase — it
is the rest of the project's life.

---

## 5. Licence, and what it means for us

**OpenRCT2 is GPL-3.0-or-later.** Confirmed two ways: the GitHub API reports
`GPL-3.0`, and the README states "OpenRCT2 is licensed under the GNU General Public
License version 3 or (at your option) any later version"
([README §6](https://github.com/OpenRCT2/OpenRCT2#6-licence)).

**This repo is PolyForm Noncommercial. Reading is fine; vendoring is not.**

- **Reading their source, wiki, issues and PRs to learn technique: unrestricted.**
  Copyright does not cover ideas or methods. Everything in §2 is described as a
  technique for that reason, and no OpenRCT2 code is quoted here beyond identifier
  names.
- **Copying any OpenRCT2 code into this repo: do not.** GPLv3 requires the whole
  combined work to be distributed under GPLv3. PolyForm Noncommercial is not
  GPL-compatible — it adds a field-of-use restriction (noncommercial only), which
  GPLv3 §7 forbids as an additional restriction. So a GPL file in this tree cannot
  be relicensed and cannot legally coexist with our licence on distribution. This is
  a stricter situation than the `bof3ext` question in PLAN §7: MIT code *could* be
  vendored with its notice; GPL code cannot be, at all, without relicensing this
  whole project.
- **Contrast: OpenLoco is MIT** ([repo metadata](https://github.com/OpenLoco/OpenLoco)),
  so if we ever do want a vendored reference implementation of the hybrid interop
  pattern, OpenLoco is the licence-compatible place to look — subject to the same
  "read it, then write our own" discipline CLAUDE.md rule 5 imposes on `bof3ext`.
- **Practical rule for this project:** treat OpenRCT2 exactly as CLAUDE.md rule 5
  treats `bof3ext` — read freely, cite generously, vendor nothing — with the added
  note that here the licence makes "vendor nothing" a legal requirement rather than
  a policy choice.

Nothing in this document is derived from RCT2 game data, and none is reproduced.

---

## 6. What I could not verify

- **The date the last original function was replaced.** Secondary sources
  ([rct.fandom.com](https://rct.fandom.com/wiki/OpenRCT2)) give 2015-10-15, but the
  September-2015 v0.0.4 readme still lists map rendering, peeps and vehicles as
  outstanding, and the standalone build did not ship until v0.0.5 on 2016-12-27.
  The 2015 date has **no primary source** and looks wrong or much narrower than
  stated. Use the v0.0.5 release date as the only citable cutover.
- **What broke at the cutover.** The v0.0.5 notes contain no "known issues" section
  and I found no retrospective on regressions. The bugs cited in §3 (#4463, #6265)
  are ones I found by search, not a complete picture. Assume there were more.
- **Whether the "~80% → 99.999% in one day" figure is project-wide or
  paint-subsystem-wide.** The OpenLoco post says "For OpenRCT2, this meant we
  suddenly went from ~80% completion to 99.999% completion in one day"; the context
  is track paint functions, and PR #4567's own description is scoped to ride paint
  code. Read the figure as *spectacular within one subsystem*, not as a claim about
  the whole binary.
- **Whether a "vanilla behaviour" toggle exists.** I found none in the wiki, the
  cheats page, or the changelog, but I did not read `config.ini` handling in source.
- **What the 6,750,208-byte `openrct2.exe` in their repo root actually contained.**
  Commit titles make clear it was a patched build artifact derived from the original
  game; I did not download or inspect it, and deliberately will not.
- **Any first-person retrospective.** None found (see §3). If one exists it is in
  their Discord or the now-archived forums, neither of which is publicly indexed.
- **First ARM port date** — listed as supported, no date found.
