# BreathOfFire3PCPort — session bootstrap

A **living-game** renovation of the 2001 Chinese PC port of *Breath of Fire III*
(`BOF3.exe`, x86-32, 2001-04-18): fix its bugs, modernise its platform, and
reach source that can be changed and extended.

**Read at session start:**

1. This file.
2. [`docs/STATUS.md`](docs/STATUS.md) — what is true today, and
   [`docs/HANDOFF.md`](docs/HANDOFF.md) — what to pick up next.
3. [`docs/PLAN.md`](docs/PLAN.md) — what we intend and why. Target analysis, the
   three candidate architectures and why C won, the phased path.
4. [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md) — the ledger. Read before changing
   any game behaviour.
5. [`docs/README.md`](docs/README.md) — where notes you produce go.

Status is deliberately **not** restated here — it went stale within a day the
one time it was. `STATUS.md` is authoritative.

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
   message. This rule is also load-bearing for the licensing path — the
   engine/data split is what keeps a commercial handoff possible
   ([`docs/LICENSING.md`](docs/LICENSING.md) §3), so it is not relaxable for
   convenience.
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

   **Vendoring copyleft would end the commercial path**, since we cannot grant
   terms we do not hold — and for GPL specifically there is no workaround, not
   merely a permission to seek ([`docs/LICENSING.md`](docs/LICENSING.md) §4).
   Reading, citing and reimplementing from a description stay fine.
6. **Keep "what it was" and "what it should be" separately answerable.** The
   PSX disc decides what the original behaviour *was*. This project decides what
   the game *should do*. The second answer must never quietly overwrite the
   record of the first.

## Related checkouts on this machine

| Path | What it is |
|---|---|
| `../BreathOfFire3Recomp` | The archival sibling. Source of the PSX name corpus (~677 names, counted in `docs/overlay-transfer-feasibility.md`), `names/*.toml`, `symbols.toml`, and `docs/PC_PORT_CROSS_REFERENCE.md` — read that one early. |
| `../bof3ext` | TheRealBiggs' hook layer — a **peer** derivative of the same Capcom binary. `src/bof3/*.ixx` cross-checks our symbol table; `docs/` is unusually good. Reference only (rule 5). |
| `../bof3ext_resources` | 360 files of translated text, fonts, HD textures. Reference only (rule 5). |

## Environment notes (this machine)

- **Use `python`, not `python3`** — `python3` does not resolve here; `python` is
  Anaconda 3.13.9. `capstone` is installed.
- Both a PowerShell tool and a Git Bash tool are available; `.sh` scripts want
  bash. `gh` is at `C:\Program Files\GitHub CLI`, authenticated as `kerokline`.
- **The build uses llvm-mingw, not MSYS2**: `cmake --preset i686 && cmake
  --build build`, with `i686-w64-mingw32-clang++` already on `PATH` (the
  `retcomm` toolchain under `~/.local/share`) — [`docs/SCAFFOLDING.md`](docs/SCAFFOLDING.md) §4.
  The MSYS2 toolchain at `/c/msys64/mingw64/bin` is the sibling repo's; if you
  ever prepend it, it shadows `python` with an interpreter missing our
  packages.

- Ghidra is at `D:\Utilities\ghidra_12.1.3_PUBLIC`, projects in
  `D:\Utilities\GhidraProjects` (project `BoF3PC`, kept separate from the
  sibling's `BoF3`). Override with `PC_GHIDRA_DIR` / `PC_GHIDRA_PROJECT_DIR`.

## Conventions for agents

- Findings go in `docs/` — see [`docs/README.md`](docs/README.md) for naming,
  status headers, and the evidence rule.
- Project status lives in `docs/STATUS.md`; next actions in `docs/HANDOFF.md`;
  unscheduled proposals in `docs/IDEAS.md`. This file changes only when the
  *rules or shape* of the repo change.
- **Evidence over assertion.** A claim about the binary cites the measurement
  that produced it. This is inherited from the sibling and survives the
  archival/living split intact — the living project still needs to know what is
  true, it just gets to decide what to do about it.
