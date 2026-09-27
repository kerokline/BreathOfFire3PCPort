# Group S26: MAGIC114, MAGIC115 and MAGIC117 (Fire Whip, Remedy, Rest / Snooze, Douse read one id down)

**Status:** IN PROGRESS (2026-09-26). All 48 functions are ours
(`src/game/magic_s26.cpp`, shadow name `magic_s26`), fuzzed headless through
the shared harness with 0 mismatches over 96,000 rounds; 156 of 158 negative controls refused by a count, the other two equivalent mutants (section 6).
Nothing recorded casts these spells, so this is fuzz only until the owner
sees them cast.

Round nine, second wave, group S26
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6a).
It takes three `Magic_Rows` overlays:

| Row | File | Overlay | Ability ids | Read one id down | Extent |
|---|---|---|---|---|---|
| 11 | 0x283 | MAGIC114 | 0x72 | Fire Whip | `0x4D7960..0x4D9437`, 28 functions, 6,654 bytes |
| 115 | 0x284 | MAGIC115 | 0x73, 0xD7 | Remedy | `0x4D9440..0x4D9ADA`, 13 functions, 1,606 bytes |
| 73 | 0x286 | MAGIC117 | 0x75, 0x98 | Rest, Snooze | `0x4D9F40..0x4DA214`, 7 functions, 668 bytes |
| 106 | 0x286 | MAGIC117 | 0x95 | Douse | (the same unit: its second entry `0x4DA0E0`) |

48 functions, 8,928 bytes, as the queue counted; none was ours, none was
found inside or missing from the extents (`tools/magic_rows.py --unit
MAGIC0NN --clones`, every jump internal, nothing `REFUSED`). The names are
the sibling's labels read one id down ([`cut-content.md`](cut-content.md) §2),
so each is a hypothesis, and the functions carry their overlay's number
instead. What they look like in play is the owner's to say; by reading:

- **MAGIC114**: a ring of eight gouraud triangles round the caster that grows,
  holds and shrinks; a volley of kind-1 children that each draw a trail from
  the caster to the target side's centre and leave a beam of three textured /
  gouraud strips and a sprite at the far end; rays of lines at the close.
- **MAGIC115**: the source sprite tinted, CLUT row 26 marked, and one child
  sprite circling the owner, dropping a mote sprite every eighth step.
- **MAGIC117**: two tint pulses on the source sprite (row 73: brighter and
  back, twice) or one (row 106: darker and back), then a flash on the target.

## 1. What each function does

