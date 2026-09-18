# Licensing: keeping the commercial path open

**Status:** STABLE (decided 2026-09-18)

**This is not legal advice.** It is a record of the project's stated goal, the
mechanics as understood, and the constraints that follow — so that a decision
made once is not quietly undone by a convenient shortcut two years from now.
Before any commercial handoff, someone who knows game IP should review the
provenance. Nothing here substitutes for that.

## The goal

> Keep the work open and viewable, and eventually be able to hand a finished
> product to a distributor — GOG, Steam or similar — who would work out IP rights
> with Capcom.

Everything below follows from that one sentence. If the goal changes, this
document is wrong and should be rewritten rather than worked around.

---

## 1. The licence is the second question. The first is who owns the copyright.

Any licence can be changed later — **for code you own**. The moment someone else
contributes under [PolyForm Noncommercial](../LICENSE), their contribution is
licensed *to this project* under PolyForm Noncommercial, and it cannot be
relicensed to a distributor without going back and asking them. One unreachable
contributor from three years hence blocks the handoff permanently, and no amount
of later care fixes it.

So the load-bearing instrument is a **contributor agreement**, not the licence.
Two shapes:

| | What it does | For this goal |
|---|---|---|
| **DCO** (`Signed-off-by:`) | Contributor certifies they had the right to contribute | Necessary, **not sufficient** — grants no relicensing right |
| **CLA** | Contributor grants this project a broad licence including the right to sublicense and relicense | This is the one that preserves the endgame |

**Decision: a CLA, or at minimum an explicit relicensing grant in
`CONTRIBUTING.md`, before the first outside contribution is merged.** Cheap now,
impossible to retrofit — the same argument [`DIVERGENCE.md`](DIVERGENCE.md) makes
about the ledger, and it fails the same way: silently, and only visibly at the
moment it matters most.

This is **not yet done** — see §7.

## 2. The licence: PolyForm Noncommercial, and why

It already does what the goal asks. Open and viewable, forkable for
non-commercial use, and this project remains the only party able to grant
commercial terms. With a contributor agreement behind it, a distributor can be
handed *whatever* licence the deal requires — including a proprietary one —
without renegotiating with anyone.

**Direction of travel is the whole argument.** Restrictive → permissive is a
decision available on any future day. Permissive → restrictive is not: a version
published under MIT or Apache is permissive forever. Starting where we are and
loosening at handoff keeps both doors open; starting permissive closes one
immediately and irreversibly.

### The alternatives, and why not

- **Apache-2.0** — the licence a corporate legal team is most comfortable with,
  and it carries an explicit patent grant. It is the right answer *at handoff*,
  if the deal wants an open licence. It is the wrong answer *now*, because it
  gives away the exclusivity that makes this project worth a conversation.
- **MIT** — same objection, minus the patent grant.
- **GPL / AGPL** — does not forbid commercial distribution, but it is friction
  with launchers and DRM, publishers dislike it, and it would make later
  relicensing impossible once contributors exist. Wrong shape for this goal.

## 3. The engine/data split is a hard constraint

The pattern that has survived commercially is: **ship the engine, require the
player's own game data.** OpenRCT2, DevilutionX and TR1X all work this way
([`prior-art/`](prior-art/)), and it is a large part of why they exist
unmolested. The plausible product here is "original game data, licensed from
Capcom, plus an open engine" — a shape distributors have shipped before.

`CLAUDE.md` rule 1 — *never commit game data* — was written as hygiene. It is
also the structural thing that keeps this path open, and it is now **doing double
duty**. That matters because hygiene rules get relaxed for convenience and
commercial constraints do not. Anyone tempted to check in "just one small
extracted table" should read this paragraph first.

The rule extends past files, as rule 1 already says: a paste into a doc, an
issue, or a commit message is the same problem with a worse audit trail.

## 4. Never vendor copyleft — now a business constraint

The [prior-art survey](prior-art/README.md#4-licences-only-one-thing-here-is-vendorable)
found that of everything studied, **only OpenLoco (MIT) is vendorable**.
OpenRCT2 and TRX are GPL-3.0, Diaphora is AGPL-3.0, devilution/DevilutionX use
the non-OSI Sustainable Use License.

Worth understanding precisely, because the GPL case is stricter than the
`bof3ext` case and the difference is easy to miss: PolyForm Noncommercial's
field-of-use restriction is an "additional restriction" that GPLv3 §7 forbids.
So there is **no notice-retention workaround** of the kind MIT would permit.
It is not "ask first" — it is "cannot".

And the consequence for the goal: **vendoring copyleft would end the commercial
path**, because we would be unable to grant terms we do not hold. `CLAUDE.md`
rule 5's *read it, cite it, vendor nothing* was written as a statement about
intellectual honesty toward a peer project. It is now also the thing standing
between this repo and an unlicensable deliverable.

Reading, citing and reimplementing from a description remain entirely fine —
techniques are not copyrightable, and every note in [`prior-art/`](prior-art/)
was deliberately written to be implementable without reference to the original
source.

## 5. What licensing cannot fix

A licence governs *this project's own contribution*. It cannot grant rights we do
not hold.

This project is a derivative of Capcom's binary in a way a clean-room
reimplementation would not be — we read the disassembly directly, and that is the
entire method ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)). No licence choice changes
that fact, and no contributor agreement makes it go away.

**Capcom's cooperation is not a formality at the end. It is the thing the whole
endgame rests on.** A distributor cannot "work out IP rights" over our
objection-free source if the rights holder declines; they can only do so if the
rights holder agrees.

That is not a reason to stop — projects in this space operate here routinely, and
the ones that fail usually fail for distributing *assets*, not source. But it
does mean the rights conversation should happen **before** anyone is committed to
a delivery date, and that §3 is not negotiable.

## 6. The provenance record is an asset, not just discipline

Worth stating explicitly because it changes how the existing rules should be
valued:

- the evidence rule ([`README.md`](README.md)) — every claim cites the
  measurement that produced it
- evidence tiering in [`symbols.toml`](../symbols.toml) — `evidence` vs
  `hypothesis`, never silently promoted
- [`DIVERGENCE.md`](DIVERGENCE.md) — every intentional behavioural change,
  attributed and dated
- [`SHARED_SOURCE.md`](SHARED_SOURCE.md) §3 — what the *porting house* changed,
  kept separate from what we changed
- `CLAUDE.md` rule 5 — no third-party code, so no licence contamination to
  untangle

Together that is a **complete, auditable account of where every line came from**.
Most projects in this space cannot produce one. It is exactly what makes a rights
discussion tractable rather than terrifying, and it is a reason the discipline
pays beyond engineering.

## 7. Open, and not yet decided

- **No contributor agreement exists yet.** §1 says one is required before the
  first outside contribution is merged. Nothing has been drafted — this needs a
  real CLA text (or a reviewed relicensing clause), not an invented one.
- **No `CONTRIBUTING.md` exists.** It is where the above belongs, alongside the
  evidence rule and the divergence rule, so a first-time contributor meets all
  three at once.
- **Whether the handoff licence is proprietary or Apache-2.0** is a decision for
  the deal, not for now. The point of §2 is that it stays a choice.
- **No legal review has happened.** §5 says one should, before commitment.
