# Drain's and the buffs' overlays: round nine group S18

**Status:** IN PROGRESS (2026-09-26) - 42 functions ours
(`src/game/magic_s18.cpp`, shadow name `magic_s18`), fuzzed headless
through the spell harness ([`magic_harness.md`](magic_harness.md)), 0
mismatches in 84,000 rounds; 121 negative controls, every one refused by a count (exit 3) in the function it touches. No recorded route casts
either overlay: fuzz only until the owner's eye.

Group S18 of the spell round ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)
section 4): two overlays of `Magic_Rows`, whole.

| Unit | Row | File | Ability ids | Read one id down | Extent | Functions |
|---|--:|---|---|---|---|--:|
| MAGIC079 | 52 | `0x15F` | `0x4F`, `0xB7` | Drain | `0x4BEB50..0x4BF8C3` | 25 |
| MAGIC082 | 32 | `0x162` | `0x22` `0x23` `0x52` `0x54` `0x55` `0xB8` `0xBA` `0xBB` | Steroids, Magic Belt, Protect, Speed, Might | `0x4C01F0..0x4C1461` | 17 |

The ids are the sibling's `names/magic.toml`; the names are TCRF's labels
read one id down ([`cut-content.md`](cut-content.md) section 2), so they
are hypotheses. One reading here supports MAGIC082's: `Buff_Kind`
(section 3) sorts exactly its eight ids into three pairs 0x66 apart - 0x52
with 0xB8, 0x54 with 0xBA, 0x55 with 0xBB (and 0x22, 0x23 with the third) -
which is what "Protect", "Speed" and "Might" each loaded by a party id and an
enemy id would look like. That the effect is a stat buff is a **guess** from
the names; nothing here reads a stat. The `Buff` prefix carries that guess.

The functions found are exactly the extents' (`tools/magic_rows.py`): 25
and 17, none missing, none found inside another's body. The extents end at
`0x4BF8C3` (padding, then MAGIC081's entry `0x4BF8D0`, group C2's) and at
`0x4C1461` (then MAGIC083's first function, group S19's).

## 1. MAGIC079: a task and four children

**The task** `Drain_Task` (`0x4BEB50`, row 52): a two-entry stack table by
`+1` - `Drain_Start`, then round eight's `BattleFx_Finish` (`0x4F7350`,
which waits for `+0xB` 0, sets the target's flag 0x40 and the done bit,
and frees).

`Drain_Start` creates four kind-1 tasks with parameter `0x2B`
(`DrainOrb_Dispatch`), gives them `+1` = 0..3 (their type) and `+9` =
0x10, counts them in its own `+0xB`, plays sound `0x100`, and steps on.
Each child's type picks a `.data` phase table (`DrainOrb_Types`,
`0x65B240`) and a draw:

