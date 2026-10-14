# The camp's fire under Garr, and the gene crystal behind the wall (group CF, 2026-10-14)

**Status:** MEASURED (2026-10-14). These are two of the owner's layering
reports, with two verdicts:

**Numbered at the round's fold-in (2026-10-14):** draft A is DIV-0071's 2026-10-14 amendment; the campfire is D301, the crystal D242.

- **The cutscene campfire drawn under the characters**
  (`vaccinationAndFireScene.txt`): **ours**. It is a side effect of DIV-0071.
  The rule moved Garr three layers later, past the fire, because the fire's
  map cell record is three cells from his and the rule looked only two cells
  out. With `BOF3X_LAYERING=0`, and with Capcom's code, the fire is drawn
  over him. The cutscene fire and the field camp's fire are the same thing:
  one record, handler `0x10` (`MapCell_DrawFrames`), at the same map cell of
  both areas. It is fixed here as a second amendment to DIV-0071 (section 4).
- **The gene crystal clipped by the wall** (known-defects D242,
  `eldritchGene.txt`): **Capcom's**. In the crystal's box the picture is
  pixel-identical with DIV-0071 on, with it off, and with Capcom's code. The
  crystal is a plain sprite of cells, not moved by DIV-0071. Its art reaches
  below the cut, and the raised wall cell three rows nearer is drawn over it
  by the original's painter's order. The area's map data is byte-identical to
  the Japanese disc's. The owner has since compared it with a let's play of
  the original and seen it clipped the same way. A change would be an Intent
  entry and is the owner's call (section 6). Nothing was built for it.

All runs were narrow (`--wide 0`), `BOF3X_OPT=none`, x4, in the routes' own
language (en-US, or `original` for `field_view` and `worldmap_sliver`). They
ran from ini-less launcher copies with a minimal `bof3x.ini` (cache, point
filter, clean screen) under the session's scratch folder `uw_cf/`, against
the main checkout's `bof3/`. `launch` is the build of `aa418238` (the round
branch's tip) and `launch_new2` is this branch's. The shot recipes come from
`tools/recipe_shots.py`; `uw_cf/run.sh` wraps `tools/input_run.py` with the
flags above. The captures cited for the owner are in
`analysis/shots/campfire_1014/` (gitignored, not committed).

    python tools/recipe_shots.py tools/recipes/vaccinationAndFireScene.txt --every 10 --from 1378 --to 1382 --out <scratch>/vacc_do.txt
    python tools/input_run.py <scratch>/vacc_do.txt --save vaccinationAndFireScene --game <main>/bof3 --out <scratch>/vacc_on \
        --lang en-US --no-front --wide 0 --env BOF3X_OPT=none --speed 4 --launcher <scratch>/launch/bof3x-launcher.exe \
        --env BOF3X_DRAWORDER=1376-1378:125,110,205,170 --env BOF3X_LAYERING_LOG=1

## 1. The campfire: what draws it

At recipe frame 1,378 the cutscene is in `AREA053` (the log's `open` lines).
In the draw-order walk of that frame (`BOF3X_DRAWORDER`, rectangle (125,
110)-(205, 170) round the fire), the lines that matter are:

| # | What | Layer | Box |
|---|---|---|---|
| 183 | sprite, object `0x802D40`, key `0x198D` | 25 | 108..132, 110..142 |
| 217, 218 | **cell, view row 26 col 14, handler `0x10`, cell bytes `0x151A`**: two code-`0x2E` primitives (semi-transparent `POLY_FT4`) - the fire | 28 | 137..183, 85..162 |
| 219 | **sprite, object `0x7DEE80` (Garr), key `0x1C8D`** | 28 | 104..208, 81..141 |

The fire is not an effect, and not a sprite either. It is a **map cell
record**: handler 16 of `MapCell_Handlers` (`0x663008 + 0x10 * 4` reads
`0x570870`, `MapCell_DrawFrames`, read with `tools/pe_read.py`'s `Pe` class,
`uw_cf/handlers.py`), whose quads change texture on a `Frame_Counter`
period. The record's run is anchored at map cell (0x15, 0x1A) = (21, 26).
`DrawLayer_Open` hands the run to its handler for the layer's view row
**before** the layer's sprites.

**The field camp's fire** (`campingFishing.txt`, `AREA090`, recipe frame
600; the same instrument, rectangle (50, 110)-(140, 200)) is the same
record: #212, #213, view row 27 col 12, handler `0x10`, bytes `0x151A`, two
code-`0x2E` quads, layer 29. There is no "cutscene campfire" that renders
differently. The two scenes differ only in who stands near the fire. At the
camp nobody stands behind it, so nothing shows the order (`camp_f00600.png`).
`caughFish.txt` was not run: it goes to the fishing spot (`AREA030`), not
the camp.

