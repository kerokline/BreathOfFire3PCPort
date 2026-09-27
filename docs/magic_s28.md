# Group S28: Firebreath, Icebreath and ThundrBreath (MAGIC122..124), and Port_DroppedCall

**Status:** IN PROGRESS (2026-09-26). All 42 functions of the three units
are ours (`src/game/magic_s28.cpp`, shadow name `magic_s28`), among them
`Port_DroppedCall` `0x4DF820`, the program's empty function. They are fuzzed
headless through the shared harness, unedited: 0 mismatches over 84,000
rounds. 153 negative controls: 152 refused, one an equivalent mutant (section 6). Nothing recorded casts these spells, so the three
overlays are fuzz only until the owner sees them cast; `Port_DroppedCall` is
the one function in the band a recorded route reaches (section 2).

Round nine, second wave, group S28
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|---|---|---|---|---|---|--:|
| 74 | `0x28A` | MAGIC122 | `0x7A` | Firebreath | `0x4DD8B0..0x4DE8BD` | 11 |
| 76 | `0x28B` | MAGIC123 | `0x7B` | Icebreath | `0x4DE8C0..0x4DF4A6` | 12 |
| 124 | `0x28C` | MAGIC124 | `0x7C` | ThundrBreath | `0x4DF4B0..0x4E0906` | 19 |

The extents are `tools/magic_rows.py --unit MAGIC0NN --clones` (capstone,
every jump internal, no jump table, no `REFUSED` line). All 42 are inside
them; none was found inside or missing. 42 functions, 12,012 bytes, as the
queue counted. The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. By reading, the code
fits a breath of three kinds: a beam from the caster to the targets (122), a
spray of motes (123), and bolts that drop sparks (124). Which is which in
play has not been measured.

Throughout, `+n` is a byte of `Sprite_Current` (the task or pool record being
run), the owner is `0x93B940`, and "the scratch +n" is a word of
DamageScratch `0x903850..0x90385F`.

## 1. What each function does

`symbols.toml` gives each to the instruction; in outline:

