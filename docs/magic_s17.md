# Group S17: Purify, Raise Dead / Resurrect, Leech Power

**Status:** IN PROGRESS (2026-09-26) - 48 functions ours
(`src/game/magic_s17.cpp`, shadow name `magic_s17`), fuzzed headless
through the shared harness, 0 mismatches; 125 negative controls, every one refused by a count (exit 3). No recorded
route casts any of them: fuzz only until the owner's eye.

Round nine, first spell wave ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)
§4, row S17), taken with [`magic_harness.md`](magic_harness.md).

## 1. The overlays and their names

| Unit | `Magic_Rows` row | File | Ability ids | Read one id down | Extent |
|---|--:|---|---|---|---|
| MAGIC075 | 46 | 0x260 | 0x4B, 0xB3 | Purify | `0x4BCBF0..0x4BCFA6`, 8 functions |
| MAGIC077 | 30 | 0x262 | 0x4C, 0xB4; 0x4D, 0xB5 | Raise Dead; Resurrect | `0x4BCFB0..0x4BDAB6`, 17 functions |
| MAGIC078 | 15 | 0x263 | 0x4E, 0xB6 | Leech Power | `0x4BDAC0..0x4BEB45`, 23 functions |

The extents are `tools/magic_rows.py --unit`'s, and every function in them
was read to its last instruction (capstone). None was missing and none was
extra. The names are hypotheses (cut-content §2). One reading is
consistent with the shift: MAGIC077's `Revive_IsStrong` (0x4BD280) returns
0 for the ability ids 0x4C / 0xB4 and 1 for any other ability. The 1 draws
the larger halo and sheds the second row's count of motes. So the two ids
of one row are a weak and a strong spell, which fits Raise Dead and
Resurrect. Which is the stronger in play is a guess, not a measurement.

`MagicFx_WaitA` (0x4BDD20) is MAGIC078's by address but is reached from 17
files. It is one body the linker kept here: +0xA down, and at 0 +2 on.

## 2. What each function does

The code comments above each function in `magic_s17.cpp` are the full
reading. `symbols.toml` has one entry per function with its extent and
calls. In short:

**MAGIC075 (Purify).**
- `Purify_Task` steps a three-entry stack table.
- `Purify_Start` takes the source's position and creates seven motes
  (kind 1, parameter 0x25), with delays 1..31 by 5. It re-tints the source
  (0, 0, 0, 1), restores CLUT row 26's first 16 cells and plays sound 0x100.
- `Purify_Glow` sets the tint's r, g, b to twice the count of landed motes.
  At seven it moves on.
- `Purify_Fade` takes 2 off each channel. At 0 it releases the tint, sets
  the target's done flag and frees the task.
- The mote dispatches through `PurifyMote_Types` / `_Phases` with the
  effects' frame-offset table (0x9039D8 = 0x8E3580) swapped in.
  `PurifyMote_Place` places it round its owner and sets its sprite fields.
  `PurifyMote_End` runs its script once, counts itself on the owner and
  frees itself.

