# The game-value gaps filled from every disc

**Status:** MEASURED (2026-10-12, the PSP-only round's group IM, branch
`psp/im2` from the round tip `958b01b9`; the seven held discs and `BOF3.exe`;
headless self-tests only, **not played**)

The owner, 2026-10-12, on [`psp-image.md`](psp-image.md)'s finding that the
four largest game-value gaps are on every disc: "let's go ahead and fill
them". [`psp-image.md`](psp-image.md) filled them for the two PSP builds;
this doc fills them for the five PSX builds, by the same rules from each
PSX disc's own files. No table value is in the tree (CLAUDE.md rule 1).

## 0. What came out

- **The PSX id table, by structure** (`psp_exe.lba_table`): the boot EXE
  holds one u32 start sector per disc file. It is found as the longest run
  of aligned words in the boot EXE each of which is the start sector of a
  file in the disc's own directory, and each sector is named by its file.
  It has 887 ids on the JP and US discs and 889 on the three PAL discs. On
  JP it equals the sibling's `analysis/file_ids.json` record for record,
  887 of 887. The id rule (`psp-image.md` 3) then gives 752 PC file ids on
  every build.
- **The four fills on every PSX build**, each row kept only when every byte
  it writes equals `BOF3.exe`'s. Every row was kept, 0 bytes off the PC's:

| Rule | What | PSX source (JP / the others) | Bytes |
|---|---|---|---:|
| `rows8` | `Magic_Rows`' effect files | `BATTLE/BATTLE.EMI#15`, the one run of 151 stride-8 rows with a main-RAM code address at +4 (`0x800B3538` / `0x800B65F8`) | 604 |
| `pairs` | `EventBattle_Records`' battle files | `ETC/GAME.EMI#0`, the catalogued record table + 56 x 4 | 82 |
| `holes` | the dragon set-file tables | `BATTLE/BATTLE.EMI#15`, the hole in the map's segment (`0x800B19AC` / `0x800B4A6C`) | 528 |
| `widen8` / `widen10` | the formation wheel's spots | `ETC/BATE.EMI#0` `0x801D0C50`. The disc's name is 8 bytes on JP, US, FR and Europe-English, and **10 on the German disc** (found by the stride; the rule is now `widen<k>`) | 120 |

