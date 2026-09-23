# Dialogue-box localisation - what is built, and what it rests on

**Status:** IN PROGRESS (2026-09-21) - English dialogue draws in the attract
sequence and in play, in the donor's glyphs, at the donor's advance; menus,
Config, the menu's buttons, the battle's command labels and New Game's names
are English too (§8). Measurements below are
dated; everything else is marked as intent or as a guess.

Stage 2 of the owner's order of work ([`STATUS.md`](STATUS.md)) is "the text
swap". This is what that means for the dialogue box, agreed with the owner on
2026-09-20, and the facts it stands on. Menus, system text and text baked into
artwork are **out of scope here**; they go through other paths.

## 1. The idea, as built

Per-language overlay files, built **locally from the player's own discs** and
never committed or shipped (CLAUDE.md rule 1, [`LICENSING.md`](LICENSING.md)
§3). An overlay `DAT\<lang>.<NAME>.DAT` is an ordinary `DAT` container holding
only the chunks that differ; with `BOF3X_LANG=<lang>` set, `LoadDatFile` walks
it after the shipped file, so its chunks land on top
([`DIVERGENCE.md`](DIVERGENCE.md) DIV-0005).

- **Text.** `en.AREA000.DAT`: one kind-0 chunk, tag 0 - the donor disc's
  message block re-encoded for the PC engine (§4).
- **Font.** `en.FIRST.DAT`: a kind-3 chunk - the shipped glyph table, whole,
  with the donor's glyphs added (§3) - and a **kind-4** chunk, which is ours: a
  pen advance for every glyph (DIV-0006, §5).
- Known and accepted: switching language between loads scrambles anything that
  stored glyph codes - character names in a save, for one.

```
python tools/loc_build.py all --disc "CDImage/Breath of Fire III (USA).cue" --game bof3
BOF3X_LANG=en build/bof3x-launcher.exe --game bof3
```

One pass, 244 overlay files: 200 area texts, 44 system pools, and `en.FIRST.DAT`
with the font, the advances, its pool and the six name tables.

`tools/psx_disc.py` reads the ISO9660 tree of a `.cue`, a raw `.bin` or a
cooked `.iso`, so the PSX and the PSP discs are the same kind of donor.

## 2. The PC engine, measured

**One font, global** ([`asset-loading-path.md`](asset-loading-path.md) §2): a
single kind-3 chunk in `FIRST.DAT`, 2,451 glyphs of 24 x 24 at 4 bits a pixel,
low nibble first, 288 bytes each. `Font_SetGlyphData` `0x5A6800` (ours) frees
the previous table - a branch that ran for the first time on 2026-09-20, under
an overlay. `tools/font_pc.py sheet` draws the table.

**The code is the glyph index** - hypothesis closed, 2026-09-20, by reading
the string draw `0x516B70` behind `Text_DrawAt` `0x516B30` (`symbols.toml` has
the full read):

| first byte | what it is |
|---|---|
| `0x00` | end |
| `0x01`, `0x05 nn`, `0x06`, `0x07 nn` | newline (y += 12), set colour, restore colour, draw the 32-byte record `0x904CE0 + nn * 32` |
| `0x20` | advance, draw nothing |
| other `<= 0x20` | falls into the glyph path with a negative index - see the trap |
| `0x21`..`0x7F` | glyph `b - 0x26` |
| `>= 0x80` | two bytes: glyph `((b0 & 0x7F) << 8) + b1` |

A glyph index above `0xA00` executes `in al, dx` - a privileged instruction, so
a crash, presumably a debug trap. **`0x993`..`0xA00` is therefore free**: 110
glyphs past the shipped 2,451, reachable as `0x89 0x93`..`0x8A 0x00`.
*Since DIV-0016 that bound is not a constant: it follows the loaded table
(`max(0xA00, glyphs - 1)`), which is the original's number for every shipped
file and lets an overlay's table be any size. The PC has no limit on the
table itself - `Font_SetGlyphData` takes a pointer and a size - so the only
ceiling was this comparison, in a function that is ours.* The
single-byte range is near-ASCII - `(` `)` digits, full-width capitals at
`0x41`, `「` at `0x2A`, no lowercase (the slots from `0x5B` up hold symbols and
circled numbers). The glyph index travels to the renderer in the *tpage* field
of a `POLY_FT4` (`+0x16` of the primitive at `[0x7E0670]`), the CLUT id
`0x7800 | colour` in the usual field; the quad is 12 x 12 PSX pixels.

