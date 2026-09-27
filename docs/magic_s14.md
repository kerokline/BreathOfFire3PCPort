# Group S14: Weretiger, Pilfer's last function and Tsunami (MAGIC064, 065, 066)

**Status:** IN PROGRESS (2026-09-26). All 57 functions are ours
(`src/game/magic_s14.cpp`, shadow name `magic_s14`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 114,000 rounds. 300 of 300 negative controls refused, every one by a count (exit 3). Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fourth spell wave, group S14
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 77 | 0x255 | MAGIC064 | 0x40 | Weretiger | `0x4B3F00..0x4B54A8` | 38 |
| 70 | 0x256 | MAGIC065 | 0x41 | Pilfer | `0x4B54B0..0x4B5998` | 1 (of 9) |
| 17 | 0x257 | MAGIC066 | 0x42 | Tsunami | `0x4B59A0..0x4B66C0` | 18 |

The extents are `tools/magic_rows.py --unit MAGIC06N --clones` (capstone
recursive descent; no jump table, nothing `REFUSED`). All 57 functions lie in
the units' extents; none was found inside or missing from them, and none was
ours before. MAGIC065's other eight functions are round eight's
(`magic_fx_reached.cpp`: `Steal_Task` .. `StealClone_*`, DIV-0046's
`Steal_Start`); this group takes its last, `0x4B58F0`. 8,654 bytes, as the
queue counted. As in group S31, the queue lists the rows (17, 70, 77) against
the overlays in file order but `Magic_Rows` pairs them the other way round:
MAGIC064 is row 77 and MAGIC066 row 17 (`analysis/magic_rows.tsv`).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What each spell looks
like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Weretiger (MAGIC064).** The kind-2 task `Weretiger_Task` steps through
  the engine's `0x492750` (+1 on), `Weretiger_Run` and `Weretiger_End`;
  `Weretiger_Run` is a twelve-entry stack table by +2:
  - `_Pose` / `_WaitPose`: with `Sprite_Current` swapped to the acting
    party member's record (ObjTrio by the actor byte `0x904B34`), its
    animation +8 + 0x38, then its script ticked to its end; sound 0x100.
  - `_FocusActor`: after one frame, the task keeps the field's kind-2 point
    and elevation, the point moves to the member, the elevation is the ground
    there, and the field view is rebuilt round it (`Weretiger_ResetMapView`);
    a child of kind 1, parameter 0x3A, +1 3: the backdrop dim.
  - `_Burst`: the view's runs placed (`MapView_PlaceRuns`), then after a
    frame a second child, +1 1: the streak burst; sound 0x101. Step 4 is a
    bare ret (`0x437CC0`): the burst child moves the task on.
  - `_Copy`: a third child whose first 0x80 bytes are the member's record
    (+1 0): the copy that spawns four images. `_Release` releases the
    owner's tint and sets its +0 bit 0x40. Step 7 is the bare ret again: the
    copy moves the task on.
  - `_LoadForm`: two pairs of 0x50 x 0x48 sprites cross-fading
    (`Weretiger_DrawSprite`, images 1 and 0 at 0x80 - +9, 3 and 2 at +9), and
    the form's DAT file by the owner's word +0x2C and facing (0x2EC, 0x2ED,
    0x2EF or 0x2F0). `_FadeIn` fades on by 2 a frame to 0x80, then, once
    the file has loaded, gives the member its animation, palette
    (`0x80D380` + its +5 x 0x40), status tint and CLUT STP bits; sound
    0x102.
  - `_WaitScript` ticks the member's script to its end; `_Return` (once the
    burst has cleared bit 0 of +0xB) puts the kind-2 point and elevation back,
    rebuilds the view and moves to entry 2.
  - `Weretiger_End` places the runs again, counts +9 (0x40 from `_Return`) down, sets the
    done flag, the member's dword +0x134 bit 0 and byte +0x142 0, flags the
    target 0x40 and frees the task.
  - The children (`WeretigerChild_Task`, four entries by +1):
    - the copy (`WeretigerCopy_*`): `_Spawn` sets 32 CLUT strip words (0xFA0
      0, 0xFA1..0xFBF 0xFFFF, in the strip and its source), clears the VRAM
      rectangle (0x340, 0x100, 0xC0, 0x100) and makes four images, each a
      copy of the member's record at (0x20 or 0x80, 0x40 or 0x90) with
      animation facing + 0x38 or + 0x3C, the first and third with their own
      palettes; `_Tick` ticks the copy's script once and moves the parent on;
      then `BattleFx_FreeTask`;
    - the images (`WeretigerImage_*`): each plays its animation to the end in
      one call, then releases its tint and frees itself;
    - the burst (`WeretigerBurst_*`): six streaks a frame from a pool of 0xC0
      records at `0x683288` (`WeretigerStreak_Alloc6`), for 0x40 frames and
      then while its size +0xB grows to 0x18 on even frames (then the parent
      moves on), and while the parent reaches step 0xA; it ends when no
      streak is live, clearing bit 0 of the parent's +0xB. Each frame every
      live record is run as the pool's current one (`0x684788`):
      `WeretigerStreak_Start` gives it a random angle, two radii of +0xB
      pixels growing by 3.125 and 1.59 a frame (x 1.25 each frame after) and
      the member's screen point plus a facing offset; `_Draw` draws five
      shaded lines between the two radii at five angles
      (`WeretigerStreak_DrawLine`, a LINE_G2 on layer 2) and frees the
      record once the second radius passes 0x1C0;
    - the dim (`WeretigerDim_*`, a `.data` table): S30's
      `MagicFx_ClearCount9`, `_Down` (the backdrop tint down to -6),
      `_WaitOwner` (the parent at step 0xA), and `FxDim_Up`.
  - `Weretiger_ResetMapView` rebuilds the field's map view round the kind-2
    point: it clears two words of each 0x48-byte entry from `0x905EB6` to
    `0x929EC6`, sets the focus and origin, brings the elevation offset
    (`AreaMap_Word1E`, or -2 x the elevation) inside -0x1FF..0x1FF by
    shifting rows, maps all 56 x 28 cell items (`MapView_CellToMap`), resets
    the draw-item pool's free list, and calls `AreaMap_BakePatches` and
    `AreaMap_SetupEntries`. It is the same sequence an area set-up would run;
    which engine function it copies was not looked for.
