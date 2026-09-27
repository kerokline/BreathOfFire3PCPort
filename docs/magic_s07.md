# Group S07: Bonebreak, War Shout, Focus / Meditation and Enlighten (MAGIC021, 038, 039, 040)

**Status:** IN PROGRESS (2026-09-26). All 59 functions are ours
(`src/game/magic_s07.cpp`, shadow name `magic_s07`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 118,000 rounds. 274 negative controls: 271 refused by a count (exit 3), one by a fault (B7) and one by a hang (E48), each with a near variant refused by a count, and one equivalent mutant (B65, section 6). Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S07
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability ids | Read one id down | Extent | Functions | Bytes |
|--:|---|---|---|---|---|--:|--:|
| 33 | 0x23B | MAGIC021 | 0x15 | Bonebreak | `0x4A3B20..0x4A4436` | 15 | 2,215 |
| 121 | 0x23D | MAGIC038 | 0x26 | War Shout | `0x4A4440..0x4A4C06` | 11 | 1,919 |
| 78 | 0x23E | MAGIC039 | 0x27, 0xA3 | Focus, Meditation | `0x4A4C10..0x4A5A06` | 18 | 3,468 |
| 79 | 0x23F | MAGIC040 | 0x28 | Enlighten | `0x4A5A10..0x4A6434` | 15 | 2,489 |

The extents are `tools/magic_rows.py --unit MAGIC0NN --clones` (capstone
recursive descent; every jump internal, no jump table, nothing `REFUSED`).
All 59 functions lie in the units' extents; none was found inside or missing
from them, and none was ours before: 10,091 bytes, as the queue counted.
Rows and files are `analysis/magic_rows.tsv`'s (the queue lists the rows in
row order against the overlays in file order; MAGIC038 is row 121).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. One reading supports
row 78's pair: `Focus_Kind` tests the ability word `0x904B80` for `0xA3`,
the second of the two ids that load the row, and the effect's shades and
motes then take the second row of each of its tables. What each spell looks
like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Bonebreak (MAGIC021).**
  - `Bonebreak_Task`, the kind-2 task: a three-entry stack table by +1
    (`Bonebreak_Start`, MAGIC017's `0x4A13B0`, `MagicFx_DoneAndFree`), then
    a **private pool** of 64 task-like records at `0x679B40`
    (`BonebreakMote_Pool`) walked: each live record (bit 0) becomes
    `Sprite_Current`, its +0x80 the owner, for `BonebreakMote_Task`; both
    put back from the values read after the phase call.
  - `Bonebreak_Start` clears the pool, animates the actor (offset 0xC) and
    makes one child (kind 1, parameter 9) that is a **copy of the acting
    actor's record** (its first 0x80 bytes, party or enemy by the actor
    byte), then sets the owner's bit 6.
  - The copy (`BonebreakChild_*`, six steps by +2): `BattleFx_SetSize`;
    `_Burst` ticks its script and at the end of a count sounds 0x100 and
    takes twelve motes from the pool (delays 1, 1, 4, 4, .. 16);
    `_WaitScript` waits for the script's end and flags the target 0x40;
    `_Shade` runs MAGIC008's screen shade (`0x4A29C0`) three times; 
    `_WaitMotes` waits for the motes; `BattleFx_FreeTask`.
  - The motes (`BonebreakMote_*`, three steps by +2 through
    `BonebreakMote_Steps`): `_Launch` starts each at the source sprite
    (`0x904B4C`), a random height and a random direction and speed;
    MAGIC131's `MagicFx_CountUp9By2`; `_Fade` counts +0xA down by 2 and frees
    the record (MAGIC219's `0x4F6290`). Each frame from step 1: a fan of 16
    gouraud triangles (`_DrawFan`) and a ring of 16 gouraud quads
    (`_DrawRing`) round the mote's screen point, sized by +9 and shaded by
    +0xA.
  - `BonebreakMote_Alloc` is the pool's allocator (the first record with
    bit 0 clear; 0xFF when full).
