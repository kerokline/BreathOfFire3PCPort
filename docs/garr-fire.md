# Garr over the camp's fire, again (wide-ring round, group GF, 2026-10-21)

**Status:** BUILT AND MEASURED (2026-10-21), on branch `wide-ring/gf`. The
ledger text is drafted in section 6, for the orchestrator to fold.

The owner's report (chat, 2026-10-21): **"Garr over fire" shows up on our
plays but not on the original code.** Known-defects D301 - the camp's fire
drawn under Garr because DIV-0071 moved him past it - was fixed by
DIV-0071's 2026-10-14 amendment ([`campfire-and-crystal.md`](campfire-and-crystal.md)),
and the owner approved it then.

**Verdict: not a regression - a sibling case the amendment does not cover.**
The amendment's test stops the move when what the fire's record drew in the
last pass reaches Garr's **body box**, 28 px wide round his screen point.
In the vaccination scene he stands with the fire inside that box, and the
fix holds there today (section 2). In two other camp scenes he is seated
beside the fire, his point 20 to 40 px from its quads, and his arm and
weapon reach over it. The body box misses the fire, so DIV-0071 still moves
him three layers on and draws his arm and weapon over the flames. Capcom's
order draws the flames over them. The scenes are `AREA053`'s camp in
chapter 6 (`cs_53.txt`) and `AREA192`'s camp (`cs_192.txt`). Both are the
same fire record as D301's. Both are wrong in `main`'s build (`31249d0c`,
which the owner plays), and identically so in the round's build. FY's
restructure of `LayerFor` changed nothing here.

