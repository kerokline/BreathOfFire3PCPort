# The PSP's widescreen choices at our 53 columns (group WA)

**Status:** BUILT (2026-10-12) - option (a) of
[`psp-widescreen-placements.md`](psp-widescreen-placements.md) section 5, as
the owner answered it in its section 6.1, on branch `psp/widescreen-a` of the
PSP-only round. Every change is an amendment of DIV-0041 and applies only in
the wide picture: narrow, every packet is the original's bit for bit (each
function's start-up fuzz compares it), and in the wide picture each value
moves by our 53 columns as the PSP's moved by its 32. Headless self-tests
only; nothing was seen in play. The pictures are owed (section 6).

## 1. The rule, and how the code reads it

The PSP release shifts every primitive by 32 columns and then re-authors
about 95 elements in six kinds (placements section 2). DIV-0041 shifts by 53
and had already done most of them; this group takes the rest:

| Kind | Rule here | Helper (`src/game/widescreen.h`) |
|---|---|---|
| full-frame fill | the left edge 0 goes to `-columns`, the right edge 320 or 319 to `320 + columns` / `319 + columns`; a tile's width to `320 + 2 columns` | `Widescreen_FillX()`, `Widescreen_FillWidth()`, **new** `Widescreen_FillRight(edge)` |
| the battle window's full-frame tile (size entry 11) | at the table's three readers: width + 2 columns, an x of 0 to `-columns` | **new** `Widescreen_BattleTile(size, x, w)` |
| cull | the x bounds out by the columns, the y bounds as they were | `Widescreen_Fill()` |
| edge anchor | a right-anchored x + columns, a left-anchored x - columns; centred elements never read it | **new** `Widescreen_Anchor()` |

All four read the **fill columns**: 0 until `Widescreen_ArmFills` has run
(after every module's self-test, in `InjectAll`) and 0 for good when the
picture is narrow, 53 after that in the wide picture. So the fuzz of every
function touched here compares the original's values whichever side of
`Widescreen_Inject` its module sits, and narrow play is the original's. No
table of the PSP's numbers is in the code: each change is "+- the columns"
applied to the value the original already holds.

## 2. The ~30 fills with no PC twin on record: paired

Placements section 2.2 left 25 PSX fill functions (22 PSP functions)
without an established PC twin. **Result: 15 were already wide (12 PC
functions), 9 are widened here (8 PC functions, `SpellRagnarok_DrawScreenTint`
among them), 1 has no PC twin found.** Each was paired here by **unit and shape**:
the PC functions of the same overlay (`analysis/magic_funcs.tsv`'s unit
column for the spells; `analysis/pairs_propagated.json`'s nearest paired
neighbours for the others) that draw a full-frame primitive, matched on the
PSX function's calls and immediates - primitive kind, semi-transparency,
texture page, ordering slot, the 319 or 320, colours (scratch `shape.py`,
`pshape.py`, `neigh.py`, `near.py`, `findimm.py`). A full scan of
`BOF3.exe`'s `.text` for every `319.0f` / `320.0f` operand (`scan319.py`: 11
functions hold 319.0, 59 hold 320.0) finds **every one inside a function
that is ours**, so no fill the PSP widened is left in Capcom's code.

