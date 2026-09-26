# Spell group S19: MAGIC083 (Shield, row 94) and MAGIC086 (row 28)

**Status:** IN PROGRESS (2026-09-26) - 43 functions ours
(`src/game/magic_s19.cpp`, shadow name `magic_s19`), fuzzed headless
through the shared harness: 86,000 rounds, 0 mismatches; `BOF3X_SHADOW='*'`
exit 0. 108 negative controls: 102 refused by a count (exit 3), each in the function it touches; 6 not refused, all re-reads that nothing in the game can tell apart (section 6). No recorded route casts either spell: fuzz only
until the owner's eye.

Group S19 of round nine's first spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4),
taken with the shared harness ([`magic_harness.md`](magic_harness.md)).
No divergence and no ledger entry: each function is a faithful replacement,
except where ours aborts. It aborts on a phase past one of the two stack tables,
a face index past the crystal's four-entry depth array, or a wall segment of
the ring linked before any point was computed. In each of those three places
the original would read or write its own stack. This follows the project's
precedent ([`magic_fx_reached.md`](magic_fx_reached.md) §3).

## 1. Which abilities

| Row | File | Extent | Loaded by ids | Name read one id down |
|--:|---|---|---|---|
| 94 | MAGIC083 | `0x4C1470..0x4C27E6`, 19 functions | 0x53, 0xB9 | Shield |
| 28 | MAGIC086 | `0x4C27F0..0x4C3487`, 24 functions | 0x56, 0xBC | (no English label) |

The queue lists this group's rows as "28, 94" and its names as "Shield,
(no label)". The rows and the names are in opposite orders: row **94** is
Shield and row **28** is the unlabelled one. The two code pointers are
`.data 0x64C5AC` (row 94) and `0x64C39C` (row 28).

