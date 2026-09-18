# docs/

Project-owned notes. Agents and humans both put findings here.

## The documents that exist

| Doc | What it is |
|---|---|
| [`PLAN.md`](PLAN.md) | The scoping document. Target analysis, architecture options and the reasoning that selected one, the phased path, constraints. **Right now this is the entire project.** |
| [`DIVERGENCE.md`](DIVERGENCE.md) | The ledger of intentional behavioural changes. Read before changing game behaviour; append when you do. |
| [`LICENSING.md`](LICENSING.md) | Why the repo is licensed the way it is, and the constraints that follow from wanting a commercial handoff to be possible. Read before vendoring anything or relaxing rule 1. |
| [`SHARED_SOURCE.md`](SHARED_SOURCE.md) | Why the two binaries are compilations of one source tree, what that licenses, and the catalogue of changes the *porting house* made. Read before treating a PSX finding as a PC fact. |
| [`kinship-probe-text-engine.md`](kinship-probe-text-engine.md) | PLAN §8 step 2, the load-bearing experiment. **Passed** 2026-09-18: PSX names transfer onto the PC binary, and global blocks keep their internal layout at a per-block constant delta. |
| [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md) | The successor probe, on a subsystem with no pre-existing landmarks. **Passed** 2026-09-18: the transfer scales, PSX *overlay* functions transfer, and value-sequence search on constant tables is a second anchor that needs no seed. |

Plus [`prior-art/`](prior-art/) — notes on four projects that have already hit
walls we are walking toward (OpenRCT2, devilution/DevilutionX, TR1X, Diaphora).
Start at its [`README`](prior-art/README.md): the cross-cutting findings are
worth more than any single note, and one of them (nobody kept a regression
oracle, all of them paid) bears directly on [`DIVERGENCE.md`](DIVERGENCE.md).

Outside `docs/`, two artifacts came out of that probe:
[`../symbols.toml`](../symbols.toml), the PC-side symbol map (same shape as the
sibling's, plus `psx` and `status` fields), and [`../tools/`](../tools) —
`pe_funcs.py`, `pe_disasm.py`, `pe_xref.py`. Their output goes to `analysis/`,
which is gitignored because it is derived from copyrighted game code.

`STATUS.md` will appear when there is status to track — in-flight work and
blockers, in the style of the sibling repo. Until phase 0 starts, `PLAN.md`
carries that weight.

## Naming

- `SCREAMING_CASE.md` for durable subsystem documents — `TEXT_ENGINE.md`,
  `BATTLE_RAM.md`, `SYMBOLS.md`. These are the ones people come back to.
- `kebab-case.md` for a single investigation, incident, or experiment —
  `matcher-first-results.md`, `fullscreen-fallback-bug.md`. Dated by content,
  not by filename.

## Status header

Every document opens with one:

```
**Status:** DRAFT | IN PROGRESS | STABLE | SUPERSEDED (by <doc>)
```

Add the date it was last verified, not the date it was written. A `STABLE`
document with a two-year-old verification date is telling you something useful.

## The evidence rule

**A claim about the binary cites the measurement that produced it.**

Not "the game dispatches through function-pointer tables" but "recursive descent
from `0x5BA057` reaches 309 functions; 738 indirect calls are reg-based memory
operands (full `.text` sweep, capstone, 2026-09-18)". Include the command or
script where one exists.

This is inherited from the sibling repo and it survives the archival/living
split intact. A living project still needs to know what is *true* — it just gets
to decide what to do about it. The two questions stay separately answerable
(`CLAUDE.md` rule 6).

Where a claim rests on the PC port agreeing with the PSX version, say which one
was actually checked. The sibling's
[`PC_PORT_CROSS_REFERENCE.md`](../../BreathOfFire3Recomp/docs/PC_PORT_CROSS_REFERENCE.md)
is the worked example of doing this well, including two cases where the port was
right and the PSX-side notes were wrong.

## Evidence tiers for names

When recording what a function or field *is*, tier the claim explicitly —
`evidence`, `hypothesis`, or `unnamed`. The sibling repo's `NAME_MAP.md` imposes
this discipline and it is the reason its 30k mapped functions are trustworthy.
A name transferred automatically by the matcher (PLAN §3) is a **hypothesis**
until something confirms it.

## What does not go here

- Game data, or anything extracted from it. Including inline in a document —
  `.gitignore` cannot catch a paste. (`CLAUDE.md` rule 1.)
- Player-facing changelogs. `DIVERGENCE.md` is an engineering record, not release
  notes; they serve different readers and should not be merged.
