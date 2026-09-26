# Spell group S24: MAGIC104, MAGIC105, MAGIC106

**Status:** IN PROGRESS (2026-09-26). All 47 functions are ours
(`src/game/magic_s24.cpp`, shadow name `magic_s24`). They are fuzzed
headless through the shared harness with 0 mismatches, and every planted
control is refused, except one equivalent mutant (section 4). No recorded route casts
these spells, so they stay fuzz-only until the owner looks at them.

Group S24 of round nine ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4),
taken with the spell harness ([`magic_harness.md`](magic_harness.md)).
Three `Magic_Rows` overlays, contiguous in the band:

| Unit | Row | Extent | Functions | Bytes | Ability, read one id down |
|---|--:|---|--:|--:|---|
| MAGIC104 | 14 | `0x4D04B0..0x4D16D3` | 19 | 4,512 | Sirocco |
| MAGIC105 | 64 | `0x4D16E0..0x4D2442` | 16 | 3,310 | Kyrie |
| MAGIC106 | 104 | `0x4D2450..0x4D2D7E` | 12 | 2,246 | Death |

The ability names are hypotheses. The shift is TCRF's rule, and it breaks
at least once ([`cut-content.md`](cut-content.md) §2). Nothing in the code
depends on the names, so the functions are named `Fx104_`, `Fx105_` and
`Fx106_` by unit, plus what each one does. The descriptions below are
readings of the code. Whether they match what the spell looks like on
screen is for the owner's eye; nothing here claims what these spells do in
the game.

`tools/magic_rows.py --unit MAGIC104 / 105 / 106` gives 47 functions not yet
ours, and the reading found none missing from the extents and none extra.
Two bodies are reached from more than their own file:

- `0x4D1AA0` is reached from 15 files. It is named
  `MagicFx_EndWithChildren` because it is a generic "wait for my children,
  tell the owner, free" step.
- `0x4D18E0..0x4D1B60`, `0x4D1AC0`, `0x4D1B60` and `0x4D21C0` are reached
  from MAGIC104 and MAGIC105. MAGIC104's only way to them is through
  `.data`, where the tables run on without a gap (section 2).

## 1. What each overlay does (by reading)

**MAGIC104.** `Fx104_Task` is the kind-2 task: a four-entry stack table by
`+1`.

- **`Fx104_Start`** takes the facing from the acting actor's record
  (`[0x904B3C] +8`). It flips the facing by 2 when the target side's 0x40
  bit and "the actor is a party member" agree. It centres on the targets
  through `0x4FC0E0`, then spawns eight kind-1 children, parameter `0xB`,
  at the pair offsets of `Fx104_ChildOffsets`. Each child gets its offset
  turned by the facing (`0x446770`), and each spawn plays sound `0x102`.
- **`Fx104_WaitFirstRing`** waits until the last whirl writes its index 7
  into the task's `+0xB`.
- **`Fx104_SecondRing`** counts down `+9`, then spawns eight more children
  with `+1` 0.
- **`Fx104_End`** runs once `+0xB` is 7 again. It sets the targets' 0x40
  flag, sets the effect-done bit and frees the task.

The child `Fx104_Child` dispatches through `.data` by `+1`:

- **Whirl** (`+1` = 1): `Fx104_WhirlDelay`, `_WhirlGrow`, `_WhirlSpin` and
  `_WhirlEnd`, by `+2`. While `+2` is not 0, each frame draws
  `Fx104_PushMatrix`, then `_DrawRing` (32 POLY_G4), `_DrawDisc` (32
  POLY_G3) and `_DrawFunnel` (up to 8 rows of 16 POLY_G4). The funnel's
  colour follows `+2`: a gradient while it grows, a fade by `+0xA` after.
- **Burst** (`+1` = 0): `Fx104_BurstDelay`, then `_BurstShrink`, which
  draws `Fx104_DrawSphere` (8 x 8 POLY_G4) as it sinks. The burst's draws
  are framed by draw-mode primitives on ordering slot 4.

