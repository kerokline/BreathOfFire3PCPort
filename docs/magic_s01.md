# Group S01: Nue Stomp and Jump (MAGIC001)

**Status:** IN PROGRESS (2026-09-26). All 26 functions are ours
(`src/game/magic_s01.cpp`, shadow name `magic_s01`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 52,000 rounds. 174 of 174 negative controls refused, every one by a count (exit 3). Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S01
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Entry |
|--:|---|---|---|---|---|
| 1 | 0x22A | MAGIC001 | 0x01 | Nue Stomp | `0x498FE0` |
| 105 | 0x22A | MAGIC001 | 0xA0 | Jump | `0x4995B0` |

One overlay, one extent: `0x498FE0..0x499D74` (the next function,
`0x499D80`, is MAGIC002's, group C3). `tools/magic_rows.py --unit MAGIC001
--clones` (capstone recursive descent): 26 functions, 3,275 bytes, every
jump internal, no jump table, nothing `REFUSED`. None was ours before; none
was found inside or missing from the extent. Both rows point into the same
file and share one function (`Magic001_EndWhenChildDone`).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What either spell looks
like in play has not been measured; the descriptions below say what the code
does.

## 1. What each function does

Every function is a battle task step (`void (void)`, `Sprite_Current` the
slot, `0x93B940` its owner). "The move" is: dword +0x14 (a vertical speed)
+= +0x20 (its step), word +0x3E (the height) += the speed's low word.

**Row 1 - Nue Stomp** (the kind-2 task and one kind-1 child, parameter 0x49):

| Address | Name | What it does |
|---|---|---|
| `0x498FE0` | `NueStomp_Task` | Three-entry stack table by +1: `NueStomp_Start`, `Magic001_EndWhenChildDone`, `MagicFx_DoneAndFree`. |
| `0x499010` | `NueStomp_Start` | The owner's direction byte and position to the task, +0xB 1; `BattleTask_Create(1, 0x49)`, the new slot's first 0x80 bytes a copy of the acting actor's record (a member's below 3, else the enemy's by index - 3), +0x80 this task, +1/+2/+9 0, +6 1, +5 0x49; `SpriteClut_CopyToFxRow(owner)` to +0x27, the owner's +0x28 / +0x24, `SpriteClut_SetStp`; the owner's +0 bit 0x40 set; +1 on. |
| `0x499170` | `NueStompChild_Task` | `jmp [Magic001_ChildTaskTable + 4 x +1]` - two entries, `NueStompChild_Run` and `JumpChild_Run`. |
| `0x499190` | `NueStompChild_Run` | Eleven-entry stack table by +2 (the ten below, then MAGIC015's `0x4A0BA0`); then `Sprite_UpdateScreen` while +0 and +2 are set. |
| `0x499220` | `NueStompChild_Begin` | The acting enemy's animation 0xA (`0x435A70`, section 3); the owner's +0x27; +9, +0xA 0; on. |
| `0x499260` | `NueStompChild_Leap` | At the sprite script's end: `BattleActor_PlaySound(2, 0)`, animation 0xB, speed 0x100 with step -8; on. |
| `0x4992B0` | `NueStompChild_Rise` | The move; when the speed reaches 0, over the source sprite (`0x904B4C`'s +0x34 / +0x38; +0x3C moved by the source's less the owner's), +9 0x1F; on. |
| `0x499330` | `NueStompChild_Drop` | The move; +9 down; at 0 `Sound_PlayById(0x203)`, animation 0xC, `Battle_SetTargetFlags(target, 0x10)`, +9 3; on. |
| `0x4993B0` | `NueStompChild_Crouch` | At the script's end animation 0xA; on. |
| `0x4993E0` | `NueStompChild_Bounce` | The script ticked three times (the last answer tested): `BattleActor_PlaySound(2, 0)`, animation 0xB, speed 0x140 with step -0x40, +0xA 9; on. |
| `0x499440` | `NueStompChild_Hop` | The move; +0xA down; at 0 on. |
| `0x499480` | `NueStompChild_Stomp` | Three ticks: sound 0x203, animation 0xC, +9 down: at 0 on, else back three steps (to `_Crouch`) - three stomps in all. |
| `0x4994E0` | `NueStompChild_Land` | At the script's end `Battle_SetTargetFlag40(target)`, +0 bit 0x20, the tint +0x5C 1 and +0x5D..+0x5F 0; on. |
| `0x499540` | `NueStompChild_Return` | The tint +0x5D..+0x5F down by 0x10 a frame until +0x5D is 0x80, then back at the owner's position; on (to MAGIC015's fade-in and free). |

**Row 105 - Jump** (the kind-2 task and one kind-1 child, parameter 0x4F):

| Address | Name | What it does |
|---|---|---|
| `0x4995B0` | `Jump_Task` | Three-entry stack table by +1: `Jump_Start`, `Magic001_EndWhenChildDone`, `MagicFx_DoneAndFree`. |
| `0x4995E0` | `Jump_Start` | As `NueStomp_Start`, with the caster's `BattleActor_SetAnimation(8, 0)` before the child (+5 0x4F) and `SpriteClut_ClearEntry31(task)` after the STP bits. |
| `0x499750` | `Magic001_EndWhenChildDone` | Both rows' entry 1: once +0xB is 0, `SpriteClut_RestoreFxRow`, the owner's bit 0x40 cleared; on. |
| `0x499780` | `JumpChild_Task` | `jmp [Magic001_ChildTaskTable + 4 + 4 x +1]` - one entry, `JumpChild_Run`. |
| `0x4997A0` | `JumpChild_Run` | Seven-entry stack table by +2 (`_Begin`, `_Rise`, `_Hover`, `_Return`, MAGIC056's `0x4AE3C0`, `_Fade`, MAGIC015's `0x4A0BA0`); then while +0 and +2 are set `Sprite_UpdateScreen`, and below step 5 the shadow: `JumpChild_PushMatrix`, `JumpChild_DrawShadow`, `Gte_PopMatrix`. |
| `0x499820` | `JumpChild_Begin` | The owner's +0x27; speed 0x100 with step -0x10; +9 0, the shadow radius +0xA 0x10; on. |
| `0x499870` | `JumpChild_Rise` | The move, the shadow shrinking by 1 while not 0; at speed 0 over the source sprite, +9 0xF; on. |
| `0x499900` | `JumpChild_Hover` | The move, the shadow growing to 8; +9 down; at 0 `BattleActor_PlaySound(2, 0)`, sound 0x203, speed 0x40 with step -8, the heading +0xC `Math_Ratan2(float (owner x - x), float (owner z - z))`, `Battle_SetTargetFlag40(target)`, +9 0x10; on. |
| `0x4999E0` | `JumpChild_Return` | x += `Math_Sin(+0xC) shl 0xD sar 0xC`, z the same with `Math_Cos` (each through the field pointer taken before its call); the move; while +9 is below 8 the shadow grows to 0x10; +9 down; at 0 on. |
| `0x499A80` | `JumpChild_Fade` | As `NueStompChild_Return`, the shadow shrinking by 2 a frame. |
| `0x499B10` | `JumpChild_PushMatrix` | `Gte_PushMatrix`; a MATRIX on the stack: `Gte_RotTrans` of (x sar 9 - 0x4000, z sar 9 - 0x4000, -(height / 2)) into its translation (the owner's height below step 2, the source sprite's after), `Gte_RotMatrix` of angles 0, `Gte_MulMatrix0(Camera_Matrix, m, m)`, `Gte_SetRotMatrix`, `Gte_SetTransMatrix`. |
| `0x499BD0` | `JumpChild_DrawShadow` | A draw-mode packet (tpage 0x55) to layer 5; the radius +0xA x 8 into `0x903850` / `0x903852`; sixteen semi-transparent POLY_G3 (0x34 bytes) for angles 0x100..0x1000: the centre, the previous rim point and the next (radius x `Math_Sin` / `Math_Cos` sar 12, in `Prim_VertexScratch`), `Gte_RotTransPers3`, `Gte_PrimDepths3_10B`, the centre's colour the radius byte in all three channels, the rim's 1, committed to layer 5; then a draw-mode packet with tpage 0x15. A disc on the ground under the jumper, by reading. |

The sounds and animations are numbers here; which sound or pose each is has
not been looked up.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the one
exception every spell group makes: a phase past any of the six dispatch
tables (four stack tables, the two `.data` task jumps) aborts with a message
([`magic_fx_reached.md`](magic_fx_reached.md) §3), where the original calls
through whatever follows the table. Calls that push one argument more than
the callee takes push it in ours too: `Gte_RotTrans` gets a flag pointer,
`Gte_RotTransPers3` a depth and a flag pointer. `DIVERGENCE.md` and
`cheats.cpp` patch no byte in the extent or the table.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4A0BA0` | MAGIC015 (group S04, this wave) | the last entry of both children's step tables: +0x5D..+0x5F up by 0x10; at 0xC0 the owner's +0xB down and the task freed |
| `0x4AE3C0` | MAGIC056 (group S10, wave four) | step 4 of `JumpChild_Run`: at the script's end +0 bit 0x20, +0x5C 1, +0x5D..+0x5F 0, on |
| `0x435A70` | engine, unnamed, in no group | Nue Stomp's animations: `BattleEnemy_SetAnimation(anim)` on the enemy `0x93B960 + 0x128 (a0 - 3)` (a0 masked to a byte, unchecked), `0x939AD8` set to it and put back |

By name, already ours: `MagicFx_DoneAndFree` (group E), the effect
library's `SpriteClut_*` and `BattleActor_*` (L), and the GTE / GPU / math /
sprite / sound library.

Shared bodies: `analysis/magic_funcs.tsv` has no other unit reaching these
26. Both children are reached through the kind-1 table
(`battle_fx_tasks.cpp`, parameters 0x49 and 0x4F).

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `Magic001_ChildTaskTable` | `0x65A4F0` | 2 |

`NueStompChild_Task` jumps through it from its first entry,
`JumpChild_Task` from its second (`0x65A4F4`) - one table read at two bases.
The count is where the next dword stops being code (read 2026-09-26).

## 5. The fuzz

`BOF3X_SHADOW=magic_s01` runs `magic_harness::Run` over the 26 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s01_fuzz.cpp`:

- **Callees** (25 listed; the standard set supplies the rest):
  - `Gfx_CommitPrim` has an `effect` that logs the primitive's bytes
    (`NoteBytes`, the size the call names) and moves `Gfx_PacketNext` on
    through a 0x2000-byte buffer of the fuzz's own - the shadow's sixteen
    triangles are built one after another, so without the log only the last
    would be compared;
  - `Gte_RotTransPers3` logs its three SVECTORs (`deref` 6 each) and writes
    three screen points where the real one writes, so the primitive logged
    at the commit carries them; the matrix push's GTE callees log theirs and
    write a result where the real ones write (S22's effects);
  - `Sprite_ScriptTickOnce` answers from its own stream (0 a third of the
    time) instead of the recorder's flag, and logs `Sprite_Current` - the
    `kFlag` blind spot (round9 doc §9): with the recorder's flag a "no" came
    only with no disturbance. No function here re-reads after a "no" (each
    returns at once), but the three-tick steps read the task after answers
    whose earlier ticks said "no";
  - `Sprite_UpdateScreen` logs which sprite;
  - `0x435A70` (the actor masked to a byte), the `SpriteClut_*` helpers,
    `Math_Sin` / `_Cos` / `_Ratan2` (the two floats as their bits), the GPU
    setters;
  - `JumpChild_PushMatrix` and `JumpChild_DrawShadow` by address as
    `kPhase` (their caller calls them directly).
- **Tables:** `Magic001_ChildTaskTable`.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`. 19,604 bytes of state
  in 12 regions.
- **Seed:** each dispatcher inside its table; +0 zero half the time (the
  runs' draw gates); each speed one step from 0, at 0 or one past
  (`NueStompChild_Rise`, `JumpChild_Rise`); each count-down one step before
  and at its end (+9 in `_Drop`, `_Stomp`, `_Hover`; +0xA in `_Hop`); the
  shadow's limits (7 / 8 in `_Hover`; +9 around 8 and +0xA 0xF / 0x10 in
  `_Return`; 0..2 in `_Rise`, `_Fade`); the tint +0x5D at 0x80, 0x70, 0x81,
  0x90 for the two fades; +0xB 0 / 1 for `Magic001_EndWhenChildDone`; +2 at
  1 / 2 for `JumpChild_PushMatrix`. The harness's actor byte (0..4) covers
  both sides of the member / enemy split.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, the
  radius words (kept below 0x100), the task's +9 / +0xA / +0xB / +0x5D /
  +0x27, and its +0xC / +0x14 / +0x20 / +0x34 / +0x38 / +0x3C.

Result in this worktree (2026-09-26):

    shadow      magic_s01 self-test: 52000 rounds over 26 functions (2000 each), 318038 calls to the stand-ins,
                0 MISMATCHES; 19604 bytes of state (12 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`; `Math_Ratan2` 452 times, `0x499B10` / `0x499BD0`
600). `BOF3X_SHADOW='*'`: exit 0 (318,113 stand-in calls for this group in
that run: the harness's pointers into the DLL move a few branches, 0
mismatches).

## 6. Controls

174 plants, each put in `magic_s01.cpp` one at a time by a script (not committed; group S31's, re-aimed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s01`, restored; after the last it restored, rebuilt and ran the clean self-test (rc=0 build=0, 0 mismatches). **174 of 174 refused**, each with a count only in the functions the plant touches (a plant in a shared helper - `SpawnCopy`, `FadeHome`, the move - is refused in every function that uses it).

The thinnest (fewer than 60 rounds):

- **JR3** (JumpChild_Run: +2 read through the old pointer): 11
- **HV12** (Hover: dx through the old task pointer): 28
- **RT6** (Return: grow while +9 below 9): 37
- **DS13** (DrawShadow: first rim z by +2): 35
- **DS22** (DrawShadow: rim read before the calls): 57

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | NueStomp_Task: table order | NueStomp_Task 1343 |
| T2 | Jump_Task: table order | Jump_Task 1339 |
| T3 | Jump_Task: last entry | Jump_Task 661 |
| T4 | NueStompChild_Task: table order | NueStompChild_Task 2000 |
| T5 | JumpChild_Task: wrong handler | JumpChild_Task 2000 |
| S1 | TakeOwnerPlace: direction byte | NueStomp_Start 1930; Jump_Start 1908 |
| S2 | TakeOwnerPlace: +0x3C from +0x38 | NueStomp_Start 1956; Jump_Start 1959 |
| S3 | NueStomp_Start: +0xB 2 | NueStomp_Start 1929 |
| S4 | NueStomp_Start: parameter 0x48 | NueStomp_Start 2000 |
| S5 | SpawnCopy: kind 3 | NueStomp_Start 2000; Jump_Start 2000 |
| S6 | SpawnCopy: party below 2 | NueStomp_Start 437; Jump_Start 422 |
| S7 | SpawnCopy: enemy index - 2 | NueStomp_Start 792; Jump_Start 817 |
| S8 | SpawnCopy: 0x7C bytes | NueStomp_Start 2000; Jump_Start 2000 |
| S9 | SpawnCopy: +0x80 the owner | NueStomp_Start 1740; Jump_Start 1763 |
| S10 | SpawnCopy: +2 1 | NueStomp_Start 2000; Jump_Start 2000 |
| S11 | SpawnCopy: +6 0 | NueStomp_Start 2000; Jump_Start 2000 |
| S12 | SpawnCopy: +5 parameter + 1 | NueStomp_Start 2000; Jump_Start 2000 |
| S13 | SpawnCopy: +9 1 | NueStomp_Start 1999; Jump_Start 1996 |
| S14 | SpawnCopy: slot & 0x1F | NueStomp_Start 713; Jump_Start 654 |
| S15 | SpawnCopy: actor read before the call | NueStomp_Start 76; Jump_Start 48 |
| S16 | SpawnCopy: +1 1 | NueStomp_Start 1998; Jump_Start 2000 |
| C1 | TakeOwnerClut: row >> 8 | NueStomp_Start 1988; Jump_Start 1990 |
| C2 | TakeOwnerClut: +0x28 from +0x29 | NueStomp_Start 1995; Jump_Start 1987 |
| C3 | TakeOwnerClut: +0x24 from +0x25 | NueStomp_Start 1997; Jump_Start 1993 |
| C4 | TakeOwnerClut: SetStp on the owner | NueStomp_Start 1739; Jump_Start 1757 |
| C5 | TakeOwnerClut: CopyToFxRow of the task | NueStomp_Start 1740; Jump_Start 1763 |
| N1 | NueStomp_Start: owner |= 0x60 | NueStomp_Start 1066 |
| N2 | NueStomp_Start: +1 by 2 | NueStomp_Start 2000 |
| N3 | NueStomp_Start: no CLUT | NueStomp_Start 2000 |
| R1 | NueStompChild_Run: steps 3 / 4 swapped | NueStompChild_Run 364 |
| R2 | NueStompChild_Run: no +2 test | NueStompChild_Run 114 |
| R3 | NueStompChild_Run: no +0 test | NueStompChild_Run 862 |
| R4 | NueStompChild_Run: last step MAGIC056's | NueStompChild_Run 169 |
| E1 | EnemyAnimation: the target | NueStompChild_Begin 1815; NueStompChild_Leap 1195; NueStompChild_Drop 485; NueStompChild_Crouch 1199; NueStompChild_Bounce 1183; NueStompChild_Stomp 1193 |
| B1 | Begin: animation 0xB | NueStompChild_Begin 2000 |
| B2 | Begin: +0x27 from +0x28 | NueStompChild_Begin 1989 |
| B3 | Begin: +9 1 | NueStompChild_Begin 2000 |
| B4 | Begin: +0xA 1 | NueStompChild_Begin 2000 |
| L1 | Leap: PlaySound(2, 1) | NueStompChild_Leap 1305 |
| L2 | Leap: speed 0x101 | NueStompChild_Leap 1305 |
| L3 | Leap: step -9 | NueStompChild_Leap 1305 |
| L4 | Leap: test inverted | NueStompChild_Leap 2000 |
| L5 | Leap: animation 0xC | NueStompChild_Leap 1305 |
| M1 | Rise: step from +0x1C | NueStompChild_Rise 2000; NueStompChild_Drop 2000; NueStompChild_Hop 2000; JumpChild_Rise 2000; JumpChild_Hover 1997; JumpChild_Return 2000 |
| M2 | Rise: height by +0x16 | NueStompChild_Rise 1094; NueStompChild_Drop 2000; NueStompChild_Hop 2000; JumpChild_Rise 1071; JumpChild_Hover 1997; JumpChild_Return 2000 |
| M3 | Rise: height down | NueStompChild_Rise 1565; NueStompChild_Drop 2000; NueStompChild_Hop 2000; JumpChild_Rise 1548; JumpChild_Hover 1997; JumpChild_Return 2000 |
| NR1 | NueStompChild_Rise: > 0 | NueStompChild_Rise 804 |
| NR2 | NueStompChild_Rise: +9 0x1E | NueStompChild_Rise 435 |
| O1 | OverSource: x from +0x38 | NueStompChild_Rise 435; JumpChild_Rise 452 |
| O2 | OverSource: z the owner's | NueStompChild_Rise 331; JumpChild_Rise 338 |
| O3 | OverSource: owner +0x38 | NueStompChild_Rise 435; JumpChild_Rise 452 |
| O4 | OverSource: d + 1 | NueStompChild_Rise 435; JumpChild_Rise 452 |
| D1 | Drop: > 1 | NueStompChild_Drop 511 |
| D2 | Drop: flags 0x20 | NueStompChild_Drop 530 |
| D3 | Drop: +9 4 | NueStompChild_Drop 530 |
| D4 | Drop: sound 0x204 | NueStompChild_Drop 530 |
| D5 | Drop: no move | NueStompChild_Drop 2000 |
| D6 | Drop: animation 0xA | NueStompChild_Drop 530 |
| D7 | Drop: target read as the actor | NueStompChild_Drop 484 |
| CR1 | Crouch: animation 0xB | NueStompChild_Crouch 1321 |
| CR2 | Crouch: +2 not moved | NueStompChild_Crouch 1321 |
| BO1 | TickThrice: the first answer | NueStompChild_Bounce 903; NueStompChild_Stomp 896 |
| BO2 | TickThrice: twice | NueStompChild_Bounce 2000; NueStompChild_Stomp 2000 |
| BO3 | Bounce: speed 0x141 | NueStompChild_Bounce 1299 |
| BO4 | Bounce: step -0x3F | NueStompChild_Bounce 1299 |
| BO5 | Bounce: +0xA 8 | NueStompChild_Bounce 1299 |
| BO6 | Bounce: animation 0xA | NueStompChild_Bounce 1299 |
| BO7 | Bounce: PlaySound(1, 0) | NueStompChild_Bounce 1299 |
| H1 | Hop: counts +9 | NueStompChild_Hop 2000 |
| H2 | Hop: > 1 | NueStompChild_Hop 492 |
| H3 | Hop: no move | NueStompChild_Hop 2000 |
| ST1 | Stomp: back two | NueStompChild_Stomp 1062 |
| ST2 | Stomp: > 1 | NueStompChild_Stomp 265 |
| ST3 | Stomp: sound 0x202 | NueStompChild_Stomp 1325 |
| ST4 | Stomp: animation 0xB | NueStompChild_Stomp 1325 |
| ST5 | Stomp: task read before the calls | NueStompChild_Stomp 82 |
| LA1 | Land: flag the actor | NueStompChild_Land 1250 |
| LA2 | Land: +0 |= 0x10 | NueStompChild_Land 1210 |
| LA3 | Land: +0x5C 2 | NueStompChild_Land 1374 |
| LA4 | Land: +0x5D 1 | NueStompChild_Land 1374 |
| LA5 | Land: +0x5E 1 | NueStompChild_Land 1374 |
| LA6 | Land: +0x5F 1 | NueStompChild_Land 1374 |
| F1 | FadeHome: above 0x80 | NueStompChild_Return 630; JumpChild_Fade 648 |
| F2 | FadeHome: - 8 | NueStompChild_Return 1644; JumpChild_Fade 1669 |
| F3 | FadeHome: +0x5E by 0xF1 | NueStompChild_Return 1644; JumpChild_Fade 1669 |
| F4 | FadeHome: +0x5F by 0xF1 | NueStompChild_Return 1644; JumpChild_Fade 1669 |
| F5 | FadeHome: shrink above 1 | JumpChild_Fade 360 |
| F6 | FadeHome: shrink by 1 | JumpChild_Fade 1666 |
| F7 | FadeHome: home at 0x70 | NueStompChild_Return 692; JumpChild_Fade 656 |
| F8 | FadeHome: x from +0x38 | NueStompChild_Return 692; JumpChild_Fade 656 |
| F9 | FadeHome: +0x3C from +0x38 | NueStompChild_Return 692; JumpChild_Fade 656 |
| F10 | FadeHome: +2 not moved | NueStompChild_Return 692; JumpChild_Fade 656 |
| F11 | JumpChild_Fade: no shrink | JumpChild_Fade 1666 |
| F12 | NueStompChild_Return: shrink | NueStompChild_Return 1996 |
| JS1 | Jump_Start: animation 9 | Jump_Start 2000 |
| JS2 | Jump_Start: animation arg 1 | Jump_Start 2000 |
| JS3 | Jump_Start: parameter 0x4E | Jump_Start 2000 |
| JS4 | Jump_Start: ClearEntry31 of the owner | Jump_Start 1759 |
| JS5 | Jump_Start: animation after the child | Jump_Start 2000 |
| JS6 | Jump_Start: +0xB 0 | Jump_Start 1882 |
| JS7 | Jump_Start: owner |= 0x41 | Jump_Start 1044 |
| W1 | EndWhenChildDone: above 1 | Magic001_EndWhenChildDone 493 |
| W2 | EndWhenChildDone: &= 0x9F | Magic001_EndWhenChildDone 241 |
| W3 | EndWhenChildDone: no restore | Magic001_EndWhenChildDone 491 |
| W4 | EndWhenChildDone: +1 by 2 | Magic001_EndWhenChildDone 491 |
| JR1 | JumpChild_Run: steps 2 / 3 swapped | JumpChild_Run 562 |
| JR2 | JumpChild_Run: draw below 4 | JumpChild_Run 124 |
| JR3 | JumpChild_Run: +2 read through the old pointer | JumpChild_Run 11 |
| JR4 | JumpChild_Run: no pop | JumpChild_Run 600 |
| JR5 | JumpChild_Run: draw before matrix | JumpChild_Run 600 |
| JR6 | JumpChild_Run: no +2 test | JumpChild_Run 146 |
| JR7 | JumpChild_Run: step 4 JumpChild_Fade | JumpChild_Run 292 |
| JB1 | Begin: speed 0x110 | JumpChild_Begin 2000 |
| JB2 | Begin: step -0x11 | JumpChild_Begin 2000 |
| JB3 | Begin: +9 1 | JumpChild_Begin 2000 |
| JB4 | Begin: +0xA 0xF | JumpChild_Begin 2000 |
| JB5 | Begin: +0x27 from +0x26 | JumpChild_Begin 1994 |
| JRi1 | Rise: shrink above 1 | JumpChild_Rise 505 |
| JRi2 | Rise: +9 0x10 | JumpChild_Rise 452 |
| JRi3 | Rise: > 0 | JumpChild_Rise 815 |
| HV1 | Hover: grow below 9 | JumpChild_Hover 502 |
| HV2 | Hover: > 1 | JumpChild_Hover 516 |
| HV3 | Hover: sound 0x201 | JumpChild_Hover 452 |
| HV4 | Hover: speed 0x41 | JumpChild_Hover 452 |
| HV5 | Hover: step -7 | JumpChild_Hover 451 |
| HV6 | Hover: Ratan2 arguments swapped | JumpChild_Hover 395 |
| HV7 | Hover: heading + 1 | JumpChild_Hover 451 |
| HV8 | Hover: flag the actor | JumpChild_Hover 410 |
| HV9 | Hover: +9 0x11 | JumpChild_Hover 452 |
| HV10 | Hover: dz from +0x3C | JumpChild_Hover 452 |
| HV11 | Hover: PlaySound(3, 0) | JumpChild_Hover 452 |
| HV12 | Hover: dx through the old task pointer | JumpChild_Hover 28 |
| RT1 | Return: Sin of +0x10 | JumpChild_Return 2000 |
| RT2 | Return: x by once | JumpChild_Return 1998 |
| RT3 | Twice: shl 12 sar 11 | JumpChild_Return 1477 |
| RT4 | Return: z at +0x3C | JumpChild_Return 2000 |
| RT5 | Return: x through the task after the call | JumpChild_Return 67 |
| RT6 | Return: grow while +9 below 9 | JumpChild_Return 37 |
| RT7 | Return: grow to 0x11 | JumpChild_Return 162 |
| RT8 | Return: > 1 | JumpChild_Return 228 |
| RT9 | Return: Sin for z | JumpChild_Return 2000 |
| RT10 | Return: no move | JumpChild_Return 2000 |
| P1 | PushMatrix: x >> 8 | JumpChild_PushMatrix 2000 |
| P2 | PushMatrix: z - 0x3FFF | JumpChild_PushMatrix 2000 |
| P3 | PushMatrix: the owner's below 3 | JumpChild_PushMatrix 717 |
| P4 | PushMatrix: height >> 1 | JumpChild_PushMatrix 521 |
| P5 | PushMatrix: source +0x3C | JumpChild_PushMatrix 976 |
| P6 | PushMatrix: angle z 1 | JumpChild_PushMatrix 2000 |
| P7 | PushMatrix: product order | JumpChild_PushMatrix 2000 |
| P8 | PushMatrix: task read before the push | JumpChild_PushMatrix 69 |
| P9 | PushMatrix: no translation set | JumpChild_PushMatrix 2000 |
| DS1 | DrawShadow: tpage 0x56 | JumpChild_DrawShadow 2000 |
| DS2 | DrawShadow: tpage 0x14 | JumpChild_DrawShadow 2000 |
| DS3 | DrawShadow: colour word x 4 | JumpChild_DrawShadow 1922 |
| DS4 | DrawShadow: radius x 4 | JumpChild_DrawShadow 1982 |
| DS5 | DrawShadow: fifteen triangles | JumpChild_DrawShadow 2000 |
| DS6 | DrawShadow: from 0x80 | JumpChild_DrawShadow 2000 |
| DS7 | DrawShadow: rim x / z swapped | JumpChild_DrawShadow 1993 |
| DS8 | DrawShadow: third z 1 | JumpChild_DrawShadow 2000 |
| DS9 | DrawShadow: centre z 1 | JumpChild_DrawShadow 2000 |
| DS10 | DrawShadow: colour from +0 | JumpChild_DrawShadow 1087 |
| DS11 | DrawShadow: rim green 2 | JumpChild_DrawShadow 2000 |
| DS12 | DrawShadow: layer 4 | JumpChild_DrawShadow 2000 |
| DS13 | DrawShadow: first rim z by +2 | JumpChild_DrawShadow 35 |
| DS14 | DrawShadow: opaque | JumpChild_DrawShadow 2000 |
| DS15 | DrawShadow: first commit 0x10 | JumpChild_DrawShadow 2000 |
| DS16 | DrawShadow: second point + 0x1C | JumpChild_DrawShadow 2000 |
| DS17 | DrawShadow: depths at the packet pointer | JumpChild_DrawShadow 853 |
| DS18 | DrawShadow: rim x + 1 | JumpChild_DrawShadow 2000 |
| DS19 | DrawShadow: first Sin of 1 | JumpChild_DrawShadow 2000 |
| DS20 | DrawShadow: blue from +3 | JumpChild_DrawShadow 1995 |
| DS21 | DrawShadow: loop Sin by +2 | JumpChild_DrawShadow 1058 |
| DS22 | DrawShadow: rim read before the calls | JumpChild_DrawShadow 57 |
| DS23 | DrawShadow: z written after the projection's inputs | JumpChild_DrawShadow 2000 |

**Re-run 2026-09-26 on the kFlag-fixed harness ([`magic_harness.md`](magic_harness.md) §8): 26 controls in the affected functions, 26 refused.** Selected: every control whose plant lies in `NueStompChild_Leap`, `_Crouch`, `_Bounce`, `_Stomp` or `_Land` - E1 (EnemyAnimation, in four of them), L1..L5, CR1, CR2, BO1..BO7 (BO1 / BO2 in `TickThrice`, which Bounce and Stomp call), ST1..ST5, LA1..LA6. Skipped: the rest, whose plants lie in none of the five (M1..M3's `Rise` is not called by them). The plants were rebuilt from the table (the original script was not committed) and run by the same plant / rebuild / self-test / restore loop; restored, rebuilt, clean self-test 0 mismatches (exit 0), and `BOF3X_SHADOW='*'` exit 0. Every count is the table's to the round (thinnest ST5 82, ST2 265): these functions reach the flag only through `Sprite_ScriptTickOnce`, whose `effect` answers from the group's own stream and has the last word, so the remixed draw changes nothing they see. No fuzz change.

## 7. What nothing reached

No recorded route casts either spell (queue §5); the live check is the owner
casting them, with a save that has them or DIV-0045's cheat. Things to look
for, by reading:

- Nue Stomp (row 1): a copy of the caster rising, landing on the target's
  place and stomping three times, then fading back to the caster. It plays
  the **acting enemy's** animations through `0x435A70`, so it is written for
  an enemy caster.
- Jump (row 105): the caster's animation 8, a copy rising with a shrinking
  shadow, hovering over the target while the shadow grows, then flying back
  along the heading to the caster and fading.

The harness stands in for everything the functions call, so what the draws
put on screen (the shadow's size and shade, tpages 0x55 / 0x15) is compared
as bytes, not seen.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the four stack tables and the
  two `.data` jumps (`JumpChild_Task`'s one-entry table is followed by other
  data). Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in both starts:
  slot 255 lies past the task pool (`0x93A000 + 255 x 0x84`), and the
  0x80-byte copy and the field writes land there - in ours as in the
  original (the same addresses are written).
- **Row 1 cast by a party member** hands `0x435A70` an actor below 3: it
  indexes the enemy records by actor - 3, unchecked, so it animates a
  "record" 0x128..0x378 bytes below `0x93B960` (among the task slots) as the
  current enemy. By reading; whether any party member can cast row 1 is the
  owner's to say (DIV-0045's cheat would).
- **The starts index the enemy records by the actor byte - 3** above 2,
  unchecked above 10.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 24 lines under a `group S01` comment. Two were
listed already (`00499B10 B8`, and `00499BD0 1B0`, which runs to the next
start through padding; the function's own size is 0x1A5). The host extent
`00498DE0 D22` (the level-up routine's) runs over `0x498FE0..0x499B01`; the
smaller extents fix it at consolidation.
