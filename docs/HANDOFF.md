# Handoff — next session

**Status:** IN PROGRESS (2026-09-20)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

Phase 0 is done and stage 1 of the owner's order of work
([`STATUS.md`](STATUS.md)) is under way: a launcher injects our DLL into the
player's `BOF3.exe` and **thirty functions are ours** - `LoadDatFile`, the
eight-function file layer (DIV-0003), `Save_WriteFile` (DIV-0002),
`Gfx_BeginFrame` (DIV-0004, a crash fix), fourteen faithful ones that make
up the image path from the rendered-frame flushes down to the texture-cache
invalidation (`src/game/gfx_*.cpp`), and the first five *logic* functions off
the takeover queue, all around the sprite draw-order pass
([`sprite-draw-order.md`](sprite-draw-order.md)). The A/B switch, a call tracer, a crash
reporter and a shadow check against clones of the originals run in-process;
the attract oracle, three memory-dump regions and the frame hash all pass
original-vs-ours **with twenty-five; the newest five have passed only their
start-up fuzz**. See [`STATUS.md`](STATUS.md) - do not expand this paragraph
into a second copy.

## Pick up here

The single next action, concrete enough to start without asking anyone.

1. **Give the five newest takeovers their live run**, about fifteen minutes
   hands off: `attract_run.py` all ours to `analysis/attract/X.tsv`, then
   `attract_diff.py orig_a.tsv X.tsv`, and `mem_dump.py --compare clutref_a X`.
   Then **re-record the frame-hash reference** `analysis/calltrace/ab3_orig/`
   (all five are in `entries_logic.txt`, and owned functions are left unarmed,
   so every frame's hash changed) and run the ours side against it.
2. **Keep working the queue** - `analysis/calltrace/queue.csv`, regenerated
   2026-09-20 (`python tools/calltrace.py queue
   analysis/calltrace/all_a/bof3x.callcounts.tsv` - the argument is the
   *counts* file). Next by call count: `0x57C0A0` - **read, deliberately left,
   ask the PSX side first** ([`sprite-draw-order.md`](sprite-draw-order.md)
   §5) - then `0x56F910`, `0x589660`, `0x589470`, `0x531BD0`, `0x534E50`.
   `0x454810` is `return 1` with 152 callers; it wants a caller read before a
   name. The pass `0x593060` itself is the prize in this corner: it looks
   checkable as memory, the linked draw list it builds, which is
   [`IDEAS.md`](IDEAS.md) I14 level 1.
3. **The Direct3D end of the image path** is where it was: the lock wrapper
   `0x5A3CC0`, the entry builders `0x5A0080` / `0x5A0510` (ten COM calls each),
   the glyph-texture lookup `0x5A2BC0` (128 entries of 0x14 bytes at
   `0x7C9F50`, keyed by glyph and CLUT, same generation trick), and the 3.7 KB
   set-up `0x5A5160` ([`asset-loading-path.md`](asset-loading-path.md) §2).
   **Decide first how such a function gets checked** - its product is a
   surface, not memory; reading a locked surface back is the obvious candidate
   ([`IDEAS.md`](IDEAS.md) I14).
4. **Owner, in game: [`USER_CHECKS.md`](USER_CHECKS.md).** The converted saves
   are done bar one item. Still owed: a save and load through the fully-ours
   file layer, DIV-0003's failing case, DIV-0002's clean A/B. New and optional:
   play with `BOF3X_SHADOW=Gfx_InvalidateTextures` set and look for `MISMATCH`
   in `build/bof3x.log`; and anywhere the game scrolls or copies VRAM, or
   shows a compressed picture, is the only live test there is of
   `Gfx_MoveImage`, `Gfx_MoveCells` and `Gfx_UploadLzss` - the attract
   sequence reaches none of them.

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

5. **[`IDEAS.md`](IDEAS.md) I12 - let the game run unfocused.** Asked for by
   the owner because every check tonight took the PC away for minutes. The
   mechanism is read (app-active byte `0x6BC63B`); it is a divergence when
   built. Worth doing early: it makes everything in item 1 cheaper.
6. **Prepare stage 2, the text swap.** The attract sequence's text boxes run
   the in-game dialogue engine ([`attract-mode.md`](attract-mode.md) §6), so
   there is already a regression check. Unmeasured: which of `MsgBox_Step`'s
   23 control codes those eight messages use (tracer detail mode), and
   nothing covers `Msg_OpenSystem`. What the swap *is* is the owner's to say.
7. **[`IDEAS.md`](IDEAS.md) I13 - save states**, the oracle for what the
   attract sequence cannot reach (menus, system text, combat - stage 3). First
   experiment is written there.
8. The first **receipt**, and a CI job that at least compiles `src/`
   ([`STATUS.md`](STATUS.md) open decisions). The evidence a receipt would
   record now exists in three forms: `attract_diff.py`, `mem_dump.py
   --compare`, `calltrace.py frames`.
9. Finish reading the asset path: the `SND\`/`BGM\` loaders `0x587910` /
   `0x587A20`, the drive-root probe `0x5A72C0`'s caller `0x4FCB50`, the 32
   callers of `LoadDatFile`, the value-sequence search for the dropped PSX
   sections ([`asset-loading-path.md`](asset-loading-path.md) §4). For I1,
   save interchange: the PC block builder `0x5806F0` and the options bytes at
   block `+0x78`; PC-to-PSX is still static only.
10. [`known-defects.md`](known-defects.md): D1, clipped stat numerals, wants its
   A/B run and the draw path read; D3, fullscreen fallback, is unreproduced;
   the frame deadline kept in a 32-bit float is a small, player-visible fix.

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
- **Shadow check:** `BOF3X_SHADOW=Gfx_InvalidateTextures`, `=Gfx_TexCacheFind`, `=gfx_clut`, `=gfx_flush`, `=gfx_unpack`, `=gfx_vram_ops`, `=sprite_order`, `=draw_pool`, `=prim` (comma-separated lists work) or `=*` before the launcher
  or `attract_run.py`; `shadow` lines in `build/bof3x.log` — a start-up
  self-test line, then a running tally every 256 calls
  ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2).
- **Takeover recipe** (each of the eighteen so far): read the function to its
  last instruction, quirks included; `symbols.toml` entry with the evidence
  and `impl`; implement, keeping every unchecked edge and saying so in the
  comment; if every jump stays inside it, clone it and fuzz ours against the
  clone at start-up under `BOF3X_SHADOW`, then break ours on purpose and see
  the fuzz refuse to run; live, all ours: `mem_dump.py --compare clutref_a X`,
  `attract_diff.py orig_a.tsv X.tsv`, and the frame hash before a merge; say
  in the doc what none of that reached; one commit per file of functions.
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
- `mem_dump.py`'s `clut` region and `attract_diff.py` are both unreliable
  under `BOF3X_CALLTRACE_MODE=all` - the first depends on run speed, the second
  miscounts frames at half speed - and both "fail" with every function
  Capcom's. Dumps and the oracle at full speed, the frame hash under the
  tracer, never mixed ([`asset-loading-path.md`](asset-loading-path.md) §2).
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

Branch `phase-3/attract-takeovers`, cut from `main` at `c63636b` (PR #4
merged). Committed locally, **not pushed, no PR**: the five takeovers in
`src/game/sprite_order.cpp`, `draw_pool.cpp` and `prim.cpp`, and
`docs/sprite-draw-order.md`.

Local only, gitignored, worth keeping:

- `bof3/BOF3.CFG` (windowed mode); the owner's PC saves `bof3/BISLPS00/01/0F.DAT`
  and the two converted ones, `02` (JP) and `03` (US).
- `analysis/attract/orig_a.tsv` - the all-original reference for
  `attract_diff.py`; `ours_d_fileopen.log`, behind
  [`attract-mode.md`](attract-mode.md) §7.
- `analysis/memdump/clutref_a_*` and `clutref_b_*` - the all-original reference
  pair for `mem_dump.py --compare`, all three regions (`drain_a` / `drain_b`
  are the same without `clut`).
- `analysis/calltrace/ab3_orig/` - the all-original frame-hash reference,
  recorded with twenty-four owned and still identical with twenty-five, **stale
  with thirty** (item 1): owned
  functions are left unarmed, so it survives a takeover only when the function
  was not in `entries_logic.txt` to begin with (`Gfx_TexCacheFind` is
  render-timed and was not). Taking over a *logic* function changes every
  frame's hash; re-record then, about five minutes.
- `analysis/memdump/slowref_*` - an all-original dump taken under the tracer,
  the evidence that the `clut` region depends on run speed.

## Waiting on someone else

- TheRealBiggs — not yet contacted ([`STATUS.md`](STATUS.md) obligations).
