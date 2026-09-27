# Group S37: Magma Breath, Geo / Gaea's Breath and Combustion (MAGIC219, 220/221, 222)

**Status:** IN PROGRESS (2026-09-27). All 60 functions are ours
(`src/game/magic_s37.cpp`, shadow name `magic_s37`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 120,000 rounds. 334 negative controls planted: 331 refused by a count (exit 3), one (H4) by a fault with its near variant (H4b) refused by a count, two equivalent (G48, G51) with their near variants refused. Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S37
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 138 | 0x2AF | MAGIC219 | 0xDB | Magma Breath | `0x4F59D0..0x4F62BE` | 14 |
| 136, 146 | 0x2B0, 0x2B1 | MAGIC220 / MAGIC221 (one code) | 0xDC, 0xDD | Geo Breath, Gaea's Breath | `0x4F62C0..0x4F71E6` | 20 |
| 133 | 0x2B2 | MAGIC222 | 0xDE | Combustion | `0x4F71F0..0x4F8638` | 26 |

The extents are `tools/magic_rows.py --unit MAGIC219 / MAGIC220/MAGIC221 /
MAGIC222 --clones` (capstone recursive descent; no jump table, nothing
`REFUSED`). All 60 functions lie in the units' extents; none was found inside
or missing from them. MAGIC222's extent also holds `BattleFx_Finish`
(`0x4F7350`), ours before (`battle_odds.cpp`), which three of this group's
tables call. 10,889 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. Rows 136 and 146 point
at one entry (`0x4F62C0`): MAGIC220 and MAGIC221 are one body, so Geo Breath
and Gaea's Breath (if those are what they are) look the same. What each spell
looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Magma Breath (MAGIC219).**
  - `MagmaBreath_Task`, the kind-2 task: a four-entry stack table by +1
    (`_Start`, `_Spawn`, `_Strike`, `BattleFx_Finish`), then a walk of its
    own pool `MagmaBreath_Pool` (32 records of 0x84 bytes at `0x6B4A60`):
    each record with bit 0 of +0 runs through `MagmaBreathRecord_Task` as
    `Sprite_Current`, its +0x80 as the owner, the task's two cells put back.
  - `MagmaBreath_Start` clears the pool, centres the task on the side,
    restores CLUT rows 2 and 26 whole with their semi-transparency bits (the
    first word of each without) and plays sound 0x100.
  - `MagmaBreath_Spawn` takes 24 records (`MagmaBreath_PoolAlloc`), each
    numbered, owned by the task and delayed by `MagmaBreath_SpawnDelays[n]`
    plus one or two frames, counted in the task's +0xB.
  - `MagmaBreath_Strike` after eight frames plays sound 0x101 and flags the
    target 0x10; `BattleFx_Finish` waits for the count to empty.
  - A record (`MagmaBreathRecord_Run`, four steps by +2, the frame-offset
    table `0x9039D8` switched to the effects' `0x8E3580` around them):
    `_Launch` starts at the acting actor's sprite (`0x904B3C`), 0x800000 up,
    aimed at a point on a ring round the owner (radius and angle from its
    number); `_Fly` ticks the sprite's script, steers toward the point
    (`MagicFx_StepTowardPoint`, speed 0x40) until it is there
    (`MagicFx_NearPoint`) or has passed it (its heading turned by more than
    0x600 and less than 0xA00); `_Land` puts it on the point with animation
    1; `_Burn` plays the script, dims the glow and frees the record
    (`MagicFx_FreeCurrentRecord`) with the owner's count down.
  - While a record is live and past step 0: its screen point, its matrix
    (`MagicFx_PushRecordMatrix`) and a glow of 16 gouraud triangles
    (`MagmaBreathRecord_DrawGlow`, radius +0xA x 16).
  - **Shared with 22 files**: `MagicFx_PushRecordMatrix` (`0x4F6020`: the
    record's x / z and half the owner's height, no rotation - the body of
    MAGIC046/047's `ElemBreathMote_PushMatrix` with three stores reordered)
    and `MagicFx_FreeCurrentRecord` (`0x4F6290`: bytes +0..+4 of
    `Sprite_Current` cleared - the pools' free, bit 0 of +0 being their
    in-use bit). The second is the last step of records and children in
    groups C1, C2, S02, S07, S10, S11, S17, S19, S21, S28 and S29, which hold
    it as a raw address.
- **Geo Breath / Gaea's Breath (MAGIC220/221).**
  - `GeoBreath_Task`: `GeoBreath_Start`, MAGIC226/227's `0x4F9F70`,
    `BattleFx_Finish`; then a walk of `GeoBreath_Pool` (64 records at
    `0x6B5AE0`, right after Magma's) through `GeoBreathRecord_Task`.
  - `GeoBreath_Start` clears the pool, puts the task at the field's kind-2
    point (`Field_Kind2X` / `Z`), makes one child (kind 1, 0x61), restores
    CLUT row 26 and the first 16 words of row 2.
  - The child (`GeoBreathChild_Run`, eight steps): `_Start` rises 0x1600000
    over its owner with animation 0; `_Swirl` widens its glow for ten
    frames; `_Burst` (three bursts: +0xA) spawns 16 records a burst into the
    owner's pool with sounds 0x101 / 0x102 and hands to `_Shake`, which
    shakes the child about the owner by +0xC (the sign by `Frame_Counter`'s
    bit 0) for eight frames and steps back to `_Burst`; after the last burst
    `MapView_Redraw` is set 0x5D and `_Quake` (0x3C frames) and `_Settle`
    (0x20) shake it on, burst every eighth frame and move `Camera_ShiftX`
    (0x10 / 0 by the parity of +9, then +9 >> 1 / 0, then 0); group S18's
    `BuffRing_Wait` waits for
    the owner's count to fall to 2; `_End` fades the glow and ends with the
    script.
  - While live: the screen point, the record matrix, a glow of 64 triangles
    (`_DrawGlow`: radius +0xB, colour the signed +0x5D x 4 - MAGIC053's
    `LavaburstChild_DrawGlow` with its own words) and a ring of 64 quads
    (`_DrawRing`: from +0xB x 2 in to +0xB).
  - A record (`GeoBreathRecord_Run`, three steps): `_Start` at its number's
    angle (+0xB << 8) 24 units round the owner, `_Fly` out along it by 3 a
    frame for 8 frames, `_Fade` by 2 while its shade +0xA falls by 2, then
    freed; each frame the actor matrix and one textured quad
    (`GeoBreathRecord_Draw`: 0x100 radius, colour +0xA x 6, linked at its
    point with `MapView_LinkPrimAt`).
- **Combustion (MAGIC222).**
  - `Combustion_Task`: `Combustion_Start`, `Combustion_Wait` (sound 0x100
    after 16 frames), `BattleFx_Finish`.
  - `Combustion_Start` centres the task and puts it on the screen, makes two
    children (kind 1, 0x60): the glow (+1 1) and the sprite (+1 0); CLUT row
    26 and row 2's first 16 words.
  - `CombustionChild_Task` dispatches four kinds by +1 through
    `CombustionChild_Kinds`: the sprite, the glow, a mote, the flash.
  - The sprite (`CombustionSprite_Run`, six steps): `_Start` after 16 frames
    a copy of the owner's point 0x7800000 up with animation 0; `_Fall` drops
    by 0x600000 a frame for 16 frames, then sound 0x101, eight motes (+1 2),
    the camera's first angle up 0x14; `_Shake` rocks that angle by 0x14 for
    eight frames and sets it to 0xFD56; `_Flash` after 22 frames sound 0x102,
    one flash (+1 3), the sprite's +0 bit 0x20 and a grey tint 0x40; step 4
    is MAGIC226/227's `0x4FA390`; `_Fade` takes 0x10 off each tint byte a
    frame (wrapping past 0: 0x40, 0x30, .. 0, 0xF0, ..) and grows the scale
    +0x40 by 0x1800, ending when +0x5D reaches 0x80 (twelve frames from
    0x40).
  - The glow (`CombustionGlow_Run`, four steps: `_Start`, `_Grow`, group
    S17's `MagicFx_WaitA`, group S12's `MagicFx_CountDownRelease`): a glow
    fan and a ring (`_DrawGlow`, `_DrawRing`) of radius +0x14 growing by 6
    for 16 frames.
  - A mote (`CombustionMote_Run`, three steps): `_Start` on an orbit round
    the owner at its number's angle (+0xB << 9), radius (Rand & 7) + 8;
    `_Grow` and `_Shrink` widen the orbit by 1 a frame, `_Shrink` ending
    the task; each frame the actor matrix and the textured quad
    (`CombustionMote_Draw`, colour +0xA x 8). MAGIC053's
    `LavaburstRecord_Steps` (group S10) holds `CombustionMote_Start` too.
  - The flash (`CombustionFlash_Run`, five steps: `_Start` at the owner's
    screen point with the target flagged 0x10, group S30's
    `MagicFx_CountUp9By2`, `_Hold`, `_Spin`, `_End`) draws
    `CombustionFlash_Draw` twice a frame: 32 steps across the screen from
    the point less 0x100, four gouraud quads a step in screen space (floats)
    - two bands above the line and two below, their heights the radius
    words x sin of the step's angle.

## 2. Divergence

No ledger entry. Each function is a faithful replacement; a phase past any
of the fourteen dispatch tables (three stack tables, eleven `.data` tables)
aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3), where the original
would call through whatever follows the table.

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: `Gte_RotTrans` gets a flag pointer, the projections a
depth and a flag pointer. `MagmaBreathRecord_Fly` passes 0 where the
original passes an uninitialised stack word as `MagicFx_StepTowardPoint`'s
fourth argument, which that function never reads (`magic_lib.cpp`; the
standard recorder masks it).

## 3. Calls to other units

By raw address (a phase in a table or a direct call; never bound or renamed
here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F9F70` | MAGIC226/227 (S38) | entry 1 of `GeoBreath_Task`: +9 down, at 0 target flags 0x10 and +1 on |
| `0x4FA390` | MAGIC226/227 (S38) | entry 4 of `CombustionSprite_Steps` |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (`MagmaBreathRecord_Launch`, as S22 calls it) |

By name, already ours: `BattleFx_Finish` (`battle_odds.cpp`), `BuffRing_Wait`
(S18), `MagicFx_WaitA` (S17), `MagicFx_CountDownRelease` (S12),
`MagicFx_CountUp9By2` (S30), `MagicFx_CenterOnSide`,
`MagicFx_StepTowardPoint`, `MagicFx_NearPoint` (L), `MagicFx_PushActorMatrix`,
and the GTE / GPU / sprite / sound library.

**For the coordinator's rebinding** (these work as they are - the harness
finds a recorder by the address): `0x4F6290` is `MagicFx_FreeCurrentRecord`
(raw in C1, C2, S02, S07, S10, S11, S17, S19, S21, S28, S29), `0x4F6020` is
`MagicFx_PushRecordMatrix` (raw in S10), `0x4F7C40` is
`CombustionMote_Start` (raw in S10's `LavaburstRecord_Steps`) and `0x4F7320`
is `Combustion_Wait` (raw in S09's `DreamBreath_Task`, as `kDreamPhase1`).
`battle_fx_tasks.cpp` lists `0x4F6440` / `0x4F7380` (`GeoBreathChild_Task`,
`CombustionChild_Task`) among the kind-1 handlers by address; unchanged.
S38's `0x4FA440` is the byte-for-byte twin of `GeoBreathChild_DrawRing`
(one operand differs), and its `0x4F9D80` of the allocators.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `MagmaBreath_Pool` | `0x6B4A60` | 32 x 0x84 bytes |
| `GeoBreath_Pool` | `0x6B5AE0` | 64 x 0x84 bytes |
| `MagmaBreath_SpawnDelays` | `0x65C22C` | 24 bytes |
| `MagmaBreathRecord_TaskTable` | `0x65C244` | 1 |
| `MagmaBreathRecord_Steps` | `0x65C248` | 4 |
| `GeoBreathChild_TaskTable` | `0x65C258` | 1 |
| `GeoBreathChild_Steps` | `0x65C25C` | 8 |
| `GeoBreathRecord_TaskTable` | `0x65C27C` | 1 |
| `GeoBreathRecord_Steps` | `0x65C280` | 3 |
| `CombustionChild_Kinds` | `0x65C28C` | 4 |
| `CombustionSprite_Steps` | `0x65C29C` | 6 |
| `CombustionGlow_Steps` | `0x65C2B4` | 4 |
| `CombustionMote_Steps` | `0x65C2C4` | 3 |
| `CombustionFlash_Steps` | `0x65C2D0` | 5 |

Each count is where the next table starts (the dump of `0x65C244..0x65C2E3`
read 2026-09-27; `0x65C2E4` begins MAGIC223's). The pools' sizes are the
walks' and allocators' counts.

## 5. The fuzz

`BOF3X_SHADOW=magic_s37` runs `magic_harness::Run` over the 60 clones, 2,000
rounds each (in this worktree: 120,000 rounds, 6,353,917 calls to the
stand-ins, 0 mismatches, 34,348 bytes of state in 22 regions). The harness
is unchanged; `magic_s37_fuzz.cpp` adds:

- **callees** beyond the standard set: the sprite calls (`Sprite_ScriptTick`,
  `_SetAnimation`, `_ScriptTickOnce` and `_UpdateScreen` noting which sprite,
  the last also the frame-offset table the Run steps swap); the draw library
  (`Math_Sin` / `Math_Cos` moving a scratch or vertex word a third of the
  time, since the draws read them again after every call; `Math_Ratan2`;
  `Gfx_CommitPrim` and `MapView_LinkPrimAt` logging each primitive's bytes
  and moving `Gfx_PacketNext` through a buffer of the fuzz's own; the GPU
  setters; the GTE calls of `MagicFx_PushRecordMatrix` with `deref` and
  results written where the real ones write; the projections with `deref`
  on each vertex); the engine's `0x446770` (logs the pair and writes a new
  one); and this group's own direct calls (the record tasks and the free as
  `kPhase`, the two allocators as `kByte` 0xFF or an index, the matrix push
  and the draws);
- **regions**: `Gfx_PacketNext` and the packet buffer, the vertex and
  scratch words, `0x9039D8`, both pools, CLUT rows 2 and 26 of the strip and
  its source, `0x905E60..0x905E6F` (`Field_Kind2Z` / `X`, `MapView_Redraw`),
  `Camera_ShiftX`, `Camera_Angles[0]`;
- **the group disturbance**: `Gfx_PacketNext`, a vertex or scratch word, the
  frame-offset table pointer, a pool record's in-use bit, the kind-2 point,
  the actor sprite pointer `0x904B3C`, `Camera_ShiftX` / the angle /
  `MapView_Redraw`;
- **seeds**: every pool record's owner a real slot or record (the walks
  make it the owner cell, which the recorders write through); the actor
  sprite pointer a record; each dispatcher's phase inside its table; the
  allocators' pools full, full but one (first, last or any) or as filled;
  every counter one step before its threshold or at it; `MagmaBreathRecord_Fly`'s
  last heading random and `Math_Ratan2` answering a turn of 0x5FF..0x601,
  0x9FF..0xA01, 0x800 or 0 from it three times in four;
  `Frame_Counter & 7` 0 for the quake and settle; the bursts' +0xA 1..3;
  the tint +0x5D at 0x8F..0x91 for `CombustionSprite_Fade`.

Coverage (calls the originals made, this worktree): every recorder and every
phase of the fourteen tables is reached - e.g. `0x4F5BC0` 31,982 (the walk's
records), `0x4F6D90` 64,380, `0x4F6230` 48,000, `0x4F7190` 51,616,
`Math_Ratan2` 2,529, `MagicFx_NearPoint` 2,000, `BuffRing_Wait` 274,
`0x4FA390` 348, `MagicFx_CountUp9By2` 380.

## 6. Controls

334 plants, each put in `magic_s37.cpp` one at a time by a script (scratch,
not committed) that planted, rebuilt (checking the file recompiled), ran
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s37`, restored; after the last it
restored, rebuilt and ran the clean self-test (0 mismatches, exit 0). Ids: H
the shared helpers (the pool walk, allocator and clear, the CLUT restore, the
projections, the glow fan, the ring band, the textured quad), M MAGIC219, G
MAGIC220/221, C MAGIC222, X `CombustionFlash_Draw`. The kinds planted: every
constant and field offset, every branch's comparison, every call's
arguments and order, every table's entries, and each read the original makes
again after a call made before it (a "stale" plant: M20, M40, M41, M78,
M94, G10, G23, G27, G46, G63, G66..G68, G76, C26, C37, C61, C62, C74, C79,
H20, H34, H47, X16).

- **331 refused by a count** in the functions the plant touches.
- **H4** (the walk reading a record's owner from +0x7C) faulted: the owner
  cell then held a non-pointer the recorders write through. Its near variant
  **H4b** (the owner cell not set for the record) was refused by a count in
  both walks (2,000 rounds each).
- **G48, equivalent**: dropping `GeoBreathChild_Settle`'s final
  `Camera_ShiftX = 0`. The step reaches it only when +9 was 1, and then the
  line before has already stored 1 >> 1 = 0, with no call between. Near
  variant **G48b** (1 there) refused (385 rounds).
- **G51, equivalent**: `GeoBreathChild_End` testing the whole eax of
  `Sprite_ScriptTickOnce` instead of al. Ours calls it through its
  `unsigned char` type, so the compiler tests al either way - no build of
  ours can tell them apart. Near variant **G51b** (`& 0xFE`) was at first
  **not refused**: a `kFlag` "yes" always has bit 4 (the harness's `h |
  0x10`, S09's D63b), so al was never exactly 1. The fuzz file now answers
  al 1 a quarter of the time for this callee (`NoteSpriteOnce`; the real
  one answers 0 or 1, `sprite_anim.cpp`), G51b was refused (313 rounds),
  and the controls of the three functions that call it (M75..M78,
  G20..G23, G50..G52) were run again, all refused.

| Id | Was | Planted | Result |
|---|---|---|---|
| H1 | `if ((rec[0] & 1) == 0` | `if ((rec[0] & 3) == 0` | refused: MagmaBreath_Task 2000, GeoBreath_Task 2000 |
| H2 | `SetLong(Mem(at::kOwner), owner); Sprite_` | `Sprite_` | refused: MagmaBreath_Task 1656, GeoBreath_Task 1597 |
| H3 | `::kOwner), owner); Sprite_Current = self;` | `::kOwner), owner);` | refused: MagmaBreath_Task 1974, GeoBreath_Task 1981 |
| H4 | `er = Long(rec + 0x80);` | `er = Long(rec + 0x7C);` | refused by a fault (rc 3221225477) |
| H5 | `return 0xFF; }` | `return 0xFE; }` | refused: MagmaBreath_PoolAlloc 676, GeoBreath_PoolAlloc 662 |
| H6 | `rec[0] = static_cast<unsigned char>(rec[0] / 1);` | `rec[0] = 1;` | refused: MagmaBreath_PoolAlloc 1319, GeoBreath_PoolAlloc 1324 |
| H7 | `t<unsigned char>(i);` | `t<unsigned char>(i ^ 1);` | refused: MagmaBreath_PoolAlloc 1324, GeoBreath_PoolAlloc 1338 |
| H8 | `rec[1] = 0; rec[2] = 0; } }` | `rec[1] = 0; } }` | refused: MagmaBreath_Start 2000, GeoBreath_Start 2000 |
| H9 | `= 0x1A00; k < 0x1B00; ++k)` | `= 0x1A00; k < 0x1AFF; ++k)` | refused: GeoBreath_Start 2000, Combustion_Start 2000 |
| H10 | `k < 0x200` | `k <= 0x200` | refused: GeoBreath_Start 2000, Combustion_Start 2000 |
| H11 | `Gfx_ClutStrip[0x1A00] = first26; Gfx_ClutStrip[0x200]` | `Gfx_ClutStrip[0x200]` | refused: GeoBreath_Start 989, Combustion_Start 988 |
| H12 | `_ClutStripDirty = 1; } /` | `_ClutStripDirty = 2; } /` | refused: GeoBreath_Start 2000, Combustion_Start 2000 |
| H13 | `(VP(0), VP(8), VP(0x10), pri` | `(VP(0), VP(0x10), VP(8), pri` | refused: MagmaBreathRecord_DrawGlow 2000, GeoBreathChild_DrawGlow 2000, CombustionGlow_DrawGlow 2000 |
| H14 | `prim + 0x28, prim + 0x38, &p,` | `prim + 0x38, prim + 0x28, &p,` | refused: GeoBreathChild_DrawRing 2000, GeoBreathRecord_Draw 2000, CombustionGlow_DrawRing 2000, CombustionMote_Draw 2000 |
| H15 | `fx_PacketNext, 0, 1, tpag` | `fx_PacketNext, 0, 0, tpag` | refused: MagmaBreathRecord_DrawGlow 2000, GeoBreathChild_DrawGlow 2000, GeoBreathChild_DrawRing 2000, GeoBreathRecord_Draw 2000, CombustionGlow_DrawGlow 2000, CombustionGlow_DrawRing 2000, CombustionMote_Draw 2000, CombustionFlash_Draw 2000 |
| H16 | `2,` | `3,` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H17 | `long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,` | `long>(Long(s + 0x38)), static_cast<unsigned long>(Long(s + 0x34)), 2,` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H18 | `a = 0x40; a < 0x1040; a +` | `a = 0x40; a < 0x1000; a +` | refused: GeoBreathChild_DrawGlow 2000, CombustionGlow_DrawGlow 2000 |
| H19 | `SetVW(0xA, y);` | `SetVW(0xA, x);` | refused: GeoBreathChild_DrawGlow 1999, CombustionGlow_DrawGlow 2000 |
| H20 | `(0xA, y); v = Sin(a); SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));` | `(0xA, y); { const short r0 = SS(0); v = Sin(a); SetVW(0x10, static_cast<unsigned>(Mul12(v, r0))); }` | refused: GeoBreathChild_DrawGlow 1491, CombustionGlow_DrawGlow 1448 |
| H21 | `p[k] = SB(0xA);` | `p[k] = SB(0xB);` | refused: GeoBreathChild_DrawGlow 2000, CombustionGlow_DrawGlow 2000 |
| H22 | `x16u, 0x24u, 0x25u, 0x26u}` | `x16u, 0x24u, 0x25u}` | refused: GeoBreathChild_DrawGlow 2000, CombustionGlow_DrawGlow 2000 |
| H23 | `CommitPrim)(5, 0x34);` | `CommitPrim)(5, 0x30);` | refused: GeoBreathChild_DrawGlow 2000, CombustionGlow_DrawGlow 2000 |
| H24 | `PrimDepths3_10B)(p);` | `PrimDepths3_10B)(p + 4);` | refused: GeoBreathChild_DrawGlow 2000, CombustionGlow_DrawGlow 2000 |
| H25 | `SetVW(4, 0);` | `SetVW(4, 1);` | refused: GeoBreathChild_DrawGlow 1950, CombustionGlow_DrawGlow 1965 |
| H26 | `ner = Mul12(v, SS(2));` | `ner = Mul12(v, SS(0));` | refused: GeoBreathChild_DrawRing 2000, CombustionGlow_DrawRing 2000 |
| H27 | `SetVW(0x10, x); SetVW(0x12, y);` | `SetVW(0x10, y); SetVW(0x12, x);` | refused: GeoBreathChild_DrawRing 2000, CombustionGlow_DrawRing 2000 |
| H28 | `p[0x16] = 1;` | `p[0x16] = 2;` | refused: GeoBreathChild_DrawRing 2000, CombustionGlow_DrawRing 2000 |
| H29 | `x26u, 0x34u, 0x35u, 0x36u}` | `x26u, 0x34u, 0x35u}` | refused: GeoBreathChild_DrawRing 2000, CombustionGlow_DrawRing 2000 |
| H30 | `CommitPrim)(5, 0x44);` | `CommitPrim)(5, 0x40);` | refused: GeoBreathChild_DrawRing 2000, CombustionGlow_DrawRing 2000 |
| H31 | `PrimDepths4_10B)(p);` | `PrimDepths4_10B)(p + 1);` | refused: GeoBreathChild_DrawRing 2000, CombustionGlow_DrawRing 2000 |
| H32 | `SetVW(0x1C, 0);` | `SetVW(0x1C, 1);` | refused: GeoBreathChild_DrawRing 1916, CombustionGlow_DrawRing 1910 |
| H33 | `gned>(Mul12(v, SS(2))));` | `gned>(Mul12(v, SS(0))));` | refused: GeoBreathChild_DrawRing 1821, CombustionGlow_DrawRing 1842 |
| H34 | `v = Cos(a); { const int inner = Mul12(v, SS(2)); const std::uint16_t x = VW(0x18);` | `const std::uint16_t x0 = VW(0x18); v = Cos(a); { const int inner = Mul12(v, SS(2)); const std::uint16_t x = x0;` | refused: GeoBreathChild_DrawRing 961, CombustionGlow_DrawRing 1035 |
| H35 | `SetSW(0, 0x100);` | `SetSW(0, 0x101);` | refused: GeoBreathRecord_Draw 1950, CombustionMote_Draw 1954 |
| H36 | `{0x200, 0x600, 0xE00, 0xA00}` | `{0x200, 0x600, 0xA00, 0xE00}` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H37 | `int v = Cos(kAngl` | `int v = Sin(kAngl` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H38 | `SetVW(c * 8 + 4, 0);` | `SetVW(c * 8 + 4, 1);` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H39 | `(0, 1, 0x340, 0x100)` | `(0, 1, 0x340, 0x101)` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H40 | `u_GetClut)(0, 0x1E2)` | `u_GetClut)(0, 0x1E3)` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H41 | `p[0x45] = 0xA0;` | `p[0x45] = 0xA1;` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H42 | `p[0x24] = 0x20;` | `p[0x24] = 0x21;` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H43 | `p[6] = SB(0xA);` | `p[6] = SB(0xB);` | refused: GeoBreathRecord_Draw 1986, CombustionMote_Draw 1991 |
| H44 | `Gte_PrimDepths4_10)(p);` | `Gte_PrimDepths4_10B)(p);` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H45 | `LinkAtSprite(0x48);` | `LinkAtSprite(0x44);` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| H46 | `signed b = Sc()[0xA];` | `signed b = Sc()[0xB];` | refused: GeoBreathRecord_Draw 1668, CombustionMote_Draw 1678 |
| H47 | `LinkAtSprite(0xC); unsigned char* const p = Gfx_PacketNext;` | `unsigned char* const p = Gfx_PacketNext; LinkAtSprite(0xC);` | refused: GeoBreathRecord_Draw 2000, CombustionMote_Draw 2000 |
| M1 | `ddr::MagmaBreath_Spawn, addr::MagmaBreath_Strike,` | `ddr::MagmaBreath_Strike, addr::MagmaBreath_Spawn,` | refused: MagmaBreath_Task 1003 |
| M2 | `ool, kMagmaRecords, addr` | `ool, kMagmaRecords - 1, addr` | refused: MagmaBreath_Task 1054 |
| M3 | `gmaRecords, addr::MagmaBreath` | `gmaRecords, addr::GeoBreath` | refused: MagmaBreath_Task 2000 |
| M4 | `ool, kMagmaRecords);` | `ool, kMagmaRecords - 1);` | refused: MagmaBreath_Start 2000 |
| M5 | `Sc()[0xB] = 0;` | `Sc()[0xB] = 1;` | refused: MagmaBreath_Start 1991 |
| M6 | `gned k = 0; k < 0x100; ++k)` | `gned k = 0; k < 0xFF; ++k)` | refused: MagmaBreath_Start 2000 |
| M7 | `0x200 + k] / 0x8000);` | `0x200 + k] / 0x8001);` | refused: MagmaBreath_Start 2000 |
| M8 | `tStripSource[0x1A00 + k]` | `tStripSource[0x1A01 + k]` | refused: MagmaBreath_Start 2000 |
| M9 | `Gfx_ClutStrip[0x200] = first2; Gfx_ClutStrip[0x1A00]` | `Gfx_ClutStrip[0x1A00]` | refused: MagmaBreath_Start 994 |
| M10 | `_ClutStripDirty = 1;` | `_ClutStripDirty = 0;` | refused: MagmaBreath_Start 2000 |
| M11 | `1; Sound(0x100);` | `1; Sound(0x101);` | refused: MagmaBreath_Start 2000 |
| M12 | `l, kMagmaRecords); MH_CALL(MagicFx_CenterOnSide)();` | `l, kMagmaRecords);` | refused: MagmaBreath_Start 2000 |
| M13 | `igned i = 0; i < 24; ++i)` | `igned i = 0; i < 23; ++i)` | refused: MagmaBreath_Spawn 2000 |
| M14 | `if (index == 0xFF) con` | `if (index >= 0x1F) con` | refused: MagmaBreath_Spawn 1054 |
| M15 | `rec[1] = 0;` | `rec[1] = 1;` | refused: MagmaBreath_Spawn 2000 |
| M16 | `t<unsigned char>(i);` | `t<unsigned char>(i + 1);` | refused: MagmaBreath_Spawn 2000 |
| M17 | `(r & 1) + Me` | `(r & 3) + Me` | refused: MagmaBreath_Spawn 2000 |
| M18 | `em(kMagmaDelays)[i] + 1)` | `em(kMagmaDelays)[i + 1] + 1)` | refused: MagmaBreath_Spawn 2000 |
| M19 | `MagmaDelays)[i] + 1)` | `MagmaDelays)[i] + 2)` | refused: MagmaBreath_Spawn 2000 |
| M20 | `SetLong(rec + 0x80, static_cast<std::int32_t>(Key(Sc())));` | `unsigned char* const self = Sc(); SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self))); (and a second edit)` | refused: MagmaBreath_Spawn 954 |
| M21 | `Sc()[9] = 8;` | `Sc()[9] = 9;` | refused: MagmaBreath_Spawn 2000 |
| M22 | `)(TargetByte(), 0x10);` | `)(TargetByte(), 0x20);` | refused: MagmaBreath_Strike 495 |
| M23 | `if (Sc()[9] != 0) retu` | `if (Sc()[9] > 1) retu` | refused: MagmaBreath_Strike 518 |
| M24 | `Sound(0x101);` | `Sound(0x102);` | refused: MagmaBreath_Strike 495 |
| M25 | `{addr::MagmaBreath` | `{addr::GeoBreath` | refused: MagmaBreathRecord_Task 2000 |
| M26 | `int32_t>(kFrameSetEffect));` | `int32_t>(kFrameSetBattle));` | refused: MagmaBreathRecord_Run 790 |
| M27 | `if (s[0] != 0` | `if ((s[0] & 1) != 0` | refused: MagmaBreathRecord_Run 386 |
| M28 | `SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle)); } //` | `} //` | refused: MagmaBreathRecord_Run 2000 |
| M29 | `MagmaBreathRecord_Fly, addr::MagmaBreathRecord_Land,` | `MagmaBreathRecord_Land, addr::MagmaBreathRecord_Fly,` | refused: MagmaBreathRecord_Run 1011 |
| M30 | `Sc()[8] = actor[8];` | `Sc()[8] = actor[9];` | refused: MagmaBreathRecord_Launch 523 |
| M31 | `Sc() + 0xC, 0x20000);` | `Sc() + 0xC, 0x20001);` | refused: MagmaBreathRecord_Launch 528 |
| M32 | `) + 0x10, 0); Turn(Sc()); int v;` | `) + 0x10, 0); int v;` | refused: MagmaBreathRecord_Launch 529 |
| M33 | `U(Long(actor + 0x34))` | `U(Long(actor + 0x38))` | refused: MagmaBreathRecord_Launch 529 |
| M34 | `+ 0x3C)) + 0x800000u` | `+ 0x3C)) + 0x800001u` | refused: MagmaBreathRecord_Launch 514 |
| M35 | `SD(0, ((b >> 3) + 2) << 4` | `SD(0, ((b >> 3) + 3) << 4` | refused: MagmaBreathRecord_Launch 515 |
| M36 | `(b & 7) * 2) & 0xF) <<` | `(b & 7) * 2) & 0x1F) <<` | refused: MagmaBreathRecord_Launch 248 |
| M37 | `>> 3) + (b & 7) * 2) & 0x` | `>> 3) + (b & 7) * 3) & 0x` | refused: MagmaBreathRecord_Launch 471 |
| M38 | `v = Sin(stati` | `v = Cos(stati` | refused: MagmaBreathRecord_Launch 529 |
| M39 | `Long(Owner() + 0x34));` | `Long(Owner() + 0x38));` | refused: MagmaBreathRecord_Launch 529 |
| M40 | `nt32_t x = Mul(v, SD(0)) + U` | `nt32_t x = Mul(v, static_cast<std::int32_t>(((Sc()[0xB] >> 3) + 2) << 4)) + U` | refused: MagmaBreathRecord_Launch 50 |
| M41 | `v = Cos(SD(4));` | `v = Cos(static_cast<int>((((Sc()[0xB] >> 3) + (Sc()[0xB] & 7) * 2) & 0xF) << 8));` | refused: MagmaBreathRecord_Launch 43 |
| M42 | `x14, Owner() + 0x3C);` | `x14, Owner() + 0x38);` | refused: MagmaBreathRecord_Launch 529 |
| M43 | `s[0x26] = 0xC0;` | `s[0x26] = 0xC1;` | refused: MagmaBreathRecord_Launch 529 |
| M44 | `s[0x27] = 2;` | `s[0x27] = 3;` | refused: MagmaBreathRecord_Launch 529 |
| M45 | `s[0x2B] = 0;` | `s[0x2B] = 1;` | refused: MagmaBreathRecord_Launch 529 |
| M46 | `ite_SetAnimation)(0);` | `ite_SetAnimation)(1);` | refused: MagmaBreathRecord_Launch 529 |
| M47 | `atic_cast<float>(dx), static_cast<float>(dz));` | `atic_cast<float>(dz), static_cast<float>(dx));` | refused: MagmaBreathRecord_Launch 529 |
| M48 | `Sc()[9] = 3;` | `Sc()[9] = 4;` | refused: MagmaBreathRecord_Launch 529 |
| M49 | `; Sc()[0xA] = 4;` | `; Sc()[0xA] = 5;` | refused: MagmaBreathRecord_Launch 529 |
| M50 | `s[0x48] = 0;` | `s[0x48] = 1;` | refused: MagmaBreathRecord_Launch 529 |
| M51 | `) - U(Long(s + 0x38))` | `) - U(Long(s + 0x34))` | refused: MagmaBreathRecord_Launch 529 |
| M52 | `(Sprite_ScriptTick)();` | `(Sprite_ScriptTickOnce)();` | refused: MagmaBreathRecord_Fly 1032 |
| M53 | `if (c != 0) {` | `if (c > 1) {` | refused: MagmaBreathRecord_Fly 6 |
| M54 | `(s + 0x1C, s + 0x18);` | `(s + 0x1C, s + 0x14);` | refused: MagmaBreathRecord_Fly 2000 |
| M55 | `(px >> 9) - 0x4000;` | `(px >> 9) - 0x4001;` | refused: MagmaBreathRecord_Fly 2000 |
| M56 | `z = (pz >> 9) - 0x` | `z = (pz >> 8) - 0x` | refused: MagmaBreathRecord_Fly 2000 |
| M57 | `(U(h) >> 16) + 0xC0) >> 1` | `(U(h) >> 16) + 0xC1) >> 1` | refused: MagmaBreathRecord_Fly 1030 |
| M58 | `h) >> 16) + 0xC0) >> 1;` | `h) >> 16) + 0xC0) / 2;` | refused: MagmaBreathRecord_Fly 523 |
| M59 | `t<short>(U(h) >> 16) + 0x` | `t<short>(U(h) >> 15) + 0x` | refused: MagmaBreathRecord_Fly 2000 |
| M60 | `U(z), U(y), 0, 0x40);` | `U(z), U(y), 0, 0x41);` | refused: MagmaBreathRecord_Fly 2000 |
| M61 | `(U(x), U(z), U(y` | `(U(z), U(x), U(y` | refused: MagmaBreathRecord_Fly 2000 |
| M62 | `(U(px), U(pz), 0x8000) != 0` | `(U(px), U(pz), 0x7FFF) != 0` | refused: MagmaBreathRecord_Fly 2000 |
| M63 | `(U(px), U(p` | `(U(Long(Sc() + 0xC)), U(p` | refused: MagmaBreathRecord_Fly 138 |
| M64 | `if (MH_CALL(MagicFx_NearPoint)(U(px), U(pz), 0x8000) != 0` | `if ((MH_CALL(MagicFx_NearPoint)(U(px), U(pz), 0x8000) & 0xFF) != 0` | refused: MagmaBreathRecord_Fly 322 |
| M65 | `if (turned > 0x600` | `if (turned >= 0x600` | refused: MagmaBreathRecord_Fly 25 |
| M66 | `0x600 && turned < 0xA00` | `0x600 && turned <= 0xA00` | refused: MagmaBreathRecord_Fly 18 |
| M67 | `if (Long(s + 0x1C) < 0) SetL` | `if (false) SetL` | refused: MagmaBreathRecord_Fly 149 |
| M68 | `ng(s + 0x1C)) & 0xFFF) -` | `ng(s + 0x1C)) & 0x7FF) -` | refused: MagmaBreathRecord_Fly 159 |
| M69 | `atic_cast<float>(dx), static_cast<float>(dz));` | `atic_cast<float>(dz), static_cast<float>(dx));` | refused: MagmaBreathRecord_Fly 2000 |
| M70 | `(s + 0x3C, s + 0x14);` | `(s + 0x3C, s + 0x10);` | refused: MagmaBreathRecord_Land 2000 |
| M71 | `s[0x2B] = 1;` | `s[0x2B] = 0;` | refused: MagmaBreathRecord_Land 2000 |
| M72 | `ite_SetAnimation)(1);` | `ite_SetAnimation)(2);` | refused: MagmaBreathRecord_Land 2000 |
| M73 | `Sc()[9] = 0xE;` | `Sc()[9] = 0xF;` | refused: MagmaBreathRecord_Land 2000 |
| M74 | `Sc()[0xA] = 0xC;` | `Sc()[0xA] = 0xD;` | refused: MagmaBreathRecord_Land 2000 |
| M75 | `if (s[0xA] != 0) Dec(s[` | `Dec(s[` | refused: MagmaBreathRecord_Burn 491 |
| M76 | `Dec(Owner()[0xB]); Call0(` | `Call0(` | refused: MagmaBreathRecord_Burn 478 |
| M77 | `Call0(addr::MagicFx_FreeCurrentRecord); }` | `MH_CALL(BattleTask_FreeCurrent)(); }` | refused: MagmaBreathRecord_Burn 481 |
| M78 | `MH_CALL(Sprite_ScriptTickOnce)(); unsigned char* const s = Sc();` | `unsigned char* const s = Sc(); MH_CALL(Sprite_ScriptTickOnce)();` | refused: MagmaBreathRecord_Burn 65 |
| M79 | `Long(s + 0x34) >> 9) - 0x` | `Long(s + 0x34) >> 8) - 0x` | refused: MagicFx_PushRecordMatrix 2000 |
| M80 | `+ 0x38) >> 9) - 0x4000);` | `+ 0x38) >> 9) - 0x3FFF);` | refused: MagicFx_PushRecordMatrix 2000 |
| M81 | `int height = S16(Owner() + 0` | `int height = S16(Sc() + 0` | refused: MagicFx_PushRecordMatrix 1746 |
| M82 | `ast<int>(U(height) - U(height >> 31)) >> 1` | `ast<int>(U(height)) >> 1` | refused: MagicFx_PushRecordMatrix 494 |
| M83 | `tatic_cast<short>(-(` | `tatic_cast<short>((` | refused: MagicFx_PushRecordMatrix 2000 |
| M84 | `ngles[3] = {0, 0, 0};` | `ngles[3] = {0, 0, 1};` | refused: MagicFx_PushRecordMatrix 2000 |
| M85 | `ix0)(Camera_Matrix, m.m,` | `ix0)(Camera_Matrix + 1, m.m,` | refused: MagicFx_PushRecordMatrix 2000 |
| M86 | `MH_CALL(Gte_PushMatrix)(); const` | `const` | refused: MagicFx_PushRecordMatrix 2000 |
| M87 | `unsigned long*>(&m));` | `unsigned long*>(&m.m[2]));` | refused: MagicFx_PushRecordMatrix 2000 |
| M88 | `DrawMode(0x55);` | `DrawMode(0x56);` | refused: MagmaBreathRecord_DrawGlow 2000 |
| M89 | `_t>(Sc()[0xA]) << 4);` | `_t>(Sc()[0xA]) << 3);` | refused: MagmaBreathRecord_DrawGlow 1953 |
| M90 | `a = 0x100; a < 0x1100; a` | `a = 0x100; a < 0x1000; a` | refused: MagmaBreathRecord_DrawGlow 2000 |
| M91 | `p[6] = 0x60;` | `p[6] = 0x61;` | refused: MagmaBreathRecord_DrawGlow 2000 |
| M92 | `x16u, 0x24u, 0x25u, 0x26u}) p[k` | `x16u, 0x24u, 0x25u}) p[k` | refused: MagmaBreathRecord_DrawGlow 2000 |
| M93 | `signed>(Mul12(v, SD(0))))` | `signed>(Mul12(v, SS(0))))` | refused: MagmaBreathRecord_DrawGlow 1317 |
| M94 | `v = Sin(a); SetVW(0x10, static_cast<unsigned>(Mul12(v, SD(0))));` | `{ const std::int32_t r0 = SD(0); v = Sin(a); SetVW(0x10, static_cast<unsigned>(Mul12(v, r0))); }` | refused: MagmaBreathRecord_DrawGlow 968 |
| M95 | `v = Cos(0);` | `v = Cos(1);` | refused: MagmaBreathRecord_DrawGlow 2000 |
| M96 | `ool, kMagmaRecords); }` | `ool, kMagmaRecords - 1); }` | refused: MagmaBreath_PoolAlloc 246 |
| M97 | `signed k = 0; k < 5; ++k)` | `signed k = 0; k < 4; ++k)` | refused: MagicFx_FreeCurrentRecord 1994 |
| M98 | `for (unsigned k = 0; k <` | `for (unsigned k = 1; k <` | refused: MagicFx_FreeCurrentRecord 983 |
| G1 | `:GeoBreath_Start, kCountDownFlag10, addr::BattleFx_Finish}` | `:GeoBreath_Start, addr::BattleFx_Finish, kCountDownFlag10}` | refused: GeoBreath_Task 1308 |
| G2 | `oPool, kGeoRecords, addr` | `oPool, kGeoRecords - 1, addr` | refused: GeoBreath_Task 1016 |
| G3 | `oPool, kGeoRecords);` | `oPool, kGeoRecords - 1);` | refused: GeoBreath_Start 2000 |
| G4 | `0x34, Field_Kind2X); SetLong(s + 0x38, Field_Kind2Z);` | `0x34, Field_Kind2Z); SetLong(s + 0x38, Field_Kind2X);` | refused: GeoBreath_Start 2000 |
| G5 | `s[9] = 0x38;` | `s[9] = 0x39;` | refused: GeoBreath_Start 1978 |
| G6 | `NewTask(0x61);` | `NewTask(0x62);` | refused: GeoBreath_Start 2000 |
| G7 | `child[1] = 0;` | `child[1] = 1;` | refused: GeoBreath_Start 2000 |
| G8 | `Sound(0x100);` | `Sound(0x101);` | refused: GeoBreath_Start 2000 |
| G9 | `eRow26ThenRow2(0x10); }` | `eRow26ThenRow2(0x11); }` | refused: GeoBreath_Start 2000 |
| G10 | `const unsigned slot = NewTask(0x61); unsigned char* const self = Sc();` | `unsigned char* const self = Sc(); const unsigned slot = NewTask(0x61);` | refused: GeoBreath_Start 63 |
| G11 | `{addr::GeoBreathChild_Run}` | `{addr::GeoBreathRecord_Run}` | refused: GeoBreathChild_Task 2000 |
| G12 | `if ((s[0] & 1) != 0` | `if (s[0] != 0` | refused: GeoBreathChild_Run 440 |
| G13 | `addr::BuffRing_Wait,` | `addr::MagicFx_WaitA,` | refused: GeoBreathChild_Run 274 |
| G14 | `eoBreathChild_DrawGlow); Call0(addr::GeoBreathChild_DrawRing);` | `eoBreathChild_DrawRing); Call0(addr::GeoBreathChild_DrawGlow);` | refused: GeoBreathChild_Run 448 |
| G15 | `+ 0x1600000u` | `+ 0x1700000u` | refused: GeoBreathChild_Start 2000 |
| G16 | `s[0x5D] = 0x10;` | `s[0x5D] = 0x11;` | refused: GeoBreathChild_Start 2000 |
| G17 | `s[9] = 0xA;` | `s[9] = 0xB;` | refused: GeoBreathChild_Start 2000 |
| G18 | `s[0x28] = 1;` | `s[0x28] = 2;` | refused: GeoBreathChild_Start 2000 |
| G19 | `ite_SetAnimation)(0);` | `ite_SetAnimation)(1);` | refused: GeoBreathChild_Start 2000 |
| G20 | `char>(s[0xB] + 0xC);` | `char>(s[0xB] + 0xD);` | refused: GeoBreathChild_Swirl 2000 |
| G21 | `s[9] = 0x1E;` | `s[9] = 0x1F;` | refused: GeoBreathChild_Swirl 486 |
| G22 | `x1E; s[0xA] = 3;` | `x1E; s[0xA] = 4;` | refused: GeoBreathChild_Swirl 486 |
| G23 | `MH_CALL(Sprite_ScriptTickOnce)(); unsigned char* const s = Sc();` | `unsigned char* const s = Sc(); MH_CALL(Sprite_ScriptTickOnce)();` | refused: GeoBreathChild_Swirl 69 |
| G24 | `igned i = 0; i < 16; ++i)` | `igned i = 0; i < 15; ++i)` | refused: GeoBreathChild_Burst 417, GeoBreathChild_Quake 1392, GeoBreathChild_Settle 1417 |
| G25 | `std::int32_t>(Key(owner)));` | `std::int32_t>(Key(Sc())));` | refused: GeoBreathChild_Burst 398, GeoBreathChild_Quake 1313, GeoBreathChild_Settle 1337 |
| G26 | `>(i); Inc(owner[0xB])` | `>(i); Inc(Sc()[0xB])` | refused: GeoBreathChild_Burst 399, GeoBreathChild_Quake 1313, GeoBreathChild_Settle 1339 |
| G27 | `eoRecords() { for` | `eoRecords() { unsigned char* const owner0 = Owner(); for (and a second edit)` | refused: GeoBreathChild_Burst 182, GeoBreathChild_Quake 552, GeoBreathChild_Settle 611 |
| G28 | `t<unsigned char>(i);` | `t<unsigned char>(i + 1);` | refused: GeoBreathChild_Burst 417, GeoBreathChild_Quake 1392, GeoBreathChild_Settle 1417 |
| G29 | `((Frame_Counter & 1) != 0` | `((Frame_Counter & 2) != 0` | refused: GeoBreathChild_Shake 1000, GeoBreathChild_Quake 341, GeoBreathChild_Settle 322 |
| G30 | `(Owner() + 0x38)) - d));` | `(Owner() + 0x38)) + d)); (void)0;` | refused: GeoBreathChild_Shake 1065, GeoBreathChild_Quake 350, GeoBreathChild_Settle 339 |
| G31 | `d = U(Long(s + 0xC));` | `d = U(Long(s + 0x10));` | refused: GeoBreathChild_Shake 2000, GeoBreathChild_Quake 2000, GeoBreathChild_Settle 2000 |
| G32 | `((Frame_Counter & 7) != 0` | `((Frame_Counter & 3) != 0` | refused: GeoBreathChild_Quake 87, GeoBreathChild_Settle 79 |
| G33 | `Sound(0x102);` | `Sound(0x103);` | refused: GeoBreathChild_Quake 1392, GeoBreathChild_Settle 1417 |
| G34 | `if (s[0xA] == 0) {` | `if (s[0xA] <= 1) {` | refused: GeoBreathChild_Burst 115 |
| G35 | `pView_Redraw = 0x5D;` | `pView_Redraw = 0x5E;` | refused: GeoBreathChild_Burst 122 |
| G36 | `s[9] = 0x3C;` | `s[9] = 0x3D;` | refused: GeoBreathChild_Burst 122 |
| G37 | `Sc()[2] = 4;` | `Sc()[2] = 5;` | refused: GeoBreathChild_Burst 122 |
| G38 | `if (Sc()[0xA] == 2) Soun` | `if (Sc()[0xA] == 3) Soun` | refused: GeoBreathChild_Burst 68 |
| G39 | `g(Sc() + 0xC, 0x800);` | `g(Sc() + 0xC, 0x801);` | refused: GeoBreathChild_Burst 417 |
| G40 | `Sc()[9] = 4;` | `Sc()[9] = 5;` | refused: GeoBreathChild_Burst 417 |
| G41 | `SpawnGeoRecords(); if (Sc()[0xA] == 2) Sound(0x101);` | `if (Sc()[0xA] == 2) Sound(0x101); SpawnGeoRecords();` | refused: GeoBreathChild_Burst 125 |
| G42 | `s[9] = 8; Dec(Sc()` | `s[9] = 8; Inc(Sc()` | refused: GeoBreathChild_Shake 471 |
| G43 | `s[9] = 8;` | `s[9] = 9;` | refused: GeoBreathChild_Shake 471 |
| G44 | `t>((s[9] & 1u) << 4);` | `t>((s[9] & 1u) << 3);` | refused: GeoBreathChild_Quake 995 |
| G45 | `s[9] = 0x20;` | `s[9] = 0x21;` | refused: GeoBreathChild_Quake 351 |
| G46 | `BurstOnEighthFrame(); unsigned char* const s = Sc();` | `unsigned char* const s = Sc(); BurstOnEighthFrame();` | refused: GeoBreathChild_Quake 522 |
| G47 | `& 1) != 0 ? c >> 1 : 0` | `& 1) != 0 ? c >> 2 : 0` | refused: GeoBreathChild_Settle 591 |
| G48 | `Camera_ShiftX = 0; Sc()[9` | `Sc()[9` | **not refused** |
| G49 | `0; Sc()[9] = 8;` | `0; Sc()[9] = 9;` | refused: GeoBreathChild_Settle 385 |
| G50 | `d char>(s[0x5D] - 2);` | `d char>(s[0x5D] - 1);` | refused: GeoBreathChild_End 1765 |
| G51 | `if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFFu) == 0` | `if (MH_CALL(Sprite_ScriptTickOnce)() == 0` | **not refused** |
| G52 | `Dec(Owner()[0xB]); MH_CAL` | `MH_CAL` | refused: GeoBreathChild_End 1328 |
| G53 | `SetSW(0xA, SignedShl2(s[0x5D]));` | `SetSW(0xA, static_cast<unsigned>(s[0x5D]) << 2);` | refused: GeoBreathChild_DrawGlow 54 |
| G54 | `SetSW(0, s[0xB]);` | `SetSW(0, s[0xA]);` | refused: GeoBreathChild_DrawGlow 1952 |
| G55 | `igned>(s[0xB]) << 1);` | `igned>(s[0xB]) << 2);` | refused: GeoBreathChild_DrawRing 1943 |
| G56 | `, SignedShl2(s[0x5D]));` | `, SignedShl2(s[0x5E]));` | refused: GeoBreathChild_DrawRing 1649 |
| G57 | `ned char>(b))) << 2;` | `ned char>(b))) << 3;` | refused: GeoBreathChild_DrawGlow 1785, GeoBreathChild_DrawRing 1650 |
| G58 | `{addr::GeoBreathRecord_Run};` | `{addr::CombustionMote_Run};` | refused: GeoBreathRecord_Task 2000 |
| G59 | `if (s[0] == 0` | `if ((s[0] & 1) == 0` | refused: GeoBreathRecord_Run 335 |
| G60 | `:GeoBreathRecord_Fly, addr::GeoBreathRecord_Fade}` | `:GeoBreathRecord_Fade, addr::GeoBreathRecord_Fly}` | refused: GeoBreathRecord_Run 1343 |
| G61 | `ed>(Sc()[0xB]) << 8);` | `ed>(Sc()[0xB]) << 7);` | refused: GeoBreathRecord_Start 1991 |
| G62 | `0x34)) + Mul(v, 24)));` | `0x34)) + Mul(v, 25)));` | refused: GeoBreathRecord_Start 2000 |
| G63 | `v = Cos(SS(4));` | `v = Cos(static_cast<short>(angle));` | refused: GeoBreathRecord_Start 34 |
| G64 | `x3C, Owner() + 0x3C);` | `x3C, Owner() + 0x38);` | refused: GeoBreathRecord_Start 2000 |
| G65 | `s[9] = 8;` | `s[9] = 9;` | refused: GeoBreathRecord_Start 2000 |
| G66 | `unsigned char* const cell = s + 0x34; const int v = Sin(static_cast<short>(angle));` | `const int v = Sin(static_cast<short>(angle)); unsigned char* const cell = Sc() + 0x34;` | refused: GeoBreathRecord_Fly 55, GeoBreathRecord_Fade 58 |
| G67 | `unsigned char* const cell = Sc() + 0x38; const int v = Cos(angle);` | `const int v = Cos(angle); unsigned char* const cell = Sc() + 0x38;` | refused: GeoBreathRecord_Fly 42, GeoBreathRecord_Fade 58 |
| G68 | `nst short angle = SS(4);` | `nst short angle = static_cast<short>(static_cast<unsigned>(Sc()[0xB]) << 8);` | refused: GeoBreathRecord_Fly 113, GeoBreathRecord_Fade 121 |
| G69 | `MoveAlongAngle(3);` | `MoveAlongAngle(4);` | refused: GeoBreathRecord_Fly 2000 |
| G70 | `MoveAlongAngle(2);` | `MoveAlongAngle(3);` | refused: GeoBreathRecord_Fade 2000 |
| G71 | `ed char>(s[0xA] - 2);` | `ed char>(s[0xA] - 1);` | refused: GeoBreathRecord_Fade 2000 |
| G72 | `Call0(addr::MagicFx_FreeCurrentRecord); }` | `MH_CALL(BattleTask_FreeCurrent)(); }` | refused: GeoBreathRecord_Fade 437 |
| G73 | `; if (s[9] == 0) Inc(` | `; if (s[9] == 1) Inc(` | refused: GeoBreathRecord_Fly 960 |
| G74 | `DrawTexturedQuad(6); }` | `DrawTexturedQuad(7); }` | refused: GeoBreathRecord_Draw 1664 |
| G75 | `oPool, kGeoRecords); }` | `oPool, kGeoRecords - 1); }` | refused: GeoBreath_PoolAlloc 234 |
| G76 | `MH_CALL(MagicFx_CenterOnSide)(); { unsigned char* const s = Sc();` | `{ unsigned char* const s = Sc(); MH_CALL(MagicFx_CenterOnSide)();` | refused: GeoBreath_Start 65 |
| G77 | `Dec(Owner()[0xB]); Call0(` | `Call0(` | refused: GeoBreathRecord_Fade 434 |
| C1 | `{addr::Combustion_Start, addr::Combustion_Wait, add` | `{addr::Combustion_Wait, addr::Combustion_Start, add` | refused: Combustion_Task 1315 |
| C2 | `MH_CALL(BattleActor_UpdateScreenXY)(); Sc()[0` | `Sc()[0` | refused: Combustion_Start 2000 |
| C3 | `Sc()[9] = 0x10;` | `Sc()[9] = 0x11;` | refused: Combustion_Start 1893 |
| C4 | `(unsigned kind : {1u, 0u}) {` | `(unsigned kind : {0u, 1u}) {` | refused: Combustion_Start 2000 |
| C5 | `slot = NewTask(0x60);` | `slot = NewTask(0x61);` | refused: Combustion_Start 2000 |
| C6 | `child[9] = 0x10;` | `child[9] = 0x11;` | refused: Combustion_Start 2000 |
| C7 | `reRow26ThenRow2(0x10); }` | `reRow26ThenRow2(0x0F); }` | refused: Combustion_Start 2000 |
| C8 | `Sound(0x100);` | `Sound(0x101);` | refused: Combustion_Wait 474 |
| C9 | `, addr::CombustionGlow_Run, addr::CombustionMote_Run,` | `, addr::CombustionMote_Run, addr::CombustionGlow_Run,` | refused: CombustionChild_Task 1008 |
| C10 | `if ((s[0] & 1) != 0` | `if (s[0] != 0` | refused: CombustionSprite_Run 453 |
| C11 | `MH_CALL(BattleActor_UpdateScreenXY)(); MH_CALL(Sprite` | `MH_CALL(Sprite` | refused: CombustionSprite_Run 415 |
| C12 | `kCombustionStep4,` | `addr::MagicFx_WaitA,` | refused: CombustionSprite_Run 348 |
| C13 | `s + 0x14, 0x7800000);` | `s + 0x14, 0x7800001);` | refused: CombustionSprite_Start 493 |
| C14 | `(s + 0x20, 0x600000);` | `(s + 0x20, 0x600001);` | refused: CombustionSprite_Start 493 |
| C15 | `Long(Owner() + 0x3C)) + U` | `Long(Owner() + 0x38)) + U` | refused: CombustionSprite_Start 493 |
| C16 | `s[0x48] = 2;` | `s[0x48] = 3;` | refused: CombustionSprite_Start 493 |
| C17 | `s[0x2B] = 1;` | `s[0x2B] = 0;` | refused: CombustionSprite_Start 493 |
| C18 | `Sc()[9] = 0x10;` | `Sc()[9] = 0x11;` | refused: CombustionSprite_Start 493 |
| C19 | `U(Long(s + 0x14)) - U(Lon` | `U(Long(s + 0x14)) + U(Lon` | refused: CombustionSprite_Fall 2000 |
| C20 | `signed i = 0; i < 8; ++i)` | `signed i = 0; i < 7; ++i)` | refused: CombustionSprite_Fall 513 |
| C21 | `child[1] = 2;` | `child[1] = 3;` | refused: CombustionSprite_Fall 513 |
| C22 | `t<unsigned char>(i);` | `t<unsigned char>(i + 1);` | refused: CombustionSprite_Fall 513 |
| C23 | `std::int32_t>(Key(owner)));` | `std::int32_t>(Key(Sc())));` | refused: CombustionSprite_Fall 475 |
| C24 | `ra_Angles[0] + 0x14);` | `ra_Angles[0] + 0x15);` | refused: CombustionSprite_Fall 513 |
| C25 | `s[9] = 8;` | `s[9] = 9;` | refused: CombustionSprite_Fall 513 |
| C26 | `8; ++i) { const unsigned slot = NewTask(0x60); unsigned char* const owner = Owner();` | `8; ++i) { unsigned char* const owner = Owner(); const unsigned slot = NewTask(0x60);` | refused: CombustionSprite_Fall 117 |
| C27 | `!= 0 ? 0x14 : -0x14)` | `!= 0 ? 0x14 : -0x15)` | refused: CombustionSprite_Shake 991 |
| C28 | `_cast<short>(0xFD56);` | `_cast<short>(0xFD57);` | refused: CombustionSprite_Shake 494 |
| C29 | `Sc()[9] = 0x16;` | `Sc()[9] = 0x17;` | refused: CombustionSprite_Shake 494 |
| C30 | `((s[9] & 1) != 0` | `((s[9] & 2) != 0` | refused: CombustionSprite_Shake 1022 |
| C31 | `Sound(0x102);` | `Sound(0x103);` | refused: CombustionSprite_Flash 500 |
| C32 | `child[1] = 3;` | `child[1] = 2;` | refused: CombustionSprite_Flash 500 |
| C33 | `ed char>(s[0] / 0x20);` | `ed char>(s[0] / 0x10);` | refused: CombustionSprite_Flash 428 |
| C34 | `s[0x5C] = 1;` | `s[0x5C] = 2;` | refused: CombustionSprite_Flash 500 |
| C35 | `s[0x5F] = 0x40;` | `s[0x5F] = 0x41;` | refused: CombustionSprite_Flash 500 |
| C36 | `s[9] = 0xC;` | `s[9] = 0xD;` | refused: CombustionSprite_Flash 500 |
| C37 | `2); { const unsigned slot = NewTask(0x60); unsigned char* const owner = Owner();` | `2); { unsigned char* const owner = Owner(); const unsigned slot = NewTask(0x60);` | refused: CombustionSprite_Flash 17 |
| C38 | `char>(s[0x5D] + 0xF0);` | `char>(s[0x5D] + 0xEF);` | refused: CombustionSprite_Fade 2000 |
| C39 | `AddLong(s + 0x40, 0x80` | `AddLong(s + 0x44, 0x80` | refused: CombustionSprite_Fade 2000 |
| C40 | `if (s[0x5D] != 0x80) retu` | `if (s[0x5D] != 0x81) retu` | refused: CombustionSprite_Fade 904 |
| C41 | `s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);` | `` | refused: CombustionSprite_Fade 2000 |
| C42 | `ow, addr::MagicFx_WaitA,` | `ow, addr::MagicFx_CountUp9By2,` | refused: CombustionGlow_Run 507 |
| C43 | `if (s[0] == 0` | `if ((s[0] & 1) == 0` | refused: CombustionGlow_Run 406 |
| C44 | `ombustionGlow_DrawGlow); Call0(addr::CombustionGlow_DrawRing);` | `ombustionGlow_DrawRing); Call0(addr::CombustionGlow_DrawGlow);` | refused: CombustionGlow_Run 796 |
| C45 | `s[0xA] = 0x26;` | `s[0xA] = 0x27;` | refused: CombustionGlow_Start 487 |
| C46 | `SetLong(s + 0x14, 0);` | `SetLong(s + 0x14, 1);` | refused: CombustionGlow_Start 487 |
| C47 | `x3C, Owner() + 0x3C);` | `x3C, Owner() + 0x38);` | refused: CombustionGlow_Start 487 |
| C48 | `AddLong(s + 0x14, 6);` | `AddLong(s + 0x14, 7);` | refused: CombustionGlow_Grow 2000 |
| C49 | `if (s[9] == 0x10) Inc(` | `if (s[9] == 0x11) Inc(` | refused: CombustionGlow_Grow 510 |
| C50 | `SW(0, Word(s + 0x14));` | `SW(0, Word(s + 0x16));` | refused: CombustionGlow_DrawGlow 1951 |
| C51 | `nsigned>(s[9]) << 2);` | `nsigned>(s[9]) << 3);` | refused: CombustionGlow_DrawGlow 1820 |
| C52 | `ord(s + 0x14)) << 1);` | `ord(s + 0x14)) << 2);` | refused: CombustionGlow_DrawRing 1961 |
| C53 | `_cast<unsigned>(s[9]) <<` | `_cast<unsigned>(s[0xA]) <<` | refused: CombustionGlow_DrawRing 1673 |
| C54 | `r::CombustionMote_Grow, addr::CombustionMote_Shrink}` | `r::CombustionMote_Shrink, addr::CombustionMote_Grow}` | refused: CombustionMote_Run 1311 |
| C55 | `if (s[0] == 0` | `if ((s[0] & 1) == 0` | refused: CombustionMote_Run 341 |
| C56 | `nt32_t>((r & 7) + 8)` | `nt32_t>((r & 7) + 9)` | refused: CombustionMote_Start 1990 |
| C57 | `td::int32_t>((r & 7) + 8)` | `td::int32_t>((r & 0xF) + 8)` | refused: CombustionMote_Start 988 |
| C58 | `s[0xA] = 0x10;` | `s[0xA] = 0x11;` | refused: CombustionMote_Start 2000 |
| C59 | `ed>(Sc()[0xB]) << 9);` | `ed>(Sc()[0xB]) << 8);` | refused: CombustionMote_Start 1995, CombustionMote_Grow 1994, CombustionMote_Shrink 1992 |
| C60 | `Long(Owner() + 0x34))` | `Long(Owner() + 0x38))` | refused: CombustionMote_Start 2000, CombustionMote_Grow 2000, CombustionMote_Shrink 2000 |
| C61 | `int v = Sin(static_cast<short>(angle)); { unsigned char* const s = Sc(); SetLong(s + 0x34, static_cast<std::int32_t>(Mul(v, Long(s + 0xC))` | `const std::int32_t rr = Long(Sc() + 0xC); int v = Sin(static_cast<short>(angle)); { unsigned char* const s = Sc(); SetLong(s + 0x34, static_cast<std::int32_t>(Mul(v, rr)` | refused: CombustionMote_Start 67, CombustionMote_Grow 65, CombustionMote_Shrink 82 |
| C62 | `v = Cos(SS(4));` | `v = Cos(static_cast<short>(angle));` | refused: CombustionMote_Start 47, CombustionMote_Grow 40, CombustionMote_Shrink 48 |
| C63 | `dLong(Sc() + 0xC, 1);` | `dLong(Sc() + 0xC, 2);` | refused: CombustionMote_Grow 1994 |
| C64 | `AddLong(Sc() + 0xC, 1); MoteOr` | `MoteOr` | refused: CombustionMote_Shrink 1987 |
| C65 | `Dec(Owner()[0xB]); MH_CAL` | `MH_CAL` | refused: CombustionMote_Shrink 441 |
| C66 | `DrawTexturedQuad(8); }` | `DrawTexturedQuad(9); }` | refused: CombustionMote_Draw 1678 |
| C67 | `ustionFlash_Draw); Call0(addr::CombustionFlash_Draw);` | `ustionFlash_Draw);` | refused: CombustionFlash_Run 835 |
| C68 | `rt, addr::MagicFx_CountUp9By2,` | `rt, addr::MagicFx_WaitA,` | refused: CombustionFlash_Run 380 |
| C69 | `] == 0 // s[2] == 0) retu` | `] == 0 // s[2] == 1) retu` | refused: CombustionFlash_Run 437 |
| C70 | `Word(Owner() + 0x2E));` | `Word(Owner() + 0x30));` | refused: CombustionFlash_Start 2000 |
| C71 | `)(TargetByte(), 0x10);` | `)(TargetByte(), 0x40);` | refused: CombustionFlash_Start 2000 |
| C72 | `s[0xB] = 8;` | `s[0xB] = 9;` | refused: CombustionFlash_Start 2000 |
| C73 | `= 0; s[0xA] = 4;` | `= 0; s[0xA] = 5;` | refused: CombustionFlash_Start 2000 |
| C74 | `MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10); unsigned char* const s = Sc();` | `unsigned char* const s = Sc(); MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);` | refused: CombustionFlash_Start 62 |
| C75 | `s[0xA] = 0x10;` | `s[0xA] = 0x11;` | refused: CombustionFlash_Hold 506 |
| C76 | `ed char>(s[0xB] + 4);` | `ed char>(s[0xB] + 5);` | refused: CombustionFlash_Spin 2000 |
| C77 | `ed char>(s[0xB] + 4);` | `ed char>(s[0xB] + 3);` | refused: CombustionFlash_End 1997 |
| C78 | `Dec(Owner()[0xB]); MH_CAL` | `MH_CAL` | refused: CombustionFlash_End 524 |
| C79 | `const unsigned slot = NewTask(0x60); unsigned char* const self = Sc();` | `unsigned char* const self = Sc(); const unsigned slot = NewTask(0x60);` | refused: Combustion_Start 127 |
| C80 | `; if (Sc()[9] != 0) retu` | `; if (Sc()[9] > 1) retu` | refused: Combustion_Wait 541 |
| C81 | `); if (s[0xA] != 0) retu` | `); if (s[0xA] > 1) retu` | refused: CombustionFlash_Hold 524 |
| C82 | `; if (Sc()[9] != 0) retu` | `; if (Sc()[9] > 1) retu` | refused: CombustionSprite_Shake 519 |
| X1 | `L(Gfx_CommitPrim)(2, 0xC)` | `L(Gfx_CommitPrim)(5, 0xC)` | refused: CombustionFlash_Draw 2000 |
| X2 | `etSW(0xA, s[9] * 15u);` | `etSW(0xA, s[9] * 14u);` | refused: CombustionFlash_Draw 1901 |
| X3 | `nsigned>(s[9]) << 3);` | `nsigned>(s[9]) << 2);` | refused: CombustionFlash_Draw 1869 |
| X4 | `_cast<unsigned>(s[9]) <<` | `_cast<unsigned>(s[0xA]) <<` | refused: CombustionFlash_Draw 1866 |
| X5 | `ord(s + 0x2E) - 0x100u) & 0` | `ord(s + 0x2E) - 0xFFu) & 0` | refused: CombustionFlash_Draw 2000 |
| X6 | `y = S16(s + 0` | `y = Word(s + 0` | refused: CombustionFlash_Draw 1015 |
| X7 | `r (unsigned n = 0x20; n !=` | `r (unsigned n = 0x1F; n !=` | refused: CombustionFlash_Draw 2000 |
| X8 | `x0 = static_cast<short>(step_x);` | `x0 = static_cast<int>(step_x & 0xFFFF);` | refused: CombustionFlash_Draw 1006 |
| X9 | `int x1 = x0 + 0x10;` | `int x1 = x0 + 0x11;` | refused: CombustionFlash_Draw 2000 |
| X10 | `t int a1 = a + 0x40;` | `t int a1 = a + 0x41;` | refused: CombustionFlash_Draw 2000 |
| X11 | `= y + Mul12(v, SS(0));` | `= y + Mul12(v, SS(2));` | refused: CombustionFlash_Draw 2000 |
| X12 | `p[at + 1] = SB(0xC);` | `p[at + 1] = SB(0xE);` | refused: CombustionFlash_Draw 2000 |
| X13 | `p[at + 2] = SB(0xA);` | `p[at + 2] = SB(0xC);` | refused: CombustionFlash_Draw 2000 |
| X14 | `p[at + 2] = 1;` | `p[at + 2] = 2;` | refused: CombustionFlash_Draw 2000 |
| X15 | `= y + Mul12(v, SS(2));` | `= y + Mul12(v, SS(0));` | refused: CombustionFlash_Draw 2000 |
| X16 | `, 4); int v = Sin(a); const int ya = y + Mul12(v, SS(0));` | `, 4); const short r0 = SS(0); int v = Sin(a); const int ya = y + Mul12(v, r0);` | refused: CombustionFlash_Draw 972 |
| X17 | `- Mul12(v, SS(0));` | `- Mul12(v, SS(0)) + 1;` | refused: CombustionFlash_Draw 2000 |
| X18 | `const int y1 = y - Mul12` | `const int y1 = y + Mul12` | refused: CombustionFlash_Draw 2000 |
| X19 | `(0)); dark(p, 4)` | `(0)); top(p, 4)` | refused: CombustionFlash_Draw 2000 |
| X20 | `CommitPrim)(2, 0x44);` | `CommitPrim)(2, 0x40);` | refused: CombustionFlash_Draw 2000 |
| X21 | `step_x += 0x10;` | `step_x += 0x11;` | refused: CombustionFlash_Draw 2000 |
| X22 | `a = a1;` | `a = a1 + 1;` | refused: CombustionFlash_Draw 2000 |
| X23 | `PutFloat(fy, y);` | `PutFloat(fy, y + 1);` | refused: CombustionFlash_Draw 2000 |
| X24 | `memcpy(p + 0x2C, fy, 4);` | `memcpy(p + 0x2C, f0, 4);` | refused: CombustionFlash_Draw 2000 |
| X25 | `memcpy(p + 0x38, f1, 4);` | `memcpy(p + 0x38, f0, 4);` | refused: CombustionFlash_Draw 2000 |
| X27 | `ast<unsigned>(s[0xB]) <<` | `ast<unsigned>(s[0xA]) <<` | refused: CombustionFlash_Draw 1857 |
| X26 | `= y - Mul12(v, SS(0));` | `= y - Mul12(v, SS(2));` | refused: CombustionFlash_Draw 2000 |
| H4b | `SetLong(Mem(at::kOwner), rec_owner); Call0(` | `Call0(` | refused: MagmaBreath_Task 2000, GeoBreath_Task 2000 |
| G48b | `Camera_ShiftX = 0;` | `Camera_ShiftX = 1;` | refused: GeoBreathChild_Settle 385 |
| G51b | `tTickOnce)() & 0xFFu) ==` | `tTickOnce)() & 0xFEu) ==` | refused: GeoBreathChild_End 313 |