`Text_DrawAt` has **341 callers** - it is the port's general text draw.
`MsgBox_Step` `0x497840` calls it at `0x497A22` for one character at a time and
keeps its own pen: `MsgBox_PenX` `0x7DEE5C` `+= 12` at `0x497A44`. Both
advances are flat 12; they do not share the constant.

**The nibbles are CLUT indices into a grey ramp.** `FIRST.DAT` kind-0 tag
`0x8000` is the CLUT strip as loaded; CLUT 0, read 2026-09-20: 1 = 224 grey,
then 216, 200, 192, 168, 136, 7 = 96; 8 = `(0, 0, 48)`. The PC font uses 1 for
the body, 2-7 to anti-alias and 8 as a full outline; the PSX font uses 1 for
the body, 2-6 to anti-alias and 7 as a drop shadow to the right and below, and
never 8. **A straight nibble copy is right**, and was right on the first try
in game. Other colours are the same ramp shape in other hues.

## 3. The donor font, measured (US disc, `SLUS_004.22`, 2026-09-20)

`BIN/ETC/ENDKANJI.EMI` section 0 is the 256 x 512 atlas the sibling's
`font_sheet.py` describes. Beside the JP-style 12 px capitals it carries, from
y = 72, **the Western dialogue font: cells of 8 x 12, 31 to a row, in script
code order from `0x30`** - cell = code - `0x30`, 100 cells to `0x93`:

| codes | glyphs |
|---|---|
| `0x30`-`0x39` | `0`-`9` |
| `0x3A`-`0x40` | `(` `)` `,` `-` `.` `/` `=` |
| `0x41`-`0x5A` | `A`-`Z` |
| `0x5B`-`0x60` | `‥` `?` `!` heart, note, sigma |
| `0x61`-`0x7A` | `a`-`z` |
| `0x7B`-`0x8C` | arrows, shapes, symbols |
| `0x8D`-`0x93` | `&` `'` `:` `"` `;` `·` `%` |
| `0xFF` | space (no cell) |

This agrees with the code list the sibling read off the script
(`LOCALIZATION_APPLY.md`), which it could not tie to a sheet position.

**How English does upper and lower case, and uneven letters: it does not do
anything.** Upper and lower case are different codes with different cells. The
font is **monospaced**: every cell is 8 px, `i` and `m` alike, and the US
stepper adds a flat 8 to its pen after every character - `addiu v0, v0, 8` at
`0x80150770`, the twin of the JP engine's `addiu 0xC` and of the PC's
`add bp, 0xC`. Corroborated from the data: over AREA000 the longest line is 24
characters and the mode 19-21, which is a 192 px box at 8 px. There is no width
table in the US build to port.

**There are two Latin sets on the donor, and we now take both** (owner,
2026-09-20). Beside the 8 x 12 dialogue set at y=72 the atlas carries the
same 100 characters again at **8 x 8, rows 120..151**, same 31 to a row and
the same code order. That is the set the 8 px UI draw `0x516E70` is for. Its
quad is 8 units where `Text_DrawString`'s is 12, but it samples the **whole**
24 x 24 glyph into that quad, so the cells are stored tripled and land at
16 x 16 on screen. Appended at glyph `0xA00` (DIV-0016). The `--glyphs` /
`--upscaler` path still covers **only the 12 px set**; the UI set is plain
tripling.

`tools/loc_build.py font` keeps the whole shipped table (so untouched Chinese
text still draws), appends the 100 cells at glyph `0x993 + (code - 0x30)`,
doubled to 16 x 24 at the left of the 24 x 24 glyph, and paints the same glyph
over the single-byte slot of each of the 74 characters that has an ASCII byte
the engine treats as plain (`>= 0x26`, not `0x2A` or `0x3C`, which hang at
line start) - so ordinary text stays one byte a character.

**Better upscales.** `loc_build.py export` writes the 100 cells as a 160 x 60
RGBA sheet (8 x 12 cells, 20 to a row, the game's own greys, clear
background); `font --glyphs sheet.png` takes a sheet of the same layout back
at any cell size up to 24 x 24 - 320 x 120 for the 2x the 8 px advance
assumes - and quantises it: alpha under half is clear, otherwise the nearest
grey of the ramp, and anything much darker than the ramp becomes 8, the PC
outline. Checked: the export doubled nearest-neighbour rebuilds the default
table byte for byte.

