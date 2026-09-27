# Group S31: Doom Breath, Corona, Main Cannon and Thunder Clap (MAGIC132, 137, 138, 143)

**Status:** IN PROGRESS (2026-09-26). All 51 functions are ours
(`src/game/magic_s31.cpp`, shadow name `magic_s31`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 102,000 rounds. 197 of 197 negative controls refused, every one by a count (exit 3). Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, second spell wave, group S31
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6a).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 139 | 0x292 | MAGIC132 | 0x84 | Doom Breath | `0x4E6950..0x4E7413` | 15 |
| 101 | 0x297 | MAGIC137 | 0x89 | Corona | `0x4E7420..0x4E7FE0` | 15 |
| 118 | 0x298 | MAGIC138 | 0x8A | Main Cannon | `0x4E7FF0..0x4E8656` | 11 |
| 66 | 0x299 | MAGIC143 | 0x8F | Thunder Clap | `0x4E8660..0x4E9138` | 10 |

The extents are `tools/magic_rows.py --unit MAGIC1NN --clones` (capstone
recursive descent; one jump table, in `CoronaRay_PushMatrix`, moved
into the copy by the harness; nothing `REFUSED`). All 51 functions lie in the
units' extents; none was found inside or missing from them, and none was ours
before. 9,832 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. Note the row order: the
queue lists the rows 66, 101, 118, 139 against the overlays in file order, but
`Magic_Rows` pairs them the other way round (MAGIC132 is row 139, MAGIC143 is
row 66; `analysis/magic_rows.tsv`). What each spell looks like in play has not
been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Doom Breath (MAGIC132).**
  - `DoomBreath_Task`, the kind-2 task: a five-entry stack table by +1.
  - `DoomBreath_Start` centres the task on the side (`MagicFx_CenterOnSide`),
    makes one orb (kind 1, parameter 0x64), restores CLUT row 26 with its
    semi-transparency bits set, tints the acting actor's sprite (the record
    at `0x904B3C`) black and keeps the tint slot in +0xA, plays sound 0x100.
  - `DoomBreath_Brighten` / `_Fade` step that tint record's three channels up
    for 16 frames (then the sprite's +0x29 = 2) and down again (then the tint
    released and the actor flashed).
  - Entry 3 is MAGIC226/227's `0x4F9F70` (+9 down, then target flags 0x10).
  - `DoomBreath_End` waits for the orb's signal (+0xB = 0xFF), sets the
    sprite's +0x29 = 4, flags the target 0x40 and ends the effect.
  - The orb (`DoomBreathOrb_Task` / `_Run`, five steps by +2 through
    `DoomBreathOrb_Steps`): `_Launch` waits, then starts at an offset from
    the field's kind-2 point turned by the owner's direction (the engine's
    `0x446770`), above the owner; `_Move` travels 0x2C frames; `_Grow` widens
    to 0x80; `_Shrink` narrows to 0x20; `_End` closes and signals the parent.
  - Each frame from step 1: the orb's matrix (`_PushMatrix`, turned about z
    by the direction), a stream of 63 textured quads up a sine
    (`_DrawStream`) and a two-quad gouraud glow (`_DrawGlow`).
