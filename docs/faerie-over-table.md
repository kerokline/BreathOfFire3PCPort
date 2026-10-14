# A faerie drawn through a table, and the dragon gene over its globe (group FY, 2026-10-13)

**Status:** MEASURED (2026-10-13). Two reports, two causes:

- **The faerie and the table** (the owner's `dolphinBoss.txt`): **Capcom's.**
  The same picture with `BOF3X_LAYERING=0` and with Capcom's code; the
  PlayStation's twins of the three functions that decide it order the same
  way; the data that place the faerie are the disc's. A candidate Intent
  entry, the owner's call (section 4). Nothing built.
- **The dragon gene at the lighthouse** (known-defects D242, the owner's
  `eldritchGene.txt`): **ours**, a side effect of DIV-0071. The rule moved
  the gene, a 3D model, out of its layer, past the globe it sits in. Fixed
  here as an amendment to DIV-0071 (section 6).

They are not one cause. The faerie is cut by a raised map cell its own layer
draws after every sprite; the gene was lifted out of a depth sort it shares
with its globe.

All runs are narrow (`--wide 0`), `BOF3X_OPT=none`, `--lang en-US`, from an
ini-less copy of the launcher with a minimal `bof3x.ini` (the cache, point
filter, clean screen) under the scratchpad, against the main checkout's
`bof3/`. Captures are not committed; the commands are given so that they can
be made again. The ones cited are in the main checkout's
`analysis/shots/faerie_1013/` (gitignored), with the three draw-order logs;
`<scratch>` below is the session's `uw_fy/` scratch folder.

## 1. The faerie: where and when

Area 175 (`AREA175.DAT`, opened at recipe frame ~960; the sibling's
`names/places.toml` id 175, the faerie village's community area). The
scene: a wooden room after the party uses the Faerie Tiara, faeries
hovering round a low table made of two planks, the party at the lower left.
A gold-winged faerie stands at the table's front left from about recipe
frame 960 until the scene ends near 2,850; others fly in and settle beside
it (about 1,890 to 2,050).

    python tools/recipe_shots.py tools/recipes/dolphinBoss.txt --every 15 --from 900 --to 2900 --out <scratch>/dolphin_s15.txt
    python tools/input_run.py <scratch>/dolphin_s15.txt --game <main>/bof3 --out <scratch>/run15_ours --lang en-US --no-front --wide 0 --env BOF3X_OPT=none --speed 4 --launcher <copy>/bof3x-launcher.exe

What is wrong, by eye on the 4x crops: the gold faerie's skirt ends on a
straight diagonal - the back edge of the right-hand plank's top - in every
capture of the scene, while its legs, further left, are drawn over the
left-hand plank. Half the table is under the faerie and half over it. The
faeries that fly in pass the same edge.

Which faerie the owner meant is not established; this one is cut in every
frame of the scene, and the instrument says the same rule cuts the purple
faerie behind it (below). **The owner should confirm it is the one.**

## 2. The draw order that frame

    BOF3X_DRAWORDER=1994-1996:140,90,190,125  BOF3X_LAYERING_LOG=1

(320 x 240 coordinates; the gold faerie's skirt is at about x 153..175,
y 107..115.) The log of frame 1995, the lines that matter
(`<scratch>/do_on/bof3x.log`):

| # | What | Layer | Box |
|---|---|---|---|
| 109 | sprite, object `0x7DEF24` (the purple faerie behind), key `0x1580` | 21 | 154..215, 71..126 |
| 112 | list 1: cell (45, 93), word `0x803C` - the table's left plank top | 21 | 130..170, 105..125 |
| 113 | list 1: its side triangle | 21 | 130..150, 115..137 |
| 125 | **sprite, object `0x7DEE80` (the gold faerie), key `0x167F`** | 22 | 140..180, 88..140 |
| 128 | **list 1: cell (46, 93), word `0x804A` - the right plank's top** | 22 | 150..190, 115..135 |
| 129, 130 | list 1: its two side triangles | 22 | 150..190, 125..148 |

DIV-0071's log for the same frame: the gold faerie at screen (159.8, 135.0),
**map cell (45, 93) - the left plank's own cell - feet -1**; the plank's
corners are `4 4 4 4`, the floor's 0. The purple faerie is at cell (44, 92),
feet 0, layer 20 (21 with DIV-0071, stopped there by item 112).

So:

1. **The faerie's ground point is inside the table.** Its position is in the
   left plank's cell and its height `+0x3E` is at the floor (`>> 5` gives
   -1; the key's low byte `0x7F` says the same: `((h ^ 0xF01F) >> 5) & 0xFF`).
   Its feet on screen, y 135, are the floor's near corner of that cell, ten
   pixels below the plank's top. The picture shows the faerie on the table
   only because the sprite is tall.
2. **Its layer is the right plank's.** `Sprite_UpdateScreen` makes the layer
   from the map cell, one row later for a coordinate between cells
   ([`sprite-draw-order.md`](sprite-draw-order.md) §18): 22, the row of cell
   (46, 93). The height never enters the layer; it is the low byte only, and
   orders a sprite against the draw table and other sprites, not against a
   layer's lists.
3. **A raised cell's top is in list 1** (cell word bit 15 set, bit 14 clear:
   `MapView_Build`, [`map-layers.md`](map-layers.md)), and list 1 is emitted
   after the layer's sprites (`DrawLayer_Close`). That is how a block of the
   row in front covers a character standing behind it.

The right plank's top (item 128) and sides are drawn after the faerie and
over its lower part. It is the painter's order working as written, for an
object whose position says it is inside the table. Neither DIV-0071 nor the
height can help: had the table's cells been draw-table items (bit 14), the
faerie's height byte `0x7F` sorts below the plank's lowest corner (`0x84`),
and the plank still comes after.

DIV-0071 is not involved: `BOF3X_LAYERING=0` gives the same walk (the purple
faerie in layer 20 instead of 21, both before item 112), and its three
captures differ from the switch-on run only in a 17 x 10 px box at (382,
237), away from the table (`cmp.py`, PIL `ImageChops.difference`).

## 3. Capcom's code, and the PlayStation's

**Capcom's code on the PC.** The same route with everything Capcom's but the
clock, the window procedure and the display (`--original
"*,-Game_Clock,-Game_WndProc,-Display_Setup"`, `inject: 2 ours, 10086 left
original` - the third is `Rand`'s hook; `Display_Setup` kept ours so that the
game writes its own frames). Capcom's side plays the Chinese text, so its
frames are some 150 frames ahead of ours at the same recipe frame; its
`f01995` matches ours' `f02145` / `f02160` best in the table area. **The
gold faerie's skirt is cut by the right plank's edge the same way**
(`<scratch>/orig_vs_off_zoom.png`). By eye, not pixel for pixel: the
sparkles and wing frames differ.

**The data are the disc's.** `analysis/dat_census.json`, `AREA175`: the map
block (`dest 0x80104000`, tag `0xC8000`, `AreaMap_Header`), its tables and
both images are byte-identical to the Japanese disc's `AREA175.EMI`; only the
enemy table's names and the script / text block (`0x80010000`) differ. The
en-US run takes that block from the US disc (`loc/en-US/dat/AREA175.DAT`
chunk 1, tag 0), and Capcom's side from the PC's Chinese one: both put the
faerie in the same place. Where in the area data the faerie's position is
written was not read.

**The PlayStation's twins** (`SLPS_009.90`, JP, read with capstone
`CS_MODE_MIPS32 | LITTLE_ENDIAN` from the sibling's `disc/`, text at file
offset `0x800`; pairs from `analysis/pairs_propagated.json`):

