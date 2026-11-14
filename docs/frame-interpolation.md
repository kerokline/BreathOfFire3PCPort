# Frame interpolation: one in-between frame (prototype)

**Status:** PROTOTYPE (2026-11-13, branch `render/interp-prototype`). Off by
default (`BOF3X_INTERP=1` turns it on), **unledgered**, not for a release
until it has a `DIVERGENCE.md` entry and the open faults of section 6 are
closed or accepted. Nothing the game computes changes, by the state hash on four routes; the
music staging buffer's timing does (section 5). The owner's ask the same day: the game's logic runs at 29.97
frames a second and the machine has headroom for twice that; show one
interpolated frame between each two, no more.

## 1. The idea, and why it adds no delay

The game counts frames and reads no clock (DIV-0047), so anything done at
present time changes nothing it computes.

`Gpu_PutDispEnv` presents the frame recorded on the *previous* pass of
WinMain's loop, and `Gfx_DrawOTag` then records the next. So half a period
after frame N-1 is shown, frame N is already whole in the shim's command
list. The in-between is frame N's commands and textures with the vertices of
its draws moved halfway back to the matching draws of frame N-1, shown from
the wait loop at `deadline - g_frame_ms / 2`. Frame N is shown at its own
time, as ever: **no frame is shown later than it was.** (An earlier estimate
in the session of 16.7 ms of added delay was wrong.)

Colours, textures and draw order in the in-between are frame N's. Only
`x`, `y`, `z` and `rhw` are moved.

## 2. Where the code is

