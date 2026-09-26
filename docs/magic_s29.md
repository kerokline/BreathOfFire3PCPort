# Spell group S29: MAGIC125 and MAGIC126 (DivineBreath, ShadowBreath)

**Status:** IN PROGRESS (2026-09-26). All 49 functions are ours
(`src/game/magic_s29.cpp`, shadow name `magic_s29`). They are fuzzed headless
through the shared harness with 0 mismatches over 98,000 rounds, and
all 142 planted controls are refused (section 6). No recorded route casts these spells, so they stay
fuzz-only until the owner casts them.

Group S29 of round nine, second wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4 and
§6a), taken with the spell harness ([`magic_harness.md`](magic_harness.md))
without editing it. Two `Magic_Rows` overlays, contiguous in the band:

| Unit | Row | File | Ability id | Read one id down | Extent | Functions | Bytes |
|---|--:|---|---|---|---|--:|--:|
| MAGIC125 | 125 | 0x28D | 0x7D | DivineBreath | `0x4E0910..0x4E1996` | 20 | 4,040 |
| MAGIC126 | 127 | 0x28E | 0x7E | ShadowBreath | `0x4E19A0..0x4E3254` | 29 | 6,125 |

The ability names are hypotheses: the shift is TCRF's rule and breaks at
least once ([`cut-content.md`](cut-content.md) §2). The functions are named
`DivineBreath_` / `ShadowBreath_` after them, and after what the code does
(a beam, a burst, motes, an orb, a glow, seekers). Everything in section 1 is a
reading of the code; what the spells look like on screen is the owner's eye.

`tools/magic_rows.py --unit MAGIC125 / MAGIC126 --clones` gives 49 functions
not yet ours. The reading found none missing from the extents and none extra;
no jump tables and no `REFUSED` lines.