| PC | PSX | What decides it, PSX side |
|---|---|---|
| `Sprite_UpdateScreen` `0x588F20` | `FUN_8014D184` (pair `call-disputed`) | `0x8014D1B8..0x8014D268`: the layer from `+0x36` and `+0x3A` less the origin halves (`0x8014931C` / `0x8014931E`), one more for each of `+0x34` / `+0x38` non-zero, plus the class table at `0x8017FED4 + 3 * +0x2B`; stored to `+0x32`, `>= 0x37` hides. The height `+0x3E` is read only after (`0x8014D2E4`), for the low byte. As [`movement-script.md`](movement-script.md) 1c had it |
| `MapView_Build` `0x56EC00` | `FUN_80153B8C` (`callers`) | `0x8015413C..0x801541C0`: the cell word's bit 15 set and bit 14 clear appends the item to the list at `0x80142CC0 + layer * 0x30 + (buffer + 2) * 8` - the layer's second list |
| `Sprite_DrawPass` `0x593060` | `FUN_8014BB64` (`callers`) | per layer `0..0x36` (`0x8014C330`): `0x801565FC(layer)` first under flag bit 2 (`0x8014BCFC`), then the sprites and table merge, then `0x80156844(layer)` under bit 2 (`0x8014C320`) |
| `DrawLayer_Close` `0x56FE80` | `0x80156844` | a draw mode committed to slot 6 (`0x8014E494(6, 0xC)`), then the list at `+ (buffer + 2) * 8` linked to the slot `0x8014933A` names - the second list, after the layer's sprites |