## 7. What nothing reached

- **The picture and the sound.** The fuzz proves ours equals Capcom's on the
  state it builds; what the three breaths look like is the owner's eye
  (DIV-0045's cheats put a skill in a list). No recorded route casts them.
- **Sequences.** Each step is fuzzed alone on random state; a whole cast
  (the task's phases over frames, the pools filling and emptying) runs only
  in the game.
- **The real callees.** The GTE, `Math_Sin` / `Math_Cos` / `Math_Ratan2` and
  the sprite calls answer from the recorders; their own groups proved them.
- **Unreachable inputs.** A table index past a table (ours aborts), a slot
  of 0xFF from `BattleTask_Create` (section 8), and the pools' record
  indices past their counts are not fuzzed.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D101, D110 and D111 in [`known-defects.md`](known-defects.md).

Described, not fixed; none numbered in `known-defects.md`.

- **Unbounded dispatch.** All fourteen tables are indexed by a phase byte
  unchecked (ours aborts past each, the precedent).
- **`BattleTask_Create`'s "none free" unchecked.** `GeoBreath_Start`,
  `Combustion_Start`, `CombustionSprite_Fall` (eight children) and
  `CombustionSprite_Flash` write through slot 0xFF when all 48 slots are
  taken - `0x93A000 + 0xFF x 0x84`, past the slots - as the original does.
  The two pool allocators' 0xFF is tested by every caller.
- **`CombustionSprite_Fade` scales one axis.** It adds 0x1000 and then 0x800
  to the dword +0x40 and never touches +0x44, which every start sets to the
  same 0x10000 - read as a copy slip for +0x44 (the sprite would stretch in
  one direction as it fades). Unmeasured on screen.
- **The camera left at a constant.** `CombustionSprite_Shake` ends by
  setting `Camera_Angles[0]` to 0xFD56 whatever it was before the fall's
  +0x14 (MAGIC053's Lavaburst does the same); a battle whose camera angle is
  not 0xFD56 would keep the new one.
- **`MagmaBreathRecord_Fly` passes an uninitialised stack word** as
  `MagicFx_StepTowardPoint`'s fourth argument; the callee never reads it.

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's list (2026-09-27): 54 lines, the extents
above to the last instruction. Six were listed right already (`004F6020`,
`004F60D0`, `004F69C0`, `004F6B60`, `004F6F70`, `004F7830`); seven were host
extents, re-listed smaller (`004F5BC0 45F`, `004F6230 789`, `004F6D90 1D3`,
`004F7190 1C0`, `004F79D0 431`, `004F7E10 345`, `004F8160 87F`).
