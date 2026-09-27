# Group S09: Bone Dart, Firebreath / Icebreath, Dream Breath and Pollen / Venom Breath (MAGIC045, 046/047, 048, 050)

**Status:** IN PROGRESS (2026-09-26). All 47 functions are ours
(`src/game/magic_s09.cpp`, shadow name `magic_s09`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 94,000 rounds. 277 of 280 negative controls refused (exit 3; two of them by our abort, the rest by a count); two equivalent mutants and one plant the harness's flags cannot see, each with a near variant refused. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fourth spell wave, group S09
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 61 | 0x244 | MAGIC045 | 0x2D | Bone Dart | `0x4A9830..0x4A9FD8` | 11 |
| 62, 63 | 0x245, 0x246 | MAGIC046 / MAGIC047 (one copy of code) | 0x2E, 0x2F | Firebreath, Icebreath | `0x4A9FE0..0x4AAC36` | 16 |
| 41 | 0x247 | MAGIC048 | 0x30 | Dream Breath | `0x4AAC40..0x4AB3EB` | 10 |
| 31 | 0x249 | MAGIC050 | 0x32, 0x33 | Pollen, Venom Breath | `0x4AB3F0..0x4ABC95` | 10 |

The extents are `tools/magic_rows.py --unit MAGIC045 / MAGIC046/MAGIC047 /
MAGIC048 / MAGIC050 --clones` (capstone recursive descent; no jump table,
nothing `REFUSED`). All 47 lie in the units' extents; none was found inside
or missing from them, none was ours before. 8,956 bytes, as the queue
counted. Each `.data` table count was checked against the dump of
`0x65A930..0x65AA00` (read 2026-09-26): the tool's counts are right.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What each spell looks
like in play has not been measured. MAGIC046 and MAGIC047 are one body: the
linker kept one copy of the two files' identical code (queue §1's folding);
what differs between Firebreath and Icebreath is data, not these functions.
MAGIC050 is one body for its two abilities, and **reads which one it is**:
the ability word `0x904B80` (0x32, Pollen read one id down, one colour order;
anything else another) - the only place in the group where the ability id is
read.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Bone Dart (MAGIC045).**
  - `BoneDart_Task`: two-entry stack table by +1 (`BoneDart_Start`, S30's
    `MagicFx_EndWhenChildrenDone`).
  - `BoneDart_Start`: with the actor and the target on **one side** (both
    below 3 or both above 2) it only flags the target 0x40 and ends. Else two
    children of kind 1, parameter 0x31 - the dart (+1 0) and its shadow (+1 1,
    +0xB the dart's slot) - and the first sixteen words of CLUT row 26 back
    with their STP bits (the first word then plain); +0xB 2.
  - `BoneDartChild_Task` dispatches by +1 through `BoneDartChild_Kinds`;
    `BoneDartShaft_Run` / `BoneDartShadow_Run` swap the effects' frame-offset
    table into `0x9039D8` round a call through their four steps (the last
    MAGIC058's `0x4AF490`) and update the sprite while +0 and +2 are set.
  - The dart: `_Start` sets the sprite up at the owner, 0x100 above it, one
    step (0x10000) out along the owner's direction; `_Fly` ticks its script
    and steps toward the source sprite (`0x904B4C`, speed 0x30); within
    0x8000 of it: the round's flag 0x2000, the target flagged 0x40, sound
    0x203, the heading back to the owner (`Math_Ratan2`), a rise 0x40 with a
    pull of -8; `_Bounce` flies back along the heading for 16 frames, darker
    by 6 a frame, rising and falling.
  - The shadow: `_Start` at the owner's position, tint mode 2 at 0xC0;
    `_Follow` sits under the dart (the ground's height at its x / z) until the
    dart reaches step 2, `_Fade` darkens by 3 until step 3.
- **Firebreath / Icebreath (MAGIC046/047).**
  - `ElemBreath_Task`: two-entry stack table by +1 (`ElemBreath_Start`,
    `BattleFx_Finish`), then **a pool of its own** (`ElemBreath_Motes`, 48
    task-shaped records at `0x67DE40`): every record with bit 0 runs as
    `Sprite_Current` (its +0x80 the owner) through `ElemBreathMote_Run`, both
    cells put back after each.
  - `ElemBreath_Start`: the pool cleared; an emitter child (kind 1, 0x32);
    **in an event battle whose current enemy (`0x939AD8`) has +0x100 0x29** a
    second child (+1 1: `ElemBreathEnemy_*`, which makes the breath task
    itself `Sprite_Current`, plays its animation 2 through the engine's
    `0x43EC10`, and sets its +1 and +2 to 1); CLUT row 26's head back; sound
    0x100; +0xB 1.
  - The emitter (`ElemBreathEmitter_*`, MAGIC104's `MagicFx_EndWithChildren`
    last) takes a mote every other frame for 0x81 frames: +0x80 the emitter,
    +0xB its phase, +0xA its life (8, a frame less for each four past 0x64).
  - Each mote (`ElemBreathMote_*`): `_Start` centres on the side
    (`MagicFx_CenterOnSide`), keeps its angle from the emitter, and starts at
    an offset from it turned by the direction - **farther and higher for an
    acting enemy whose byte +0x8C is 0x10 or 0x72** (0x10000 / 0x800000,
    0x1C000 / 0x1000000; else 0x6000 / 0x400000); `_Flow` (16 frames) and
    `_Fade` (its life) drift it along a swaying heading and draw a textured
    gouraud quad (`_Draw`, tpage 0x340 / 0x100, CLUT row 0x1FA) that grows
    with +9; every frame a flare of eight gouraud triangles (`_DrawFlare`,
    layer 5) under the mote's matrix (`_PushMatrix`, the emitter's height).
  - `ElemBreathMote_Alloc`: the first free record's index in al, 0xFF when
    all 48 are taken.
- **Dream Breath (MAGIC048).**
  - `DreamBreath_Task`: three-entry stack table by +1 (`DreamBreath_Start`,
    MAGIC222's `0x4F7320`, `BattleFx_Finish`).
  - `DreamBreath_Start`: twelve motes (kind 1, 0x1E), each delayed
    `((i ^ 0xF) + 0x1D) << 3` frames (a byte).
  - Each mote (`DreamBreathMote_*`, through `DreamBreathMote_Steps`): `_Start`
    at the end of its delay takes the **mean height of the target side's
    actors still in** (`DreamBreath_TargetHeight`: the eight enemies for a
    target with bit 0x40, else the three party members), starts one step out
    from the owner and aims at a point 0x80000 out from the field's kind-2
    point; `_Fly` steps toward it (speed 0x10), brightening to 0x10, until
    `MagicFx_NearPoint3D` (0x60000); `_Land` fades out and frees. Each frame a
    band of 32 gouraud quads between two radii in the mote's y-z plane
    (`_Draw`), under a matrix turned toward the destination (`_PushMatrix`).
- **Pollen / Venom Breath (MAGIC050).**
  - `Pollen_Task`: three-entry stack table by +1 (`Pollen_Start`,
    `Pollen_Sounds`, `BattleFx_Finish`).
  - `Pollen_Start`: ten motes (kind 1, 4), delays from `Pollen_Delays`;
    `Pollen_Sounds` plays 0x101 at frames 8 and 0x18, 0x102 at 0x10, and ends
    at 0x19.
  - Each mote (`PollenMote_*`): `_Start` at the end of its delay stands at its
    pair of `PollenMote_Offsets` turned by the direction, from the field's
    kind-2 point (the first mote flags the target 0x10); step 1 is group C2's
    `HolocaustBeam_Grow`; `_Fade` frees it. Each frame, in screen space round
    its screen point: a fan of 16 gouraud triangles (`_DrawFan`), a ring of 16
    gouraud quads (`_DrawRing`) and 16 one-pixel tiles (`_DrawSparks`), their
    colour channels ordered by the ability word.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedents:

- a phase past any of the dispatch tables (seven stack tables, eight `.data`
  tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3);
- `DreamBreath_TargetHeight` aborts where the original's `idiv` faults: no
  actor of the target side in (the live-target divide the owner ruled on,
  [`takeover-queue-round9.md`](takeover-queue-round9.md) §7), and the one
  overflowing quotient (`INT_MIN / -1`, which its sums of shorts cannot
  reach).

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: `Gte_RotTrans` gets a flag pointer, the projections a
depth and a flag pointer. `MagicFx_StepTowardPoint`'s fourth argument, an
unset stack word in the original, is 0 in ours (the callee does not read it,
`symbols.toml`).

## 3. Calls to other units

By raw address (a phase in a table; never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4AF490` | MAGIC058 (S11, this wave) | entry 3 of `BoneDartShaft_Steps` and `BoneDartShadow_Steps`: the owner's +0xB down, the task freed |
| `0x4F7320` | MAGIC222 (S37, wave five) | entry 1 of `DreamBreath_Task` |
| `0x43EC10` | engine, unnamed | entry 1 of `ElemBreathEnemy_Task`: the script ticked and queued, +2 on at the done flag |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S22, S31 call it) |

By name or address, already ours: `MagicFx_EndWhenChildrenDone` (S30),
`MagicFx_EndWithChildren` (S24), `HolocaustBeam_Grow` (C2, entry 1 of
`PollenMote_Steps`), `BattleFx_Finish`, `BattleFx_FreeTask`, the effect
library (`MagicFx_CenterOnSide`, `_StepTowardPoint`, `_NearSprite`,
`_NearPoint3D`, `BattleActor_UpdateScreenXY`), `Battle_ActorIsOut`, and the
GTE / GPU / sprite / sound library.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `BoneDartChild_Kinds` | `0x65A948` | 2 |
| `BoneDartShaft_Steps` | `0x65A950` | 4 |
| `BoneDartShadow_Steps` | `0x65A960` | 4 |
| `ElemBreathChild_Kinds` | `0x65A970` | 2 |
| `DreamBreathMote_TaskTable` | `0x65A978` | 1 |
| `DreamBreathMote_Steps` | `0x65A97C` | 3 |
| `Pollen_Delays` | `0x65A988` | 10 bytes |
| `PollenMote_TaskTable` | `0x65A994` | 1 |
| `PollenMote_Offsets` | `0x65A998` | 10 dword pairs |
| `PollenMote_Steps` | `0x65A9E8` | 3 |
| `ElemBreath_Motes` | `0x67DE40` | 48 records of 0x84 bytes |

Ours reads `Pollen_Delays` and `PollenMote_Offsets` in place, as the
original does (no values are copied into the source).

## 5. The fuzz

`BOF3X_SHADOW=magic_s09` runs `magic_harness::Run` over the 47 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s09_fuzz.cpp`:

- **Callees** (47 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` log the whole
    0x800-byte packet buffer and move `Gfx_PacketNext` on as the real ones do
    (group S07's);
  - the projections log their SVECTORs through `deref`; the two matrix
    pushes' GTE callees log theirs and write a result where the real ones
    write (group S22's);
  - `0x446770` logs the task's direction and pair and writes a new pair;
  - the sprite calls log which sprite, `Sprite_UpdateScreen` the frame-offset
    table too; the dart's and shadow's steps are listed as `kPhase` with
    `0x9039D8`, so each logs the table the run swapped in;
  - `Math_Sin` / `Math_Cos` rewrite a scratch or vertex word a quarter of the
    time (S07's), so every re-read after a trig call is compared;
  - `Battle_ActorIsOut` (a `kFlag`) answers "in" for the last actor of
    `DreamBreath_TargetHeight`'s loop when none is in yet - the original's
    `idiv` would fault, and ours' abort is the precedent, not a fuzz target;
  - `ElemBreathMote_Alloc` answers 0..0x2F or 0xFF; the 0xFF record's writes
    (`0x6861BC`, "record 255") are a compared region;
  - `DreamBreath_TargetHeight`'s stand-in writes `Sprite_Current`'s +0x3E, so
    `DreamBreathMote_Start`'s read of it after the call is compared;
  - this group's functions called directly, by address (`kPhase`).
- **Tables:** the eight `.data` handler tables of section 4.
- **Regions** beyond the standard ones: the scratch `0x903850..0x90385F`,
  `Prim_VertexScratch`, `Gfx_PacketNext` and the packet buffer, CLUT row
  26's first 32 words and their source, `0x9039D8`, the ability word, the
  current-enemy pointer, `Field_Kind2Z` / `_Kind2X`, `ElemBreath_Motes` and
  record 255. 20,076 bytes of state in 20 regions.
- **Added after the first controls run** (section 6): `AreaMap_Elevation`
  answers the dart's height a third of the time (control B54), and the CLUT
  regions grew from 16 to 32 words (control B11).
- **Seed:** every pool record's owner a real slot or record, the pool full,
  part-full or random; the current enemy an enemy or sprite record with
  +0x100 0x29 two times in three, the event byte set two times in three; the
  ability word 0x32, 0x33, 0x32 under a high byte, or random; each dispatcher
  inside its table; each count one step before, at and past its end
  (`_Bounce` 1, `_Flow` 0x10, `_Emit` 0x64 / 0x80, `_Fly` 0x10, the lives 1,
  the sounds 8 / 0x10 / 0x18 / 0x19); the dart's slot inside the task table
  two times in three, else past it but inside the image, its step either side
  of 2 / 3; the acting enemy's +0x8C 0x10, 0x72 and neighbours; the target's
  bit 0x40 half the time for `DreamBreath_TargetHeight`.
- **Disturb** (the group's case): `Gfx_PacketNext`, a scratch word (the
  divisor dword `0x903854` only 1..8), a vertex word, a pool record's live
  bit or owner, the owner's count +0xB, the ability word, `0x9039D8`, the
  event byte or the current enemy's +0x100, `Field_Kind2*`.
- **Settle** (only for `BoneDartShadow_Follow` / `_Fade`): a task slot's +0xB
  of 155 or more goes back inside the table - both sides would fault reading
  past the image (section 8).

Result in this worktree (2026-09-26):

    shadow      magic_s09 self-test: 94000 rounds over 47 functions (2000 each), 1932868 calls to the stand-ins,
                0 MISMATCHES; 20076 bytes of state (20 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0, 2,891 ours, every group 0 mismatches (1,932,678 stand-in
calls for this group in that run: the harness's pointers into the DLL move a
few branches; 0 mismatches).

## 6. Controls

280 plants, each put in `magic_s09.cpp` one at a time by a script (not
committed; `controls.py` / `run_controls.py` in this session's scratchpad,
group S31's pattern) that planted, rebuilt, checked the build had recompiled
the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s09`, restored; after
the last it restored, rebuilt and ran the clean self-test (0 mismatches).
**277 of 280 refused**, every one by exit 3: all but two by a count only in the
functions the plant touches; D54 and D59 by our Fatal (a refusal by an
abort, which proves less than a count, but the plant changes which actors
are counted, and the harness keeps only the last one in) (the B-, E-, D-, P- controls are Bone Dart,
Firebreath / Icebreath, Dream Breath, Pollen). The kFlag-fixed harness
(2026-09-26) was in place from the start.

Not refused, three:

- **E21** (`_Emit`: the life's threshold 0x65 for 0x64) is an **equivalent
  mutant**: at +9 0x64 both branches give 8 (`(0x64 - 0x64) >> 2` is 0), and
  so for every +9 up to 0x67. Its near variant **E21b** (threshold 0x69,
  where 0x68 gives 7 in the original) was refused.
- **D63** (`DreamBreath_TargetHeight`: the whole of eax tested, not al) is
  **equivalent in C**: `Battle_ActorIsOut` is declared to answer an
  `unsigned char`, so ours tests al either way. **D63b** (`& 0x7F`) is not
  equivalent but **unobservable with the harness's answers**: a `kFlag`'s
  non-zero answer always has bit 4 set. Its variant **D63c** (`& 0x0F`) was
  refused.

Two controls of the first run needed the fuzz changed, then refused (the
table has the second run): **B11** (seventeen CLUT words) wrote the word
past the two 16-word CLUT regions - widened to 32 words each; **B54** (`<=`
for `<` in the shadow's height test) needs the ground and the dart at one
height, which garbage answers never give - `AreaMap_Elevation` now answers
the dart's height a third of the time. **P15**'s first plant did not build
(a slip in the script) and was re-planted.

The thinnest (fewer than 60 rounds):

- **B8** (Start: shadow owner read before the create): 41
- **E19** (Emit: task read before the alloc): 31
- **E82** (Flare: last edge read before the calls): 20
- **E88** (Alloc: 47 records): 12
- **D43** (Draw: last outer read before the calls): 27
- **P2** (Start: +0xB cleared after the centring): 30

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| B1 | BoneDart_Task: entries swapped | BoneDart_Task 2000 |
| B2 | Start: party side at 0..1 | BoneDart_Start 110 |
| B3 | Start: enemy side from 4 | BoneDart_Start 287 |
| B4 | Start: same side flags the actor | BoneDart_Start 737 |
| B5 | Start: done bit 3 | BoneDart_Start 640 |
| B6 | Start: parameter 0x32 | BoneDart_Start 1078 |
| B7 | Start: dart +1 1 | BoneDart_Start 1059 |
| B8 | Start: shadow owner read before the create | BoneDart_Start 41 |
| B9 | Start: shadow +0xB its own slot | BoneDart_Start 1042 |
| B10 | Start: +0xB 3 | BoneDart_Start 1078 |
| B11 | Row26: seventeen words | BoneDart_Start 1078, ElemBreath_Start 2000 |
| B12 | Row26: first word kept STP | BoneDart_Start 563, ElemBreath_Start 954 |
| B13 | Row26: dirty 2 | BoneDart_Start 1078, ElemBreath_Start 2000 |
| B14 | Child_Task: kinds swapped | BoneDartChild_Task 2000 |
| B15 | ChildRun: frame table not swapped | BoneDartShaft_Run 2000, BoneDartShadow_Run 2000 |
| B16 | ChildRun: update with +2 clear | BoneDartShaft_Run 261, BoneDartShadow_Run 268 |
| B17 | ChildRun: frame table put back before the update | BoneDartShaft_Run 794, BoneDartShadow_Run 758 |
| B18 | Shaft_Run: Fly / Bounce swapped | BoneDartShaft_Run 1005 |
| B19 | Shaft_Start: facing from the owner's +9 | BoneDartShaft_Start 1984 |
| B20 | Shaft_Start: step 0x10001 | BoneDartShaft_Start 1989 |
| B21 | Shaft_Start: z by the x step | BoneDartShaft_Start 2000 |
| B22 | Shaft_Start: height + 0xFF | BoneDartShaft_Start 2000 |
| B23 | Shaft_Start: +0x2B 0 | BoneDartShaft_Start 2000 |
| B24 | Shaft_Start: +0x27 0xA1 | BoneDartShaft_Start 2000 |
| B25 | Shaft_Start: scale y 0x10001 | BoneDartShaft_Start 2000 |
| B26 | Shaft_Start: animation 1 | BoneDartShaft_Start 2000 |
| B27 | Fly: no tick | BoneDartShaft_Fly 2000 |
| B28 | Fly: y + 0xC1 | BoneDartShaft_Fly 1003 |
| B29 | Fly: x sar 8 | BoneDartShaft_Fly 2000 |
| B30 | Fly: speed 0x31 | BoneDartShaft_Fly 2000 |
| B31 | Fly: near 0x8001 | BoneDartShaft_Fly 2000 |
| B32 | Fly: flag 0x1000 | BoneDartShaft_Fly 1240 |
| B33 | Fly: sound 0x204 | BoneDartShaft_Fly 1643 |
| B34 | Fly: +0x5C 2 | BoneDartShaft_Fly 1643 |
| B35 | Fly: Ratan2 arguments swapped | BoneDartShaft_Fly 1436 |
| B36 | Fly: +0x20 -7 | BoneDartShaft_Fly 1643 |
| B37 | Fly: +0x14 0x41 | BoneDartShaft_Fly 1643 |
| B38 | Fly: +9 0x11 | BoneDartShaft_Fly 1643 |
| B39 | Bounce: every frame | BoneDartShaft_Bounce 958 |
| B40 | Bounce: tint -5 | BoneDartShaft_Bounce 2000 |
| B41 | Bounce: sine x 4 | BoneDartShaft_Bounce 2000 |
| B42 | Bounce: x address after the call | BoneDartShaft_Bounce 66 |
| B43 | Bounce: cos of +0x10 | BoneDartShaft_Bounce 2000 |
| B44 | Bounce: no pull | BoneDartShaft_Bounce 2000 |
| B45 | Bounce: rise as a byte | BoneDartShaft_Bounce 1995 |
| B46 | Shadow_Run: Follow / Fade swapped | BoneDartShadow_Run 976 |
| B47 | Shadow_Start: height + 1 | BoneDartShadow_Start 2000 |
| B48 | Shadow_Start: +0x5C 1 | BoneDartShadow_Start 2000 |
| B49 | Shadow_Start: tint 0xC1 | BoneDartShadow_Start 2000 |
| B50 | Shadow_Start: no bit 5 | BoneDartShadow_Start 981 |
| B51 | Shadow_Start: +0x2B 1 | BoneDartShadow_Start 2000 |
| B52 | Follow: z from the slot's +0x34 | BoneDartShadow_Follow 1571, BoneDartShadow_Fade 1598 |
| B53 | Follow: elevation (z, x) | BoneDartShadow_Follow 1571, BoneDartShadow_Fade 1598 |
| B54 | Follow: <= compare | BoneDartShadow_Follow 403, BoneDartShadow_Fade 458 |
| B55 | Follow: unsigned compare | BoneDartShadow_Follow 790, BoneDartShadow_Fade 745 |
| B56 | Follow: only the height copied | BoneDartShadow_Follow 788, BoneDartShadow_Fade 760 |
| B57 | Follow: the dart's +1 tested | BoneDartShadow_Follow 372, BoneDartShadow_Fade 330 |
| B58 | Follow: step 3 | BoneDartShadow_Follow 638 |
| B59 | Follow: tick after | BoneDartShadow_Follow 2000 |
| B60 | Fade: step 2 | BoneDartShadow_Fade 679 |
| B61 | Fade: tint -4 | BoneDartShadow_Fade 2000 |
| B62 | Fade: even frames | BoneDartShadow_Fade 2000 |
| B63 | Follow: ground stored before the slot re-read | BoneDartShadow_Follow 1121, BoneDartShadow_Fade 1139 |
| E1 | Task: entries swapped | ElemBreath_Task 2000 |
| E2 | Task: self read before the phase | ElemBreath_Task 62 |
| E3 | Task: 47 records walked | ElemBreath_Task 1030 |
| E4 | Task: bit 1 tested | ElemBreath_Task 2000 |
| E5 | Task: owner not put back | ElemBreath_Task 1615 |
| E6 | Task: Sprite_Current not put back | ElemBreath_Task 1971 |
| E7 | Start: +2 not cleared | ElemBreath_Start 2000 |
| E8 | Start: parameter 0x33 | ElemBreath_Start 2000 |
| E9 | Start: event test dropped | ElemBreath_Start 470 |
| E10 | Start: enemy 0x28 | ElemBreath_Start 1023 |
| E11 | Start: enemy +0xFF | ElemBreath_Start 933 |
| E12 | Start: second +1 0 | ElemBreath_Start 933 |
| E13 | Start: sound 0x101 | ElemBreath_Start 2000 |
| E14 | Start: +0xB 2 | ElemBreath_Start 2000 |
| E15 | Child_Task: kinds swapped | ElemBreathChild_Task 2000 |
| E16 | Emitter_Task: steps 0/1 swapped | ElemBreathEmitter_Task 1357 |
| E17 | Emitter_Start: +9 1 | ElemBreathEmitter_Start 2000 |
| E18 | Emit: even frames | ElemBreathEmitter_Emit 2000 |
| E19 | Emit: task read before the alloc | ElemBreathEmitter_Emit 31 |
| E20 | Emit: mote +0xB our +0xB | ElemBreathEmitter_Emit 1008 |
| E21 | Emit: life from 0x65 | not refused: equivalent (below) |
| E22 | Emit: life shr 3 | ElemBreathEmitter_Emit 518 |
| E23 | Emit: life 9 | ElemBreathEmitter_Emit 303 |
| E24 | Emit: count not kept | ElemBreathEmitter_Emit 1014 |
| E25 | Emit: on at 0x80 | ElemBreathEmitter_Emit 121 |
| E26 | Emit: owner the record's own | ElemBreathEmitter_Emit 1014 |
| E27 | Enemy_Task: steps 1/2 swapped | ElemBreathEnemy_Task 1361 |
| E28 | Enemy_Start: Sprite_Current kept | ElemBreathEnemy_Start 1723 |
| E29 | Enemy_Start: animation 3 | ElemBreathEnemy_Start 2000 |
| E30 | Enemy_Start: +2 2 | ElemBreathEnemy_Start 2000 |
| E31 | Mote_Run: Flow / Fade swapped | ElemBreathMote_Run 1335 |
| E32 | Mote_Run: drawn with +0 clear | ElemBreathMote_Run 960 |
| E33 | Mote_Run: flare before matrix | ElemBreathMote_Run 1040 |
| E34 | Mote_Start: no centring | ElemBreathMote_Start 2000 |
| E35 | Mote_Start: Ratan2 swapped | ElemBreathMote_Start 1768 |
| E36 | Mote_Start: facing from +9 | ElemBreathMote_Start 1981 |
| E37 | Mote_Start: kind +0x8D | ElemBreathMote_Start 408 |
| E38 | Mote_Start: by the target | ElemBreathMote_Start 372 |
| E39 | Mote_Start: kind 0x11 | ElemBreathMote_Start 390 |
| E40 | Mote_Start: kind 0x73 | ElemBreathMote_Start 373 |
| E41 | Mote_Start: reach 0x6001 | ElemBreathMote_Start 1589 |
| E42 | Mote_Start: lift 0x1000001 | ElemBreathMote_Start 197 |
| E43 | Mote_Start: lift 0x800001 | ElemBreathMote_Start 200 |
| E44 | Mote_Start: reach 0x1C001 | ElemBreathMote_Start 197 |
| E45 | Mote_Start: +0x3C without the lift | ElemBreathMote_Start 2000 |
| E46 | Mote_Start: angle and 0x7FF | ElemBreathMote_Start 996 |
| E47 | Mote_Start: +9 1 | ElemBreathMote_Start 2000 |
| E48 | Drift: sway & 0x1F | ElemBreathMote_Flow 1011, ElemBreathMote_Fade 999 |
| E49 | Drift: sway word not kept | ElemBreathMote_Flow 1922, ElemBreathMote_Fade 1936 |
| E50 | Drift: x 301 | ElemBreathMote_Flow 2000, ElemBreathMote_Fade 2000 |
| E51 | Drift: heading without +0x14 | ElemBreathMote_Flow 2000, ElemBreathMote_Fade 2000 |
| E52 | Drift: sway x 8 | ElemBreathMote_Flow 2000, ElemBreathMote_Fade 2000 |
| E53 | Drift: sway not masked | ElemBreathMote_Flow 2000, ElemBreathMote_Fade 2000 |
| E54 | Drift: z address after the call | ElemBreathMote_Flow 61, ElemBreathMote_Fade 64 |
| E55 | Drift: no screen point | ElemBreathMote_Flow 2000, ElemBreathMote_Fade 2000 |
| E56 | Flow: on at 0x11 | ElemBreathMote_Flow 828 |
| E57 | Fade: +9 not counted | ElemBreathMote_Fade 2000 |
| E58 | Fade: four bytes cleared | ElemBreathMote_Fade 533 |
| E59 | Fade: owner's +0xA | ElemBreathMote_Fade 535 |
| E60 | Draw: tpage 0x36 | ElemBreathMote_Draw 2000 |
| E61 | Draw: radius below 9 | ElemBreathMote_Draw 378 |
| E62 | Draw: radius x 5 | ElemBreathMote_Draw 391 |
| E63 | Draw: radius + 0x21 | ElemBreathMote_Draw 1583 |
| E64 | Draw: shade x 21 | ElemBreathMote_Draw 1835 |
| E65 | Draw: centre y from +0x2E | ElemBreathMote_Draw 1957 |
| E66 | Draw: corners 1/2 swapped | ElemBreathMote_Draw 2000 |
| E67 | Draw: y at the x centre | ElemBreathMote_Draw 2000 |
| E68 | Draw: tpage abr 2 | ElemBreathMote_Draw 2000 |
| E69 | Draw: CLUT row 0x1FB | ElemBreathMote_Draw 2000 |
| E70 | Draw: uv 0x1E | ElemBreathMote_Draw 2000 |
| E71 | Draw: size 0x50 | ElemBreathMote_Draw 2000 |
| E72 | Draw: one colour missed | ElemBreathMote_Draw 1989 |
| E73 | PushMatrix: height from the mote | ElemBreathMote_PushMatrix 1766 |
| E74 | PushMatrix: height / 2 by shift | ElemBreathMote_PushMatrix 517 |
| E75 | PushMatrix: rotation 1 | ElemBreathMote_PushMatrix 2000 |
| E76 | PushMatrix: z - 0x3FFF | ElemBreathMote_PushMatrix 2000 |
| E77 | Flare: tpage 0x56 | ElemBreathMote_DrawFlare 2000 |
| E78 | Flare: layer 4 | ElemBreathMote_DrawFlare 2000 |
| E79 | Flare: radius + 5 | ElemBreathMote_DrawFlare 1972 |
| E80 | Flare: shade x 7 | ElemBreathMote_DrawFlare 1911 |
| E81 | Flare: seven triangles | ElemBreathMote_DrawFlare 2000 |
| E82 | Flare: last edge read before the calls | ElemBreathMote_DrawFlare 20 |
| E83 | Flare: origin z not cleared | ElemBreathMote_DrawFlare 2000 |
| E84 | Flare: rim shade 2 | ElemBreathMote_DrawFlare 2000 |
| E85 | Flare: size 0x30 | ElemBreathMote_DrawFlare 2000 |
| E86 | Flare: closing tpage 0x16 | ElemBreathMote_DrawFlare 2000 |
| E87 | Flare: no depths | ElemBreathMote_DrawFlare 2000 |
| E88 | Alloc: 47 records | ElemBreathMote_Alloc 12 |
| E89 | Alloc: bit not taken | ElemBreathMote_Alloc 1528 |
| E90 | Alloc: none 0xFE | ElemBreathMote_Alloc 472 |
| D1 | Task: entries 1/2 swapped | DreamBreath_Task 1350 |
| D2 | Start: +9 0xB | DreamBreath_Start 1448 |
| D3 | Start: eleven motes | DreamBreath_Start 2000 |
| D4 | Start: parameter 0x1F | DreamBreath_Start 2000 |
| D5 | Start: delay xor 0xE | DreamBreath_Start 2000 |
| D6 | Start: delay + 0x1E | DreamBreath_Start 2000 |
| D7 | Start: delay not a byte before the shift | DreamBreath_Start 2000 |
| D8 | Start: mote +8 the owner's | DreamBreath_Start 1085 |
| D9 | Start: +0xB not cleared | DreamBreath_Start 1439 |
| D10 | Start: owner read before the create | DreamBreath_Start 619 |
| D11 | Mote_Run: Fly / Land swapped | DreamBreathMote_Run 1378 |
| D12 | Mote_Run: drawn at step 0 | DreamBreathMote_Run 309 |
| D13 | Mote_Run: no pop | DreamBreathMote_Run 682 |
| D14 | Mote_Start: at 1 | DreamBreathMote_Start 1325 |
| D15 | Mote_Start: no target height | DreamBreathMote_Start 672 |
| D16 | Mote_Start: reach 0x10001 | DreamBreathMote_Start 668 |
| D17 | Mote_Start: destination 0x80001 | DreamBreathMote_Start 669 |
| D18 | Mote_Start: destination x from Kind2Z | DreamBreathMote_Start 672 |
| D19 | Mote_Start: +0x14 + 0xFF | DreamBreathMote_Start 672 |
| D20 | Mote_Start: +0x14 unsigned | DreamBreathMote_Start 354 |
| D21 | Mote_Start: height set before +0x14 | DreamBreathMote_Start 672 |
| D22 | Mote_Start: +0xA 1 | DreamBreathMote_Start 672 |
| D23 | Step: y sar 2 | DreamBreathMote_Fly 2000, DreamBreathMote_Land 2000 |
| D24 | Step: speed 0x11 | DreamBreathMote_Fly 2000, DreamBreathMote_Land 2000 |
| D25 | Step: z - 0x3FFF | DreamBreathMote_Fly 2000, DreamBreathMote_Land 2000 |
| D26 | Fly: +9 not counted | DreamBreathMote_Fly 1980 |
| D27 | Fly: shade to 0x11 | DreamBreathMote_Fly 833 |
| D28 | Fly: near 0x60001 | DreamBreathMote_Fly 2000 |
| D29 | Fly: near the position | DreamBreathMote_Fly 2000 |
| D30 | Fly: al tested | DreamBreathMote_Fly 317 |
| D31 | Land: shade up | DreamBreathMote_Land 2000 |
| D32 | Land: count not kept | DreamBreathMote_Land 601 |
| D33 | Draw: inner x 3 | DreamBreathMote_Draw 1932 |
| D34 | Draw: outer x 7 | DreamBreathMote_Draw 1971 |
| D35 | Draw: jitter from 8 | DreamBreathMote_Draw 420 |
| D36 | Draw: jitter & 0x1F | DreamBreathMote_Draw 582 |
| D37 | Draw: shade x 9 | DreamBreathMote_Draw 1806 |
| D38 | Draw: shade x 5 | DreamBreathMote_Draw 1831 |
| D39 | Draw: 31 quads | DreamBreathMote_Draw 2000 |
| D40 | Draw: first outer by cos 0x80 | DreamBreathMote_Draw 2000 |
| D41 | Draw: outer at the inner radius | DreamBreathMote_Draw 2000 |
| D42 | Draw: inner last read before the store | DreamBreathMote_Draw 2000 |
| D43 | Draw: last outer read before the calls | DreamBreathMote_Draw 27 |
| D44 | Draw: third corner the inner shade | DreamBreathMote_Draw 1998 |
| D45 | Draw: near corners 2 | DreamBreathMote_Draw 2000 |
| D46 | Draw: vertex order | DreamBreathMote_Draw 2000 |
| D47 | Draw: size 0x40 | DreamBreathMote_Draw 2000 |
| D48 | PushMatrix: Ratan2 swapped | DreamBreathMote_PushMatrix 2000 |
| D49 | PushMatrix: angle & 0x7FF | DreamBreathMote_PushMatrix 1027 |
| D50 | PushMatrix: turn about y | DreamBreathMote_PushMatrix 2000 |
| D51 | PushMatrix: height from the owner | DreamBreathMote_PushMatrix 1766 |
| D52 | PushMatrix: dx from the destination alone | DreamBreathMote_PushMatrix 2000 |
| D53 | Height: enemies by bit 0x80 | DreamBreath_TargetHeight 1005 |
| D54 | Height: seven enemies | by a Fatal: ours aborts (no actor in - the plant skips the last actor, the one the stand-in keeps in) |
| D55 | Height: enemy index i + 2 | DreamBreath_TargetHeight 1005 |
| D56 | Height: enemy height +0x3C | DreamBreath_TargetHeight 1003 |
| D57 | Height: party height unsigned | DreamBreath_TargetHeight 553 |
| D58 | Height: party count by 2 | DreamBreath_TargetHeight 993 |
| D59 | Height: two party members | by a Fatal: ours aborts (no actor in - the plant skips the last actor, the one the stand-in keeps in) |
| D60 | Height: party height +0x3C | DreamBreath_TargetHeight 994 |
| D61 | Height: count not cleared | DreamBreath_TargetHeight 1991 |
| D62 | Height: unsigned divide | DreamBreath_TargetHeight 414 |
| D63 | Height: out tested in full | not refused: equivalent (below) |
| P1 | Task: Start / Sounds swapped | Pollen_Task 1327 |
| P2 | Start: +0xB cleared after the centring | Pollen_Start 30 |
| P3 | Start: height the owner's too | Pollen_Start 1766 |
| P4 | Start: nine motes | Pollen_Start 2000 |
| P5 | Start: parameter 5 | Pollen_Start 2000 |
| P6 | Start: delay index + 1 | Pollen_Start 2000 |
| P7 | Start: delay + 2 | Pollen_Start 2000 |
| P8 | Start: mote height not copied | Pollen_Start 2000 |
| P9 | Start: sound 0x101 | Pollen_Start 2000 |
| P10 | Start: mote +0xB not i | Pollen_Start 664 |
| P11 | Sounds: 0x101 at 9 | Pollen_Sounds 334 |
| P12 | Sounds: 0x102 at 0x11 | Pollen_Sounds 297 |
| P13 | Sounds: 0x103 | Pollen_Sounds 134 |
| P14 | Sounds: on at 0x18 | Pollen_Sounds 317 |
| P15 | Sounds: +2 on at 0x19 | Pollen_Sounds 152 |
| E21b | Emit: life from 0x69 (E21's near variant) | ElemBreathEmitter_Emit 70 |
| D63b | Height: out tested by its low seven bits (D63's near variant) | not refused: unobservable with the harness's flags (below) |
| P16 | Mote_Run: steps 0/2 swapped | PollenMote_Run 1332 |
| P17 | Mote_Run: ring before fan | PollenMote_Run 684 |
| P18 | Mote_Run: drawn at step 0 | PollenMote_Run 329 |
| P19 | Mote_Start: pair index + 1 | PollenMote_Start 642 |
| P20 | Mote_Start: dz from dx | PollenMote_Start 645 |
| P21 | Mote_Start: z from Kind2X | PollenMote_Start 652 |
| P22 | Mote_Start: flags from the second | PollenMote_Start 315 |
| P23 | Mote_Start: flags 0x20 | PollenMote_Start 314 |
| P24 | Mote_Start: +9 0x21 | PollenMote_Start 652 |
| P25 | Mote_Start: +0xA 3 | PollenMote_Start 652 |
| P26 | Mote_Start: at 1 | PollenMote_Start 1313 |
| P27 | Mote_Fade: +0xA down | PollenMote_Fade 1994 |
| P28 | Mote_Fade: count not kept | PollenMote_Fade 655 |
| P29 | Shades: 0x33 | PollenMote_DrawFan 1237, PollenMote_DrawRing 1352, PollenMote_DrawSparks 1177 |
| P30 | Shades: a byte test | PollenMote_DrawFan 514, PollenMote_DrawRing 516, PollenMote_DrawSparks 496 |
| P31 | Point: x from +0x30 | PollenMote_DrawFan 2000, PollenMote_DrawRing 2000, PollenMote_DrawSparks 2000 |
| P32 | Point: sar 11 | PollenMote_DrawFan 1999, PollenMote_DrawRing 2000, PollenMote_DrawSparks 1998 |
| P33 | Point: radius read before the call | PollenMote_DrawFan 1047, PollenMote_DrawRing 1485, PollenMote_DrawSparks 585 |
| P34 | Fan: radius +0xA + 1 | PollenMote_DrawFan 1973 |
| P35 | Fan: shade x 4 | PollenMote_DrawFan 478 |
| P36 | Fan: shade x 6 | PollenMote_DrawFan 1438 |
| P37 | Fan: fifteen triangles | PollenMote_DrawFan 2000 |
| P38 | Fan: step 0x80 | PollenMote_DrawFan 2000 |
| P39 | Fan: centre y from +0x2E | PollenMote_DrawFan 2000 |
| P40 | Fan: Pollen's first colour | PollenMote_DrawFan 872 |
| P41 | Fan: Venom's green 2 | PollenMote_DrawFan 1620 |
| P42 | Fan: Venom's last green | PollenMote_DrawFan 1615 |
| P43 | Fan: size 0x30 | PollenMote_DrawFan 2000 |
| P44 | Fan: layer 3 | ElemBreathMote_Draw 2000, DreamBreathMote_Draw 2000, PollenMote_DrawFan 2000, PollenMote_DrawRing 2000, PollenMote_DrawSparks 2000 |
| P45 | Ring: inner x 2 | PollenMote_DrawRing 1876 |
| P46 | Ring: shade x 5 | PollenMote_DrawRing 461 |
| P47 | Ring: shade x 7 | PollenMote_DrawRing 1356 |
| P48 | Ring: inner at the outer radius | PollenMote_DrawRing 2000 |
| P49 | Ring: inner end at the start | PollenMote_DrawRing 2000 |
| P50 | Ring: Pollen's middle | PollenMote_DrawRing 1026 |
| P51 | Ring: Venom's first | PollenMote_DrawRing 1631 |
| P52 | Ring: inner 2 | PollenMote_DrawRing 2000 |
| P53 | Ring: size 0x48 | PollenMote_DrawRing 2000 |
| P54 | Ring: fifteen quads | PollenMote_DrawRing 2000 |
| P55 | Sparks: radius x 2 | PollenMote_DrawSparks 1976 |
| P56 | Sparks: colour x 5 | PollenMote_DrawSparks 1937 |
| P57 | Sparks: Pollen's order swapped | PollenMote_DrawSparks 810 |
| P58 | Sparks: size 0x10 | PollenMote_DrawSparks 2000 |
| P59 | Sparks: tile at step 0x200 | PollenMote_DrawSparks 2000 |
| P60 | Sparks: first colour +0x5C | PollenMote_DrawSparks 1996 |
| P61 | Draws: tpage 0x34 | ElemBreathMote_Draw 2000, ElemBreathMote_DrawFlare 2000, DreamBreathMote_Draw 2000, PollenMote_DrawFan 2000, PollenMote_DrawRing 2000, PollenMote_DrawSparks 2000 |
| D63c | Height: out tested by its low four bits (D63's near variant) | DreamBreath_TargetHeight 110 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is the
owner casting them, with a save that has them or DIV-0045's cheat. Things to
look for:

- Bone Dart: a dart from the caster to the target and back, a shadow under
  it on the ground; nothing but the hit when the caster and the target are
  on one side.
- Firebreath / Icebreath: a stream of textured quads and flares from the
  caster; for two kinds of enemy (+0x8C 0x10, 0x72) the stream starts
  farther and higher.
- Dream Breath: twelve bands flying to points over the target side.
- Pollen / Venom Breath: ten motes round the field's point, each a fan, a
  ring and sparkles; the two abilities in different colours.

`ElemBreath_Start`'s second child is reached only in an event battle whose
current enemy's +0x100 is 0x29 (the same test S31's Corona makes); which boss
that is was not read. Which enemies have +0x8C 0x10 / 0x72 was not read.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: seven stack tables and eight
  `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `BoneDart_Start` (two calls), `ElemBreath_Start` (two), `DreamBreath_Start`
  (twelve) and `Pollen_Start` (ten): slot 255 lies past the image, an access
  violation in ours as in the original.
- **`ElemBreathMote_Alloc`'s 0xFF is unchecked** by `ElemBreathEmitter_Emit`:
  with the 48 records taken it writes "record 255", `0x6861BC` (+0x80,
  +0xA, +0xB) - inside `.data`, so no fault: a silent write over whatever
  lives there. Ours writes the same bytes. Whether 48 can be live at once is
  not measured (the emitter makes one every other frame for 0x81 frames, 65
  in all, each living 8 frames or fewer at its last step - by reading, far
  fewer than 48 at once).
- **`BoneDartShadow_Follow` / `_Fade` index the task slots by +0xB,
  unchecked**: +0xB is set to the dart's slot by `BoneDart_Start`, from an
  unchecked create (above).
- **`ElemBreathMote_Start` reads the acting enemy's +0x8C by the actor byte
  - 3, unchecked**: for a party actor it reads a byte among the task slots.
- **`DreamBreath_TargetHeight` divides by the count of live actors**: none in
  faults (ours aborts, section 2).
- **`ElemBreathEnemy_Start` leaves `Sprite_Current` at the breath task**,
  not put back: the runner's loop then sees the parent as the current task
  for the rest of the step (`BattleTask_RunAll` resets it per slot, by
  reading `battle_flow.md`, not measured). It also zeroes the breath task's
  count +0xB and sets its +1 to 1 (`BattleFx_Finish`, which ends a task
  whose +0xB is 0): so, by reading, in that event battle the breath ends
  (the done flag, the task freed) the frame its second child starts, while
  the emitter and its motes run on, and the emitter's own end
  (`MagicFx_EndWithChildren`) later counts down the freed task's +0xB.
  Not measured; the count +0xB `ElemBreath_Start` sets is 1 with or without
  the second child.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 41 lines under a `group S09` comment: 37 new,
plus four host extents re-listed smaller (`004AA360 48`, `004AABE0 57`,
`004AB330 BC`, `004AB8E0 28B`, each the function's own size). Six were listed
right already (`004AA6D0`, `004AA980`, `004AAA30`, `004AAFD0`, `004AB250`,
`004AB6C0`).
