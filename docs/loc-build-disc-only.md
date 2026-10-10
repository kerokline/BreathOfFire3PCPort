# A language layer from the disc alone: `loc_build.py --cache`

**Status:** IN PROGRESS (verified 2026-10-10, a local session without the game
running: built and diffed for all five held PlayStation releases; the engine
does not read a cache yet, so nothing here has been played - section 7)

[`importer-transforms.md`](importer-transforms.md) section 8 item 3 stated the
last gap between a US disc and a playable game: `tools/loc_build.py` built
`loc/<tag>/` against the PC install's `DAT/` and `BOF3.exe`, so
`importer.py build --lang` refused without them, and a disc-only cache had no
font and no text. This doc is the closing of that gap, as far as the evidence
goes: what was built, what was measured, what the disc-only layer gives up
against the PC-built one and why, and what is still missing.

**The result in one line.** From the US disc alone, `loc/en-US/` is 245
containers of which 213 are byte-identical to the PC-built layer's; every
difference is one of three, each explained below: the font's 2,376 Chinese
glyphs (blank) and the second battle suffix's two glyphs, sixteen Japanese
template messages in 31 areas (empty, where the PC build keeps the port's
Chinese), and the faerie village's board sheet (not built). The bar -
byte-identical to today's - is met wherever the two sources carry the same
bytes; it cannot be met where the PC build's bytes are the port's own.

## 1. What was built

`tools/loc_build.py all --disc DISC --cache CACHE [--lang TAG]` builds a
language layer against an importer cache made from a disc
(`importer.py build --source DISC --out CACHE`), and writes it to
`CACHE/loc/<tag>/dat/<NAME>.DAT` - the layout `importer.py` writes and
`importer.py install` reads. `importer.py build --lang TAG` runs it itself when
neither the PC's `DAT/` nor its `BOF3.exe` is a source (`build_languages_disc_only`);
`base/exe/` is now built before the languages so it is there to read. With
both PC sources given, nothing changed: the PC path is the same code behind an
object, and its output is byte-identical to the parent commit's for every tag
(section 5.1).

The three parts the gap named:

- **(b) `base/` and the disc instead of `DAT/`.** The PC side is an object:
  `PcInstall` (the install) or `DiscCache` (the cache). `DiscCache.chunks`
  lists a container's chunks in the recipe's order (`recipes/pc-zh.toml`),
  with the bytes from `base/dat/` for a `base` chunk and none for a
  `loc/zh-CN` one - the area text, the pools, the font, the text CLUT, the
  title page, the village sheet, the plates' pages. Each converter says what
  it does without the PC's bytes (section 4).
- **(c) The exe tables from `base/exe/`.** `DiscCache.exe` reads
  `base/exe/data.bin`; `DiscCache.carried` says whether every byte of a range
  was filled from the disc (`map`, `table`, `widen`, `exe` in `data.toml`) or
  is a zero (`pointer`, `none`). The converters only use the exe's bytes as
  anchors - the numbers and surrounding bytes the PC and the disc share, by
  which each finds its donor's table on the disc - plus the fallback names of
  a record that does not convert. Where an image does not carry an anchor,
  the anchor is found by its hash (section 3).
- **(a) A font from the disc's sheets alone.** The same appends over a blank
  table of the shipped size (`blank_table`, 2,451 glyphs), the second battle
  suffix from the disc's own cells, and for a Japanese disc the single-byte
  slots its script shares with ASCII painted from its sheet (section 4.1).

## 2. Measured: what a cache from the US disc holds, and what `loc_build` needs

```
python tools/importer.py build --source "CDImage/Breath of Fire III (USA).cue" --out <scratch>/cache-us-only
```

