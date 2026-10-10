# The optional layers: the PSP's content as `opt/` layers, the loader's second prefix, the presets

**Status:** IN PROGRESS (2026-10-08, one cloud session: unified-data step 4.
The five layers built and verified offline from both PSP discs, the loader
change compiled, the presets and `install` run. **Not run:** the owner's
llvm-mingw build, the `'*'` self-tests and any look in play - section 8)

[`unified-data-plan.md`](unified-data-plan.md) step 4. A later build's
content change is an `opt/` layer from the player's own disc, off by default
(the plan's section 7) - except `area4-walls`, a fix, on by default since
2026-10-10 (section 1). This is those layers for the PSP releases: what they
carry and why, how the engine walks them (DIV-0086, `BOF3X_OPT`), how a layer
gets from the cache into a game's `DAT/`, and the importer's presets.

No game content is reproduced here: counts, sections, tags, offsets, deltas
and hashes only (CLAUDE.md rule 1). The recipe `recipes/opt.toml` holds
hashes and offsets, never bytes.

## 1. The layers

| Layer | Carries | Chunks | From |
|---|---|---:|---|
| `psp-art` | **P6**: Stallion's palettes - `AREA067` rows 6-7 and `AREA166` rows 0-7 of the `0x8002D800` section (PC tag `0xA000`), the rows of [`psp-stallion.md`](psp-stallion.md) 2 and no others | 2 (320 bytes) | either PSP disc |
| `psp-tiles` | the PSP's other art in the areas: 840 tiles of the `0x0A081000` texture page in 68 areas (583 blanked on the PSP, 257 redrawn), and 515 rows of the `0x8002BE00` palettes in 52 areas | 437 | either PSP disc |
| `psp-maps` | the PSP's edits to 11 areas' map bands (`0x80104000`, PC tag `0xC8000`): texture coordinates, tile words, cell-run records (section 4) | 260 (1,608 bytes) | either PSP disc |
| `psp-names-en-150` | **P7** and the other English renames: the 8 name records where the PSP-EU's tables differ from the US disc's - abilities 46, 47, **116** and 218, weapon 14, armour 9, accessories 29 and 45 (`Ability_Records` ids, [`exe-tables-by-build.md`](exe-tables-by-build.md) 4.2) | 8 | the PSP-EU disc |
| `psp-names-ja-JP` | the PSP-JP's renames: ability 116 and key items 2, 5, 7, 9 | 5 | the PSP-JP disc |
| `area4-walls` | **DIV-0080** (added 2026-10-10): `AREA004`'s collision as the Western PSX discs ship it - the area block's whole cell-byte plane (7,920 bytes, inside PC tag `0xC8000`) and the whole battle placement map (5,000 bytes, PC tag `0xC0800`). On JP's map: 72 cells walled, 8 placement nibbles to 0, nothing else; not the 30-cell re-texture | 2 (12,920 bytes) | the US, European, French or German PSX disc (all four carry both chunks byte for byte); not a PSP disc, which lacks the placement half |

**The choices, recorded:**

- **`psp-art` is P6 alone; everything else the PSP repainted in the areas
  is `psp-tiles`.** P6 is the change the owner asked about, ten palette rows
  whose effect is rendered and read (Stallion and its three palette-swapped
  kin); the 840 tiles and 515 rows are unread content - 583 of the tiles
  are the PSP *removing* texture, for a reason not read. Folding them in
  would turn a Stallion toggle into a 100-area repaint. `AREA067`'s four
  changed page tiles and `AREA166`'s `0x8002BE00` row 5 (the row
  [`psp-stallion.md`](psp-stallion.md) 4 calls a separate decision) are in
  `psp-tiles`, not `psp-art`. Area 166's three kin go with Stallion: the
  eight rows are one section and one toggle (psp-stallion.md's
  recommendation). The owner's call to merge or split further (section 9).
- **The names are per language**: they are glyph codes of a language
  layer's font (DIV-0008), so each names layer is named by its text's BCP 47
  tag and the engine refuses it under another language (section 5).
  `psp-names-en-150` is the PSP-EU's text (its `fixtures.toml` tag), but its
  records are those that differ from the **US** tables: measured, the PSP-EU
  ELF's 537 names equal the US disc's in all but those 8, while against the
  European-English PSX disc 16 ability names differ (that disc's own 12 edits,
  which the PSP did not take, plus 4 of the 8). So the layer is the PSP's
  renames over either English layer: under `en-US` the result is the PSP-EU's
  tables exactly (section 6); under `en-150` the European disc's 12 other
  ability names stay.