So the PlayStation orders the faerie and the plank the same way: same key,
same list, list after sprites. **Verdict: Capcom's.** The rule `psx-twin-check.md`
sets: an Intent entry if it is to change.

**For the owner, on the PlayStation** (an emulator is enough): area 175, the
scene in the wooden room right after the Faerie Tiara is used. Look at the
gold-winged faerie standing at the front left of the two-plank table, at the
bottom right of its skirt, where it meets the right plank's top. On the PC
the skirt stops on the plank's back edge, a straight diagonal. The same cut
on the PlayStation settles it as the original's look; a whole skirt there
would mean a difference this reading has not found (the GPU's rasterisation
is not it: the order, not the edges, decides).

## 4. A fix, proposed and not built

The cut comes from an object placed at floor height inside a raised block,
which no walking character can be. A rule that reads the original's intent
- "on the table" - from that:

> A sprite whose height is below the lowest corner of its own map cell
> (it stands inside a raised cell) is drawn after the second list of its
> layer, as long as everything in that list that reaches it is a top or a
> side of cells of the same height as its own.

In this frame it would move the gold faerie after items 128..130 (all of
the right plank, corners `4 4 4 4` like its own cell), so the whole faerie
draws over the table. The purple faerie is on a floor cell (44, 92) and
would not move; it is cut only where it hangs over the left plank, and the
rule says nothing about that. Cost: about thirty lines in
`src/game/layering.cpp` beside DIV-0071's, the same switch
(`BOF3X_LAYERING=0`). It is an Intent change to Capcom's picture: the
owner's call, after the PlayStation look of section 3. Not ledgered, not
built.

Not measured: whether other scenes place objects inside raised cells
(`BOF3X_LAYERING_LOG` prints each sprite's cell and feet; a census over the
routes would say).

## 5. The dragon gene (D242): what is drawn

`eldritchGene.txt` (the `eldritchGene` save), the lighthouse's top, the gene
stone's pedestal: a faceted translucent globe over a small square plate on a
raised altar, the gene inside it.

    BOF3X_DRAWORDER=2699-2700:60,80,90,110  BOF3X_LAYERING_LOG=1     (DIV-0071 on, the default)
    the same with BOF3X_LAYERING=0
    --original "*,-Game_Clock,-Game_WndProc,-Display_Setup" BOF3X_LAYERING=0

The objects at the pedestal, all at screen point (72.2, 99.0), map cell
(115, 11), feet -6:

