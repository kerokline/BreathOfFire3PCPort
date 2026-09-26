# Group C2: the enemy-only skills TCRF flags (MAGIC057, 081, 116, 129)

**Status:** IN PROGRESS (2026-09-26) - 64 functions ours
(`src/game/magic_c2.cpp`, shadow name `magic_c2`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)), with no edit to
the harness: 0 mismatches in 128,000 rounds. @CONTROLS_SUMMARY@ Fuzz only:
no recorded route casts any of them (section 11).

Group C2 of round nine's second spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6a):
the four overlays of the enemy-only skills that TCRF reports something about
([`cut-content.md`](cut-content.md) §2).

## 1. The four overlays

| Row | File | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|--:|
| 84 | MAGIC057 | `0x39` | Bone Dance | `0x4AE7C0..0x4AF035` | 21 (and `BattleFx_FreeTask`, ours) |
| 85 | MAGIC081 | `0x51` | RottenBreath | `0x4BF8D0..0x4C01E6` | 16 |
| 59 | MAGIC116 | `0x74` | UtmostAttack | `0x4D9AE0..0x4D9F35` | 11 |
| 145 | MAGIC129 | `0x81` | Holocaust | `0x4E3260..0x4E4416` | 16 |

Extents from `tools/magic_rows.py --unit MAGIC0NN --clones` (capstone
recursive descent: every jump internal, no jump table, nothing `REFUSED`).
64 functions, 9,622 bytes, as the queue counted; none was found inside the
extents or missing from them. The one function of these extents that was
already ours is `BattleFx_FreeTask` `0x4AEE90` (a bare `jmp
BattleTask_FreeCurrent` the linker kept in MAGIC057, reached from 44 files).

The names are TCRF's, which are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2); each is a hypothesis. Each row is
loaded by a single ability id. What these skills look like in play, and
which enemy uses them, is not measured here.

**Shared bodies.** MAGIC129's beam and spark functions (`0x4E33D0` on, all
but `Holocaust_Task`, `Holocaust_Start` and `HolocaustBeam_Task`) are also
reached from MAGIC126 (group S29's) - `tools/magic_rows.py`'s `reached_by`.
Taking them takes them for MAGIC126 too; the harness keys on the address.

## 2. MAGIC057, Bone Dance (row 84)

A kind-2 task (`BoneDance_Task`, three phases by `+1`) and one kind-1 child
type (parameter `0x41`, `BoneDanceChild_Task`, three kinds by `+1`):

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `BoneDance_Task` | `0x4AE7C0` | 0x2E | row 84; stack table by `+1`: start, run, end |
| `BoneDance_Start` | `0x4AE7F0` | 0x59 | at the owner; `Gfx_ClutStripCopyRow(26)`, the dirty byte up by one |
| `BoneDance_Run` | `0x4AE850` | 0x36 | stack table by `+2`: cast, hold, spawn, wait |
| `BoneDance_Cast` | `0x4AE890` | 0x83 | sounds `0x101`, `0x102`; the caster's animation 3; the shaker (child, `+1` 1) |
| `BoneDance_Hold` | `0x4AE920` | 0x33 | the caster's script ticked once; `+9`, `+0xA` 0 - it never moves `+2` itself |
| `BoneDance_Spawn` | `0x4AE960` | 0x82 | on odd frames a bone (child, `+1` 0) at the owner, up to 32 (`+9`) |
| `BoneDance_WaitBones` | `0x4AE9F0` | 0x22 | every bone ended (`+0xA` == `+9`): `+1` on |
| `BoneDance_End` | `0x4AEA20` | 0x1A | the done flag, `Battle_SetTargetFlag40(target)`, free |
| `BoneDanceChild_Task` | `0x4AEA40` | 0x2E | the child: stack table by `+1`: bone, shaker, follower |
| `BoneDanceBone_Run` | `0x4AEA70` | 0x52 | the frame-offset table `0x9039D8` at `0x8E3580`; phases by `+2`; drawn; the table back at `0x8B3580` |
| `BoneDanceBone_Start` | `0x4AEAD0` | 0x144 | placed round the owner by `Rand % 6` (whole units), 0x600 up, falling; 3 bounces |
| `BoneDanceBone_Fall` | `0x4AEC20` | 0xC8 | the fall; under `AreaMap_Elevation(x, z)`: placed again round the owner, 0x600 up; `+9` down |
| `BoneDanceBone_End` | `0x4AECF0` | 0xD | the parent's `+0xA` up, free |
| `BoneDanceShake_Run` | `0x4AED00` | 0x2E | stack table by `+2`: start, step, `BattleFx_FreeTask` |
| `BoneDanceShake_Start` | `0x4AED30` | 0x3E | beat 0, its length from `BoneDanceShake_Counts`; sound `0x100` |
| `BoneDanceShake_Step` | `0x4AED70` | 0xDB | swings `+0x10` 0..-3 (by `+3`); each beat a sound; at the third the parent's `+2` on; `Camera_ShiftY` = `+0x10` x 2, `MapView_Redraw` 2 |
| `BoneDanceShake_Down` / `_Up` | `0x4AEE50` / `0x4AEE70` | 0x16 / 0x15 | the swing's two halves |
| `BoneDanceFollow_Run` | `0x4AEEA0` | 0x5E | as the bone's run, drawn only while the owner's `+0xA` is 0 |
| `BoneDanceFollow_Start` / `_Step` | `0x4AEF00` / `0x4AEFE0` | 0xD7 / 0x56 | one unit short of the owner in x and z, on the map's elevation - read at (x, x) (section 8) |

By reading: the caster's animation, a camera shake of three beats (the
`Camera_ShiftY` swinging 0..-6), during which the parent holds; then 32 bones
dropped round the task's point, each bouncing three times off the map's
elevation before it ends. The third child kind, the follower (`+1` 2), is
set by no creator in MAGIC057: the two creators store `+1` 1 (the shaker)
and 0 (the bones), and `tools/magic_rows.py` has no other file reaching
parameter `0x41`. It is taken with the rest and fuzzed; nothing in play is
known to start it.

