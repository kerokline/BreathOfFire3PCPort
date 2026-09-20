# Dialogue-box localisation - the plan, and what it rests on

**Status:** DRAFT (2026-09-20) - a plan; nothing built. Measurements below are dated;
everything else is marked as intent or as a guess.

Stage 2 of the owner's order of work ([`STATUS.md`](STATUS.md)) is "the text
swap". This is what that means for the dialogue box, agreed with the owner on
2026-09-20, and the facts it stands on. Menus, system text and text baked into
artwork are **out of scope here**; they go through other paths.

## 1. The idea

Per-language overlay files, built **locally from the player's own discs** and
never committed or shipped (CLAUDE.md rule 1, [`LICENSING.md`](LICENSING.md)
§3):

- **Text.** For each `DAT` file that carries dialogue, a sibling such as
  `en.AREA000.DAT` whose text chunk is the donor disc's message block
  re-encoded for the PC engine. `LoadDatFile` is ours already; "try the
  language-prefixed file, else the original" is a small divergence for the
  ledger.
- **Font.** The donor's glyph atlases rendered, upscaled 12 px to 24 px, and
  re-encoded in the format of the port's global glyph table; swapped in with
  the text.
- Known and accepted: switching language between loads scrambles anything that
  stored glyph codes - character names in a save, for one.

## 2. What is measured

**The port has one font, and it is global** ([`asset-loading-path.md`](asset-loading-path.md)
§2): a single kind-3 chunk in `FIRST.DAT`, 2,451 glyphs of 24 x 24 at 4 bits a
pixel, low nibble first, 288 bytes each, each nibble through a 16-entry table
of 16-bit colours. `Font_SetGlyphData` `0x5A6800` (ours) takes a buffer and a
size and frees the previous one - a branch no shipped data has ever run. One
reader, `0x5A2CA0`, `base + glyph * 0x120`. The PSX builds keep glyphs as
32 KB atlas textures inside the EMIs instead.

**The script is glyph codes, not readable text, and the engine is the PSX
one.** `MsgBox_Step` `0x497840`, read 2026-09-20: a byte `<= 0x16` goes
through the 23-entry jump table `0x497A70` - the PSX control codes; anything
else is a character, handed by pointer to the draw `0x516B30`, after which
`test byte [esi], 0x80` (`0x497A2A`) skips a second byte when the first has its
high bit set. So: one- or two-byte characters, two-byte ones flagged by the
high bit. A GBK scan finds nothing; the census has the area text at 2,264
distinct codes, all `<= 0x0FFF`, against a 2,451-glyph font
([`DAT_CONTAINER.md`](DAT_CONTAINER.md)). That the code **is** the font index is
still a hypothesis: `0x516B30` is unread.

In that branch, bytes `0x2A` and `0x3C` pull the pen 12 px left when they start
a line. Those are `*` and `<` in ASCII. *Guess:* single-byte codes may be
near-ASCII in this font. Two constants, nothing more.

**The message slots line up across languages.** `AREA000`, text block at arena
offset 0 (PSX `0x80010000`), 2026-09-20:

| Build | Block bytes | Offset-table entries | Bytes `>= 0x80` after the table |
|---|---|---|---|
| PSX-JP = PSP-JP (byte-identical section) | 5,041 | 256 | 32% |
| PSP-EU, English | 9,170 | 256 | 16% |
| PC, Chinese | 5,732 | 256 | 63% |

One area, not a census. The sibling's `LOCALIZATION_APPLY.md` says the US
script is aligned slot for slot with JP; not re-measured here.

**The PSP discs are PSX data, renamed very little** (owner's discs,
`fixtures.toml` `psp-jp` / `psp-eu`, 2026-09-20). 885 `.EMI` files in the PSX
container - `MATH_TBL` header, same section types and tags - not the PC's
`.DAT`. `AREA000`: 14 sections in PSX-JP, PSP-JP and PSP-EU alike; 9 of 14
byte-identical PSX-JP to PSP-JP, the differences being the three sound
sections and one image page; load addresses lose bit 31. The font is still
32 KB atlas sections at the PSX's VRAM tags: **no 24 px table - that is the
Chinese port's own.** PSP `FIRST.EMI` has two sections the PSX lacks
(`0x596000`, `0x600000`; unread) and a system message pool of a different size
(`0x3660` against `0x3628`), and ships `libfont.prx`, use unknown.

Consequence: **a PSP EU disc is as good an English dialogue donor as a PSX US
one** - same format, same atlas font - and the converter should read EMIs
without caring which. For the *system* pool prefer a PSX disc until the PSP
pool is diffed against it.

## 3. What is not known, in the order it bites

1. **What `0x516B30` does**: code to glyph index, the single-byte range, and
   above all the **advance**. JP draws a flat 12 px advance; if the PC advances
   a flat 24 px, upscaled English renders correctly and reads like a
   typewriter, and line lengths authored for the US engine will not fit. A
   narrower or proportional advance is an engine divergence with its own
   ledger entry. The English build's cell width and advance are not known here
   - ask the owner, or read the sibling's `TEXT_ENGINE.md`.
2. **What a nibble means on each side.** PC: an index into a 16-entry colour
   table (which table, filled by whom - unread). PSX: a CLUT index, which may
   be body / outline / shadow rather than intensity. Straight copy or remap is
   decided by reading both.
3. Whether the donor block's **script half** - the bytes the offset table does
   not point at, if any - differs between regions. The plan swaps a whole
   chunk; that is only safe if the block is all messages or the rest is equal.
4. The port widened name fields (enemy names 8 to 12 bytes, character names 5
   to 9: [`DAT_CONTAINER.md`](DAT_CONTAINER.md), [`save-interchange.md`](save-interchange.md)).
   Dialogue that embeds a name goes through control codes, not these fields -
   *believed*, not checked.
5. Which of the 23 control codes the dialogue actually uses; the attract
   sequence's eight messages are the regression check that exists
   ([`attract-mode.md`](attract-mode.md) §6).

`bof3ext` put English into this port by hooking the draw. Its `docs/` are worth
reading for which problems it met - cited, not copied (CLAUDE.md rule 5).

## 4. First steps

1. **Read `0x516B30`** and its path to `0x5A2CA0`. Settles §3.1 and the
   code-is-index hypothesis in one sitting. Static.
2. **Look at both fonts.** A scratch tool that dumps the PC table and one PSX
   atlas page to PNG (output to `analysis/`, gitignored). Settles §3.2 by eye.
3. **The gibberish test.** Build a table from the PSP-EU atlas - nearest-
   neighbour 2x, glyph N = atlas cell N, the PSX page lead bytes (`0x13`
   `+0x100`, `0x15` `+0x5B`: sibling `TEXT_ENGINE.md`) folded into the index -
   and load it in place of the Chinese one with the text untouched. The
   dialogue will be nonsense in our glyphs at 24 px: format, nibble mapping and
   advance, all visible at once. Owner in game, or the attract sequence.
4. Census: the offset-table entry count of every area, JP / EU / PC
   (`tools/dat.py` has both parsers). Turns §2's one row into 199.
5. Then the converter (`tools/`, EMI text block to PC chunk), the overlay
   lookup in `LoadDatFile`, and their `DIVERGENCE.md` entries.
