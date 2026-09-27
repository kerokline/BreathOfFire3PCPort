# Group S34: Charm, (no label), Timed Blow, Transfer and Monopolize (MAGIC158, 159, 161, 162, 166)

**Status:** IN PROGRESS (2026-09-27). All 47 functions are ours
(`src/game/magic_s34.cpp`, shadow name `magic_s34`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 94,000 rounds. 267 negative controls: 265 refused, every one by a count (exit 3); two equivalent mutants not refused, each with a near variant refused. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S34
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 89 | 0x2A2 | MAGIC158 | 0x9E | Charm | `0x4ED670..0x4EDDC6` | 10 |
| 120 | 0x2A3 | MAGIC159 | 0x9F | (no label) | `0x4EDDD0..0x4EE2D7` | 9 |
| 111 | 0x2A4 | MAGIC161 | 0xA1 | Timed Blow | `0x4EE2E0..0x4EE6BC` | 9 |
| 116 | 0x2A5 | MAGIC162 | 0xA2 | Transfer | `0x4EE6C0..0x4EF20E` | 12 |
| 90 | 0x2A6 | MAGIC166 | 0xA6 | Monopolize | `0x4EF210..0x4EF616` | 7 |

The extents are `tools/magic_rows.py --unit MAGIC1NN --clones` (capstone
recursive descent; no jump table; nothing `REFUSED`). All 47 functions lie in
the units' extents; none was found inside or missing from them, and none was
ours before. MAGIC162's extent holds one function more, `BattleFx_WaitStep4`
(`0x4EE8A0`, group CK's, already ours), which Transfer's table calls by name.
7,664 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. Note the row order: the
queue lists the rows 89, 90, 111, 116, 120 against the overlays in file
order, but `Magic_Rows` pairs MAGIC159 with row 120 and MAGIC166 with row 90
(`analysis/magic_rows.tsv`); the names in the queue line follow the overlays.
What each spell looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Charm (MAGIC158)** and **Monopolize (MAGIC166)** are one effect with two
  pools. Each owns a pool of 64 task-like records of 0x84 bytes in `.bss`
  (`Charm_Pool` `0x6A9108`, `Monopolize_Pool` `0x6AC810`): the kind-2 task
  runs its two-entry stack table (`_Start`, `BattleFx_Finish`) and then walks
  the pool, making each live record (bit 0 of +0) `Sprite_Current` with its
  +0x80 as the owner for the record's task, both put back after each.
  - `_Start` clears the pool's +0..+2, puts the task at the source sprite
    (`0x904B4C`: facing and point), and takes 16 (Charm) or 32 (Monopolize)
    records from its allocator (`_PoolAlloc`, first free by bit 0, 0xFF when
    full, tested), each owned by the task, +0 or 0x41, numbered in +0xB, a
    delay of 1..0x20 in +9; sound 0x100.
  - A mote's task is a one-entry `.data` table to `_Run`, which steps through
    a `.data` table by +2 and, while +0 and +2 are set, pushes
    `CharmMote_PushMatrix` (angles 0x400, 0xE00 and the frame counter's low
    five bits, inverted on odd motes; at the mote's point and height, after the
    camera), draws `CharmMote_Draw` (a fan of five semi-transparent flat quads,
    radii +9 x 2 / x 4, coloured by the mote's triple x +0xA) and pops it.
    Monopolize's run calls Charm's push and draw.
  - The steps: `_Launch` waits out the delay, then places the mote round the
    owner (the kind-2 task, at the source sprite's point) at the angle
    (+0xB & 0xF) << 8, picks one of eight colour triples (`Charm_Colours` /
    `Monopolize_Colours`, which hold the same bytes), speed 0x80 and a fall
    (-16 / -8). Charm puts its height word 0x480 off the owner's and has
    `_ToOwner` step it back by 0x60 a frame until it is the owner's, growing
    +9 / +0xA to 0x10; then `_Fall` spirals it (the speed into the height,
    the fall into the speed) and frees it when +0xA runs out. Monopolize
    launches at the owner's height and `_Fall` grows +9 to 0x10 before
    counting +0xA down.
- **MAGIC159** (no label one id down): `Magic159_Start` puts the task at the
  source sprite, makes one child (kind 1, 0x58, +9 8), restores CLUT row 26
  with its STP bits, flags the target 0x10 and plays 0x100. The child
  (`Magic159Child_*`) waits +9 frames, moves to the owner, then grows (+0xA
  up by 2 to 0x10), holds (+9 past 0x20) and fades (+0xA down by 2, then the
  owner's count down and freed), drawing each frame one semi-transparent
  textured gouraud quad at the owner's screen point + (8, -0x10), radius
  +9 x 2, shaded +0xA x 8 on the top corners and +0xA on the bottom ones.
- **Timed Blow (MAGIC161)**: `TimedBlow_Start` puts the task at the owner,
  sets the actor's animation 0xC (`BattleActor_SetAnimation(0xC, 2)`), makes
  a child (kind 1, 0x54) that is a byte copy of the acting actor's record's
  first 0x80 bytes, and sets bit 0x40 of the owner's +0 (what that bit does
  to the owner was not read). The copy
  (`TimedBlowCopy_*`) sizes itself (`BattleFx_SetSize`), ticks its script
  until +9 runs out (`_Strike`: `BattleActor_PlaySound(2, 4)`, the target
  flagged 0x40), then flashes (`_Flash`: +9 up by 4 to 0x10, sound 0x100)
  drawing `TimedBlow_DrawFlash` - two screen-wide gouraud quads,
  (0, 0)..(319, 120) and (0, 120)..(319, 359) in float constants, red at the
  outer edges fading to +9 x (0xC, 6, 6) at the shared one - then
  `BattleFx_ScriptToEnd` (a shared step of eleven files: the script ticked
  until it reports its end, then the owner's count down) and
  `BattleFx_FreeTask`. `TimedBlow_Wait` clears the owner's bit 0x40 and sets
  animation 0x18 once the copy is gone.
- **Transfer (MAGIC162)** has a different pool: 128 records of 0x2C bytes
  (`Transfer_Pool` `0x6AB208`, owner at +0x28) run through a pointer that
  follows the pool (`TransferMote_Current` `0x6AC808`), not through
  `Sprite_Current`. The task's six-entry table is `Transfer_Start`, then four
  shared steps (`BattleFx_TintActor`, `_Brighten`, `_WaitStep4`,
  `Sparkle_End`) and `BattleFx_Finish`, and after it the walk.
  - `Transfer_Start` clears the pool, sets kind +4 = 4, and makes
    `Transfer_Counts[4]` records, each with a colour
    variant +3 (Rand & 3), its number +7 and a delay +5 of (number / 4) x
    `Transfer_Delays[4]` + 1.
  - A record's steps: `_Start` waits the delay, then takes the owner's point
    and screen point, offsets the screen point by `TransferMote_Offsets[+7 &
    0x1F]` and Rand & 7, and sets a sway (+8 base, +0xA the y step Rand & 1,
    +0xC a phase) and a rise length +7; `_Rise` sways it (x = base + a sine
    of the phase x 24 >> 12, y by +0xA) for +7 frames; `_Fade` sways it,
    shrinks +6 every fourth frame and frees it after 0x10 frames.
  - Each frame, a live record draws a star of eight gouraud triangles
    (`_DrawStar`, radius +6 + Rand & 3, centre shade from
    `TransferMote_Colours` x +5, rim +5) and, when its phase +0xC & 3 is not
    0, four gouraud rays (`_DrawRays`) and four two-segment forks
    (`_DrawForks`) turning with the frame counter. The vertices are floats
    (`fild` / `fstp`), as in every PC draw of the port.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the project's
one precedent: a phase past any of the dispatch tables (six stack tables, eight
`.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3).
Calls that push more than the callee takes push it in ours too (the flag
pointer to `Gte_RotTrans`, the depth and flag to `Gte_RotTransPers4`).

## 3. Calls to other units

None by raw address: every callee outside the units is already ours and
called by name - `BattleFx_Finish`, `BattleFx_SetSize`, `BattleFx_TintActor`,
`BattleFx_Brighten`, `BattleFx_WaitStep4` (group CK), `Sparkle_End` (CJ),
`MagicFx_DoneAndFree` (E), `BattleFx_FreeTask`, the effect library's
`BattleActor_*` (L), and the GTE / GPU / sprite / sound / battle-task library.

Shared bodies: `BattleFx_ScriptToEnd` (`0x4EE560`) is a step of eleven files
(`analysis/magic_funcs.tsv`); group S02's `ElemStrikeCopy_Run` calls it by
address (`magic_s02.cpp`'s `kCopyStep2`), which keeps working through the
jmp. `CharmMote_PushMatrix` / `_Draw` are reached from MAGIC158 and MAGIC166,
both in this group.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `Charm_Pool` | `0x6A9108` | 64 x 0x84 |
| `Transfer_Pool` | `0x6AB208` | 128 x 0x2C |
| `TransferMote_Current` | `0x6AC808` | pointer |
| `Monopolize_Pool` | `0x6AC810` | 64 x 0x84 |
| `Charm_Colours` | `0x65BF00` | 8 x 3 |
| `CharmMote_TaskTable` | `0x65BF18` | 1 |
| `CharmMote_Steps` | `0x65BF1C` | 3 |
| `Magic159Child_TaskTable` | `0x65BF28` | 1 |
| `Magic159Child_Steps` | `0x65BF2C` | 4 |
| `TimedBlowCopy_TaskTable` | `0x65BF3C` | 1 |
| `TransferMote_Offsets` | `0x65BF40` | 32 x 2 |
| `TransferMote_Colours` | `0x65BFE0` | 5 x 4 x 3 |
| `Transfer_Counts` | `0x65C01C` | 5 |
| `Transfer_Delays` | `0x65C024` | 5 |
| `Transfer_Rise` | `0x65C02C` | 5 |
| `TransferMote_TaskTable` | `0x65C034` | 1 |
| `Monopolize_Colours` | `0x65C038` | 8 x 3 |
| `MonopolizeMote_TaskTable` | `0x65C050` | 1 |
| `MonopolizeMote_Steps` | `0x65C054` | 2 |

Each count is where the next table starts (the dump of `0x65BF00..0x65C05C`
read 2026-09-27) and, for the byte tables, what the code indexes; the three
by-kind tables are padded to 8 bytes and only kind 4 is ever set. The 0x20
bytes `0x65BFC0..0x65BFDF` after the offsets are read by nothing in the
group. The pools' extents are the walks' and the allocators' counts; the
three are consecutive `.bss`, the current pointer between the second and the
third.

## 5. The fuzz

`BOF3X_SHADOW=magic_s34` runs `magic_harness::Run` over the 47 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s34_fuzz.cpp`:

- **Callees** (38 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs each primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own (group S31's);
  - the projection logs its four SVECTORs through `deref`; the matrix push's
    GTE callees log theirs and write a result where the real ones write;
  - `Sprite_ScriptTickOnce` and `Sprite_UpdateScreen` log which sprite they
    act on, and the tick answers exactly 1 a third of the time (section 6,
    B27);
  - this group's own functions called directly: the three allocators answer
    `kByte` "none or an index inside the pool" (0xFF..0x3F, 0xFF..0x7F) - an
    al answer, and each allocator's clone has `ret_mask 0xFF`; the pool
    tasks, pushes and draws are `kPhase` recorders, MAGIC162's with
    `masks[0]` = `0x6AC808` so the record they run for is logged; the two
    line draws, which take two words, log their arguments masked to 16 bits
    and the current record (an `effect`).
- **Tables:** the eight `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer, `Prim_VertexScratch`, `0x903850..0x90385F`, the three pools and the
  current pointer (`0x6A9108..0x6AE90F`), CLUT row 26 and its source. 43,164
  bytes of state in 15 regions.
- **Seed:** the current record pointer at a pool record; each dispatcher
  inside its table; each count one step before and at its threshold (the
  delays at 1, `_ToOwner`'s 0x10s and the height word 0x60 above the
  owner's, Grow's 0x10, Hold's 0x20, Fade's 2, the flash's 0x10, the
  record's +5 / +6 / +7, every fourth frame); the allocators' pools full, all
  but one taken, or random; the walks' records' owner fields at real slots
  (the harness's disturbance writes through the owner).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte, the current record pointer and a byte of that record, a pool record's
  live bit, and the task's words +0x14, +0x20, +0x2E, +0x30, +0x3E.

Result in this worktree (2026-09-27):

    shadow      magic_s34 self-test: 94000 rounds over 47 functions (2000 each), 1031542 calls to the stand-ins,
                0 MISMATCHES; 43164 bytes of state (15 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (1,032,344
stand-in calls for this group in that run: the harness's pointers into the DLL
move a few branches; 0 mismatches).

## 6. Controls

267 plants, each put in `magic_s34.cpp` one at a time by a script (not
committed) that planted, rebuilt, checked the build had recompiled the file,
ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s34`, and restored; after the
last it restored, rebuilt and ran the clean self-test (0 mismatches, exit 0).
Each plant is anchored on a string the script checked occurs exactly once.
**265 of 267 refused**, every one by exit 3 with a count only in the
functions the plant touches (a plant in a shared helper counts in each of its
callers). The ids: H shared helpers, W / S / P / R / L / T / F / M / D / CT /
MO the mote pools (Charm, Monopolize), G / C MAGIC159, B Timed Blow, X / TM /
TS / TR / TF / LH / DR / DF / DS / FR Transfer.

**Not refused, both equivalent:**

- **E1** (Monopolize's launch reading `Charm_Colours`): the two eight-triple
  tables hold the same 24 bytes (read 2026-09-27), so no input tells them
  apart. Near variant **E2** (`TransferMote_Colours`): refused in 517 rounds.
- **X9** (`Transfer_Start` giving up below a count of 2 instead of at 0): the
  count is a byte of `Transfer_Counts` indexed by a task byte, and none of the
  256 bytes that index can reach is 1 (read 2026-09-27). Near variant **X9b**
  (at or below the kind-4 count): refused in 1,915 rounds.

**One fuzz change came out of the controls.** B27 (`BattleFx_ScriptToEnd`
testing `al & 0xFE`) was first not refused: the harness's `kFlag` answers a
random non-zero byte, which is 1 only one time in 255, where the real
`Sprite_ScriptTickOnce` answers 0 or 1. Its stand-in now answers exactly 1 a
third of the time (an `effect` in the fuzz file, no harness edit); B19..B28
(the three functions that call it) were re-planted and all refused, B27 in 645
rounds.

The thinnest (fewer than 60 rounds of 2,000):

- **L4** (launch: the cosine of the local angle, not the scratch word read
  back): 1 - only a disturbance of the scratch word between the two calls
  tells them apart;
- **TS17** (Transfer start: the x pointer taken after `Rand`): 1;
- **TS19** (the offset index read before `Rand`): 2;
- **TM9** (the forks' length read before the rays are drawn): 4;
- **TS18** 5, **B13** 7, **P3** 8, **TR8** 9, **LH4** 14, **D8** 15,
  **DF7** 41, **B21** 52, **F2** 58.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| H1 | Rtp4: third point at 3 steps | CharmMote_Draw 2000 |
| H2 | LinkAtSprite: z from +0x3C | CharmMote_Draw 2000, Magic159Child_Draw 2000 |
| H3 | LinkAtCur: x from +0x1C | TransferMote_DrawRays 2000, TransferMote_DrawForks 2000, TransferMote_DrawStar 2000 |
| H4 | DrawMode: dtd 0 | CharmMote_Draw 2000, Magic159Child_Draw 2000, TimedBlow_DrawFlash 2000, TransferMote_DrawRays 2000, TransferMote_DrawForks 2000, TransferMote_DrawStar 2000 |
| W1 | walk: live bit 2 | Charm_Task 2000, Monopolize_Task 2000 |
| W2 | walk: owner not put back | Charm_Task 1743, Monopolize_Task 1767 |
| W3 | walk: the next record owner | Charm_Task 2000, Monopolize_Task 2000 |
| W4 | walk: 63 records | Charm_Task 1004, Monopolize_Task 982 |
| W5 | walk: Sprite_Current not put back | Charm_Task 1981, Monopolize_Task 1980 |
| S1 | start: +2 not cleared | Charm_Start 2000, Monopolize_Start 2000 |
| S2 | start: facing from src +9 | Charm_Start 1755, Monopolize_Start 1615 |
| S3 | start: +0 or 0x40 | Charm_Start 2000, Monopolize_Start 2000 |
| S4 | start: +0xB i + 1 | Charm_Start 2000, Monopolize_Start 2000 |
| S5 | start: delay & 0x3F | Charm_Start 1995, Monopolize_Start 2000 |
| S6 | start: index 0x3F skipped | Charm_Start 424, Monopolize_Start 780 |
| S7 | start: record owned by the owner | Charm_Start 1964, Monopolize_Start 1996 |
| S8 | start: count not kept | Charm_Start 1998, Monopolize_Start 1997 |
| S9 | start: sound 0x101 | Charm_Start 2000, Monopolize_Start 2000 |
| S10 | Charm: 15 motes | Charm_Start 2000 |
| S11 | Monopolize: 31 motes | Monopolize_Start 2000 |
| S12 | start: Rand before the record | Charm_Start 716, Monopolize_Start 1109 |
| P1 | alloc: taken by bit 1 too | Charm_PoolAlloc 665, Transfer_PoolAlloc 646, Monopolize_PoolAlloc 672 |
| P2 | alloc: none 0xFE | Charm_PoolAlloc 680, Transfer_PoolAlloc 710, Monopolize_PoolAlloc 648 |
| P3 | Charm alloc: 63 records | Charm_PoolAlloc 8 |
| P4 | Transfer alloc: stride 0x28 | Transfer_PoolAlloc 1665 |
| P5 | Monopolize alloc: stride 0x80 | Monopolize_PoolAlloc 1676 |
| P6 | alloc: bit 0 not set | Charm_PoolAlloc 1320, Transfer_PoolAlloc 1290, Monopolize_PoolAlloc 1352 |
| R1 | run: +0 alone | CharmMote_Run 353, MonopolizeMote_Run 513 |
| R2 | run: draw before push | CharmMote_Run 668, MonopolizeMote_Run 541 |
| R3 | Charm steps: ToOwner / Fall swapped | CharmMote_Run 1320 |
| R4 | Monopolize steps swapped | MonopolizeMote_Run 2000 |
| R5 | run: no pop | CharmMote_Run 668, MonopolizeMote_Run 541 |
| L1 | launch: angle & 0x1F | CharmMote_Launch 264, MonopolizeMote_Launch 241 |
| L2 | launch: x sar 11 | CharmMote_Launch 517, MonopolizeMote_Launch 517 |
| L3 | launch: z from the owner +0x34 | CharmMote_Launch 517, MonopolizeMote_Launch 517 |
| L4 | launch: cosine of the local angle | CharmMote_Launch 1 |
| L5 | Charm launch: lift 0x4000000 | CharmMote_Launch 517 |
| L6 | launch: colour + 1 | CharmMote_Launch 517, MonopolizeMote_Launch 517 |
| L7 | launch: Rand & 3 | CharmMote_Launch 269, MonopolizeMote_Launch 255 |
| L8 | launch: speed 0x81 | CharmMote_Launch 517, MonopolizeMote_Launch 517 |
| L9 | Charm launch: +0xA 3 | CharmMote_Launch 517 |
| L10 | Monopolize launch: fall -9 | MonopolizeMote_Launch 517 |
| L11 | Monopolize launch: lift 0x100 | MonopolizeMote_Launch 517 |
| L12 | launch: on at 1 | CharmMote_Launch 491, MonopolizeMote_Launch 503 |
| L13 | Charm launch: +9 9 | CharmMote_Launch 517 |
| L14 | launch: height from +0x38 | CharmMote_Launch 517, MonopolizeMote_Launch 517 |
| E1 | Monopolize launch: Charm_Colours (equivalent?) | not refused (exit 0) |
| T1 | ToOwner: step 0x5F | CharmMote_ToOwner 2000 |
| T2 | ToOwner: +9 to 0x11 | CharmMote_ToOwner 538 |
| T3 | ToOwner: +0xA by 1 | CharmMote_ToOwner 1483 |
| T4 | ToOwner: owner +0x3C | CharmMote_ToOwner 836 |
| F1 | spiral: x sl 13 | CharmMote_Fall 2000, MonopolizeMote_Fall 2000 |
| F2 | spiral: x pointer after the call | CharmMote_Fall 75, MonopolizeMote_Fall 58 |
| F3 | spiral: fall from +0x1C | CharmMote_Fall 2000, MonopolizeMote_Fall 2000 |
| F4 | spiral: height by +0x16 | CharmMote_Fall 2000, MonopolizeMote_Fall 2000 |
| F5 | Charm fall: sl 7 | CharmMote_Fall 1889 |
| F6 | Monopolize fall: & 0xF | MonopolizeMote_Fall 979 |
| F7 | free: +4 kept | CharmMote_Fall 489, MonopolizeMote_Fall 170 |
| F8 | free: owner +0xA | CharmMote_Fall 490, MonopolizeMote_Fall 170 |
| F9 | Charm fall: free at 1 | CharmMote_Fall 921 |
| F10 | Monopolize fall: below 0x10 | MonopolizeMote_Fall 708 |
| F11 | Monopolize fall: +9 not up | MonopolizeMote_Fall 1346 |
| F12 | spiral: z pointer after the call | CharmMote_Fall 71, MonopolizeMote_Fall 61 |
| M1 | push: rot x 0x401 | CharmMote_PushMatrix 2000 |
| M2 | push: rot y 0xE01 | CharmMote_PushMatrix 2000 |
| M3 | push: inverted by bit 1 | CharmMote_PushMatrix 1035 |
| M4 | push: inverted & 0x3F | CharmMote_PushMatrix 498 |
| M5 | push: sl 6 | CharmMote_PushMatrix 976 |
| M6 | push: x sar 8 | CharmMote_PushMatrix 2000 |
| M7 | push: z - 0x3FFF | CharmMote_PushMatrix 2000 |
| M8 | push: height sar 1 | CharmMote_PushMatrix 480 |
| M9 | push: camera twice | CharmMote_PushMatrix 2000 |
| M10 | push: task read before the push | CharmMote_PushMatrix 67 |
| D1 | draw: inner radius x 3 | CharmMote_Draw 1991 |
| D2 | draw: red unsigned | CharmMote_Draw 953 |
| D3 | draw: blue x +9 | CharmMote_Draw 1979 |
| D4 | draw: step 0x19A | CharmMote_Draw 2000 |
| D5 | draw: four quads | CharmMote_Draw 2000 |
| D6 | draw: last closed at 6 | CharmMote_Draw 2000 |
| D7 | draw: outer point at the inner radius | CharmMote_Draw 1991 |
| D8 | draw: cosine of the local angle | CharmMote_Draw 15 |
| D9 | draw: vertex +0x14 1 | CharmMote_Draw 2000 |
| D10 | draw: depths at +4 | CharmMote_Draw 2000 |
| D11 | draw: green from the red | CharmMote_Draw 1960 |
| D12 | draw: linked 0x34 | CharmMote_Draw 2000 |
| D13 | draw: vertex +0xC 1 | CharmMote_Draw 2000 |
| D14 | draw: projected at 0x10 | CharmMote_Draw 2000 |
| D15 | draw: tpage 0x36 | CharmMote_Draw 2000 |
| D16 | draw: flat quad G4 | CharmMote_Draw 2000 |
| CT1 | Charm_Task: entries swapped | Charm_Task 2000 |
| CT2 | Charm_Task: Monopolize pool | Charm_Task 2000 |
| MO1 | Monopolize_Task: entries swapped | Monopolize_Task 2000 |
| MO2 | Monopolize_Task: Charm pool | Monopolize_Task 2000 |
| MO3 | Monopolize_Task: Charm mote task | Monopolize_Task 2000 |
| G1 | Magic159_Task: entries swapped | Magic159_Task 2000 |
| G2 | Magic159_Start: z from +0x3C | Magic159_Start 2000 |
| G3 | Magic159_Start: parameter 0x59 | Magic159_Start 2000 |
| G4 | Magic159_Start: child +9 9 | Magic159_Start 1998 |
| G5 | Magic159_Start: child +2 0 | Magic159_Start 1985 |
| G6 | Magic159_Start: STP 0x4000 | Magic159_Start 2000 |
| G7 | Magic159_Start: 0xFF words | Magic159_Start 2000 |
| G8 | Magic159_Start: dirty 2 | Magic159_Start 2000 |
| G9 | Magic159_Start: flags 0x11 | Magic159_Start 2000 |
| G10 | Magic159_Start: child owned by the owner | Magic159_Start 1763 |
| G11 | Magic159_Start: sound 0x102 | Magic159_Start 2000 |
| G12 | Magic159_Start: count not kept | Magic159_Start 1980 |
| G13 | Magic159_Start: self read before the call | Magic159_Start 60 |
| C1 | Magic159Child_Run: Grow / Hold swapped | Magic159Child_Run 1011 |
| C2 | Magic159Child_Run: x + 9 | Magic159Child_Run 755 |
| C3 | Magic159Child_Run: y - 0xF | Magic159Child_Run 745 |
| C4 | Magic159Child_Run: +2 alone | Magic159Child_Run 755 |
| C5 | Wait: +9 0x11 | Magic159Child_Wait 531 |
| C6 | Wait: +0xA 1 | Magic159Child_Wait 531 |
| C7 | Wait: height from +0x38 | Magic159Child_Wait 531 |
| C8 | Grow: at 0x12 | Magic159Child_Grow 503 |
| C9 | Grow: +9 not up | Magic159Child_Grow 2000 |
| C10 | Hold: at 0x20 | Magic159Child_Hold 501 |
| C11 | Fade: by 1 | Magic159Child_Fade 2000 |
| C12 | Fade: owner count kept | Magic159Child_Fade 530 |
| C13 | Draw: shade x 7 | Magic159Child_Draw 1988 |
| C14 | Draw: dim shade +9 | Magic159Child_Draw 1984 |
| C15 | Draw: corners 3/4 swapped | Magic159Child_Draw 2000 |
| C16 | Draw: x from +0x30 | Magic159Child_Draw 2000 |
| C17 | Draw: y radius word 52 | Magic159Child_Draw 1999 |
| C18 | Draw: tpage abr 2 | Magic159Child_Draw 2000 |
| C19 | Draw: clut 0x1FB | Magic159Child_Draw 2000 |
| C20 | Draw: u 0x58 | Magic159Child_Draw 2000 |
| C21 | Draw: v 9 | Magic159Child_Draw 2000 |
| C22 | Draw: v 0x49 | Magic159Child_Draw 2000 |
| C23 | Draw: bright corners dim | Magic159Child_Draw 1991 |
| C24 | Draw: linked 0x50 | Magic159Child_Draw 2000 |
| C25 | Draw: GT4 as FT4 | Magic159Child_Draw 2000 |
| C26 | Draw: packet read before the link | Magic159Child_Draw 2000 |
| B1 | TimedBlow_Task: last two swapped | TimedBlow_Task 1342 |
| B2 | Start: facing from the owner +9 | TimedBlow_Start 1916 |
| B3 | Start: height from +0x38 | TimedBlow_Start 1956 |
| B4 | Start: animation 0xD | TimedBlow_Start 2000 |
| B5 | Start: parameter 0x55 | TimedBlow_Start 2000 |
| B6 | Start: party at 0..1 | TimedBlow_Start 403 |
| B7 | Start: 0x7C bytes copied | TimedBlow_Start 2000 |
| B8 | Start: +6 2 | TimedBlow_Start 2000 |
| B9 | Start: +5 0x55 | TimedBlow_Start 2000 |
| B10 | Start: +3 cleared for +2 | TimedBlow_Start 2000 |
| B11 | Start: owner bit 0x20 | TimedBlow_Start 1538 |
| B12 | Start: actor read before the call | TimedBlow_Start 73 |
| B13 | Wait: at 1 | TimedBlow_Wait 7 |
| B14 | Wait: and 0x9F | TimedBlow_Wait 434 |
| B15 | Wait: animation (0x18, 1) | TimedBlow_Wait 983 |
| B16 | Wait: +2 on | TimedBlow_Wait 983 |
| B17 | Copy_Run: Strike / Flash swapped | TimedBlowCopy_Run 811 |
| B18 | Copy_Run: +0 alone | TimedBlowCopy_Run 207 |
| B19 | Strike: sound (2, 5) | TimedBlowCopy_Strike 491 |
| B20 | Strike: the actor flagged | TimedBlowCopy_Strike 446 |
| B21 | Strike: task read before the tick | TimedBlowCopy_Strike 52 |
| B22 | Strike: +1 on | TimedBlowCopy_Strike 491 |
| B23 | Flash: at or past 0x10 | TimedBlowCopy_Flash 896 |
| B24 | Flash: +9 by 2 | TimedBlowCopy_Flash 1489 |
| B25 | Flash: sound 0x101 | TimedBlowCopy_Flash 487 |
| B26 | Flash: tick before the draw | TimedBlowCopy_Flash 2000 |
| B27 | ScriptToEnd: bit 0 ignored | BattleFx_ScriptToEnd 645 |
| B28 | ScriptToEnd: owner +0xA | BattleFx_ScriptToEnd 1545 |
| B29 | DrawFlash: edge x 0xB | TimedBlow_DrawFlash 1990 |
| B30 | DrawFlash: mid x 5 | TimedBlow_DrawFlash 1990 |
| B31 | DrawFlash: right 318.0 | TimedBlow_DrawFlash 2000 |
| B32 | DrawFlash: middle 120.5 | TimedBlow_DrawFlash 2000 |
| B33 | DrawFlash: bottom 358.0 | TimedBlow_DrawFlash 2000 |
| B34 | DrawFlash: mid red | TimedBlow_DrawFlash 1974 |
| B35 | DrawFlash: bottom green 0x61 | TimedBlow_DrawFlash 2000 |
| B36 | DrawFlash: closing tpage 0x16 | TimedBlow_DrawFlash 2000 |
| B37 | DrawFlash: layer 2 | TimedBlow_DrawFlash 2000 |
| B38 | DrawFlash: second top at the bottom | TimedBlow_DrawFlash 2000 |
| B39 | Shade: blue from the red | TimedBlow_DrawFlash 2000 |
| B40 | DrawFlash: commit 0x40 | TimedBlow_DrawFlash 2000 |
| B41 | DrawFlash: first quad G3 | TimedBlow_DrawFlash 2000 |
| X1 | Transfer_Task: Tint / Brighten swapped | Transfer_Task 649 |
| X2 | Transfer_Task: owner not put back | Transfer_Task 1745 |
| X3 | Transfer_Task: the next record owner | Transfer_Task 2000 |
| X4 | Transfer_Task: live bit 2 | Transfer_Task 2000 |
| X5 | Transfer_Start: kind 3 | Transfer_Start 1953 |
| X6 | Transfer_Start: +2 not cleared | Transfer_Start 2000 |
| X7 | Transfer_Start: +9 9 | Transfer_Start 1657 |
| X8 | Transfer_Start: sound 0x103 | Transfer_Start 2000 |
| X9 | Transfer_Start: none below 2 | not refused (exit 0) |
| X10 | Transfer_Start: +3 Rand & 7 | Transfer_Start 1893 |
| X11 | Transfer_Start: kind from +5 | Transfer_Start 1947 |
| X12 | Transfer_Start: number in +6 | Transfer_Start 1947 |
| X13 | Transfer_Start: delay by pairs | Transfer_Start 1796 |
| X14 | Transfer_Start: delay from Transfer_Rise | Transfer_Start 1689 |
| X15 | Transfer_Start: one record fewer | Transfer_Start 356 |
| X16 | Transfer_Start: record owned by the owner | Transfer_Start 1907 |
| X17 | Transfer_Start: task read before Rand | Transfer_Start 1089 |
| X18 | Transfer_Start: count not kept | Transfer_Start 1947 |
| X19 | Transfer_Start: facing from +9 | Transfer_Start 1621 |
| TM1 | TransferMote_Run: Rise / Fade swapped | TransferMote_Run 1333 |
| TM2 | Run: live by bit 1 too | TransferMote_Run 169 |
| TM3 | Run: lines by bit 0 | TransferMote_Run 249 |
| TM4 | Run: rays from the frame shr 2 | TransferMote_Run 717 |
| TM5 | Run: rays length +7 | TransferMote_Run 716 |
| TM6 | Run: forks + 5 | TransferMote_Run 717 |
| TM7 | Run: forks shr 2 | TransferMote_Run 710 |
| TM8 | Run: star only with the lines | TransferMote_Run 281 |
| TM9 | Run: forks length read before the rays | TransferMote_Run 4 |
| TS1 | Start: on at 1 | TransferMote_Start 968 |
| TS2 | Start: x from the owner +0x38 | TransferMote_Start 511 |
| TS3 | Start: y word from +0x32 | TransferMote_Start 511 |
| TS4 | Start: height from +0x38 | TransferMote_Start 511 |
| TS5 | Start: 0 counted negative | TransferMote_Start 103 |
| TS6 | Start: negative plus Rand | TransferMote_Start 149 |
| TS7 | Start: positive Rand & 3 | TransferMote_Start 149 |
| TS8 | Start: y by the x offset | TransferMote_Start 511 |
| TS9 | Start: y moved down | TransferMote_Start 511 |
| TS10 | Start: base from y | TransferMote_Start 511 |
| TS11 | Start: +0xA Rand & 3 | TransferMote_Start 202 |
| TS12 | Start: +0xC Rand & 0xFFF | TransferMote_Start 449 |
| TS13 | Start: rise Rand & 7 | TransferMote_Start 271 |
| TS14 | Start: rise from Transfer_Delays | TransferMote_Start 316 |
| TS15 | Start: +5 0x11 | TransferMote_Start 511 |
| TS16 | Start: +6 1 | TransferMote_Start 511 |
| TS17 | Start: x pointer after Rand | TransferMote_Start 1 |
| TS18 | Start: y pointer after Rand | TransferMote_Start 5 |
| TS19 | Start: offset index before Rand | TransferMote_Start 2 |
| TR1 | Sway: +0xC by 2 | TransferMote_Rise 2000, TransferMote_Fade 2000 |
| TR2 | Sway: angle & 0x1F | TransferMote_Rise 1032, TransferMote_Fade 998 |
| TR3 | Sway: x 25 | TransferMote_Rise 2000, TransferMote_Fade 2000 |
| TR4 | Sway: base from +0xA | TransferMote_Rise 2000, TransferMote_Fade 2000 |
| TR5 | Sway: y by +0xC | TransferMote_Rise 2000, TransferMote_Fade 2000 |
| TR6 | Rise: at or past +7 | TransferMote_Rise 525 |
| TR7 | Sway: angle word 54 | TransferMote_Rise 2000, TransferMote_Fade 2000 |
| TR8 | Sway: record read before the call | TransferMote_Rise 10, TransferMote_Fade 9 |
| TF1 | Fade: every eighth frame | TransferMote_Fade 603 |
| TF2 | Fade: owner count kept | TransferMote_Fade 521 |
| TF3 | Fade: not freed | TransferMote_Fade 522 |
| TF4 | Fade: +7 down | TransferMote_Fade 1175 |
| TF5 | Fade: free at 1 | TransferMote_Fade 995 |
| LH1 | Lines: y from x | TransferMote_DrawRays 2000, TransferMote_DrawForks 1999 |
| LH2 | Lines: shade x 5 | TransferMote_DrawRays 1987, TransferMote_DrawForks 1992 |
| LH3 | Lines: blue + 1 | TransferMote_DrawRays 1997, TransferMote_DrawForks 1995 |
| LH4 | Lines: point stored after the packet | TransferMote_DrawRays 22, TransferMote_DrawForks 14 |
| DR1 | Rays: three | TransferMote_DrawRays 2000 |
| DR2 | Rays: length a byte | TransferMote_DrawRays 1996 |
| DR3 | Rays: angle & 0x3F | TransferMote_DrawRays 1768 |
| DR4 | Rays: end y from x | TransferMote_DrawRays 2000 |
| DR5 | Rays: end shade 2 | TransferMote_DrawRays 2000 |
| DR6 | Rays: G3 line | TransferMote_DrawRays 2000 |
| DR7 | Rays: linked 0x30 | TransferMote_DrawRays 2000 |
| DF1 | Forks: inner a quarter | TransferMote_DrawForks 2000 |
| DF2 | Forks: end x at the inner | TransferMote_DrawForks 2000 |
| DF3 | Forks: start shade 2 | TransferMote_DrawForks 2000 |
| DF4 | Forks: end shade 2 | TransferMote_DrawForks 2000 |
| DF5 | Forks: G2 line | TransferMote_DrawForks 2000 |
| DF6 | Forks: five | TransferMote_DrawForks 2000 |
| DF7 | Forks: middle shade green from red | TransferMote_DrawForks 41 |
| DS1 | Star: radius Rand & 7 | TransferMote_DrawStar 985 |
| DS2 | Star: kind x 3 | TransferMote_DrawStar 1762 |
| DS3 | Star: red from green | TransferMote_DrawStar 1254 |
| DS4 | Star: rim from +6 | TransferMote_DrawStar 1986 |
| DS5 | Star: seven | TransferMote_DrawStar 2000 |
| DS6 | Star: step 0x180 | TransferMote_DrawStar 2000 |
| DS7 | Star: first rim x from y | TransferMote_DrawStar 1999 |
| DS8 | Star: rim blue | TransferMote_DrawStar 1952 |
| DS9 | Star: G4 | TransferMote_DrawStar 2000 |
| DS10 | Star: Rand before the packet | TransferMote_DrawStar 2000 |
| DS11 | Star: second rim y from the sine | TransferMote_DrawStar 2000 |
| FR1 | Free: +4 kept | TransferMote_Free 1989 |
| FR2 | Free: the next record | TransferMote_Free 2000 |
| E2 | Monopolize launch: Transfer colours (E1 near variant) | MonopolizeMote_Launch 517 |
| X9b | Transfer_Start: none at 0x3C or below (X9 near variant) | Transfer_Start 1915 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat.
Things to look for, by reading:

- Charm: sixteen coloured motes appear one by one off the source sprite's
  point, close on its height and spiral.
- Monopolize: 32 such motes, launched at that height.
- MAGIC159: the target flagged, one textured glow at its screen point that
  grows, holds and fades.
- Timed Blow: a copy of the acting actor strikes (a sound, the target
  flagged), the screen flashes red at top and bottom.
- Transfer: the shared tint and brighten steps, then the sparks rising
  from the owner along a sine, with rays and forks on some.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the six stack tables and the
  eight `.data` tables (an index past a one-entry task table reads the next
  table). Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Magic159_Start` and `TimedBlow_Start`: slot 255 lies past the image's end
  - an access violation, in ours as in the original (the same address is
  written).
- **`TimedBlow_Start` indexes the enemy records by the actor byte - 3**,
  unchecked above 10, and copies 0x80 bytes from there.
- **`TransferMote_DrawStar` indexes `TransferMote_Colours` by +4 x 4 + +3**
  with no bound; +4 is only ever 4 (`Transfer_Start`), so it stays inside
  the table.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 42 lines under a `group S34` comment: 35 new,
plus seven host extents re-listed smaller (`004ED7D0 12`, `004EDD70 57`,
`004EE580 13D`, `004EE8C0 12`, `004EF190 4A`, `004EF370 12`, `004EF5C0 57`,
each the function's own size). Five were listed right already (`004EDA60`,
`004EDB30`, `004EEBE0`, `004EED70`, `004EEF60`).
