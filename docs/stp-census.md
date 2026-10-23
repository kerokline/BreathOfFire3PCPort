# The bit-15 census: which semi-transparent draws the port blends wrongly

**Status:** STABLE for its five routes (2026-10-23, step 1 of the bit-15 round, [`HANDOFF.md`](HANDOFF.md) item 00;
branch `render/stp-blend`). The PlayStation side of the text palettes is measured on the twin (section 4).

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

At x1 from an ini-less launcher copy, `--lang en-US`, `BOF3X_FILTER=point`; root
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
- Routes not reached: the world map, the faerie village, fishing, the minigames, the late-game effects. Rerun the
  census there before calling the change small: the instrument costs one environment variable a run.