**The fix:** the record test also takes what the sprite itself drew in the
last pass (its cell sprite's box) as well as its body box. Both scenes are
now pixel-identical to Capcom's draw pass. Every other route checked is
unchanged, the faerie (D300) included.

All runs: narrow (`--wide 0`), `BOF3X_OPT=none`, `BOF3X_FILTER=point`,
en-US unless stated, ini-less launcher copies (`bof3x-launcher-dev.exe`),
GF's own game copies. The harness root is `C:\Users\kerok\bof3x-runs\r_ring\GF\`:
`cap.py` and `batch.py` run captures, `cmp.py` compares shot directories
(PIL `ImageChops.difference`), `sheet.py` builds crops, `chain.py` and
`check.py` run the state hash. Shots are named by recipe frame
(`tools/recipe_shots.py`, or the `cs_` recipes' own). Captures are not
committed. The builds compared:

| Label | Build |
|---|---|
| `base` | `main` `31249d0c`: the owner's play build's code (`basewt\` worktree) |
| `new` | `wide-ring/round` `4b07b9a8`: this round, FY's faerie included |
| `fix` | this branch (`bin_fix1\`) |
| `off` | `new` with `BOF3X_LAYERING=0`: Capcom's order |
| `origdraw` | `new` hosted (`--host bof3`), `BOF3X_ORIGINAL=Sprite_DrawPass,DrawLayer_Open,DrawLayer_Close,Gfx_CommitPrim,Sprite_Draw`: Capcom's code for the draw pass (`inject: 10083 ours, 5 left original`) |

`BOF3X_ORIGINAL='*,-Game_Clock,-Game_WndProc,-Display_Setup'` (CF's set) was
tried on `cs_53` and stalled before recipe frame 0 for ten minutes, with no
frame logged. It was killed by its PID. The five-function set above is the
draw pass, which is all that decides this order.

## 1. Reproduced: two camps where Garr's arm is over the fire

`cs_53.txt` (the chapter-jump save `cs53`, `AREA053`, chapter 6, run 3 from
step `0xB`) and `cs_192.txt` (`cs192`, `AREA192`) are CS's recipes, run again
here:

| Recipe | Shots | `base` vs `new` | `new` vs `off` | `fix` vs `off` | `origdraw` vs `off` | `origdraw` vs `fix` |
|---|--:|---|---|--:|--:|--:|
| `cs_53` | 41 | 41 identical | **21 differ**, f0600..f1000, all in (276..363, 5..260) of 640 x 480: the fire's box | 41 / 41 identical | 41 / 41 identical | 41 / 41 identical |
| `cs_192` | 4 | 4 identical | **3 differ**, f0480..f0600, 985 px in (313..361, 245..280) | 4 / 4 identical | 4 / 4 identical | 4 / 4 identical |

`cs_153`, `cs_154` (4 shots each) and `cs_194b` (4): `new` against `off` is
identical. No sprite stands at their fires in those shots.

**What the log says.** `BOF3X_LAYERING_LOG=1` with a `BOF3X_DRAWORDER`
window round the fire:

`cs_53`, recipe frames 698..700, `AREA053`:

    DIV-0071    frame 698: sprite 007DEE80 at (119.618, 122.663), cell 18,26, feet 13: layer 24 drawn in layer 27; stopped by nothing
    draworder   #217 slot 6 code 2E box (138.5, 71.8)-(181.6, 145.2) ... cell layer 26 row 48 col 14 handler 10 b1b0 151A
    draworder   #218 slot 6 code 2E box (137.7, 71.8)-(180.8, 145.2) ... cell layer 26 row 48 col 14 handler 10 b1b0 151A
    draworder   #230 slot 6 code 84 box (81.6, 81.7)-(185.6, 141.7) ... sprite layer 27 key 1B8D object 007DEE80

`cs_192`, recipe frame 479, `AREA192`:

    DIV-0071    frame 479: sprite 007DEFC8 at (222.284, 137.301), cell 20,23, feet 13: layer 26 drawn in layer 29; stopped by nothing
    draworder   #203 slot 6 code 2E box (137.6, 85.2)-(182.5, 161.7) ... cell layer 28 row 26 col 14 handler 10 b1b0 151A
    draworder   #204 slot 6 code 2E box (136.7, 85.2)-(181.7, 161.7) ... cell layer 28 row 26 col 14 handler 10 b1b0 151A

Both fires are D301's record: handler `0x10` (`MapCell_DrawFrames`), cell
bytes `0x151A`, anchored at map cell (21, 26), two code-`0x2E` quads. Both
are anchored three cells from Garr: dx 3 in `AREA053` (Garr at (18, 26)) and
dz 3 in `AREA192` (Garr at (20, 23)). So the proximity test (`RecordsClear`,
two cells) misses them, as it did in D301.

**Why the 10-14 amendment misses them too.** Its `RunsClear` tests the run's
kept quads against the body box, `BoxRound(sprite, 14, 44, 8)`:

- **`AREA053`:** x 105.6..133.6, y 78.7..130.7. The fire's quads start at
  x 137.7, 4 px to the right. Garr's cell sprite spans x 81.6..185.6 and
  covers the fire's whole width: he sits to its left with his arm and weapon
  held out over it.
- **`AREA192`:** x 208.3..236.3. The fire ends at x 182.5, 26 px to the
  left. Garr sits to its right, mirrored, his weapon over it, his sprite
  spanning x 156.3..260.3.

What should have stopped the move is the fire's run at layer 26 (`AREA053`)
or 28 (`AREA192`). Its quads reach what Garr draws, though not the body box.

**And D301's own scene is still fixed.** `vaccinationAndFireScene.txt`, every
60 recipe frames from 1,260 to 2,940 (29 shots): `new` against `off` differs
in every shot, but only in (222..248, 230..248), 226 px each. That is
another sprite's feet (DIV-0071's purpose), not the fire's box (286..363,
198..275). In that scene Garr's point is beside the fire, (169.9, 121.6),
and the body box reaches it.

## 2. Which build, which case

- **Not FY's change, and not a regression of D301.** `base` (`main`, before
  this round) and `new` are pixel-identical on `cs_53` (41 / 41) and
  `cs_192` (4 / 4). Both draw Garr over the fire. The vaccination scene is
  fixed in both.
- **A case the 10-14 amendment does not cover:** the same fire, with Garr
  seated beside it rather than standing at it. CF verified the amendment on
  `vaccinationAndFireScene` and `campingFishing`. At the field camp nobody
  stands near the fire (`campingFishing.txt` every 60 frames from 420 to
  1,200 plus its own nine shots: `new` against `off`, 22 / 22 identical).
  `cs_53` and `cs_192` were first reached by CS's chapter-jump saves today.
  The owner most likely saw one of these two scenes in play: chapter 6's
  camp, or area 192's.

## 3. The fix: the record test takes what the sprite drew (DIV-0071, third amendment)

`src/game/layering.cpp`, `src/game/layering.h`, one call in
`src/game/draw_pass.cpp`, the quicksave record's version in
`src/render/gfx_snapshot.cpp`:

1. **What each sprite drew is kept** (`layering.cpp` `KeepDrawn`, `DrawnBox`,
   from line 259). `Sprite_DrawPass` calls `layering::BeforeSprite()` before
   each sprite's draw (`draw_pass.cpp`, mode 1 only). `AfterSprite`, already
   called after it, finds the one 0x20-byte cell sprite (code `0x84`)
   `Sprite_Draw` committed: `Gfx_PacketNext` is exactly 0x20 past the mark,
   and the byte at +7 is `0x84`. It keeps that primitive's screen box, laid
   out as `D3d_BuildCellTexture` lays out its cells: the cells' union from
   `SpriteCell_Table`, scaled, mirrored with bit `0x400` of the mode word.
   This is the same arithmetic as `draw_order.cpp`'s box for the code. The
   box is keyed by the object's address, up to 64 a pass, in two generations
   stamped by pass, as the runs are. A box with a coordinate past +/-4,096
   is not kept.
2. **The record test uses it** (`LayerFor`, `reach_box`, line 468; the
   `RunsClear` call, line 514). The box tested against each run's kept quads
   is now the body box grown to the sprite's last-pass box. Nothing else in
   the rule changes. The body box still drives the lists, the table items,
   the other sprites and the frame nodes. `RecordsClear` is unchanged. A
   sprite with no box from the last pass (new, culled, or its packet
   skipped) is tested with the body box alone, as before.
3. **Log.** The `RunsClear` line now reads "drew a quad over the sprite last
   pass (n kept; the sprite's box (...))".
4. **Quicksave.** The boxes the current pass drew are what the next pass's
   `Defer` reads, so the graphics participant carries them after the runs
   (`layering::SnapshotSave` / `SnapshotRestore`). The record's version
   goes from 2 to 3.

**Switches.** `BOF3X_LAYERING=0` is Capcom's order, as before.
`BOF3X_LAYERING_SPRITEBOX=0` drops only this amendment and tests the body box
alone. It is the negative control, and the build then draws as `new` does.
`BOF3X_LAYERING_RECORDS=0` still drops the whole record test, this amendment
with it.

**It can only stop moves.** The box tested only grows. Every pixel it
changes is therefore Capcom's order at that spot. The rule stops one layer
before the fire's, and Garr is still moved where the floor allows: `AREA053`
layer 24 to 25, `AREA192` layer 26 to 27.

    DIV-0071    frame 698: sprite 007DEE80 at (119.618, 122.663), cell 18,26, feet 13: layer 24 drawn in layer 25; stopped by what a cell record drew
    DIV-0071      layer 26: the record run at 008D3724 (view column 14) drew a quad over the sprite last pass (2 kept; the sprite's box (81.6184, 78.6625)-(185.618, 141.663))
    DIV-0071    frame 479: sprite 007DEFC8 at (222.284, 137.301), cell 20,23, feet 13: layer 26 drawn in layer 27; stopped by what a cell record drew
    DIV-0071      layer 28: the record run at 008D3744 (view column 14) drew a quad over the sprite last pass (2 kept; the sprite's box (156.284, 93.301)-(260.284, 156.301))

**Costs and limits.** The box is the cells' rectangles, not the opaque
pixels, so it is larger than the art. A record run whose quads fall in a
sprite's transparent corners now also stops the move. The routes below found
no frame where that changed a pixel. The box is one pass old, as the quads
are. Static data is 2 x 64 x 20 bytes.

**No fuzz needed.** This arms with DIV-0071, after the self-tests:
`BeforeSprite` and `AfterSprite` are called only under mode 1, and `g_mode`
is `kOff` until `Arm`. The self-tests compare `Sprite_DrawPass` with
Capcom's on the unarmed path.

## 4. Checks

**Captures, `fix` against `new`** (`--speed 4` except `cs_*` and the camp):

| Recipe | Lang | Shots | Identical | The rest |
|---|---|--:|--:|---|
| `cs_53` | en-US | 41 | 20 | 21 differ in the fire's box; **all 41 equal to `off` and to `origdraw`** |
| `cs_192` | en-US | 4 | 1 | 3 differ in the fire's box; **all 4 equal to `off` and to `origdraw`** |
| `vaccinationAndFireScene`, every 60, 1,260..2,940 | en-US | 29 | 29 | |
| `campingFishing`, every 60, 420..1,200, and its 9 | en-US | 22 | 22 | |
| `dolphinBoss`, FY's 19 (every 105, 960..2,850) - **the faerie** | en-US | 19 | 19 | |
| `dolphinBoss`, every 60 to 3,600 (CF's set) | en-US | 60 | 60 | |
| `ninaWalkBehindBlock`, every 60 | en-US | 11 | 11 | |
| `eldritchGene`, every 60 | en-US | 59 | 58 | f03000: the dialogue's typing, which differs between two runs of one build (`new` vs `new`: 912 px; `fix` vs `fix`: 160 px; `--speed 4` and 1 alike) |
| `yraallRoad`, every 30, 3,300..3,720 (D306's bush) | en-US | 15 | 15 | |
| `yraallRoad`, every 120 | en-US | 41 | 41 | |
| `worldMapAndAreaTransition_ab` | en-US | 35 | 35 | |
| `field_view` | original | 4 | 4 | |

FY's faerie is untouched: `dolphin_fix` against `dolphin_new` is 19 / 19
identical, so FY's captures stand as FY left them.

**Self-tests** (`selftest.py`: `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original
BOF3X_SHADOW='*'`, an ini-less copy, `--no-config`), with `BOF3X_WIDE`
unset, `1` and `2`:

- All three exit 0 (the hosted game's exit code, read by the launcher).
- `inject: 10087 ours, 0 left original by BOF3X_ORIGINAL` in each.
- 1,092 / 1,094 / 1,094 lines name mismatches, and every count is 0
  (`grep -i -E "[1-9][0-9]* mismatch"` finds none).
- Among them: `draw_pass self-test: 3000 rounds ... 0 MISMATCHES`
  (`Sprite_DrawPass`, the one function touched outside `layering.*`) and
  `draw_emit self-test: 2 functions, 12000 rounds ... 0 MISMATCHES`.

**State hash**, the ten routes, narrow, layering at its default, `fix0`
against `main`'s build `base0` (`chain.py`, `check.py diff`; a second run of
`main`'s build, `base0b` or `base0d`, for the noise, `statehash.py check`):

- **`Rand`:** the `randlog` lines (one per frame) are identical between
  `base0` and `fix0` on all ten routes, from 1,455 to 13,182 frames.
- **Identical:** `combat` (2,625 ticks), `dragonTransform`, `field_menu`,
  `menu_screens` and `shop`. With the noise filtered (`check base0 base0b
  fix0`), `caughFish` and `masterAndManillo` are also identical. Their only
  page, `0x6C3000`, Direct3D's state, differs between two runs of `main`.
- **`cutsceneAndNue`, `whelpBoss`:** with the noise filtered, only the
  start-up group at ticks 2..3: the VRAM shadow pages, the sound page
  `0x7DE000` and `0x937000`. FY's runs saw the same group come and go
  between runs of one build ([`faerie-fix.md`](faerie-fix.md) 4). No field
  pass has drawn by then.
- **`worldMapAndAreaTransition`:** the same start-up group, `0x6C3000`
  (noise), and **`0x7E0000` (ticks 2,064..2,112, 34) and `0x903000` (2,064..2,082,
  5)**. Raw dumps of both sides at ticks 2,064 and 2,070
  (`BOF3X_STATEHASH_DUMP`, `statehash.py bytes`) name the bytes in those
  two pages: `Sprite_DrawList+0xC` (two entries swapped, objects `0x7DEE80`
  and `0x802E8C`) and `Sprite_DrawRecords+0xC`. That is the draw list's
  order after a different verdict, the same pages FY names for DIV-0071
  itself. It is not game state. A second run of the fix (`fix0d`) repeats
  them exactly. The control (`BOF3X_LAYERING_SPRITEBOX=0`, `fixctl0`)
  differs from `main` only on `0x6C3000`, so the round's other changes are
  inert here and these two pages are this amendment's. The picture: that
  recipe shot every 6 frames from 1,980 to its end at 2,142, `fix` against
  `new`, 28 / 28 identical. The swapped sprites do not overlap.

No game page differs. The negative control on the captures:
`BOF3X_LAYERING_SPRITEBOX=0` gives `cs_53` 41 / 41 and `cs_192` 4 / 4
identical to `new` (Garr over the fire again).

## 5. Sheets (in the harness root, not committed)

`C:\Users\kerok\bof3x-runs\r_ring\GF\sheets\`:

- `garr_fire_053.png`: `cs_53` f0610, f0700, f1000, 2x crops. Columns:
  `main`, round, fix, `LAYERING=0`, Capcom's draw pass. The first two have
  Garr's arm and weapon over the flames; the last three have the flames
  over them.
- `garr_fire_192.png`: `cs_192` f0480, f0600, the same five columns.
- `garr_fire_vacc.png`: the vaccination scene, f01380, f02100, f02940:
  round, fix, `LAYERING=0`. D301's fix holds; the fire is over Garr in all
  three.
- `faerie_dolphin.png`: the faerie at f01065, f01905, f02010, f02850:
  round, fix, `LAYERING=0`. Round and fix are identical, with the faerie
  whole over the table.
- `gene_f03000.png`: the one non-identical `eldritchGene` shot. It is the
  dialogue's typing, which also differs between runs of one build.

## 6. For the other files

**A - `docs/DIVERGENCE.md`, DIV-0071, an amendment** (append after the
2026-10-17 one):

- **Amended 2026-10-21 (the wide-ring round, group GF, the owner's "Garr
  over fire" report; [`garr-fire.md`](garr-fire.md), known-defects D301):
  the record test takes what the sprite drew.** The 2026-10-14 amendment
  tested a cell record's last-pass quads against the sprite's body box, 28
  px wide. In two camps Garr sits beside the fire, not at it: chapter 6's
  `AREA053` camp (`cs_53.txt`) and `AREA192`'s (`cs_192.txt`). That is the
  same `MapCell_DrawFrames` record as D301, three cells from him. His arm
  and weapon reach over the flames 4 to 26 px beyond the body box, so the
  rule still moved him three layers on, over the fire. `Sprite_DrawPass`
  now notes each sprite's committed cell sprite (code `0x84`), and its box
  (the cells' union, as `D3d_BuildCellTexture` lays them out) is kept for
  the next pass. The record test uses the body box grown to it. Nothing
  else in the rule changes, and it only stops moves. The quicksave's
  graphics record carries the boxes (version 3).
  `BOF3X_LAYERING_SPRITEBOX=0` drops this amendment alone (the control).
  Verified live, narrow:
  - `cs_53` 41 shots and `cs_192` 4, pixel-identical to `BOF3X_LAYERING=0`
    and to Capcom's draw pass (`BOF3X_ORIGINAL` naming its five functions);
    `main`'s build drew Garr over the fire in 21 and 3 of them;
  - identical to the rule before it on `vaccinationAndFireScene` 29 / 29,
    `campingFishing` 22 / 22, `dolphinBoss` 19 / 19 (the D300 faerie) and
    60 / 60, `ninaWalkBehindBlock` 11 / 11, `yraallRoad` 56 / 56,
    `worldMapAndAreaTransition_ab` 35 / 35 and `field_view` 4 / 4;
  - `eldritchGene` 58 / 59, the other shot being dialogue typing that
    differs between runs of one build;
  - headless `'*'` at `BOF3X_WIDE` unset, 1 and 2: exit 0, every
    mismatch count 0;
  - the state hash on the ten routes, narrow, against `main`: `Rand` equal
    every frame, no game page; only the draw list's order differs, on
    `worldMapAndAreaTransition` ticks 2,064..2,112, where the picture is
    identical.

  **Not yet seen by the owner.**

**B - `docs/known-defects.md`, D301's status line** (title: "... fixed by
DIV-0071's 2026-10-14 and 2026-10-21 amendments"):

> **Status:** fixed by DIV-0071's 2026-10-14 amendment for the scene
> reported (`vaccinationAndFireScene.txt`; the owner approved it then), and
> by its 2026-10-21 amendment ([`garr-fire.md`](garr-fire.md)) for two more
> camps where Garr sits beside the same fire record with his arm and weapon
> over it: `AREA053` in chapter 6 (`cs_53.txt`) and `AREA192`
> (`cs_192.txt`). Those two were drawn wrong by `main`'s build since
> DIV-0071 (the owner's "Garr over fire", 2026-10-21). The body box the
> 10-14 test used did not reach the fire; the sprite's own drawn box does.
> Both are now pixel-identical to Capcom's draw pass. Seen on captures; not
> yet in play.

**C - `docs/owner-review.md`:** the two camps on the sheets of section 5
(`garr_fire_053.png`, `garr_fire_192.png`), or in play. Capcom's order hides
Garr's weapon in the flames, which is what the fix restores. Owed: the
owner's look.

**D - `docs/README.md`:** a row for this document:

| [`garr-fire.md`](garr-fire.md) | Garr over the camp's fire again (wide-ring round, group GF, 2026-10-21): not a regression of D301 but two more camps (`AREA053` chapter 6, `AREA192`) where Garr sits beside the same fire record, his arm over it outside the body box; fixed by DIV-0071's third amendment (the record test takes the sprite's own drawn box). MEASURED. |

## 7. Not done

- **Wide** (`BOF3X_WIDE=1` / `2`) was not captured live. The sprite's box
  and the runs' quads are both in the primitives' own coordinates, before
  the wide shift, as DIV-0071's tests are. The self-tests ran at all three
  widths (section 4); the state hash ran narrow only.
- **A census** of other sprites wider than the body box beside records (a
  boss, a large NPC by a plant). The routes of section 4 found none whose
  picture changed.
- **CF's full `BOF3X_ORIGINAL` set** stalled on `cs_53` before recipe frame
  0 (above). The five draw-pass functions settle the order.
