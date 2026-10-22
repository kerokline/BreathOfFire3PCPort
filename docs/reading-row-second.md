# The second-language layer: `tools/ruby_build.py second`

**Status:** BUILT 2026-10-22 (the reading-row round's group SB, branch
`reading-row/sb`; nothing played - the renderer is group RR's, running
beside this). Every number below is from one run over a copy of the
owner's held layers (`C:\Users\kerok\bof3x-runs\rr_sl\DAT`, copied to
`rr_sb`; the census's install, [`second-language-census.md`](second-language-census.md)
1). The contract is [`reading-row.md`](reading-row.md) section 1 and is
not changed here; the rule built is the census's section 9; the round is
[`reading-row-round.md`](reading-row-round.md) (SB, section 3).

## 1. What it does

`ruby_build.py second --lang <main> --over <second>` reads two held text
layers and writes the optional text layer `ruby-<second>-<main>`
(DIV-0086), for example `ruby-en-US-ja-JP`: English in the reading rows over
the Japanese text played. Per text block it annotates - the kind-0 area
script block (tag 0) of every `AREA*.DAT` and both halves of every message
pool (tag `0x4000`) the main layer carries - one **kind-18 side chunk** of
composed messages keyed by slot, the shape FB's furigana layer has; and in
the layer's `FIRST.DAT`, before the pool's side chunks, **a composed font
chunk** (kind 3, with its kind-4 advances) that replaces the main's table
when the layer is played (section 3). The text blocks are never touched.

    python tools/ruby_build.py second --game G --lang ja-JP --over en-US
    python tools/ruby_build.py second --cache C --lang en-US --over ja-JP [--only AREA014,FIRST] [--out DIR]
    python tools/ruby_build.py second --game G --lang ja-JP --over en-US --selftest
    python tools/ruby_build.py second --game G --lang ja-JP --over en-US --lint FILE --json FILE