**MAGIC105.** `Fx105_Task` runs `Fx105_Start` or `BattleFx_Finish` by
`+1`. After that it runs and draws every live record of its own 64-record
pool, `Fx105_MotePool` (`0x695C38`, records of `0x84`). For each one it
sets `Sprite_Current` to the record and the owner to the record's `+0x80`,
then puts both back.

- **`Fx105_Start`** empties the pool and centres on the targets. It turns
  an offset of -8.0 by the owner's facing, and sets `+0xC` to
  `Math_Ratan2` back to the field's point. It then makes seven orbs (kind
  1, parameter `0x33`), sets CLUT strip row 26 semi-transparent (the same
  code as `FxDiscFan_Start`'s), and plays sound `0x100`.
- **An orb** (`Fx105_Orb`, a four-entry stack table by `+2`) does the
  following in turn:
  - `_OrbStart` takes its two angles from `Fx105_OrbAngles` plus the
    owner's `+0xC`.
  - `_OrbEmit` asks `Fx105_MoteAlloc` for a mote every fourth frame and
    counts it in `+0xB`, while `+9` rises to 32.
  - `_OrbHold` waits 64 frames.
  - `MagicFx_EndWithChildren` ends the orb.

  Each frame until then it draws `+9` stacked POLY_G4 bands
  (`Fx105_DrawOrb`), coloured from `Fx105_OrbColours` and fading by
  `+0xA`.
- **A mote** runs `Fx105_Mote`, then `Fx105_MoteRun`. The run switches
  the sprite bank `0x9039D8` to `0x8E3580` while it runs:
  - `_MoteStart` places the mote round the orb and finds the ground under
    it. It sets the mote up as a sprite (texture `0x1D`, CLUT `0xA0`,
    semi-transparent) with animation 1 or 2, chosen by `Rand`.
  - `_MoteDrift` makes it wobble and sink. When its script ends, or it
    falls below the ground, it tells the orb and frees itself.
  - `Fx105_MoteDraw` draws the mote's frame as a list of SPRTs.

**MAGIC106.** `Fx106_Task` runs `Fx106_Start` or `BattleFx_Finish` by
`+1`. It then runs every live spark of `Fx106_SparkPool` (`0x697D38`, 96
records of `0x20`). For each one, `Fx106_SparkCurrent` (`0x698938`) is set
to the spark and the owner to the spark's `+0x1C`.

- **`Fx106_Start`** takes the point from the source sprite, raised by
  `0x1000000`. It allocates 96 sparks, each with a delay (`Rand` and its
  number), and plays sounds `0x100` and `0x101`.
- **A spark** (`Fx106_SparkRun`, by `+2`) does the following in turn:
  - `_SparkDelay` places the spark on a circle round the owner and picks
    colours with `Rand`.
  - `_SparkConverge` closes the circle in. Spark 0 sets the target's 0x10
    flag as it arrives.
  - `_SparkSpin` spins it while the colours shift.
  - `_SparkRise` rises and dims it on odd frames, then frees it
    (`Fx106_SparkFree`).

  Each spark is drawn as four POLY_G3 shards (`Fx106_DrawSpark`).

## 2. Tables, dispatch, and what aborts

**Stack tables** (checked immediates):

- `Fx104_Task`: 4 entries by `+1`;
- `Fx105_Task` and `Fx106_Task`: 2 entries by `+1`;
- `Fx105_Orb`: 4 entries by `+2`.

**`.data` tables**, read in place. `symbols.toml` names them all; they run
end to end with no gap between them:

- `0x65B868`: `Fx104_ChildPhases` (2), `_WhirlPhases` (4), `_BurstPhases`
  (2), then `Fx105_ChildPhases` (1);
- `0x65B8B4`: `Fx105_MotePhases` (1), then `_MoteRunPhases` (2);
- `0x65B8E0`: `Fx106_SparkPhases` (1), then `_SparkRunPhases` (4).

The sizes are the entries each index's writers can reach. `magic_rows.py`
counts each run as one table (9, 3 and 5 entries), and the fuzz swaps each
run as one. Past its own table, each of the originals' dispatchers calls
the next table's entries or data. Ours aborts there, as every dispatcher
the project has taken does. That is not a divergence: the index is past
anything the game writes.

**Other `.data`** named: `Fx104_ChildOffsets`, `Fx105_OrbAngles`,
`Fx105_OrbColours`, `Fx105_MoteSizes` and `Fx106_ShardTables`. The two
pools and `Fx106_SparkCurrent` are named too.

No divergence and no ledger entry. Each function is a faithful
replacement, including every memory re-read after a call: `Sprite_Current`,
the owner, `Fx106_SparkCurrent`, the scratch words `0x903850..` and the
vertices `0x9037A0..`. DIVERGENCE.md and `cheats.cpp` patch nothing in
`0x4D04B0..0x4D2D7E` (grep, 2026-09-25).

## 3. Calls across groups

Both of these go through raw addresses in `magic_s24_callees.h`, re-aimed
at recorders in the fuzz, and neither is bound here:

- **`0x4FC0E0`** (group L's, the effect library). It centres the effect on
  the targeted side: the mean `+0x34` / `+0x38` / `+0x3E` of every actor of
  the side, by the 0x40 bit of `0x904B44`, that is not out. The result
  goes into `Sprite_Current`. It divides by the count unchecked, so a side
  with no one standing divides by 0. That is by reading, and the defect is
  L's to describe.
- **`0x446770`** (engine code outside the band, in no group). It turns a
  task's `+0xC` / `+0x10` by its facing `+8`, in quarter turns.

Everything else is already ours and is called by name: the GTE and GPU
helpers, `Math_Sin` / `_Cos` / `_Ratan2`, `MapView_LinkPrimAt`,
`Gfx_CommitPrim`, `AreaMap_Elevation`, `Sprite_SetAnimation`,
`BattleActor_UpdateScreenXY`, `BattleFx_Finish` and the standard set.

## 4. The fuzz, and the controls

`BOF3X_SHADOW=magic_s24` (`magic_s24_fuzz.cpp`) runs through
`magic_harness::Run`: 47 clones and 2,000 rounds a function.

**Callees.** The group lists 39 callees beyond the standard set:

- **The GTE and GPU helpers.** A GTE input the caller builds in its own
  frame is logged by its bytes (`deref`). These are the vector and angles
  of `Gte_RotTrans` and `Gte_RotMatrix`, and the vertices of
  `Gte_RotTransPers3` / `4`. Output matrices are masked.
- **The group's own functions called directly** (`Answer::kPhase`). These
  are logged as the task they run for; the spark phases also log
  `Fx106_SparkCurrent`.
- **The two allocators.** `Fx105_MoteAlloc` answers `0xFF..0x3F`, so the
  "none left" path is reached. `Fx106_SparkAlloc` answers `0..0x5F`,
  because its caller does not test for 0xFF (section 6).

**Regions** beyond the standard ones:

- the vertex and scratch words;
- `Field_Kind2X` / `Z`;
- `Gfx_PacketNext` and a 256-byte primitive buffer;
- a frame buffer for `Fx105_MoteDraw`;
- the sprite bank;
- the CLUT row and its source;
- both pools and `Fx106_SparkCurrent`;
- the named `.data` tables, which are randomised and then put back.

**Primitives.** Every `MapView_LinkPrimAt` and `Gfx_CommitPrim` also logs
the 0x40 bytes at `Gfx_PacketNext` through an `effect`. The draws write
each primitive into the same bytes, so without this the regions would
compare only the last one.

**`phase_span = 2`.** `Fx104_Burst` dispatches by `+2` after two calls, so
the disturbance keeps the phase bytes at 0 or 1.

**The seed** puts every pointer the functions dereference back inside:

- `Gfx_PacketNext`, `0x904B3C` and `Fx106_SparkCurrent`;
- the pools' owners;
- each task's frame pointer `+0x54` / `+0x5A`.

It also puts each compare on its boundary or one either side: the
countdowns, `+9` at 8 / 16 / 24 / 32, the screen bounds of the mote draw,
the converge limit, the spin caps, the frame parity, the full and
one-free pools, and the target's 0x40 bit. The group's `disturb` moves the
packet pointer, the current spark, a scratch or vertex byte, or a byte of
the current spark.

**Result in this worktree** (2026-09-26, after the port onto the folded
harness `ea27991`):

    shadow      magic_s24 self-test: 94000 rounds over 47 functions (2000 each), 8118126 calls to the stand-ins,
                0 MISMATCHES; 23696 bytes of state (24 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'` gave exit 0. Call counts depend on the build directory
(the harness's records live in our DLL); see HANDOFF.

**Controls.** Each was planted alone by a script (not committed: apply,
build, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s24`, restore).
80 controls were planted. 79 are refused by a count (exit 3), each only in the function or functions it touches. The one not refused, F3 (the funnel's gradient switching at 0x900 instead of 0x800), is an equivalent mutant, not a gap. The only angle between the two thresholds is 0x800, at column 8, where both branches write the same bytes: (8 + 5) << 4 and 0x50 - (8 << 4) are both 0xD0, and (8 + 6) << 4 and 0x60 - (8 << 4) are both 0xE0. F3b moves the threshold to 0x700 instead, and it is refused. The thinnest controls are J3 (6 rounds: a mirrored part needs part bit 7, +0x2A and the screen bounds together), L2 (7: a pool with only the 96th record free) and O4 (16: the allocator's 0xFF answer).

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Fx104_Task: entries 0 and 1 swapped | Fx104_Task 978 |
| S1 | Fx104_Start: facing flip on disagreement | Fx104_Start 1,791 |
| S2 | Fx104_Start: sound 0x103 | Fx104_Start 2,000 |
| S3 | Fx104_Start: spin i not 2i | Fx104_Start 2,000 |
| C1 | ring child: +0x3C from +0x38 | Fx104_Start 2,000, Fx104_SecondRing 345 |
| C2 | ring child: offset pair z from x | Fx104_Start 2,000, Fx104_SecondRing 345 |
| C3 | ring child: turned before the offset is stored | Fx104_Start 383, Fx104_SecondRing 71 |
| W1 | Fx104_WaitFirstRing: waits for 6 | Fx104_WaitFirstRing 681 |
| R1 | Fx104_SecondRing: delay 16i | Fx104_SecondRing 345 |
| E1 | Fx104_End: done bit 8 | Fx104_End 252 |
| D1 | Fx104_WhirlDelay: odd/even sounds swapped | Fx104_WhirlDelay 346 |
| G1 | Fx104_WhirlGrow: at 9 | Fx104_WhirlGrow 675 |
| P1 | Fx104_WhirlSpin: at 0x11 | Fx104_WhirlSpin 700 |
| N1 | Fx104_WhirlEnd: owner +0xB not set | Fx104_WhirlEnd 272 |
| H1 | Fx104_Whirl: draws on +2 == 0 too | Fx104_Whirl 264 |
| F1 | Funnel: radius 31 | Fx104_DrawFunnel 781 |
| F2 | Funnel: rows rounded down (>> 1) | Fx104_DrawFunnel 136 |
| F4 | Funnel: case 2 fade *11 | Fx104_DrawFunnel 1,195 |
| F5 | Funnel: draw mode 0xB4 | Fx104_DrawFunnel 1,649 |
| M1 | Fx104_PushMatrix: angle mask 7 | Fx104_PushMatrix 490 |
| M2 | PointOf: height not halved | Fx104_PushMatrix 2,000, Fx105_PushMatrix 2,000, Fx106_PushMatrix 2,000 |
| K1 | Disc: colour 0x3F | Fx104_DrawDisc 2,000 |
| K2 | Disc: rim >> 8 | Fx104_DrawDisc 1,995 |
| K3 | Disc: closing mode on slot 4 | Fx104_DrawDisc 2,000 |
| G2 | Ring: outer colour above 9 | Fx104_DrawRing 339 |
| G3 | Ring: inner radius *33 | Fx104_DrawRing 1,989 |
| B1 | Fx104_Burst: second mode on slot 5 | Fx104_Burst 2,000 |
| B2 | BurstDelay: height +0x100 | Fx104_BurstDelay 1,000 |
| B3 | BurstShrink: height -0x20 | Fx104_BurstShrink 2,000 |
| Y1 | Sphere: colour +9 * 16 + 2 | Fx104_DrawSphere 2,000 |
| Y2 | Sphere: latitude to 0xB00 | Fx104_DrawSphere 2,000 |
| T2 | Fx105_Task: mote owner not set | Fx105_Task 2,000 |
| A1 | Fx105_Start: offset -7.0 | Fx105_Start 2,000 |
| A2 | Fx105_Start: Ratan2 arguments swapped | Fx105_Start 2,000 |
| A3 | Fx105_Start: CLUT bit 14 | Fx105_Start 2,000 |
| A4 | Fx105_Start: six orbs | Fx105_Start 2,000 |
| O1 | Fx105_Orb: draws on +2 == 3 | Fx105_Orb 232 |
| O2 | OrbStart: +0x10 from the first word | Fx105_OrbStart 1,968 |
| O3 | OrbEmit: every other frame | Fx105_OrbEmit 246 |
| O4 | OrbEmit: 0xFF not tested | Fx105_OrbEmit 16 |
| O5 | OrbHold: at 0x3F | Fx105_OrbHold 379 |
| X1 | EndWithChildren: owner not decremented | MagicFx_EndWithChildren 1,010 |
| V1 | DrawOrb: colour c >> 5 | Fx105_DrawOrb 1,987 |
| V2 | DrawOrb: band radius << 5 | Fx105_DrawOrb 1,975 |
| V3 | DrawOrb: no band for +9 1 | Fx105_DrawOrb 509 |
| U1 | MoteRun: bank not put back | Fx105_MoteRun 2,000 |
| Q1 | MoteStart: radius >> 4 | Fx105_MoteStart 2,000 |
| Q2 | MoteStart: ground not sign-extended | Fx105_MoteStart 989 |
| Q3 | MoteStart: animation (r & 1) | Fx105_MoteStart 2,000 |
| Q4 | MoteStart: +0xA & 0x1F | Fx105_MoteStart 1,006 |
| Z1 | MoteDrift: > not >= | Fx105_MoteDrift 101 |
| Z2 | MoteDrift: no sign-extension | Fx105_MoteDrift 1,741 |
| L1 | MoteAlloc: not marked | Fx105_MoteAlloc 1,517 |
| J1 | MoteDraw: x bound 0x17F | Fx105_MoteDraw 120 |
| J2 | MoteDraw: CLUT row 0x1E1 | Fx105_MoteDraw 390 |
| J3 | MoteDraw: mirror ignores +0x2A | Fx105_MoteDraw 6 |
| J4 | MoteDraw: semi bit 4 | Fx105_MoteDraw 476 |
| J5 | MoteDraw: tint + 0x7F | Fx105_MoteDraw 765 |
| I1 | Fx106_Task: owner put back before the call | Fx106_Task 2,000 |
| I2 | Fx106_Start: height +0x800000 | Fx106_Start 2,000 |
| I3 | Fx106_Start: delay groups of 64 | Fx106_Start 2,000 |
| I4 | Fx106_Start: +8 0x3F - delay | Fx106_Start 2,000 |
| I5 | SparkRun: draws on +2 == 0 | Fx106_SparkRun 242 |
| I6 | SparkDelay: radius +5 | Fx106_SparkDelay 328 |
| I7 | SparkDelay: colour & 3 | Fx106_SparkDelay 278 |
| I8 | Converge: limit +0x21 | Fx106_SparkConverge 319 |
| I9 | Converge: flag for every spark | Fx106_SparkConverge 856 |
| I10 | Converge: radius -1 | Fx106_SparkConverge 2,000 |
| I11 | Spin: +5 up to 0xD | Fx106_SparkSpin 341 |
| I12 | Spin: +3 up to 8 | Fx106_SparkSpin 331 |
| I13 | SparkTurn: & 0x7FF | Fx106_SparkSpin 1,021, Fx106_SparkRise 974 |
| I14 | Rise: even frames | Fx106_SparkRise 2,000 |
| I15 | Fx106_PushMatrix: +3 * 0x40 | Fx106_PushMatrix 1,970 |
| I16 | DrawSpark: colours in order | Fx106_DrawSpark 1,962 |
| I17 | DrawSpark: tilt & 7 | Fx106_DrawSpark 1,882 |
| I18 | DrawSpark: height << 4 | Fx106_DrawSpark 2,000 |
| L2 | SparkAlloc: pool of 95 | Fx106_SparkAlloc 7 |
| L3 | SparkFree: +4 kept | Fx106_SparkFree 1,992 |
| F3b | Funnel: gradient step at 0x700 | Fx104_DrawFunnel 1,461 |

## 5. What nothing reached

Every function here is fuzz-only. The combat route casts none of these
spells (the queue's §5), and neither do the shop or world-map routes.
The owner's live check is to cast rows 14, 64 and 104, with DIV-0045's
cheat putting the abilities in a list. What to look for:

- **MAGIC104:** two rings of whirls round the targets, then the targets'
  reaction.
- **MAGIC105:** seven orbs shedding sprite motes.
- **MAGIC106:** a closing, spinning ring of sparks.

The fuzz does not reach two things:

- `Fx105_MoteDraw`'s real frame data. Its seed is random bytes.
- The real GTE results, since recorders answer garbage. What the
  projections draw is the owner's eye, not the fuzz.

## 6. Latent defects (Capcom's, kept; described, not numbered)

- **Unchecked dispatch indices.** Every stack table and `.data` dispatch
  here is unchecked, and past its table the original calls the next
  table's entries or data (section 2). Ours aborts.
- **`Fx106_Start` can write past the spark pool.** It never tests
  `Fx106_SparkAlloc`'s 0xFF "none". A full pool would write the new spark
  `0x1FE0` bytes past the pool's start, beyond its `0xC00` bytes. It cannot
  happen as written, because the start empties the pool before it
  allocates exactly 96.
- **Unbounded orb index.** `Fx105_OrbStart` and `Fx105_DrawOrb` index
  `Fx105_OrbAngles` / `_OrbColours` by the orb index `+4` without a bound.
  `Fx105_Start` only writes 0..6.
- **`Fx105_ChildPhases` has one entry.** A `+1` of 1 would jump to the
  words of `Fx105_OrbAngles`, but nothing steps it.
- **`Fx105_OrbEmit` can overcount.** It counts a mote in `+0xB` only when
  one was allocated, and each mote counts itself down in `_MoteDrift`. An
  orb whose motes outlive it waits in `MagicFx_EndWithChildren` until they
  are gone. That is by design; noted only because the count is a byte.
- **Whirl 0 can outlive its parent** (a defect candidate, by reading and
  not measured). `Fx104_WhirlEnd` and `Fx104_BurstShrink` signal by index,
  not by count: each writes its own index into the parent's `+0xB`, and
  the parent moves on at 7.
  - In the first ring, whirl i's delay `+0xA` starts at 2i. Whirl 0's
    starts at 0, and `Fx104_WhirlDelay` decrements before it tests, so it
    wraps to 255 frames. Whirl 7 therefore ends first, at about 14 + 24
    frames, and the parent goes on to the second ring.
  - Burst 7 (delay 113 frames) then ends the parent at about 170 frames.
  - Whirl 0 is still drawing until about 280 frames. When it ends it writes
    its `+0xB` through its owner pointer, into a task slot that has been
    freed and may be reused by then.

  On screen this would be a lone late whirl, which is a guess. The live
  check is a cast of row 14 with a watch on the task slots.

## 7. For `analysis/calltrace/entries_logic.txt`

39 lines were appended to the main checkout's copy under a `group S24`
comment. The other eight were already listed right. Six earlier lines were
host extents and are now listed again with the smaller extent:
`004D0FF0`, `004D1370`, `004D1F30`, `004D2220`, `004D25C0` and `004D2D00`.