**Replicating a build exactly, with no image checked in.** `font` and `all`
take `--upscaler "CMD {in} {scale}"`: the cells are exported to a temporary
directory, the player's own upscaler is run there, and its output - `{out}`
if the command uses it, else the one new PNG it left - is read back; an
output without alpha is accepted, exact black being clear. The SHA-256 of the
font overlay is printed, which is what two people compare (it depends on the
disc, the shipped `FIRST.DAT` and the upscaler, nothing else). Written with
[cole8888/Nearest-Neighbour-Upscale](https://github.com/cole8888/Nearest-Neighbour-Upscale)
in mind (MIT, C, `make`; its driver writes a fixed file name into the working
directory, and keeps alpha only when built with `CHANNELS_PER_PIXEL` 4 - both
handled). *Not yet run with that tool itself:* checked 2026-09-20 with a
stand-in of the same behaviour (`analysis/font/nn_like.py`), which gives the
same hash as the built-in doubling, `967e0d4c...5c50` on the owner's US disc -
as it must, since integer nearest-neighbour is what both compute. The hook
earns its keep with a *different* upscaler; the hash then names the result.

## 4. The text

**The message slots line up across languages**, and the US block is all
messages. Census of the US disc's 200 area scripts, 2026-09-20: 256 slots in
every file; walking every slot control-aware covers each block to within four
bytes of its end, so there is no "script half" to lose by swapping the chunk.
Controls used: `01 02 03 04 05 06 07 0A 0B 0C 0D 0E 0F 10 11 14 16`, the set
the sibling counted. Real English uses only the codes of §3; everything else
in the US blocks is the untranslated JP leftovers the sibling describes.

`loc_build.py text` rebuilds each block: every distinct donor message is
re-encoded - one byte where the character has an ASCII slot, else the two-byte
code of its appended glyph; `0xFF` becomes `0x20`; controls, their arguments
and the `0x14` choice block pass through - and a message holding any code
English lacks **keeps the PC file's own message for that slot**, which still
draws because the Chinese table is intact. 200 of 200 areas built (since
DIV-0007), 496 slots kept as shipped.

`AREA000`, text block, for scale: PSX-JP 5,041 bytes; PSP-EU 9,170; PC 5,732;
the English overlay 9,533.

**The PSP discs are PSX data, renamed very little** (owner's discs,
`fixtures.toml` `psp-jp` / `psp-eu`): 885 `.EMI` files in the PSX container,
the font still 32 KB atlas sections at the PSX's VRAM tags. A PSP EU disc
should be as good a donor as a PSX US one; not yet run through `loc_build.py`.
The owner's PSX discs for USA, Japan, Germany and France are in `CDImage/` too.

## 5. The advance (DIV-0006)

A kind-4 chunk: one byte a glyph, the advance in PSX pixels, the chunk's tag
the advance of the space. The engine re-aims `MsgBox_Step`'s one call of
`Text_DrawAt` and corrects `MsgBox_PenX` by `advance - 12` after it; nothing
of the stepper is replaced. 8 for the donor's glyphs, 12 for the rest. Because
it is a table, a proportional font is a data change.

### What the owner saw, 2026-09-20, and what it turned out to be

- *The camp menu and the yes / no box are still at 12 px.* A third pen:
  `Text_DrawImmediate` `0x5961C0`, the PSX immediate draw's twin, one
  `Text_DrawAt` call a character with the pen in `ebp`. Ours now. **Three
  pens in all** - `Text_DrawString`'s, `MsgBox_Step`'s, this one - and nine
  functions call `Text_DrawAt` with a count of 1; the other seven
  (`0x45B490`, `0x45B5F0`, `0x460730`, `0x460920`, `0x466260`, `0x4B1090`,
  `0x4B11F0`) place single characters without a `+ 12` beside the call and
  are unread - where the next "still 12 px" report will come from.
- *The apostrophe has padding it should not.* It is the US release's: every
  cell advances 8 and the apostrophe's ink is columns 1..3 of its cell. The
  US stepper's only special case is the double quote, hung 8 px into the
  margin at the start of a line (`0x80150680`). Both are now done: the hang
  faithfully, the apostrophe and comma tightened to 5 by the tool unless
  `--mono` - a divergence from the US release, DIV-0009, which the owner
  confirmed against an emulated PSX build the same day (`you' ll`, and a
  twelve-pixel gap after a comma) and prefers ours to.

## 6. Open, in the order it bites

1. ~~**Sixteen areas do not fit.**~~ **Closed 2026-09-20, DIV-0007.** The
   English blocks of `AREA039`, `090`, `094`, `153`, `154` and `175`-`185` run
   `0x467D`-`0x5559`, past the system pool at arena `0x4000`. The sibling's
   `regional-builds.md` had the answer: the US release moved the *pool*
   (`0x80014000` to `0x8001A000`) and left the script where it is. Here the
   pool has one reader, `Msg_SystemPtr`, against 41 functions that read the
   script in place - so the pool moved, to a buffer of its own, and all 200
   areas build.
1a. **The boxes that are not the dialogue box** (owner's recollection,
   2026-09-20: zenny pick-up, item pick-up, the masters' talk - not yet
   matched to addresses). 35 functions read the script block at `0x803580`
   themselves and draw through `Text_DrawAt` without `MsgBox_Step`
   (`analysis/pc_funcs.json` operand census; `0x458D70`, `0x45E820`,
   `0x52D560`, `0x468560`, `0x5869A0` are the table-lookup shape, and
   `0x498280` is by position the PSX `MsgBox_Replay`). Because the script did
   not move they find the English text. They advance inside the string draw
   `0x516B70`, **which is ours since 2026-09-20** (`Text_DrawString`,
   `src/game/text_draw.cpp`) and takes the advance table too - seen on the
   attract narration, which is one of these paths. Still open here: several
   of the 35 read fixed offsets (`+0x6E`..`+0xE0`, `+0x200`, `+0x3E0`), which
   no area script has - other files put other things at arena 0, unread; and
   a caller that centres text by counting characters at 12 px would now sit
   left of centre. The narration does not: the US script centres with spaces.
2. **Owner, in game.** Only the attract sequence has been seen: a caption and
   two speakers. Choice menus, name inserts (`0x03` `0x04` `0x07`), colour,
   the instant-print spans and page breaks are converted by rule and unseen.
3. **The stepper's second draw, `0x4987E0`** (flag 8 of `0x7DEE44`), still
   advances 12. Unread; by position it is the PSX grow/shrink text effect.
4. **Menus: built 2026-09-20, seen 2026-09-21** (DIV-0008, §7; captured by
   input recipe on every menu screen). Left in Chinese: enemy and place
   names, the stat labels, the list headers, the battle's target banner, and
   any string outside what §8 lists.
5. The port widened name fields (enemy names 8 to 12 bytes, character names 5
   to 9). Dialogue that embeds a name goes through control codes, not these
   fields - *believed*, not checked; what a JP/US-length name record looks
   like under the PC's 9-byte walk is unread.
6. German and French: their discs are on hand; their cells for the accented
   letters are unread, and may need more than the 110 free glyphs leave.
7. ~~The regression check with `BOF3X_LANG` unset.~~ Run 2026-09-20 with all
   111 ours, DIV-0005..0007 in: identical to the all-original reference at
   1,734 of 1,734 frames (a 1.9-minute run, not the whole cycle). The frame
   hash has NOT been re-recorded: `Text_DrawString` and `Msg_SystemPtr` are
   logic functions, so `ab14_orig` is stale if either is in `entries_logic.txt`.
8. **The save screen's Yes / No hand is in the wrong place, and moves the
   wrong way** (owner's screenshots, 2026-09-22, English, at an inn: "OK to
   overwrite?" over the three slots, against the same prompt in the
   sibling's recompiled US build). Measured at 640 wide (the recomp's
   captures scaled from 1,539), the hand's tip and the text:

   | | hand on Yes | hand on No | "Yes" starts | "No" starts |
   |---|---|---|---|---|
   | ours | ~505 | ~437 | ~490 | ~548 |
   | US PSX (recomp) | ~493 | ~555 | ~495 | ~557 |

   On the PlayStation the hand sits just left of each word and steps
   **right** to No; ours overlaps the `Y` on Yes and steps **left**, to well
   before "Yes", on No. The text itself sits where the recomp has it. So
   the hand's x is reckoned by something that did not follow the text into
   English - the same family as the Config screen's controller panel
   ([`config-screen.md`](config-screen.md) §8: a Chinese width, `len * 6`, on
   two-byte strings that advance 8), but a leftward step is not explained by
   a width that is merely too wide, so the placement is unread and may count
   from the right edge. Where the prompt and its options come from (system
   pool, DIV-0007, or a table in the exe) and who positions the hand are
   the first two reads. No recipe reaches a save point yet; save 5's
   position is not at one.

## 7. Menu text: the system pool and the name tables (DIV-0008)

The owner's recollection, 2026-09-20 - that the Japanese and English item and
armour names differ in length, and would the Chinese port have kept that -
was right, and the answer is better than either. Sibling `TEXT_TABLES.md`: the
JP disc's item and ability records carry `name[8]`; the US build widened
every one to `name[12]`, numbers untouched. **The Chinese port has
`name[16]`**, in `BOF3.exe`'s `.data`, numbers equal to JP's in 534 of 534
records (`symbols.toml`, `NameTable_*`). A Chinese name is two bytes a glyph,
so 16 bytes is the JP field's eight characters; an English one is a byte a
letter, so every US name fits with four bytes over.

| table | JP | US | PC | records |
|---|---|---|---|---|
| consumables | 14 | 18 | 22 | 92 |
| key items | 12 | 16 | 20 | 16 |
| weapons | 20 | 24 | 28 | 83 |
| armour | 18 | 22 | 26 | 68 |
| accessories | 16 | 20 | 24 | 52 |
| abilities | 16 | 20 | 24 | 227 |

(strides in bytes; name = stride minus the JP record's numeric bytes.)

The US name bytes are the dialogue font's codes - `0xFF` the space, `0x8E`
the apostrophe, `0x3D` and `0x3E` the hyphen and the stop the sibling saw as
`=` and `>` - so the dialogue encoder converts them unchanged. The donor's
tables are found by the PC table's own numeric bytes, not by address.

The system pool - descriptions, menu strings, the pick-up messages - has the
same two-block shape and the same slot counts, 309 and 455, on the PC and on
the US disc, and converts slot for slot with the area-script converter.

**Room on screen is the open question, not room in memory.** Eight Chinese
glyphs are 96 px; at 8 px a letter that is twelve letters - the US field
exactly. Whether a column has slack for the 13 to 15 letters the field would
hold (`Ballock Knife`, `Leather Armor`) is for the owner to see in game.

`bof3ext` put English into this port by hooking the draw. Its `docs/` are worth
reading for which problems it met - cited, not copied (CLAUDE.md rule 5).

## 8. Strings in the executable: slot tables, one chunk kind each (2026-09-21)

Much of the menu and battle text is not in any `DAT` but in `BOF3.exe`'s
`.data`, in fixed slots. Each piece so far had the same shape on both sides:
the US disc carries the same table beside data the PC still has byte for byte,
so `loc_build.py` finds the donor's strings by the PC's own bytes and writes
them into an overlay chunk of a kind of ours, and the DLL writes the slots at
start-up after checking the pointer table or the instructions that read them.
All of them load with `FIRST.DAT`.

| Kind | What | PC slots | Found on the US disc by | Entry |
|--:|---|---|---|---|
| 4 | per-glyph pen advance | - | the font | DIV-0006 |
| 5 | item and ability names | six record tables | the records' numeric bytes | DIV-0008 |
| 6 | title menu row widths | code operands | - | DIV-0014 |
| 7 | Config labels, options, controller names | `0x6536F8`, operands, `0x66A368` | the two row tables (`START.EMI`) | DIV-0015 |
| 8 | the menu's button verbs (Quit, Init, Use, Sort, ...) | 22 x 8 at `0x66A228` via `0x6637E4` | the button-set records `0x66383C` (`START.EMI`) | DIV-0018 |
| 9 | the battle's command labels (Atk ... Esc) | 7 x 8 at `0x669D28` via `0x669D60` | the box table `0x64E2C8` (`BATTLE.EMI`) | DIV-0019 |
| 10 | New Game's default names (Ryu ... Whelp) | name fields of 8 records at `0x64B390` | the records past the name, 4 bytes earlier (`START.EMI`) | DIV-0020 |
| 11 | Manillo, the fish merchant | 8 bytes at `0x669CD8` | twelve bytes at `0x6608CC` (the fishing areas) | DIV-0020 |

What makes this cheap: the US abbreviations were made to fit the PlayStation's
boxes, and the PC's boxes were made for two 12-unit Chinese glyphs - which are
three 8-unit English letters wide. Only the verbs needed a layout change
(re-centring on the real width, DIV-0018).

**Finding the next one:** `BOF3X_TEXTLOG=1` logs every string drawn and its
address; a search of `.data` for a pointer to that address finds its table,
and the port's own font renders the Chinese glyphs so the table can be read
(`tools/font_pc.py`'s `glyph_pixels`). Found and not yet converted: the stat
labels at `0x669CF0` (Attack / Defense / Int / Agility - the US `Pwr` `Def`
`Int` `Agl` stand before the verb table in `STATUS.EMI`), the turn counter's
残留 / 回合 at `0x669D10` / `0x669D18`, and the skill list's header 龙技 at
`0x66A220`.

**Names in saves** are the save's: a save keeps the names it was begun with,
and shows gibberish across a language switch. The owner accepted that on
2026-09-21 until a language-independent name system exists.

