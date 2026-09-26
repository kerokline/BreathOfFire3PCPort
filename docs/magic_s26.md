# Group S26: MAGIC114, MAGIC115 and MAGIC117 (Fire Whip, Remedy, Rest / Snooze, Douse read one id down)

**Status:** IN PROGRESS (2026-09-26). All 48 functions are ours
(`src/game/magic_s26.cpp`, shadow name `magic_s26`), fuzzed headless through
the shared harness with 0 mismatches over 96,000 rounds; CONTROLS_SUMMARY.
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

    RESULT_LINE

Coverage: every callee and handler the clones name is reached - e.g.
`0x446770` 170,296, `MapView_LinkPrimAt` 165,536, `Gte_RotAverage4` 59,148,
`MagicFx_LinkByDepth` 2,000, `Tint_Release` 448, `Battle_SetTargetFlags` 214,
each sprite phase 479..1,013. `BOF3X_SHADOW='*'`: exit 0.

## 6. Controls

CONTROLS_TEXT

## 7. What nothing reached

No recorded route casts any of these spells (the queue's §5). The live check
is the owner casting them, with a save that has them or DIV-0045's cheat:

- MAGIC114 (Fire Whip by its label): the ring round the caster, the trails
  to the target side, the beam and its sprite, the rays at the close;
- MAGIC115 (Remedy): the caster tinted, a sprite circling it dropping motes;
- MAGIC117 (Rest / Snooze, Douse): the caster pulsing, the target flashing.

## 8. Latent defects (Capcom's, kept)

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
