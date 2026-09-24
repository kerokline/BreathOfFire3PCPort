# The party members' sprites: following, walking, the cell ahead

**Status:** IN PROGRESS (2026-09-23) - 21 functions ours (round six, group Z,
[`takeover-queue-round6.md`](takeover-queue-round6.md)): the second and
third members' frame, its states 0 and 1 with state 1's two sub-states, the
follow step and its four helpers, and the cell-ahead tests the leader shares.
Each read to its last instruction by capstone against its true extent, the
ones with a PSX twin side by side with it, and fuzzed against a byte copy of
Capcom's with every call out re-aimed at a recorder; 48 negative controls,
46 refused by a count of mismatching rounds, 2 not refused and shown to be
changes that change nothing (section 5). **Through the batch `ab26`** (2026-09-23, [`takeover-queue-round6.md`](takeover-queue-round6.md) status): the shop A/B 35 of 35; section 7 had it owed.

Source: [`src/game/member_sprites.cpp`](../src/game/member_sprites.cpp),
[`member_sprites_callees.h`](../src/game/member_sprites_callees.h),
[`member_sprites_fuzz.cpp`](../src/game/member_sprites_fuzz.cpp). Shadow name
`member_sprites`.

## 1. The functions

Extents by capstone, 2026-09-23 (a linear range dump and a script that lists
every transfer leaving each extent; inline tables skipped). The queue's list
had 16 entries; three were not what the catalogue said, and five
pointer-reached functions were missing:

