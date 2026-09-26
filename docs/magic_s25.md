# Group S25: four spell overlays (MAGIC107..110)

**Status:** IN PROGRESS (2026-09-26). All 56 functions are ours
(`src/game/magic_s25.cpp`, shadow name `magic_s25`) and fuzzed headless through
the shared harness: 0 mismatches in 112,000 rounds. 113 negative controls planted one at a time: 107 refused by a count in the function they touch; the six not refused are equivalent mutants (section 3). Only the
fuzz covers this group: no recorded route casts any of these spells, so it
waits for the owner's eye.

This is group S25 of round nine's first spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) section 4),
taken with the harness of [`magic_harness.md`](magic_harness.md).

## 1. The four overlays

`tools/magic_rows.py --unit MAGIC107..110`. Each extent starts at its
overlay's `Magic_Rows` entry and runs to the next overlay's. Every function
in them is ours now: none was already ours, none was found outside the
extents, and none is missing.

| Overlay | Row | File | Ability ids (label read one id down) | Extent | Functions |
|---|--:|---|---|---|--:|
| MAGIC107 | 24 | 0x27C | 0x6B, 0xD1 ("Sleep") | `0x4D2D80..0x4D3E9A` | 15 |
| MAGIC108 | 25 | 0x27D | 0x1B (no label), 0x6C, 0xD2 ("Confuse") | `0x4D3EA0..0x4D49D2` | 13 |
| MAGIC109 | 12 | 0x27E | 0x6D, 0xD3 ("Depress") | `0x4D49E0..0x4D52FC` | 6 |
| MAGIC110 | 38 | 0x27F | 0x6E, 0xD4 ("Ragnarok") | `0x4D5300..0x4D610B` | 22 |

The names come from TCRF's and the sibling's labels read one id down
([`cut-content.md`](cut-content.md) section 2). They are **hypotheses**:
nothing read here confirms which spell a row is. What each function does
(section 2) is the reading. The prefixes are `SpellSleep_`, `SpellConfuse_`,
`SpellDepress_` and `SpellRagnarok_`. Whether a player or an enemy casts any
of these spells is not measured.

## 2. What they do

`symbols.toml` has one entry per function, with its reading (extent, every
call, every table). In short:

- **MAGIC107** is a kind-2 task with a two-entry stack table: its start, then
  MAGIC078's `0x4BDC10`, which frees the task once the owner's `+0xB` is 0xFF.
  - The start puts the effect in the middle of the target side (the library's
    `0x4FC0E0`). It then creates a kind-1 child `0x19` at the field's kind-2
    point, facing the way the record at `0x904B3C` does.
  - The child steps through four `.data` phases (`SpellSleep_ChildPhases`
    `0x65B918`): clear, grow to `+9` = 0x20, hold to 0xC0, fade `+0xA` to 0.
    At 0 it sets the owner's `+0xB` to 0xFF and frees itself.
  - While the child is live it draws under two actor matrices (the actor
    matrix of `MagicFx_PushActorMatrix`, with angles of its own). Under the
    swaying one: a line stem, a dome of flat quads and a fan of flat
    triangles. Under the turned one: a shadow fan, then a trail of gouraud
    quads and a trail of lines behind the sway.