(2026-10-10, before any change here.) It wrote `base/` 727 of 741 containers
(the 14 port-edited arenas missing, `pc-edit`), `base/exe/` from `psx-us`
(`data.bin` sha256 `8f950c6c...`), `base/snd/` 880 files, `base/bgm/` 247,
`opt/area4-walls`; and no `loc/` at all - `loc/zh-CN` lacks 274 text chunks,
200 enemy-name sets and the font. Compared chunk by chunk with the PC's
`DAT/` (a scratch script, `cmp_base.py`): 474 containers
identical, 253 differing, every difference a `loc/zh-CN` chunk absent from
`base/` (199 area text blocks, 43 pools, 46 kind-1 pages, the font) or a
by-design difference (106 enemy tables with their names blank; the plates' and
atlases' disc stand-ins).

What `loc_build all` reads from the PC side, and where the cache has it:

| What | PC build reads | In a disc-only cache | Disc-only build |
|---|---|---|---|
| area text blocks (200), pools (44) | the PC's block: its slot count, and its message for every slot the donor's does not convert | no (loc/zh-CN) | the donor's own table; such a slot an empty message (4.2) |
| enemy tables (200) | the PC's records' numbers (a check) and names (whether a record is named) | base/, names blank; 8 areas lacking from an EU / FR / DE cache (their banks) | base/'s table, or the donor's widened as the importer widens it (`importer.widen_enemies`) |
| the font, `FIRST.DAT` kind 3 | the port's 2,451-glyph table, appended to | no (loc/zh-CN) | a blank table of that size (4.1) |
| the text CLUT, `FIRST.DAT` 0x8000 | the PC's strip with the disc's row 0 | no (loc/zh-CN) | the disc's whole strip: the strips differ in row 0 alone (US: rows 1..15 equal), so the same bytes |
| the title page, `START.DAT` 0x1C000200 | its size (a check) | no (loc/zh-CN; `START` also `pc-edit`) | the recipe's size |
| the village sheet, `COMMU01` / `COMMU05` 0x1C080200 | the PC's sheet, the label rows from the disc | no (loc/zh-CN) | not built (4.3) |
| the plates' four sections, 10 world maps | sizes and the frame header (checks) | the page not; 0xB0000 / 0xA000 as the disc's own stand-ins | the recipe's sizes; the header check is the shape's |
| 37 anchors in `.data` | `BOF3.exe` | `base/exe/data.bin` where carried | the image's bytes, or the anchor's hash (3) |

## 3. The exe anchors

Every exe read `loc_build` makes was logged during a PC build (a wrapper round
`exe_bytes`, 29 distinct ranges) and compared with the US image: 16 identical,
13 differing. Of the 13, seven differ only where the converter does not look
(the six name tables' blank names, the character records' names), one is a
pointer table (below), and five are anchors the US image does not carry at
all:

| Anchor | PC VA, bytes | US image |
|---|---|---|
| the battle stats' tail (`LABEL_BATTLE_TAIL`) | `0x66B5B4`, 16 | `none` |
| the gene window's head (`LABEL_GENE_HEAD`) | `0x66AF38`, 20 | first two bytes `none` |
| the formations' s16 pairs (`LABEL_FORMATIONS`) | `0x6636B0` + 28i + 16, 10 x 12 | records 1..9 `none` |
| the faerie traits' stats (`FAERIE_TRAITS`) | `0x653210` + 20r, 60 x 4 | `none` (59 of 60 differ) |
| the fishing lines' label and pause bytes (`FISH_LINES`) | `0x653B98` + 8i + 4, 13 x 2 | `none` / `pointer` |

and one check that cannot be made: the battle messages' pointer table
`0x669DE0` is twelve pointer words, which no disc carries (the image leaves
them zero); the check that they are `MESSAGE_SHIPPED` is `BOF3.exe`'s to make,
and the DLL's patch is what reads those addresses.

The bytes are on every disc (the PC build finds them there), but no exe map
or table of `tables.toml` puts them into the image. So every anchor is held
twice: as its PC ranges (`anchor_pieces`, 37 names: 25 for the converters'
tables, and two per name table - record 1's numbers, which locate it, and
every record's, which confirm it) and as the
SHA-256 of the PC's bytes there (`PC_SHA`) - never the bytes (`CLAUDE.md`
rule 1; the owner's stance of 2026-10-10 on Capcom's tables in code). Where
`DiscCache.carried` says the image has every piece, the bytes are used; where
not, `find_hashed` slides over the one file the converter searches for the
window that hashes to it (the whole US build, searches included, takes about
four seconds). A build against the PC install recomputes every
hash first (`check_anchors`), so a wrong entry cannot survive a PC build;
`FAERIE_UNNAMED = (59,)`, the one trait record whose PC name field holds no
glyph string, is checked the same way.

Which anchors each held build's image carries (`carried.py` over the five
single-disc caches; "ok": carried and equal to the PC's, "-": not carried;
no image carried a wrong one):

