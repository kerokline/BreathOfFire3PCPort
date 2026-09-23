# The in-game Config screen: where its text lives, and how it was translated

**Status:** CONFIRMED in game by the owner (2026-09-21) — layout, lettering
and the selected row. The buttons above the panel are DIV-0018 (§8).

The Config screen is menu state 7, `0x5902E0`
([`menu-screens.md`](menu-screens.md) §1), whose code sits at
`0x460C40`..`0x461E10`. Its panel frame was the subject of DIV-0011. This
document is about its **text**, which is the one part of the menu that no
`DAT` carries and no message pool holds: it is in `BOF3.exe` itself.

## 1. Three shapes, three keys

Read 2026-09-20 with `tools/pe_disasm.py` over `0x461710`, `0x461800`,
`0x461970`, `0x461AF0` and `tools/pe_xref.py` on the addresses they name.

**Six row labels.** The panel draw `0x461710` loops six rows, and the row draw
`0x461800` puts six string addresses on its stack as immediates —
`0x669F0C`, `0x669F18`, `0x669F24`, `0x669F2C`, `0x669F34`, `0x669F3C` at
`0x461832`, `0x46183A`, `0x461842`, `0x46184A`, `0x461852`, `0x46185A` — and
indexes them by the row. Each string is 4 or 8 bytes with padding to 8 or 12.

**Seventeen option strings.** `0x461970` reads two parallel byte tables,
`0x653808` (row → first record) and `0x653810` (row → option count), then
walks 16-byte records at `0x6536F8 + 16 * index`: `[0]` a count used for the
half-width, `[1]` a signed x, `[2]..` the string. The two tables are

```
0x653808   0  3  7 11 13 15  0  0
0x653810   3  4  4  2  2  0  0  0
```

so the rows hold 3, 4, 4, 2, 2 and 0 options, and records 15 and 16 are
**unused**.

**Six controller-panel names.** `0x461AF0` draws one string from the pointer
table `0x66A368`, whose six entries point at `0x66A338` + 8·n.

## 2. What it says

Rendered from the shipped glyph table (`FIRST.DAT` kind-3, the codes are the
indices — [`dialogue-localisation.md`](dialogue-localisation.md) §2):

| Row | Chinese | Options |
|---|---|---|
| 0 | 讯息速度 message speed | 快 / 中等 / 慢 |
| 1 | 视窗颜色 window colour | 黑色 / 蓝色 / 绿色 / 红色 |
| 2 | 背景 background | 木纹 / 砖瓦 / 磁砖 / 绿色 |
| 3 | 音效 sound | 立体声 / 单音 |
| 4 | 冲刺 dash | 手动 / 自动 |
| 5 | 控制 controller | — |

Controller panel: 会话 / 移动 / 动作 / 选单 / 视点 / 交换.

## 3. The donor: the same structures, in `START.EMI`

The US disc carries the Config screen in `BIN/ETC/START.EMI` — the same file
the title menu comes from ([`title-menu.md`](title-menu.md)) — with **the same
two tables, byte for byte**: `00 03 07 0B 0D 0F 00 00` and
`03 04 04 02 02 00`. That pair is what locates the records, which sit
immediately before it, 8 bytes each there against the PC's 16. The labels are
six NUL-terminated strings before a seven-byte table `01 06 02 03 04 00 00`,
and the controller names are six 21-byte records after it.

Both builds also carry the same two **unused** records 15 and 16. One source
tree, as [`SHARED_SOURCE.md`](SHARED_SOURCE.md) would predict.

The US wording, extracted 2026-09-20 and confirmed against a photograph of the
running PlayStation screen the owner supplied the same day:

| Row | Label | Options (count, x from the disc) |
|---|---|---|
| 0 | Msg Speed | Fast (4, 30) · Med. (4, 66) · Slow (4, 102) |
| 1 | Window Clr | 1 (1, 20) · 2 (1, 51) · 3 (1, 82) · 4 (1, 113) |
| 2 | Background | 1 · 2 · 3 · 4, the same four |
| 3 | Sound | Stereo (6, 38) · Mono (4, 102) |
| 4 | Autorun | Off (6, 38) · On (4, 102) |
| 5 | Controller | — |

