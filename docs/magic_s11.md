# Group S11: Sanctuary and Tornado (MAGIC058, MAGIC059)

**Status:** IN PROGRESS (2026-09-26). All 35 functions are ours
(`src/game/magic_s11.cpp`, shadow name `magic_s11`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 70,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, spell wave four, group S11
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 82 | 0x250 | MAGIC058 | 0x3A | Sanctuary | `0x4AF040..0x4AFB76` | 17 |
| 83 | 0x251 | MAGIC059 | 0x3B | Tornado | `0x4AFB80..0x4B0D42` | 18 |

The extents are `tools/magic_rows.py --unit MAGIC058 / MAGIC059 --clones`
(capstone recursive descent; no jump table; nothing `REFUSED`). All 35
functions lie in the units' extents; none was found inside or missing from
them, and none was ours before. 7,142 bytes, as the queue counted. Every
handler the tables hold is covered by the tool's lines and by the coverage
line of the fuzz (section 5).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What each spell looks
like in play has not been measured; the outlines below say what the code
draws.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Sanctuary (MAGIC058).**
  - `Sanctuary_Task`, the kind-2 task: a three-entry stack table by +1, then
    every live record of the effect's own mote pool (36 records of 0x84 at
    `0x680780`, `SanctuaryMote_Pool`) run through `SanctuaryMote_Dispatch`,
    with `Sprite_Current` and the owner cell `0x93B940` switched to the record
    and its +0x80 for the call and put back after it.
  - `Sanctuary_Start` empties the pool and the eleven reached bytes
    (`Sanctuary_Reached`, one per battle index), takes the owner's facing and
    position, creates the ring (kind 1, parameter 0x3F) and plays sound 0x100.
  - `Sanctuary_Message` waits for the count +0xB (the ring and the motes) to
    reach 0, then queues the battle message `Msg_SystemPtr(0x14)`
    (`BattleQueue_Push(1, 0x3C, text)`).
  - `Sanctuary_End` waits for the message window to close, then sets the
    target's flag 0x40 and the done flag and frees the task.
  - **The ring** (`SanctuaryRing_*`): a one-entry `.data` dispatch, then a
    three-entry stack table by +2. `_Start` takes the owner's position, radius
    8. `_Spread` widens it by 0x20 a frame; each actor - enemies (battle index
    3..10) first, then the party - that is not yet reached, not out
    (`Battle_ActorIsOut`), within the ring's radius (MAGIC218's `0x4F5970`)
    and not the acting actor gets two motes (delays 1 and 13) from
    `SanctuaryMote_Alloc` and is marked reached. Past radius 0x600 the ring
    ends through `MagicFx_UncountAndFree` (the owner's count down, the task
    freed; ten files' tables hold it). `_Draw` draws 64 semi-transparent
    gouraud quads round the ring, shaded 0x60 outside and 1 inside, between
    two draw modes (tpages 0x35 and 0x15).
  - **The mote** (`SanctuaryMote_*`): a one-entry `.data` dispatch, then a
    three-entry stack table by +2. `_Wait` counts its delay down, then takes
    its actor's position (+0xB: the party record below 3, else the enemy's)
    and sets radius and step 0x10. It moves +2 from 0 to 2, so `_Slow` (entry
    1) is never reached. `_Fade` widens the ring by a shrinking step, and on
    odd frames counts +0xA down; at 0 the owner's count goes down and
    MAGIC219's `0x4F6290` frees the record. `_Draw` is the ring's draw with
    16 quads, its outer shade +0xA x 6.
