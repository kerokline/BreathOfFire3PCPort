# The install-gap round: staging

**Status:** DONE (merged as PR #91, 2026-10-19). Since then: **D279 fixed
2026-10-18** by DIV-0144 and DIV-0069's amendment
([`known-defects.md`](known-defects.md) D279), so the crash below is
history; **D266 fixed 2026-10-20** (the `titlemark` rule, PR #98); item 6
(STRICT as the default) moved to [`exe-gap-round.md`](exe-gap-round.md).
The paragraph that follows is the status as it stood on 2026-10-18.

(Staged 2026-10-17 against `origin/main` `8382691d`;
run 2026-10-18. **Stage A** (RD, PB, OT, TW, LA, RW) and **stage B as run**
(PT, CW, FP; section 3a) are merged on `install-gap/round`, with the recipes
regenerated over the seven discs after each tool change; the tip is `0baf26f7`.
**The end batch's docs are done** on `install-gap/end-docs` (section 5 item 5:
the census's section 8 rewritten with every status, the drafts written into
DIVERGENCE as amendments of DIV-0101, DIV-0103, DIV-0069 and DIV-0064, D319
and D320 numbered, HANDOFF item 4 replaced, the owner's calls in
`owner-review.md`). Items 1 and 2 ran at the merges (the checks; `exe_scan.py`
at the tip, the census's 8.1). **Items 3 and 4 ran on 2026-10-18 evening**
(section 6, branch `install-gap/live`). The self-tests pass. The gate and
the eleven routes show no game-state difference the round made: every page
left is a ledgered divergence or a stale pair. The captures are in
`g_abc\shots`. One finding: D279 crashes `psx-jp`'s fishing spot every run. **Item 6, `BOF3X_EXEIMAGE_STRICT=1` as the default, is not ready**:
RW's watch must be re-run on the merged branch, and the two rows still read in
play are the owner's calls (D266's title mark, the empty save slot). The
census it closes is [`install-gap-census.md`](install-gap-census.md). This is
also the staging of [`IDEAS.md`](IDEAS.md) I43's steps 2 to 4: RW is step 2,
the fix groups step 3, the end batch's last item step 4.)

The census gave every row of `recipes/exe-gaps.tsv` a status and left, in its
section 8, the NEEDS FIX items across the seven disc builds: two P1 items not
done, the P2 twins the disc search already locates, the P3 long tail and the
P4 "find a reader" rows. This document stages that remainder as parallel
groups that can run with no owner present. The 13 OWNER CALL items are not in
it (section 7).

## 0. The shape in one paragraph

The remaining items split by **method**, and each method touches a different
set of files, which is what makes them parallel:

| Method | Items (census 8) | What it edits |
|---|---|---|
| a `twin`: a short run verbatim in one file of the disc | P2 item 11, P1 item 7 | `tools/psp_exe.py` `TWINS` |
| the PSP's `BOOT.BIN` as a twin's file | P2 item 12, twins-1013 6's class | `tools/psp_exe.py` (the ELF reader), `TWINS` |
| the owning function's immediates on the player's disc | P2 item 13, P3's state and area tables, P1 item 8 | a new place kind in the recipe tools, `symbols.toml` `psx` fields |
| a language layer entry or a module's widths | P3's FR / DE rows, D286, D289 | `tools/loc_build.py`, `src/game/labels.cpp`, the module |
| a reader found first | P4 | a reading; then one of the above |

The generated recipes (`recipes/exe*.tsv`, `recipes/exe.toml`) are the one
file set every group would otherwise fight over. **No group commits a
regenerated recipe.** The coordinator regenerates after each merge (section
5), as the ultrawide round kept `widescreen.cpp` to one owner.

## 1. The rule every fix follows

The owner's stance on game data in code (2026-10-10): a label naming a game
section is fine; a copied Capcom value table in `src/` is not. The value
comes from a rule, or from the player's disc. The census's fixes so far obey
it: D310's strings are the engine's own text (a format the PC draws, not a
disc's), D311 and D267 are `twin` rows that read the player's disc. This
round adds one more shape, for the P3 rows whose bytes exist on no disc as
data because the PlayStation's compiler folded them into instructions: **a
place that reads immediates out of a PSX function's code** (section 2, OT).
That is still the player's disc, read at build time, with the function and
the instruction offsets as the recipe's row. A value typed into `src/` is a
stub by another name, and rule 4 applies.

## 2. Stage A (six groups, in parallel)

Each group works in its own worktree off `main`, commits with `git commit -s`,
self-tests headless only, and ends with a hand-back report naming what it
could not reach. Game-derived scratch (caches, installs, captures) stays
outside the tree; each group's harness root is its own
(`C:\Users\kerok\g_<group>`), never another's.

### TW: the PlayStation twins by byte search (`tools/psp_exe.py` `TWINS`)

The P2 rows whose windows the disc search already found (census 4, "`twin`
by byte search"), one segment each unless said:

- `Area115_Sprites` (10 of 10 windows) and `Area115_Buttons` (2 of 2): world
  map 115's legend frame;
- `EffectKind99_States` (3 of 3), `Fx106_ShardTables` (3 of 3);
- `EffectKind33_SignX`, `_SignZ`, `EffectKindA0_States`,
  `MagicBallOrb_Shades`, `AuraBreathDome_V`;
- `BossDolphin_Anims`, `BossGaist_Sounds`;
- `Area121_Cells`, `Area146_EffectStates` (its disc-unplaced row),
  `Area124_Weights` (EU, DE);
- `EffectKind3C_Op` (US, EU, FR), `EffectKind66_States` (1 of 4),
  `EffectKind5B_States` (1 of 2), `EffectKind86_States` (DE, 1 of 2),
  `EffectKind18_08_States` (1-2 of 2), `EffectKind18Sub5D_States` (1 of 2),
  `CommuMusic_CloseSteps` (FR, DE: 1 of 8), `Fish_Kinds+0x72A` (JP: 1 of 2);
- **`Area169_Rects+0xA`** (P1 item 7): 20 of 21 windows, no verbatim run of
  the whole 189 bytes on any PSX disc. TW extends `TWINS` to take a segment
  list for one row (the pieced rule the census asked for), with the
  "a twin takes back another file's differing byte" rule of D268 applied
  per segment.

Where a file holds the run more than once, the choice is by the reader, as
[`exe-image-twins-1013.md`](exe-image-twins-1013.md) 2 chose `Area94_EffectArgsD`'s
third copy: the PSX-US map's neighbouring segment and the `lui`/imm pairs
into the table. Record the choice in the doc, not only in the row.

Checks: `psp_exe.py check`, `exe_tables.py check`; a rebuilt cache for one
affected build per row class (`importer.py build --source <cue> --out
<cache>`, about 3.5 minutes) and `tools/exe_scan.py` on it, the row's bytes
now equal to the PC's; the headless self-test from that cache
(`BOF3X_SELFTEST_ONLY=1 BOF3X_EXEIMAGE=<cache> ...`, HANDOFF "How to run
things"). Output: `docs/install-gap-twins.md` (the rows, the copies chosen,
the segment rule) and the `TWINS` entries.

### PB: the PSP's `BOOT.BIN` as a twin's file (`tools/psp_exe.py`, the ELF reader)

[`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6 names the class: on
the PSP discs many remaining runs are held by one overlay **and** by
`BOOT.BIN`, and the PSP's native code reads `BOOT.BIN`'s copy. A twin there
wants `BOOT.BIN`'s reader settled first, the ELF's relocations, as `msets`
does. PB builds that reader once and then places:

- the default light pair `0x663B20` (`EventScript_OpLengths+0x15`, `+0x1B`)
  and `Sprite_ScreenMargins`: 12 bytes `BOOT.BIN` and `MAGIC095` both hold,
  read at every area entry;
- `MoveScript_EffectArg+0x45` (1 of 3 windows on the PSP; the PSX builds
  place it from the boot executable, the model);
- `EffectKind18Sub1C`, `Sub1D`, `Sub33`, `Sub64_States` (four bytes each,
  under the six-byte floor `unplaced.py` used);
- `ItemUse_HandlerIndex+0xD0`'s precedent (already placed from `BOOT.BIN`
  on both PSP discs) is the worked example.

Not PB's: `Char_WhelpSlot+0x4` (a single byte, no safe twin: stays a listed
gap) and `ConfigMenu_Steps+0xC` (RD's, a reader first). PB reads
`tools/psp_exe.py` but its `TWINS` rows land after TW merges (stage B), so
the two never edit the list at once; PB's own edits are the ELF reader.

Checks: as TW, on the `psp-jp` and `psp-eu` caches. Output: a section in
`docs/install-gap-twins.md` (PB commits it after TW's doc exists, the way PD
added to SS's in the ultrawide round).

### OT: the owning-function place kind (tools; the enabler for stage B)

[`exe-import-engine.md`](exe-import-engine.md) 3.3 measured the method and
did not build it: a PC function that reads a gap row has a PSX twin
(`symbols.toml` `psx`), and the twin's code holds the row's values as
immediates. OT builds the place kind: a recipe row that names the disc file,
the PSX function, and the instruction offsets whose immediates (`lui` /
`addiu` / `ori` / `li` pairs, `addiu`'s sign kept) rebuild the PC's bytes;
`exe_tables.py recipe` writes such rows where `find_places` has a function
twin and the immediates match the PC's bytes exactly, and `build` applies
them; `check` refuses a row whose immediates do not rebuild the PC's bytes
on the disc build they name.

Two inputs OT also delivers, because stage B needs them:

1. **The code-pointer pairs fed back as function twins.** 3.3's cheap
   widening: every placed code-pointer word is a (PC function, PSX function)
   pair the image already knows; written into `symbols.toml`'s `psx` fields
   per file, by a script, with the `hypothesis` tier the attract work used
   for imports. This is the one `symbols.toml` edit of stage A, and it is
   OT's alone.
2. **The measurement:** after the feed-back, how many of the P3 rows' readers
   have a twin (3.3 counted 19 of the mixed / small / other addresses before
   it). That number sizes stage B.

Checks: a unit test of the immediate decoder on `analysis/` fixtures the
group writes (counts and addresses only); `exe_tables.py check`;
`verify_fixtures.py`. Output: `docs/exe-immediate-places.md`.

### LA: the language and layout rows (`tools/loc_build.py`, `src/game/labels.cpp`)

The P3 rows that are a Latin disc's re-laid words, not game values:

- D286's FR / DE headings (`EffectKind0F_PanelTitles`): the module's widths
  in ours, DIV-0069's way ([`fishing-rule-pages.md`](fishing-rule-pages.md));
- `Equip_SlotIcons` labels, `SharedList_SortSteps`, `MenuList_GeneWinSteps`
  (FR, DE): a layer entry or the module's widths, decided row by row the
  same way;
- D289 (`CommuRank_Modes+0x8C`, group 16's one-record name shift): a `src/`
  change the census lists as its fix;
- `Encounter_SlotChance+0x38`: the US string is in `BATE.EMI`
  ([`exe-chinese-remaining.md`](exe-chinese-remaining.md) 7), a label group
  anchored on it;
- `CommuName_RecordNames+0xF4`: find `EffectKind5E`'s PSX twin first (the
  same doc, 7); `+0xF0` has no disc source found. LA says so again or finds
  one, and does not invent text.

Checks: `loc_build.py all --discs`; `importer.py check`; the language
self-test (`BOF3X_SHADOW='*,-field_c1'` under `fr-FR` and `de-DE`, the
Traps line); a capture of each screen on an FR and a DE disc-only install
through the existing routes (`campingFishing`, `menu_screens`, `shop`,
`sortScreens`) for the owner's eye by chat. Output: `docs/install-gap-labels.md`.

### RD: readers first (reads; no `src/`)

The P4 rows and the two reader questions the census left:

- `Music_LoadedTrack+0x1E30` (`0x6671EC..0x6671F3`): likely unread;
  `analysis/pc_xref.json` and a run with `BOF3X_DATLOG=1` and a memory
  watch say which;
- `ConfigMenu_Steps+0xC` (PSP): `BOOT.BIN` once, no reader by the scan;
- song 166 on a disc-only install: which install kind lacks it and what
  asks for it ([`music-open-ends.md`](music-open-ends.md));
- `Area146_EffectStates`' two rows (one disc-unplaced, one game-value) and
  `Area169_Rects`' game-value remainder: which reader each byte belongs to,
  so TW's and stage B's rows do not overlap.

Each answer is a status in the census's table and, where a reader exists, a
row handed to TW, PB or stage B. Output: `docs/install-gap-readers.md`.

### RW: the read watch - which gaps play reads (`src/hook`, an instrument; the exe host)

I43's step 2, and the ranking every other group wants: of the gap ranges
still zero in a disc-built image, which does play actually read? The exe
host owns the image's pages, so it can **page-guard the gap ranges**
(`PAGE_GUARD`, the vectored handler `calltrace.cpp` and `crash.cpp` already
install) and log the first read of each range with the reader's address,
the frame and the route - `BOF3X_GAPWATCH=<file>`, an `instrument = true`
module, off by default, no behaviour change (a guard page faults once and
is re-armed after the log line). Ranges that share a page with hot data
fall back to a page-granular count the way the state hash reads pages.
Run over the reference routes and the attract on each one-source install
(the seven A/B/C caches), from its own harness root. Output:
`docs/install-gap-reads.md`: per range, read or not, by which function, on
which route and install; the census's section 8 re-ranked by it (a range
read in play is a live defect; one never read can wait). RW's list is the
first thing TW, OF-A and OF-E take up.

Self-test: the watch on a scratch page with a planted read logs it once
and leaves the value untouched; off, nothing is guarded.

## 3. Stage B (after OT merges; OF-A and OF-E in parallel)

The P3 game-value rows by the owning-function method, split by family so
the two groups' `symbols.toml` entries never share a function:

- **OF-A, areas and scenario:** `Area26_EffectKinds`, `Area91_RingTurns`,
  `Area94_EffectArgsE`, `Area96_Handlers`, `Area115_Directions`,
  `Area118_SpawnKinds`, `Area121_BandHeights`, `Area135_EffectKinds`,
  `Area141_CellEntries`, `Area146_EffectStates` (game-value),
  `Area154_ChoicePairs`, `Area169_Rects` (game-value), `Area172_EffectStates`,
  `Area196_Keys1`, `Scena02_EffectX`, `Scena03_Bob2`, `_Bob4`,
  `Scena04_Quake`, `_TremorSteps`, `GameMode3_Steps`, `LinkedObjectD_Amounts`,
  `Area104_PanelTris`' two trailing bytes; and **`MasterWin_Requirements+0x44`**
  (P1 item 8): JP and both PSPs place it and no Western disc holds the PC's
  bytes, so the Western discs' own list is read from the Western master
  module's code. A regional value, and a ledger entry (what the PC asks,
  what the Western disc asks, which an install gets).
- **OF-E, effects, magic and bosses:** `EffectKind2A_States` (P2: it moves
  the party) and `EffectKind86_States` (P2) first, then `EffectKind0D`,
  `27`, `29`, `65_ShakeSteps`, `6D`, `75`, `9A_Branches`, the `EffectKind18Sub*`
  rows (`1C`, `1D`, `21`, `2E`, `2F`, `30`, `31`, `32`, `33`, `37`, `38`,
  `45`, `64`; where one disc holds a twin, EU for `2E` / `2F`, US / FR for
  `30`, FR for `32`, that disc's row is TW's shape and the rest this),
  `ChlorineCloud_Offsets`, `SnapWave_FacingPhase`, `AirRaidImage_ShadeSteps`,
  `Magic020Slash_Animations`, `WardMote_Tilts`, `BoneDanceShake_Counts`,
  `TornadoMote_Phases` (the remainder after D278), `ReviveHalo_MoteCounts`,
  `Magic087_RisePhases`, `WhelpBreath_GlowAngles`, `InkPuff_Offsets`,
  `InkInkPuff_Offsets`, `AuraBreathDome_Height`, `BreathMote_Angles`,
  `BossDolphin_Sounds`, `BossGaist_Anims`, `BossTorch_Sounds`,
  `CommuMusic_CloseSteps`' game-value part (or, for `EffectKind0D_States`, a
  rule from the fishing module, the census's alternative).

For each row: the reader (census 4), its PSX twin (OT's fed-back pairs, or
found by the caller's constants per [`psx-twin-check.md`](psx-twin-check.md):
GCC folds small compares, so search by what calls it, not by its own
values), the immediates that rebuild the bytes, an `imm` row. A row whose
twin cannot be found stays NEEDS FIX with that said; a row whose PSX twin
holds *different* values is a finding, not a fix (rule 6: the PSX decides
what was; the ledger decides what should be).

Also in stage B: **PB's `TWINS` rows** (after TW), and **RD's hand-offs**.

### 3a. Stage B as run (re-planned 2026-10-18 after stage A)

Stage A merged on `install-gap/round` (RD, PB, OT, TW, LA, RW; the recipes
regenerated after each tool change). Three of its measurements changed this
stage's shape:

- **OT** ([`exe-immediate-places.md`](exe-immediate-places.md)): the P3 rows
  are not folded into instructions; a twin's `lui` / `%lo` pair addresses the
  bytes in its own overlay (`immref`). Only 11 of 66 P3 rows have a reader with
  a known PSX twin, and **no effect, magic, boss or scenario row** does: their
  readers are effect-state functions no code-pointer word pairs. OF-A and OF-E
  as staged would place almost nothing.
- **RW** ([`install-gap-reads.md`](install-gap-reads.md)): ten gap rows are read
  in play while zero; after the merges two stay open and need a source
  (`ChlorineCloud_Offsets`, the PSPs' `Char_WhelpSlot+0x3`), one needs a reader
  extension (below), two are the owner's.
- **PB's `boot_place`** refuses `Char_TraitLists`' list 1 (RD's
  `ConfigMenu_Steps+0xC`): the one `BOOT.BIN` copy is reached through a
  relocated pointer table, not a direct pair.

So stage B runs as three groups by file set:

| Group | Work | Owns |
|---|---|---|
| PT | `boot_place` through a relocated pointer table (the trait list); DE `0x663660..0x663668` as a twin (LA); `LANGUAGE_ZERO` and `exe_own.py`'s language regions per build (LA's rows reached on Latin builds, still zero under ja-JP); TW's `0x6543E8` place row checked; `MagicBallOrb_Shades+0x10` | `tools/psp_exe.py`, `tools/exe_tables.py`, `tools/exe_own.py` |
| CW | the two open live rows' sources, by the PSX-twin method ([`psx-twin-check.md`](psx-twin-check.md)); rows handed to PT or the coordinator, never written into PT's files | `docs/` only |
| FP | effect / magic / boss state functions paired with PSX twins by another method (call graph from paired callers, handler tables, constants), fed to `symbols.toml` `psx` fields; then the P3 rows those pairs reach placed as `immref` through `exe-imm.tsv`'s generator | `symbols.toml` `psx` fields, `tools/exe_imm.py` |

`ChlorineCloud_Offsets` (read in the Nue fight on four installs) is CW's first
row and FP's first family. The end batch (section 5) follows stage B.

## 4. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/psp_exe.py` `TWINS` list | TW (PB appends in stage B) |
| `tools/psp_exe.py` ELF reader | PB |
| `tools/exe_tables.py` (the `imm` kind), `tools/exe_own.py` | OT |
| `symbols.toml` `psx` fields | OT in stage A; OF-A / OF-E by family in stage B, resolved by entry keyed on `pc`, then a `tomllib` check |
| `tools/loc_build.py`, `src/game/labels.cpp`, the fishing-rule module | LA |
| `src/hook`'s gap watch (`BOF3X_GAPWATCH`) | RW |
| `recipes/exe*.tsv`, `recipes/exe.toml`, `recipes/exe-psp.tsv` | the coordinator only, regenerated after each merge |
| `docs/install-gap-census.md` section 8's statuses | the coordinator, from each hand-back report |
| `docs/README.md` rows | each group adds its own; keep both sides at a merge |

Merge order: **OT** first (stage B reads its kind), then TW, LA, RD, PB's
reader, in any order; then OF-A and OF-E. After every merge: the regeneration
chain (section 5), the build, `BOF3X_SHADOW='*'` headless, reading the
output.

Numbering: groups draft lettered ledger entries (A, B, ...) and describe
defects without numbers. The coordinator numbers at the merges from the
first free entry, **the one after `DIV-0141`** today (the button unification round took
DIV-0133..0141 and D312..D315, 2026-10-17). Defects number from the first
free after D315. Every commit `git commit -s`; no game data in any doc: ranges,
symbol names, counts and file names only (rule 1).

## 5. The end batch (the coordinator; live, unattended)

1. Regenerate over the seven discs: `psp_exe.py recipe`, `exe_tables.py
   recipe`, `exe_own.py measure`, `exe_tables.py gaps`, `exe_tables.py
   recipe` (the order [`exe-import-engine.md`](exe-import-engine.md) 6 and
   HANDOFF 4 give); then `exe_tables.py check`, `psp_exe.py check`,
   `importer.py check`, `tables.py check`, `ledger_check.py`, 0 errors each.
   Stop a regeneration only by the exact PIDs of its `python` children
   (Traps).
2. `tools/exe_scan.py` on each one-source image, against the census's
   section 2 totals as the baseline: the NEEDS FIX rows and bytes per build
   before and after, in the census's own table.
3. Rebuild the A/B/C caches (`abc_run.py prepare`, then `exe_tables.py
   gate-skip` again, Traps), and run: the attract from a US disc-only
   install against the PC-plus-US install under the gate (`statehash.py
   diff`), the eleven hash routes against the `cn_*_orig2` pairs with
   `BOF3X_OPT=none BOF3X_LAYERING=0`, and the headless self-test from each
   install kind. A difference a fix explains is its evidence; one it does
   not is a finding.
4. One capture per row that draws something (area 115's legend frame, the
   fishing kinds, the FR / DE pages, area 169's panel, the master's list on
   a Western install), through the existing routes or a chapter-jump save,
   sent to the owner by chat with the question each asks.
5. The census's section 8 rewritten with the statuses; HANDOFF item 4
   replaced; the drafts numbered.
6. **`BOF3X_EXEIMAGE_STRICT=1` as the default for a disc-only install** once
   RW's list shows no range read in play still zero (I43's step 4): a new
   gap then fails loudly at start instead of drawing wrong. A ledger entry
   (DIV-0101 amended); the launcher's message names the range and says to
   rebuild. Not before that condition is measured.

## 6. The live end batch as run (2026-10-18 evening)

Items 3 and 4 of section 5, run unattended at the owner's request on
`install-gap/live`, cut from `e5c5cad7` (the tip of `install-gap/round`:
both stages, the end docs, `origin/main` merged in, so DIV-0134, DIV-0135
and DIV-0143 are in the build). Built in the worktree's own `build/` (llvm-mingw,
886 steps, no error). Roots, new and this batch's only:
`C:\Users\kerok\g_abc` (caches, installs, the harness's `L\`, captures) and
`C:\Users\kerok\g_hash` (a copy of the owner's PC install without
`CDImage\`, the launcher copies `L1\` and `L2\`, the hash files `sh\`). The
main checkout's `bof3\`, `analysis/statehash/` and `tools/recipe_saves/`
were read only. Another live agent (RW2, root `g_rw2`) and the session on
`ab88` ran games beside these all evening: the machine was not quiet.

**Defender** stopped nothing of this batch. Every launcher copy here was a
plain `cp` of `build/` (`g_hash\L1`, `L2`, and `abc_run.py prepare`'s
`g_abc\L`) and none was quarantined: `Get-WinEvent` "Microsoft-Windows-Windows
Defender/Operational" ids 1116 / 1117 at 17:05 and 18:25 hold no path under
`g_abc`, `g_hash` or the worktree. The same log has 13 detections that
evening (17:37..17:41, `Behavior:Win32/DefenseEvasion.A!ml`), all of them
RW2's copies under `g_rw2`. Every result below is read from its own log
(`self-test only: done`, `recipe done`, the runner's `done:` line, the
`inject:` line), not from an exit code.

### 6.1 Self-tests (headless)

- **Hosted, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' BOF3X_LANG=original`**,
  from the ini-less `g_hash\L1` against the main checkout's `bof3\`: launcher
  exit 0, `inject: 10087 ours`, `self-test only: done`, **1,085
  `MISMATCHES` lines, every one ` 0 MISMATCHES`**, no `FATAL`. This is the
  `'*'` the round's merged branch had not had because of Defender.
- **The exe host from each install kind** (`BOF3X_SELFTEST_ONLY=1`,
  `BOF3X_CACHE` = the install's cache, `BOF3X_EXEIMAGE` too for the disc-only
  ones, the install's language, `BOF3X_OPT=none`, from `g_abc\L`): all eight
  `self-test only: done`, 0 `FATAL`.

  | Install | Language | `inject:` | Gap ranges logged |
  |---|---|--:|--:|
  | `pc-enus` (PC + the US disc) | en-US | 10,088 | - |
  | `psx-us` | en-US | 10,088 | 76 |
  | `psx-jp` | ja-JP | 10,087 | 58 |
  | `psx-eu` | en-150 | 10,088 | 75 |
  | `psx-fr` | fr-FR | 10,088 | 76 |
  | `psx-de` | de-DE | 10,088 | 78 |
  | `psp-jp` | ja-JP | 10,087 | 54 |
  | `psp-eu` | en-150 | 10,088 | 52 |

  **The 10,087 under `ja-JP`** (the coordinator's open note on PSP-JP)
  is by design: the one inject missing from both Japanese lists is
  `Config_DrawControllerCell`, which `config_text.cpp` injects only under a
  Latin overlay (D312's note there). The hosted run under `original` counts
  10,087 for the same reason.

### 6.2 The A/B/C caches and the attract gate

```
python tools/abc_run.py prepare --set all --only pc-enus,psx-us,psx-jp,psx-eu --root C:/Users/kerok/g_abc --installs C:/Users/kerok/g_abc/installs.toml --build build
python tools/abc_run.py prepare --set all --only psx-fr,psx-de,psp-jp,psp-eu  (the same otherwise, beside it)
python tools/exe_tables.py gate-skip --cache C:/Users/kerok/g_abc/c_psxus --game C:/Users/kerok/g_hash/bof3
python tools/abc_run.py run --set us --tag gate --route attract --statehash --statehash-skip C:/Users/kerok/g_abc/c_psxus/base/exe/gate-skip.txt  (--root, --installs as above)
```

`g_abc\installs.toml` is the shipped `recipes/abc-installs.toml` with the
discs read from `D:\bof3x-discs` (the main checkout's `CDImage\` holds zips)
and all seven one-source discs plus `pc-enus`, as RW's file had them. Every
cache was new, so every one was built: 192..286 s a cache, 4..9 s an install,
987 s and 1,109 s for the two halves. The gate list: the committed list plus
841 ranges, 44,559 bytes.

**The gate** (`pc-enus` against `psx-us`, `en-US` from the installs file,
`BOF3X_OPT=none BOF3X_LAYERING=0` from the harness, 11,000 recipe frames):
both sides `done at frame 11000`, 366 timed frames each, the scenes never
parted. **Pictures:** 305 of 366 frames equal; 61 differ, in two spans,
both the title screen: 420..1290 (matched by `pc-title-mark`, DIV-0114) and
6120..7020, the attract's second title, logged as mode 2 area 2, so the rule
(modes 0..1) did not match. Its regions are the copyright line
`20,208-300,224` and the `®` mark `260,128-272,140`, the same as the first
span (DIV-0114; D266 / D277 are the owner's calls). **State hash** (`diff
--align recipe`, 10,945 ticks): 96 pages. Outside the VRAM shadow and the
CLUT strip there are three:
- `0x903000` from tick 1 to the end: the nine pad words, **DIV-0134**. A
  US disc-only install starts with the West's buttons; its verification
  records this page from tick 1 on `psx-us`, and the attract loads no save.
- `0x802000` from tick 1310 (`DrawTable`, `Gfx_FrameNodes`): words
  pointing into area 4's loaded data, which differs between the PC's and
  the US disc's chunks. Read to the byte on 2026-10-13
  ([`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 7).
- `0x813000` (`Gfx_ClutStrip+0x3A80`) from tick 1310: the CLUT strip of
  the same data.

No difference here is the round's, and none is unexplained.

### 6.3 The state-hash routes

`BOF3X_OPT=none BOF3X_LAYERING=0`, `BOF3X_STATEHASH_SKIP` = the worktree's
173-range `tools/statehash_skip.txt`, `BOF3X_RECIPE_SAVES` = the main
checkout's, the game `g_hash\bof3` (a copy of the owner's install taken
at 17:06 with the owner's saves as they are now), `input_run.py
tools/recipes/R.txt --no-front` (shop `--slot0-hold`), the attract by
`attract_run.py --minutes 6 --no-front`. The script, `hash_route.sh`, is
in the session scratchpad (`.../scratchpad/live/`). Files are
`g_hash\sh\cn_<route>_<tag>.sh`, with the `.check` and `diff_*.txt`
beside them. Three passes:
- `live1`: ours, the exe host, from `L2`.
- `orig1`: Capcom's side once a route, `--original "*,-Game_Clock"`,
  hosted, foreground held, from `L1`.
- `off2`: ours hosted from `L2`, with the three divergences below switched
  off (`--original WorldMapLegend,AreaTrackFallback,AreaKeepStoppingNone`).
  It doubles as the repeat of `live1`.

| Route | `live1` against its pair | Capcom's side (`orig1`) against the pair | `off2` against `orig1` |
|---|---|---|---|
| attract (`attract_r14_*`) | **passes**: 8 pages, every one a platform page at tick 4 (the fresh-copy start-up trap), the sound page `0x7DE000` on 11 ticks and one VRAM page at 1311 | not run: the pair still holds | - |
| combat | stale pages, as Capcom's | stale: `0x929000` / `0x92A000` from 179, `0x905000` from 226, `0x803000` / `0x903000` / `0x904000` / `0x7E0000` 227..470, D3D pages | **identical on all 2,561 ticks** |
| shop | the same | the same, from 164 | `0x7CA000` only |
| worldMapAndAreaTransition | the same, plus 24 VRAM pages and `0x6C3000` from 1098 | the same, from 164 | `0x7CA000`; start-up noise at tick 3 |
| menu_screens | the same, plus the music pages from 753 | the same, from 500 | `0x7CA000` only |
| field_menu | the same, plus the music pages from 753 | the same, from 500 | `0x7CA000`; tick-3 noise |
| dragonTransform | the same, plus the music pages from 333 | the same, from 119 | `0x7CA000`; tick-3 noise |
| caughFish | the same, plus VRAM / draw pages 395..767 | the same, from 190 | `0x7CA000`; tick-3 noise |
| masterAndManillo | the same, plus VRAM / draw pages 324..3683 | the same, from 131 | `0x7CA000`; tick-3 noise |
| cutsceneAndNue | the same, plus the music pages from 205 | the same, from 95 | `0x7CA000`; tick-3 noise (7,361 ticks) |
| whelpBoss | the same, plus `0x6BD000` 10,347..12,070 | the same, from 119 | `0x7CA000` only (13,185 ticks) |

**The pairs are stale, as HANDOFF item 2 says, and on more than the save
pages.** Capcom's own side, run once a route this evening, differs from
every `cn_<route>_orig2` pair the same way ours does. It differs from each
route's first save-list read: `0x929000`, `0x92A000` and `0x905000` to the
end, and `0x803000` / `0x903000` / `0x904000` / `0x7E0000` over the load. The
owner's `bof3\` now holds one save slot (`BISLPS00.DAT`, 2026-10-17; the
others are in its `savesBackup\`), where the pairs staged the list of
2026-10-10. So the reading below is ours against Capcom's side of the same
hour, not against the pairs.

**Ours against Capcom's side, the same evening** (`diff orig1 live1`): the
game-state pages that remain are three ledgered divergences, all from
before this round. Switched off (`off2`), each is gone:
- **DIV-0092** (`AreaTrackFallback`): `Music_Track`, `Music_LoadedTrack`,
  `Music_File*`, the sound page, from the load. Seen on menu_screens,
  field_menu, dragonTransform and cutsceneAndNue.
- **DIV-0135** (`WorldMapLegend`): on a PC install with no PSX disc the
  engine paints the legend's key names over in VRAM and draws its
  stand-ins, so the VRAM shadow, `0x903000`, `0x929000`, `0x7E0000`
  (draw records) and `0x6C3000` (the texture cache) differ while a world
  map is up. Seen on worldMapAndAreaTransition (1098..1780), caughFish
  (395..767) and masterAndManillo (324..3683).
- **DIV-0143** (`AreaKeepStoppingNone`): whelpBoss's `0x6BD000`
  (`Music_File*`, `Music_FadeCount`) over 10,347..12,070, Ryu's dream.

**One page is Capcom's side's, not ours.** `0x7CA000` (`D3d_Vertices`,
`D3d_TexCoords`; Direct3D state the D3D11 renderer replaces) differs on
every tick of eight of the nine `orig1` runs, against the pairs as much
as against ours. On `combat_orig1`, the first, it never differs. Ours,
exe host and hosted, agrees with the pairs there. It is a platform page of
the all-original side's renderer, not game state; why it changed between
two Capcom runs of one evening was not read to the byte.

**The PC path, under the original language, shows no game-state
difference the round made, and none unexplained.** The round changes no
PC-install behaviour but LA's label groups under a Latin layer, and these
runs use none. The pairs are not re-recorded. If they are wanted, record
`orig1`'s like into `g_hash` twice (`orig1` alone is one side).

### 6.4 Captures (item 4), for the owner's eye

Under `C:\Users\kerok\g_abc\shots` (game-derived, outside the tree):

| Row | File | What is drawn |
|---|---|---|
| World map 115's legend frame (`Area115_Sprites`, `_Buttons`, TW) | `area115_legend_psx-us_t000480.png`, `area115_legend_pc-enus_t000480.png` | The frame at the top left, whole, with the compass, on the US disc-only install: circle `Enter`, cross `Guide`, `START` `Camp`. The PC install shows its pictograms beside the same shapes (DIV-0130 / DIV-0135, by source). The two differ in that box only (`68,20-124,52`) |
| The German equipment panel's slot icons (D319) | `de_equip_slot_icons_psx-de_t001770.png` | The shop's `AUSR` panel on `psx-de` (`shop`, recipe frame 1,770), each slot row with its icon (weapon, shield, helmet, armour, two accessories) |
| The PSP default light (PB) | `psp_light_psp-eu_shop_t000540.png` (the item shop), `psp_light_psp-eu_shop_t001290.png` (the street) | Area 7 on `psp-eu`, lit, the shading as on the other installs |
| The fishing kinds (JP) | `jp_fishing_psx-jp_t001080.png`, `jp_fishing_psx-jp_t001800_before_crash.png` | `caughFish` on `psx-jp`: the cast, the lure's name, the fight. The route does not reach the catch: it crashes first (below) |
| Area 169's dial panel (P1 item 7) | not captured | A crafted save puts the party in area 169 on both `us` installs (122 frames there, `pc-enus` and `psx-us` equal frame for frame), but the panel needs the dial's use, which no route or save here reaches. Skipped |

**How area 115 and area 169 were reached:** `ending17`'s method with only
the area word changed. `craft_area.py` (scratchpad) took the main
checkout's `dolphinBoss.DAT`, set the game block's `+0x24` to 115 (169),
and recomputed the `+0x70` checksum, after first checking the donor's own
checksum against the rule. The route was `ending17.txt` with its header
renamed (`g_abc\area115.txt`). The saves are in `g_abc\saves`, through a
second installs file whose `recipe_saves` names that folder. The load put
the party in area 115 on both installs, mode 2, from recipe frame 450.

**Not asked for here:** the FR / DE pages and the master's list on a Western
install (section 5 item 4's other rows) are not in this batch's brief.

### 6.5 Finding: D279 crashes `psx-jp` at the fishing spot, 2 runs of 2

`caughFish` on `psx-jp` (the A/B/C install, exe host, `ja-JP`) ended in
`FATAL` twice. Both times it was `CRASH 0: exception 0xC0000005 at
bof3x.dll+0x145328`, `area 0x001E ... mode 8`, reading about 0x1DC7xxxx.
The first run crashed after recipe frame 1,800 and the repeat after 960.
`llvm-symbolizer` on the run's `bof3x.dll` names the stack:
`Font_UnpackGlyph` (`tex_cells.cpp:240`) <- `Font_BuildGlyphTexture` <-
`Font_GlyphTexture` <- `D3d_DrawGlyph` <- `Gfx_DrawOTag` <- `Gfx_Present`.
The frame before the first crash shows the banner box empty. That is
**D279**: the Japanese fishing banner's NUL drawn as glyph `0xFFDA`, its
cell far past the glyph table. D279 recorded garbage on every PSX-JP visit
and a crash on PSP-JP one run in five; on this build PSX-JP crashes every
time. **No round change explains it.** No `ja-JP` layer writes the banner's
lines or its button label (CW's second path). The crash only needs the read
to land on unmapped memory, which depends on where the DLL and the heap sit.
Not measured: whether the round's base build crashes as often. D279's fix
is still the owner's call ([`owner-review.md`](owner-review.md)). Until
then a Japanese disc-only install cannot fish.
_(2026-10-21) Fixed the same evening, 2026-10-18, by DIV-0144 and
DIV-0069's amendment on `install-gap/d279` (the owner's call: both paths),
merged with the round as PR #91 ([`known-defects.md`](known-defects.md)
D279)._

### 6.6 What this leaves

- **Item 6 (STRICT as the default)** is RW2's measurement, not this batch's.
- **The nine stale pairs plus Nue's** (HANDOFF item 2): re-recording wants
  a quiet machine and a fixed save list. Tonight's comparison went around
  them by running Capcom's side the same hour.
- **D279 on `psx-jp`** (6.5): every time on this build. _(2026-10-21) fixed
  2026-10-18 by DIV-0144, merged with PR #91._
- **The roots** `g_abc` (about 7 GB of caches and installs) and `g_hash`
  are scratch. Delete them once this branch has merged.

## 7. What stays the owner's (not in this round)

The 13 OWNER CALL items (12 since 2026-10-17: the event battles' track
words were filled by rule after issue #83, D316): DIV-0055's plates
(the nine `port-relaid` rows), D266's Western title mark, `Scena17_EndSteps`
(the Western ending), the empty save slot `0x669CCC` (the staged
localisation PR), D257, D246, D277, D256, D280, and the US button
assignment (now the button round, [`button-unification-round.md`](button-unification-round.md)).
What the round can do for them while the owner is away: one capture or one
table per item, so each question in chat is answerable from a phone.
_(2026-10-21) Since: D266 built 2026-10-20 (the `titlemark` rule) and the
Western ending answered and built the same day (DIV-0148), both PR #98;
the rest are tracked in [`exe-gap-round.md`](exe-gap-round.md) 3._

## 8. The brief, per group

Each brief is written from this document and says, in its own words: the
goal; the rows (by symbol and range, copied from the census, never the
bytes); the files it owns and the files it must not touch; the docs to read
first ([`install-gap-census.md`](install-gap-census.md) 1 and 4,
[`exe-image-twins-1013.md`](exe-image-twins-1013.md) 1-2,
[`exe-import-engine.md`](exe-import-engine.md) 3.3 and 6,
[`psx-twin-check.md`](psx-twin-check.md), the owner's stance above); its
harness root; the checks that mean done; the hand-back report's shape (what
was placed, what was not and why, the drafts lettered, the regenerations it
did *not* commit). Rules 1, 4 and 7 are restated in every brief. A usage cut
resumes with a message naming the next step; only the hand-back report means
merge.

## 9. Not decided here

- Whether the `imm` place kind should also replace any existing engine rule
  that holds a disc's values (none known today; OT reports if it meets one).
- The order of OF-A against OF-E if only one can run: OF-E first, since
  `EffectKind2A_States` moves the party (a gameplay effect) and the rest of
  OF-A is visual.
- Whether to run the end batch once (after stage B) or twice (after stage A
  too). Twice if stage B waits a day; the batch is about an hour of machine
  time.
