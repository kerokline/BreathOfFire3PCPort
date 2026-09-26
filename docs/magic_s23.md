# Group S23: Cyclone, Typhoon, Quake, Simoon (MAGIC100..103)

**Status:** IN PROGRESS (2026-09-26) - 51 functions ours
(`src/game/magic_s23.cpp`, shadow name `magic_s23`), fuzzed headless
through the shared harness: 102,000 rounds, 0 mismatches; 69 of 70 negative controls refused by a count, the other an equivalent mutant. No
recorded route casts any of these spells: fuzz only until the owner's eye.

Group S23 of round nine's first spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) Â§4).
Four overlays of `Magic_Rows`, their names the sibling's read one id down
([`cut-content.md`](cut-content.md) Â§2) - **hypotheses**, not confirmed by
anything here: what the code draws fits a whirlwind, a spiral wind, an
earthquake and a sandstorm, but that is my reading of the draws, not a
measurement of the spells.

| Overlay | Row | Ability ids | Name (one id down) | Extent |
|---|--:|---|---|---|
| MAGIC100 | 13 | `0x64`, `0xCA` | Cyclone | `0x4CC970..0x4CD6A1` (13 functions) |
| MAGIC101 | 26 | `0x65`, `0xCB` | Typhoon | `0x4CD6B0..0x4CE0F1` (14) |
| MAGIC102 | 69 | `0x66`, `0xCC` | Quake | `0x4CE100..0x4CEB3B` (5) |
| MAGIC103 | 57 | `0x67`, `0xCD` | Simoon | `0x4CF5F0..0x4D04A2` (19) |

Between MAGIC102 and MAGIC103 lie the five `MapCell_Handlers` functions
(`0x4CEB40..0x4CF5E7`): field code, not this group's (queue Â§1). The
extents are `tools/magic_rows.py --unit MAGIC100..103 --clones`; none of
the 51 is REFUSED, one has a jump table (`Quake_Start`, two entries), and
no function turned up inside or missing from them.

**Shared bodies.** The funnel (kind 1, parameter `0x0A`, `0x4CCAC0..`) is
reached from MAGIC096..100 and the spiral (kind 1, `0x1B`, `0x4CDA20..`)
from MAGIC096..101 (`analysis/magic_funcs.tsv`, column `reached_by`):
taking them takes them for S22's overlays too. S22 reaches them only
through `BattleTask_Create`'s kind tables, so nothing on its side changes.

## 1. The functions

Every one is a battle task's step, `void (void)`, `__cdecl`
([`magic_harness.md`](magic_harness.md) Â§1). `+n` is a byte of the task
slot (`Sprite_Current`); the owner is `0x93B940`, the source `0x904B4C`.
`symbols.toml` has each one's evidence; the `[[data]]` tables are named
there too (read in place, never copied).

