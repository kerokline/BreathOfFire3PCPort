# The world map: the owner's route, its A/B, the compass needle, and the next wave's queue

**Status:** IN PROGRESS (2026-09-23)

The owner recorded a route through the world map on 2026-09-23
(`tools/recipes/worldMapAndAreaTransition.txt`, save 3, 2,143 frames,
[`input-script.md`](input-script.md) §5a): the title, slot 3, out of the town
(the inn's area) to the world map - the Yraall Region, two place plates
passed - into an area on the coast, back to the map, and into two adjacent
areas. This is what it showed, what it reaches, and how that orders the next
takeover wave.

## 1. Captures

`tools/recipes/worldMapAndAreaTransition_ab.txt` puts a one-frame shot every
60 frames (35 shots; `tools/recipe_shots.py --every 60`). Runs, all local
under `analysis/shots/`: `worldmap_orig` (all Capcom's but the language
machinery, DIV-0022 and DIV-0004, under Capcom's DirectDraw device),
`worldmap_ours` (everything ours, every pixel-changing divergence off),
`worldmap_ours2` (the same after DIV-0044). The map is on screen in
`f01140`..`f01380` and `f01620`..`f01740`.

What the map frames show, at 640 x 480: the dial in the top-left corner with
the port's keyboard legend beside it (`SPACE`, `X`, `ENTER` - the PSX's
`○ / × / START`); the region's name in a box at the bottom, **already
English** (`Yraall Region`: text through the area script's slot 4, so the
stage 2 overlays carry it); and the **place plates** over the spots, in
Chinese - painted, not text (§5).

## 2. The A/B

`analysis/validate_worldmap.sh` (local; the settings are in its header)
plays the `_ab` recipe on both sides from a scratch copy of the launcher whose
`bof3x.ini` is windowed, `renderer=1`, `wide=0`, `scale=2`, with no
`bof3x.window` beside it, so both clients are 640 x 480 (the owner's build
directory is wide at 6x and remembers its placement; `tools/input_run.py`
took `--launcher` for this, as `attract_run.py` had). Then it plays the
route once more all original under the all-calls tracer for §4. About six
minutes.

Result (`analysis/attract/worldmap_batch.log`): 35 shots a side, none black,
**7 of 35 identical**. Two classes of difference:

- every field frame, 1 to 114 pixels scattered on tile edges - the
  DirectDraw-against-Direct3D-11 class `rb1` measured on the attract A/B
  (28 of 55 identical, the rest 1..12 pixels; [`HANDOFF.md`](HANDOFF.md)
  000000 item 1 has the pixel-offset experiment). 114 is more than 12 and
  the frame is an area entry under a fade; not chased.
- **every world-map frame, 2,365 pixels**: a purple sliver from the dial to
  the party's position on our side and nothing on Capcom's. §3. After
  DIV-0044 the map frames differ by the needle alone, ~650 pixels
  (`analysis/attract/worldmap_ours2_compare.log`).

## 3. The compass needle (D41, DIV-0044)

The owner asked what the purple triangle was, then found PSX screenshots:
the dial holds a red-to-blue diamond that turns with the map. The PC port
draws no needle at all.

The reading: `0x408530`, once a frame from the map's frame function
`0x404390`, pushes the matrix, loads the map's rotation `0x929EC8`, zeroes
the translation, builds a `POLY_G4` over four corners at `0x9037A0` -
(-10, 0, 0), (0, -4, 0), (0, 4, 0), (10, 0, 0) - projects them with
`Gte_RotTransPers4`, takes their depths from `Gte_PrimDepths4_10B`, moves them
into the dial by `(0x9E - a, 0x76 - b)`, colours them red, purple, purple,
blue, and commits. The bisect: the sliver stayed with all 18 `D3d_*` handlers
Capcom's and went with `Display_Setup` Capcom's - the backend, not the
primitive. `BOF3X_DRAWLOG_RGB=800080` (new, `render_shim.cpp`) then logged
the quad our backend received: depths 1/4096, 0, 0, 0, so `rhw = 0.1 / z` is
infinite on three corners. Capcom's Direct3D 6 device dropped the primitive
(D41: the needle never drew on PC); ours divided by the infinity and
collapsed those corners to the screen centre. DIV-0044 clamps a corner's
`rhw` to 409.6 - depth 1/4096, the nearest the game hands the handlers - and
the needle draws (`analysis/shots/sliver_fix/wm.png`, from
`tools/recipes/worldmap_sliver.txt`, the route cut at frame 1260). Owed: the
owner's eye in game, turning the map.

Seen beside it and not read: the PSX's dial is translucent over the map, the
PC's opaque (D41's note). That is for the HUD's takeover (§4).

