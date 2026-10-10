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

- **DIV-0081's second cut (2026-10-10, `catchup/music-tooling`,
  [`music-open-ends.md`](music-open-ends.md) 3):** four shipped rows looped
  at a period the sequence does not have - `064` and `076` dropped the last
  of their 16 bars each pass, `131` 2.9 s, `070` 20 ms. The table is now 39
  rows: listen first to `064` or `076` (should now play its whole body before
  the join) and one new row (`037`, `084`, `109`, `130`) with
  `BOF3X_MUSIC_LOOPS` on against `=0`; `131` and `070` rewind as the
  original now. DIV-0081's near-full call (a slip a pass, or the rewind) has numbers now
  (`music-open-ends.md` 5: the rewind is within -41..+7 ms of the disc's
  period on all 23; a slip loop scores worse on most).

## To review in play

0. **The game from the cache** (DIV-0089, 2026-10-10,
   [`cache-read.md`](cache-read.md) section 9). **At the machine:** with
   `BOF3X_CACHE=<a cache built with the PC's DAT>` (or the ini's `cache=`)
   and nothing installed in `DAT\`, then with `BOF3X_CACHE_DATA=0` for
   comparison: the title, a town, a fight, the menus (whatever draws from
   `FIRST.DAT`'s images - the one container whose order matters; which
   screen shows the overlapped tiles is not read), an inn (an `SND\`
   wave), English from the cache's `loc/en-US`, and `psp-art` on Stallion if
   a save reaches area 67. **Look for** any difference at all; the log's
   `DIV-0089` lines say what was read from where. Also the one-root call
   (section 6 there): keep `BOF3X_CACHE` as the one root, or the cache for
   the music only.

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

- **Garr drawn over the campfire in a campfire cutscene** (the owner,
  2026-10-07 midday, catalogued for later: "in a campfire cutscene (not
  regular camp), garr's sprite was rendered over the fire instead of
  behind/under it"; their capture
  `analysis/shots/owner_reports/campfire_cutscene_garr_over_fire_1007.webp`:
  the party round the fire at night by the tent, Momo's line "You mean
  after we came all this", Garr's sprite in front of the flames). Not
  looked at. Where to start: this is a scripted scene, not the camp menu's
  screen, so the fire is likely an effect object or a sprite of the scene
  rather than a map cell - first tell whether it is DIV-0071's layering
  rule (A/B under `BOF3X_LAYERING=0`), the sprite sort order of the scene
  (compare `--original '*'`), or Capcom's order on the PlayStation twin
  (read the PSX scene first, per the twin rule). Needs a route: the owner
  to record one with `BOF3X_RECORD` from a save before the scene, or name
  the scene so a save can be found.

- **The sort screens, the formation screen and the camp's Skill Notes**
  (the owner, 2026-10-07 morning, `tools/recipes/sortScreens.txt`: "a few
  other localization needed spots"). **Done the same morning**, DIV-0064's
  groups 7..11 and DIV-0084: the item and ability sort menus (`SORT`,
  `ManualSort`, `NormalItem`, `CombatItem`, `High AP`, `Low AP`, and the
  equipment sort's `Power` / `Defence` / `Kind`, which the route does not
  open), the camp's `SORT` / `LOOK`, `Ink`, the formation names `Normal` /
  `Attack` / `Defense` centred as the US draws them, and the zenny unit's
  Z (the letter `s` on the owner's Items screen was the overlay's repaint
  of the port's coin glyph). Replayed from the route,
  `analysis/shots/sortScreens2` frames 330, 420, 720, 1440, 1980. The
  owner confirmed the armour and weapon screens' sorts the same morning.
  **Owed the owner's eye in play:** the French and German builds' words
  (`Tri=man`, `ManSort`, `Encre`, `Tinte`...).

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
  The pupils box's 弟子: the owner's US capture (the same morning) shows the
  PlayStation draws no label box there, so DIV-0083 leaves the port's out
  under a Latin overlay (`analysis/shots/master_labels2/masters.png`).

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
     - **the "waterfall": fixed and seen, 2026-10-08** (DIV-0085, D239):
       the deck's edge cells gained a south step from the sky effect and
       their cliff read the next tile's sea texture; both faces now take the
       cliff word. The owner, the bridge wide, walked several times: "I
       can't notice anything off about the deck edges", the streaks gone.
  The third capture (`sprite_crop_2135_1006.png`) is a close-up of one
  of these and needs no separate reading.

## Decisions the measurements raised

- **34 French and 11 German messages the PC build leaves Chinese** (2026-10-10,
  [`loc-build-disc-only.md`](loc-build-disc-only.md), the disc-only build's
  open question 1): where a disc's slot table is shorter than 256 entries,
  `loc_build.py`'s PC path measures a slot's offset against the PC's table
  size, so the disc's first messages look out of range and the Chinese stays.
  The disc-only path reads them right. Fixing the PC path changes today's
  fr-FR / de-DE output (a ledger entry, the layers rebuilt); the owner says
  whether, and whether to look at one of them first (the doc names the areas).
- **What the engine holds as layout for a disc-only build** (2026-10-10,
  [`exe-import-engine.md`](exe-import-engine.md) section 5, extending step 8's
  call 4): the 9,142 code pointers, the 117 pooled zero objects past the last
  named table, and the 147 `.bss` / 63 `.rdata` pointer words can come from no
  disc. Hold them in the engine (as `exe-pointers.tsv` already holds addresses,
  never bytes), or stop at "the PC install only" for them? Not blocking: the
  PC-sourced build is byte-identical either way.
- **Decided 2026-10-10: minimp3 replaces Capcom's MP3 decoder** ("I agree
  with using minimp3 unless there is a known decoding bug that has been
  resolved" - none: [`mp3-decoder-choice.md`](mp3-decoder-choice.md), 42 of
  the decoders' 57 items token-identical, the 15 others casts and naming but
  for dr_mp3 keeping the bit reservoir on a `pcm == NULL` call). Recorded in
  [`platform-layers-plan.md`](platform-layers-plan.md) 2.4; nothing to hear
  until it is built, then the ledger entry's PCM bound and the loop seams.

- **`symbols.toml` quotes table values** ([`exe-tables-by-build.md`](exe-tables-by-build.md)
  section 7, 2026-10-08): about 25 `[[data]]` evidence strings carry six or
  more of a table's numbers in a row (`Battle_DamageVarianceTable`,
  `Steal_RateTable`, `Field_MoveSpeeds`, `WorldMap_Records`, the CLUT tables
  and others listed there), older than the step that found them. Rule 1 says
  values from the game stay out of the repository; the fix is to replace the
  quoted numbers with what they mean and how they were read. **Done
  2026-10-08** (the owner: the values are public knowledge, recreated in
  FAQs, taken from the sibling before the discs verified them - sunset them):
  25 entries reworded in the tree, the addresses, counts, strides, readers
  and PSX cross-references kept, the value runs replaced by their shape
  (eleven are tables of game numbers, the rest engine constants - CLUT
  strides, op lengths, direction angles, cell codes). The seven address
  lists the regex also caught (`Gte_Vertices`, `BattleObj_StateTable`,
  `MenuList_Kinds`, `Scena00_Runs` and three more) are function and data
  addresses, not values, and stay. **Not done, the owner's call:** a history
  rewrite - the strings stay in `git log`. Recommended against: the figures
  are public, the last rewrite cost every cited hash (CLAUDE.md rule 7), and
  the licensing path rests on the tree's engine / data split, not the
  history's. Also open: the same kind of value runs in `docs/` (known-defects,
  the group docs, `importer.md`'s list of the eleven world-map areas) were
  not scanned.

- **Step 3's four calls** ([`importer-transforms.md`](importer-transforms.md)
  "The owner's calls", 2026-10-08): (1) a disc-only player's dial page - the
  PlayStation's buttons as built, or a keyboard-legend layer independent of
  the PC's art; (2) the 40 French and 54 German enemy names over 8 bytes -
  Chinese on the PC today, blank over the cache: abbreviate in `loc_build.py`
  or widen DIV-0053's draw; (3) the 14 port-edited arenas - read what the
  edits fix, or accept the disc's decode with a ledger entry; (4) `AREA004`
  from a Western disc alone brings the re-texture (DIV-0080's note) - accept
  as a by-source difference, or refuse. Nothing to look at until the engine
  reads the cache.

- **Step 8's four calls** ([`exe-import.md`](exe-import.md) "The owner's
  calls", 2026-10-08): (1) `recipes/exe-pointers.tsv` in the repo - 3,134
  lines of addresses and counts from the executable, no values; kept under
  rule 1's reading, say if it should be generated on the player's machine
  instead; (2) blank the PC-source image's six name tables and move the
  Chinese names to `loc/zh-CN/`, so `base/exe/` is one thing from every
  source; (3) a PSP-only `base/exe/` carries the PSP's level table and
  consumable 87 and no `sin_table` - accept and ledger, or require a PSX disc
  or the PC; (4) the data pointers from a disc - held by the engine, or a
  rebuild transform (93 % exact; recommended).

- **Decided 2026-10-08: the disc's music by default** ("default to disc-delivered
  music if available; there doesn't seem to be a compelling reason to use the
  PC-delivered music unless you absolutely have to"). Recorded in
  [`unified-data-plan.md`](unified-data-plan.md) section 6; step 9 moves up,
  the loop table stays as the PC-only fallback. What it asks next: the
  listening session that gates step 9 (`bgm-comparison.md` 10), and whether
  the 11 jingles (step 6's call 3) follow the same rule - from `S_XA00.STR`
  on the disc, by the engine playing a kind-0 stream from PCM.

- **Step 6's three calls** ([`sound-import.md`](sound-import.md) section 6,
  2026-10-08): (1) the PAL discs' 8 swapped area banks - refused by hash
  today, so a PAL-only player lacks 8 containers' banks; accept as that
  build's? (2) the port's converter wraps where the SPU clamps - 9 sample
  values in 5 sounds, a full-scale click on the PC (`AREA000`'s bank holds
  one: a listen first); clamping is a DIV and those banks stop being the
  PC's bytes; (3) the 11 jingles for a disc-only player - an MP3 encoder in
  the import (licensing), the engine playing kind-0 streams from WAV (`PURE`
  / `KARA` 47 MB each, streamed), or PC-only.

- **Step 4's six calls** ([`opt-layers.md`](opt-layers.md) "The owner's
  calls", 2026-10-08): `psp-art` one layer or two (two chosen: P6's ten rows,
  `psp-tiles` the rest); the PSP map bands a layer (chosen) or a rule; what
  the launcher offers (recommended: a "PSP extras" group of boxes); the
  `en-150` names layer the 8 renames only (chosen) or PSP-EU's whole tables;
  the PSP content left out (logo, title page, button labels, `SCENA17`, P8,
  the level table, consumable 87); a preset with layers on. **To look at once
  built:** Stallion with `psp-art` on and off (fight 24, area 67; area 166's
  fight 48), ability 116's banner under `psp-names-en-150`, `AREA128`'s dock
  wide with `psp-maps`, a few `psp-tiles` areas.

- **The world map's gauge words in French and German** ([`importer.md`](importer.md)
  section 3, 2026-10-08): the port's dial page (the keyboard legend, kept for
  every language) restyled the ENGINE / OVER HEAT gauge frames with the words
  in English, as JP and US have them. The French and German discs translate
  them (MOTEUR / SURCHAUFFE) in 7-row strips inside the frames, where the
  port's swirl overlaps the first letters. Splicing the disc's word rows into
  the port's page is possible with a rectangle diff (no tool yet), as a
  ledgered divergence under the French and German overlays only. **Wants the
  owner's eye on a render first**, then a yes or no. Seen on the way, and settled
  the same day: four areas (`AREA104`, `127`, `134`, `164`) whose plate pages
  the US disc left in Japanese are **not a gap** ([`importer.md`](importer.md)
  section 3).

- **The base tree's version of the Western data rows** ([`region-diff.md`](region-diff.md),
  2026-10-06): every build after JP - US, FR, DE and both PSP discs - changes
  `AREA004` section 8 (992 bytes) and one cue byte in 65 dragon and Ryu sound
  banks; only the PC, built from JP, lacks them (two more kinds are Western-
  or FR/DE-only). Should the cache's `base/` keep JP's rows, or take the later
  version as a ledgered divergence? **Read 2026-10-06** (section 8 there):
  `AREA004` is Dauna Mine's minecart area; its section 8 walls 72 cells that
  fill gaps between wall stubs JP already placed (along the raised strip's
  east edge with a doorway kept, the north end of a column on its west side,
  and the corridor's bottom edge) plus a 30-cell texture fix, and section 10 closes the same cells
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
  proven against the US and German discs offline). **Changed 2026-10-10** at
  the owner's asking (no Capcom table in our code, if we can): a rule from the
  JP map's own data was looked for and **does not exist** - 58 other open
  cells in area 4 have the same local heights and neighbours as the 72
  ([`region-diff.md`](region-diff.md) 10.1) - so the table is gone and the
  walls are the **`area4-walls` layer** from the player's own US, European,
  French or German disc. **On by default since 2026-10-10** (the owner's
  word): a build with a Western disc builds it, `install` installs it, and
  the launcher plays it whenever it is installed and the ini's `opt=` is
  empty; `opt=none` turns it off (DIV-0080). **Without a Western disc, area
  4 is the shipped open map.** **Owed the
  owner's eye:** with the layer on, in the minecart area, walk the raised
  strip's east edge and the corridor's bottom edge - blocked with the layer,
  open without it.
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

- **The launcher's Music, Cache folder and PSP extras boxes** (2026-10-10,
  [`launcher-settings.md`](launcher-settings.md) 3.1; the owner's call 3 of
  [`opt-layers.md`](opt-layers.md)): driven by a script, never by hand - a
  look at the dialog's layout at the owner's DPI, Browse... for a cache
  folder, and the PSP boxes with the PSP layers installed.

- **`Cfg_Load`'s key-line overrun** ([`shell.md`](shell.md) section 5): the
  owner (2026-10-06) wanted overrun protection; **built the same day as
  DIV-0078** (a zero-filled 32-entry table of our own, lines past it ignored;
  `shell` self-test 0 mismatches with the switch off). Visible only with a
  hand-edited `BOF3.CFG`; a check is to add 25 key lines and see the game
  still start.
