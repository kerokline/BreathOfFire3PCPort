# Group S03: Mind Sword, Chlorine and Blitz (MAGIC006, 009, 012)

**Status:** IN PROGRESS (2026-09-26). All 44 functions are ours
(`src/game/magic_s03.cpp`, shadow name `magic_s03`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 88,000 rounds. 217 of 219 negative controls refused, every one by a count (exit 3); the two others are equivalent mutants, each with a near variant refused. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S03
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 109 | 0x22F | MAGIC006 | 0x6 | Mind Sword | `0x49C3D0..0x49D831` | 19 |
| 51 | 0x231 | MAGIC009 | 0x9 | Chlorine | `0x49D840..0x49DEEE` | 14 |
| 71 | 0x233 | MAGIC012 | 0xC | Blitz | `0x49E000..0x49E9E2` | 11 |

The extents are `tools/magic_rows.py --unit MAGIC0NN --clones` (capstone
recursive descent; no jump table inside a function, nothing `REFUSED`). All 44
functions lie in the units' extents; none was found inside or missing from
them, and none was ours before. 9,184 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. The queue's row list
(51, 71, 109) is in row order; `Magic_Rows` pairs them with the overlays the
other way round (MAGIC006 is row 109, MAGIC009 row 51, MAGIC012 row 71;
`analysis/magic_rows.tsv`), and the names here follow the pairing. What each
spell looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Mind Sword (MAGIC006).**
  - `MindSword_Task`, the kind-2 task: `MindSword_Start`, then
    `BattleFx_Finish` (the done flag once +0xB, the children, is 0).
  - `MindSword_Start` takes the owner's direction and position and makes four
    blades (kind 1, parameter 0x53, +1 0, +0xB 0..3, +9 1..4).
  - Kind 1 parameter 0x53 is `MindSwordChild_Task`, a jmp through
    `MindSwordChild_Kinds` by +1: the blade, the spark, the flash.
  - The blade (`MindSwordBlade_Run`, six steps by +2): `_Appear` waits +9
    frames, then stands over the owner (height + 0x800000), takes the heading
    to the source sprite (`0x904B4C`) in the plane and on screen, sound 0x100;
    `_Spin` turns its screen angle by 0x80 a frame for 0x20 frames; `_Wait`
    waits +0xB + 12 frames, sound 0x101; `_Fly` steps toward the source
    (`MagicFx_StepTowardPoint`, speed 0x60) until it is near it
    (`MagicFx_NearSprite`, 0x20000) or its heading has swung by more than
    0x600 and less than 0xA00 (it passed the source); the lead blade (+0xB 0)
    then flags the target 0x10. `_Burst`: every blade but the lead frees
    itself; the lead makes eight sparks (+1 1) and a flash (+1 2), all owned
    by the row task, sound 0x102, and ends through MAGIC060's `0x4B1740`.
  - Each frame from step 1 the blade is drawn on layer 3: the lead
    (`_DrawLead`) as a fan of three gouraud quads (tpage 0x55) whose far radii
    are `MindSword_BladeRadii[1..3]`, then two quads (tpage 0x35), the others
    (`_Draw`) as those two quads alone, dimmer.
  - The spark (`MindSwordSpark_*`): starts 12 x (cos, sin) of +0xB x 0x200
    from the source sprite; grows +9 by 0x10 a frame to 0x80 (+0xA on odd
    frames); MAGIC039's `0x4A5180`; drawn as one gouraud quad +0xA wide and +9
    tall above its screen point.
  - The flash (`MindSwordFlash_*`): at the source sprite, +9 up by 4 to 0x10,
    then `BarrierLine_Wait` (group S19) and `0x4B1740`; drawn under
    `MagicFx_PushActorMatrix` as a disc of sixteen gouraud triangles of radius
    0x100 projected by `Gte_RotTransPers3`, its centre (+9 x 8, +9 x 2, 1).
- **Chlorine (MAGIC009).**
  - `Chlorine_Task`: `_Start`, `_Release`, `SpellFx_Countdown` (group S27),
    `_WaitChildren`, `_End`.
  - `Chlorine_Start` sets the acting actor's record +0x4B to 0xFF, plays its
    animation (`BattleActor_SetAnimation(0x2C, 6)`), makes a copy of that
    record's first 0x80 bytes as a kind-1 task (parameter 0x2A, +1 1), sets
    CLUT row 26's words 0x21..0x2F semi-transparent and word 0x20 to 0, and
    sets the owner's +0 bit 0x40.
  - The copy (`ChlorineCopy_*`): its size (`BattleActor_FxSizeB`), its script
    ticked until the end with `BattleActor_PlaySound(2, 5)` when +9 counts to
    0, eight frames, the owner's +0xB down, `BattleFx_FreeTask`.
  - `Chlorine_Release`, once the copy is gone: three clouds (+1 0, +9 1, 5,
    9), `BattleActor_SetAnimation(4, 0)`, the owner's bit 0x40 cleared, sound
    0x100.
  - The cloud (`ChlorineCloud_*`): at `ChlorineCloud_Offsets[+0xB]` turned by
    the source sprite's direction (the engine's `0x446770`) from the source;
    grows +9 and +0xA by 2 to 0x10; MAGIC040's `0x4A5D50`; drawn as one
    textured quad (tpage 0x340 / 0x100, CLUT row 0x1FA) of radius +9 x 2.
  - `Chlorine_WaitChildren` (`0x49DA50`) is also in the stack tables of
    MAGIC039, MAGIC145 and MAGIC146; `ChlorineCloud_Grow` (`0x49DB90`) in nine
    overlays' tables.
  - `Chlorine_End`: the flags word `0x904AA8` |= 0x2004, the target flagged
    0x40, freed.
