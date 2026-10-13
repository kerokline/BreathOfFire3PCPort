# The PSP's 16:9 placements, site by site, against our wide mode

**Status:** DRAFT (2026-10-12) - a reading by group WS for the owner's
question of 2026-10-12: *"since all the code is ours, maybe it's worth having
the psp placement of text / enemies / etc. as the widescreen placement?"*
Nothing in `src/` changed; no game was run. Every PSP fact is from the code
and data of `BOOT.BIN` (JP, EU where given) read against the sibling's PSX
captures with CD1's fixed pairing (`tools/psp_pair.py` at `8fdd1f36`). What
any of it looks like on a screen is an inference, marked as one; the owner's
eye on the handheld is the check.

Read with [`psp-widescreen.md`](psp-widescreen.md) (the PSP's frame: x + 32,
384 x 216 from row 12 at 480 x 272), [`widescreen.md`](widescreen.md) and
DIV-0041 (ours: 426 x 240, no crop, 53 columns a side, the same shift).

## 0. The answer

- **The PSP's widescreen is one rule applied in ~95 places, not a layout.**
  Every element Capcom touched falls in one of six kinds (section 2): the
  frame and its presentation; full-frame fills stretched to the wide edges;
  repeating backdrops given more pattern; the field's tile ring grown; three
  culls widened; and **eight** elements re-anchored to an edge by exactly
  32 px (plus two data tables). Everything else - title rows, the field
  menu's windows, the money and time boxes, the load menu's panel, the gene
  windows, every other slide-off bound, the two other `SetGeomOffset`
  callers - is unchanged on the PSP, i.e. left **centred** by the global
  +32 shift (section 3).
- **That is the rule DIV-0041 already follows.** Our shift by 53 is Capcom's
  shift by 32; of the sites below with an established PC twin, DIV-0041
  handles **19 the PSP's way** (13 fills with the transition fade, 2
  backdrops, 2 culls, the shop's 2 slide clamps), **one partly**, **the tile
  ring differently** (inset, not a bigger ring), and **leaves 14 unhandled**
  (section 4). About 30 PSP fill sites have no PC twin on record yet.
- **The open survey half-day is done:** a scan of every paired function's
  `ori` / `addiu` / `slti` / `sltiu` immediates for a PSX-to-PSP change of
  +-32 (+-31, +-33) or a 320 -> 384 kind of value (section 1) found no
  placement change that psp-widescreen.md and CD's three stage-B docs had not
  named, **except** the battle window's full-frame tile size (data, 321 ->
  385 wide) and the five magic flashes drawn to x 319 that DIV-0041's float
  scan for `320.0` could not see.
- **Proposal (section 5):** (a) take the PSP's per-element choices into our
  53-column mode by its own rule (stretch, anchor +-53, centre): about 14 PC
  functions and the message-box table's reader, in 12 files, all ours, as amendments to DIV-0041 - no new DIV;
  (b) a "PSP framing" option (32 columns, the 12-row crop, the PSP's values
  unscaled) is cheap on top of (a) once (a) reads the column count, but it is
  the crop the owner declined on 2026-09-23, so it is offered, not
  recommended; (c) finding D then splits cleanly: the code anchors by rule,
  and the PSP-only base's three widescreen-framed values are put back to
  their 4:3 values by rule at import, so 4:3 and both wide modes work from
  one normalised data set.

## 1. Method and what it measured

Scratch scripts in `<scratchpad>/psp_ws/` (not in the tree; they print
addresses, counts and small numbers only, never disc bytes into a doc):

| script | what it does |
|---|---|
| `survey.py` | for each of CD1's pairs (`psp_cd/pairs2.json`, the fixed tool's run), the immediates of `li` / `addiu` / `ori $zero` / `slti` / `sltiu`, plus the PSX's `addiu +K; sltiu M` range form as its two bounds; relocated PSP halves dropped; reports (PSX u, PSP v) one-sided pairs with v - u in +-31..33, or in a 320 -> 384 / 319 -> 351 / 28 -> 33 / 56 -> 58 table, and PSP-only -32 / 352 / 384 |
| `survey_ctl.py` | the same with another dx, as a noise control |
| `triage.py`, `ctx.py` | classes the 220 candidate functions; shows each side's lines round the values |
| `slides.py`, `combo.py`, `pspcombo.py` | every PSX function holding given values (the slide bounds, the title / load-menu sets) and what its PSP twin holds; PSP functions holding a set |
| `geom.py` | every PSP call of `SetGeomOffset` (JP `0x9d4ec`) with its arguments, and the twins |
| `final.py`, `pcq.py`, `symq.py` | PC twins: `analysis/pairs_propagated.json` (section + PSX -> PC) and `symbols.toml`'s `psx =`; ours or Capcom's |
| `tsprites.py`, `tsizes.py`, `datascan.py` | the PC's `Title_Sprites`, `MsgBox_PlacementTable` and the battle window's tile sizes located in `BOOT.BIN` by window match; every `(319..321, 239..241)` pair in the PC's data sections located in `BOOT.BIN` |

