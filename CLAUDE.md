# BreathOfFire3PCPort — session bootstrap

A **living-game** renovation of the 2001 Chinese PC port of *Breath of Fire III*
(`BOF3.exe`, x86-32, 2001-04-18): fix its bugs, modernise its platform, and
reach source that can be changed and extended.

**Read at session start:**

1. This file.
2. [`docs/PLAN.md`](docs/PLAN.md) — the whole project right now. Target
   analysis, the three candidate architectures and why C won, the phased path.
3. [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md) — the ledger. Read before changing
   any game behaviour.
4. [`docs/README.md`](docs/README.md) — where notes you produce go.

**Status: scoping. There is no code yet.** The plan is contingent on an
experiment that has not been run (PLAN §8 step 2). Do not start phase 0 work on
the assumption that phase 1 will succeed.

## The one distinction that governs everything

There are two Breath of Fire III projects in this account and they have
**deliberately opposite invariants**:

| | `BreathOfFire3Recomp` (sibling) | **this repo** |
|---|---|---|
| Nature | **Archival** — preserve the PlayStation release as it was | **Living** — change and extend the game |
| Correctness is | Matching original hardware | Matching *intent*, deliberately chosen |
| Divergence is | A defect to be root-caused | The deliverable |

Do not import the archival invariant here, and do not export this repo's
liberties there. Concretely: a change that would be a bug in the sibling repo
may be exactly right here — **provided it is in the ledger.**

## Hard rules

1. **Never commit game data.** `BOF3.exe`, `DAT/`, `BGM/`, `SND/`, disc images,
   the AVIs, and anything extracted or dumped out of them. `.gitignore` covers
   the files; it does not cover a paste into a doc, an issue, or a commit
   message.
2. **Unledgered divergence is a bug.** Every intentional behavioural change gets
   a [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md) entry: what the original did,
   what this does, why. This is cheap now and very expensive to retrofit — it is
   what keeps "we changed this" and "we broke this" distinguishable years from
   now.
3. **Addresses are load-bearing constants.** A number like `0x497740` is a
   function in `BOF3.exe`. Never refactor, reformat, or "clean up" one without
   knowing what it points at. Inherited from `bof3ext`'s rule 3 and it gets more
   dangerous as this codebase grows.
4. **No stubs.** A reimplemented function is fully implemented or it aborts
   loudly. `return 0;`, `// TODO`, `// for now` in a function that has replaced
   original code is a silent behavioural fork that will cost weeks to find.
5. **`bof3ext` is a peer project, not a base.** The foundation of this project
   is Capcom's binary; `bof3ext` is an independent derivative of that same
   binary with a different goal, and this project is another. Read it freely and
   cite it — its findings are *facts about Capcom's binary* that we rediscover
   and, per the evidence rule, verify independently anyway. Do not vendor its
   code, do not inherit its architecture, and do not describe this project as
   built on it. (It also ships no `LICENSE`; its author licenses his other work
   MIT, so that reads as an oversight — but the plan vendors nothing, so it
   gates nothing.) See PLAN §4 and §7.
6. **Keep "what it was" and "what it should be" separately answerable.** The
   PSX disc decides what the original behaviour *was*. This project decides what
   the game *should do*. The second answer must never quietly overwrite the
   record of the first.

## Related checkouts on this machine

| Path | What it is |
|---|---|
| `../BreathOfFire3Recomp` | The archival sibling. Source of ~30k mapped functions, `names/*.toml`, `symbols.toml`, and `docs/PC_PORT_CROSS_REFERENCE.md` — read that one early. |
| `../bof3ext` | TheRealBiggs' hook layer — a **peer** derivative of the same Capcom binary. `src/bof3/*.ixx` cross-checks our symbol table; `docs/` is unusually good. Reference only (rule 5). |
| `../bof3ext_resources` | 360 files of translated text, fonts, HD textures. Reference only (rule 5). |

## Environment notes (this machine)

- **Use `python`, not `python3`** — `python3` does not resolve here; `python` is
  Anaconda 3.13.9. `capstone` is installed.
- Both a PowerShell tool and a Git Bash tool are available; `.sh` scripts want
  bash. `gh` is at `C:\Program Files\GitHub CLI`, authenticated as `kerokline`.
- The MSYS2 toolchain at `/c/msys64/mingw64/bin` is **not on PATH** — prepend it
  for builds, but note it shadows `python` with an interpreter missing our
  packages. Run Python tooling *before* prepending, or call Anaconda by absolute
  path. (Learned the hard way in the sibling repo.)

## Conventions for agents

- Findings go in `docs/` — see [`docs/README.md`](docs/README.md) for naming,
  status headers, and the evidence rule.
- Project status lives in `docs/PLAN.md` (and `docs/STATUS.md` once there is
  status to track). This file changes only when the *rules or shape* of the repo
  change.
- **Evidence over assertion.** A claim about the binary cites the measurement
  that produced it. This is inherited from the sibling and survives the
  archival/living split intact — the living project still needs to know what is
  true, it just gets to decide what to do about it.
