# Group S06: MAGIC008's doubles and MAGIC020's shadows and slashes

**Status:** IN PROGRESS (2026-09-26). All 56 functions are ours
(`src/game/magic_s06.cpp`, shadow name `magic_s06`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 112,000 rounds. 203 negative controls: 202 refused by a count (exit 3), one an equivalent mutant (section 6). Nothing recorded
casts these abilities, so this is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S06
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability ids (read one id down, hypotheses) | Extent | Functions |
|--:|---|---|---|---|--:|
| 42 | 0x230 | MAGIC008 | 0x2 Gambit, 0x7 Mind Flay, 0x8 Blind, 0x1C Devour, 0x3D (no label), 0x8E Syphon, 0xA4 Feign Swing, 0xA5 Backhand, 0xAA Risky Blow | `0x4A2190..0x4A3140` | 35 |
| 8 | 0x23A | MAGIC020 | 0x14 Disembowel | `0x4A3150..0x4A3B18` | 21 |

The extents are `tools/magic_rows.py --unit MAGIC008 / MAGIC020 --clones`
(capstone recursive descent; no jump table, nothing `REFUSED`). All 56
functions lie in the units' extents; none was found inside or missing from
them, and none was ours before. The names below say what the code does; the
labels in the table are the sibling's read one id down
([`cut-content.md`](cut-content.md) §2). What any of it looks like in play has
not been measured.

**A tool gap, for the coordinator.** `magic_rows.py` lists a stack table's
handlers only where the table is built with `mov dword [esp + k], imm32`.
`Magic008TwoBlows_Run` (`0x4A2DC0`) and `Magic008ThreeBlows_Run` (`0x4A2F40`)
load three of theirs into a register first (`mov ecx, 0x4A2FB0`, `mov eax,
0x4A2E20` / `0x4A2FF0`, `mov edx, 0x4A3020`) and store the register, so the
tool's `Imm` lists missed them and the clones would have called Capcom's
handlers for real. They are added by hand here (offsets 4, 9 and 0x31; the
harness checks only the value at the offset). Other groups' tables may do the
same: a coverage line missing a handler the table holds is the sign.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **MAGIC008 (row 42): the doubles.**
  - `Magic008_Task`, the kind-2 task: `Magic008_Start`, `Magic008_Apply`,
    then `MagicFx_EndWhenChildrenDone` (a three-entry stack table by +1).
  - `Magic008_Start` takes the owner's direction and position, sets +4 to
    `Magic008_BuffStat` (7 when the ability `0x904B80` is 7, else 0xFF),
    plays the actor's animation 0xC, and makes a **double**: a kind-1 task
    (parameter 0x1F) whose first 0x80 bytes are a copy of the acting actor's
    record - its sprite - with body 2. It sets the owner's +0 bit 0x40 and
    copies CLUT strip words 0x1A00..0x1A1F back from their source.
  - `Magic008_Apply` waits for the double to finish (+0xB 0); unless +4 is
    0xFF it moves to the source sprite, applies one buff to the target
    (`MagicFx_ApplyBuff`, the stat `MagicFx_BuffStats[+4 & 3]`) and makes its
    popup (kind 1, 0x48: `BuffPopup_Task`, showing the stat when the buff
    took, else 8). Then the actor's animation 4 and the owner's bit cleared.
    So by reading only ability 7 applies anything here.
  - `Magic008Double_Task` dispatches the double's body by +1 through
    `Magic008Double_Bodies` (six entries). `Magic008_Start` makes body 2
    only; the other five are reached from MAGIC017, 018 and 019
    (`analysis/magic_funcs.tsv`), whose code is group S05's.
    - Body 0, `Magic008Grow_*`: scale 0x10000, grows (x by 0x400, y by 0x800
      a frame) for 30 frames, ticks its script for the effect size and flags
      the target 0x40 with a sound, waits for the script's end (sound 0x101),
      shrinks for 15 frames, sets the owner's +0xB 0xFF, frees.
    - Body 1, `Magic008Glow_*`: at the field's kind-2 point, colour 0x80
      brightening by 3 for 32 frames, strike and wait as body 0, then fades
      by 3 for 32 frames and frees.
    - Body 2, `Magic008Mirror_*`: the effect size, the script ticked until it
      strikes (sound) and again until it ends (target flag 0x40, the owner's
      +0xB down), then three frames of a screen flash
      (`Magic008_DrawFlash`: two semi-transparent gouraud quads, white at the
      screen's top and bottom edges and +9 x 16 at y 120) unless the ability
      is 0xA4, and free.
    - Body 3, `Magic008Dash_*`: colour 0xB0 at the owner, offset by
      `Magic008Dash_Offsets[+0xB]` turned by the owner's direction (the
      engine's `0x446770`); on odd frames on the owner ticking its script,
      on even frames beside it; the double with +0xB 0 flags the target and
      sounds; step 3 is MAGIC018/019's `0x4A01C0`; then fades by 3 to 0x80
      and counts itself off the owner.
    - Bodies 4 and 5, `Magic008TwoBlows_Run` / `Magic008ThreeBlows_Run`: the
      effect size (`BattleFx_SetSize`), then two or three rounds of
      `Magic008Blow_Strike` (script ticks, then sound and target flag 0x40),
      `_WaitTwo` / `_WaitThree` (the script's end; on the last round the
      owner's +0xB 0xFF) and `_ReactTwo` / `_ReactThree`: when the target is
      out the owner is told and the double skips to its free; else it waits
      while the target's state +1 is 6 (reacting), then starts the next
      swing - a member's animation +8 + 0xC with the upload that call just
      queued cancelled, or an enemy's animation 2 (`0x435A70`). `_ReactThree`
      also cuts the blows short one time in eight (`Rand & 7`) when the
      ability is 0xB (row 68's, MAGIC018 - group S05's overlay).
- **MAGIC020 (row 8): shadows and slashes.**
  - `Magic020_Task`: `_Start`, `_Darken`, `_Hold`, `_End`, then
    `MagicFx_DoneAndFree` and `MagicFx_FlagTargetEnd` (group E's), by +1.
  - `Magic020_Start`: the owner's direction and position, animation 0xC,
    four shadows (doubles of the actor, kind 1 0x2E, body 1, delays 4..1 and
    +0xB 3..0) and a lead (body 0); the owner's sprite CLUT to the effect row
    (`SpriteClut_CopyToFxRow`), its STP bits on this task's; a member's sound;
    the owner's bit 0x40.
  - The lead (`Magic020Lead_*`) steps toward the source sprite by 0x60 until
    within 0x20000, then tells the parent (+0xB 0xFF); the parent
    (`Magic020_Darken`) draws the darkening tile (grey +9 x 15, +9 rising by 4
    a frame) until +9 is 0x10, then makes eight slashes (body 2, +0xB 0..7, delays
    1, 9, .., 57) and counts them in its +0xB. Each slash
    (`Magic020Slash_*`) waits its delay, takes the source sprite's direction
    and position, bank 0x1D and the animation +0xB (colour row from
    `Magic020Slash_Animations`), plays its script to the end (the last,
    +0xB 7, flags the target 0x40) and counts itself off. The slashes run
    with the frame-offset table `0x9039D8` switched to the effects' `0x8C5D80`
    round each step. The parent holds the fade until they are done, then
    lets it fall (`_Hold`). The lead ticks its script, waits for the slashes,
    flashes to 0xC0, dims to 0x80 back at the owner, then brightens by 0x10
    until the colour wraps to 0 and tells the parent (+0xB 0xFF); `_End`
    restores the effect CLUT row, clears the owner's bit and plays animation 4.
  - The shadows (`Magic020Shadow_*`) wait their delay, darken by their order
    (colour (0xFF - +0xB) x 0x14, a byte) and step toward the source sprite
    until near it, then free.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the project's
precedent exceptions ([`magic_fx_reached.md`](magic_fx_reached.md) §3; the
owner's word on aborts, [`takeover-queue-round9.md`](takeover-queue-round9.md) §6):

- a phase past any of the eleven dispatch tables (nine stack tables, two
  `.data` tables) aborts;
- `Magic008Dash_Start` aborts on a +0xB past `Magic008Dash_Offsets`' three
  pairs, and `Magic020Slash_Start` on a +0xB past `Magic020Slash_Animations`'
  twelve bytes, where the originals read whatever follows (the next table).
  Their creators write +0xB 0..7 for the slashes (`Magic020_Darken`); who
  writes a dash double's +0xB is MAGIC017..019's code (group S05), not read
  here.

`Magic008_BuffStat` computes its answer from al over whatever eax held; the
mask leaves exactly 7 or 0xFF in the whole of eax, which ours returns.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4A01C0` | MAGIC018/019 (group S05) | entry 3 of `Magic008Dash_Run`'s stack table |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S22, S23, S31 call it) |
| `0x435A70` | engine, unnamed | an enemy's animation: `BattleEnemy_SetAnimation` with `0x939AD8` pointed at enemy `actor` and put back |

By name, already ours: `MagicFx_EndWhenChildrenDone` (S30),
`MagicFx_DoneAndFree` and `MagicFx_FlagTargetEnd` (E), `BattleFx_SetSize` and
`BattleFx_FreeTask` (round eight), the effect library (L: `MagicFx_ApplyBuff`,
`MagicFx_StepToward`, `MagicFx_NearSprite`, the `SpriteClut_*` three,
`BattleActor_*`), and the GPU / sprite / sound library.

**Called into from other units** (a rel32 scan of the exe):
`Magic008_DrawFlash` (`0x4A29C0`) is called directly from `0x49BE3B`
(MAGIC004, group S02) and `0x4A3DCB` (MAGIC021, group S07) - those groups call
it by address until they rebind it to the name. The double bodies are reached
from MAGIC017..019 (S05) through `BattleTask_Create(1, 0x1F)`, which needs
nothing from them.

`symbols.toml`'s evidence for `Gfx_UploadQueueCount`, `Gfx_UploadQueueX` and
`Gfx_UploadQueueRecord` names `0x4A29C0` as the function that cancels a queue
entry; it is `0x4A2E50` and `0x4A3020` (`Magic008Blow_ReactTwo` / `_ReactThree`)
- `0x4A29C0` is the screen flash. Left for the coordinator (other entries).

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `Magic008Double_Bodies` | `0x65A6A0` | 6 |
| `Magic008Dash_Offsets` | `0x65A6B8` | 3 pairs |
| `Magic020Child_Bodies` | `0x65A6D0` | 3 |
| `Magic020Slash_Animations` | `0x65A6DC` | 12 bytes |

Each count is where the next table starts (read 2026-09-26: MAGIC021's table
begins at `0x65A6E8`).

## 5. The fuzz

`BOF3X_SHADOW=magic_s06` runs `magic_harness::Run` over the 56 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s06_fuzz.cpp`:

- **Callees** (18 listed; the standard set supplies the rest):
  - `Gfx_CommitPrim` has an `effect` that logs the primitive's bytes
    (`NoteBytes`, the size the call names) and moves `Gfx_PacketNext` on
    through a buffer of the fuzz's own - both quads of the flash are built at
    one pointer, so without the log only the last would be compared;
  - the sprite calls that act on `Sprite_Current` (`Sprite_ScriptTickOnce`,
    `Sprite_ScriptTick`, `Sprite_SetAnimation`, `Sprite_UpdateScreen`,
    `BattleActor_UpdateScreenXY`) log which sprite; `Sprite_QueueOverlay`
    logs `0x9039D8` too (the slashes' table switch);
  - `Battle_ActorIsOut` answers from its own stream and a quarter of the time
    moves the target (group E's work-round for the harness's `kFlag` blind
    spot): the reactions read the target again after a "not out";
  - `MagicFx_NearSprite` answers `kBool` (its callers test the whole eax;
    the standard `kFlag` answer is almost never 0 in eax);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - this group's `0x4A2440`, `0x4A29C0` and `0x4A3A70` by address (their
    callers call them directly).
- **Tables:** `Magic008Double_Bodies`, `Magic020Child_Bodies`.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; CLUT strip words 0x1A00..0x1A1F and their source;
  `Field_Kind2Z` / `_X`; `0x9039D8`; the ability word `0x904B80`; the upload
  queue (`Gfx_UploadQueueCount`, the x / y words and the record pointers at
  any count 0..255, since the reactions index them by the count less one);
  17,179 bytes in 18 regions.
- **Seed:** each dispatcher inside its table; each count-down one step before
  and at 0; the ability 7, 0xA4, 0xB (and one either side) half the time for
  the functions that test it; `+0xB` 0 / 0xFF / 7 and the owner's +0xB 0 for
  the waits; the colour one step either side of 0x80, 0x80 and 0 for the
  fades; +2 at 4..6 / 7..9 for the waits' last-round tests; every actor's
  state 6 half the time and `SetRandHint` 0 or 8 for the reactions; the dash
  and slash indices inside their tables; the upload count 1..20 two times in
  three.
- **Disturb** (the group's case, from its hash only): `Gfx_PacketNext`,
  `Field_Kind2*`, `0x9039D8`, the ability word, the upload count, the word
  `0x903852`. **Settle:** `Magic020Slash_Start` reads +0xB again after a call
  as its table index; a disturbed +0xB is put back inside the twelve.

Result in this worktree (2026-09-26):

    shadow      magic_s06 self-test: 112000 rounds over 56 functions (2000 each), 173586 calls to the stand-ins,
                0 MISMATCHES; 17179 bytes of state (18 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`; `Rand` 388 times, all from `_ReactThree`).
`BOF3X_SHADOW='*'`: exit 0 (173,804 stand-in calls for this group in that
run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

203 plants, each put in `magic_s06.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s06`, restored; after the last it restored, rebuilt and ran the clean self-test (exit 0, 0 mismatches). **202 of 203 refused**, every one by exit 3 with a count only in the functions the plant touches (A- controls MAGIC008, B- MAGIC020). One is an equivalent mutant: A104 reads `Sprite_Current` once where the original reads it twice with no call between - no input tells them apart; its near variant A104b (the task read before the tick) is refused in 60 rounds.

The thinnest (fewer than 60 rounds): B59 (Slash_Start flipped at 1 or 3) 2, A93 (Dash_Strike: task not read again) 4, A17 (Apply waits while +0xB above 1) 5, A25 (Apply: task read before the popup) 34, A122 (ReactThree: one in four) 49, A15 (party below 4) 50 in `_ReactTwo` and more elsewhere.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| A1 | Task: entries 0/1 swapped | Magic008_Task 1326 |
| A2 | Start: facing from the owner +9 | Magic008_Start 1888 |
| A3 | Start: +4 stat + 1 | Magic008_Start 1936 |
| A4 | Start: +0xB 1 | Magic008_Start 1921 |
| A5 | Start: animation 0xD | Magic008_Start 2000 |
| A6 | Start: parameter 0x20 | Magic008_Start 2000 |
| A7 | Start: body 3 | Magic008_Start 2000 |
| A8 | Start: owner bits 0x60 | Magic008_Start 1026 |
| A9 | Start: 31 CLUT words | Magic008_Start 2000 |
| A10 | Start: dirty 2 | Magic008_Start 2000 |
| A11 | Double: 0x7C bytes copied | Magic008_Start 2000, Magic020_Start 2000 |
| A12 | Double: actor read before the task | Magic008_Start 67, Magic020_Start 290 |
| A13 | Double: +6 2 | Magic008_Start 2000, Magic020_Start 2000 |
| A14 | Double: +2 1 | Magic008_Start 2000, Magic020_Start 2000 |
| A15 | ActorRecord: party below 4 | Magic008_Start 404, Magic008Blow_ReactTwo 50, Magic008Blow_ReactThree 66, Magic020_Start 497 |
| A16 | Double: +0x80 the owner | Magic008_Start 1766, Magic020_Start 1790 |
| A17 | Apply: waits while +0xB above 1 | Magic008_Apply 5 |
| A18 | Apply: stat index & 7 | Magic008_Apply 333 |
| A19 | Apply: position from the owner | Magic008_Apply 739 |
| A20 | Apply: popup parameter 0x49 | Magic008_Apply 1003 |
| A21 | Apply: popup +4 9 | Magic008_Apply 310 |
| A22 | Apply: took inverted | Magic008_Apply 999 |
| A23 | Apply: popup +9 0x16 | Magic008_Apply 1003 |
| A24 | Apply: popup +0xA 9 | Magic008_Apply 1003 |
| A25 | Apply: task read before the popup | Magic008_Apply 34 |
| A26 | Apply: animation (4, 1) | Magic008_Apply 1329 |
| A27 | Apply: owner mask 0x9F | Magic008_Apply 622 |
| A28 | BuffStat: ability 6 | Magic008_BuffStat 926 |
| A29 | BuffStat: none 0xFE | Magic008_BuffStat 1192 |
| A30 | Double_Task: bodies 2/3 swapped | Magic008Double_Task 630 |
| A31 | Grow_Run: steps 2/3 swapped | Magic008Grow_Run 633 |
| A32 | Grow_Run: update at +0 not 1 | Magic008Grow_Run 956 |
| A33 | Grow_Start: +0x44 0x10001 | Magic008Grow_Start 2000 |
| A34 | Grow_Start: +9 0x1F | Magic008Grow_Start 2000 |
| A35 | Grow_Grow: y by 0x801 | Magic008Grow_Grow 2000 |
| A36 | Grow_Grow: size to +0xA | Magic008Grow_Grow 526 |
| A37 | Grow_Strike: flags the actor | Magic008Grow_Strike 425 |
| A38 | Grow_Strike: sound (2, 5) | Magic008Grow_Strike 474 |
| A39 | Grow_Wait: sound 0x102 | Magic008Grow_Wait 1347 |
| A40 | Grow_Wait: +9 0xE | Magic008Grow_Wait 1347 |
| A41 | Grow_Shrink: x by -0x7FF | Magic008Grow_Shrink 2000 |
| A42 | Grow_Shrink: owner +0xB 0xFE | Magic008Grow_Shrink 509 |
| A43 | CountDown: at 1 | Magic008Grow_Grow 1017, Magic008Grow_Strike 1005, Magic008Grow_Shrink 1008, Magic008Glow_Brighten 986, Magic008Glow_Strike 976, Magic008Glow_Fade 1030, Magic008Mirror_Strike 683, Magic008Dash_Follow 508, Magic008Dash_Strike 513, Magic008Blow_Strike 921, Magic020_Hold 646, Magic020Lead_Tick 993, Magic020Lead_Flash 973, Magic020Shadow_Start 1030, Magic020Slash_Start 1011 |
| A44 | Glow_Run: steps 2/3 swapped | Magic008Glow_Run 789 |
| A45 | Glow_Start: kind-2 x / z swapped | Magic008Glow_Start 2000 |
| A46 | Glow_Start: height from owner +0x38 | Magic008Glow_Start 2000 |
| A47 | Glow_Start: +0x44 0x28001 | Magic008Glow_Start 2000 |
| A48 | Glow_Start: blue 0x81 | Magic008Glow_Start 2000 |
| A49 | Glow_Start: +0x2B 2 | Magic008Glow_Start 2000 |
| A50 | Glow_Start: sound 0x102 | Magic008Glow_Start 2000 |
| A51 | Glow_Start: +0x27 from owner +0x28 | Magic008Glow_Start 1993 |
| A52 | Glow_Brighten: by 4 | Magic008Glow_Brighten 2000 |
| A53 | AddColour: blue by one more | Magic008Glow_Brighten 2000, Magic008Glow_Fade 2000, Magic008Dash_Fade 2000, Magic020Lead_Return 2000, Magic020Lead_Brighten 2000 |
| A54 | Glow_Strike: +9 0x21 | Magic008Glow_Strike 510 |
| A55 | Glow_Wait: +9 0x1F | Magic008Glow_Wait 1321 |
| A56 | Glow_Fade: not freed | Magic008Glow_Fade 493 |
| A57 | Glow_Fade: owner +0xB 0xFE | Magic008Glow_Fade 491 |
| A58 | Mirror_Run: steps 2/3 swapped | Magic008Mirror_Run 1014 |
| A59 | Mirror_Size: no screen update | Magic008Mirror_Size 2000 |
| A60 | Mirror_Strike: no tick at 0 | Magic008Mirror_Strike 492 |
| A61 | Mirror_Strike: +9 1 at the end | Magic008Mirror_Strike 979 |
| A62 | Mirror_Strike: sound (2, 3) at the end | Magic008Mirror_Strike 990 |
| A63 | Mirror_Hit: owner +0xB up | Magic008Mirror_Hit 1329 |
| A64 | Mirror_Hit: update only at the end | Magic008Mirror_Hit 663 |
| A65 | Mirror_Flash: freed at 0xD | Magic008Mirror_Flash 415 |
| A66 | Mirror_Flash: skip for 0xA5 | Magic008Mirror_Flash 410 |
| A67 | Mirror_Flash: +9 by 5 | Magic008Mirror_Flash 940 |
| A68 | DrawFlash: shade << 3 | Magic008_DrawFlash 1956 |
| A69 | DrawFlash: tpage 0x36 | Magic008_DrawFlash 2000 |
| A70 | DrawFlash: closing layer 2 | Magic008_DrawFlash 2000 |
| A71 | DrawFlash: first commit 0x40 | Magic008_DrawFlash 2000 |
| A72 | DrawFlash: second quad at the first pointer | Magic008_DrawFlash 2000 |
| A73 | DrawFlash: first quad bottom y 121 | Magic008_DrawFlash 1993 |
| A74 | DrawFlash: second quad bottom right y | Magic008_DrawFlash 2000 |
| A75 | DrawFlash: top-left 0x81 | Magic008_DrawFlash 1997 |
| A76 | DrawFlash: second quad bottom right shade | Magic008_DrawFlash 1870 |
| A77 | DrawFlash: first quad opaque | Magic008_DrawFlash 2000 |
| A78 | DrawFlash: first quad right x 320 | Magic008_DrawFlash 1996 |
| A79 | DrawMode: dtd 0 | Magic008_DrawFlash 2000, Magic020_DrawFade 2000 |
| A80 | Dash_Run: steps 2/3 swapped | Magic008Dash_Run 663 |
| A81 | Dash_Run: update without the +2 test | Magic008Dash_Run 171 |
| A82 | Dash_Start: blue 0xB1 | Magic008Dash_Start 2000 |
| A83 | Dash_Start: facing from owner +9 | Magic008Dash_Start 1984 |
| A84 | Dash_Start: x offset from the z column | Magic008Dash_Start 2000 |
| A85 | Dash_Start: z offset from the next pair | Magic008Dash_Start 1331 |
| A86 | Dash_Start: the owner turned | Magic008Dash_Start 1757 |
| A87 | Dash_Start: +0x27 from owner +0x26 | Magic008Dash_Start 2000 |
| A88 | Dash_Follow: frame bit 1 | Magic008Dash_Follow 997 |
| A89 | Dash_Follow: +9 = +0xB + 5 | Magic008Dash_Follow 264 |
| A90 | BesideOwner: z by the x offset | Magic008Dash_Follow 999, Magic008Dash_Strike 976 |
| A91 | OnOwner: z from owner +0x3C | Magic008Dash_Follow 1001, Magic008Dash_Strike 1024 |
| A92 | Dash_Strike: first double at +0xB 1 | Magic008Dash_Strike 139 |
| A93 | Dash_Strike: task not read again | Magic008Dash_Strike 4 |
| A94 | Dash_Strike: frame test inverted | Magic008Dash_Strike 2000 |
| A95 | Dash_Fade: at 0x81 | Magic008Dash_Fade 670 |
| A96 | Dash_Fade: own +0xB down | Magic008Dash_Fade 276 |
| A97 | TwoBlows_Run: steps 1/2 swapped | Magic008TwoBlows_Run 581 |
| A98 | TwoBlows_Run: step 5 a reaction | Magic008TwoBlows_Run 306 |
| A99 | ThreeBlows_Run: step 8 a reaction | Magic008ThreeBlows_Run 203 |
| A100 | ThreeBlows_Run: update by +2 | Magic008ThreeBlows_Run 1014 |
| A101 | Blow_Strike: sound (2, 1) | Magic008Blow_Strike 470 |
| A102 | Blow_Strike: flag before sound | Magic008Blow_Strike 470 |
| A103 | WaitTwo: at step 4 | Magic008Blow_WaitTwo 403 |
| A104 | WaitTwo: task not read again | equivalent: no call between the two reads of `Sprite_Current` (the owner write cannot move it); its near variant A104b is refused |
| A104b | WaitTwo: task read before the tick | Magic008Blow_WaitTwo 60 |
| A105 | WaitThree: at step 7 | Magic008Blow_WaitThree 405 |
| A106 | WaitThree: owner +0xB 0xFE | Magic008Blow_WaitThree 201 |
| A107 | React: state 7 | Magic008Blow_ReactTwo 665, Magic008Blow_ReactThree 704 |
| A108 | ReactTwo: state read before the out test | Magic008Blow_ReactTwo 87 |
| A109 | React: queue count not down | Magic008Blow_ReactTwo 418, Magic008Blow_ReactThree 402 |
| A110 | React: next record zeroed | Magic008Blow_ReactTwo 418, Magic008Blow_ReactThree 402 |
| A111 | React: y 1 | Magic008Blow_ReactTwo 418, Magic008Blow_ReactThree 402 |
| A112 | React: animation +8 + 0xD | Magic008Blow_ReactTwo 418, Magic008Blow_ReactThree 402 |
| A113 | React: member below 2 | Magic008Blow_ReactTwo 124, Magic008Blow_ReactThree 129 |
| A114 | React: +0x4B 0xFE | Magic008Blow_ReactTwo 270, Magic008Blow_ReactThree 259 |
| A115 | React: enemy animation 3 | Magic008Blow_ReactTwo 270, Magic008Blow_ReactThree 259 |
| A116 | React: size to +0xA | Magic008Blow_ReactTwo 688, Magic008Blow_ReactThree 661 |
| A117 | ReactTwo: out to step 7 | Magic008Blow_ReactTwo 648 |
| A118 | ReactTwo: out owner +0xB 0xFE | Magic008Blow_ReactTwo 648 |
| A119 | ReactThree: out to step 8 | Magic008Blow_ReactThree 636 |
| A120 | ReactThree: cut skipped at 8 | Magic008Blow_ReactThree 350 |
| A121 | ReactThree: ability 0xC | Magic008Blow_ReactThree 437 |
| A122 | ReactThree: one in four | Magic008Blow_ReactThree 49 |
| A123 | ReactThree: cut to step 10 | Magic008Blow_ReactThree 95 |
| A124 | ReactThree: no cut while reacting | Magic008Blow_ReactThree 132 |
| B1 | Task: entries 0/1 swapped | Magic020_Task 665 |
| B2 | Task: entries 4/5 swapped | Magic020_Task 674 |
| B3 | Start: shadow delay + 2 | Magic020_Start 2000 |
| B4 | Start: order i xor 2 | Magic020_Start 2000 |
| B5 | Start: three shadows | Magic020_Start 2000 |
| B6 | Start: lead body 2 | Magic020_Start 1999 |
| B7 | Start: shadows parameter 0x2F | Magic020_Start 2000 |
| B8 | Start: CLUT row + 1 | Magic020_Start 2000 |
| B9 | Start: +0x24 from owner +0x25 | Magic020_Start 1993 |
| B10 | Start: STP on the owner | Magic020_Start 1733 |
| B11 | Start: sound for actor 3 | Magic020_Start 442 |
| B12 | Start: +9 1 | Magic020_Start 1717 |
| B13 | Start: CLUT from the task | Magic020_Start 1735 |
| B14 | Start: owner bits 0x41 | Magic020_Start 1054 |
| B15 | Darken: waits for 0xFE | Magic020_Darken 1320 |
| B16 | Darken: at 0x14 | Magic020_Darken 706 |
| B17 | Darken: seven slashes | Magic020_Darken 553 |
| B18 | Darken: delays 9 apart | Magic020_Darken 553 |
| B19 | Darken: slash body 1 | Magic020_Darken 553 |
| B20 | Darken: order + 1 | Magic020_Darken 553 |
| B21 | Darken: task read before the slash | Magic020_Darken 120 |
| B22 | Darken: +0xB 1 | Magic020_Darken 466 |
| B23 | Hold: +2 on | Magic020_Hold 332 |
| B24 | Hold: no fade drawn | Magic020_Hold 2000 |
| B25 | End: owner mask 0xBE | Magic020_End 620 |
| B26 | End: animation 5 | Magic020_End 1312 |
| B27 | Child_Task: bodies 0/1 swapped | Magic020Child_Task 1343 |
| B28 | Lead_Run: steps 4/5 swapped | Magic020Lead_Run 491 |
| B29 | Lead_Run: update at +2 not 1 | Magic020Lead_Run 287 |
| B30 | Lead_Start: +9 0x11 | Magic020Lead_Start 2000 |
| B31 | Lead_Start: +0x2B 0 | Magic020Lead_Start 2000 |
| B32 | Lead_Approach: step 0x61 | Magic020Lead_Approach 2000 |
| B33 | Lead_Approach: near 0x20001 | Magic020Lead_Approach 2000 |
| B34 | Lead_Approach: owner +0xB 0xFE | Magic020Lead_Approach 1329 |
| B35 | Lead_Tick: no tick | Magic020Lead_Tick 2000 |
| B36 | WaitSlashes: at owner +0xB 1 | Magic020Lead_WaitSlashes 1383 |
| B37 | WaitSlashes: +9 0xF | Magic020Lead_WaitSlashes 932 |
| B38 | Lead_Flash: blue 0xC1 | Magic020Lead_Flash 476 |
| B39 | Lead_Flash: bit 0x10 | Magic020Lead_Flash 412 |
| B40 | Lead_Return: by -9 | Magic020Lead_Return 2000 |
| B41 | Lead_Return: back at the source | Magic020Lead_Return 229 |
| B42 | Lead_Brighten: by 0x11 | Magic020Lead_Brighten 2000 |
| B43 | Lead_Brighten: at 0x10 | Magic020Lead_Brighten 379 |
| B44 | Shadow_Run: steps 0/1 swapped | Magic020Shadow_Run 1329 |
| B45 | Shadow_Start: x 0x15 | Magic020Shadow_Start 514 |
| B46 | Shadow_Start: two channels | Magic020Shadow_Start 512 |
| B47 | Shadow_Start: +0x2B 1 | Magic020Shadow_Start 514 |
| B48 | Shadow_Start: +0x5C 2 | Magic020Shadow_Start 514 |
| B49 | Shadow_Approach: step 0x40 | Magic020Shadow_Approach 2000 |
| B50 | Shadow_Approach: near the owner | Magic020Shadow_Approach 1473 |
| B51 | Slash_Run: frame table + 4 | Magic020Slash_Run 500 |
| B52 | Slash_Run: table not put back | Magic020Slash_Run 2000 |
| B53 | Slash_Run: queued without the +2 test | Magic020Slash_Run 498 |
| B54 | Slash_Run: steps swapped | Magic020Slash_Run 2000 |
| B55 | Slash_Start: bank 0x1E | Magic020Slash_Start 475 |
| B56 | Slash_Start: +0x24 0x85 | Magic020Slash_Start 475 |
| B57 | Slash_Start: +0x2C 1 | Magic020Slash_Start 471 |
| B58 | Slash_Start: - 0x4F | Magic020Slash_Start 475 |
| B59 | Slash_Start: flipped at 1 or 3 | Magic020Slash_Start 2 |
| B60 | Slash_Start: animation + 1 | Magic020Slash_Start 475 |
| B61 | Slash_Start: facing from source +9 | Magic020Slash_Start 466 |
| B62 | Slash_Start: no screen point | Magic020Slash_Start 475 |
| B63 | Slash_Start: +0x29 1 | Magic020Slash_Start 475 |
| B64 | Slash_Start: +0x26 1 | Magic020Slash_Start 475 |
| B65 | Slash_Start: +0x28 1 | Magic020Slash_Start 475 |
| B66 | Slash_Play: last at 6 | Magic020Slash_Play 430 |
| B67 | Slash_Play: own +0xB down | Magic020Slash_Play 1168 |
| B68 | Slash_Play: tick once | Magic020Slash_Play 2000 |
| B69 | DrawFade: grey x 16 | Magic020_DrawFade 1986 |
| B70 | DrawFade: g from the high byte | Magic020_DrawFade 1986 |
| B71 | DrawFade: word + 0x100 | Magic020_DrawFade 1961 |
| B72 | DrawFade: commit 0x18 | Magic020_DrawFade 2000 |
| B73 | DrawFade: layer 1 | Magic020_DrawFade 2000 |
| B74 | DrawFade: width 319 | Magic020_DrawFade 2000 |
| B75 | DrawFade: height + 1 ulp | Magic020_DrawFade 2000 |
| B76 | DrawFade: semi-trans 2 | Magic020_DrawFade 2000 |
| B77 | DrawFade: closing layer 1 | Magic020_DrawFade 2000 |
| B78 | DrawFade: y at 1 | Magic020_DrawFade 2000 |

**Re-run 2026-09-26 on the kFlag-fixed harness ([`magic_harness.md`](magic_harness.md) §8): 74 controls in the affected functions, 74 refused** (plus one new, A120b, refused). The 22 section-8 functions are Apply, the Grow / Glow / Mirror steps, Dash_Start / _Follow, the blows' Strike / Wait / React, and MAGIC020's Lead_Approach / _Tick / _WaitSlashes, Shadow_Approach and Slash_Play. Selected: every plant in them or in a helper they call - A15 (ActorRecord, read by React), A17..A27, A35..A40, A43 (CountDown), A52..A55 (A53: AddColour), A59..A64, A82..A91 (A90 / A91: BesideOwner / OnOwner), A101..A103, A104b, A105..A124, B32..B37, B49, B50, B66..B68. Skipped: the other 128, whose plants lie only in functions (or helpers such as MakeDouble and DrawMode) no section-8 function reaches, and A104, the equivalent mutant (its reason is independent of the stream). The plants were rebuilt from the table by a script (not committed; each anchored inside its function's extent) that planted, rebuilt, checked `magic_s06.cpp` recompiled, ran the self-test, and restored.

The first pass refused 73 of 74. **A120** (ReactThree: cut skipped at 8, rebuilt as `+2 == 9 || +2 == 8`) was not refused: the seed put ReactThree's +2 at 9 or at a random byte, so the cut test almost never met +2 8 (a random 7 then React, or a random 8 with the target reacting). **Fuzz change** (`magic_s06_fuzz.cpp`, ReactThree's seed): when +2 is not seeded 9, half the time it is 7..9, so after React the test meets 8..10. A120 is then refused in 39 rounds, and a near variant A120b (`+2 >= 8`) in 198. After the change all of the selected controls were run again, and all 75 were refused. The thinnest in that pass (fewer than 60 rounds) were A17 2, A25 30, A120 39, A122 39, A104b 47. Clean self-tests after it: `magic_s06` exit 0, 0 mismatches (173,373 stand-in calls), and `BOF3X_SHADOW='*'` exit 0. Between the two passes, round counts also moved by a few in functions whose seed was not touched (A17 5 → 2 in Apply). This was not isolated. It fits section 5's note that the harness's pointers into the DLL move a few branches, since the rebuilt fuzz file moves them.

## 7. What nothing reached

No recorded route casts any of these (queue §5); the live check is the owner
casting them, with a save that has them or DIV-0045's cheat. Things to look
for, by reading:

- row 42: a copy of the caster that strikes and then flashes the screen
  (not for ability 0xA4), then for ability 7 alone a buff and its popup on
  the target;
- row 8: four darkening copies and a lead running to the target, the screen
  darkening, eight slashes one after another at the target, the lead
  flashing, returning and brightening away.

The double bodies 0, 1, 3, 4 and 5 are MAGIC017..019's (S05's) to cast; their
look belongs with that group's rows.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: nine stack tables and two
  `.data` tables, and the two `.data` data tables read by +0xB. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Magic008_Start`, `Magic008_Apply`, `Magic020_Start` (five calls) and
  `Magic020_Darken` (eight): slot 255 is `0x93A000 + 255 x 0x84 = 0x9423FC`,
  past the image's end - an access violation, in ours as in the original (the
  same address is written; the doubles' copy writes 0x80 bytes there first).
- **The reactions cancel an upload they did not check was queued**:
  `Magic008Blow_ReactTwo` / `_ReactThree` take `Gfx_UploadQueueCount` down by
  one after `Sprite_SetAnimation` and zero that entry, assuming the call
  queued one. If it did not (`symbols.toml` has the enqueuer `0x5894D0` skip a
  record with +0 bit 1; whether `Sprite_SetAnimation` always reaches it was not
  read), an earlier entry is dropped; at a count of 0 the count wraps to
  255 and the three arrays (20 entries each) are written 235 entries past
  their end. Not measured whether a member's double can reach it.
- **The reactions wait without a limit** while the target's state is 6 (the
  shape of Head Cracker's wait, [`magic_engine.md`](magic_engine.md)).
- **The actor records are indexed by the actor byte - 3, unchecked**
  (`Magic008_Start`, `Magic020_Start`, the reactions' state reads).

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 56 lines under a `round 9 group S06` comment,
each function's own extent. Three host lines ran over them
(`004A2440 57A`, `004A29C0 10A1`, `004A3A70 392`); the smaller extents are
listed for the consolidation to keep.
