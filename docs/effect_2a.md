# Group E2A: effect kinds 0x28..0x2E and the pool's specks, sparks and drops

**Status:** MEASURED (2026-09-29) - round thirteen, wave two
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 10), on the
round branch's tip `dcef661`. **65 functions ours** (`src/game/effect_2a.cpp`,
declarations generated from `symbols.toml` and in `src/game/effect_2a.h`,
shadow name `effect_2a`): the cut table's 64 rows for E2A
(`analysis/round13_cut.tsv`, the band `0x470300..0x473100`) and the one
function its span holds that no list had (`0x471A60`, kind 0x2B's ring draw),
each read to its last instruction with capstone and fuzzed through the
scenario harness's effect mode ([`scenario_harness.md`](scenario_harness.md)
section 8), the harness unchanged: 390,000 rounds, 0 mismatches. 134 controls
planted: 131 refused by a count, 3 equivalent mutants each with a refused
near variant (section 6).
Two of the 65 (`EffectSpecks_Spawn`, `EffectSpecks_Draw`) are entered by the
recorded whelp route through E1C's kind 0x1C (section 9); the rest are fuzz
only. No start dropped: none is a case or a shared tail.

Every row is effect code: the seven `hypothesis` rows are the four-instruction
dispatchers of kinds 0x28, 0x29, 0x2A, 0x2C and 0x2D (`Effect_KindHandlers`
entries 0x28..0x2E point at all seven), and the catalogued rows are their
states and draws. The names are hypotheses from what the code does
(`symbols.toml` status `hypothesis`): "beam", "box", "ring", "speck",
"spark", "drop", "curtain" name the shape the code builds - the points it
projects and the primitives it commits - not what the game shows. Where the
game shows each kind was not traced beyond the spawners below.

| Kind | Dispatcher (`Effect_KindHandlers` cell) | Table (named here, its length) | Spawned by |
|---|---|---|---|
| 0x28 | `EffectKind28_Run` `0x470300` (`0x6553F0`) | `EffectKind28_States` `0x6542F8` (4) | `Area42_Init` (`area_w1b.cpp`, `Cond_ByteFD` 1): `+6` = 2, the points (0x41.8, 0x10.8) and (0x4D.8, 0x10.8) |
| 0x29 | `EffectKind29_Run` `0x4709E0` (`0x6553F4`) | `EffectKind29_States` `0x654310` (6) | nothing found |
| 0x2A | `EffectKind2A_Run` `0x4710A0` (`0x6553F8`) | `EffectKind2A_States` `0x654334` (4) | nothing found |
| 0x2B | `EffectKind2B_Run` `0x471930` (`0x6553FC`) | `EffectKind2B_States` `0x65434C` (5) | `Scena09_Run11` (`scena_sc9a.cpp`, step 2): at (0x23.8, 0x1F.8) on the ground |
| 0x2C | `EffectKind2C_Run` `0x471EA0` (`0x655400`) | `EffectKind2C_States` `0x654360` (3) | nothing found |
| 0x2D | `EffectKind2D_Run` `0x472420` (`0x655404`) | `EffectKind2D_States` `0x6543B8` (8) | `Area67_SpawnEffect2D` (`area_w1e.cpp`, handler 2) at the running object's point |
| 0x2E | `EffectKind2E_Run` `0x472060` (`0x655408`) | `EffectKind2E_States` `0x6543AC` (3) | nothing found |

"Nothing found" is `grep` of `src/game` for a store of the kind into a
record's `+5` (2026-09-29); a kind stored from script data is not seen by it.
The spell and scenario files' `child[5] = 0x2A` / `0x2C` are the spell task
pool's kinds, not effect records.