- **Tornado (MAGIC059).**
  - `Tornado_Task`, the kind-2 task: a two-entry stack table by +1, the
    screen point (`BattleActor_UpdateScreenXY`), then every live record of its
    own pool (24 records at `0x681A20`, `TornadoMote_Pool`) run through
    `TornadoMote_Dispatch`, as Sanctuary's.
  - `Tornado_Start` empties the pool, creates the funnel (kind 1, parameter
    0x40), then three rings of eight motes (angles 8 i, 8 i + 2, 8 i + 4;
    kinds 3, 2, 1; radius 0x80, 0xC0, 0x100; heights 0x800000, 0x400000, 0):
    exactly the pool's 24. It gives CLUT row 26's first 16 cells the
    semi-transparency bit (cell 0 without), sets battle flag byte `0x904AA9`
    bit 1, and places the task: the acting actor's facing (+8 of the record at
    `0x904B3C`); an offset (0x40000, 0) turned by it (the engine's
    `0x446770`) and added to the point `Field_Kind2X / Z` as the centre;
    `Tornado_AverageHeight`; sound 0x100; +9 from `Tornado_FacingPhase` by
    the facing.
  - `Tornado_Spin` steps +9 and circles the task round its centre (+0x18 /
    +0x1C plus `Math_Sin` / `Math_Cos` of +9 << 4, shifted left 6). When the
    count +0xB is 0 it sets the
    target's flag 0x40 and the done flag and frees the task.
  - `Tornado_AverageHeight` sets the task's height +0x3E to the average of
    the live actors' heights on the side opposite the acting actor.
  - **The funnel** (`TornadoFunnel_*`): a one-entry `.data` dispatch, then
    `TornadoFunnel_Phases` (`_Reset`, `_Grow`, `TornadoFx_Hold` and group
    S07's `EnlightenRays_Fade`) by +2. While it lives it pushes its own matrix
    (`_PushMatrix`: the owner's position and height, no rotation) and draws
    sixteen bands (`_DrawBand(angle, radius)`, eight of radius 0x20 and eight
    of 0x50 at angles turning with +9). A band is 30 semi-transparent gouraud
    quads over three rings of points that step out, up and round, each point
    bobbed by a cosine of the frame.
  - **The mote** (`TornadoMote_*`): a one-entry `.data` dispatch, then
    `TornadoMote_Phases` (`_Place`, `_Rise`, `TornadoFx_Hold`, `_Fade`) by
    +2. While it lives it orbits its owner (angle +9 x 3, radius +0xC with a
    frame wobble), bobs, and draws one textured semi-transparent quad round
    its screen point (`_Draw`: tpage (0, 1, 0x340, 0x100), clut (0, 0x1FA),
    shade +0xB x +0xA).

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedents:

- a phase past any of the four stack tables aborts
  ([`magic_fx_reached.md`](magic_fx_reached.md) §3). The six `.data`
  dispatch tables are read in place, their indexes unchecked, as the
  originals read them;
- `Tornado_AverageHeight` aborts where the original divides by a count of 0
  live actors (the owner's call, round9 doc §6).

Calls that push one argument more than the callee takes, as the originals
do, push it in ours too: `Gte_RotTrans` gets a flag pointer, the projections
a depth and a flag pointer.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F5970` | MAGIC218 (S36) | `SanctuaryRing_Spread`: 1 (whole eax) when the record lies within word `0x903850` of `Sprite_Current` |
| `0x4F6290` | MAGIC219 (S37) | `SanctuaryMote_Fade`, `TornadoMote_Fade`: the record freed (+0..+4 cleared), a tail jmp |
| `0x446770` | engine, unnamed | `Tornado_Start`: the dx / dz turn by direction (as S22, S23, S31 call it) |

By name, already ours: `EnlightenRays_Fade` (S07, in
`TornadoFunnel_Phases`), `MagicFx_PushActorMatrix` (L), and the GTE / GPU /
battle / sound library.

**Called by other groups by address:** `0x4AF490`, now
`MagicFx_UncountAndFree`, is a table entry or a direct call in S03, S04, S07,
S26 and S31 (their `kFreeOwnerCount`). They reach it through the address, so
taking it changes nothing for them; they can be rebound to the name.

## 4. Named data (`symbols.toml` `[[data]]`)

| Name | Address | Count |
|---|---|--:|
| `SanctuaryMote_Pool` | `0x680780` | 36 records of 0x84 |
| `Sanctuary_Reached` | `0x681A10` | 11 bytes |
| `TornadoMote_Pool` | `0x681A20` | 24 records of 0x84 |
| `SanctuaryRing_Types` | `0x65AA68` | 1 |
| `SanctuaryMote_Types` | `0x65AA6C` | 1 |
| `Tornado_FacingPhase` | `0x65AA70` | 4 dwords (a byte used of each) |
| `TornadoFunnel_Types` | `0x65AA80` | 1 |
| `TornadoFunnel_Phases` | `0x65AA84` | 4 |
| `TornadoMote_Types` | `0x65AA94` | 1 |
| `TornadoMote_Phases` | `0x65AA98` | 4 |

Each count is where the next table starts (the words `0x65AA60..0x65AABC`
read 2026-09-26); `0x65AA70` holds data, not handlers, and is not swapped by
the fuzz. The two pools and the reached bytes are consecutive `.bss`
(`0x680780..0x68267F`), named here for the first time.

## 5. The fuzz

`BOF3X_SHADOW=magic_s11` runs `magic_harness::Run` over the 35 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s11_fuzz.cpp`:

- **Callees** (32 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own - every primitive of a draw is built at the same pointer, so without
    the log only the last would be compared;
  - `Gte_RotTransPers4` logs its four SVECTORs through `deref` (6 bytes
    each); the matrix push's GTE callees log theirs and write a result where
    the real ones write (S22's effects);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - `0x4F5970` answers `kBool` (its callers test eax whole) and logs the
    radius word and `Sprite_Current`'s point it reads;
  - `Battle_ActorIsOut` (`kFlag`) keeps one actor of each side in while
    `Tornado_AverageHeight` is fuzzed, so no round divides by zero;
  - the two allocators answer an index inside their pool (`kByte`; their
    callers never test 0xFF); their clones compare their answer
    (`ret_mask 0xFF`);
  - this group's own draws, pushes, dispatches and the height, by address
    (`kPhase`: logged as the task they run for), and the band with its two
    arguments.
- **Tables:** the three runs of section 4's handler tables (`0x65AA68` 2,
  `0x65AA80` 5, `0x65AA94` 5).
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `Field_Kind2Z` /
  `Field_Kind2X`; both pools and the reached bytes; CLUT row 26 and its
  source. 28,572 bytes of state in 16 regions.
- **Seed:** every pool record's owner +0x80 a real sprite (the runners make
  it the owner cell, which the disturbance writes through); `0x904B3C` at
  one of the harness's sprite records; the acting actor 0..10 two times in
  three; each dispatcher inside its table; the runners' pools with random
  in-use bits; the allocators' pools full to a random depth, full included;
  each count-down one step before and at its threshold (the mote's +9 1,
  +0xA 1, the step 0x10, the funnel's 0x20, the hold's 0xA0, the rise's
  0x20); the ring's radius at 0x5E0 / 0x5E1 (one either side of 0x600 after
  the step) and the reached bytes 0 two times in three; the mote's actor
  0..10; the facing 0..3; the actor 2 or 3 for the height; the band's
  arguments its caller's two times in three (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, `Field_Kind2*`, the `0x904B3C` record, a pool record's in-use bit
  (the runners test it after each call), a reached byte.

Result in this worktree (2026-09-26):

    shadow      magic_s11 self-test: 70000 rounds over 35 functions (2000 each), 2302922 calls to the stand-ins,
                0 MISMATCHES; 28572 bytes of state (16 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`), `SanctuaryMote_Slow` included (the
fuzz enters it directly and through the table). `BOF3X_SHADOW='*'`: exit 0
(2,301,892 stand-in calls for this group in that run, 0 mismatches).

## 6. Controls

CONTROLS_TEXT

## 7. What nothing reached

No recorded route casts either spell (queue §5); the live check is the owner
casting them, with a save that has them or DIV-0045's cheat. By reading:

- Sanctuary: a flat ring widening from the caster; motes dropping onto each
  actor it reaches (either side, not the caster) and fading as small rings;
  then a battle message (system text 0x14).
- Tornado: a turning funnel of bands at the caster, 24 motes orbiting it in
  three rings, the effect's task circling a point beside the caster.

`SanctuaryMote_Slow` is reached by nothing in the game (section 1).

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the four stack tables (ours
  aborts) and the six `.data` tables (an index past a one-entry table reads
  the next table; ours reads it as the original does).
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Sanctuary_Start` and `Tornado_Start`: slot 255's +0x80 is `0x9423FC`,
  past the image's end - an access violation, in ours as in the original.
- **The allocators' "none free" (0xFF) is unchecked** by their callers:
  record 255 of either pool lies 0x837C bytes past its start
  (`0x688AFC`, `0x689D9C`), inside other `.bss`, which the caller would
  overwrite silently. Not reachable as written: `Tornado_Start` empties its
  pool of 24 and takes exactly 24; `SanctuaryRing_Spread` takes at most 22
  (two per actor, each actor once) from an emptied 36.
- **`SanctuaryMote_Wait` indexes the enemy records by +0xB - 3**, unchecked;
  +0xB is only ever set to 0..10 by `SanctuaryRing_Spread`.
- **`Tornado_AverageHeight` divides by the count of live actors** on the
  opposite side (section 2; ours aborts at 0).
- **`Tornado_Start` reads `Tornado_FacingPhase` by the facing, unchecked**
  (four entries).

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 29 lines: each function's own extent, four of
them re-listing a host extent smaller (`004AF4A0 12` under `004AF4A0 1FD`,
`004AFB20 57` under `004AFB20 64A`, `004B0800 12` under `004B0800 1F4`,
`004B0CB0 93` under `004B0CB0 238`); six were listed right already
(`004AF6A0`, `004AF8D0`, `004B0170`, `004B0250`, `004B0A00`, `004B0C50`).
