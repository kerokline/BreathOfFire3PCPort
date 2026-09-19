# Breath of Fire III — Chinese PC Port: renovation & uplift plan

**Status:** DRAFT (2026-09-18). Scoping document, no code yet.

**Update 2026-09-18:** §8 step 2 — the load-bearing experiment — has been run and
**passed**, twice (text engine, then battle engine). See [`kinship-probe-text-engine.md`](kinship-probe-text-engine.md).
It also changes the shape of §3: the strongest matching signal is not call-graph
isomorphism but *global block layout*, which the port preserves exactly at a
per-block constant delta. Eight text-engine functions are named in
[`../symbols.toml`](../symbols.toml) as a result, and the probe answered the
sibling repo's open `0x0C` question from the PC side.

**Goal, as stated:** a **living game**, not an archival one. A *renovation* of
the 2001 Chinese PC port — fix its bugs, modernise its platform, and be able to
**change and extend game logic**. The sibling project
[`BreathOfFire3Recomp`](../../BreathOfFire3Recomp) is the archival counterpart
and stays that way; §6 explains why that distinction, rather than any judgement
about which version plays better, is what selects the PC port as this project's
base — and why it makes the two projects unusually good collaborators.

That goal is also what decides the architecture, and it rules out the obvious
answer: a technique that preserves behaviour by construction cannot host a
project whose deliverable is deliberate divergence.

---

## 1. What the target actually is

Measured on `bof3ext/bof3/BOF3.exe` (2,584,576 bytes, 2001-04-18), 2026-09-18.

| Property | Value | Why it matters |
|---|---|---|
| Machine / subsystem | i386, GUI | 32-bit x86, native code — **there is no CPU to emulate** |
| Linker | MSVC 6.0, `/FIXED` (no `.reloc`) | Image base `0x400000` is guaranteed; every absolute address in `bof3ext` is stable by construction |
| Packing | none | Clean linear `.text`, no unpacker stub to defeat |
| Sections | `.text` 1.80 MB @ `0x401000`, `.rdata` 87 KB, `.data` 3.39 MB virtual / 624 KB raw, `.rsrc` 3 KB | ~2.8 MB of that `.data` is BSS — the game's working state |
| Entry point | `0x5BA057` (CRT `mainCRTStartup`) | |
| Instructions in `.text` | ~590,900 (full sweep, 100% decodable, 119 stray data bytes) | Real but finite: roughly the size of a mid-90s C codebase |
| `ret` instructions | 14,968 | |
| Distinct direct-call targets | 3,276 | Order-of-magnitude function count: **3–5k** |
| Imports | **101 symbols across 7 DLLs** | See below — this is the headline number |
| SEH (`fs:` refs) | 37, all in CRT | No exception-heavy code to model |
| x87 FP instructions | ~10,800 | Present, must be handled; no MMX/SSE |

### The import surface is tiny

```
DDRAW.dll    (2)  DirectDrawEnumerateA, DirectDrawCreate
DSOUND.dll   (1)  ordinal #1 (DirectSoundCreate)
DINPUT.dll   (1)  DirectInputCreateA
WINMM.dll    (1)  mciSendStringA            (AVI / CD audio)
USER32.dll  (22)  window, message pump, cursor
GDI32.dll    (5)  GetDeviceCaps, TextOutA, SetTextColor, SetBkMode, GetStockObject
ole32.dll    (1)  CoInitialize
KERNEL32.dll(68)  MSVC6 CRT support + file I/O + heap
```

Seven DLLs. No networking, no threading beyond critical sections, no COM beyond
the three DirectX creates. **The entire host platform you would have to
reimplement to free this game from Windows fits on one page.** That is an
unusually good position — far better than a typical Windows game of the era.

### The one hard part: indirect control flow

| | count | shape |
|---|---|---|
| Indirect `call` | ~1,392 | 738 reg-based memory (function-pointer tables / state machines), 280 absolute `[imm]` (imports + global fn pointers), 259 scaled `[reg*4+imm]` (switch/vtable), 109 register |
| Indirect `jmp` | ~1,542 | mostly tail-call and jump-table shapes |

