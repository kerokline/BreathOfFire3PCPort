# Group E5G: kind 0x18's sub-kinds 0x2B, 0x2C, 0x36, 0x3A and 0x4A

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave five, from the round branch's tip `0834edf`. **24 functions ours**
(`src/game/effect_5g.cpp`, shadow name `effect_5g`): the cut table's 24 rows
for E5G (`analysis/round13_cut.tsv`, the band `0x50AF90..0x50BFF0`), none
added, none dropped. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
@@FUZZ@@. **Fuzz only**: no recorded route enters any of the 24 (section 9).
Every row is effect code (no `hypothesis` row in the group; none left
original). **DIV-0041's listed fill `0x50B4B5` is in this band** (sub-kind
0x36, section 2): it now draws through `Widescreen_FillX` /
`Widescreen_Fill`; no new ledger entry (the coordinator amends DIV-0041).

All five are sub-kinds of effect kind 0x18: `EffectKind18_Run` (`0x46D830`,
ours) jumps through `EffectKind18_States` by `+1`, which `EffectKind18_Start`
copies from `+0xB`. Four of them dispatch again by `+2` through a table of
their own; the fifth is one state.

| Sub-kind | Functions | Reached through |
|---|--:|---|
| 0x2B: two textured panels, mirrored, at a cell a variant table picks by the spawn's x cell; when the leader stands at the cell's front they slide apart over twelve frames (sound 0x200), are not drawn while the leader stays, and slide back once the leader is two cells away (sound 0x201). Drawn on the ground while waiting, flat at the variant's height while moving | 7 | `EffectKind18_States[0x2B]` (`0x654118`), `EffectKind18Sub2B_States` `0x65E9EC` (5) |
| 0x36: a semi-transparent, additive blue fill over the frame, its blue stepped through four levels every eighth frame; never ends by its own code | 1 | `EffectKind18_States[0x36]` (`0x654144`) |
| 0x2C: two panels like 0x2B's, lying across x or z (the spawn's z cell 0 chooses), drawn always and linked into the map's depth order at the record; eight frames each way | 7 | `EffectKind18_States[0x2C]` (`0x65411C`), `EffectKind18Sub2C_States` `0x65EA48` (5) |
| 0x3A: two panels at the record's cell that slide apart while any party member stands at them (sound 0x202) and back once none does (sound 0x203) | 7 | `EffectKind18_States[0x3A]` (`0x654154`), `EffectKind18Sub3A_States` `0x65EA60` (5) |
| 0x4A: 0x2C's panels placed with two map bytes written under them (`AreaMap_SetByte`) and `+0xB` set from the variant; then E6A's state, 0x2C's opening, the draw held | 2 | `EffectKind18_States[0x4A]` (`0x654194`), `EffectKind18Sub4A_States` `0x65EA78` (4) |

