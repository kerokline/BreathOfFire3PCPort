# Group S27: Burn, Whelp Breath and DragonBreath (MAGIC118, 120, 121)

**Status:** IN PROGRESS (2026-09-26). All 47 functions are ours
(`src/game/magic_s27.cpp`, shadow name `magic_s27`), fuzzed headless through
the shared harness with 0 mismatches over 94,000 rounds; 167 negative controls, 166 refused and one an equivalent mutant (its near variant refused).
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

There are 167 plants, each put in `magic_s27.cpp` one at a time by a script
that was not committed (it anchors each plant on a string it checks is unique;
for each: plant, rebuild, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s27`,
restore; the clean source rebuilt at the end). **166 are refused**, every one
by a count (exit 3) and only in the functions the plant touches; one is an
equivalent mutant, and its near variant is refused. The figures are
mismatched rounds out of 2,000 per function, in this worktree.

| | Planted | Refused in |
|---|---|---|
| B1 | Burn_Task: draws gated on +2 | Burn_Task 401 |
| B2 | Burn_Task: disc before ring | Burn_Task 788 |
| B3 | Burn_Task: entries 3/4 swapped | Burn_Task 811 |
| B4 | Burn_Start: height from the source's +0x38 | Burn_Start 2,000 |
| B5 | Burn_Start: delay (i + 2) x 4 | Burn_Start 2,000 |
| B6 | Burn_Start: parameter 0x30 | Burn_Start 2,000 |
| B7 | Burn_Start: the task read before the create | Burn_Start 445 |
| B8 | Burn_Start: second run not semi-transparent | Burn_Start 2,000 |
| B9 | LightClutRow: the last run's entry 0 kept | Burn_Start 2,000; WhelpBreath_Start 2,000; DragonBreath_Start 2,000 |
| B10 | Burn_Start: +9 1 | Burn_Start 2,000 |
| B11 | Burn_WaitFlag: at 0x11 | Burn_WaitFlag 849 |
| B12 | Burn_WaitFlag: flags 0x20 | Burn_WaitFlag 414 |
| B13 | SpellFx_Countdown: on at 1 | SpellFx_Countdown 1,333 |
| B14 | BurnFlame_Task: entries 1/2 swapped | BurnFlame_Task 1,335 |
| B15 | BurnFlame_Task: draw gated on +2 | BurnFlame_Task 472 |
| B16 | BurnFlame_Start: speed 9 | BurnFlame_Start 655 |
| B17 | BurnFlame_Start: +0xB 0x11 | BurnFlame_Start 655 |
| B18 | Climb: speed up by +0xB | BurnFlame_Rise 1,995; BurnFlame_Fade 1,992 |
| B19 | BurnFlame_Rise: on at 0x12 | BurnFlame_Rise 448 |
| B20 | BurnFlame_Fade: the owner's +0xA | BurnFlame_Fade 656 |
| B21 | BurnFlame_Fade: +0xB kept | BurnFlame_Fade 1,996 |
| B22 | BurnFlame_Draw: top shade +0xB << 4 | BurnFlame_Draw 1,943 |
| B23 | BurnFlame_Draw: height + 0x21 | BurnFlame_Draw 1,996 |
| B24 | BurnFlame_Draw: waver & 7 | BurnFlame_Draw 2,000 |
| B25 | BurnFlame_Draw: z sorted by the first point's y | BurnFlame_Draw 1,999 |
| B26 | BurnFlame_Draw: far v + 0xB | BurnFlame_Draw 2,000 |
| B27 | BurnFlame_Draw: u1 (k + 2) x 8 | BurnFlame_Draw 2,000 |
| B28 | BurnFlame_Draw: page x 0x380 | BurnFlame_Draw 2,000 |
| B29 | BurnFlame_Draw: depths 4_10 | BurnFlame_Draw 2,000 |
| B30 | BurnFlame_Draw: sorted size 0x50 | BurnFlame_Draw 2,000 |
| B31 | BurnFlame_Draw: point 1 green 2 | BurnFlame_Draw 2,000 |
| B32 | Burn_DrawRing: outer 0xC1 | Burn_DrawRing 1,999 |
| B33 | Burn_DrawRing: shade +9 x 5 | Burn_DrawRing 1,973 |
| B34 | Burn_DrawRing: point 0 y from x | Burn_DrawRing 2,000 |
| B35 | Burn_DrawRing: slot 4 | Burn_DrawRing 2,000 |
| B36 | Burn_DrawRing: closing tpage 0x55 | Burn_DrawRing 2,000 |
| B37 | Burn_DrawDisc: rim shade +9 x 7 | Burn_DrawDisc 1,987 |
| B38 | Burn_DrawDisc: centre blue a | Burn_DrawDisc 1,993 |
| B39 | Burn_DrawDisc: size 0x30 | Burn_DrawDisc 2,000 |
| W1 | WhelpBreath_Task: entries 0/1 swapped | WhelpBreath_Task 1,312 |
| W2 | WhelpBreath_Start: beam delay 5 | WhelpBreath_Start 1,962 |
| W3 | WhelpBreath_Start: sprite's owner the task | WhelpBreath_Start 1,725 |
| W4 | WhelpBreath_Start: direction bit 0 | WhelpBreath_Start 1,029 |
| W5 | WhelpBreath_Start: second sprite +0xB 2 | WhelpBreath_Start 1,000 |
| W6 | WhelpBreath_Start: middle run semi-transparent | WhelpBreath_Start 2,000 |
| W7 | WhelpBreath_WaitBeam: at 2 | WhelpBreath_WaitBeam 1,028 |
| W8 | WhelpBreath_WaitBeam: kinds 1 and 2 | WhelpBreath_WaitBeam 1,024 |
| W9 | WhelpBreath_WaitBeam: the task read before the create | WhelpBreath_WaitBeam 60 |
| W10 | WhelpBreathChild_Task: kinds swapped | WhelpBreathChild_Task 2,000 |
| W11 | WhelpBreathBeam_Run: owner 0xFE | WhelpBreathBeam_Run 801 |
| W12 | WhelpBreathBeam_Run: MAGIC122's hold / fade swapped | WhelpBreathBeam_Run 785 |
| W13 | Aim: kept y from +0x2E | WhelpBreathBeam_Aim 685; DragonBreathBeam_Aim 658 |
| W14 | Aim: Math_Ratan2's arguments swapped | WhelpBreathBeam_Aim 685; DragonBreathBeam_Aim 661 |
| W15 | Aim: y moved by +0x12 | WhelpBreathBeam_Aim 685; DragonBreathBeam_Aim 661 |
| W16 | Aim: screen point before the move to the owner | WhelpBreathBeam_Aim 53; DragonBreathBeam_Aim 53 |
| W17 | WhelpBreathBeam_Aim: +0xB 0x12 | WhelpBreathBeam_Aim 676 |
| W18 | WhelpBreathBeam_Aim: sound before +2 on | WhelpBreathBeam_Aim 24 |
| W19 | WhelpBreathBeam_Grow: length up by 4 | WhelpBreathBeam_Grow 2,000 |
| W20 | WhelpBreathBeam_Grow: at 7 | WhelpBreathBeam_Grow 930 |
| W21 | WhelpBreathBeam_Draw: waver x 12 | WhelpBreathBeam_Draw 1,639 |
| W22 | WhelpBreathBeam_Draw: step << 5 | WhelpBreathBeam_Draw 1,640 |
| W23 | WhelpBreathBeam_Draw: near width's angle without - 1 | WhelpBreathBeam_Draw 1,640 |
| W24 | BeamWidth: + 2 | WhelpBreathBeam_Draw 1,640 |
| W25 | BeamWidth: x 3 | WhelpBreathBeam_Draw 1,640 |
| W26 | WhelpBreathBeam_Draw: far base k x 4 + 5 | WhelpBreathBeam_Draw 473 |
| W27 | WhelpBreathBeam_Draw: point 1 y from x | WhelpBreathBeam_Draw 1,637 |
| W28 | WhelpBreathBeam_Draw: page x 0x340 | WhelpBreathBeam_Draw 1,640 |
| W29 | WhelpBreathBeam_Draw: second quad u 0x41 | WhelpBreathBeam_Draw 1,640 |
| W30 | BeamV: x 0x1F | WhelpBreathBeam_Draw 1,630 |
| W31 | BeamShade: x 11 | WhelpBreathBeam_Draw 1,640 |
| W32 | WhelpBreathBeam_Draw: second angle - 0x3FF | WhelpBreathBeam_Draw 1,639 |
| W33 | WhelpBreathBeam_Draw: size 0x50 | WhelpBreathBeam_Draw 1,640 |
| W34 | WhelpBreathBeam_Draw: one step short | WhelpBreathBeam_Draw 973 |
| W35 | WhelpBreathSprite_Run: bank + 1 | WhelpBreathSprite_Run 2,000 |
| W36 | WhelpBreathSprite_Run: default bank + 4 | WhelpBreathSprite_Run 2,000 |
| W37 | WhelpBreathSprite_Run: glow grow / hold swapped | WhelpBreathSprite_Run 459 |
| W38 | WhelpBreathSprite_Start: +0x27 0xA0 | WhelpBreathSprite_Start 2,000 |
| W39 | WhelpBreathSprite_Start: +0x2A bit 1 | WhelpBreathSprite_Start 1,268 |
| W40 | WhelpBreathSprite_Start: animation 2 | WhelpBreathSprite_Start 213 |
| W41 | WhelpBreathSprite_Start: +0x2A not inverted | WhelpBreathSprite_Start 213 |
| W42 | WhelpBreathSprite_Start: +9 0x77 | WhelpBreathSprite_Start 381 |
| W43 | WhelpBreathSprite_Start: five ages cleared | WhelpBreathSprite_Start 426 |
| W44 | WhelpBreathSprite_Start: +0xA 0x51 | WhelpBreathSprite_Start 373 |
| W45 | WhelpBreathSprite_Start: 4 as 3 | WhelpBreathSprite_Start 382 |
| W46 | WhelpBreathSprite_Play: freed while running | WhelpBreathSprite_Play 2,000 |
| W47 | WhelpBreathSprite_Hold: screen before the tick | WhelpBreathSprite_Hold 2,000 |
| W48 | WhelpBreathFlames_Grow: at 0x5F | WhelpBreathFlames_Grow 869 |
| W49 | WhelpBreathFlames_End: at 0x6F | WhelpBreathFlames_End 863 |
| W50 | WhelpBreathGlow_Grow: at 0x11 | WhelpBreathGlow_Grow 873 |
| W51 | WhelpBreathGlow_Hold: +9 down | WhelpBreathGlow_Hold 2,000 |
| W52 | WhelpBreathGlow_End: freed at 1 | WhelpBreathGlow_End 1,291 |
| W53 | WhelpBreathFlames_Draw: offsets by 8 | WhelpBreathFlames_Draw 1,992 |
| W54 | WhelpBreathFlames_Draw: +0xA / +0xB swapped | WhelpBreathFlames_Draw 1,974 |
| W55 | WhelpBreathFlames_Draw: z from dx | WhelpBreathFlames_Draw 2,000 |
| W56 | WhelpBreathFlames_Draw: height from dz | WhelpBreathFlames_Draw 2,000 |
| W57 | WhelpBreathFlames_Draw: direction from +9 | WhelpBreathFlames_Draw 1,981 |
| W58 | WhelpBreathFlames_Draw: 3j <= +9 | WhelpBreathFlames_Draw 187 |
| W59 | WhelpBreathFlames_Draw: & 0x1F | WhelpBreathFlames_Draw 240 |
| W60 | WhelpBreathFlames_Draw: actor e + 2 | WhelpBreathFlames_Draw 2,000 |
| W61 | WhelpBreathFlames_DrawColumn: k > +9 | WhelpBreathFlames_DrawColumn 229 |
| W62 | WhelpBreathFlames_DrawColumn: ages up to 0x10 | WhelpBreathFlames_DrawColumn 305 |
| W63 | WhelpBreathFlames_DrawColumn: radius + 0x33 | WhelpBreathFlames_DrawColumn 1,723 |
| W64 | WhelpBreathFlames_DrawColumn: height x 16 | WhelpBreathFlames_DrawColumn 1,708 |
| W65 | WhelpBreathFlames_DrawColumn: corner 0xA01 | WhelpBreathFlames_DrawColumn 1,724 |
| W66 | WhelpBreathFlames_DrawColumn: CLUT x 0x21 | WhelpBreathFlames_DrawColumn 1,724 |
| W67 | WhelpBreathFlames_DrawColumn: v 0x61 | WhelpBreathFlames_DrawColumn 1,724 |
| W68 | WhelpBreathFlames_DrawColumn: shade 0x60 | WhelpBreathFlames_DrawColumn 1,724 |
| W69 | WhelpBreathFlames_DrawColumn: depths 4_10B | WhelpBreathFlames_DrawColumn 1,724 |
| W70 | WhelpBreathFlames_PushMatrix: direction 1 about y | WhelpBreathFlames_PushMatrix 225 |
| W71 | WhelpBreathFlames_PushMatrix: height >> 1 | WhelpBreathFlames_PushMatrix 496 |
| W72 | WhelpBreathFlames_PushMatrix: z - 0x3FFF | WhelpBreathFlames_PushMatrix 2,000 |
| W73 | WhelpBreathGlow_Draw: height from +0x3C | WhelpBreathGlow_Draw 1,923 |
| W74 | WhelpBreathGlow_Draw: direction not copied | WhelpBreathGlow_Draw 1,871 |
| W75 | WhelpBreathGlow_DrawFan: frame & 0x1F | WhelpBreathGlow_DrawFan 1,033 |
| W76 | WhelpBreathGlow_DrawFan: - 5 | WhelpBreathGlow_DrawFan 2,000 |
| W77 | WhelpBreathGlow_DrawFan: + 7 | WhelpBreathGlow_DrawFan 2,000 |
| W78 | WhelpBreathGlow_DrawFan: centre x 10 + 2 | WhelpBreathGlow_DrawFan 2,000 |
| W79 | WhelpBreathGlow_DrawFan: tpage 0xD4 | WhelpBreathGlow_DrawFan 2,000 |
| D1 | DragonBreath_Task: entries swapped | DragonBreath_Task 2,000 |
| D2 | DragonBreath_Start: parameter 0x5B | DragonBreath_Start 2,000 |
| D3 | DragonBreath_Start: middle run semi-transparent | DragonBreath_Start 2,000 |
| D4 | DragonBreath_Start: +9 1 | DragonBreath_Start 2,000 |
| D5 | DragonBreath_Start: beam delay 3 | DragonBreath_Start 1,957 |
| D6 | DragonBreathBeam_Run: every fourth frame | DragonBreathBeam_Run 54 |
| D7 | DragonBreathBeam_Run: +0x5E & 1 | DragonBreathBeam_Run 180 |
| D8 | DragonBreathBeam_Run: +0x5E + 6 | DragonBreathBeam_Run 860 |
| D9 | DragonBreathBeam_Run: 0xB - +0x5E | DragonBreathBeam_Run 860 |
| D10 | DragonBreathBeam_Run: gated on +1 | DragonBreathBeam_Run 389 |
| D11 | DragonBreathBeam_Aim: +9 0x11 | DragonBreathBeam_Aim 661 |
| D12 | DragonBreathBeam_Aim: +0xC 1 | DragonBreathBeam_Aim 661 |
| D13 | DragonBreathBeam_Aim: +0x5D & 3 | DragonBreathBeam_Aim 266 |
| D14 | DragonBreathBeam_Widen: by 7 | DragonBreathBeam_Widen 2,000 |
| D15 | DragonBreathBeam_Hit: at 7 | DragonBreathBeam_Hit 963 |
| D16 | DragonBreathBeam_Brighten: at 0x12 | DragonBreathBeam_Brighten 875 |
| D17 | DragonBreathBeam_Hold: at 0x2D | DragonBreathBeam_Hold 871 |
| D18 | DragonBreathBeam_Dim: by 1 | DragonBreathBeam_Dim 2,000 |
| D19 | DragonBreathBeam_End: +0xB down past 0 | DragonBreathBeam_End 1,014 |
| D20 | DragonBreathBeam_End: owner 0xFE | DragonBreathBeam_End 646 |
| D21 | DragonBreathBeam_Draw: steps of 13 | DragonBreathBeam_Draw 1,702 |
| D22 | DragonBreathBeam_Draw: near taper to 3 | DragonBreathBeam_Draw 510 |
| D23 | DragonBreathBeam_Draw: far taper to 4 | **not refused** - an equivalent mutant (below) |
| D23b | DragonBreathBeam_Draw: far taper to 5 (D23's near variant) | DragonBreathBeam_Draw 290 |
| D24 | DragonBreathBeam_Draw: shade x 14 | DragonBreathBeam_Draw 1,699 |
| D25 | DragonBreathBeam_Draw: jitter & 7 | DragonBreathBeam_Draw 1,306 |
| D26 | DragonBreathBeam_Draw: near jitter the new one | DragonBreathBeam_Draw 1,519 |
| D27 | ShadeRed: >> 2 | DragonBreathBeam_Draw 47 |
| D28 | DragonBreathBeam_Draw: outer + 0x11 | DragonBreathBeam_Draw 511 |
| D29 | DragonRadii: near x step | DragonBreathBeam_Draw 1,703 |
| D30 | DragonBreathBeam_Draw: second quad point 0 full | DragonBreathBeam_Draw 1,690 |
| D31 | ShadeOne: red 2 | DragonBreathBeam_Draw 1,703 |
| D32 | DragonBreathBeam_Draw: last quad size 0x40 | DragonBreathBeam_Draw 1,703 |
| D33 | DragonCentres: far y from x | DragonBreathBeam_Draw 1,703 |
| D34 | DragonSides: far x by the near radius | DragonBreathBeam_Draw 1,703 |
| D35 | DrawLines: n steps | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |
| D36 | DrawLines: n & 0x1FF | DragonBreathBeam_DrawLinesA 997; DragonBreathBeam_DrawLinesB 1,025 |
| D37 | DrawLines: 64 out | DragonBreathBeam_DrawLinesA 1,882; DragonBreathBeam_DrawLinesB 1,889 |
| D38 | DrawLines: first radius << 3 | DragonBreathBeam_DrawLinesA 1,900; DragonBreathBeam_DrawLinesB 1,896 |
| D39 | DrawLines: shade + 6 | DragonBreathBeam_DrawLinesA 1,963; DragonBreathBeam_DrawLinesB 1,980 |
| D40 | DrawLines: walk <= 12 | DragonBreathBeam_DrawLinesA 1,922; DragonBreathBeam_DrawLinesB 1,900 |
| D41 | DrawLines: back to the kept x for y | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |
| D42 | DrawLines: swing + 0x11 | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |
| D43 | DrawLines: (k & 0x3F) | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |
| D44 | DrawLines: blue / 4 | DragonBreathBeam_DrawLinesA 1,989; DragonBreathBeam_DrawLinesB 1,992 |
| D45 | DrawLines: size 0x24 | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |
| D46 | DragonBreathBeam_DrawLinesB: side - 0x3FF | DragonBreathBeam_DrawLinesB 2,000 |
| D47 | DrawLines: first point x from y | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |
| D48 | DrawLines: kept y not moved | DragonBreathBeam_DrawLinesA 2,000; DragonBreathBeam_DrawLinesB 2,000 |

**D23, not refused: equivalent.** `DragonBreathBeam_Draw`'s far radius is
`+0xB x step` while the step is below 4, else `+0xB x 4`. Moving the bound to
`step <= 4` changes only step 4, where both give `+0xB x 4`: no input tells
them apart. The near variant D23b (`step <= 5`, which differs at step 5) is
refused in 290 rounds.

The thinnest are W18 (24 rounds: the beam's sound moved before `+2` on - seen
only when the sound's recorder moves `Sprite_Current` and the increment lands
on another slot), D27 (47: the signed `/ 4` of the shade as a shift - it
differs only for a negative shade, which only the disturbance's scratch word
makes), W16 (53 each: only a recorder's disturbance tells the two orders of
`AtOwner` and the screen update apart), D6 (54) and W9 (60: the task pointer
read before the create, seen when the create's disturbance moves
`Sprite_Current`).

**Re-run 2026-09-26 on the kFlag-fixed harness ([`magic_harness.md`](magic_harness.md) §8): 12 controls in the affected functions, 12 refused** (`WhelpBreathSprite_Play` / `_Hold`, `WhelpBreathFlames_Draw`, `WhelpBreathGlow_Draw`: W46, W47, W53..W60, W73, W74), each by a count in the function or functions its plant touches, as before. Thinnest: W58 171 rounds, W59 236. Selected: every control whose plant lies in one of the group's section-8 functions; the rest of the table plants outside them and stands without a re-run. The plants are the original round's own (its scratch script's anchors and edits, each anchor checked unique). Each: plant, rebuild (the file checked recompiled), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s27`, restore, rebuild; then the clean self-test, 0 mismatches, exit 0 (`BOF3X_SHADOW='*'`: exit 0). No fuzz change; no equivalent mutant among them.

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

Numbered D89, D90, D94, D97, D100 and D130 in [`known-defects.md`](known-defects.md).

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
