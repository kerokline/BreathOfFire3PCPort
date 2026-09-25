# Map-cell handlers, area set-up entries, three menu draws and three event-script functions (group DD)

**Status:** IN PROGRESS (2026-09-25) - sixteen functions ours in
[`src/game/map_field_objects.cpp`](../src/game/map_field_objects.cpp). Each
was read to its last instruction, and every site holding its address was read
for what the caller does with the call. Each is fuzzed against a byte-copy of
Capcom's at start-up (`BOF3X_SHADOW=map_field_objects`, 121,988 rounds, 0
mismatches). 132 negative controls were planted, and every one was refused by a comparison (section 3.1). With `BOF3X_SHADOW='*'` the self-test exits 0
(`inject: 1278 ours`, every self-test line of every module 0 mismatches). No divergence. The live check (the shop and world-map routes)
is the coordinator's, after the merge.

This is group DD of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md), "DD"). The queue
calls it "map layers and field objects". The first nine are the map-layer
code `pe_funcs.py` folded into five functions already ours; they are reached
only through two `.data` tables, `MapCell_Handlers` and
`AreaMap_SetupHandlers` ([`map-layers.md`](map-layers.md) section 2). The
seven "field objects" are recorded starts the world-map route reaches; three
of them are **menu draws**, not field objects (section 1.3), the catalogue's
label notwithstanding.

Every claim about the binary is from capstone over `bof3/BOF3.exe` on
2026-09-25: `tools/pe_disasm.py` style disassembly by extent, and two
scratchpad scripts (`DD/ddis.py`, `DD/refs.py`) - the second finds every
`.data` / `.rdata` dword equal to an address and every `E8` / `E9` whose
target it is, and the three instructions after each call site were read.

## 1. The functions

| PC | name | code | reached from | what |
|---|---|---|---|---|
| `0x570210` | `MapCell_DrawUprights` | `0x311` | `MapCell_Handlers` 1..3, 5..0xF, 0x31..0x33, 0x35..0x3E (26 slots) | quads standing on a cell (1.1) |
| `0x570530` | `MapCell_DrawDiagonalWall` | `0x125` | `MapCell_Handlers` 4, 0x34 | a wall along a cell's diagonal (1.1) |
| `0x570A00` | `MapCell_DrawFlatFaces` | `0xA6` | `MapCell_Handlers` 0x11..0x17, 0x19..0x1F | the cell's item to `MapCell_FlatOverlay` (1.1) |
| `0x571090` | `MapCell_PatchThenStep` | `0x34` | `MapCell_Handlers` 0x24 | a patch, the kind to 0x25 (1.1) |
| `0x5710D0` | `MapCell_PatchThenStop` | `0x34` | `MapCell_Handlers` 0x25 | a patch, the kind to 0x23 (1.1) |
| `0x5712E0` | `MapCell_DrawAnimated` | `0x218` | `MapCell_Handlers` 0x27; PSX `0x80158464` (gap4) | an animated sprite on a cell (1.1) |
| `0x571880` | `AreaMap_SetupTint` | `0x6F` | `AreaMap_SetupHandlers` 0; PSX `0x80158C70` (gap4) | `Gfx_ClutAdjust` by a condition (1.2) |
| `0x571A30` | `AreaMap_SetupFlatColours` | `0x3F` | `AreaMap_SetupHandlers` 1; PSX `0x80158E78` (gap4) | the two flat colours (1.2) |
| `0x571A70` | `AreaMap_SetupTexture` | `0x71` | `AreaMap_SetupHandlers` 2; PSX `0x80158EE4` (gap4) | one texture of the area's run (1.2) |
| `0x573560` | `Menu_DrawMemberStatus` | `0x332` | 5 `E8` callers; PSX `0x801D8270` (call-anchored) | a party member's list entry (1.3) |
| `0x5744B0` | `Menu_DrawCell8` | `0x76` | 8 `E8` callers; PSX `0x801DA0D4` (gap) | one 8 x 8 SPRT (1.3) |
| `0x5746C0` | `Menu_DrawPlayTime` | `0x108` | 1 `E8` caller (`0x599DE1`) | the play-time box (1.3) |
| `0x579CF0` | `EventScript_SkipSwitch` | `0x50` + `0x2C` table | `EventScript_SkipControl` `0x579ACD`; PSX `0x801A584C` | a skipped switch (1.4) |
| `0x579F00` | `AreaMap_SetByte` | `0x27` | ~420 `E8` callers; PSX `0x801A5BC8` | one `AreaMap_Bytes` byte (1.4) |
| `0x57A3A0` | `EventOp_8x` | `0x239` | `EventScript_Op`'s table; PSX `0x801A6408` | event op 8x, an object placed (1.4) |
| `0x57C7A0` | `ScriptFlags_Clear40` | `0x16` | ~390 `E8` callers, two tail `jmp`s; PSX `0x8015CA48` | two blocking flags cleared (1.4) |

"code" is to the byte after the last instruction. Against the queue's
extents (`pe_hidden.py`, to the next start): `0x570210` 800 is 0x311 and
nops; `0x570530` 304 is 0x125; `0x571090` 64 and `0x571A30` 64 are 0x34 and
0x3F; `0x5712E0` 544 is 0x218; `0x579CF0` 188 (0xBC) is 0x7C - its body, its
jump table, four nops, and then **`EventObj_Face` `0x579D70`**, Capcom's, which
the listed extent swallowed (section 6).

