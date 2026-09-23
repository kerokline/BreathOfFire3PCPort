# Map patches, move tests, three draw handlers and four setters (group M)

**Status:** IN PROGRESS (2026-09-23) - sixteen functions ours in
[`src/game/field_misc.cpp`](../src/game/field_misc.cpp), each read to its last
instruction and against its PSX twin where one is paired, fuzzed against
Capcom's at start-up (`BOF3X_SHADOW=field_misc`, 170,000 rounds, 0 mismatches)
with 77 negative controls: 70 refused by a comparison, 3 that change nothing, 2 mis-planted and planted again, 2 refused only by a fault and replaced (section 3.1). No divergence, no defect. **Not yet through a live
check** - the shop A/B and the attract batch run centrally after the merge
(section 6).

Group M of the sixth parallel round
([`takeover-queue-round6.md`](takeover-queue-round6.md)): what the owner's shop
route (`tools/recipes/shop.txt`) reaches and the attract sequence does not, in
the map, the move script and the renderer. Fifteen from the queue and one it
missed (`AreaMap_TooSteepAt`, section 1). Call counts are the shop route's
(`analysis/calltrace/recipe_shop/bof3x.callcounts.tsv`); the attract sequence
reaches none of the sixteen.

Every claim about the binary is from capstone over `bof3/BOF3.exe`,
2026-09-23 (the scratchpad's `gM/reach.py` - reachability from the entry,
jump tables followed - `gM/sites.py` for the calls out, `gM/callers.py` and
`gM/after.py` for the E8 callers and what each does with `eax` next); the PSX
side from capstone over the sibling's `disc/SLPS_009.90` (boot EXE, loaded at
`0x80093800`) and its Ghidra decompilation of the field overlay
(`analysis/ghidra/GAME_EMI0_80196800_decomp/`).

## 1. The functions

| PC | name | size | PSX twin | shop calls | what |
|---|---|---|---|--:|---|
| `0x5183C0` | `AreaMap_BlockedNarrow` | `0x240` + table `0x1C` = `0x25C` | `FUN_801A2EE0` | 112 | the map test for a sprite of size 0 (1.1) |
| `0x5187A0` | `AreaMap_TooSteepAt` (new) | `0x13` | `FUN_801A3244` | - | `AreaMap_TooSteep(x, y)`, its `eax` passed on |
| `0x570AB0` | `MapCell_FlatOverlay` (new) | `0x110` | none paired | 3,032 | flat quads over a map cell's faces (1.4) |
| `0x571110` | `AreaMap_ApplyPatch` | `0x1CC` | `FUN_801581B8` | 60 | one entry of the area's patch list (1.3) |
| `0x5718F0` | `Gfx_ClutAdjust` (new) | `0x13F` | `FUN_80158CE8` (gap4) | 7 | the area palette's tint (1.5) |
| `0x5725C0` | `MapView_SlopeAt` (new) | `0x24` | `FUN_80155A34` | 1,277 | the slope with the height scale (1.6) |
| `0x572650` | `MoveCmd_TestFB` | `0x13A` | `FUN_80155B38` | 10 | move-script op FB: switch a cell's patches on (1.2) |
| `0x572790` | `MoveCmd_TestFC` | `0x13A` | `FUN_80155CD4` (gap4) | 7 | op FC: switch them off (1.2) |
| `0x572ED0` | `MapView_ItemAt` (new) | `0x98` | `FUN_801563AC` | 120 | a map cell's draw item (1.2) |
| `0x5A0AB0` | `D3d_DrawPolyF4` (new) | `0x185` | - | 2,368 | Direct3D handler, code `0x28` (1.7) |
| `0x5A1290` | `D3d_DrawPolyG4` (new) | `0x22F` | - | 188 | code `0x38` |
| `0x5A1A00` | `D3d_DrawLineF3` (new) | `0x14C` | - | 219 | code `0x48` |
| `0x5A75B0` | `Gpu_SetPolyF4` (new) | `0x1A` | `SetPolyF4` `0x8017B31C` | 3,032 | the libgpu setters (1.8) |
| `0x5A7610` | `Gpu_SetPolyG4` (new) | `0x1A` | `SetPolyG4` `0x8017B344` | 241 | |
| `0x5A7670` | `Gpu_SetLineF3` (new) | `0x17` | `SetLineF3` `0x8017B420` | 234 | |
| `0x5A7810` | `Gpu_SetDrawMove` (new) | `0x29` | `SetDrawMove` `0x8017B554` | 2,733 | |

