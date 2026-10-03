# Group E5F: kind 0x18's sub-kinds 0x27, 0x28, 0x29, 0x2A, 0x3C, 0x42, 0x48, 0x49 and 0x4B / 0x4C

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..15),
wave five, from the round branch's tip `0834edf`. **49 functions ours**
(`src/game/effect_5f.cpp`, shadow name `effect_5f`): the cut table's 48 rows
for E5F (`analysis/round13_cut.tsv`, the band `0x508CC0..0x50AD70`) and one
catalog row of no group inside the band, sub-kind 0x42's draw `0x509A70`
(section 5). Each read to its last instruction with capstone and fuzzed
through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
196,000 rounds, 0 mismatches; 135 of 136 controls refused by a count, the
other an equivalent mutant (a levelled word) whose near variant is refused
(section 8). **Fuzz only**: no recorded route enters any of the 49 (section 10).
Every row is effect code, the two `hypothesis` rows among them (sub-kind
0x42's dispatcher `0x509690` and its party test `0x509C00`): both taken.
**No divergence, no ledger entry - but one latent defect reads memory the
original never writes** (`EffectKind18Sub4B_Run`'s stack word, section 7):
ours takes the quad's own x there, and the fuzz levels those words on both
sides. The coordinator's and the owner's to ledger.

| Sub-kind | Functions | Reached through |
|---|--:|---|
| 0x27: a textured panel on the ground at a cell (two placements), sliding by `+0x30` (to -0x100) when the leader enters the cell's gate and back once the leader is two cells away | 7 | `EffectKind18_States[0x27]` (`0x654108`), `EffectKind18Sub27_States` `0x65E890` (5) by `+2` |
| 0x28: 0x27's panel with `+8` clear only, waiting first on story flag 0x54 | 8 | `[0x28]` (`0x65410C`), `EffectKind18Sub28_States` `0x65E8B0` (6) |
| 0x42: six pieces raised from the ground at one of three places by a story flag (the place's `+0xB`) when no party member is within reach; the two cells under it marked on the map view (DIV-0062's site) | 9 | `[0x42]` (`0x654174`), `EffectKind18Sub42_States` `0x65E8C8` (6) |
| 0x29: a lifted panel in two halves (four placements) that slide apart by `+0x30` (to 0x80) at the leader's gate | 7 | `[0x29]` (`0x654110`), `EffectKind18Sub29_States` `0x65E968` (5) |
| 0x48: 0x29's panel opened by `Cond_ByteFE` 0x10, then left open | 3 | `[0x48]` (`0x65418C`), `EffectKind18Sub48_States` `0x65E980` (4) |
| 0x49: 0x29's panel opened by `Cond_ByteFE` 0x20, or by the leader once story flag 0x8F is set | 3 | `[0x49]` (`0x654190`), `EffectKind18Sub49_States` `0x65E990` (5) |
| 0x4B / 0x4C: one program - three quads drifting along z, their width pulsing with `Frame_Counter`, drawn on `Draw_PassFlags` bit 2 | 1 | `[0x4B]` and `[0x4C]` (`0x654198`, `0x65419C`), no table |
| 0x2A: 0x29's two halves with six placements, opened to 0xC0 | 6 | `[0x2A]` (`0x654114`), `EffectKind18Sub2A_States` `0x65E9A8` (5) |
| 0x3C: two quads at a fixed point that rise by `+0x30` to 0x100 once story flag 0x67 is set, then `MoveCmd_TestFB(9, 0xF)` and the record released | 5 | `[0x3C]` (`0x65415C`), `EffectKind18Sub3C_States` `0x65E9D8` (3) |

What the panels and pieces look like in the game, and where it shows them,
is not said here: no spawner of these sub-kinds was found by a grep of our
area and scenario source (kind 0x18's programs are chosen by `+1`, which the
spawner sets), and no recorded route reaches them. The descriptions are the
code's.

## 1. What each function does

Every function runs with `Sprite_Current` an `Effect_Objects` record whose
`+5` is 0x18 and `+1` the sub-kind; `+2` is the sub-state. The record's words:
`+0x36` / `+0x3A` the cell (x, z; the start states take a placement index
from `+0x36` and overwrite both with the placement's cell), `+8` the axis
(set when the starting `+0x3A` is 0: the panel runs along z), `+0x30` the
slide, `+0x3E` the lift, `+0x2E` / `+0x20` the texture, `+0x34` / `+0x38` the
16.16 point the draws link at. **The leader's gate** (`Within`, all the
panels): along the axis `+8` picks, the leader's point (`ObjTrio` record 0,
`+0x34` / `+0x38`) within 0x8000 of the middle of the cell's far edge
(`(c + 1) << 16 | 0x8000`); across it within 0x10000 of the cell's start or
the next's. "Two cells away" is the negation of the same test at
(0x20000, 0x20000). The absolute values are `cdq / xor / sub`, so
0x80000000 stays negative and passes every bound, as in the original.

### 1.1 Sub-kind 0x27 (`EffectKind18Sub27_Run` `0x508CC0`)

| `+2` | Function | Does |
|--:|---|---|
| 0 | `_Start` `0x508CE0` | `+8`; the placement (two) gives the cell, `+0x3E` and `+0x2E`; `+0x30` 0, `+2` 1; the leader in the gate: `+0x30` -0x100, `+2` 3; the draw |
| 1 | `_WaitNear` `0x508E20` | the leader in the gate: sound 0x200 unless `Field_Request`, `+2` up; the draw |
| 2 | `_Open` `0x508F00` | `+0x30` -0x20; at -0x100 or below `+2` up; the draw (tail) |
| 3 | `_WaitFar` `0x508F20` | the leader two cells away: `+2` up; the draw |
| 4 | `_Close` `0x508FF0` | `+0x30` +0x20; at 0 or above sound 0x201 unless `Field_Request`, `+2` 1; the draw (tail) |
| - | `_Draw` `0x509030` | a draw mode linked at the point (dy -2), one shaded `POLY_FT4` in `Prim_VertexScratch`: along the axis the cell's 0x80 less `+0x30`, across it the cell, each corner 0x40 / 0x180 above half `AreaMap_Elevation` (four calls, the second pair's point read after the second call); `Prim_SetTexture(+0x2E sign-extended | +8 << 21 | 0x1500000)`, linked with 0x48 bytes |

### 1.2 Sub-kind 0x28 (`EffectKind18Sub28_Run` `0x509290`)

