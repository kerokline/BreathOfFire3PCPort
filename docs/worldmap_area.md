# The world map's area code and the effect kinds it runs

**Status:** IN PROGRESS (2026-09-25)

Group DA of the eighth round ([`takeover-queue-round8.md`](takeover-queue-round8.md)).
**Thirty of the queue's thirty-one functions are ours** (`src/game/worldmap_area.cpp`).
Each was read to its last instruction with capstone over `bof3/BOF3.exe` on
2026-09-25. The fuzz ran each against a byte-copy of Capcom's with every call
out re-aimed at a recorder: 60,000 rounds, 0 mismatches. 97 negative controls
were planted and all 97 were refused (section 7).

The one left is `0x404180`. It is a case block of `WorldMap_FrameStep`, which
has been ours since round seven (section 9).

Every function is a faithful replacement, so no `DIVERGENCE.md` entry is owed.
DIV-0041 (widescreen) and DIV-0044 (the needle) patch none of these functions.
The box states call `WorldMap_DrawHud`, which is already ours, so they pass
through it unchanged. No cheat patches any of the addresses (`grep` of
`src/game/*.cpp`, 2026-09-25).

Nearly all of the group is pointer-reached. The tables that reach it are named
in `symbols.toml` (section 5).

## 1. The functions

"Size" is the extent read to the last instruction. `sites.py` (in the
session scratchpad's `DA/`) decodes each extent exactly, and every jump stays
inside it.

| PC | Name | Size | Reached through | Route | PSX |
|---|---|--:|---|---|---|
| `0x4037B0` | `Area29_PickFieldObject` | 0xCB | area 29's descriptor `+0x40` (init), from `Area_Enter` | combat | `0x801F2D2C` |
| `0x403CE0` | `Area33_ClearCellsA` | 0x5F | `Area33_Handlers` 0 | worldmap | `0x801F2C38` |
| `0x403D40` | `Area33_ClearCellsB` | 0x25 | `Area33_Handlers` 1 | worldmap | `0x801F2CD0` |
| `0x403D70` | `WorldMap33_PlaceMessage` | 0x87 | `WorldMap_FieldHooks` 1 | worldmap | |
| `0x403E00` | `WorldMap33_PlateRun` | 0xD6 | `WorldMap_Records`[1] +0 (effect kind 0) | worldmap | |
| `0x403EE0` | `WorldMap33_PlateShow` | 0x142 | `WorldMap33_PlateStates` 1 | worldmap | |
| `0x404030` | `WorldMap33_PlateGrow` | 0x41 | ... 2 | worldmap | |
| `0x404080` | `WorldMap33_PlateHold` | 0x58 | ... 3 | worldmap | |
| `0x4040E0` | `WorldMap33_PlateShrink` | 0x50 | ... 4 | worldmap | |
| `0x404130` | `WorldMapHud_Run` | 0x12 | `WorldMap_Records`[1] +0xC (effect kind 0x58) | worldmap | |
| `0x404150` | `WorldMapHud_Frame` | 0xA | `WorldMapHud_States` 1 | worldmap | |
| `0x404230` | `WorldMapHud_BoxStep` | 0x12 | `WorldMapHud_Frame`'s tail jump | worldmap | |
| `0x404250` | `WorldMapHud_BoxSlideIn` | 0x62 | `WorldMapHud_BoxStates` 1 | worldmap | |
| `0x4042C0` | `WorldMapHud_BoxHold` | 0x6E | ... 2 | worldmap | |
| `0x404330` | `WorldMapHud_BoxSlideOut` | 0x57 | ... 3 | worldmap | |
| `0x4048E0` | `WorldMap33_DrawDrift` | 0x462 | `WorldMap_Records`[1] +0x10 | worldmap | |
| `0x411310` | `WorldMap_FrameWait` | 0x27 | `WorldMap_FrameStep`'s state 0, in eleven tables | worldmap | |
| `0x414BB0` | `WorldMapHud_BoxWait` | 0x38 | `WorldMapHud_BoxStates` 0, in eleven tables | worldmap | |
| `0x419110` | `WorldMapHud_Start` | 0x1D | `WorldMapHud_States` 0, in eleven tables | worldmap | |
| `0x462A90` | `WorldMap_RecordIndex` | 0x26 | 17 rel32 callers | worldmap (4,560 calls) | |
| `0x462AE0` | `EffectKind00_WorldMap` | 0x1A | `Effect_KindHandlers` 0x00 | worldmap | |
| `0x462B40` | `EffectKind58_WorldMap` | 0x1A | `Effect_KindHandlers` 0x58 | worldmap | |
| `0x462B80` | `WorldMap_RecordHook10` | 0x1A | `EffectKind18_States` 2 and 3 | worldmap | |
| `0x469BB0` | `EffectKind06_Run` | 0x12 | `Effect_KindHandlers` 0x06 | combat | |
| `0x469BD0` | `EffectKind06_Start` | 0xD7 | `EffectKind06_States` 0 | combat | `0x8019B404` |
| `0x469CB0` | `EffectKind06_Tick` | 0x12 | `EffectKind06_States` 1 | combat | `0x8019B544` |
| `0x469CD0` | `EffectKind06_Blink` | 0x3D | `EffectKind06_Ticks` 0, 1, 2, 4 | combat | `0x8019B588` |
| `0x469DB0` | `EffectKind06_End` | 0x22 | `EffectKind06_States` 2 | combat | `0x8019B7DC` |
| `0x46D830` | `EffectKind18_Run` | 0x12 | `Effect_KindHandlers` 0x18 | worldmap | |
| `0x46D850` | `EffectKind18_Start` | 0x32 | `EffectKind18_States` 0 | worldmap | |

The PSX twins of the three area functions come from the sibling's
`names/area_records.toml` (area 29, kind init; area 33, handler[0] and [1]).
The four kind-6 twins are the catalogue's (`pairs_propagated`). None was
disassembled on the PSX side this round.

**The names are working names.** "Plate", "HUD", "box" and "drift" are
readings from the code (sections 2 to 4). Nobody has watched them live yet.
"33" is the area (`WORLD00/AREA033`, the Yraall map of the owner's route):
the world map's code exists once per world-map area
([`world-map-hud.md`](world-map-hud.md) §1). "Area 29" and "area 33" are the
descriptor numbers in `0x667590`.

- **Area 29's init** (`0x4037B0`).
  - Its entry is a five-byte `jmp 0x4037C0` into the body.
  - `Rand() & 0x3F` against the chances in `Area29_Weights` (16 12 12 8 8 4 2
    2, summing to 64) keeps one of the first eight `Sprite_Objects` records.
    The other seven get byte `+0 = 0`.
  - The kept object goes to one of eight cells in `Area29_Cells`, chosen by
    `Rand() & 7`. Its `+0x3E` is set from `AreaMap_Elevation`.
  - Then `Field_EdgeBits` = the low word of (the leader's dword `+0x134`) − 5.
  - The weights sum to exactly 64, so the loop's "none kept" exit (index 8)
    is never taken with the shipped table.
- **Area 33's handlers** zero map cells through `0x579F00` (group DD's cell
  store):
  - `ClearCellsA`: x 0x3A and 0x3B of rows 0x1C, 0x1D, 0x1F and 0x20.
  - `ClearCellsB`: (0x11, 0x15), (0x11, 0x16) and (0x12, 0x16).
  - They are field-script handlers (ops 0x03 / 0xDE via `0x577B80`). Most
    likely they open passages.
- **`WorldMap33_PlaceMessage`**. `0x56DE30` jumps through
  `WorldMap_FieldHooks` by `WorldMap_RecordIndex`. The function is a
  two-state machine on the s8 `0x9039F4`.
  - State 0: `ScriptFlags_Set40`. It then opens message `(row * 16 + (s8)
    Cond_ByteFA)` of `WorldMap33_PlaceMessages`. The row is the one whose
    first word is the place `0x937F82`, or 6 when no row matches. Then state
    + 1 (read again after `Msg_OpenScript`) and `Field_Request = 2`.
  - State 1: once `Field_Request` is no longer 2, `ScriptFlags_Clear40` and
    the three bytes are zeroed.
  - Neither the row nor the byte is checked.

## 2. The place plate: effect kind 0 on the Yraall map

`EffectKind00_WorldMap` (kind 0 of `Effect_KindHandlers`) tail-jumps to
`WorldMap_Records`[i] +0. On the owner's route that is `WorldMap33_PlateRun`.

1. **`WorldMap33_PlateRun`** computes the cell ahead of the leader. That is
   the high words of `(ObjTrio +9) * (+0xC) + (+0x34)` and of the same with
   `+0x10` / `+0x38`: the position plus a step count times a direction.
2. It stores that cell in the object's `+0xC` / `+0x10`, then sets the kind
   `+0xB`:
   - 1 when `AreaMap_ByteAt` is 0xA1;
   - 2 when it is 0xA0;
   - 3 when it is 0xAE (the cell is asked for again for each test);
   - otherwise 4 when `Field_ScriptFlags2` bit 12 is set, else 0.

   `WorldMap_DrawFrame` tests the same three cell values
   ([`world-map-hud.md`](world-map-hud.md) §1.2).
3. It runs `WorldMap33_PlateStates` by `+1`:
   - State 0 is `0x401DE0`. That is area 16's copy's state 0, shared, and not
     in this round.
   - **Show** copies the kind to `+7`. For kind 1 it looks up the place word
     `0x937F82` in `WorldMap33_PlateAnims` (places 0x17, 8, 7, 0xE, 0x13 and
     0x60) and keeps the place in `+0x18`. Kinds 2, 3 and 4 get animations
     3, 0 and 1. It then sets `+0x40 = 0`, `+0x44 = 0x10000`, `+0x48 = 2`,
     `+9 = 8`, `Sprite_SetAnimation`, and state 2.
   - **Grow** adds 0x2000 to `+0x40` each frame for eight frames (0 to
     0x10000), then state 3.
   - **Hold** leaves (to state 4) when:
     - the kind changes;
     - `Field_Request` is 5; or
     - the kind is 1 and the place word is no longer `+0x18`.

     It does not leave while `Game_Mode` is 1.
   - **Shrink** runs the scale back over eight frames. It then goes back to
     Show, or `Effect_Release` when `Field_Request` is 5.
   - Grow, Hold and Shrink each call `WorldMap_PinSprite` first, which pins
     the sprite to (160, 80). Each ends in `Sprite_QueueOverlay`, except on
     Shrink's last frame.

**The reading.** This is the place-name plate. When the leader faces a place
cell, its animation (chosen by the place) scales in horizontally above the
party. It holds while the party stays, and it folds away when they leave. The
[`world-map.md`](world-map.md) §5 localisation question ("where the plates'
rectangles live") has its answer here:

- the plate is a **sprite animation chosen by place** through
  `Sprite_SetAnimation`, not a `WorldMap_DrawSprite` rectangle;
- that fits [`world-map-hud.md`](world-map-hud.md) §5.2's finding that the
  section which changes size with the plates is `0x800D3800`, the sprite
  frame records.

The live check should confirm the reading on `f01260` / `f01740`.

## 3. The map's HUD task and the region box

`EffectKind58_WorldMap` (kind 0x58) → `WorldMap_Records`[i] +0xC →
`WorldMapHud_Run`. That dispatches `WorldMapHud_States` by `+1`:

- **Start** (`0x419110`, shared by the eleven copies) sets `+9 = +0xB = 0`
  and `+1 += 1`.
- **Frame** (`0x404150`) is `call WorldMap_FrameStep; jmp
  WorldMapHud_BoxStep`. So the object carries three machines:
  - `+1` is the task;
  - `+2` is the dial frame's slide ([`world-map-hud.md`](world-map-hud.md)
    §1.1, whose state 0 is `WorldMap_FrameWait` `0x411310`);
  - `+3` is the region box's.

`WorldMapHud_BoxStep` dispatches `WorldMapHud_BoxStates` by `+3`:

| State | Function | Does |
|---|---|---|
| 0 | `WorldMapHud_BoxWait` (shared) | unless the box leaves: y `+0x30 = 0xF0`, state 1 |
| 1 | `_BoxSlideIn` | y −= 10; at 0xC8 or below, state 2; when the box leaves, state 3 |
| 2 | `_BoxHold` | while `+0xB` is 0, count `+9` to 0x5A frames, then `+0xB = 1`; when the box leaves, state + 1 |
| 3 | `_BoxSlideOut` | y += 10; at 0xF0 or above, state 0; unless the mode byte, `Field_Request` 2 or flag bit 8, state 1 |

**The box leaves** when any of these holds:

- the map's mode byte `0x9045FA` is set while `+0xB` is set;
- `Field_Request` is 2;
- `Field_ScriptFlags` bit 8 is set.

The slide-out's own test leaves out `+0xB`. States 1 to 3 each end with
`WorldMap_DrawHud(0x5C, y)`.

The box slides up from y 0xF0 to 0xC8 and holds for 0x5A frames. Once the
hold has run (`+0xB` set), the mode byte slides it away. The original pushes y
as the word in a register whose upper half is the object pointer's.
`WorldMap_DrawHud` and `Text_DrawAt` read only the low word
(`symbols.toml`: "stores word x ... word y"), so ours passes the word
sign-extended.

## 4. The drift layer (`0x4048E0`)

Reached by `WorldMap_RecordHook10` through `EffectKind18_States` 2 and 3.
With `b = +0xB`:

1. The first frame adds `Frame_Counter & 0xF` to `+0x3A`.
2. Nothing more happens unless `Draw_PassFlags` bit 2 is set.
3. `+0x38` += `b << 10` each frame. `+0x3A` wraps to −8 past the map's height
   (`AreaMap_Header[1] + 8`).
4. The layer draws only when the leader is within 25 cells of the layer's
   position in x **or** in z.
5. **One textured square**, whose half-side pulses: `|15 − (Frame_Counter &
   0x1F)| + ((4 − b) << 8)`. It is centred on `(+0x34 >> 9) − 0x3FC0` and
   `(+0x38 >> 9) − 0x3FC0`, at depth −0x300, and goes through the GTE with
   `Prim_SetTexture((b − 2) | 0xBB509100, prim, 1)`.
6. **Then a grid** of `(17 − 4b) × (16 − 4b)` cells around `+0x36` / `+0x3A`.
   Each map item `MapView_ItemHalfAt` answers gets a semi-transparent
   `POLY_FT4`. It takes the item's corners, colour 0x28, CLUT 0x78CB and
   tpage 0x5B. Its u / v come from `WorldMap33_DriftUV`, scrolled by `+0x38`.
   It is appended with `MapView_LinkPrimAt`.

Only `b` 2 and 3 have rows in `WorldMap33_DriftUV` (cell sizes 10 and 12).
`b` 0, 1 and 4 read the bytes either side, and 5 and up draw no grid.

What it looks like is unread. The name is a guess from the scroll: a texture
drifting over the map's items near the party (cloud shadow or mist). The x
**or** z distance test is as the code has it. Whether an AND was meant is
open. The PSX twin was not read.

## 5. The tables named

All are in `symbols.toml`, each a `[[data]]` entry with its count:

| Table | At | Count | What |
|---|---|--:|---|
| `WorldMap_Records` | `0x653910` | 77 dwords | eleven 0x1C-byte records, one per world map (areas 16, 33, 45, 65, 87, 88, 104, 115, 121, 151, 152): five code slots, a data pointer `+0x14`, the area byte `+0x18` |
| `WorldMap_FieldHooks` | `0x662DF0` | 12 | one field hook per world map, and `0x4139E0` for none |
| `Area33_Handlers` | `0x5EF4C8` | 2 | area 33's descriptor `+0x3C` |
| `WorldMap33_PlateStates` | `0x5EF5DC` | 5 | the plate's states |
| `WorldMapHud_States` | `0x5EF5F0` | 2 | the HUD task's |
| `WorldMapHud_BoxStates` | `0x5EF608` | 4 | the region box's |
| `WorldMap33_PlateAnims` | `0x5EF320` | 6 | (place, animation) |
| `WorldMap33_PlaceMessages` | `0x5EF51C` | 96 words | six rows of place + fifteen messages |
| `WorldMap33_DriftUV` | `0x5EF6B4` | 12 bytes | u base, v base, cell size by `+0xB − 2` |
| `Area29_Cells` / `Area29_Weights` | `0x5EE258` / `0x5EE268` | 16 / 8 bytes | area 29's placement |
| `EffectKind06_States` | `0x653EC8` | 3 | |
| `EffectKind06_Anims` | `0x653ED4` | 8 bytes | |
| `EffectKind06_Ticks` | `0x653EDC` | 6 | |
| `EffectKind18_States` | `0x65406C` | 160 (upper bound) | a run of 160 code pointers; where the table ends is not established |

**`WorldMap_RecordIndex` answers 11 when no record matches the area.** The
three kind handlers then read the dwords after the eleventh record. Those are
the table `0x653A44` of `0x462BA0` (`0x462BC0`, `0x462BF0`, `0x462E70`, ...).

So an effect of kind 0, 0x58 or 0x18/2..3 run outside a world map calls
`0x462BA0`'s state entries. Which effects reach that is unread.

Record 6 (area 104) holds nulls at `+4`, `+8` and `+0x14`. Kinds 0xE and 0x16
(`0x462B00` and `0x462B20`, not in this round) would call a null there. Ours
reads the tables in place, as the original does.

## 6. Effect kinds 6 and 0x18

**Kind 6** (combat route):

- `EffectKind06_Run` dispatches by `+1`.
- **Start** (`0x469BD0`):
  - `Sprite_SetAnimationBank(0x18)`;
  - offsets the screen point by `+0xC` / `−+0x10`;
  - sets `+0x29 = 2`;
  - variant `+6 == 3` gets `+0 |= 0x20` and the tint bytes `+0x5D..+0x5F =
    0xF8` for 0x14 frames; the others get 0 for 0x10 frames;
  - the animation comes from `EffectKind06_Anims[+6]`.
- **Tick** dispatches by the variant through `EffectKind06_Ticks`.
- **Blink** counts `+9` down and runs `Sprite_ScriptTick`. It draws only on
  frames whose `+9` has bit 2 set (`Sprite_QueueOverlay` for kind `+5 == 6`,
  else `Sprite_UpdateScreen`).
- **End** clears the tint and releases the effect.
- Variants 3 and 5 tick through `0x469D40` and `0x469D10`, which are not in
  the queue.

**Kind 0x18:**

- `EffectKind18_Run` dispatches by `+1`.
- `EffectKind18_Start` sets `+1 = +0xB` and clears `+2`, `+3`, `+4` and `+9`.
  The effect's sub-kind becomes its next state.
- Sub-kinds 2 and 3 are the world map's record `+0x10` (the drift layer).

## 7. The fuzz and its negative controls

`BOF3X_SHADOW=worldmap_area`, `src/game/worldmap_area_fuzz.cpp`, in the
style of `magic_fx_reached_fuzz.cpp`.

**The copies.** There are thirty byte-copies. Every E8 and E9 that leaves a
function is re-aimed at a recorder (56 sites, each checked against the callee
it was read to reach). The calls among the thirty are re-aimed too, so each
function is tested alone.

`Area29_PickFieldObject`'s entry is itself a five-byte jmp. `CloneOriginal`
takes that for a detour, so the copy is of the body it reaches, `0x4037C0`
(0xBB bytes).

**The tables.** The seven dispatch tables are swapped for recorders (61
slots) and put back afterwards:

- the first five of `EffectKind18_States`;
- slots `+0`, `+0xC` and `+0x10` of all twelve records, the "none" row
  included.

**The state.** Each round, both passes start from random bytes over these
regions:

- the fuzz's two objects, packet buffer and four map items;
- the first eight `Sprite_Objects` and the leader's record;
- the place, message, flag, mode, request, area-number and pass bytes;
- `Gfx_PacketNext` and `Prim_VertexScratch`;
- the map header and the leader's cells;
- the plate and drift tables, and area 29's.

The pointers are then put back inside their buffers. The dispatch bytes stay
inside the smallest table that reads them after a call.

**The seeded boundaries:**

- area 29's first roll on each running sum and one below, and a zero table
  (none kept);
- the message state and `Cond_ByteFA` at their sign edges;
- the three cell kinds and flag bit 12;
- the plate's place planted in a random entry (the search has no bound);
- `Game_Mode` 1, 0x101 and 2, the place word with bit 16;
- the box's y one either side of each threshold, and the leave test's four
  inputs;
- the drift's `b` 0..5 with the leader just in and just out of 25 cells;
- the area number as each record's, its + 0x100, and none.

**The recorders are louder than the real callees.** Two thirds of the calls
also move one of these: `Sprite_Current`, an object field, the mode byte,
`Field_Request`, either flag word, the place, the message state,
`Frame_Counter`, `Draw_PassFlags`, `Game_Mode`, `Cond_ByteFA` or the leader's
`+0x134`. The primitive setters overwrite every byte of the primitive, and
the answers carry garbage above the byte that is read.

**Result** (2026-09-25, `BOF3X_SELFTEST_ONLY=1`, exit 0):

    worldmap_area self-test: 60000 rounds over 30 functions (2000 each), 270307 calls to the stand-ins, 0 MISMATCHES
    coverage: plate states 449 / 436 / 392 / 335 / 388, hud states 1027 / 973, box states 461 / 489 / 537 / 513,
      records +0 2000 +0xC 2000 +0x10 2000, kind 6 states 666 / 680 / 654, ticks 321 / 372 / 331 / 331 / 334 / 311,
      kind 0x18 states 420 / 417 / 382 / 372 / 409
    coverage: area 29 none kept 613, cells set 22000, elevations 1387; messages opened 380, flags set 380 /
      cleared 282; cells asked 4998; plate kinds shown 0:171 1:509 2:170 3:176 4:173 other:801, animations 3028,
      pins 6000, overlays 5927, releases 2143; hud draws 6000, frame steps 2000, box steps 2000; drift drawn 869,
      items 31676, commits 869, textures 869; record index none 1059

With `BOF3X_SHADOW='*'` the run exited 0 with `inject: 1292 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** The scratchpad's `DA/controls.py` planted each one
alone: edit, build, headless self-test, restore. **97 were planted and 97
refused.** Every refusal was by comparison, with exit 3 and no crash. The
table gives the mismatching rounds of 2,000.

| | Planted | Refused in |
|---|---|---|
| A1 | Area29: the roll masked 0x7F | 583 |
| A2 | Area29: a chance taken at <= | 276 |
| A3 | Area29: the kept object cleared too | 1383 |
| A4 | Area29: x from the z byte | 1271 |
| A5 | Area29: the elevation at (z, z) | 1271 |
| A6 | Area29: the edge less 4 | 2000 |
| A7 | Area29: the edge only when one is kept | 613 |
| C1 | ClearCellsA: row 0x1E for 0x1F | 2000 |
| C2 | ClearCellsA: x 0x3B set to 1 | 2000 |
| C3 | ClearCellsB: the third cell (0x12, 0x15) | 2000 |
| M1 | PlaceMessage: rows 0x10 apart | 339 |
| M2 | PlaceMessage: Cond_ByteFA zero-extended | 125 |
| M3 | PlaceMessage: the state not read again after the message | 10 |
| M4 | PlaceMessage: Field_Request 3 | 380 |
| M5 | PlaceMessage: state 1 whatever Field_Request | 108 |
| M6 | PlaceMessage: 0x9039F5 kept | 279 |
| R1 | PlateRun: one step more ahead | 2000 |
| R2 | PlateRun: 0xA0 is kind 3 | 144 |
| R3 | PlateRun: Field_ScriptFlags2 bit 11 | 643 |
| R4 | PlateRun: dispatch by +2 | 1584 |
| R5 | PlateRun: +0xB to the object of before the calls | 90 |
| R6 | PlateRun: the cell asked once | 1572 |
| S1 | PlateShow: +7 not set | 1986 |
| S2 | PlateShow: +0x18 with bit 16 | 509 |
| S3 | PlateShow: kind 2 animation 2 | 170 |
| S4 | PlateShow: +0x48 = 1 | 1028 |
| S5 | PlateShow: +9 = 7 | 1024 |
| S6 | PlateShow: the animation from the pad byte | 507 |
| S7 | PlateShow: +1 = 2 before Sprite_SetAnimation | 28 |
| G1 | PlateGrow: step 0x1000 | 2000 |
| G2 | PlateGrow: at 0 the state 4 | 403 |
| G3 | PlateGrow: no overlay on the last frame | 403 |
| H1 | PlateHold: Game_Mode 2 holds | 379 |
| H2 | PlateHold: Field_Request 4 | 196 |
| H3 | PlateHold: +0x18 compared as a word | 95 |
| H4 | PlateHold: the place test for any kind | 391 |
| H5 | PlateHold: +9 = 7 on leaving | 1051 |
| K1 | PlateShrink: the scale grows | 2000 |
| K2 | PlateShrink: released unless Field_Request 5 | 419 |
| K3 | PlateShrink: at 0 the state 0 | 276 |
| D1 | HudRun: dispatch by +3 | 1020 |
| D2 | BoxStep: dispatch by +1 | 1521 |
| D3 | Kind06Run: the next entry | 2000 |
| D4 | Kind06Tick: by +5 | 1672 |
| D5 | Kind18Run: by +2 | 1585 |
| T1 | HudStart: +9 kept | 1995 |
| T2 | HudStart: +1 = 1 | 1015 |
| F1 | HudFrame: the box before the frame | 2000 |
| W1 | FrameWait: the mode byte 1 holds | 265 |
| W2 | FrameWait: y = -0x20 | 851 |
| W3 | FrameWait: flag bit 9 | 904 |
| B1 | BoxWait: y 0xE0 | 680 |
| B2 | BoxWait: +3 = 1 | 529 |
| L1 | BoxLeaves: the mode byte alone | SlideIn 207, Hold 195, Wait 295 |
| L2 | BoxLeaves: Field_Request 5 | SlideIn 155, Hold 168, Wait 190 |
| I1 | BoxSlideIn: below 0xC8 | 60 |
| I2 | BoxSlideIn: step 8 | 2000 |
| I3 | BoxSlideIn: leaving to state 2 | 1339 |
| I4 | BoxSlideIn: the box at x 0x5B | 2000 |
| O1 | BoxHold: 0x59 frames | 131 |
| O2 | BoxHold: counting whatever +0xB | 875 |
| U1 | BoxSlideOut: above 0xF0 | 122 |
| U2 | BoxSlideOut: the slide-in leave test | 226 |
| X1 | DrawDrift: pass bit 3 | 960 |
| X2 | DrawDrift: +0x38 by b << 9 | 1518 |
| X3 | DrawDrift: height + 7 | 66 |
| X4 | DrawDrift: the distance test an AND | 745 |
| X5 | DrawDrift: the pulse by Frame_Counter & 0xF | 401 |
| X6 | DrawDrift: centre less 0x3FB0 | 869 |
| X7 | DrawDrift: texture \| 0xBB509000 | 704 |
| X8 | DrawDrift: commit 0x44 | 869 |
| X9 | DrawDrift: one row fewer | 443 |
| X10 | DrawDrift: the item x one on | 443 |
| X11 | DrawDrift: the cells opaque | 443 |
| X12 | DrawDrift: CLUT 0x78CC | 443 |
| X13 | DrawDrift: u from the row | 327 |
| X14 | DrawDrift: the scroll from +0x3A | 319 |
| X15 | DrawDrift: link dy 0 | 443 |
| X16 | DrawDrift: v0 depth 0xFE00 | 869 |
| X17 | DrawDrift: +2 not counted | 899 |
| X18 | DrawDrift: v3 depth 0xFD01 | 869 |
| N1 | RecordIndex: the area truncated to a byte | 201 |
| N2 | RecordIndex: ten records | 1059 |
| E1 | EffectKind00: the record +0x10 | 2000 |
| E2 | EffectKind58: the record +0x10 | 2000 |
| E3 | RecordEntry: the index mod 12, not & 0xFF | Kind00 1339, Kind58 1336, Hook10 1271 |
| Q1 | Kind06Start: bank 0x19 | 2000 |
| Q2 | Kind06Start: variant 2 is the bright one | 659 |
| Q3 | Kind06Start: +0x30 += +0x10 | 2000 |
| Q4 | Kind06Start: +9 = 0x15 | 399 |
| Q5 | Kind06Start: the animation by +5 | 1432 |
| Q6 | Kind06Start: +0 \|= 0x10 | 314 |
| Z1 | Kind06Blink: bit 3 | 642 |
| Z2 | Kind06Blink: kind 5 queues | 346 |
| Z3 | Kind06Blink: at 0 the state 2 | 178 |
| Y1 | Kind06End: +0x5D kept | 1986 |
| J1 | Kind18Start: +1 from +0xA | 1992 |
| J2 | Kind18Start: +4 kept | 1996 |

The thinnest refusals:

- M3 (10): the message state moved by the recorder of `Msg_OpenScript`;
- S7 (28): `+1` moved by the recorder of `Sprite_SetAnimation`;
- I1 (60) and X3 (66): each boundary seeded on purpose;
- R5 (90) and H3 (95).

**What the fuzz cannot see:**

- anything the callees really do: the sprite, map-cell, GTE and primitive
  code is ours and fuzzed in its own modules, and `0x579F00` / `0x57C7A0` are
  group DD's;
- `0x401DE0`, `0x404800`, `0x404680`, `0x469D10`, `0x469D40`, `0x462BA0`'s
  table and the other effect sub-kinds, which the tables reach;
- the order of stores with no call between them;
- the stale upper halves the originals push:
  - `WorldMap33_PlateRun`'s cell words carry an uninitialised stack word;
  - the box states' y carries the object pointer's high half.

  The callees read only the low words, and the recorders record only those.
- the drift's texel values, since only the primitives' bytes are compared;
- `WorldMap33_PlateShow`'s search for a place that is in no entry. The search
  has no bound: the original would read on through `.data`, and so would
  ours, faithfully. The fuzz always plants the place.

## 8. `entries_logic.txt`

These lines were appended to the main checkout's
`analysis/calltrace/entries_logic.txt` on 2026-09-25 under the comment `#
--- 2026-09-25 (round 8), group DA`:

    004037B0 CB    00403CE0 5F    00403D40 25    00403D70 87    00403E00 D6
    00403EE0 142   00404030 41    00404080 58    004040E0 50    00404130 12
    00404150 A     00404230 12    00404250 62    004042C0 6E    00404330 57
    004048E0 462   00411310 27    00414BB0 38    00419110 1D    00462AE0 1A
    00462B40 1A    00462B80 1A    00469BB0 12    00469BD0 D7    00469CB0 12
    00469CD0 3D    00469DB0 22    0046D830 12    0046D850 32

`00462A90 26` was already listed with the right size. Eight lines already in
the file overlap the group; the merger should judge them:

| Line now | Overlaps | Body, as read |
|---|---|---|
| `00402570 1BEA` | `004037B0..0040415A` | 0x58, ret at `0x4025C7` |
| `00404160 227` (`WorldMap_FrameStep`, ours) | `00404230..00404387` | 0xC1 ([`world-map-hud.md`](world-map-hud.md) §7) |
| `00404620 95D` (`WorldMap_DrawHud`, ours) | `004048E0` | 0x58 |
| `004112F0 257` | `00411310` | 0x12, a dispatcher |
| `00414AC0 267` | `00414BB0` | 0x78 |
| `00462AC0 442` | `00462AE0..00462B9A` | 0x1B |
| `00469AD0 360` | `00469BB0..00469DD2` | 0xDA |
| `0046D770 2AF` | `0046D830..0046D882` | 0xA |

## 9. Left original: `0x404180`

`0x404180` is entry 1 of `WorldMap_FrameStep`'s state table `0x5EF5F8`. It
is a case block: the state-1 code that falls into state 2 (`jmp 0x4041B0`).
The only thing that jumps to it is `WorldMap_FrameStep` `0x404160`'s `jmp
[eax*4 + 0x5EF5F8]`:

- `symbols.toml`'s evidence for `WorldMap_FrameStep` names it: "the
  dispatcher and its case blocks 0x404180, 0x4041B0, 0x4041E0";
- `refs.py` finds the one dword reference.

Round seven's `WorldMap_FrameStep` carries states 1 to 3 inline, so
`0x404180` has not been entered since then. The route that queued it was
traced all-original ([`remaining-catalog.md`](remaining-catalog.md) §4). By
the round's rule for switch cases, it went with its host. It has no
`symbols.toml` entry and no detour.

## 10. For other groups and the coordinator

- **`0x404180` is misfiled in the queue.** It is not a function (section 9).
  The queue counts 31 for DA; 30 is the real number.
- **Group DD's** `0x579F00` (a map cell's byte, `(x, z, value)`: `AreaMap_Bytes
  + AreaMap_Header[0] * z + x`) and `0x57C7A0` (`ScriptFlags_Clear40`) are
  called here through raw addresses.
- **Other addresses these tables reach**, in no group this round:
  - `0x401DE0`: the plate's state 0, area 16's copy;
  - `0x404800` / `0x404680`: record 1's `+4` / `+8`, kinds 0xE and 0x16. The
    latter is the dispatcher [`world-map-hud.md`](world-map-hud.md) §6 found;
  - `0x462AC0`: the record's data pointer, called from `0x531B0C`;
  - `0x462B00` / `0x462B20`: kinds 0xE and 0x16;
  - `0x56DE30`: the field hook's dispatcher;
  - `0x469D10` / `0x469D40`: kind 6's variants 5 and 3;
  - `0x469E00`: state 1 of a second kind whose table `0x653EF4` shares these handlers;
  - `0x4139E0`: the no-world-map field hook.
- **The ten other world-map copies** share `WorldMap_FrameWait`,
  `WorldMapHud_BoxWait` and `WorldMapHud_Start`, which are now ours. Each
  copy's own states are the same code over its own tables, a cheap follow-up
  as [`world-map-hud.md`](world-map-hud.md) §6 said.
- **For the localisation build** ([`world-map.md`](world-map.md) §5, DIV-0055):
  the plates are sprite animations chosen by place (section 2).
- **Latent in the original, kept.** No D-number: the coordinator assigns
  them.
  1. `WorldMap33_PlateShow`'s search has no bound. A kind-1 cell (0xA1) at a
     place not among the six would read on through `.data`.
  2. Outside a world map, `WorldMap_RecordIndex`'s 11 makes the kind handlers
     call through `0x462BA0`'s table (section 5).
  3. Record 6's null slots.
  4. Area 29's "none kept" exit is unreachable with its table.
  5. The drift's x-or-z distance test (section 4).

  None of these has been seen in play.

## 11. Open

- The owner's eye on the plate, the region box and the drift layer on the
  world-map route, where the captures show the plate on `f01260` / `f01740`.
- What writes `0x937F82` (the place under the party). `refs.py` finds 40-odd
  `.text` references across the eleven copies' code; which of them write it
  is unread.
- What area 29's kept object is, and the combat route's kind-6 effect.
- The PSX twins of the plate, the box and the drift, which live in the world
  map overlay `0x801F2C00` ([`world-map-hud.md`](world-map-hud.md) read five
  of its functions).
