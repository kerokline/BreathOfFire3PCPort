# A language layer from the disc alone: `loc_build.py --cache`

**Status:** IN PROGRESS (verified 2026-10-10, a local session without the game
running: built and diffed for all five held PlayStation releases; the engine
reads a cache since DIV-0089 and plays disc-only since PR #55, but nothing here has been played - section 7.
**2026-10-12, the disc-only round's LG group** (branch `disc-only/language`):
the gaps of section 7 items 1, 2 and 6 closed by rule, measured in 4.3,
4.5..4.7, 5.3 and 5.4; still not played)

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

**2026-10-12.** A disc-only layer now also carries every `loc/zh-CN` image
the PC-built layer leaves to the port's files (section 4.6): the port's
glyph atlas and kanji sheet turned out to be the PlayStation's sheets
re-laid by a rule of two to nine rectangles and one strip of icons, which
the build applies to the disc's own; the four area pages are the disc's
own. The village sheet is built again (4.3), French `AREA004` has its text
(4.5), and a French or German disc alone draws CONFIG from its own sheet
(4.7). From the US disc alone: 254 containers, 381 of the 475 `loc/zh-CN`
chunks replaced and every other one by design (5.3).

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
| the dock's legend, `AREA128` 0x0A081000 (DIV-0190) | the PC's page, the panel's word columns from the disc | no (loc/zh-CN) | not built: the install's page is the disc's own |
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
The disc's sheet differs from the PC's outside the label rows too - "rows
96..175" counted as 128-byte lines (2,136 bytes in each of `COMMU01` and
`COMMU05` on the US disc, `misc.py`) - and a disc-only cache has no PC sheet,
so until 2026-10-12 no sheet was built.

*Revised 2026-10-12.* Read as pixels (the chunk is 32 x 32-halfword tiles,
two across: a 128-byte line is two half rows, so those "rows" were pixel
rows 96..191 of both tiles), the difference is not the board's frame pieces:
the PlayStation's sheet has twelve empty framed cells there, 40 x 48 each,
and the port cleared them and laid a strip of eight of the sheet's own
16 x 16 icons, copied from the icon row at y 208, at a pitch of 20 from
(0, 100). It is the same sheet as the glyph atlas of section 4.6, so the
same rule (`RELAID`, `relay_sheet`): the disc-only sheet is the disc's
section re-laid. From the US disc it is **byte-identical to the PC-built
layer's** (DIV-0088's sheet: the PC's with the disc's labels); from the
French and German discs it also takes those discs' repaints outside the
rule (669 and 455 pixels, 5.3), where the PC-built sheet keeps the port's.

### 4.4 Enemy names

The French, German and EU discs alone leave eight areas (AREA100, 112, 135,
138, 147, 168, 170, 188) out of `base/` for their banks (`containers not
written: base 8 bank` in the import log). Their enemy names are built from the
donor's table widened exactly as the importer widens it into `base/`
(`importer.widen_enemies`), so the names are there for when the container is:
identical to the PC build's.

### 4.5 FR AREA004

The French disc's `AREA004.EMI` text section (`0x80010000`, 1,008 bytes)
begins `2a 31 3d 3f 3d 3f ...`; the US section of the same dest begins `00 02`
(256 slots). Until 2026-10-12 the disc-only build could not take a slot count
from it and skipped it (`SKIP AREA004.DAT tag 0`).

*Revised 2026-10-12.* Only the first seven words are bad: `0x312A` and six
`0x3F3D`, all past the block's end. From slot 7 on the table is an ascending
one whose first offset, `0x200`, puts its end at 256 slots, the US and
German count. On the other discs the head slots all point at the first
message: the German table's slots 0..7 at `0x200`, the US's slots 0..2 at
`0x200` and 3..7 at offsets two bytes apart into that same message. So the
rule (`table_after_bad_head`, used only when the first offset's table does
not fit): the count is the smallest in-block offset before it / 2, and the
bad head slots point at the first message. French `AREA004` disc-only: 256
slots, 33 with text, the same as the US layer's count. Against the PC-built
`fr-FR` layer: slots 7..255 the same message (the PC path converts them too:
the "keeps the port's whole block" above was wrong), slots 0..6 the first
message disc-only where the PC build keeps 464- and 440-byte messages of the
port's own block. Why the French master's first fourteen bytes differ is not
read.

### 4.6 The images no layer replaced: the port's re-laid sheets and the disc's own

