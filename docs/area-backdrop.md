# The area's backdrop and the rest of the area-entry handlers

**Status:** IN PROGRESS (2026-09-23) - four functions ours
(`src/game/area_backdrop.cpp`), each fuzzed alone against a copy of Capcom's
with every callee a recording stand-in, 0 mismatches, and every planted bug
refused. **Not yet through the live check**: the world-map route captured
wide and narrow, which runs after the merge, is what checks the widened sky
(DIV-0041) and the faithful narrow one pixel for pixel.

Group 2 of the world-map wave ([`world-map.md`](world-map.md) §4): the three
handlers of `AreaMap_EntryHandlers` `0x66329C` that
[`map-layers.md`](map-layers.md) took the table's pass and entry 0 from but
left unread, and the small function [`widescreen.md`](widescreen.md) §3d
called "the sky" - which it is not (§1).

## 1. The functions

Sizes are the disassembly's (capstone, 2026-09-23: each decodes exactly to its
last `ret`, and every jump stays inside). None of the four is reached by the
attract cycle or the shop route; the three handlers were **never traced**
(absent from `entries_plus_hidden.txt` - add them before the next trace) and
are on screen in the world-map route's hill and coast areas
(`world-map.md` §4); the pin's 259 calls are that route's.

| Function | Address | Size | PSX twin (pair tier) | Calls |
|---|---|--:|---|--:|
| `AreaMap_DrawBackdrop` | `0x571BE0` | 0x147 | `FUN_80159108` (gap4) | untraced |
| `AreaMap_TextureCycle` | `0x571D30` | 0xE5 | `FUN_801592EC` (gap4) | untraced |
| `AreaMap_SlotZones` | `0x571E20` | 0x1C7 | `FUN_801593FC` (gap4) | untraced |
| `WorldMap_PinSprite` | `0x4112A0` | 0x18 | unpaired (pointer-reached; not in `pc_funcs.json`) | 259 (`recipe_worldmap`) |