**MAGIC100 (row 13).**

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Cyclone_Task` | `0x4CC970` | 0x2E | phase `+1` through a 3-entry stack table: Start, Wait, `BattleFx_Finish` |
| `Cyclone_Start` | `0x4CC9A0` | 0xC6 | owner's `+8` / x / z and the source's `+0x3C`; eight funnels (kind 1, `0x0A`), delays 1, 3 .. 15; sound `0x100` |
| `Cyclone_Wait` | `0x4CCA70` | 0x26 | once a funnel has bumped `+9`, the target's flags `0x10`, on |
| `FxFunnel_Dispatch` | `0x4CCAA0` | 0x12 | `jmp [FxFunnel_Types + 4 * +1]` |
| `FxFunnel_Task` | `0x4CCAC0` | 0x42 | phase `+2` through `FxFunnel_Phases`; alive and past phase 0, the funnel and its ground ring under two matrices |
| `FxFunnel_Wait` | `0x4CCB10` | 0x17C | the delay; then the owner's position, a heading, a drift and rise from `Rand` |
| `FxFunnel_Approach` | `0x4CCC90` | 0x9C | spin; orbit the target's record (`0x4FBB40`); `+0xA` up 4 to 0x10 |
| `FxFunnel_Orbit` | `0x4CCD30` | 0x104 | spin and orbit; near the target (`0x4FBC30`), or the angle moved by 0x401..0xBFF: the owner's `+9` up, on |
| `FxFunnel_Rise` | `0x4CCE40` | 0xAA | drift by the facing (`0x446770`), height up 0x40, spin; `+0xA` down to 0: the owner's count down, free |
| `FxFunnel_PushMatrix` | `0x4CCEF0` | 0xA4 | the funnel's own matrix at its position, 0x100 under half its height |
| `FxFunnel_Draw` | `0x4CCFA0` | 0x328 | a draw mode and 31 semi-transparent G4 quads round the circle, linked by `MapView_LinkPrimAt` |
| `FxFunnel_PushGroundMatrix` | `0x4CD2D0` | 0xBC | as above, from phase 3 at the ground's height (`AreaMap_Elevation`) |
| `FxFunnel_DrawGround` | `0x4CD390` | 0x312 | a wavy ring of 31 G4 quads committed to slot 5 |

**MAGIC101 (row 26).**

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Typhoon_Task` | `0x4CD6B0` | 0x5E | phase `+1` through a 5-entry stack table (Start, Grow, `0x4DA3B0`, Fade, `BattleFx_Finish`); then while `+0xA` and `+0`, the fan under the actor's matrix |
| `Typhoon_Start` | `0x4CD710` | 0xCA | the side's facing; the side's centre (`0x4FC0E0`); sixteen spirals (kind 1, `0x1B`) in four delays; sound `0x100` |
| `Typhoon_Grow` | `0x4CD7E0` | 0x32 | `+0xA` up; at 0x10 the target's flags `0x10`, on |
| `Typhoon_Fade` | `0x4CD820` | 0x1D | `+0xA` down; at 0 on |
| `Typhoon_DrawFan` | `0x4CD840` | 0x1BB | eight G3 triangles round a pulsing radius, committed to slot 5 |
| `FxSpiral_Dispatch` | `0x4CDA00` | 0x12 | `jmp [FxSpiral_Types + 4 * +1]` |
| `FxSpiral_Task` | `0x4CDA20` | 0x33 | phase `+2` through `FxSpiral_Phases`; alive and past phase 0, the band under its own matrix |
| `FxSpiral_Wait` | `0x4CDA60` | 0xC9 | the delay; then 0x180 above the owner, a tilt from `FxSpiral_Turns[+4]`, radius 0x60 |
| `FxSpiral_Grow` | `0x4CDB30` | 0x5E | bob; `+0xA` up 2 to 0x10 |
| `FxSpiral_Hold` | `0x4CDB90` | 0xB8 | bob; `+9` down; at 0 sound `0x101` (spiral 0 only), a tilt from `FxSpiral_Tilts[+8]`, radius 0x80 |
| `FxSpiral_Spread` | `0x4CDC50` | 0x3B | radius up by the spread for 16 frames |
| `FxSpiral_Fade` | `0x4CDC90` | 0x57 | spread slowing; `+0xA` down to 0: the owner's count down, free |
| `FxSpiral_PushMatrix` | `0x4CDCF0` | 0xC5 | the tilt about y or x by the owner's facing bit 0 |
| `FxSpiral_Draw` | `0x4CDDC0` | 0x332 | 31 G4 quads, brightening over the first seven |

