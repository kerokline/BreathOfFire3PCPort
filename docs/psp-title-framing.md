# The PSP-only title's logo framing (D256, DIV-0130)

**Status:** DONE (2026-10-17, `install/recipe-version`; the owner saw the mock-up)

## 1. What was wrong

A PSP-only install (DIV-0103) loads the PSP's own title pages (`DEMO.DAT` #0
at VRAM 576,256, #1 at 704,256, #2 at 832,256; 8-bit, CLUT rows 484..491 from
`DEMO.DAT` #4) and draws them through Capcom's PlayStation sprites
(`Title_Sprites`; the PSP's image differs only in entries 8, 9 and 10) at
Capcom's offsets. The PSP lays its pieces out otherwise, so the logo tore, its
right wing was cut flat, the caption under the emblem showed and the mark
sprite drew a second TM (captures at 4:3, 16:9, 21:9: the same at every width).

## 2. Where the PSP's pieces lie

Measured from the pages' alpha (decoded as `dat_load.cpp` `LoadImageChunk`
uploads them: 32 x 32-halfword tiles, `(tag >> 8) & 0xFF` columns), by row runs
and bounding boxes (u, v, w x h):

| Page | Piece | PSP | PlayStation sprite |
|---|---|---|---|
| 576,256 | the upper text, the left wing | rows 4..110, x 8..223: cut (0, 0, 224 x 116) | 4: (0, 0, 224 x 128) |
| 576,256 | the lower text, the right wing, the TM | rows 117..218, x 0..105: (0, 117, 106 x 102) | 5: (0, 128, 96 x 128) |
| 704,256 | the emblem | rows 7..120, x 6..200: cut (0, 0, 202 x 122) | 2: (0, 0, 240 x 160) |
| 704,256 | the "FISHING ver." caption | rows 129..140 | inside sprite 2 |

**The join.** The lower piece's first column against the upper's last (x 223),
each vertical offset from -40 to 59 scored by matching opaque pixels' colours:
the best is the lower piece's row 117 at the upper's row 5 (25.5 against 21.6
for the next), at x + 224 - the seam where Capcom's own sprites meet.

**The emblem's place** is not in the art: the PSP's code holds it. Kept the
PlayStation's relation to the text, which centres it over the PSP's text.

## 3. In the engine

`title_states.cpp` (DIV-0130): on a PSP base the sets draw Capcom's sprites 4 /
5 (and the fade's 0xF / 0x10) and 2 with these rectangles, their palettes from
the sprites' own records; sprite 3 and the mark sprite 1 are not drawn. The
press-start and copyright lines are unchanged (the PSP's copyright row is wider
than its sprite and keeps the cut it always had).
