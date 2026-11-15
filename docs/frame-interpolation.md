# Frame interpolation: one in-between frame (prototype)

**Status:** EXPERIMENTAL (prototype 2026-11-13; ledgered 2026-11-14 as
DIVERGENCE DIV-0230). Off by default; the launcher's Settings page has the
box "Smooth motion: 60 frames each second (experimental)" (`interp=1` in
`bof3x.ini`, which goes out as `BOF3X_INTERP=1`). The open items of section 6
want play more than code. Nothing the game computes changes, by the state hash on four routes and the ten proof routes; the
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
| `src/render/render_d3d11.cpp` | `PresentInBetweenFrame`: `RunFrame` and `Show`, without the sweep of released textures or a pending scale change. `Show` advances the looks' clock by half a game frame for an in-between and for the present after it. |
| `src/game/platform/magic_080.cpp` | WinMain's loop: the in-between is due at speed 1, not paused, no shot pending; shown in the wait loop. `InterpDump`. |

Switches (the first is the launcher's `interp=`, DIV-0230; the rest are tooling):

| Variable | What |
|---|---|
| `BOF3X_INTERP=1` | on |
| `BOF3X_INTERP_MAX=<px>` | the cap on a vertex's move, target pixels (default 96) |
| `BOF3X_INTERP_DUMP=<dir>` | writes `<Frame_Counter>_real.bmp` and `_mid.bmp` for two passes in every `BOF3X_INTERP_DUMP_EVERY` (default 100). The directory must exist: the first four writes log `NOT saved` otherwise. Logs nothing when unset. |
| `BOF3X_INTERP_DEBUG=<Frame_Counter>` or `<first>-<last>` | logs those in-betweens' every decision (`interp-dbg` lines: `FRAME` at the head of each, then `DECIDE`, `VERT`, `FAIL`, `RUN`, `CUT?`) |

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
   whose touching draws go different ways is shown unmoved. Two touching
   draws that share corners (half a pixel) go different ways when a shared
   corner comes apart in the in-between; two that share none, when their
   first vertices move differently (4 px, or half the larger motion). First
   vertices that differ between draws whose corners hold count only in a
   run where some corner does come apart: a mesh that changes shape (the
   rope bridge's deck, section 6 item 1) has none, a mesh paired wrongly
   (a spell's sphere) has some.

   **A line of letters** is one run too. The font gives each letter a
   surface of its own, drawn with the same texture coordinates and the same
   box, so by surface alone each letter was a run of one and nothing held a
   line together. A draw that follows one of another surface with the same
   coordinates and the same box, both upright on the screen, goes on the
   run, and such a run takes no break: one letter going its own way is a
   letter that changed, and the whole line is shown where frame N has it.
   Text that scrolls or a window that slides keeps its letters, they pair
   by look and by order and go one way, and still moves. "Upright" keeps
   the sea out: its tiles are a surface each with the same coordinates and
   box too, and without it one break showed a run of them unmoved and left
   holes in the sea (bridgeWalk 1452, the first form of the rule).
8. **Cuts.** When a quarter of the draws have no pair, or the runs' breaks
   are a tenth of them, the whole frame is shown unmoved.

## 4. How it was checked

`BOF3X_INTERP_DUMP` gives triples: real N, the in-between, real N+1. Two
checkers (kept in the session's scratch, `bof3x-runs/interp1113/sweep` and
`bof3x-runs/interp1114/s2/check3.py`, not
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

Recipes swept (the state of 2026-11-14 after the runs rule changed, section
3 rule 7; flagged in-betweens of those sampled; a flag is not a fault).
"Same" means the per-frame numbers equal those of the build before the last
change of the rule, frame for frame:

| Recipe | Sampled | Flagged | Seen by eye |
|---|---|---|---|
| bridgeWalk (one in six) | 351 | 3 thin | the character's own animation (732, 1410, 1860); the deck is whole |
| shop (one in six) | 523 | 14 | same as before the rule changed; the message bar's letters, closed later the same day (section 6, 3) |
| bossAndFlash (one in ten) | 552 | - | compared frame by frame with a build of the old rule: the sphere equal, six frames better, the beam changed (section 6, 12) |
| combat | 263 | 7 | same |
| menu_screens | 162 | 7 | same; fast slides |
| dragonTransform | 427 | 10 | same but one frame (1430): the gene cursor mid-move, correct, a timing difference between runs |
| cutsceneAndNue | 711 | 13 | same; not all read |
| worldMapAndAreaTransition | 213 | 4 | same |
| camera_rotate | 142 | 0 | |
| minecartToZombieDragon | 811 | 7 | same; not read |
| campingFishing | 519 | 20 | 1870, 2030, 3150 read: window slides and the scene's dark edge |
| balioAndSunder_1 | 633 | 12 | 2740, 3320 read: battle animation |
| desertToManmo | 451 | 8 | 2660, 4450 read: correct |
| adultFight | 320 | 2 | 680 read: battle animation |
| radiationRoom | 256 | 1 | 1390 read: correct |
| vaccinationAndFireScene | 331 | 2 | not read |
| garrBlocks | 133 | 2 | 990 read: correct |
| ninaWalkBehindBlock | 72 | 4 | 550 read: correct |
| tarmanLogic | 213 | 1 | 300 read: correct |
| yraallRoad, maekyssGorge, myriaHall, fairyVillage, liftScene, deisCave, ending17, lighthousePowerMiniGame, field_view | 64 to 498 each | 0 | |
| shisuMinigame (first form of the rule) | 458 | 2 | 1840 read: correct |
| overheatAndDeepSea (first form of the rule) | 433 | 0 | |

All but bridgeWalk and shop are one in ten. No gap was found in the frames
read; not every flagged frame was read. Not run again since the rule
changed: the two wide-mode routes, cs_53 and attract_cycle (2026-11-13: 0,
0, 0 and 5 small flags).

`desertSummary.txt` (recipe frame 2771), `overheatAndDeepSea.txt` (4356)
and `shisuMinigame.txt` (4619) ended FAILED at the same frames with the
switch **off** too: the recipes, not the in-between. Each recording ends on
the F9 that quits the game, and the recorder wrote `wait 1` after it where
the harness wants `quit`; the three end with `quit` since 2026-11-14 (not
run again yet when this was written).

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

**The ten proof routes, 2026-11-14** (`tools/solo_proof.py`'s method with
`BOF3X_INTERP=1` as the variant; the base `main` `651a5ad8` run twice a
route as the references, the candidate the build of `8efb6cfa`; 32-bit
`bof3x.exe`; run at the owner's word without waiting for a quiet machine,
with another session's games beside it, and no load of our own added):

| Variant | Routes identical | The rest |
|---|---|---|
| the switch off | 10 of 10 | - |
| `BOF3X_INTERP=1` | 9 of 10 on the first run | dragonTransform: page `0x7DE000` at 215 ticks (2174..2601); identical on the repeat |

`check --timing`, which compares `Music_Staging`'s 17 pages too, reads the
same on all twenty runs: **the staging buffer is identical at every
compared tick with the switch on**, where the 2026-11-13 runs above had it
differ at up to 21 ticks. Why the two days differ is not measured: the
base moved to `main` `651a5ad8` between them, and so did the machine's load.

The one difference, dragonTransform's first run with the switch on, is
page `0x7DE000`: the page that holds the staging buffer's last `0x378`
bytes (`0x7CC378 + 0x12000 = 0x7DE378`) beside `Snd_BufferDesc`,
`Snd_WaveFormat`, `Music_Events` and `Snd_Device`. It is not one of the 17
timing pages, because it is not wholly inside the buffer. A page hash
cannot say which bytes moved; the repeat run is identical, so it does not
follow the switch on every run, but it was seen only with the switch on
(1 run of 11 on, 0 of 30 off or base). Not shown: that it is the buffer's
tail and not a sound object, and whether anything is heard. The owner's
ear and the WAV A/B stay owed (IDEAS I68).

## 6. Open faults, and where to start

1. **The far planks of the rope bridge** - *closed 2026-11-14.* The deck
   itself lifts every other row of tiles about 5 px on one frame in four
   (Frame_Counter 640, 644, 648, 652 on bridgeWalk); the corners rows share
   agree, and the pairing was right. Rule 7's break test compared first
   vertices, read the lifted rows as going another way, and showed runs of
   two to four tiles unmoved beside moved ones: the step. The test now
   reads shared corners (section 3, rule 7). bridgeWalk one in six: frames
   with a thin seam above 20 px went 47 to 3, the worst from 314 px to 95,
   and the three left are the character's animation. Tried and withdrawn on
   the way: the shared-corner test alone (a spell's sphere showed facet
   streaks, bossAndFlash 3470), and a higher first-vertex bar where corners
   hold (a hairline came back, bridgeWalk 2016).
2. **Hairlines between planks** - *closed 2026-11-14*, the same cause as 1.
3. **A message bar whose text changes outright** - *closed 2026-11-14.*
   Shop, Frame_Counter 1860 ("Def1 Wgt0" to "Buy an item") and 1788 (the
   bar and the list, as the item window closes). The decision log: each
   letter is a surface of its own, and a new letter has no partner by
   order. It took whatever stood at its surface's vote, and the vote came
   from the same letter in some other text on the screen: the "m" of
   "item" agreed with the sliding list's "m" (-64 px) and took, by shape
   alone, the old bar's letter 64 px to its left, and slid in from there;
   at 1788 the list's new letters took old letters by shape (+64), the
   same letter of another word by look (-32, +82), and partners by order
   in another place (-96). Other new letters paired by shape with the old
   letter in their own place, unmoved, which is harmless. Rule 7 could not
   catch the mixed motions: by surface, each letter was a run of one. Now
   a line of letters is one run and takes no break (section 3, rule 7).
   Limiting the lone look-alike rule (rule 4) had not cured it because
   rule 4 was not the rule that paired them. Both frames now show the new
   text whole, and the list still slides. Against the build before, frame
   for frame: shop (one in six) 3 of 523 frames differ, bridgeWalk (one in
   six) none of 343, menu_screens 1 of 159, combat 4 of 262,
   cutsceneAndNue 3 of 701; none worse by the measures, and all but combat
   2070 read by eye: a label "Atk" to "Use", a message bar, a highlight changing colour,
   spell sparkles, all correct. One reads differently: menu_screens 1740,
   a title "'s status" leaving at the top, whose letters had paired by
   order 40 px to one side; it was shown sliding sideways and is now shown
   where the next frame has it, off the screen, one in-between early.
   Withdrawn on the way: the first form without "upright" (holes in the
   sea, bridgeWalk 1452).
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
10. **The satpixie look** runs on the in-betweens too (2026-11-14). It had
    counted presents for its animation and faded its trail by
    `acc_modulate` on each present, so an in-between ran both at twice the
    game's pace. Now `Show` gives it the picture's time in game frames and
    the step since the last picture (1, or 0.5 for an in-between and the
    present after it), and the trail fades by `acc_modulate^step` (0.65
    becomes 0.8062 a half step, logged once). With the in-between off the
    step is always 1: the clock is the present count and the knob goes out
    as it is, so the look is the same bits as before. The grain is fresh on
    each picture shown. Not seen by eye: `SaveFrame` captures the target,
    which the look never touches, so the dumps cannot show it.
11. **Captures between presents.** From the in-between to the next real
    present the target holds the in-between, so F11 (`SaveFrameBesideDll`)
    pressed in the second half of a period saves an interpolated picture.
    Scripted shots are safe: no in-between is due while one is pending.
12. **A beam of flickering rays** (bossAndFlash, Frame_Counter about 2310
    to 2440, 3610 to 3750, 4080 to 4210): each frame draws the rays anew
    at random. The old break test showed the beam unmoved; since the runs
    rule changed (item 1) the rays share their corner and hold together,
    so the in-between shows a fan halfway between two flickers. It read as
    a normal fan in the frames looked at. The owner's eye decides.

Done 2026-11-14: the `DIVERGENCE.md` entry (DIV-0230) and the launcher's
box. Before it leaves "experimental": the checkers in `tools/`, the debug
logging trimmed, and the owner's eye over battles and menus (the row in
`owner-review.md`).
