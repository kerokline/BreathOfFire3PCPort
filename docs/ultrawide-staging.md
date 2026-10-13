# Ultrawide staging: what the wider view shows that the 320 view hid

**Status:** MEASURED (2026-10-13), static. Group SS of the ultrawide round
([`ultrawide-round.md`](ultrawide-round.md) section 2, "SS"): the owner's
"code scan". A read of the executable and of our sources, no game run. The
tool is `tools/staging_scan.py`; its table is
`analysis/ultrawide/staging_static.tsv` (gitignored: it holds coordinates and
script addresses read from the exe), with the bank's literal points beside it
in `staging_static_bank.tsv`. PD adds its own section to this file after SS
merges.

The question: which scenes park an actor, walk one in, or make one appear or
vanish just outside the original 320-column view, where the 16:9 picture
(53 columns a side) or the 21:9 one (120 a side) now shows it.

## 1. The answer in one paragraph

The scenes are timelines on one shared clock, and the clock can be read
statically. A scene's actors are movement scripts that wait for values of
counter 0 (`0x903848`, ops `A1 n` / `A2`) and step half a cell per move; the
scenario bank starts the scene and puts the camera on a literal point; the
field camera has fixed angles while a scene runs. So for every actor at rest
while others talk, the scan knows where it stands, from when to when, and
where the camera points, and can project it. **Case zero is found unaided**:
the two party scripts of area 175 that walk off west at clock `0x1B` and wait
until `0x29` come out as *parked, 16:9 band, screen x -40* - and the same
model puts the other two actors of the owner's capture within a few pixels
of where the capture has them (section 2). Over all 200 areas: **332 rows in
57 areas; 36 in the 16:9 band, 20 more in the 21:9 band only, 123 beyond 560
(never shown), 153 that need the census** (section 5).

## 2. Case zero