| | psx-us | psx-eu-en | psx-fr | psx-de | psx-jp |
|---|---|---|---|---|---|
| battle tail, gene head, formation pair 0 and pairs, faerie stat 0 and stats, fish label/pause (7) | - | - | - | - | - |
| label tail (`LABEL_TAIL`) | ok | ok | ok | - | ok |
| village rows (`LABEL_VILLAGE_ROWS`) | ok | ok | ok | ok | - |
| the other 28 | ok | ok | ok | ok | ok |

The first version used the hashes only for the US gaps; the German-only build
then lost the status words and menu stats (labels groups 1 and 2: `label
tail` not carried, the anchor read as zeros, found nowhere). That is why
every anchor goes through `find_anchor` / `same_anchor` now.

## 4. What the disc-only build does without the PC's bytes

### 4.1 The font

The kind-3 table: the PC build appends the Latin cells at `0x993` to the
port's 2,451 Chinese glyphs and paints 75 single-byte slots (`ASCII_OF`); the
disc-only build does the same over 2,451 blank glyphs. Measured (`font_cmp.py`,
en-US): 2,378 of 2,671 glyphs differ - the 2,376 port glyphs below `0x993`
not repainted (blank), and `0xA6D` / `0xA6E`, the second battle suffix. The
kind-4 advances are identical. The same holds for en-150, fr-FR (2,743
glyphs) and de-DE (2,740).

The second suffix (`SUFFIX_GLYPHS` `0x52`, `0x53`): the PC build keeps the
port's two glyphs, as the disc's cells "were not identified". The disc-only
build has no port glyphs, so it takes the cells the disc's own mapper gives
its codes: a `0x15 nn` code is cell nn + `0x5B` on the 21-wide 12 px grid
(the sibling's `docs/TEXT_ENGINE.md`, `0x80151F4C`), which puts EX's
`0x151B` / `0x151C` exactly at `EX_CELLS` (156, 60) and (168, 60) - the check -
and the second suffix's `0x151F` / `0x1520` at (204, 60) and (216, 60)
(`SECOND_CELLS`). Neither shape is identified; on the Japanese page `0x15 0x1F`
is 業 (sibling `TEXT_ENGINE.md`), and the cell at (204, 60) of the US atlas
looks like a kanji too. What the US PlayStation draws for message 3 was not
looked at.

A Japanese disc: the PC build keeps the port's single-byte glyphs (its own
half-width digits, letters and punctuation, rendered from `FIRST.DAT`) and
paints only the two hanging brackets. A blank table would leave every
single-byte string of the exe - its numbers among them - blank, so the
disc-only Japanese table paints the 42 slots whose code means the same in the
JP script and in ASCII from the JP sheet (`JA_SHARED`: `( ) , . /`, the
digits, `=`, the capitals; `0x2D` is the long-vowel bar there). 2,448 of 3,570
glyphs differ from the PC build's: 42 painted from the disc, 2,406 blank.

### 4.2 The text