**Row 94 is Shield, and the code confirms it.** Its `Shield_Kind` (`0x4C1710`)
rewrites an action id of 0x136 at `0x904B80` to **0x53** when the actor-kind
byte `0x904B35` is not 4. 0x53 is the id the loader uses. MAGIC082's twin of
that function (`0x4C04F0`, group S18's) maps three other action ids to
0x52, 0x54 and 0x55, with colour kinds 0, 1 and 2. MAGIC083 is a copy of
that effect with the kind fixed at 0: the same function sizes, the same
four-kind colour-table layout. The ids 0x52..0x55 are one family of
effects. Read one id down, they are Protect, Shield, Speed and Might in the
sibling's `names/magic.toml`.

**Row 28 is probably Barrier (likely, not measured).** Ids 0x56 and 0xBC
read one id down are the sibling's id 85 (and 187). `names/magic.toml` has
only its Japanese name (パリア). `names/abilities.toml` gives it the `us`
name **Barrier**. That 0x56 sits just after the 0x52..0x55 family fits a
defensive spell, but that is a guess. The code shows a buff-like visual: a
disc and a ring on the source sprite, whose tint is raised and lowered,
then the target flagged. It sets no status itself. How sure: the shift has
held across the whole 0x52..0x56 neighbourhood as far as the code can check
(one family, ids adjacent). Nothing here measures id 0x56 itself, and the
shift is known to break once elsewhere (row 148).

## 2. MAGIC083 (row 94): Shield

An effect task (kind 2) that puts an **aura** (kind 1, parameter 0x22) on
every member of the target side that is not out. Each aura raises three
**crystals**, which run from a pool of their own: 24 slots of a task's
shape at `0x68FA78`, which the effect task steps itself every frame.

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Shield_Task` | `0x4C1470` | 0x75 | row 94: phase `+1` through a two-entry stack table (`Shield_Start`, `0x4E5200`); then every live pool slot run with `Sprite_Current` and the owner swapped in and put back |
| `Shield_Start` | `0x4C14F0` | 0x218 | clears the pool; kind; one aura per side member not out (delays 1, 0x11, ...), counted in `+0xB`; two CLUT rows copied, strip dirty; sound 0x100 |
| `Shield_Kind` | `0x4C1710` | 0x20 | 0; rewrites action id 0x136 to 0x53 (above) |
| `ShieldAura_Dispatch` | `0x4C1730` | 0x12 | kind 1, 0x22: `jmp [ShieldAura_Types + 4 * +1]` |
| `ShieldAura_Task` | `0x4C1750` | 0x38 | phase `+2` through `ShieldAura_Phases`; while live and past phase 0, the halo and the disc under the actor's matrix |
| `ShieldAura_Wait` | `0x4C1790` | 0x86 | delay; then the member's tint set (0, 0, 0, 1), its record in `+0xA` |
| `ShieldAura_Rise` | `0x4C1820` | 0x111 | tint up to 8; at `+9` 0x10 three crystals (angles `+0xB` + 0, 0x10, 0x20; delays 1, 9, 17) |
| `ShieldAura_Fade` | `0x4C1940` | 0xDF | tint down; at 0 released, the member flashed, `0x4FB790(kind, index)` |
| `ShieldAura_DrawDisc` | `0x4C1A20` | 0x1F1 | 16 `POLY_G3`s, radius 128, rim `ShieldAura_DiscColours[kind] * +9` |
| `ShieldAura_DrawHalo` | `0x4C1C20` | 0x247 | 16 `POLY_G4`s, the annulus 128..256, inner edge `ShieldAura_HaloColours[kind] * +9` |
| `ShieldSpark_Dispatch` | `0x4C1E70` | 0x12 | `jmp [ShieldSpark_Types + 4 * +1]` |
| `ShieldSpark_Task` | `0x4C1E90` | 0x12 | `jmp [ShieldSpark_Phases + 4 * +2]` |
| `ShieldSpark_Place` | `0x4C1EB0` | 0xE8 | delay; then placed on the aura's circle (radius 176 >> 3), lift 0x40 by -4 |
| `ShieldSpark_Rise` | `0x4C1FA0` | 0x95 | screen point, MAGIC082's disc (radius 0x18..0x1B), the crystal; spin every other frame; lift |
| `ShieldSpark_Fall` | `0x4C2040` | 0x82 | as the rise; then waits 0x1C - `+0xB` frames (in turn) |
| `ShieldSpark_Spin` | `0x4C20D0` | 0xE1 | the spin `+0xC` up to 4 every 8 frames; at the end sound 0x101 / 0x102 unless `+3 & 0xF0` |
| `ShieldSpark_Orbit` | `0x4C21C0` | 0x104 | an outward spiral (radius `+0xA * +0x10 + 0xB0`); after 16 frames the aura's count down and the slot freed (`0x4F6290`) |
| `ShieldSpark_DrawCrystal` | `0x4C22D0` | 0x4B6 | two four-sided pyramids (apex ±0x50, radius 0x30) turned by `+9`, eight `POLY_G3`s; each four depth-sorted by `0x4FB880` |
| `ShieldSpark_Alloc` | `0x4C2790` | 0x57 | first free pool slot (bit 0 of byte 0), marked; 0xFF when full |

`ShieldAura_Phases` entries 2 and 4 are other units' shared bodies:
`0x4EBA30` (MAGIC151: `+2` on once `+0xB`, the crystals, is 0) and
`0x4D1AA0` (MAGIC105: at `+0xB` 0 the owner's count down and free).
`Shield_Task`'s entry 1, `0x4E5200` (MAGIC131), sets the done flag and
frees once `+0xB`, the auras, is 0.

## 3. MAGIC086 (row 28): the unlabelled effect (Barrier?)

An effect task with a six-entry stack table, and **parts** (kind 1,
parameter 0x1D) of three types by `+1`.

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Barrier_Task` | `0x4C27F0` | 0x46 | row 28: phase `+1` through six entries (`_Start`, `_Tint`, `BattleFx_Brighten` (ours, round eight), `_WaitRings`, `_Fade`, `0x4BDC10`) |
| `Barrier_Start` | `0x4C2840` | 0x116 | the source sprite's position; a disc part and a ring part (`+0xB` 0x30); sound 0x100 |
| `Barrier_Tint` | `0x4C2960` | 0x46 | once the ring says 1: the source's tint (1, 1, 1, 0) |
| `Barrier_WaitRings` | `0x4C29B0` | 0xF | on once the ring says 2 |
| `Barrier_Fade` | `0x4C29C0` | 0x85 | tint down (unguarded); at 0 released and the target flashed |
| `BarrierPart_Dispatch` | `0x4C2A50` | 0x12 | kind 1, 0x1D: `jmp [BarrierPart_Types + 4 * +1]` |
| `BarrierDisc_Task` | `0x4C2A70` | 0x2D | phase `+2`; while live the disc |
| `BarrierDisc_Grow` / `_Wait` / `_Shrink` | `0x4C2AA0` / `0x4C2AD0` / `0x4C2AF0` | 0x29 / 0x14 / 0x1F | grow to 0x10; wait for the ring's 2; shrink and free |
| `BarrierDisc_Draw` | `0x4C2B10` | 0x1C7 | 32 `POLY_G3`s, radius `+0xA * 12`, shaded by `+9` |
| `BarrierRing_Task` | `0x4C2CE0` | 0x2D | phase `+2`; while live the ring |
| `BarrierRing_Grow` .. `_End` | `0x4C2D10` .. `0x4C2DF0` | 0x1C..0x3C | grow; rise (tells the effect 1); lift to 0x16; hold 0x30 frames; settle (tells it 2); end (tells it 0xFF, free) |
| `BarrierRing_Draw` | `0x4C2E30` | 0x456 | 64 `POLY_G4`s: six of every eight stand up as a waving green wall of radius 0xC0 linked at their map cells, the other two (moving with `Frame_Counter`) and all of the first phase lie flat |
| `BarrierLine_Task` | `0x4C3290` | 0x33 | type 2: phase `+2`; while live and past phase 0, the line |
| `BarrierLine_Wait` / `_Delay` / `_Fly` | `0x4C32D0` / `0x4C32F0` / `0x4C3330` | 0x14 / 0x35 / 0x2E | wait for the effect's 1; delay; rise and fall, free below -16 |
| `BarrierLine_Draw` | `0x4C3360` | 0x128 | one `LINE_G3` of three points at heights 0x20, 0x18, 0 |

`0x4BDC10` (MAGIC078, the last entry) waits for the ring's 0xFF, then
`Battle_SetTargetFlag40(target)`, the done flag, free.

`symbols.toml` has each function's evidence and the twelve `.data` tables:
`ShieldSpark_TopColours` / `_BottomColours` / `ShieldAura_DiscColours` /
`_HaloColours` (`0x65B404..0x65B47B`), and the eight handler tables
`ShieldAura_Types` `0x65B47C`, `ShieldAura_Phases` `0x65B480`,
`ShieldSpark_Types` `0x65B494`, `ShieldSpark_Phases` `0x65B498`,
`BarrierPart_Types` `0x65B4AC`, `BarrierDisc_Phases` `0x65B4B8`,
`BarrierRing_Phases` `0x65B4C4`, `BarrierLine_Phases` `0x65B4DC`. They are
read in place by ours, index unchecked, as the original.

## 4. Calls into other units (raw addresses, never bound)

| Address | Unit (group) | Called by | What it is, by reading |
|---|---|---|---|
| `0x4C12F0` | MAGIC082 (S18) | the four crystal phases | a disc of the given radius at the task |
| `0x4F6290` | MAGIC219 | `ShieldSpark_Orbit` (tail) | clears bytes 0..4 of `Sprite_Current`: the pool slot's free |
| `0x4FB790` | LIBRARY (L) | `ShieldAura_Fade` | a kind-1 task 0x48 by (kind & 3, side index) |
| `0x4FB880` | LIBRARY (L) | `ShieldSpark_DrawCrystal` | links `count` primitives by their depths at a map cell, or drops them (`Gfx_PacketNext` back) |
| `0x4E5200` | MAGIC131 | `Shield_Task`'s table | done flag and free at `+0xB` 0 |
| `0x4EBA30` | MAGIC151 | `ShieldAura_Phases[2]` | phase on at `+0xB` 0 |
| `0x4D1AA0` | MAGIC105 | `ShieldAura_Phases[4]` | the owner's count down and free at `+0xB` 0 |
| `0x4BDC10` | MAGIC078 | `Barrier_Task`'s table | at `+0xB` 0xFF the target's flag 0x40, done flag, free |

## 5. The fuzz

`BOF3X_SHADOW=magic_s19` runs one `magic_harness::Run` over the 43 clones
(`tools/magic_rows.py --unit MAGIC083 / MAGIC086 --clones`), with 2,000
rounds per function. Besides the standard regions it compares:

- the crystal pool;
- all 256 tint records (the index is a byte);
- `0x904B80`;
- the GTE vectors `0x9037A0..0x9037BF` and the scratch words
  `0x903850..0x90385F`;
- the two CLUT rows and their sources;
- `Gfx_PacketNext`, and a 12 KB primitive buffer of the fuzz's own that
  it points into;
- MAGIC083's colour tables.

The eight handler tables are swapped for recorders.

**The draws' callees have the group's own recorders** (`Callee::custom`,
[`magic_harness.md`](magic_harness.md) §7). They are needed because:

- `Math_Sin` / `Math_Cos` and the GPU and GTE primitive calls take up to
  ten arguments;
- they write through pointers into the caller's frame and the primitive;
- they change nothing a spell reads, so these recorders log and answer
  but never disturb. A disturbed task byte would send the crystal's
  depth-array store out of the original's frame.

What the custom recorders log and write:

- **Projections** (`Gte_RotTransPers3` / `4`, `Gte_RotAverage3`): they log
  the vectors' contents and the output places, and write salted screen
  points.
- **`Gfx_CommitPrim`, `MapView_LinkPrimAt` and `0x4FB880`**: they move
  `Gfx_PacketNext` as the real ones do, three times in four. `0x4FB880`
  also logs the four depths and zeroes some of them.
- **The other recorders** (ours called by ours, `Sprite_SetTint`,
  `ShieldSpark_Alloc`, `ShieldSpark_Dispatch`): they log the current slot
  and owner and disturb. `ShieldSpark_Dispatch` also flips pool live bits.

**Seeds by function:**

- phase bytes inside their tables;
- the side bit 0x40 set half the time;
- counters at their edges (`+9` / `+0xA` / `+0xB` one off the test, two
  in three);
- the tint's first level at its cap;
- the spin at 3 / 4 / 5 and at values that are negative or have the top bit set;
- `Sprite_Current` in the pool half the time for pool functions;
- the exe's colour tables two in three (read at start-up, not written
  here).