- **The map bands are a layer, not a rule** (section 4).
- **`area4-walls` is the one layer from a PSX disc** and the one that is a
  fix, not a content change. It was a coordinate table in our code until
  2026-10-10; the owner asked for it without Capcom's table, no rule from the
  map's own data gives the 72 cells ([`region-diff.md`](region-diff.md)
  10.1), so it is a layer: whole planes, so that the recipe records no cell.
  **It is the one layer on by default** (2026-10-10, the owner's word):
  `build` builds it whenever one of the four Western discs is among its
  sources, `install` installs it whenever the cache holds it, and the
  launcher names it whenever it is installed and the ini's `opt=` is empty
  (section 5). `--no-opt area4-walls` leaves it out of a build or an install
  (the install also removing a copy already in `DAT/`); `opt=none` keeps it
  installed but unplayed. A whole plane is a sub-range of the
  area block, as section 2 requires, landing at its tag plus the plane's
  offset.

## 2. How a layer is made

**A layer chunk is a sub-range of the base chunk it changes**, not a whole
chunk: the plan's "layers override by chunk" holds at the level of the
loader's own unit, because each chunk kind lands by its tag.

- **Kind 0** lands at `0x803580 + tag`, one arena, no table
  ([`asset-loading-path.md`](asset-loading-path.md) 2, "Kind 0 answers the
  open question"; our `WalkDatFile` does exactly that). So a palette row
  is a chunk at the section's tag plus the row's offset: `AREA067`'s rows
  6-7 are one 64-byte chunk at tag `0xA0C0`. Nothing else keys on those
  tags: the loader's only tag tests are `0x10000` (the upload queue),
  `MsgPool_TakeChunk`'s pool tag and `0xC8000` (DIV-0085's snapshot flag,
  set by the base file, which every layer follows).
- **Kind 1**'s tag is a tile rectangle - x, y and width in 32-pixel tiles
  ([`asset-loading-path.md`](asset-loading-path.md) 2, "Kind 1"). A run of
  changed tiles within one row of the page is one chunk whose tag names that
  run's position with the run's length as the width: it uploads exactly
  those tiles and wraps nowhere.
- **Kind 5** (DIV-0008) took only a whole table; DIV-0086 widens it to any
  run of records of one table (`NameTables_Apply`: the tag on a record's
  name field, the size at most the records left). A whole-table chunk is
  handled as before.

**`recipes/opt.toml`** is generated, as `recipes/pc-zh.toml` is: `importer.py
opt-recipes` reads the JP, US and both PSP discs, the PC's `DAT/`, and
`region_diff.py pair`'s `psx-jp_vs_psp-jp.json`; for every PSP section the
pair calls changed that a layer takes, it finds the PC base chunk the JP
section is (the recipe's `psx-jp:copy` source; for the port's dial page,
the PC chunk at the same tag), cuts the changed rows / tiles / byte runs out
of the PSP section, and records for each chunk its kind, tag, size, sha256,
source (`emi`, `at`) and the PSP builds that carry it byte for byte (`on`),
and for each base chunk it lands on (`over`) the hash of that chunk with the
layer laid over it (`composed`). Map-band runs are merged across equal gaps
of up to 16 bytes. A names chunk records its table and record; its hash is
of the encoded 16-byte field.

**The port's own art wins.** In the 16 world-map areas the PC's
`0x0A081000` page is the port's (its keyboard legend, the dial block of 14
tiles, [`importer.md`](importer.md) 3). The PSP changed tiles of the same
block (its own buttons): 114 of its 954 changed tiles are inside the port's
block and are **not** taken - 13 areas lose every PSP change that way (all
their PSP tiles are inside the block), `AREA019`, `066` and `123` keep their
other PSP tiles.

**Names** are encoded as `loc_build.py`'s `convert_names` encodes a donor
name (`encode_text`; `ja_name` for Japanese), in-process. A Japanese name
that would need a pair code (DIV-0057) is refused: pair codes are numbered by
the `ja-JP` layer's own build. None of the 5 needs one (measured).