- **Pilfer (MAGIC065).** `Item_CopyName` (`0x4B58F0`): an item's name into
  `Text_Records` (16 bytes, `Str_CopyN`) by index and category - 1 a weapon,
  2 armour, 3 an accessory, anything else a consumable. Pilfer's success
  (`0x4B57C0`, ours since round eight) and Steal's (`SkillSteal_Roll`
  `0x4F5140`) call it by address.
- **Tsunami (MAGIC066).** `Tsunami_Task`: `_Start` (the owner's position,
  the wave child - kind 1, parameter 0x11, +1 0 -, CLUT row 26 and row 2's
  first 16 words back from their source with the STP bit, the first word of
  row 2 then without it; sound 0x100), `_Rings` (after 0x18 frames the rings
  child, +1 1, and target flags 0x20), then MAGIC077's `Leech_WaitOrbs`
  (group S17: the target flagged 0x40 and the done flag at the wave's
  signal).
  - The wave (`TsunamiWave_*`, `.data` tables): it starts at the kind-2
    point + (-0x12, 0x88) << 16, moves 0x10 frames by -0xC000 in z, grows
    its width by 2 a frame to 0x80, shrinks width and amplitude by 2 to a
    width of 0x20, then the width by 1 and the amplitude by 2 to a width of
    0, and signals the parent (+0xB 0xFF).
    Each frame, under the actor matrix, `TsunamiWave_Draw`: 127 textured
    quads (tpage 0x340 / 0x100, CLUT row 0x1FA) 0x1000 wide stepping back by
    0x80 + width / 2, lifted by a sine of amplitude +0x14 whose phase turns
    with +0xB, the texture's v stepping in eight rows; then two gouraud quads
    (from edge 0x7E again) bright at the crest's far and near edges.
  - The rings (`TsunamiRings_*`): 0x80 frames; each frame a ring round every
    live enemy (0..7) and party member (0..2) at three heights (the offsets
    at `0x65AC54`), each ring up to four semi-transparent textured quads
    (CLUT row 0x1E2), one per ring phase (`0x68478C`) under 0x10, growing in
    radius (b x 4 + 0x32) and depth (b x 15) and fading (0x41 - (b >> 1) x
    4) as the phase steps; one more phase starts every three frames.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one exception
