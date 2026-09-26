# Spell group S20: MAGIC087, MAGIC088, MAGIC092

**Status:** IN PROGRESS (2026-09-26) - 51 functions ours
(`src/game/magic_s20.cpp`, shadow name `magic_s20`), fuzzed headless
through the shared harness with 0 mismatches; 134 of 134 negative controls refused. No
recorded route casts any of the three: fuzz only until the owner's eye.

Round nine, first spell wave, group S20
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).
Three `Magic_Rows` overlays, taken whole through the harness
([`magic_harness.md`](magic_harness.md)). The functions are named by their
PSX file (`Magic087_`, `Magic088_`, `Magic092_`), not by a spell: the
ability names are the sibling's `magic.toml` **read one id down**, and
nothing read here confirms them. They are:

| Unit | Row | Ability ids | Read one id down (a hypothesis) |
|---|--:|---|---|
| MAGIC087 | 48 | `0x57`, `0xBD` | Silence |
| MAGIC088 | 29 | `0x1E` `0x1F` `0x58` `0x59` `0x5A` `0xBE` `0xBF` `0xC0` | Molasses, Tarbaby, Slow, Blunt, Weaken |
| MAGIC092 | 49 | `0x5C`, `0xC2` | Fireblast |

Whether any of these is a party spell or an enemy one is not measured
here. What the effects look like is described below from the code alone.

## 1. The extents

`tools/magic_rows.py --unit MAGIC087 / 088 / 092 --clones` (2026-09-25):
22 + 16 + 13 functions, none ours before, all taken. None was found
outside the extents and none is missing from them.

| Unit | Extent | Functions | Bytes |
|---|---|--:|--:|
| MAGIC087 | `0x4C3490..0x4C4486` | 22 | 3,897 |
| MAGIC088 | `0x4C4490..0x4C4FB1` | 16 | 2,769 |
| MAGIC092 | `0x4C5680..0x4C62F6` | 13 | 3,112 |

