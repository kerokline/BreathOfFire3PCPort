# The bit-15 census: which semi-transparent draws the port blends wrongly

**Status:** STABLE for its five routes (2026-10-23, step 1 of the bit-15 round, [`HANDOFF.md`](HANDOFF.md) item 00;
branch `render/stp-blend`). The PlayStation side of the text palettes is measured on the twin (section 4).
MEASURED on fourteen more routes and the spell sweep's member side (2026-10-27, section 6, branch `away1027/stp`).
They show one new family, the world map's button legend, in blend mode 0, and the first draws that mix the two
kinds of texel. The twin's CLUT for it has the same shape, but no twin state shows the world map.

## 1. The question

On the PlayStation a semi-transparent textured primitive blends only the texels whose colour has bit 15 set; a
texel with bit 15 clear draws opaque, and a colour of 0 is transparent. The PC port drops bit 15
(`Gfx_ConvertRow` `0x59EBB0`, `Tex_Convert16` `0x5A9D2F`) and blends the whole primitive (`D3d_SetBlend`
`0x59FCA0`). The rooftop rope in area 28 is one result: it draws at about a quarter of its PSX brightness. Before
changing every semi-transparent textured draw, this census sizes the change: which draws have texels the port
blends but the PlayStation would draw opaque.

## 2. The instrument

`BOF3X_STPCENSUS=<file.tsv>` (`src/hook/stp_census.{h,cpp}`, diagnostic only, no DIV). It hooks `Gfx_DrawOTag`'s
dispatch on the Direct3D path. For each primitive whose code has bits 1 and 2 set (semi-transparent, textured), it
reads every texel the primitive can sample out of the VRAM shadow and the primitive's CLUT:

- polygons FT3 / FT4 / GT3 / GT4: the box of the corners' (u, v), through the page's texture window
  (`Gfx_TexCacheKey`, which `D3d_BuildPageTexture` tiles across its 256 x 256 texture);
- the sprites SPRT / SPRT_8 / SPRT_16: (u, v) to (u + w, v + h) of `Gfx_DrawTpage`'s page;
- the cell sprite `0x84`: each `SpriteCell_Table` record's block, as `D3d_BuildCellTexture` reads it.

The glyph `0x6C` is skipped: `D3d_DrawGlyph` turns the blend off. Each texel is counted as zero, bit 15 set
(`stp`) or bit 15 clear and non-zero (`opaque`). Each draw is then classed by its texels: stp only (correct
today), opaque only (the PSX draws it all opaque), mixed, or clear (nothing drawn). Rows aggregate the draws by
area, code, tpage, CLUT and window, with the recipe frames, the (u, v) box and the screen box of the first corners.
`tools/stp_census.py` summarises one or more files.

A box of (u, v) is an over-estimate for a rotated or scaled polygon. It cannot hide an opaque-only draw, but it
could report a draw as mixed when it is not. None was reported mixed.

## 3. The runs

