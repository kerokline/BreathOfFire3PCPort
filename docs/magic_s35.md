# Group S35: Last Resort, Cure and Benediction (MAGIC167, 168, 169)

**Status:** IN PROGRESS (2026-09-27). All 46 functions are ours
(`src/game/magic_s35.cpp`, shadow name `magic_s35`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 92,000 rounds. 228 of 228 negative controls refused (227 by a count, one by a hang whose bounded variant was refused by a count). Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S35
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 91 | 0x2A7 | MAGIC167 | 0xA7 | Last Resort | `0x4EF620..0x4F0666` | 15 |
| 117 | 0x2A8 | MAGIC168 | 0xA8 | Cure | `0x4F0670..0x4F119E` | 12 |
| 97 | 0x2A9 | MAGIC169 | 0xA9 | Benediction | `0x4F11A0..0x4F1E36` | 19 |

The extents are `tools/magic_rows.py --unit MAGIC16N --clones` (capstone
recursive descent; no jump table, nothing `REFUSED`). All 46 functions lie in
the units' extents; none was found inside or missing from them, and none was
ours before. 9,873 bytes, as the queue counted. The rows are as
`analysis/magic_rows.tsv` pairs them (MAGIC168 is row 117, MAGIC169 row 97;
the queue lists the rows in number order).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What each spell looks
like in play has not been measured; the outlines below are what the code
draws, not what a player sees.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Last Resort (MAGIC167).**
  - `LastResort_Task`, the kind-2 task: a six-entry stack table by +1 -
    `LastResort_Start`, MAGIC077's `Revive_TintSource`, `BattleFx_Brighten`,
    `LastResort_WaitChildren` (+1 on once +0xB is 1 or less), MAGIC086's
    `Barrier_Fade`, `BattleFx_Finish`.
  - `LastResort_Start` puts the task at the source sprite (`0x904B4C`) and
    makes nine children of kind 1, parameter 0x47: one ring (+1 0) and eight
    beams (+1 1, +0xB `(i & 3) << 3`, +9 `(i >> 2) x 0x28 + 0x11`), each
    counted in +0xB; sound 0x100.
  - `LastResortChild_Task` dispatches by +1 through `LastResortChild_Kinds`.
  - The ring (`LastResortRing_Run`, steps `BarrierRing_Grow`,
    `MagicFx_WaitOwnerChildren`, `MagicFx_CountDownRelease`): under the
    actor matrix, a disc of sixteen gouraud triangles of radius 0xC0
    (`_DrawDisc`) and a band of sixteen quads between radius 0xC0 and 0x10
    (`_DrawBand`), shaded +9 x 5 through dword `0x903854`, each committed to
    layer 5 between draw-mode packets.
  - `MagicFx_WaitOwnerChildren` (+2 on once the owner's +0xB is 1 or less) is
    a step of thirteen overlays' tables (`analysis/magic_funcs.tsv`); it is
    this unit's by address.
  - The beams (`LastResortBeam_Run`, steps `_Wait`, `_Rise`, `_Shrink`,
    `_End`): a delay, then +0xB (the angle) up by 2 a frame while +0xA (the
    rows) grows to 0x10 and shrinks again; under the actor matrix, while the
    step is below 3, `_DrawColumn` - rows 1 .. +0xA - 1 of four stacked
    semi-transparent gouraud quads joining each row's point on a turning ring
    to the previous row's, shaded 3 x (0x10 - row) (from step 2, faded by
    +9) - and always `_DrawSparks(2)` and `_DrawSparks(3)`: trails of flat
    tiles up the same ring, one every k rows, fading by 5 k a tile.
- **Cure (MAGIC168).**
  - `Cure_Task`: a six-entry stack table (`Cure_Start`, `BattleFx_TintActor`,
    `BattleFx_Brighten`, `BattleFx_WaitStep4`, `Sparkle_End`,
    `BattleFx_Finish`) and then every live mote of its own pool.
  - The pool, `CureMote_Pool` (`0x6AE910`, 128 records of 0x2C bytes), is not
    task-shaped: a mote is run with `CureMote_Current` (`0x6AFF10`) at it and
    its +0x28 in the owner cell, and every mote step reads its record through
    that pointer. `CureMote_Alloc` / `_Free` take and clear a record.
  - `Cure_Start` clears the pool, sets the task's kind (+4) to 3 and makes
    `Cure_MoteCounts[kind]` motes, each with a variant (`Rand & 3`), its number
    and a delay `Cure_MoteDelays[kind] x (n >> 2) + 1`.
  - A mote (`CureMote_Task` / `_Run`): `_Wait` counts its delay down and then
    starts at the owner's screen point moved by `CureMote_Offsets[n & 0x1F]`
    with a little `Rand`, with a lifetime `CureMote_Lifetimes[kind]` + `Rand
    & 6`; `_Rise` sways it along a sine and lifts it; `_Fade` does the same
    while +5 counts down, then frees it. Each frame it draws an eight-point
    star of gouraud triangles (`_DrawStar`, the centre coloured
    `CureMote_Colours[kind][variant]` x +5) and, while +0xC & 3, four rays and
    four three-point lines turning with the frame counter (`_DrawRays`,
    `_DrawArcs`, screen-space floats).
- **Benediction (MAGIC169).**
  - `Benediction_Task`: a four-entry stack table (`_Start`, `_Spawn`, `_Wait`,
    `MagicFx_DoneAndFree`); while +0 and +1, the screen point and a textured
    halo (`_DrawHalo`: one 0x70 x 0x60 flat-textured quad above the point,
    tpage `Gpu_GetTPage(1, 1, 0x340, 0x100)`, CLUT `(0, 0x1FA)`, shaded +9
    << 3); then every live mote of its pool, as tasks.
  - `Benediction_Start`: the pool cleared, the task at the side's centre
    (`MagicFx_CenterOnSide`) raised 0xC00000, CLUT row 26 restored with its
    STP bits (then its first word without), sound 0x100.
  - `Benediction_Spawn`: after 16 frames, **when the target byte has 0x80**
    (the party side), one child (kind 1, parameter 8) per party member
    (`0x904AB0`), +4 the member, delays 1, 0x29, 0x51, ...
  - A child (`BenedictionChild_Task` / `_Run`, steps `_Start`, `_Tint`,
    `_Brighten`, MAGIC073's `ActorFx_WaitStep4`, `_Fade`, `_End`): after its
    delay it stands at party record +4 and releases 24 motes of the pool;
    tints the member black and brightens the tint over 8 frames (sound 0x101
    for an odd member, 0x102 for an even one), fades it back and flashes the
    member; once its motes have ended it counts its parent down, sets target
    flag 0x40 on the member **when `Battle_ActorIsOut` answers non-zero** (as
    the code reads; what that means in play is not measured), and frees
    itself.
  - The pool, `BenedictionMote_Pool` (`0x6AFF18`, 80 task-shaped records of
    0x84 bytes, +0x80 the owner), run by `Benediction_Task` as
    `Sprite_Current`. A mote (`BenedictionMote_Task` / `_Run`): `_Launch`
    starts it on a circle of radius 8 round its owner (angle `(+0xB & 0xF) <<
    8`), with a rise speed and three colour weights from `Rand`; `_Spiral`
    widens the circle by 2 and rises, decelerating; `_Fade` widens by 1 and
    counts down on odd frames, then frees the record through MAGIC219's
    `0x4F6290`. Each frame: MAGIC077's `ReviveMote_Draw` and a ring of eight
    gouraud quads (`_DrawGlow`) between radius 8 and 0x14. `_Spiral` and
    `_DrawGlow` are also MAGIC077's (group S17 calls them by address).

## 2. Divergence

No ledger entry. Each function is a faithful replacement, except that a
phase past any of the twelve dispatch tables (six stack tables, six `.data`
tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3).

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: the projections get a depth and a flag pointer.
`CureMote_Alloc` / `BenedictionMote_Alloc` answer in al; ours returns the
index in eax (the callers read al).

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F6290` | MAGIC219 (S37) | tail jmp of `BenedictionMote_Fade`: `Sprite_Current` bytes 0..4 cleared (the pool record freed) |

By name, already ours: `Revive_TintSource` and `ReviveMote_Draw`
(`magic_s17.cpp`), `BattleFx_Brighten`, `BattleFx_TintActor`,
`BattleFx_WaitStep4`, `BattleFx_Finish` (`battle_odds.cpp`), `Sparkle_End`
(`magic_fx_reached.cpp`), `Barrier_Fade` and `BarrierRing_Grow`
(`magic_s19.cpp`), `MagicFx_CountDownRelease` (`magic_s12.cpp`),
`ActorFx_WaitStep4` (`magic_s16.cpp`), `MagicFx_DoneAndFree`
(`magic_engine.cpp`), `MagicFx_CenterOnSide` (`magic_lib.cpp`),
`MagicFx_PushActorMatrix` (`battle_items.cpp`), and the GTE / GPU / sprite /
sound library.

Shared bodies: `magic_funcs.tsv` has `LastResort_WaitChildren` reached from
MAGIC013 and MAGIC088, `MagicFx_WaitOwnerChildren` from thirteen files, and
`BenedictionMote_Spiral` / `_DrawGlow` from MAGIC077. Those reach them
through the addresses, so taking them changes nothing for the callers.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `LastResortChild_Kinds` | `0x65C05C` | 2 |
| `LastResortRing_Steps` | `0x65C064` | 3 |
| `LastResortBeam_Steps` | `0x65C070` | 4 |
| `CureMote_Offsets` | `0x65C080` | 40 pairs (32 used) |
| `CureMote_Colours` | `0x65C120` | 20 triples |
| `Cure_MoteCounts` | `0x65C15C` | 5 (+3 padding) |
| `Cure_MoteDelays` | `0x65C164` | 5 (+3) |
| `CureMote_Lifetimes` | `0x65C16C` | 5 (+3) |
| `CureMote_TaskTable` | `0x65C174` | 1 |
| `BenedictionChild_TaskTable` | `0x65C178` | 1 |
| `BenedictionMote_TaskTable` | `0x65C17C` | 1 |
| `CureMote_Pool` | `0x6AE910` | 128 x 0x2C |
| `CureMote_Current` | `0x6AFF10` | 1 pointer |
| `BenedictionMote_Pool` | `0x6AFF18` | 80 x 0x84 |

Each count is where the next table starts, or the index mask; the tables
indexed by the kind hold five kinds, of which `Cure_Start` sets only 3.

## 5. The fuzz

`BOF3X_SHADOW=magic_s35` runs `magic_harness::Run` over the 46 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s35_fuzz.cpp`:

- **Callees** (42 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own (S31's);
  - the projections log their SVECTORs through `deref` (6 bytes each);
  - MAGIC168's mote steps - the ones called directly (`CureMote_Task`,
    `_DrawStar`, `_Free`) and the ones in its tables (`_Run`, `_Wait`,
    `_Rise`, `_Fade`) - are `kPhase` callees that also log
    `CureMote_Current`, since a mote is not `Sprite_Current`;
  - this group's other functions called directly, `ReviveMote_Draw` and
    `0x4F6290` as `kPhase`; the two allocators as `kByte` (0xFF, or an index
    in the pool);
  - `Sprite_SetTint`, `Battle_ActorIsOut` (`kFlag`), the libgpu setters.
- **Tables:** the six `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`;
  `MoveScript_TintRecords`; CLUT row 26 and its source; both pools and
  `CureMote_Current` (`0x6AE910..0x6B2857`). 39,900 bytes of state in 16
  regions.
- **Seed:** every pool record's owner pointer (+0x28, +0x80) at one of the
  harness's slots or records (`Cure_Task` / `Benediction_Task` put it in the
  owner cell, which the harness's disturbance writes through);
  `CureMote_Current` at a record; both pools full one round in eight (the
  allocators' 0xFF); each dispatcher inside its table; each count-down one
  step before and at its threshold; the beam's rows mostly below 0x20 and
  its +9 across the step-2 clamp; the sparks' step-3 branch; the party
  count 0..4 and the side bit for `Benediction_Spawn`; the member index 0..2
  for the children; the mote rise's two dwords equal a third of the time;
  `LastResortBeam_DrawSparks`' k 2 or 3 (else 1..6; `Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte, `CureMote_Current`, a byte of a Cure mote (below its owner pointer),
  a byte of a Benediction mote (below its owner pointer), a tint byte.

Result in this worktree (2026-09-27):

    shadow      magic_s35 self-test: 92000 rounds over 46 functions (2000 each), 3253795 calls to the stand-ins,
                0 MISMATCHES; 39900 bytes of state (16 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (3,193,919 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches). The count above is after the allocators' seed was added (section 6).

## 6. Controls

228 plants (L- Last Resort, C- its column, S- its trails, U- Cure, B- Benediction), each put in `magic_s35.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s35`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches, exit 0). **227 of 228 refused by a count** (exit 3, a count only in the functions the plant touches); the other, **B18** (`Benediction_Spawn` one child more, as `<=` on the u8 counter), made the self-test hang - at a seeded party count of 0xFF the planted loop never ends - so it was refused, but not by a count; its near variant **B18b** (the same extra child, bounded) was refused by a count. No equivalent mutant; none left standing.

The first run of the controls, on the seed before the allocators' boundary case was added, left **U57** (`CureMote_Alloc` searching 127 records) standing: the seed filled the pool wholly or not at all, never all but the last. The seed now leaves one record free half the time (the last a third of those); every control was re-run on it and the table is that run's.

The thinnest (fewest rounds):

- **U30** (Wait: y pointer after Rand): 1
- **U31** (Wait: x offset read before Rand): 2
- **B59** (Orbit: angle not re-read): 3
- **S9** (Sparks: kept at 1): 5
- **B3** (Task: Sprite_Current not re-read): 39
- **C14** (Column: z from bp, not re-read): 41

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| L1 | LastResort_Task: entries 0/1 swapped | LastResort_Task 700 |
| L2 | Start: facing from source +9 | LastResort_Start 1892 |
| L3 | Start: +9 0x11 | LastResort_Start 1586 |
| L4 | Start: ring +9 1 | LastResort_Start 1676 |
| L5 | Start: ring word +0x3E from +0x3C | LastResort_Start 1686 |
| L6 | Start: beam +0xB sl 2 | LastResort_Start 2000 |
| L7 | Start: beam +9 + 0x12 | LastResort_Start 2000 |
| L8 | Start: seven beams | LastResort_Start 2000 |
| L9 | Start: sound 0x101 | LastResort_Start 2000 |
| L10 | Start: beam kind 0 | LastResort_Start 2000 |
| L11 | WaitChildren: below 1 | LastResort_WaitChildren 664 |
| L12 | Child_Task: kinds swapped | LastResortChild_Task 2000 |
| L13 | Ring_Run: shade x 4 | LastResortRing_Run 1012 |
| L14 | Ring_Run: gate on +1 | LastResortRing_Run 1023 |
| L15 | Ring_Run: steps 0/1 swapped | LastResortRing_Run 1353 |
| L16 | WaitOwnerChildren: at 2 | MagicFx_WaitOwnerChildren 635 |
| L17 | Disc: radius 0xC1 | LastResortRing_DrawDisc 2000 |
| L18 | Disc: rim blue 2 | LastResortRing_DrawDisc 2000 |
| L19 | Disc: half by sar | LastResortRing_DrawDisc 502 |
| L20 | Disc: fifteen triangles | LastResortRing_DrawDisc 2000 |
| L21 | Disc: layer 4 | LastResortRing_DrawDisc 2000 |
| L22 | Disc: rim x / y swapped | LastResortRing_DrawDisc 2000 |
| L23 | Band: inner radius 0x80 | LastResortRing_DrawBand 2000 |
| L24 | Band: inner shade 2 | LastResortRing_DrawBand 2000 |
| L25 | Band: outer blue 0 | LastResortRing_DrawBand 2000 |
| L26 | Band: closing tpage 0x14 | LastResortRing_DrawBand 2000 |
| L27 | Band: inner z 1 | LastResortRing_DrawBand 2000 |
| L28 | Beam_Run: column below 2 | LastResortBeam_Run 279 |
| L29 | Beam_Run: second trail 4 | LastResortBeam_Run 780 |
| L30 | Beam_Run: +2 not tested | LastResortBeam_Run 245 |
| L31 | Beam_Wait: +0xA 2 | LastResortBeam_Wait 517 |
| L32 | Beam_Rise: up to 0x10 | LastResortBeam_Rise 487 |
| L33 | Beam_Rise: at 0x17 | LastResortBeam_Rise 515 |
| L34 | Beam_Rise: +0xB by 3 | LastResortBeam_Rise 2000 |
| L35 | Beam_Shrink: at 0x29 | LastResortBeam_Shrink 1002 |
| L36 | Beam_Shrink: +0xA up | LastResortBeam_Shrink 2000 |
| L37 | Beam_End: at 0x19 | LastResortBeam_End 1003 |
| L38 | Beam_End: owner not counted down | LastResortBeam_End 517 |
| C1 | Column: first radius 0x61 | LastResortBeam_DrawColumn 1999 |
| C2 | Column: first height sl 3 | LastResortBeam_DrawColumn 1994 |
| C3 | Column: 3 +0xA first | LastResortBeam_DrawColumn 1923 |
| C4 | Column: height word from +0xA | LastResortBeam_DrawColumn 1920 |
| C5 | Column: runs at +0xA 1 | LastResortBeam_DrawColumn 44 |
| C6 | Column: radius + 0xC1 | LastResortBeam_DrawColumn 1925 |
| C7 | Column: height x 11 | LastResortBeam_DrawColumn 1925 |
| C8 | Column: top i / 4 | LastResortBeam_DrawColumn 1881 |
| C9 | Column: first z word + | LastResortBeam_DrawColumn 1925 |
| C10 | Column: l20 a quarter | LastResortBeam_DrawColumn 1925 |
| C11 | Column: quad 1 z0 + 1 | LastResortBeam_DrawColumn 1925 |
| C12 | Column: quad 1 z2 - 1 | LastResortBeam_DrawColumn 1925 |
| C13 | Column: x sl 8 | LastResortBeam_DrawColumn 1925 |
| C14 | Column: z from bp, not re-read | LastResortBeam_DrawColumn 41 |
| C15 | Column: clamp above 0xF | LastResortBeam_DrawColumn 62 |
| C16 | Column: step 1 branch | LastResortBeam_DrawColumn 1879 |
| C17 | Column: edge x 13 | LastResortBeam_DrawColumn 1924 |
| C18 | Column: quad 1 first row at 2 | LastResortBeam_DrawColumn 1925 |
| C19 | Column: quad 2 z3 l1c | LastResortBeam_DrawColumn 1925 |
| C20 | Column: quad 2 z0 + half | LastResortBeam_DrawColumn 1924 |
| C21 | Column: quad 2 lower edge 1 | LastResortBeam_DrawColumn 1925 |
| C22 | Column: quad 3 l20 + l1c | LastResortBeam_DrawColumn 1925 |
| C23 | Column: quad 3 z2 half - h | LastResortBeam_DrawColumn 1925 |
| C24 | Column: quad 4 z3 l10 + l20 | LastResortBeam_DrawColumn 1925 |
| C25 | Column: quad 4 z2 h + half | LastResortBeam_DrawColumn 1925 |
| C26 | Column: next y + 1 | LastResortBeam_DrawColumn 1881 |
| C27 | Column: next x from +0x10 | LastResortBeam_DrawColumn 1925 |
| C28 | Column: next height + 1 | LastResortBeam_DrawColumn 1881 |
| C29 | Column: next l10 + 1 | LastResortBeam_DrawColumn 1881 |
| C30 | Column: quad 4 tpage 0x15 | LastResortBeam_DrawColumn 1925 |
| C31 | Column: quad 4 first row at 2 | LastResortBeam_DrawColumn 1925 |
| C32 | Column: one row more | LastResortBeam_DrawColumn 299 |
| C33 | Column: quad 4 top edge 2 | LastResortBeam_DrawColumn 1925 |
| C34 | Column: quad 3 +0x24 unset | LastResortBeam_DrawColumn 1925 |
| C35 | Column: angle & 0x3F | LastResortBeam_DrawColumn 1843 |
| C36 | Column: quad 2 angle not re-read | LastResortBeam_DrawColumn 542 |
| S1 | Sparks: shade 0x7C | LastResortBeam_DrawSparks 1381 |
| S2 | Sparks: step 4 k | LastResortBeam_DrawSparks 1367 |
| S3 | Sparks: row up to +9 | LastResortBeam_DrawSparks 51 |
| S4 | Sparks: step-3 limit 0x17 | LastResortBeam_DrawSparks 85 |
| S5 | Sparks: step-3 shade 0x81 | LastResortBeam_DrawSparks 606 |
| S6 | Sparks: step-3 lift k + 1 | LastResortBeam_DrawSparks 595 |
| S7 | Sparks: jitter & 7 | LastResortBeam_DrawSparks 1313 |
| S8 | Sparks: lift + 9 | LastResortBeam_DrawSparks 1574 |
| S9 | Sparks: kept at 1 | LastResortBeam_DrawSparks 5 |
| S10 | Sparks: z sl 8 | LastResortBeam_DrawSparks 1584 |
| S11 | Sparks: rows by 2 | LastResortBeam_DrawSparks 907 |
| S12 | Sparks: on at 8 | LastResortBeam_DrawSparks 51 |
| S13 | Sparks: depth at +0xC | LastResortBeam_DrawSparks 1584 |
| S14 | Sparks: angle + 1 | LastResortBeam_DrawSparks 1584 |
| U1 | Cure_Task: entries 0/1 swapped | Cure_Task 637 |
| U2 | Cure_Task: owner not put back | Cure_Task 1640 |
| U3 | Cure_Task: bit 1 | Cure_Task 2000 |
| U4 | Cure_Task: the last mote skipped | Cure_Task 1140 |
| U5 | Cure_Start: kind 2 | Cure_Start 1955 |
| U6 | Cure_Start: +9 9 | Cure_Start 1588 |
| U7 | Cure_Start: +2 not cleared | Cure_Start 2000 |
| U8 | Cure_Start: variant & 7 | Cure_Start 1935 |
| U9 | Cure_Start: delay n >> 1 | Cure_Start 1878 |
| U10 | Cure_Start: +7 n + 1 | Cure_Start 1973 |
| U11 | Cure_Start: one mote more | Cure_Start 450 |
| U12 | Cure_Start: none free at 0x7F | Cure_Start 699 |
| U13 | Cure_Start: mote owner the owner | Cure_Start 1946 |
| U14 | CureMote_Task: Wait | CureMote_Task 2000 |
| U15 | CureMote_Run: Rise / Fade swapped | CureMote_Run 1294 |
| U16 | CureMote_Run: +0xC & 1 | CureMote_Run 110 |
| U17 | CureMote_Run: rays frame >> 2 | CureMote_Run 326 |
| U18 | CureMote_Run: arcs + 5 | CureMote_Run 326 |
| U19 | CureMote_Run: arcs radius w / 4 | CureMote_Run 325 |
| U20 | CureMote_Run: gate on +1 | CureMote_Run 407 |
| U21 | Wait: at 1 | CureMote_Wait 1019 |
| U22 | Wait: +0x1C from +0x38 | CureMote_Wait 524 |
| U23 | Wait: y from +0x2E | CureMote_Wait 524 |
| U24 | Wait: 0 takes the negative side | CureMote_Wait 103 |
| U25 | Wait: negative side + rnd | CureMote_Wait 146 |
| U26 | Wait: y from the x column | CureMote_Wait 524 |
| U27 | Wait: word +0xA & 3 | CureMote_Wait 213 |
| U28 | Wait: lifetime & 7 | CureMote_Wait 249 |
| U29 | Wait: +5 0x11 | CureMote_Wait 524 |
| U30 | Wait: y pointer after Rand | CureMote_Wait 1 |
| U31 | Wait: x offset read before Rand | CureMote_Wait 2 |
| U32 | Sway: +0xC by 2 | CureMote_Rise 2000, CureMote_Fade 2000 |
| U33 | Sway: angle sl 5 | CureMote_Rise 1971, CureMote_Fade 1968 |
| U34 | Sway: x 25 | CureMote_Rise 2000, CureMote_Fade 2000 |
| U35 | Sway: y down | CureMote_Rise 2000, CureMote_Fade 2000 |
| U36 | Rise: against +5 | CureMote_Rise 499 |
| U37 | Fade: every other frame | CureMote_Fade 337 |
| U38 | Fade: owner not counted down | CureMote_Fade 520 |
| U39 | Rays: three rays | CureMote_DrawRays 2000 |
| U40 | Rays: radius a byte | CureMote_DrawRays 1993 |
| U41 | Rays: outer x from y | CureMote_DrawRays 2000 |
| U42 | Rays: outer red 2 | CureMote_DrawRays 2000 |
| U43 | Rays / arcs: shade x 5 | CureMote_DrawRays 1994, CureMote_DrawArcs 1993 |
| U44 | Arcs: middle a quarter | CureMote_DrawArcs 2000 |
| U45 | Arcs: end at the middle | CureMote_DrawArcs 2000 |
| U46 | Arcs: end red 0 | CureMote_DrawArcs 2000 |
| U47 | Rays / arcs: packet layer 1 | CureMote_DrawRays 2000, CureMote_DrawArcs 2000 |
| U48 | Star: radius Rand & 7 | CureMote_DrawStar 1008 |
| U49 | Star: colour index transposed | CureMote_DrawStar 1764 |
| U50 | Star: blue from green | CureMote_DrawStar 1183 |
| U51 | Star: rim +6 | CureMote_DrawStar 1987 |
| U52 | Star: sixteen points | CureMote_DrawStar 2000 |
| U53 | Star: y2 from x | CureMote_DrawStar 2000 |
| U54 | Star: rim blue | CureMote_DrawStar 1903 |
| U55 | Alloc: bits 0 and 1 | CureMote_Alloc 901 |
| U56 | Alloc: none free 0xFE | CureMote_Alloc 107 |
| U57 | Alloc: 127 motes | CureMote_Alloc 342 |
| U58 | Free: +3 1 | CureMote_Free 2000 |
| B1 | Benediction_Task: entries 0/1 swapped | Benediction_Task 1045 |
| B2 | Task: halo gated on +2 | Benediction_Task 435 |
| B3 | Task: Sprite_Current not re-read | Benediction_Task 39 |
| B4 | Task: Sprite_Current not put back | Benediction_Task 1968 |
| B5 | Task: the first mote skipped | Benediction_Task 1125 |
| B6 | Start: lift 0xC00001 | Benediction_Start 1989 |
| B7 | Start: CLUT bit 14 | Benediction_Start 2000 |
| B8 | Start: first word keeps its bit | Benediction_Start 1005 |
| B9 | Start: 255 CLUT words | Benediction_Start 2000 |
| B10 | Start: +9 1 | Benediction_Start 2000 |
| B11 | Start: mote +2 not cleared | Benediction_Start 2000 |
| B12 | Spawn: at 0x11 | Benediction_Spawn 1022 |
| B13 | Spawn: side bit 0x40 | Benediction_Spawn 240 |
| B14 | Spawn: first delay 2 | Benediction_Spawn 240 |
| B15 | Spawn: delay step 0x20 | Benediction_Spawn 201 |
| B16 | Spawn: +4 the delay | Benediction_Spawn 240 |
| B17 | Spawn: parameter 9 | Benediction_Spawn 240 |
| B18 | Spawn: one child more | not by a count: the self-test hung (timeout) - the planted `<=` on a u8 never ends at a party count of 0xFF; see B18b |
| B18b | Spawn: one child more, counted in int | Benediction_Spawn 239 |
| B19 | Wait: at +0xB 1 | Benediction_Wait 1040 |
| B20 | Wait: on at 1 | Benediction_Wait 544 |
| B21 | Halo: tpage 0xB4 | Benediction_DrawHalo 2000 |
| B22 | Halo: packet layer 2 | Benediction_DrawHalo 2000 |
| B23 | Halo: shade sl 2 | Benediction_DrawHalo 1994 |
| B24 | Halo: x1 + 0x37 | Benediction_DrawHalo 2000 |
| B25 | Halo: y0 - 0x5F | Benediction_DrawHalo 2000 |
| B26 | Halo: y2 + 1 | Benediction_DrawHalo 2000 |
| B27 | Halo: tpage abr 0 | Benediction_DrawHalo 2000 |
| B28 | Halo: CLUT 0x1FB | Benediction_DrawHalo 2000 |
| B29 | Halo: v 0x69 | Benediction_DrawHalo 2000 |
| B30 | Halo: size 0x44 | Benediction_DrawHalo 2000 |
| B31 | Halo: x from +0x30 | Benediction_DrawHalo 2000 |
| B32 | Child_Task: the mote's run | BenedictionChild_Task 2000 |
| B33 | Child_Run: Fade for Tint | BenedictionChild_Run 345 |
| B34 | Child_Start: facing from +9 | BenedictionChild_Start 320 |
| B35 | Child_Start: height from +0x38 | BenedictionChild_Start 357 |
| B36 | Child_Start: 23 motes | BenedictionChild_Start 548 |
| B37 | Child_Start: delay n >> 1 | BenedictionChild_Start 548 |
| B38 | Child_Start: +0xB n + 1 | BenedictionChild_Start 548 |
| B39 | Child_Start: none free 0xFE | BenedictionChild_Start 128 |
| B40 | Child_Start: mote owner the owner | BenedictionChild_Start 534 |
| B41 | Child_Start: next member | BenedictionChild_Start 359 |
| B42 | Tint: blue 1 | BenedictionChild_Tint 501 |
| B43 | Tint: +9 7 | BenedictionChild_Tint 501 |
| B44 | Tint: slot to +0xB | BenedictionChild_Tint 501 |
| B45 | Brighten: two channels | BenedictionChild_Brighten 2000 |
| B46 | Brighten: sounds swapped | BenedictionChild_Brighten 520 |
| B47 | Fade: record stride 11 | BenedictionChild_Fade 1991 |
| B48 | Fade: flash +5 | BenedictionChild_Fade 490 |
| B49 | End: flag when in | BenedictionChild_End 1002 |
| B50 | End: owner not counted down | BenedictionChild_End 986 |
| B51 | End: at +0xB 1 | BenedictionChild_End 1005 |
| B52 | Mote_Task: the child's run | BenedictionMote_Task 2000 |
| B53 | Mote_Run: tpage 0x36 | BenedictionMote_Run 2000 |
| B54 | Mote_Run: gate on +1 | BenedictionMote_Run 511 |
| B55 | Mote_Run: closing tpage 0x14 | BenedictionMote_Run 2000 |
| B56 | Mote_Run: draw before the screen point | BenedictionMote_Run 768 |
| B57 | Orbit: angle & 0x1F | BenedictionMote_Launch 282, BenedictionMote_Spiral 999, BenedictionMote_Fade 964 |
| B58 | Orbit: x from the owner's +0x38 | BenedictionMote_Launch 530, BenedictionMote_Spiral 2000, BenedictionMote_Fade 2000 |
| B59 | Orbit: angle not re-read | BenedictionMote_Spiral 1, BenedictionMote_Fade 2 |
| B60 | Rise: also when equal | BenedictionMote_Spiral 911, BenedictionMote_Fade 979 |
| B61 | Rise: by +0x20 | BenedictionMote_Spiral 555, BenedictionMote_Fade 513 |
| B62 | Launch: radius 9 | BenedictionMote_Launch 526 |
| B63 | Launch: lift 0x800001 | BenedictionMote_Launch 521 |
| B64 | Launch: rise sl 19 | BenedictionMote_Launch 530 |
| B65 | Launch: step an eighth | BenedictionMote_Launch 530 |
| B66 | Launch: colours + 5 | BenedictionMote_Launch 530 |
| B67 | Launch: +0xA 0x11 | BenedictionMote_Launch 530 |
| B68 | Spiral: radius by 3 | BenedictionMote_Spiral 1988 |
| B69 | Spiral: +9 by 1 | BenedictionMote_Spiral 2000 |
| B70 | Spiral: at 0x12 | BenedictionMote_Spiral 472 |
| B71 | Fade: radius by 2 | BenedictionMote_Fade 1989 |
| B72 | Fade: frame bit 1 | BenedictionMote_Fade 1093 |
| B73 | Fade: owner not counted down | BenedictionMote_Fade 202 |
| B74 | Glow: outer 0x15 | BenedictionMote_DrawGlow 2000 |
| B75 | Glow: colour unsigned | BenedictionMote_DrawGlow 984 |
| B76 | Glow: inner x at the outer radius | BenedictionMote_DrawGlow 2000 |
| B77 | Glow: step 0x1FF | BenedictionMote_DrawGlow 2000 |
| B78 | Glow: +0x34 green | BenedictionMote_DrawGlow 1967 |
| B79 | Glow: size 0x40 | BenedictionMote_DrawGlow 2000 |
| B80 | Alloc2: bits 0 and 2 | BenedictionMote_Alloc 943 |
| B81 | Alloc2: none free 0xFE | BenedictionMote_Alloc 137 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Things
to look for, as the code reads:

- Last Resort: a disc and band round the caster, eight columns of quads
  turning up round it with trails of tiles.
- Cure: motes rising from the target in stars and flickering lines.
- Benediction: a halo over the side's centre; with the party side targeted,
  each member tinted, brightened and flashed, with 24 motes spiralling up.

`Benediction_Spawn` makes nothing when the target byte lacks 0x80, so cast at
a single member the spell shows only its halo; whether that case arises in
play is not measured. `Cure_Start` sets the kind to 3; the other four kinds
of its tables are read by nothing here.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the six stack tables and the six
  `.data` tables (an index past a one-entry task table reads the next
  overlay's table). Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `LastResort_Start` (nine calls) and `Benediction_Spawn`: slot 255 is
  `0x93A000 + 255 x 0x84 = 0x9423FC`, past the image's end (`0x93F000`) - an
  access violation, in ours as in the original (the same address is
  written). The two pool allocators' 0xFF **is** tested by their callers.
- **`BenedictionChild_*` index the party records by +4 unchecked** (`0x802D40
  + 0x14C x +4`); `Benediction_Spawn` sets it below the party count
  (`0x904AB0`), so it stays inside the records as long as that count does.
- **`LastResortBeam_DrawSparks` never ends for a step k of 0 or below** (the
  row does not advance past +9, the shade does not fall): its only callers
  pass 2 and 3.
- **`CureMote_DrawStar` indexes `CureMote_Colours` by kind x 4 + variant**,
  unchecked; with the kind `Cure_Start` sets (3) and a variant below 4 it
  stays inside the table.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 40 lines under a `group S35` comment: 33 new,
plus seven host extents re-listed smaller (`004EFA00 1F7`, `004F0470 1F7`,
`004F0850 12`, `004F1120 4A`, `004F13C0 133`, `004F1850 12`, `004F1DE0 57`,
each the function's own size; the hosts ran on over the functions after
them). Six were listed right already (`004EF860`, `004EFD30`, `004F0B70`,
`004F0D00`, `004F0EF0`, `004F1BD0`).
