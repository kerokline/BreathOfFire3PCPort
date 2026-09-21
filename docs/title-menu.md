# The title menu

**Status:** IN PROGRESS (2026-09-20) - built, previewed offline; not yet seen in game.

The three rows of the start screen - 新游戏 / 载入游戏 / 选项, "new game", "load
game", "options" - and how `BOF3X_LANG=en` turns them into NEW GAME / LOAD
GAME / CONFIG (DIV-0014). Asked for by the owner, 2026-09-20, with a PC and a
PlayStation screenshot side by side.

## 1. It is artwork, not text

Nothing here goes through the font. Measured 2026-09-20 with `tools/dat.py
compare bof3/DAT/START.DAT <JP START.EMI>`:

- **Image chunk `0x1C000200` of `START.DAT`** differs from the disc's section of
  the same tag in 4,981 of 32,768 bytes. The tag is a tile rectangle
  ([`asset-loading-path.md`](asset-loading-path.md) kind 1): x tile `0x1C`, y
  tile 0, two tiles wide - **a 256 x 256 4bpp page at VRAM (896, 0)**.
- The payload is *tile*-ordered - 0x800-byte tiles of 32 x 32 words, row-major,
  two to a row - so a row-ordered render of it looks like a 128-wide image
  with every other band empty. De-tiled (`loc_build.tiles_to_rows`), the
  port's page holds three rows of 32 px Chinese characters at (0, 0), (0, 32)
  and (0, 64), 96, 128 and 64 wide, and nothing else.
- **The discs' page** (US and JP sections are byte-identical, SHA-256
  `e06a47cfcb16a858...`) holds NEW GAME at (0, 0) and LOAD GAME at (0, 16),
  16 rows each, 130 and 140 wide with their shadows; and a second group from
  (41, 48) down - both strings twice over - whose use on the PlayStation is
  unread. The PlayStation's menu has two rows: the third is the port's own.
- **The palettes are shared.** Both pages are drawn through the 16 CLUTs of
  kind-0 tag `0x8600` (0x200 bytes), which compares IDENTICAL to the disc's
  section for `0x8002BE00`. CLUT 0 there is the white-to-blue look: indices
  1-5 a grey ramp from white, 6-11 a blue ramp, 12 and 13 pale cyans, 15 the
  drop shadow. The lettering is shaded *by row*: whites at the top, blues at
  the bottom, the same five steps away from the body colour in each. The
  Chinese rows use the same scheme, so a disc's nibbles can be used on the PC
  as they are.
- `START.DAT` is not opened during the attract sequence
  ([`attract-mode.md`](attract-mode.md) section 7 lists every open): it loads
  when the player leaves the logo for the menu. So nothing unattended reaches
  this screen.

## 2. The draw

Read 2026-09-20 (`tools/pe_disasm.py 0x588880`, `0x5888D0`); neither is in
`symbols.toml` yet.

- `0x588880 (glow)`: if byte `0x6BDF8E` is set, three rows - indices 0, 1, 2 at
  y = `0x50`, `0x70`, `0x90`; otherwise two - indices 0 and 2 at `0x60`,
  `0x80`. (By its effect the byte is "there is something to load"; unread.)
  Called from four places in the title flow `0x587DB0` (`0x587F23`,
  `0x587F47`, `0x587FA8`, `0x5880CA`).
- `0x5888D0 (y, row, glow)`: a draw mode for page (896, 0) and one `SPRT`
  (`Gpu_SetSprt`), x = 160 - w / 2, y as given, (u, v) = (0, `row << 5`),
  h = 32, colour from byte `0x6BDF90 + row` on all three channels, CLUT
  `getClut(0x20, 0x1E3)`. When `glow` is set a second, semi-transparent sprite
  goes first, with CLUT `getClut(0, 0x1EB)` and the colour doubled.
- **The width is a table built on the stack by three immediates**, `0x60`,
  `0x80`, `0x40` at `0x5888E4`, `0x5888E9`, `0x5888EE`
  (`C6 44 24 1x ww`). That is the one thing an English sheet cannot live with.

## 3. What DIV-0014 does

- `tools/loc_build.py all` writes `en.START.DAT`: the page rebuilt from the
  player's disc, and a **chunk of kind 6, ours**: three bytes, the row widths.
  Rows keep the port's 32 px pitch; the 16-row lettering sits in the middle
  of each band, so its centre is where the Chinese row's was.
- NEW GAME and LOAD GAME are the disc's pixels, untouched.
- **CONFIG is cut from their letters** - the disc has N E W G A M L O D and no
  more. O, N, G as they are; **C** is the G without its spur, its lower
  terminal the upper one turned over and re-shaded for its rows (grey step n
  becomes blue 6 + n); **F** is the E down to its middle arm on the L's lower
  stem; **I** is the L with its foot cut off after the stem's serif. New
  pixels get the lettering's own drop shadow, one down and one right - a rule
  that predicts 83% of the donor's shadow pixels (226 of 272 in NEW GAME's
  band), the rest being the artist's touching up. The word is the owner's
  choice (2026-09-20): it needs no drawn letter, and it is what the US field
  menu calls that screen.
- The builder refuses a sheet whose hash is not the measured one - the letter
  boxes are coordinates - and leaves the title as shipped. German and French
  discs are unmeasured.
- The engine (`src/game/title_menu.cpp`) writes the three widths into the
  immediates through `bof3::PatchBytes`, whole instruction checked. Not a
  reimplementation: the draw stays Capcom's.

Preview without the game: the scratch script behind the 2026-09-20 session
composed the page through CLUT 0 at the draw's own positions; the owner's
verdict on it was "perfect". **In game it is unseen** -
[`USER_CHECKS.md`](USER_CHECKS.md).

## 4. Open

- The owner's look in game: all three rows, the selected row's glow (the
  semi-transparent pass at doubled colour was drawn for thick Chinese strokes),
  and the two-row layout with no save present.
- What the discs' second group of strings at (41, 48) is for - the
  PlayStation's glow, by guess. Reading the PSX draw would say, and would say
  whether the port's glow pass is Capcom's or the porting house's.
- Names and signatures for `0x588880` / `0x5888D0` in `symbols.toml`.
- The options screen behind the third row, and the load screen, are not looked
  at here.
