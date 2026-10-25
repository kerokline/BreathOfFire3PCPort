# The every-door sweep: where each exit leads, and its music, per install

**Status:** IN PROGRESS (2026-10-25, branch `harness/every-door` off `main`
`48511747`). Built and run once over every area with a link list: the PC
install against the players' disc-built shape (`set.us-disc`), on the current
image and on the image of the commit before PR #136. Not seen by the owner.
Its four crashing doors (area 135) were a port defect a save reaches, fixed
as DIV-0180 on `fix/area-135-door-crash` (4a); its eleven stuck doors are
event areas' messages and scripts (4b), and a change still pending is now
tapped through with the confirm button (`--confirm`, 2026-10-25).

GitHub issues 130, 134 and 139 were exits that sent a player nowhere or to the
wrong area - and 139 played the wrong music - on an install built from the US
PlayStation disc plus the PSP disc, never on the PC install the owner plays.
The A/B/C harness ([`abc-harness.md`](abc-harness.md)) compares pictures along
recorded routes, and no route walked those doors. This sweep takes every door
of every area on two installs and compares where the party arrives and what
music plays there.

## 1. Commands

```
python tools/abc_run.py prepare --set us-disc --root R --installs R/installs.toml --build build
python tools/door_sweep.py run --set us-disc --root R --installs R/installs.toml [--areas 0-199]
python tools/door_sweep.py compare --set us-disc --root R --installs R/installs.toml
python tools/door_sweep.py plan ...        # the reference's link map and the plan only
python tools/door_sweep.py all ...         # abc_run prepare, run, compare
```

