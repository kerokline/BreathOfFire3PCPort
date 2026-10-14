# The ultrawide round: 21:9, and the staging the wider view exposes

**Status:** STAGED (2026-10-13), not launched. Branch `wide/ultrawide-round` off
`main` `d0d9162a`; briefs in `analysis/ultrawide/` (gitignored, the main
checkout's). Nothing in `src/` changed yet.

The owner asked for this on 2026-10-13. There are two parts:

- **An ultrawide mode,** built the way the 16:9 mode was: extra side bands that
  widen the view.
- **An audit of what the wider view exposes.** The owner's example is a
  capture in the 16:9 mode. During a cutscene, Nina and Momo stand
  "stopped" at the top left, in a spot that was off-screen in the original
  320 view. The audit should also be a code scan, so we find such scenes ahead
  of time rather than one by one in play.

It is staged like the earlier rounds ([`psp-only-round.md`](psp-only-round.md)).
The groups run in parallel in worktrees and self-test headless. They merge one
at a time, and the live checks are held for one batch at the end.

## 0. The decision, and the answer in one paragraph

**The owner's call (2026-10-13): a fixed 21:9 preset.** The picture is
**560 x 240**, which is 240 x 21/9 exactly: **120 columns a side**, every row
kept, and the same primitive shift as DIV-0041. The other options were "fit the
monitor" (573 on the owner's 3440 x 1440) and an uncapped 32:9. On the owner's
monitor, 560 at k = 6 is 3360 wide, which leaves 40 px of black each side. This
replaces [`widescreen.md`](widescreen.md) 1's "a 21:9 monitor stays
pillarboxed". The 16:9 decision (426, no crop) stands as it is.

The 16:9 work already drives most of what depends on the view's width through
one number, the columns: `Widescreen_Columns` / `_Live` / `_Fill` / `_Anchor`.
That number already moves the backend's `pad_x`, every fill, the culls taken
over since 2026-09-30, the slide bounds, the area-map ranges and WA's
anchors. Section 1 lists what is still fixed at 53, or tuned for it. The real
unknowns at 120 are these:

- whether the field's 28-column cell ring covers the view at all (IDEAS I37,
  the ring grown);
- whether the draw pool holds out (DIV-0062);
- the map edges' void;
- and the owner's actual subject, **staging**: actors parked just outside the
  320 view, entrances seen early, and spawns and despawns that pop.

At 120 columns, more than twice as much of the off-screen area shows as at 53.

Stage A runs five groups at once:
- **UW**, the 21:9 frame;
- **UC**, the read of the culls and capacities;
- **BA**, a band census instrument;
- **SS**, the static staging scan;
- **PD**, the PSP's data diff: did Capcom move anything for its 32 columns?

Stage B builds:
- **UC2**, the cull fixes;
- **RG**, the ring grown, if UC says it must be;
- **EB**, the live end batch, which runs BA's census on every recipe at 16:9
  and 21:9.

Stage C, **ST**, fixes the staging once the owner has chosen a fix shape for
each class from SS's and EB's list (section 4).

## 1. What is width-dependent today

Read 2026-10-13 from `src/game/widescreen.{h,cpp}` and its 42 consumers.

