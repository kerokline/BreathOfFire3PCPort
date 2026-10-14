# The ending's backdrop: the desert scene, its two pictures, the seam and the wide picture

**Status:** BUILT (2026-10-14; DIV-0127, D302). Live-checked in captures at
the three widths; the owner's eye on the mirrored bands owed. A wider
painting of the two pictures is the art path, not started.

The owner wants the ending's desert scene - the party on a dune under a
cloud sky - as the launcher page's background, in the wide picture. Nobody
had a save near it. This is how the scene was reached, what draws it, the
seam it had and what the wide picture does with it.

## 1. Reaching the scene: a chapter-jump save

The scene is scenario chapter 17's intro ([`scena_sc15.md`](scena_sc15.md)
3.3): `Scena17_Start` sets area 0xC7 (199, `AREA199.EMI`, world 4's
"undeveloped land") itself, then sixteen steps move the camera's focus and
elevation under a letterbox, then the staff roll. So a save can start there
without playing to it: a copy of a recipe save with the game block's first
sixteen bytes - loaded straight into `0x8034E0..` by `LoadMenu_Apply`,
byte 0 the chapter, byte 2 the state - set to `17, 0, 0, ...`, the area
word `+0x24` 0xC7, and the checksum recomputed: the u16 at **block +0x70**
is the byte sum of bytes 0..0x10AF with that word zeroed (`LoadMenu_Read`;
the load menu says "file is damaged" otherwise - the +0x270 the sibling's
save docs name is not the PC's). `tools/recipe_saves/ending17.DAT` is that
save (gitignored, from `dolphinBoss.DAT`); `tools/recipes/ending17.txt` is
the load route (`bossAndFlash.txt`'s opening) and a shot a second for a
minute. The four-character dune shot is recipe frames about 2,540..2,620;
the intro's camera pull-back 2,300..2,700; the roll follows.

The chapter's party is placed by the chapter, not the save: Rei, Ryu, Nina
and Momo stand on the dune whatever the donor save's party.

## 2. What draws it

Measured on the captures (every row of the sky and the far dunes
pixel-identical across the camera's pan, 2,549..2,709; the terrain moves)
and then read: the dune is the area's terrain (`AreaMap_Frame`, 3D) and the
four are field sprites; the sky and the far dunes are two paletted pictures
drawn in screen space by **effect kind 0x18, sub-kind 0x61**
(`EffectKind18Sub61_DrawOverlay` `0x5151A0`, [`effect_6d.md`](effect_6d.md)
1.4), the ending's backdrop effect:

- a tile over 0..320 x 0..256 from page 0x99 under a 16 x 16 texture window
  (the sky strip's top-left 16 x 16, a plain blue);
- eight pieces of the table `0x65F470` (14 bytes each: s16 x, y, w, h; a
  page byte; u, v, du, dv): **pieces 0 and 1 the cloud sky** - 256 + 64
  columns, 48 rows, page 0x99 (VRAM x 576..703, rows 256..), CLUT 0x79C0
  (row 487), the strip's rows 16..63 and, for the right 64, a 64 x 32 piece
  at its rows 64..; **pieces 2..7 the far dunes** - page 0x97 (VRAM x
  448..575), CLUT 0x7980 (row 486), a 256 x 256 picture: its first 64 rows
  and its rows 64..191 across columns 0..256, and the screen's last 64
  columns filled with the picture's first 64 columns and three 64 x 64
  blocks of its rows 192..255 - the repeat the owner noticed in the
  rendered strip.

Both pictures are `AREA199.DAT`'s two image chunks, 8-bit, 256 KB each,
loaded into the VRAM shadow (`Gfx_LoadImage`, [`asset-loading-path.md`](asset-loading-path.md)).
The pictures only ever show their lower rows: the sky's strip starts 16 rows
above the screen and the letterbox covers 30 more.

How it was found, for next time: the all-calls tracer cannot see our own
functions in the exe host, so the draw was found from the data - the VRAM
shadow (`0x6C9F44`, 1 MiB) read from the running scene by
`ReadProcessMemory`, the sky and the dunes located as 8-bit blocks, their
palettes found by rendering each 256-colour row of VRAM and scoring against
the capture (the sky: the best match of 2,700; the dunes: the row whose
luminance follows the index), and the CLUT word of the sky's row, 0x79C0,
searched in `src/game` - one hit, this function.

## 3. The seam (D302)

A one-pixel light line across the picture where the sky meets the dunes
(game row 48) and a dark column down the sky's last column (319), both
modes. Pieces 0 and 1 are 48 rows and 64 columns with `dv` 48 and `du` 64,
where the table's other pieces and every fill of the port use w - 1 and
h - 1. The port's texel-centre table `D3d_TexCoords` (`(i + 0.512) / 256`)
then samples texel row 64 and column 64 on the last pixel row and column -
index 0 of the page. DIV-0127 draws the two pieces with w - 1 and h - 1 once
the fills are armed; the fuzz keeps Capcom's packets.

## 4. The wide picture (DIV-0127)

Before: the terrain fills the bands by itself (geometry, DIV-0041's good
case), the sky and the dunes stop at the 320 columns (black band corners,
the dunes cut), and the letterbox's two 320-wide tiles leave terrain showing
above and below them at the sides. After: the letterbox tiles are full-frame
fills; the tile runs to the bands; and every piece on an edge gets band
pieces beside it - its rows mirrored about the edge column, mirrored again
past the piece's width (the 64-wide right pieces under 120 columns) - so the
texel at the edge continues outward. `tools/wide_seams.py`: 0 of 14 frames
at or above 0.2 at 53 and at 120 columns (10..13 before).

What a wider painting would replace: the mirrored 53 (or 120) columns each
side of a 256-wide picture. The substituted-art path ([`opt-layers.md`](opt-layers.md))
is where it would go; the draw would then read the wider texels instead of
mirroring.

## 5. Checks

- Narrow before / after at recipe frames 2,469..2,729: only the sky's rows
  (30..47) differ, the lines gone; the rest of the picture identical
  (`numpy` equality on the captures).
- Wide and 21:9: the seam finder as above; the captures looked at.
- Headless self-tests (`BOF3X_SELFTEST_ONLY=1`, a launcher copy with no ini): `BOF3X_SHADOW='*'` exit 0 (every
  fuzz 0 mismatches, `effect_6d` and `scena_sc15` among them - the fuzz sees Capcom's packets, the changes gated on
  `Widescreen_FillsArmed`), and `BOF3X_LANG=en-US` exit 0. (`'*'` under `en-US` is the HANDOFF trap: the ConfigText
  patch re-aims `Config_DrawRowLabel + 0x9F` and `field_c1`'s clone check stops, exit 3 - by design, not this change.)

## 6. Open

- The owner's eye on the mirrored bands (symmetry in the clouds and the
  dunes at the edges).
- The sky's right 64 columns (piece 1) are Capcom's own 64 x 32 piece, not
  the strip's continuation; the dunes' last 64 columns are the picture's
  first 64. A wider painting would redo both.