`R` is the session's own harness root ([`abc-harness.md`](abc-harness.md) 1a).
`--installs` is a scratch copy of `recipes/abc-installs.toml` with `[paths]
discs` pointed at the extracted discs (`D:/bof3x-runs/discs`; `CDImage\` holds
zips), as earlier sessions did. Options: `--areas` (default `0-199`), `--donor`
(the recipe save the crafted saves start from, default `deisCave`),
`--settle` (frames after an arrival before the peeks, default 60),
`--settle-src` (after a `goto`, default 20), `--chunk` (doors a launch, default
400), `--speed` (default 4), `--minutes` (a launch, default 60), `--only`,
`--tag`, `--fresh`, `--confirm` (the button tapped while a change stays
pending, default `circle`; `none` is the plain wait of section 4's run) and
`--confirm-after` (the frames a change may take before the taps, default 60).
The set's first install is the reference. `circle` is what the load's own
presses (`PREAMBLE`) confirm with on both `us-disc` installs; a set whose
installs confirm with cross (the `eu` installs, [`input-script.md`](input-script.md)
2a) needs both changed.

What lands under `R\door\<set>\`: `plan.json`, and per install `links.json`
(the image's link lists), `steps.json` (one row a door) and `chunkNNN\`
(the recipe, the run's `bof3x.log`, `meta.json`); the crafted saves in
`R\door_saves\`; the report `R\report_door_<set>.md`. All of it is
game-derived and stays out of the tree (rule 1).

## 2. The installs: `set.us-disc`

`recipes/abc-installs.toml` gained `set.us-disc` = `pc-enus` (the PC install
with the US disc's `en-US` layer, as in `set.us`) against **`psx-us-psp`**: a
cache built from the US PSX disc and then the PSP-EU disc (`importer.py build
--source <US cue> --source <PSP-EU iso> --lang en-US`, the order `importer.py
setup` gives a player's two discs - the PlayStation disc the base, the PSP's
after it) and `install --base` from it, `BOF3X_EXEIMAGE` the cache. Its traits
are `psx`, `psp-source`, `en-US` - not `psp`, so abc-known's PSP-only rules
do not match it. Under the harness's `BOF3X_OPT=none` the PSP's optional
layers are off, as a player's defaults have them (only `area4-walls` is on by
default, [`launcher-suites.md`](launcher-suites.md)); what the PSP disc adds to
this install is therefore nothing the runs read. The base image is the US
disc's, as on the players' installs (`base/exe from psx-us`).

`abc_run.py prepare --set us-disc` built it under `C:\Users\kerok\bof3x-runs\everydoor`
on 2026-10-25 in 447 s (`c_pc_us` 195 s, `c_uspsp` 247 s, `install --base` 2 s).
`install --base` writes the disc's `CAPCOM.AVI` and no `LOGOS.AVI`; as for the
`i130` install, the PC's two movies were copied into `g_uspsp` before the runs
(the title presses are timed from launch).

## 3. What it drives: the direct form

The sweep does **not walk**. Every area's exits are its descriptor's link
list, which `Area_LinkAt` reads ([`area-entry.md`](area-entry.md) 1): `+0x20`
points at 12-byte links and `+0x30` holds the index of the last; a link is a
run of cells and either an area and its cells or, `+2` `0xFFFF`, a list of
10-byte alternatives chosen by story flags. When the leader's step lands on a
link cell, `Field_LeaderStepLands` (`0x52E580`, `src/game/event_leader.cpp`)
calls `Area_LinkAt` of the leader's cell and sets `Field_Request` 5. The sweep
makes exactly those two calls at a chosen cell. Three recipe steps were added
to the input script (`src/hook/input_script.cpp`, [`input-script.md`](input-script.md)
3), harness-only - nothing is patched unless a recipe names them, so no
ledger entry:

| Step | Does |
|---|---|
| `links` | logs all 200 descriptors' link lists and alternatives as the running image holds them, every pointer checked before it is followed |
| `goto AREA X Z FLAGS` | `Field_ChangeArea` through the original's entry (`0x594E00`): the funnel a save's load and every scripted area change go through |
| `door X Z` | `Area_LinkAt(X, Z)` through the original's entry (`0x5951D0`), then `Field_Request = 5` when it found a link; logs the pending area, cells, flags and the track `Area_PickMusic` chose |

`tools/door_sweep.py`:

1. **The map.** A first launch on the reference install loads a crafted save
   of the donor where it stands and runs `links`. Measured on the PC image:
   192 areas with a list, 1,542 links - the count [`area-entry.md`](area-entry.md)
   1 gives from a scratch walk of 2026-09-22.
2. **The plan.** For each link, the first cell of its run - searched as
   `Area_LinkAt` searches, from the far end - that no later link also holds
   (the search takes the last link holding a cell, so another cell would take
   another link). 1,535 links in 188 areas have one; 7 are **shadowed** (every
   cell also a later link's): 25.0, 30.0, 30.1, 89.0, 129.0, 141.6, 141.7 -
   listed, not driven. The place `goto` stands the party in an area is where
   the reference's first direct link into it puts it (its cells as
   `Area_LinkAt` makes them, `(w & 0xFF80) << 8`), else an alternative's,
   else (0x12, 0x18), the chapter crafts' "field" cells.
3. **The save.** For the first door of a launch, `chapter_jump.py`'s method:
   the donor's game block with its chapter kept, state 2, run 0, step 0 (the
   field, no run), the area word `+0x24`, entry flags `+0x27` and cells
   `+0x28` / `+0x2C` set to that door's area, the `+0x70` checksum
   recomputed. The donor is `deisCave` (chapter 6, issue 139's route); its
   story flags decide the indirect links' alternatives, the same on every
   install.
4. **The recipe.** The load (ending17's title-to-slot-0 presses, then `until
   Game_Mode == 2`), then per door: `goto` the area unless already there,
   `until Field_Request == 0`, `peek` the area, `door`, `until Field_Request
   == 0`, 60 frames, then `peek` the arrival: `Game_AreaNumber`, `Game_Mode`,
   `Music_Track` (`0x904131`, the track playing), `AreaW1f_MusicCurrent`
   (`0x904CD0`, the one `Area_PickMusic` chose) and the leader's cells
   (`ObjTrio` +0x34 / +0x38). A launch that fails (an `until` past 1,800
   frames, a crash) is resumed from the next door with a save crafted for
   that door's area; the door it stopped in is recorded as failed. Since
   2026-10-25 (`--confirm`, default `circle`) each `until Field_Request == 0`
   is `wait 60` and then `seek circle Field_Request == 0 max 60`: nothing is
   pressed when the change is done within 60 frames (area 135's took 35, one
   51, in section 4b's plain run), else circle is tapped every 30 frames until it is,
   up to the same 1,800. The peeks are then 120 frames after the door, not
   35 + 60; the report's section 4 lists the doors that took taps, a door
   asked with the party outside its area, and one whose party did not stand
   in the pending area's field (mode 2) at the peeks.
5. **The comparison.** Per door, against the reference: the source area
   held, what `Area_LinkAt` made pending (`link`), where the party arrived
   and the leader's cells (`area`), the track playing and the one chosen
   (`music`), and a door one side finished and the other did not (`run`).
   Then the two link maps, link by link. Known differences are
   `recipes/door-known.toml` (abc-known's style and rule: when in doubt,
   unexplained); it has no entry.

**Not seen this way** (a walking sweep would): whether the cells can be
reached on foot; the map's cell codes (`0xA7` / `0xC0`) that decide whether a
step there asks for a link at all; the map edge's own exit
(`Field_InputFlags` bit 1, `AreaW2c_LastArea`) and the return point (`0xA6`);
exits that are not links (scripts' own `Field_ChangeArea`); and the music a
scene starts later than 60 frames after the arrival. A walking form was not
built: entering at a link's arrival cell and walking off it needs each area's
walkable cells and a direction per door, which nothing in the tree gives yet.

## 4. Run: the current image (2026-10-25)

`main` `48511747` plus this branch, `BOF3X_OPT=none`, `BOF3X_LAYERING=0`,
`--speed 4`, all areas:

| Install | links in the image | doors driven | failed | launches | wall |
|---|---|---|---|---|---|
| `pc-enus` | 1,542 in 192 areas | 1,520 | 15 | 19 | 42.9 min |
| `psx-us-psp` | 1,542 in 192 areas | 1,520 | 15 | 18 | 42.8 min |

**No door differs**: on all 1,520 the same pending area, cells, flags and
chosen track, the same arrival area and cells, the same track playing. **No
link list differs.** The disc-built image's links, after PRs #136 and #144,
are the PC's in every area.

The same 15 doors fail on both, and nothing about them differs: 11 where
`Field_Request` stays 5 or 2 past 1,800 frames (54.1, 92.1, 92.3, 92.5,
104.0..104.4, 121.7, 122.0 - an area whose own script holds the change, or
asks for another mode, in this crafted state) and 4 where the game stops with
an access violation reading address 3 in area 135 (135.8, 135.14, 135.17,
135.19; the crash's area 0x87, our DLL `+0xA5E52`, both installs). Both
were read on 2026-10-25: the crash is a port defect a player's save reaches,
fixed as DIV-0180 (4a); the stuck doors are event areas' messages and
scripts (4b). Of the driven doors, 1,517
arrived in the area `Area_LinkAt` made pending; for 54.0 the `goto` into area
54 ended in area 53 in mode 4 (the area's entry moved the party on), so the
door was asked in 53 and found no link there; 92.2 and 92.4 made area 65
pending but the party was still in 92, mode 4, 60 frames later; 74 arrivals play a track other than the one chosen (an
arrival on the world map, mostly) - all of it the same on both installs.

### 4a. Area 135's four crashes: a port defect a save reaches (DIV-0180)

Branch `fix/area-135-door-crash` (off `harness/build-diff-prevention`), root
`C:\Users\kerok\bof3x-runs\door135`.

- **Where.** `llvm-symbolizer` on the run's DLL (`everydoor\L\bof3x.dll`,
  image base `0x10000000`): `+0xA5E52` is `Sprite_UpdateScreenA`
  (`src/game/field_frame.cpp:378`, original `0x57B830`), the stack
  `Field_Frame` - `Field_WaitTransition` - `GameMode_Enter`. The line reads
  `Record54(sprite)[3]`, byte 3 of the record `+0x54` points at; `eax` is
  `0x8021EC`, `Sprite_ObjectsExtra[3]`, reached from `Party_ExtraScreens`
  (`0x57B780`). In the crash dump (read with a scratch minidump reader) that
  object is `+0` 1, `+0x80` 0x10 and nothing else: present, never placed.
- **Why.** That object is area 135's "marker" ([`area_w3d.md`](area_w3d.md)
  1). `Area135_Init` (`0x41EB40`) places it only on an entry into zone 4 from
  zone 1 (op Bx from `Area135_InitOp` `0x62DA8C`, its object byte 3), and in
  zones 4, 6 and 7 sets its `+0` again; the area entry clears only `+0`, so a
  placed marker survives the area's own reloads. The four crashing doors
  arrive in zones 7, 4, 4 and 6 (135.8, .14, .17, .19; peeked after the fix),
  the sweep's standing place is not on the zone-1 path, so the marker was
  never placed. The two working arrivals probed the same way (135.16, 135.18)
  are zones 8 and 9.
- **Reachable in play: yes.** A crafted save standing at each of the four
  arrival cells, loaded after a launch - the shape of an F12 save, which the
  original writes anywhere in the field (`Save_QuickWrite`) - crashes the
  same way; at a zone-8 or zone-9 cell it loads. With Capcom's code
  (`--original Area135_Init,Sprite_UpdateScreenA,Party_ExtraScreens,Field_Frame,Field_WaitTransition`,
  the hosted build) it crashes at `BOF3.exe` `0x57B844` reading address 3:
  the shipped PC port's defect, not a takeover's. Link 17 walks from zone 8
  into zone 4, so a save in zone 8 then that door, with zone 1 not passed
  since the launch, is the same state (from the init's reading; not walked).
- **The PlayStation twin.** The same init (PSX `0x801F5A44`); a read of
  address 3 there is main RAM and does not fault (the R3000's memory map,
  reasoned, not run), so the marker would be drawn from whatever is there. A
  port repair.
- **The fix (DIV-0180).** In zones 4, 6 and 7 an unplaced marker (`+0x54` 0)
  is first placed by the zone-1 branch's own steps, then shown;
  `BOF3X_ORIGINAL=Area135MarkerPlaced` keeps the crash. Self-test
  `area_w3d` exit 0, 276,000 rounds, 0 mismatches; the four loads reach the
  field in mode 2 with the marker placed (cell (20, 85)). Owed: the owner's
  eye on the marker's sequences when the party starts in zone 6 or 7.

### 4b. The eleven stuck doors (2026-10-25)

Read from the logs and the source, with the owner's account of the areas
(owner, 2026-10-25), then re-run over areas 54, 92, 104, 121, 122 and 135 on
both installs: once with the plain wait (`--confirm none`), once with the
confirm taps. "Measured" is a run's log or peek; "source" is a reading of
`src/` not run; "owner" is the owner's play.

The common mechanism (source: `GameMode4_Run` `0x495E60`,
[`mode-tasks.md`](mode-tasks.md), [`mode-rest.md`](mode-rest.md)): an area
that opens a message sets `Field_Request` 2 and mode 4, which returns to the
field only when the message is closed; a request 5 written meanwhile is read
by nobody (only `GameMode_Field`, mode 2, acts on it). `Area_Enter` loads the
new area's data only if the request is still 5 (`mode_flow.cpp:522`), so a
message opened during the fade leaves the party at the new cells on the old
map (92.2 / 92.4 in section 4).

| Door(s) | What the change waits on | Evidence | Save the owner would need |
|---|---|---|---|
| 54.1 (and 54.0) | Nothing of area 54's own: in section 4's run the `goto` came from area 53 with a message open there (mode 4), so 54 was never entered. When the launch starts in area 54 (the crafted save), both doors go to area 65 in mode 2, no taps, on both installs. | measured (plain and tap runs); owner: the entrance is scripted, the party arrives "on the rocket", the area cannot be entered before that scene and not by that entrance after it - only from the world map | A save standing in area 54 (the owner records the rocket route); with it the doors are ordinary. |
| 92.1, 92.3, 92.5 (92.0, 92.2, 92.4) | Area 92's event: entering it opens a message (request 2, mode 4). With taps the request clears; 92.2 and 92.4 then reach area 65 in mode 2, but 92.0, 92.1, 92.3 and 92.5 still stand in 92 in mode 4 at the peeks - the event goes on with further messages. Which code opens the message was not traced. | measured (both runs, both installs); owner: an event area, not a place a player walks freely | A save inside the event at a moment the player can walk, if one exists; otherwise these links are the event's, taken by its script, not a player's doors. |
| 104.0 .. 104.4 | Area 104's script, run on every entry: `Area104_Init` sets `Field_ScriptFlags` 0x140 and arms tail 40; without key item 0xA it shows a message and on its close calls `Field_ChangeArea(121, ...)` itself; with key item 0xA a message, then a countdown of about 25 seconds to area 121, during which a request 5 goes ahead. With taps the message closes and the party is sent on before the door is asked: 104.0 is asked in area 92, 104.1..104.4 in 121, all "no link". | source (`area_w2e.cpp` 138-148, 1052-1107, [`area_w2e.md`](area_w2e.md)); measured (tap run); owner: an event area | A save holding key item 0xA, entering 104 (from 121), the door taken within the countdown. Whether `deisCave` holds key item 0xA was not measured. |
| 121.7 | Plain run: the `goto` into 121 came from area 104 with 104's message open (mode 4). Tap run: 121.6, 121.7 and 121.8's door to 104 completes, and area 104's script sends the party back to 121 (mode 2). | measured; owner: area 121 (Middle Sea, map 7) is an event area too, sailed into from the world map, returning the player to the world map at once | Not a free door: no save makes it one. |
| 122.0 | Nothing of area 122's own: the plain run's `goto` came from area 104 in mode 4. In the tap run the `goto` came from 121 in mode 2 and the door reached area 45 in mode 2 on both installs, no taps. | measured; area 122 has no code (`area_w3b.md`); owner: working out area 122 ("kenju A045") themselves | None for the sweep; what a player meets there is the owner's to say. |

**The re-runs** (root `door135`, worktree build of this branch, `--speed 4`,
48 doors planned in the six areas, none shadowed):

| Run | `pc-enus` | `psx-us-psp` | differ |
|---|---|---|---|
| plain wait (`--confirm none`) | 38 driven, 10 timed out | the same 38 and 10 | none |
| confirm taps (default) | 48 finished: 12 took taps, 5 asked outside their area (104.x), 7 not standing in the pending area's field (92.0, .1, .3, .5; 121.6, .7, .8) | the same | none |

All 23 doors of area 135 arrive in both runs on both installs (135.8 at
(46, 112), .14 at (25, 91), .17 at (2, 85), .19 at (19, 112), mode 2, track
139). The plain run's ten are 92.1, .3, .5, 104.0..104.4, 121.7, 122.0 (54.1
now starts its launch in area 54 and arrives). The taps leave the sweep's
verdict honest: a door finished by taps is listed as such, and the doors an
event moved elsewhere are listed, not counted as arrivals.

## 5. Would it have caught issues 130, 134 and 139?

Measured, not reasoned: a second worktree of `main` at `b0fef7a2` (the first
parent of PR #136's merge, `23a77f57`, so before both PR #136 and PR #144) under
the root, its `exe_tables.py build --source <US cue>` run into a copy of
`c_uspsp` (`c_uspsp_old`, `base/exe` hash `a6bc1336..`, the one the `i130`
install's first setup made; the current tools give `8d1b3b69..`), and the sweep
run on `g_uspsp` with that cache (a scratch `psx-us-psp-old` install of kind
`use`, set `us-disc-old`, the reference's results reused). The old image holds
1,531 links, 11 fewer, and **12 doors differ, none explained**:

| Door | PC | Old disc-built image | Classification |
|---|---|---|---|
| 62.1, 62.2 | to 62, and to 45 | no link: the party stays in 62 | issue 130 (the house near Maekyss Gorge): area 62's `+0x30` read 0, one link of three; port repair, fixed by PR #136 ([`exe-import-engine.md`](exe-import-engine.md) 7.8) |
| 97.1 | to 98, music 67 | no link: stays in 97, music 54 | issue 134 (Mt. Zublo's cave entrance, the way to Deis's temple): area 97's descriptor hole from `+0x24`; same cause and fix |
| 97.2 | to 102, music 90 | no link: stays in 97, music 54 | the same hole (issue 134's area) |
| 98.0 | to 97 at (35, 59), music 54 | **to area 0 at (0, 0), music 2** | issue 139 (Deis's temple's exit to McNeil Village, off the map, with its music): the first alternative zeroed, its condition (flag row 0 bit 0) holding; port repair, fixed by PR #144 (7.9) |
| 162.1..162.4 | to 157 | no link: stays in 162 | area 162's descriptor hole from `+0x24` (7.8 found it with 62 and 97; no player report); fixed by PR #136 |
| 163.1..163.3 | to 157 | no link: stays in 163 | area 163's, the same |

So the sweep flags areas 62, 97, 162, 163 and the Deis temple exit, on the
door's arrival area and, where the party did not move, on the music. The link
map's comparison lists the same 12 links independently (11 gone, 98.0's first
alternative read as area 0). Every difference it reported is a port defect of
the disc-built image already repaired on `main`; none is a divergence, so
`door-known.toml` stays empty.

## 6. What it cannot see

- Section 3's list: reachability on foot, cell codes, the map edge and return
  points, scripted exits, music started late.
- Story flags other than the donor's: an indirect link is driven with
  `deisCave`'s flags only. Another `--donor` (a chapter 1 save, say) sweeps
  the other alternatives.
- The 7 shadowed links, and the event areas' links (4b): with the taps they
  finish, but the event decides where the party goes, not the link.
- The whole sweep has not been re-run with the taps (only the six areas of
  4b); section 4's table is the plain wait's.
- Optional layers: `BOF3X_OPT=none` as every harness run; a player's PSP
  suites (`psp-tables`, which rewrites ranges of `.data`) are not in this run.
- Only `en-US`; the `eu` set's installs (`psx-eu`, `psp-eu`) and `set.us`'s
  `psx-us` were not swept (the last has the same base image as `psx-us-psp`
  under `BOF3X_OPT=none`).

## For the other files

- `docs/README.md`: a row for this file.
- `docs/input-script.md` 3: the three steps.
- `docs/abc-harness.md` 2: `us-disc` in the sets table.
- No ledger entry: the steps are instrumentation, off unless a recipe names them.
