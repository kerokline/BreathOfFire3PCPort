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

Phase 0 is done: a launcher injects our DLL into the player's `BOF3.exe`, twenty-four
functions are ours (`LoadDatFile`; the whole eight-function file layer, with DIV-0003 in
`File_OpenWrite`; `Save_WriteFile` with DIV-0002; `Gfx_BeginFrame` with DIV-0004, a
crash fix; and thirteen on the image path, faithful — `Gfx_LoadImage`, `Font_SetGlyphData`,
`Gfx_InvalidateTextures`, `Gfx_ConvertRow`, `Gfx_LoadImageIfChanged`, `Gfx_ClutPixels`,
`Gfx_FlushDirtyStrip`, `Gfx_FlushUploadQueue`, `Gfx_UploadPacked5`, `Gfx_UploadLzss`,
`Gfx_ClearImage`, `Gfx_MoveImage`, `Gfx_MoveCells`), the A/B switch works, a call tracer and a crash reporter run in-process, and a
deterministic attract-mode regression check passes original-vs-ours. See
[`STATUS.md`](STATUS.md) — do not expand this paragraph into a second copy.

## Pick up here

The single next action, concrete enough to start without asking anyone.

1. **Owner, in game: [`USER_CHECKS.md`](USER_CHECKS.md).** The first check is
   done — both converted PlayStation saves load, play and re-save
   ([`STATUS.md`](STATUS.md)). That file says which of the rest are still owed:
   a save and load through the now fully-ours file layer, DIV-0003's failing
   case, DIV-0002's clean A/B. Since the saves loaded clean, the next step of
   I1 is reading the PC block builder `0x5806F0` and the options bytes at
   block `+0x78`.
2. **Replace what the attract sequence reaches** — stage 1 of the owner's
   order of work ([`STATUS.md`](STATUS.md)). The image path is ours from the
   rendered-frame flushes down to the texture-cache invalidation
   ([`asset-loading-path.md`](asset-loading-path.md) §2), `ClearImage` and
   `MoveImage` included. Next: where cache entries are *built* —
   `0x5A3CC0` / `0x5A5160` for the texture cache, and the readers of the
   palette generations `0x5A2BC0` / `0x5A3160` — which is where bytes +2..+0xF
   of a texture-cache entry get their meaning. The shadow check
   ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2) suits any of these whose jumps stay
   inside; relative calls can be re-aimed.
   **Owner, optional:** a play session with
   `BOF3X_SHADOW=Gfx_InvalidateTextures` set would check the invalidation
   against the original on every call outside the attract sequence — look for
   `MISMATCH` in `build/bof3x.log`.

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