The `rows8` finder on a PSX disc has no relocations to read. It looks only in
the file `exe_maps` places `Effect_HpDamageVariance` from (the battle
engine's), and requires exactly one run of exactly 151 rows.

## 1. The gaps per build, before and after

`recipes/exe-gaps.tsv` and `recipes/exe.toml`, regenerated (`exe_tables.py
recipe`, `exe_own.py measure`, `exe_tables.py gaps` over the seven discs):

| Build | gap ranges | gap bytes (rows) | game-value, PC non-zero | identical to the PC's |
|---|---|---|---|---|
| `psx-jp` | 435 to **333** | 4,635 to 3,309 | 1,653 to **793** | 612,863 to 613,723 |
| `psx-us` | 456 to **354** | 5,455 to 4,129 | 2,196 to **1,336** | 612,176 to 613,036 |
| `psx-eu-en` | 454 to 352 | 5,439 to 4,113 | 2,192 to 1,332 | 612,191 to 613,051 |
| `psx-fr` | 456 to 354 | 5,527 to 4,201 | 2,223 to 1,363 | 612,125 to 612,985 |
| `psx-de` | 457 to 355 | 5,543 to 4,217 | 2,224 to 1,364 | 612,112 to 612,972 |
| `psp-jp`, `psp-eu` | unchanged (319, 352) | unchanged | unchanged (744, 800) | unchanged |

Every PSX build loses 102 gap ranges and 1,326 range bytes, and gains 860
bytes equal to the PC's. Of the other recipe files, only `exe-psp.tsv`
changed (20 PSX rows added; the file keeps its name, and its header says it
now holds every disc build's fills). `exe-pointers.tsv`, `exe-places.tsv`,
`exe-layout.tsv` and `exe-rebuild.tsv` are unchanged. The two PSP sections of
`exe.toml` are unchanged.

What is left of the game-value gaps on a PSX build (US, 1,336 bytes) is the
long tail: the `EventBattle_Records` track half (the port's MP3 track
numbers), the formation names (language), the area and effect tables of
`exe-engine-own.md` 4.3, and on the Western discs `Scena17_EndSteps`. The
PSP-JP `BOOT.BIN` carries the last of these verbatim (`psp-image.md` 5.1);
it is not taken into a PSX build here.

### 1.1 After `main` was merged (2026-10-12, `psp/main-merge`)

`main`'s `GAME.EMI` place (the story-flag seed `0x662CD8` and `0x662DE8`,
[`exe-import-engine.md`](exe-import-engine.md) 7.4) and the four fills
together, measured with the merged tools (`exe_tables.py recipe`,
`psp_exe.py recipe`, `exe_own.py measure`, `exe_tables.py gaps`, the seven
discs). Every fill row still writes 0 bytes off `BOF3.exe`'s.

| Build | gap ranges | gap bytes (rows) | game-value, PC non-zero | identical to the PC's |
|---|---:|---:|---:|---:|
| `psx-jp` | 330 | 3,281 | 789 | 613,727 |
| `psx-us` | **351** | **4,085** | **1,323** | 613,040 |
| `psx-eu-en` | 349 | 4,069 | 1,319 | 613,055 |
| `psx-fr` | 351 | 4,157 | 1,350 | 612,989 |
| `psx-de` | 352 | 4,173 | 1,351 | 612,976 |
| `psp-jp` | 316 | 2,897 | 740 | 611,648 |
| `psp-eu` | 351 | 3,112 | 791 | 611,575 |

The PSX builds lose 3 ranges each to `main`'s place, and 13 game-value bytes
on the Western builds (4 on JP), as `disc-only-install.md` 3's
arithmetic said for US. **PSP-JP loses them too**: its
`psxmap` rows (89 runs now, from 87) read the two runs from the PSP-JP
disc's own `GAME.EMI` at PSX-JP's place. PSP-EU keeps both as gaps; its
`GAME.EMI` does not hold them there.

**Since VF's fills** (`twin`, `msets`, `unwide`,
[`visual-findings-1012.md`](visual-findings-1012.md) 5, on `6c2ae0c9`):
PSX-JP 307 ranges / 2,998 bytes / game-value 728; **PSX-US 329 / 3,819 /
1,262**; PSX-EU-en 327 / 3,803 / 1,258; FR 329 / 3,891 / 1,285; DE 329 /
3,887 / 1,286; PSP-JP 278 / 2,432 / 664; PSP-EU 311 / 2,612 / 709.

The merge's checks (headless, an ini-less copy of the merged launcher):
- **The exe host on the US image over the real install:** exit 0, `place
  649 ranges 6153 bytes`, `gap 351 ranges 4085 bytes`. Against `BOF3.exe`,
  `627996 of 638976 bytes equal; every class that must equal does`, and no
  `place` range differs.
- **The same on a US disc-only install** (`C:\\b3g`, `--lang en-US`): exit 0.
- **On a PSP-EU-only install** (`C:\\b3p`, `--lang en-150`): exit 0, `place
  527 / 5296`, `gap 351 / 3112`.
