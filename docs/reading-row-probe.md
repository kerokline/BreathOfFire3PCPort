# The reading-row probe: the box's height, the row table, and the look

**Status:** MEASURED 2026-10-22 (the reading-row round's group PR, branch
`reading-row/pr`). Every number below was read off frames the game wrote
itself (`BOF3X_SHOT_DIR`) or off a `peek` in the same run; the frames and
the scripts are outside the tree (section 7). The probe code that drew them
was deleted before this branch was handed back (rule 4); this file is what
remains. Nothing here is in the ledger: the probe was instrumentation, and
the reading row's ledger entry is RR's.

The round ([`reading-row-round.md`](reading-row-round.md) 2, "PR") asked
for one hand-written annotated page drawn by our own `MsgBox_Step` with the
row rule of [`reading-row.md`](reading-row.md) 1.2, captured at three gaps,
two filters and two aspects, and measured, so that the owner can choose
the layout by eye and RR can compile the numbers in. This is that.

## 1. What was built, and the scene

**The probe** (`src/game/ruby_probe.cpp`, deleted): `BOF3X_RUBYPROBE=<file>`
replaced every message `Msg_OpenScript` opened with the file's bytes (the
original's leading `0x0C nn` placement kept), by swapping `msgbox::g.reset`
for a stand-in that repoints `0x7DEE4C` / `0x7DEE50` and then calls
`MsgBox_Reset`. The box's step pointer `msgbox::g.step` was swapped for a
walk with the row rule while the base pointer lay inside the probe's
buffer, and `MsgBox_Step` itself otherwise: **not one instruction of
`MsgBox_Step`, `Msg_OpenScript` or `MsgBox_StatePrint` changed** - the
probe lived in the callee table, which is also where RR's side-chunk
repoint will live (`reading-row.md` 1.1). Unset, `RubyProbe_Inject`
returned before touching the table.

The walk: rows alternate ruby / text from the page's top; a ruby glyph is
the ordinary glyph code emitted as one quad of `q x q` units from the
glyph's whole 12-unit texture cell (a copy of `Text_EmitGlyph`'s record with
the quad scaled; the page-scroll clip pair scaled with it); `0x09` in a ruby
row moved the ruby pen 6 units; a text glyph went through `MsgBox_DrawChar`
exactly as `MsgBox_Step` sends it (DIV-0006's advance, the `0x2A` / `0x3C`
hang). Any other control in a probe file was a `Fatal`. Switches:
`BOF3X_RUBYGAP` (0, 1, 2), `BOF3X_RUBYLATIN=half|twothirds`,
`BOF3X_RUBYTOP` (a signed offset of the whole block), and
`BOF3X_RUBYMODE=plain` (the swap only, Capcom's walk - the plain-row
measurement).

**Ruby quad and advance, per glyph:** `q = 6` and an advance of half the
glyph's own kind-4 advance (6 for every Japanese glyph); a Latin glyph
(advance not 12) at 1/2 the same, at 2/3 `q = 8` and two thirds of its
advance (5.33 for a letter, kept in sixths of a unit, the quad's x
floored). A ruby row holding a 2/3 Latin glyph is 8 units high, else 6.

