# The 21:9 frame: `BOF3X_WIDE=2`, 560 x 240 (group UW)

**Status:** BUILT (2026-10-13), headless only - group UW of the ultrawide
round ([`ultrawide-round.md`](ultrawide-round.md) section 2 "UW"), branch
`wide/uw` off the round tip `6b4c97cc`. Nothing was seen in play: every
number below is from a build, a self-test or a reading of our source. The
pictures are EB's (stage B).

## 0. The answer

- `BOF3X_WIDE=2` gives a **560 x 240** picture, **120 columns a side**, the
  game's 320 x 240 view centred in it by the same primitive shift as
  DIV-0041. `1` is still 426 x 240 (53 a side), unset or `0` the original's,
  anything else Fatal.
- **Narrow and 16:9 are what they were at `6b4c97cc`.** Every value that had
  53 folded in is now a rule in the columns (section 1), and a start-up
  property check proves each rule gives the 16:9 build's constant at 53
  (section 2). The one value tuned by eye at 53, the terrain cull's margin,
  is 100 there by the same rule and **167 at 120 for the owner's eye**.
- Found on the way, and fixed by rule: **Manillo's trade-screen backdrop
  would have aborted at 120** (its grown halves pass the byte's u 255), and
  **the cell inset cannot do what 120 asks**: the original's largest inset
  is 6, so 21:9's lowering of 6 walks the whole 28-column ring whatever the
  camera; what that ring covers at 560 is UC's question (section 3).
- The launcher's "Widescreen" box is a three-way choice (`wide=0|1|2`), and
  `attract_run.py`, `input_run.py` and `abc_run.py` take `--wide 0|1|2`.

## 1. The rules (`widescreen.h`, `namespace widescreen_rule`)

Each is a function of the columns `c`; none is a table of per-width numbers.

| Value | Rule | At 53 (the 16:9 constant it replaced) | At 120 |
|---|---|---|---|
| The columns | `ColumnsForAspect(num, den)`: 240 num / den rounded down to even, less 320, halved | `kColumns` 53 (426 wide) | 120 (560, exact) |
| Terrain cull margin past `[-50, 370]` | `c + 47` (tuned by eye at 53; `BOF3X_WIDE_TERRAIN` still overrides) | `kTerrainMargin` 100: `[-150, 470]` | 167: `[-217, 537]` - **the owner's eye** |
| Cell inset lowered | `ceil(c / 20)` | `kInsetColumns` 3 | 6 (section 3) |
| Menu backdrop pairs a side | `ceil(c / 64)` | `extra = Live ? 1 : 0` - 1 | 2: seven pairs become nine, `-0x80..0x1C0` over `-120..440` |
| Area-map frame ranges | `lo - (c - 1)`, `hi + (c - 1)` | `[-252, 572]`, `[-102, 422]` | `[-319, 639]`, `[-169, 489]` |
| Fourteen slide-off bounds | `bound - c` left, `bound + c` right | `-223 .. 400` | `-420 .. 467` |

The fills, anchors, culls and the battle tile already read the columns
(`Widescreen_Fill` / `_Live` / `_Anchor` / `_FillRight` / `_BattleTile`);
they follow the number unchanged. `DisplaySetup_ViewWidth`, the backend's
`pad_x`, the window's placement, `WM_SIZING`'s snap and minimum and
`FitScale` all take the view's width (`320 + 2 c`), so the present needed no
change (section 4).

## 2. The property check

`Widescreen_CheckRules` (`widescreen.cpp`), run by `Widescreen_Inject` at
every width, and `static_assert`s of the same at compile time:

- at 53, each rule against the constant it replaced - the columns, 100, 3,
  1, the four range floats, and the fourteen slide bounds as the 16:9 build
  patched them (`-223, -353, -153, -233, -163, -253, -203, -173, 373, 375,
  -243, -218, 400, 376`); Fatal on a difference;
- at 120, every slide bound still fits the imm16 or imm32 its site holds;
- one log line with the 21:9 values (section 6 quotes it).

Beside it:

- `effect_1g`'s backdrop check (`effect_1g_fuzz.cpp` `CheckWideSpans`) ran
  0..63 columns; it now runs **0..224** (the widest a band quad's byte
  allows): the quads cover `-c..320 + c` exactly once, one texel a column, u
  in the original's phase (`u = x mod 32`) - and at 53 the two quads are
  byte for byte the 16:9 ones (`-53..160` u `11..224`, `160..373` u
  `0..213`).
- `display_setup.cpp` `CheckFitScale`: `FitScale` at the three widths on the
  monitors named here (section 4).

## 3. What 120 changed beyond the number

### 3.1 Manillo's backdrop: four quads past 64 columns

`ItemTrade_DrawBackground` (`0x5942C0`, ours, `effect_1g.cpp`) draws the
tiled fish pattern as two POLY_FT4s whose u range grows with the columns
(DIV-0041, 2026-10-03). The left half's u runs to `(-c mod 32) + 0xA0 + c`,
which passes the byte's 255 from **65 columns** on: at 120 it would be 288,
and `ItemTrade_BackdropSpan` aborted on that by design. Now, from 65 on, the
original's two halves are drawn as they are (u `0..0xA0` each) and a band
quad each side covers `-c..0` (u from `-c mod 32`) and `320..320 + c` (u from
0), so the pattern still keeps its phase at every column with no stretch.
Up to 64 columns (16:9 among them) it is the two grown halves as before.
`ItemTrade_BackdropQuads(c)` says which; the packets at 0 and 53 are what
they were (the property check proves the spans; the clone fuzz, which runs
unarmed, compares the original's).

### 3.2 The slide bounds pass at 120

Each of the fourteen readers (ours: `menu_lists.cpp` `SlideX`, `rest_2g.cpp`
`Slide` and `GeneWin_GridSlideOut`, `rest_2f.cpp` `MenuSlide_LeftOff170`,
`rest_2h.cpp` `MasterWin_SlideOut`, `menu_draw_helpers.cpp`
`MenuWin_SlideOutLeft`, `battle_e7.cpp`'s three gene lists) steps the x
word by 0x20 a frame in one direction and either clamps at the bound
(`SlideX`, `Slide`, `MenuSlide_LeftOff170`) or frees the window once at or
past it (`GeneWin_*`). Read 2026-10-13: no reader steps back or wraps, and
the widest bound (`-420`, `467`) is far inside the s16 the x word is read
as, so a box resting anywhere in the view reaches its bound after at most
`(|bound| + 320) / 0x20` frames (23 for `-420`). The boxes take that many
frames more to leave; whether the longer slide reads well is the owner's
eye.

### 3.3 The cell inset reaches its floor

`map_layers.cpp` `Inset()` trims the 28-column ring by
`(0x160 - |angle - 0x200|) / 50 - 1`, less `Camera_Distance / 650`, less 2
with input flag 1, 0 for `Cond_ByteFE == 0x23` - at most **6** (head-on, at
distance 0). The wide picture lowers it by `Widescreen_InsetColumns`, not
below 0. At 53 that was 3; at 120 the rule asks for 6, so **the whole ring is
walked at every camera**. The log line says so (section 6). Whether 28
columns of ring cover 560 columns of picture is UC's measurement; if not, RG
grows the ring (IDEAS I37). Until then a notch at the 21:9 edges, where the
ring runs out, is expected - not a defect of this group's rule.

### 3.4 The message boxes' left placements go negative at their centre

Not changed by this group, read for the record. `window_task.cpp` moves the
side placements by `Widescreen_Anchor()` (WA, 2026-10-12). Kind 0's left
box starts at its table's x; the PSP's values put that x at 16 on the 4:3
picture (`window_task.cpp`'s note), so it is -37 at 53 and **-104 at 120**,
and its centre, stored `(x + 0x64) << 4` in a u16, goes from +63 at 53 to
**-4 at 120**. Wrapped into the u16, that is the same arithmetic that
already carries the box's negative left edge at 53; whether every consumer
of the centre word reads it as signed was not traced. **EB's census
or the owner's eye should look at a left-placed message box at 21:9.**

## 4. The present

`FitScale(view_w, cw, ch)` is the largest k in 1..8 whose
`view_w k x 240 k` fits the client; the snap, the window's minimum
(`cw < view_w`) and its first placement all take `DisplaySetup_ViewWidth()`.
Checked at start-up (`CheckFitScale`, Fatal on a difference):

| Monitor | 320 | 426 | 560 |
|---|---|---|---|
| 3440 x 1440 (the owner's) | 6 | 6 | **6: 3360 x 1440, 40 px of black each side** |
| 2560 x 1080 | - | - | 4 (2240 x 960) |
| 1920 x 1080 | - | 4 | 3 (1680 x 720) |
| 559 x 239 (smaller than a picture) | - | - | 1 |

The target at k = 8 is 4480 x 1920, inside the 0x4000 the device
description reports. The snap-off rule (the height filled, the smallest k
not smaller) does not depend on the width.

## 5. The launcher and the run tools

- `bof3x.ini` `wide=0|1|2` (anything else is read as the default, as every
  unknown value is); `ConfigApplyEnvironment` sets `BOF3X_WIDE` to it unless
  the variable is already set. The dialog's checkbox is a combo box, "Off -
  320 x 240", "16:9 - 426 x 240" and "21:9 - 560 x 240", its index the ini's
  value (`launcher.rc`, `config_dialog.cpp`).
- `input_run.py`, `attract_run.py`: `--wide 0|1|2` exports `BOF3X_WIDE`;
  set, it wins over the ini (the launcher fills an unset variable only), so
  `--wide 0` is narrow even beside a `wide=1` ini. Unset, the ini decides, as
  before. `attract_run.py` writes `# BOF3X_WIDE=N` under its run's TSV.
  `abc_run.py --wide N` passes it to every `input_run.py` it starts and
  records it in each run's meta.

## 6. Checks run

All 2026-10-13, this branch, `cmake --preset i686 && cmake --build build`
(clean of warnings in the files touched; the build log's warnings are SDL's
and `labels.cpp`'s, untouched).

- **The self-tests**, `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original
  BOF3X_SHADOW='*'` from an ini-less copy of the launcher, `bof3x.exe` and
  the DLL (`--game <main checkout>/bof3 --no-config`): RESULTS
- **The launcher host suite** (`tools/launcher_host`, clang++): 234 checks,
  0 failures, the new `TestWide` among them (`wide=` 0, 1, 2 read, written
  back and exported; a set `BOF3X_WIDE` kept; `3`, `-1`, `21:9`, `true`
  read as 0).
- **`dialog_test.py`**: DIALOG
- **`tools/ledger_check.py`**: LEDGER

## 7. What was not verified

- **Nothing was seen.** No live run: the 21:9 picture, the bands' fills,
  the backdrop's band quads, the slide-outs' longer exits and the present at
  k = 6 on the owner's monitor are EB's captures and the owner's eye.
- The terrain margin 167 at 120 is the rule's, not an eye's.
- Whether the ring covers 560 (section 3.3): UC's, then RG's.
- The pool (DIV-0062) at 120: UC estimates, EB measures.
- The left message box's negative centre (section 3.4): read, not traced.
- The culls left at Capcom's values (`ultrawide-round.md` section 1's row):
  UC's and UC2's; at 120 they pop inside the view.
- The state hash at `BOF3X_WIDE=2`: EB's.

## 8. The owner's calls this meets

- 2026-10-13: a fixed 21:9 preset, 560 x 240, 120 columns a side; the 16:9
  decision (426, no crop) as it was; wide mode still opt-in (the default
  `wide=0` and an unset variable are narrow).
- The round's rule: narrow and 16:9 bit for bit; every width-dependent value
  a rule in the columns; the one tuned value (the terrain margin) kept at 53
  and flagged at 120.

## 9. For the other files

### `docs/DIVERGENCE.md`, DIV-0041 - DIV A (an amendment; the coordinator numbers and places it)

**Amended 2026-10-13 (the ultrawide round, group UW,
[`ultrawide-frame.md`](ultrawide-frame.md)): the wide picture comes in two
widths, 426 and 560.** `BOF3X_WIDE=1` is the 16:9 picture as before, 426 x
240, 53 columns a side; **`BOF3X_WIDE=2` is the 21:9 picture, 560 x 240, 120
columns a side** (240 x 21 / 9 exactly; the owner's choice of 2026-10-13: a
fixed preset rather than the monitor's width or 32:9). This replaces
[`widescreen.md`](widescreen.md) 1's "a 21:9 monitor stays pillarboxed".
Everything DIV-0041 widens follows the columns: the shift, the target, the
fills, anchors and culls of ours, the battle tile. The values that had 53
folded in are rules in the columns (`widescreen_rule`): the terrain cull's
margin `c + 47` (100 at 53 as tuned by eye, **167 at 120, owed the owner's
eye**), the cell inset lowered by `ceil(c / 20)` (3, and 6 - the original's
largest inset, so at 21:9 the whole 28-column ring is walked; whether it
covers 560 is the round's UC / RG), the menu backdrop's `ceil(c / 64)` extra
pairs (1, 2), the area-map frame ranges moved by `c - 1`, the fourteen slide
bounds by `c`. Manillo's backdrop draws the original's two halves and a
band quad each side from 65 columns on, since the grown halves would pass u
255. The launcher's "Widescreen" box is a three-way choice (`wide=0|1|2`).
On the owner's 3440 x 1440 the 21:9 picture lands on k = 6, 3360 x 1440 with
40 px of black each side. **Verification:** a start-up property check
proves every rule gives the 16:9 build's constant at 53
(`Widescreen_CheckRules`, and `static_assert`s), the backdrop's spans are
proved for 0..224 columns, `FitScale` at the three widths; the hosted
`'*'` self-tests unset, `=1` and `=2` (section 6 there). Not seen in play.
**Reversible:** `BOF3X_WIDE=1` (16:9) or unset (narrow); the
`BOF3X_ORIGINAL` switches as above.

The entry's header lines want the 21:9 width beside the 426 one: "Subsystem"
can stay; the title might read "the view widened by 53 or 120 columns a
side" - the coordinator's call.

### `docs/README.md` row

| [`ultrawide-frame.md`](ultrawide-frame.md) | The 21:9 frame (`BOF3X_WIDE=2`, 560 x 240, 120 columns a side): the width rules in the columns and their property check at 53, the backdrop's band quads, the inset's floor, the present at k = 6, the launcher's three-way box and the tools' `--wide` (ultrawide round, group UW). |

### `docs/widescreen.md`

Section 1's second bullet ("A 21:9 monitor stays pillarboxed") is replaced
by the 2026-10-13 decision: link [`ultrawide-frame.md`](ultrawide-frame.md).

### For STATUS / HANDOFF

- The 21:9 frame is built and self-tested; nothing seen. EB owes the
  captures (the band fills, Manillo's backdrop at 120, a left message box,
  the slide-outs, the present at k = 6) and the terrain margin's eye at 167.

## 10. Needs from the other groups

- **UC:** whether 28 ring columns, fully walked (the inset at 0), cover 560
  at the recipes' camera distances; the pool's estimate at 120.
- **RG:** the ring, if UC says so. `Widescreen_InsetColumns()` keeps its
  rule; a grown ring would want the inset rule re-read against the new
  column count.
- **EB:** the captures in section 7, `--wide 2` on the run tools for every
  recipe, and the terrain margin to the owner.
- **BA:** nothing from this group beyond `Widescreen_Columns()` (unchanged in
  shape: 0, 53 or 120).