**`build --opt NAME`** cuts each chunk from the first PSP disc in the
player's order that the recipe lists for it, hashes it against the recipe,
and writes `opt/<name>/dat/NAME.DAT` - a layer whole or not at all - with
each chunk's source in `manifest.toml` (`[cache] opt`, and
`opt_recipe_sha256`). **`verify`** checks each layer chunk against the
recipe and the manifest, and composes each landing chunk - the cache's
`base/` chunk with the layer laid over it, as the loader lays it - against
`composed`. **`check`** (CI, no game data) checks `opt.toml` against
`pc-zh.toml`: every chunk lands inside a base chunk of its kind (kind 0
inside the bytes, kind 1 inside the page's rectangle), every names chunk on
one record of a table `NameTables_Apply` knows, every source a PSP build.

**psp-unwrap, as far as these layers need it:** only the destination
undoing (`region_diff.nd`: the PSP clears bit 31 of RAM destinations and
sets bit 0 of some image words), used to pair sections; a layer chunk's tag
is the PC's, never the PSP's destination. The PSP-EU's kind-0 bands sit
`0x8000` higher (the Western layout); its twin is matched by section index,
type and size. The `pBVC` strip is not needed by any layer (audio, step 6).

## 3. What the PSP changed that no layer carries

`opt-recipes` reports every changed PSP section it does not take, by
`region_diff.py`'s class (psx-jp against psp-jp):

| Class, what | Sections | Why not |
|---|---:|---|
| art, an image page | 62 | 59 the PSP's shoulder-button labels on the battle / menu UI page (`0x1A080200`); `DEMO`'s two logo pages (P13); `SCENA17`'s page (one tile, unassigned) |
| art, a CLUT section | 3 | `DEMO`'s (beside its logo), `SCENA17`'s (one row), `START`'s `0x8002BE00` (11 rows, a menu module's, beside the title page) |
| art, `DEMO`'s language page | 3 | `DEMO`, `AREA004`, `AREA024` at `0x1A080400`: language (they differ PSP-JP to PSP-EU) |
| art, the glyph atlas / kanji sheet | 14 + 2 | the PSX font; the PC draws with its own |
| art, the title menu page | 1 | P14, the PSP's title menu; the PC's own page |
| logic-data, data | 9 | `AREA004`'s band (DIV-0080's walls are the `area4-walls` layer, from a Western PSX disc; the PSP's form lacks the placement half, region-diff.md 8.2) and Ryu's form data in 8 `RYUD` files (P8, unread, not art) |
| logic-data, cue entries | 65 | the cue byte, nothing on the PC (region-diff.md 8.3) |
| text, edits within Japanese | 9 + 44 | message blocks and system pools - text, a language layer's business (the battle pool's filled slot among them) |
| converted | 2,225 | the PSP's own formats (audio, decompressed arenas, `FIRST`'s PSP-only section): not content changes |

Nor do the PSP's content changes outside the EMIs have a layer: the level
table of roster index 6 rebalanced on both PSP ELFs, and consumable 87's flag
bit ([`exe-tables-by-build.md`](exe-tables-by-build.md) 4.4). Those live in
the exe's `.data`, not in a `DAT`; a layer for them waits on `base/exe/`
(step 8) or a chunk kind of its own, and the level table is a balance
change (an Extension-tier entry, not a Sensible one) - the owner's call.

## 4. The PSP's 11 map bands, read

[`region-diff.md`](region-diff.md) 6 left them "located and sized only".
Read with the area block's header (the u16 list offsets at `+4..+0x12`,
`+0x20..+0x28`, [`map-scroll.md`](map-scroll.md) "The area block's three
lists"), against the JP disc (scratch scripts over `region_diff.Build`;
psp-jp and psp-eu carry the same bands, 11 of 11):