**MAGIC114** (row 11). The kind-2 task and its phases:

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Magic114_Task` | `0x4D7960` | 0x9F | draw mode (tpage 0x35) to slot 3; while +0 and +1 are set, the ring; a seven-entry stack table by +1; the draw mode again |
| `Magic114_Start` | `0x4D7A00` | 0x95 | CLUT row 2 from its source with the STP bit, row 26 as it is; sound 0x100; the owner's direction and position; +0xA 0 |
| `Magic114_Grow` | `0x4D7AA0` | 0x2A | the ring's size +0xA up to 0x21 |
| `Magic114_Hold` | `0x4D7AD0` | 0x2A | +9 up to 0x21 |
| `Magic114_Launch` | `0x4D7B00` | 0x6D | +9 up; from 0x20: sound 0x101 and the child (kind 1, task 7, `+1` 0) |
| `Magic114_Rays` | `0x4D7B70` | 0x22 | +9 down; until 0, the rays (a tail jmp) |
| `Magic114_Shrink` | `0x4D7BA0` | 0x2B | +0xA down; at 0 +9 0x40 |
| `Magic114_End` | `0x4D7BD0` | 0x35 | +9 down; at 0 the target's flag 0x40, the done flag, free. Also in MAGIC054's and MAGIC113's stack tables |
| `Magic114_DrawRing` | `0x4D7C10` | 0xEC | under the owner's matrix, eight triangles from `Magic114_RingOffsets` scaled by +0xA and turned, sorted by their depths (`MagicFx_LinkByDepth`) |
| `Magic114_DrawTriangle` | `0x4D7D00` | 0x2FC | one triangle (gouraud), its depth into the caller's `keys[+0xB]`; colours by index; a flash on some frames of phase 2; one vertex faded by +9 in phases 3 and 4 |
| `Magic114_PushMatrix` | `0x4D8000` | 0xBA | the GTE matrix at the owner, turned about z by the ring's size |

The child (kind 1, parameter 7), three kinds by +1 through
`Magic114_ChildKinds`:

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Magic114_ChildTask` | `0x4D80C0` | 0x12 | `jmp [Magic114_ChildKinds + 4 * +1]` |
| `Magic114_TrailRun` | `0x4D80E0` | 0x12 | kind 0: `jmp [Magic114_TrailPhases + 4 * +2]` |
| `Magic114_TrailAim` | `0x4D8100` | 0xA0 | at the target side's centre (`MagicFx_CenterOnSide`), its height kept in +0x14; the owner's position moved by an offset |
| `Magic114_TrailSpawn` | `0x4D81A0` | 0x10D | the trail drawn; a sprite child (`+1` 2) at its end each frame; on the first frame the beam child (`+1` 1); free after 13 frames |
| `Magic114_DrawTrail` | `0x4D82B0` | 0x271 | one gouraud quad, dark red to orange, from the owner's side to the far end |
| `Magic114_BeamRun` | `0x4D8530` | 0x40 | kind 1: `call [Magic114_BeamPhases + 4 * +2]`; while +0 and +2 are set, the glow, the strip (mode +0xB) and the core |
| `Magic114_BeamWait` | `0x4D8570` | 0x81 | +9 down; at 0 sound 0x102, the beam at its end, the length +0x14 and the scroll +0x20 0 |
| `Magic114_BeamGrow` | `0x4D8600` | 0x52 | the scroll down 5, the length up 2; at 0x30 the target's flag 0x10; at 0xC0 +0xB 1 (additive) |
| `Magic114_BeamFade` | `0x4D8660` | 0x3B | the scroll and length on, +9 down; at 0 free |
| `Magic114_DrawBeamCore` | `0x4D86A0` | 0x3B7 | up to 55 textured quads (page (0x340, 0x100), CLUT (0, 0x1E2)) along the beam, v scrolling, grey pulsing with the frame |
| `Magic114_DrawBeamStrip` | `0x4D8A60` | 0x372 | the same with semi-transparency mode abr (an argument) and u stepping |
| `Magic114_DrawBeamGlow` | `0x4D8DE0` | 0x2E3 | gouraud quads, grey fading to black at the outer edge |
| `Magic114_SpriteRun` | `0x4D90D0` | 0x27 | kind 2, with the effects' frame-offset table `0x9039D8 = 0x8E3580`: `call [Magic114_SpritePhases + 4 * +2]` |
| `Magic114_SpriteStart` | `0x4D9100` | 0x7F | the sprite set up (slot 0x1D, CLUT row 0x1A), animation 0 |
| `Magic114_SpriteTick` | `0x4D9180` | 0x13 | the script ticked; at its end free |
| `Magic114_DrawRays` | `0x4D91A0` | 0x178 | with +9 above 0x18: 64 lines round a circle shrinking with +9 |
| `Magic114_PushRayMatrix` | `0x4D9320` | 0x118 | the task put at the owner moved by (0xC000, 0) turned; the matrix there, turned 0x400 about x or y |

The beam's steps: step i (from 1) runs while i is below the length +0x14 and
at most 55 times (the depth -0x8000 x i above -0x1C0000); each quad joins the
last pair of points to the next, both turned from (width, depth); the width
narrows over the last eight steps (`(i - length) x 3 << 13`, x 5 for the
glow's outer edge) and the core's inner width widens over the first 0x20.

**MAGIC115** (row 115):

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Magic115_Task` | `0x4D9440` | 0x36 | a four-entry stack table by +1: start, tint up, tint down, `BattleFx_Finish` |
| `Magic115_Start` | `0x4D9480` | 0xDB | at the source sprite (read once); the child (kind 1, task 0x30); the source's tint (0, 0, 0, 1) kept in +0xA; CLUT row 26's first sixteen marked STP, the first then restored; sound 0x100; +0xB 1 |
| `Magic115_TintUp` | `0x4D9560` | 0x4D | the tint record's colour = +9 x 2 (the motes count +9 up); from 6 on |
| `Magic115_TintDown` | `0x4D95B0` | 0x78 | on odd frames the colour down one; at red 0 `Tint_Release` |
| `Magic115_ChildTask` | `0x4D9630` | 0x12 | `jmp [Magic115_ChildKinds + 4 * +1]` |
| `Magic115_OrbitRun` | `0x4D9650` | 0x3D | kind 0, with the effects' frame-offset table: `call [Magic115_OrbitPhases + 4 * +2]`; the screen point |
| `Magic115_OrbitStart` | `0x4D9690` | 0xF1 | at the owner, raised 0x2C0; the sprite set up (slot 0x1D, CLUT row 0xA0, scale 1); animation 0 |
| `Magic115_Orbit` | `0x4D9790` | 0x148 | the script ticked; +9 up; the position the owner's plus (3 sin, 3 cos) << 2 of (+9 & 0x3F) << 6; every eighth step a mote (kind 1, task 0x30, `+1` 1) counted on the owner's +9 and +0xB; at 0x40 the colour set and on |
| `Magic115_OrbitFade` | `0x4D98E0` | 0xA9 | the colour down 4, the step, red 0x80 |
| `Magic115_MoteRun` | `0x4D9990` | 0x3D | kind 1: as the orbit's through `Magic115_MotePhases` |
| `Magic115_MoteStart` | `0x4D99D0` | 0xAF | the sprite set up, animation 1 |
| `Magic115_MoteTick` | `0x4D9A80` | 0x18 | the script ticked twice; at its end past the fade |
| `Magic115_MoteFade` | `0x4D9AA0` | 0x3B | the colour down 4, one tick, red 0x80 |

Both children end through `0x4AF490` (MAGIC058's: the owner's +0xB down,
free), which each table's fourth entry holds; `BattleFx_Finish` waits for
the parent's +0xB (1 for the orbit plus one a mote) to reach 0.

**MAGIC117** (rows 73 and 106):

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Magic117_Task` | `0x4D9F40` | 0x36 | row 73: tint set, brighten, dim, and the engine's `0x43F460` (the target's flag 0x40, the done flag, free) |
| `Magic117_TintSet` | `0x4D9F80` | 0x47 | the source's tints released, a tint (0, 0, 0, 1) kept in +0xB; two pulses (+0xA 2) |
| `Magic117_Brighten` | `0x4D9FD0` | 0x64 | the tint record up one a frame for 16 frames. Also MAGIC010's |
| `Magic117_Dim` | `0x4DA040` | 0x9C | down one a frame for 16; a pulse left: back to brighten; none: released, the target flashed |
| `Magic117_TaskB` | `0x4DA0E0` | 0x36 | row 106: MAGIC010's tint set `0x49DF30`, darken, lighten, `0x43F460` |
| `Magic117_Darken` | `0x4DA120` | 0x64 | the tint record down one a frame for 16 |
| `Magic117_Lighten` | `0x4DA190` | 0x85 | up one a frame for 16; released, the target flashed |