What each looks like in the game is not stated here (the owner's to say);
the descriptions are what the code draws and tests. "Panels" is a word for
two textured quads that slide apart; whether they are doors, gates or
something else is not established. **No spawner is found**: a grep of our
sources for these sub-kind numbers stored into `+0xB` finds none (kind 0x18
records come from the areas' effect lists, data not code); the spawn's `+0x34`
/ `+0x38` (the 16.16 point) carry the variant in the x cell and, for 0x2C /
0x4A, the axis in the z cell.

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`, the four dispatchers `evidence`); no PSX twin gives a name
(`analysis/pairs_propagated.json` has no pair in the band).

## 1. What each function does

### 1.1 Sub-kind 0x2B (`EffectKind18Sub2B_Run` `0x50AF90`)

| PC | Name | What |
|---|---|---|
| `0x50AF90` | `EffectKind18Sub2B_Run` | `jmp [EffectKind18Sub2B_States + +2 * 4]`, unbounded (hidden in E5F's `0x50AD70`) |
| `0x50AFB0` | `_Place` (sub-state 0) | the variant `v` = s16 `+0x36` (tables `0x65EA00` heights, `0x65EA08` texture bytes, `0x65EA0C` cells): the cell into `+0x36` / `+0x3A`; `AreaMap_Elevation(+0x34, +0x38)` - read **after** the cell is written, so at the new cell with the spawn's fraction; `+0x3E` = -height - ground / 2 (the low word), `+0x32` the height, `+0xC` the texture byte << 24, `+0x30` 0, `+2` up; the leader already at the cell's front: `+0x30` = 0xC0 and `+2` = 3. Then the draw on the ground |
| `0x50B0C0` | `_WaitNear` (1) | the leader at the cell's front: sound 0x200 unless `Field_Request` is set, `+2` up (`Sprite_Current` read after the sound). The draw on the ground |
| `0x50B150` | `_Open` (2) | `+0x30` up 0x10; at 0xC0 (signed) `+2` up. The draw flat |
| `0x50B180` | `_WaitFar` (3) | once the leader is two cells off (the front test at 0x20000 both ways fails) `+2` up. **Nothing drawn** |
| `0x50B1E0` | `_Close` (4) | `+0x30` down 0x10; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1. The draw flat |
| `0x50B220` | `EffectKind18Sub2B_Draw(int follow)` | the draw mode (page 0x95) committed at slot 6 (0xC); two POLY_FT4s (i 0, 1), the corners in `Prim_VertexScratch`: x `(+0x36 << 7) - 0x3FC0` at all four, y `(+0x3A << 7) - 0x3F40` / `- 0x4040` plus `+0x30` times the s8 `0x65EA14[i]` (one panel each way); heights from four `AreaMap_Elevation` calls when `follow`, else `+0x32` (the top 0x180 above); `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture(+0xC \| (0x130 - i) \| 0x1690000, p, 1)`, committed at slot 6 (0x48) |

**The front test** (every waiting state of 0x2B and 0x2C): the leader's
point (`ObjTrio +0x34` / `+0x38`) against the cell - across the first axis
`|p - ((cell + 1) << 16 | 0x8000)|` within 0x8000 (the next cell's centre,
half a cell either way), along the second `|p - (cell << 16)|` or `|p - (cell
<< 16) - 0x10000|` within 0x10000. The far test is the same at 0x20000 both.
Every word the draws compute is a 16-bit value (the originals store `ax` /
`cx` / `dx`); ours keeps the low sixteen bits. **The ground**: each corner's
16.16 point is `(s16 + 0x4000) << 9`; corners 0 and 2 share vertex 0's point
and corners 1 and 3 vertex 1's, but each corner calls `AreaMap_Elevation`
itself (four calls, two pairs with the same arguments) - ours makes the four.
The lift is `-(height / 2) - +0x3E` (a signed divide, toward zero), the top
corners 0x180 above.

### 1.2 Sub-kind 0x36 (`EffectKind18Sub36_Pulse` `0x50B480`)

One state, hidden in `0x50B220`: a draw mode (page 0xB5: blend 1, additive)
committed at slot 3 (0xC); a semi-transparent POLY_F4 over the frame -
**DIV-0041's listed fill** (`0x50B4B5` is the `mov ecx, 320.0f`) - corners
`(0, 0)`, `(320, 0)`, `(0, 240)`, `(320, 240)` as floats, colour `(0, 0x30,
0x65EA18[+2] << 3)`, committed at slot 3 (0x38); when `Frame_Counter & 7` is 0
(read after the commit) `+2` = `(+2 + 1) & 3`. Its index `+2` is unbounded
(room four).

### 1.3 Sub-kind 0x2C (`EffectKind18Sub2C_Run` `0x50B520`)

| PC | Name | What |
|---|---|---|
| `0x50B520` | `EffectKind18Sub2C_Run` | `jmp [EffectKind18Sub2C_States + +2 * 4]`, unbounded (hidden in `0x50B220`) |
| `0x50B540` | `_Place` (0) | `+8` = whether the spawn's z cell (s16 `+0x3A`) is 0; the variant (`0x65EA1C` heights, `0x65EA28` texture words, `0x65EA3C` cells): the cell, `AreaMap_Elevation` at the record's point, `+0x3E` = -ground / 2 - height, `+0x20` the texture word, `+0x30` 0, `+2` up; the leader at the cell's front (z first when `+8`): `+0x30` = 0x80, `+2` = 3. The draw |
| `0x50B6A0` | `_WaitNear` (1) | the front test (the axis `+8` names first): sound 0x200 unless `Field_Request`, `+2` up. The draw |
| `0x50B780` | `_Open` (2; also `EffectKind18Sub4A_States[2]`) | `+0x30` up 0x10; at 0x80 `+2` up. A tail jump to the draw |
| `0x50B7A0` | `_WaitFar` (3) | the far test fails: `+2` up. The draw |
| `0x50B870` | `_Close` (4) | `+0x30` down 0x10; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1. A tail jump to the draw |
| `0x50B8B0` | `EffectKind18Sub2C_Draw` | with `+8` the corners' y `(+0x3A << 7) - 0x3FC0` and the panels slide along x, else x `(+0x36 << 7) - 0x3FC0` and they slide along z; the corners' heights first `+0x3E` (dead stores: the ground overwrites them, below); two sides: a draw mode (page 0x95) linked by `MapView_LinkPrimAt(+0x34, +0x38, -1, 0xC)`, a POLY_FT4 slid by `+0x30` times the s8 `0x65EA5C[side]`, the heights from four `AreaMap_Elevation` calls, projected, `Prim_SetTexture((+0x20 + side) \| (+8 << 21), p, 1)`, linked likewise (0x48) |

### 1.4 Sub-kind 0x3A (`EffectKind18Sub3A_Run` `0x50BB90`)

| PC | Name | What |
|---|---|---|
| `0x50BB90` | `EffectKind18Sub3A_Run` | `jmp [EffectKind18Sub3A_States + +2 * 4]`, unbounded (hidden in `0x50B8B0`) |
| `0x50BBB0` | `_Start` (0) | `+8` = 1, `+0x3E` 0, `+0x20` = `0x28709126`, `+0x30` 0, `+2` up. The pair at dy 1, 2 |
| `0x50BC00` | `_WaitParty` (1) | any of the three `ObjTrio` records in use (`+0`) with `|z - (cell z << 16)|` within 0x18000 and x on the cell or the next (0x10000): sound 0x202 unless `Field_Request`, `+2` up. Every record is tested (the loop does not stop at the first). The pair at dy 1, 2 |
| `0x50BCB0` | `_Open` (2) | `+0x30` up 0x10; at 0x80 `+2` up. The pair at dy 0, 3 |
| `0x50BCE0` | `_WaitClear` (3) | no record in use within 0x28000 along z and 0x20000 along x: `+2` up. The pair at dy 0, 3 |
| `0x50BD70` | `_Close` (4) | `+0x30` down 0x10; at 0 or below sound 0x203 unless `Field_Request`, `+2` = 1. The pair at dy 1, 2 |
| `0x50BDC0` | `EffectKind18Sub3A_DrawPanel(unsigned side, int dy)` | one POLY_FT4 across x at column `+0x36 + side`: x `((side + +0x36) << 7)` minus `+0x30` times the s8 `0x65EA74[side]`, `- 0x4040` / `- 0x3FC0`; y `((+0x3A - 0x80) << 7)`; a draw mode (page 0x95) linked at the record's point with `dy` (0xC), the heights from four `AreaMap_Elevation` calls, projected, `Prim_SetTexture(+0x20 + side, p, 1)`, linked with `dy` (0x48). The states call it as `(0, dy0)` then `(1, dy1)` - the pushes of the first call stay on the stack under the second's |

### 1.5 Sub-kind 0x4A (`EffectKind18Sub4A_Run` `0x50BFD0`)

| PC | Name | What |
|---|---|---|
| `0x50BFD0` | `EffectKind18Sub4A_Run` | `jmp [EffectKind18Sub4A_States + +2 * 4]`, unbounded (hidden in `0x50BDC0`) |
| `0x50BFF0` | `EffectKind18Sub4A_Place` (0) | 0x2C's place without the front test: `+8`, the cell, the lift, `+0x20`, `+0x30` 0; `+0xB` the variant's byte of `0x65EA88` (every fourth byte); `AreaMap_SetByte(+0x36, +0x3A, b)` and `(+0x36, +0x3A + 1, b)`, `b` the variant's byte of `0x65EA9C` (read again for each; `Sprite_Current` too); `+2` up. The draw |

Its table's other entries: E6A's `0x50C0D0` (state 1, wave six), 0x2C's
`_Open` (2) and the draw itself (3: held open, drawn, nothing else).

## 2. Divergence

**DIV-0041's fill** (`docs/widescreen.md` section 5's table, `0x50B4B5`):
`EffectKind18Sub36_Pulse` draws its POLY_F4's left corners at
`Widescreen_FillX()` and its right at `320 + Widescreen_Fill()` (the shape
`area_w3c.cpp`'s gradient uses for a quad), which is the original's `0.0f` /
`320.0f` until `Widescreen_ArmFills` has run and whenever the picture is
narrow. The fuzz runs before the arm (`Effect5G_Inject` sits before
`FishingText_Arm` / `Widescreen_ArmFills` in `inject_all.cpp`), so it compares
the original's 320 x 240. **No new ledger entry**: the coordinator amends
DIV-0041's list. Otherwise no divergence: each function is a faithful
replacement. `widescreen.cpp`, `cheats.cpp` and `DIVERGENCE.md` patch no byte
inside the band (a grep of every address).

## 3. The tables

**The sub-state tables** (`symbols.toml` `[[data]]`): each its own length to
the data that follows it, checked by hand against what the states store
into `+2` (`band_rows.py` reads 5, 5, 5 and 4 code entries; here they agree):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind18Sub2B_States` `0x65E9EC` | 5 | `0x65EA00`, sub-kind 0x2B's variant tables (data) |
| `EffectKind18Sub2C_States` `0x65EA48` | 5 | `0x65EA5C`, sub-kind 0x2C's two slide signs (data) |
| `EffectKind18Sub3A_States` `0x65EA60` | 5 | `0x65EA74`, sub-kind 0x3A's slide signs (data) |
| `EffectKind18Sub4A_States` `0x65EA78` | 4 | `0x65EA88`, sub-kind 0x4A's variant bytes (data); entry 1 is E6A's `0x50C0D0`, entries 2 and 3 0x2C's `_Open` and draw |

**The data read in place** (raw in `effect_5g_callees.h`, not named in
`symbols.toml`; their bytes are not copied here): sub-kind 0x2B's three
variant tables `0x65EA00` / `0x65EA08` / `0x65EA0C` (room for four entries
each - the fourth all zero, alignment padding by its look - so three variants
with data and a fourth of zeros), the panel signs `0x65EA14` (two s8), 0x36's
four blue steps `0x65EA18`; 0x2C's and 0x4A's variant tables `0x65EA1C`
(room six), `0x65EA28` (five dwords, the least room, so five variants),
`0x65EA3C` (room six); the signs `0x65EA5C` and `0x65EA74` (room four each);
0x4A's `0x65EA88` and `0x65EA9C` (five dwords each, a byte read of each).
Ours aborts on an index past a table's room (section 6).

## 4. The fuzz (`effect_5g_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_5g`, effect mode (`g.effect`; kind 0x18
for every clone), 4,000 rounds a function (`BOF3X_E5G_ONLY=<name>` runs the
clones whose name holds it). Shapes: 22 `kEffect` (the four dispatchers'
`sub_span` their table's length, the pulse's 4), two `kCall`
(`EffectKind18Sub2B_Draw`, `EffectKind18Sub3A_DrawPanel`). No function
answers (no `ret_mask`). The four tables are `DataTable`s, swapped for
recorders on both sides. **Regions**: none beyond effect mode's standard ones -
the records, `Sprite_Current`, `ObjTrio` (the leader's and the members'
`+0`, `+0x34`, `+0x38`), `Prim_VertexScratch`, `Field_Request`,
`Frame_Counter`, the packet buffer. The variant, sign and step tables are the
image's, read-only, left in place.

**Callees**: the standard and effect-standard rows for `AreaMap_Elevation`
(both words whole), `AreaMap_SetByte` (s16, s16, byte - the originals push
registers with leftovers above), `Sound_PlayEffect`, `MapView_LinkPrimAt`
(the cursor moved two times in three), `Gte_RotTransPers4` (the outs
filled), `Gte_PrimDepths4_10`, `Prim_SetTexture`, `Gfx_CommitPrim`, the
`Gpu_*`. None re-listed: no caller here needs another reading. **The group's
own** called directly, listed by name: `EffectKind18Sub2B_Draw` at one whole
word and `EffectKind18Sub3A_DrawPanel` at two (the callers push immediates),
each logging `Sprite_Current` and its words `+0x30..+0x3B` (the slide, the
point); `EffectKind18Sub2C_Draw` as `kPhase` (it logs `Sprite_Current`).

**Seeds** (per function, after the harness's per-round fill): on all 20
records the slide `+0x30` one step from each end and either side of it (0x70,
0x80, 0xB0, 0xC0, 0x10, 0, ...), `+8` 0 half the time; the current record's
cell words small (0..0x7F) two times in three; for a place state the variant
inside its room (0..3, 0..4) and the cell the leader is tested against taken
from the variant's table (read from the image at run time), the z cell 0 half
the time for 0x2C / 0x4A; the leader and each of the three party records
about that cell seven times in eight - a whole cell -3..+4 off and a fraction
at the tests' boundaries (0, 1, 0x7FFF, 0x8000, 0x8001, 0xFFFF, random) - and
each record in use or not; `Field_Request` 0 half the time; `Frame_Counter`'s
low three bits 0 half the time. **Arguments**: `follow` 0 half the time, else
1 or any word; the panel's side inside its table's room (0..3), dy the
callers' 0..3, -1 or any. **Disturbance** (the group's, from the hash only):
the slide at or about 0xC0 / any, `+8`, the leader's x or z, a cell word.

@@RESULT@@

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 24 (4,227 bytes against the cut's
  4,406: 20 differ by padding only, none by code); each extent checked by hand
  to its `ret` or tail `jmp`. No shared tail, no case of a switch, no second
  entry among them; the tool lists no code of the band no list has.
- **Hidden starts**: 21, each an entry by address - a cell of
  `EffectKind18_States` or of a sub-kind's table - not a case. Their recorded
  hosts: E5F's `0x50AD70` (0x2B's six; its `entries_logic` extent `4AF` runs to
  `0x50B21E`), and this band's `0x50B220`, `0x50B8B0`, `0x50BDC0`, whose
  catalog extents (`68B`, `50C`, `6EB`) span the hidden starts after their
  `ret` - `0x50BDC0`'s into E6A's band. None of the hosts contains another's
  code as a fall-through: `0x50B220` ends at `0x50B470`, `0x50B8B0` at
  `0x50BB86`, `0x50BDC0` at `0x50BFC7`.
