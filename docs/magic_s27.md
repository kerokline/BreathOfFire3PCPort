# Group S27: Burn, Whelp Breath and DragonBreath (MAGIC118, 120, 121)

**Status:** IN PROGRESS (2026-09-26). All 47 functions are ours
(`src/game/magic_s27.cpp`, shadow name `magic_s27`), fuzzed headless through
the shared harness with 0 mismatches over 94,000 rounds; @@CONTROLS_SUMMARY@@
Nothing recorded casts these spells, so this is fuzz only until the owner sees
them cast.

This is round nine, second wave, group S27
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).
It takes three `Magic_Rows` overlays:

| Row | File | Overlay | Ability id | Read one id down | Extent |
|---|---|---|---|---|---|
| 60 | 0x287 | MAGIC118 | 0x76 | Burn | `0x4DA220..0x4DACCD`, 11 functions |
| 16 | 0x288 | MAGIC120 | 0x78 | Whelp Breath | `0x4DACD0..0x4DC109`, 22 functions |
| 122 | 0x289 | MAGIC121 | 0x79 | DragonBreath | `0x4DC110..0x4DD8A4`, 14 functions |

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2), so each is a hypothesis; the function
names use them the same way. By reading, the code fits a fire spell and two
breaths: eight flames rising round the source for MAGIC118, and for 120 and
121 a beam from the caster's screen point toward the targets' centre. Which
spell is which in play has not been measured.

The extents are `tools/magic_rows.py --unit MAGIC0NN --clones`. All 47
functions are in the units' extents; none was found inside them or missing.
The linker folded bodies across overlays: `SpellFx_Countdown` (`0x4DA3B0`) is
also MAGIC009's and MAGIC101's (Typhoon, group S23, which called it by address
until now), `DragonBreathBeam_Brighten` (`0x4DC530`) is reached from seven
files, and the Whelp beam's last three phases are MAGIC122's copies (group
S28).

## 1. What each function does

The `symbols.toml` evidence gives each function to the instruction. In
outline:

- **Burn (MAGIC118).**
  - `Burn_Task` (the row's kind-2 task): a five-entry stack table by +1 -
    `Burn_Start`, `Burn_WaitFlag`, MAGIC041's `0x4A6640` (on once +0xB is
    1), `SpellFx_Countdown`, `BattleFx_Finish` - then, while +0 and +1 are
    set, the ring and the disc under the actor's matrix.
  - `Burn_Start`: the task at the source sprite (`0x904B4C`); eight flames
    (kind-1 task 0x2F), each with a delay (i + 1) x 4 and this task its
    owner, +0xB counting them; CLUT row 26's two runs of fifteen made
    semi-transparent; sound 0x100.
  - `Burn_WaitFlag`: +9 up to 0x10, then target flags 0x10.
  - `SpellFx_Countdown`: +9 down to 0, then +1 on.
  - The flame (`BurnFlame_Task`, three phases through `BurnFlame_Phases`):
    `_Start` waits out its delay and stands at its owner; `_Rise` and `_Fade`
    climb (a speed +0x20 growing by +0xA, a height +0x14 growing by the
    speed); `_Fade` ends it and counts it off the owner's +0xB.
  - `BurnFlame_Draw`: a ring of 32 textured quads (POLY_GT4) round the
    actor matrix's origin, radius +9 x 4, their tops wavering with the frame
    counter.
  - `Burn_DrawRing` / `Burn_DrawDisc`: 32 gouraud quads between radii 0x80
    and 0xC0, and 32 gouraud triangles to radius 0x80, shaded by +9, between
    draw modes 0x55 and 0x15 on ordering slot 5.