The PC build takes a block's slot count from the PC's block and, where the
donor's message does not convert (no glyph for a code) or the donor's slot
points outside its block, keeps the PC's message - the port's Chinese. The
disc-only build has no PC block: the slot count is the donor's own (the first
offset / 2), and such a slot is an empty message. Measured with
`slotn.py`: the PC's 200 area blocks are 187 of 256 slots, 11 of 677 (the
faerie village, AREA175..185), AREA009 of 14 and AREA031 of 4; the US, EU and
JP discs' counts are the PC's in every block; the French disc's differ in 15
areas and the German's in 11 (the "15 FR and 11 DE areas" `convert_block`
already noted), one of them FR AREA004, whose block does not parse at all
(section 4.5).

### 4.3 The faerie village's board sheet

DIV-0088's sheet is the PC's sheet with the label rows taken from the disc.
The disc's sheet differs from the PC's outside the label rows too - rows
96..175, the board's frame pieces the port's draws place (2,136 bytes in each
of `COMMU01` and `COMMU05` on the US disc, `misc.py`) - and a disc-only cache
has no PC sheet. So no sheet is built: the disc's whole sheet would put the
PlayStation's frame pieces where the port's draws expect its own. The build
says so (`not built disc-only`).

### 4.4 Enemy names

The French, German and EU discs alone leave eight areas (AREA100, 112, 135,
138, 147, 168, 170, 188) out of `base/` for their banks (`containers not
written: base 8 bank` in the import log). Their enemy names are built from the
donor's table widened exactly as the importer widens it into `base/`
(`importer.widen_enemies`), so the names are there for when the container is:
identical to the PC build's.

### 4.5 FR AREA004

The French disc's `AREA004.EMI` text section (`0x80010000`, 1,008 bytes)
begins `2a 31 3d 3f 3d 3f ...` - no slot table; the US section of the same
dest begins `00 02` (256 slots). The PC build reads the PC's 256 slots out of
it, finds every one outside, and keeps the port's whole block. The disc-only
build cannot take a slot count from it and skips it (`SKIP AREA004.DAT tag 0`),
so a French disc-only layer has no text for area 4 and the cache's
`AREA004.DAT` none either. (Area 4 is DIV-0080's minecart area; why the French
master differs there is not read.)

## 5. The diff, container by container

Every build in scratch, with `pc_build.py` (the PC path, against a scratch
game linked to `bof3/`; nothing written to `bof3/DAT/`) and the disc-only path
into caches from one disc each plus one from US + FR + DE (`--lang en-US
--lang fr-FR --lang de-DE`, so the French and German title menus have the
English CONFIG row to borrow). Compared with `cmp_sum.py` (containers, then
chunks by kind and tag) and, for the text, slot by slot against the PC's own
blocks and the donor's (`text_exact.py`).

### 5.1 The PC path is unchanged

`git archive` of the parent commit's `tools/`, `fixtures/`, `recipes/` into
scratch, then both trees' `loc_build all --game` for each tag: **en-US 247/247,
en-150 247/247, fr-FR 247/247, de-DE 247/247, ja-JP 245/245 containers
byte-identical.** (A first run against the parent's importer-built
`loc/en-US/` gave the same 247/247.)

### 5.2 PC-built against disc-only

| Tag, disc-only cache | Containers (PC / disc-only) | Identical | What differs |
|---|---|---|---|
| en-US, US alone | 247 / 245 | 213 | font; 31 area blocks; COMMU01 / 05 not built |
| en-US, US + FR + DE | 247 / 245 | 213 | the same |
| en-150, EU alone | 247 / 245 | 213 | the same as en-US |
| fr-FR, US + FR + DE | 247 / 245 | 197 | font; 46 area blocks; AREA004's block (4.5); COMMU01 / 05 |
| fr-FR, FR alone | 247 / 244 | 196 | as above, and no `START.DAT`: no English CONFIG row to borrow (`build_title`, "left as shipped") |
| de-DE, US + FR + DE | 247 / 245 | 202 | font; 42 area blocks; COMMU01 / 05 |
| de-DE, DE alone | 247 / 244 | 201 | as above, and no `START.DAT` |
| ja-JP, JP alone | 245 / 245 | 202 | font; 42 pools |

