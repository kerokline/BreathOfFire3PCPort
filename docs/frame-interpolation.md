# Frame interpolation: one in-between frame (prototype)

**Status:** EXPERIMENTAL (prototype 2026-11-13; item 18 closed and two measures added 2026-11-16; figures paired by the game's own object 2026-11-21, section 6 item 20, on branch `render/interp-ident`; a list's highlight no longer slides, item 21, 2026-11-21; ledgered 2026-11-14 as
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
| `src/render/interp_ident.h` | The names: `Sprite_Draw` (`sprite_2.cpp`) tags its primitive with its object, `Gfx_BeginFrame` forgets a pool's tags, `D3d_DrawCellSprite` (`d3d.cpp`) notes the recorded draw's object and where it stands. Nothing unless `BOF3X_INTERP`. |
| `src/game/platform/magic_080.cpp` | WinMain's loop: the in-between is due at speed 1, not paused, no shot pending; shown in the wait loop. `InterpDump`. |

Switches (the first is the launcher's `interp=`, DIV-0230; the rest are tooling):

| Variable | What |
|---|---|
| `BOF3X_INTERP=1` | on |
| `BOF3X_INTERP_MAX=<px>` | the cap on a vertex's move, target pixels (default 96) |
| `BOF3X_INTERP_DUMP=<dir>` | writes `<Frame_Counter>_real.bmp` and `_mid.bmp` for two passes in every `BOF3X_INTERP_DUMP_EVERY` (default 100). The directory must exist: the first four writes log `NOT saved` otherwise. Logs nothing when unset. |
| `BOF3X_INTERP_DEBUG=<Frame_Counter>` or `<first>-<last>` | logs those in-betweens' every decision (`interp-dbg` lines: `FRAME` at the head of each, then `DECIDE`, `VERT`, `FAIL`, `RUN`, `CUT?`) |
| `BOF3X_INTERP_STATS=1` | one `interp-stat` line for each in-between: the draws by the rule that paired them, the runs broken, whether the frame was a cut, the matcher's time, and the time since the present (`after`); `none late` for a pass that drew and shows no in-between (section 6, item 18). `tools/interp_stats.py` reads them. Logs nothing when unset. |
| `BOF3X_INTERP_IDENT=0` | rule 0 of section 3 off: the game's names for its figures are still kept and logged, the matcher does not use them. For an A/B of the rule. |
| `BOF3X_LATE_EVERY=<n>` | not the in-between's own: WinMain takes the pass at each `Frame_Counter` that is n - 1 past a multiple of n as late and does not draw it, as on a machine that cannot keep the period. With a dump of one in six and n = 12, half the triples follow a late pass. |

## 3. How draws are matched

A draw's *look* is its state, surface and per-vertex texture coordinates to
the whole texel (colours when untextured); its *shape* is state and box size
in steps of 4. The whole texel is since 2026-11-15 (section 6, item 13): a
sprite scaled with its distance has coordinates whose last digits follow the
scale.
The order of the rules is what the day's faults taught, each named in the
code beside its rule:

0. **The game's own object** (2026-11-21, section 6 item 20). A figure
   drawn by `Sprite_Draw` is named: the draw carries its sprite object and
   the place the object stands at in the picture (`interp_ident.h`). It
   goes with the kept draw of the same object, by the step of that place,
   whole, before every other rule - whatever its cel, texture and box did.
   A kept draw with a name is no other draw's candidate. Rules 3's two
   forms of "a cel that changed" still serve what has no name.
1. **Votes.** Each surface's commonest motion, from each draw's nearest
   look-alike - and from its looks drawn once in each frame alone, where it
   has any (section 6, item 15: a row of look-alike pieces, moved more than
   half a piece, has its nearest look-alike a piece over). A draw wants to
   agree (within 5 px) with its surface's vote,
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
   **A cel that changed** - a candidate by shape alone that is clearly the
   best and agrees with nothing - does not move by the step between the two
   boxes: it takes the motion its kept draw was shown with the frame before,
   and none when that draw had no pair (section 6, item 14).
   **A cel that changed its box and its texture** has no candidate at all,
   by look or by shape: a figure's new cel is another surface. It is found
   by its place in the list - the kept draws between the partners by order
   of the nearest draws before and after it (four looked past each way,
   since a tile that came in at the picture's edge can be sorted in beside
   it), the nearest of them within half the cap - and carries that draw's
   motion the same way (section 6, item 17). It is tried only when every
   rule above and rule 4 gave no pair.
4. **A lone look-alike** (two or fewer) within a quarter of the cap.
   **A leap among the still** (2026-11-21, section 6 item 21) is refused
   by rule 3's pair and by this one: a pair that is not the draw's partner
   by order, that moves an upright textured draw further than a sixth of
   the cap, while of the 24 draws before it in the list and of the 24
   after it some have a partner by order that leaves them still (a quarter
   of the leap at most) and none has one that goes the leap's way. The
   draw is shown where frame N has it.
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

`BOF3X_INTERP_DUMP` gives triples: real N, the in-between, real N+1. In the
file names that is `N_real`, `N_mid`, `(N+1)_real`: `N_mid` is shown after
frame N (read the other way round on 2026-11-15, and a jug then seemed to
move backwards). `tools/interp_holes.py` is the envelope measure below over
a dump directory. `tools/interp_stats.py` and `tools/interp_motion.py` are
of 2026-11-16 (section 4a). Two
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

## 4a. Two more measures (2026-11-16)

Every fault the owner saw in play on 2026-11-15 (section 6, items 13, 14,
17) was one the envelope cannot see: a thing drawn whole, a few pixels from
where it belongs. And a picture dump of one triple in six leaves five in six
unread.

**`tools/interp_stats.py LOG...`** reads the `interp-stat` lines
(`BOF3X_INTERP_STATS=1`). It needs no dump, so it covers every in-between of
a route at the cost of a log line. It lists the in-betweens where the
matcher was least sure - draws left unmoved beside draws that moved, the
slow ones - and counts cuts, changed cels carried, and passes with no
in-between after a late frame. `--csv` appends one row a log with the
commit, for a chart over commits.

**`tools/interp_motion.py DUMP_DIR`** is a proof of concept. It cuts the
in-between into blocks of 16 pixels and asks of each whether one step s
serves both ways: the block in frame N at -s and in frame N+1 at +s. A block
that matches each neighbour well somewhere, at places 3 pixels or more from
opposite, is counted. What it flags by design: a frame shown unmoved (a cut,
a broken run), a line of letters that changed. What it cannot see: a part
whose cel changed outright, a flat part, a step beyond its reach (20 px).
About 0.7 s a triple on eight processes.

**What the motion measure found** (2026-11-16, the build of `c3a2c7d8`,
`--wide 2`, `BOF3X_FILTER=point`). On pathCouple with every in-between
dumped it lists the six frames of section 6 item 17 (550, 554, 572, 576,
581, 586) on the build before that fix - yesterday's dump, 44 of 1,014
triples listed - and none of the six on this build, 19 of 930 listed. One
triple in six on the other routes:

| Route | Triples | Listed | Read |
|---|---|---|---|
| worldMapAndAreaTransition | 357 | 6 | 480: section 6, item 19 |
| bridgeWalk | 350 | 22 | 960: a figure of the cutscene whose cel changed, shown where frame N+1 has it |
| combat | 435 | 8 | 588: a message bar that jumps 16 px with its text where frame N+1 has it, the bar the same; 450: a fighter whose cel changed, shown where frame N+1 has it |
| shop | 526 | 18 | not read; the largest are steps of +20 and -20, the reach, on the floor's pattern |
| bossAndFlash | 921 | 68 | not read; most are in 2856..2910, the spell |

Four listed triples were read, of 141. A repeating pattern (the shop's
floor) matches at many steps and is listed falsely when the true step is
past the reach.

**What the statistics lines gave** on the same runs (`tools/interp_stats.py`;
"loose" is in-betweens with draws left unmoved beside draws that moved, a
twentieth of the frame's or 24):

| Route | In-betweens | None, late | Cuts | Loose | Changed cels carried | Match ms, mean and most |
|---|---|---|---|---|---|---|
| bossAndFlash | 5,528 | 1 | 10 | 536 | 3,179 | 1.41, 4.0 |
| combat | 2,615 | 3 | 3 | 153 | 1,433 | 1.06, 2.3 |
| pathCouple | 926 | 1 | 3 | 2 | 691 | 0.88, 2.4 |
| shop | 3,158 | 3 | 31 | 31 | 263 | 0.34, 2.4 |
| worldMapAndAreaTransition | 2,146 | 1 | 12 | 31 | 951 | 1.00, 5.3 |

None of the loose in-betweens was read. The worst are battle frames where
about half the draws stand: bossAndFlash 2860..2868 (a run of 496 draws
broken, 230 to 300 too far) and combat 1091..1101 and 2073..2079 (the
same shape, 328 to 384 draws). The shop has three runs of three or more
cuts (1859..1863, 2976..2980, 3126..3128). Those are where to look next.

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
    present after it). With the in-between off the step is always 1: the
    clock is the present count, so the look is the same bits as before. The
    grain is fresh on each picture shown. `SaveFrame` captures the target,
    which the look never touches, so the dumps cannot show the look.

    **The trail fades by the whole knob on every picture** (2026-11-15).
    The first form faded it by `acc_modulate^step` (0.65 became 0.8062 a
    half step), so the trail lasted as long on the screen. The owner saw it
    in play: a smear on the leader's head and a second copy of every edge
    that scrolls, at 60 and not at 30. The trail is `max(last * fade, new)`,
    so it shows only where the last picture is brighter than the new by
    `0.96 / fade`: 1.48 times at 0.65, 1.19 times at 0.8062. And a walk's
    new cel comes in on the in-between (it is frame N's commands), when the
    old cel is half a frame old. Measured on an every-frame dump of
    pathCouple at 21:9: the leader, still on the screen, is still in 121 of
    121 in-betweens, so the matcher does not move them; the trail formula
    run over the dump's pictures gives a double head at 0.8062 and nearly
    none at 0.65. Now each picture has the trail as strong as with the
    in-between off, and it lasts half as long. Not captured from the look
    itself; the owner's eye decides.

    **The scan lines roll by the frame's roll, halved** (2026-11-16). The
    lines are `sin(6 * time - y)`: 6 radians a frame is 0.2832 short of a
    turn, so they roll slowly back. On the half-frame clock a picture moved
    them 3 radians, 0.1416 short of half a turn: bright and dark lines
    changed places on every picture and all but cancelled. And since item
    18 a late frame is followed by pictures a whole frame apart, so the
    lines stood for a moment and went again - the owner, in play: the look
    "flickering in and out". With `scanroll=0` it was gone (the owner, the
    same day), which names the term. The shader now takes the whole frames
    at 6 radians, as the preset does, and the part of a frame at
    6 - 2 pi: -0.1416 a half step, the same roll on the screen with the
    in-between on, off, or coming and going. With it off the part is 0 and
    the sum is the preset's own. A `scanroll` knob below 1 scales a whole
    frame's 6 radians, as before: at 0.5 it is 3 a frame with the
    in-between off too, the preset's own behaviour. Not captured from the
    look. Seen by the owner in play on the build with the change, the same
    day: "much better".
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

13. **Jugs and barrels a step ahead of the ground** - *closed 2026-11-15*
    (the owner, in play at 21:9: static objects "jumping forward, then
    skipping back"). worldMapAndAreaTransition at `--wide 2`, Frame_Counter
    414: the jug is 13 px along in the in-between and the ground 7. The
    decision log: the jug and the barrel are sprites of one surface, scaled
    with their distance, and their texture coordinates differ in the last
    digits from frame to frame (the barrel's `v` 0.1533, then 0.1534), so
    they had `looks 0` on every frame and paired by shape alone. The shape
    is the box in steps of 4 px, and on the frame the scaled box crosses a
    step (the jug at 68 px, the barrel at 76) there was no candidate at
    all: the draw stood where frame N has it while the ground went halfway.
    The look now reads texture coordinates to the whole texel; the same
    in-between has the jug at 6 px and the ground at 7.
14. **The world map's leader jumping left and right** - *closed
    2026-11-15* (the owner, the same session). The walk's cel changes every
    fourth frame (Frame_Counter 1676, 1680, 1684) and the new cel's box is
    10 px to one side of the old one's, the figure itself unmoved. The new
    cel paired with the old by shape, and halfway was 5 px to the side of
    where either frame has the figure. Such a pair now carries the kept
    draw's own motion (section 3, rule 3). Measured on the two world map
    stretches, the figure against the next frame's: 10 of 59 sampled
    in-betweens 3 px or more off before, none after. The first form, which
    fell back to the boxes' step when the kept draw had no motion, left one
    (1182, the frame after a turn).
15. **Gaps as the camera pans** - *closed 2026-11-15* (the owner, at 21:9).
    Reproduced at Frame_Counter 888 of the same route: a hole in the hedge
    at the right edge that neither real frame has. The hedge is a row of
    pieces 41 px apart, some of a look drawn once, and the picture moved
    22 px: the nearest look-alike of the repeated pieces was the piece
    beside their own (+19 where the ground went -22), the surface's vote
    took +19, pieces with a look of their own then went by shape with the
    wrong piece or the right way alone, and the row opened. A surface now
    votes by its looks drawn once where it has any (section 3, rule 1). The
    owner's own capture, a sliver beside a house roof on the same pan, was
    not logged; it is taken to be the same fault and wants their eye.
    **The three changes against the build before, 2026-11-15** (items 13
    to 15; every route at `--wide 2`, `BOF3X_FILTER=point`, one triple in
    six; "differ" is in-betweens that are not the same picture, between
    equal real frames; flagged is `tools/interp_holes.py`):

    | Route | Triples | Differ | Flagged before | Flagged after |
    |---|---|---|---|---|
    | worldMapAndAreaTransition | 358 | 61 | 2, 524 px | 1, 42 px |
    | bridgeWalk | 351 | 84 | 29, 2,522 px | 29, 1,914 px |
    | shop | 527 | 13 | 13, 8,788 px | 13, 8,788 px |
    | bossAndFlash | 921 | 72 | 25, 23,224 px | 21, 22,981 px |
    | combat | 437 | 26 | 10, 10,672 px | 10, 10,672 px |

    By the shift measure of section 4 no differing in-between is worse but
    one, shop 2136, read by eye: the barrel outside the shop, a step ahead
    before and halfway after - the measure calls halfway "further from
    both". Thirteen are better. Not run: the other routes of section 4,
    the original 4:3 picture, and no route was played.
16. **One stray** of 42 pixels at the bottom edge, Frame_Counter 840 of the
    same route, before and after the changes of 2026-11-15. Not read.
17. **A figure that wiggles as the party walks past** - *closed
    2026-11-15* (the owner, in play at 21:9: a man and a woman standing by
    a path, the man waving). Route `pathCouple` (the owner's save, walked
    past both ways at `--wide 2`, every in-between dumped): on the frame
    the man's cel changes, one in five (Frame_Counter 550, 554, 572, 576,
    581, 586 ...), the in-between has him where frame N does while the
    woman and the ground go halfway, 5 to 6 px apart from her. The decision
    log at 586: his draw has `looks 0 best -1`. The new cel is another
    surface in a box of another size, so there is no candidate by look or
    by shape, and a draw with no pair stands still. He is now found by his
    place in the list and carries his kept draw's motion (section 3, rule
    3). Measured by matching the pair's picture of frame N in the
    in-between: 13 of 94 tracked in-betweens had the two apart before, none
    of 96 after.

    Three forms were withdrawn on the way. By surface: the cel's surface
    is new, so nothing matched. Between partnered neighbours only: a tile
    entering at the picture's edge is sorted in beside him two frames in
    ten. Without "alone on its surface and upright": ground tiles took the
    rule. A carried figure is also kept out of what the next draw in the
    list agrees with.

    **Against the build before, 2026-11-15** (`--wide 2`,
    `BOF3X_FILTER=point`; in-betweens between real frames that are equal
    in both runs): bridgeWalk, one triple in three, 24 of 212 and 40 of 177
    differ; the five that differ over more than 170 px were read and are
    the party's three figures and nothing else. Two runs of one build
    differ at 2 of 104 and 5 of 280, none over a large area. One triple in
    six: worldMapAndAreaTransition 29 of 290, shop 11 of 438, combat 19 of
    302. Read: worldMapAndAreaTransition 1116, the world map's mist patch
    (a lone upright surface, now carried) and 1416, three small pieces. Not
    read: the rest. The envelope flags could not judge the change: see
    item 18. **Not checked: that a party figure carried this way sits
    better than one left where the frame has it** - the owner's eye.
18. **Holes in the sea after a late frame** - *closed 2026-11-16.* Not the
    kept frame being two steps old, as first taken: the list the in-between
    was built from held two frames. `Gpu_PutDispEnv` (`0x5A7860`) presents
    only when the display buffer's `y` is not the one last put, and WinMain
    swaps the buffers every pass, drawn or not. So after one late pass the
    next pass puts the same buffer again: nothing is shown, the frame list
    is not reset, and `Gfx_DrawOTag` records the new frame on top of the last
    one's. The pass after that shows both. This is so with the in-between off
    too (known-defects D426). With it on, the in-between of each of those two
    passes paired a list of two frames against the kept one.

    Measured with `BOF3X_LATE_EVERY=12` on bridgeWalk at `--wide 2`,
    `BOF3X_FILTER=point`, one triple in six, the build before
    (`bfdbde94`): of 131 triples 6 flagged for 52,028 px, 5 of them the
    in-between right after a made-late pass. And it fed itself: the matcher
    on a doubled list took long enough (7.4 ms on average, 89 ms at most,
    against 3.2 and 7.5) that the next pass was late, so the pass after it
    put the same buffer again and showed nothing again, and the list grew -
    to 14,890 draws where a frame has about 1,450. 814 passes of 2,097 had
    no in-between beyond the 174 made late, and the longest time between a
    present and an in-between was 1,422 ms: the picture stood for that long.
    That is what a loaded machine saw on 2026-11-15, the frames differing
    from run to run.

    Now an in-between is shown only when this pass and the pass before both
    presented (`magic_080.cpp`, `render::PresentCount`): two passes without
    one after each late frame. The same run on the fix (`c3a2c7d8`): 175
    triples, none flagged; 351 passes with no in-between for the 174 made
    late and none else; 1,534 draws at most; 17 ms at most from a present to
    its in-between. With no pass made late, the two builds' in-betweens are
    the same picture at 346 of 346 triples.

    The late frame itself is DIV-0237, the same day at the owner's call:
    a pass that draws presents the waiting frame when `Gpu_PutDispEnv` did
    not, so the list never holds two frames. With it the same run has 176
    passes with no in-between for 175 late ones - one after each late
    frame, not two - and still none flagged. The in-between's own rule
    stays: the present after a late frame shows a frame two steps back.

19. **A party figure stands for one in-between as a walk begins** - *closed
    2026-11-21 by item 20; found 2026-11-16* by `tools/interp_motion.py`. worldMapAndAreaTransition
    at `--wide 2`, Frame_Counter 480, in town: the ground goes halfway
    (898 of 903 draws moved, about 16 px by 5) and the two party figures are
    shown where frame N+1 has them, 11 px by 6 from where frame N has them.
    The decision log: one figure's draw has `looks 0 best -1` and no pair by
    its place either; the other is a cel that changed, carried with
    `d (0.0, 0.0)`. A carried cel takes the motion its kept draw was shown
    with (rule 3), and on the first frame of a scroll that motion is none.
    pathCouple 530 and 913 have the same step (10 px, nothing to frame N+1)
    and are taken to be the same; not logged. Not seen in play. Where to
    start: a carried draw whose kept motion is none, in a frame whose other
    draws move - against the world map's leader (item 14), who is rightly
    still while the ground moves.

20. **Figures paired by the game's own object** - *prototype 2026-11-21*
    (the owner's question: do the animations that change mid-loop change
    the function that draws them, and should the matcher go by it).

    **Not by the function.** `BOF3X_DRAWORDER` over recipe frames 418 to
    928 of pathCouple at `--wide 2` (510 frames, the whole picture): every
    figure is `Sprite_Draw`'s, 2,323 draws of code `0x84` / `0x86`, on the
    cel-change frames as on the rest; the map is four paths (the layer
    lists 214,207 draws, cell handlers 19,970, model records 9,509) and one
    bare commit. The couple are sprite objects `0x7DF258` and `0x7DF1B4`;
    their boxes change on Frame_Counter 550, 554, 572, 576 and 561, 566,
    581, 586, item 17's frames, with the same tag. A cel is the object's
    frame record (+0x54 with the word +0x5A) composed into a cell texture:
    another record is another texture and box from the same code. So the
    function says nothing a look does not, and no name or family changes.

    **By the object.** The same tag carries the sprite object, which a cel
    change does not touch, and the primitive carries where the object
    stands (floats +8, +0xC). Rule 0 of section 3 pairs a named draw with
    the kept draw of its object and moves it by that place's step. One cell
    sprite is one recorded draw of four vertices, so there is nothing to
    pair inside a figure.

    Measured 2026-11-21, the build of `main` `ffb2c892` with the change,
    `--wide 2`, `BOF3X_FILTER=point`, the rule off (`BOF3X_INTERP_IDENT=0`)
    against on in one build. The `interp-fig` lines (`BOF3X_INTERP_STATS`)
    give each named draw's half step as shown against its object's own:

    | Route | Named draws | Off: 1.5 target px or more from the object's step | On |
    |---|---|---|---|
    | pathCouple | 3,090 | 127, the worst 11.8 px | 0 |
    | worldMapAndAreaTransition | 4,510 | 192, the worst 45.9 px | 0 |

    Most of the off side's are the two party followers (`0x802E8C`,
    `0x802FD8`: 104 of 127 and 157 of 192). The on side's 0 says only that
    no later rule undid rule 0; the pictures are the independent check.
    `tools/interp_motion.py`: pathCouple 3 triples listed off (624, 774,
    870), none on, of 155; worldMapAndAreaTransition 6 off, 4 on, of 357 -
    480 (item 19) and 528 gone, 738, 804, 840 and 894 on both sides. Read by
    eye: 480, the follower where frame N+1 has it off and halfway on, the
    chimney halfway in both; 894, world map ground with no figure in it,
    the same picture both ways. The rest not read.

    Rule 3's place-in-the-list form is no longer used on these two routes
    (653 and 458 draws off, 0 on); its by-shape form still is (41 to 4,
    493 to 264). A battle's fighters are named too: combat, 12,895 named
    draws in 2,284 of 2,622 in-betweens, not compared off against on. The
    matcher's time is unchanged (pathCouple 0.95 ms off, 0.93 on; the
    world map route 1.08 both).

    **Logic neutrality.** `tools/statehash.py check`, two runs with the
    rule off as the references and one with it on, the same build: identical
    on all 961 compared ticks of pathCouple and all 2,113 of
    worldMapAndAreaTransition. Run with the owner at the machine, not on a
    quiet one. The ten proof routes not run.

    **Not done.** The map's draws carry a name of their own in the same tag
    (the view cell and its map cell), which would serve the look-alike
    tiles of items 15 and 1; not tried. Models (`Sprite_AddDrawRecords`)
    and effects have no name. Not played: the owner's eye on a walk through
    a town and on a battle. The worst case not looked for: an object slot
    reused by another figure between two frames pairs the two, held only by
    the cap.
21. **A list's highlight slid half a row** - *fixed 2026-11-21* (the owner:
    going up a shop's item list, the bullets jump; buy and sell screens;
    the route `ultrawideMerchantCull`, `--wide 2`, `BOF3X_FILTER=point`).

    The list draws the row under the hand a second time: a dark copy of
    its letters and price, then the bullet, the letters and the price
    again, 2 game px higher. The dark copy's letters and price are surfaces
    of their own, so each look is drawn once in a frame. When the hand
    goes to another row, those draws come at that row's place in the list.
    The decision log of Frame_Counter 687 (`BOF3X_INTERP_DEBUG`): the dark
    price's four draws and the raised bullet each took the old row's draw,
    26 target px away - the lone looks voted that step for their surfaces
    and agreed with it - and were shown 13 px from their row; the letters
    alone stood, by rule 7's line of letters. At 571 the step went on
    through "what the draw before it did" into the rows below, which hold
    look-alikes (prices, letters): 55 draws, six rows of the list, shown
    half a row down for one in-between. It showed going up more than going
    down (5 draws at 687; 3 or 4, the price alone, at 897 and 909).

    A first attempt is recorded because it failed: a number for each
    surface's content, so that a pair across a written surface counts as a
    pair by shape. The dark letters then took the row's own letters by
    shape and the raised letters took the old row's; and the dark price is
    written with the same pixels (two rows at 200Z), so it paired as
    before. Taken out.

    **The rule** (section 3, under rule 4): a leap among the still.
    In-betweens of Frame_Counter 560 to 970 of the route (the buy list, the
    change to the sell list, the sell list) by their unbroken runs shown
    8 px or more from where the frame has them:

    | | One such draw | 2 to 99 draws | 100 or more (a window sliding) |
    |---|---|---|---|
    | before | 50 | 40 | 15 |
    | after | 49 | 0 | 15 |

    All forty are gone with the rule; three of them were read in the
    decision log (564, 571, 687), each the highlight. The single draws are the hand
    (its partner by order), which slides as before; which one of the fifty
    went was not looked up. Over frames 450 to 1290 the build with the rule
    has 13 in-betweens of 10 to 99 such draws - windows sliding at 454 to
    456 and 1274 to 1277, and 999 and 1166 to 1193 in the sell screen, a
    block going one way with no pair refused, not read further - and 74 of
    the hand's.
    The rule refused 628 pairs on 85 in-betweens of the route.
    On other routes, the same build: combat, frames 500 to 1700, 16 pairs
    on 10 of 1,151 in-betweens; menu_screens, 300 to 1900, 78 on 11 of
    1,311. The lines read are letters that took a look-alike letter of
    another line or window (one 390 px away). **Not read by eye** on those
    two routes; the state hash not run (the matcher reads the frame list
    and writes the in-between's vertices only). The owner watched the
    route's replay on the build with the rule: "that looks good".

Done 2026-11-14: the `DIVERGENCE.md` entry (DIV-0230) and the launcher's
box. Before it leaves "experimental": the black and shift checkers in `tools/`, the debug
logging trimmed, and the owner's eye over battles and menus (the row in
`owner-review.md`).
