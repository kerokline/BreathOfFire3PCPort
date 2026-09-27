# Group S07: Bonebreak, War Shout, Focus / Meditation and Enlighten (MAGIC021, 038, 039, 040)

**Status:** IN PROGRESS (2026-09-26). All 59 functions are ours
(`src/game/magic_s07.cpp`, shadow name `magic_s07`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 118,000 rounds. @@CONTROLS_SUMMARY@@ Nothing recorded
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

    shadow      magic_s07 self-test: 118000 rounds over 59 functions (2000 each), 3628913 calls to the stand-ins,
                0 MISMATCHES; 25444 bytes of state (23 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (3,628,585
stand-in calls for this group in that run: the harness's pointers into the
DLL move a few branches, 0 mismatches).

## 6. Controls

@@CONTROLS_TABLE@@

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