- **Whelp Breath (MAGIC120).**
  - `WhelpBreath_Task`: a three-entry stack table - `_Start`, `_WaitBeam`,
    `Leech_WaitOrbs` (group S17's: held until +0xB is 0xFF, then the done
    flag).
  - `WhelpBreath_Start`: at the owner; three children of kind-1 task 0x0F -
    the beam (+1 0) and one or two sprites at the caster (+1 1; the second
    only without direction bit 1); CLUT row 26's three runs, the first and
    third semi-transparent.
  - `WhelpBreath_WaitBeam`: once the beam has grown (+0xB 1), two more
    sprite children: the flames (+0xB 2) and the glow (+0xB 3).
  - The child (`WhelpBreathChild_Task`) dispatches by +1 to the beam or the
    sprite.
  - The beam (`WhelpBreathBeam_Run`, five phases through
    `WhelpBreathBeam_Phases`): `_Aim` (the targets' centre, the screen
    points, `Math_Ratan2` into +0x14), `_Grow` (+0xA the length, up by 3;
    at 8 it signals the parent), then MAGIC122's sweep, hold and fade (the
    last sets the parent's +0xB 0xFF). `WhelpBreathBeam_Draw`: +0xA steps
    of 16 along a wavering angle, two textured quads a step either side of
    the line, width divided by +0xB. Screen space: the points are written
    as floats, not projected.
  - The sprite (`WhelpBreathSprite_Run`, eight phases through
    `WhelpBreathSprite_Phases`, under the effects' sprite bank `0x8E3580`):
    `_Start` sets the sprite fields and branches on +0xB; `_Play` / `_Hold`
    run the caster-side sprites' scripts; `WhelpBreathFlames_Grow` / `_End`
    and `WhelpBreathGlow_Grow` / `_Hold` / `_End` draw the flames and the
    glow for set counts.
  - `WhelpBreathFlames_Draw`: over every live enemy, three columns at
    `WhelpBreath_FlameOffsets` (turned by the enemy's direction), each up
    to six flames (`_DrawColumn`: textured squares at heights age x 15,
    fading with age) under the column's own matrix (`_PushMatrix`); then
    the six ages in `WhelpBreathFlames_Ages` step on.
  - `WhelpBreathGlow_Draw` / `_DrawFan`: under every live enemy one gouraud
    triangle, its angle from `WhelpBreath_GlowAngles` by direction.
- **DragonBreath (MAGIC121).**
  - `DragonBreath_Task`: a two-entry stack table - `_Start`,
    `Leech_WaitOrbs`.
  - `DragonBreath_Start`: at the owner; the beam (kind-1 task 0x5A); the
    same CLUT runs as Whelp's; sound 0x100.
  - The beam (`DragonBreathBeam_Task` through one entry, then
    `DragonBreathBeam_Run`, seven phases through `DragonBreathBeam_Phases`):
    `_Aim` (Whelp's aim, then three jitters), `_Widen` (+0xA the length, by
    8 to 0x20), `_Hit` (+0xB the width, to 8, target flags 0x10),
    `_Brighten`, `_Hold`, `_Dim`, `_End` (sets the parent's +0xB 0xFF).
    Every eighth frame the jitters are re-rolled.
  - `DragonBreathBeam_Draw`: +0xA steps of 12; four gouraud quads a step,
    two either side (an inner pair from the centre line, an outer pair
    beyond), tapered over the first four steps and jittered by Rand & 3.
  - `DragonBreathBeam_DrawLinesA` / `B`: six crackling lines, three either
    side, 47 line segments each, starting a given number of steps down the
    beam.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedent
([`magic_fx_reached.md`](magic_fx_reached.md) §3, round nine §7):

- A phase past any of the eleven dispatch tables (five on the stack, six in
  `.data`) aborts. The original calls through whatever follows.
- `WhelpBreathBeam_Draw` aborts where the original divides by a zero +0xB
  (§8).

Three calls push one argument more than the callee takes, as the originals
do, and ours pushes it too: `Gte_RotTrans` gets a flag pointer, and the two
projections get depth and flag pointers.

## 3. Calls to other units (by raw address, not bound)

| Address | Owner | Reached as |
|---|---|---|
| `0x4A6640` | MAGIC041 (S08) | entry 2 of `Burn_Task`'s stack table: +1 on once +0xB is 1 |
| `0x4DDC30` | MAGIC122 (S28) | entry 2 of `WhelpBreathBeam_Phases`: +0xB down, +9 up; at 0x18 target flags 0x10 |
| `0x4DDC70` | MAGIC122 (S28) | entry 3: +9 up to 0x78 |
| `0x4DDC90` | MAGIC122 (S28) | entry 4: +0xB up, +4 down, +9 up to 0x88; the owner's +0xB 0xFF, freed |
| `0x446770` | engine, unnamed | turns a task's dx / dz pair +0xC / +0x10 by its direction +8 |

They are in `src/game/magic_s27_callees.h`. Round seven's to nine's functions
(`BattleFx_Finish`, `Leech_WaitOrbs`, `MagicFx_CenterOnSide`,
`MagicFx_FormationOffset`, `MagicFx_PushActorMatrix`, the GTE / GPU layer,
`MapView_LinkPrimAt`, `Gfx_CommitPrim`, the `Sprite_*` functions) are called
through their names. The cells `0x9039D8` (the sprite bank) and its two
values are named there too.

**Called into from elsewhere:** group S23's `Typhoon_Task` named `0x4DA3B0`
by address; it now names it `SpellFx_Countdown` (`magic_s23.cpp`,
`magic_s23_callees.h`). The kind-1 task table `battle_fx_tasks.cpp` lists
`0x4DA3D0`, `0x4DAF00` and `0x4DC260` by address, as it lists every kind-1
entry; those are data, unchanged.

## 4. Named data (`symbols.toml` `[[data]]`)

| Kind | Tables |
|---|---|
| Handler tables | `BurnFlame_Phases` `0x65BAB8` (3), `WhelpBreathChild_Kinds` `0x65BAC4` (2), `WhelpBreathBeam_Phases` `0x65BACC` (5), `WhelpBreathSprite_Phases` `0x65BAE0` (8), `DragonBreathChild_Kinds` `0x65BB28` (1), `DragonBreathBeam_Phases` `0x65BB2C` (7) |
| MAGIC120's tables | `WhelpBreath_FlameOffsets` `0x65BB00` (three dx / dz / dy dword triples), `WhelpBreath_GlowAngles` `0x65BB24` (4 bytes) |
| `.bss` | `WhelpBreathFlames_Ages` `0x69ACC0` (6 bytes) |

This document records only their addresses and sizes, not their values.

## 5. The fuzz

`BOF3X_SHADOW=magic_s27` runs `magic_harness::Run` over all 47 clones, 2,000
rounds each, on the consolidated harness without edits.

**Callees and tables.** The group lists 50 callees: the GTE / GPU layer,
`Battle_ActorIsOut`, `Math_Ratan2`, `Sprite_SetAnimation`,
`Sprite_ScriptTick`, the unnamed `0x446770`, the group's own functions its
others call directly (`kPhase`: each logs the task it ran for; the two line
draws log their argument's low byte), and the eight entries of
`WhelpBreathSprite_Phases`. It lists the six `.data` handler tables.

**Regions.** Beyond the harness's own: DamageScratch `0x903850` and
`Prim_VertexScratch` `0x9037A0`; `Gfx_PacketNext` and a 1 KB packet buffer
of the fuzz's own; CLUT row 26 and its source; the sprite bank cell
`0x9039D8`; `WhelpBreathFlames_Ages`; MAGIC120's `.data` up to its handler
table.

**What the group adds, all through the harness's fields:**

- `effect` on `Gfx_CommitPrim` and `MapView_LinkPrimAt`: each logs the
  primitive it commits (`NoteBytes`, its size) and advances
  `Gfx_PacketNext` by the size as the real one does. Every primitive of a
  draw is built at the same few addresses, so without this only the last
  would be compared.
- `deref` on the projections (the vertices, 6 bytes each) and the matrix
  push's GTE callees, with `effect`s that write a result where the real
  callee writes (S22's).
- An `effect` on each of `WhelpBreathSprite_Phases`' eight entries that
  logs the sprite bank pointer, so a wrong bank value (switched back after
  the call) is seen.
- `settle`: `WhelpBreathBeam_Draw`'s divisor +0xB kept off 0 and both
  beams' step counts +0xA at most 7, whatever the disturbance moves;
  `WhelpBreathSprite_Run`'s phase +2 kept below 8 (it reads +2 after its
  draw mode's two calls - past its table the original calls through the
  next table's bytes, which crashed the first full run).
- `args`: the two line draws' argument, its low byte mostly under 0x10,
  garbage above.

**The seed.** Every dispatcher gets an index inside its table and, half the
time, +0 cleared (the draws' gate); each counter is seeded either side of
its end; the draws' loop bounds are kept short; the flames' ages straddle
0x10; the direction byte of the matrix push and the glow is 0..5 (the jump
table's default case included); `WhelpBreathBeam_Run`'s owner +0xB is 0xFF
half the time; `DragonBreathBeam_Run`'s frame counter is a multiple of 8
half the time.

**Disturb.** After a call the group's case moves one of: `Gfx_PacketNext`,
a scratch word, a vertex word, a task field the harness leaves alone (+0x10,
+0x14, +0x15, +0x20, +0x2E, +0x2F, +0x31, +0x3E, +0x5D..+0x5F), a flame's
age.

Result in this worktree (2026-09-26):

    shadow      magic_s27 self-test: 94000 rounds over 47 functions (2000 each), 5342315 calls to the stand-ins,
                0 MISMATCHES; 13512 bytes of state (17 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'` exits 0; so does `magic_s23` with its phase now named.

## 6. Controls

@@CONTROLS@@

## 7. What nothing reached

No recorded route casts any of these spells: the combat route casts none
(queue §5). The live check is the owner casting them, with a save that has
them or DIV-0045's cheat. Things to look for:

- Burn: eight flames rising in a ring at the source, a ring and disc at the
  actor, a semi-transparent CLUT row.
- Whelp Breath: a textured beam from the caster toward the targets, flame
  columns and a glow over each live enemy, sprites at the caster.
- DragonBreath: a gouraud beam with crackling lines either side.

Not reached by the fuzz: the real `Math_Ratan2`, GTE and GPU (recorders
stand in); a `BattleTask_Create` that answers 0xFF (the recorder answers
0..47; §8).

## 8. Latent defects (Capcom's, kept)

These are described here, not numbered:

- **`WhelpBreathBeam_Draw` divides by +0xB unchecked** (signed `idiv`). By
  reading it is never 0 while the beam draws: `_Aim` sets 0x11 and MAGIC122's
  sweep takes one off a frame for sixteen frames (to 1), and the fade adds.
  Ours aborts where the original would fault.
- **The children's slots are unchecked.** `Burn_Start`, `WhelpBreath_Start`,
  `WhelpBreath_WaitBeam` and `DragonBreath_Start` write into the slot
  `BattleTask_Create` answers; with all 48 slots taken it answers 0xFF and
  the writes land 0xFF x 0x84 past the table (from `0x9425FC`). Kept.
- **Every dispatcher's index is unchecked**, including the one-entry
  `DragonBreathChild_Kinds` (an index of 1 reaches `DragonBreathBeam_Aim`
  directly). `WhelpBreathSprite_Run` reads its index after two calls. Ours
  aborts past a table.
- **`WhelpBreathGlow_DrawFan` indexes `WhelpBreath_GlowAngles` (4 bytes) by
  the direction byte**, unbounded: a direction past 3 reads the handler
  table after it as angles. Kept.
- **`BurnFlame_Draw` sorts each quad at the task moved by its first point's
  x in both x and z** (the z uses x, not the point's second coordinate) -
  by reading a slip in the source; the depth order it gives is kept.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 41 lines under a `group S27` comment: 37 new,
plus four host extents re-listed at the function's own size (`004DAB00 1CE`,
`004DBE10 E4`, `004DBF70 19A`, `004DD4B0 3F5`). Six were listed right
already.