| Item | Where | At 53 | At 120 | Group |
|---|---|---|---|---|
| The columns | `widescreen.cpp` `kColumns` (constexpr 53), `Widescreen_Columns` (`BOF3X_WIDE` 0 or 1, anything else Fatal) | 53 | runtime: `BOF3X_WIDE=2` gives 120 | UW |
| Target and present | `display_setup.cpp` `o.pad_x`, `DisplaySetup_ViewWidth`; `render_d3d11.cpp` `g_pad_x`, `MakeTarget` | `(320 + 106) k` | `(320 + 240) k`; DIV-0042's resize and the k snap at 560 | UW |
| Launcher | `config.{h,cpp}` `wide=` (0 or 1), the "Widescreen" box | box | a three-way choice: off, 16:9, 21:9 | UW |
| Fills, anchors, battle tile | `Widescreen_FillX/Width/Right`, `_Anchor`, `_BattleTile`, in about 40 files | columns | follow the number; check none assumes 53 (the fish backdrop's u span, `effect_1g.h`, needs `0 mod 32` at column 0) | UW (review) |
| Slide bounds | `g_slides`, `kColumns` added | +-53 | +-120: does a box that steps 0x20 a frame still clear the edge before it is freed? | UW |
| Area-map frame ranges | `g_wide_lo/hi`, `g_narrow_lo/hi`: `kColumns - 1` | 52 | 119 | UW |
| Terrain cull | `kTerrainMargin` 100 (tuned by eye at 53: 53 + 47) | `[-150, 470]` | at least `columns + 47`; the owner's eye | UW, then EB |
| Cell inset | `kInsetColumns` 3, `BOF3X_WIDE_INSET` 0..14 | 3 | about 7 by the same rule (a ring column is ~20 px). **The inset can only reach the ring's 28 columns** | UC, then RG |
| Menu backdrop | `menu_windows.cpp:1223`, `extra = Live ? 1 : 0` pairs of 64 px | 1 pair a side | 2 pairs a side (`ceil(columns / 64)`) | UW |
| Draw-item pool | DIV-0062: 1,024 doubled to 2,048 for 53; the narrow view peaked at 855 | ample | unknown: EB measures the peak, UC estimates it | UC, EB |
| The culls already ours | `sprite_draw.cpp:102`, `map_cells.cpp:167`, `map_field_objects.cpp:186/344`, `area_w3b.cpp:1333`: original bound + columns | margin kept | margin kept | none |
| The culls left Capcom's values | widescreen.md 3b: the battle field's `0x4CF319` `[-60, 380]` and `0x4CEC09` `[-100, 420]`, the `[-40, 360]` PSX `80161ef4`, `0x5054E3` `[-20, 340]`, `0x59293A` unread | some inside 53 | all inside 120: pops. **Every one is in our code now**, so the change goes to our source | UC, UC2 |
| Fixed art | title, FMV (DIV-0035), the ending's letterbox, full-screen pictures | pillarboxed in the frame | the same, with wider black | none (the owner's eye) |
| Map edges | the camera clamp in `AreaMap_Frame` `0x56E6C0` (ours) | some void seen? | more void: areas barely wider than 320 + 240 | SS (list), the owner |

The rule for every group: **narrow and 16:9 stay bit for bit what they are
today.** Each value is written as the original's value moved by the columns,
or by a rule in the columns, so at 53 it gives today's number. No table of
per-width numbers goes in. The only exception is a value tuned by eye at 53,
such as `kTerrainMargin`. That one keeps its 53 value exactly, and its 21:9
value is the owner's eye at the end batch.

## 2. Stage A (five groups, in parallel)

### UW: the 21:9 frame (builds; `src/`)

- `BOF3X_WIDE=2` gives 120 columns, and `1` stays 53. Any other value is still
  Fatal. `kColumns` becomes the runtime value, and every place it was folded in
  is derived from it: the slide bounds, the area-map ranges, the menu
  backdrop's pairs, and the inset. The inset scales as `ceil(columns / 20)`
  (3 at 53, 6 at 120), capped by what the ring has. UC's measurement then says
  whether that is enough.
- The launcher: `wide=0|1|2`, and the dialog's box becomes a choice. The tool
  side: `attract_run.py` / `input_run.py` / `abc_run.py` pass the width through
  (`--wide 0|1|2`). Today a wide run means a `wide=1` ini.
- The present at 560: DIV-0042's resize and k snap, and the window's minimum
  size. Under DIV-0036 / 0042, the owner's 3440 x 1440 should land on k = 6.
- Self-tests at 0, 1 and 2. A property check, as `effect_1g`'s does: every
  derived value at 53 equals today's constant.
- Ledger: an **amendment of DIV-0041** ("the wide picture comes in two widths"),
  drafted, not numbered.