- **War Shout (MAGIC038).**
  - `WarShout_Task`: `_Start`, `_Rally`, `MagicFx_EndWhenChildrenDone`.
  - `WarShout_Start` centres the task on the side, 0x200 up, makes sixteen
    motes (kind 1, 0x59, +1 0, delays 1, 9, .. 0x79), restores CLUT row 26
    and row 2 (with STP but for its word 0), sounds 0x100.
  - The motes (`WarShoutMote_*`, five steps through `WarShoutMote_Steps`,
    run with the frame-offset table `0x9039D8` switched to the effects'):
    `_Appear` places a sprite on the owner and gives it an animation from
    `WarShoutMote_Animations`; `_Rise`, `_Circle` and `_Fade` orbit it round
    the owner (radius sl 5, height by a sine x 0x800) while its colour rises
    to 0xC0, holds, and falls; MAGIC058's `0x4AF490` frees it.
  - `WarShout_Rally`, once the motes are gone: sound 0x101 and one child
    (the same kind, +1 1) on every actor of the target's side that
    `Battle_ActorIsOut` does not answer for, at that actor's position;
    `WarShoutBuff_Start` gives each `MagicFx_BuffPopup(2, index)` and
    MAGIC105's `MagicFx_EndWithChildren` ends it.
- **Focus / Meditation (MAGIC039).**
  - `Focus_Task`: `_Start`, MAGIC009's `0x49DA50` (on at +0xB 0),
    `BattleFx_Finish`.
  - `Focus_Start`: `Focus_Kind` (2, or 3 for ability `0xA3`) to +4; an aura
    (kind 1, 0x3B, +1 0) and eight motes (+1 1, delays
    `FocusMote_Delays` + 9), each with +4 the kind - 2; CLUT row 26 back;
    sound 0x100.
  - The aura (`FocusAura_*`, six steps): grows (+9 by 2 to 0x10), swells
    (+0xA by 2 to 0x18, then the source sprite tinted black), brightens and
    dims that tint record by 2 a frame (to 0x10 and back to 0), then ends
    (tint released, the target flashed). Each frame from step 1, under the
    actor matrix: a ring of 64 projected gouraud quads whose radius and lift
    wobble by `Rand` (`_DrawRing`) and a disc of 16 projected triangles
    (`_DrawDisc`), shaded by `FocusAura_RingShades` / `_DiscShades` x +9 (x 16
    from 0x10).
  - The motes (`FocusMote_*`): each starts on a circle round the owner by
    its number (+0xB sl 9), grows (+9 by 0x10 to 0x80) and rises (+9 by 8,
    +0xA down from 4); drawn as one gouraud quad (`_Draw`) above its screen
    point, shaded by `FocusMote_Shades`.
- **Enlighten (MAGIC040).**
  - `Enlighten_Task`: `_Start`, `_Apply`, `MagicFx_EndWhenChildrenDone`.
  - `Enlighten_Start` makes two children (kind 1, 0x3C): rays (+1 0) and a
    ring (+1 1); CLUT row 26 back; sound 0x100.
  - `Enlighten_Apply`, once both are gone: `MagicFx_ApplyBuff(3, target)`
    and the popup task (kind 1, 0x48) with icon 3, or 8 when it answered 0.
  - The rays (`EnlightenRays_*`): start on the owner, anchored on the target
    (`Enlighten_AnchorToActor`), grow, then group S20's `Magic088_WaveGrow`,
    then fade; drawn as a disc of 16 gouraud triangles (`_DrawDisc`) and two
    sets of four gouraud lines (`_DrawLines`) turning with +9.
  - The ring (`EnlightenRing_*`): starts anchored with +0xB 0xB and spreads
    (+9 by a shrinking step, +0xA by -0xC) until +0xB is 0; drawn as 32 flat
    lines (`EnlightenRing_Draw`).
  - `Enlighten_AnchorToActor`: the task's screen point, moved for a party
    target by a per-character offset (`Enlighten_AnchorOffsets` by the
    record's +0x89, or `_AnchorOffsetsB` by `0x904B89` while its +0x134 has
    bit 1), dx signed by the direction.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one
exception that follows the project's precedent: a phase past any of the
dispatch tables (nine stack tables, eight `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3). The originals' loop that
does not end (section 7) is kept: it does not end in ours either.

Calls that push one argument more than the callee takes, as the originals
do, push it in ours too: the two projections get a depth and a flag
pointer.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4A13B0` | MAGIC017 (S05) | entry 1 of `Bonebreak_Task`: at +0xB 0, animation 4, the owner's bit 6 cleared, +1 on |
| `0x4A29C0` | MAGIC008 (S06) | called by `BonebreakChild_Shade`: two screen-wide gouraud quads shaded by +9 |
| `0x4F6290` | MAGIC219 (S37) | tail-jumped by `BonebreakMote_Fade`: the record's bytes +0..+4 cleared |
| `0x49DA50` | MAGIC009 (S03) | entry 1 of `Focus_Task`: +1 on once +0xB is 0 |
| `0x4AF490` | MAGIC058 (S11) | entry 4 of `WarShoutMote_Steps`: the owner's +0xB down, the task freed |

By name, already ours: `MagicFx_DoneAndFree` (E), `MagicFx_EndWhenChildrenDone`
and `MagicFx_CountUp9By2` (S30), `MagicFx_EndWithChildren` (S24),
`Magic088_WaveGrow` (S20), `BattleFx_SetSize` / `_FreeTask` / `_Finish`, the
effect library (`MagicFx_CenterOnSide`, `_ApplyBuff`, `_BuffPopup`,
`BattleActor_*`), `MagicFx_PushActorMatrix`, and the GTE / GPU / sprite /
sound library.

Shared bodies (`magic_funcs.tsv`): `FocusMote_Rise` (`0x4A5180`) is reached
from 12 files, `EnlightenRays_Fade` (`0x4A5D50`) from 14,
`EnlightenRays_Grow` and `WarShoutBuff_Start` from five each. Those reach
them through the addresses, so taking them changes nothing for the callers.
The tsv also has MAGIC021 reaching MAGIC038's child code
(`0x4A4710..0x4A4BE0`) and MAGIC039 reaching MAGIC040's
(`0x4A5C30..0x4A62B0`): that reach follows the tool's over-counted `.data`
tables (section 4: `BonebreakChild_TaskTable` is read by an index that is
always 0, `FocusChild_Kinds` has two entries). By the dispatchers read here,
neither overlay reaches the other's code.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `BonebreakChild_TaskTable` | `0x65A6E8` | 1 |
| `BonebreakMote_TaskTable` | `0x65A6EC` | 1 |
| `BonebreakMote_Steps` | `0x65A6F0` | 3 |
| `WarShoutChild_Kinds` | `0x65A6FC` | 2 |
| `WarShoutMote_Steps` | `0x65A704` | 5 |
| `WarShoutMote_Animations` | `0x65A718` | 16 bytes |
| `WarShoutBuff_Steps` | `0x65A728` | 2 |
| `FocusAura_DiscShades` | `0x65A730` | 2 x 3 bytes |
| `FocusMote_Shades` | `0x65A738` | 2 x 3 bytes |
| `FocusAura_RingShades` | `0x65A740` | 2 x 3 bytes |
| `FocusMote_Delays` | `0x65A748` | 8 bytes |
| `FocusChild_Kinds` | `0x65A750` | 2 |
| `EnlightenChild_Kinds` | `0x65A758` | 2 |
| `Enlighten_AnchorOffsets` | `0x65A760` | 22 (dx, dy) words |
| `Enlighten_AnchorOffsetsB` | `0x65A7B8` | 22 (dx, dy) words |
| `Enlighten_AnchorIndexB` | `0x65A810` | 25 bytes |
| `BonebreakMote_Pool` | `0x679B40` | 64 x 0x84 bytes |

Each count is where the next table starts (the dump of `0x65A6D0..0x65A830`
read 2026-09-26); the tool's "12 / 11 / 10 code entries" notes count every
code pointer that follows, across the next tables. The three shade tables
have two rows each because +4 is `Focus_Kind` - 2 (0 or 1).

## 5. The fuzz

`BOF3X_SHADOW=magic_s07` runs `magic_harness::Run` over the 59 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s07_fuzz.cpp`:

- **Callees** (39 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` log the whole
    0x800-byte packet buffer of the fuzz's own at each call (group C1's way:
    every primitive of a draw is built in it) and move `Gfx_PacketNext` on;
  - the projections log their SVECTORs through `deref` (6 bytes each), the
    outputs by address, the depth and flag pointers masked off;
  - the sprite calls that act on `Sprite_Current` (the two script ticks,
    the animation, the screen update) log which sprite, the screen update
    the frame-offset table too; `WarShoutMote_Run`'s five steps are listed
    as `kPhase` recorders logging `0x9039D8`, before their `.data` table
    registers them;
  - `Math_Sin` / `Math_Cos` rewrite a scratch or vertex word a quarter of
    the time, and `BattleTask_Create` every actor record's position a third
    of the time, so the re-reads after those calls are compared (controls
    F40, W20: section 6);
  - **the `kFlag` blind spot** (queue §9): the script ticks and
    `Battle_ActorIsOut` answer from the recorders' stream through an
    `effect`, not from the hash that chose their disturbance, so a "no" can
    follow a moved cell;
  - this group's own functions called directly and the other units' by
    address as `kPhase` recorders; the pool allocator as a byte 0..0x3F (its
    caller does not test 0xFF); `Focus_Kind` as a byte; `_DrawLines` with
    its two arguments masked to what it reads (a byte, a word).
- **Tables:** the eight `.data` handler tables of section 4.
- **Regions** beyond the standard ones: the scratch `0x903850..5F`,
  `Prim_VertexScratch`, `Gfx_PacketNext` and the packet buffer,
  `MoveScript_TintRecords`, CLUT rows 26 (32 words) and 2 (16 words) and
  their sources, `0x9039D8`, `0x904B80..0x904B8F` (the ability word, the
  form index), the mote pool, and the overlays' `.data` the functions read
  (the animation, shade, delay and anchor tables). 25,444 bytes in 23
  regions.
- **Seed:** the pool full, partly taken or random, each record's owner a
  real slot or sprite record; the target's side bit half the time; the
  ability word 0xA3 or one either side half the time; each dispatcher inside
  its table (+0 cleared half the time where it gates a draw); each count
  one step before, at and past its threshold (`_Burst`'s +9, `_Shade`'s
  0xC, the rise's 0xC0, the circle's 0x100, the grow's 0x10 and 0x80, the
  swell's 0x18, the tint channel's 0x10 and 0, the spread's +0xB, ..); the
  shade branch of +9 below and at 0x10; for the anchor a party target most of
  the time, both tables, directions 0..4, indices inside the tables;
  `_DrawLines`'s first step below 0xE0 (section 7), garbage above it
  (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a scratch or vertex
  word, a pool record's live bit, the owner's +0xB, the ability word, a
  tint byte, `0x9039D8`.

Result in this worktree (2026-09-26):

    shadow      magic_s07 self-test: 118000 rounds over 59 functions (2000 each), 3627404 calls to the stand-ins,
                0 MISMATCHES; 25444 bytes of state (23 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (3,628,875
stand-in calls for this group in that run: the harness's pointers into the
DLL move a few branches, 0 mismatches).

## 6. Controls

274 plants, each put in `magic_s07.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s07`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). B, W, F and E are Bonebreak, War Shout, Focus and Enlighten. 274 negative controls: 271 refused by a count (exit 3), one by a fault (B7) and one by a hang (E48), each with a near variant refused by a count, and one equivalent mutant (B65, section 6).

The first run (272 plants) left four standing that were not equivalent, and the fuzz was fixed for each, then every control run again: **F40** (a scratch angle not read again after a call) and **W20** (an actor's position read before the create) showed the group disturbance reached one word too rarely - `Math_Sin` / `Math_Cos` and `BattleTask_Create` now have effects that rewrite the scratch and the actor positions; **F14** (the ability id tested as a byte) showed the seed never put a high byte above 0xA3; E48's first near variant (five lines) could hang too and was replaced by two lines (E63).

The thinnest (fewer than 60 rounds):

- **B2** (Bonebreak_Task: self read before the phase): 54
- **B13** (Start: actor read before the create): 57
- **B17** (Start: +0xB counted before the copy): 43
- **B27** (WaitScript: +2 before the flag): 48
- **B30** (Shade: +9 read before the shade): 25
- **B41** (Launch: z pointer after the cos): 24
- **B44** (Launch: the cos angle not read again): 6
- **B62** (Alloc: 63 records): 4
- **W31** (Appear: the cos angle not read again): 14
- **W41** (Orbit: the height scale not read again): 32
- **W41** (Orbit: the height scale not read again): 45
- **W41** (Orbit: the height scale not read again): 42
- **W49** (Rise: tick after the colour): 47
- **F40** (Mote_Start: sin angle not read again): 3
- **F83** (Disc: previous point read before the prim): 42
- **E17** (Rays_Run: task not read again): 6
- **E30** (Anchor: target below 4): 43

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| B1 | Bonebreak_Task: entries 0/1 swapped | Bonebreak_Task 1367 |
| B2 | Bonebreak_Task: self read before the phase | Bonebreak_Task 54 |
| B3 | Bonebreak_Task: 63 records | Bonebreak_Task 1002 |
| B4 | Bonebreak_Task: live bit 2 | Bonebreak_Task 2000 |
| B5 | Bonebreak_Task: owner not set | Bonebreak_Task 1986 |
| B6 | Bonebreak_Task: owner not put back | Bonebreak_Task 1610 |
| B7 | Bonebreak_Task: record owner +0x7C | a fault (access violation) in the first round: the owner pointer read 4 bytes low is garbage the walk makes the owner; near variant B66 refused by a count |
| B8 | Start: pool +2 kept | Bonebreak_Start 2000 |
| B9 | Start: animation 0xD | Bonebreak_Start 2000 |
| B10 | Start: parameter 8 | Bonebreak_Start 2000 |
| B11 | Start: party at 0..1 | Bonebreak_Start 391 |
| B12 | Start: 0x7C bytes copied | Bonebreak_Start 2000 |
| B13 | Start: actor read before the create | Bonebreak_Start 57 |
| B14 | Start: child +6 2 | Bonebreak_Start 2000 |
| B15 | Start: child +5 8 | Bonebreak_Start 2000 |
| B16 | Start: owner bit 5 | Bonebreak_Start 1482 |
| B17 | Start: +0xB counted before the copy | Bonebreak_Start 43 |
| B18 | Child_Run: steps 1/2 swapped | BonebreakChild_Run 671 |
| B19 | Child_Run: update with +0 zero | BonebreakChild_Run 2000 |
| B20 | Burst: +9 checked before the tick | BonebreakChild_Burst 1333 |
| B21 | Burst: sound 0x101 | BonebreakChild_Burst 667 |
| B22 | Burst: eleven motes | BonebreakChild_Burst 667 |
| B23 | Burst: delay x 4 | BonebreakChild_Burst 667 |
| B24 | Burst: owner read before the alloc | BonebreakChild_Burst 205 |
| B25 | Burst: +9 1 not 0 | BonebreakChild_Burst 629 |
| B26 | WaitScript: on at no end | BonebreakChild_WaitScript 2000 |
| B27 | WaitScript: +2 before the flag | BonebreakChild_WaitScript 48 |
| B28 | Shade: at 0xD | BonebreakChild_Shade 664 |
| B29 | Shade: +9 by 3 | BonebreakChild_Shade 675 |
| B30 | Shade: +9 read before the shade | BonebreakChild_Shade 25 |
| B31 | WaitMotes: at 1 | BonebreakChild_WaitMotes 1344 |
| B32 | WaitMotes: owner +0xA | BonebreakChild_WaitMotes 699 |
| B33 | Mote_Run: steps 0/2 swapped | BonebreakMote_Run 1274 |
| B34 | Mote_Run: drawn at +2 0 | BonebreakMote_Run 326 |
| B35 | Mote_Run: ring before fan | BonebreakMote_Run 711 |
| B36 | Launch: Rand & 3 | BonebreakMote_Launch 341 |
| B37 | Launch: height sl 4 | BonebreakMote_Launch 555 |
| B38 | Launch: speed 0xD | BonebreakMote_Launch 673 |
| B39 | Launch: angle sl 8 | BonebreakMote_Launch 569 |
| B40 | Launch: sar 4 | BonebreakMote_Launch 683 |
| B41 | Launch: z pointer after the cos | BonebreakMote_Launch 24 |
| B42 | Launch: +0xA 0x11 | BonebreakMote_Launch 683 |
| B43 | Launch: x from the source's +0x38 | BonebreakMote_Launch 683 |
| B44 | Launch: the cos angle not read again | BonebreakMote_Launch 6 |
| B45 | Fade: by 1 | BonebreakMote_Fade 2000 |
| B46 | Fade: owner not counted | BonebreakMote_Fade 705 |
| B47 | DrawFan: tpage 0x36 | BonebreakMote_DrawFan 2000 |
| B48 | DrawFan: x 10 | BonebreakMote_DrawFan 1998 |
| B49 | DrawFan: radius +9 x 3 | BonebreakMote_DrawFan 1972 |
| B50 | DrawFan: 15 triangles | BonebreakMote_DrawFan 2000 |
| B51 | DrawFan: step 0x80 | BonebreakMote_DrawFan 2000 |
| B52 | DrawFan: rim +0x15 c1 | BonebreakMote_DrawFan 1998 |
| B53 | DrawFan: centre y from x | BonebreakMote_DrawFan 2000 |
| B54 | DrawFan: linked size 0x30 | BonebreakMote_DrawFan 2000 |
| B55 | DrawFan: layer 1 | BonebreakMote_DrawFan 2000 |
| B56 | DrawRing: x 7 | BonebreakMote_DrawRing 1994 |
| B57 | DrawRing: outer +9 x 4 | BonebreakMote_DrawRing 1925 |
| B58 | DrawRing: outer at the inner radius | BonebreakMote_DrawRing 1999 |
| B59 | DrawRing: vertices 1 / 2 swapped | BonebreakMote_DrawRing 2000 |
| B60 | DrawRing: outer shade 2 | BonebreakMote_DrawRing 2000 |
| B61 | DrawRing: size 0x40 | BonebreakMote_DrawRing 2000 |
| B62 | Alloc: 63 records | BonebreakMote_Alloc 4 |
| B63 | Alloc: not marked | BonebreakMote_Alloc 1455 |
| B64 | Alloc: full 0xFE | BonebreakMote_Alloc 545 |
| B65 | Child_Task: past table allowed | not refused: equivalent - the one-entry table's index is always 0 in the fuzz (any other index aborts ours by design and reads the next table in Capcom's), so dispatching entry 0 regardless cannot differ; the table's own contents are checked by B18 / B33 |
| B66 | Bonebreak_Task: record owner this task (B7's near variant) | Bonebreak_Task 1988 |
| W1 | WarShout_Task: entries 0/1 swapped | WarShout_Task 1307 |
| W2 | Start: + 0x1000000 | WarShout_Start 2000 |
| W3 | Start: fifteen motes | WarShout_Start 2000 |
| W4 | Start: delay sl 2 | WarShout_Start 2000 |
| W5 | Start: +0xB not numbered | WarShout_Start 2000 |
| W6 | Start: parameter 0x58 | WarShout_Start 2000 |
| W7 | Start: row 2 without STP | WarShout_Start 2000 |
| W8 | Start: row 2 word 0 kept with STP | WarShout_Start 1027 |
| W9 | Start: row 26 half | WarShout_Start 2000; Focus_Start 2000; Enlighten_Start 2000 |
| W10 | Start: not dirty | WarShout_Start 1993 |
| W11 | Start: +9 kept | WarShout_Start 1324 |
| W12 | Rally: at 1 | WarShout_Rally 1298 |
| W13 | Rally: sound 0x100 | WarShout_Rally 666 |
| W14 | Rally: side bit 0x80 | WarShout_Rally 321 |
| W15 | Rally: seven enemies | WarShout_Rally 321 |
| W16 | Rally: enemy index i + 2 | WarShout_Rally 321 |
| W17 | Rally: out made | WarShout_Rally 666 |
| W18 | Rally: +4 3 | WarShout_Rally 548 |
| W19 | Rally: +1 0 | WarShout_Rally 548 |
| W20 | Rally: x read before the create | WarShout_Rally 303 |
| W21 | Rally: z from +0x3C | WarShout_Rally 548 |
| W22 | Rally: +1 not on | WarShout_Rally 666 |
| W23 | Child_Task: kinds swapped | WarShoutChild_Task 2000 |
| W24 | Mote_Run: table not swapped | WarShoutMote_Run 2000 |
| W25 | Mote_Run: table not put back | WarShoutMote_Run 2000 |
| W26 | Mote_Run: steps 1/2 swapped | WarShoutMote_Run 812 |
| W27 | Mote_Run: update at +2 0 | WarShoutMote_Run 215 |
| W28 | Appear: +0x14 kept | WarShoutMote_Appear 662 |
| W29 | Appear: x sl 0x11 | WarShoutMote_Appear 662 |
| W30 | Appear: height sl 0x16 | WarShoutMote_Appear 661 |
| W31 | Appear: the cos angle not read again | WarShoutMote_Appear 14 |
| W32 | Appear: +0x27 0x21 | WarShoutMote_Appear 662 |
| W33 | Appear: +0x29 5 | WarShoutMote_Appear 662 |
| W34 | Appear: animation by +0xA | WarShoutMote_Appear 656 |
| W35 | Appear: +0x5E 0x81 | WarShoutMote_Appear 662 |
| W36 | Appear: +0 bit 4 | WarShoutMote_Appear 502 |
| W37 | Appear: +0x25 0x1E | WarShoutMote_Appear 662 |
| W38 | Appear: word +0x2C 1 | WarShoutMote_Appear 661 |
| W39 | Orbit: angle mask 0x3F | WarShoutMote_Rise 977; WarShoutMote_Circle 772; WarShoutMote_Fade 1013 |
| W40 | Orbit: x sl 0x12 | WarShoutMote_Rise 2000; WarShoutMote_Circle 2000; WarShoutMote_Fade 2000 |
| W41 | Orbit: the height scale not read again | WarShoutMote_Rise 32; WarShoutMote_Circle 45; WarShoutMote_Fade 42 |
| W42 | Orbit: 0x400 | WarShoutMote_Rise 1980; WarShoutMote_Circle 1977; WarShoutMote_Fade 1980 |
| W43 | Orbit: +0x14 by 2 | WarShoutMote_Rise 2000; WarShoutMote_Circle 2000; WarShoutMote_Fade 2000 |
| W44 | Orbit: height angle sl 5 | WarShoutMote_Rise 1956; WarShoutMote_Circle 1966; WarShoutMote_Fade 1974 |
| W45 | Orbit: z from the owner's +0x34 | WarShoutMote_Rise 2000; WarShoutMote_Circle 2000; WarShoutMote_Fade 2000 |
| W46 | Rise: colour by 5 | WarShoutMote_Rise 2000 |
| W47 | Rise: at 0xBF | WarShoutMote_Rise 1213 |
| W48 | Rise: bit 5 kept | WarShoutMote_Rise 302 |
| W49 | Rise: tick after the colour | WarShoutMote_Rise 47 |
| W50 | Circle: at 0xFF | WarShoutMote_Circle 783 |
| W51 | Circle: +0x5F 0xC1 | WarShoutMote_Circle 411 |
| W52 | Circle: +0x5C 0 | WarShoutMote_Circle 411 |
| W53 | Fade: colour by 3 | WarShoutMote_Fade 228 |
| W54 | Fade: +0x5D 0x7F | WarShoutMote_Fade 2000 |
| W55 | Fade: orbit before the tick | WarShoutMote_Fade 2000 |
| W56 | Buff_Run: steps swapped | WarShoutBuff_Run 2000 |
| W57 | Buff_Start: arguments swapped | WarShoutBuff_Start 1993 |
| W58 | Buff_Start: +0xB 2 | WarShoutBuff_Start 2000 |
| F1 | Focus_Task: entries 1/2 swapped | Focus_Task 1324 |
| F2 | Start: position not the owner's z | Focus_Start 1759 |
| F3 | Start: kind to +5 | Focus_Start 2000 |
| F4 | Start: aura +4 - 1 | Focus_Start 1683 |
| F5 | Start: seven motes | Focus_Start 2000 |
| F6 | Start: delay + 8 | Focus_Start 2000 |
| F7 | Start: delay from the next | Focus_Start 2000 |
| F8 | Start: motes +1 0 | Focus_Start 2000 |
| F9 | Start: parameter 0x3A | Focus_Start 2000 |
| F10 | Start: aura not counted | Focus_Start 1602 |
| F11 | Start: CLUT not dirty | Focus_Start 1992 |
| F12 | Kind: 0xA4 | Focus_Kind 567 |
| F13 | Kind: 3 / 1 | Focus_Kind 1548 |
| F14 | Kind: the byte | Focus_Kind 352 |
| F15 | Child_Task: kinds swapped | FocusChild_Task 2000 |
| F16 | Aura_Run: steps 3/4 swapped | FocusAura_Run 697 |
| F17 | Aura_Run: bit 1 | FocusAura_Run 409 |
| F18 | Aura_Run: disc before ring | FocusAura_Run 406 |
| F19 | Aura_Run: no pop | FocusAura_Run 406 |
| F20 | Aura_Start: +0xA 2 | FocusAura_Start 2000 |
| F21 | Grow: by 1 | FocusAura_Grow 2000 |
| F22 | Grow: at 0x12 | FocusAura_Grow 623 |
| F23 | Swell: at 0x16 | FocusAura_Swell 643 |
| F24 | Swell: tint alpha 0 | FocusAura_Swell 643 |
| F25 | Swell: tint slot to +0xA | FocusAura_Swell 643 |
| F26 | Swell: not released | FocusAura_Swell 643 |
| F27 | Swell: +9 kept | FocusAura_Swell 1993 |
| F28 | Brighten: two channels | FocusAura_Brighten 2000 |
| F29 | Brighten: stride 8 | FocusAura_Brighten 1992 |
| F30 | Brighten: at 0x12 | FocusAura_Brighten 695 |
| F31 | Dim: by 1 | FocusAura_Dim 2000 |
| F32 | Dim: at channel +3 | FocusAura_Dim 713 |
| F33 | End: by 1 | FocusAura_End 2000 |
| F34 | End: the actor flashed | FocusAura_End 644 |
| F35 | End: owner not counted | FocusAura_End 660 |
| F36 | Mote_Run: steps 1/2 swapped | FocusMote_Run 1359 |
| F37 | Mote_Run: no screen point | FocusMote_Run 346 |
| F38 | Mote_Start: angle sl 8 | FocusMote_Start 642 |
| F39 | Mote_Start: x 4 | FocusMote_Start 643 |
| F40 | Mote_Start: sin angle not read again | FocusMote_Start 3 |
| F41 | Mote_Start: +0xA 5 | FocusMote_Start 643 |
| F42 | Mote_Start: height kept | FocusMote_Start 573 |
| F43 | Mote_Grow: at 0x90 | FocusMote_Grow 673 |
| F44 | Mote_Rise: by 4 | FocusMote_Rise 1996 |
| F45 | Mote_Draw: height +0xA | FocusMote_Draw 1988 |
| F46 | Mote_Draw: shade x 2 | FocusMote_Draw 1337 |
| F47 | Mote_Draw: layer 1 | FocusMote_Draw 2000 |
| F48 | Mote_Draw: bottom y - h | FocusMote_Draw 1992 |
| F49 | Mote_Draw: top shade 2 | FocusMote_Draw 2000 |
| F50 | Mote_Draw: +0x35 SB(4) | FocusMote_Draw 1926 |
| F51 | Mote_Draw: y from +0x2E | FocusMote_Draw 1994 |
| F52 | Ring: height +9 | FocusAura_DrawRing 1966 |
| F53 | Ring: Rand & 7 | FocusAura_DrawRing 1012 |
| F54 | Ring: first lift x 5 | FocusAura_DrawRing 1769 |
| F55 | Ring: outer 0x81 | FocusAura_DrawRing 1920 |
| F56 | Ring: first radius sl 3 | FocusAura_DrawRing 1953 |
| F57 | Ring: shades from the disc's | FocusAura_DrawRing 1918 |
| F58 | Ring: first z not the lift | FocusAura_DrawRing 1833 |
| F59 | Ring: first outer x at the inner radius | FocusAura_DrawRing 1853 |
| F60 | Ring: 63 quads | FocusAura_DrawRing 2000 |
| F61 | Ring: step not in the angle | FocusAura_DrawRing 2000 |
| F62 | Ring: loop lift sl 2 | FocusAura_DrawRing 2000 |
| F63 | Ring: loop second sin not read again | FocusAura_DrawRing 1011 |
| F64 | Ring: loop radius + 0x81 | FocusAura_DrawRing 2000 |
| F65 | Ring: old point z from the new lift | FocusAura_DrawRing 2000 |
| F66 | Ring: new inner x at the outer radius | FocusAura_DrawRing 2000 |
| F67 | Ring: old outer y from x | FocusAura_DrawRing 2000 |
| F68 | Ring: sort by y too | FocusAura_DrawRing 2000 |
| F69 | Ring: shift sl 8 | FocusAura_DrawRing 2000 |
| F70 | Ring: inner shade 0 | FocusAura_DrawRing 2000 |
| F71 | Ring: vertices 2 / 3 swapped | FocusAura_DrawRing 2000 |
| F72 | Ring: depths 4_10 | FocusAura_DrawRing 2000 |
| F73 | Ring: linked on layer 1 | FocusAura_DrawRing 2000 |
| F74 | Ring: last outer y at the inner radius | FocusAura_DrawRing 2000 |
| F75 | Shades: below 0xF | FocusAura_DrawRing 290; FocusAura_DrawDisc 295 |
| F76 | Shades: sl 3 | FocusAura_DrawRing 1049; FocusAura_DrawDisc 1115 |
| F77 | Shades: third x +0xA | FocusAura_DrawRing 512; FocusAura_DrawDisc 577 |
| F78 | Disc: radius 0x89 | FocusAura_DrawDisc 1976 |
| F79 | Disc: layer 4 first | FocusAura_DrawDisc 2000 |
| F80 | Disc: 15 triangles | FocusAura_DrawDisc 2000 |
| F81 | Disc: centre x 1 | FocusAura_DrawDisc 2000 |
| F82 | Disc: rim y from cos at x | FocusAura_DrawDisc 2000 |
| F83 | Disc: previous point read before the prim | FocusAura_DrawDisc 42 |
| F84 | Disc: centre shade 0 | FocusAura_DrawDisc 2000 |
| F85 | Disc: size 0x30 | FocusAura_DrawDisc 2000 |
| F86 | Disc: closing tpage 0x35 | FocusAura_DrawDisc 2000 |
| F87 | Disc: z kept | FocusAura_DrawDisc 1959 |
| E1 | Enlighten_Task: entries 0/1 swapped | Enlighten_Task 1338 |
| E2 | Start: direction +9 | Enlighten_Start 1956 |
| E3 | Start: one child | Enlighten_Start 2000 |
| E4 | Start: parameter 0x3D | Enlighten_Start 2000 |
| E5 | Start: +1 kind + 1 | Enlighten_Start 2000 |
| E6 | Start: sound 0x101 | Enlighten_Start 2000 |
| E7 | Apply: at 1 | Enlighten_Apply 1346 |
| E8 | Apply: stat 2 | Enlighten_Apply 673 |
| E9 | Apply: icons swapped | Enlighten_Apply 673 |
| E10 | Apply: parameter 0x47 | Enlighten_Apply 673 |
| E11 | Apply: +9 2 | Enlighten_Apply 673 |
| E12 | Apply: +1 not on | Enlighten_Apply 673 |
| E13 | Child_Task: kinds swapped | EnlightenChild_Task 2000 |
| E14 | Rays_Run: steps 1/2 swapped | EnlightenRays_Run 974 |
| E15 | Rays_Run: length x 4 | EnlightenRays_Run 364 |
| E16 | Rays_Run: second from + 8 | EnlightenRays_Run 365 |
| E17 | Rays_Run: task not read again | EnlightenRays_Run 6 |
| E18 | Rays_Run: no disc | EnlightenRays_Run 365 |
| E19 | Rays_Start: not anchored | EnlightenRays_Start 2000 |
| E20 | Rays_Start: +0xA 1 | EnlightenRays_Start 2000 |
| E21 | Rays_Grow: at 0x11 | EnlightenRays_Grow 1323 |
| E22 | Rays_Grow: +9 kept | EnlightenRays_Grow 2000 |
| E23 | Rays_Fade: +9 down | EnlightenRays_Fade 1994 |
| E24 | Ring_Run: steps swapped | EnlightenRing_Run 2000 |
| E25 | Ring_Run: drawn with bit 0 clear | EnlightenRing_Run 726 |
| E26 | Ring_Start: +0xB 0xC | EnlightenRing_Start 2000 |
| E27 | Ring_Start: +0xA 0x7F | EnlightenRing_Start 2000 |
| E28 | Spread: +9 by the old +0xB | EnlightenRing_Spread 1998 |
| E29 | Spread: by 0xB | EnlightenRing_Spread 1993 |
| E30 | Anchor: target below 4 | Enlighten_AnchorToActor 43 |
| E31 | Anchor: flag bit 2 | Enlighten_AnchorToActor 653 |
| E32 | Anchor: index from +0x88 | Enlighten_AnchorToActor 728 |
| E33 | Anchor: tables swapped | Enlighten_AnchorToActor 678 |
| E34 | Anchor: direction 2 adds | Enlighten_AnchorToActor 316 |
| E35 | Anchor: index x 1 | Enlighten_AnchorToActor 1293 |
| E36 | Anchor: dy from dx | Enlighten_AnchorToActor 1363 |
| E37 | Anchor: dy subtracted | Enlighten_AnchorToActor 1356 |
| E38 | Anchor: form index table + 1 | Enlighten_AnchorToActor 641 |
| E39 | Anchor: direction >> 2 | Enlighten_AnchorToActor 1016 |
| E40 | Lines: 3 lines | EnlightenRays_DrawLines 2000 |
| E41 | Lines: angle mask 0xF | EnlightenRays_DrawLines 2000 |
| E42 | Lines: sin for x | EnlightenRays_DrawLines 2000 |
| E43 | Lines: length's low byte | EnlightenRays_DrawLines 1995 |
| E44 | Lines: shade 0x71 | EnlightenRays_DrawLines 2000 |
| E45 | Lines: size 0x20 | EnlightenRays_DrawLines 2000 |
| E46 | Lines: step 4 | EnlightenRays_DrawLines 2000 |
| E47 | Lines: x read after the angle store | EnlightenRays_DrawLines 2000 |
| E48 | Lines: the step from the whole word | a hang: the loop bound grows with the argument's garbage above the byte and never ends; near variant E63 refused by a count |
| E49 | RaysDisc: radius +9 | EnlightenRays_DrawDisc 1972 |
| E50 | RaysDisc: step 0x100 | EnlightenRays_DrawDisc 2000 |
| E51 | RaysDisc: centre 0x7F | EnlightenRays_DrawDisc 2000 |
| E52 | RaysDisc: size 0x30 | EnlightenRays_DrawDisc 2000 |
| E53 | RaysDisc: second y by sin | EnlightenRays_DrawDisc 2000 |
| E54 | RingDraw: 31 lines | EnlightenRing_Draw 2000 |
| E55 | RingDraw: step sl 6 | EnlightenRing_Draw 2000 |
| E56 | RingDraw: radius +0xA | EnlightenRing_Draw 1963 |
| E57 | RingDraw: grey from +9 | EnlightenRing_Draw 2000 |
| E58 | RingDraw: size 0x1C | EnlightenRing_Draw 2000 |
| E59 | RingDraw: end y at +0x1C | EnlightenRing_Draw 2000 |
| E60 | RingDraw: cos angle not read again | EnlightenRing_Draw 624 |
| E61 | RingDraw: closing tpage 0x35 | EnlightenRing_Draw 2000 |
| E63 | Lines: two lines (E48's near variant) | EnlightenRays_DrawLines 2000 |
| E62 | Start: CLUT not dirty | Enlighten_Start 1992 |

**Re-run 2026-09-26 on the kFlag-fixed harness
([`magic_harness.md`](magic_harness.md) §8): 42 controls in the affected
functions, 42 refused.** The seven affected clones are
`BonebreakChild_Burst` / `_WaitScript`, `WarShout_Rally`,
`WarShoutMote_Rise` / `_Circle` / `_Fade` and `Enlighten_Apply`. Selected:
B20..B27, W12..W22, W39..W45 (the orbit step the three motes' phases share),
W46..W55 and E7..E12; every other control plants outside these functions
(B18 / B19 and W26 / W27 are the `_Run` dispatchers, not the phases). Each was
planted again by a script (plant, rebuild, recompile checked,
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s07`, restore, rebuild), rebuilt
from the table and anchored on strings that occur once (the orbit's angle
lines also occur in `WarShoutMote_Appear`, so W39 / W44 anchor on the line
before). All 42 exit 3, only in the planted functions, and 41 of them in
**the same count as the table's**: the script ticks and `Battle_ActorIsOut`
already answer through this group's own `effect` (section 5, the kFlag blind
spot), so the harness's new draw never reaches a caller here. The one that
moved, W53 (2000, was 228), is the rebuilt plant's wording, not the stream:
this time all three channels step down by 3. Thinnest: W41 (Rise
32, Circle 45, Fade 42), W49 47, B27 48. No fuzz change. Clean self-test
after: 0 mismatches, exit 0; `BOF3X_SHADOW='*'`: exit 0.

## 7. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: nine stack tables and eight
  `.data` tables (a one-entry task table's next index reads the next
  table). Ours aborts.
- **`EnlightenRays_DrawLines` does not end for a first step of 0xE0 or
  more**: its step is a byte (`add bl, 8`) compared with the dword first +
  0x20, which the byte never reaches, so the loop commits lines forever - a
  freeze. Its first step is the rays' +9 (and +9 + 4), which in play runs
  0 .. 0x30 (Start 0, Grow to 0x10, `Magic088_WaveGrow` to 0x20, Fade + 0x10),
  so by reading play never reaches it. Kept in ours; the fuzz keeps the
  argument below 0xE0.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** by every
  creator here (`Bonebreak_Start`, `WarShout_Start` and `_Rally`,
  `Focus_Start`, `Enlighten_Start` and `_Apply`): slot 255 lies past the
  image's end, an access violation in ours as in the original.
- **`BonebreakMote_Alloc`'s 0xFF is unchecked** by `BonebreakChild_Burst`:
  record 255 is `0x681F3C`, past the pool's end, in `.bss` this project has
  not named, where the burst writes an owner (+0x80) and a delay (+9). By
  reading unreachable: one burst takes 12 of the 64 records, and the start
  clears the pool.
- **`Bonebreak_Start` indexes the enemy records by the actor byte - 3**,
  unchecked above 10.

## 8. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Which
character or enemy uses each is not measured here. Things to look for:

- Bonebreak: a copy of the caster, twelve shaded discs bursting from the
  source, the screen shaded three times.
- War Shout: sixteen sprites circling the side, then a buff popup on each
  living actor of the target's side.
- Focus / Meditation: a wobbling ring and a disc under the caster, eight
  rising quads, the source tinted dark and back; Meditation (id 0xA3) with
  the second shade row.
- Enlighten: rays and a ring at the target (moved per character for a party
  target), then the buff and its popup.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy has a `group S07` comment and 59 lines: 48 new,
six host extents re-listed smaller in place (`004A3E10 12`, `004A3F90 1D0`,
`004A43E0 57`, `004A4D90 10`, `004A57C0 247`, `004A62B0 185`, each the
function's own size), and five listed right already (`004A51C0`,
`004A53A0`, `004A5E90`, `004A5FC0`, `004A6120`).
