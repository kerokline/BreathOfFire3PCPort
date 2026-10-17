# The button routes: the instrument, the site routes and the reference captures

**Status:** MEASURED (2026-10-17, group RC of the button unification round,
branch `buttons/rc` off `839a3e1c`). Built and run on today's code (before
SG merges): `sigma_swap.txt` on all four install kinds, four site routes and
their swapped twins on all four, the references recorded with the state hash.
The seven sites with no route today were reached as far as section 5 says:
**none of their minigames was started**. Nothing here is seen by the owner.

This is group RC of [`button-unification-round.md`](button-unification-round.md)
section 2: the routes every other group of the round cites as its acceptance
evidence, identity beside swapped. The method is that doc's section 1: every
site is a draw on a screen a recipe reaches, and every hard-coded test is a
press a recipe makes, so one capture with the Config untouched and one with
the Config swapped (Speak <-> Action, View <-> Change) settle each site.

Nothing of the game is in this document or in the tree: the saves, frames
and sheets live under the harness root `C:\Users\kerok\b_rc` (rule 1), and
scenes are described, not quoted.

## 1. How to run them

```
python tools/abc_run.py prepare --installs tools/recipes/sigma-installs.toml --set sigma --root C:/Users/kerok/b_rc
python tools/sigma_saves.py stock C:/Users/kerok/b_rc/saves            # the stock recipe saves, copied out
python tools/sigma_saves.py swap C:/Users/kerok/b_rc/saves/town.DAT C:/Users/kerok/b_rc/saves/town_sw.DAT   # likewise camping, fairyVillage
python tools/abc_run.py run --installs tools/recipes/sigma-installs.toml --set sigma --root C:/Users/kerok/b_rc \
    --route tools/recipes/sigma_worldmap.txt --tag id --wide 0 --statehash
python tools/abc_run.py run ... --route tools/recipes/sigma_worldmap_sw.txt --tag sw --wide 0
python tools/sigma_sheets.py --root C:/Users/kerok/b_rc                  # a contact sheet per site
python tools/sigma_routes.py check                                      # every _sw twin current
```