- Owns: `widescreen.{h,cpp}`, `display_setup.cpp`, `render_d3d11.cpp`,
  `config.{h,cpp}`, the launcher dialog, `menu_windows.cpp`'s backdrop line,
  and the run tools' width option.

### UC: the culls and capacities at 120 (reads; no `src/`)

- **Every screen-x bound in our source.** Search for comparisons of a screen x
  against constants near 0 and 320: `fcomp` twins, `cmp` against `-N` /
  `320 + N`, and the `Widescreen_Fill()`-moved ones. Class each by margin past
  the 21:9 view: fine, pops at 21:9 only, or pops at 16:9 already. Name the
  ones that gate **logic** (an object update skipped off-screen). Those change
  the state hash, and they are a divergence of their own.
- **The cell ring.** Using the field camera's projection (distance, rotation
  and the inset rule in `map_layers.cpp` `Inset()`), find how many screen
  columns 28 ring columns cover. Do it at the distances the recipes' areas use
  (`Camera_Distance`), and at both the near and far extremes. If 560 is not
  covered, RG is needed. The PSP grew the ring to 33 x 58; IDEAS I37 has its
  23 sites ([`psp-widescreen-placements.md`](psp-widescreen-placements.md) 5).
- **The pool.** Use the cells kept per frame against the view's width, from
  the 16:9 runs on disk if any logged the peak, or from the cull geometry.
  Estimate whether 2,048 holds at 120. EB measures it.
- Output: `docs/ultrawide-culls.md`, with a table that UC2 builds from.

### BA: the band census (builds an instrument; `src/`, not a divergence)

The tool that turns "Nina and Momo were stopped there" into a list. It is
`BOF3X_BANDLOG=<file>`, an `instrument = true` module. Each frame, it records
every sprite whose screen rectangle lies **inside the wide picture and wholly
outside `[0, 320)`**. `Sprite_UpdateScreen` `0x588F20` is ours
(`sprite_screen.cpp`): the float screen point is at `+0x74` / `+0x78`. Its
size comes from the frame the draw pass uses. Each row holds:

- the frame, area and chapter;
- the object's index and record, and its sprite or bank;
- the screen x and y, and which side;
- which band it is in: **16:9** (`-53..0` / `320..373`) or **21:9 only**;
- whether the object is **parked**: not moved for N frames, with its movement
  script at an end or a wait;
- whether a scene is running: the scenario bank active or the leader's control
  taken (BA finds the flag that says so).

The rows are run-length merged, so a parked actor is one row with a first and
last frame, not 600. A second, optional channel records **void**: the
fraction of each band's columns that no primitive covered that frame. That is
the map-edge class, as numbers rather than eyes.

Self-test: the logger off is a no-op; on, a fuzz with stand-in screen points
checks the classification at the band edges (-53, 0, 320, 373 and the 21:9
edges). It owns `sprite_screen.cpp`'s hook line and a new `band_census.cpp`.
It also owns `tools/band_census.py`, which sums a log into a per-scene table:
area, frame range, objects, class, and the shot frame nearest to it.

### SS: the static staging scan (reads; the "code scan")

The owner asked for this scan explicitly: find, ahead of play, the scenes where
an actor waits or stops just outside the 320 view. The sources:

1. **The scenario banks.** These are ours: the chapter tables `0x662C80` /
   `0x660B84` / `0x660BD4` ([`scenario-roots.md`](scenario-roots.md)), and the
   `scena_*.cpp`, `area_*.cpp` and `effect_18*` sources that move objects
   during scenes. Find every call that places an object, gives it a walk
   target or starts a movement script, with **literal coordinates**. Also find
   whether a later step hides or removes that object, or whether it stays.
2. **The placement scripts** (`tools/event_scan.py`): objects placed at area
   entry under a scene's flag condition.
3. **The movement scripts** (`tools/movement_scan.py`): scripts that end
   with the object at rest, with no hide op.
