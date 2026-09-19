# Handoff — next session

**Status:** IN PROGRESS (2026-09-19)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

Phase 0 is done: a launcher injects our DLL into the player's `BOF3.exe`, three
functions are ours (`File_Read`, `File_Open`; `Save_WriteFile` with DIV-0002),
the A/B switch works, and a deterministic attract-mode regression check passes
original-vs-ours. See [`STATUS.md`](STATUS.md) — do not expand this paragraph
into a second copy.

## Pick up here

The single next action, concrete enough to start without asking anyone.

1. **Finish taking over the file layer**, `0x5A7370`..`0x5A75F0`
   ([`asset-loading-path.md`](asset-loading-path.md) §1). `File_Open` and
   `File_Read` are ours; left are `File_CdRoot`, `File_OpenWrite` `0x5A7420`,
   `File_Write`, `File_Size`, `File_Seek`, `File_Close` — all read, all small,
   all with signatures in `symbols.toml`. `File_Seek`'s CRT callee `0x5B9F9E`
   is still identified by argument shape only; read it first. The payoff comes
   when the *whole* layer is ours: only then can `File_Slots` hold something
   other than the exe's CRT `FILE*`. Procedure: [`SCAFFOLDING.md`](SCAFFOLDING.md)
   §3. Faithful replacements need no ledger entry; fixing `File_OpenWrite`'s
   missing null check (a crash on a read-only install) would — it is the
   obvious DIV-0003. **Verify with the attract check** (below) — it exercises
   open/read/size/close on every scene load, and not write or seek, which need
   a manual save and load.
2. **Then `LoadDatFile` `0x454590` itself** — it is fully read for kinds 0 and
   1, and blocked on `0x587CD0` (kind 2), `0x5A6800` (kind 3) and `0x59EA70`
   (image upload) only for their *signatures*, which can be bound as originals.
   Its output is checkable in bytes: dump the arena at `0x803580` after a load
   with ours and with `BOF3X_ORIGINAL=LoadDatFile` and diff. Mind the 16 KB
   coroutine stack ([`SCAFFOLDING.md`](SCAFFOLDING.md) §3, "Hazard").

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

3. Finish reading the asset path: the `SND\`/`BGM\` loaders `0x587910` /
   `0x587A20`, who sets the drive root `0x66BC2C`, the 32 callers of
   `LoadDatFile`, and the value-sequence search for the dropped PSX sections
   ([`asset-loading-path.md`](asset-loading-path.md) §4).
4. **Grow the attract oracle** ([`attract-mode.md`](attract-mode.md) §6-7). It
   works today as an external sampler: the port is deterministic from launch,
   to the frame and to the `Rand` call, and original-vs-ours compares
   identical. Next: log from inside the DLL (exact frame counter, richer state
   hash), and write the first receipt. **Run it before merging anything that
   touches `src/`.**
5. A CI job that at least *compiles* `src/` (needs no game data), and the
   receipt format ([`STATUS.md`](STATUS.md) open decisions).
6. A written note of the known defects (fullscreen fallback, resolution; the
   window title is GBK bytes passed to `CreateWindowExA`, so it is mojibake on
   a non-Chinese system locale — seen in the owner's screenshots 2026-09-19).

The game runs, so [`IDEAS.md`](IDEAS.md) **I1, save interchange**, is available
whenever a visible win is wanted — and `0x454870`, the file layer's only
writer, is probably where it starts.

## How to run things

_Commands a fresh session needs, verified on the date above._

- **Build:** `cmake --preset i686 && cmake --build build` — llvm-mingw's
  `i686-w64-mingw32-clang++` is already on `PATH` on this machine (the
  `retcomm` toolchain under `~/.local/share`), **not** the MSYS2 one.
- **Run:** `build/bof3x-launcher.exe --game bof3`; log in `build/bof3x.log`.
  Original behaviour for one function or all: `BOF3X_ORIGINAL=File_Read` / `=*`.
  **Windowed:** put a two-line `BOF3.CFG` (`0`, then `1`) in the game
  directory, or press F8 in game ([`windowed-mode.md`](windowed-mode.md)) —
  recommended for agent sessions, since it avoids the display mode-set.
  Without it the game mode-sets to exclusive fullscreen for the FMVs; from an agent
  session, end it with `taskkill //F //IM BOF3.exe`.
- **Regression check (10 min, hands off the game window):**
  `python tools/attract_run.py --out analysis/attract/ref.tsv --original "*"`,
  the same without `--original` to `new.tsv`, then
  `python tools/attract_diff.py ref.tsv new.tsv` — exit 0 means identical
  `Rand` count, message and area at every frame
  ([`attract-mode.md`](attract-mode.md) §6).
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
- A Python `'''` string or a bash heredoc silently eats backslashes — two
  generated files came out wrong this way. Write source files with the editor
  tools, and keep backslashes out of `symbols.toml` evidence strings (TOML
  basic strings treat them as escapes).
- The game freezes whenever its window is not the foreground window, then
  replays the missed time unrendered ([`windowed-mode.md`](windowed-mode.md)).
  Any unattended observation must foreground it first; `attract_run.py` does.
- A rebuild fails at link with "Permission denied" while a game launched
  through the launcher is running — it holds `bof3x.dll` open. Close the game.
- Pairing EMI sections to DAT chunks by order or by address mis-pairs 47
  files; use `dat_census.align` ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).

## In flight / uncommitted

_Branches, open PRs, half-finished experiments, files in `analysis/` worth
keeping. "Nothing" is a valid entry._

Branch `phase-0/scaffolding-and-asset-loading-path`, **all of it uncommitted**
at the end of the 2026-09-19 session (the owner had not yet asked for commits).
A natural four-way split:

1. Scaffolding — `src/hook/`, `src/launcher/`, `src/game/file_io.*`, `cmake/`,
   `CMakeLists.txt`, `CMakePresets.json`, `tools/gen_symbols.py`,
   `docs/SCAFFOLDING.md`, the file-layer and CRT symbols.
2. Exe reading — `docs/asset-loading-path.md`, `docs/windowed-mode.md`,
   `IDEAS` I9, the config symbols, the `DAT_CONTAINER` / `PLAN` touch-ups.
3. The save fix — `src/game/save_io.*`, `docs/save-files.md`, DIV-0002, the
   save symbols.
4. The attract oracle — `tools/attract_watch.py`, `attract_run.py`,
   `attract_diff.py`, `attract_opens.py`, `docs/attract-mode.md`, the task-system
   and `Game_AreaNumber` symbols, `IDEAS` I6.

Owed on DIV-0002: reproduce the vanishing save under `BOF3X_ORIGINAL=*`.

Local only, gitignored, worth keeping: `bof3/BOF3.CFG` (windowed mode); three
saves `bof3/BISLPS00/01/0F.DAT` — the first PC saves we have, the raw material
for [`IDEAS.md`](IDEAS.md) I1; and `analysis/attract/` — `orig_a.tsv` is a
valid all-original reference recording for `attract_diff.py`, and
`ours_d_fileopen.log` is the file-open log behind
[`attract-mode.md`](attract-mode.md) §7.

## Waiting on someone else

- TheRealBiggs — not yet contacted ([`STATUS.md`](STATUS.md) obligations).
