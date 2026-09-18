# Shared source: what the two binaries have in common, and what they don't

**Status:** STABLE (established 2026-09-18 by the two kinship probes)

`BOF3.exe` (Chinese PC port, 2001, x86, MSVC6) and `SLPS-00990` (PlayStation,
1997, MIPS, Psy-Q) are **two compilations of one C source tree**, forked and
modified rather than identical. This document sets out why that is established
rather than assumed, what it does and does not license, and the third category of
difference it creates that neither existing document had a home for.

It is a synthesis of [`kinship-probe-text-engine.md`](kinship-probe-text-engine.md)
and [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md); every
measurement it rests on is cited there.

## 1. Why this is common descent, not convergence

The matches that *make sense* prove nothing. Two independent teams implementing a
dialogue box both end up with a string pointer and a delay counter; two teams
implementing damage variance both end up with a random multiplier near 1.0.
Functional requirements force functional similarity.

What establishes common descent is **shared arbitrary choices** — the same
reasoning textual criticism uses on manuscripts, where shared *errors* rather
than shared correct readings prove a common exemplar:

| Idiosyncrasy | Where |
|---|---|
| One `^= 0x10` toggle reached from **two** different switch arms (`0x10` and `0x11`), so "start" and "end" are the same instruction | `MsgBox_Step`, PSX `0x8015096C` / PC `0x497840` |
| A `0xCD` clamp sitting at one arbitrary point in a multi-term damage formula | `Battle_ScaleDamage`, PSX `0x801DCD18` / PC `0x446430` |
| `MsgBox_Reset` consuming a leading `0x0C` and storing its argument to a field `0x2E` bytes away from everything else the function touches | PSX `0x8015042C` / PC `0x497770` |
| Struct **field order** preserved even where field sizes changed | enemy and party working records |
| Two message pools sitting exactly `0x4000` apart | PSX `0x80010000`/`0x80014000`, PC `0x803580`/`0x807580` |

None of these is something a reimplementation converges on. They are the
fingerprints of particular decisions by particular people, and they survived a
change of compiler, calling convention, and instruction set.

The second leg of the argument is what the PC code *isn't*. It is idiomatic
MSVC6 output — magic-number division (`0x51EB851F` with `sar 5` for `/100`),
`lea` chains for strength reduction, register allocation that reflects an
optimiser rather than a transliterator. It is not lifted, transpiled, or
hand-reimplemented from a disassembly. Somebody compiled C.

## 2. The source's *file structure* survived too

This is the finding that goes beyond "same algorithms", and it was not in
[`PLAN.md`](PLAN.md) §3's original hypothesis.

Globals preserve their layout **within** a block and are reordered **between**
blocks. Four distinct deltas over five blocks in the text engine alone; the
message-box interpreter state keeps twelve fields at one delta, while the window
records sit at a completely different one.

That is the signature of **per-translation-unit static allocation**. Each `.c`
file's statics were emitted as one contiguous run by both compilers; the two
linkers then placed those runs in different orders. The internal layout is
preserved because it was decided by the source, and the ordering differs because
it was decided by the linker.

**Consequence worth designing around:** clustering the PC binary's globals into
blocks, and matching those blocks to PSX blocks, is a route to recovering the
original **source file decomposition** — which `.c` file each function and global
belonged to. For a project whose deliverable is readable, maintainable source
(§2C), starting from something close to Capcom's own file boundaries is
materially better than inventing our own. This should be an explicit phase-1
output, not a side effect.

## 3. Same tree, forked — and the fork is already documented

"Same source" is established. **"Same source unchanged" is already false**, and
the differences found so far are systematic rather than incidental:

| Change | Evidence |
|---|---|
| Enemy working record grew `0x10` — a 16-byte name prepended | stride `0x118` → `0x128`, battle probe §1 |
| Party working record grew `0xC` past `+0x13C` | stride `0x140` → `0x14C` |
| Text field widths grew for Chinese (`name[8]` → `name[16]` in the item/skill tables) | `PC_PORT_CROSS_REFERENCE.md` §3 |
| Damage-variance table widened `u16` → `u32`, values unchanged | PSX `0x801EAF50` / PC `0x64E38C` |
| BIOS `Rand` thunk `0x8017ED4C` → MSVC6 CRT `rand()` `0x5B93D2` | battle probe §4 |
| PSX scratchpad `0x1F800000` relocated to an ordinary global `0x903850` | battle probe §4 |
| `GetText` gained a PC-only index split | [`PLAN.md`](PLAN.md) §4a |
| Platform layer replaced wholesale — DirectDraw, DirectInput, DirectSound, Win32 message pump | §1 import table |

The picture is a porting house taking Capcom's tree, swapping the platform layer,
widening what Chinese text required, and recompiling. Exactly the fork you would
expect, and no part of it undermines §1.

## 4. What this licenses, and what it does not

**It does license** treating the PSX corpus as a first-class input to naming the
PC binary, which is what the two probes measured and what `symbols.toml` now
records. It also means the PC binary is a **second independent observation of the
same source**: where the two agree, we have corroboration of what the source said
that neither binary alone provides.

**It does not license** skipping PC-side verification. A PSX name remains a
*hypothesis* here until something on this side confirms it — `CLAUDE.md` rule 5's
evidence discipline and `docs/README.md`'s tiering are unchanged by this
document. What changes is the cost of verification, not its necessity: because
bodies are recognisable by eye, `evidence` tier is usually one disassembly read
away.

**The honest bound.** Twelve functions across two subsystems, both core game
logic — the parts *most* likely to be shared. The platform layer is certainly not
shared. Menus, field, script and audio are untested. §1's conclusion is well
supported for game logic and known-false for platform code, and nothing here
claims a uniform mapping across the whole image.

## 5. A third category of difference

The project now has to keep three things apart, and only two of them had a home:

| Category | What it is | Where it goes |
|---|---|---|
| **Port divergence** | The porting house changed it, between 1997 and 2001. Not ours, and it is *history*, not a decision. | **Here**, §3 |
| **Our divergence** | We changed it, deliberately, as part of the renovation. | [`DIVERGENCE.md`](DIVERGENCE.md) |
| **Regression** | It changed and nobody meant it to. | A bug |

Conflating the first two would be a slow disaster of exactly the kind
`DIVERGENCE.md` exists to prevent — in five years "why does this differ from the
PlayStation version?" needs to distinguish "Capcom's porting house did that in
2001" from "we did that on purpose" from "we broke it". The first is an
observation to record; only the second is a decision to justify.

So port divergences are catalogued in §3 of this document as they are found, and
`DIVERGENCE.md` stays what it is: the ledger of **our** intentional changes.

### The one that constrains the plan

**RNG sequences will not match between the two binaries.** The PSX calls the BIOS
`A0:2F` `rand()`; the PC calls the MSVC6 CRT `rand()`. Same role, different
generator, different sequence.

[`DIVERGENCE.md`](DIVERGENCE.md)'s *Differential testing* section lists "RNG
sequences" among the algorithms that can be run head-to-head against the archival
build. They cannot. Damage formulas, encounter tables and control-code handling
still can — but only with the random draw **injected rather than generated**, or
the comparison is noise. Worth designing into the harness in phase 2/3 rather
than discovering when the first differential test fails for the wrong reason.