4. **The camera during the scene.** Where the bank sets the focus, distance or
   rotation, project the rest position. Classify it as inside 320, in the 16:9
   band, in the 21:9 band only, or beyond 560. Where the camera is not static,
   say "needs the census", not a guess.

Other classes from the same reading:
- **entrance seen early:** an actor walking in from a start that was
  off-screen;
- **despawn pop:** a hide while the object is in a band;
- **spawn pop:** an appearance in a band.

Output: `analysis/ultrawide/staging_static.tsv`, and the method and the summary
in `docs/ultrawide-staging.md`.

**Case zero:** the owner's capture of 2026-10-13. Nina and Momo stand at the
top left of a forest path beside a wooden bridge, while Ryu is spoken to. SS
locates the scene through the dialogue line's area. The line is in the
owner's message in this session; it is copied into the brief, not into a
committed doc (rule 1). Then SS checks that the scan finds it unaided. **A scan
that misses case zero is not done.**

### PD: did the PSP move anyone? (reads)

The PSP widened by 32 columns. If Capcom moved any parked actor or scene
position for it, that change is both a found site and a model for our fix.
PD diffs PSX against PSP:
- the placement scripts;
- the movement scripts;
- the scenario banks' coordinate immediates, using
  `tools/psp_pair.py`'s pairing;
- the camera set-ups.

The code diff ([`psp-code-diff.md`](psp-code-diff.md)) classed code changes,
not the data in the scripts. A coordinate that changed by about 32 or more, or
a hide added, is a hit. "None" is a result too: it means Capcom judged 32
columns safe, which tells us how far SS's 16:9 list should reach. Output: a
section in `docs/ultrawide-staging.md`.

## 3. Stage B (after UW and UC merge)

- **UC2: the culls.** Every bound UC classes as popping moves out by the
  columns, in our source, with its fuzz comparing the original at 0. A cull
  that gates logic is **not** widened here. It is listed for the owner, since
  it changes the game, not the picture.
- **RG: the ring** (only if UC says 28 columns do not cover 560). This is
  IDEAS I37, built: the ring grows to what 120 needs, under the wide view only.
  Narrow, the ring is the original's. It was estimated at about half a week for
  the PSP's 33 x 58. The 21:9 figure comes from UC.
- **EB: the end batch** (live; the owner's machine left alone, or while the
  owner is away). It runs the 51 recipes and the attract cycle at
  `BOF3X_WIDE=1` and `=2`, with `BOF3X_BANDLOG` and shots every N frames
  (`tools/recipe_shots.py`, never by hand). It also runs:
  - **the state hash** at 2 against the `cn_*_orig2` pairs (the view must not
    change logic; a difference is a finding);
  - the 55-shot attract, with the middle 640 columns at k = 2 equal to the
    narrow capture;
  - the pool's peak;
  - a contact sheet per route of every census row's nearest shot.

  The census tables and the sheets go to the owner.

## 4. Stage C: the staging fixes (after the owner's choice)

The list from SS, the census and PD goes to the owner by class, with the fix
shapes to choose from. Each chosen fix gets a ledger entry, and the 16:9 mode
gets the same fix as 21:9:

| Shape | What it does | Fits | Cost |
|---|---|---|---|
| **Park farther** | The scene's rest position moves out past the 21:9 edge (a rule in the columns: past `320 + columns` plus the sprite's width). Data per site, like the `twin` rows | an actor waiting for a later entrance | per site; changes where the actor walks *from* |
| **Hide when parked** | A parked actor wholly outside the 320 view, during a scene, is not drawn until it moves or the scene ends | the generic case, the owner's example | one rule, but it must never hide an actor the original showed |
| **Narrow scene** | The bands draw black for the scene's duration: a per-scene pillarbox | scenes where several things depend on the edge | per scene; the most faithful, the least wide |
| **Leave** | The wider view shows it, and it reads as natural | entrances seen early, mostly | none |

**The owner's call (2026-10-13): two layers.**

