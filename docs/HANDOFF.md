# Handoff — next session

**Status:** IN PROGRESS (2026-09-22)

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
- replace what the attract sequence reaches - stands at **a hundred and thirty-four
functions ours**, every one through the full live check (the last two
on 2026-09-21, `ab19_*`); and **stage 2, the text swap, went from a plan to a playable
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
port's rewritten sound ops and three PSX calls it dropped (§1a, §1c).

## Pick up here

The single next action, concrete enough to start without asking anyone.

00000. **The game's pace is done short term: DIV-0022, confirmed in game by
   the owner 2026-09-22.** The complete fix, the deadline in a double, is
   [`IDEAS.md`](IDEAS.md) I16, not scheduled. **Pace figures before
   2026-09-21 are the 31.25 band.** The untraced oracle needs only 7 minutes
   at 30 a second, but traced hash runs are far slower (`ab18` at 7 minutes
   reached 6,312 frames against `ab17`'s 10,062 at 11): keep hash runs at 11
   minutes or more.

0000. **Keep taking over what the attract sequence reaches (the owner's
   order, 2026-09-21), batching the live check.** What is left, regenerated
   2026-09-22 after the movement script (`python tools/attract_catalog.py
   analysis/calltrace/hidden_b/bof3x.callcounts.tsv --also
   analysis/calltrace/all_a/bof3x.callcounts.tsv,analysis/calltrace/all_b/bof3x.callcounts.tsv`):
   **305 functions, ~67 KB** outside audio, the MP3 decoder and the CRT -
   renderer 45 / 17.4 KB, field objects 85 / 15.1 KB, event script 52 / 9.0 KB,
   map and draw layers 18 / 7.0 KB, top-level modes 31 / 6.1 KB, text and
   windows 27 / 5.6 KB, the rest under 4 KB each. (`0x576CD0`, `0x577800`
   and `0x577B80` in the list are pieces of functions now ours - a catalogue
   artefact, not work.) **The next highest-value targets, in order:**

   1. **The field-object frame loop** (agreed with the owner 2026-09-22): the
      per-frame entries `0x517200` / `0x517240` reach `0x517490` (432 bytes,
      walks the objects into `Field_ObjectUpdate` / `Field_ObjectFollow`),
      `0x517350` (139), `0x5173E0` (96, calls `Sprite_UpdateScreen`),
      `0x573080` / `0x573090` (the kind-2 object's script runner, 544 bytes,
      the interpreter's second caller), `0x57B780` / `0x57B7B0` / `0x57B830`
      (554 bytes, 20,793 calls) and `0x592F00` / `0x592F20` - 12,165 calls a
      cycle each, once a frame. With them, from the field's frame down to the
      draw list is ours. Also in the group: `0x5722D0` (672 bytes, the
      elevation's sibling with a store, called by `MoveCmd_Move`) and
      `MoveCmd_Move` `0x578C10` itself - **not reached by the attract cycle**;
      the batch's new-game capture is its only test.
   2. **The map and draw layers**, 7 KB: `0x56E6C0` (730 bytes, called from
      the field's frame) into `0x56EC00` (2,465), `0x56F9B0` (283 bytes,
      28,357 calls), and the pointer-reached handlers `0x571500` (543, 33,550)
      and `0x571B40` (160, 29,701). Everything here feeds the draw, so the
      frozen-shot A/B checks it pixel for pixel; read `0x56E6C0` first for the
      handler table.
   3. **The top-level mode tasks**: `0x4621C0`'s state machine - `0x462420`,
      `0x462600`, `0x462740` (16,127 calls each) and `0x462560` (36,109) - and
      the field's mode-2 handler `0x4959F0` (2,139 bytes; it turns
      `Field_Request` 1 into the menu). Small, hot, and the oracle's own
      territory (mode and area changes).
   4. **The event script**, 52 functions / 9 KB (`attract-remaining.md` §4.6):
      the area script the attract's text boxes run - `0x52DA70`, the
      dispatcher `0x56D690` and `0x56B5D0`. The other interpreter of the game,
      and the one a living game most wants to extend; start from its op
      table and its PSX twin, as the movement script did.
   5. **`MsgBox_Step` `0x497840`** (752 bytes) - after measuring which of its
      23 control codes the attract's eight messages use ("Then" item 6),
      since the oracle sees only those.
   6. Not per-function work yet: **the renderer** (45 / 17.4 KB) waits on a
      way to check a surface (item 3 below, [`IDEAS.md`](IDEAS.md) I14);
      `0x437CC0` is a bare `ret` with 4.6 million calls - nothing to gain.

   The pattern that worked on 2026-09-22, for each: read it against its PSX
   twin (the sibling's `GAME_EMI0` or `SLPS_009.90` Ghidra output), type
   every callee in `symbols.toml`, clone the original with every call
   re-aimed at a recording stand-in (`StubFor` in `move_groups.cpp` /
   `field_objects.cpp`), fuzz from random state with each branch's
   boundaries seeded, then plant bugs until each behaviour-changing one is
   refused (Traps: a quiet stand-in, a change that changes nothing); list a
   clone's calls and jump tables with capstone rather than by eye. The batch
   is `analysis/validate_ab20.sh` (local, with `analysis/shots_compare.py`):
   copy it, put the new functions in `THREE`, and note that the frame hash's
   reference is now `ab20_orig`.

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
1. **The frame hash reference is `analysis/calltrace/ab20_orig`** (twin
   `ab20_origb`), recorded all-original 2026-09-22 at 11 minutes: all 10,062
   frames identical original-vs-original and original-vs-ours with all 158
   injects on. It differs from `ab19_orig` by exactly the calls of the
   functions taken over (eleven of the twenty-one are on the traced list;
   no other total changed).
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
3. **The Direct3D end of the image path** is where it was: the lock wrapper
   `0x5A3CC0`, the entry builders `0x5A0080` / `0x5A0510` (ten COM calls each),
   the glyph-texture lookup `0x5A2BC0` (128 entries of 0x14 bytes at
   `0x7C9F50`, keyed by glyph and CLUT, same generation trick), and the 3.7 KB
   set-up `0x5A5160` ([`asset-loading-path.md`](asset-loading-path.md) §2).
   **Decide first how such a function gets checked** - its product is a
   surface, not memory; reading a locked surface back is the obvious candidate
   ([`IDEAS.md`](IDEAS.md) I14).
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
  Without it the game mode-sets to exclusive fullscreen for the FMVs; from an agent
  session, end it with `taskkill //F //IM BOF3.exe`.
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
- **Shadow check:** `BOF3X_SHADOW=Gfx_InvalidateTextures`, `=Gfx_TexCacheFind`, `=gfx_clut`, `=gfx_flush`, `=gfx_unpack`, `=gfx_vram_ops`, `=sprite_order`, `=draw_pool`, `=prim`, `=map_view`, `=sprite_anim`, `=sprite_find`, `=field_input`, `=sprite_clut`, `=draw_layers`, `=psx_gpu`, `=psx_gte`, `=psx_gte_float` (which also compares every live call of the two precision-dependent functions and counts the x87 control word), `=psx_gte_transform`, `=draw_emit`, `=draw_pass`, `=msg_pool`, `=text_draw`, `=text_immediate`, `=map_cells` (which also shadows every in-game call of the two handlers) (comma-separated lists work) or `=*` before the launcher
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
  an original-vs-original run beside it**, the noise floor (one
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

## In flight / uncommitted

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

Branch `phase-3/further-mining-attract`, **2026-09-22, uncommitted**: the
movement script and what surrounds it - `src/game/move_script.cpp`,
`move_groups.cpp`, `field_objects.cpp`, `sprite_screen.cpp` and
`move_script_bytes.h`, their `symbols.toml` entries, `tools/movement_scan.py
--decode`, D7, [`movement-script.md`](movement-script.md) and the doc
updates, DIV-0022 marked confirmed. All through the batch check
(`analysis/validate_ab20.sh`, log `analysis/attract/ab20_batch.log`).

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
- `analysis/calltrace/ab20_orig/` - **the all-original frame-hash reference
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
