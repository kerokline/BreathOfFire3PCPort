# Getting the PSX overlay corpus into play

**Status:** DRAFT (investigated 2026-09-18)

Both kinship probes and [`bsim-evaluation.md`](bsim-evaluation.md) close on the
same open item: everything measured so far is **boot-EXE only**, and the overlay
corpus is supposed to be the prize. This is what is actually on disk, what it
costs to use, and one correction that changes the size of the prize by two
orders of magnitude.

## 0. The headline correction: there are 121 named overlay functions, not 29,036

`PLAN.md` §3, [`bsim-evaluation.md`](bsim-evaluation.md), both kinship probes and
two `prior-art/` notes all repeat "29,036 overlay functions". The claim
originates in one sentence of the sibling's
[`PC_PORT_CROSS_REFERENCE.md`](../../BreathOfFire3Recomp/docs/PC_PORT_CROSS_REFERENCE.md)
line 253, which attributes them to `names/functions.toml`. Measured:

| Quantity | Count | Measurement |
|---|---|---|
| Named overlay functions | **121** | `[[function]]` blocks in `names/functions.toml` (74 `evidence`, 46 `hypothesis`, 1 `verified`) |
| Overlays carrying any name | **8** of 406 | distinct `overlay` md5 keys in the same file |
| Named boot-EXE functions | **556** | `[[func]]` rows in the sibling's `symbols.toml` |
| Statically discovered boot functions | **1,026** | `analysis/analysis.json` `functions` |
| Statically discovered overlay functions | **5,805** | sum of `function_count` over 406 records in `analysis/overlay_catalog.json` |

A whole-repo grep for `29036`/`29,036` in the sibling returns only coincidental
hits inside `analysis/callstacks/*.json` trace data — **no artifact on disk
produces that number.** `docs/NAME_MAP.md` line 94 still says "23 `evidence`, 23
`hypothesis`" overlay functions, which is the same corpus at an earlier date.

This does not kill the idea; it re-aims it. The transferable asset is not 29k
ready-made names. It is:

- **121 hand-established overlay names** — small, but high quality, and the
  battle-engine probe already transferred three of them onto the PC binary.
- **5,805 statically discovered function entries** (≈12,800 once Ghidra
  re-analyses — see §5), which are *structure*, not names: a call graph, in- and
  out-degree, constant references. That is what BSim and delta propagation
  actually consume.
- **216 of 406 overlays with a human role/alias** in `names/overlays.toml`
  (135 `evidence`, 79 `hypothesis`, 2 `verified`, 190 `unnamed`) — subsystem
  labels, which is what lets a BSim hit be interpreted at all.

`PLAN.md` §3's "transfer even 30% of those names" arithmetic should be redone
against 121 + 556, not 30,062. **Correcting the five documents that repeat the
figure is a separate task and is not done here.**

*(Done 2026-09-18, immediately after this document landed: `PLAN.md` §3, `STATUS.md`, both kinship probes, `bsim-evaluation.md`, `prior-art/README.md` and `prior-art/diaphora.md` corrected, each marking the correction rather than silently editing the number.)*

### Provenance of the bad number, and a caveat on the good one

**Where 29,036 came from: nowhere.** `git log -S'29,036' --all` in the sibling
returns a single commit — `837e981`, *"PC_PORT_CROSS_REFERENCE: record the third
derivative"*, the commit that added the §6 section describing *this* project. The
figure never existed in any data file. Candidate multiplex aggregates over
`analysis/overlay_catalog.json` do not reproduce it either: `sum(static_total ×
band_occupant_count)` = 455,221 and `sum(jal × band_occupant_count)` = 199,761.

The **1,026 does have an origin**: `analysis/analysis.json` carries
`stats.total_functions = 1026`, the boot EXE's *discovered* count. That sentence
took a real number and labelled it "named" (the named count is 556).

**And 5,805 is softer than it looks.** It is `roots.static_total`, the union of
`jal` targets and prologue-pattern matches:

| | count |
|---|---|
| `jal` targets | 2,680 |
| prologue matches | 4,797 |
| both | 1,672 |
| union (`static_total`) | **5,805** |
| distinct PCs observed at runtime, all bands (`counts.observed_pcs`) | 3,345 |

