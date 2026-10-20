# A second language in the reading row: the census and the pairing rule

**Status:** MEASURED 2026-10-20 (the reading-row round's group SL, branch
`reading-row/sl`; reads only, nothing built, nothing played). Every number
below is from one run of `tools/second_language_census.py run` on a copy of
the owner's install, unless it says otherwise; section 1 names the run.

The reading-row round ([`reading-row-round.md`](reading-row-round.md), on
`docs/reading-row-staging` until it merges) puts a half-height row above each
text row of the dialogue box. One use is a **second language**: the same
message from another held language layer, re-flowed into the reading rows
(IDEAS I45). This document answers the round's worry with numbers - when the
second script is longer than the main one, how often does it overflow the two
reading rows a page can hold, at 1/2 and at 2/3 - and drafts the pairing rule
group SB builds from.

## 0. The answer, short

- **The second language almost never overflows.** Over the 200 area text
  blocks, English over Japanese needs a third reading row on **1.0%** of the
  box pages at 1/2 and **6.4%** at 2/3; French and German over Japanese
  1.4% / 7.1% and 1.7% / 7.9%. Japanese over any Latin main overflows on
  0.4%, and one Latin language over another on at most 0.1% at 1/2 and 1.2%
  at 2/3. A reading row at 1/2 is 48 Latin letters or 32 kana wide, and a
  box page of the main carries a median 7..11 cells of the second's text
  at 1/2 (section 3).
- **The cost is the main script's own third row, not the second's length.**
  A page with reading rows holds two text rows. 22% of the Japanese pages
  and 43..46% of the Latin ones are authored with three; re-broken at word
  boundaries most Japanese ones fit in two, but **28..33% of every Latin
  main's box pages cannot**, and split whatever the second says - about
  5,300..6,000 extra pages over the area blocks, the same with any second
  language, Japanese included.
  If the box can hold **three text rows and two reading rows** (3 x 12 +
  2 x 6 = 48 units, the pen travel three plain rows use today, with no gap),
  98.3..100% of box pages fit with no split at 1/2 for every pair. That is a
  question for PR's box measurement and the owner (section 10, question 1).
- **Two overlays' fonts cannot be resident together as built**: the PC has
  one glyph table, and the Latin and Japanese layers both start their cells at
  glyph `0x993` and differ in every one of the 190..236 cells both ink
  (section 7). A **composed font chunk** works and fits under DIV-0016's
  limit: the main's table with the second's cells appended past its end.
  Latin over Latin needs nothing new except the French / German accented
  block, which an English table leaves free.
- **The composed text does not fit the 0x8000 arena in 16 of the 200 areas**
  for every pair (the faerie village 175..185, AREA039, 090, 094, 153, 154;
  39..47 KB against 32 KB), so the second script cannot simply be appended
  in-band to every block (section 8).

## 1. What was measured, on what