**The noise floor.** The survey's dx = +-32 hits 142 (PSX, PSP) pairs in 136
functions; the same survey at dx +-16 finds 73, +-24 58, +-40 56, +-48 69,
+-53 7. So about 75 of the 142 are signal, and the rest is the compilers'
folding (stack and scratchpad offsets, `16 -> -16` sign conventions, colour
arithmetic `0x60 - 3x` against `0x180 - 3x`, jump-table case values). Every
candidate was put in one of four buckets: 84 functions whose only hits are
the known noise pairs (`-16 / 16`, `48 / 16`, `64 / 32` and their mirrors),
62 with a full-frame value, 27 with the tile-grid values, and **47 read by
hand** at each value (`ctx.py`): 39 of the 47 are noise (read; e.g. START
`0x801de474`'s 120 / 88 is `(i + 11) * 8` folded, AREA047's 384 is
`0x180 - 3x` colour), 8 are placements, all already named by CD.

A y survey at dx +-12 (the crop's rows) finds 115 hits against 30 at +-10
and 40 at +-14, but 88 of the 115 are packet lengths (`28 / 16`, `20 / 32`,
`36 / 24`: the PSX's 12-byte packet headers against the PSP's 16); the 20
singletons left were not read. The only y move found is AREA172's panel
(section 2.6).

## 2. The sites, by kind

PSX addresses are the sibling's (overlay file as captured); PSP addresses
are `BOOT.BIN` link-time offsets, JP then EU (`-`: no EU mapping in CD1's
diff). "Ours" is `impl =` in `symbols.toml`. PC twins come from
`analysis/pairs_propagated.json` unless marked *by shape* (my reading of
the PC function's evidence line against the PSP change; a hypothesis until
a pair is recorded).

### 2.1 The frame (1 class)

| What | PSP | DIV-0041 |
|---|---|---|
| every primitive's x + 32 at conversion, 21 converters, 51 sites | `0xa0264` and 50 more | **same rule**, by 53, in the scene vertex shader |
| presentation `(0, 12, 384, 216)` -> 480 x 272, uniform 1.25x | `0xa349c`, `0xa2198` | **different by the owner's choice**: 426 x 240, no crop |
| `(32, 30, 320, 180)` -> 480 x 272 (a crop of the 4:3 view to 16:9) | the boot shell; **and SCENA17** (2.7) | not used |

### 2.2 Full-frame fills stretched to the wide edges: 53 PSX functions, 47 PSP functions

The PSP writes `-32` for the left edge and `352` / `384` (or `351` / `383`
for a `319`) for the right, beside `240`; for the battle window's tile it
changes the size table instead (2.8). **Element:** a fade, flash, tint,
dimming or sky gradient over the whole picture (inference from the shape;
the reading of each PC twin says which).

| PSX | PSP JP / EU | PC twin | DIV-0041 |
|---|---|---|---|
| boot `0x8014f5c0` | `6d64` / `76d8` | `Transition_DrawTile` `0x495750` ours, *by reading*: the PSX tile 320 x 240, colour `level >> 7`, `level += step`, the sign returned - the PC's evidence line term for term | **same** (`mode_flow.cpp`) |
| boot `0x80159108` | `115ec` / `11480` | `AreaMap_DrawBackdrop` `0x571BE0` ours | **same** |
| AREA030 `0x801e1734` | `e3cc0` / `e39d0` | `FieldPanel_DrawShade` `0x52D5C0` ours | **same** |
| SHOP `0x801df800`, `0x801d6ea4` (one PSP function) | `1f370c` / `1f33f8` | `Menu_DrawBlackScreen` `0x580970` ours | **same** |
| AREA082 `0x801f52d8` | `6e5d8` / `6e30c` | `EffectKind18Sub3F_WhiteOut` `0x507BC0` ours | **same** |
| SCENA17 `0x801f8914` | `2b8214` / `2b8064` | `Scena17_DrawFade` `0x56CBA0` ours | **same** |
| BATL_OVR `0x801ef318` | `14ac6c` / `14aa2c` | `BattleLoss_DrawBlack` `0x432930` ours | **same** |
| AREA189 `0x801f3514` | `7d188` / `7cec0` | `EffectKind18Sub44_Follow` `0x510C90` ours | **same** (its tile `0x510E6C`) |
| MAGIC020 `0x801efd88` | `15f6bc` / `15f61c` | `Magic020_DrawFade` `0x4A3A70` ours, *by shape* (a 320 x 240 TILE) | **same** |
| AREA121 `0x801f6390` | `24fab0` / `24f8ec` | `Area121_DrawTopBand` `0x41C790` ours, *by shape* (POLY_G4 0..320) | **same** |
| AREA132 `0x801f3cb0` | `251e04` / `251c40` | `Area132_EffectGradient` `0x41D620` ours, *by shape* | **same** |
| AREA145 `0x801f532c` | `2585bc` / `2583f8` | `Area145_DrawGradient` `0x421F10` ours, *by shape* | **same** |
| AREA172 `0x801f4060` or `0x801f2df4` | `56938` / `56728`, `5561c` / `5540c` | one of them `Area172_DrawShade` `0x428370` ours, *by shape*; the other not established | **same** for the one |
| AREA189 `0x801f3800` | `7d3ec` / `7d124` | `EffectKind18Sub44_Draw` `0x510EB0` ours | **partly**: its 320-wide band and TILE (`0x510F12`, `0x510FE5`) are DIV-0041's "found, not widened"; the PSP widened both and moved a `-256` to `-288` |
| MAGIC004, MAGIC008, MAGIC021 (`0x801ef6e0`, `0x801f0bbc`, `0x801ef348`; one PSP function) | `15462c` / `15458c` | `Magic008_DrawFlash` `0x4A29C0` ours, *by shape* (x 0 / 319; called from MAGIC004 and MAGIC021 code on the PC too) | **not handled**: drawn to 319, which the `320.0` scan missed |
| MAGIC111 `0x801ef330`, `0x801ef970` | `1a8f10` / `1a8c5c`, `1a8e30` / `1a8b7c` | `Magic111Wash_Draw` `0x4D6420`, `Magic111Flash_Draw` `0x4D6750` ours, *by shape* (0..319) | **not handled** |
| MAGIC225 `0x801efca0` | `1da76c` / `1da4c4` | `Magic225Veil_DrawShade` `0x4F9980` ours (0 / 319) | **not handled** |
| AREA080 `0x801f31a0` | `3429c` / `340c0` | `EffectKind44_RingDraw` `0x476A00` ours (edges to x 0 or 320, TILEs 320 wide) | **not handled** |
| BATTLE.EMI#3 `0x801d98d0` | `1226e8` / `122368` | `BattleWin_DimScreen` `0x444660` ours | **not handled on the left**: the PC already draws it 960 wide from x 0, so the right band is dimmed and the left is not; the PSP starts it at -32 |
| BOSS008 `0x800c19c0`, BOSS034 `0x800c199c`; BATTLE.EMI#3 `0x801e63f4`, `0x801e6538`, `0x801e6900`, `0x801e6994` | `1e0f84`, `1e920c`; `1449e0`, `144b14`, `144eb0`, `144f2c` | `BossTorast_DeathFxFlash` `0x438F40`, `BossMikba_DeathFlash` `0x43DCC0` ours (the four BATTLE.EMI#3 callers: not established); all call `BattleWin_DrawTile(Rgb)` with size 11 | **not handled**: the PSP passes x -32 and widened size 11 (2.8) |
| SpellRagnarok's tint | - | `SpellRagnarok_DrawScreenTint` `0x4D55C0` ours draws 0..319 | **not handled**; its PSX twin is not among the 53 by pairing (MAGIC067 / 110 / 131 / 137 / 161 / 223 are candidates) |
| the rest: AREA060 `0x801f2c90`, AREA085 `0x801f2ed8`, AREA175 `0x801f3438`, AREA082 `0x801f5424`, COMMU02 `0x801d5740`, SCENA01 x2, SCENA13 x2 (one PSP function), SCENA15, SCE15EF2 / SCE15EF3 x5 (three PSP functions), MAGIC060, 067, 110, 131, 137, 161, 223, BATTLE.EMI#15 `0x800abf3c` | in `sites.json` | **not established** | unknown: DIV-0041 widened about twenty fills whose PSX twins are not recorded (`EffectKind38_DrawTint`, `EffectKind46_DrawFlash`, `IdentifyDim_Draw`, `Gfx_DrawSkyGradient`, ...), so an unknown share of these is already handled |

### 2.3 Repeating backdrops given more pattern (2 PC functions)

| PSX | PSP | PC | PSP change | DIV-0041 |
|---|---|---|---|---|
| SHOP `0x801dbcbc` (and copies BATE `0x801d6200`, SHISU `0x801d71c0`, START `0x801df09c`) | `1f65a0` / `1f62d0` and three | `Menu_DrawBackdrop` `0x575690` ours | the column loop starts at -32 | **same** (two more column pairs from -0x40) |
| AREA030#5 `0x800f5e5c` | `e80e0` / `e7e00` | `ItemTrade_DrawBackground` `0x5942C0` ours (pairs file, call-anchored) | two 160-wide quads from 0 -> two 192-wide from -32 (field-1 row 264); whether its u range grows or stretches was not read | **same idea**: the tile pattern continued, texels = columns |

### 2.4 The field's tile ring grown: 23 functions, all ours on the PC

The PSP grows the view ring from 28 columns x 56 rows (row stride 112 bytes)
to 33 x 58 (132): loop bounds 27 / 28 / 55 / 56 -> 32 / 33 / 57 / 58
(`survey.py`, CD's field-1 rows 53, 58, 59, 64, 66, 67, 75, 117, 150, 211..213,
field-2 row 478, battle rows 29, 30, 87). The same grid code is copied into
MAGIC064 / MAGIC102 (the battle field's quake) and AREA040.

| PSX (boot unless named) | PC |
|---|---|
| `0x80153758`, `0x80153858`, `0x80153950`, `0x80153a6c` | `MapView_ShiftColumnPrev` / `Next`, `MapView_ShiftRowsPrev` / `Next` `0x56E9A0..0x56EB50` |
| `0x80153b8c` (also the terrain cull, 2.5) | `MapView_Build` `0x56EC00` |
| `0x80154780`, `0x801548a4`, `0x80155154` | `DrawLayers_Reset` `0x56F5B0`, `Field_ViewReset` `0x56F670`, `MapView_FillCells` `0x56FCA0` |
| `0x80155b38`, `0x80155cd4`, `0x80155e74`, `0x801563ac`, `0x801564c4`, `0x801595e0` | `MoveCmd_TestFB` / `FC`, `AreaMap_ClearCell`, `MapView_ItemAt`, `MapView_LinkPrimAt`, `MapView_PlaceRuns` |
| `0x8014af98`, `0x8014bb64`, `0x8014d184`, `0x801565fc` | `Packet_FrameReset`, `Sprite_DrawPass` `0x593060`, `Sprite_UpdateScreen` `0x588F20`; `0x801565fc` not established |
| MAGIC064 `0x801f103c`, MAGIC102 `0x801eec80` / `0x801efd7c`, BATTLE.EMI#15 `0x800ac020`, AREA040 `0x801f30b8` | `Field_ViewReset` (the pairs file names it for MAGIC064's copy), `Quake_Start` `0x4CE140` / `Quake_End` `0x4CEA20`, `Area40_TileLit` `0x405BF0`; `0x800ac020` not established |

**DIV-0041: handled differently.** Ours keeps the 28-column ring and lowers
the camera's cell inset by 3 columns a side (`Widescreen_InsetColumns`,
2026-10-07), with the terrain cull opened to `[-150, 470]` and the draw-item
pool doubled (DIV-0062). That reaches as far as the PSP's 33 columns only
while the inset is at least 3; at inset 0 the 28 columns are the limit, and
the PSP's answer - a bigger ring - is the one that has no such floor.

### 2.5 Culls widened (3)

| Cull | PSX / PSP | PC | PSP | DIV-0041 |
|---|---|---|---|---|
| terrain x | `0x80153b8c` / `d998` | `MapView_Build` ours | `[-50, 370]` -> `[-96, 416]` | **same**, wider: `[-150, 470]` |
| area-map frame pass, two x ranges | AREA189 `0x801f2de0` / `7cbe0` / `7c918` | `AreaMapBD_BuildView` `0x510780` ours (by structure, psp-widescreen 3a #3) | `+31` a side | **same rule**, `+52` a side |
| a mote sprite, x | MAGIC105 `0x801efcf0` / `1a3340` / `1a308c` | `Fx105_MoteDraw` `0x4D2220` ours, *by shape* (`-0x40..0x180` x, `-0x40..0x130` y; `magic_s24.cpp`) | `[-64, 384]` -> `[-96, 416]`, y unchanged | **not handled**: 11 px of margin past our 373 |

### 2.6 Elements re-anchored to an edge (8 PSX functions) - the placements proper

| PSX | PSP JP / EU | PC | PSP change | anchoring | DIV-0041 |
|---|---|---|---|---|---|
| SHOP `0x801e3b70` | `1ff1ac` / `1fef20` | `MenuSlide_LeftOff170` `0x596920` ours (by the bound; the PC merged every overlay's copy of this state into one) | slide-off clamp -170 -> -202 | left edge | **same**, -223; ours moves every user of the merged state (the PSP kept GAME.EMI's `0x8019a5f8`, BATTLE.EMI#15's three and COMMU04's / BATE's at -170) |
| SHOP `0x801e4130` | `1ff474` / `1ff1e8` | `MasterWin_SlideOut` `0x59C130` ours | -120 -> -152 | left edge | **same**, -173 |
| AREA104 `0x801f676c` | `245a9c` / `2458d8` | the Area104 HUD family (`Area104_DrawFrame` / `_DrawHud` / `_DrawSprite`, `area_w2e.cpp`) ours, *by name*; function-level pair not made | sprite x 300 / 210 / 255 -> 332 / 242 / 287, and a table's x + 32 at load | right edge | **not handled** |
| AREA104 `0x801f6360` | `24486c` / `2446a8` | as above | centre x 250 -> 282 | right edge | **not handled** |
| AREA121 `0x801f595c`, AREA104 `0x801f5418` (one PSP function) | `243f04` / `243d38` | `Area104_DrawGauge` `0x415940` ours, *by shape* (x 228; the PC calls it from area 121's code too) | x 228 -> 260 | right edge | **not handled** |
| AREA172 `0x801f4de0`, `0x801f4e68` | `262594` / `2623d0`, `26260c` / `262448` | `Area172_EffectA5Grow` `0x428240` / `_Hold` `0x428290` ours, *by shape*: both call `Area172_DrawPanel(0xDC, 0xC8)`, a 64 x 32 quad | (220, 200) -> (256, 190) | neither: +36 x and **-10 y**, which keeps the panel inside the 216-row window (inference) | **not handled**; the only y move found |

Plus the message box's side placements, which are data:

| Data | PSP | PC | PSP change | DIV-0041 |
|---|---|---|---|---|
| `MsgBox_PlacementTable` entries 3..6, x | JP `.data` file `0x2df970` (EU `0x2df0e0`) | `0x66AE10`, read by `Window_*` (`window_task.cpp`) ours | 145 / 226 -> 113 / 258 (`tsprites.py`: the 6 differing bytes are exactly those four x halfwords) | **not handled**: left as it is by the owner on 2026-09-23 ("the dialogue boxes seem ok to me"); ours would be 92 / 279 |

[`psp-image.md`](psp-image.md) 1 reads the table's six differing bytes as
"two box positions, each x 32 less and y 32 more"; the bytes are four x
values, two -32 and two +32 (`226 = 0x00E2` -> `258 = 0x0102` changes two
bytes each). The y values are unchanged.

### 2.7 A letterbox turned into a crop (SCENA17)

| PSX | PSP | PC | PSP change | DIV-0041 |
|---|---|---|---|---|
| SCENA17 `0x801f8f38` | `2b83c0` / `2b8210` | `Scena17_DrawLetterbox` `0x56CFF0` ours | the PSX's two black tiles `(0, 0, 320, 30)` and `(0, 210, 320, 30)` are not drawn; the PSP calls `0xa24cc(32, 30, 320, 180)` instead - the presentation rectangle of psp-widescreen 2.4 - so the letterboxed 4:3 picture's middle 180 rows fill the 480 x 272 screen at 1.5x (inference from the rectangle's use in 2.4) | **not handled**: the bars are 320 wide and the bands beside them show the scene |
| SCENA17 `0x801f8204` | `2b7b58` / `2b79a8` | `Scena17_Outro` `0x56C750` ours | puts the rectangle back to `(0, 12, 384, 216)` | - |

Not a placement: SCENA17's `@` graphic is 88 px on the PSX and 105 on the
PSP (`Scena17_DrawLine`, field-1 rows 108 / 253): new art, CD's content row.

### 2.8 Data: the battle window's full-frame tile size

`BattleWin_DrawTile` / `_DrawTileRgb` (`0x444900` / `0x4449E0`, ours) take
their w, h from the 4-byte size table at `0x64E268`. Entry 11 is
`(321, 241)` on the PC; in `BOOT.BIN` JP (file `0x2f0394`, located by its
neighbours, `tsizes.py`) it is **`(385, 241)`**, the only differing byte of
the table. With the callers' x moved from 0 to -32 (2.2's BOSS008 / BOSS034
/ BATTLE.EMI#3 rows) that is the frame's -32..353. A scan of the PC's data
sections for every `(319..321, 239..241)` pair finds this one only
(`datascan.py`). `Title_Sprites` (IM's other "placed differ") differs in
u / v / w / h, not in screen x: texture layout, not placement.

## 3. Checked and unchanged on the PSP (centred by the shift)

| Element | PSX / PSP | PC | evidence |
|---|---|---|---|
| field menu windows: the four records at (0xC8, 0x10), (0x10, 0x10), (0x68, 0x2A), (0x73, 0x10) - time, money, tabs (inference from widescreen.md 5) | START `0x801d2010` / `af264` | `FieldMenu_PlaceWindows` `0x589E60` ours | diff `minor` (globals only); `combo.py`: 200 / 104 / 115 present, none +-32 |
| the load menu's panel `(0x14, 0x12, 0x118, 0x13)` and title `(0x1C, 0x15)` | 19 SHOP / START functions holding 20, 280, 28 | `LoadMenu_Open` `0x588190` ours | every paired one holds the same three on the PSP (`combo.py`) |
| the title rows `x = 0xA0 - w / 2` | - | `TitleMenu_DrawRow` ours | centred by construction |
| the title frame pieces `(0x1A, 0x18)`, `(0x10A, 0x88)`, `(-6, 0x1C)`, `(0xDA, 0x1C)` | START `0x801e7588`, **unpaired** | `TitleMenu_DrawFrame` `0x588C00` ours | no PSP function holds 266 with 218 or 26, as is or +-32 (`pspcombo.py`): **not measured** - the PSP's title may be rebuilt |
| gene windows' slide bounds 322, 347, 323, -165, -190 | BATTLE.EMI#15 `0x800a1714`, `0x800a1bd8`, `0x800a2020`, `0x800a2980`, `0x800a2d48`, `0x800a2f94`, `0x800ad338` | `GeneWin_*SlideOut` ours | unchanged (`slides.py`); DIV-0041 moves them by 53 anyway |
| other slide bounds -170, -100, -150, -200 | GAME, BATTLE, SHOP, START, SHISU, COMMU04, BATE | the merged `MenuSlide_*`, `MenuList_*` | unchanged except SHOP's two (2.6) |
| `Encounter_OnScreen` `[-40, 360]` | GAME `0x801c805c` / `1431e4` | `0x5928F0` ours | `same` |
| `Menu_DrawCursorBox` | SHOP `0x801d8e08` | `0x573CE0` ours | `same` |
| **the "other two `SetGeomOffset` callers"** | AREA060 `0x801f2c48` / `68470` passes (160, 144); AREA189 `0x801f4de0` / `26b1d0` passes (160, 185) with `SetGeomScreen(300)` | `EffectKind18Sub15_Start` `0x502E30` (effect_5c.cpp), `Area189_LeaderStart` `0x42A8D0` (area_w4e.cpp), both ours, both `(0xA0, ...)` | **the PSX twins pass the same** (`geom.py`, `sbs.py`): psp-widescreen 2.2's "code the PSX boot EXE does not contain" is overlay code, unchanged, and x 160 is centred. Nothing to do; widescreen.md 5's open item closes |
| 3b's culls and ranges | - | - | psp-widescreen 3b |

## 4. Counts

| Kind | PSX functions | PC twin established | DIV-0041 the PSP's way | partly / differently | not handled |
|---|---|---|---|---|---|
| frame | - | - | shift: same | presentation: no crop (owner) | - |
| full-frame fills | 53 (47 PSP) | 22 PC functions | 13 | 1 (`EffectKind18Sub44_Draw`) | 8: 7 functions and the size-11 tile path (2.8) |
| backdrops | 5 | 2 | 2 | - | - |
| tile ring | 23 | 21 | - | all (inset) | - |
| culls | 3 | 3 | 2 | - | 1 (`Fx105_MoteDraw`) |
| edge anchors | 8 + `MsgBox_PlacementTable` | 6 PC functions + the table | 2 (shop clamps) | - | 4 PC functions + the table |
| letterbox -> crop | 1 | 1 | - | - | 1 |
| data | 1 table entry | 1 | - | - | (the size-11 tile, counted under fills) |

"The PSP's way" counts **19** PC sites (13 fills incl. `Transition_DrawTile`,
2 backdrops with `Menu_DrawBackdrop`'s four PSX copies counted once, 2
culls, 2 clamps). **14** are not handled: 8 fill functions (`Magic008_DrawFlash`,
`Magic111Wash_Draw`, `Magic111Flash_Draw`, `Magic225Veil_DrawShade`,
`SpellRagnarok_DrawScreenTint`, `EffectKind44_RingDraw`,
`BattleWin_DimScreen`, the size-11 tile path), `Fx105_MoteDraw`, the
Area104 / 121 HUD (2 functions), the Area172 panel (2 call sites, one
function), `Scena17_DrawLetterbox`, and `MsgBox_PlacementTable`.

## 5. The proposal, for the owner

### (a) The PSP's placements in our 53-column mode (recommended)

One rule per kind, each scaled from 32 to 53:

| Kind | Rule | Sites | Files |
|---|---|---|---|
| fill | stretch: `(Widescreen_FillX(), 0)` over `Widescreen_FillWidth()`, as the 13 already are; a 319 edge goes to `319 + Fill()` | `Magic008_DrawFlash`, `Magic111Wash_Draw`, `Magic111Flash_Draw`, `Magic225Veil_DrawShade`, `SpellRagnarok_DrawScreenTint`, `EffectKind44_RingDraw` (its edges and TILEs), `BattleWin_DimScreen` (x from `FillX()`), `EffectKind18Sub44_Draw`'s band and TILE | `magic_s06.cpp`, `magic_c3.cpp`, `magic_s38.cpp`, `magic_s25.cpp`, `effect_2c.cpp`, `battle_e4.cpp`, `effect_6c.cpp` |
| the size-11 tile | x `FillX()`, w `321 + 2 Fill()` when the size index is 11 and x is 0 (the PSP's two changes, by rule; no table copied) | `BattleWin_DrawTile` / `_DrawTileRgb` | `battle_window_draw.cpp` |
| cull | keep the margin past the view: `[-64 - c, 384 + c]` | `Fx105_MoteDraw` | `magic_s24.cpp` |
| right anchor | `x + Widescreen_Live()` (+53) for each value the PSP moved +32 | the Area104 / 121 HUD (`Area104_DrawGauge` and the family that draws 300 / 210 / 255 / 250) | `area_w2e.cpp`, `area_w3b.cpp` |
| the Area172 panel | the PSP's +36 is not an anchor; centre-preserving would be "nothing". Offer: leave (ours has no crop, so the -10 y is not needed) | `Area172_EffectA5Grow` / `_Hold` | `area_w4b.cpp` (only if the owner wants it) |
| message box sides | left / right anchor: entries 3..6 x -53 / +53 (92 / 279), applied where `Window_*` reads the table, never by editing the table | `window_task.cpp` | owner's call: reverses "seem ok" of 2026-09-23 |
| ending letterbox | either widen the bars to `FillX()` / `FillWidth()` (cheap; the bands go black top and bottom like the middle), or the PSP's crop: present the view's rows 30..210 at 240 / 180 = 1.33x, which fills 426 x 240 almost exactly (320 x 1.33 = 427) | `Scena17_DrawLetterbox`, the backend's present | `scena_sc15.cpp`; the crop also `render_d3d11.cpp` |
| tile ring | optional: grow the ring as the PSP did (28 -> 34 columns for 53 px at ~20 px a column, rows 56 -> 58), replacing the inset trick | 21 `MapView_*` / `Field_*` / `Quake_*` functions, `DrawItems` (DIV-0062 already doubled) | `map_layers.cpp`, `map_scroll.cpp`, `draw_emit.cpp`, `magic_s23.cpp`, `area_w1*.cpp` - the costliest item; only if the inset's floor shows in play |

**Cost:** about 14 PC functions and the table's reader in 12 files for everything but the ring (about a
day: each change is a value under `Widescreen_Fill()` / `_Live()`, fuzzed
against the original's with the fill unarmed, the way the 13 were); the ring
is a separate half-week. **Ledger:** amendments to DIV-0041, one per kind
(fills, the size-11 tile, the mote cull, the HUD anchors, and the message
box if taken). No new DIV: the rule is DIV-0041's own. The ending's crop
would want its own amendment, since it changes what part of the 4:3 picture
is shown. **Before any of it:** the ~30 fills of 2.2 with no PC twin get
paired (the pairing tool against our `symbols.toml`'s area / scenario /
magic functions), because some are already widened and some may not be.

### (b) The PSP's exact framing, as an option

A third value of the "Widescreen" box: 32 columns a side and the 12-row
crop, i.e. a 384 x 216 logical window at one scale.

- **What it takes:** `Widescreen_Columns()` returns 32 for the new value
  (every `Widescreen_Fill()` / `_Live()` user, the culls and the slide
  bounds follow, since they already derive from the column count); the
  backend's present takes rows 12..228 of the 384 x 240 target
  (`render_d3d11.cpp`, `display_setup.cpp`: the target height stays 240k,
  the presented rectangle 384k x 216k); the launcher grows a three-way
  choice (`src/launcher/`). With (a) done, the PSP's values come out
  exactly (32 for 32), with no table of the PSP's numbers in our code.
- **What it changes for the player (inference):** anything in rows 0..11
  or 228..239 is off screen - the PSP moved one element for that (the
  Area172 panel, y -10); whether any HUD of ours sits in those rows is
  unmeasured (psp-widescreen 5 has it open too). The ending's letterbox
  then is exactly the PSP's.
- **Cost:** about a day on top of (a); one amendment to DIV-0041 (a second
  framing) or a new DIV for the crop. **It is the crop the owner declined**
  ("I don't think I want to crop", 2026-09-23), so this is offered for a
  PSP-faithful look, not recommended.

### (c) Finding D: a PSP-only base's data is framed for widescreen

Measured from the code side here (VF has the data side): the PSP's own
widescreen values that live **in data** rather than code are three -
`MsgBox_PlacementTable` entries 3..6 (x +-32), the battle tile size entry
11 (321 -> 385), and the presentation / letterbox rectangles that are code
on the PC. Everything else of section 2 is code, which on a PSP-only base
is still ours, so it does what our mode says regardless of where the data
came from.

- **Under (a) or (b) in wide mode,** the data and the code agree only if
  the code re-derives the placement from the 4:3 value: a PSP-only base
  hands us 113 / 258 and 385, which are right for 32 columns and wrong for
  53 (and would be moved twice if our wide rule were applied on top).
- **So the rule is: normalise at import, anchor in code.** The importer
  puts the PSP's three values back to their 4:3 values by the same rule the
  PSP used (`x + 32` for the left placements, `x - 32` for the right, `w -
  64` for the tile; derived, not copied - the owner's stance of
  2026-10-10), the base is then framed for 4:3 like a PSX or PC base, and
  the code applies 0, 53 or 32 columns. 4:3 needs nothing more; without
  the normalisation a PSP-only 4:3 game would show the side boxes 32 px
  toward the centre's opposite edges (left box 32 px further left, right
  box 32 px further right; inference) and an over-wide battle tile (harmless
  overdraw).
- The rule belongs beside the importer's other PSP transforms; it is one
  small module and one call site, and its ledger entry is the importer's
  (a data transform, not a behaviour change).

## 6. Questions for the owner

1. **(a), (b), or both?** (a) is recommended; (b) is the crop declined on
   2026-09-23 - still no?
2. **The message box's side placements:** take the PSP's anchoring (92 / 279
   under 53 columns), or keep them where they are?
3. **The Area104 / 121 HUD** (a gauge and frame at the right, x 228..332 on
   the PSX; what they are in play is yours to say): anchor them to the right
   edge as the PSP did?
4. **The ending (SCENA17):** wide black bars, or the PSP's crop of the
   letterboxed picture to fill the screen?
5. **The tile ring:** keep the inset approach unless a notch shows again, or
   grow the ring as the PSP did?
6. **The field menu's time and money boxes** stay centred-relative on the
   PSP too (section 3), so ours already matches it - confirm nothing to do.

### 6.1 Answered by the owner (2026-10-12, evening)

1. **(a) only.** The crop is still no, so there is no "PSP framing" setting
   and (b) is not built.
2. **The message-box side placements move in wide mode** (to 92 and 279 at
   53 columns, by the PSP's rule). 4:3 is unchanged.
3. **The Area104 / Area121 HUD is anchored to the right edge** in wide mode.
4. **The SCENA17 ending keeps the letterbox** (wide black bars), not the
   PSP's crop and zoom.
5. **Growing the tile ring** (28x56 to 33x58, as the PSP did) is wanted, but
   later: IDEAS I37. The inset approach stays for now.
6. **The time and money boxes:** the owner will check on the PSP. They stay
   as they are until then.

**Built** (2026-10-12, group WA): [`psp-widescreen-a.md`](psp-widescreen-a.md) -
(a) at 53 columns, the 25 unpaired fill functions of 2.2 paired (15 already
wide, 9 widened, one with no PC twin found), and two corrections to this reading: the
PSP framed the dialogue box's own tables `0x66ADD8` / `0x66ADE8` too (2.6
lists only `MsgBox_PlacementTable`), and the gauge's frame moves inside its
sprite function. The ending's letterbox (2.7) stays, as answered.

## 7. What was not verified

- Nothing was seen in play, on the PSP or on ours. Every "what it is" and
  "where it is on screen" above is an inference from code.
- PC twins marked *by shape* or *by name* are readings of evidence lines,
  not recorded pairs. About 30 PSP fill sites have no PC twin.
- The survey reads only immediates. A placement computed from a table, a
  register or a float load (as the PC's are) with no immediate changed
  would not show; the data scan covered the `(320, 240)` pair only.
- The 47 hand-read candidates were read at their values, not end to end;
  the 84 pattern-noise functions were classed by their hit pairs alone.
- The title frame's PSP twin was not found; the title's layout on the PSP is
  unmeasured.
- EU addresses come from CD1's diff where it mapped one; the battle tile
  table and `Title_Sprites` were located on JP only.
- The y survey's 20 singletons were not read.

## 8. For the other files

**Folded in 2026-10-12, night** (the coordinator's second pass, branch `psp/ledger2`): The README row, `psp-widescreen.md` 2.2 / 3a / 5, `widescreen.md` 5 and `psp-image.md` 1 are in; (a) became DIV-0041's amendment (WA), (c) DIV-0103's (VF); (b) not taken.

- **`docs/README.md`, one row:** `psp-widescreen-placements.md` | The PSP's
  16:9 placements site by site against DIV-0041 (2026-10-12, group WS):
  ~95 sites in six kinds; 19 already handled the PSP's way, the ring
  differently, 14 not; proposal (a) scale to 53 columns, (b) the PSP's
  framing as an option, (c) normalise the PSP-only base's three framed
  values at import.
- **`docs/psp-widescreen.md`:** 2.2 - the two further `SetGeomOffset`
  callers are AREA060 (160, 144) and AREA189 (160, 185) overlay twins whose
  PSX code passes the same; 3a - add the battle tile size entry 11 (321 ->
  385) and the MAGIC004 / 008 / 021 / 111 / 225 flashes drawn to 319 (item
  6's class); 5's survey item is this doc.
- **`docs/widescreen.md` 5:** "The two PSP `SetGeomOffset` callers without
  PC twins" closes (section 3); the 319-wide fills and the size-11 tile are
  what the `320.0` scan could not see.
- **`docs/psp-image.md` 1:** the `MsgBox_PlacementTable` row reads four x
  values (two -32, two +32), not "x 32 less and y 32 more".
- **`docs/owner-review.md`:** section 6's questions.
- **Ledger:** none now (a reading). If (a) is taken, DIV-0041 amendments per
  kind as listed in 5(a); (b) one amendment or a new DIV for the crop; (c)
  the importer's entry.
