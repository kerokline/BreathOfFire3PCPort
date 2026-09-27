# Group S10: Ovum, Lavaburst, Howling, Ebonfire and Sacrifice (MAGIC052..MAGIC056)

**Status:** IN PROGRESS (2026-09-26). All 60 functions are ours
(`src/game/magic_s10.cpp`, shadow name `magic_s10`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 120,000 rounds. 359 negative controls: 355 refused by a count (exit 3), one by a fault (L6, its near variant L6b by a count), and three equivalent mutants (S41, S46, S48, section 6), each with a near variant refused by a count. Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, fourth spell wave, group S10
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions taken | Bytes |
|--:|---|---|---|---|---|--:|--:|
| 107 | 0x24A | MAGIC052 | 0x34 | Ovum | `0x4ABCA0..0x4ABF16` | 7 | 596 |
| 134 | 0x24B | MAGIC053 | 0x35 | Lavaburst | `0x4ABF20..0x4ACAA6` | 16 | 2,816 |
| 56 | 0x24C | MAGIC054 | 0x36 | Howling | `0x4ACAB0..0x4ACFDA` | 9 | 1,257 |
| 65 | 0x24D | MAGIC055 | 0x37 | Ebonfire | `0x4ACFE0..0x4AD892` | 5 of 11 | 1,370 |
| 112 | 0x24E | MAGIC056 | 0x38 | Sacrifice | `0x4AD8A0..0x4AE7B6` | 23 | 3,700 |

The extents are `tools/magic_rows.py --unit MAGIC0NN --clones` (capstone
recursive descent; every jump internal, no jump table, nothing `REFUSED`).
All 60 functions lie in the units' extents; none was found inside or missing
from them. MAGIC055's other six were ours already (group CJ's disc-and-fan
phases `FxDiscFan_Grow` / `_Fade` and ring phases `FxRing_Wait` / `_Rise` /
`_Fade`, [`magic_fx_reached.md`](magic_fx_reached.md), and
`MagicFx_DrawFan`, [`battle_items.md`](battle_items.md)). 9,739 bytes, as
the queue counted. Rows and files are `analysis/magic_rows.tsv`'s (the queue
lists the rows in row order against the overlays in file order; MAGIC052 is
row 107).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. Note that
[`battle_items.md`](battle_items.md) calls the PSX overlay whose shapes the
shared draws match "Sacrifice (MAGIC055)"; by this round's rule MAGIC055 is
Ebonfire and Sacrifice is MAGIC056. The two are one id apart, which is the
shift itself. What each spell looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Ovum (MAGIC052).**
  - `Ovum_Task`, the kind-2 task: a three-entry stack table by +1.
  - `Ovum_Start`: +9 8 (the children), +0xA 4 (the first delay).
  - `Ovum_Spawn`: every ninth frame (+0xA counts down from 8) a child (kind
    1, parameter 0x50) at the source sprite's screen point, x jittered by
    `Rand & 0xF`, y raised by `Rand & 0x1F` (a first `Rand` is called and
    its answer dropped), its +9 the count left; after the ninth (+9 was 0)
    +0xA 0x2D and on.
  - `Ovum_End`: after 0x2E frames the target flagged 0x40, the effect's
    done flag, the task freed.
  - The child (`OvumChild_Task` / `_Run` / `_Start`) is Main Cannon's blast
    (group S31's `MainCannonBlast_Run`) with its own start: the blast's
    frame-offset table `0x8C5D80` swapped in round its step, animation 5,
    sound 0x102 for the last child and 0x101 / 0x100 by +9's parity for the
    rest, then S31's `MainCannonBlast_Play` and `BattleFx_FreeTask`.
- **Lavaburst (MAGIC053).**
  - `Lavaburst_Task`: a three-entry stack table by +1 (`Lavaburst_Start`,
    MAGIC226/227's `0x4F9F70`, `BattleFx_Finish`), then a **private pool**
    of 32 task-like records at `0x67F700` (`Lavaburst_Pool`) walked: each
    live one (bit 0 of +0) run as `Sprite_Current` with its +0x80 as the
    owner, through `LavaburstRecord_Task`.
  - `Lavaburst_Start`: the pool's +0..+2 cleared; the task at the side's
    centre and on the screen; its direction the acting actor's sprite's
    (`0x904B3C` +8), turned round (xor 2) when the target's side bit is the
    other side from the actor (0x40 with a party actor, none with an enemy
    actor); eight children (kind 1, 0x5E), each +4 and +9 from the byte
    table `Lavaburst_ChildDelays`, +0xB its number; CLUT row 26 and the
    first 16 words of row 2 back with their STP bits, the first word of each
    without.
  - The child (`LavaburstChild_*`, five steps through `LavaburstChild_Steps`,
    with the effects' frame-offset table `0x8E3580`): `_Launch` waits its
    delay, then starts on a circle round the owner (angle +0xB << 9, radius
    0x18 or 0x28 by `Rand & 0x10`), lifted by a turned offset and a height
    of 0x6000000, animation 0; `_Rise` flies it up by a turned step against
    a fall of 0x600000 a frame, and at the end of its count plays a sound,
    takes eight records from the pool (`Lavaburst_PoolAlloc`; a full pool
    skips one) owned by itself and counted in +4, animation 1, and turns the
    camera's first angle `Camera_Angles[0]` by 0x14; `_Shake` rocks that
    angle by 0x14 each frame for four frames and then **sets it to 0xFD56**;
    `_Settle` plays the script to its end; `_End` waits for its records and
    frees itself. From step 1 to 3 each frame: the screen point, the screen
    update, MAGIC219's `0x4F6020`, the glow (`LavaburstChild_DrawGlow`: 64
    semi-transparent gouraud triangles round the origin, radius +0xB,
    grey +0xA x 4), MAGIC226/227's `0x4FA440`, the matrix popped.
  - The pool record (`LavaburstRecord_*`, three steps through
    `LavaburstRecord_Steps`: MAGIC222's `0x4F7C40`, `_Grow`, `_Shrink`)
    orbits the child at the angle +0xB << 9 and a growing radius +0xC, and
    at the end of its count takes itself off the child's count and frees
    itself through MAGIC219's `0x4F6290`. Each frame under the actor's
    matrix it draws one semi-transparent textured quad of radius 0x100 at
    the angles 0x200, 0x600, 0xE00, 0xA00 (`LavaburstRecord_Draw`).
  - `Lavaburst_PoolAlloc`: the first free record's index, 0xFF when full.
- **Howling (MAGIC054).**
  - `Howling_Task`: a four-entry stack table by +1 (`Howling_Start`, group
    S23's `Simoon_Wait`, `MagicFx_DoneAndFree`, group S26's
    `Magic114_End`).
  - `Howling_Start`: **in an event battle whose byte `0x904AAA` is 0x37**,
    only the target's flags 0x10, +1 3 (straight to `Magic114_End`) and +9
    0x1E. Otherwise every live actor of the target's side (the enemies when
    its 0x40 bit is set, else the party; out by `Battle_ActorIsOut`) is
    given an animation - an enemy 4 through the engine's `0x435A20`, a
    member its +8 + 0x10 with its record made `Sprite_Current` for the call
    - and copied into a child (kind 1, 0x2C) that holds the record's first
    0x80 bytes; the record's +0 gets 0x40; sound 0x100.
  - The child (`HowlingChild_*`, a six-entry stack table: `_Start`, `_Out`,
    `_Hold`, `_Back`, `_End`, MAGIC058's `0x4AF490`) takes its record's
    position and stretches its sprite - the scale +0x40 down while +0x44
    goes up, by steps that shrink to 0 over 33 frames (+0xC from -0xC60 by
    0x60, +0x10 from 0x1080 by -0x80), a hold of 0x10 frames, then 32
    frames the other way (+0xC to 0xC00) - playing its script; at the
    script's end it clears the record's 0x40.
- **Ebonfire (MAGIC055)** is group CJ's disc-and-fan effect
  (`FxDiscFan_Task`, [`magic_fx_reached.md`](magic_fx_reached.md)) with its
  own start and rings.
  - `Ebonfire_Task`: `FxDiscFan_Task` instruction for instruction:
    `Ebonfire_Start`, `FxDiscFan_Grow`, `FxDiscFan_Fade`, then the disc and
    the fan.
  - `Ebonfire_Start`: `FxDiscFan_Start`'s shape with six rings of kind 1
    parameter 0x34 (where the other's are 0x16) and the target read after
    the sound (the other reads it before).
  - `EbonfireRing_Task`: `FxRing_Task` through `FxRing_PhasesTwin` with its
    own draw; `EbonfireRing_Draw` is `MagicFx_DrawRing` with the tpage 0x55
    and abr 2 where the other has 0x35 and 1.
  - `EbonfireRing_End` is the fourth entry of both ring tables, after
    `FxRing_Fade`, which frees the task: **never reached** by reading (the
    third phase frees the task and nothing sets +1 to 3). Taken faithfully
    all the same.
- **Sacrifice (MAGIC056).**
  - `Sacrifice_Task`: `Sacrifice_Start`, `Sacrifice_Wait`,
    `MagicFx_DoneAndFree`.
  - `Sacrifice_Start`: the task at the side's centre; the acting actor's
    animation 8 (`BattleActor_SetAnimation`); a child (kind 1, 0x55, +1 3:
    the actor's copy) holding the acting actor's record's first 0x80 bytes
    (a party member at 2 or less, else the enemy by index - 3, unchecked);
    CLUT row 26's words 1..15 and 17..31 back with their STP bits, words 0
    and 16 cleared; the owner's CLUT copied to the effect row
    (`SpriteClut_CopyToFxRow`, its answer to +0x27), +0x28 / +0x24 the
    owner's, the STP bits of this task's CLUT; the owner's +0 | 0x40.
  - `Sacrifice_Wait`: once the children are gone, the owner's 0x40 off, the
    effect row back, the actor's animation 0x1C, the target flagged 0x40,
    and the acting actor's record put in state 6 / 4 with two words set
    (+0x98 / +0x90 a member's, +0xA4 / +0x92 an enemy's) and its tint
    released.
  - The children (`SacrificeChild_Task` through `SacrificeChild_Kinds`):
    - the actor's copy (`SacrificeActor_*`, nine steps): the owner's CLUT,
      the script to its end, a dark tint that deepens to 0x80 while the copy
      moves to the owner's position and lightens again, then `_Split` makes
      a ring and the disc - **children of the owner, not of the copy**,
      counted in the owner's +0xB - with sound 0x100 and, for a party actor,
      its cue 0 (`Battle_PlayActorCue` with `Field_State` its record) and
      animation 0x10 + direction on it and on the copy (an enemy: animation
      4 through the engine's `0x435A70`); after 0x1E frames the tint again,
      the copy moves to the acting actor's sprite (`0x904B3C`) with
      animation 0x1C + direction, and, once it is the owner's last child,
      fades and ends;
    - the ring (`SacrificeRing_*`): at the owner's position, growing
      (radius +9 x 28, the target flagged 0x10 at +9 = 4) then fading
      (radius +9 x 4 + 0x120); `SacrificeRing_Draw` builds eight bands of
      32 textured quads on a sphere between the angles 0xC00 and 0x800,
      each band's shade fading by 8 a frame once +9 passes 0x10 + the band;
    - the disc (`SacrificeDisc_*`): at the owner's position, 16 gouraud
      triangles of radius `Math_Cos(0x800)` x (+9 + 0x84) x 4 past its first
      step and x +9 x 48 before it (sar 12), red +0xA x 6 and green / blue
      +0xA x 8 at the centre.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one
exception that follows the project's precedent: a phase past any of the
eighteen dispatch tables (nine stack tables, nine `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3).

Calls that push one argument more than the callee takes, as the originals
do, push it in ours too: the projections a depth and a flag pointer. Calls
that push a whole register where the callee reads a byte (the engine's
enemy animations, `Sprite_SetAnimation`) pass the value ours holds; the
fuzz masks them to the byte.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F9F70` | MAGIC226/227 (S38) | entry 1 of `Lavaburst_Task`: +9 down, at 0 target flags 0x10 and +1 on |
| `0x4F7C40` | MAGIC222 (S37) | entry 0 of `LavaburstRecord_Steps` |
| `0x4F6020` | MAGIC219 (S37) | called by `LavaburstChild_Run` before the glow (reached from six overlays) |
| `0x4FA440` | MAGIC226/227 (S38) | called by `LavaburstChild_Run` after the glow |
| `0x4F6290` | MAGIC219 (S37) | the tail jmp of `LavaburstRecord_Shrink`: a pool record freed (group S17's reading) |
| `0x4AF490` | MAGIC058 (S11) | entry 5 of `HowlingChild_Run`: the owner's +0xB down, the task freed |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (`LavaburstChild_Launch`) |
| `0x435A20` | engine, unnamed | an enemy's animation, the enemy made `Sprite_Current` for the call (`Howling_Start`) |
| `0x435A70` | engine, unnamed | an enemy's animation (`SacrificeActor_Split`, `_Return`) |

By name, already ours: `MainCannonBlast_Play` (S31), `Simoon_Wait` (S23),
`Magic114_End` (S26), `MagicFx_DoneAndFree` (E), `FxDiscFan_Grow` / `_Fade`
and `FxRing_Wait` / `_Rise` / `_Fade` (CJ), `BattleFx_FreeTask` /
`BattleFx_Finish`, `MagicFx_DrawDisc` / `_DrawFan` / `_PushActorMatrix`, the
effect library (L), `Battle_ActorIsOut`, `Battle_PlayActorCue`, and the GTE /
GPU / sprite / sound library.

Shared bodies (`analysis/magic_funcs.tsv`): `LavaburstChild_DrawGlow` is also
reached from MAGIC226/227, `EbonfireRing_End` from MAGIC091 (Flare, whose
ring table holds it), `SacrificeFx_TakeOwnerPos` from MAGIC126,
`SacrificeDisc_Hold` / `_Fade` from MAGIC103 and `SacrificeActor_Play` from
MAGIC001. They reach them through the addresses, so taking them changes
nothing for the callers.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `Lavaburst_ChildDelays` | `0x65A9F4` | 8 bytes |
| `LavaburstChild_TaskTable` | `0x65A9FC` | 1 |
| `LavaburstChild_Steps` | `0x65AA00` | 5 |
| `LavaburstRecord_TaskTable` | `0x65AA14` | 1 |
| `LavaburstRecord_Steps` | `0x65AA18` | 3 |
| `HowlingChild_TaskTable` | `0x65AA24` | 1 |
| `SacrificeChild_Kinds` | `0x65AA38` | 4 |
| `SacrificeRing_Steps` | `0x65AA48` | 3 |
| `SacrificeDisc_Steps` | `0x65AA54` | 4 |
| `Lavaburst_Pool` | `0x67F700` | 32 records of 0x84 |

`FxRing_PhasesTwin` (`0x65AA28`, four entries) was named by group CJ. Each
count is where the next table starts (the dump of `0x65A9E0..0x65AA70` read
2026-09-26); the next overlay's data begins at `0x65AA64`.

## 5. The fuzz

`BOF3X_SHADOW=magic_s10` runs `magic_harness::Run` over the 60 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s10_fuzz.cpp`:

- **Callees** (36 listed; the standard set supplies the rest):
  - `Rand`, `Math_Sin` and `Math_Cos` (the first listed over the standard
    one) have an `effect` that moves a scratch word (`0x903850..`) or a
    vertex word a third of the time: every caller keeps its angle, radius
    or vertices there and reads them again after these calls (section 6);
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own; the projections log their SVECTORs through `deref` (6 bytes each);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - the sprite calls that act on `Sprite_Current` (`Sprite_ScriptTick`,
    `_ScriptTickOnce`, `_SetAnimation`, `_UpdateScreen`) log which sprite -
    Howling and Sacrifice make a member's record `Sprite_Current` for one
    call - and the screen update also logs `0x9039D8`, the frame-offset
    table Ovum's and Lavaburst's children swap;
  - `Battle_PlayActorCue` logs `Field_State`, which Sacrifice points at the
    member's record for the call;
  - `Battle_ActorIsOut`, `Sprite_ScriptTick` and `_ScriptTickOnce` answer
    `kFlag` (their callers test al);
  - the pool alloc's recorder answers 0..0x1F or 0xFF, and the clone of the
    alloc answers al (`ret_mask` 0xFF); the pool walk's callee
    (`LavaburstRecord_Task`) is a `kPhase` recorder, logging the record and
    owner it ran for;
  - this group's own draws, the other units' `0x4F6020` / `0x4FA440` /
    `0x4F6290` and the engine's enemy animations, by address.
- **Tables:** the nine `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `0x9039D8`;
  `Camera_Angles[0]`; `Field_State`; `Lavaburst_Pool`; CLUT row 26 and its
  source; the first 16 words of row 2 and their source. 25,886 bytes of
  state in 20 regions.
- **Seed:** `0x904B3C` at one of the harness's sprite records; the pool's
  live bits full, partly taken or random, every record's +0x80 a real slot
  or record (the walk makes it the owner); the target's side bit half the
  time and the actor byte 0..10; each dispatcher inside its table; each
  count one step before and at its threshold (Ovum's +0xA / +9 at 0 and 1,
  the ring's +9 at 3, 4, 5, 0xB, 0xC, the disc's 0xB / 0x1D / 0x2D, the
  tints at 0x90 / 0xA0 and 0xE0 / 0xF0, ...); Howling's event byte 0x37 or
  0x36 half the time and its children's record number inside their side;
  the scale step one frame from its end; the ring draw's +9 across the
  shade branches (0x10..0x2F).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, `0x9039D8`, the camera angle, a pool record's live bit, the
  `0x904B3C` pointer, the event byte.
- **Settle:** while `HowlingChild_End` is fuzzed, its record number +0xB is
  kept inside its side's records after each disturbance (the original
  indexes them unchecked after a call; past them it would write outside the
  compared state).

Result in this worktree (2026-09-26):

    shadow      magic_s10 self-test: 120000 rounds over 60 functions (2000 each), 9078943 calls to the stand-ins,
                0 MISMATCHES; 25886 bytes of state (20 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0, 0 mismatches in every group (2,904 ours; this group's stand-in count 9,078,332 in that run, the harness's pointers into the DLL moving a few branches).

## 6. Controls

359 plants, each put in `magic_s10.cpp` one at a time by a script (not committed; in this session's
scratchpad, `s10/controls.py` and `s10/run_controls.py`) that planted, rebuilt, checked the build had
recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s10`, restored and rebuilt; after the last it ran
the clean self-test (0 mismatches, exit 0). The O-, L-, H-, E-, S- controls are Ovum, Lavaburst, Howling, Ebonfire,
Sacrifice.

- **355 refused by a count** in the functions the plant touches only.
- **L6** (the walk's owner read from +0x7C) was refused by a fault: the owner became a garbage pointer the
  recorders write through. Its near variant **L6b** (the next record's owner, a real pointer) was refused by a count.
- **Three equivalent mutants**, each at a boundary where both sides compute the same value, each with a near
  variant refused: **S41** (`n - 1 <= b - 0x10`: at equality the shade is 0x81 - 0, the default; S41b 79 rounds),
  **S46** (the same for the second shade; S46b 82), **S48** (limit 0x10: at d = 0x10 the formula gives
  0x81 - 0x80 = 1, the else's value; S48b, limit 0x12, 68).

A first run (before the fuzz's final form) left four more standing, and the fuzz was changed for them, not the
controls: **L19** (row 2's seventeenth word) wrote outside the compared state - row 2 is now compared whole; **L34**,
**L36** and **L81** (a scratch word read before, not after, `Rand` / `Math_Sin`) were blind because the harness's
own disturbance reaches one scratch word about once in 1,500 calls - `Rand`, `Math_Sin` and `Math_Cos` now move a
scratch or vertex word a third of the time (`StirScratch`). The whole set was then run again; the table is that run.

The thinnest (fewer than 60 rounds of 2,000): O15 14, L34 10, L36 12, L99 15, S35 12, S93 15, S117 10 (each a
cell read before or after a call where a disturbance must land), O11 19, L81 37, H48 44, S25 49, L64 59, S16 59.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| O1 | Ovum_Task: entries 1/2 swapped | Ovum_Task 1328 |
| O2 | Start: +9 7 | Ovum_Start 2000 |
| O3 | Start: +0xA 5 | Ovum_Start 2000 |
| O4 | Spawn: tests after the decrement | Ovum_Spawn 1315 |
| O5 | Spawn: parameter 0x51 | Ovum_Spawn 665 |
| O6 | Spawn: child +2 kept | Ovum_Spawn 637 |
| O7 | Spawn: the first Rand used | Ovum_Spawn 598 |
| O8 | Spawn: x jitter & 0x1F | Ovum_Spawn 343 |
| O9 | Spawn: y jitter added | Ovum_Spawn 640 |
| O10 | Spawn: +0x32 from +0x30 | Ovum_Spawn 665 |
| O11 | Spawn: source read before the third Rand | Ovum_Spawn 19 |
| O12 | Spawn: child +9 after the decrement | Ovum_Spawn 647 |
| O13 | Spawn: +0xA 9 | Ovum_Spawn 480 |
| O14 | Spawn: +0xA 0x2C | Ovum_Spawn 185 |
| O15 | Spawn: owner read before the create | Ovum_Spawn 14 |
| O16 | End: done bit 3 | Ovum_End 495 |
| O17 | End: no flag 0x40 | Ovum_End 691 |
| O18 | End: tests after the decrement | Ovum_End 1358 |
| O19 | OvumChild_Task: wrong handler | OvumChild_Task 2000 |
| O20 | OvumChild_Run: the effects' frame table | OvumChild_Run 1024 |
| O21 | OvumChild_Run: steps 0/1 swapped | OvumChild_Run 1296 |
| O22 | OvumChild_Run: update with +0 clear | OvumChild_Run 2000 |
| O23 | OvumChild_Run: the table not put back | OvumChild_Run 2000 |
| O24 | OvumChild_Start: +0x25 0x1E | OvumChild_Start 2000 |
| O25 | OvumChild_Start: +0x27 0xB5 | OvumChild_Start 2000 |
| O26 | OvumChild_Start: last sound 0x103 | OvumChild_Start 696 |
| O27 | OvumChild_Start: parity swapped | OvumChild_Start 1304 |
| O28 | OvumChild_Start: animation 6 | OvumChild_Start 2000 |
| O29 | OvumChild_Start: +0x24 0x85 | OvumChild_Start 2000 |
| O30 | OvumChild_Start: word +0x2C 1 | OvumChild_Start 1980 |
| L1 | Lavaburst_Task: entries 0/1 swapped | Lavaburst_Task 1337 |
| L2 | Walk: 31 records | Lavaburst_Task 1030 |
| L3 | Walk: bit 1 | Lavaburst_Task 2000 |
| L4 | Walk: owner not put back | Lavaburst_Task 1627 |
| L5 | Walk: Sprite_Current not put back | Lavaburst_Task 1965 |
| L6 | Walk: owner from +0x7C | a fault (exit 0xC0000005) |
| L6b | Walk: owner the next record's | Lavaburst_Task 1993 |
| L7 | Start: pool +2 kept | Lavaburst_Start 2000 |
| L8 | Start: turn rule inverted | Lavaburst_Start 1912 |
| L9 | Start: actor boundary 4 | Lavaburst_Start 244 |
| L10 | Start: turned by xor 1 | Lavaburst_Start 1008 |
| L11 | Start: +9 0x11 | Lavaburst_Start 1620 |
| L12 | Start: parameter 0x5F | Lavaburst_Start 2000 |
| L13 | Start: seven children | Lavaburst_Start 2000 |
| L14 | Start: +4 the low nibble | Lavaburst_Start 2000 |
| L15 | Start: +9 the byte | Lavaburst_Start 2000 |
| L16 | Start: +0xB i + 1 | Lavaburst_Start 2000 |
| L17 | Start: owner read before the create | Lavaburst_Start 462 |
| L18 | Start: row 26 bit 0x4000 | Lavaburst_Start 2000 |
| L19 | Start: row 2 17 words | Lavaburst_Start 2000 |
| L20 | Start: row 26 word 0 keeps STP | Lavaburst_Start 1026 |
| L21 | Start: not marked dirty | Lavaburst_Start 1992 |
| L22 | Start: delays one on | Lavaburst_Start 2000 |
| L23 | LavaburstChild_Task: wrong handler | LavaburstChild_Task 2000 |
| L24 | Child_Run: the blast's frame table | LavaburstChild_Run 630 |
| L25 | Child_Run: steps 2/3 swapped | LavaburstChild_Run 761 |
| L26 | Child_Run: +0 not bit 0 | LavaburstChild_Run 256 |
| L27 | Child_Run: steps below 5 | LavaburstChild_Run 172 |
| L28 | Child_Run: glow after 0x4FA440 | LavaburstChild_Run 637 |
| L29 | Child_Run: no pop | LavaburstChild_Run 637 |
| L30 | Launch: at 1 | LavaburstChild_Launch 1352 |
| L31 | Launch: angle << 8 | LavaburstChild_Launch 663 |
| L32 | Launch: Rand & 8 | LavaburstChild_Launch 509 |
| L33 | Launch: radius 0x19 | LavaburstChild_Launch 663 |
| L34 | Launch: angle read before Rand | LavaburstChild_Launch 10 |
| L35 | Launch: x from the owner's +0x38 | LavaburstChild_Launch 675 |
| L36 | Launch: cosine of the kept angle | LavaburstChild_Launch 12 |
| L37 | Launch: direction the owner's +9 | LavaburstChild_Launch 666 |
| L38 | Launch: lift 0x20001 | LavaburstChild_Launch 673 |
| L39 | Launch: height 0x6000001 | LavaburstChild_Launch 675 |
| L40 | Launch: +0x3C over the owner's +0x38 | LavaburstChild_Launch 675 |
| L41 | Launch: step 0xFFFFF000 | LavaburstChild_Launch 675 |
| L42 | Launch: fall 0x600001 | LavaburstChild_Launch 675 |
| L43 | Launch: +0x27 0x1B | LavaburstChild_Launch 675 |
| L44 | Launch: +0x29 3 | LavaburstChild_Launch 675 |
| L45 | Launch: animation 2 | LavaburstChild_Launch 675 |
| L46 | Launch: +0xA 0x11 | LavaburstChild_Launch 675 |
| L47 | Launch: no second turn | LavaburstChild_Launch 675 |
| L48 | Launch: +0x48 3 | LavaburstChild_Launch 675 |
| L49 | Rise: the fall added | LavaburstChild_Rise 2000 |
| L50 | Rise: +0xB up by 7 | LavaburstChild_Rise 1959 |
| L51 | Rise: sound parity swapped | LavaburstChild_Rise 688 |
| L52 | Rise: +4 kept | LavaburstChild_Rise 655 |
| L53 | Rise: record number i + 1 | LavaburstChild_Rise 688 |
| L54 | Rise: record +1 1 | LavaburstChild_Rise 688 |
| L55 | Rise: animation 2 | LavaburstChild_Rise 688 |
| L56 | Rise: camera + 0x15 | LavaburstChild_Rise 688 |
| L57 | Rise: +9 5 | LavaburstChild_Rise 688 |
| L58 | Rise: seven records | LavaburstChild_Rise 688 |
| L59 | Rise: counted in +0xB | LavaburstChild_Rise 688 |
| L60 | Shake: parity swapped | LavaburstChild_Shake 1372 |
| L61 | Shake: angle 0xFD57 | LavaburstChild_Shake 628 |
| L62 | Shake: at 1 | LavaburstChild_Shake 1287 |
| L63 | Settle: down by 1 | LavaburstChild_Settle 980 |
| L64 | Settle: tick first | LavaburstChild_Settle 59 |
| L65 | Settle: on when the tick answers 0 | LavaburstChild_Settle 2000 |
| L66 | Child_End: the owner's +0xA | LavaburstChild_End 1305 |
| L67 | Child_End: at +4 1 | LavaburstChild_End 1307 |
| L68 | Glow: tpage 0x56 | LavaburstChild_DrawGlow 2000 |
| L69 | Glow: radius +0xA | LavaburstChild_DrawGlow 1965 |
| L70 | Glow: colour << 3 | LavaburstChild_DrawGlow 1792 |
| L71 | Glow: 63 triangles | LavaburstChild_DrawGlow 2000 |
| L72 | Glow: sine and cosine swapped | LavaburstChild_DrawGlow 2000 |
| L73 | Glow: previous y + 1 | LavaburstChild_DrawGlow 2000 |
| L74 | Glow: colour from 0x90385B | LavaburstChild_DrawGlow 1999 |
| L75 | Glow: commit 0x30 | LavaburstChild_DrawGlow 2000 |
| L76 | LavaburstRecord_Task: wrong handler | LavaburstRecord_Task 2000 |
| L77 | Record_Run: Grow / Shrink swapped | LavaburstRecord_Run 1344 |
| L78 | Record_Run: drawn at +2 0 | LavaburstRecord_Run 350 |
| L79 | Orbit: angle << 8 | LavaburstRecord_Grow 1993, LavaburstRecord_Shrink 1993 |
| L80 | Orbit: x from the owner's +0x38 | LavaburstRecord_Grow 2000, LavaburstRecord_Shrink 2000 |
| L81 | Orbit: cosine of the kept angle | LavaburstRecord_Grow 37, LavaburstRecord_Shrink 43 |
| L82 | Orbit: z radius +0x10 | LavaburstRecord_Grow 2000, LavaburstRecord_Shrink 2000 |
| L83 | Record_Grow: up by 3 | LavaburstRecord_Grow 1993 |
| L84 | Record_Grow: at 1 | LavaburstRecord_Grow 1225 |
| L85 | Record_Shrink: up by 2 | LavaburstRecord_Shrink 1989 |
| L86 | Record_Shrink: the owner's +0xB | LavaburstRecord_Shrink 584 |
| L87 | Record_Shrink: counted in +9 | LavaburstRecord_Shrink 2000 |
| L88 | Record_Draw: radius 0x101 | LavaburstRecord_Draw 1951 |
| L89 | Record_Draw: colour x 5 | LavaburstRecord_Draw 1667 |
| L90 | Record_Draw: corners 1/2 swapped | LavaburstRecord_Draw 2000 |
| L91 | Record_Draw: z 1 | LavaburstRecord_Draw 2000 |
| L92 | Record_Draw: abr 2 | LavaburstRecord_Draw 2000 |
| L93 | Record_Draw: clut 0x1E3 | LavaburstRecord_Draw 2000 |
| L94 | Record_Draw: v 0x7F | LavaburstRecord_Draw 2000 |
| L95 | Record_Draw: +0x45 0xA1 | LavaburstRecord_Draw 2000 |
| L96 | Record_Draw: depths 4_14 | LavaburstRecord_Draw 2000 |
| L97 | Record_Draw: stride 0x14 | LavaburstRecord_Draw 2000 |
| L98 | Record_Draw: commit 0x44 | LavaburstRecord_Draw 2000 |
| L99 | PoolAlloc: 31 records | Lavaburst_PoolAlloc 15 |
| L100 | PoolAlloc: bit 1 set too | Lavaburst_PoolAlloc 733 |
| L101 | PoolAlloc: full answers 0xFE | Lavaburst_PoolAlloc 544 |
| H1 | Howling_Task: entries 1/2 swapped | Howling_Task 1009 |
| H2 | Start: event kind 0x36 | Howling_Start 1003 |
| H3 | Start: event flags 0x20 | Howling_Start 495 |
| H4 | Start: event +1 2 | Howling_Start 495 |
| H5 | Start: event +9 0x1F | Howling_Start 495 |
| H6 | Start: side bit 0x80 | Howling_Start 760 |
| H7 | Start: seven enemies | Howling_Start 760 |
| H8 | Start: out tested one low | Howling_Start 760 |
| H9 | Start: enemy animation 5 | Howling_Start 720 |
| H10 | Start: enemy animation through 0x435A70 | Howling_Start 720 |
| H11 | Start: two members | Howling_Start 745 |
| H12 | Start: member animation + 0x11 | Howling_Start 547 |
| H13 | Start: member animation on the task | Howling_Start 536 |
| H14 | Start: Sprite_Current not put back | Howling_Start 534 |
| H15 | Start: sound 0x101 | Howling_Start 1505 |
| H16 | Start: +0xB kept | Howling_Start 1380 |
| H17 | Copy: 0x7C bytes | Howling_Start 1267, Sacrifice_Start 2000 |
| H18 | Copy: parameter 0x2D | Howling_Start 1267 |
| H19 | Copy: +6 2 | Howling_Start 1267 |
| H20 | Copy: +5 0x2D | Howling_Start 1267 |
| H21 | Copy: +0xB the battle index | Howling_Start 1267 |
| H22 | Copy: record bit 0x20 | Howling_Start 1131 |
| H23 | Copy: owner read before the create | Howling_Start 90 |
| H24 | HowlingChild_Task: wrong handler | HowlingChild_Task 2000 |
| H25 | Child_Run: Out / Hold swapped | HowlingChild_Run 636 |
| H26 | Child_Run: update with +0 clear | HowlingChild_Run 2000 |
| H27 | Child: the member one on | HowlingChild_Start 1038, HowlingChild_End 540 |
| H28 | Child_Start: +0x3C from +0x38 | HowlingChild_Start 2000 |
| H29 | Child_Start: +0x44 0x10001 | HowlingChild_Start 2000 |
| H30 | Child_Start: +0xC 0xFFFFF3A1 | HowlingChild_Start 2000 |
| H31 | Child_Start: +0x10 0x1081 | HowlingChild_Start 2000 |
| H32 | Child_Start: +0x18 0x61 | HowlingChild_Start 2000 |
| H33 | Child_Start: +0x1C 0xFFFFFF81 | HowlingChild_Start 2000 |
| H34 | Child_Start: +0x48 3 | HowlingChild_Start 2000 |
| H35 | Scale: +0x10 by +0x18 | HowlingChild_Out 2000, HowlingChild_Back 2000 |
| H36 | Scale: +0x44 by +0xC | HowlingChild_Out 2000, HowlingChild_Back 2000 |
| H37 | Scale: +0x40 by the old +0xC | HowlingChild_Out 2000, HowlingChild_Back 2000 |
| H38 | Out: +9 0x11 | HowlingChild_Out 647 |
| H39 | Out: on at 1 | HowlingChild_Out 1278 |
| H40 | Out: ticked once | HowlingChild_Out 2000 |
| H41 | Hold: at 1 | HowlingChild_Hold 1288 |
| H42 | Hold: +0x18 0x5F | HowlingChild_Hold 661 |
| H43 | Hold: no tick | HowlingChild_Hold 2000 |
| H44 | Back: at 0xC01 | HowlingChild_Back 1288 |
| H45 | Back: no scale step | HowlingChild_Back 2000 |
| H46 | Child_End: bit 0x80 cleared | HowlingChild_End 1036 |
| H47 | Child_End: on while the tick answers 0 | HowlingChild_End 2000 |
| H48 | Child_End: record read before the tick | HowlingChild_End 44 |
| E1 | Ebonfire_Task: Grow / Fade swapped | Ebonfire_Task 1330 |
| E2 | Task: 8 up | Ebonfire_Task 1005 |
| E3 | Task: disc under the matrix | Ebonfire_Task 1022 |
| E4 | Task: drawn with +0 clear | Ebonfire_Task 2000 |
| E5 | Task: no pop | Ebonfire_Task 1022 |
| E6 | Start: direction the source's +9 | Ebonfire_Start 1893 |
| E7 | Start: +0x3C from +0x38 | Ebonfire_Start 2000 |
| E8 | Start: parameter 0x35 | Ebonfire_Start 2000 |
| E9 | Start: delay 6 i | Ebonfire_Start 2000 |
| E10 | Start: five rings | Ebonfire_Start 2000 |
| E11 | Start: CLUT from word 2 | Ebonfire_Start 2000 |
| E12 | Start: word 0 kept | Ebonfire_Start 2000 |
| E13 | Start: target read before the sound | Ebonfire_Start 86 |
| E14 | Start: flags 0x20 | Ebonfire_Start 2000 |
| E15 | Start: +0xA 2 | Ebonfire_Start 2000 |
| E16 | Start: owner read before the create | Ebonfire_Start 353 |
| E17 | Ring_Task: Rise / Fade swapped | EbonfireRing_Task 955 |
| E18 | Ring_Task: drawn at +1 0 | EbonfireRing_Task 263 |
| E19 | Ring_End: +0xB up by 3 | EbonfireRing_End 1946 |
| E20 | Ring_End: at 0x11 | EbonfireRing_End 481 |
| E21 | Ring_End: owner 0xFE | EbonfireRing_End 478 |
| E22 | Ring_End: +9 up | EbonfireRing_End 1999 |
| E23 | Ring_Draw: inner radius x 5 | EbonfireRing_Draw 1957 |
| E24 | Ring_Draw: outer radius x 2 | EbonfireRing_Draw 1816 |
| E25 | Ring_Draw: lift + 0x41 | EbonfireRing_Draw 1877 |
| E26 | Ring_Draw: first phase & 7 | EbonfireRing_Draw 945 |
| E27 | Ring_Draw: phase + i + 1 | EbonfireRing_Draw 2000 |
| E28 | Ring_Draw: lift added in the loop | EbonfireRing_Draw 2000 |
| E29 | Ring_Draw: lift added before it | EbonfireRing_Draw 1854 |
| E30 | Ring_Draw: 31 quads | EbonfireRing_Draw 2000 |
| E31 | Ring_Draw: lean from y | EbonfireRing_Draw 2000 |
| E32 | Ring_Draw: lean << 8 | EbonfireRing_Draw 2000 |
| E33 | Ring_Draw: tpage 0x35 | EbonfireRing_Draw 2000 |
| E34 | Ring_Draw: abr 1 | EbonfireRing_Draw 2000 |
| E35 | Ring_Draw: clut 0x1FB | EbonfireRing_Draw 2000 |
| E36 | Ring_Draw: columns x 8 | EbonfireRing_Draw 2000 |
| E37 | Ring_Draw: row + 9 | EbonfireRing_Draw 2000 |
| E38 | Ring_Draw: row + 13 | EbonfireRing_Draw 2000 |
| E39 | Ring_Draw: shade 0x69 | EbonfireRing_Draw 2000 |
| E40 | Ring_Draw: inner shade 2 | EbonfireRing_Draw 2000 |
| E41 | Ring_Draw: projected at stride 0x10 | EbonfireRing_Draw 2000 |
| E42 | Ring_Draw: sorted 0x50 | EbonfireRing_Draw 2000 |
| E43 | Ring_Draw: z + 1 | EbonfireRing_Draw 2000 |
| S1 | Sacrifice_Task: entries 1/2 swapped | Sacrifice_Task 1346 |
| S2 | Start: +9 1 | Sacrifice_Start 1905 |
| S3 | Start: animation 9 | Sacrifice_Start 2000 |
| S4 | Start: parameter 0x56 | Sacrifice_Start 2000 |
| S5 | ActingRecord: party below 4 | Sacrifice_Start 267, Sacrifice_Wait 176 |
| S6 | Start: child +1 2 | Sacrifice_Start 2000 |
| S7 | Start: child +6 0 | Sacrifice_Start 2000 |
| S8 | Start: child +5 0x54 | Sacrifice_Start 2000 |
| S9 | Start: second half from the first | Sacrifice_Start 2000 |
| S10 | Start: word 16 kept | Sacrifice_Start 2000 |
| S11 | Start: the task's CLUT to the row | Sacrifice_Start 1736 |
| S12 | Start: +0x28 the owner's +0x24 | Sacrifice_Start 1997 |
| S13 | Start: STP on the owner | Sacrifice_Start 1733 |
| S14 | Start: owner bit 0x20 | Sacrifice_Start 1544 |
| S15 | Start: +0x27 the row + 1 | Sacrifice_Start 2000 |
| S16 | Start: owner read before the create | Sacrifice_Start 59 |
| S17 | Wait: at 1 | Sacrifice_Wait 1342 |
| S18 | Wait: owner bit 0x80 cleared | Sacrifice_Wait 947 |
| S19 | Wait: animation 0x1D | Sacrifice_Wait 1340 |
| S20 | Wait: state 5 | Sacrifice_Wait 1340 |
| S21 | Wait: +2 3 | Sacrifice_Wait 1340 |
| S22 | Wait: member word +0x94 | Sacrifice_Wait 564 |
| S23 | Wait: enemy word 0x2000 | Sacrifice_Wait 776 |
| S24 | Wait: enemy word +0xA6 | Sacrifice_Wait 776 |
| S25 | Wait: actor read before the flag | Sacrifice_Wait 49 |
| S26 | SacrificeChild_Task: kind 1 the disc | SacrificeChild_Task 452 |
| S27 | Ring_Run: Grow / Fade swapped | SacrificeRing_Run 1324 |
| S28 | Ring_Run: drawn at +2 0 | SacrificeRing_Run 348 |
| S29 | TakeOwnerPos: +0x38 from +0x34 | SacrificeFx_TakeOwnerPos 2000 |
| S30 | TakeOwnerPos: +0xA 1 | SacrificeFx_TakeOwnerPos 2000 |
| S31 | Ring_Grow: flags at 5 | SacrificeRing_Grow 499 |
| S32 | Ring_Grow: flags 0x20 | SacrificeRing_Grow 240 |
| S33 | Ring_Grow: x 27 | SacrificeRing_Grow 1995 |
| S34 | Ring_Grow: at 0xD | SacrificeRing_Grow 596 |
| S35 | Ring_Grow: not read again after the call | SacrificeRing_Grow 12 |
| S36 | Ring_Fade: + 0x121 | SacrificeRing_Fade 2000 |
| S37 | Ring_Fade: at 0x29 | SacrificeRing_Fade 520 |
| S38 | Ring_Fade: +0xA kept | SacrificeRing_Fade 1996 |
| S39 | Ring_Draw: outer shade 0x80 | SacrificeRing_Draw 719 |
| S40 | Ring_Draw: inner shade 0x80 | SacrificeRing_Draw 740 |
| S41 | Ring_Draw: first fade from n - 1 at | equivalent (at the boundary d is 0: 0x81, the default) |
| S41b | Ring_Draw: first fade from n + 1 (near S41) | SacrificeRing_Draw 79 |
| S42 | Ring_Draw: first d - 0xE | SacrificeRing_Draw 921 |
| S43 | Ring_Draw: first limit 0x12 | SacrificeRing_Draw 90 |
| S44 | Ring_Draw: first by 4 | SacrificeRing_Draw 968 |
| S45 | Ring_Draw: first past 2 | SacrificeRing_Draw 1997 |
| S46 | Ring_Draw: second fade from n at | equivalent (at the boundary d is 0: 0x81, the default) |
| S46b | Ring_Draw: second fade from n + 1 (near S46) | SacrificeRing_Draw 82 |
| S47 | Ring_Draw: second d - 0x11 | SacrificeRing_Draw 962 |
| S48 | Ring_Draw: second limit 0x10 | equivalent (at d = 0x10 both give 1) |
| S48b | Ring_Draw: second limit 0x12 (near S48) | SacrificeRing_Draw 68 |
| S49 | Ring_Draw: second by 16 | SacrificeRing_Draw 962 |
| S50 | Ring_Draw: second past 2 | SacrificeRing_Draw 1999 |
| S51 | Ring_Draw: inner angle - 0x40 | SacrificeRing_Draw 2000 |
| S52 | Ring_Draw: outer radius from the inner angle | SacrificeRing_Draw 2000 |
| S53 | Ring_Draw: first V8 by the outer radius | SacrificeRing_Draw 2000 |
| S54 | Ring_Draw: inner height x the inner radius | SacrificeRing_Draw 2000 |
| S55 | Ring_Draw: V14 not set | SacrificeRing_Draw 2000 |
| S56 | Ring_Draw: rows m + 0x15 | SacrificeRing_Draw 2000 |
| S57 | Ring_Draw: row1 << 2 | SacrificeRing_Draw 2000 |
| S58 | Ring_Draw: 31 quads | SacrificeRing_Draw 2000 |
| S59 | Ring_Draw: V12 from V18 | SacrificeRing_Draw 2000 |
| S60 | Ring_Draw: x lean << 8 | SacrificeRing_Draw 2000 |
| S61 | Ring_Draw: z by the x lean | SacrificeRing_Draw 2000 |
| S62 | Ring_Draw: tpage 0x55 | SacrificeRing_Draw 2000 |
| S63 | Ring_Draw: abr 2 | SacrificeRing_Draw 2000 |
| S64 | Ring_Draw: depths before the projection | SacrificeRing_Draw 2000 |
| S65 | Ring_Draw: columns x 4 | SacrificeRing_Draw 2000 |
| S66 | Ring_Draw: +0x51 the first row | SacrificeRing_Draw 2000 |
| S67 | Ring_Draw: inner shade the outer's | SacrificeRing_Draw 2000 |
| S68 | Ring_Draw: outer shade the inner's | SacrificeRing_Draw 2000 |
| S69 | Ring_Draw: seven bands | SacrificeRing_Draw 2000 |
| S70 | Ring_Draw: rows down by 2 | SacrificeRing_Draw 2000 |
| S71 | Ring_Draw: n up by 2 | SacrificeRing_Draw 804 |
| S72 | Ring_Draw: sorted 0x50 | SacrificeRing_Draw 2000 |
| S73 | Disc_Run: Hold / Fade swapped | SacrificeDisc_Run 996 |
| S74 | Disc_Run: drawn with +0 clear | SacrificeDisc_Run 739 |
| S75 | Disc_Grow: +0xA up by 3 | SacrificeDisc_Grow 2000 |
| S76 | Disc_Grow: at 0xD | SacrificeDisc_Grow 498 |
| S77 | Disc_Hold: at 0x1F | SacrificeDisc_Hold 516 |
| S78 | Disc_Fade: cap 0x2F | SacrificeDisc_Fade 522 |
| S79 | Disc_Fade: the owner's +0xA | SacrificeDisc_Fade 696 |
| S80 | Disc_Fade: at 1 | SacrificeDisc_Fade 1337 |
| S81 | Disc_Draw: branch past step 2 | SacrificeDisc_Draw 643 |
| S82 | Disc_Draw: + 0x85 | SacrificeDisc_Draw 668 |
| S83 | Disc_Draw: << 3 | SacrificeDisc_Draw 668 |
| S84 | Disc_Draw: x 5 | SacrificeDisc_Draw 1287 |
| S85 | Disc_Draw: << 5 | SacrificeDisc_Draw 1287 |
| S86 | Disc_Draw: green / blue << 2 | SacrificeDisc_Draw 1805 |
| S87 | Disc_Draw: red x 7 | SacrificeDisc_Draw 1819 |
| S88 | Disc_Draw: 15 triangles | SacrificeDisc_Draw 2000 |
| S89 | Disc_Draw: green the red | SacrificeDisc_Draw 1997 |
| S90 | Disc_Draw: rim 1 | SacrificeDisc_Draw 2000 |
| S91 | Disc_Draw: commit 0x30 | SacrificeDisc_Draw 2000 |
| S92 | Disc_Draw: tpage 0x35 | SacrificeDisc_Draw 2000 |
| S93 | Disc_Draw: task read before the cosine | SacrificeDisc_Draw 15 |
| S94 | Actor_Run: Lighten / Split swapped | SacrificeActor_Run 403 |
| S95 | Actor_Run: update at +2 0 | SacrificeActor_Run 131 |
| S96 | Actor_Start: the owner's +0x28 | SacrificeActor_Start 1986 |
| S97 | TintOn: +0x5C 2 | SacrificeActor_Play 1323, SacrificeActor_Wait 661 |
| S98 | TintOn: bit 0x10 | SacrificeActor_Play 1148, SacrificeActor_Wait 584 |
| S99 | TintOn: +0x5F kept | SacrificeActor_Play 1319, SacrificeActor_Wait 659 |
| S100 | TintBy: +0x5F not stepped | SacrificeActor_Darken 2000, SacrificeActor_Lighten 2000, SacrificeActor_Return 2000, SacrificeActor_End 1290 |
| S101 | Animations: the copy's by the member's direction | SacrificeActor_Split 281, SacrificeActor_Return 259 |
| S102 | Animations: Sprite_Current not put back | SacrificeActor_Split 265, SacrificeActor_Return 249 |
| S103 | Play: on while the tick answers 0 | SacrificeActor_Play 2000 |
| S104 | Darken: at 0x70 | SacrificeActor_Darken 685 |
| S105 | Darken: by 0x20 | SacrificeActor_Darken 2000 |
| S106 | Darken: +0x3C from +0x38 | SacrificeActor_Darken 681 |
| S107 | Lighten: +9 9 | SacrificeActor_Lighten 678 |
| S108 | Lighten: bit 0x40 cleared | SacrificeActor_Lighten 262 |
| S109 | Lighten: +0x5C 1 | SacrificeActor_Lighten 678 |
| S110 | Split: at 1 | SacrificeActor_Split 1324 |
| S111 | Split: kinds 1 and 2 | SacrificeActor_Split 643 |
| S112 | Split: children of this task | SacrificeActor_Split 572 |
| S113 | Split: parameter 0x56 | SacrificeActor_Split 652 |
| S114 | Split: sound 0x101 | SacrificeActor_Split 652 |
| S115 | Split: cue 1 | SacrificeActor_Split 289 |
| S116 | Split: Field_State not put back | SacrificeActor_Split 289 |
| S117 | Split: the actor not read again | SacrificeActor_Split 10 |
| S118 | Split: member animation base 0x11 | SacrificeActor_Split 284 |
| S119 | Split: enemy animation 5 | SacrificeActor_Split 368 |
| S120 | Split: enemy path +9 0x1D | SacrificeActor_Split 368 |
| S121 | Split: member path +9 0x1F | SacrificeActor_Split 284 |
| S122 | Split: member below 4 | SacrificeActor_Split 95 |
| S123 | Wait: at 1 | SacrificeActor_Wait 1306 |
| S124 | Wait: no tick | SacrificeActor_Wait 2000 |
| S125 | Return: at 0x70 | SacrificeActor_Return 638 |
| S126 | Return: at the owner | SacrificeActor_Return 476 |
| S127 | Return: member base 0x1D | SacrificeActor_Return 260 |
| S128 | Return: enemy animation 3 | SacrificeActor_Return 377 |
| S129 | Return: member below 4 | SacrificeActor_Return 95 |
| S130 | End: while the owner's count is 2 | SacrificeActor_End 1293 |
| S131 | End: the owner's +0xA | SacrificeActor_End 420 |
| S132 | End: by 0x20 | SacrificeActor_End 1290 |
| S133 | Return: no tick | SacrificeActor_Return 2000 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat.
Things to look for:

- Ovum: nine small blasts near the caster, one every ninth frame.
- Lavaburst: eight bodies rising round the side's centre, each rocking the
  camera and throwing out spinning quads; the glow under them.
- Howling: every live actor of the target's side copied and scaled out and
  back; in an event battle of kind 0x37, nothing but the target flags.
- Ebonfire: Flare's disc and fan with its own rings.
- Sacrifice: the acting actor copied, darkened, moved to the side's centre;
  a ring of eight bands and a red disc; the copy returning to the actor.

Howling's event-battle branch (`0x904AAA` = 0x37) was not traced to a
battle; which fight sets that byte to 0x37 was not read.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the nine stack tables and the
  nine `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Ovum_Spawn`, `Lavaburst_Start` (eight calls), `Howling_Start` (which
  then copies 0x80 bytes into the slot), `Ebonfire_Start` (six),
  `Sacrifice_Start` (with its copy) and `SacrificeActor_Split` (two): slot
  255 is `0x9423FC`, past the image's end - an access violation, in ours as
  in the original (the same address is written).
- **`Sacrifice_Start` and `Sacrifice_Wait` index the enemy records by the
  actor byte - 3**, unchecked above 10; `HowlingChild_Start` / `_End` index
  the side's records by the child's +0xB, unchecked (only 0..7 / 0..2 are
  written by `Howling_Start`).
- **`LavaburstChild_Shake` sets the camera's first angle to 0xFD56**
  whatever it was before the spell, after rocking it relative to its value:
  a camera not at that angle jumps. Whether any battle's camera differs
  there was not measured.
- **`EbonfireRing_End` is dead code** (section 1): if it ran, it would set
  the owner's ring count +0xB to 0xFF, and `FxDiscFan_Fade` would never see
  0 rings.
- **Lavaburst can ask for 64 pool records of a pool of 32** (eight children,
  eight each): `Lavaburst_PoolAlloc` answers 0xFF when full and the child
  skips the record (its number is not reused), so this cannot fault; how
  many are alive at once depends on the delays and was not measured.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 57 lines under a `round 9 group S10` comment:
53 new, plus four host extents re-listed smaller (`004AC6B0 12`, `004ACA50
57`, `004ADC40 416`, `004AE130 1E6`, each the function's own size). Three
were listed right already (`004AC510`, `004AC840`, `004AD350`).