**The tables.** No dispatcher bounds its index. Each table's length is its
own - the run of code pointers to the next table a dispatcher names, read by
hand (scratch `e2a/dz.py tab`): `0x6542F8` four, then two dwords of bytes
(`EffectKind28_PushParty`'s cells); `0x654310` six, then kind 0x2A's colours;
`0x654334` four, then kind 0x2A's rows (bytes); `0x65434C` five, then kind
0x2C's `0x654360` three, then kind 0x2C's offsets (dwords); `0x6543AC` three,
then kind 0x2D's `0x6543B8` eight, then kind 0x2F's `0x6543D8` (E2B's
`0x4731A0` indexes it). The run from `0x6543AC` is seventeen code pointers
long (`band_rows.py`'s "17 code entries"), but kind 0x2E's table is three:
nothing of kind 0x2E sets `+1` past 2 (section 7). `band_rows.py` names the
cells `run 0x65434c[5]`, `run 0x6543ac[3..10]` by the run, not the table.

## 1. What each function does

Offsets are of the effect record (`Sprite_Current`) unless a record is named.
`+1` is the kind's state, `+9` a frame count, the point `+0x34` / `+0x38` /
`+0x3C` (x, z, height; the height's integer part in its high word). Every
function's comment in `effect_2a.cpp` is the full read; each `symbols.toml`
evidence string the summary. "Read afresh" means the original reloads
`Sprite_Current` before an access (after a call the harness's disturbance may
have moved it); ours reads it where the original does.

### 1.1 Kinds 0x28 and 0x2A: a beam on the ground

A beam is two points (`+0xC..+0x14` and `+0x18..+0x20`, each x, z, height)
0x100 above the ground under the first. Kind 0x28's and kind 0x2A's draws are
**two copies of one routine** in the image: `0x470470` / `0x4713C0`,
`0x470640` / `0x471590`, `0x4707A0` / `0x4716F0` differ only in the colour
table's address (`0x6542EC` / `0x654328`, whose bytes are equal) and in which
copy they call (scratch `cmp.py`: 149, 103 and 165 instructions, those
operands the only differences). Ours has one implementation of each and
three thin entries per kind.

| State | Function | What |
|---|---|---|
| 0x28 / 0 | `EffectKind28_Start` `0x470320` | `AreaMap_Elevation(+0xC, +0x10)`; both heights `+0x14` / `+0x20` = (its low word, signed, + 0x100) << 16, `+0x3E` its low word, the dword `+0x70` 0; `+1` = 1 while story flag 0xB (`Flags_Test(0x904030, 0xB)`) is clear, else 2 |
| 0x28 / 1 | `EffectKind28_WaitFlag` `0x470390` | once flag 0xB is set, `+1` = 0 |
| 0x28 / 2 | `EffectKind28_Beam` `0x4703B0` | `EffectKind28_DrawBeam(+0xC, +0x18)`, `EffectKind28_PushParty`; while flag 0xB is clear, `+1` = 0 (the flag's answer) |
| 0x28 / 3 | FC1's `Effect_StateRelease` | |
| 0x2A / 0 | `EffectKind2A_Start` `0x4710C0` | `+1` = 2 while story flag 0xD is clear, else 1; `+0x3C` = 0x1000000 |
| 0x2A / 1 | `EffectKind2A_Beam` `0x471110` | `+6` = 1; the beam (0x60.8, 0x11.8) - (0x6B.8, 0x11.8) at the ground + 0x100, drawn (`EffectKind2A_DrawBeam`), its row pushed (`EffectKind2A_PushParty`); flag 0xD clear: `+1` = 2 |
| 0x2A / 2 | `EffectKind2A_Beams` `0x4711B0` | the same beam at z 0xD.8 (`+6` = 0) and 0x15.8 (`+6` = 2), each drawn and pushed; flag 0xD set: `+1` = 1 |
| 0x2A / 3 | `Effect_StateRelease` | |

So kind 0x28 draws one beam while flag 0xB is clear and goes idle once it is
set; kind 0x2A draws the middle beam while flag 0xD is set and the two outer
ones while it is clear.

- **`EffectKind28_PushParty` `0x4703F0`**: for the eight x cells of the byte
  table `0x654308` at z cell 0x10: `+0x34` = the cell << 16, `+0x38` =
  0x100000, `Party_MemberAt(+0x34, +0x38, 0)`; a member there, with
  `Field_ScriptFlags2`'s bits 0x1400 clear (the dword read, `test ah, 0x14`)
  and `Field_Request` 0: `Member_SetState2_8(member, 2)`. The record's point
  is left at the last cell.
- **`EffectKind2A_PushParty` `0x4712E0`**: for the ten x cells 0x61..0x6A,
  each at its middle (`| 0x8000`), at the z cell of `+6`'s row (two bytes at
  `0x654344 + 2 * +6`, the z and a state): `Party_MemberAt`; a member there
  (the same two tests): its `ObjTrio` record's `+8` = 5 when the row lies past
  its z (`+0x38`, signed), else 1; and when that record's `+1` is 1,
  `Member_SetState2_8(member, the row's second byte)`. The answer indexes
  `ObjTrio`: ours aborts on one past the three members (the callee answers
  0..2 or 0xFF).
- **`EffectKind28_DrawBeam` `0x470470`** (a, b): a draw mode (page 0x3C0, 0,
  transparency 1) committed in slot 3, `EffectGte_LoadMapCamera`; an opaque
  flat line (`Gpu_SetLineF2`) at the packet cursor from a's screen point to
  b's (`EffectGte_ProjectPoint` straight into the line), coloured by `+6`
  (three bytes at table + 3 * `+6`), committed 0x20 in slot 3. The angle
  `Math_Ratan2(dy, dx)` of the screen difference (each through `_ftol` and
  back to a float); at each end its size `EffectGte_ProjectSize(end, {0x40,
  0})` - the out's dword, both words - plus the frame's low bit, and
  `EffectKind28_DrawEnd(x, y, size, angle + 0x400)` at a, `+ 0xC00` at b (the
  screen words through `_ftol`); then `EffectKind28_DrawSides` between them.
- **`EffectKind28_DrawEnd` `0x470640`** (x, y, radius, angle): eight
  semi-transparent Gouraud triangles, each committed 0x34 in slot 3: the
  centre as floats coloured by `+6`, two rim points `(x + (cos(u) * r sar 12),
  y + (sin(u) * r sar 12))` shaded 0 at u and u + 0x100 from the angle's low
  word - half a circle. The angle is stepped whole in the original's argument
  slot and masked for each `Math_*` call.
- **`EffectKind28_DrawSides` `0x4707A0`** (x0, y0, r0, a0, x1, y1, r1, a1):
  a semi-transparent Gouraud quad from the two centres (coloured by `+6`) to
  circle points at a0 and at a1 + 0x800 (shaded 0), committed 0x44; then a
  copy of it at `p + 0x44` - not the cursor read again - with its far points
  at a0 + 0x800 and a1, committed 0x44. Every argument is read as its low word;
  the `+ 0x800`s are not masked again.

### 1.2 Kind 0x29: two boxes and five segments

| State | Function | What |
|---|---|---|
| 0 | `EffectKind29_Start` `0x470A00` | box A at `+0xC` (x, z, height) = (0x56.8, 0x12.8, 0x100), its half sizes `+0x1C` = 0 and `+0x20` = 0x2200000; box B at `+0x24` the same point, `+0x34` = 0x4000 and `+0x38` = 0x2200000 (`+0x18`, `+0x30` not written); `EffectKind29_SetSegments`; `+9` = 8, `+1` up |
| 1 | `EffectKind29_Extend` `0x470A70` | box B moved (x `+0x24` + 0x10000, half height `+0x38` + 0x220000, half depth `+0x34` + 0x2000); the segments to t = 0x10 - 2 `+9` (the pushed dword's upper bytes the new `+0x34`'s), shade 0xFF; the boxes; `+9` down, at 0 `+9` = 0x40 and `+1` up |
| 2 | `EffectKind29_Hold` `0x470AF0` | the boxes, the whole segments; `+9` down, at 0 `+9` = 4, `+1` up |
| 3 | `EffectKind29_Fade` `0x470B40` | `+0x1C` + 0x4000, `+0x34` + 0x10000; the boxes; the segments shaded 0xFF at `+9` = 8, else `+9` << 5; `+9` down, at 0 `+9` = 0x20, `+1` up |
| 4 | `EffectKind29_Linger` `0x470BC0` | the boxes; `+9` down, at 0 `+1` up |
| 5 | `Effect_StateRelease` | |

- **`EffectKind29_DrawBoxes` `0x470BF0`** (boxes = the record + 0xC): a draw
  mode committed 0xC in slot 1; the eight corners into `0x676080` (0x10
  each): box A at (x, z + dz, h + dh), (x, z - dz, h + dh), (x, z + dz, h),
  (x, z - dz, h), box B likewise; `EffectGte_LoadMapCamera`; five faces
  `EffectKind29_DrawFace` (0, 1, 2, 3 | 1, 1), (0, 1, 4, 5 | 1, 0), (2, 0, 6, 4
  | 1, 0), (1, 3, 5, 7 | 1, 0), (2, 3, 6, 7 | 1, 0).
- **`EffectKind29_DrawFace` `0x470D80`** (i0..i3, shade0, shade1): a
  semi-transparent Gouraud quad of the corners `0x676080 + 0x10 * index` (the
  low bytes; ours aborts past the eight), `EffectGte_ProjectPoint` each,
  shaded ((shade << 5) + the frame's bit) << 2 in byte arithmetic, blue at
  half; committed 0x44 in slot 1.
- **`EffectKind29_SetSegments` `0x470E70`**: thirty dword stores of
  constants - five segments at `0x675FE0` of 0x20, two points each (the dwords
  `+0xC` / `+0x1C` left as they are).
- **`EffectKind29_DrawSegments` `0x470F70`** (t, shade): a draw mode
  committed 0xC in slot 1, `EffectGte_LoadMapCamera`; for each segment a
  semi-transparent Gouraud line from A's screen point to that of A + ((B - A)
  * t sar 4) (32 bits a coordinate), shaded ((0x20 + bit) * shade * 4) >> 8 at
  A and (bit * 4 * shade) >> 8 at the far end, blue at half; committed 0x24.
  t and shade are read as bytes.

### 1.3 Kind 0x2B: a ring and its specks

The dispatcher **calls** the state (`call [EffectKind2B_States + 4 * +1]`,
not a `jmp`), then, when `+1` - read again - is not 0, tail-jumps to the ring
draw `0x471A60`, which no list held (`band_rows.py`: "code no list has, in the
span of 0x471a30"; the cut's 731 bytes for `0x471A30` ran on over it). It has
its own frame (`sub esp, 0x48` .. `ret`) and is reached only by that `jmp`: a
function, taken as `EffectKind2B_Draw`.

| State | Function | What |
|---|---|---|
| 0 | `EffectKind2B_Start` `0x471960` | the words `+0x2E` (radius) = 0x100, `+0x30` (rise) = 0, `+0x32` (count) = 0x18; `+1` up |
| 1 | `EffectKind2B_Rise` `0x471990` | `+0x30` up 0x80, `+0x32` down; at 0 `EffectShards_Clear` (E1C's), `+0x32` = 0x12C, `+1` up, `Sound_PlayEffect(0x209)` |
| 2 | `EffectKind2B_Specks` `0x4719E0` | on odd frames `EffectSpecks_Spawn`; `EffectSpecks_Move`; the counter byte `0x903848` at 0x29: `+1` up |
| 3 | `EffectKind2B_Settle` `0x471A10` | `EffectSpecks_Move` answering 0 (none left): `+0x32` = 0x10, `+1` up |
| 4 | `EffectKind2B_Shrink` `0x471A30` | `+0x2E` down 0x10, `+0x32` down; at 0 a tail `jmp` to `Effect_Release` |

Chapter 9's run 11 spawns kind 0x2B when its counter byte is 0x26 and raises
it later (`scena_sc9a.cpp`); the kind throws specks until the byte is 0x29 -
which later step of the script sets it was not traced.

- **`EffectKind2B_Draw` `0x471A60`**: `EffectGte_LoadMapCamera`; the ring's
  point at angle u is (x + (cos(u) * r sar 4), z + (sin(u) * r sar 4)), r the
  radius word (signed), (x, z) `+0x34` / `+0x38` read afresh after each
  `Math_*` call, projected at the ground `+0x3C` and at the top `+0x3C` +
  (`+0x30` << 16). For u = 0x100, 0x200, .. 0x1000 (masked for `Math_*`),
  sixteen semi-transparent Gouraud quads between the last angle's two
  projections and this one's, each linked at this angle's (x, z) by
  `MapView_LinkPrimAt` between a draw mode dithered before it and one not
  after it (0xC, 0x44, 0xC). The shade starts at (8 + the frame's bit) * 4;
  a quad's first two vertices take it, then it moves by 4 - down for quads
  4..11, up for the rest - and its last two take that (byte arithmetic, kept
  from quad to quad).
- **`EffectSpecks_Spawn` `0x471D10`** (also called by E1C's kinds 0x1C and
  0x1D): the first free speck of the pool (0x80 of 0x14 at
  `EffectKind30_Shards`, `+0` 0; none: return) gets `+0` 1, `+1` 0x40; a
  distance d = `Rand % +0x2E` (the word read again after `Rand`, `idiv`) when
  `+0x2E` is not 0, else 0; an angle `Rand & 0xFFF`; `+4` / `+8` = (cos / sin *
  d (s16) sar 4) + `+0x34` / `+0x38`; `+0xC` = (`+0x30` << 16) + `+0x3C`; the
  fall speed word `+2` = (`Rand & 0xFFF`) + 0x1000. It answers nothing a
  caller reads (E1C's callee comment "al the index" is not so: section 8).
- **`EffectSpecks_Move` `0x471DD0`**: each speck in use: its height `+0xC`
  down by `+2` << 8, drawn, freed once below `Sprite_Current`'s `+0x3C`
  (signed); al 1 when any was in use, else 0 (`Clone::ret_mask` 0xFF).
- **`EffectSpecks_Draw` `0x471E20`** (speck): its point projected into a
  stack vector; an opaque `TILE_1` there, white on an odd `Rand`, else black;
  `MapView_LinkPrimAt(x, z, 0, 0x14)`.

### 1.4 Kinds 0x2C and 0x2E: sparks

The sparks are the pool's 0x80 records of 0x28: `+0..+8` a point, `+0xC` a
fourth dword copied with it, `+0x10..+0x18` a velocity, `+0x20` a size, `+0x24`
a shade, `+0x25` frames left (0 free). The centre `0x92D380` and kind 0x2C's
eight ground points `0x92D390` lie right after the pool.

| State | Function | What |
|---|---|---|
| 0x2C / 0 | `EffectKind2C_Start` `0x471EC0` | the centre from `Sprite_ObjectsExtra[0]` (x, z, height sar 8); eight ground points at the offsets `0x65436C` / `0x65438C` from it, their heights `AreaMap_Elevation` << 8; `EffectSparks_Clear`; `+0xC` = 0x200, `+1` up |
| 0x2C / 1 | `EffectKind2C_Emit` `0x471F60` | when `+0xC`'s low four bits are 0: a spark per ground point (a free one; none: the next point) from the centre toward it - velocity (point - centre) sar 4, normalised in place (`Gte_VectorNormal`) -, shade 0x80, size 0x1000, 0x80 frames; the centre again; `EffectSparks_Move`; `+0xC` down, at 0 `+1` up |
| 0x2C / 2 | `Effect_StateRelease` | |
| 0x2E / 0 | `EffectKind2E_Start` `0x472080` | `EffectSparks_Clear`, `+1` up |
| 0x2E / 1 | `EffectKind2E_Trickle` `0x472090` | every eighth frame a still spark at `Sprite_Objects[1]`'s point (height sar 8), shade 0x80, size 0x800, 0x80 frames; `EffectSparks_Move`; the counter byte at 3: `+1` up |
| 0x2E / 2 | `EffectKind2E_Burst` `0x4720F0` | sixteen sparks (a spark not found skips its angle) at `Sprite_Objects[1]`'s point thrown flat at the angles 0x100 k (velocity sin sar 2, cos sar 2, 0), shade 0x40, size 0x800, 0x40 frames; `EffectSparks_Move`; the dword `+0xC` = 0x40. `+1` is not moved |

- **`EffectSparks_Move` `0x4721A0`**: `EffectGte_LoadMapCamera`; each spark
  alive: drawn, moved by its velocity (x, z, height), and when the ground
  (`AreaMap_Elevation(x, z)`'s low word, signed) is above its height: the
  height = the ground, the vertical velocity 0, the velocity normalised in
  place; shade down one, size up 4, frames down one.
- **`EffectSparks_Draw` `0x472240`** (spark): a draw mode (page 0x2C0, 0x100)
  linked at its x, z; a semi-transparent textured quad of half size
  `EffectGte_ProjectSize(point, {size >> 6, -})` round
  `EffectGte_ProjectPoint(point)`, the point (x, z, height << 8); CLUT
  (0xA0, 0x1E3), page (0x2C0, 0x100), the texture corners fixed, shaded
  `+0x24`; linked (1, 0x48). The original's second size word is stack it
  never wrote (the second quotient is not read): ours passes 0 there.
- **`EffectSparks_Clear` `0x4723E0`**: every spark's `+0x25` = 0.
- **`EffectSparks_FindFree` `0x472400`**: the first spark with `+0x25` 0, or
  0 (eax; `ret_mask` 0xFFFFFFFF).

### 1.5 Kind 0x2D: rays, a disc, rings, a curtain and drops

Its draws take the record + 0xC: a screen point (three floats: x, y, depth),
then the words `+0x18` (a radius), `+0x1A` (an angle) and `+0x1C` (a level).
Every primitive is linked by `MapView_LinkPrimAt` at the cell `0x676100` /
`0x676104`, which the start sets.

| State | Function | What |
|---|---|---|
| 0 | `EffectKind2D_Start` `0x472440` | the link cell = (0x1C0000, 0x180000); `EffectGte_LoadMapCamera`; `Sprite_Objects[0]`'s point projected into `+0xC..+0x14`, `Sprite_Objects[1]`'s into a stack vector, the two added (x87: section 2); `+0x18` / `+0x1A` 0; x and y scaled by the float `0x5C41D8`; `Msg_OpenScript(0x14)`, `Field_Request` 2, `+9` = 0x32, `+1` up, `Sound_PlayEffect(0x200)` |
| 1 | `EffectKind2D_Spin` `0x472530` | `+0x18` up 2 while `+9` is above 0x19, else down 2; `+0x1A` up 0x40; the rays; `+9` down, at 0 `+1` up |
| 2 | `EffectKind2D_Pause` `0x472580` | `+0x18` / `+0x1A` 0, `+9` = 0x5A, `+1` up, `Sound_PlayEffect(0x201)` |
| 3 | `EffectKind2D_Open` `0x4725B0` | `+0x18` up one, held at 0x40; `+0x1A` up 0x40; the rings; `+9` down - at 0 `EffectDrops_Clear`, `+0x1C` 0, `+9` 0, `+1` up |
| 4 | `EffectKind2D_Pour` `0x472620` | `+0x1A` up 0x40, `+0x1C` up 2; `+9` up (at 0xF `Sound_PlayEffect(0x202)`); the rings, `EffectDrops_Move`, the curtain; on odd frames a free drop launched; `+0x1C` above 0x100: `+9` = 7, `+1` up |
| 5 | `EffectKind2D_Close` `0x4726B0` | the curtain; `+0x18` down 8; `+9` down - at 0 the counter byte = 0x31, `+0x18` = 2, `+0x1A` = 0, `+9` = 0xF, `+1` up, `Sound_PlayEffect(0x203)` |
| 6 | `EffectKind2D_Lift` `0x472720` | the curtain; `+0x1A` up 2 and the screen y up by it; the curtain again; `+9` down, at 0 `+1` up |
| 7 | `EffectKind2D_End` `0x472770` | the byte `0x7DEE44` \|= 2, the counter byte = 0x32, a tail `jmp` to `Effect_Release` |

- **`EffectKind2D_DrawRays` `0x472790`**: eight semi-transparent Gouraud
  lines from the screen point (x, y copied, the depth at both ends) to (x +
  cos(u) r sar s, y + sin(u) r sar s), u = `+0x1A` + 0x200 k, s 12 on even k
  and 13 on odd; grey 0x80 at the centre, 0 at the tip; then
  `EffectKind2D_DrawDisc`; each between draw modes.
- **`EffectKind2D_DrawDisc` `0x472920`**: h = the radius word sar 2 (16
  bits); when not negative, for each row i = -h..h a half width w = the low
  word of `0x5A7A90(h * h - i * i)` (fild, fsqrt, `_ftol`: the square root
  truncated) and two semi-transparent `TILE_1`s grey 0x40 at (x + w, y + i)
  and, a copy right after it (not the cursor read again), at x - w.
- **`EffectKind2D_DrawRings` `0x472AB0`**: eight rings j between the radii j
  R / 8 and (j + 1) R / 8 (R the radius word; signed divides toward zero, each
  an s16), each sixteen semi-transparent Gouraud quads of an ellipse (x + cos
  r sar 12, y + sin r sar 13), the inner edge at A - 0x80 j + 0x100 n and the
  outer at A - 0x80 (j + 1) + 0x100 n (A the angle word), shaded 0 inside and
  0x40 outside; between a draw mode of transparency 2 and one of 1.
- **`EffectKind2D_DrawCurtain` `0x472F00`**: eight semi-transparent Gouraud
  quads over u = 0..0x800: at u and the next, a vertex at (x + cos(u) r sar 12,
  0.0) - the screen's top - and one at (the same x, y + sin(u) r sar 13); the
  shade the level word clamped to 0..0xFF, the first pair's before it moves by
  4 (up for quads 0 and 1, down after), the second pair's after (16 bits, kept
  from quad to quad); between a dithered draw mode and an undithered one.

The drops are the pool's 0x80 records of 0x18: `+0..+8` a screen point
(floats), `+0xC` a half length, `+0xE` a speed, `+0x10` / `+0x12` the top and
bottom bounds, `+0x14` frames left (0 free).

- **`EffectDrops_Clear` `0x472D70`**: every drop's `+0x14` = 0.
- **`EffectDrops_Move` `0x472D90`**: each drop alive: drawn; its y down by
  the speed (y - it); the speed + (`+0x14`'s low bit); `+0xC` up one; `+0x14`
  down one - at 0 launched again.
- **`EffectDrops_Draw` `0x472E00`**: an opaque flat quad, grey 0x80, x .. x +
  the float `0x5C41C0`, from y - `+0xC` (held at `+0x10` at least) to y +
  `+0xC` (held at `+0x12` at most), at the drop's depth. Each hold is an x87
  compare of the unrounded sum with the bound (`fcomp`, `test ah, 1` for the
  top and `test ah, 0x41` for the bottom): an unordered compare - a NaN -
  holds the top and not the bottom. Ours compares the double as the x87 at
  53 bits holds it.
- **`EffectDrops_FindFree` `0x472EE0`**: the first drop with `+0x14` 0, or 0.
- **`EffectDrops_Launch` `0x473100`** (drop): at the record's screen point
  (`Sprite_Current` read on entry) + (cos(a) d sar 12, sin(a) d sar 13), a =
  `Rand & 0xFC0`, d = `Rand & 0x38`; the depth copied; `+0x12` =
  `_ftol(y)`'s low word; `+0xC`, `+0xE`, `+0x10` 0; `+0x14` = 0x20.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor of its tables; checked 2026-09-29). Where the original indexes
past a table ours aborts with a `Fatal` naming the function (the owner's rule,
round9 doc section 6; nothing in ordinary play reaches one): the seven
dispatchers past their tables (the original jumps through the next table or
data); `EffectKind29_DrawFace` past the eight corners; `EffectKind2A_PushParty`
on a `Party_MemberAt` answer past the three members; `EffectSpecks_Spawn`'s
`idiv` by a radius word that is 0 (or -1 under `INT_MIN`) when read again after
`Rand` - the word is tested before `Rand` and re-read after it, which only a
`Sprite_Current` moved during the call can make differ.

**x87.** The game runs its FPU at 53 bits (`psx_gte_float.cpp`, measured), so
the originals' `fadd` / `fsub` / `fmul` of floats and whole numbers are
written in double (SSE2: the same rounding) and stored once. Two places are
x87's own: a float the original only loads and stores (`fld` / `fstp`) goes
through `CopyQuiet` - the x87 load turns a signalling NaN quiet -, and kind
0x2D's start adds two floats that may both be NaN, where the x87 keeps the one
with the larger significand and SSE the first operand: ours adds those three
with `flds` / `fadds` / `fstps` itself (`X87Add`), under the control word the
original runs with. The CRT's `_ftol` is reproduced (a 64-bit truncation, the
integer indefinite's low word 0 for NaN and out of range).

## 3. Arguments, masks, answers

| Callee | Pushed | Read by the callee |
|---|---|---|
| `EffectKind28_DrawEnd` / `2A_` | `_ftol` results, the size dword (both out words + the bit), the angle + 0x400 whole | x, y, radius `movsx` 16 bits; the angle `and 0xFFFF` |
| `EffectKind28_DrawSides` / `2A_` | the same eight | every one 16 bits |
| `EffectKind29_DrawSegments` | t: eax whose low byte is 0x10 - 2 `+9`, the upper bytes the new `+0x34`'s; the shade `+9` << 5 whole | both `and 0xFF` |
| `EffectKind29_DrawFace` | six immediates | index bytes (`and 0xFF`), shade bytes |
| `Member_SetState2_8` | the member: a stack dword whose low byte is `Party_MemberAt`'s al, the rest the entry `ecx` | its standard row's byte masks |

The fuzz lists the helpers with those masks (`k16`, `k8`) and draws their own
arguments with leftovers above. **Answers**: `EffectSpecks_Move` al
(`ret_mask` 0xFF); `EffectSparks_FindFree` and `EffectDrops_FindFree` a
pointer (0xFFFFFFFF); everything else is `void` - `EffectSpecks_Spawn` leaves
0x80 or the fall speed in eax, which neither caller reads.

## 4. The fuzz (`effect_2a_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_2a`, effect mode (`g.effect`, kinds
0x28..0x2E), 6,000 rounds a function (`BOF3X_E2A_ONLY=<name>` runs the clones
whose name holds it). Shapes: 51 `kEffect` (each clone's own kind; the seven
dispatchers' `state_span` their table's length - 4, 6, 4, 5, 3, 3, 8), 14
`kCall` (the helpers handed arguments: the records as their callers hand them
- the record + 0xC / + 0x18, a pool record -, the face indexes below eight
with leftovers). The seven tables are `DataTable`s, swapped for recorders on
both sides. **Regions** beyond effect mode's standard ones: kind 0x29's
segments and corners and kind 0x2D's link cell (`0x675FE0..0x676108`), and
the pool past the standard 0x644 bytes to the spark centre and points
(`0x92C5C4..0x92D410`).

**Callees**: the group's own called directly, by name, with the masks of
section 3 and the records they are handed hashed (`deref`); E1C's
`EffectShards_Clear`; the effect-standard rows (`Party_MemberAt` 0..2 or
0xFF, `Member_SetState2_8` writing what the real one writes,
`Effect_Release`, `Flags_Test`, `Gte_VectorNormal` filling its out, `Math_*`,
`Math_Ratan2`, `Msg_OpenScript`, `Sound_PlayEffect`, `Rand`, the `Gpu_*`,
`Gfx_CommitPrim` moving the cursor, `MapView_LinkPrimAt`,
`EffectGte_LoadMapCamera`, `0x5A7A90`), the CRT's `_ftol` `kThrough`.
**Re-listed** in the group: `EffectGte_ProjectPoint` - the point hashed (12
bytes: a stack vector's address differs between the copy and ours), out not
logged and filled with floats, one in eight a NaN of either sign or payload,
signalling or quiet, or a value past 2^63; `EffectGte_ProjectSize` - the point
hashed, the size hashed to its **first word only** (`EffectSparks_Draw`'s
second is stack the original never wrote), out filled; `AreaMap_Elevation` -
half the time a low word 0, 1, -1 or 2, where the sparks' heights are seeded;
`EffectSparks_FindFree` / `EffectDrops_FindFree` - a free record (as the real
ones test) from a start the answer picks, or 0 a quarter of the time.

**Seeds** (per round, after the harness's fill): `+9` at every compare (0, 1,
2, 7..9, 0xE, 0xF, 0x19, 0x1A, 0x80, 0xFF or random); the pool - sparks and
drops first, specks last (a drop's `+0x14` is a speck's `+0` every fifth drop):
a quarter of each family in use, no speck at all a quarter of the time, some
specks landing exactly on the ground after the move, some sparks at the ground
answers' heights, some drops with a whole y and bounds at, above and below
their top and bottom; every record's radius `+0x2E` not 0 (the spawn divides
by the word of the record current after `Rand`), the current one 0 now and
then; the counter byte at 3, 0x29 and their neighbours; `+6` below 3 two times
in three; the party's `+1` 1 half the time and their z at the rows;
`Field_Request` and `Field_ScriptFlags2`'s bits clear two times in three;
kind 0x2B's count at 0..2 and 0x12C; kind 0x2C's `+0xC` at its nibble and 1;
kind 0x2D's screen point floats (a NaN or an out-of-range one one time in
sixteen), its radius small (the disc draws 2 (r >> 2) + 1 rows) and at its
compares, its level at 0xFE..0x101. **Disturbance** (the group's, from the
hash only): `+9`, the radius `+0x2E` (never to 0), `+0x30`, `+0x32`, `+0x1A`,
`+0x1C`, the dword `+0xC`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_2a`,
exit 0): 390,000 rounds over 65 functions, 14,133,730 calls to the stand-ins,
**0 mismatches**; 28,712 bytes of state in 47 regions. Every entry of the
seven tables reached (each state's handler recorder 688..2,021 calls, FC1's
`Effect_StateRelease`, four cells, 5,859 together); `Effect_Release` 7,154,
`EffectShards_Clear` 1,214, `EffectSpecks_Spawn` 2,982, `EffectDrops_Clear`
482, `Member_SetState2_8` 14,367, `0x5A7A90` 134,011.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0, 687
self-test lines, every mismatch count 0, `inject: 7238 ours, 0 left original`;
`effect_2a` there 390,000 rounds, 14,125,392 calls, 0 mismatches. **With
`BOF3X_WIDE=1`**: `'*'` exit 0, 687 self-test lines, all 0 mismatches. No
silent death (exit 127) met. `tools/ledger_check.py`: 63 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read all 65 to the byte (10,694 bytes against
  the cut's 11,720): 33 differ by padding only; one by code - `0x471A30`, 731
  in the cut, is 0x25, and the rest is `0x471A60`. The non-hidden lines of
  `entries_logic.txt` that ran past their `ret` over the next start
  (`0x4707A0` 0x44E, `0x470F70` 0x362, `0x4716F0` 0x61B, `0x471E20` 0x371,
  `0x472400` 0x383, `0x473100` 0x1C8) have the read extents appended (section
  11).
- **Hidden starts**: 36, each an entry by address - an `Effect_KindHandlers`
  cell or a state-table cell - none a case or a shared tail, none holding our
  code as a fall-through. Their recorded hosts: `0x46FFB0` (E1D's
  `EffectKind26_DrawBand`, whose code ends at `0x4702F5` - its old line 0x434
  ran over kind 0x28's dispatcher and states; E1D appended 0x346) and this
  group's own `0x4707A0`, `0x470F70`, `0x4716F0`, `0x471E20`, `0x472400`.
- **`hypothesis` rows**: the seven dispatchers, each effect code (above):
  taken. No row is non-effect code.
- **Added**: `0x471A60` (section 1.3). **Dropped**: none.
- **The PSX twins** of the seventeen rows `0x472440..0x473100` (kind 0x2D and
  the drops; `pairs_propagated.json`) all lie in `AREA067`'s overlay
  (section `659384883b...`, `names/overlays.toml` of the sibling), the area
  whose handler 2 spawns kind 0x2D - the one case this round where a twin's
  overlay is the spawner's. The sibling's `names/*.toml` name none of them
  (the `area_records.toml` hits on the same addresses are other areas'
  sections); the names here are from the code.

## 6. Controls

Planted all at once behind `BOF3X_E2A_CTL=<n>` in a scratch copy of
`effect_2a.cpp` (scratch `e2a/plant.py`, `ctl.ps1`: plant, one rebuild, run
each under `BOF3X_E2A_ONLY=<its function>`, restore, rebuild; the committed
file has no switch). Counts are rounds refused of 6,000 (12,000 where the
filter runs two clones), in this worktree, with the final fuzz.

| # | Function | Plant | Refused |
|--:|---|---|--:|
| 1 | `EffectKind28_Run` | dispatch to the next entry | 6,000 |
| 2 | `EffectKind29_Run` | dispatch to the next entry | 6,000 |
| 3 | `EffectKind2A_Run` | dispatch to the next entry | 6,000 |
| 4 | `EffectKind2B_Run` | dispatch to the next entry | 6,000 |
| 5 | `EffectKind2C_Run` | dispatch to the next entry | 6,000 |
| 6 | `EffectKind2E_Run` | dispatch to the next entry | 6,000 |
| 7 | `EffectKind2D_Run` | dispatch to the next entry | 6,000 |
| 8 | `EffectKind28_Start` | height 0x101 above the ground | 6,000 |
| 9 | `EffectKind28_Start` | +0x3E not stored | 6,000 |
| 10 | `EffectKind28_Start` | the two states swapped | 6,000 |
| 11 | `EffectKind28_WaitFlag` | flag 0xC | 6,000 |
| 12 | `EffectKind28_Beam` | +1 = 1 when the flag is clear | 1,993 |
| 13 | `EffectKind28_PushParty` | z cell 0x11 | 6,000 |
| 14 | `EffectKind28_PushParty` | state 3 handed | 2,822 |
| 15 | `EffectKind28_PushParty` | Field_ScriptFlags2 & 0x1000 only | 354 |
| 16 | `EffectKind28_DrawBeam` | the line committed in slot 2 | 6,000 |
| 17 | `EffectKind28_DrawBeam` | size 0x41 | 6,000 |
| 18 | `EffectKind28_DrawBeam` | end angle + 0x401 | 6,000 |
| 19 | `EffectKind28_DrawBeam` | no frame bit at b | 4,333 |
| 20 | `EffectKind28_DrawBeam` | Ratan2 (dx, dy) | 5,048 |
| 21 | `EffectKind28_DrawBeam` | dx rounded, not truncated | 1,915 |
| 22 | `EffectKind28_DrawEnd` | rim sar 11 | 6,000 |
| 23 | `EffectKind28_DrawEnd` | rim shaded 1 | 6,000 |
| 24 | `EffectKind28_DrawEnd` | seven triangles | 6,000 |
| 25 | `EffectKind28_DrawEnd` | the angle masked before stepping (a 16-bit slot) | **0: equivalent** - the argument is masked to 16 bits after the step either way |
| 26 | `EffectKind28_DrawSides` | the far angle + 0x800 masked | 193 |
| 27 | `EffectKind28_DrawSides` | the copy 0x40 bytes | 6,000 |
| 28 | `EffectKind28_DrawSides` | the second quad at the far angle + 0x800 | 6,000 |
| 29 | `EffectKind29_Start` | +9 = 7 | 6,000 |
| 30 | `EffectKind29_Start` | box B depth 0x4001 | 6,000 |
| 31 | `EffectKind29_Extend` | t = 0x10 - +9 | 5,538 |
| 32 | `EffectKind29_Extend` | reload 0x3F | 434 |
| 33 | `EffectKind29_Hold` | reload 5 | 434 |
| 34 | `EffectKind29_Fade` | full shade at +9 = 7 | 912 |
| 35 | `EffectKind29_Fade` | box B depth + 0x10001 | 6,000 |
| 36 | `EffectKind29_Linger` | reload 1 | 441 |
| 37 | `EffectKind29_DrawBoxes` | the top at h - dh | 6,000 |
| 38 | `EffectKind29_DrawBoxes` | face 3 shaded (1, 1) | 6,000 |
| 39 | `EffectKind29_DrawFace` | blue not halved | 4,838 |
| 40 | `EffectKind29_DrawFace` | no frame bit | 3,688 |
| 41 | `EffectKind29_SetSegments` | one constant off | 6,000 |
| 42 | `EffectKind29_DrawSegments` | sar 3 | 5,975 |
| 43 | `EffectKind29_DrawSegments` | near shade 0x21 | 5,242 |
| 44 | `EffectKind2A_Start` | +0x3C = 0x1000001 | 6,000 |
| 45 | `EffectKind2A_Beam` | +6 = 2 | 6,000 |
| 46 | `EffectKind2A_Beam` | z 0x118001 | 6,000 |
| 47 | `EffectKind2A_Beams` | +1 = 2 once the flag is set | 3,992 |
| 48 | `EffectKind2A_Beam` | the ground + 0xFF | 12,000 |
| 49 | `EffectKind2A_PushParty` | facing 4 | 1,994 |
| 50 | `EffectKind2A_PushParty` | past >= 0 (the row at the member) | 396 |
| 51 | `EffectKind2A_PushParty` | the state from the row's first byte | 2,076 |
| 52 | `EffectKind2A_PushParty` | x cells from 0x62 | 6,000 |
| 53 | `EffectKind2A_PushParty` | Field_ScriptFlags2 & 0x1000 only | 345 |
| 54 | `EffectKind2A_DrawBeam` | kind 0x28's colours | 2,973 |
| 55 | `EffectKind2A_DrawEnd` | the centre 1.0 lower | 6,000 |
| 56 | `EffectKind2A_DrawSides` | the second quad not committed | 6,000 |
| 57 | `EffectKind2B_Run` | the ring drawn at +1 = 0 too | 1,221 |
| 58 | `EffectKind2B_Start` | count 0x17 | 6,000 |
| 59 | `EffectKind2B_Rise` | rise 0x81 | 5,985 |
| 60 | `EffectKind2B_Rise` | sound 0x20A | 1,181 |
| 61 | `EffectKind2B_Specks` | even frames | 6,000 |
| 62 | `EffectKind2B_Specks` | counter 0x28 | 666 |
| 63 | `EffectKind2B_Settle` | count 0x11 | 2,055 |
| 64 | `EffectKind2B_Shrink` | radius - 0xF | 5,994 |
| 65 | `EffectKind2B_Draw` | shade down for quads 4..10 | 6,000 |
| 66 | `EffectKind2B_Draw` | radius sar 5 | 6,000 |
| 67 | `EffectKind2B_Draw` | the trailing mode dithered | 6,000 |
| 68 | `EffectKind2B_Draw` | the rise read from +0 instead of +0x30 | 6,000 |
| 69 | `EffectSpecks_Spawn` | +1 = 0x41 | 6,000 |
| 70 | `EffectSpecks_Spawn` | the divisor from the record read before Rand | 112 |
| 71 | `EffectSpecks_Spawn` | fall speed & 0x7FF | 2,944 |
| 72 | `EffectSpecks_Move` | freed at or below | 3,420 |
| 73 | `EffectSpecks_Move` | answers 1 always | 1,471 |
| 74 | `EffectSpecks_Draw` | Rand & 2 | 2,507 |
| 75 | `EffectSpecks_Draw` | linked 0x10 | 6,000 |
| 76 | `EffectKind2C_Start` | ground << 7 | 6,000 |
| 77 | `EffectKind2C_Start` | +0xC = 0x1FF | 6,000 |
| 78 | `EffectKind2C_Emit` | every 8 frames | 47 |
| 79 | `EffectKind2C_Emit` | velocity sar 3 | 3,127 |
| 80 | `EffectKind2C_Emit` | shade 0x81 | 3,127 |
| 81 | `EffectSparks_Move` | held at or below the ground | 4,297 |
| 82 | `EffectSparks_Move` | size + 5 | 6,000 |
| 83 | `EffectSparks_Draw` | half size >> 5 | 5,999 |
| 84 | `EffectSparks_Draw` | CLUT x 0xA1 | 6,000 |
| 85 | `EffectSparks_Draw` | texture v 0x4E | 6,000 |
| 86 | `EffectSparks_Clear` | the last spark kept | 1,546 |
| 87 | `EffectSparks_FindFree` | by +0x24 | 5,606 |
| 88 | `EffectKind2E_Start` | +1 not moved | 6,000 |
| 89 | `EffectKind2E_Trickle` | every fourth frame | 748 |
| 90 | `EffectKind2E_Trickle` | height sar 7 | 561 |
| 91 | `EffectKind2E_Burst` | fifteen sparks | 6,000 |
| 92 | `EffectKind2E_Burst` | sin sar 3 | 6,000 |
| 93 | `EffectKind2E_Burst` | +0xC = 0x41 | 6,000 |
| 94 | `EffectKind2D_Start` | link x 0x1C0001 | 6,000 |
| 95 | `EffectKind2D_Start` | Field_Request 1 | 5,747 |
| 96 | `EffectKind2D_Start` | the depth scaled too | 5,109 |
| 97 | `EffectKind2D_Start` | the sums in SSE (its NaN rule) | 6 |
| 98 | `EffectKind2D_Spin` | grows while +9 >= 0x19 | 460 |
| 99 | `EffectKind2D_Pause` | +9 = 0x59 | 5,968 |
| 100 | `EffectKind2D_Open` | held at 0x3F | 1,690 |
| 101 | `EffectKind2D_Open` | level not cleared | 410 |
| 102 | `EffectKind2D_Pour` | sound at 0xE | 475 |
| 103 | `EffectKind2D_Pour` | ends above 0xFF | 786 |
| 104 | `EffectKind2D_Pour` | drops on even frames | 6,000 |
| 105 | `EffectKind2D_Close` | counter 0x30 | 448 |
| 106 | `EffectKind2D_Close` | radius - 7 | 5,535 |
| 107 | `EffectKind2D_Lift` | y + the word | 5,656 |
| 108 | `EffectKind2D_End` | \|= 4 | 4,517 |
| 109 | `EffectKind2D_DrawRays` | shift 11 / 12 | 5,643 |
| 110 | `EffectKind2D_DrawRays` | the depth copied without the quieting load | 88 |
| 111 | `EffectKind2D_DrawDisc` | one row fewer | 3,541 |
| 112 | `EffectKind2D_DrawDisc` | the mirror at x + w | 4,684 |
| 113 | `EffectKind2D_DrawDisc` | h = radius sar 1 | 4,248 |
| 114 | `EffectKind2D_DrawRings` | y sar 14 | 4,672 |
| 115 | `EffectKind2D_DrawRings` | the outer angle not stepped back | 6,000 |
| 116 | `EffectKind2D_DrawRings` | radii by an unsigned shift | 673 |
| 117 | `EffectKind2D_DrawCurtain` | clamped at 0xFE | 3,404 |
| 118 | `EffectKind2D_DrawCurtain` | up for three quads | 5,314 |
| 119 | `EffectKind2D_DrawCurtain` | top y 1.0 | 6,000 |
| 120 | `EffectDrops_Clear` | +0x12 cleared | 6,000 |
| 121 | `EffectDrops_Move` | speed + bit 1 | 6,000 |
| 122 | `EffectDrops_Move` | relaunched at 1 left | 6,000 |
| 123 | `EffectDrops_Draw` | the top held by a plain < | 29 |
| 124 | `EffectDrops_Draw` | the bottom held at or above | **0: equivalent** - at equality the hold stores the value already there |
| 125 | `EffectDrops_Draw` | width from 0x5C41D8 | 3,558 |
| 126 | `EffectDrops_FindFree` | by +0x12 | 5,973 |
| 127 | `EffectDrops_Launch` | y sar 12 | 4,930 |
| 128 | `EffectDrops_Launch` | 0x1F frames | 6,000 |
| 129 | `EffectDrops_Launch` | _ftol rounding to nearest | 1,966 |
| 130 | `EffectDrops_Launch` | distance & 0x3C | 2,970 |
| 131 | `EffectKind28_Start` | the ground read as unsigned | **0: equivalent** - (x << 16) keeps only x's low 16 bits, signed or not |
| 132 | `EffectKind28_DrawEnd` | the angle masked to 15 bits (25's near variant) | 3,196 |
| 133 | `EffectKind28_Start` | the ground sign-extended from its byte (131's near variant) | 2,987 |
| 134 | `EffectDrops_Draw` | the bottom held from one below its bound (124's near variant) | 6 |

**131 of 134 refused by a count.** Three are equivalent mutants, each with a
refused near variant: 25 (the angle slot masked before it is masked again for
the call; near variant 132, masked to 15 bits), 124 (the bottom held at
equality stores the bound, which is the value already there; near variant
134, held from one below), 131 (the ground's word read unsigned: `(x + 0x100)
<< 16` keeps x's low 16 bits either way; near variant 133, sign-extended from
its byte). **Not refused on the first run, the fuzz's fault** and refused
once it was louder: 72 and 73 (no speck landed on the ground exactly, and the
pool never lacked a live speck - a drop's frames word is every fifth speck's
in-use byte, so the specks are now seeded last), 81 (the ground stand-in
never met a spark's height: `AreaMap_Elevation` re-listed), 97 (two NaNs met
too seldom: one projection in eight now odd). 124 was first taken for the
fuzz's and is equivalent. Control 97 is the x87 one: SSE's first-operand NaN
rule in place of `X87Add` - refused in 6 rounds.

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x2E never leaves its burst.** (D202) `EffectKind2E_Burst` (state 2) does
  not move `+1`: once the counter byte reaches 3, the effect throws up to
  sixteen sparks every frame - as many as the pool has free - until something
  else releases the record. It sets the dword `+0xC` to 0x40 each frame,
  which nothing of the kind reads. Whether a script releases it was not
  traced.
- **Three particle families share one pool.** (D201) Specks (0x14), sparks (0x28)
  and drops (0x18) are all laid over `EffectKind30_Shards` (and FC2's kind
  0x30 and E1C's kind 0x1E use it with their own strides), each with its own
  "free" test: a drop's frames word is every fifth speck's in-use byte. Two
  kinds using different families at once would corrupt each other's records.
  Unreached as far as the code shows (no two of these kinds are spawned
  together by anything found).
- **`EffectSpecks_Spawn` divides by a word it tested before a call.** (D207) `+0x2E`
  is tested for 0, then `Rand` is called and the word read again for the
  `idiv`. Harmless in the game (`Sprite_Current` does not change during the
  call); ours aborts where the two reads differ and the second is 0.
- **`EffectSparks_Draw` hands `EffectGte_ProjectSize` a half-written size.** (D213)
  Its second word is the caller's stack; the quotient computed from it is not
  read, so nothing shows.
- **Kind 0x28's push leaves the record's point at its last cell** (D238), and kind
  0x2A's sets a member's `+8` (5 or 1) whether or not its `+1` lets
  `Member_SetState2_8` run.
- **The dispatchers index unchecked.** (D200) Every writer of `+1` in the band keeps
  it inside its table; ours aborts past one.

## 8. Calls across groups

**Outbound**: none raw. Every callee is Capcom's library layer or the CRT
(`0x5A7A90`, `_ftol`; `effect_2a_callees.h`) or already ours, called by name:
three of EGT's four helpers, E1C's `EffectShards_Clear`, FC1's `Effect_StateRelease`
(four table cells), the engine's (`band_rows.py --edges`: no edge into
another group of the round).

**Inbound from outside the group** (for the rebinding pass): E1C's
`EffectKind1C_Rise` / `_Hold` and `EffectKind1D_Rise` / `_Hold` call
`EffectSpecks_Spawn`, and `EffectKind1C_MoveShards` calls `EffectSpecks_Draw`
(`band_rows.py --edges`: E1C -> E2A 5; through `effect_1c_callees.h`'s `kShardSpawn` /
`kShardTile`, rebound here, section 10). `Effect_RunObjects` (ours) reaches
the seven dispatchers through `Effect_KindHandlers` `0x6553F0..0x655408`
(read in place). E1C's callee comment says `0x471D10` answers the index in al:
it does not (the found path leaves the fall speed), and E1C's callers ignore
it.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 64 rows. The first-call traces
(`analysis/calltrace/*/bof3x.calltrace.tsv`, 31 runs): `reach_whelp` enters
`0x471D10` (`EffectSpecks_Spawn`, from `0x46D94F` in E1C's
`EffectKind1C_Rise`) and `0x471E20` (`EffectSpecks_Draw`, from `0x46DD2C` in
`EffectKind1C_MoveShards`) - kind 0x1C's specks, the whelp route's one cycle
of kind 0x1C that wave one recorded; no run enters any other function of the
group, 28 of which (the non-hidden starts) were armed in `entries_logic.txt`.
So the whelp route's frame hash covers the two speck helpers; **every other
function is fuzz only** until the owner records a route through area 42
(kind 0x28), area 67 (kind 0x2D) or chapter 9's run 11 (kind 0x2B).

## 10. The rebinding

`band_rows.py --refs` and `grep -rn -i` of the 65 addresses and the seven
tables in `src/game`:

- **Rebound** (the round-nine form, the value unchanged): `effect_1c_callees.h`
  `kShardSpawn` = `bof3::addr::EffectSpecks_Spawn`, `kShardTile` =
  `bof3::addr::EffectSpecks_Draw` (E1C's fuzz keys on the value; the header now
  includes `symbols.gen.h`). Comments naming E2A's addresses by the old form
  in `effect_1c.cpp` (five) and `scena_sx2.cpp` (`Member_SetState2_8`'s
  callers `0x4703F0`, `0x4712E0`) now name the functions.
- **Left raw**: `effect_1c_fuzz.cpp`'s `CallSite` targets `0x471D10` and its
  two stand-in rows' labels (the fuzz's keys, left raw on purpose as in round
  ten); `scenario_harness_ekh.cpp`'s copy of `0x472770` (`EffectKind2D_End`) -
  a harness, which the brief forbids a group to edit; the fold would spell it
  `bof3::addr::EffectKind2D_End`. It works while its inject runs before ours.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block, 43
lines): the 36 hidden starts, `0x471A60`, and the read extents of the six
non-hidden lines that ran past their `ret` (their old lines left for the
merger to drop). The 22 other non-hidden starts were already listed with the
read extent.