1. **Fix properly at 16:9.** Every site visible in the 16:9 bands is
   re-staged per scene: the actor's start, or the point before it hides, moves
   out past the 16:9 edge, and the walk grows by the same amount so its end
   point is unchanged. This is "park farther", and it is the shape Capcom used
   on the PSP (PD, [`ultrawide-staging-psp.md`](ultrawide-staging-psp.md): 9
   of SS's 56 band rows changed in 7 scenes, none by adding a hide). Each move
   is written as a rule in the columns, never as copied script bytes.
2. **A simple hide at 21:9.** Whatever is still visible in the 21:9-only part
   of the bands, or parked and visible there, is not drawn: "hide when
   parked", applied only outside the 16:9 view.

Where the PSP re-staged a site, its fix is the model, scaled to 53 columns
(two of its moves still land inside our bands: area 0 at our 16:9 edge, area
27 in the 21:9 band). Case zero is the exception to "out". There, the PSP
brought the pair **into** the 4:3 view, a staging change rather than a move.
Keeping that or moving them out is still the owner's call. **ST** builds both
layers, and EB re-runs the census afterwards: the 16:9 band should hold no
parked actor, and the 21:9 band no visible one.

## 4a. Stage A's results and the owner's calls on them (2026-10-13)

Stage A is merged: UC ([`ultrawide-culls.md`](ultrawide-culls.md)), SS
([`ultrawide-staging.md`](ultrawide-staging.md)), BA
([`band-census.md`](band-census.md)), PD
([`ultrawide-staging-psp.md`](ultrawide-staging-psp.md)) and UW
([`ultrawide-frame.md`](ultrawide-frame.md)). The owner's calls on what they
found:

- **Split the off-screen flag.** `Sprite_Draw`'s cull also sets bit 7 of an
  object's byte 0, which nine functions of game logic read. Under 16:9 it has
  decided at the widened bound since 2026-09-30, and that was never ledgered.
  From here on, drawing uses the wide bounds and bit 7 is set at the
  original's bound, at every width (UC2).
- **The six culls that still hold Capcom's values widen at both widths**
  (UC2), as DIV-0041 has done so far. That is an exception to section 1's
  "16:9 bit for bit", taken on purpose: four of the six already pop at 16:9.
- **Case zero copies the PSP's staging in the wide modes.** The pair wait
  inside the 4:3 view, as on the PSP. This is written as a change to the
  original's walk in our code, cited to PD, not as copied script bytes.
  Narrow, the scene is the original's (ST).
- **The live batch runs now** (EB-1, the "before" census). EB-2 re-runs it
  after UC2 and ST merge. RG (the ring for far-camera scenes) and the seven
  areas narrower than 560 wait for the census pictures.

## 5. Merge order, ownership, numbering

- Stage A: **UW** first, since everything reads its columns. Then BA, then the
  reading groups' docs (UC, SS, PD), in any order.
- Shared files have one owner each:
  - `widescreen.{h,cpp}`, the launcher and the run tools: **UW**;
  - `sprite_screen.cpp`: **BA**;
  - `docs/ultrawide-staging.md`: SS writes it, and PD adds a section by a
    separate commit after SS merges;
  - `docs/README.md` rows: each group adds its own; keep both sides when
    merging.
- Ledger: groups draft lettered entries, A, B and so on. The coordinator
  numbers them after the merges, from the first free number (0115 at staging, 0122 since main took DIV-0115..0121). Defects are described, not
  numbered: the D-numbers collide, so the coordinator numbers them.
- Every commit `git commit -s` (CLAUDE.md rule 7). No game data in any doc,
  dialogue lines included: describe the scene, do not quote it (rule 1).

## 6. What this round does not do

- No crop (the 16:9 decision holds), and no 32:9.
- No change to fixed art, the title, FMV or the ending's letterbox: they stay
  pillarboxed in the frame.
- Battle scenes: the battle field's culls are in UC's read, and the battles'
  backgrounds at 120 are EB's to look at. Any re-staging of battles is a later
  round.
- The default stays the owner's call: wide mode remains opt-in.
