# The dialogue's button codes

**Status:** MEASURED, the build half (2026-10-17, the button unification
round's group DT-1, branch `buttons/dt1` off `839a3e1c`). Built for every held
disc and diffed against the build before it; nothing drawn differently, by
reading and by a model of both text readers (section 2.3). Not played. The
draw half (DT-2) is not built: until it is, the codes are inert.

The round is [`button-unification-round.md`](button-unification-round.md); this
is its section 2 "DT-1". The census is
[`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.6 and
[`button-roles-by-release.md`](button-roles-by-release.md) 5, the maps `P` and
`S` that doc's section 2, the owner's answers its section 8. Rule 1: no line
of dialogue is quoted here; scenes are described.

## 1. What the build writes

### 1.1 The code

Every release names a button in running text the same way: a colour control
and an icon glyph, `05 cc` then the icon. `tools/loc_build.py` now writes, in
front of each such mention in the nine areas' text blocks:

    05 (0xB0 + n) 06   05 cc  <the release's own icon glyph>

`n` is the index of **Japan's bit** - the bit the PC's code tests, whatever
the release's text drew: triangle `0x10` is `0xB4`, circle `0x20` `0xB5`,
cross `0x40` `0xB6`, square `0x80` `0xB7` (`BUTTON_ARG0 + bit.bit_length() - 1`;
`code_bit` is the inverse). The `05 cc` and the icon after it are the
release's own bytes, unchanged: the icon is the fallback that draws until
DT-2 resolves the code (section 2), and the colour is the release's.

The code is a colour control with a colour no text uses, closed at once by
`06`. Why that shape and not a box selector is section 2.2.

### 1.2 Which icons, and their bit

| Source | The icon | Its button, in the release's terms | To Japan's bit |
|---|---|---|---|
| US, EU (English), FR, DE discs | glyph `0x80..0x83` after `05 cc` | circle, cross, triangle, square | `P^-1` |
| PSP-EU | the same bytes | the same | `S^-1`; areas 68 and 121 by `P^-1` (owner's answer 3) |
| JP disc, PSP-JP | `15 08`, `15 09`, `15 0A`, `15 0B` after `05 cc` | circle, cross, triangle, square | identity |
| the PC's own text, in a message a layer keeps | one of the port's single-byte icon glyphs (`b`..`g`, `X`) after `05 cc` | **by the colour**: 2 circle, 1 cross, 6 triangle, 5 square | identity |

- The release's map is chosen by **what the disc is** (`release_map`), not by
  its tag: PSX-EU and PSP-EU are both `en-150`.
- **JP was not in the brief**; it is here because the round's section 0 maps
  JP by the identity and a `ja-JP` layer's icons would otherwise stay baked.
  `15 08` circle, `15 09` cross and `15 0B` square are measured (the JP
  disc's text names the PC's buttons area for area); `15 0A` triangle is by
  the order only - no text uses it.
- **The PC's colour cue**: the port's dialogue does not use its Config's
  code-to-button assignment (circled 3 is triangle there, but in the text it
  is circle in area 1 and square in area 121); the colour is the cue. The
  colours are per drawn shape and the same on the US disc's text (cross 1,
  triangle 6, circle 2). Key-icons 2.6 and the brief give the colours as 25,
  4 and 1; the bytes after `05` measure 2 (circled 1, and circled 3 in area
  1), 1 (`X`) and 5 (circled 3 in area 121) (`census.py`, section 3.3), so
  those numbers are not the control's argument; what they count was not
  traced.
- The areas (`BUTTON_AREAS`): 8, 30, 68, 75, 89, 111, 121, 129, 135, and the
  PC's area 1. Area 78's crosses and the faerie village's times sign are
  outside them and get no code; the selftest checks both shapes are left.

### 1.3 What the code does not reach

- **The PC's own Chinese text with no layer.** `loc_build.py` builds layers
  from discs; the PC's text is the shipped `DAT/` (or the cache's
  `loc/zh-CN`), which no tool rewrites, and the engine reads no `zh-CN`
  overlay. So on a PC install without a language layer the dialogue's 31
  mentions carry no code. The colour-cue converter (`pc_button_codes`) runs
  on every PC message a layer keeps in the nine areas; in today's five
  PC-path layers it found none (every slot with a mention has a donor
  message). What DT-2 does for the bare PC is an open question (section 3.6).
- Text outside the area text blocks (the system pools, the exe's strings):
  not coded, and not searched for mentions here (the census, key-icons 2.6,
  is of the area text; the fishing banner's labels are IC-F's site).

### 1.4 The build

- `convert_run` writes the code when `05 cc` is followed by the release's
  icon and the area is a button area (`set_button_area`, set by `build_all`
  around each area's text block and cleared after). It refuses a donor that
  already holds a colour in `0xB0..0xB7` (a `SystemExit` naming the bytes),
  as `pc_button_codes` refuses such a PC message.
- `build_all` prints the codes it wrote per area and by which map.
- `convert_block`'s disc-only path now converts each message once (it used to
  convert a second time to count the kept slots, which would have counted
  every code twice); the kept count is the same.
- `loc_build.py buttons --dat DIR [--lang TAG] [--pc-text]` is the census: for
  each area text block, every code (with the glyph and colour after it) and
  every icon left without one, counted - no text printed.
- `loc_build.py selftest` (CI) gains `selftest_buttons`: every icon by `P`
  and by `S`, PSP-EU's 68 and 121 by `P`, a JP icon, the PC's four colours,
  areas 78 and 175 left, an icon not right after its colour left, a colour
  in the range refused, and the census reader against the converter.

## 2. Why it draws as today

### 2.1 The two readers

The area text block is the script pool `MessagePools` (`0x803580`), and two
functions draw from it:

- **The message box**, `MsgBox_Step` and `MsgBox_StatePrint`
  (`src/game/msgbox.cpp`). `05 nn` takes colour `nn`, `06` takes colour 0;
  neither is a character (the drawn count `0x7DEE59` counts glyphs only), and
  `MsgBox_StatePrint` keeps going over both in the same frame (only the
  codes that set a state or start a substitution clear its flag). The
  code's colour is never used: the `05 cc` right after it sets the colour the
  icon is drawn in, as before.
- **`Text_DrawString`** (`src/game/text_draw.cpp`, original `0x516B70`),
  through `Text_DrawAt`: `EffectKind0F_DrawMessageList` (`0x468560`, the
  fishing panel's list) draws pool messages by the id table `0x653E00`
  (`src/game/effect_1b.cpp`), and **all five fishing mentions are on that
  list**: in area 30 the five slots that hold one (114, 118, 123, 133, 143)
  are among the table's 29 ids (read off `BOF3.exe` and the PC's text,
  `listids.py`, scratch; areas 89 and 129 carry the same text). They are one
  line each with only colour controls. Here `05 nn` remembers the colour and takes
  `nn`, `06` takes the remembered one back. So after `05 X 06` the colour and
  the remembered colour are what they were before it, whatever came earlier,
  and the `05 cc` ... `06` that follows behaves as it did. The cost: two of
  the draw's `count` passes (the list passes `0xFF`; the longest coded
  message is far below it).

### 2.2 The design that was dropped

The first build used the box selector, `0C (0xB0 + n)` after the `05 cc`: the
message box steps over a `0C` and its argument, and `MsgBox_Reset` eats only
a leading one. But `Text_DrawString` has no case for `0x0C`: it draws it as
glyph `0x0C - 0x26`, which wraps past the table and reaches `GlyphTrap` - a
fault. The fishing list would have crashed the game in areas 30, 89 and 129.
The `05 X 06` code is safe in both readers.

### 2.3 Checked

- **The argument range is unused.** Every area text block of the PC's `DAT/`
  and of all seven held discs (US, EU, FR, DE, JP, PSP-EU, PSP-JP), every
  message walked: `0x05`'s arguments are `0x01..0x07` and nothing else
  (`census.py`, scratch). The box selector's are `0x00..0x06`, `0x43` and
  `0x80..0x86`, head and mid-message - which is why the dropped design had a
  free range too.
- **Only the codes were added.** Each layer built before and after, file for
  file: in every layer the nine area files differ and nothing else; removing
  every `05 Bn 06` from the new text block (and re-pointing its slot table)
  gives the old block byte for byte (`cmpdirs.py`, scratch). Section 3.2.
- **Both readers draw the same.** A model of `MsgBox_Step` (glyph, colour,
  pen, the offset pen, every event control) and of `Text_DrawString`
  (glyph index, CLUT nibble, pen, the remembered colour, count `0xFF`, the
  trap) run over every changed slot, before and after: the draw lists are
  equal in every slot of every layer (`drawmodel.py`, scratch). Section 3.2.
  The same model on the dropped design (2.2) reaches the trap.
- **Not checked:** a capture. No route opens these messages (key-icons 2.6);
  RC's site routes are the round's evidence. A reader of the pool that is not
  ours and not one of the two above was not searched for in the binary;
  the two above are the ones `src/` reaches the pool through.

## 3. The census, and what is left

### 3.1 Codes per release, against the census

Counted by `loc_build.py buttons` on the built layers, and by the build's own
report; "Japan's" is the bit each code names.

| Area | Scene | PC's own text (colour cue, no code) | US / EU / FR / DE codes | PSP-EU codes | JP codes |
|---|---|---|---|---|---|
| 1 | (PC only) | circle x1 | - | - | - |
| 8 | Bunyan's wood chop | cross x2 | cross x2 | cross x2 | cross x2 |
| 30, 89, 129 | the fishing help | circle x5 each | circle x5 each | circle x5 each | circle x5 each |
| 68 | the well | cross x2, circle x2 | cross x2, circle x2 | cross x2, circle x2 | cross x2, circle x2 |
| 75 | the rope pull | circle x3 | circle x3 | circle x3 | circle x3 |
| 111 | the antenna | circle x1 | circle x1 | circle x1 | circle x1 |
| 121 | ramming the Black Ship | square x1 | square x1 | square x1 | square x1 |
| 135 | the crane | circle x4 | circle x6 | circle x6 | circle x4 |
| | **total** | **31 in 10 areas** | **32 in 9 areas** | **32 in 9 areas** | **30 in 9 areas** |

Every code names the button the PC's text names in the same area: the
Western and PSP-EU icons translated by their maps land on Japan's buttons,
mention for mention, which is the census's claim checked on the output.
The two counts that differ are the releases' own text:

- **Area 135:** the Western text and PSP-EU's name the crane's button six
  times, the PC's and the JP disc's four. Correction to
  [`button-roles-by-release.md`](button-roles-by-release.md) 5, whose PC (JP)
  column reads six there, and whose "31 mentions" for the Western discs is
  32 (its own table sums to 32).
- **Area 1:** the PC's one mention (circled 3 in the circle colour) stands
  where the JP disc has a kanji in that colour, not its icon code. No Western
  disc names a button there. The brief's reading (circle) is kept.

Every coded layer has **0 icons left without a code** in the nine areas.

PSP-EU's map, seen in the data: in area 8 its icon is the circle glyph (`S` of
the West's triangle) but its colour is still the triangle's, 6 - the icon was
changed and the colour left. With a code in front, DT-2 can colour by the
drawn shape.

### 3.2 The check outputs

- `loc_build.py all --discs <the JP, EU, FR, DE and both PSP images> --disc
  <the US image> --game <a copy of the PC install>`: en-US, en-150, fr-FR,
  de-DE 247 overlay files each, ja-JP 245, the two PSP images skipped as
  before; each layer 32 codes (ja-JP 30). PSP-EU through its cache, as
  [`loc-build-psp.md`](loc-build-psp.md) builds it: `all --disc <PSP-EU> --lang
  en-150 --cache <a copy of the A/B/C harness's c_pspeu>`, 254 files, 32
  codes. The discs and the harness cache were read, not written: the PC
  install and the cache are copies in the session's scratch.
- Before against after (`cmpdirs.py`): en-US, en-150, fr-FR, de-DE, ja-JP:
  1,188 files equal, 45 differ (nine per layer, the nine areas' text chunk),
  each "only the codes added"; PSP-EU 245 equal, 9 differ, the same.
- `drawmodel.py`, changed slots and slots whose draws are equal in both
  readers: en-US, en-150, fr-FR, de-DE 29 of 29 each, ja-JP 28 of 28, PSP-EU
  29 of 29 (a slot can hold two mentions).
- `loc_build.py buttons`: each Western layer and PSP-EU 32 codes (circle 27,
  cross 4, square 1), ja-JP 30 (circle 25, cross 4, square 1), 0 icons
  without a code; the PC's `DAT/` with `--pc-text` 31 uncoded mentions in
  10 areas, the census.
- `loc_build.py selftest`: ok, with the new `button codes` line.
- `importer.py check`: 742 files, 3,582 chunks, 0 errors (and its lint of the
  tools' globals).

### 3.3 The scripts

The scratch scripts (`census.py`, `cmpdirs.py`, `drawmodel.py`, `listids.py`) read game data
and print counts; they are not committed. `loc_build.py buttons` is the
committed census.

### 3.4 Ledger draft H (for the coordinator to number)

> **The dialogue names buttons by code** (amends DIV-0005's family: the
> language layer's text).
> - **Original behaviour:** a button in running text is an icon glyph after a
>   colour control, baked per release: the PC's text draws Japan's buttons
>   (the port's circled numerals and `X`, read by their colour), the
>   PlayStation West's and PSP-EU's draw their own (`P`, `S` of Japan's), and
>   a Western layer copied those over the PC's code - so under every Western
>   layer the rope pull, the fishing help and the crane name a button the
>   code does not test.
> - **New behaviour:** `tools/loc_build.py` writes a button code, `05 (0xB0 +
>   n) 06`, before each mention in the nine button areas, naming Japan's bit
>   `1 << n` (the release's icon read by `P^-1`, `S^-1` - by `P^-1` in PSP-EU's
>   areas 68 and 121, the owner's answer 3 - the identity for JP, the colour
>   for the PC). [DT-2's half:] the text draw shows the glyph of `σ(bit)` in
>   the icon's place, so the dialogue names the button the Config has put
>   that role on, in every language.
> - **Rationale:** the owner's "unify every button flow" (2026-10-17); fixes
>   key-icons 9's first defect.
> - **Known and accepted:** the PC's own Chinese text with no layer carries no
>   code (section 1.3). Text_DrawString's count budget loses two passes per
>   code.
> - **Verification:** section 2.3; DT-2's captures through RC's routes.
> - **Also in the PSX version?** No: each release's text names its own
>   buttons; the code is ours.
> - **Reversible?** DT-1 alone changes nothing drawn. [DT-2's switch.]

### 3.5 What DT-2 must do

1. **Recognise the code** in both readers: `05 Bn 06` immediately followed by
   `05 cc` and a glyph (one byte, or two when the first has bit 7). In
   `MsgBox_Step` (the minigames' talk) **and in `Text_DrawString`** (the
   fishing help, 15 of the 32 Western mentions, is drawn only there, by the
   fishing list); `MsgBox_StatePrint` needs nothing: it already steps over
   the code in the same frame. `code_bit` in `loc_build.py` is the reference
   decoding.
2. **Draw the glyph of `σ(1 << n)`** in the icon's place and skip the
   fallback glyph; the glyph set is the layer's own icons (`0x9E3..0x9E6` in
   a Latin layer's table, the JP sheet's in `ja-JP`), or DIV-0051's
   PlayStation shapes - DT-2's choice, and the width must re-flow the line as
   the layer's widths do if it differs from the fallback's.
3. **Recolour by the drawn shape**: the `05 cc` is the release's colour for
   the release's shape (cross 1, circle 2, square 5, triangle 6 on the PC
   and the West; PSP-EU's area 8 keeps the triangle's colour on a circle).
   Under `σ` the shape changes, so the colour should be the drawn shape's.
4. **Then remove the fallback** from `loc_build.py` (the brief's last step)
   only if the draw no longer needs it to measure the line; keeping it is
   harmless to both readers.
5. **The bare PC** (section 1.3): decide whether the draw resolves the PC's
   colour cue directly in the nine areas, or a `zh-CN` text layer carries
   the codes (an importer or engine change), or the PC keeps Japan's baked
   icons until a Config swap matters.

### 3.6 Open questions

- The bare PC's 31 mentions (3.5 item 5): which of the three.
- Area 1's PC-only mention stands where JP has a kanji, not an icon: is the
  port's icon a button there at all? Coded as circle (the brief's reading)
  only in a layer that keeps that message - none does today.
- Whether any reader of the script pool outside `src/` (a function not yet
  ours) walks these messages; the build is safe for the two that do.
- PSP-JP was not built (no PSP-JP cache was at hand); its map is the identity
  and its text the JP disc's, so it should give the JP column.
