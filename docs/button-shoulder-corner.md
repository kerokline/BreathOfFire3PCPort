# The item lists' L1 / R1 corner as σ has it

**Status:** BUILT (2026-10-17, group IC-L of
[`button-unification-round.md`](button-unification-round.md) 3, branch
`buttons/icl`). Headless self-tests pass. Not played; RC's item-list routes
are its live evidence. Ledger draft F (section 6) is lettered; the
coordinator numbers it at the merge.

The site is [`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.5 and
rank 6 of its section 6. The owner's answer 3 (round doc 5.1) is binding:
one code path, the PSP's `L` / `R` blanked and the same L1 / R1 drawn as on
every other install.

## 1. What is built

- `src/game/shoulder_glyphs.{h,cpp}`: the label geometry, a pure plan per
  piece, the glyph codes, the self-test and `Arm`.
- `Menu_DrawPiece` (`0x57D860`, ours in `src/game/menu_windows.cpp`): a
  flag-1 piece with a label, with the switch on, is drawn by its plan. Every
  other piece, and every piece while the switch is off, is drawn by the
  original's body, unchanged.
- `shoulder_glyphs::Arm` runs in `inject_all.cpp` right after
  `sigma::Arm`, so `menu_windows`' fuzz compares the original's
  `Menu_DrawPiece`.

For each labelled piece, the plan:

1. draws the piece without its label box (columns 1..8 of the label rows);
2. refills the box a row at a time from a plain row of the same piece;
3. draws the glyph of `σ(slot)` whole at the slot.

When `σ(slot)` is the slot's own button and the page carries the L1 / R1
art, there is no plan: the piece is drawn as the original draws it. So at
the identity Config nothing a PC or PSX install shows changes, by
construction.

The glyph of a button:

- **L1 or R1, on a page with the art** (the PC's page and every PSX
  disc's): the page's own L1 or R1 label, drawn from VRAM as sprites.
- **A face button, or L1 / R1 on a PSP base**: the glyph the install's
  Config panel draws for that button, through `Text_DrawSmall` (`0x516E70`,
  8 px). Under a Latin layer that is DIV-0051's icon (glyphs `0xA65..0xA6A`,
  `loc_build.py` `ICONS_AT`). Otherwise it is the port's stand-in, the
  one-byte string `0x66A2FC[k]`. The colours are `0x461C00`'s: circle 2,
  cross 1, triangle 6, square 5, L1 and R1 0. After the text the plan
  re-opens the piece page `(0x340, 0x100)`, the page every caller of a
  flag-1 piece opened with `Menu_DrawPieces(..., 1)`.

## 2. The pieces, measured

The UI page is `0x1A080200`: VRAM (832, 256), 4-bit, 256 x 256 texels, read
through the rectangles `0x663C8C` (`Menu_PieceRect` with flag 1).

| Piece | Rectangle | Label rows | Plain row | Slot |
|---|---|---|---|---|
| `0x15` | u 144, v 128, 16 x 24 | 19..23 | 18 | L1, first five rows |
| `0x2E` | u 216, v 144, 16 x 24 | 19..23 | 18 | the same, arrow lit |
| `0x33` | u 144, v 168, 16 x 24 | 19..23 | 18 | the same, no arrow |
| `0x16` | u 160, v 128, 16 x 16 | 0..1 | 2 | L1, last two rows |
| `0x18` | u 176, v 128, 16 x 24 | 8..15 | 7 | R1, eight rows |

- **The L1 label is cut across two pieces.** It has seven rows: five at the
  bottom of the bar's top piece and two at the top of the cap `0x16` under
  it. Evidence: the page's nibbles printed with a scratch script; the PSP's
  `L` sits in the same seven rows. Key-icons 2.5 named four pieces. The
  fifth, `0x16`, carries the label's last two rows, so it is drawn by the
  same plan.
- **`0x16` is always under the top piece.** All ten list draws draw `0x16`
  at the top piece's x and `y + 0x18`, and no other code draws it with flag
  1 (`callargs.py ... 57D860`: `0x16`, `0x18` and `0x33` only in the ten).
  `0x15` and `0x2E` come from the piece lists, at column 18 of `0x663490`,
  `0x6641F0`, `0x66AEA4`, `0x66AF58`, `0x66B1E4`, `0x66B3C4`, `0x66B4BC` and
  of the lit list `0x6634A8`.
- **The draw order varies.** `Menu_DrawItemList` draws the top piece (by its
  piece list) before `0x16`, and for key items `0x33` after both.
  `BattleMenu_DrawItemList` draws `0x16` first. So both pieces of the L1
  slot draw the whole glyph, and whichever comes last leaves it complete.
  The self-test rasters both orders.
- **Every shipped UI page meets the two properties the plan relies on.**
  The three top pieces' L1 boxes are equal, and the five plain rows are a
  single index (4) across the box columns. Checked on the 59 modules'
  pages (`AREA030`, the battle, boss, community, shop and status modules)
  of the PC install, the European PSX disc's cache and the PSP-EU disc's
  cache. The tag's other chunks (`BATL_RET`, `BATL_SE`, the `MAGIC` files)
  are other images and are not drawn as these pieces.

## 3. The label source by install

| Install | The page's labels | The corner at the identity | After a swap |
|---|---|---|---|
| PC, no cache or a PC base (any language) | L1 / R1 | the original's draw | the page's L1 / R1 moved; a face button as the Config panel draws it |
| a PSX disc base (JP or West) | L1 / R1 | the original's draw | as above |
| a PSP base (`base_policy = "psp"`) | `L` / `R` | the boxes refilled, the Config panel's L1 / R1 | the Config panel's glyphs |

- **A PC-only install.** The PC's own page carries the L1 / R1 art, so the
  label source is that art. It is left out and redrawn only when `σ` moves
  the slot. A face button there is the port's stand-in (a circled numeral
  or an `X` in the button's colour), as the owner's answer of 2026-10-17
  keeps it wherever no disc's shapes are present.
- **A PSP base has no L1 / R1 art anywhere.** All 58 of the PSP's UI pages,
  and its `BATL_DRA` page, carry `L` / `R`. The PSP's world-map dial sheet
  (u 80..87, v 96..110 of `0x0A081000` in `AREA016`) also has `L` where the
  PSX's has `L1` and `L2`. So answer 3's "the same L1 / R1" is met with the
  one L1 / R1 that a PSP install has: the Config panel's (DIV-0051's,
  composed from the disc's own atlas). Drawing our own L1 / R1 art was not
  an option, because the owner declined a drawn shape set.
- **The PSP base is read from the cache's manifest** (`ManifestPspBase`, the
  shared reader), not from VRAM. No importer change is needed:
  - every page is refilled at draw time;
  - a repaint at build time could not reach a PC install played from its
    own `DAT\`, which has no cache;
  - `BOF3X_ORIGINAL=ShoulderGlyphs` still shows the PSP's own `L` / `R`.

## 4. Verification

- **Build:** `cmake --build build`, no errors and no warnings in the three
  files.
- **Headless, narrow** (`BOF3X_SELFTEST_ONLY=1
  BOF3X_SHADOW=shoulder_glyphs,menu_windows,sigma`, an ini-less copy under
  `C:\Users\kerok\b_icl\L`, the PC install). The log shows:
  - `shoulder_glyphs self-test: 720 plans rastered (90 drawn as the
    original), 5 rectangles, 12 glyph codes, 4 stand-in Configs; 0
    MISMATCHES`;
  - `menu_windows` 74,000 rounds and `sigma` 2,211 tables, each 0
    mismatches;
  - `patch ON ShoulderGlyphs`.
- **What the self-test checks.** It uses a page of our own: random nibbles
  with the two properties of section 2. For the three top pieces, both page
  kinds, both draw orders, with and without the identity shortcut, it tries
  every pair of distinct buttons for the two slots, and every canvas pixel
  must equal the expected one:
  - outside the boxes: the original's draw;
  - in a moved slot: the moved-in art cell, or a font glyph's 8 x 8 (each
    followed by the page re-opened);
  - the rest of a box: the plain row.
- **The identity** is equal to the original twice over: by the shortcut,
  and through the long path (the `force` runs).
- **Stand-in Configs** through `sigma::Build`:
  - Japan's defaults and Speak <-> Action give `σ(L1) = L1` and
    `σ(R1) = R1`, so the corner is unchanged;
  - View <-> Change gives R1 at the top and L1 at the bottom;
  - Speak <-> Change gives circle at the top.
- **The rectangles** are checked against the live table `0x663C8C`.
- **The codes** are checked against DIV-0051's `ICONS_AT` and `0x66A2FC`.
- **Headless, everything** (`BOF3X_SHADOW='*'`, the same copy):
  `self-test only: done`, `inject: 10087 ours, 0 left original`, and 1,079
  self-test lines report `0 MISMATCHES` (SG's 1,078 and this one); none
  reports any other count.

## 5. Not reached

- **No live run.** RC's item-list routes (`fairyVillage` t004230..t004320,
  `shop`, `menu_screens`) in their identity and swapped variants are the
  live evidence. At the identity on the PC and PSX installs the draw is the
  original's call for call, so those frames should equal the references
  `C:\Users\kerok\ab\runs\n1012_menu_screens\{pc-en150,psx-eu}\items.png`.
  On a PSP-EU install the corner changes from `L` / `R` to the Config
  panel's L1 / R1 even at the identity (answer 3).
- **The face glyph at 8 px** is DIV-0051's 12 x 12 icon sampled into the
  small draw's 8 x 8 quad. It is legible in principle; the owner judges it
  on the swapped captures.
- **The list keys themselves** (`0x500C` / `0xF00C`) are HT's: until HT
  asks `σ` there, a swapped corner names the button the Config says while
  the list still pages on L1 / R1.

## 6. Ledger draft F: the item lists' shoulder labels from σ

- **Date:** 2026-10-17
- **Subsystem:** the menu pieces: `Menu_DrawPiece` `0x57D860` (ours,
  `src/game/menu_windows.cpp`); the plan in `src/game/shoulder_glyphs.cpp`.
- **Tier:** Intent. The owner, 2026-10-17: every icon tied to the mapping
  (the round, answer 1); answer 3 for the PSP page.
- **Original behaviour:** the ten item and skill lists draw `L1` above
  their scroll bar and `R1` below it. The labels are baked into the UI
  page, five pieces of it, so they name L1 / R1 whatever the Config says.
  On the PSP, Capcom redrew them as `L` / `R`.
- **New behaviour:** each labelled piece is drawn with its label box
  refilled from its own plain background, and the glyph of `σ(L1)` /
  `σ(R1)` drawn at the slot.
  - The glyph is the page's own L1 or R1 art where the page has it (the
    PC's and the PSX discs'). Otherwise it is the Config panel's glyph for
    that button: DIV-0051's icon under a Latin layer, else the port's
    stand-in.
  - On a PSP base, whose UI page and dial sheet carry only `L` / `R`, the
    L1 / R1 are the Config panel's.
  - With `σ(slot)` the slot's own button on a page with the art, the piece
    is drawn by the original's code: nothing changes on a PC or PSX
    install at the identity.
- **Rationale:** an icon that follows the Config tells the truth once HT
  routes the lists' L1 / R1 tests through `σ`. The PSP case is the owner's
  answer 3, one code path.
- **Also in the PSX version?** The PlayStation's labels are baked the same
  way on every release. This is a change to Capcom's design, not a port
  repair.
- **Verification:** `BOF3X_SHADOW=shoulder_glyphs` (section 4); RC's
  item-list routes, identity and swapped, on the four install kinds.
- **Reversible?** `BOF3X_ORIGINAL=ShoulderGlyphs`.
