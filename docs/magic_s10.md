# Group S10: Ovum, Lavaburst, Howling, Ebonfire and Sacrifice (MAGIC052..MAGIC056)

**Status:** IN PROGRESS (2026-09-26). All 60 functions are ours
(`src/game/magic_s10.cpp`, shadow name `magic_s10`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 120,000 rounds. @CONTROLS_SUMMARY@ Nothing recorded
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

- **Callees** (35 listed; the standard set supplies the rest):
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
  source; the first 16 words of row 2 and their source. 24,926 bytes of
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

    shadow      magic_s10 self-test: 120000 rounds over 60 functions (2000 each), 9078492 calls to the stand-ins,
                0 MISMATCHES; 24926 bytes of state (20 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: @STAR@.

## 6. Controls

@CONTROLS_INTRO@

@CONTROLS_TABLE@

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

The main checkout's copy gets 58 lines under a `round 9 group S10` comment:
54 new, plus four host extents re-listed smaller (`004AC6B0 12`, `004ACA50
57`, `004ADC40 416`, `004AE130 1E6`, each the function's own size). Three
were listed right already (`004AC510`, `004AC840`, `004AD350`).