The pairs are `analysis/pairs_propagated.json`'s. The twins were read in MIPS
with the sibling's `tools/disasm_exe.py` (`80159108:130 801592EC:70
801593FC:130`) and match instruction for instruction, with the porting
house's usual changes - shorts to floats in the primitives, the PC's 0x14C
object stride for the PSX's 0x140 - and one dropped test (below). The three
were `AreaMap_EntryKind1..3` in `symbols.toml` (`hypothesis`, calls only);
renamed and promoted with the readings here.

### `AreaMap_DrawBackdrop` `0x571BE0` - the sky

`AreaMap_EntryHandlers` entry 1. Nothing while `MapView_BuildFlags`
`0x929ED8` is 0 - `MapView_Build` sets its bit 0 for any view cell whose
map x or y is 0, so the backdrop is drawn only while the view reaches off the
map. Then the entry's dword `+4` as four bytes `x0 z0 x1 z1` against the
focus in cells, `(0x7FFF - MapView_FocusX) >> 8` and `(0x8000 -
MapView_FocusZ) >> 8` (signed 32-bit compares): all of `x0 <= fx <= x1` and
`z0 <= fz <= z1` must hold. Then:

1. `Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, 0x95, 0)` - tpage `0x95`,
   dithering on - and `Gfx_CommitPrim(7, 0xC)`.
2. The cursor read again; `Gpu_SetPolyG4`, `Gpu_SetSemiTrans(p, 0)`; the
   corners `(0, 0)`, `(320, 0)`, `(0, 240)`, `(320, 240)` as floats at
   `+8`, `+0x18`, `+0x28`, `+0x38`. **The left x is a zeroed register
   (`ebx`), not an immediate**, which is why this could not be byte-patched
   (`widescreen.md` §5) and is taken over instead.
3. The colour dword at `+8`, or `+12` when `Cond_ByteFF` `0x7E1BE2` is set
   (the byte `Area_TestCondition`'s kind `0xFF` reads; a day/night or
   alternate-palette switch is the guess, unread): its low word's 5-bit r,
   g, b unpacked to 8 (`and 0x1F, shl 3`) into corners 0 and 1, its high
   word's into 2 and 3 - a vertical gradient. `Gfx_CommitPrim(7, 0x44)`.

The PSX twin does the same with shorts (0x24 bytes), but picks tpage `0x225`
over `0x95` by two calls of `0x8017BC2C` (a mode test returning 1 or 2); the
port passes `0x95` always.

**DIV-0041.** Under `Widescreen_Live()` the quad's x runs from **-53 to
373** in the game's 320-wide units: `left = 0.0f - wide`, `right = 320.0f +
wide` with `wide = Widescreen_Live()` (53 wide, 0 otherwise), the rows 0 and
240 unchanged. That is exactly how `Transition_DrawTile` (`mode_flow.cpp`)
and `Menu_DrawBlackScreen` (`save_menu.cpp`) were widened: no scale factor
belongs here - the game draws in its 320 x 240 space and the backend's scene
shader shifts every x by 53k and scales by k (`widescreen.md` §3a), so a quad
from -53 to 373 lands on target columns 0 to 426k exactly, no band and no
seam. (`0.0f - wide` rather than `-wide`: the latter is `-0.0f` when the
view is not wide, and a differing bit pattern in the packet is a fuzz
mismatch - the same trap `mode_flow.cpp` met.) Off, the quad is the
original's (0, 0)..(320, 240) bit for bit.

### `AreaMap_TextureCycle` `0x571D30`

`AreaMap_EntryHandlers` entry 2: a texture animation, one VRAM rectangle
copied over another on a frame schedule - the texture-page counterpart of
`AreaMap_ClutCycle`. `Area_TestCondition` of the word `+4` (pushed with
stale bits above it; the callee reads 16) or nothing. `f = Frame_Counter mod
(byte 0 + 1)`. From `+8`, pairs of dwords `(A, B)` in order of `A`'s top
byte, a frame threshold: the scan moves on while `f` is above it, so the
first pair at or above `f` is taken. The move: source rect x `= (A bits
16..19) << 7 | (B bits 9..15)`, `+ 0x140`; y `= B byte 0 + 0x100`; w `= B's
top byte`; h `= B byte 2 + 1`; destination x `= (A bits 20..23) << 7 | (A
bits 9..15)`, `+ 0x140`; y `= A byte 0 + 0x100`. `Gpu_SetDrawMove
(Gfx_PacketNext, rect, x, y)`, `Gfx_CommitPrim(6, 0x18)`. The `0x140` /
`0x100` put every rectangle in the VRAM quarter the area's texture pages
live in. The PSX twin is the same; its `divu` carries a `break` on a zero
divisor the PC cannot reach (a byte plus one).

### `AreaMap_SlotZones` `0x571E20`

`AreaMap_EntryHandlers` entry 3: rectangles of the map inside which the
party's sprites change draw slot. Nothing while `Draw_OtSlot` `0x92BF19` is
4. The entry's byte `+2` less one is a count of zone dwords from `+4` (0 or
1: nothing). A zone: `x0` the top byte, `z0` byte 2, a width of bits 9..15,
a depth of bits 2..8, bit 0 the sense. For each of the party's three objects
(`ObjTrio` `0x802D40`, stride `0x14C`: positions `+0x34` / `+0x38`, the
flag byte `+0x138`, the slot byte `+0x29` - what `Sprite_Draw` commits the
sprite's primitives to) whose rounded cell - the signed high word of the
16.16 position plus `0x8000` - lies in `[x0, x0 + width) x [z0, z0 +
depth)`: sense 1 sets flag bit 0 and the slot byte 4, sense 0 clears the
bit and sets the slot byte `Draw_OtSlot`. A fourth object record at
`0x905DA0` (unnamed; the one `GameMode_Field`'s request 9 sets up and
`MapView_CheckHeightScale` tests bit 0 of its byte `+0xB` for, as it tests
`ObjTrio`'s `+0x138`) gets only its bit, by the same test. The PSX twin is
the same over its records at `0x80145EC0` (stride 0x140) and `0x8014629C`.

What the zones look like in game is not established (a bridge or an
overpass the party walks under, drawn in front of the map, is the guess);
the name describes what is set.

### `WorldMap_PinSprite` `0x4112A0` - not the sky

Six instructions: `Sprite_Current +0x2E = 0xA0`, `+0x30 = 0x50` - the
sprite's screen point (what `Sprite_UpdateScreen` writes and
`Sprite_DrawOverlays` reads back) pinned to (160, 80). `widescreen.md` §3d
listed `0x4112A9` - the `0xA0` immediate - as "the sky" from `bof3ext`'s site
list; it is a sprite position. Callers (rel32 scan of `.text`): 33 sites in
eleven triads, `0x401F80` / `0x401FD0` / `0x402030` .. `0x424D50` /
`0x424DA0` / `0x424E00`, one triad per world-map region overlay compiled
into the exe (the PSX `0x801F2C00` family); each calls it first, then moves
the sprite's scale `+0x40` by `0x2000`, counts `+9` down, sets `+0x48` and
the state `+1`, and tail-jumps to `Sprite_UpdateScreen` `0x5890E0` - the
zoom in and out of the party's marker as the map opens and closes, by the
shape (a hypothesis in the name). On the route: 259 calls from `0x404035`
(24) and `0x404085` (211).

Under the chosen widescreen approach a point centred on the 320 view stays
centred: **nothing to widen**, and nothing of it shows in the bands. Taken
over because it was in the queue and is six instructions.

## 2. Callees

Every call goes through `area_backdrop::g`
(`src/game/area_backdrop_callees.h`), so the fuzz can stand recorders in.
All six were already typed and ours: `Gpu_SetDrawMode`, `Gpu_SetSemiTrans`
(`psx_gpu.cpp`), `Gpu_SetPolyG4`, `Gpu_SetDrawMove` (`field_misc.cpp`),
`Gfx_CommitPrim` (`draw_emit.cpp`), `Area_TestCondition` (`map_cells.cpp`).
Nothing new is bound; the fourth object at `0x905DA0` and `ObjTrio`'s field
offsets are constants in `area_backdrop.cpp` with their reading, not
`symbols.toml` entries (a data name for `0x905DA0` is worth having once
someone reads what the record is).

## 3. Kept as the original has it

Each is in the code's comments; the ones a control refused are marked.

- **The bound tests** are signed 32-bit compares of a byte against the
  24-bit shifted focus (B2..B5, B16).
- **`AreaMap_SlotZones`' cell** is sign-extended (`movsx`) before the
  compares, and that is unobservable: the bounds never exceed 382 (control
  Z7, §4).
- **The cursor is read again** after the first commit for the quad (B6):
  a full pool leaves it where it was, and the quad then overwrites the draw
  mode, as in the original.
- **The condition word** goes to `Area_TestCondition` with stale bits above
  it; only the low 16 reach the test (the stand-in masks them).
- **`AreaMap_TextureCycle`'s scan** has no end but a threshold at or above
  the frame: a list without one runs on (read, not run - the fuzz ends every
  list with `0xFF`, which no frame exceeds).
- **`AreaMap_SlotZones`** re-reads `Draw_OtSlot` for every object; a count
  byte of 0 or 1 does nothing (Z2).
- **The PSX's tpage test** is not restored: the port passes `0x95` always
  (B8 is the alternative refused).

## 4. Checks

**Start-up fuzz**, `BOF3X_SHADOW=area_backdrop`
(`src/game/area_backdrop_fuzz.cpp`). Each original is cloned with every call
re-aimed at a recording stand-in and ours runs with the same stand-ins
through `area_backdrop::g`. Compared each round: every byte of the
function's state and the stand-ins' log (count, a hash of every entry, the
first 32 kept). The clone runs under control word `0x027F`. The stand-ins
give back what the caller reads: the packet cursor moved by the commit's
size - or, one time in four, not moved, as a full pool's is - and the bytes
the setters scribble. `Widescreen_Inject` runs after every fuzz, so
`Widescreen_Live()` is 0 here and **the faithful 0..320 quad is what is
compared**; the wide quad is the live check's.

Seeded: the focus at each of the box's four bounds and a cell past it, and
inside the box; the flags byte 0; both colour dwords; the texture cycle's
frame at each threshold and a step either side, periods 1, 2, 3, 255, 256;
the zones' positions at each bound of a zone and a cell past it with the
fraction at `0x7FFF` / `0x8000`, negative cells, `Draw_OtSlot` 4, a count
byte of 0 and 1.

The run of 2026-09-23 (`build/bof3x.log`, the same under `BOF3X_SHADOW=*`):

| Function | Rounds | Coverage | Mismatches |
|---|--:|---|--:|
| `AreaMap_DrawBackdrop` | 60,000 | 15,106 with the flags clear, 34,893 with the focus outside the box, 10,001 drawn - 4,959 from the alternate colour, 2,542 with the cursor held by the first commit | **0** |
| `AreaMap_TextureCycle` | 60,000 | 20,146 refused by the condition, 39,854 moves - 21,734 from the first pair, 6,844 past every threshold but the `0xFF` | **0** |
| `AreaMap_SlotZones` | 40,000 | 10,601 with `Draw_OtSlot` 4, 2,160 with no zones; 547,131 zones run; slot bytes changed 5,689 to 4 and 5,538 to `Draw_OtSlot`; 1,914 fourth-object flags changed | **0** |
| `WorldMap_PinSprite` | 4,000 | every byte of the sprite random | **0** |

**Negative controls.** Each a rebuild and a start-up run, reverted after
(`controls.py`, a scratch script: one planted change per run). All refused
with exit 3, by comparison:

| Control | Mismatches |
|---|--:|
| B1 the flags test on bit 0 only | 5,029 |
| B2 `x0` bound `>=` | 1,751 |
| B3 `z0` bound against `fx` | 5,238 |
| B4 `x1` bound `<=` | 1,866 |
| B5 `z1` bound `<=` | 1,867 |
| B6 the quad's cursor read before the first commit | 7,459 |
| B7 draw mode `dfe` for `dtd` | 10,001 |
| B8 tpage `0x225` (the PSX's alternative) | 10,001 |
| B9 the colour dwords swapped by `Cond_ByteFF` | 10,001 |
| B10 channel `<< 2` | 10,001 |
| B11 bottom colour from bit 15 | 10,001 |
| B12 right edge 319 | 10,001 |
| B13 bottom row 239 | 10,001 |
| B14 the quad committed as `0x40` bytes | 10,001 |
| B15 semi-transparent | 10,001 |
| B16 `fx` from `0x8000` (as `fz`) | 12 |
| T1 the period from byte 1 | 22,083 |
| T2 the scan stops one threshold early | 4,034 |
| T3 source x high bits from 20 | 37,428 |
| T4 source x low bits from 8 | 39,546 |
| T5 height without `+ 1` | 39,854 |
| T6 width from the threshold byte | 39,682 |
| T7 commit to slot 7 | 39,854 |
| T8 the condition from word `+2` | 59,998 |
| T9 destination y `+ 0x140` | 39,854 |
| Z1 gated on slot 5 | 4,864 |
| Z2 the count without the `- 1` | 2,247 |
| Z3 width 6 bits | 554 |
| Z4 `x0` bound exclusive | 3,237 |
| Z5 depth bound inclusive | 2,422 |
| Z6 rounding at `0x7FFF` | 1,182 |
| Z8 slot 6 for sense 1 | 4,652 |
| Z9 flag bit 1 cleared | 4,113 |
| Z10 the fourth object tested by the leader's position | 3,062 |
| Z11 the PSX's stride `0x140` | 6,274 |
| P1 x 161 | 4,000 |
| P2 y at `+0x32` | 4,000 |

Two first tries were refused by a fault instead of a count and replaced (a
crash proves less): a period byte `+ 2` and a scan stopping at `>=` both
let the frame reach the `0xFF` terminator and the scan run off the entry -
the runaway of §3, reproduced by accident. **One control was not refused
and is information about the claim:** Z7, the cell compared unsigned
instead of the original's `movsx`, 0 mismatches in 40,000 rounds with
negative cells seeded. The bounds are bytes and a 7-bit width, at most 382,
so a cell with bit 15 set lies outside the box whether it is read as -1 or
as 65,535: the sign extension is unobservable, and the code keeps it only
because the original has it (§3 no longer claims it as behaviour).

B16's twelve: the two shifts differ only when the low byte of `0x7FFF -
MapView_FocusX` is `0xFF`, one round in 256, and then only at a bound.

**Seen on the way:** an all-shadow run (`BOF3X_SHADOW=*`, some twenty
seconds) ended three times in six with exit code 1, no `Fatal`, no `CRASH`
and the log cut at a different fuzz each time (`tex_cells`, `map_scroll`),
`BOF3X_EXITTRACE=1` logging nothing; each rerun passed whole (313 self-test
lines at 0 mismatches, 800 injects). Another session's `BOF3.exe` was
running at the time (`Get-Process`), so a `taskkill` by image name from it
is the likely cause (HANDOFF's trap: kill by pid); a run that ends with
exit 1 and no summary line was cut short, not failed.

**What the fuzz does not reach.** The real callees, and the wide quad: the
live check is the route's captures wide and narrow (`world-map.md` §1's
method), the sky continuous across all 852 columns at k = 2 in the hill and
coast frames, the middle 640 identical to the narrow capture.

**For the batch check** (`--original`):
`AreaMap_DrawBackdrop,AreaMap_TextureCycle,AreaMap_SlotZones,WorldMap_PinSprite`
- only the pin is reached by a traced route so far; the three handlers need
adding to `entries_plus_hidden.txt` first.

## 5. Possible defects of the 2001 code

Written down, not fixed.

- **The dropped tpage test.** The PSX picks tpage `0x225` over `0x95` by a
  mode test the port left out. Whether any PC area needed the other page -
  a backdrop with the wrong texture page set would show as the wrong
  blending, not the wrong colour, since the quad is untextured - is unread.