## 3. MAGIC081, RottenBreath (row 85)

A kind-2 task with a pool of 48 motes (`RottenBreath_Motes` `0x68E1B8`,
0x84-byte records it walks every frame) and one kind-1 child type
(parameter `0x42`) in two kinds - a cloud (`+1` 0) and an aim (`+1` 1),
dispatched through `.data` (`RottenBreathChild_Types` `0x65B358`):

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `RottenBreath_Task` | `0x4BF8D0` | 0x85 | row 85; stack table by `+1`: start, the engine's `0x43F430`, emit, end; then the mote walk |
| `RottenBreath_Start` | `0x4BF960` | 0xB5 | the motes cleared; at the owner; CLUT row 26's first 16 entries from their source with bit 15 set, entry 0 without; the caster's animation 1 |
| `RottenBreath_Emit` | `0x4BFA20` | 0x64 | the cloud (child, `+1` 0); the caster's script ticked once; `+0xB` 1 |
| `RottenBreath_End` | `0x4BFA90` | 0x47 | the caster ticked once; with the cloud gone (`+0xB` 0): `Battle_SetTargetFlag40(target)`, the done flag, free |
| `RottenBreathChild_Task` | `0x4BFAE0` | 0x12 | `jmp` through `RottenBreathChild_Types` by `+1`, in place, unchecked |
| `RottenBreathCloud_Run` | `0x4BFB00` | 0x2E | stack table by `+2`: start, emit, `MagicFx_EndWithChildren` |
| `RottenBreathCloud_Start` | `0x4BFB30` | 0x9D | two units past the owner in x; the aim (child, `+1` 1), its slot in `+0xA`; sound `0x100` |
| `RottenBreathCloud_Emit` | `0x4BFBD0` | 0x90 | every fourth frame a mote, its `+0xB` the cloud's age (the swirl's phase) and `+0xA` 8, or less past age 100 - a value the swirl overwrites with 0x40 before anything reads it; past age 0x80 on |
| `RottenBreathAim_Run` | `0x4BFC60` | 0x13 | until the done flag, each frame `MagicFx_CenterOnSide` (the target side's centre); then free |
| `RottenBreathMote_Run` | `0x4BFC80` | 0x36 | stack table by `+2`: start, swirl, fly, end |
| `RottenBreathMote_Start` | `0x4BFCC0` | 0xB0 | at the cloud; heading `Math_Ratan2` toward the aim |
| `RottenBreathMote_Swirl` | `0x4BFD70` | 0xC1 | 16 frames along the heading waved by a sine; drawn |
| `RottenBreathMote_Fly` | `0x4BFE40` | 0x4B | `MagicFx_StepToward(the aim, 3)` for its life; drawn |
| `RottenBreathMote_End` | `0x4BFE90` | 0x46 | drawn once more; the cloud's count down; the record freed |
| `RottenBreathMote_Draw` | `0x4BFEE0` | 0x2AA | a textured quad (`Gpu_SetPolyGT4`) round the mote's screen point, growing to 0x40 |
| `RottenBreathMote_Alloc` | `0x4C0190` | 0x57 | the first free mote, its index in al, 0xFF when full |

Phase 1 of the task is the engine's `0x43F430` - the step engine row 123
(Paralyzer) uses, the caster's script ticked until it reports its end
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §3;
group E's).

## 4. MAGIC116, UtmostAttack (row 59)

A kind-2 task and a pool of 128 streaks (`UtmostAttack_Streaks`
`0x698EC0`, 0x3C bytes each):

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `UtmostAttack_Task` | `0x4D9AE0` | 0x36 | row 59; stack table by `+1`: start, wait, stream, end |
| `UtmostAttack_Start` | `0x4D9B20` | 0x83 | at the caster, 0x100 units up; the streaks cleared; the caster's animation 1 |
| `UtmostAttack_WaitCaster` | `0x4D9BB0` | 0x48 | the caster's script to its end; then 0x78 frames, sound effect `0x100` |
| `UtmostAttack_Stream` | `0x4D9C00` | 0x44 | eight streaks a frame, all drawn and moved; at the last frame `Battle_SetTargetFlag40(target)`, sound effect `0x101` |
| `UtmostAttack_End` | `0x4D9C50` | 0x16 | drawn and moved until none is left; the done flag, free |
| `UtmostAttack_ClearStreaks` | `0x4D9C70` | 0x14 | every streak's `+0` cleared |
| `UtmostAttack_DrawStreaks` | `0x4D9C90` | 0xB4 | a draw-mode packet, the map camera (`0x494060`); each live streak drawn, moved by its velocity, its life down; al: any was live |
| `UtmostAttack_DrawStreak` | `0x4D9D50` | 0xEC | one gouraud quad: two points 0x1000 either side in x, projected by `0x494110` |
| `UtmostAttack_SpawnStreaks` | `0x4D9E40` | 0x1D | eight free streaks started |
| `UtmostAttack_FreeStreak` | `0x4D9E60` | 0x19 | the first free streak (eax), or 0 |
| `UtmostAttack_InitStreak` | `0x4D9E80` | 0xB6 | a streak from the task's point, spread by `Rand`, rising one unit a frame, 16 frames, colour `Rand << 7` per channel |

`0x494060` and `0x494110` are Capcom's engine code, unnamed and in no group:
the first sets the GTE rotation and translation from `Camera_Angles` and the
map focus (no arguments), the second projects a world point (x, z, height
<< 16, three dwords) into a primitive's vertex with `Gte_RotTransPers` and
`Gte_StoreDepthF`. Ours calls both by address.

## 5. MAGIC129, Holocaust (row 145)

A kind-2 task with a pool of 64 sparks (`Holocaust_Sparks` `0x6A2D38`, 0x84
bytes, walked every frame) and one kind-1 child type (parameter `0x6A`, a
beam per party member), both dispatched through `.data`
(`HolocaustBeam_Types` / `_Phases` `0x65BC30` / `0x65BC34`,
`HolocaustSpark_Types` / `_Phases` `0x65BC44` / `0x65BC48`; each phases
table begins one cell into its types table):

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `Holocaust_Task` | `0x4E3260` | 0x7D | row 145; stack table by `+1`: start, MAGIC226/227's `0x4F9F70`, `BattleFx_Finish`; then the spark walk |
| `Holocaust_Start` | `0x4E32E0` | 0xC6 | the sparks cleared; at the target side's centre; with the side bit `0x40` nothing more; else a beam per party member 0..2 present |
| `HolocaustBeam_Task` | `0x4E33B0` | 0x12 | `jmp` through `HolocaustBeam_Types` by `+1` |
| `HolocaustBeam_Run` | `0x4E33D0` | 0x36 | `call` through `HolocaustBeam_Phases` by `+2`; `+0xB` up; the band while `+2` |
| `HolocaustBeam_Aim` | `0x4E3410` | 0x24B | after the member's delay: from the acting actor's enemy record to the member; the step and heading |
| `HolocaustBeam_Grow` | `0x4E3660` | 0x1D | the band's length `+0xA` by two to 16 |
| `HolocaustBeam_Emit` | `0x4E3680` | 0x84 | 0x86 frames; every fourth, a spark on the parent |
| `HolocaustBeam_Fade` | `0x4E3710` | 0x27 | the fade `+0x5D` down; the parent's count down, free |
| `HolocaustBeam_Draw` | `0x4E3740` | 0x8B4 | the band: `+0xA` segments of four gouraud quads each (section 5a) |
| `HolocaustSpark_Task` | `0x4E4000` | 0x12 | `jmp` through `HolocaustSpark_Types` by `+1` |
| `HolocaustSpark_Run` | `0x4E4020` | 0x84 | `call` through `HolocaustSpark_Phases` by `+2`; the disc while `+2` |
| `HolocaustSpark_Start` | `0x4E40B0` | 0xF8 | after its delay: round the parent at a random radius, rising; a random colour |
| `HolocaustSpark_Rise` / `_Fade` | `0x4E41B0` / `0x4E41F0` | 0x3D / 0x47 | rising 16 frames, then 16 fading; the parent's count down and MAGIC219's `0x4F6290` (the record's `+0..+4` cleared) |
| `HolocaustSpark_Draw` | `0x4E4240` | 0x17B | a disc of eight gouraud triangles, radius 32 |
| `HolocaustSpark_Alloc` | `0x4E43C0` | 0x57 | the first free spark, its index in al, 0xFF when full |

**5a. The band.** `HolocaustBeam_Draw` keeps its working values in
`DamageScratch`'s words (`0x903850..0x90385E`): the previous and this
segment's half-width, their headings, their centres. For i from 1 while i
is below `+0xA` + 1 (read again each time): the half-width 8 + sin(i x 128)
>> 7; the centre the beam's screen point plus i steps, waved across the
heading by a sine of `+0xB` + i (which `HolocaustBeam_Run` counts up every
frame, so the wave travels); the heading `Math_Ratan2` from the previous
centre; then four semi-transparent gouraud quads between the two segments,
an inner and an outer strip on each side, shaded `+0x5D` x 15 / 12 / 6 and
fading to 1 at the rims and at the band's two ends.

## 6. Calls across groups

Ours calls everything already ours by name (the library's `MagicFx_*`,
`BattleFx_Finish`, `BattleFx_FreeTask`, `MagicFx_EndWithChildren`, the
engine's). The rest by raw address, re-aimed at recorders in the fuzz; none
is bound or renamed here:

| Address | What | Owner | How ours reaches it |
|---|---|---|---|
| `0x43F430` | the caster's script ticked until its end, `+1` on | engine, group E | `RottenBreath_Task`'s stack table (`Phase`) |
| `0x4F9F70` | `+9` down, at 0 target flag `0x10`, `+1` on | MAGIC226/227, group S38 | `Holocaust_Task`'s stack table (`Phase`) |
| `0x4F6290` | `+0..+4` of the current task cleared | MAGIC219, group S37 | `HolocaustSpark_Fade`'s tail call (`MH_AT`) |
| `0x494060` | the map camera's GTE matrices | engine, unnamed, no group | `UtmostAttack_DrawStreaks` (`MH_AT`) |
| `0x494110` | a world point projected into a vertex | engine, unnamed, no group | `UtmostAttack_DrawStreak` (`MH_AT`) |

## 7. The fuzz

`BOF3X_SHADOW=magic_c2`, `magic_c2_fuzz.cpp`, one `magic_harness::Run`;
nothing added to the harness (every need met by its fields):

- **the clones**: 64, as `magic_rows.py --clones` printed them (every
  stack-table immediate re-aimed at a handler recorder, every `E8` / `E9`
  at its callee's);
- **callees the standard set lacks** (listed first, so a standard callee
  listed here stands): the sprite and task callees with an `effect` that
  notes `Sprite_Current` (`Sprite_SetAnimation`, `Sprite_ScriptTickOnce`,
  `Sprite_ScriptTick`, `Sprite_UpdateScreen` - also the frame-offset table
  `0x9039D8` -, `BattleActor_UpdateScreenXY`, `BattleTask_FreeCurrent`,
  `MagicFx_StepToward`, `MagicFx_CenterOnSide`, MAGIC219's `0x4F6290`), so a
  caster swap left out or undone early shows; `Gfx_ClutStripCopyRow`,
  `AreaMap_Elevation`; the draws' GPU calls and `Math_Sin` / `Math_Cos` /
  `Math_Ratan2`; `Gfx_CommitPrim` and `MapView_LinkPrimAt` with an `effect`
  that logs the packet (0x58 bytes) - the stand-ins never advance
  `Gfx_PacketNext`, so every primitive of a draw is built in the same
  bytes; `0x494110` with its vector logged by its twelve bytes (`deref`);
  and this group's own functions called directly (`kPhase` for the steps,
  `kByte` 0xFF..0x2F / 0xFF..0x3F for the two allocators, `kFlag` for the
  any-live flag, `kBool` for the free-streak pointer);
- **`ret_mask`** on the four that answer: the two allocators and the
  any-live flag (`0xFF`), the free-streak pointer (`0xFFFFFFFF`);
- **the `.data` tables**, swapped for recorders: `0x65B358` (2),
  `0x65BC30` (5: the beam's types and its four phases), `0x65BC44` (4);
- **regions** (34,439 bytes with the standard ones): the packet pointer and
  a 0x100-byte buffer of the fuzz's own, the scratch words `0x903850..5F`,
  the vertex scratch's first two words, the frame-offset table pointer,
  `Camera_ShiftY`, `MapView_Redraw`, row 26's first 16 CLUT entries and
  their source, the three pools, and record 255 of the motes and of the
  sparks (where an unchecked 0xFF writes);
- **the seed**: every round the packet pointer into the buffer, every mote's
  and spark's owner `+0x80` a real slot or record (the walks make it the
  owner, which a recorder writes through), half the streaks dead and a
  quarter at their last frame, the side bit `0x40` half the time; then by
  function: each dispatcher's phase inside its table, each count-down at
  and either side of its end (`BoneDance_Spawn`'s 0x20, the cloud's age at
  0x63..0x65 and 0x7F..0x81, the beam's emit window at 0x10 / 0x11, the
  shake's `+0xA` at 0xFF / 1 / 2 and its swing at -4..1), the pools full or
  filled to a point for the allocators and the free-streak search, the aim
  slot the mote reads kept below 48 and the member the beam reads below 5
  (both read directly: past them the originals read outside the image),
  `+0xA` of the four task slots below 0x14 for the band;
- **`settle`** keeps the task slots' `+0xA` below 0x14 while
  `HolocaustBeam_Draw` runs (it loops to `+0xA`, read each time, and at
  0xFF never ends - section 8);
- **`args`**: a streak record for `UtmostAttack_DrawStreak` and
  `_InitStreak`;
- **the disturbance** of the group's cells (the harness's case 14): the
  packet pointer, a scratch word, a vertex word, a byte of a streak, a byte
  of a mote or spark below its owner, the frame-offset table pointer, bit 0
  of a party record (`Holocaust_Start` reads them across its creates).

**Result** (2026-09-26, this worktree):

    shadow      magic_c2 self-test: 128000 rounds over 64 functions (2000 each), 946570 calls to the stand-ins,
                0 MISMATCHES; 34439 bytes of state (22 regions) and the stand-ins' log compared

Coverage: every callee and handler the clones name is reached, e.g.
`Math_Sin` 174,094, `Gpu_SetPolyG4` 37,784, `0x4D9D50` 127,521, `0x494110`
8,000, `0x43F430` 515, `0x4F9F70` 678, `0x4F6290` 1,011, the allocators
1,253 and 725, `HolocaustBeam_Draw` (as `HolocaustBeam_Run`'s tail) 1,115.
Counts depend on the build directory ([`takeover-queue-round9.md`](takeover-queue-round9.md)
§6). `BOF3X_SHADOW='*'`: exit 0.

## 8. Defects (Capcom's, latent, kept)

Described, not fixed; for the coordinator to number.

- **The follower's elevation is read at (x, x).** `BoneDanceFollow_Start`
  and `_Step` push the dword `+0x34` for both of `AreaMap_Elevation`'s
  arguments (`mov ecx, [eax + 0x34]; mov edx, ecx; push ecx; push edx`),
  where the bone's fall pushes `+0x38` then `+0x34`. So a follower would
  stand at the ground height of (x, x). No creator in MAGIC057 starts a
  follower (section 2): unreachable by reading. Control B44 (the fix) is
  refused.
- **Unchecked task slots.** `BoneDance_Cast`, `RottenBreath_Emit`,
  `RottenBreathCloud_Start` and `Holocaust_Start` use
  `BattleTask_Create`'s answer without testing it for 0xFF (full), where
  `BoneDance_Spawn` tests it. With all 48 slots taken each would write
  slot 255, `0x93A000 + 0x84 x 255` = `0x9423FC`, past the image's end
  (`0x93F000`): an access violation unless something is mapped there (not
  measured). Ours writes the same bytes. The fuzz cannot reach it: the
  standard recorder answers 0..47, and an answer of 0xFF would fault both
  sides.
- **Unchecked pool indexes.** `RottenBreathCloud_Emit` and
  `HolocaustBeam_Emit` use their allocator's 0xFF: the record written is
  the 256th, `0x696534` and `0x6AB0B4`, inside other overlays' `.bss`. By
  reading neither pool fills in one cast (a mote every fourth frame over
  the cloud's 0x81 frames, each living about 82 - 16 swirling, 64 flying -
  so about 21 at once, of 48; a spark every fourth frame per beam for 0x86
  frames, each living about 33, so about 25 at once for three beams, of
  64): not expected to happen. The fuzz answers 0xFF one time in 49 and 65 and covers the two
  records.
- **The band never ends at `+0xA` 0xFF.** `HolocaustBeam_Draw` loops while
  the byte counter is below `+0xA` + 1 as a dword: at 0xFF the counter wraps
  to 0 and the loop runs for ever. `HolocaustBeam_Grow` stops `+0xA` at 0x10
  (by twos from 0): unreachable.
- **The beam's near end is the acting actor's enemy record.**
  `HolocaustBeam_Aim` reads the enemy record `actor - 3` (the byte
  `0x904B34`) unchecked: for an enemy caster that is its own record. With a
  party member acting (the skill put in a player's list), the index is
  -3..-1 and the position comes from the task slots below the enemy
  records (`0x93B5FC..`): the beam would start from whatever lies there.
  Its far end reads the enemy record `+4` *without* the `- 3` when the side
  bit is set - but on that side `Holocaust_Start` creates no beam, so that
  branch is not reached by the beams it makes.
- **The stack tables are unbounded** (every `Step` in `magic_c2.cpp`): ours
  aborts past a table, as every stack dispatcher the project has taken
  does; nothing steps a phase past. The `.data` dispatches are unbounded
  too and overlap (`HolocaustBeam_Types` entry 1 is `HolocaustBeam_Phases`
  entry 0; `HolocaustSpark_Phases` entry 3 is MAGIC130 code): read in place
  and unchecked in ours, as in the original; nothing sets the out-of-range
  values.

## 9. TCRF's claims and the PC code

TCRF's notes are about the PlayStation release; here is only what the PC
code of these overlays does. None of the four overlays computes damage,
an element or a status: each draws, plays sounds and sets target flags,
and the effect's numbers come from elsewhere (the ability's data and the
battle code, not read here).

| Skill | TCRF says | The PC overlay |
|---|---|---|
| Holocaust | wrong description; neutral damage | no damage or element code; `Battle_SetTargetFlags(target, 0x10)` after 0x38 frames (MAGIC226/227's `0x4F9F70`) and `Battle_SetTargetFlag40` when the beams are done (`BattleFx_Finish`). Beams only toward party members; with the side bit (the enemies) it draws nothing and ends at once |
| Bone Dance | wrong description | no text: a description is the ability's data |
| RottenBreath | wrong description; poisons all enemies | no status code: one `Battle_SetTargetFlag40(target byte)` at the end; the motes fly at a task kept on `MagicFx_CenterOnSide`'s centre of the target side |
| UtmostAttack | five elements at once | no element code: streaks coloured `Rand << 7` per channel (each 0 or 0x80), one `Battle_SetTargetFlag40(target byte)` |

Whether an element or "all enemies" is what the player sees is the
ability data's and the battle code's, and the owner's eye; the "wrong
description" claims are about text these overlays do not hold.

## 10. The controls

@CONTROLS_TABLE@

## 11. What reaches it

Nothing recorded. The combat route's traces enter no function of these
four overlays ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)
§5); they are enemy skills, cast by whichever enemies have them. The live
check is the owner meeting an enemy that uses Bone Dance, RottenBreath,
UtmostAttack or Holocaust, or DIV-0045's cheat putting the skill in a
list - which for Holocaust means the party-actor case of section 8.

Not reached by the fuzz as a behaviour: a `BattleTask_Create` answer of
0xFF (section 8), and the follower kind in play. No divergence and no
ledger entry: every function is a faithful replacement, except that a phase
past a stack table aborts.

## 12. For `analysis/calltrace/entries_logic.txt`

57 lines appended to the main checkout's copy under a `group C2` comment:
7 of the 64 were listed already with their own extents; 5 were listed as
hosts running on over their neighbours (`0x4BFC80`, `0x4C0190`,
`0x4D9E80`, `0x4E4000`, `0x4E43C0`) and have their own extent added.