- **Blitz (MAGIC012).**
  - `Blitz_Task`: `_Start`, `_End`.
  - `Blitz_Start` makes one bolt (kind 1, parameter 0x37) per actor of the
    targeted side that is not out (`Battle_ActorIsOut`): the eight enemies
    (battle index 3..10) when the target byte has 0x40, else the three party
    members; +3 the battle index, +4 the side's index, +9 1, 9, 17, ... It sets
    CLUT row 26 semi-transparent but its first word.
  - The bolt (`BlitzBolt_Task` / `_Run`, twelve steps by +2 through
    `BlitzBolt_Steps`): `_Start` places it 0x20000 in front of the owner;
    `_Seek` steps it (speed 0x40) toward its actor's point at (height + 0xC0) /
    2 until it is near (0x8000): byte `0x904AA9` bit 0x20, the actor flagged
    0x40, sound 0x203, the heading back to the owner; `_Bounce` moves it back
    with a rise and fall for 16 frames; `_Next` waits while its actor's state
    byte +1 is 6 (the reaction state), then launches it again 0x10000 in
    front of the owner. Three rounds, then MAGIC058's `0x4AF490`. If its actor
    is out, `_Seek` sends it to `_Drift` (toward the actor's point for its +9
    frames, then back to `0x4AF490`) and `_Next` straight to `0x4AF490`.
  - Every bolt step draws it (`BlitzBolt_Draw(unused, abr)`): one textured
    quad of radius 0x24 whose corners turn with +0xA, its u one of eight
    32-wide cells by +0xB (Rand & 7 at each launch), blended by `abr` (1 on
    the way back).
  - `Blitz_End`, once every bolt has ended: the acting actor's word +0x98
    (party) / +0xA4 (enemy) halved, at least 1 - the fields symbols.toml's
    `ObjTrio` evidence reads as the party's and the enemies' HP (`0x802DD8`,
    `0x93BA04`) - then the done flag and free.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the
