# Prior art: Severed Chains (The Legend of Dragoon, PSX → Java)

**Status:** DRAFT (researched 2026-09-26, at upstream commit `d3d4c02`, 2026-09-18)

Research note on [Severed Chains](https://github.com/Legend-of-Dragoon-Modding/Severed-Chains),
a from-scratch Java engine for the 1999 PlayStation *Legend of Dragoon*. It was
built from a decompilation and now has none of the original code left. It is the
fifth project in this directory and the first whose *source* is a PlayStation
game. It is also the first that has already built a modding API. We only name
that API as a Phase 5 goal (PLAN §5).

Everything below was read from the main repo, its three-page wiki and five
sibling repos from the same org (`asm-translator`, `Mod-Loader`,
`Script-Recompiler`, `lod-core`, `Severed-Chains-Metadata-Utility`), shallow
clones taken 2026-09-26. Paths are relative to the repo named. **Licence:
AGPL-3.0 on all six. Read only** (§5 below; `CLAUDE.md` rule 5,
[`LICENSING.md`](../LICENSING.md) §4).

---

## 1. What it is, and how it differs from us

| | Severed Chains | this repo |
|---|---|---|
| Source of truth | The PSX disc (`SCUS94491`..`586`) | The 2001 PC `BOF3.exe`, cross-checked against the PSX disc |
| Route | Ghidra listing → mechanical MIPS→Java translator running on an emulated-hardware core (`lod-core`) → hand rewrite into typed Java | Hybrid binary: DLL detours one function at a time (PLAN §2C) |
| Today | 100% Java. `grep "MEMORY.\|CPU.\|.ref("` over `src/main/java` finds nothing. `lod-core` is archived, "no longer needed to run the game" | 1,465 of ~2,952 recorded functions ours ([`STATUS.md`](../STATUS.md)) |
| Oracle | None automated. Three E2E tests, excluded in `build.gradle` (`test { exclude '**/*' }`), not run in CI | Frame hash, shadow checks, fuzz against copies of Capcom's function, negative controls |
| Divergence record | A wiki tier policy, `patches/scripts.csv`, and code comments | [`DIVERGENCE.md`](../DIVERGENCE.md), checked in CI by `tools/ledger_check.py` |

In short: their engineering *after* decompilation is far ahead of ours, and
their verification is far behind. They are another case for the finding in
[`README.md`](README.md) §1 that nobody kept an oracle. What we can learn from
them is mostly about **Phase 5**, and about **patching data and scripts**, not
about how to decompile.

---

## 2. Techniques worth adopting

### 2.1 Divergence tiers, with a default one (small, docs-only)

The wiki page `Retail-Accuracy.md` sorts every change into one of four tiers:

- **Retail Accurate**: 1:1, crashes included.
- **Retail Intent**: fix crashes, hangs and clear bugs.
- **Retail Sensible**: loading times, save-anywhere, cutscene skip, fast text. This tier is *the default target*.
- **Non-Retail**: balance, curves, formulas. These go only through the mod API.

It also commits that options or mods will "permit you to recover Retail
Accurate behaviour". Debug features sit outside the scheme.

Our ledger already records *what* changed and whether it is reversible. It does
not record *which kind* of change each entry is. A tier field would add that:

- **`Tier:`** as a new ledger field (Accurate / Intent / Sensible / Non-retail).
  `ledger_check.py` could enforce it the same way it enforces the others.
- **Presets drop out of it for free.** "Play as shipped" means turn off every
  Intent-and-above entry. "Default" means Intent + Sensible. This is the same
  destination as TR1X's era presets ([`README.md`](README.md) §3), reached from
  data we already keep.
- **The honest tier is "Accurate", not "PC-port accurate".** Our Accurate tier
  has two candidates, the PC port and the PSX release. The PSX record already
  has a field (*Also in the PSX version?*). The tier should state which of the
  two it restores.

### 2.2 Patches as diffs of disassembly text, applied at first run

This is the most transferable single technique. It is `ScriptPatcher.java` in
`src/main/java/legend/game/unpacker/scripts/`, together with the
`Script-Recompiler` library.

- Each retail script is **disassembled to text**. A unified diff is applied to
  the text, and the result is **reassembled**. What is in the repo is the diff,
  not the bytes. That keeps it readable and reviewable, and it holds no game data.
- **`patches/scripts.csv` is the ledger**: 349 rows (347 `diff`, 2
  `replacement`), each with a target, a patch file and a free-text reason, e.g.
  `diff,SECT/DRGN1.BIN/20,scripts/DRGN1/20.diff,"Lizardman deallocate leaked script state"`.
- A sidecar `.config.csv` gives the disassembler what it cannot infer (extra
  branch targets, table lengths).
- **Shared includes** live in `patches/libs/` (`widescreen.txt`, `char_utils.txt`).
- **Patching is incremental and reversible.** The originals are backed up.
  CRC32s of each patch and its includes are cached, so only changed patches
  are reapplied. A deleted patch restores its original.

**For us:** our two script formats already decode completely:

- the event/placement script: all 200 areas via `tools/event_scan.py`, byte-identical to the PSX disc ([`event-script.md`](../event-script.md));
- the movement script ([`movement-script.md`](../movement-script.md), `tools/movement_scan.py`).

`tools/loc_build.py` already follows the pattern of building overlays *from the
player's disc* into the gitignored game directory. So the missing piece is only
an **assembler** for each script format and a patch step in `loc_build.py`.
After that, script-level fixes (a softlock, a flag bug, widening a camera for
the 426-wide view in DIV-0041) become reviewable diffs with ledger IDs, not
code workarounds in the interpreter. One refinement over theirs: the reason
column should be a `DIV-NNNN`, so the CSV and the ledger cannot drift.
Candidate for IDEAS.

### 2.3 One source of truth for the script VM's ABI

Every native script handler in Severed Chains carries `@ScriptDescription`,
`@ScriptParam(direction, type, name, description)` and `@Method(0x800…)`. CI
runs the metadata utility, which reflects over the handler table
(`scriptSubFunctions_8004e29c`) and emits `descriptions.csv` / `params.csv` /
`enums.csv`. The disassembler reads those to print named, typed operands. So
the **documentation, the disassembler and the interpreter cannot disagree**.

We have the table-driven half of this for functions (`symbols.toml` →
`symbols.gen.h`). We do not have it for script ops. Right now op meanings live
in prose (`event-script.md`, `movement-script.md`) and separately in the
scanners. A `script_ops.toml` would fix that: opcode, length, operand types,
name, `evidence` tier. `event_scan.py`, the §2.2 assembler, and a generated
header for our interpreter would all read it. That is the same move as
`symbols.toml`, applied one level up.

### 2.4 Names that carry the size, offset and address

Their wiki's `Naming-Guidelines.md`:

- struct types carry their size: `GameState52c`, `Model124`;
- fields carry their offset: `gold_94`, `chapterIndex_98`, `_b0`;
- globals carry their address: `gameState_800babc8`;
- a method carries its address in an annotation instead (`@Method(0x800133acL)`, 1,793 of them).

Their stated reason: it "aids in the two-way investigations still required daily".

We already do most of this. Fields are named like `area_1f` and `frame_bd`, and
functions get their address from `symbols.toml`. The one piece we lack is the
**size in the struct name**, and it is cheap. A `static_assert(sizeof)` beside
it would make it checkable, which theirs is not. Worth adopting as a convention
for *new* struct types only. Renaming the existing ones would be churn for no
behavioural value.

### 2.5 First-run unpack with a version stamp and a disc check

`Unpacker.java` unpacks the player's discs into `files/`:

- It checks each ISO's volume ID against the known US IDs, with a distinct
  "wrong region" message for the EU and JP discs.
- It writes a `VERSION = 5` stamp. A stale stamp **wipes the unpack and redoes it**.
- The transforms are a fixpoint pipeline of `LeafTransformation(name,
  discriminator, transformer)`. Examples: decompress, un-archive, slice named
  assets out of the executable, transcode XA→Opus, and patch named asset bugs
  such as "Lavitz oof Dart missing hand".

`loc_build.py` has no build stamp that I can see, so a player's overlays could
outlive a change to the builder. **A version stamp in each built overlay,
checked by `LoadDatFile` at start-up,** is small and prevents a whole class of
"works on my machine" reports. Also worth copying: named, individually listed
data fixes. Each one is a ledger row, not an anonymous byte patch.

### 2.6 Modding: what Phase 5 should look like (gated, LOW)

This is the part of Severed Chains with no counterpart here yet. It is also the
part most at risk of being imported wholesale without thought.

- **Built-in content is itself a mod.** `CoreMod` holds the engine and
  "Game can not run without it". `LodMod` holds the retail content and "will be
  able to be disabled for total overhaul mods".
- **Typed, namespaced, lockable registries.** There are 21 of them in
  `core/Registries.java`: items, equipment, spells, encounters, elements, input
  actions, config and more. IDs look like `lod:fire`, and the registries lock
  after start-up.
- **Modify-and-return events.** There are 98 `EVENTS.postEvent(...)` sites,
  many shaped like `x = EVENTS.postEvent(new XEvent(x)).x`. A mod sees and
  replaces a value at a known point, with no detour.
- **Hardcoding is removed in the scripts too, not just the engine.** 82 of the
  349 script patches mention registries, character IDs or character counts.
  Script bytecode was extended with parameter types `0x20`..`0x23` so that
  retail scripts can name registry entries.
- **Config has a scope:** GLOBAL or CAMPAIGN (per save), each entry with an
  `onChange` hook. The enabled-mods list is itself per-campaign config.

**For us:** none of this is actionable until the logic it touches is ours. The
shape still matters now, for one reason. Their "modify-and-return event at a
known point" is exactly what a *faithful* takeover of ours can grow into
without changing behaviour: our function calls a hook, and the hook's default
returns its argument. If Phase 5 is to add those, the decomp should keep the
values a mod would want in named locals at those points, rather than folded
away. That costs nothing today. Also noted: their per-save config scope answers
a question our `bof3x.ini` will face once a divergence changes game *rules*
rather than presentation. A rules change should travel with the save, not the
machine.

### 2.7 Small ones

- **Crash saves** (`CREATE_CRASH_SAVE_CONFIG`): write a save from the crash
  handler. We have the in-process crash reporter
  ([`crash-reporter.md`](../crash-reporter.md), I11), and a save-from-crash is
  a natural next step. It needs care: the state being saved is the state that
  crashed.
- **Retail save import** (`RetailSavedGame`, `convert_memcard.sh`). We already
  have this, and in both directions (I1).
- **Render and logic rates are separate knobs.** They compute
  `hz = 60 / vsyncMode * gameSpeedMultiplier` for input and logic, and
  interpolate model animation on top (`Model124.interpolationScale`). This is
  relevant if we ever go past DIV-0047/0048's pacing. *Unverified* whether
  their submap change to 60 Hz render / 30 Hz scripts differs from retail.
- **Rolling dev build**: every push to `main` stamps build number and commit
  into `Version.java` and updates a `devbuild` release. This is useful once
  there are players. It is not urgent.

---

## 3. What not to copy

- **Their verification posture.** The tests are compiled out and CI never runs
  them. Retail matching is recorded in comments ("This matches the retail
  behaviour (it uses divu)", `combat/deff/Cmb.java:101`). Engine-level changes
  are marked only by comments like "Retail bug: …" and "NOTE: fixed a retail
  bug…". Only *script* changes have a ledger. Our frame-hash, shadow and fuzz
  checks are the thing to protect, not to trade away for their velocity.
- **Retail data transcribed into source.** The README says the code "does not
  include any official Legend of Dragoon code or assets". Yet equipment stat
  tables are Java constructors (`lodmod/LodEquipment.java`), and item names and
  descriptions are in `src/main/resources/lod/lang/en.lang`. For us that would
  break rule 1 ([`LICENSING.md`](../LICENSING.md) §3). Our overlays stay *built
  from the player's disc*, and a registry for BoF3 content would need to be
  filled at start-up from `DAT/`, not from tables in `src/`.
- **Mechanical translation onto emulated hardware** (`asm-translator` +
  `lod-core`). This is their counterpart of PLAN §2B / our I3 lifter. It worked
  for them as scaffolding and was then thrown away entirely. That is consistent
  with I3's framing as an *accelerator*, not an architecture. It is not a
  reason to change course.

---

## 4. Proposed follow-ups

None of these are scheduled. Each is a candidate for [`IDEAS.md`](../IDEAS.md):

| # | What | Kind | Feasibility |
|---|---|---|---|
| a | A `Tier:` field in the ledger (§2.1), enforced by `ledger_check.py`, backfilled over DIV-0001..0057 | docs / process | **built 2026-09-26**: [`DIVERGENCE.md`](../DIVERGENCE.md) "Tiers", with a fourth tier, *Forced*, theirs lacks. Presets from it are not built |
| b | `script_ops.toml` as the single description of the event and movement script ops, read by the scanners (§2.3) | tooling | HIGH |
| c | A script assembler + diff-patch step in `loc_build.py`, with a `DIV-NNNN` per patch (§2.2) | tooling | MEDIUM (needs b) |
| d | A version stamp in built overlays, checked at load (§2.5) | tooling | HIGH |
| e | A size suffix on new struct type names, with `static_assert` (§2.4) | process | HIGH |
| f | Faithful takeovers keep "mod-visible" values named at natural hook points (§2.6) | process | HIGH, but only worth it if Phase 5 is committed to |

---

## 5. Licence

AGPL-3.0 (`LICENSE`, and the same in each sibling repo). It is copyleft, as
with OpenRCT2 and TRX ([`README.md`](README.md) §4): **read only**, and no
vendoring under any notice arrangement, since our PolyForm NC terms cannot sit
beside it. Every technique above is written so that it can be built from this
description alone.

## 6. Not established

- Any progress metric. There is no tracker in the repo, and 250 `FUN_800…`
  names remain. It may live on Discord or in issues (192 open, via the GitHub API).
- Release history and commit count (not fetched).
- Whether their render/logic split in submaps is a divergence from retail.