| File | What |
|---|---|
| `src/render/interp.h`, `interp.cpp` | `Keep` (a copy of the frame just presented, with a look key per draw) and `Build` (the in-between's vertices). All of the matching. |
| `src/render/render_shim.cpp` | `Present` calls `interp::Keep`; `PresentInBetween` builds, presents, and puts the frame list back as it was, so `BOF3X_FRAMEHASH` is untouched. Logs a stats line every 150 in-betweens, with the matcher's time (mean and most) and how many order tables were dropped. |
| `src/render/render_d3d11.cpp` | `PresentInBetweenFrame`: `RunFrame` and `Show`, without the sweep of released textures or a pending scale change. Not installed under the satpixie look (section 6). |
| `src/game/platform/magic_080.cpp` | WinMain's loop: the in-between is due at speed 1, not paused, no shot pending; shown in the wait loop. `InterpDump`. |

Switches (all tooling, none in the launcher):

| Variable | What |
|---|---|
| `BOF3X_INTERP=1` | on |
| `BOF3X_INTERP_MAX=<px>` | the cap on a vertex's move, target pixels (default 96) |
| `BOF3X_INTERP_DUMP=<dir>` | writes `<Frame_Counter>_real.bmp` and `_mid.bmp` for two passes in every `BOF3X_INTERP_DUMP_EVERY` (default 100). The directory must exist: the first four writes log `NOT saved` otherwise. Logs nothing when unset. |
| `BOF3X_INTERP_DEBUG=<Frame_Counter>` | logs that in-between's every decision (`interp-dbg` lines: `DECIDE`, `VERT`, `FAIL`, `RUN`, `CUT?`) |

## 3. How draws are matched

A draw's *look* is its state, surface and per-vertex texture coordinates
(colours when untextured); its *shape* is state and box size in steps of 4.
The order of the rules is what the day's faults taught, each named in the
code beside its rule:

1. **Votes.** Each surface's commonest motion, from each draw's nearest
   look-alike. A draw wants to agree (within 5 px) with its surface's vote,
   with what the last paired draw of its surface did, or with what the draw
   before it in the list did.
2. **By order.** The longest common run of looks between the two draw lists
   (prefix, suffix, then a table). A draw's partner by order is taken when a
   list neighbour's partner by order moves the same way (2 px) - of any
   surface, because a window's letters are a texture each. A common look
   (more than two in the kept frame) is taken by order only as far as its
   surface or the draw before it is known to move: a field's look-alike tiles
   pair a row off by order. It gives way to a candidate that agrees better.
3. **By motion.** Otherwise the candidate of its look or shape closest to
   what it wants, when within 5 px, or when clearly better than the next.
4. **A lone look-alike** (two or fewer) within a quarter of the cap.
5. **Rigidity.** A pair is refused when its vertices move differently by
   more than a quarter of the polygon's longest extent (12 px at the least):
   clipped polygons, wrong pairs.
6. **Follow.** A textured draw with no pair moves vertex by vertex with the
   corners it shares (half a pixel) with a paired draw of its surface near
   it in the list; its other corners stay.
7. **Runs.** A run of the list (consecutive draws of one surface and blend)
   whose touching draws go different ways is shown unmoved.
8. **Cuts.** When a quarter of the draws have no pair, or the runs' breaks
   are a tenth of them, the whole frame is shown unmoved.

## 4. How it was checked

`BOF3X_INTERP_DUMP` gives triples: real N, the in-between, real N+1. Two
checkers (kept in the session's scratch, `bof3x-runs/interp1113/sweep`, not
in the tree - worth moving to `tools/` if the work goes on):

- **Envelope**: in-between pixels outside the range of both neighbours'
  neighbourhood (41x41 for holes and strays, 13x13 for thin seams, which is
  noisy on fast motion).
- **Black**: pixels black in the in-between only.
- **Shift**: 32x32 blocks where the in-between is further from *both*
  neighbours than they are from each other - a part of the picture moved the
  wrong way.

Sampling one frame in twenty with the 41x41 envelope **missed** thin seams
and scene cuts; the owner saw both in play before the check did. One frame
in six with all three measures found them. Read flagged frames by eye: a
fast window slide is flagged and correct.

A misreading to avoid: in a strip of the three pictures side by side, two
windows at the edges of adjacent panels read as one box of scrambled digits.

Recipes swept (the last state of each, flagged in-betweens of those
sampled; a flag is not a fault):

| Recipe | Sampled | Flagged | Seen by eye |
|---|---|---|---|
| bridgeWalk (one in six) | 339 | 49 thin | hairlines and the far-plank seam, section 6 |
| bridgeWalk, wide mode 1 | 106 | 0 | |
| camera_rotate, and wide mode 2 | 70 | 0 | |
| cs_53 (first pass only) | 28 | 0 | |
| shop (one in six) | 509 | 7 | the message bar, section 6 |
| menu_screens | 82 | 1 | a fast slide, correct |
| combat | 131 | 1 | a window's edge line |
| worldMapAndAreaTransition | 107 | 1 | looked correct |
| dragonTransform | 216 | 0 | |
| cutsceneAndNue | 367 | 5 | the equip screen looked correct; not all read |
| minecartToZombieDragon | 406 | 4 | not read |
| bossAndFlash | 274 | 10 | window edge lines; not all read |
| attract_cycle (first pass only) | 625 | 5 small | not read |

`desertSummary.txt` ends FAILED at recipe frame 2771 (after its F9 lines)
with the switch on; not checked with it off.

`bridgeWalk.txt` had no `# save` line and ran on whatever slot 0 held, three
times in one day; it names `bridgeWalk` now.

## 5. Logic neutrality

Measured 2026-11-13 with the state hash (`docs/state-hash.md`): per route two
runs with `BOF3X_INTERP=0` (the references) and one with `BOF3X_INTERP=1`,
`tools/statehash.py check REF REFB NEW`. Run beside another session's proof
chain, not on a quiet machine.

| Route | Ticks compared | Result |
|---|---|---|
| bridgeWalk | 2,049 | identical on every compared page and tick |
| combat | 2,625 | 17 pages differ at 1 tick, all `Music_Staging` |
| shop | 3,137 | 17 pages differ at 21 ticks, all `Music_Staging` |
| bossAndFlash | 5,505 | 17 pages differ at 10 ticks, all `Music_Staging` |

Every page outside the music staging buffer is identical on every tick of
all four routes: nothing the game computes changed. `Music_Staging` is filled
by wall time, and the two references disagree with each other on the same 17
pages (shop: at 6 ticks), so `check` compares it only at the ticks where the
references happened to agree. It differs at **more** ticks with the switch on
(shop: 21 against 6): the in-between's present runs in the wait loop between
`Sound_Tick` calls, so the buffer's refill lands on the other side of a tick
boundary more often. That is timing of the audio stream, not game state -
but nobody has listened for it. **Owed:** the owner's ear on music with the
switch on, and a WAV A/B (`docs/wav-dump.md`) - IDEAS I68.

Three pages (`0x6BC000`, `0x7CC000`, `0x7DE000`) differ between the
references at every tick and are never compared, as in every proof.

## 6. Open faults, and where to start

1. **The far planks of the rope bridge** (bridgeWalk, Frame_Counter 648 and
   about 45 other sampled frames): a step in the deck with a light gap, at
   the far end. In the vertex log one edge of a plank moves about 5 px
   further than the other between frames, plank after plank in turn - read
   as the game drawing far planks two as one from frame to frame, **not
   confirmed**. Tried and withdrawn: (a) rigidity by a quarter of the
   polygon's *smaller* side - the seam went (49 flagged to 26) but the
   town's ground cracked as the view turned (shop, Frame_Counter 444); by a
   third, the town held and the seam came back; (b) welding corners that
   meet to their mean - no gain on the hairlines, and holes in the sea
   (bridgeWalk 852), cause not found. **Start here:** treat the planks as a
   group - when one piece of a run deforms, show that run unmoved - rather
   than refusing planks one at a time; and first confirm what the game does
   to far planks (the mesh's draw code, or `VERT` lines for one plank over
   several frames).
2. **Hairlines between planks**, one pixel, the sea showing through: pieces
   that meet along an edge without sharing corners.
3. **A message bar whose text changes outright** (shop, Frame_Counter 1860,
   "Def1 Wgt0" to "Buy an item"): the new letters slide in from old ones for
   one in-between. Limiting the lone look-alike rule did not cure it; the
   decision log of that frame has not been read.
4. **One dark stray** of 44 pixels at bridgeWalk Frame_Counter 427, after
   the right-edge column was fixed at the other nine places. Not read.
5. **Draws that left the screen** are not drawn in the in-between (they are
   not in frame N), so a leaving edge can gap. Not seen yet.
6. **Music timing** with the switch on (section 5): the staging buffer's
   refill moves against the tick more often. Listen, and run a WAV A/B.
7. **Vsync and pacing.** The two presents are evenly timed and not locked
   to the display (`BOF3X_VSYNC` is off by default). Not looked at.
8. **Not swept:** frame-buffer grab effects, the FMVs, the ending, wide
   modes beyond two routes, the 64-bit build.
9. **Cost.** Measured 2026-11-14 on bridgeWalk (about 800 draws a frame):
   the matcher takes 1.0 to 1.4 ms an in-between on average, 2.8 ms at
   most, on the game's thread inside the wait. The order table is n x m at
   worst (capped at 8 million cells); past 8 ms of filling it is dropped
   and the middle of the list pairs without an order, so a slow table
   cannot make the next frame late. The stats line counts the drops; none
   on bridgeWalk or shop.
10. **The satpixie look.** It counts presents for its animation and blends
    each present with the one before; an in-between would run both at twice
    their rate. Under `BOF3X_PRESENT=satpixie` the in-between is off (one
    log line). Making it work means counting real presents apart from
    in-betweens, and deciding what the feedback should see.
11. **Captures between presents.** From the in-between to the next real
    present the target holds the in-between, so F11 (`SaveFrameBesideDll`)
    pressed in the second half of a period saves an interpolated picture.
    Scripted shots are safe: no in-between is due while one is pending.

Before it can ship: the `DIVERGENCE.md` entry, a launcher option written in
Simplified Technical English, the checkers in `tools/`, the debug logging
trimmed, and the owner's eye over battles and menus (rows in
`owner-review.md`).