- `0x526DB0` is 22 bytes, a two-way tail jump, not 1,717: the catalogue's
  size ran on through `0x526DD0`, the body it jumps to. **Added:**
  `Field_CellAheadFlat` `0x526DD0`, reached only by that `jmp` (its callees
  `0x527470`, `0x5280F0`, `0x528070`, `0x528120`, `0x5282C0` are the queue's).
- `0x51B9D0` is 0x8E bytes, not 0x3CD, and `0x51BDA0` 0xE6, not 0x4CB (the
  sizes in `entries_logic.txt` ran into the states after them).
- **Added, pointer-reached:** `Member_Start` `0x51AC70` and `Member_Control`
  `0x51AD40` (entries 0 and 1 of the state table `0x65F960` that
  `Field_MemberFrame` jumps through), `Member_Walk` `0x51AFC0` (entry 1 of
  state 1's table `0x65F99C`) and `Member_WalkEnd` `0x51AFE0` (reached only by
  `Member_Walk`'s `jmp` at `0x51AFCD`). The shop route reaches all four:
  `Member_Follow` is called 1,437 times from `0x51AD52` (inside
  `Member_Control`) and 78 from `0x51B02F` (inside `Member_WalkEnd`)
  ([`analysis/calltrace/recipe_shop/`], caller column).
- No case label posed as a function.

| PC | bytes | PSX | name | shop calls | what it does |
|---|--:|---|---|--:|---|
| `0x51AC50` | 0x17 | `0x801BCE40` | `Field_MemberFrame` | 1,792 | `Sprite_RestoreClut`, then `jmp [0x65F960 + state*4]` |
| `0x51AC70` | 0xC5 | `0x801BCE90` | `Member_Start` (new) | (pointer) | state 0: counters cleared, +6 = +5 - 1, grounded, speed 3, animation, state 1 |
| `0x51AD40` | 0x17 | `0x801BCFAC` | `Member_Control` (new) | (pointer) | state 1: `call [0x65F99C + sub*4]`, `jmp Sprite_ScriptTick` |
| `0x51AD60` | 0x260 | `0x801BCFF8` | `Member_Follow` (new) | 1,515 | sub-state 0: the tests, then `Member_FollowStep`'s answer through a 7-entry table |
| `0x51AFC0` | 0x1C | `0x801BD3B0` | `Member_Walk` (new) | (pointer) | sub-state 1: +9 counted down through `0x52E140`, at 0 `Member_WalkEnd` |
| `0x51AFE0` | 0x6D | `0x801BD3F8` | `Member_WalkEnd` (new) | (jmp) | four event checks, three tests, stop, `Member_Follow` again |
| `0x51B050` | 0x3DD | `0x801BD758` | `Member_FollowStep` (new) | 1,515 | whom to follow, near enough, catch-up, speed, direction, the step - or a jump onto the leader's next half-cell |
| `0x51B430` | 0x198 | `0x801BD4C8` | `Member_StepAhead` (new) | 79 | the step's frames, its aim point, `Field_CellAhead`'s answer |
| `0x51B5D0` | 0x3F8 | `0x801BDD58` | `Member_CatchUp` (new) | 79 | a member left far behind jumps toward its target |
| `0x51B9D0` | 0x8E | `0x801BE288` | `Field_DirectionTo` (new) | 79 | the octant from the sprite to a point |
| `0x51BDA0` | 0xE6 | `0x801BE930` | `Member_Idle` (new) | 1,515 | the standing counter; past 0xF0 one member fidgets (state 6) |
| `0x526DB0` | 0x16 | `0x801BA590` | `Field_CellAhead` (new) | 227 | raised (+0x70) `jmp 0x527640`, else `jmp Field_CellAheadFlat` |
| `0x526DD0` | 0x695 | - | `Field_CellAheadFlat` (new) | (jmp) | the cell ahead of a grounded sprite: turn, stop, go, change state |
| `0x527470` | 0x1C1 | - | `Field_CellClass` (new) | 182 | the class of up to three cells |
| `0x528070` | 0x26 | - | `Field_CellSlope` (new) | 6 | a slope cell's answer by its low nibble |
| `0x5280F0` | 0x2B | - | `Field_TurnUnless` (new) | 8 | turn a straight facing to `to` or its opposite |
| `0x528120` | 0x68 | - | `Field_CellPairTurn` (new) | 14 | two cells against a value: 1, or a turn |
| `0x5282C0` | 0xA2 | - | `Field_ReadCells` (new) | 185 | cells 1..5: the cell ahead and its neighbours |
| `0x528370` | 0x3BD | - | `Field_CellKind` (new) | 925 | the kind of a map cell as seen from another |
| `0x528730` | 0x3C | - | `Field_CellFacing` (new) | 725 | a way-out cell the sprite faces |
| `0x528770` | 0x31 | - | `Field_ObjectAhead` (new) | 739 | an object over 0x40 high at a point |

Bytes are each body to its last instruction, inline tables included (the
figure for `entries_logic.txt`). "Shop calls" are the route's all-original
trace (`bof3x.callcounts.tsv`, caller 0 = total); "(pointer)" and "(jmp)"
functions were not in the traced list, so they have no count. PSX twins from
`analysis/pairs_propagated.json` (the `call`, `callers` and `call-anchored`
tiers) in the sibling's capture of `GAME.EMI` section 0 (`9d00fd19`,
load address `0x80196800`), read with capstone MIPS: `Field_MemberFrame`,
`Member_Start`, `Member_Follow`, `Member_Walk`, `Member_WalkEnd` and
`Field_DirectionTo` instruction by instruction (every test and store in the
same order; the PSX `Field_State` offsets differ - `0x11C` for `0x128`,
`0x12B` for `0x137`, `0x13C` for `0x148`, `0x79` for `0x89` - and its actor
bit is a halfword `lhu` where the PC tests a byte);
`Member_StepAhead` by its calls and constants (`divu` where the PC has
`idiv`); the others by the pairing only. The cell tests have no pair.

## 2. The data

- **Member i** is `ObjTrio + i * 0x14C` (`0x802D40`); the leader is member 0.
  The fields read here: +0 bit 0x80 (may jump) and 0x40 (set by the jump),
  +1 the state, +2 the sub-state, +5 the member's index (1 or 2), +6 whom it
  follows (0 the leader, 1, 2), +8 the facing (0..7, `Field_DirectionTo`'s
  octants), +9 a step's frames left, +0xB, +0xC / +0x10 the step (16.16 per
  frame), +0x29 the draw slot, +0x34 / +0x38 the position (16.16; +0x36 /
  +0x3A its integer cells), +0x3E the ground, +0x5C..0x5F, +0x70 raised; as
  `Field_State`: +0x128 the walking speed (an index into `Field_MoveSpeeds`),
  +0x136 the standing counter, +0x137 an action (8, 9 read here), +0x138 bit
  0, +0x148 the actor index, +0x89.