Concretely: **recursive descent from the entry point reaches only 309
functions** out of 3,000+. The game is a function-pointer machine — area
scripts, battle actions, and menu states all dispatch through tables. None of
`GetText` (`0x497740`), `LoadDialogue` (`0x4976D0`), or `LoadDatFile`
(`0x454590`) is reachable from `main` by direct calls alone.

This is not a blocker; it is a *seeding* problem, and it is **exactly the
problem `psxrecomp` already solved for the MIPS side** (`seeds/ghidra_funcs.txt`,
`docs/FUNCTION_DISCOVERY.md`). The same playbook applies: Ghidra headless export
for the first pass, then function-pointer table recovery, then runtime harvest
of any PC that still falls through.

---

## 2. Three candidate architectures

### A. Deepen `bof3ext` — hook and patch in place

Keep the proxy-`ddraw.dll` + MinHook model, write more hooks.

- **Gets you:** immediate results, on a proven base that is already ~80%
  translated with an OpenGL renderer and FreeType text.
- **Ceiling:** permanently 32-bit, permanently Windows, permanently the shipped
  `.exe`. Every change is address surgery. You cannot restructure a system, only
  intercept it. "Extend game logic" is possible only where a hook boundary
  happens to exist.
- **Verdict:** necessary as a substrate, insufficient as a destination.

### B. Static recompilation, x86 → C (the literal reading of "recomp")

Lift `.text` to C the way `psxrecomp` lifts MIPS, link against a new host layer.

- **Gets you:** portable, 64-bit-capable, always-runnable native build with no
  Windows dependency. Bugs in the *platform* (DirectDraw, fullscreen, resolution)
  evaporate because you rewrote the platform.
- **Costs:** x86 is materially harder to lift than MIPS — variable-length
  encoding, no clean function boundaries, EFLAGS dataflow, x87's 80-bit stack
  semantics, and 3,000 indirect dispatch sites. Off-the-shelf lifters are not a
  real option (`mcsema` is archived, `retdec` unmaintained, `remill` is a library
  not a pipeline); you would be writing a lifter.
- **The fatal flaw for your stated goal:** lifted C is *unreadable*. Changing
  game logic in a sea of `eax = eax + 4; cpu_write32(...)` is barely easier than
  hooking. Option B does not actually deliver requirement #1.

### C. Incremental decompilation to a hybrid binary — **recommended**

The OpenRCT2 / OpenLoco model, which is the proven path for exactly this class
of project (an x86 PC game of this era, renovated into maintainable source):

1. Start with the original `.exe` loaded in-process, as `bof3ext` already does.
2. Pick one function. Write a readable C/C++ reimplementation. Redirect the
   original address to it via the existing hook mechanism.
3. Prove equivalence, then move on. The game is **playable at every commit**.
4. When every function in a subsystem is reimplemented, delete the original code
   path and the subsystem becomes ordinary source you can refactor, port, and
   extend.
5. When the last function is replaced, the original `.exe` is no longer loaded
   and you have a free-standing, portable, 64-bit game.

- **Gets you:** readable source, incrementally, without ever having a broken
  build. Extending game logic is trivial in any subsystem you have landed.
- **Costs:** it is long. 3–5k functions, though a large fraction is MSVC6 CRT
  and DirectX glue you replace wholesale rather than decompile.

### Recommendation

**C, with B as an optional accelerator. A is a peer approach, not our base** —
see §4.

Option C needs a way to get our code into the process and redirect individual
addresses to our reimplementations. That mechanism is a loader plus a detour
library — a few hundred lines over MinHook — and it is *scaffolding we erect in
order to dismantle it*. Every function we land makes it matter less, and the
endpoint of this project is a build where none of it exists. Do not confuse
needing that mechanism with building on someone else's implementation of it.

The accelerator idea is worth stating explicitly: a *crude* x86→C lifter is far
easier than a good one, because in model C it never has to be pretty — only
correct. Lifted-but-ugly C for a not-yet-decompiled function is strictly better
than the original machine code, because it compiles for x86-64 and for other
platforms. That converts the "replace the last 2,000 boring functions" tail from
a decade of hand work into a mechanical pass, and lets you reach a portable
64-bit build *long before* decompilation finishes. Treat it as a phase-4 option,
not a prerequisite.

---

## 3. The asset nobody else has: your PSX recomp as a Rosetta stone