## 2. Divergence

No ledger entry. Each function is a faithful replacement, except that a
phase past any of the eleven dispatch tables (four stack tables, seven
`.data` tables) aborts, the project's
precedent ([`magic_fx_reached.md`](magic_fx_reached.md) §3). `DIVERGENCE.md`
and `cheats.cpp` patch nothing in `0x4D7960..0x4DA214`.

Where the originals push more than the callee's prototype takes, ours pushes
it too: `Gte_RotTrans` gets the flag pointer, the projections
(`Gte_RotAverage3` / `4`, `Gte_RotTransPers`) the depth and flag pointers.

## 3. Calls to other units (by raw address, not bound)

| Address | Owner | Reached as |
|---|---|---|
| `0x446770` | engine, unnamed, in no group | turns a task's dx / dz pair +0xC / +0x10 by its direction +8 (MAGIC114, eleven sites) |
| `0x4AF490` | MAGIC058 (S11) | entry 3 of `Magic115_OrbitPhases` and `Magic115_MotePhases`: the owner's +0xB down, free |
| `0x49DF30` | MAGIC010 (C1) | entry 0 of `Magic117_TaskB`'s stack table: the tint set |
| `0x43F460` | engine row 123 (E) | entry 3 of both MAGIC117 stack tables: the target's flag 0x40, the done flag, free |

Ours calls them through `MH_AT` / `Phase` by address. The library
(`MagicFx_LinkByDepth`, `MagicFx_CenterOnSide`, `BattleActor_Flash`),
`BattleFx_Finish` and the GTE / GPU layer are called by name.

## 4. Named data (`symbols.toml` `[[data]]`)

| Kind | Tables |
|---|---|
| MAGIC114's offsets | `Magic114_RingOffsets` `0x65BA40` (ten dwords, read in place) |
| Handler tables | `Magic114_ChildKinds` `0x65BA68` (3), `Magic114_TrailPhases` `0x65BA74` (2), `Magic114_BeamPhases` `0x65BA7C` (3), `Magic114_SpritePhases` `0x65BA88` (2), `Magic115_ChildKinds` `0x65BA90` (2), `Magic115_OrbitPhases` `0x65BA98` (4), `Magic115_MotePhases` `0x65BAA8` (4) |

Only addresses and sizes are recorded here, not values. The handler tables
are back to back: each unchecked index past its table reads the next's.

## 5. The fuzz

`BOF3X_SHADOW=magic_s26` runs `magic_harness::Run` over all 48 clones, 2,000
rounds each, on the consolidated harness without edits (every need below is
built in `magic_s26_fuzz.cpp`).