So **only 2,680 of 5,805 are actually called from anywhere**; the remaining 3,125
are prologue-shaped bytes with no caller. Static discovery over-approximates, and
the count falls as overlay residency becomes knowable — which means **any
overlay-derived function count is a moving target and must be dated when
cited**, this one included (2026-09-18).

Consequence for §5's effort estimate: the "12–13k Ghidra functions" projection
rests on a 2.2x ratio over static discovery, so soft roots **compound** rather
than cancel. An overlay BSim database would be not just ~17x larger than the
boot-EXE-only pool but also noisier, indexing prologue-guessed boundaries that
are not functions. A second, independent reason the import must happen on a
database copy with the nine published pairs re-scored before and after.

## 1. What already exists on disk

All paths relative to `../BreathOfFire3Recomp` (read-only to us).

| Artifact | Contents | State |
|---|---|---|
| `analysis/overlay_captures_all.json` (7.6 MB) | 406 records: `load_addr`, `size`, **`bytes_b64` (the overlay image)**, entry-PC seed lists, `source_md5`, `crc32` | **Complete.** All 406 records carry `bytes_b64`; base64-decoded length equals the declared `size` for **406/406**. Total 3,820,812 bytes of overlay image. |
| `analysis/overlay_catalog.json` (1.3 MB) | per-overlay metadata: family, registry id, band, `function_count`, runtime heat | Complete, 406 records, 406 distinct `content_md5` |
| `names/overlays.toml` | 406 `[[overlay]]` rows keyed by md5 | Complete |
| `names/functions.toml` | 121 `[[function]]` rows keyed by (overlay md5, pc) | Sparse — 8 overlays |
| `analysis/ghidra/*.json` | Ghidra exports for **7 overlays** + the boot EXE | Partial, listed below |
| `tools/extract_overlays.py`, `tools/emi_survey.py` | rebuild the captures from the disc, statically | Not needed; see below |
| `tools/ghidra_run.py`, `tools/ghidra/{seed_overlay,export_program,apply_names}.py` | headless import/seed/analyse/export driver | Working, reusable with adaptation (§3) |
| `docs/OVERLAYS.md`, `OVERLAY_EXTRACTION.md`, `OVERLAY_HEADERS.md`, `OVERLAY_SIZE.md` | the why, the how, the header format, the band map | Current |

**Confirmed: the overlay images are already extracted and sitting on disk. No
re-extraction run is needed.** Even if it were, `extract_overlays.py`'s own
header states extraction is fully static from the .EMI TOC on the disc image —
"no DMA-time capture" — so regeneration would be minutes, not a play session.
The one thing that *is* session-derived is `dispatch_entry_pcs` (112,736 PCs
across the 406, harvested from live interpretation) and
`analysis/callstacks/*.json`; those cannot be regenerated cheaply, but they are
additive seeds, not the images.

Already-analysed overlays (`analysis/ghidra/`), Ghidra's own function counts:

| Program | Ghidra fns | static roots |
|---|---|---|
| `START_EMI8_801D0C00` | 674 | 169 |
| `BATTLE_EMI3_801D0C00` | 663 | 239 |
| `GAME_EMI0_80196800` | 583 | 497 |
| `SHOP_EMI0_801D0C00` | 535 | 136 |
| `BATTLE_EMI15_80093800` | 364 | 303 |
| `BATL_END_EMI0_801EEC00` | 109 | 17 |
| `SHOP_EMI8_801EEC00` | 99 | 11 |
| `SLPS_009.90` (boot) | 1,029 | 1,026 |

These exports live in *their* Ghidra project. They are useful to us as a
cross-check, not as an input — BSim needs programs in **our** project.

## 2. Overlay inventory and priority order

406 overlays, 401 distinct source files, 406 distinct registry ids, 406 distinct
content md5s. Per-overlay static function count: min 0, median **7**, mean 14.3,
max 497. 25 overlays have zero discovered functions.

Families: BMAGIC 124, WORLD04 44, WORLD01 37, WORLD00 35, WORLD02 35, BOSS 35,
WORLD03 33, SCENARIO 25, PLCHAR 19, ETC 14, BATTLE 4, LOGO.EXE 1.

The distribution is extremely long-tailed:

