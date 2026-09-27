# Group S04: Snap, Charge, Flying Kick and Air Raid (MAGIC013, MAGIC015 with MAGIC016 folded)

**Status:** IN PROGRESS (2026-09-26). All 56 functions are ours
(`src/game/magic_s04.cpp`, shadow name `magic_s04`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 112,000 rounds. 307 of 307 negative controls refused, 306 by a count and one by a fault (its variant by a count). Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S04
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Entry | Extent |
|--:|---|---|---|---|---|---|
| 50 | 0x234 | MAGIC013 | 0xD | Snap | `Snap_Task` 0x49E9F0 | `0x49E9F0..0x49FA66` (20 functions) |
| 4 | 0x235 | MAGIC015 | 0xF | Charge | `Charge_Task` 0x49FA70 | `0x49FA70..0x4A11D8` (36 functions) |
| 7 | 0x235 | MAGIC015 | - (no ability loads it) | - | `AirRaid_Task` 0x49FCF0 | (the same) |
| 55 | 0x235 | MAGIC015 | 0xE | Flying Kick | `FlyingKick_Task` 0x49FEE0 | (the same) |
| 58 | 0x236 | MAGIC016, folded into MAGIC015 | 0x10 | Air Raid | `AirRaid_Task` 0x49FCF0 | (the same) |

The extents are `tools/magic_rows.py --unit MAGIC013 / MAGIC015 --clones`
(capstone recursive descent). Two jump tables, one in each wave matrix
(`SnapWave_PushMatrixA` 0x49EFE8, `SnapWave_PushMatrixB` 0x49F104), each of
four entries, bounded by the `cmp ecx, 3` before its dispatch - the tool's
count and the `cmp` agree; the harness moves them into the copy. Nothing
`REFUSED`. All 56 functions lie in the units' extents; none was found inside
or missing from them, and none was ours before. 9,776 bytes, as the queue
counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses; row 58's MAGIC016
entry is row 7's, so "Air Raid" names the code rows 7 and 58 share. What
each spell looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Snap (MAGIC013).**
  - `Snap_Task`, the kind-2 task: a four-entry stack table by +1
    (`Snap_Start`, MAGIC167's `0x4EF7C0` - +1 on once +0xB is 1 or less -,
    `Snap_Buff`, `MagicFx_EndWhenChildrenDone`), then every live record of
    `SnapSpark_Pool` (64 records of 0x84 bytes at `0x677A40`, the overlay's
    own `.bss`) run through `SnapSpark_Task` as `Sprite_Current` with its
    +0x80 as the owner cell, both put back after each (C1's InkInk pool is the
    same shape).
  - `Snap_Start` clears the pool's +0..+2, puts the task on the source sprite,
    copies CLUT row 26's first two CLUTs back from their source (no STP bit),
    makes the wave (kind 1, parameter 0x29) and sets its own +0xB to 2.
  - `Snap_Buff`: when `Battle_ActorIsOut(target)` says no and
    `MagicFx_ApplyBuff(1, target)` says yes, a child of kind 1, parameter 0x48
    (another overlay's), with +4 4, +9 0x18, +0xA 4.
  - The wave (`SnapWave_*`, five steps through `SnapWave_Steps`): `_Start`
    puts it 0x20000 out from the owner along its facing (the engine's
    `0x446770` turn), 0x1000000 up, with a step of -0x6000; step 1 is group
    S27's `DragonBreathBeam_Brighten` (+4 up by 2 to 0x10: the rings' shade, +4 x 12, brightens);
    `_Burst` moves it on until +9 has counted down by 4 to 0, then throws up to
    32 sparks from the pool, flags the target 0x10 on the first swing, turns
    the step to 0x3000 and plays sound 0x100; `_Swing` moves it back until +9
    has counted up to 0x10 (0xC on the last), then either turns round to
    `_Burst` again or, after four swings, flags the target 0x40, plays 0x201
    and counts the owner down; `_End` waits for its sparks, then frees itself.
    Each frame while +0 is set, two rings of sixteen semi-transparent gouraud
    quads, each under its own matrix tipped by +9 about x or y by the facing
    (`_PushMatrixA` / `_B`, jump tables), their radius swelling with a sine
    (`SnapWave_FacingPhase[facing]` the first angle step), the lower ring's
    second edge 0x50 below its first and the upper's 0x50 above, shaded +4 x
    12 at the far edge.
  - A spark (`SnapSpark_*`, two steps through `SnapSpark_Steps`): `_Launch`
    waits +9 frames, then starts on the source sprite up to 0x800 out along its
    angle (+0xB x 0x80), with a rise of 0x80..0x87 falling by 12; `_Fly` moves out by
    up to 0x2000 a frame and up by the rise for 16 frames, then counts the wave down
    and frees the record. Drawn as one flat triangle of radius 16 whose colour
    is three `(Rand & 0xF) x +9` bytes. `SnapSpark_Alloc` takes the first free
    record (0xFF when all 64 are live, which `SnapWave_Burst` tests).
- **Charge, Air Raid, Flying Kick (MAGIC015).** Each kind-2 task hides its
  owner (+0 bit 0x40), takes its palette into the FX row
  (`SpriteClut_CopyToFxRow`) and makes **images**: kind-1 children of
  parameter 0x23 that are copies of the acting actor's record's first 0x80
  bytes (party `0x802D40 + 0x14C i` below 3, else enemy
  `0x93B960 + 0x128 (i - 3)`, unchecked). `KickImage_Task` dispatches an image
  by +1 through `KickImage_Kinds`.
  - `Charge_Task` (row 4): `Charge_Start` plays the owner's animation 8,
    makes four trails (+1 1, +9 4..1 frames of delay, +0xB 3..0) and one
    dashing image (+1 0); entry 1 is MAGIC018/019's `0x4A1EC0` (at +0xB 0 the
    FX row back, animation 4, the owner shown). The dashing image
    (`ChargeImage_*`): on the owner's point, the script ticked (a party actor
    only), `_Dash` steps toward the source sprite (0x60) until within 0xC000,
    then the hit (a party member's cry, sound 0x203, target flag 0x40) and a
    turn back at the owner (`Math_Ratan2`); `KickImage_Arc` back along that
    heading with a rise and fall for 16 frames; `_Wait` for the trails;
    `_Squash` and `_Stretch` (scale +0x40 / +0x44) for 8 frames each; freed.
    The trails (`ChargeTrail_*`) do the same without the hit, shaded darker
    by +0xB, and end through MAGIC058's `0x4AF490`.
  - `AirRaid_Task` (rows 7 and 58): `AirRaid_Start` makes three images (+0xB
    0..2, +9 1, 3, 5 frames of delay) and plays sound 0x100. Each
    (`AirRaidImage_*`): `_Start` (the later two shaded 0xC0), `_Rise` round the
    source sprite (0x1800) for 16 frames, narrowing, then over it 0xC00 up;
    `_Turn` (4 frames; the scale 0x8000 x 0x18000; the shades (0xFD - n,
    0xFC - n, 0xFC - n) x 16 for the later ones, (0x30, 0xC0, 0xC0) for the
    first); `_Dive` 0xC0 a frame for 20 frames, the first image's hit;
    `_Bounce` (the later images step their shades by `AirRaidImage_ShadeSteps`
    and are freed when they fall below the source; the first unshades and goes
    on); `KickImage_Settle` (shade back to 0x80, then once the owner's +0xB
    is 1 and the script ends, back on the owner); `_FadeOut` (freed at 0xC0).
    While in the air (+9 set, the first image only, steps below 5) a shadow.
  - `FlyingKick_Task` (row 55): `FlyingKick_Start` makes one image (+1 3);
    `FlyingKick_End` is `0x4A1EC0`'s work with the owner's bit cleared before
    the animation. The image (`FlyingKickImage_*`): `_Start` (rise 0x80
    falling by 16), `_Rise` round the source (0x3800) for 7 frames, `_Dive`
    toward it until within 0xC000 in 3D (`MagicFx_NearSprite3D`) and the hit,
    `_Bounce` until it falls below the source, `KickImage_Settle`, `_FadeOut`,
    `BattleFx_FreeTask`. A shadow below step 4.
  - The shadow: `KickImage_PushMatrixGround` (on the ground under the image,
    `AreaMap_Elevation`) or `KickImage_PushMatrixSource` (at the source
    sprite's height), then `KickImage_DrawShadow`: eight semi-transparent
    gouraud triangles round a disc of radius +9 x 4, centre 0x80 grey, rim 1,
    under a subtractive draw mode (tpage 0x55), committed to slot 5.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedents:

- a phase past any of the eleven dispatch tables (six stack tables, five
  `.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3);
- `SnapWave_PushMatrixA` / `_B` abort on a facing byte past 3, where the
  original's four-entry jump table leaves two of the three angles as
  uninitialised stack words (group S31's `CoronaRay_PushMatrix`, group S25's
  `SpellConfuse_PushFacingMatrix`). The wave's +8 is written only by
  `SnapWave_Start`, from the owner's +8; whether a live owner's facing can be
  past 3 is not measured.

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: `Gte_RotTrans` a flag pointer, the projections a depth
and a flag pointer.

## 3. Calls to other units

By raw address (a phase in a stack table, or a callee; never bound or renamed
here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4EF7C0` | MAGIC167 (S35, wave five) | entry 1 of `Snap_Task`: +1 on once +0xB is 1 or less |
| `0x4A1EC0` | MAGIC018/019 (S05, this wave) | entry 1 of `Charge_Task` and `AirRaid_Task`: at +0xB 0 `SpriteClut_RestoreFxRow`, animation 4, the owner's bit 0x40 cleared, +1 on |
| `0x4AF490` | MAGIC058 (S11, wave four) | entry 4 of `ChargeTrail_Run`: the owner's +0xB down, the task freed |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S22, S23, S31 call it) |
| `0x5A7570` | libgpu, unnamed (SetPolyF3 by shape: code 0x20, three `0x3C23D70A` floats) | `SnapSpark_Draw`'s triangle |

By name, already ours: `MagicFx_EndWhenChildrenDone` (S30),
`MagicFx_DoneAndFree` and `MagicFx_FlagTargetEnd` (E), `BattleFx_FreeTask`,
`DragonBreathBeam_Brighten` (S27, entry 1 of `SnapWave_Steps`, read from the
table), the effect library (`MagicFx_ApplyBuff`, `_StepToward`,
`_StepAround`, `_NearSprite`, `_NearSprite3D`, `SpriteClut_*`,
`BattleActor_SetAnimation`, `_PlaySound`), and the GTE / GPU / sprite / sound
library.

Shared bodies: `magic_funcs.tsv` has MAGIC012 (S03) reaching the wave's
bodies (`0x49EBE0..0x49F3A0`, `0x49FA10`), MAGIC013 and MAGIC016 reaching
MAGIC015's images, MAGIC001 (S01) reaching `AirRaidImage_FadeOut`, and seven
files reaching `KickImage_Tick`. They reach them through the addresses, so
taking them changes nothing for the callers.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `SnapWave_TaskTable` | `0x65A660` | 1 |
| `SnapWave_Steps` | `0x65A664` | 5 |
| `SnapWave_FacingPhase` | `0x65A678` | 4 bytes |
| `SnapSpark_TaskTable` | `0x65A67C` | 1 |
| `SnapSpark_Steps` | `0x65A680` | 2 |
| `KickImage_Kinds` | `0x65A688` | 4 |
| `AirRaidImage_ShadeSteps` | `0x65A698` | 4 bytes (pairs for +0xB 1, 2; read as `0x65A696 + 2 +0xB`) |
| `SnapSpark_Pool` | `0x677A40` | 64 x 0x84 bytes |

Each table's count is where the next starts (the dump of `0x65A650..0x65A6BC`
read 2026-09-26); the tool's "6 / 5 / 7 / 6 / 4 code entries" notes count
every code pointer that follows.

## 5. The fuzz

`BOF3X_SHADOW=magic_s04` runs `magic_harness::Run` over the 56 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s04_fuzz.cpp`:

- **Callees** (42 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` log each primitive's
    bytes (`NoteBytes`) and move `Gfx_PacketNext` through a 0x2000-byte buffer
    of the fuzz's own; the projections log their SVECTORs through `deref`; the
    matrix pushes' GTE callees log theirs and write a result where the real
    ones write (S31's effects);
  - `0x446770` logs the task's direction and pair and writes a new pair;
  - the sprite calls that act on `Sprite_Current` (`Sprite_UpdateScreen`, the
    steps) log which sprite;
  - **the `kFlag` blind spot** (a `kFlag` recorder answers 0 exactly when its
    own disturbance did nothing): `Battle_ActorIsOut` answers from `Noise` and
    a quarter of the time moves the target byte (`Snap_Buff` reads it again
    after a "no"); `MagicFx_ApplyBuff` and `Sprite_ScriptTickOnce` answer from
    `Noise` and a quarter of the time move `Sprite_Current`;
    `MagicFx_NearSprite` / `_NearSprite3D`, whose callers test all of eax,
    answer a C bool the same way;
  - `Gte_PushMatrix` keeps the facing inside 0..3 while the two wave matrices
    are fuzzed;
  - this group's own functions called directly, as `kPhase` recorders (the
    task they ran for), and `SnapSpark_Alloc` answering a pool index or 0xFF.
- **Tables:** the five `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `SnapSpark_Pool`; CLUT
  row 26's first two CLUTs and their source. 28,180 bytes of state in 15
  regions.
- **Seed:** `0x904B3C` at one of the harness's sprite records; the pool's
  live bits full, partly taken or random and every record's +0x80 a real slot
  or record (the walk makes it the owner cell, which recorders write
  through); each dispatcher inside its table; each count one step before and
  at its threshold (the wave's +9 at 4 / 8 and 0xE / 0xF or 0xA / 0xB by the
  swing, +4 at 1, the sparks' and images' +9 at 1, the turn at 3, the dive at
  0x13, the fade at 0xAF / 0xB0, the rise's +0xA at 1); +0xB 0 half the time
  where a branch tests it; the owner's +0xB 0..2; for the bounces the rise
  and fall either side of 0, the height either side of the source's, +0x5D
  at 0x80 / 0 / 3 / 0x90, and `Frame_Counter` even half the time.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex or scratch word,
  a pool record's live bit or owner, `0x904B3C`, the task's +0x14 / +0x20 /
  +0x3E / +0x40 / +0x44 / +0x5D..+0x5F, the owner's +0xB.

Result in this worktree (2026-09-26):

    shadow      magic_s04 self-test: 112000 rounds over 56 functions (2000 each), 1072246 calls to the stand-ins,
                0 MISMATCHES; 28180 bytes of state (15 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`; the thinnest, `Battle_SetTargetFlags`,
54 calls - `SnapWave_Burst`'s first swing). `BOF3X_SHADOW='*'`: exit 0
(1,071,119 stand-in calls for this group in that run: the harness's pointers
into the DLL move a few branches, 0 mismatches).

## 6. Controls

307 plants, each put in `magic_s04.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s04`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). **307 of 307 refused**: 306 by exit 3 with a count only in the functions the plant touches, one (S4, the pool walk's owner read from +0x7C) by an access violation on both sides - a garbage owner the recorders write through - so its near variant S4b (the owner cell not set at all) was planted and refused by a count. No equivalent mutant was planted; none was left standing.

One control was not refused at first: C124 (`AirRaidImage_Bounce`'s height compare `>=` made `>`). The seed put the image's +0x3C either side of the source's, but the function adds the rise to +0x3E, the high word of that dword, before comparing; the seed now puts it there after the rise, and the controls of the two bounces and the shared rise were run again (the table is the second run).

The S- controls are Snap_Task / Start, B- Snap_Buff, W- the wave, R- its rings, SP- the sparks, C- MAGIC015. The thinnest (fewer than 60 rounds):

- **B9** (Buff: +0xB of the task read before the create): Snap_Buff 11
- **W20** (Burst: flags 0x11): SnapWave_Burst 54
- **W21** (Burst: flags at +0xA 3): SnapWave_Burst 55
- **R17** (Ring: depths after the colours): SnapWave_DrawRingA 25
- **R17** (Ring: depths after the colours): SnapWave_DrawRingB 24
- **SP8** (Launch: source re-read for the height): SnapSpark_Launch 16
- **SP33** (Alloc: 63 records): SnapSpark_Alloc 8
- **C39** (FlyingKick_End: bit cleared after the animation): FlyingKick_End 25
- **C83** (AirRaidImage_Run: shadow below step 6): AirRaidImage_Run 36
- **C84** (AirRaidImage_Run: ground below step 3): AirRaidImage_Run 36
- **C113** (Dive: task not read again after the hit): AirRaidImage_Dive 17
- **C114** (hit: party below 4): AirRaidImage_Dive 53
- **C121** (Bounce: first image stop at 1): AirRaidImage_Bounce 49
- **C122** (Bounce: +9 held at 0x11): AirRaidImage_Bounce 8

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| S1 | Snap_Task: entries 1/2 swapped | Snap_Task 995 |
| S2 | walk: live bit 3 | Snap_Task 1510 |
| S3 | walk: owner not put back | Snap_Task 1623 |
| S4 | walk: owner from +0x7C | an access violation on both sides (exit 0xC0000005); S4b refused by a count |
| S4b | walk: the owner cell left as it was | Snap_Task 1992 |
| S5 | Start: +2 not cleared | Snap_Start 2000 |
| S6 | Start: +8 from the source's +9 | Snap_Start 1975 |
| S7 | Start: +0x3C from +0x38 | Snap_Start 2000 |
| S8 | Start: second CLUT with STP | Snap_Start 2000 |
| S9 | Start: 15 CLUT words | Snap_Start 2000 |
| S10 | Start: dirty 2 | Snap_Start 2000 |
| S11 | Start: parameter 0x2A | Snap_Start 2000 |
| S12 | Start: +0xB 3 | Snap_Start 2000 |
| S13 | Start: child +0x80 the owner | Snap_Start 1772 |
| B1 | Buff: out test inverted | Snap_Buff 2000 |
| B2 | Buff: stat 2 | Snap_Buff 646 |
| B3 | Buff: parameter 0x49 | Snap_Buff 441 |
| B4 | Buff: +4 5 | Snap_Buff 441 |
| B5 | Buff: +9 0x19 | Snap_Buff 441 |
| B6 | Buff: +0xA 3 | Snap_Buff 441 |
| B7 | Buff: target read once, before the out test | Snap_Buff 171 |
| B8 | Buff: the task read before the calls for +1 | Snap_Buff 206 |
| B9 | Buff: +0xB of the task read before the create | Snap_Buff 11 |
| W1 | Wave_Run: step + 1 | SnapWave_Run 2000 |
| W2 | Wave_Run: drawn on +1 | SnapWave_Run 971 |
| W3 | Wave_Run: ring B twice | SnapWave_Run 1026 |
| W4 | Wave_Task: the spark's table | SnapWave_Task 2000 |
| W5 | Wave_Start: facing from +9 | SnapWave_Start 1980 |
| W6 | Wave_Start: offset 0x20001 | SnapWave_Start 1990 |
| W7 | Wave_Start: height + 0x1000001 | SnapWave_Start 1986 |
| W8 | Wave_Start: z from the owner's x | SnapWave_Start 2000 |
| W9 | Wave_Start: step 0xFFFFA001 | SnapWave_Start 1992 |
| W10 | Wave_Start: +9 0x11 | SnapWave_Start 2000 |
| W11 | Wave_Start: +0xA 5 | SnapWave_Start 2000 |
| W12 | Wave_Start: +4 1 | SnapWave_Start 2000 |
| W13 | Burst: +9 down by 3 | SnapWave_Burst 2000 |
| W14 | Burst: 31 sparks | SnapWave_Burst 472 |
| W15 | Burst: 0x3F taken for none | SnapWave_Burst 172 |
| W16 | Burst: +0 or 0x40 | SnapWave_Burst 351 |
| W17 | Burst: +0xB i + 1 | SnapWave_Burst 472 |
| W18 | Burst: +9 Rand & 7 + 2 | SnapWave_Burst 472 |
| W19 | Burst: spark owner the owner | SnapWave_Burst 472 |
| W20 | Burst: flags 0x11 | SnapWave_Burst 54 |
| W21 | Burst: flags at +0xA 3 | SnapWave_Burst 55 |
| W22 | Burst: step 0x3001 | SnapWave_Burst 467 |
| W23 | Burst: sound 0x101 | SnapWave_Burst 472 |
| W24 | Burst: +0xB of the task read before Rand | SnapWave_Burst 292 |
| W25 | Swing: limit 0xE | SnapWave_Swing 152 |
| W26 | Swing: short swing at +0xA 2 | SnapWave_Swing 310 |
| W27 | Swing: +9 up by 3 | SnapWave_Swing 2000 |
| W28 | Swing: sound 0x202 | SnapWave_Swing 151 |
| W29 | Swing: owner's +0xA down | SnapWave_Swing 151 |
| W30 | Swing: +2 on, not back | SnapWave_Swing 500 |
| W31 | Swing: the actor flagged | SnapWave_Swing 135 |
| W32 | Swing: step 0xFFFFA800 | SnapWave_Swing 500 |
| W33 | End: waits on +0xA | SnapWave_End 1043 |
| W34 | End: counts +5 | SnapWave_End 1042 |
| W35 | End: owner not counted down | SnapWave_End 241 |
| W36 | MatrixA: facing 0 0x41 | SnapWave_PushMatrixA 498 |
| W37 | MatrixA: facing 1 about y | SnapWave_PushMatrixA 505 |
| W38 | MatrixA: facing 2 sl 5 | SnapWave_PushMatrixA 531 |
| W39 | MatrixA: facing 3 about z | SnapWave_PushMatrixA 460 |
| W40 | MatrixA: height + 0x21 | SnapWave_PushMatrixA 989 |
| W41 | MatrixB: facing 0 about x | SnapWave_PushMatrixB 509 |
| W42 | MatrixB: facing 3 sl 7 | SnapWave_PushMatrixB 499 |
| W43 | MatrixB: height from +0x3C | SnapWave_PushMatrixB 2000 |
| W44 | pushes: x sar 8 | SnapWave_PushMatrixA 2000, SnapWave_PushMatrixB 2000, SnapSpark_PushMatrix 2000, KickImage_PushMatrixGround 1999, KickImage_PushMatrixSource 2000 |
| W45 | pushes: z - 0x3FFF | SnapWave_PushMatrixA 2000, SnapWave_PushMatrixB 2000, SnapSpark_PushMatrix 2000, KickImage_PushMatrixGround 2000, KickImage_PushMatrixSource 2000 |
| W46 | pushes: halving by sar | SnapWave_PushMatrixA 498, SnapWave_PushMatrixB 494, SnapSpark_PushMatrix 530, KickImage_PushMatrixGround 509, KickImage_PushMatrixSource 490 |
| W47 | pushes: camera on the right | SnapWave_PushMatrixA 2000, SnapWave_PushMatrixB 2000, SnapSpark_PushMatrix 2000, KickImage_PushMatrixGround 2000, KickImage_PushMatrixSource 2000 |
| W48 | pushes: rotation from the translation | SnapWave_PushMatrixA 2000, SnapWave_PushMatrixB 2000, SnapSpark_PushMatrix 2000, KickImage_PushMatrixGround 2000, KickImage_PushMatrixSource 2000 |
| R1 | Ring: tpage 0x36 | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R2 | Ring: shade x 13 | SnapWave_DrawRingA 1980, SnapWave_DrawRingB 1982 |
| R3 | Ring: first radius sl 7 | SnapWave_DrawRingA 1999, SnapWave_DrawRingB 1996 |
| R4 | Ring: first angle sl 6 | SnapWave_DrawRingA 1572, SnapWave_DrawRingB 1555 |
| R5 | Ring: first x from the cosine | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R6 | Ring: first y cos of the radius cell | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R7 | Ring: first z Sin(1) | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R8 | Ring: 15 quads | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R9 | Ring: first edge the other way | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R10 | Ring: v2 z the y | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R11 | Ring: angle mask 0x3F | SnapWave_DrawRingA 1315, SnapWave_DrawRingB 1288 |
| R12 | Ring: height step i & 7 | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R13 | Ring: second edge 2 dz | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R14 | Ring: near colour 2 | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R15 | Ring: far colour from 0x903859 | SnapWave_DrawRingA 1992, SnapWave_DrawRingB 1996 |
| R16 | Ring: linked 0x40 | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R17 | Ring: depths after the colours | SnapWave_DrawRingA 25, SnapWave_DrawRingB 24 |
| R18 | RingA: 0x40 below | SnapWave_DrawRingA 2000 |
| R19 | RingB: 0x60 above | SnapWave_DrawRingB 2000 |
| R20 | Ring: v1 x the y | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R21 | Ring: radius sar 11 | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R22 | Ring: projected at 0x14 | SnapWave_DrawRingA 2000, SnapWave_DrawRingB 2000 |
| R23 | Ring: the facing read before the sine | SnapWave_DrawRingA 698, SnapWave_DrawRingB 695 |
| SP1 | Spark_Task: the wave's table | SnapSpark_Task 2000 |
| SP2 | Spark_Run: steps swapped | SnapSpark_Run 2000 |
| SP3 | Spark_Run: +9 not tested | SnapSpark_Run 473 |
| SP4 | Spark_Run: the matrix twice | SnapSpark_Run 521 |
| SP5 | Launch: angle sl 6 | SnapSpark_Launch 477 |
| SP6 | Launch: x sar 8 | SnapSpark_Launch 486 |
| SP7 | Launch: z from the source's x | SnapSpark_Launch 486 |
| SP8 | Launch: source re-read for the height | SnapSpark_Launch 16 |
| SP9 | Launch: rise Rand & 0xF | SnapSpark_Launch 240 |
| SP10 | Launch: fall -11 | SnapSpark_Launch 486 |
| SP11 | Launch: +9 0x11 | SnapSpark_Launch 486 |
| SP12 | Launch: z cos of the radius cell | SnapSpark_Launch 486 |
| SP13 | Fly: x sl 9 | SnapSpark_Fly 2000 |
| SP14 | Fly: x through the task read after the call | SnapSpark_Fly 71 |
| SP15 | Fly: z sar 10 | SnapSpark_Fly 2000 |
| SP16 | Fly: rise from +0x16 | SnapSpark_Fly 2000 |
| SP17 | Fly: +0..+3 cleared | SnapSpark_Fly 485 |
| SP18 | Fly: owner not counted down | SnapSpark_Fly 486 |
| SP19 | Fly: fall from +0x24 | SnapSpark_Fly 2000 |
| SP20 | Spark matrix: sl 5 | SnapSpark_PushMatrix 1997 |
| SP21 | Spark matrix: height + 1 | SnapSpark_PushMatrix 987 |
| SP22 | Spark draw: tpage 0x34 | SnapSpark_Draw 2000 |
| SP23 | Spark draw: SetPolyG3 | SnapSpark_Draw 2000 |
| SP24 | Spark draw: radius sl 5 | SnapSpark_Draw 1993 |
| SP25 | Spark draw: corner 0x556 | SnapSpark_Draw 2000 |
| SP26 | Spark draw: v1 z 1 | SnapSpark_Draw 1999 |
| SP27 | Spark draw: v2 y from the sine | SnapSpark_Draw 2000 |
| SP28 | Spark draw: projected at 0x10 | SnapSpark_Draw 2000 |
| SP29 | Spark draw: colour Rand & 0x1F | SnapSpark_Draw 1573 |
| SP30 | Spark draw: two colour bytes | SnapSpark_Draw 2000 |
| SP31 | Spark draw: linked 0x28 | SnapSpark_Draw 2000 |
| SP32 | Spark draw: depths 3_10B | SnapSpark_Draw 2000 |
| SP33 | Alloc: 63 records | SnapSpark_Alloc 8 |
| SP34 | Alloc: takes with 3 | SnapSpark_Alloc 755 |
| SP35 | Alloc: none 0xFE | SnapSpark_Alloc 491 |
| SP36 | Alloc: free by bit 1 | SnapSpark_Alloc 1650 |
| C1 | Charge_Task: entries 1/2 swapped | Charge_Task 1006 |
| C2 | AirRaid_Task: entries 2/3 swapped | AirRaid_Task 1022 |
| C3 | FlyingKick_Task: MAGIC018's end | FlyingKick_Task 488 |
| C4 | Charge_Start: +9 1 | Charge_Start 1720 |
| C5 | Charge_Start: animation argument 3 | Charge_Start 2000 |
| C6 | Charge_Start: three trails | Charge_Start 2000 |
| C7 | Charge_Start: n xor 2 | Charge_Start 2000 |
| C8 | Charge_Start: trail +1 2 | Charge_Start 2000 |
| C9 | Charge_Start: +9 back + 2 | Charge_Start 2000 |
| C10 | Charge_Start: +0xB back + 1 | Charge_Start 2000 |
| C11 | Charge_Start: dasher +1 1 | Charge_Start 1999 |
| C12 | Charge_Start: trail +6 2 | Charge_Start 2000 |
| C13 | Charge_Start: owner bit 0x20 | Charge_Start 1513 |
| C14 | palette: +0x28 from +0x29 | Charge_Start 1992, AirRaid_Start 1867, FlyingKick_Start 1994 |
| C15 | palette: +0x25 | Charge_Start 1998, AirRaid_Start 1877, FlyingKick_Start 2000 |
| C16 | palette: row copied from the task | Charge_Start 1762, AirRaid_Start 1744, FlyingKick_Start 1742 |
| C17 | images: 0x7C bytes copied | Charge_Start 2000, AirRaid_Start 2000, FlyingKick_Start 2000 |
| C18 | images: party below 2 | Charge_Start 467, AirRaid_Start 435, FlyingKick_Start 387 |
| C19 | images: enemy index - 2 | Charge_Start 858, AirRaid_Start 845, FlyingKick_Start 798 |
| C20 | images: actor read before the create | Charge_Start 293, AirRaid_Start 196, FlyingKick_Start 73 |
| C21 | Charge_Start: task read before the create | Charge_Start 233 |
| C22 | Charge_Start: dasher not counted | Charge_Start 1968 |
| C23 | AirRaid_Start: entry 31 kept | AirRaid_Start 2000 |
| C24 | AirRaid_Start: two images | AirRaid_Start 2000 |
| C25 | AirRaid_Start: +1 3 | AirRaid_Start 2000 |
| C26 | AirRaid_Start: +0xB number + 1 | AirRaid_Start 2000 |
| C27 | AirRaid_Start: +9 delay + 1 | AirRaid_Start 2000 |
| C28 | AirRaid_Start: sound 0x101 | AirRaid_Start 2000 |
| C29 | AirRaid_Start: +0xB 1 | AirRaid_Start 1831 |
| C30 | AirRaid_Start: +1 by 2 | AirRaid_Start 2000 |
| C31 | FlyingKick_Start: height dword +0x3C | FlyingKick_Start 1705 |
| C32 | FlyingKick_Start: animation argument 1 | FlyingKick_Start 2000 |
| C33 | FlyingKick_Start: +1 2 | FlyingKick_Start 2000 |
| C34 | FlyingKick_Start: +9 1 | FlyingKick_Start 1998 |
| C35 | FlyingKick_Start: +0xB 2 | FlyingKick_Start 1895 |
| C36 | FlyingKick_End: at +0xB 1 | FlyingKick_End 1040 |
| C37 | FlyingKick_End: mask 0xBE | FlyingKick_End 451 |
| C38 | FlyingKick_End: animation argument 1 | FlyingKick_End 1015 |
| C39 | FlyingKick_End: bit cleared after the animation | FlyingKick_End 25 |
| C40 | KickImage_Task: kind + 1 | KickImage_Task 2000 |
| C41 | ChargeImage_Run: Wait / Squash swapped | ChargeImage_Run 463 |
| C42 | image runs: +2 not tested | ChargeImage_Run 145, ChargeTrail_Run 212, AirRaidImage_Run 156, FlyingKickImage_Run 141 |
| C43 | by side: party below 2 | ChargeImage_Start 368, ChargeTrail_Start 104 |
| C44 | by side: enemy by 3 | ChargeImage_Start 792, ChargeTrail_Start 207 |
| C45 | ChargeImage_Start: +0x2B 2 | ChargeImage_Start 2000 |
| C46 | owner point: height from +0x38 | ChargeImage_Start 2000, ChargeImage_Squash 496, ChargeTrail_Start 500, KickImage_Settle 396 |
| C47 | Tick: on at 0 | KickImage_Tick 2000 |
| C48 | ChargeImage_Dash: speed 0x61 | ChargeImage_Dash 2000 |
| C49 | ChargeImage_Dash: near 0xC001 | ChargeImage_Dash 2000 |
| C50 | ChargeImage_Dash: cry 1 | ChargeImage_Dash 831 |
| C51 | ChargeImage_Dash: flag before the sound | ChargeImage_Dash 1335 |
| C52 | aim: Ratan2 (dz, dx) | ChargeImage_Dash 1162, ChargeTrail_Dash 1160 |
| C53 | aim: +0x14 0x41 | ChargeImage_Dash 1335, ChargeTrail_Dash 1336 |
| C54 | aim: +0x20 -9 | ChargeImage_Dash 1335, ChargeTrail_Dash 1336 |
| C55 | aim: +9 0x11 | ChargeImage_Dash 1335, ChargeTrail_Dash 1336 |
| C56 | aim: dz from the task's x | ChargeImage_Dash 1335, ChargeTrail_Dash 1336 |
| C57 | aim: angle + 1 | ChargeImage_Dash 1335, ChargeTrail_Dash 1336 |
| C58 | Arc: x sl 12 | KickImage_Arc 2000 |
| C59 | Arc: cos of +0x10 | KickImage_Arc 2000 |
| C60 | Arc: x through the task read after the call | KickImage_Arc 61 |
| C61 | rise and fall: from +0x16 | KickImage_Arc 2000, AirRaidImage_Rise 1472, AirRaidImage_Bounce 1542, FlyingKickImage_Rise 2000, FlyingKickImage_Bounce 1362 |
| C62 | rise and fall: fall from +0x1C | KickImage_Arc 2000, AirRaidImage_Rise 2000, AirRaidImage_Bounce 2000, FlyingKickImage_Rise 2000, FlyingKickImage_Bounce 2000 |
| C63 | Arc: on at +9 1 | KickImage_Arc 960 |
| C64 | Wait: owner's +0xB 2 | ChargeImage_Wait 1292 |
| C65 | Wait: +0x48 3 | ChargeImage_Wait 1118 |
| C66 | Wait: +9 9 | ChargeImage_Wait 1118 |
| C67 | Squash: x scale - 0x1FFF | ChargeImage_Squash 2000 |
| C68 | Squash: y scale + 0x3001 | ChargeImage_Squash 2000 |
| C69 | Squash: lift 0x21 | ChargeImage_Squash 1580 |
| C70 | Squash: +9 7 | ChargeImage_Squash 496 |
| C71 | Stretch: x scale + 0x2001 | ChargeImage_Stretch 2000 |
| C72 | Stretch: y scale - 0x2FFF | ChargeImage_Stretch 2000 |
| C73 | Stretch: owner not counted down | ChargeImage_Stretch 491 |
| C74 | ChargeTrail_Run: Dash / Arc swapped | ChargeTrail_Run 799 |
| C75 | ChargeTrail_Run: BattleFx_FreeTask last | ChargeTrail_Run 400 |
| C76 | Trail_Start: bit 0x10 | ChargeTrail_Start 438 |
| C77 | Trail_Start: shade x 0x15 | ChargeTrail_Start 499 |
| C78 | Trail_Start: two shade bytes | ChargeTrail_Start 498 |
| C79 | Trail_Start: +0x2B 1 | ChargeTrail_Start 500 |
| C80 | Trail_Start: +0x5C 2 | ChargeTrail_Start 500 |
| C81 | Trail_Dash: turn when not near | ChargeTrail_Dash 2000 |
| C82 | Trail_Dash: toward the owner | ChargeTrail_Dash 1515 |
| C83 | AirRaidImage_Run: shadow below step 6 | AirRaidImage_Run 36 |
| C84 | AirRaidImage_Run: ground below step 3 | AirRaidImage_Run 36 |
| C85 | AirRaidImage_Run: shadow on +0xA | AirRaidImage_Run 149 |
| C86 | shadow: not drawn | AirRaidImage_Run 149, FlyingKickImage_Run 288 |
| C87 | AirRaidImage_Run: Dive / Bounce swapped | AirRaidImage_Run 587 |
| C88 | AirRaidImage_Start: +0x2B 2 | AirRaidImage_Start 235 |
| C89 | AirRaidImage_Start: blue 0xC1 | AirRaidImage_Start 235 |
| C90 | AirRaidImage_Start: fall -3 | AirRaidImage_Start 522 |
| C91 | AirRaidImage_Start: rise 0x81 | AirRaidImage_Start 522 |
| C92 | AirRaidImage_Start: +9 0x11 | AirRaidImage_Start 522 |
| C93 | AirRaidImage_Start: +0x27 from +0x26 | AirRaidImage_Start 522 |
| C94 | Rise: radius 0x1801 | AirRaidImage_Rise 2000 |
| C95 | Rise: x scale - 0x7FF | AirRaidImage_Rise 2000 |
| C96 | Rise: y scale + 0x1001 | AirRaidImage_Rise 2000 |
| C97 | Rise: height + 0xC01 | AirRaidImage_Rise 527 |
| C98 | Rise: step round the source | AirRaidImage_Rise 417 |
| C99 | Rise: x from the source's z | AirRaidImage_Rise 528 |
| C100 | Turn: at 5 | AirRaidImage_Turn 1005 |
| C101 | Turn: y scale 0x18001 | AirRaidImage_Turn 518 |
| C102 | Turn: red 0xFE - | AirRaidImage_Turn 254 |
| C103 | Turn: green sl 3 | AirRaidImage_Turn 250 |
| C104 | Turn: first red 0x31 | AirRaidImage_Turn 264 |
| C105 | Turn: first blue 0xC1 | AirRaidImage_Turn 264 |
| C106 | Turn: x scale 0x8001 | AirRaidImage_Turn 518 |
| C107 | Dive: radius 0x2001 | AirRaidImage_Dive 2000 |
| C108 | Dive: down 0xBF | AirRaidImage_Dive 2000 |
| C109 | Dive: at 0x15 | AirRaidImage_Dive 1023 |
| C110 | Dive: rise 0x49 | AirRaidImage_Dive 532 |
| C111 | Dive: +0x48 1 | AirRaidImage_Dive 532 |
| C112 | Dive: the hit for the later images | AirRaidImage_Dive 532 |
| C113 | Dive: task not read again after the hit | AirRaidImage_Dive 17 |
| C114 | hit: party below 4 | AirRaidImage_Dive 53, FlyingKickImage_Dive 253 |
| C115 | hit: flag last | AirRaidImage_Dive 270, FlyingKickImage_Dive 1368 |
| C116 | Bounce: red by the second byte | AirRaidImage_Bounce 1331 |
| C117 | Bounce: green by the first byte | AirRaidImage_Bounce 1334 |
| C118 | Bounce: stop at 0x81 | AirRaidImage_Bounce 206 |
| C119 | Bounce: first image red - 4 | AirRaidImage_Bounce 333 |
| C120 | Bounce: first image green + 5 | AirRaidImage_Bounce 333 |
| C121 | Bounce: first image stop at 1 | AirRaidImage_Bounce 49 |
| C122 | Bounce: +9 held at 0x11 | AirRaidImage_Bounce 8 |
| C123 | Bounce: round the source sprite | AirRaidImage_Bounce 997 |
| C124 | Bounce: below or at the source | AirRaidImage_Bounce 174 |
| C125 | Bounce: falling at 0 | AirRaidImage_Bounce 83 |
| C126 | Bounce: owner not counted down | AirRaidImage_Bounce 385 |
| C127 | Bounce: red left 1 | AirRaidImage_Bounce 84 |
| C128 | Bounce: height from the source's +0x38 | AirRaidImage_Bounce 84 |
| C129 | Bounce: no tick | AirRaidImage_Bounce 2000 |
| C130 | Settle: down 0xF | KickImage_Settle 1487 |
| C131 | Settle: green + 0xF1 | KickImage_Settle 1487 |
| C132 | Settle: owner's +0xB 2 | KickImage_Settle 664 |
| C133 | Settle: at 0x90 | KickImage_Settle 606 |
| C134 | Settle: on when the tick says 0 | KickImage_Settle 602 |
| C135 | AirRaid FadeOut: up 0x11 | AirRaidImage_FadeOut 2000 |
| C136 | AirRaid FadeOut: at 0xB0 | AirRaidImage_FadeOut 551 |
| C137 | shade up: blue + 1 | AirRaidImage_FadeOut 1999, FlyingKickImage_FadeOut 2000 |
| C138 | FlyingKickImage_Run: shadow below step 5 | FlyingKickImage_Run 70 |
| C139 | FlyingKickImage_Run: ground below step 2 | FlyingKickImage_Run 100 |
| C140 | FlyingKickImage_Run: Bounce / Settle swapped | FlyingKickImage_Run 560 |
| C141 | Kick Start: fall -15 | FlyingKickImage_Start 2000 |
| C142 | Kick Start: +0xA 8 | FlyingKickImage_Start 2000 |
| C143 | Kick Start: rise 0x81 | FlyingKickImage_Start 2000 |
| C144 | Kick Start: +9 0x15 | FlyingKickImage_Start 2000 |
| C145 | Kick Rise: radius 0x3801 | FlyingKickImage_Rise 2000 |
| C146 | Kick Rise: +9 kept | FlyingKickImage_Rise 2000 |
| C147 | Kick Rise: on at +0xA 1 | FlyingKickImage_Rise 974 |
| C148 | Kick Dive: +9 up to 0x13 | FlyingKickImage_Dive 464 |
| C149 | Kick Dive: the flat test | FlyingKickImage_Dive 2000 |
| C150 | Kick Dive: fall -7 | FlyingKickImage_Dive 1363 |
| C151 | Kick Dive: speed 0x5F | FlyingKickImage_Dive 2000 |
| C152 | Kick Bounce: frame bit 1 | FlyingKickImage_Bounce 467 |
| C153 | Kick Bounce: +9 up to 0x15 | FlyingKickImage_Bounce 155 |
| C154 | Kick Bounce: bit 0x40 | FlyingKickImage_Bounce 368 |
| C155 | Kick Bounce: blue left 1 | FlyingKickImage_Bounce 440 |
| C156 | Kick Bounce: radius 0x2001 | FlyingKickImage_Bounce 2000 |
| C157 | Kick Bounce: rising above 0 | FlyingKickImage_Bounce 84 |
| C158 | Kick FadeOut: owner not counted down | FlyingKickImage_FadeOut 473 |
| C159 | Kick FadeOut: +2 by 2 | FlyingKickImage_FadeOut 473 |
| C160 | Ground: elevation not cut to 16 bits | KickImage_PushMatrixGround 1259 |
| C161 | Ground: elevation (z, x) | KickImage_PushMatrixGround 2000 |
| C162 | Ground: turned 1 about z | KickImage_PushMatrixGround 2000 |
| C163 | Source matrix: the image's height | KickImage_PushMatrixSource 2000 |
| C164 | Shadow: tpage 0x56 | KickImage_DrawShadow 2000 |
| C165 | Shadow: slot 4 | KickImage_DrawShadow 2000 |
| C166 | Shadow: radius x 8 | KickImage_DrawShadow 1987 |
| C167 | Shadow: seven triangles | KickImage_DrawShadow 2000 |
| C168 | Shadow: centre 0x81 | KickImage_DrawShadow 2000 |
| C169 | Shadow: rim 2 | KickImage_DrawShadow 2000 |
| C170 | Shadow: committed 0x30 | KickImage_DrawShadow 2000 |
| C171 | Shadow: closing tpage 0x16 | KickImage_DrawShadow 2000 |
| C172 | Shadow: previous rim (z, x) | KickImage_DrawShadow 1989 |
| C173 | Shadow: centre height 1 | KickImage_DrawShadow 2000 |
| C174 | Shadow: rim x by word 0x903852 | KickImage_DrawShadow 2000 |
| C175 | Shadow: projected at 0x14 | KickImage_DrawShadow 2000 |
| C176 | Shadow: first rim Cos(1) | KickImage_DrawShadow 2000 |
| C177 | Shadow: radius read before the draw mode | KickImage_DrawShadow 132 |

**Re-run 2026-09-26 on the kFlag-fixed harness
([`magic_harness.md`](magic_harness.md) section 8): 56 controls in the
affected functions, 56 refused.** The eight clones that reach a `kFlag` /
`kBool` answer are `Snap_Buff`, `ChargeImage_Dash`, `ChargeTrail_Dash`,
`KickImage_Tick`, `KickImage_Settle`, `AirRaidImage_Bounce`,
`FlyingKickImage_Dive` and `FlyingKickImage_Bounce`. Selected, every control
whose plant lies in one of them or in a helper they call: B1..B9, C46 (the
shared owner point, `KickImage_Settle` among its four), C47..C57 (C52..C57 in
the shared aim), C61 / C62 (the shared rise and fall), C81 / C82, C114 / C115
(the shared hit), C116..C134 and C148..C157, each rebuilt from the table.
Skipped: every other control, whose plant lies only in functions that reach
no `kFlag` / `kBool` answer. Each was refused, and in exactly the rounds the
table gives: this group's fuzz already answers `Battle_ActorIsOut`,
`MagicFx_ApplyBuff`, `Sprite_ScriptTickOnce` and the near tests from
`Noise` (section 5), not from the harness's draw. The thinnest: **C122** 8,
**B9** 11, **C121** 49, **C114** 53 (`AirRaidImage_Dive`). Clean self-test
after the restore: 0 mismatches, exit 0; `BOF3X_SHADOW='*'` exit 0. No fuzz
change.

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Things
to look for:

- Snap: a wave swinging out and back four times from the caster, two tipped
  rings, sparks thrown up at each turn; a buff on the target.
- Charge: four trails and an image dashing to the target and springing back,
  squashing and stretching; the caster hidden meanwhile.
- Air Raid: three images rising round the target, turning, diving and
  bouncing, with a shadow under the first.
- Flying Kick: one image rising round the target and diving at it.

Row 7 (MAGIC015's `AirRaid_Task`) is loaded by no ability id; row 58
(MAGIC016's, ability 0x10) runs the same code.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96, D97 and D106 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the six stack tables and the
  five `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in `Snap_Start`,
  `Snap_Buff` and every image maker (`Charge_Start` five times,
  `AirRaid_Start` three, `FlyingKick_Start` one): slot 255 is
  `0x93A000 + 255 x 0x84 = 0x9423FC`, past the image's end (`0x93F000`) - the
  images copy 0x80 bytes there. An access violation, in ours as in the
  original (the same addresses are written).
- **The wave matrices turn by uninitialised stack words** for a facing past 3
  (section 2; ours aborts).
- **The images index the actor records by the actor byte**, unchecked above
  10 (the enemy record by index - 3).
- **`SnapWave_FacingPhase` is indexed by the facing unchecked** (a facing past
  3 reads the next table's bytes) and `AirRaidImage_ShadeSteps` by +0xB
  unchecked (only 0..2 are made); both are reads inside `.data`, harmless.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 50 lines under a `group S04` comment, in
address order: 45 new, plus five host extents re-listed smaller
(`0049EEE0 118`, `0049F000 114`, `0049F620 12`, `0049FA10 57`,
`004A1050 189`). Six were listed right already (`0049F120`, `0049F3A0`,
`0049F7F0`, `0049F8A0`, `004A0EF0`, `004A0FA0`).
