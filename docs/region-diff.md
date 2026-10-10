# Region diff: every held disc against the JP disc, section by section

**Status:** MEASURED (2026-10-06). The data side is settled to the row; the
code side is counted, not read (section 6); four PSP leads are matched to
data rows that were not rendered, so they stay candidates (section 5).
Section 8 (added the same day) reads the four Western data changes through
the code that consumes them: two are one collision fix, one is a sound
priority made consistent, one is a PAL-only sample swap of unknown purpose.
None has been seen in play. Section 10 is the collision fix: built the same
day as a coordinate table in our code, and since 2026-10-10 the
`area4-walls` optional layer from the player's own Western disc, after a
measured search for a rule found none (10.1). Checked offline against the US,
European, French and German discs; live check owed to the owner.

The phase 4 measurement of [`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 8
("do the regional builds differ beyond their text?"), with the two PSP discs
put through the same diff for [`IDEAS.md`](IDEAS.md) I32. It answers whether
the asset cache's `base/` can be one tree (ASSET_SOURCES section 3), what the
PSP changed in its data, and what the importer must transform per build.

No content is reproduced here. The tables hold counts, section indices,
destinations, sizes and offsets; the "what it is" columns name things, they do
not quote them (CLAUDE.md rule 1). The per-section rows, with truncated
hashes, are in `analysis/region/` (gitignored).

## 1. What is held, and against what files

All six images are in the main checkout's gitignored `CDImage/`. Every one
was hashed whole and file by file on 2026-10-06; the results are now rows of
[`fixtures.toml`](../fixtures.toml), and `tools/verify_fixtures.py --tree`
identifies a disc or a `DAT/` tree from them (it names `psx-us` and `psp-eu`
from their images, and `pc-zh:dat_tree` from `bof3/DAT`, all files matching).

| build | file(s) | sha256 (image) | boot EXE | files |
|---|---|---|---|---:|
| `psx-jp` | `Breath of Fire III (Japan).bin` (+ `.cue`) | `19fbbedd…0a69e` (sha1 and md5 equal to the row already there) | `SLPS_009.90`, sha256 `61ade0b3…` as recorded | 887 |
| `psx-us` | `Breath of Fire III (USA).bin` (+ `.cue`) | `beade306…1baf` | `SLUS_004.22` | 887 |
| `psx-fr` | `Breath of Fire III (France) (Track 1).bin`, `(Track 2).bin` | `79aebcae…be686`, `ce5509fa…0c77` | `SLES_013.19` | 889 |
| `psx-de` | `Breath of Fire III (Germany) (Track 1).bin`, `(Track 2).bin` | `2835767c…8304`, `ce5509fa…0c77` (the same audio track as FR) | `SLES_013.20` | 889 |
| `psp-jp` | `Breath of Fire III (Japan).iso` | `861c0209…dcd6`, as recorded | `PSP_GAME/SYSDIR/BOOT.BIN` | 1,846 |
| `psp-eu` | `Breath of Fire III (PSP) (Europe).iso` | `216ac26c…1e82`, as recorded (the row's file name differs; the hash does not) | `PSP_GAME/SYSDIR/BOOT.BIN` | 1,837 |
| `pc-zh` | `bof3/DAT/`, the 742 shipped containers | per file only | `BOF3.exe`, as recorded | 742 |

`fixtures.toml`'s "not held" notes for the Western PSX discs were stale:
the US image has been in `CDImage/` since 2026-08-29 (`loc_build.py` reads
it), and France and Germany are there too. **`psx-eu-en` is the one release
not held**, so the `DEMO.EMI` section 5 anomaly the sibling flagged is not
re-measured here. The boot EXE names agree with the sibling's
`regional-builds.md` serials, and the US/FR/DE trees have the file counts it
gives (887, 889, 889; the two extra PAL files are `ETC/LOAD.EMI` and
`ETC/WARNING.EMI`).

The `DAT/` tree's per-file hashes are in `fixtures/pc-zh.DAT.files.tsv`. An
install also holds our language overlays (`<lang>.<NAME>.DAT`, 980 of them
here); the artifact's `match` excludes them. The 742 files' chunk census is
`DAT_CONTAINER.md`'s exactly (2,087 data, 593 image, 901 bank, 1 kind 3) and
none is newer than 2001-04-18, which is why they are taken as shipped.

## 2. The tool

[`tools/region_diff.py`](../tools/region_diff.py):

- `pair A=DISC B=DISC --out J` pairs every EMI of one build with the other's
  by its path under the data root (`BIN/` on a PSX disc,
  `PSP_GAME/USRDIR/<region>/` on a PSP one), then aligns each pair's sections
  **by content, order-preserving, never by address**: a dynamic programme over
  both section lists, skips allowed on either side, a pair legal only between
  sections of the same type, scored identical content 8, equal size 2, equal
  destination 1 (destinations move between regions, so they only break ties).
  This is `dat_census.align`'s rule with skips on both sides.
- `pc` runs the classifier over the census's own pairing
  (`analysis/dat_census.json`), so the PC sits in the same table without the
  census being redone.
- `files` writes a per-file manifest; `table` prints this page's summary rows.

A pair JSON per pair is in `analysis/region/<a>_vs_<b>.json` (indices,
destinations, sizes, classes, offsets of differing bytes, 16-hex hashes).
Eight disc pairs plus the PC took about ten minutes each, run in parallel.

The JP-US pair reproduces the sibling's census exactly: **6,344 sections on
each side, 5,372 identical, 972 different**.

## 3. The classifier, and how it decides

Every differing pair gets one class. In order:

| class | decided by |
|---|---|
| **text** | where a language lives, known before this measurement: the area message block (`0x80010000`), the system pool (`0x80014000` JP layout, `0x8001A000` Western), the enemy table's name fields (`0x800E4000`: the 8-byte names differ and **every other byte is equal**, checked record by record), the language-bearing image destinations (glyph atlas `0x1C080200`, kanji sheet `0x1E000200`, area pages `0x0E001000` / `0x0A081000`, `DEMO`'s `0x1A080400`, the title menu `0x1C000200`), and `loc_build.py`'s world-map plate data in its ten `PLATE_AREAS`. Only between two languages: within one language the same sections are an *edit* (blocks, pools) or *art* (images). |
| **layout** | code whose every differing word is a relocation once address fields are masked (J/JAL targets, `lui`, base-register loads, stores and `addiu`/`ori`, pointer words in `0x80000000..0x80200000`), with no inserted or deleted words; data whose only differences are pointer words; compression changed. A section is code if it is bound for an overlay band (`0x801F2C00`, `0x801EEC00`, `0x801D0C00`, `0x800C1800`, `0x801F6C00`, `0x801CE400`, `0x800F5000`, `0x80117000`, the boot band `0x80093800` / `0x80096800`, `GAME.EMI`'s `0x80196800` / `0x80195800`) or passes the sibling's `jr ra` / prologue density test. `0x80104000` is deliberately not a code band: it is data (the sibling's `OVERLAYS.md`). |
| **layout+text** | the PC's enemy tables: names widened 8 to 12, stats equal. |
| **logic-code** | code that differs beyond relocation: `addiu`/`ori`/`slti` from `$zero` or `andi`/`xori` immediates changed ("value"), branch offsets changed, words inserted or deleted (aligned by `difflib` over the masked words). |
| **logic-data** | everything else that is data: numbers, sound cue entries, sound banks, map data. |
| **art** | an image or a CLUT section (type 0, `0x8002B000..0x80037000`, whole 32-byte rows: the sibling's `TEXT_TABLES.md` reads `0x8002BE00` as twelve 256-entry CLUTs) that differs and is not language. |
| **converted** | the PSP's own formats (section 5.1): audio re-authored, type-1 arenas shipped decompressed, PSP-only sections. |
| **identity** | the save file name's product code. |

Rows the rules cannot settle are read by hand and recorded in `HAND` in the
tool, each keeping its automatic class as `auto_class`: the memory-card
module's product code (`START`/`SHOP` `0x801EEC00`, identity), the two test
modules `MTEST`/`RTEST` (byte-for-byte re-encoded labels, text), the PC's
`FIRST` CLUT strip (row 0 brightened for the port's glyphs, `loc_build.py`
DIV-0013, text), and `FIRST`'s PSP-only sections.

## 4. The PSX pairs

| pair | sections | identical | text | layout | logic-data | logic-code | art | identity | unpaired |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| psx-jp vs psx-us | 6,344 | 5,372 | 378 | 309 | 67 | 216 | 0 | 2 | 0 |
| psx-jp vs psx-fr | 6,344 | 5,336 | 394 | 295 | 83 | 230 | 4 | 2 | 0 |
| psx-jp vs psx-de | 6,344 | 5,322 | 396 | 297 | 83 | 228 | 16 | 2 | 0 |
| psx-us vs psx-fr | 6,344 | 5,428 | 377 | 359 | 16 | 158 | 4 | 2 | 0 |
| psx-us vs psx-de | 6,344 | 5,423 | 370 | 358 | 16 | 159 | 16 | 2 | 0 |

(The PAL discs' two extra files, `LOAD.EMI` and `WARNING.EMI`, have no JP or
US counterpart and are outside the counts.)

JP-US's 378 text rows: 168 area message blocks, 106 enemy tables (names
only), 44 system pools, 21 plate-data sections, 21 area pages, 13 glyph
atlases, 2 kanji sheets, 2 test modules, `DEMO`'s page. Its 309 layout rows
are all overlay code that differs only by relocation.

### 4.1 Every logic-data row of the PSX pairs

| rows | file(s) | section | size | what it is | how big | in which builds |
|---:|---|---|---:|---|---|---|
| 1 | `WORLD00/AREA004.EMI` (Dauna Mine, minecart - the sibling's `areas.toml`) | 8, `0x80104000` | 60,280 | the area's data band (not code: no `jr ra`, no prologue) | 992 bytes in 802 runs from `+0x7F33` to `+0xE4CB`; a field set at a 90-byte stride, and many bytes stepped by one (index-like) | US, FR, DE **and both PSP discs**; JP and the PC have the original |
| 65 | `BPLCHAR/` `DRG*`, `RYUD*`, `RYUU*`, `CRYUD*`, `CRYUU*`, `REID*`, `REIU*`, `RTD*`, `RTU*` | 1, type 8 | 24 | a sound bank's cue entries (the sibling's `EMI_TYPES.md`: 4 bytes per cue) | 1 byte, offset 6 (the second cue's tone byte), stepped by one | US, FR, DE **and both PSP discs** |
| 1 | `WORLD00/AREA004.EMI` | 10, `0x8002A000` (`0x80032000` Western) | 5,000 | unidentified data (the sibling's `XREF.md`: "pre-text, unknown") | 6 bytes | US, FR, DE only: the PSP discs keep JP's |
| 16 | `WORLD02/AREA100`, `AREA112`, `WORLD03/AREA135`, `AREA138`, `AREA147`, `WORLD04/AREA168`, `AREA170`, `AREA188` | 0 and 2, VH and VB | 3,616-4,128 / 90-188 KB | the area's sound bank | the VH's sample count is one lower (three in `AREA188`), the tone count unchanged; the VB is 15-44 KB shorter | FR and DE only (byte-identical between them) |

That is the whole list: **83 data rows across the three Western discs, in
four groups**, of which two groups are also in the PSP and none is in the PC.

### 4.2 Art rows (not language, not logic)

- FR and DE: the three fishing areas' `0x8002BE00` CLUTs (`AREA030`,
  `AREA089`, `AREA129`; 8 rows each on DE).
- FR only: `DEMO`'s `0x8002C000` CLUT.
- DE only: `SCENA17`'s `0x8002D200` CLUT (16 rows), and twelve spell modules (`BMAGIC/MAGIC008, 013, 038, 039, 040, 043,
  062, 069, 082, 083, 088, 225`), two rows of their `0x8002EC00` CLUT.

### 4.3 The code rows

216 code sections differ JP-US beyond relocation, 116 distinct by content
(the battle engine's two sections ship in 40 `BOSS` files each). By band:
57 area overlays at `0x801F2C00` (130 changed words in all), the battle
engine at `0x801D0C00` and `0x80093800`, 18 `PLP` overlays (one `slti`
constant each), 12 scenario overlays, the menu modules (`START`, `STATUS`,
`SHOP`, `SHISU`, `SISYOU`, `BATE`, `COMMU*`), and `GAME.EMI`. Read by hand
in samples (section 6). These are code, not data: they do not enter the
cache, which is why they do not decide section 3 of ASSET_SOURCES.

## 5. The PSP pairs

| pair | sections | identical | text | logic-data | art | converted | layout / logic-code | unpaired |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| psx-jp vs psp-jp | 6,346 | 3,760 | 53 | 85 | 220 | 2,225 | 0 / 0 | 1 + 2 |
| psp-jp vs psp-eu | 6,375 | 5,994 | 379 | 0 | 2 | 0 | 0 / 0 | 0 |
| psx-us vs psp-eu | 6,346 | 3,261 | 89 | 20 | 221 | 2,225 | 309 / 216 | 1 + 2 |

**The PSP's EMIs carry the JP build's code, byte for byte.** psx-jp vs
psp-jp has no code row at all; psx-us vs psp-eu has exactly JP-US's 309 +
216 + 2. And **psp-eu differs from psp-jp in its language sections and
nothing else** (379 text, one image tile each in `DEMO` and `TURISHAR`). Capcom's own
English PSP release is one base with a language layer over it - the shape
ASSET_SOURCES section 3 proposes, already practised.

**Not in these rows** (they compare EMI sections; the PSP's ELF carries more,
[`exe-tables-by-build.md`](exe-tables-by-build.md) 4.4): both PSP ELFs rebalance roster index 6's level
table (249 bytes of EXP and stat gains in its 99 rows, learnt abilities
untouched), and PSP-JP renames key items 2, 5, 7 and 9 and ability 116.

Five files are PSP-only: `ETC/CAPLOGO.EMI`, `TMBGM000.EMI`, `TMBGM001.EMI`,
`TURIMODE.EMI`, `TURISHAR.EMI`.

### 5.1 What "converted" is

- **Audio.** Every VH (type 6) becomes a `PPHD` block, every sequence
  (type 10) a `pPMS` block, every VB (type 7) a `pBVC` block. Of the VBs,
  **every PSX sample body sits verbatim inside the PSP's**, after a header of
  32 to 224 bytes (1,019 sections checked; one file's section count differs).
  The samples are the PSX's; the instrument and sequence data are in
  formats nothing here reads.
- **Type-1 arenas.** All 65 ship decompressed on the PSP: the PSX section's
  first word is the PSP section's size in 65 of 65. **37 of them are
  byte-identical to the PC's decompressed chunk** - a ready test oracle for
  the type-1 decompressor `DAT_CONTAINER.md` section 4 still lacks. The rest
  differ by the PSP's own changes below, or are the eight `PL` arenas the PC
  ships `0x160` short.
- **Headers.** The PSP clears bit 31 of every RAM destination and sets bit 0
  of some image words; `region_diff.nd` undoes both before classifying.
- `FIRST.EMI` drops one `0x1A080400` image and gains a `0x1A080200` image and
  two sections bound for PSP RAM (`0x00596000`, `0x00600000`).

### 5.2 Every content row of psp-jp against psx-jp

| rows | file(s) | section | what | size of change | I32 lead |
|---:|---|---|---|---|---|
| 1 | `AREA004` | 8, `0x80104000` | as section 4.1: the Western change | 992 bytes | none (Western) |
| 65 | the dragon and Ryu banks | 1, type 8 | as section 4.1: the Western cue byte | 1 byte | none (Western) |
| 11 | `AREA005`, `AREA010`, `AREA011`, `AREA012`; `AREA030`, `AREA089`, `AREA129`; `AREA052`; `AREA128`; `AREA142`; `AREA155` | `0x80104000` | the areas' data band | 5 bytes between `+0xADA8` and `+0xAFC3` in the first four (`AREA005`, `010`, `011` are the Cedar Woods treehouse in spring, in fall and burning, by `areas.toml`; `AREA012` is unnamed there); 12 contiguous bytes at `+0x4200` in the three fishing areas; 15 in the dump site; 304 in the dock; 474 in Ryu's second dream; 6 at a 200-byte stride in `AREA155` | **unlisted**; P1 (Angel Tower) and P2 (the desert) not identifiable among them by name |
| 8 | `BPLCHAR/RYUD00..03`, `RYUD10..13` | 3, `0x8007280C` etc. | Ryu's form data | 13,770 bytes (13,755 in the `1x` files), header words and a block from `+0xB781` | **P8 candidate** (an animation frame); the same 13,770-byte change is in 15 of the decompressed `BPLD`/`BRTD` arenas (section 5.1) |
| 59 | every battle, menu and boss module, three areas | `0x1A080200` image | the battle / menu UI page | 2 tiles: the shoulder-button labels redrawn (looked at, rendered grey, 2026-10-06) | unlisted; the PSP's buttons |
| 2 | `AREA067`, `AREA166` | `0x8002D800` CLUT | palettes | 2 and 8 rows | **P6 consistent**: area 67 is Stallion's (`boss_se.md`, kind 29); `AREA067`'s page also changes 4 tiles. Colours not rendered |
| 53 | `START` and 52 areas | `0x8002BE00` CLUT | palettes | 1 row in 43 areas; 2 to 160 rows in nine areas and `START` | unlisted |
| 81 | 81 areas | `0x0A081000` page | area textures | 954 tiles: **653 blanked on the PSP**, 301 redrawn | unlisted |
| 3 + 1 | `DEMO` (sections 3, 4, 5, `0x8002C000` CLUT) | images and palette | 22 + 22 + 7 tiles, 48 CLUT rows | **P13 candidate** (a new title logo) |
| 1 | `START` | 5, `0x1C000200` | the title menu page | 6 tiles | **P14 candidate**, with the five PSP-only files (`TURIMODE`, `TURISHAR`, `TMBGM000/001`, `CAPLOGO`; their contents not read) |
| 14 + 2 | the glyph atlas in 14 modules; the kanji sheet in `ENDKANJI` and `FIRST` | images | the fonts | 1 tile each; 2 tiles | unlisted |
| 1 + 1 | `SCENA17` | `0x1E080200` image, `0x8002D400` CLUT | a scenario page | 1 tile, 1 row | unlisted (possibly P8's scene; unread) |
| 9 | `AREA035, 042, 045, 057, 079, 080, 081, 128, 199` | `0x80010000` | message edits within Japanese | per slot not read | unlisted |
| 42 + 2 | `BATTLE`, `BATTLE2`, 40 `BOSS`; `FIRST`, `AFLDKWA` | the system pool | the battle pool: one slot that pointed past its block's end (an empty message) now repeats its neighbours'; the field pool grows 56 bytes, not read by slot | a fix; **P15 candidate** (in English it is this pool whose save prompts change, 5.3) |

### 5.3 What the English PSP changed in its text, against psx-us

Counted at word level, never reproduced: in the area blocks, three item or
spell names renamed together across eleven area blocks (`AREA175` to
`AREA185`), one boss name respelled, a word for one item replaced in
seven areas, a title replaced in two, a plural fixed in 24 button prompts;
in the system pools, two item descriptions (one a grammar fix, one
rewritten), the save and discard prompts reworded, and a button legend
added. **P9 is confirmed**: the respelled character name `IDEAS.md` gives
occurs in eight sections (`FIRST`, `AFLDKWA`, `AREA041` and others).

### 5.4 I32's leads against the data

| lead | verdict from the data |
|---|---|
| P1 Angel Tower passage | **cannot reach by name**: eleven areas change their `0x80104000` band (5.2), none named Angel Tower in the sibling's `areas.toml` (`AREA012`, `AREA089`, `AREA129`, `AREA155` are unnamed there); whether the passage is one of them needs the area named or the map decoded |
| P2 the desert heading | not in data seen; code (the code half) |
| P3, P4, P5 | code; not reachable here |
| P6 Stallion recoloured | **consistent**: area 67's CLUTs change two rows and its page four tiles; the colours are not rendered |
| P7 the attack renamed | **cannot reach**: the PSP does not read names from `GAME.EMI` (psp-jp and psp-eu ship it byte-identical while their languages differ), so its ability names are in `BOOT.BIN` |
| P8 the ascension frame | **candidate**: Ryu's form data changes 13,770 bytes in 8 sections and 15 arenas; not rendered |
| P9 a name respelled | **confirmed** (5.3) |
| P10 the music | **half settled**: every sample body is the PSX's, byte for byte; the instrument tables and sequences are re-authored in PSP formats and not compared |
| P13 title logo, P14 fishing from the title, P15 storage wording | **candidates**, rows above |
| P11, P12, P16 | not data |

**Unlisted by anyone:** the eleven areas' map-band changes, the 653 blanked
and 301 redrawn page tiles, 51 palette changes, the button labels, the font
tiles, the battle pool's filled slot, nine Japanese message edits, and the
English text edits of 5.3.

## 6. What could not be settled

- **The Western code differences.** Read in samples only: name-width
  constants (`slti` and `addiu` 8 to 12, the field the US widened), draw
  coordinates in area overlays, the product-code string, and in `SCENA00` a
  branch removed and a word added at the end. The last is not text. A full
  reading is per function against the PSX twin (`psx-twin-check.md`), the
  code half of I32 and of ASSET_SOURCES section 9; nothing here enters the
  cache.
- **What the data rows do.** The four Western kinds are read in section 8.
  ~~The eleven PSP map bands of 5.2 are located and sized only~~ - read
  2026-10-08 ([`opt-layers.md`](opt-layers.md) section 4): no cell byte,
  height or header changes; texture-coordinate records, a texture given to
  152 edge cells in `AREA128`, one byte lowered in `AREA142`'s runs; a
  layer, `psp-maps`.
- **Anything rendered.** No palette or tile was rendered in colour; P6, P8
  and P13 stay candidates until one is.
- **`psx-eu-en`**: not held.
- **The PSP's sequences and instrument tables** (`PPHD`, `pPMS`): no reader.

## 7. The three answers

### (1) Do the regional PSX builds differ beyond their text? Yes, in four small groups

Beyond text, layout and relocated code, the three Western discs differ from
JP's data in **83 rows of four kinds** (4.1): one area's map band
(`AREA004`), one cue byte in 65 sound banks, six bytes of `AREA004`'s
section 10, and - on the PAL discs only - one sample fewer in eight area
banks. Every enemy table differs only in its names (106 of 200 differ, stats
equal byte for byte); no image differs that is not language or a CLUT.

So **`base/` can be one tree** - JP's, the census's pair - with a short list
of per-build exceptions rather than per-language bases. The exceptions are
not language: two of the four groups are in every build after JP, the PSP
included, and only the PC (built from JP) lacks them. They look like Capcom's
own post-release changes, and that raises **the decision**: whether the base
takes JP's version of these rows (what the PC has, and the archival record),
or the later one, as a ledgered divergence, the way
[`psx-twin-check.md`](psx-twin-check.md) settles a bug's origin. The owner's
call, row by row; section 8 reads each row and gives a verdict.

### (2) The PSP's data changes

Section 5.2 is the list and 5.4 the leads: P9 confirmed; P6, P8, P13, P14,
P15 matched to rows not yet rendered; P1 not reachable by name; P7 and P10's
other half beyond the data. Of everything the PSP changed in data, the only
rows that look like game logic are the eleven `0x80104000` bands; the rest is
art, text and format.

### (3) What the importer must transform, per build

- **psx-jp**: `copy` for identical sections; the type-1 decompressor (65
  arenas; the PSP gives 37 exact test vectors); widen enemy names 8 to 12;
  split each message block, pool, enemy-name field and language image into
  `loc/ja/`.
- **psx-us, psx-fr, psx-de**: the same, plus the destination remap by band
  (`+0x8000` on `0x8002A000`, `0x8002BE00`, `0x8002D800`; `+0x6000` on the
  pool; `+0x3000` on the boot band; `regional-builds.md`), language sections
  to `loc/<lang>/` (text, fonts, plates and their CLUTs in the ten
  `PLATE_AREAS`, the area pages), and the base taken from the base's source
  except for section 4.1's rows, which follow (1)'s decision. Code sections
  are never imported.
- **PAL**: the eight area banks and the DE spell CLUTs are per-build
  exceptions; the fishing areas' CLUTs go with the language.
- **psp-jp, psp-eu**: undo the header changes (bit 31, the image flag);
  type-1 sections are already decompressed; strip the `pBVC` header to get
  the PSX sample body; `PPHD` and `pPMS` need readers of their own or the
  instrument tables and sequences come from a PSX source; `FIRST`'s and the
  five PSP-only files' sections are new kinds; section 5.2's rows are the
  data half of I32's toggle, each one a layer over the base.
- **pc-zh**: as the census (DAT_CONTAINER section 2); its own art (`FIRST`
  section 6) and its CLUT strip row stay PC-only.

## 8. What the rows do

The owner, 2026-10-06: "let's figure out what those rewrites changed. If it
looks like a bug fix, it's probably worth keeping the change as the default
option." Each of section 4.1's four kinds, read through the structure the
game's code reads it with. Measured with
[`tools/region_read.py`](../tools/region_read.py) (new) against the JP, US
and French discs; its printout is `analysis/region/read_us.txt`. The text
below gives fields, cells and values only.

### 8.1 `AREA004` section 8: the area block, 72 cells walled

**What the section is.** `0x80104000` is the PC's kind-0 tag `0xC8000`
(`DAT_CONTAINER.md` section 2), so it loads at `AreaMap_Header` `0x8CB580`
(`sprite-draw-order.md`, `MapView_CellToMap`). It is not a world-map node
table: it is the field area's own map. The layout below is what our code
reads (`map-layers.md`, `map-scroll.md`, `effect_6c.md` 1.4, `rest_2d.md`
L1), and in `AREA004` the regions butt end to end exactly:

| region | where | read by |
|---|---|---|
| header | `+0x00`: width 90, depth 88, the tile base, the list indices | everything below |
| corner heights | `+0x30`, four s8 per cell (`AreaMap_Corners`) | `MapView_Build`, `AreaMap_Slope` |
| cell bytes | header dword `+0x14` x 4 = `+0x7BF0`, one byte per cell (`AreaMap_Bytes`) | `AreaMap_ByteAt`, so `AreaMap_CellBlocked` `0x518620` (ours, `field_blocked.cpp`; PSX `FUN_801A3100`): high nibble 1-5, A, B or F blocks a step |
| tile words | 4 x the header's word `+2` = `+0x9AE0`, a u16 per cell | `MapView_CellTextures` |
| texture records | from `+0xD8C0`, 1,035 dwords | the same |

**What changed** (JP against US; FR, DE and both PSP discs carry the same
section):

- **Header and corner heights: identical.** The ground is the same shape.
- **72 cell bytes, every one `0x00` to `0x10`** - open floor to a blocking
  cell (high nibble 1), in three straight runs: 53 cells along the raised
  strip's east edge, 3 at the north end of a column on its west side, and 16
  along the corridor's bottom edge.
- On JP those lines already have wall cells at their ends and in their
  middle: stubs at both ends of the two columns, and a full wall row three
  rows above the bottom-edge run. The new cells fill the gaps between
  Capcom's own wall stubs. The one gap left open in the east-edge column is
  two cells, one of them a `0xC0` cell (the link-cell code of `rest_2d.md`
  L1): a doorway kept.
- By the corner heights, the area has a raised strip two cells wide (about 25
  units, against 0..3 either side; it rises with the ground towards the
  south). The east-edge column is a half-height cell (about 5 units) between
  the strip and the floor east of it. The area is "Dauna Mine - Minecart" in
  the sibling's `names/areas.toml`. That the strip is the track bed is a
  guess from the name.
- **Tile words: 494 changed.** 464 of them are +1, because one texture
  record is inserted and every later index shifts. 30 cells are re-pointed
  to other textures, in two short runs: one along the strip's west edge, one
  further east. One record is replaced and one dropped. The visible change
  is those 30 cells' texture, mostly along the strip's west edge.

Positions are deliberately not given (2026-10-10): a list of where Capcom
put the walls is Capcom's data in another form (section 10). The measurement
is repeatable with `tools/region_read.py` against the discs.

**In play, as far as the reading reaches.** On JP's data the party can step
onto the 53-cell edge column and the gaps beside the bottom wall row. On the
later data `AreaMap_CellBlocked` refuses those cells, which applies to the
party and to every object asking `Field_ObjectBlockedAhead`
(`field-blocked.md`). Whether JP's open edge lets the party climb onto the
strip, or walk down off the bottom-edge run, also depends on `AreaMap_TooSteep`
(slope above `0x40`), which was not computed for these cells. So "a walkway
edge that could be walked on, now walled" is read; "a way out of bounds" is
not proven.

**Live check (not run).** No recorded route walks area 4 under control. The
attract cycle shows area 4 twice (`attract-mode.md`), but its moves are
scripted, and `Field_ObjectBlocked`, the scripted move's test, skips the map
(`field-blocked.md`). The check is the owner's: in the Dauna mine's minecart
area, try to walk along the raised strip's east edge, and the bottom edge
below the wall row. JP data (today's PC) should allow it; US data should
not.

### 8.2 `AREA004` section 10: the battle placement map follows the walls (Western PSX only)

**What the section is.** `0x8002A000` is PC tag `0xC0800`, loaded at
`0x8C3D80`: the cell nibble map that `AreaMap_CellNibble` `0x592890` reads
(ours, `inventory_ops.cpp`; PSX `801C7FCC`). A nibble per cell (7,920 cells
in the first 3,960 bytes; the other 1,040 are zero on JP and US). Its
callers place the party and the enemies when a fight starts:
`Encounter_CellFits` wants a size class of at least `4 x size + 1`, and
`Encounter_StepOpen` a passage class (`inventory_ops.md` section 3). 0 means
no one is placed there.

**What changed.** 6 bytes, 8 nibbles. Each is one of section 8.1's new wall
cells, set to 0: three on the east-edge column, five on the bottom-edge run.
The other 64 new wall cells were already 0.

**Why, by measurement.** Across all 200 JP area files, a cell whose byte
blocks has nibble 0 in 980,813 of 981,024 cells (211 exceptions). The
placement map follows the collision. Section 10's change is that map
brought back in line after 8.1's walls. The PSP took 8.1 but **not** this
(its section 10 is JP's), so on the PSP these 8 cells are walls a fight can
still place someone on.

**In play.** On the PC today neither change is present, so the pair is
consistent. Applying 8.1 without 8.2 (the PSP's state) could put a member or
an enemy on a wall cell in a random battle at those spots.

### 8.3 The cue byte: one sound's priority in 65 transformed-form voice banks

**What the section is.** A type-8 section is the bank's cue-table entries,
4 bytes per cue: flags; pan and program; tone and priority; chord and voice.
The sibling's `docs/SOUND_CUES.md` ("The cue tables") reads them, from
`SE_Play` `0x8015E908` and the bank set-up. Priority is byte 2's low nibble.
In `SE_Play`, a new cue on the same primary voice as the last one is
**dropped** when its priority is lower and that voice is still sounding.

**What changed.** In every `BPLCHAR` party-voice bank, the six cues are
program 0 tones 0, 2, 4, 6 and programs 1 and 2 tone 0, on voices 20 and
18. On JP:

| banks | files | cue 1's priority | the other five |
|---|---|---:|---:|
| the party's own sets | `BPLD*`, `BPLU*` (54 each), `BRTD*`, `BRTU*` (12 each), `PAPYD*`, `PAPYU*` (5 each): 142 | **11** | 10 |
| the transformed forms' sets | `DRG*` (37), `RYUD*`, `RYUU*` (8 each), `CRYUD*`, `CRYUU*`, `REID*`, `REIU*`, `RTD*`, `RTU*` (2 each) | **10** | 10 |

The later builds set cue 1 to 11 in the second group, which makes it match
the first. Nothing else in the 24 bytes changes.

**In play (on the PSX).** In the party's normal sets, cue 1 (program 0,
tone 2) outranks its siblings: while it sounds, a later cue of the same set
on voice 20 cannot cut it. In the transformed sets on JP it could be cut. The
later builds make the forms behave as the normal sets do. What cue 1's
sample is (a cry, an attack voice) was not identified. The sibling's
`names/se_cues.toml` has no label for it.

**On the PC: nothing.** The port's `Sound_PlayEffect` `0x587740` (ours,
`sound.cpp`; `sound.md`) plays a cue as four voice words on DirectSound
channels, with no priority field and no drop rule. The byte has nowhere to
go until an engine plays the PSX's banks on an SPU model (I23's sequencer
path).

### 8.4 The PAL area banks: one sound replaced in 8 of 14 areas

**What changed** (US against FR; FR and DE identical, US identical to JP).
In the bank-2 (field sounds) VAB of eight areas - `AREA100` (Relay Point B),
`AREA112` (Relay Point A), `AREA135`, `AREA138`, `AREA147` (Caer Xhan),
`AREA168` (Station Myria), `AREA170`, `AREA188`; names from the sibling's
`areas.toml` where it has them:

- six of them drop one pair of samples (23,248 and 31,824 bytes; md5
  `f481d147612e`, `f235f1cd48c0`) and add one 10,992-byte sample
  (`4908872c1f70`). That sample is not new: it is `AREA173`'s sample 12 on
  the US disc. The four tones that played the pair (program 1 or 2, tones
  0..3; program 0, tones 10..13 in `AREA170`) now all play the one sample.
  2 to 4 cue entries reach those tones in each area;
- `AREA135` and `AREA188` drop two and four samples of another set
  (`2081d9761925`, `d67a52ce6de6`, plus `6d5032b64fd8` and `f15f21140320`
  in `AREA188`) for the same 10,992-byte sample. Two tones, one cue entry.
  Some of the dropped samples have no tone pointing at them on the US disc;
- the cue tables are unchanged.

**Not consistent across the game.** The same pair of samples is in six more
areas (`AREA139`, `140`, `148`, `149`, `167`, `197`), and the second set in
`AREA121` and `AREA136`. The PAL discs keep them there unchanged. So the
same sound plays in some areas and the substitute in others.

**In play.** On the PAL discs, whatever sound those cues make in these eight
areas is replaced by `AREA173`'s sound, shorter. What either sound is was not
identified (no label in `se_cues.toml`). The PC's banks are JP's, as are the
US's.

### 8.5 Verdicts

| kind | verdict | evidence | making it the default |
|---|---|---|---|
| 8.1 `AREA004` walls | **bug fix** (collision gaps closed); a small texture fix with it | 72 open cells along lines Capcom had already started walling, the doorway kept, the heights untouched; adopted by every later build including the PSP | possible today without new engine work, as a kind-0 overlay chunk of tag `0xC8000` for `AREA004` built from the player's US, FR, DE or PSP disc. The overlay mechanism exists (`loc_build.py`'s `<lang>.<NAME>.DAT`, walked after the original), but a non-language overlay needs its own switch and a DIV entry. Without a later disc: nothing, because the bytes are Capcom's and a patch carrying them would be shipping data (ASSET_SOURCES section 2). Area 4 is in the attract cycle, so the attract and state-hash references change wherever the layer is on |
| 8.2 placement nibbles | **bug fix, the companion of 8.1** - not separable from it | the placement map follows collision in 99.98% of JP's cells | goes with 8.1 in the same overlay (tag `0xC0800`) from a Western PSX disc. From a PSP disc it is missing. Since the rule is measured, it can be derived instead: zero the nibble under every newly blocking cell. That is an algorithm, not shipped bytes |
| 8.3 cue priority | **bug fix** (consistency) on the PSX's sound engine | 65 form banks made to match the 142 party banks' cue 1 | **nothing on the PC**, which has no cue priority. It matters only if the PSX banks are ever played through an SPU model (I23). Then it is one byte per bank, the importer's to take from a later disc or to derive ("cue 1 of a form bank takes the party banks' priority") |
| 8.4 PAL sample swap | **regional, purpose unknown**: not a fix | JP and US agree; the PAL discs change 8 of the 14 areas that use the sounds, leaving the rest | keep JP/US. Offering PAL's would be a per-build option no one has asked for |

## 9. The PC in the same table

| pair | sections | identical | text | layout | layout+text | logic-data | art | dropped |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| psx-jp vs pc-zh | 3,222 | 2,120 | 290 | 65 | 200 | 4 | 1 | 542 |

The census's numbers (2,120 identical of 2,680 paired, 542 dropped), now
classified: 290 text (199 message blocks, 44 pools, 46 of the port's 47
replaced images, the CLUT strip row), 65 layout (the type-1 arenas,
decompressed), 200 layout+text (the enemy tables: names widened, **stats equal
in 200 of 200**), 1 art (`FIRST` section 6, the 47th image), and 4 logic-data: `RYUD00..03`'s
one byte at `+0x7ACE`, still unexplained. The PC has JP's `AREA004` section 8, not the later version; the cue byte is
in a bank the port converted to WAV, so it is not compared. Audio banks are not in the census's pairing
(`DAT_CONTAINER.md`: every bank pairs exactly).

## 10. The fix: from the player's Western disc

The owner, 2026-10-06: "make the code change so that it works the same
regardless of source". That was built first as a coordinate table in our code
(`src/game/area4_walls.cpp`, `BOF3X_AREA4_WALLS`, 2026-10-06 to 2026-10-10).
The owner, 2026-10-10: do it without carrying Capcom's table in our code, if
we can ([`LICENSING.md`](LICENSING.md) section 3). A table of where Capcom put
walls, in our source, is Capcom's data in another form. So the question was
first whether a rule, read from the JP map's own data at load time, gives the
72 cells (10.1). It does not, so the fix is now the `area4-walls` optional
layer built from the player's own Western disc (10.2). The table and its code
are removed.

### 10.1 No rule gives the 72 cells

Measured 2026-10-10 on the JP disc's `AREA004` section 8 against the US
disc's, then over all 200 areas' area blocks (`0x80104000`) on the JP disc.
The scripts read the discs and print counts and coordinates; nothing they
read is in the repo, and the coordinates are not given here either (section
8.1's note).

**The map's own data.** The corner heights (`+0x30`, four s8 per cell, in the
order x0z0, x1z0, x0z1, x1z1) give each cell's four corners, so a step
between two neighbouring cells is the difference of their corners on the
shared edge. Along the strip's east edge, the walled column is 16 units below
the strip on the shared edge, a vertical step: a plausible "cliff" cue. The
other two lines are not like that:

- Of the three cells on the west side's column, the first has **no** step to
  any open neighbour (its only one is to the wall stub above it), the other
  two a step of 7 at one corner, and of 7 and 13. Yet the same column's
  cells south of them, left open by every later disc, step 13 and 25 to the
  strip beside them.
- The bottom-edge run sits 8 units below the row above it along its whole
  length, a step; but a step of 8 is common ground in this map (the table
  below), and that run's step is smaller than many left open.

**Candidate rules, scored.** An open cell is "hit" by:

| rule | hits in AREA004 | of the 72 | outside the 72 | cells hit in all 200 areas | areas hit |
|---|---:|---:|---:|---:|---:|
| a step of 7 or more to any neighbour | 1,300 | 72 | 1,228 | 56,441 | 179 |
| a step of 8 or more | 1,264 | 71 | 1,193 | 56,357 | 179 |
| a step of 16 or more | 895 | 54 | 841 | 33,120 | 174 |
| a step of 7 or more *up* to a neighbour | 670 | 71 | 599 | 31,390 | 165 |
| on a straight open run of at most 31 cells between two `0x10` cells | 814 | 69 | 745 | 154,696 | 181 |
| the same, at most 40 | 1,549 | 72 | 1,477 | 161,849 | 181 |
| a run of at most 40 and a step of 7 or more | 325 | 72 | 253 | 37,415 | 167 |

The best of them takes all 72 at the price of 253 other cells in area 4
alone and 37,415 cells in 167 areas. No threshold separates them.

**Local twins.** The sharpest test: a cell's signature taken as the four
neighbours' cell bytes and the exact step vectors on all four shared edges.
The 72 cells have 23 distinct signatures, and **58 other open cells in area 4
share one of them** (with a coarse signature - each edge's step as up, down
or none, and whether the neighbour blocks - 260 cells). So no rule that looks
only at a cell and its neighbours' heights and bytes can choose the 72 without
also choosing those; any rule that does is a table of coordinates in another
form. The walls are a level designer's choice along the lines the designer
had started, not a property of the terrain.

### 10.2 The `area4-walls` layer

- **What it carries.** Two kind-0 chunks for `AREA004.DAT`: the Western
  disc's **whole** cell-byte plane (the area block's `AreaMap_Bytes`, 7,920
  bytes at 4 x the header's dword `+0x14`, landing at tag `0xC8000` plus that
  offset) and its **whole** placement map (5,000 bytes at tag `0xC0800`).
  Whole planes, not the 78 differing bytes, so that `recipes/opt.toml`
  records two tags, sizes and hashes and no cell's place or value. On JP's
  map they change the 72 cells and the 8 nibbles and nothing else (10.3).
  Not the tile words or texture records: the 30-cell re-texture is still
  not taken.
- **Where it comes from.** `tools/importer.py build --opt area4-walls` from
  the player's US, European, French or German PSX disc (the four carry both
  chunks byte for byte). Since 2026-10-10 it is **on by default**: any
  `build` with one of those discs among its sources builds it, `install`
  installs it whenever the cache holds it (to `DAT\area4-walls.AREA004.DAT`),
  and the launcher names it in `BOF3X_OPT` whenever it is installed and the
  ini's `opt=` is empty. `--no-opt area4-walls` leaves it out of a build or an
  install (an install also removes a copy already in `DAT\`); the ini's
  `opt=none`, or an `opt=` list without it, turns it off in play
  ([`launcher-settings.md`](launcher-settings.md)). Not from a PSP disc: the
  PSP lacks the placement half. The game walks it as DIV-0086's loader prefix
  walks any layer ([`opt-layers.md`](opt-layers.md)).
- **No Western disc, no walls.** With only the PC install or the JP disc, area
  4 is the shipped open map. That is the price of keeping Capcom's data out
  of our code.
- **The old switch.** `BOF3X_AREA4_WALLS` is gone with the code. `=0` is
  accepted with a log line (the shipped map is now the default); any other
  value is refused at start-up, naming the layer, rather than ignored.

### 10.3 Proof without the game

`tools/region_read.py fix --layer <cache>/opt/area4-walls/dat/AREA004.DAT`
lays the built layer over the JP disc's `AREA004` sections 8 and 10 as
`LoadDatFile` lays a kind-0 chunk (the payload at its tag) and compares with a
later disc (2026-10-10, the layer built from the US disc):

| against | cells changed on JP's map | placement nibbles changed | cell plane after | placement map after | section 8 outside the plane |
|---|---|---|---|---|---|
| `psx-us` | 72, every one `0x00` to `0x10`, the three runs of 8.1 | 8, every one to 0, every one on a walled cell | identical | identical | JP's, untouched (the 920 bytes still different are the re-texture) |
| `psx-de`, `psx-fr`, `psx-eu-en` | the same | the same | identical | identical | the same |

The layer built from the German disc is the same file as the one built from
the US disc (md5 equal). `importer.py verify`: 2 of 2 chunks and 2 of 2
composed base chunks as `recipes/opt.toml` records them. `importer.py
check`: 6 layers, 0 errors.

Self-tests: the layer adds no code and no chunk kind. Built with llvm-mingw;
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=field_blocked` from an ini-less copy:
exit 0, `inject: 10081 ours`; with `BOF3X_AREA4_WALLS=1`, the refusal.

### 10.4 The attract, the live check

- **The attract demo shows area 4 twice a cycle** (`attract-mode.md`). The
  2026-10-06 A/B with the walls on against off (`analysis/statehash/
  attract_walls_on` / `_off`) still describes the layer: the same bytes in
  memory, four pages differing from the first load at tick 1,312 (the cell
  plane's `0x8D3000`, `0x8D4000`, the placement map's `0x8C3000`,
  `0x8C4000`) and nothing else, so the demo's scripted moves never meet the
  walls. The references are recorded in a game directory without the layer
  installed, so they are unchanged; where it is installed it now plays by
  default, and an attract hash there differs at those four pages.
- **Live check (the owner's, not run).** With the layer installed (on by
  default since 2026-10-10):
  in Dauna Mine's minecart area, walk along the raised strip's east edge and
  along the bottom edge below the wall row. They should block. Without it
  they should be open, as on JP and the shipped PC. If the area has random
  battles, one started beside those edges should place no one on the new
  walls.

The ledger entry is DIV-0080.

## Tools used

All ours: `tools/region_diff.py` and `tools/region_read.py` (new), `tools/psx_disc.py`, `tools/dat.py`,
`analysis/dat_census.json`. Python 3.13 with capstone (BSD-3) for reading
sample instructions and Pillow (MIT-CMU) for one grey render of tiles; nothing
of theirs is in the repo. The sibling's `names/areas.toml`,
`docs/EMI_TYPES.md`, `docs/OVERLAYS.md`, `docs/TEXT_TABLES.md` and
`docs/regional-builds.md` are cited, not vendored.