| | static fns | share | bytes |
|---|---|---|---|
| top 5 overlays | 1,400 | 24% | 717 KB |
| top 10 | 1,891 | 33% | 986 KB |
| top 20 | 2,406 | 41% | 1.27 MB |
| top 50 | 3,347 | 58% | 1.75 MB |
| all 406 | 5,805 | 100% | 3.82 MB |

**Priority order for import** — the top ten by static function count, annotated
with named functions already held and runtime heat from `overlay_catalog.json`:

| # | Overlay | Load addr | Size | static fns | named fns | note |
|---|---|---|---|---|---|---|
| 1 | `BIN/ETC/GAME.EMI#0` | `0x80196800` | 227 KB | 497 | 8 | field engine; 2nd-highest heat (1.6e8 insns) |
| 2 | `BIN/BATTLE/BATTLE.EMI#15` | `0x80093800` | 133 KB | 303 | 26 | battle engine; probe-proven transferable |
| 3 | `BIN/BATTLE/BATTLE.EMI#3` | `0x801D0C00` | 118 KB | 239 | **68** | battle game-mode; most named of any overlay |
| 4 | `LOGO/LOGO.EXE#0` | `0x801CE000` | 121 KB | 192 | 0 | highest heat, but it is the intro — low value |
| 5 | `BIN/ETC/START.EMI#8` | `0x801D0C00` | 118 KB | 169 | 1 | title/save menu |
| 6 | `BIN/ETC/SHOP.EMI#0` | `0x801D0C00` | 87 KB | 136 | 2 | |
| 7 | `BIN/WORLD00/AREA030.EMI#4` | `0x801D0C00` | 75 KB | 129 | 0 | representative area overlay |
| 8 | `BIN/ETC/COMMU02.EMI#8` | `0x801D0C00` | 43 KB | 94 | 0 | |
| 9 | `BIN/SCENARIO/SCENA08.EMI#0` | `0x801F6C00` | 32 KB | 67 | 0 | representative scenario overlay |
| 10 | `BIN/SCENARIO/SCENA09.EMI#0` | `0x801F6C00` | 31 KB | 65 | 0 | |

Recommended first wave is **not** the raw top ten. Take **GAME.EMI#0,
BATTLE.EMI#15, BATTLE.EMI#3, START.EMI#8, SHOP.EMI#0, BATL_END.EMI#0,
SHOP.EMI#8, MAGIC069.EMI#3** — the two that already have names attached, the
engine that dominates runtime, and the five remaining overlays that carry any
`functions.toml` entry at all. That set is every overlay for which we hold
ground truth, so it is the set where BSim's output can be *scored* rather than
merely generated. `LOGO.EXE#0` is deliberately out: it runs the Capcom logo and
its code is unlikely to survive into a PC port that plays an AVI.

Then one representative of each large family (AREA030 for WORLD, SCENA08 for
SCENARIO, a BMAGIC and a BOSS overlay) to learn whether family members are
near-identical — 124 BMAGIC overlays with a median of 7 functions each smell
like one template recompiled per spell, and if so, one import per family
teaches most of what 124 would.

## 3. Reusability of the sibling's driver

**Directly reusable, conceptually.** `tools/ghidra_run.py import` already does
the exact job: decode `bytes_b64` to a temp file, `-import` with `BinaryLoader`
at the capture's `load_addr`, `-processor MIPS:LE:32:default`, run
`seed_overlay.py` as a pre-script with a seed JSON of function starts, then
analyse and export. Its program naming (`<FILE>_EMI<idx>_<LOADADDR>`) was built
precisely so the same overlay at two addresses stays two programs.

**Reusable as-is:** nothing, because of paths and side effects.

**Config-only:** the project target. `ghidra_run.py` reads `PSX_GHIDRA_DIR`,
`PSX_GHIDRA_PROJECT_DIR`, `PSX_GHIDRA_PROJECT` from the environment, so pointing
it at `BoF3PC` is one variable. That does **not** make running it safe — see
below.

**Needs adapting / rewriting on our side:**

1. **The `merge` subcommand must never run.** It writes `names/functions.toml`
   and, with `--symbols`, their `symbols.toml`. Our version should not have the
   subcommand at all rather than rely on not typing it.