## 4. What the route reaches: the next wave's queue

The route traced once all original (`analysis/calltrace/recipe_worldmap/`,
the entry list `entries_plus_hidden.txt`), less the attract sequence's reach
and the shop route's:

    python tools/attract_catalog.py analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv \
      --minus analysis/calltrace/hidden_b/bof3x.callcounts.tsv,analysis/calltrace/all_a/bof3x.callcounts.tsv,analysis/calltrace/all_b/bof3x.callcounts.tsv,analysis/calltrace/recipe_shop/bof3x.callcounts.tsv \
      --out analysis/worldmap_catalog.md

**40 reached and not ours, 39 in scope** (`Task_RunAll` out). The tracer arms
only functions not already ours (642 "left unarmed as owned"), so the
catalogue is the queue and nothing else. The world map's own code is small,
at low addresses (the PSX's overlay `0x801F2C00` family, compiled into the
exe), all unnamed until now - names below are working names from the call
lists, not bound in `symbols.toml`:

| Entry | Bytes | Calls | Reading so far | Priority |
|---|--:|--:|---|---|
| `0x404160` | 32 | 564 | the map task's frame entry; calls `0x404390` | with the frame |
| `0x404390` | 453 | 562 | **the map's frame**: `Gpu_SetDrawMode`, `Gfx_CommitPrim`, `0x404560` from six sites, `AreaMap_ByteAt`, `0x531920`, `0x408530` | 1 |
| `0x404560` | 188 | 3,614 | **a sprite draw** - `Gpu_SetSprt`, `Gfx_CommitPrim`, one path through `Gpu_SetSemiTrans`; ~6.4 a frame: the dial, the legend and, most likely, the plates over the spots | 1 |
| `0x404620` | 96 | 477 | **the HUD**: two `0x404560` and `Text_DrawAt` (the region label) | 1 |
| `0x408530` | 406 | 562 | **the compass needle** (§3) | 1 |
| `0x4112A0` | 32 | 259 | no calls; "the sky" of [`widescreen.md`](widescreen.md) §3d - read: `WorldMap_PinSprite`, a sprite pinned to (160, 80), **ours** ([`area-backdrop.md`](area-backdrop.md)) | done |
| `0x572F70` / `0x572FA0` | 47 / 175 | 115,056 / 9,714 | map-view helpers over `MapView_ItemAt` and `Gpu_LinkPrim`, from `0x404620` | with the frame |
| `0x462A90`, `0x496250`, `0x496830`, `0x496A00`, `0x496AD0` | | | top-level mode helpers around the area change | 3 |
| `0x517290` `Field_LoadingFrame`, `0x5172F0`, `0x573560`, `0x5744B0`, `0x5746C0`, `0x516E70` | | | the area entry's loading frames and field-object set-up (`0x599B90`'s callees) | 3 |
| the event-script rows (`0x52EBA0`, `0x5356xx`, `0x579F00`, `0x57A3A0` `EventOp_8x`, `0x5898D0` `EventOp_Ex`, `0x579CF0`, `0x57C7A0`) and `0x591F30`.., `0x5898xx`, `0x51EBD0` | | | the areas' scripts on the way | 3 |

**The owner's order for the wave (2026-09-23):** first the compass and the
HUD - `0x408530`, `0x404620`, `0x404560`, `0x404390` - with the dial's
translucency read on the way (the PSX draws it translucent); then the
background; the place plates are high for the localisation build (§5) but
are data, not code.

