# The reading-row round: furigana, and a second language, in the row above the text

**Status:** STAGED 2026-10-20, not launched (branch `docs/reading-row-staging`;
briefs in `C:\Users\kerok\bof3x-runs\r_ruby\`). **Group SL ran the same day**
([`second-language-census.md`](second-language-census.md), merged here) and
the owner decided on its numbers: **two text rows plus two reading rows per
page**, overflow accepted as extra confirm presses; and **composed messages
delivered from a side chunk, not re-authored in the arena** (section 4). Asked for by the owner on 2026-10-20
after GitHub issue
[kerokline/BreathOfFire3PCPort#97](https://github.com/kerokline/BreathOfFire3PCPort/issues/97)
("Second Script": furigana or a second language in the rendered script, from
a Reddit request). The furigana half is IDEAS I21 (2026-09-24, "its own
branch and playtesting"); the second-language half is IDEAS I50. Nothing
here is built, measured or in the ledger yet; every number below that is not
cited is the sibling's or a guess, and says so.

## 0. The shape in one paragraph

The dialogue box gets an optional **reading row** above each text row: a
row of smaller glyphs, aligned to the text under it, carrying a *second
script*. Two second scripts are in scope. **Furigana**: the Japanese text
(`ja-JP`, DIV-0056) with every kanji word's kana reading over it, the
sibling repo's `jp_furigana` done the PC's way. **A second language**: the
same message from another language layer the install holds (English over
Japanese, Japanese over English, German over English...), re-flowed into the
reading rows so the two scripts run in step page by page. Both are **text
layers built on the player's machine** from layers the install already has,
never shipped (rule 1); both are drawn by `MsgBox_Step`, which is ours, so
no hook, no span flag and no global pitch are needed (IDEAS I21). The
furigana content that *is* shippable - the chosen readings for words the
dictionary gets wrong - travels as the first **mod zip**, loaded from the
launcher's Mods tab the way a disc is added on the Sources page; the second
language needs no zip, only two held languages. The launcher gains one
dropdown, *Row above the text*: none, furigana, or a language.

**The one fact that makes it cheap here** (the owner, 2026-10-20; I21): the
Japanese sheet's cells are 12 px, doubled to 24 x 24 on the PC and drawn on
a 12-unit advance. A reading glyph drawn on a **6-unit quad is the original
12 px cell texel for texel**: no shrinking, no second font, and **two kana
sit over one kanji cell** (2 : 1). The sibling had to point-sample 12 px
glyphs to 6 px and then dig out the disc's 8 x 8 kana font to be legible,
and its reading row cost it a third of a text row's width per kana; here the
reading row is exactly half a text row high and the arithmetic is integer.
A row pair is 12 + 6 = 18 units against the plain newline's 14, so **two
annotated rows fit where three plain ones do** (6 + 3 x 14 = 48 units of
pen travel today; 2 x 18 = 36) - the same budget the sibling reached, with
room to spare for the owner's eye on the gap.

## 1. What is already known, and the traps

- **The PC box stepper is ours** (`src/game/msgbox.cpp` `MsgBox_Step`,
  [`msgbox.md`](msgbox.md) 3): it re-walks the message from the top every
  frame, draws as many glyphs as `0x7DEE59` says, newline is `pen y += 14`,
  `0x0D`/`0x0E` turn the offset pen on and off, `0x09 0x0B 0x10 0x11 0x16`
  are one byte and draw nothing, `0x0A 0x0C 0x0F 0x14` two bytes. The draw
  goes through `MsgBox_DrawChar` (DIV-0006) to `Text_EmitGlyph` `0x516D50`,
  whose signature is `(x, y, w, h, u, v, clut)` (`symbols.toml`): the quad's
  size is a parameter, so a 6-unit glyph is a call, not a patch.
  `MsgBox_EffectDraw` `0x4987E0` (the growing shout, DIV-0070) is a separate
  draw with no pair handling; it stays unannotated in this round.
- **A language is an overlay layer** (DIV-0005: `DAT\<tag>.<NAME>.DAT`
  walked after the shipped file; `cache/loc/<tag>/`), and **an optional
  layer is the same mechanism with a name** (DIV-0086: `BOF3X_OPT`, a text
  layer's name ends in `-<tag>` and the DLL refuses it under another
  language, `game/language_tags.h` `LayerLanguage`). A reading-row layer is
  an optional text layer of the main language: `ruby-furigana-ja-JP`,
  `ruby-en-US-ja-JP` (second language `en-US` over main `ja-JP`). The
  launcher already lists optional layers and composes `BOF3X_OPT`
  (`config.cpp` `ConfigOptPlayable`); the dropdown is a view over them.
  **Trap:** the DLL reads `BOF3X_LANG` into `char played[8]`; the main
  language tag stays as it is and the second script is named by the layer,
  never by a longer tag.
- **Messages pair across languages by slot.** `loc_build.py convert_block`
  converts a donor block slot by slot onto the PC's slot table; the US disc's
  blocks have the PC's slot counts in 288 of 288 cases
  ([`loc-build-disc-only.md`](loc-build-disc-only.md)), French and German
  point outside their blocks in 15 and 11 areas (slots that are empty on the
  PC). So "the same message in another language" is the same area block, the
  same slot - a lookup that exists today. Pages (`0x02`) and lines (`0x01`)
  inside a message do **not** pair: English runs to more pages than Japanese
  for the same slot. That is what SL measures.
- **The sibling's rules are the starting point, not its code** (rule 5 is
  about `bof3ext`; the sibling is the owner's own repo, so its `readings.toml`
  is the owner's work and may be carried over; its builder is rewritten here
  against the PC's two-byte codes and pair codes, DIV-0057). What it settled
  ([`../BreathOfFire3Recomp/docs/FURIGANA.md`](../../BreathOfFire3Recomp/docs/FURIGANA.md)):
  a word and its reading are one unit that never splits across rows; closing
  punctuation clings to the unit before it; authored line breaks are kept
  where they fit; a page that needs a third annotated row splits on a `0x02`
  (a cost in confirm presses, 2,301 pages there); readings from SudachiPy
  mode C with okurigana already on the page trimmed, `readings.toml`
  overrides first (keyed on surface, dictionary form, or surface plus
  `next`/`prev` context; 何 = なに before a particle), dialect stretching
  (気ィ) re-attached after the reading, numerals' counters by `next` rules;
  the `--selftest` round trip of the unannotated script byte for byte as the
  acceptance test; `--lint` simulating the draw-time cursor over every row;
  a reading wider than its stem hangs right and may take a free half-cell on
  the left; two readings never touch. Readings are **not proofread** there
  (the review pass is the thing still owed, 2026-09-12).
- **Runtime inserts** (`0x03`, `0x04 nn` names; `0x07 nn` item / skill
  records; `0x08 nn` sub-pool messages) have a width known only at draw time.
  The sibling built fragment tables and marker expansion for this
  (`INSERT_MARK`, 109 fragments). Here the stepper knows the inserted bytes
  when it draws them, so the reading row's x after an insert is computed in
  the walk and the inserted *name's* reading can come from a per-name table
  built with the layer - simpler, and stage B's.
- **Pair codes** (DIV-0057) stand for two kana in item, ability and enemy
  names only; a dialogue message never carries one, but an inserted name
  does. The ruby row must count a pair as two cells.
- **Choice blocks** (`0x14`): options are NUL-terminated and the box draws
  them its own way (`MsgBox_ChoiceOpen`...). The sibling lost options by
  stopping at the first NUL. Options are left plain in this round (no
  reading row over a choice), and the layer copies a `0x14` block verbatim.
- **Fonts cannot be resident together as built** (SL, 2026-10-20,
  [`second-language-census.md`](second-language-census.md) 7): the engine
  has one glyph table, every overlay's `FIRST.DAT` carries a whole one, and
  Latin and Japanese both start their cells at glyph `0x993`; 76 of the 90
  single-byte slots differ and both hanging slots `0x2A` / `0x3C` differ. So
  a second-language layer carries a **composed font chunk**: Latin cells
  relocated above the Japanese set (`0xDF2..`) under a Japanese main, or the
  882 Japanese cells at `0xAC0..0xE31` under a Latin main; English against
  French or German differs in letter shapes (98 of 100 cells) with the
  accented block appended where English is blank. A reading-row glyph is
  therefore a code of the relocated range, never an ordinary kana code,
  whenever the second script's font is not the main's.
- **The arena is too small for composed blocks** (SL, section 8): the area
  text block has `0x8000` bytes of room, the largest main blocks are 20 to
  24 KB, and with the second script added sixteen areas pass it in every
  pair (AREA039, 090, 094, 153, 154 and the faerie village 175..185, one
  block of 665 short messages whose largest is the hi-lo tutorial at eleven
  pages - nothing to reuse, the sum is the problem). The owner's decision:
  composed messages live outside the arena (section 4).
- **The second script rarely overflows; the main's third row is the cost**
  (SL, section 4): over Japanese at 1/2 size, English, French and German need
  more than two reading rows on 1 to 2 % of box pages, at 2/3 size 6 to 8 %;
  Japanese over English and Latin over Latin under 1 %. But a Latin main's
  three-row pages are 28 to 33 % of its box pages, and every one splits
  under two row pairs - about 5,300 to 6,000 pages added. The owner's
  decision (2026-10-20): **two pairs per page, consistently**; the presses
  are accepted. Three text rows plus two reading rows was offered and
  declined.
- **The typewriter reveals by glyph count.** The sibling accepted "a reading
  types in just before its word". Here, because the walk is ours, a reading
  can be revealed with its word (RR's design choice, recommended). The
  next-page arrow places off the last row's pen; a page ends on a text row.
- **The layering and wide modes** (DIV-0071, DIV-0041) move the box; the
  reading row is drawn relative to the box's pen like every glyph, so they
  need no change, but the end batch captures 4:3 and 16:9.
- **The state hash** ([`state-hash.md`](state-hash.md)) is the regression
  check: with no reading layer, every route hashes as before. With one, the
  message bytes and the page count differ by design; the comparison is
  pictures, as the localisation rounds did it.

## 2. Stage A (four groups, in parallel)

### PR: the probe (`src/game/msgbox.cpp` behind `BOF3X_RUBYPROBE`; no layer, no builder)

I21's first concrete step. From a field save in front of a talkable NPC
with `BOF3X_LANG=ja-JP`, `MsgBox_Step` draws one hand-written annotated page
(the probe's bytes in a scratch file, read at start) with the row rule:
ruby row first, 6-unit kana on a 6-unit advance, then its 12-unit text row;
pen y per row from a table. Captures (`BOF3X_SHOT_DIR`, the game's own
frames, [`captures-need-the-screen-uncovered`](HANDOFF.md)): the 2 : 1 row
at three gaps between the pairs (0, 1, 2 units), under point and bilinear
filtering (DIV-0012), at 4:3 and 16:9; and the **box's inner height**
measured off the frame (origin `y + 6`, how far the third row's glyphs
reach, where the frame's bottom edge is). A second page for the Latin case:
`BOF3X_LANG=en-US`, a reading row of the donor's glyphs at **1/2 (4 x 6
units, texel-exact at the donor's 8 x 12) and 2/3 (the owner's 2026-10-20
ask; 1.5 : 1 texels, uneven under point filtering)**, so the owner chooses
the second-language row's size by eye. The probe is deleted before merge;
its numbers go to section 5 and the row table RR compiles in.

### FB: the furigana builder (`tools/ruby_build.py`, `data/furigana/readings.toml`)

`ruby_build.py furigana --cache C --lang ja-JP [--only AREA014]` reads the
install's `ja-JP` text chunks (the kind-0 area blocks and the pools the
overlay carries, DIV-0056) and writes the optional layer
`cache/opt/ruby-furigana-ja-JP/` (or `DAT\ruby-furigana-ja-JP.<NAME>.DAT`
over a PC install), one **side chunk of composed messages keyed by slot** per
area (section 4; the text block itself is never rewritten): for every message
whose pages take a reading, each text row preceded by its ruby row. Readings: the
PC's own kana codes (`loc_build.py ja_encode`), SudachiPy mode C, the
sidecar first. **The side chunk and the string shape are RR's contract** (section 4) and is
fixed before either starts: a page-head marker no shipped page starts with,
`<ruby row> 0x01 <text row> 0x01 ...`, half-cell gaps as `0x09`, which the
stepper already skips. `--selftest` (no readings, no re-flow) must
reproduce every message of every held Japanese block byte for byte -
that is the acceptance test. `--lint` simulates the stepper's x over every
ruby row. `--review FILE` writes every reading placed with its context, for
the proofreading pass the sibling never ran; `--ambiguous FILE` the surfaces
the lexicon reads two ways. The sidecar starts as the sibling's 97-line
`names/readings.toml` carried over (the owner's own choices) under
`data/furigana/`. Numbers to report: messages changed, readings placed,
pages split for a third row, readings dropped for no room, the longest
block against the pool's room. **Do not copy a Capcom table**; the
readings are generated from the player's text at build time and the sidecar
is ours.

### SL: the second-language census and the pairing rule - **DONE 2026-10-20**

Run alone on the day of staging, as an Opus agent in a worktree:
[`second-language-census.md`](second-language-census.md) (MEASURED) and
`tools/second_language_census.py`, merged into this branch. Its drafted
pairing rule (section 9 there) is SB's starting point; its open questions
are folded into section 6 below. The text that follows is the brief as
written.

For every pair of held text layers the owner has (`ja-JP`, `en-US`,
`en-150`, `fr-FR`, `de-DE`; the PSP's as well where their layers exist),
over every area block: slots that pair, slots that do not (empty on one
side, the French and German out-of-block slots); per paired message the
page counts of both, the row counts per page, the width of each page's
second-language text in reading-row cells at 1/2 and at 2/3, and how often
it fits over the main page's rows with no re-flow, with the main page's
rows re-broken, or only with a page split. From that, **the pairing rule**
as a draft: pages paired in order where counts match; otherwise the second
script packed into the main's pages in reading order and the main page
split where the second overflows two reading rows, never the other way
round (the main script is the one being played, so it keeps its authored
breaks wherever it can). Also measured: whether two overlays' glyph tables
can be loaded together (the Latin cells against the Japanese `0x993..`;
`0x2A` / `0x3C` the Japanese overlay repaints), by loading both and reading
`BOF3X_TEXTLOG`. A doc, `second-language-census.md`; no code merged.

### ML: the mod zip and the Mods tab (`src/launcher/`, `tools/mods.py`, `tools/importer.py`)

The first real mod. A mod is a zip holding `mod.toml` and data files, never
code; `tools/mods.py add ZIP --home H` validates it (a `kind` the tools
know, a `requires` list of language tags, the files it names and nothing
else, sizes bounded), unpacks it under `<home>\mods\<id>\`, and records it
in `mods.toml` beside `sources.toml`; `remove` and `list --json` as the
Sources page's contract ([`install-sources.md`](install-sources.md) 4).
`setup` runs each mod's build step after the language layers: for
`kind = "reading-row"` with `base = "ja-JP"` that is FB's builder with the
mod's `readings.toml`, writing the optional layer. The Mods tab lists mods
with Add zip..., Remove and the greyed-by-source rule
([`config-shows-all-options-greyed-by-source`](HANDOFF.md): a mod whose
`requires` the install lacks is shown and greyed, with the reason). The
Settings page gets the dropdown *Row above the text*: none; each installed
reading-row mod whose base is the language played; each other held language
(the second-language layers, built by `setup` for every held pair, SB). Its
choice is `ruby=` in `bof3x.ini` and one more layer in `BOF3X_OPT`. The
furigana mod zip itself is made by `tools/make_mod.py furigana` from
`data/furigana/` and attached to releases by `release.yml`. Until FB lands,
ML's build step calls a placeholder command that is **refused loudly**
(rule 4: no stub) and the end-to-end check runs once FB merges.

## 3. Stage B (after PR's numbers and FB's layer; two groups)

### RR: the renderer (`src/game/msgbox.cpp`, `text_advance.cpp`; the ledger entry)

`MsgBox_Step` learns the annotated page: for a message opened from the side
chunk (section 4) the row table is PR's (pen y per row, ruby rows at 6 units, text at 12, the gap
the owner chose), a ruby glyph is emitted on a 6 x 6-unit quad from the same
cell, `0x09` advances half a cell, inserts (`0x03`/`0x04`/`0x07`/`0x08`)
report their drawn width back into the ruby row's x, a pair code counts two
cells, a reading is revealed with the first glyph of its word (the count
`0x7DEE59` counts text glyphs only on an annotated page), the next-page
arrow and `MsgBox_State3Step`'s scroll see the page's last *text* row.
`MsgBox_EffectDraw`, choice options and every other text path are
untouched. **Identity when off:** a page without the marker walks exactly as
today - the `msgbox` shadow fuzz ([`msgbox.md`](msgbox.md) 5) at 0
mismatches, `BOF3X_ORIGINAL=MsgBox_Step` still Capcom's body, which draws an
annotated page wrongly but does not fault. Hand-back: captures of the AREA014
scarecrow talk and the AREA150 desert prompt (the sibling's two reference
scenes) and of a pickup line with an inserted item name; the ledger draft.

### SB: the second-language layer (`tools/ruby_build.py second`, from SL's rule)

`ruby_build.py second --cache C --lang ja-JP --over en-US` writes
`ruby-en-US-ja-JP` by SL's pairing rule, using FB's emitter and RR's string
shape; the other language's glyphs on the reading row at the size the owner
chose from PR's Latin captures; a composed font chunk if SL found the two
tables collide; names and inserts drawn from the *other* layer's name table
where RR reports an insert (stage B of its own if it does not fit the
round). `setup` builds every held pair. Captures of the same two scenes,
English over Japanese and Japanese over English.

## 4. The contract between FB, SB and RR: the side chunk and the string shape

Decided by the owner on 2026-10-20 after the census (section 1's arena
finding), to be written out by the coordinator in `docs/reading-row.md`
section 1 before FB and RR launch:

- **Delivery: a side chunk, not a rewritten block.** The arena's text block
  stays exactly as the language layer built it. A reading-row layer adds,
  per area file (and per pool), one chunk of a new kind of ours holding
  **composed messages keyed by slot**: a slot table and the composed bytes.
  `LoadDatFile` (ours) keeps it in our own memory with no arena limit; when
  `Msg_OpenScript` (ours) opens slot `n` and the loaded chunk has a composed
  message for it, the box's base and read pointers take the composed copy,
  else the original - the sibling's `MsgBox_Reset` repoint, done natively.
  A slot with no composed message plays exactly as today, which is also what
  keeps the shadow fuzz and the state hash meaningful. Every area gets the
  same mechanism; the sixteen big areas are not a special case.
- **The composed message's shape.** A page is `ruby 0x01 text 0x01 ruby
  0x01 text`, ruby rows first so a page ends on text and the arrow places
  off a text row; **an empty text row under a reading row is allowed**
  (SL's gap: the second script may have text where the main has none);
  **two pairs per page**, a third pair goes to a new page on `0x02`; a ruby
  row's cells are half-cells of the text row, `0x09` a half-cell gap; a
  reading's glyphs are ordinary two-byte kana codes for furigana and codes
  of the composed font's relocated range for a second language whose font
  is not the main's; inside a ruby row only glyphs, gaps, `0x05 nn` / `0x06`
  and **one insert marker** (a one-byte code the stepper never sees in a
  shipped message, chosen and recorded in `reading-row.md` 1) that stands
  for "the width of the k-th insert in the text row below", which the
  stepper resolves at draw time; every other control stays in the text row
  verbatim; a `0x14` choice block is copied from the main from its first
  byte. The stepper knows a page is annotated because the message came from
  the side chunk, so **no page-head marker** is needed and no shipped byte
  changes meaning.
- **The hanging brackets** `0x2A` / `0x3C` are written as two-byte codes in
  a ruby row so they do not hang at a row start (SL 9).

## 5. Stage C: the end batch, then the owner's play

1. Build, `--selftest` on every held Japanese layer, the shadow fuzz, the
   text and menu self-tests ([`self-tests-need-a-clean-launcher-dir`](HANDOFF.md)).
2. State hash on the ten routes with no reading layer: identical to the
   `cn_<route>_orig3` references ([`state-hash.md`](state-hash.md)).
3. Captures with `ruby-furigana-ja-JP` and one second-language pair over
   the attract sequence and the dialogue-heavy routes, 4:3 and 16:9, point
   filter, in the contact-sheet form - plus the faerie village's minigame
   tutorials (the block's longest messages, SL; **a new recipe the owner
   records**, since no route reaches them yet); the owner's eye on every sheet (per
   [`owner-tunes-layout-by-eye`](HANDOFF.md): their words, applied
   literally).
4. The ledger entries numbered from the ledger's tail at merge time (the
   away-day branch holds the next number today), one for the reading row and the
   furigana layer, one for the second-language layer, one for the mod
   mechanism; the defects numbered; [`owner-review.md`](owner-review.md)
   gets the calls below; I21 and I50 to built; the README row; HANDOFF.

## 6. The owner's calls (asked here, answered in chat or `owner-review.md`)

**Decided 2026-10-20 (chat, on SL's numbers):** two text rows plus two
reading rows per page, consistently, overflow as presses; composed messages
from a side chunk (section 4); the default pair is left to the numbers,
which favour a Japanese main with an English second (SL 10).

1. **The gap between row pairs** and the Latin row's size (1/2 or 2/3), off
   PR's captures. SL's numbers for the size: 2/3 overflows on 6 to 8 % of
   Latin-over-Japanese pages against 1 to 2 % at 1/2.
2. **Should furigana be a mod zip at all?** Everything but the override list
   is in the tree. The case for the zip: it is the first real mod and proves
   the Mods tab end to end, and the proofread readings can be re-issued
   without a release. The case against: a dropdown entry that is simply there
   when `ja-JP` is held is one step fewer for the player. Staged as the zip
   (the owner's framing of 2026-10-20); ML builds the mechanism either way,
   so the answer changes one default, not the round.
3. **Reveal rule:** a reading with its word (recommended), or the whole ruby
   row with the row's first glyph.
4. **Readings proofread by whom?** `--review` writes the list; the round does
   not include a proofreading pass. Issue #97's poster may want it.
5. **Choice options** plain for now - agreed, or wanted in this round?
6. **The second-language default pair** to build on `setup` when more than
   two languages are held (every pair is cheap; the dropdown shows them all).
7. **From SL (section 10 there):** names on the reading row from the main's
   name table or the second's own; proportional sharing of the second's text
   when page counts differ (drafted) or strict carry-forward; non-box and
   offset-pen pages left plain (drafted); under a Latin main, the second in
   the main's letter shapes or its own.

## 7. Not in this round

- Readings or a second script in menus, battle text, the shout draw
  (`MsgBox_EffectDraw`), the world-map plates, names in lists (`Text_DrawString`
  paths) - dialogue box only.
- A proofreading pass over the readings (6: owner's call 4).
- A third script, or two reading rows.
- Mods of any other kind (art, scenario): ML's mechanism leaves the `kind`
  field open and refuses what it does not know.
- The PSP's text layers as a second language: measured by SL where held,
  built by SB only if the census shows them pairing like the PSX's.

## 8. The brief, per group

`C:\Users\kerok\bof3x-runs\r_ruby\BRIEF_<group>.md`, one per group (PR, FB, SL, ML;
RR and SB when stage A's numbers are in). Each names the goal, the files it
owns and must not touch, the docs to read first (this file; I21 and I50;
[`msgbox.md`](msgbox.md) 3; [`dialogue-localisation.md`](dialogue-localisation.md)
1, 2 and 9; DIV-0005, DIV-0056, DIV-0057, DIV-0086; the sibling's
`docs/FURIGANA.md` for FB and SL; [`install-sources.md`](install-sources.md)
for ML), its harness root under `C:\Users\kerok\bof3x-runs\<group>` (never
another session's), the checks that mean done, and the hand-back report's
shape. Rules 1, 4 and 7 restated. Worktrees from `main` at launch; a usage
cut resumes with a message naming the next step; only the hand-back report
means merge. Merge order: PR's numbers first (a doc, not code), then FB, ML,
SL's doc; RR after FB; SB after RR and SL.
