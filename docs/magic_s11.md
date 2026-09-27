# Group S11: Sanctuary and Tornado (MAGIC058, MAGIC059)

**Status:** IN PROGRESS (2026-09-26). All 35 functions are ours
(`src/game/magic_s11.cpp`, shadow name `magic_s11`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 70,000 rounds. 226 of 226 negative controls refused (223 by a count). Nothing recorded casts
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
  word (half the time the angle word `0x90385E`), `Field_Kind2*`, the `0x904B3C` record, a pool record's in-use bit
  (the runners test it after each call), a reached byte.

Result in this worktree (2026-09-26):

    shadow      magic_s11 self-test: 70000 rounds over 35 functions (2000 each), 2302922 calls to the stand-ins,
                0 MISMATCHES; 28572 bytes of state (16 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`), `SanctuaryMote_Slow` included (the
fuzz enters it directly and through the table). `BOF3X_SHADOW='*'`: exit 0
(2,301,892 stand-in calls for this group in that run, 0 mismatches).

## 6. Controls

226 plants, each put in `magic_s11.cpp` one at a time by a script (not committed; the round's pattern, `run_controls.py` with `controls.py`) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s11`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches, exit 0).

**226 of 226 refused**: 223 by a count (exit 3, a count only in the functions the plant touches); 2 (V2, V4) by our own zero-count abort in `Tornado_AverageHeight` (the plant left every actor it tests out while the original still counted one), each with a near variant refused by a count (V2b, V4b); 1 (H1) by a fault (the plant made the owner cell a random word, which the disturbance writes through), with a near variant refused by a count (H1b). No equivalent mutant was planted; none was left standing.

This is the second run. The first (223 plants, before V2b, V4b and H1b)
refused T33 - `Tornado_Spin` reading the angle word back after its call - in
one round of 2,000; the group's disturbance then hit the angle word 0x90385E
one time in sixteen, and now does half the time it moves a scratch word. The
whole set was re-run on that fuzz; T33 went to 5 rounds, the rest stayed
refused.

The letters: H the shared helpers, S Sanctuary's task, R the ring, M the Sanctuary mote, D the ring draws, T Tornado's task, F the funnel, B the band, N the Tornado mote, Q its quad, A its allocator, V the height.

The thinnest (fewer than 100 rounds):

- **T33** (Spin: the angle not read back): 5
- **M15** (Fade: freed before the count): 9
- **N8** (Mote_Task: the cell taken after the call): 20
- **M4** (Wait: party below 2): 35
- **D24** (Mote_Alloc: 35 motes): 48
- **S11** (Start: the ring's owner read before the call): 64
- **D4** (RingStep: the inner edge read before the call): 65
- **H6** (PoolAlloc: none is 0xFE): 74
- **A1** (Mote_Alloc: 23): 79

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| H1 | RunPool: the record's +0x7C as owner | a fault (exit 3221225477: an access violation) |
| H2 | RunPool: owner not put back | Sanctuary_Task 1,609; Tornado_Task 1,610 |
| H3 | RunPool: in-use bit 2 | Sanctuary_Task 2,000; Tornado_Task 2,000 |
| H4 | RunPool: Sprite_Current not put back | Sanctuary_Task 1,977; Tornado_Task 1,970 |
| H5 | PoolAlloc: marks bits 0 and 1 | SanctuaryMote_Alloc 1,003; TornadoMote_Alloc 1,001 |
| H6 | PoolAlloc: none is 0xFE | SanctuaryMote_Alloc 57; TornadoMote_Alloc 74 |
| H7 | PoolAlloc: tests bit 1 | SanctuaryMote_Alloc 1,941; TornadoMote_Alloc 1,911 |
| H8 | MarkDoneAndFree: flag bit 3 | Sanctuary_End 694; Tornado_Spin 335 |
| H9 | MarkDoneAndFree: the actor's flag | Sanctuary_End 869; Tornado_Spin 410 |
| H10 | Mul12: sar 11 | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000; TornadoFunnel_DrawBand 2,000; TornadoMote_Draw 2,000 |
| H11 | Shade: byte 7 for 6 | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000; TornadoFunnel_DrawBand 2,000 |
| H12 | LinkAtCurrent: z from +0x3C | TornadoFunnel_DrawBand 2,000; TornadoMote_Draw 2,000 |
| H13 | DrawMode: dtd 0 | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000; TornadoFunnel_DrawBand 2,000; TornadoMote_Draw 2,000 |
| H14 | Rtp4: vertices 2 and 3 swapped | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000; TornadoFunnel_DrawBand 2,000 |
| S1 | Sanctuary_Task: Start / Message swapped | Sanctuary_Task 1,345 |
| S2 | Sanctuary_Task: pool of 35 | Sanctuary_Task 660 |
| S3 | Sanctuary_Task: the Tornado pool's dispatch | Sanctuary_Task 2,000 |
| S4 | Start: +2 not cleared | Sanctuary_Start 2,000 |
| S5 | Start: reached byte 11 for 10 | Sanctuary_Start 2,000 |
| S6 | Start: facing from the owner's +9 | Sanctuary_Start 1,966 |
| S7 | Start: parameter 0x3E | Sanctuary_Start 2,000 |
| S8 | Start: the ring's +1 1 | Sanctuary_Start 2,000 |
| S9 | Start: sound 0x101 | Sanctuary_Start 2,000 |
| S10 | Start: +0xB 1 | Sanctuary_Start 1,987 |
| S11 | Start: the ring's owner read before the call | Sanctuary_Start 64 |
| S12 | Message: text 0x15 | Sanctuary_Message 529 |
| S13 | Message: queue 0x3D | Sanctuary_Message 529 |
| S14 | Message: waits on +0xA | Sanctuary_Message 531 |
| S15 | End: ends while the window is up | Sanctuary_End 2,000 |
| R1 | Ring_Dispatch: the mote table | SanctuaryRing_Dispatch 2,000 |
| R2 | Ring_Task: Start / Spread swapped | SanctuaryRing_Task 1,305 |
| R3 | Ring_Task: draws by +1 | SanctuaryRing_Task 995 |
| R4 | Ring_Task: the mote's draw | SanctuaryRing_Task 1,025 |
| R5 | Ring_Start: radius 9 | SanctuaryRing_Start 2,000 |
| R6 | Ring_Start: +0xA 0x11 | SanctuaryRing_Start 2,000 |
| R7 | Ring_Start: height from +0x38 | SanctuaryRing_Start 2,000 |
| R8 | Spread: step 0x21 | SanctuaryRing_Spread 2,000 |
| R9 | Spread: reach from +0xE | SanctuaryRing_Spread 1,997 |
| R10 | Spread: seven enemies | SanctuaryRing_Spread 1,355 |
| R11 | Spread: enemies' reached byte ignored | SanctuaryRing_Spread 1,911 |
| R12 | Spread: the next enemy's record tested | SanctuaryRing_Spread 1,747 |
| R13 | Spread: enemies skip the actor + 1 | SanctuaryRing_Spread 274 |
| R14 | Spread: enemies marked 0xFE | SanctuaryRing_Spread 1,378 |
| R15 | Spread: party's out test dropped | SanctuaryRing_Spread 1,930 |
| R16 | Spread: party skips the actor + 1 | SanctuaryRing_Spread 246 |
| R17 | Spread: two members | SanctuaryRing_Spread 1,316 |
| R18 | Spread: on at 0x600 | SanctuaryRing_Spread 342 |
| R19 | SendMotes: second delay 12 | SanctuaryRing_Spread 1,609 |
| R20 | SendMotes: +0xB actor + 1 | SanctuaryRing_Spread 1,609 |
| R21 | SendMotes: +1 1 | SanctuaryRing_Spread 1,609 |
| R22 | SendMotes: the owner's count down | SanctuaryRing_Spread 1,587 |
| R23 | SendMotes: owner read before the call | SanctuaryRing_Spread 197 |
| R24 | SendMotes: +9 delay + 1 | SanctuaryRing_Spread 1,609 |
| R25 | UncountAndFree: count up | MagicFx_UncountAndFree 1,987 |
| M1 | Mote_Dispatch: the ring table | SanctuaryMote_Dispatch 2,000 |
| M2 | Mote_Task: Wait / Slow swapped | SanctuaryMote_Task 1,295 |
| M3 | Mote_Task: draws at +2 0 | SanctuaryMote_Task 347 |
| M4 | Wait: party below 2 | SanctuaryMote_Wait 35 |
| M5 | Wait: height from +0x38 | SanctuaryMote_Wait 493 |
| M6 | Wait: step 0x11 | SanctuaryMote_Wait 493 |
| M7 | Wait: +2 on by one | SanctuaryMote_Wait 493 |
| M8 | Wait: lands at 1 | SanctuaryMote_Wait 1,013 |
| M9 | Wait: radius 0x11 | SanctuaryMote_Wait 493 |
| M10 | Wait: +0xA 0xF | SanctuaryMote_Wait 493 |
| M11 | Slow: on at 0x11 | SanctuaryMote_Slow 464 |
| M12 | Slow: step down 2 | SanctuaryMote_Slow 2,000 |
| M13 | Fade: even frames | SanctuaryMote_Fade 2,000 |
| M14 | Fade: step not down | SanctuaryMote_Fade 988 |
| M15 | Fade: freed before the count | SanctuaryMote_Fade 9 |
| M16 | Fade: radius by +0x14 | SanctuaryMote_Fade 2,000 |
| D1 | FirstEdge: cos for the outer x | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000 |
| D2 | FirstEdge: inner y by the outer radius | SanctuaryRing_Draw 1,995; SanctuaryMote_Draw 1,992 |
| D3 | RingStep: z 1 | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000 |
| D4 | RingStep: the inner edge read before the call | SanctuaryRing_Draw 65; SanctuaryMote_Draw 21 |
| D5 | RingStep: x / y swapped | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000 |
| D6 | RingStep: last z 1 | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000 |
| D7 | RingStep: the new outer y by the inner radius | SanctuaryRing_Draw 2,000; SanctuaryMote_Draw 2,000 |
| D8 | Ring_Draw: outer + 0x7F | SanctuaryRing_Draw 2,000 |
| D9 | Ring_Draw: tpage 0x36 | SanctuaryRing_Draw 2,000 |
| D10 | Ring_Draw: 63 quads | SanctuaryRing_Draw 2,000 |
| D11 | Ring_Draw: outer shade 0x61 | SanctuaryRing_Draw 2,000 |
| D12 | Ring_Draw: commit slot 4 | SanctuaryRing_Draw 2,000 |
| D13 | Ring_Draw: closing tpage 0x16 | SanctuaryRing_Draw 2,000 |
| D14 | Ring_Draw: opaque | SanctuaryRing_Draw 2,000 |
| D15 | Ring_Draw: inner shade 2 | SanctuaryRing_Draw 2,000 |
| D16 | Ring_Draw: depths before the projection | SanctuaryRing_Draw 2,000 |
| D17 | Mote_Draw: shade x 5 | SanctuaryMote_Draw 1,984 |
| D18 | Mote_Draw: outer + 0x41 | SanctuaryMote_Draw 2,000 |
| D19 | Mote_Draw: 15 quads | SanctuaryMote_Draw 2,000 |
| D20 | Mote_Draw: shade from byte 5 | SanctuaryMote_Draw 1,992 |
| D21 | Mote_Draw: second outer shade 1 | SanctuaryMote_Draw 2,000 |
| D22 | Mote_Draw: inner shade 2 | SanctuaryMote_Draw 2,000 |
| D23 | Mote_Draw: commit size 0x40 | SanctuaryMote_Draw 2,000 |
| D24 | Mote_Alloc: 35 motes | SanctuaryMote_Alloc 48 |
| T1 | Tornado_Task: Start / Spin swapped | Tornado_Task 2,000 |
| T2 | Tornado_Task: the screen point after the pool | Tornado_Task 2,000 |
| T3 | Tornado_Task: pool of 23 | Tornado_Task 664 |
| T4 | Start: +1 not cleared | Tornado_Start 2,000 |
| T5 | Start: parameter 0x41 | Tornado_Start 2,000 |
| T6 | Start: ring A kind 4 | Tornado_Start 1,991 |
| T7 | Start: ring B radius 0xC1 | Tornado_Start 2,000 |
| T8 | Start: ring C height 1 | Tornado_Start 2,000 |
| T9 | Start: ring C angle 5 | Tornado_Start 2,000 |
| T10 | SpawnRing: angles 4 apart | Tornado_Start 2,000 |
| T11 | SpawnRing: radius at +0x10 | Tornado_Start 2,000 |
| T12 | SpawnRing: +1 1 | Tornado_Start 2,000 |
| T13 | SpawnRing: seven a ring | Tornado_Start 2,000 |
| T14 | SpawnRing: owner read before the call | Tornado_Start 1,059 |
| T15 | Start: 15 CLUT cells | Tornado_Start 2,000 |
| T16 | Start: CLUT bit 14 | Tornado_Start 2,000 |
| T17 | Start: cell 0 keeps the bit | Tornado_Start 1,024 |
| T18 | Start: battle flag bit 2 | Tornado_Start 1,499 |
| T19 | Start: strip not dirty | Tornado_Start 2,000 |
| T20 | Start: facing from +9 | Tornado_Start 1,972 |
| T21 | Start: offset 0x40001 | Tornado_Start 1,989 |
| T22 | Start: dz 1 | Tornado_Start 2,000 |
| T23 | Start: X / Z swapped | Tornado_Start 2,000 |
| T24 | Start: sin shl 5 | Tornado_Start 2,000 |
| T25 | Start: cos of 1 | Tornado_Start 2,000 |
| T26 | Start: sound 0x101 | Tornado_Start 2,000 |
| T27 | Start: facing table byte 1 | Tornado_Start 1,605 |
| T28 | Start: +2 on for +1 | Tornado_Start 2,000 |
| T29 | Start: the mote's draw for the height | Tornado_Start 2,000 |
| T30 | Start: the owner turned | Tornado_Start 1,747 |
| T31 | Start: centre z from +0xC | Tornado_Start 2,000 |
| T32 | Spin: angle shl 3 | Tornado_Spin 1,997 |
| T33 | Spin: the angle not read back | Tornado_Spin 5 |
| T34 | Spin: z about +0x18 | Tornado_Spin 2,000 |
| T35 | Spin: ends at +0xA 0 | Tornado_Spin 458 |
| T36 | Spin: +9 not up | Tornado_Spin 2,000 |
| F1 | Funnel_Dispatch: the mote table | TornadoFunnel_Dispatch 2,000 |
| F2 | Funnel_Task: the mote phases | TornadoFunnel_Task 1,465 |
| F3 | Funnel_Task: draws at +2 0 | TornadoFunnel_Task 228 |
| F4 | Funnel_Task: inner radius 0x21 | TornadoFunnel_Task 795 |
| F5 | Funnel_Task: outer angles not doubled | TornadoFunnel_Task 795 |
| F6 | Funnel_Task: inner k 0x1C | TornadoFunnel_Task 795 |
| F7 | Funnel_Task: outer k 0xF | TornadoFunnel_Task 795 |
| F8 | Funnel_Task: +9 read once | TornadoFunnel_Task 170 |
| F9 | Funnel_Task: no pop | TornadoFunnel_Task 795 |
| F10 | Reset: +0xA 1 | TornadoFunnel_Reset 2,000 |
| F11 | Grow: by 3 | TornadoFunnel_Grow 2,000 |
| F12 | Grow: on at 0x21 | TornadoFunnel_Grow 673 |
| F13 | Hold: on at 0x9F | TornadoFx_Hold 1,043 |
| F14 | PushMatrix: height from +0x3C | TornadoFunnel_PushMatrix 2,000 |
| F15 | PushMatrix: x sar 8 | TornadoFunnel_PushMatrix 2,000 |
| F16 | PushMatrix: height / 4 | TornadoFunnel_PushMatrix 2,000 |
| F17 | PushMatrix: a z turn | TornadoFunnel_PushMatrix 2,000 |
| F18 | PushMatrix: z - 0x3FFF | TornadoFunnel_PushMatrix 2,000 |
| F19 | PushMatrix: y the owner's +0x3C | TornadoFunnel_PushMatrix 2,000 |
| F20 | PushMatrix: camera + 1 | TornadoFunnel_PushMatrix 2,000 |
| B1 | Band: tpage 0x34 | TornadoFunnel_DrawBand 2,000 |
| B2 | Band: shade x 4 | TornadoFunnel_DrawBand 1,986 |
| B3 | Band: angle mask 0x1F | TornadoFunnel_DrawBand 1,041 |
| B4 | Band: radius + 0x33 | TornadoFunnel_DrawBand 2,000 |
| B5 | Band: radius + 0x1B | TornadoFunnel_DrawBand 1,999 |
| B6 | Band: height 0xFF41 | TornadoFunnel_DrawBand 1,998 |
| B7 | Band: height 0xFFA1 | TornadoFunnel_DrawBand 1,997 |
| B8 | Band: first bob frame + 5 | TornadoFunnel_DrawBand 2,000 |
| B9 | Bob: shl 6 | TornadoFunnel_DrawBand 2,000 |
| B10 | Band: frame not read again (ring B) | TornadoFunnel_DrawBand 243 |
| B11 | Band: ring B's height from 6 | TornadoFunnel_DrawBand 1,997 |
| B12 | Band: ring C's height from 8 | TornadoFunnel_DrawBand 1,998 |
| B13 | Band: 14 steps | TornadoFunnel_DrawBand 2,000 |
| B14 | Band: radius step 0xE | TornadoFunnel_DrawBand 2,000 |
| B15 | Band: height step 0x41 | TornadoFunnel_DrawBand 2,000 |
| B16 | Band: angle one further | TornadoFunnel_DrawBand 2,000 |
| B17 | Band: step bob + 3 | TornadoFunnel_DrawBand 2,000 |
| B18 | Band: frame not read again in the step | TornadoFunnel_DrawBand 1,694 |
| B19 | Band: ring B bob + 1 | TornadoFunnel_DrawBand 2,000 |
| B20 | Band: first quad's last shade 1 | TornadoFunnel_DrawBand 1,994 |
| B21 | Band: first quad size 0x40 | TornadoFunnel_DrawBand 2,000 |
| B22 | Band: second quad's first x from the register | TornadoFunnel_DrawBand 2,000 |
| B23 | Band: ring C x / y swapped | TornadoFunnel_DrawBand 2,000 |
| B24 | Band: ring C bob + 1 | TornadoFunnel_DrawBand 2,000 |
| B25 | Band: second quad's shade 1 from byte 0xD | TornadoFunnel_DrawBand 1,995 |
| B26 | Band: ring A's height from 8 | TornadoFunnel_DrawBand 1,999 |
| B27 | Band: ring C y without the bob | TornadoFunnel_DrawBand 2,000 |
| B28 | Band: ring B's step radius + 0xE | TornadoFunnel_DrawBand 2,000 |
| B29 | Band: ring C's heights not stepped | TornadoFunnel_DrawBand 2,000 |
| B30 | Band: the second quad's sort dy 3 | TornadoFunnel_DrawBand 2,000 |
| B31 | Band: carried ring A y register reread | TornadoFunnel_DrawBand 2,000 |
| N1 | Mote_Dispatch: the funnel table | TornadoMote_Dispatch 2,000 |
| N2 | Mote_Task: the funnel phases | TornadoMote_Task 1,466 |
| N3 | Mote_Task: angle x 5 | TornadoMote_Task 758 |
| N4 | Mote_Task: wobble mask 0x1F | TornadoMote_Task 410 |
| N5 | Mote_Task: wobble shl 5 | TornadoMote_Task 792 |
| N6 | Mote_Task: x sar 4 | TornadoMote_Task 792 |
| N7 | Mote_Task: z about the owner's x | TornadoMote_Task 792 |
| N8 | Mote_Task: the cell taken after the call | TornadoMote_Task 20 |
| N9 | Mote_Task: bob shl 6 | TornadoMote_Task 792 |
| N10 | Mote_Task: no screen point | TornadoMote_Task 792 |
| N11 | Mote_Task: draws at +2 0 | TornadoMote_Task 237 |
| N12 | Mote_Task: bob frame mask 0x1F | TornadoMote_Task 380 |
| N13 | Place: height + 0x10 | TornadoMote_Place 2,000 |
| N14 | Place: +0xA 1 | TornadoMote_Place 2,000 |
| N15 | Rise: on at 0x1F | TornadoMote_Rise 1,028 |
| N16 | Rise: +9 not up | TornadoMote_Rise 2,000 |
| N17 | Fade: +9 down | TornadoMote_Fade 1,995 |
| N18 | Fade: the owner not uncounted | TornadoMote_Fade 504 |
| N19 | Place: y the owner's +0x3C | TornadoMote_Place 2,000 |
| Q1 | Mote_Draw: tpage 0x15 | TornadoMote_Draw 2,000 |
| Q2 | Mote_Draw: a G4 | TornadoMote_Draw 2,000 |
| Q3 | Mote_Draw: radius 0x21 | TornadoMote_Draw 2,000 |
| Q4 | Mote_Draw: corner 0xB00 | TornadoMote_Draw 2,000 |
| Q5 | Mote_Draw: y from +0x2E | TornadoMote_Draw 2,000 |
| Q6 | Mote_Draw: tpage x 0x341 | TornadoMote_Draw 2,000 |
| Q7 | Mote_Draw: clut 0x1FB | TornadoMote_Draw 2,000 |
| Q8 | Mote_Draw: u1 0x1E | TornadoMote_Draw 2,000 |
| Q9 | Mote_Draw: v3 0x21 | TornadoMote_Draw 2,000 |
| Q10 | Mote_Draw: shade +0xB x +9 | TornadoMote_Draw 1,988 |
| Q11 | Mote_Draw: size 0x44 | TornadoMote_Draw 2,000 |
| Q12 | Mote_Draw: blue from byte 0xD | TornadoMote_Draw 1,984 |
| Q13 | Mote_Draw: opaque | TornadoMote_Draw 2,000 |
| Q14 | Mote_Draw: the corners' floats a column late | TornadoMote_Draw 2,000 |
| A1 | Mote_Alloc: 23 | TornadoMote_Alloc 79 |
| A2 | Mote_Alloc: the Sanctuary pool | TornadoMote_Alloc 2,000 |
| V1 | AverageHeight: party below 2 | Tornado_AverageHeight 651 |
| V2 | AverageHeight: seven enemies | our abort (Tornado_AverageHeight: no live actor on the opposite side) |
| V3 | AverageHeight: enemy +0x3C | Tornado_AverageHeight 896 |
| V4 | AverageHeight: member i + 1 tested | our abort (Tornado_AverageHeight: no live actor on the opposite side) |
| V5 | AverageHeight: members count twice | Tornado_AverageHeight 1,104 |
| V6 | AverageHeight: floor division | Tornado_AverageHeight 469 |
| V7 | AverageHeight: party heights unsigned | Tornado_AverageHeight 427 |
| H1b | RunPool: the previous record's owner | Sanctuary_Task 2,000; Tornado_Task 1,999 |
| V2b | AverageHeight: the eighth enemy's height left out | Tornado_AverageHeight 382 |
| V4b | AverageHeight: member (i + 1) % 3 tested | Tornado_AverageHeight 1,104 |

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