**The background: identified (the owner's screenshot, 2026-09-23 night).**
In the hill area just before the world map, under `BOF3X_WIDE=1`, the sky
gradient covers the middle 320 columns and the wide bands beside it are
black. That is [`widescreen.md`](widescreen.md) §5's open item: the area's
backdrop is one Gouraud quad from (0, 0) to (320, 240) drawn by the entry
handler `AreaMap_EntryKind1` `0x571BE0` (0x147 bytes, entry 1 of
`AreaMap_EntryHandlers`; the left x is in a register, so no byte patch), and
its takeover was deferred until a recorded route reached it. **This route
reaches it**: the sky is on screen in `f00720`..`f01080` (the hill area) and
`f01500`..`f01800` (the coast). The trace did not count it because it and
its siblings `0x571D30`, `0x571E20` are absent from `entries_plus_hidden.txt`
- added by hand on 2026-09-23 with `0x571B40` (local file; `pe_hidden.py plus` regenerates it without them - re-add after). **Taken over** (group 2,
[`area-backdrop.md`](area-backdrop.md)): `AreaMap_DrawBackdrop` with the
quad's x from -53 to 373 under `Widescreen_Live()` and the original's
0..320 otherwise, its siblings `AreaMap_TextureCycle` and
`AreaMap_SlotZones` faithfully (fuzz only, never traced), and `0x4112A0` -
which is not a sky but `WorldMap_PinSprite`, the marker sprite pinned to
(160, 80), centred, nothing to widen. Owed: this route's captures wide and
narrow.

## 5. The place plates, for the localisation build

The sibling repo established it for the PlayStation
(`../BreathOfFire3Recomp/docs/TEXT_TABLES.md` "World-map plates",
`names/plates.toml`): the spot names on the world map are **paint** in each
map's texture page - the area section with `dest = 0x0E001000`, 256 KB,
1024 x 256 8-bit texels; the plates a strip of 14-row bodies with a 2-row
pointer tail, 44 / 60 / 76 texels wide; the palettes in the section at
`dest = 0x8002BE00`. Ten world-map areas (`WORLD00/AREA016`, `033`;
`WORLD01/AREA045`, `065`; `WORLD02/AREA087`, `088`; `WORLD03/AREA115`, `121`,
`151`; `WORLD04/AREA152` - `tools/plates.py`'s list there), 85 plates, 42
names, and the JP and US discs differ in exactly those pages: Capcom
repainted them to localise.

The PC port is the same: the captures show Chinese plates, and
[`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2 counts "one 14-tile block pasted
into 16 area pages" among the 47 image differences from the JP disc - 14 is
the plate height, so that block is very likely the Chinese plate strip.

**The build.** The overlay-DAT mechanism (`tools/loc_build.py`) already
carries image chunks tagged by the EMI `dest` (the title art, DIV-0014), so
an `en.AREA016.DAT` holding the US disc's `0x0E001000` section would put the
US plates on the page. Two things to settle first, both from data:

1. Whether the PC page differs from the JP page only in the plate strip
   (diff the PC `DAT` chunk against the JP section) - if so the US section
   goes in whole; if the port touched the page elsewhere, splice the strip.
2. Where the plates' rectangles live. The draw (`0x404560`, §4) takes each
   plate's texels from somewhere - the map's node table (the sibling's
   `0x80104000`, "still a TODO" there) or the sprite records - and the US
   plates are not the JP widths. If the rectangles are in a data section, the
   overlay carries that section too; its `dest` will come out of the takeover.

Then verify by capture: this route's `f01260` and `f01740` show one plate
each.

## 6. Traps met

- The MSYS2 `PATH` for the build shadows Anaconda's `python`: an
  `input_run.py` launched in the same shell failed on `PIL` before the game
  started, and the run looked like a capture that never happened.
- **The runners killed every `BOF3.exe` by image name** - `attract_run.py`,
  `input_run.py` and the batch scripts' `taskkill //IM` - and the agents'
  headless self-tests run beside them: three all-shadow runs of group 2 were
  cut short by the coordinator's live runs (`area-backdrop.md` §4). Since
  2026-09-23 night the runners track the pid their own launcher started
  (`launch()`, `game_pid()`, `kill_game()`), `kill_stale()` ends only a
  leftover game whose `bof3x.dll` came from the same launcher directory, and
  `attract_watch.py` takes the pid in `BOF3X_RUN_PID`. No script kills by
  image name any more; agents never did.
- The tracer arms nothing that is ours whatever `BOF3X_ORIGINAL` says, so a
  route's catalogue never lists a function already taken over: compare draw
  handler counts across traces by address, and expect zeros for ours.
