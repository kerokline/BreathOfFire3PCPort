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
swap), 2620 (after it) and 3810 (the save's confirmation over slot 7).

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
| `f01140` | the reel's banner: the reel line with its circle label | the reel's circles from 1220 on follow |

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

On today's code the twin's L1 at 571 and 615 leave the first page as it is
(no page before it), and its R1 at 659 goes to the second: its pages differ
from the identity run's from 571 on - the hard-coded test HT changes.

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
| the four `_sw` twins | `sw` | done (config: crash) | done | done | done |

**Two identity runs agree picture for picture**: `id` against `id2`, every
frame (timed and shot) of every route on every install byte-equal - 51 / 51
worldmap, 49 / 49 fishing, 32 / 32 config (22 / 22 up to PC's crash), 32 / 32
items. So the identity check a later group makes is "every frame equal".

**The hash routes** are the four site routes: `id` and `id2` are two runs
each, so `python tools/statehash.py check <root>/runs/id_<r>/<i>/run.sh
<root>/runs/id2_<r>/<i>/run.sh NEW.sh` is the regression question for any
later build ([`state-hash.md`](state-hash.md)). They were recorded **without**
`--statehash-skip`, so 30..40 pages differ between `id` and `id2` from tick 0
(the window, DirectDraw / Direct3D, the sound and music staging pages, the
texture cache: the platform pages `state-hash.md` calls noise); `check` reads
those as noise where the two references disagree. A later group's NEW run
should be made the same way (no skip list) for the comparison to hold.

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
did not finish.

The saves and the exploration drafts stay in the root (`saves\`, `drafts\`;
frames in `runs\x_look_*`, `runs\x_ex_*`). The next step is the same for
every site: from its placed save, find the person or object that starts it
(`tools/event_scan.py --area N` lists the area's placements; their positions
and the story flags gating them are not decoded here), then record the start
as a route. The owner, who knows where each minigame is, could record each
with `BOF3X_RECORD` from these saves in a minute or two.

| Site | Area | Save placed (`saves\`) | Entry used | Reached |
|---|---|---|---|---|
| the chop | 8 | `chop8` (town, chapter 1) | area 9's link 0 | the area, at the woods' edge by a signpost; a walk up the path with talk presses (`ex_chop`, 2,733 frames) met no one, and a walk in from the world map met a random battle; the woodcutting not found |
| the well | 68 | `well68` (stallion, chapter 6) | area 68's own link 0 | the area: a shop interior with a counter; a walk round it with talk presses (`ex_well`) opened a customer's rumour, no well |
| the rope pull | 75 | `rope75` (preBeyd, chapter 6) | area 86's link 0, which lands inside `Area75_StepHook`'s zone | the area; the step hook's event did not start (its flag, Cond row 10 flag 2, is presumably set by this point of the story) **Recorded by the owner 2026-10-30: `tools/recipes/ropeMiniGame.txt`** (save `ropeMiniGame`, en-US, 10,522 frames: the title, the load, the walk to the foreman, the pull from about frame 1,500 to 4,500 with the two distance counters, the fight that follows, the foreman's thanks, F9 twice to quit). Replayed the same morning at x8 with the owner's `game\bof3x.ini` (cheats, CRT, wide 1) and with no ini: 175 shots a second apart, every pair alike but the CRT and the enemy-information gauge - the pull and the fight do not depend on the launcher's settings (`C:\Users\kerok\bof3x-runs\rope1030`). |
| the lighthouse valve | 77 | - (not one of the seven; found 2026-10-30) | - | **Recorded by the owner 2026-10-30: `tools/recipes/lighthousePowerMiniGame.txt`** (save `lighthousePowerMiniGame`, en-US, 1,172 frames: the title, the load, the ore into the boiler, the valve switch, the gauge from about frame 660, one press at 817, the valve open, F9 twice). Effect kind `0x45` reads the confirm and cancel button sets, so the Config swap never touched it ([`cheats.md`](cheats.md) 7). |
| the antenna | 111 | `tower112` (preBeyd) into area 112 | area 86's link 3 | area 112: a metal room with a ladder; not climbed to 111 |
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

What the four sheets show on today's code (`C:\Users\kerok\b_rc\sheets\`):

- **worldmap**: on PSX US and PSP-EU the legend already follows the words:
  the twin's Enter row shows a cross and its Guide row a circle, and the
  twin's circle at 1290 opens the same Guide message as the identity's cross
  (the map reads words 0 and 6). On PC and PC + en-US the port's keyboard
  art is the same on both sides (IC-W's site).
- **fishing**: the area 16 legend follows the words on the disc installs, as
  above. The stage-1 press does not: the identity's circle at 850 opens the
  gear menu, the twin's cross at 850 opens the quit prompt (stage 1's
  hard-coded test, HT's site), so the twin's later frames are another scene.
- **config**: the panel shows the swapped buttons on every install that draws
  it (Speak cross, Action circle, View L1, Change R1); PC: no frame (the
  crash, section 7); PSP-EU: icons with no names (D246).
- **items**: the corner tiles are the same on both sides (baked art, IC-L's
  site); the identity's R1 pages forward, the twin's L1 on the first page does
  not move, so the twin is behind until its R1 at 659 (HT's site).

## 7. Found on the way

- **The PC install with no language layer crashes opening the Controller
  panel** (numbered D312, fixed in the round; not fixed here, no
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
  letters. Not a site, and not a defect: DIV-0005's known and accepted
  case (names stored under one language drawn under another).
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

## 9. The round's validation (2026-10-17 evening, `buttons/round` `baff5e45`)

Every group merged (SG, DT-1, IC-F, IC-D, DT-2, HT, HT-2, IC-L, IC-W with
dimming, RC, D312). Built in this worktree; `prepare` rebuilt all four caches
(the importer and `loc_build.py` changed) and reinstalled both disc-only
installs. Runs tagged `r_id` (identity) and `r_sw` (twins), `--statehash`,
against section 4's `id` / `id2`.

**The two disc-only installs do not start.** Every run on `psx-us` and
`psp-eu` ends in its first second: `FATAL: ButtonText: 0x461C00's stand-in 0
(0x0066A2DC) is not a one-byte stand-in` - `ButtonText_Arm`
(`src/game/text_draw.cpp`, DT-2) checks that the six strings `0x66A2FC`
points at are the PC's one-byte stand-ins, and a disc-built image's bytes
there are not (they are no gap the image logs, so the image holds something
else at `0x66A2DC`). A regression of the round on every disc-only install;
no disc row below could be measured.

**Identity, `r_id` against `id`** (frames byte-equal / compared):

| Route | pc | pc-enus | psx-us, psp-eu |
|---|---|---|---|
| `sigma_worldmap` | 36 / 51 | 36 / 51 | FATAL at start |
| `sigma_fishing` | 36 / 49 | 36 / 49 | FATAL |
| `sigma_config` | 22 / 22 (the reference crashed there; the round runs on, 32 frames) | 32 / 32 | FATAL |
| `sigma_items` | 29 / 32 | 29 / 32 | FATAL |
| `sigma_swap` | 82 / 82 (as above; runs to its end) | 131 / 135 | FATAL |

Every differing frame of the four site routes differs only in the world
map legend's key column (game pixels `66,24-90,49`, on the three maps the
routes cross): **IC-W, intended** - the port's `SPACE` / `X` / `ENTER` gone;
on PC the stand-ins are the Chinese font's circled numerals in their colours
with `START`, under en-US the PlayStation shapes with `START`. The pictograms
right of the column are unchanged. `sigma_swap`'s four on `pc-enus`: three
are the save list's slot 7, which held the reference run's save and was
empty this time (the harness's own folder, not the game); one, `t003450`, is
the inn's morning fade one step brighter (mean difference 3 of 255, at most
16) with the next frame equal again - a fade phase, not a glyph (the
reference run's, not the round's: section 9.2). No other difference.

**`sigma_swap`'s close press desynced, as section 2 warned**: since words 7
/ 8 follow the Config, cross after the swap is confirm and reopened the
Config, and the route then wandered in it and wrote nothing. The press at
frame 2721 is now circle (no frame moved). After it, on `pc` and `pc-enus`,
slot 7 holds Speak `0x43`, Action `0x20`, View `0x4`, Change `0x8`, **confirm
`0x43`, cancel `0x20`** - DIV-0133. `sigma_saves.py swap` now writes words 7 /
8 the same way (`--before-round` for the old rebind); the three `_sw` saves
were rewritten. The R2 confirms of every route were fine.

**The state hash** (`statehash.py check id id2 r_id`, pc and pc-enus; the
platform pages the two references never agree on are skipped by `check`):
`config` on pc identical on every compared tick. Elsewhere the differing
pages are the VRAM shadow, the texture cache, the packet pools and the music
staging, plus, only while a PC-only world map is up (worldmap 1098..1408,
fishing's area 16 395..766, items' area 65 298..415), `0x7E0000`,
`0x7E1000`, `0x903000` and `0x929000` (pages that also hold draw state) -
IC-W's new legend draw; and on pc-enus's fishing from tick 768, `0x803000` /
`0x804000` (the window records and message pools: the banner's labels
resolved through `σ`, IC-F / DT-2). No page outside draws and text moved on
a route whose frames are equal.

**Swapped, `r_sw` against `r_id`**: every twin now stays in step with its
identity run, and every differing frame differs only where a glyph is:

| Route | pc | pc-enus | Where they differ |
|---|---|---|---|
| `sigma_worldmap` | 36 / 51 | 36 / 51 | the legend's key column (rows 1 and 2 trade) |
| `sigma_fishing` | 27 / 49 | 29 / 49 | the area 16 legend; the reel banner's label (circle becomes cross; on PC circled 1 becomes circled 2) as it scrolls |
| `sigma_config` | 24 / 32 | 24 / 32 | the panel's icon column (on PC also the keyboard letter column D312 draws) |
| `sigma_items` | 12 / 32 | 12 / 32 | the L1 / R1 tiles at the scroll bar (`161,72-174,220`); the world map legend before the menu |

- **The fishing's stage-1 press**: the twin's cross at 850 now opens the gear
  menu, as the identity's circle does (before the round it opened the quit
  prompt). The frames after it are equal but for the banner label.
- **L1 pages the item list after the swap**: the twin's L1 at 571 goes to the
  second page as the identity's R1 does, and its pages equal the identity's at
  every shot.
- **The glyphs trade**: the legend's rows 1 and 2, the reel label, the
  Config panel's icons, the list corner's tiles.
- PSP-EU's corner (IC-L: `L` / `R` becoming `L1` / `R1`) and the PSX US
  rows could not be seen (the FATAL above).

Sheets: `C:\Users\kerok\b_rc\sheets_round\<site>.png` (identity beside
swapped on the round) and `C:\Users\kerok\b_rc\sheets_ref_vs_round\<site>.png`
(section 4's reference beside the round's identity run).

### 9.1 The disc-only installs, on `1a87a000`

`1a87a000` makes the missing port stand-ins fatal only with no layer; a
disc-built image logs that it holds none and goes on. Rebuilt and re-run on
`psx-us` and `psp-eu` (`prepare` rebuilt the caches again, since the stamp
hashes `tools/*.py`). Spot check on `pc-enus`: `sigma_items` 32 / 32 equal to
its `baff5e45` run.

**Identity, `r_id` against `id`:**

| Route | psx-us | psp-eu |
|---|---|---|
| `sigma_worldmap` | 51 / 51 | 51 / 51 |
| `sigma_fishing` | 42 / 49 | 42 / 49 |
| `sigma_config` | 32 / 32 | 32 / 32 |
| `sigma_items` | 32 / 32 | 15 / 32 |
| `sigma_swap` | 135 / 135 | 122 / 135 |

- The disc legends are unchanged at the identity (D307's repaint, as
  expected).
- `sigma_items` on PSP-EU: the scroll bar's tiles, `L` / `R` before, `L1` /
  `R1` now (`161,72-174,220`), in every list frame: **IC-L, intended**.
  `sigma_swap` on PSP-EU: twelve of its thirteen are the same tiles in the
  town shop's list (`219,72-232,220`): intended.
- `sigma_fishing` on both: the reel banner's label (seven frames as it
  scrolls) is a **red circle where the disc's own line drew a white one** -
  the same button, in the PC's coloured icon, not the disc text's glyph: the
  label now resolved through `σ` (IC-F / DT-2). Not on the expected list:
  **the owner's or IC-F's call** whether the disc install should keep the
  disc's white glyph at the identity.
- `sigma_swap` on PSP-EU `t000330` (and on pc-enus `t003450`, section 9):
  a fade frame one step brighter (at most 16 of 255), the next frame equal.
  A second round run (`r_id2`) equals the first in all 135 frames, so it is
  the round's, reproducible, not noise. On a fade only, on this route only (the
  other routes load the same saves with equal frames); **unexplained** -
  a candidate for whoever owns the load path (SG's `LoadMenu_Apply` writer?).
  **Explained in 9.2: the reference run's, not the round's** - a late pass
  the capture did not cover (PSP-EU) and the inn's real-time jingle wait
  (pc-enus).
- Slot 7 on both: confirm `0x43`, cancel `0x20`, as on the PC.

**State hash** (`check id id2 r_id`, the platform pages left aside): on every
route `0x903000` (the nine pad words' page) differs from tick 1 until the
save's load (ticks 291..343) - **the starting Config by source** (SG's DIV-0134: a disc install now boots with its release's
words), intended, and the R2
confirms kept every route in step through it. After the load: fishing's
`0x803000` / `0x804000` from 768 (the banner's labels, as on pc-enus); items
on PSP-EU the packet pools from 470 and `0x903000` to the end (the corner's
draw, IC-L; the word page's later difference not decoded). Nothing else.

**Swapped, `r_sw` against `r_id`** - every difference a glyph, every twin in
step:

| Route | psx-us | psp-eu | Where |
|---|---|---|---|
| `sigma_worldmap` | 36 / 51 | 36 / 51 | the legend's rows 1 and 2 trade |
| `sigma_fishing` | 29 / 49 | 29 / 49 | the area 16 legend; the reel label (circle to cross) |
| `sigma_config` | 24 / 32 | 24 / 32 | the panel's icon column |
| `sigma_items` | 12 / 32 | 12 / 32 | the list corner's tiles; the legend before the menu |

The fishing's swapped cross at 850 opens the gear menu on both, L1 pages the
item list after the swap on both, and the glyphs trade on all four installs.

### 9.2 The fade step on `sigma_swap` (2026-10-17 night, `buttons/fade-step` off `43a72a16`)

The one-frame fade differences of 9 and 9.1 are **the reference run's, not
the round's**, and they have two different causes. Root `C:\Users\kerok\b_fade`
(its own `prepare`, psp-eu and pc-enus); its runs under
`runs\<tag>_sigma_swap\`, RC's under `b_rc\runs\`. Mean brightness of the
frame, 0..255:

| Frame | RC `id` (the reference, `0538b7dc`) | pre-round reruns | round runs |
|---|---|---|---|
| psp-eu `t000330` | 12.43 | 18.61, 18.61 (`f_base`, `f_base2`: the reference's own commit, rebuilt here) | 18.61 x 4 (`r_id`, `r_id2`, `f_id`, `f_sp4`) |
| pc-enus `t003450` | 45.44 | 42.39 (RC's `x_sigswap`, `839a3e1c`, the old recipe) | 42.39 x 2 (`r_id`, `f_id`) |

So no switch can explain either: the pre-round build itself draws the
round's picture, and a bisect by `BOF3X_ORIGINAL` switches had nothing to
find. Nor does the recipe change at frame 2721: `x_sigswap` pressed cross
there, as the reference did. The two frames:

**psp-eu `t000330`, the load's fade-in: a late pass the capture did not
cover.** Game logic is the same in both runs: `statehash.py` between `id`
and `r_id` finds, from tick 318 (the end of the boot-time `0x903000`
difference, 9.1) to 345, only tick 323 (the VRAM shadow's CLUT-strip rows,
pages `0x7BF000..0x7C2000`, and `0x7E1000`) and tick 328 (the packet pool
page `0x7F1000`), one tick each. The reference's log alone has `DIV-0004: 1
uploads left by an unrendered frame` at 323, and the window icon found at
`Frame_Counter` 165 rather than 164: its loop left passes undrawn, late
against the wall-clock deadline (`WinMain` draws a frame only when it is
early, as Capcom's did), around the area's file loads. The capture
(`BOF3X_SHOT_EVERY`) forced only the one pass before the frame it writes.
An experiment build (not committed: one chosen pass left undrawn) on a
420-frame cut of the route:

| Pass left undrawn (`Frame_Counter`) | `t000330` against the round's |
|---|---|
| 320 .. 325 | equal |
| 326 | 163 pixels, by at most 2 |
| 327 (the pass before the forced one) | **the reference's frame, pixel for pixel** (mean 6.18 darker, at most 32) |
| 328 (the forced pass itself, overridden) | one fade frame darker (mean 3.09, at most 16) |

With every frame captured (`--every 1`) and 327 undrawn, `t000329` and
`t000330` are both `t000328`'s picture, and `t000331` is right again. The
fade-in brightens by one level a frame (mean +3.1), so the reference's
capture, two frames old, was two levels darker and the next capture equal.
Only this route: a capture has to land inside the 16-frame fade with an
undrawn pass just before it; loads are where passes go late, and of RC's
runs only this one logged one there.

**pc-enus `t003450`, the inn's morning fade: Capcom's real-time wait.** Here
game logic does differ. `InnNight_Wait` (`0x57FC70`) counts `0x929F04` down
from `0x96`, then waits for `Sound_StreamDone` - the inn jingle's DirectSound
buffer stopping, which is wall-clock time - before `Transition_Start(1)`,
the morning fade. The task page `0x66C000` (`Task_Records`) changes at ticks
3432 / 3447 in the reference, 3433 / 3448 in ten hashed runs of both
installs, and 3434 / 3449 in three (`f_sp4`, and `f_fix` on psp-eu and
pc-enus; `f_fix2` and `f_fix3`, the same build, are back at 3433): the fade
starts a frame earlier or later, `t003450` is a level brighter or darker,
and `t003480` (the fade done) is equal. The capture's clock pause
(`GameClock_Pause` while a frame is written) stops the game and not the
stream, one way the jingle's end can move by a frame. Not a capture defect
and not ours to change; any route with a capture on the inn's morning fade
can show it. The reference's log also holds a 3-second shot freeze
(`f02515`: no release from the driver); that is not the cause: a run with
every shot timing out (`f_frz`) keeps 3433.

**The fix, for the psp-eu half.** `InputScript_ShotPending`
(`src/hook/input_script.cpp`) now forces the three passes before each
`BOF3X_SHOT_EVERY` frame (`kShotLeadPasses`), and the two before a recipe
`shot` step besides the step itself, so a late pass there no longer changes
the capture. Instrumentation only: game logic, inputs and frames are
untouched (the state hash of `f_fix` against `f_id`, the same tree before
the fix, differs only in the platform pages, the slot 7 directory and the
inn's jingle tick above). Checked:

- the experiment build with the fix: a pass left late at any of 320 .. 328
  gives `t000330` equal to the round's, all nine (`efix`, `efix3`);
- the whole route with the fix (`f_fix`): psp-eu and pc-enus 131 / 135
  equal to `r_id` and to `f_id`, `t000330` among them; of the four, three
  are the save list's slot 7 (this root's slot 7 held an earlier run's
  save) and one is `t003450` at tick 3434 (above);
- the self-tests: `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, an ini-less
  launcher copy, `--game bof3 --no-config`: no FATAL, every MISMATCH count
  0, "self-test only: done".

[`abc-harness.md`](abc-harness.md) section 3 says the same.