3. Finish reading the asset path: the `SND\`/`BGM\` loaders `0x587910` /
   `0x587A20`, the drive-root probe `0x5A72C0`'s caller `0x4FCB50`, the 32 callers of
   `LoadDatFile`, and the value-sequence search for the dropped PSX sections
   ([`asset-loading-path.md`](asset-loading-path.md) §4).
4. **Grow the attract oracle** ([`attract-mode.md`](attract-mode.md) §6-7). It
   works today as an external sampler: the port is deterministic from launch,
   to the frame and to the `Rand` call, and original-vs-ours compares
   identical. An in-process first-call tracer now exists
   ([`call-trace.md`](call-trace.md)): 540 of 2,936 functions reached, exact
   frame counter, it does not perturb the run, and two launches give the same
   540 calls in the same order (one audio-timed call moves by a frame, §4).
   A per-frame hash of every logic call is identical across two launches
   (§6), and original-vs-ours passes it with all ten functions ours (§7).
   A negative control fails it as it should. `mem_dump.py` now waits for the
   upload queues to drain before it snapshots (§6). Next: write the first
   receipt. What else the data is good for: [`IDEAS.md`](IDEAS.md) I10. **Run it before merging anything that
   touches `src/`.**
5. A CI job that at least *compiles* `src/` (needs no game data), and the
   receipt format ([`STATUS.md`](STATUS.md) open decisions).
6. [`known-defects.md`](known-defects.md) exists (2026-09-19). D1, clipped stat
   numerals on the equipment screen, wants its A/B run and the draw path read;
   D3, fullscreen fallback, is still unreproduced.

[`IDEAS.md`](IDEAS.md) **I1, save interchange**, is under way: format solved,
`tools/save_convert.py` converts both ways, and both converted saves work in
game; PC→PSX is still static only.

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
- Saves: `python tools/save_convert.py list CARD.mcr` / `info` / `psx2pc` /
  `pc2psx` — usage in the file's docstring. In Git Bash pass Windows-style
  paths (`cygpath -m`): a `/c/...` path inside a `CARD:SLOT` argument is not
  translated.
- **Byte-level check of a loader:** start `python tools/attract_run.py --out
  analysis/attract/tmp.tsv --minutes 2.4` (add `--original NAME` for the
  reference), and within a few seconds `python tools/mem_dump.py --label X`;
  then `python tools/mem_dump.py --compare A B`. Always take two reference
  runs — the pair is the noise floor.
- **Shadow check:** `BOF3X_SHADOW=Gfx_InvalidateTextures`, `=gfx_clut`, `=gfx_flush`, `=gfx_unpack`, `=gfx_vram_ops` or `=*` before the launcher
  or `attract_run.py`; `shadow` lines in `build/bof3x.log` — a start-up
  self-test line, then a running tally every 256 calls
  ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2).
- **After a crash:** `CRASH` lines in `build/bof3x.log`, then
  `python tools/crash_report.py` ([`crash-reporter.md`](crash-reporter.md)).
- **Call trace:** [`call-trace.md`](call-trace.md) §8.
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
- `attract_run.py` re-takes the foreground for the whole run, so **anything the
  owner types goes into the game** and one keypress ends the attract sequence.
  An oracle or hash run needs the keyboard and mouse left alone entirely, not
  just the game window (lost a run to this 2026-09-19).
- A rebuild fails at link with "Permission denied" while a game launched
  through the launcher is running — it holds `bof3x.dll` open. Close the game.
- `pe_xref.py` answers "who touches this *data* address". It does not index
  calls: "(no references)" for a function means nothing. For callers use
  `callees` in `analysis/pc_funcs.json`, or scan `.text` for E8/E9 rel32. Cost
  one wrong "no caller" claim, caught the same day.
- One disagreeing frame from `attract_diff.py`, at a state change, is a torn
  sample until a re-run says otherwise; the frame hash is the arbiter
  ([`attract-mode.md`](attract-mode.md) §6).
- `--original "*"` inside an unquoted `$(...)` or an `echo` is glob-expanded:
  `attract_run.py` dies on the file names, and the `cp` after it then saves the
  PREVIOUS run's `build/bof3x.callframes.tsv` as this run's. Check the
  `inject:` line of `build/bof3x.log` says what the run was meant to be.
- Pairing EMI sections to DAT chunks by order or by address mis-pairs 47
  files; use `dat_census.align` ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).

## In flight / uncommitted

_Branches, open PRs, half-finished experiments, files in `analysis/` worth
keeping. "Nothing" is a valid entry._

Phase 0 merged to `main` as PR #3 (`cee66ad`, 2026-09-19). Branch
`phase-3/gfx-loadimage` is cut from it for the `Gfx_LoadImage` /
`Font_SetGlyphData` takeovers: the `mem_dump.py` drain wait, those two in
`src/game/gfx_image.cpp`, `Gfx_InvalidateTextures` in
`src/game/gfx_texcache.cpp` with the shadow check, then the palette cache's
three in `src/game/gfx_clut.cpp`, the two flushes in
`src/game/gfx_flush.cpp`, the unpackers in `src/game/gfx_unpack.cpp`, then
`ClearImage` / `MoveImage` in `src/game/gfx_vram_ops.cpp`. Each passed its byte-level or shadow check, a
negative control, and the attract oracle. Not pushed; PR planned for the end
of the 2026-09-19 session.

Owed in game, all listed in [`USER_CHECKS.md`](USER_CHECKS.md): the converted
saves, the file layer's write/seek, DIV-0003's failing case, DIV-0002's A/B.

Local only, gitignored, worth keeping: `bof3/BOF3.CFG` (windowed mode); three
saves `bof3/BISLPS00/01/0F.DAT` — the owner's own PC saves — plus `02` (JP)
and `03` (US), converted from the sibling's cards by `save_convert.py`; and `analysis/attract/` — `orig_a.tsv` is a
valid all-original reference recording for `attract_diff.py`, and
`ours_d_fileopen.log` is the file-open log behind
[`attract-mode.md`](attract-mode.md) §7.

## Waiting on someone else

- TheRealBiggs — not yet contacted ([`STATUS.md`](STATUS.md) obligations).