- **The sub-kind dispatchers**: every `EffectKind18_States` cell pointing
  into the band is one of this group's rows (`[0x2B]`, `[0x2C]`, `[0x36]`,
  `[0x3A]`, `[0x4A]`).
- **The draws as states**: `0x50B8B0` is both a callee (six calls, two tail
  jumps here, E6A's tail jump) and `EffectKind18Sub4A_States[3]`; it takes no
  argument and reads only `Sprite_Current`, so it is one function, fuzzed as a
  state.
- **The cut's columns**: the unit hints (tables `0x65E9EC`, `0x65EA48`,
  `0x65EA60`, `0x65EA78` and `EffectKind18_States`) are right; the "(7)",
  "(6)", "(3)" counts are rows per hint, not table lengths.
- **PSX twins**: none in the band.

## 6. Controls

@@CONTROLS@@

## 7. Latent defects (Capcom's, described, not fixed)

- **Unchecked indexes**: the four dispatchers do not bound `+2` (every writer
  in the band keeps it inside its table: the states step 1..4 and back); the
  variant index is the spawn's x cell, s16 and unchecked, into tables with
  room for four (0x2B) and five (0x2C, 0x4A) - a spawn whose x cell is past
  them reads the next table's bytes as a cell, a height and a texture; 0x36's
  `+2` into four bytes (kept 0..3 by its own `& 3`, but read before that wrap
  from whatever the spawn left, where `EffectKind18_Start` zeroes it); the
  draw's side into `0x65EA74` (its callers push 0 and 1). Ours aborts past any.