**MAGIC077 (Raise Dead / Resurrect).**
- `Revive_Task` steps a six-entry stack table (two entries are round
  eight's `BattleFx_Brighten` / `BattleFx_Finish`). It then runs every live
  record of its own 64-record pool at 0x68C0B8 (`ReviveMote_Pool`), with
  `Sprite_Current` and the owner cell switched for each.
- `Revive_Start` empties the pool, takes the strength, creates the halo
  (kind 1, parameter 0x62) and plays sound 0x100. It gives CLUT row 26 and
  row 2's first 16 cells the semi-transparency bit (cell 0 of each without).
- The tint phases re-tint the source, wait for the motes' count to reach 0,
  and fade.
- The halo:
  - `ReviveHalo_Wait` takes the owner's strength and position, 0xC00000
    higher.
  - `ReviveHalo_Spawn` allocates motes from the pool (`ReviveMote_Alloc`),
    as many as `ReviveHalo_MoteCounts[+4]`.
  - `MagicFx_WaitA` waits.
  - MAGIC060's 0x4B1740 ends it.
  - `ReviveHalo_Draw` draws a textured, semi-transparent FT4 quad over the
    target's screen point. Its size and texture cells depend on the
    strength.
- A mote:
  - `ReviveMote_Task` steps a three-entry stack table between two draw
    modes.
  - `ReviveMote_Launch` sets its orbit (radius +0xC, angle +0xB << 8), its
    rise and three colour bytes, from `Rand`.
  - `ReviveMote_Rise` widens the orbit, rises by a shrinking step and counts
    down on odd frames. At 0 it counts itself off the owner and calls
    MAGIC219's 0x4F6290.
  - `ReviveMote_Draw` draws a fan of eight gouraud triangles.

**MAGIC078 (Leech Power).**
- `Leech_Task` steps a two-entry stack table between two draw modes.
- `Leech_Start` creates two children (kind 1, parameter 0xD):
  - the shell, 0x2000000 above the caster;
  - the orb, 0x1000000 above the source, whose owner is the shell and whose
    +0x4C is the effect's task.
- `Leech_WaitOrbs` ends the effect when +0xB is 0xFF.
- The shell (`LeechShell_*`):
  - It grows to +9 = 12.
  - It spins (+0xC from the frame counter) until it faces the source
    (`Math_Ratan2`).
  - It holds, then signals the orb (the task's +0xB = 1) and waits for the
    merge (+0xB = 2).
  - It fades, then ends with +0xB = 0xFF.
  - `LeechShell_Draw` draws two bands of six flat quads, each outlined by a
    line quad (`LeechShell_Edge`), and two caps of two quads. It sorts the
    16 quads and the 12 lines by depth with the library's 0x4FB880.
  - `LeechShell_PushMatrix` is `MagicFx_PushActorMatrix` with the spin
    about z.
- The orb (`LeechOrb_*`):
  - It waits for the shell's signal, chimes, then rises toward the shell
    (0x4FB9F0 / 0x4FBBD0).
  - It pauses, then merges (sets the task's +0xB to 2) and fades out.
  - `LeechOrb_DrawRings` draws 8 rings of 16 flat quads (128 quads a
    frame), each sorted at its own far edge.

No divergence and no ledger entry. Each function is a faithful
replacement, except the four stack-table dispatchers (`Purify_Task`,
`Revive_Task`, `ReviveMote_Task`, `Leech_Task`): past their table they
abort, as the project's precedent has it. The five `.data` tables are read
in place with the index unchecked, as the originals read them.

## 3. The fuzz

`BOF3X_SHADOW=magic_s17` (`magic_s17_fuzz.cpp`) runs 2,000 rounds for each
function. Beyond the harness's standard callees, the group lists these
callees:
- the GPU and GTE setters;
- the sin, cos and atan helpers;
- `Sprite_SetTint`, `Tint_Release` and `Sprite_SetAnimation`;
- the draw commits;
- its own functions that its other functions call;
- the raw addresses other groups own (section 5).

**Custom stand-ins** (`Callee::custom`):
- **The GTE projections, matrix setters and `LeechShell_Edge`** log the
  bytes their stack pointers point at, not the pointers. They write noise
  where the real ones write (the projected vertices, the matrix, the
  depth), so the caller reads defined values afterwards.
- **`Gfx_CommitPrim` and `MapView_LinkPrimAt`** move `Gfx_PacketNext` on by
  the primitive's size, three times in four, inside the group's 12 KB
  packet buffer. That spreads each quad of a draw over its own bytes of the
  compared state.
- **The draw-mode setter, `Sprite_SetTint` and the library's sort** take
  more than four arguments; their stand-ins log every one.
- **`Math_Ratan2`** answers the spin the seed aimed at half the time.
- **`ReviveMote_Alloc`** answers 0..63 or 0xFF.
- **The library's 0x4FBBD0** answers 0 or a non-zero eax.
- **`ReviveMote_Dispatch`** logs which record it was run on and that
  record's owner.
- **`Sprite_UpdateScreen`** (a standard callee, re-listed) logs the
  frame-offset table pointer 0x9039D8, which `PurifyMote_Task` swaps around
  it. This was added after control T3 (the swap left out) passed the
  harness's own recorder.

**Other harness features used:**
- The five `.data` handler tables are swapped for recorders. Adjacent
  tables are listed as one run.
- `Revive_IsStrong` and `ReviveMote_Alloc` compare their result
  (`ret_mask` 0xFF).
- `LeechShell_Edge` is called through `Group::args` with the seed's
  vertices and out cell.
- `Group::settle` keeps the phase byte that `Leech_Task`,
  `LeechShell_Task` and `LeechOrb_Task` read after a call inside their
  table.
- The group's own disturbance moves the packet pointer, a vertex word or a
  radius / colour word.

**Regions:**
- all 256 tint records;
- CLUT rows 2 and 26 and their sources;
- the vertex scratch and the scratch words at 0x903850;
- the frame-offset table pointer;
- the ability word;
- the mote pool;
- the group's buffer.

**The seed** puts in each function's boundaries:
- the table indexes, inside what the originals' tables hold;
- countdowns at 1;
- the thresholds and one past them (the fade's 0x80, the shell's 12, the
  mote-count rows 0..3);
- the ability ids 0x4C / 0xB4 / 0x4D / 0xB5 and near misses;
- the rise step equal to the rise, or one past it either way;
- the frame counter's low byte at 0x40;
- a lit quad's number in the frame bits;
- a pool full to a random depth;
- every sprite pointer the functions follow (the four slots' +0x4C, the
  pool's owners) made real.

Result, in this worktree (2026-09-26, after the port onto the folded
harness `ea27991`):

    shadow      magic_s17 self-test: 96000 rounds over 48 functions (2000 each), 5424020 calls to the stand-ins,
                0 MISMATCHES; 36356 bytes of state (20 regions) and the stand-ins' log compared

Every recorder was reached. The coverage lines (`build/bof3x.log`) count
every phase entry at 232..1,726 calls. `BOF3X_SHADOW='*'`: exit 0.

## 4. The negative controls

Each control was planted one at a time by a script (not committed): apply,
build, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s17`, restore. The
counts are rounds of 2,000 in which the planted function mismatched.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| P1 | Purify_Task: entries 0 and 1 swapped | Purify_Task 1,323 |
| P2 | Purify_Start: delay step 6 | Purify_Start 2,000 |
| P3 | Purify_Start: a mote +0xB its delay | Purify_Start 2,000 |
| P4 | Purify_Start: tint alpha 2 | Purify_Start 2,000 |
| P5 | Purify_Start: 15 CLUT cells | Purify_Start 2,000 |
| P6 | Purify_Start: sound 0x101 | Purify_Start 2,000 |
| G1 | Purify_Glow: on at six | Purify_Glow 788 |
| G2 | Purify_Glow: green x4 | Purify_Glow 1,930 |
| F1 | Purify_Fade: red down 1 | Purify_Fade 2,000 |
| F2 | Purify_Fade: end on green | Purify_Fade 364 |
| F3 | TargetDone: flag 8 | Purify_Fade 241, Leech_WaitOrbs 691 |
| D1 | PurifyMote_Dispatch: by +2 | PurifyMote_Dispatch 1,325 |
| T1 | PurifyMote_Task: update gated on +1 | PurifyMote_Task 440 |
| T2 | PurifyMote_Task: battle table + 4 | PurifyMote_Task 2,000 |
| T3 | PurifyMote_Task: effect table not set | PurifyMote_Task 787 |
| PL1 | PurifyMote_Place: angle & 3 | PurifyMote_Place 673 |
| PL2 | PurifyMote_Place: x radius 6 | PurifyMote_Place 1,336 |
| PL3 | PurifyMote_Place: height + 0x181 | PurifyMote_Place 1,336 |
| PL4 | PurifyMote_Place: +0x27 0xA1 | PurifyMote_Place 1,336 |
| PL5 | PurifyMote_Place: animation +0xB / 4 | PurifyMote_Place 1,298 |
| PL6 | PurifyMote_Place: cos angle not re-read | PurifyMote_Place 5 |
| E1 | PurifyMote_End: own +0xB up | PurifyMote_End 1,201 |
| R1 | Revive_Task: entries 0 and 1 swapped | Revive_Task 684 |
| R2 | Revive_Task: owner not put back | Revive_Task 1,606 |
| R3 | Revive_Task: live bit 1 | Revive_Task 2,000 |
| RS1 | Revive_Start: +3 cleared, not +2 | Revive_Start 2,000 |
| RS2 | Revive_Start: +9 0x3D | Revive_Start 1,931 |
| RS3 | Revive_Start: halo +9 2 | Revive_Start 1,999 |
| RS4 | Revive_Start: row 26 cell 0 keeps the bit | Revive_Start 1,003 |
| RS5 | Revive_Start: +0xA up, not +0xB | Revive_Start 2,000 |
| TS1 | Revive_TintSource: +9 9 | Revive_TintSource 1,349 |
| W1 | Revive_WaitMotes: waits on +0xA | Revive_WaitMotes 981 |
| RF1 | Revive_Fade: blue down 2 | Revive_Fade 2,000 |
| RF2 | Revive_Fade: releases the owner | Revive_Fade 986 |
| IS1 | Revive_IsStrong: 0xB5 weak | Revive_IsStrong 318 |
| IS2 | Revive_IsStrong: kind 5 | Revive_IsStrong 1,002 |
| IS3 | Revive_IsStrong: id by its low byte | Revive_IsStrong 323 |
| HD1 | ReviveHalo_Dispatch: the next entry | ReviveHalo_Dispatch 2,000 |
| HT1 | ReviveHalo_Task: draw before the screen point | ReviveHalo_Task 777 |
| HW1 | ReviveHalo_Wait: height + 0xC00001 | ReviveHalo_Wait 1,321 |
| HW2 | ReviveHalo_Wait: owner +5 | ReviveHalo_Wait 1,320 |
| HS1 | ReviveHalo_Spawn: at 0x11 | ReviveHalo_Spawn 1,345 |
| HS2 | ReviveHalo_Spawn: delay & 7 | ReviveHalo_Spawn 646 |
| HS3 | ReviveHalo_Spawn: owner the current slot | ReviveHalo_Spawn 658 |
| HS4 | ReviveHalo_Spawn: one mote more | ReviveHalo_Spawn 254 |
| HR1 | ReviveHalo_Draw: height 0x61 | ReviveHalo_Draw 1,048 |
| HR2 | ReviveHalo_Draw: half 0x17 | ReviveHalo_Draw 952 |
| HR3 | ReviveHalo_Draw: corner 3 x - half | ReviveHalo_Draw 2,000 |
| HR4 | ReviveHalo_Draw: tpage y 0x101 | ReviveHalo_Draw 2,000 |
| HR5 | ReviveHalo_Draw: v3 0x49 | ReviveHalo_Draw 830 |
| HR6 | ReviveHalo_Draw: shade +9 * 4 | ReviveHalo_Draw 1,982 |
| HR7 | ReviveHalo_Draw: size 0x44 | ReviveHalo_Draw 2,000 |
| MD1 | ReviveMote_Dispatch: the orbs table | ReviveMote_Dispatch 2,000 |
| MT1 | ReviveMote_Task: second mode 0x35 | ReviveMote_Task 2,000 |
| MT2 | ReviveMote_Task: 0x4F6290 for 0x4F1BD0 | ReviveMote_Task 773 |
| ML1 | ReviveMote_Launch: sound for mote 1 | ReviveMote_Launch 719 |
| ML2 | ReviveMote_Launch: radius 9 | ReviveMote_Launch 1,320 |
| ML3 | ReviveMote_Launch: height + 0x800001 | ReviveMote_Launch 1,309 |
| ML4 | ReviveMote_Launch: rise << 19 | ReviveMote_Launch 1,332 |
| ML5 | ReviveMote_Launch: step an eighth | ReviveMote_Launch 1,332 |
| ML6 | ReviveMote_Launch: colour + 5 | ReviveMote_Launch 1,332 |
| ML7 | ReviveMote_Launch: +0xA 0x11 | ReviveMote_Launch 1,332 |
| MR1 | ReviveMote_Rise: <= not < | ReviveMote_Rise 300 |
| MR2 | ReviveMote_Rise: frame bit 1 | ReviveMote_Rise 1,095 |
| MR3 | ReviveMote_Rise: owner +0xA down | ReviveMote_Rise 545 |
| MR4 | Orbit: z from the owner x | ReviveMote_Launch 1,332, ReviveMote_Rise 2,000 |
| MA1 | MoteAngle: & 7 | ReviveMote_Launch 317, ReviveMote_Rise 983 |
| MW1 | ReviveMote_Draw: radius 9 | ReviveMote_Draw 1,983 |
| MW2 | ReviveMote_Draw: shade * 11 | ReviveMote_Draw 1,961 |
| MW3 | ReviveMote_Draw: green unsigned | ReviveMote_Draw 928 |
| MW4 | ReviveMote_Draw: step 0x180 | ReviveMote_Draw 2,000 |
| MW5 | ReviveMote_Draw: v2 green from blue | ReviveMote_Draw 1,964 |
| AL1 | ReviveMote_Alloc: bits 0 and 1 | ReviveMote_Alloc 975 |
| AL2 | ReviveMote_Alloc: full 0xFE | ReviveMote_Alloc 36 |
| LT1 | Leech_Task: entries swapped | Leech_Task 2,000 |
| LT2 | ModeCommit4: slot 5 | Leech_Task 2,000, LeechShell_Task 2,000, LeechOrb_Task 2,000 |
| LS1 | Leech_Start: shell + 0x2000001 | Leech_Start 1,950 |
| LS2 | Leech_Start: orb +0x4C the shell | Leech_Start 1,959 |
| LS3 | Leech_Start: orb +1 2 | Leech_Start 2,000 |
| LS4 | Leech_Start: source read again | Leech_Start 92 |
| LW1 | Leech_WaitOrbs: at 0xFE | Leech_WaitOrbs 989 |
| OD1 | LeechOrb_Dispatch: by +2 | LeechOrb_Dispatch 1,539 |
| ST1 | LeechShell_Task: full spin at 0x41 | LeechShell_Task 224 |
| ST2 | LeechShell_Task: spin before phase 4 | LeechShell_Task 305 |
| ST3 | LeechShell_Task: own +0xB gates the draw | LeechShell_Task 799 |
| SG1 | LeechShell_Grow: to 13 | LeechShell_Grow 1,039 |
| WA1 | MagicFx_WaitA: steps +1 | MagicFx_WaitA 1,011 |
| SA1 | LeechShell_Aim: dx, dz swapped | LeechShell_Aim 2,000 |
| SA2 | LeechShell_Aim: spin >> 6 | LeechShell_Aim 4 |
| SH1 | LeechShell_Hold: owner +0xB 3 | LeechShell_Hold 982 |
| SW1 | LeechShell_WaitOrb: waits for 3 | LeechShell_WaitOrb 984 |
| SF1 | LeechShell_Fade: >= 0x80 | LeechShell_Fade 324 |
| SE1 | LeechShell_End: owner +0xB 0xFE | LeechShell_End 1,317 |
| SD1 | LeechShell_Draw: height +9 * 8 | LeechShell_Draw 1,988 |
| SD2 | LeechShell_Draw: lines 0x384 on | LeechShell_Draw 2,000 |
| SD3 | LeechShell_Draw: lit quad by frame >> 3 | LeechShell_Draw 193 |
| SD4 | LeechShell_Draw: fade red + 0x11 | LeechShell_Draw 1,599 |
| SD5 | LeechShell_Draw: caps depths swapped | LeechShell_Draw 2,000 |
| SD6 | LeechShell_Draw: cap vertex 2 at -4 | LeechShell_Draw 2,000 |
| SD7 | LeechShell_Draw: 15 quads sorted | LeechShell_Draw 2,000 |
| SD8 | LeechShell_Draw: packet left 0x38 short | LeechShell_Draw 2,000 |
| SD9 | ShellVertex: sin for cos | LeechShell_Draw 2,000 |
| SD10 | LeechShell_Draw: lit blue 0x41 | LeechShell_Draw 113 |
| SD11 | LeechShell_Draw: edge vertices 2 and 3 swapped | LeechShell_Draw 2,000 |
| ED1 | LeechShell_Edge: blue 0xC1 | LeechShell_Edge 2,000 |
| ED2 | LeechShell_Edge: vertices 2 and 3 swapped | LeechShell_Edge 2,000 |
| PM1 | LeechShell_PushMatrix: spin about y | LeechShell_PushMatrix 948 |
| PM2 | LeechShell_PushMatrix: height >> 1 | LeechShell_PushMatrix 485 |
| PM3 | LeechShell_PushMatrix: z - 0x4001 | LeechShell_PushMatrix 2,000 |
| OT1 | LeechOrb_Task: rings gated on +1 | LeechOrb_Task 762 |
| OW1 | LeechOrb_WaitShell: waits for 2 | LeechOrb_WaitShell 980 |
| OC1 | LeechOrb_Chime: sound 0x102 | LeechOrb_Chime 1,016 |
| OR1 | LeechOrb_Rise: speed 0x31 | LeechOrb_Rise 2,000 |
| OR2 | LeechOrb_Rise: 0x11 higher | LeechOrb_Rise 2,000 |
| OR3 | LeechOrb_Rise: range 0x1C001 | LeechOrb_Rise 2,000 |
| OP1 | LeechOrb_Pause: sound 0x103 | LeechOrb_Pause 980 |
| OM1 | LeechOrb_Merge: +0x4C read after | LeechOrb_Merge 59 |
| OM2 | LeechOrb_Merge: speed 9 | LeechOrb_Merge 2,000 |
| OE1 | LeechOrb_End: at 9 | LeechOrb_End 1,022 |
| DR1 | LeechOrb_DrawRings: radius +9 * 8 | LeechOrb_DrawRings 1,991 |
| DR2 | LeechOrb_DrawRings: seven rings | LeechOrb_DrawRings 2,000 |
| DR3 | LeechOrb_DrawRings: fade green << 2 | LeechOrb_DrawRings 2,000 |
| DR4 | LeechOrb_DrawRings: quad size 0x34 | LeechOrb_DrawRings 2,000 |
| DR5 | LeechOrb_DrawRings: z from the edge z | LeechOrb_DrawRings 2,000 |
| DR6 | LeechOrb_DrawRings: fifteen quads | LeechOrb_DrawRings 2,000 |

The thinnest controls:
- **SA2 (4 rounds):** a spin divided by 64 as a shift. It shows only for a
  negative spin that is not a multiple of 64, and only when the atan
  answer's six bits land on it.
- **PL6 (5 rounds):** the cosine's angle not read back. It shows only when a
  recorder moves the word 0x903850 between the two calls.
- **AL2 (36 rounds):** the full pool's 0xFE. The seed fills the pool to the
  top in about 1 round in 65.
- **OM1 (59 rounds):** the task pointer read after the calls. It shows only
  when a recorder moves `Sprite_Current` to another slot.

The first run crashed HD1 (reading `ReviveHalo_Phases` by +1 past its end
calls data), and it was re-planted as the next entry. T3 passed the first
run and was refused after the `Sprite_UpdateScreen` recorder above. Every
other control was refused in the function it touches, or in each function
sharing the planted helper (`TargetDone`, `Orbit`, `MoteAngle`,
`ModeCommit4`).

## 5. Calls into other groups' units

These go through raw addresses (`MH_AT` / a stack table's immediate / a
`.data` entry). None is bound or renamed here:

| Address | Unit (group) | Called from | What it looks like by its callers |
|---|---|---|---|
| `0x4FB880` | LIBRARY (L) | `LeechShell_Draw` | (x, z, long *depths, prims, count, stride, dy): links `count` primitives of `stride` bytes into the row the position sorts to, deepest first, zeroing the depths |
| `0x4FB9F0` | LIBRARY (L) | `LeechOrb_Rise`, `_Merge` | (target, s16 speed): `Sprite_Current` moved toward the target along the normalised difference |
| `0x4FBBD0` | LIBRARY (L) | `LeechOrb_Rise`, `_Merge` | (target, range): 1 when within range on x, z and height |
| `0x4F1BD0` | MAGIC169 | `ReviveMote_Task` | a mote's second draw (reached from MAGIC077 / 168 / 169) |
| `0x4F1A40` | MAGIC169 | `ReviveMote_Task`'s table, entry 1 | a mote phase |
| `0x4F6290` | MAGIC219 | `ReviveMote_Rise` (tail jmp) | the end of a child, reached from 35 files |
| `0x4B1740` | MAGIC060 | `ReviveHalo_Phases` entry 3 | the halo's end, reached from 37 files |

`BattleFx_Brighten` / `BattleFx_Finish` (round eight's) are called through
`Revive_Task`'s table by address, and the harness's standard set covers
the rest.

## 6. What nothing reached

No recorded route casts any of these rows (the queue's section 5): the
combat route's traces enter none of the 48. The live check is the owner
casting Purify, Raise Dead / Resurrect and Leech Power, with a save that
has them or DIV-0045's cheat. What to look for:
- **Purify:** the glow on the caster whitening as seven motes land, then
  fading.
- **Raise Dead / Resurrect:** the halo and its motes, larger and more
  numerous for one of the two ids.
- **Leech Power:** the blue shell over the caster turning to face the
  target, the red rings rising from the target and merging into it.

The colours are by the rgb bytes in the code; what they look like is the
owner's eye.

## 7. Latent defects of the original (kept)

- **The four stack tables are unbounded.** `Purify_Task` (3 entries),
  `Revive_Task` (6), `ReviveMote_Task` (3) and `Leech_Task` (2): a phase
  byte past the table calls through the caller's stack. Ours aborts.
- **`Leech_Task` reads its phase after two calls.** It reads +1 after its
  draw mode and commit. Nothing in the game moves +1 there, so this is
  latent only.
- **The five `.data` tables are read unchecked.** Each is followed by the
  next table or by data. For example, `ReviveHalo_Phases` by +2 past 3
  reads the mote-count bytes as a pointer.
- **`ReviveHalo_Spawn` indexes the count table by the halo's +4 unchecked.**
  +4 is only ever 0 or 1 in the game.