Controller panel: Speak / Move / Action / Menu / View / Change.

Two rows are not translations of the Chinese but different content: the port
**names** the window colours and the backgrounds where the US release numbers
them, and the port's row 4 is 冲刺 (dash) manual/auto where the US release's
is Autorun off/on. DIV-0015 takes the US wording; the port's is arguably
better and is left as a later choice.

`Off` carries the count 6 it inherited from `Stereo`, so on the disc it hangs
where `Stereo` hangs. The counts and x bytes are taken **verbatim**, quirk
included, because they are what puts an option where the disc puts it.

## 4. How it is delivered (DIV-0015)

`tools/loc_build.py` finds all three blocks **by structure, not by English
words** — the two tables for the records, the seven-byte table for the label
and controller boundary — so a German or French disc reaches the same code.
Every string is re-encoded through `encode_char`, the same path the dialogue
takes. The result is one chunk of **kind 7**, ours, tag 0, in
`en.FIRST.DAT`; 191 bytes on the US disc.

`src/game/config_text.cpp` applies it:

- option records and controller names are **written in place** — the longest
  English controller name, "Change", is 7 bytes with its NUL against 8 of
  room, and the longest option, "Stereo", is 7 against 14;
- the six labels are **re-pointed**, not overwritten: "Background" needs 11
  bytes where the Chinese string has 4. Six operand patches, each validated
  against the address it must hold, aimed at static buffers in the DLL.

### The layout arithmetic, which we do not touch

`0x461800`, `0x461970` and `0x461AF0` each place their string by subtracting a
width from an anchor, and each has two branches: one that computes `n * 12`
and draws through `Text_DrawAt`, and one that computes `n * 8` and draws
through the small UI font `0x516E70`. The labels subtract a **full** width, so
they are right-aligned on a common edge; the options subtract **half**, so
they are centred on their x.