`BreathOfFire3Recomp/docs/PC_PORT_CROSS_REFERENCE.md` already establishes that
the PC port is not a rewrite — it is **the same C source recompiled for x86**,
keeping the PlayStation data shapes and even the PSX GPU packet model (replayed
through DirectDraw). The doc proves kinship at three independent points:

| Thing | PSX | PC port |
|---|---|---|
| Character record stride | `0x80144964 + 0xA4·n` | `0x64B390 + 164·n` — **the same 164 bytes** |
| Current area number | `0x80143F00` | `0x904EFC` |
| Message open → box re-point | `Msg_OpenScript` → `MsgBox_Reset` | `0x4976D0` → `0x7DEE4C` → `0x497770` |

...and the traffic already flows both ways: the port's control-code table
produced **two corrections** to `TEXT_ENGINE.md`, and its `descriptionId` field
name prompted the proof that our `ref` is a description id.

**The leverage: if two binaries are compilations of one source, their call
graphs are isomorphic up to inlining.** That makes automated function matching
tractable, using signals that survive recompilation across architectures:

- call-graph shape (in-degree / out-degree / SCC structure)
- referenced constants and struct strides (`0xA4`, `0x118`, `0x20`, table sizes)
- string/table reference patterns into known data (the `names/*.toml` corpus)
- leaf-function fingerprints (arithmetic identities are architecture-neutral)

