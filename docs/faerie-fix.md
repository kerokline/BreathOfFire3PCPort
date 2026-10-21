# The table faerie drawn over its table (wide-ring round, side group FY, 2026-10-21)

**Status:** BUILT AND MEASURED (2026-10-21), on branch `wide-ring/fy`; the ledger number
is the orchestrator's (the entry is drafted in section 6). Known-defects
D300; the owner's decision of 2026-10-15: fix this scene narrowly, as an
Intent entry, with a note to broaden it to
[`faerie-over-table.md`](faerie-over-table.md) section 4's rule if more
examples of the class turn up. **The owner's look, 2026-10-21 (chat):** the
before/after sheets accepted ("the faerie looks great"), the sides' second
draw with them; not yet seen in play.

What was wrong, in one line ([`faerie-over-table.md`](faerie-over-table.md)
1-4): in area 175, the faerie village's wooden room after the Faerie Tiara,
the gold-winged faerie at the table's front left stands at floor height
inside the left plank's raised cell, and the right plank's top - in its
layer's second list, drawn after the layer's sprites - cuts its skirt on a
straight diagonal. Capcom's on both platforms (the PlayStation twins order it
the same way; the owner's PlayStation look, 2026-10-15).

All runs: `BOF3X_OPT=none`, an ini-less launcher copy, FY's own copies of the
game folder, the harness root `C:\Users\kerok\bof3x-runs\r_ring\FY\`
(`chain.py`, `run.py`, `check.py`, `cmp.py` there). Captures are not
committed.

## 1. Which object it is, by reading

`BOF3X_LAYERING_LOG=1` with a probe line for every field object of area 175
whose feet stand below its own map cell's lowest corner (`dolphinBoss.txt`,
en-US, shots every 105 frames from 960 to 2,850; `probe2.log`):

| Object | Cell | Feet | What else | Lines |
|---|---|---|---|---|
| `0x7DEE80` (`Sprite_Objects[0]`) | (45, 93), corners 4 4 4 4 | -1 | pose `+1` 4 (the movement script, `Field_ObjectUpdate`), **`+0x83` 1**, `+0x24` 0 (not a model) | 1,933, recipe frames ~952..2,884 |
| `0x7DF06C`, `0x7DF110`, `0x7DF1B4` | stair and slope cells (35..37, 19..29) | below by a step | `+0x83` 6, 7, 8 | 9..10 each, a walk past |

So the gold faerie is the one object of the area standing inside a raised
cell for the whole scene, and **it runs area 175's movement script 1**:
object `+0x83` is the index into the area descriptor's `+0x10` script table
that `Field_ObjectUpdate` steps ([`movement-script.md`](movement-script.md)
1b). That is the identity the rule tests - the area, a `Sprite_Objects`
entry, script 1 - never a screen position.

Two readings that did not serve: `+0x54` is not the placing entry here
(`Sprite_InitFromEntry` writes the area descriptor `+8` entry there, but
`Sprite_ScriptStart` rewrites it to the animation script's frame words:
`0x8B50DC`, `+0x50` + 12), and the purple faerie behind it (`0x7DEF24`,
cell (44, 92), feet 0) never matches - it stands on the floor, as the brief
has it, and is not moved.

The same probe in the original language (`--lang original`, the PC's Chinese
text): the same object, script 1, cell (45, 93), 2,001 frames - the
identity does not depend on the text block.

## 2. The rule, and the shadow it had to keep

`src/game/layering.cpp` (`TableFaerie`, `LayerFor`'s `clear_to`, `Defer`,
`AfterSprite`), with one call in `src/game/draw_pass.cpp` after a sprite is
drawn.

1. **Which sprite.** Area 175; an object of `Sprite_Objects`; `+0x83` = 1;
   its feet (`+0x3E >> 5`, the key's own measure) below the lowest corner of
   its own map cell (`+0x36`, `+0x3A`) - section 4's "it stands inside a
   raised cell".
2. **Only where DIV-0071 kept it.** DIV-0071 is judged first, as for any
   sprite; the faerie's rule runs only when that left it in its own layer.
3. **One layer later.** DIV-0071's whole test for the next layer, with one
   exemption: in its own layer's second list, the top or side of any cell
   whose lowest and highest corners equal its own cell's (the table's two
   planks, 4 4 4 4) stops nothing. Anything else there - a later sprite
   over its body, a table item that is not floor, the next layer's first
   list where it is not floor, a cell record within two cells, what a
   record drew over it, a frame node - still keeps it where it was.
4. **The shadow.** Drawn one layer later, the faerie was whole, but a black
   oval appeared on the right plank's front face (first attempt,
   `cmp_23_top.png`): the faerie's shadow, a piece of the same sprite
   primitive (code `0x84`, box (140, 87.5)-(179.5, 139.9) at frame 1,994,
   `BOF3X_DRAWORDER`), at its ground point on the floor inside the table. In
   Capcom's order the right plank's two side triangles (items 75 and 76,
   list 1 of layer 22) are drawn over it. So the side triangles of the table
   in the list it passed, those within reach of it (a 48 px wide box round
   its screen point), are **drawn again right after it**: a draw mode (page
   `0x95`, as `DrawLayer_Close` commits) and a copy of each 0x48-byte
   primitive, committed to slot 6. They are opaque quads, so they draw the
   same pixels they drew in the list; only the tops no longer cover the
   faerie.
5. **Nothing else under the copies.** If any other sprite is drawn into the
   layers between its own and the one it moves to and reaches the sides'
   box, the faerie stays (logged). With more than 8 such sides, or no room
   in the packet pool for the copies, it stays too.
6. **Log.** `BOF3X_LAYERING_LOG=1`: `D300 fix    frame N: the table faerie
   007DEE80 (area 175, script 1), cell 45,93 (corners 4..4), feet -1: layer
   22 drawn in layer 23; 2 table side(s) drawn again after it`, every pass it
   is judged.

`BOF3X_LAYERING=0` turns it off with DIV-0071 (`Defer` and `AfterSprite`
run only in mode 1); under `BOF3X_ORIGINAL` the pass is Capcom's and neither
is reached.

Moving the faerie further, as DIV-0071 does for floor, was the first
attempt's result (layer 22 to 25); the rule now stops at one layer, the
second list it stands in being the only thing it is lifted past.

## 3. Captures

`dolphinBoss.txt` (its save), en-US, point filter (`BOF3X_FILTER=point`),
x4, shots every 105 recipe frames from 960 to 2,850 (`recipe_shots.py`, 19
shots), the build of `main` (`31249d0c`, DIV-0071 on, its default) against
this branch, each from its own ini-less launcher copy (`run.py`):

| Width | Shots | Differ | Where |
|---|--:|--:|---|
| 4:3 (`--wide 0`) | 19 | 19 | only in the faerie's box: (312..361, 230..257) of 640 x 480 at 960..2,745; (446..497, 200..227) at 2,850, the camera moved |
| 16:9 (`--wide 1`) | 19 | 19 | the same boxes 106 px right ((418..467, 230..257); (552..603, 200..227)) |
| 4:3, `BOF3X_LAYERING=0` on both | 19 | **0** | identical: Capcom's order |

By eye (the owner's sheets `faerie_fix_w0.png` and `faerie_fix_w1.png` in
the harness root: whole frames and 3x crops of 1,065, 1,905, 2,010 and
2,850): the gold faerie's skirt whole over the right plank; the plank's
front faces and the floor round it as before, no shadow on the table; the
faerie landing beside it at 2,010 (its own shadow on the floor), the purple
faerie behind and the party unchanged. Besides the skirt, a sparkle of the
faerie's own sprite that the plank's top used to hide now shows.

## 4. The state hash and the self-tests

**Self-tests**, headless, the launcher copy with no ini
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, narrow): exit 0, `inject: 10087
ours, 0 left original`, no mismatch line other than 0 (1,088 lines naming
mismatches, every one 0); `draw_pass self-test: 3000 rounds ... 0
MISMATCHES` - `Sprite_DrawPass` being the one function touched outside
`layering.*`. The rule arms after the self-tests, as DIV-0071 does, so the
fuzz shows only that the pass around it is unchanged.

**The state hash**, the ten routes, `BOF3X_OPT=none`, exe host, FY's own
copies of the game folder (`chain.py`, `check.py`; the `.sh` files and the
reports in the harness root's `sh/` and `checks/`):

1. **Narrow, DIV-0071 on (the default), this branch against `main`'s build
   (`new0` against `base0`):** `menu_screens` identical; the others differ
   only on pages that also differ between two runs of `main`'s build - the
   start-up VRAM and sound pages at tick 3 (`0x7BB000..0x7BF000`,
   `0x7DE000`, `0x937000`), the sound page `0x7DE000`, Direct3D's state page
   `0x6C3000`. `combat`, the one route with more (VRAM, `0x937000`,
   `0x7C9000` / `0x7CA000` at ticks 471..477): `base0` against a second run
   of `main`'s build (`base0b`) differs on exactly those pages and ticks, and
   `check base0 base0b new0` is **identical on all 2,561 compared ticks**.
2. **Narrow, `BOF3X_LAYERING=0`, against the `cn_<route>_orig3` pairs
   (`off0`):** `combat` exit 0, identical, as `cn_combat_a1020` was; every
   other route reports the pages `a1020` reported
   (`bof3x-runs/away1020/check_*.txt`) and no other, apart from the tick-3
   start-up group, which comes and goes between runs. With DIV-0071 on,
   `main`'s own build already differs from the pairs (the draw list's order,
   `0x903000`; the packet pointer, `0x7E0000`): the references were recorded
   with `BOF3X_LAYERING=0`, so (1) is the comparison that tests this change.
3. **Wide, against `main`'s build at the same width** (`new1` against
   `base1`, `new2` against `base2`): identical on combat, cutsceneAndNue,
   field_menu, menu_screens and shop at both widths and dragonTransform at
   21:9; the rest only on the same noise pages (`0x6C3000`, `0x7DE000`, the
   start-up group at ticks 2..7). No game page and no `Rand` page differs.
   Wide against narrow differs as `main` does (DIV-0041's own pages); the
   comparison at equal width isolates this change.

Three runs lost a race in `recipe_saves.players_games` (a game just started
had not yet set its unattended event, so another run's slot-0 swap refused,
rc 1 in one second) and were run again alone.

## 5. Where the rule fires

`BOF3X_LAYERING_LOG=1` on every run of section 4's (1) and (3) with this
branch (thirty runs: the ten routes at three widths): **no `D300 fix` line
in any**. On `dolphinBoss`: 1,933 lines at 4:3 and at 16:9 (2,001 in the
original language), all for object `0x7DEE80` in area 175, cell (45, 93):
1,915 "layer 22 drawn in layer 23" and 18 "layer 20 drawn in layer 21"
(while the camera pans), each with "2 table side(s) drawn again after it" -
items 75 and 76. No "kept in layer" line: no other sprite came under the
sides. With `BOF3X_LAYERING=0`, no line at all.

## 6. For the other files

**A - `docs/DIVERGENCE.md`, a new entry** (the orchestrator numbers it):

### The table faerie in the faerie village is drawn over its table

- **ID:** DIV-NNNN (the orchestrator's)
- **Date:** 2026-10-21
- **Subsystem:** field draw order (`Sprite_DrawPass` `0x593060`, ours in
  `src/game/draw_pass.cpp`; the rule in `src/game/layering.cpp`, beside
  DIV-0071's)
- **Tier:** Intent - the owner's decision, 2026-10-15: a faerie the scene
  stands on the table is drawn on the table, not through it
- **Original behaviour:** in area 175 (the faerie village's wooden room
  after the Faerie Tiara; `dolphinBoss.txt` recipe frames about
  960..2,850) the gold-winged faerie at the front left of the two-plank
  table is placed at floor height inside the left plank's raised cell. Its
  layer is the right plank's row, and a raised cell's top is in its layer's
  second list, drawn after the layer's sprites, so the right plank's top
  cuts the faerie's skirt on a straight diagonal for the whole scene; the
  plank's side triangles hide the faerie's shadow at its floor point
  ([`faerie-over-table.md`](faerie-over-table.md) 1-3, known-defects D300).
  Capcom's: the same with `BOF3X_LAYERING=0` and with Capcom's code; the
  PlayStation's twins of the three deciding functions order it the same
  way; the placing data are the disc's; the owner's PlayStation look
  (2026-10-15) shows the same cut.
- **New behaviour:** that faerie alone - a field object of area 175 running
  the area's movement script 1 (object `+0x83`), whose feet stand below its
  own map cell's lowest corner - is drawn one layer later when DIV-0071 left
  it in its own layer, judged by DIV-0071's test with one exemption: the
  tops and sides of cells as high as its own in the second list it stands
  in. The side triangles of those cells within reach of it are drawn again
  right after it (copies of their primitives, after a draw mode as
  `DrawLayer_Close` commits), so they still hide its shadow; if any other
  sprite would come under those copies, the faerie stays where it was.
  `BOF3X_LAYERING_LOG=1` logs each pass it is judged (`D300 fix`).
- **Rationale:** the scene stands the faerie on the table; the cut comes
  from where its object is placed, not from anything meant to stand in
  front of it. Fixed for this scene only, on the owner's word. **To be
  broadened** to [`faerie-over-table.md`](faerie-over-table.md) section 4's
  general rule - any sprite standing inside a raised cell drawn after its
  layer's second list, where everything there that reaches it is a top or
  side of cells of its own height - if more examples of the class turn up;
  known-defects D242's gene crystal would be the second, the same class from
  the other side.
- **Also in the PSX version?** Yes, the same cut (above): a change to the
  original's picture, not a port repair.
- **Verification:** [`faerie-fix.md`](faerie-fix.md) 3-5: 19 captures at
  4:3 and 19 at 16:9 against `main`, differing only in the faerie's box; 19
  identical with `BOF3X_LAYERING=0`; the state hash on the ten routes,
  narrow and at both wide widths, equal to `main`'s build up to run-to-run
  noise, and narrow with the switch off as `cn_<route>_a1020` against the
  `orig3` pairs; the self-tests exit 0 with no mismatch; no `D300 fix` line
  on any of the ten routes. **Not seen in play.**
- **Reversible?** `BOF3X_LAYERING=0` (with DIV-0071); Capcom's order under
  `BOF3X_ORIGINAL` (the pass is Capcom's).

**B - `docs/known-defects.md`, D300's status:**

Replace the **Status** paragraph (and the title's "the owner's call" with
"fixed by DIV-NNNN"):

> **Status:** fixed by DIV-NNNN (2026-10-21, the wide-ring round's side
> group FY, [`faerie-fix.md`](faerie-fix.md)) for this scene only, on the
> owner's decision of 2026-10-15: area 175's table faerie (the object
> running the area's movement script 1) is drawn one layer later, past its
> table's tops, with the table's sides drawn again after it over its
> shadow. The DIV carries the note to broaden it to `faerie-over-table.md`
> 4's rule if more examples of the class turn up (D242 would be the
> second). Seen on captures; not yet in play.

**C - `docs/owner-review.md`:** the faerie scene with the fix, on the
captures of section 3 or in play (`dolphinBoss`, recipe frames 960..2,850):
the gold faerie whole over the table, no shadow on the plank's front.

**D - `docs/README.md`:** a row for this document.

## 7. Not done

- A census of other objects standing inside raised cells over the routes
  (the class of section 4 of `faerie-over-table.md`): the probe of section 1
  found three objects briefly below their cell on stairs and slopes in the
  same route, which the rule never touches. The broadening is the owner's
  call when a second example is seen (D242's gene crystal would be the class
  from the other side).