| Type | Task | Phases (`.data`) | Draw | Does, by the counters |
|--:|---|---|---|---|
| 0 | `DrainOrbA_Task` | `DrainOrbA_Phases` `0x65B250` (5) | `DrainOrb_Draw` | at the **target** (`0x904B4C`'s object) less 0x4000 in x and z, 0x400 up; drops 0x40 a frame for 16 frames; `+0xA` 0xB0 down to 0x30, then to 0x10, counting the owner's `+0xB` down there; `+0xB` (the radius base) down to 0, freed |
| 1 | `DrainOrbB_Task` | `DrainOrbB_Phases` `0x65B264` (5) | `DrainOrb_Draw` | waits 16 frames, then the same at the **actor** (`0x904B3C`'s object), `+0xA` counting 0 up to 0x80, then 0xA0 (the owner's count down), then shrinks |
| 2 | `DrainOrbC_Task` | `DrainOrbC_Phases` `0x65B2F8` (4) | `DrainOrb_DrawDisc` | at the target, `+9` 4; phase 1 is `0x4C2D10` (MAGIC086's: `+9` up to 0x10); **waits until the owner's `+0xB` equals its `+1` (2)**; `+9` down to 4, the owner's count down, freed |
| 3 | `DrainOrbD_Task` | `DrainOrbD_Phases` `0x65B308` (4) | `DrainOrb_DrawDisc` | waits 16 frames, then at the actor; the same, waiting for the count to equal 3 |

The four task bodies are one shape (`OrbTask` in ours): the phase through
its table by `+2`, then - while the slot is live (`+0`) and past phase 0 -
the draw between `MagicFx_PushActorMatrix` and `Gte_PopMatrix`.

That the target-to-actor pair is a drain's "from the enemy to the caster"
is a reading of which object each type starts at, not something seen.

## 2. The two Drain draws

**`DrainOrb_Draw`** (`0x4BEF70`, 0x5AC bytes, types 0 and 1): a column of
24 rings of eight Gouraud quads (`POLY_G4`, tpage 0x15), built in the
scratch words `0x903850..0x90385F` (the PSX scratchpad's, `DamageScratch`)
and `Prim_VertexScratch`:

- ring *n* (1..24) sits at height -40 *n*, offset in x by
  (sin((`Frame_Counter` + *n*) & 63 << 6) * 3 *n*) >> 12 - a sway that
  grows up the column;
- its radius is `+0xB` * 2 plus sin(*i* << 8) * 4 >> 12, *i* = (`+0xA` + *n*)
  & 15; in phases 2 and 3 the factor is 7 + 3 (*n* - 1) for *i* below 8;
- its colour is `DrainOrb_ShadeA[i]` (phases 2 and 3) or `DrainOrb_ShadeB[i]`,
  each corner (c, c / 3, c / 2) - a red-dominant ramp by construction;
- in phases 2 and 3 a ring beyond `+9` (or short of it - which, by `+1` and
  the phase) collapses to the base radius with ShadeB: the column grows or
  shrinks ring by ring as `+9` moves;
- each quad joins ring *n* - 1 to ring *n* (`Gte_RotTransPers4`,
  `Gte_PrimDepths4_10B`, `MapView_LinkPrimAt(x, z, 0, 0x44)`).

About 1,700 calls a frame per orb. The two offsets live in two stack words
of which only the low halves reach a vertex (ours keeps 16 bits).

**`DrainOrb_DrawDisc`** (`0x4BF720`, types 2 and 3): a flat fan of eight
semi-transparent `POLY_G3` (tpage 0x55), radius `+9` * 16, the centre's
red `DrainOrb_DiscShade[+0xA & 15]` with green and blue 0xF0, the rim 1 1 1.

## 3. MAGIC082: a task, a ring and four spikes

**The task** `Buff_Task` (`0x4C01F0`, row 32): a six-entry stack table by
`+1`:

1. `Buff_Start` - the target's object's `+8` and position to the task;
   `+4` = `Buff_Kind()`; `+9` 0x10; five kind-1 children (parameter 5,
   `BuffFx_Dispatch`): one ring (`+1` 0) and four spikes (`+1` 1, `+0xB`
   0 / 8 / 0x10 / 0x18, `+9` that + 1, `+0xA` the task's `+9`); a CLUT
   strip copy (16 + 16 words `0x80E980` to `0x812980`,
   `Gfx_ClutStripDirty` 1); sound `0x100`.
2. `BattleFx_TintActor`, 3. `BattleFx_Brighten` (round eight's, MAGIC062's:
   a tint record taken into `+0xA`, brightened 8 frames).
4. `Buff_WaitChildren` - until one child or none is left, then `+9` 8.
5. `Buff_Fade` - the tint record's three colour bytes down one a frame for
   8 frames; then `Sprite_ReleaseTint` on the target's object,
   `BattleActor_Flash(target)`, and a last child (kind 1, parameter 0x48)
   whose `+4` is the kind when `0x4FB6F0(0x65C39C[kind], target)` answers
   non-zero, else 8.
6. `0x4E5200` (MAGIC131's) - the done bit and free once `+0xB` is 0.

**`Buff_Kind`** (`0x4C04F0`) - the kind 0..3 (colours and the last child's
type). Command kind 4 (`0x904B35`, an ability): by the id `0x904B80`, 0x52
and 0xB8 give 0, 0x54 and 0xBA 1, 0x22 0x23 0x55 0xBB 2, anything else 3.
Another command kind: 0x10B becomes 0x52 (and 0), 0x211 becomes 0x54 (1),
0x14 and 0x117 become 0x55 (2), else 3. The original is a jump table of
four cases indexed by a byte table over 0x22..0xBB; ours is a switch (no
table copied). What command kinds other than 4 carry these ids (items, by
the ids' size) is a **guess**.

**The ring** (`BuffRing_Task`, phases `BuffRing_Phases` `0x65B3E0`:
`0x4C2D10` MAGIC086's, `BuffRing_Wait` (until the owner's count is 2 or
less), `0x4B1740` MAGIC060's (`+9` down, the owner's count down, freed)),
draws every frame while live: `BuffRing_DrawBand` (sixteen semi-transparent
`POLY_G4`, radius 0x80 to 0x100, the inner edge `BuffRing_BandColors[kind]`
times `+9`) then `BuffRing_DrawDisc` (sixteen `POLY_G3` to radius 0x80, the
centre `+9` * 5 grey, the rim `BuffRing_DiscColors[kind]` times `+9`).

**The spikes** (`BuffSpike_Dispatch`, `BuffSpike_Phases` `0x65B3EC`, six:
`0x4BDD20` MAGIC078's, `0x4C1EB0` MAGIC083's, then ours):

| Phase | Function | Does |
|--:|---|---|
| 2 | `BuffSpike_Arc` | a speed `+0x14` with acceleration `+0x20` into the height word `+0x3E`, `+9` (the spike's angle) up on odd frames; after `+0xA` frames, speed -16, acceleration 4, 8 frames |
| 3 | `BuffSpike_Arc2` | the same flight; then `+0xA` = 0x1C - `+0xB` |
| 4 | `BuffSpike_Spin` | the spin `+0xC` up one every eighth frame of `+0x10` to 4, `+9` plus the spin; at spin 4 `+0xA` frames, sound `0x101` for the first spike; the disc wider (0x40) for the last 4 frames |
| 5 | `BuffSpike_Orbit` | the spike at the owner's x / z plus sin / cos((`+0xB` & 31) << 7) times (`+0xA` * `+0x10` + 0xB0) >> 3, `+0xB` up 2 a frame; after 16 frames the owner's count down and freed |

Each phase first draws the actor's screen point (`BattleActor_UpdateScreenXY`),
a jittered disc on screen (`MagicFx_DrawDiscRadius`, radius 0x18 / 0x20 /
0x40 + `Rand() & 3`) and the spike itself under the actor matrix
(`BuffSpike_Draw`: two rows of four flat triangles, apex +0x50 then -0x50,
radius 0x30, coloured by `BuffSpike_ColorsUp` / `_ColorsDown` by kind; each
row's depths to `0x4FB880`).

`MagicFx_DrawDiscRadius` (`0x4C12F0`) is reached by MAGIC083 too: round
seven's `MagicFx_DrawDisc` (`0x4C54F0`) shape with the radius an argument
(its low word) and the centre grey 0x80.

## 4. What is not a faithful copy, and the ledger

No divergence and no `DIVERGENCE.md` entry. Each function is a faithful
replacement, except where the original would run off a table:

- the two stack tables (`Drain_Task` 2, `Buff_Task` 6) and the eight `.data`
  tables are indexed unchecked by the originals; ours aborts past each (the
  precedent: [`magic_fx_reached.md`](magic_fx_reached.md) section 3);
- `BuffSpike_Draw` stores each triangle's depth into a four-entry array on
  its stack by (*a* - `+9`) / 8 with `+9` **read again** after the call; in
  the game nothing between the reads moves `+9`, so the index is 0..3; ours
  aborts if it is not (the original would write outside its frame).

The GTE calls push one pointer more than `symbols.toml`'s prototypes
(`Gte_RotTransPers3` / `_4` / `Gte_RotAverage3`'s trailing flag word); ours
passes it too, to a local, as the originals do to theirs.

## 5. Calls into other groups' units (raw addresses)

| Address | Unit (owner) | Called by | How ours calls it |
|---|---|---|---|
| `0x4FB880` | LIBRARY (group L) | `BuffSpike_Draw`, twice | `MH_AT`: (x, z, depths[4], first packet, 4, 0x34, 2) - a depth-sorted link of a row of packets, by reading its arguments; not read further |
| `0x4FB6F0` | LIBRARY (group L) | `Buff_Fade` | `MH_AT`: (byte, target) answering a byte tested for 0 |
| `0x4E5200` | MAGIC131 | `Buff_Task` entry 5 | `Phase(0x4E5200)` |
| `0x4C2D10` | MAGIC086 (S19) | `DrainOrbC_Phases` / `_D_` entry 1, `BuffRing_Phases` entry 0 | the `.data` cell |
| `0x4B1740` | MAGIC060 (S12) | `BuffRing_Phases` entry 2 | the `.data` cell |
| `0x4BDD20` | MAGIC078 (S17) | `BuffSpike_Phases` entry 0 | the `.data` cell |
| `0x4C1EB0` | MAGIC083 (S19) | `BuffSpike_Phases` entry 1 | the `.data` cell |

The byte table `0x65C39C` `Buff_Fade` indexes by kind lies in the library's
`.data` (4 bytes below `FxDim_Phases`); it is read, not named here.
Everything else ours calls is ours already, through its name.

## 6. The fuzz, and the harness changes it needed

`BOF3X_SHADOW=magic_s18` (`magic_s18_fuzz.cpp`): 42 clones, 22 callees
beyond the standard set (the draw primitives, the GTE, `0x4FB880`,
`0x4FB6F0`, and the group's own draws and `Buff_Kind`), the eight `.data`
tables, eight regions (the packet cursor `Gfx_PacketNext` and a 1 KB packet
buffer of the fuzz's own, the scratch words, the vertex scratch, the action
id, the 256 tint records, the CLUT strip's two halves). The seed puts each
function's thresholds in (a counter one either side of its end, the phase
within its table, the owner's count at the type, the command kind and the
listed ids for `Buff_Kind`, the spin's signed compare at -1 and 0x80000000);
the group's disturbance moves the packet cursor, the actor's object, the
action id and the command kind. 2,000 rounds a function.

Result (2026-09-25):

    shadow      magic_s18 self-test: 84000 rounds over 42 functions (2000 each), 4410238 calls to the stand-ins,
                0 MISMATCHES; 15640 bytes of state (16 regions) and the stand-ins' log compared

(Counts in this worktree; the harness's own pointers make them depend on
the build directory.) `BOF3X_SHADOW='*'`: exit 0; `magic_steal` 0
mismatches.

**What the harness gives it** (the consolidated harness, group HX,
[`magic_harness.md`](magic_harness.md) section 7; this group's first
version added these to the harness and was ported onto HX's):

- ten logged arguments (`Callee::masks`): the GTE calls take eight to ten,
  `Gpu_SetDrawMode` five, `0x4FB880` seven;
- `Callee::deref` byte counts: `0x4FB880`'s depth array (16 bytes, on the
  caller's stack, so its pointer differs between the passes while its
  contents must not), and the GTE calls' vertex arguments (8 bytes each: the
  vertex scratch is rewritten every quad, so only its contents at each call
  show a vertex computed wrong - controls W3 and W13 needed it);
- `Clone::calm` for `BuffSpike_Draw`: no recorder disturbs anything while it
  runs - its stack index would otherwise send the original's store out of
  its frame (section 4); its callees touch no battle state, so the game never
  moves anything there either;
- `Clone::ret_mask` 0xFF for `Buff_Kind`'s answer;
- a log long enough for `DrainOrb_Draw`'s ~1,700 calls a round, compared by
  prefix;
- the target-record disturbance kept to targets 3..10. This group met the
  reason first: a target of 0..2 put the "enemy" record over the
  current-slot and owner cells `0x93B8C0..0x93B95F`, and the next
  disturbance wrote through a garbage owner - a crash in
  `DrainOrb_Draw`'s tenth round.

## 7. The controls

121 controls, planted one at a time by a script (not committed: apply, build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s18`, restore; no plant is left in the source). Every one is refused by a count (exit 3), each only in the function it touches - the shared bodies (`OrbTask`, `SpikeFly`) in each function that has them. Counts are this worktree's; R1 and S7..S12 were re-run after the seed gave `BuffSpike_Spin` a second spike (`+0xB` 1) and `BuffFx_Dispatch` a bounded `+2`: before that, S10 went unrefused (`+0xB` 1 was seeded 1 time in 256) and R1 was refused only by our own abort past the two-entry table, not by a count.

Before the port onto the consolidated harness, W3 and W13 (a vertex offset wrong) went unrefused: the vertex scratch is rewritten every quad, so only its last contents were compared. The GTE calls now log their vertex arguments by their eight bytes (`deref`), and both are refused in every round.

The thinnest: S11 (1 round: `Sprite_Current` not read again after the one sound call - it needs a recorder to move the slot across that call), K3 (4: `0x211` under another command kind), W11 (17: a ring collapsing at `+9` equal to *n*), K1 / K2 / K4 (one id each of the seeded list).

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| D1 | Drain_Task: entries swapped | Drain_Task 2000 |
| D2 | Drain_Start: child +1 = i + 1 | Drain_Start 2000 |
| D3 | Drain_Start: child +9 0x11 | Drain_Start 2000 |
| D4 | Drain_Start: sound 0x101 | Drain_Start 2000 |
| D5 | Drain_Start: parameter 0x2C | Drain_Start 2000 |
| D6 | Drain_Start: Sprite_Current read before the create | Drain_Start 218 |
| O1 | DrainOrb_Dispatch: by +2 | DrainOrb_Dispatch 1486 |
| A1 | OrbTask: draws at phase 0 too | DrainOrbA_Task 221, DrainOrbB_Task 220, DrainOrbC_Task 255, DrainOrbD_Task 265 |
| A2 | OrbTask: Sprite_Current not read again after the phase | DrainOrbA_Task 27, DrainOrbB_Task 32, DrainOrbC_Task 45, DrainOrbD_Task 35 |
| A3 | DrainOrbA_Place: height +0x4000100 | DrainOrbA_Place 2000 |
| A4 | DrainOrbA_Place: +0xA 0xAF | DrainOrbA_Place 2000 |
| A5 | DrainOrbA_Drop: -0x3F | DrainOrbA_Drop 2000 |
| A6 | DrainOrbA_Run: cap 0x1F | DrainOrbA_Run 768 |
| A7 | DrainOrbA_Run: at 0x2F | DrainOrbA_Run 996 |
| A8 | DrainOrbA_Signal: at 0xF | DrainOrbA_Signal 983 |
| A9 | DrainOrbA_Signal: the owner not counted down | DrainOrbA_Signal 489 |
| A10 | DrainOrbA_Shrink: freed at 1 | DrainOrbA_Shrink 1028 |
| B1 | DrainOrbB_Place: at the target | DrainOrbB_Place 258 |
| B2 | DrainOrbB_Place: +0xA 1 | DrainOrbB_Place 492 |
| B3 | DrainOrbB_Drop: +9 0x21 | DrainOrbB_Drop 496 |
| B4 | DrainOrbB_Run: at 0x81 | DrainOrbB_Run 1002 |
| B5 | DrainOrbB_Run: +9 below 0 | DrainOrbB_Run 381 |
| B6 | DrainOrbB_Signal: at 0x9F | DrainOrbB_Signal 1032 |
| B7 | DrainOrbB_Shrink: +0xA not up | DrainOrbB_Shrink 1999 |
| C1 | DrainOrbC_Place: +9 5 | DrainOrbC_Place 2000 |
| C2 | DrainOrbC_Wait: against +2 | DrainOrbC_Wait 627 |
| C3 | DrainOrbC_Shrink: at 3 | DrainOrbC_Shrink 1020 |
| C4 | DrainOrbC_Shrink: the owner not counted down | DrainOrbC_Shrink 522 |
| E1 | DrainOrbD_Place: at the target | DrainOrbD_Place 262 |
| E2 | DrainOrbD_Wait: +0xA down | DrainOrbD_Wait 2000 |
| E3 | DrainOrbD_Shrink: +0xA not up | DrainOrbD_Shrink 1999 |
| W1 | DrainOrb_Draw: frame & 0x7F | DrainOrb_Draw 1004 |
| W2 | DrainOrb_Draw: base radius +0xB * 3 | DrainOrb_Draw 1994 |
| W3 | DrainOrb_Draw: first offset x2 | DrainOrb_Draw 2000 |
| W4 | DrainOrb_Draw: the first radius from phase 3 | DrainOrb_Draw 127 |
| W5 | DrainOrb_Draw: the first collapse without +9 | DrainOrb_Draw 221 |
| W6 | DrainOrb_Draw: height -41 n | DrainOrb_Draw 2000 |
| W7 | DrainOrb_Draw: ring offset x2 | DrainOrb_Draw 2000 |
| W8 | DrainOrb_Draw: shade index + 1 | DrainOrb_Draw 2000 |
| W9 | DrainOrb_Draw: phase 2/3 ring shade B | DrainOrb_Draw 301 |
| W10 | DrainOrb_Draw: factor below 7 | DrainOrb_Draw 105 |
| W11 | DrainOrb_Draw: collapse at +9 = n | DrainOrb_Draw 17 |
| W12 | DrainOrb_Draw: collapsed radius + 1 | DrainOrb_Draw 1279 |
| W13 | DrainOrb_Draw: the upper ring's offset the lower's | DrainOrb_Draw 2000 |
| W14 | DrainOrb_Draw: seven quads a ring | DrainOrb_Draw 2000 |
| W15 | DrainOrb_Draw: green c / 4 | DrainOrb_Draw 2000 |
| W16 | DrainOrb_Draw: third corner the upper shade | DrainOrb_Draw 1998 |
| W17 | DrainOrb_Draw: linked by 0x34 | DrainOrb_Draw 2000 |
| W18 | DrainOrb_Draw: the previous height not kept | DrainOrb_Draw 2000 |
| W19 | DrainOrb_Draw: depths before the projection | DrainOrb_Draw 2000 |
| W20 | DrainOrb_Draw: mode tpage 0x35 | DrainOrb_Draw 2000 |
| X1 | DrainOrb_DrawDisc: radius << 3 | DrainOrb_DrawDisc 1993 |
| X2 | DrainOrb_DrawDisc: tpage 0x35 | DrainOrb_DrawDisc 2000 |
| X3 | DrainOrb_DrawDisc: centre green 0xF1 | DrainOrb_DrawDisc 2000 |
| X4 | DrainOrb_DrawDisc: shade & 7 | DrainOrb_DrawDisc 1100 |
| X5 | DrainOrb_DrawDisc: committed by 0x44 | DrainOrb_DrawDisc 2000 |
| X6 | DrainOrb_DrawDisc: opaque | DrainOrb_DrawDisc 2000 |
| F1 | Buff_Task: entries 3 and 4 swapped | Buff_Task 638 |
| F2 | Buff_Start: +8 not copied | Buff_Start 1918 |
| F4 | Buff_Start: the kind to +5 | Buff_Start 2000 |
| F5 | Buff_Start: +9 0x11 | Buff_Start 1891 |
| F6 | Buff_Start: the ring +1 2 | Buff_Start 1837 |
| F7 | Buff_Start: a spike +9 = b | Buff_Start 2000 |
| F8 | Buff_Start: a spike's +0xA the task's +0xA | Buff_Start 1994 |
| F9 | Buff_Start: 15 words of the strip | Buff_Start 2000 |
| F10 | Buff_Start: the strip not marked | Buff_Start 1992 |
| F11 | Buff_Start: sound 0x102 | Buff_Start 2000 |
| F12 | Buff_WaitChildren: at 2 | Buff_WaitChildren 431 |
| F13 | Buff_Fade: blue byte +5 | Buff_Fade 2000 |
| F14 | Buff_Fade: the next kind's byte | Buff_Fade 457 |
| F15 | Buff_Fade: fallback 7 | Buff_Fade 177 |
| F16 | Buff_Fade: parameter 0x47 | Buff_Fade 482 |
| F17 | Buff_Fade: the actor's tint released | Buff_Fade 235 |
| F18 | Buff_Fade: child +0xA 1 | Buff_Fade 482 |
| K1 | Buff_Kind: 0xB8 answers 1 | Buff_Kind 28 |
| K2 | Buff_Kind: 0x23 answers 3 | Buff_Kind 27 |
| K3 | Buff_Kind: 0x211 becomes 0x55 | Buff_Kind 4 |
| K4 | Buff_Kind: 0x14 answers 3 | Buff_Kind 16 |
| K5 | Buff_Kind: command kind 5 | Buff_Kind 353 |
| R1 | BuffFx_Dispatch: by +2 | BuffFx_Dispatch 1023 |
| R2 | BuffRing_Task: disc before band | BuffRing_Task 1020 |
| R3 | BuffRing_Wait: below 2 | BuffRing_Wait 263 |
| R4 | BuffRing_DrawDisc: centre +9 * 4 | BuffRing_DrawDisc 1988 |
| R5 | BuffRing_DrawDisc: radius << 6 | BuffRing_DrawDisc 2000 |
| R6 | BuffRing_DrawDisc: from 0x80 | BuffRing_DrawDisc 2000 |
| R7 | BuffRing_DrawDisc: rim green the table blue | BuffRing_DrawDisc 1900 |
| R8 | BuffRing_DrawBand: outer << 7 | BuffRing_DrawBand 2000 |
| R9 | BuffRing_DrawBand: outer 2 2 2 | BuffRing_DrawBand 2000 |
| R10 | BuffRing_DrawBand: committed by 0x34 | BuffRing_DrawBand 2000 |
| S1 | BuffSpike_Dispatch: by +1 | BuffSpike_Dispatch 1670 |
| S2 | BuffSpike_Arc: disc 0x19 | BuffSpike_Arc 2000 |
| S3 | SpikeFly: even frames | BuffSpike_Arc 2000, BuffSpike_Arc2 2000 |
| S4 | SpikeFly: height by +0x16 | BuffSpike_Arc 2000, BuffSpike_Arc2 2000 |
| S5 | BuffSpike_Arc: speed -15 | BuffSpike_Arc 392 |
| S6 | BuffSpike_Arc2: 0x1D | BuffSpike_Arc2 411 |
| S7 | BuffSpike_Spin: wide below 6 | BuffSpike_Spin 67 |
| S8 | BuffSpike_Spin: wide 0x41 | BuffSpike_Spin 202 |
| S9 | BuffSpike_Spin: every fourth | BuffSpike_Spin 75 |
| S10 | BuffSpike_Spin: the sound for +0xB 1 too | BuffSpike_Spin 42 |
| S11 | BuffSpike_Spin: not read again after the sound | BuffSpike_Spin 1 |
| S12 | BuffSpike_Spin: the spin compared unsigned | BuffSpike_Spin 820 |
| S13 | BuffSpike_Orbit: +0x10 up 5 | BuffSpike_Orbit 2000 |
| S14 | BuffSpike_Orbit: + 0xB1 | BuffSpike_Orbit 2000 |
| S15 | BuffSpike_Orbit: x >> 4 | BuffSpike_Orbit 2000 |
| S16 | BuffSpike_Orbit: z from the owner's +0x3C | BuffSpike_Orbit 2000 |
| S17 | BuffSpike_Orbit: freed at 0x11 | BuffSpike_Orbit 719 |
| T1 | BuffSpike_Draw: radius 0x31 | BuffSpike_Draw 2000 |
| T2 | BuffSpike_Draw: apex 0x51 | BuffSpike_Draw 2000 |
| T3 | BuffSpike_Draw: the second apex not negated | BuffSpike_Draw 2000 |
| T4 | BuffSpike_Draw: the second angle a + 7 | BuffSpike_Draw 2000 |
| T5 | BuffSpike_Draw: depths reversed | BuffSpike_Draw 2000 |
| T6 | BuffSpike_Draw: packets 0x30 apart | BuffSpike_Draw 2000 |
| T7 | BuffSpike_Draw: colours by 3 * kind | BuffSpike_Draw 1691 |
| T8 | BuffSpike_Draw: the first row slot 3 | BuffSpike_Draw 2000 |
| T9 | BuffSpike_Draw: the second row from the first packet | BuffSpike_Draw 2000 |
| T10 | BuffSpike_Draw: semi-transparent | BuffSpike_Draw 2000 |
| T11 | BuffSpike_Draw: mode linked at dy 0 | BuffSpike_Draw 2000 |
| Q1 | MagicFx_DrawDiscRadius: centre 0x7F | MagicFx_DrawDiscRadius 2000 |
| Q2 | MagicFx_DrawDiscRadius: the centre's y from +0x2E | MagicFx_DrawDiscRadius 2000 |
| Q3 | MagicFx_DrawDiscRadius: steps of 0x100 | MagicFx_DrawDiscRadius 2000 |
| Q4 | MagicFx_DrawDiscRadius: the triangle linked at dy 0 | MagicFx_DrawDiscRadius 2000 |

## 8. What nothing reached

No recorded route casts Drain or any of the five buffs: the combat route's
traces enter nothing in these extents. Every function is fuzz-only. The
live check is the owner's - cast Drain and one of Protect / Speed / Might
(the names are guesses until then) with a save that has them or DIV-0045's
cheat - and what to see is: Drain's four children (two columns of rings, two
discs) at the target and the caster; the buff's ring and four spikes at the
target, the tint and the flash at the end, in the kind's colours.

## 9. Defects (Capcom's, latent, kept)

For the coordinator to number or not:

- **Unbounded dispatch** (the known class, D59's): two stack tables and
  eight `.data` tables indexed by a task byte, unchecked.
- **`BuffSpike_Draw`'s stack index re-read** (section 4): safe as the game
  runs; a store outside the frame if anything between the two reads of
  `+9` ever moved it.
- **Drain's waits on an exact count** (by reading, not measured): types 2
  and 3 wait for the owner's child count to *equal* 2 and 3. It starts at 4;
  types 0 and 1 count it down when their `+0xA` runs out (by the counters
  about frame 177 and 192 after they start), types 2 and 3 when they end
  (about 12 frames after their wait ends). By those counts type 3 sees 3
  after type 0's step and type 2 sees 2 in a window of a few frames; if a
  frame's task order ever let the count pass a value between two runs of
  the waiter, that child would wait forever and `BattleFx_Finish` (which
  waits for 0) would never end the spell. A reading of the counters only.