The PSX side has a name corpus to transfer from, managed with
`names/functions.toml`, `names/overlays.toml`, `symbols.toml` and
`tools/name_map.py`. **Counted 2026-09-18** (see
[`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md)):

| | count |
|---|---|
| Named boot-EXE functions (`symbols.toml`) | **556** |
| Named overlay functions (`names/functions.toml`) | **121**, across 8 of 406 overlays |
| Statically discovered overlay function entries | **5,805** — structure, not names |
| Overlays with a human role (`names/overlays.toml`) | 216 of 406 |

An earlier revision of this section said "1,026 named boot-EXE functions plus
29,036 overlay functions". **Both figures were wrong**, inherited from one prose
sentence in the sibling's `PC_PORT_CROSS_REFERENCE.md` that no artifact on disk
supports. The corpus is roughly 677 names, not 30,000 — which does not
invalidate the matcher (the probes measured the *method*, and it works), but it
does mean name transfer annotates a useful fraction of the PC binary rather than
most of it, and that structure — which overlay a function lived in, and what
that overlay does — may be worth more than the names.

This is also the evidence rule doing its job: the number survived five documents
because it was repeated rather than counted.

This is the part of the plan that is genuinely novel and that only you are
positioned to do. It should be built early, because everything downstream gets
cheaper.

**Established 2026-09-18** ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)): the kinship
is not a hypothesis any more, and it reaches further than this section assumed.
Globals keep their layout *within* a block and are reordered *between* blocks —
the signature of per-translation-unit static allocation. So the source's **file
decomposition** survived into both binaries, and clustering PC globals into
blocks is a route to recovering which `.c` file each function belonged to. For a
project whose deliverable is maintainable source, starting from Capcom's own file
boundaries beats inventing our own; make it an explicit phase-1 output.

### Reciprocity

The flow is not one-way, and the PC port is the better source for some things:

- **Field names for PSX gaps** — `SkillData.examineChance`, `WeaponData.element`
  / `weight`, the equipment-id block ordering (§3 of the cross-reference doc).
- **A PC-side answer to `0x801490CA`** — the open `0x0C` question.
- **`bof3ext_resources` as an English menu corpus** — 360 files of item, weapon,
  armour, accessory and skill names, menu tabs and categories, written without a
  JP byte budget. Exactly the categories the PSX translation scope deferred.
  *(Licence unclear — see §7.)*

---

## 4. `bof3ext` is a peer project, not a base

The foundation of this project is **Capcom's binary**. Everything else in the
picture is an independent derivative of one of two Capcom artifacts:

```
   BOF3.exe (Capcom, 2001, x86)          SLPS-00990 (Capcom, 1997, MIPS)
        /              \                              |
   bof3ext        THIS PROJECT                BreathOfFire3Recomp
  (hook and         (replace                      (archival
   translate         entirely)                 recompilation)
   in place)
```

`bof3ext` and this project are **siblings**, not a stack. They share an
upstream, not a lineage. And their goals diverge at the root: `bof3ext` exists
to make the shipped executable better, and this project exists to make the
shipped executable *unnecessary*. Its whole value proposition is the thing we
intend to delete.

That is not a criticism of it — it is very good at what it does, and its
`docs/` is unusually thorough for a hook project. Read all of it. But read it as
**prior art from a peer**, which is a different relationship from a dependency,
and it has practical consequences in three places:

### What is actually valuable is knowledge, not code

| | |
|---|---|
| **The address corpus** (`src/bof3/*.ixx`) | ~11 modules of pure declarations — `Func<0x497740, …>`, struct layouts, global accessors. The genuinely valuable thing in the repo for us. |
| **The control-code table** (`docs/translation.md`) | An independent derivation of the message vocabulary; already produced two corrections to the sibling repo's `TEXT_ENGINE.md`. |
| **The caller-finding technique** | `DrawStringHook` takes `std::stacktrace::current()` on untranslated text to find which function is responsible. A *technique*, not code — and it generalises into an excellent decompilation work-queue generator. |

Note what these have in common: they are **facts about Capcom's binary**, which
that team discovered and we can rediscover. `GetText` is at `0x497740` because
Capcom's linker put it there, not because anyone invented it. Ghidra plus the
matcher in §3 gets us the same table.

And our own evidence rule means we would verify every one of those addresses
independently regardless of where we first read it — which is exactly the
discipline the sibling repo's `PC_PORT_CROSS_REFERENCE.md` already applies:
*the port is corroboration, never authority.* The right posture toward
`bof3ext`'s findings is the one already established for them. Cite them, credit
them, verify them.

### What we would have taken, and now would not

- **The OpenGL PSX-packet renderer** (`src/renderer.ixx`, `psx_shader.*.glsl`).
  Impressive work — 4bpp/8bpp texture pages and CLUT lookups in a fragment
  shader. But it exists to *replay GPU packets that the original code builds*.
  The moment we reimplement the graphics layer, we stop building those packets
  and draw directly. It is a crutch with a guaranteed expiry date, and adopting
  it would mean inheriting the packet model we are trying to escape.
- **The `ddraw.dll` proxy shape.** Proxying DirectDraw is how you inject into a
  game you do not control. We intend to control it. That shape is a constraint
  inherited from their goal, not ours.
- **C++23 named modules.** MSVC-only by construction, and the point of this
  project is to *leave* MSVC-only. `bof3ext`'s own `docs/building.md` says
  adding a non-MSVC path "is a real project" — so pay that cost on day one
  rather than at the end.
- **The translated script.** A translation of a Chinese translation. The
  official US script the sibling repo has aligned slot-for-slot is better.
  (`bof3ext_resources`' **menu and name text** is a different matter — see §3.)

### Which makes licensing much less load-bearing than §7 originally implied

If we are not vendoring their code, the MIT question stops gating the project.
It still matters — for anything we *do* vendor, and as a plain courtesy to
someone whose documentation saved us real time. But it moves from prerequisite
to good manners plus a narrow legal check, and §7 has been rewritten to say so.

### Where this leaves the relationship

Peer projects with a shared upstream and non-overlapping endpoints, which is a
good position for exchange rather than an awkward one. We have a ~677-name
corpus on the PSX side (§3) and a matcher that may name large parts of the PC
binary (§3); they have years of accumulated knowledge of that binary's
behaviour. Neither needs to adopt the other's architecture to trade findings,
and neither is obliged to. That is worth saying out loud when making contact:
this is not a fork of their work and does not compete with it.

---

## 4a. What does not transfer from the PSX side

**The archives are re-containered, and only the audio is re-encoded.** `DAT/`
holds 742 `.DAT` files, one per `.EMI`. Parsed 2026-09-19
([`DAT_CONTAINER.md`](DAT_CONTAINER.md), `tools/dat.py`): a flat stream of
16-byte-header chunks, unencrypted; PSX audio groups became banks of RIFF WAVs
and the MIPS overlay section was dropped, but images and most data sections are
**byte-identical to the JP disc** (2,120 of 2,680 paired sections, full census). So a byte-level asset path *does* exist for everything but audio —
an earlier revision of this paragraph said otherwise from one file's header.

**The Chinese script is not economically harvestable.** Port-specific encoding
(`((c|0x8000)>>8) | ((c&0xFF)<<8)`), resolved at runtime through `GetText`
`0x497740` with a PC-only index split that has no PSX counterpart. Extracting
it means writing both a container parser and a decoder to obtain a JP→ZH
translation, when the official US script is already aligned slot-for-slot
([`LOCALIZATION_APPLY.md`](../../BreathOfFire3Recomp/docs/LOCALIZATION_APPLY.md)).

**The rendering work is inapplicable in both directions.** The sibling emulates
the GPU properly; `bof3ext` re-emulates texture pages in a shader because the
port replays packets through DirectDraw. This project ends up doing neither.

---

## 5. A phased path

Each phase ends with something playable and something proven. Nothing here is
speculative about phase N+1 succeeding.

### Phase 0 — Our own scaffolding (weeks)
Write a minimal loader and detour layer of our own: get code into the process,
redirect one address to one of our functions, call the original. CMake, clang-cl
and MSVC both green, plain C++20, no modules. Deliberately thin — this is
scaffolding built to be dismantled (§2), and every hour spent making it elegant
is an hour spent on something with a guaranteed expiry date.

Build our own symbol header from Ghidra output rather than importing anyone's
(§4). `bof3ext`'s corpus is a cross-check on ours, not its source.

**Exit test:** one function of Capcom's binary replaced by one function of ours,
under a non-MSVC compiler, with the game still running.

### Phase 1 — Symbol foundation (weeks)
Ghidra headless on `BOF3.exe`. Export functions, jump tables, and
function-pointer tables. Build the PSX↔PC matcher described in §3 and transfer
names from `names/*.toml` + `symbols.toml`. Stand up a `symbols.toml` of the
same shape for the PC binary so `sync_symbols.py`-style tooling works on both
sides. **Exit test:** a named function count, honestly measured, with an
evidence tier per name (`evidence` / `hypothesis` / `unnamed`) — the same
discipline `NAME_MAP.md` already imposes.

### Phase 2 — Instrumentation and coverage (weeks)
Runtime call tracing from the injected DLL: which functions execute, how often,
in which game state. Generalise the `DrawStringHook` stacktrace trick into a
general "who called this" harvester. Feed it with play sessions the way
`area_poller.py watch` already does on the PSX side, including the Chao2
coverage estimator from `pc_coverage.py` — you will want the same honest answer
to "how much of the game have I actually seen".
**Exit test:** a ranked work queue — functions by (frequency × subsystem), so
decompilation targets the hot, well-understood core first.

### Phase 3 — Incremental decompilation (long; the main body of work)
Function by function, per §2C. Order: platform layer first (windowing, input,
file I/O, DirectDraw replay) because it is the least game-specific and buys
portability fastest; then the PSX-packet graphics layer; then menus; then
battle; then field/script. Each replaced function gets an equivalence test.
**Exit test per subsystem:** the original code path deleted, the subsystem
refactorable, and every behavioural difference introduced along the way present
in the divergence ledger (§6) — nothing drifted by accident.

### Phase 4 — Portability cutover (optional accelerator)
Once the platform layer is yours, the remaining original machine code is the
only thing pinning you to x86-32 Windows. Either finish decompiling it, or lift
it crudely (§2B) to reach a portable 64-bit build early. Decompilation then
continues *replacing lifted C* rather than machine code, with no loss of
momentum.

### Phase 5 — Renovation proper
The actual goal: bug fixes with real source, finished widescreen, working
fullscreen, resolution independence, the modding API `bof3ext`'s README wants,
and extended game logic. This phase is only cheap because phases 0–4 made it so.

---

## 6. Why the PC port is the base: archival vs. living

This is not a decision to weigh. It follows from what the two codebases *are*.

**`psxrecomp` is an archival framework, by constitution.** Its rules are
preservation rules:

- *"No per-game hacks during foundation work. If the recompiler or the hardware
  simulation is wrong, fix **that**."*
- *"Evidence over assertion. Accuracy claims are cross-referenced against an
  external comparative"* — psx-spx, the in-tree Beetle oracle, DuckStation,
  hardware test ROMs.
- A faithful core proven first; enhancement legitimate only afterwards.

Every one of those encodes the same commitment: **the output should be
equivalent to the original, and any divergence is a defect.** Modding hooks and
display quality-of-life sit *on top* of that invariant without weakening it.
That is exactly right for preserving a PlayStation game as it was, and it is
load-bearing — relax it and the framework stops being trustworthy for every
other title built on it.

**A living game inverts the invariant.** Divergence is the product. You want to
rebalance, fix *design* bugs and not just port bugs, restructure systems, add
content. The original's behaviour is a starting point, not an oracle. A codebase
whose correctness criterion is "matches the original" cannot host that work
without one of the two goals corrupting the other.

So the two projects can never merge — and that is fine, because it makes them
unusually good collaborators:

| | `BreathOfFire3Recomp` (archival) | This project (living) |
|---|---|---|
| Correctness criterion | Matches original hardware | Matches *intent*, deliberately chosen |
| Divergence | A defect to be root-caused | The deliverable |
| Owns | What the game **was** — data truth, script alignment, field semantics | What the game **becomes** — editable source |
| Code shape | Lifted MIPS C, unreadable by design, ~677 functions named (§3) | Path to readable C++ |
| Role to the other | Reference implementation and name donor | Independent corroboration, field names, English menu corpus |

### The consequence worth designing around

An archival build is the ideal **regression oracle for a living fork.** You have
a provably faithful BoF3 sitting in the next directory. That means every
behavioural difference in this project can be detected and attributed — you
always know exactly how far you have drifted from the original, and whether each
step of drift was on purpose.

Most renovation projects lose this. They diverge gradually, nobody records
which changes were intentional, and five years in nobody can say whether a given
oddity is a fix, a regression, or original behaviour. With the archival build as
a control, that failure mode is avoidable — but only if you build for it
deliberately:

- **A divergence ledger.** Every intentional behavioural change gets an entry:
  what the original did, what this does, and why. Not a changelog for players —
  an engineering record that keeps "we changed this" and "we broke this"
  distinguishable forever.
- **Differential testing against the PSX build** wherever a shared input exists
  — damage formulas, encounter tables, script control-code handling, RNG
  sequences. These are the same algorithms in both binaries (§3), so they can be
  run head-to-head.
- **Unledgered divergence is a bug**, by definition. That single rule is what
  keeps a living fork from becoming an untraceable mess, and it costs nothing to
  adopt on day one and a great deal to retrofit later.

This also explains why §2's option B is the wrong shape for this project and not
merely inconvenient: static recompilation *preserves behaviour by construction*.
It is an archival technique. Reaching for it as a foundation would be building a
second archival project by accident.

### One risk the framing exposes

Phases 0–2 deliver **nothing a player can see.** `bof3ext` today ships real
player value; a renovation that goes dark for months to build symbol
infrastructure is at risk of stalling on enthusiasm alone. Worth deliberately
interleaving visible wins — the fullscreen bug, resolution handling, the
remaining untranslated location bubbles — into the early phases even though the
plan does not technically need them yet. A living project has to stay alive.

---

## 7. Constraints and hazards

- **`bof3ext` ships no `LICENSE` file — but the intent is almost certainly
  MIT.** TheRealBiggs licenses his other repositories under MIT
  (`Copyright (c) 2025 Biggs McWedge`), so the omission here reads as an
  oversight rather than a deliberate reservation of rights. That matters, but it
  does not change what to do today: **absent a licence file the legal default is
  all-rights-reserved**, and a pattern of licensing other work permissively is
  evidence of intent, not a grant. Read the repo freely; vendor nothing until
  the file exists.

  The good news is that this makes the conversation easy — the ask is "would you
  add the licence you meant to?", not a negotiation over terms. And MIT is
  permissive enough for everything in this plan: use, modify, distribute,
  sublicense, with attribution. It is compatible with this repo's PolyForm
  Noncommercial licence in the direction that matters (MIT code can live inside
  a more restrictively licensed project as long as its notice is retained), so
  vendored material would carry its MIT notice in a `THIRD_PARTY.md`.

  **This is much less load-bearing than it first appeared** (§4). The plan does
  not vendor their code — the valuable material is *facts about Capcom's
  binary*, which we rediscover with Ghidra and must verify independently under
  our own evidence rule regardless of where we first read them. So licensing
  gates only what we would actually copy, and the current answer is "nothing".

  What remains is narrower and mostly a matter of conduct: credit their prior
  work where it corroborates ours, ask about the missing licence file before
  vendoring anything, and be clear when making contact that this is **not a
  fork of `bof3ext` and does not compete with it** — different endpoint,
  different architecture, shared upstream. Their roadmap (finish widescreen,
  fix fullscreen, modding API) overlaps this plan's *goals* while sharing none
  of its *code*, which makes exchange of findings easy and obligation-free in
  both directions.
- **Never commit game data.** `BOF3.exe`, `DAT/`, `BGM/`, `SND/`, the disc
  image, the AVIs. Same rule as both existing repos, and it covers pastes into
  docs and commit messages, not just files.
- **Addresses are load-bearing constants.** `bof3ext`'s hard rule #3 applies
  here unchanged and gets *more* dangerous as the codebase grows.
- **Keep "what it was" and "what it should be" separately answerable.** The
  archival rule — *the port is corroboration, never authority; where the PC port
  and the PSX disc disagree, the disc decides* — still governs the first
  question, and it has already been exercised twice, both times with the port
  moving the needle. But it no longer settles the second. A living project
  answers "what was the original behaviour?" from the disc and "what should this
  game do?" for itself, and it must never let the second answer quietly
  overwrite the record of the first. That is what the divergence ledger (§6) is
  for.
- **Unledgered divergence is a bug.** Adopt on day one; it is very expensive to
  retrofit. See §6.
- **No stubs.** Inherited from `psxrecomp/CLAUDE.md`. A decompiled function is
  fully implemented or it aborts loudly. `return 0;` in a replaced function is a
  silent behavioural fork that will cost weeks to find.

---

## 8. Immediate next steps

1. **Contact TheRealBiggs** (§7) — as a peer, not a downstream. Credit the
   prior work, flag the missing `LICENSE` file, offer findings back. Not gating
   on anything, since the plan vendors nothing (§4); worth doing early anyway,
   because the exchange of reverse-engineering knowledge is the part where both
   projects genuinely gain.
2. ~~**Prototype the §3 matcher on one subsystem**~~ — **done 2026-09-18, and it
   passed** ([`kinship-probe-text-engine.md`](kinship-probe-text-engine.md)).
   Eight functions, four global blocks, and a correction to how §3 should work:
   propagate block deltas, then use call-graph shape as the check.

   The successor probe — **whether it scales, and whether the overlay corpus
   transfers** — was run the same day on the battle engine and also passed
   ([`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md)). Every
   function matched there is PSX overlay-resident, from two different overlays,
   so the method does not care whether a PSX function was boot-resident. (What
   the overlay corpus is *worth* is a separate question, and smaller than this
   section once assumed — see above.) It also found a **second anchor needing no seed**: constant
   data tables transfer by value (width-agnostically), so searching for a known
   PSX table names the PC functions that reference it.

   **Ghidra BSim was then measured against those same pairs**
   ([`bsim-evaluation.md`](bsim-evaluation.md)): it matches across MIPS→x86
   (4 of 8 true pairs at rank 1, `MsgBox_Reset` at a perfect 1.000, and a
   negative control that correctly failed), but it misses tiny wrappers and very
   large functions, and it produced one *high-confidence wrong* answer. So phase
   1's matcher is **BSim proposes, block deltas dispose** — BSim needs no seed
   and gets us into a subsystem cold, delta propagation then names a whole block
   at once, and each checks the other. Neither is trusted alone, and no
   BSim-derived name exceeds `hypothesis` tier without a PC-side read.
3. ~~**Write the `DAT/` container parser**~~ — **done 2026-09-19**
   ([`DAT_CONTAINER.md`](DAT_CONTAINER.md)); 742 of 742 files parse. The
   follow-on is the full EMI↔DAT census.

Step 2 was the load-bearing experiment. It passed, so phases 0–2 are committed
to. The current order of work is in [`STATUS.md`](STATUS.md).