precedented exceptions ([`magic_fx_reached.md`](magic_fx_reached.md) §3, the
round's §7):

- a phase past any of the twelve dispatch tables (seven stack tables, five
  `.data` tables) aborts;
- `ChlorineCloud_Start` aborts on an offset index +0xB past the three entries
  of `ChlorineCloud_Offsets`, where the original reads on into
  `BlitzBolt_TaskTable` and its steps (code pointers as offsets). Its only
  writer, `Chlorine_Release`, sets 0..2.

The fourth word the originals push to `MagicFx_StepTowardPoint` is their own
uninitialised stack; the callee reads none of it, and ours pushes 0.
`Gte_RotTransPers3` gets the flag pointer the original pushes beyond its
prototype.

## 3. Calls to other units

By raw address (a phase in a table; never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4B1740` | MAGIC060 (S12) | entry 5 of `MindSwordBlade_Run`'s table, entry 3 of `MindSwordFlash_Run`'s: +9 down, at 0 the owner's +0xB down and free |
| `0x4A5180` | MAGIC039 (S07) | entry 2 of `MindSwordSpark_Run`'s table |
| `0x4A5D50` | MAGIC040 (S07) | entry 2 of `ChlorineCloud_Steps` |
| `0x4AF490` | MAGIC058 (S11) | entry 10 of `BlitzBolt_Steps`: the owner's +0xB down, the task freed |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S22, S23, S31 call it) |

By name, already ours: `BattleFx_Finish`, `BattleFx_FreeTask`,
`SpellFx_Countdown` (S27), `BarrierLine_Wait` (S19), the effect library
(`MagicFx_StepTowardPoint`, `MagicFx_NearSprite`, `BattleActor_FxSizeB`,
`BattleActor_UpdateScreenXY`, `BattleActor_SetAnimation`,
`BattleActor_PlaySound`, `MagicFx_PushActorMatrix`), `Battle_ActorIsOut`, and
the GTE / GPU / sprite / sound library.

Shared bodies: `magic_funcs.tsv` has eleven files reaching Mind Sword's child
task (`0x49C4C0..`, kind 1 parameter 0x53), MAGIC039 / 145 / 146 reaching
`Chlorine_WaitChildren` and nine files `ChlorineCloud_Grow`; group C1's
`Ink_Task`, `InkInk_Task`, `InkPuff_Run` and `InkInkPuff_Run` call the last
two by raw address. They reach them through the addresses, so taking them
changes nothing for the callers.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `MindSwordChild_Kinds` | `0x65A5E0` | 3 |
| `MindSword_BladeRadii` | `0x65A5EC` | 4 words, [1..3] read |
| `ChlorineChild_Kinds` | `0x65A5F4` | 2 |
| `ChlorineCloud_Steps` | `0x65A5FC` | 3 |
| `ChlorineCloud_Offsets` | `0x65A608` | 3 (dx, dz, dy) |
| `BlitzBolt_TaskTable` | `0x65A62C` | 1 |
| `BlitzBolt_Steps` | `0x65A630` | 12 |

Each count is where the next table starts (the dump of `0x65A5D0..0x65A6BC`
read 2026-09-26; `BlitzBolt_Steps` entry 11, `BlitzBolt_Drift`, is reached
only by `BlitzBolt_Seek` setting +2 to 11, and MAGIC013's table starts at
`0x65A660`). Ours reads the radii and the offsets in place.

## 5. The fuzz

`BOF3X_SHADOW=magic_s03` runs `magic_harness::Run` over the 44 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s03_fuzz.cpp`:

- **Callees** (24 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own - every primitive of a draw is built at the same pointer, so without
    the log only the last would be compared; `Gte_RotTransPers3` logs its
    three SVECTORs through `deref`; `Math_Sin` / `Math_Cos` / `Math_Ratan2`
    and the libgpu setters are recorded (arguments logged, answers garbage);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - `MagicFx_NearSprite` answers `kBool` (the standard set's `kFlag` leaves
    garbage above al, and both callers here test the whole of eax: with it
    the "not near" branch never ran - found by controls M25..M30 not being
    refused on the first run);
  - **the kFlag blind spot, worked round**: `Battle_ActorIsOut`,
    `MagicFx_NearSprite` and `Sprite_ScriptTickOnce` (the flag callees
    whose callers read state again after the answer) each note a marker and
    call `Stir()`: the note grows the log, so the second disturbance comes
    from a new hash, independent of the answer - a "not out" is followed by
    a moved cell two times in three;
  - `Battle_ActorIsOut`, while `BlitzBolt_Seek` is fuzzed, also moves the
    bolt's record's point half the time (the out branch reads it again after
    the call; the harness writes a record only as the target enemy's), and
    `Math_Cos`, while `MindSwordSpark_Start` is, the angle word `0x903858`
    the sine reads back - controls B44 and M57, not refused before;
  - `Math_Ratan2`, while `MindSwordBlade_Fly` is fuzzed, answers half the
    time 0x5FF, 0x600, 0x601, 0x9FF, 0xA00 or 0xA01 from the heading kept in
    +0x10 (either way), so the swing test's two bounds are met;
  - this group's own draws, and `BlitzBolt_Draw` with its two words (the
    first masked: it is never read), by address (their callers call them
    directly).
- **Tables:** the five `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; CLUT row 26 and its
  source. 20,628 bytes of state in 14 regions.
- **Settle:** while `BlitzBolt_Seek` / `_Next` are fuzzed, the bolt's record
  index +4 is kept inside its side's records (8 enemies, 5 party records in
  the harness); while `ChlorineCloud_Start` is, its +0xB inside 0..2. Both are
  read again after a call, and past them the original reads outside the image
  (the enemy records by a byte) or its table.
- **Seed:** each dispatcher inside its table; each count-down one step before
  and at its threshold (+9 at 1, `_Grow`'s 0x70 / 0xC / 0xE); +0xB 0 half
  the time for the waits on children and the lead blade; the copy's +9 at
  0xFF or 1; the target byte's side bit half the time for Blitz, and the
  bolt's actor in the reaction state half the time for `_Next`;
  `Blitz_End`'s HP word 0..3 half the time; `BlitzBolt_Draw`'s blend 0 / 1
  two times in three (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte, the task's words +0x10 / +0x14 / +0x16 / +0x20 / +0x2E / +0x30 /
  +0x3E, the target's side bit, an actor's state byte (6 or not), a word of
  the harness's sprite records (the source sprite's point).

Result in this worktree (2026-09-26):

    shadow      magic_s03 self-test: 88000 rounds over 44 functions (2000 each), 578898 calls to the stand-ins,
                0 MISMATCHES; 20628 bytes of state (14 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (578,345 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

219 plants, each put in `magic_s03.cpp` one at a time by a script (not
committed) that planted, rebuilt, checked the build had recompiled the file,
ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s03`, restored; after the last
it restored, rebuilt and ran the clean self-test (0 mismatches). **217 of 219
refused**, every one by exit 3 with a count only in the functions the plant
touches (M-, C-, B- are Mind Sword, Chlorine, Blitz; H- the shared helpers).
The table is the final run, after the fuzz changes of section 5.

Two equivalent mutants, not refused, each with its near variant refused:

- **B7** (`Blitz_Start`'s CLUT loop from word 1): the original then writes
  word 0 from its source without the STP bit, so the loop's word 0 is always
  overwritten. Near variant **B8** (the loop to word 0xFE) refused.
- **H4** (`BladeQuad` without the dead store of 0x400 to the angle word): the
  next vertex writes the angle word before any call reads it. Near variant
  **H5** (the store into the centre word) refused.

The thinnest (fewer than 60 rounds):

- **C18** (`Chlorine_WaitChildren` at +0xB 1): 3
- **H3** (the blade vertices' cosine angle not read back): 22 in
  `MindSwordBlade_DrawLead`, 7 in `MindSwordBlade_Draw`
- **B64** (`BlitzBolt_Draw`'s sine angle not read back): 13
- **M18** (`MindSwordBlade_Appear`'s source read before the update): 15
- **M26** (`MindSwordBlade_Fly`'s swing bound at 0xA00): 44

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| M1 | MindSword_Task: entries swapped | MindSword_Task 2000 |
| M2 | Start: height from +0x38 | MindSword_Start 2000 |
| M3 | Start: three blades | MindSword_Start 2000 |
| M4 | Start: blade +9 i + 2 | MindSword_Start 2000 |
| M5 | Start: blade +1 1 | MindSword_Start 2000 |
| M6 | Start: parameter 0x54 | MindSword_Start 2000 |
| M7 | Child_Task: spark / flash swapped | MindSwordChild_Task 1304 |
| M8 | Blade_Run: Wait / Fly swapped | MindSwordBlade_Run 681 |
| M9 | Blade_Run: lead draw for +0xB 1 too | MindSwordBlade_Run 229 |
| M10 | Blade_Run: no screen update | MindSwordBlade_Run 865 |
| M11 | Appear: height + 0x800001 | MindSwordBlade_Appear 535 |
| M12 | Appear: first heading's arguments swapped | MindSwordBlade_Appear 413 |
| M13 | Appear: angle - 0x1FF | MindSwordBlade_Appear 549 |
| M14 | Appear: angle mask 0x7FF | MindSwordBlade_Appear 278 |
| M15 | Appear: sound 0x101 | MindSwordBlade_Appear 549 |
| M16 | Appear: +9 0x21 | MindSwordBlade_Appear 549 |
| M17 | Appear: dy from +0x2E | MindSwordBlade_Appear 549 |
| M18 | Appear: source read before the update | MindSwordBlade_Appear 15 |
| M19 | Spin: angle + 0x81 | MindSwordBlade_Spin 2000 |
| M20 | Spin: +9 +0xB + 13 | MindSwordBlade_Spin 484 |
| M21 | Wait: sound 0x102 | MindSwordBlade_Wait 509 |
| M22 | Fly: speed 0x61 | MindSwordBlade_Fly 2000 |
| M23 | Fly: height sar 16 | MindSwordBlade_Fly 2000 |
| M24 | Fly: near 0x20001 | MindSwordBlade_Fly 2000 |
| M25 | Fly: turned at 0x600 | MindSwordBlade_Fly 62 |
| M26 | Fly: turned at 0xA00 | MindSwordBlade_Fly 44 |
| M27 | Fly: near, lead +0xB 1 | MindSwordBlade_Fly 594 |
| M28 | Fly: turned, flags 0x20 | MindSwordBlade_Fly 82 |
| M29 | Fly: no negate | MindSwordBlade_Fly 330 |
| M30 | Fly: kept heading mask 0x7FF | MindSwordBlade_Fly 335 |
| M31 | Fly: +0x10 not kept | MindSwordBlade_Fly 2000 |
| M32 | Fly: near the task | MindSwordBlade_Fly 2000 |
| M33 | Burst: own +0xB down | MindSwordBlade_Burst 891 |
| M34 | Burst: seven sparks | MindSwordBlade_Burst 997 |
| M35 | Burst: +9 Rand & 7 | MindSwordBlade_Burst 974 |
| M36 | Burst: flash +1 3 | MindSwordBlade_Burst 997 |
| M37 | Burst: sound 0x103 | MindSwordBlade_Burst 997 |
| M38 | Burst: +9 0x11 | MindSwordBlade_Burst 997 |
| M39 | Burst: sparks owned by the blade | MindSwordBlade_Burst 943 |
| M40 | DrawLead: tpage 0x56 | MindSwordBlade_DrawLead 2000 |
| M41 | DrawLead: two fan quads | MindSwordBlade_DrawLead 2000 |
| M42 | DrawLead: radius index - 1 | MindSwordBlade_DrawLead 2000 |
| M43 | DrawLead: far angle sl 8 | MindSwordBlade_DrawLead 2000 |
| M44 | DrawLead: near radius the far word | MindSwordBlade_DrawLead 2000 |
| M45 | DrawLead: fan tip shade 0x41 | MindSwordBlade_DrawLead 2000 |
| M46 | DrawLead: fan on layer 2 | MindSwordBlade_DrawLead 2000 |
| M47 | DrawLead: first quad centre 0xE1 | MindSwordBlade_DrawLead 1992 |
| M48 | DrawLead: second quad centre 0x81 | MindSwordBlade_DrawLead 2000 |
| M49 | DrawLead: near radius 9 | MindSwordBlade_DrawLead 1999 |
| M50 | Draw: inner shade 0x31 | MindSwordBlade_Draw 2000 |
| M51 | Draw: near radius 7 | MindSwordBlade_Draw 1997 |
| M52 | Draw: tpage 0x34 | MindSwordBlade_Draw 2000 |
| M53 | Spark_Run: Start / Grow swapped | MindSwordSpark_Run 1323 |
| M54 | Spark_Run: no screen update | MindSwordSpark_Run 682 |
| M55 | Spark_Start: angle sl 8 | MindSwordSpark_Start 499 |
| M56 | Spark_Start: x 5 | MindSwordSpark_Start 502 |
| M57 | Spark_Start: sine angle not read back | MindSwordSpark_Start 257 |
| M58 | Spark_Start: +0xA 5 | MindSwordSpark_Start 502 |
| M59 | Spark_Start: height from +0x38 | MindSwordSpark_Start 502 |
| M60 | Spark_Grow: frame bit 1 | MindSwordSpark_Grow 1019 |
| M61 | Spark_Grow: at 0x70 | MindSwordSpark_Grow 537 |
| M62 | Spark_Draw: tpage 0x36 | MindSwordSpark_Draw 2000 |
| M63 | Spark_Draw: first link offset 1 | MindSwordSpark_Draw 2000 |
| M64 | Spark_Draw: top y by the width | MindSwordSpark_Draw 1988 |
| M65 | Spark_Draw: last x left | MindSwordSpark_Draw 1992 |
| M66 | Spark_Draw: bottom green 0x21 | MindSwordSpark_Draw 2000 |
| M67 | Spark_Draw: width from +9 | MindSwordSpark_Draw 1987 |
| M68 | Spark_Draw: second link 0x48 | MindSwordSpark_Draw 2000 |
| M69 | Flash_Run: steps 2 / 3 swapped | MindSwordFlash_Run 1034 |
| M70 | Flash_Run: no pop | MindSwordFlash_Run 808 |
| M71 | Flash_Start: +9 1 | MindSwordFlash_Start 2000 |
| M72 | Flash_Grow: + 3 | MindSwordFlash_Grow 2000 |
| M73 | Flash_Grow: at 0xC | MindSwordFlash_Grow 504 |
| M74 | Flash_Draw: tpage 0x36 | MindSwordFlash_Draw 2000 |
| M75 | Flash_Draw: radius 0x101 | MindSwordFlash_Draw 1999 |
| M76 | Flash_Draw: red sl 2 | MindSwordFlash_Draw 1989 |
| M77 | Flash_Draw: blue 2 | MindSwordFlash_Draw 1995 |
| M78 | Flash_Draw: fifteen triangles | MindSwordFlash_Draw 2000 |
| M79 | Flash_Draw: rim z 1 | MindSwordFlash_Draw 1999 |
| M80 | Flash_Draw: last point swapped | MindSwordFlash_Draw 2000 |
| M81 | Flash_Draw: last rim 2 | MindSwordFlash_Draw 2000 |
| M82 | Flash_Draw: commit 0x30 | MindSwordFlash_Draw 2000 |
| M83 | Flash_Draw: closing tpage 0x16 | MindSwordFlash_Draw 2000 |
| M84 | Flash_Draw: projected v1 / v2 swapped | MindSwordFlash_Draw 2000 |
| M85 | Flash_Draw: green the red byte | MindSwordFlash_Draw 1986 |
| M86 | Flash_Draw: 0x9037AC not cleared | MindSwordFlash_Draw 1997 |
| C1 | Chlorine_Task: entries 2 / 3 swapped | Chlorine_Task 786 |
| C2 | Start: record +0x4A | Chlorine_Start 2000 |
| C3 | Start: animation 7 | Chlorine_Start 2000 |
| C4 | Start: parameter 0x2B | Chlorine_Start 2000 |
| C5 | Start: 0x7C bytes copied | Chlorine_Start 2000 |
| C6 | Start: actor read before the create | Chlorine_Start 73 |
| C7 | Start: copy +6 2 | Chlorine_Start 2000 |
| C8 | Start: copy +5 0x2B | Chlorine_Start 2000 |
| C9 | Start: CLUT words to 0x2E | Chlorine_Start 2000 |
| C10 | Start: CLUT word 0x20 1 | Chlorine_Start 2000 |
| C11 | Start: owner bit 0x20 | Chlorine_Start 1523 |
| C12 | Start: copy +2 1 | Chlorine_Start 2000 |
| C13 | Release: +9 4i + 2 | Chlorine_Release 1022 |
| C14 | Release: animation 5 | Chlorine_Release 1022 |
| C15 | Release: owner mask 0x3F | Chlorine_Release 469 |
| C16 | Release: +9 9 | Chlorine_Release 1022 |
| C17 | Release: two clouds | Chlorine_Release 1022 |
| C18 | WaitChildren: at 1 | Chlorine_WaitChildren 3 |
| C19 | End: flags 0x2005 | Chlorine_End 902 |
| C20 | End: target + 1 | Chlorine_End 2000 |
| C21 | Child_Task: kinds swapped | ChlorineChild_Task 2000 |
| C22 | Cloud_Run: Start / Grow swapped | ChlorineCloud_Run 1346 |
| C23 | Cloud_Run: drawn with +2 0 | ChlorineCloud_Run 353 |
| C24 | Cloud_Start: direction from +9 | ChlorineCloud_Start 526 |
| C25 | Cloud_Start: dz the dx | ChlorineCloud_Start 347 |
| C26 | Cloud_Start: dy the dz | ChlorineCloud_Start 349 |
| C27 | Cloud_Start: x from the source's z | ChlorineCloud_Start 532 |
| C28 | Cloud_Start: +0xA 1 | ChlorineCloud_Start 532 |
| C29 | Cloud_Start: offset stride 8 | ChlorineCloud_Start 352 |
| C30 | Cloud_Start: turned with the owner | ChlorineCloud_Start 463 |
| C31 | Cloud_Grow: +0xA + 3 | ChlorineCloud_Grow 2000 |
| C32 | Cloud_Grow: at 0xE | ChlorineCloud_Grow 507 |
| C33 | Cloud_Draw: radius sl 2 | ChlorineCloud_Draw 1992 |
| C34 | Cloud_Draw: corners 3 / 4 swapped | ChlorineCloud_Draw 2000 |
| C35 | Cloud_Draw: blend 2 | ChlorineCloud_Draw 2000 |
| C36 | Cloud_Draw: CLUT row 0x1FB | ChlorineCloud_Draw 2000 |
| C37 | Cloud_Draw: u3 0xA1 | ChlorineCloud_Draw 2000 |
| C38 | Cloud_Draw: red x 11 | ChlorineCloud_Draw 1997 |
| C39 | Cloud_Draw: blue x 8 | ChlorineCloud_Draw 1997 |
| C40 | Cloud_Draw: second link offset 1 | ChlorineCloud_Draw 2000 |
| C41 | Cloud_Draw: y from the cosine | ChlorineCloud_Draw 2000 |
| C42 | Copy_Task: Play / Wait swapped | ChlorineCopy_Task 993 |
| C43 | Copy_Task: update by +2 | ChlorineCopy_Task 972 |
| C44 | Copy_Size: size + 1 | ChlorineCopy_Size 2000 |
| C45 | Copy_Play: stop at 0xFE | ChlorineCopy_Play 176 |
| C46 | Copy_Play: sound (2, 6) | ChlorineCopy_Play 488 |
| C47 | Copy_Play: +9 7 | ChlorineCopy_Play 1376 |
| C48 | Copy_Wait: own +0xB down | ChlorineCopy_Wait 438 |
| B1 | Blitz_Task: entries swapped | Blitz_Task 2000 |
| B2 | Start: seven enemies | Blitz_Start 1030 |
| B3 | Start: battle index i + 2 | Blitz_Start 1030 |
| B4 | Start: +4 the battle index | Blitz_Start 991 |
| B5 | Start: shade step 7 | Blitz_Start 1083 |
| B6 | Start: first shade 0 | Blitz_Start 1678 |
| B7 | Start: CLUT row from word 1 (equivalent) | not refused: equivalent (see below) |
| B8 | Start: CLUT row to word 0xFE | Blitz_Start 2000 |
| B9 | Start: first word with STP | Blitz_Start 1040 |
| B10 | Start: parameter 0x38 | Blitz_Start 1678 |
| B11 | Start: out test inverted | Blitz_Start 2000 |
| B12 | Start: side bit 0x80 | Blitz_Start 1030 |
| B13 | End: party word +0x9A | Blitz_End 634 |
| B14 | End: enemy word +0xA6 | Blitz_End 407 |
| B15 | End: a quarter | Blitz_End 526 |
| B16 | End: floor 2 | Blitz_End 252 |
| B17 | End: party below 2 | Blitz_End 209 |
| B18 | End: done bit 3 | Blitz_End 754 |
| B19 | Bolt_Task: wrong handler | BlitzBolt_Task 2000 |
| B20 | Bolt_Run: Seek / Bounce swapped | BlitzBolt_Run 352 |
| B21 | Bolt_Run: free / Drift swapped | BlitzBolt_Run 327 |
| B22 | Start: launch 0x20001 | BlitzBolt_Start 491 |
| B23 | Next: launch 0x10001 | BlitzBolt_Next 322 |
| B24 | Launch: height + 0x101 | BlitzBolt_Start 492, BlitzBolt_Next 325 |
| B25 | Launch: +0xB Rand & 3 | BlitzBolt_Start 240, BlitzBolt_Next 163 |
| B26 | Launch: +9 0x11 | BlitzBolt_Start 492, BlitzBolt_Next 325 |
| B27 | Launch: z from the owner's x | BlitzBolt_Start 492, BlitzBolt_Next 325 |
| B28 | Launch: direction from the owner's +9 | BlitzBolt_Start 486, BlitzBolt_Next 325 |
| B29 | Seek: frame bit 1 | BlitzBolt_Seek 928 |
| B30 | Seek: aim height + 0xC1 | BlitzBolt_Seek 993 |
| B31 | Seek: out height sar 2 | BlitzBolt_Seek 606 |
| B32 | Seek: out step 10 | BlitzBolt_Seek 1317 |
| B33 | Seek: speed 0x41 | BlitzBolt_Seek 2000 |
| B34 | Seek: near 0x8001 | BlitzBolt_Seek 2000 |
| B35 | Seek: 0x904AA9 bit 0x40 | BlitzBolt_Seek 1036 |
| B36 | Seek: sound 0x204 | BlitzBolt_Seek 1370 |
| B37 | Seek: fall -7 | BlitzBolt_Seek 1368 |
| B38 | Seek: rise 0x41 | BlitzBolt_Seek 1369 |
| B39 | Seek: drawn with blend 1 | BlitzBolt_Seek 2000 |
| B40 | Seek: party index mod 3 | BlitzBolt_Seek 398, BlitzBolt_Next 62 |
| B41 | Seek: out test of +4 | BlitzBolt_Seek 1995 |
| B42 | Seek: x - 0x3FFF | BlitzBolt_Seek 2000 |
| B43 | Seek: heading's arguments swapped | BlitzBolt_Seek 1182 |
| B44 | Seek: out point not read again | BlitzBolt_Seek 266 |
| B45 | Bounce: x step sl 14 | BlitzBolt_Bounce 2000 |
| B46 | Bounce: z by the sine | BlitzBolt_Bounce 2000 |
| B47 | Bounce: rise less the fall | BlitzBolt_Bounce 2000 |
| B48 | Bounce: height by the rise's high word | BlitzBolt_Bounce 1999 |
| B49 | Bounce: drawn with blend 0 | BlitzBolt_Bounce 2000 |
| B50 | Bounce: x's task read after the call | BlitzBolt_Bounce 62 |
| B51 | Bounce: +0xA down | BlitzBolt_Bounce 1956 |
| B52 | Next: out step 9 | BlitzBolt_Next 1373 |
| B53 | Next: waits on state 5 | BlitzBolt_Next 306 |
| B54 | Next: waits on +2 | BlitzBolt_Next 304 |
| B55 | Drift: speed 0x31 | BlitzBolt_Drift 2000 |
| B56 | Drift: +2 on | BlitzBolt_Drift 470 |
| B57 | Drift: frame bit 1 | BlitzBolt_Drift 999 |
| B58 | Drift: z the x | BlitzBolt_Drift 2000 |
| B59 | Draw: tpage | 0x14 | BlitzBolt_Draw 2000 |
| B60 | Draw: radius 0x25 | BlitzBolt_Draw 1999 |
| B61 | Draw: shade sl 2 | BlitzBolt_Draw 1988 |
| B62 | Draw: last corner + 6 | BlitzBolt_Draw 2000 |
| B63 | Draw: angle mask 7 | BlitzBolt_Draw 1990 |
| B64 | Draw: sine angle not read back | BlitzBolt_Draw 13 |
| B65 | Draw: tpage y 0x101 | BlitzBolt_Draw 2000 |
| B66 | Draw: CLUT x 1 | BlitzBolt_Draw 2000 |
| B67 | Draw: u1 + 0x1E | BlitzBolt_Draw 2000 |
| B68 | Draw: v3 0x41 | BlitzBolt_Draw 2000 |
| B69 | Draw: blue the next byte | BlitzBolt_Draw 1990 |
| B70 | Draw: semi-transparency 1 | BlitzBolt_Draw 1300 |
| B71 | Draw: x centred on the y | BlitzBolt_Draw 2000 |
| B72 | Draw: blend masked 0x7F | BlitzBolt_Draw 335 |
| H1 | LinkAtSprite: x / z swapped | MindSwordSpark_Draw 2000, ChlorineCloud_Draw 2000, BlitzBolt_Draw 2000 |
| H2 | Row26Stp: bit 0x4000 | Chlorine_Start 2000, Blitz_Start 2000 |
| H3 | AngleVertex: cosine angle not read back | MindSwordBlade_DrawLead 22, MindSwordBlade_Draw 7 |
| H4 | BladeQuad: dead store dropped (equivalent) | not refused: equivalent (see below) |
| H5 | BladeQuad: the dead store into the centre word | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H6 | BladeQuad: outer radius 0x41 | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H7 | BladeQuad: second radius 0x39 | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H8 | BladePair: first angle 0x231 | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H9 | BladePair: second angle 0x1D1 | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H10 | BladeQuad: centre y the x | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H11 | BladeQuad: commit 0x40 | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |
| H12 | PutShades: channels reversed | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000, MindSwordSpark_Draw 2000 |
| H13 | AngleVertex: y centred on the x | MindSwordBlade_DrawLead 2000, MindSwordBlade_Draw 2000 |

**Re-run 2026-09-26 on the kFlag-fixed harness ([`magic_harness.md`](magic_harness.md) §8): 51 controls in the affected functions, 50 refused, the fifty-first the known equivalent.** Section 8 lists `BlitzBolt_Next`, `BlitzBolt_Seek`, `Blitz_Start`, `ChlorineCopy_Play` and `MindSwordBlade_Fly`. Selected: M22..M32 (`_Fly`), C45..C47 (`ChlorineCopy_Play`), B2..B12 and H2 (`Blitz_Start`; H2's `Row26Stp` is shared with `Chlorine_Start`), B23..B28 (`_Next`, and `LaunchBolt` shared with `BlitzBolt_Start`), B29..B44 (`_Seek`; B40's `BoltRecord` also `_Next`), B52..B54 (`_Next`). Skipped: every other control, which plants only in functions whose originals call no `kFlag` / `kBool` recorder - among them B22 (`BlitzBolt_Start` alone) and B20 / B21 (`BlitzBolt_Run`, whose clone reaches `_Seek` / `_Next` only as handler recorders). Rebuilt from the table (the scripts were never committed) and run by the same plant / rebuild / self-test / restore loop. B7 is again not refused (the equivalent above) and B8, its near variant, is refused in 2,000. The thinnest: M26 30, B40 63 in `_Next` (398 in `_Seek`), M25 69, M28 93, C45 186, B25 191 in `_Next`; the rest as the table or near it (e.g. B23 373, B53 282, B54 279, M29 / M30 349). No fuzz change. After the last: restored, rebuilt, clean self-test exit 0, 0 mismatches.

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Things
to look for:

- Mind Sword: four blades rising over the caster, spinning, flying at the
  target; a burst of sparks and a flash where the first arrives.
- Chlorine: the caster's copy playing an animation, then three clouds
  opening round the source sprite.
- Blitz: one bolt per live actor of the targeted side, each flying out and
  back three times; the caster's HP halved at the end.

`BlitzBolt_Drift` (entry 11) is reached only when a bolt's actor is out by
the time it seeks it; `MindSwordBlade_Fly`'s swing test only when a blade
passes the source without meeting `MagicFx_NearSprite`'s box.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96, D97 and D104 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **`BlitzBolt_Seek` can leave +2 past `BlitzBolt_Steps`.** When the bolt's
  actor is out it sets +2 to 11 (`BlitzBolt_Drift`) and goes on to its step
  and near test; if the bolt is also within 0x8000 of that actor that frame,
  the near branch runs too (the out actor flagged 0x40, sound 0x203) and
  moves +2 on to 12. The next frame `BlitzBolt_Run` jumps through
  `BlitzBolt_Steps[12]`, which is MAGIC013's table's first entry (`0x65A660`,
  `0x49EBE0`): another overlay's function run as this bolt's step. Ours
  aborts there (the phase check). Whether a bolt can meet an actor in the
  frame it is found out is not measured: a party member or enemy knocked out
  by an earlier bolt while this one is on its way is the likely case.
- **Every dispatcher's index is unchecked**: the seven stack tables and the
  five `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `MindSword_Start`, `MindSwordBlade_Burst` (nine calls), `Chlorine_Start`,
  `Chlorine_Release` and `Blitz_Start`: slot 255 is
  `0x93A000 + 255 x 0x84 = 0x9423FC`, past the image's end - an access
  violation, in ours as in the original (the same address is written;
  `Chlorine_Start`'s copy of 0x80 bytes goes there first).
- **The records are indexed unchecked**: `Chlorine_Start` and `Blitz_End`
  by the acting actor byte (the enemies' by the byte - 3), the bolts by +4
  (set 0..7 / 0..2 by `Blitz_Start`); `ChlorineCloud_Start`'s offsets by +0xB
  (ours aborts past 2, section 2).

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 43 lines under a `group S03` comment: 39 new,
plus four host extents re-listed smaller (`0049CF10 3EC`, `0049D430 181`,
`0049D680 1B2`, `0049E720 2C3`, each the function's own size). One was listed
right already (`0049C910 600`).
