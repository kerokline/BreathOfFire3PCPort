# The ST3 check: dolphinBoss on the narrow route, case zero off the top row, the hide in mode 4 (ultrawide round, EB-3)

**Status:** MEASURED (2026-10-14), live.

- **Branch and tip:** branch `wide/eb3` off the round tip `7d4122c4`. The tip
  holds ST3 ([`wide-staging.md`](wide-staging.md) 5a and 6): case zero's
  walks on the original's frame count, north steps chosen at run time, and
  the hide admitting `Game_Mode` 4. It also holds the frame-grab restoration
  (`BOF3X_FBGRAB`), the D242 layering fix and the lighthouse rays fix
  (DIV-LR-A).
- **The four checks:** the coordinator's EB-3 brief. Earlier parts:
  [`ultrawide-census-3.md`](ultrawide-census-3.md).
- **The launcher:** an ini-less copy, `uw_eb1/L6/`.
- **Where things are:**
  - Run scripts: `c4_main.sh`, `c4_dump.sh` and `c4_cz.sh` in the session
    scratchpad's `uw_eb1/`, with `hash6_one.sh` and `census6_one.sh`.
  - Hash runs: `analysis/statehash/cn_<route>_eb4*`.
  - Shots and sheets: `analysis/shots/uw_census4/`.
  - Census logs: `analysis/ultrawide/census4/`.
- Nothing in `src/` changed.

## 0. The answer

1. **dolphinBoss stays on the narrow route at 16:9 and 21:9, through the
   fight's end.**
   - The `Rand` count equals narrow on all 14,923 recipe frames.
   - The state hash shows 24 pages differing, all of them draw state or the
     staged actors' own records:
     - the case-zero pair, party slots 1 and 2, from tick 5995 to 8543;
     - the re-staged sites 15, 16 and 17: objects 3 and 4, `+0x10[6]` and
       `[7]`, from tick 3262;
     - the bit 7 of object 4 while it is staged outside the edge.
   - By tick 9000 the party records and the live objects equal narrow's.
     Census 3 had this route parting at about frame 6184.
2. **Case zero.**
   - **No sliver on the top row.** Through the waits, the 4:3 middle of the
     16:9 and 21:9 pictures is pixel-identical to narrow's. The only
     differences are on the frames the pair leave by the top instead of the
     left: 6072..6084 and 7434..7454.
   - **Same frames as narrow's.** The walks start on the same frames as
     narrow's. The DIV-E lines are at 5984 / 5994, 7347 / 7363 and
     8471 / 8487, and the dialogue matches frame for frame.
   - **North steps.** The build logs no north-step count. The dumps place
     the pair at map (25.5, 24) where narrow has (24, 30) in the first wait,
     and at (25.5, 22) where narrow has (24, 28) in the second. That is 1.5
     cells fewer west and **6 cells, 12 steps, north**, in both waits. At
     tick 9000, after the east walk, they stand on narrow's cells.
3. **balioAndSunder_2 at 21:9: the mode-4 rows are hidden now.**
   - Every field-object row in a scene in areas 39 and 41 is `hidden yes`,
     in mode 2 and in mode 4 (283 rows).
   - **The leader (party slot 0) is never hidden** (51 rows).
   - Not hidden: one area-load row (mode 1), one battle row (mode 5), three
     mode-7 rows, and the world map's three `other` rows in area 45.
4. **The picture-side changes left logic alone.**
   - Ten of the eleven hash routes at 21:9 were compared with census 3's
     21:9 runs on the previous tip (`eb3w2`); `depress` was compared with
     its narrow run only (row below). Five of them are **identical on every
     tick**: combat, shop, worldMapAndAreaTransition, caughFish,
     masterAndManillo and cutsceneAndNue. shop is identical to its repeat
     (`eb3w2b`).
   - field_menu and menu_screens differ only on the exe host's tick-3
     pages.
   - dragonTransform is identical to its census-3 repeat (`eb3w2b`).
   - whelpBoss differs only on the draw pages (`Sprite_DrawRecords`, the
     window page) at 396..7463. That is ST3's hide now hiding its mode-4
     band actors.
   - Against narrow, every route has `Rand` equal and only picture pages
     differing; depress included.
   - **eldritchGene** (the rays and the gene; not a `cn_` route, run at 0 and
     2 under en-US): `Rand` equal, 13 pages, all picture (`DrawTable`, the
     message pen x).