**Called, not jumped to.** Every entry is reached by a `call`: the nine
table-reached ones by `DrawLayer_Open`'s `call [0x663008 + kind * 4]` and
`AreaMap_SetupEntries`' `call [0x663290 + (e >> 24 & 0x3F) * 4]` (both ours,
both through a `void`-typed pointer), the others by `E8`. No entry is a
switch case inside a host.

**eax.** The nine handlers return nothing: both their callers are ours and
discard eax (and Capcom's copies of the callers, under `BOF3X_ORIGINAL`, do
not read it either: after the call `0x56FE44` reads the record's `+2` and
loops, loading eax from its frame only after the loop; `0x571754` reads the
entry's `+2` and loads eax over it). The menu draws hand their last callee's eax on,
as the originals do: `0x59BEE9` (`Menu_DrawMemberStatus`) and `0x599C17`
(`Menu_DrawCell8`) and `0x599DE1` (`Menu_DrawPlayTime`) are `call; add esp,
n; ret`. `AreaMap_SetByte`'s eax is defined (the row offset, the byte in
al). `ScriptFlags_Clear40`'s eax is the caller's own with the new byte in
al, and two callers tail-jump to it (`0x417D49`, `0x543A5D`): ours is a
naked entry that passes eax to the C++ body (section 1.4).

### 1.1 The map-cell handlers

