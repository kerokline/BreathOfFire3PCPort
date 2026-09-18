# Breath of Fire III — Chinese PC Port: renovation & uplift plan

**Status:** DRAFT (2026-09-18). Scoping document, no code yet.

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

**C, on the substrate of A, with B as an optional accelerator.**

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

You have **1,026 named boot-EXE functions plus 29,036 overlay functions**
already mapped on the PSX side, with `names/functions.toml`,
`names/overlays.toml`, `symbols.toml`, and `tools/name_map.py` to manage them. A
matcher that transfers even 30% of those names onto the PC binary turns a
nameless 3,000-function blob into a *substantially annotated* one on day one —
and that is the single biggest determinant of how fast decompilation goes.

This is the part of the plan that is genuinely novel and that only you are
positioned to do. It should be built early, because everything downstream gets
cheaper.

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

## 4. What transfers from `bof3ext`, and what does not

Read `bof3ext/CLAUDE.md` and `bof3ext/docs/` first; it is unusually well
documented for a hook project.

**Take directly:**

| Asset | Where | Note |
|---|---|---|
| The address corpus | `bof3ext/src/bof3/*.ixx` | ~11 modules of pure declarations: `Func<0x497740, …>`, struct layouts, global accessors. This *is* a partial symbol table for the PC binary. First thing to import. |
| Hook primitives | `src/helpers.ixx` | `Func`/`Accessor` templates with compile-time signature checking, `WriteProtectedMemory`, `WriteCall`, `WriteNops` |
| The injection substrate | `loader/` | Proxy `ddraw.dll` → `loader.cfg` → your DLL. Works, ships, uninstalls by deleting one file. |
| OpenGL PSX-packet renderer | `src/renderer.ixx`, `res/psx_shader.*.glsl` | 4bpp/8bpp texture pages + CLUT in the fragment shader. Solves the hardest graphics problem for you. |
| Text / glyph pipeline | `text_manager.ixx`, `glyph_manager.ixx` + `bof3ext_resources` | FreeType at `renderScale`×240p, the `{{TAG}}` control-code markup, 360 resource files |
| The caller-finding trick | `DrawStringHook` in `render_hooks.ixx` | `std::stacktrace::current()` on untranslated text to find the responsible function. Generalises into a superb decompilation work-queue generator. |

**Do not take:**

- **C++23 named modules (`.ixx`, `import std;`).** MSVC-only by construction,
  and the whole point of this project is to *leave* MSVC-only. `bof3ext`'s own
  `docs/building.md` says adding a non-MSVC path "is a real project". For a
  portability-targeted renovation, start on CMake + plain C++20 and pay that
  cost on day one instead of at the end.
- **`bof3ext`'s deliberate 32-bit assumptions** where they are cosmetic —
  though note the *load-bearing* ones (casting pointers through `uint32_t`) stay
  true until the original `.exe` is fully replaced.

**Explicitly does not transfer from the PSX side** (per the cross-reference
doc, already investigated — do not re-litigate):

- **The `DAT/` archives are repacked.** 742 `.DAT` files whose names echo the
  `.EMI` families, but `AREA000.DAT` has a `(offset,size)` TOC at `0x198`, no
  `MATH_TBL` magic, and subfile 0 is a **RIFF WAVE** — the port decompressed the
  PSX audio and re-containered everything. `tools/emi.py` will not read these.
  You need a new container parser, and it is a small, well-defined job.
- **The Chinese script is not economically harvestable** — port-specific
  encoding (`((c|0x8000)>>8) | ((c&0xFF)<<8)`), resolved through a PC-only index
  split. And it is a translation of a translation; the official US script is
  better and already aligned slot-for-slot.

---

## 5. A phased path

Each phase ends with something playable and something proven. Nothing here is
speculative about phase N+1 succeeding.

### Phase 0 — Substrate (weeks)
Fork or re-found the injection layer on portable toolchain. CMake, clang-cl and
MSVC both green, no C++ modules. Import `bof3ext`'s `src/bof3/*.ixx` address
corpus as plain headers. Get the game launching under your own DLL with the
existing renderer and text pipeline intact. **Exit test:** a build that is
byte-behaviour-identical to a `bof3ext` release, built by a non-MSVC compiler.

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
| Code shape | Lifted MIPS C, unreadable by design, 30k functions mapped | Path to readable C++ |
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

  Still a phase-0 prerequisite, not an afterthought — and collaboration remains
  the obviously better outcome, since the author's stated roadmap (finish
  widescreen, fix fullscreen, modding API) is a subset of this plan.
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

1. **Contact TheRealBiggs** (§7) — ask him to add the `LICENSE` file that is
   missing from `bof3ext`, and open the collaboration conversation. Gating for
   anything vendored, and likely a short conversation given his other repos are
   already MIT.
2. **Prototype the §3 matcher on one subsystem** — the text engine is ideal,
   because the cross-reference doc has already established three matched
   landmarks there and both sides are well documented. If name transfer works on
   the text engine, phase 1 is real; if it does not, the plan needs rethinking
   before any code is written.
3. **Write the `DAT/` container parser** (§4). Small, self-contained, needed by
   everything downstream, and testable against the 742 files today.

Step 2 is the load-bearing experiment. Do it before committing to phases 0–2.
