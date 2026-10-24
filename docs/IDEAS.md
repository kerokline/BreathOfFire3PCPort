# Ideas — intake for unscheduled proposals

**Status:** IN PROGRESS (2026-10-22, I21 and I50 built; 50 entries, I1..I50 - see the index for each one's state)

Nothing here is scheduled. This is the intake: an idea lands here with a
feasibility rating and a first step, and leaves when it is promoted, built, or
rejected.

- Promoted to work → moves to [`STATUS.md`](STATUS.md) order of work.
- A *behavioural* change that gets built → also gets a
  [`DIVERGENCE.md`](DIVERGENCE.md) entry. An idea here is not a ledger entry and
  does not authorise a divergence.
- Rejected → stays, with the reason. A recorded "no" stops the idea being
  re-proposed.

## Ratings

- **HIGH** — tooling or docs only; every input exists today.
- **MEDIUM** — one unknown that a short investigation settles.
- **LOW** — gated on a phase that has not landed ([`PLAN.md`](PLAN.md) §5), or
  on someone outside the project.

## Entry template

```
## I<n> — <title>

**Ask (<date>):** what was proposed, in a sentence or two.
**Kind:** tooling | engine | platform | game behaviour | docs | process
**Feasibility:** HIGH | MEDIUM | LOW   **Gated on:** <phase / decision / nobody>

### What already exists
### What is missing
### First concrete step
### Outcome
_(<date>) promoted to … | built in … | rejected because …_
```

Ids are permanent; never renumber. Claims about the binary follow the evidence
rule ([`README.md`](README.md)) here too.

---

## Index

| Id | Title | Kind | Feasibility | State |
|---|---|---|---|---|
| I1 | PSX ↔ PC save file interchange | tooling | MEDIUM | **PSX saves load and play on PC** (owner, 2026-09-19, [`save-interchange.md`](save-interchange.md) §4); only PC→PSX in a real PSX runtime is unchecked |
| I2 | Selectable localisations from original discs | game behaviour | LOW | **partly built**: English from the player's US disc (DIV-0005..0009, DIV-0013..0020), `BOF3X_LANG`, the launcher's Language box; since 2026-10-10 every held disc's language from the disc (`loc_build all --discs`, DIV-0090; fr-FR, de-DE, ja-JP, en-150); widened into [`ASSET_SOURCES.md`](ASSET_SOURCES.md) |
| I3 | Crude x86→C lifter as portability accelerator | engine | MEDIUM | **closed 2026-10-21** (the owner): no longer needed - every function is ours by takeover; the prototype stays in [`lifter-feasibility.md`](lifter-feasibility.md) |
| I4 | Stacktrace-driven "who called this" work-queue harvester | tooling | LOW | **first-call tracer built 2026-09-19** — [`call-trace.md`](call-trace.md) |
| I5 | Recover Capcom's `.c` file boundaries from global blocks | tooling | MEDIUM | **folded into I44** 2026-10-21 (the owner): a later step of the typed-source work, as a source-layout pass, not an idea of its own |
| I6 | Demo/attract playback as determinism oracle | tooling | MEDIUM | **built 2026-09-19** (external sampler) — [`attract-mode.md`](attract-mode.md) §6 |
| I7 | Replace MCI/VFW with a bundled video decoder | platform | LOW | **overtaken 2026-09-23**: the owner chose MCI into the window (DIV-0035); `Fmv_Play` is ours. Stays on the list |
| I8 | Replace the DirectDraw / `IDirect3D3` presentation layer | platform | LOW | **built 2026-09-23** (DIV-0031) — [`render-backend.md`](render-backend.md) |
| I9 | Integer-scale the picture and extend the view into the remainder | game behaviour | LOW | **integer scaling built** (DIV-0036, DIV-0042); the wide view built as two fixed presets, 426 x 240 and 560 x 240 (DIV-0041, amended 2026-10-13: [`ultrawide-frame.md`](ultrawide-frame.md)), not the `W / k` rule |
| I10 | Uses for the call-trace data (seven, ranked) | tooling | MIXED | item 1 **passed 2026-09-19** — [`call-trace.md`](call-trace.md) §7; item 2 (the work queue) **built** — §9 there |
| I11 | In-process crash reporter | tooling | HIGH | **built 2026-09-19** — [`crash-reporter.md`](crash-reporter.md) |
| I12 | Let the game run while its window is not in front | tooling | HIGH | **built 2026-09-23** (DIV-0033) |
| I13 | Save states: snapshot the running game, restore it | tooling | MEDIUM | **built 2026-10-19** as the quicksave round ([`quicksave-round.md`](quicksave-round.md), PR #95, DIV-0145..0147); the all-original half not built |
| I14 | Compare what is drawn: display-list, texture, back-buffer hash | tooling | MIXED | **overtaken 2026-10-21** (the owner): the A/B/C harness ([`abc-harness.md`](abc-harness.md)) and the state hash cover what it was for |
| I15 | A look toggle: clean / sharp against soft / CRT-like | look | — | **sunset 2026-10-21** (the owner): the looks stay as built and chosen in the launcher; the live toggle key will not be built |
| I16 | Exact frame pacing: the deadline in a double | platform | MEDIUM | **done 2026-09-24** (DIV-0047) |
| I17 | Fast-forward: a shorter frame period while a key is held | game behaviour | MEDIUM | **done 2026-09-24** (DIV-0048, a toggle) |
| I18 | F12 before shipping: disable it, or make it a true quicksave | game behaviour | HIGH / MEDIUM | **closed** (the owner, 2026-10-17): F12 stays; the true quicksave is I13's round |
| I19 | Screen curvature for the SatPixie look | look | HIGH | open |
| I20 | A Config row that opens the physical binding screen in game | — | — | **deferred** by the owner, 2026-09-24 |
| I21 | Furigana over the Japanese script, drawn by our message box | game behaviour | MEDIUM | **built 2026-10-22** (DIV-0155; the mod zip DIV-0157) - the reading-row round ([`reading-row-round.md`](reading-row-round.md), [`reading-row.md`](reading-row.md), issue #97) with I50; the owner's play and the readings' proofreading owed |
| I22 | Cut content: the unused skills, music, text, the whelp's portrait, Sunder's animation made to loop | game behaviour | MIXED | open; mapped in [`cut-content.md`](cut-content.md) (owner, 2026-09-25); **the Sunder loop built as DIV-0176** (2026-10-24) |
| I23 | Music: the PC's MP3s against the disc's sequences | tooling | HIGH | **built** 2026-10-06..11 (DIV-0081 the loops, DIV-0087 the disc's music, DIV-0091 minimp3, heard); method in [`bgm-comparison.md`](bgm-comparison.md) (owner, 2026-09-26). **Asked for first, 2026-10-04**: the owner wants to hear the MP3s against the sequences before the MP3 decoder's fate is decided ([`platform-layers-plan.md`](platform-layers-plan.md) section 5) |
| I24 | The scenario round: the ~620 chapter-bank functions wave by wave, on the spell round's pattern | engine | HIGH | **built 2026-09-27..28** in round ten's first three waves - every chapter bank ours, fuzz-only ([`takeover-queue-round10.md`](takeover-queue-round10.md)); planned in [`takeover-queue-scenario.md`](takeover-queue-scenario.md) |
| I25 | The area round: the area overlays enumerated from their tables | engine | HIGH | **built 2026-09-27..28** in round ten's six waves - every area overlay of worlds 0..4 ours, fuzz-only ([`takeover-queue-round10.md`](takeover-queue-round10.md)); planned in [`takeover-queue-areas.md`](takeover-queue-areas.md) |
| I26 | The boss round: the BOSS overlays enumerated from the engine's three root sets | engine | HIGH | **done** 2026-09-28: round eleven, 531 functions, [`takeover-queue-round11.md`](takeover-queue-round11.md); the live check per fight and the debts remain |
| I27 | Round twelve: the field modes and the battle engine, by address band | engine | HIGH | done 2026-09-29: 654 functions in fourteen groups, 6,891 ours, live-checked, [`takeover-queue-round12.md`](takeover-queue-round12.md); planned and cut in [`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) (owner, 2026-09-28): 631 functions, 14 groups, two harness groups |
| I28 | Round thirteen: the effect-object engine (1,697 functions: the 627 the catalog labels area overlays, which are effect-kind code, and the 1,070 the labelling pass found beside them) | engine | HIGH | **done** 2026-10-03/04 (round thirteen, PR #40); [`takeover-queue-round13.md`](takeover-queue-round13.md) section 9 - the owner's scope of 2026-09-29, to be cut with `tools/band_rows.py` at round twelve's tip |
| I29 | Button prompts drawn from the bindings in force, not from each release's defaults | game behaviour | LOW | **built 2026-10-17** (the button unification round, DIV-0133..0140) - [`button-unification-round.md`](button-unification-round.md); keyboard and other pads' glyphs a later round |
| I30 | Machine-readable docs: Open Knowledge Format frontmatter and a generated wiki | docs / tooling | HIGH | open; parked until the spec hardens or is validated outside Google (owner, 2026-10-04) |
| I31 | The platform layers: the C runtime, the MP3 decoder, the renderer's remainder, the small glue layers, and the cutover to our own executable | engine / platform | MIXED | **built through state 3** (2026-10-12, PRs #49..#53: the C runtime, minimp3, state 2 proved, `bof3x.exe`); the renderer's remainder open - [`platform-layers-plan.md`](platform-layers-plan.md); its first step (a read pass and a reach trace) needs no decision |
| I32 | The PSP release's game-logic changes, found by measurement and offered as a config toggle | game behaviour | UNKNOWN | **measured, built in part** (the PSP-only round: the code diff `psp-code-diff*.md`, DIV-0108's two switches, PR #56); opened 2026-10-04 (the owner); sixteen leads from players' reports, catalogued in the entry and unmeasured but for the widescreen - the first step is the diff, beside [`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 8's phase 4 item |
| I33 | Productisation: what stands between our own executable and a finished product - a portable platform layer, mods as cache layers, presets from the ledger, in-game settings, first-run import, saves, the release gate, the commercial audit | engine / product | MIXED | open 2026-10-04 (the owner); three early decisions of shape, the rest after the cutover (I31) |
| I34 | The name entry back: the community's renamer and naming at New Game, from the PlayStation routine ([`name-entry-restoration.md`](name-entry-restoration.md)) | gameplay / localisation | HIGH / MEDIUM | open 2026-10-05 (the owner); researched, deferred to the localisation rework of phases 4 and 5; DIV-0075 ends the hang meanwhile |
| I35 | The music host suite (`tools/bgm/host`) building and running on the owner's Windows machine, not only a POSIX cloud host | tooling | HIGH | built 2026-10-10 (`catchup/music-tooling`): `abort_check.h`, the suite in CI |
| I36 | Our own ATRAC3plus decoder for the PSP's sound effects, voice and jingles, in place of the player's ffmpeg | platform | MEDIUM | open 2026-10-10 (the owner): **low priority** |
| I37 | Grow the field's tile ring for wide mode as the PSP did (28 x 56 to 33 x 58 cells) instead of lowering the inset ([`psp-widescreen-placements.md`](psp-widescreen-placements.md) 5 and 6.1; 23 sites; about half a week) | display | MEDIUM | **built 2026-10-21** as RG in the wide-ring round (DIV-0041's 2026-10-21 ring amendment; [`wide-ring.md`](wide-ring.md)); on `wide-ring/round`, PR next; the owner's play owed |
| I38 | Draw the two edge sweeps (`EffectKind0D`'s slide, area 172's growing shade) mapped onto the wide picture, so they start and end past its edge in the original's frames ([`wide-staging.md`](wide-staging.md) 8) | display | HIGH | **built 2026-10-21** as SW in the wide-ring round (DIV-0041's 2026-10-21 sweeps amendment; [`wide-sweeps.md`](wide-sweeps.md)); on `wide-ring/round`, PR next; the owner's play owed |
| I39 | The Identify panel's element icons shaded by how weak the enemy is, not bright / dim | display | HIGH | open 2026-10-15, the owner: an argument for it, not now |
| I40 | Port the importer to C++ inside the launcher, so a release needs no bundled Python ([`install-sources.md`](install-sources.md) 7) | platform | MEDIUM | open 2026-10-15 (the owner: bundle Python now, port later) |
| I41 | Read CHD and CSO images as sources (today refused with a sentence; [`install-sources.md`](install-sources.md) 4) | tooling | MEDIUM | open 2026-10-15, from the Sources page work |
| I42 | Make the install faster: setup rebuilds everything from every source on each add (about 3 to 4 minutes a setup, two of them the movie's conversion; [`install-sources.md`](install-sources.md) 5 and 6) - keep the base (movie, music, its `DAT` files) across setups and build only a new source's layers | tooling | MEDIUM | **partly built** 2026-10-15: every step timed; the movie, PSX sound and language layers reused when unchanged (adding a disc 89 s, not 372); the PSP's conversions left |
| I43 | Settle every gap of a disc-built exe image: each of the ranges where the PC's `.data` has a value and the image leaves zero (`recipes/exe-gaps.tsv`; for the EU disc 188 ranges, 2,561 bytes on `main` the day it opened) placed from a disc, filled by an engine rule, or shown never read - first by placing a region from the pointers that point into it | tooling / engine | HIGH | **step 1 built** 2026-10-16 (the owner asked), from the forest sprite of a PSX-only install: the importer places a region from the pointers into it (gap ranges EU 185 to 177, the sprite fixed); steps 2..4 open |
| I44 | The data image as typed source: the next step toward reconstructed source (stages 1..3; I5's file boundaries folded in 2026-10-21) | engine / tooling | HIGH / MEDIUM | stages 1 and 2 **built** in the typed-data round ([`typed-data-round.md`](typed-data-round.md), PRs #94, #100); stage 3 (relocatable) and the source-layout pass open |
| I45 | Shrink the quicksave files (about 4 MB raw) | engine | HIGH | open 2026-10-17 (the owner): nothing measured |
| I46 | A quicksave is native memory: revisit what loading one trusts | engine | HIGH | open 2026-10-19 (the owner): a note to consider again |
| I47 | Quick-loading across translations: review the text that carries over | engine | HIGH | open 2026-10-20 (the owner): nothing measured |
| I48 | A comment-cleanup round after the typed-data round | tooling / source | HIGH | open 2026-10-20 (the owner): not now |
| I49 | What `Battle_AutoTargetCheck`'s `+0xBA` test means | engine | — | open 2026-10-20: the rename done, the meaning unconfirmed |
| I50 | A second language in the row above the text: another held language layer re-flowed into the reading rows, page by page, by the same renderer as I21 | game behaviour / localisation | MEDIUM | **built 2026-10-22** (DIV-0156) - every held pair, the reading-row round ([`reading-row-second.md`](reading-row-second.md)); from issue #97 (the owner, 2026-10-20); the owner's play owed |

---

## I1 — PSX ↔ PC save file interchange

**PC side read 2026-09-19 ([`save-files.md`](save-files.md)):** one file per
slot, `BISLPS0<X>.DAT` (`X` = hex 0-F — the PSX memory-card filename convention
kept as a filename), each exactly `0x12B0` = 4,784 bytes, staged through one
buffer at `0x92A0E0`; the slot summary is `0x1C` bytes at file offset `0xCA0`.

**Format solved and converter written 2026-09-19
([`save-interchange.md`](save-interchange.md), `tools/save_convert.py`):** the
PC file is the PSX `0x10B0`-byte game block from offset 0, same checksum rule,
same offsets, with the character-record name field widened 5→9 bytes. Both
directions round-trip byte-identically and the sibling's verifier accepts a PC
save. **Next step:** the owner loads the converted JP and US saves
([`USER_CHECKS.md`](USER_CHECKS.md)). *Done the same day: both load, play
and re-save ([`save-interchange.md`](save-interchange.md) §4).*

**Ask (2026-09-18, moved here from the order of work 2026-09-19):** convert a
save between the PlayStation release and the PC port, in at least one direction.
**Kind:** tooling. **Feasibility:** MEDIUM. **Gated on:** the game launching
and running stably here (a converted save has to be loaded to be verified).

Deliberately the *first* idea to pick up once that gate clears: it is a visible
win early ([`PLAN.md`](PLAN.md) §6 names the risk of phases 0–2 delivering
nothing a player can see), and it tests the shared-source finding on real data
where errors are cheap.

### What already exists
- The sibling's `SAVE_IMPORT.md`, `tools/save_tool.py`, `tools/save_import.py`
  — reference, not vendored.
- The persistent character record is 164 bytes in both binaries
  ([`PLAN.md`](PLAN.md) §3).

### What is missing
- The PC save format itself — unexamined.
- Field-by-field confirmation of record layouts. **Do not assume they match:**
  the port widened the enemy name field 8→12 bytes
  ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2), so layout drift is measured, not
  hypothetical. Character *names* are the obvious place to look first.

### First concrete step
Make a PC save, locate the 164-byte records in it by value, diff against the
sibling's documented PSX layout.

### Outcome
_open — tool exists; in-game load done 2026-09-19 (both converted PSX saves
load, play and re-save on PC, [`save-interchange.md`](save-interchange.md)
§4). Left: PC→PSX in a real PSX runtime, and the US save's facility names
(§4's open check)._

## I2 — Selectable localisations from original discs

**Ask (2026-09-18):** JP/EN/DE/FR/ZH selectable at runtime, reading assets from
player-supplied discs. **Kind:** game behaviour. **Feasibility:** LOW.
**Gated on:** phase 5.

Written up in [`STATUS.md`](STATUS.md) "A stated goal worth recording now" —
not restated here. _Remaining sections to fill._

**Partly built since (noted 2026-09-24):** English, from overlays built on
the player's machine out of their own US disc - dialogue, system pools, item
and ability names, the font, the title menu, the Config screen, menu verbs,
battle labels, default names (DIV-0005..0009, DIV-0013..0020;
[`dialogue-localisation.md`](dialogue-localisation.md)) - selected by
`BOF3X_LANG=en` (`en-US` since the tags; the bare code is retired, DIV-0005) or the launcher's Language box. The other languages, and a
runtime switch, are open.

**Widened into a delivery plan (2026-09-26):** [`ASSET_SOURCES.md`](ASSET_SOURCES.md)
treats this idea as the special case of a general one: build the game from
whatever copies the player owns, down to a single PSX disc once the code is
ours. It covers identity by per-file hash, recipes per build, one import step
into a canonical cache, and what a disc can and cannot supply. It also
proposes the rule that taken-over code never transcribes the exe's game
tables, audited in [`exe-table-audit.md`](exe-table-audit.md). Music is
split out as I23.

## I3 — Crude x86→C lifter as portability accelerator

**Ask (2026-09-18):** [`PLAN.md`](PLAN.md) §2 recommendation and phase 4.
**Kind:** engine. **Feasibility:** MEDIUM (was LOW; 2026-09-25). **Gated on:**
phase 3 platform layer; the task system ours on host coroutines; the renderer
thread's x87 control word measured.

Studied in [`lifter-feasibility.md`](lifter-feasibility.md) with a prototype,
`tools/lift/`. On a synthetic corpus written to imitate the game's shapes, the
prototype lifts every function to 64-bit-clean C. That C matches the original
bytes run in an emulator on every comparable round, and six negative controls
are refused. Lifting instructions is the easy part. What blocks a cutover is
what the docs already measured: the task system's raw `esp` swap, which lifted
C cannot express; the x87 model being exact only under `0x027F`; discovery of
~10,200 starts; and the imports.

### First concrete step

Run `lift_x86.py` over `BOF3.exe`'s entries on the owner's machine, output to
`analysis/` (never committed). Its refusal report is the measure of how far
"crude" is from the real binary (§7 there).

### Outcome
_(2026-10-21) closed by the owner: not needed - the takeover reached every
function, so there is nothing left for a lifter to accelerate. The prototype
and its study stay as a record._

## I4 — Stacktrace-driven work-queue harvester

**Ask (2026-09-18):** generalise the caller-finding technique
([`PLAN.md`](PLAN.md) §4, phase 2). **Kind:** tooling. **Feasibility:** LOW.
**Gated on:** phase 0 (needs code in-process). _To fill._

## I5 — Recover source file boundaries from global blocks

**Ask (2026-09-18):** cluster PC globals into per-translation-unit blocks
([`SHARED_SOURCE.md`](SHARED_SOURCE.md); [`PLAN.md`](PLAN.md) §3).
**Kind:** tooling. **Feasibility:** MEDIUM — unknown is whether block edges are
detectable without PSX-side anchors. _To fill._

**Outcome** _(2026-10-21)_: folded into [I44](#i44--the-data-image-as-typed-source-the-next-step-toward-reconstructed-source)
by the owner. With every function ours its use is the source's layout - which
of our files match Capcom's translation units - so it belongs to the typed
source's later steps, not to an idea of its own.

## I6 — Demo playback as determinism oracle

**Ask (2026-09-18):** [`prior-art/tr1x.md`](prior-art/tr1x.md).
**Kind:** tooling. **Feasibility:** MEDIUM.

**It exists (owner, 2026-09-19):** idle on the start screen and the game plays
through several areas with story text. Not FMV — on the PSX it ran on the
overlays and the normal area code, so it exercises the real engine. Not a full
attract mode: no recorded gameplay input is known, so expect it to police area
load, script, text and rendering rather than battle.

**First recording, 2026-09-19 ([`attract-mode.md`](attract-mode.md) §5):** the
sequence is exactly periodic — 5,522 logic frames, eight cycles of eight
identical to the frame — and `Rand` turns out to be deterministic by
construction (seed 1, no `srand`, no clock reachable from game logic). A
launch-to-launch comparison then **passed** the same day — four fresh launches
identical to the frame and to the `Rand` call, including one with our two
replaced functions active — and `tools/attract_run.py` + `attract_diff.py` are
the working harness. **State: built, as an external sampler.** Left: an
in-process logger, a richer state hash, and receipts.

**The unknown was determinism**, not existence: what drives the sequence
(script, timer table, recorded pad data), whether two runs produce the same
state, and whether anything in it reaches `Rand` `0x5B93D2` (CRT `rand()`, so
the seed matters). **First step:** find the idle timer on the title screen and
the area list it walks — the sibling's overlay roles may name the PSX side —
then log `LoadDatFile` indices across two idle runs from the injected DLL and
diff them. A replay oracle must run in a known configuration, or every ledgered
change reads as a failure ([`prior-art/tr1x.md`](prior-art/tr1x.md) §3).

## I7 — Replace MCI/VFW with a bundled video decoder

**Ask (2026-09-19):** stop playing FMV through Windows' MCI/VFW stack and decode
it in-process instead. **Kind:** platform. **Feasibility:** LOW.
**Gated on:** the renderer phase ([`PLAN.md`](PLAN.md) §5).

Costed in full in [`replacing-mci.md`](replacing-mci.md) — read that, not this.

### What already exists
The whole surface is one function, `Fmv_Play` `0x59E360`, with two call sites,
and it is **modal and blocking**: no frame-loop integration to design, and FMV
never composites with the game's renderer.

### What is missing
A decoder, pacing and A/V sync, and a licensing decision on the codec
([`LICENSING.md`](LICENSING.md) §4 — copyleft is disqualifying, H.264 patents
are a real cost).

### Why it is LOW and not scheduled
The compatibility argument is already spent: DIV-0001 fixed the one broken video
for the cost of one ffmpeg command. There is no higher-resolution source to
unlock — the assets are 640x480 and always were. What remains is dropping the
last OS multimedia dependency, and that is best done *with* the presentation
layer, because `Fmv_EnterFullscreen` `0x59E4F0` does an exclusive-fullscreen
`SetDisplayMode(640, 480, 16)` that renderer work has to remove anyway. Doing it
sooner means integrating twice.

### First concrete step
Answer [`replacing-mci.md`](replacing-mci.md) §7: does anything call `Fmv_Play`
with `fullscreen = 0`? A windowed path already existing would change the shape
of the work.

### Outcome
_(open when written) - overtaken 2026-09-23: the owner chose MCI into the
window over a bundled decoder (DIV-0035, [`window-modes.md`](window-modes.md)
§3); `Fmv_Play` `0x59E360` is ours and the exclusive mode-set is gone. The
idea stays on the list._

## I8 — Replace the DirectDraw / `IDirect3D3` presentation layer

**Ask (2026-09-19):** retire the port's 1998-vintage display path — DirectDraw
surfaces with a Direct3D immediate-mode device attached — for something a
current Windows actually supports. **Kind:** platform. **Feasibility:** LOW.
**Gated on:** [`PLAN.md`](PLAN.md) phase 3, which names the platform layer
("windowing, input, file I/O, DirectDraw replay") as the *first* decompilation
target precisely because it is the least game-specific.

### What already exists
The inventory, in [`media-stack-survey.md`](media-stack-survey.md) §1 and §4:
`DirectDrawCreate` + `IDirect3D3` / `IDirect3DDevice3` / `IDirect3DViewport3`,
identified from the error-string set (`Direct3D3 QueryInterface Error!`,
`Create ZBuffer Error!`, `Setup Texture Format Error!`, …). On Windows 11 this
is served by the `d3dim700.dll` compatibility shim.

**There are two independent DirectDraw users**, and only one of them is read:

- **FMV** — `Fmv_EnterFullscreen` `0x59E4F0` creates its *own* DirectDraw object
  and does an exclusive-fullscreen `SetDisplayMode(640, 480, 16)`. Fully
  disassembled ([`replacing-mci.md`](replacing-mci.md) §2).
- **The game's renderer** — surfaces, flipping chain, z-buffer, texture format
  negotiation. **Not yet read at all.** This is the actual subject of this idea
  and its size is unknown.

### What is missing
A measurement of the second one. Nothing here should be costed until the
renderer's DirectDraw usage has been disassembled the way the FMV path now has
been — the FMV path was small and self-contained, and it would be a mistake to
assume the renderer is.

### Why it is LOW and not urgent
**The game launches, reaches the start screens, and plays the intro level on
this machine** (owner, 2026-09-19). So the legacy display path *works* today,
including the exclusive-fullscreen mode-set, and this is not a compatibility
emergency. It is a known-fragile foundation on a deprecated shim, with the
defects already named in [`STATUS.md`](STATUS.md) step 0 (fullscreen fallback,
resolution handling) sitting on top of it — a thing to replace deliberately,
during phase 3, not to rush.

It is also the gate on several things people actually want: resolution
independence and finished widescreen are [`PLAN.md`](PLAN.md) phase 5 items that
cannot land while presentation is a 640x480-shaped DirectDraw blit.

### First concrete step
Disassemble the renderer's DirectDraw/Direct3D usage from the error strings'
xrefs, as was done for FMV, and write it up: which interfaces, which surface
formats, where the mode-set lives, whether the fullscreen fallback is in this
code. Output: a `kebab-case.md` note and symbols. That is a read-only
investigation, it needs no phase, and it converts this entry from a guess into
an estimate.

### Outcome
_(open when written) - built 2026-09-23: Direct3D 11 behind DirectX 6's
objects, no exclusive display mode (DIV-0031,
[`render-backend.md`](render-backend.md), [`display-setup.md`](display-setup.md))._

## I9 — Integer-scale the picture and extend the view into the remainder

**Ask (owner, 2026-09-19):** instead of stretching the picture to the display,
scale it by the largest whole multiple that fits (320 → 640 → 960 → …) and use
the leftover screen area to **show more of the scene** rather than stretching
or letterboxing. **Kind:** game behaviour (the visible field changes), on top
of platform work. **Feasibility:** LOW. **Gated on:** I8 / [`PLAN.md`](PLAN.md)
phase 3 for the scaling half; the field-and-camera code being ours for the
extension half.

### The rule, stated precisely
For a display of W x H: `k = floor(min(W / 320, H / 240))`, and the logical
view becomes `(W / k) x (H / k)` — never smaller than 320x240, every logical
pixel exactly `k x k` physical pixels. Examples: 1920x1080 → k = 4, logical
480x270; 3440x1440 → k = 6, logical 573x240; 2560x1440 → k = 6, 426x240. The
view grows by at most one scale step in each axis, so the extension is modest on
16:9 and large only on ultrawide.

This assumes the game's logical picture is 320x240, as on the PlayStation.

**2026-10-13, the ultrawide round:** the owner chose fixed presets over the
`W / k` rule - 426 x 240 (16:9) and 560 x 240 (21:9, 120 columns a side; 573
would have fitted 3440 x 1440): [`ultrawide-frame.md`](ultrawide-frame.md),
DIV-0041 amended.

**Observed 2026-09-19 (owner's side-by-side screenshots, same room and same
save prompt, sibling recomp at 3x nearest vs the port's 640x480 window; not
committed — game imagery). Visual evidence, not yet a code read:**

- **Same framing.** Both show the same extent of the room, so the logical view
  is still 320x240; the port maps it 2x onto 640x480.
- **It is a real 640x480 render, not a doubled 320x240 image.** Polygon edges
  (floor boundary, the window's light shaft) are clean at 640 resolution with
  no 2x2 stair-stepping.
- **Textures are magnified with a smoothing filter.** The plank grain, the
  bedding and the character sprite are soft where the PSX picture is blocky —
  the look of bilinear texture filtering on the Direct3D device, applied to
  the unchanged low-resolution PSX art. This is the "something odd about the
  artwork": sharp geometry edges around blurred texels, and filtered sprite
  edges against their transparent key. Which filter, and whether it is set per
  primitive, needs the renderer read (I8's first step).
- **Text is native 640x480.** Hanzi strokes are one physical pixel wide, i.e.
  half a logical pixel, and the window frame is a thin redrawn border rather
  than the PSX's textured one. The port has a separate high-resolution text
  and window path. `TextOutA` is *not* it: its single call site is in
  `0x5A66B0`, fed by a once-a-second frame-count `sprintf` in WinMain's loop —
  a debug FPS counter.

Consequence for this idea, and it is good news: the renderer already
transforms logical coordinates by a scale factor into a hardware-rasterised
target. **Scaling by `k` can mean rasterising at `k` times, not blitting a
finished 640x480 image** — crisp edges at any multiple, with the texture
filter becoming a choice (nearest for the PSX look, bilinear for the port's).
The text path needs its own answer at `k` > 2: its glyphs are authored for
exactly 2x.

### What already exists
- **What "stretching" is today.** In fullscreen the port sets a 640x480 display
  mode and the monitor or GPU scales it — the stretch is not the game's code,
  and neither is its filtering. In a window it is fixed at 640x480 with no
  scaling at all. So the artefacts are uneven pixel duplication from a
  non-integer display scale (1440 / 480 = 3 is clean; 1080 / 480 = 2.25 is
  not), which is what integer scaling cures. If the complaint is true
  *tearing* — a horizontal split during motion — that is vsync on the flip
  (`0x5A66B0`, unread) and a separate, smaller fix.
- **Evidence the view can be widened.** `bof3ext` ships partial widescreen as
  patches to "hardcoded draw-range floats" (its `docs/architecture.md`, module
  `:widescreen`; cited, not copied — `CLAUDE.md` rule 5). So the limits are
  constants in the binary, and widening them mostly works. Its README also
  says the result is unfinished, which is the honest size of the second half.
- The port kept the PSX GPU packet model ([`PLAN.md`](PLAN.md) §3), so a wider
  view is a wider draw area and clip rectangle, not a new renderer.

### What is missing
Everything that assumes 320x240, none of it inventoried:

- **Culling and tile ranges** — what `bof3ext` patched. Field maps draw a fixed
  window of tiles; sprites are culled against the old screen edge.
- **Map edges.** Many areas are barely larger than one screen. Extending the
  view shows the void past the edge, so the extension must be capped to what
  the map has (there is no camera clamp to change,
  [`ultrawide-culls.md`](ultrawide-culls.md) 5; at 21:9 the census entered
  three areas narrower than the picture, 31, 90 and 53).
- **Staging that relies on the screen edge.** Actors waiting just off-screen
  for a cutscene entrance become visible; so do spawn and despawn pops.
- **Fixed-size full-screen art** — battle backgrounds, the title, fades and
  wipes, FMV. These cannot extend. They need a per-screen policy: centre with
  borders, or fall back to plain integer scale.
- **UI anchoring** — windows and the dialogue box are placed in absolute
  320x240 coordinates (`MsgBox_PlacementTable` `0x66AE10` is a table of them).
  Centre, or anchor to edges, case by case.
- **Battles** are a 3D scene with a fixed camera and may extend more cheaply
  than the 2D field — unknown.

### Why it splits in two
**Integer scaling alone is the cheap, safe half**: it is pure presentation,
changes nothing the game logic can observe, and falls out of I8 almost for free
(render to a 320x240 or 640x480 target, blit at `k`, centre, border). It still
gets a [`DIVERGENCE.md`](DIVERGENCE.md) entry — the picture a player sees
changes — but a one-line one, default-on, trivially reversible.

**View extension is the expensive half** and is real game-behaviour divergence:
it changes what the player can see, per area. It should be a separate toggle,
default off until the per-map problems above are handled, and it must be off
for any attract-mode oracle run (I6).

### First concrete step
Read-only, needs no phase: measure the logical resolution and find the limits.
(1) From `0x59EE50` (the per-frame draw call in WinMain's loop) and
`0x59EA70` (the image upload, [`asset-loading-path.md`](asset-loading-path.md)
§2), establish whether packets are drawn at 320x240 and doubled, or at 640x480.
(2) Locate the draw-range constants `bof3ext` patches, independently — search
`.rdata` for the floats and integers 320 / 240 / 160 / 120 and xref them — and
list which functions own them. Output: a note sizing the second half honestly.

### Outcome
_(open when written) - integer scaling built 2026-09-23 (DIV-0036, then
DIV-0042: `k` follows the resizable window); the extension built as a fixed
426 x 240 picture under `BOF3X_WIDE=1` (DIV-0041, a survey build,
[`widescreen.md`](widescreen.md)) rather than this entry's `W / k` rule._

## I10 — Uses for the call-trace data

**Ask (2026-09-19):** owner, after [`call-trace.md`](call-trace.md) produced
per-function counts, call edges and a per-frame call hash identical across
launches: what can be done with it? **Kind:** tooling. Ranked by value.

1. **Regression check for takeovers.** Run the frame hash original-vs-ours: a
   faithful reimplementation makes the same calls into still-original code, in
   the same order. Catches a wrong loop count or early-out that leaves `Rand`,
   message and area intact, at the frame it happens. Wrinkle: a return address
   inside our DLL differs from the original's, so callers inside an owned
   function must be mapped to that function's identity.
   **State:** passed 2026-09-19 for all ten owned functions —
   [`call-trace.md`](call-trace.md) §7. Negative control fails as it should.
2. **Takeover work queue (I4 proper).** The reached functions are the ones
   testable today. Order leaves-first by call edges, weight by call count. The
   never-reached remainder is the honest "cannot verify yet" set. **State:** built 2026-09-19,
   `calltrace.py queue` — call-trace §9.
3. **Logic/presentation seam for I8.** The speed-dependent functions plus
   everything called only from them are a measured first cut of the
   presentation layer; the edges from logic into that set are the interface a
   replacement renderer implements. One query over `callcounts.tsv`. **State:** measured
   2026-09-19, seven functions — call-trace §6.
4. **Subsystem map.** Cluster by first-call frame and caller (the area-load
   burst, the message-box group, per-task callees) and cross-check against the
   source-file boundaries of [`SHARED_SOURCE.md`](SHARED_SOURCE.md) — evidence
   for I5 from a second direction.
5. **Name-transfer features.** Call frequency and edge shape ("once per frame",
   "8 per frame beside `Rand`") as matcher features; if the sibling can trace
   its PSX attract run the same way, a third transfer technique. **Unknown:**
   what tracing the sibling has.
6. **Two reads the data points at.** (First one done 2026-09-19: frame
   skipping, call-trace §6.) What gates `0x461FC0`'s image upload
   (call-trace §6 — it sits in front of the `Gfx_LoadImage` takeover), and the
   four hottest unnamed functions `0x5A7BF0`, `0x5A8380`, `0x5B3760`,
   `0x5A8340` — what they are is unread; do not guess.
7. **Scripted input.** The in-process frame counter makes input on an exact
   logic frame possible: title menu, save load, a battle — coverage past the
   attract sequence's 18%. Biggest item. How the game takes input and what the
   first scripted path should be: ask the owner, do not assume.

**Suggested order:** 1, then the `0x461FC0` read, then 2.

## I11 — In-process crash reporter

**Ask (2026-09-19):** owner, after the first crash seen in play
([`known-defects.md`](known-defects.md) D4): "we should probably get a crash
detection / logging tool". **Kind:** tooling. **Feasibility:** HIGH — the
call tracer already installs a vectored exception handler in the game
(`src/hook/calltrace.cpp`); this is the same mechanism pointed at faults.

**What it would do:** on an unhandled access violation (or any fatal
exception) write to `bof3x.log`: exception code and address, the faulting
address resolved to a `symbols.toml` name or nearest function entry, all
registers, a stack scan for return addresses inside `BOF3.exe` and
`bof3x.dll`, the logic frame, the area word, which functions were ours in this
run, and the last few files opened; then write a minidump next to the log
(`MiniDumpWriteDump`). A companion `tools/crash_report.py` to read a dump
offline — D4 was diagnosed with exactly such a throwaway script.

**Constraints:** the handler runs on whatever stack faulted, possibly a 16 KB
task stack, possibly a corrupted one: static buffers, no allocation, and the
dump written from a separate pre-created thread. Dumps are game-derived and
never committed (CLAUDE.md rule 1).

**Found along the way:** WER already leaves full dumps in
`%LOCALAPPDATA%\CrashDumps` on this machine, and the Application event log
has the fault offset — enough to diagnose D4 without any tool of ours.

## I12 — Let the game run while its window is not in front

**Built 2026-09-23 as DIV-0033** with the WinMain / WndProc takeover
([`window-modes.md`](window-modes.md)): on by default, the pads read zero
while the window is not in front, `BOF3X_BACKGROUND=0` or the launcher's box
restores the freeze; the debt clamp beside it is DIV-0034. The harness's
`attract_run.py --no-front` uses it. What is below is the ask and the
reading it was built from.

**Ask (2026-09-19):** owner, after an evening of attract-oracle and memory-dump
runs that each took the PC away for two to five minutes: "It would be nice if
the game ran without requiring being in focus … just to be able to do these
comparisons without blocking me from the pc." **Kind:** tooling first, a
player-facing option second. **Feasibility:** looks HIGH for windowed mode —
the mechanism is already read — but untried.

**What is known** ([`windowed-mode.md`](windowed-mode.md), "Focus loss"):
WinMain's loop does nothing at all while the app-active byte `0x6BC63B` is 0;
`WM_ACTIVATEAPP` clears it and pauses sound (`0x587C30`), sets it and resumes
(`0x587B90`). On return the missed time is replayed unrendered, because
nothing clamps the frame deadline's debt. `tools/attract_run.py` works around
all of it by holding the foreground for the whole run, which is why a run
eats the keyboard ([`HANDOFF.md`](HANDOFF.md), traps).

**What it would take:** a switch (an environment variable for tooling, later
perhaps a setting) under which deactivation does not clear the byte. Open
questions, each a reason this is a session and not a one-liner: whether
DirectInput's cooperative level lets the game read anything, or needs to,
while in the background — for an attract run it must read *nothing*, which is
the point; whether DirectDraw presents to an unfocused or covered window in
windowed mode, and what exclusive fullscreen does (probably: out of scope);
whether sound should keep playing; and whether `attract_run.py`, `mem_dump.py`
and the call tracer then agree with a focused run — the logic should, being
deterministic from launch, and the existing oracle is the test. It changes
behaviour, so it is a [`DIVERGENCE.md`](DIVERGENCE.md) entry when built, and
it sits next to the unclamped-debt fix that file's "Focus loss" section
already calls obviously wanted.

## I13 — Save states: snapshot the running game, restore it, in ours and in the original

**Ask (2026-09-19):** owner: "state saving … would be a big help when we've
finished getting all we can out of the attract mode. If we could find a way to
inject states into the original for comparison sake, even better, but even
just in our exe it would be helpful." **Kind:** tooling - the oracle for
everything the attract sequence cannot reach (stage 2 and 3 of the order of
work in [`STATUS.md`](STATUS.md): menus, the system text pool, combat).
**Feasibility:** untried; the reasoning below says MEDIUM, and that the
"original too" half is not much harder than the "ours" half.

**Why this game is a good candidate.** Nothing here is measured for this
purpose yet; each point is an existing finding read with a save state in mind.

- **It is one flat image at a fixed base** (`/FIXED`, no `.reloc`), and game
  state lives in its `.data`/`.bss`: the DAT arena, the VRAM shadow, the queues.
- **The coroutine stacks are in that image too.** The four tasks run on
  0x4000-byte stacks carved from a static arena ([`SCAFFOLDING.md`](SCAFFOLDING.md)
  §3, [`attract-mode.md`](attract-mode.md) §2), so a snapshot taken *between*
  logic frames, in WinMain's loop, captures every task's suspended stack as
  plain data, and needs no thread context at all - the main thread is at a
  known place with nothing of interest on its stack.
- **The port is deterministic from launch** and logic cannot reach a clock, so
  a restored state should replay identically - which is also the test that a
  restore was complete: restore twice, compare frame hashes.
- **Most of what is NOT plain memory is rebuilt on demand.** Direct3D textures
  are built lazily from the VRAM shadow and the texture cache can be emptied
  (`Gfx_InvalidateTextures` over the whole shadow); the converted palette rows
  are regenerated from the shadow (`Gfx_ConvertRow`). Both are ours now.

**The hard parts, as far as can be seen from here.**

- **The CRT heap.** Glyph data, sound banks, palette-row buffers and whatever
  else is `malloc`ed hold state, and `.data` holds pointers into them. Within
  one process a restore can copy the heap blocks back in place. Across
  processes the addresses move. The way through is probably to **own
  `Crt_malloc` / `Crt_free`** with an arena at a fixed address - then the heap
  is one more region to snapshot, and its addresses are the same in every
  launch.
- **Sound.** DirectSound buffers and the streaming MP3 decoder's position are
  outside the image. For a logic oracle they may not need restoring at all;
  for a state a person plays from, they do, or the music restarts.
- **Open files and the file layer's 16 slots** - ours already, so inspectable.
- **Anything in DirectInput or the window** - probably nothing that matters
  between frames.

**The original, for comparison.** The injected DLL is present in an
all-original run too (`BOF3X_ORIGINAL=*`), and a state loader is tooling in
the DLL, not a game function - so the same restore can run with every game
function Capcom's. The one catch is the heap: if stable addresses need our
allocator, then "original" means original except `malloc`, and that has to be
said wherever such a comparison is reported.

**First experiment, when this is picked up:** in one process, at the
between-frames point, copy `.data`/`.bss` and the heap blocks aside, run 300
frames recording the frame hash, copy everything back, run 300 again. Equal
hashes says the state is complete for logic; the first difference says what
was missed.

_(2026-10-17) staged as [`quicksave-round.md`](quicksave-round.md): the owner's F5 / F6 / F7 design, full snapshots with a save block in front; the all-original half left out._

**Outcome** _(2026-10-21)_: built 2026-10-19 as the quicksave round, merged
as PR #95 (DIV-0145..0147, D322): F6 / F7 save states and the F5 load menu;
its routes and proof in [`quicksave-routes.md`](quicksave-routes.md). Not
yet played by the owner;
the all-original half is not built.

## I14 — Compare what is drawn: display-list hash, texture hash, back-buffer hash

**Ask (2026-09-19):** owner, when the image path's takeovers reached functions
whose product is a Direct3D surface: "Could we hash screenshots, or compare
them some other way? That would allow us to compare specific frames (post
certain calls, or just at random) for any delta." **Kind:** tooling - an
oracle for rendering, and the regression check the text swap wants.
**Feasibility:** level 1 HIGH, levels 2-3 untried.

Three levels, cheapest and most deterministic first:

1. **The display list.** Each logic frame, game logic builds a PSX-style
   ordering table and packet pool (`Gfx_OtHeads`, `Gfx_PacketPools`,
   `Gfx_BeginFrame`). It is plain memory built by deterministic logic: hash it
   in-process at the frame boundary, next to - or inside - the call tracer's
   frame hash ([`call-trace.md`](call-trace.md) §6). It catches any change in
   *what* is drawn - which glyph, where, from which texture page and CLUT -
   and is indifferent to whether that frame was rendered, so the wall-clock
   problem does not reach it. OpenRCT2's lesson
   ([`prior-art/openrct2.md`](prior-art/openrct2.md) §2.3): compare the paint
   calls, not the pixels - a structured difference says what broke. It says
   nothing about the renderer itself. Needs the packet formats read far enough
   to walk a list (the draw `0x59EE50` is the reader).
2. **Textures.** After a cache entry is built or refreshed (`0x5A0080`,
   `0x5A0510`), lock the surface and hash its pixels. This is the check that
   lets those builders be taken over. Deterministic given the VRAM shadow, the
   CLUT row and the key - for one display pixel format.
3. **The final frame.** Hash the back buffer in-process, after the draw
   returns and before the flip `0x5A66B0` - not a screenshot from outside,
   which adds capture timing and the window manager. Rendering happens only on
   frames the wall clock allows ([`call-trace.md`](call-trace.md) §6), so two
   runs render different sets of frames: **key each hash by logic frame and
   compare the frames both runs rendered.** Whether DirectDraw / Direct3D
   output is bit-exact from run to run on one machine is unknown - an
   original-vs-original pair is the noise floor, as for every other check
   here. On a mismatch, write both frames and a difference image; the point of
   an image check is that a person can look. Perceptual hashes are the wrong
   tool: they exist to ignore small differences, and a glyph one pixel off is
   the difference being looked for.

"After certain calls, or at random" works at all three, the hook being
in-process; random frames have to be chosen by logic frame from a fixed seed,
so that both runs choose the same ones.

**What this grows into:** a VRAM-shadow snapshot plus one frame's display list
is this game's equivalent of an emulator's GPU dump - replayable through the
draw path alone, original and ours on the same input, no game logic, no focus,
no minutes of attract sequence. That is the test harness [I8](#i8--replace-the-directdraw--idirect3d3-presentation-layer)
needs, and a near relation of I13.

How other projects do it, beyond OpenRCT2, is from general knowledge and not
surveyed for [`prior-art/`](prior-art/): emulator CI that replays recorded GPU
command streams and hashes the frames (Dolphin's FifoCI, PCSX2's GS dumps),
and exact-hash sets of accepted images triaged by people (Skia Gold).

**Outcome** _(2026-10-21)_: overtaken, the owner: the A/B/C harness
([`abc-harness.md`](abc-harness.md)) and the state hash
([`state-hash.md`](state-hash.md)) do the comparing this was for.

## I15 — A look toggle: clean / sharp against soft / CRT-like

**Asked for by the owner, 2026-09-20**, after seeing what the port's bilinear
filter does to text. Two looks, switchable in game:

- **Clean**: point sampling (DIV-0012 already does it at launch,
  `BOF3X_FILTER=point`), exact 2x sprites (DIV-0010).
- **Soft**: the port's bilinear look - or better, something that is actually
  CRT-like (scanlines, a slight bloom) rather than merely blurred. That half
  is a post-process and belongs with the presentation layer's replacement
  (I8); under DirectDraw / `IDirect3D3` there is nowhere to put it.
  *Since 2026-09-23 there is: the Direct3D 11 backend (DIV-0031) carries two
  CRT looks, DIV-0037 and SatPixie DIV-0043, chosen in the launcher's Look
  box.*

What a LIVE toggle needs: the two `SetTextureStageState` calls made again on
the device at `[0x7CC350]` (vtable `+0xA0`, stage 0, states `0x10` / `0x11`,
value 1 or 2) - safe from the render thread between frames; `Gfx_BeginFrame`
is ours and is the obvious place. And a key: **the game's input path is
unread**, which the owner expects to come with a controller-mapping pass. F8
(windowed) shows the port does read function keys somewhere; that reader is
the place to start. *Since 2026-09-24 the input path is read and ours
([`controls.md`](controls.md) §1, DIV-0050), and our WndProc already takes
F1 for DIV-0048: what I15 still needs is only the live toggle key and the
two state calls.*

**Outcome** _(2026-10-21)_: sunset by the owner. The looks stay as built,
chosen in the launcher; the live toggle key is not wanted.


## I16 — Exact frame pacing: the deadline in a double

**Done 2026-09-24 as DIV-0047:** the deadline is base + frames x period in a
double, the period 1001 / 30 ms (29.97, the owner's choice); measured 29.971
a second, the per-frame call hash identical to the latest reference on all 10,319 frames.

**Ask (2026-09-21):** the owner, choosing DIV-0022 (the game's clock starts
with the game) as the short-term fix for [`known-defects.md`](known-defects.md)
D5, asked for the complete fix to be kept as a long-term idea.
**Kind:** platform   **Feasibility:** MEDIUM   **Gated on:** nobody - it
means owning part of WinMain's loop, which nothing does yet.

**What DIV-0022 leaves.** The frame deadline at `0x6BC628` is still a 32-bit
float. Counting from the game's start keeps it small, but not exact: each
frame's +33.334 ms rounds to the float's spacing, so one long session drifts
through bands - within 0.1 ms under 35 minutes, then 30.1, 29.85, 30.3 and
29.4 logic frames a second up to 9.3 hours, 31.25 (4% fast) from there to
6.2 days, then half speed, and past 12.4 days of one unbroken session no
drawing at all. The 2^32 wrap path (the float at `0x5C4214`) is unreachable
under DIV-0022 and was broken before it: near 2^32 the float's spacing is
512 ms.

**The fix.** Keep the deadline where its precision does not depend on its
size: a double, or 64-bit integer milliseconds (or `QueryPerformanceCounter`
ticks), advanced by exactly 33.334 ms - or a count of frames times the
period, which cannot drift at all. The four places in WinMain that touch it
are known ([`HANDOFF.md`](HANDOFF.md), the D5 item): the first deadline at
`0x4FCDB0`..`0x4FCDC4`, the render-or-skip compare at `0x4FCE24`..`0x4FCE3D`,
the spin at `0x4FCEBC`..`0x4FCEDC`, and the advance and wrap at
`0x4FCF0F`..`0x4FCF3A`. Either take over WinMain's loop, or re-aim those
four stretches at helpers of ours, keeping the float's slot unused.

**Decisions it brings, the owner's:** the rate - 33.334 ms (29.9994 a
second, what the code says) or the PlayStation's NTSC 29.97; and whether to
clamp the deadline's debt at the same time. That debt is the fast-forward
after focus loss ([`windowed-mode.md`](windowed-mode.md), I12, DIV-0004's
"not fixed by this"), and would be its own ledger entry.

**29.97 needs this first (checked 2026-09-21).** The frame period is the
double 33.334 at `0x5C4218`, read once (`0x4FCF15`, the advance), and the
first deadline's 33.34 at `0x5C4220` once (`0x4FCDBE`); nothing else in the
exe reads either, and logic counts frames, so changing the period breaks no
logic. But through the float deadline it changes almost nothing: simulating
the x87 sequence (float + double, stored as float) over 3,000 frames,
33.334 and 1001/30 (29.97) give 29.994 against 29.966 in the first four
minutes, and **the same rate** from about 20 minutes on (29.963, 30.075,
30.303, 29.412, 31.25 at 20 minutes, 1, 3, 6 and 12 hours) - the 0.033 ms
between them is below the float's spacing. With the deadline in a double
(or frames times the period) the rate is one constant. What else runs on
wall-clock time and would notice the 0.1%: the music (DirectSound, fed from
the spin by `0x587C70`) and the AVIs (their own clock) - both already
independent of the frame count. Whether an in-game play-time counter
assumes 30 frames a second is unread.

**Why not now:** DIV-0022 fixes what players meet (Windows' uptime, which
Fast Startup carries across shutdowns) with four bytes and no game code
touched; the rest needs a session of six-plus hours to show.

## I17 — Fast-forward: a shorter frame period while a key is held

**Done 2026-09-24 as DIV-0048:** F1 toggles the period halved (2x), the
deadline rebased at the change; a toggle rather than a hold, the owner's
2026-09-24 wording.

**Ask (2026-09-21):** the owner asked whether a 2x / 4x fast-forward is best
done by cutting the frame period, or whether that is a bad mechanism.
**Kind:** game behaviour (opt-in)   **Feasibility:** MEDIUM   **Gated on:**
an input reader for the key (the game's keyboard path is unread, as for
I15); exact rates want I16. *(Both met by 2026-09-24: our WndProc takes the
key, I16 is DIV-0047.)*

**The mechanism: yes, cut the period** - 16.667 ms for 2x, 8.333 ms for 4x.
Why it suits this game (read 2026-09-21, WinMain's loop):

- **Logic never sees it.** Logic counts frames and reads no clock, so a
  shorter period only changes how many logic frames run per second of wall
  time. Saves, the RNG, scripted events: identical to 1x. The oracle and the
  frame hash would not change.
- **The frame skip already exists.** When the loop is late for its deadline
  it runs the next logic frame without drawing - the same path the game takes
  catching up after focus loss. At 4x, when drawing cannot keep up, draws are
  skipped on their own. DIV-0004 drains the upload queue after unrendered
  frames. Seen working 2026-09-21: the D5 run at base 2^30 went at 91.7
  logic frames a second (DIV-0022's `analysis/attract/clk_1073741824*`).
- **Music keeps being fed.** The spin to the deadline starts with a call to
  `0x587C70`, the stream pump. A late frame still passes through it once,
  so it is serviced at every speed.
- **It can be one runtime write.** The step is `fadd qword [0x5C4218]` every
  frame, read from memory (the double 33.334, one reader; I16). Code of ours
  that runs once a logic frame (`Gfx_BeginFrame`) can poll the key and
  rewrite that double - no takeover of WinMain. After I16 the step is ours
  anyway.

**Caveats:**

- **Rounding.** The float deadline rounds 16.667 and 8.333 as it rounds
  33.334. Under DIV-0022 that is small for the first hours, e.g. 17 or 16 ms.
  With I16 it is exact.
- **Sound.** Effects fire per logic frame, so at 4x they come four times
  as often. Music and the AVIs keep their own clocks and do not speed up.
  Muting effects while fast-forwarding is an option to decide.
- **Menus.** Cursor repeat and the like are per frame, so they run fast too.
  Held-to-fast-forward mostly avoids it.
- **Unrendered frames at high rates.** DIV-0004 covers the upload queue.
  Anything else that assumes a draw per frame is unknown, but the refocus
  catch-up has exercised the same path for years.

**The alternative:** own the loop and run N logic steps per drawn frame.
Steadier draw cadence, but it needs the WinMain takeover I16 starts.
Revisit only if the skipped-draw cadence looks poor.

**First concrete step:** find the keyboard reader (or poll
`GetAsyncKeyState` from `Gfx_BeginFrame`), write the double on a held key,
and measure pace with `attract_run.py` as for DIV-0022. It gets its own
ledger entry.

## I18 — F12 before shipping: disable it, or make it a true quicksave

**Ask (2026-09-23):** the owner, having tried F12: "I kind of like the
function. for now, lets leave it in, but probably disable it / replace it
with a true quicksave before shipping".
**Kind:** game behaviour   **Feasibility:** HIGH to disable, MEDIUM to
replace   **Gated on:** I13 for the true quicksave.

**What F12 is** (`Save_QuickWrite` `0x5809C0`, [`save-files.md`](save-files.md)):
the PC port's hidden key for an ordinary save, written anywhere, always to
slot 0 (`BISLPS00.DAT`), with no confirmation. Tested by the owner: pressed
in a battle, the save loads in the field where the party stood, out of
combat - the save block has no battle state, so it doubles as an escape.
Overwrites slot 0 silently (and the input recorder's F12 shot writes one).

**Before shipping, one of:**

- **Disable it** - a line in our WndProc (`src/game/win_main.cpp`), a
  ledger entry.
- **A true quicksave** - F12 takes a snapshot of the running game and a
  second key restores it, battles and events included. That is I13's save
  state offered to players: memory regions plus the display and sound
  state, restored between logic frames. Its own slot, never the player's
  save files.

In between, cheaper steps if the owner wants them: refuse it in battle
and events, give it a slot of its own, draw "Save OK" on screen (it only
reaches the log since DIV-0032).

**Outcome** _(2026-10-17)_: closed by the owner - F12 stays as it is; the true quicksave is the quicksave round ([`quicksave-round.md`](quicksave-round.md)).

## I19 — Screen curvature for the SatPixie look

**Ask (2026-09-23):** the owner, on finding the SatPixie look (DIV-0043)
has no curvature knob: "Maybe later".
**Kind:** look   **Feasibility:** HIGH   **Gated on:** nothing.

SatPixie (Conkwer's fork) deliberately removed the newpixie preset's screen
curvature - "straight screen" is its headline feature - so our port
(`src/render/satpixie.cpp`) has none. The original newpixie Slang preset
(hunterk's adaptation, same MIT / public-domain terms) keeps it: a UV warp
before the picture pass samples, and a border fade with it. Adding it back
is one more parameter in `BOF3X_SATPIXIE` and a slider in the launcher's
Options dialog, defaulting to 0 (straight, as SatPixie ships), and a line
in DIV-0043's entry saying it is our addition to the preset. Not "accumulate
modulation", which is the phosphor persistence between frames.

## I20 — A Config row that opens the physical binding screen in game

**Deferred 2026-09-24** by the owner, at the end of the controls work
([`controls.md`](controls.md)): "a button in-game which would launch the
mapper would be nice, but leave it as a future idea for now." What exists
is the dual approach - the launcher's Controls dialog maps keys and pad
inputs to PlayStation buttons (DIV-0050, `bof3x.ini`), and the game's own
Config screen swaps PlayStation buttons between actions as the disc did
(its panel back to the PlayStation's one icon column, DIV-0051).

**The shape, when wanted** ([`controls.md`](controls.md) §4.3): a seventh
Config row - the row tables `0x653808` / `0x653810` hold eight with two
unused, the panel draw `0x461710` loops six and opens a `0x21` x `0x0D`
frame, the caption is pool message `0xBC + row` - opening a screen we draw
with the game's frame, small font and cursor: fourteen lines, a PlayStation
input each, a keyboard cell and a pad cell, the device of a press deciding
the cell, cells drawn as our own overlay through the backend so the glyph
table is not spent. Bindings write to the physical layer and take effect
the next frame, as `Pad_Read` reads the live table.

## I21 — Furigana over the Japanese script, drawn by our message box

**Ask (2026-09-24):** the sibling repo shipped `jp_furigana`, every kanji
word's reading in a row above it. Do the same here, using the PC's larger
glyphs to show each reading at the kana font's native 12 px.
**Kind:** game behaviour
**Feasibility:** MEDIUM   **Gated on:** the plain Japanese overlay (I2);
the owner's call to take it into its own branch, because it wants a lot of
playtesting.

### What already exists
- **Glyph size.** `Text_EmitGlyph` `0x516D50` (ours) takes the quad's width
  and height and always samples the whole 24 x 24 glyph (`symbols.toml`).
  A Japanese kana is its 12 px sheet cell doubled to 24 x 24, so on a
  6-unit quad it shows the original cell texel for texel at the base 2x
  output. Two kana fit over each 12-unit kanji. The sibling repo had to
  shrink native 12 px glyphs (6 px strokes, then its 8 x 8 kana font); here
  the reduction is exact.
- **Placement.** `MsgBox_Step` and `Text_DrawString` are ours, so a reading
  can be placed by our own code. No hook, no `<0d>..<0e>` span, no single
  global `P`, and no need for the PC's shrink draw `0x4987E0` (unread).
- **The script side.** In the sibling repo:
  - readings from SudachiPy plus 47 chosen ones (`names/readings.toml`);
  - the encoder `tools/build_ruby_script.py`;
  - the reflow rule, owner's call 2026-09-09: re-author breaks, and spend a
    page break rather than a fourth row (`docs/FURIGANA.md`).

  Readings are not proofread there yet.
- **A mechanism for extra glyphs (DIV-0057, built the same night).** Pair
  codes already make `Text_DrawString` emit more than one quad for one code,
  with the advance from the kind-4 table, and they came with a census of
  every text path (docs/dialogue-localisation.md section 9). A reading
  could be carried the same way: a code standing for its reading's glyphs,
  drawn small and lifted above the word. The census also shows which draws
  are not ours (the 8-unit `0x516E70`, the effect draw `0x4987E0`). Only
  `MsgBox_Step` and `Text_DrawString` matter for dialogue.

### What is missing
- The PC box's inner height: an annotated row needs 18 units against the
  newline's 12. Two annotated rows should fit where three plain ones do.
  Measure it off a capture.
- A way for the script to mark which kana sit over which word, which our
  `MsgBox_Step` reads.
- Long readings over one kanji (志 こころざし): overhang or a gap. Count them
  from the sibling repo's reading data.
- The look under bilinear against point filtering (DIV-0012).

### First concrete step
A capture of one Japanese message with a hand-placed reading row, from a
field save, to settle height and legibility before any script work.

### Outcome
**BUILT 2026-10-22** in the reading-row round
([`reading-row-round.md`](reading-row-round.md)), ledgered as DIV-0155: the
optional layer `ruby-furigana-ja-JP`, built on the player's machine by
`tools/ruby_build.py furigana` ([`reading-row-builder.md`](reading-row-builder.md))
into kind-18 side chunks, drawn by `MsgBox_Step` as two row pairs a page
([`reading-row.md`](reading-row.md) 2 and 3), F4 to hide the readings; the
readings ship as the first mod zip (DIV-0157, [`mods.md`](mods.md)). Owed:
the owner's play, and the readings' proofreading (known-defects D328; the
readings follow-on in [`HANDOFF.md`](HANDOFF.md)).

---
## I22 — Cut content: what the disc holds that play never shows

**Ask (2026-09-25):** the owner brought TCRF's page on the game (saved as a
PDF) and picked out the unused spells and skills, the unused songs and
narration, the unused text, the whelp's menu portrait, and Sunder's
animation on Mt. Myrneg, which ends before the player can reach it and
should loop.
**Kind:** game behaviour
**Feasibility:** MIXED - the skills are located (their rows and PC entries,
[`cut-content.md`](cut-content.md) §2) and go with the spell round; the
loop wants the area's code ours first; the music, narration and text are
unread on the PC.
**First step:** the owner reaches the menu as the whelp with an F12 save,
to see whether the portrait exists on the PC; the spell round reads the
engine-side rows 123 and 128 first, which TCRF says crash and freeze.


---
## I23 — Music: the PC's MP3s against the disc's sequences

**Decided 2026-10-08 (the owner): the disc's music is the default wherever a
disc is a source** - [`unified-data-plan.md`](unified-data-plan.md) section 6
has the numbers behind it. This idea's measurement is done; its engine work
is the plan's step 9.

**Ask (2026-09-26):** how different are the PC port's MP3s from the
PlayStation's own music data played back properly - would disc music feel
like restoring the original sound, or be much the same?
**Kind:** tooling (a measurement; what it decides is engine work)
**Feasibility:** HIGH for the comparison - every input is the owner's own
files and an emulator. **Gated on:** nobody, but it needs the game files, so
it runs on the owner's machine.

### What already exists
- The disc's music is sequenced (VH/VB + SEQ in 81 `BGM*.EMI`); the PC's 166
  MP3s are recordings of it, renumbered ([`DAT_CONTAINER.md`](DAT_CONTAINER.md)
  §5).
- The PC loops by rewinding the decoder to the file's start
  ([`asset-loading-path.md`](asset-loading-path.md) §1a).

### What is missing
- The track map (exe table beside `BGM\%03d`), the MP3s' encoding parameters,
  where each SEQ's loop point lives.

### First concrete step
[`bgm-comparison.md`](bgm-comparison.md) §3, steps 1-3: inventory both sides,
then the loop test on three tracks. That settles the likeliest audible
difference (intros replayed or lost on loop) in an afternoon.

### Outcome
_(2026-09-26) open; method written._
_(2026-10-04) **asked for first**: the owner wants to hear the MP3s against
the sequences before the MP3 decoder's fate is decided
([`platform-layers-plan.md`](platform-layers-plan.md) section 5); the first
step there is a listening set, a few tracks rendered both ways, side by
side. Still unrun._
## I24 — The scenario round: the chapter banks wave by wave

**Ask (2026-09-26):** the owner, after the chapter tables were walked
([`scenario-roots.md`](scenario-roots.md)): take the scenario banks over
"in a wave-by-wave approach like we're doing with the magic calls", and for
now document the plan and list it here.
**Kind:** engine
**Feasibility:** HIGH   **Gated on:** nobody - the function list is a
script's output; the live check per chapter wants a recipe save the owner
records, which can follow the fuzz rather than precede it.

### What already exists
- The list: 638 functions from the three chapter-indexed tables (`0x662C80`
  vtables, `0x660B84` / `0x660BD4` call tables), 17 already ours, in one
  band `0x537F20..0x56C080` in chapter order; `tools/scenario_roots.py`
  reproduces it and names every engine function the banks call (232).
- The pattern: the spell round's whole-overlay groups, its shared harness
  (`magic_harness`, [`magic_harness.md`](magic_harness.md)), its per-group
  docs and controls; chapter 16 already taken over the same way
  ([`field-modes.md`](field-modes.md)) as the template for a chapter's
  state machine.
- The catalogue's reason for ranking the banks last - discovery needing
  saves - is gone (scenario-roots §6).

### What is missing
- A harness for the three call shapes (vtable slot, call-table entry, state
  handler) over one frame of scenario state, from `magic_harness`.
- Names for the 20 vtables and the call and state tables in `symbols.toml`.
- One small chapter read whole to fix the harness's state table.

### First concrete step
Read SC0 (chapter 0, 23 functions at `0x537F20..0x539A30`) to its last
instruction and list the state it reads and the engine functions it calls;
then the harness, proved on SC0. The plan, groups and waves:
[`takeover-queue-scenario.md`](takeover-queue-scenario.md).

### Outcome
_(2026-09-26) open; planned, not scheduled._
_(2026-09-28) built: round ten's waves one to three took every chapter bank, the shared helpers, the call tables' block and the engine callees through `scenario_harness` ([`takeover-queue-round10.md`](takeover-queue-round10.md) §4, §7, §10); the live check per chapter is the owner's._

## I25 — The area round: the area overlays enumerated from their tables

**Ask (2026-09-26):** the owner: take the area overlays over "in a
programmatic manner instead of by singleton discovery - similar to how
we're doing magic captures and plan to do scenario capture"; then, after
the measurement, write the plan up on that shape with the effect-kind and
field-core code on rounds of their own, and a clean tool as the first step.
**Kind:** engine
**Feasibility:** HIGH   **Gated on:** nobody - the roots are seven tables
and the areas' data blocks, all in the exe; the live check per world wants a
recorded walk the owner makes after the fuzz.

### What already exists
- The measurement: a scratch re-aim of `tools/scenario_roots.py` at the
  area descriptor table `0x667590` and six more tables reaches 1,133 of the
  1,457 starts in the band `0x401000..0x430000`, and the per-area blocks
  come out in area order (1 of 161 out of order), so the block rule of
  `magic_rows.py` applies ([`takeover-queue-areas.md`](takeover-queue-areas.md) §1).
- The pattern: the spell round's whole-overlay groups and shared harness,
  the scenario plan's call shapes; DA's world-map areas and `Area29_*` /
  `Area33_*` already taken the singleton way.
- Ground truth per root: the sibling's `names/area_records.toml`, 728
  pairs.

### What is missing
- `tools/area_rows.py`, the clean tool (the scratch copy is not in the
  repo and splits the data blocks approximately).
- Names for the two unnamed engine tables `0x662CE8` and `0x662F28` and the
  cell hook `0x56E670`.
- An `area_harness` for the six call shapes over one field frame.

### First concrete step
Write `tools/area_rows.py` and let its report replace the plan's numbers;
then the harness, proved on a world 0 area the attract cycle enters. The
plan, groups and waves: [`takeover-queue-areas.md`](takeover-queue-areas.md).
The effect-kind (`0x460000..0x48FFFF`, through `EffectKind18_States`) and
field-core (`0x500000..0x52FFFF`, through the tables at `0x65E000..`)
functions the catalogue labels "Area overlays" are **not** this round's;
each is a round of its own with the same walker.

### Outcome
_(2026-09-26) open; planned, not scheduled._
_(2026-09-28) built: round ten's six waves took every area overlay of worlds 0..4 through `area_harness` and `tools/area_rows.py` ([`takeover-queue-round10.md`](takeover-queue-round10.md) §4..§19); the live check per area is the owner's, the world-map body shared once is [`round-10-cleanup.md`](round-10-cleanup.md) item 1._

## I26 — The boss round: the BOSS overlays enumerated from the engine's three root sets

**Ask (2026-09-28):** the owner: after the scenario and area takeovers the
boss scripts are the next biggest target - stage them "like we did area",
for round eleven; check the sibling for what it knows about the bosses
(it has met three or four more than this project has) to validate the
shape and the entry points; one fight is within reach of a recipe save,
the whole sweep is not.
**Kind:** engine
**Feasibility:** HIGH   **Gated on:** the area round finishing (round ten's
remaining waves); the live check per fight wants a recipe save the owner
records after the fuzz.

### What already exists
- The tool, `tools/boss_rows.py` ([`boss-rows.md`](boss-rows.md)): the
  three root sets (`Boss_SetupTable` `0x656954` by the event-battle byte,
  `BossKind_Table` `0x64B088` by the enemy's kind, the effect dispatchers'
  stack slots) reach all 541 functions of the band `0x437A00..0x441000`;
  525 to take in 123 units (55 set-ups, 61 kinds, 7 effect tasks) plus 20
  shared helpers; the set-ups' address order is the sibling's file order.
- The engine side: every caller of the roots and of the three hooks
  `0x904B64..6C` is ours (BG, round 7, `EnemyRunAll`, `BattleTask_RunAll`).
- Ground truth on the sibling: `docs/loader_records/BOSS.md` (the same
  56-entry table, entered by one `jalr`; 55 of 55 entries inside their
  file's section), `names/boss_records.toml` (id to file), twelve BOSS
  images resident across its sessions; no function inside a BOSS overlay
  named.

### What is missing
- Names for the two tables, the kind-3 dispatcher `0x4357D0` and the hooks;
  the `EnemyOp_StepsB..F` tables re-attributed to kinds 1 and 2.
- A `boss_harness` for the eight call shapes over one battle frame.
- Recipe saves before each fight (the owner's, after the fuzz).

### First concrete step
Name the tables (BH's), write the harness from `magic_harness`, prove it on
BH and BSA; then wave one (BH, BSA..BSE, 278 functions) and wave two
(BSF..BSJ, 247). The plan, groups and waves:
[`takeover-queue-bosses.md`](takeover-queue-bosses.md).

### Outcome
_(2026-09-28) open; planned and cut, not scheduled - after the area round._

## I29 — Button prompts drawn from the bindings in force

**Ask (2026-10-03, the owner, on the fishing banner):** the US disc's banner
names cross / triangle / square where the port's names circle / cross /
triangle. The owner's reading: the US release has different default bindings
from the Japanese and Chinese ones, so neither set of icons is wrong - each
shows its own defaults. Long term, a prompt should show the button the
action is bound to now (`docs/controls.md`, the launcher's key and pad
tables), whatever the language.

### What it needs
- The prompts that name buttons, listed: the fishing banner (DIV-0069), the
  world map's HUD, the config screen, the battle's hints.
- For each, which action the icon stands for, and the icon set (DIV-0051).

### Outcome
_(2026-10-03) open. Until then DIV-0069 keeps the port's icons, by the owner's word._

_(2026-10-17) built for the PlayStation shapes by the button unification round
([`button-unification-round.md`](button-unification-round.md)): one table, `σ`,
from the save's Config (DIV-0133); the starting Config by source (DIV-0134);
the world map's legend (DIV-0135), the fishing banner (DIV-0136), the dock
(DIV-0137), the item lists' corner (DIV-0138), the hard-coded tests (DIV-0139)
and the dialogue (DIV-0140). Keyboard key names and other pads' letters are
left for a later round (that doc 6)._
## I30 — Machine-readable docs: Open Knowledge Format frontmatter and a generated wiki

**Ask (2026-10-04):** the owner: Google is validating a spec for curated
knowledge as Markdown with YAML frontmatter
([Open Knowledge Format](https://github.com/GoogleCloudPlatform/open-knowledge-format),
OKF, v0.2, Apache-2.0). Could `docs/` adopt it, so the documentation indexes
itself and is published automatically as a human-readable wiki?
**Kind:** docs / tooling
**Feasibility:** HIGH   **Gated on:** nobody for the format; the owner's
choice to revisit once the spec is hardened or tested outside Google.

### What already exists
- `docs/` is an OKF bundle in everything but syntax. Measured 2026-10-04
  (`grep`/`awk` over `docs/**/*.md`): 279 documents, 2,927 inter-document
  Markdown links, 278 of 279 opening with a `**Status:**` line that carries
  a verification date (the exception is `README.md`, the index itself). The
  spec's `status` and `verified.at` are that line, unparsed; its link graph
  and backlinks are those links, uncomputed.
- The evidence rule and the three evidence tiers ([`README.md`](README.md))
  are the spec's trust tiers, applied per claim instead of per document.
- The spec's `index.md` is the hand-maintained 300-line table at the top of
  [`README.md`](README.md), the part of `docs/` most prone to going stale.
- The spec's "Attested Computation" (a computation, an executor that returns
  a receipt, a deterministic attester) is the shape every takeover doc's
  headline already takes: the harness command, the round count, the
  mismatch count. Noted as a fit, not proposed - see "What is missing".
- `tools/ledger_check.py` already parses `docs/` structurally in CI
  (`.github/workflows/checks.yml`) and is the natural home for a conformance
  check.
- The spec itself is small: `type` is the only mandatory frontmatter key;
  `title`, `tags`, `sources`, `generated`, `verified`, `status`
  (`draft|stable|deprecated`), `stale_after`, reserved `index.md`/`log.md`
  per directory; consumers must tolerate unknown `type` values, unknown
  keys and broken links. Apache-2.0 is compatible with
  [`LICENSING.md`](LICENSING.md); nothing would be vendored in any case -
  only field names are adopted.

### What is missing
- Frontmatter on 279 files. Mechanical: `type` derives from the naming
  rule (SCREAMING_CASE = subsystem, `area_`/`battle_`/`boss_`/`bsim_` =
  takeover group, kebab-case = investigation, `prior-art/` = prior art);
  `status` and `verified.at` derive from the Status line. Our vocabulary
  (`IN PROGRESS`, `MEASURED`, `SUPERSEDED (by X)`) is wider than the spec's
  three values; the spec permits extra values, so keep ours and map
  `STABLE`→`stable`, `DRAFT`→`draft`, `SUPERSEDED`→`deprecated` only in the
  renderer.
- A rule that the human-readable Status line and the frontmatter agree,
  enforced by `ledger_check.py`. Agents read the Status line; it stays.
- The wiki. OKF ships no renderer - its reference consumer is a Cytoscape
  graph viewer - so the wiki is ours: a generator in `tools/` emitting
  `index.md` per directory and a backlinks section per page, published by
  a workflow (MkDocs reads frontmatter natively and needs no code; a
  static-HTML emitter of a few hundred lines avoids the dependency).
- A rule-1 gate: publishing `docs/` makes it public-facing. The generator
  refuses to publish if a scan finds extracted game data in any page.
- Not proposed: per-claim `sources` footnotes across 1.3 million words of
  existing prose, or Attested Computation entries for the harnesses. Both
  fit; neither pays for itself retroactively. New documents may adopt them.

### Risk
The spec is v0.2 and self-described as proof of concept (17 open issues,
2026-10-04), with one breaking rename already between v0.1 and v0.2
(`timestamp`→`generated`). Exposure is five field names; a rename is a
`sed`. Frontmatter itself is standard Markdown practice whatever happens
to OKF.

### First concrete step
One session: a script that adds frontmatter to every document from its
filename and Status line, and a `ledger_check.py` check that the two agree.
No behaviour, no divergence. The wiki is a second step and waits on the
first.

### Outcome
_(2026-10-04) open; parked by the owner until the spec is hardened or
validated outside Google. Revisit then._

## I31 — The platform layers and the cutover

**Ask (2026-10-04, the owner, during round fourteen):** is there a plan to
take over the executable, runtime, decoder and renderer layers once the
game's own code is ours? There was not; make one.
**Kind:** engine / platform
**Feasibility:** MIXED - the read pass, the small layers and the runtime
boundary HIGH; the decoder MEDIUM, its shape waiting on I23; our own
executable MEDIUM, and a goal: the first of the phase 4 / 5 work (the owner,
2026-10-04).
**Gated on:** round fourteen's end; the decoder on I23 - the owner wants to
hear the MP3s against the disc's sequences before any replacement is chosen
(2026-10-04). Our own executable is not gated on a decision: the owner made
it the first goal, as the route to builds from the player's disc or PC game
and 64-bit builds ([`platform-layers-plan.md`](platform-layers-plan.md)
section 5).

### What already exists
The presentation layer, WinMain, the input path and `Fmv_Play` are ours; the
catalog counts 533 starts left in those layers; the tracer can prove that no
code of Capcom's runs.

### What is missing
A read of the 86 starts that are neither runtime nor decoder (some are game
code filed by address range), the list of runtime entry points the game
calls, a decoder behind the music seam, a loader that maps the player's data
sections into a process of ours.

### First concrete step
[`platform-layers-plan.md`](platform-layers-plan.md) section 4, step 1.

### Outcome
_(2026-10-04) planned, not scheduled._

## I32 — The PSP release's game-logic changes, as a config toggle

**Ask (2026-10-04, the owner, while shaping the asset cache):** "I do think
there were some game logic changes in the PSP version of the game" -
investigate, and offer them as a toggleable config option.
**Kind:** game behaviour
**Feasibility:** UNKNOWN - the premise is the owner's recollection and is not
measured. Nothing in `docs/` records a PSP logic change yet.
**Gated on:** nobody for the measurement; the toggle on what it finds.

### What already exists
- Both PSP discs are held and catalogued: `psp-jp` and `psp-eu`
  ([`fixtures.toml`](../fixtures.toml)).
- The PSP's code is readable: `PSP_GAME/SYSDIR/BOOT.BIN` is a plain MIPS ELF,
  a native port and not a PS1 image under an emulator
  ([`display-overhaul.md`](display-overhaul.md)). It has been read once
  already, for the widened view ([`psp-widescreen.md`](psp-widescreen.md)).
- Its data is PSX-format EMIs, and one section is known byte-identical to
  `psx-jp`'s (`fixtures.toml`, `psp-jp`). So the section-by-section diff that
  [`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 8 schedules for phase 4
  takes PSP pairs as well as PSX ones, with the same tool.
- The sibling's PSX name corpus, to pair PSP functions with their PSX
  originals.

### Leads: what players report (none of it measured here)

The owner, 2026-10-04: the PSP release is remembered as a graphical step down
from the PSX - compromises made for copyright reasons (the Balio and Sunder
fight) and the widescreen crop - with a few quality-of-life changes and bug
fixes that people liked. The owner wants to know how extensive the changes
were, catalogued as our own investigation meets them.

The list below is a fan wiki's, paraphrased, read 2026-10-04:
<https://bof.fandom.com/wiki/Breath_of_Fire_III#PS1/PSP_version_differences>.
The wiki cites no sources for it. Every row is a **lead**: a claim to confirm
or refute against the discs, not a fact about either release. "Where" is a
guess at where the difference would live, to be corrected by the measurement.

**Behaviour - the candidates for a toggle:**

| # | Claim | Where it would live | Status |
|---|---|---|---|
| P1 | Angel Tower's block-puzzle room: one passage is narrower, and random battles can no longer be used there to skip the puzzle | the area's map and encounter data | **data changes in 11 areas** (2026-10-06, [`region-diff.md`](region-diff.md)), none identifiable as Angel Tower by name; the code half unread |
| P2 | The Desert: a random encounter no longer turns Ryu to a random heading | field code, or the area's script | unmeasured |
| P3 | The item duplication glitch no longer works | menu / inventory code | unmeasured; whether the PC port has the glitch is also unmeasured |
| P4 | The Holy Mantle suppresses encounters better while walking straight, and a turn after a long walk brings one on | the encounter-step code | **refuted** (2026-10-06, [`psp-stallion.md`](psp-stallion.md)): the Mantle's record and every code path that reads it identical on PSX and both PSPs; the PSP's one encounter-path change (the render-load guard removed) applies to every encounter and the PC never trips it |
| P5 | The Factory's electric room runs slow throughout, which makes its lever sequence easier | unknown: a performance effect or a deliberate change, the wiki does not know either | unmeasured |

**Content - data changes, the "copyright" set:**

| # | Claim | Where it would live | Status |
|---|---|---|---|
| P6 | Stallion (Balio and Sunder fused) is recoloured, brown and blue | the boss's palette, an image section | **confirmed, palettes only** (2026-10-06, [`psp-stallion.md`](psp-stallion.md)): two 16-entry rows in areas 67 and 166 (a pale ramp with red accents to a brown ramp with blue-grey), plus the three palette-swapped variants in 166; sprite pixels, the enemy table, the AI rows and the BOSS024 module identical. A toggle: a palette layer from the player's PSP disc, about a day; an option, not a default |
| P7 | Stallion's signature attack is renamed | the ability name table | **confirmed for the English PSP** (2026-10-06, [`psp-stallion.md`](psp-stallion.md)): ability 115's name in the EU executable's table, numbers unchanged; the PC's name lives in the table the English overlay already rewrites (DIV-0008) - hours on P6's layer |
| P8 | One frame of Ryu's ascension animation, which showed another publisher's character, is removed | an effect's image or frame table | candidate rows found, not rendered (2026-10-06, [`region-diff.md`](region-diff.md)) |
| P9 | One character's name is respelled (Hachio) | text | **confirmed** (2026-10-06, [`region-diff.md`](region-diff.md)) |
| P10 | Some music tracks differ slightly in instrumentation | the sequence or sound-bank data, or the PSP's player | half settled (2026-10-06, [`region-diff.md`](region-diff.md)): every sample body is the PSX's, re-encoded; instrument and sequence data not compared (A's music investigation reads them) |

**Presentation and platform - recorded for the count, not for a toggle:**

| # | Claim | Status |
|---|---|---|
| P11 | 16:9 display | **measured**: [`psp-widescreen.md`](psp-widescreen.md) |
| P12 | A light bilinear filter over the sprites | unmeasured; ours is already a choice (`BOF3X_FILTER`) |
| P13 | A new title logo | candidate rows found, not rendered (2026-10-06, [`region-diff.md`](region-diff.md)) |
| P14 | The fishing minigame offered from the title screen, and shareable to a second PSP | candidate rows found, not rendered (2026-10-06, [`region-diff.md`](region-diff.md)) |
| P15 | Memory-card wording changed to the PSP's storage | candidate rows found, not rendered (2026-10-06, [`region-diff.md`](region-diff.md)) |
| P16 | Longer loads before battles from UMD; slowdown before attacks early in a session | platform behaviour, not ours to reproduce |
| P17 | *(found by the code diff, not in the players' list or the fan wiki's)* Myria's item: the PSP sets her first item word (the one a drop rolls and Steal takes) to accessory 0x17, "Ivory Charm"; both PSX discs (US, JP) hold consumable 0x17, "Napalm" (item word 0x0017, class 1 on both). The index is kept and only the category changes, so a slip in the PSX data is a plausible reading (a guess) | **confirmed** (2026-10-12, [`psp-battle-changes.md`](psp-battle-changes.md), [`psp-code-diff-battle.md`](psp-code-diff-battle.md) C2): a toggle, `psp=myria-item` (DIV-0108), on by default only under a PSP base (the owner) |
| P18 | *(found by the code diff)* The Beyd-Zig duel (chapter 6, area 92, event battle 0x1B) does not end on a loss unless the party was caught helping | **confirmed in code** (2026-10-12, C1): a toggle, `psp=duel-loss` (DIV-0108), on by default only under a PSP base; whether it is a repair, and whether a lost duel can stall, is for the owner's play |
| P19 | *(found by the code diff)* A PSP-only mode, entered from a five-way title menu, with 22 hooks, 19 of them in the fishing area (AREA030) | **found in code, not identified** (2026-10-12, [`psp-code-diff-field-1.md`](psp-code-diff-field-1.md), [`psp-code-diff-field-2.md`](psp-code-diff-field-2.md)): likely P14 (the owner: "probably fishing"), left for the owner |

Rows are added here as the investigation finds changes nobody listed, and a
row's status changes only with the measurement cited. The count of rows that
end "confirmed" is the answer to "how extensive".

Some leads may have a head start, each to be checked before it is relied on.
P6 and P7 concern a fight this project has a recorded route into (the
`balioAndSunder_2` recipe), if that route reaches the fused form. P2, P3 and
P4 are in field, menu and encounter code, much of which is taken over on the
PC side; where the function is ours, the PSP's has a named counterpart to be
read against.

### What is missing
- The measurement itself, in two halves. **Data:** `psp-jp` against `psx-jp`
  and `psp-eu` against the Western PSX discs, every difference sorted into
  text, layout or logic (tables, scripts, enemy stats). **Code:** the PSP ELF
  against the PSX code, function by function; widescreen and platform changes
  set aside, what is left is the list.
- For each change found: what it does, and whether it reads as a fix, a
  rebalance or a platform accommodation. The owner decides which are offered.
- The toggle. A data change is a layer in the asset cache, chosen like a
  language layer (`ASSET_SOURCES.md` section 3). A code change is a branch in
  our code under one option, in the launcher and as a `BOF3X_` variable.

### The rules it touches
- Rule 6: the PSP release is a third answer to "what it was", beside the PSX
  disc and the PC port. The finding records it as such; it does not replace
  either.
- Rule 2: an option the player turns on is not a divergence, but the ledger
  records that the option exists. A PSP behaviour made the *default* is a
  [`DIVERGENCE.md`](DIVERGENCE.md) entry.
- Rule 1: a data change must come from the player's own PSP disc through the
  importer. A patch that carries the changed bytes is shipping data.

### First concrete step
Add the two PSP pairs to the phase 4 region diff. If the data shows nothing,
the code diff is the second step and the larger one.

### Outcome
_(2026-10-04) open; sixteen leads listed, one measured (P11), none of the
behaviour or content rows confirmed._
_(2026-10-06) the data half measured ([`region-diff.md`](region-diff.md)):
P9 confirmed, P6 consistent, P8 / P13 / P14 / P15 candidates found, P1 data
in 11 areas, P7 only in the PSP's executable, P10 half (samples identical).
The PSP's data carries JP's code byte for byte and PSP-EU is PSP-JP plus a
language layer; changes nobody listed: 653 texture tiles blanked and 301
redrawn, 53 palette changes, the shoulder-button labels, an empty battle
message filled, 11 map-data changes. The code half (P1..P5) is the next
step, and the renders that would settle P6 / P8 / P13._
_(2026-10-06, evening) P6 and P7 confirmed and P4 refuted by reading the
PSP executables ([`psp-stallion.md`](psp-stallion.md)); the boss module
and the encounter paths are identical compiles of the PSX's. Unlisted:
three other ability renames, four item renames, one consumable flag bit
(consumable 87) in the EU executable. The region diff's one collision fix
is DIV-0080._

## I33 — Productisation: from our own executable to a finished product

**Ask (2026-10-04, the owner):** is there anything else to consider once we
have our own exe, and what steps lead to a more finished product? The list
below is the answer given, recorded at the owner's word.
**Kind:** engine / product
**Feasibility:** MIXED - each piece is ordinary work; none is measured or
planned in detail yet. The three under "Early" are decisions, not builds.
**Gated on:** the cutover ([`platform-layers-plan.md`](platform-layers-plan.md),
I31) for most of it; the owner for the three early decisions.

This entry is an index of pieces, each of which becomes its own entry or
plan when it is taken up. It leaves out what already has an entry: the crash
reporter (I11), save states (I13), fast-forward (I17), frame pacing (I16),
button prompts (I29), the video decoder (I7) and the music question (I23).

### What our own executable makes possible
- **A portable platform layer.** Once no code of Capcom's runs, window, input,
  audio output and timing can sit on a portable library in place of Win32.
  SDL is the usual choice and is zlib-licensed, so [`LICENSING.md`](LICENSING.md)
  section 4 is not touched. It brings Linux, macOS and handhelds, and
  controller hotplug and rumble. The platform plan already names 64 bits;
  this is the same step taken further.
- **Mods as cache layers.** [`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 3
  lays the cache out as a base and language layers. A mod is one more layer
  over the base, with a load order: art, retranslation, rebalance. Nothing is
  built for it; the cache format must only not rule it out. The launcher's
  Mods tab is kept for it (the owner, 2026-10-15: side-loaded DAT and EMI
  files - custom pixel art, scenario; [`launcher-settings.md`](launcher-settings.md) 2a).
- **Hardened loaders.** The original loaders trusted their own files. Ours
  read what a player or a mod supplies, so the asset and save parsers want
  bounds checks and a fuzzing pass.

### What makes it a product
- **Presets from the ledger.** Each [`DIVERGENCE.md`](DIVERGENCE.md) entry is
  a described behaviour change. Tagged by kind (fix, enhancement, option),
  the entries become presets a player picks in one choice: the PC port as it
  was, the PSX behaviour, the PSP's changes (I32), ours as recommended.
- **Settings in the game.** One config file and an in-game options screen;
  the launcher optional; the `BOF3X_` variables kept as developer overrides.
  I20 is one row of this.
- **First-run import.** The importer's face: point at a folder or an image,
  see which build was recognised and what is missing, and a plain refusal
  for an unknown build (`ASSET_SOURCES.md` section 1).
- **Saves.** In the user's profile, not beside the executable; a versioned
  format a later release can migrate; the original PC and PSX saves still
  imported (I1).
- **Accessibility.** Text speed, full remapping, hold or toggle. Each is a
  ledgered option.

### What makes it releasable
- **A release gate.** The state-hash references and the recorded routes
  become the test a build passes before release. They need game data, so
  they run on the owner's machine or a private runner, never in public CI
  (rule 1).
- **Release engineering.** Version numbers, signed Windows binaries, symbols
  kept for the crash reporter, a changelog generated from the ledger.
- **A commercial-readiness audit.** The tracer's proof that no code of
  Capcom's executes, [`exe-table-audit.md`](exe-table-audit.md) closed, an
  inventory of third-party licences, and the provenance record
  (`LICENSING.md` section 6) in a form that can be handed over.

### Early: three decisions of shape, cheap now
1. **Tag the ledger's entries by kind.** A column or a field, checked by
   `tools/ledger_check.py`. Retrofitting it after a hundred more entries is
   the expensive way.
2. **Note mod layers in the cache design**, before the cache format is fixed.
3. **Choose the portable platform library**, before more platform code is
   written against Win32 in the platform plan's steps.

### First concrete step
The owner's word on the three early decisions. The first needs only a
vocabulary for the kinds.

### Outcome
_(2026-10-04) open; recorded, nothing decided or scheduled._

## I34 — The name entry back: the community's renamer, and naming at New Game

**Ask (2026-10-05, the owner):** take the name screen from the PlayStation
game so it is at least available; the port removed it.
**Kind:** gameplay / localisation
**Feasibility:** HIGH for the community's renamer (the port kept every caller
and both commit paths; only the grid's draw, its legend and its input step are
gone), MEDIUM for New Game (the port left no call to re-arm).
**Gated on:** the localisation rework of phases 4 and 5 - the owner's word,
2026-10-05: it may change the structure of the font files, and the grid's
glyphs, the encoding per language and the font all follow from it.

[`name-entry-restoration.md`](name-entry-restoration.md) is the research: the
PlayStation routine's two copies (COMMU02 and START.EMI), each function's
arguments, state cells and control flow, what survives in `BOF3.exe`, and what
the sibling has. Two pieces, in the owner's order:

- **The community's renamer.** Until it is built the entry ends unanswered
  (DIV-0075, 2026-10-05), where the port's never left its step.
- **Naming at New Game, as an uplift** with the same work: the PlayStation asks
  for the hero's name before the game starts and the port goes straight in
  (DIV-0020's "Original behaviour").

### Outcome
_(2026-10-05) open; researched, deferred to the localisation rework._

## I35 — The music host suite on the owner's machine

**Ask (2026-10-09, the owner):** make `tools/bgm/host` (`spu_tests`,
`seq_tests`, `synth_render`) build and run locally, so the SPU model and the
sequencer can be checked here without a cloud session - after PR #46, not in it.
**Kind:** tooling
**Feasibility:** HIGH. The code is plain C++17 and CMake; what stops it on
Windows is that both test programs check the model's aborts by `fork`ing a
child and reading its exit, and the render check (`synth_check.py`) looks for
the Mednafen renders under `/workspace/scratch`. The step-9 session ran the
suite on a POSIX host only ([`music-seq-engine.md`](music-seq-engine.md)
section 6).
**First step:** replace the `fork` in the abort checks with a re-exec of the
test binary itself with an argument naming the one case to run (`CreateProcess`
on Windows, `posix_spawn` elsewhere, or CTest's `WILL_FAIL`), build with the
`cmake-clang-v1` toolchain's `clang++` under `~/.local/share/retcomm`, and give
`synth_check.py` a `--renders` argument for the owner's render directory.
**Gated on:** nothing.

### Outcome
_(2026-10-10) built on `catchup/music-tooling`:_ the aborts are checked in a
child by `tools/bgm/host/abort_check.h` - `fork` on POSIX, the test binary
re-run with `--abort-case N` on Windows (std::abort's exit status 3, the
runtime's message box and error report off in the child); `spu_tests` and
`seq_tests` pass under the `cmake-clang-v1` `clang++` (431,706 and 28 checks,
`tools/bgm/host/README.md`), and `synth_render` and `synth_check.py
--renders` run against the owner's renders in `analysis/bgm/renders`
([`music-open-ends.md`](music-open-ends.md)). CI builds and runs the two test
programs on Linux (`.github/workflows/checks.yml`).

## I36 — Our own ATRAC3plus decoder for the PSP's sounds

**Ask (2026-10-10):** the owner asked what a decoder for the PSP's sound files
would entail, then filed it as a low-priority idea for later. Today a PSP-only
import decodes them with the player's own ffmpeg ([`psp-only-build.md`](psp-only-build.md)
sections 2.1.1 and 8, `tools/at3.py`).
**Kind:** platform
**Feasibility:** MEDIUM. The format is unpublished; the practical source is
FFmpeg's LGPL decoder, which cannot be vendored ([`LICENSING.md`](LICENSING.md)
section 4, CLAUDE.md rule 5).   **Gated on:** the owner (priority, and the
tables question below).

### What already exists
- The 892 files' format measured: RIFF `WAVE_FORMAT_EXTENSIBLE`, ATRAC3plus,
  44.1 kHz, 744-byte frames (128 kbit/s), stereo except `WHISTLE`, the `fact`
  chunk's encoder delay 2048 (`psp-only-build.md` 2.1.1).
- `recipes/psp.snd.toml`: every clip's onset, so a decoder slots in where
  ffmpeg is called (`at3.decode`).
- An oracle for every sample: the player's ffmpeg on the same 892 files, and
  the PSX disc's XA as a second, independent one.

### What it would take (estimates, not measured)
- **The parts:** frame and channel-unit parsing; Huffman-coded spectra over
  many codebooks with noise filling and power compensation; tonal-component
  synthesis; gain control; per-band inverse MDCT; the 16-band synthesis
  filter bank. About 3-5 k lines of C++, much of it numeric tables.
- **Two stages, clean room:** one agent reads FFmpeg's decoder and writes a
  prose specification with no code (2-3 days); another implements from it
  alone (3-5 days); verification against ffmpeg within 1 LSB on all 892 files,
  unattended (1-2 days). Roughly 1.5-2 weeks.
- **Where:** C++ in `src/audio/` beside the sequencer, so the engine could
  decode at run time and drop the import step for effects. Pure Python would be
  slow and might need numpy on the player's machine.
- **The jingles stay an issue:** the PC plays them as MP3s, so without ffmpeg
  they would need an MP3 encoder (LAME is LGPL too) or an engine change so that
  kind 0 plays PCM.
- **For the owner:** whether the format's tables, numeric facts about Sony's
  format, may be taken from an LGPL source. The patents (ATRAC3plus about
  2002) are probably expired, which is unverified.

### First concrete step
A feature survey of the 892 files: which block types, tonal components and
codebooks they actually use. They are one bitrate and channel layout, so it
may drop whole parts of the decoder and firm up the estimate.

### Outcome
_(2026-10-10) open, low priority (the owner). The player's own ffmpeg stays the
route until this is picked up._

## I37 — Grow the field's tile ring for wide mode as the PSP did

**Ask (2026-10-12):** the owner, on WS's survey: "I'm interested in growing the
ring, let's place it as an idea for later". The PSP widened the field's tile
ring from 28 x 56 to 33 x 58 cells; our wide mode lowers the inset instead
(DIV-0041).
**Kind:** display
**Feasibility:** MEDIUM - 23 sites, about half a week
([`psp-widescreen-placements.md`](psp-widescreen-placements.md) sections 5 and 6.1).

**Measured for 21:9 (2026-10-13, [`ultrawide-culls.md`](ultrawide-culls.md)
section 3):** the field camera (distance 0) needs no ring growth at 120
columns - DIV-0041's inset rule reaches inset 0. Scenes at `Camera_Distance`
above about 2,200 (three areas, effect kind 0x8D) and off-axis yaws over low
ground do (known-defects D292). 32 columns cover every persistent distance our
source writes; the PSP's 33 x 58 covers all but kind 0x8D's first frames.
`Sprite_UpdateScreen`'s layer test `0..0x36` sets the off-screen flag the
logic reads: a grown ring keeps it, or ledgers it. The round's RG group (this
idea, built) waits on the owner, after the far-camera scenes are seen.

### Outcome
_(2026-10-12) open, for later (the owner)._
_(2026-10-15) scheduled: the owner, "build I37 now" - the RG group, without
waiting for a far-camera picture (D292)._
_(2026-10-21) found not built (no PR from #62 on touches it); staged as RG in
[`wide-ring-round.md`](wide-ring-round.md)._
_(2026-10-21) **built** (group RG, [`wide-ring.md`](wide-ring.md)): the PSP's
33 x 58 at 16:9 and 21:9 as Capcom's ring plus a draw-only rim, Capcom's
28 x 56 at 4:3; DIV-0041 amended 2026-10-21; D292 narrowed to kind 0x8D's
first frames. On `wide-ring/round`; the owner's play owed._

## I38 — Draw the two edge sweeps mapped onto the wide picture

**Ask (2026-10-14, from the ultrawide round, [`wide-staging.md`](wide-staging.md)
section 8):** two effects sweep across the picture and end their state at a
screen-x bound: `EffectKind0D` (`0x465F30` / `0x466050`; slides in from x 390
by 30 a frame to 160, holds, slides out to -70, where the record goes idle)
and `Area172_EffectA5Grow` (`0x428240`; a shade's edge grows to 400, then the
script goes on). In the wide picture they start or vanish inside the bands
([`ultrawide-culls.md`](ultrawide-culls.md) rows 14 and 15). Moving their bounds
would change when the record idles or the script goes on - logic.
**Kind:** display
**Feasibility:** HIGH - keep the logic's x and draw it mapped: `[160, 390]`
onto `[160, 390 + c]` and `[-70, 160]` onto `[-70 - c, 160]` (the same for the
shade), so each starts and ends past the edge in the original's frames. Needs
a draw hook in `Sprite_QueueOverlay`'s path. A built version is a DIV-0041
amendment.

### Outcome
_(2026-10-14) open: the owner's call (leave, or the mapping)._
_(2026-10-15) scheduled: the owner chose the mapping (logic's x kept, the draw
mapped; a DIV-0041 amendment when built)._
_(2026-10-21) found not built; staged as SW in
[`wide-ring-round.md`](wide-ring-round.md)._
_(2026-10-21) **built** (group SW, [`wide-sweeps.md`](wide-sweeps.md)): the
logic's x kept, the draw mapped; DIV-0041 amended 2026-10-21; the shade's
right-anchored map and the faster slide accepted off the sheets. On
`wide-ring/round`; the owner's play owed._

## I39 — Shade the element icons by how weak the enemy is

**Ask (2026-10-15, the owner, on the Identify panel's element icons, PR #63):** "There's maybe an argument to be
made for some kind of bright -> dark scaling based on how weak they are, but
for now going with the original brightness settings sounds correct."
`Identify_DrawElements` reads one resistance byte per element (`+0xBF..0xC4`)
and draws shade 0x80 at 1 or less (holy: 3 or less), else 0x30; a scale
would map the byte's range onto the shade instead.
**Kind:** display
**Feasibility:** HIGH - one function (`src/game/magic_s12.cpp`); what the
byte's values mean across enemies wants reading first.

### Outcome
_(2026-10-15) open: the original's two levels kept for now (the owner)._

## I40 — Port the importer to C++ inside the launcher

**Ask (2026-10-15):** the owner, deciding the alpha release: bundle the
official embeddable Python with the importer now, and port the importer to
C++ later, so a release is the launcher and the engine alone.
**Kind:** platform
**Feasibility:** MEDIUM   **Gated on:** the owner (priority); nothing technical

### What already exists
- The contract the launcher calls ([`install-sources.md`](install-sources.md)
  section 4: `identify --json`, `sources`, `setup --json-progress`); a C++
  importer behind the same calls changes nothing on the Sources page.
- Readers already in C++ that the import needs at run time: the DAT
  container walk and the cache manifest (`src/common/cache_manifest.cpp`),
  the sequencer's SEQ / VAB reading (`src/audio/`).
- Every importer output is hashed against a committed recipe or fixture
  (`recipes/*.toml`, `fixtures/*.tsv`), so a port is checked file by file
  against the Python one on the owner's discs, unattended.

### What is missing
- The Python side reached by `setup`: 25 files of `tools/` (measured by
  `tools/make_release.py`, 17,898 lines on 2026-10-15), of which the large parts are
  `loc_build.py` (the language layers), `exe_tables.py` / `exe_twins.py`
  (`base/exe/`), `seq.py` / `ppms.py`, `xa.py` / `at3.py`, `type1.py`, the
  transforms, and numpy's use in `at3.py`, Pillow's in the font work.
- capstone is imported by `exe_tables.py`; whether `build` needs it at all
  is unmeasured.

### First concrete step
Measure which functions of those 25 files a `setup` actually runs (a
coverage trace over the three setups in `install-sources.md` section 6), so
the port's size is counted, not estimated.

### Outcome
_(2026-10-15) open: the alpha bundles Python (`tools/make_release.py`)._

## I41 — Read CHD and CSO images as sources

**Ask (2026-10-15):** from the Sources page work: players commonly keep
PlayStation discs as `.chd` (MAME's compressed hunks) and PSP discs as
`.cso`; today `identify` refuses both with a sentence telling the player to
convert them back ([`install-sources.md`](install-sources.md) section 4).
**Kind:** tooling
**Feasibility:** MEDIUM   **Gated on:** the licensing check below

### What already exists
- `tools/psx_disc.py` reads any sector-addressed image through one
  `sector(lba)` method; a reader that yields raw 2,352-byte sectors (CHD) or
  2,048-byte blocks (CSO) slots in behind it.
- CSO is a block index plus zlib (or LZ4 for ZSO) blocks: a few dozen lines
  with the standard library.

### What is missing
- CHD v5 for CD-ROMs: hunks compressed with zlib, LZMA, FLAC (audio tracks)
  and Huffman, a CD-specific codec wrapper (`cdzl` / `cdlz` / `cdfl`) and the
  subcode / ECC rebuild. Python has zlib and lzma; FLAC would need our own
  decoder or a library - libchdr is BSD-3, readable; whether to vendor it is
  the owner's call ([`LICENSING.md`](LICENSING.md) section 4).
- Measured identity: a CHD's raw sectors must hash file by file to the
  catalogued tree, as a `.bin` does - to be proved on a CHD made from a held
  disc.

### First concrete step
Make a CHD of the US disc with `chdman createcd`, list the codecs its hunks
use, and decide whether the build's needs (the data track's sectors, XA
included) avoid FLAC entirely.

### Outcome
_(2026-10-15) open._

---

## I42 — Make the install faster

**Ask (2026-10-15):** the owner, testing the Sources page by building from
some sources and then adding more: worth looking for ways to speed the
install up, for later.
**Kind:** tooling
**Feasibility:** MEDIUM   **Gated on:** nothing; the first step is a measurement

### What is measured
- A setup takes **171 to 249 s** on this machine
  ([`install-sources.md`](install-sources.md) section 6): the US `.cue` alone
  243 s, the PC folder with the US `.cue` 177 s, the PSP-JP `.iso` 249 s; a
  second run over an existing install is barely faster (227, 171, 238 s).
- **ffmpeg looks like most of it:** the same PSP-JP `.iso` with no ffmpeg on
  the PATH took **22 s** against 249 s with it - the movie and the PSP's
  ATRAC3plus audio skipped. The first place to look.
- **Every add rebuilds the whole install from every listed source**
  (section 5): a new `cache.new` and `game.new`, swapped in at the end. Adding
  a third disc to a two-disc install redoes the first two. A `.zip` source is
  unpacked again each time.
- Identifying a source is not the slow part: 16-21 s for an image's first
  read, 1-3 s once it is in the file cache; a `.zip` by its member's hash in
  under 4 s.
- Not measured: how a setup's time splits between the base build, each
  language layer, the movie's conversion (ffmpeg), the PSP's audio
  (ATRAC3plus by ffmpeg), the copy into `game\` and the swap.

**The owner's read (2026-10-15):** rebuilding only what a new source adds is
the biggest win - an install needs one intro movie, one copy of the
PlayStation's music, one copy of most `DAT` files. The log agrees: one
setup already builds each of those once (a three-source setup,
`psx-us` + `psx-eu-en` + `psp-eu`: `base/exe from psx-us`, a language layer
for each disc, `base/fmv converting LOGO/CAPCOM30.STR from psx-us ... (about
two minutes)`), but **the next add builds all of it again**, though a disc
added after the base changes none of it. So the work is to keep the base -
movie, music, the base's `DAT` files - across setups, and build only the new
source's own layers.

### What could make it faster
- **Measure first.** Time each step of `setup` in `setup.log` (it already
  logs the steps), for a disc-only, a PC + disc and a PSP-only install.
- **Rebuild only what changed** (the owner's pick). The cache is a base and layers
  ([`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 3). A new source that only
  adds a language, or the PSP's extras, needs only its own layer built and
  installed, not the base again - if the cache can say which layers came from
  which sources.
- **Convert once.** ffmpeg's output - the movie, the PSP's audio - kept
  per source (keyed by the source's hash) and reused by every later setup,
  and the conversions run side by side rather than one after another.
- **Keep what does not change.** The unpacked `.zip`, the PC folder's copy
  when the PC source is unchanged.
- **Run independent work at once.** Each language layer is built on its own
  (`tools/loc_build.py`), and ffmpeg's conversions are separate processes.
- Whatever changes, keep the swap: the old install stays working until the
  new one is complete.

### Outcome
_(2026-10-15) steps 1 and "rebuild only what changed" built_
(`install/folder-scan`, [`install-sources.md`](install-sources.md) section 5,
"Timings and reuse"): `setup.log` times every step; a setup reuses the
previous cache's movie, PSX sound effects and language layers whenever their
inputs and the code are unchanged. Adding a fourth disc (France) to a
three-disc install: 89 s where a fresh build of the four is 372 s, and the two installs are
identical file for file. Left: the PSP's
own conversions (its movie and ATRAC3plus audio, for a PSP-only install) are
not reused yet; adding a PSP disc after the PlayStation ones changes one base
chunk, so the language layers are rebuilt then (a finer key - only the base
chunks `loc_build` reads - would keep them); the zips are still unpacked each
time (about a second each).

## I43 — Settle every gap of a disc-built exe image

**Ask (2026-10-16):** the owner, after the forest sprite below: the exe image
a disc-only install builds should not leave Capcom's values out - an open item
to look at every one of them, not wait for play to trip over the next.
**Kind:** tooling / engine
**Feasibility:** HIGH   **Gated on:** nothing; the first step is a tooling change and a regeneration

### What already exists
- A disc-built image of `.data` (`tools/exe_tables.py`,
  [`exe-import-engine.md`](exe-import-engine.md)) classes every byte it cannot
  take from a disc (`recipes/exe-gaps.tsv`, generated by `exe_tables.py gaps`).
  Only the `gap` class is a real difference: the PC has a value there and the
  image leaves zero (`BOF3X_EXEIMAGE_STRICT=1` refuses such an image; the
  default logs each range and plays on). `language`, `widen`, `unread`, `own`
  and `keys` differ from the PC's file by design, and `own` / `keys` equal it
  after the engine's fills.
- For the EU disc on `main` (2026-10-16, before the batch `fix/psx-only-1016`), `gap` is 188 ranges, 2,561 bytes: `game-value` 148 / 1,581,
  `disc-unplaced` 26 / 830, `port-relaid` 9 / 99, `language-zero` 5 / 51. The
  other six disc builds are of the same order.
- Until now the only record was a trap in [`HANDOFF.md`](HANDOFF.md) ("a
  disc-only route that behaves oddly may be one of them"). Two gaps were fixed
  because the attract happened to reach them (the field button map, the
  story-flag seed; DIV-0101).

### The case that opened it (measured 2026-10-16)
The owner, on a PSX-only install: the child's field sprite in the forest after
the mine (area 9, the `whelpBoss` route, frames about 4,530..6,870) is drawn
in pieces and displaced up and to the right. Runs under
`C:\Users\kerok\abv\runs\`, each `whelpBoss`, frame 4710's sprite region
scored against the PC install's (pixels differing by more than 40):

| Run | What | Score |
|---|---|---|
| `wbpsx` | PSX-EU disc alone | 5,190 |
| `wbplpc` | the same, with the PC's eight `PL` arenas and `START` | 5,190 |
| `wbpcexe` | the PSX data with the PC cache's exe image | 0 |
| `wbh1` / `wbh4` / `wbh5` | the PSX image with the PC's bytes in `gap` + `unread` / `gap` / `unread` only | 0 / 0 / 5,190 |
| `bis0`..`bis8` | a bisection over the 282 differing `gap` sub-ranges (eight halvings, then the one range alone) | 0 |

Every `AREA009` chunk is byte-identical between the two caches. The range is
`0x669868..0x669883` (27 bytes, `game-value`, labelled
`Field_MoveSpeeds+0x78` but not part of that six-byte table), alone enough to
fix the frame (score 0). It is the record lists of party members 7 and 9 in
the member-animation table `0x669884` (11 pointers, `{bank, animation, +0x2A}`
records; [`sprite-pose.md`](sprite-pose.md) section 3): the pointer words at
`0x6698A0` and `0x6698A8` point at `0x669868` and `0x669874`. Zero records
give every pose bank 0 and animation 0. Which of the two members the forest's
sprite is was not split.

**The disc has the records.** In the US boot executable the same lists are at
`0x80181C58` (3 records) and `0x80181C68` (4 records), byte for byte the PC's.
The PlayStation gives each of the five members with no records (3, 5, 6, 8, 10)
its own one-word empty list between the others. The PC moved those to `.bss`
and packed the rest, so the two layouts disagree from `0x669868` on, and the
byte-run map (`exe_maps/`) stops there.

**Why `find_places` missed it:** it places an unmapped region only by data or
code pointer words *inside* the region, or by a 16-byte window of at least six
distinct byte values. These 27 bytes are records with no pointer in them, and
their windows hold five distinct values. The pointers that locate them point
*in*, from `0x669884`, which is placed; the disc's own words there
(`0x80181C58`, `0x80181C68`) name the place exactly. The method does not use
incoming pointers.

### What is missing
1. **Placing a region from the pointers into it.** In `find_places`: for each
   placed data-pointer word whose PC target lies in an unplaced region, take
   the build's word at the same place as the target's address in the build,
   and check the region there byte by byte as the other places are (a stretch
   that disagrees nowhere). Two incoming pointers that agree on one
   displacement, or one pointer and 16 equal bytes, as the existing rule.
   Upper bound of what it can reach, measured 2026-10-16 (a PC data pointer
   word aimed into a gap range, from `recipes/exe-pointers.tsv`): EU 23 of 188
   ranges, 789 bytes; US 22, 776; FR 22, 842; DE 23, 851; JP 13, 201; PSP-JP
   and PSP-EU 13, 202. Then `exe_tables.py recipe` and `gaps` again for every
   disc build, and the forest checked on a rebuilt PSX-only cache.
2. **Which of the rest are read.** Run the reference routes on a disc-only
   install with a read watch on the remaining gap ranges (the exe host can
   page-guard them, or a page-granular log as the state hash does). A range
   read in play is a live defect like this one; the rest can wait.
3. **Settle each read range** by one of three answers, never by copying the
   PC's bytes into code (the owner's stance, 2026-10-10: a rule or the
   player's disc): a place on the disc the tools have not found yet (an
   overlay section, another file); an engine rule that computes it
   (`src/hook/exe_own.*`'s pattern, DIV-0101's two); or a reading that the
   value is the port's own and the engine supplies it.
4. **Then `BOF3X_EXEIMAGE_STRICT=1` as the default for disc-only installs**,
   once no gap is left that play reads - so a new one fails loudly at start
   rather than drawing wrong.

### First concrete step
Step 1: the incoming-pointer placement in `find_places`, the seven builds'
recipes regenerated, `exe-gaps.tsv`'s shrink recorded per build, and the
`whelpBoss` forest on the rebuilt PSX-EU cache scored against the PC's
(score 0 expected). A disc-only regression run (the A/B/C attract and the
routes) after it, since every newly placed range changes the image.

### Outcome
_(2026-10-16) step 1 built_ (`tools/exe-gaps-incoming-pointers`, onto
`fix/psx-only-1016`; [`exe-import-engine.md`](exe-import-engine.md) 7.7,
DIV-0101 amended): `find_places` places a region from the placed pointers
into it. The seven builds regenerated: 18 to 54 more places each, no placed
byte other than the PC's; `gap` ranges EU 185 to 177 (2,453 to 2,308 bytes),
US 187 to 181 (2,502 to 2,277), JP 170 to 162, FR 189 to 181, DE 192 to 184,
PSP-JP 158 to 150, PSP-EU 159 to 151 (against the batch tip `da58415f`). The forest: `whelpBoss` frame 4710 on a
PSX-EU disc-only install rebuilt with these tools (root `abg`), the sprite's
region against the PC's, **5,190 differing pixels to 44** (the frames are
written at 3x now; scored at 1x by every third pixel), and the route keeps
step with the PC's to its end (frames 6,870 to 13,170: 192 to 1,034 pixels of
the whole frame, where the unfixed image falls out of step from about 6,500).
Regression: the attract on that install with this image and with a control
(the same image, the gaps this closes zeroed again): 366 frame pairs, 0
differing. An install built before this is refused at start with a message
to rebuild it (`FilledSince()`, the row `0x669868..0x669883`; the general
form is HANDOFF "Pick up here" 11). Steps 2 to 4 open.
_(2026-10-17) steps 2 to 4 staged_ in [`install-gap-round.md`](install-gap-round.md): RW the read watch (step 2), the fix groups by method (step 3), the strict default as the end batch's last item (step 4).
_(2026-10-20) step 2 measured, steps 3 and 4 framed_: the read watch found only the owner's two rows read while zero ([`install-gap-reads.md`](install-gap-reads.md) 9.7), but STRICT refuses every `gap` row, read or not (49..74 per build after stage 0's recount, `recipes/exe-gaps.tsv`), so the default waits on [`exe-gap-round.md`](exe-gap-round.md): every row accounted for, STRICT narrowed to rows not on a reviewed list, then on.

## I44 — The data image as typed source: the next step toward reconstructed source

**Ask (2026-10-17, the owner):** if we want to move closer to a fully
reconstructed source, what is the next code step?
**Kind:** engine / tooling
**Status (2026-10-20):** stages 1 and 2 IN PROGRESS: stages A and B of
[`typed-data-round.md`](typed-data-round.md) merged as PR #94 (50,943 raw
data sites down to 27,394, machine code identical to `main`); stage C
([`typed-data-stage-c.md`](typed-data-stage-c.md)) built on `typed/c`, the
machine code identical again (the typed tables, `GameData`, the strict CI
rule; every raw site left has a reason), its PR after the owner's smoke run.
**Status (2026-10-21):** stages 1 and 2 DONE (PR #94 2026-10-19, stage C
PR #100 2026-10-20); stage 3, the relocatable image, open and not staged.
**Feasibility:** HIGH for stages 1 and 2 (mechanical, no behaviour change,
parallel by file group); MEDIUM for stage 3 (a loader change, proved by the
state hash). **Gated on:** nothing for stage 1; stage 3 on the state hash
made address-independent ([`platform-layers-plan.md`](platform-layers-plan.md) 3).

### Where the line is today
Since state 3 ([`state3-design.md`](state3-design.md), PR #53) no byte of
Capcom's code runs: `bof3x.exe` is ours and the 10,088 functions are ours.
What is still Capcom's is the **data image**: `.data` and `.rdata` from the
player's file, laid at their linked addresses by a fixed-base executable
(`/FIXED`, base `0x400000`) and read by our code through addresses. Measured
2026-10-17 in `src/game` (a `grep` over the sources, counts only):

| What | Count |
|---|---|
| raw accessor lines (`Word(`, `Byte(`, `Address(`, `Dword(`, `Float(`) | 7,600 (4,616 / 1,837 / 882 / 180 / 85) |
| `[[data]]` entries in `symbols.toml`, with a `ctype` | 2,696, 2,613 |
| code pointers in `.data` bound through the entry-stub page | 9,142 words, 6,346 targets ([`exe-import.md`](exe-import.md) 5) |
| run-time raw calls through an address | 61 ([`round-13-cleanup.md`](round-13-cleanup.md) 1.3) |

"Reconstructed source" here means: a program whose source builds the game,
reads the player's data through named, typed structures, and holds no fixed
address. The remaining distance is all on the data side. The engine / data
split ([`ASSET_SOURCES.md`](ASSET_SOURCES.md) 5, [`LICENSING.md`](LICENSING.md) 3)
holds throughout: no table is copied into source; the image stays the
player's file, only *how it is addressed* changes.

### Stage 1 - type the remainder (no behaviour change)
Every raw accessor site becomes a named, typed access generated from
`symbols.toml` (`tools/gen_symbols.py` already emits a typed lvalue, or a
typed pointer to element 0 with `count`, for every entry with a `ctype`).
Each site needs a `[[data]]` entry with evidence (the reader, the width the
instruction used) and a `ctype`. Target: 7,600 lines to 0, and a check in
`checks.yml` refusing a new raw accessor in `src/game`. Proof: the state
hash identical on the eleven routes and the attract, the headless
self-tests. The shape of a takeover round: file groups in worktrees,
`symbols.toml` merged by entry keyed on `pc`.

### Stage 2 - the layout as one generated struct (no behaviour change)
From the same entries, generate a `GameData` struct whose member offsets
are `static_assert`ed against the original addresses (offset = `pc` minus
the image base); the inline-asm sites that name a global by `"m"` keep
working because the bytes do not move. The engine still maps the image at
its base, so the struct is a view over the same bytes and the compile-time
check is the equivalence proof. The gaps between members become named
padding: the first complete census of what in `.data` is still unknown, by
address and size.

### Stage 3 - make the image relocatable
The recipe tools already classify every pointer word in `.data`
(`recipes/exe-pointers.tsv`: code pointers, data pointers, and the
`number` class for words that only look like pointers,
[`exe-import-engine.md`](exe-import-engine.md) 7.2). With that table the
loader can place the image anywhere: data-to-data pointers rebased, code
pointers bound straight to our functions. That retires the entry-stub page,
the 61 run-time raw calls (they become calls through named members) and
`/FIXED`. Proof: the game running with the image at another base, the state
hash address-independent (rebased pointer pages normalised, as
[`state-hash.md`](state-hash.md) 6 already does for the overlay's cells).
The hosted build's clone fuzz, which needs Capcom's layout, is replaced here
by a fuzz over the struct (state3-design 11's "later state").

### What follows, not this entry
After stage 3 the only pin to x86-32 is pointer width inside the records;
widening them at load is the same table applied once more (a native-pointer
`GameData` filled from the 32-bit image). That is the portable 64-bit build,
[`PLAN.md`](PLAN.md) phase 4 and I33's platform library. The language
overlay's cells and the cache's layers are unchanged by all three stages.

### First concrete step
Stage 1's measurement as a script (`tools/raw_access_census.py`: each raw
accessor site, the address it names, whether a `[[data]]` entry covers it,
with what `ctype`), committed with its counts in a doc. It needs no decision
and sizes the round.

### Outcome
_(2026-10-17) open; proposed, nothing built._ _Stages 1 and 2 staged the same day in [`typed-data-round.md`](typed-data-round.md); the measurement there splits stage 1 into globals (about 900 sites) and record fields by offset (about 6,700 sites), so the record types come first._
_(2026-10-21) stages 1 and 2 built: the typed-data round, PRs #94 and #100
(machine code identical to `main` throughout, `tools/codegen_same.py`);
stage 3 open._
_(2026-10-21) I5 folded in by the owner: recovering Capcom's translation-unit
boundaries (from the globals' blocks and the `.text` order) as a source-layout
pass after stage 3 - which of our files should be split or joined to match._

## I45 — Shrink the quicksave files

**Ask (2026-10-17, the owner):** a quicksave ([`quicksave-round.md`](quicksave-round.md))
is about 4 MB raw, the image span plus the participants' records; "ok for
now, but probably worth considering if we can shrink that at some point".
**Kind:** engine   **Feasibility:** HIGH   **Gated on:** the quicksave round
built and its format measured; the owner's word to pick it up.

### What to measure first
- How much of the span is zero pages, and how much is unchanged from the
  image's load-time bytes (`base/exe/data.bin`): both are free to drop - a
  page list and a diff against the install's own image, no third-party code.
- How much of the rest repeats (the VRAM shadow and the DAT arena are the
  likely bulk): a plain run-length or LZ pass in our own code, measured
  against the time a restore may take (one frame's budget is the target).

### The shapes, cheapest first
1. **Zero-page and unchanged-page skip** against the install's image: a page
   bitmap in the header; probably the largest win for the least code.
2. **A diff against the previous quicksave in the same slot**: smaller files,
   but a chain a lost file breaks - only with the base kept whole.
3. **A compressor**: zlib-class code is BSD / zlib-licensed and would not
   touch [`LICENSING.md`](LICENSING.md) 4, but the project has vendored no
   third-party compression so far; our own LZ is a few hundred lines.

### First concrete step
The measurement, on the six routes' quicksaves once the round's stage C has
made them: zero pages, unchanged pages, and an LZ ratio, in a table.

### Outcome
_(2026-10-17) open; logged at the owner's word, nothing measured._

## I46 — A quicksave is native memory: revisit what loading one trusts

**Ask (2026-10-19, the owner):** leave a note to consider this again later.
**Kind:** engine   **Feasibility:** HIGH   **Gated on:** the quicksave round
built; the owner's word to pick it up.

An emulator's save state holds the emulated console's memory, so a doctored
state can at worst misbehave inside the emulator. A quicksave here
([`quicksave-round.md`](quicksave-round.md)) is the native process's own
memory: the image span and the task stacks, with return addresses into
`bof3x.dll`'s real code, written straight back on a quick-load. A doctored
quicksave can therefore run anything, and the DLL's load address (fixed
since 2026-10-19, the owner's call) does not change that. The game is
offline and single-player; the realistic case is a quicksave shared online
and loaded by someone else.

What the round already does: the file records the build and the DLL base
and refuses a mismatch (that catches the everyday case, a quicksave from
another build, not a doctored one). `docs/quicksave.md` says to treat a
quicksave like a program: load your own.

Options when picked up, none chosen:
- A per-install key (made on first run, kept beside the ini) and a MAC over
  the file: the F5 menu and F7 refuse a quicksave this install did not
  write. Cheap; stops shared files, not a local attacker (who has the key).
- Validating the return addresses on the task slices against the known
  resume points in our code before writing them back. Narrower, and does
  not cover function pointers elsewhere in the span.
- Nothing beyond the note, if quicksaves stay a local convenience.

## I47 — Quick-loading across translations: review the text that carries over

**Ask (2026-10-20, the owner):** keep the one-translation-into-another
quick-load working ("the slow replace"), and come back to check it behaves
as wanted.
**Kind:** engine   **Feasibility:** HIGH   **Gated on:** the owner's word to
pick it up.

A quicksave does not record the language, only whether a translation ran
([`quicksave.md`](quicksave.md) 4b). The original Chinese against any
translation is refused before anything is written. One translation's
quicksave loaded under another passes every check: the take's system message
pool (DIV-0007's buffer, in `ST`'s record) and the area text in the span come
back in the take's language, and stay so until the game reloads them from
the disc in the running one. Read from the code on 2026-10-20, not played.

### What to measure first
- A quicksave under en-US, loaded under fr-FR (or de-DE) in one place each:
  the field, a menu, a battle, mid-dialogue. Which text shows in which
  language, and what reloads it (an area change, a menu, a battle's end).
- Whether anything ours outside `ST` is language-shaped: the title menu's
  labels (DIV-0064), the font's widths, the name tables.

### Options when picked up, none chosen
- Leave it: a deliberate way to change language mid-game, with the reload
  rule written down.
- Reload the running language's system pool after a cross-language load
  (record the tag in `ST`, a version bump), so only the area text lags.
- Refuse it like the original-against-translation case.

### Outcome
_(2026-10-20) open; logged at the owner's word, nothing measured._

## I48 — A comment-cleanup round after the typed-data round

**Ask (2026-10-20, the owner):** the typed-data round left a lot of
retire notes and stale comments; worth a cleanup round at some point, not
now.
**Kind:** tooling / source   **Feasibility:** HIGH   **Gated on:** the
typed-data round's stage C merged; the owner's word to pick it up.

Every time stages B and C retired a module's constant for its
`symbols.toml` name, the converter left a note where the constant was, in
the form `// kX: now Name[i].field (symbols.toml).`, so a reader of the old
docs could still find the module's word. Measured on `typed/c` `2a208d56`
(a scan of `src/` for comment lines of that shape): **4,714 retire notes in
338 files**, 389 of them in fuzz files; 92 files carry twenty or more (the
most, `scena_sc7_callees.h`, 67). 14 name a symbol `symbols.toml` no longer
has (renamed since the note was written). Besides them, comments written
for the module's own constants and helpers that the stages removed: 40
comment lines still name an `at::kX` constant, 4 of them a constant
defined nowhere now; the per-module header comments that describe the
retired constants or the old per-file view helpers are not counted.

### What to measure first
- The notes by kind: a constant whose name says what the symbol does not
  (worth keeping as one line in the entry's `note`, or a typed alias as C4
  did for the scratch block), against one that only repeats the symbol.
- Which docs cite a module constant by name, so a note's removal does not
  orphan a doc's search (`grep` of `docs/` for each `kX`).

### Options when picked up, none chosen
- Drop the notes whose constant's name is the symbol's or adds nothing;
  move the rest's word into the entry's `note` in `symbols.toml`; one
  tool, gated by `tools/codegen_same.py` (comments only, so `0 differ` by
  construction) and the census's `--check`.
- Keep the notes, and only fix the stale ones (the 14, the `at::kX`
  comments, the header comments).
- Leave them: they cost reading time, not correctness.

### Outcome
_(2026-10-20) open; logged at the owner's word, nothing changed._

## I49 — What `Battle_AutoTargetCheck`'s `+0xBA` test means

**Status:** open (2026-10-20, logged at the owner's word: "maybe leave a note in ideas to check it again later").

**What.** `Battle_AutoTargetCheck` (`src/game/rest_4a.cpp`) keeps a forced target only while the enemy record's
u16 at `+0xBA` is above 1 (with `+0x92` bit 5 and `+0x114` bit 14 clear). The module called that word `kEnemyHp`;
`records.h` calls `+0xBA` `BattleEnemy::intel`, the stat copy's `+0xA`, which the damage code reads as INT
(`battle_damage` kAttackerInt / kTargetInt) and `BattleEnemy_PickAction` tests for `<= 1`; HP is `+0xA4`. The C4
audit (slice B) found the two names disagree; the owner had the constant renamed `kEnemyIntel` on 2026-10-20.
`magic_steal.md` R22's "speed" at `+0xBA` is a third reading (its code reads `+0xB8`).

**The check.** An INT gate on an auto-target is an odd rule, so confirm the field by measurement: the writers of
`+0xBA` (`Battle_CopyEnemyData`, `EnemyAI_ScaleStat`), the PSX twin's test (psx-twin-check.md), and a live read of
`+0xBA` against a known enemy's stats. If it is not INT, the struct's name and both readers' docs change together.

### Outcome
_(2026-10-20) open; the rename is done, the meaning unconfirmed._

---
## I50 — A second language in the row above the text

**Ask (2026-10-20, the owner, from GitHub issue #97 and a Reddit request):**
beside furigana (I21), let two languages run in step: the message the player
is reading in one language, and the same message from another held language
layer in the smaller row above each text row, re-flowed page by page so
the two scripts stay together.
**Kind:** game behaviour / localisation
**Feasibility:** MEDIUM. **Gated on:** I21's renderer (the reading row in
`MsgBox_Step`) and a measured pairing rule.

### What already exists
- **Messages pair by slot.** Every language overlay is converted onto the
  PC's slot table (`loc_build.py convert_block`; the US disc's blocks have
  the PC's slot counts in 288 of 288, `convert_block`'s docstring,
  2026-10-10), so "the same message in another
  language" is the same area block and slot. Pages and lines inside a
  message do not pair; that re-flow is the work.
- **The reading row** (I21): the ja-JP kana at 12 px are the sheet cell texel
  for texel on a 6-unit quad, so a reading row is exactly half a text row
  (the owner, 2026-10-20). A Latin second language has the same choice of
  1/2 (texel-exact at the donor's 8 x 12) or 2/3.
- **Optional text layers** (DIV-0086) already carry a language in their name
  and are refused under another; a second-language layer is one more:
  `ruby-en-US-ja-JP`.

### What is missing
- The census: for every held pair, how often the second script fits over the
  main page's rows as authored, with the rows re-broken, or only with a page
  split; whether two overlays' glyph tables can be resident together.
- The pairing rule, drafted from the census (the main script keeps its
  breaks; the second is packed in order; a split only when it overflows).
- Names and inserts drawn from the other layer's tables on the reading row.

### First concrete step
The census (the round's group SL), before any build.

### Outcome
**BUILT 2026-10-22** in the reading-row round, ledgered as DIV-0156: the
optional layers `ruby-<second>-<main>`, every ordered pair of held
languages built by `setup` (`tools/ruby_build.py second`,
[`reading-row-second.md`](reading-row-second.md)), the census's pairing rule
as built, a composed font chunk per layer, `0x13` drawing an inserted name at
reading size; chosen in the launcher's *Row above the text*. Owed: the
owner's play (English over Japanese is on the sheets; Japanese over English
is not captured).