### MAGIC122: the beam (row 74)

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Firebreath_Task` | `0x4DD8B0` | 0x26 | the kind-2 task: a two-entry stack table by `+1`, Start and S17's `Leech_WaitOrbs` (waits for `+0xB` 0xFF, then flag 0x40, the done flag, free) |
| `Firebreath_Start` | `0x4DD8E0` | 0x103 | the owner's direction and position; one beam (kind 1, `0x38`); CLUT row 26 (`0x812980`) from its buffer, words 1..15 and 0x21..0x2F half transparent (bit 15), words 0, 0x10, 0x20 zero; `Gfx_ClutStripDirty` |
| `BreathBeam_Task` | `0x4DD9F0` | 0x12 | kind 1 `0x38`: `jmp [BreathBeam_Phases + 4 x +1]`, one entry |
| `BreathBeam_Run` | `0x4DDA10` | 0x35 | a step through `BreathBeam_Steps` by `+2`; then, unless the owner's `+0xB` is 0xFF or `+2` is 0, the glow band and the textured band |
| `BreathBeam_Aim` | `0x4DDA50` | 0x1A8 | after `+9` frames: the targets' centre (`MagicFx_CenterOnSide`), the caster's screen point moved by the formation offset (a party caster) or a fixed offset by direction (an enemy), `+0x14` the `Math_Ratan2` angle between them; `+4` 0x10, `+0xB` 0x11, `+0xA` 1; sound 0x100 |
| `BreathBeam_Widen` | `0x4DDC00` | 0x2A | `+0xA` up by 3 a frame for 8 frames |
| `BreathBeam_Hit` | `0x4DDC30` | 0x3F | `+0xB` down for 16 frames, then the target flags 0x10 |
| `BreathBeam_Hold` | `0x4DDC70` | 0x1C | until `+9` is 0x78 |
| `BreathBeam_Fade` | `0x4DDC90` | 0x42 | `+0xB` up, `+4` (the shade) down, until `+9` 0x88: the owner's `+0xB` 0xFF, free |
| `BreathBeam_DrawTextured` | `0x4DDCE0` | 0x682 | `+0xA` steps from the caster along `+0x14`, the path swinging by the sine of `+9 + i`; two `POLY_GT4` a step, one either side, half-widths from a sine over `+0xB` |
| `BreathBeam_DrawGlow` | `0x4DE370` | 0x54E | the same walk with two `POLY_G4` a step, wider, shaded from `+4` at the edge |

`BreathBeam_Run` .. `_DrawGlow` are reached from MAGIC114, 115, 118, 120
and 121 too (their own task tables name `BreathBeam_Run`); taking them here
takes them for those overlays (S26's and S27's).

### MAGIC123: the motes (row 76)

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Icebreath_Task` | `0x4DE8C0` | 0xB4 | the kind-2 task: Start, `BattleFx_Finish` by `+1`; then draw mode 0x35, every live record of `BreathMote_Pool` run through `IcePool_Dispatch` as `Sprite_Current` (its `+0x80` the owner), draw mode 0x15 |
| `Icebreath_Start` | `0x4DE980` | 0x1F9 | the pool cleared; the task placed and aimed as the beam's Aim does; 90 motes (`+1` i & 1, `+9` a delay (i / 4) x 3 + 1); sound 0x102; the target flags 0x10 |
| `IcePool_Dispatch` | `0x4DEB80` | 0x12 | `jmp [IcePool_Kinds + 4 x +1]` (both entries `BreathMote_Task`) |
| `BreathMote_Task` | `0x4DEBA0` | 0x12 | `jmp [BreathMote_Steps + 4 x +2]`: Launch, Fly, Burst, Free |
| `BreathMote_Launch` | `0x4DEBC0` | 0xDA | after its delay: at the owner's screen point, the owner's heading turned by up to 0xFF either way, a random speed, phase, flight time; size 2 |
| `BreathMote_Fly` | `0x4DECA0` | 0x144 | moves along a heading that swings by the sine of `+0xB`; grows by 2 up to `+3` + 6; at the end of its flight a sound on odd frames (0x100 or 0x101 by `+1`); draws its hexagon and ring while on the screen |
| `BreathMote_Burst` | `0x4DEDF0` | 0x45 | `+0xA` frames of burst, then the owner's count down and the record freed (MAGIC219's `0x4F6290`) |
| `BreathMote_Free` | `0x4DEE40` | 0xD | the owner's count down and the record freed |
| `BreathMote_DrawHex` | `0x4DEE50` | 0x173 | six `POLY_G3` round the point, corners at the `BreathMote_Angles` steps |
| `BreathMote_DrawRing` | `0x4DEFD0` | 0x232 | six `POLY_G4` between radius `+0xA` and `+0xA` x 2 + Rand & 7 |
| `BreathMote_DrawBurst` | `0x4DF210` | 0x23E | eight `POLY_G3`, each round a point thrown out along 0x200 k |
| `IcePool_Alloc` | `0x4DF450` | 0x57 | the first free record of the 90, in al; 0xFF when none |

`BreathMote_Task` .. `_DrawBurst` are reached from seven files
(`tools/magic_rows.py`).

