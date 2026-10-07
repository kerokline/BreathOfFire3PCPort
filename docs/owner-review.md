# Owner review list - things that want the owner's eye or ear

**Status:** IN PROGRESS (2026-10-06) - started the morning after the platform
round merged (PR #42), from the calls [`platform-round.md`](platform-round.md)
section 4 and [`round-14-cleanup.md`](round-14-cleanup.md) section 5
gathered. One list, so the questions only the owner can settle in play are
not scattered across round docs. **Rewrite, do not append**: an item leaves
when the owner has looked and said, and its answer goes where it belongs
(the ledger, the defect list, the round doc). Agents add here; the owner
strikes.

Each item says what to do at the machine, what to look or listen for, and
where the reading behind it is. Nothing here is a divergence until it is in
[`DIVERGENCE.md`](DIVERGENCE.md).

## To listen to (the music investigation, 2026-10-06)

- **The listening set is ready**: `analysis/bgm/listen/README.txt` says the order
  ([`bgm-comparison.md`](bgm-comparison.md) section 9). Play each pair disc,
  then MP3, then disc: `141_*` the title music (brightness and room), `153_*`
  the battle theme (the replayed 7.4 s intro and the cut at 23.6 s), `000_*`
  the town theme (a rhythm hiccup at 17.3 s), then `000_seam.wav` and
  `153_seam.wav` (the PC's loop join on its own, at 8 s). For each: same,
  slightly different or clearly different, and in what. **Then the decision
  the plan waits on** (section 10 there): the disc's music as an option, a
  measured loop-point table for the MP3s as the cheap fix, or both; and the
  decoder swap's target is now known (MPEG-1 Layer III, 44.1 kHz, 128 kbit/s
  CBR, plain stereo, no tags). **The owner heard it (2026-10-06 night):**
  quality close, the seams very noticeable - **the loop fix is built as
  DIV-0081** (13 tracks today; the rest measured tomorrow). Next for the ear:
  `analysis/bgm/listen/153_loop_fixed.wav`, the battle theme spliced the
  engine's way (the join at 23.6 s), and then the game itself with
  `BOF3X_MUSIC_LOOPS` on against `=0`. **One decision:** the town theme
  `000`'s file is 0.44 s shorter than one loop, so no correct loop exists in
  it; the ways on are to accept the rewind for that song, a stretched loop,
  or samples from outside the file (a render is audio derived from the
  game's data and ships nowhere, rule 1; a re-encode the player makes would
  be their own). Undecided.

## To review in play

1. **Music coming back after a pause** (`Sound_ResumeAll`,
   [`sound-rest.md`](sound-rest.md) section 3 item 2). Read from the code,
   never heard: `Sound_ResumeAll` plays whatever music buffer is loaded,
   whether or not it was playing, and a stopping fade leaves the buffer in
   place. **At the machine:** reach a scene whose music fades to silence
   (an area change through the field mode's `Music_FadeOutStop`, or a
   scenario step that fades), then while it is silent press F9 (pause) and
   any key (resume). Also: Alt-Tab away and back during the silence. **Listen
   for** the faded track returning. Run it on ours and under
   `BOF3X_ORIGINAL='*'` - the same on both is Capcom's, and a fix is a ledger
   entry. The scripted resumes (the inn's night, the flag-message fanfare,
   area 113's reward, area 135's box, chapter 11 step 47, areas 57 / 108 /
   145's shared handler) stop the music themselves first and are not the
   case.
2. **The layering fix in play** (DIV-0071, HANDOFF item 0000000): a town,
   stairs, a bridge, followers close behind, with the default on.
3. **TILE_1 drawn as one point** ([`d3d-rest.md`](d3d-rest.md) D-a). Shown to
   the owner 2026-10-06 from the `whelpBoss` route's frame 11880 (the dream
   scene, Deis in the light pillar: the drifting specks are TILE_1s, each one
   screen pixel where the PlayStation's covered four at this scale). A paint
   mock of the 2x2 was sent; the owner: "go ahead and make it larger".
   **Built the same day as DIV-0077** (on by default, `BOF3X_TILE1=0` the
   point). Owed the owner's eye in play: the dream scene after the whelp
   fight, or any Kaiser cast.

4. **Lines drawn one screen pixel wide** (the owner's note, 2026-10-06: the
   fishing gauge's thin orange bar "looks like it's supposed to be a bit
   thicker"). Measured the same day on the `caughFish` route's frame 1680
   (`analysis/shots/fish_1006_every60/f01680.png`, the PLAYER vs FISH gauge
   at 4x): the dark red line under the green bar and the gauge's grey centre
   mark are each one screen pixel, at a window scale of about 3.3. They are
   PlayStation LINE primitives (`LINE_F2` here; `D3d_DrawLineF2` / `_F4` in
   `d3d_draw.cpp`, `_G4` in `d3d_rest.cpp` draw a `LINESTRIP`, which Direct3D
   rasterises one pixel wide at any scale) - **the TILE_1 class again
   (DIV-0077), for lines**. A fix is each segment as a quad of the scale's
   width (every line in the game: the fishing gauge, the fishing grey lines,
   magic trails). The owner (2026-10-06): "all elements should scale" - **built
   the same day as DIV-0079** (one quad per segment, square ends, smooth
   diagonals; on by default, `BOF3X_LINES=0` the strip). Owed the owner's eye
   in play: the fishing gauge and the grey lines, a magic trail if one is
   cast.

## Reported by the owner, for a cleanup session

- **Unrendered squares at the wide view's outer edges** - **fixed 2026-10-07**
  (the owner, 2026-10-06 night; `analysis/shots/owner_reports/bridge_left_edge_unrendered_1006.webp`).
  Not a cull: the view's cell *inset* (`map_layers.cpp` `Inset()`) trims
  each ring row to what 320 columns need, and the wide picture's extra
  columns were never walked. Lowered by three columns a side under the wide
  view (DIV-0041's entry, `BOF3X_WIDE_INSET`); the `bridgeWalk` route wide
  shows the sea at both edges on every frame. The owner (2026-10-07): "the
  sea tiles stepping is probably correct now". Owed the owner's eye on the
  canyon bridge of the first screenshot, and on the attract sequence's map.

- **The Volt's EXP bonus did not appear** (the owner, 2026-10-06 night;
  screenshots `analysis/shots/owner_reports/volt_fight_menu_1006.webp` and
  `volt_fight_result_78exp_1006.webp`): a field fight against three Volts and
  one Thunder, Nina died, the result screen "You gain 78 EXP!" with 78 on each
  of the three rows. The owner's reading - their recollection of the game's
  rules, to be verified against the code: a Volt hit by an electric attack
  should change mode and give extra EXP; a Thunder gives about 16; expected
  about 84 x 3 + 16 = 308 with the bonus, 156 without, and 78 x 2 = 156. The
  first report (2026-10-02, HANDOFF item 00000) was held for a route; this is
  its substitute. **Staged for tomorrow** as a read of the reward path and
  the mode-change op against the PSX twin, then a route for the owner to
  record (brief `plat2/brief_volt.md` in the session-7d0c9683 scratchpad);
  not launched tonight (the PC goes off).

- **The Skill Notes prompt's hand a word short of `Yes`** (the owner,
  2026-10-06 night; screenshot
  `analysis/shots/owner_reports/skill_notes_record_yes_no_1006.webp`: the
  skill menu's Note tab, "Record in Skill Notes?  Yes No" on the top line,
  the hand pointing at the gap after the question mark, a word's width left
  of `Yes`). The owner's reading: the same yes / no issue as the other
  screens - another chooser `Menu_YesNo` does not reach, the fifth after
  [`yes-no-prompts.md`](yes-no-prompts.md) section 2's four. A cleanup
  session, by that doc's method: find the prompt's `Menu_DrawHand` site
  (the E8 scan's 44 sites; the skill menu's Note tab, so `0x9398D2`-style
  answer byte and stops to read), measure the English `Yes` / `No` from the
  line as section 3 does, give it DIV-0027's gap under a Latin overlay only,
  amend DIV-0027's list, capture before and after for the owner. Check the
  other skill-menu prompts (the Note tab's forget / overwrite, if any) while
  there, since they will share the site.

- **The master list's Chinese header and its marks** (the owner, 2026-10-06
  night; our screenshot
  `analysis/shots/owner_reports/master_list_header_marks_1006.webp`, a web
  capture of the US PlayStation screen beside it,
  `master_list_web_reference_1006.png`). Two things on one screen, the camp's
  "View master's profile" list:
  1. **The list's title is still 师匠** under the English overlay. The US
     screen's word is **`MSTR`** - this answers half of
     [`yes-no-prompts.md`](yes-no-prompts.md) section 5's question to the
     owner (the portrait box's 弟子 label, `0x66A1F8`, is still unanswered:
     the web capture does not show that box). The slot is `0x66A1F0`, 8
     bytes, drawn by `MasterWin_DrawList` `0x59C2C0` (ours, `rest_2h.cpp`)
     through `0x57D800`. Section 5 found no `MSTR` on the US disc as text, so
     the four letters would be ours to write - DIV-0064's way, a kind-15
     group into the slot, the word recorded as authored from the owner's
     reference, not read from a disc.
  2. **The mark beside a completed master is a cross (`†`), where the US
     screen shows a star (`★`), and a dot for the others.** The owner's
     reading: crosses where stars should be. `MasterWin_DrawList` draws the
     mark from `0x66A2D8` (`rest_2h.md`'s row: "the mark `0x66A2D8`
     (available) or an icon"), a `.data` string whose Chinese glyph the
     Latin font maps to `†`. The fix is the same shape as the header's: find
     what glyph the US overlay's font has for the star (the web capture shows
     the US font draws one), and write the code into the slot under a Latin
     overlay only; or, if the mark is an icon on the US side, draw it as one.
     **The dot is already right:** a second capture
     (`master_list_unfinished_dot_1006.webp`, Fahl unfinished) shows the
     unfinished mark as a small dot under our overlay, as on the US screen -
     so only the completed mark's glyph is wrong, and `rest_2h.md`'s "or an
     icon" is the dot. Amend DIV-0064 with both.

  **Done 2026-10-07 (DIV-0064 amended, DIV-0059 extended):** both are the
  US disc's own - `SHOP.EMI` has `MSTR` and the star beside each other, so
  nothing was authored. The cross was ours: the mark is the byte `t`, the
  shipped font's `t` slot is a star, and the overlay's letter repaint wrote
  a t over it. `MSTR` is centred on its box in
  `analysis/shots/master_labels/masters.png` (the camp route); **the star
  is owed the owner's eye** - no committed save has a completed master.
  The pupils box's 弟子 stays as shipped: not on the disc beside `MSTR`, and
  the US box is not in the owner's capture.

- **Two routes with visual glitches, recorded by the owner** (2026-10-06
  night, `d1c5dcf1`: `tools/recipes/ninaWalkBehindBlock.txt` and
  `tools/recipes/bridgeWalk.txt`, their saves imported by the owner as
  `tools/recipe_saves/ninaWalkBehindBlock.DAT` and `bridgeWalk.DAT`; replay
  with `--save NAME`). Read 2026-10-07 with a capture every 60 frames
  (`tools/recipe_shots.py --every 60`), A/B against `BOF3X_LAYERING=0`,
  `BOF3X_WIDE=0` and `--original '*'`:
  1. **Nina through a crate - DIV-0071's rule, fixed.** Identical with the
     layering off, wrong with it on (frames 360 and 540): the rule guarded
     the feet only, and the crate's raised top covered her body, not her
     feet, so it was crossed. Now nothing but floor is crossed where it
     reaches the sprite at all (the entry's "Refined 2026-10-07"); the
     route's captures identical to the switch off, the fix's own routes
     re-run. **Owed the owner's eye in play** with the default on.
  2. **The bridge scene** (area 41, `DAT\AREA041.DAT`), each cause apart:
     - the sea's stair-stepped edges: the cell inset, **fixed** (above);
     - **Garr behind the deck and the near railing: Capcom's order**, the
       same under `--original '*'` and with the layering off (the rule is
       held off there by the railing's cell records, as the entry says).
       The owner (2026-10-07): fine as it is, the railing has holes;
     - **the sky stopping short at the left, and the stretched sliver at
       the right: fixed.** The sky is `EffectKind18Sub15_Draw`'s (the area's
       own effect, `effect_5c.cpp`): a 320-wide haze band, three scrolling
       cloud strips clipped to 0..320 and four gradients; a strip starting
       past 320 was drawn backwards into the band. Widened to the picture's
       edges under DIV-0041 (its entry, "The sea bridge's sky"). The owner
       on the capture (2026-10-07): "Sky looks perfect";
     - **the "waterfall": cause found, not yet fixed** (D239 in
       `known-defects.md`): the deck's east side faces read their texture
       word from the map by *which sides the cell has*, the sides are chosen
       once at the cell's creation from corner heights the sky effect
       rewrites every frame, and the wider cull creates the deck cells at a
       moment that gives them a south side too - so the east face reads the
       next word, a 16 x 16 sea tile stretched over 125 px. **The owner's
       call:** a data survey (which order the map authors its side words
       in) decides whether the fix is to the reader or to the allocation;
       about half a day.
  The third capture (`sprite_crop_2135_1006.png`) is a close-up of one
  of these and needs no separate reading.

## Decisions the measurements raised

- **The base tree's version of the Western data rows** ([`region-diff.md`](region-diff.md),
  2026-10-06): every build after JP - US, FR, DE and both PSP discs - changes
  `AREA004` section 8 (992 bytes) and one cue byte in 65 dragon and Ryu sound
  banks; only the PC, built from JP, lacks them (two more kinds are Western-
  or FR/DE-only). Should the cache's `base/` keep JP's rows, or take the later
  version as a ledgered divergence? **Read 2026-10-06** (section 8 there):
  `AREA004` is Dauna Mine's minecart area; its section 8 walls 72 cells that
  fill gaps between wall stubs JP already placed (column x28 at z 9..30 and
  35..65 with the doorway at z 32..33 kept, column x25 at z 9..11, row z71 at
  x 7..22) plus a 30-cell texture fix, and section 10 closes the same cells
  to battle placement - **a collision bug fix**, taken by every later build
  (the PSP took the walls but not the placement half). The cue byte is a PSX
  sound-priority fix the PC's sound code never reads; the PAL sample swap is
  regional with no clear purpose (keep JP/US). The owner's rule: a bug fix is
  worth keeping as the default. Two ways to build it: an overlay chunk from
  the player's own later disc (needs that disc), or our own fix in code - the
  72 cells blocked and the 8 placement cells closed by coordinate, a
  divergence with the later discs as precedent, for every player. Area 4 is
  in the attract demo, so the state hash's reference runs want the switch off.
  The owner chose code; **built as DIV-0080** (`BOF3X_AREA4_WALLS`, the table
  proven against the US and German discs offline). **Owed the owner's eye:**
  in the minecart area, walk the raised strip's east edge and the corridor's
  bottom edge - blocked with the fix, open with `BOF3X_AREA4_WALLS=0`.
- **Stallion's PSP recolour as an option** ([`psp-stallion.md`](psp-stallion.md)):
  palettes only (two rows in areas 67 and 166, plus the three variants), the
  fight's code identical. A toggle is a palette layer from the player's own
  PSP disc (about a day, plus a second overlay prefix in `LoadDatFile`) and
  the renamed attack hours on top through the name table. The agent
  recommends an option, not a default. **The renders are in
  `analysis/stallion/`** (`AREA067_stallion_cells_jp_left_psp_right.png`,
  `AREA166_palettes0to3_jp_left_psp_right.png`) for the owner to look at.
  **The owner recorded the route the same night**: `tools/recipes/stallion.txt`
  (956 lines, the pre-fight, the transform scene and the fight; `# save
  stallion`, imported from their slot 0), so the live check exists once the
  option is built. **The Holy Mantle lead is refuted**: nothing
  to toggle.

## Seen by the owner (struck)

- **DIV-0076, the save slot's summary** - confirmed 2026-10-06: a save with
  another member leading shows Ryu's level and name together on the load
  screen.

## Debt 3: measured by a log line, review the log

- `PartyAction_SpawnKind1B` `0x5252B0` now logs `debt3` when
  `Effect_FindFree` answers 0xFF (added 2026-10-06, `rest_1f.cpp`; behaviour
  unchanged). After a play session, grep `bof3x.log` for `debt3`. A hit means
  the wait polled a packet-pool byte ([`rest_1d.md`](rest_1d.md) L1); the
  owner then decides whether the action should end at once with no effect
  placed (a ledger entry) or stay as Capcom's.

## Parked, documented, revisit if ever seen

- **The `0x91` camp cells** ([`rest_2d.md`](rest_2d.md) L1,
  [`psx-twin-check.md`](psx-twin-check.md)): 91 real cells where the camp
  opens though the compare says refuse; Capcom's on both machines. The owner
  (2026-10-06): leave it, revisit later. A fix is a one-line divergence.
- **The one-texel POLY_FT3 colour** ([`d3d-rest.md`](d3d-rest.md) D-b): a
  triangle whose corners share one texel is flattened to a colour read from
  the PSX offsets of a primitive this port widened to floats, then the wrong
  nibble. No route reaches it, no builder of one found. The owner
  (2026-10-06): leave it documented; review if a miscoloured speck is ever
  spotted.

## Decided and built, not yet seen live

- **`Cfg_Load`'s key-line overrun** ([`shell.md`](shell.md) section 5): the
  owner (2026-10-06) wanted overrun protection; **built the same day as
  DIV-0078** (a zero-filled 32-entry table of our own, lines past it ignored;
  `shell` self-test 0 mismatches with the switch off). Visible only with a
  hand-edited `BOF3.CFG`; a check is to add 25 key lines and see the game
  still start.