| Area | Where in the block | What changes |
|---|---|---|
| `AREA005`, `010`, `011`, `012` (the Cedar Woods treehouse in spring, fall, burning; one unnamed) | the list at header `+6` | two records, the same edit in all four: coordinate bytes moved by 2 and by 15 |
| `AREA030`, `089`, `129` (the three fishing areas) | the list at header `+8` | three records, the same in all three: in each, two bytes 2 lower and two 1 higher |
| `AREA052` (the dump site) | the lists at `+4` and `+6` | ten records, bytes moved by 1 or 2 (15 bytes) |
| `AREA128` (the dock) | the tile words | 152 cells at x 0..18, z 66..73 that have no texture on the JP disc (tile word 0) get one |
| `AREA142` (Ryu's second dream) | the cell-run list (`+0x24`, `MapView_PlaceRuns`') | the low byte of 474 records, each 8 lower |
| `AREA155` | the tile words | 6 cells (x 41, z 12..17) pointed at other textures |

**No band changes a cell byte, a corner height or the header**: nothing in
any of them is collision or ground shape, so I32's P1 (an Angel Tower
passage) is not among them as a collision change. The lists at `+4` and
`+6` are texture coordinates: `Prim_SetTexture` `0x572A00` takes a cell's
(u, v) "through u16 offsets at +4 / +6" (`symbols.toml`, its evidence); the
list at `+8` has no direct reference in the exe (a byte scan for
`0x8CB588`: none) and its bytes move as the others do.

**Verdict: a layer (`psp-maps`), not a rule.** By the plan's section 7 a
later build's *fix* made the default is a rule in code, its *content change*
an optional layer. These are presentation, not logic: texture coordinates
nudged by a texel or two, a map's edge cells given texture, one list's
records shifted. The reading that fits is the PSP's renderer - bilinear
filtering bleeds a texel across a coordinate's edge, and a 16:9 view shows
cells a 4:3 one never did - which would make them interesting under this
port's linear filter (the original's default, DIV-0012's point filter the
alternative) and DIV-0041's wide picture. That is a reading, not a
measurement; and expressing them by rule would mean carrying Capcom's
coordinate and tile values in our code, which region-diff.md 10.2 declined
for `AREA004`'s re-texture. A look in play could promote one to a rule.

## 5. The loader's second prefix (DIV-0086)