`--game G` reads `G\DAT\<tag>.<NAME>.DAT` and writes
`G\DAT\ruby-<second>-<main>.<NAME>.DAT`; `--cache C` reads
`C/loc/<tag>/dat/` and writes `C/opt/ruby-<second>-<main>/dat/`; `--out`
is the folder the DATs go into (setup's step uses it, section 5). The
shipped text (`G\DAT\<NAME>.DAT`, or the cache's `loc/zh-CN/dat/`) is read
only to tell the port's kept messages (section 2). The `--cache` and
`--game` paths write byte-identical files (checked on all 211 files of
`ruby-en-US-ja-JP`). A pair builds in about 7 to 8 s.

The second's own codes are never written as they are where the fonts
differ: every glyph of a reading row is a code of the composed table.

## 2. The pairing rule as built

The census's section 9, rule by rule, with what the builder does where the
draft left room. The calls marked **(SB)** are mine, made without the
owner (the brief); each is one place in the code.

1. **By slot.** The second's message is the same block's same slot. The
   main stays plain when either side is empty or is the port's kept message
   (`kept_pc`: equal to the shipped slot, or a two-byte glyph below
   `0x993` - `second_language_census.is_foreign`'s rule, which also takes
   the French and German out-of-block slots). A slot empty in the main has
   nothing to show.
2. **Box pages only - per message (SB).** A main message with a row past
   the box (192 units, glyph advances, a first `0x2A` / `0x3C` hanging), a
   page past three rows, or the offset pen / effect preset (`0x0D 0x0E
   0x0F`) is left plain **whole**: the stepper knows an annotated message by
   its source and walks every page of it as pairs (reading-row.md 1.2), so
   one plain page inside a composed message cannot be expressed. FB does the
   same.
3. **Pages pair in order when the counts agree** (counting pages with a
   glyph or an insert); otherwise the second's text, its own page and row
   breaks dissolved (a Latin break is a space, a Japanese one nothing), is
   shared over the main's pages in reading order in proportion to their
   glyphs (`share_out`, the census's `share`): a unit goes to the page its
   midpoint falls in. Units are Latin words (cut at spaces) and Japanese
   glyphs, with a small kinsoku: closing punctuation, the long-vowel bar and
   small kana cling to the glyph before, opening brackets to the glyph after.
4. **The main keeps its authored rows when they fit**: a page of one or two
   rows as authored (class A). A three-row page is re-broken into two
   (class B, `rebreak_page`): at spaces for a Latin main (an authored row end
   becomes a space; the break takes one space), between glyphs with the same
   kinsoku for a Japanese one; inserts at their widest budget, the hang
   honoured, every row checked against 192 units. Two refinements **(SB)**:
   - a first row that is **only an insert - the speaker's name** (`04 nn`;
     1,000 of the Japanese layer's three-row pages) is kept as its own row
     when the other two re-break into one; else all three are re-broken and
     the name runs into the text;
   - a page with a row that is **empty or opens on a space** (a blank line,
     an indent, a signature) is not re-broken: its spacing is layout, and the
     page splits on its authored rows instead.
5. **The second over the page.** Greedy reading rows of 192 units at the
   reading scale; when they fit in two and the page has two text rows, the
   second is then **balanced over the two text rows (SB)**: shared by the
   rows' glyphs at word boundaries, so each reading row stands over the text
   it reads with - used when each part fits its one row and its inserts are
   that row's, else the greedy rows stand (10,645 of the 20,095 area box
   pages of English over Japanese). The page count is unchanged by it.
6. **Split only on overflow.** When the main does not fit two rows (three
   authored rows that do not re-break) or the second needs a third reading
   row, the page splits on a `0x02`: three authored rows as rows 1-2 and 3,
   two as 1 and 2, the second re-shared over the parts by their glyphs, and
   again while a part overflows; under a single main row the second's rows
   go two to a page, the main's row under the first and **an empty text
   row** under the others. A split page starts its walk at colour 0
   (`MsgBox_Step`'s local), so its first reading row ends on the colour the
   original drew that row in. A split page that ends on `0x16 nn` (timed)
   would make a self-closing page wait for a press: that message stays
   plain, as in FB.
7. **Choice blocks**: the main's `0x14` block is copied verbatim; the
   second's options are dropped.
8. **Inserts.** The main's stay in its text rows. The second's become the
   marker `0x12` in the reading row, which by the contract stands for the
   k-th insert of the text row below - so the second's insert must be that
   row's k-th insert, same control, same argument. The wrap puts each
   insert of the second over the first text row (from the last one used)
   whose next insert is the same control, breaking early when it must
   (`insert_rows`, `wrap_to`). When no row can carry it - the second inserts
   a name the main spells out, or the main's row has another insert ahead
   of it, which a k-th-insert marker cannot skip - **the message is left
   plain and counted** (rule 4; about 2% of the paired area slots, section
   7). Its budget in the reading row: the census's widest print (5, 5, 8, 11
   glyphs for `03 04 07 08`) in the main's letters at the reading scale - the
   name drawn is the main's (section 8, answer 3).
9. **Colour** is the second's own, glyph by glyph (its walk restarting at 0
   on each of its pages); the reading row starts on the walk's colour and
   ends on the colour the text row below starts in, so every main glyph is
   drawn in the colour the original drew it in (`check` compares them).
   **DT-2's button code** in the second (`05 Bn 06 05 cc G`) is carried as
   one piece with **the main's own icon** for button `n` in place of the
   second's (Latin `0x9E3..0x9E6`, ja-JP `0x9F6..0x9F9`): `ResolveCode`
   stops the game on an icon that is not this layer's family
   ([`button-dialogue.md`](button-dialogue.md) 4.1), and a relocated cell
   would not be. Every other control of the second is dropped.
10. **Glyphs** come from the composed table (section 3); the hanging bytes
   are written two-byte.

## 3. The composed font chunk

The engine keeps one glyph table and every language overlay's `FIRST.DAT`
carries a whole one (census 7). The layer's `FIRST.DAT` therefore carries
**the main's table and advances unchanged, with the second's cells added**,
and the reading rows point into them. Loaded after the language overlay
(DIV-0086's walk), its kind-3 and kind-4 chunks replace the main's; the
main overlay's kind-13 pair table (DIV-0057) stays. Every main cell that
has ink is kept byte for byte (checked on read-back). The largest table is
3,719 glyphs (`0xE86`), under DIV-0016's `0x1000`.

| pair | the second's cells | where | space cell |
|---|---|---|---|
| Latin over ja-JP | the 100 dialogue cells (codes `0x30..0x93`) | `0xE00..0xE63` | `0xE64`, advance 8 |
| | the accented cells, French `0x94..0xAA` / German `0x94..0xA7` | `0xE70..0xE86` / `0xE70..0xE83` | |
| ja-JP over Latin | the single sheet and the kanji sheet, 882 cells | `0xAC0..0xE31` (`g - 0x993 + 0xAC0`) | `0xE40`, advance 12 |
| Latin over Latin | the main's own codes and letter shapes | as in the main | `0xA70` (en over en), `0xAC0` (the rest), advance 8 |
| | French or German over English: the accented blocks the English table leaves blank | at their own codes, `0xA80..0xAB6` | |
| | French over German: the three cells German lacks | `0xA94..0xA96` (and their UI twins) | |

- Under a Japanese main a Latin single byte is its dialogue cell
  (`loc_build.ASCII_OF`: the single-byte slots are painted from those
  cells); the hanging double quote `0x2A` is cell `0x90`'s.
- Under a Latin main the Japanese hangs `0x2A` / `0x3C` are the sheet cells
  `0x2A` / `0x3B` (`loc_build.JA_HANG`), relocated.
- Latin over Latin keeps single-byte letters as they are (the main's
  letters, the drafted answer); `0x2A` / `0x3C` become `80 04` / `80 16`,
  the same glyphs two-byte.
- The space is a blank cell with the second's space advance, so a Latin
  space in a reading row is 4 units at 1/2 and a Japanese one 6. No `0x09`
  is written by this mode.
- A glyph of the second outside these sets (a code its table does not
  relocate) leaves the message plain, counted; none did in the area blocks.

## 4. What the renderer reads (for RR)

- **Every glyph of a reading row** of a `ruby-<second>-<main>` layer is a
  code of the composed table; draw it on a `12 s` x `12 s` quad from its
  cell and move the pen by its kind-4 advance times `s`, where `s` is
  `JA_READING_SCALE` (1/2, fixed) when the second is `ja-JP` and
  `LATIN_READING_SCALE` (1/2 until the owner picks from PR's captures;
  `tools/ruby_build.py`, one constant) for a Latin second. At 1/2 a Latin
  letter (8 x 12 in its 24 x 24 glyph's left two thirds) draws 4 x 6 units,
  texel for texel, and advances 4 (the apostrophe and comma 2.5); a kana
  draws 6 x 6 and advances 6. The scale is per layer (the second's tag), so
  one constant on each side keeps the two in step; the codes per pair are
  section 3's table.
- **`0x12`** is the k-th insert of the text row below (contract 1.2), same
  control and argument as the second's; this layer means **draw that
  insert's text at the reading scale** (the main's name table, the drafted
  answer), not only skip its width as the furigana layer does. The builder
  budgets it at the widest print.
- **A button code** in a reading row is `05 Bn 06 05 cc G` with `G` the
  main's own icon: if the ruby walk asks `ButtonText_Code`, it resolves as
  in a text row; if not, `G` draws as an ordinary glyph.
- **Colour** codes `05 nn` / `06` appear in reading rows; a reading row
  always ends on the colour its text row starts in.
- **Empty rows**: an empty text row (`ruby 01` then `01` or the page's
  terminator) where the second runs on past the main; an empty reading row
  (`01` first) where the second has nothing over a row.
- Pools get side chunks too (tag `0x4000`, header word 3 the half); they
  are inert unless the system box's opens are repointed.

## 5. `setup`

`importer.py setup` (`tools/player_sources.py run_setup`) builds **every
ordered pair of held text layers** after the mods' step
(`build_second_layers`): for each tag the engine plays that the new cache
holds (`loc/<tag>/dat/FIRST.DAT`), each other one over it. Each pair runs
`ruby_build.py second --cache C --lang <main> --over <second> --out
C/opt/.<layer>.new/dat` under `-I`, is renamed to `opt/<layer>/` and every
chunk is listed in the manifest's `[cache] opt` rows (`ruby_build.py
second:<second>:<main>` in the what-built-it column) with `mods.py`'s
`_chunks` and `_manifest_rows`, so `ManifestOffersLayer` and the launcher's
*Row above the text* offer it. A builder that fails or writes nothing stops
the setup with an `error` line (rule 4); the install as it was is kept. Five
held layers make 20 pairs, about 2.5 minutes; two make 2. Checked on a test
cache with three layers (six pairs, the manifest rows, the files equal to
the `--game` build's).

## 6. The checks

- `--selftest`: every message of every block of both layers joined back
  from items byte for byte (FB's gate, both sides): 0 mismatches in every
  pair (79,123 messages for ja-JP and en-US; 74,918..79,183 per pair).
- `check`, on every composed message as it is written: its reading rows
  taken out and its row and page breaks normalised (a Latin break a space,
  runs of spaces one; a Japanese break nothing; empty rows dropped), the
  main's text items are the original's, **every glyph in the colour the
  original drew it in**; the same head, tail and non-page terminators; at
  most two pairs a page; a reading row holds only glyph codes (no hanging
  byte, no `0x20`), `0x12`, `05 nn`, `06`. A difference stops the build.
- `--lint FILE`: walks each reading row as RR will - every code inside the
  composed table and inked (or the space cell), its width at the reading
  scale with each marker at its budget inside 192 units, no hanging first
  byte, a marker only where the text row below has that insert - and every
  text row inside the box. **0 faults in all 20 pairs.** It also counts
  text rows that pass the box only when every insert prints its widest
  (authored rows: 897..1,703 a pair, informational).
- A read-back of every written layer, independent of the builder (in the
  hand-back's scratch): chunk headers, the main's ink unchanged in the
  composed table, each side chunk's slot count equal to its block's, every
  composed message terminated inside its chunk.

## 7. Numbers (2026-10-22, the held layers, area blocks)

| second over main | area slots paired | plain: inserts | plain: non-box / effect / timed | plain: kept-PC | box pages | as authored | re-broken | split: main / second / both | pages added | second > 2 reading rows | empty text rows |
|---|---|---|---|---|---|---|---|---|---|---|---|
| en-US over ja-JP | 17,332 | 382 | 409 / 134 / 29 | 511 | 20,095 | 78.3% | 18.5% | 441 / 119 / 72 | 640 | 191 (1.0%) | 252 |
| en-150 over ja-JP | 17,332 | 382 | 409 / 134 / 29 | 511 | 20,095 | 78.3% | 18.5% | 441 / 119 / 72 | 640 | 191 (1.0%) | 252 |
| fr-FR over ja-JP | 17,283 | 415 | 397 / 134 / 30 | 538 | 19,864 | 78.5% | 18.1% | 428 / 173 / 85 | 693 | 258 (1.3%) | 326 |
| de-DE over ja-JP | 17,337 | 390 | 400 / 134 / 29 | 507 | 20,042 | 78.3% | 18.0% | 413 / 234 / 102 | 762 | 336 (1.7%) | 374 |
| ja-JP over en-US | 17,282 | 473 | 340 / 132 / 59 | 511 | 21,943 | 63.2% | 14.0% | 5,003 / 0 / 1 | 5,004 | 1 (0.0%) | 1 |
| en-150 over en-US | 17,733 | 27 | 661 / 132 / 57 | 511 | 18,670 | 55.1% | 17.0% | 5,209 / 0 / 0 | 5,209 | 0 | 0 |
| fr-FR over en-US | 17,597 | 140 | 649 / 132 / 50 | 553 | 18,260 | 55.1% | 17.0% | 5,088 / 3 / 2 | 5,093 | 5 (0.0%) | 22 |
| de-DE over en-US | 17,639 | 119 | 652 / 132 / 57 | 522 | 18,343 | 55.1% | 17.1% | 5,093 / 3 / 3 | 5,100 | 6 (0.0%) | 14 |
| ja-JP over en-150 | 17,283 | 473 | 339 / 132 / 59 | 511 | 21,945 | 63.3% | 13.9% | 5,005 / 0 / 1 | 5,006 | 1 (0.0%) | 1 |
| en-US over en-150 | 17,734 | 27 | 660 / 132 / 57 | 511 | 18,672 | 55.2% | 16.9% | 5,211 / 0 / 0 | 5,211 | 0 | 0 |
| fr-FR over en-150 | 17,598 | 140 | 648 / 132 / 50 | 553 | 18,262 | 55.1% | 17.0% | 5,090 / 3 / 2 | 5,095 | 5 (0.0%) | 22 |
| de-DE over en-150 | 17,640 | 119 | 651 / 132 / 57 | 522 | 18,345 | 55.1% | 17.0% | 5,095 / 3 / 3 | 5,102 | 6 (0.0%) | 14 |
| ja-JP over fr-FR | 17,155 | 512 | 394 / 133 / 65 | 538 | 21,820 | 61.5% | 15.0% | 5,122 / 0 / 2 | 5,124 | 2 (0.0%) | 0 |
| en-US over fr-FR | 17,791 | 185 | 394 / 133 / 65 | 553 | 18,255 | 52.9% | 18.3% | 5,255 / 1 / 5 | 5,263 | 6 (0.0%) | 30 |
| en-150 over fr-FR | 17,791 | 185 | 394 / 133 / 65 | 553 | 18,255 | 52.9% | 18.3% | 5,255 / 1 / 5 | 5,263 | 6 (0.0%) | 30 |
| de-DE over fr-FR | 17,825 | 167 | 394 / 133 / 65 | 540 | 18,276 | 52.9% | 18.3% | 5,259 / 2 / 7 | 5,271 | 9 (0.0%) | 44 |
| ja-JP over de-DE | 17,163 | 467 | 455 / 130 / 75 | 507 | 21,836 | 63.9% | 9.8% | 5,724 / 0 / 1 | 5,725 | 1 (0.0%) | 0 |
| en-US over de-DE | 17,802 | 134 | 458 / 130 / 75 | 522 | 18,167 | 56.0% | 12.1% | 5,783 / 0 / 3 | 5,786 | 3 (0.0%) | 6 |
| en-150 over de-DE | 17,802 | 134 | 458 / 130 / 75 | 522 | 18,167 | 56.0% | 12.1% | 5,783 / 0 / 3 | 5,786 | 3 (0.0%) | 6 |
| fr-FR over de-DE | 17,788 | 145 | 455 / 130 / 66 | 540 | 18,174 | 56.1% | 12.1% | 5,792 / 1 / 3 | 5,797 | 4 (0.0%) | 19 |

- Paired and plain add up to the census's paired slots (section 2 there):
  18,286 for English over Japanese. Slots empty on one side are as the
  census counts them (324 Japanese-empty under English etc.).
- **Against the census** (its section 4, at 1/2): English over Japanese
  78.3 / 18.5% as authored / re-broken (census 78.2 / 18.5), 640 pages added
  (687), the second past two reading rows on 1.0% (1.0); French and German
  over Japanese 1.3% and 1.7% (1.4, 1.7). Japanese over English adds 5,004
  (5,481), every Latin main 5,100..5,800 (5,296..6,005): the main's own third
  row, as the census found. The builder adds a little less than the census
  because it splits three authored rows into 2 + 1 rows and re-shares the
  second, where the census's estimate re-wraps the main at worst.
- **Pages added are confirm presses added** (the owner accepted them,
  2026-10-20).
- Pools (88 halves): 14,210..20,430 slots paired a pair; their pages are one
  row and nothing splits; 5,656..11,876 are past the box (menu strings)
  and stay plain.
- The layer: 207..211 files, 3.0..4.7 MB a pair; longest composed message
  1,782 bytes (AREA175 slot 112, the faerie village's long tutorial).

## 8. The census's open questions: the drafted answers taken

The owner was not asked (the brief); each is the census's drafted answer
or the round's decision, recorded here and in the hand-back for
[`owner-review.md`](owner-review.md):

1. **Two text rows a page** - decided by the owner 2026-10-20; built.
2. **1/2 or 2/3** for a Latin reading row - **1/2** until PR's captures;
   `LATIN_READING_SCALE`, one constant. 2/3 changes widths only (the wrap
   reads the constant); the census says about a thousand more pages.
3. **Names in the reading row** - **the main's name table** at reading size:
   the marker refers to the text row's insert, so no name table is carried.
4. **Unequal page counts** - **shared in proportion** (drafted), by glyphs,
   in reading order.
5. **Non-box and offset-pen pages left plain** - **agreed**, and per message
   (section 2, rule 2).
6. **Letter shapes under a Latin main** - **the main's**, with the second's
   accented block added where the main is blank.
7. **The default pair** - **every held pair is built** by `setup` (the
   brief); the dropdown shows them all, the numbers favour English over
   Japanese.

## 9. Not done, and open

- Captures: the end batch's (round doc 5), once RR's renderer is in.
- **Inserts the marker cannot reference**: 27..512 area slots a pair stay
  plain. Recovering them needs either a marker that names the insert (an
  index after `0x12`) or the second's own name tables - a contract change,
  the coordinator's.
- The reading row of a choice option (none, as the round decided).
- `importer.py verify` / `verify_opt` and `mods.py remove` do not know these
  layers; `setup` rebuilds them every time, so nothing goes stale.
- The PSP's text layers (none held).