The same machine as 0x27's with `+8` clear (no `+8` test: the gate along x, the slide along z) and drawn by
`_Draw` `0x5094E0` (a committed draw mode, `Gfx_CommitPrim(6, ...)`, texture
0x1780119). `_Start` `0x5092B0` sets `+0x3E` -0x5C0, `+0x30` 0, `+2` 1, then
`Flags_Test(0x904030, 0x54)`: clear, `+2` 5; set, the gate test as 0x27's.
`_WaitNear` `0x509360`, `_Open` `0x5093E0`, `_WaitFar` `0x509400`, `_Close`
`0x509460` as 0x27's; `_WaitFlag` `0x5094A0` (state 5): the flag set, sound
0x200, `+2` 2 (straight to opening).

### 1.3 Sub-kind 0x42 (`EffectKind18Sub42_Run` `0x509690`)

| `+2` | Function | Does |
|--:|---|---|
| 0 | `_Start` `0x5096B0` | the placement (three records of four bytes) gives the cell, `+8` (the variant) and `+0xB` (a story flag); `+8` clear: `+0x3E` 0, `+2` 1; set: `+0x3E` 0x80, `+2` 4 |
| 1 | `_WaitFlag` `0x509740` | `Flags_Test(+0xB)` byte `^ +8` not 0 and `_MemberNear(point | 0x8000)` none: `MoveCmd_TestFB(cell + 1)`, `+2` up, sound 0x205; the draw. Otherwise nothing at all, **not the draw** |
| 2 | `_Raise` `0x5097E0` | `+0x3E` +0x10; at 0x80: `MoveCmd_TestFB(cell)`, `MoveCmd_TestFC(cell + 1)`, `+2` up; the draw |
| 3 | `_Mark` `0x509850` | for the cell and its neighbour along `+8`'s axis: the area block's cell word indexes a texture dword past the cells; the cell's draw item (`MapView_ItemAt`) keeps a second item in its `+0x7E` (`+8` set) or `+0x8E` word, allocated (`DrawItemPool_Alloc`) and textured when the word is 0. `MapView_Redraw` 2, `+2` up; the draw |
| 4 | `_WaitFlagBack` `0x509980` | `(Flags_Test(+0xB) == 0)` differing from `+8` (a whole compare): `MoveCmd_TestFB` at the cell and the cell + 1, `+2` up, sound 0x205; the draw. Otherwise nothing |
| 5 | `_Lower` `0x509A20` | `+0x3E` -0x10; at 0 or below `MoveCmd_TestFC(cell + 1)`, `+2` 1; the draw |
| - | `_Draw(variant, height)` `0x509A70` | the variant's six pieces (dx, dz, shape, texture byte), each a draw mode linked at its cell (dy 1) and a shaded `POLY_FT4` of the shape's four s8 vertices, heights `<< 7` less `height` plus the variant's lift word; `Sprite_Current` read again for every piece and vertex |
| - | `_MemberNear(x, z)` `0x509C00` | for each of `Field_MemberCount` `ObjTrio` records in use: its point plus its velocity (`+0xC`, `+0x10`) times `+9` within `(+0x70 + 3) << 15` of (x, z) on both axes: that member in `al`; else 0xFF |

The `MoveCmd_TestFB` / `FC` answers are unread (the calls mark or test the
cells; their own docs say which).

### 1.4 Sub-kinds 0x29, 0x48, 0x49 (`0x509C90`, `0x50A2D0`, `0x50A3C0`)