`DrawLayer_Open` hands each record of a cell's run to `MapCell_Handlers[record
>> 24](record, b1, b0)`, b1 and b0 the run head's bytes 1 and 0 - the cell's
map x and y ([`map-layers.md`](map-layers.md) section 2).

- **`MapCell_DrawUprights`** (26 kinds). With `e = (kind & 0xF) - 1`: the
  cell origin `((b1 - 0x81) << 7, (b0 - 0x80) << 7)` goes to the scratch
  dwords `0x903850` / `0x903854` and is read back after
  `AreaMap_Elevation((b1 << 16) - 0x10000, b0 << 16)`; `h = -(s16 elevation /
  2)` (a C divide) to `0x903858`. `Gte_RotTransPers3` projects the top
  (`h - MapCell_UprightHeights[e]`) and the base of the origin, and a third
  vertex at `Prim_VertexScratch + 0` whose x and y are whatever was left there
  (its screen point is never read). Culled unless `x0 >= -80`, `x0 <= 400`,
  `y1 >= -20` and `y0 <= 260` - x87 `fcomp` against `.rdata` `0x5C4264`,
  `0x5C4260`, `0x5C422C`, `0x5C425C`, so a NaN fails the first three and
  passes the fourth. Then `MapCell_UprightCounts[e]` times: two POLY_FT4s
  whose vertices 2 and 3 are that top and base, u / v / CLUT from the tables
  (section 2), tpage 0x1B, colour 0x80, each committed (slot 6 for kinds with
  a top nibble, else `Draw_OtSlot`); then the four outer vertices -
  `MapCell_UprightOffsetsX / Y` entry `2 (i + 3e)` and the next, about the
  origin, top and base - through `Gte_RotTransPers4` into the pair's
  vertices 0 and 1, and `Gte_StoreDepthF4`. So each pair is two planes from
  the cell's centre line out to two offsets. What the kinds depict is unread.
- **`MapCell_DrawDiagonalWall`** (kinds 4, 0x34): one POLY_FT4, 0x80 high,
  from the cell's corner (0, 1) to (1, 0), at the heights of corner bytes 2 and
  1 of `AreaMap_Corners + (width * b0 + b1) * 4` (unsigned, times -16);
  texture word `0xB1800110`.
- **`MapCell_DrawFlatFaces`** (kinds 0x11..0x17, 0x19..0x1F): the cell's
  draw item by the view ring's diagonal lookup, inline (`MapView_ItemAt`'s,
  [`field-misc.md`](field-misc.md)), and a non-zero one to
  `MapCell_FlatOverlay(kind - 0x10, item)`.
- **`MapCell_PatchThenStep` / `Stop`** (kinds 0x24, 0x25): the record's kind
  byte is rewritten - 0x24 to 0x25, 0x25 to 0x23 (`0x437CC0`, a bare `ret`) -
  and then patch-list entry `(record & 0xFFFF) + AreaMap_PatchBase` goes to
  `AreaMap_ApplyPatch`. So an 0x24 record applies its patch on two draws of
  the cell, and never after.
- **`MapCell_DrawAnimated`** (kind 0x27): unless `Area_TestCondition(word
  +0)`, one point (`(s8 +7 + (b1 - 0x80) * 64) * 2`, `(s8 +6 + (b0 - 0x80) *
  64) * 2`, word +4) through `Gte_RotTransPers`, culled to `-60..380` by
  `-150..300` and a non-zero depth `p`. The frame: `t = Frame_Counter mod byte
  +8`, `k` the first index with `t < byte (+0xA + k)`, `n = (byte +9 + 5) >>
  2`; dword `n + 2k + 2` holds four s8 corner offsets and the next dword the
  texture. The quad's corners are `x - o0 * 1125 / p`, then `+ o1 * 1125 /
  p`, and `y - o2 * 1125 / p`, then `+ o3 * 1125 / p` - x87 at the game's
  double precision, the running value kept in the register, each corner
  stored as a float. The texture goes in without bit 15; bit 30, read again
  after `Prim_SetTexture`, picks slot 6.

### 1.2 The area set-up entries

`AreaMap_SetupEntries` calls `AreaMap_SetupHandlers[(e >> 24) & 0x3F](entry)`
for each entry of the area's set-up list, once per area.

- **`AreaMap_SetupTint`** (0): `Gfx_ClutAdjust(word +4, word +6, s8 +8, +9,
  +10)` when `Area_TestCondition(word +0)` holds, else with 0, 0, 0 - the
  palette's rows, columns and a tint.
- **`AreaMap_SetupFlatColours`** (1): the area block's words `+0x1A` / `+0x1C`
  (`MapCell_FlatOverlay`'s two colours) from the high words of dwords +4 / +8
  when `Cond_ByteFF` is set (the story bit `AreaMap_SetupEntries` copies in),
  else the low words.
- **`AreaMap_SetupTexture`** (2): dword +4 (`Cond_ByteFF` set) or +8 into the
  area's texture run at `(height * width + 1) / 2 + offset + tile`, `tile` the
  map word of (byte 1, byte 0) as `MapView_CellTextures` reads it.

### 1.3 The menu draws

- **`Menu_DrawMemberStatus(x, y, member, highlight)`** - the party list's
  entry (`menu_frame.cpp`: the PSX `0x801EA99C`'s entry body). A box
  `Menu_DrawBox(x + 3, y + 3, 0x7D, 0x30, highlight, window colour)`; the
  portrait (`Menu_DrawItemIcon`, shade 1 when highlighted, else 2 with `+0x10`
  bit 7); the name (`Text_DrawAt`, 5 characters); the level, HP and AP in the
  8 px font through `Crt_sprintf` into `0x904BA0` (`"%3d"`, `"%3d/   "`,
  `"   /%3d"`); a status word through the 8 px UI font `0x516E70` (group DB's)
  - `0x66A0E8` with `+0x10` bit 7, `0x66A0F0` with bit 5, alternating on
  `Frame_Counter` bit 5 with both; the frame pieces `Menu_MemberPieces`; the
  EXP bar. It also **stores** `+0x10` bit 13: set when HP is below a quarter
  of the maximum, cleared otherwise. Colours: everything 7 when highlighted;
  else the HP 4 below a quarter and 2 at 1 or less, the maximum HP 4 when byte
  `+0x1E` is set, the AP 4 below a quarter and 2 at 0, the maximum AP 0, the
  status word 1.
- **`Menu_DrawCell8(x, y, u, v, clut, shade)`** - one SPRT_8 at `(x & 0xFFFF,
  y & 0xFFFF)`, `(u * 8, v * 8)`, opaque, committed to slot 1.
- **`Menu_DrawPlayTime(x, y)`** - the field menu's clock box: the hours and
  minutes of the play clock `0x9040C8..` in the 12 px font, a colon of two
  dots for the first 15 of each second's 30 frames, and three piece lists
  (`Menu_PlayTimePieces`).

As the originals have it, and kept:

- The colours go to `Text_DrawAt` and `Text_DrawFont8` in a dword whose upper
  three bytes are stale stack (the `sub esp, 8` local); both callees read the
  low byte (6 bits for the font). The window colour goes to `Menu_DrawBox`
  with the entry's ecx above it; it reads the byte.
- `Menu_DrawMemberStatus` passes the level to `Menu_DrawExpBar` with
  `Menu_DrawPieces`' eax above it (`mov al, [esi + 0xA]`), and
  `Menu_DrawPlayTime` computes the six middle pieces' x from `(eax & 0xFFFF0000)
  | i` (`xor ax, ax; mov al, bl`). Ours takes the callee's eax the same way.
  Neither shows: `Char_ExpForLevel` reads the level's byte, and
  `Menu_DrawPiece` draws at the low 16 bits of x, which `hi << 19` does not
  reach.

### 1.4 The event script

- **`EventScript_SkipSwitch(at)`** - `at` is on the F4 of a switch being
  skipped; from `at + 2` to one past its F5: ops below F0 by
  `EventScript_OpLengths[op >> 4]`, F6 two bytes, F7 and F8 one, F0 F1 F4 F9
  FA through `EventScript_SkipControl`, whose pointer is the new position. The
  jump table is `EventScript_SkipSwitchCases` (section 2).
- **`AreaMap_SetByte(x, z, value)`** - `AreaMap_Bytes[s16 x + s16 z * width] =
  value`, the grid the blocking tests read; the event ops stamp 0x10 through it.
- **`EventOp_8x(op)`** - places the next event object (index `0x903850`, while
  below 0x1E): Sprite_Current and `Field_ActiveMember`, the animation bank,
  its position with the half-cell flags, its elevation, bytes from the op,
  `EventObj_SetFlags`, `Sprite_SetAnimation`, and 0x10 stamped into
  `AreaMap_Bytes` at its cell and at the cells its half-cell position
  overlaps; then the index + 1. `Sprite_Current` is read again for every
  access, as the original reads `[0x937F88]`.
- **`ScriptFlags_Clear40`** - `Field_StatusBits &= ~0x40`, `Field_ScriptFlags
  &= ~0x100`; the byte read first and stored last.

## 2. Tables

The two dispatch tables are named already: `MapCell_Handlers` `0x663008`
(the "Pointer in" column's `0x66300C`.. and `0x663098`.. are its slots) and
`AreaMap_SetupHandlers` `0x663290`. Named here as `[[data]]` (`symbols.toml`,
the group's block at the end):

| PC | name | ctype, count | what |
|---|---|---|---|
| `0x66313C` | `MapCell_UprightCounts` | u8, 12 | pairs per kind index: 3 2 3 3 3 3 3 3 3 0 0 0 |
| `0x663148` | `MapCell_UprightOffsetsX` | s16, 54 | outer x offsets, 3 pairs of 2 per kind index 0..8 |
| `0x6631B4` | `MapCell_UprightOffsetsY` | s16, 54 | outer y offsets |
| `0x663220` | `MapCell_UprightHeights` | s16, 10 | the uprights' height above the ground |
| `0x663234` | `MapCell_UprightV01` | u8, 20 | v of vertices 0 and 1, per quad |
| `0x663248` | `MapCell_UprightV23` | u8, 20 | v of vertices 2 and 3, per quad |
| `0x66325C` | `MapCell_UprightU02` | u8, 12 | u of vertices 0 and 2 |
| `0x663268` | `MapCell_UprightU13` | u8, 12 | u of vertices 1 and 3 |
| `0x663274` | `MapCell_UprightClut` | u8, 12 | CLUT = (byte >> 4) \| 0x78C0 |
| `0x579D40` | `EventScript_SkipSwitchCases` | u32, 11 | `EventScript_SkipSwitch`'s jump table, F0..FA |
| `0x6632C8` | `Menu_MemberPieces` | u8, 141 | 47 pieces to an id of 0xFF |
| `0x6633B4` | `Menu_PlayTimePieces` | u8, 45 | the clock's three piece lists, back to back |

The counts of the upright tables are the gaps between them (the last ends at
`MapCell_WallTextures` `0x663280`). `Menu_MemberPieces`' first four bytes are
also the zero dword after `Kind2_StateTable` (`Kind2_Dispatch`'s state 5).

## 3. The fuzz

`src/game/map_field_objects_fuzz.cpp`, once at start-up under
`BOF3X_SHADOW=map_field_objects`. Sixteen byte-copies, every `E8` re-aimed at
a recording stand-in (`bof3::CloneCall` with `expected`; no body has a tail
`jmp` out); `EventScript_SkipSwitch`'s jump table relocated into its copy
(`move_script::Relocate`). Ours runs on the same stand-ins through
`map_field_objects::g`, `EventOp_8x`'s call of `AreaMap_SetByte` too, so each
function is tested alone. `ScriptFlags_Clear40` is called through an asm
helper that sets eax on entry, and its whole eax compared.

A round: the regions the function reads, random, with each branch's edges
seeded; Capcom's copy, then ours, from the same state, **both under the
game's x87 control word 0x027F**; compared: the stand-ins' log (count, a hash
of every entry, the first 48 kept), eax where a caller reads it, and every
region either side could write - the scratch dwords, `Prim_VertexScratch`,
`MapView_ScreenXY`, a 40 KiB packet pool, the area block's first 8 KiB, the
record, the view ring, 16 member records, the print buffer, 33 event objects,
`Sprite_Current`, `Field_ActiveMember`, the flag bytes. Everything is put
back afterwards.

The stand-ins log what the real callee reads (the colour's byte, the low
word of `Area_TestCondition`'s code, `Gfx_CommitPrim`'s two bytes, a
primitive's hash at commit and at `Prim_SetTexture`), and give back:

- screen points on each cull bound, a float step either side, far in and
  out, quiet and signalling NaNs (as bits), infinities and random bits;
- depths of 0, +-1, 1125, `0x7FFFFFFF`, `0x80000000` and random;
- elevations at the s16 edges with random upper halves;
- a packet cursor that moves by the size three times in four;
- printed text in the buffer; `Menu_DrawPieces` eaxes with and without a high
  word; `EventScript_SkipControl` landing a few bytes on.

They also disturb, a third of the time, what the caller reads after them:
the scratch dwords (uprights), the record and the screen point (animated),
the member record's `+9..+0x22`, `Frame_Counter` and the window colour (member
status), the clock (play time), the bank word, the object index, the objects
and `Sprite_Current` (op 8x). The upright tables are replaced by random ones
in a third of the rounds.

Result, 2026-09-25, `BOF3X_SELFTEST_ONLY=1`:

    MapCell_DrawUprights 8000 rounds, 316800 calls out, covered 2272 2149 5728, 0 MISMATCHES
    MapCell_DrawDiagonalWall 5000 rounds, 40000 calls out, covered 2799 0 0, 0 MISMATCHES
    MapCell_DrawFlatFaces 20000 rounds, 7494 calls out, covered 7494 0 0, 0 MISMATCHES
    MapCell_PatchThenStep 3000 rounds, 3000 calls out, covered 0 0 0, 0 MISMATCHES
    MapCell_PatchThenStop 3000 rounds, 3000 calls out, covered 0 0 0, 0 MISMATCHES
    MapCell_DrawAnimated 20000 rounds, 58150 calls out, covered 14995 4631 2322, 0 MISMATCHES
    AreaMap_SetupTint 5000 rounds, 10000 calls out, covered 3710 0 0, 0 MISMATCHES
    AreaMap_SetupFlatColours 3000 rounds, 0 calls out, covered 1966 0 0, 0 MISMATCHES
    AreaMap_SetupTexture 4988 rounds, 0 calls out, covered 3433 0 0, 0 MISMATCHES
    Menu_DrawMemberStatus 20000 rounds, 315118 calls out, covered 10199 15118 6437, 0 MISMATCHES
    Menu_DrawCell8 3000 rounds, 9000 calls out, covered 0 0 0, 0 MISMATCHES
    Menu_DrawPlayTime 5000 rounds, 67160 calls out, covered 1080 0 0, 0 MISMATCHES
    EventScript_SkipSwitch 5000 rounds, 5799 calls out, covered 2952 0 0, 0 MISMATCHES
    AreaMap_SetByte 5000 rounds, 0 calls out, covered 0 0 0, 0 MISMATCHES
    EventOp_8x 10000 rounds, 68035 calls out, covered 8892 3718 0, 0 MISMATCHES
    ScriptFlags_Clear40 2000 rounds, 0 calls out, covered 0 0 0, 0 MISMATCHES
    121988 rounds over 16 functions, 903556 calls to the stand-ins, 0 MISMATCHES

The five coverage numbers per line are: uprights drawn / two pairs or more /
projected but not drawn; walls with a top nibble; flat faces handed on; the
animated sprite's condition passed / drawn / committed to slot 6; tints with a
colour; `Cond_ByteFF` set; member entries not highlighted / with a status word
/ with bit 13 set; the colon drawn; nested skips; objects placed / with all
four cells stamped.

### 3.1 Negative controls

A scratch script (`DD/controls.py`) planted each alone in
`map_field_objects.cpp`: replace, build, self-test, restore. Each row gives
the rounds that refused it.

| | Planted | Refused in |
|---|---|---|
| U1 | Uprights: slot always Draw_OtSlot | MapCell_DrawUprights 1,204 of 8,000 |
| U2 | Uprights: e from 5 bits of the kind | MapCell_DrawUprights 3,462 of 8,000 |
| U3 | Uprights: origin x (b1 - 0x80) << 7 | MapCell_DrawUprights 7,799 of 8,000 |
| U4 | Uprights: elevation x without - 0x10000 | MapCell_DrawUprights 8,000 of 8,000 |
| U5 | Uprights: h by an arithmetic shift, not a divide | MapCell_DrawUprights 1,856 of 8,000 |
| U6 | Uprights: scratch x not re-read after the elevation | MapCell_DrawUprights 662 of 8,000 |
| U7 | Uprights: top = h + height | MapCell_DrawUprights 7,720 of 8,000 |
| U8 | Uprights: rtp3 first two vertices swapped | MapCell_DrawUprights 7,720 of 8,000 |
| U9 | Uprights: cull x0 > -80 | MapCell_DrawUprights 268 of 8,000 |
| U10 | Uprights: cull y0 upper fails a NaN | MapCell_DrawUprights 342 of 8,000 |
| U11 | Uprights: cull on y0 for y1 | MapCell_DrawUprights 1,243 of 8,000 |
| U12 | Uprights: depth 1 for depth 0 | MapCell_DrawUprights 2,272 of 8,000 |
| U13 | Uprights: the packet read once for all pairs | MapCell_DrawUprights 2,128 of 8,000 |
| U14 | Uprights: V01 not by the quad | MapCell_DrawUprights 2,268 of 8,000 |
| U15 | Uprights: CLUT from the low nibble | MapCell_DrawUprights 1,186 of 8,000 |
| U16 | Uprights: tpage 0x1A | MapCell_DrawUprights 2,272 of 8,000 |
| U17 | Uprights: offset entry by v & 1 | MapCell_DrawUprights 2,272 of 8,000 |
| U18 | Uprights: offsets 2e per kind, not 3 | MapCell_DrawUprights 1,863 of 8,000 |
| U19 | Uprights: one pair more | MapCell_DrawUprights 2,272 of 8,000 |
| U20 | Uprights: commit size 0x40 | MapCell_DrawUprights 2,272 of 8,000 |
| U21 | Uprights: depth_f4 third and fourth swapped | MapCell_DrawUprights 2,272 of 8,000 |
| U22 | Uprights: y offset from the x table | MapCell_DrawUprights 2,272 of 8,000 |
| W1 | Wall: both heights from corner byte 1 | MapCell_DrawDiagonalWall 4,968 of 5,000 |
| W2 | Wall: top 0x80 the other way | MapCell_DrawDiagonalWall 5,000 of 5,000 |
| W3 | Wall: width + 1 | MapCell_DrawDiagonalWall 4,942 of 5,000 |
| W4 | Wall: corner index by b1 rows | MapCell_DrawDiagonalWall 4,795 of 5,000 |
| W5 | Wall: texture 0xB1800111 | MapCell_DrawDiagonalWall 5,000 of 5,000 |
| W6 | Wall: depths after the texture | MapCell_DrawDiagonalWall 5,000 of 5,000 |
| W7 | Wall: slot always Draw_OtSlot | MapCell_DrawDiagonalWall 2,787 of 5,000 |
| W8 | Wall: second vertex y + 0x80 | MapCell_DrawDiagonalWall 4,987 of 5,000 |
| F1 | Flat: sum bound 0x37 | MapCell_DrawFlatFaces 1,207 of 20,000 |
| F2 | Flat: row wrap above 0x38 | MapCell_DrawFlatFaces 124 of 20,000 |
| F3 | Flat: column wrap from 0x1D | MapCell_DrawFlatFaces 316 of 20,000 |
| F4 | Flat: row without + 1 | MapCell_DrawFlatFaces 10,086 of 20,000 |
| F5 | Flat: item 11 bits | MapCell_DrawFlatFaces 3,747 of 20,000 |
| F6 | Flat: faces kind - 0x11 | MapCell_DrawFlatFaces 7,494 of 20,000 |
| F7 | Flat: rows 27 apart | MapCell_DrawFlatFaces 9,850 of 20,000 |
| F8 | Flat: origin z from x | MapCell_DrawFlatFaces 8,284 of 20,000 |
| F9 | Flat: diff bound 0x39 | MapCell_DrawFlatFaces 774 of 20,000 |
| P1 | PatchThenStep: kind 0x24 kept | MapCell_PatchThenStep 3,000 of 3,000 |
| P2 | PatchThenStop: kind 0x25 kept | MapCell_PatchThenStop 3,000 of 3,000 |
| P3 | Patch: AreaMap_PatchBase whole | MapCell_PatchThenStep 2,525 of 3,000, MapCell_PatchThenStop 2,529 of 3,000 |
| P4 | Patch: the kind stored after the call | MapCell_PatchThenStep 2,789 of 3,000, MapCell_PatchThenStop 2,994 of 3,000 |
| P5 | Patch: index with the kind byte | MapCell_PatchThenStep 2,388 of 3,000, MapCell_PatchThenStop 2,401 of 3,000 |
| A1 | Animated: the condition ignored | MapCell_DrawAnimated 5,005 of 20,000 |
| A2 | Animated: x from byte 6 | MapCell_DrawAnimated 14,937 of 20,000 |
| A3 | Animated: y cell * 32 | MapCell_DrawAnimated 13,315 of 20,000 |
| A4 | Animated: cull x > -60 | MapCell_DrawAnimated 501 of 20,000 |
| A5 | Animated: cull y >= 300 | MapCell_DrawAnimated 526 of 20,000 |
| A6 | Animated: depth 0 drawn | MapCell_DrawAnimated 113 of 20,000 |
| A7 | Animated: Frame_Counter taken signed | MapCell_DrawAnimated 524 of 20,000 |
| A8 | Animated: threshold t > byte | MapCell_DrawAnimated 210 of 20,000 |
| A9 | Animated: n = (b + 4) >> 2 | MapCell_DrawAnimated 1,533 of 20,000 |
| A10 | Animated: offsets one dword early | MapCell_DrawAnimated 4,628 of 20,000 |
| A11 | Animated: 1124 for 1125 in the first corner | MapCell_DrawAnimated 4,361 of 20,000 |
| A12 | Animated: the running x rounded to float | MapCell_DrawAnimated 1,012 of 20,000 |
| A13 | Animated: vertex 3 x from a | MapCell_DrawAnimated 4,526 of 20,000 |
| A14 | Animated: texture bit 15 kept | MapCell_DrawAnimated 2,328 of 20,000 |
| A15 | Animated: bit 30 from the dword read before the call | MapCell_DrawAnimated 2,291 of 20,000 |
| A16 | Animated: slot 7 | MapCell_DrawAnimated 2,313 of 20,000 |
| A17 | Animated: x read before the primitive calls | MapCell_DrawAnimated 572 of 20,000 |
| A18 | Animated: y corners subtract then subtract | MapCell_DrawAnimated 4,538 of 20,000 |
| T1 | Tint: the condition ignored | AreaMap_SetupTint 1,269 of 5,000 |
| T2 | Tint: green from byte 10 | AreaMap_SetupTint 3,713 of 5,000 |
| T3 | Tint: columns and rows swapped | AreaMap_SetupTint 1,269 of 5,000 |
| T4 | Tint: the words read before the condition | AreaMap_SetupTint 557 of 5,000 |
| C1 | FlatColours: the flag inverted | AreaMap_SetupFlatColours 3,000 of 3,000 |
| C2 | FlatColours: the second high word the low | AreaMap_SetupFlatColours 1,966 of 3,000 |
| X1 | SetupTexture: the source dwords swapped | AreaMap_SetupTexture 4,988 of 4,988 |
| X2 | SetupTexture: x and y swapped | AreaMap_SetupTexture 4,685 of 4,988 |
| X3 | SetupTexture: tile at offset, not offset * 2 | AreaMap_SetupTexture 4,977 of 4,988 |
| X4 | SetupTexture: half without + 1 | AreaMap_SetupTexture 1,217 of 4,988 |
| M1 | MemberStatus: box height 0x31 | Menu_DrawMemberStatus 20,000 of 20,000 |
| M2 | MemberStatus: highlight tested whole | Menu_DrawMemberStatus 10,199 of 20,000 |
| M3 | MemberStatus: shade from bit 6 | Menu_DrawMemberStatus 5,141 of 20,000 |
| M4 | MemberStatus: member & 7 | Menu_DrawMemberStatus 3,459 of 20,000 |
| M5 | MemberStatus: name count 6 | Menu_DrawMemberStatus 20,000 of 20,000 |
| M6 | MemberStatus: level format the HP one | Menu_DrawMemberStatus 20,000 of 20,000 |
| M7 | MemberStatus: flags read before the level draw | Menu_DrawMemberStatus 47 of 20,000 |
| M8 | MemberStatus: blink on Frame_Counter bit 4 | Menu_DrawMemberStatus 2,508 of 20,000 |
| M9 | MemberStatus: status colour 0 kept | Menu_DrawMemberStatus 7,703 of 20,000 |
| M10 | MemberStatus: HP half, not quarter | Menu_DrawMemberStatus 4,098 of 20,000 |
| M11 | MemberStatus: HP colour 2 only at 0 | Menu_DrawMemberStatus 2,070 of 20,000 |
| M12 | MemberStatus: max HP colour from +0x1F | Menu_DrawMemberStatus 4,942 of 20,000 |
| M13 | MemberStatus: AP colour 2 at 1 too | Menu_DrawMemberStatus 2,276 of 20,000 |
| M14 | MemberStatus: max AP in the AP colour | Menu_DrawMemberStatus 3,661 of 20,000 |
| M15 | MemberStatus: the level clean of the pieces eax | Menu_DrawMemberStatus 19,961 of 20,000 |
| M16 | MemberStatus: the member byte to the EXP bar | Menu_DrawMemberStatus 5,025 of 20,000 |
| M17 | MemberStatus: bit 13 never cleared | Menu_DrawMemberStatus 6,612 of 20,000 |
| M18 | MemberStatus: EXP read before the pieces | Menu_DrawMemberStatus 379 of 20,000 |
| M19 | MemberStatus: status 0x66A0E8 for bit 5 alone | Menu_DrawMemberStatus 5,069 of 20,000 |
| E1 | Cell8: x signed | Menu_DrawCell8 1,644 of 3,000 |
| E2 | Cell8: v from u | Menu_DrawCell8 2,901 of 3,000 |
| E3 | Cell8: semi-transparency 1 | Menu_DrawCell8 3,000 of 3,000 |
| E4 | Cell8: slot 0 | Menu_DrawCell8 3,000 of 3,000 |
| E5 | Cell8: red not set | Menu_DrawCell8 2,996 of 3,000 |
| Q1 | PlayTime: the colon to frame 15 | Menu_DrawPlayTime 500 of 5,000 |
| Q2 | PlayTime: minutes for hours | Menu_DrawPlayTime 4,981 of 5,000 |
| Q3 | PlayTime: the middle x clean of the eax | Menu_DrawPlayTime 4,921 of 5,000 |
| Q4 | PlayTime: left end at x - 0x18 | Menu_DrawPlayTime 5,000 of 5,000 |
| Q5 | PlayTime: second dot at y + 8 | Menu_DrawPlayTime 1,080 of 5,000 |
| Q6 | PlayTime: box width 0x56 | Menu_DrawPlayTime 5,000 of 5,000 |
| Q7 | PlayTime: middle pieces 9 apart | Menu_DrawPlayTime 5,000 of 5,000 |
| S1 | SkipSwitch: from +1 | EventScript_SkipSwitch 4,149 of 5,000 |
| S2 | SkipSwitch: F6 one byte | EventScript_SkipSwitch 864 of 5,000 |
| S3 | SkipSwitch: F5 returns two on | EventScript_SkipSwitch 5,000 of 5,000 |
| S4 | SkipSwitch: F7 two bytes | EventScript_SkipSwitch 901 of 5,000 |
| S5 | SkipSwitch: the control skip result dropped | EventScript_SkipSwitch 2,460 of 5,000 |
| B1 | SetByte: one byte on | AreaMap_SetByte 5,000 of 5,000 |
| B2 | SetByte: eax only the value | AreaMap_SetByte 2,815 of 5,000 |
| B3 | SetByte: rows one narrower | AreaMap_SetByte 4,934 of 5,000 |
| O1 | Op8x: bound 0x1F | EventOp_8x 389 of 10,000 |
| O2 | Op8x: bank bytes swapped | EventOp_8x 8,863 of 10,000 |
| O3 | Op8x: bank not re-read after the reset | EventOp_8x 2,907 of 10,000 |
| O4 | Op8x: half-cell x by bit 7 | EventOp_8x 2,188 of 10,000 |
| O5 | Op8x: +0x8C through Sprite_Current | EventOp_8x 1,983 of 10,000 |
| O6 | Op8x: +0x94 not zeroed | EventOp_8x 8,892 of 10,000 |
| O7 | Op8x: elevation x and z swapped | EventOp_8x 7,768 of 10,000 |
| O8 | Op8x: +2 from op[9] | EventOp_8x 8,856 of 10,000 |
| O9 | Op8x: +8 three bits | EventOp_8x 4,449 of 10,000 |
| O10 | Op8x: +0xA0 0x7E | EventOp_8x 8,892 of 10,000 |
| O11 | Op8x: flag 0x20 by op[3] bit 6 | EventOp_8x 4,161 of 10,000 |
| O12 | Op8x: +0x5C five bits | EventOp_8x 4,475 of 10,000 |
| O13 | Op8x: +0x2A from bit 5 | EventOp_8x 4,391 of 10,000 |
| O14 | Op8x: Sprite_Current not re-read after the x stamp | EventOp_8x 283 of 10,000 |
| O15 | Op8x: the index + 2 | EventOp_8x 8,892 of 10,000 |
| O16 | Op8x: flags from op + 10 | EventOp_8x 8,892 of 10,000 |
| O17 | Op8x: the diagonal stamp on either half | EventOp_8x 3,335 of 10,000 |
| O18 | Op8x: Field_ActiveMember not set | EventOp_8x 8,892 of 10,000 |
| L1 | Clear40: bit 7 cleared too | ScriptFlags_Clear40 1,003 of 2,000 |
| L2 | Clear40: eax only the byte | ScriptFlags_Clear40 2,000 of 2,000 |
| L3 | Clear40: script flag bit 9 | ScriptFlags_Clear40 1,519 of 2,000 |

**Every one of the 132 is refused by a comparison** (exit 3), none by a
crash. Five first tries were replaced because they ended the process instead
- a crash proves less than a count: W3 (the width a word) and B1 / B3 (z
unsigned; rows by the height byte) wrote past the fuzz's buffers (139); A7
(the period + 1) let the threshold scan run off the record (139); S4 (op
lengths by the low nibble) landed on an F2 / F3 and **hung** in the
original's own endless loop (124, section 5). The ones in the table are their
replacements.

**The thinnest:** M7 (47 of 20,000), A6 (113), F2 (124), A8 (210), U9 (268
of 8,000) and O14 (283 of 10,000). A15 (bit 30 taken before `Prim_SetTexture`)
was refused in 1 round of 20,000 until the texture stand-in was made to flip
that bit in the record; M7 and F2 were thinner before the member-status and
flat-face tests went from 10,000 and 3,000 rounds to 20,000.

## 4. What the fuzz does not reach

- What the callees really do: every one is a stand-in. The shop and
  world-map A/B are the test of the whole (the nine handlers draw every field
  frame; the menu draws are the field menu's).
- The originals' hangs and faults, read and not run: `EventScript_SkipSwitch`
  on F2, F3 or FB..FF (section 5), `MapCell_DrawAnimated`'s period of 0 (a
  divide fault on both sides) and its unbounded threshold scan.
- The argument slots the originals overwrite (`MapCell_DrawUprights` keeps
  its counter and two depths there, `MapCell_DrawAnimated` its depth and the
  corner offsets, `Menu_DrawMemberStatus` `x + 0x16` over `y`,
  `Menu_DrawCell8` two coordinates, `AreaMap_ApplyPatch` its result, which
  the two patch handlers pop into ecx): no caller reads them.
- The stale bytes the originals pass above a byte (section 1.3), and the
  vertices' fourth words `MapCell_DrawDiagonalWall` never writes: the callees
  read below them.
- The order of stores with no call between them.

## 5. Possible defects of the 2001 code (for the coordinator; not numbered, not fixed)

- **`EventScript_SkipSwitch` hangs on F2, F3 or FB..FF.** Its jump table
  sends F2 and F3 back to the byte they are on, and `cmp ecx, 0xA; ja`
  sends FB..FF there too; the loop never advances. A switch body being
  skipped with one of those at an op position freezes the game task. The
  PSX `0x801A584C` was not read. Latent: which scripts have them unread.
- **`MapCell_DrawUprights` indexes its tables past their nine kinds.** Kinds
  with a low nibble of 0xA..0xC (and 0x3A..0x3C) read counts 0 and draw
  nothing; 0xD..0xF (and 0x3D, 0x3E) read `MapCell_UprightOffsetsX`' bytes 9,
  0xFF and 0xBE as counts and would draw 9, 255 or 190 pairs from offsets
  well past the tables. Whether any area's cell runs use those kinds is unread
  (a scan of the area files would answer it).
- **`MapCell_DrawAnimated`** divides by its period byte unchecked and scans
  its thresholds without a bound.
- **`MapCell_PatchThenStep`** applies its patch twice (once as 0x24, once as
  0x25). Harmless if the patch is idempotent; may be deliberate.

## 6. Found on the way (other groups' addresses - said, not acted on)

- **`0x516E70`** (group DB's, the 8 px UI font) is called by
  `Menu_DrawMemberStatus` (x, y, colour, 0x10, text) and `Menu_DrawPlayTime`
  (x, y, 0, 0xFF, "."), through its raw address in
  `map_field_objects_callees.h`.
- **Seven `MapCell_Handlers` entries are nobody's:** `0x570870` (kind 0x10),
  `0x570BC0` (0x21), `0x570DE0` (0x22) and `0x4CEB40`, `0x4CED60`,
  `0x4CEFC0`, `0x4CF270` (0x28..0x2B). None is in `symbols.toml`, in
  `entries_logic.txt` or in this round's queue; none was reached by the
  routes' first-call trace, so they are for a later queue.
- **`EventObj_Face` `0x579D70`** (Capcom's, typed) is not in
  `entries_logic.txt`; the listed `00579CF0 BC` ran over it.
- The catalogue's "Field objects" label is wrong for `0x573560`, `0x5744B0`
  and `0x5746C0` (menu draws).

## 7. `entries_logic.txt`

Appended to `analysis/calltrace/entries_logic.txt` in the main checkout
(gitignored), with a comment line each:

    00570210 311
    00570530 125
    00570A00 A6
    00571090 34
    005710D0 34
    005712E0 218
    00571880 6F
    00571A30 3F
    00571A70 71
    00579CF0 7C

The last replaces `00579CF0 BC` (duplicates keep the smaller extent). The six
other recorded starts were already listed with the sizes read here
(`00573560 332`, `005744B0 76`, `005746C0 108`, `00579F00 27`, `0057A3A0
239`, `0057C7A0 16`).

## 8. For the batch check

The shop and world-map A/B (`validate_shop.sh`, `validate_worldmap.sh`)
reach all sixteen (the queue's "Routes" column: the nine handlers on both,
the rest on the world map). The first-call trace should no longer list any
of them. `BOF3X_ORIGINAL` with all sixteen names switches the group back.

## 9. symbols.toml

Thirteen new `[[func]]` entries and twelve `[[data]]` entries at the end of
the file. `EventScript_SkipSwitch`, `EventOp_8x` and `ScriptFlags_Clear40`
were typed before (2026-09-22) and are edited in place: `impl`, `status =
"evidence"` and the day's reading appended to their evidence.