### MAGIC124: the bolts (row 124)

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Thunderbreath_Task` | `0x4DF4B0` | 0xC9 | the kind-2 task: Start, MAGIC226's `0x4F9F70` (8 frames, then the target flags 0x10), Wait, `BattleFx_Finish` by `+1`; then draw mode 0x35, every live record of `ThunderPool` through `ThunderPool_Dispatch`, the orb, draw mode 0x15 |
| `Thunderbreath_Start` | `0x4DF580` | 0x111 | the pool cleared; the task at the owner, moved by the formation offset; 32 bolts (`+4` i, `+9` a delay i x 4 + 1); sound 0x100 |
| `Thunderbreath_Wait` | `0x4DF6A0` | 0x23 | once 2 or fewer bolts are left, `+0xA` frames |
| `Thunderbreath_DrawOrb` | `0x4DF6D0` | 0x146 | eight `POLY_G3` round the task, radius 0x20 + Rand & 3, grey `+0xA` x 15 at the centre |
| `Port_DroppedCall` | `0x4DF820` | 0x1 | a bare `ret`: section 2 |
| `ThunderPool_Dispatch` | `0x4DF830` | 0x12 | `jmp [ThunderPool_Kinds + 4 x +1]`: bolt, spark |
| `ThunderBolt_Task` | `0x4DF850` | 0x9E | a step through `ThunderBolt_Steps` by `+2`; the bolt drawn; every fourth frame of the flicker step, a spark at the bolt's end (unless the pool is full) |
| `ThunderBolt_Aim` | `0x4DF8F0` | 0x12D | after its delay: from the owner's screen point toward the targets' centre, turned by up to 7 x 0x40 (left or right by `+4`'s parity) |
| `ThunderBolt_Grow` | `0x4DFA20` | 0x1D | `+0xA` (its length in steps) up by 4 to 0x10 |
| `ThunderBolt_Flicker` | `0x4DFA40` | 0x43 | `+0xA` 0x10 +- 4 by the sine of `+0xB`, for 16 frames |
| `ThunderBolt_Fade` | `0x4DFA90` | 0x50 | the same, `+9` down by 2; then the owner's count down and the record freed |
| `ThunderBolt_Draw` | `0x4DFAE0` | 0x98E | `+0xA` steps of 16 along `+0x14`, bent across it by a bow (a sine over the length) times a random wobble; four `POLY_G4` a step round the segment, the inner pair shaded from `+9`, the outer fading to 1 |
| `ThunderSpark_Task` | `0x4E0470` | 0x2E | a step through `ThunderSpark_Steps` by `+2`; the fan and the ring |
| `ThunderSpark_Start` | `0x4E04A0` | 0x39 | the colour (8, 6, 0xF) in `+0x5D..+0x5F` |
| `ThunderSpark_Grow` | `0x4E04E0` | 0x2B | `+9` up by 4 to 0x10, `+0xA` up by 2 |
| `ThunderSpark_Fade` | `0x4E0510` | 0x35 | `+9` down by 4; then the owner's count down and the record freed |
| `ThunderSpark_DrawFan` | `0x4E0550` | 0x163 | eight `POLY_G3`, radius `+0xA` x 2, the colour x `+9` at the rim |
| `ThunderSpark_DrawRing` | `0x4E06C0` | 0x1E2 | eight `POLY_G4` between `+0xA` x 2 and x 3 |
| `ThunderPool_Alloc` | `0x4E08B0` | 0x57 | the first free record of the 64, in al; 0xFF when none |

## 2. `Port_DroppedCall` (`0x4DF820`)

**What it is.** One byte, `C3`, then fifteen `nop`s of padding to
`0x4DF830` (disassembly, 2026-09-26). It is the program's empty function:
every function the port compiled to nothing was folded by the linker into
this one copy, which it happened to keep inside MAGIC124's code. It belongs
to no spell; it is taken here because the extent is this group's.

**Who calls it.** The PSX called real functions at these sites; the PC
calls the empty one:

- 25 call sites with 0, 1 or 4 arguments (DIV-0011 counted them; four of
  them, the menu frames, DIV-0011 re-aims at `Menu_DrawFrame`, bytes outside
  this function);
- move-script op B8 (PSX `0x8015DCF0`), and `Sprite_UpdateScreen` /
  `Sprite_QueueOverlay` for a sprite with flag bit 8 (PSX `0x80161E44`)
  (symbols.toml, 2026-09-22);
- the battle kind-1 task table's entry 91 (`0x5B`, the one MAGIC113
  creates: `tools/magic_rows.py` lists `0x4DF820` as reached by MAGIC113 through it).
  A task with this handler does nothing each frame and never frees itself;
  what MAGIC113 does about that is group C1's to read.

**What a route reaches.** The combat route's call counts to frame 2560
(`analysis/calltrace/recipe_combat/bof3x.callcounts.tsv`) have 6 calls:
one each from `Game_Init` (`0x4FD150`), `Title_StateMenu` (`0x462406`) and
`BattleEnd_AwaitMemberTasks` (`0x431471`), and 3 with caller `0xFFFFFFFF`,
which the tracer writes for a call made from inside a function already ours
([`call-trace.md`](call-trace.md)) - title states, mode flow and sprite
screen call it by name.

**Ours.** A `ret` too, written as a naked function padded with `int3` to
the five bytes `BOF3X_ORIGINAL=Port_DroppedCall` writes its jump back over.
Its name now binds to ours in `symbols.gen.h`; the modules that listed it as
an original (`sprite_screen`, `mode_flow`, `title_states`, `move_groups`) run
their own fuzzes before this module injects and re-aim the call sites they
clone by the address, so none changes. It is on the attract and combat
paths, so the coordinator's frame-hash batch is its live check;
`entries_logic.txt` already listed `004DF820 1`.

## 3. Divergence

No ledger entry. Each function is a faithful replacement, with two kinds of
exception that follow the project's precedents:

- a phase past any of the three stack tables or seven `.data` tables aborts
  ([`magic_fx_reached.md`](magic_fx_reached.md) §3; S22's reading of the
  `.data` ones);
- where the original's `idiv` would fault, ours aborts with a message: the
  beam's half-widths divide by `+0xB`, the bolt's bow by `+0xA` (section 8).
  The same kind of case as the live-target divides the owner ruled on
  ([`takeover-queue-round9.md`](takeover-queue-round9.md) §6).

Nothing in `DIVERGENCE.md` or `cheats.cpp` patches bytes in
`0x4DD8B0..0x4E0906` (DIV-0011 re-aims four call sites elsewhere that call
`0x4DF820`).

## 4. Calls to other units, and data

By raw address, never bound (`MH_AT` / a stack-table entry):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F6290` | MAGIC219 (S37), shared by 35 overlays | tail jump of `BreathMote_Burst`, `_Free`, `ThunderBolt_Fade`, `ThunderSpark_Fade`: clears `+0..+4` of the record |
| `0x4F9F70` | MAGIC226/227 (S38) | entry 1 of `Thunderbreath_Task`'s stack table: `+9` down, at 0 the target flags 0x10 and `+1` on |