**The install.** The owner's play install holds every PSX language overlay
beside the shipped files: `game\game\DAT\` (the brief said `game\DAT\`; that
folder does not exist, the install is one level down), 1,976 files, 247
overlay files each for `en-US`, `en-150`, `fr-FR`, `de-DE` and 245 for
`ja-JP` (no `COMMU01` / `COMMU05` overlay, which are images in the others),
plus `area4-walls`. **No PSP text layer is held there**, so the PSP is left
out. The folder was copied whole to `C:\Users\kerok\bof3x-runs\rr_sl\DAT\`
on 2026-10-20 and only the copy was read; the game was not run. Identity,
SHA-256 prefixes of the copy:

| layer | `<tag>.FIRST.DAT` | the 200 `<tag>.AREA*.DAT` concatenated |
|---|---|---|
| ja-JP | `fec1970d9acc6890` | `b6c3837a4171ed04` |
| en-US | `d585032bc98b7f52` | `2154ea163ceda73e` |
| en-150 | `86228172856c2ab9` | `1b10af630d5948b1` |
| fr-FR | `835abb1ff201683b` | `0c071a8585dfbd59` |
| de-DE | `235b87ed2a7bf7d5` | `b85907babc23da4d` |
| shipped `AREA???.DAT` | | `d3163c8b2f51c58b` |

**The run.**

    python -I tools/second_language_census.py run --dat C:\Users\kerok\bof3x-runs\rr_sl\DAT --out census.json
    python -I tools/second_language_census.py tables census.json [--kind area | SYSPOOL | BATPOOL]

80 s; the JSON holds counts, area names and slot numbers only (rule 1). The
tables below are `tables`' output pasted as printed.

**What is a block.** The 200 area blocks (kind 0, tag 0 of
`<tag>.AREAnnn.DAT`, against the shipped `AREAnnn.DAT`'s) and the two pools
(kind 0, tag `0x4000`): the system pool in `FIRST.DAT` (`AFLDKWA.DAT`
carries the same bytes) and the battle pool in `BATTLE.DAT` (the 42
`BATTLE*` / `BOSS*` files carry the same bytes), each two sub-blocks. Every
overlay was built over the PC install, so every block has the PC's slot
count (`convert_block`) and slots pair one for one.

**The model** (the tool's docstring; the round doc's facts, and the
sibling's where this repo has no measurement):

- A text row is **W = 192 units** (16 cells of 12): the widest authored row
  of English is 192 at the 99th percentile (24 letters of 8), Japanese 16
  glyphs is the sibling's measured frame. A page **with reading rows holds
  two text rows and two reading rows** (round doc 0: 2 x 18 = 36 of the 48
  units three plain rows use).
- A glyph's width is its own layer's advance (`FIRST.DAT` kind 4, DIV-0006;
  the chunk's tag is the space): 12 for every Japanese glyph, 8 for Latin
  (5 for the apostrophe and the comma). A **reading glyph is that width
  times 1/2 or 2/3**: Latin 4 or 5.33 units, kana 6 or 8.
- Inserts are budgeted at the widest each can print, the sibling's
  `INSERT_WIDTH`: `03` and `04 nn` 5 glyphs, `07 nn` 8, `08 nn` 11, at the
  layer's letter width.
- Latin breaks only at spaces (an authored row end becomes a space);
  Japanese between any two glyphs (no kinsoku: a small undercount of rows).
  A row opening on `0x2A` / `0x3C` hangs that glyph into the margin
  (`msgbox.md` 3) and does not count.
- **A box page** is a page of at most three rows, none wider than W. A page
  of more rows, or with a wider row, is the narration path or a wider box
  (the sibling's rule); it is counted as **non-box** and left out of the fit
  classes (rule 4 below leaves it plain).
- A slot is the **port's kept message** (an untranslated or out-of-block
  slot `convert_block` filled from the PC file) when it equals the shipped
  slot or holds a two-byte glyph below `0x993`.
- **Pairing inside a message**: when both sides have the same number of
  pages (before any `0x14` choice), page i carries the second's page i;
  otherwise the second's text, its own page and row breaks dissolved, is
  shared out over the main's pages in proportion to each page's glyph count,
  cut at the second's break points.

**Fit classes, per main box page, at each scale:**

- **A, as authored**: the main page has at most two rows, kept as they are,
  and the second's share fits two reading rows (a one-row page whose second
  needs two rows gets an empty text row under the second: counted inside A,
  `A_needs_empty_text_row`, 205 pages for English over Japanese at 1/2).
- **B, main re-broken**: the main page has three rows, re-broken at word
  boundaries into two, and the second fits two reading rows.
- **C, a page split**: the main does not fit two rows even re-broken (**C
  main alone**), or the second needs a third reading row (**C by the
  second**), or both. The page becomes `max(ceil(main rows / 2), ceil(second
  rows / 2))` pages; *pages added* sums the difference, and *of them by the
  second* the pages that only the second's length adds.
- **3 + 2 fits**: the same page under the alternative layout of three text
  rows and two reading rows: the main's rows as authored, the second in two
  reading rows.

## 2. Slots

Per language, the area blocks (`langs` in the JSON): 55,337 slots in 200
blocks, every language.

| layer | native slots | distinct messages | pages | rows a page: 1 / 2 / 3 / more, % | row width p50 / p90 / p99 / max | wide rows on box-size pages | kept-PC slots | pages a message |
|---|---|---|---|---|---|---|---|---|
| ja-JP | 18797 | 15529 | 21759 | 37.7 / 38.1 / 22.1 / 2.0 | 108 / 156 / 228 / 528 | 197 | 0 | 1.40 |
| en-US | 18610 | 10837 | 19079 | 17.6 / 36.7 / 43.9 / 1.8 | 133 / 168 / 192 / 524 | 220 | 511 | 1.76 |
| en-150 | 18610 | 10837 | 19078 | 17.6 / 36.8 / 43.8 / 1.8 | 133 / 168 / 192 / 524 | 219 | 511 | 1.76 |
| fr-FR | 18911 | 10826 | 19200 | 16.8 / 35.4 / 46.2 / 1.6 | 128 / 168 / 200 / 453 | 328 | 538 | 1.77 |
| de-DE | 18639 | 10828 | 19098 | 17.1 / 38.2 / 43.0 / 1.7 | 144 / 168 / 208 / 848 | 346 | 507 | 1.76 |

Japanese messages are shorter and fewer to a page: 1.40 pages a message
against 1.76, and 22% of its pages have three rows against 43..46%. The
Japanese layer has more distinct messages (15,529), presumably because the
Japanese disc's slot table shares fewer offsets (not checked); pairing is by slot, so this changes
nothing below.

Per ordered pair, the area blocks:

| second over main | slots paired | distinct pairs | both empty | empty main only | empty second only | kept-PC main | kept-PC second | kept-PC both | second has more pages | fewer | non-box msgs |
|---|---|---|---|---|---|---|---|---|---|---|---|
| en-US over ja-JP | 18286 | 15030 | 36216 | 324 | 0 | 0 | 511 | 0 | 2252 | 168 | 604 |
| en-150 over ja-JP | 18286 | 15030 | 36216 | 324 | 0 | 0 | 511 | 0 | 2251 | 168 | 604 |
| fr-FR over ja-JP | 18259 | 14993 | 35888 | 652 | 0 | 0 | 538 | 0 | 2377 | 224 | 578 |
| de-DE over ja-JP | 18290 | 15024 | 36191 | 349 | 0 | 0 | 507 | 0 | 2282 | 183 | 595 |
| ja-JP over en-US | 18286 | 15030 | 36216 | 0 | 324 | 511 | 0 | 0 | 168 | 2252 | 550 |
| en-150 over en-US | 18610 | 10837 | 36216 | 0 | 0 | 0 | 0 | 511 | 0 | 1 | 542 |
| fr-FR over en-US | 18568 | 10814 | 35888 | 328 | 0 | 15 | 42 | 496 | 568 | 423 | 532 |
| de-DE over en-US | 18599 | 10836 | 36191 | 25 | 0 | 15 | 11 | 496 | 198 | 168 | 539 |
| ja-JP over en-150 | 18286 | 15030 | 36216 | 0 | 324 | 511 | 0 | 0 | 168 | 2251 | 549 |
| en-US over en-150 | 18610 | 10837 | 36216 | 0 | 0 | 0 | 0 | 511 | 1 | 0 | 541 |
| fr-FR over en-150 | 18568 | 10814 | 35888 | 328 | 0 | 15 | 42 | 496 | 569 | 423 | 531 |
| de-DE over en-150 | 18599 | 10836 | 36191 | 25 | 0 | 15 | 11 | 496 | 199 | 168 | 538 |
| ja-JP over fr-FR | 18259 | 14993 | 35888 | 0 | 652 | 538 | 0 | 0 | 224 | 2377 | 626 |
| en-US over fr-FR | 18568 | 10814 | 35888 | 0 | 328 | 42 | 15 | 496 | 423 | 568 | 620 |
| en-150 over fr-FR | 18568 | 10814 | 35888 | 0 | 328 | 42 | 15 | 496 | 423 | 569 | 620 |
| de-DE over fr-FR | 18584 | 10814 | 35866 | 22 | 325 | 33 | 2 | 505 | 447 | 570 | 620 |
| ja-JP over de-DE | 18290 | 15024 | 36191 | 0 | 349 | 507 | 0 | 0 | 183 | 2282 | 676 |
| en-US over de-DE | 18599 | 10836 | 36191 | 0 | 25 | 11 | 15 | 496 | 168 | 198 | 671 |
| en-150 over de-DE | 18599 | 10836 | 36191 | 0 | 25 | 11 | 15 | 496 | 168 | 199 | 671 |
| fr-FR over de-DE | 18584 | 10814 | 35866 | 325 | 22 | 2 | 33 | 505 | 570 | 447 | 661 |

- **Every slot either pairs, is empty on one side, or is the port's.** The
  Western layers' 496 kept slots are the US disc's sixteen Japanese template
  messages in 31 areas (`dialogue-localisation.md` 4); a Japanese main has
  nothing to pair them with, so they are **kept-PC second** (511 with 15
  more) and the main stays plain there.
- **The French and German out-of-block slots** (`loc-build-disc-only.md`:
  15 French and 11 German areas) show as kept-PC on that side: 42 French and
  11 German slots against English, 15 the other way (English slots the
  French or German table leaves to the PC). They pair with nothing; the main
  stays plain.
- **Empty on one side**: 324 slots where Japanese is empty and English is
  not (652 for French, 349 for German), and 25 German-only and 328
  French-only slots against English. The
  main stays plain, or there is no main.
- **Page counts agree** in 82.7..83.9% of Japanese / Latin messages, 90.6..90.8%
  of French / English and French / German, 96.6% German / English, and all but one en-US / en-150 message.
  Where they differ the Latin side mostly has **more** pages than the
  Japanese (2,252 English messages over Japanese, against 168 fewer).
- **The pools** pair the same way: system pool 701..707 messages, battle
  pool 580..585; no block in the run has a slot count mismatch.

## 3. The second's width

Per main box page, the second's share in **reading-row cells** (12 units; a
reading row holds 16): English over Japanese **9.6 cells at the median, 17.3
at the 90th percentile** at 1/2, 12.8 / 23.1 at 2/3, against 32 cells in two
reading rows. Japanese over English: 7.0 / 12.0 at 1/2, 9.3 / 16.0 at 2/3.
Latin over Latin: 10.2..11.3 / 15.5..17.0 at 1/2, 13.6..15.1 / 20.7..22.7 at
2/3. The
maxima (205 cells at 1/2 for English over Japanese) are single long messages
whose Japanese is short: AREA039 slot 0 is the worst message of every
Latin-over-Japanese pair (+7..8 pages at 1/2).

The second's whole message over the main's, in drawn width at full size:
English over Japanese 1.31 at the median and 2.67 at the 90th percentile;
French 1.43 / 4.0; German 1.38 / 2.67; Japanese over English 0.76 / 1.15;
Latin over Latin 0.91..1.10 / 1.07..1.33 at the median / 90th percentile.

## 4. Fit classes, area blocks

| second over main | msgs paired | equal page counts | box pages | scale | A as authored | B main re-broken | C main alone | C by the second | C both | second > 2 rows | pages added | of them by the second | 3 + 2 fits |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| en-US over ja-JP | 15030 | 12610 | 20404 | 1/2 | 15954 (78.2%) | 3774 (18.5%) | 474 (2.3%) | 113 (0.6%) | 89 (0.4%) | 202 (1.0%) | 687 | 124 | 20202 (99.0%) |
| | | | | 2/3 | 15725 (77.1%) | 3066 (15.0%) | 315 (1.5%) | 1050 (5.1%) | 248 (1.2%) | 1298 (6.4%) | 1639 | 1076 | 19106 (93.6%) |
| en-150 over ja-JP | 15030 | 12611 | 20404 | 1/2 | 15954 (78.2%) | 3774 (18.5%) | 474 (2.3%) | 113 (0.6%) | 89 (0.4%) | 202 (1.0%) | 687 | 124 | 20202 (99.0%) |
| | | | | 2/3 | 15725 (77.1%) | 3067 (15.0%) | 315 (1.5%) | 1049 (5.1%) | 248 (1.2%) | 1297 (6.4%) | 1638 | 1075 | 19107 (93.6%) |
| fr-FR over ja-JP | 14993 | 12392 | 20387 | 1/2 | 15931 (78.1%) | 3715 (18.2%) | 459 (2.3%) | 177 (0.9%) | 105 (0.5%) | 282 (1.4%) | 751 | 187 | 20105 (98.6%) |
| | | | | 2/3 | 15631 (76.7%) | 3018 (14.8%) | 287 (1.4%) | 1174 (5.8%) | 277 (1.4%) | 1451 (7.1%) | 1765 | 1201 | 18936 (92.9%) |
| de-DE over ja-JP | 15024 | 12559 | 20407 | 1/2 | 15938 (78.1%) | 3681 (18.0%) | 444 (2.2%) | 224 (1.1%) | 120 (0.6%) | 344 (1.7%) | 801 | 237 | 20063 (98.3%) |
| | | | | 2/3 | 15615 (76.5%) | 2906 (14.2%) | 281 (1.4%) | 1322 (6.5%) | 283 (1.4%) | 1605 (7.9%) | 1915 | 1351 | 18802 (92.1%) |
| ja-JP over en-US | 15030 | 12610 | 22790 | 1/2 | 14255 (62.5%) | 3076 (13.5%) | 5366 (23.5%) | 4 (0.0%) | 89 (0.4%) | 93 (0.4%) | 5481 | 26 | 22697 (99.6%) |
| | | | | 2/3 | 14246 (62.5%) | 3076 (13.5%) | 5362 (23.5%) | 13 (0.1%) | 93 (0.4%) | 106 (0.5%) | 5567 | 112 | 22684 (99.5%) |
| en-150 over en-US | 10837 | 10836 | 18532 | 1/2 | 10186 (55.0%) | 3050 (16.5%) | 5296 (28.6%) | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | 5296 | 0 | 18532 (100.0%) |
| | | | | 2/3 | 10186 (55.0%) | 3050 (16.5%) | 5270 (28.4%) | 0 (0.0%) | 26 (0.1%) | 26 (0.1%) | 5296 | 0 | 18506 (99.9%) |
| fr-FR over en-US | 10814 | 9823 | 18509 | 1/2 | 10165 (54.9%) | 3044 (16.4%) | 5290 (28.6%) | 4 (0.0%) | 6 (0.0%) | 10 (0.1%) | 5300 | 4 | 18499 (99.9%) |
| | | | | 2/3 | 10160 (54.9%) | 3033 (16.4%) | 5090 (27.5%) | 20 (0.1%) | 206 (1.1%) | 226 (1.2%) | 5319 | 23 | 18283 (98.8%) |
| de-DE over en-US | 10836 | 10470 | 18536 | 1/2 | 10186 (55.0%) | 3051 (16.5%) | 5292 (28.5%) | 3 (0.0%) | 4 (0.0%) | 7 (0.0%) | 5299 | 3 | 18529 (100.0%) |
| | | | | 2/3 | 10180 (54.9%) | 3041 (16.4%) | 5174 (27.9%) | 19 (0.1%) | 122 (0.7%) | 141 (0.8%) | 5315 | 19 | 18395 (99.2%) |
| ja-JP over en-150 | 15030 | 12611 | 22790 | 1/2 | 14265 (62.6%) | 3064 (13.4%) | 5368 (23.6%) | 4 (0.0%) | 89 (0.4%) | 93 (0.4%) | 5483 | 26 | 22697 (99.6%) |
| | | | | 2/3 | 14256 (62.6%) | 3064 (13.4%) | 5364 (23.5%) | 13 (0.1%) | 93 (0.4%) | 106 (0.5%) | 5569 | 112 | 22684 (99.5%) |
| en-US over en-150 | 10837 | 10836 | 18532 | 1/2 | 10196 (55.0%) | 3038 (16.4%) | 5298 (28.6%) | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | 5298 | 0 | 18532 (100.0%) |
| | | | | 2/3 | 10196 (55.0%) | 3038 (16.4%) | 5272 (28.4%) | 0 (0.0%) | 26 (0.1%) | 26 (0.1%) | 5298 | 0 | 18506 (99.9%) |
| fr-FR over en-150 | 10814 | 9822 | 18509 | 1/2 | 10175 (55.0%) | 3032 (16.4%) | 5292 (28.6%) | 4 (0.0%) | 6 (0.0%) | 10 (0.1%) | 5302 | 4 | 18499 (99.9%) |
| | | | | 2/3 | 10170 (54.9%) | 3021 (16.3%) | 5091 (27.5%) | 20 (0.1%) | 207 (1.1%) | 227 (1.2%) | 5321 | 23 | 18282 (98.8%) |
| de-DE over en-150 | 10836 | 10469 | 18536 | 1/2 | 10196 (55.0%) | 3039 (16.4%) | 5294 (28.6%) | 3 (0.0%) | 4 (0.0%) | 7 (0.0%) | 5301 | 3 | 18529 (100.0%) |
| | | | | 2/3 | 10190 (55.0%) | 3029 (16.3%) | 5175 (27.9%) | 19 (0.1%) | 123 (0.7%) | 142 (0.8%) | 5317 | 19 | 18394 (99.2%) |
| ja-JP over fr-FR | 14993 | 12392 | 22773 | 1/2 | 13775 (60.5%) | 3301 (14.5%) | 5605 (24.6%) | 2 (0.0%) | 90 (0.4%) | 92 (0.4%) | 5719 | 24 | 22681 (99.6%) |
| | | | | 2/3 | 13770 (60.5%) | 3301 (14.5%) | 5601 (24.6%) | 7 (0.0%) | 94 (0.4%) | 101 (0.4%) | 5801 | 106 | 22672 (99.6%) |
| en-US over fr-FR | 10814 | 9823 | 18544 | 1/2 | 9723 (52.4%) | 3282 (17.7%) | 5531 (29.8%) | 2 (0.0%) | 6 (0.0%) | 8 (0.0%) | 5539 | 2 | 18536 (100.0%) |
| | | | | 2/3 | 9718 (52.4%) | 3279 (17.7%) | 5421 (29.2%) | 10 (0.1%) | 116 (0.6%) | 126 (0.7%) | 5548 | 11 | 18418 (99.3%) |
| en-150 over fr-FR | 10814 | 9822 | 18544 | 1/2 | 9723 (52.4%) | 3282 (17.7%) | 5531 (29.8%) | 2 (0.0%) | 6 (0.0%) | 8 (0.0%) | 5539 | 2 | 18536 (100.0%) |
| | | | | 2/3 | 9718 (52.4%) | 3279 (17.7%) | 5421 (29.2%) | 10 (0.1%) | 116 (0.6%) | 126 (0.7%) | 5548 | 11 | 18418 (99.3%) |
| de-DE over fr-FR | 10814 | 9797 | 18538 | 1/2 | 9723 (52.4%) | 3283 (17.7%) | 5521 (29.8%) | 3 (0.0%) | 8 (0.0%) | 11 (0.1%) | 5532 | 3 | 18527 (99.9%) |
| | | | | 2/3 | 9711 (52.4%) | 3272 (17.7%) | 5385 (29.0%) | 26 (0.1%) | 144 (0.8%) | 170 (0.9%) | 5557 | 28 | 18368 (99.1%) |
| ja-JP over de-DE | 15024 | 12559 | 22691 | 1/2 | 14347 (63.2%) | 2171 (9.6%) | 6081 (26.8%) | 4 (0.0%) | 88 (0.4%) | 92 (0.4%) | 6198 | 26 | 22599 (99.6%) |
| | | | | 2/3 | 14338 (63.2%) | 2171 (9.6%) | 6077 (26.8%) | 13 (0.1%) | 92 (0.4%) | 105 (0.5%) | 6284 | 112 | 22586 (99.5%) |
| en-US over de-DE | 10836 | 10470 | 18437 | 1/2 | 10278 (55.7%) | 2157 (11.7%) | 5999 (32.5%) | 0 (0.0%) | 3 (0.0%) | 3 (0.0%) | 6005 | 0 | 18434 (100.0%) |
| | | | | 2/3 | 10274 (55.7%) | 2156 (11.7%) | 5940 (32.2%) | 5 (0.0%) | 62 (0.3%) | 67 (0.4%) | 6010 | 5 | 18370 (99.6%) |
| en-150 over de-DE | 10836 | 10469 | 18437 | 1/2 | 10278 (55.7%) | 2157 (11.7%) | 5999 (32.5%) | 0 (0.0%) | 3 (0.0%) | 3 (0.0%) | 6005 | 0 | 18434 (100.0%) |
| | | | | 2/3 | 10274 (55.7%) | 2156 (11.7%) | 5940 (32.2%) | 5 (0.0%) | 62 (0.3%) | 67 (0.4%) | 6010 | 5 | 18370 (99.6%) |
| fr-FR over de-DE | 10814 | 9797 | 18410 | 1/2 | 10257 (55.7%) | 2159 (11.7%) | 5987 (32.5%) | 2 (0.0%) | 5 (0.0%) | 7 (0.0%) | 5997 | 2 | 18403 (100.0%) |
| | | | | 2/3 | 10250 (55.7%) | 2156 (11.7%) | 5815 (31.6%) | 12 (0.1%) | 177 (1.0%) | 189 (1.0%) | 6008 | 13 | 18221 (99.0%) |

Reading it:

- **English over Japanese at 1/2**: 78.2% of box pages as authored, 18.5%
  with the Japanese re-broken into two rows, 3.3% split; the second alone
  forces 124 of the 687 added pages. At 2/3 the second forces 1,076 of
  1,639 (+8.0% pages).
- **French and German over Japanese** are the hard direction, slightly
  harder than English: 2/3 adds 1,765 and 1,915 pages (+8.7%, +9.4%), most
  of them the second's.
- **Japanese over a Latin main**: the second never matters (it adds 24..26
  pages at 1/2); the Latin main's three-row pages split by themselves:
  23.5..26.8%.
- **Latin over Latin**: the second adds 0..4 pages at 1/2 and 0..28 at
  2/3; the main's own third row adds 5,300..6,000.
- **3 + 2** fits 99.0% of English-over-Japanese box pages at 1/2 (93.6% at
  2/3), 98.3..98.6% of French and German over Japanese at 1/2, and
  98.8..100% of every Latin main's at both scales.

The pools (`tables --kind SYSPOOL` / `BATPOOL`): almost every pool message
is one row. System pool: 98.9..99.4% of box pages as authored in every pair,
no page overflowed by the second, 1..4 pages added; battle pool: 100% as
authored, nothing added. 166..295 system-pool and 170..293 battle-pool
pages are non-box (a row wider than a box row: descriptions and banners
drawn by other paths). The pools are outside this round's dialogue-box scope (round doc 7);
if SB wants them, the rule costs nothing there.

## 5. Growth per message

| second over main | msgs by pages added, 1/2 | at 2/3 | width ratio p50 / p90 | cells a page at 1/2 | at 2/3 | blocks over 0x8000 |
|---|---|---|---|---|---|---|
| en-US over ja-JP | 14374/634/18/3/0/1 | 13641/1250/93/21/13/12 | 1.313 / 2.667 | 9.62 / 17.33 / 205.0 | 12.83 / 23.11 / 273.33 | 22 / 16 |
| en-150 over ja-JP | 14374/634/18/3/0/1 | 13642/1249/93/21/13/12 | 1.313 / 2.667 | 9.62 / 17.33 / 205.0 | 12.83 / 23.11 / 273.33 | 22 / 16 |
| fr-FR over ja-JP | 14276/691/22/3/0/1 | 13475/1364/118/19/4/13 | 1.429 / 4.0 | 9.88 / 18.0 / 196.08 | 13.17 / 24.0 / 261.44 | 22 / 16 |
| de-DE over ja-JP | 14259/739/21/4/0/1 | 13408/1422/145/32/5/12 | 1.384 / 2.667 | 10.67 / 18.67 / 206.67 | 14.22 / 24.89 / 275.56 | 22 / 16 |
| ja-JP over en-US | 10779/3338/761/89/20/43 | 10770/3292/794/111/20/43 | 0.761 / 1.154 | 7.0 / 12.0 / 69.0 | 9.33 / 16.0 / 92.0 | 16 / 16 |
| en-150 over en-US | 6744/3207/734/89/20/43 | 6744/3207/734/89/20/43 | 1.0 / 1.0 | 10.33 / 15.54 / 20.33 | 13.78 / 20.72 / 27.11 | 23 / 16 |
| fr-FR over en-US | 6717/3211/734/89/20/43 | 6706/3216/738/91/20/43 | 1.04 / 1.328 | 10.54 / 16.33 / 42.67 | 14.06 / 21.78 / 56.89 | 23 / 16 |
| de-DE over en-US | 6740/3210/734/89/20/43 | 6728/3219/736/90/20/43 | 1.097 / 1.322 | 11.33 / 17.0 / 38.21 | 15.11 / 22.67 / 50.94 | 23 / 16 |
| ja-JP over en-150 | 10778/3339/760/90/20/43 | 10769/3293/793/112/20/43 | 0.761 / 1.154 | 7.0 / 12.0 / 69.0 | 9.33 / 16.0 / 92.0 | 16 / 16 |
| en-US over en-150 | 6743/3208/733/90/20/43 | 6743/3208/733/90/20/43 | 1.0 / 1.0 | 10.33 / 15.54 / 20.33 | 13.78 / 20.72 / 27.11 | 23 / 16 |
| fr-FR over en-150 | 6716/3212/733/90/20/43 | 6705/3217/737/92/20/43 | 1.04 / 1.331 | 10.54 / 16.33 / 42.67 | 14.06 / 21.78 / 56.89 | 23 / 16 |
| de-DE over en-150 | 6739/3211/733/90/20/43 | 6727/3220/735/91/20/43 | 1.097 / 1.322 | 11.33 / 17.0 / 38.21 | 15.11 / 22.67 / 50.94 | 23 / 16 |
| ja-JP over fr-FR | 10523/3527/763/119/33/28 | 10518/3477/796/141/33/28 | 0.7 / 1.118 | 7.0 / 12.0 / 69.0 | 9.33 / 16.0 / 92.0 | 16 / 16 |
| en-US over fr-FR | 6495/3404/736/118/33/28 | 6489/3409/737/117/33/29 | 0.962 / 1.201 | 10.21 / 15.54 / 44.33 | 13.61 / 20.72 / 59.11 | 23 / 16 |
| en-150 over fr-FR | 6495/3404/736/118/33/28 | 6489/3409/737/117/33/29 | 0.962 / 1.201 | 10.21 / 15.54 / 44.33 | 13.61 / 20.72 / 59.11 | 23 / 16 |
| de-DE over fr-FR | 6500/3400/736/117/33/28 | 6487/3405/742/118/33/29 | 1.05 / 1.293 | 11.0 / 16.67 / 47.08 | 14.67 / 22.22 / 62.78 | 24 / 16 |
| ja-JP over de-DE | 10278/3646/892/147/33/28 | 10269/3600/925/169/33/28 | 0.723 / 1.066 | 7.0 / 12.0 / 69.0 | 9.33 / 16.0 / 92.0 | 16 / 16 |
| en-US over de-DE | 6252/3512/867/144/33/28 | 6251/3511/868/144/34/28 | 0.912 / 1.069 | 10.21 / 15.54 / 30.75 | 13.61 / 20.72 / 41.0 | 23 / 16 |
| en-150 over de-DE | 6252/3512/867/144/33/28 | 6251/3511/868/144/34/28 | 0.911 / 1.069 | 10.21 / 15.54 / 30.75 | 13.61 / 20.72 / 41.0 | 23 / 16 |
| fr-FR over de-DE | 6236/3508/865/144/33/28 | 6233/3508/866/143/36/28 | 0.953 / 1.162 | 10.54 / 16.21 / 42.67 | 14.06 / 21.61 / 56.89 | 23 / 16 |

(The last column: area blocks whose composed size, section 8, passes
`0x8000` with every second glyph two bytes / with the second in its own
encoding.)

The messages that grow most at 1/2: AREA039 slot 0 (+8 pages, every Latin
second over Japanese; +10..11 at 2/3) and slot 112 of AREA181..185, the
faerie village's one long message (+8 under any Latin main, the main's own
rows). Under a Japanese main with an English second at 1/2, 95.6% of paired
messages grow not at all and 99.9% by at most one page.

## 6. Controls that complicate pairing

| pair | msgs with an insert | insert counts differ | one side only | of 04 | of 07 | of 03 | choice either | choice one side | option count differs | colour either | colour one side | span count differs | 0D offset pen |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| en-US / ja-JP | 2823 | 304 | 191 | 206 | 98 | 0 | 765 | 11 | 0 | 625 | 16 | 37 | 115 |
| en-150 / ja-JP | 2823 | 304 | 191 | 206 | 98 | 0 | 765 | 11 | 0 | 625 | 16 | 37 | 115 |
| fr-FR / ja-JP | 2808 | 302 | 199 | 204 | 98 | 0 | 766 | 16 | 0 | 616 | 19 | 45 | 115 |
| de-DE / ja-JP | 2819 | 302 | 194 | 204 | 97 | 1 | 765 | 13 | 0 | 619 | 18 | 53 | 116 |
| en-150 / en-US | 2641 | 0 | 0 | 0 | 0 | 0 | 755 | 0 | 0 | 619 | 0 | 0 | 108 |
| fr-FR / en-US | 2639 | 43 | 29 | 41 | 2 | 0 | 756 | 5 | 0 | 610 | 3 | 18 | 113 |
| de-DE / en-US | 2647 | 33 | 23 | 31 | 1 | 1 | 755 | 2 | 0 | 619 | 8 | 29 | 110 |
| fr-FR / en-150 | 2639 | 43 | 29 | 41 | 2 | 0 | 756 | 5 | 0 | 610 | 3 | 18 | 113 |
| de-DE / en-150 | 2647 | 33 | 23 | 31 | 1 | 1 | 755 | 2 | 0 | 619 | 8 | 29 | 110 |
| de-DE / fr-FR | 2630 | 34 | 24 | 32 | 1 | 1 | 756 | 7 | 0 | 621 | 15 | 33 | 112 |

- **Inserts** (`03`, `04 nn`, `07 nn`, `08 nn`): 18.8% of English / Japanese
  messages carry one. In 304 the two sides' counts differ (206 a member name
  `04`, 98 an item or skill record `07`), and in 191 **one side spells out
  what the other inserts** (or drops it). Latin pairs differ in 33..43
  messages, nearly all `04`.
- **Choice blocks** (`0x14`): 755..766 messages; the option counts agree in
  every message where both sides have one; 11..16 messages (Japanese pairs),
  0..7 (Latin pairs) have a choice on one side only.
- **Colour** (`05 nn` / `06`): 610..625 messages; the span count differs in
  0..53, a colour on one side only in 0..19.
- **The offset pen** (`0D` / `0E`): 108..116 messages on either side.
- `0C` (2,056..2,133 messages), `0B`, `0F`, `0A`, `16`, `10` / `11` occur on
  either side; they draw nothing.

## 7. Two overlays' fonts together

Read from the five `<tag>.FIRST.DAT` chunks (kind 3, the glyph table; kind
4, the advances); `fonts` in the JSON. **No live check was run** (the game
must not run from the owner's folder, and the answer does not need one: the
engine keeps one global table, `Font_SetGlyphData` frees the previous one,
`dialogue-localisation.md` 2, and an overlay's `FIRST.DAT` kind-3 chunk is a
whole table, so two overlays applied in turn leave the last one's).

| layer | glyphs | inked from `0x993` |
|---|---|---|
| ja-JP | 3,570 | `0x993-0x9B9`, `0x9BB-0xD04` (the two 441-cell sheets, one cell blank), `0xD10-0xDF1` (226 pair codes, DIV-0057) |
| en-US, en-150 | 2,671 | `0x993-0x9F6` (the 100 dialogue cells), `0xA00-0xA6E` (the 8 x 8 set, icons, suffixes) |
| fr-FR | 2,743 | as English, and the accented block `0xA80-0xA96` / `0xAA0-0xAB6` |
| de-DE | 2,740 | as English, and `0xA80-0xA93` / `0xAA0-0xAB3` |

- **Japanese against any Latin layer: they collide everywhere.** Every glyph
  both tables ink from `0x993` differs (190 of 190 against English, 236 of
  236 against French), all 100 Latin dialogue cells differ in shape and
  advance, 76 of the 90 single-byte slots differ (the Latin layers paint
  their letters over them, `build_table`), and both hang slots differ:
  `0x2A` is the Japanese corner bracket on one side and the Latin double
  quote on the other; `0x3C` is the white corner bracket on the Japanese
  side and the port's own glyph on the Latin. The Japanese area text uses no
  single byte but the two hangs (10,917 of them); the Latin text is 99% single
  bytes.
- **English against French or German**: the same code ranges, the same
  meanings (one `ASCII_OF` map builds every Western layer), but **different
  shapes**: 98 of the 100 dialogue cells differ in ink mask (the French and
  German discs' 8 x 12 font is drawn differently from the English discs'),
  one advance differs, 73 single-byte slots differ, `0x2A` differs. French
  and German are identical where both ink, and en-US and en-150 are
  identical throughout.
- **What a composed font chunk must hold**: the main layer's whole table
  (and advances) unchanged, then the second's cells appended past its end,
  under DIV-0016's limit, which follows the table:
  - Latin over Japanese: the Latin dialogue cells from `0xDF2` (after the
    pair codes), 100 + 20..23 accented, to about `0xE6D`. The second's text
    is re-encoded two bytes a glyph into that range. Painting the Latin
    letters over the Japanese table's single-byte slots would halve the
    bytes (the Japanese text never draws them), but any other text path that
    draws a port single byte under a Japanese main would change; not
    recommended until measured.
  - Japanese over a Latin main: the 882 cells from `0xAC0` (past the French
    accented block) to `0xE31`; no pair codes (no dialogue message holds
    one, round doc 1).
  - Latin over Latin: nothing to relocate if the second may draw in the
    main's shapes; only the accented block (French or German over English)
    is appended, at the indices it has in its own table, which an English
    table leaves blank. Keeping the second's own shapes would relocate its
    100 cells like Latin over Japanese.
  - In every case the second's `0x2A` / `0x3C` must become two-byte codes in
    a reading row: a reading row starts a line, and the stepper hangs a
    single `0x2A` / `0x3C` at line start.

## 8. Block room

The composed block is the main block plus the second's text (two bytes a
glyph, one a space, three bytes a page of row and page codes; an estimate,
`composed_*` in the JSON). The arena room an area block is built against is
`TEXT_ROOM` = `0x8000` (`loc_build.py`; the CLUT strip starts there). The
largest main blocks are 19,954 (Japanese, AREA185) to 23,828 bytes
(German, AREA153). Composed:

- **16 areas pass 0x8000 in every pair** with the second in its own
  encoding (largest 39,279..47,269 bytes), 16..24 with every second glyph
  two bytes (largest 39,520..64,376, the top close to the `u16` offset
  table's 65,535): AREA039, 090, 094, 153, 154 and the faerie village 175..185.
- So an in-band layer (the round doc's section 4 shape) fits 184 of 200
  areas; the other 16 need the reading rows somewhere else - a side chunk
  keyed by slot and page that the stepper reads, or a larger arena for the
  text block - which is SB's and RR's call, not measured here.

## 9. The pairing rule (draft, for SB)

1. **Pair by slot.** The second's message is the same block's same slot. A
   slot that is empty on either side, or the port's kept message on either
   side (section 2), leaves the main **plain**. So do the French / German
   out-of-block slots, which arrive as kept slots.
2. **Only box pages are annotated.** A main page of more than three rows,
   or with a row wider than 192, stays as it is with no reading row (536..677
   pages a pair in the area blocks at 1/2). So does a page whose main uses the
   offset pen `0D` (108..116 messages), whose glyphs the stepper moves by
   signed offsets (`msgbox.md` 3).
3. **Pages pair in order when the counts agree** (83..100% of messages).
   Otherwise the second's text is shared over the main's pages **in reading
   order, in proportion to each main page's glyph count**, cut at the
   second's word boundaries (any glyph boundary for Japanese). The second
   never moves text between its own sentences out of order; when it has more
   pages than the main they merge, and when it has fewer the share simply
   thins (an empty reading row is allowed).
4. **The main keeps its authored breaks where the page allows it.** A page of
   one or two rows is kept as authored (class A); a three-row page is
   re-broken at word boundaries into two (B); the second is packed greedily
   into the page's two reading rows.
5. **Split only on overflow**, at a `0x02` and a word boundary of both
   scripts: when the main does not fit two rows, or the second does not fit
   two reading rows. Balancing the split pages' rows rather than filling
   the first is suggested, not measured.
6. **Choice blocks stay plain.** The prompt pages before `0x14` pair like
   any page; the `0x14` block is the main's, copied verbatim; the second's
   options are dropped (round doc call 5). A choice on one side only (0..16
   messages a pair) changes nothing: the main's block, if any, is copied.
7. **Inserts.** The main's inserts are carried as they are. In the reading
   row the second's insert must not be the raw control, because at run time
   `04 nn` / `07 nn` draw from the *main's* name and record tables; the
   layer writes the second's insert as a marker RR resolves (the second's
   name table if SB carries one, else the main's name at reading size), with
   the width budget of section 1. Where one side spells a name out and the
   other inserts it (191 English / Japanese messages), each side keeps what
   it has.
8. **Colour** (`05 nn` / `06`) is carried into the reading row where the
   second has it (the round doc allows these two in a ruby row); spans need
   not line up with the main's. Every other control of the second (`0A 0B
   0C 0F 10 11 16`) is dropped from the reading row: none draws, and the
   main's own copy stays in its text row.
9. **The second's glyphs** come from the composed font (section 7), and its
   `0x2A` / `0x3C` are written two-byte.

**Serves well:**

- Any Latin second over Japanese at 1/2: 78% of box pages as authored,
  1.0..1.7% overflow, +3.4..3.9% pages.
- Japanese over any Latin main, as far as the second goes: 0.4% overflow.
- Latin over Latin at either scale, as far as the second goes: at most 1.2%.

**Serves badly:**

- **Every Latin main, under two text rows a page**: 28..33% of box pages
  split by the main's own third row (+5,300..6,000 pages, 28..33%), with any
  second. The 3 + 2 layout removes it (98.8..100% fit).
- **French and German over Japanese at 2/3**: 7.1% / 7.9% of pages
  overflow, +8.7% / +9.4% pages.
- **AREA039 slot 0** under a Japanese main (+7..8 pages at 1/2) and the
  faerie village's slot 112 under a Latin main (+8): long messages on one
  side.
- **The 16 big areas** cannot carry the second in-band in a 0x8000 block
  (section 8).
- **Inserts**: 304 English / Japanese messages whose insert counts differ;
  the reading row's names are the main's until SB carries the second's
  name table.

## 10. Open questions for the owner

1. **Two text rows a page, or three?** With two text rows and two reading
   rows a page, about a third of a Latin main's pages split, whatever the
   second language; with three text rows and two reading rows (48 units, no
   gap between rows; PR's box measurement says whether the frame allows it)
   almost none do. Which layout, once PR's captures are in?
2. **1/2 or 2/3** for a Latin row over Japanese (round doc call 1): 2/3 costs
   about a thousand more split pages (English over Japanese +1,639 against
   +687).
3. **Names in the reading row**: the main's name at reading size (no new
   data), or the second language's names (SB carries each held layer's name
   tables)?
4. **Unequal page counts**: shared out in proportion (drafted), or strictly
   page by page with leftovers carried forward?
5. **Pages left plain**: non-box pages and offset-pen pages without a
   reading row - agreed?
6. **The second's letter shapes** under a Latin main: the main's (no new
   glyphs) or the second's own (French and German letters are drawn
   differently from English ones)?
7. **The default pair** to build on `setup` when more than two languages are
   held (round doc call 6): every pair costs one layer; the numbers favour a
   Japanese main with an English second.

## 11. Not measured

- The box's real inner height and whether 3 + 2 fits (PR).
- Kinsoku in Japanese breaks (closing punctuation clinging to the glyph
  before it): rows are counted with free breaks, a slight undercount.
- The runtime width of inserts (budgeted, not measured).
- The composed block's exact size: estimated per distinct slot pair;
  section 8's 16 areas are well past the line either way.
- The PSP's text layers (none held in the owner's install).