Shared bodies. 19 of MAGIC087's functions are reached from MAGIC083 and 086
as well (group S19's). `Magic088_WaveGrow` is reached from MAGIC039 and 040.
Seven of MAGIC092's are reached from MAGIC088 and 091, and two
(`Magic092_FlameGrow`, `Magic092_DrawFlame`) from nine files
(`analysis/magic_funcs.tsv`, `reached_by`). They lie in these extents, so
they are this group's; the harness keys on the address.

## 2. What they do

Every function is a battle task step: `void (void)`, with `Sprite_Current`
the slot and `0x93B940` its owner. The exceptions are `Magic087_DrawColumn(int)`
and the two allocators and `Magic088_Variant`, which answer al.
`symbols.toml` has each one's reading, instruction by instruction.

**Two private pools.** MAGIC087 keeps 80 "motes" of 0x84 bytes at
`0x6906D8` (`Magic087_Motes`). MAGIC092 keeps 48 at `0x693018`
(`Magic092_Motes`), directly after. Each overlay's kind-2 task first runs
its phase, then walks its pool. For every mote with `+0` bit 0, it sets
`Sprite_Current` to the mote and `0x93B940` to the mote's `+0x80`, runs the
mote's dispatcher, and puts both back. Each allocator answers the first
free mote's index, or `0xFF` when all are in use.

**MAGIC087 (row 48).** `Magic087_Task` has a two-entry stack table:
`_Start`, then `BattleFx_Finish` (`0x4F7350`, ours). `_Start` clears the
pool. For every actor on the target's side that is not out, it creates a
kind-1 child with parameter `0x27`. The side is the eight enemies when
`0x904B44` has bit `0x40`, else the three members. Each child sits at its
actor's position, raised by `0x800000`; an enemy's is raised again by a
byte at `0x8C564F + 0x8C * type`. `_Start` then sets CLUT strip row 26
semi-transparent, from `Gfx_ClutStripSource`. The child dispatches twice:
by type (`0x65B4E8`, one entry), then by phase (`0x65B4EC`, five phases):

- phase 0 spawns eight circling motes, four of each of the two orbit
  types;
- phase 1 waits for two of them;
- phase 2 spawns two rising motes every 8 frames, 16 times;
- phase 3 grows;
- phase 4 frees the child when the motes are gone (`+0xB` = `0x80`).

Phases 2 and 3 draw `Magic087_DrawColumn` twice (offsets 0 and 15). It
draws up to eight textured quads stacked up the actor's screen point,
swaying by a sine. The shade carries from each quad's top to the next
one's bottom.

A mote dispatches by type (`0x65B500`):

- **Circling motes** (types 0 and 1, `Magic087_OrbitRun`, four phases):
  after a delay they spin round the owner. The radius shrinks from `0x100`
  by 4 a frame, at an angle stepping 64. They wait for the owner's `0x82`,
  then fade.
- **Rising motes** (types 2 and 3, `_RiseRun`, two phases): they climb 8 a
  frame for 32 frames.

Each mote draws `Magic087_DrawTriangle`: a triangle outline in two
three-point gouraud lines, under the matrix `Magic087_PushMatrix` loads.
That is `MagicFx_PushActorMatrix`'s shape, with a turn by `+0xB` and a
lift of `0x100`.

**MAGIC088 (row 29).** `Magic088_Task` has a seven-entry stack table:

1. `_Start`: takes the variant from `Magic088_Variant`. It creates two
   kind-1 children (parameter `0x20`), copies CLUT strip rows 26 and 27
   from `0x4000` below, and plays sound `0x100`.
2. `_TintOn`: tints the source sprite (`0x904B4C`) black.
3. `_Darken`: steps the tint record down 8 frames.
4. `0x4EF7C0` (MAGIC167's): waits until the children are down to one.
5. `_Lighten`: steps the tint back up, then flashes the target.
6. `_Apply`: calls the effect library's stat change `0x4FB6F0` with the
   stat index `0x65C39C[variant]` and the target. It creates a kind-1
   child (parameter `0x48`) whose `+4` is `variant + 4` when the change
   answers non-zero, else 8.
7. `0x4E5200` (MAGIC131's end).

`Magic088_Variant` reads the ability id (word `0x904B80`). With the side
byte `0x904B35` at 4, a byte table in `.text` (`0x4C488C`, read in place)
maps ids `0x1E..0xC0` onto four answers:

- `0x1E`, `0x58` and `0xBE` answer 1;
- `0x1F`, `0x59` and `0xBF` answer 2;
- `0x5A` and `0xC0` answer 0;
- anything else answers 3.

On the other side, id `0x112` is rewritten to `0x5A` and answers 0.

The two children dispatch by type (`0x65B574`):

- **A fan** (`_FanRun`, three phases that are other overlays' bodies):
  sixteen gouraud triangles of radius `0xF0` under the actor's matrix.
- **A wave** (`_WaveRun`, four phases): grows, holds, fades.
  `_WaveStep` steps the six band phases at `0x65B588`. `_DrawWave` draws
  six bands of 64 flat quads round the actor. Each point is lifted by a
  sine of its band's phase plus its column. The colour comes from
  `0x65B52C` by variant and band.

**MAGIC092 (row 49).** `Magic092_Task` has a two-entry stack table:
`_Start`, then `0x4E5200`. After the phase it walks pool B. `_Start` does
what MAGIC087's does, with parameter `0x28` and delays `16 n + 1`, then
makes CLUT strip row 26 semi-transparent from `0x4000` below. The child
dispatches by type (`0x65B5C8`, one entry), then by phase (`0x65B5CC`,
four phases):

- phase 0 spawns three motes (a flame, then two sparks);
- phase 1 tints the actor at `+9` = 16;
- phase 2 is `0x4E93C0` (MAGIC144's), which waits for bit 7;
- phase 3 releases the tint, flashes the actor, sets its flag `0x40` and
  frees the child.

While the child lives it draws MAGIC144's `0x4E9420` under the actor's
matrix.

A mote dispatches by type (`0x65B5DC`):

- **The flame** (`_FlameRun`, whose phases are MAGIC144's bodies and
  `_FlameGrow`): `_DrawFlame` draws 28 rows of four textured gouraud quads
  up the screen point. The half-widths are sines of the row times `+9`,
  with `Rand() & 15` in the outer pair.
- **The spark** (`_SparkRun`): moves the screen point and tail-jumps to
  MAGIC144's `0x4E9850`.

## 3. Calls to other groups (raw addresses)

| Address | Owner | Called from |
|---|---|---|
| `0x4FB6F0` | LIBRARY (group L) | `Magic088_Apply` (`MH_AT`, two u8 arguments, al a flag) |
| `0x4E9420`, `0x4E9850` | MAGIC144 | `Magic092_ChildRun`, `Magic092_SparkRun` |
| `0x4EF7C0` | MAGIC167 | `Magic088_Task`'s stack table (a handler) |
| `0x4E5200` | MAGIC131 | the stack tables of `Magic088_Task` and `Magic092_Task` |
| `0x4C2D10`, `0x4EF840`, `0x4B1740` | MAGIC086, MAGIC167, MAGIC060 | `Magic088_FanPhases` (read in place) |
| `0x4E47F0` | MAGIC130 | `Magic088_WavePhases` |
| `0x4E93C0`, `0x4E9630`, `0x4E9690`, `0x4E96B0`, `0x4E9770`, `0x4E97D0`, `0x4E97F0` | MAGIC144 | MAGIC092's phase tables |
| `0x4E5950` | MAGIC131 | `Magic092_SparkPhases` |

`BattleFx_Finish` (`0x4F7350`) is ours already: `Magic087_Task`'s table
calls it by address through `Phase`. No other group's address is bound or
renamed here.

## 4. The fuzz

`BOF3X_SHADOW=magic_s20` (`magic_s20_fuzz.cpp`) runs 51 copies at 2,000
rounds each.

**Callees.** Beyond the harness's standard set, it lists:

- the PSX library layer the draws call (`Gpu_*`, `Gte_*`, `Math_Sin` /
  `Cos`, `MapView_LinkPrimAt`, `Gfx_CommitPrim`);
- `Battle_ActorIsOut` and `Sprite_SetTint` (five arguments logged);
- the group's own functions that call each other;
- the three raw addresses.

`deref` logs what the matrix calls read on the stack (`Gte_RotTrans`'s
vector, `Gte_RotMatrix`'s angles) and the vertices each projection reads
as it is called. Stack addresses themselves are masked off. Each commit
(`MapView_LinkPrimAt`, `Gfx_CommitPrim`) logs the packet buffer through an
`effect`: every primitive of a draw is built in the same bytes. `ret_mask 0xFF`
on the two allocators and `Magic088_Variant` compares their al.

**The thirteen `.data` tables** are swapped for recorders, each with the
entries its overlay reaches.

**Regions.** Beyond the harness's standard set:

- the two pools, and 256 slots' worth from pool B's start: an allocator's
  stand-in answers any byte, 0xFF included, and the unchecked callers write
  there;
- `Gfx_PacketNext` and the fuzz's own packet buffer, which it points at
  every round;
- 256 tint records;
- the scratch `0x903850..0x90385F` and `Prim_VertexScratch`;
- the CLUT source rows and strip rows 26 and 27;
- MAGIC088's band phases and the ability id;
- the lift bytes `0x8C564F` for enemy types 0..7 (zero at start-up, so
  seeded). That is 60,240 bytes in 20 regions.

**Seeds.** Each dispatcher's index stays inside its table. Each compare's
two sides are seeded:

- `+9` one step from its threshold;
- `+0xB` at 2, `0x80` or `0x82`;
- the radius reaching 0;
- the side bit `0x40`;
- the variant's ids and their neighbours, with and without side 4;
- `DrawColumn`'s `+4` round `0x10` and its phase 3;
- the pools full but for one mote, or full.

**Result (2026-09-26, this worktree, on the consolidated harness `ea27991`):**

    shadow      magic_s20 self-test: 102000 rounds over 51 functions (2000 each), 10663511 calls to the stand-ins,
                0 MISMATCHES; 60240 bytes of state (20 regions) and the stand-ins' log compared

Every stand-in and every handler was reached (the coverage lines). The
draws dominate: `Magic088_DrawWave` makes 4,262 calls a run.
`BOF3X_SHADOW='*'`: exit 0.

Before the harness was consolidated, this group added four things to it:

- a log of 8,192 entries, compared by prefix;
- `LogPointee`, now `Callee::deref`;
- `LogReturn`, now `Clone::ret_mask`;
- the Disturb 12 / 13 fix.

That last one was the crash that found it. A target of 2 made the
enemy-record disturbance write `0x93B940`, the owner pointer, and a side
bit indexed past the image. All four are in the consolidated harness
([`magic_harness.md`](magic_harness.md) §7).

## 5. The controls

134 negative controls, planted one at a time by a script (not committed: apply, `cmake --build build`, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s20`, restore), on the ported fuzz in this worktree. **134 of 134 are refused by a count (exit 3), each only in the function it touches.** The thinnest: AR3 (14), AP3 (22), F4 (50), AB3 (66), B7 (69).

| | Function | Planted | Refused in (rounds of 2,000) |
|---|---|---|--:|
| A1 | `Magic087_Task` | stack table entries swapped | 2,000 |
| A2 | `Magic087_Task` | mote live test bit 1 | 1,997 |
| A3 | `Magic087_Task` | Sprite_Current not put back | 1,981 |
| A4 | `Magic087_Task` | walks 79 motes | 1,013 |
| B1 | `Magic087_Start` | enemy lift byte one on | 979 |
| B2 | `Magic087_Start` | enemy +9 step 9 | 954 |
| B3 | `Magic087_Start` | member lift 0x80000 | 710 |
| B4 | `Magic087_Start` | enemy out test i + 2 | 1,023 |
| B5 | `Magic087_Start` | CLUT bit 14 not 15 | 2,000 |
| B6 | `Magic087_Start` | side bit 0x20 | 1,023 |
| B7 | `Magic087_Start` | Sprite_Current read before the create | 69 |
| B8 | `Magic087_Start` | child +4 the count after | 980 |
| C1 | `Magic087_Child` | type by +2 | 1,439 |
| C2 | `Magic087_ChildPhase` | phase by +1 | 1,614 |
| D1 | `Magic087_ChildSpawn` | type-0 +9 = 2n + 3 | 2,000 |
| D2 | `Magic087_ChildSpawn` | second four type 0 | 2,000 |
| D3 | `Magic087_ChildSpawn` | sound at +4 = 1 | 754 |
| E1 | `Magic087_ChildWait` | waits for 3 | 1,038 |
| F1 | `Magic087_ChildRing` | +9 back to 7 | 691 |
| F2 | `Magic087_ChildRing` | +0xB 0x81 | 617 |
| F3 | `Magic087_ChildRing` | second column offset 14 | 2,000 |
| F4 | `Magic087_ChildRing` | second mote owner the first read | 50 |
| G1 | `Magic087_ChildGrow` | threshold 0x11 | 1,009 |
| H1 | `Magic087_ChildEnd` | owner count down by 2 | 1,023 |
| I1 | `Magic087_DrawColumn` | n = +4 / 2 + 2 | 811 |
| I2 | `Magic087_DrawColumn` | phase-3 start +9 / 4 | 615 |
| I3 | `Magic087_DrawColumn` | shade threshold 0x150 | 1,227 |
| I4 | `Magic087_DrawColumn` | first angle + 2 | 1,602 |
| I5 | `Magic087_DrawColumn` | x - 3 | 1,602 |
| I6 | `Magic087_DrawColumn` | tpage x 0x380 | 1,602 |
| I7 | `Magic087_DrawColumn` | x + 8 from the y | 1,602 |
| I8 | `Magic087_DrawColumn` | v & 0x3F | 1,164 |
| I9 | `Magic087_DrawColumn` | shade not carried | 1,532 |
| I10 | `Magic087_DrawColumn` | Sprite_Current not read again after the second sine | 256 |
| J1 | `Magic087_MoteRun` | type by +2 | 975 |
| K1 | `Magic087_OrbitRun` | no triangle in phase 2 (not 3) | 504 |
| L1 | `Magic087_OrbitStart` | radius 0x101 | 1,017 |
| L2 | `Magic087_OrbitStart` | type-0 +9 0x1E | 678 |
| M1 | `Magic087_OrbitSpin` | x sar 4 | 998 |
| M2 | `Magic087_OrbitSpin` | angle & 0x7F | 994 |
| M3 | `Magic087_OrbitSpin` | cap 0x22 | 337 |
| M4 | `Magic087_OrbitSpin` | +4 test inverted | 926 |
| M5 | `Magic087_OrbitSpin` | z from the owner +0x34 | 2,000 |
| N1 | `Magic087_OrbitWait` | waits for 0x80 | 946 |
| O1 | `Magic087_OrbitFade` | owner count down by 2 | 987 |
| P1 | `Magic087_RiseRun` | no triangle in phase 1 | 972 |
| Q1 | `Magic087_RiseStart` | type test 3 | 1,342 |
| Q2 | `Magic087_RiseStart` | +0xA 0x1F | 2,000 |
| R1 | `Magic087_RiseStep` | rises 16 | 2,000 |
| S1 | `Magic087_PushMatrix` | turn << 5 (seen only by LogPointee) | 1,958 |
| S2 | `Magic087_PushMatrix` | lift 0x100 (seen only by LogPointee) | 2,000 |
| S3 | `Magic087_PushMatrix` | x sar 8 (seen only by LogPointee) | 2,000 |
| S4 | `Magic087_PushMatrix` | Camera_Matrix + 1 | 2,000 |
| T1 | `Magic087_DrawTriangle` | radius 0x81 | 1,011 |
| T2 | `Magic087_DrawTriangle` | corner 0x680 | 2,000 |
| T3 | `Magic087_DrawTriangle` | shade table one on | 1,870 |
| T4 | `Magic087_DrawTriangle` | p[0x16] 2 | 2,000 |
| T5 | `Magic087_DrawTriangle` | corner z 1 (the first triangle: seen by LogPointee) | 2,000 |
| U1 | `Magic087_PoolAlloc` | answers i + 1 (seen only by LogReturn) | 1,019 |
| U2 | `Magic087_PoolAlloc` | marks bit 1 | 1,019 |
| V1 | `Magic088_Task` | entries 2 and 3 swapped | 594 |
| W1 | `Magic088_Start` | +9 0x11 | 1,943 |
| W2 | `Magic088_Start` | second child +0xA 0x11 | 2,000 |
| W3 | `Magic088_Start` | row 27 from 2 bytes on | 2,000 |
| W4 | `Magic088_Start` | first child type 2 | 1,948 |
| W5 | `Magic088_Start` | variant + 1 | 1,972 |
| X1 | `Magic088_TintOn` | tint (0, 0, 1) | 980 |
| X2 | `Magic088_TintOn` | +0x5C 2 | 980 |
| X3 | `Magic088_TintOn` | tint's fifth argument 2 | 980 |
| Y1 | `Magic088_Darken` | record +4 not +3 | 2,000 |
| Z1 | `Magic088_Lighten` | tests +3 | 1,018 |
| Z2 | `Magic088_Lighten` | flashes the actor | 935 |
| AA1 | `Magic088_Apply` | child +4 = +4 + 5 | 661 |
| AA2 | `Magic088_Apply` | stat table one on | 816 |
| AA3 | `Magic088_Apply` | arguments swapped | 957 |
| AB1 | `Magic088_Variant` | id 0x113 | 164 |
| AB2 | `Magic088_Variant` | case 2 answers 2 (seen only by LogReturn) | 130 |
| AB3 | `Magic088_Variant` | bound 0xA1 | 66 |
| AB4 | `Magic088_Variant` | writes 0x5B | 104 |
| AC1 | `Magic088_Child` | type by +2 | 1,337 |
| AD1 | `Magic088_FanRun` | lives by +1 | 1,053 |
| AE1 | `Magic088_DrawFan` | radius 0xF1 | 2,000 |
| AE2 | `Magic088_DrawFan` | fifteen triangles | 2,000 |
| AE3 | `Magic088_DrawFan` | centre * 11 | 1,996 |
| AE4 | `Magic088_DrawFan` | closing tpage 0x16 | 2,000 |
| AE5 | `Magic088_DrawFan` | rim x from y (seen by LogPointee) | 2,000 |
| AF1 | `Magic088_WaveRun` | bands not stepped | 803 |
| AG1 | `Magic088_WaveGrow` | threshold 0x1F | 977 |
| AH1 | `Magic088_WaveHold` | owner count down by 2 | 1,028 |
| AI1 | `Magic088_WaveFade` | down by 1 | 2,000 |
| AJ1 | `Magic088_WaveStep` | odd and even swapped | 2,000 |
| AK1 | `Magic088_DrawWave` | colour row * 5 | 2,000 |
| AK2 | `Magic088_DrawWave` | z not negated | 1,992 |
| AK3 | `Magic088_DrawWave` | column phase + 2 | 2,000 |
| AK4 | `Magic088_DrawWave` | inner radius 16 b | 2,000 |
| AK5 | `Magic088_DrawWave` | link size 0x34 | 2,000 |
| AK6 | `Magic088_DrawWave` | green from 0x90385E | 2,000 |
| AK7 | `Magic088_DrawWave` | B4 from the old B0 (seen by LogPointee) | 2,000 |
| AL1 | `Magic092_Task` | walks 47 motes | 1,030 |
| AL2 | `Magic092_Task` | entries swapped | 2,000 |
| AM1 | `Magic092_Start` | +9 = 16 n + 2 | 1,645 |
| AM2 | `Magic092_Start` | +3 = i + 1 | 1,645 |
| AM3 | `Magic092_Start` | cell 0 = 1 | 2,000 |
| AM4 | `Magic092_Start` | member out test i + 3 | 975 |
| AN1 | `Magic092_Child` | type by +2 | 1,487 |
| AO1 | `Magic092_ChildRun` | no draw in phase 1 | 509 |
| AP1 | `Magic092_ChildSpawn` | +0 |= 0x40 | 866 |
| AP2 | `Magic092_ChildSpawn` | type n != 1 | 1,047 |
| AP3 | `Magic092_ChildSpawn` | full test 0xFE | 22 |
| AQ1 | `Magic092_ChildTint` | threshold 0x12 | 978 |
| AQ2 | `Magic092_ChildTint` | enemy flags index + 2 | 476 |
| AQ3 | `Magic092_ChildTint` | red -7 | 975 |
| AQ4 | `Magic092_ChildTint` | enemy record one on | 476 |
| AR1 | `Magic092_ChildEnd` | +9 down at 0 too | 1,021 |
| AR2 | `Magic092_ChildEnd` | flag40 index + 4 | 535 |
| AR3 | `Magic092_ChildEnd` | flash index from the first read | 14 |
| AS1 | `Magic092_MoteRun` | type by +2 | 1,369 |
| AT1 | `Magic092_FlameRun` | no screen point | 800 |
| AU1 | `Magic092_FlameGrow` | threshold 0x18 | 1,019 |
| AU2 | `Magic092_FlameGrow` | owner bit 6 | 765 |
| AV1 | `Magic092_DrawFlame` | Rand & 7 | 2,000 |
| AV2 | `Magic092_DrawFlame` | 27 rows | 2,000 |
| AV3 | `Magic092_DrawFlame` | counter from 5 | 2,000 |
| AV4 | `Magic092_DrawFlame` | inner shade 0x90 | 2,000 |
| AV5 | `Magic092_DrawFlame` | row + 0xD | 2,000 |
| AV6 | `Magic092_DrawFlame` | outer row << 2 | 2,000 |
| AV7 | `Magic092_DrawFlame` | clut y 0x1FB | 2,000 |
| AV8 | `Magic092_DrawFlame` | 0x90385C by 0x903850 | 2,000 |
| AV9 | `Magic092_DrawFlame` | rows +0xA + 1 apart | 2,000 |
| AV10 | `Magic092_DrawFlame` | outer right p[0x50] from 0x90385C | 2,000 |
| AW1 | `Magic092_SparkRun` | left 17 | 365 |
| AW2 | `Magic092_SparkRun` | up 11 | 771 |
| AW3 | `Magic092_SparkRun` | type 2 goes left | 365 |
| AX1 | `Magic092_PoolAlloc` | answers i + 1 (seen only by LogReturn) | 982 |

A first run (before the fuzz logged the packet at each commit and seeded the enemy lift bytes) left three unrefused, which is why those two were added. `B1` (the lift byte one on) read a table that is all zero at start-up, so both reads agreed; its eight rows by enemy type are now a seeded region. `AV4` and `AV5` (the flame's first quad) wrote the one packet buffer that the later quads overwrite before anything compared it; every `MapView_LinkPrimAt` and `Gfx_CommitPrim` now logs the packet as it stands.

Not planted, because no fuzz can see them: the order of writes between two calls when nothing reads them in between (ours keeps the original's order anyway), and the original's extra flag arguments to `Gte_RotTrans` / `Gte_RotTransPers3` / `4`, which the replacements do not declare.

## 6. What nothing reached

Everything here is fuzz only. No recorded route casts rows 29, 48 or 49
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §5).
The owner's live check is to cast the spells: with a save that has them,
or with DIV-0045's cheat.

What to look for, by the code:

- **MAGIC087:** a swaying column of quads over each actor on the target's
  side, and triangles circling in and rising.
- **MAGIC088:** the caster darkening and coming back, a fan and a wavy
  ring, and the stat change.
- **MAGIC092:** the actors tinted, flames rising from them, and sparks.

The fuzz cannot tell whether the picture is right. It can only tell that
ours computes what Capcom's does.

## 7. Defects (Capcom's, latent, kept)

These are described here and not numbered; the coordinator numbers them.

- **Unbounded dispatch.** None of the three stack tables and none of the
  thirteen `.data` tables is bounds-checked. Ours aborts past a stack table
  (the precedent) and reads the `.data` tables in place, as Capcom's does.
- **A full pool.** `Magic087_ChildSpawn` and `Magic087_ChildRing` do not
  test pool A's `0xFF`: with the pool full, their writes land 0xFF motes
  in, inside pool B. `Magic092_ChildSpawn` does test it.
- **The task slot index.** `BattleTask_Create`'s index is used untested in
  `Magic087_Start`, `Magic088_Start`, `Magic088_Apply` and
  `Magic092_Start`. `0xFF` writes past the 48 slots.
- **The tint index.** `Magic088_Darken` and `_Lighten` index the 32
  `MoveScript_TintRecords` by the answer `Sprite_SetTint` stored, unchecked.
  Its `0xFF` (no record free) writes 0xFF records in.
- **Actor indices.** `Magic092_ChildTint` and `_ChildEnd` take the actor's
  index `+3` unchecked. So does `Magic088_Apply` with the variant's
  `0x65C39C` index, which is always 0..3 by `Magic088_Variant`.
- **Side and id.** `Magic088_Variant` rewrites the ability id `0x112` to
  `0x5A` when the side byte is not 4. That is a mutation of battle state by
  a presentation effect. Whether it is meant is not known.

## 8. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy has a `group S20` comment. Four of the 51 were
listed right already. Seven were listed with a host's larger extent and
are re-listed smaller: `004C3CC0`, `004C4430`, `004C4830`, `004C4980`,
`004C4C00`, `004C5B10` and `004C62A0`. The other 40 are new. The
consolidation keeps the smaller of duplicates.
