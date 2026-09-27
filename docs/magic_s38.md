# Group S38: Tempest / Hurricane, an unlabelled id and MeteorStrike (MAGIC223, 225, 226/227)

**Status:** IN PROGRESS (2026-09-27). All 54 functions are ours
(`src/game/magic_s38.cpp`, shadow name `magic_s38`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 108,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
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

    shadow      magic_s38 self-test: 108000 rounds over 54 functions (2000 each), 3663402 calls to the stand-ins,
                0 MISMATCHES; 40680 bytes of state (22 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (3,665,474 stand-in
calls for this group in that run: the harness's pointers into the DLL move a
few branches, 0 mismatches).

## 6. Controls

CONTROLS_TEXT

CONTROLS_TABLE

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