The group's `disturb` moves a pool live bit, a tint level or the action id.

Result (2026-09-26, in this worktree):

    shadow      magic_s19 self-test: 86000 rounds over 43 functions (2000 each), 2759874 calls to the stand-ins,
                0 MISMATCHES; 30192 bytes of state (18 regions) and the stand-ins' log compared

Every recorder and every handler was reached. The coverage lines list
them: `Math_Sin` 612,553 calls, `0x4FB880` 8,000, `ShieldSpark_Alloc`
3,885, and every phase of the eight tables between 301 and 1,437.
`BOF3X_SHADOW='*'`: exit 0.

## 6. Negative controls

Planted one at a time by a script (not committed: apply, build,
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s19`, restore). The table gives
the rounds of 2,000 refused in the function planted.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Shield_Task: table index phase + 1 | Shield_Task 2000 |
| T2 | Shield_Task: pool slot's live bit 2 | Shield_Task 2000 |
| T3 | Shield_Task: owner not set for the crystal | Shield_Task 2000 |
| S1 | Shield_Start: pool byte 2 not cleared | Shield_Start 2000 |
| S2 | Shield_Start: enemies at i + 2 | Shield_Start 980 |
| S3 | Shield_Start: side bit 0x80 | Shield_Start 948 |
| S4 | Shield_Start: delay step 0x0F | Shield_Start 1084 |
| S5 | Shield_Start: aura z from +0x40 | Shield_Start 1655 |
| S6 | Shield_Start: sound 0x101 | Shield_Start 2000 |
| S7 | Shield_Start: second CLUT row not copied | Shield_Start 2000 |
| S8 | Shield_Start: aura's +4 from the slot not the effect | Shield_Start 1643 |
| K1 | Shield_Kind: kind byte test 3 | Shield_Kind 363 |
| K2 | Shield_Kind: rewrites to 0x52 | Shield_Kind 297 |
| A2 | ShieldAura_Task: draws without the +2 test | ShieldAura_Task 227 |
| A3 | ShieldAura_Task: disc before halo | ShieldAura_Task 857 |
| W1 | ShieldAura_Wait: tint (0,0,1,1) | ShieldAura_Wait 1323 |
| W2 | ShieldAura_Wait: party record by +3 + 1 | ShieldAura_Wait 671, ShieldAura_Fade 657 |
| R1 | ShieldAura_Rise: tint cap 7 | ShieldAura_Rise 980 |
| R2 | ShieldAura_Rise: third level not raised | ShieldAura_Rise 1012 |
| R3 | ShieldAura_Rise: crystal delay step 7 | ShieldAura_Rise 1295 |
| R4 | ShieldAura_Rise: crystal +0xA 2 | ShieldAura_Rise 1283 |
| R5 | ShieldAura_Rise: full pool not skipped (slot 0xFF counted) | ShieldAura_Rise 672 |
| F1 | ShieldAura_Fade: enemy actor + 2 | ShieldAura_Fade 645 |
| F2 | ShieldAura_Fade: mark's arguments swapped | ShieldAura_Fade 1297 |
| F3 | ShieldAura_Fade: +0xB not raised | ShieldAura_Fade 1302 |
| D1 | ShieldAura_DrawDisc: shade +9 * 4 | ShieldAura_DrawDisc 1985 |
| D2 | ShieldAura_DrawDisc: green from the red column | ShieldAura_DrawDisc 1700 |
| D3 | ShieldAura_DrawDisc: radius 64 (<< 6) | ShieldAura_DrawDisc 2000 |
| D4 | ShieldAura_DrawDisc: prim not re-read (Gfx_PacketNext kept) | ShieldAura_DrawDisc 2000 |
| D5 | ShieldAura_DrawDisc: commit size 0x30 | ShieldAura_DrawDisc 2000 |
| D6 | DrawModeAndCommit: slot 4 | ShieldAura_DrawDisc 2000, ShieldAura_DrawHalo 2000, BarrierDisc_Draw 2000, BarrierRing_Draw 2000 |
| H1 | ShieldAura_DrawHalo: outer radius << 7 | ShieldAura_DrawHalo 2000 |
| H2 | ShieldAura_DrawHalo: outer colour 0 | ShieldAura_DrawHalo 2000 |
| H3 | ShieldAura_DrawHalo: vertex 2 from vertex 1 | ShieldAura_DrawHalo 2000 |
| L1 | ShieldSpark_Place: radius 160 | ShieldSpark_Place 1337 |
| L2 | ShieldSpark_Place: angle mask 0x3F | ShieldSpark_Place 692 |
| L3 | ShieldSpark_Place: lift step -3 | ShieldSpark_Place 1337 |
| L4 | ShieldSpark_Place: owner not re-read after Sin (+0x38 from the owner before) | **not refused** (below) |
| U1 | ShieldSpark_Rise: disc radius 0x19 base | ShieldSpark_Rise 2000 |
| U2 | ShieldSpark_Rise: spin on odd frames' complement | ShieldSpark_Rise 2000 |
| U3 | Lift: height from the dword's high word | ShieldSpark_Rise 2000, ShieldSpark_Fall 2000 |
| U4 | ShieldSpark_Fall: wait 0x1D - +0xB | ShieldSpark_Fall 1036 |
| N1 | ShieldSpark_Spin: wide below 6 | ShieldSpark_Spin 114 |
| N2 | ShieldSpark_Spin: spin test unsigned | ShieldSpark_Spin 753 |
| N3 | ShieldSpark_Spin: every 4th frame | ShieldSpark_Spin 71 |
| N4 | ShieldSpark_Spin: sounds swapped | ShieldSpark_Spin 105 |
| N5 | ShieldSpark_Spin: sound gate 0xE0 | ShieldSpark_Spin 2 |
| O1 | ShieldSpark_Orbit: radius base 0xA0 | ShieldSpark_Orbit 2000 |
| O2 | ShieldSpark_Orbit: owner count not lowered | ShieldSpark_Orbit 1012 |
| O3 | ShieldSpark_Orbit: +0xB step 1 | ShieldSpark_Orbit 2000 |
| C1 | ShieldSpark_DrawCrystal: apex 0x48 | ShieldSpark_DrawCrystal 2000 |
| C2 | ShieldSpark_DrawCrystal: lower faces from the top colours | ShieldSpark_DrawCrystal 2000 |
| C3 | CrystalFace: second point + 4 | ShieldSpark_DrawCrystal 2000 |
| C4 | CrystalFace: colour row by kind * 3 | ShieldSpark_DrawCrystal 1682 |
| C5 | CrystalFace: depth slot not re-read after the call | **not refused** (below) |
| C6 | CrystalFace: stride 0x30 | ShieldSpark_DrawCrystal 2000 |
| C7 | ShieldSpark_DrawCrystal: second link from the first loop's primitive | ShieldSpark_DrawCrystal 1505 |
| C8 | CrystalFace: semi-transparency 1 | ShieldSpark_DrawCrystal 2000 |
| C9 | ShieldSpark_DrawCrystal: loop bound not re-read | **not refused** (below) |
| X1 | ShieldSpark_Alloc: from slot 1 | ShieldSpark_Alloc 97 |
| X2 | ShieldSpark_Alloc: full answers 0xFE | ShieldSpark_Alloc 340 |
| B1 | Barrier_Task: entries 3 and 4 swapped | Barrier_Task 654 |
| B2 | Barrier_Start: ring +0xB 0x2F | Barrier_Start 1952 |
| B3 | Barrier_Start: part height from +0x3C | Barrier_Start 2000 |
| B4 | Barrier_Start: part parameter 0x1C | Barrier_Start 2000 |
| B5 | Barrier_Start: +9 not cleared | Barrier_Start 1897 |
| B6 | Barrier_Tint: tint (1,1,1,1) | Barrier_Tint 1330 |
| B7 | Barrier_Tint: source read after the test (after nothing: kept), +9 0x0F | Barrier_Tint 1330 |
| B8 | Barrier_WaitRings: waits for 1 | Barrier_WaitRings 1325 |
| B9 | Barrier_Fade: flashes the actor byte | Barrier_Fade 1202 |
| B10 | Barrier_Fade: third level not lowered | Barrier_Fade 1984 |
| Q1 | BarrierPart_Dispatch: disc phase table | BarrierPart_Dispatch 2000 |
| Q2 | BarrierDisc_Grow: at 0x11 | BarrierDisc_Grow 1346 |
| Q3 | BarrierDisc_Wait: waits for 1 | BarrierDisc_Wait 1328 |
| Q4 | BarrierDisc_Shrink: never frees | BarrierDisc_Shrink 1345 |
| Q5 | BarrierDisc_Draw: radius * 11 | BarrierDisc_Draw 1994 |
| Q6 | BarrierDisc_Draw: rim not read back from the primitive | **not refused** (below) |
| Q7 | BarrierDisc_Draw: centre blue * 3 | BarrierDisc_Draw 1993 |
| Q8 | BarrierDisc_Draw: 31 steps | BarrierDisc_Draw 2000 |
| G1 | BarrierRing_Task: draws the disc | BarrierRing_Task 1011 |
| G2 | BarrierRing_Rise: owner +0xB 2 | BarrierRing_Rise 1363 |
| G3 | BarrierRing_Lift: at 0x15 | BarrierRing_Lift 1347 |
| G4 | BarrierRing_Hold: counts +9 | BarrierRing_Hold 2000 |
| G5 | BarrierRing_Settle: +0xA not lowered | BarrierRing_Settle 2000 |
| G6 | BarrierRing_End: +9 lowered through 0 | BarrierRing_End 969 |
| G7 | BarrierRing_End: owner +0xB 0xFE | BarrierRing_End 1325 |
| G8 | BarrierRing_Grow: at 0x0F | BarrierRing_Grow 1354 |
| Y1 | BarrierRing_Draw: outer radius 0xB0 | BarrierRing_Draw 442 |
| Y2 | BarrierRing_Draw: inner radius * 13 | BarrierRing_Draw 1987 |
| Y3 | BarrierRing_Draw: second gap frame + 2 | BarrierRing_Draw 442 |
| Y4 | BarrierRing_Draw: wave by << 4 | BarrierRing_Draw 442 |
| Y5 | BarrierRing_Draw: wall link point z << 8 | BarrierRing_Draw 442 |
| Y6 | BarrierRing_Draw: wall green 0xB0 | BarrierRing_Draw 442 |
| Y7 | BarrierRing_Draw: second +2 test not re-read | **not refused** (below) |
| Y8 | BarrierRing_Draw: flat outer blue +9 << 1 | BarrierRing_Draw 1987 |
| Y9 | BarrierRing_Draw: frame not re-read for the wave | **not refused** (below) |
| Z1 | BarrierLine_Task: draws without the +2 test | BarrierLine_Task 346 |
| Z2 | BarrierLine_Wait: waits for 2 | BarrierLine_Wait 1303 |
| Z3 | BarrierLine_Delay: +0x20 -2 | BarrierLine_Delay 1291 |
| Z4 | BarrierLine_Fly: free at -15 | BarrierLine_Fly 384 |
| Z5 | BarrierLine_Draw: middle height 0x10 | BarrierLine_Draw 2000 |
| Z6 | BarrierLine_Draw: first link dy 1 | BarrierLine_Draw 2000 |
| Z7 | BarrierLine_Draw: angle << 8 | BarrierLine_Draw 1993 |
| T4b | Shield_Task: Sprite_Current read before the phase call | Shield_Task 76 |
| A1b | ShieldAura_Dispatch: through ShieldAura_Phases | ShieldAura_Dispatch 2000 |
| P1b | ShieldSpark_Dispatch: through ShieldSpark_Phases | ShieldSpark_Dispatch 2000 |
| P2b | ShieldSpark_Task: through ShieldSpark_Types | ShieldSpark_Task 2000 |
| K3 | Shield_Kind: returns 1 | Shield_Kind 2000 |

The **6 not refused** (L4, C5, C9, Q6, Y7, Y9) are all the same kind:
ours reading a cell once where the original reads it again after a call,
or reading back a value it has just written. In each case the call
between the two reads is one of the draws' recorders: `Math_Sin`, a GTE
projection, or a GPU primitive call. These recorders never disturb, by
design (section 5). The real callees change nothing the spell reads, so
in the game such a change makes no difference, and no fuzz with realistic
callees can see it. Ours re-reads each of these cells exactly where the
original does all the same.

- L4: the owner, after `Math_Sin`.
- C5: the depth slot's `+9`, after `Gte_RotAverage3`.
- C9: the loop bound's `+9`.
- Q6: the rim bytes, read back from the primitive.
- Y7: `+2`, the second test.
- Y9: `Frame_Counter`, for the wave.

Four first plants were replaced:

- **T4** was a no-op as written (identical code).
- **A1, P1 and P2** indexed a handler table by a byte the seed leaves
  anywhere, so the copy called a random `.data` dword and crashed (exit
  0xC0000005) rather than mismatch. Their replacements T4b, A1b, P1b and
  P2b point the dispatch at the neighbouring table.

Planting **X2** found that the harness did not compare the two answering
functions' results. `Shield_Kind` and `ShieldSpark_Alloc` now carry
`ret_mask 0xFF`. K3 and the re-run X2 were refused with it.

## 7. What nothing reached

No recorded route casts either spell. The combat route's traces enter no
function here (queue §5). The live check is the owner casting Shield and
row 28's ability, with a save that has them or DIV-0045's cheat. What to
look for:

- **Shield:** a disc and a halo on each member, then three crystals each
  that rise, spin and spiral out.
- **Row 28:** a disc and a waving ring on the caster, whose tint is raised
  and lowered.

Nothing creates a part of type 2 (`BarrierLine_*`): no function read here
creates a 0x1D task with `+1` of 2. Only the fuzz reaches those five.

## 8. Latent defects of the original (kept)

- **Unbounded dispatches.** Both stack tables are unchecked (ours aborts
  past them). The eight `.data` tables are also unchecked, and ours reads
  them in place as the original does.
- **Colour index.** The colour tables are indexed by the task's `+4`
  (the kind) unbounded. MAGIC083's kind is always 0, so this is harmless
  as the effect is built.
- **Link point before any wall segment (`BarrierRing_Draw`).** The
  original keeps the wall's link point in registers across segments, and
  tests "stands up" twice per segment (`+2` read twice). If `+2` changed
  between the two tests of a segment before any standing segment had
  run, it would link with an uninitialised stack dword. Nothing the
  function calls changes `+2`, so this is unreachable. Ours aborts there.
- **Face index (`ShieldSpark_DrawCrystal`).** The crystal stores each
  face's depth at `(step - +9) / 8`, with `+9` read again after the call.
  A callee that moved `+9` would store outside the four-entry array, into
  the original's own frame. No real callee does, so this is unreachable.
  Ours aborts past the array.
- **`Barrier_Fade` lowers the tint levels unguarded.** If they start at 0
  they wrap to 0xFF, and the fade then runs 255 more frames. The tint is
  raised by `BattleFx_Brighten` first, so in practice they do not start
  at 0.
- **Stack garbage in arguments.** `Shield_Start` passes a stack dword
  whose upper bytes are uninitialised to `Battle_ActorIsOut`, and
  `ShieldAura_Fade` passes register garbage above the bytes to
  `BattleActor_Flash` and `0x4FB790`. Every callee reads only the low
  byte (read: `0x4456C0`, `0x4FB6F0`), so this is harmless.

## 9. For `analysis/calltrace/entries_logic.txt`

In the main checkout's copy, under a `group S19` comment:

- 34 lines are new.
- `004C1A20 1F1`, `004C1C20 247` and `004C22D0 4B6` were already right.
- The old host lines `004C1710 30F`, `004C1E70 454`, `004C2790 37F`,
  `004C2B10 31C`, `004C2E30 52E` and `004C3360 659` still run over the
  functions after them.
