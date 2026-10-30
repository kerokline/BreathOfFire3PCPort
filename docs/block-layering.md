# Garr beside the pushed block: a model's box in the layering rule (issue 141)

**Status:** MEASURED (2026-10-29). Fixed by DIV-0071's 2026-10-29 amendment on
`fix/issue-141-model-box`; the owner saw the sheet and called it resolved.

## 1. The reports

- ScrubOasis, GitHub issue #141 (2026-10-24): a screenshot of Garr beside a
  pushed stone block in Angel Tower, his elbow and shoulder drawn over the
  block's top while his feet stand behind its near corner.
- The owner, 2026-10-29, after recording `garrBlocks` (the save
  `tools/recipe_saves/garrBlocks.DAT`, Garr leading in `AREA083`): "several
  places where he incorrectly layers with the blocks - my bet is this is due
  to his extra large sprite. The main issue appears to be at the corner of a
  block, where he starts to walk past the block's bottom edge, but part of
  his sprite still is behind its upper part."

Read first as opposite complaints about the same spot; they were the same
one. Shown the sheet, the owner circled frames 440 and 1,060 (Garr drawn
over the block) and called them resolved (section 4).

## 2. What was measured

All on the PC install, en-US, layering on unless said, root
`D:\bof3x-runs\lay141` (its `lay-installs.toml`, launchers `L` the `main`
build `b7bd1d29` and `L2` the fix); sheets in `analysis/shots/i141/`.

1. **A/B/C, narrow, a frame every 10** (`report_lay_garrBlocks_garrBlocks.md`):
   the PC, the US PlayStation disc alone and the US disc with the European
   PSP disc (ScrubOasis's install shape) draw the field identically (the
   A-B and A-C differences are the title and the load screen, and two
   texture-detail frames of the block). So the fault is not an install's.
   Against `BOF3X_LAYERING=0` the PC differs in 45 frames, every one Garr's
   box beside a block - the rule is what moves him.
2. **The rule's log** (`BOF3X_LAYERING_LOG=1`, a `BOF3X_DRAWORDER` window,
   the two log lines now carrying the key and the position words): the block
   is object `0x7E12E0`, a model (`+0x24` bit `0x40`) of 25 quads, 85 x 93
   px on screen, at cell 62,61; Garr `0x802D40` walks down its west side at
   cell 60,61 then 60,62, two cells west of its centre. His frame is 72 px
   wide (`draworder` box 128.8..200.8); the block's screen point is 52 px
   right of his and 13 to 24 px lower. In rows (x + z, one row a cell), the
   block's centre is 1.25 to 2 rows nearer than Garr's feet the whole way
   down its side; its near corner, for a block drawn symmetric about its
   point, is at the centre's row.
3. **What happened in `main`:** a model stops a sprite's move past its layer
   through the later-sprite test, whose box for a model was 28 x 50 px round
   its screen point (a model draws no cell sprite, so `Reach` had nothing to
   grow it with). Garr's body box reached that small box by 4 px; on walking
   frames where his drawn box is narrower it did not, and the rule took him
   past the block's layer: "stopped by nothing" at frames 368, 1,032, 1,156
   (a frame each) and from 1,057 and 1,180 for as long as he stood there, and
   from 428 to 496 beside the other block (`0x7E1260`). In every one he is
   drawn over the block while his feet are behind its corner - the issue's
   screenshot (`f440_zoom.png`, left).
4. **The fix:** `Sprite_DrawPass` hands each model's records to
   `layering::AfterModel` after `Sprite_AddDrawRecords` makes them; the
   union of the quads' screen points is kept in the sprites' box table
   (`Keep`), and `Reach` grows a model's box with it as it grows a sprite's.
   A frame every 4 from route frame 300, 16:9: 54 of 256 frames differ, each
   in Garr's box beside a block, each now as `BOF3X_LAYERING=0` draws it
   (`fix_sheet.png`). Headless `BOF3X_SHADOW='*'`: section 5.

## 3. Why the remainder is Capcom's drawing

Within the block's layer the pass sorts the sprite's one record against the
block's quads by depth, each quad by its farthest vertex
([`sprite-draw-order.md`](sprite-draw-order.md) 2 and 12): a quad is drawn
over the sprite only when all four of its vertices are nearer than the
sprite's point. At frames 1,033..1,056 (Garr standing two cells west of the
centre, half a cell south of it, feet 1.5 rows behind the corner) 22 of the
25 quads are wholly nearer than his feet, and 11 of them reach his frame.
His right arm, 36 px east of his feet, lies over the block's west face, and
that face runs from the corner (the centre's row) to the front corner - all
of it nearer than where he stands. A 72 px billboard beside a box two and a
half cells wide has no drawing that is right from every angle: drawn behind,
his arm vanishes into the face; drawn in front, his elbow floats over the
top. Capcom's order chooses behind, on the PlayStation as on the PC (the same
key and the same per-quad sort, `FUN_8014D184`), and after the fix ours does
too, everywhere the feet are behind the corner.

## 4. What stays, and the owner's answer

The fix repairs ScrubOasis's frames and the one-frame pops. Shown
`fix_sheet.png`, the owner circled frames 440 and 1,060 as the scenarios they
meant and called them resolved (2026-10-29 night). The first reading of
their report - Garr behind the block's near face with his feet 1.25 to 2
rows behind its corner (`behind_corner_f1044.png`, `corner_on_sheet.png`
1,000..1,052) - was wrong; that case is not reported, and the fix leaves it
as Capcom draws it. Should it ever be wanted otherwise, the choices:

- **Keep Capcom's.** Behind until the feet pass the corner's row, then in
  front at once (frames 1,057 on in the fix build are the pop at that row).
- **A taste rule:** a party sprite beside a model is drawn in front once its
  feet are within N rows of the model's centre (N about 1.5 would put the
  1,033..1,056 frames in front). It would also put ScrubOasis's frames back,
  since those are the same geometry mirrored (`f440_zoom.png`: feet 1.25
  rows behind the right corner). The two reports cannot both be met by a
  depth rule; only by a per-pixel one the engine does not have.

Nothing is built for the second; the measurements above are what it would
be tuned against.

## 5. Gate

`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'` from launcher `L2` on the fix build:
exit 0, 1,095 mismatch lines every one 0 (the rule arms after the
self-tests, so this shows only that they still pass).