- **MAGIC108** has the same task shape. Its child `0x1A` has six phases
  (`SpellConfuse_ChildPhases` `0x65B928`):
  1. It starts at the source sprite, takes one step turned by its facing
     (the engine's `0x446770`), and jumps onto the acting actor.
  2. It flies back towards the start point (the library's `0x4FBA90` /
     `0x4FBC70`).
  3. It chimes every 0x20 frames.
  4. It grows, then holds.
  5. It ends by setting the owner's `+0xB` to 0xFF.

  It draws rays (LINE_F2, one per count up to `+9`) under a matrix picked by
  its facing through a four-entry jump table, and two gouraud quads under a
  spinning matrix.
- **MAGIC109** is a four-entry stack table.
  - The start ends at once when the spell targets the caster's own side: the
    target byte 0x80 with a party caster, or 0x40 with an enemy caster.
    Otherwise it moves the target's screen box into VRAM at (0x340, 0x100)
    (DR_MOVE) and puts the sprite at a side corner (`SpellDepress_Corners`,
    by `0x904AAC`).
  - Open, hold (which sets target flag 0x10) and close then redraw the box as
    a vortex. `SpellDepress_DrawVortex` builds nine rings of sixteen points
    (`SpellDepress_Rings` `0x6989C0`), draws eight bands of a black F4 under a
    GT4 textured from the grabbed box, and twists each band further by the
    task's swinging `+0x64`.
- **MAGIC110** is a six-entry stack table ending in the engine's `0x43FE80`.
  - The start creates two kind-1 children `0x10` and counts `+0xB` up three
    times. The children count it back down, and the task goes on at 2.
  - The start also copies two CLUT strips (`0x80E980` to `0x812980`, 256
    words; `0x80B980` to `0x80F980`, 16 words) with the semi-transparency bit
    set on every entry but the first.
  - The task draws a screen-wide tint (two gouraud quads covering 0..319 x
    0..359) while it fades in, bursts (target flag 0x10, then six more
    children) and fades out (target flag 0x40).
  - The kind-1 child jumps through `SpellRagnarok_ChildKinds` (`0x65B960`) on
    `+1` to one of three kinds:
    - **A rising sprite**: five phases, the first MAGIC130's `0x4E47F0`. It
      runs with the sprite bank pointer `0x9039D8` switched to `0x8E3580`.
    - **A ring on the ground**: four phases, two of them MAGIC082's and
      MAGIC060's. It draws a disc and a rim.
    - **A spark**: an FT4 at an offset from `SpellRagnarok_SparkOffsets`,
      picked by `+0xB`.

## 3. The fuzz, and the controls

`BOF3X_SHADOW=magic_s25` runs the fuzz in `src/game/magic_s25_fuzz.cpp`.
It has 56 clones, from the clone tables `magic_rows.py --clones` printed,
with the names filled in. They contain:

- 13 stack-table immediates, re-aimed at handler recorders;
- the one jump table (`SpellConfuse_PushFacingMatrix`), moved into its copy;
- six `.data` phase tables (27 entries), swapped for recorders.

Beyond the standard set, the group lists its own callees:

- the draw library: `Math_Sin` / `Math_Cos`, the libgpu setters (two of them
  unnamed), the GTE entry points, `Gfx_CommitPrim`, `MapView_LinkPrimAt`;
- `Sprite_SetAnimation`;
- the other units' functions (section 5);
- 17 of its own functions that its other functions call.

The fuzz adds these to the harness's defaults:

- **Arguments logged by their bytes (`deref`).** A projection's SVECTORs are
  logged by their six bytes each; the RotTrans vector, the RotMatrix angles
  and the DR_MOVE rectangle likewise. So what each iteration of a loop hands
  the GTE is compared, not just what the scratch holds at the end.
  `Gte_RotTransPers4`'s ten arguments are all logged.
- **Effects.**
  - `Gfx_CommitPrim` and `MapView_LinkPrimAt` move `Gfx_PacketNext` on by
    their size through a packet buffer of the fuzz's own, 0x2000 bytes, as
    the real ones do. So every primitive a draw builds stays in the state
    that is compared.
  - `Gte_PushMatrix` keeps the facing `+8` below 4 (section 6).
- **Regions:**
  - `Gfx_PacketNext` and the buffer;
  - the vertex scratch `0x9037A0` and the words `0x903850..0x90385F`;
  - `Field_Kind2X` / `Z` and `Gfx_BufferIndex`;
  - the sprite bank pointer;
  - the four CLUT strips;
  - the vortex rings `0x698940..0x698EBF`.
- **Disturbance:** it moves `Gfx_PacketNext`, a vertex word, a scratch byte,
  the kind-2 point, the side corner, a ring byte or the buffer index. The
  loop counter at `0x903850` only gets a small low byte: a large value would
  make `SpellConfuse_DrawRays`' loop run for ever.
- **Seeds:**
  - every dispatcher inside its table;
  - each counter at its threshold and one step off it, half the time;
  - side targets (0x80 / 0x40) and every caster for `SpellDepress_Start`;
  - `+0x64` at the swing's ends;
  - the frame counter's low bits;
  - the sprite heights and the ring radius at their thresholds.

Result in this worktree, 2026-09-26 (stand-in counts depend on the build
directory: [`magic_harness.md`](magic_harness.md) section 7):

    shadow      magic_s25 self-test: 112000 rounds over 56 functions (2000 each), 10676325 calls to the stand-ins,
                0 MISMATCHES; 22113 bytes of state (20 regions) and the stand-ins' log compared

The coverage lines show every callee, and every handler in the stack and
`.data` tables, called by the originals. The draws run 752..1,073 times each
as a callee of a live child. `BOF3X_SHADOW='*'` exits 0.

**Controls:** each was planted one at a time by a script (not committed): apply,
build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s25`, restore. Counts are
the rounds of 2,000 that mismatched in each function named.

One hundred and thirteen planted, 107 refused by a count (exit 3), each only in the function it touches (M2 and J1 touch shared helpers and are refused in each of their callers). The six not refused are equivalent mutants - no input tells them from the original - and each has a refused neighbour (FA1b, FA2b, DO2b, R2b, Q1c). Two plants were refused only after the fuzz was widened: V4 (the vortex's winding at +0x64 = 0 needed a seed of 0 and its neighbours) and SC1 (the sprite bank pointer is overwritten before the function returns, so it needed `Sprite_UpdateScreen`'s effect, which notes the pointer the update draws with). The thinnest are FA3 (37 rounds: a vertex word read again after the draw, shown only when a recorder moves it) and CH1 (34), CS2 (39) - re-reads and a mask shown only at a threshold.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Sleep_Task: the two entries swapped | Sleep_Task 2000 |
| S1 | Sleep_Start: the facing from +9 | Sleep_Start 1994 |
| S2 | Sleep_Start: height + 0x4000001 | Sleep_Start 2000 |
| S3 | Sleep_Start: child parameter 0x18 | Sleep_Start 2000 |
| G1 | Sleep_ChildGrow: 0x21 | Sleep_ChildGrow 1018 |
| H1 | Sleep_ChildHold: 0xBF | Sleep_ChildHold 477 |
| M1 | Sleep_PushSway: shl 8 | Sleep_PushSwayMatrix 966 |
| M2 | the actor matrix: height / 4 (all four) | Sleep_PushSwayMatrix 2000, Sleep_PushTurnMatrix 2000, Confuse_PushFacingMatrix 2000, Confuse_PushSpinMatrix 1999 |
| ST1 | Sleep_DrawStem: 0x171 | Sleep_DrawStem 2000 |
| FA1 | Sleep_DrawFan: below -0x30 | no: equivalent - the counter steps from 0x81 by 0x10 and never lands on -0x30 |
| FA2 | Sleep_DrawFan: above 0x12 for the shade | no: equivalent - no drawn counter is 0x12 |
| DO1 | Sleep_DrawDome: drawn below 0xC2 | Sleep_DrawDome 2000 |
| DO2 | Sleep_DrawDome: shade test 0x82 | no: equivalent - the counter never lands on 0x82 |
| DO3 | Sleep_DrawDome: five bands | Sleep_DrawDome 2000 |
| TR1 | Sleep_DrawTrail: outer radius 0x1E1 | Sleep_DrawTrail 2000 |
| TR2 | Sleep_DrawTrail: fade 3 | Sleep_DrawTrail 1973 |
| TR3 | Sleep_DrawTrail: one step longer | Sleep_DrawTrail 2000 |
| TL1 | Sleep_DrawTrailLines: 20 for 21 | Sleep_DrawTrailLines 1974 |
| TL2 | Sleep_DrawTrailLines: fade 0xB | Sleep_DrawTrailLines 1974 |
| SF1 | Sleep_DrawShadowFan: radius 0x31 | Sleep_DrawShadowFan 1999 |
| SF2 | Sleep_DrawShadowFan: size 0x38 | Sleep_DrawShadowFan 2000 |
| CT1 | Confuse_Task: the entries swapped | Confuse_Task 2000 |
| CS1 | Confuse_Start: the facing from +9 | Confuse_Start 1991 |
| CS2 | Confuse_Start: the source read after the create | Confuse_Start 39 |
| CI1 | Confuse_ChildInit: height + 0xC1 | Confuse_ChildInit 2000 |
| CI2 | Confuse_ChildInit: actor 3 a party member | Confuse_ChildInit 396 |
| CI3 | Confuse_ChildInit: + 0xC00001 | Confuse_ChildInit 1984 |
| CF1 | Confuse_ChildFly: z sar 16 | Confuse_ChildFly 2000 |
| CF2 | Confuse_ChildFly: box 0x8001 | Confuse_ChildFly 2000 |
| CF3 | Confuse_ChildFly: speed 0x21 | Confuse_ChildFly 2000 |
| CH1 | Confuse_ChildChime: & 0xF | Confuse_ChildChime 34 |
| CG1 | Confuse_ChildGrow: 0x88 | Confuse_ChildGrow 503 |
| CH2 | Confuse_ChildHold: 0x61 | Confuse_ChildHold 1009 |
| CE1 | Confuse_ChildEnd: 0x81 | Confuse_ChildEnd 993 |
| PF1 | Confuse_PushFacing: case 1 z 0x800 | Confuse_PushFacingMatrix 477 |
| PF2 | Confuse_PushFacing: & 0xF | Confuse_PushFacingMatrix 977 |
| R1 | Confuse_DrawRays: semi in phase 4 | Confuse_DrawRays 1265 |
| R2 | Confuse_DrawRays: >= for > | no: equivalent - at +0xA == i both give 0 |
| R3 | Confuse_DrawRays: angle step 0x81 | Confuse_DrawRays 1863 |
| R4 | Confuse_DrawRays: * 4 for * 3 | Confuse_DrawRays 968 |
| Q1 | Confuse_DrawQuads: cap test 0xC1 | no: equivalent - r is a multiple of 8, never 0xC1 |
| Q2 | Confuse_DrawQuads: corners swapped | Confuse_DrawQuads 2000 |
| Q3 | Confuse_DrawQuads: shade 0x61 | Confuse_DrawQuads 1694 |
| DS1 | Depress_Start: own side < 2 | Depress_Start 71 |
| DS2 | Depress_Start: 241 lines | Depress_Start 1112 |
| DS3 | Depress_Start: xor 1 | Depress_Start 384 |
| DS4 | Depress_Start: +0x64 = 1 | Depress_Start 1617 |
| DO4 | Depress_Open: 0x56 | Depress_Open 541 |
| DH2 | Swing: turn at 0x81 | Depress_Hold 99, Depress_Close 62 |
| DH3 | Swing: turn at -0x81 | Depress_Hold 94, Depress_Close 88 |
| DC1 | Depress_Close: at 3 | Depress_Close 1001 |
| V1 | Depress_DrawVortex: rings >> 2 | Depress_DrawVortex 2000 |
| V2 | Depress_DrawVortex: twist + 1 | Depress_DrawVortex 2000 |
| V3 | Depress_DrawVortex: shade step 0x11 | Depress_DrawVortex 2000 |
| V4 | Depress_DrawVortex: winding >= 0 | Depress_DrawVortex 247 |
| V5 | Depress_DrawVortex: texture row + 0x88 | Depress_DrawVortex 1846 |
| V6 | Depress_DrawVortex: tpage 0x11E | Depress_DrawVortex 2000 |
| V7 | Depress_DrawVortex: ring copy 0x78 | Depress_DrawVortex 2000 |
| RT1 | Ragnarok_Task: fade out and end swapped | Ragnarok_Task 674 |
| RS1 | Ragnarok_Start: +9 0x59 | Ragnarok_Start 1909 |
| RS2 | Ragnarok_Start: sprite +9 0x29 | Ragnarok_Start 2000 |
| RS5 | Ragnarok_Start: first entry keeps the bit | Ragnarok_Start 961 |
| RW1 | Ragnarok_WaitChildren: 3 | Ragnarok_WaitChildren 1004 |
| RF1 | Ragnarok_FadeIn: bit 1 | Ragnarok_FadeIn 1082 |
| RF2 | Ragnarok_FadeIn: below 0x11 | Ragnarok_FadeIn 104 |
| RB1 | Ragnarok_Burst: +0xB b + 1 | Ragnarok_Burst 942 |
| RB2 | Ragnarok_Burst: five sparks | Ragnarok_Burst 942 |
| RO1 | Ragnarok_FadeOut: & 1 | Ragnarok_FadeOut 338 |
| TN1 | Ragnarok_DrawScreenTint: * 14 | Ragnarok_DrawScreenTint 1995 |
| TN2 | Ragnarok_DrawScreenTint: 359.0 wrong | Ragnarok_DrawScreenTint 2000 |
| RC1 | Ragnarok_Child: dispatch on +2 | Ragnarok_Child 1314 |
| SC1 | Ragnarok_SpriteChild: bank 0x8E3581 | Ragnarok_SpriteChild 285 |
| SC2 | Ragnarok_SpriteChild: from phase 3 | Ragnarok_SpriteChild 89 |
| SI1 | Ragnarok_SpriteInit: scale 0xC001 | Ragnarok_SpriteInit 2000 |
| SI2 | Ragnarok_SpriteInit: animation 1 | Ragnarok_SpriteInit 2000 |
| SR1 | Ragnarok_SpriteRise: 0x18001 | Ragnarok_SpriteRise 448 |
| J1 | the jitter & 7 | Ragnarok_SpriteRise 983, Ragnarok_SpriteClimb 973, Ragnarok_SpriteLeave 1017 |
| SL1 | Ragnarok_SpriteClimb: 0xB3 | Ragnarok_SpriteClimb 466 |
| SL2 | Ragnarok_SpriteLeave: 0x81 | Ragnarok_SpriteLeave 507 |
| RI1 | Ragnarok_RingInit: 0x60001 | Ragnarok_RingInit 2000 |
| RG1 | Ragnarok_RingGrow: 0xC40 | Ragnarok_RingGrow 485 |
| RR1 | Ragnarok_DrawRim: + 0x81 | Ragnarok_DrawRim 2000 |
| RR2 | Ragnarok_DrawRim: inner shade 2 | Ragnarok_DrawRim 2000 |
| SI4 | Ragnarok_SparkInit: z offset + 8 | Ragnarok_SparkInit 1757 |
| SP1 | Ragnarok_DrawSpark: clut 0x1E3 | Ragnarok_DrawSpark 2000 |
| SP2 | Ragnarok_DrawSpark: u 0x21 | Ragnarok_DrawSpark 2000 |
| C1 | Sleep_Child: dome and fan swapped | Sleep_Child 1021 |
| C2 | Sleep_Child: live test on +1 | Sleep_Child 968 |
| F1 | Sleep_ChildFade: owner +0xB 0xFE | Sleep_ChildFade 502 |
| ST2 | Sleep_DrawStem: the size 0x35 | Sleep_DrawStem 2000 |
| FA3 | Sleep_DrawFan: 0x9037B2 not read again | Sleep_DrawFan 37 |
| TT1 | Sleep_PushTurn: 0x401 | Sleep_PushTurnMatrix 985 |
| CC1 | Confuse_Child: spin and quads swapped | Confuse_Child 1029 |
| PS1 | Confuse_PushSpin: 0x401 | Confuse_PushSpinMatrix 963 |
| DT1 | Depress_Task: open and hold swapped | Depress_Task 1001 |
| DS5 | Depress_Start: no done flag | Depress_Start 181 |
| DO5 | Depress_Open: +0xA 0x41 | Depress_Open 493 |
| DH1 | Depress_Hold: flags 0x20 | Depress_Hold 481 |
| RS3 | Ragnarok_Start: counted twice | Ragnarok_Start 1986 |
| RS4 | Ragnarok_Start: 0x4000 for the bit | Ragnarok_Start 2000 |
| RO2 | Ragnarok_FadeOut: +0xA - 2 | Ragnarok_FadeOut 604 |
| RC2 | Ragnarok_RingChild: disc and rim swapped | Ragnarok_RingChild 766 |
| DD1 | Ragnarok_DrawDisc: shade << 3 | Ragnarok_DrawDisc 1985 |
| DD2 | Ragnarok_DrawDisc: 63 triangles | Ragnarok_DrawDisc 2000 |
| SK1 | Ragnarok_SparkChild: live test on +1 | Ragnarok_SparkChild 483 |
| SI3 | Ragnarok_SparkInit: +0xA 0x41 | Ragnarok_SparkInit 2000 |
| SF3 | Ragnarok_SparkFade: + 3 | Ragnarok_SparkFade 1995 |
| FA1b | Sleep_DrawFan: below -0x3F | Sleep_DrawFan 2000 |
| FA2b | Sleep_DrawFan: above 0x21 for the shade | Sleep_DrawFan 1180 |
| DO2b | Sleep_DrawDome: shade test 0x91 | Sleep_DrawDome 1801 |
| R2b | Confuse_DrawRays: x 1 for 0 | Confuse_DrawRays 676 |
| Q1b | Confuse_DrawQuads: cap test 0xB8 | no: equivalent - r above 0xB8 is at least 0xC0, capped to 0xC0 either way |
| Q1c | Confuse_DrawQuads: capped at 0xC8 | Confuse_DrawQuads 332 |

## 4. What the route reaches

Nothing. No recorded route casts any of these spells, and the combat route's
first-call trace enters no function of these four extents
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)
section 5). The live check is the owner's: cast each spell with a save that
has it, or with DIV-0045's cheat, and watch for:

- Sleep's stem, dome and trail;
- Confuse's rays;
- Depress's vortex over the target;
- Ragnarok's tint, rising sprite, ring and sparks.

## 5. Calls into other units (by raw address, not bound)

| Address | Owner | Called from | What it is, by reading |
|---|---|---|---|
| `0x4FC0E0` | the effect library (L) | `SpellSleep_Start`, `SpellRagnarok_Start` | the sprite to the average position of the target side's live members (by byte `0x904B44`'s 0x40 bit) |
| `0x4FBA90` | L | `SpellConfuse_ChildFly` | one step of the sprite towards a point passed as a VECTOR by value, at a speed argument |
| `0x4FBC70` | L | `SpellConfuse_ChildFly` | 1 when the sprite is within a box of a point, else 0 |
| `0x4BDC10` | MAGIC078 (S17) | stack tables of `SpellSleep_Task`, `SpellConfuse_Task` | free the task once its `+0xB` is 0xFF (target flag 0x40, the done flag) |
| `0x4E47F0` | MAGIC130 (S30) | `SpellRagnarok_SpritePhases` entry 0 | `+9` down, `+2` on at 0 |
| `0x4C0680` | MAGIC082 (S18) | `SpellRagnarok_RingPhases` entry 2 | `+2` on once the owner's `+0xB` is 2 or less |
| `0x4B1740` | MAGIC060 (S12) | `SpellRagnarok_RingPhases` entry 3 | `+9` down; at 0 it goes on to the owner (`0x93B940`) - read no further |
| `0x43FE80` | the engine (row 128's, group E) | `SpellRagnarok_Task` entry 5 | the done flag, then `BattleTask_FreeCurrent` |
| `0x446770` | the engine | `SpellConfuse_ChildInit` | turns a sprite's `+0xC` / `+0x10` by its facing `+8` |
| `0x5A7570`, `0x5A76F0` | libgpu (no group) | `SpellSleep_DrawFan`, `SpellSleep_DrawTrailLines` | SetPolyF3 (code 0x20) and SetLineG4 (code 0x5C): the float 0.01 into the vertex pads |

## 6. Defects (Capcom's, latent, kept)

These are described here, not numbered. For every dispatcher on the list,
ours aborts past the table, as the project's precedent has it, and the fuzz
keeps each index inside.

- **Stack and `.data` phase tables are unbounded:**
  - the task stack tables of 2, 2, 4 and 6 entries;
  - the child tables of 4 and 6;
  - `SpellRagnarok_ChildKinds` (3; phase 3 would jump into
    `SpellRagnarok_SpritePhases`' first entry, MAGIC130's code);
  - the sprite, ring and spark phase tables of 5, 4 and 2.
- **`SpellConfuse_PushFacingMatrix`'s jump table** sends a facing past 3 to
  the common tail with the three angle words never written. The matrix would
  be built from whatever was on the stack.
- **`SpellDepress_Start`'s corner** (`0x904AAC`, xor 2) is unbounded. A
  value past 3 reads other `.data` as a screen point. This is a read, so it
  is kept.
- **`SpellRagnarok_SparkInit`'s offset row** (`+0xB`) is unbounded. Only
  0..5 are set by `SpellRagnarok_Burst`.
- **`SpellConfuse_ChildFly` passes a VECTOR by value with its pad word
  uninitialised.** The callee reads three words, so this is harmless.
- **`SpellRagnarok_DrawScreenTint` writes `+9 * 15` to `0x903854` and
  overwrites it at once with `+0xA * 15`.** It is a dead store, kept.

No divergence and no ledger entry: each function is a faithful replacement
apart from the aborts above.

## 7. Notes for other groups

- `0x5A7570` and `0x5A76F0` (libgpu SetPolyF3 / SetLineG4) are still
  unnamed.
- `SpellDepress_Corners` (`0x65B940`) sits between two of this group's phase
  tables, in the same `.data` run as MAGIC108's.
- The 46 lines for this group are appended to the main checkout's
  `analysis/calltrace/entries_logic.txt`. Ten extents were already listed
  right. Seven were host extents and are re-listed smaller.
