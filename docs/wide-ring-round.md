# The wide-ring round: the field's tile ring grown, and the two sweeps drawn mapped

**Status:** STAGED 2026-10-21, not launched (briefs to go in
`C:\Users\kerok\bof3x-runs\r_ring\`); the owner's three calls answered the
same day (section 6). The owner decided both halves on
2026-10-15 ([`owner-review.md`](owner-review.md), "Decided by the owner, to
build"): **build IDEAS I37 now** (the ring, RG, without waiting for a picture
of a far-camera scene) and **map the two sweeps onto the wide picture**
(IDEAS I38: the logic's x kept, the draw mapped). Neither was built; the
button, install-gap and quicksave rounds started the next days and the list
was never given a round of its own (found 2026-10-21 - no PR from #62 to #101
touches either). Nothing below is measured anew; every number is cited from
the ultrawide round's docs or is marked as a guess.

The other two items of that 2026-10-15 list - D300's faerie fixed narrowly
and the per-scene pillarbox for areas 31, 90 and 53 - are **not in this
round** (section 7).

## 0. The shape in one paragraph

Two display changes to the wide modes (`BOF3X_WIDE=1` 426 x 240, `=2`
560 x 240), each **logic-neutral and narrow-identical**. **RG** grows the
field's view ring from Capcom's 28 columns x 56 rows to the PSP's 33 x 58
when the picture is wide, so far cameras (D292) stop running out of cells at
the picture's edges; at 4:3 the ring stays 28 x 56 byte for byte. **SW**
draws `EffectKind0D`'s slide and area 172's growing shade through a map from
the logic's x to the wide picture's, so each starts and ends past the wide
edge on the original's frames; the records' x, their idle frame and the
script's step are Capcom's. Both are DIV-0041 amendments. The round's gate
is the one the ultrawide round set: **the eleven hash routes keep narrow's
`Rand` and game pages at 16:9 and 21:9** ([`state-hash.md`](state-hash.md)).

## 1. What is already known, and the traps

- **The ring's 23 sites**, PSX and PC paired, all ours:
  [`psp-widescreen-placements.md`](psp-widescreen-placements.md) 2.4 (the
  `MapView_*` shift / build / fill functions `0x56E9A0..0x56FCA0`,
  `DrawLayers_Reset`, `Field_ViewReset`, `MoveCmd_TestFB` / `FC`,
  `AreaMap_ClearCell`, `MapView_ItemAt`, `MapView_LinkPrimAt`,
  `MapView_PlaceRuns`, `Packet_FrameReset`, `Sprite_DrawPass`,
  `Sprite_UpdateScreen`, the quake pair `Quake_Start` / `Quake_End`,
  `Area40_TileLit`). The PSP's change is loop bounds 27 / 28 / 55 / 56 ->
  32 / 33 / 57 / 58 and the row stride 112 -> 132 bytes. Two PSP sites have
  no PC twin established (`0x801565fc`, BATTLE.EMI#15 `0x800ac020`): RM
  settles whether the PC has them.
- **How much ring is needed** ([`ultrawide-culls.md`](ultrawide-culls.md) 0
  and 3): at the field's own camera (`Camera_Distance` 0) the 28-column ring
  already covers 560 px once DIV-0041's inset reaches 0. It runs out past
  distance about 2,200 (level ground) or 1,600 (ground 256 below), which
  three areas' scenes (2,176..2,560) and effect kind 0x8D's pull (up to
  7,296) reach, and at off-axis yaws over low ground. 32 columns cover every
  persistent distance our code writes; the PSP's 33 covers up to about 3,200
  to 3,700. **Kind 0x8D's first frames stay past even 33** - D292 narrows,
  it does not close (call 1: the owner accepts that).
- **The off-screen flag is logic.** `Sprite_UpdateScreen`'s layer test
  `0..0x36` sets the flag the logic reads (IDEAS I37;
  [`ultrawide-culls.md`](ultrawide-culls.md) 2: nine readers, the party's
  following among them). A grown ring must keep that decision at Capcom's
  bound, as DIV-0041's bit-7 split already does for `Sprite_Draw`'s cull -
  or the hash routes will part.
- **The random split is sized for 28.** `wide_rand::Row` holds
  `int columns_[28]` and classes each cell walked as Capcom's or a band
  cell (`src/game/wide_rand.h`; D283, [`ultrawide-rand.md`](ultrawide-rand.md)).
  A cell only the grown ring adds is a band cell by definition: its
  `MapCell_DrawRising` `Rand` calls must come from the draw-only generator,
  or the wide `Rand` count changes. RG owns that file's change.
- **The inset stays.** DIV-0041 lowers the camera's cell inset
  (`Widescreen_InsetColumns`, `map_layers.cpp` `Inset()`). The owner's 6.1
  answer kept "the inset approach for now"; RG adds ring columns beside it,
  it does not replace it (call 2, confirmed 2026-10-21).
- **The quicksave holds the field.** A quicksave is the data span plus the
  participants' records ([`quicksave.md`](quicksave.md)); `wide_rand` is
  already a graphics participant. If the grown ring lives outside the span
  (a buffer of ours), a quicksave taken at 16:9 and loaded at 4:3, or the
  other way, must either rebuild the ring from the map or refuse. RM reads
  where Capcom's ring lives and which way is cheaper; the answer goes in the
  ledger entry.
- **The sweeps' numbers** ([`wide-staging.md`](wide-staging.md) 8;
  [`ultrawide-culls.md`](ultrawide-culls.md) rows 14, 15): `EffectKind0D`
  (`effect_1a.cpp`, `0x465F30` / `0x466050`) slides in from x 390 by 30 a
  frame to 160, holds 30 frames, out to -70 or less, then idles;
  `Area172_EffectA5Grow` (`area_w4b.cpp`, `0x428240`) grows a shade's edge
  by 10 a frame and at more than 400 steps the script. The map is
  `[160, 390]` onto `[160, 390 + c]` and `[-70, 160]` onto `[-70 - c, 160]`
  for the slide, `c` the columns a side (53 or 120); the shade's edge the
  same way past its start. It needs a draw hook in `Sprite_QueueOverlay`'s
  path (`0x5890E0`, ours).

## 2. Stage A (two groups, in parallel)

### RM: the ring's read pass (a doc; no code but a census script if wanted)

Every reader and writer of the ring, not only PSP 2.4's 23: the arrays'
addresses and sizes, every constant 27 / 28 / 55 / 56 / 112 (and their
shifted or scaled forms) that bounds or strides it, every other function that
indexes it (an image scan, as `ultrawide-rand.md` 1 did for `MapView_Inset`),
the two unpaired PSP sites, `Area40_TileLit` and the quake copies, and the
off-screen flag's derivation from the ring in `Sprite_UpdateScreen`. Then the
plan RG builds to: where the grown ring lives (in place if the span has room
after it - it will not, by the PSP's own relocation, but measure; else a
buffer of ours with the 23 sites re-aimed when wide), how narrow stays
byte-identical, how the quicksave carries it, and a cost per site. Doc:
`docs/wide-ring-read.md`. Done: every site listed with its file and line in
our source, and the open questions for the owner (if any) in section 6's
form.

### SW: the two sweeps drawn mapped (`effect_1a.cpp`, `area_w4b.cpp`, the draw hook)

Build I38 as decided: the logic's x unchanged, a draw-time map applied to
each sweep's quad when the picture is wide, nothing at 4:3. Owns
`src/game/effect_1a.cpp`, `src/game/area_w4b.cpp` and the hook's file
(`widescreen.cpp` / `.h` if that is where it belongs; not `map_*`,
`wide_rand.*` or `sprite_screen.cpp`, which are RG's). Done: build;
`--selftest` and the two functions' fuzz at narrow (0 mismatches - the map
must be inert there); the state hash on the eleven routes at 16:9 and 21:9
identical to narrow's pages; a capture of each sweep at 16:9 and 21:9 for
the owner (area 172's from a chapter-jump save, call 3,
[`chapter-jump-save`](HANDOFF.md); `EffectKind0D`'s reaching scene named by
reading if no route has it); the DIV-0041 amendment's text drafted in the
group doc `docs/wide-sweeps.md`.

## 3. Stage B (after RM): RG, the ring grown

Build to RM's plan: 33 x 58 when wide (call 1), Capcom's
28 x 56 when narrow; the 23-plus sites, the off-screen flag kept at Capcom's
bound, `wide_rand::Row` and the band test widened so grown cells are band
cells, the quicksave rule. Owns `map_scroll.*`, `map_layers.*`,
`sprite_screen.cpp`, `wide_rand.*`, `draw_pool.cpp` and the sites' own files
as RM lists them. Done: build; `--selftest`; every touched function's fuzz
at narrow with 0 mismatches; the state hash on the eleven routes and
`dolphinBoss` at 16:9 and 21:9 matching narrow's `Rand` and game pages; a
quicksave across 4:3 and 16:9 both ways (loads, or refuses with the
sentence, per RM's rule); the draw-item pool's peak logged wide on the
routes (DIV-0062's 2,048; [`ultrawide-culls.md`](ultrawide-culls.md) 4 says
it should hold); captures of the three far-camera areas at 21:9 from chapter-jump
saves (call 3). Group doc `docs/wide-ring.md`.

## 4. Stage C: the end batch, then the owner's eye

1. Build, `--selftest`, the shadow fuzz, from a clean launcher dir
   ([`self-tests-need-a-clean-launcher-dir`](HANDOFF.md)).
2. State hash, ten routes, narrow: identical to the `cn_<route>_orig3`
   references; and wide at both widths against narrow's pages.
3. The contact sheets at 16:9 and 21:9 over the attract and the routes, point
   filter; the far-camera scenes and the two sweeps on their own sheet.
4. Ledger: DIV-0041 amended twice (RG, SW); D292 narrowed to kind
   0x8D's first frames (call 1), the rest fixed; I37 and I38 to built;
   [`owner-review.md`](owner-review.md) the round's entry; STATUS, HANDOFF.

## 5. Order and merge

Worktrees from `main` at launch. RM and SW start together; RG starts when
RM's doc merges. Merge order: RM's doc, SW, RG. Only a group's hand-back
report means merge; a usage cut resumes with a message naming the next step.

## 6. The owner's calls - answered 2026-10-21

1. **The ring's size: the PSP's 33 x 58**, at both 16:9 and 21:9. It covers
   every persistent distance our code writes; kind 0x8D's first frames still
   notch, at 16:9 as today, so D292 narrows to them rather than closing.
2. **The inset: DIV-0041's lowered inset stays**, beside the grown ring. The
   ring adds columns; the walk's inset rule is unchanged.
3. **Far-camera captures: chapter-jump saves**
   ([`chapter-jump-save`](HANDOFF.md)), for the three far-camera areas and
   area 172's shade; no new owner route.

**Answered 2026-10-21, in chat, after the groups handed back:**

4. **RM's section 7** ([`wide-ring-read.md`](wide-ring-read.md)): sprites in
   the grown rows kept hidden (0..0x36); D295's rule kept, the gate decides;
   the walk is today's plus one, `g = max(0, i + 2 - L)`; a quickload across
   widths rebuilds the rim, never refuses.
5. **RG:** `MapView_Redraw` = 2 after a load across widths (not RM's 1),
   kept.
6. **SW** ([`wide-sweeps.md`](wide-sweeps.md)): the shade's right-anchored
   map and the banner's faster slide in the bands, both accepted off the
   sheets.
7. **FY** ([`faerie-fix.md`](faerie-fix.md)): the faerie's look and the
   sides' second draw, accepted off the sheets.
8. **PB** ([`narrow-pillarbox.md`](narrow-pillarbox.md)): menus opened in a
   framed area stay framed; area 53 at 426; area 194's close-camera opening
   framed at 426 - all three as PB recommended.
9. **Captures:** an agent crafts chapter saves for PB's nine uncaptured
   areas and tries the three real far-camera scenes (area 167's init, area
   19's camera-out, chapter 1's scene 3); a scene no crafted save reaches
   goes to the owner by name.
10. **After the crafted-save sheets** (group CS; [`narrow-pillarbox.md`](narrow-pillarbox.md)
    10): **only areas 31, 90 and 198 keep the bars** - the three with real
    void past the frame; 149, 186, 98 and the camps (53, 153, 154, 192, 194)
    render fine at the full width and are left unframed (group PX). Area
    194's first-visit crash under a crafted save is tested against the
    original and our switches "just in case" (group CR). **Area 19 needs
    nothing more:** the owner knows its only far camera as the one quick pan,
    and the zoom the recorded route already captures (`yraallRoad`).

## 7. Side groups, beside the round (added 2026-10-21)

The other two items of the 2026-10-15 list run as one agent each, beside
stage A, on files of their own:

- **FY, D300's faerie through the table** (`BRIEF_FY.md`): area 175's table
  faerie only, an Intent DIV with the note to broaden it to
  [`faerie-over-table.md`](faerie-over-table.md) 4's rule; owns `layering.*`.
- **PB, the narrow areas pillarboxed** (`BRIEF_PB.md`): widened by the owner
  on 2026-10-21 from areas 31, 90 and 53 to **every area narrower than the
  picture** ([`ultrawide-culls.md`](ultrawide-culls.md) 5): 31, 149, 186, 198
  framed at 320 in both wide modes, and 53, 90, 98, 153, 154, 192, 194 at 426
  at 21:9. Wide modes only, draw-only. The owner's one worry - an effect or
  anchored box drawn past an area's width - is checked and reported. Owns a
  `pillarbox.*` of its own; one call in the frame path, which may land in
  SW's `widescreen.cpp` (the later merge rebases).

Merge them whenever they hand back; each is its own DIV (FY) or DIV-0041
amendment (PB). Not in this round: D293 and D294, left at Capcom's bounds by
the owner.

## 8. The brief, per group

`C:\Users\kerok\bof3x-runs\r_ring\BRIEF_<group>.md`, one per group (RM, SW;
RG when RM's doc is in). Each names the goal, the files it owns and must not
touch, the docs to read first (this file; IDEAS I37 and I38;
[`psp-widescreen-placements.md`](psp-widescreen-placements.md) 2.4 and 6.1;
[`ultrawide-culls.md`](ultrawide-culls.md) 0, 2, 3 and rows 14, 15;
[`ultrawide-rand.md`](ultrawide-rand.md); [`wide-staging.md`](wide-staging.md) 8;
DIV-0041, DIV-0062; [`state-hash.md`](state-hash.md); [`quicksave.md`](quicksave.md)
for RM and RG), its harness root under `C:\Users\kerok\bof3x-runs\r_ring\<group>`
(never another session's), the checks that mean done, and the hand-back
report's shape. Rules 1, 3, 4 and 7 restated.