Every other chunk - the config screen, verbs, character and merchant names,
fishing, battle commands and messages, all sixteen label groups, the six name
tables, 448 enemy names (FR 408, DE 394), the pause lines, the CLUT, the title
page, the plates - is byte-identical in every row.

The text, slot by slot (`text_exact.py`, every differing block of every tag):

| | en-US | en-150 | fr-FR | de-DE | ja-JP |
|---|---|---|---|---|---|
| PC keeps the port's message, disc-only empty | 496 | 496 | 497 | 496 | 42 (pools) |
| PC keeps the port's message where the disc's converts | | | 34 | 11 | |
| PC slot past the disc's own table (a fragment, or the port's) | | | 649 | 340 | |
| disc's table longer: extra slots (empty / with text) | | | 35 / 3 | 27 / 3 | |
| unexplained | 0 | 0 | 0 | 0 | 0 |

- **The 496.** Slots 0..15 of 31 areas (AREA006, 025, 029, 036, 063, 064, 070,
  072, 073, 076, 084, 093, 102, 106, 109, 110, 124, 125, 127, 137, 138, 139,
  146, 156, 158..163, 190): sixteen Japanese messages the Western discs carry
  unconverted (they open `12 90 01 2a`, `e3 d2 fc 01 2a`, `cd fc e4 01 2a` -
  JP codes). The PC build keeps the port's Chinese for them; disc-only, a
  message is empty.
- **The 42 Japanese pools' slots** point outside their blocks on the JP disc
  (`ja_pool.py`: all 42 "outside"), in BATTLE, BATTLE2 and the BOSS overlays.
- **34 French, 11 German: the PC build is the worse one.** Where the disc's
  table is shorter than the PC's 256 (FR AREA018 43, AREA030 / 089 / 129 148,
  AREA067 176, AREA094 224; DE AREA030 / 089 / 129 148, AREA191 240), the PC
  build tests each donor offset against 2 x 256, so the disc's first few
  messages - which start before byte 512 - read as "outside" and keep the
  port's Chinese: FR AREA030 slot 0 is `Méduse facile ...` disc-only and Chinese in
  the PC build. Four of them (FR AREA018 slots 6 and 11, AREA094 slot 1, DE
  AREA191 slot 1) carry another slot's Chinese: they share a donor offset
  with a slot kept first, and `placed` maps the offset to that slot's message
  (seen side by side for AREA018 and AREA094). The disc-only build converts
  all of them. This is a defect of the PC path, measured here and not changed
  (section 7).
- **Past the disc's table**, the PC build reads the donor's message bytes as
  offsets: 649 French and 324 German slots become a fragment of some message,
  16 German keep the port's. The French and German PlayStation releases have
  no such slots; whether the PC's event scripts name them is not read.
- **Longer tables**: the French and German discs append slots in 8 and 7
  areas (AREA007 263, AREA009 19, ...); the disc-only blocks keep them, empty
  but three in AREA154 that carry the disc's text.

## 6. Can the game tell?

Nobody can play a disc-only cache yet: the engine still reads the PC's `DAT/`
and runs the player's `BOF3.exe` (section 7). For when it does:

- **The blank Chinese glyphs** are reached only by text no layer replaces. The
  overlay's own text never reaches them (it names the painted slots and the
  appended glyphs only). The exe's Chinese strings are `none` in a disc's
  image - zero bytes, empty strings - so there is no string left to draw them.
  Not enumerated: glyph indices the exe draws directly rather than through a
  string.
- **Fifteen single-byte slots stay blank in a Latin table**: `+ < > @ [ \ ] ^
  _ \` { | } ~` and 0x7F, which `ASCII_OF` does not paint and which the PC
  build shows as the port's half-width glyphs. An exe string using one would
  show a gap. Not enumerated.
- **The second suffix** draws the disc's cells instead of the port's whenever
  banner message 3 shows.
- **The 496 empty messages**: the Western releases shipped them as Japanese
  bytes their fonts cannot draw, which suggests no route reaches them; not
  proven. The 42 Japanese pool slots point outside their blocks on the
  PlayStation itself.