## 2. Why Garr lands over it

`BOF3X_LAYERING_LOG=1`, the same frames:

    DIV-0071    frame 1373: sprite 007DEE80 at (169.881, 121.572), cell 18,24, feet 13: layer 25 drawn in layer 28; stopped by nothing

Garr's own layer is 25, three before the fire's 28, so Capcom's order draws
him first and the fire over him. DIV-0071 moved him up to its limit of three
layers because everything in layers 26..28 that reached his feet was floor.
What should have stopped him is the fire's record, and the rule has only one
test for cell records (`RecordsClear`): a run anchored **within two cells**
of the sprite's own cell. The fire is anchored at (21, 26) and Garr stands
at (18, 24), so dx = 3. The test missed it by one cell. Garr's sprite is
104 px wide, so a record three cells away still draws over his body.

**With `BOF3X_LAYERING=0`** (Capcom's order), the walk has Garr as #184 in
layer 25 and the fire as #219 / #220 in layer 28: the fire is over him
(`fire_on_off_capcom.png`, left to right: DIV-0071 on, off, Capcom's).
**Capcom's code** (`--original "*,-Game_Clock,-Game_WndProc,-Display_Setup"`,
`BOF3X_LAYERING=0`, `inject: 2 ours, 10086 left original`; its Chinese text
runs a little apart from ours, so it is compared by eye) puts the fire over
Garr in all ten shots from 1,260 to 1,800.

**Verdict: ours.** Nothing on the PlayStation needs reading for this one.
The order DIV-0071 departs from is Capcom's, and FY's twin reading
([`faerie-over-table.md`](faerie-over-table.md) section 3) has the PSX
`0x801565FC(layer)` (the cell records) before the layer's sprites, as here.

## 3. The owner's look

The owner asked for the flames on top of the characters. That is Capcom's
look and the field camp's, and it is what the fix restores. The owner has
seen it in a replay of this branch and says it looks good (2026-10-14, via
the coordinator).

## 4. The fix: a record that drew over the body stops the move (DIV-0071, second amendment)

`src/game/layering.cpp`, `src/game/draw_emit.cpp`:

1. **What each record run drew is recorded.** Under mode 1 only,
   `DrawLayer_Open` brackets each cell's run with
   `layering::RunBegin(run)` / `RunEnd()`. `Gfx_CommitPrim` hands each
   primitive committed in between to `RunPrim`, which keeps the run's
   `POLY_FT4` and `POLY_G4` quads (code `0x2C` / `0x38`, low two bits aside;
   the four float points at +8, +0x18, +0x28, +0x38, as the handlers'
   `Project4` writes them) in perimeter order. It keeps up to eight; past
   that, their union box. Two generations are kept, stamped by pass, in a
   2,048-entry table keyed by the run's address.
2. **The verdict uses the last pass.** `LayerFor`, after the proximity test
   (unchanged), asks `RunsClear(next, whole_box)`. It walks the row and the
   columns `DrawLayer_Open` walks for that layer, computed as it computes
   them. A run whose kept quads reach the sprite's body and feet box (the
   same convex test `Reaches` uses for the lists' quads, now
   `QuadReaches`), or a run that was not drawn in the last pass at all,
   stops the sprite at the layer before. It logs under
   `BOF3X_LAYERING_LOG=1` ("what a cell record drew").