2. **Every path is repo-relative to the sibling**: `analysis/overlay_captures_all.json`,
   `disc/SLPS_009.90`, `generated/SLPS_009.90_full.ranges`, `symbols.toml`,
   `names/*.toml`, plus `import callstack_diff` from their `tools/`. A port
   needs these as explicit read-only inputs. Precedent exists: our
   `tools/ghidra_pc.py` already reads `../BreathOfFire3Recomp/disc/SLPS_009.90`
   directly.
3. **`traced_entries()` reads `analysis/callstacks/*.json`** — runtime capture we
   do not have and cannot produce. Read theirs read-only, or drop the seeds and
   accept slightly fewer function starts.
4. **The boot seed is the problem, not a freebie** — §4.
5. **Their pre/post scripts (`seed_overlay.py`, `export_program.py`) are Ghidra
   scripts, not repo tools.** We could pass `-scriptPath` at their directory, but
   that couples our runs to their working tree. Our `tools/ghidra/` already holds
   `bsim_query.py` and `check_funcs.py`; an equivalent seeder belongs there.

Verdict: **write our own `tools/ghidra_pc.py overlays` subcommand, modelled on
theirs, reading their data files.** Roughly 150 lines. Copying their driver
wholesale imports the merge step, their path layout and a dependency on
`callstack_diff.py`, for no benefit.

## 4. The load-address collision problem

Three distinct collisions, and only one of them is hard.

**(a) Overlays collide with the boot EXE.** Boot text is `0x80093800` +
`0x163800`, ending `0x801F7000`. **All eleven overlay bands start inside that
span** — `BATTLE.EMI#15` loads exactly at `0x80093800`, the boot text start. The
boot image is 81.6% zeros (`OVERLAYS.md` §1); the overlays land in the holes.
So a bare PSX address is meaningless without knowing which overlay was resident.

**(b) Overlays collide with each other, massively.** Occupants per band:

| Band | Occupants | Families |
|---|---|---|
| `0x801F2C00` | **181** | WORLD00–04 |
| `0x801EEC00` | **128** | BMAGIC, BATTLE, ETC |
| `0x800C1800` | 35 | BOSS |
| `0x801F6C00` | 20 | SCENARIO |
| `0x801CE400` | 19 | PLCHAR |
| `0x801D0C00` | 18 | BATTLE, ETC, SCENARIO, WORLD00 |
| `0x80093800`, `0x800F5000`, `0x80117000`, `0x80196800`, `0x801CE000` | 1 each | |

181 different overlays share one base address. Address alone cannot be an
identity anywhere in this corpus.

**(c) The same content at two addresses.** Handled by construction: the sibling
keys `names/overlays.toml` by **md5 of the section bytes, not by load address**,
with a comment saying exactly why ("bands overlap"), and the program name embeds
the load address. In the current captures every one of the 406 has a distinct
md5 and a single load address, so (c) is latent, not live.

**What this means for a BSim database.** BSim's unit of identity is
(executable, address), and executables are keyed by file md5 — our
`tools/ghidra/bsim_query.py` already records `exe` on every match. Import each
overlay as its own program from its own image file and identity is
automatically correct: 406 distinct md5s, measured. Nothing extra is needed.
The work is on the *interpretation* side — a BSim hit comes back as
(exe name, address), and turning that into a `names/functions.toml` lookup needs
the program→md5 mapping the sibling keeps in `analysis/ghidra/*.meta.json`. Our
importer should write the same sidecar.

**The real hazard is the boot seed.** `seed_overlay.py` maps the boot EXE into
each overlay program as `boot_*` memory blocks and creates a function at every
boot entry — `generated/SLPS_009.90_full.ranges` holds **1,468 `F` lines**, and
`BATTLE_EMI15_80093800` came out with 51 such blocks. This is right for
decompilation (without it, every overlay decompile truncates at the first call
into boot code) and **wrong for BSim**: indexing 406 programs that each contain
~1,400 boot functions would commit roughly **600,000 near-duplicate signatures**
of ~1,000 distinct boot functions. BSim scores by rarity, so mass duplication
does not just waste space — it flattens significance and degrades every query,
including the boot-EXE queries that currently work. Their `export_program.py`
already excludes `boot_*` functions from exports, so the distinction exists in
their tooling; BSim does not inherit it.