- **Corona (MAGIC137).**
  - `Corona_Task`: `Corona_Start`, `Corona_Wait` (target flags 0x20 after 24
    frames), `Corona_End` (once both children have ended).
  - `Corona_Start` makes two children of kind 1, parameter 0x4B: the ray (+1
    0, the owner's direction) and the flash (+1 1), and restores CLUT row 26
    with its STP bits. **In an event battle** (`0x904AAA`) **whose acting
    enemy's byte +0x100 is 0x29** it makes a third: a copy of that enemy's
    record's first 0x80 bytes, which plays its animation 3 until the effect's
    done flag (`CoronaEnemy_*`, the engine's `0x43EC10`, `BattleFx_FreeTask`).
  - The ray (`CoronaRay_*`): starts 0x28000 behind the kind-2 point along the
    direction, on the ground there + 0x200 (`AreaMap_Elevation`), sound
    effect 0x100; MAGIC067's two steps; `_Advance` moves it 0x2000 a frame
    and frees it when +9 has counted down to 0x30. It draws a fan of up to
    +9 - 1 textured gouraud quads between two arcs (angles 0x580 and 0x280)
    of growing radius, lifted by a sine, shaded up over the first 16 and down
    over the last 8, its texture scrolling with the frame counter.
  - The flash (`CoronaFlash_*`): one semi-transparent gouraud quad over the
    whole screen, bright on the side the owner faces; its steps are
    `_Start` and three other groups' (`BarrierRing_Grow`, `MagicFx_WaitA`,
    MAGIC060's `0x4B1740`).
- **Main Cannon (MAGIC138).**
  - `MainCannon_Task`: `_Start` (the screen point, the first 16 words of CLUT
    row 26 back, the owner's script ticked), `_Fire` (a shell of kind 1,
    parameter 0x56, every eighth frame, six in all; the owner's script
    ticked), `_End` (once every shell has ended).
  - The shells and blasts share one kind-1 task (`MainCannonChild_Task`, two
    entries by +1). The shell (`MainCannonShell_*`, with the frame-offset
    table `0x9039D8` the effects' `0x8E3580`) aims at the **acting** actor
    (`0x904B34`: a party record at 0..2, else the enemy's), flies in
    sixteenths of a pixel until it is 0x14 (party) or 0x28 (enemy) above it,
    and leaves a blast; the blast (`MainCannonBlast_*`, table `0x8C5D80`)
    plays animation 5 and a sound effect, then MAGIC058's `0x4AF490`.
- **Thunder Clap (MAGIC143)** is Lightning (group S22) with one bolt.
  - `ThunderClap_Start`: one bolt (kind 1, parameter 0x35) at the source
    sprite (`0x904B4C`), +0xB 1, sound 0x100; entry 1 is MAGIC131's
    `0x4E5200` (the done flag once +0xB is 0).
  - `ThunderClapBolt_Run` is `LightningBolt_Run`'s shape: steps `_Start`,
    `_Rise` (the source sprite darkened, target flags 0x10), `_Fade` (target
    flag 0x40, the tint released, the target flashed) and S22's
    `JoltBolt_End`; under S22's `LightningBolt_PushMatrix`, three band rows,
    one arc, two flashes 16 pixels either side.
  - `ThunderClapBolt_DrawBand` is `Myollnir_DrawBand` (S22) with 23 steps
    0x30 apart, the first radius half the argument, the shades +0xA x 8 and
    x 13 (kept in `Scratch_Swap` and read back at every use), and no
    screen-top test; `_DrawArcs` is `MyollnirRing_DrawArcs` with the swing
    0xA0 +- `Rand & 0x1F`; `_DrawFlash` is `Myollnir_DrawFlash` with the
    radius `Rand & 7` + +0xA x 4 and the centre +0xA x (8, 8, 6).

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedents:

- a phase past any of the eighteen dispatch tables (ten stack tables, eight
  `.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3);
- `CoronaRay_PushMatrix` aborts on a direction byte past 3, where the
  original's four-entry jump table skips the store and turns the ray by an
  uninitialised stack word (group S25's `SpellConfuse_PushFacingMatrix`
  precedent). Whether a live ray can have such a direction is not measured:
  its +8 is written only by `Corona_Start`, from the owner's +8.

Calls that push one argument more than the callee takes, as the originals do,
push it in ours too: `Gte_RotTrans` gets a flag pointer, the projections a
depth and a flag pointer.

## 3. Calls to other units

By raw address (a phase in a table; never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4F9F70` | MAGIC226/227 (S38) | entry 3 of `DoomBreath_Task`: +9 down, at 0 target flags 0x10 and +1 on |
| `0x4E5200` | MAGIC131 (S30) | entry 1 of `ThunderClap_Task`: the done flag and free once +0xB is 0 |
| `0x4B6B20`, `0x4B6B70` | MAGIC067 (S15) | entries 1 and 2 of `CoronaRay_Steps` |
| `0x4B1740` | MAGIC060 (S12) | entry 3 of `CoronaFlash_Steps` |
| `0x4AF490` | MAGIC058 (S11) | entry 2 of `MainCannonBlast_Run`: the owner's +0xB down, the task freed |
| `0x43EC10` | engine, unnamed | entry 1 of `CoronaEnemy_Task`: the script ticked and queued, +2 on at the done flag |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S22 and S23 call it) |

By name, already ours: `LightningBolt_PushMatrix` and `JoltBolt_End` (S22),
`BarrierRing_Grow` (S19), `MagicFx_WaitA` (S17), `MagicFx_CenterOnSide`
(L), `BattleFx_FreeTask`, and the GTE / GPU / sprite / sound library.

Shared bodies: `magic_funcs.tsv` has MAGIC131 (S30) reaching the orb, ray,
flash, shell and bolt bodies (`0x4E6BC0..`, `0x4E7650..`, `0x4E81A0..`,
`0x4E8720..`), MAGIC065..067 reaching `CoronaRay_Advance`, and MAGIC052 and
nine others reaching `MainCannonBlast_Play` / `CoronaFlash_Start`. Those
reach them through the addresses, so taking them changes nothing for the
callers.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `DoomBreathOrb_TaskTable` | `0x65BD64` | 1 |
| `DoomBreathOrb_Steps` | `0x65BD68` | 5 |
| `CoronaChild_Kinds` | `0x65BD7C` | 3 |
| `CoronaRay_Steps` | `0x65BD88` | 4 |
| `CoronaFlash_Steps` | `0x65BD98` | 4 |
| `MainCannonChild_Kinds` | `0x65BDA8` | 2 |
| `ThunderClapBolt_TaskTable` | `0x65BDB0` | 1 |
| `ThunderClapBolt_Steps` | `0x65BDB4` | 4 |

Each count is where the next table starts (the dump of `0x65BD50..0x65BDCC`
read 2026-09-26); the tool's "43 code entries" notes count every code
pointer that follows, across the next overlays' tables.

## 5. The fuzz

`BOF3X_SHADOW=magic_s31` runs `magic_harness::Run` over the 51 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s31_fuzz.cpp`:

- **Callees** (44 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own - every primitive of a draw is built at the same pointer, so without
    the log only the last would be compared;
  - the projections log their SVECTORs through `deref` (6 bytes each); the
    two matrix pushes' GTE callees log theirs and write a result where the
    real ones write (S22's effects);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - the sprite calls that act on `Sprite_Current` (`Sprite_ScriptTick`,
    `_SetAnimation`, `_QueueOverlay`, `_UpdateScreen`) log which sprite, and
    the two that draw also log `0x9039D8` (the frame-offset table the shells
    and blasts swap round their steps);
  - `BattleTask_Create` answers 0xFF (none free) a quarter of the time while
    `MainCannon_Fire` is fuzzed - the only caller that tests it;
  - `Gte_PushMatrix` keeps the facing inside 0..3 while `CoronaRay_PushMatrix`
    is fuzzed;
  - this group's own draws and pushes, and S22's `LightningBolt_PushMatrix`,
    by address (their callers call them directly).
- **Tables:** the eight `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `Field_Kind2Z` /
  `Field_Kind2X`; `0x9039D8`; `MoveScript_TintRecords`; CLUT row 26 and its
  source. 23,712 bytes of state in 17 regions.
- **Seed:** `0x904B3C` at one of the harness's sprite records; each
  dispatcher inside its table; each count-down one step before and at its
  threshold (Brighten's 0x10, the orb's 0x80 / 0x20 / 0, the ray's 0x30,
  the rise's 0x10, ...); `Frame_Counter & 7` 0 two times in three and +9
  near 6 for `MainCannon_Fire`; for `Corona_Start` the event-battle byte
  set and the acting "enemy"'s +0x100 0x29, each two times in three; for
  `MainCannonShell_Fly` the shell's y one either side of the actor's limit;
  step 4 / step 3 for the draws' shade branches; the ray's quad count mostly
  below 0x30; `ThunderClapBolt_DrawBand`'s three words its callers' rows two
  times in three (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte (the two loop bounds the draws read back kept small), `Field_Kind2*`,
  `0x9039D8`, the task's +0x18 / +0x1C / +0x20 / +0x2E / +0x30, the
  `0x904B3C` record, a tint byte.

Result in this worktree (2026-09-26):

    shadow      magic_s31 self-test: 102000 rounds over 51 functions (2000 each), 3926859 calls to the stand-ins,
                0 MISMATCHES; 23712 bytes of state (17 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (3,909,316 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

197 plants, each put in `magic_s31.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s31`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). **197 of 197 refused**, every one by exit 3 with a count only in the functions the plant touches (the D-, C-, M-, T- controls are Doom Breath, Corona, Main Cannon, Thunder Clap). No equivalent mutant was planted; none was left standing.

The thinnest (fewer than 60 rounds):

- **T2** (Start: source read after the task): 38
- **T39** (Arcs: sorted at the point / 4): 47

Re-run 2026-09-26 on the kFlag-fixed harness
([`magic_harness.md`](magic_harness.md) §8): 21 controls in the affected
functions, 21 refused. The section-8 functions are `CoronaEnemy_Start`,
`MainCannon_Start`, `MainCannon_Fire`, `MainCannon_End`,
`MainCannonShell_Fly` and `MainCannonBlast_Play`; their controls C56, C57,
M2..M12, M26..M32 and M39 were re-planted from the table (the same script
pattern) and each refused by a count in the functions it touches only, in
the same number of rounds as before. The other 176 plant in functions that
reach no `kFlag` / `kBool` recorder and stand. The thinnest: M6 (every
fourth frame) 89, M28 179, M27 271. No fuzz change. Clean self-test after
the last: 0 mismatches, exit 0.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| D1 | DoomBreath_Task: entries 2/3 swapped | DoomBreath_Task 808 |
| D2 | Start: facing from the owner's +9 | DoomBreath_Start 1936 |
| D3 | Start: orb +9 0x19 | DoomBreath_Start 1965 |
| D4 | Start: parameter 0x65 | DoomBreath_Start 2000 |
| D5 | CLUT row: bit 0x4000 | DoomBreath_Start 2000, Corona_Start 2000 |
| D6 | Start: tint blue 1 | DoomBreath_Start 2000 |
| D7 | Start: +0xA tint + 1 | DoomBreath_Start 1992 |
| D8 | Start: +9 1 | DoomBreath_Start 2000 |
| D9 | Brighten: a fourth channel | DoomBreath_Brighten 2000 |
| D10 | Brighten: at 0x11 | DoomBreath_Brighten 1007 |
| D11 | Brighten: +0x29 3 | DoomBreath_Brighten 509 |
| D12 | Fade: record stride 11 | DoomBreath_Fade 1991 |
| D13 | Fade: the target flashed | DoomBreath_Fade 440 |
| D14 | Fade: +9 0x11 | DoomBreath_Fade 495 |
| D15 | End: at 0xFE | DoomBreath_End 1042 |
| D16 | End: +0x29 5 | DoomBreath_End 546 |
| D17 | End: done bit 3 | DoomBreath_End 389 |
| D18 | Orb_Task: wrong handler | DoomBreathOrb_Task 2000 |
| D19 | Orb_Run: Grow / Shrink swapped | DoomBreathOrb_Run 780 |
| D20 | Orb_Run: +0xA past step 2 | DoomBreathOrb_Run 181 |
| D21 | Orb_Run: glow before stream | DoomBreathOrb_Run 830 |
| D22 | Launch: offset z -14 | DoomBreathOrb_Launch 500 |
| D23 | Launch: height + 0x800001 | DoomBreathOrb_Launch 494 |
| D24 | Launch: +9 0x2D | DoomBreathOrb_Launch 500 |
| D25 | Launch: +0x20 0x81 | DoomBreathOrb_Launch 500 |
| D26 | Launch: step 0x8001 | DoomBreathOrb_Launch 497 |
| D27 | Move: z by the x step | DoomBreathOrb_Move 2000 |
| D28 | Move: on at 1 | DoomBreathOrb_Move 1022 |
| D29 | Grow: angle mask 0x1F | DoomBreathOrb_Grow 1019 |
| D30 | Grow: at 0x7F | DoomBreathOrb_Grow 997 |
| D31 | Shrink: radius - 3 | DoomBreathOrb_Shrink 2000 |
| D32 | Shrink: at 0x21 | DoomBreathOrb_Shrink 494 |
| D33 | End: width - 2 | DoomBreathOrb_End 2000 |
| D34 | End: owner +0xB 0xFE | DoomBreathOrb_End 541 |
| D35 | PushMatrix: facing 1 0x900 | DoomBreathOrb_PushMatrix 252 |
| D36 | PushMatrix: other facings 1 | DoomBreathOrb_PushMatrix 1235 |
| D37 | pushes: height / 4 | DoomBreathOrb_PushMatrix 2000, CoronaRay_PushMatrix 2000 |
| D38 | pushes: x sar 8 | DoomBreathOrb_PushMatrix 2000, CoronaRay_PushMatrix 2000 |
| D39 | Stream: shade 0x81 | DoomBreathOrb_DrawStream 1086 |
| D40 | Stream: width 0xE01 | DoomBreathOrb_DrawStream 2000 |
| D41 | Stream: step i sl 5 | DoomBreathOrb_DrawStream 2000 |
| D42 | Stream: v mod 13 | DoomBreathOrb_DrawStream 2000 |
| D43 | Stream: tpage + 1 | DoomBreathOrb_DrawStream 2000 |
| D44 | Stream: +0x45 without 0x10 | DoomBreathOrb_DrawStream 2000 |
| D45 | Stream: 62 quads | DoomBreathOrb_DrawStream 2000 |
| D46 | Stream: depths 4_10B | DoomBreathOrb_DrawStream 2000 |
| D47 | Stream: closing tpage 0x96 | DoomBreathOrb_DrawStream 2000 |
| D48 | Stream: step-4 shade sl 2 | DoomBreathOrb_DrawStream 911 |
| D49 | Glow: shade 0xC1 | DoomBreathOrb_DrawGlow 1322 |
| D50 | Glow: step-4 x 8 | DoomBreathOrb_DrawGlow 827 |
| D51 | Glow: start y 0xFF81 | DoomBreathOrb_DrawGlow 1998 |
| D52 | Glow: step - 0x3F | DoomBreathOrb_DrawGlow 2000 |
| D53 | Glow: an eighth | DoomBreathOrb_DrawGlow 1999 |
| D54 | Glow: the bright edge on the second | DoomBreathOrb_DrawGlow 2000 |
| D55 | Glow: one quad | DoomBreathOrb_DrawGlow 2000 |
| D56 | Glow: width 0xC01 | DoomBreathOrb_DrawGlow 2000 |
| D57 | Glow: commit 0x48 | DoomBreathOrb_DrawGlow 2000 |
| C1 | Corona_Task: entries 0/1 swapped | Corona_Task 1334 |
| C2 | Start: +9 0x19 | Corona_Start 1938 |
| C3 | Start: ray facing from the owner's +9 | Corona_Start 1966 |
| C4 | Start: flash +1 2 | Corona_Start 1979 |
| C5 | Start: event test inverted | Corona_Start 1235 |
| C6 | Start: 0x2A | Corona_Start 1237 |
| C7 | Start: 0x7C bytes copied | Corona_Start 1232 |
| C8 | Start: copy +0x29 2 | Corona_Start 1232 |
| C9 | Start: copy +5 0x4C | Corona_Start 1232 |
| C10 | Start: copy +6 2 | Corona_Start 1232 |
| C11 | Wait: flags 0x21 | Corona_Wait 530 |
| C12 | End: +9 0x1F | Corona_End 489 |
| C13 | End: at 1 | Corona_End 994 |
| C14 | Child_Task: ray / flash swapped | CoronaChild_Task 1272 |
| C15 | Ray_Run: MAGIC067's two swapped | CoronaRay_Run 979 |
| C16 | Ray_Run: no pop | CoronaRay_Run 1018 |
| C17 | Ray_Start: offset 0xFFFD9000 | CoronaRay_Start 2000 |
| C18 | Ray_Start: ground + 0x201 | CoronaRay_Start 2000 |
| C19 | Ray_Start: elevation (z, x) | CoronaRay_Start 2000 |
| C20 | Ray_Start: +0x14 0x3000 | CoronaRay_Start 2000 |
| C21 | Ray_Start: +0xA 0x11 | CoronaRay_Start 2000 |
| C22 | Advance: +9 shr 5 | CoronaRay_Advance 1868 |
| C23 | Advance: frame bit 1 | CoronaRay_Advance 977 |
| C24 | Advance: step 0x2001 | CoronaRay_Advance 1994 |
| C25 | Advance: at 0x32 | CoronaRay_Advance 488 |
| C26 | Advance: +9 down by 1 | CoronaRay_Advance 2000 |
| C27 | Ray_PushMatrix: facing sl 9 | CoronaRay_PushMatrix 1485 |
| C28 | Ray_Draw: first radius 0x81 | CoronaRay_Draw 1999 |
| C29 | Ray_Draw: angle mask 0x1F | CoronaRay_Draw 950 |
| C30 | Ray_Draw: cos at the height's scale | CoronaRay_Draw 1998 |
| C31 | Ray_Draw: height at the radius | CoronaRay_Draw 1998 |
| C32 | Ray_Draw: radius (i + 5) sl 5 | CoronaRay_Draw 1953 |
| C33 | Ray_Draw: step-3 shade / 4 | CoronaRay_Draw 819 |
| C34 | Ray_Draw: step 2 | CoronaRay_Draw 1638 |
| C35 | Ray_Draw: ramp below 0xF | CoronaRay_Draw 1272 |
| C36 | Ray_Draw: ramp in eighths | CoronaRay_Draw 1729 |
| C37 | Ray_Draw: last 7 | CoronaRay_Draw 1365 |
| C38 | Ray_Draw: far + 2 | CoronaRay_Draw 1788 |
| C39 | Ray_Draw: near in quarters | CoronaRay_Draw 1777 |
| C40 | Ray_Draw: scroll x 4 | CoronaRay_Draw 1952 |
| C41 | Ray_Draw: 0xFC made 0xFA | CoronaRay_Draw 841 |
| C42 | Ray_Draw: +0x3D + 5 | CoronaRay_Draw 1953 |
| C43 | Ray_Draw: projected at the G4 stride | CoronaRay_Draw 1953 |
| C44 | Ray_Draw: one quad more | CoronaRay_Draw 1593 |
| C45 | Ray_Draw: closing tpage 0x35 | CoronaRay_Draw 2000 |
| C46 | Ray_Draw: vertex copy | CoronaRay_Draw 1953 |
| C47 | Flash_Run: steps 2/3 swapped | CoronaFlash_Run 973 |
| C48 | Flash_Run: drawn with +0 clear | CoronaFlash_Run 948 |
| C49 | Flash_Start: +0xA 0x89 | CoronaFlash_Start 2000 |
| C50 | Flash_Draw: facing bit 1 | CoronaFlash_Draw 981 |
| C51 | Flash_Draw: bottom 240.0 | CoronaFlash_Draw 2000 |
| C52 | Flash_Draw: x 13 | CoronaFlash_Draw 1989 |
| C53 | Flash_Draw: sl 3 | CoronaFlash_Draw 1968 |
| C54 | Flash_Draw: tpage 0x36 | CoronaFlash_Draw 2000 |
| C55 | Enemy_Task: steps 0/1 swapped | CoronaEnemy_Task 1345 |
| C56 | Enemy_Start: animation 4 | CoronaEnemy_Start 2000 |
| C57 | Enemy_Start: queued before the tick | CoronaEnemy_Start 2000 |
| M1 | MainCannon_Task: entries 0/1 swapped | MainCannon_Task 1316 |
| M2 | Start: x 0xF1 | MainCannon_Start 2000 |
| M3 | Start: 17 CLUT words | MainCannon_Start 2000 |
| M4 | TickOwner: the task's own script | MainCannon_Start 1674, MainCannon_Fire 1184, MainCannon_End 1675 |
| M5 | Start: +2 on | MainCannon_Start 2000 |
| M6 | Fire: every fourth frame | MainCannon_Fire 89 |
| M7 | Fire: +0xB counted with no slot | MainCannon_Fire 338 |
| M8 | Fire: + 0x15 | MainCannon_Fire 543 |
| M9 | Fire: bit 1 of +9 | MainCannon_Fire 812 |
| M10 | Fire: at 7 | MainCannon_Fire 612 |
| M11 | Fire: parameter 0x57 | MainCannon_Fire 1425 |
| M12 | End: at 1 | MainCannon_End 968 |
| M13 | Child_Task: shell / blast swapped | MainCannonChild_Task 2000 |
| M14 | Shell_Run: the blast's frame table | MainCannonShell_Run 536 |
| M15 | Shell_Run: bit 1 of +0 | MainCannonShell_Run 523 |
| M16 | Shell_Run: the table not put back | MainCannonShell_Run 2000 |
| M17 | Aim: sl 4 / 4 | MainCannonShell_Aim 2000 |
| M18 | Aim: party at 0..1 | MainCannonShell_Aim 435 |
| M19 | Aim: party y from +0x2E | MainCannonShell_Aim 1222 |
| M20 | Aim: enemy y from +0x2E | MainCannonShell_Aim 778 |
| M21 | Aim: parity from the owner's +8 | MainCannonShell_Aim 1007 |
| M22 | Aim: animation target & 1 | MainCannonShell_Aim 2000 |
| M23 | Aim: +0x27 0xA1 | MainCannonShell_Aim 2000 |
| M24 | Aim: +0x1C sl 3 | MainCannonShell_Aim 1999 |
| M25 | Aim: +9 4 | MainCannonShell_Aim 2000 |
| M26 | Fly: sar 3 | MainCannonShell_Fly 1998 |
| M27 | Fly: party limit 0x15 | MainCannonShell_Fly 271 |
| M28 | Fly: enemy limit 0x27 | MainCannonShell_Fly 179 |
| M29 | Fly: <= the limit | MainCannonShell_Fly 421 |
| M30 | Fly: blast +1 2 | MainCannonShell_Fly 771 |
| M31 | Fly: blast owned by the shell | MainCannonShell_Fly 669 |
| M32 | Fly: no tick | MainCannonShell_Fly 2000 |
| M33 | Blast_Run: the shell's frame table | MainCannonBlast_Run 1021 |
| M34 | Blast_Run: update with +0 clear | MainCannonBlast_Run 2000 |
| M35 | Blast_Run: steps 0/1 swapped | MainCannonBlast_Run 1303 |
| M36 | Blast_Start: +0x25 0x1E | MainCannonBlast_Start 2000 |
| M37 | Blast_Start: animation 6 | MainCannonBlast_Start 2000 |
| M38 | Blast_Start: sound & 3 | MainCannonBlast_Start 997 |
| M39 | Blast_Play: branches swapped | MainCannonBlast_Play 2000 |
| T1 | ThunderClap_Task: entries swapped | ThunderClap_Task 2000 |
| T2 | Start: source read after the task | ThunderClap_Start 38 |
| T3 | Start: +0xB 2 | ThunderClap_Start 1989 |
| T4 | Start: +0x3C from +0x38 | ThunderClap_Start 2000 |
| T5 | Start: parameter 0x36 | ThunderClap_Start 2000 |
| T6 | Bolt_Task: wrong handler | ThunderClapBolt_Task 2000 |
| T7 | Bolt_Run: steps 2/3 swapped | ThunderClapBolt_Run 999 |
| T8 | Bolt_Run: first row jitter 7 | ThunderClapBolt_Run 779 |
| T9 | Bolt_Run: angle + 5 | ThunderClapBolt_Run 751 |
| T10 | Bolt_Run: angle back 0xF5 | ThunderClapBolt_Run 767 |
| T11 | Bolt_Run: flash 0x11 right | ThunderClapBolt_Run 776 |
| T12 | Bolt_Run: flash 0x21 back | ThunderClapBolt_Run 778 |
| T13 | Bolt_Run: +2 not tested | ThunderClapBolt_Run 270 |
| T14 | Bolt_Start: Rand & 7 | ThunderClapBolt_Start 997 |
| T15 | Rise: +0xA up by 3 | ThunderClapBolt_Rise 2000 |
| T16 | Rise: tint blue -7 | ThunderClapBolt_Rise 466 |
| T17 | Rise: flags 0x11 | ThunderClapBolt_Rise 466 |
| T18 | Rise: +9 0x21 | ThunderClapBolt_Rise 466 |
| T19 | Fade: the actor flashed | ThunderClapBolt_Fade 468 |
| T20 | Fade: +0xB not up | ThunderClapBolt_Fade 1991 |
| T21 | Band: first radius a2 / 4 | ThunderClapBolt_DrawBand 1997 |
| T22 | Band: shade sl 2 | ThunderClapBolt_DrawBand 1990 |
| T23 | Band: x 12 | ThunderClapBolt_DrawBand 1984 |
| T24 | Band: steps sl 5 | ThunderClapBolt_DrawBand 2000 |
| T25 | Band: 22 steps | ThunderClapBolt_DrawBand 2000 |
| T26 | Band: radius + 1 on the up side | ThunderClapBolt_DrawBand 2000 |
| T27 | Band: first quad +0x16 the light shade | ThunderClapBolt_DrawBand 1998 |
| T28 | Band: second quad +0x26 at step 1 | ThunderClapBolt_DrawBand 1982 |
| T29 | Band: a1 x 3 | ThunderClapBolt_DrawBand 2000 |
| T30 | Band: last quad +0x36 at step 2 | ThunderClapBolt_DrawBand 1991 |
| T31 | Band: angle mask 0x1F | ThunderClapBolt_DrawBand 2000 |
| T32 | Band: first quad sorted 0x40 | ThunderClapBolt_DrawBand 2000 |
| T33 | Arcs: first radius 0x81 | ThunderClapBolt_DrawArcs 1995 |
| T34 | Arcs: swing 0xA1 + | ThunderClapBolt_DrawArcs 1852 |
| T35 | Arcs: swing - Rand & 0x3F | ThunderClapBolt_DrawArcs 1709 |
| T36 | Arcs: one line more | ThunderClapBolt_DrawArcs 1689 |
| T37 | Arcs: angle + 3 | ThunderClapBolt_DrawArcs 1903 |
| T38 | Arcs: blue 0x21 | ThunderClapBolt_DrawArcs 1903 |
| T39 | Arcs: sorted at the point / 4 | ThunderClapBolt_DrawArcs 47 |
| T40 | Flash: radius x 3 | ThunderClapBolt_DrawFlash 1983 |
| T41 | Flash: centre blue x 7 | ThunderClapBolt_DrawFlash 1998 |
| T42 | Flash: sixteen triangles | ThunderClapBolt_DrawFlash 2000 |
| T43 | Flash: first rim y from the sine | ThunderClapBolt_DrawFlash 2000 |
| T44 | Flash: sorted 0x30 | ThunderClapBolt_DrawFlash 2000 |

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. By
reading, the enemy-only skills among them (the "Breath" and "Cannon" names
are the sibling's) would be cast by an enemy. Things to look for:

- Doom Breath: the caster darkening and brightening, an orb flying out with a
  stream and a glow.
- Corona: a ray crossing the field, a screen flash from the caster's side;
  in an event battle, a copy of the caster.
- Main Cannon: six shells from the caster to the acting actor, each leaving a
  blast.
- Thunder Clap: one Lightning-like bolt at the source.

`Corona_Start`'s third child is reached only in an event battle whose acting
enemy's +0x100 is 0x29; which boss that is was not read.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96 and D106 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the ten stack tables and the
  eight `.data` tables (an index past a one-entry task table reads the next
  table). Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `DoomBreath_Start`, `Corona_Start` (three calls), `MainCannonShell_Fly`
  and `ThunderClap_Start`: slot 255 is `0x93A000 + 255 x 0x84 = 0x9423FC`,
  past the image's end (`0x93F000`) - an access violation, in ours as in the
  original (the same address is written). Only `MainCannon_Fire` tests it.
- **`CoronaRay_PushMatrix` turns by an uninitialised word** for a direction
  past 3 (section 2; ours aborts).
- **`Corona_Start` indexes the enemy records by the actor byte - 3**,
  unchecked: with a party actor it reads a "record" among the task slots
  (below `0x93B960`), and the 0x80-byte copy may overlap the new slot (ours
  copies dword by dword, forward, as `rep movsd` does). Reached only in an
  event battle.
- **`MainCannonShell_Aim` / `_Fly` index the enemy records by the actor
  byte - 3**, unchecked above 10.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 47 lines under a `group S31` comment: 44 new,
plus three host extents re-listed smaller (`004E7130 2E4`, `004E77D0 DC`,
`004E8FA0 199`, each the function's own size). Four were listed right
already (`004E6DF0`, `004E6EC0`, `004E88D0`, `004E8DA0`).