`src/game/dat_load.cpp`: after `DAT\<name>` and the language overlay
`DAT\<tag>.<name>` (DIV-0005), `LoadDatFile` walks `DAT\<layer>.<name>` for
each layer of **`BOF3X_OPT`** - a comma-separated list, in order, each
landing on top of the last - when the file exists; then DIV-0085's
snapshot, as before, so it sees the layers. (Until 2026-10-10 `area4_walls`
ran there too; DIV-0080's walls are now the `area4-walls` layer.)

- **The path buffer** is `0x40` bytes: `DAT\` + a layer of at most 23
  characters + `.` + the file name + NUL fits any name up to 35 characters;
  the longest of `Dat_FileNames`' 742 is 12 (the exe's table, read
  2026-10-08). A path that would not fit is a `Fatal`, not a skip. (The
  language overlay's `0x30` buffer and its `strlen(name) < 0x20` skip are
  DIV-0005's, unchanged.)
- **Refused at injection**, loudly (rule 4): a list over 191 characters, a
  name of 0 or over 23 characters or with a character other than a letter,
  digit or `-`, a name twice, more than 8, a layer with no
  `DAT\<layer>.*.DAT` installed, and a **text layer** - a name ending in
  `-<tag>`, the tag one of the five overlay tags (`src/game/language_tags.h`,
  the launcher's list too since 2026-10-10) - whose language is not
  `BOF3X_LANG`'s primary subtag (no language: refused too). `BOF3X_OPT=original` is none, as `BOF3X_LANG`'s; so is `BOF3X_OPT=none` (2026-10-10).
  A `BOF3X_LANG` that is a retired bare code (`en`, `fr`, `de`, `ja`) stops
  the game before any of this, and the launcher before it starts (DIV-0005,
  2026-10-10): a text layer is only ever matched against a tag.
- **Logs** `DIV-0086: optional layer N, DAT\<layer>.*.DAT` per layer at
  injection; `NameTables_Apply` logs `DIV-0086: n <table> name(s) replaced
  from record k` per names chunk.
- **The launcher**: `bof3x.ini` gains `opt=` (the list); the launcher sets
  `BOF3X_OPT` from it, when the variable is empty, with the layers whose
  `DAT\<layer>.*` exist and, of the text layers, those of the language
  played - each one dropped said on stderr, as `language=` does. The
  language played is the `BOF3X_LANG` the game is actually given, not the
  ini's `language=` (since 2026-10-10: an overlay not built, or a
  `BOF3X_LANG=original` already in the environment, left the ini's text
  layer in and the DLL refused it at start-up). **No dialog
  box**: the pattern for one (`kLanguages`' combo box) is a list of fixed
  entries, and a set of checkboxes found by what is installed is new dialog
  work; the variable is the must, the box the owner's call (section 9).
- **The default (2026-10-10, DIV-0080).** An empty `opt=` - the default,
  and what a launcher with no `bof3x.ini` has - names the default layers
  that are installed (`kOptDefault` in `src/launcher/config.h`: today only
  `area4-walls`, when a `DAT\area4-walls.*.DAT` exists), with a line on
  stderr. `opt=none` names none; a list is exactly that list, so one
  without `area4-walls` turns the walls off. A set `BOF3X_OPT` still wins.
  `opt=` is kept as written and checked at start (`ConfigOptValid`): a
  value the DLL would refuse, or `none` inside a list, stops the launcher
  with a message rather than being dropped into the default. The dialog has
  no box for it, so a save writes it back unchanged and never pins the
  default off. The DLL has no default of its own: `BOF3X_OPT` unset is
  none, as before.

**How a layer reaches the engine today.** The engine reads `DAT\` in the game
directory, not the cache. `importer.py install --cache CACHE --game DIR --opt
NAME` copies `opt/<name>/dat/X.DAT` to `DIR/DAT/<name>.X.DAT` (and `--lang
TAG` `loc/<tag>/dat/X.DAT` to `DIR/DAT/<tag>.X.DAT`, what `loc_build.py`
writes), after removing that layer's old `<name>.*.DAT` files, and refuses a
directory without `DAT/FIRST.DAT` or a layer naming a file the install lacks.
Then `BOF3X_OPT=<name>` (or the ini's `opt=`). A default layer
(`DEFAULT_OPT` in `importer.py`: `area4-walls`) is installed without
`--opt` whenever the cache holds it, and then played without `opt=`;
`--no-opt NAME` leaves it out and removes its `<name>.*.DAT` from `DAT/`.

**Not done by this session and the owner's:** the build with llvm-mingw
(`cmake --preset i686 && cmake --build build`), the `'*'` self-tests (the
loader is injected, not fuzzed; `NameTables_Apply` has no shadow test), and
the live look. The state hash: a layer changes the arena and VRAM, so
reference runs want no layer: `BOF3X_OPT` unset with none installed, or
`BOF3X_OPT=none` where `area4-walls` is installed (the launcher would name it).

## 6. Measured (2026-10-08)

All from `/workspace/scratch/game` (the PC's `DAT/` and `BOF3.exe`, the JP,
US, both PSP discs; every source identified against `fixtures.toml` by the
importer):

| What | Command | Result |
|---|---|---|
| the recipe | `importer.py opt-recipes --dat DAT --disc JP --disc US --disc PSPJP --disc PSPEU` | 5 layers; 699 art chunks, **699 on both PSP discs**; 13 name records |
| CI check | `importer.py check` | 742 files, 3,582 chunks, 0 errors; `opt.toml` 5 layers, 0 errors |
| all five layers | `build --source JP --source DAT --source PSPJP --source PSPEU --opt` x5, then `verify` | 742 of 742; `psp-art` 2 of 2 chunks, 2 of 2 composed; `psp-tiles` 437 / 437, 120 / 120; `psp-maps` 260 / 260, 11 / 11; names 8 / 8 and 5 / 5 |
| from either PSP disc | `build --source PSPEU --opt psp-art --opt psp-tiles --opt psp-maps`, `diff -r` against the PSP-JP build | the three layers' 114 containers byte-identical |
| the arena proof | a scratch simulation of `LoadDatFile`'s walk (kind 0 into an arena at the tag, kind 1 into a 1024 x 512 VRAM image by the rectangle) over the PC's `DAT/X.DAT` then each layer's `X.DAT`, every PC chunk's region hashed | `psp-art` alone: 2 of 2 landing chunks equal the PSP section, 18 of 18 others the PC's. All three art layers: 130 of 133 landing chunks equal the PSP section (the same against the PSP-EU disc), the other 3 (`AREA019`, `066`, `123`'s pages) differ from it in exactly the port's 14 dial-block tiles each; 920 of 920 others the PC's own |
| the names composed | `--preset pc-plus-us-text` + PSP-EU, `--opt psp-names-en-150`; the `loc/en-US` kind-5 tables with the layer's chunks laid on | 537 of 537 records equal the PSP-EU's names (529 without the layer) |
| | `--preset pc-plus-jp-text` + PSP-JP, `--opt psp-names-ja-JP` | 465 of 465 records that encode without a pair code equal the PSP-JP's; verify 742 of 742 |
| no `--opt` | `build --source JP --source DAT`, `verify` | **742 of 742** byte-identical, 2,376 chunks from psx-jp, unchanged |
| compile | `i686-w64-mingw32-g++ -std=c++20 -fsyntax-only -Wall -Wextra -Isrc -I<gen> -DUNICODE -D_UNICODE` | `dat_load.cpp`, `name_tables.cpp`, `launcher/config.cpp`: clean; `LayerLanguage` run natively on 10 names as expected |
| ledger | `tools/ledger_check.py` | 86 entries, 0 errors |

## 7. Presets

`importer.py build --preset NAME --source ...`: a named source order plus
layers (`PRESETS` in `importer.py`). The player gives the files in any
order; the preset puts them in its own, refuses when one it names is not
given (with the preset's name and what is missing), and refuses an extra
source - except a PSP disc, which goes last, where it can only fill what the
preset's sources leave, for `--opt`. `--lang` / `--opt` on the command line
add to the preset's. Any build, preset or not, with a US, European, French
or German PSX disc among its sources also builds `area4-walls` (DIV-0080,
`DEFAULT_OPT`, since 2026-10-10); `--no-opt area4-walls` leaves it out.

| Preset | Sources, in order | Layers |
|---|---|---|
| `pc-install` | the PC's `DAT/`, `BOF3.exe` | - |
| `us-disc`, `jp-disc`, `eu-en-disc`, `fr-disc`, `de-disc` | that PSX disc alone | - |
| `pc-plus-us-text` | `DAT/`, `BOF3.exe`, the US disc | `--lang en-US` (and `area4-walls` by default) |
| `pc-plus-eu-en-text`, `pc-plus-fr-text`, `pc-plus-de-text`, `pc-plus-jp-text` | `DAT/`, `BOF3.exe`, that disc | its tag (and, but for JP, `area4-walls` by default) |

Measured: `pc-install` 742 of 742; `pc-plus-us-text` and `pc-plus-jp-text`
(with a PSP disc and a names layer) 742 of 742 with their language layers
(245 containers each); a preset missing a source, given an extra, or
unknown, refused with the message. No preset turns a PSP `opt/` layer on: the
plan's three are the PC and PSX sources; a "PC + PSP extras" preset is the
owner's call. `area4-walls` comes from the default above, not a preset.

## 8. Not done

- **No live run.** The owner's llvm-mingw build, the self-tests and a look:
  Stallion is fight 24 in area 67 and no recorded route reaches it
  ([`psp-stallion.md`](psp-stallion.md) 4); `psp-tiles` and `psp-maps` want a
  walk through a few of their areas (`AREA128`'s dock edge under the wide
  picture, the treehouse, a fishing area) with the layer on and off.
- **No launcher box** (section 5).
- **The engine still reads `DAT/`**, not the cache; `install` bridges.
- **What the blanked tiles and the map-band edits are for** is read only to
  their structures (section 4).
- **The PSP's other content changes**: the level table and consumable 87
  (section 3, the exe's tables), the PSP-EU's text edits (P9's respelled
  name and the rest of region-diff.md 5.3: a text layer would be a
  `loc_build.py` run over the PSP's message blocks, a language layer of its
  own), the button labels, logo and title page (PSP-specific), Ryu's form
  data (P8, unread). The battle pool's filled slot is a fix and, by the
  plan's section 7, a rule candidate for every language, not a layer.

## For the other files

Edits proposed for the coordinator to fold in (not made here):

- **`docs/unified-data-plan.md`**: step 4's row - done 2026-10-08, this
  doc; five layers `psp-art`, `psp-tiles`, `psp-maps`, `psp-names-en-150`,
  `psp-names-ja-JP`, `BOF3X_OPT` (DIV-0086), presets, `install`; not seen
  live. Section 2's `opt/<name>/` line: name the five. Section 3 item 2:
  presets done (`--preset`, section 7 here). Section 4's table:
  "`LoadDatFile`'s second prefix" done. Section 10's second item (the PSP's
  map bands, rule or layer): closed - a layer, read in section 4 here.
- **`docs/importer.md`**: status - presets and the `opt/` layers done
  (step 4); section 2's pipeline - `--opt`, `--preset`, `install`, the
  manifest's `[cache] opt` and `opt_recipe_sha256`; section 6 "Not done":
  strike presets; add to its commands `opt-recipes` (needs
  `analysis/region/psx-jp_vs_psp-jp.json`) and the preset table (section 7
  here, or a pointer). Its `FROM_OTHER_BUILDS` "a PSP disc's differ: step
  4" wording can point here.
- **`docs/STATUS.md`**: unified-data step 4 landed: the PSP's P6 / P7 and
  the rest of its area art, map bands and renames as optional layers,
  `BOF3X_OPT` (DIV-0086), 11 presets; build and self-tests owed.
- **`docs/HANDOFF.md`**: the owner's llvm-mingw build and `'*'` narrow with
  `BOF3X_OPT` unset (self-tests unchanged) and set (`psp-art`); `install`
  then a look at Stallion when a route reaches fight 24; the launcher box if
  wanted. Trap: a state-hash or attract reference run wants `BOF3X_OPT`
  unset (the layers change the arena and VRAM).
- **`docs/owner-review.md`**: (1) Stallion with `psp-art` on and off (fight
  24, area 67; area 166's fight 48 for the kin); (2) the attack's banner
  with `psp-names-en-150` (ability 116); (3) a walk through `AREA128`'s dock
  (wide picture on), the treehouse and a fishing area with `psp-maps` on and
  off - does any edit read as a fix worth a rule?; (4) a few `psp-tiles` areas
  on and off - do the 583 blanked tiles show?
- **`docs/DIVERGENCE.md` DIV-0008**: a line that DIV-0086 widened kind 5 to
  a run of records (whole tables unchanged). If another branch has taken
  DIV-0086 by the merge, renumber this one (the code cites it in
  `dat_load.cpp`, `name_tables.cpp`, `launcher/config.h` / `.cpp`,
  `tools/importer.py`).
- **`docs/launcher-settings.md`**: the `opt=` key and what the launcher
  does with it (section 5 here).
- **`docs/psp-stallion.md`** section 4 ("P6, how", "P7, how"): built, here;
  the layer takes rows exactly and P7 as one record. **`docs/region-diff.md`**
  section 6's map-band item: read, section 4 here.
- **`docs/state-hash.md`**: `BOF3X_OPT` among the variables a reference run
  leaves unset.
- **`fixtures.toml`**: nothing.

### The owner's calls

1. **`psp-art` one layer or two.** Chosen: two - `psp-art` is P6's ten
   rows, `psp-tiles` the 840 tiles and 515 rows of the other areas (583 of
   the tiles blanked on the PSP). Fold them into one, or split `psp-tiles`
   further (the palettes from the tiles, the blanked from the redrawn)?
2. **The PSP map bands: rule or layer.** Chosen: a layer (`psp-maps`),
   off by default - presentation, not logic, and its purpose a reading
   (section 4). A look in play could promote any of them to a rule.
3. **What the launcher should offer.** Today the ini's `opt=` only.
   Recommended: a "PSP extras" group of checkboxes for the installed
   layers, the names layer shown only when its language is chosen.
4. **`psp-names-en-150` over `en-150`**: the PSP's 8 renames only (chosen),
   or the PSP-EU's whole tables, which would also undo the European PSX
   disc's 12 other ability names?
5. **The PSP content left out** (section 3): the logo (P13), title page
   (P14), button labels, `SCENA17`, Ryu's form data (P8), the level table
   and consumable 87 - any wanted as layers?
6. **A preset with layers on** ("PC + PSP extras")?