**The fonts:** a second script's glyphs are not resident under the main
language's table ([`second-language-census.md`](second-language-census.md)
7), so the probe's root carried **composed** `FIRST.DAT` overlays, built by
a scratch script from the held layers: the main table and advances
unchanged, the donor's whole table appended (`ja-JP` + `en-US` from glyph
3,570; `en-US` + `ja-JP` from 2,671), then one blank cell with the donor's
space advance, at 6,241. The DLL took both as ordinary kind-3 / kind-4
chunks (DIV-0016's limit follows the table: 6,242 glyphs logged). A second
script's glyph in a probe file was the two-byte code of its relocated
index, the hanging brackets included, as `reading-row.md` 1.2 says.

**The probe files** (scratch, rule 1: no byte of them here): the AREA014
scarecrow talk (slot 7 of `ja-JP.AREA014.DAT`'s block, the sibling's
reference scene) in `ja-JP`, its 2 / 3 / 1 rows re-paired as three pages
of two pairs, with nine readings hand-placed at twice their stem's cell
index (the four the brief names and five more); the same talk's English
rows with their Japanese rows as the reading row (Japanese over English,
two pages); its Japanese rows with the English rows over them (English
over Japanese, two pages, at 1/2 and at 2/3); and the talk's three-row
page in each language unannotated, for the plain measurement.

**The scene.** No recipe or save puts the party in front of a talkable
NPC in AREA014, so the probe used the nearest scene a recipe reaches with
the dialogue box open in the field: **NEW GAME, the opening scene's first
dialogue** (the start of `battle_commands.txt`: the title, confirm, the
first box; about 1,000 recipe frames, no save). Since every message was
replaced, the box shows the probe there; it is a standard bottom box
(`0x0C` placement of the scene's own message kept). The owner can place
an AREA014 save if the scarecrow's own box should be seen (section 8).

## 2. The box, measured

At 4:3, scale 2 (640 x 480 frames, two pixels a unit), from `peek`s in the
same runs and the frame's white border lines:

| What | Units | Frame pixels |
|---|--:|--:|
| Box inner top (first row inside the white line) | 170 | 340 |
| Box origin y, `Msgbox_OriginY` (window y + 6) | 176 | - |
| Box line start x, `MsgBox_LineX` | 114 (167 at 16:9) | - |
| Box inner bottom (last row inside the white line) | 224 | 449 |
| **Inner height from the origin to the inner bottom edge** | **49** | 98 |
| Inner height, line to line | 55 | 110 |

A glyph is drawn at its row's pen y + 1 (`Text_DrawAt`), on a 12-unit
quad for text and a 6-unit quad for a reading.

**The plain page** (`BOF3X_RUBYMODE=plain`, Capcom's walk; `ja-JP`):
the three rows' pen y are origin + 0, 14, 28 (the last `peek`ed: 204);
their ink spans 177.0..187.5, 191.0..201.5, 205.0..215.5 units (pixels
354..375, 382..403, 410..431). **The third plain row's glyph bottoms land
at 215.5 units** (quad bottom 217), 9.5 units of ink-free box under them.

**Two annotated pairs, as probed** (the block at the origin,
`BOF3X_RUBYTOP=0`), ink in units (the frame pixels / 2), the same in
`ja-JP` furigana and in Japanese over English:

| | ruby 1 | text 1 | ruby 2 | text 2 | arrow | text 2 ink to the inner bottom |
|---|---|---|---|---|---|--:|
| gap 0 | 177.5..182.0 | 183.0..193.5 | 195.5..200.0 | 201.0..211.5 | 214.0..217.5 | 13.5 |
| gap 1 | 177.5..182.0 | 183.0..193.5 | 196.5..201.0 | 202.0..212.5 | 215.0..218.5 | 12.5 |
| gap 2 | 177.5..182.0 | 183.0..193.5 | 197.5..202.0 | 203.0..213.5 | 216.0..219.5 | 11.5 |

Pen y of the last text row: 200 + gap (`peek`ed 200 at gap 0). Within a
pair the reading's ink ends 1 unit above its own text's ink; across pairs
the ink gap from text 1 to ruby 2 is 2 + gap units. So the reading always
reads with the row under it, and the gap only widens the space between
pairs.

**The next-page arrow sits under the last text row** in every annotated
frame: `MsgBox_State3Arrow` places it at the pen y + 12, and the walk's pen
y is the last text row's, so its ink is 2.5 units under the text's ink and
inside the box at every gap (lowest: 219.5 at gap 2, against the inner edge at 225). Under the
probe's walk the arrow never sat under a ruby row - the reason
`reading-row.md` 1.2 puts ruby rows first.

**16:9** (`BOF3X_WIDE=1`, 852 x 480 frames, scale 2): every row, ink band,
arrow and box edge above at the same y, to the pixel; the box and its line
start move right by 53 units (and the target's own 53-column pad; 106
pixels in all). Nothing in the row table depends on the aspect.

## 3. The kana, texel for texel

The check (scratch `texel.py`): for every reading glyph on page 1, its 12 x
12-pixel square in the frame against the glyph's own 24 x 24 table cell
taken at its odd texels - the 12 px sheet cell, since every texel of a PC
Japanese glyph is a sheet texel doubled. A texel is exact when its pixel
has the grey the CLUT gives its nibble (206, 189, 165, 140, 107, 66, 33 for
1..7, read off the frames); a row is lost when the cell row holds body ink
(nibbles 1..4) and no pixel of its frame row is that bright.

| Frames | Reading glyphs | Inked texels exact | Rows lost |
|---|--:|--:|--:|
| `ja-JP` furigana, gaps 0 / 1 / 2, point and linear, 4:3 and 16:9 (12 frames) | 7 each | 440 / 440 each | 0 |
| Japanese over English (kana and kanji at 6 units), the same 12 | 25 each | 1,436 / 1,436 each | 0 |
| English over Japanese at 1/2 (letters at 4 x 6), point and linear | 33 each | 1,149 / 1,149 each | 0 |

**At scale 2 no 6-unit glyph loses a row of texels under either filter;
every inked texel lands on its own pixel with its own grey.** The linear
filter changes nothing in a reading glyph: DIV-0025 samples at texel
centres, and at scale 2 each pixel's centre falls inside one doubled texel
pair, so bilinear and point read the same value (the frames differ only in
the scene behind the box). The same holds for a Latin letter at 1/2 (its 8
x 12 PSX cell shown at 4 x 6 units).

**English at 2/3** (letters on 8-unit quads, 5.33 x 8 units): 33 letters,
242 inked PSX rows, **none lost** under either filter, but the sampling is
uneven by construction - 16 screen rows over 12 PSX rows, so 92 of the 242
rows are drawn two pixels high and 150 one pixel (columns likewise). That is
the "1.5 : 1, uneven" the round doc predicted; whether it reads worse is the
owner's eye (section 5).

**Other scales.** The frames above are scale 2, the 640 x 480 and 852 x 480
windows. A window too small for scale 2 renders at scale 1 (the first 16:9
runs did, a 640 x 480 window under `BOF3X_WIDE=1`: 426 x 240 frames, scale
1 logged): there a 6-unit reading is 6 pixels over a 12-texel cell and 6
of 7 reading glyphs lost a body row - inherent, not a defect of the row.
At scale 3 (a 960 x 720 client) a 6-unit reading is 18 pixels over the 12
sheet rows, 1.5 : 1 like a Latin letter at 2/3: of 62 inked rows of the
seven readings none lost under point (31 drawn two pixels high, 31 one);
under linear one row of one kana (twice on the page) dimmed to grey 117
against nibble 4's 140, not gone. At scale 4 (1,280 x 960) every sheet row
is exactly two pixels, none lost. So the reading row is exact at even
scales and uneven, not broken, at 3 (`kana_zoom_k3_*.png`, section 7).

## 4. The row table

Pen y per row, in units from the box origin `O` (`Msgbox_OriginY` = window
y + 6); a glyph draws at pen y + 1. Ruby height `r` = 6 (kana; Latin at
1/2) or 8 (Latin at 2/3).

**As probed** (`BOF3X_RUBYTOP=0`, the block from the origin down):

| Row | Pen y |
|---|---|
| ruby 1 | `O` |
| text 1 | `O + r` |
| ruby 2 | `O + r + 12 + gap` |
| text 2 | `O + 2r + 12 + gap` |

It leaves 7 units over the first reading and 11.5..13.5 under the last text
row - the block sits high, and the arrow lands 2..4 units above the plain
page's.

**Recommended: bottom-aligned to the plain page.** The last text row on
the plain third row's pen y, `O + 28`, so the last row's glyph bottoms, the
arrow and the space under them are exactly the original page's, and the
gap is taken from the top:

| Row | Pen y (r = 6) | gap 0 | gap 1 | gap 2 | Pen y (r = 8) | gap 0 | gap 1 | gap 2 |
|---|---|--:|--:|--:|---|--:|--:|--:|
| ruby 1 | `O + 4 - gap` | +4 | +3 | +2 | `O - gap` | 0 | -1 | -2 |
| text 1 | `O + 10 - gap` | +10 | +9 | +8 | `O + 8 - gap` | +8 | +7 | +6 |
| ruby 2 | `O + 22` | +22 | +22 | +22 | `O + 20` | +20 | +20 | +20 |
| text 2 | `O + 28` | +28 | +28 | +28 | `O + 28` | +28 | +28 | +28 |

Above the first reading's ink this leaves 11.5 - gap units (r = 6) and
7 - gap (r = 8), against the plain page's 7; below the last text row 9.5,
as the plain page. With `r = 6` the block (36 + gap units of pen travel
plus a 12-unit row) is 4 - gap units shorter than the plain page's three
rows, so every gap fits with room; with `r = 8` and gap 2 the first reading
starts 5 units under the inner top. Captured as the "bottom-aligned" sheets
(`BOF3X_RUBYTOP` = 4 - gap for r = 6, -gap for r = 8).

**The gap:** the probe recommends **2**, by the measurement above - at gap
0 a reading's ink is 1 unit from its own text and 2 from the text above it,
which reads correctly but tight; at 2 it is 1 against 4 - and leaves the
choice to the owner's eye on the sheets (round doc 6, call 1). The plain
newline's 14 stays for every message not from a side chunk.

## 5. The Latin row: 1/2 against 2/3

English over Japanese, the same two pages at both sizes (gap 1, top 0):

| | 1/2 (4 x 6 units a letter) | 2/3 (5.33 x 8) |
|---|---|---|
| Ruby row ink, row 1 | 177.0..182.0 | 177.0..184.0 |
| Pair pitch | 18 + gap | 20 + gap |
| Last text row ink bottom (gap 1) | 212.5 | 216.5 |
| Arrow ink (gap 1) | 215.0..218.5 | 219.0..222.5, 2.5 units above the inner bottom edge (225) |
| Texels | exact, as kana | no row lost; a third of the rows doubled |

Both are legible at scale 2 in the frames. At 2/3 the two pairs top-aligned
leave the arrow's ink 2.5 units from the frame's inner edge at gap 1 and 1.5
at gap 2, so a 2/3 row wants the bottom-aligned table (r = 8), which puts
the arrow where the plain page's is. SL's numbers stand beside the look: at 2/3 the
second language overflows two reading rows on 6.4..7.9% of Latin-over-
Japanese box pages against 1.0..1.7% at 1/2.

## 6. Identity with the probe off

The `msgbox` shadow fuzz ([`msgbox.md`](msgbox.md) 5), headless from an
ini-less launcher copy built from this branch with the probe compiled in
(after the merge of `localization/second-script` at `1fb979cf`):

    BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=msgbox            exit 0
      44,000 rounds (1,000 per function, 44 functions), 35,831 calls to
      the stand-ins, 0 MISMATCHES; DIV-0070's check 0 MISMATCHES
    the same with BOF3X_RUBYPROBE set                    exit 0, the same counts

The fuzz runs inside `MsgBox_Inject`, before `RubyProbe_Inject` touched the
callee table, so the second line shows only that the probe's start-up did
no harm; the first is the identity the brief asked for. With the code
deleted the question is moot for the merge - nothing of the probe is in
`src/` - and it is RR's to answer again for the real repoint.

## 7. The frames and the sheets (outside the tree; game pictures)

- Harness root `C:\Users\kerok\bof3x-runs\rr_pr\`: `g\` the game (the PC
  install's `BGM`, `SND`, `BOF3.exe`; `DAT\` the shipped files plus the
  held `ja-JP` and `en-US` overlays, the two `FIRST.DAT` overlays composed),
  `L\` the ini-less launcher, `shots\<run>\` the frames with each run's
  `.out` and `.log`. Run names: `ja_g<gap>_<point|linear>_w<0|1>` furigana,
  `en_...` Japanese over English, `lat_<half|twothirds>_<filter>_w<n>`
  English over Japanese, `*T_*` bottom-aligned, `*plain_*` the plain page,
  `*_k3` / `*_k4` scales 3 and 4.
- Contact sheets, labelled, in `C:\Users\kerok\bof3x-runs\r_ruby\pr\sheets\`:
  `ja_p1b.png`, `ja_p2b.png`, `ja_p3.png` (furigana; every gap x filter x
  aspect, the box cropped), `en_p1b.png`, `en_p2b.png` (Japanese over
  English), `ja_bottomaligned.png`, `en_bottomaligned.png`, `ja_plain.png`,
  `en_plain.png`, `latin.png` (1/2 beside 2/3, and 2/3 bottom-aligned), `latin_zoom.png`
  and `kana_zoom.png` (4x crops, point against linear), `kana_zoom_k3_point.png`,
  `kana_zoom_k3_linear.png`, `kana_zoom_k4_point.png` (scales 3 and 4).
- The scratch scripts in `C:\Users\kerok\bof3x-runs\r_ruby\pr\work\`
  (`compose.py` the fonts, `build_probe.py` the probe files, `measure.py`
  the box and ink bands, `texel.py` the texel check, `sheet.py`), the
  recipes in `..\recipes\`.

## 8. For RR and SB, and what the probe could not measure

- **A Latin space in a ruby row.** `reading-row.md` 1.2 allows only glyph
  codes, `0x09`, colour and the marker there; a Latin second language has
  spaces, and `0x09` (6 units) is wider than a Latin space at either size
  (4 or 5.33 units at an advance of 8). The probe gave the composed font a
  **blank cell carrying the donor's space advance** and wrote the space as
  that cell's code, which is inside the contract - SB's composed font chunk
  wants the same cell, or the contract a word on `0x20`.
- **The ruby advance is the glyph's own advance times the size**, so the
  composed font's kind-4 table must carry the donor's advances for the
  relocated range (the probe's did); 2/3 advances are fractional (5.33) and
  the probe kept the pen in sixths of a unit.
- **Which glyphs are Latin-sized** was the probe's shortcut (an advance not
  12); RR needs a rule of its own - the relocated range, or the layer's
  name.
- **Reveal order.** The probe counted ruby glyphs in byte order, so a
  reading row typed in whole before its text row; the owner's call 3
  (reading with its word) is RR's, not measured here.
- **The page scroll** between pages drew readings with the clip pair scaled
  to the quad; no frame was taken mid-scroll, so its look is unmeasured.
- **The AREA014 scarecrow's own box**: not reached by any recipe (section
  1); the probe's box is the opening scene's, a standard bottom box. A
  top box (`0x0C` placement) was not seen.
- **Scales other than 2** were captured for the furigana page at gap 1
  only (scale 3 point and linear, scale 4 point); the borderless window's
  monitor-sized scale was not captured.
- **English over Japanese at 1/2, bottom-aligned** was not captured (the
  run was cut while another session held the machine); its table is the
  r = 6 one, captured for furigana and for Japanese over English.
