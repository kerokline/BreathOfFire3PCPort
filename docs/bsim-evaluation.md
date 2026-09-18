# Evaluating Ghidra BSim as the PLAN §3 matcher

**Status:** STABLE (measured 2026-09-18)

[`PLAN.md`](PLAN.md) §3 proposes building a PSX↔PC function matcher. BSim ships
with Ghidra, fingerprints functions from **P-code** — Ghidra's
architecture-independent IR — and claims to match "across compilers,
architectures, and/or small changes to source code". If it works here, most of
phase 1's largest line item is already written.

We have an answer key nobody else has: the hand-verified pairs in
[`symbols.toml`](../symbols.toml), established by the two kinship probes without
reference to BSim. This is what they say.

## Result

**BSim works cross-ISA, and it is a seed generator rather than a matcher.**

| PC function | PC insns | PSX bytes | rank of true pair | similarity |
|---|---|---|---|---|
| `MsgBox_Reset` | 31 | 220 | **1** | **1.000** |
| `Window_Alloc` | 19 | 104 | **1** | 0.720 |
| `Msg_SystemPtr` | 12 | 52 | **1** | 0.584 |
| `MsgBox_FrameTask` | 18 | 144 | **1** | 0.418 |
| `Msg_OpenSystem` | 11 | 76 | 3 | 0.267 |
| `Msg_OpenScript` | 13 | 96 | 8 | 0.423 |
| `MsgBox_Step` | 789 | 1488 | miss | — |
| `Window_Task` | 613 | 112 | miss | — |
| `Rand` *(negative control)* | 16 | 12 | miss *(correct)* | — |

**4 of 8 true pairs rank #1; 6 of 8 are in the top 8.** Candidate pool is 749
PSX functions.

`MsgBox_Reset` scoring **1.000 — a perfect match — between a 1997 MIPS
build and a 2001 x86 build** is the single most striking number in this
document. It is also an independent confirmation of a pair we derived by hand.

## The negative control passed, which is what makes the rest meaningful

`Rand` was included deliberately because we *know* it is not a real pair: the
PSX calls a BIOS `A0:2F` thunk, the PC calls the MSVC6 CRT `rand()`
([`SHARED_SOURCE.md`](SHARED_SOURCE.md) §3). Same role, different
implementation.

BSim did **not** rank its PSX counterpart — it returned 16 other candidates
instead. So the four rank-1 hits are not an artefact of a small candidate pool
or of a scoring function that flatters any input. Without this control the
result would be much weaker than it looks.

## Two failure classes, and they are not the same failure

**Tiny wrappers rank but do not win.** `Msg_OpenScript` (13 instructions) and
`Msg_OpenSystem` (11) are both "load an argument, call one thing, store three
globals, return". There is not enough dataflow in them to be distinctive. The
true pair was found — ranks 3 and 8 — but beaten by unrelated functions with the
same shape. Note `Msg_OpenScript`'s true match scored **0.423 while a false match
scored 0.477**: the ordering is actively wrong, not merely uncertain.

This is precisely the lesson [`prior-art/diaphora.md`](prior-art/diaphora.md)
records — every heuristic Diaphora demoted is a good signal applied without a
size or complexity floor. BSim has the same property, and the floor has to come
from us.

**Large functions miss, and one misses confidently.** `MsgBox_Step` (789
instructions, the 23-arm control-code stepper) returned exactly **one** candidate
— `0x80150598`, which is `MsgBox_Render`. Wrong function, right subsystem,
adjacent address.

The dangerous part is that this single wrong answer carried **significance 44.73,
the highest score in the entire run.** Significance measures how much information
a match carries, not whether it is correct. A pipeline that auto-accepts
high-significance matches would have written `MsgBox_Render` into `symbols.toml`
with more confidence than any correct pair in the table.

**`Window_Task`'s miss is arguably BSim being right.** We tiered that pair
`hypothesis`, not `evidence`, precisely because PC `0x595450` is 613 instructions
against a 112-byte PSX function — almost certainly a driver with `Window_Task`
inlined into it. BSim independently declining to match them is evidence *for* our
hedge. This is the most useful kind of disagreement: two methods disagreeing at
exactly the point our own tiering already flagged as uncertain.

## What this changes for phase 1

