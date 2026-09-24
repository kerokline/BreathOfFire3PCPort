# The world map's frame, HUD and compass needle (group 1 of the world-map wave)

**Status:** IN PROGRESS (2026-09-24) - seven functions ours in
[`src/game/world_map.cpp`](../src/game/world_map.cpp), each read to its last
instruction and against its PlayStation twin (the world-map overlay off the JP
disc for five, the boot exe for two), fuzzed against Capcom's at start-up
(`BOF3X_SHADOW=world_map`, 90,000 rounds, 0 mismatches) with 47 negative
controls: 46 refused by a comparison, 1 a change that changes nothing (section
3.1). No divergence; one defect written down (D42, the dial's opacity - a port
change, kept). **Not yet through a live check** - the world-map route's A/B
runs centrally after the merge (section 7).

The first group of the wave [`world-map.md`](world-map.md) §4 queued: the
owner's order was the compass and the HUD first, with the dial's translucency
read on the way (section 5.1), and the place plates' rectangles for the
localisation build (section 5.2). Call counts are the world-map route's
(`analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv`).

Every claim about the binary is from capstone over `bof3/BOF3.exe`,
2026-09-24 (the scratchpad's `dasm.py`, `refs.py` for the E8 / E9 and dword
references, `copies.py` for the byte-identical copies); the PSX side from
capstone MIPS over the JP disc's `BIN/WORLD00/AREA016.EMI` section 13 (`dest
0x801F2C00`, 9,694 bytes, read through the sibling's `tools/emi.py` and
`text_tables.Disc` - the sibling has no Ghidra decompilation of that overlay)
and over `disc/SLPS_009.90` for the two boot-exe twins.

## 1. The functions

| PC | name | size | PSX twin | route calls | what |
|---|---|---|---|--:|---|
| `0x404160` | `WorldMap_FrameStep` | `0xC1` (the dispatcher and its three case blocks) | the `0x801F4178` family (a `jalr` through a state table) | 564 | the map task's frame: the frame's slide in and out (1.1) |
| `0x404390` | `WorldMap_DrawFrame` | `0x1C5` | `0x801F3B00` | 562 | the frame: the dial, the keyboard legend, the needle (1.2) |
| `0x404560` | `WorldMap_DrawSprite` | `0xBC` | `0x801F39D8` | 3,614 | one sprite of the dial page (1.3) |
| `0x404620` | `WorldMap_DrawHud` | `0x58` | `0x801F40C4` | 477 | the region box and its label (1.4) |
| `0x408530` | `WorldMap_DrawNeedle` | `0x196` | `0x801F3ECC` | 562 | the compass needle (1.5) |
| `0x572F70` | `MapView_ItemHalfAt` | `0x2F` | `0x80156468` (gap15) | 115,056 | a draw item's half for this buffer (1.6) |
| `0x572FA0` | `MapView_LinkPrimAt` | `0xAF` | `0x801564C4` (call-disputed, confirmed by reading) | 9,714 | a primitive linked into a map-view row (1.7) |

Sizes are to the byte after the last reachable instruction. The catalogue's
were right for five; `0x404160`'s 32 is the dispatcher alone (its cases
follow it, reached only through its table), and `entries_logic.txt` (the main
checkout's) lists `0x404160` at `0x227` and `0x404620` at `0x95D`, each
running into its neighbours - the sizes above replace them (section 7).

**The world map's code is compiled into the exe once per world-map area.**
`copies.py` (the instruction stream with every address operand masked) finds
`WorldMap_DrawSprite` eleven times byte for byte - `0x4024B0`, `0x404560`,
`0x408470`, `0x40C1F0`, `0x4105A0`, `0x411720`, `0x414F00`, `0x419540`,
`0x41B3B0`, `0x4241E0`, `0x425280` - each reading its own sprite table
(`0x5E5F98`, `0x5EF618`, `0x5F7690`, ...); the frame and the state machine
ten times each with their own button and state tables; the HUD twice
(`0x402570`); and the needle once, called from all eleven frames (the linker
folded it, as it folded the state machines' state-0 block, section 6). This
group takes the copy the owner's route runs - the Yraall region's, at
`0x404xxx`; the other ten are the same C++ over other tables, a cheap
follow-up once this one has been seen live.

### 1.1 `WorldMap_FrameStep`

`0x404150` is the map task's frame body: `call 0x404160; jmp 0x404230` - this
function, then a tail jump to the HUD's state machine (section 6). By byte
`+2` of `Sprite_Current` through the four-entry table `0x5EF5F8`:

| state | does |
|---|---|
| 0 | `0x411310`, a block eleven state tables share (Capcom's, called through a raw address): unless the map's mode byte `0x9045FA` is 2 or `Field_ScriptFlags` bit 8 is set, the word `+0x2E` (the frame's y) = -0x30 and the state 1 |
| 1 | y += 0x10; at 0x10 and above the state becomes 2; then **falls into state 2** |
| 2 | the mode byte 2 makes the state 3; `WorldMap_DrawFrame(0x10, y)` |
| 3 | y -= 0x10; at -0x30 and below the state is 0; unless the mode byte is 2 the state is 1 (over the 0); `WorldMap_DrawFrame(0x10, y)` |

So the frame slides in from y = -0x30 to 0x10 over four frames and holds;
mode 2 (the pad's `0x100` cycles the byte, [`event-ops.md`](event-ops.md))
slides it out again, and back in when the mode leaves 2. The byte is loaded
whole and indexes past the four entries into the HUD's table and the sprite
table - never reached (1 -> 2 -> 3 -> 0 is the only path), and ours ends the
process naming the byte. The y the original pushes carries a stale high half
(the object pointer's in state 3, a register's in state 2); every reader of
the frame's y takes its low word (`movsx` in the sprite draw and the needle;
the frame's `lea` offsets carry into the high half, which nothing reads), so
ours passes the word sign-extended and the fuzz's frame stand-in records the
low word.

### 1.2 `WorldMap_DrawFrame`

`(x, y)`, nothing unless `Draw_PassFlags & 0x1B`. A draw-mode primitive
`Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0)` - tpage `0x9C`: 8-bit, VRAM (768,
256), blend mode 0 - committed to slot 1; the dial at `(x, y)`; the cell the
leader stands on, `AreaMap_ByteAt(the high word of Field_Kind2X, of
Field_Kind2Z)`; then three legend rows and the needle:

| row | label | when | key |
|---|---|---|---|
| 1 | sprite 1 at `(x + 0x30, y)` | unless the cell is `0xA0`, `0xA1` or `0xAE`, or `Field_ScriptFlags2` bit 12 | the first of the **six** entries of `0x5EF670` whose mask word has a bit of the button map's word 0 (`0x903580`): its sprite byte + 1 (the lit glyph) at `(x + 0x38, y + 8)` - or, when the label is withheld, the byte itself (the dim one) |
| 2 | sprite 2 at `(x + 0x30, y + 0x10)` | unless the cell is `0xA0` or `0xA1` | word 6 (`0x90358C`) over **eight** entries, the same lit / dim rule at `(x + 0x38, y + 0x10)` |
| 3 | sprite 3 at `(x + 0x30, y + 0x18)` | when `0x531920(cell x, cell z)` answers non-zero, or flag bit 12, or the party set (`0x90412C & 0x7F`) is `0xC`, or `Field_ScriptFlags` bit 14 | none |
| - | the needle at `(x + 0x18, y + 0x18)` | always | |

The flag words are read after the calls that precede them (the fuzz's
stand-ins change them to prove it). The button table has six entries; the
second search's seventh and eighth are the state table `0x5EF688`'s words
`0x004253C0` and `0x004046A0`, so a word-6 button with bit 8, 9, 10, 12 or
14 set picks sprite `0x42` or `0x40`, past the sprite table (section 6). The
PSX twin (`0x801F3B00`) is the same branch for branch - its cell test is
`(cell + 0x60) & 0xFF < 2`, its searches `slti 6` and `slti 8` - and it picks
the tpage by `GetGraphType` (`0x22C` on types 1 and 2, `0x9C` otherwise);
the port kept `0x9C` alone. The button map: nine words at `0x903580`,
defaults at `0x656AEC` (`0023 0080 0010 0008 0004 0100 0040 0023 0040`; word
2 is `Field_MenuButton`, 7 `Field_ConfirmButtons`, 8 `Field_CancelButtons`),
save data since `Field_Init`.

### 1.3 `WorldMap_DrawSprite`

`(x, y, index)`: the draw-mode primitive again (tpage `0x9C`, slot 1), then
at `Gfx_PacketNext` - **read again after that commit** - a `SPRT`
(`Gpu_SetSprt`), `Gpu_SetSemiTrans(prim, (index & 0xFF) != 0)`, colour
`0x80 x 3`, x and y as floats of the arguments' low words at `+8` / `+0xC`,
CLUT `0x7B80` at `+0x16`, and `(w, h, u, v)` from the 4-byte table
`0x5EF618` by `index & 0xFF` at `+0x18` / `+0x1A` / `+0x14` / `+0x15`;
`Gfx_CommitPrim(1, 0x1C)`. The link and the rest of the code byte are the
setters'. The table, 22 entries:

| index | w x h at (u, v) | what |
|---|---|---|
| 0 | 112 x 48 at (0, 0) | the dial |
| 1, 2, 3 | 56 x 16 at (8, 80), 56 x 8 at (8, 96), 56 x 16 at (8, 104) | the three legend labels |
| 4, 5 | 128 x 24 at (0, 56), 8 x 24 at (0, 80) | the region box and its cap |
| 6..21 | 8 x 8 at u 64 / 72 / 80 / 88, v 80 / 88 / 96 / 104 | the key glyphs, in dim / lit pairs (the button table names the dim one) |

Nothing draws the place plates through it: the table has no plate-sized
entry (section 5.2). The one thing the PSX twin (`0x801F39D8`) does
differently is `SetSemiTrans(prim, 1)` for **every** index - section 5.1.

### 1.4 `WorldMap_DrawHud`

`(x, y)`, nothing unless `Draw_PassFlags & 0x1B`: sprite 4 at `(x, y)`, sprite
5 at `(x + 0x80, y)`, then `Text_DrawAt(x + 4, y + 4, 0, 0xFF, text)` with
the region's name at `0x803580 + the low word of the dword 0x803588` - the
area's `0x80010000` section (LoadDatFile's arena) and its word `+8`, read
after the two sprites. That is why the label is already English on the
route: the section is the stage 2 overlay's. Called with `(0x5C, word +0x30
of Sprite_Current)` from the HUD state machine's three cases (section 6), so
the box slides too. `Text_DrawAt`'s return is not read.

### 1.5 `WorldMap_DrawNeedle`

`(x, y)`, the arguments' low words: `Gte_PushMatrix`; a local `MATRIX` from
`Camera_Angles` by `Gte_RotMatrix`, its translation zeroed, `Gte_SetRotMatrix`
and `Gte_SetTransMatrix` of it; the four corners (-10, 0, 0), (0, -4, 0), (0,
4, 0), (10, 0, 0) into `Prim_VertexScratch` (their pads untouched); a
`POLY_G4` at `Gfx_PacketNext` by `Gpu_SetPolyG4`; `Gte_RotTransPers4` of the
four into the primitive's corners `+8` / `+0x18` / `+0x28` / `+0x38` with
**ten arguments** - libgte's `(v0..v3, sxy0..sxy3, p, flag)`, two stack
locals for the last two; our `Gte_RotTransPers4` declares nine and the tenth
is harmless under cdecl, but ours pushes both as the original does;
`Gte_PrimDepths4_10B(prim)` (depths 1/4096, 0, 0, 0: D41, DIV-0044 in the
backend); each corner moved by `(x - 0x9E, y - 0x76)` on the x87 - `fild` of
the integer, `fld` of the corner, `fsub`, one store, kept as inline asm;
colours red, purple, purple, blue at `+4`, `+0x14`, `+0x24`, `+0x34`;
`Gfx_CommitPrim(1, 0x44)`; `Gte_PopMatrix`. The PSX twin (`0x801F3ECC`) is
the same, its corners `sh`-moved in 16 bits. Capcom's code stays byte
faithful; the needle's fix is DIV-0044.

### 1.6 `MapView_ItemHalfAt`

`item = MapView_ItemAt(x, y)` (the whole `eax`); 0 when 0, else `DrawItems +
(Gfx_BufferIndex + item * 2) * 0x48`, the buffer index read after the call.
The PSX `0x80156468` is the same over its `0x50`-byte halves. Eleven callers:
the ten map frames' neighbours (`0x402A4D` ... `0x4258CD`, one per world map)
and two in `0x5154xx`.

### 1.7 `MapView_LinkPrimAt`

`(x, z, dy, size)`: `row = x.high + z.high - MapView_Origin's two words +
(x.low != 0) + (z.low != 0) + (dy as a signed byte) + 2`; nothing unless `0
<= row < 0x38` and the pool has room - `(Gfx_BufferIndex << 16) + 0x7F1BAC`
above `Gfx_PacketNext + (size & 0xFF)`, unsigned, `Gfx_CommitPrim`'s own
bound. Then `Gpu_LinkPrim(tail, Gfx_PacketNext)` with the row's tail for this
buffer - the dword at `0x8022C4 + (Gfx_BufferIndex + row * 6) * 8`, the `+4`
of `Gfx_FrameNodes`' record `(Gfx_BufferIndex + row * 6)` - and, with the
buffer index and `Gfx_PacketNext` **read again** after the call, the tail
slot = `Gfx_PacketNext`, `Gfx_PacketNext += size & 0xFF`. That is an append
to the map view's per-row ordering list ([`sprite-draw-order.md`](sprite-draw-order.md)
§7 read the same lists from the draw pass), by a map position rather than a
layer. The PSX `0x801564C4` is the same: `psx_pair` marked it call-disputed;
the row, the bound (`(buf * 9) << 12 + 0x8002FCC` there) and the 24-bit link
store match instruction for instruction.

## 2. Registers the originals leave, and what ours returns

- **`MapView_ItemHalfAt`** returns the whole register: the pointer or 0.
- **`MapView_LinkPrimAt`** is `void`: the original leaves the tail slot's new
  value, or the row or the bound on its exits, and its 371 callers (E8 scan)
  are draw code that reloads `eax` before any use - the fuzz compares the
  memory instead.
- **The five map functions** are `void`: `WorldMap_DrawHud`'s three callers
  `ret` at once after it; the frame's two return to `0x404150`, which jumps
  on; the sprite draw's and the needle's callers reload.

## 3. The fuzz

`BOF3X_SHADOW=world_map`, one start-up run of about a second. Seven
byte-copies; every relative call re-aimed at a recording stand-in (for ours
alike, through `world_map::g`); `WorldMap_FrameStep`'s `jmp [eax*4 +
0x5EF5F8]` re-aimed at a table of the fuzz's own whose entry 0 is the state-0
stand-in and entries 1..3 the copy's case blocks. Per round: Capcom's copy,
then ours from the same state; compared: the calls out with their arguments,
the result and every region either side could write - the fuzz's own packet
buffer and map-task object, `Gfx_PacketNext`, `Gfx_BufferIndex`, the flag
words, the party-set byte, the button words, the leader's cells, the text
offset, `Prim_VertexScratch`, `Camera_Angles`, and `Gfx_FrameNodes`' 0x38
records.

The stand-ins write what the real callees write where the caller reads it
again - `Gfx_CommitPrim` advances `Gfx_PacketNext`; the setters write their
code and z; `Gte_RotMatrix` fills the matrix (the two `Set` stand-ins record
its contents, not its stack address); `Gte_RotTransPers4` records the four
vectors' words and writes float corners from the seed list (signed zeros,
denormals, infinities, NaNs, `FLT_MAX`, 2^24 - 1, 98, 118, 32767, and plain
values) and the depth and flag through both locals - and now and then change
what the caller reads after the call: the packet cursor (moved on by 8:
`WorldMap_DrawSprite` reads it again), `Gfx_BufferIndex` (read again by
`MapView_ItemHalfAt` and `MapView_LinkPrimAt`), `Gfx_PacketNext` (again by
`MapView_LinkPrimAt`), a flag word's bit 12 or 14, the party-set byte, a
button word, the leader's cells, the text offset (all read by the frame or
the HUD after the calls that precede them), and the vertex scratch
(scrambled by `Gpu_SetPolyG4`'s stand-in: the needle must have written its
vectors before). The needle runs under control words `0x027F`, `0x007F` and
`0x037F`.

Seeded, per function:

- **`WorldMap_FrameStep`**: the state 0..3; y at -0x40, -0x31, -0x30, -0x2F,
  -0x20, 0, 0xF, 0x10, 0x11, 0x20, random; the mode byte 0..3 and any.
- **`WorldMap_DrawFrame`**: the pass flags 0, `0x1B`, `0xE4`, single bits,
  random; the cell (the stand-in's answer, stale upper bits) `0xA0`, `0xA1`,
  `0xAE`, `0x9F`, `0xA2`, `0xAF`, 0, any; the button words one of the six
  masks, none, bits 8..14 (`0x5300`, `0x4600`: the state table's words),
  several, any dword; the party set `0xC`, `0x8C`, `0xFF`, `0xB`, `0xD`, any;
  the flag words with bits 12 and 14 alone, clear, set and random; x and y
  with stale upper halves.
- **`WorldMap_DrawSprite`**: the index 0..21, the table's edges (21, 22, `0x40`,
  `0x42`, `0x43`, `0xFF`), `0x100`, `0x1FF`, `0x122`, `0x10000`, `0xFFFFFF00`
  (the low byte decides the blend and the table), any; the cursor at the
  buffer and 4 in; x and y as above.
- **`WorldMap_DrawHud`**: the text offset 0, `0xFFFF`, `0x10000`, 8, `0x1FFFF`,
  all ones (the low word counts), random; the pass flags as the frame's.
- **`WorldMap_DrawNeedle`**: the corners from the float seeds; x and y 0,
  `0x9E`, `0x76`, -1, `0x7FFF`, `0x8000`, `0xFFD0`, random with stale halves;
  the angles random; the scratch random (the pads must survive).
- **`MapView_ItemHalfAt`**: the item 0 a third of the time, else 12 bits or
  any dword; the buffer index 0, 1 and any.
- **`MapView_LinkPrimAt`**: rows aimed at -2 .. 0x39 from the origin, the
  fractions and dy (0, 1, 2, `0x7F`, `0x80`, `0xFE`, `0xFF`, any); origins
  small mostly, at the 16-bit edges one round in four; the cursor at the
  bound exactly, one byte short, one over, far past, anywhere, and short by
  up to 4 KB; sizes 0, 1, `0x1C`, `0x44`, `0xFF`, `0x100`, `0x1FF`,
  `0xFFFFFF00`, any; the buffer index 0 or 1 only (a larger one would store
  past `Gfx_FrameNodes` into the game's data).

Result, 2026-09-24 (`BOF3X_SHADOW=world_map`, and with `'*'`: exit 0, 803
ours): **90,000 rounds, 0 mismatches.** Per function (rounds / covered):
`WorldMap_FrameStep` 10,000 (7,430 drew the frame), `WorldMap_DrawFrame`
20,000 (17,170 past the pass test, 161,880 calls out), `WorldMap_DrawSprite`
10,000 (1,691 with index 0, the opaque path), `WorldMap_DrawHud` 5,000
(4,298 drew), `WorldMap_DrawNeedle` 20,000, `MapView_ItemHalfAt` 5,000 (3,330
found an item), `MapView_LinkPrimAt` 20,000 (7,110 linked).

### 3.1 Negative controls

Each planted alone in ours by the scratchpad's `controls.py` (edit, build,
headless self-test, restore); the count is mismatching rounds per function
on the final fuzz. Every refusal is by comparison - a call's arguments, the
call count or memory - none by a hang.

| # | control | refused |
|---|---|---|
| S1 | step: state 1 threshold > 0x10 | WorldMap_FrameStep 96 |
| S2 | step: state 3 threshold < -0x30 | WorldMap_FrameStep 14 |
| S3 | step: state 3 sets 1 only when it set 0 | WorldMap_FrameStep 1,098 |
| S4 | step: state 2 tests the mode for 1 | WorldMap_FrameStep 1,233 |
| S5 | step: state 1 returns instead of falling into 2 | WorldMap_FrameStep 2,570 |
| S6 | step: state 3 passes y before the subtraction | WorldMap_FrameStep 2,429 |
| F1 | frame: the third legend on flag bit 13 | WorldMap_DrawFrame 2,430 |
| F2 | frame: legend 1 not withheld on 0xAE | WorldMap_DrawFrame 858 |
| F3 | frame: the second search over six entries | WorldMap_DrawFrame 2,456 |
| F4 | frame: the first search over eight entries | WorldMap_DrawFrame 2,661 |
| F5 | frame: legend 2 withheld on 0xAE too | WorldMap_DrawFrame 1,731 |
| F6 | frame: legend 1 on flag bit 13 | WorldMap_DrawFrame 6,035 |
| F7 | frame: Field_ScriptFlags2 read before AreaMap_ByteAt | WorldMap_DrawFrame 463 |
| F8 | frame: the party set tested whole | WorldMap_DrawFrame 179 |
| F9 | frame: the needle at y + 0x10 | WorldMap_DrawFrame 17,170 |
| F10 | frame: the draw-mode commit of 0x10 | WorldMap_DrawFrame 17,170 |
| F11 | frame: the cell read before the dial | WorldMap_DrawFrame 17,170 |
| F12 | frame: the pass mask 0x1F | WorldMap_DrawFrame 1,117 |
| P1 | sprite: semi-transparent for every index (the PSX's behaviour) | WorldMap_DrawSprite 1,691 |
| P2 | sprite: the whole index decides the blend | WorldMap_DrawSprite 1,422 |
| P3 | sprite: w and h swapped | WorldMap_DrawSprite 6,329 |
| P4 | sprite: CLUT 0x7B81 | WorldMap_DrawSprite 10,000 |
| P5 | sprite: x as an unsigned word | WorldMap_DrawSprite 1,520 |
| P6 | sprite: Gfx_PacketNext not read again after the commit | WorldMap_DrawSprite 2,006 |
| P7 | sprite: colour 0x7F | WorldMap_DrawSprite 10,000 |
| H1 | hud: the text offset read before the sprites | WorldMap_DrawHud 305 |
| H2 | hud: the offset not masked to 16 bits | WorldMap_DrawHud 3,311 |
| H3 | hud: count 0xFE | WorldMap_DrawHud 4,298 |
| H4 | hud: the cap at x + 0x7F | WorldMap_DrawHud 4,298 |
| N1 | needle: dx from 0x9D | WorldMap_DrawNeedle 19,981 |
| N2 | needle: the subtraction in SSE, not the x87 sequence | not refused: unobservable - the difference of a float and a 17-bit integer is exact in 64 bits, so either way rounds once; the x87 form is kept as the original's instruction sequence |
| N3 | needle: the translation left as Gte_RotMatrix left it | WorldMap_DrawNeedle 20,000 |
| N4 | needle: the vectors written after Gpu_SetPolyG4 | WorldMap_DrawNeedle 5,025 |
| N5 | needle: corner 3 blue 0xFE | WorldMap_DrawNeedle 20,000 |
| N7 | needle: Gte_PopMatrix before the commit | WorldMap_DrawNeedle 20,000 |
| N8 | needle: dy from x | WorldMap_DrawNeedle 19,462 |
| I1 | half: stride 0x40 | MapView_ItemHalfAt 3,330 |
| I2 | half: Gfx_BufferIndex read before MapView_ItemAt | MapView_ItemHalfAt 639 |
| I3 | half: the item masked to 12 bits | MapView_ItemHalfAt 800 |
| L1 | link: row bound > 0x38 | MapView_LinkPrimAt 469 |
| L2 | link: the x fraction term dropped | MapView_LinkPrimAt 3,685 |
| L3 | link: the bound test strict | MapView_LinkPrimAt 1,751 |
| L4 | link: size not masked | MapView_LinkPrimAt 1,760 |
| L5 | link: Gfx_PacketNext not read again after Gpu_LinkPrim | MapView_LinkPrimAt 879 |
| L6 | link: Gfx_BufferIndex not read again for the store | MapView_LinkPrimAt 923 |
| L7 | link: dy zero-extended | MapView_LinkPrimAt 3,144 |
| L8 | link: + 1 | MapView_LinkPrimAt 7,579 |

S2's 14 is the boundary seeded on purpose (y = -0x20 with the mode byte 2:
the only input that tells `<` from `<=`); P1 is the divergence section 5.1
did not make, refused by every opaque-path round.

## 4. What the fuzz does not reach

- The real callees: `Gpu_SetDrawMode`, `Gfx_CommitPrim`, the setters, the GTE
  and `MapView_ItemAt` are ours and fuzzed in their own modules; the state-0
  block `0x411310` and the event exit `0x531920` are Capcom's and recorders
  here.
- Pixels: the primitives' bytes and the calls are compared, never a surface.
- Real tables: the sprite, button and state tables are read from the image
  on both sides (constant data); the `Gfx_FrameNodes` records are the
  fuzz's.
- Gfx_BufferIndex above 1 in `MapView_LinkPrimAt` (the store would leave the
  records), and a state byte above 3 in `WorldMap_FrameStep` (ours refuses
  it; the original jumps into the HUD's table).

## 5. The two readings the owner asked for

### 5.1 The dial: translucent on the PlayStation, opaque on the PC (D42)

**The cause is one operand.** The PSX sprite draw `0x801F39D8` calls
`SetSemiTrans(prim, 1)` for every sprite it draws (`addiu $a1, $zero, 1`
before the `jal`); the port's `WorldMap_DrawSprite` calls `Gpu_SetSemiTrans
(prim, index != 0)` (`test bl, bl; push 1 / push 0`). The dial is index 0, so
it is the one sprite of the page the port draws with the semi-transparency
bit clear; the legend labels, the key glyphs and the region box keep it.
Everything else in the function is the same.

**Why flipping it back does not give the PlayStation's picture.** The PSX
blends a textured semi-transparent primitive **per texel**: only a texel
whose CLUT entry has STP (bit 15) set is blended, the rest draw opaque. On
the JP disc's dial (`AREA016.EMI`'s page at `0x0A081000`, VRAM (320, 256),
through CLUT `0x7B80` = row 494, the twelfth of the `0x8002BE00` section;
the scratchpad's `dial_stp.py`): of the dial's 5,376 texels, **983 are index
0 (transparent), 1,054 have STP and 3,339 are opaque** - the glass over the
map blends, the rim and the markings do not. The three legend labels and the
region box have **no** STP texel at all: on the PSX they draw opaque even
with the bit set. The port's texture path drops STP - `Gfx_ConvertRow`'s
palettes "keep 0 as 0 and force other cells opaque"
([`tex-page.md`](tex-page.md)) - and its Direct3D blend is per primitive:
`D3d_PrimColor` gives the diffuse alpha `0x80` for code bit 1 under blend
mode 0 and `D3d_SetBlend` sets `SRCALPHA / INVSRCALPHA`
([`glyph-draw.md`](glyph-draw.md) §4). So with the PSX's operand our
backend would draw the **whole** dial at 50 % - rim, markings and glass
alike - which is not the PlayStation's dial either; and the port already
draws the legend at 50 % where the PSX draws it opaque, the same loss in the
other direction. That is, in all likelihood, why the porting house cleared
the bit for the dial: a half-transparent rim looked worse than an opaque
glass.

**Kept faithful; no DIV-0045.** The owner's rule was a switchable divergence
if the cause is a clear, small port change; the cause is, but the PSX
behaviour is not reachable from this function - it needs STP carried into
the 8-bit page's palette as alpha (a texture-path change, every
semi-transparent 8-bit sprite in the game would then blend per texel as on
the PSX) - and a whole-dial 50 % blend would be a third picture, neither
release's. Written down as D42 in [`known-defects.md`](known-defects.md);
the control P1 above is the flip, refused. The proposed real fix, for the
texture path's owner: STP into the palette's alpha (index 0 stays 0) and the
sprite handlers' blend keyed on texel alpha - then `SetSemiTrans(prim, 1)`
here becomes a one-line DIV with the PlayStation's picture behind it.

### 5.2 Where the place plates' rectangles live

**Not in this group's code.** `WorldMap_DrawSprite`'s table (1.3) holds the
dial, three labels, the box and sixteen 8 x 8 glyphs - no 44 / 60 / 76 x 14
entry - and the frame and HUD draw indices 0..5 and the glyphs alone. The
plates are drawn by something else on the route: the map layer itself, or
the map's objects; that draw is unread (the candidates are what this group's
two helpers serve - the route calls `MapView_ItemHalfAt` 115,056 times and
`MapView_LinkPrimAt` 9,714 - and the field-object path).

**From the data, the rectangles are in the area's `0x800D3800` section, and
the map data is not localised.** `AREA016.EMI`'s fourteen sections on the JP
and US discs, hashed (the scratchpad's `area_secs.py` through the sibling's
tools):

| idx | dest | bytes JP / US | JP vs US |
|---|---|---|---|
| 0, 1, 2 | audio (VH, ?, VB) | 4,128 / 84 / 184,640 | same |
| 3 | `0x800E3800` | 60 | same |
| 4 | `0x800D3800` | 2,700 / **2,708** | **differs** (below) |
| 5 | `0x0E001000` the map page (the plates) | 262,144 | differs |
| 6 | `0x8002D800` (US `0x80035800`) | 1,536 | differs: 104 of the first 256 words (one CLUT of three) |
| 7 | `0x0A081000` the dial page | 262,144 | differs (the legend's labels) |
| 8 | `0x8002BE00` (US `0x80033E00`) the twelve CLUTs | 6,144 | same |
| 9 | `0x8002A000` (US `0x80032000`) | 5,000 | same |
| 10 | `0x800E4000` | 1,160 | same |
| 11 | `0x80010000` the text | 1,340 / 1,959 | differs |
| 12 | `0x80104000` the map (the sibling's node table) | 53,324 | **same** |
| 13 | `0x801F2C00` the overlay code | 9,694 / 9,698 | differs by four bytes |

So whatever the map's cells and draw records say about texture rectangles is
identical on both discs: the US plates, hand-lettered to other widths, are
reached through something else. Section 4 is it: 2,700 bytes on the JP disc
and 2,708 on the US, a six-dword header of offsets (`0x10C, 0x3BC, 0x590,
0x760, 0x898, 0x900`, each **+8** on the US disc) over a table of word
offsets to variable-length records (`0x18, 0x24, 0x30, 0x42, 0x54 / 0x58,
...`: two records four bytes longer on the US disc, one cell each) whose
bodies look like sprite frame records (`01 01 0a 00 02 00 01 05 f8 f8 00 00`
- a cell count, cells with -8 / -8 offsets). The sibling's JP / US census
found `0x800D3800` "changing *size* in ten area files"
(`regional-builds.md`) - the ten world maps are ten files - and left what it
is open; this is what it is: the map's object poses, the plates' cells among
them, sized per plate. On the PC it is the kind-0 chunk tagged `0xB0000`
([`DAT_CONTAINER.md`](DAT_CONTAINER.md): dest `0x800D3800` + `0x80023800`),
loaded at `0x803580 + 0xB0000 = 0x8B3580`.

**For the localisation build** ([`world-map.md`](world-map.md) §5): an
`en.AREA016.DAT` wants the US disc's `0x0E001000` page **and** its
`0x800D3800` section (the offsets inside it are section-relative, so it goes
in whole), and possibly the first CLUT of `0x8002D800` if the US plates use
it; the map section `0x80104000` stays. Two things are still owed to
data before that is safe: a read of the PC's `0xB0000` chunk against the JP
section (the same format, or the port's own), and the plate draw itself (a
capture of `f01260` after the swap answers it in one look).

## 6. Found on the way

- **Eleven copies of the world map's code** (section 1): one per world-map
  area overlay, compiled into the exe, the state-0 block and the needle
  folded onto one address, everything else per copy with its own tables.
  The other ten are the same functions over other tables - take them
  together, after the live check of this copy.
- **Three state machines in the map task's frame.** `0x404150`: `call
  0x404160` (this group's `WorldMap_FrameStep`, byte +2), then `jmp
  0x404230` - the HUD's, byte +3, table `0x5EF608`: state 0 `0x414BB0`
  (shared by eleven: the box's y `+0x30` = 0xF0 and state 1 unless the mode
  byte is set with the byte `+0xB`, or `Field_Request` is 2, or flag bit 8),
  1 `0x404250` (y -= 10 down to 0xC8, then state 2; `WorldMap_DrawHud(0x5C,
  y)` at the end of every case), 2 `0x4042C0` (holds 0x5A frames by the
  counter `+9` while `+0xB` is clear, then state 3 on the mode or the
  request), 3 `0x404330` (y += 10 up to 0xF0, then 0). A third dispatcher
  `0x404680` runs on byte +1 through the table `0x5EF688` (entries
  `0x4253C0`, `0x4046A0`, `0x40C490`, ...). None of the three is in the
  queue (pointer- and jump-reached: `pe_hidden.py`'s blind spot) and none is
  bound here. The HUD machine is the natural next take: sixty-odd bytes a
  case, the same shape as 1.1.
- **The frame's second key search overreads the button table**: eight
  entries where the table has six; the seventh and eighth are the state
  table's words (1.2). A word-6 button with a bit in `0x5300` or `0x4600`
  would draw sprite `0x42` or `0x40` - a rectangle read past the sprite
  table at `0x5EF720` or `0x5EF718`. The defaults and the measured saves
  keep the button in the low byte; the PSX has the same `slti 8`. Kept, and
  seeded; not written down as a defect without a save that reaches it.
- **`Gte_RotTransPers4` is called with ten arguments** here (the needle,
  1.5), as libgte's takes; our implementation reads nine. Harmless under
  cdecl; the fuzz's stand-in records that both locals are handed over.
- **The region label's text** comes from the area's `0x80010000` section
  through its word `+8` - the stage 2 overlay carries it, which is why the
  route's label is English already.
- **`0x9045FA` is the world map's mode byte**: 2 slides the frame and the HUD
  off screen and holds them there (1.1); the pad's `0x100` cycles it
  ([`event-ops.md`](event-ops.md)). A name for it waits on its writer.
- **The US disc moves the CLUT sections up 0x8000** (`0x8002BE00` ->
  `0x80033E00`, `0x8002D800` -> `0x80035800`, `0x8002A000` -> `0x80032000`)
  while the code and map sections keep their addresses (5.2's table).

## 7. The live check

Owed after the merge: the world-map route's A/B (`analysis/validate_worldmap.sh`)
reaches all seven (the counts in section 1). `entries_logic.txt` wants the
seven at the sizes of section 1 (`0x404160` `0xC1`, `0x404390` `0x1C5`,
`0x404560` `0xBC`, `0x404620` `0x58`, `0x408530` `0x196`, `0x572F70` `0x2F`,
`0x572FA0` `0xAF`) - `0x404160`'s `0x227` and `0x404620`'s `0x95D` there run
into the HUD machine and the neighbours. What a wrong function would show
in the captures: the dial, the legend and its key glyphs (`f01140`..
`f01380`, `f01620`..`f01740`), the region box and its label, the needle
(DIV-0044 on), and - through `MapView_LinkPrimAt` - the map's own draw
order.

No DIV entry; no behaviour change.

## 8. Traps met

- **A control runner that restores the source but not the build leaves the
  last bug's dll in place**: the `'*'` self-test run after the controls
  reported `MapView_LinkPrimAt` 7,579 mismatches - control L8's count
  exactly - until the tree was rebuilt. Rebuild after the restore, and
  check the count against the controls' before believing it.
- **A stand-in that draws from the shared random stream is not
  deterministic across the two sides**: `Next()` inside a stand-in gave the
  clone one float and ours another (20,000 needle mismatches, 800 in
  `MapView_ItemHalfAt`, on the first run). Everything a stand-in makes up
  comes from `Mix` (the round and the log position).
- `tools/pe_disasm.py` opens `bof3/BOF3.exe` by a relative path: from a
  worktree, copy it with an absolute one.
- The sibling's `analysis/pairs_propagated.json` pairs nothing in the world
  map's overlay family (it holds no decompilation of `0x801F2C00`); the
  twins came from disassembling the section straight off the JP disc, which
  its `tools/emi.py` and `text_tables.Disc` make a ten-line script.
