# Widescreen — the plan

**Status:** PLANNED (2026-09-23). Step 6 of [`display-overhaul.md`](display-overhaul.md)
§5, for a session of its own (the owner). Nothing built.

## 1. What the owner decided

- **426 x 240, no crop.** The PSP release's 384 x 216 (320 + 2 x 32 columns,
  12 rows cut top and bottom, [`psp-widescreen.md`](psp-widescreen.md)) was
  offered and turned down: "I don't think I want to crop". 426 is 240 x 16/9,
  rounded down to an even number: **53 extra columns a side**, every row kept.
- **A 21:9 monitor stays pillarboxed.** 426 x 240 at the largest integer
  scale, black at the sides - no wider view.
- DIV-0036's rule (a window's k from the launcher, borderless the largest k
  that fits) "will get re-evaluated" with widescreen: at 426 x 240 the owner's
  3440 x 1440 fits k = 6 (2556 x 1440), full height.

## 2. The approach: Capcom's, from the PSP

Two ways to widen are on record:

- **Shift the primitives (the PSP's, chosen).** The game keeps drawing in its
  0..320 space with the projection centre at (160, 120); every primitive's x
  is moved by +53 on the way to the screen, so the original picture sits
  centred in a 426-wide frame. Anything the game already draws past its
  edges - 3D geometry, scrolling layers - fills the side bands for free, and
  anything centred stays centred. What needs work is only what was cut or
  anchored at the old edges: culls, full-frame fills, edge-anchored UI.
  Capcom did exactly this on the PSP (`+32` in all 21 primitive converters,
  51 sites, psp-widescreen §2.3) and touched six other things (§3a there).
- **Widen the coordinate space (not chosen).** Move the projection centre to
  213 and add the offset to every centred UI element - what the peer project
  `bof3ext` does (`src/hooks/widescreen_patches.ixx`, read for its site list
  only; `CLAUDE.md` rule 5). It needs every UI position in code found and
  moved, and misses show up as off-centre menus. Its site list is still the
  best inventory of PC addresses that know the screen is 320 wide (§4).

A side band the game draws nothing into stays black: the failure mode of the
chosen approach is an internal pillarbox, never a broken layout.

## 3. The work, in order

### 3a. The frame (ours, the backend)

- The render target 426k x 240k; `D3d_ScaleX/Y` stay k; the scene vertex
  shader adds 53k target pixels to x (`render_d3d11.cpp`, next to the
  `PIXEL_OFFSET` of the edge-pixel A/B).
- The game's clears, Blts and full-target operations: check each against
  the new width (`render_shim.cpp` - the back buffer is still the game's
  640 x 480 surface as it sees it; only the target grows).
- The present: 426:240 at the largest integer scale; DIV-0036's k rule
  re-evaluated with the owner.
- Behind `BOF3X_WIDE=1` for the survey; the launcher and the ledger entry
  once it holds up.

### 3b. The culls - what pops in at the edges

The PSP widened only the two culls that visibly pop (terrain and the area
map's frame pass) because its 32 columns stayed inside every other margin.
**53 does not**: three ranges Capcom left alone are now too tight. Kept
screen-x intervals, from psp-widescreen §3a / §3b and `bof3ext`'s list:

| Cull | PC | Now | Needs to reach | Note |
|---|---|---|---|---|
| Terrain | `MapView_Build` `0x56EC00` (`0x56EDFA` / `0x56EE14`), **ours** | `[-50, 370]` | about `[-117, 437]` | PSP widened by 46 for 32; ours - patch our source |
| Area-map frame pass, wide | `AreaMap_FrameAreaBD` `0x510780` (`0x51097E` / `0x510991`) | `[-200, 520]` | `[-252, 572]` | PSP +31 for 32 |
| Area-map frame pass, narrow | same (`0x5109BB` / `0x5109D2`) | `[-50, 370]` | `[-102, 422]` | PSP +31 for 32 |
| Sprite | `0x4CF319`, `0x4FF6A3`, `0x571366` | `[-60, 380]` | beyond `[-53, 373]` + sprite width | 7 px of margin left: pops |
| `[-40, 360]` | PSX `80161ef4`; PC twin unread | `[-40, 360]` | wider | inside the new view: pops |
| "unknown" | `0x5054E3` / `0x5054FA` | `[-20, 340]` | wider | inside the new view: pops |
| Object | `0x4CEC09`, `0x5700D1` | `[-100, 420]` | - | 47 px margin, probably fine |
| Object 2 | `0x570319` / `0x570333` | `[-80, 400]` | - | 27 px, watch |
| `Sprite_Draw` | `0x59360B` / `0x593615` (int16), **ours** | `[-64, 384]` | - | 11 px, watch |
| `0x59293A` | in `bof3ext`'s list | ? | ? | unread |

Every one is read before it is changed: many sit in functions that are ours
now, where the change is to our source, not to bytes.

### 3c. Full-frame fills and fades

Anything that fills `(0, 0, 320, 240)` - fades to black, flashes, the
battle transition - leaves the side bands unfaded: a fade to black with two
bright strips. The PSP widened these to `(-32, 0, 384, 240)` (47 sites);
ours become `(-53, 0, 426, 240)`. Known: `mode_flow.cpp:171`,
`save_menu.cpp:391` (ours, in logical units), the black fade `0x4957CF`
(`bof3ext`). The rest are found by the survey - a fade with bright sides is
unmistakable - or by a scan for `320` paired with `240` near a fill call.

### 3d. Edge-anchored UI

Centred UI needs nothing under the chosen approach. What was placed by its
distance from a screen edge moves outward by 53, as the PSP moved it by 32:

- `MsgBox_PlacementTable` `0x66AE10` entries 3..6, the side placements:
  145 / 226 to **92 / 279** (the PSP's 113 / 258). Centre placements stay.
- Candidates from `bof3ext`'s list, each to be read and judged - under the
  primitive shift most of them are centred and need nothing: the title's
  menu panel and items (`0x588C26`..`0x588C76`, `0x58893B`, `0x588A19`), the
  save menu (`0x5881AC`..`0x5887E8`), the field menu's panels (`0x589EB7`,
  `0x589F9C`), overworld popups (`0x571C85`), `0x462560`, `0x573CE0`,
  `0x576960`.
- Screen-wide art: the menu backdrop repeats a 32-column pattern a fixed
  number of times (`0x575732`); widened, it needs two more columns a side or
  its edges show. The sky (`0x4112A9`). The title and the full-screen
  pictures are 320 wide by nature: they stay pillarboxed inside the frame.

### 3e. Survey, then the owner's routes

1. The survey build: 3a plus the terrain and area-map culls. The attract
   sequence captured wide (`input_run.py` recipes; `analysis/shots/`), the
   field in the recipes' saves, a menu walk, a fade. Every pop, bright
   band and off-edge element goes in §5 below.
2. Fix by class (3b, 3c, 3d), one ledger entry for the view and one per
   re-authored element class, as the PSP's were.
3. The owner plays with `BOF3X_RECORD`; each route's leftovers become the
   next list, like the takeover rounds.
4. Default on or off: the owner, once it holds up.

## 4. Checks

- **The oracle and the frame hash do not change** with the view: the shift
  is in the backend and a cull's margin changes what is drawn, not logic -
  except where a cull gates logic (an object update skipped off-screen).
  Run both with `BOF3X_WIDE=1` once; a difference there is a finding.
- The 55-shot attract A/B against the 320 view, cropped to the middle 640
  columns at k = 2: the centre must be identical; only the bands differ.
- The owner's eye on everything else.

## 5. Open

- Found by the survey (empty until it runs).
- DIV-0036's k rule at 426 (the owner).
- The two PSP `SetGeomOffset` callers without PC twins (`(160, 144)`,
  `(?, 185)`, psp-widescreen §5) - an unmoved projection there would put
  something off-centre by 53.
- The FMVs stay 4:3 (640 x 480 at an integer scale, DIV-0035); nothing to do.