Mitigation, cheapest first: delete or un-define functions inside `boot_*` blocks
in a post-script before `generatesigs`. Alternative is importing with
`--no-boot`, but the boot seed changes call resolution and therefore the P-code
that BSim fingerprints, so the seeded form is probably the *better* signature —
this needs measuring, not assuming.

## 5. Effort estimate

| # | Step | Kind | Estimate |
|---|---|---|---|
| 1 | `tools/ghidra_pc.py overlays` — read their captures JSON, decode to scratch, import + seed at `load_addr`, write `.meta.json` sidecar. Import into a `/overlays` folder in `BoF3PC`. | script, ~150 lines | 0.5 day |
| 2 | Port `seed_overlay.py` into our `tools/ghidra/` | adapt, ~250 lines | 0.5 day |
| 3 | Import + analyse the 8-overlay first wave | machine time | ~5 min |
| 4 | Boot-function suppression post-script, then A/B: BSim ranks with the boot seed vs `--no-boot` | **real work** | 1 day |
| 5 | `generatesigs` over the overlay folder, commit into the existing `medium_nosize` DB | config | ~10 min |
| 6 | Score against the 3 overlay pairs in our `symbols.toml` | script | 0.5 day |
| 7 | Import + index all 406 | machine time | ~1–2 h |
| 8 | Name lookup: BSim `(exe, addr)` → `meta.json` md5 → `names/functions.toml` | script | 0.5 day |

**Machine time is not the constraint.** Our own `analysis/ghidra_import_psx.log`
records the 1.4 MB PSX boot EXE importing and analysing headless in **15 s**
(3.6 s of that JVM startup). The whole overlay corpus is 3.82 MB. Per-overlay
JVM startup dominates: at ~8 s each, 406 sequential headless runs is ~1 h, and
that is the honest number for step 7.

**`generatesigs` does not need 406 invocations.** `bsim.bat` usage (run
2026-09-18) shows the Ghidra URL form
`ghidra:/<dir>/<project>?/<folder-path>` — a *folder* path. Importing overlays
into one project folder makes signature generation a single call.

**Expected corpus size.** Across the 7 already-analysed overlays Ghidra finds
3,027 functions where static discovery found 1,372 — a 2.2x ratio. Applied to
5,805 static roots, expect roughly **12,000–13,000 overlay functions** in the
BSim database, against the current 749-function PSX boot pool. That is a 17x
larger candidate pool, which is the thing to watch: `bsim-evaluation.md` already
warns its ranks are a mild over-estimate because the pool was small, and this
step makes the pool large.

**Riskiest step: #4.** Everything else is plumbing with a known shape. Step 4
decides whether the BSim database is usable at all, and it has a failure mode
that is silent — if boot functions leak into the index, queries still return
results, they are just quietly worse, and the boot-EXE ranks in
`bsim-evaluation.md` would degrade without anything visibly breaking. Re-running
the nine-pair scoring from that document **before and after** the overlay commit
is the regression check, and it should be done on a database copy, not the one
that produced the published numbers.

## 6. What could not be determined

- **Where 29,036 came from.** No artifact on disk produces it, and I did not
  trace the sibling's git history for the commit that introduced the sentence.
- **Whether BMAGIC/WORLD family members are near-duplicates.** 124 BMAGIC
  overlays at a median 7 functions each *look* like one template per spell, but
  nothing on disk answers it. `content_md5` is distinct for all 406, which only
  rules out byte-identity. Cheap to settle: import three BMAGIC overlays and
  diff their function inventories.
- **Whether the boot seed helps or hurts BSim signatures.** Argued both ways
  above; step 4 measures it.
- **Ghidra analysis quality on overlay images without the runtime seeds.** Their
  imports used `dispatch_entry_pcs` and callstack-derived entries we would be
  reading from their tree; how much coverage is lost without them is unmeasured.
- **Whether `medium_nosize` still behaves at 13k functions.** The
  `MsgBox_Step`-returns-one-candidate anomaly (`bsim-evaluation.md`, Open) is
  unexplained at 749, and I would not assume it stays benign at 13,000.
- **Runtime heat for 401 of 406 overlays is absent** (`heat` is null), so
  "which overlays matter at runtime" can only be answered for the five that were
  profiled. The priority order in §2 is therefore static-function-count-driven,
  which is a proxy.
