# Group S13: Sudden Death (MAGIC063)

**Status:** IN PROGRESS (2026-09-26). All 25 functions are ours
(`src/game/magic_s13.cpp`, shadow name `magic_s13`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 50,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
this spell, so this is fuzz only until the owner sees it cast.

Round nine, fourth spell wave, group S13
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 143 | 0x254 | MAGIC063 | 0x3F | Sudden Death | `0x4B2F40..0x4B3EFE` | 25 |

The extent is `tools/magic_rows.py --unit MAGIC063 --clones` (capstone
recursive descent; every jump internal, no jump table, nothing `REFUSED`).
All 25 functions lie in it; none was found inside or missing from it, and
none was ours before. 3,796 bytes, as the queue counted. The next unit,
MAGIC064, starts at `0x4B3F00`.

The name is the sibling's label read one id down
([`cut-content.md`](cut-content.md) §2) - a hypothesis. What the spell looks
like or does in play has not been measured; everything below is what the
code does.

## 1. What each function does

**The task** (row 143, a kind-2 task):

| Function | Address | Bytes | Does |
|---|---|--:|---|
| `SuddenDeath_Task` | `0x4B2F40` | 0x61 | a two-entry stack table by +1 (`SuddenDeath_Spawn`, then S30's `MagicFx_EndWhenChildrenDone`); then the walk of the mote pool: each record with bit 0 becomes `SuddenDeath_CurrentMote`, its +0x1C the owner `0x93B940`, `SuddenDeathMote_Task` called, the owner (saved once, after the phase) put back |
| `SuddenDeath_Spawn` | `0x4B2FB0` | 0x131 | the pool's +0..+2 cleared; +0xB, +9 0, +1 on; a child (`BattleTask_Create(1, 0x68)`: +0x80 this task, +1 0, +4 the actor, this task's +0xB up) on every enemy 3..10, then every member 0..2, that `Battle_ActorIsOut` does not answer for and that is not the acting actor (`0x904B34`); `Sound_PlayById(0x100)` |

**The children** (kind 1, parameter 0x68), one per actor:

| Function | Address | Bytes | Does |
|---|---|--:|---|
| `SuddenDeathChild_Task` | `0x4B30F0` | 0x12 | `jmp [SuddenDeathChild_Kinds + 4 * +1]` (one entry) |
| `SuddenDeathChild_Run` | `0x4B3110` | 0x3E | a five-entry stack table by +2 (the five below) |
| `SuddenDeathChild_Start` | `0x4B3150` | 0xB1 | the actor +4's record (party below 3, else enemy +4 - 3): its x / z, its height + 0x2000000; +9 a delay of (`Rand` & 0xF) + 1; +0xB 0, +2 on; one **orbit** mote (+1 1) from the pool, owned by the child and counted in its +0xB (0xFF, a full pool, skipped) |
| `SuddenDeathChild_Delay` | `0x4B3210` | 0x3A | +9 down; at 0: if the parent's +0xB is 1 (every other child has counted itself out), step 3; else the parent's +0xB down and step 2 |
| `SuddenDeathChild_WaitMotes` | `0x4B3250` | 0x12 | its motes gone (+0xB 0): the task freed |
| `SuddenDeathChild_Burst` | `0x4B3270` | 0xA2 | its orbit mote gone: 92 **burst** motes (+1 0), mote n with +4 n, a delay +0xA of (`Rand` & 0xF) + (n / 32) x 16 + 1 and a spin count +8 of 0x40 less that, each counted in +0xB; sounds 0x101 and 0x102; step 4 |
| `SuddenDeathChild_End` | `0x4B3320` | 0x26 | its motes gone: `Battle_SetTargetFlag40(+4)`, the parent's +0xB down, the task freed. Also the second entry of C1's `InkInkActor_Phases` (MAGIC146) |

So, by reading: every child shows an orbit mote on its actor and, after a
random delay of 1..16 frames, all but one free themselves when their mote has
faded. The **last child to finish its delay** (the parent's count 1 at that
moment: the random delays decide it, and between equal delays the task slot
order) waits for its own orbit mote, then bursts 92 motes on its actor, and
at its end flags that one actor 0x40 (and the burst's mote 0 flags it 0x10,
below). Whether that is the spell's target choice in play is not measured.

**The motes** (`SuddenDeath_Motes`, 96 records of 0x20 bytes; the one being
run `SuddenDeath_CurrentMote`, its owner the child). Every step re-reads the
cell after every call.

| Function | Address | Bytes | Does |
|---|---|--:|---|
| `SuddenDeathMote_Task` | `0x4B3350` | 0x12 | `jmp [SuddenDeathMote_Kinds + 4 * +1]` (two entries: burst, orbit) |
| `SuddenDeathBurst_Run` | `0x4B3370` | 0x33 | `call [SuddenDeathBurst_Steps + 4 * +2]` (four); then while +0 and +2 are set: the matrix, the draw, `Gte_PopMatrix` |
| `SuddenDeathBurst_Launch` | `0x4B33B0` | 0x141 | +0xA down; at 0 the mote is thrown out: radius ((+4 / 32) + 6) x 16, angle (+4 & 0x1F) x 128 (the words `0x903850` / `0x903854`), its point the owner's plus sin / cos x the radius, its height the owner's; colours +5..+7 (`Rand` & 7) + 5; +3, +9 0; +0xA (`Rand` & 3) x 8; +0xB `Rand`; +0xC the radius; step 1 |
| `SuddenDeathBurst_Close` | `0x4B3500` | 0xCA | the radius +0xC in by 2; the point; +0xB up; +9 up by 2 below 0x10; at a radius not above (+4 / 32) x 8 + 0x20: step 2, and mote 0 calls `Battle_SetTargetFlags(the owner's +4, 0x10)` |
| `SuddenDeathBurst_Spin` | `0x4B35D0` | 0xE3 | the angle +0x40 (mod 0x1000); the point; +0xB up; +5 up to 0xC, +6 / +7 down to 4, +3 up to 7; +8 down, at 0 step 3 |
| `SuddenDeathBurst_Rise` | `0x4B36C0` | 0xBC | the angle +0x40; the point; the height word +0x1A up by +0xA; +0xB up; on odd frames (`Frame_Counter` bit 0) +9 down, at 0 the owner's +0xB down and `SuddenDeathMote_Free` (tail jmp) |
| `SuddenDeathMote_PushMatrix` | `0x4B3780` | 0xBC | `Gte_PushMatrix`; `Camera_Matrix` x a turn about z of (0x800 - +0xE + +3 x 128) & 0xFFF, translated by `Gte_RotTrans` of ((+0x10 >> 9) - 0x4000, (+0x14 >> 9) - 0x4000, -(+0x1A / 2)) - the shape of S31's orb push, with its own turn |
| `SuddenDeathMote_Draw` | `0x4B3840` | 0x245 | a draw-mode packet (tpage 0x35, 0xC bytes) linked at the mote; its colour +5 / +6 / +7 x +9 into `0x90385A` / C / E; four semi-transparent gouraud triangles, triangle i from the origin to the points (radius, angle) of `SuddenDeathMote_Shape`'s words i and i + 4, both lifted by sin(((+0xB + `SuddenDeathMote_Turns[i]`) & 0xF) x 256) x 32 >> 12; the first vertex coloured (E, C, A), the others (A, C, E); each through `Gte_RotTransPers3` / `Gte_PrimDepths3_10B`, linked at the mote (0x34 bytes) |
| `SuddenDeathOrbit_Run` | `0x4B3A90` | 0x33 | as the burst's, through `SuddenDeathOrbit_Steps` (five) |
| `SuddenDeathOrbit_Start` | `0x4B3AD0` | 0xC6 | the point 32 from the owner at angle 0, its height; colours; +0xE 0, +3 7, +9 0, +0xA 0x40, +0xB `Rand`; step 1 |
| `SuddenDeathOrbit_Brighten` | `0x4B3BA0` | 0x91 | the angle +0x20; the point (radius 32); +9 up 2, +0xB up; at +9 0x10 step 2 |
| `SuddenDeathOrbit_Circle` | `0x4B3C40` | 0xAB | the angle +0x20; the point; +0xB up; +0xA down, at 0: +0xA again - 0x40 while the owner child is at its burst (step 3), else 1 - and step 3 |
| `SuddenDeathOrbit_Spin` | `0x4B3CF0` | 0xC0 | the owner at step 3: +0xB up 4 and the angle 0x40, else 1 and 0x20; the point; +0xA down, at 0 step 4 |
| `SuddenDeathOrbit_Fade` | `0x4B3DB0` | 0xCA | the same by 2 / 1; +9 down, at 0 the owner's +0xB down and `SuddenDeathMote_Free` (tail jmp) |
| `SuddenDeathMote_Alloc` | `0x4B3E80` | 0x4F | the first of the 96 records without bit 0 gets it, its index in al; al 0xFF when none (eax above al not set) |
| `SuddenDeathMote_Free` | `0x4B3ED0` | 0x2F | bytes +0..+4 of the current mote cleared, the cell read again for each |

So an orbit mote fades in (+9 up to 0x10) circling its actor at 32, circles
on, spins faster once its child is at the burst, and fades out; a burst mote
waits, is thrown out to a radius of 96, 112 or 128 by its number (n / 32),
at an angle of (n & 0x1F) x 128, closes in to 32, 40 or 48, spins while its colours move toward (0xC, 4, 4), then rises and fades
on odd frames. The colours are +5..+7 scaled by +9, the brightness.

## 2. Divergence

None. Every function is a faithful replacement; a phase past a stack or
`.data` table aborts where the original would call through whatever follows
it (the precedent, [`magic_fx_reached.md`](magic_fx_reached.md) §3).
`DIVERGENCE.md` and `src/game/cheats.cpp` patch no byte inside the extent
(grepped 2026-09-26 for each address). No ledger entry.

## 3. Calls to other units

| Address | Owner | How | From |
|---|---|---|---|
| `0x4E5200` | S30's `MagicFx_EndWhenChildrenDone` (MAGIC131) | stack-table immediate, `bof3::addr` by name | `SuddenDeath_Task` entry 1 |
| `0x4B3320` (ours) | - | C1's `InkInkActor_Phases` holds it | MAGIC146 |

No raw-address call into a unit not yet ours. Everything else called is the
engine's or the draw library's, all ours already: `Battle_ActorIsOut`,
`BattleTask_Create`, `BattleTask_FreeCurrent`, `Battle_SetTargetFlags`,
`Battle_SetTargetFlag40`, `Sound_PlayById`, `Rand`, `Math_Sin` / `Math_Cos`,
`MapView_LinkPrimAt`, `Gpu_SetDrawMode` / `_SetPolyG3` / `_SetSemiTrans`, the
GTE (`Gte_PushMatrix`, `_RotTrans`, `_RotMatrix`, `_MulMatrix0`,
`_SetRotMatrix`, `_SetTransMatrix`, `_RotTransPers3`, `_PrimDepths3_10B`,
`_PopMatrix`). The group's own functions its functions call directly
(`SuddenDeathMote_Task`, `_PushMatrix`, `_Draw`, `_Alloc`, `_Free`) are called
by address, as the originals call them.

## 4. Named data (`symbols.toml` `[[data]]`)

| Name | Address | What |
|---|---|---|
| `SuddenDeathChild_Kinds` | `0x65ABA4` | one code pointer (`SuddenDeathChild_Run`); `SuddenDeathMote_Kinds` follows |
| `SuddenDeathMote_Kinds` | `0x65ABA8` | two (`SuddenDeathBurst_Run`, `SuddenDeathOrbit_Run`) |
| `SuddenDeathBurst_Steps` | `0x65ABB0` | four (Launch, Close, Spin, Rise) |
| `SuddenDeathMote_Shape` | `0x65ABC0` | sixteen words: the four triangles' first and second radii, then their angles |
| `SuddenDeathMote_Turns` | `0x65ABE0` | four bytes added to the mote's +0xB for each triangle's lift |
| `SuddenDeathOrbit_Steps` | `0x65ABE4` | five (Start, Brighten, Circle, Spin, Fade) |
| `SuddenDeath_Motes` | `0x682680` | the pool, 96 x 0x20 |
| `SuddenDeath_CurrentMote` | `0x683280` | the current mote |

`magic_rows.py`'s notes count the four tables' code entries as 1, 2, 4 and
5, the same as read. Ours reads the shape words and turn bytes in place,
from the exe's `.data`; none of their values is written in the source.

## 5. The fuzz

`BOF3X_SHADOW=magic_s13` runs `magic_harness::Run` over the 25 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s13_fuzz.cpp`:

- **Callees** (20 listed; the standard set supplies the rest):
  - `MapView_LinkPrimAt` has an `effect` that logs the primitive's bytes
    (`NoteBytes`, the size the call names) and moves `Gfx_PacketNext` on
    through a 0x2000-byte buffer of the fuzz's own: the four triangles are
    built at the same pointer, so without the log only the last would be
    compared;
  - `Gte_RotTransPers3` logs its three SVECTORs through `deref` (6 bytes
    each) and writes screen points where the real one writes (S01's
    effect); the matrix push's GTE callees log what their pointers hold and
    write a result where the real ones write (S22's effects);
  - `Battle_ActorIsOut` answers `kFlag` (its callers test al);
  - this group's own functions called directly: `SuddenDeathMote_Task`,
    `_PushMatrix`, `_Draw`, `_Free` as `kPhase` recorders that also log the
    current mote cell; `SuddenDeathMote_Alloc` as a `kByte` of 0xFF or
    0..0x5F (full, or an index inside the pool - what the real one answers).
- **Answers:** `SuddenDeathMote_Alloc` has `ret_mask 0xFF`: its al is logged
  after each pass and compared.
- **Tables:** the four `.data` tables of section 4, swapped for recorders.
- **Regions** beyond the standard ones: the pool and its cell
  (`0x682680..0x683283`); `SuddenDeathMote_Shape` / `_Turns`;
  `Gfx_PacketNext` and the packet buffer; `Prim_VertexScratch`'s three
  SVECTORs; `0x903850..0x90385F`. 22,708 bytes of state in 14 regions.
- **Seed:** every round the packet pointer into the buffer, every mote's
  owner +0x1C a real slot or record (the walk hands it to the recorders as
  the owner), the current cell a mote, and the exe's shape `.data` two
  rounds in three (read at start-up); each dispatcher inside its table;
  each count one step before and at its threshold (the delays at 1, Close's
  +9 about 0x10 and its radius one to three above the limit with mote 0 half
  the time, Spin's four clamps, Brighten's 0x10); the owner child at step 3
  half the time for the orbit's steps; the acting actor 0..10 for the spawn;
  the pool full or filled to a point for the allocator.
- **Disturb** (the group's case): the current cell to another mote,
  `Gfx_PacketNext`, a scratch or vertex word, a byte of the current mote
  below its owner field.

Result in this worktree (2026-09-26):

    shadow      magic_s13 self-test: 50000 rounds over 25 functions (2000 each), 476713 calls to the stand-ins,
                0 MISMATCHES; 22708 bytes of state (14 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`: e.g. `Battle_ActorIsOut` 22,000, `Battle_SetTargetFlags`
312, `SuddenDeathMote_Free` 725, each of the nine mote steps 364..519).
`BOF3X_SHADOW='*'`: exit 0, no mismatch in any group.