**MAGIC102 (row 69).**

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Quake_Task` | `0x4CE100` | 0x36 | phase `+1` through a 4-entry stack table |
| `Quake_Start` | `0x4CE140` | 0x351 | the facing; the heaved block's corner (the view's cell plus `Quake_FacingOffsets`); every actor's and object's height above the ground kept; the map's polygon codes made semi-transparent; the hover flag by the event battle; sound `0x100` |
| `Quake_Rumble` | `0x4CE4A0` | 0x57 | 16 frames of camera shake growing with `+9` |
| `Quake_Heave` | `0x4CE500` | 0x516 | a sine wave through a 16 x 14 block of `AreaMap_Corners`; everything put back on the moving ground; shake; on when the wave has settled and `+9` passed 0x5A |
| `Quake_End` | `0x4CEA20` | 0x11C | the polygon codes put back, the shake 0, the target's flag `0x40`, done flag, free |

**MAGIC103 (row 57).**

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Simoon_Task` | `0x4CF5F0` | 0x2E | phase `+1` through a 3-entry stack table (Start, Wait, `0x43FE80`) |
| `Simoon_Start` | `0x4CF620` | 0x114 | four effects (kind 1, `0x2D`): a fan, two dusts, a dome; two CLUT rows made semi-transparent; sound `0x100` |
| `Simoon_Wait` | `0x4CF740` | 0x24 | the four gone: the target's flag `0x40`, on |
| `SimoonFx_Dispatch` | `0x4CF770` | 0x12 | `jmp [SimoonFx_Types + 4 * +1]` |
| `SimoonDome_Task` / `_Start` / `_Grow` / `_Expand` / `_Draw` | `0x4CF790` `0x4CF7D0` `0x4CF820` `0x4CF870` `0x4CF8C0` | 0x33 0x48 0x4F 0x46 0x453 | a dome of 8 x 32 textured GT4s growing then expanding at the source; the target's flags `0x10` at step 4 |
| `SimoonDust_Task` / `_Wait` / `_Rise` / `_Arc` / `_Fall` / `_Draw` | `0x4CFD20` `0x4CFD50` `0x4CFDE0` `0x4CFE10` `0x4CFE50` `0x4CFEB0` | 0x29 0x8E 0x2F 0x3A 0x58 0x34B | a spinning screen-space GT4 at an offset from the source, rising and arcing |
| `SimoonFan_Task` / `_Start` / `_Grow` / `_Draw` | `0x4D0200` `0x4D0240` `0x4D0290` `0x4D02C0` | 0x33 0x46 0x2A 0x1E3 | sixteen G3s round the source; phases 2 and 3 are MAGIC056's |

## 2. Calls into other units (raw addresses, never bound)

