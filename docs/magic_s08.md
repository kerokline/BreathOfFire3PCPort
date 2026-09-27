# Group S08: Berserk, Counter / Mind's Eye, WardOfLight / Resist and Evil Eye (MAGIC041, 042, 043, 044)

**Status:** IN PROGRESS (2026-09-26). All 59 functions are ours
(`src/game/magic_s08.cpp`, shadow name `magic_s08`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 118,000 rounds. 221 of 221 negative controls refused, 220 by a count (exit 3) and one by a fault, whose variant is refused by a count. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S08
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability ids | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 80 | 0x240 | MAGIC041 | 0x29 | Berserk | `0x4A6440..0x4A6DE9` | 15 |
| 81 | 0x241 | MAGIC042 | 0x1A, 0x2A | Counter, Mind's Eye | `0x4A6DF0..0x4A7822` | 13 |
| 110 | 0x242 | MAGIC043 | 0x2B, 0x99 | WardOfLight, Resist | `0x4A7830..0x4A8246` | 14 |
| 96 | 0x243 | MAGIC044 | 0x2C | Evil Eye | `0x4A8250..0x4A9822` | 17 |

The extents are `tools/magic_rows.py --unit MAGIC04N --clones` (capstone
recursive descent; no jump table, nothing `REFUSED`). All 59 functions lie in
the units' extents; none was found inside or missing from them, and none was
ours before. 12,823 bytes, as the queue counted. The rows are as
`analysis/magic_rows.tsv` pairs them (MAGIC043 is row 110, MAGIC044 row 96:
the queue lists the rows in number order).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. One reading supports
MAGIC043's: its code tests the ability word `0x904B80` for 0x2B, one of the two
ids that load it (WardOfLight read one id down), and only then applies a buff.
What each spell looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Berserk (MAGIC041).**
  - `Berserk_Task`, the kind-2 task: a six-entry stack table by +1
    (`_Start`, `_Tint`, `_Brighten`, `_WaitChildren`, `_Fade`,
    `BattleFx_Finish`).
  - `Berserk_Start`: the owner's position; eight rings and a glow, all kind 1
    parameter 0x3D (`BerserkChild_Task`, a jmp through `BerserkChild_Kinds` by
    +1), the rings delayed 1, 9, .., 0x39 frames; sound 0x100.
  - `_Tint` darkens the **source** sprite (`0x904B4C`) after 0xA frames,
    `_Brighten` steps its tint record up for 8 frames (red by 2, green and blue
    by 1), `_WaitChildren` waits until one child is left, `_Fade` steps it
    back, releases the tint and flashes the target.
  - A ring (`BerserkRing_*`): waits its delay, opens (+0xA up by 4 to 0x20),
    fades (+9 up by 4, +0xA down to 0, then the owner's count down and freed).
    `BerserkRing_Draw` is 64 gouraud quads round the caster: the lower edge a
    circle of radius 0x80 on the ground, the upper edge a wavy circle (radius
    +9 - sin x 16 + 0xA0, height +0xA x 8 + 0x20 - sin x 8), the wave's phase
    `(Rand & 3) + +9 + i`; bright red below, dark above, linked at layer 0.
  - The glow (`BerserkGlow_*`): steps `_Start`, the library's
    `MagicFx_CountUp9By2`, S19's `BarrierLine_Wait` and MAGIC060's
    `0x4B1740`; `BerserkGlow_Draw` is a disc of sixteen gouraud triangles,
    radius 0x88, the rim +9 x 8 in red and a quarter of that in green and
    blue, at layer 5.
- **Counter / Mind's Eye (MAGIC042).**
  - `Counter_Task`: `_Start` (one mark, kind 1 parameter 0x3E, sound 0x100),
    `BattleFx_Finish`.
  - The mark (`CounterMark_*`, `CounterChild_Task` through
    `CounterChild_Kinds`): `_Start` takes the screen point (`_Place`), then
    `_Open`, `_Grow`, `_Swell`, `_Fade` count +9 (a radius) and +0xA (a
    radius) up and down while +0xB turns; each frame a disc of sixteen
    triangles (`_DrawDisc`), spokes (`_DrawSpokes`: per step three lines, one
    of +9 and two of two thirds of it a pixel above and below) and three arcs
    of four lines (`_DrawArc`, radius +0xA, angles +0xB + 8, 0x10, 0x18).
  - `CounterMark_Place`: `BattleActor_UpdateScreenXY`, then **for a party
    target only** an offset from one of two word-pair tables, by direction
    (+8 >> 1) and a row: `CounterMark_MemberOffsets` by the member's +0x89
    (its character), or, while the member's +0x134 has bit 1,
    `CounterMark_FormOffsets` by `CounterMark_FormIndex[0x904B89]`. The x is
    added for a direction of 0 or 3 and taken away otherwise.
- **WardOfLight / Resist (MAGIC043).**
  - `Ward_Task`: a five-entry stack table by +1 (`_Start`, `_Tint`,
    `_Brighten`, `_Fade`, `BattleFx_Finish`), then every live record of its
    own pool `WardMote_Pool` (64 records of 0x84 at `0x67BC40`) run as
    `Sprite_Current` with its +0x80 as the owner.
  - `Ward_Start` clears the pool, takes the source sprite's direction and
    position, takes eight motes (`WardMote_Alloc`, delays 1..0x39), copies row
    26's first 32 CLUT entries from their source, sound 0x100.
  - `_Tint` darkens the source sprite, `_Brighten` / `_Fade` step its tint
    record up and down by 2 four frames each, back and forth **while two or
    more motes live**; then the tint released, the target flashed, and for
    ability 0x2B only, `MagicFx_ApplyBuff(1, target)` and a child (kind 1,
    0x48) whose +4 is 0 when the buff took, else 8.
  - A mote (`WardMote_*`; `WardMote_Task` through `WardMote_TaskTable`,
    `WardMote_Run` through `WardMote_Steps`): `_Launch` waits its delay, then
    starts 0x20000 out from the owner turned by the owner's direction (the
    engine's `0x446770`), 0x1000000 above it, with a step of 0x1000 back
    toward it and a heading (+0x14) from the field's kind-2 point to the owner
    (`Math_Ratan2`); `_Open` (0x10 frames, +0xA up by 2), `_Fly` (to +9 0x18),
    `_End` (+0xA down by 2, then the owner's count down and the record freed).
    Each frame its matrix (`_PushMatrix`: rotation 0x400 about x and
    `WardMote_Tilts[+8]` about y) and a band of eight quads between radii +9 x
    12 and +9 x 8 (`_Draw`), coloured from +0xA - randomly per channel for
    ability 0x2B, fixed otherwise.
- **Evil Eye (MAGIC044).**
  - `EvilEye_Task`: `_Start` (two beams, kind 1 parameter 6, +4 0 and 1; row
    26's third CLUT entries 1..15 with STP, entry 0 cleared; sound 0x100),
    `BattleFx_Finish`.
  - A beam (`EvilEyeBeam_*`, through `EvilEyeChild_Kinds` and
    `EvilEyeBeam_Steps`): `_Start` places it off the owner by the acting
    actor (a party member, or an enemy by its record's +0x8C: 0x9A, 0xB5,
    other) and heads it at the source sprite; `_Seek` steps toward the source
    (`MagicFx_StepTowardPoint`, speed 0xC0) and snaps onto it when
    `MagicFx_NearSprite` says so or when its heading turned by more than 0x600
    and less than 0xA00; beam 0 flags the target 0x10 until then. `_Trail`
    lets the trail grow to 16 points and sheds a spark every fourth frame;
    `_End` erases the trail from its head over 16 frames.
  - The trail (`EvilEyeBeam_Trails`, 32 screen points a beam at `0x67DD40`):
    `_Record` pushes the points back and writes the screen point at +0xB,
    `_Erase` writes 0xFFFF. Beam 1 draws it thin (`_DrawThin`, 2 wide, a ribbon
    along each segment's `Math_Ratan2` heading), beam 0 wide (`_DrawWide`, 0xC,
    the two edges as two passes), shaded down the trail.
  - A spark (`EvilEyeSpark_*`, through `EvilEyeSpark_Steps`): `_Start` below
    the screen point with three colour weights `Rand & 3` + 1, `_Grow` (up-left
    a pixel a frame, +9 up by 4 to 0x10), `_Fade` (+0xA down, then freed);
    `_Draw` is one textured quad (tpage 0x55, CLUT (0x20, 0x1FA)).

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedents:

- a phase past any of the fourteen dispatch tables (seven stack tables, seven
  `.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3);
- `CounterMark_DrawSpokes` aborts for a first angle of 0xE0 or more. The
  original counts its steps in the byte of its argument and stops at the
  first step not below `first + 0x20`: from 0xE0 the byte wraps below that
  end for ever, emitting lines. Its one caller (`CounterMark_Run`) passes 0,
  so no play reaches it (group S31's `CoronaRay_PushMatrix` precedent).

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: `Gte_RotTrans` gets a flag pointer, the projections a
depth and a flag pointer. `MagicFx_StepTowardPoint`'s fourth argument, which
the original leaves as a stack word, is 0 in ours (the callee does not read
it: the standard listing masks it 0).

## 3. Calls to other units

By raw address (a phase in a table, or the engine's; never bound or renamed
here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4B1740` | MAGIC060 (S12) | entry 3 of `BerserkGlow_Run`'s stack table |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (`WardMote_Launch`, `EvilEyeBeam_Start`), as S22, S23 and S31 call it |

By name, already ours: `BattleFx_Finish` (the tasks' last entries),
`MagicFx_CountUp9By2` (S30) and `BarrierLine_Wait` (S19) in `BerserkGlow_Run`'s
table, the library's `MagicFx_PushActorMatrix`, `_ApplyBuff`,
`_StepTowardPoint`, `_NearSprite` and `BattleActor_*`, and the GTE / GPU /
sprite / sound functions.

Shared bodies: `analysis/magic_funcs.tsv` lists `Berserk_WaitChildren`
(`0x4A6640`) as reached from MAGIC118 too: S27's `Burn_Task` holds it as entry
2 of its stack table (`kWaitOneChild` in `magic_s27_callees.h`, by address),
so it can now be called by name. It also lists MAGIC042's mark functions as
reached from MAGIC041: that is the tool reading three entries at
`BerserkChild_Kinds`, the third being `CounterChild_Kinds`' (section 4); by
reading, MAGIC041 creates only parameter 0x3D (its own children).

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `BerserkChild_Kinds` | `0x65A82C` | 2 |
| `CounterChild_Kinds` | `0x65A834` | 1 |
| `CounterMark_MemberOffsets` | `0x65A838` | 22 word pairs |
| `CounterMark_FormOffsets` | `0x65A890` | 22 word pairs |
| `CounterMark_FormIndex` | `0x65A8E8` | 28 bytes |
| `WardMote_TaskTable` | `0x65A904` | 1 |
| `WardMote_Steps` | `0x65A908` | 4 |
| `WardMote_Tilts` | `0x65A918` | 4 words |
| `EvilEyeChild_Kinds` | `0x65A924` | 2 |
| `EvilEyeBeam_Steps` | `0x65A92C` | 4 |
| `EvilEyeSpark_Steps` | `0x65A93C` | 3 |
| `WardMote_Pool` | `0x67BC40` | 64 x 0x84 |
| `EvilEyeBeam_Trails` | `0x67DD40` | 2 x 32 points |

Each count is where the next table starts (a dump of `0x65A800..0x65A97F`
read 2026-09-26; MAGIC045's table follows at `0x65A948`). The tool's ".data"
notes counted every code pointer that follows (3, 5, 25, 23, 19): the harness
would have swapped the next overlays' entries too.

## 5. The fuzz

`BOF3X_SHADOW=magic_s08` runs `magic_harness::Run` over the 59 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s08_fuzz.cpp`:

- **Callees** (44 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` log the primitive's
    bytes and move `Gfx_PacketNext` on through a 0x2000-byte buffer of the
    fuzz's own (S31's);
  - the projections log their SVECTORs through `deref`; the matrix push's
    GTE callees log theirs and write a result where the real ones write;
  - `0x446770` logs the task's direction and pair and writes a new pair;
    `BattleActor_UpdateScreenXY` logs the sprite and writes its screen point,
    which the callers read back;
  - **the `kFlag` blind spot**: `MagicFx_NearSprite` answers a whole-eax 0 or
    1 from its own stream (`EvilEyeBeam_Seek` tests all 32 bits, which the
    standard kFlag's garbage upper bits would make almost always true) and
    moves `Sprite_Current` a quarter of the time whatever it answers, so the
    "not near" path's re-reads are exercised. `MagicFx_ApplyBuff`'s answer is
    followed by `BattleTask_Create` before anything is read again, which
    disturbs on its own;
  - `Math_Ratan2` answers, for `EvilEyeBeam_Seek` only, a heading whose turn
    from the old one is 0x5FF, 0x600, 0x601, 0x9FF, 0xA00 or 0xA01 half the
    time (the snap's two bounds);
  - `BattleTask_Create` answers 0xFF a quarter of the time for
    `EvilEyeBeam_Trail`, the one caller that tests it;
  - this group's own functions that others of it call directly, as `kPhase`
    (the task they ran for), the two argument draws by their arguments, and
    `WardMote_Alloc` answering "none" (0xFF) or a record 0..0x3F.
- **Tables:** the seven `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `Field_Kind2Z` / `_X`;
  `MoveScript_TintRecords`; CLUT row 26 and its source; `0x904B50..0x904B9F`
  (the ability word `0x904B80`, the form byte `0x904B89`); `WardMote_Pool`;
  `EvilEyeBeam_Trails` and 0x500 bytes past it. 33,772 bytes in 19 regions.
- **Seed:** each dispatcher inside its table; each count one step before and
  at its threshold; the ability word 0x2B (or 0x2A, 0x2C, 0x99) two times in
  three; the pool full, part-taken or as filled, every record's owner a task
  or a sprite record; for `CounterMark_Place` a party target and the
  member's +0x134 bit 1 flipped half the time; for `EvilEyeBeam_Start` the
  enemy kind 0x9A / 0xB5 / 0x9B / 0xB4; `Frame_Counter & 3` 0 for `_Trail`;
  `Ward_Fade`'s live count 0..2; the trail's +0xA / +0xB small and its head
  point erased (0xFFFF) half the time, the next a quarter;
  `CounterMark_DrawSpokes`' first angle below 0xE0 (a sixth of the time
  0xD0..0xDF) and `_DrawArc`'s at 0, 0x7F, 0x80, 0xFF half the time
  (`Group::args`).
- **Settle:** while MAGIC044's functions run, the beam byte +4 of
  `Sprite_Current` is put back to 0 / 1 after every disturbance (and the four
  seedable tasks' at the seed): the trail index (+4 x 32 + point) is
  unchecked in the original, and a disturbed +4 would write far outside any
  compared region.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, `Field_Kind2*`, a tint byte, a pool record's live bit, a trail word
  (0xFFFF half the time), the ability word or the form byte.

Result in this worktree (2026-09-26):

    shadow      magic_s08 self-test: 118000 rounds over 59 functions (2000 each), 3385563 calls to the stand-ins,
                0 MISMATCHES; 33772 bytes of state (19 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`; the thinnest, `MagicFx_ApplyBuff`, 178 calls).
`BOF3X_SHADOW='*'`: exit 0 (3,408,401 stand-in calls for this group in that
run, 0 mismatches).

## 6. Controls

221 plants, each put in `magic_s08.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s08`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). **221 of 221 refused**: 220 by exit 3 with a count only in the functions the plant touches (B-, C-, W-, E- are Berserk, Counter, Ward, Evil Eye), one (E34, the 0xFF test dropped) by an access violation - ours writes slot 255 as the original would, a fault on the ours pass only - and its variant E34b by a count. No equivalent mutant was planted; none was left standing.

The thinnest (fewer than 60 rounds):

- **B9** (Tint: source read before the release): 11
- **B39** (Glow_Draw: / 4 by shift): 44
- **C30** (Spokes: two thirds written twice): 7
- **W13** (Tint: source read again): 11
- **W51** (Alloc: 63 records): 4
- **E25** (Seek: >= 0x600): 16
- **E26** (Seek: <= 0xA00): 17
- **E34b** (Trail: slot 0x2F taken as none): 29
- **E46** (Thin: far right edge from the register): 37
- **E51** (TrailStart: beam read after the call): 30
- **E53** (Corner: cos from the register): 58
- **E53** (Corner: cos from the register): 43

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| B1 | Berserk_Task: entries 0/1 swapped | Berserk_Task 685 |
| B2 | Start: +9 0xB | Berserk_Start 1596 |
| B3 | Start: ring delay + 1 | Berserk_Start 2000 |
| B4 | Start: glow +1 2 | Berserk_Start 2000 |
| B5 | Start: glow parameter 0x3E | Berserk_Start 2000 |
| B6 | Start: seven rings | Berserk_Start 2000 |
| B7 | Tint: tint blue 1 | Berserk_Tint 511 |
| B8 | Tint: slot into +0xA | Berserk_Tint 511 |
| B9 | Tint: source read before the release | Berserk_Tint 11 |
| B10 | Brighten: +2 by 3 | Berserk_Brighten 2000 |
| B11 | Brighten: at 9 | Berserk_Brighten 985 |
| B12 | WaitChildren: at 2 | Berserk_WaitChildren 483 |
| B13 | Fade: +3 by 2 | Berserk_Fade 2000 |
| B14 | Fade: the actor flashed | Berserk_Fade 457 |
| B15 | Child_Task: the table one on | BerserkChild_Task 2000 |
| B16 | Ring_Run: Grow / Fade swapped | BerserkRing_Run 1347 |
| B17 | Ring_Run: bit 1 of +0 | BerserkRing_Run 657 |
| B18 | Ring_Start: +0xA 1 | BerserkRing_Start 502 |
| B19 | Ring_Grow: at 0x1C | BerserkRing_Grow 479 |
| B20 | Ring_Fade: +9 by 3 | BerserkRing_Fade 1998 |
| B21 | Ring_Fade: owner +0xA | BerserkRing_Fade 500 |
| B22 | Glow_Run: steps 1/2 swapped | BerserkGlow_Run 965 |
| B23 | Glow_Start: +9 1 | BerserkGlow_Start 2000 |
| B24 | Ring_Draw: lower radius 0x81 | BerserkRing_Draw 1995 |
| B25 | Ring_Draw: first angle & 7 | BerserkRing_Draw 986 |
| B26 | Ring_Draw: upper radius + 0xA1 | BerserkRing_Draw 2000 |
| B27 | Ring_Draw: closes at 0x3F | BerserkRing_Draw 1985 |
| B28 | Ring_Draw: the lift sl 8 | BerserkRing_Draw 2000 |
| B29 | Ring_Draw: z not lifted | BerserkRing_Draw 2000 |
| B30 | Ring_Draw: mode linked at layer 1 | BerserkRing_Draw 2000 |
| B31 | Ring_Draw: green 0x31 | BerserkRing_Draw 2000 |
| B32 | Ring_Draw: around i sl 5 | BerserkRing_Draw 2000 |
| B33 | Ring_Draw: first height + 0x21 | BerserkRing_Draw 1983 |
| B34 | Ring_Draw: height sin sl 2 | BerserkRing_Draw 2000 |
| B35 | Ring_Draw: V4 from VA | BerserkRing_Draw 2000 |
| B36 | Glow_Draw: radius 0x89 | BerserkGlow_Draw 1998 |
| B37 | Glow_Draw: rim x 4 | BerserkGlow_Draw 1974 |
| B38 | Glow_Draw: commit 0x30 | BerserkGlow_Draw 2000 |
| B39 | Glow_Draw: / 4 by shift | BerserkGlow_Draw 44 |
| B40 | Glow_Draw: fifteen triangles | BerserkGlow_Draw 2000 |
| B41 | Glow_Draw: V4 not cleared | BerserkGlow_Draw 1996 |
| C1 | Counter_Task: entries swapped | Counter_Task 2000 |
| C2 | Start: parameter 0x3F | Counter_Start 2000 |
| C3 | Start: facing from the owner +9 | Counter_Start 1965 |
| C4 | Mark_Run: Grow / Swell swapped | CounterMark_Run 784 |
| C5 | Mark_Run: spokes radius +0xA | CounterMark_Run 815 |
| C6 | Mark_Run: two arcs | CounterMark_Run 822 |
| C7 | Mark_Run: arc radius +9 | CounterMark_Run 816 |
| C8 | Mark_Start: +9 1 | CounterMark_Start 2000 |
| C9 | Mark_Start: facing from the owner +0xB | CounterMark_Start 1979 |
| C10 | Mark_Open: at 5 | CounterMark_Open 990 |
| C11 | Mark_Grow: by 9 | CounterMark_Grow 2000 |
| C12 | Mark_Grow: at 0x48 | CounterMark_Grow 521 |
| C13 | Mark_Swell: +9 by 5 | CounterMark_Swell 2000 |
| C14 | Mark_Fade: +9 down at 0 too | CounterMark_Fade 1033 |
| C15 | Mark_Fade: +0xA by 3 | CounterMark_Fade 2000 |
| C16 | Mark_Fade: no spin | CounterMark_Fade 1999 |
| C17 | Place: target 3 too | CounterMark_Place 80 |
| C18 | Place: +0x134 bit 0 | CounterMark_Place 729 |
| C19 | Place: the form byte unmapped | CounterMark_Place 697 |
| C20 | Place: member +0x88 | CounterMark_Place 755 |
| C21 | Place: added for facing 2 | CounterMark_Place 340 |
| C22 | Place: y from the x cell | CounterMark_Place 1417 |
| C23 | Place: facing & 1 | CounterMark_Place 1038 |
| C24 | Place: target read before the update | CounterMark_Place 69 |
| C25 | Spokes: y from +0x2E | CounterMark_DrawSpokes 1994 |
| C26 | Spokes: two thirds rounded late | CounterMark_DrawSpokes 684 |
| C27 | Spokes: angle sl 6 | CounterMark_DrawSpokes 2000 |
| C28 | Spokes: 0xC1 | CounterMark_DrawSpokes 2000 |
| C29 | Spokes: sides swapped | CounterMark_DrawSpokes 2000 |
| C30 | Spokes: two thirds written twice | CounterMark_DrawSpokes 7 |
| C31 | Spokes: step 0x11 | CounterMark_DrawSpokes 2000 |
| C32 | Spokes: commit 0x20 | CounterMark_DrawSpokes 2000 |
| C33 | Arc: angle mask 0x3F | CounterMark_DrawArc 2000 |
| C34 | Arc: step 0x10 | CounterMark_DrawArc 2000 |
| C35 | Arc: end + 0x60 | CounterMark_DrawArc 2000 |
| C36 | Arc: blue 0x21 | CounterMark_DrawArc 2000 |
| C37 | Arc: radius a byte | CounterMark_DrawArc 1989 |
| C38 | Disc: +0xA / 4 | CounterMark_DrawDisc 1973 |
| C39 | Disc: step 0x100 | CounterMark_DrawDisc 2000 |
| C40 | Disc: centre 0x61 | CounterMark_DrawDisc 2000 |
| C41 | Disc: commit 0x30 | CounterMark_DrawDisc 2000 |
| C42 | Disc: y from S0 | CounterMark_DrawDisc 2000 |
| W1 | Ward_Task: Tint / Brighten swapped | Ward_Task 785 |
| W2 | Walk: bit 1 | Ward_Task 2000 |
| W3 | Walk: the owner not put back | Ward_Task 1642 |
| W4 | Walk: owner read at each record | Ward_Task 1642 |
| W5 | Start: +9 8 | Ward_Start 1898 |
| W6 | Start: index + 1 | Ward_Start 2000 |
| W7 | Start: 0x3F taken as none | Ward_Start 259 |
| W8 | Start: index counts takes | Ward_Start 211 |
| W9 | Start: 31 CLUT entries | Ward_Start 2000 |
| W10 | Start: facing from the source +9 | Ward_Start 1884 |
| W11 | Start: pool +2 kept | Ward_Start 2000 |
| W12 | Tint: +9 5 | Ward_Tint 533 |
| W13 | Tint: source read again | Ward_Tint 11 |
| W14 | Brighten: two channels | Ward_Brighten 2000 |
| W15 | Brighten: +9 3 | Ward_Brighten 483 |
| W16 | Fade: back at 3 | Ward_Fade 157 |
| W17 | Fade: back is on | Ward_Fade 486 |
| W18 | Fade: ability 0x2C | Ward_Fade 216 |
| W19 | Fade: +4 inverted | Ward_Fade 178 |
| W20 | Fade: buff 2 | Ward_Fade 178 |
| W21 | Fade: parameter 0x47 | Ward_Fade 178 |
| W22 | Fade: child +0xA 1 | Ward_Fade 178 |
| W23 | Fade: tint +9 released | Ward_Fade 443 |
| W24 | Mote_Task: the steps table | WardMote_Task 2000 |
| W25 | Mote_Run: bit 0 of +0 | WardMote_Run 367 |
| W26 | Mote_Run: draw before the matrix | WardMote_Run 1084 |
| W27 | Launch: offset 0x20001 | WardMote_Launch 527 |
| W28 | Launch: height + 0x1000001 | WardMote_Launch 527 |
| W29 | Launch: step -0xFFF | WardMote_Launch 529 |
| W30 | Launch: Ratan2 arguments swapped | WardMote_Launch 531 |
| W31 | Launch: facing from the owner +9 | WardMote_Launch 520 |
| W32 | Launch: dz from Kind2X | WardMote_Launch 531 |
| W33 | MoteMove: +9 by 2 | WardMote_Open 2000, WardMote_Fly 2000, WardMote_End 2000 |
| W34 | Open: at 0x12 | WardMote_Open 492 |
| W35 | Fly: at 0x17 | WardMote_Fly 461 |
| W36 | End: +0xA by 1 | WardMote_End 2000 |
| W37 | End: +4 kept | WardMote_End 544 |
| W38 | PushMatrix: x angle 0x401 | WardMote_PushMatrix 2000 |
| W39 | PushMatrix: tilt stride 4 | WardMote_PushMatrix 1735 |
| W40 | PushMatrix: height / 4 | WardMote_PushMatrix 2000 |
| W41 | Draw: outer x 13 | WardMote_Draw 1991 |
| W42 | Draw: red + 5 | WardMote_Draw 832 |
| W43 | Draw: blue x 5 | WardMote_Draw 1145 |
| W44 | Draw: green x 7 | WardMote_Draw 1144 |
| W45 | Draw: V1A from V10 | WardMote_Draw 1994 |
| W46 | Draw: blue from green | WardMote_Draw 1865 |
| W47 | Draw: link 0x40 | WardMote_Draw 2000 |
| W48 | Draw: seven quads | WardMote_Draw 2000 |
| W49 | Draw: ability 0x2A | WardMote_Draw 1019 |
| W50 | Draw: V4 not cleared | WardMote_Draw 1995 |
| W51 | Alloc: 63 records | WardMote_Alloc 4 |
| W52 | Alloc: +0 made 1 | WardMote_Alloc 1478 |
| W53 | Alloc: none 0xFE | WardMote_Alloc 517 |
| E1 | EvilEye_Task: entries swapped | EvilEye_Task 2000 |
| E2 | Start: parameter 7 | EvilEye_Start 2000 |
| E3 | Start: beam +4 from 1 | EvilEye_Start 2000 |
| E4 | Start: CLUT from 0x1A22 | EvilEye_Start 2000 |
| E5 | Start: entry 0 1 | EvilEye_Start 2000 |
| E6 | Start: bit 0x4000 | EvilEye_Start 2000 |
| E7 | Child_Task: phase ^ 1 | EvilEyeChild_Task 2000 |
| E8 | Beam_Run: thin / wide swapped | EvilEyeBeam_Run 1144 |
| E9 | Beam_Run: +1 for +2 | EvilEyeBeam_Run 632 |
| E10 | Beam_Start: party offset 0x4001 | EvilEyeBeam_Start 1154 |
| E11 | Beam_Start: kind 0x9B | EvilEyeBeam_Start 302 |
| E12 | Beam_Start: kind 0xB4 | EvilEyeBeam_Start 278 |
| E13 | Beam_Start: height 0x1C00001 | EvilEyeBeam_Start 542 |
| E14 | Beam_Start: party below 2 | EvilEyeBeam_Start 390 |
| E15 | Beam_Start: kind +0x8D | EvilEyeBeam_Start 304 |
| E16 | Beam_Start: point 1 y from x | EvilEyeBeam_Start 2000 |
| E17 | Beam_Start: +0xA 2 | EvilEyeBeam_Start 2000 |
| E18 | Beam_Start: Ratan2 swapped | EvilEyeBeam_Start 2000 |
| E19 | Beam_Start: height from +0x10 | EvilEyeBeam_Start 2000 |
| E20 | Seek: up to 0x11 | EvilEyeBeam_Seek 479 |
| E21 | Seek: speed 0xC1 | EvilEyeBeam_Seek 2000 |
| E22 | Seek: height sar 16 | EvilEyeBeam_Seek 2000 |
| E23 | Seek: old heading + 1 | EvilEyeBeam_Seek 1999 |
| E24 | Seek: near 0x7FFF | EvilEyeBeam_Seek 2000 |
| E25 | Seek: >= 0x600 | EvilEyeBeam_Seek 16 |
| E26 | Seek: <= 0xA00 | EvilEyeBeam_Seek 17 |
| E27 | Seek: old & 0x7FF | EvilEyeBeam_Seek 320 |
| E28 | Seek: flags unless +2 2 | EvilEyeBeam_Seek 645 |
| E29 | Seek: flags 0x20 | EvilEyeBeam_Seek 642 |
| E30 | Seek: snapped z from x | EvilEyeBeam_Seek 1533 |
| E31 | Seek: erased, not recorded | EvilEyeBeam_Seek 2000 |
| E32 | Trail: grows below 0xF | EvilEyeBeam_Trail 505 |
| E33 | Trail: every eighth frame | EvilEyeBeam_Trail 723 |
| E34 | Trail: 0xFF not tested | an access violation (exit 0xC0000005): slot 255 written past the image |
| E34b | Trail: slot 0x2F taken as none | EvilEyeBeam_Trail 29 |
| E35 | Trail: spark owned by the beam | EvilEyeBeam_Trail 959 |
| E36 | Trail: spark +1 0 | EvilEyeBeam_Trail 1090 |
| E37 | Trail: the beam counts it | EvilEyeBeam_Trail 959 |
| E38 | Trail: +9 0x11 | EvilEyeBeam_Trail 920 |
| E39 | Trail: spark height from z | EvilEyeBeam_Trail 1090 |
| E40 | Beam_End: recorded, not erased | EvilEyeBeam_End 2000 |
| E41 | Thin: width 3 | EvilEyeBeam_DrawThin 1995 |
| E42 | Thin: near 0xCE | EvilEyeBeam_DrawThin 979 |
| E43 | Thin: scratch 4 0x62 | EvilEyeBeam_DrawThin 978 |
| E44 | Thin: far 0xC2 | EvilEyeBeam_DrawThin 979 |
| E45 | Thin: near right edge + 0x400 | EvilEyeBeam_DrawThin 979 |
| E46 | Thin: far right edge from the register | EvilEyeBeam_DrawThin 37 |
| E47 | TrailStart: erased 0xFFFE | EvilEyeBeam_DrawThin 920, EvilEyeBeam_DrawWide 920 |
| E48 | TrailStart: heading arguments swapped | EvilEyeBeam_DrawThin 2000, EvilEyeBeam_DrawWide 2000 |
| E49 | TrailStart: first point j + 1 | EvilEyeBeam_DrawThin 1992, EvilEyeBeam_DrawWide 2000 |
| E50 | TrailStart: from j + 2 | EvilEyeBeam_DrawThin 979, EvilEyeBeam_DrawWide 1030 |
| E51 | TrailStart: beam read after the call | EvilEyeBeam_DrawThin 30, EvilEyeBeam_DrawWide 64 |
| E52 | Corner: sin angle & 0x7FF | EvilEyeBeam_DrawThin 979, EvilEyeBeam_DrawWide 1011 |
| E53 | Corner: cos from the register | EvilEyeBeam_DrawThin 58, EvilEyeBeam_DrawWide 43 |
| E54 | Segment heading: dy from SC | EvilEyeBeam_DrawThin 979, EvilEyeBeam_DrawWide 1030 |
| E55 | Wide: width 0xD | EvilEyeBeam_DrawWide 1994 |
| E56 | Wide: right edge - 0x300 | EvilEyeBeam_DrawWide 429 |
| E57 | Wide: near y from x | EvilEyeBeam_DrawWide 1030 |
| E58 | Wide: left near 0x68 | EvilEyeBeam_DrawWide 999 |
| E59 | Wide: right near 0x8A | EvilEyeBeam_DrawWide 429 |
| E60 | Wide: right far 0x82 | EvilEyeBeam_DrawWide 429 |
| E61 | Wide: +0x24 1 | EvilEyeBeam_DrawWide 1029 |
| E62 | Wide: +0x16 shaded | EvilEyeBeam_DrawWide 1027 |
| E63 | Shift: stops above +0xB | EvilEyeBeam_Record 1055, EvilEyeBeam_Erase 1039 |
| E64 | Shift: y from x | EvilEyeBeam_Record 1055, EvilEyeBeam_Erase 1039 |
| E65 | Record: y from x | EvilEyeBeam_Record 2000 |
| E66 | Erase: y 0xFFFE | EvilEyeBeam_Erase 2000 |
| E67 | Trail: 33 points a beam | EvilEyeBeam_Start 998, EvilEyeBeam_DrawThin 1131, EvilEyeBeam_DrawWide 1188, EvilEyeBeam_Record 1002, EvilEyeBeam_Erase 1004 |
| E68 | Spark_Run: drawn at +2 1 only when not 1 | EvilEyeSpark_Run 1010 |
| E69 | Spark_Start: 9 lower | EvilEyeSpark_Start 1958 |
| E70 | Spark_Start: Rand & 7 | EvilEyeSpark_Start 1715 |
| E71 | Spark_Start: +0xA 0x11 | EvilEyeSpark_Start 2000 |
| E72 | Spark_Grow: no Rand | EvilEyeSpark_Grow 2000 |
| E73 | Spark_Grow: at 0x14 | EvilEyeSpark_Grow 502 |
| E74 | Spark_Fade: up by 3 | EvilEyeSpark_Fade 1986 |
| E75 | Spark_Fade: +9 after the Rand | EvilEyeSpark_Fade 81 |
| E76 | Spark_Draw: radius x 3 | EvilEyeSpark_Draw 1992 |
| E77 | Spark_Draw: corners 2/3 swapped | EvilEyeSpark_Draw 2000 |
| E78 | Spark_Draw: abr 1 | EvilEyeSpark_Draw 2000 |
| E79 | Spark_Draw: CLUT row 0x1FB | EvilEyeSpark_Draw 2000 |
| E80 | Spark_Draw: v 0x41 | EvilEyeSpark_Draw 2000 |
| E81 | Spark_Draw: green unsigned | EvilEyeSpark_Draw 1025 |
| E82 | Spark_Draw: link 0x44 | EvilEyeSpark_Draw 2000 |
| E83 | Spark_Draw: y from +0x2E | EvilEyeSpark_Draw 2000 |
| E84 | Spark_Draw: mode at layer 1 | EvilEyeSpark_Draw 2000 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Things
to look for:

- Berserk: the source darkening then brightening red, eight wavy rings rising
  round it one after another, a red glow.
- Counter / Mind's Eye: a disc, spokes and turning arcs on the target, offset
  per character for a party target.
- WardOfLight / Resist: the source tinted, eight bands flying in from round
  the caster; for WardOfLight a buff and a further child effect.
- Evil Eye: two beams (one thin, one wide) curling onto the source sprite and
  shedding sparks.

`CounterMark_FormOffsets` is used only while a party member's +0x134 has bit
1 (read by [`battle_obj_states.md`](battle_obj_states.md) as a flag of the
member's form); which forms those are was not read.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the seven stack tables and the
  seven `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Berserk_Start` (nine calls), `Counter_Start`, `Ward_Fade` and
  `EvilEye_Start` (two): slot 255 is `0x93A000 + 255 x 0x84 = 0x9423FC`, past
  the image's end (`0x93F000`) - an access violation, in ours as in the
  original (the same address is written). `EvilEyeBeam_Trail` tests it;
  `Ward_Start` tests its own pool's `WardMote_Alloc`.
- **`CounterMark_DrawSpokes` never ends for a first angle of 0xE0 or more**
  (section 2; unreachable, ours aborts).
- **`CounterMark_Place` reads its tables unchecked**: the form byte
  `0x904B89` indexes the 28-byte `CounterMark_FormIndex`, and the row it gives
  (or the member's +0x89) the 22-pair tables; a large row reads the following
  `.data` (no fault, a wrong offset).
- **`WardMote_PushMatrix` reads `WardMote_Tilts` by the direction +8
  unchecked**: a direction past 3 turns the mote by the following `.data`
  words.
- **The trail is indexed unchecked** (+4 x 32 + point, `EvilEyeBeam_Record`,
  `_Erase`, the draws), and **the draws' scan for the first point not erased
  has no bound**: it relies on a point before the end not being 0xFFFF. The
  heading of the first segment reads the point after it even when that lies
  past +0xA.
- **`EvilEyeBeam_Start` indexes the enemy records by the actor byte - 3**,
  unchecked above 10.
- **`EvilEyeSpark_Grow` and `_Fade` call `Rand` and drop its answer**: the
  bit 0 test that follows sets flags nothing reads (by the shape, a left /
  right jitter that lost its branch).

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 52 lines under a `group S08` comment: 47 new,
plus five host extents re-listed smaller (`004A6C10 1DA`, `004A7690 193`,
`004A7BE0 12`, `004A81F0 57`, `004A93E0 8B`, each the function's own size).
Seven were listed right already (`004A68D0`, `004A70F0`, `004A7220`,
`004A7530`, `004A7E60`, `004A7F10`, `004A9350`).