| Function | Does |
|---|---|
| `EffectKind18Sub29_Start` `0x509CB0` (0x29's and 0x49's state 0) | `+8`; the placement (four) gives the cell, `+0x3E` = -(half `AreaMap_Elevation` at the point) less the placement's word, `+0x20`; `+0x30` 0, `+2` 1; the leader in the gate: `+0x30` 0x80, `+2` 3 |
| `_WaitNear` `0x509E10`, `_WaitFar` `0x509F10` | as 0x27's |
| `_Open` `0x509EF0` (state 2 of all three) | `+0x30` +0x10, at 0x80 `+2` up |
| `_Close` `0x509FE0` (0x29's and 0x49's state 4) | `+0x30` -0x10, at 0 or below sound 0x201, `+2` 1 |
| `_Draw` `0x50A020` (and 0x48's state 3) | two halves k, each a draw mode linked at the point (dy -1) and a `POLY_FT4`: along the axis the cell + k (z - k) moved by `+0x30` times the half's s8 direction; every height less half the ground's and `+0x3E`, v0 / v1 0x180 lower; texture `(+0x20 + k) | +8 << 21` |
| `EffectKind18Sub48_Start` `0x50A2F0` | 0x29's set-up without the gate test |
| `EffectKind18Sub48_WaitCue` `0x50A390` | `Cond_ByteFE` 0x10: sound 0x200, `+2` up |
| `EffectKind18Sub49_WaitCue` `0x50A3E0` | `Cond_ByteFE` 0x20: sound, `+2` up; then story flag 0x8F and the leader in the gate (along x): sound, `+2` up again (two steps in one frame possible) |
| `EffectKind18Sub49_WaitFar` `0x50A4A0` | unless `Cond_ByteFE` is 0x20, the leader two cells away (along x): `+2` up |

### 1.5 Sub-kinds 0x4B / 0x4C (`EffectKind18Sub4B_Run` `0x50A510`)

One function at both entries; v = `+0xB` - 0x4B (an unchecked byte, used in
arithmetic only). `+2` 0: `+0x10` = `(Rand & 0xFF) + ((3 v + 6) << 9)`,
`+0x2E` / `+0x30` from the byte pair `0x65E9A4` picks by `+0x34`'s low word
being 0, `+0x3A` up `Rand & 7` (the pointer taken before the call), `+2` 1.
Then, on `Draw_PassFlags` bit 2 only: `+0x38 += +0x10`; `+0x3A` past `+0x30`
(signed) back to `+0x2E`; a committed draw mode and three shaded quads k,
corners at `x0 + 0xA00 k -/+ b`, `z0 + 0x180 k -/+ b`, height 0xB00, where
`x0 = (+0x34 >> 9) - 0x4EC0`, `z0 = (+0x38 >> 9) - 0x3FC0`,
`b = |0xF - (Frame_Counter & 0x1F)| + ((2 - v) << 8)`; texture
`v | 0xBB509100`. The far corners' x: section 7.

### 1.6 Sub-kind 0x2A (`EffectKind18Sub2A_Run` `0x50A760`)

`_Start` `0x50A780` (six placements, `+0x3E` from the ground less the
placement's word, the gate opening to 0xC0), `_WaitNear` `0x50A8D0`, `_Open`
`0x50A9B0` (to 0xC0), state 3 **E5E's `0x508670`** (the same far wait as
0x27's, with no draw), `_Close` `0x50A9D0`; `_Draw` `0x50AA10`: a committed
draw mode, two halves moved by `+0x30` times the half's direction (no `+ k`
on the cell), heights as 0x29's, texture `+8 << 21 | (0x127 - k) |
0x25510000`, committed with 0x48 bytes.

### 1.7 Sub-kind 0x3C (`EffectKind18Sub3C_Run` `0x50ACB0`)

`_Start` `0x50ACD0` (`+0x30` 0, `+2` 1, draw flat), `_WaitFlag` `0x50ACF0`
(story flag 0x67: sound 0x200, `+2` up; draw on the ground), `_Rise`
`0x50AD30` (`+0x30` +0x20; at 0x100 `MoveCmd_TestFB(9, 0xF)` and
`Effect_Release`, then the draw anyway); `_Draw(edge)` `0x50AD70`: two quads,
x from `+0x30`, z fixed; `edge` (the whole word) set: the heights from the
ground, the first quad's far pair 0x180 above; clear: fixed (-0x480; the far
pair -0x300 / -0x480); textures the two dwords at `0x65E9E4`.

## 2. Divergence

None ledgered. Every function is a faithful replacement but for
`EffectKind18Sub4B_Run`'s stack word (section 7), which the original never
writes; every call goes through `SH_CALL`; `Sprite_Current` is read again
wherever the original reads `[0x937F88]` after a call, and each call's answer
is taken before the memory reads that follow it. `widescreen.cpp`,
`cheats.cpp` and `DIVERGENCE.md` patch no byte inside the 49 but DIV-0062's
five immediates in `0x509850` (`0x5098F4`, `0x5098FB`, `0x509909`,
`0x509910`, `0x509930`, `draw_pool.cpp`'s `kSites`): ours reads the item
array through `draw_pool::Items()` / `Count()`, and
`EffectKind18Sub42_Mark` is added to `draw_pool.cpp`'s `kOwnedUsers` (left
original by `BOF3X_ORIGINAL`, it keeps the original's pool for everybody, as
E3D's `EffectKind7D_SetMap`). No full-frame fill lies in the band (no
DIV-0041 site).

## 3. The tables

| Table | Named | Entries | Why that many |
|---|---|--:|---|
| `0x65E890` | `EffectKind18Sub27_States` | 5 | the states set 1..4; `0x65E8A4` is the placement words |
| `0x65E8B0` | `EffectKind18Sub28_States` | 6 | to `0x65E8C8`, which `0x509690` indexes (the tool ran the two as one run of 12) |
| `0x65E8C8` | `EffectKind18Sub42_States` | 6 | the states set 1..5; `0x65E8E0` is the placement records |
| `0x65E968` | `EffectKind18Sub29_States` | 5 | then two direction bytes and 0x48's table |
| `0x65E980` | `EffectKind18Sub48_States` | 4 | to `0x65E990`, which `0x50A3C0` indexes (the tool's run of 9); state 3 is the draw itself |
| `0x65E990` | `EffectKind18Sub49_States` | 5 | then the byte pairs `0x65E9A4` |
| `0x65E9A8` | `EffectKind18Sub2A_States` | 5 | entry 3 is E5E's `0x508670`; then the placement words |
| `0x65E9D8` | `EffectKind18Sub3C_States` | 3 | then the two texture dwords |

None is bounded by a compare. A raw scan of the image for every address
`0x65E880..0x65E9EF` finds each table named once, by its dispatcher, and no
other code indexing inside one (one unaligned hit, `0x65E8C3` at file offset
`0x12E4A5`, is not code that reads it). Data read in place (no bytes copied
here; `effect_5f_callees.h` names each with its count): the placements
`0x65E8A4..0x65E8AF` (two), `0x65E8E0` (three records of four),
`0x65E948..0x65E967` (four), `0x65E9BC..0x65E9D3` (six); sub-kind 0x42's
shapes `0x65E8EC` (three of twelve bytes), pieces `0x65E910` (two lists of
six), lifts `0x65E940`, page bytes `0x65E944`; the direction bytes
`0x65E97C`, `0x65E9D4`; the bound pairs `0x65E9A4`; the textures `0x65E9E4`.

## 4. The fuzz (`effect_5f_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_5f`, effect mode (`g.effect`, kind
0x18), 4,000 rounds a function (`BOF3X_E5F_ONLY=<name>` runs the clones whose
name holds it). Shapes: 46 `kEffect` (the eight dispatchers' `sub_span`
their table's length), three `kCall` (`_Draw(variant, height)`,
`_Draw(edge)`, `_MemberNear`, whose `ret_mask` is 0xFF: its caller tests
`al`'s sign). The eight tables are `DataTable`s. **Regions** beyond effect
mode's: the first 64 draw items `0x905E80` (0x2400; `MapView_ItemAt`'s
stand-in answers 0..0x3F). The area block (`AreaMap_Header` and 8 KiB) is
field mode's own.

**Callees**: the standard rows (`Flags_Test` a `kBool`, `Sound_PlayEffect`,
`Rand`, `MoveCmd_TestFB` / `FC`, `AreaMap_Elevation`, `MapView_ItemAt`,
`MapView_LinkPrimAt` moving the cursor, `Prim_SetTexture`, `Effect_Release`,
the `Gpu_*`, `Gte_PrimDepths4_10`). **The group's own** called directly: the
four void draws as `kPhase`; sub-kind 0x42's draw (two whole words), 0x3C's
(one), the party test (two words, answering 0xFF..0x02). **Re-listed**:
`Gte_RotTransPers4` - the four vertices noted (six bytes each) by its effect
rather than hashed by the row, so that while `EffectKind18Sub4B_Run` runs the
x words the original takes from its unwritten stack word are levelled to 0
on both sides first (v1's and v3's in the first quad, all four after); the
screen points and depth filled; `Gfx_CommitPrim` - the standard cursor move,
and a draw mode's commit (0xC) restarts the quad count; `DrawItemPool_Alloc`
answering 0 a third of the time, else an item of the 64 in the region.

**Seeds**: on all 20 records a start's `+0x36` inside its placement table,
for `_Mark` the cell 2..9 / 0..7 and `+8` 0..2 (its area-block index stays in
the compared 8 KiB, never on the header's own words), otherwise small cells
half the time and `+8` 0 / 1 mostly; the leader within two cells of the
record's cell half the time with the fractions at the gate's edges;
`Cond_ByteFE` 0x10 / 0x20; `Field_Request` 0 half the time; `Draw_PassFlags`
bit 2 two times in three; `+0x30` / `+0x3E` at every compare's boundary
(-0x100, 0, 0x80, 0xC0, 0x100, each step's either side); for `_Mark` the
area header 1..8 x 1..8, the base below 0x20, the cell words below 0x100 and
the items' marks 0 half the time; for `_MemberNear` a count 0..3, records in
use or not, small velocities; for `0x50A510` `+2` 0 and `+0xB` 0x4B / 0x4C
mostly and `+0x3A` at, below and past `+0x30`. **Arguments**: the draw's
variant below two, its height an s16; the edge 0 / 1 or a word; the party
test's point at a member's reach edge half the time. **Disturbance** (the
group's, from the hash only): `+8` (0 / 1), `+0x30`, `+0x3E`, `Cond_ByteFE`,
the leader's point, `+0xB`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_5f`,
exit 0): 196,000 rounds over 49 functions, 847,132 calls to the stand-ins,
**0 mismatches**; 33,972 bytes of state in 46 regions; 396 stand-ins. Every
entry of the eight tables reached (each handler recorder 616..2,676 calls,
E5E's `0x508670` 801); `Sound_PlayEffect` 8,798, `Effect_Release` 1,727,
`EffectKind18Sub42_MemberNear` 2,376, `DrawItemPool_Alloc` 3,788.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
706 self-test lines, no mismatching shadow, `inject: 8152 ours, 0 left
original`; `effect_5f` there 196,000 rounds, 847,498 calls, 0 mismatches.
**With `BOF3X_WIDE=1`**: `'*'` exit 0, 706 self-test lines, no mismatching
shadow, the same `effect_5f` counts. Neither died silently. `tools/ledger_check.py`: 70 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the cut's 48 to the byte; all 40 that
  differ from the cut differ by padding only. 0 extents differ by code.
- **Added**: `0x509A70` (400 bytes, its frame and `ret`), sub-kind 0x42's
  draw - a catalog row (part 7, unlabelled) in no group of the cut, inside
  the band, called only by sub-kind 0x42's six states. Taken whole (the
  addendum's tail-draw rule), named `EffectKind18Sub42_Draw`.
- **Labels**: the cut calls the eight dispatchers "Table
  EffectKind18_States" (right: each is an entry of it) and the rest
  "Unlabelled" but two "Area overlays" twins (`0x509850` -> PSX `0x801F35B0`,
  `0x50A510` -> `0x801F38F0`; AREA overlay copies, `pairs_propagated.json`;
  the sibling names neither). Its unit column put `0x50A3C0` with table
  `0x65E968`: it indexes `0x65E990`.
- **Hosts**: every hidden start is reached by a `.data` cell (its table
  entry or `EffectKind18_States`), none by falling through; the cut's hosts
  (`0x508BA0` E5E's, `0x509030`, `0x5094E0`, `0x509C00`, `0x50A020`,
  `0x50AA10`) are where they sit.
- **Not a function, a case or a shared tail**: none in the band.
- **Not taken, in the band**: nothing else; `0x508670` (E5E's) is entry 3 of
  sub-kind 0x2A's table.

## 6. Where ours aborts

Each where Capcom's code reads or writes past what it owns; the band's own
code keeps every one in range, so ordinary play reaches none of them unless a
spawner hands a placement index out of range:

- a dispatcher's `+2` past its table;
- a start's placement index `+0x36` (signed) outside its table (2, 3, 4, 6);
- `EffectKind18Sub42_Draw`'s variant past two, a piece's shape past three;
- `EffectKind18Sub42_MemberNear` reaching `ObjTrio` record 3
  (`Field_MemberCount` past three);
- `EffectKind18Sub42_Mark`'s draw item from `MapView_ItemAt`, or the pool's
  answer, past `draw_pool::Count()`.

## 7. Latent defects (Capcom's, described, not fixed)

- **`EffectKind18Sub4B_Run` `0x50A510` reads a stack word it never writes.**
  In its quad loop it keeps x0 in `ebp`, adds the quad's offset into `ebp`
  for the near corners (and stores that over x0's home `[esp+0x24]`), then
  reloads `ebp` from `[esp+0x20]` - a local no instruction of the function
  writes - and builds the far corners' x (v1, v3) from it; from the second
  quad on `ebp` is that word, so all four corners' x come from it. The
  original's far corners and its second and third quads therefore sit
  wherever the caller's stale stack puts them. **Ours writes x0 there** (the
  near corners' own base: v0 / v2 at `x0 + 0xA00 k - b`, v1 / v3 at
  `x0 + 0xA00 k + b`, a quad symmetric about its column), the nearest
  sensible value; the fuzz levels those words (section 4). Not seen live
  (no route); a ledger entry, if the owner wants one, is the coordinator's.
- **Sub-kind 0x2A draws nothing in its state 3** (E5E's `0x508670`, the far
  wait): the panel vanishes while it waits open, where 0x27 and 0x29 draw
  through theirs. Possibly intended; the owner's eye would say.
- **Sub-kind 0x42's waits draw nothing** when their test fails (`_WaitFlag`,
  `_WaitFlagBack` return before the draw).
- **`_WaitFlag` compares the flag's whole byte with `+8`** (`xor dl, [+8]`)
  while `_WaitFlagBack` compares `(flag == 0)` with `+8` as a word: the same
  pair of tests only while `+8` is 0 or 1 (the placements set it).
- **`EffectKind18Sub42_Mark` textures item 0** when the pool is empty
  (`DrawItemPool_Alloc` answers 0; the word stays 0 and the next frame tries
  again).
- **`EffectKind18Sub49_WaitCue` can step `+2` twice in one frame** (the cue
  and the gate both).
- **`EffectKind18Sub3C_Rise` draws after releasing its record.**
- **Unchecked indexes** (section 6).

## 8. Controls

Each a plant in `effect_5f.cpp` (anchored on a unique string), rebuilt, run
with `BOF3X_E5F_ONLY` on the clone it targets, restored and rebuilt (the
scratchpad's `controls.py`). **135 of 136 refused**, each by a count (exit 3
after a mismatch). The one not refused (113) changes a far corner's x in
`EffectKind18Sub4B_Run`, a word the fuzz levels on both sides because the
original takes it from its unwritten stack word (section 7) - equivalent by
construction; its near variant (112, the first quad's v0 x, which is
compared) is refused. **Six were not refused on the first run, the fuzz's fault**: the five gate
tests of the starts (rows 9, 10, 43, 96, 120), because the seed placed the
leader by the record's old cell while a start tests the placement's new one;
and row 108 (`>=` for `>` on `+0x3A` against `+0x30`), because `+0x3A` is
`+0x38`'s high word and the drift moved it off the seeded bound. Row 114
(the bound pair picked by `+0x36`) was refused in only 16 rounds: `+0x34`'s
low word was never 0. The seeds were fixed (section 4) and the seven re-run:
all refused, in 101..951 rounds.

| # | Function | Mutant | Result |
|--:|---|---|---|
| 1 | `EffectKind18Sub27_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 2 | `EffectKind18Sub28_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 3 | `EffectKind18Sub42_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 4 | `EffectKind18Sub29_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 5 | `EffectKind18Sub48_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 6 | `EffectKind18Sub49_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 7 | `EffectKind18Sub2A_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 8 | `EffectKind18Sub3C_Run` | `e_Current[2];` -> `e_Current[2] ? Sprite_Current[2] - 1u : 1u;` | refused (4000 rounds) |
| 9 | `EffectKind18Sub27_Start` | `Abs(across - 0x10000u) <` -> `Abs(across + 0x10000u) <` | refused (163 rounds) |
| 10 | `EffectKind18Sub2A_Start` | `Abs(across - 0x10000u) <` -> `Abs(across + 0x10000u) <` | refused (154 rounds) |
| 11 | `EffectKind18Sub29_WaitNear` | `ng = lz - (((z + 1) << 16) \| 0x8000u);` -> `ng = lz - ((z + 1) << 16);` | refused (107 rounds) |
| 12 | `EffectKind18Sub28_WaitFar` | ` << 16) \| 0x8000u);` -> ` << 16) \| 0x4000u);` | refused (199 rounds) |
| 13 | `EffectKind18Sub2A_WaitNear` | `, z_axis, 0x8000, 0x10000); ` -> `, z_axis, 0x7FFF, 0x10000); ` | refused (60 rounds) |
| 14 | `EffectKind18Sub29_WaitFar` | ` 0x20000, 0x20000); }` -> ` 0x20000, 0x1FFFF); }` | refused (106 rounds) |
| 15 | `EffectKind18Sub27_WaitNear` | `Effect)(0x200);` -> `Effect)(0x202);` | refused (97 rounds) |
| 16 | `EffectKind18Sub28_WaitNear` | `++s[2];` -> `s[2] += 2;` | refused (206 rounds) |
| 17 | `EffectKind18Sub27_Close` | `S()[2] = 1;` -> `S()[2] = 2;` | refused (1399 rounds) |
| 18 | `EffectKind18Sub2A_Close` | `S()[2] = 1;` -> `S()[2] = 2;` | refused (1645 rounds) |
| 19 | `EffectKind18Sub28_Close` | `Effect)(0x201);` -> `Effect)(0x200);` | refused (726 rounds) |
| 20 | `EffectKind18Sub27_Draw` | `reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);` -> `reinterpret_cast<float*>(p + 0x38), reinterpret_cast<float*>(p + 0x28), &depth);` | refused (4000 rounds) |
| 21 | `EffectKind18Sub28_Draw` | `epths4_10)(p);` -> `epths4_10)(p + 8);` | refused (4000 rounds) |
| 22 | `EffectKind18Sub3C_Draw` | `reinterpret_cast<const short*>(Vertex(at::kV2)), reinterpret_cast<const short*>(Vertex(at::kV3)),` -> `reinterpret_cast<const short*>(Vertex(at::kV3)), reinterpret_cast<const short*>(Vertex(at::kV2)),` | refused (4000 rounds) |
| 23 | `EffectKind18Sub27_Draw` | `v0 + 4, low - Half(e));` -> `v0 + 4, low + Half(e));` | refused (4000 rounds) |
| 24 | `EffectKind18Sub28_Draw` | `gh - Half(e));` -> `gh - Half(e) + 1);` | refused (4000 rounds) |
| 25 | `EffectKind18Sub29_Draw` | ` 0x4000) << 9; }` -> ` 0x4000) << 8; }` | refused (4000 rounds) |
| 26 | `EffectKind18Sub2A_Draw` | ` 0x3E) - 0x180u);` -> ` 0x3E) - 0x100u);` | refused (4000 rounds) |
| 27 | `EffectKind18Sub29_Draw` | `u - Half(e) - Word(S() + ` -> `u - Half(e) + Word(S() + ` | refused (4000 rounds) |
| 28 | `EffectKind18Sub2A_Draw` | `cast<U>(e)) / 2); }` -> `cast<U>(e)) >> 1); }` | refused (3615 rounds) |
| 29 | `EffectKind18Sub42_Draw` | `t, 0, 0, 0x95, 0); }` -> `t, 0, 0, 0x96, 0); }` | refused (4000 rounds) |
| 30 | `EffectKind18Sub29_Draw` | `hadeTex)(p, 0);` -> `hadeTex)(p, 1);` | refused (4000 rounds) |
| 31 | `EffectKind18Sub3C_Draw` | `)(UL(s + 0x34), UL(s + 0x38), dy, size)` -> `)(UL(s + 0x38), UL(s + 0x34), dy, size)` | refused (4000 rounds) |
| 32 | `EffectKind18Sub29_Draw` | `V0) + off, c);` -> `V0) + off, c + 1);` | refused (3712 rounds) |
| 33 | `EffectKind18Sub27_Draw` | `\| 0x1500000u, p, 1` -> `\| 0x1400000u, p, 1` | refused (1979 rounds) |
| 34 | `EffectKind18Sub27_Draw` | `nkAtRecord(-2, 0x48);` -> `nkAtRecord(-1, 0x48);` | refused (4000 rounds) |
| 35 | `EffectKind18Sub27_Draw` | `0x36) << 7) - Word(s + 0x` -> `0x36) << 7) + Word(s + 0x` | refused (2384 rounds) |
| 36 | `EffectKind18Sub27_Draw` | `0x30) - 0x4040u;` -> `0x30) - 0x4000u;` | refused (1584 rounds) |
| 37 | `EffectKind18Sub27_Draw` | `atic_cast<U>(S16(Word(s + 0x2E)))` -> `atic_cast<U>(Word(s + 0x2E))` | refused (2021 rounds) |
| 38 | `EffectKind18Sub28_Draw` | `re)(0x1780119, p, 1);` -> `re)(0x1780118, p, 1);` | refused (4000 rounds) |
| 39 | `EffectKind18Sub28_Draw` | `Prim)(6, 0x48);` -> `Prim)(6, 0x44);` | refused (4000 rounds) |
| 40 | `EffectKind18Sub28_Draw` | `0x30) - 0x3F40u;` -> `0x30) - 0x3F00u;` | refused (4000 rounds) |
| 41 | `EffectKind18Sub27_Start` | `W(at::kSub27Height + 2 * i));` -> `W(at::kSub27Texture + 2 * i));` | refused (3949 rounds) |
| 42 | `EffectKind18Sub27_Start` | ` == 0 ? 1 : 0;` -> ` == 0 ? 1 : 2;` | refused (4000 rounds) |
| 43 | `EffectKind18Sub27_Start` | ` + 0x30, 0xFF00);` -> ` + 0x30, 0xFE00);` | refused (230 rounds) |
| 44 | `EffectKind18Sub27_WaitNear` | `Near(S()[8] != 0);` -> `Near(S()[8] == 0);` | refused (278 rounds) |
| 45 | `EffectKind18Sub27_Open` | `s + 0x30)) <= -0x100) ++s` -> `s + 0x30)) < -0x100) ++s` | refused (493 rounds) |
| 46 | `EffectKind18Sub27_WaitFar` | `if (Far(s, s[8] != 0)) ++s[2];` -> `if (Far(s, false)) ++s[2];` | refused (518 rounds) |
| 47 | `EffectKind18Sub27_Close` | `s + 0x30)) >= 0) Closed()` -> `s + 0x30)) > 0) Closed()` | refused (556 rounds) |
| 48 | `EffectKind18Sub28_Start` | `+ 0x3E, 0xFA40);` -> `+ 0x3E, 0xFA00);` | refused (3915 rounds) |
| 49 | `EffectKind18Sub28_Start` | `S()[2] = 5;` -> `S()[2] = 4;` | refused (1301 rounds) |
| 50 | `EffectKind18Sub28_Start` | `if (Near(s, false)) {` -> `if (Near(s, true)) {` | refused (177 rounds) |
| 51 | `EffectKind18Sub28_Open` | `x30)) <= -0x100) ++s[2];` -> `x30)) <= -0xE0) ++s[2];` | refused (476 rounds) |
| 52 | `EffectKind18Sub28_WaitFar` | `if (Far(s, false` -> `if (!Far(s, false` | refused (4000 rounds) |
| 53 | `EffectKind18Sub28_Close` | `+ 0x20u);` -> `+ 0x10u);` | refused (3971 rounds) |
| 54 | `EffectKind18Sub28_WaitFlag` | `yFlags), 0x54) != 0) {` -> `yFlags), 0x55) != 0) {` | refused (4000 rounds) |
| 55 | `EffectKind18Sub28_WaitFlag` | `S()[2] = 2;` -> `S()[2] = 3;` | refused (2699 rounds) |
| 56 | `EffectKind18Sub42_Draw` | ` 2))) << 7) - static_cast` -> ` 2))) << 7) + static_cast` | refused (3005 rounds) |
| 57 | `EffectKind18Sub42_Draw` | `+ 0x1200u) << 16) ` -> `+ 0x1300u) << 16) ` | refused (4000 rounds) |
| 58 | `EffectKind18Sub42_Draw` | `At)(xx, zz, 1, 0xC);` -> `At)(xx, zz, 2, 0xC);` | refused (4000 rounds) |
| 59 | `EffectKind18Sub42_Draw` | `dx = B(piece), dz = B(piece + 1);` -> `dx = B(piece + 1), dz = B(piece);` | refused (4000 rounds) |
| 60 | `EffectKind18Sub42_Draw` | `>(S8(B(point + 1))) + Word` -> `>(S8(B(point))) + Word` | refused (4000 rounds) |
| 61 | `EffectKind18Sub42_Draw` | `2Lift + 2 * variant);` -> `2Lift + 2 * (variant ^ 1));` | refused (4000 rounds) |
| 62 | `EffectKind18Sub42_MemberNear` | `if (o[0] == 0) continue;` -> `if (o[0] != 1) continue;` | refused (249 rounds) |
| 63 | `EffectKind18Sub42_MemberNear` | `(o[0x70]) + 3) << 15);` -> `(o[0x70]) + 2) << 15);` | refused (364 rounds) |
| 64 | `EffectKind18Sub42_MemberNear` | `ast<U>(z)) < reach)` -> `ast<U>(z)) <= reach)` | refused (245 rounds) |
| 65 | `EffectKind18Sub42_MemberNear` | `nst U t = o[9];` -> `nst U t = o[8];` | refused (450 rounds) |
| 66 | `EffectKind18Sub42_MemberNear` | `gned char>(i);` -> `gned char>(i + 1);` | refused (473 rounds) |
| 67 | `EffectKind18Sub42_MemberNear` | `return 0xFF;` -> `return 0xFE;` | refused (3527 rounds) |
| 68 | `EffectKind18Sub42_Start` | ` At(place + 3)[0];` -> ` At(place + 2)[0];` | refused (3951 rounds) |
| 69 | `EffectKind18Sub42_Start` | `s[2] = 4;` -> `s[2] = 3;` | refused (2754 rounds) |
| 70 | `EffectKind18Sub42_Start` | `aces + 4 * i;` -> `aces + 4 * i + 4;` | refused (4000 rounds) |
| 71 | `EffectKind18Sub42_WaitFlag` | `if (static_cast<unsigned char>(set ^ s[8]) == 0) retur` -> `if (set == 0) retur` | refused (1846 rounds) |
| 72 | `EffectKind18Sub42_WaitFlag` | `+ 0x34) \| 0x8000u),` -> `+ 0x34) \| 0x4000u),` | refused (1787 rounds) |
| 73 | `EffectKind18Sub42_WaitFlag` | `if (static_cast<signed char>(who) >= 0) return;` -> `if (who == 0) return;` | refused (1203 rounds) |
| 74 | `EffectKind18Sub42_WaitFlag` | `Effect)(0x205);` -> `Effect)(0x206);` | refused (264 rounds) |
| 75 | `EffectKind18Sub42_Raise` | ` 0x3E) + 0x10u);` -> ` 0x3E) + 0x11u);` | refused (3970 rounds) |
| 76 | `EffectKind18Sub42_Raise` | `x3E)) >= 0x80) {` -> `x3E)) >= 0x81) {` | refused (635 rounds) |
| 77 | `EffectKind18Sub42_Raise` | `rd(s + 0x3A) + 1));` -> `rd(s + 0x3A)));` | refused (1670 rounds) |
| 78 | `EffectKind18Sub42_Mark` | `ew_Redraw = 2;` -> `ew_Redraw = 3;` | refused (4000 rounds) |
| 79 | `EffectKind18Sub42_Mark` | `A))) + flat + b * i;` -> `A))) + flat * i + b;` | refused (4000 rounds) |
| 80 | `EffectKind18Sub42_Mark` | `t::kItemHalfB : at::kItemHalfA)` -> `t::kItemHalfA : at::kItemHalfB)` | refused (3463 rounds) |
| 81 | `EffectKind18Sub42_Mark` | `(Word(keep) != 0) continue;` -> `(Word(keep) == 1) continue;` | refused (3006 rounds) |
| 82 | `EffectKind18Sub42_Mark` | `ItemStride, 2);` -> `ItemStride, 1);` | refused (2880 rounds) |
| 83 | `EffectKind18Sub42_Mark` | `Base) + word);` -> `Base) + word + 1);` | refused (2880 rounds) |
| 84 | `EffectKind18Sub42_Mark` | `pth) * width + 1) / 2);` -> `pth) * width) / 2);` | refused (716 rounds) |
| 85 | `EffectKind18Sub42_Mark` | `MapCellBase) * 2;` -> `MapCellBase);` | refused (2779 rounds) |
| 86 | `EffectKind18Sub42_WaitFlagBack` | `f (clear == s[8]) return;` -> `f (clear == (s[8] & 1u)) return;` | refused (389 rounds) |
| 87 | `EffectKind18Sub42_WaitFlagBack` | `s + 0x36) + 1), static_ca` -> `s + 0x36) + 2), static_ca` | refused (2391 rounds) |
| 88 | `EffectKind18Sub42_Lower` | `s + 0x3E)) <= 0) {` -> `s + 0x3E)) < 0) {` | refused (635 rounds) |
| 89 | `EffectKind18Sub42_Lower` | `S()[2] = 1;` -> `S()[2] = 2;` | refused (1645 rounds) |
| 90 | `EffectKind18Sub29_Draw` | `>(s[8]) << 21), p, 1);` -> `>(s[8]) << 20), p, 1);` | refused (2371 rounds) |
| 91 | `EffectKind18Sub29_Draw` | `ub29Slide + k)));` -> `ub29Slide + 1 - k)));` | refused (3956 rounds) |
| 92 | `EffectKind18Sub29_Draw` | `nkAtRecord(-1, 0x48);` -> `nkAtRecord(-2, 0x48);` | refused (4000 rounds) |
| 93 | `EffectKind18Sub29_Draw` | `d(s + 0x3A) - k) << 7) + ` -> `d(s + 0x3A) + k) << 7) + ` | refused (1585 rounds) |
| 94 | `EffectKind18Sub48_Start` | `u - Half(e) - W(at::kSub2` -> `u - Half(e) + W(at::kSub2` | refused (3956 rounds) |
| 95 | `EffectKind18Sub29_Start` | `ure + 4 * i));` -> `ure + 4 * i) + 1);` | refused (4000 rounds) |
| 96 | `EffectKind18Sub29_Start` | `s + 0x30, 0x80);` -> `s + 0x30, 0x70);` | refused (218 rounds) |
| 97 | `EffectKind18Sub29_Open` | `s + 0x30)) >= 0x80) ++s[2` -> `s + 0x30)) > 0x80) ++s[2` | refused (635 rounds) |
| 98 | `EffectKind18Sub29_WaitFar` | `Far(s, s[8] != 0)) ++s[2]` -> `Far(s, s[8] == 0)) ++s[2]` | refused (862 rounds) |
| 99 | `EffectKind18Sub29_Close` | `s + 0x30)) <= 0) Closed()` -> `s + 0x30)) < 0) Closed()` | refused (634 rounds) |
| 100 | `EffectKind18Sub29_WaitNear` | `StepIfNear(S()[8] != 0);` -> `StepIfNear(true);` | refused (113 rounds) |
| 101 | `EffectKind18Sub48_WaitCue` | `yteFE == 0x10) {` -> `yteFE == 0x11) {` | refused (1339 rounds) |
| 102 | `EffectKind18Sub49_WaitCue` | `0x8F) != 0) StepIfN` -> `0x8F) == 0) StepIfN` | refused (186 rounds) |
| 103 | `EffectKind18Sub49_WaitCue` | `++S()[2];` -> `S()[2] = 2;` | refused (1342 rounds) |
| 104 | `EffectKind18Sub49_WaitFar` | `ByteFE != 0x20) {` -> `ByteFE != 0x10) {` | refused (1881 rounds) |
| 105 | `EffectKind18Sub4B_Run` | `+ ((v * 3 + 6) << 9));` -> `+ ((v * 3 + 5) << 9));` | refused (2032 rounds) |
| 106 | `EffectKind18Sub4B_Run` | `(z) + (r2 & 7));` -> `(z) + (r2 & 3));` | refused (834 rounds) |
| 107 | `EffectKind18Sub4B_Run` | `PassFlags & 4) == 0) retu` -> `PassFlags & 2) == 0) retu` | refused (1964 rounds) |
| 108 | `EffectKind18Sub4B_Run` | `s + 0x3A)) > S16(Word(s ` -> `s + 0x3A)) >= S16(Word(s ` | refused (101 rounds) |
| 109 | `EffectKind18Sub4B_Run` | `+ ((2u - v) << 8)` -> `+ ((1u - v) << 8)` | refused (2608 rounds) |
| 110 | `EffectKind18Sub4B_Run` | `d(v0 + 2, c - b + z0);` -> `d(v0 + 2, c + b + z0);` | refused (2608 rounds) |
| 111 | `EffectKind18Sub4B_Run` | ` v \| 0xBB509100u;` -> ` v \| 0xBB509200u;` | refused (2434 rounds) |
| 112 | `EffectKind18Sub4B_Run` | `, x0 + a - b);` -> `, x0 + a - b + 1);` | refused (2608 rounds) |
| 113 | `EffectKind18Sub4B_Run` | `, x0 + a + b);` -> `, x0 + a + b + 1);` | not refused: equivalent (the levelled word), near variant 112 (the first quad's v0) refused |
| 114 | `EffectKind18Sub4B_Run` | `Word(s + 0x34) == 0 ? 1u ` -> `Word(s + 0x36) == 0 ? 1u ` | refused (951 rounds) |
| 115 | `EffectKind18Sub2A_Draw` | `page \| 0x25510000u, p, 1)` -> `page \| 0x25500000u, p, 1)` | refused (4000 rounds) |
| 116 | `EffectKind18Sub2A_Draw` | `:kSub2ASlide + k)));` -> `:kSub2ASlide)));` | refused (3791 rounds) |
| 117 | `EffectKind18Sub2A_Draw` | ` page = 0x127;` -> ` page = 0x128;` | refused (4000 rounds) |
| 118 | `EffectKind18Sub2A_Draw` | `<< 7) - 0x3F40u;` -> `<< 7) - 0x3F00u;` | refused (2745 rounds) |
| 119 | `EffectKind18Sub2A_Start` | `ight + 2 * i));` -> `ight + 2 * i + 2));` | refused (1982 rounds) |
| 120 | `EffectKind18Sub2A_Start` | `s + 0x30, 0xC0);` -> `s + 0x30, 0xB0);` | refused (218 rounds) |
| 121 | `EffectKind18Sub2A_Open` | `0x30)) >= 0xC0) ++s[2];` -> `0x30)) >= 0xB0) ++s[2];` | refused (680 rounds) |
| 122 | `EffectKind18Sub2A_Close` | `+ 0x30)) <= 0) Closed();` -> `+ 0x30)) <= 1) Closed();` | refused (678 rounds) |
| 123 | `EffectKind18Sub3C_Draw` | `0xFFFFFC78u - k) << 4;` -> `0xFFFFFC78u + k) << 4;` | refused (4000 rounds) |
| 124 | `EffectKind18Sub3C_Draw` | `x30) - 0x3BC0u);` -> `x30) - 0x3BC1u);` | refused (4000 rounds) |
| 125 | `EffectKind18Sub3C_Draw` | `k == 0 ? 0x180u : 0u;` -> `k == 0 ? 0x100u : 0u;` | refused (2388 rounds) |
| 126 | `EffectKind18Sub3C_Draw` | `1 + 4, 0xFB80);` -> `1 + 4, 0xFB81);` | refused (1612 rounds) |
| 127 | `EffectKind18Sub3C_Draw` | ` 1u : 0u) - 3u) * 3u) << ` -> ` 1u : 0u) - 2u) * 3u) << ` | refused (1612 rounds) |
| 128 | `EffectKind18Sub3C_Draw` | `tures + 4 * k), p, 1);` -> `tures + 4 * (1 - k)), p, 1);` | refused (4000 rounds) |
| 129 | `EffectKind18Sub3C_Draw` | `if (edge != 0) {` -> `if (edge == 1) {` | refused (792 rounds) |
| 130 | `EffectKind18Sub3C_Draw` | `Record(2, 0xC);` -> `Record(2, 0xD);` | refused (4000 rounds) |
| 131 | `EffectKind18Sub3C_Start` | `Sub3C_Draw)(0);` -> `Sub3C_Draw)(1);` | refused (4000 rounds) |
| 132 | `EffectKind18Sub3C_WaitFlag` | `0x67) != 0) {` -> `0x66) != 0) {` | refused (4000 rounds) |
| 133 | `EffectKind18Sub3C_WaitFlag` | `Sub3C_Draw)(1);` -> `Sub3C_Draw)(0);` | refused (4000 rounds) |
| 134 | `EffectKind18Sub3C_Rise` | `s + 0x30)) >= 0x100) {` -> `s + 0x30)) > 0x100) {` | refused (635 rounds) |
| 135 | `EffectKind18Sub3C_Rise` | `estFB)(9, 0xF);` -> `estFB)(9, 0xE);` | refused (1669 rounds) |
| 136 | `EffectKind18Sub3C_Rise` | `SH_CALL(Effect_Release)();` -> `;` | refused (1669 rounds) |


## 9. Calls across groups

**Outbound**: none to another group of this round (`band_rows.py --edges`:
0 edges). By name, already ours: `Flags_Test`, `Sound_PlayEffect`,
`MoveCmd_TestFB` / `FC`, `AreaMap_Elevation`, `MapView_ItemAt`,
`MapView_LinkPrimAt`, `DrawItemPool_Alloc`, `Prim_SetTexture`,
`Effect_Release`, `Gfx_CommitPrim`, `Gte_RotTransPers4`,
`Gte_PrimDepths4_10`, the `Gpu_*`; Capcom's `Rand`. Through a table: E5E's
`0x508670` (sub-kind 0x2A's entry 3, read in place; swapped for a recorder in
the fuzz). No callee is called raw.

**Inbound from outside the group**: none in code. `EffectKind18_States`
(kind 0x18's sub-kind table) holds the eight dispatchers and `0x50A510`
twice; `scenario_harness.cpp`'s `kEffectStd` lists `FX_RAW(0x509A70)` (EKH's
row: two words, the first hashed as a pointer of four bytes - it is the
variant byte, 0 or 1, so the guard logs it by value): a harness file, left
for the coordinator's fold (`FX_OURS(EffectKind18Sub42_Draw)`, `{kAll,
kAll}`, no deref).

## 10. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for every row of the band, and no `bof3x.callcounts.tsv`
under `analysis/calltrace` names an address in `0x508CC0..0x50AF8D`. **Fuzz
only.** No live run was made (the brief). A recorded walk past one of these
panels (which area is the owner's to say) would let the coordinator's
frame-hash A/B cover them.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): a comment and 48 lines,
the read extents of the 49 less `0x509A70`, whose line `00509A70 190` was
already the read extent. Six re-list host lines smaller: `00509030 4A7`
(0x25B), `005094E0 58E` (0x1AF), `00509C00 41B` (0x88), `0050A020 9EB`
(0x2B0), `0050AA10 351` (0x29D), `0050AD70 4AF` (0x21E).

## 12. The rebinding

`band_rows.py --refs`: 0 raw references to the 48 in `src/game`; a grep for
the 49 addresses and the eight tables in `src` finds only
`scenario_harness.cpp`'s `FX_RAW(0x509A70)` (section 9, a harness file: left
raw for the coordinator) and `draw_pool.cpp`'s five DIV-0062 site addresses
inside `0x509850` (byte sites, not references to the function: they stay).
**Rebound**: nothing needed. `docs/DIVERGENCE.md` DIV-0062 lists the five
sites; its text predates the owned-users list and is unchanged.
