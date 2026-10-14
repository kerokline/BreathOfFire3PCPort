# The wide staging: scenes re-staged for the wider picture (group ST)

**Status:** BUILT (2026-10-13), headless only. Group ST of the ultrawide
round, stage B ([`ultrawide-round.md`](ultrawide-round.md) sections 4 and
4a; `analysis/ultrawide/groups.md`, "ST"). Three ledger drafts, D, E and F
(section 10). **No game was run**: every screen position here is SS's static
projection, and EB-2's census is the first test against play.

**Numbered at the round's fold-in (2026-10-14):** drafts D, E and F (section 10) are DIV-0122, DIV-0123 and DIV-0124; the log tags `DIV-D/E`, `DIV-D`, `DIV-E` and `DIV-F` quoted below are now `DIV-0122/23`, `DIV-0122`, `DIV-0123` and `DIV-0124`.

The owner's two layers (2026-10-13), and the case-zero call:

1. **16:9, fixed properly (DIV D).** An actor a scene starts, parks or ends
   in the band beside the 320 view starts out (or walks on to) a point past
   the live picture's edge, and its walk grows by the same steps, so the walk
   ends where the original's did: the PSP's "out" shape
   ([`ultrawide-staging-psp.md`](ultrawide-staging-psp.md)), scaled to our
   columns, as a rule per site.
2. **Case zero (DIV E)**, area 175, chapter 7: the PSP's staging, the pair
   waiting inside the 4:3 view.
3. **21:9, a simple hide (DIV F).** A field object's sprite wholly beyond the
   16:9 view is not drawn while a scene holds the leader.

Narrow (`BOF3X_WIDE` unset or 0) arms none of it: every hook is a load of
one `bool` and a branch.

## 1. The answer

- **18 site rows re-stage 22 of SS's 36 16:9-band rows**, plus one PD site
  SS's table leaves to the census (area 19). Of the 18: 12 move a start out
  (kIn), 5 walk farther before an end or a hide (kOut, kOutBefore), and 3
  of the kIn rows walk their added steps at the fastest speed because the
  actor's next wait is for a clock value at most 3 past the walk's (section
  4). Every row is checked against the executable by
  `tools/wide_staging_check.py`: the script's address and hash, the ops,
  and the point the rule puts the actor at, wholly outside the picture at
  53 and at 120 columns (0 problems).
- **14 rows are not re-staged** (section 3): the out shape cannot fix a
  point the scene ends on, a stationary actor, or a walk whose end is
  itself in the band. They stay the owner's call; at 21:9 the hide does not
  reach them either (they are inside `[-53, 373)`).