By name, already ours: `Leech_WaitOrbs` (S17), `BattleFx_Finish`,
`MagicFx_CenterOnSide` and `MagicFx_FormationOffset` (L),
`BattleActor_UpdateScreenXY`, the GPU and trigonometry library.

Named data (`symbols.toml` `[[data]]`, addresses and sizes only):
`BreathBeam_Phases` `0x65BB48` (1), `BreathBeam_Steps` `0x65BB4C` (5),
`IcePool_Kinds` `0x65BB60` (2), `BreathMote_Steps` `0x65BB68` (4),
`BreathMote_Angles` `0x65BB78` (8 bytes), `ThunderPool_Kinds` `0x65BB80`
(2), `ThunderBolt_Steps` `0x65BB88` (4), `ThunderSpark_Steps` `0x65BB98`
(3); the pools `BreathMote_Pool` `0x69ACC8` (90 records of 0x84) and
`ThunderPool` `0x69DB30` (64).

## 5. The fuzz

`BOF3X_SHADOW=magic_s28` runs `magic_harness::Run` over the 42 clones, 2,000
rounds each, with no harness edits. What the group adds
(`src/game/magic_s28_fuzz.cpp`):

- **Callees:** the trigonometry (`Math_Sin` / `_Cos` answering in the
  tables' range three times in four, `Math_Ratan2`'s two floats as bits);
  the GPU calls, a setter writing the primitive's code byte, and
  `Gfx_CommitPrim` with an effect that logs the primitive it links (the
  size's bytes at `Gfx_PacketNext`) and moves the cursor on half the time -
  every primitive of a draw is built in the same bytes, so the state alone
  would show only the last; MAGIC219's record free by address; the group's
  own functions called directly as `kPhase` recorders (they log the
  `Sprite_Current` and owner they are called with - the pool runners' only
  output); the two allocators as `kByte` answers with `ret_mask 0xFF`, the
  bolt pool's answering 0xFF a quarter of the time when `ThunderBolt_Task`
  (the one caller that tests it) is fuzzed.
- **Tables:** the seven `.data` handler tables.
- **Regions:** the scratch, `Prim_VertexScratch`, `Gfx_PacketNext`, both
  pools, CLUT row 26 and its buffer, `BreathMote_Angles`, a 0x200-byte
  primitive buffer of the fuzz's own.
- **Seed:** every pool record's owner a real slot or sprite record (the
  disturbance writes through the owner); each dispatcher's index inside its
  table; each count at and either side of its end; the mote's screen point
  at and either side of 0 and 0x140 / 0xF0, its speed small; the
  allocators' pools full, full but the last, or mixed; the draws' loop
  counts short.
- **Disturb:** the cursor, a scratch word, a vertex word, an angle byte, a
  byte of a pool record.
- **Settle:** after every disturbance, while a beam draw runs, `+0xB` of
  every slot the task may be is kept from 0 and `+0xA` below 6; while the
  bolt draw runs, `+0xA` in 1..9 - the divisors the originals re-read after
  a call, where both sides would fault.

Result in this worktree (2026-09-26):

    shadow      magic_s28 self-test: 84000 rounds over 42 functions (2000 each), 1827336 calls to the stand-ins,
                0 MISMATCHES; 32452 bytes of state (17 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'`: exit 0 (2026-09-26, every group's self-test in this worktree 0 mismatches, `sprite_screen`, `title_states` and `mode_flow` among them with `Port_DroppedCall` bound to ours).

## 6. Controls