**Which branch, and what size, is now measured** - off the owner's screenshot
of the Chinese screen, 2026-09-20 (609 x 418, so a scale of 0.95 from the
game's 640):

- four-character labels ink from x=101 to x=159, two-character labels from
  x=132 to x=160. **All six end together**, so the full-width, right-aligned
  form is what runs;
- the advance works out at about 16 native pixels a character, against about
  24 for the banner 设定终了 and the buttons above the panel.

So the draw's "units" are 2 screen pixels: `Text_DrawString`'s 12-unit quad is
24 px and `0x516E70`'s 8-unit quad is 16 px. **The Config screen is the small
one.** The names "12 px" and "8 px" in these documents are units, not pixels.

**What the small quad shows is the whole 24 x 24 cell, scaled to 16 x 16** -
not a 16 x 16 corner of it. The proof is the Chinese screen itself: a
full-width Chinese character fills its cell, and it renders legibly at
16 pixels here, which a crop could not do.

The code says the same: the emitter `0x516D50` writes the quad's texture
extent as a flat `0xC` units whatever width and height it is handed, and
`0x516E70` hands it 8.

That decides both the cells and the encoding:

- **The cells are the donor's 8 x 8 UI ones** (`0xA00` up), stored
  **tripled** so that they fill the 24 x 24 glyph; scaled by two thirds they
  arrive at 16 x 16, the PlayStation's doubled 8 x 8 - texel for texel under
  a point filter. Measured off the owner's PlayStation screenshot with the
  panel as the ruler (1305 px for 260, 512 px for 115): label ink is 37 px,
  8.3 rows, on a 41 px pitch, 8.2 columns; the banner's is 54 px, 12.1 rows.
  An 8-row font on an 8 advance under a 12-row banner.
- **Every character is two bytes**, even where a one-byte slot holds the same
  letter. A Chinese character is two bytes, so `4 * len` is `8 * chars` - a
  full width at 8 units - and the arithmetic is exactly right only if English
  keeps that ratio. `config_encode` forces it, with the blank glyph after the
  8 x 8 set serving as the space.

Three earlier builds were wrong, in turn. The first used single-byte codes
and patched three half-width computations to compensate - wrong glyphs, then
code changed to hide it. The second used the 8 x 8 UI set **doubled** into a
corner of its glyph, believing the quad cropped that corner; the letters came
out two thirds of the disc's size. The third read that as the wrong font and
moved to the 8 x 12 cells, which came out the right height and two thirds of
the width - thin letters with wide gaps (measured off the owner's screenshot:
ink 15 tall, about 10 wide on a 16 pitch). The font was right all along, as
the owner held; the scale was wrong. The owner caught all three, from
screenshots.

## 5. Verified, and what is not

**2026-09-20, this machine.** The extractor reproduces the owner's screenshot
of the PlayStation screen string for string. Under `BOF3X_LANG=en` the game
starts, the log carries `DIV-0015: Config screen - 6 labels, 17 options, 6
controller names`, and a read-only `ReadProcessMemory` sample decodes all
twenty-nine strings back to that text - labels through the six patched
operands, options in their records with the disc's counts and x bytes,
controller names through the rewritten pointer table - every glyph in the
`0xA00` set.

**Seen in game, 2026-09-20.** Three rounds of the owner opening the screen and
saying what was wrong, which is what set the layout:

1. text the right size and readable, but the label column ran off the left of
   the panel and sat a little high;
2. down two pixels - right - and the column moved right by 69, the furthest an
   `imm8` anchor reaches. Too far: the labels ran into the separator bar;
3. back 32, to a right edge of 205. Confirmed, and the lettering read to the
   owner as the PlayStation release's.
4. with the tripled 8 x 8 cells (section 4): "much closer", and the text two
   pixels low - sitting on the row's floor where the disc centres it. The drop
   of round 2 had been compensating for cells that sat in the top of their
   slot; it is removed, and the rows' y is the original's again.

The 237 of round 2 came from my measuring their photograph of the PlayStation
screen and putting the disc's right edge near 251. That was wrong, and the
`imm8` ceiling I described as the constraint turned out not to bind at all -
the value wanted was below it. Recorded because the lesson is the cheap one:
on this screen the owner's eye settled in three rounds what pixel archaeology
on an upscaled photo got wrong twice.

## 6. The donor has a second Latin set, and it is the one this screen uses

**Owner, 2026-09-20**, reading the US atlas: rows **120-144 hold a second set
of letters at 8 x 8**, complementing the 8 x 12 set at 72-108 that
`loc_build.py` imports (`CELLS_Y0 = 72`). Both carry lowercase; the caps-only
sets at y=0-22 and y=24-60 are neither.

That matches the code exactly. `0x516E70` draws **8 x 8** quads with a flat 8
advance where `Text_DrawString` draws 12 x 12 - so on the PlayStation the
small UI font is a separate 8 x 8 set, and the Config screen is drawn with it.
**We import only the 8 x 12 set.**

What does not follow straight through is the PC side:

- both PC paths map a byte to the **same** glyph index (`b - 0x26`, or the
  two-byte form), so the port cannot pick a different cell per path;
- the PC table holds **one** Latin set - full width, `A`-`Z` at glyph
  `0x01B`, no lowercase at all (rendered 2026-09-20);
- and `0x516E70` reads its `(u, v)` pairs from `0x65F5A8`, a table of its own,
  where `Text_DrawString` reads `0x65F588`.

What the porting house did is now read (section 4): the small draw samples
the whole 24 x 24 glyph into an 8-unit quad, so one table serves both sizes
and the small font is the big one at two thirds. English wants a second
appended set, because its two sizes are different drawings.

**The set is imported, tripled, and this screen draws with it** (DIV-0016,
2026-09-20): 100 cells at glyph `0xA00` plus a blank for the space, and the
glyph guard follows the table instead of stopping at `0xA00`. `config_encode`
names those glyphs two bytes at a time.

**The budget, as it was.** The free glyph range is `0x993`..`0xA00` - 110
slots, of which the 8 x 12 set already holds 100
([`dialogue-localisation.md`](dialogue-localisation.md) §2; a glyph index above
`0xA00` executes a privileged instruction). Ten left against the ~70 cells an
8 x 8 set needs. A second set therefore needs either a census of Chinese
glyphs the game never draws, to reclaim slots, or the crash boundary moved.

## 7. The selected row is the other branch (DIV-0017)

`0x461800` picks on its fourth argument being 3, which is set only for the row
under the cursor, and `0x461970` does the same for that row's chosen option.
**Confirmed on screen by the owner, 2026-09-21**, on both releases: the
selected row is drawn large.

What "large" is differs. The port draws the same glyph through `Text_DrawAt`'s
12-unit quad and reckons 12 units a character. The PlayStation changes *font*
- the 8 x 12 dialogue cells - and keeps the 8 advance, so the row occupies the
same columns large or small. With English strings naming the 8 x 8 cells, the
port's branch drew a tripled 8 x 8 at full size, crowded on its 8 advance, and
four units a character left of where it belonged.

DIV-0017 retargets the two `Text_DrawAt` calls (`0x46189F`, `0x4619F9`) to a
draw that swaps each UI glyph for the dialogue glyph of the same character,
and turns the two width computations into the small branch's own (`len * 4`,
`count * 8`). The controller panel `0x461AF0` has no large branch.