**Shared bodies and the reach.** `magic_rows.py` has most of both units
reached from MAGIC124 too, and MAGIC126 reaching MAGIC129 / 130. By reading,
that is the `.data` tables running on without a gap (as S24 found for its
own): MAGIC124's tables at `0x65BB80..0x65BBA3` are followed directly by
MAGIC125's, MAGIC125's by MAGIC126's, and MAGIC126's last table
(`ShadowMote_Steps`, four entries) by MAGIC129's at `0x65BC30`. MAGIC124's
code creates neither kind-1 parameter `0x5C` nor `0x4E` and calls none of
these functions. The real reach from other files is two phases other
overlays list: `DivineBeam_Widen` (`0x4E0BE0`, also MAGIC151's) and
`ShadowSeeker_Fade` (`0x4E29E0`, also MAGIC168 / 169 / 172's), both reached
by address, so taking them takes them for every overlay.

## 1. What each overlay does (by reading)

**MAGIC125 (DivineBreath).** `DivineBreath_Task` is the kind-2 task, a
two-entry stack table by `+1` (`DivineBreath_Start`, `BattleFx_Finish`); after
the phase it runs every live record of its own pool, `DivineMote_Pool`
(`0x69FC30`, 64 task-shaped records of `0x84`), as `Sprite_Current` with the
record's `+0x80` as the owner.

- **`DivineBreath_Start`** empties the pool, centres the task on the target
  side (`MagicFx_CenterOnSide`) and makes two kind-1 children, parameter
  `0x5C` (`DivineBreathFx_Task`, dispatched by `+1` through
  `DivineBreathFx_Kinds`): the **beam** (`+1` 0, delay 1) and the **burst**
  (`+1` 1, delay 9). Sound `0x100`.
- **The beam** (`DivineBeam_Run`, steps `DivineBeam_Steps` by `+2`): after
  its delay it stands at the side's centre `0x4000000` up (`_Wait`), drops
  `0x800000` a frame for 8 frames (`_Descend`), then widens its radius `+0x14`
  by `0x20` a frame for 16 frames and frees itself (`_Widen`). Each frame
  (with new colour bytes on even frames) it draws a disc of sixteen
  semi-transparent POLY_G3 (`DivineBeam_DrawDisc`) and a wall of 32 POLY_G4
  from height `-0x300` down to 0 round the radius (`DivineBeam_DrawWall`),
  under the actor's matrix.
- **The burst** (`DivineBurst_Run`, `DivineBurst_Steps`): after its delay it
  stands at the centre with radius `0x100` (`_Wait`) and closes in, faster
  each frame (`_Shrink`: its step `+0x20` goes 0x21, 0x1F, ... 1). When the
  radius is under `0x30` it sets the targets' flag `0x10`, plays sound
  `0x101` and sheds 64 motes into the pool; then it shrinks to 8 and fades
  on odd frames (`_Fade`). It draws the beam's wall.
- **A mote** (`DivineMote_Task` / `_Run`, `DivineMote_Steps`): after a
  delay of 1..88 frames it starts `0x5000` out from the centre at its own
  angle (`_Launch`), then flies outward and upward, accelerating, until its
  distance is 0 and it frees its record through MAGIC219's `0x4F6290`
  (`_Fly`). Each frame, between two draw-mode links at its point, it draws
  a screen-space star of eight POLY_G3 (`_DrawStar`) and a ring of eight
  POLY_G4 (`_DrawRing`) round its screen point.
- `DivineMote_Alloc` takes the first free record of the pool.

**MAGIC126 (ShadowBreath).** `ShadowBreath_Task` is the kind-2 task, a
two-entry stack table by `+1` (`ShadowBreath_Start`, `BattleFx_Finish`); after
the phase it runs every live record of its pool, `ShadowMote_Pool`
(`0x6A1D30`, 128 records of `0x20`), by making it `ShadowMote_Current`
(`0x6A2D30`) with the record's `+0x1C` as the owner.

- **`ShadowBreath_Start`** empties the pool, centres the task on the target
  side, makes eight kind-1 children, parameter `0x4E`
  (`ShadowBreathFx_Task`, by `+1` through `ShadowBreathFx_Kinds`), all
  **seekers** (`+1` 2) numbered 0..7 with delays 1..8, plays sound `0x102`,
  and makes CLUT strip row 26 semi-transparent (its source with bit 15 set).
- **A seeker** (`ShadowSeeker_Run`, `ShadowSeeker_Steps`): after its delay it
  starts at the acting actor's sprite (`0x904B3C`), `0x8000` ahead by its
  direction (`0x446770`), heading toward the side's centre (`_Launch`,
  `Math_Ratan2`); it steps toward the centre (`MagicFx_StepToward`, speed
  `0x60`) until it is near (`MagicFx_NearSprite`, `0xC000`) or its heading
  swings by between `0x600` and `0xA00` - it has passed it (`_Home`). Then
  seeker 0 alone **bursts** (`_Burst`): sound `0x101`, an **orb** and a
  **glow** (kind-1 `0x4E` children of the task, `+1` 0 and 1), 64 motes into
  the pool, the targets' flag `0x10`. Every seeker then fades (`_Fade`). A
  seeker draws sixteen screen-space POLY_G3 round its screen point, its
  centre colour from `ShadowSeeker_Colours` by its number (`_Draw`).
- **The orb** (`ShadowOrb_Run`, `ShadowOrb_Steps`): steps borrowed from
  MAGIC056 (`0x4ADB50`: to the owner's point) and MAGIC060 (`0x4B1740`: count
  down and free) around its own `_Grow` and `_WaitChildren` (on once the
  task's count is under 2). It draws a rim and a band of sixteen POLY_G4
  each, the rim's outer radius jittered by `Rand & 3`, and a disc of 32
  POLY_G3, under the actor's matrix.
- **The glow** (`ShadowGlow_Run`, `ShadowGlow_Steps`, two of them MAGIC040's
  `0x4A5D20` / `0x4A5D50`): after `0x2E` frames it stands at the centre with
  sound `0x100` (`_Wait`) and grows (`_Grow`); it is two semi-transparent
  POLY_GT4 sprites stacked at its screen point (`_Draw`).
- **A shadow mote** (`ShadowMote_Task` / `_Run`, `ShadowMote_Steps`, on
  the current record): after a delay of `0x18..0x37` frames it starts on a
  circle round the owner at one of sixteen angles (`_Wait`), spreads outward
  (`_Spread`), lifts, accelerating (`_Lift`), and fades out (`_Fade`), then
  frees its record (`ShadowMote_Free`). Each frame it takes its screen point
  (`ShadowMote_Project`, `BattleActor_UpdateScreenXY`'s shape for a pool
  record) and draws a POLY_FT4 sprite there (`ShadowMote_Draw`).
- `ShadowMote_Alloc` takes the first free record of the pool.

The `symbols.toml` evidence gives every function to the instruction.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the
project's precedent ([`magic_fx_reached.md`](magic_fx_reached.md) §3): a phase
past any of the sixteen dispatch tables (three stack tables, eleven `.data`
tables, the two pool tables among them) aborts where the original would call
through whatever follows. Where the original reads `Sprite_Current`, the
owner, the current record or a scratch word again after a call, ours reads
it again; where it holds a pointer across a call (`DivineMote_Fly`'s two
position dwords), ours holds it.

Two things that look like divergence and are not:

- `Gte_RotTransPers3` / `4` / `Gte_RotTransPers` are called with the
  arguments their prototypes read; the originals push one more (the flag),
  which the callees never read (symbols.toml).
- The CRT's `_ftol` in `ShadowMote_Project` is done inline (`Ftol16`, as
  `battle_items.cpp`'s): truncation through a 64-bit integer, 0 in the low
  word for a NaN or out-of-range float.

## 3. Calls to other units (by raw address, not bound)

| Address | Owner | Reached as |
|---|---|---|
| `0x446770` | engine, in no group | `ShadowSeeker_Launch`: the task's `(+0xC, +0x10)` turned by its direction `+8` |
| `0x4F6290` | MAGIC219 (S37) | `DivineMote_Fly`'s tail jump: `Sprite_Current` bytes 0..4 cleared |
| `0x4ADB50` | MAGIC056 (S10) | `ShadowOrb_Steps` entry 0: to the owner's point, `+9` `+0xA` 0, on |
| `0x4B1740` | MAGIC060 (S12) | `ShadowOrb_Steps` entry 3: `+9` down, at 0 the owner's `+0xB` down and free |
| `0x4A5D20` | MAGIC040 (S07) | `ShadowGlow_Steps` entry 1: `+9` `+0xA` up, on at `+0xA` 0x10 |
| `0x4A5D50` | MAGIC040 (S07) | `ShadowGlow_Steps` entry 3: `+9` up, `+0xA` down, at 0 the owner's `+0xB` down and free |

The four table entries are called through the tables, read in place; the two
direct calls go through `MH_AT` (`src/game/magic_s29_callees.h`). The effect
library (`MagicFx_CenterOnSide`, `_StepToward`, `_NearSprite`,
`_PushActorMatrix`, `BattleActor_UpdateScreenXY`), `BattleFx_Finish` and the
GTE / GPU layer are called by name.

## 4. Named data (`symbols.toml` `[[data]]`)

| Kind | Names |
|---|---|
| MAGIC125's handler tables | `DivineBreathFx_Kinds` `0x65BBA4` (2), `DivineBeam_Steps` `0x65BBAC` (3), `DivineBurst_Steps` `0x65BBB8` (3), `DivineMote_TaskTable` `0x65BBC4` (1), `DivineMote_Steps` `0x65BBC8` (2) |
| MAGIC126's handler tables | `ShadowBreathFx_Kinds` `0x65BBD0` (3), `ShadowOrb_Steps` `0x65BBDC` (4), `ShadowGlow_Steps` `0x65BBEC` (4), `ShadowSeeker_Steps` `0x65BBFC` (4), `ShadowMote_TaskTable` `0x65BC1C` (1), `ShadowMote_Steps` `0x65BC20` (4) |
| MAGIC126's constants | `ShadowSeeker_Colours` `0x65BC0C` (eight byte pairs) |
| Pools (`.bss`) | `DivineMote_Pool` `0x69FC30` (64 x `0x84`), `ShadowMote_Pool` `0x6A1D30` (128 x `0x20`), `ShadowMote_Current` `0x6A2D30` |

This document records only their addresses and sizes, not their values.

## 5. The fuzz

`BOF3X_SHADOW=magic_s29` (`magic_s29_fuzz.cpp`) runs `magic_harness::Run`
over the 49 clones, 2,000 rounds each.

**Callees** beyond the standard set (40):

- the GTE / GPU / Math layer as recorders: the projections' vertices logged
  by the bytes they hold (`deref` 8, 6 for the single point), their outputs
  in the fuzz's own buffer (compared), the depth pointer in the caller's
  frame masked off;
- `Gfx_CommitPrim` and `MapView_LinkPrimAt` with an `effect` that logs the
  `0x58` bytes at `Gfx_PacketNext` (a POLY_GT4's): every primitive of a draw
  is built in the same bytes, so the regions at the end hold only the last;
- `Math_Ratan2` with an `effect` that, a third of the time, answers a
  heading whose turn from `+0x10` lands on `0x600` / `0xA00` or one beside
  (`ShadowSeeker_Home`'s bounds);
- `MagicFx_NearSprite` as `Answer::kBool` (its caller tests all of eax);
- the CRT's `_ftol` as `Answer::kThrough` (the copy keeps its call; ours
  truncates inline);
- `0x446770` and `0x4F6290` (the latter `kPhase`: the task it frees is
  logged);
- the group's own functions called directly, as `kPhase` (the MAGIC126 pool's
  with `ShadowMote_Current`);
- the two allocators as `kByte` inside their pools (neither caller tests for
  `0xFF`, section 7).

**Tables.** Eleven `.data` tables in three runs (`0x65BBA4` 11 entries,
`0x65BBD0` 15, `0x65BC1C` 5), swapped for recorders.

**Regions** beyond the standard ones: the scratch `0x903850..0x90385F`,
`Prim_VertexScratch`, `Gfx_PacketNext` and a 256-byte primitive buffer, both
pools and `ShadowMote_Current`, CLUT strip row 26 and its source.

**Seed.** Every round: `Gfx_PacketNext` at one of sixteen places in the
buffer, the acting actor's sprite pointer and `ShadowMote_Current` put inside,
every pool record's owner a task slot or sprite record. Per function: each
dispatcher's index inside its table (`+0` cleared half the time where it
gates the draws); each count-down one step either side of its end
(`+9`, `+0xA`, the records' `+5` / `+6`); `DivineBurst_Shrink`'s step on 1,
2, 3, `0x21`, `0x40`, 0, -1 or -2 (the last three added after control D34)
with the radius landing on `0x30` or beside it;
`DivineBurst_Fade`'s radius 7..9; `DivineMote_Fly`'s distance one step from 0;
the orb's and glow's `+9` beside their ends; `ShadowSeeker_Home`'s `+9`
round `0x10`; `ShadowSeeker_Burst`'s number 0 half the time;
`ShadowSeeker_Fade`'s `+9` at 0..3 and `0x7F..0x82` (the sign flip);
`ShadowMote_Lift`'s rise one below `0x800000` after its step; the allocators'
pools full half the time, one record free in half of those.

**Disturb.** After a call the group's disturbance moves one of: the packet
pointer, `ShadowMote_Current`, a scratch or vertex byte, a byte of the
current record below its owner, a pool record's in-use bit.

Result in this worktree (2026-09-26):

    shadow      magic_s29 self-test: 98000 rounds over 49 functions (2000 each), 2871134 calls to the stand-ins,
                0 MISMATCHES; 25240 bytes of state (17 regions) and the stand-ins' log compared

Every function's callees and phases are reached (the coverage lines).
`BOF3X_SHADOW='*'` exits 0.

## 6. Controls

142 plants, each put into `magic_s29.cpp` one at a time by a script (not
committed) anchored on a string that occurs once in the file: plant, rebuild,
run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s29`, restore, rebuild. **All
142 are refused** (exit 3), each only in the functions the plant touches. The
figures are mismatched rounds out of 2,000, in this worktree.

One was not refused on the first pass: **D34** (`DivineBurst_Shrink`'s
`!= 1` made `> 1`) - the seed only ever gave the step `+0x20` positive values.
The seed now also gives 0, -1 and -2, and D34 is refused in 723 rounds;
D35..D38 (the same function) were run again under the new seed. None is an
equivalent mutant.

| | Planted | Refused in |
|---|---|---|
| D1 | Task: stack table entries swapped | DivineBreath_Task 2,000 |
| D2 | Task: owner not put back | DivineBreath_Task 1,652 |
| D3 | Task: live test & 3 | DivineBreath_Task 2,000 |
| D4 | Start: +2 of a record not cleared | DivineBreath_Start 2,000 |
| D5 | Start: burst delay 8 | DivineBreath_Start 2,000 |
| D6 | Start: sound 0x101 | DivineBreath_Start 2,000 |
| D7 | Start: self read before the create | DivineBreath_Start 117 |
| D8 | Fx_Task: kind ^ 1 | DivineBreathFx_Task 2,000 |
| D9 | Beam_Run: colours on odd frames | DivineBeam_Run 721 |
| D10 | Beam_Run: colour + 5 | DivineBeam_Run 356 |
| D11 | Beam_Run: disc and wall swapped | DivineBeam_Run 721 |
| D12 | Beam_Run: +2 not tested | DivineBeam_Run 348 |
| D13 | Beam_Wait: height + 0x4000001 | DivineBeam_Wait 321 |
| D14 | Beam_Wait: +0xA 9 | DivineBeam_Wait 329 |
| D15 | Beam_Descend: step 0xFF810000 | DivineBeam_Descend 2,000 |
| D16 | Beam_Widen: + 0x21 | DivineBeam_Widen 2,000 |
| D17 | Beam_Widen: owner's count kept | DivineBeam_Widen 323 |
| D18 | BeamColours: green x +0xA | DivineBeam_DrawDisc 1,979, DivineBeam_DrawWall 1,977 |
| D19 | BeamColours: red unsigned | DivineBeam_DrawDisc 993, DivineBeam_DrawWall 881 |
| D20 | DrawDisc: draw mode 0x36 | DivineBeam_DrawDisc 2,000 |
| D21 | DrawDisc: fifteen triangles | DivineBeam_DrawDisc 2,000 |
| D22 | DrawDisc: rim grey 0x11 | DivineBeam_DrawDisc 2,000 |
| D23 | DrawDisc: commit size 0x30 | DivineBeam_DrawDisc 2,000 |
| D24 | DrawDisc: rim z 1 | DivineBeam_DrawDisc 2,000 |
| D25 | DrawWall: top z -0x2FF | DivineBeam_DrawWall 2,000 |
| D26 | DrawWall: link z from x | DivineBeam_DrawWall 2,000 |
| D27 | DrawWall: second link 0x40 | DivineBeam_DrawWall 2,000 |
| D28 | DrawWall: 30 quads | DivineBeam_DrawWall 2,000 |
| D29 | DrawWall: top shade 2 | DivineBeam_DrawWall 2,000 |
| D30 | DrawWall: new x read before the cosine | DivineBeam_DrawWall 28 |
| D31 | Burst_Run: the disc drawn | DivineBurst_Run 720 |
| D32 | Burst_Wait: step 0x20 | DivineBurst_Wait 337 |
| D33 | Burst_Wait: green 0xE | DivineBurst_Wait 337 |
| D34 | Burst_Shrink: step > 1 | DivineBurst_Shrink 723 |
| D35 | Burst_Shrink: threshold 0x31 | DivineBurst_Shrink 448 |
| D36 | Burst_Shrink: flag 0x20 | DivineBurst_Shrink 772 |
| D37 | Burst_Shrink: delay i / 5 | DivineBurst_Shrink 772 |
| D38 | Burst_Shrink: mote's owner the task | DivineBurst_Shrink 772 |
| D39 | Burst_Fade: >= 8 | DivineBurst_Fade 318 |
| D40 | Burst_Fade: even frames | DivineBurst_Fade 2,000 |
| D41 | Mote_Task: the steps table | DivineMote_Task 2,000 |
| D42 | Mote_Run: last draw mode 0x35 | DivineMote_Run 561 |
| D43 | Mote_Run: no screen point | DivineMote_Run 561 |
| D44 | MoteAngle << 5 | DivineMote_Launch 291, DivineMote_Fly 1,999 |
| D45 | Launch: distance 0x5001 | DivineMote_Launch 344 |
| D46 | Launch: x from the owner's z | DivineMote_Launch 344 |
| D47 | Launch: colour + 6 | DivineMote_Launch 344 |
| D48 | Launch: +0xA 0x17 | DivineMote_Launch 344 |
| D49 | Fly: x's record read after the sine | DivineMote_Fly 58 |
| D50 | Fly: rise less its step | DivineMote_Fly 2,000 |
| D51 | Fly: freed only at <= 0 | DivineMote_Fly 817 |
| D52 | Fly: owner's count kept | DivineMote_Fly 306 |
| D53 | DrawStar: centre 0xF1 | DivineMote_DrawStar 2,000 |
| D54 | DrawStar: first x from +0x30 | DivineMote_DrawStar 2,000 |
| D55 | DrawStar: green x 15 | DivineMote_DrawStar 1,994 |
| D56 | DrawStar: link size 0x30 | DivineMote_DrawStar 2,000 |
| D57 | DrawRing: outer radius x 3 | DivineMote_DrawRing 1,991 |
| D58 | DrawRing: +0x28 from the outer x | DivineMote_DrawRing 1,998 |
| D59 | DrawRing: outer shade 0 | DivineMote_DrawRing 2,000 |
| D60 | DrawRing: link size 0x40 | DivineMote_DrawRing 2,000 |
| D61 | Mote_Alloc: none answers 0xFE | DivineMote_Alloc 494 |
| D62 | Mote_Alloc: marks bits 0 and 1 | DivineMote_Alloc 757 |
| S1 | Task: current record not set | ShadowBreath_Task 2,000 |
| S2 | Task: owner not put back | ShadowBreath_Task 1,615 |
| S3 | Start: seeker +1 1 | ShadowBreath_Start 2,000 |
| S4 | Start: seeker delay i + 2 | ShadowBreath_Start 2,000 |
| S5 | Start: CLUT bit 14 | ShadowBreath_Start 2,000 |
| S6 | Start: sound 0x100 | ShadowBreath_Start 2,000 |
| S7 | Start: no screen point | ShadowBreath_Start 2,000 |
| S8 | Fx_Task: kind ^ 1 | ShadowBreathFx_Task 2,000 |
| S9 | Orb_Run: rim and band swapped | ShadowOrb_Run 808 |
| S10 | Orb_Grow: +0xA up 0x17 | ShadowOrb_Grow 2,000 |
| S11 | Orb_WaitChildren: <= 2 | ShadowOrb_WaitChildren 396 |
| S12 | OrbDisc: radius x 3 | ShadowOrb_DrawDisc 1,989 |
| S13 | OrbDisc: blue x 13 | ShadowOrb_DrawDisc 1,987 |
| S14 | OrbDisc: draw mode 0x35 | ShadowOrb_DrawDisc 2,000 |
| S15 | OrbDisc: 31 triangles | ShadowOrb_DrawDisc 2,000 |
| S16 | OrbRing: v2 y from v1 | ShadowOrb_DrawRim 1,994, ShadowOrb_DrawBand 1,997 |
| S17 | OrbRing: commit 0x40 | ShadowOrb_DrawRim 2,000, ShadowOrb_DrawBand 2,000 |
| S18 | DrawRim: Rand & 7 | ShadowOrb_DrawRim 943 |
| S19 | DrawRim: green x 5 | ShadowOrb_DrawRim 1,990 |
| S20 | DrawBand: radius +9 | ShadowOrb_DrawBand 1,994 |
| S21 | DrawBand: shade on the inner edge | ShadowOrb_DrawBand 2,000 |
| S22 | Glow_Run: no screen point | ShadowGlow_Run 775 |
| S23 | Glow_Wait: +9 0x15 | ShadowGlow_Wait 349 |
| S24 | Glow_Wait: x read before the sound | ShadowGlow_Wait 27 |
| S25 | Glow_Grow: at 0x94 | ShadowGlow_Grow 358 |
| S26 | Glow_Draw: half-width 0x29 | ShadowGlow_Draw 1,996 |
| S27 | Glow_Draw: top - 0xF | ShadowGlow_Draw 2,000 |
| S28 | Glow_Draw: tpage x 0x300 | ShadowGlow_Draw 2,000 |
| S29 | Glow_Draw: v + 0x10 | ShadowGlow_Draw 2,000 |
| S30 | Glow_Draw: lower's foot shaded | ShadowGlow_Draw 2,000 |
| S31 | Glow_Draw: clut 0x1FB | ShadowGlow_Draw 2,000 |
| S32 | Seeker_Run: the glow drawn | ShadowSeeker_Run 782 |
| S33 | Seeker_Launch: direction the owner's | ShadowSeeker_Launch 247 |
| S34 | Seeker_Launch: offset 0x8001 | ShadowSeeker_Launch 329 |
| S35 | Seeker_Launch: lift 0x80000 | ShadowSeeker_Launch 331 |
| S36 | Heading: x and z swapped | ShadowSeeker_Launch 283, ShadowSeeker_Home 1,769 |
| S37 | Home: +9 <= 0x10 | ShadowSeeker_Home 323 |
| S38 | Home: speed 0x61 | ShadowSeeker_Home 2,000 |
| S39 | Home: near 0xB000 | ShadowSeeker_Home 2,000 |
| S40 | Home: > 0x5FF | ShadowSeeker_Home 44 |
| S41 | Home: <= 0xA00 | ShadowSeeker_Home 35 |
| S42 | Home: no absolute value | ShadowSeeker_Home 329 |
| S43 | Home: heading & 0x7FF | ShadowSeeker_Home 337 |
| S44 | Burst: glow +9 0x2F | ShadowSeeker_Burst 936 |
| S45 | Burst: delay + 0x19 | ShadowSeeker_Burst 952 |
| S46 | Burst: delay by i & 2 | ShadowSeeker_Burst 952 |
| S47 | Burst: seekers 0 and 1 burst | ShadowSeeker_Burst 4 |
| S48 | Burst: glow +1 2 | ShadowSeeker_Burst 936 |
| S49 | Seeker_Fade: freed at any non-zero | ShadowSeeker_Fade 1,018 |
| S50 | Seeker_Fade: down 1 | ShadowSeeker_Fade 1,993 |
| S51 | Seeker_Draw: blue from the red byte | ShadowSeeker_Draw 1,842 |
| S52 | Seeker_Draw: radius 0x19 | ShadowSeeker_Draw 1,999 |
| S53 | Seeker_Draw: first rim x as a word | ShadowSeeker_Draw 2,000 |
| S54 | Seeker_Draw: commit slot 5 | ShadowSeeker_Draw 2,000 |
| S55 | Seeker_Draw: next angle i << 8 | ShadowSeeker_Draw 2,000 |
| S56 | Mote_Task: the steps table | ShadowMote_Task 2,000 |
| S57 | Mote_Run: draw before project | ShadowMote_Run 791 |
| S58 | Mote_Run: live test on Sprite_Current | ShadowMote_Run 1,054 |
| S59 | Mote_Wait: radius + 9 | ShadowMote_Wait 309 |
| S60 | Mote_Wait: height + 0x700000 | ShadowMote_Wait 309 |
| S61 | Mote_Wait: +7 radius + 1 | ShadowMote_Wait 309 |
| S62 | ShadeCircle: x from the owner's z | ShadowMote_Wait 309, ShadowMote_Spread 2,000, ShadowMote_Lift 2,000, ShadowMote_Fade 2,000 |
| S63 | ShadeAngle << 7 | ShadowMote_Wait 287, ShadowMote_Spread 1,872, ShadowMote_Lift 1,867, ShadowMote_Fade 1,857 |
| S64 | Spread: until 0xF | ShadowMote_Spread 640 |
| S65 | Spread: even frames | ShadowMote_Spread 2,000 |
| S66 | Lift: rise 0x80001 | ShadowMote_Lift 2,000 |
| S67 | Lift: > 0x800000 | ShadowMote_Lift 316 |
| S68 | Fade: +5 down, not +6 | ShadowMote_Fade 2,000 |
| S69 | ShadeMove: height + rise + 1 | ShadowMote_Spread 2,000, ShadowMote_Lift 2,000, ShadowMote_Fade 2,000 |
| S70 | Mote_Draw: height x 3 | ShadowMote_Draw 1,996 |
| S71 | Mote_Draw: shade x 5 | ShadowMote_Draw 1,988 |
| S72 | Mote_Draw: v 0x49 | ShadowMote_Draw 2,000 |
| S73 | Mote_Draw: tpage at +0x2A | ShadowMote_Draw 2,000 |
| S74 | Mote_Draw: link size 0x54 | ShadowMote_Draw 2,000 |
| S75 | Mote_Alloc: the last record never taken | ShadowMote_Alloc 2 |
| S76 | Mote_Free: +3 kept | ShadowMote_Free 1,994 |
| S77 | Project: height >> 1 | ShadowMote_Project 528 |
| S78 | Project: y from the x float | ShadowMote_Project 529 |
| S79 | Project: x - 0x3FFF | ShadowMote_Project 2,000 |
| S80 | Ftol16: rounding, not truncation | ShadowMote_Project 192 |

The thinnest:

- **S75 (2 rounds)**: the allocator's last record, seen only when the pool is
  full but for that one record.
- **S47 (4)**: seekers 0 and 1 bursting, seen only when the seed leaves `+0xB`
  at 1.
- **S24, D30, D49 (27, 28, 58)**: a read moved across a call; seen only when
  the disturbance moves that cell during the call.
- **S40, S41 (44, 35)**: `ShadowSeeker_Home`'s bounds, reached through the
  `Math_Ratan2` effect.


## 7. What nothing reached, and latent defects (Capcom's, kept)

No recorded route casts either spell (queue §5). The live check is the
owner casting them, with a save that has them or DIV-0045's cheat. What to
look for, by reading: MAGIC125 - a beam dropping onto the target side and
widening, a ring closing in, then a spray of star-like motes flying out and
up; MAGIC126 - eight dark seekers leaving the caster and converging on the
target side, then an orb, a glow and 64 motes rising.

Latent defects, described here and not numbered:

- **Unchecked allocations.** `DivineBurst_Shrink` and `ShadowSeeker_Burst`
  take 64 records each and never test the allocator's `0xFF`. A full
  `DivineMote_Pool` would write a record at `0x69FC30 + 0xFF x 0x84`
  (`0x6A7FAC`), and a full `ShadowMote_Pool` one at `0x6A1D30 + 0xFF x 0x20`
  (`0x6A3D10`), both past their pools into whatever `.bss` follows. The
  pools hold 64 and 128 and each spell fills 64 once, so a single cast never
  meets it; two casts overlapping (or MAGIC126's burst made twice) is not
  measured.
- **Unchecked task slots.** `DivineBreath_Start`, `ShadowBreath_Start` and
  `ShadowSeeker_Burst` never test `BattleTask_Create`'s `0xFF` (no slot): the
  writes land at slot 255, past the 48 slots.
- **Every dispatcher's index is unchecked**: the sixteen tables. Ours aborts.
- **`ShadowSeeker_Colours` is indexed by the seeker's `+0xB`**, unbounded;
  the seekers are numbered 0..7, which is inside.
- **Only seeker 0 bursts.** `ShadowSeeker_Burst` does nothing but step on
  for a seeker whose number `+0xB` is not 0, so if seeker 0 were freed early
  (nothing seen does it) no orb, glow, motes or target flag would come. By
  reading, not measured.
- **`ShadowBreath_Task` leaves `ShadowMote_Current` at the last live record**
  after its loop (the original does not put it back); nothing else reads it.

## 8. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 44 lines under a `group S29` comment: 37 new,
plus seven host extents re-listed at the function's own size (`004E0E00`,
`004E1240`, `004E1940`, `004E1FB0`, `004E2BF0`, `004E3140`, `004E31C0`). Five
were listed right already. MAGIC124's `004E08B0 365` (S28's) still runs over
`0x4E0910..` - the consolidation cuts it at the next entry.