**The clone table** is `magic_rows.py`'s but for one number:
`Magic114_DrawTriangle`'s first jump table has **8** entries (`cmp ecx, 7;
ja`), where the tool printed 14 - it ran on into the second table
(`0x4D7FE4`), and the harness's move relocated those six twice and stopped
(`move_script: jump table +0x2E4 entry 0 is 0x29001E8`). A tool fix for the
coordinator: a jump table's count should stop at the bounds check or at the
next table's start.

**Callees.** The group lists 47 (registered before the standard set):

- **stand-ins of its own** (`custom`):
  - `Gfx_CommitPrim` and `MapView_LinkPrimAt` log their arguments and the
    primitive's bytes at `Gfx_PacketNext` (up to 16 as they are, more as a
    hash), then move `Gfx_PacketNext` on by the size, kept inside the
    fuzz's buffer - so every primitive of a loop is compared, not the last;
  - `0x446770` logs the task, its direction and dx / dz, then turns it for
    real (Capcom's code): a dx read before the turn where the original reads
    it after shows;
  - `Magic114_DrawTriangle` (the ring's call, with a pointer into the
    ring's frame) logs the task's +0xB / +0xA / +1 and fills all eight keys,
    which `MagicFx_LinkByDepth`, listed again with `deref` 32 on the keys,
    logs by their bytes;
  - `Sprite_UpdateScreen` and the nine sprite phases the runners call
    through `.data` log the frame-offset table pointer `0x9039D8`;
- the GTE: vectors by their bytes (`deref` 6), the frame pointers not at all;
  the matrix pushes' `Gte_RotTrans` / `_RotMatrix` / `_MulMatrix0` write a
  result where the real ones write (group S22's effects) and
  `Gte_SetTransMatrix` logs the translation;
- the GPU setters, `Math_Sin` / `Math_Cos`, the sprite and tint callees;
- this group's draws and pushes called directly by address, as `kPhase`
  (logging the task and its phase bytes).

**Tables.** Seven `.data` tables, swapped for recorders while the fuzz runs.

**Regions** beyond the harness's: `Gfx_PacketNext` and a 1 KB packet
buffer; 256 keys for `Magic114_DrawTriangle` fuzzed alone (`Group::args`
hands it the pointer); `Prim_VertexScratch`; `DamageScratch`'s sixteen
bytes; `0x9039D8`; `Field_Kind2X` / `Z`; `MoveScript_TintRecords` whole (an
unchecked byte index of 12-byte records); CLUT rows 2 and 26 and their
sources; `Magic114_RingOffsets`.

**The seed**: every dispatcher's index inside its table (+0 cleared half
the time where it gates a draw); each count one step either side of its end;
the beam's length small (-4..65, so the 55-step bound is reached too); the
triangle's index 0..9 (past the colour table) in phases 2..4 with
`Frame_Counter & 0x18` clear half the time; the rays' +9 round 0x18; the
orbit's +9 at 0x3F or one before a multiple of 8; the tint record's red 0..2
for the tint-down. **Disturb** (the harness's case 14): `Gfx_PacketNext`, a
vertex word, a scratch word, `0x9039D8`, the task's dwords +0xC / +0x10 /
+0x14 / +0x20 and its sprite bytes, `Field_Kind2X` / `Z`, a tint record's
colour byte, the owner's height and +9. `phase_span` 2 keeps a phase the
disturbance moves inside the smallest table (`Magic114_Task` reads +1 again
after the ring).

**Result** in this worktree (2026-09-26):

    shadow      magic_s26 self-test: 96000 rounds over 48 functions (2000 each), 1709421 calls to the stand-ins,
                0 MISMATCHES; 18632 bytes of state (21 regions) and the stand-ins' log compared

Coverage (counted on the run before the final seed; the final run's move by a few hundred): every callee and handler the clones name is reached - e.g.
`0x446770` 170,296, `MapView_LinkPrimAt` 165,536, `Gte_RotAverage4` 59,148,
`MagicFx_LinkByDepth` 2,000, `Tint_Release` 448, `Battle_SetTargetFlags` 214,
each sprite phase 479..1,013. `BOF3X_SHADOW='*'`: exit 0.

## 6. Controls

158 plants, each put in `magic_s26.cpp` one at a time by a script (the
scratch `controls.py`, not committed: replace an anchor that must be unique,
rebuild, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s26`, restore, rebuild),
2026-09-26, on the final fuzz. **156 of 158 refused** by a count
(exit 3), each in the function or functions its plant touches; the two
others are equivalent mutants, each with a near variant planted and refused.
Counts are this worktree's.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Task: first draw mode tpage 0x36 | Magic114_Task 2000 |
| T2 | Task: the ring without the +0 test | Magic114_Task 807 |
| T3 | Task: the phase not read again after the ring | Magic114_Task 25 |
| T4 | Task: table entries 1 and 2 swapped | Magic114_Task 658 |
| T5 | Task: the second commit to slot 4 | Magic114_Task 2000 |
| S1 | Start: row 2 with bit 0x4000 | Magic114_Start 2000 |
| S2 | Start: row 26 from the next word | Magic114_Start 2000 |
| S3 | Start: sound 0x101 | Magic114_Start 2000 |
| S4 | Start: +0x3C not copied | Magic114_Start 1763 |
| S5 | Start: Gfx_ClutStripDirty 2 | Magic114_Start 2000 |
| G1 | Grow: past 0x21 | Magic114_Grow 478 |
| G2 | Hold: at 0x20 | Magic114_Hold 431 |
| L1 | Launch: from 0x21 | Magic114_Launch 445 |
| L2 | Launch: task 8 | Magic114_Launch 1472 |
| L3 | Launch: the owner read before the create | Magic114_Launch 44 |
| L4 | Launch: the child +8 the task's | Magic114_Launch 1289 |
| R1 | Rays: drawn at 0 too | Magic114_Rays 459 |
| SH1 | Shrink: +9 0x41 | Magic114_Shrink 450 |
| E1 | End: flag 2 | Magic114_End 341 |
| E2 | End: the actor's flag 0x40 | Magic114_End 404 |
| DR1 | Ring: offset rows by i & 1 | Magic114_DrawRing 1995 |
| DR2 | Ring: +0x34 from +0x10 | Magic114_DrawRing 1995 |
| DR3 | Ring: the task not read again after the first turn | Magic114_DrawRing 431 |
| DR4 | Ring: +0xB = i + 1 | Magic114_DrawRing 2000 |
| DR5 | Ring: bias 3 | Magic114_DrawRing 2000 |
| DR6 | Ring: the start packet read after the loop | Magic114_DrawRing 198 |
| DR7 | Ring: x the task's | Magic114_DrawRing 1754 |
| TR1 | Triangle: lift x 2 | Magic114_DrawTriangle 1991 |
| TR2 | Triangle: lowered from index 5 | Magic114_DrawTriangle 193 |
| TR3 | Triangle: the key index not read again | Magic114_DrawTriangle 56 |
| TR4 | Triangle: colour of index 1 red 0x41 | Magic114_DrawTriangle 368 |
| TR5 | Triangle: flash when Frame_Counter & 0x10 is 0 | Magic114_DrawTriangle 15 |
| TR6 | Triangle: flash from index 3 | Magic114_DrawTriangle 97 |
| TR7 | Triangle: index 5 fade red (+9 + 0x15) << 2 | Magic114_DrawTriangle 34 |
| TR8 | Triangle: fades in phase 3 only | Magic114_DrawTriangle 75 |
| TR9 | Triangle: the packet on by 0x30 | Magic114_DrawTriangle 2000 |
| TR10 | Triangle: index 6 blue 0xB1 - +9 | Magic114_DrawTriangle 45 |
| TR11 | Triangle: the first vertex z 1 | Magic114_DrawTriangle 2000 |
| PM1 | PushMatrix: angle << 6 | Magic114_PushMatrix 1939 |
| PM2 | PushMatrix: height + 0x100 | Magic114_PushMatrix 2000 |
| PM3 | PushMatrix: x the task's | Magic114_PushMatrix 1730 |
| CT1 | ChildTask: kinds 0 and 1 swapped | Magic114_ChildTask 1325 |
| TRR1 | TrailRun: phases swapped | Magic114_TrailRun 2000 |
| TA1 | TrailAim: +0x14 from +0x3C | Magic114_TrailAim 1995 |
| TA2 | TrailAim: offsets << 4 | Magic114_TrailAim 2000 |
| TA3 | TrailAim: height + 0x201 | Magic114_TrailAim 2000 |
| TA4 | TrailAim: not centred | Magic114_TrailAim 2000 |
| TS1 | TrailSpawn: the sprite child +1 1 | Magic114_TrailSpawn 1981 |
| TS2 | TrailSpawn: Field_Kind2X read before the create | Magic114_TrailSpawn 2 |
| TS3 | TrailSpawn: the beam child at +9 1 | Magic114_TrailSpawn 921 |
| TS4 | TrailSpawn: freed past 0xD | Magic114_TrailSpawn 160 |
| TS5 | TrailSpawn: the beam child +9 0x11 | Magic114_TrailSpawn 916 |
| TS6 | TrailSpawn: the beam child +8 the owner's | Magic114_TrailSpawn 788 |
| DT1 | DrawTrail: (5 - +9) << 16 | Magic114_DrawTrail 1989 |
| DT2 | DrawTrail: the draw mode linked one row off | Magic114_DrawTrail 2000 |
| DT3 | DrawTrail: colour 0xE1 | Magic114_DrawTrail 2000 |
| DT4 | DrawTrail: the start point read again | Magic114_DrawTrail 179 |
| DT5 | DrawTrail: the end height from +0x3E | Magic114_DrawTrail 2000 |
| DT6 | DrawTrail: the side -0x800 | Magic114_DrawTrail 2000 |
| BR1 | BeamRun: the strip mode +0xA | Magic114_BeamRun 711 |
| BR2 | BeamRun: drawn without the +0 test | Magic114_BeamRun 639 |
| BR3 | BeamRun: glow and core swapped | Magic114_BeamRun 712 |
| BW1 | BeamWait: sound 0x103 | Magic114_BeamWait 456 |
| BW2 | BeamWait: +9 0xF | Magic114_BeamWait 456 |
| BW3 | BeamWait: the scroll kept | Magic114_BeamWait 456 |
| BG1 | BeamGrow: the flag at 0x32 | Magic114_BeamGrow 221 |
| BG2 | BeamGrow: flag 0x20 | Magic114_BeamGrow 221 |
| BG3 | BeamGrow: the task not read again after the flag | Magic114_BeamGrow 8 |
| BG4 | BeamGrow: scroll -4 | Magic114_BeamGrow 2000 |
| BF1 | BeamFade: length + 3 | Magic114_BeamFade 2000 |
| BC1 | Core: 54 steps at most | Magic114_DrawBeamCore 66 |
| BC2 | Core: widening below 0x21 | **not refused: equivalent** (below) |
| BC3 | Core: v mod 0xBD | Magic114_DrawBeamCore 1485 |
| BC4 | Core: page x 0x341 | Magic114_DrawBeamCore 1488 |
| BC5 | Core: pulse while +2 below 3 | Magic114_DrawBeamCore 781 |
| BC6 | Core: pulse sar 11 | Magic114_DrawBeamCore 1334 |
| BC7 | Core: grey +9 x 9 | Magic114_DrawBeamCore 777 |
| BC8 | Core: pulse by 2 i | Magic114_DrawBeamCore 1334 |
| BC9 | Core: commit 0x44 | Magic114_DrawBeamCore 1488 |
| BC10 | Core: nothing below length 3 | Magic114_DrawBeamCore 25 |
| BS1 | Strip: tpage by abr & 1 | Magic114_DrawBeamStrip 982 |
| BS2 | Strip: semi-transparency 1 | Magic114_DrawBeamStrip 1470 |
| BS3 | Strip: u by i & 7 | Magic114_DrawBeamStrip 696 |
| BS4 | Strip: v 0xC5 | Magic114_DrawBeamStrip 1472 |
| BS5 | Strip: grey +9 x 4 | Magic114_DrawBeamStrip 1471 |
| BS6 | Strip: narrowing below 9 | **not refused: equivalent** (below) |
| GL1 | Glow: tpage 0x56 | Magic114_DrawBeamGlow 2000 |
| GL2 | Glow: outer narrowing x 4 | Magic114_DrawBeamGlow 838 |
| GL3 | Glow: outer colour 2 | Magic114_DrawBeamGlow 1481 |
| GL4 | Glow: first outer point -0x40000 | Magic114_DrawBeamGlow 2000 |
| GL5 | Glow: grey +9 x 5 | Magic114_DrawBeamGlow 1479 |
| GL6 | Glow: the vertex copy a2 from ba | Magic114_DrawBeamGlow 539 |
| SR1 | SpriteRun: bank 0x8E3581 | Magic114_SpriteRun 2000 |
| SR2 | SpriteRun: bank after 0x8B3581 | Magic114_SpriteRun 2000 |
| SS1 | SpriteStart: CLUT row 0x1B | Magic114_SpriteStart 2000 |
| SS2 | SpriteStart: flip +8 & 1 | Magic114_SpriteStart 1998 |
| SS3 | SpriteStart: animation 1 | Magic114_SpriteStart 2000 |
| ST1 | SpriteTick: inverted | Magic114_SpriteTick 2000 |
| RY1 | Rays: from +9 0x1A | Magic114_DrawRays 356 |
| RY2 | Rays: nearer row from direction 1 | Magic114_DrawRays 305 |
| RY3 | Rays: radius & 0xF | Magic114_DrawRays 290 |
| RY4 | Rays: grey 0x81 - r x 16 | Magic114_DrawRays 1270 |
| RY5 | Rays: the far point sine and cosine swapped | Magic114_DrawRays 1270 |
| RY6 | Rays: the line linked as 0x1C | Magic114_DrawRays 1270 |
| RY7 | Rays: the task read before the matrix | Magic114_DrawRays 33 |
| PR1 | RayMatrix: axes swapped | Magic114_PushRayMatrix 2000 |
| PR2 | RayMatrix: offset 0xB000 | Magic114_PushRayMatrix 2000 |
| PR3 | RayMatrix: height + 0x2C0 | Magic114_PushRayMatrix 2000 |
| MT1 | 115 Task: TintUp and TintDown swapped | Magic115_Task 1034 |
| MS1 | 115 Start: the source read again for the tint | Magic115_Start 83 |
| MS2 | 115 Start: the child owned by the owner | Magic115_Start 1751 |
| MS3 | 115 Start: fifteen CLUT entries | Magic115_Start 2000 |
| MS4 | 115 Start: the first entry keeps its bit | Magic115_Start 1003 |
| MS5 | 115 Start: tint alpha 0 | Magic115_Start 2000 |
| MS6 | 115 Start: +0xB 2 | Magic115_Start 2000 |
| TU1 | TintUp: on from 7 | Magic115_TintUp 459 |
| TU2 | TintUp: green +9 << 2 | Magic115_TintUp 1995 |
| TD1 | TintDown: on even frames | Magic115_TintDown 2000 |
| TD2 | TintDown: green tested | Magic115_TintDown 485 |
| TD3 | TintDown: the next record released | Magic115_TintDown 481 |
| MC1 | 115 ChildTask: kinds swapped | Magic115_ChildTask 2000 |
| OR1 | OrbitRun: the screen point without the +0 test | Magic115_OrbitRun 740 |
| OR2 | OrbitRun: the bank not restored | Magic115_OrbitRun 2000 |
| OR3 | OrbitRun: the screen point with the engine's bank | Magic115_OrbitRun 740 |
| OS1 | OrbitStart: height + 0x2C1 | Magic115_OrbitStart 1997 |
| OS2 | SpriteSetUp: +0x48 2 | Magic115_OrbitStart 2000, Magic115_MoteStart 2000 |
| OS3 | SpriteSetUp: CLUT row 0xA1 | Magic115_OrbitStart 2000, Magic115_MoteStart 2000 |
| OS4 | OrbitStart: +9 0xFE | Magic115_OrbitStart 2000 |
| CI1 | Circle: angle & 0x1F | Magic115_Orbit 665, Magic115_OrbitFade 644 |
| CI2 | Circle: the angle not read again for the cosine | Magic115_OrbitFade 3 |
| CI3 | Circle: x 2 | Magic115_Orbit 1997, Magic115_OrbitFade 2000 |
| CI4 | Circle: the owner read before the sine | Magic115_Orbit 73, Magic115_OrbitFade 82 |
| OB1 | Orbit: a mote every fourth step | Magic115_Orbit 105 |
| OB2 | Orbit: the mote delay + 3 | Magic115_Orbit 1270 |
| OB3 | Orbit: the owner +0xB not counted | Magic115_Orbit 1270 |
| OB4 | Orbit: the end at 0x3F | Magic115_Orbit 554 |
| OB5 | Orbit: bit 0x10 | Magic115_Orbit 394 |
| OB6 | Orbit: the mote +1 0 | Magic115_Orbit 1270 |
| OF1 | OrbitFade: red down 3 | Magic115_OrbitFade 185 |
| OF2 | OrbitFade: red 0x81 | Magic115_OrbitFade 2000 |
| MR1 | MoteRun: phases 1 and 2 swapped | Magic115_MoteRun 985 |
| MST1 | MoteStart: animation 2 | Magic115_MoteStart 2000 |
| MTK1 | MoteTick: on by one | Magic115_MoteTick 1328 |
| MTK2 | MoteTick: the first tick tested | Magic115_MoteTick 660 |
| MF1 | MoteFade: red 0x81 | Magic115_MoteFade 2000 |
| K1 | 117 Task: Brighten and Dim swapped | Magic117_Task 1013 |
| K2 | TintSet: the source read once | Magic117_TintSet 30 |
| K3 | TintSet: three pulses | Magic117_TintSet 2000 |
| K4 | Brighten: on at 0x11 | Magic117_Brighten 916 |
| K5 | TintStep: green through +0xA | Magic117_Brighten 1992, Magic117_Dim 1992, Magic117_Darken 1988, Magic117_Lighten 1993 |
| K6 | Dim: back two phases | Magic117_Dim 325 |
| K7 | Dim: the actor flashed | Magic117_Dim 94 |
| K8 | 117 TaskB: Darken and Lighten swapped | Magic117_TaskB 966 |
| K9 | Darken: brighter | Magic117_Darken 2000 |
| K10 | Lighten: the actor flashed | Magic117_Lighten 411 |
| K11 | Lighten: the tint not released | Magic117_Lighten 455 |
| BC2b | Core: widening below 0x22 (BC2 near variant) | Magic114_DrawBeamCore 85 |
| BS6b | Strip: narrowing below 10 (BS6 near variant) | Magic114_DrawBeamStrip 120 |