**BSim is a work-queue generator, not an oracle.** "Here are ten candidates, one
is probably right" beats "here are 749" by enough to change the economics of
phase 1, and for 6 of 8 pairs a human reviewing the top ten would have found the
answer. That is a large saving over writing a matcher from scratch.

**It does not replace delta propagation; the two are complementary and their
failure modes are opposite.**

| | BSim | Global block deltas ([kinship probes](kinship-probe-text-engine.md)) |
|---|---|---|
| Needs | nothing | a confirmed seed pair |
| Gives | ranked candidates, anywhere in the image | every field in a block at once, then the functions touching them |
| Strong on | mid-sized functions with distinctive dataflow | anything touching a known data block, regardless of size |
| Weak on | tiny wrappers, very large functions, inlined code | subsystems with no seed yet |

`Msg_OpenScript` and `Msg_OpenSystem` — BSim's two worst true-pair ranks — are
exactly the functions delta propagation nails instantly, because they write the
`MsgBoxState` block. And BSim needs no seed, which is what gets us into a
subsystem cold.

**So the phase-1 design is: BSim proposes, deltas dispose.** Use BSim to generate
seed candidates in an unexplored subsystem, confirm one by hand, derive the block
delta, propagate to name the rest, and use BSim's ranking as an independent check
on the propagated names. Neither is trusted alone.

**Every BSim-derived name is `hypothesis` tier at best.** The `MsgBox_Step` case
is the argument: a confident, high-significance, wrong answer. Nothing here
justifies promoting a name without a PC-side read
([`SHARED_SOURCE.md`](SHARED_SOURCE.md) §4).

## Method

Deliberately a **separate Ghidra project** (`BoF3PC`) from the archival sibling's
`BoF3`: that project's `ghidra_run.py merge` reads every program in it and writes
names into *their* `symbols.toml`, so a PC binary living there would quietly feed
PC addresses into the archival name corpus. Cost is one re-analysis of the PSX
boot EXE.

```
python tools/ghidra_pc.py import      # BOF3.exe (PE loader, 1971 s) + SLPS_009.90
python tools/bsim_probe.py build      # H2 database, medium_nosize, PSX sigs
python tools/bsim_probe.py query      # our 9 PC functions -> analysis/bsim_query.json
python tools/bsim_probe.py score      # rank of the true counterpart per pair
```

- PSX import: raw binary, `MIPS:LE:32:default`, base `0x80093800`, PS-X EXE
  `0x800` header stripped — the sibling's recipe.
- Database config **`medium_nosize`**, the size-insensitive template. The port's
  functions are not the same length as the PSX ones, so a size-sensitive config
  would be measuring the wrong thing.
- Query thresholds floored at 0. The question is where the true counterpart
  *ranks*, not whether it clears a bar. BSim still filters server-side — returned
  candidate counts ranged from 1 to 50.

**Controlling for import quality.** A bad score could mean a badly analysed PSX
program rather than a weak matcher. Before trusting anything,
`tools/ghidra/check_funcs.py` confirmed all nine ground-truth PSX addresses
resolve to **exact function entries** with sane sizes. They do. Our fresh import
finds 749 functions where the sibling's seeded project has 1,029, so the
candidate pool is smaller than it would be at full coverage — which makes these
ranks a mild *over*-estimate of BSim's performance, not an under-estimate.

## Open

- **`MsgBox_Step` returning a single candidate** is unexplained. Server-side
  filtering varies a lot (1 to 50 candidates across nine queries) and the
  interaction between `medium_nosize`, self-significance bounds and very large
  functions was not investigated. Worth understanding before relying on BSim for
  the big battle functions.
- **The overlay corpus is still untested, and indexing it is now known to be
  risky rather than simply pending.** Everything here is boot-EXE against
  `.text`. Importing all 406 overlays would grow the candidate pool roughly 17x
  (749 to an estimated 12–13k) and, if the boot EXE is seeded into each overlay
  program as the sibling's tooling does, would commit hundreds of thousands of
  near-duplicate signatures — flattening BSim's rarity scoring and **degrading
  the ranks measured above, silently**. Any overlay indexing must be done on a
  database copy with these nine pairs re-scored before and after. See
  [`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md).
- **Size floors are not yet chosen.** The data says tiny wrappers are unreliable;
  it does not say where the cutoff is. Nine pairs is too few to fit one.
- Whether BSim's **callgraph** option (the database was built with it enabled by
  default) is contributing anything, and whether `--nocallgraph` changes these
  ranks.
