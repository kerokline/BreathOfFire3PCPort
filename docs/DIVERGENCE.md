# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; no entries yet — there is no code)

Every intentional behavioural difference between this project and the original
Chinese PC port gets an entry here.

## Why this file exists on day one

This is a **living** project: divergence is the deliverable, not a defect
([`PLAN.md`](PLAN.md) §6). That freedom has a well-known failure mode. Renovation
projects diverge gradually, nobody records which changes were deliberate, and
some years in nobody can say whether a given oddity is a fix, a regression, or
original behaviour that always looked wrong. At that point the original is the
only oracle left, and re-deriving intent from a decade of commits is miserable.

We are unusually well placed to avoid it. A provably faithful implementation of
this game sits in the next directory — the archival sibling,
[`BreathOfFire3Recomp`](https://github.com/kerokline/BreathOfFire3Recomp) —
which means **every behavioural difference here can be detected and attributed.**
That only pays off if the ledger is kept from the first change, which is why the
file exists before the code does. Retrofitting it means reconstructing intent
from memory, and the memory will be gone.

The governing rule, from [`CLAUDE.md`](../CLAUDE.md) rule 2:

> **Unledgered divergence is a bug.**

Not a style violation — a bug, closed by either writing the entry or reverting
the change.

## What counts

**Ledger it** when the game *does something different*: damage or formula
changes, altered encounter or drop rates, changed menu behaviour, different
text, new or removed content, fixed design bugs, changed timing that is visible
in play.

**Don't ledger** changes that preserve behaviour: refactors, a function
reimplemented to be byte-equivalent, performance work, build system, renderer
changes that produce the same image at the same time, or anything in tooling and
docs.

The boundary case is worth naming, because it will come up constantly:
**a port bug fix is still a divergence.** The fullscreen fallback to 640×480 is
a defect, and fixing it is obviously correct — and it still gets an entry,
because five years from now "why does this not match the original?" deserves an
answer better than "we assumed it was a bug". Cheap to write, and the reason the
ledger stays trustworthy.

## Entry format

```markdown
### <short imperative title>

- **ID:** DIV-0001
- **Date:** YYYY-MM-DD
- **Subsystem:** text / battle / field / menu / platform / audio / render
- **Original behaviour:** what the shipped PC port does, and how that was
  established (the evidence rule applies — cite the measurement).
- **New behaviour:** what this project does instead.
- **Rationale:** why. If it is a fix, say what makes it a bug rather than a
  design choice.
- **Also in the PSX version?** yes / no / unknown — whether the archival sibling
  shows the same behaviour. This is what distinguishes a *port* bug from an
  *original* one, and it changes how confident the fix should be.
- **Reversible?** whether it sits behind a config toggle, and the key if so.
```

Assign IDs sequentially and never reuse them. A superseded entry stays, marked
`SUPERSEDED by DIV-NNNN` — the record of what was once believed is part of the
value.

## Differential testing

Where an algorithm exists in both binaries — damage formulas, encounter tables,
script control-code handling, RNG sequences — it can be run head-to-head against
the archival build. These are the same algorithms compiled for two
architectures ([`PLAN.md`](PLAN.md) §3), so a mismatch is real signal rather
than noise.

Two distinct uses, and they should not be confused:

- **Before a change:** establish what the original actually does, so the
  "Original behaviour" field is measured rather than assumed.
- **After a change:** confirm the *only* differences are the ledgered ones.
  An unexpected mismatch is a regression, found immediately instead of in a bug
  report two years later.

Harness to be built in phase 2/3; this section records the intent so it gets
designed in rather than bolted on.

---

## Entries

*None yet.*