`magic_s23_callees.h`: `0x446770` (a record's `+0xC` / `+0x10` turned by
its `+8`: engine, in no queue group), `0x4FBB40` (orbit a record),
`0x4FBC30` (within a box of a record: a C bool tested whole), `0x4FC0E0`
(a side's centre) - all three group L's library. As phases, by the address
their tables hold: `0x4DA3B0` (MAGIC118's, Typhoon's third phase),
`0x43FE80` (group E's engine row 128 end, Simoon's third), `0x4AE0D0` /
`0x4AE0F0` (MAGIC056's, `SimoonFan_Phases[2..3]`). Everything else it
calls is ours already (the GTE / GPU library, `Math_*`, `MapView_LinkPrimAt`,
`Gfx_CommitPrim`, `AreaMap_Elevation`, `Battle_ActorIsOut`, the battle
task calls) and is called by name.

No divergence and no ledger entry: every function is a faithful
replacement, except that (a) a phase past one of the four stack tables
aborts (the precedent, [`magic_fx_reached.md`](magic_fx_reached.md) Â§3),
and (b) `Quake_Heave` aborts with a `Fatal` where the original takes an
integer divide fault (section 5) - both are crashes of the original made
loud, not behaviour a game reaches.

## 3. The fuzz

`BOF3X_SHADOW=magic_s23` (`magic_s23_fuzz.cpp`): the clone table, and
beyond the standard set:

- **`Answer::kThrough`** for the GTE / GPU library and `Math_Sin` / `_Cos`
  / `_Ratan2` (21 callees): both sides call them for real. The draws hand
  them stack vectors and matrices; recorders would log pointers that differ
  between passes and write nothing, so a wrong vector would go unseen and
  uninitialised stack would be read. With them real, what they compute
  lands in the compared state (the GTE block `0x7DE428..0x7DE7A7`, the
  primitive buffer, the scratch words). S23 introduced this answer kind and
  `kBool`; HX folded both into the harness unchanged
  ([`magic_harness.md`](magic_harness.md) Â§7).
- **Recorders** for `MapView_LinkPrimAt`, `Gfx_CommitPrim`,
  `AreaMap_Elevation`, `Battle_ActorIsOut`, the four raw callees
  (`0x4FBC30` as `kBool`, `0x4FBB40` with an effect that half the time
  answers an angle 0x3FF / 0x400 / 0x401 / 0xBFF / 0xC00 / 0xC01 from the
  last, so `FxFunnel_Orbit`'s two bounds are met), and the group's own ten
  draws and matrices, which the tasks call directly.
- **The eight `.data` tables** of handlers swapped for recorders.
- **Regions** (23 with the standard): `Camera_ShiftY`, the scratch
  `0x903850..0x90385F`, `Prim_VertexScratch`, the GTE block,
  `Camera_Matrix` to `MapView_Redraw` (`0x905E40..0x905E6F`),
  `Gfx_PacketNext` and a 1 KB primitive buffer of the fuzz's own, Quake's
  .bss `0x695990..0x695C33`, `Sprite_Objects`, `Sprite_ObjectsExtra`,
  `Effect_Objects`, `MapView_Cells`, the loaded area's first 0x800 bytes,
  and the two CLUT rows and their source. 27,508 bytes compared a round.
- **The seed**, every round: `Gfx_PacketNext` into the buffer, the actor
  sprite `0x904B3C` at a record, `Gte_MatrixDepth` 0..0x15, the facing
  0..3 (a facing of 4 divides by 0, section 5), the block's corner at small
  and extreme values, the target with the `0x40` / `0x80` side bits half
  the time, the event battle at and beside the four hover values; the map:
  `MapView_Cells` all zero but up to a dozen cells naming four cell runs of
  1..8 dwords whose steps land exactly on their ends (a run that does not
  runs away, in the original too), their codes the ones the walks switch;
  the area 0..16 wide and high (so the heave stays in the region). Per
  function: each table index across its entries, and each counter at and
  around its compare.
- **Disturb** moves `Gfx_PacketNext`, a scratch word, a vertex word, the
  hover flag and Quake's kept heights - the cells the loops read again
  after a recorder.

Result (2026-09-26, in this worktree):

    shadow      magic_s23 self-test: 102000 rounds over 51 functions (2000 each), 1589141 calls to the stand-ins,
                0 MISMATCHES; 27508 bytes of state (23 regions) and the stand-ins' log compared

Coverage (calls the originals made, in this worktree): MapView_LinkPrimAt 1156000, Gfx_CommitPrim 120000, AreaMap_Elevation 155778, Battle_ActorIsOut 28152, 0x446770 2503, 0x4FBB40 4000, 0x4FBC30 2000, 0x4FC0E0 2000, FxFunnel_PushMatrix 796, FxFunnel_Draw 796, FxFunnel_PushGroundMatrix 796, FxFunnel_DrawGround 796, Typhoon_DrawFan 566, FxSpiral_PushMatrix 806, FxSpiral_Draw 806, SimoonDome_Draw 702, SimoonDust_Draw 721, SimoonFan_Draw 778, BattleTask_Create 56000, BattleTask_FreeCurrent 4129, Battle_SetTargetFlags 1996, Battle_SetTargetFlag40 3002, Sound_PlayById 8522, BattleActor_UpdateScreenXY 6000, MagicFx_PushActorMatrix 2046, Gte_PopMatrix 4444, Rand 1006, phase 0x4CC9A0 667, phase 0x4CCA70 681, phase 0x4F7350 1031, phase 0x4CD710 406, phase 0x4CD7E0 421, phase 0x4DA3B0 412, phase 0x4CD820 382, phase 0x4CE140 499, phase 0x4CE4A0 474, phase 0x4CE500 554, phase 0x4CEA20 473, phase 0x4CF620 673, phase 0x4CF740 650, phase 0x43FE80 677, phase 0x4CCAC0 2000, phase 0x4CCB10 499, phase 0x4CCC90 510, phase 0x4CCD30 492, phase 0x4CCE40 499, phase 0x4CDA20 2000, phase 0x4CDA60 399, phase 0x4CDB30 404, phase 0x4CDB90 406, phase 0x4CDC50 378, phase 0x4CDC90 413, phase 0x4CF790 665, phase 0x4CFD20 668, phase 0x4D0200 667, phase 0x4CF7D0 662, phase 0x4CF820 674, phase 0x4CF870 664, phase 0x4CFD50 495, phase 0x4CFDE0 514, phase 0x4CFE10 466, phase 0x4CFE50 525, phase 0x4D0240 502, phase 0x4D0290 496, phase 0x4AE0D0 487, phase 0x4AE0F0 515

`BOF3X_SHADOW='*'`: exit 0.

The first runs crashed twice on the fuzz's own blindness, both fixed before
the result: the harness's disturbance wrote a random byte into "the target
enemy's record" for a party target, whose record lies over the owner
pointer `0x93B940` (the next disturbance wrote through it, in `Quake_Heave`
round ~7,273) - now only for targets 3..10, as HX folded it; and the
seed's side bits made that "enemy" lie past the image.

## 4. The controls

70 negative controls, planted one at a time by a script (not committed:
apply to `magic_s23.cpp`, build, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=magic_s23`, restore) against the final fuzz, 2026-09-26.
**69 are refused by a count (exit 3)**, each in the function it
touches (or in every function sharing the helper it touches: F5 `Spin`,
S3 `Bob`, M12 `Arc`, T2 `Facing`, which Quake_Start shares); none by a
fault. One is not refused, and cannot be:

- **F4** swaps `Math_Ratan2`'s arguments in `FxFunnel_Wait`. Both are
  always 0 there (section 5: the offset is overwritten before the heading
  is taken, and no call between can move either side), and `atan2(0, 0)`
  does not care. An equivalent mutant; ours keeps the original's order.

Before the port's final seeds, three more controls were unrefused and were
fixed by the fuzz, not the plant: F7 (`FxFunnel_Orbit`'s lower bound: the
answer of `0x4FBB40` now lands on either bound half the time, and F16 was
added for the upper), Q12 (the hover
compare: `AreaMap_Elevation` now answers the seeded heights' neighbourhood
a third of the time); and the first S8 (a threshold of 0x240 instead of
0x200) was an equivalent mutant (+0xA x 8 = +0xA << 3 at the boundary),
replaced by 0x1C0.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| C1 | Cyclone_Task: entries 0 and 1 swapped | Cyclone_Task 1,348 |
| C2 | Cyclone_Start: +0x3C from the owner, not the source | Cyclone_Start 1,498 |
| C3 | Cyclone_Start: child delay 2i + 2 | Cyclone_Start 2,000 |
| C4 | Cyclone_Start: seven funnels | Cyclone_Start 2,000 |
| C5 | Cyclone_Wait: flags 0x20 | Cyclone_Wait 975 |
| F1 | FxFunnel_Task: draws when +2 is 0 | FxFunnel_Task 245 |
| F2 | FxFunnel_Wait: heading less 0x7FF | FxFunnel_Wait 503 |
| F3 | FxFunnel_Wait: rise Rand & 0x70 | FxFunnel_Wait 255 |
| F4 | FxFunnel_Wait: Ratan2 arguments swapped | **not refused** (equivalent: see below) |
| F5 | Spin: 0x100 | FxFunnel_Approach 996, FxFunnel_Orbit 1,014, FxFunnel_Rise 1,018 |
| F6 | FxFunnel_Approach: 0x11 bound | FxFunnel_Approach 965 |
| F7 | FxFunnel_Orbit: bound 0x3FF | FxFunnel_Orbit 100 |
| F16 | FxFunnel_Orbit: upper bound 0xBFF | FxFunnel_Orbit 32 |
| F8 | FxFunnel_Orbit: near test inverted | FxFunnel_Orbit 2,000 |
| F9 | FxFunnel_Rise: height += 0x20 | FxFunnel_Rise 2,000 |
| F10 | FxFunnel_PushMatrix: height + 0x80 | FxFunnel_PushMatrix 2,000 |
| F11 | FxFunnel_Draw: colour +0xA * 11 | FxFunnel_Draw 1,944 |
| F12 | FxFunnel_Draw: link size 0x48 | FxFunnel_Draw 2,000 |
| F13 | FxFunnel_PushGroundMatrix: elevation from phase 2 | FxFunnel_PushGroundMatrix 346 |
| F14 | FxFunnel_DrawGround: inner - 0x10 | FxFunnel_DrawGround 1,961 |
| F15 | FxFunnel_DrawGround: outer colour 2 | FxFunnel_DrawGround 2,000 |
| T1 | Typhoon_Task: draws when +0xA is 0 | Typhoon_Task 481 |
| T2 | Typhoon_Start: facing flip inverted | Typhoon_Start 1,843, Quake_Start 2,000 |
| T3 | Typhoon_Start: delay (i/4)*12 + 2 | Typhoon_Start 2,000 |
| T4 | Typhoon_Grow: bound 0xF | Typhoon_Grow 1,318 |
| T5 | Typhoon_DrawFan: radius + 0x81 | Typhoon_DrawFan 1,992 |
| S1 | FxSpiral_Wait: height + 0x1000000 | FxSpiral_Wait 684 |
| S2 | FxSpiral_Wait: tilt << 7 | FxSpiral_Wait 626 |
| S3 | Bob: + 0x100 | FxSpiral_Grow 2,000, FxSpiral_Hold 2,000 |
| S4 | FxSpiral_Hold: sound for every spiral | FxSpiral_Hold 487 |
| S5 | FxSpiral_Hold: Sprite_Current not re-read after the sound | FxSpiral_Hold 12 |
| S6 | FxSpiral_Fade: spread - 3 | FxSpiral_Fade 2,000 |
| S7 | FxSpiral_PushMatrix: always about y | FxSpiral_PushMatrix 998 |
| S8 | FxSpiral_Draw: colour threshold 0x1C0 | FxSpiral_Draw 9 |
| Q1 | Quake_Task: entries 2 and 3 swapped | Quake_Task 1,027 |
| Q2 | Quake_Start: offsets row swapped | Quake_Start 1,531 |
| Q3 | Quake_Start: out enemies measured too | Quake_Start 2,000 |
| Q4 | Quake_Start: effect kind 0x18 | Quake_Start 301 |
| Q5 | Semi: 0x22 -> 0x2B | Quake_Start 984 |
| Q6 | Quake_Start: hover for 0x24 not 0x23 | Quake_Start 285 |
| Q7 | Quake_Start: 254 bytes zeroed | Quake_Start 1,994 |
| Q8 | Quake_Rumble: bound 0xF | Quake_Rumble 505 |
| Q9 | Quake_Heave: lift at i 0 kept | Quake_Heave 47 |
| Q10 | Quake_Heave: corners 1 / 2 not swapped across | Quake_Heave 123 |
| Q11 | Quake_Heave: row bound > not >= | Quake_Heave 316 |
| Q12 | Quake_Heave: hover compare >= | Quake_Heave 269 |
| Q13 | Quake_Heave: end bound 0x5B | Quake_Heave 70 |
| Q14 | Quake_Heave: shake -7 | Quake_Heave 2,000 |
| Q15 | Opaque: 0x28 kept | Quake_End 981 |
| Q16 | Quake_End: flags |= 8 | Quake_End 1,449 |
| M1 | Simoon_Start: fan +1 = 3 | Simoon_Start 1,875 |
| M2 | Simoon_Start: CLUT without bit 15 | Simoon_Start 2,000 |
| M3 | Simoon_Start: second row's cell 0 kept | Simoon_Start 2,000 |
| M4 | Simoon_Wait: flag40 while +0xB is set | Simoon_Wait 2,000 |
| M5 | SimoonDome_Grow: flags at +9 = 5 | SimoonDome_Grow 661 |
| M6 | SimoonDome_Grow: size * 27 | SimoonDome_Grow 2,000 |
| M7 | SimoonDome_Expand: bound 0x27 | SimoonDome_Expand 983 |
| M8 | SimoonDome_Draw: colour bound 0x12 | SimoonDome_Draw 27 |
| M9 | SimoonDome_Draw: clut y 0x1FB | SimoonDome_Draw 2,000 |
| M10 | SimoonDome_Draw: v band + 0x15 for the low row | SimoonDome_Draw 2,000 |
| M11 | SimoonDust_Wait: offsets row stride 8 | SimoonDust_Wait 327 |
| M12 | Arc: / 4 | SimoonDust_Arc 2,000, SimoonDust_Fall 1,993 |
| M13 | SimoonDust_Fall: +9 cap 0x2D | SimoonDust_Fall 981 |
| M14 | SimoonDust_Draw: size threshold >= 1 | SimoonDust_Draw 347 |
| M15 | SimoonDust_Draw: turn +0x41 | SimoonDust_Draw 1,846 |
| M16 | SimoonFan_Draw: radius + 0x57 | SimoonFan_Draw 1,310 |
| M17 | SimoonFan_Draw: centre colour order | SimoonFan_Draw 1,983 |
| M18 | SimoonFan_Grow: +0xA += 3 | SimoonFan_Grow 2,000 |
| D1 | SimoonFx_Dispatch: index ^ 1 | SimoonFx_Dispatch 2,000 |
| D2 | SimoonDust_Task: draws when +2 is 0 | SimoonDust_Task 264 |

The thinnest are the ones only a seeded value or a recorder moving a cell
across exactly one call can show: S8 (9 rounds: the spiral's colour at the
one quad whose angle is 0x1C0), S5 (`Sprite_Current` not read again after
spiral 0's sound), M8 (a dome colour bound), Q9 and Q13 (the heave's first
column and its end bound).

## 5. Latent defects (Capcom's, kept; described, not numbered)

- **Quake_Heave divides by zero for a facing of 4 or more.** The divisor
  row is `(facing >> 1) * 15` into `Quake_Divisors` (two rows of 15); a
  facing of 4 or 5 reads 0x65B7D6, a 0 byte of padding, and `idiv` faults
  when a cell of the block lies inside the area. The facing is the actor
  sprite's `+8` (`0x904B3C`), turned by 2; whether a sprite's `+8` is ever
  4 or more in a battle was not measured. Ours stops with a `Fatal` naming
  the facing where the original faults. `Quake_FacingOffsets` is read past
  its four pairs the same way (a quiet read there).
- **Quake_End loses a map code 0x28.** Start turns code 0 into 0x28 and
  End turns 0x28 back into 0, so a cell prim that was 0x28 before the
  spell comes back as 0.
- **FxFunnel_Wait's turned offset is dead.** It sets `(0x60000, 0)`, turns
  it by the facing (`0x446770`), adds it to the owner's position - then
  copies the owner's position over it. The heading from the owner is then
  `Math_Ratan2(0, 0)` every time (no call between the copy and the
  subtraction can move either side), so it is a constant. Control F4 is
  unrefusable for that reason.
- **Unbounded reads:** `FxSpiral_Turns[+4]` (16 words; `+4` is the
  child's index 0..15, so safe as Typhoon_Start makes them),
  `FxSpiral_Tilts[2 * +8]`, `SimoonDust_Offsets[+0xB]` (two rows; the dusts
  get 0 and 1).
- **`BattleTask_Create`'s 0xFF is not checked** by the four starts: with
  every slot taken, the child's fields are written 0x84 x 255 bytes past
  the task table (the same as every overlay the project has read).
- **The stack tables are unbounded** (ours aborts), as in every overlay.
- **A cell run that does not land on its end runs away** in Quake's walks:
  data, not code, decides it; the loaded areas presumably never do.

## 6. What nothing reached

No recorded route casts these spells (the combat route's first-call trace
enters none of the 51, [`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)
Â§5): every function is fuzz only. The live check is the owner's, casting
each with DIV-0045's cheat or a save that has them. What to look for:
Cyclone's eight funnels closing on the target and rising, Typhoon's fan and
sixteen spirals, Quake's rumble, the map heaving and settling with every
actor on it (and, in the four hover battles, a floating enemy staying up),
the map back to normal after; Simoon's dome, two dust sprites and fan. The
fuzz cannot see the pictures: the GTE / GPU library it calls for real is
the same code on both sides.

## 7. For `analysis/calltrace/entries_logic.txt`

47 lines appended to the main checkout's copy under a `group S23` comment
(51 functions; `004CCEF0 A4`, `004CCFA0 328`, `004CD2D0 BC` and
`004CDCF0 C5` were listed right already). The host lines `004CD390 4AD`,
`004CD840 4A7`, `004CDDC0 16E4`, `004CF8C0 9FA` and `004D02C0 5C7` ran
over the functions after them; the new lines give each its own extent.