## 6. Controls

CONTROLS_BODY

## 7. What nothing reached

No recorded route casts this spell (queue §5); the live check is the owner
casting it, with a save that has it or DIV-0045's cheat. Things to look
for, by reading: an orbiting mote on every actor but the caster, then a
burst of motes closing in on one of them, spinning and rising away.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the task's two-entry and the
  child's five-entry stack tables, and the four `.data` tables. A child's +1
  of 1 would run `SuddenDeathMote_Kinds[0]`, the burst mote's run, on the
  child task. Nothing sets them past their tables; ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `SuddenDeath_Spawn`: slot 255 is `0x93A000 + 255 x 0x84 = 0x9423FC`, past
  the image's end - an access violation, in ours as in the original (the same
  address is written). With up to ten children and other effects running,
  whether 48 slots can run out here is not measured.
- **The actor records are indexed unchecked** (`SuddenDeathChild_Start`,
  +4 - 3 for an enemy); the spawn only sets 0..10.
- **The burst can run short of motes**: 92 burst motes plus up to ten orbit
  motes of children still fading can ask for more than the pool's 96;
  `SuddenDeathMote_Alloc` answers 0xFF and the caller skips the mote
  (checked, not a defect). If mote 0 is the one skipped, the burst's
  `Battle_SetTargetFlags(..., 0x10)` is never called. Not measured.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 24 lines under a `group S13` comment: 21 new,
plus three host extents re-listed smaller (`004B3350 12`, `004B3840 245`,
`004B3E80 4F`; the hosts `42C`, `63A` and `D04` ran over their
neighbours). `004B3780 BC` was listed right already.
