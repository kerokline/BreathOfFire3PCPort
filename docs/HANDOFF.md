# Handoff — next session

**Status:** IN PROGRESS (2026-09-23, night)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

Phase 0 is done; stage 1 of the owner's order of work ([`STATUS.md`](STATUS.md))
- replace what the attract sequence reaches - is **closed: the attract
queue is 0 in scope, 789 functions ours** (2026-09-23, rounds five and six,
`ab26`); the input-reached queue has begun from a route the owner recorded
("Pick up here" 0000); and **stage 2, the text swap, went from a plan to a playable
English game in one session (2026-09-20)**: `tools/loc_build.py` builds 244
overlay `DAT`s from the owner's US disc - every area's dialogue, the 44 system
pools, the item and ability names, and the US font doubled into the port's
glyph table - and `BOF3X_LANG=en` loads them (DIV-0005..0009). The owner has
played it: dialogue, narration and menus read in English and "look great";
two reports (choice lists at 12 px, a gap after the apostrophe) were fixed the
same day. Three of them are the text path's:
`Msg_SystemPtr`, `Text_DrawString` and `Text_DrawImmediate`, each fuzzed
against a clone of the original. The attract oracle passes original-vs-ours
with all of them (then a hundred and twelve) and no language set, and **the frame hash was
re-recorded the same evening** (`ab15_*`, "Pick up here" 1). Then the owner
walked the field menu while it was sampled read-only: the menu's state
machine is mapped, four defects of the 2001 menu are written down and the
first is fixed - DIV-0010, the sprite handlers' far texture edge
([`menu-screens.md`](menu-screens.md)). Details: [`dialogue-localisation.md`](dialogue-localisation.md);
do not expand this paragraph into a second copy. **2026-09-21:** what the
attract sequence still runs of Capcom's code is catalogued, the function list
turned out to miss every pointer-reached function (~7,300), and PSX functions
now pair with PC ones at scale - 3,330 pairs through the tables both builds
kept ([`attract-remaining.md`](attract-remaining.md) §3, §5). **Later the
same day an agent can walk the game unattended**: `BOF3X_INPUT` plays a recipe
of pad presses in the game's own frames and `tools/input_run.py` captures the
window at each `shot` ([`input-script.md`](input-script.md)) - the menus, the
Config screen, a loaded save's field, and, through a new game, the opening's
scripted battle. The owner judged the first captures: DIV-0010's numerals
"look good", DIV-0014's title menu "looks perfect". Then three more pieces of
the exe's own text went English from the US disc - the menu's button verbs
(DIV-0018), the battle's command labels (DIV-0019), New Game's names and the
fish merchant's (DIV-0020) - all but the merchant captured in game
([`dialogue-localisation.md`](dialogue-localisation.md) §8). **Last, the
matrix product was taken over with zeros in its padding (the owner's call,
DIV-0021), then everything the draw-order pass calls** - `Sprite_AddDrawRecords`,
`Sprite_Draw` with its CLUT and cell helpers, `DrawLayer_Open`, and
`Prim_SetTexture` under the map cells ([`sprite-draw-order.md`](sprite-draw-order.md)
§12-15); an oracle failure on the way turned out to be the harness inheriting
the owner's `language=en`, now pinned. **The day's half speed turned out to
be D5** - the float frame deadline, predicted from the code on 2026-09-19 -
because `GetTickCount` passed 6.2 days: Fast Startup keeps it running across
the owner's nightly shutdowns. The owner's short-term fix, DIV-0022, starts
the game's clock with the game: 30.00 logic frames a second (item 00000).
**Then the map cells:** two of `DrawLayer_Open`'s handlers and the two
functions under them - the record condition and the ground's elevation - are
ours, with DIV-0023 (zeros in a vertex's padding, as DIV-0021 - the owner's
call) ([`sprite-draw-order.md`](sprite-draw-order.md) §16). **That night,
section 11's parked function:** `0x57C0A0`'s discarded search turned out to be
Capcom's on both platforms and asked by no shipped script - a latent defect,
D6 - so it and its hottest caller `0x589770` are ours, faithful (§17); the
handle it reads led to the field objects' movement script
([`movement-script.md`](movement-script.md)). **2026-09-22: the movement
script is ours** - the step, the flow pass, all eight group handlers and the
counter ops, the object update around it, the attached object's update and
the sprite's screen update, twenty-one functions (155 ours), each fuzzed
against a clone with negative controls and all through one batch check in
game: captures 4 / 9 / 55 of 4 / 9 / 55 in three scenes, oracle, memory
dump, frame hash identical ([`movement-script.md`](movement-script.md) §1d).
On the way: the op-length table (the PSX's byte for byte), a decoder that
makes D6's list exact, D7 (the table's wrong length for `C1`, latent), the
port's rewritten sound ops and three PSX calls it dropped (§1a, §1c). **Then,
the same day, the handoff's five targets in parallel** - five agents in
worktrees, 52 functions, 210 ours, one batch check, all identical. **Then a second
parallel round the same day: seven agents, 119 functions, 326 ours** - the
field's mode handlers with the attract demo's scenario 16, the event script's
field side, the event script interpreter and the flag helpers, the frame
loop's remaining callees, the title's state handlers and the new-game start,
the map view's scrolling and reset, and the blocked-ahead test - 300 negative
controls, one batch check, everything identical ("Pick up here" 0000). Two
ceilings in our own scaffolding fell over on the way: the `BOF3X_ORIGINAL` and
`BOF3X_SHADOW` name lists held 2,048 characters and a whole round is more than
that, and the tracer held 256 owned functions - the second cost 90 minutes,
because a Fatal at start-up looks exactly like a hang. **That evening, a
third round in five groups: 140 functions, 466 ours** (`ab24`, after one
fix - our `Boot_Task`'s last call had compiled to a tail jump, Traps).
**That night, the fourth: 101 functions, 567 ours** - the area's links and
the drop-in party (O), the field menu's item use (P, the "title cluster"
turned out to be the 33 item handlers), sound (Q), the glyph draw (N) and
the Direct3D draw handlers with the list walk (R, built on N's vertex-block
fuzz) - through one batch (`ab25`): oracle, memory dump and frame hash
identical, the last after one fix to our own fade divergence (Traps). Five
divergences on the way, all the owner's requests and all confirmed by the
owner in game on 2026-09-23: DIV-0025 glyphs sample texel centres (D17, the
"wobbly" English text), DIV-0026 the Config controller panel, DIV-0027 the
Yes / No layout, DIV-0028 music fades per frame (D26: the PC's fades were
near instant), DIV-0029 the save slot's name clear of a cut. D18..D28
written down. **2026-09-23: rounds five and six, 222 functions, 789 ours** -
the attract sequence's last (the display calls, the texture builders, the
DIV-0010 sprite handlers rewritten) and all the owner's recorded shop route
reaches; the pad recorder (`BOF3X_RECORD`); DIV-0030; D30..D40; one batch,
`ab26` ("Pick up here" 0000). **Then the owner turned to the UI overhaul**
([`display-overhaul.md`](display-overhaul.md): window modes without a
mode-set, integer scaling, a shader present pass, widescreen) and its first
step is built: **a Direct3D 11 backend behind DirectX 6's own objects**
([`render-backend.md`](render-backend.md), DIV-0031) - one function taken
over, `Display_Setup` `0x5A5160` (790 ours), no draw handler changed; the
attract A/B against Capcom's DirectDraw path 28 of 55 identical and the
rest 1..12 pixels apart on tile edges, the frame hash, oracle and memory
dump unchanged (`rb1`). The frame hash reference is re-recorded without
the CRT's `sscanf` (`ab27`, identical both ways). The PSP release's 16:9
was read in a parallel session ([`psp-widescreen.md`](psp-widescreen.md)): Capcom
widened the view by 32 columns a side and cropped 12 rows, and re-authored
four things to suit. **Then step 3, the same afternoon: WinMain, WndProc
and the FMV player ours** (796; [`window-modes.md`](window-modes.md),
DIV-0032..0035) - a resizable or borderless window with no mode-set
anywhere, the game running unfocused with the pads zeroed (I12 built), the
frame debt clamped, the FMVs into the window - checked by `wm1` with the
window never in front ("Pick up here" 000000). **That evening a review
session** fixed the tracer's `pushfd` exit, re-recorded the frame hash
(`wm1b`), routed WinMain's calls so each `BOF3X_ORIGINAL` switch holds, and
built step 4, integer scaling (DIV-0036), and step 5, the CRT look (DIV-0037,
our own shaders - the model was GPL); then the pause lines and the window
title in English and F7 / F11 gone (DIV-0038..0040), F12's quick save read
and kept for now (I18). The owner checked it all in game. All pushed on
`phase-3/UI-overhaul`. **Next: widescreen, 426 x 240, planned in
[`widescreen.md`](widescreen.md).**

## Pick up here

The single next action, concrete enough to start without asking anyone.

0000000. **The owner's world-map route, 2026-09-23 night** ([`world-map.md`](world-map.md)):
   `tools/recipes/worldMapAndAreaTransition.txt` (save 3, 2,143 frames), its
   A/B (`analysis/validate_worldmap.sh`, a scratch launcher at 640 x 480,
   `input_run.py --launcher`) and its all-original trace. **Found and fixed:
   the compass needle** - `0x408530` builds a Gouraud diamond whose corners
   `Gte_PrimDepths4_10B` puts at depth 0, `rhw` infinite, which Capcom's
   Direct3D 6 device dropped (the PC port never shows the needle, D41) and
   our backend collapsed to a sliver; DIV-0044 clamps the corner to the
   nearest depth and the red-to-blue needle draws as on the PSX (the
   owner's screenshots). Owed: the owner's eye, turning the map. **Next
   wave, the owner's order:** the compass and HUD (`0x408530`, `0x404620`,
   `0x404560`, `0x404390` - the dial is translucent on PSX, opaque on PC:
   read why), then the background - **identified by the owner's screenshot as
   the sky gradient's black wide bands, `AreaMap_EntryKind1` `0x571BE0`,
   DIV-0041's deferred takeover, which this route reaches** (captures
   `f00720`..`f01080`, `f01500`..`f01800`; it is missing from
   `entries_plus_hidden.txt` with `0x571D30`, `0x571E20` - add them before
   the next trace) with `0x4112A0` beside it, then the rest
   of the 40 (section 4 there). **The place plates are paint** (the sibling's
   `names/plates.toml`; the PC's "14-tile block pasted into 16 area pages"
   is very likely the Chinese strip): the localisation build's next item is
   an overlay chunk of the US `0x0E001000` section per world map, once the
   plates' rectangles' home is known (section 5 there). 