| PSX (overlay) | PSP JP | Shape (PSX) | PC twin | Before this branch | Now |
|---|---|---|---|---|---|
| MAGIC060 `0x801F00C8` | `0x1737FC` | semi TILE 320 x 240, slot 3 | `IdentifyDim_Draw` `0x4B1770` (the unit's only fill) | **already wide** | - |
| MAGIC067 `0x801F00C4` | `0x17B774` | G4 0..319, bright edge by the owner's facing | `ChillFlash_Draw` `0x4B7240` | 320 | **widened** |
| MAGIC110 `0x801EF148` | `0x1A7E2C` | two G4 0..319 | `SpellRagnarok_DrawScreenTint` `0x4D55C0` (placements' guess confirmed by unit) | 320 | **widened** |
| MAGIC131 `0x801F030C` | `0x1BD620` | F4 0..319 | `KaiserFlash_Draw` `0x4E59A0` (between the pair's anchors `0x4E5640` / `0x4E6830`) | 320 | **widened** |
| MAGIC137 `0x801EFDF8` | `0x1C0330` | G4 0..319 by facing | `CoronaFlash_Draw` `0x4E7E80` | 320 | **widened** |
| MAGIC161 `0x801EF1DC` | `0x1CA990` | two G4 0..319 | `TimedBlow_DrawFlash` `0x4EE580` | 320 | **widened** |
| MAGIC223 `0x801EF1E8` | `0x1D94F4` | G4 0..319 by facing | `TempestFlash_Draw` `0x4F88B0` | 320 | **widened** |
| AREA085 `0x801F2ED8` | `0x35914` | semi TILE, page `0x4F`, shade-tex, slot 5 | `EffectKind46_DrawFlash` `0x477D20` (its neighbour `0x801F30E0` is `0x477E30`) | **already wide** | - |
| AREA175 `0x801F3438` | `0x2FE48` | the same, slot 2 | `EffectKind38_DrawTint` `0x473F10` | **already wide** | - |
| AREA060 `0x801F2C90` | `0x68534` | the haze band, strips, 320 -> 352 | `EffectKind18Sub15_Draw` `0x502E60` (the next function after `0x801F2C48` = `EffectKind18Sub15_Start` `0x502E30`) | **already wide** (2026-10-07) | - |
| AREA082 `0x801F5424` | `0x6E734` | G4 320, after `0x801F5388` = `0x507C40` | `EffectKind18Sub3F_DrawSky` `0x507CB0` | **already wide** | - |
| AREA172 `0x801F2DF4` | `0x5561C` | semi TILE page `0x3C0`, slot 5 | `Effect_DrawScreenTint` `0x48CA90` | **already wide** | - |
| AREA172 `0x801F4060` | `0x56938` | semi TILE page `(0x380, 0x100)`, slot 1 | `EffectKind98_DrawFlash` `0x48DBA0` (between the pair's anchors `0x48D3F0` / `0x48DC40`) | **already wide** | - |
| SCENA13 `0x801F6FF8`, `0x801F7484` | `0x45938` (one) | semi TILE page `0x3C0`, slot 5 | `Effect_DrawScreenTint` `0x48CA90` | **already wide** | - |
| SCENA15 `0x801F79C4` | `0x50F0C` | semi TILE page `0x3C0`, slot 3 | `EffectKind89_DrawTint` `0x489CD0` | **already wide** | - |
| SCENA01 `0x801F7DCC` | `0x60484` | opaque G4 0..319 x 0..239, page `0x95`, slot 7 | `Gfx_DrawSkyGradient` `0x4FD350` (area 23's sunset) | **already wide** | - |
| SCENA01 `0x801F7EDC` | `0x60560` | semi F4 320 x 240, slot 3, red the step / 8 | `Gfx_DrawSunsetGlow` `0x4FD3E0` | **already wide** | - |
| SCE15EF2 `0x801D19A8`, SCE15EF3 `0x801D1BAC` | `0x5CEF0` (one) | page `(0x380, 0x100)`, two G4 320 x 120, slot 7 | `EffectKindAA_DrawFill` `0x492400` (SCE15EF3's paired neighbour `0x801D10AC` is `0x492780`) | **already wide** | - |
| SCE15EF3 `0x801D23AC` | `0x5E344` | semi TILE 320 x 240, no draw mode, slot 1 | `EffectKindAF_DrawScreen` `0x4932E0` | **already wide** | - |
| SCE15EF2 `0x801D1414`, SCE15EF3 `0x801D2424` | `0x5D070` (one) | two opaque G4 0..320, red `0x80`, slot 7 | `EffectKindA8_DrawBar` `0x4920F0` | 320: **held by the owner** on 2026-10-04 | **widened** (its own commit; section 4) |
| COMMU02 `0x801D5740` | `0x11177C` | the title, a semi G4 band of 16 rows over 320, a black TILE 320 x (0xE0 - y) | `CommuDraw_Reveal` `0x45CC50` | 320 | **widened** (section 4) |
| BATTLE.EMI#15 `0x800ABF3C` | `0x1486FC` | opaque G4 0..319 x 0..239 by the owner's facing, `0x20` / `0x80`, slot 7 | **not found** | - | - |

`0x800ABF3C` has no twin: its PSX neighbours are `BattleActor_SetAnimation`
`0x4FB830` and `BattleActor_UpdateScreenXY` `0x4FBD10`, and the seven
functions between them on the PC are the spells' motion helpers; no PC
function holds a 319.0 that is not listed above, and none of the three
facing-sided spell flashes has its fixed `0x20` / `0x80` opaque shape. Either
the PC dropped it or it is built from registers; it is recorded, not
handled.

Also confirmed by unit on the way (placements' *by shape* rows): MAGIC020 is
`Magic020_DrawFade`, MAGIC004 / 008 / 021 share `Magic008_DrawFlash` (one PSP
function `0x15462C`; the PC's row lists 14 files).

## 3. What changed, site by site (wide picture only)

| Site | Was (narrow, and wide before) | Now in the wide picture | PSP | Check |
|---|---|---|---|---|
| `Magic008_DrawFlash` `0x4A29C0` (`magic_s06.cpp`) | two G4 x 0..319 | x -53..372 | -32..351 | `magic_s06` fuzz |
| `Magic111Wash_Draw` `0x4D6420`, `Magic111Flash_Draw` `0x4D6750` (`magic_c3.cpp`, their shared `FullScreen`) | 0..319 | -53..372 | -32..351 | `magic_c3` |
| `Magic225Veil_DrawShade` `0x4F9980` (`magic_s38.cpp`) | F4 0..319 | -53..372 | -32..351 | `magic_s38` |
| `SpellRagnarok_DrawScreenTint` `0x4D55C0` (`magic_s25.cpp`) | two G4 0..319 | -53..372 | (twin by unit) | `magic_s25` |
| `ChillFlash_Draw` `0x4B7240` (`magic_s15.cpp`) | G4 0..319, either side bright | -53..372 | -32..351 | `magic_s15` |
| `KaiserFlash_Draw` `0x4E59A0` (`magic_s30.cpp`) | F4 0..319 | -53..372 | -32..351 | `magic_s30` |
| `CoronaFlash_Draw` `0x4E7E80` (`magic_s31.cpp`) | G4 0..319 | -53..372 | -32..351 | `magic_s31` |
| `TimedBlow_DrawFlash` `0x4EE580` (`magic_s34.cpp`) | two G4 0..319 | -53..372 | -32..351 | `magic_s34` |
| `TempestFlash_Draw` `0x4F88B0` (`magic_s38.cpp`) | G4 0..319 | -53..372 | -32..351 | `magic_s38` |
| `EffectKind44_RingDraw` `0x476A00` (`effect_2c.cpp`) | each edge's run to x 0 or 320; two TILEs 0, 320 wide | runs to -53 or 373; TILEs -53, 426 wide | -32 / 352; -32, 384 | `effect_2c` |
| `BattleWin_DimScreen` `0x444660` (`battle_e4.cpp`) | TILE at 0, 960 wide | at -53, 960 wide (the 960 already passes the right edge) | -32 | `battle_e4` |
| `BattleWin_DrawTile` `0x444900`, `_DrawTileRgb` `0x4449E0` (`battle_window_draw.cpp`), `_DrawTileTint` `0x4446E0` (`battle_e4.cpp`) | size 11: (321, 241) at the caller's x (0 from all four of ours: `boss_h.cpp`, `boss_sg.cpp`, `rest_3a.cpp` x2) | size 11: width 427, an x of 0 at -53 (-53..374) | the entry 385 and the callers' x -32 | `battle_window_draw`, `battle_e4` |
| `EffectKind18Sub44_Draw` `0x510EB0` (`effect_6c.cpp`), area 189's sky | textured band (0, 90)..(320, 122), u 0..255; white tile (0, 50) 320 x 40; three 256-wide sky quads from `angle - 256`; four haze bands between 0 and 320 | the band gets a quad each side (x -53..0 with u 213..255, 320..373 with u 0..42 - the pattern continued at the middle's rate, not stretched); the tile -53, 426 wide; a fourth sky quad at `angle - 512` (the three reach only -1 at the largest angle); a flat haze quad each side at the outer rows | stretched the band to -32..352, the tile to 384, moved the three sky quads by -32 | `effect_6c` |
| `Fx105_MoteDraw` `0x4D2220` (`magic_s24.cpp`) | x kept in `[-64, 384]` | `[-117, 437]`; y as it was | `[-96, 416]` | `magic_s24` |
| `Area104_DrawGauge` `0x415940` (`area_w2e.cpp`) and `Area121_GaugeSprite` `0x41C350` (`area_w3b.cpp`) | the charge gauge's bars at x 228, its frame at the caller's 0xDC | both + 53 | bars 228 -> 260; the frame +32 **inside** the sprite function (its twin JP `0x244020`, `addiu x, x, 0x20` - the survey's immediates could not see it) | `area_w2e`, `area_w3b` |
| `Area104_DrawPanel` `0x4161F0`, `Area104_Kind6ACountdown` `0x415D50` (`area_w2e.cpp`) | the countdown's panel quad at 210 / 255 / 300, its four triangles from `Area104_PanelTris`, the leader's ring centred at 250 + the projection | all x + 53; the countdown's box and digits (0x7E, 0x84) stay centred | quad +32, the triangles' x +32, the ring's 250 -> 282; box and digits unchanged | `area_w2e` |
| `Area172_EffectA5Grow` `0x428240`, `_Hold` `0x428290` (`area_w4b.cpp`) | `Area172_DrawPanel(0xDC, 0xC8)` | x 0xDC + 53; y 0xC8 | (220, 200) -> (256, 190) | `area_w4b` |
| `Window_Kind0Open`, `Window_Grow`, `Window_Shrink`, `Window_Kind2Open` (`window_task.cpp`) | placement codes 3..6 read from three tables | entries 3 and 5 x - 53, 4 and 6 x + 53, in all three tables | the same entries -32 / +32 in all three (section 5) | `window_task` |
| `CommuDraw_Reveal` `0x45CC50` (`rest_4d.cpp`) | G4 band 0..320, TILE 0, 320 wide | band -53..373, TILE -53, 426 wide | TILE only (section 4) | `rest_4d` |
| `EffectKindA8_DrawBar` `0x4920F0` (`rest_3f.cpp`) | two G4 0..320 | -53..373 | -32..352 | `rest_3f` |

The message boxes at 53 columns: kind 0's wide box (200 wide) at x 16 / 104
goes to -37 / 157 (its left edge 16 columns inside the wide frame's, as the
original's is inside the 4:3 frame's), the narrow box (120 wide) at 16 / 184
to -37 / 237, and kind 2's box (71 wide) at 145 / 226 to 92 / 279 - the 92
and 279 the placements doc and the owner named.

**The SCENA17 ending keeps its letterbox** (the owner, 2026-10-12, 6.1 item
4): `Scena17_DrawLetterbox` `0x56CFF0` is not touched - its two black bars
stay 320 wide and the scene shows in the bands beside them, as DIV-0041 has
it; the PSP's crop of the letterboxed picture is not built. Decided, not
owed.

**Not now** (the owner): the tile ring (IDEAS I37) and the field menu's time
and money boxes (the owner checks on the PSP first). Not taken because the
PSP left them: effect kinds 0x76..0x7C's `(0, 0)..(320, 320)` quads
(`effect_3d.cpp` `FullScreen`) and `EffectKind96_Pulse` (`effect_4d.cpp`) -
no PSX twin of theirs is among the PSP's 47 fill functions.

## 4. Two calls this branch made that the owner may want back

- **`EffectKindA8_DrawBar` widened against the hold of 2026-10-04.** DIV-0041
  says the bars were "left as it is by the owner, 2026-10-04, to be looked at
  in game under the wide picture". The pairing above shows the PSP widened
  them, and 6.1's answer takes the PSP's fills. It is its own commit
  (`EffectKindA8_DrawBar: the bars over the wide frame`) so the coordinator
  can drop it.
- **`CommuDraw_Reveal`: both primitives, not the PSP's one.** The PSP moved
  the black tile to -32 / 384 and left the 16-row gradient band above it 320
  wide (JP `0x11177C`: the band's x still `0` / `0x140`). Taken literally that
  leaves a 16-row strip in each band unshaded between the scene and the black
  (inference from the semi-transparency: the band subtracts from black to
  white). Ours widens both, DIV-0041's fill rule.

## 5. Found on the way

- **The PSP framed the message box's own tables too.** The placements doc
  found the PSP's change in `MsgBox_PlacementTable` `0x66AE10` (kind 2's box)
  only, because its window match started at `0x66ADFC`. The tables kind 0 -
  the dialogue box itself - reads, `kBoxXY` `0x66ADD8` and `kBoxAltXY`
  `0x66ADE8` (eight `(u8 x, u8 y)` each on the PC), are on the PSP eight
  `(s16 x, s16 y)` each (JP file `0x2DF920` / `0x2DF93C`, EU `0x2DF110` /
  `0x2DF12C`; `boxtab2.py`, matched on the seven y values), and their entries
  3..6 are 32 further out: left x 16 -> -16, right 104 -> 136 and 184 -> 216.
  Ours moves those entries by the anchor where `window_task.cpp` reads them,
  as above. **For VF / the importer:** a PSP-only base whose `kBoxXY` /
  `kBoxAltXY` come from `BOOT.BIN` needs the same normalisation as
  `MsgBox_PlacementTable` (x + 32 left, x - 32 right, by entry), and the PC's
  u8 layout cannot hold the PSP's -16 at all.
- **The gauge's +32 is inside the sprite function** (section 3), not at its
  callers: a survey of placement immediates cannot see an `addiu` on an
  argument register. The placements doc's other "not found" answers rest on
  the same survey.
- **Every 319.0 / 320.0 in `.text` is in a function of ours** (section 2), so
  any further fill the owner's eye finds is a change to our source.

## 5.1 The owner's answers (2026-10-12, night)

- **`EffectKindA8_DrawBar` (the red bars) is widened.** The owner: "If psp
  widened the a8 bar, let's widen it too". This lifts the hold of 2026-10-04
  in DIV-0041. Commit `0d3bcd8b` stays.
- **`CommuDraw_Reveal`: WA's version stays** (both the gradient band and the
  black tile widened). The owner: "I assume some psp draws will leave 16 row
  bands since we are not cropping the same".
- **The rule this gives later widening work:** the PSP crops 12 rows top and
  bottom (384 x 216 of its 384 x 240 frame, `psp-widescreen.md` 2.4), and we
  do not crop. Where a PSP widening relied on that crop to hide an edge, ours
  widens over the full height, not only the PSP's extent.

## 6. What only captures can confirm (for the A/B/C screenshot harness)

Narrow is proved by the fuzzes; the wide picture is not seen. For each, the
middle 320 columns should equal the narrow capture of the same frame, and the
named element should reach (fills) or sit against (anchors) the 426-column
frame's edges:

1. **Spell flashes, in battle:** Bonebreak / ThundrStrike and the 14 files
   that share `Magic008_DrawFlash`; MAGIC111's wash and flash; Chill
   (MAGIC067); Ragnarok (MAGIC110); the MAGIC131 flash (Kaiser's row in
   `magic_rows.tsv`); Corona (MAGIC137); Timed Blow (MAGIC161); Tempest /
   Hurricane (MAGIC223); MAGIC225's veil; Kyrie's motes (MAGIC105) not
   popping at the band edges. Ability names from `analysis/magic_rows.tsv`;
   which character casts which is the owner's to say.
2. **The battle window's dim** (`BattleWin_DimScreen`, battle_e1's
   `0x42EE00`) - the left band dimmed with the rest.
3. **The size-11 tile:** the boss death flashes (`BossTorast_DeathFxFlash`,
   `BossMikba_DeathFlash`) and rest_3a's two flashes - the whole 426 white or
   tinted.
4. **Area 80's ring** (`EffectKind44_RingDraw`): the shade outside the ring
   reaching both bands.
5. **Area 189's sky** (`EffectKind18Sub44_Draw`): the textured band and the
   white strip continuous across columns 53 and 373 (no seam, no wider
   texels at the sides), sky texture over the left band at every camera
   angle, the haze bands flat at the outer rows.
6. **Areas 104 / 121, the charge gauge and the countdown:** the gauge frame
   and bars 53 further right, together; the countdown panel, its triangles
   and the pulsing ring 53 further right, the countdown digits still
   centred.
7. **Area 172's panel** (effect kind 0xA5): 53 further right, same row.
8. **Message boxes:** any dialogue at placement codes 3..6 (the attract
   sequence's lower-left box is one; `widescreen.md` 5 `wide_attract_cycle`)
   - the box 53 further toward its edge, the text inside it, and a kind 2
   choice box beside it moved with it.
9. **The community's reveal** (`CommuDraw_Reveal`, the COMMU02 screen): the
   black and the gradient band across all 426 columns.
10. **The SCE15EF bars** (`EffectKindA8_DrawBar`, scenario 15's effects; kind
    0xB0 draws them too): the red bars across all 426 columns.
11. **Unchanged, to confirm nothing moved:** the SCENA17 ending's letterbox
    (bars 320 wide, scene in the bands), the field menu's time and money
    boxes.

## 7. Checks run

| Check | Result |
|---|---|
| `cmake --preset i686 && cmake --build build` | exit 0; no new warning in a touched file (the one left, `magic_s06.cpp` `kF320` unused, was there before) |
| hosted, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, an ini-less copy of the launcher, `bof3x.exe` and the DLL, `--game` the PC install, `--no-config`, narrow, at the branch tip | game process exit 0; 1,073 self-test lines at `0 MISMATCHES`, none other; `inject: 10087 ours`; `self-test only: done`. Every touched module among them (`magic_s06`, `magic_c3`, `magic_s15`, `magic_s24`, `magic_s25`, `magic_s30`, `magic_s31`, `magic_s34`, `magic_s38`, `effect_2c`, `effect_6c`, `rest_3f`, `rest_4d`, `area_w2e`, `area_w3b`, `area_w4b`, `battle_e4`, `battle_window_draw`, `window_task`) |
| the same with `BOF3X_WIDE=1` | game process exit 0; 1,073 at `0 MISMATCHES`, none other; `DIV-0041 full-frame fills at (-53, 0) 426 x 240`; `inject: 10087 ours`; done |
| exe host (`bof3x.exe`) on a PSX disc-only scratch install - a US cache built here (`importer.py build --source "...(USA).cue" --lang en-US`) and `install --base`, `BOF3X_CACHE` = the cache, `BOF3X_LANG=en-US`, no shadow (hosted-only) - narrow and `BOF3X_WIDE=1` | both: game process exit 0, `inject: 10088 ours`, `self-test only: done`; wide logs the fills' line |
| `python tools/importer.py check` | exit 0 (the importer is not touched) |
| `python tools/ledger_check.py` | 113 entries, 0 errors, 2 notes (no draft of this doc is in the ledger yet, so none is owed to the coordinator's insert) |

## 8. What was not verified

- Nothing was run live; no capture was taken (section 6).
- Every wide branch runs only after the self-tests (that is what keeps the
  fuzzes on the original's values), so no fuzz exercises the widened values;
  they are read, not tested. The new prims in `EffectKind18Sub44_Draw`
  (three quads and two haze quads in the wide picture) go through the same
  pool test as the original's own (`EffectKind18Sub44_Commit`, and
  `Gfx_CommitPrim` for the band quads as the original's band).
- The pairings in section 2 are by unit and shape, not recorded pairs; the
  shapes matched on every compared feature, but the full functions were not
  read end to end against each other.
- Kind 0's side placements on the PSP were found as data (the tables), not
  traced through the PSP's reader.
- What any element is in play (a spell, a scene) is a reading of names and
  code, not something seen.

## 9. For the other files

**Folded in 2026-10-12, night** (the coordinator's second pass, branch `psp/ledger2`): A..E are one DIV-0041 amendment (E, the letterbox kept, inside it), with 5.1's answers: the A8 bars widened (the 2026-10-04 hold lifted), the reveal kept, the full-height rule; the 2026-09-23 "seem ok" reversal stated. README row, STATUS, HANDOFF (section 6's scenes as the follow-up round's harness pass) and `psp-widescreen.md` 3a rows 13..16 are in.

### `docs/DIVERGENCE.md`, DIV-0041 - amendments (DIV A..E of this group; folded into one DIV-0041 amendment, 2026-10-12)

**A - the full-frame fills the PSP widened (2026-10-12, PSP-only round group
WA, [`psp-widescreen-a.md`](psp-widescreen-a.md)).** The owner's answer to
`psp-widescreen-placements.md` 6.1 takes the PSP release's per-element
choices at our 53 columns. Fifteen more full-frame fills reach the wide
frame's edges: the five spell flashes drawn to x 319 that the `320.0` scan
could not see (`Magic008_DrawFlash`, `Magic111Wash_Draw`,
`Magic111Flash_Draw`, `Magic225Veil_DrawShade`,
`SpellRagnarok_DrawScreenTint`) and five more found by pairing the PSP's
unpaired fills (`ChillFlash_Draw`, `KaiserFlash_Draw`, `CoronaFlash_Draw`,
`TimedBlow_DrawFlash`, `TempestFlash_Draw`) - each `0..319` becoming
`-53..372` through the new `Widescreen_FillRight(319.0f)`, the PSP's
`-32..351`; `EffectKind44_RingDraw` (the runs to the sides and its two tiles,
the PSP's `-32` / `352` / `384`); `BattleWin_DimScreen` (x from
`Widescreen_FillX()`, the PSP's `-32`); `CommuDraw_Reveal` (its gradient band
and its black tile - the PSP widened the tile only); `EffectKindA8_DrawBar`
(the bars, `-53..373`, the PSP's `-32..352`; this ends the hold of
2026-10-04); and area 189's sky (`EffectKind18Sub44_Draw`: the textured band
continued by a quad each side at the middle's texel rate rather than
stretched as the PSP did, the white tile `(-53, 50)` 426 wide, a fourth sky
quad so the left band is covered at every camera angle, and a flat haze quad
each side). All read `Widescreen_Fill()`'s columns, so every fuzz compares
the original's and narrow is bit for bit Capcom's. Every `319.0f` / `320.0f`
operand in `.text` now lies in a function of ours.

**B - the battle window's full-frame tile.** Entry 11 of the size table
`0x64E268` ((321, 241)) is the full-frame tile the bosses' death flashes and
rest_3a's flashes draw at x 0; the PSP widened the entry to 385 and moved the
callers' x to -32. Ours applies that rule where the table is read
(`BattleWin_DrawTile`, `_DrawTileRgb`, `_DrawTileTint`, via
`Widescreen_BattleTile`): for size 11 the width + 2 columns and an x of 0 at
-columns, the table untouched.

**C - the mote cull.** `Fx105_MoteDraw`'s x bounds `[-64, 384]` become
`[-117, 437]` in the wide picture, keeping the original's margin past the
view as the PSP's `[-96, 416]` did for 32; the y bounds as they were.

**D - edge anchors.** Elements the PSP moved by 32 toward an edge move by 53
(`Widescreen_Anchor()`): the charge gauge of areas 104 / 121 (its bars in
`Area104_DrawGauge`, its frame in `Area121_GaugeSprite`), area 104's
countdown panel, its triangles and the leader's ring (the countdown's box and
digits stay centred, as on the PSP), area 172's panel (x only: the PSP's -10
rows served its crop); and the message boxes' side placements - codes 3 and
5 left, 4 and 6 right, in kind 0's wide and narrow tables (`0x66ADD8`,
`0x66ADE8`) and kind 2's `MsgBox_PlacementTable` `0x66AE10`, moved where
`window_task.cpp` reads them, never in the tables. This reverses the owner's
"the dialogue boxes seem ok to me" of 2026-09-23, at the owner's word of
2026-10-12.

**E - decided, not built: the ending's letterbox stays** (the owner,
2026-10-12): `Scena17_DrawLetterbox` keeps its 320-wide bars; the PSP's crop
of the letterboxed picture is not taken. (Fold into A's paragraph if the
coordinator prefers one amendment per change kind.)

**Reversible?** additions: unset `BOF3X_WIDE`; `BOF3X_ORIGINAL=<function>`
for any of the functions above brings back Capcom's (narrow-sized) draw.

### `docs/README.md`, one row

`psp-widescreen-a.md` | The PSP's widescreen choices built at our 53
columns (2026-10-12, group WA): fifteen more fills widened (the five 319
flashes, and of the 25 unpaired PSP fill functions 15 already wide, 9
widened, one with no PC twin), the size-11 battle tile at its readers, the mote cull,
the edge anchors (areas 104 / 121 gauge and countdown, area 172's panel, the
message boxes' side placements in all three tables); the ending's letterbox
kept; the PSP's dialogue-box tables found framed too; the scenes for the
screenshot harness. BUILT.

### `docs/STATUS.md` / `docs/HANDOFF.md`

- DIV-0041 amended by the PSP's choices (option (a)); wide pictures owed for
  the scenes of `psp-widescreen-a.md` 6.
- For the importer: `kBoxXY` / `kBoxAltXY` on a PSP-only base are framed for
  32 columns like `MsgBox_PlacementTable` (section 5).
- `EffectKindA8_DrawBar`'s hold of 2026-10-04 ended by this branch (its own
  commit) - the owner to confirm.

### `docs/psp-widescreen.md`

3a: add the five 319 flashes, the gauge's +32 inside `Area121_GaugeSprite`'s
twin, and the dialogue-box tables `kBoxXY` / `kBoxAltXY` (section 5).
