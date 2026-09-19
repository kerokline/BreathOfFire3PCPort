# Ideas — intake for unscheduled proposals

**Status:** IN PROGRESS (2026-09-19; 9 entries, I6 built)

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
| I1 | PSX ↔ PC save file interchange | tooling | MEDIUM | open — first pick once the game runs |
| I2 | Selectable localisations from original discs | game behaviour | LOW | open |
| I3 | Crude x86→C lifter as portability accelerator | engine | LOW | open |
| I4 | Stacktrace-driven "who called this" work-queue harvester | tooling | LOW | **first-call tracer built 2026-09-19** — [`call-trace.md`](call-trace.md) |
| I5 | Recover Capcom's `.c` file boundaries from global blocks | tooling | MEDIUM | open |
| I6 | Demo/attract playback as determinism oracle | tooling | MEDIUM | **built 2026-09-19** (external sampler) — [`attract-mode.md`](attract-mode.md) §6 |
| I7 | Replace MCI/VFW with a bundled video decoder | platform | LOW | open |
| I8 | Replace the DirectDraw / `IDirect3D3` presentation layer | platform | LOW | open |
| I9 | Integer-scale the picture and extend the view into the remainder | game behaviour | LOW | open |
| I10 | Uses for the call-trace data (seven, ranked) | tooling | MIXED | item 1 **passed 2026-09-19** — [`call-trace.md`](call-trace.md) §7 |
| I11 | In-process crash reporter | tooling | HIGH | **built 2026-09-19** — [`crash-reporter.md`](crash-reporter.md) |

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
([`USER_CHECKS.md`](USER_CHECKS.md)).

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
_open — tool exists, in-game load pending._

## I2 — Selectable localisations from original discs

**Ask (2026-09-18):** JP/EN/DE/FR/ZH selectable at runtime, reading assets from
player-supplied discs. **Kind:** game behaviour. **Feasibility:** LOW.
**Gated on:** phase 5.

Written up in [`STATUS.md`](STATUS.md) "A stated goal worth recording now" —
not restated here. _Remaining sections to fill._

## I3 — Crude x86→C lifter as portability accelerator

**Ask (2026-09-18):** [`PLAN.md`](PLAN.md) §2 recommendation and phase 4.
**Kind:** engine. **Feasibility:** LOW. **Gated on:** phase 3 platform layer.
_To fill._

## I4 — Stacktrace-driven work-queue harvester

**Ask (2026-09-18):** generalise the caller-finding technique
([`PLAN.md`](PLAN.md) §4, phase 2). **Kind:** tooling. **Feasibility:** LOW.
**Gated on:** phase 0 (needs code in-process). _To fill._

## I5 — Recover source file boundaries from global blocks

**Ask (2026-09-18):** cluster PC globals into per-translation-unit blocks
([`SHARED_SOURCE.md`](SHARED_SOURCE.md); [`PLAN.md`](PLAN.md) §3).
**Kind:** tooling. **Feasibility:** MEDIUM — unknown is whether block edges are
detectable without PSX-side anchors. _To fill._

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
_(open)_

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
_(open)_

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
  view shows the void past the edge, so the camera clamp must change per map,
  or the extension must be capped to what the map has.
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
_(open)_

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