Section 7.1 listed eighteen `loc/zh-CN` images the PC-built layer leaves to
the port's files and a disc-only cache therefore lacks: eleven glyph atlases
(`0x1C080200`), the kanji sheet of `ENDKANJI` and `FIRST` (`0x1E000200`), and
the area pages of `AREA104`, `127`, `134` and `164` (`0x0E001000`), with the
village sheet (4.3) the atlas of two more. Each was compared with the same
dest's section on all five discs (2026-10-12, scratch `standin_cmp.py`):

| Image | Containers | Port's chunk against the JP disc's | JP against US / EU | against FR / DE |
|---|---|---|---|---|
| glyph atlas, VRAM (896, 256) | 13 (one image) | pixel rows 96..191 (twelve empty framed cells cleared, eight icons laid) | the three village labels (y 226..252) | the labels and more words (y 48..59, 165..207) |
| kanji sheet, VRAM (960, 0) | 2 (one image) | rows 72..247 (the PlayStation's glyphs cleared, twelve icons laid at y 80, their source row cleared) | y 17..21, 60..71 | and y 208..231 |
| area pages | 4 | 5, 7, 8 and 7 of 128 tiles (the port's painted words) | equal | equal |

**The two sheets are a rule, not art.** Every pixel where the port's sheet
differs from the JP disc's is either zero (cleared) or a 16 x 16 icon of the
same sheet moved: the PlayStation keeps a row of icons at a pitch of 16, the
port lays a copy of them at a pitch of 20 for its draws. Found by matching
each changed component against every position of the JP sheet (scratch
`derive.py`): the atlas is two clear rectangles and a strip of eight icons
from (120, 208) to (0, 100); the kanji sheet nine rectangles (a greedy cover)
and a strip of twelve from (56, 232) to (0, 80). `RELAID` in `loc_build.py`
holds those coordinates - the port's layout, no pixel of either sheet.
Applied to the JP disc's section the rule gives the port's chunk **byte for
byte**; to the US / EU disc's, the port's but for the disc's own repaints
outside the rule (atlas 612 pixels, the village labels; kanji 205); to the
French and German discs', the port's but for theirs (atlas 1,625 / 1,122,
kanji 1,089 / 931). The icon strips' source cells are equal on all five
discs. **The check**: a PC-path build compares the rule's output from the
disc with the PC's chunk on every pixel the rule writes and stops on a
difference (`check_relaid`): all five discs pass.

**The stand-ins** (`build_standins`, disc-only only): every `loc/zh-CN`
image of the recipe that no builder above wrote is the disc's section of the
same dest and size, re-laid where `RELAID` names its tag. Sixteen on every
Western disc: 10 atlases, the two kanji sheets, the four area pages (and the
two village sheets through 4.3). An overlay walks after its container's base
chunks, so a stand-in must be what the PC's slot order leaves: `FIRST.DAT`'s
atlas (slot 2) is wholly covered by base slot 10 (`0x1A080400`, VRAM 832..959
x 256..511), so the PC's end state holds none of it and the stand-in is left
out; a stand-in only partly covered by a later slot would stop the build
(none is).

What a player sees from a disc-only cache, against the PC install:

- **the atlas and kanji sheet** (eleven containers and the village's two):
  the port's layout; the disc's own paint where the disc repainted
  (the village labels on every Western disc - the board draws them only in
  `COMMU01` / `COMMU05`, whose sheet DIV-0088 already takes from the disc -
  and, from the French and German discs, the words those discs repainted).
- **the four area pages**: the PlayStation's paint, which is the same on all
  five discs, where the PC shows the port's (painted words, by
  `importer.md`'s class "an area page (words painted on)"). The US
  PlayStation shows the same pages. What the words are was not looked at.

The engine's start-up check of a disc-only cache (IN's) decides whether a
container is held: with these, base + layer carries every `loc/zh-CN` chunk
of the recipe but `FIRST.DAT`'s covered atlas (5.3).

### 4.7 The CONFIG row of a French or German disc alone

The French and German `START.EMI` title sheets carry the US sheet's NEW GAME
/ LOAD GAME bands whole, 128 rows down: rows 128..159 of both equal the US
sheet's rows 0..31 texel for texel (2026-10-12, scratch `letters.py`; the US,
EU and JP sheets have them at rows 0..31). So `build_title` finds the
measured letters on any sheet by that band's hash (`TITLE_LETTERS_SHA256`,
`title_letters`) and cuts CONFIG from the disc's own sheet, as it does from
the US's; the English layer's page is borrowed only from a sheet without the
band (none of the five). The row is the same glyphs, so the test is the
diff: the French and German pages and widths are **byte-identical** to the
PC-built `fr-FR` and `de-DE` layers' (which borrowed the English row), and
a French- or German-only cache now has its title page.

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