- **The cells**, the bytes at `0x903850`: cell 0 is the flag `0x5725C0`
  leaves (an object there); `Field_ReadCells` writes cells 1..5;
  `Field_CellAheadFlat` keeps its classes in cells 8..10. Cells 8..0xF are
  also the two longs of the **aim point**, `0x903858` / `0x90385C` (the
  latter named `Scratch_Swap`), where `Member_StepAhead` puts the end of the
  step and `Member_FollowStep` reads it back. `Field_CellClass` indexes the
  cells by the bytes it is given, so all 256 are in the fuzz's state.
- **Tables** (read-only, indexed unchecked): `0x6696DC` two longs a direction,
  a step's unit; `0x66971C` two signed bytes a direction, the cell ahead;
  `Field_DirectionSteps` `0x6697B0`; `Field_MoveSpeeds` `0x6697F0` (0, 1, 2,
  4, 8, 16); the state tables `0x65F960` (9) and `0x65F99C` (2).
- **Map bytes** by `AreaMap_ByteAt`, as `Field_CellKind` reads them: the high
  nibble picks the case - 0x00, 0x60, 0x90, 0xB0..0xE0 plain (0xB_ a way out
  with its facing in the low nibble), 0x80 plain for the leader and a wall
  for a member, 0x10 a wall except 0x11 (a flag), 0x30..0x50 walls except
  0x52, 0x70, 0x20 steps, 0xA0 slopes (0xAE, 0xAF special from a 0xC0 cell),
  0xF0 a wall.

## 3. Callees not ours