- **The INT_MIN distance**: the waiting states' `cdq; xor; sub` absolute value
  leaves `0x80000000` negative, so a leader exactly 0x8000 cells away counts as
  near. Unreachable on a map (cells are bytes); ours computes it the same way.
- **Dead stores**: `EffectKind18Sub2C_Draw` writes the four corners' heights
  from `+0x3E` before the ground overwrites every one of them; harmless, kept.
- **0x2B's waiting-open state draws nothing** while 0x2C's draws: as read,
  perhaps deliberate (the panels slid out of sight).
- **0x36 never ends**: no state of it releases the record or advances past
  itself.

## 8. Calls across groups

**Outbound**: none raw (`band_rows.py --edges`: no call from E5G into another
group). By name, already ours: `AreaMap_Elevation`, `AreaMap_SetByte`,
`Sound_PlayEffect`, `MapView_LinkPrimAt`, `Gfx_CommitPrim`, the `Gpu_*`,
`Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture`.
`EffectKind18Sub4A_States[1]` is E6A's `0x50C0D0`, reached through the table
read in place (swapped for a recorder in the fuzz).

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `0x50C0D0` (`jmp` at `0x50C139`) | E6A (wave six) | `EffectKind18Sub2C_Draw` `0x50B8B0` - E6A's files, raw until this merges |
| `EffectKind18_Run` `0x46D830` through `EffectKind18_States` `[0x2B]`, `[0x2C]`, `[0x36]`, `[0x3A]`, `[0x4A]` | ours (worldmap_area) | the five dispatchers / the pulse, read in place |

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 24 rows, and no first-call trace under
`analysis/calltrace` names any of the 24 (a grep of every file: the hits are
the extent lists `entries*.txt`, not traces). **Fuzz only.** No live run was
made (the brief). No spawner is known (the top); a recorded route through an
area whose effect list holds one of these sub-kinds would let the
coordinator's frame-hash A/B cover it.

## 10. The rebinding

`grep -rn -i` of the 24 addresses and the four tables in `src/game`
(`band_rows.py --refs`: 0 references). **Nothing to rebind**; nothing left
raw. E6A's tail jump to `0x50B8B0` is in E6A's (wave six) files, for the
coordinator.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 21 lines, the read extents
of the hidden starts. The three host lines `0050B220 68B`, `0050B8B0 50C`,
`0050BDC0 6EB` are left in place (each spans the hidden starts after its
host's `ret`; the code is 0x251, 0x2D7, 0x208), as E4D left its hosts'.
