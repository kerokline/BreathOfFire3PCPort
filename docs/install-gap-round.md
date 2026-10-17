# The install-gap round: staging

**Status:** DRAFT (staged 2026-10-17 against `origin/main` `8382691d`; nothing
launched, nothing built. The census it closes is
[`install-gap-census.md`](install-gap-census.md), verified the same day.)

The census gave every row of `recipes/exe-gaps.tsv` a status and left, in its
section 8, the NEEDS FIX items across the seven disc builds: two P1 items not
done, the P2 twins the disc search already locates, the P3 long tail and the
P4 "find a reader" rows. This document stages that remainder as parallel
groups that can run with no owner present. The 13 OWNER CALL items are not in
it (section 6).

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

## 2. Stage A (five groups, in parallel)

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

## 4. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/psp_exe.py` `TWINS` list | TW (PB appends in stage B) |
| `tools/psp_exe.py` ELF reader | PB |
| `tools/exe_tables.py` (the `imm` kind), `tools/exe_own.py` | OT |
| `symbols.toml` `psx` fields | OT in stage A; OF-A / OF-E by family in stage B, resolved by entry keyed on `pc`, then a `tomllib` check |
| `tools/loc_build.py`, `src/game/labels.cpp`, the fishing-rule module | LA |
| `recipes/exe*.tsv`, `recipes/exe.toml`, `recipes/exe-psp.tsv` | the coordinator only, regenerated after each merge |
| `docs/install-gap-census.md` section 8's statuses | the coordinator, from each hand-back report |
| `docs/README.md` rows | each group adds its own; keep both sides at a merge |

Merge order: **OT** first (stage B reads its kind), then TW, LA, RD, PB's
reader, in any order; then OF-A and OF-E. After every merge: the regeneration
chain (section 5), the build, `BOF3X_SHADOW='*'` headless, reading the
output.

Numbering: groups draft lettered ledger entries (A, B, ...) and describe
defects without numbers. The coordinator numbers at the merges from the
first free entry. **`DIV-0131` is taken** (the PSP-only title, PR #81); the
fishing-panel duck in flight on 2026-10-17 also labels itself `DIV-0131` and
must be renumbered before it lands. Defects number from the first free
after D311. Every commit `git commit -s`; no game data in any doc: ranges,
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

## 6. What stays the owner's (not in this round)

The 13 OWNER CALL items, unchanged: the event battles' track words
(DIV-0101 provisional; the census's first owner item), DIV-0055's plates
(the nine `port-relaid` rows), D266's Western title mark, `Scena17_EndSteps`
(the Western ending), the empty save slot `0x669CCC` (the staged
localisation PR), D257, D246, D277, D256, D280, and the US button
assignment (now the button round, [`button-unification-round.md`](button-unification-round.md)).
What the round can do for them while the owner is away: one capture or one
table per item, so each question in chat is answerable from a phone.

## 7. The brief, per group

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

## 8. Not decided here

- Whether the `imm` place kind should also replace any existing engine rule
  that holds a disc's values (none known today; OT reports if it meets one).
- The order of OF-A against OF-E if only one can run: OF-E first, since
  `EffectKind2A_States` moves the party (a gameplay effect) and the rest of
  OF-A is visual.
- Whether to run the end batch once (after stage B) or twice (after stage A
  too). Twice if stage B waits a day; the batch is about an hour of machine
  time.
