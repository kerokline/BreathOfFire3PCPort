# docs/

Project-owned notes. Agents and humans both put findings here.

## The documents that exist

| Doc | What it is |
|---|---|
| [`PLAN.md`](PLAN.md) | The scoping document. Target analysis, architecture options and the reasoning that selected one, the phased path, constraints. **Right now this is the entire project.** |
| [`DIVERGENCE.md`](DIVERGENCE.md) | The ledger of intentional behavioural changes. Read before changing game behaviour; append when you do. |

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