## 8. Open

- **The caption** ("Close Config", "Set message speed", ...) turned out to be
  English already: it is a system-pool message (`0x497740(0xBC + row)`,
  drawn at `0x460E5B`), so DIV-0007's pool carries it.
- **The two buttons are DIV-0018** (2026-09-21), and they are not this
  screen's: the menu's button-row draw `0x574890` takes a set number, the
  sets are 5-byte records at `0x66383C` (count, up to four verb indices), and
  the verbs are 22 NUL-padded 8-byte slots at `0x66A228` behind the pointer
  table `0x6637E4`. Config is set 6, verbs 11 and 14 (`终了` / `预设值`, the
  US `Quit` / `Init`). Its other callers, `0x5965FB`, `0x59A66B` and
  `0x59B33B`, take the set from a window record's `+0xA`; they draw Items
  (set 0), Equipment (1), Ability (2) and Tactics (7). The US disc has the
  same table in `START.EMI`, `STATUS.EMI` and `BATE.EMI`, with set records
  byte-identical for sets 0 to 7 and a 23rd verb, `End`. One direct use
  besides: `0x59E198` draws verb 0 (`Use`) left-aligned. The labels are
  centred as `6 * n` - the Chinese advance - which is why DIV-0018 also
  re-centres them.
- **Next door on the disc, the stat labels:** the US table is preceded by
  `Int`, `Agl` and their neighbours - the labels Status and Equipment still
  draw in Chinese (攻击 / 防御 / 智力 / 速度, seen 2026-09-21). The obvious
  next converter.
- **Names for these functions in `symbols.toml`.** The four are still
  addresses here.
- **The controller panel's names sit too far left, ragged and oversized**
  (owner's screenshot, 2026-09-22, English, point filter: "Speak", "Move",
  "Action", "Menu", "View", "Change" start from x = 268 to 315 of 640 and run
  out past the panel's left edge, their right ends staggered). Ours, a
  consequence of DIV-0015/0016, not the 2001 port's. `0x461AF0` places a
  name at `x = anchor - len * 6 + 0x20` (`lea eax, [ecx + ecx*2]` /
  `shl eax, 1` at `0x461B36`) and draws it through the large `Text_DrawAt`
  (`0x461B43`) - a right edge at `anchor + 0x20` for Chinese, two bytes and
  12 units a character. Ours are two bytes a character too, but advance 8
  (DIV-0006), so each starts 4 units a character left of where the edge wants
  it: 16 px a letter apart, as measured (4 letters at 315, 5 at 292, 6 at
  268). And the large quad shows the tripled 8 x 8 cell at 24 px on a 16 px
  advance, the crowding DIV-0017 fixed for the selected row. The obvious
  repair is DIV-0017's pair on this function - width `len * 4` at `0x461B36`
  (`89 C8 C1 E0 02`: `mov eax, ecx` / `shl eax, 2`) and the call at
  `0x461B43` re-aimed at `ConfigText_DrawSelected` - then check the right
  edge `anchor + 0x20` against the panel in game: "Change" at 16 px a letter
  may still not fit left of it, and where the US disc puts these names is
  unread. Reached by `tools/recipes/config_controller.txt` (written
  2026-09-22, not yet run), which also shows D17's uneven glyphs
  ([`known-defects.md`](known-defects.md)).