**The two equivalent mutants.** BC2 moves the core's widening bound from
`d < 0x20` to `d < 0x21`: at d = 0x20 the widened value `0x20 << 14` is
0x80000, the value the other branch stores, so no input tells them apart;
BC2b (`< 0x22`) is refused. BS6 moves the strip's narrowing bound from 8 to
9: at `length - i` = 8 the narrowed value `-24 << 13` is 0xFFFD0000, the
other branch's; BS6b (`< 10`) is refused.

**The thinnest** are the re-reads across a call, which show only when the
disturbance moves exactly that cell across exactly that call: CI2 (the angle
read again for the cosine: 2 and 3 rounds), BG3 (the task after the flag
call), TS2 (`Field_Kind2X` after the create), T3 (the phase after the ring),
and TR5 (the flash's frame mask, which needs phase 2, an index above 3 and a
matching frame at once). A first run on a seed one step off the count edges
refused G2 in 4 rounds; the seeds were moved to the edges (431 now) and the
whole set run again.

**Re-run 2026-09-26 on the kFlag-fixed harness ([`magic_harness.md`](magic_harness.md) §8): 16 controls in the affected functions, 16 refused** (`Magic114_SpriteTick`, `Magic115_Orbit`, `_OrbitFade`, `_MoteTick`, `_MoteFade`: ST1, CI1..CI4, OB1..OB6, OF1, OF2, MTK1, MTK2, MF1), each by a count in the function or functions its plant touches, as before. Thinnest: CI2 3 rounds, CI4 73 / 82, OB1 105. Selected: every control whose plant lies in one of the group's section-8 functions (the CI rows plant in `Magic115_Circle`, which Orbit and OrbitFade share); the rest of the table plants outside them and stands without a re-run. The plants are the original round's own (its scratch script's anchors and edits, each anchor checked unique); OF1's anchor, found twice now (MoteFade has the same line), was lengthened by the function's header - the same edit. Each: plant, rebuild (the file checked recompiled), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s26`, restore, rebuild; then the clean self-test, 0 mismatches, exit 0 (`BOF3X_SHADOW='*'`: exit 0). No fuzz change; no equivalent mutant among them.

## 7. What nothing reached

No recorded route casts any of these spells (the queue's §5). The live check
is the owner casting them, with a save that has them or DIV-0045's cheat:

- MAGIC114 (Fire Whip by its label): the ring round the caster, the trails
  to the target side, the beam and its sprite, the rays at the close;
- MAGIC115 (Remedy): the caster tinted, a sprite circling it dropping motes;
- MAGIC117 (Rest / Snooze, Douse): the caster pulsing, the target flashing.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D98, D100 and D101 in [`known-defects.md`](known-defects.md).

Described, not numbered, nothing fixed:

- **Every dispatcher's index is unchecked**: the four stack tables (+1) and
  the seven `.data` tables, which lie back to back, so an index one past a
  table runs the next table's first handler (`Magic115_MotePhases` runs on
  into MAGIC118's). Nothing seen here steps one past; ours aborts.
- **`BattleTask_Create`'s 0xFF (no free slot) is not checked** by
  `Magic114_Launch`, `_TrailSpawn` (twice), `Magic115_Start` or
  `Magic115_Orbit`: each then writes its child's fields at slot 255,
  `0x93A000 + 0xFF x 0x84` = `0x94237C..`, past the 48 slots. Ours does the
  same.
- **`Magic114_DrawTriangle` indexes the caller's eight keys by +0xB read
  after its projection**: an index of 8 or more writes past the ring's frame
  array. The ring sets +0xB to 0..7 just before, and nothing between moves it
  in play.
- **`Magic114_DrawRays` passes `MapView_LinkPrimAt` a dword whose upper three
  bytes are stack garbage** (only its low byte, 0 or 0xFE, is set). The callee
  reads the low byte signed, so nothing follows from it.
- **The tint record indices** (+0xA in MAGIC115, +0xB in MAGIC117) are the
  bytes `Sprite_SetTint` answered, unchecked; `MoveScript_TintRecords` holds
  256 records, so every byte lands inside it.

## 9. For `analysis/calltrace/entries_logic.txt`

46 lines appended to the main checkout's copy under a `group S26` comment,
each function's own extent. Two were listed right already (`004D7C10 EC`,
`004D8A60 372`); five host lines run over their neighbours and were not
edited: `004D7D00 2FE` (its own is 0x2FC), `004D8000 2AD`, `004D82B0 7A7`,
`004D8DE0 538`, `004D9320 946` (the last into MAGIC115). The consolidation
keeps the smaller.
