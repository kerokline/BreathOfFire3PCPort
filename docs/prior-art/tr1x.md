# Prior art: TR1X / Tomb1Main (now TRX)

**Status:** DRAFT (researched 2026-09-18)

Research note on the closest living prior art to this project. Everything below
was read from the project's own repository, docs and changelogs on 2026-09-18;
each claim links to what it came from. Section 6 lists what I could not
establish.

---

## 1. What it is

TR1X is a reimplementation of the 1996 PC *Tomb Raider* that was built by
injecting a DLL into the shipped executable and replacing its functions one at a
time with readable C, until none of the original binary was left. It is now part
of **TRX**, a single engine covering TR1, TR2 and TR3 with TR4 in progress; the
name went `TR1Main` (2021) → `Tomb1Main` → `TR1X` (2023) → `TRX`, and the former
`TR2X` and `libtrx` repositories were folded into it
([GLOSSARY.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/GLOSSARY.md),
[README.md](https://github.com/LostArtefacts/TRX/blob/develop/README.md)). It is
GPL-3.0 ([COPYING.md](https://github.com/LostArtefacts/TRX/blob/develop/COPYING.md)).

**Shape versus ours.** The architecture is the same as PLAN §2C, close enough
that their file layout is nearly a diagram of our phase 0–3: a launcher that
starts the original process and injects a DLL, a patch table that redirects
original addresses to their C, generated headers that let their C call whatever
is not replaced yet, and a progress file that is the single source of truth for
both. The invariant is the same too — their stated top values are "compatibility
with the original game's look and feel" *and* "player choice whether to enable
any impactful changes"
([CODING_GUIDELINES.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/CODING_GUIDELINES.md)),
which is a living-game stance, not an archival one: they fix original design
bugs, and they gate the impactful ones.

Two differences that matter before reading on. First, **they had names.** Their
progress file is grouped by original source files (`3dsystem/3d_gen.cpp`,
`game/lara.cpp`) with original function names like `phd_RotYXZpack`
([tr1-1.2.0 progress.txt](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/docs/progress.txt)) —
the Tomb Raider scene has had original symbol names and file decomposition for
years. That is exactly the asset PLAN §3 is trying to manufacture from the PSX
side, and it is the reason their timelines look so short. Second, **their target
is much smaller**: TR1 catalogues ~872 functions and TR2 ~1,218, against our
~2,952.

---

## 2. What we can steal

### 2.1 The injection mechanism (our phase 0, almost verbatim)

TR2X's is the cleanest and the best documented in code. Three pieces:

- **A launcher executable** that does `CreateProcess` on the original
  (hard-coded `Tomb2.exe`) with `CREATE_SUSPENDED`, then the textbook remote
  `LoadLibraryA` injection — `VirtualAllocEx` the DLL path into the target,
  `WriteProcessMemory` it, `CreateRemoteThread` at `LoadLibraryA`, wait, then
  `ResumeThread`. The DLL path is derived from the launcher's own path with the
  extension swapped, and command-line arguments are forwarded
  ([main_exe.c](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/main_exe.c)).
  Because the process is suspended, **all patching happens before the game's
  entry point runs** — no races, no "hook after the menu loads" ordering
  problems.
- **`DllMain` on `DLL_PROCESS_ATTACH`** opens a log, loads the shim library it
  needs, and calls one `Inject_Exec()`
  ([main_dll.c](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/main_dll.c)).
- **The detour itself is ~25 lines.** `VirtualProtect` the first five bytes of
  the original function to `PAGE_EXECUTE_READWRITE`, write an `0xE9` near-`jmp`
  with the offset computed from the two addresses, done
  ([inject_util.c](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/inject_util.c)).
  No trampoline, no MinHook, no disassembler. The public surface is a single
  `INJECT(enable, address, our_function)` macro
  ([inject_util.h](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/inject_util.h)).

Two design details worth copying outright:

- **The `enable` flag swaps the direction of the patch**, so the whole table can
  be compiled in and turned off. That is a free A/B switch: run with injection
  disabled and you are running Capcom's binary, same process, same loader. For a
  project whose deliverable is divergence, having "original behaviour" one
  boolean away is worth a great deal.
- **A five-byte overwrite is enough because they never resume into the original
  body.** A replaced function is replaced whole; they do not wrap it. Same
  commitment as our rule 4 (no stubs), and it is what lets the detour layer stay
  trivial — the clobbered prologue never executes again.

Our `BOF3.exe` is `/FIXED` with no `.reloc` (PLAN §1), so image base `0x400000`
is guaranteed and a raw `0xE9` at a literal address is as safe here as it was
for them. Our DLL must be 32-bit for the same reason theirs was.

### 2.2 The two-binding header convention — the single best idea here

This is the thing to take. For every original function, **the name is bound
exactly once, in a header, to one of two things**:

- not yet decompiled → a macro that expands to a cast of the literal address to
  a function pointer of the correct signature;
- decompiled → an ordinary C prototype for their own function.

Call sites are identical in both cases
([3d_gen.h at tr1-1.2.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/src/3dsystem/3d_gen.h)
shows both forms in one file). So **decompiling a function requires no edit to
any of its callers** — you write the C, delete one macro line, add one
prototype, add one `INJECT` line. The same trick covers globals: original
variables are macros that dereference a typed pointer at a literal address, with
separate forms for scalars, arrays and function-pointer tables, and the
initialiser value recorded in the macro for the ones that have one
([util.h at tr1-1.2.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/src/util.h),
[vars.h at tr1-1.2.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/src/global/vars.h)).
Late in the project the same names become real `extern` C globals owned by their
DLL ([vars.h at tr1-1.4.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.4.0/src/global/vars.h)) —
again with no call-site churn.

For us this is the answer to "how does our C call not-yet-decompiled original
code", and it is a better answer than a hand-maintained `extern` table because
it makes the *transition* free rather than the steady state cheap.

### 2.3 The address corpus is generated, not written

`funcs.h` and `vars_decomp.h` are both autogenerated and say so in their first
line ([funcs.h](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/global/funcs.h),
[vars_decomp.h](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/global/vars_decomp.h)).
The source of truth is one plain-text file, `docs/tr2/progress.txt`, with three
sections — `# TYPES`, `# FUNCTIONS`, `# VARIABLES` — where each function line is
`offset, size, flags, C declaration`, grouped under comment headers naming the
original source file
([progress.txt](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/docs/tr2/progress.txt)).
Flags are a four-state lattice:

| flag | meaning |
|---|---|
| `-` | to do |
| `*` | to do, **and our code already calls it** |
| `x` | unused — present in the binary, never called |
| `+` | fully decompiled |

Three tools read that one file
([generate_funcs](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/generate_funcs),
[generate_ida_importer](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/generate_ida_importer),
[render_progress](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/render_progress),
shared parser in
[ida_progress.py](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/shared/ida_progress.py)):
one emits the C headers, one emits an **IDC script that pushes types, names and
signatures back into IDA**, and one renders a treemap SVG of progress. There was
also a reverse path in the TR1 era — a script that consumed IDA's function list
on stdin and merged it into the progress file
([update_functions](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/docs/update_functions)).

Read that as a direct instruction for us: **`symbols.toml` should be the only
place an address is typed, and the C header, the Ghidra import script and the
progress report should all be generated from it.** We already have the file and
the `sync_symbols.py`-shaped tooling in the sibling; what we do not yet have is
the C-header generator and the Ghidra round-trip. The `*` flag in particular is
worth adopting verbatim — "not done, but we already depend on it" is precisely
the ranked work queue PLAN §2 phase 2 wants, and it falls out of the build for
free rather than needing runtime tracing.

### 2.4 Where the `INJECT` table lives

Two shapes, and they changed their minds between projects:

- **TR1 (distributed):** each module exposes its own `T1MInject<Module>()` that
  lists that module's addresses; one central file calls them all in order
  ([inject.c at tr1-1.2.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/src/inject.c)).
  By the end only two subsystems still had entries
  ([inject.c at tr1-1.4.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.4.0/src/inject.c)) —
  which makes the file a live progress indicator.
- **TR2 (centralised):** one 1,053-line `inject_exec.c` with a static function
  per subsystem, each holding a block of `INJECT` lines in address order
  ([inject_exec.c](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/inject_exec.c)).

The distributed shape is better for us. Our rule 3 makes addresses load-bearing
constants, and keeping a subsystem's addresses next to that subsystem's code
means a reviewer sees the address and the implementation in one diff.

### 2.5 How they knew a replacement was correct

Honest answer: **they did not verify equivalence, and they did not try to.**
There is no byte-matching, no decomp-permuter, no objdiff — nothing in the repo
or its issue history resembles the `asm-differ` culture of the console decomp
scene. Their verification was, in order of weight:

1. **It has to link and run.** The two-binding header turns a signature mistake
   into a compile error, and a calling-convention mistake (`__cdecl` is spelled
   out in every generated declaration) into an immediate crash.
2. **Playtesting, with pull-request review** — every change needs at least one
   approval
   ([CHANGE_SUBMISSION.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/CHANGE_SUBMISSION.md)).
3. **The game's own demo playback as a determinism oracle.** This is the
   interesting one and it is visible only in the bug record: they shipped fixes
   for "a desync in the Vilcabamba demo if the wall glitch fix option was
   enabled" and "a desync in the Lost Valley demo if responsive swim
   cancellation was enabled"
   ([tr1 CHANGELOG](https://github.com/LostArtefacts/TRX/blob/tr1-4.15.1/docs/tr1/CHANGELOG.md)).
   A recorded demo is a fixed input sequence replayed through the full
   simulation; if any replaced function drifts by one tick or one integer, Lara
   walks into a wall and the run visibly diverges. It is a whole-system
   regression test that the original game hands you for free.

**We should look for our equivalent of that demo.** Candidates in BoF3: the
attract-mode sequence, and anything with a fixed input script. It will not catch
everything, but it is cheap, it is end-to-end, and it is sensitive to exactly
the arithmetic drift that unit tests miss. Note also the shape of those two
changelog lines — the desync was caused by an *intentional, config-gated*
divergence, which is a warning about what our own ledgered changes will do to
any replay-based oracle.

### 2.6 The end of the hybrid period — they reached it, twice

| | start | fully native | elapsed |
|---|---|---|---|
| TR1 | 0.1, 2021-02-10 | **2.0, 2021-12-07** | ~10 months |
| TR2 | 0.1, 2024-04-26 | **0.8, 2025-01-01** (decompilation closed 2024-12-23) | ~8 months |

TR1's 2.0 note is the one to read: *"Shipped our own .exe! Tomb1Main is now
fully open source and no longer needs injecting itself to the game. It also no
longer depends on any of the TombATI .dll files."*
([tr1 CHANGELOG](https://github.com/LostArtefacts/TRX/blob/tr1-4.15.1/docs/tr1/CHANGELOG.md)).
TR2's is *"completed decompilation efforts – TR2X.dll is gone, Tomb2.exe no
longer needed"*
([tr2 CHANGELOG](https://github.com/LostArtefacts/TRX/blob/tr2-1.5.1/docs/tr2/CHANGELOG.md),
tracked as
[issue #1694](https://github.com/LostArtefacts/TRX/issues/1694), one of a series
of "bring decompilation to 70/80/90/100%" tickets).

What the cutover took, read off the same releases: the last things to go were
the **platform layer** — TR1 2.0 moved music, sound and FMV playback to SDL and
libavcodec in the same release that dropped injection, and TR2 switched to
OpenGL at 0.7, one release before the DLL disappeared. This confirms PLAN §5's
ordering instinct with a caveat: **platform first is right, but the platform is
also what pins you to the host until the very last release.** Expect the final
step to be a lump, not a taper.

The scaffolding was then deleted deliberately — a PR titled "Remove old
decompilation documents and tools", *"Removes old artifacts from the
decompilation phase"*
([issue #1289](https://github.com/LostArtefacts/TRX/issues/1289), April 2024).
Today `develop` has no `inject_util`, no `inject_exec`, no `progress.txt`, and a
single `src/trx` tree.

Rate of work, for calibration: at tr2-0.5 (2024-10-08) their progress file
showed 755 of 1,218 functions decompiled (62%), 415 to do, 30 unused, 18 "to do
but already called", plus 520 variables across 52 source-file groups; they
closed the remaining ~460 in about eleven weeks. That is a team with original
names, original file boundaries and typed signatures already in hand. Our corpus
is ~2.4× larger and our names are hypotheses until the matcher confirms them.

### 2.7 Managing deliberate divergence

They diverge constantly and they manage it with four mechanisms, none of which
is a ledger:

1. **A config toggle per impactful change**, declared in X-macro `.def` files
   with the default value inline — e.g. `gameplay.fix_water_exit` defaults off,
   `gameplay.enable_multiple_pickups` defaults on
   ([map_tr1.def](https://github.com/LostArtefacts/TRX/blob/develop/src/trx/config/map_tr1.def)).
   The naming is itself a taxonomy: `fix_*` for bug fixes, `enable_*` for
   behaviour that did not exist, `*_mode` for a choice between eras.
2. **Named presets that restore an era wholesale.** `cfg/presets/tr1-pc.json5`,
   `tr1-ps1.json5`, `tr1-moveset.json5` and their TR2/TR3 equivalents are
   shipped JSON files, each a flat list of config keys and values
   ([presets.h](https://github.com/LostArtefacts/TRX/blob/develop/src/trx/config/presets.h),
   [tr1-pc.json5](https://github.com/LostArtefacts/TRX/blob/develop/data/trx/ship/cfg/presets/tr1-pc.json5)).
   This is a much better idea than it looks: it means "play it as the PC
   original did" is a single selectable state rather than a memory of which
   twenty toggles to flip.
3. **A changelog line per change, carrying the metadata inline.** Current
   entries look like *"Fixed Lara being able to vault or crawl through breakable
   walls that stand on the edge of a tile (Gameplay → Fixes → Fix breakable wall
   clipping) (OG bug) (TRX1024)"*
   ([CHANGELOG.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/CHANGELOG.md)).
   Four fields are encoded: the setting path that gates it, **`(OG bug)`** —
   meaning the original game had this defect, as opposed to it being theirs —
   the ticket, and where relevant `(regression from 1.10)`. That `(OG bug)` tag
   is a direct analogue of our ledger's *"Also in the PSX version?"* field, and
   `(regression from X)` is their way of keeping "we changed this" and "we broke
   this" apart.
4. **Ticket-first policy** — *"For player-facing changes without an existing
   ticket, a ticket needs to be created first"*
   ([CHANGE_SUBMISSION.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/CHANGE_SUBMISSION.md)) —
   so the rationale lives in the issue thread and the changelog links to it.

**What works:** the toggle-plus-preset system, and the `(OG bug)` /
`(regression from X)` tags. Adopt both. The presets idea in particular should go
into `DIVERGENCE.md`'s "Reversible?" field — not just "is there a toggle" but
"which preset restores the original value".

**What they never formalised, and it shows:** there is no place that records
*what the original did*. The changelog says what was fixed; reconstructing the
original behaviour means reading the linked issue, and for the 2021 entries the
issue is often a one-liner. Their lint suite enforces changelog *hygiene* —
entries appended only to unreleased versions, sections in a fixed verb order
([changelog_guard](https://github.com/LostArtefacts/TRX/blob/develop/tools/lint/checks/changelog_guard)) —
but nothing enforces that a behavioural change has an entry at all. That is the
gap our rule 2 exists to close, and this project is the evidence that it is a
real gap rather than a hypothetical one: five years and ~13,000 changelog lines
in, the original behaviour of TR1 is recoverable only by running TombATI.

### 2.8 Build system and portability

- **The build has been a Linux cross-compile from the start, including during
  the Windows-only hybrid phase.** Meson, and a multi-stage Dockerfile that
  builds each dependency with `i686-w64-mingw32` and then builds the DLL against
  them
  ([Dockerfile](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/docker/game-win/Dockerfile),
  [cross file](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/docker/game-win/meson_linux_mingw32.txt)).
  `just` drives it. The Windows-native story is "use WSL"
  ([BUILDING_ON_WINDOWS.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/BUILDING_ON_WINDOWS.md)),
  and they say plainly that any other toolchain is at the user's own risk
  ([BUILDING.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/BUILDING.md)).
- **Portability came from replacing the platform layer, not from the language.**
  SDL2 for window/input/audio, libavcodec for video, OpenGL for rendering; Linux
  and macOS followed once those landed.
- **Going native did not make them 64-bit.** The Windows target in `develop` is
  still `i686-w64-mingw32`
  ([cross file](https://github.com/LostArtefacts/TRX/blob/develop/tools/shared/docker/game-win/meson_linux_mingw32.txt)),
  years after the last original byte was replaced. A caution against PLAN §5's
  implicit assumption that phase 4 falls out of phase 3.

The transferable decision: **pick the non-MSVC toolchain on day one and make it
the only one.** The risk is not that a second toolchain is hard to add later; it
is that the first one quietly accumulates dependencies. They avoided that by
never having an MSVC path at all.

---

## 3. Pitfalls and things they'd do differently

- **Incremental replacement generates a steady stream of regressions, and they
  are not cheap.** Their changelogs contain **262** entries tagged "regression
  from &lt;version&gt;" for TR1 and **182** for TR2 (counted 2026-09-18 over
  [tr1](https://github.com/LostArtefacts/TRX/blob/tr1-4.15.1/docs/tr1/CHANGELOG.md)
  and [tr2](https://github.com/LostArtefacts/TRX/blob/tr2-1.5.1/docs/tr2/CHANGELOG.md)).
  Several land four or five releases after the change that caused them. This is
  the real cost of "playable at every commit" with no equivalence proof, and it
  is the strongest argument in this document for our differential-testing plan
  (`DIVERGENCE.md` §"Differential testing"): they had no archival oracle, and
  they paid for it in bug reports. We have one sitting in the next directory.
- **The demo desyncs show config-gated divergence colliding with the replay
  oracle.** If we build a replay-based regression harness, it has to run with a
  known configuration — probably an "original" preset — or every ledgered change
  will look like a failure.
- **They changed the scaffolding's shape between TR1 and TR2**, moving from
  distributed per-module inject functions and hand-written address headers to a
  centralised inject table and fully generated headers driven by one progress
  file. The generated-header half is clearly the improvement; do that from the
  start. The centralisation half is arguably a regression for a rule-3 codebase.
- **They committed the game's executable to the repository**
  ([bin/README.md at tr1-1.4.0](https://github.com/LostArtefacts/TRX/blob/tr1-1.4.0/bin/README.md)
  documents `tombati.exe`, a patched host that loads their DLL, and
  `tombati.orig.exe`). That is how TR1's injection worked — a modified host
  binary rather than TR2's launcher-and-remote-thread. **We cannot do this**
  (CLAUDE.md rule 1), so TR2's launcher approach is the only one of the two
  available to us. Good: it is also the better one, since it leaves the original
  file untouched and works for any copy the user owns.
- **"Injection" got reused for something else.** In today's TRX an *injection*
  is a binary patch applied to level *data* at load time
  ([INJECTIONS.md](https://github.com/LostArtefacts/TRX/blob/develop/docs/trx/INJECTIONS.md)),
  which makes the historical record confusing to search — a changelog line about
  "injections" after ~2022 is almost never about code. Worth picking a different
  word for our data-patching mechanism when we get one.
- **Unused functions are worth marking early.** 30 of TR2's 1,218 were flagged
  `x` (present, never called). Small, but each one is a function you would
  otherwise decompile and never be able to test.

---

## 4. Where to go when blocked

| Our problem | Read |
|---|---|
| Phase 0: getting our DLL into the process | [`src/tr2/main_exe.c`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/main_exe.c) and [`main_dll.c`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/main_dll.c) at tag `tr2-0.5` — suspended `CreateProcess`, remote `LoadLibraryA`, patch in `DllMain`, resume |
| Phase 0: redirecting one address | [`src/tr2/inject_util.c`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/src/tr2/inject_util.c) — the whole detour layer, ~25 lines |
| Calling original code from ours; the decompile-one-function edit | [`src/3dsystem/3d_gen.h`](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/src/3dsystem/3d_gen.h) at `tr1-1.2.0` (both bindings in one file) and [`src/util.h`](https://github.com/LostArtefacts/TRX/blob/tr1-1.2.0/src/util.h) (the variable macros) |
| Making `symbols.toml` generate our headers | [`tools/tr2/generate_funcs`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/generate_funcs) + [`tools/shared/ida_progress.py`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/shared/ida_progress.py) |
| Pushing our names back into Ghidra (phase 1) | [`tools/tr2/generate_ida_importer`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/generate_ida_importer) — IDC, but the pattern (emit a script that applies types + names + signatures) ports to Ghidra headless directly |
| Honest progress reporting (phase 1/3 exit tests) | [`docs/tr2/progress.txt`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/docs/tr2/progress.txt) for the format and flag lattice; [`tools/tr2/render_progress`](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/render_progress) for byte-weighted rather than count-weighted percentages |
| Designing the divergence toggle vocabulary | [`src/trx/config/map_tr1.def`](https://github.com/LostArtefacts/TRX/blob/develop/src/trx/config/map_tr1.def) and the shipped [presets](https://github.com/LostArtefacts/TRX/tree/develop/data/trx/ship/cfg/presets) |
| What a divergence record looks like when it is *not* a ledger | [`docs/CHANGELOG.md`](https://github.com/LostArtefacts/TRX/blob/develop/docs/CHANGELOG.md) — read for the `(OG bug)` / `(regression from X)` / setting-path convention, and for what is missing |
| Whether to let level builders pin settings | [`USER_CONFIGURATION.md`](https://github.com/LostArtefacts/TRX/blob/develop/docs/trx/game_flow/USER_CONFIGURATION.md) — `enforced_config` and `hidden_config` |
| End of the hybrid period | [tr1 CHANGELOG](https://github.com/LostArtefacts/TRX/blob/tr1-4.15.1/docs/tr1/CHANGELOG.md) §2.0, [tr2 CHANGELOG](https://github.com/LostArtefacts/TRX/blob/tr2-1.5.1/docs/tr2/CHANGELOG.md) §0.8, [issue #1694](https://github.com/LostArtefacts/TRX/issues/1694), [issue #1289](https://github.com/LostArtefacts/TRX/issues/1289) |
| 32-bit-bound cross-compiling toolchain | [Dockerfile](https://github.com/LostArtefacts/TRX/blob/tr2-0.5/tools/tr2/docker/game-win/Dockerfile) + [meson cross file](https://github.com/LostArtefacts/TRX/blob/develop/tools/shared/docker/game-win/meson_linux_mingw32.txt) |

Useful tags for reading history, since `develop` no longer contains any of it:
`tr1-1.2.0` (mid-hybrid, hand-written headers), `tr1-1.4.0` (end of hybrid, two
subsystems left), `tr2-0.5` (mid-hybrid, generated headers — the best single
snapshot), `tr2-0.8` (first fully native TR2).

---

## 5. Licence, and what it means for us

**TRX is GPL-3.0**
([COPYING.md](https://github.com/LostArtefacts/TRX/blob/develop/COPYING.md);
[the GitHub repository page](https://github.com/LostArtefacts/TRX) states
GPL-3.0 as the licence).

Plainly: **reading is fine, vendoring is not.** GPL-3.0 is a strong copyleft — a
derivative work must itself be distributed under GPL-3.0. This repository is
PolyForm Noncommercial, which is *not* GPL-compatible in either direction: we
cannot relicense their code under PolyForm, and we cannot relicense this project
under GPL without abandoning the noncommercial restriction. So:

- **Do not copy any TRX source into this repository.** Not the detour function,
  not the macros, not a Python tool, not a `.def` file. This includes
  "translating" a file — a line-by-line port of a GPL file is a derivative work.
- **Techniques, facts and designs are not copyrightable.** Suspending a process
  and injecting via `CreateRemoteThread`, patching five bytes with a near-`jmp`,
  binding a name to either an address macro or a prototype, keeping one text
  file as the source of truth for generated headers — these are ideas, and using
  them is unencumbered. Each is described in this document precisely so we can
  implement from the description rather than from their source.
- The practical discipline is the one CLAUDE.md rule 5 already sets for
  `bof3ext`: read freely, cite generously, implement independently. Here the
  licence makes it mandatory rather than merely tidy. The only lawful route to
  using their code would be relicensing this project as GPL-3.0, which conflicts
  with the noncommercial intent; treat that as closed.

---

## 6. What I could not verify

- **Where TR1's original function and source-file names came from.** Their
  progress file uses original-looking names (`phd_RotYXZpack`) and original
  `.cpp` file paths, but nothing in the repository says how those were obtained.
  The neighbouring TOMB5 project states that its names came from leaked PSX
  debugging symbols (`.SYM`/`.MAP`) plus the Mac executable
  ([TOMB5 README](https://github.com/Trxyebeep/TOMB5)), which is probably the
  same lineage — but I did not find that stated for TR1X. **Unverified.** It
  matters because it is the closest analogue to our PSX Rosetta stone and would
  tell us how much of their speed came from having names on day one.
- **The exact mechanism by which TR1's patched `tombati.exe` loaded their DLL.**
  `bin/README.md` says only that it is "a patched version of TombATI executable
  that loads the `Tomb1Main.dll` library". An import-table entry is the obvious
  method and is what OpenRCT2 did, but I did not confirm it. **Unverified.**
  Moot for us either way — we cannot ship a patched game binary.
- **Why the Windows target is still 32-bit.** Observed in the current cross
  file; the reason is nowhere stated. **Unverified.**
- **Whether any written decompilation guide ever existed.** I found no
  `DECOMPILATION.md`, no wiki page and no postmortem — the contributing docs
  cover formatting, commits and releases only, and the decompilation-era
  documents were deliberately deleted
  ([issue #1289](https://github.com/LostArtefacts/TRX/issues/1289)). Everything
  in §2 above was reconstructed from the code and the generated artifacts.
- **Whether they had any rule equivalent to our rule 4 (no stubs).** Their
  practice looks like whole-function replacement, and the 5-byte-clobber detour
  makes partial replacement impractical, but I found no written rule.
  **Unverified.**
- **Team size and hours.** The rate figures in §2.6 are elapsed calendar time
  from tags and changelogs, not effort. Do not read "eight months" as a budget.
- **TR3's provenance.** A maintainer answered "TR3 likely yes" to a question
  pointing at existing third-party TR3/TR4/TR5 decompilations
  ([issue #2104](https://github.com/LostArtefacts/TRX/issues/2104)), and TR3 is
  now supported. Whether it went through the same injection process or was
  adopted from `Trxyebeep/tomb3` I did not establish. **Unverified**, and worth
  resolving before we weigh PLAN §2B's crude lifter — a project that skipped the
  hybrid phase is the relevant comparison.
