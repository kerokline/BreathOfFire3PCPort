# Handoff — next session

**Status:** IN PROGRESS (skeleton, 2026-09-19)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

The name-transfer experiment passed, the game launches and plays here, no game
code yet; phase 0 is unblocked. See
[`STATUS.md`](STATUS.md) — do not expand this paragraph into a second copy.

## Pick up here

The single next action, concrete enough to start without asking anyone.

1. **Read the exe: the asset-loading path.** Start from what the DAT work gave
   us as anchors and work outward, recording names in `symbols.toml` (tiered)
   and findings in a `kebab-case.md` note:
   - `LoadDatFile` `0x454590` and its callers — what a kind-0 `tag` indexes
     (one arena or several), how kind-1 reaches the PSX-packet renderer, who
     decompresses nothing because the port pre-decompressed type 1.
   - The filename table at file offset `0x24FFE4` and the `SND\%s.DAT` /
     `BGM\%03d.DAT` / `BGM\%03dN.DAT` strings (`0x266F9C`–`0x266FB8`): their
     xrefs name the file-open layer and settle the `SND/NNN_KK` and `BGM/`
     numbering questions ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §5).
   - Value-sequence search for the dropped non-code PSX sections (8 at
     `0x80117000`, 19 at `0x801F2C00`; [`DAT_CONTAINER.md`](DAT_CONTAINER.md)
     §2) — embedded in the exe, or lost.

   Why this subsystem first: it is self-contained, its inputs and outputs are
   files we can now parse and diff, and it is a good **first replacement
   candidate** for phase 0 — a function whose correctness is checkable by
   comparing bytes, not by watching the screen.

   What this step does and does not unlock: reading the exe tells us *what* to
   replace and with what signature. The ability to replace call by call comes
   from the phase 0 loader + detour layer ([`PLAN.md`](PLAN.md) §5), which needs
   only one well-understood function to pass its exit test — so the two can
   overlap; phase 0 does not wait for the whole exe to be read.

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

2. **Phase 0 scaffolding**, with a function from step 1 as the exit-test target.
3. Finish the launch baseline: launch itself passed 2026-09-19 (FMVs, start
   screen, first area). Still owed: the attract/demo-mode check and a written
   note of known defects ([`STATUS.md`](STATUS.md) step 0).

The game runs, so [`IDEAS.md`](IDEAS.md) **I1, save interchange**, is available
whenever a visible win is wanted.

## How to run things

_Commands a fresh session needs, verified on the date above. Empty until there
is something to build._

- DAT containers: `python tools/dat.py survey ../bof3ext/bof3/DAT` (expect
  742 clean); `list` / `extract --out analysis/dat/<name>` / `compare <DAT> <EMI>`
- Fixtures check: `python tools/verify_fixtures.py`
- Ghidra: `python tools/ghidra_pc.py import` (paths in `CLAUDE.md`)

## Traps already paid for

_One line each, with a pointer. Add when something costs more than an hour._

- BSim produced one high-confidence wrong match; no BSim name exceeds
  `hypothesis` without a PC-side read ([`bsim-evaluation.md`](bsim-evaluation.md)).
- Indexing PSX overlays into the BSim database degrades published ranks unless
  done on a copy ([`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md)).
- Figures repeated across docs are not evidence — count them (the 29,036 case,
  same doc).
- Pairing EMI sections to DAT chunks by order or by address mis-pairs 47
  files; use `dat_census.align` ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).

## In flight / uncommitted

_Branches, open PRs, half-finished experiments, files in `analysis/` worth
keeping. "Nothing" is a valid entry._

Nothing.

## Waiting on someone else

- TheRealBiggs — not yet contacted ([`STATUS.md`](STATUS.md) obligations).