| Object | What | Original layer | Drawn as |
|---|---|---|---|
| `0x7DEE80` | the glowing sphere, a sprite (code `0x84`) | 37 | one cell sprite |
| `0x802000`, `0x8020A4` | the globe's facets, models (`+0x24` bit `0x40`) | 38 | semi-transparent `POLY_FT4` records (code `0x2E`) |
| `0x802148` | **the gene**, a model | 38 | two opaque `POLY_FT4` records (code `0x2C`), boxes 66..77 x 83..102 and 66..74 x 85..103 |

**With DIV-0071 off**, and in Capcom's code, the gene's two quads are walk
items #244 and #251, among the facets' (#249..#275): the pass sorts every
record of layer 38 by depth together, so most of the facets are drawn over
the gene and tint it - the gene seen inside the globe.

**With DIV-0071 on**, the log says `sprite 00802148 ... layer 38 drawn in
layer 41; stopped by nothing`, and the gene's quads come last (#316, #317),
after every facet: a crisp black gene on top of the globe.

Captures (`gene_w.txt`, shots at 2400, 2700, 3000, 3300): **ours with
DIV-0071 on differs from ours with it off only inside the gene's box**,
(134..154, 175..203) in 640 x 480, in two of the four; with Capcom's code at
3,300 the same box and nothing else. The silhouette is identical in all
three - the difference is the tint (`<scratch>/gene_bottom.png`: on, off,
and their difference mask). No primitive is drawn over the gene's box after
it in either order, so nothing "clips" it in the sense of covering: the
gene's bottom edge is where its own quads end.

**Against the owner's PlayStation picture** (the rays before the flash,
`images/2.webp` of the session): the gene is grey inside the globe, as ours
with DIV-0071 off and Capcom's. Its foot meets the plate at about the same
place: the gene's lowest point is 60 % of the way from the plate's back
corner to its front on the PSX and 62 % on the PC (by eye on the crops,
`gene_three.png`; the camera and moment differ). So what reads as "under the
floor" is, on the evidence, the gene drawn over the globe that should veil
it, with its foot - unveiled and black - on the plate's back edge. That the
gene's model reaches no lower on the PlayStation is a judgement by eye, and
the owner's eye should confirm it.

## 6. The fix for D242: DIV-0071 leaves models in their layer

`src/game/layering.cpp`, `LayerFor`:

1. **A model (`+0x24` bit `0x40`) keeps its layer.** The draw pass turns a
   model into records - one per quad - and sorts them by depth with the
   records of every other sprite and model of the layer
   ([`sprite-draw-order.md`](sprite-draw-order.md) §2, §12). A model moved to
   a later layer leaves that sort and is drawn after all of it. Its feet's
   box, which the rule tests, means nothing for a model either.
2. **A sprite whose body overlaps a model of its own layer keeps its layer**,
   for the same reason from the other side: moved, it would leave the depth
   sort it shares with the model's quads.