that follows the project's precedent: a phase past any of the twelve
dispatch tables (seven stack tables by +1 / +2 / +3, the streak record's
stack table by its +1, four `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3).

`Weretiger_DrawSprite` and `WeretigerStreak_DrawLine` take arguments; the
originals read them as bytes and words, and ours masks them the same.
`Gte_RotTrans` gets a flag pointer and `Gte_RotTransPers4` a depth and flag
pointer, as the originals push them.

## 3. Calls to other units

No raw-address call into a unit not yet ours. Two engine phases a stack
table holds, unnamed and Capcom's, are called by address as the table holds
them: `0x492750` (+1 on; entry 0 of `Weretiger_Task`) and `0x437CC0` (a bare
ret; entries 4 and 7 of `Weretiger_Run`).

By name, already ours: `Leech_WaitOrbs` (S17), `MagicFx_ClearCount9` (S30),
`FxDim_Up` and `BattleFx_FreeTask` (round eight), `MagicFx_PushActorMatrix`,
the field-view functions (`map_scroll.cpp`, `map_view.cpp`, `map_cells.cpp`),
`Str_CopyN`, `Battle_ActorIsOut`, and the GTE / GPU / sprite / sound library.

`Item_CopyName` is called by address by two earlier modules
(`magic_steal.cpp`, `magic_fx_reached_callees.h`) and listed as `"0x4B58F0"`
in the harness's standard set; all three keep working (the address is our
jmp now) and can be rebound to the name.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `WeretigerStreak_Offsets` | `0x65ABF8` | 4 pairs |
| `WeretigerDim_Steps` | `0x65AC00` | 4 |
| `WeretigerImage_UV` | `0x65AC10` | 4 of 4 bytes |
| `TsunamiChild_Kinds` | `0x65AC2C` | 2 |
| `TsunamiWave_Steps` | `0x65AC34` | 5 |
| `TsunamiRings_Steps` | `0x65AC48` | 3 |
| `TsunamiRing_Offsets` | `0x65AC54` | 3 of 3 dwords |
| `WeretigerStreak_Pool` | `0x683288` | 0xC0 of 0x1C bytes |
| `WeretigerStreak_Current` | `0x684788` | 1 |
| `TsunamiRing_Phases` | `0x68478C` | 4 |

The `.data` counts are where the next table starts (the dump of
`0x65ABE0..0x65AC70` read 2026-09-26; `0x65AC20` is Pilfer's
`Steal_RateTable`); the loops bound the `.bss` ones.

## 5. The fuzz

`BOF3X_SHADOW=magic_s14` runs `magic_harness::Run` over the 57 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s14_fuzz.cpp`:

- **Callees** (48 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own; the projection logs its SVECTORs through `deref`; the ring matrix
    push's GTE callees log theirs and write a result where the real ones
    write (S22's and S31's effects);
  - the sprite calls that act on `Sprite_Current` (`Sprite_EnsureAnimation`,
    `_ScriptTickOnce`, `_LoadPalette`, `_SetClutStp`, `_UpdateScreen`,
    `Battle_StatusTint`) log which sprite - Weretiger's steps swap
    `Sprite_Current` for the member's record round them;
  - `File_LoadDone` answers a bool (`eax` tested whole),
    `Battle_ActorIsOut` and the script tick a flag;
  - this group's own functions called directly, by address: the draws and
    pushes, the view rebuild and the streak allocator as `kPhase`, the
    streak run logging the pool's current record, the two with arguments
    by their masks.
- **Tables:** the four `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` .. `MapView_Origin`
  and the packet buffer; `Prim_VertexScratch`; `0x903850..0x90385F`;
  `Field_Kind2Z` / `X` and `MapView_Redraw`; the head (`0x905EB0`, 0x100)
  and tail (`0x929E00`, 0x130, with the focus, elevation, column and row) of
  the entries `Weretiger_ResetMapView` clears - the whole 0x24000 bytes
  exceed the harness's state; the streak pool, its current record and the
  ring phases; the six CLUT strip ranges the spells write; `AreaMap_Word1E`,
  `MapView_ScrollX` / `_ElevationOffset`, `MoveScript_FAWord`,
  `DrawTable_Count`, `MapView_CellItems`, `DrawItemPool_Free` and `_Top`.
  35,208 bytes of state in 29 regions.
- **Seed:** `0x684788` at a pool record; each dispatcher inside its table;
  each count-down one step before and at its threshold (+9 at 0 / 1, 0x5F /
  0x60, 0x7F / 0x80, 0xFA / 0xFB; the wave's width at 0x7E / 0x7F, 0x22 /
  0x23, 1 / 2; the burst size at 0x17 / 0x18; the fade at 0x7E / 0x80); the
  owner's step at 0xA half the time; the pool's live bytes cleared at several
  densities (all, one in 2, 8, 32, 64) for the allocator, the burst run and
  its end; the second streak radius either side of 0x1C0 after its growth;
  the view's offset word and elevation at the shift loops' bounds; the owner's
  word +0x2C and facing for the form's file; the rings' +9 and phases small;
  `Frame_Counter` even two times in three for the burst's growth. Arguments:
  `Weretiger_DrawSprite`'s image mostly 0..3, `Item_CopyName`'s category
  mostly 0..4 (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte, `Field_Kind2*`, the pool's current record, a pool byte, a ring phase,
  the task's +0x10 / +0x14 / +0x34 / +0x38 / +0x3E, and
  `MapView_ElevationOffset` (which the shift loops read back after each
  call).

Result in this worktree (2026-09-26):

    shadow      magic_s14 self-test: 114000 rounds over 57 functions (2000 each), 5917827 calls to the stand-ins,
                0 MISMATCHES; 35208 bytes of state (29 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (5,922,653 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

300 plants, each put in `magic_s14.cpp` one at a time by a script (not
committed) that planted, rebuilt, checked the build had recompiled the file,
ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s14`, restored; after the last
it restored, rebuilt and ran the clean self-test (0 mismatches). **300 of 300
refused**, every one by exit 3 with a count only in the functions the plant
touches (W-: Weretiger's task, C-: its copy and images' spawn, B- / S-: the
burst and its streaks, I- / D- / P- / R-: the images, the dim, the sprite draw,
the view rebuild, N-: `Item_CopyName`, T-: Tsunami). No equivalent mutant was
planted; none was left standing.

The first run left one standing, T6 (a 17th row-2 word): the fuzz's regions
held only the 16 words `Tsunami_Start` writes. Both row-2 regions were widened
to 32 words, and the whole set was run again on that fuzz: the table is the
second run.

The thinnest (fewer than 60 rounds):

- **B17** (Burst_End: 0xBF records): 4 - the last record alone live is rare;
- **S3** (Alloc6: 0xBF records): 13;
- **R12** (Reset: offset not read back): 17 - needs a disturbance of the
  offset between shifts;
- **T15** (Rings: target read before the create): 22;
- **T99** 58, **T56** 59.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| W1 | Task: entries 1/2 swapped | Weretiger_Task 1308 |
| W2 | Task: entry 0 the bare ret | Weretiger_Task 692 |
| W3 | Run: Pose / WaitPose swapped | Weretiger_Run 352 |
| W4 | Run: entry 4 _Copy | Weretiger_Run 174 |
| W5 | Run: FadeIn / WaitScript swapped | Weretiger_Run 306 |
| W6 | Pose: animation + 0x39 | Weretiger_Pose 2000 |
| W7 | Pose: animation on the task | Weretiger_Pose 1923 |
| W8 | Pose: sound 0x101 | Weretiger_Pose 2000 |
| W9 | Pose: +0xB 1 | Weretiger_Pose 2000 |
| W10 | WaitPose: +9 0x11 | Weretiger_WaitPose 653 |
| W11 | WaitPose: the tick on the task | Weretiger_WaitPose 1980 |
| W12 | WaitPose: on when not done | Weretiger_WaitPose 2000 |
| W13 | FocusActor: on at 1 | Weretiger_FocusActor 1021 |
| W14 | FocusActor: +0x38 from Kind2X | Weretiger_FocusActor 520 |
| W15 | FocusActor: +0x3E the elevation + 1 | Weretiger_FocusActor 520 |
| W16 | FocusActor: elevation at (z, x) | Weretiger_FocusActor 520 |
| W17 | FocusActor: elevation not sign-extended | Weretiger_FocusActor 259 |
| W18 | FocusActor: Kind2Z the record +0x3C | Weretiger_FocusActor 520 |
| W19 | FocusActor: child +1 2 | Weretiger_FocusActor 520 |
| W20 | FocusActor: no map rebuild | Weretiger_FocusActor 520 |
| W21 | FocusActor: +0xA 2 | Weretiger_FocusActor 520 |
| W22 | Burst: runs placed at +0xA 0 | Weretiger_Burst 2000 |
| W23 | Burst: +0xB and 0xFD | Weretiger_Burst 361 |
| W24 | Burst: sound 0x102 | Weretiger_Burst 472 |
| W25 | Burst: child +1 0 | Weretiger_Burst 472 |
| W26 | Burst: on at 1 | Weretiger_Burst 997 |
| W27 | copy: 0x7C bytes | Weretiger_Copy 2000, WeretigerCopy_Spawn 2000 |
| W28 | Copy: +3 not cleared | Weretiger_Copy 1994 |
| W29 | Copy: +5 0x3B | Weretiger_Copy 2000 |
| W30 | Copy: parameter 0x3B | Weretiger_Copy 2000 |
| W31 | Release: at 1 | Weretiger_Release 509 |
| W32 | Release: owner bit 0x20 | Weretiger_Release 375 |
| W33 | Release: the task's tint | Weretiger_Release 418 |
| W34 | CrossFade: shade 0x81 - +9 | Weretiger_LoadForm 2000, Weretiger_FadeIn 2000 |
| W35 | CrossFade: image 3 drawn as 2 | Weretiger_LoadForm 2000, Weretiger_FadeIn 2000 |
| W36 | CrossFade: blend 1 for image 1 | Weretiger_LoadForm 2000, Weretiger_FadeIn 2000 |
| W37 | LoadForm: 0x2EE | Weretiger_LoadForm 285 |
| W38 | LoadForm: near is facing 0 or 2 | Weretiger_LoadForm 541 |
| W39 | LoadForm: the owner's +0x2E | Weretiger_LoadForm 868 |
| W40 | LoadForm: Sprite_Current not put back | Weretiger_LoadForm 1859 |
| W41 | FadeIn: +9 up by 1 | Weretiger_FadeIn 1493 |
| W42 | FadeIn: at 0x7E | Weretiger_FadeIn 1025 |
| W43 | FadeIn: palette by +6 | Weretiger_FadeIn 342 |
| W44 | FadeIn: palette index 1 | Weretiger_FadeIn 344 |
| W45 | FadeIn: +0 bit 0x80 | Weretiger_FadeIn 256 |
| W46 | FadeIn: sound 0x103 | Weretiger_FadeIn 344 |
| W47 | FadeIn: not loaded goes on | Weretiger_FadeIn 507 |
| W48 | FadeIn: status of +0x92 | Weretiger_FadeIn 344 |
| W49 | FadeIn: animation +9 | Weretiger_FadeIn 343 |
| W50 | WaitScript: on when not done | Weretiger_WaitScript 2000 |
| W51 | WaitScript: the tick on the task | Weretiger_WaitScript 1980 |
| W52 | Return: bit 1 | Weretiger_Return 1010 |
| W53 | Return: elevation unsigned | Weretiger_Return 739 |
| W54 | Return: +1 3 | Weretiger_Return 1487 |
| W55 | Return: +9 0x41 | Weretiger_Return 1487 |
| W56 | End: done bit 8 | Weretiger_End 340 |
| W57 | End: +0x134 bit 2 | Weretiger_End 367 |
| W58 | End: +0x142 1 | Weretiger_End 490 |
| W59 | End: flags on the actor | Weretiger_End 442 |
| W60 | End: on at 1 | Weretiger_End 1001 |
| C1 | Child_Task: entry 1 the image | WeretigerChild_Task 508 |
| C2 | Copy_Run: Step / FreeTask swapped | WeretigerCopy_Run 1338 |
| C3 | Copy_Run: update at +0 clear | WeretigerCopy_Run 2000 |
| C4 | Copy_Step: entries swapped | WeretigerCopy_Step 2000 |
| C5 | Spawn: strip word 0xFA0 1 | WeretigerCopy_Spawn 2000 |
| C6 | Spawn: 30 words | WeretigerCopy_Spawn 2000 |
| C7 | Spawn: source 0x7FFF | WeretigerCopy_Spawn 2000 |
| C8 | Spawn: dirty not set | WeretigerCopy_Spawn 1996 |
| C9 | Spawn: rect height 0xFF | WeretigerCopy_Spawn 2000 |
| C10 | Spawn: first image +0x27 0x7C | WeretigerCopy_Spawn 1873 |
| C11 | Spawn: second image animation + 0x3C | WeretigerCopy_Spawn 1908 |
| C12 | Spawn: third y 0x41 | WeretigerCopy_Spawn 1923 |
| C13 | Spawn: fourth x 0x81 | WeretigerCopy_Spawn 2000 |
| C14 | Spawn: third palette index 0 | WeretigerCopy_Spawn 2000 |
| C15 | Spawn: first palette 0x80D400 | WeretigerCopy_Spawn 2000 |
| C16 | Spawn: images keep bit 0x40 | WeretigerCopy_Spawn 1100 |
| C17 | Spawn: images +1 3 | WeretigerCopy_Spawn 2000 |
| C18 | Spawn: +0x24 or 0x80 | WeretigerCopy_Spawn 1090 |
| C19 | Spawn: first image not current | WeretigerCopy_Spawn 1871 |
| C20 | Spawn: the task not put back | WeretigerCopy_Spawn 1988 |
| C21 | Spawn: +2 stepped, not +3 | WeretigerCopy_Spawn 2000 |
| C22 | Spawn: status of the image's +0x90 | WeretigerCopy_Spawn 2000 |
| C23 | Spawn: owner the image | WeretigerCopy_Spawn 2000 |
| C24 | Tick: owner not stepped | WeretigerCopy_Tick 2000 |
| C25 | Tick: +3 1 | WeretigerCopy_Tick 2000 |
| C26 | Tick: +2 by 2 | WeretigerCopy_Tick 2000 |
| B1 | Burst_Run: Grow / WaitOwner swapped | WeretigerBurst_Run 826 |
| B2 | Burst_Run: tpage 0x2E | WeretigerBurst_Run 2000 |
| B3 | Burst_Run: 0xBF records | WeretigerBurst_Run 1529 |
| B4 | Burst_Run: dead records run too | WeretigerBurst_Run 1983 |
| B5 | Burst_Run: layer 3 | WeretigerBurst_Run 2000 |
| B6 | Burst_Start: +9 0x3F | WeretigerBurst_Start 1989 |
| B7 | Burst_Start: +0xB 1 | WeretigerBurst_Start 1982 |
| B8 | Burst_Start: no allocation | WeretigerBurst_Start 2000 |
| B9 | Emit: on at 1 | WeretigerBurst_Emit 959 |
| B10 | Emit: allocation after the count | WeretigerBurst_Emit 68 |
| B11 | Grow: odd frames | WeretigerBurst_Grow 2000 |
| B12 | Grow: at 0x19 | WeretigerBurst_Grow 396 |
| B13 | Grow: owner not stepped | WeretigerBurst_Grow 1207 |
| B14 | Grow: +0xB up by 2 | WeretigerBurst_Grow 474 |
| B15 | Burst_WaitOwner: at 0xB | WeretigerBurst_WaitOwner 1014 |
| B16 | Burst_WaitOwner: no allocation | WeretigerBurst_WaitOwner 2000 |
| B17 | Burst_End: 0xBF records | WeretigerBurst_End 4 |
| B18 | Burst_End: owner bit 1 | WeretigerBurst_End 755 |
| B19 | Burst_End: the records' +1 | WeretigerBurst_End 1021 |
| S1 | Alloc6: five | WeretigerStreak_Alloc6 1531 |
| S2 | Alloc6: +0 2 | WeretigerStreak_Alloc6 1984 |
| S3 | Alloc6: 0xBF records | WeretigerStreak_Alloc6 13 |
| S4 | Alloc6: marks the live | WeretigerStreak_Alloc6 2000 |
| S5 | Streak_Run: entries swapped | WeretigerStreak_Run 2000 |
| S6 | Streak_Start: angle mask 0x7FF | WeretigerStreak_Start 1013 |
| S7 | Streak_Start: growth 0x32001 | WeretigerStreak_Start 2000 |
| S8 | Streak_Start: growth 0x19000 | WeretigerStreak_Start 2000 |
| S9 | Streak_Start: radius sl 15 | WeretigerStreak_Start 1991 |
| S10 | Streak_Start: y offset the x byte | WeretigerStreak_Start 1906 |
| S11 | Streak_Start: offset unsigned | WeretigerStreak_Start 495 |
| S12 | Streak_Start: x from +0x30 | WeretigerStreak_Start 2000 |
| S13 | Streak_Start: +2 stepped | WeretigerStreak_Start 2000 |
| S14 | Streak_Draw: growth x 5 / 8 | WeretigerStreak_Draw 2000 |
| S15 | Streak_Draw: second growth x 3 | WeretigerStreak_Draw 2000 |
| S16 | Streak_Draw: +0xC by +0x18 | WeretigerStreak_Draw 2000 |
| S17 | Streak_Draw: four lines | WeretigerStreak_Draw 2000 |
| S18 | Streak_Draw: angle step 2 | WeretigerStreak_Draw 2000 |
| S19 | Streak_Draw: radius sar 15 | WeretigerStreak_Draw 2000 |
| S20 | Streak_Draw: x1 centre +8 | WeretigerStreak_Draw 2000 |
| S21 | Streak_Draw: x0 / y0 swapped | WeretigerStreak_Draw 2000 |
| S22 | Streak_Draw: freed at 0x1C0 | WeretigerStreak_Draw 578 |
| S23 | Streak_Draw: +1 1 | WeretigerStreak_Draw 681 |
| S24 | Streak_Draw: y1 from the cosine | WeretigerStreak_Draw 2000 |
| S25 | Streak_Draw: mask 0xFFFF8000 | WeretigerStreak_Draw 292 |
| S26 | Line: colour 0xF1 | WeretigerStreak_DrawLine 2000 |
| S27 | Line: far red 1 | WeretigerStreak_DrawLine 2000 |
| S28 | Line: x0 unsigned | WeretigerStreak_DrawLine 988 |
| S29 | Line: opaque | WeretigerStreak_DrawLine 2000 |
| S30 | Line: size 0x20 | WeretigerStreak_DrawLine 2000 |
| I1 | Image_Run: Play / End swapped | WeretigerImage_Run 1307 |
| I2 | Image_Run: update at +0 clear | WeretigerImage_Run 2000 |
| I3 | Play: animation +0xA | WeretigerImage_Play 1992 |
| I4 | Play: one tick | WeretigerImage_Play 660 |
| I5 | Play: +1 stepped | WeretigerImage_Play 2000 |
| I6 | Next: +3 | WeretigerFx_Next 2000 |
| I7 | Image_End: the owner's tint | WeretigerImage_End 1766 |
| I8 | Image_End: not freed | WeretigerImage_End 2000 |
| D1 | Dim_Run: entry 1 _WaitOwner | WeretigerDim_Run 520 |
| D2 | Dim_Run: entry 3 MagicFx_ClearCount9 | WeretigerDim_Run 529 |
| D3 | Dim_Down: at 0xF9 | WeretigerDim_Down 991 |
| D4 | Dim_Down: level unsigned | WeretigerDim_Down 1482 |
| D5 | Dim_Down: +9 up | WeretigerDim_Down 2000 |
| D6 | Dim_WaitOwner: at 9 | WeretigerDim_WaitOwner 1029 |
| D7 | Dim_WaitOwner: the task's +2 | WeretigerDim_WaitOwner 897 |
| P1 | Sprite: tpage mode 1 | Weretiger_DrawSprite 2000 |
| P2 | Sprite: abr and 3 | Weretiger_DrawSprite 1973 |
| P3 | Sprite: tpage not masked | Weretiger_DrawSprite 2000 |
| P4 | Sprite: first link 0x10 | Weretiger_DrawSprite 2000 |
| P5 | Sprite: +5 shade + 1 | Weretiger_DrawSprite 2000 |
| P6 | Sprite: image 2 at - 0x20 | Weretiger_DrawSprite 334 |
| P7 | Sprite: y - 0x3F | Weretiger_DrawSprite 2000 |
| P8 | Sprite: v from +1 | Weretiger_DrawSprite 1279 |
| P9 | Sprite: width 0x48 | Weretiger_DrawSprite 2000 |
| P10 | Sprite: second link 0x18 | Weretiger_DrawSprite 2000 |
| R1 | Reset: stride 0x46 | Weretiger_ResetMapView 2000 |
| R2 | Reset: first word at - 0x12 | Weretiger_ResetMapView 2000 |
| R3 | Reset: stops at 0x929E00 | Weretiger_ResetMapView 2000 |
| R4 | Reset: focus 0x7FFE | Weretiger_ResetMapView 2000 |
| R5 | Reset: focus z sar 9 | Weretiger_ResetMapView 2000 |
| R6 | Reset: origin - 0x17 | Weretiger_ResetMapView 2000 |
| R7 | Reset: origin z + 4 | Weretiger_ResetMapView 2000 |
| R8 | Reset: draw count 1 | Weretiger_ResetMapView 2000 |
| R9 | Reset: offset from +elevation | Weretiger_ResetMapView 319 |
| R10 | Reset: shift at 0x201 | Weretiger_ResetMapView 221 |
| R11 | Reset: shift back at -0x1FF | Weretiger_ResetMapView 216 |
| R12 | Reset: offset not read back | Weretiger_ResetMapView 17 |
| R13 | Reset: scroll 1 | Weretiger_ResetMapView 2000 |
| R14 | Reset: redraw 2 | Weretiger_ResetMapView 2000 |
| R15 | Reset: height scale kept | Weretiger_ResetMapView 1990 |
| R16 | Reset: 27 columns | Weretiger_ResetMapView 2000 |
| R17 | Reset: item +2 1 | Weretiger_ResetMapView 2000 |
| R18 | Reset: row and column swapped | Weretiger_ResetMapView 2000 |
| R19 | Reset: free list from 1 | Weretiger_ResetMapView 2000 |
| R20 | Reset: top 0 | Weretiger_ResetMapView 2000 |
| R21 | Reset: row 0x36 | Weretiger_ResetMapView 2000 |
| R22 | Reset: column 0x1C | Weretiger_ResetMapView 2000 |
| R23 | Reset: entries before the patches | Weretiger_ResetMapView 2000 |
| R24 | Reset: FAWord kept | Weretiger_ResetMapView 2000 |
| R25 | Reset: F3Divisor kept | Weretiger_ResetMapView 2000 |
| N1 | Name: weapons 0x1B | Item_CopyName 263 |
| N2 | Name: armour at 4 | Item_CopyName 529 |
| N3 | Name: accessories 0x16 | Item_CopyName 272 |
| N4 | Name: consumables 0x18 | Item_CopyName 1196 |
| N5 | Name: 15 bytes | Item_CopyName 2000 |
| N6 | Name: index and 0x1FF | Item_CopyName 1024 |
| N7 | Name: category and 0x1FF | Item_CopyName 373 |
| N8 | Name: answer + 1 | Item_CopyName 2000 |
| T1 | Tsunami_Task: Start / Rings swapped | Tsunami_Task 1329 |
| T2 | Start: z from the owner's +0x34 | Tsunami_Start 2000 |
| T3 | Start: parameter 0x12 | Tsunami_Start 2000 |
| T4 | Start: child +1 1 | Tsunami_Start 2000 |
| T5 | Start: row 26 bit 0x4000 | Tsunami_Start 2000 |
| T6 | Start: row 2 17 words | Tsunami_Start 2000 |
| T7 | Start: word 0x200 keeps its STP | Tsunami_Start 992 |
| T8 | Start: +9 0x17 | Tsunami_Start 2000 |
| T9 | Start: sound 0x101 | Tsunami_Start 2000 |
| T10 | Start: strip not dirty | Tsunami_Start 1993 |
| T11 | Rings: flags 0x10 | Tsunami_Rings 530 |
| T12 | Rings: child +1 0 | Tsunami_Rings 530 |
| T13 | Rings: at 1 | Tsunami_Rings 1019 |
| T14 | Rings: +2 stepped | Tsunami_Rings 530 |
| T15 | Rings: target read before the create | Tsunami_Rings 22 |
| T16 | Child_Task: entries swapped | TsunamiChild_Task 2000 |
| T17 | Wave_Run: Grow / Shrink swapped | TsunamiWave_Run 806 |
| T18 | Wave_Run: drawn at 0xFF | TsunamiWave_Run 977 |
| T19 | Wave_Run: no pop | TsunamiWave_Run 1029 |
| T20 | Wave_Start: x - 0x110000 | TsunamiWave_Start 2000 |
| T21 | Wave_Start: z + 0x870000 | TsunamiWave_Start 2000 |
| T22 | Wave_Start: height the owner's +0x38 | TsunamiWave_Start 2000 |
| T23 | Wave_Start: +0xB 0x1B | TsunamiWave_Start 2000 |
| T24 | Wave_Start: +0x14 0xF1 | TsunamiWave_Start 2000 |
| T25 | Wave_Start: +9 0x11 | TsunamiWave_Start 2000 |
| T26 | Advance: step 0xFFFF8000 | TsunamiWave_Advance 2000 |
| T27 | Advance: on at 1 | TsunamiWave_Advance 1022 |
| T28 | Grow: angle mask 0x3F | TsunamiWave_Grow 978 |
| T29 | Grow: at 0x7E | TsunamiWave_Grow 495 |
| T30 | Grow: width up by 1 | TsunamiWave_Grow 2000 |
| T31 | Shrink: amplitude - 1 | TsunamiWave_Shrink 2000 |
| T32 | Shrink: at 0x22 | TsunamiWave_Shrink 479 |
| T33 | Shrink: angle up | TsunamiWave_Shrink 2000 |
| T34 | Wave_End: width - 2 | TsunamiWave_End 2000 |
| T35 | Wave_End: owner +0xB 0xFE | TsunamiWave_End 504 |
| T36 | Wave_End: amplitude kept | TsunamiWave_End 1999 |
| T37 | WaveDraw: first tpage 0xB4 | TsunamiWave_Draw 2000 |
| T38 | WaveDraw: closing tpage 0x94 | TsunamiWave_Draw 2000 |
| T39 | WaveDraw: amplitude from +0x16 | TsunamiWave_Draw 2000 |
| T40 | WaveDraw: first angle mask 0x3F | TsunamiWave_Draw 974 |
| T41 | WaveDraw: 126 textured quads | TsunamiWave_Draw 2000 |
| T42 | WaveDraw: near x 0xFFF | TsunamiWave_Draw 2000 |
| T43 | WaveDraw: far x 0x1001 | TsunamiWave_Draw 2000 |
| T44 | WaveDraw: y step - 0x7F | TsunamiWave_Draw 2000 |
| T45 | WaveDraw: width sar 2 | TsunamiWave_Draw 2000 |
| T46 | WaveDraw: shade 0x81 | TsunamiWave_Draw 2000 |
| T47 | WaveDraw: step-4 shade x 4 | TsunamiWave_Draw 952 |
| T48 | WaveDraw: CLUT row 0x1FB | TsunamiWave_Draw 2000 |
| T49 | WaveDraw: v mask 0xF | TsunamiWave_Draw 2000 |
| T50 | WaveDraw: u 0xF0 | TsunamiWave_Draw 2000 |
| T51 | WaveDraw: +0x45 without 0x10 | TsunamiWave_Draw 2000 |
| T52 | WaveDraw: textured depths 10B | TsunamiWave_Draw 2000 |
| T53 | WaveDraw: back one edge | TsunamiWave_Draw 2000 |
| T54 | WaveDraw: three gouraud quads | TsunamiWave_Draw 2000 |
| T55 | WaveDraw: gouraud shade 0xC1 | TsunamiWave_Draw 1964 |
| T56 | WaveDraw: gouraud step-4 x 8 | TsunamiWave_Draw 59 |
| T57 | WaveDraw: bright edge on 0x7F | TsunamiWave_Draw 2000 |
| T58 | WaveDraw: gouraud commit 0x48 | TsunamiWave_Draw 2000 |
| T59 | WaveDraw: tpage (1, 2) | TsunamiWave_Draw 2000 |
| T60 | WaveDraw: sine at the angle + 1 | TsunamiWave_Draw 2000 |
| T61 | WaveDraw: near z the y | TsunamiWave_Draw 2000 |
| T62 | WaveDraw: the step byte +3 | TsunamiWave_Draw 949 |
| T63 | Rings_Run: Grow / End swapped | TsunamiRings_Run 652 |
| T64 | Rings_Start: phases 1 | TsunamiRings_Start 2000 |
| T65 | Rings_Start: +9 1 | TsunamiRings_Start 2000 |
| T66 | Rings_Grow: at 0x5F | TsunamiRings_Grow 516 |
| T67 | Rings_Grow: no draw | TsunamiRings_Grow 2000 |
| T68 | Rings_End: at 0x7F | TsunamiRings_End 482 |
| T69 | Rings_End: +9 up by 2 | TsunamiRings_End 2000 |
| T70 | RingsDraw: first tpage 0x36 | TsunamiRings_Draw 2000 |
| T71 | RingsDraw: closing tpage 0x16 | TsunamiRings_Draw 2000 |
| T72 | RingsDraw: 7 enemies | TsunamiRings_Draw 2000 |
| T73 | RingsDraw: enemy index e + 2 | TsunamiRings_Draw 2000 |
| T74 | RingsDraw: 2 party members | TsunamiRings_Draw 2000 |
| T75 | RingsDraw: the out drawn | TsunamiRings_Draw 2000 |
| T76 | RingsDraw: two heights for the party | TsunamiRings_Draw 2000 |
| T77 | RingsDraw: stepped below 3 j + 1 | TsunamiRings_Draw 96 |
| T78 | RingsDraw: kept below 0x10 at step 2 | TsunamiRings_Draw 1175 |
| T79 | RingsDraw: mask 0x1F | TsunamiRings_Draw 642 |
| T80 | RingAt: offsets 11 apart | TsunamiRings_Draw 1957 |
| T81 | RingAt: +0xA the height | TsunamiRings_Draw 1766 |
| T82 | RingAt: z offset the y one | TsunamiRings_Draw 1998 |
| T83 | RingAt: no pop | TsunamiRings_Draw 2000 |
| T84 | RingsDraw: enemy stride 0x124 | TsunamiRings_Draw 1361 |
| T85 | Quads: drawn at 3 j == +9 | TsunamiRing_DrawQuads 156 |
| T86 | Quads: phase 0x10 drawn | TsunamiRing_DrawQuads 209 |
| T87 | Quads: radius + 0x33 | TsunamiRing_DrawQuads 1681 |
| T88 | Quads: depth x 14 | TsunamiRing_DrawQuads 1649 |
| T89 | Quads: angle 0x700 | TsunamiRing_DrawQuads 1682 |
| T90 | Quads: depth positive | TsunamiRing_DrawQuads 1649 |
| T91 | Quads: x from the sine | TsunamiRing_DrawQuads 1682 |
| T92 | Quads: CLUT row 0x1E3 | TsunamiRing_DrawQuads 1682 |
| T93 | Quads: tpage (0, 2) | TsunamiRing_DrawQuads 1682 |
| T94 | Quads: u 0x1F | TsunamiRing_DrawQuads 1682 |
| T95 | Quads: shade 0x40 - | TsunamiRing_DrawQuads 1682 |
| T96 | Quads: shade by b >> 2 | TsunamiRing_DrawQuads 1610 |
| T97 | Quads: +6 shade + 1 | TsunamiRing_DrawQuads 1682 |
| T98 | Quads: layer 2 | TsunamiRing_DrawQuads 1682 |
| T99 | Quads: the phase for the shade not read again | TsunamiRing_DrawQuads 58 |
| T100 | Ring matrix: angle 0xC01 | TsunamiRing_PushMatrix 2000 |
| T101 | Ring matrix: turned about y | TsunamiRing_PushMatrix 2000 |
| T102 | Ring matrix: x sar 8 | TsunamiRing_PushMatrix 2000 |
| T103 | Ring matrix: z - 0x3FFF | TsunamiRing_PushMatrix 2000 |
| T104 | Ring matrix: height / 4 | TsunamiRing_PushMatrix 2000 |
| T105 | Ring matrix: height sar 1 | TsunamiRing_PushMatrix 494 |
| T106 | Ring matrix: camera second | TsunamiRing_PushMatrix 2000 |
| T107 | Ring matrix: no translation | TsunamiRing_PushMatrix 2000 |

## 7. What nothing reached

No recorded route casts Weretiger or Tsunami (queue §5); the combat route's
first-call trace does not enter `Item_CopyName` either (a theft that
succeeds calls it). The live check is the owner casting them, with a save
that has them or DIV-0045's cheat. Things to look for:

- Weretiger: the member posing, the view re-centring on it, a burst of short
  lines, four images of the member, the backdrop dimming, a cross-fade of two
  sprite pairs into the new form, the view put back.
- Pilfer / Steal: the stolen item's name in the battle message.
- Tsunami: a wave of textured quads rolling across the field, then rings
  opening round every live actor at three heights.

Which party member or form `Weretiger_LoadForm`'s four files hold, and what
`Weretiger_ResetMapView`'s view rebuild is for in a battle, were not read.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96, D97, D99 and D124 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the seven stack tables, the
  streak record's by its +1, and the four `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Weretiger_FocusActor`, `_Burst`, `_Copy`, `WeretigerCopy_Spawn` (four
  calls), `Tsunami_Start` and `Tsunami_Rings`: slot 255 is past the image's
  end, an access violation in ours as in the original.
- **MAGIC064 indexes the party records by the actor byte, unchecked**: with
  an enemy acting (3..10) it reads, copies and writes (`Weretiger_End`'s
  +0x134 / +0x142) "records" past the three party ones.
- **`WeretigerStreak_Alloc6` does not set a record's step +1**: it relies on
  the previous streak's end (`WeretigerStreak_Draw` clears +0 and +1). A
  record left live with any other +1 would dispatch past the two-entry table.
- **`WeretigerImage_Play` ticks the image's script until it reports its end,
  in one call**: a script that never ends hangs the frame.
- **`WeretigerStreak_Start` indexes its offset pairs by the member's facing
  +8, unchecked** (four pairs), and `Weretiger_DrawSprite` its UV entries by
  the image (four; its callers pass 0..3).
- **`Item_CopyName` checks neither index nor category**: a category of 0 or
  above 3 reads the consumable names, and an index past a table reads the
  next.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 53 lines under a `group S14` comment: 47 new,
plus six host (or padded) extents re-listed smaller (`004B4BD0 26`,
`004B50A0 7A`, `004B5350 158`, `004B58F0 A8`, `004B5CB0 4AB`,
`004B6620 A1`). Four were listed right already (`004B4B90`, `004B5240`,
`004B6200`, `004B63C0`).