## 1. dolphinBoss and case zero

### 1.1 The runs

- **Hash runs.** `dolphinBoss` with case zero's shot windows (the census-3
  copy, shots every 8 frames) was hashed at `--wide 0`, `1` and `2`. The
  settings: en-US, staging and hide at their defaults,
  `BOF3X_WIDE_STAGING_LOG=1`, speed 1. Tags are `eb4cz0/1/2`; the shots are
  in `dolphinBoss_cz_w<N>/`.
- **Raw dumps.** Separate runs at narrow and 21:9 (`eb4dw0/2`) dumped ticks
  3300, 6200, 8000, 9000 and 12000.
- **Windows on the narrow timeline.** The census-3 windows sat where the
  lagging wide runs had their walks (7650.., 8940..), and the walks now
  happen earlier (7347, 8471). So a second copy took shots every 4 frames
  over 5980..6120, 7330..7520 and 8456..8660 (`recipe_shots.py` three
  times). It ran at 0, 1 and 2 with the band census (`dolphinBoss_cz3_w<N>`,
  speed 8). Its `Rand` also equals narrow on all frames.

### 1.2 The hash

`statehash.py diff` of `eb4cz1` and `eb4cz2` against `eb4cz0`: 24 pages over
14,913 ticks, `Rand` equal. The same at both widths.

| Page | Ticks | Read in the dumps |
|---|---|---|
| static display pages, draw records, map view, `Desktop_Width`, window | from 0 / the first field frame | picture |
| `0x801000` `DrawTable`, `0x92B000`, `0x8E4000..` `Gfx_UnpackScratch` | 398.., 6069.. | draw state, uploads |
| `0x7E1000` | 977..12244 | `Text_LineX` and message pen x (WA) |
| `0x7DF000` `Sprite_Objects+0x180` | 3262..14912 | objects 3 and 4, scripts `+0x10[6]` and `[7]` of area 175: **sites 15, 16 and 17**. Object 4 at z 12 against narrow's 21 from 3300 (the kIn start moved), with bit 7 set while it is outside. At 12000 both are inactive (byte 0 = 0) and only their last positions differ |
| `0x802000` `ObjTrio` / `Field_Members` | 5995..8543 | party slots 1 and 2, **case zero's pair**. Equal again at 9000 |

### 1.3 The pictures

These are the sheets for the coordinator, all in
`analysis/shots/uw_census4/sheets/`. Each shows narrow, 16:9 and 21:9 side
by side, with the 4:3 edges in yellow and the 16:9 edges in blue:

- **`casezero_part1.png`**: recipe frames 6008, 6048, 6072, 6096, 6200 and
  6400.
- `casezero_part1_close.png`: 6060..6112, the pair leaving.
- `casezero_part2.png`: 7330..7502.
- `casezero_east.png`: 8456..8620.

`uw_eb1/czdiff.py` compares each wide shot's 4:3 middle with the narrow shot
of the same frame, counting pixels that differ by more than 24, split into
the top 16 rows and the rest above the message box:

- **Part one** (5980..6600, both windows): differences only at 6072, 6080
  and 6084, where the pair leave by the top-left corner going north while
  narrow's go west. **Every frame from 6088 to 6600 is identical to
  narrow's**: no sliver, at 16:9 or 21:9.
- **Part two** (7330..7520 and the census-3 window 7650..8300): differences
  only at 7434..7454, as they leave; identical after.
- **The east walk** (8456..8624):
  - The 4:3 middle differs throughout. The faeries of sites 15, 16 and 17
    come in on their re-staged paths: at 8456 three of them are on screen in
    the wide pictures where narrow has two, and at 8476 they stand at other
    points.
  - From about 8544 the remaining differences repeat with a 16-frame period
    in the area where the faeries hover. I read that, unproven, as their
    animation phase. The pair come back down from the top where narrow's
    come from the left, and reach the same spot by 8580.
  - At tick 9000 the dumps show no object or party byte differing.
  - These are the scenes the wide_staging rows name (`DIV-D site 15/16/17`
    and `DIV-E`), as the brief allows.