There are 153 plants, each put in `magic_s28.cpp` by a script (not
committed) that anchors every replacement on a string found exactly once in
the file, then rebuilds, runs `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=magic_s28`, restores the file and rebuilds again. **152 are
refused**, every one by a count (exit 3), only in the functions the plant
touches; one is an equivalent mutant. Counts are rounds of 2,000, in this
worktree. B4, B5, B6, T9, Y2, J8 and Z2 were run again after the seed took
the aims' direction from the owner's `+8` (B4 and B5 had been refused in 16
to 22 rounds); the table has the second figures.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| F1 | Firebreath_Task entries swapped | Firebreath_Task 2,000 |
| F2 | Firebreath_Start: child +9 5 | Firebreath_Start 1,955 |
| F3 | Firebreath_Start: words 1..15 bit 14 | Firebreath_Start 2,000 |
| F4 | Firebreath_Start: word 0x11 zeroed, not 0x10 | Firebreath_Start 2,000 |
| F5 | Firebreath_Start: child +1 1 | Firebreath_Start 2,000 |
| F6 | Firebreath_Start: direction from the owner +9 | Firebreath_Start 1,976 |
| B1 | BreathBeam_Run: owner +0xB 0xFE | BreathBeam_Run 602 |
| B2 | BreathBeam_Run: bands swapped | BreathBeam_Run 633 |
| B3 | BreathBeam_Run: +2 1 gates | BreathBeam_Run 542 |
| B4 | direction 1: dx -40 | BreathBeam_Aim 94, Icebreath_Start 166 |
| B5 | direction 2: dy -37 | BreathBeam_Aim 108, Icebreath_Start 124 |
| B6 | offset: party below 2 | BreathBeam_Aim 284, Icebreath_Start 432 |
| B7 | Aim: Ratan2 arguments swapped | BreathBeam_Aim 1,297 |
| B8 | Aim: +0xB 0x12 | BreathBeam_Aim 1,303 |
| B9 | Aim: sound 0x101 | BreathBeam_Aim 1,314 |
| B10 | Aim: centre read after the second UpdateScreenXY | BreathBeam_Aim 61 |
| W1 | Widen: +0xA up by 2 | BreathBeam_Widen 2,000 |
| W2 | Widen: at 9 | BreathBeam_Widen 1,642 |
| H1 | Hit: flags 0x20 | BreathBeam_Hit 1,333 |
| H2 | Hit: at 0x19 | BreathBeam_Hit 1,653 |
| O1 | Hold: at 0x77 | BreathBeam_Hold 1,692 |
| D1 | Fade: +4 up | BreathBeam_Fade 1,993 |
| D2 | Fade: owner +0xB 0xFE | BreathBeam_Fade 1,316 |
| T1 | textured: v x 31 | BreathBeam_DrawTextured 1,596 |
| T2 | textured: CLUT 0x1FB | BreathBeam_DrawTextured 1,612 |
| T3 | textured: width 9i - 4 | BreathBeam_DrawTextured 384 |
| T4 | textured: side 1 u 0x41 | BreathBeam_DrawTextured 1,612 |
| T5 | textured: inner shade +4 + 1 | BreathBeam_DrawTextured 1,612 |
| T6 | beams: draw mode 0x34 | BreathBeam_DrawTextured 2,000, BreathBeam_DrawGlow 2,000 |
| T7 | textured: commit 0x50 | BreathBeam_DrawTextured 1,612 |
| T8 | beam walk: step x 2 | BreathBeam_DrawTextured 1,593, BreathBeam_DrawGlow 1,580 |
| T9 | beam walk: cosine of the kept angle | BreathBeam_DrawTextured 3, BreathBeam_DrawGlow 3 |
| T10 | beam width: dividend not a short | BreathBeam_DrawTextured 1,299, BreathBeam_DrawGlow 1,108 |
| T11 | beam vertex: y from the x base | BreathBeam_DrawTextured 1,612, BreathBeam_DrawGlow 1,596 |
| G1 | glow: width 18i - 13 | BreathBeam_DrawGlow 313 |
| G2 | glow: phase mask 0x1F | BreathBeam_DrawGlow 1,029 |
| G3 | glow: shade x 8 | BreathBeam_DrawGlow 1,594 |
| G4 | glow: inner 2 | BreathBeam_DrawGlow 1,596 |
| I1 | Icebreath_Task: closing draw mode 0x16 | Icebreath_Task 2,000 |
| I2 | pools: bit 1 tested | Icebreath_Task 2,000, Thunderbreath_Task 2,000 |
| I3 | pools: owner not put back | Icebreath_Task 1,473, Thunderbreath_Task 1,620 |
| I4 | Icebreath_Task: 89 motes run | Icebreath_Task 1,004 |
| S1 | Icebreath_Start: +1 i & 3 | Icebreath_Start 2,000 |
| S2 | Icebreath_Start: +9 + 2 | Icebreath_Start 2,000 |
| S3 | Icebreath_Start: sound 0x100 | Icebreath_Start 2,000 |
| S4 | Icebreath_Start: Ratan2 x + 1 | Icebreath_Start 2,000 |
| S5 | pools: byte +2 not cleared | Icebreath_Start 2,000, Thunderbreath_Start 2,000 |
| S6 | Icebreath_Start: +9 1 | Icebreath_Start 2,000 |
| P1 | IcePool_Dispatch: the steps table | IcePool_Dispatch 2,000 |
| M1 | BreathMote_Task: by +1 | BreathMote_Task 1,498 |
| L1 | Launch: turn & 0x7F | BreathMote_Launch 347 |
| L2 | Launch: Rand bit 1 chooses | BreathMote_Launch 539 |
| L3 | Launch: speed + 9 | BreathMote_Launch 1,325 |
| L4 | Launch: flight & 7 | BreathMote_Launch 684 |
| L5 | Launch: size 3 | BreathMote_Launch 1,334 |
| Y1 | Fly: swing << 6 | BreathMote_Fly 1,990 |
| Y2 | Fly: x through the task read after the call | BreathMote_Fly 52 |
| Y3 | Fly: grows by 1 | BreathMote_Fly 372 |
| Y4 | Fly: grows under +3 + 5 | BreathMote_Fly 274 |
| Y5 | Fly: sounds swapped | BreathMote_Fly 557 |
| Y6 | Fly: frame bit 1 | BreathMote_Fly 648 |
| Y7 | Fly: x > 0x140 | BreathMote_Fly 30 |
| Y8 | Fly: y < 0 | BreathMote_Fly 28 |
| Y9 | Fly: x < 0 | BreathMote_Fly 34 |
| Y10 | Fly: y > 0xF0 | BreathMote_Fly 37 |
| U1 | Burst: owner +0xB up | BreathMote_Burst 1,335 |
| U2 | Burst: drawn after the free | BreathMote_Burst 1,349 |
| R0 | BreathMote_Free: owner +0xB up | BreathMote_Free 1,991 |
| X1 | Hex: centre blue 0xC1 | BreathMote_DrawHex 2,000 |
| X2 | Hex: second corner j + 2 | BreathMote_DrawHex 2,000 |
| X3 | mote angles & 0x7F | BreathMote_DrawHex 1,993, BreathMote_DrawRing 1,990 |
| X4 | Hex: five triangles | BreathMote_DrawHex 2,000 |
| X5 | mote corner: cosine radius from +0 | BreathMote_DrawRing 1,999, BreathMote_DrawBurst 2,000 |
| R1 | Ring: outer +0xA x 3 | BreathMote_DrawRing 1,980 |
| R2 | Ring: inner corner at the outer radius | BreathMote_DrawRing 1,999 |
| R3 | Ring: last blue 0xC1 | BreathMote_DrawRing 2,000 |
| Q1 | Burst draw: distance + 5 | BreathMote_DrawBurst 2,000 |
| Q2 | Burst draw: third corner - 5 | BreathMote_DrawBurst 2,000 |
| Q3 | Burst draw: sixteen triangles | BreathMote_DrawBurst 2,000 |
| Q4 | Burst draw: y base from +0x2E | BreathMote_DrawBurst 2,000 |
| Q5 | Burst draw: middle 0xC1 | BreathMote_DrawBurst 2,000 |
| A1 | allocs: bits 0 and 1 set | IcePool_Alloc 881, ThunderPool_Alloc 881 |
| A2 | IcePool_Alloc: 89 records | IcePool_Alloc 261 |
| A3 | allocs: none free 0xFE | IcePool_Alloc 263, ThunderPool_Alloc 250 |
| K1 | Thunderbreath_Task entries 2/3 swapped | Thunderbreath_Task 983 |
| K2 | Thunderbreath_Task: orb before the pool | Thunderbreath_Task 2,000 |
| K3 | Thunderbreath_Task: 63 records run | Thunderbreath_Task 996 |
| N1 | Thunderbreath_Start: +9 9 | Thunderbreath_Start 1,729 |
| N2 | Thunderbreath_Start: bolt +9 + 2 | Thunderbreath_Start 2,000 |
| N3 | Thunderbreath_Start: bolt +4 i + 1 | Thunderbreath_Start 2,000 |
| N4 | Thunderbreath_Start: 31 bolts | Thunderbreath_Start 2,000 |
| N5 | Thunderbreath_Start: no formation offset | Thunderbreath_Start 2,000 |
| V1 | Wait: +0xB above 3 | Thunderbreath_Wait 369 |
| V2 | Wait: +2 on | Thunderbreath_Wait 1,085 |
| B11 | Orb: radius 0x21 + | Thunderbreath_DrawOrb 2,000 |
| B12 | Orb: grey x 14 | Thunderbreath_DrawOrb 1,994 |
| B13 | Orb: seven triangles | Thunderbreath_DrawOrb 2,000 |
| P2 | Port_DroppedCall: a byte moved | Port_DroppedCall 2,000 |
| P3 | ThunderPool_Dispatch: kinds swapped | ThunderPool_Dispatch 2,000 |
| J1 | ThunderBolt_Task: every eighth frame | ThunderBolt_Task 97 |
| J2 | ThunderBolt_Task: step 2 drops | ThunderBolt_Task 254 |
| J3 | ThunderBolt_Task: 0xFE for none | ThunderBolt_Task 44 |
| J4 | ThunderBolt_Task: spark kind 2 | ThunderBolt_Task 135 |
| J5 | ThunderBolt_Task: spark y from +8 | ThunderBolt_Task 135 |
| J6 | ThunderBolt_Task: owner +0xB down | ThunderBolt_Task 135 |
| J7 | ThunderBolt_Task: +0 1 gates | ThunderBolt_Task 376 |
| J8 | ThunderBolt_Task: owner read before the allocator | ThunderBolt_Task 2 |
| Z1 | Aim: turn sides swapped | ThunderBolt_Aim 1,130 |
| Z2 | Aim: heading through the task read after Rand | ThunderBolt_Aim 39 |
| Z3 | Aim: phase & 0x1F | ThunderBolt_Aim 666 |
| Z4 | Aim: +9 0x11 | ThunderBolt_Aim 1,338 |
| Z5 | Aim: Ratan2 y from +8 | ThunderBolt_Aim 1,338 |
| Z6 | Aim: y from the owner +0x2E | ThunderBolt_Aim 1,338 |
| g1 | bolt Grow: by 3 | ThunderBolt_Grow 2,000 |
| g2 | bolt Grow: at 0x11 | ThunderBolt_Grow 1,650 |
| f1 | Flicker: swing << 3 | ThunderBolt_Flicker 1,817 |
| f2 | Flicker: at 0xF | ThunderBolt_Flicker 1,613 |
| e1 | bolt Fade: by 1 | ThunderBolt_Fade 2,000 |
| e2 | bolt Fade: size + 0x11 | ThunderBolt_Fade 1,999 |
| d1 | bolt: width 4 + | ThunderBolt_Draw 1,984 |
| d2 | bolt: grey x 14 | ThunderBolt_Draw 1,992 |
| d3 | bolt: bend - 0x300 | ThunderBolt_Draw 2,000 |
| d4 | bolt: first y from vertex +0 | ThunderBolt_Draw 2,000 |
| d5 | bolt: new width every fourth step | ThunderBolt_Draw 1,008 |
| d6 | bolt: bow x (0x17 +) | ThunderBolt_Draw 1,658 |
| d7 | bolt: bow 0x400 / +0xA | ThunderBolt_Draw 1,998 |
| d8 | bolt: wobble & 0x3F | ThunderBolt_Draw 1,577 |
| d9 | bolt: across not a short | ThunderBolt_Draw 1,530 |
| d10 | bolt: previous heading + 0x200 | ThunderBolt_Draw 1,998 |
| d11 | bolt: quad 1 inner corners swapped | ThunderBolt_Draw 1,998 |
| d12 | bolt: outer width + 7 | ThunderBolt_Draw 1,945 |
| d13 | bolt: far corners at i + 1 | ThunderBolt_Draw 1,727 |
| d14 | bolt: turn back 0xF900 | ThunderBolt_Draw 1,998 |
| d15 | bolt: quad 2 green x 8 | ThunderBolt_Draw 1,962 |
| d16 | bolt: one step fewer | ThunderBolt_Draw 771 |
| d17 | bolt: step x 2 | ThunderBolt_Draw 1,980 |
| d18 | bolt: previous width + 1 | ThunderBolt_Draw 1,992 |
| d19 | bolt: x base the word, not the dword | **not refused**: equivalent (below) |
| d20 | bolt: x base the y word (near d19) | ThunderBolt_Draw 1,996 |
| s1 | spark task: ring before fan | ThunderSpark_Task 1,021 |
| s2 | spark task: +2 1 gates | ThunderSpark_Task 982 |
| s3 | spark start: green 7 | ThunderSpark_Start 2,000 |
| s4 | spark grow: +9 by 3 | ThunderSpark_Grow 2,000 |
| s5 | spark fade: +9 by 3 | ThunderSpark_Fade 2,000 |
| s6 | spark fade: +0xA down | ThunderSpark_Fade 1,989 |
| s7 | spark grow: +0xA by 1 | ThunderSpark_Grow 2,000 |
| n1 | spark fan: radius x 4 | ThunderSpark_DrawFan 1,993 |
| n2 | spark fan: green from +0x5D | ThunderSpark_DrawFan 1,973 |
| n3 | spark fan: grey x 14 | ThunderSpark_DrawFan 1,993 |
| q1 | spark ring: outer x 4 | ThunderSpark_DrawRing 1,991 |
| q2 | spark ring: first corner 2 | ThunderSpark_DrawRing 2,000 |
| q3 | spark ring: inner y from +0x2E | ThunderSpark_DrawRing 2,000 |
| u1 | ThunderPool_Alloc: 63 records | ThunderPool_Alloc 232 |