Called through raw addresses in `member_sprites_callees.h`, never through a
`symbols.toml` name (the round's rule for calls across groups):

| address | whose | how it is typed |
|---|---|---|
| `0x535120`, `0x535240` | V2 | `unsigned char (unsigned)`: `Member_Follow` passes 1, `Member_WalkEnd` 0 |
| `0x534920` | V2 | `unsigned char ()` |
| `0x534610`, `0x535F50`, `0x535270`, `0x5350C0`, `0x534F10`, `0x534A00` | V2 | `void ()` |
| `0x531DF0` | V1 | `unsigned char ()` |
| `0x52E140` | V1 | `void ()` (`Member_Walk` jumps to it) |
| `0x5725C0` | M | `long (long x, long z, unsigned dir)`: calls `MapView_CheckHeightScale` and `0x5722D0(x, z, dir)`, which uses `dir & 0xFF` and sets the byte `0x903850` (cell 0) - every caller here reads that byte after it; the low word of the answer is compared with 0x40 |
| `0x535610` | nobody's | `unsigned char (long x, long z, unsigned raised, long ground)`: `raised` only tested as a byte, the rest passed to `0x535640` or `0x535830` |
| `0x592890` | nobody's | `unsigned char (unsigned x, unsigned z)`: 16-bit cell coordinates against the bytes `0x8CB580` / `0x8CB581`, a nibble of the map at `0x8C3D80` |
| `0x527640` | nobody's | `unsigned char ()`: the raised sprite's cell-ahead, not reached by the route (its callees `0x527DB0`, `0x527FF0`, `0x5287B0` show no calls in the trace) |

## 4. What each does, and the quirks kept

**The member's frame.** `Field_MemberFrame` restores the clut and jumps
through the 9-entry table by +1, unchecked. State 0 (`Member_Start`) sets the
member up and moves to state 1; state 1 (`Member_Control`) calls the
sub-state (0 `Member_Follow`, 1 `Member_Walk`) and ticks the animation
script. States 2..8 (`0x525370`, `0x51BA60`, `0x51BA80`, `0x437CC0`,
`0x51BAA0`, `0x51BBD0`, `0x51BCF0`) are not taken: not in the traced list, so
nothing says the route reaches them.

**Following.** `Member_Follow` gives way to the event script (request 9,
script flags, the two V2 tests, the leader in state 0xA), lets
`Member_Idle` count, then acts on `Member_FollowStep`: 0 starts a step
(sub-state 1, +9 frames through `0x52E140`), 1..4 stand, 5 and 6 hand over to
states 2 and 8. `Member_FollowStep` picks whom to follow - member 2 follows
member 1 unless member 1 follows it; member 1 follows the leader, or member 2
when there are three and member 2 is nearer the leader's next position -
answers 1 when already within 0x18000 (0x20000 raised) of where that one's
step ends, and otherwise aims two half-cells behind it: a catch-up when far,
the speed from the distance (the followed one's +0x128, one more past two
cells, 5 past six, then clamped to 3..5), the direction, and
`Member_StepAhead`. A member free to jump (+0 bit 0x80, flag bit 0 clear) may
then land on the leader's next half-cell outright, taking the leader's
+0x138 bit and draw slot (answer 0xFF, state 2 sub-state 2).

`Member_CatchUp` is the member left behind: up to n tries, the k-th putting
it k double half-cells toward the target, then growing side tries, the first
free place kept only more than six cells from the leader and where
`0x592890` has a map cell. Everything is cut to the half-cell and grounded.

Kept as the original has them, each named in the source:

- Every index unchecked: the state (+1, table of 9), the sub-state (+2, table
  of 2), the member indices +5 ^ 3 and +6, the actor index +0x148, the
  facing into three tables, `Field_State +0x128` into `Field_MoveSpeeds`.
- **`Member_StepAhead` divides by the speed** (`idiv`): entry 0 of
  `Field_MoveSpeeds` is 0, so a `Field_State +0x128` of 0 faults. Its one
  caller clamps +0x128 to 3..5 just before, so it cannot; ours divides
  through a `volatile` divisor and faults the same way.
- **`Member_CatchUp`'s direction is a stack byte** that the first
  `Field_DirectionTo` fills - and `Field_DirectionTo` writes nothing when the
  point is the sprite's own position, so the original would then read what
  the stack held. Unreachable from the only caller (it calls only when a
  coordinate is more than 0x18000 away, and the first try is made from where
  the sprite stands); ours starts the byte at 0.
- The distances are the `cdq; xor; sub` absolute values (`INT_MIN` stays
  negative) compared signed, and the positions 32-bit sums that wrap.
- `Member_Follow` fetches the ground at the step's end when `Field_State
  +0x89` is 2 and throws it away; `Member_StepAhead` does the same at the aim
  point when nothing is there. Only `MapView_GroundAt`'s side effect on the
  height scale remains.
- `Member_FollowStep` sets `Field_State` to the leader for the free test and
  puts back what it held before, whatever the callees did to it.
- A z-side try of `Member_CatchUp` that fails leaves the sprite at its last
  tried z; the next try sets both coordinates again, and the end puts the
  start back, so nothing outside sees it.

**The cell ahead.** `Field_CellAhead` splits on +0x70. `Field_CellAheadFlat`
answers 1 at once mid-cell on both axes; otherwise `Field_ReadCells` fills
cells 1..5 - `Field_CellKind` of the cell ahead, the two beside the way, and
the two one further on - and by facing and fractions `Field_CellClass`
reduces them to classes (0x10 blocked, 0x20 two different steps, 0x70 a
ledge, 0xB0 a way out, 0xA_ a slope) that turn the sprite
(`Field_TurnUnless`, `Field_CellPairTurn`), stop it (0), let it go (1 or 3),
or change its state (6, 7, or `Field_CellSlope`'s 2, 4, 5). The half-cell
points it asks `0x5725C0` about are `cdq; sub; sar` halvings - but every sum
halved is of whole 16.16 cells, so even, and the rounding never shows (a
control confirms it, section 5).

## 5. The fuzz and its controls

`BOF3X_SHADOW=member_sprites` (or `*`): 168,000 rounds, 8,000 per function,
each on random state with the branches' boundaries seeded. Every original is
byte-copied with every call out - 118 sites, the 21 functions' calls to each
other included - re-aimed at a recording stand-in, and ours runs with the
same stand-ins through `member_sprites::g`; `0x65F960` and `0x65F99C` become
tables of numbered stand-ins (the copy's operand and `g` alike), and the
three inline jump tables (`Member_Follow` 7 entries at +0x244,
`Member_StepAhead` 8 at +0x178, `Field_CellKind` 9 at +0x2A8) are relocated
into their copies (`Field_CellKind`'s byte index table is read in place, it
holds no addresses). Compared: the three members, the object at `0x905DA0`
and `Field_State`, a scratch object, all 256 cells, the eight actor records,
the flag words, `Field_Request`, `Field_MemberCount`, `Draw_OtSlot`,
`Sprite_Current`, the stand-ins' log (a count, a hash of every entry, the
first 48 kept) and the answer's low byte.

The stand-ins give back what callers read: cell 0 after `0x5725C0`, the
direction byte, the aim point after `Member_StepAhead`, a moved position
after `Member_CatchUp`, cells 1..5 after `Field_ReadCells`, a turned facing
after `Field_TurnUnless`, stopped steps and state 1 after `Member_Follow`
and `Sprite_ClearSteps` - and now and then disturb what the caller reads
again (`Sprite_Current`, `Field_State`, object bytes, cells, flags).

Never generated, because the original would fault or write outside the
compared state: a `Sprite_Current` / `Field_State` outside the five objects;
+1 of 9 or more, +2 of 2 or more; a walking speed of 0; +0x148 above 7;
+5 or +6 above 3; a catch-up longer than a few dozen tries.

Result (2026-09-23), headless (`BOF3X_SELFTEST_ONLY=1`), alone and with
`BOF3X_SHADOW='*'` (every module's self-test, all at 0): 168,000 rounds,
313,255 stand-in calls, **0 mismatches**. Branch counts on the original's
runs: the follower walked 222, asked the follow step and stood or hopped
2,580; the follow step was near enough 1,250 times, jumped onto the leader
1,371, answered 1 after a step 3,544; the catch-up asked the map 1,340 times
and made side tries 556; the flat cell test asked for an object 591;
fidgeted 130; `Field_DirectionTo` wrote nothing 604. Distinct answers seen:
follow step 6, step ahead 4, cell class 19, cell kind 145.

Negative controls (2026-09-23), planted one at a time in
`member_sprites.cpp` by a script, each rebuilt and run headless; mismatching
rounds of 8,000 for that function. None was refused by a hang.

| # | control | refused |
|--:|---|--:|
| 0 | `Field_MemberFrame` dispatches by +2 | 6,995 |
| 1 | `Member_Start`: +6 = +5 (no less one) | 7,999 |
| 2 | `Member_Control` without the script tick | 8,000 |
| 3 | `Member_Follow`: flags 0x800 only (not 0xC00) | 207 |
| 4 | `Member_Follow`: answer 6 stops facing 1 or 5 (not 7) | 51 |
| 5 | `Member_Follow`: the discarded ground when +0x89 is 3 | 72 |
| 6 | `Member_Walk`: +9 not counted down | 2,560 |
| 7 | `Member_WalkEnd`: no test of +1 | 923 |
| 8 | `Member_FollowStep`: speed clamped to 6 | 280 |
| 9 | `Member_FollowStep`: `Field_State` not put back | 2,085 |
| 10 | `Member_FollowStep`: member 2 chosen on a tie | 50 |
| 11 | `Member_FollowStep`: near as `<` on x | 27 |
| 12 | `Member_FollowStep`: the aim point's tie as nearer | 689 |
| 13 | `Member_StepAhead`: 0x20 frames facing 2 or 4 | 1,575 |
| 14 | `Member_StepAhead`: the raised aim not doubled | 3,080 |
| 15 | `Member_StepAhead`: facing not restored on an object | 96 |
| 16 | `Member_CatchUp`: side tries from the first try | 221 |
| 17 | `Member_CatchUp`: z put back after its side tries | 1 |
| 18 | `Member_CatchUp`: the direction not kept | 531 |
| 19 | `Member_CatchUp`: tries by `>> 15` | 1,273 |
| 20 | `Member_CatchUp`: the height difference `>> 7` | 723 |
| 21 | `Member_CatchUp`: "near the leader" as `< 0x50000` | 66 |
| 22 | `Field_DirectionTo`: (-,0) as 3 | 737 |
| 23 | `Field_DirectionTo`: writes 1 on (0,0) | 578 |
| 24 | `Member_Idle`: `<= 0xF0` as `<` | 243 |
| 25 | `Member_Idle`: Rand bit 0 without the +1 | 236 |
| 26 | `Field_CellAhead`: raised and flat swapped | 8,000 |
| 27 | `Field_CellAheadFlat`: the corner turn `& 3` | 79 |
| 28 | `Field_CellAheadFlat`: the x edge also answering 6 on 0xB0 | 130 |
| 29 | `Field_CellAheadFlat`: the corner stop on a third 0x20 | 5 |
| 30 | `Field_CellAheadFlat` / `Field_CellKind`: `Half` as a plain `>> 1` | **0** |
| 31 | `Field_CellAheadFlat`: mid-cell with `||` | 3,538 |
| 32 | `Field_CellAheadFlat`: the diagonal z side's last answer 1 | 10 |
| 33 | `Field_CellClass`: two 0x70 as 0x10 | 24 |
| 34 | `Field_CellClass`: 0xA2 whatever the facing | 188 |
| 35 | `Field_CellClass`: the 0x2_ search not ended at the first | **0** |
| 36 | `Field_CellClass`: a slope pair 0xA0 (not 0xA1) | 138 |
| 37 | `Field_CellSlope`: 0xE for 0xF | 2,701 |
| 38 | `Field_TurnUnless`: opposite as xor 2 | 1,777 |
| 39 | `Field_CellPairTurn`: both answering 0 | 2,253 |
| 40 | `Field_ReadCells`: the fourth from cx | 8,000 |
| 41 | `Field_CellKind`: the z midpoint condition inverted | 3,820 |
| 42 | `Field_CellKind`: 0x80 a wall for anyone | 269 |
| 43 | `Field_CellKind`: 0xAE not special | 59 |
| 44 | `Field_CellKind`: the 0x2_ answer masked 0xF0 | 260 |
| 45 | `Field_CellKind`: 0x11 by bit 1 | 112 |
| 46 | `Field_CellFacing`: low nibble under 5 | 90 |
| 47 | `Field_ObjectAhead`: `>= 0x40` | 590 |

The two not refused are changes that change nothing, and no input could tell
them apart: every value `Half` is given is a sum of two whole 16.16 cells
(`(a + b) << 16`, `(X << 17) + 0x10000`), so always even and halved exactly
either way; and the 0x2_ search at position 0 already compares that cell
with every other, so a later start finds nothing new. Three controls were
refused only after seeding (first run 2, 15 and 0 rounds for 5, 4 and 12:
`Field_State +0x89` = 2, the facing kept in 0..7, and an aim point exactly
on the sprite for the tie). Control 17 is refused in 1 round of 8,000.
Read against the code, the z a failed side try leaves is overwritten by the
next try or put back at the end (section 4), so the one round is most likely
a stand-in's disturbance (`Sprite_Current` or the position moved in the
middle of the loop) - not chased further; the quirk is kept either way.

## 6. What the shop route does not reach

By the trace: `0x527640` (the raised sprite's cell-ahead) and so
`Field_CellAhead`'s raised path; `Field_CellKind`'s second caller
`0x5287B0`; the member states 2..8. By reading, rare or unreachable in
play: `Member_CatchUp`'s uninitialised direction and `Member_StepAhead`'s
division by zero (both guarded by their caller), `Member_Idle`'s fidget
(0xF0 frames standing - the route may or may not stand that long). Fuzz only
for those.

## 7. For the batch check

Entries for `analysis/calltrace/entries_logic.txt` (owned ranges, sizes as
measured here; four replace wrong sizes, the rest are new):

    0051AC50 17
    0051AC70 C5
    0051AD40 17
    0051AD60 260
    0051AFC0 1C
    0051AFE0 6D
    0051B050 3DD
    0051B430 198
    0051B5D0 3F8
    0051B9D0 8E
    0051BDA0 E6
    00526DB0 16
    00526DD0 695
    00527470 1C1
    00528070 26
    005280F0 2B
    00528120 68
    005282C0 A2
    00528370 3BD
    00528730 3C
    00528770 31

The shop A/B (`analysis/validate_shop.sh`) reaches every function but
`0x527640`'s side of `Field_CellAhead`. Whether the attract batch reaches
them was not checked here (the attract catalogue listed none of them as
reached-and-unowned, so it may not).
