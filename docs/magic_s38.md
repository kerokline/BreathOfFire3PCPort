# Group S38: Tempest / Hurricane, an unlabelled id and MeteorStrike (MAGIC223, 225, 226/227)

**Status:** IN PROGRESS (2026-09-27). All 54 functions are ours
(`src/game/magic_s38.cpp`, shadow name `magic_s38`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 108,000 rounds. 361 negative controls: 359 refused, every one by a count (exit 3), and 2 equivalent mutants, each with a refused near variant. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S38
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 137 | 0x2B3 | MAGIC223 | 0xDF, 0xE0 | Tempest, Hurricane | `0x4F8640..0x4F8F36` | 13 |
| 140 | 0x2B4 | MAGIC225 | 0xE1 | (no label) | `0x4F8F40..0x4F9DD6` | 18 |
| 135 | 0x2B5 | MAGIC226 | 0xE2 | MeteorStrike | `0x4F9DE0..0x4FAFE6` | 23 |
| 147 | 0x2B6 | MAGIC227 | 0xE3 | (one past the list) | the same code | - |

The extents are `tools/magic_rows.py --unit MAGIC223 / MAGIC225 /
MAGIC226/MAGIC227 --clones` (capstone recursive descent; no jump table,
nothing `REFUSED`). All 54 functions lie in the units' extents; none was
found inside or missing from them, and none was ours before. 10,218 bytes, as
the queue counted. Row 147 (ability 0xE3, [`magic_c3.md`](magic_c3.md) §1:
its ability record is byte-identical to 0xE2's) has the entry 0x4F9DE0, so
MAGIC227 runs MAGIC226's code and is covered by the same 23 functions.
`0x4FAF90`, first given to the effect library, is MAGIC226/227's own pool
allocator ([`magic_lib.md`](magic_lib.md) §1); it is taken here.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses; MAGIC225's id has no
label, so its functions are `Magic225*`. What each spell looks like in play
has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Tempest / Hurricane (MAGIC223).**
  - `Tempest_Task`, the kind-2 task: a three-entry stack table by +1
    (`Tempest_Start`, `MagicFx_CountDownFlag10`, `Tempest_End`), then the 64
    records of its own pool `Tempest_GustPool` (`0x6B7BE0`) walked: each live
    record becomes `Sprite_Current` with its +0x80 the owner for
    `TempestGust_Task`, both put back after each.
  - `Tempest_Start` clears the pool, takes the owner's direction and
    position, makes the flash (kind 1, 0x63) and 48 gusts
    (`Tempest_GustAlloc`), each with a delay +9 of (Rand & 0xF) + 16 x (i / 8)
    + 1, restores CLUT row 26's words 1..15 and 17..31 with their STP bits
    (words 0 and 16 cleared) and plays sound 0x100.
  - `Tempest_End` waits for every child, then sets the effect word
    `0x904AA8 |= 0x2004` (the done bit and 0x2000), flags the target 0x40 and
    ends.
  - The flash (`TempestFlash_*`) is Corona's (group S31) with one other step:
    `CoronaFlash_Start`, `BarrierRing_Grow`, MAGIC167's `0x4EF840` (on when
    the owner's +0xB is 1 or less), `MagicFx_CountDownRelease`; each frame one
    semi-transparent gouraud quad over the screen, bright on the side the
    owner faces.
  - A gust (`TempestGust_*`): after its delay it starts at the owner, moved
    72 x the sine / cosine of an angle by its number and
    `TempestGust_Angles[direction]` from its screen point, and takes that
    table's angle as its heading; it drifts 8 a frame for four frames and
    flies 24 a frame until +9 reaches 0x14, then ends (MAGIC219's free). Its
    draw is one textured quad of radius 0x4A turned by the heading, its V row
    by +9 & 3.
  - **The one difference between the two ids**: `TempestGust_Launch` sets the
    gust's CLUT row +0xB to 0 when the acting ability (the word `0x904B80`)
    is 0xDF, else 0x10. Nothing else in the overlay reads the id.
- **The unlabelled id (MAGIC225).**
  - `Magic225_Task`: `Magic225_Start`, `_Spawn`, `_Apply`, `BattleFx_Finish`,
    then its pool `Magic225_ShardPool` (32 at `0x6B9CE0`) walked.
  - `Magic225_Start` clears the pool, centres the task on the side, makes the
    veil (kind 1, 0x65), copies CLUT rows 2 and 26 back from their source
    **without** STP bits, and plays sound 0x101.
  - `Magic225_Spawn` (after 0x18 frames) makes sixteen shards with delays 8,
    12, .. 0x44, playing sound 0x100 before each: sixteen sound calls in one
    frame.
  - `Magic225_Apply` waits for the shards and then, for every actor of the
    target's side that `Battle_ActorIsOut` says is in (enemies 3..10 with the
    side bit, else the party 0..2), applies `MagicFx_BuffStats[0..2]`
    (`MagicFx_ApplyBuff`) and makes a popup (kind 1, 2: the library's
    `BuffPopupAt_Task`) per stat: +4 j + 4, or 8 when refused, +0xB the actor,
    delays 1 / 6 / 11, +0xA 12 j. Its `BattleTask_Create` is the only one
    here tested for 0xFF (none free: no popup, not counted).
  - The veil (`Magic225Veil_*`): `_Start` moves the acting actor's sprite and
    the side's live actors to draw layer 2 and takes layer 7 itself in the
    event battle of kind 0x37 (else 3); `_FadeIn`; `BarrierRing_Grow`;
    `_WaitBuffs` (on when the owner's count is 1 or less, the veil itself);
    `_End` puts everyone back on layer 4. Each frame a flat shade over the
    screen (`_DrawShade`, semi-transparent by +0xB) and four bursts
    (`_DrawBurst`) at (0x30, 0x10), (0x12C, 0), (0x40, 0xF0), (0x140, 0xF0):
    a fan of 32 gouraud triangles to the radius and a ring of 32 quads to 1.5
    x it, coloured `Magic225Veil_BurstColours[kind]` x +9.
  - A shard (`Magic225Shard_*`) starts at the acting actor's sprite
    (`0x904B3C`), its height +0x3C the sprite's + 0x1000000, heads for the task (`Math_Ratan2`,
    then +/- Rand & 0xFF by its number's bit 0), grows and slows every other
    frame from speed 0xC to 4, then falls on at 4 for 0x10 frames and ends.
- **MeteorStrike (MAGIC226, and MAGIC227).**
  - `MeteorStrike_Task`: `MeteorStrike_Start`, `MagicFx_CountDownFlag10`,
    `BattleFx_Finish`, then its pool `MeteorStrike_RecordPool` (48 at
    `0x6BAD60`) walked.
  - `MeteorStrike_Start` is MAGIC053's `Lavaburst_Start` (group S10) in
    shape: the pool cleared, the side's centre, the direction turned round
    when the target's side and the actor's disagree, one rock (kind 1, 0x5F),
    sound 0x100, CLUT row 26 and row 2's first 16 words with STP bits.
  - `MagicFx_CountDownFlag10` (`0x4F9F70`): +9 down, at 0 target flags 0x10
    and +1 on. A stack-table phase of ten files by `magic_funcs.tsv`; the
    merged groups hold it by raw address (C2's MAGIC129, S10's MAGIC053,
    S28's MAGIC124, S30's MAGIC130, S31's MAGIC132).
  - The rock (`MeteorStrikeRock_*`): `_Launch` sets its height +0x3C the
    owner's + 0x9000000, offset and stepped by turned vectors; `_Fall` takes
    0x300000 a frame off that for 0x20 frames, leaving a trail record every fourth
    frame, then makes 32 chips owned by the kind-2 task, jolts the camera
    and plays sound 0x101; `_Shake` rocks `Camera_Angles[0]` +/- 0x14 for 8
    frames then sets it to 0xFD56; `_Hide` (0x3C frames) hides the sprite
    (+0 |= 0x20); `_End` fades the tint down to 0x80 and ends. Each frame it
    is drawn with MAGIC219's matrix push, MAGIC053's
    `LavaburstChild_DrawGlow` and `MeteorStrikeRock_DrawRing` (64 gouraud
    quads between radii +0xB and 2 x +0xB; MAGIC053's Lavaburst draws it too).
  - The records (`MeteorStrikeRecord_Task`, two kinds by +1): a chip
    (`MeteorStrikeChip_*`: out from the owner at 24 x the sine of
    ((+0xB - 2) & 0xF) << 8, its height on by a lift from
    `MeteorStrikeChip_Lifts` that a pull takes down each frame, drawn as one textured quad linked at its map point) and a trail
    (`MeteorStrikeTrail_*`: at the rock's x / z, its height from the rock's -
    0x3000000 down by 0x60000 a frame, drawn as 32 gouraud
    quads between two rings under the actor matrix).
- **The three allocators** (`Tempest_GustAlloc`, `Magic225_ShardAlloc`,
  `MeteorStrike_RecordAlloc`) are one body at three pools: the first record
  without bit 0 gets it, the index in al, 0xFF when none.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the project's
one precedent: a phase past any of the sixteen dispatch tables (three stack
tables, thirteen `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3). The value tables
(`TempestGust_Angles`, `Magic225Veil_BurstColours`, `MeteorStrikeChip_*`)
are read in place by whatever index the original uses, so an index past one
reads what the original reads - no abort and no divergence.

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: `Gte_RotTransPers4` gets the depth and flag pointers.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F6290` | MAGIC219 (S37) | the tail jmp that ends a pool record (+0..+4 of `Sprite_Current` cleared): `TempestGust_Fly`, `Magic225Shard_Fall`, `MeteorStrikeChip_Fall`, `MeteorStrikeTrail_Fade` |
| `0x4F6020` | MAGIC219 (S37) | the rock's matrix push in `MeteorStrikeRock_Run` (S10's `LavaburstChild_Run` calls it the same way) |
| `0x4EF840` | MAGIC167 (S35) | entry 2 of `TempestFlash_Steps`: +2 on when the owner's +0xB is 1 or less |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S10, S22, S31 call it) |

By name, already ours: `CoronaFlash_Start` (S31), `BarrierRing_Grow` (S19),
`MagicFx_CountDownRelease` (S12), `LavaburstChild_DrawGlow` (S10),
`BattleFx_Finish`, the library (`MagicFx_CenterOnSide`,
`MagicFx_ApplyBuff`, `MagicFx_BuffStats`, `MagicFx_PushActorMatrix`,
`BattleActor_UpdateScreenXY`) and the GTE / GPU / sprite / sound library.

**Owed to others**: `0x4F9F70` (now `MagicFx_CountDownFlag10`) is held by
raw address in C2, S10, S28, S30 and S31, and `0x4FA440` (now
`MeteorStrikeRock_DrawRing`) in S10 (`kDrawLava`); they work as they are
(the stand-in falls back to the address) and can be rebound to the names.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `TempestFlash_TaskTable` | `0x65C2E4` | 1 |
| `TempestFlash_Steps` | `0x65C2E8` | 4 |
| `TempestGust_TaskTable` | `0x65C2F8` | 1 |
| `TempestGust_Steps` | `0x65C2FC` | 3 |
| `TempestGust_Angles` | `0x65C308` | 4 dwords |
| `Magic225Veil_TaskTable` | `0x65C318` | 1 |
| `Magic225Veil_BurstColours` | `0x65C31C` | 12 bytes |
| `Magic225Veil_Steps` | `0x65C328` | 5 |
| `Magic225Shard_TaskTable` | `0x65C33C` | 1 |
| `Magic225Shard_Steps` | `0x65C340` | 3 |
| `MeteorStrikeRock_TaskTable` | `0x65C34C` | 1 |
| `MeteorStrikeRock_Steps` | `0x65C350` | 5 |
| `MeteorStrikeChip_Shades` | `0x65C364` | 4 bytes |
| `MeteorStrikeChip_Fades` | `0x65C368` | 4 bytes |
| `MeteorStrikeRecord_Kinds` | `0x65C36C` | 2 |
| `MeteorStrikeChip_Lifts` | `0x65C374` | 4 dwords |
| `MeteorStrikeChip_Steps` | `0x65C384` | 3 |
| `MeteorStrikeTrail_Steps` | `0x65C390` | 3 |
| `Tempest_GustPool` | `0x6B7BE0` | 64 x 0x84 |
| `Magic225_ShardPool` | `0x6B9CE0` | 32 x 0x84 |
| `MeteorStrike_RecordPool` | `0x6BAD60` | 48 x 0x84 |

Each table's count is where the next starts (the addresses read
2026-09-27; `MagicFx_BuffStats`, the library's, follows at `0x65C39C`); the
tool's `.data` notes bounded every handler table the same. The three pools
are consecutive `.bss`.

## 5. The fuzz

`BOF3X_SHADOW=magic_s38` runs `magic_harness::Run` over the 54 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s38_fuzz.cpp`:

- **Callees** (41 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` log each
    primitive's bytes and move `Gfx_PacketNext` on through a 0x2000-byte
    buffer of the fuzz's own (S31's effects); `Gte_RotTransPers4` logs its
    four SVECTORs through `deref`; `Gfx_CommitPrim`'s and
    `MapView_LinkPrimAt`'s byte arguments masked to the byte they read (the
    originals push garbage above a layer byte);
  - `Math_Sin`, `Math_Cos` and `Rand` move a scratch word
    (`0x903850..0x90386B`) or a vertex word a third of the time - every
    caller reads its radius, angle or colour back from them after the call
    (S10's `StirScratch`, widened to the colour dwords);
  - the calls that act on `Sprite_Current` (the script tick, the animation,
    the screen point, the side's centre, the actor matrix, MAGIC219's two,
    `LavaburstChild_DrawGlow`, this group's draws) log which sprite; the
    screen update also logs `0x9039D8`, the frame-offset table the veil,
    shards and rock swap;
  - `0x446770` logs the task's direction and pair and writes a new pair;
  - `BattleTask_Create` answers 0xFF a quarter of the time while
    `Magic225_Apply` is fuzzed, the one caller that tests it;
  - this group's own functions called directly, by address: the pool
    record tasks as `kPhase` (so a wrong `Sprite_Current` or owner in a walk
    shows), the allocators as `kByte` 0xFF or an index.
- **Tables:** the thirteen `.data` handler tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer, `Prim_VertexScratch`, `0x903850..0x90386B`, `0x9039D8`,
  `Camera_Angles[0]`, the ability word `0x904B80`, the three pools, CLUT rows
  2 and 26 and their source rows. 40,680 bytes of state in 22 regions.
- **Seed:** `0x904B3C` at one of the harness's sprite records; each pool
  full, taken up to a point or as the fill left it, every record's owner a
  real slot or record; the target's side bit half the time, the actor 0..10,
  the owner's and task's direction 0..3 mostly, the ability 0xDF / 0xE0 half
  the time; each dispatcher inside its table; each count-down one step before
  and at its threshold (+9 at 1 / 2, `TempestGust_Drift`'s +0xA near 0x10,
  `_Fly`'s +9 near 0x14, the trail's near 0x18, the veil's fade, the rock's
  tint near 0x80, `Frame_Counter & 3` 0 half the time, the shard's speed at
  4 / 5); the event battle 0x37 / 0x36 for `Magic225Veil_Start`; the
  actor at 2..3 for `MeteorStrike_Start`'s side test;
  `Magic225Veil_DrawBurst`'s four words its caller's rows two times in three
  (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, `0x9039D8`, the camera angle, a pool record's live bit, the actor's
  sprite pointer, the event-battle byte, the ability word.

Result in this worktree (2026-09-27):

    shadow      magic_s38 self-test: 108000 rounds over 54 functions (2000 each), 3667422 calls to the stand-ins,
                0 MISMATCHES; 40680 bytes of state (22 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (3,665,115 stand-in
calls for this group in that run: the harness's pointers into the DLL move a
few branches, 0 mismatches).

## 6. Controls

361 plants, each put in `magic_s38.cpp` one at a time by a script (not
committed) that planted, rebuilt, checked the build had recompiled the file,
ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s38`, restored; after the last
it restored, rebuilt and ran the clean self-test (0 mismatches, exit 0). The
H- controls plant in the shared helpers, T- in MAGIC223, V- in MAGIC225, M- in
MAGIC226/227. **359 of 361 refused**, every one by exit 3 with a count only in
the functions the plant touches; **2 equivalent**, each with a near variant
refused:

- **T17** (`Tempest_Start` copies the CLUT words from 0, not 1): words 0 and
  16 are cleared after the loop, so no input tells them apart; T17b (the loop
  to word 16) is refused in 2,000.
- **M98** (`MeteorStrikeChip_Launch`'s pull by an arithmetic shift, not a
  divide toward zero): the lift it divides is loaded from
  `MeteorStrikeChip_Lifts` just before, and all four entries are positive
  multiples of 16, so the two agree on every input; M98b (a shift of 5) is
  refused in 483.

The first run (the same 361, on the fuzz before two changes) left two more
standing, both blind spots, now closed in `magic_s38_fuzz.cpp` and the whole
set re-run: **T50** (the ability compared by its low byte) - the seed now
puts 0xDF with a high byte in the word a third of the time for
`TempestGust_Launch`; **V97** (`Magic225Shard_Launch` reading the actor's
sprite pointer again after the turn) - the turn's stand-in now moves
`0x904B3C` a quarter of the time. Both refused on the re-run: T50 in 112 rounds, V97 in 78.

The thinnest (fewer than 60 rounds): T76 (63-record gust allocator) 4, M95
(the chip's cosine angle not read back) 8, M153 13, T45 14, V120 15, V66 16,
V65 30, V110 57. The three allocator plants are thin because a pool fills to
its last record only when the seed fills it.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| H1 | Mul12: a logical shift | TempestGust_Draw 1996, Magic225Veil_DrawBurst 2000, MeteorStrikeChip_Draw 1984 |
| H2 | WalkPool: bit 1 tested | Tempest_Task 2000, Magic225_Task 2000, MeteorStrike_Task 2000 |
| H3 | WalkPool: the owner not put back | Tempest_Task 1613, Magic225_Task 1618, MeteorStrike_Task 1637 |
| H4 | WalkPool: Sprite_Current not put back | Tempest_Task 1972, Magic225_Task 1946, MeteorStrike_Task 1965 |
| H5 | PoolAlloc: bit 1 set | Tempest_GustAlloc 1499, Magic225_ShardAlloc 1491, MeteorStrike_RecordAlloc 1482 |
| H6 | PoolAlloc: none is 0xFE | Tempest_GustAlloc 501, Magic225_ShardAlloc 509, MeteorStrike_RecordAlloc 518 |
| H7 | ClearPool: +2 kept | Tempest_Start 2000, Magic225_Start 2000, MeteorStrike_Start 2000 |
| H8 | AdoptChild: child +1 1 | Tempest_Start 1985, Magic225_Start 2000, MeteorStrike_Start 2000 |
| H9 | AdoptChild: counted in +0xA | Tempest_Start 1825, Magic225_Start 2000, MeteorStrike_Start 2000 |
| H10 | Rtp4: the fourth point at 2 x step | MeteorStrikeRock_DrawRing 2000, MeteorStrikeTrail_Draw 2000 |
| T1 | Tempest_Task: entries 1/2 swapped | Tempest_Task 1353 |
| T2 | Tempest_Task: 63 records walked | Tempest_Task 1015 |
| T3 | Tempest_Start: 63 records cleared | Tempest_Start 2000 |
| T4 | Tempest_Start: +9 7 | Tempest_Start 1506 |
| T5 | Tempest_Start: parameter 0x64 | Tempest_Start 2000 |
| T6 | Tempest_Start: height from +0x38 | Tempest_Start 2000 |
| T7 | Tempest_Start: direction from +9 | Tempest_Start 1468 |
| T8 | Tempest_Start: 47 gusts | Tempest_Start 2000 |
| T9 | Tempest_Start: delay Rand & 0x1F | Tempest_Start 2000 |
| T10 | Tempest_Start: delay by i / 4 | Tempest_Start 2000 |
| T11 | Tempest_Start: delay + 2 | Tempest_Start 2000 |
| T12 | Tempest_Start: gust number i + 1 | Tempest_Start 2000 |
| T13 | Tempest_Start: gusts owned by the owner | Tempest_Start 2000 |
| T14 | Tempest_Start: counted through the task read before Rand | Tempest_Start 1399 |
| T15 | Tempest_Start: gust +1 1 | Tempest_Start 2000 |
| T16 | Tempest_Start: second half STP 0x4000 | Tempest_Start 2000 |
| T17 | Tempest_Start: the words from 0 (equivalent: 0 and 16 are cleared after) | not refused: equivalent (above) |
| T17b | Tempest_Start: the words to 16 | Tempest_Start 2000 |
| T18 | Tempest_Start: word 0 kept | Tempest_Start 2000 |
| T19 | Tempest_Start: sound 0x101 | Tempest_Start 2000 |
| T20 | Tempest_End: at 1 | Tempest_End 1338 |
| T21 | Tempest_End: flags 0x2000 | Tempest_End 601 |
| T22 | Tempest_End: flags 0x0004 | Tempest_End 679 |
| T23 | Tempest_End: the actor flagged | Tempest_End 1275 |
| T24 | TempestFlash_Task: the gust's handler | TempestFlash_Task 2000 |
| T25 | TempestFlash_Run: steps 1/2 swapped | TempestFlash_Run 1053 |
| T26 | TempestFlash_Run: drawn with +0 clear | TempestFlash_Run 2000 |
| T27 | TempestFlash_Draw: facing bit 1 | TempestFlash_Draw 967 |
| T28 | TempestFlash_Draw: right edge 320.0 | TempestFlash_Draw 963 |
| T29 | TempestFlash_Draw: +0x3C 240.0 | TempestFlash_Draw 2000 |
| T30 | TempestFlash_Draw: +9 x 4 | TempestFlash_Draw 1988 |
| T31 | TempestFlash_Draw: +0x34 +9 x 4 | TempestFlash_Draw 1965 |
| T32 | TempestFlash_Draw: p[5] from 0x903858 | TempestFlash_Draw 1988 |
| T33 | TempestFlash_Draw: p[0x16] from Scratch_Swap | TempestFlash_Draw 1988 |
| T34 | TempestFlash_Draw: commit 0x40 | TempestFlash_Draw 2000 |
| T35 | TempestFlash_Draw: closing tpage 0x16 | TempestFlash_Draw 2000 |
| T36 | TempestFlash_Draw: layer 3 | TempestFlash_Draw 2000 |
| T37 | TempestFlash_Draw: +0x28 on the right | TempestFlash_Draw 1037 |
| T38 | TempestGust_Task: the flash's handler | TempestGust_Task 2000 |
| T39 | TempestGust_Run: steps 1/2 swapped | TempestGust_Run 1366 |
| T40 | TempestGust_Run: +2 not tested | TempestGust_Run 342 |
| T41 | TempestGust_Launch: at 1 | TempestGust_Launch 1010 |
| T42 | TempestGust_Launch: +0xB & 3 | TempestGust_Launch 257 |
| T43 | TempestGust_Launch: + 0x401 | TempestGust_Launch 501 |
| T44 | TempestGust_Launch: x 73 | TempestGust_Launch 501 |
| T45 | TempestGust_Launch: y taken after the call | TempestGust_Launch 14 |
| T46 | TempestGust_Launch: the angle stored once | TempestGust_Launch 500 |
| T47 | TempestGust_Launch: heading from the next direction | TempestGust_Launch 466 |
| T48 | TempestGust_Launch: ability 0xE0 | TempestGust_Launch 303 |
| T49 | TempestGust_Launch: row 0x11 | TempestGust_Launch 235 |
| T50 | TempestGust_Launch: the ability's low byte | TempestGust_Launch 112 |
| T51 | TempestGust_Launch: height not copied | TempestGust_Launch 428 |
| T52 | TempestGust_Launch: +0xA 1 | TempestGust_Launch 501 |
| T53 | TempestGust_Drift: x 9 | TempestGust_Drift 2000 |
| T54 | TempestGust_Drift: y by the sine | TempestGust_Drift 2000 |
| T55 | TempestGust_Drift: +0xA up by 3 | TempestGust_Drift 2000 |
| T56 | TempestGust_Drift: at 0x14 | TempestGust_Drift 493 |
| T57 | TempestGust_Drift: +9 not up | TempestGust_Drift 2000 |
| T58 | TempestGust_Fly: x 25 | TempestGust_Fly 2000 |
| T59 | TempestGust_Fly: at 0x13 | TempestGust_Fly 930 |
| T60 | TempestGust_Fly: the owner's +0xA | TempestGust_Fly 471 |
| T61 | TempestGust_Fly: y by 24 x the sine | TempestGust_Fly 2000 |
| T62 | TempestGust_Draw: radius 0x4B | TempestGust_Draw 1973 |
| T63 | TempestGust_Draw: shade x 4 | TempestGust_Draw 1797 |
| T64 | TempestGust_Draw: corner 0x889 | TempestGust_Draw 2000 |
| T65 | TempestGust_Draw: y from +0x2E | TempestGust_Draw 2000 |
| T66 | TempestGust_Draw: x by the angle word | TempestGust_Draw 2000 |
| T67 | TempestGust_Draw: the cosine of the angle not read back | TempestGust_Draw 196 |
| T68 | TempestGust_Draw: page x 0x341 | TempestGust_Draw 2000 |
| T69 | TempestGust_Draw: CLUT y 0x1FB | TempestGust_Draw 2000 |
| T70 | TempestGust_Draw: row +9 & 7 | TempestGust_Draw 988 |
| T71 | TempestGust_Draw: u 0x97 | TempestGust_Draw 2000 |
| T72 | TempestGust_Draw: lower rows + 0x20 | TempestGust_Draw 2000 |
| T73 | TempestGust_Draw: layer 2 | TempestGust_Draw 2000 |
| T74 | TempestGust_Draw: blue from Scratch_Swap | TempestGust_Draw 1990 |
| T75 | TempestGust_Draw: tpage 0x35 | TempestGust_Draw 2000 |
| T76 | Tempest_GustAlloc: 63 records | Tempest_GustAlloc 4 |
| V1 | Magic225_Task: entries 1/2 swapped | Magic225_Task 985 |
| V2 | Magic225_Task: 31 records walked | Magic225_Task 1015 |
| V3 | Magic225_Task: the gusts' task | Magic225_Task 1978 |
| V4 | Magic225_Start: 31 records cleared | Magic225_Start 2000 |
| V5 | Magic225_Start: +9 0x19 | Magic225_Start 1969 |
| V6 | Magic225_Start: parameter 0x66 | Magic225_Start 2000 |
| V7 | Magic225_Start: row 2 with STP | Magic225_Start 2000 |
| V8 | Magic225_Start: 255 words | Magic225_Start 2000 |
| V9 | Magic225_Start: row 26 from row 25 | Magic225_Start 2000 |
| V10 | Magic225_Start: sound 0x100 | Magic225_Start 2000 |
| V11 | Magic225_Start: no centre | Magic225_Start 2000 |
| V12 | Magic225_Spawn: at 1 | Magic225_Spawn 1002 |
| V13 | Magic225_Spawn: 15 shards | Magic225_Spawn 503 |
| V14 | Magic225_Spawn: delays from 9 | Magic225_Spawn 503 |
| V15 | Magic225_Spawn: sound 0x101 | Magic225_Spawn 503 |
| V16 | Magic225_Spawn: number i + 1 | Magic225_Spawn 503 |
| V17 | Magic225_Spawn: the task read before the alloc | Magic225_Spawn 188 |
| V18 | Magic225_Spawn: +1 of the shard 1 | Magic225_Spawn 503 |
| V19 | Magic225_Apply: at 1 | Magic225_Apply 1337 |
| V20 | Magic225_Apply: enemies 3..9 | Magic225_Apply 666 |
| V21 | Magic225_Apply: party 0..1 | Magic225_Apply 667 |
| V22 | Magic225_Apply: out enemies too | Magic225_Apply 666 |
| V23 | Magic225_Apply: side bit 0x80 | Magic225_Apply 666 |
| V24 | Magic225_Apply: +1 not on | Magic225_Apply 1333 |
| V25 | BuffActor: stat j + 1 | Magic225_Apply 1113 |
| V26 | BuffActor: +4 j + 5 | Magic225_Apply 1061 |
| V27 | BuffActor: refused 9 | Magic225_Apply 859 |
| V28 | BuffActor: delays by 4 | Magic225_Apply 1113 |
| V29 | BuffActor: +0xA 13 j | Magic225_Apply 1087 |
| V30 | BuffActor: popup parameter 3 | Magic225_Apply 1113 |
| V31 | BuffActor: counted with none free | Magic225_Apply 823 |
| V32 | BuffActor: the task read before the create | Magic225_Apply 165 |
| V33 | BuffActor: +0xB the stat | Magic225_Apply 1089 |
| V34 | BuffActor: the actor + 1 buffed | Magic225_Apply 1113 |
| V35 | Magic225Veil_Task: the shard's handler | Magic225Veil_Task 2000 |
| V36 | Magic225Veil_Run: steps 3/4 swapped | Magic225Veil_Run 795 |
| V37 | Magic225Veil_Run: the battle's table not put back | Magic225Veil_Run 2000 |
| V38 | Magic225Veil_Run: burst 2's kind 2 | Magic225Veil_Run 815 |
| V39 | Magic225Veil_Run: burst 1's radius 0x53 | Magic225Veil_Run 815 |
| V40 | Magic225Veil_Run: burst 4 at y 0xF1 | Magic225Veil_Run 815 |
| V41 | Magic225Veil_Run: +2 not tested | Magic225Veil_Run 223 |
| V42 | Magic225Veil_Run: the shade after the bursts | Magic225Veil_Run 815 |
| V43 | Magic225Veil_Start: the actor's layer 3 | Magic225Veil_Start 2000 |
| V44 | Magic225Veil_Start: the side's layer 1 | Magic225Veil_Start 1665 |
| V45 | LayerSide: enemies 0..6 | Magic225Veil_Start 1048, Magic225Veil_End 240 |
| V46 | LayerSide: asks for i + 2 | Magic225Veil_Start 1048, Magic225Veil_End 240 |
| V47 | LayerSide: party +0x28 | Magic225Veil_Start 664, Magic225Veil_End 194 |
| V48 | LayerSide: side bit 0x80 | Magic225Veil_Start 1048, Magic225Veil_End 240 |
| V49 | LayerSide: out party members too | Magic225Veil_Start 952, Magic225Veil_End 260 |
| V50 | Magic225Veil_Start: event 0x36 | Magic225Veil_Start 966 |
| V51 | Magic225Veil_Start: layer 6 there | Magic225Veil_Start 492 |
| V52 | Magic225Veil_Start: +0xB 2 | Magic225Veil_Start 2000 |
| V53 | Magic225Veil_FadeIn: up by 1 | Magic225Veil_FadeIn 2000 |
| V54 | Magic225Veil_FadeIn: at 0x12 | Magic225Veil_FadeIn 532 |
| V55 | Magic225Veil_WaitBuffs: above 2 | Magic225Veil_WaitBuffs 465 |
| V56 | Magic225Veil_WaitBuffs: +0xB 0 | Magic225Veil_WaitBuffs 890 |
| V57 | Magic225Veil_End: +9 down by 3 | Magic225Veil_End 985 |
| V58 | Magic225Veil_End: +9 wraps | Magic225Veil_End 996 |
| V59 | Magic225Veil_End: the actor's layer 5 | Magic225Veil_End 500 |
| V60 | Magic225Veil_End: the side's layer 3 | Magic225Veil_End 428 |
| V61 | Magic225Veil_End: the owner's +0xA | Magic225Veil_End 500 |
| V62 | Magic225Veil_End: at 1 | Magic225Veil_End 505 |
| V63 | DrawBurst: the radius's low byte | Magic225Veil_DrawBurst 988 |
| V64 | DrawBurst: outer r + r / 4 | Magic225Veil_DrawBurst 380 |
| V65 | DrawBurst: outer r + (r >> 1) | Magic225Veil_DrawBurst 30 |
| V66 | DrawBurst: Scratch_Swap +9 x 8 | Magic225Veil_DrawBurst 16 |
| V67 | DrawBurst: colours 4 bytes apart | Magic225Veil_DrawBurst 1597 |
| V68 | DrawBurst: kind's low 7 bits | Magic225Veil_DrawBurst 336 |
| V69 | DrawBurst: green x +0xA | Magic225Veil_DrawBurst 1638 |
| V70 | DrawBurst: blue from the red | Magic225Veil_DrawBurst 1039 |
| V71 | DrawBurst: 31 triangles | Magic225Veil_DrawBurst 2000 |
| V72 | DrawBurst: triangles 0x81 apart | Magic225Veil_DrawBurst 2000 |
| V73 | DrawBurst: the centre's y from x | Magic225Veil_DrawBurst 2000 |
| V74 | DrawBurst: triangle green from red | Magic225Veil_DrawBurst 1982 |
| V75 | DrawBurst: triangle commit 0x30 | Magic225Veil_DrawBurst 2000 |
| V76 | DrawBurst: quads' outer ring the inner radius | Magic225Veil_DrawBurst 2000 |
| V77 | DrawBurst: quads 0x40 wide | Magic225Veil_DrawBurst 2000 |
| V78 | DrawBurst: quads' outer colour 2 | Magic225Veil_DrawBurst 2000 |
| V79 | DrawBurst: the last vertex's colour not set | Magic225Veil_DrawBurst 2000 |
| V80 | DrawBurst: quad commit 0x40 | Magic225Veil_DrawBurst 2000 |
| V81 | DrawBurst: quads' angle not carried | Magic225Veil_DrawBurst 2000 |
| V82 | DrawBurst: the red read once, before the fan | Magic225Veil_DrawBurst 1595 |
| V83 | DrawBurst: tpage 0x35 | Magic225Veil_DrawBurst 2000 |
| V84 | DrawBurst: the radius read before the call | Magic225Veil_DrawBurst 1916 |
| V85 | DrawShade: tpage by +0xB & 7 | Magic225Veil_DrawShade 1025 |
| V86 | DrawShade: semi-transparency 1 | Magic225Veil_DrawShade 1994 |
| V87 | DrawShade: grey x 14 | Magic225Veil_DrawShade 2000 |
| V88 | DrawShade: +0x24 at +0x28 | Magic225Veil_DrawShade 2000 |
| V89 | DrawShade: commit 0x34 | Magic225Veil_DrawShade 2000 |
| V90 | DrawShade: layer 2 first | Magic225Veil_DrawShade 1996 |
| V91 | DrawShade: blue 0 | Magic225Veil_DrawShade 1991 |
| V92 | Magic225Shard_Task: the veil's handler | Magic225Shard_Task 2000 |
| V93 | Magic225Shard_Run: steps 1/2 swapped | Magic225Shard_Run 1334 |
| V94 | Magic225Shard_Run: updated with +2 0 | Magic225Shard_Run 352 |
| V95 | Magic225Shard_Run: the blast's frame table | Magic225Shard_Run 689 |
| V96 | Magic225Shard_Launch: at 2 | Magic225Shard_Launch 484 |
| V97 | Magic225Shard_Launch: the record read again after the turn | Magic225Shard_Launch 78 |
| V98 | Magic225Shard_Launch: step 0x10001 | Magic225Shard_Launch 478 |
| V99 | Magic225Shard_Launch: height + 0x1000001 | Magic225Shard_Launch 467 |
| V100 | Magic225Shard_Launch: Ratan2 (dz, dx) | Magic225Shard_Launch 420 |
| V101 | Magic225Shard_Launch: Rand & 0x7F | Magic225Shard_Launch 121 |
| V102 | Magic225Shard_Launch: bit 1 of +0xB | Magic225Shard_Launch 225 |
| V103 | Magic225Shard_Launch: +0x26 0x81 | Magic225Shard_Launch 481 |
| V104 | Magic225Shard_Launch: +0x2A from +9 | Magic225Shard_Launch 223 |
| V105 | Magic225Shard_Launch: animation 1 | Magic225Shard_Launch 481 |
| V106 | Magic225Shard_Launch: speed 0xD | Magic225Shard_Launch 481 |
| V107 | Magic225Shard_Launch: +0x48 3 | Magic225Shard_Launch 481 |
| V108 | Magic225Shard_Launch: dz from the owner's x | Magic225Shard_Launch 481 |
| V109 | Magic225Shard_Fly: size + 0x801 | Magic225Shard_Fly 2000 |
| V110 | Magic225Shard_Fly: the speed of the task read before the call | Magic225Shard_Fly 57 |
| V111 | Magic225Shard_Fly: even frames | Magic225Shard_Fly 2000 |
| V112 | Magic225Shard_Fly: at 5 | Magic225Shard_Fly 473 |
| V113 | Magic225Shard_Fly: z by the sine | Magic225Shard_Fly 2000 |
| V114 | Magic225Shard_Fly: speed down by 2 | Magic225Shard_Fly 624 |
| V115 | Magic225Shard_Fall: x << 3 | Magic225Shard_Fall 2000 |
| V116 | Magic225Shard_Fall: no tick | Magic225Shard_Fall 2000 |
| V117 | Magic225Shard_Fall: +0x44 by 0x400 | Magic225Shard_Fall 2000 |
| V118 | Magic225Shard_Fall: at 1 | Magic225Shard_Fall 895 |
| V119 | Magic225Shard_Fall: the owner's count not down | Magic225Shard_Fall 437 |
| V120 | Magic225_ShardAlloc: 31 records | Magic225_ShardAlloc 15 |
| M1 | MeteorStrike_Task: entries 1/2 swapped | MeteorStrike_Task 1345 |
| M2 | MeteorStrike_Task: 47 records walked | MeteorStrike_Task 995 |
| M3 | MeteorStrike_Start: 47 records cleared | MeteorStrike_Start 2000 |
| M4 | MeteorStrike_Start: party below 2 | MeteorStrike_Start 224 |
| M5 | MeteorStrike_Start: enemies from 4 | MeteorStrike_Start 240 |
| M6 | MeteorStrike_Start: xor 1 | MeteorStrike_Start 1033 |
| M7 | MeteorStrike_Start: +9 0x11 | MeteorStrike_Start 1980 |
| M8 | MeteorStrike_Start: parameter 0x60 | MeteorStrike_Start 2000 |
| M9 | MeteorStrike_Start: sound 0x101 | MeteorStrike_Start 2000 |
| M10 | MeteorStrike_Start: 17 words of row 2 | MeteorStrike_Start 2000 |
| M11 | MeteorStrike_Start: row 2's first word with STP | MeteorStrike_Start 1000 |
| M12 | MeteorStrike_Start: row 26 STP 0x4000 | MeteorStrike_Start 2000 |
| M13 | MeteorStrike_Start: no screen point | MeteorStrike_Start 2000 |
| M14 | MeteorStrike_Start: direction from the owner | MeteorStrike_Start 1455 |
| M15 | MeteorStrike_Start: dirty byte not set | MeteorStrike_Start 1991 |
| M16 | MagicFx_CountDownFlag10: flags 0x11 | MagicFx_CountDownFlag10 477 |
| M17 | MagicFx_CountDownFlag10: at 1 | MagicFx_CountDownFlag10 1004 |
| M18 | MagicFx_CountDownFlag10: +2 on | MagicFx_CountDownFlag10 477 |
| M19 | MagicFx_CountDownFlag10: the actor flagged | MagicFx_CountDownFlag10 459 |
| M20 | MeteorStrikeRock_Task: the chip's handler | MeteorStrikeRock_Task 2000 |
| M21 | MeteorStrikeRock_Run: steps 2/3 swapped | MeteorStrikeRock_Run 809 |
| M22 | MeteorStrikeRock_Run: bit 1 of +0 | MeteorStrikeRock_Run 824 |
| M23 | MeteorStrikeRock_Run: the glow after the ring | MeteorStrikeRock_Run 784 |
| M24 | MeteorStrikeRock_Run: no pop | MeteorStrikeRock_Run 784 |
| M25 | MeteorStrikeRock_Run: the frame table not set | MeteorStrikeRock_Run 774 |
| M26 | MeteorStrikeRock_Run: no matrix push | MeteorStrikeRock_Run 784 |
| M27 | MeteorStrikeRock_Launch: height 0x9000001 | MeteorStrikeRock_Launch 2000 |
| M28 | MeteorStrikeRock_Launch: lift 0x20001 | MeteorStrikeRock_Launch 1993 |
| M29 | MeteorStrikeRock_Launch: step -0x1001 | MeteorStrikeRock_Launch 2000 |
| M30 | MeteorStrikeRock_Launch: fall 0x300001 | MeteorStrikeRock_Launch 2000 |
| M31 | MeteorStrikeRock_Launch: height over its own | MeteorStrikeRock_Launch 1771 |
| M32 | MeteorStrikeRock_Launch: x by the z step | MeteorStrikeRock_Launch 2000 |
| M33 | MeteorStrikeRock_Launch: +0x27 0x1B | MeteorStrikeRock_Launch 2000 |
| M34 | MeteorStrikeRock_Launch: layer 5 | MeteorStrikeRock_Launch 2000 |
| M35 | MeteorStrikeRock_Launch: +9 0x21 | MeteorStrikeRock_Launch 2000 |
| M36 | MeteorStrikeRock_Launch: +0xA 0x11 | MeteorStrikeRock_Launch 2000 |
| M37 | MeteorStrikeRock_Launch: the step not turned | MeteorStrikeRock_Launch 2000 |
| M38 | MeteorStrikeRock_Launch: animation 1 | MeteorStrikeRock_Launch 2000 |
| M39 | MeteorStrikeRock_Launch: direction from +9 | MeteorStrikeRock_Launch 1980 |
| M40 | MeteorStrikeRock_Fall: the height up by the fall | MeteorStrikeRock_Fall 2000 |
| M41 | MeteorStrikeRock_Fall: +0xB by 7 | MeteorStrikeRock_Fall 1932 |
| M42 | MeteorStrikeRock_Fall: every other frame | MeteorStrikeRock_Fall 279 |
| M43 | MeteorStrikeRock_Fall: trail +1 0 | MeteorStrikeRock_Fall 1071 |
| M44 | MeteorStrikeRock_Fall: trail +9 2 | MeteorStrikeRock_Fall 1071 |
| M45 | MeteorStrikeRock_Fall: trails counted in +0xB | MeteorStrikeRock_Fall 1229 |
| M46 | MeteorStrikeRock_Fall: trail owned by the owner | MeteorStrikeRock_Fall 943 |
| M47 | MeteorStrikeRock_Fall: at 1 | MeteorStrikeRock_Fall 1006 |
| M48 | MeteorStrikeRock_Fall: 31 chips | MeteorStrikeRock_Fall 520 |
| M49 | MeteorStrikeRock_Fall: chips owned by the rock | MeteorStrikeRock_Fall 515 |
| M50 | MeteorStrikeRock_Fall: the owner read before the alloc | MeteorStrikeRock_Fall 343 |
| M51 | MeteorStrikeRock_Fall: shades by i & 3 | MeteorStrikeRock_Fall 520 |
| M52 | MeteorStrikeRock_Fall: +0xB i & 3 | MeteorStrikeRock_Fall 520 |
| M53 | MeteorStrikeRock_Fall: +9 x 4 | MeteorStrikeRock_Fall 520 |
| M54 | MeteorStrikeRock_Fall: fades from the shades | MeteorStrikeRock_Fall 520 |
| M55 | MeteorStrikeRock_Fall: camera + 0x15 | MeteorStrikeRock_Fall 518 |
| M56 | MeteorStrikeRock_Fall: sound 0x100 | MeteorStrikeRock_Fall 520 |
| M57 | MeteorStrikeRock_Fall: +9 9 | MeteorStrikeRock_Fall 520 |
| M58 | MeteorStrikeRock_Fall: chip +1 1 | MeteorStrikeRock_Fall 520 |
| M59 | MeteorStrikeRock_Shake: bit 1 of +9 | MeteorStrikeRock_Shake 1016 |
| M60 | MeteorStrikeRock_Shake: up by 0x13 | MeteorStrikeRock_Shake 492 |
| M61 | MeteorStrikeRock_Shake: 0xFD57 | MeteorStrikeRock_Shake 507 |
| M62 | MeteorStrikeRock_Shake: +9 0x3D | MeteorStrikeRock_Shake 507 |
| M63 | MeteorStrikeRock_Shake: at 1 | MeteorStrikeRock_Shake 1030 |
| M64 | MeteorStrikeRock_Hide: 0x40 | MeteorStrikeRock_Hide 468 |
| M65 | MeteorStrikeRock_Hide: +0x5C 2 | MeteorStrikeRock_Hide 521 |
| M66 | MeteorStrikeRock_Hide: +0x5E kept | MeteorStrikeRock_Hide 519 |
| M67 | MeteorStrikeRock_Hide: at 1 | MeteorStrikeRock_Hide 995 |
| M68 | MeteorStrikeRock_End: +0xA down by 1 | MeteorStrikeRock_End 995 |
| M69 | MeteorStrikeRock_End: tint down 7 | MeteorStrikeRock_End 2000 |
| M70 | MeteorStrikeRock_End: at 0x88 | MeteorStrikeRock_End 446 |
| M71 | MeteorStrikeRock_End: +0x5F kept | MeteorStrikeRock_End 2000 |
| M72 | MeteorStrikeRock_End: the owner's +4 | MeteorStrikeRock_End 444 |
| M73 | DrawRing: layer 4 | MeteorStrikeRock_DrawRing 2000 |
| M74 | DrawRing: outer radius x 3 | MeteorStrikeRock_DrawRing 1972 |
| M75 | DrawRing: grey x 5 | MeteorStrikeRock_DrawRing 1809 |
| M76 | DrawRing: 63 quads | MeteorStrikeRock_DrawRing 2000 |
| M77 | DrawRing: steps of 0x41 | MeteorStrikeRock_DrawRing 2000 |
| M78 | DrawRing: v0's y not carried | MeteorStrikeRock_DrawRing 2000 |
| M79 | DrawRing: v2 not carried | MeteorStrikeRock_DrawRing 2000 |
| M80 | DrawRing: outer x by the inner radius | MeteorStrikeRock_DrawRing 2000 |
| M81 | DrawRing: v2's z not zeroed | MeteorStrikeRock_DrawRing 1907 |
| M82 | DrawRing: inside black 2 | MeteorStrikeRock_DrawRing 2000 |
| M83 | DrawRing: outer blue from 0x90385B | MeteorStrikeRock_DrawRing 1999 |
| M84 | DrawRing: projected at the textured stride | MeteorStrikeRock_DrawRing 2000 |
| M85 | DrawRing: commit 0x40 | MeteorStrikeRock_DrawRing 2000 |
| M86 | DrawRing: v1's y by the sine | MeteorStrikeRock_DrawRing 2000 |
| M87 | DrawRing: the first outer x not set | MeteorStrikeRock_DrawRing 1913 |
| M88 | MeteorStrikeRecord_Task: kinds swapped | MeteorStrikeRecord_Task 2000 |
| M89 | MeteorStrikeChip_Run: steps 1/2 swapped | MeteorStrikeChip_Run 1331 |
| M90 | MeteorStrikeChip_Run: no screen point | MeteorStrikeChip_Run 695 |
| M91 | MeteorStrikeChip_Run: +2 not tested | MeteorStrikeChip_Run 340 |
| M92 | MeteorStrikeChip_Launch: at 2 | MeteorStrikeChip_Launch 486 |
| M93 | MeteorStrikeChip_Launch: angle of +0xB - 3 | MeteorStrikeChip_Launch 483 |
| M94 | MeteorStrikeChip_Launch: x 25 | MeteorStrikeChip_Launch 483 |
| M95 | MeteorStrikeChip_Launch: the cosine's angle not read back | MeteorStrikeChip_Launch 8 |
| M96 | MeteorStrikeChip_Launch: lift by +0xB & 7 | MeteorStrikeChip_Launch 242 |
| M97 | MeteorStrikeChip_Launch: pull an eighth | MeteorStrikeChip_Launch 483 |
| M98 | MeteorStrikeChip_Launch: pull by a shift (equivalent: the lifts are positive multiples of 16) | not refused: equivalent (above) |
| M98b | MeteorStrikeChip_Launch: pull by a shift of 5 | MeteorStrikeChip_Launch 483 |
| M99 | MeteorStrikeChip_Launch: +0x5E 0x11 | MeteorStrikeChip_Launch 483 |
| M100 | MeteorStrikeChip_Launch: height from +0x38 | MeteorStrikeChip_Launch 483 |
| M101 | ChipDrift: x by scale + 1 | MeteorStrikeChip_Rise 2000, MeteorStrikeChip_Fall 2000 |
| M102 | ChipDrift: z by the sine | MeteorStrikeChip_Rise 2000, MeteorStrikeChip_Fall 2000 |
| M103 | ChipDrift: height by the pull | MeteorStrikeChip_Rise 2000, MeteorStrikeChip_Fall 2000 |
| M104 | ChipDrift: lift by +0x1C | MeteorStrikeChip_Rise 1545, MeteorStrikeChip_Fall 2000 |
| M105 | ChipDrift: the cosine's angle from the task | MeteorStrikeChip_Rise 94, MeteorStrikeChip_Fall 103 |
| M106 | MeteorStrikeChip_Rise: at 1 | MeteorStrikeChip_Rise 940 |
| M107 | MeteorStrikeChip_Rise: pull a sixteenth | MeteorStrikeChip_Rise 455 |
| M108 | MeteorStrikeChip_Rise: +9 0x11 | MeteorStrikeChip_Rise 455 |
| M109 | MeteorStrikeChip_Rise: scale 3 | MeteorStrikeChip_Rise 2000 |
| M110 | MeteorStrikeChip_Fall: scale 2 | MeteorStrikeChip_Fall 2000 |
| M111 | MeteorStrikeChip_Fall: +0x5E wraps | MeteorStrikeChip_Fall 965 |
| M112 | MeteorStrikeChip_Fall: at 1 | MeteorStrikeChip_Fall 934 |
| M113 | MeteorStrikeChip_Draw: linked at dy 3 | MeteorStrikeChip_Draw 2000 |
| M114 | MeteorStrikeChip_Draw: grey x 5 | MeteorStrikeChip_Draw 1799 |
| M115 | MeteorStrikeChip_Draw: grey unsigned | MeteorStrikeChip_Draw 913 |
| M116 | MeteorStrikeChip_Draw: radius from +9 | MeteorStrikeChip_Draw 1964 |
| M117 | MeteorStrikeChip_Draw: corner 0x700 | MeteorStrikeChip_Draw 2000 |
| M118 | MeteorStrikeChip_Draw: x by the sine | MeteorStrikeChip_Draw 2000 |
| M119 | MeteorStrikeChip_Draw: y from +0x2E | MeteorStrikeChip_Draw 2000 |
| M120 | MeteorStrikeChip_Draw: CLUT y 0x1E3 | MeteorStrikeChip_Draw 2000 |
| M121 | MeteorStrikeChip_Draw: v 0xA1 | MeteorStrikeChip_Draw 2000 |
| M122 | MeteorStrikeChip_Draw: red without the tint | MeteorStrikeChip_Draw 1994 |
| M123 | MeteorStrikeChip_Draw: second link 0x44 | MeteorStrikeChip_Draw 2000 |
| M124 | MeteorStrikeChip_Draw: tpage 0x55 | MeteorStrikeChip_Draw 2000 |
| M125 | MeteorStrikeChip_Draw: the radius read before the calls | MeteorStrikeChip_Draw 636 |
| M126 | MeteorStrikeTrail_Run: no matrix push | MeteorStrikeTrail_Run 692 |
| M127 | MeteorStrikeTrail_Run: steps 0/1 swapped | MeteorStrikeTrail_Run 1320 |
| M128 | MeteorStrikeTrail_Start: 0x3000001 below | MeteorStrikeTrail_Start 491 |
| M129 | MeteorStrikeTrail_Start: rise 0x60001 | MeteorStrikeTrail_Start 491 |
| M130 | MeteorStrikeTrail_Start: +0xA 0x11 | MeteorStrikeTrail_Start 491 |
| M131 | MeteorStrikeTrail_Start: direction from +9 | MeteorStrikeTrail_Start 481 |
| M132 | MeteorStrikeTrail_Start: at 1 | MeteorStrikeTrail_Start 977 |
| M133 | TrailFollow: x from the owner's z | MeteorStrikeTrail_Follow 2000, MeteorStrikeTrail_Fade 2000 |
| M134 | TrailFollow: sinking | MeteorStrikeTrail_Follow 2000, MeteorStrikeTrail_Fade 2000 |
| M135 | MeteorStrikeTrail_Follow: at 0x17 | MeteorStrikeTrail_Follow 1020 |
| M136 | MeteorStrikeTrail_Fade: down by 3 | MeteorStrikeTrail_Fade 2000 |
| M137 | MeteorStrikeTrail_Fade: the owner's +0xB | MeteorStrikeTrail_Fade 443 |
| M138 | MeteorStrikeTrail_Fade: +9 not up | MeteorStrikeTrail_Fade 1998 |
| M139 | MeteorStrikeTrail_Draw: outer x 25 | MeteorStrikeTrail_Draw 1917 |
| M140 | MeteorStrikeTrail_Draw: inner x 16 | MeteorStrikeTrail_Draw 1961 |
| M141 | MeteorStrikeTrail_Draw: red x 13 | MeteorStrikeTrail_Draw 1806 |
| M142 | MeteorStrikeTrail_Draw: blue x 7 | MeteorStrikeTrail_Draw 1785 |
| M143 | MeteorStrikeTrail_Draw: z -0x41 | MeteorStrikeTrail_Draw 1879 |
| M144 | MeteorStrikeTrail_Draw: 31 quads | MeteorStrikeTrail_Draw 2000 |
| M145 | MeteorStrikeTrail_Draw: link x << 8 | MeteorStrikeTrail_Draw 1999 |
| M146 | MeteorStrikeTrail_Draw: link z from +0x34 | MeteorStrikeTrail_Draw 2000 |
| M147 | MeteorStrikeTrail_Draw: outer x not read back | MeteorStrikeTrail_Draw 548 |
| M148 | MeteorStrikeTrail_Draw: linked at dy 1 | MeteorStrikeTrail_Draw 2000 |
| M149 | MeteorStrikeTrail_Draw: green from 0x90385A | MeteorStrikeTrail_Draw 2000 |
| M150 | MeteorStrikeTrail_Draw: last link 0x44 | MeteorStrikeTrail_Draw 2000 |
| M151 | MeteorStrikeTrail_Draw: v1's y not carried | MeteorStrikeTrail_Draw 2000 |
| M152 | MeteorStrikeTrail_Draw: the first inner y not set | MeteorStrikeTrail_Draw 1870 |
| M153 | MeteorStrike_RecordAlloc: 47 records | MeteorStrike_RecordAlloc 13 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Things
to look for, by reading:

- Tempest / Hurricane: a screen flash from the caster's side and 48 gusts
  sweeping off at an angle by the caster's direction; the two ids differ
  only in the gusts' CLUT row.
- The unlabelled id: the screen shaded, four coloured bursts at the screen's
  corners, sixteen shards flying from the caster to the side's centre, then
  three stat popups over each live actor of the side.
- MeteorStrike: a rock coming down over the target side with a trail,
  breaking into 32 chips, the camera shaking, the rock fading.

`Magic225Veil_Start`'s layer 7 is reached only in the event battle of kind
0x37; which fight that is was not read.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the three stack tables and the
  thirteen `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Tempest_Start`, `Magic225_Start` and `MeteorStrike_Start`: slot 255 is
  `0x93A000 + 255 x 0x84 = 0x9423FC`, past the image's end - an access
  violation, in ours as in the original. Only `Magic225_Apply` tests it (and
  there the stat is applied before the create, so a full table loses the
  popup, not the buff).
- **`TempestGust_Launch` reads `TempestGust_Angles` by the owner's direction
  unchecked**: a direction past 3 reads the next table's bytes (a code
  pointer of `Magic225Veil_TaskTable`, then the burst colours) as an angle.
  Faithful in ours (read in place); a direction past 3 is not expected.
- **`Magic225_Spawn` plays sound 0x100 sixteen times in one frame**, one
  before each shard. Whether the sound layer merges them was not measured.
- **Each overlay's pool is cleared by its own start**: a second cast of the
  same overlay while the first still runs would clear the first's records,
  whose owner then never sees its count reach 0 (a wait without end). Whether
  two casts can overlap was not measured.
- `Tempest_End` sets 0x2000 in the effect word beside the done bit; what reads
  that bit was not traced.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 50 lines under a `group S38` comment: 44 new,
plus six host extents re-listed smaller (`004F89E0 12`, `004F8EE0 57`,
`004F9A40 12`, `004F9D80 57`, `004FA670 12`, `004FAF90 57`, each the
function's own size). Four were listed right already (`004F9610`,
`004F9980`, `004FA440`, `004FAD00`).