**The capture** (the owner's, 2026-10-13, 16:9 at k = 4, 1703 x 958; kept in
the session scratch, not committed): in the owner's words, a forest path
beside a wooden bridge, Ryu (the owner's identification) in the middle, two characters
the owner names Nina and Momo standing at the top left, a message box.

**Located through the text, as the brief asked.** The line is in the `en-US`
overlay of areas 175..185 only (a byte search of `bof3/DAT/en-US.*.DAT`; the
eleven share one block), slot 28 of the block (the slot table at the head of
the kind-0 chunk). Areas 175..185 are the community, the faerie village
(memory note; `docs/labelling-pass.md`). No code passes 28 to
`Msg_OpenScript` as an immediate in those areas, no placement word `+0x88`
and no talk script `Dx` names it; the movement-script talk ops do: `E0 t` /
`E3 t` / `E1 d t` store message `t` in the object's word `+0x88` and call
`MoveScript_SetTurnTarget` `0x517E90`, which opens it (`move_groups.cpp`
`MoveScript_GroupE`; `symbols.toml`). One script in the eleven areas has
`E3` with 28: area 175 `+0x10[5]` at `0x641B1C`, offset `+0xCA`, run by the
object the placement script puts down (hidden, placement flags bit 0) under
chapter 7's branch `IF cond 0 x 07 / IF cond 8 x 00 / IFNOT cond 2 x 01 /
IF cond 9 x 01`.

**The scene.** Chapter 7's step hook `Scena07_StepHook` `0x550D20` starts run
2 (`Scena07_Scene2` `0x54F420`) when the leader steps into a band of area 175;
run 2 changes area into 175 at a literal point (`scena_sc7.cpp:349`) and from
then on paces itself on counter 0, resetting the camera angles to the default
at `0x12`. Three of area 175's descriptor `+0x18` scripts (`[3]`, `[4]`,
`[5]`), each starting with an `0D` placement, are the party's for this scene
(the round's reading; who runs which is not read). At clock `0x1B`, `[4]` and
`[5]` walk a dozen cells west (`F2 07 18`) and wait for the clock to reach
`0x29`, when an `0D` puts them back; `[3]` stays and talks; the line of case
zero is spoken at `0x1E`.

**Found without the text.** `python tools/staging_scan.py --area 175` lists
`party +0x18[4]` and `+0x18[5]`, parked from `0x1B` to `0x29` while three
lines are spoken by others, screen (-40, 20): **16:9 band**, confidence
medium (a 30-pixel camera error would cross the band's edge). In the
all-areas ranking they are 6th and 7th.

**The model against the capture** (positions at clock `0x1E`, the camera on
the bank's point; `at_k.py` in the session scratch; capture pixels divided
by 4, less 53):

| Actor | Predicted (x, y) | In the capture |
|---|---|---|
| `+0x18[4]`, `[5]` (the owner's Nina and Momo) | (-40, 20) | about (-34, 22), the pair's centre and feet, cut by the frame's left edge |
| `+0x18[3]` (the owner's Ryu) | (139, 152) | about (138, 157) |
| `+0x10[5]` (the speaker of the line; what it is, the owner's) | (50, 175) | a figure at about (55..80, 165..175), half under the box |

Three independent actors within about ten pixels: the projection, the
half-cell step, the clock and the camera point are right for this scene.
The owner's capture is the only one checked; nothing else in this document
is checked against a picture.

## 3. Method

`tools/staging_scan.py` reads `bof3/BOF3.exe` and our C++ sources; it copies
no table. Four sources, in the brief's order.

**1. The scenario banks (ours).** Every literal point the banks give in
`src/game/scena_*.cpp`, `area_*.cpp`, `effect_18*.cpp`: `ChangeArea(area, x,
z, ..)` / `Field_ChangeArea` (the leader's arrival, which is where the view
is reset), `Kind2At(x, z)` and the kind-2 words set from literals (the
camera's focus: the view follows the kind-2 object,
[`kind2-object.md`](kind2-object.md)), `PlaceParty(x, z, ..)`. The area of a
`Kind2At` is the nearest `Area() == n` test before it in the same function.
**433 literal points** (403 leader, 25 camera, 5 party), 416 with an area.
Also every call from code into a placement op (`EventOp_0x` .. `Bx`): **48**,
36 with a `.data` record address in the instructions before - objects a bank
places mid-scene from its own records (e.g. `Scena07_PlaceObjects`). These are
listed in `staging_static_bank.tsv`, not projected (section 6).

**2. The placement scripts** (`event_scan.py`'s walk, both arms of every
branch). Every op that places an object with a movement script and a
non-zero speed (the update never steps a speed-0 object): its start, its
script, its hidden bit, its condition path. The operand layouts of the
eight Capcom handlers were read for this (capstone on `0x57A010`,
`0x57A3A0`, `0x57A7C0`, `0x57A990`, `0x57AB50`, `0x57AD10`, `0x57AEC0`,
`0x57B530`): x and z (half flag, whole cell) at op `[6..9]` for `0x`..`4x`,
`[5..8]` for `5x` / `7x`, `[4..7]` for `6x` / `8x` / `9x`, `[1..4]` for `Ax` /
`Bx`; the script byte at `[0x10]` (`0x`, `2x`, `3x`), `[0xF]` (`5x`, `7x`),
`[0xE]` (`6x`), `[0xA]` (`Ax`, `Bx`); the flags byte, whose bit 0 sets sprite
`+0` bit `0x40`, at `[0xD]`, `[0xC]`, `[0xB]` or `[8]`. Bit `0x40` is "not
drawn": `Sprite_Draw` returns on it (`sprite_draw.cpp:89`) and
`Party_ExtraScreens` skips it. `1x` and `4x` set no script byte and are
taken as standing objects.

**3. The movement scripts** (`movement_scan.py`'s lengths, labels and flow;
every op a script can reach, in address order). Each script instance is run
as a timeline:

| Ops | Read as |
|---|---|
| `A0 n` / `A1 n` / `A2` (counter 0) | the clock: set / wait until `n` / step. Ops after an `A1 n` happen at `n` |
| `10`..`4F`, `F2 d n`, `F3`, `FA` | `n` moves in direction `d`: `MoveCmd_F9Steps` (read from `0x6696DC`) times the speed for `0x10 / speed` frames (`0x20` for directions 2 and 6) - half a cell on each axis that moves, whatever the speed |
| `02 d n` | the moves of the loop body `n` times more |
| `0D x xh z zh`, `F7` | placed at an absolute point; `0D` to cell (0, 0) is a script's "not here yet" and is skipped |
| `EB` | visibility toggled (bit `0x40`) |
| `E6` | the object ends: removed |
| `E0 t`, `E3 t`, `E1 d t` | a line spoken (message `t`) |
| `FF`, `EF`, `D8`, `86` | the script ends; the object stays |

The descriptor `+0x18` scripts (op `86` hands one to an object; in scenes they
are the party's) start at their first `0D`; the `+0x1C` scripts move the
kind-2 object, the camera.

**4. The camera.** For an actor's stretch at rest, the speakers are the other
actors with a line on the clock inside the stretch (or nearest to it). The
focus is the bank's literal point for the area nearest the speakers' centroid
(within 24 cells); with none, the centroid itself and the row says "needs
census". The projection is the field's own while a scene runs:
`Gte_SetGeomScreen(1000)`, offset (160, 120), `Camera_Angles` (`0xFD56`, 0,
`0x200`) as `Field_ViewReset` sets them and `Scena07_Scene2` resets them,
`Gte_RotMatrix`'s `Rx Ry Rz`, depth `Camera_Distance` 0 + `0x1194`
([`map-layers.md`](map-layers.md), [`map-scroll.md`](map-scroll.md)); the
sprite's vertex is `(x >> 9) - 0x4000` as in `Sprite_UpdateScreen`
(`sprite_screen.cpp`), so a cell is 128 units. Elevation is ignored. When the
area has one kind-2 script on the clock, the screen point is computed with
and without its steps: different bands make the row "needs census (camera
script)", since which scene starts that script is not read.

**Bands** (the projected anchor, the sprite's feet, in the original's
coordinates; a sprite is taken as 24 px wide): **inside** - any part within
`[0, 320)`, the original showed it; **16:9 band** - anchor in `[-53, 0)` or
`[320, 373)`; **21:9 band only** - `[-120, -53)` or `[373, 440)`; **beyond
560** - farther, never shown; **off vertically** - feet above row -8 or below
272, never shown (dropped).

**Classes:** **parked** - visible and at rest for a stretch in which another
actor speaks; "waits, then enters" when its next move ends inside the view.
**Entrance seen early** - its first move starts outside the view and ends
inside. **Spawn pop** - shown (`EB`) or placed while visible (`0D`) in a band.
**Despawn pop** - ended (`E6`) or hidden while in a band. Rows reached by
several instances (one script placed by several branches) are merged and
counted in `instances`.

**Confidence:** **high** - a bank focus point, no kind-2 script in doubt, and
the band the same 30 px either way; **medium** - one of those fails
(usually the 30 px); **low** - no bank point, or the camera script decides
the band.

## 4. The table

`analysis/ultrawide/staging_static.tsv`, one row per finding: `area`,
`chapter` (from the placement's `IF cond 0 x n`, else the bank function's
`ScenaNN_`), `site` (the placement op's address and script, or the `+0x18`
script), `bank` (the function whose literal point is the focus), `object`,
`rest` (map cells), `sx` / `sy` (estimated screen point), `class`, `band`,
`confidence`, `clock` (the stretch, counter-0 values), `fate` (what the
script does at its end: removed, stays, runs on), `staged` (walked there by
the scene, or placed there), `instances`, `spoken` (lines spoken by others
in the stretch), `evidence` (the focus's source line and function, the
kind-2 steps applied, unmodelled ops).

## 5. Results (2026-10-13)

`python tools/staging_scan.py` (1.5 s): 4,034 scripted placements and party
scripts, 2,269 of them on a scene clock in 122 areas; 322 camera scripts.
**332 rows** (386 before merging) in 57 areas.

| Class | 16:9 band | 21:9 band only | beyond 560 | needs census | all |
|---|--:|--:|--:|--:|--:|
| parked | 21 | 16 | 121 | 119 | 277 |
| entrance seen early | 5 | 2 | 2 | 13 | 22 |
| spawn pop | 5 | 2 | 0 | 16 | 23 |
| despawn pop | 5 | 0 | 0 | 5 | 10 |
| **all** | **36** | **20** | **123** | **153** | **332** |

Confidence: 19 high (all of them beyond 560), 160 medium, 153 low. Of the
153 that need the census, 86 have no bank point for their area (the
speakers' centroid stood in) and 67 turn on whether the area's camera script
belongs to the scene. "Beyond 560" rows are listed for completeness: the
wider view does not reach them. The 56 rows in a band are in 21 areas (0, 7,
26, 27, 32, 39, 43, 47, 50, 60, 66, 69, 80, 86, 94, 119, 120, 143, 172,
175, 188); of their parked rows, 17 were walked there by the scene and 20
placed there; 12 of the 56 actors are removed by their script later (`E6`),
the rest stay.

**The ten most likely to be seen** (ranked by band - 16:9 before 21:9, both
modes show the first - then confidence, parked before the momentary
classes, walked there before placed there, then the lines spoken while it
waits):

| # | Area | Ch | Site | Class | Band | sx, sy | Clock | Staged |
|--:|--:|--:|---|---|---|---|---|---|
| 1 | 143 | - | `+0x18[7]` `0x630698` | parked | 16:9 | 366, 200 | `0x3`..end | walked |
| 2 | 0 | 2 | `+0x18[0]` `0x5DAD08` | parked | 16:9 | -15, 0 | start..end | walked |
| 3 | 27 | 2 | `7x` `0x5EC98A` `+0x10[2]` | parked | 16:9 | -46, 17 | `0x12`..end | walked |
| 4 | 172 | 14 | `0x` `0x63D88A` `+0x10[6]` | parked | 16:9 | 357, 13 | `0x13`..end | walked |
| 5 | 60 | - | `+0x18[2]` `0x603D90` | parked | 16:9 | 351, 0 | `0x2`..end | walked |
| 6 | 175 | 7 | `+0x18[4]` `0x641FEC` | parked (case zero) | 16:9 | -40, 20 | `0x1B`..`0x29` | walked |
| 7 | 175 | 7 | `+0x18[5]` `0x6420A4` | parked, waits then enters (case zero) | 16:9 | -40, 20 | `0x1B`..`0x29` | walked |
| 8 | 7 | 1 | `6x` `0x5DFF42` `+0x10[1]` | parked | 16:9 | 347, 214 | `0x5`..`0x8` | walked |
| 9 | 60 | - | `+0x18[1]` `0x603D64` | parked | 16:9 | 351, 0 | `0x8`..end | walked |
| 10 | 39 | 6 | `0x` `0x5F269F` `+0x10[15]` | parked | 16:9 | -27, 10 | start..end | placed |

All ten are medium. Rows 2, 5 and 9 have their feet at the top row (y 0):
mostly above the picture, a head-high sliver at most. What each scene is in
the story is not read here; the area numbers and the sibling's
`names/places.toml` are where to start, and the owner says what it is.

## 6. What this does not see, and did not verify

- **Checked against a picture: case zero only.** Every other row is a
  prediction for EB's census (`BOF3X_BANDLOG`) to confirm or drop.
- **The camera is the bank's literal point.** A scene the bank starts
  without one (a step hook that does not change area or set the kind-2
  words) has no focus here: the speakers' centroid stands in, and the row is
  "needs census". The kind-2 scripts that move without the clock, the
  camera effects of kinds `0x13` / `0x31` / `0x38` (`Scena07_Scene2` uses
  them), `Camera_Distance` changes inside a scene and the player's own camera
  turn before the scene are not followed.
- **Branches are linearised.** Every reachable op is taken in address order;
  a `04`..`09` compare (a branch on a script variable, often a player's
  choice) does not split the timeline. A scene with two outcomes shows the
  positions of both, one after the other.
- **Not modelled:** `C3` / `C4` / `C7` (8.8 moves of sprite fields), `84`
  (to another object's position), `85`, `D4`..`D6` (the party slot's moves),
  `0C` (the trio's visibility). Rows whose actor uses one say so in
  `evidence`. Blocked moves (an object walking into another stops) are not
  simulated: the two actors of case zero both land on the same cell here.
- **Bank code that moves sprites directly** (writes to `+0x34` / `+0x38`
  from literals) and the 48 calls into the placement ops from code are
  listed, not run: their objects' scripts are not attached to a clock here.
- **`1x` / `4x` objects** set no script byte; the object's `+0x83` is
  whatever the slot held. They are taken as standing, and only appear as the
  speakers' context, never as rows.
- **Elevation** is ignored: an actor on a ledge projects higher or lower
  than it is drawn; how far, per area, was not measured.
- **Map edges** (the round's section 1 lists them for SS): not done. Whether
  the band shows void depends on the area map's extent against the camera's
  path, which is area data and play; BA's void channel measures it as
  numbers, and EB runs it. No static list is offered.

## 7. For EB and ST

- EB: the census rows to look for first are the 56 band rows, the ten
  above leading; the 153 "needs census" rows are the ones only a run can
  class. A row the census never shows is either a branch not taken or a
  wrong camera here; a census row with no row here is a scene this scan
  cannot see (the bank's direct moves, a camera effect).
- ST's fix shapes ([`ultrawide-round.md`](ultrawide-round.md) section 4):
  **hide when parked** fits every parked row here - each is a stretch with a
  clock range, so "at rest, wholly outside 320, while a scene runs" is
  exactly the predicate the scan used. The "waits, then enters" rows (an
  actor that later walks back in) are where a hidden actor would pop in at
  the old edge: the candidates for **park farther** instead. Despawn and
  spawn pops in a band would need the opposite treatment (draw nothing in
  the band until the original's edge is crossed), or **narrow scene**.

## 8. Reproduce

    python tools/staging_scan.py                 # writes analysis/ultrawide/staging_static.tsv and _bank.tsv, prints the summary
    python tools/staging_scan.py --area 175      # one area: every actor's timeline, then its rows
    python tools/staging_scan.py --top 25        # a longer ranking

Needs `bof3/BOF3.exe` and capstone (for the bank's placement calls), run
from the repository root.

## For the other files

**`docs/README.md`**, a row:

| [`ultrawide-staging.md`](ultrawide-staging.md) | Group SS of the ultrawide round (2026-10-13): the static staging scan - scenes as timelines on counter 0, the movement scripts' moves, talk and visibility ops, the bank's literal camera points, the field projection; `tools/staging_scan.py` writes `analysis/ultrawide/staging_static.tsv`. Case zero (the owner's capture, area 175, chapter 7) found unaided and three of its actors predicted within about ten pixels; 332 rows in 57 areas, 36 in the 16:9 band, 20 in the 21:9 band only, 153 for the census. MEASURED. |

**`docs/STATUS.md`**, the round's line: SS - the static scan built
(`tools/staging_scan.py`), case zero found unaided; 36 + 20 band rows, 153
for the census; nothing in `src/`.

**`docs/HANDOFF.md`**: EB to run the census against the 56 band rows and
the 153 "needs census" rows of `staging_static.tsv`; the owner to say what
the top ten scenes are and to choose ST's fix shape per class.

**Ledger:** none - no behaviour changes. **Defects:** none found.