At x1 from an ini-less launcher copy (four files: `bof3x-launcher.exe`, `bof3x-launcher-dev.exe`, `bof3x.dll`,
`bof3x.exe`; without `bof3x.exe` the launcher stops on a modal "bof3x.exe is missing" dialog, which cost the world-map run on
2026-10-27, run again by section 6's `chain2.sh`), `--lang en-US`, `BOF3X_FILTER=point`; root
`C:\Users\kerok\bof3x-runs\stp_census` (`chain.sh`, `census_<route>.tsv`, logs).

| route | save | frames walked | semi-textured draws | changed (opaque only) | frames with a change |
|---|---|---|---|---|---|
| `attract_cycle` | none | 11,100 (timed out at recipe frame 11,190 of ~12,300) | 31,096 | 126 | 126 |
| `combat` | `combat` | 2,400 | 16,122 | 5,036 | 578 |
| `bossAndFlash` | `bossAndFlash` | 5,400 | 50,984 | 14,679 | 1,035 |
| `dragonTransform` | `adult_ryu` | 4,200 | 17,746 | 11,747 | 1,786 |
| `ropeAndAmalgam` (PR #112's recipe) | `ropeAndAmalgam` | 8,400 | 7,519 | 3,270 | 296 |

Over all five, 123,467 draws: **34,858 opaque only, 0 mixed**, the rest stp only.

By blend mode, all routes together:

| blend | draws | stp only | opaque only |
|---|---|---|---|
| 0 (half) | 20,085 | 20,085 | 0 |
| 1 (add) | 101,658 | 67,907 | 33,751 |
| 2 (subtract) | 576 | 576 | 0 |
| 3 (quarter add) | 1,148 | 41 | 1,107 |

## 4. What changes

Only two families of draws have bit-15-clear texels:

1. **The rope**, area 28: FT4, code `2E`, tpage `007B` (page 27, 4-bit, blend 3), CLUT `78C3`. 1,107 draws, every
   texel either 0 or bit 15 clear; recipe frames 264..383. This is the known case, and it matches the handoff's
   PSX measurement. The same area's CLUT `78C6` on the same page is stp only, so it is right already.
2. **Text on page 15**: tpage `002F` (page 15, 4-bit, blend 1, additive), CLUTs `7800` and `7807`, the text
   colour palettes at (colour << 4, 480) (`battle_flow.cpp`, `battle_window_draw.cpp`). SPRT_8 `76` and SPRT
   `66`. Everything else in the "changed" column: 33,751 draws. Located by screen box and shots
   (`s_combat_shots\`):
   - on the title, a 10 x 10 SPRT at (262, 130), recipe frames ~40..120 on every route: by its position the (R)
     mark beside the logo (not singled out in a shot);
   - the load screen's slot labels, numbers, LV and TIME (SPRT_8, x 57..119, y 59..205);
   - in battle, the numbers and short words over the field, and the target's HP pop-up, which slides in (SPRT_8,
     x -99..444, y 73..185).

   Every one seen sits on an opaque dark backing (the "Max!" text has a black box under it; the pop-up is a dark
   window), so additive and opaque draw nearly the same there. The difference grows with the backing's
   brightness: added to a dark grey backing, the text comes out lighter than its palette colour.

**The PlayStation agrees** (measured 2026-10-23). The twin is the sibling's runtime on the Japanese disc
(SLPS-00990). Its savestates `saves/openbios/state_8014AA0C_slot00` (title), `slot02` (field, AREA014),
`slot03` (a field battle) and `slot06` (the NEW GAME / LOAD GAME menu) were read offline with the sibling's
`tools/pst_tool.py` (`load`, `vram_u16`). In all four, VRAM (0..15, 480) and (112..127, 480) hold the same
values, and bit 15 is clear in every non-zero entry:

    7800 (0,480):   0000 6739 5EF7 5294 4631 35AD 2108 1084 0400 77BF 439F 071F 1E7D 31BA 4918 2CCE
    7807 (112,480): 0000 35AE 318D 2D6C 252A 18C7 0C64 0843 0000 4A53 6318 0000 0000 0000 0000 0000

So the PlayStation draws this text opaque, and the port's additive draw is a defect of Capcom's port, as the
rope's is. The strip (VRAM rows 480..511) is game data uploaded unchanged on the PC (`Gfx_FlushDirtyStrip`
`0x454960`). Row 483, by contrast, changes with the area: the rope's palette is there only in the rope's area, and other states
hold other palettes there, some with bit 15 set. That is consistent with the census's per-area rows. The script:
`C:\Users\kerok\bof3x-runs\stp_census\clut480.py`. It reads the four rows named above from any `.pst`.

Never changed on these routes: blend modes 0 and 2, every GT4, FT3 and GT3, and every cell sprite (all characters
and monsters). Every texel that has bit 15 set in these draws stays blended as today.

## 5. What it means for the rest of the round

- **No draw mixes the two kinds of texel.** On these routes, deciding per primitive (opaque if no texel has bit
  15, blended otherwise) would give the same picture as deciding per texel. The per-texel path (plan steps 2 and 3)
  is still what the PlayStation does, and the only form that is right for a texture this census did not reach. But
  it carries the cost of a 32-bit page format. Per-primitive is a cheaper fallback, and this census is its
  evidence.
- The visible change is the rope plus a slight darkening of text drawn over dark grey windows. Effects routes
  (the boss's flash, the dragon transformation) blend only stp texels: unchanged.
- Routes not reached on 2026-10-23: the world map, the faerie village, fishing, the minigames, the late-game
  effects. Section 6 reaches all of them but the minigames; 6.5 lists what is still not reached.
- **Amended 2026-10-27 (section 6):** the world map's button legend mixes the two kinds of texel in one
  primitive (2,709 draws), so the per-primitive fallback in the first bullet would draw it wrong. DIV-0160 draws
  per texel, which is what these draws need.

## 6. The routes section 5 named (2026-10-27)

### 6.1 The runs

With DIV-0160 built and on, from an ini-less launcher copy (`bof3x-launcher.exe`, `bof3x-launcher-dev.exe`,
`bof3x.dll`, `bof3x.exe`), `--lang en-US`, `BOF3X_FILTER=point`, the PC install (the main checkout's `bof3`).
Root `C:\Users\kerok\bof3x-runs\away1027\stp` (`chain.sh`, `chain2.sh`, `chain3.sh`, `census_<route>.tsv`,
`sweep_census\`, logs, `table.py` for the table below). The recorded routes ran at x1, so every frame was drawn.
The census reads nothing that depends on speed: it counts the primitives of each drawn frame, and frame numbers
come from the recipe. At x4, though, only about one frame in four is drawn, so a short effect can be missed. For
that reason only the spell sweep, whose casts last many frames, ran at its usual x4.

Chosen routes: `worldMapAndAreaTransition` (the world map and an area change; the `quick_` recipes replay the
same route with F6/F7 added), `fairyVillage` and `fairyVillageNews` (the faerie village, areas 175..185),
`campingFishing` and `caughFish` (the camp, the fishing spot, a catch, Manillo's screen), `menu_screens`,
`shop` and `identify` (menus, a shop, the bio-plant), and for the late game `ending17`, `cutsceneAndNue`,
`eldritchGene`, `minecartToZombieDragon`, `dolphinBoss` and `genmelAndLift`. Fishing is the one minigame a
recipe reaches: the minigame starts are still owed by the owner ([`button-routes.md`](button-routes.md)). The
spell sweep (`tools/spell_sweep.py run --side member --sections member`, the 2026-10-26 `full_pc` form, chunks of
ten, `BOF3X_STPCENSUS=...\{chunk}.tsv`) cast 157 of the member side's 160 abilities. Pilfer was not reached,
Accession was refused and Snooze was not reached: the same seek failures as on 2026-10-26.

| route | save | frames walked | semi-textured draws | opaque only | mixed | frames with a change | other text palettes | new, not text |
|---|---|--:|--:|--:|--:|--:|--:|--:|
| `worldMapAndAreaTransition` | `town` | 2,100 | 30,770 | 5,646 | 542 | 764 | 0 | 1,928 |
| `fairyVillage` | `fairyVillage` | 4,680 | 75,658 | 18,471 | 490 | 2,929 | 404 | 1,230 |
| `fairyVillageNews` | `fairyVillage` | 720 | 26,087 | 2,492 | 301 | 492 | 0 | 698 |
| `campingFishing` | `camping` | 5,220 | 380,907 | 32,177 | 548 | 3,540 | 123 | 1,653 |
| `caughFish` | `camping` | 3,840 | 448,447 | 8,526 | 546 | 3,052 | 0 | 1,467 |
| `menu_screens` | `adult_ryu` | 1,740 | 31,907 | 25,853 | 0 | 877 | 0 | 0 |
| `shop` | `town` | 3,120 | 58,641 | 49,415 | 0 | 1,419 | 1,438 | 0 |
| `identify` | `identify` | 1,200 | 4,721 | 1,845 | 6 | 194 | 0 | 6 |
| `ending17` | `ending17` | 4,080 | 78,605 | 5,497 | 0 | 272 | 0 | 0 |
| `cutsceneAndNue` | `nue` | 7,380 | 26,780 | 15,561 | 0 | 767 | 112 | 0 |
| `eldritchGene` | `eldritchGene` | 3,540 | 143,073 | 2,299 | 0 | 149 | 0 | 0 |
| `minecartToZombieDragon` | `minecartToZombieDragon` | 8,100 | 11,066 | 2,309 | 0 | 219 | 0 | 0 |
| `dolphinBoss` | `dolphinBoss` | 14,880 | 529,353 | 9,934 | 282 | 1,572 | 0 | 668 |
| `genmelAndLift` | `genmelAndLift` | 22,500 | 18,292 | 1,337 | 0 | 112 | 0 | 0 |
| spell sweep, member side (17 chunks, x4) | `combat` | 63,600 | 312,322 | 62,629 | 0 | - | 0 | 0 |

"Other text palettes" are page-15 draws through CLUTs other than `7800` / `7807`. "New, not text" counts the
changed draws (opaque only plus mixed) outside the rope and page 15. The recorded routes together: 1,864,307
draws, 181,362 opaque only, 2,715 mixed. By blend mode:

| blend | draws | stp only | opaque only | mixed |
|---|--:|--:|--:|--:|
| 0 (half) | 328,365 | 299,517 | 4,935 | 2,715 |
| 1 (add) | 270,340 | 93,574 | 176,427 | 0 |
| 2 (subtract) | 581,898 | 561,444 | 0 | 0 |
| 3 (quarter add) | 683,704 | 683,704 | 0 | 0 |

The sweep's first chunk (abilities 2..12) ran twice. On the first run its census file was empty: the harness
kills the game at `done` (`TerminateProcess`), and the kill fell inside the file's rewrite. Since this round the
census writes every 60 drawn frames, not 300, to a file beside its own, and moves that file into place (the
instrument's two changes, no behaviour). `chain3.sh` ran the chunk again with that build (`sweep_redo\`,
`sweep_census\redo_member_002.tsv`: 3,780 frames, 11,943 draws, 3,541 opaque only, all page-15 text). The
sweep row adds the two.

### 6.2 The rows, and what each draws

Everything outside these is the rope or the page-15 text palettes `7800` / `7807`, as in section 4. The sweep
adds nothing else: every changed draw of its 157 casts is page-15 text.

1. **The world map's button legend.** SPRT `66`, tpage `009C` (page 28, 8-bit, blend 0 half), CLUT `7B80`
   (0, 494), uv (0, 56)..(128, 120), screen x 64..220 (it slides in and out, y -32..240). The world maps: area
   33 (`worldMapAndAreaTransition`), 65 (`fairyVillage*`), 16 (`campingFishing`, `caughFish`), 87
   (`dolphinBoss`). 7,644 draws changed, 2,709 of them **mixed**: the first draws in either census with both
   kinds of texel in one primitive. Its texels are almost all bit 15 clear (in `caughFish`: 10,503 with bit 15,
   1,543,672 without, 176,897 zero). The on/off shots show what it is: the panel beside the compass with the
   button rows ("b .. c .. START" and their icons). With DIV-0160 the icons and the panel's frame come out in
   their palette colours (on: (132, 132, 115), (115, 115, 115), (99, 90, 90); off, Capcom's half blend over what
   is under them: (140, 140, 119), (140, 140, 140), (132, 127, 127)). The pixels that change by more than 6 in
   RGB lie inside the legend (and the area label below, which is page-15 text): `dolphinBoss` frame 500, 1,116
   pixels in (128, 64)..(239, 107) of the 640 x 480 frame. Small in size, but visible in a side-by-side view.
2. **Page-15 text through the other colour palettes.** SPRT_8 `76`, tpage `002F`, CLUTs `7802` (32, 480) and
   `7803` (48, 480): 2,077 draws in the shop (the stat numbers that slide in from x -98, and the coloured
   number for a stat the item would change), the faerie village (3,956..4,056), the camp (1,684..2,108), and
   the Nue route's house (4,511..5,082). The same family as section 4's `7800` / `7807`: the PC's texels have
   bit 15 clear, the change is the text drawn in its palette colour, not added. The shots show only the
   brightness of the numbers changing.
3. **One texel of a cell sprite in the bio-plant.** Cell `86`, tpage `0087` (8-bit, blend 0), CLUT `7C00`
   (0, 496), area 117, screen (93, 86), `identify` frames 612..769: 144 draws, 6 mixed: 6 bit-15-clear texels in all,
   one in each mixed draw, against 439,007 with bit 15. Six shots over those frames (615..765, every 30) differ only in the page-15 numbers
   column at x 414..425. No pixel of the sprite differs. The texel is in the uv box of a record that is not seen
   (section 2: the box over-estimates), or the colour it takes is the same either way.

### 6.3 The PlayStation side

The twin's savestates (`../BreathOfFire3Recomp/saves/openbios/state_8014AA0C_slot00..11.pst`, read with its
`tools/pst_tool.py`; scripts `away1027\stp\twin\wm_sprite.py` and an inline read of row 480):

- **CLUT (0, 494)**, the legend's: in all five states that hold it (slots 1, 2, 3, 4, 10, field and battle), the
  row is identical (one hash). It has 255 non-zero entries, 35 of them with bit 15 (entries 64, 72, 75, 80,
  112..127, 130..132, 224, 232, 235, 241, 242, 244..248, 250, 254). It is a resident palette. Bit 15 is clear
  in most of it on the PlayStation too, which matches the PC's draws: almost all texels are bit 15 clear, with
  a few set. No twin state is on the world map, so the legend's own page cannot be compared texel for texel.
  The strip (VRAM rows 480..511) is uploaded unchanged on the PC (section 4, `Gfx_FlushDirtyStrip`
  `0x454960`).
- **CLUTs (32, 480) and (48, 480)**: identical in slots 0, 2, 3 and 6. `7802` has bit 15 clear in all 14
  non-zero entries; `7803` has it set in entry 11 only (`E198`), an entry the PC's glyph texels never sampled
  (0 stp texels in every `7803` row). So the PlayStation draws this text opaque, as it does `7800` / `7807`.

So the PlayStation would draw rows 1 and 2 as DIV-0160 now draws them: the bit-15-clear texels opaque, the
legend's few bit-15 texels blended. Nothing in this section is a wrong frame, and the renderer is not changed.

### 6.4 The pairs for the owner's eye

`away1027\stp\ab\<route>_{on,off}\` hold the frames (`--host bof3`, the dev launcher; off is
`BOF3X_ORIGINAL=StpBlend`, Capcom's whole-primitive blend). `away1027\stp\pairs\` holds the views: off, on and
the difference, left to right, or off over on in the zooms:

- `dolphin_f00500_legend_zoom_off_top_on_bottom.png`, `caught_f00600_legend_zoom_off_top_on_bottom.png`:
  the button legend, 3x;
- `caught_f00420_off_on_diff.png`, `caught_f00600_off_on_diff.png`, `dolphin_f00420_off_on_diff.png`,
  `dolphin_f00500_off_on_diff.png`, `fairy_f00300_off_on_diff.png` (a fade-in), `fairy_f00360_off_on_diff.png`:
  the legend and the area label on three world maps;
- `shop_f01800_off_on_diff.png`, `fairy_f03960_off_on_diff.png`, `fishing_f01900_off_on_diff.png`,
  `nue_f04515_off_on_diff.png`: the `7802` / `7803` text;
- `identify_f00735_off_on_diff.png`: the bio-plant frame, where only the page-15 numbers differ.

Outside those boxes, a frame differs by at most 1 in a channel (`caughFish` frame 420: 1,769 pixels on the edge
of the mist, the sum of the channel differences at most 2). That is the rounding of the step-3 shader on
bit-15 texels. The `diffs.py` script in the root measures each pair.

### 6.5 Still not reached

- **The rope pull is now reached** (2026-10-30, the owner's `ropeMiniGame.txt`, x1, `C:\Users\kerok\bof3x-runs\rope1030\census_rope2.tsv`: 10,440 frames, 32 rows): area 75's 689,604 draws change by 0.7 %, every changed row the page-15 text palettes `7800` / `7807` (the counters' windows and the messages), as section 4 found everywhere else; the rooftop rope is area 28's, not this area's. Nothing new for the bit-15 change.
- The minigames other than fishing and the rope pull: none is recorded ([`button-routes.md`](button-routes.md) lists the seven
  placed saves).
- The spell sweep's enemy side and the items. The member side ran at x4, which draws about a quarter of the
  frames.
- The disc-built installs (US PSX, PSP data). Their art and CLUTs are the disc's, so this census does not speak
  for them.
- The recorded routes not run here: the `cs_*` cutscenes, `stallion`, `masterAndManillo`, `radiationRoom`,
  `vaccinationAndFireScene`, `whelpBoss`, `balioAndSunder_*`, `yraallRoad`, `maekyssGorge`, `deisCave` and
  the Angel Tower saves.
