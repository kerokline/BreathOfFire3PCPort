# Group S33: Accession and Mighty Chop (MAGIC151, 154)

**Status:** IN PROGRESS (2026-09-27). All 57 functions are ours
(`src/game/magic_s33.cpp`, shadow name `magic_s33`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 114,000 rounds. 223 of 224 negative controls refused (one equivalent mutant, its near variant refused). Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S33
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 86 | 0x29E | MAGIC151 | 0x97 | Accession | `0x4EAE70..0x4ED0B1` | 44 |
| 35 | 0x29F | MAGIC154 | 0x9A | Mighty Chop | `0x4ED0C0..0x4ED66E` | 13 (+ `BattleFx_SetSize`, ours before) |

The extents are `tools/magic_rows.py --unit MAGIC151 / MAGIC154 --clones`
(capstone recursive descent; no jump table, nothing `REFUSED`). All 57
functions lie in the units' extents; none was found inside or missing from
them. MAGIC154 also holds `BattleFx_SetSize` (`0x4ED5C0`, round eight's CK),
which its copy's step table calls. 9,790 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. As in S31, the queue
lists the rows (35, 86) against the overlays in file order, but `Magic_Rows`
pairs them the other way round: MAGIC151 is row 86, MAGIC154 row 35
(`analysis/magic_rows.tsv`). What each spell looks like in play has not been
measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Accession (MAGIC151): the task.**
  - `Accession_Task`, the kind-2 task: `_Start`, `_Run`, `_End` by +1.
  - `Accession_Start`: the engine's `0x4514A0` (unnamed: resets the acting
    member's battle state; not read to its end), a banner (`BattleBanner_Add`
    kind 1, 0x3C frames) of the first eight characters of the system message
    the byte table `0x64ECB0` gives for the battle byte `0x904B89` (magic_lib's
    `kFormation`); +2 up when that byte is 3, 0xB or 0xD (+2 picks the step
    table below); +9 3 or 0x1E by the target's party record +0x89.
  - `Accession_Run` picks `Accession_StepsA` or `_StepsB` by +2; each is a
    five-step table by +3 sharing `_ActorPose` (animation +8 + 0x3C ensured on
    the acting member's record, Sprite_Current swapped to it), `_ActorScript`
    (the member's script ticked once a frame; at its end the controller child
    is made and +3 moves on) and `_Finish` (the member's +0 bit 0x40 cleared,
    +1 on).
  - Path A (`_LoadFormA`, `_ApplyA`): once the controller signals (+0xB 1), the
    DAT file `u16 [ptr][party set]` is loaded, `ptr` from `0x64E9BC + 8 x
    0x904B89` (the second pointer when the owner faces neither 0 nor 1); once
    loaded, the acting member's tint, palette, status tint, CLUT STP bits and
    animation are rebuilt.
  - Path B (`_LoadFormB`, `_ApplyB`): every party record +0 bit 0x40 and the
    table's first file; once loaded, each present member's tint, queued item
    and turn are undone, the **acting member's record is copied over party
    record 0** and placed at the fight's centre, members 1 and 2 are cleared
    (+0 0), the actor byte becomes 0, the battle windows 0xD.. are allocated
    again, and the new member 0 is rebuilt through `Field_MemberSprite` for
    the party set's first member. What this is in play is not measured.
  - `Accession_End`: once the controller has ended (+0xB 2), the done flag,
    the acting member's +0x134 bit 2 cleared and bit 0x20 set, freed.
- **Accession: the children** (kind 1, parameter 0x44, `AccessionChild_Task`
  by +1 through `AccessionChild_Kinds`):
  - the controller (`AccessionCtl_*`, +1 4): a bolt at once (sound 0x102), a
    ring four frames later, an orb four after that (sound 0x103); 0x14 frames
    on it signals the task (+0xB 1), waits for the task's +1 to reach 2, and
    when its three children have ended signals +0xB 2 and ends;
  - the orb (`AccessionOrb_*`, +1 0): grows its radius +9 to 0x20, emits a
    spark every fourth frame until the owner reaches step 5, waits for the
    sparks, dims and fades; under the actor matrix it draws a **shell** (a dome
    of 8 x 16 flat semi-transparent quads, tpage 0x55) and a **glow** (a band
    of 16 gouraud quads);
  - the sparks (`AccessionSpark_*`, +1 1): `_Draw(a1, a2)` draws +0xA lines
    wandering over a sphere of radius 0x200 from two angle bytes of
    `AccessionSpark_Angles` (`0x65BEA0`, 16 pairs by +4);
  - the bolt (`AccessionBolt_*`, +1 2): S22's shape - `_Start`, `_Rise`, then
    S22's `MyollnirBolt_Hold` and `JoltBolt_End`; under S25's
    `SpellSleep_PushTurnMatrix`, three rows of `_DrawBand` (17 steps of four
    gouraud quads);
  - the rings (`AccessionRing_*`, +1 3): a radius +0x14 from 0x100 widening to
    0x400, then S29's `DivineBeam_Widen`; two flat rings of 64 gouraud quads.
- **Mighty Chop (MAGIC154).**
  - `MightyChop_Task`: `_Start`, `_Throw`, `_End`, `BattleFx_Finish` by +1.
  - `MightyChop_Start`: the actor's animation 0xC; a **copy** of the acting
    actor's record (its first 0x80 bytes: a party record below 3, else the
    enemy's) as a child (kind 1, 0x52, +1 1); CLUT row 26's first 16 words
    back from their source; the owner's +0 bit 0x40.
  - The copy (`MightyChopCopy_*`, through `MightyChopChild_Kinds`):
    `BattleFx_SetSize`, `_Play` (its script until its end, the actor's sound
    (2, 4)), `_Wait` (its script to the end again, then the parent's +9 down),
    `BattleFx_FreeTask`.
  - `MightyChop_Throw`: once the copy has played (+0xB 0), six **blades**
    (the same kind, +1 0) at the source sprite plus the offset (0, -0x18000)
    turned by its direction, each started 3 i + 1 frames later.
  - A blade (`MightyChopBlade_*`): a draw-mode packet on layer 3, the
    frame-offset table `0x9039D8` swapped to the effects' `0x8E3580` round its
    step and screen update; `_Start` places it +0xB x 0x8000 along the owner's
    direction with sprite fields and animation 0; `_Grow` / `_Shrink` scale
    +0x40 / +0x44 up and down over four frames each, then free it. Entry 3,
    `_Sink`, is reached by no step.
  - `MightyChop_End`: once the copy's second script has ended (+9 0), the
    actor's animation 4, the owner's bit cleared, target flags 0x10.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one exception
that follows the project's precedent: a phase past any of the fifteen
dispatch tables (eight stack tables, seven `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3).

`AccessionSpark_Draw` keeps the original's use of its first argument's stack
slot as the running longitude (ours keeps it in a local byte: the same value
at every use).

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x446770` | engine, unnamed | the dx / dz turn by direction (`MightyChop_Throw`, `MightyChopBlade_Start`; as S22, S23, S31 call it) |
| `0x4514A0` | engine, unnamed | `Accession_Start`'s first call: resets the acting member's battle state (no arguments) |

By name, already ours: `MyollnirBolt_Hold` and `JoltBolt_End` (S22),
`SpellSleep_PushTurnMatrix` (S25), `DivineBeam_Widen` (S29),
`BattleFx_SetSize`, `BattleFx_Finish`, `BattleFx_FreeTask` (CK),
`MagicFx_PushActorMatrix`, and the GTE / GPU / sprite / sound / battle
library (`Field_MemberSprite`, `Window_Alloc`, `Battle_ReturnQueuedItem`,
`LoadDatFile`, `BattleBanner_Add`, ...). No other wave-five group's address is
called.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `AccessionChild_Kinds` | `0x65BE74` | 5 |
| `AccessionOrb_Steps` | `0x65BE88` | 6 |
| `AccessionSpark_Angles` | `0x65BEA0` | 32 bytes |
| `AccessionSpark_Steps` | `0x65BEC0` | 3 |
| `AccessionBolt_Steps` | `0x65BECC` | 4 |
| `AccessionRing_Steps` | `0x65BEDC` | 3 |
| `MightyChopChild_Kinds` | `0x65BEE8` | 2 |
| `MightyChopBlade_Steps` | `0x65BEF0` | 4 |

Each count is where the next table starts (the dump of `0x65BE60..0x65BF0C`
read 2026-09-27); the next overlay's data starts at `0x65BF00`. The tables
MAGIC151 reads by `0x904B89` and the party set (`0x64ECB0`, `0x64E9BC`,
`0x669750`) and the window offsets (`0x64DF70`, `0x64E2BC`) are the engine's
and are read in place, not named here.

## 5. The fuzz

`BOF3X_SHADOW=magic_s33` runs `magic_harness::Run` over the 57 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s33_fuzz.cpp`:

- **Callees** (42 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes and moves `Gfx_PacketNext` on through a
    0x2000-byte buffer of the fuzz's own (S20, S24);
  - the projections log their SVECTORs through `deref` (6 bytes each);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - every call that acts on `Sprite_Current` (`Sprite_ScriptTickOnce`,
    `_ScriptTick`, `_SetAnimation`, `_EnsureAnimation`, `_LoadPalette`,
    `_SetClutStp`, `Battle_StatusTint`, `Field_MemberSprite`,
    `Sprite_UpdateScreen`) logs which sprite - MAGIC151 swaps it to a party
    record round them - and the screen update also logs `0x9039D8`;
  - `File_LoadDone` answers `kFlag` (its callers test all 32 bits);
  - `Window_Alloc` moves, half the time, the member's +8 / +0x89 / +0x2E /
    +0x30 and `0x904AB0`, and `AreaMap_Elevation` party record 0's +0x34 /
    +0x38: the cells `Accession_ApplyB` reads back after them, which the
    harness's disturbance reaches too rarely (controls A45, A52, A63);
  - this group's own draws, called directly, by address.
- **Tables:** the seven `.data` handler tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `0x9039D8`; the first 16
  words of CLUT row 26 and their source; the fight's centre `0x903780`;
  `0x904B50..0x904B9F` (`0x904B79`, `0x904B89`, `0x904B8A`, `0x904B8F`); the
  party set `0x90412C`; `0x939B00..0x939EFF` (the per-member cells
  `0x939B07` / `0x939C05` / `0x939C10`). 20,788 bytes of state in 19 regions.
- **Seed:** `0x904B89` always one of the 23 kinds whose `0x64E9BC` pointers
  are not null (entries 10, 19 and 20 are: a null there is a fault on both
  sides); each dispatcher inside its table; each count-down one step before
  and at its threshold; `Accession_Start` 3 / 0xB / 0xD and a party target
  with +0x89 0; the load gates +0xB 1 and the owner's facing 0..3;
  `Accession_ApplyA`'s bit 14 and `0x904B8A` equal to the actor; the orb's
  frame counter and the owner at step 5; the spark's +4 inside its 16 pairs
  and +0xA small; `AccessionBolt_DrawBand`'s three words its caller's rows two
  times in three (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex or scratch word,
  the fight's centre, `0x9039D8`, a task word (+0xC, +0x10, +0x14, +0x3E,
  +0x40, +0x44, +0x5D), a byte of party records 0..2 the rebuild reads (+0,
  +5, +8, +0x27, +0x2E, +0x30, +0x89, +0x90), and `0x904B79`, `0x904B89`,
  `0x90412C`, `0x904AB0`, `0x904AAC`, `0x904B8A` (`0x904B89` is read through a
  pointer only before any call).
- **Settle:** while `MightyChopBlade_Run` is fuzzed, +2 is kept inside its
  four-entry table after every disturbance: the original reads it after two
  calls, and past the table it calls through whatever follows (ours aborts).
  Found by the first run (`FATAL: MightyChopBlade_Run: phase 252`).

Result in this worktree (2026-09-27):

    shadow      magic_s33 self-test: 114000 rounds over 57 functions (2000 each), 7001837 calls to the stand-ins,
                0 MISMATCHES; 20788 bytes of state (19 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (7,001,064 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

224 plants, each put in `magic_s33.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s33`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). **223 of 224 refused**, all by exit 3 with a count only in the functions the plant touches but A24, refused by a fault after its first mismatch; one equivalent mutant not refused (A54, below). The A-, C-, O-, S-, B-, R- controls are Accession's task, controller, orb, sparks, bolt and rings; M- Mighty Chop.

The first run left four standing in `Accession_ApplyB`: A45, A52, A63 (a read moved across a call that nothing in the fuzz moved) and A54. `Window_Alloc` and `AreaMap_Elevation` then got an `effect` that moves the cells `Accession_ApplyB` reads back after them (section 5), and the controls in the functions that call them (A37..A64, A54b, C3..C8) were re-planted: all refused but A54, and the numbers below are the re-run's.

The thinnest (fewer than 60 rounds):

- **A6** (Start: kind read before the calls): 6
- **A46** (ApplyB: slot 0 z from the old read): 3
- **B15** (Band: V[8] before the call): 5
- **M12** (Throw: source re-read after the turn): 22

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| A1 | Accession_Task: entries 1/2 swapped | Accession_Task 1380 |
| A2 | Start: 0x8031F3 2 | Accession_Start 2000 |
| A3 | Start: banner id by the kind + 1 | Accession_Start 1710 |
| A4 | Start: 7 characters | Accession_Start 2000 |
| A5 | Start: banner timer 0x3D | Accession_Start 2000 |
| A6 | Start: kind read before the calls | Accession_Start 6 |
| A7 | Start: 0xC for 0xB | Accession_Start 498 |
| A8 | Start: +9 4 | Accession_Start 1161 |
| A9 | Start: party +0x88 | Accession_Start 715 |
| A10 | Run: entries swapped | Accession_Run 2000 |
| A11 | StepsA: LoadFormA / ApplyA swapped | Accession_StepsA 829 |
| A12 | StepsA: by +2 | Accession_StepsA 1605 |
| A13 | ActorPose: animation + 0x3D | Accession_ActorPose 2000 |
| A14 | ActorPose: Sprite_Current not swapped | Accession_ActorPose 1916 |
| A15 | ActorPose: +3 on the current slot | Accession_ActorPose 2000 |
| A16 | ActorScript: sounds swapped | Accession_ActorScript 476 |
| A17 | ActorScript: +9 at 1 | Accession_ActorScript 1024 |
| A18 | ActorScript: +0 bit 0x20 | Accession_ActorScript 1032 |
| A19 | ActorScript: controller +1 3 | Accession_ActorScript 1376 |
| A20 | ActorScript: +0xB 1 | Accession_ActorScript 1367 |
| A21 | LoadFormA: at +0xB 2 | Accession_LoadFormA 1337 |
| A22 | LoadFormA: set & 0x7F | Accession_LoadFormA 656 |
| A23 | FormTable: facing 2 as 0 | Accession_LoadFormA 124, Accession_LoadFormB 131 |
| A24 | FormTable: stride 4 | Accession_LoadFormA: a mismatch at round 576, then an access violation (exit 0xC0000005): the mutant reads a pointer pair that is not one |
| A25 | ApplyA: bit 13 | Accession_ApplyA 302 |
| A26 | ApplyA: 0x904B8B | Accession_ApplyA 596 |
| A27 | ApplyA: mask 0xBFFE | Accession_ApplyA 234 |
| A28 | ApplyA: +0x28 kept | Accession_ApplyA 1662 |
| A29 | ApplyA: palette by +6 | Accession_ApplyA 1659 |
| A30 | ApplyA: status +0x92 | Accession_ApplyA 1663 |
| A31 | ApplyA: animation from the saved slot | Accession_ApplyA 1618 |
| A32 | Finish: 0x7F | Accession_Finish 1491 |
| A33 | Finish: +3 kept | Accession_Finish 1997 |
| A34 | StepsB: ApplyB / LoadFormB swapped | Accession_StepsB 825 |
| A35 | LoadFormB: two members | Accession_LoadFormB 632 |
| A36 | LoadFormB: the second u16 | Accession_LoadFormB 171 |
| A37 | ApplyB: present by bit 1 | Accession_ApplyB 1491 |
| A38 | ApplyB: 0x939C05 1 | Accession_ApplyB 1497 |
| A39 | ApplyB: 0x939C10 at +0x14C less | Accession_ApplyB 1288 |
| A40 | ApplyB: queue and turn order swapped | Accession_ApplyB 1497 |
| A41 | ApplyB: 0x14C - 4 bytes copied | Accession_ApplyB 1314 |
| A42 | ApplyB: centre read after the copy | Accession_ApplyB 1688 |
| A43 | ApplyB: +5 1 | Accession_ApplyB 1687 |
| A44 | ApplyB: elevation (z, x) | Accession_ApplyB 1688 |
| A45 | ApplyB: z0 read before the call | Accession_ApplyB 501 |
| A46 | ApplyB: slot 0 z from the old read | Accession_ApplyB 3 |
| A47 | ApplyB: height sl 15 | Accession_ApplyB 1688 |
| A48 | ApplyB: 0x904AA9 0x40 | Accession_ApplyB 870 |
| A49 | ApplyB: member 2 kept | Accession_ApplyB 1677 |
| A50 | ApplyB: 0x904B8F after the store | Accession_ApplyB 1683 |
| A51 | ApplyB: window 0xC + i | Accession_ApplyB 845 |
| A52 | ApplyB: window row read before the call | Accession_ApplyB 184 |
| A53 | ApplyB: row x 3 | Accession_ApplyB 804 |
| A54 | ApplyB: side ^ 3 | not refused: equivalent - (b ^ 3) >> 1 equals (b ^ 2) >> 1 for every byte; the near variant A54b (^ 6) refused |
| A54b | ApplyB: side ^ 6 | Accession_ApplyB 781 |
| A55 | ApplyB: x from +0x30 | Accession_ApplyB 845 |
| A56 | ApplyB: y + 0xB | Accession_ApplyB 845 |
| A57 | ApplyB: w[-1] 4 | Accession_ApplyB 845 |
| A58 | ApplyB: w[7] i + 1 | Accession_ApplyB 845 |
| A59 | ApplyB: 0x803188 0x71 | Accession_ApplyB 1688 |
| A60 | ApplyB: actor bit by +4 | Accession_ApplyB 1685 |
| A61 | ApplyB: member sprite 1 | Accession_ApplyB 1033 |
| A62 | ApplyB: +0 bit on the saved slot | Accession_ApplyB 1044 |
| A63 | ApplyB: 0x80318E before the loop | Accession_ApplyB 441 |
| A64 | LoadPaletteOfCurrent: index 0x904B78 | Accession_ApplyB 1674 |
| A65 | End: at +0xB 1 | Accession_End 1321 |
| A66 | End: bit 8 | Accession_End 494 |
| A67 | End: done bit 2 | Accession_End 473 |
| C1 | Child_Task: bolt / ring swapped | AccessionChild_Task 811 |
| C2 | Ctl_Run: SpawnRing / SpawnOrb swapped | AccessionCtl_Run 656 |
| C3 | Ctl_Start: direction from +9 | AccessionCtl_Start 1957 |
| C4 | Ctl_Start: x and z swapped | AccessionCtl_Start 2000 |
| C5 | Ctl_Start: +0x3C | AccessionCtl_Start 2000 |
| C6 | Ctl_Start: +9 5 | AccessionCtl_Start 1943 |
| C7 | Ctl_Start: bolt +9 2 | AccessionCtl_Start 2000 |
| C8 | Ctl_Start: sound 0x103 | AccessionCtl_Start 2000 |
| C9 | SpawnRing: +1 2 | AccessionCtl_SpawnRing 532 |
| C10 | SpawnRing: +9 3 | AccessionCtl_SpawnRing 532 |
| C11 | SpawnOrb: +9 0x15 | AccessionCtl_SpawnOrb 468 |
| C12 | SpawnOrb: +0xB not counted | AccessionCtl_SpawnOrb 465 |
| C13 | Signal: owner +0xB 2 | AccessionCtl_Signal 512 |
| C14 | WaitOwner: at 3 | AccessionCtl_WaitOwner 650 |
| C15 | Ctl_End: owner +0xB 3 | AccessionCtl_End 1021 |
| C16 | Child44: +0x80 the owner | Accession_ActorScript 1205, AccessionCtl_Start 1762, AccessionCtl_SpawnRing 481, AccessionCtl_SpawnOrb 422, AccessionOrb_Emit 1320 |
| C17 | Child44: parameter 0x45 | Accession_ActorScript 1376, AccessionCtl_Start 2000, AccessionCtl_SpawnRing 532, AccessionCtl_SpawnOrb 468, AccessionOrb_Emit 1496 |
| O1 | Orb_Run: Grow / Emit swapped | AccessionOrb_Run 672 |
| O2 | Orb_Run: glow before shell | AccessionOrb_Run 852 |
| O3 | Orb_Start: +0x5F 0x25 | AccessionOrb_Start 2000 |
| O4 | Orb_Start: +0xA 0x1F | AccessionOrb_Start 2000 |
| O5 | AtOwner: height from +0x38 | AccessionOrb_Start 2000, AccessionSpark_Start 2000, AccessionBolt_Start 536, AccessionRing_Start 2000 |
| O6 | Grow: at 0x22 | AccessionOrb_Grow 502 |
| O7 | Emit: every other frame | AccessionOrb_Emit 176 |
| O8 | Emit: +4 & 7 | AccessionOrb_Emit 734 |
| O9 | Emit: owner step 4 | AccessionOrb_Emit 1297 |
| O10 | Emit: +4 not up | AccessionOrb_Emit 1496 |
| O11 | WaitChildren: +0xA | AccessionOrb_WaitChildren 1027 |
| O12 | Dim: at 9 | AccessionOrb_Dim 527 |
| O13 | Fade: +0x5D down past 0 | AccessionOrb_Fade 1024 |
| O14 | Shell: shade x 14 | AccessionOrb_DrawShell 1954 |
| O15 | Shell: radius sl 3 | AccessionOrb_DrawShell 1988 |
| O16 | Shell: start 0x500 | AccessionOrb_DrawShell 2000 |
| O17 | Shell: seven bands | AccessionOrb_DrawShell 2000 |
| O18 | Shell: step 0x40 | AccessionOrb_DrawShell 2000 |
| O19 | Shell: previous ring not kept | AccessionOrb_DrawShell 1999 |
| O20 | Shell: tpage 0x56 | AccessionOrb_DrawShell 2000 |
| O21 | Shell: x sl 8 | AccessionOrb_DrawShell 2000 |
| O22 | Shell: link 0x34 | AccessionOrb_DrawShell 2000 |
| O23 | Shell: blue from 0x90385B | AccessionOrb_DrawShell 1996 |
| O24 | Shell: fifteen longitudes | AccessionOrb_DrawShell 2000 |
| O25 | Glow: shade x 13 | AccessionOrb_DrawGlow 1961 |
| O26 | Glow: green sl 2 | AccessionOrb_DrawGlow 1959 |
| O27 | Glow: outer radius x 17 | AccessionOrb_DrawGlow 1990 |
| O28 | Glow: Rand & 7 | AccessionOrb_DrawGlow 1022 |
| O29 | Glow: latitude sl 4 | AccessionOrb_DrawGlow 1989 |
| O30 | Glow: cos by the register | AccessionOrb_DrawGlow 2000 |
| O31 | Glow: tpage 0x36 | AccessionOrb_DrawGlow 2000 |
| O32 | Glow: green at +0x35 the red | AccessionOrb_DrawGlow 1972 |
| O33 | Glow: inner shade 2 | AccessionOrb_DrawGlow 2000 |
| O34 | Glow: depths 4_0C | AccessionOrb_DrawGlow 2000 |
| S1 | Spark_Run: Grow / Fade swapped | AccessionSpark_Run 1325 |
| S2 | Spark_Run: +0xC up by 2 | AccessionSpark_Run 645 |
| S3 | Spark_Run: +9 up by 3 | AccessionSpark_Run 654 |
| S4 | Spark_Run: pair stride 1 | AccessionSpark_Run 623 |
| S5 | Spark_Run: +9 on the second | AccessionSpark_Run 650 |
| S6 | Spark_Start: Rand & 0x3F | AccessionSpark_Start 1036 |
| S7 | Spark_Start: +0xA 5 | AccessionSpark_Start 2000 |
| S8 | Spark_Grow: from 0x21 | AccessionSpark_Grow 474 |
| S9 | Spark_Grow: +0xB 2 | AccessionSpark_Grow 1368 |
| S10 | Spark_Fade: down by 1 | AccessionSpark_Fade 2000 |
| S11 | Spark_Draw: shade x 11 | AccessionSpark_Draw 1971 |
| S12 | Spark_Draw: radius 0x201 | AccessionSpark_Draw 1998 |
| S13 | Spark_Draw: Rand & 3 + 5 | AccessionSpark_Draw 1770 |
| S14 | Spark_Draw: turn & 0x3F | AccessionSpark_Draw 1524 |
| S15 | Spark_Draw: wobble sl 3 | AccessionSpark_Draw 1770 |
| S16 | Spark_Draw: longitude not kept | AccessionSpark_Draw 1599 |
| S17 | Spark_Draw: tpage / 0x16 | AccessionSpark_Draw 1770 |
| S18 | Spark_Draw: mode 1 | AccessionSpark_Draw 1769 |
| S19 | Spark_Draw: second depth at +0x18 | AccessionSpark_Draw 1770 |
| S20 | Spark_Draw: count read once | AccessionSpark_Draw 1770 |
| S21 | Spark_Draw: link 2 at 0x24 | AccessionSpark_Draw 1770 |
| S22 | Spark_Draw: first point cos of the lon | AccessionSpark_Draw 2000 |
| B1 | Bolt_Run: Hold / End swapped | AccessionBolt_Run 984 |
| B2 | Bolt_Run: screen point skipped | AccessionBolt_Run 2000 |
| B3 | Bolt_Run: row 2 (0x10, 0x41, 0xF) | AccessionBolt_Run 769 |
| B4 | Bolt_Run: back by 7 | AccessionBolt_Run 765 |
| B5 | Bolt_Start: Rand & 7 | AccessionBolt_Start 277 |
| B6 | Bolt_Rise: +9 9 | AccessionBolt_Rise 507 |
| B7 | Bolt_Rise: at 0x14 | AccessionBolt_Rise 510 |
| B8 | Band: radius / 4 | AccessionBolt_DrawBand 1996 |
| B9 | Band: shade x 14 | AccessionBolt_DrawBand 1981 |
| B10 | Band: swap x 12 | AccessionBolt_DrawBand 1983 |
| B11 | Band: 16 steps | AccessionBolt_DrawBand 2000 |
| B12 | Band: angle & 7 | AccessionBolt_DrawBand 1999 |
| B13 | Band: Rand bit 1 | AccessionBolt_DrawBand 1999 |
| B14 | Band: minus as plus | AccessionBolt_DrawBand 2000 |
| B15 | Band: V[8] before the call | AccessionBolt_DrawBand 5 |
| B16 | Band: up sl 5 | AccessionBolt_DrawBand 2000 |
| B17 | Band: first quad +0x15 the swap | AccessionBolt_DrawBand 1991 |
| B18 | Band: first quad dark at step 2 | AccessionBolt_DrawBand 1977 |
| B19 | Band: second quad out by a1 + 1 | AccessionBolt_DrawBand 2000 |
| B20 | Band: second quad +0x16 the swap | AccessionBolt_DrawBand 1991 |
| B21 | Band: third quad by 2 a1 - 1 | AccessionBolt_DrawBand 2000 |
| B22 | Band: third quad +0x34 the swap | AccessionBolt_DrawBand 1992 |
| B23 | Band: fourth quad +0x26 the swap | AccessionBolt_DrawBand 1991 |
| B24 | Band: back by in, not 2 in | AccessionBolt_DrawBand 2000 |
| B25 | Band: top bottom not offset | AccessionBolt_DrawBand 2000 |
| B26 | Band: first draw mode 0x34 | AccessionBolt_DrawBand 2000 |
| R1 | Ring_Run: Widen / DivineBeam swapped | AccessionRing_Run 1350 |
| R2 | Ring_Run: outer before inner | AccessionRing_Run 690 |
| R3 | Ring_Start: +0x14 0x180 | AccessionRing_Start 2000 |
| R4 | Ring_Start: +9 0x11 | AccessionRing_Start 2000 |
| R5 | Widen: at 0x380 | AccessionRing_Widen 479 |
| R6 | Ring: layer 3 | AccessionRing_DrawInner 2000, AccessionRing_DrawOuter 2000 |
| R7 | Ring: shade x 14 | AccessionRing_DrawInner 1976, AccessionRing_DrawOuter 1982 |
| R8 | Ring: 63 quads | AccessionRing_DrawInner 2000, AccessionRing_DrawOuter 2000 |
| R9 | Ring: first inner point by the outer radius | AccessionRing_DrawInner 1997, AccessionRing_DrawOuter 1998 |
| R10 | Ring: V[0x14] not cleared | AccessionRing_DrawInner 1996, AccessionRing_DrawOuter 1997 |
| R11 | Inner: outer + 0x41 | AccessionRing_DrawInner 1999 |
| R12 | Outer: inner at 0x3F | AccessionRing_DrawOuter 1999 |
| R13 | Outer: shades as the inner | AccessionRing_DrawOuter 1997 |
| R14 | Ring: commit 0x40 | AccessionRing_DrawInner 2000, AccessionRing_DrawOuter 2000 |
| M1 | MightyChop_Task: entries 1/2 swapped | MightyChop_Task 994 |
| M2 | Start: animation argument 3 | MightyChop_Start 2000 |
| M3 | Start: party below 4 | MightyChop_Start 387 |
| M4 | Start: 0x7C bytes copied | MightyChop_Start 2000 |
| M5 | Start: +6 2 | MightyChop_Start 2000 |
| M6 | Start: 15 CLUT words | MightyChop_Start 2000 |
| M7 | Start: owner bit 0x20 | MightyChop_Start 1503 |
| M8 | Start: +9 2 | MightyChop_Start 2000 |
| M9 | Throw: at +0xB 1 | MightyChop_Throw 982 |
| M10 | Throw: dz 0xFFFF8000 | MightyChop_Throw 978 |
| M11 | Throw: x from the source's +0x38 | MightyChop_Throw 978 |
| M12 | Throw: source re-read after the turn | MightyChop_Throw 22 |
| M13 | Throw: five blades | MightyChop_Throw 978 |
| M14 | Throw: +9 3 i + 2 | MightyChop_Throw 978 |
| M15 | Throw: +0xB i + 1 | MightyChop_Throw 978 |
| M16 | Throw: blade +1 1 | MightyChop_Throw 978 |
| M17 | End: animation 5 | MightyChop_End 1016 |
| M18 | End: flags 0x20 | MightyChop_End 1016 |
| M19 | End: owner bit kept | MightyChop_End 485 |
| M20 | Child_Task: kinds swapped | MightyChopChild_Task 2000 |
| M21 | Blade_Run: layer 2 | MightyChopBlade_Run 2000 |
| M22 | Blade_Run: the battle's table during the step | MightyChopBlade_Run 792 |
| M23 | Blade_Run: update without +2 | MightyChopBlade_Run 279 |
| M24 | Blade_Run: Grow / Shrink swapped | MightyChopBlade_Run 1023 |
| M25 | Blade_Start: offset sl 14 | MightyChopBlade_Start 488 |
| M26 | Blade_Start: +0x44 0x8001 | MightyChopBlade_Start 488 |
| M27 | Blade_Start: +0x27 0xA1 | MightyChopBlade_Start 489 |
| M28 | Blade_Start: +0x2A & 3 | MightyChopBlade_Start 249 |
| M29 | Blade_Start: animation 1 | MightyChopBlade_Start 489 |
| M30 | Blade_Start: z from the owner's +0x34 | MightyChopBlade_Start 489 |
| M31 | Blade_Start: +0x25 0x1E | MightyChopBlade_Start 489 |
| M32 | Grow: +0x40 by 0x4001 | MightyChopBlade_Grow 2000 |
| M33 | Grow: at 5 | MightyChopBlade_Grow 1015 |
| M34 | Shrink: +0x44 by 0x7000 | MightyChopBlade_Shrink 2000 |
| M35 | Shrink: owner +9 | MightyChopBlade_Shrink 485 |
| M36 | Sink: at 0x7F | MightyChopBlade_Sink 980 |
| M37 | Sink: by 0x800 | MightyChopBlade_Sink 2000 |
| M38 | Copy_Run: Play / Wait swapped | MightyChopCopy_Run 969 |
| M39 | Copy_Run: update with +0 clear | MightyChopCopy_Run 898 |
| M40 | Copy_Play: sound (2, 5) | MightyChopCopy_Play 509 |
| M41 | Copy_Play: +9 1 at the end | MightyChopCopy_Play 999 |
| M42 | Copy_Play: no tick at 0 | MightyChopCopy_Play 509 |
| M43 | Copy_Wait: owner +0xB | MightyChopCopy_Wait 1340 |

## 7. What nothing reached

No recorded route casts either spell (queue §5); the live check is the owner
casting them, with a save that has them or DIV-0045's cheat. Things to look
for, by reading:

- Accession: a banner with a name; the caster's script; a bolt, rings, an orb
  with sparks; a file load and the caster rebuilt - by path B, the caster
  becoming party member 0 with the other two cleared and the windows rebuilt.
  Which kinds of `0x904B89` take path B (3, 0xB, 0xD) and what the loaded
  files hold was not read.
- Mighty Chop: the caster's animation 0xC, a copy of it playing its script,
  six blades that grow and shrink, then animation 4 and target flags 0x10.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96, D97, D100, D107 and D108 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the eight stack tables and the
  seven `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** everywhere this
  group creates a child (`Accession_ActorScript`, the controller's three,
  `AccessionOrb_Emit`, `MightyChop_Start`, `MightyChop_Throw`): slot 255 is
  past the image's end, an access violation in ours as in the original.
- **`0x904B89` indexes three tables unchecked**: `0x64ECB0` (bytes),
  `0x64E9BC` (26 pointer pairs, three of them null, dereferenced at once) -
  a kind of 10, 19 or 20, or past 25, reaching `Accession_LoadFormA` / `_B`
  faults. Whether a caster can have such a kind is not measured.
- **The acting member is indexed as a party record unchecked** (0..2 by
  design): an enemy actor would make `Accession_*` read and write past party
  record 4, and `Accession_ApplyB` would copy that over party record 0.
- **`AccessionSpark_Run` indexes `AccessionSpark_Angles` by +4 unchecked**
  (16 pairs; `AccessionOrb_Emit` gives +4 & 0xF).
- **`MightyChop_Start` indexes the enemy records by the actor byte - 3**,
  unchecked, and its 0x80-byte copy may overlap the new slot (ours copies
  dword by dword, forward, as `rep movsd` does).
- **`MightyChopBlade_Run` reads its phase after two calls** (a draw-mode
  commit): harmless in the game, where nothing those calls do moves +2.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 55 lines under a `group S33` comment: 51 new,
plus four host extents re-listed smaller (`004EBDB0 337`, `004EC240 2B6`,
`004EC640 531`, `004ECE80 232`, each the function's own size). Two were listed
right already (`004EBAA0`, `004ECC50`).