- **`tools/recipes/sigma-installs.toml`**: the four install kinds, as an
  installs file for `abc_run.py --installs` (`recipes/abc-installs.toml`'s
  format): `pc` the PC install with no language layer (a cache of the PC's
  `DAT\` and `BOF3.exe` alone, no `BOF3X_LANG`), `psx-us` the US disc alone,
  `psp-eu` the PSP-EU disc alone, `pc-enus` the PC with the en-US layer. Its
  `recipe_saves` is `{root}/saves`, so the crafted saves never enter the
  tree. Its `discs` is `{root}/discs`: the main checkout's `CDImage\` holds
  only `.zip` files since 2026-10-15, and `importer.py build` takes a `.cue`
  / `.iso` (section 7), so the USA and PSP-EU zips are unpacked there.
- **`--wide 0`** on every run: 320 x 240 at the window's 640 x 480, so
  every group's captures compare whatever its launcher ini says.
- **`tools/sigma_saves.py`**: `swap` (section 2), `jump` (the chapter-jump
  save: block bytes 0..15 = chapter, 0, state, ...; the `+0x70` checksum),
  `place` (the area word `+0x24` and the position `+0x28` / `+0x2C` set, the
  scenario bytes kept: section 5), `words`, `check`, `stock`. It refuses to
  write inside the checkout.
- **`tools/sigma_routes.py variant`** writes a route's swapped twin; `check`
  says whether every twin is current.
- **`tools/sigma_sheets.py`**: the contact sheets (section 6).

A route is a recorded-form recipe (wait / hold / one-frame shot lines,
`tools/recipe_shots.py`'s input and output) with two header lines: `# save
NAME` and `# sigma-from FRAME`, the first frame after the save's load. **Every
shot line was added by `recipe_shots.py`** (`--every 1 --from F --to F`, once
per shot), never by hand.

**The twin rule.** The twin loads `NAME_sw` and, from `sigma-from` on, presses
circle where the route pressed cross and back, L1 where it pressed R1 and back.
No frame moves; the shots keep their places. Before `sigma-from` the title
and the load menu run on no save's words, so nothing there is renamed; every
route confirms with **R2** there and in its menus wherever it could, because
the Config panel ORs L2 / R2 into Speak and into the confirm word whatever
the swap (`ConfigScreen_Controller`, `src/game/rest_4f.cpp`): R2 confirms on
both sides, before and after the round. Square (Move) and triangle (Menu)
are not swapped.

## 2. The instrument: `sigma_swap.txt`

The town save, `shop.txt`'s walk to the inn (its frames 0..2201), then the
field menu (triangle), right x5 to Config, R2, down x6 to Controller, R2;
**cross on the Speak line** (Speak takes cross, Action circle), down x4,
**L1 on the View line** (View takes L1, Change R1), Start to Quit, R2 to
close the Config, cross to close the menu; then `shop.txt`'s inn from its
frame 2201 (Rest, "Save?" Yes), down x7 to **slot 7**, R2, left to Yes on
the overwrite prompt, R2. 3,972 frames; shots at 2515 (the panel before the
swap), 2620 (after it) and 3810 ("save complete" over slot 7).

**Slot 7**: not 6 (`balioAndSunder_2` writes there, the owner's), not 4
(`bossAndFlash` loads it), not 0 (every recipe's save goes there). The slot
is the game folder's, the harness's copy, never the owner's `bof3\`.

**Checked**: the `BISLPS07.DAT` the game wrote on `pc-enus`, `psx-us` and
`psp-eu` holds, word for word, what `sigma_saves.py swap` writes from the
same save: Speak `0x43`, Move `0x80`, Menu `0x10`, View `0x4`, Change `0x8`,
select `0x100`, Action `0x20`, confirm `0x23`, cancel `0x40` (from `0x23 0x80
0x10 0x8 0x4 0x100 0x40 0x23 0x40`). So the `_sw` saves the twins load are
the game's own swap, made by the tool for each route's save (`town_sw`,
`camping_sw`, `fairyVillage_sw`) rather than by walking each save to an inn.
Confirm and cancel (words 7 and 8) do not move: today's code is
[`button-roles-by-release.md`](button-roles-by-release.md) 4's "confirm and
cancel ignore the Config".

**Two caveats.** The cross that closes the field menu is today's cancel; once
SG's words 7 / 8 follow Speak and Action that close is circle, and the route
needs that one press renamed (the R2s stay right). And on the `pc` install
(no language layer) the Controller panel **crashes** (section 7), so
`sigma_swap` runs on the other three kinds only.

## 3. The site routes

Frames are recipe frames; a shot is `f<frame>.png`, a timed frame
`t<frame>.png` (every 30). "Press" is what the route sends from `sigma-from`;
the twin sends the swapped button.

### 3.1 The world map's legend: `sigma_worldmap.txt` (town, from 300)

The owner's `worldMapAndAreaTransition.txt` to its frame 1440, with the field
menu it opened at 631..680 made a wait of the same length (that menu's close
is a cancel, which the twin could not press the same way on today's code; no
other frame moved). Out of the town onto the Yraall Region's map (area 33,
`WorldMap_DrawFrame`), a walk to the Cedar Woods plate, into the woods.

| Frame | Shows | Press before it |
|---|---|---|
| `f01230` | the legend at the top left (rows Speak / Action / Start), the Cedar Woods plate | the walk (square + directions) |
| `f01260` | the same, standing | - |
| `f01320` | the Guide row's message box | **cross at 1290**: word 6, Action (the twin: circle) |
| `t001380`..`t001440` | the message closed (cross at 1372), R1 at 1396, R2 into the woods at 1411 | |

Earlier presses: cross at 578 in the town (Action, word-read), R1 held with
directions 714..880 and L1 at 915 / 1014 (View / Change: the camera).

What the four kinds draw at `f01230` (identity): **PC** and **PC + en-US** the
port's keyboard art (`SPACE` / `X` / `ENTER` with pictograms), **PSX US** and
**PSP-EU** the disc's words with a circle and a cross (Enter, Guide) and
START (Camp) - D307's repaint, one icon a row.

### 3.2 The fishing: `sigma_fishing.txt` (camping, from 340)

The owner's `caughFish.txt` to its frame 1320, its three load-menu circles
(225, 283, 310) made R2. The area 16 map (`Area16_DrawFrame`, a legend copy),
the lake (area 30, mode 8).

| Frame | Shows | Press before it |
|---|---|---|
| `f00600` | the area 16 map's legend | the walk; circle at 720..733 enters the fishing spot (Speak on the map, word 0) |
| `f00845` | stage 1: the banner line starting to scroll, the panel | circle at 808 (READY away) |
| `f00870` | the gear menu (Gear / Data / Rule, the rod list) | **circle at 850**: stage 1's hard-coded test (`LeaderPanel_S1Buttons`) |
| `f00990` | the cast: CASTING POWER | circles 872..980 (the menus' confirm, word 7), cross 941 / 952 (cancel) |
| `f01140` | the reel's banner: "Wind reel..." with its circle label | the reel's circles from 1220 on follow |

The twin presses cross for every one of those circles. On today's code that is
the wrong button for stage 1 (hard-coded circle) and for the menus (word 7,
`0x23`), so the twin parts from the identity run at the first press after
the map (section 4) - what HT and SG change.

### 3.3 The Controller panel: `sigma_config.txt` (town, from 300)

Town save, the field menu, Config, Controller (as `sigma_swap`), the cursor
down each line, Start to Quit. No face button inside the panel, so the twin
presses exactly what the route does: it is the one route whose twin should
equal its identity run frame for frame except the panel's icons.

| Frame | Shows |
|---|---|
| `f00700` | the panel, cursor on Speak: Speak circle, Move square, Action cross, Menu triangle, View R1, Change L1 (identity) |
| `f00790` | cursor on View |
| `f00845` | cursor on Change |

On **PSP-EU** the panel's line names and the Config's row labels are blank
(D246, known); on **PC** the run crashes opening the panel (section 7).

### 3.4 The item lists' corner: `sigma_items.txt` (fairyVillage, from 369)

The fairyVillage save (53 items: the list has pages), loaded on the world map
as `fairyVillage.txt` does (its circles made R2), the field menu, Items, Use:
the list.

| Frame | Shows | Press before it |
|---|---|---|
| `f00565` | the list's first page, the L1 tile at the scroll bar's top and R1 at its foot | R2 at 527 (Use) |
| `f00605` | the second page | **R1 at 571** (the hard-coded `0x500C` test; the twin: L1) |
| `f00650` | the third page | R1 at 615 |
| `f00695` | the second page again | **L1 at 659** (the twin: R1) |
| `f00790` | the next page, the cursor three rows down | R1 at 751 |

On today's code the twin's L1 at 571 pages *back* (it wraps), so its pages
differ from the identity run's from 571 on: the hard-coded test HT changes.

### 3.5 What `sigma_swap.txt` shows

`f02515` the panel at identity, `f02620` the same panel after the two
presses (Speak cross, Action circle, View L1, Change R1), `f03810` the save
written to slot 7. On PSP-EU the line names are blank (D246); the icons swap
the same way.

## 4. The references (today's code, before SG)

Built from this branch's tree (`839a3e1c` plus these files: no `src/`
change), the root's launcher copy `C:\Users\kerok\b_rc\L`. Frames under
`C:\Users\kerok\b_rc\runs\<tag>_<route>\<install>\`; the state hash
(`--statehash`) as `run.sh` in each.

| Route | Tag | pc | psx-us | psp-eu | pc-enus |
|---|---|---|---|---|---|
| `sigma_worldmap` | `id`, `id2` | done | done | done | done |
| `sigma_fishing` | `id`, `id2` | done | done | done | done |
| `sigma_config` | `id`, `id2` | **crash** at the panel | done | done (D246 blank) | done |
| `sigma_items` | `id`, `id2` | done | done | done | done |
| `sigma_swap` | `id` | **crash** at the panel | done, slot 7 | done, slot 7 | done, slot 7 |
| the four `_sw` twins | `sw` | see 4.1 | | | |

**The hash routes** are the four site routes: `id` and `id2` are two runs
each, so `python tools/statehash.py check <root>/runs/id_<r>/<i>/run.sh
<root>/runs/id2_<r>/<i>/run.sh NEW.sh` is the regression question for any
later build (`state-hash.md`). REPLACE-4.1

## 5. The sites with no route today

The method that reaches each area: **`sigma_saves.py place`** - a save of
the nearest chapter with the area word and the position set, the scenario
bytes kept. The position is a valid entry: the destination of an area link
into that area (descriptor `+0x20`, 12-byte links, [`area-entry.md`](area-entry.md)
`Area_LinkAt`), read from `BOF3.exe` (`0x667590`'s descriptors). Every one
loaded straight into its area on `pc-enus` (`x_look_look_*`), the party
standing at the link's spot. **None of the seven minigames was started**:
each begins from a person or object of its area under the story's flags, and
finding which, where, in which chapter, is a walk per site that this round
did not finish. REPLACE-5

| Site | Area | Save placed (`saves\`) | Entry used | Reached |
|---|---|---|---|---|
| the chop | 8 | `chop8` (town, chapter 1) | area 9's link 0 | the area, at the woods' edge by a signpost; REPLACE-CHOP |
| the well | 68 | `well68` (stallion, chapter 6) | area 68's own link 0 | the area: a shop interior with a counter; REPLACE-WELL |
| the rope pull | 75 | `rope75` (preBeyd, chapter 6) | area 86's link 0, which lands inside `Area75_StepHook`'s zone | the area; the step hook's event did not start (its flag, Cond row 10 flag 2, is presumably set by this point of the story) |
| the antenna | 111 | `tower112` (preBeyd) into area 112 | area 86's link 3 | REPLACE-TOWER |
| the ram | 121 | `ram121` (preBeyd, chapter 6) | area 94's link 2 | the Middle Sea map with the ship (its ENGINE gauge); no Black Ship to ram |
| the dock | 128 | `dock128` (identify, chapter 9) | area 128's own link 1 | the area: a room with a ladder; the parts menu not opened |
| the crane | 135 | `crane135` (identify, chapter 9) | area 126's link 1 | the area: a deck with stairs; the crane not found |
| the name entry | - | - | - | **not on the PC**: the port cut it ([`name-entry-restoration.md`](name-entry-restoration.md) 1: `TitleFlow_NewGame` goes straight on; the faerie renamer's input step is `BareRetZero`). Its route comes with IDEAS I34's restoration |

## 6. The contact sheets

`python tools/sigma_sheets.py --root C:/Users/kerok/b_rc` writes
`C:\Users\kerok\b_rc\sheets\<site>.png`: a row per install kind (PC, PSX US,
PSP-EU, PC + en-US), for each frame below the identity run beside the
swapped twin, cropped to the site. The table it reads:

<!-- sites -->
```
worldmap  sigma_worldmap  f01230  56,12,132,56   legend
worldmap  sigma_worldmap  f01320  -              Action pressed
fishing   sigma_fishing   f00600  56,12,132,56   area 16 legend
fishing   sigma_fishing   f00845  -              stage 1
fishing   sigma_fishing   f00870  -              stage 1 press
fishing   sigma_fishing   f01140  140,14,320,40  reel banner
config    sigma_config    f00700  160,95,275,215 panel, Speak
config    sigma_config    f00845  160,95,275,215 panel, Change
items     sigma_items     f00565  0,50,175,225   list
items     sigma_items     f00605  0,50,175,225   after R1
items     sigma_items     f00695  0,50,175,225   after L1
```
<!-- sites -->

REPLACE-6

## 7. Found on the way

- **The PC install with no language layer crashes opening the Controller
  panel** (a new defect for the coordinator to number; not fixed here, no
  `src/` in this group). `CRASH 0: exception 0xC0000096` in `Text_DrawString`
  (bof3x.dll `+0x3514A`) <- `Text_DrawAt` <- `Config_DrawControllerCell` <-
  `Config_DrawControllerPanel` <- `ConfigScreen_Controller`, `ecx 0xA65`:
  Capcom's glyph trap, a glyph index past the Chinese font's (the en-US
  layer's icon glyphs sit at `0x9E3..`, which this install lacks). Measured
  three ways on 2026-10-17: the title's `config_controller.txt` under the
  harness (`BOF3X_CACHE` the PC-only cache), the same with no cache, and the
  same with no `BOF3X_*` variable at all; the field menu's Config the same.
  Under the en-US layer and on both disc-only installs the panel draws.
  So `sigma_config` and `sigma_swap` have no PC row until it is fixed.
- On the `pc` install the item list's party names (a save made under a Latin
  layer) draw as stray glyphs: the save's names in a font that has no Latin
  letters. Not a site; noted.
- **The harness gaps**: (1) `CDImage\` holds zips only, and `abc_run.py
  prepare` passes the installs file's `build` sources straight to
  `importer.py build`, which identifies a `.cue` / `.iso` / folder and refuses
  a `.zip` ("not a build fixtures.toml catalogues whole"); worked around by
  unpacking into the root and pointing `[paths] discs` there. (2) The
  "one game at a time" wait: the other groups' headless self-tests run as
  `BOF3.exe` processes for many minutes; a wait on every `BOF3.exe` starved,
  so this group's runner waits only for a game with a window (the
  self-tests have none). (3) `abc_run.py`'s cache stamp hashes every
  `tools/*.py`, so adding `tools/sigma_*.py` after `prepare` makes the next
  `prepare` rebuild every cache (about 15 minutes for the four); `run` does
  not check, so the references were run without one.

## 8. For the other files

`docs/README.md`: one row, added with this doc.