### 5.3 2026-10-12: the stand-ins, the village sheet, CONFIG, FR AREA004

**The PC path is unchanged.** `git archive` of the parent commit `6e6b4f3b`'s
`tools/`, `fixtures/`, `recipes/`, and both trees' `loc_build all --game`
against two scratch games hard-linked to `bof3/`, each tag's disc in turn:
**en-US 247/247, en-150 247/247, fr-FR 247/247, de-DE 247/247, ja-JP 245/245
containers byte-identical**; the new tree's run also passes `check_relaid` on
all five discs (and draws the French and German CONFIG from their own
letters, identically).

**Disc-only against PC-built**, each cache from one disc (`importer.py build
--source <cue> --lang <tag>`, then `loc_build all --cache`):

| Tag, cache | Containers (PC / disc-only) | Identical | What differs, beyond 5.2's |
|---|---|---|---|
| en-US, US alone | 247 / 254 | 208 | 7 new containers (`BATE`, `COMMU02B`, `ENDKANJI`, `SHISU`, `SHOP`, `SISYOU`, `STATUS`: the stand-ins); the stand-ins in `AREA030`, `089`, `104`, `127`, `129`, `134`, `164`, `FIRST`, `START`; `COMMU01` / `05` now built and identical |
| fr-FR, FR alone | 247 / 254 | 193 | as en-US; `START.DAT`'s title page and widths identical; `AREA004` (4.5); `COMMU01` / `05` built, 669 pixels the disc's |
| de-DE, DE alone | 247 / 254 | 198 | as fr-FR; `COMMU01` / `05` 455 pixels the disc's |

(5.2's 213 identical containers for en-US less the seven areas that now
carry a stand-in beside their text, plus the two village sheets, is 208.)

