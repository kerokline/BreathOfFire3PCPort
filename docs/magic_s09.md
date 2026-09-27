# Group S09: Bone Dart, Firebreath / Icebreath, Dream Breath and Pollen / Venom Breath (MAGIC045, 046/047, 048, 050)

**Status:** IN PROGRESS (2026-09-26). All 47 functions are ours
(`src/game/magic_s09.cpp`, shadow name `magic_s09`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 94,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
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

- **Callees** (48 listed; the standard set supplies the rest):
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
  26's head and its source, `0x9039D8`, the ability word, the current-enemy
  pointer, `Field_Kind2Z` / `_Kind2X`, `ElemBreath_Motes` and record 255.
  20,012 bytes of state in 20 regions.
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

    shadow      magic_s09 self-test: 94000 rounds over 47 functions (2000 each), 1932950 calls to the stand-ins,
                0 MISMATCHES; 20012 bytes of state (20 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (1,933,886 stand-in
calls for this group in that run: the harness's pointers into the DLL move a
few branches; 0 mismatches).

## 6. Controls

CONTROLS_BODY

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