**Not refused, d19 (equivalent).** `ThunderBolt_Draw` adds the dword at
`Prim_VertexScratch` (both vertex words) to the bent offset and keeps the
low word; the plant adds the word alone. The low 16 bits of the two sums
are the same for every input, so no state can tell them apart. Its near
variant d20 (the other word) is refused in 1,996 rounds.

The thinnest refusals depend on the disturbance moving a cell between two
reads: T9 (3 + 3 rounds: the cosine of the kept angle, seen only when a
recorder moves scratch `+4` between the sine and the cosine), J8 (2: the
owner read before the allocator, seen only when the allocator's recorder
moves the owner), Z2 (39), Y2 (52), B10 (61).

## 7. What nothing reached

No recorded route casts any of these spells (queue §5): the fuzz is the
whole check of the three overlays. For the owner casting them (a save that
has them, or DIV-0045's cheat), what the code says to look for, as
hypotheses:

- Firebreath: a beam from the caster that widens over 8 frames, holds, and
  fades, over a half-transparent CLUT row;
- Icebreath: 90 motes streaming from the caster toward the targets, each
  bursting at the end of its flight;
- ThundrBreath: an orb at the caster, 32 bolts toward the targets' centre
  that flicker and drop sparks.

Not reached by the fuzz as the game reaches it: the pools' records run by
their task (the fuzz runs each record function on a task slot, and the pool
runner with recorders standing in for the dispatch); the shared bodies as
the other five beam overlays and six mote overlays drive them.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D92, D93, D94, D113 and D131 in [`known-defects.md`](known-defects.md).

Described, not numbered:

- **The beam divides by `+0xB`** (`BreathBeam_DrawTextured` / `_DrawGlow`,
  four `idiv` a step). MAGIC122's own steps keep it at 1 or more (0x11 at
  the aim, down 16 times in Hit, up in Fade); the five other overlays that
  drive `BreathBeam_Run` were not read for what they leave in `+0xB`. A zero
  faults (#DE); ours aborts.
- **The bolt's step counter is a byte** (`ThunderBolt_Draw`): with `+0xA`
  0xFF the loop never ends. Its own steps keep `+0xA` in 0..0x14. Its
  `0x800 / +0xA` is reached only with `+0xA` of 1 or more.
- **Every dispatcher's index is unchecked** (three stack tables, seven
  `.data` tables: `BreathBeam_Phases` index 1 runs `BreathBeam_Aim`); ours
  aborts.
- **The starts use the allocators' answers unchecked**
  (`Firebreath_Start`'s `BattleTask_Create`, `Icebreath_Start`'s 90 and
  `Thunderbreath_Start`'s 32 records). The pool ones cannot fail as the
  starts use them (each clears its pool first, and asks for 90 of 90 and 32
  of 64), but the clearing is the defect's other side: **a second cast
  while the first runs clears the first's records** - the two casts share
  one `.bss` pool. Whether two such breaths can overlap in play was not
  measured.
- **A kind-1 task `0x5B` never ends by itself** (section 2): its handler is
  the empty function.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 40 lines under a `group S28` comment: 32 new,
and eight host extents re-listed at the function's own size (`004DE370`,
`004DEB80`, `004DEE50`, `004DF450`, `004DF830`, `004DFAE0`, `004E0550`,
`004E08B0`). Two were listed right already (`004DF6D0 146`, `004DF820 1`).
`004DD4B0 EB2` (a MAGIC121 host) covers the first eleven; the consolidation
keeps the smaller.