The three caches rebuilt end to end (`importer.py build --source <cue> --lang
<tag>`, then `verify`): `loc/<tag>` 254 containers each, identical to the
layers measured here; `base/` 727 (US), 719 (FR, DE) containers held, 0
problems; the run's one problem is `opt/area4-walls`' slot 6 (7.6, the
parent's too).

**The `loc/zh-CN` chunks a layer replaces** (`loc_build.py cover`, the
method of 7.1's `zh_cover.py` as a command; a kind-1 image wholly covered by
a later base slot counts as needing none):

| | US before | US after | FR before | FR after | DE before | DE after |
|---|---:|---:|---:|---:|---:|---:|
| of 475 | 363 | **381** | 361 | **381** | 362 | **381** |
| area pages (14) | 10 | 14 | 10 | 14 | 10 | 14 |
| glyph atlases (12, and `FIRST`'s covered one) | 0 | 12 | 0 | 12 | 0 | 12 |
| kanji sheets (2) | 0 | 2 | 0 | 2 | 0 | 2 |
| area blocks (199) | 199 | 199 | 198 | 199 | 199 | 199 |
| title page (1) | 1 | 1 | 0 | 1 | 0 | 1 |
| enemy names (200 tables) | 106 | 106 | 106 | 106 | 106 | 106 |

The 94 enemy tables left are the PC-built layer's too: their records carry
no name to write (`convert_enemy_names` writes a chunk per named record).
Pools 44 of 44, the font and the text CLUT 1 of 1, in every column.

### 5.4 The exe image's language bytes from a US disc alone

[`exe-import-engine.md`](exe-import-engine.md) 3.2 counted 183 addresses `src/`
reads in `0x669CCC..0x66A450` and `0x6531F4..0x6536E0` that no disc carries
(JP). Re-measured on the US disc (`exe_tables.py xref`, 2026-10-12): **182**,
all `none`. A language layer does not write the image (`base/exe/data.bin`);
its FIRST.DAT chunks are applied into `.data` at load by the engine's
handlers (`dat_load.cpp` `TakeChunk`, kinds 5..16). Read handler by handler
against the 182 (a read-only pass over `src/`, `analysis/pc_xref.json` and
the image; per-address table in scratch `lang182_class.tsv`), with the US
layer of 5.3, which carries every one of those chunks:

| | Addresses | How |
|---|---:|---|
| written in place by a handler | 83 | labels groups 1, 2, 5, 6, 8, 9, 11, 12, 16 (`labels.cpp` 296-297); battle commands `0x669D28` and the verbs `0x66A228..` (`menu_verbs.cpp` 112-114); the merchant `0x669CD8` and the whelp's slot `0x669CE0` (`char_names.cpp`) |
| read through ours, the bytes unread | 91 | re-aimed pointer tables (battle messages `0x669D7C..0x669DD8`, labels groups 3, 4, 7, 13, fishing, the controller names `0x66A338..`); patched operands (the config row labels `0x669F0C..0x669F3C`, the pause lines `0x66A3F0`, `0x66A404`, `0x66A420`, `0x66A434`); our draws through `Labels_Slot` (groups 14, 15); `0x66A1F8` not drawn under a Latin overlay (DIV-0083) |
| **stay zero** | **8** | below |

The eight, by what reads them and what a zero shows (none crashes):

| Address | What | Read by | A zero |
|---|---|---|---|
| `0x6531F4` | the format string `%d/%d` | `CommuRank_Show` (`rest_4e.cpp`, `kPageFormat`) via sprintf | the ranked lists' page counter blank. The engine's own by meaning - a format string - so NB's class, not a language layer's |
| `0x653200` | faerie record 0's name field (base of the 60 x 20 records: a 16-byte name, then four stats) | `CommuSim_AddRecord` (`rest_4a.cpp` 747), the entry's default name (`rest_4d.cpp`) | faerie 0 nameless; see the defect below |
| `0x653210` (+ 20 r) | the faeries' four stat bytes | the community's ticks (`rest_4a.cpp`), the bars (`rest_4b.cpp`, `rest_4e.cpp`) | every bar 0 and the simulation's traits 0. Game values, on the disc (COMMU00's nine-byte records carry the same four bytes, the "faerie stats" anchor), but no chunk kind carries them: an engine handler would be needed |
| `0x653601` (read at + area, `0x6536B0..0x6536BA`) | a byte a faerie-village area (175..185) | `rest_4b.cpp` (five readers) | every area's limit 0: placements refused. Game values; not language |
| `0x669CCC` | the empty save slot's text | `field_o.cpp` 633 | blank |
| `0x669CE8` | a five-byte name `EquipOpen` copies over character record 7's | `battle_e1.cpp` 313 | that name blank until `EquipLeave` restores it from `0x669CE0` |
| `0x66A094` | a one-glyph word after a count | `effect_2g.cpp` 789 | blank |
| `0x66A098` | a menu's title | `rest_3e.cpp` 1127 (sprintf `%s`) | blank |

None is closable in `loc_build.py` alone: a layer reaches `.data` only
through a handler, and these have none. `0x669CCC`, `0x669CE8`, `0x66A094`,
`0x66A098` are text a new label group could carry (a `src/` change, listed
for the coordinator); `0x6531F4` is the engine's own; `0x653210`'s stats and
`0x6536B0..`'s limits are game values a disc carries but no layer does.

**A defect found on the way (labels group 16, DIV-0064), not changed here.**
The PC's faerie record is the name first: `kRecordTraits = 0x653200`,
"+0x10..+0x13 read by the ticks" (`rest_4a_callees.h`), and the image agrees
(a glyph string at `0x653200`, the stats `03 02 02 03` at `0x653210`, the
next name at `0x653214`). `convert_labels` pairs the disc's record r (a
five-byte name, then the stats that equal the PC's record r) with the PC's
stats at `0x653210 + 20 r`, and `labels.cpp` writes name r to `0x653214 + 20 r`
- record r + 1's name field. So faerie r + 1 shows faerie r's name, record
0's name is never written, and the "sixtieth record whose PC name field
holds no glyph string" (`FAERIE_UNNAMED`) is the area-limit table above,
`0x6536B0` (`01 01 02 03 03 04 05 05 06 07 08`): skipping it is what keeps
the write from clobbering that table. The fix is `src/game/labels.cpp`'s
group-16 slots at `0x653200 + 20 r` and the name check at + 0 in
`loc_build.py` (`FAERIE_UNNAMED` empty): a `src/` change and a change of the
PC path's output, so the coordinator's and the owner's. Whether the owner's
fairyVillage route showed shifted names was not looked at.

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
- ~~**The village sheet, FR AREA004's text, and a French- or German-only
  cache's title page** are absent~~ - built since 2026-10-12 (4.3, 4.5, 4.7).
  The stand-ins of 4.6 show the disc's paint in the port's layout; the four
  area pages the PlayStation's paint.

## 7. What is left

1. ~~**The `loc/zh-CN` chunks no disc-only layer replaces.**~~ **Done
   2026-10-12** (4.3, 4.6, 5.3): the stand-ins, the two sheets by rule. A disc-only cache's
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
2. ~~**FR AREA004** (4.5), and **a French- or German-only cache's title page**~~
   **Done 2026-10-12** (4.5, 4.7):
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
6. **Found on the way:** ~~in `loc_build.py`, `convert_verbs`,
   `convert_char_names`, `convert_merchant` and `convert_battle_commands`
   test `tag == 16` (copied from `label_chunk`) where no `tag` is defined - a
   `NameError` instead of their message, on an empty string only (pyflakes)~~
   **fixed 2026-10-12**, with `loc_build.py selftest` (CI) running each on an
   empty string; and `importer.py verify` on a US-only cache reports `opt/area4-walls`
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
# which loc/zh-CN chunks a layer replaces (5.3); the synthetic checks (CI)
python tools/loc_build.py cover --cache <cache> --lang en-US [--layer <dat dir>]
python tools/loc_build.py selftest
```

The scratch scripts that measured this (`cmp_base.py`, `trace_exe.py`,
`carried.py`, `font_cmp.py`, `cmp_sum.py`, `text_exact.py`, `slotn.py`,
`zh_cover.py`, `misc.py`) are not committed; each is a few lines over
`tools/dat.py`, `tools/font_pc.py` and `loc_build`'s own functions, and the
tables above say what each counted.

## 9. Ledger entries to draft (for the coordinator)

**DIV-0097 - A disc-only layer stands in the disc's own images where the
port's art showed.**
- Original: over the PC install every language layer leaves eighteen images
  of `loc/zh-CN` to the port's files - the glyph atlas of eleven modules, the
  kanji sheet of `ENDKANJI` / `FIRST`, the area pages of `AREA104`, `127`,
  `134`, `164` - and a disc-only cache (DIV-0090) had none of them, nor the
  village sheet.
- New: `loc_build.py all --cache` writes each as the disc's section of the
  same dest; the atlas and the kanji sheet re-laid by the port's own rule
  (`RELAID`: rectangles cleared, a strip of the sheet's icons at a pitch of
  20), which gives the port's chunk byte for byte from the JP disc; the
  village sheet the same way. The player sees the port's layout with the
  disc's paint where the disc repainted (US: the village labels; French and
  German: their words, also on the village sheet where the PC-built layer
  keeps the port's), and on the four area pages the PlayStation's paint
  where the PC shows the port's. Over the PC install nothing changes
  (byte-identical, five tags). `FIRST.DAT`'s atlas is left out (a later
  slot covers it on the PC too).
- Why: the disc-only install has no port art; the disc's own is what that
  release shows, laid where the port's draws read.
- Also in the PSX version? The paint is each disc's own; the layout is the
  port's.
- Verification: section 5.3 (`check_relaid` on five discs; the US village
  sheet identical to DIV-0088's). Not played. Owed the owner's eye: the
  four area pages and the French and German sheets.

**DIV-0098 - French `AREA004`'s first seven message slots, disc-only.**
- Original: the French disc's table starts with seven words past its block's
  end; the disc-only build skipped the block (no text for area 4), and the
  PC-built `fr-FR` layer keeps two of the port's messages (464 and 440
  bytes) in slots 0..6.
- New: a disc-only block whose head slots point past its end takes its count
  from the first in-block offset and points those slots at its first
  message, as the German table does (`table_after_bad_head`). Slots 7..255
  equal the PC-built layer's.
- Why: the US and German tables point those slots at the same first message;
  an empty message would be the only other choice.
- Also in the PSX version? On the French PlayStation those slots read past
  the block. Not played; which of slots 0..6 the event script names is not
  read.

**DIV-0090, amend:** "no village sheet, no text for French `AREA004`, no
title page from a French or German disc without an English one" is no
longer so (4.3, 4.5, 4.7); the French and German title pages from their
own sheets are byte-identical to the PC-built layers' (no player
difference, so no entry of its own).
