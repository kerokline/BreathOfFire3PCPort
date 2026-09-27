# Group S32: Wall of Fire and Eye Beam (MAGIC144, MAGIC150)

**Status:** IN PROGRESS (2026-09-27). All 36 functions are ours
(`src/game/magic_s32.cpp`, shadow name `magic_s32`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 72,000 rounds. 155 of 159 negative controls refused, every one by a count (exit 3); the four standing are equivalent mutants, each with a near variant refused (section 6). Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S32
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Read one id down | Extent | Functions |
|--:|---|---|---|---|--:|
| 67 | 0x29A | MAGIC144 | Wall of Fire | `0x4E9140..0x4E9A66` | 20 |
| 72 | 0x29D | MAGIC150 | Eye Beam | `0x4EA450..0x4EAE61` | 16 |

The extents are `tools/magic_rows.py --unit MAGIC144 / MAGIC150 --clones`
(capstone recursive descent; no jump table, nothing `REFUSED`). All 36
functions lie in the units' extents; none was found inside or missing from
them, and none was ours before. 4,648 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What each spell looks
like in play has not been measured; the descriptions below are the code's.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Wall of Fire (MAGIC144)** is MAGIC092 (group S20's `Magic092_*`, read
  one id down Fireblast) with one child instead of one per actor. The two
  overlays share code both ways: MAGIC092's child and motes run this
  overlay's `WallOfFireChild_WaitFlame`, `WallOfFire_DrawDisc`, flame and
  spark steps and `WallOfFireSpark_Draw` by address, and this overlay's flame
  draws with `Magic092_DrawFlame` and grows with `Magic092_FlameGrow`.
  - `WallOfFire_Task`, the kind-2 task: a two-entry stack table by +1
    (`WallOfFire_Start`, `MagicFx_EndWhenChildrenDone`), then a walk of its
    own pool `WallOfFire_Pool` (48 records of 0x84 at `0x6A5680`): each live
    record run (`WallOfFireMote_Run`) as `Sprite_Current` with its +0x80 as
    the owner `0x93B940`, both put back afterwards.
  - `WallOfFire_Start` clears the pool, makes one child (kind 1, parameter
    0x36) at the source sprite's (`0x904B4C`) position, restores CLUT row 26's
    first sixteen words with the STP bit (word 0 cleared), +0xB 1.
  - The child (`WallOfFireChild_*`, four steps by +2 through
    `WallOfFireChild_Steps`): `_Spawn` takes three pool records (a flame, two
    sparks: +1 0 for the first, 1 after; +0xB their number) and plays sound
    0x100; `_Tint` counts +9 up by 2 to 0x10, then tints the source sprite
    (-8, -8, -8) and flags the target 0x40; `_WaitFlame` waits for bit 7 of
    +0xB (the flame sets it); `_End` waits for +0xB to be exactly 0x80 (every
    mote gone), then releases the tint, flashes the target, counts the task's
    +0xB down and frees itself. While it lives past step 0 it draws
    `WallOfFire_DrawDisc` under the actor's matrix: eight gouraud triangles
    of radius 0x180 round the origin, the centre shaded +9 x 8 (0x80 from
    +9 0x10 on), the rim 1.
  - The flame (`WallOfFireFlame_*` through `WallOfFireFlame_Steps`): `_Start`
    (the owner's position, +9 / +0xA 1), `_Rise` (+9 up by 4 to 0x11),
    `Magic092_FlameGrow` (+9 to 0x19, then the owner's +0xB bit 7), `_Fade`
    (+9 down by 2, +0xA up on odd frames; at 1 the owner's count down and the
    record freed); drawn by `Magic092_DrawFlame` at its screen point.
  - The sparks (`WallOfFireSpark_*` through `WallOfFireSpark_Steps`):
    `_Start`, `MagicFx_CountUp9By2`, `_Wait` (the owner's bit 7), `_End` (+9
    down; at 0 the count down and the record freed); drawn 16 pixels left
    (+0xB 1) or right of the screen point and 12 up by `WallOfFireSpark_Draw`:
    eight gouraud triangles in screen space, radius 0x40 + `Rand & 15`, the
    centre +9 x 10 red and a fifth of it green and blue, the rim 1.
  - `WallOfFire_PoolAlloc`: the first free record's index in al, 0xFF when
    none.
- **Eye Beam (MAGIC150)** works on a state block of its own at `0x6A9040`
  (`EyeBeam_State`, `EyeBeam_Sparks`, `EyeBeam_SparkCurrent`,
  `EyeBeam_SpiralPoints`; symbols.toml).
  - `EyeBeam_Task`: a seven-entry stack table by +1 - `_Start` (the beam's
    origin 0x14000 in x and 0x2C00000 in height from the caster, the current
    slot's `0x93B8C4` +0x80; the sparks cleared; +9 0x3C; sound effect
    0x100), `_Charge` (a spark a frame, the sparks drawn, for 0x3C frames),
    `_WaitSparks` (until none is live), `_Widen` (4 frames: the cylinder's
    radius +0x40, the spiral's +0x60; then sound effect 0x101), `_Fire` (0x78
    frames: the spiral turning -0x200 a frame, the cylinder 0x100 / 0x110 on
    even / odd frames; then the target flagged 0x40 and sound effect 0x102),
    `_Narrow` (4 frames back), `MagicFx_DoneAndFree`.
  - `EyeBeam_Draw`: a draw-mode packet, the map camera (Capcom's `0x494060`),
    `EyeBeam_DrawCylinder` (sixteen red flat quads round the x axis from the
    origin to 0x100000 along it, each corner a world point projected by
    Capcom's `0x494110`, committed only when `0x4941B0` - the sign of the
    cross product of three projected vertices - says it faces the camera)
    and `EyeBeam_DrawSpiral` (0x80 segments along x, the steps 0x1000,
    0x1040, ..., each point jittered in x by `Rand` and turned 0x80 on; each
    point projected three ways by `EyeBeam_ProjectSpiralPoint`, each segment
    two yellow-to-black gouraud quads and a line by
    `EyeBeam_DrawSpiralSegment`).
  - The sparks: `EyeBeamSparks_Clear`; `_Spawn` (the first free of eight:
    radius 0x300, a random angle, disc radius 8); `_Draw` (each live one at
    its angle and radius round the x axis, projected, drawn as a red disc of
    sixteen triangles by `EyeBeamSpark_DrawDisc`; its angle on 0x80, radius
    down 0x60, freed at 0; answers whether any was live). The walk goes
    through the pointer cell `EyeBeam_SparkCurrent`, read again after every
    call, as the original does.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one
exception that follows the project's precedent: a phase past any of the
seven dispatch tables (two stack tables, five `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3), where the original calls
through whatever follows the table.

Where the originals work on the x87, ours does too, in inline assembly:
`EyeBeamSpark_DrawDisc` adds each rim offset to the centre's float with
`fild` / `fadd dword` / `fstp` and copies the centre's depth with `fld` /
`fst` / `fst` / `fstp` (a signalling NaN comes out quiet, as in the
original). The integer-to-float vertex stores are exact (`fild` / `fstp` of
values far below 2^24).

## 3. Calls to other units

By raw address (Capcom's, unnamed, outside the band and in no group; never
bound or renamed here):

| Address | What it is | Reached from |
|---|---|---|
| `0x494060` | the map camera's rotation and translation (`Camera_Angles`, the map focus), no arguments | `EyeBeam_Draw`, `EyeBeam_DrawSpiral`, `EyeBeamSparks_Draw` |
| `0x494110` | a world point (x, z, height << 16) projected into a vertex: two floats and a depth (`Gte_RotTransPers`, `Gte_StoreDepthF`) | `EyeBeam_DrawCylinder`, `_ProjectSpiralPoint`, `EyeBeamSparks_Draw` |
| `0x4941B0` | the cross product of three projected vertices (floats), truncated by `_ftol` | `EyeBeam_DrawCylinder` (its low word's sign) |

`0x494060` and `0x494110` are group C2's too ([`magic_c2.md`](magic_c2.md)
§5), called the same way.

By name, already ours: `MagicFx_EndWhenChildrenDone`, `MagicFx_CountUp9By2`
(S30), `MagicFx_DoneAndFree` (E), `Magic092_FlameGrow`, `Magic092_DrawFlame`
(S20), `MagicFx_PushActorMatrix`, and the GTE / GPU / sprite / sound
library.

Shared bodies (`analysis/magic_funcs.tsv`): MAGIC092 reaches
`WallOfFireChild_WaitFlame`, `WallOfFire_DrawDisc`, the flame's and sparks'
steps and `WallOfFireSpark_Draw` (group S20 calls the two draws by address,
`0x4E9420` and `0x4E9850`, and holds the steps in its `.data` tables). They
reach them through the addresses, so taking them changes nothing for S20.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `WallOfFireChild_TaskTable` | `0x65BDC4` | 1 |
| `WallOfFireChild_Steps` | `0x65BDC8` | 4 |
| `WallOfFireMote_Kinds` | `0x65BDD8` | 2 |
| `WallOfFireFlame_Steps` | `0x65BDE0` | 4 |
| `WallOfFireSpark_Steps` | `0x65BDF0` | 4 |
| `WallOfFire_Pool` | `0x6A5680` | 48 x 0x84 |
| `EyeBeam_State` | `0x6A9040` | 0x18 bytes |
| `EyeBeam_Sparks` | `0x6A9058` | 8 x 8 |
| `EyeBeam_SparkCurrent` | `0x6A9098` | a pointer |
| `EyeBeam_SpiralPoints` | `0x6A90A0` | 2 x 0x34 |

Each `.data` count is where the next table starts (the words at
`0x65BDC0..0x65BE00` read 2026-09-27; MAGIC145's table follows at
`0x65BE00`). The pool ends where `InkInkPuff_Pool` (group C1) starts and
begins after `KaiserMote_Current` (group S30). Whether anything outside the
two overlays reads the pool or the state block was not measured (a raw scan
of `.text` for the dwords finds only unaligned byte runs outside the
extents, which is not an answer either way).

## 5. The fuzz

`BOF3X_SHADOW=magic_s32` runs `magic_harness::Run` over the 36 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s32_fuzz.cpp`:

- **Callees** (33 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes and moves `Gfx_PacketNext` on through a
    0x2000-byte buffer of the fuzz's own, kept 0x100 bytes from either end
    (`EyeBeam_DrawSpiralSegment` copies the 0x44 bytes before the pointer);
  - `Gte_RotTransPers3` logs its SVECTORs (`deref` 6 bytes each);
  - `0x494110` logs its vector (`deref` 12) and writes a vertex (12 bytes of
    the recorders' stream) where the real one writes - the callers' vertex
    is often a local of theirs, copied into the packet after the call;
    `0x4941B0` logs the three vertices it compares (`deref` 8 each) and
    answers garbage, so both sides of the cull are taken;
  - `BattleActor_UpdateScreenXY`, `BattleTask_FreeCurrent` and
    `Magic092_DrawFlame` log `Sprite_Current` and the owner (a walk that
    forgets to swap them shows);
  - this group's own functions called directly, by address: the mote
    dispatcher and the draws as `kPhase`, `WallOfFire_PoolAlloc` as a byte
    `0xFF..0x2F`, `EyeBeamSparks_Draw` as a flag, the pointer-taking ones
    with their arguments (`EyeBeamSpark_DrawDisc`'s centre by its 12 bytes,
    its radius as a word, shade and rim as bytes).
- **Answers:** `ret_mask 0xFF` on `WallOfFire_PoolAlloc` and
  `EyeBeamSparks_Draw`.
- **Tables:** the five `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; the 12-byte centre `EyeBeamSpark_DrawDisc` is handed;
  `Prim_VertexScratch`; `0x903850..0x90385F`; the pool; `0x6A9040..0x6A9107`;
  CLUT row 26's first sixteen words and their source. 26,216 bytes of state
  in 17 regions.
- **Seed:** every pool record's owner a real slot or record (the walk makes
  it the owner a recorder writes through); half the sparks dead, a quarter at
  their last radius (0x60); the spark cell at one of the eight; each
  dispatcher inside its table; each count one step before and at its
  threshold (Tint's 0x10, Rise's 0x11, Fade's 1, End's 0x80, the beam's
  steps' 0); `Frame_Counter`'s bit 0 flipped half the time for the fade;
  the pool full a quarter of the time for the allocator; all sparks live a
  third of the time for `_Spawn` (the search runs off the end), none live a
  third of the time for `_Draw` (its answer's other side); the disc's shade
  at 0xF, 0x10, 0, 0xFF or 7. `Group::args` hands the two spiral functions a
  spiral point and the disc its centre.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, any byte of a pool record below its owner, a word of the beam's
  state, a spark byte, a spiral point byte, and the spark cell - only back
  to the first spark: a cell moved forward walks `_Draw`'s eight steps onto
  the cell itself, which the loop then writes through (a fault on both
  sides, and no caller can reach it: nothing but the walk moves the cell).

Result in this worktree (2026-09-27):

    shadow      magic_s32 self-test: 72000 rounds over 36 functions (2000 each), 2313274 calls to the stand-ins,
                0 MISMATCHES; 26216 bytes of state (17 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`: the flame's and sparks' steps 462..529 times
each, `EyeBeamSpark_DrawDisc` 5,390, `0x494110` 139,390).
`BOF3X_SHADOW='*'`: exit 0 (2,312,423 stand-in calls for this group in that
run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

159 plants, each put in `magic_s32.cpp` one at a time by a script (not
committed) that planted, rebuilt, checked the build had recompiled the file,
ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s32`, restored; after the last
it restored, rebuilt and ran the clean self-test (0 mismatches, exit 0).
**155 refused**, each by exit 3 with a count only in the functions the plant
touches (the W controls are Wall of Fire, the E controls Eye Beam; W50, E24,
E25, E30 and E31 plant in a helper several functions share). Four stand, each
an equivalent mutant:

- **W36** (the disc's shade below 0x11 instead of 0x10): at +9 0x10 both
  branches give 0x80 (0x10 x 8). Its near variant **W36b** (below 0xF) was
  refused in 349 rounds.
- **E54** (0x40 of the 0x44 bytes copied): the last dword is the fourth
  vertex's depth, which the next store (12 bytes at +0x38) overwrites. Its
  near variant **E54b** (0x34 bytes: the fourth corner's colour left out) was
  refused in 1,999 rounds.
- **E73** (the cosine's angle read through the pointer cell rather than the
  walk's register): no call lies between the store of the cell and the read,
  so the two are the same spark. **E73b** (the radius word instead) was
  refused.
- **E84** (the rim sum in C float arithmetic instead of the x87 sequence):
  the sum of a small integer and a float rounds the same once to double and
  then to float as it does directly to float, for every input the fuzz drew.
  Ours keeps the x87 sequence regardless.

One control first stood and led to a fuzz change: **E47** (the spiral's first
point at the cylinder's radius) was invisible because
`EyeBeam_ProjectSpiralPoint`'s recorder logged only the point's address and
the loop overwrites the point's z and height before anything else reads them.
Its recorder now logs the point's 0x34 bytes, and
`EyeBeam_DrawSpiralSegment`'s the two points' 0x68 (`deref`); E47 was then
refused in 2,000 rounds. The other controls ran before that change; it only
adds log entries, so none of their refusals can have been lost.

The thinnest (fewer than 60 rounds):

- **E20** (Fire: the radius set after the draw): 1 - the draw is a
  recorder, so only a disturbance of the radius between the two tells
- **E64** (Spawn: the angle through the found spark, not the cell): 5
- **W56** (Spark_Run: left at +0xB 2): 6
- **E37** (Cylinder: radius read before the cosine): 16
- **E76** (Sparks_Draw: the spark not re-read before the height): 27
- **W72** (PoolAlloc: from record 1): 31

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| W1 | Task: entries swapped | WallOfFire_Task 2000 |
| W2 | Task: owner not put back | WallOfFire_Task 1614 |
| W3 | Task: owner read before the phase | WallOfFire_Task 69 |
| W4 | Task: bit 1 tested | WallOfFire_Task 2000 |
| W5 | Task: 47 records | WallOfFire_Task 981 |
| W6 | Start: parameter 0x37 | WallOfFire_Start 2000 |
| W7 | Start: source read after the create | WallOfFire_Start 48 |
| W8 | Start: +0xB 2 | WallOfFire_Start 2000 |
| W9 | Start: pool +2 kept | WallOfFire_Start 2000 |
| W10 | Start: CLUT from cell 2 | WallOfFire_Start 2000 |
| W11 | Start: CLUT bit 14 | WallOfFire_Start 2000 |
| W12 | Start: cell 0 kept | WallOfFire_Start 2000 |
| W13 | Start: child +1 1 | WallOfFire_Start 2000 |
| W14 | Start: child owned by itself | WallOfFire_Start 1956 |
| W15 | Child_Task: the steps table | WallOfFireChild_Task 2000 |
| W16 | Child_Run: the flame steps | WallOfFireChild_Run 2000 |
| W17 | Child_Run: no pop | WallOfFireChild_Run 1143 |
| W18 | Child_Run: +1 tested | WallOfFireChild_Run 634 |
| W19 | Spawn: +9 kept | WallOfFireChild_Spawn 1952 |
| W20 | Spawn: two motes | WallOfFireChild_Spawn 2000 |
| W21 | Spawn: flags 0x40 | WallOfFireChild_Spawn 1722 |
| W22 | Spawn: kinds by n != 1 | WallOfFireChild_Spawn 1996 |
| W23 | Spawn: +0xB n + 1 | WallOfFireChild_Spawn 2000 |
| W24 | Spawn: sound 0x101 | WallOfFireChild_Spawn 2000 |
| W25 | Spawn: task read before the alloc | WallOfFireChild_Spawn 197 |
| W26 | Tint: up by 3 | WallOfFireChild_Tint 2000 |
| W27 | Tint: red -7 | WallOfFireChild_Tint 500 |
| W28 | Tint: the actor flagged | WallOfFireChild_Tint 467 |
| W29 | Tint: no release | WallOfFireChild_Tint 500 |
| W30 | WaitFlame: bit 6 | WallOfFireChild_WaitFlame 1514 |
| W31 | End: +9 kept | WallOfFireChild_End 1012 |
| W32 | End: at 0x81 | WallOfFireChild_End 476 |
| W33 | End: the actor flashed | WallOfFireChild_End 216 |
| W34 | End: owner +0xB kept | WallOfFireChild_End 225 |
| W35 | Disc: radius 0x181 | WallOfFire_DrawDisc 1998 |
| W36 | Disc: shade below 0x11 | not refused |
| W37 | Disc: shade x 4 | WallOfFire_DrawDisc 733 |
| W38 | Disc: seven triangles | WallOfFire_DrawDisc 2000 |
| W39 | Disc: rim red 2 | WallOfFire_DrawDisc 2000 |
| W40 | Disc: vertex z 4 kept | WallOfFire_DrawDisc 2000 |
| W41 | Disc: last rim x from z | WallOfFire_DrawDisc 2000 |
| W42 | Disc: layer 4 | WallOfFire_DrawDisc 2000 |
| W43 | Disc: closing mode 0x55 | WallOfFire_DrawDisc 2000 |
| W44 | Disc: no depths | WallOfFire_DrawDisc 2000 |
| W45 | Mote_Run: the flame steps | WallOfFireMote_Run 2000 |
| W46 | Flame_Run: +0 not tested | WallOfFireFlame_Run 349 |
| W47 | Flame_Run: the spark steps | WallOfFireFlame_Run 2000 |
| W48 | Flame_Start: & 0x3F | WallOfFireFlame_Start 738 |
| W49 | Flame_Start: +0xA 2 | WallOfFireFlame_Start 2000 |
| W50 | Flame_Start: z from the height | WallOfFire_Start 2000, WallOfFireFlame_Start 2000, WallOfFireSpark_Start 2000 |
| W51 | Flame_Rise: up by 3 | WallOfFireFlame_Rise 2000 |
| W52 | Flame_Rise: at 0x12 | WallOfFireFlame_Rise 474 |
| W53 | Flame_Fade: frame bit 1 | WallOfFireFlame_Fade 996 |
| W54 | Flame_Fade: at 2 | WallOfFireFlame_Fade 488 |
| W55 | Flame_Fade: +4 kept | WallOfFireFlame_Fade 478 |
| W56 | Spark_Run: left at +0xB 2 | WallOfFireSpark_Run 6 |
| W57 | Spark_Run: up 11 | WallOfFireSpark_Run 1160 |
| W58 | Spark_Run: no screen point | WallOfFireSpark_Run 1167 |
| W59 | Spark_Start: +9 1 | WallOfFireSpark_Start 2000 |
| W60 | Spark_Wait: the task's +0xB | WallOfFireSpark_Wait 848 |
| W61 | Spark_End: owner kept | WallOfFireSpark_End 503 |
| W62 | Spark_Draw: Rand & 0x1F | WallOfFireSpark_Draw 1051 |
| W63 | Spark_Draw: shade x 9 | WallOfFireSpark_Draw 1982 |
| W64 | Spark_Draw: a quarter | WallOfFireSpark_Draw 1983 |
| W65 | Spark_Draw: first link 0x10 | WallOfFireSpark_Draw 2000 |
| W66 | Spark_Draw: first rim y by sin | WallOfFireSpark_Draw 2000 |
| W67 | Spark_Draw: step 0x100 | WallOfFireSpark_Draw 2000 |
| W68 | Spark_Draw: centre x from +0x30 | WallOfFireSpark_Draw 2000 |
| W69 | Spark_Draw: rim red 0 | WallOfFireSpark_Draw 2000 |
| W70 | PoolAlloc: | 3 | WallOfFire_PoolAlloc 791 |
| W71 | PoolAlloc: none 0xFE | WallOfFire_PoolAlloc 469 |
| W72 | PoolAlloc: from 1 | WallOfFire_PoolAlloc 31 |
| E1 | Task: Fire / Narrow swapped | EyeBeam_Task 539 |
| E2 | Start: x + 0x14001 | EyeBeam_Start 2000 |
| E3 | Start: height + 0x2C00001 | EyeBeam_Start 1997 |
| E4 | Start: z from +0x3C | EyeBeam_Start 2000 |
| E5 | Start: Sprite_Current's owner | EyeBeam_Start 617 |
| E6 | Start: +9 0x3D | EyeBeam_Start 1985 |
| E7 | Start: sound 0x101 | EyeBeam_Start 2000 |
| E8 | Start: angle kept | EyeBeam_Start 1998 |
| E9 | Charge: no spawn | EyeBeam_Charge 2000 |
| E10 | Charge: at 1 | EyeBeam_Charge 474 |
| E11 | WaitSparks: +9 5 | EyeBeam_WaitSparks 675 |
| E12 | WaitSparks: on while live | EyeBeam_WaitSparks 2000 |
| E13 | Widen: + 0x41 | EyeBeam_Widen 1994 |
| E14 | Widen: spiral + 0x50 | EyeBeam_Widen 1999 |
| E15 | Widen: +9 0x77 | EyeBeam_Widen 495 |
| E16 | Widen: sound 0x100 | EyeBeam_Widen 499 |
| E17 | Fire: turned + 0x200 | EyeBeam_Fire 1997 |
| E18 | Fire: frame bit 1 | EyeBeam_Fire 1499 |
| E19 | Fire: the actor flagged | EyeBeam_Fire 444 |
| E20 | Fire: radius after the draw | EyeBeam_Fire 1 |
| E21 | Fire: +9 5 | EyeBeam_Fire 490 |
| E22 | Narrow: - 0x41 | EyeBeam_Narrow 1999 |
| E23 | Narrow: no draw | EyeBeam_Narrow 2000 |
| E24 | Draw mode: layer 2 | EyeBeam_Draw 2000, EyeBeamSparks_Draw 2000 |
| E25 | Draw mode: x 0x2C1 | EyeBeam_Draw 2000, EyeBeamSparks_Draw 2000 |
| E26 | Draw: spiral first | EyeBeam_Draw 2000 |
| E27 | Draw: no camera | EyeBeam_Draw 2000 |
| E28 | Cylinder: 15 quads | EyeBeam_DrawCylinder 2000 |
| E29 | Cylinder: x + 0x100001 | EyeBeam_DrawCylinder 2000 |
| E30 | Ring: z sar 5 | EyeBeam_DrawCylinder 2000, EyeBeam_DrawSpiral 2000 |
| E31 | Ring: height shl 3 | EyeBeam_DrawCylinder 2000, EyeBeam_DrawSpiral 2000, EyeBeamSparks_Draw 1333 |
| E32 | Cylinder: step 0x80 | EyeBeam_DrawCylinder 2000 |
| E33 | Cylinder: sign of al | EyeBeam_DrawCylinder 2000 |
| E34 | Cylinder: facing of vertices 1..3 | EyeBeam_DrawCylinder 2000 |
| E35 | Cylinder: red 0x81 | EyeBeam_DrawCylinder 2000 |
| E36 | Cylinder: the spiral radius | EyeBeam_DrawCylinder 2000 |
| E37 | Cylinder: radius read before cos | EyeBeam_DrawCylinder 16 |
| E38 | Spiral: step 0x1001 | EyeBeam_DrawSpiral 2000 |
| E39 | Spiral: step + 0x41 | EyeBeam_DrawSpiral 2000 |
| E40 | Spiral: angle + 0x81 | EyeBeam_DrawSpiral 2000 |
| E41 | Spiral: jitter & 0x7FF | EyeBeam_DrawSpiral 1496 |
| E42 | Spiral: jitter shl 3 | EyeBeam_DrawSpiral 2000 |
| E43 | Spiral: x re-read | EyeBeam_DrawSpiral 2000 |
| E44 | Spiral: 0x7F segments | EyeBeam_DrawSpiral 2000 |
| E45 | Spiral: 12 dwords copied | EyeBeam_DrawSpiral 2000 |
| E46 | Spiral: the first angle whole | EyeBeam_DrawSpiral 2000 |
| E47 | Spiral: the cylinder radius | EyeBeam_DrawSpiral 2000 |
| E48 | Point: back 0x7000 | EyeBeam_ProjectSpiralPoint 2000 |
| E49 | Point: first into +0x10 | EyeBeam_ProjectSpiralPoint 2000 |
| E50 | Point: on 0x8000 | EyeBeam_ProjectSpiralPoint 2000 |
| E51 | Segment: red 0x41 | EyeBeam_DrawSpiralSegment 1991 |
| E52 | Segment: third corner black | EyeBeam_DrawSpiralSegment 1993 |
| E53 | Segment: second quad from next +0x1C | EyeBeam_DrawSpiralSegment 2000 |
| E54 | Segment: 0x40 bytes copied | not refused |
| E55 | Segment: line 0x21 | EyeBeam_DrawSpiralSegment 2000 |
| E56 | Segment: line to next +0x10 | EyeBeam_DrawSpiralSegment 2000 |
| E57 | Segment: second corner +0x28 | EyeBeam_DrawSpiralSegment 1992 |
| E58 | Segment: first commit 0x40 | EyeBeam_DrawSpiralSegment 2000 |
| E59 | Clear: seven | EyeBeamSparks_Clear 2000 |
| E60 | Clear: stride 4 | EyeBeamSparks_Clear 2000 |
| E61 | Spawn: radius 0x301 | EyeBeamSparks_Spawn 1345 |
| E62 | Spawn: angle shl 3 | EyeBeamSparks_Spawn 1341 |
| E63 | Spawn: disc 9 | EyeBeamSparks_Spawn 1345 |
| E64 | Spawn: angle through the found spark | EyeBeamSparks_Spawn 5 |
| E65 | Spawn: live 2 | EyeBeamSparks_Spawn 1345 |
| E66 | Spawn: cell left at the last | EyeBeamSparks_Spawn 655 |
| E67 | Sparks_Draw: shade 0x81 | EyeBeamSparks_Draw 1333 |
| E68 | Sparks_Draw: angle + 0x81 | EyeBeamSparks_Draw 1333 |
| E69 | Sparks_Draw: radius - 0x61 | EyeBeamSparks_Draw 1333 |
| E70 | Sparks_Draw: disc radius kept | EyeBeamSparks_Draw 1333 |
| E71 | Sparks_Draw: freed at 1 | EyeBeamSparks_Draw 824 |
| E72 | Sparks_Draw: answer 1 | EyeBeamSparks_Draw 667 |
| E73 | Sparks_Draw: cos of the cell's angle (equivalent) | not refused |
| E73b | Sparks_Draw: cos of the radius | EyeBeamSparks_Draw 1333 |
| E74 | Sparks_Draw: sin angle not re-read | EyeBeamSparks_Draw 33 |
| E75 | Sparks_Draw: z from x | EyeBeamSparks_Draw 1333 |
| E76 | Sparks_Draw: height re-read skipped | EyeBeamSparks_Draw 27 |
| E77 | Disc: sar 11 | EyeBeamSpark_DrawDisc 1349 |
| E78 | Disc: 15 triangles | EyeBeamSpark_DrawDisc 2000 |
| E79 | Disc: second rim x at a | EyeBeamSpark_DrawDisc 2000 |
| E80 | Disc: depth twice | EyeBeamSpark_DrawDisc 2000 |
| E81 | Disc: rim green | EyeBeamSpark_DrawDisc 1992 |
| E82 | Disc: radius a byte | EyeBeamSpark_DrawDisc 1774 |
| E83 | Disc: centre y from x | EyeBeamSpark_DrawDisc 2000 |
| E84 | Disc: sum in C float | not refused |
| W36b | Disc: shade below 0xF (near W36) | WallOfFire_DrawDisc 349 |
| E54b | Segment: 0x34 bytes copied (near E54) | EyeBeam_DrawSpiralSegment 1999 |

## 7. What nothing reached

No recorded route casts either spell (queue §5); the live check is the owner
casting them, with a save that has them or DIV-0045's cheat. Things to look
for, by reading:

- Wall of Fire: at the source actor, a glowing disc, a flame (MAGIC092's
  draw) and two spark bursts either side; the source darkening; the target
  flashing at the end.
- Eye Beam: sparks gathering in front of the caster, then a red ring-shaped
  beam along x with a yellow spiral round it that widens, flickers for two
  seconds (0x78 frames), and narrows.

Which party or enemy ability loads each row was not measured here.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the two stack tables and the
  five `.data` tables (an index past a table reads the next one). Ours
  aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `WallOfFire_Start`: slot 255 is past the image's end, an access violation
  in ours as in the original (the same address is written).
- **A freeze by reading, not reachable as far as read:** the child's
  `_WaitFlame` and both sparks wait for bit 7 of the child's +0xB, which only
  the flame sets. If the pool had no record for the flame (the first
  allocation failing), nothing would set it and the effect would never end.
  The pool is cleared by `WallOfFire_Start` and only this effect takes from
  it (three records), so this is not reached; MAGIC092's child has the same
  shape over its own pool.
- **`EyeBeamSparks_Spawn` with all eight sparks live** leaves the cell one
  past the last (on itself) and spawns nothing; `_Draw` resets the cell
  before it walks, so nothing follows. `_Charge` spawns one a frame and each
  lives 8 draws, so at most 8 are live when it spawns: the eighth slot is
  exactly enough.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 29 lines under a `group S32` comment: 25 new,
plus four host extents re-listed smaller (`004E95E0 12`, `004E9A10 57`,
`004EA6C0 1CA`, `004EAD40 122`, each the function's own size). Seven were
listed right already (`004E9420`, `004EA670`, `004EA9D0`, `004EAA10`,
`004EAB70`, `004EABA0`, `004EAC00`).