- **Case zero** is six ops in two scripts: each west walk 3 steps shorter
  and then 6 north, the east walk 6 south first and 3 shorter. SS's camera
  puts the waiting pair at screen x 38, inside the 4:3 view (PD's figure).
- **The 21:9 hide** is one call in `Sprite_Draw`, after the cull and bit 7,
  and the census's new `hidden` column reads the same decision.
- **Self-tests:** `BOF3X_SHADOW=wide_staging`, 79,938 checks, 0 mismatches
  (the rule, the table, narrow applying nothing, the hide's decision at the
  band edges, and the rewrite engine on six real rows); ten planted
  changes, all refused (section 9). The `'*'` runs at `BOF3X_WIDE` unset,
  1 and 2 are in section 9.

## 2. What is built

`src/game/wide_staging.{h,cpp}`, armed by `wide_staging::Inject()` in
`InjectAll` after every module's self-test (beside `Widescreen_ArmFills`).
Four hook lines, each one line:

| Where | Line | What it does |
|---|---|---|
| `MoveScript_Step` (`move_script.cpp`), after the flow pass | `wide_staging::Step(object, script, result)` | at a site's op, runs the rewritten walk |
| the flow pass's op `0D` (`move_script.cpp`), after it places the sprite, before the height | `wide_staging::Placed0D(object, script, Pos(object))` | moves a site's start out |
| `EventScript_Op` (`event_script.cpp`), after the handler | `wide_staging::Placed(at)` | moves a placement-op start out |
| `Sprite_Draw` (`sprite_draw.cpp`), after `object[0] &= 0x7F` | `if (wide_staging::Hide(object)) return;` | DIV F |

**The rewrite.** A site's op is run as a walk of up to two elements, each a
direction and a step count, issued exactly as `MoveScript_Step` issues its
own move ops: the direction is the step's result, the steps go to the
context's `+7`, and `MoveCmd_Move` starts the timed move. The position
stays on the op until the last element, which leaves it where the op would
have (or, for a walk inserted before an op, lets that op run). The script
bytes are never written: the move's direction and steps are read from the
script at run time, and the row only says how many steps to add.

- **kIn** - the start moves out. Where the placement op (`EventScript_Op`)
  or the script's `0D` puts the actor, it is moved back along the walk's
  direction by `Steps()` steps (the same per-step displacement
  `MoveCmd_Move` and `Field_ObjectMotion` give: half a cell an axis), with
  the ground's height taken there; the object is marked. At the walk, a
  marked object walks `Steps()` steps and then the original's. An object
  whose start was not moved (another instance of the script) walks the
  original's.
- **kOut** - the original's walk, then `Steps()` more: the point the actor
  ends or hides at moves out.
- **kOutBefore** - `Steps()` steps in the direction of the move at `move`,
  walked just before the op at `before` (area 60: the actor talks at the
  walk's end, and the PSP put its extra walk after the talk and before the
  hide; here too, so the talk stays where it was).

**The rule** (the brief's "rule in the columns"): for `c` columns,
`Steps(c) = ceil((c + 12 - beyond) * 10 / pps10)`, capped by `top` when it
is not 0. `beyond` is how far the site's point already lies past the 320
view's edge (px; negative inside it), `pps10` the screen px one step
outward gains (x10), and `top` the steps that take the feet above row -8
(SS's `VIS_Y`: the sprite is drawn up from its feet). 12 is SS's half
sprite. `c = 0` gives 0 steps. The three per-site numbers are measurements,
not per-width values: `--measure` derives them from SS's camera, taking the
largest `pps10` whose rule still reaches the exact count at both 53 and 120
(the projection is not linear, so the rule is never short at either width;
it takes one step more than the exact count on two rows - row 6 at 120,
row 11 at 53 - and the exact count everywhere else).

**The checks at the arm.** Each row's script is found through
`Area_Descriptors[area]` (the `+0x10` or `+0x18` table and the index) and
must hold the address the row names and the FNV-1a hash of its first
`span` bytes (and of the placement op's 17 bytes, for a placement start);
the op at `move` must be a move with steps, the one at `at0d` an `0D`, the
one at `before` an `EB` or `E6`. A row that fails is left original and
logged - a disc-built image or a PSP-data install whose scripts differ (the
PSP's own changes, among them). Like `PatchBytes` and the twin rows: the
original's bytes are checked, never copied.

**Switches.** `BOF3X_WIDE_STAGING=0` leaves D and E original in the wide
modes; `BOF3X_WIDE_HIDE=0` leaves F off at 21:9; `BOF3X_WIDE_STAGING_LOG=1`
logs every application (by default each row logs its first). The arm logs
`DIV-D/E     the wide staging on: N of 24 rows armed at C columns` and the
hide's state.

**The live state.** A small table (32 entries) holds, per script context,
the mark of a moved start and a walk in progress (its plan, the next
element, where the sprite stood when the last element began, a speed to put
back). A walk in progress whose context has moved to another op, script or
area is dropped. A first step blocked by another object (`Field_ObjectBlocked`
- objects and party members only, no map test) puts the position back in
`Field_ObjectUpdate`; the engine sees the sprite where the element began and
issues it again, as the original re-runs the whole op.

## 3. SS's 36 16:9-band rows

`analysis/ultrawide/staging_static.tsv`, the rows with band "16:9 band", in
its order. "Row" is the `kSites` row (`wide_staging.cpp`); sx is SS's.

| # | Area | Script | SS class, sx | Done | Shape, or why not |
|--:|--:|---|---|---|---|
| 1 | 0 | `+0x18[0]` | parked, -15, sy 0, start..end | no | the rest is the scene's whole stretch and its end; the feet are on row 0, so the sprite is above the picture (SS: "a head-high sliver at most") |
| 2 | 0 | `+0x10[5]` | spawn pop, 340 | row 0 | kIn by its placement op `0x5DA1FC`; 7 steps at both widths (to the top) |
| 3 | 0 | `+0x10[6]` | spawn pop, 360 | row 1 | kIn by `0x5DA20D` (the PSP moved this one a cell) |
| 4 | 7 | `+0x10[1]` | parked, 347, 0x5..0x8 | no | a point mid-route the scene walks the actor to and from, in view on both sides; moving it changes the scene, not the edge. Its camera is uncertain (a kind-2 script off the clock) |
| 5 | 26 | `+0x10[27]` | despawn pop, 343 | no | stands where it is placed and is removed: no walk to lengthen |
| 6 | 26 | `+0x10[2]` | parked, -26, placed | no | a stationary actor that speaks; moving it moves where it speaks |
| 7 | 27 | `+0x10[2]` | parked, -46, then E6 | row 3 | kOut (the PSP added 2 steps) |
| 8 | 32 | `+0x10[4]` | spawn pop, -46 | row 4 | kIn by the `0D` at `+0x10` (the PSP moved it a cell) |
| 9 | 39 | `+0x10[15]` | parked, -27, placed | no | stationary, speaks (as 6) |
| 10 | 39 | `+0x10[34]` | parked, waits then enters, -46 | row 5 | kIn by `0x5F2AC8` |
| 11 | 39 | `+0x18[14]` | spawn pop, -38 | row 6 | kIn by the `0D` at `+0x10E` |
| 12 | 43 | `+0x18[3]` | parked, waits then enters, 334 | row 7 | kIn by the `0D` at `+0x8`; fast (its next wait is 2 clock values on). SS's "parked" stretch is the scan's: the script's first `0D` is the "not here yet" cell, so the actor appears at `0x11`, just before the walk |
| 13 | 50 | `+0x18[2]` | entrance seen early, 347 | row 8 | kIn by the `0D` at `+0x0`; fast (1 value on) |
| 14, 15 | 60 | `+0x18[2]` | despawn pop and parked, 351, sy 0 | row 9 | kOutBefore the `EB` at `+0x31` (the PSP added 6 steps there); 3 steps take it off the top |
| 16 | 60 | `+0x18[1]` | parked, 351, sy 0, to the end | no | walks to the same door as 14 and stays there to the scene's end (EF): the end point; on row 0, above the picture. The PSP left it |
| 17 | 66 | `+0x10[1]` | entrance seen early, 365 | row 10 | kIn by `0x6049BD` |
| 18 | 69 | `+0x10[1]` | despawn pop, 332 | row 11 | kOut |
| 19 | 69 | `+0x10[2]` | parked, waits then enters, -44 | row 12 | kIn by `0x607EEC` |
| 20, 22 | 94 | `+0x10[39]` | entrance, and parked waits then enters, -34 | row 13 | kIn by `0x6158CD` (the PSP moved it 2 cells) |
| 21 | 94 | `+0x10[9]` | parked, 333, placed, to the end | no | stationary (as 6) |
| 23, 24 | 120 | `+0x18[15]` | entrance, and parked waits then enters, 334 | row 14 | kIn by the `0D` at `+0x0` |
| 25..29 | 143 | `+0x18[6]`, `[7]`, `[8]` | five rows, 334 / 366 | no | three party members appear one by one at the same point (`0D` at clock 1, 2, 3) and step a few steps aside, two of them outward, and stay there to the end (SS's top-ranked row is `[7]` at 366). Their end points are in the bands; the out shape cannot move an end. `[8]`'s entrance could be re-staged only with a walk in a direction the original does not have. The camera is 14 to 20 cells from the speakers in SS's model: the census first |
| 30 | 172 | `+0x10[6]` | parked, 357, to the end | no | walks out from the view and stays at the end of its walk: the end point is in the band (its partner `[7]` ends in the 21:9 band) |
| 31 | 175 | `+0x10[6]` | despawn pop, 353 | row 15 | kOut |
| 32 | 175 | `+0x10[7]` | despawn pop, 358 | row 16 | kOut |
| 33, 34 | 175 | `+0x18[4]`, `[5]` | case zero | E | section 5 |
| 35 | 175 | `+0x10[7]` | spawn pop, 337 | row 17 | kIn by `0x6418BD`; fast (3 values on) |
| 36 | 188 | `+0x10[1]` | parked, -28, placed, to the end | no | stationary (as 6) |

**PD's sites beyond SS's band rows:** area 19 `+0x18[3]` (row 2), the PSP's
start 2 cells farther and walk 4 longer; SS has it as "needs census (16:9
band if the speakers are centred)", with no bank focus - low confidence,
included because Capcom's own change confirms the site. PD's other changed
scripts (areas 26, 28, 94 `+0x10[4]`, 128, 141, 150 and the rest of its
section 5) sit in SS's "needs census" or "beyond 560" rows or have no row;
their screen positions are not known statically, so they wait for EB.

## 4. The rows, measured

`python tools/wide_staging_check.py` (the bytes and the outcome; `--measure`
derives the measured fields). Frames are the added steps at the object's
own speed (the placement op's speed byte); "party" rows are the party
members' scene scripts, whose speed is not read here. After the walk: what
the actor's own script does next.

| Row | Area | Shape | beyond, pps10, top | Steps 53 / 120 | New point 53 / 120 (sx, sy) | Frames 53 / 120 | After the walk |
|--:|--:|---|---|---|---|---|---|
| 0 | 0 | kIn | 20, 74, 7 | 7 / 7 | (386, -9) / (386, -9) | 56 / 56 | waits for 0x17 at 0x10 |
| 1 | 0 | kIn | 40, 83, 9 | 4 / 9 | (387, 7) / (418, -9) | 32 / 72 | its end (FF) |
| 2 | 19 | kIn | 16, 144, 0 | 4 / 9 | (-70, 210) / (-143, 245) | party | it steps the clock |
| 3 | 27 | kOut | 46, 94, 8 | 3 / 8 | (-66, 7) / (-97, -9) | 24 / 64 | it steps the clock |
| 4 | 32 | kIn | 46, 171, 0 | 2 / 6 | (-74, 213) / (-132, 242) | 8 / 24 | it steps the clock |
| 5 | 39 | kIn | 46, 171, 0 | 2 / 6 | (-74, 213) / (-132, 242) | 16 / 48 | it steps the clock |
| 6 | 39 | kIn | 38, 134, 0 | 3 / 8 | (-78, 194) / (-150, 227) | party | it steps the clock |
| 7 | 43 | kIn, fast | 14, 168, 0 | 4 / 8 | (394, 289) / (459, 324) | party | **waits for 0x13 at 0x11** |
| 8 | 50 | kIn, fast | 27, 149, 0 | 3 / 8 | (389, 235) / (465, 273) | party | **waits for 0x16 at 0x15** |
| 9 | 60 | kOutBefore | 31, 169, 3 | 3 / 3 | (371, -9) / (371, -9) | party | waits for 0x63 at 0x3 |
| 10 | 66 | kIn | 45, 173, 0 | 2 / 6 | (394, 250) / (456, 281) | 16 / 48 | it steps the clock |
| 11 | 69 | kOut | 12, 74, 27 | 8 / 17 | (393, 57) / (453, 23) | 64 / 136 | it steps the clock |
| 12 | 69 | kIn | 44, 73, 23 | 3 / 13 | (-66, 61) / (-134, 24) | 24 / 104 | it steps the clock |
| 13 | 94 | kIn | 34, 69, 17 | 5 / 15 | (-69, 30) / (-133, -3) | 40 / 120 | it steps the clock |
| 14 | 120 | kIn | 14, 168, 0 | 4 / 8 | (394, 289) / (459, 324) | party | it steps the clock |
| 15 | 175 | kOut | 33, 79, 10 | 5 / 10 | (387, 6) / (418, -10) | 40 / 80 | its end (E6) |
| 16 | 175 | kOut | 38, 72, 14 | 4 / 14 | (386, 24) / (449, -9) | 32 / 112 | its end (E6) |
| 17 | 175 | kIn, fast | 17, 71, 17 | 7 / 17 | (387, 23) / (449, -9) | 56 / 136 | **waits for 0x35 at 0x32** |

**The clock.** A longer walk takes more frames. Where the actor steps or
sets the clock itself right after its walk (rows 2..6, 10..14), nothing can
pass it: the scene waits for it, a little longer than the original's (at
most 136 frames, row 11 at 21:9, where the scene's next beat comes about 2
s later). Where its script ends after the walk (rows 1, 15, 16), nothing
waits on it. Where it next waits for a clock value another script steps,
the value could pass while it walks on, and the actor would wait for ever
(the `A1` test is equality): **rows 7, 8 and 17** wait for a value 2, 1
and 3 past the walk's, so they walk the added steps at the fastest speed
the game has whose frames stay whole (`Field_MoveSpeeds` index 5, 16: one
frame a step, two on a diagonal) - the added steps lie out of view, and
they cost at most 17 frames. Rows 0 (7 values on) and 9 (96 on) walk at
their own speed. `RISK_GAP` (3) in the tool is that line; it is a
judgement, not a measurement - how many frames a clock value lasts depends
on who steps it, often a line the player closes.

**What the shape does not see.** The camera is SS's (medium confidence: a
30 px error crosses a band edge); the new start's map cells are within the
map's range (the check) but not known to be clear: the extended walk is not
blocked by walls (`Field_ObjectBlocked` tests objects only) but would stop
at an object standing on it, and a blocked actor retries until it can
move - an object parked on a site's extended path would hold the scene.
Not checked statically.

## 5. Case zero (DIV E)

Area 175, chapter 7, `Scena07_Scene2`, the party's scene scripts
`+0x18[4]` and `[5]` - the pair the owner names Nina and Momo
([`ultrawide-staging.md`](ultrawide-staging.md) 2). The original walks them
24 steps west at clock `0x1B` and they wait at screen x -40, in the 16:9
band, until `0x29`. The PSP (PD's section 2, rows 53 and 54, and its
script diff) walks them 21 west and then 6 north, so they wait inside the
4:3 view (SS's camera: x 38); the route at `0x29` does the same, and before
the east walk at `0x33` it adds 6 south and walks 3 fewer east, so the
route ends where the original's does.

In our code (`kCaseZero`): at each west walk (`[4]` `+0x60`, `+0x82`;
`[5]` `+0x74`, `+0x94`) the walk keeps its direction with 3 fewer steps,
then 6 steps north; at the east walk (`[4]` `+0x8B`, `[5]` `+0x9D`) 6 steps
south first, then the walk with 3 fewer. The two scripts are checked by
address and hash at the arm, each op must be an `F2` of 4 steps or more.
Both wide modes, as the owner asked; narrow is the original's.
`wide_staging_check.py` replays the route: the waiting point (25.5, 27),
sx 38; the east walk ends at (33, 28) and (34, 28), the original's.

The extra 6 steps each way are the PSP's own timing; `[4]` steps the clock
after its walk, `[5]` waits for `0x29`, 13 clock values on (lines spoken
between).

### 5a. Revised 2026-10-14: the original's frames, and off the top row

EB-2's census 3 ([`ultrawide-census-3.md`](ultrawide-census-3.md) 3)
found two things. First, the PSP's 21 + 6 steps take 3 steps' frames more
than the original's 24. Because `[4]` steps the clock after its walk, the
dialogue started later and `dolphinBoss`'s narrow recording parted from
about frame 6184 (`Rand` from 10,921). With `BOF3X_WIDE_STAGING=0` it was
equal on all 14,923 frames. Second, the pair waited just above the top
edge: a sliver of one sprite's feet stayed on row 0 at game x 25..48.
The PSP crops 12 rows, which hid them there; we keep all 240.

The round's rule is that game timing stays Capcom's. So each of the three
rewritten walks (both west walks and the east one) now takes **exactly the
original's frames**:

- **Where the frames go.** The walk loses 3 steps, which is `3 * f` frames,
  where `f = 0x10 / speed` is the actor's frames a step (`MoveCmd_Move`).
  Those frames are spent on the added walk: `a` steps at the actor's own
  speed, then `b` at the fastest whole speed (`Field_MoveSpeeds` index 5,
  one frame a step), with `a * f + b = 3 * f`. Elements of no steps are
  left out, because each would cost a frame. The next element starts the
  frame after the last one's last step, as the original's next op would.
- **How far north.** The north walk must take the drawn rectangle's bottom
  above row 0. `band_census::ScreenRect` is read when the north walk
  starts. Its bottom row, divided by `kCaseZeroRise10 / 10` px a step,
  gives the steps needed: 2.7 px, the smallest lift over 20 steps from the
  turn by SS's camera. `wide_staging_check.py` measures that lift and fails
  if the constant exceeds it. `a` is taken as large as that allows, so
  the walk is at the actor's own pace while visible and fast once out of
  view. If even all-fast steps (`3 * f` of them) are too few, all-fast is
  used.
- **Back south.** The east walk's south steps are the last north walk's of
  the same script (`a` paced after `b` fast), so the route ends where the
  original's does. With no north walk recorded, the east walk is the
  original's.
- **The speed.** A plan that ends on the fast steps puts the actor's speed
  back at its next step, before its next op.

At speed index 2 (8 frames a step, assumed in the tool's replay; the
party's is read at run time), the choices are 3, 10, 17 or 24 north steps.
SS's camera puts the turn at row 31, which needs 12 steps, so 17 are taken.
The census's real camera sits about 10 rows higher, so 10 may do. The
cases a speed does not divide 0x10 leave the walk original (logged).

The engine's self-test runs the three walks at speed indices 1..5 with the
feet at rows -10..120 when the north walk starts. For each run it counts
every element's steps times `0x10 / speed`:

- west: 24 steps' frames, 21 west;
- north: enough steps to clear the top row whenever the frames allow, and
  the rectangle's bottom then at or above row 0;
- east: 18 steps' frames, the south steps equal to the north ones, 15 east;
- the speed is the actor's again afterwards.

Result: `wide_staging self-test: 79938 checks, 0 mismatches`. Two controls
were refused: the fast steps walked at the actor's speed (184 mismatches),
and the speed not put back (92). **Not verified:** that the party's script
path (`FieldCore_ScriptMoveNext`) spends exactly `0x10 / speed` frames a
step and starts the next element the frame after, as `Field_ObjectUpdate`
does for objects. EB's live `dolphinBoss` hash is the check.

## 6. The 21:9 hide (DIV F)

At `BOF3X_WIDE=2` (the columns more than the 16:9 picture's 53, by
`widescreen_rule::ColumnsForAspect(16, 9)`), `Sprite_Draw` returns before
building the primitive when:

- the record is one of the 30 field objects, the 4 extra objects, or party
  member 1 or 2 of `ObjTrio` - never member 0, the leader, and never the
  kind-2 object or anything else;
- a scene holds the leader: the census's scene mask, bits 1..8
  (`band_census::SceneMask()`, the four tests the game makes before the
  leader may move; [`band-census.md`](band-census.md) 3) - bit 0x10 alone
  (a menu, an encounter) is not a scene;
- **and it is a field scene** (`FieldScene()`, added 2026-10-13 after
  EB-1's census, [`ultrawide-census-1.md`](ultrawide-census-1.md) 2.2, found
  the scene mask set on the world map, `0C`, and through the attract demo,
  `02`, where the hide as first keyed would have hidden every band object).
  The game's own tests, not more mask values: `Game_Mode` 2, the field
  (`GameMode_Field` `0x4959F0` runs as mode 2; 3 is the menu, 5 battle),
  **or 4**, the field with a message open (added 2026-10-14). Census 3,
  [`ultrawide-census-3.md`](ultrawide-census-3.md) 4.1, found 175 object
  rows and 26 party rows drawn in mode 4 in areas 39 and 41's long scenes.
  `GameMode_Field` sets mode 4 on `Field_Request` 2. `GameMode4_Run`
  `0x495E60` (ours, `mode_rest.cpp`; PSX `0x801989D4`) runs
  `Field_FrameScripted`, the scripted field frame, every frame. Once the
  message cells' bit 1 (`0x7DEE44`) is set, it puts `Game_Mode` back to 2.
  Mode 6, `Look_PadControl`, is not the field frame and is not admitted;
  `WorldMap_RecordIndex` `0x462A90` is 11, no world-map record for the area
  (the value `0x56DE30` dispatches its world-map hooks on; the eleven
  records' areas, read from the exe: 16, 33, 45, 65, 87, 88, 104, 115, 121,
  151, 152 - 16 and 45 are where the census saw `0C`); and the chapter
  `Cond_ByteFA` is not 16, the attract demo (`Title_StateStartDemo`
  `0x462300` starts it with `Scenario_Start(0x10)`;
  [`field-modes.md`](field-modes.md)). The fuzz runs every mode 0..15,
  chapter -1..20 and record index 0..12, the census's world-map and demo
  cases (none hidden), the menu and battle modes, a field scene and free
  walking, and `WorldMap_RecordIndex` itself on areas 16, 45 (a record), 7
  and 175 (none): `wide_staging self-test: 79025 checks, 0 mismatches`. A
  control that drops the world-map test is refused (255 mismatches). Left
  as the census found it: mask `0C` also shows at a field area change
  (area 7's entry, `Field_Request` 5); off the world map that still counts
  as a scene, for the frames of the fade;
- its rectangle (`band_census::ScreenRect()`, the census's reading of what
  the draw pass draws) lies wholly beyond the 16:9 view: right edge at or
  left of -53, or left edge at or right of 373;
- no row of `kHideExceptions` names it (area, `0x10` and the object's
  script index, or 0 and the member's slot). Empty to start.

**Draw-only.** The call is after the scale store, the cull and bit 7 (the
off-screen flag nine logic readers take, UC2's), so the flag and every
field the original writes are written as before; returning there skips
only the primitive and its cell records - `Gfx_PacketNext` and
`SpriteCell_Count` are not advanced, as for a frame of count 0. Nothing
reads whether a sprite was drawn. With `BOF3X_SHADOW=sprite_draw` live,
the clone draws what ours hides: a mismatch by design at 21:9 in scenes.

**The census's column.** `band_census::Record` now writes `hidden` (`yes` /
`no`) as the last column, from `wide_staging::WouldHide()`, the same
decision on the same rectangle in the same frame (the census records in
`Sprite_UpdateScreen`, before the draw); a hidden stretch is a row of its
own. `tools/band_census.py` reads it, keys on it, says "hidden" in
`--rows`, and reads a log without the column as not hidden (its self-test,
0 mismatches).

## 7. For EB: adding sites

EB-1's live census may name sites SS's static table missed or marked
"needs census". Each becomes one `kSites` row:

1. Find the actor's script and ops: `python tools/staging_scan.py --area N`
   (the timelines) and the census row's object (`who`, `index`, `script`).
2. Write the row with zeros in `hash`, `beyond`, `pps10` and `top`: the
   area, `0x10` / `0x18` and the index, the script's address, a `span` past
   the last op the row names, the start (`place` or `at0d`) for kIn, the
   move, `before` for kOutBefore.
3. `python tools/wide_staging_check.py --measure` prints the four fields
   from SS's camera; for a site SS's scan has no row for, take `beyond` and
   `pps10` from the census's rectangle and the walk's per-step motion
   instead.
4. `python tools/wide_staging_check.py` must say 0 problems; mark `fast`
   where it says "at risk".

## 8. The two sweeps (proposals, not built)

The brief: read them, propose rules, build only an end point plainly moved
by the columns.

- **`EffectKind0D`** (`effect_1a.cpp`, `0x465F30` / `0x466050`): slides in
  from x 390 by 30 a frame to 160, holds 30 frames, slides out to -70 or
  less, where the record goes idle (`+1 = 0`). The rule is plain - start at
  `390 + c`, end at `-70 - c` (UC's row 14) - but the record goes idle
  `ceil(c / 30)` frames later each way (2 at 53, 4 at 120), and whether
  anything waits for that idle state was not read. **Proposed instead, as
  draw-only:** keep the logic's x and draw it mapped, `[160, 390]` onto
  `[160, 390 + c]` and `[-70, 160]` onto `[-70 - c, 160]`, so the slide
  starts and ends past the edge in the same frames. Not built: it needs a
  draw hook in `Sprite_QueueOverlay`'s path. The owner's call.
- **`Area172_EffectA5Grow`** (`area_w4b.cpp`, `0x428240`): the shade's edge
  grows by 10 a frame and at more than 400 it adds one to the script
  variable after `MoveScript_Var7` and moves to state 2 - the scene goes on
  from that. Ending at `400 + c` would hold the scene 6 frames (53) or 12
  (120) longer: logic, not plainly an end point. **Proposed:** leave it
  (the shade vanishing with 37 px in the 16:9 band, 104 in the 21:9 band,
  UC's row 15), or the same draw-only mapping. The owner's call.

## 9. Runs

All headless: a copy of the launcher, `bof3x.dll` and `bof3x.exe` in an
ini-less scratch folder (`uw_st/st.sh`), `BOF3X_SELFTEST_ONLY=1
BOF3X_LANG=original`, `--game bof3 --no-config`, the branch as committed.

- **`BOF3X_SHADOW=wide_staging`, `BOF3X_WIDE=2`:** exit 0;
  `shadow      wide_staging self-test: 79025 checks, 0 mismatches`;
  `DIV-D/E     the wide staging on: 24 of 24 rows armed at 120 columns (0
  left original)`; `DIV-F       the 21:9 hide on: ...`.
- **Ten planted changes, all refused** (`uw_st/controls.py`: the change
  applied, built, the self-test alone at `=2`): the hide's edge `<` for
  `<=` (31 mismatches), the scene mask ignored (284), the leader a
  candidate (1), kOut's added steps first (3), no blocked-step reissue (3),
  a kIn walk without its moved start (2), the start moved the wrong way
  (1), the fast speed not put back (1), case zero's delta ignored (2), the
  rule's ceiling dropped (11,875).
- **The `'*'` runs** at `BOF3X_WIDE` unset, `=1` and `=2`: see the table
  below (filled from the logs).

| `BOF3X_WIDE` | Exit | `inject:` | `DIV-0041` lines | This group's lines |
|---|---|---|---|---|
| unset | 0 | `10087 ours, 0 left original by BOF3X_ORIGINAL` | none | `wide_staging self-test: 79938 checks, 0 mismatches`; no arm line (0 columns: nothing armed) |
| `1` | 0 | the same | `widescreen: 53 columns a side (426 x 240); terrain cull [-150, 470]; ...`, `full-frame fills at (-53, 0) 426 x 240` | the self-test as above; `the wide staging on: 24 of 24 rows armed at 53 columns (0 left original)`; `the 21:9 hide off: the view is not wider than 16:9` |
| `2` | 0 | the same | `widescreen: 120 columns a side (560 x 240); terrain cull [-217, 537]; ...`, `full-frame fills at (-120, 0) 560 x 240` | the self-test; `24 of 24 rows armed at 120 columns`; `the 21:9 hide on` |

In all three: `band_census self-test: 94532 checks ..., 0 MISMATCHES`,
`sprite_screen self-test: 20000 rounds, ..., 0 MISMATCHES`, and no line in
any log with a non-zero mismatch count or `FATAL` (a search of the three
logs).

`python tools/wide_staging_check.py`: 0 problems. `python
tools/wide_staging_check.py --self-test` and `python tools/band_census.py
--self-test`: 0 mismatches each.

**Not verified:** any of it in play. Whether a moved start or an extended
walk looks right, whether a camera pan in a scene brings a moved actor into
view, whether an object stands on an extended path, whether the hide ever
hides something the scene meant to show from beyond the 16:9 edge. That
the four scene-mask bits are set in the scenes and clear in free walking is
BA's open question, answered by EB's census. `MoveScript_Step`'s party path
(`FieldCore_ScriptMoveNext`) is taken to call the step only when a move's
steps are spent, as `Field_ObjectUpdate` does - read for the field objects,
assumed for the party (the census of rows 2, 6..9, 14 and case zero tests
it).

## 10. For the other files

**`docs/DIVERGENCE.md`**, three entries (the coordinator numbers them):

### D. The wide modes re-stage the scenes' edge actors

- **Date:** 2026-10-13
- **Subsystem:** field scenes (`src/game/wide_staging.cpp` kSites; hooks in
  `MoveScript_Step`, the flow pass's `0D`, `EventScript_Op`;
  [`wide-staging.md`](wide-staging.md))
- **Tier:** Sensible - the wider picture's own consequence, fixed the way
  Capcom fixed it on the PSP.
- **Original behaviour:** in the 320-column view, actors a scene starts,
  parks or ends just past the view's edge are not seen. Under DIV-0041's
  wider picture 36 such rows show in the 16:9 band (SS's static scan),
  among them actors standing at the edge through a conversation and
  actors popping in or out there.
- **New behaviour:** in the wide modes only (`Widescreen_Live()` not 0), 18
  site rows (22 of SS's rows and one of PD's) move the actor's start out past
  the live picture's edge, or extend its last walk before a hide or an end
  past it, by `ceil((columns + 12 - beyond) * 10 / pps10)` steps (capped by
  the steps to the picture's top), and lengthen the walk by the same steps,
  so it ends where the original's did. Three rows walk the added steps at
  the fastest speed, because the actor's next wait is for a clock value
  close to the walk's. Narrow is the original's. A row whose script bytes
  differ (a disc-built or PSP-data install) is left original and logged.
- **Rationale:** the owner's call of 2026-10-13 (`ultrawide-round.md` 4):
  fix properly at 16:9, park farther. It is the shape Capcom used on the
  PSP for its 32 columns (PD: 9 of SS's 56 band rows in 7 scenes, starts
  and exits moved out with the walk lengthened, no hide added).
- **Also in the PSX version?** The PSX has the 320 view, where the original
  staging is never seen; the PSP has its own re-staging of some of these
  sites (its data, not ours).
- **Verification:** `tools/wide_staging_check.py` (each row's address,
  hash, ops, and the rule's point outside the picture at 53 and 120 by SS's
  camera, 0 problems); `BOF3X_SHADOW=wide_staging` (the rule, the table,
  narrow applying nothing, the engine on six real rows; 0 mismatches) and
  ten controls refused. **Not played;** EB-2's census is the check.
- **Reversible?** `BOF3X_WIDE_STAGING=0`, or narrow.

### E. Case zero takes the PSP's staging in the wide modes

- **Date:** 2026-10-13
- **Subsystem:** area 175, chapter 7 (`Scena07_Scene2`), the party's scene
  scripts `+0x18[4]` and `[5]` (`wide_staging.cpp` kCaseZero)
- **Tier:** Sensible - Capcom's own later staging.
- **Original behaviour:** the pair walk 24 steps west at clock `0x1B` and
  wait at screen x -40, in the 16:9 band (the owner's capture, 2026-10-13);
  later they walk the same way again and 18 / 20 steps east.
- **New behaviour:** in the wide modes, as the PSP stages it:
  - each west walk is 3 steps shorter and is followed by a walk north;
  - the east walk is preceded by the same walk south, and is 3 shorter.

  The pair leave the band for the 4:3 view's top, and the route ends where
  it did. Two departures from the PSP's 6 north / 6 south (revised
  2026-10-14, [`wide-staging.md`](wide-staging.md) 5a):
  - each rewritten walk takes exactly the original's frames: the 3 lost
    steps' frames are spent on the added walk, partly at the actor's speed
    and the rest at the fastest whole speed, out of view;
  - the north walk goes far enough to take the drawn rectangle's bottom
    above the top row (the rectangle is read at run time). The PSP crops 12
    rows, which hid the pair; we keep 240.

  Narrow is the original's.
- **Rationale:** the owner's call (`ultrawide-round.md` 4a): copy the PSP's
  staging, written as a change of the original's steps in our code, cited
  to PD ([`ultrawide-staging-psp.md`](ultrawide-staging-psp.md) section 2,
  rows 53 and 54). Game timing stays Capcom's: EB-2's hash
  ([`ultrawide-census-3.md`](ultrawide-census-3.md) 3.2) found the PSP's
  longer walk moving `dolphinBoss`'s recorded route.
- **Also in the PSX version?** The PSX is the original's; the PSP (JP and
  EU alike) is the model. The PSP's own walk takes 3 steps' frames longer
  than the original's.
- **Verification:**
  - the scripts' address and hash at the arm;
  - the engine's self-test: each walk's frames equal the original's at
    speed indices 1..5 and feet rows -10..120; north clears row 0 where the
    frames allow; south equals north; the speed is put back; only in area
    175;
  - two controls refused;
  - the check tool's replay of the route, and the lift constant measured.

  **Not played** since the revision; EB re-runs `dolphinBoss`'s hash.
- **Reversible?** `BOF3X_WIDE_STAGING=0`, or narrow.

### F. At 21:9, a field object past the 16:9 view is not drawn in a scene

- **Date:** 2026-10-13
- **Subsystem:** `Sprite_Draw` `0x5935B0` (ours), one call after its cull
  and bit 7 (`wide_staging.cpp` HideSite)
- **Tier:** Sensible - draw-only.
- **Original behaviour:** no picture wider than 320 columns; under DIV-0041
  at 21:9, actors parked or passing in the 120 columns past the 16:9 view
  are drawn during scenes.
- **New behaviour:** at `BOF3X_WIDE=2`, a field object, extra object or
  non-leader party member whose drawn rectangle lies wholly past -53 or
  373, while a scene holds the leader (the census's scene mask, bits
  1..8) in a field scene (`Game_Mode` 2, or 4 - the field with a message
  open, `GameMode4_Run` `0x495E60` - no world-map record for the area
  by `WorldMap_RecordIndex`, not the attract demo's chapter 16), and that
  no exception names, is not drawn. Bit 7 and every field
  the original writes are unchanged; only the primitive is skipped. 16:9
  and narrow are unchanged.
- **Rationale:** the owner's call (`ultrawide-round.md` 4, layer 2): a
  simple hide for what is still visible in the 21:9-only part of the bands.
- **Also in the PSX version?** No wider picture there.
- **Verification:** the self-test's fuzz of the decision at the band
  edges, the mask on and off, the leader and an exception excluded (and a
  sweep of x -200..520); controls refused; the census's `hidden` column
  records the same decision for EB-2. **Not played.**
- **Reversible?** `BOF3X_WIDE_HIDE=0`, or 16:9.

**`docs/README.md`**, a row:

| [`wide-staging.md`](wide-staging.md) | Group ST of the ultrawide round (2026-10-13): the wide modes' re-staging - 18 site rows (22 of SS's 36 16:9-band rows and one PD site) moving a start out or an exit on as a rule in the columns, case zero as the PSP's staging, and the 21:9 draw-only hide with the census's `hidden` column; `tools/wide_staging_check.py` checks every row against the executable. DIV D, E, F (drafts). BUILT, not played. |

**`docs/STATUS.md`**, the round's line: ST - the staging built
(`wide_staging.cpp`): 18 rows, case zero, the 21:9 hide; 14 band rows left
for the owner (section 3); headless only.

**`docs/HANDOFF.md`:** EB-2's census after UC2 and ST merge - the 16:9 band
should hold none of the 22 re-staged rows, the 21:9 band nothing visible in
a scene (`hidden yes`); the owner to look at case zero and decide the 14
rows of section 3, the two sweeps (section 8), and whether the gated rows'
pauses at 21:9 (up to 136 frames, section 4) want the fast speed too. EB-1's
new sites go in as section 7 says.

**`docs/IDEAS.md`:** the sweeps' draw-only mapping (section 8).

**Defects:** none numbered. Found in passing: none.
