# docs/

Project-owned notes. Agents and humans both put findings here.

## The documents that exist

| Doc | What it is |
|---|---|
| [`PLAN.md`](PLAN.md) | The scoping document. Target analysis, architecture options and the reasoning that selected one, the phased path, constraints. |
| [`STATUS.md`](STATUS.md) | Where the project is right now: order of work, open decisions, outstanding obligations. Start here. |
| [`HANDOFF.md`](HANDOFF.md) | What the next session picks up, how, and the traps already paid for. Rewritten, not appended to. |
| [`IDEAS.md`](IDEAS.md) | Intake for unscheduled proposals, each with a feasibility rating and a first step. |
| [`DIVERGENCE.md`](DIVERGENCE.md) | The ledger of intentional behavioural changes. Read before changing game behaviour; append when you do. |
| [`LICENSING.md`](LICENSING.md) | Why the repo is licensed the way it is, and the constraints that follow from wanting a commercial handoff to be possible. Read before vendoring anything or relaxing rule 1. |
| [`SHARED_SOURCE.md`](SHARED_SOURCE.md) | Why the two binaries are compilations of one source tree, what that licenses, and the catalogue of changes the *porting house* made. Read before treating a PSX finding as a PC fact. |
| [`DAT_CONTAINER.md`](DAT_CONTAINER.md) | The `DAT/*.DAT` format: a chunk stream mirroring the PSX `.EMI` section list, unencrypted. Audio re-encoded to WAV, overlay code dropped, most other sections byte-identical to the JP disc. |
| [`media-stack-survey.md`](media-stack-survey.md) | What the port uses to draw, play and decode, and which of it still exists on Windows 11. DirectDraw + `IDirect3D3`, DirectSound 1, DirectInput 3, MCI/VFW for FMV, a statically-linked MP3 decoder. One thing is actually broken: the Indeo 5 logo video. |
| [`replacing-mci.md`](replacing-mci.md) | What replacing the MCI/VFW FMV path with a bundled decoder would take. Anatomy of `Fmv_Play` `0x59E360` — one function, two call sites, modal and blocking — plus the codec licensing table. Costed, not scheduled (`IDEAS.md` I7). |
| [`SCAFFOLDING.md`](SCAFFOLDING.md) | Phase 0: the launcher, the five-byte detour, the `BOF3X_ORIGINAL` A/B switch, and how `symbols.toml` generates the one binding of every name. **Read before taking over a function** — §3 is the procedure. |
| [`asset-loading-path.md`](asset-loading-path.md) | The 16-slot file layer and `LoadDatFile`'s four chunk kinds, read from the exe. Kind-0 chunks land in one arena at `0x803580 + tag` — the port's repacked image of PSX RAM. In progress. |
| [`launcher-settings.md`](launcher-settings.md) | The launcher's settings dialog: why a bare Win32 dialog resource and no toolkit, the three places a setting can go (our environment variables, the game's own `BOF3.CFG`, nowhere yet), and the measurement that shows the disc's `START.EXE` is an autorun shell and `SETUP.EXE` is InstallShield 5 - neither is a configurator. |
| [`windowed-mode.md`](windowed-mode.md) | The port has a built-in windowed mode: a two-line `BOF3.CFG`, or F8 at runtime. Not a patch. Also skips the FMV exclusive mode-set. |
| [`save-interchange.md`](save-interchange.md) | The PC save is the PSX `0x10B0` game block from offset 0 — same checksum, same offsets — with the character name widened 5→9 bytes. `tools/save_convert.py` converts both ways; statically verified, not yet loaded in game. |
| [`crash-reporter.md`](crash-reporter.md) | Always-on fault logging in the injected DLL (`CRASH n:` lines in `bof3x.log`, a minidump beside it) and `tools/crash_report.py` to read dumps, WER's included. Observes, never handles. |
| [`known-defects.md`](known-defects.md) | What the 2001 port does wrong on a current machine, as observed: who saw it, in which configuration, what is established about the cause. Observations, not decisions to fix. |
| [`call-trace.md`](call-trace.md) | The in-process call tracer: which functions a run reaches, counts and edges, a per-frame call hash identical across launches and passing original-vs-ours, the frame-skip finding, and the takeover work queue. |
| [`sprite-draw-order.md`](sprite-draw-order.md) | The field's sprite structures, found by working the takeover queue: **30 + 4 sprite objects of `0xA4` bytes at `0x7DEE80` / `0x802000`**, the 40-entry draw list and the pass `0x593060` that orders it over 0x37 layers, the 56 x 28 view-cell grid, the draw-item index pool, the animation script format. Twenty-two functions ours, the pass `Sprite_DrawPass` among them - fuzzed with recording stand-ins for the callees that are not - all live-checked. In progress. |
| [`psx-library-layer.md`](psx-library-layer.md) | The porting house wrote Sony's libraries rather than rewrite the game's calls into them: libgpu setters and ordering-table links, `getTPage` / `getClut`, a sine, and a GTE whose registers are globals at `0x7DE4xx` with a 20-deep matrix stack. 62 functions ours - `ApplyMatrix` at 2.6 M calls, and the x87 ones in `double` once the control word was measured at `0x027F` on 11 M live calls; the transforms over them; a `NormalColor` that discards its lighting. Also where `-fno-strict-aliasing` was found necessary, and why the matrix product waits on the owner. In progress. |
| [`dialogue-localisation.md`](dialogue-localisation.md) | Stage 2, the text swap, as built: per-language overlay `DAT`s made locally from the player's disc by `tools/loc_build.py` - dialogue, system pools, item and ability names, the US font doubled into the port's glyph table - and the engine side, DIV-0005..0009. Measured: the text code is the glyph index; English is monospaced at 8 px; the three pens that advance text; why the system pool moved; `name[16]` against the US disc's 12. §6 is the open list. In progress. |
| [`USER_CHECKS.md`](USER_CHECKS.md) | Checks only the owner can do, in game. Agents add; the owner ticks and reports. |
| [`save-files.md`](save-files.md) | Saves are `BISLPS0?.DAT`, 4,784 bytes, sixteen slots; the four save-path functions; and an open defect — a fresh save vanishes from the reopened menu although the file is on disk. In progress. |
| [`config-screen.md`](config-screen.md) | The in-game Config screen's text, which is in `BOF3.exe` and in no `DAT`: six label operands, seventeen 16-byte option records, six controller names, and the two row tables that are byte-identical to the PlayStation's. Translated from the player's disc as DIV-0015, with the 8 px UI font `0x516E70` read, the donor's 8 x 8 set imported for it (DIV-0016) and the selected row in the dialogue font (DIV-0017). Confirmed in game. |
| [`menu-screens.md`](menu-screens.md) | The field menu: mode 3, the state machine at `0x589970` and its state block `0x929F00`, mapped from the owner's walk under read-only sampling; and the four defects of the 2001 menu the owner reported, with the cause of two. In progress. |
| [`title-menu.md`](title-menu.md) | The start screen's three rows are artwork, not text: a 4bpp page in `START.DAT` drawn by `0x5888D0` with its row widths as immediates. How `BOF3X_LANG=en` rebuilds it from the player's disc - NEW GAME and LOAD GAME as the disc has them, CONFIG cut from their letters (DIV-0014). In progress. |
| [`attract-mode.md`](attract-mode.md) | The attract sequence exists on PC and runs the real engine. Also: the game's **four-coroutine task system** (hand-written `esp` swap, 16 KB stacks), and why `Rand` is deterministic — seed 1, no `srand` in the binary, no clock reachable from game logic. Groundwork for a regression oracle. In progress. |
| [`kinship-probe-text-engine.md`](kinship-probe-text-engine.md) | PLAN §8 step 2, the load-bearing experiment. **Passed** 2026-09-18: PSX names transfer onto the PC binary, and global blocks keep their internal layout at a per-block constant delta. |
| [`bsim-evaluation.md`](bsim-evaluation.md) | Ghidra BSim measured against our own hand-verified pairs. Works cross-ISA (4 of 8 at rank 1, one perfect match), but it is a seed generator, not an oracle — and it produced one confident wrong answer. |
| [`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md) | What the PSX overlay name corpus is actually worth: 121 named overlay functions, not 29,036, and why indexing them into BSim must be done on a copy. |
| [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md) | The successor probe, on a subsystem with no pre-existing landmarks. **Passed** 2026-09-18: the transfer scales, PSX *overlay* functions transfer, and value-sequence search on constant tables is a second anchor that needs no seed. |

Plus [`prior-art/`](prior-art/) — notes on four projects that have already hit
walls we are walking toward (OpenRCT2, devilution/DevilutionX, TR1X, Diaphora).
Start at its [`README`](prior-art/README.md): the cross-cutting findings are
worth more than any single note, and one of them (nobody kept a regression
oracle, all of them paid) bears directly on [`DIVERGENCE.md`](DIVERGENCE.md).

Outside `docs/`, two artifacts came out of that probe:
[`../symbols.toml`](../symbols.toml), the PC-side symbol map (same shape as the
sibling's, plus `psx` and `status` fields), and [`../tools/`](../tools) —
`pe_funcs.py`, `pe_disasm.py`, `pe_xref.py` — and, since phase 0, `gen_symbols.py`,
which turns `symbols.toml` into the C++ header the code in [`../src/`](../src)
builds against. Their output goes to `analysis/`,
which is gitignored because it is derived from copyrighted game code.

Four documents, four questions — keep them from bleeding into each other:
`PLAN.md` says what we intend and why; `STATUS.md` says what is true today;
`HANDOFF.md` says what to do next; `IDEAS.md` holds what nobody has committed
to. **When `PLAN.md` and `STATUS.md` disagree, `STATUS.md` is right and
`PLAN.md` needs updating.**

## Naming

- `SCREAMING_CASE.md` for durable subsystem documents — `TEXT_ENGINE.md`,
  `BATTLE_RAM.md`, `SYMBOLS.md`. These are the ones people come back to.
- `kebab-case.md` for a single investigation, incident, or experiment —
  `matcher-first-results.md`, `fullscreen-fallback-bug.md`. Dated by content,
  not by filename.

## Status header

Every document opens with one:

```
**Status:** DRAFT | IN PROGRESS | STABLE | SUPERSEDED (by <doc>)
```

Add the date it was last verified, not the date it was written. A `STABLE`
document with a two-year-old verification date is telling you something useful.

## The evidence rule

**A claim about the binary cites the measurement that produced it.**

Not "the game dispatches through function-pointer tables" but "recursive descent
from `0x5BA057` reaches 309 functions; 738 indirect calls are reg-based memory
operands (full `.text` sweep, capstone, 2026-09-18)". Include the command or
script where one exists.

This is inherited from the sibling repo and it survives the archival/living
split intact. A living project still needs to know what is *true* — it just gets
to decide what to do about it. The two questions stay separately answerable
(`CLAUDE.md` rule 6).

Where a claim rests on the PC port agreeing with the PSX version, say which one
was actually checked. The sibling's
[`PC_PORT_CROSS_REFERENCE.md`](../../BreathOfFire3Recomp/docs/PC_PORT_CROSS_REFERENCE.md)
is the worked example of doing this well, including two cases where the port was
right and the PSX-side notes were wrong.

## Evidence tiers for names

When recording what a function or field *is*, tier the claim explicitly —
`evidence`, `hypothesis`, or `unnamed`. The sibling repo's `NAME_MAP.md` imposes
this discipline and it is the reason its name corpus is trustworthy.
A name transferred automatically by the matcher (PLAN §3) is a **hypothesis**
until something confirms it.

## What does not go here

- Game data, or anything extracted from it. Including inline in a document —
  `.gitignore` cannot catch a paste. (`CLAUDE.md` rule 1.)
- Player-facing changelogs. `DIVERGENCE.md` is an engineering record, not release
  notes; they serve different readers and should not be merged.
