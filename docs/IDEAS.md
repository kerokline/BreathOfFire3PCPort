# Ideas — intake for unscheduled proposals

**Status:** IN PROGRESS (skeleton, 2026-09-19)

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
| I4 | Stacktrace-driven "who called this" work-queue harvester | tooling | LOW | open |
| I5 | Recover Capcom's `.c` file boundaries from global blocks | tooling | MEDIUM | open |
| I6 | Demo/attract playback as determinism oracle | tooling | MEDIUM | open |
| I7 | Replace MCI/VFW with a bundled video decoder | platform | LOW | open |
| I8 | Replace the DirectDraw / `IDirect3D3` presentation layer | platform | LOW | open |

---

## I1 — PSX ↔ PC save file interchange

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
_open_

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
**Kind:** tooling. **Feasibility:** MEDIUM — unknown is whether the port has an
attract mode at all; [`STATUS.md`](STATUS.md) step 0 answers it. _To fill._

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