- **The census** (`dolphinBoss_cz3_w2.log`): the pair have no row in either
  band during the waits.
  - What remains in area 175 is the census-3 set:
    - party slots 1 and 2 standing in the right 16:9 band at 3571..4740
      (not one of ST's rows);
    - their 21:9 stretches `hidden`;
    - the leader never hidden.

## 2. balioAndSunder_2 at 21:9

`analysis/ultrawide/census4/balioAndSunder_2_w2.log`. These are the 21:9-band
rows with the scene bit in areas 39, 41 and 45, grouped by `Game_Mode`,
record and hidden:

| Area | Mode | Record | Hidden | Rows |
|--:|--:|---|---|--:|
| 39 | 2 | obj | yes | 72 |
| 39 | 4 | obj | **yes** | 74 |
| 39 | 2, 4 | party 0 (leader) | no | 2 + 2 |
| 41 | 2 | obj | yes | 74 |
| 41 | 4 | obj | **yes** | 63 |
| 41 | 2, 4 | party 0 (leader) | no | 27 + 20 |
| 41 | 1 / 5 / 7 | obj / other / obj | no | 1 / 1 / 3 |
| 45 | 1, 2 | other (the world map) | no | 1 + 2 |

Census 3 had areas 39 and 41's 175 mode-4 object rows not hidden. All are
hidden now.

## 3. The 21:9 repeat on this tip

Each route is hashed at `--wide 2` (`eb4w2`) and compared three ways: with
the previous tip's 21:9 run (`eb3w2`, or its repeat `eb3w2b`), with narrow
on the previous tip (`eb3w0`), and by the `Rand` count.

| Route | Against the previous tip's 21:9 | Against narrow |
|---|---|---|
| combat | identical, 2,561 ticks | picture, 16 pages; `Rand` equal |
| shop | identical to `eb3w2b`, 3,137 ticks | picture, 16; equal |
| worldMapAndAreaTransition | identical, 2,113 ticks | picture, 11; equal |
| field_menu | tick-3 pages only | picture, 9; equal |
| menu_screens | tick-3 pages only | picture, 11; equal |
| dragonTransform | identical to `eb3w2b` | picture, 11; equal |
| caughFish | identical, 3,841 ticks | picture, 11; equal |
| masterAndManillo | identical, 5,505 ticks | picture, 11; equal |
| cutsceneAndNue | identical, 7,425 ticks | picture, 13; equal |
| whelpBoss | 2 draw pages at 396..7463 (the hide in mode 4) | picture, 14; equal |
| depress | identical, 1,601 ticks | picture, 9; equal |
| eldritchGene (en-US; its own narrow run on this tip, `eb4w0`) | - | picture, 13 (`DrawTable` 734.., the message pen x 1142..3362); `Rand` equal on all 3,521 ticks |

The frame grab, D242 and the rays change no hashed byte on these routes,
`.data`'s VRAM shadow included. The pictures themselves were not compared.

## 4. What was not verified

- **The north-step count is derived, not logged.** It comes from the dumped
  party positions (12 steps in both waits); no log line reports it.
- **The faeries' 16-frame difference** in the east walk is read as animation
  phase, not proven. The dumps at 9000 are equal.
- **The pictures of the grab, D242 and the rays** were not looked at; only
  their effect on the hash was checked.

## 5. For the other files

**`docs/README.md`**, a row:

| [`ultrawide-census-4.md`](ultrawide-census-4.md) | The ST3 check (group EB-3, live, 2026-10-14): dolphinBoss on the narrow route at 16:9 and 21:9 through the fight (Rand equal on all frames; only the staged actors' records and draw state differ), case zero off the top row and in narrow's frames (12 north steps by the dumps), the 21:9 hide covering mode 4 (balioAndSunder_2), and the 21:9 hash on this tip identical to the previous tip's (the grab, D242 and the rays touch no hashed byte). MEASURED. |

**`docs/STATUS.md`:** EB-3 - ST3 verified live: no recorded route parts from
narrow at 16:9 or 21:9 on the twelve routes run.

**`docs/HANDOFF.md`:**
- Case zero is done for the round. Still owed: the owner's eye on
  `casezero_part1.png`, `casezero_part2.png` and `casezero_east.png` in
  `analysis/shots/uw_census4/sheets/`.
- A log line for the north steps chosen would save the next check a dump.