3. **Not kept, and so left to the proximity test as before:** other
   primitive codes, and any quad with a point outside +/-4,096. A vertex
   behind the eye projects to about 1e24; one such quad, from a record of
   the tent beside the fire, would otherwise have stopped every sprite in
   its rows. The first version of this amendment used a union box and
   counted unknown codes as covering the screen. It undid three of
   DIV-0071's shadow repairs (`worldMapAndAreaTransition_ab` f01860, the
   gene route's f02700, `dolphinBoss` f00720 in part). That is why the
   quads are kept.

The proximity rule stays, so the amendment can only **stop** moves. It never
moves a sprite that was not moved before. Every pixel it changes is
therefore the original's order at that spot (measured in section 5).

**Switches.** `BOF3X_LAYERING=0` is Capcom's order, as before.
`BOF3X_LAYERING_RECORDS=0` keeps DIV-0071 and drops only this amendment.
That is the negative control: with it, the build behaves as `aa418238` did.
`BOF3X_ORIGINAL` naming `DrawLayer_Open` leaves no run recorded, and the
test then stands aside (it needs a non-zero count from the last pass), so
it can never stop everything for want of data.

**Cost:** the table is 2 x 2,048 x 300 bytes of static data, about 1.2 MB.
Per frame, one hash insert per drawn run and one `RunPrim` per committed
quad, only while a run is open.

**One frame behind.** The quads are from the pass before. Under a moving
camera they are a few pixels stale. A run drawn for the first time, when a
scene opens or a cell scrolls in, counts as in the way, which is the
original's order, for its first frame.

**No fuzz.** DIV-0071 arms after the start-up self-tests (`Arm` in
`InjectAll`, as before), and so does every line added here: `g_mode` is
`kOff` until then, `RunBegin` is called only under `kFloor`, and
`g_in_run` is never set before it. The self-tests compare `Gfx_CommitPrim`
and `DrawLayer_Open` with Capcom's under the switch's default state, so
they see the original's code path. The checks are the live captures below
and the headless runs of section 7.

## 5. Verification: before and after

Old DLL (`launch`, `aa418238`) against new (`launch_new2`), both with
DIV-0071 on: narrow, x4, `BOF3X_OPT=none`. The scripts are `uw_cf/b_ab.sh`
and `b_ab2.sh`, compared by `cmpdir.py` (PIL `ImageChops.difference`);
`boxcmp.py` and `cmpdir.py` compare against `BOF3X_LAYERING=0` runs of the
new DLL.