Both log under `BOF3X_LAYERING_LOG=1` ("is a model: layer kept", "beside
model ...: kept"). `BOF3X_LAYERING=0` is still Capcom's order; nothing
changes there. The rule arms after the start-up self-tests, so no fuzz
covers it, as before; the checks are live captures.

**Checked, live, old DLL (`a80f30a5`) against new, DIV-0071 on, narrow,
x4:** see section 7.

## 7. Verification of the amendment

Old DLL (built at `a80f30a5`) against new, both DIV-0071 on, narrow, x4,
`BOF3X_OPT=none`, from two launcher copies (`<scratch>/batch.sh`,
`nina.sh`, compared by `abcmp.py`, PIL difference):

| Route | Language | Shots | Identical |
|---|---|--:|--:|
| `field_view.txt` (the floor over Ryu's foot, DIV-0071's first case) | original | 4 | 4 |
| `worldmap_sliver.txt` (the Cedar Woods node, forest in front) | original | 1 | 1 |
| `worldMapAndAreaTransition_ab.txt` | en-US | 35 | 35 |
| `ninaWalkBehindBlock.txt`, every 60 frames (`--save ninaWalkBehindBlock`; the recipe has no `# save` line) | en-US | 11 | 11 |
| `dolphinBoss.txt`, every 60 to 3,600 (the faerie scene included) | en-US | 60 | 60 |
| `eldritchGene.txt` at 2,400 / 2,700 / 3,000 / 3,300 | en-US | 4 | **0** - each differs in the gene's box only, (134..154, 175..203), 287 pixels |

And the four gene shots of the new DLL against DIV-0071 off: the gene's box
identical in all four (two identical whole; two differ in a 10 x 4 box at
(355, 282), a character's feet that DIV-0071 still repairs); against
Capcom's code at 3,300: **identical, the whole frame**.

Headless: `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'`
from an ini-less copy, `BOF3X_WIDE` unset, 1 and 2: SELFTEST.
`python tools/ledger_check.py`: 121 entries, 0 errors, 2 notes.

## 8. What was not done

- The faerie's fix (section 4): proposed only.
- A census of where else DIV-0071 moved a model, or a sprite past a model,
  before this amendment: the captures of section 7 are the routes DIV-0071
  was verified on, plus these two.
- A plain sprite sorted *before* another of its layer by depth but listed
  after it is judged against the list's order, not the depth order the pass
  draws in (`LayerFor`'s later-sprites loop). It did not show here; it is
  the same class as item 2 of section 6 and is noted, not changed.
- Wide (`BOF3X_WIDE=1` / `2`): not run. The rule works in the primitives'
  coordinates, which the wide shift moves after it, as DIV-0071's own wide
  check found.
- D243 (the lighthouse's rays before the flash): not looked at.

## For the other files

**A - `docs/DIVERGENCE.md`, DIV-0071, an amendment** (append to the entry):

- **Amended 2026-10-13 (known-defects D242, `docs/faerie-over-table.md`
  §5-7):** a model (`+0x24` bit `0x40`, drawn as depth-sorted quad records)
  is never moved, and a sprite whose body overlaps a model of its own layer
  is not moved either. Moving the dragon gene out of layer 38 took it out of
  the depth sort with its globe's facets and drew it over the globe instead
  of inside it (`eldritchGene.txt`, the pedestal). Verified (§7): against the
  rule before the amendment, `field_view.txt` 4 of 4, `worldmap_sliver.txt`
  1 of 1, `worldMapAndAreaTransition_ab.txt` 35 of 35,
  `ninaWalkBehindBlock.txt` 11 of 11 and `dolphinBoss.txt` 60 of 60 shots
  identical; the gene's four differ in its box only, which is now the same
  as with the switch off, and at 3,300 the whole frame equals Capcom's.

**B - `docs/known-defects.md`, D242:** cause found - DIV-0071 moved the gene
(a model) past its globe's semi-transparent facets; ours, not Capcom's (the
gene inside the globe with `BOF3X_LAYERING=0` and with Capcom's code, as on
the owner's PlayStation picture). Fixed by DIV-0071's 2026-10-13 amendment.
The silhouette is the same on all sides; the owner's eye owed on whether the
PlayStation's gene reaches lower.

**C - `docs/known-defects.md`, a new entry:** the faerie drawn through the
table (`dolphinBoss.txt`, area 175, recipe frames ~960..2850): Capcom's -
an object placed at floor height inside a raised cell, cut by the next
cell's top in its layer's second list; the PSX twins order it the same way.
Candidate Intent entry, section 4's rule; the owner's PlayStation look owed
(section 3's scene).

**D - `docs/owner-review.md`:** two looks - the faerie scene on the
PlayStation (section 3), and the gene with the fix (section 5's frames).

**E - `docs/README.md`:** a row for this document.
