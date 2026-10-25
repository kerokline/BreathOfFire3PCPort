# The A/B/C install harness: the same route, three installs, compared by picture

**Status:** MEASURED (2026-10-12, the PSP-only round's group AB, branch
`psp/abc-harness`); the comparison tested offline on earlier frames and
synthetic ones (section 7), then live on the eu triple: the attract
sequence, combat before and after VF's data fixes, cutsceneAndNue, shop
(section 8). Not seen by the owner. Amended 2026-10-13 (`away/1013-tooling`):
the save every side loads is decided before the first run (section 3a), and
`prepare` names a process holding the launcher copy (section 2).

The owner, 2026-10-12: "we should probably set up a harness to a/b/c test
the full install / psx only / psp only recipes, it would quickly reveal
things that aren't matching in a way that frames / call execs might not" -
and, of what to compare: "screenshot compare I mean". The faults seen in the
disc-only installs that day (combatants shifted left in battle by a zeroed
formation table, flame icons in the battle equip screen, the empty save
slots' text offset) were all visible and none was caught: the state hash
and the call traces check that our code computes the same thing, and miss
the places where the **data** differs while the code runs the same. So this
harness plays one route on each install and compares what the game drew.

## 1. Commands

```
python tools/abc_run.py prepare --set eu          # caches, installs, the launcher copy (idempotent, offline)
python tools/abc_run.py run --set eu --route attract
python tools/abc_run.py run --set eu --route combat           # a recipe: a name in tools/recipes, or a path
python tools/abc_run.py compare --set eu --route combat       # again, from the frames on disk
python tools/abc_run.py all --set us --route attract          # prepare, run, compare
python tools/abc_compare.py --side A=DIR --side B=DIR [--side C=DIR] --out DIR   # any folders of frames
```

Options: `--root` (default `~/abc` or `BOF3X_ABC_ROOT`; keep it short, MCI
fails on a movie path near 160 characters), `--every N` (a frame every N
recipe frames, default 30: one a second of game time), `--frames` (the
attract route's length, default 11,000), `--only a,b` (installs instead of
the set's), `--statehash` (section 6), `--speed` (`BOF3X_SPEED`, default 1),
`--tol` / `--min-pixels` (section 4), `--build` (where `prepare` takes the
launcher, DLL and exe; default this checkout's `build/`), `--save NAME` /
`--no-save` (the save every side loads, section 3a).

What lands under the root: `c_*` caches, `g_*` game folders, `L\` the
ini-less launcher, `logs\` the importer's output, `runs\<set>_<route>\<install>\`
(the frames, `bof3x.log`, `meta.json`, `_run.txt`), `runs\<set>_<route>\compare\`
(`index.html`, `sheet_NN.png`, `heat\<pair>\<frame>.png`, `result.json`),
and `report_<set>_<route>.md`. All of it is game-derived and stays out of
the tree (rule 1).

### 1a. One root per session (the owner, 2026-10-16)

Concurrent sessions sharing one root collided three times on 2026-10-16: a
`prepare` run with main's tools rebuilt `c_psxus` and reinstalled `g_psxus`
under a branch whose importer made more (the D255 jingles); another
session's runs read that cache half-built; and the shared `L\` was
overwritten with another build's DLL. Hence the rules:

- **Each session makes its own root** - `--root C:/Users/kerok/<short-name>`,
  named for the session or branch - and runs `prepare` into it with **its
  own checkout's** `tools/` and `build/`. About 6 minutes and 0.8 GB a
  disc-only install (348 s for PSX-US on 2026-10-16: 423 MB cache, 400 MB
  game); short names, for MCI's path limit.
- **Never `prepare` into a root another session made**, and never replace
  its `L\` or its caches. Reading another root's frames and logs (`runs\`)
  is fine; running a game on its install is not.
- **One game at a time on the machine**, whatever the root: wait for
  `bof3x.exe` and `BOF3.exe` to exit (other sessions' harness runs count).
- A root is scratch: delete it when the branch has merged.

## 2. The installs: `recipes/abc-installs.toml`

Each `[[install]]` names a game folder, a cache, a language, traits and any
environment; a `[set.NAME]` lists installs in column order. Shipped:

| Set | A | B | C |
|---|---|---|---|
| `eu` (default) | `pc-en150`: a copy of the PC install, the cache's `en-150` layer (cache from the PC's `DAT\`, `BOF3.exe` and the European disc) | `psx-eu`: the European PSX disc alone, `install --base` | `psp-eu`: the PSP-EU disc alone, `install --base` |
| `us` | `pc-enus`: the PC copy with an `en-US` cache (PC + US disc) | `psx-us`: the US disc alone | - |
| `us-disc` | `pc-enus` | `psx-us-psp`: the US disc as base with the PSP-EU disc after it, the players' shape (issues 130, 134, 139; [`door-sweep.md`](door-sweep.md) 2) | - |
| `jp` | `pc-jajp`: the PC copy with a `ja-JP` cache (PC + the Japan disc) | `psx-jp`: the Japan disc alone | `psp-jp`: the PSP-JP disc alone ([`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md)) |

Kinds: `pc` copies the PC install (`BOF3.exe`, `DAT\`, `SND\`, `BGM\`, the
movies and the small root files - never a save, never `BOF3.CFG`, which is
copied separately) and reads its language from the cache; `base` builds the
cache and runs `importer.py install --base --lang`; `use` takes an existing
folder as it is. A cache with no `build` must already exist (so the owner's
own caches can be named). `prepare` is idempotent: a stamp in each cache
(`.abc_build.json`, the sources and layers) and game folder
(`.abc_install.json`, the cache's manifest hash) says whether it is current;
it rebuilds only what changed, and refuses to replace a folder it did not
make. It copies the same `BOF3.CFG` (`[defaults] cfg`, the PC install's) into
every game folder: without it a disc-only install starts on the config's
defaults and differs at tick 0 (`disc-only-install.md` 5).

Before it refreshes the launcher copy `<root>\L`, `prepare` asks which
processes run from it (`users_of`: a `BOF3` / `bof3x` / `bof3x-launcher`
process whose exe is under `L\`, or whose `bof3x.dll` is `L\`'s) and refuses
with their pids and paths. Before 2026-10-13 it copied anyway and stopped
part way with WinError 32 when another harness's game held the DLL, leaving
the copy half refreshed. Tested on a stand-in (a copy of `PING.EXE` named
`bof3x.exe`, running from a scratch `L` folder): refused with its pid; the
same folder name plus one letter found nothing (the prefix test is by whole
folder).

## 3. A run

Sequential, one game at a time, each through `tools/input_run.py` with
`--no-front` (the game writes its frames and runs unfocused, DIV-0033) and
`--slot0-hold` (a recipe's save stays in slot 0 for the whole run, so a save
menu lists the same file on every side). Every run gets `BOF3X_OPT=none`,
`BOF3X_LAYERING=0`, `BOF3X_CACHE` = the install's cache, `BOF3X_EXEIMAGE` =
the cache for a disc-only install, `BOF3X_RECIPE_SAVES` = the main
checkout's `tools/recipe_saves`, the install's `BOF3X_LANG`, and nothing
else of the caller's `BOF3X_*` environment. The attract route is a recipe
written into the run folder: `wait N`, no input.

### 3a. The save every side loads

`input_run.py` puts a recipe's save in slot 0 when the recipe names it in a
`# save NAME` header; a recipe without one loads whatever each install's own
slot 0 holds, and the sides can start from different saves (the 2026-10-13
sweep's `fairyVillage`, [`abc-sweep-1012.md`](abc-sweep-1012.md) 7). Since
2026-10-13 `run` and `all` decide the save once, before `prepare` and before
the first side:

- the recipe has a header: as before - the header's save, checked to exist
  in `BOF3X_RECIPE_SAVES` (the main checkout's `tools/recipe_saves`) or this
  checkout's;
- `--save NAME`: that save on every side, passed on as `input_run.py --save`
  (it overrides a header);
- `--no-save`: the recipe loads no save (title-only, NEW GAME); each
  install's slot 0 is left alone (`input_run.py --no-save`);
- none of these: refused, the message naming the three fixes. The attract
  route (the harness's own `wait N` recipe) needs none.

Measured (2026-10-13, a scratch root with no launcher copy, so each run
stops at the launcher check after the save is decided): `--route
fairyVillage` refused, exit 1; `--save nosuch` refused naming both folders;
`--save fairyVillage` printed `slot 0: fairyVillage.DAT on every side
(--save)`; `--route combat` (header) printed `combat.DAT ... (the recipe's
header)`; `--route load_list --no-save` passed; `all --route fairyVillage`
refused before `prepare` started. Of the 51 recipes in `tools/recipes`, 17
have no header (`grep -L "^# save" tools/recipes/*.txt`): 6 written by
hand that stay on the title or start a NEW GAME (`attract_cycle`,
`battle_commands`, `config_controller`, `config_screen`, `load_list`,
`title_timeline`: `--no-save`; `load_list` draws each install's own slot
list, so its sides differ by design) and 11 `BOF3X_RECORD` routes. Seven of
those have a save of the same name in the main checkout's
`tools/recipe_saves` (`bridgeWalk`, `fairyVillage`, `identify`,
`ninaWalkBehindBlock`, `stallion`, `tarmanLogic`, `vaccinationAndFireScene`);
`combatGroup`, `dragonMenu`, `fairyVillageNews` and `sortScreens` have none,
so which save they were recorded from is the owner's to say. The recipes
themselves are unchanged: adding the headers is a separate edit.
**2026-10-13:** at the owner's guess, `fairyVillageNews` and `dragonMenu`
were played with `input_run.py --save fairyVillage --lang en-US` and
`BOF3X_SHOT_EVERY=60`. Both reached `recipe done` at their last frame (763 and
2150) with no FATAL, and their frames show the same route as the 2026-10-10
captures `analysis/shots/news_loc4` and `dragonmenu_loc2`. Both now carry
`# save fairyVillage`. A rerun of `fairyVillageNews` from the header alone
gave the same bytes as the `--save` run in all 12 frames. `combatGroup` and
`sortScreens` still have no header.

**The timed frames: `BOF3X_SHOT_EVERY=N`** (new, `src/hook/input_script.cpp`).
With a recipe playing and `BOF3X_SHOT_DIR` set, at the latch of every recipe
frame that is a multiple of N the game writes the frame just presented as
`t<FRAME>.bmp` (`render::SaveFrame`, the same as a `shot` step's), and logs

```
input       frame t000600 saved mode 2 area 4 (Frame_Counter 600)
```

with `Game_Mode` (`0x66C7E8`) and `Game_AreaNumber` (`0x904EFC`): the scene
key the comparison and the known differences use. Nothing waits for a
driver; the clock is paused for the write (`GameClock_Pause`), so the frame
deadline has no debt to replay, and `InputScript_ShotPending` is true on the
three passes before (and on the two before a recipe's `shot` step and during
it), so those are drawn even when late - the same frame on every run. One
forced pass was not enough until 2026-10-17: a late pass just before it still
changed the picture, a fade step in a fade
([`button-routes.md`](button-routes.md) 9.2). No input, step or frame of the recipe moves, and nothing happens unless
the variable is set: instrumentation like the `shot` step, no ledger entry.
Under Capcom's `WinMain` (`BOF3X_ORIGINAL=Game_WinMain`) the forced draw is
not there, so a late frame could be the one before; the harness never runs
it.

## 4. The comparison

Frames are matched by name: a timed frame's tick is in its name, a recipe
shot's is the recipe frame the log gives it. Per pair of sides, per frame:

- **The measure.** A pixel differs when any channel moves by more than
  `--tol` (default 4 of 255); a frame differs past `--min-pixels` (default 16)
  such pixels. Reported: the fraction of pixels differing, the mean absolute
  difference over every channel, the largest. A pair whose frame sizes differ
  is a finding of its own, compared after a nearest-neighbour resize.
- **The regions.** The differing pixels, on a grid of 4 game pixels,
  8-connected after a one-cell dilation (so a line of text is one region):
  each a box in game pixels (320 x 240) and a pixel count, largest first.
- **Moved.** For the three largest regions of a frame, the offset (up to 16
  game pixels across and 8 down) at which the second side's picture around the
  box best matches the first's; when that leaves less than half the
  difference of no offset, the region is **moved by dx,dy**. This is the
  check that tells a shifted combatant or text line - the owner's fault
  kind - from a changed texture, and a moved region is never explained by a
  data rule. Tested on a synthetic band shifted 12 game pixels: found as
  `moved -12,+0` in every frame.
- **The heat map**: the first side dimmed to grey, the difference from red
  (small) to yellow (large), each region boxed, magenta unexplained and teal
  matched to a known difference.
- **The scene**: mode and area from each side's log. The first tick where
  a pair's scenes differ is where the runs part (a load that took longer, a
  route that drifted); frames after it are of different moments.

Per pair: frames compared, differing, unexplained, missing, the **first
differing tick**, the first unexplained one, and the first where the scene
parts. The **contact sheet**, 16 rows a page: a label column (frame, scene,
each pair's percentage, UNEXPL or known), then A | B | C | diff A-B | diff
A-C | diff B-C at half size. `index.html` holds the summary, the sheets and
a row per differing frame linking its heat maps.

## 5. Known differences: `recipes/abc-known.toml`

Each entry maps an expected difference to its ledger entry. The conditions
(`tools/abc_known.py`): the pair (`pair_trait`: one side has the trait and
the other not; `both_traits`), the route and tick window, the scene (`modes`,
`areas`, both sides in the same scene), the data (`area_data = "base"` or
`"loc"`: the area's `AREAnnn.DAT` has a chunk of that layer whose hash
differs between the two sides' caches, read from their `manifest.toml`), and
a screen box. A region matches the first entry whose conditions all hold.

Shipped:

| Id | Ledger | Matches |
|---|---|---|
| `psp-area-art` | DIV-0103 | a PSP side against a non-PSP side, in the field (mode 2), in an area whose base chunks differ between the two caches |
| `psp-area-text` | DIV-0090 (LG's amendment, `loc-build-psp.md` 5.3) | the same, both `en-150`, in an area whose `loc/en-150` chunks differ |
| `psp-attract-logo` | DIV-0103 (amendment) | a PSP side against a non-PSP side, the attract route only, modes 1 and 4, the box `196,12-320,84`: the title logo the attract's dialogue scenes draw at the top right (2026-10-13, [`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 5) |
| `psp-tables` | DIV-0104 | state-hash pages only (`Char_ExpTable`, `NameTable_Consumables`) |
| `psp-music` | DIV-0105 | state-hash pages only (`Music_`, `Snd_`, `Seq_`) |
| `logo` | DIV-0099 / DIV-0106 | nothing: the movies play before recipe frame 0 |

**How coarse, and the rule of the file.** A rule explains a whole region by
its scene and pair; it does not look at what is drawn. So the field rules
would swallow, say, a misplaced sprite in an area whose art also differs -
except that a moved region is never explained (section 4). Not shipped, on
purpose, because no scene key says when they are on screen: the PSP's UI
page (its shoulder-button labels) and dial page, its system-pool text in
menus and battle, Ryu's forms (P8). Those show as unexplained and the
owner's eye sorts them; when one is settled, add an entry with the narrowest
box and tick window seen. **When in doubt, unexplained.**

## 6. The report, and the state hash as context

`report_<set>_<route>.md` under the root: the sides (install, frames, how
the run ended, wall time), the per-pair table, then **1. unexplained
differences** - spans of consecutive frames in one scene, each with its worst
frame, percentage, largest regions (moved ones marked) and the heat map and
sheet to open - then 2. where the runs part (scene splits, missing frames,
runs that did not finish), 3. the explained differences per known entry,
4. the equal counts, and what the harness cannot see.

`--statehash` (off by default; the owner: the picture is the point) adds
`BOF3X_STATEHASH` to every run (`--statehash-skip` for a skip list: for
disc-built images, a union of `exe_tables.py gate-skip` lists, as the end
batch used) and `statehash.py diff --align recipe` per pair, its page lines
tagged by the known entries' `pages`, as section 5 of the report.

## 7. Measured

| What | Result |
|---|---|
| `abc_compare.py` on `analysis/shots/combat_a`, `combat_b` and a synthetic C (frames 10.. a band of 128 rows moved 12 game pixels left, two frames with a 16 x 16 orange square, the last frame missing) | 43 frames, 3 pairs, 3 sheets, 17 s. A-C: first differs 660 (frame 10), the band `0,148-320,212` found **moved -12,+0** in every frame, the square `20,20-36,36` as its own region, 1 missing. A-B (two runs of ours from 2026-09-23): 20 pixels in every frame, two 4 x 4 corners at the bottom - a difference between those runs, not the harness |
| the report on that, laid out as a run (`analysis`-style logs with mode and area, EB's `c_psxeu` / `c_pspeu` manifests for the rules) | the field frames of B-C in area 4 matched `psp-area-art` (AREA004's base chunks differ between the two caches), the moved band stayed unexplained in the same frames for A-C and B-C, a scene split at the tick it was put in |

## 8. Live (2026-10-12 evening, the eu triple, `--every 30`)

Headless first: the hosted `BOF3X_SHADOW='*'` self-test from the ini-less
launcher copy, exit 0, `inject: 10087 ours`, every `MISMATCHES` line 0; the
exe host's self-test on the PSX-EU disc-only install, exit 0, 10,088 ours.
The self-tests end before `InputScript_Start`, so `BOF3X_SHOT_EVERY` is
checked by the live runs only: every run wrote its timed frames (366 for
the attract's 11,000 frames, 87 for combat) and a `frame ... mode M area A`
line for each.

| Run | Wall | Compare | Pairs: frames differing / compared |
|---|---|---|---|
| attract, eu triple (tip `07eae8e4`'s parent build, caches of `8fdd1f36`'s tools) | 391 + 396 + 378 s | 294 s | A-B 61/366, A-C 285, B-C 285 |
| attract, psp-eu twice (after the `6c2ae0c9` merge) | 400 + 414 s | 19 s | **2 of 366** (8160: a character's pose; 10320: the camera) - the attract is not frame-exact run to run at those two ticks |
| combat, eu triple, before VF (`6c2ae0c9` merged) | 108 + 97 + 112 s | 94 s | A-B 80/87, A-C 81, B-C 15 |
| combat, eu triple, after VF (`psp/round` `9c5bd99f` merged, caches rebuilt) | 128 + 97 + 125 s | 13 s | **A-B 7/87, A-C 19, B-C 15** |
| cutsceneAndNue, eu triple (before VF) | 268 + 260 + 272 s | 135 s | A-B 95/252, A-C 207, B-C 205; the PSP side parts from tick 6750 |
| shop, eu triple (before VF) | 122 + 116 + 115 s | 45 s | A-B 39/105, A-C 87, B-C 84 |

What the pictures showed (each from the report's unexplained spans, opened
and looked at; descriptions are what is drawn, not game facts):

- **Battle, both disc-only installs, before VF:** the battle scene sits
  lower right with the enemy off the left edge and its name box at the top
  left, the HP bars in place (97% of pixels in 74 battle frames). After VF's
  data fixes: the battle frames equal the PC's but one.
- **The load screen, both disc-only installs (after VF too):** the slot
  numbers gone, each empty slot showing `) 16`, a stray glyph after `Ryu`;
  before VF the TIME read `1` for `00:16`. On the PC install with the
  `en-150` layer the empty slots read the port's Chinese empty-record word.
- **"You gain 3 EXP!"**: the number missing on both disc-only installs
  (after VF; region `96,20-144,36`).
- **The battle Equip window (cutsceneAndNue's `equip_window`), both
  disc-only installs, before VF:** the equipped weapon named `Sea Bream`
  with flame icons on every row, and the count `JK` where the PC has
  `2/128`; the shop's item counts drawn as `50` outside the window.
- **The PSP install:** the title page is the PSP's own logo with a
  `FISHING ver.` line and two `TM` marks, wider than the frame (its right
  edge cut); the message box drawn at the top left where the others draw it
  at the bottom; the battle's shoulder labels `L` / `R` at the scroll bars;
  the Nue fight parting from the others from about tick 4530 (different
  actions, a longer fight; the scene key parts at 6750).
- **The PC install against the PSX-EU disc:** the title's `®` mark,
  present on the PC, absent on the disc-only install (`260,128-272,140`).

What the live check taught the tool (fixed on this branch): a cache built by
older tools is rebuilt (the stamp carries a hash of the importer's tools);
input_run read a game as exited when `tasklist` itself could not start
(another harness's games running, `0xC0000142`), and left it running - a run
now ends any game of its launcher folder by pid, restores slot 0 and tries
once more. Not fixed, written here: the scene key (mode, area) did not see
the shop route part - the PC side reached the inn's save menu at 3000 while
the PSX side was a black fade and the PSP side still in the inn's dialogue,
all mode 7 area 7 - so a pair that differs a great deal inside one scene
may be a drift, and the frames say so before the key does; and `moved`
labelled a line with a missing digit as moved 8 pixels (the rest of the
line matched at that offset).

## 9. What it cannot see

- Sound and music: no audio is captured. The logo movies play before recipe
  frame 0.
- Anything between two timed frames (default a second apart) that is not at
  a recipe shot.
- Anything the route does not reach: the attract sequence shows no menu and
  no battle command; a fault in the equip screen needs a route that opens it.
- A difference inside a region a known rule matched (rules are by scene and
  box), and two sides wrong the same way.
- A route that parts between installs (a load that takes longer on one, an
  `until` met later): from that tick the frames are of different moments, and
  the report says so rather than comparing them as data.

## For the other files

**Folded in 2026-10-12, night** (the coordinator's second pass, branch `psp/ledger2`): The README row and `input-script.md` 3's sentence are in; no ledger entry, as drafted.

### `docs/README.md`

One row: `abc-harness.md` - the A/B/C install harness: one route on the PC
install, a PSX disc alone and a PSP disc alone, the game writing a frame
every N recipe frames (`BOF3X_SHOT_EVERY`), compared picture by picture with
heat maps, contact sheets, moved-region detection and known differences
mapped to the ledger; BUILT.

### `docs/input-script.md` section 3

After "The game writes its own captures": `BOF3X_SHOT_EVERY=N` (with
`BOF3X_SHOT_DIR`) also writes `t<FRAME>.bmp` every N recipe frames, logging
`input frame tNNNNNN saved mode M area A`; no input or frame moves
([`abc-harness.md`](abc-harness.md) 3).

No ledger entry: `BOF3X_SHOT_EVERY` is instrumentation, off unless set, and
changes no game behaviour.

**Trap (2026-10-13, the sweep):** a recipe with no `# save` header (`fairyVillage`)
loads whatever each install's slot 0 holds, so the three sides can start from
different saves ([`abc-sweep-1012.md`](abc-sweep-1012.md) 7). **Closed
2026-10-13:** the harness refuses such a recipe unless `--save NAME` or
`--no-save` is given (section 3a), and `prepare` refuses while a process runs
from `L\` instead of failing with WinError 32 (section 2).

**Trap (2026-10-13):** `input_run.py` can read a `recipe done` line left in
`L\bof3x.log` by the previous run and report the new run done at once, with no
frames ("done at frame 5519" after 1 s). Re-run that side alone; a run that ends
in seconds with no frames is this ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md)).
**Closed 2026-10-13:** a stale `FATAL` line did the same to two sides at once
(the PC and PSX-EU sides "FATAL" in 3 s, reading the 00:58 PSP-EU crash's log);
`input_run.py` now removes the launcher directory's `bof3x.log` before it
launches ([`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 4).

**Fixed 2026-10-13 (the jp sweep):** the DLL truncates the log only when it
opens it, after the launch; `input_run.py` now removes the old `bof3x.log`
before it launches (and stops if it cannot: a game of that launcher folder is
still running). The jp sweep's combat lost both disc-only sides and its shop the
PC side to it before the fix, and no run after it
([`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 1).

**Trap (2026-10-13, the jp sweep):** a recipe that starts at the title with no
`# save` header presses for a cursor that starts on LOAD GAME, which it does
only when the folder has a save file. The harness's game folders have none: the
cursor starts on NEW GAME and LOAD GAME is passed over (inferred from the two
recipes' outcomes, not read in the code), so `battle_commands`'
`press up` lands on CONFIG (on all three sides; `config_screen`'s `press down`
reaches CONFIG either way). Run such a recipe from a scratch copy with that
press made a `wait 1` (no later frame moves).