- **Caches built with `main`'s own tools (`035f40b6`) against the merged
  ones:** the JP + PC cache differs only in `manifest.toml`'s `[build]`
  table. The US and European caches differ in `base/exe/data.bin`,
  `base/exe/data.toml` and `manifest.toml` (the `[build]` table and
  `[exe]`'s sha256), and nothing else.

## 2. Checks

| Check | Result |
|---|---|
| `importer.py check`, `exe_tables.py check`, `psp_exe.py check` | 0 errors |
| `ledger_check.py` | 0 errors, 2 notes |
| JP + PC cache against `psp_coord/ref_jppc` | every file byte-identical except `manifest.toml`. Its `built =` differs, and so does the `[build]` table the round added after the reference was built (`base_policy`, `exe_tables_from = "pc-zh"`) |
| US-only cache against `ref_us`, European-only against `ref_eu` | differ in **`base/exe/data.bin` and `base/exe/data.toml`** only, plus `manifest.toml` (`built`, the round's `[build]` table, `[exe]`'s sha256) |
| `importer.py verify`, the new US cache | `base/exe/ (psx-us): the image recipes/exe.toml records for it`. The one problem it reports (`opt/area4-walls`, AREA004 slot 6 "composed with the layer, not the recipe's") is the same on `ref_us`: not this change |
| exe host self-test, the real install, `BOF3X_EXEIMAGE` = the US image | exit 0; `place 647 ranges 6129 bytes`, `gap 354 ranges 4129 bytes`; against `BOF3.exe`: `627983 of 638976 bytes equal; every class that must equal does` (627,123 before), **no `place` range differs**; `inject: 10087 ours` |
| exe host self-test, the scratch disc-only install (`disc-only-install.md` 1 and 5: `build --lang en-US`, `install --base` to `C:\b3g`, no `BOF3.exe`; `BOF3X_CACHE` = `BOF3X_EXEIMAGE` = that cache, `BOF3X_LANG=en-US`) | exit 0; the same `place` / `gap` counts; `no BOF3.exe to compare the disc-built image with`; `exe_own: 17 regions, 20866 bytes filled over a disc-built image`; `inject: 10088 ours`; `self-test only: done` |

## 3. Not verified

- **Not played.** No magic, event battle, dragon transformation or formation
  wheel was seen from a disc-only install. The bytes equal the PC's, so the
  game should behave as the PC does. The owner's play is the check.
- The scratch-install run cannot compare against `BOF3.exe` (it has none).
  The real-install run with the same image is the comparison.
- The PAL discs' and the US disc's id tables were not checked against an
  outside table (the sibling's is JP's). The fills' equality with the PC's
  bytes is their check.

## For the other files

**Folded in 2026-10-12** (the coordinator's pass, branch `psp/ledger`): DIV-0101's amendment, `disc-only-install.md` 3, the README row, STATUS, HANDOFF and owner-review are in.

Not edited here; the coordinator folds them in.

**DIV-0101, amended** (the disc-only round's gaps entry, provisional). Add
after its "New":
- Amended 2026-10-12 (the owner: "let's go ahead and fill them"; PSP-only
  round, group IM, [`disc-gap-fills.md`](disc-gap-fills.md),
  [`psp-image.md`](psp-image.md)): four of the gaps are no longer zero on any
  disc-built image. `Magic_Rows`' effect files, `Effect_HpDamageVariance`'s
  dragon set-file tables, `EventBattle_Records`' battle files and the
  formation wheel's spots are read from the disc's own tables:
  - a PSX disc's `BATTLE.EMI#15`, `GAME.EMI#0` and `BATE.EMI#0`;
  - a PSP disc's `BOOT.BIN` and `BATTLE.EMI#15`.

  The disc's file ids are turned into the PC's by the file-id rule (the
  disc's file table to path to `Dat_FileNames`' slot, all in the tree as
  addresses and names), and the wheel is widened from the disc's 8- or
  10-byte names. Every filled byte equals `BOF3.exe`'s, on all seven
  builds (`recipes/exe-psp.tsv`, kept only so). The image is the PC's in
  860 more bytes per build. Per PSX build: 102 fewer gap ranges, game-value
  bytes US 2,196 to 1,336, JP 1,653 to 793.
- Still zero: the event battles' track half (the port's own MP3 numbering),
  the formation names (language), and the rest of section 3's list.
- Verification: `psp_exe.py recipe` (0 bytes off the PC's on every row); the
  exe host's self-test over the real install (no `place` range differs from
  `BOF3.exe`) and on a scratch disc-only install (exit 0); not played.

**`docs/disc-only-install.md`** section 3: the table's first four rows are
filled now (this doc); the US counts become 354 gap ranges, 4,129 bytes,
game-value 1,336.

**`docs/STATUS.md` / `docs/HANDOFF.md`**: the four largest game-value gaps
are filled on every disc build; what is left per build is section 1's.

**`docs/README.md`**: a row for `disc-gap-fills.md`.

**`docs/owner-review.md`**: the gaps call is answered for these four (filled
by rule, on the owner's word). The rest stays open: the track half, the
names, and the long tail.