- **The French and German slot tables**: the disc-only blocks are the discs'
  own; a slot past them that the event script names would read past the table.
  The French and German PlayStation releases run on these tables.
- **The village sheet, FR AREA004's text, and a French- or German-only
  cache's title page** are absent: the containers lack the chunk entirely
  (section 7). That the game can tell.

## 7. What is left

1. **The `loc/zh-CN` chunks no disc-only layer replaces.** A disc-only cache's
   containers are `base/` plus a language layer; these chunks are in neither,
   so the containers lack them (`zh_cover.py` against the PC-built en-US
   layer): the village sheet (`COMMU01`, `COMMU05`, 4.3); 11 glyph atlases
   (kind 1 `0x1C080200` in AREA030, AREA089, AREA129, BATE, COMMU02B, FIRST,
   SHISU, SHOP, SISYOU, START, STATUS); the two kanji sheets (`0x1E000200` in
   ENDKANJI and FIRST); four area pages (`0x0E001000` in AREA104, 127, 134,
   164). The PC build never replaces them either - over the PC install the
   port's Chinese art shows. Each is a disc section of the same dest that
   could stand in (`own`, as the plates and atlases of
   `importer-transforms.md` section 5 do) once someone reads what the port's
   draws expect of it, as the village sheet's rows 96..175 show they can
   differ.
2. **FR AREA004** (4.5), and **a French- or German-only cache's title page**:
   the CONFIG row comes from an English layer (the owner's choice of
   2026-09-29); without an English disc there is none. Drawing CONFIG from
   the French or German sheet's own letters is the open alternative.
3. **The PC path's misread slots** (5.2, 34 French and 11 German): testing a
   donor offset against the donor's own table end instead of the PC's would
   give the French and German players those messages in their language. It
   changes today's output, so it wants a ledger entry and the owner's eye.
4. **The slot counts.** The recipe could carry each text block's slot count
   (an index, not game bytes) so a French or German disc-only block could be
   padded to the PC's; not done, since the discs' own tables are what those
   releases run on.
5. **The engine reading the cache** (`LoadDatFile` on `base/dat/` +
   `loc/<tag>/dat/` + `opt/`) and the engine half of step 8 (`base/exe/` mapped
   where `.data` was, [`exe-import.md`](exe-import.md) 6) - until both, a
   disc-only layer is built and diffed but cannot be played. Then the
   self-tests under it, and the owner's eye on the second suffix and the
   blank slots.
6. **Found on the way, not touched:** in `loc_build.py`, `convert_verbs`,
   `convert_char_names`, `convert_merchant` and `convert_battle_commands`
   test `tag == 16` (copied from `label_chunk`) where no `tag` is defined - a
   `NameError` instead of their message, on an empty string only (pyflakes);
   and `importer.py verify` on a US-only cache reports `opt/area4-walls`
   `AREA004.DAT slot 6: composed with the layer, not the recipe's` on the
   parent commit too.

## 8. Commands

Game data stays out of the repo: every cache and build below went to the
session's scratch directory.

```
# a cache from one disc, its language layer built disc-only
python tools/importer.py build --source "CDImage/Breath of Fire III (USA).cue" --lang en-US --out <cache>
# or against an existing cache
python tools/loc_build.py all --disc "CDImage/Breath of Fire III (USA).cue" --cache <cache> --lang en-US
# the anchors' hashes anew from the PC's exe, and whether PC_SHA agrees
python tools/loc_build.py anchors --game bof3
```

The scratch scripts that measured this (`cmp_base.py`, `trace_exe.py`,
`carried.py`, `font_cmp.py`, `cmp_sum.py`, `text_exact.py`, `slotn.py`,
`zh_cover.py`, `misc.py`) are not committed; each is a few lines over
`tools/dat.py`, `tools/font_pc.py` and `loc_build`'s own functions, and the
tables above say what each counted.