000000. **The UI overhaul, steps 3 and 4 built: the window, the loop, the
   FMV player (DIV-0032..0035) and integer scaling (DIV-0036)**
   ([`window-modes.md`](window-modes.md), [`display-overhaul.md`](display-overhaul.md)
   §5; 796 ours). Step 3, 2026-09-23 afternoon: WinMain `0x4FCB00`, WndProc
   `0x4FC6F0`, `Cursor_Sync`, `Display_WindowMoved`, `Display_DeviceName`
   and `Fmv_Play` `0x59E360`, read whole, written call for call - a
   resizable window or a borderless one the size of the monitor, no
   mode-set anywhere, the game running unfocused with the pads zeroed (I12),
   the frame debt clamped at 500 ms, the FMVs into the window; batch `wm1`
   with the window never in front: oracle identical, memory dump identical
   (`clut` row 482, the known artefact). **Reviewed the same evening** in a
   separate session (the owner's request), which fixed three things
   (`window-modes.md` §4a, §5): (a) **the traced all-original runs that
   ended 16 s in** - not `Display_Setup`'s failure paths but the tracer's
   own single step over the MMX probe `0x5A9A30`'s `pushfd`, reached only on
   Capcom's software-renderer set-up, which `renderer=0` in the ini selects;
   the CRT's `__except` turned the stray step into `_exit(0x80000004)`.
   `calltrace.cpp` now clears the saved trap flag after a stepped `pushfd`
   and makes any stray step a `Fatal`; `BOF3X_EXITTRACE` logs the exit code
   and unhandled steps. (b) Our WinMain called `Display_Setup`, `Fmv_Play`
   and registered `Game_WndProc` directly, so their `BOF3X_ORIGINAL`
   switches did nothing under it (DIV-0031's way back to Capcom's
   DirectDraw included); it reaches them through Capcom's addresses now,
   checked live. (c) A quit during the FMVs tore down a display never set
   up; it returns at once, as `0x4FCD3A` does. Also `attract_run.py` refuses
   `--no-front` with Capcom's WndProc (the 0-frame trap). **The frame hash
   reference is re-recorded, `wm1b`**: `analysis/calltrace/wm1b_orig` (twin `wm1b_origb`,
   `analysis/validate_wm1b.sh`, log `analysis/attract/wm1b_batch.log`):
   original-vs-original identical on all 10,313 frames, original-vs-ours
   identical on every logic frame - only frame 0, the set-up, differs, as it
   has since `rb1` made `Display_Setup` ours (next time, compare from frame
   1 or say so). The afternoon's `wm1_ours` fell three frames behind at
   3418 and stayed there (all 263 differing frames equal the reference
   three later) - timing, seen once, not chased.
   Step 4 the same evening, to the owner's rule ("windowed options should
   be the available fixed k values, fullscreen uses largest k that will
   fit", at start-up only): the launcher's Resolution box is now "Window
   size", 2x..8x (`scale=` in `bof3x.ini`, `BOF3X_SCALE`); a borderless
   window takes the largest k that fits the monitor (6 on 3440 x 1440); the
   window opens at its target's size; DIV-0010's far-edge table follows k;
   a client smaller than the target is fitted, not cropped. k = 3 run:
   target 960 x 720, the far edge at the exact value. The launcher's
   renderer box is settled: 0 is Capcom's software renderer, 1 the HAL,
   read only by Capcom's set-up.
   **Next, in the plan's order** ([`display-overhaul.md`](display-overhaul.md) §5):
   0. **Owner's eye: done 2026-09-23 evening** - fullscreen, F8, a 3x
      window, resizing, the FMVs at 2x and 4x, running unfocused with keys
      elsewhere not reaching the game, the CRT look. F9's pause was Chinese:
      now English, DIV-0038 ([`window-modes.md`](window-modes.md) §6). Left:
      the title-bar drag (only matters under `BOF3X_BACKGROUND=0`), and
      sprite edges at k = 3 / 6 looked at closely.
   1. The edge pixels of `rb1`: `BOF3X_PIXEL_OFFSET=0.498046875` (0.5 - 1/512,
      new, no rebuild) against the 27 differing captures of the 55-shot
      attract A/B against Capcom's DirectDraw (`validate_rb1.sh` has the
      set-up); needs the window visible and uncovered, about 25 minutes.
   2. Presets (§4c): **decided and the first built** - looks come
      pre-packaged in the dll, no loader (the owner). The CRT look
      ([`crt-look.md`](crt-look.md), DIV-0037, `BOF3X_PRESENT=crt`, the
      launcher's Look box): our own four passes, since the model named,
      `crt-easymode-halation`, is GPL. Owed: the owner tunes it in game
      with `BOF3X_CRT` (defaults are a first guess), best at k = 6
      borderless. Then maybe a live toggle key.
   3. **Widescreen - the survey build is built and played (2026-09-23
      night, DIV-0041, [`widescreen.md`](widescreen.md))**: `BOF3X_WIDE=1`
      or the launcher's "Widescreen" box; the target 426k x 240k with the
      view shifted 53k in the scene shader (`src/game/widescreen.{h,cpp}`,
      `render_d3d11.cpp`, `display_setup.cpp`); the terrain cull
      `[-150, 470]`, the area-map frame pass's four `fcomp` operands
      re-aimed at our floats (they were operand addresses, not float
      addresses - §3b there); the menu backdrop, the fade tile and the save
      menu's black tile widened under `Widescreen_Live()` (0 until
      `Widescreen_Inject`, which runs last so the fuzzes see the original).
      Survey findings in §5 there. The menu and shop boxes' slide-out
      bounds are widened (fourteen operand patches, `kSlides`); the
      dialogue boxes are fine by the owner's eye. **Open:** the sky gradient
      `AreaMap_EntryKind1` `0x571BE0` needs a takeover (§5 there has the
      reading) - deferred by the owner until their quicksave-based route
      reaches it; whether any area change still shows a black centre with
      live bands; the corner pop with the margin at 100. **Owed:** the
      oracle and the frame hash once with `BOF3X_WIDE=1` (§4 there); the
      attract A/B cropped to the middle 640 columns; a live
      `BOF3X_SHADOW=map_layers` under the wide view reports the cull's
      divergence by design. Not touched: the sprite and object culls
      (§3b's table), the sky `0x571C85`, the message-box table.
   4. **The window resizes; snap or fit (DIV-0042, 2026-09-23 night):** the
      launcher's size list is gone, a "Snap" box chooses whole multiples
      (a drag lands on them, the target follows between frames through
      `render::RequestScale` and the set-up's `Rescale`) or the picture
      fitted to the client's height; the FMVs follow and never resize the
      window; `bof3x.window` beside the dll remembers the placement. The
      owner dragged it by hand: both modes "work perfect". Not measured: a
      rescale under the CRT look (DIV-0037).
   5. **The SatPixie look (DIV-0043, the same night):** the owner's chosen
      MIT shader ported to HLSL (`src/render/satpixie.cpp`,
      [`THIRD_PARTY.md`](THIRD_PARTY.md)), the Look box's fourth entry with
      an Options dialog of sliders. The vignette covers the whole picture
      by default since the preset's 4:3 shape showed on the wide view.
      Owed: the owner's tuning, the dialog by hand (only tried by code).
   Not built, loud if reached: a `Lock` of the primary or back buffer
   (`D3d_AfterDraw`, never seen requested - `Gfx_DrawOTag` logs the first
   request), sub-rectangle locks, depth / fog / lighting, the back buffer's
   `GetDC` (which is why the overlays are logged). The set-up's constants
   (pixel formats 1-5-5-5 / 5-5-5 / X-8-8-8, caps `0xCCD`) are what this
   machine's HAL reported; another machine's may differ, and the backend
   takes any RGB masks. Left as Capcom's in the WinMain group, typed:
   `Game_Init` `0x4FD110`, `Gfx_LinkOTags` `0x4FD290`, `DInput_Init`
   `0x5A94C0` (keyboard `DISCL_BACKGROUND`), the three shutdowns, the sound
   pause pair, `Save_QuickWrite` `0x5809C0` (F12 writes a normal save to
   slot 0 from anywhere, battles included - the owner keeps it for now and
   wants it disabled or a true quicksave before shipping, [`IDEAS.md`](IDEAS.md)
   I18; the recorder's F12 shot lands on top of it), `Disc_Probe` `0x5A72C0`.
   F7 and F11 do nothing since DIV-0040; the window title is English,
   DIV-0039 - built, self-tests 0 mismatches; the owner's eye owed.

00000. **The game's pace is done short term: DIV-0022, confirmed in game by
   the owner 2026-09-22.** The complete fix, the deadline in a double, is
   [`IDEAS.md`](IDEAS.md) I16, not scheduled. **Pace figures before
   2026-09-21 are the 31.25 band.** **The all-original runs are at half
   speed because of D5, not the tracer** (measured 2026-09-23): every
   `--original "*"` run turns DIV-0022 off with the rest, and Windows' tick
   count stood at 8.2 days - D5's half-speed band - so the hash's reference
   runs made 15.2 logic frames a second (`ab22b`..`ab26`) where ours, traced
   the same, made 29.3, and the untraced oracle 28.9. The hash compares
   logic frames, so its verdicts stand. **Past 12.4 days of uptime (about
   2026-09-27 at this rate; Fast Startup keeps it counting, only a Restart
   resets it) the original clock stops pacing and, by the code, draws
   nothing - every all-original reference run would break.** Run reference
   sides with `*,-Game_Clock` (the A/Bs already do: the clock changes pace,
   never logic), which also halves the hash step; until then keep hash runs
   at 11 minutes or more.

0000. **Stage 1's attract queue is closed; the input-reached queue is open.**
   **2026-09-23, rounds five and six: 222 functions, 789 ours**
   ([`takeover-queue-round5.md`](takeover-queue-round5.md),
   [`takeover-queue-round6.md`](takeover-queue-round6.md)). Five: S the
   display environments and `Snd_LoadBank` ([`display-env.md`](display-env.md)),
   T the page textures and the fake DirectDraw `src/game/ddraw_fuzz.*`
   ([`tex-page.md`](tex-page.md)), U the glyph and cell textures
   ([`tex-cells.md`](tex-cells.md)). Six, the first input-reached queue - what
   the owner's recorded shop route (`tools/recipes/shop.txt`, save 3,
   [`input-script.md`](input-script.md) §5a) reaches and the attract
   sequence does not: V1 the leader's walk and the event script's placement
   ops ([`event-ops.md`](event-ops.md)), V2 the event script's object ops
   ([`event-objs.md`](event-objs.md)), W stats and inventory
   ([`char-stats.md`](char-stats.md)), X save / load / the inn and the title
   flow ([`save-menu.md`](save-menu.md)), Y the menu and shop windows
   ([`menu-windows.md`](menu-windows.md)), Z the party members' follow
   ([`member-sprites.md`](member-sprites.md)), M map patches and three D3D
   handlers ([`field-misc.md`](field-misc.md)), and D the DIV-0010 sprite
   handlers rewritten with the divergence inside (`SpriteFarEdge`,
   [`sprt-draw.md`](sprt-draw.md)). `python tools/attract_catalog.py ...`
   (item 1's command) now prints **0 in scope**: what the attract sequence
   still runs of Capcom's is the CRT, the MP3 decoder, the Windows shell,
   the task system, fifteen functions of run-once platform set-up (the
   owner's scope - [`IDEAS.md`](IDEAS.md) I8 / I12 replace them) and seven
   entries that are not functions. **The batch** (`analysis/validate_ab26.sh`,
   log `analysis/attract/ab26_batch.log`; then `validate_ab26b.sh`): self-tests
   at 0 mismatches in both languages; the shop route A/B 35 of 35; the
   attract captures all-Capcom against all-ours 55 of 55; oracle identical at
   7,478 frames; memory dump identical in all three regions; frame hash
   original-vs-original identical on 10,063 frames, ours one frame off -
   5524, the `sscanf` of the music buffer's address (Traps), reproduced
   and explained, not game logic; the backdrop A/B 11 of 12 (kind 5, DIV-0030);
   and `ab25b`, round four's owed captures, every pair identical and none
   black. DIV-0030 (the menu backdrop past Config's four draws nothing - the
   owner's call); D30..D40 latent.
   **Next:**

   0. **The frame hash reference is `analysis/calltrace/wm1b_orig` since
      the WinMain takeover** (twin `wm1b_origb`, ours `wm1b_ours`; item
      000000), recorded from a scratch launcher (`renderer=1`, windowed)
      with the foreground held under the `wm1` list. `wm1_orig` /
      `wm1_origb` are the 0-frame artefacts of the unfocused first attempt.
      Before it, `ab27` (2026-09-23): the reference was
      `analysis/calltrace/ab27_orig`** (twin `ab27_origb`, ours `ab27_ours`),
      recorded with the CRT's `sscanf` engine (`0x5BCF64` and seven callees)
      out of `entries_logic.txt` (`entries_logic_0923b.txt` is the list
      before), the reference sides at `--original "*,-Game_Clock"`, 6 minutes
      a run: 10,259 / 10,310 / 10,316 logic frames, original-vs-original
      identical on 10,312, original-vs-ours identical on 10,318. The clock
      kept on the reference is now verified. Then `0x5A5160`'s size in the
      list was corrected to `0xA55` when it became ours (`rb1`).
   1. **More recorded routes** - the owner plays with `BOF3X_RECORD` (F12 for
      a shot), then the route is A/B'd and traced once all original, less the
      attract reach (`attract_catalog.py --minus`, the command in
      `takeover-queue-round6.md`), and that is the next round's queue.
      Battle waits (the owner, 2026-09-23): it wants a save-state system or a
      deterministic way to start a fight first.
   2. **Named on the way and not taken**: nine pointer-reached window-task
      handlers `0x59B7B0`..`0x59BEA0` (Y); the leader's sub-state 2
      `0x52E110` and where a step lands `0x52E580` (V1); the member's states
      2..8 and `0x527640` (Z); the save block builder `0x5806F0` and the
      writer `0x5809C0` (X); `0x591810`, `0x591B60` (`Inventory_Remove`),
      `0x591CC0` (W, missing from every entry list); `MsgBox_SystemChoice`
      (D22).
   3. **Not reached by any check yet**, fuzz only: each round's doc lists its
      own - the rounds before, group C's kinds 0-2 and fades, the title's
      states 6 and 7, and so on; this round, the software-surface and
      direct-colour texture paths (T, U), step codes 2..7 and an encounter
      firing (V1), the floor-damage kinds (V2), `Stat_AddResist` and
      `Equip_PreviewSet` (W), `TitleFlow_NewGame` (X).
   4. **`pe_hidden.py`'s blind spot, again**: round six found ~40 functions
      the queue missed (X's title flow after `Snd_LoadBank`, Z's follow states,
      V1's three) and a dozen wrong sizes. Expect it per group.

   **The parallel method** (worked twice on 2026-09-22; seven groups in 18
   to 42 minutes of agent time each, then about 2 hours of merge and batch):
   - Split by *file*: each group a new `src/game/*.cpp`, its own shadow
     name, its call at the end of `inject_all.cpp`. Every call out of a
     clone re-aimed at a recorder, so inject order does not matter.
   - **Before spawning, commit, and register the boundary callees** - a
     function one group calls and another takes over - in `symbols.toml`
     with provisional names, so no two groups bind one address
     (`gen_symbols.py` refuses that).
   - Agents self-test headless: `BOF3X_SELFTEST_ONLY=1` (SCAFFOLDING §2),
     never the game window, never `taskkill //IM`. The live batch runs once,
     after the merge.
   - Merge one branch at a time, the build and `BOF3X_SHADOW='*'` after
     each - and read the build's output, not just the self-test's exit code:
     a failed build leaves the previous `bof3x.dll` in place and the
     self-tests pass on it. Resolve `symbols.toml` entry by entry, never by
     text (the scratchpad's `merge_symbols.py` did it three-way this round,
     keyed on each entry's `pc`); `CMakeLists.txt` and `inject_all.cpp`
     conflict every time and want both sides, but the source list's closing
     paren has to end up on the last line only. Traps below: the worktree
     base, `symbols.toml`, the trace list, and our own ceilings.
   The per-function pattern inside a group is unchanged: read it against
   its PSX twin, type every callee, clone with every call re-aimed at a
   recording stand-in, fuzz from random state with each branch's
   boundaries seeded, then plant bugs until each behaviour-changing one is
   refused (Traps: a quiet stand-in, a change that changes nothing).

   Found on the way, not yet acted on: **`pe_hidden.py` misses the functions
   after an inline jump table** - ten or so at `0x593950`..`0x594240`, none in
   `entries.txt` ([`attract-remaining.md`](attract-remaining.md) §3); and
   `pe_funcs.py` sizes run on through pointer-reached neighbours (`0x56FF00`
   was 0xBA6 bytes, really 0x118 - fixed by hand in `entries_logic.txt`).
   Fixing the seeding changes the frame hash's content, so re-record the
   reference with it.

000. **The rest of the exe's labels, the same way**
   ([`dialogue-localisation.md`](dialogue-localisation.md) §8 has the method
   and the table of chunk kinds). Found and waiting: the **stat labels** at
   `0x669CF0` (攻击 / 防御 / 智力 / 速度 on Status and Equipment; the US
   `Pwr` `Def` `Int` `Agl` stand before the verb table in `STATUS.EMI`) - the
   draw is unread, so first check whether `Pwr` fits the box; the skill
   list's header `龙技` at `0x66A220` and the item list's `物品`; the battle's
   target banner (seen as `攻 击` after choosing Attack); the turn counter's
   残留 / 回合 at `0x669D10` / `0x669D18`. `BOF3X_TEXTLOG=1` gives each one's
   address. Captures: `tools/recipes/menu_screens.txt`,
   `battle_commands.txt`. Still Chinese beyond those: enemy names (battle
   data, 12-byte fields; the sibling's `names/enemies.toml` has the JP side)
   and place names. **A real encounter** needs a deterministic boss fight or
   save states (owner, 2026-09-21: walking on save 5 meets one, but not
   repeatably). **Saved names** stay as they are - the owner's decision; a
   language-independent name system is future work, not scheduled.

00. **The PSX pairing, step 1 of [`attract-remaining.md`](attract-remaining.md)
   §5.1's list: draw the divergence map.** `python tools/psx_pair.py areas &&
   python tools/psx_pair.py fill && python tools/psx_pair.py propagate`
   (five minutes; reads `../BreathOfFire3Recomp`, writes
   `analysis/pairs_propagated.json`) rebuilds the 3,330 pairs. Unpaired PSX
   runs inside paired neighbourhoods, and PC functions with no twin inside
   paired blocks, should mark where the port was rewritten - the owner
   expects the text path and the dropped naming / options screens there.
   Treat the `callers` and `table-anchored` tiers as a few points less sure
   than the rest (the table in §5.1), and never use `call-disputed`. Then
   scenario overlays, the name import (as `hypothesis`), and seeding
   `pe_funcs.py` from the pairs - which changes the frame hash's content, so
   re-record the reference with it.

0. **Stage 2: what is left of the text swap.**
   [`dialogue-localisation.md`](dialogue-localisation.md) has the whole state;
   §6 is the open list. In the order the owner will meet them:
   - **The owner plays with `BOF3X_LANG=en` and sends screenshots** - that is
     how both of the session's engine bugs were found, and it is faster than
     reading. Owed a look: the choice lists at 8 px (fixed after the last
     screenshot, unseen since), item and ability menus for clipping, a
     pick-up, the masters' talk, a long area (`AREA090`, `175`-`185`: the ones
     that only fit since the system pool moved, DIV-0007).
   - **The Config screen is English and confirmed in game** (DIV-0015 /
     DIV-0016, [`config-screen.md`](config-screen.md)): six labels, seventeen
     options and six controller names from the disc's `START.EMI` through a
     kind-7 chunk, drawn from the donor's 8 x 8 UI cells, tripled, named two
     bytes at a time. The owner saw it ("much closer") and found the text
     two pixels low; the earlier two-pixel drop is removed and that build is
     the next thing they look at - seen, right. The selected row's large
     lettering is DIV-0017, seen and confirmed 2026-09-21. **The 8-unit quad scales a whole 24 x 24 glyph to 16 x 16** rather
     than cropping it - the fact that decides which cells any UI string wants.
     `0x516E70` is now read (same glyph table, 8 x 8 quads, flat 8 advance,
     (u, v) from `0x65F5A8`), so the list below is one shorter. The two
     buttons above the panel are DIV-0018 (`Quit` / `Init`), which also
     translated the button rows of Items, Ability, Equipment and Tactics.
   - **The next "still 12 px" report** will be one of seven unread functions
     that call `Text_DrawAt` a character at a time: `0x45B490`, `0x45B5F0`,
     `0x460730`, `0x460920`, `0x466260`, `0x4B1090`, `0x4B11F0`. Three pens
     are done: `Text_DrawString`'s, `MsgBox_Step`'s (a re-aimed call site,
     `bof3::RetargetCall`), `Text_DrawImmediate`'s. Also still 12: the
     stepper's effect draw `0x4987E0` and the small 8 px UI font `0x516E70`
     (its own glyph arithmetic, unread).
   - **The title menu is English and confirmed in game** (DIV-0014,
     [`title-menu.md`](title-menu.md)): the owner judged a recipe's capture
     "perfect", 2026-09-21. Still open from [`USER_CHECKS.md`](USER_CHECKS.md)
     6: the two-row layout. The load screen behind it reads "Load game?" /
     "Loading complete" in English already.
   - **Still Chinese:** enemy names (12-byte fields in battle data), place
     names (character names are DIV-0020's, for a new game), text baked
     into artwork, and any string in the
     executable outside the six name tables. Enemy names are the obvious
     next converter: the sibling's `names/enemies.toml` has the JP side.
   - **Longer names.** The port's name fields are 16 bytes against the US
     disc's 12 (DIV-0008), so `BallockKnife` could be `Ballock Knife` - if the
     menu column has the pixels. Owner's call, in game; a new ledger entry.
   - **A better upscale.** The font is the US cells doubled. `loc_build.py
     export` / `all --glyphs PNG` / `all --upscaler CMD` are the round trip;
     no image is committed and the table's SHA-256 is printed for comparing
     builds. The PSX draws nibble 7 as a dark drop shadow; ours copies it as
     the ramp's grey - worth a look when redrawing.
   - German and French: the discs are in `CDImage/`; their accented cells are
     unread and only 10 glyph slots are free past the 100 English ones.
0a. **The menu's defects** ([`menu-screens.md`](menu-screens.md) section 3), all
   present in the 2001 release and all the owner's to look at in game:
   - **DIV-0010 is confirmed** by the owner off an in-menu A/B capture
     (2026-09-21, [`input-script.md`](input-script.md) §5).
   - **DIV-0011**: Config's panel frame (owner: "looks right") and the
     reserve list's on "change party members" (seen in the owner's session,
     `analysis/d1/point/s009.png`; the owner has not commented) - drawn as
     the PlayStation drew them
     (`src/game/menu_frame.cpp`; off with `BOF3X_ORIGINAL=Menu_DrawFrame`).
     It costs some 590 sprites a frame - if anything else on that screen
     goes missing, the packet pool is full.
   - **The text "glow" is two things, both answered.** Bilinear filtering
     with a low alpha test (DIV-0012: `BOF3X_FILTER=point` is the clean look,
     opt-in, played by the owner), and a white text CLUT the PC team
     brightened (DIV-0013: the English overlay restores the disc's row, so
     the drop shadow is dark again - **rebuild the overlays**, `loc_build.py
     all`). A live toggle is [`IDEAS.md`](IDEAS.md) I15 and waits on input.
   - The screen title's box and centring are still unread; `0x574AB0` (a box
     out of semi-transparent `POLY_FT4`s) is the lead. The way in that worked
     for the frames: search the PSX disc's `STATUS.EMI` for the call's
     constant arguments, read the PlayStation function, then look for what
     the PC kept of it.
   - Ability (state 3) was only seen closing, and the list cursors of Items
     and Equipment are unfound: one more walk under
     `python tools/mem_watch.py --seconds 600 929F00:16` and a wider range.
1. **The frame hash reference is `analysis/calltrace/ab25_orig`** (twin
   `ab25_origb`), recorded all-original 2026-09-22/23 at 11 minutes under
   `entries_logic.txt` with the fourth round's 71 unlisted functions added
   (`entries_logic_0922d.txt` is the list before): all 10,062 frames
   identical original-vs-original, and original-vs-ours with all 567 injects
   and every DIV on (`ab25_oursc`). The list change alone moves the hash's
   content, so `ab24_orig` and everything before it compare as 117
   differing frames - that is the re-recording, not a regression. **An all-ours traced
   run is now twice as fast as an all-original one** (19,318 frames in 11
   minutes against 10,062: owned functions are not traced), so the pair is
   compared over the shorter run's frames. **The
   trace list must name every owned function with its size** - an owned
   function missing from it keeps its callers' return addresses in the
   all-original run but not in ours, and the hash differs on thousands of
   frames with every count equal (`ab21_*`: 5,236 frames, 23 functions
   missing; the list before is `entries_logic_0922.txt`). Add each takeover's
   functions to the list before its batch.
   **Frame 5524 is same-configuration noise**: one of two all-ours runs had
   342 calls there against 346, the other matched the reference - the same
   frame [`psx-library-layer.md`](psx-library-layer.md) section 4 met. One
   differing frame at 5524 wants a re-run, not a hunt.
   **What is left is catalogued** in [`attract-remaining.md`](attract-remaining.md)
   (2026-09-21): 541 reached functions not ours, 364 of them outside the MP3
   decoder and the CRT, grouped with counts and callers - and 89 of them
   *hidden*, reached only through pointers and invisible to `entries.txt`
   until `tools/pe_hidden.py`. Its §5 is the owner's follow-up question
   answered: the PSX area descriptor table has a PC twin at `0x667590`, and
   area overlays sit in the PC exe as ordered blocks (`tools/psx_pair.py`); the
   next steps are listed there. **Fixing `pe_funcs.py` to seed pointer-reached
   entries changes the frame hash's content - re-record the reference with it.**
   **Then keep working the queue** - regenerate it first (`python
   tools/calltrace.py queue analysis/calltrace/all_b/bof3x.callcounts.tsv` -
   the argument is the *counts* file. `all_b`, 2026-09-20, is a full-list run of
   12,813 frames, a whole attract cycle, where `all_a` stopped at 3,072; what
   is ours is unarmed in it, which a queue of what is *not* ours does not
   mind. Call counts in the docs are `all_a`'s up to the integer library layer and `all_b`'s from the x87 batch on; each says which. And drop the
   `< 0x5A6000` habit: the library layer above it is where the calls are).
   Known and not taken over:
   - **The matrix product `0x5A7D70`** and the rotations on it: ours,
     2026-09-21 (item 0000; DIV-0021). The product itself is no longer
     parked; `analysis/experiments/experiment_mulmatrix.cpp` is history.
   - Library leaves still unread: `0x5A9700` (341 bytes, five indirect
     calls), `0x5A7C70` (the `s16`-out `ApplyMatrix`, unreached), `0x5A6790` /
     `0x5A6780` (an 8-byte record appended to a table at `0x6BEA18`, count
     `0x7CC374`, and its reset - read: `(u16, u8, u16, u8, u8, u8, u8)` in,
     the last two packed into one byte as `((g & 0xF8) << 1) | (f >> 3)`,
     returns the index, no bound; the one reader is `0x5A32B0`, in the
     Direct3D end. Unnamed because what the records are is still unknown).
   - `0x494030`: runs **20 objects of `0x80` bytes at `0x7E11E0`** - a second
     object kind - through the handler table `0x655350` by byte `+5`, setting
     `Sprite_Current` for each. Indirect calls, so no clone; check it live.
   - `0x56D690`: `call [[0x662C80 + s8 [0x8034E0] * 4]]`, then a tail jump to
     `0x56D8B0`. A mode dispatcher. (`CloneOriginal` can re-aim a tail `jmp`
     since 2026-09-20; the *detour* side of a function that ends in one needs
     nothing special.)
   - `0x454810` is `return 1` with 152 callers; read a caller before naming.
   - `0x57C0A0`: ours since 2026-09-21 (`Sprite_ObjectByHandle`, §17); the
     PSX side had the same dead search ([`known-defects.md`](known-defects.md) D6).
   Bigger leaves still unread: `0x454AD0` (491 bytes), `0x496870` (399),
   `0x5722D0` (672), `0x5187C0` (433), `0x57C310` (431). (`0x5720C0` is ours:
   `AreaMap_Elevation`, 2026-09-21; `0x5722D0` is its sibling with a store to
   `0x903850`.)
   (`0x572A00` is ours: `Prim_SetTexture`, 2026-09-21.)
2. **Under the draw-order pass.** `Sprite_DrawPass` `0x593060` and all
   three of its callees are ours ([`sprite-draw-order.md`](sprite-draw-order.md)
   §9, §12, §14, §15): `Sprite_AddDrawRecords`, `Sprite_Draw` (with
   `0x593860` 213 bytes, not 2,644) and `DrawLayer_Open`, whose fuzz swaps
   the handler table for **recording stand-ins** - the pattern for anything
   that calls through a table. What the pass still lacks is a check of the list it builds in the real
   game - [`IDEAS.md`](IDEAS.md) I14 level 1.
   Worth doing alongside: **a struct for the sprite object.** Five files now
   address it by offset; `symbols.toml` has no struct types, so it would be a
   hand-written header, and the offsets are collected in §5 and §6 there.
3. **The Direct3D end of the image path is ours** as of rounds four and
   five and the backend: the builders `0x5A0080` / `0x5A0510`, the glyph
   lookup `0x5A2BC0`, the set-up `0x5A5160` (DIV-0031). Left: the software
   path's lock wrapper `0x5A3CC0` (only the software renderer reaches it,
   which the backend retired), the teardown `0x5A6380` and the
   enumeration callbacks (retired with it, still Capcom's bytes;
   [`display-setup.md`](display-setup.md)).
4. **Owner, in game: [`USER_CHECKS.md`](USER_CHECKS.md).** The converted saves
   are done bar one item. Still owed: a save and load through the fully-ours
   file layer, DIV-0003's failing case, DIV-0002's clean A/B; and item 5, the
   next time a party member is confused on the field: do the controls reverse
   the same with ours as with `BOF3X_ORIGINAL=Field_CopyInput`? (The owner has
   confirmed the status exists and reverses inputs.) New and optional:
   play with `BOF3X_SHADOW=Gfx_InvalidateTextures` set and look for `MISMATCH`
   in `build/bof3x.log`; and anywhere the game scrolls or copies VRAM, or
   shows a compressed picture, is the only live test there is of
   `Gfx_MoveImage`, `Gfx_MoveCells` and `Gfx_UploadLzss` - the attract
   sequence reaches none of them.

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

5. **[`IDEAS.md`](IDEAS.md) I12 - let the game run unfocused.** Asked for by
   the owner because every check tonight took the PC away for minutes. The
   mechanism is read (app-active byte `0x6BC63B`); it is a divergence when
   built. Worth doing early: it makes everything in item 1 cheaper.
6. **Stage 2's regression check.** The attract sequence's text boxes run
   the in-game dialogue engine ([`attract-mode.md`](attract-mode.md) §6), so
   item 0 inherits one. Unmeasured: which of `MsgBox_Step`'s 23 control codes
   those eight messages use (tracer detail mode), and nothing covers
   `Msg_OpenSystem`.
7. **[`IDEAS.md`](IDEAS.md) I13 - save states**, the oracle for what the
   attract sequence cannot reach (menus, system text, combat - stage 3). First
   experiment is written there.
8. The first **receipt**, and a CI job that at least compiles `src/`
   ([`STATUS.md`](STATUS.md) open decisions). The evidence a receipt would
   record now exists in three forms: `attract_diff.py`, `mem_dump.py
   --compare`, `calltrace.py frames`.
9. Finish reading the asset path: the `SND\`/`BGM\` loaders `0x587910` /
   `0x587A20`, the drive-root probe `0x5A72C0`'s caller `0x4FCB50`, the 32
   callers of `LoadDatFile`, the value-sequence search for the dropped PSX
   sections ([`asset-loading-path.md`](asset-loading-path.md) §4). For I1,
   save interchange: the PC block builder `0x5806F0` and the options bytes at
   block `+0x78`; PC-to-PSX is still static only.
10. [`known-defects.md`](known-defects.md): D1, clipped stat numerals, wants its
   A/B run and the draw path read; D3, fullscreen fallback, is unreproduced;
   the frame deadline kept in a 32-bit float is DIV-0022 (item 00000) and
   [`IDEAS.md`](IDEAS.md) I16.

## How to run things

_Commands a fresh session needs, verified on the date above._

- **Build:** `cmake --preset i686 && cmake --build build` — llvm-mingw's
  `i686-w64-mingw32-clang++` is already on `PATH` on this machine (the
  `retcomm` toolchain under `~/.local/share`), **not** the MSYS2 one.
- **Run:** `build/bof3x-launcher.exe --game bof3`; log in `build/bof3x.log`.
  This now opens the settings dialog first — **pass `--no-config` from a script
  or an agent session**, which skips it
  ([`launcher-settings.md`](launcher-settings.md) §4).
  Original behaviour for one function or all: `BOF3X_ORIGINAL=File_Read` / `=*`.
  **Windowed:** the dialog's Display box, or a two-line `BOF3.CFG` (`0`, then
  `1`) in the game directory, or F8 in game
  ([`windowed-mode.md`](windowed-mode.md)) — recommended for agent sessions,
  since it avoids the display mode-set.
  Since 2026-09-23 (DIV-0032, DIV-0035) nothing mode-sets: `display=fullscreen`
  is a borderless window the size of the monitor, and the FMVs play into
  the window. From an agent session, end a run with `taskkill //F //IM BOF3.exe`.
  **The game runs while not in front** (DIV-0033; `BOF3X_BACKGROUND=0` is the
  original's freeze), so `attract_run.py --no-front` leaves the desktop
  alone for the oracle and the hash, and `--launcher DIR/bof3x-launcher.exe`
  runs a copy with its own `bof3x.ini` (a windowed one, for a batch on the
  owner's machine: `analysis/validate_wm1.sh` shows the set-up).
- **Every start-up self-test, headless (half a second, no window):**
  `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' build/bof3x-launcher.exe --game <dir> --no-config`
  - exit 0 passed, 3 a Fatal ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2).
- **Regression check (10 min, hands off the game window):**
  `python tools/attract_run.py --out analysis/attract/ref.tsv --original "*"`,
  the same without `--original` to `new.tsv`, then
  `python tools/attract_diff.py ref.tsv new.tsv` — exit 0 means identical
  `Rand` count, message and area at every frame
  ([`attract-mode.md`](attract-mode.md) §6).
- Saves: `python tools/save_convert.py list CARD.mcr` / `info` / `psx2pc` /
  `pc2psx` — usage in the file's docstring. In Git Bash pass Windows-style
  paths (`cygpath -m`): a `/c/...` path inside a `CARD:SLOT` argument is not
  translated.
- **Byte-level check of a loader:** start `python tools/attract_run.py --out
  analysis/attract/tmp.tsv --minutes 2.4` (add `--original NAME` for the
  reference), and within a few seconds `python tools/mem_dump.py --label X`;
  then `python tools/mem_dump.py --compare A B`. Always take two reference
  runs — the pair is the noise floor.
- **Shadow check:** `BOF3X_SHADOW=Gfx_InvalidateTextures`, `=Gfx_TexCacheFind`, `=gfx_clut`, `=gfx_flush`, `=gfx_unpack`, `=gfx_vram_ops`, `=sprite_order`, `=draw_pool`, `=prim`, `=map_view`, `=sprite_anim`, `=sprite_find`, `=field_input`, `=sprite_clut`, `=draw_layers`, `=psx_gpu`, `=psx_gte`, `=psx_gte_float` (which also compares every live call of the two precision-dependent functions and counts the x87 control word), `=psx_gte_transform`, `=draw_emit`, `=draw_pass`, `=msg_pool`, `=text_draw`, `=text_immediate`, `=map_cells` (which also shadows every in-game call of the two handlers), `=title_states`, `=field_blocked`, `=frame_callees`, `=field_modes`, `=map_scroll`, `=field_event`, `=event_script` (comma-separated lists work) or `=*` before the launcher
  or `attract_run.py`; `shadow` lines in `build/bof3x.log` — a start-up
  self-test line, then a running tally every 256 calls
  ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2).
- **Takeover recipe** (each takeover since the first): read the function to its
  last instruction, quirks included; `symbols.toml` entry with the evidence
  and `impl`; implement, keeping every unchecked edge and saying so in the
  comment; if every jump stays inside it, clone it and fuzz ours against the
  clone at start-up under `BOF3X_SHADOW` (a call that leaves is fine if it is to
  something already cloned - `bof3::CloneCall`, which re-aims a tail `jmp`
  too; a callee that another module owns must be cloned before that module's
  `Inject` patches it, so such a module goes EARLIER in `inject_all.cpp`, as
  `psx_gte_transform.cpp` does; functions that only call each
  other clone as one block, as `sprite_anim.cpp` does; x87 code: set the
  control word `0x027F` around the clone's call; a callee that cannot be
  cloned - indirect calls, or simply not read yet - gets a recording stand-in,
  for the clone and for ours alike, as `draw_pass.cpp` does; a function whose
  whole call tree is already ours can instead inject FIRST, so its clone's
  calls reach Capcom's unpatched originals and the fuzz compares the whole
  tree, as `sprite_records.cpp` does - and when the tree reaches the matrix
  product, compare a MATRIX's padding word apart, DIV-0021), then break ours on purpose and see
  the fuzz refuse to run; live, all ours: `mem_dump.py --compare clutref_a X`,
  `attract_diff.py orig_a.tsv X.tsv`, and the frame hash before a merge - **with
  an original-vs-original run beside it**, the noise floor, its reference
  sides `--original "*,-Game_Clock"` so D5 does not hold them to half speed
  (item 00000) (one
  background command can run the oracle and then the hash pair, about 16
  minutes, with `mem_dump.py` started beside it); say
  in the doc what none of that reached; one commit per file of functions.
- **Rebuilding the frame hash's exclusion list** (when an original-vs-original
  pair differs): a full-list all-original run - `BOF3X_CALLTRACE=<abs
  path>/entries.txt BOF3X_CALLTRACE_MODE=all`, `attract_run.py --original "*"
  --minutes 9` - its `callcounts` concatenated after `all_a`'s, then `python
  tools/calltrace.py wallclock <merged> --static 59E000-5A6000,5A9600-5AB000
  --also 5BC8E0,5BDA20 --check <old list>`. To see which calls a differing
  frame holds, `BOF3X_CALLTRACE_DETAIL=lo-hi` on both sides and diff
  `build/bof3x.calldetail.tsv` per frame ([`call-trace.md`](call-trace.md) §6).
- **Scripted input and captures:** `python tools/input_run.py
  tools/recipes/field_menu.txt --out analysis/shots/X --lang en` - recipes in
  `tools/recipes/`, the language in [`input-script.md`](input-script.md) §3.
  Keyboard and mouse off for the run. The field menu button is per save:
  `press @0x903584`, not a shape.
- **Where does this on-screen string live?** `--env BOF3X_TEXTLOG=1` on
  `input_run.py` (or the variable before the launcher): `textlog` lines in
  `build/bof3x.log`, each string's address once; then search `.data` for a
  pointer to it ([`dialogue-localisation.md`](dialogue-localisation.md) §8).
- **After a crash:** `CRASH` lines in `build/bof3x.log`, then
  `python tools/crash_report.py` ([`crash-reporter.md`](crash-reporter.md)).
- **Call trace:** [`call-trace.md`](call-trace.md) §8.
- **English overlays:** `python tools/loc_build.py all --disc "CDImage/Breath of Fire III (USA).cue" --game bof3`
  (about a minute, 244 files), then `BOF3X_LANG=en` before the launcher or
  `attract_run.py` ([`dialogue-localisation.md`](dialogue-localisation.md) §1).
- DAT containers: `python tools/dat.py survey ../bof3ext/bof3/DAT` (expect
  742 clean); `list` / `extract --out analysis/dat/<name>` / `compare <DAT> <EMI>`
- Fixtures check: `python tools/verify_fixtures.py`
- Ghidra: `python tools/ghidra_pc.py import` (paths in `CLAUDE.md`)

## Traps already paid for

_One line each, with a pointer. Add when something costs more than an hour._

- BSim produced one high-confidence wrong match; no BSim name exceeds
  `hypothesis` without a PC-side read ([`bsim-evaluation.md`](bsim-evaluation.md)).
- Indexing PSX overlays into the BSim database degrades published ranks unless
  done on a copy ([`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md)).
- Figures repeated across docs are not evidence — count them (the 29,036 case,
  same doc).
- A Python `'''` string or a bash heredoc silently eats backslashes — two
  generated files came out wrong this way. Write source files with the editor
  tools, and keep backslashes out of `symbols.toml` evidence strings (TOML
  basic strings treat them as escapes).
- The game freezes whenever its window is not the foreground window, then
  replays the missed time unrendered ([`windowed-mode.md`](windowed-mode.md)).
- A scripted run takes the language and the filter from the owner's
  `build/bof3x.ini` unless the environment sets them. An English attract run
  against the Chinese reference looks exactly like a regression in
  `LoadDatFile` - an afternoon, 2026-09-21. `attract_run.py` now pins
  `BOF3X_LANG=original` and `BOF3X_FILTER=linear`
  ([`launcher-settings.md`](launcher-settings.md) section 4). Check the recording's
  second `#` line before believing a diff.
  Any unattended observation must foreground it first; `attract_run.py` does.
- `attract_run.py` re-takes the foreground for the whole run, so **anything the
  owner types goes into the game** and one keypress ends the attract sequence.
  An oracle or hash run needs the keyboard and mouse left alone entirely, not
  just the game window (lost a run to this 2026-09-19).
- A rebuild fails at link with "Permission denied" while a game launched
  through the launcher is running — it holds `bof3x.dll` open. Close the game.
- `pe_xref.py` answers "who touches this *data* address". It does not index
  calls: "(no references)" for a function means nothing. For callers use
  `callees` in `analysis/pc_funcs.json`, or scan `.text` for E8/E9 rel32. Cost
  one wrong "no caller" claim, caught the same day.
- **The `clut` region was off by one palette row in 8 of 9 dumps on
  2026-09-21 night, all-original included** (row 506 all original, 482 ours,
  sometimes identical): its reference `clutref_a` predates DIV-0022's clock.
  Until it is re-recorded, a one-row `clut` difference with arena and VRAM
  identical is that, not a regression - check it against an all-original
  dump of the same evening ([`sprite-draw-order.md`](sprite-draw-order.md)
  §17). Re-recording the pair is cheap: two all-original dumps.
- `mem_dump.py`'s `clut` region and `attract_diff.py` are both unreliable
  under `BOF3X_CALLTRACE_MODE=all` - the first depends on run speed, the second
  miscounts frames at half speed - and both "fail" with every function
  Capcom's. Dumps and the oracle at full speed, the frame hash under the
  tracer, never mixed ([`asset-loading-path.md`](asset-loading-path.md) §2).
- One disagreeing frame from `attract_diff.py`, at a state change, is a torn
  sample until a re-run says otherwise; the frame hash is the arbiter
  ([`attract-mode.md`](attract-mode.md) §6).
- `--original "*"` inside an unquoted `$(...)` or an `echo` is glob-expanded:
  `attract_run.py` dies on the file names, and the `cp` after it then saves the
  PREVIOUS run's `build/bof3x.callframes.tsv` as this run's. Check the
  `inject:` line of `build/bof3x.log` says what the run was meant to be.
- A differential fuzz of random bytes barely tests a comparison with a
  constant: `>= 0x80` against `> 0x80` was caught in 4 of 12,000 rounds until
  the input was seeded with `0x7F` and `0x80`, then in 341. Seed the
  boundaries, and let the negative control say whether you did
  ([`sprite-draw-order.md`](sprite-draw-order.md) §6).
- **A frame-hash difference means nothing without an original-vs-original
  pair.** 29 differing frames turned out to be 17 between two all-original
  runs: wall-clock draw code the exclusion list had never seen, entered only
  once the traced game got fast. `wallclock --static` with the renderer's
  ranges is the fix; expect to need it again as tracing gets cheaper
  ([`call-trace.md`](call-trace.md) §6).
- The optimiser can hide a wrong build from the fuzz: under strict aliasing a
  `short *` read was hoisted over a `long *` store to the same bytes.
  `-fno-strict-aliasing` is project-wide now; do not remove it
  ([`psx-library-layer.md`](psx-library-layer.md) §2).
- A negative control that is NOT refused is information about the claim:
  a "kept quirk" of `MapView_SetElevation` turned out to be unobservable, and
  had already been written down as behaviour
  ([`sprite-draw-order.md`](sprite-draw-order.md) §7). Run the control for
  every quirk a comment claims.
- A fault inside a start-up self-test - a division by zero in the fuzz's own
  arithmetic did it - does not crash: the game **hangs at start-up** with
  nothing in the log after the `cloned` lines.
- A fault during a start-up self-test and a hang look the same from outside;
  `Get-Process BOF3 | select CPU` tells them apart (near zero is a fault). A
  harness that restores a block too far can make the *original's copy* fault:
  the layers' block plus 2 KB reaches `Sprite_DrawListCount`.
- A quirk a comment claims may be unobservable, and only the negative control
  says so: three more on 2026-09-20 (`Gte_Rtpt` "is not three RTPS" - it is;
  [`psx-library-layer.md`](psx-library-layer.md) §4). And a control can pass
  because the fuzz is blind, not because the code is right: precision in
  `Gte_DepthRamp` showed only once values next to a 4096th of a wide ramp
  were seeded (§3 there).
- A bash heredoc holding Python triple quotes or C++ with apostrophes dies
  with "unexpected EOF" in this tool. Write the patch script with the editor
  tool and run it.
- `pe_xref.py` indexes memory operands only: `add eax, 0x803580` is invisible
  to it, and a raw byte scan for an address drowns in `push 0x80` and
  `[reg + 0x80]` encodings. For "who uses this address at all", walk the
  `imms` / `offs` / `globals_` lists in `analysis/pc_funcs.json`
  ([`dialogue-localisation.md`](dialogue-localisation.md) §6, DIV-0007).
- A clone of a function with a jump table runs its cases in the ORIGINAL
  body - the table holds absolute addresses. Relocate the entries and the
  `jmp [reg*4 + table]` operand in the copy, as `text_draw.cpp` does.
- A fuzz can generate the original's own trap: `Text_DrawString` executes
  `in al, dx` for a glyph above `0xA00`, and a seed nudged to `0xA01` hung the
  start-up test with nothing in the log. And a control that is refused by a
  HANG proves less than one refused by a count - write controls that fail by
  comparison.
- Pairing EMI sections to DAT chunks by order or by address mis-pairs 47
  files; use `dat_census.align` ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).
- A recipe's `shot NAME 10` backs out of a screen before the driver's grab
  (0.4 s later): four captures of a menu mid-slide. Keep the default 30 on
  anything that changes ([`input-script.md`](input-script.md) §3).
- The field's buttons are save data: menu, confirm and cancel are
  `0x903584` / `0x90358E` / `0x903590`, and save 5's differ from saves 0-3.
  Press `@0x903584`, not a shape; the menu's top-bar cursor is remembered -
  `seek` it ([`input-script.md`](input-script.md) §4).
- **A start-up self-test runs before `BOF3.exe`'s C runtime.** The launcher
  loads us into a suspended process, so anything that reaches the CRT's
  `_getptd` - `Rand` does - ends the process, and the launcher says only
  "could not load the dll" in a dialog. Give the fuzz a stand-in
  ([`sprite-draw-order.md`](sprite-draw-order.md) §16). In game the CRT is up.
- Window captures include Windows 11's rounded bottom corners, which blend
  what is behind the window: mask 8 x 8 at each before comparing pixels.
  Captures used to land wherever the game had run to 0.4 s after the shot
  line - 34 of 55 attract shots differed between identical runs. Shots now
  freeze the game until grabbed ([`input-script.md`](input-script.md) section 3): 55 of 55
  identical. A run that is not through `input_run.py` does not freeze.
- A raw byte scan of the `DAT`s for a script pattern drowns in audio (5,552
  "hits" for `F8 07 [80..FF]`, nearly all in kind-2 banks); walk the chunks and
  keep kind 0. And the movement scripts are not in the `DAT`s at all - they
  are in `BOF3.exe`'s `.data` ([`movement-script.md`](movement-script.md) §3).
- **A stand-in quieter than the real callee hides what the caller undoes**,
  2026-09-22: `Field_ObjectUpdate` puts back the script position a step moved,
  and a step stand-in that moved nothing made dropping that restore invisible.
  Give a stand-in the side effects the caller reads or reverts
  ([`movement-script.md`](movement-script.md) §1b).
- **A control that is not refused may be a change that changes nothing**:
  six of 2026-09-22's were (`& 0xDF` against `& 0xCF` with the bit already
  clear, a clamp to 4 of a value of 4). Before calling the fuzz blind, ask
  whether any input could tell the two apart; if one can, seed it.
- The backslash trap again, 2026-09-21: `\0` in a Python heredoc became two
  NUL bytes in `loc_build.py` ("source code cannot contain null bytes").
  Edit Python with the editor tool.
- **Agent worktrees start from `main`, not the current branch**
  (2026-09-22): all five of the parallel round's did. Commit first, and tell
  each agent to check `git merge-base --is-ancestor <commit> HEAD` and reset
  or rebase onto it before starting.
- **`symbols.toml` does not merge by text** (2026-09-22): git aligned two
  groups' appended blocks on a coincidentally equal entry, twice, and a
  keep-both resolver would have deleted repeated lines like `ret = "void"`.
  Resolve it by hand, then check with `tomllib` that no address or name is
  bound twice. Better: give each group its own fragment to append at merge.
- **Our own scaffolding has ceilings, and they fail like hangs** (2026-09-22,
  the second parallel round): the `BOF3X_ORIGINAL` / `BOF3X_SHADOW` name lists
  held 2,048 characters and a round's 119 names is 2,061; the tracer held 256
  owned functions and 326 are ours. Both end in a `Fatal` before the game
  window opens, which from outside is a process sitting at near-zero CPU with
  nothing in the log after the inject lines - 90 minutes lost before anyone
  looked at `build/bof3x.log`. Both limits are raised; **read the log when a
  run is slow, and check `Get-Process BOF3 | select CPU` first.**
- **A failed build leaves the previous `bof3x.dll` in place**, and the
  self-tests then pass on the old one (2026-09-22, twice during the merges:
  a CMake source list with a paren in the middle, and a symbol renamed out
  from under another group's source). Read the build's own output, not only
  the self-test's exit code.
- **An owned function missing from `entries_logic.txt` breaks the frame
  hash** without changing a count (2026-09-22, 23 functions, 5,236 frames):
  see "Pick up here" 1. Check the list before a batch. The tracer reads a
  size only for an *owned* entry (`calltrace.cpp`, the owned ranges); an
  unowned entry's size is ignored, so the over-long `pe_funcs.py` sizes
  still in the list only matter once their function is taken over.
- **The compiler's tail call at a task's top frame shows in the frame
  hash** (2026-09-22, `ab24`): our `Boot_Task` ended `jmp [Task_Exit]` where
  Capcom's has `call`, so `Task_Exit` saw the fresh task stack's 0 as its
  caller - one differing frame, frame 1, every count equal. Found in two
  minutes with `BOF3X_CALLTRACE_DETAIL=1-1` on both sides and a `diff`;
  fixed with `__attribute__((disable_tail_calls))`. Deeper tail calls are
  harmless (their caller is our dll either way); a task body's last call
  to a traced function is not.
- **`symbols.toml` spliced again at merge** (2026-09-22, third round): git
  attached one group's edit of an entry to the end of another group's
  block. Rebuilding the file by entry worked every time: the current file
  less the entries the incoming group moved, plus its block verbatim,
  then a three-way check keyed on `pc` against the merge base (the
  groups were told to put every change in one block at the end).
- **A black screen cover makes every capture black and every A/B "identical"**
  (2026-09-23, `ab25`): the owner covers the screen at night; captures came
  back solid black on both sides from 00:04 on, and `shots_compare.py`
  passed them. Check that captures are not black before believing a pair.
  The memory checks (oracle, dump, hash) are unaffected.
- **A divergence that stretches time must not stretch what the game sees**
  (2026-09-23, DIV-0028): fades spread over frames left `Music_Track` naming
  a fading track, so a `Music_Play` of it was ignored and the fade then
  stopped the music - silence at attract frame 3439 that the frame hash
  caught in one frame. Keep the logical state on the original's schedule
  and let only the audible part linger; the traced hash with the DIV on is
  the check.
- **Check a patch's expected bytes against the image, not the disassembly
  in your head** (2026-09-23): `push 0xC` at `0x461A61` is followed by
  `xor bl, bl` (`32`), not a `push ebp` (`55`); `PatchBytes` would have
  refused at start-up. Dump the three bytes first.
- **clang 22.1.8 miscompiles a pointer's high bits merged with a byte read
  through it** (2026-09-23, group V1, reproduced independently):
  `(a & 0xFFFFFF00) | ((unsigned char*)a)[8]` compiles at -O2 to
  `movzbl 8(%eax), %eax` - the pointer gone - with or without
  `-fno-strict-aliasing`. The originals do this often (`mov al, [eax+8];
  push eax`: a direction in a pointer's low byte). Write it as inline asm, as
  `event_ops.cpp`'s `DirectionInPointer` does; no other instance was in the
  tree ([`event-ops.md`](event-ops.md) §8).
- **`BOF3X_ORIGINAL=*` switched off the input recipe itself** (2026-09-23):
  the recipe's latch hook went through `RetargetCall`, which honours the
  list, and an all-original recipe run sat in the attract sequence with no
  input. Instruments now pass `instrument = true`. And `*,-NAME` excludes one
  name - an English A/B's original side keeps the language machinery ours
  (`analysis/validate_shop.sh` has the list; `Text_DrawImmediate` is in it).
- **An A/B's two sides need the same divergences** (2026-09-23): the first
  backdrop A/B switched off `Menu_DrawBackdrop` alone on Capcom's side and
  every pixel DIV on ours - all 12 captures "differed", by sub-pixel sampling.
- **The frame hash counts the CRT's `sscanf` by characters** (2026-09-23,
  `ab26`): `Mp3_MemoryIo` scans `"%lx@%lx"` - the music buffer's address in
  hex - so a buffer one hex digit shorter in one run is four fewer calls at
  that frame (5524, reproduced, `ab26b`), with nothing else different. An
  all-original pair agrees; ours sits the heap differently. A difference
  whose extra calls are `0x5BCF64`'s (`0x5BD989` / `0x5BD9C0` pairs) under
  `0x5B9AA6` is this; exclude them from the list at the next re-record.
- **The game's task stacks lie inside the main thread's stack, and the
  present runs on one** (2026-09-23, the backend): a check of esp against
  the TEB's bounds passes on a 16 KB task stack, and DXGI's `Present` ran
  off the end of it into the scheduler's records - the game then returned
  into garbage (`ret` to `0x1FD`, `0`, `0xFE`) a few calls later, with
  nothing of ours on the stack. Bisected by replacing the present with a
  20 ms `Sleep` (fine) and with a non-waiting `Present` (crashes). Anything
  heavier than a few hundred bytes of stack runs on a fiber of its own
  (`render_d3d11.cpp`, `RunOnFiber`). Three hours.
- `DrawState` is a Win32 macro and `pass` an HLSL keyword (2026-09-23).
- **The crash reporter's stack scan misses the faulting frame**: the dump's
  thread context is the handler's, not the fault's; the exception stream's
  context has the real esp (the scratch script `dumpstack.py` did it; worth
  folding into `tools/crash_report.py`).
- **`attract_run.py --no-front` on an all-original side makes 0 logic
  frames** (2026-09-23, `wm1`): DIV-0033 lives in our WndProc, so under
  `--original "*"` Capcom's WndProc clears `App_Active` the moment the
  window is not in front and the loop pumps messages for the whole run -
  `done: 0 logic frames in 16 s`. Reference sides of the frame hash keep
  the foreground hold; only all-ours runs can go unfocused.
- **Group names collide across parallel groups** (2026-09-23): W and Y both
  named an `Item_Price` and a `Menu_DrawIcon`, Z and V1 a
  `Field_ObjectAhead`; the merge's tomllib check caught all three. Rename the
  later group's in its own files only.
- **The tracer's single step is visible to `pushfd`** (2026-09-23, `wm1`):
  a traced entry whose first instruction saves the flags saved TF = 1, a
  `popfd` later put it back, and the stray step went unhandled to the
  CRT's `__except` in `WinMainCRTStartup` (`0x5BA154`), which ends the
  process with `_exit(exception code)` - no dialog, no CRASH line (the
  reporter skips signals). It looked like a clean exit "inside
  `Display_Setup`" for an afternoon. `calltrace.cpp` handles `pushfd` now
  and fails loudly on any other stray step. **A silent exit whose stack
  holds `0x5BA15F` is an unhandled exception**: `BOF3X_EXITTRACE=1` logs
  its code and address.
- **`renderer=` in `bof3x.ini` picks Capcom's software renderer at 0**
  (2026-09-23): harmless to ours (DIV-0031 ignores it) but every
  all-original run draws through a different set-up path. Reference runs
  pin it with their own scratch `bof3x.ini` (`renderer=1`, as `wm1b`).
- **Our code calls ours directly**: a C++ call to a taken-over name binds
  our function, not Capcom's address, so `BOF3X_ORIGINAL=NAME` switches only
  what Capcom's code calls. Where a switch matters from our side (WinMain's
  set-up, player and WndProc), call through `bof3::orig::NAME` - the
  address, detoured or not (2026-09-23 review).
## In flight / uncommitted

Branch `phase-3/UI-overhaul` (from `main` at PR 10). **Everything committed
and pushed as of the night of 2026-09-23; the working tree is clean.** The
day's commits in order: the backend (`340d373`, merged with PR 11's PSP
findings in `5c168ed`), the window, loop and FMVs with integer scaling and
the CRT look (`9c2bb22`), then the night: the widescreen survey build
(`9f84be5`, DIV-0041), the resizable window with snap or fit (`6f50fc6`,
DIV-0042), the SatPixie look (`0fb9188`, DIV-0043, `docs/THIRD_PARTY.md`),
its dialog label, I19, and the slide-out bounds (`09ff603`). No PR yet for
the branch. `.claude/worktrees/vibrant-wilbur-f9676a` was the PSP session's.
The round-four PR is merged (PR 10).

Branch `phase-3/intro-takeover`, **committed and pushed 2026-09-21, no PR**:
the attract catalogue and the PSX pairing ([`attract-remaining.md`](attract-remaining.md),
`tools/pe_hidden.py`, `tools/attract_catalog.py`, `tools/psx_pair.py`, the
WndProc correction), then scripted input (`src/hook/input_script.cpp`,
`tools/input_run.py`, `tools/recipes/`, [`input-script.md`](input-script.md))
and DIV-0018..0020 (`src/game/menu_verbs.cpp`, `src/game/char_names.cpp`,
kinds 8-11 in `dat_load.cpp`, `tools/dat.py` and `tools/loc_build.py`), with
`BOF3X_TEXTLOG` in `src/game/text_draw.cpp`. Then the harness fix, the matrix
takeover (DIV-0021), `Sprite_AddDrawRecords`, `Prim_SetTexture`, the sprite
draw and `DrawLayer_Open` - one commit each, after the batch check passed -
and these docs. The validation script is `analysis/validate_ab17.sh` (local).
Then the map cells (`src/game/map_cells.cpp`, DIV-0023, the two recipes
`field_view.txt` and `attract_cycle.txt`, batch `analysis/validate_ab18.sh`)
and frozen shots. **All of it pushed 2026-09-21, no PR.** Then, committed
locally that night and not pushed: `Sprite_ObjectByHandle` and
`Sprite_InheritDrawKey` in `src/game/sprite_find.cpp`, D6,
[`movement-script.md`](movement-script.md) and `tools/movement_scan.py`
(batches `analysis/validate_ab19.sh`, `validate_ab19b.sh`). That was
merged as PR 8.

Branch `phase-3/further-mining-attract`, **2026-09-22, pushed, no PR**: the movement script and what surrounds it (batch `ab20`), then
`BOF3X_SELFTEST_ONLY`, the six boundary callees, and the five parallel
groups merged one by one - `field_frame.cpp`, `kind2_object.cpp` +
`area_slope.cpp`, `object_kinds.cpp`, `map_layers.cpp` (+ `_fuzz.cpp`,
`_callees.h`), `mode_tasks.cpp`, with a doc each, DIV-0023's extension,
DIV-0024, and `tools/recipes/camera_rotate.txt`. All through the batch
(`ab21` + `ab21b`). **Then the second parallel round the same day**: two
boundary callees (`Flags_Set`, `Kind2_Place`), then seven groups merged one
by one - `title_states.cpp`, `field_blocked.cpp`, `frame_callees.cpp`,
`field_modes.cpp` (+ `_fuzz.cpp`, `_callees.h`), `map_scroll.cpp`
(+ `_fuzz.cpp`), `field_event.cpp` (+ `_fuzz.cpp`, `_callees.h`),
`event_script.cpp` (+ `_fuzz.cpp`) - a doc each, `tools/event_scan.py`, the
two raised ceilings (`detour.cpp`, `calltrace.cpp`) and `known-defects.md`
D8..D11. All through the batch (`ab22` + `ab22b`). No new DIV entry.
**Then the third round that evening**:
H and J merged (`11dbc4a`), `Math_Ratan2` registered (`c966c21`), then M, L
and K merged one by one - `sprite_pose.cpp`, `move_cmds.cpp`,
`mode_flow.cpp` (each + `_fuzz.cpp`), a doc each, D14..D16, and the
`Boot_Task` call fix (`f199624`). All through the batch (`ab24`). No new DIV
entry. **Then the fourth round that night, and the owner's display fixes**:
the round's queue (`afe0ec2`), O, N, P, Q and R merged one by one -
`area_entry.cpp`, `glyph_draw.cpp` + `d3d_fuzz.cpp` + `yes_no_layout.cpp`,
`item_use.cpp`, `sound.cpp`, `d3d_draw.cpp` + `d3d_list.cpp` (each with its
`_fuzz.cpp`), a doc each - DIV-0025..0029, D17..D28, the DIV-0028
correction (`9d62125`), `tools/recipes/config_controller.txt` and
`load_list.txt`. Oracle, memory dump and frame hash through `ab25`; the
capture A/Bs owed (`analysis/validate_ab25b.sh`, local). **Pushed
2026-09-23, PR opened for the owner to merge.**
All four rounds' agent branches and worktrees are merged and removed, as are the
two older ones (`claude/silly-bhabha-776856`, and the detached
`epic-chandrasekhar-cc0704`); `.claude/worktrees/` is empty.

_Branches, open PRs, half-finished experiments, files in `analysis/` worth
keeping. "Nothing" is a valid entry._

`localization/script-font-upscale` (DIV-0005..0017) is merged into `main`
(PR 6); `phase-3/intro-takeover` was cut after it.

Local only, gitignored, worth keeping:

- `bof3/BOF3.CFG` (windowed mode) and `build/bof3x.ini` (launcher settings);
  the owner's PC saves `bof3/BISLPS00`..`05` and `0F.DAT`. Slots 0 and 1 are
  from before the menu exists, 2 is at camp, **5 is adult Ryu, Lv 38 - the
  one the menu recipes load** (converted from a US save, so its shape
  buttons are a US layout: menu square, confirm cross, cancel triangle).
- `analysis/shots/` - every recipe capture, including the A/Bs and sheets
  sent to the owner.
- `analysis/attract/orig_a.tsv` - the all-original reference for
  `attract_diff.py`; `ours_d_fileopen.log`, behind
  [`attract-mode.md`](attract-mode.md) §7.
- `analysis/memdump/clutref_a_*` and `clutref_b_*` - the all-original reference
  pair for `mem_dump.py --compare`, all three regions (`drain_a` / `drain_b`
  are the same without `clut`).
- `analysis/calltrace/all_b/` - the full-list all-original run of a whole
  attract cycle, and `all_ab.callcounts.tsv`, `all_a`'s and its counts
  concatenated, which the exclusion list was rebuilt from.
- `analysis/calltrace/ab24_orig/` - **the all-original frame-hash reference
  since the third parallel round, 2026-09-22** (`ab24_origb`, and
  `ab24_oursb` the all-ours match), under `entries_logic.txt` with the
  round's functions added (`entries_logic_0922c.txt` is the list before it);
  10,062 frames. `ab24_ours` is the first all-ours run, one frame off (the
  `Boot_Task` tail call); `ab24_f1_orig` / `ab24_f1_ours` the frame-1 call
  detail that found it. `analysis/attract/ab24_*` the batch's runs and logs
  (`ab24_msgbox_watch.log` the watch of `0x7DEE40` and window record 0),
  `analysis/shots/ab24_*` its six A/B capture pairs.
- `analysis/calltrace/ab22b_orig/` - the reference before it, **since the
  second parallel round, 2026-09-22** (`ab22b_origb`, `ab22b_ours`),
  under `entries_logic.txt` with the round's 119 functions added
  (`entries_logic_0922b.txt` is the list before it); 10,060 frames.
  `analysis/attract/ab22_*` the batch's runs, captures and logs,
  `analysis/shots/ab22_*` its five A/B capture pairs. `ab22_orig` is the
  aborted first hash run (the 256-owned-function Fatal), kept as the artefact.
- `analysis/calltrace/ab21b_orig/` - the reference before it, **from the first
  parallel round, 2026-09-22** (`ab21b_origb`, `ab21b_ours`), under
  `entries_logic.txt` with the 23 added ranges. `ab21_*` the batch's first
  hash trio, under the old list (the artefact). `analysis/attract/ab21_*`,
  `analysis/shots/ab21_*` the batch's runs and captures;
  `analysis/shots/camera_ours`, `camera_orig` the first camera A/B.
- `analysis/calltrace/ab20_orig/` - the reference before it
  since the movement script, 2026-09-22** (`ab20_origb`, `ab20_ours`).
  `analysis/attract/ab20_*` the batch's runs and log, `analysis/shots/ab20_*`
  the captures, `analysis/memwatch/ab20_runtime_scripts.tsv` the watch of
  `0x6758E0`.
- `analysis/calltrace/ab19_orig/` - the reference before it, **since the
  attachment handle, 2026-09-21 night** (`ab19_origb`, `ab19_ours`),
  10,062 frames at 11 minutes. `analysis/attract/ab19_*` the batch's runs,
  `analysis/probe/ab19_probe_*` the live shadow and attach probe in three
  scenes, `analysis/shots/ab19_*` the captures (`ab19_attract_nokey` the
  coverage run).
- `analysis/calltrace/ab18_orig/` - the reference before it, since the map
  cells, 2026-09-21 (`ab18_origb` its noise-floor twin,
  `ab18_ours`), recorded under the corrected `entries_logic.txt`
  (`entries_logic_0921.txt` is the list before it); 6,312 frames, the traced
  pace at 7 minutes. `analysis/attract/ab18_*` the oracle, memory-dump, live
  shadow and run logs of the batch; `analysis/shots/cells_*` its captures.
- `analysis/calltrace/ab17_orig/` - the reference before it (`ab17_origb`,
  `ab17_ours`), under the old list.
- `analysis/calltrace/ab15_orig/` - the reference before it
  (and `ab15_origb`, its noise-floor twin; `ab15_ours`, `ab15_oursb`),
  recorded 2026-09-20 with a hundred and fifteen owned under
  `entries_logic.txt` (`entries_logic_0919.txt` is the old list). `ab14_*`
  and `ab3_*` are stale. Owned
  functions are left unarmed, so it survives a takeover only when the function
  was not in `entries_logic.txt` to begin with (`Gfx_TexCacheFind` is
  render-timed and was not). Taking over a *logic* function changes every
  frame's hash; re-record then, about five minutes.
- `bof3/DAT/en.*.DAT` - the English overlays, 244 files; rebuilt by
  `loc_build.py` in under a minute. `analysis/font/` - sheets, the exported
  cells, and `shots/`, the attract screenshots behind DIV-0005/0006.
- `CDImage/` - the owner's PSX discs (USA, Japan, Germany, France) and two PSP disc images (`psp-jp`, `psp-eu` in
  `fixtures.toml`). Never commit; extract to scratch, not into the tree.
- `analysis/memwatch/menu_state.tsv` - the owner's menu walk, state bytes
  against time; `analysis/d1/` - screenshots original against DIV-0010
  (`fix1` is the first, wrong, attempt with its seams).
- `analysis/experiments/experiment_mulmatrix.cpp` and `analysis/attract/pad_*`,
  `analysis/calltrace/padh_*` / `padd_*` - the matrix-padding experiment.
- `analysis/attract/ab12_shadow.log` - the run the x87 control word was
  measured in: 11 million calls, all `0x027F`.
- `analysis/memdump/slowref_*` - an all-original dump taken under the tracer,
  the evidence that the `clut` region depends on run speed.

## Waiting on someone else

- TheRealBiggs — not yet contacted ([`STATUS.md`](STATUS.md) obligations).