| Route | Lang | Shots | Identical | The rest |
|---|---|--:|--:|---|
| `vaccinationAndFireScene.txt`, every 60 from 1,260 to 2,940 (`--save vaccinationAndFireScene`) | en-US | 29 | 0 | **each differs in the fire's box only**, (286..363, 198..275) in 640 x 480, 3,570..3,694 px: the fire now over Garr |
| `campingFishing.txt`, every 60 to 2,700 (the field camp's fire at 600 / 660) and its nine shots | en-US | 50 | 50 | |
| `field_view.txt` | original | 4 | 4 | |
| `worldmap_sliver.txt` | original | 1 | 1 | |
| `worldMapAndAreaTransition_ab.txt` | en-US | 35 | 35 | |
| `ninaWalkBehindBlock.txt`, every 60 (`--save ninaWalkBehindBlock`) | en-US | 11 | 11 | |
| `dolphinBoss.txt`, every 60 to 3,600 | en-US | 60 | 58 | f00720 (299..315, 388..397) 55 px and f03060 (301..457, 224..306) 629 px; **in both boxes the new frame equals DIV-0071 off, 0 px** |
| `eldritchGene.txt` at 2,400 / 2,700 / 3,000 / 3,300 (FY's set) | en-US | 4 | 4 | |
| `eldritchGene.txt` every 30 from 330 to 720 | en-US | 14 | 14 | |

The two `dolphinBoss` frames are the fire's class. In f03060
(`dolphin_f03060_before_after_off.png`: before, after, off), Ryu stands
behind a bush and Nina behind a tuft of grass, both plant records nearer
than they are. The old rule drew both over their plants. The new one stops
them, and the plants are in front again, as in Capcom's order. In f00720
(`dolphin_f00720_before_after_off.png`), the grass tuft in front of the
character's shadow now covers its lower edge again, as with the switch off.
Whether a grass tuft in front of a shadow *should* cover it is the same
question DIV-0071 answered for floor. These tufts are records, which the
ledger already lists as things that stop the rule, so it is not reopened
here.

## 6. The gene crystal (D242)

**What it is.** `eldritchGene.txt` (the `eldritchGene` save), `AREA077`,
recipe frame 340, rectangle (90, 70)-(135, 110):

| # | What | Layer | Box |
|---|---|---|---|
| 92 | **sprite, object `0x7DF1B4`, key `0x1D6A`** - the crystal, one code-`0x84` cell sprite | 29 | 95.7..127.7, 74.7..106.7 |
| 94, 95 | list 1, cell (107, 13), word `0x8035`, and a side triangle - the wall top to its left | 29 | x below 92.3: they do not reach it |
| 99, 100 | list 0, cells (108, 13) and (109, 12): floor | 30 | |
| **142** | **list 1, cell (109, 14), word `0x805C` - the top of the wall in front** | 32 | 67.2..108.4, 89.8..109.8 |
| **143** | **its side triangle (the wall's face)** | 32 | 86.4..109.1, 99.8..135.3 |

DIV-0071's log for it: `cell 108,12, feet -22: layer 29 drawn in layer 29;
stopped by a cell record nearby`. DIV-0071 does not move it, and moving it
later could not help in any case: what covers it is a raised cell (bit 15
set, bit 14 clear: list 1), which no rule of DIV-0071 crosses. The
crystal's screen point (111.7, 95.7) is the centre of its floor cell's
diamond (#85, cell (108, 12), 92.3..130.8 x 86.5..105.1). It stands on
the floor at the centre of its cell. Its art reaches 11 px below that point
and over the wall's top corner, which is two rows nearer and drawn after it.

**The art from its sheet, outside the scene** (the owner's test,
`analysis/shots/campfire_1014/gene_sheet_vs_scene.png`). The run froze on
`shot f00340` (`BOF3X_SHOT_WAIT`, `--delay 30`), and `uw_cf/cell_dump.py`
read, without writing, the primitive the walk logged for object `0x7DF1B4`
(`0x7F1DB8`): point (111.66, 95.72), scale 1, cells 0..7 of
`SpriteCell_Table` `0x6BEA18`, CLUT word `0x7C84`, flags `0x89` (no mirror).
It also read those eight records and the VRAM shadow `0x6C9F44`.
`cell_compose.py` lays the records out as `D3d_BuildCellTexture` does
(each at (x, y) of a stage; tpage `0x89`: page column 9, 4-bit; u, v; the
size nibbles x 8; colour 0 transparent). Their opaque extent is x -15..15,
y -21..11 about the point, 30 x 32 px. The sheet puts it at 4x on grey
beside the capture's crop of the same game pixels at 4x, with a red line
on the art's lowest row. **The art continues below the cut:** its lower-left
facets and the dark row under them are whole on the left and hidden on the
right, where the wall's top and face cover them. The art is not short.
Something is drawn over it, and that something is #142 / #143.

**Capcom's code.** Capcom's code (the same `--original` set, its own shots
every 30 from 300 to 600) against ours with DIV-0071 off, ours with it on,
and this branch, at f00330 in the box (186..260, 144..220) of 640 x 480:
**0 of 5,624 px differ** in all three (`uw_cf/genebox.py`;
`gene_on_off_capcom.png`).

**The data.** `analysis/dat_census.json`, `AREA077`: the map block
(`0x80104000`, tag `0xC8000`, 60,600 bytes), both images, and the tables at
`0x8002BE00`, `0x8002A000` and `0x8002D800` are byte-identical to the
Japanese disc's `AREA077.EMI`. Only `0x800E4000` (549 bytes) and the script
and text block (`0x80010000`) differ. The en-US run takes the latter from
the US disc. Where the crystal's position is written was not read.

**The PlayStation.** The order is decided by `Sprite_UpdateScreen`'s layer
(the crystal's 29), `MapView_Build`'s list choice (the wall top to list 1)
and `DrawLayer_Close` (list 1 after the layer's sprites, layer 32 long after
layer 29). FY read the PSX twins of all three
([`faerie-over-table.md`](faerie-over-table.md) section 3: `FUN_8014D184`,
`FUN_80153B8C`, `0x80156844`), and they order the same way. They were not
read again here. The owner's PlayStation image of the session
(`images/2.webp`) shows the lighthouse's bulb, not the crystal, so it
settles nothing here. The owner's let's play of the original (2026-10-14,
via the coordinator) shows the crystal clipped the same way.

**Verdict: Capcom's.** The port draws it as the original does. If it is to
change, the change is an Intent entry and the owner's call. One candidate,
**not built**: a sprite whose art reaches below its ground point by more
than a few pixels (a sprite drawn centred on its point, as this one is)
could be drawn after a raised cell of the next two rows when that cell's
top is no higher than the sprite's point. It is the faerie's class
([`faerie-over-table.md`](faerie-over-table.md) section 4) from the other
side, and a census of such sprites (`BOF3X_LAYERING_LOG` prints each
sprite's cell and feet) should come first.

## 7. Headless

`BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'` from an
ini-less copy (`uw_cf/selftest/`, launcher, DLL and exe only),
`--game <main>/bof3 --no-config`, with `BOF3X_WIDE` unset, `1` and `2`
(`uw_cf/b_self.sh`):

- All three exit 0 (the hosted game's exit code, read by the launcher).
- `inject: 10087 ours, 0 left original by BOF3X_ORIGINAL` in each.
- The DIV-0041 width-rules line is the same in all three (53 and 120
  columns' constants).
- Unset: the two band self-tests report "not run (narrow: no band)". At
  `=1` / `=2`, the `sprite_draw` flag split (16,000 rounds, 6,945 / 7,483
  in the band, the control refused in 4,112 / 4,429) and the
  `field_frame` band (4,000 rounds) each report 0 MISMATCHES. The
  widescreen line reads 53 columns (426 x 240) and 120 columns
  (560 x 240) respectively.
- 1,079 / 1,081 / 1,081 lines name mismatches, and every count in them is
  0 (`grep -i -E "[1-9][0-9]* mismatch"` finds none). Among them is
  `draw_emit` (12,000 rounds of `Gfx_CommitPrim` and `DrawLayer_Close`,
  0), `DrawLayer_Open` (12,000 rounds, 219,759 handler calls, 0) and the
  map-cell handlers (24,000 rounds, 0).

As with DIV-0071 itself, the amendment arms after these self-tests. They
show that `Gfx_CommitPrim` and `DrawLayer_Open`, with the new hooks in
them, still match Capcom's while the switch is unarmed.

`python tools/ledger_check.py`: 121 ledger entries, 0 errors, 2 notes.
The amendment's text (A below) waits for the coordinator.

## 8. What was not done

- **The PSX twins for the crystal** were not read again (FY's reading is
  cited). The crystal on a PlayStation was not seen by this group.
- **Wide** (`BOF3X_WIDE=1` / `2`) was not run live. The quads are kept in
  the primitives' own coordinates and tested against the sprite's box in
  the same coordinates, before the wide shift, as DIV-0071's own tests are.
  The view walk uses the inset as `DrawLayer_Open` does, including
  DIV-0041's lowered one.
- **A census** of where else the old rule drew a sprite over a record beyond
  two cells: the routes of section 5 found two such frames, both plants.
- `caughFish.txt` was not run (it does not reach the camp).

## For the other files

**A - `docs/DIVERGENCE.md`, DIV-0071, an amendment** (append to the entry):

- **Amended 2026-10-14 (the owner's campfire report,
  `docs/campfire-and-crystal.md` sections 1-5):** a cell record stops the
  rule when what its run drew in the last pass reaches the sprite, as well
  as when it is anchored within two cells. The fire of the camp (a
  `MapCell_DrawFrames` record, the same in the cutscene's `AREA053` and the
  field's `AREA090`) is anchored three cells from Garr. The rule drew him
  three layers later, over the flames, where Capcom's order puts the flames
  over him. The runs' `POLY_FT4` / `POLY_G4` quads are kept from each pass,
  up to eight a run (`DrawLayer_Open`, `Gfx_CommitPrim`, mode 1 only), and
  tested with the lists' convex test. Quads with a point past +/-4,096 and
  other primitives are left to the proximity test. A run not drawn in the
  last pass counts as in the way. The amendment only stops moves.
  `BOF3X_LAYERING_RECORDS=0` drops it (the control). Verified against the
  rule before it, live, narrow:
  - `vaccinationAndFireScene.txt`: 29 shots, each differing in the fire's
    box only;
  - `campingFishing.txt` 50 / 50 identical;
  - `field_view.txt` 4 / 4, `worldmap_sliver.txt` 1 / 1,
    `worldMapAndAreaTransition_ab.txt` 35 / 35 and
    `ninaWalkBehindBlock.txt` 11 / 11 identical;
  - `eldritchGene.txt` 18 / 18 identical;
  - `dolphinBoss.txt` 58 / 60: the other two have plants in front of a
    character drawn in front again, equal in their boxes to the switch off.

**B - `docs/known-defects.md`, a new entry:** the camp's fire drawn under
Garr (`vaccinationAndFireScene.txt`, `AREA053`, recipe frames ~1,260..2,970;
the owner, 2026-10-14). Cause: DIV-0071 moved him past the fire's cell
record, three cells from his own, beyond the rule's two-cell record test.
Ours, not Capcom's: the fire is over him with `BOF3X_LAYERING=0` and with
Capcom's code. Fixed by DIV-0071's 2026-10-14 amendment. The owner has seen
it fixed in a replay.

**C - `docs/known-defects.md`, D242, rewritten:** the dragon gene is the
purple-blue crystal at the upper left at the start of `eldritchGene.txt`
(recipe frames ~330..700, `AREA077`). Its lower-left corner is covered by
the top and face of the raised wall cell (109, 14), two rows nearer, drawn
in layer 32 after the crystal's 29. The art reaches below the cut (composed
from VRAM, `gene_sheet_vs_scene.png`), so it is covered, not short.
**Capcom's**: 0 px differ in the crystal's box between ours (DIV-0071 on
and off) and Capcom's code; the map data is the disc's; the PSX twins order
the same; the owner's let's play of the original shows it the same. A
candidate Intent change is in section 6, the owner's call. The 2026-10-13
fix FY filed under D242 was not this crystal (see D).

**D - a correction to `docs/faerie-over-table.md` (FY), sections 5-7 and
its "For the other files" A, B and D:** what FY's D242 fix changed (the
model at the pedestal, object `0x802148`, drawn inside the globe's facets
again) is **the lighthouse bulb's filament**, not the dragon gene. The
owner likes the filament's current look and the fix stays as built. The
dragon gene is the purple-blue crystal at the upper left at the start of
the route (section 6 here). D242's text and DIV-0071's 2026-10-13
amendment should name the filament, and D242 should point here for the
crystal.

**E - `docs/owner-review.md`:** the campfire, seen and approved
(2026-10-14). Owed: whether the crystal should change (section 6's
candidate), and a look at `dolphinBoss` f03060 / f00720, where plants in
front of a character are in front again.

**F - `docs/README.md`:** a row for this document.
