# Group S04: Snap, Charge, Flying Kick and Air Raid (MAGIC013, MAGIC015 with MAGIC016 folded)

**Status:** IN PROGRESS (2026-09-26). All 56 functions are ours
(`src/game/magic_s04.cpp`, shadow name `magic_s04`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 112,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
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

    shadow      magic_s04 self-test: 112000 rounds over 56 functions (2000 each), 1072437 calls to the stand-ins,
                0 MISMATCHES; 28180 bytes of state (15 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`; the thinnest, `Battle_SetTargetFlags`,
54 calls - `SnapWave_Burst`'s first swing). `BOF3X_SHADOW='*'`: exit 0
(1,071,337 stand-in calls for this group in that run: the harness's pointers
into the DLL move a few branches, 0 mismatches).

## 6. Controls

CONTROLS_TEXT

CONTROLS_TABLE

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