Sizes are to the byte after the last reachable instruction (plus the one jump
table). **The catalogue's were right for all fifteen**; `entries_logic.txt`
(the main checkout's) is not: it lists `0x570AB0` at `0x654`, `0x571110` at
`0x60F` and `0x5718F0` at `0x1F1` (each running into its neighbours) - the
sizes above replace them. The three handlers are not in that list (the
renderer's range is excluded from the frame hash).

**Taken beyond the queue: `AreaMap_TooSteepAt` `0x5187A0`.** A two-argument
wrapper of `AreaMap_TooSteep` (ours, group G) whose only callers are ten sites
in `AreaMap_BlockedNarrow` (E8 scan, `0x51841A`..`0x5185D8`); it is not in the
shop trace's list, so its calls were never counted - the 112 of
`AreaMap_BlockedNarrow` reach it. The PSX has the same wrapper
(`FUN_801A3244`). No case label posed as a function in this group.

### 1.1 `AreaMap_BlockedNarrow`

`Field_MapBlockedAhead`'s test for a sprite whose `+0x70` is 0
([`field-blocked.md`](field-blocked.md) §1: `AreaMap_BlockedWide` for the
others). X, Y the cells (the high words of x, y), fx, fy the fractions; by
`direction & 0xFF`, less 1, through the 7-entry table `0x518600`:

| direction | cells (`AreaMap_CellBlocked`) | then, if the other axis has a fraction |
|---|---|---|
| 1 | (X, Y) | (X+1, Y); `AreaMap_TooSteepAt` at (x & ~0xFFFF, y), (x + 0x8000, y) |
| 7 | (X, Y) | (X, Y+1); at (x, y & ~0xFFFF), (x, y + 0x8000) |
| 5 | (X, Y+1 if fy else Y) | (X+1, that row); direction 1's two |
| 3 | (X+1 if fx else X, Y) | (that column, Y+1); direction 7's two |
| any other | - (1, nothing called) | |

then `AreaMap_TooSteep(x, y)`; the answers summed as a **byte** and its
non-zero-ness returned in `al`, as `AreaMap_BlockedWide` does. The same branch
for branch as the PSX `FUN_801A2EE0`. The original reads its arguments
unaligned (`[esp + 0x16]`, `[esp + 0x1A]`), so each cell dword it passes has
the neighbouring argument's bytes above it, and X + 1 is added to the whole
dword; `AreaMap_CellBlocked` reads 16 bits, so ours passes shorts. The one
caller is ours and reads `al`.

### 1.2 The view ring: `MapView_ItemAt`, `MoveCmd_TestFB` / `FC`

All three (and the cell handler `0x570A00`, Capcom's, inline) place map cell
(x', z') - relative to `MapView_Origin` - on the view's ring of 0x38 rows by
0x1C columns ([`map-scroll.md`](map-scroll.md)): row
`x' + z' + MapView_Row + 1`, column `(x' - z') / 2 + MapView_Column + 1`, each
wrapped by **one** subtraction (a head outside the grid's range lands outside
it), and nothing unless `0 <= x' + z' < 0x38` and `0 <= x' - z' < 0x38`. The
PSX twins test the same ranges with `sltiu`.

- **`MapView_ItemAt(x, y)`** returns the low 12 bits of the word at `+2` of
  that `MapView_CellItems` slot - the cell's draw item - or 0. Its x' is the
  argument dword less the origin word sign-extended, in 32 bits.
- **`MoveCmd_TestFB(x, z)` / `FC`** subtract in **16** bits (`sub ax, [..]`,
  then `movsx`). The cell's `MapView_Cells` word, 0 for none, plus the low
  half of `AreaMap_CellBase`, indexes a run of `AreaMap_Header`'s dwords whose
  count is the high half of the dword before it. Records step by their byte
  +2, in dwords, until the step lands **exactly** on the last dword, which is
  not visited (a step of 0, or one past the end, walks on - in both, and on
  the PSX). Each record of kind (top byte) `0x23` is rewritten as kind
  `0x24` at once - **by both ops** - and the patch-list entry at its low word
  plus the low half of `AreaMap_PatchBase` is switched: FB sets bit 0 of an
  entry whose `& 0xF001` is `0x8000`, FC clears it where it is `0x8001`. `al`
  is 1 when one was switched. `0x8000` / `0x8001` are `Area_TestCondition`'s
  "always false / always true" codes ([`map-scroll.md`](map-scroll.md) §1,
  `AreaMap_BakePatches`), so the ops turn a cell's conditional patches on and
  off. **The `0x24` asks the draw pass to apply them**: in `MapCell_Handlers`
  (`0x663008`, called by `DrawLayer_Open` for every record of every drawn
  cell) kind `0x23` is a bare `ret`, kind `0x24` (`0x571090`) rewrites the
  record as `0x25` and calls `AreaMap_ApplyPatch` on its entry, and kind
  `0x25` (`0x5710D0`) rewrites it as `0x23` and applies it again - once for
  each of the two display buffers' draw items, then idle.

### 1.3 `AreaMap_ApplyPatch`

One entry of the area's patch list: `AreaMap_SetupEntries`' second walk
([`map-scroll.md`](map-scroll.md) §1), the map-cell handlers `0x571090` and
`0x5710D0` (kinds `0x24` and `0x25` of `MapCell_Handlers`), and two callers at
`0x413593` / `0x41CC43`. c = `Area_TestCondition(entry word)` as a **signed**
byte (`movsx`); the length n - the entry's high word - is read after that
call. Records follow until n dwords are used (a signed compare; the last may
run past n):

| kind (top byte) | dwords | does |
|---|---|---|
| 0 | 3 | dword `+4 + 4c` into the texture run at `(height * width + 1) / 2 + offset + tile + bits 16-19` (tile the word of map (x, y): `AreaMap_BakePatches`' address), then kind 1's work |
| 1 | **2** | the face (bits 20-23) of `MapView_ItemAt(x, y)`: face 0 the item, 1 the word at its `+0x8E`, 2 and up the word at `+0x7E`; its half for this buffer handed dword `+4 + 4c` by `Prim_SetTexture(texture, half, 1)` |
| 2 | 3 | dword `+4 + 4c` to `AreaMap_Corners[width * y + x]`, `MapView_Redraw` = 2 |
| any other | 2 | the low byte of dword `+4 >> ((c << 4) & 31)` - its low half for c 0, high half for c 1 - to `AreaMap_Bytes[width * y + x]` |

x is byte 1, y byte 0; width, height and the offset (header bytes 0, 1 and
word +2) are re-read per record. A kind-1 record's `+8` is the **next**
record's head, so with c = 1 it hands `Prim_SetTexture` that head (section 5).
The PSX `FUN_801581B8` is the same, its record pointer recomputed as
`entry + 4 + used * 4` each time. The original also stores c over its own
argument slot: of the five callers, three reload `eax` and never read the
slot, and the two cell handlers `pop ecx` it and return to `DrawLayer_Open`,
which reloads `ecx` before any use - so ours does not write it.

### 1.4 `MapCell_FlatOverlay`

Called only by the cell handler `0x570A00` (`MapCell_Handlers` kinds
`0x11`..`0x17` and `0x19`..`0x1F`, the table at `0x663008`; `0x18` is a bare
`ret`), with faces = kind - `0x10` - so 1..7 and 9..15 - and the cell's draw
item when the cell is on the ring. For each of faces' bits 1, 2 and 4: a `POLY_F4` at
`Gfx_PacketNext` (read once, before the setter) made by `Gpu_SetPolyF4`; its
four corners' x and y copied from the `POLY_FT4` half for this buffer
(`DrawItems + (Gfx_BufferIndex + i * 2) * 0x48`) of the item (bit 1), the item
in its `+0x8E` word (bit 2) or its `+0x7E` word (bit 4) - read after the
setter; its colour the 15-bit word at `0x8CB59A + (faces sar 3) * 2` (area
header `+0x1A`: faces 0..7 the first colour, 8..15 the second), each channel's
five bits `<< 3`; then `Gfx_CommitPrim(Draw_OtSlot, 0x38)`. A flat, coloured
copy of up to three of the cell's quads - drawn under the Direct3D table's
`D3d_DrawPolyF4`.

### 1.5 `Gfx_ClutAdjust`

`(columns, rows, red, green, blue)`: for CLUT rows 3..14 whose bit of `rows`
is set (bit 0 row 3; `rows` halved per row, signed), each of the row's 16
CLUTs - on **row 3 only** those whose bit of `columns` is set; `columns` is
halved per CLUT of every row processed, but row 3 comes first and no other
row tests it - copied from `Gfx_ClutStripSource` to `Gfx_ClutStrip`
(`+0x4000`) with each non-zero 5-bit channel plus its offset clamped to
1..0x1F (a zero channel stays 0), bit 15 kept. Then
`Gfx_ClutStripDirty` = 1. The PSX `FUN_80158CE8` does the same at
`0x8002BE00` / `0x8002FE00`. `eax` on return is `rows` after its twelve
halvings, and ours returns it (`long`); the halvings write the caller's
argument slots, which all five callers (`0x42270A`, `0x5111A0`, `0x5718CA`,
`0x5718E2`, `0x57306A`) pop unread.

### 1.6 `MapView_SlopeAt`

`MapView_CheckHeightScale`, `AreaMap_Slope(x, y, direction)` with the three
dwords as passed, `MapView_HeightScale` = 0, and `AreaMap_Slope`'s `eax`
returned whole - the elevation counterpart is `MapView_GroundAt`. The PSX twin
sign-extends the result's low half; the PC does not, and 97 call sites get
the register.

### 1.7 The three Direct3D handlers

More of `Gfx_DrawOTag`'s Direct3D table ([`d3d-draw.md`](d3d-draw.md) §2,
entries 2, 6 and 9, called at `0x59F0C6`, `0x59F0FE`, `0x59F128`); untextured,
so the line handlers' shape: `D3d_PrimColor(r, g, b, code, Gfx_DrawTpage & 0xFFFF,
&diffuse, NULL)`, corners from `+8` as `sx`, `sy` = scale times x, y on the x87,
`sz` = z by `mov`, `rhw` = 0.1 / z, the diffuse - specular, `tu`, `tv` left as
the last draw left them - then `SetTexture(0, NULL)`, `0x437CC0` twice,
`D3d_SetBlend(code, mode read again)`, `D3d_SetShadeMode`, `DrawPrimitive`,
whose result is returned.

| handler | corners | colours | bare-ret args | shade | primitive |
|---|---|---|---|---|---|
| `D3d_DrawPolyF4` (`0x28`) | 4 of `0xC` | one, +4..+6 | 0, 0 | 1 flat | strip of 4 |
| `D3d_DrawPolyG4` (`0x38`) | 4 of `0x10` | four, +4 / +0x14 / +0x24 / +0x34, all before any corner, the code byte and mode re-read for each | 0, 0 | 2 Gouraud | strip of 4 |
| `D3d_DrawLineF3` (`0x48`) | 3 of `0xC` | one | 0, 1 | 1 | line strip of 3 |

F4 and F3 hand the colour helper their own argument slot as the diffuse
pointer and read it back; G4 uses four locals. Ours uses locals (the caller
pops the slot unread - group R's note, the same walk). The x87 sequences are
group R's (`X87Mul`, `X87Div`), copied into this file.

### 1.8 The setters

The port's primitives hold floats: each setter writes the code byte `+7` and
the float 0.01 (`0x3C23D70A`) into every corner's z, where the PSX's write
the length byte `+3` - `Gpu_SetPolyF4` code `0x28`, z at `+0x10 +0x1C +0x28
+0x34`; `Gpu_SetPolyG4` `0x38`, `+0x10 +0x20 +0x30 +0x40`; `Gpu_SetLineF3`
`0x48`, `+0x10 +0x1C +0x28` (the PSX also writes the polyline terminator
`0x55555555` at `+0x14`, where the PC's third corner lives). `Gpu_SetDrawMove
(prim, rect, x, y)`: `+4` = `0xEC000000`, `+8` / `+0xC` the rect's two dwords
(read after the code is stored), `+0x10` = x, `+0x14` = y - the walk's
`0xEC` case hands it to `Gfx_MoveImage(prim + 8, +0x10, +0x14)`; the PSX
packs x and y into one word and makes an empty rect a no-op. Each returns its
argument, as the originals leave it in `eax` (`mov eax, [esp + 4]` first).

## 2. Registers the originals leave, and what ours returns

- **`MoveCmd_TestFB` / `FC` return the original's whole `eax`.** Their 68 call
  sites (60 and 8, E8 scan) include tail returns and partial writes of `al`
  (`gM/after.py`), too many to clear one by one, so ours reproduces the
  register: the sign-extended x' or the difference with `al` cleared on the
  range exits, the column's upper half on an empty cell, the run index with
  `al` cleared when the walk is empty, and 0 or 1 after a walk. The declared
  signature (`unsigned char`, which three pointer tables of ours use) is kept:
  each entry is a naked `jmp` into a body that returns the register.
- **`MapView_SlopeAt`, `MapView_ItemAt`, `Gfx_ClutAdjust` and the setters**
  return the whole register, exactly.
- **`AreaMap_BlockedNarrow`, `AreaMap_TooSteepAt`** return `al` alone: their
  only callers (ours, and each other) read `al`.
- **`AreaMap_ApplyPatch`, `MapCell_FlatOverlay`** are `void`: every caller
  reloads `eax` or returns to `DrawLayer_Open`, which does not read it
  (`0x56FE4B`: `xor edx, edx`, then `eax` reloaded at `0x56FE62`).

## 3. The fuzz

`BOF3X_SHADOW=field_misc`, one start-up run of about 1.3 seconds. Sixteen
byte-copies; every relative call re-aimed at a recording stand-in (for ours
alike, through `field_misc::g`), `AreaMap_BlockedNarrow`'s jump table moved
into its copy; the three handlers on the vertex-block harness's fake device
(`d3d_fuzz.h`, unchanged). Per round: Capcom's copy, then ours from the same
state; compared: the calls out with their arguments, the result (`al` for the
two map tests, `eax` otherwise) and every region either side could write -
the area header block (`0x6000` bytes), `MapView_Cells`, the view heads,
`DrawItems`' first 64 items, both CLUT strips, the packet pool, the vertex
block and the primitive.

The stand-ins write what the real callees write where the caller reads it
again - `Gfx_CommitPrim` advances `Gfx_PacketNext`, `Gpu_SetPolyF4` writes its
code and z, `MapView_CheckHeightScale` sets the scale `AreaMap_Slope` is
recorded with, `AreaMap_Slope` its scratch flag - and now and then change what
the caller reads after the call: the entry's length (`Area_TestCondition`), a
record's face bits or dwords and `Gfx_BufferIndex` (`MapView_ItemAt`), the
packet cursor, `Gfx_BufferIndex`, a face word and the colour words
(`Gpu_SetPolyF4`), `Draw_OtSlot` (`Gfx_CommitPrim`), and for the handlers a
byte of the primitive, the vertex block, the scales or `Gfx_DrawTpage`.

Seeded, per function:

- **`AreaMap_BlockedNarrow`**: fractions 0, 1, `0x7FFF`, `0x8000`, `0xFFFF`;
  cells near 0 and `0x7FFF` / `0x8000` / `0xFFFF` (X + 1 wrapping); directions
  1, 3, 5, 7 half the time, 0, the evens, 8, 9, `0xFF`, any byte, with noise
  above it; answers mostly 0, then 1, `0x7F`, `0x80`, `0xFF` with stale upper
  bits, and one round in eight every non-zero answer `0x80` (the byte sum
  wraps).
- **The ring**: origins near 0, `0x7FFF`, `0x8000`, `0xFFFF`; heads in range,
  at 0 and the last row / column, at -1, a column head at -2 (a column of -1:
  the previous row's last word, or for row 0 the word before the grid, which
  the fuzz owns and empties), and for `MapView_ItemAt` past the end (its reads
  then land after the grid, inside `.data`); points on the
  diamond's edges (sums and differences -1, 0, `0x37`, `0x38`) and around it;
  argument dwords with stale upper halves (read as words by the ops) or with
  bits flipped above bit 16 (read whole by `MapView_ItemAt`).
- **The ops**: eight runs laid in the header block per round, counts 1..16
  with steps of 1..3 that end exactly on the last dword; kinds `0x23` half the
  time, else `0x24`, `0x22`, `0x63`, `0xA3`, 0, any; patch entries with
  `& 0xF001` at `0x8000`, `0x8001`, 0, 1, `0x9000`, `0xA001`, `0x4000`,
  `0xF001`; records whose entry is a run's own dword, `AreaMap_CellBase` or
  `AreaMap_PatchBase` (the loop re-reads the latter).
- **`AreaMap_ApplyPatch`**: condition answers 0 and 1, and 2 and `0xFF` (a
  signed -1) one time in eight each; lengths 0..24; kinds 0, 1, 2, 3, `0x24`,
  `0x80`, `0xFF`; faces 0, 1, 2, 3, `0xF`; widths and heights 0, 1..16; the
  entry placed where the texture-run stores can reach it; items 0 a third of
  the time.
- **`MapCell_FlatOverlay`**: faces 0..15 and the kinds' extremes (-0x10 ..
  `0xEF`, the `sar`'s negative side); the packet cursor aligned and not.
- **`Gfx_ClutAdjust`**: masks 0, 1, all ones, `0x80000001` (negative odd),
  random; offsets -32..32 and 0, ±1, `0x1F`, `-0x1F`, `0x20`, `0x7FFFFFFF`,
  `0x80000000` (the add wraps); source colours with channels forced to 0 and
  to `0x1F`.
- **The handlers**: group R's float seeds (signed zeros, denormals, infinities,
  quiet and signalling NaNs, `FLT_MAX`, 0.01, 0.99, 2^24 - 1, 1/3) and
  scales, colours 0, 1, `0x7F`, `0x80`, `0xFF`, under control words `0x027F`,
  `0x007F`, `0x037F`.
- **`Gpu_SetDrawMove`**: the rect inside the primitive (read after `+4` is
  stored) as well as outside it.

Result, 2026-09-23 (`BOF3X_SHADOW=field_misc`, and with `'*'`: exit 0, 599
ours): **170,000 rounds, 0 mismatches.** Per function (rounds / covered):
`AreaMap_BlockedNarrow` 20,000 (10,052 with direction 1, 3, 5 or 7, 43,495
calls out), `AreaMap_TooSteepAt` 2,000, `MapView_SlopeAt` 5,000,
`MapView_ItemAt` 20,000 (10,702 found an item), `MoveCmd_TestFB` /
`MoveCmd_TestFC` 10,000 each (1,106 / 1,077 switched an entry),
`AreaMap_ApplyPatch` 20,000 (11,461 set a texture, 66,491 calls out),
`MapCell_FlatOverlay` 10,000 (5,004 with bit 1), `Gfx_ClutAdjust` 2,000, the
three handlers 20,000 each, the setters 2,000 each and `Gpu_SetDrawMove`
5,000.

### 3.1 Negative controls

Each planted alone in ours by the scratchpad's `gM/runner.py` (edit, build, headless self-test, restore) from `gM/controls.py` and `gM/controls2.py`; the count is mismatching rounds per function, on the final fuzz (all were run twice, before and after the column head of -2 was seeded; the verdicts are the same except F9, refused 0 times then). Every refusal is by comparison - a call's arguments, the call count, the result or memory - none by a hang.

| # | control | refused |
|---|---|---|
| N1 | narrow: direction 5 row by fx | AreaMap_BlockedNarrow 859 |
| N2 | narrow: direction 1 fraction test on fy | AreaMap_BlockedNarrow 886 |
| N3 | narrow: half step 0x4000 | AreaMap_BlockedNarrow 3,922 |
| N4 | narrow: sum in an int, no byte wrap | AreaMap_BlockedNarrow 1,250 |
| N5 | narrow: direction masked with 7 | AreaMap_BlockedNarrow 3,844 |
| N6 | narrow: direction 3 always column X | AreaMap_BlockedNarrow 1,870 |
| N7 | narrow: X + 1 computed as X | AreaMap_BlockedNarrow 5,792 |
| N8 | narrow: direction 0 open | AreaMap_BlockedNarrow 737 |
| N9 | narrow: final slope at the cell corner | AreaMap_BlockedNarrow 7,829 |
| T1 | steep_at: arguments swapped | AreaMap_TooSteepAt 2,000 |
| S1 | slope: height scale cleared before the slope | MapView_SlopeAt 4,132 |
| S2 | slope: result sign-extended from 16 bits (the PSX) | MapView_SlopeAt 5,000 |
| S3 | slope: direction masked to a byte | MapView_SlopeAt 2,506 |
| I1 | item_at: mask 0x7FF | MapView_ItemAt 5,229 |
| I2 | item_at: origin subtracted in 16 bits | MapView_ItemAt 1,538 |
| I3 | ring: row wrapped by a modulo | MapView_ItemAt 434 |
| I4 | ring: column rounds half up | MapView_ItemAt 5,160, MoveCmd_TestFB 2,226, MoveCmd_TestFC 2,241 |
| I5 | ring: sum bound > 0x38 | MapView_ItemAt 378, MoveCmd_TestFB 118, MoveCmd_TestFC 120 |
| I6 | ring: column wrap at 0x1B | MapView_ItemAt 268, MoveCmd_TestFB 216, MoveCmd_TestFC 205 |
| F1 | tile: the 0x24 not stored back | MoveCmd_TestFB 4,047, MoveCmd_TestFC 4,035 |
| F2 | tile: step read before the stores | not refused: mis-planted (the early read was never used) - planted again as F2b |
| F3 | tile: FB switches 0x8001 on too (mask 0xF000) | MoveCmd_TestFB 885, MoveCmd_TestFC 852 |
| F4 | tile: early exit returns 0, not the spilled eax | MoveCmd_TestFB 875, MoveCmd_TestFC 861 |
| F5 | tile: PatchBase not masked to 16 bits | refused by a fault only (the unmasked high half reads 256 KB and more away) - replaced by F5b |
| F6 | tile: count read as the word at +2 (changes nothing?) | not refused: a change that changes nothing (the high half of the dword is the word at +2) |
| F7 | tile: kind tested on 7 bits | MoveCmd_TestFB 1,325, MoveCmd_TestFC 1,279 |
| F8 | tile: the origin subtracted in 32 bits | MoveCmd_TestFB 950, MoveCmd_TestFC 958 |
| F9 | tile: cell-zero exit returns 0 | MoveCmd_TestFB 9, MoveCmd_TestFC 12 |
| F10 | tile: found counted, not set | MoveCmd_TestFB 132, MoveCmd_TestFC 143 |
| C1 | FC: clears when want 0x8000 as well | MoveCmd_TestFB 1,106 |
| A1 | patch: n read before the condition | AreaMap_ApplyPatch 3,064 |
| A2 | patch: c zero-extended | AreaMap_ApplyPatch 981 |
| A3 | patch: kind 1 three dwords | AreaMap_ApplyPatch 11,713 |
| A4 | patch: face read after the lookup | AreaMap_ApplyPatch 186 |
| A5 | patch: kind-1 texture read before the lookup | AreaMap_ApplyPatch 352 |
| A6 | patch: kind 2 leaves MapView_Redraw | AreaMap_ApplyPatch 7,279 |
| A7 | patch: other kinds shift by c * 8 | AreaMap_ApplyPatch 9,694 |
| A8 | patch: kind 0 area (h * w) / 2 | AreaMap_ApplyPatch 1,982 |
| A9 | patch: kind 0 texture offset bits 16-19 dropped | AreaMap_ApplyPatch 9,242 |
| A10 | patch: face 1 uses +0x7E | AreaMap_ApplyPatch 3,255 |
| A11 | patch: loop while used <= n | AreaMap_ApplyPatch 8,141 |
| O1 | overlay: Gfx_PacketNext re-read after Gpu_SetPolyF4 | not refused: mis-planted (the re-read was never used) - planted again as O1b |
| O2 | overlay: faces >> 3 unsigned | refused by a fault only (a negative face read 2 GB away) - replaced by O2b |
| O3 | overlay: bit 2 uses +0x7E | MapCell_FlatOverlay 4,585 |
| O4 | overlay: green masked with 0xF | MapCell_FlatOverlay 4,296 |
| O5 | overlay: commit size 0x34 | MapCell_FlatOverlay 8,404 |
| O6 | overlay: face index read before Gpu_SetPolyF4 | MapCell_FlatOverlay 1,090 |
| O7 | overlay: bits 1, 2, 4, 8 | MapCell_FlatOverlay 4,608 |
| K1 | clut: clamped to 0 below | Gfx_ClutAdjust 1,634 |
| K2 | clut: columns tested on every row | Gfx_ClutAdjust 1,677 |
| K3 | clut: a zero channel adjusted too | Gfx_ClutAdjust 1,875 |
| K4 | clut: returns 0 | Gfx_ClutAdjust 199 |
| K5 | clut: bit 15 dropped | Gfx_ClutAdjust 1,876 |
| K6 | clut: halving by shift (floor) | Gfx_ClutAdjust 243 |
| K7 | clut: clamp at >= 0x1F | Gfx_ClutAdjust 1,634 |
| D1 | F4: Gouraud | D3d_DrawPolyF4 20,000 |
| D2 | G4: corner 3 coloured from corner 2 | D3d_DrawPolyG4 20,000 |
| D3 | F3: four corners drawn | D3d_DrawLineF3 20,000 |
| D4 | F4: mode read once for colour and blend | D3d_DrawPolyF4 2,760 |
| D5 | G4: code byte read once | D3d_DrawPolyG4 73 |
| D6 | F3: second bare ret gets 0 | D3d_DrawLineF3 20,000 |
| D7 | handlers: rhw in SSE | D3d_DrawPolyF4 34, D3d_DrawPolyG4 25, D3d_DrawLineF3 25 |
| D8 | handlers: sz through the x87 | D3d_DrawPolyF4 3,575, D3d_DrawPolyG4 3,610, D3d_DrawLineF3 2,741 |
| D9 | handlers: sx times the y scale | D3d_DrawPolyF4 19,186, D3d_DrawPolyG4 19,309, D3d_DrawLineF3 19,128 |
| D10 | handlers: a specular pointer | D3d_DrawLineF3 20,000 |
| D11 | F4: no SetTexture | D3d_DrawPolyF4 20,000 |
| D12 | G4: colours interleaved with corners | D3d_DrawPolyG4 5,055 |
| P1 | SetPolyF4: code 0x2C | Gpu_SetPolyF4 2,000 |
| P2 | SetPolyG4: last z at +0x3C | Gpu_SetPolyG4 2,000 |
| P3 | SetLineF3: the PSX terminator at +0x14 | Gpu_SetLineF3 2,000 |
| P4 | SetDrawMove: rect read before the code is stored | Gpu_SetDrawMove 835 |
| P5 | SetPolyF4: returns null | Gpu_SetPolyF4 2,000 |
| P6 | SetDrawMove: second rect dword read after +8 is stored (changes nothing?) | Gpu_SetDrawMove 835 |
| F2b | tile: step read before the stores (planted properly) | not refused: unobservable - neither store can change the current record's byte 2 (the 0x24 store writes byte 3, the entry store bit 0) |
| F5b | tile: PatchBase read once, before the walk | not refused: unobservable - the only entry that aliases AreaMap_PatchBase is index 0xA, which needs a base of 0xA or less, whose low half can never pass the 0x8000 / 0x8001 test |
| O1b | overlay: Gfx_PacketNext re-read after Gpu_SetPolyF4 (planted properly) | MapCell_FlatOverlay 1,693 |
| O2b | overlay: colour index faces >> 4 | MapCell_FlatOverlay 4,323 |

## 4. What the fuzz does not reach

- The real callees: `AreaMap_CellBlocked`, `AreaMap_TooSteep`,
  `Area_TestCondition`, `Prim_SetTexture`, `Gfx_CommitPrim`,
  `MapView_CheckHeightScale`, `AreaMap_Slope` and the D3D helpers are ours
  and fuzzed in their own modules; here they are recorders.
- Pixels: the handlers' vertices and calls are compared, never a surface.
- Real area data: the patch lists, runs and cell words are the fuzz's; a walk
  that does not end on its last dword is not generated (it would run into
  memory the fuzz does not own - identically on both sides).
- The x87 control word the renderer really runs under (three are checked).

## 5. Found on the way

- **A kind-1 patch record is two dwords but reads `+4 + 4c`**: with a true
  condition its texture is the next record's head (`AreaMap_ApplyPatch`, and
  the PSX alike). Whether any area has a conditional kind-1 record is not
  read; if one does, its face gets a texture word that is a record head. Kept;
  not written down as a defect without an area that reaches it.
- **Both move ops turn a kind-`0x23` record into `0x24`**, FC as well as FB
  (the PSX alike): that is the request for the draw pass to re-apply the
  patch on the next two drawn frames (section 1.2), after which the record is
  `0x23` again. Two ops on one cell before a draw leave it `0x24`, and the
  second finds no `0x23` record, so reports 0 - the patch entry was switched
  by the first all the same.
- **`Gfx_ClutAdjust`'s column mask applies to row 3 alone** (the PSX alike);
  rows 4..14 take all sixteen CLUTs.
- **`Area_TestCondition` answers above 1 exist** (map_cells' self-test counts
  2,327 in 65,536), so `AreaMap_ApplyPatch`'s `+4 + 4c` can reach `+0xC` and
  beyond, and its other kinds shift by `(c << 4) & 31`; kept, and seeded.
- `0x5A7810` is `SetDrawMove`, confirmed from both ends: the PSX twin's shape
  and the walk's `0xEC` case (`Gfx_MoveImage`).

## 6. The live check

Owed after the merge: the shop A/B (`analysis/validate_shop.sh`) reaches all
sixteen (the counts in section 1; `AreaMap_TooSteepAt` through
`AreaMap_BlockedNarrow`'s 112). `entries_logic.txt` wants the thirteen logic
entries at the sizes of section 1 (`0x5183C0` `0x25C`, `0x5187A0` `0x13`,
`0x570AB0` `0x110`, `0x571110` `0x1CC`, `0x5718F0` `0x13F`, `0x5725C0`
`0x24`, `0x572650` / `0x572790` `0x13A`, `0x572ED0` `0x98`, `0x5A75B0` /
`0x5A7610` `0x1A`, `0x5A7670` `0x17`, `0x5A7810` `0x29`). A wrong handler
would show in a capture of the shop route: `D3d_DrawPolyF4` draws the flat
overlays `MapCell_FlatOverlay` makes (3,032 in the route) and the other
`POLY_F4`s; `D3d_DrawPolyG4` / `D3d_DrawLineF3` the menus' Gouraud panels and
lines.

No DIV entry; no behaviour change.
