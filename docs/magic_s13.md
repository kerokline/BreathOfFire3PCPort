# Group S13: Sudden Death (MAGIC063)

**Status:** IN PROGRESS (2026-09-26). All 25 functions are ours
(`src/game/magic_s13.cpp`, shadow name `magic_s13`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 50,000 rounds. 152 of 153 negative controls refused (151 by a count, one by a fault with its near variant refused by a count), one equivalent with its near variant refused. Nothing recorded casts
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

    shadow      magic_s13 self-test: 50000 rounds over 25 functions (2000 each), 474506 calls to the stand-ins,
                0 MISMATCHES; 22708 bytes of state (14 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`: e.g. `Battle_ActorIsOut` 22,000, `Battle_SetTargetFlags`
291, `SuddenDeathMote_Free` 703, each of the nine mote steps 379..530).
`BOF3X_SHADOW='*'`: exit 0, no mismatch in any group (472,473 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches).

## 6. Controls

153 plants, each put in `magic_s13.cpp` one at a time by a script (not committed; the round's pattern, S31's) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s13`, restored; after the last it restored, rebuilt and ran the clean self-test (exit 0, 0 mismatches). **152 of 153 refused**: 151 by a count (exit 3) only in the functions the plant touches, one (T6) by a fault, its near variant by a count; **one equivalent** (F2), its near variant refused.

A first run (151 plants) left L11 standing - the launch's radius word read before `Math_Sin` instead of after: the group's disturbance moved that scratch word too rarely. The disturbance now moves the scratch words four times as often, half of them the launch's two, and the launch's delay is seeded at 1; every control was then re-run on that fuzz (the table). The thinnest are the re-reads across a call: MR2 (3 and 6 rounds), L11 (4), BP2 (8..12), CS8 (15).

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Task: entries 0/1 swapped | SuddenDeath_Task 2000 |
| T2 | Walk: the owner not put back | SuddenDeath_Task 1627 |
| T3 | Walk: motes with bit 1 | SuddenDeath_Task 2000 |
| T4 | Walk: 95 motes | SuddenDeath_Task 996 |
| T5 | Walk: the owner saved before the phase | SuddenDeath_Task 57 |
| T6 | Walk: the owner from +0x18 | a fault (exit 0xC0000005): a torn owner written through; its near variant T6b by a count |
| S1 | Spawn: +2 kept | SuddenDeath_Spawn 2000 |
| S2 | Spawn: +9 1 | SuddenDeath_Spawn 1847 |
| S3 | Spawn: enemies 3..9 | SuddenDeath_Spawn 2000 |
| S4 | Spawn: the caster not skipped (enemies) | SuddenDeath_Spawn 367 |
| S5 | Spawn: members 0..1 | SuddenDeath_Spawn 2000 |
| S6 | Spawn: party caster test inverted | SuddenDeath_Spawn 1378 |
| S7 | Spawn: parameter 0x69 | SuddenDeath_Spawn 1962 |
| S8 | Spawn: child +1 1 | SuddenDeath_Spawn 1962 |
| S9 | Spawn: child not counted | SuddenDeath_Spawn 1927 |
| S10 | Spawn: Sprite_Current read before the create | SuddenDeath_Spawn 177 |
| S11 | Spawn: sound 0x101 | SuddenDeath_Spawn 2000 |
| S12 | Spawn: party out test skipped | SuddenDeath_Spawn 2000 |
| S13 | Spawn: enemy out test inverted | SuddenDeath_Spawn 2000 |
| S14 | Spawn: +0 set to 1 | SuddenDeath_Spawn 2000 |
| S15 | Spawn: child +4 the actor + 1 | SuddenDeath_Spawn 1962 |
| C1 | ChildTask: the mote table | SuddenDeathChild_Task 2000 |
| C2 | ChildRun: entries 1/2 swapped | SuddenDeathChild_Run 796 |
| C3 | ChildRun: entry 3 the start | SuddenDeathChild_Run 427 |
| CS1 | ChildStart: enemy index - 2 | SuddenDeathChild_Start 1473 |
| CS2 | ChildStart: party below 2 | SuddenDeathChild_Start 174 |
| CS3 | ChildStart: height + 0x1000000 | SuddenDeathChild_Start 2000 |
| CS4 | ChildStart: x from +0x38 | SuddenDeathChild_Start 2000 |
| CS5 | ChildStart: delay Rand & 7 | SuddenDeathChild_Start 1013 |
| CS6 | ChildStart: +0xB 1 | SuddenDeathChild_Start 1984 |
| CS7 | ChildStart: mote kind 0 | SuddenDeathChild_Start 1986 |
| CS8 | ChildStart: index 0x5F skipped too | SuddenDeathChild_Start 15 |
| CS9 | ChildStart: Sprite_Current read before the alloc | SuddenDeathChild_Start 62 |
| CS10 | ChildStart: +2 not on | SuddenDeathChild_Start 1990 |
| CD1 | ChildDelay: the last at 2 | SuddenDeathChild_Delay 232 |
| CD2 | ChildDelay: the last to step 4 | SuddenDeathChild_Delay 116 |
| CD3 | ChildDelay: the parent's count kept | SuddenDeathChild_Delay 390 |
| CD4 | ChildDelay: at 1 | SuddenDeathChild_Delay 508 |
| W1 | WaitMotes: at 1 | SuddenDeathChild_WaitMotes 1025 |
| B1 | Burst: 91 motes | SuddenDeathChild_Burst 973 |
| B2 | Burst: kind 1 | SuddenDeathChild_Burst 973 |
| B3 | Burst: band n / 16 | SuddenDeathChild_Burst 973 |
| B4 | Burst: +8 0x3F less | SuddenDeathChild_Burst 973 |
| B5 | Burst: sounds swapped | SuddenDeathChild_Burst 973 |
| B6 | Burst: motes not counted | SuddenDeathChild_Burst 973 |
| B7 | Burst: +2 up 2 | SuddenDeathChild_Burst 973 |
| B8 | Burst: gate at 1 | SuddenDeathChild_Burst 976 |
| B9 | Burst: Rand & 7 | SuddenDeathChild_Burst 973 |
| B10 | Burst: +4 n + 1 | SuddenDeathChild_Burst 973 |
| E1 | ChildEnd: the target flagged | SuddenDeathChild_End 933 |
| E2 | ChildEnd: the owner's count kept | SuddenDeathChild_End 1004 |
| E3 | ChildEnd: gate at 1 | SuddenDeathChild_End 1015 |
| MT1 | MoteTask: kind ^ 1 | SuddenDeathMote_Task 2000 |
| MR1 | MoteRun: drawn at step 0 | SuddenDeathBurst_Run 353, SuddenDeathOrbit_Run 310 |
| MR2 | MoteRun: the cell read before the step | SuddenDeathBurst_Run 6, SuddenDeathOrbit_Run 3 |
| MR3 | MoteRun: draw before the matrix | SuddenDeathBurst_Run 1174, SuddenDeathOrbit_Run 1137 |
| MR4 | BurstRun: the orbit's table | SuddenDeathBurst_Run 2000 |
| MR5 | MoteRun: no pop | SuddenDeathBurst_Run 1174, SuddenDeathOrbit_Run 1137 |
| L1 | Launch: at 1 | SuddenDeathBurst_Launch 1314 |
| L2 | Launch: radius + 7 | SuddenDeathBurst_Launch 668 |
| L3 | Launch: angle & 0x3F | SuddenDeathBurst_Launch 358 |
| L4 | Launch: x by the owner's +0x38 | SuddenDeathBurst_Launch 672 |
| L5 | Launch: Cos of the radius word | SuddenDeathBurst_Launch 670 |
| L6 | Launch: height from +0x38 | SuddenDeathBurst_Launch 672 |
| L7 | Colours: Rand & 3 | SuddenDeathBurst_Launch 574, SuddenDeathOrbit_Start 1714 |
| L8 | Launch: +3 1 | SuddenDeathBurst_Launch 672 |
| L9 | Launch: +0xA << 2 | SuddenDeathBurst_Launch 484 |
| L10 | Launch: +0xC the angle | SuddenDeathBurst_Launch 670 |
| L11 | Launch: radius read before Sin | SuddenDeathBurst_Launch 4 |
| L12 | Launch: +0xB Rand + 1 | SuddenDeathBurst_Launch 672 |
| L13 | Launch: +9 1 | SuddenDeathBurst_Launch 671 |
| CL1 | Close: radius - 3 | SuddenDeathBurst_Close 1999 |
| CL2 | Close: +9 below 0x11 | SuddenDeathBurst_Close 328 |
| CL3 | Close: limit x 4 | SuddenDeathBurst_Close 546 |
| CL4 | Close: at the limit | SuddenDeathBurst_Close 435 |
| CL5 | Close: mote 1 flags | SuddenDeathBurst_Close 297 |
| CL6 | Close: flags 0x20 | SuddenDeathBurst_Close 291 |
| CL7 | Close: the task's +4 | SuddenDeathBurst_Close 251 |
| CL8 | Close: +0xB not up | SuddenDeathBurst_Close 2000 |
| BP1 | BurstPoint: x by the owner's +0x38 | SuddenDeathBurst_Close 2000, SuddenDeathBurst_Spin 2000, SuddenDeathBurst_Rise 2000 |
| BP2 | BurstPoint: the cell read before Cos | SuddenDeathBurst_Close 8, SuddenDeathBurst_Spin 12, SuddenDeathBurst_Rise 11 |
| BP3 | BurstPoint: x by Cos | SuddenDeathBurst_Close 2000, SuddenDeathBurst_Spin 2000, SuddenDeathBurst_Rise 2000 |
| TU1 | Turn: mask 0x1FFF | SuddenDeathBurst_Spin 1025, SuddenDeathBurst_Rise 1015, SuddenDeathOrbit_Brighten 1005, SuddenDeathOrbit_Circle 993, SuddenDeathOrbit_Spin 997, SuddenDeathOrbit_Fade 988 |
| SP1 | Spin: turn 0x20 | SuddenDeathBurst_Spin 2000 |
| SP2 | Spin: +5 up to 0xD | SuddenDeathBurst_Spin 429 |
| SP3 | Spin: +6 down to 3 | SuddenDeathBurst_Spin 444 |
| SP4 | Spin: +7 down to 5 | SuddenDeathBurst_Spin 420 |
| SP5 | Spin: +3 up to 8 | SuddenDeathBurst_Spin 444 |
| SP6 | Spin: +8 at 1 | SuddenDeathBurst_Spin 481 |
| R1 | Rise: height by +9 | SuddenDeathBurst_Rise 1994 |
| R2 | Rise: even frames | SuddenDeathBurst_Rise 2000 |
| R3 | Rise: the owner's count kept | SuddenDeathBurst_Rise 226 |
| R4 | Rise: +0xB not up | SuddenDeathBurst_Rise 2000 |
| R5 | Rise: +9 at 1 | SuddenDeathBurst_Rise 227 |
| P1 | PushMatrix: +3 x 64 | SuddenDeathMote_PushMatrix 1968 |
| P2 | PushMatrix: 0x900 | SuddenDeathMote_PushMatrix 2000 |
| P3 | PushMatrix: x sar 8 | SuddenDeathMote_PushMatrix 2000 |
| P4 | PushMatrix: height / 4 | SuddenDeathMote_PushMatrix 2000 |
| P5 | PushMatrix: y from +0x18 | SuddenDeathMote_PushMatrix 2000 |
| P6 | PushMatrix: the mote read before the push | SuddenDeathMote_PushMatrix 11 |
| P7 | PushMatrix: turn mask 0x7FF | SuddenDeathMote_PushMatrix 994 |
| P8 | PushMatrix: offset 0x3FFF | SuddenDeathMote_PushMatrix 2000 |
| D1 | Draw: tpage 0x36 | SuddenDeathMote_Draw 2000 |
| D2 | Draw: the mode linked as 0x10 | SuddenDeathMote_Draw 2000 |
| D3 | Draw: red by +8 | SuddenDeathMote_Draw 1960 |
| D4 | Draw: three triangles | SuddenDeathMote_Draw 2000 |
| D5 | Draw: second radius one on | SuddenDeathMote_Draw 2000 |
| D6 | Draw: lift angle & 7 | SuddenDeathMote_Draw 1407 |
| D7 | Draw: lift x 16 | SuddenDeathMote_Draw 2000 |
| D8 | Draw: first vertex (A, C, E) | SuddenDeathMote_Draw 1962 |
| D9 | Draw: triangles linked as 0x30 | SuddenDeathMote_Draw 2000 |
| D10 | Draw: opaque | SuddenDeathMote_Draw 2000 |
| D11 | Draw: the cell read once | SuddenDeathMote_Draw 270 |
| D12 | Draw: second point not lifted | SuddenDeathMote_Draw 2000 |
| D13 | ShapePoint: radius read before Sin | SuddenDeathMote_Draw 56 |
| D14 | Draw: vertex 0 z not cleared | SuddenDeathMote_Draw 1992 |
| D15 | Draw: second screen point at +0x14 | SuddenDeathMote_Draw 2000 |
| D16 | Draw: the packet read after the setters | SuddenDeathMote_Draw 2000 |
| D17 | Draw: last vertex blue from C | SuddenDeathMote_Draw 1963 |
| D18 | Draw: points swapped | SuddenDeathMote_Draw 2000 |
| D19 | Draw: colour words by +8 for blue | SuddenDeathMote_Draw 1967 |
| OS1 | OrbitStart: Sin(1) | SuddenDeathOrbit_Start 2000 |
| OS2 | OrbitStart: radius 16 | SuddenDeathOrbit_Start 2000 |
| OS3 | OrbitStart: +3 6 | SuddenDeathOrbit_Start 2000 |
| OS4 | OrbitStart: +0xA 0x3F | SuddenDeathOrbit_Start 1999 |
| OS5 | OrbitStart: +0xE 1 | SuddenDeathOrbit_Start 2000 |
| OS6 | OrbitStart: +9 1 | SuddenDeathOrbit_Start 1999 |
| OS7 | OrbitStart: height from +0x38 | SuddenDeathOrbit_Start 2000 |
| OP1 | OrbitPoint: radius 16 | SuddenDeathOrbit_Brighten 2000, SuddenDeathOrbit_Circle 2000, SuddenDeathOrbit_Spin 2000, SuddenDeathOrbit_Fade 2000 |
| OP2 | OrbitPoint: z by the owner's +0x3C | SuddenDeathOrbit_Brighten 2000, SuddenDeathOrbit_Circle 2000, SuddenDeathOrbit_Spin 2000, SuddenDeathOrbit_Fade 2000 |
| OB1 | Brighten: +9 up 3 | SuddenDeathOrbit_Brighten 2000 |
| OB2 | Brighten: at 0x12 | SuddenDeathOrbit_Brighten 657 |
| OB3 | Brighten: turn 0x40 | SuddenDeathOrbit_Brighten 2000 |
| OC1 | Circle: else 2 | SuddenDeathOrbit_Circle 252 |
| OC2 | Circle: step 2 | SuddenDeathOrbit_Circle 275 |
| OC3 | Circle: at 1 | SuddenDeathOrbit_Circle 496 |
| OC4 | Circle: +0xB not up | SuddenDeathOrbit_Circle 2000 |
| OT1 | OrbitStep: step 2 | SuddenDeathOrbit_Spin 1130, SuddenDeathOrbit_Fade 1150 |
| OT2 | OrbitStep: fast turn 0x30 | SuddenDeathOrbit_Spin 968, SuddenDeathOrbit_Fade 994 |
| OT3 | OrbitStep: slow +0xB up 2 | SuddenDeathOrbit_Spin 1032, SuddenDeathOrbit_Fade 1006 |
| OSP1 | Spin: fast 3 | SuddenDeathOrbit_Spin 968 |
| OSP2 | Spin: +0xA at 1 | SuddenDeathOrbit_Spin 530 |
| OF1 | Fade: fast 1 | SuddenDeathOrbit_Fade 992 |
| OF2 | Fade: the owner's count kept | SuddenDeathOrbit_Fade 476 |
| OF3 | Fade: at 1 | SuddenDeathOrbit_Fade 481 |
| A1 | Alloc: marked with 3 | SuddenDeathMote_Alloc 768 |
| A2 | Alloc: 95 records | SuddenDeathMote_Alloc 8 |
| A3 | Alloc: full 0xFE | SuddenDeathMote_Alloc 500 |
| A4 | Alloc: index + 1 | SuddenDeathMote_Alloc 1500 |
| F1 | Free: +4 kept | SuddenDeathMote_Free 1991 |
| F2 | Free: the cell read once | **not refused - equivalent**: no call between the five stores, so no input can move the cell between them; its near variant F3 refused |
| T6b | Walk: the owner the next mote's | SuddenDeath_Task 2000 |
| F3 | Free: +0 kept (F2's near variant) | SuddenDeathMote_Free 1991 |

## 7. What nothing reached

No recorded route casts this spell (queue §5); the live check is the owner
casting it, with a save that has it or DIV-0045's cheat. Things to look
for, by reading: an orbiting mote on every actor but the caster, then a
burst of motes closing in on one of them, spinning and rising away.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96 and D123 in [`known-defects.md`](known-defects.md).

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
