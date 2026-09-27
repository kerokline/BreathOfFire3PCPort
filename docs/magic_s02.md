# Group S02: Super Combo, and the Strikes and Claws (MAGIC003, MAGIC004 and the nine files folded into it)

**Status:** IN PROGRESS (2026-09-26). All 48 functions are ours
(`src/game/magic_s02.cpp`, shadow name `magic_s02`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 96,000 rounds. 230 of 231 negative controls refused (229 by a count, one by a fault); the
one left is an equivalent mutant, whose near variant was refused. Nothing recorded casts
these abilities, so this is fuzz only until the owner sees them used.

Round nine, third spell wave, group S02
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 3 | 0x22C | MAGIC003 | 0x3 | Super Combo | `0x49A7B0..0x49BAE6` | 35 |
| 88 | 0x22D | MAGIC004 | 0x4 | ThundrStrike | `0x49BAF0..0x49C3C5` | 13 |
| 98 | 0x22E | MAGIC005 | 0x5, 0x9B | Flame Strike, Pyrokinesis | (MAGIC004's) | |
| 100 | 0x23C | MAGIC029 | 0x1D | Wind Strike | (MAGIC004's) | |
| 99 | 0x248 | MAGIC049 | 0x31 | Frost Strike | (MAGIC004's) | |
| 129..132 | 0x293..0x296 | MAGIC133..136 | 0x85..0x88 | Flame, Frost, Thunder, Shining Claw | (MAGIC004's) | |
| 92 | 0x2A0 | MAGIC156 | 0x9C | Holy Strike | (MAGIC004's) | |
| 93 | 0x2A1 | MAGIC157 | 0x9D | Demonbane | (MAGIC004's) | |

The extents are `tools/magic_rows.py --unit MAGIC003` and `--unit
MAGIC004/MAGIC005/.../MAGIC157 --clones` (capstone recursive descent; one jump
table, in `ElemStrike_Kind`, moved into the copy by the harness; nothing
`REFUSED`). All 48 functions lie in the two extents; none was found inside or
missing from them, and none was ours before. 6,774 bytes, as the queue counted.
The ten rows of MAGIC004's group all point at `0x49BAF0`: the linker folded
nine identical overlays into MAGIC004's copy (queue §1), so one set of
functions serves twelve ability ids; `ElemStrike_Kind` tells them apart.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. "Super Combo" and
"ElemStrike" are this group's names for what the code does; which ability a
player meets as which has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **MAGIC003 (row 3): a timed button prompt, then a flurry.**
  - `SuperCombo_Task`, the kind-2 task: a call through `SuperCombo_Phases`
    (ten entries, `.data` `0x65A524`) by +1; then it runs the hit sprites
    itself - every entry of a private pool of 32 task-like entries of 0x84
    bytes at `0x6769C0` (`SuperComboHit_Pool`, `.bss`) whose +0 has bit 0,
    with Sprite_Current the entry and the owner its +0x80, both put back.
  - The phases: `_Start` clears the pool and sets the prompt's screen point
    (0xA0, 0x70); `_Prompt1` / `_Prompt2` draw box and text 1 and 2 for 30
    frames each; `_PickButton` waits +9 frames (10 the first time, 6 after a
    press), then asks for a button (Rand & 3) and allows a time
    taken from a table by the count so far (`0x65A504`, 16 entries, 3 past
    them); `_ReadButton` draws box 4 and the button, then reads
    Input_Pressed: a press of any of bits 0xF0 that equals the button's bit
    (`0x65A518`) **as a whole word** counts a hit (+0xA) and asks again while
    the count is below 0x20, any other press ends the prompt, and so does
    the time running out; `_Pause`; `_ShowCount` draws box 0, the count in
    decimal and text 0; `_Strike` animates the caster (0xC), makes the dash
    and four after-images; `_WaitChildren` waits for them; `_End` writes the
    count to the battle byte `0x904B96`, flags the target 0x40 and ends the
    effect.
  - The children (kind 1, parameter 1, `SuperComboChild_Task` by +1): each a
    copy of the acting actor's record (0x80 bytes). The dash
    (`SuperComboDash_*`) waits the caster's effect size, leaps (a rise of
    0x300000 falling by 0x80000 a frame) and, when its count-down reaches 0,
    plays two sounds, makes the hit sprites (`SuperCombo_SpawnHits`: one
    pool entry per hit counted) and flags the target 0x10; then it turns and
    returns along the ground and ends once the others have. The four
    after-images (`SuperComboImage_*`) leap the same way, delayed 5, 9, 13
    and 17 frames, semi-transparent and tinted by their index (`0x65A4F8`).
  - The hit sprites (`SuperComboHit_*`): at the source sprite, delayed 1 + 4 i
    frames, an animation by (i >> 1) & 7 under the frame-offset table
    `0x8C5D80`, ended through MAGIC219's `0x4F6290`.
  - The draws: `SuperCombo_DrawBox` (two semi-transparent tiles round a
    rectangle of `0x65A55C`), `_DrawText` (a 0-ended string of the pointer
    table `0x66A0D8` as 12 x 12 glyph quads, 0xFF a half-width space, centred
    by a length of `0x65A51C`), `_DrawButton` and `_DrawCount` (12 x 12 glyph
    quads). All on layer 2, screen space, clut row 0x1E0.
- **MAGIC004 (rows 88, 92, 93, 98..100, 129..132): a strike with an element.**
  - `ElemStrike_Task`, kind 2: five phases by +1.
  - `ElemStrike_Kind` maps the ability word `0x904B80` to a kind through a
    byte table in `.text` (`0x49C0CC`, ids 4..0x9C) and a twelve-case jump
    table: +4 the kind (0..0xB), +3 a variant bit. By that table: id 4 kind
    0, id 5 kind 1, id 7 kind 2, id 0x1D kind 3, id 0x31 kind 4, ids
    0x85..0x88 kinds 8..0xB, id 0x9B kind 5, id 0x9C kind 6; **every other id,
    Demonbane's 0x9D among them, takes the default, kind 7.** Id 7 has a
    kind of its own but is in no row of this group (read 2026-09-26; whether
    another file loads MAGIC004 for id 7 was not traced).
  - `_Start` animates the caster (0xC), takes the kind, and makes two
    children of kind 1, parameter 0x46 (`ElemStrikeChild_Task` by +1): a copy
    of the acting actor's record that plays (`ElemStrikeCopy_*`: MAGIC154's
    `BattleFx_SetSize`, a script until its count-down, MAGIC161's
    `0x4EE560`, `BattleFx_FreeTask`), and an effect sprite
    (`ElemStrikeFx_*`, under the effects' frame-offset table `0x8E3580`:
    MAGIC167's `0x4EF840`, then at the source sprite 0x800000 higher an
    animation, a sound 0x101 after the kind's delay (none for a delay of 0),
    freed at its script's end, counting the owner's +0xB down). It also restores CLUT row 26 from its source (no STP bits).
  - `_Tint` (once +0xB, the children alive, is at most 1) tints the target's record by the kind's
    three bytes (`0x65A584`), animates the caster (4), flags the target
    0x20 and plays 0x100 (and 0x101 for ids 0x85..0x88); `_Hit` (once it
    is 0) plays the kind's hit sound (`0x65A5B4`, 0 none) and flags the
    target 0x40; `_Fade` calls MAGIC008's `0x4A29C0` three times and steps
    the three tint channels down by 2 into the tint record until all are 0,
    then flashes the target; `_End` waits while the target is not out and
    in state 6, then ends the effect.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one exception
that follows the project's precedent: a phase past any of the ten dispatch
tables (six stack tables, four `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3). `ElemStrike_Kind`'s cases
cannot run past its twelve (its byte table's largest entry is 0xB, and an id
past the table takes the last case), but ours aborts there too. Tables of
data (tints, times, sounds, boxes, texts) are read in place and unchecked, as
the originals read them.

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x4A29C0` | MAGIC008 (S06, this wave) | called by `ElemStrike_Fade` while +9 is below 0xC |
| `0x4F6290` | MAGIC219 (S37) | `SuperComboHit_Play`'s tail jmp at its script's end |
| `0x4EE560` | MAGIC161 (S34) | entry 2 of `ElemStrikeCopy_Run`'s stack table |
| `0x4EF840` | MAGIC167 (S35) | entry 0 of `ElemStrikeFx_Steps` |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (as S22, S31 call it) |

By name, already ours: `BattleFx_SetSize` (MAGIC154's, a phase of
`ElemStrikeCopy_Run`), `BattleFx_FreeTask`, the effect library's
`BattleActor_*`, and the sprite / sound / GPU library.

Shared bodies: `magic_funcs.tsv` has every function reached by its own
files only (MAGIC003; the ten files of MAGIC004).

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `SuperCombo_Phases` | `0x65A524` | 10 |
| `SuperComboHit_TaskTable` | `0x65A54C` | 1 |
| `ElemStrikeChild_Kinds` | `0x65A5CC` | 2 |
| `ElemStrikeFx_Steps` | `0x65A5D4` | 3 |
| `SuperComboHit_Pool` | `0x6769C0` | 32 x 0x84 bytes |

Each count is where the next table starts (the dump of `0x65A4F0..0x65A600`
read 2026-09-26); the tool's "11", "8" and "6 code entries" notes count every
code pointer that follows, across the next tables. `ElemStrikeFx_Steps` ends
where MAGIC006's table starts (`0x65A5E0`, `0x49C4E0` is MAGIC006's).

## 5. The fuzz

`BOF3X_SHADOW=magic_s02` runs `magic_harness::Run` over the 48 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s02_fuzz.cpp`:

- **Callees** (33 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` has an `effect` that logs the primitive's
    bytes (`NoteBytes`, the size the call names) and moves `Gfx_PacketNext`
    on through a 0x2000-byte buffer of the fuzz's own;
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - the sprite calls that act on Sprite_Current log which sprite, and
    `Sprite_QueueOverlay` / `Sprite_UpdateScreen` also log `0x9039D8` (the
    frame-offset table the hit sprites and the effect swap round them);
  - **the `kFlag` blind spot** (round9 doc §9): `Sprite_ScriptTickOnce`,
    `BattleActor_FxSize` and `Battle_ActorIsOut` answer from the stream after
    a second disturbance (`Stir`) of their own, so a read after a "no" meets
    moved cells; `Battle_ActorIsOut` also moves the target record's state
    byte +1 half the time, which `ElemStrike_End` reads after it through a
    pointer taken before;
  - this group's own functions called directly (`kPhase`, or the draws with
    their one u8 argument), and the four other units' by address;
  - the five phases run under a swapped frame-offset table
    (`SuperComboHit_Run`'s two, `ElemStrikeFx_Steps`' three) as `kPhase`
    callees that also log `0x9039D8` (the handlers' recorders do not);
  - `SuperComboHit_Alloc`'s stand-in answers 0..31 only (section 8).
- **Tables:** the four `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; the pool; `0x904B80..0x904B97` (the ability word, the hit count);
  Input_Pressed; `0x9039D8`; `MoveScript_TintRecords`; the prompt texts'
  pointer table `0x66A0D8` and 64 bytes of strings of the fuzz's own; CLUT
  row 26 and its source. 27,988 bytes of state in 19 regions.
- **Seed:** every pool entry's owner a task slot (the disturbance writes
  through the owner while `SuperCombo_Task` runs an entry); `0x904B3C` at one
  of the harness's sprite records; the ability word the group's ids half the
  time; each dispatcher inside its table; each count-down at 1 or 2; the
  prompt's count at 0xF / 0x10 / 0x13 / 0x14 (the time table's entries
  0x10..0x13 are the late time, 3) and 0x1F / 0x20; the text table the
  game's strings or, half the time, the fuzz's own (up to 12 bytes of 0xFF,
  glyphs and anything, or empty: the game's have no 0xFF and none is
  empty); Input_Pressed nothing, one
  bit, several, the wanted bit alone (half the time) or with bit 0x100; the
  dash's +9 0, 1, 2 or 0xFF; the landing and start points one step away
  (or one off it); the owner's +0xB 0..2 and +0xA 0..0x20; the pool full a
  quarter of the time for the allocator; the fade's channels 0..3; the
  target's state 6; the ability word one of the kind table's ids, around
  its ends, or anything.
  `Group::args` gives the draws a text 0..3, a box 0..4, any button and
  count (half below 10), with garbage above the byte half the time.
  `phase_span = 3`: `ElemStrikeFx_Run` reads its phase after a call.
- **Disturb** (the group's case): `Gfx_PacketNext`, Input_Pressed, the
  ability word, `0x9039D8`, a pool entry's byte, `0x904B3C`, a tint byte.

Result in this worktree (2026-09-26):

    shadow      magic_s02 self-test: 96000 rounds over 48 functions (2000 each), 313838 calls to the stand-ins,
                0 MISMATCHES; 27988 bytes of state (19 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (310,580
stand-in calls for this group in that run: the harness's pointers into the DLL
move a few branches, 0 mismatches).

## 6. Controls

231 plants, each put in `magic_s02.cpp` one at a time by a script (not committed) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s02`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). **230 of 231 refused**: 229 by exit 3 with a count only in the functions the plant touches, one (H36) by an access violation in ours. The one not refused is an equivalent mutant (P19), and its near variant (P19b) was refused. The P-, K-, H-, D- controls are MAGIC003's task and phases, its dash and after-images, its hit sprites and pool, its draws; the E- controls MAGIC004's.

The first run left three more standing, which the fuzz then learnt to see (their rows are the second run's, with every control of the functions concerned run again):

- **D2, D10**: the game's four prompt texts hold no 0xFF and none is empty, so the half-width space and the first-byte test were never met. The seed now aims the text table (`0x66A0D8`, a region, put back after) at strings of the fuzz's own half the time.
- **E56**: `ElemStrikeFx_Run`'s frame-offset table is overwritten after its phase, and the handlers' recorders did not log it. The five phases run under a swapped table are now listed as `kPhase` callees logging `0x9039D8`.

And `ElemStrike_Kind`'s seed now names the kind table's ids directly (E43 and E48 had been refused in 3 rounds). The thinnest now (fewer than 60 rounds): E47 16, E42 22, E52 24, E21 31, E48 36, E22 40, E43 45, E23 51, P28 52, P19b 57.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| P1 | Task: phases 3/4 swapped | SuperCombo_Task 395 |
| P2 | Task: pool entries run on bit 1 | SuperCombo_Task 2000 |
| P3 | Task: 31 pool entries | SuperCombo_Task 1019 |
| P4 | Task: owner not set for the entry | SuperCombo_Task 2000 |
| P5 | Task: owner not put back | SuperCombo_Task 1709 |
| P6 | Task: Sprite_Current saved before the phase | SuperCombo_Task 71 |
| P7 | Task: Sprite_Current not put back | SuperCombo_Task 1982 |
| P8 | Start: pool +2 kept | SuperCombo_Start 2000 |
| P9 | Start: x 0xA1 | SuperCombo_Start 2000 |
| P10 | Start: y 0x71 | SuperCombo_Start 2000 |
| P11 | Start: +9 0x1F | SuperCombo_Start 2000 |
| P12 | Start: count kept | SuperCombo_Start 1985 |
| P13 | Prompt1: box 2 | SuperCombo_Prompt1 2000 |
| P14 | Prompt1: text 0 | SuperCombo_Prompt1 2000 |
| P15 | Prompt1: +9 0x1D | SuperCombo_Prompt1 457 |
| P16 | Prompt2: +9 0xB | SuperCombo_Prompt2 457 |
| P17 | Prompt2: text 1 | SuperCombo_Prompt2 2000 |
| P18 | Pick: Rand & 7 | SuperCombo_PickButton 251 |
| P19 | Pick: times below 0x11 | not refused: **equivalent** (entry 0x10 of the time table is 3, the late time) |
| P19b | Pick: times below 0x15 (near variant of P19) | SuperCombo_PickButton 57 |
| P20 | Pick: late time 4 | SuperCombo_PickButton 435 |
| P21 | Pick: time table + 1 | SuperCombo_PickButton 85 |
| P22 | Read: box 3 | SuperCombo_ReadButton 2000 |
| P23 | Read: pad mask 0x70 | SuperCombo_ReadButton 265 |
| P24 | Read: low byte compared | SuperCombo_ReadButton 234 |
| P25 | Read: at most 0x21 | SuperCombo_ReadButton 111 |
| P26 | Read: wrong press goes to 3 | SuperCombo_ReadButton 1316 |
| P27 | Read: +9 7 on a press | SuperCombo_ReadButton 1783 |
| P28 | Read: time-out +9 5 | SuperCombo_ReadButton 52 |
| P29 | Read: the button from +0xA | SuperCombo_ReadButton 1994 |
| P30 | Pause: +9 0x1F | SuperCombo_Pause 501 |
| P31 | ShowCount: count from +9 | SuperCombo_ShowCount 1952 |
| P32 | ShowCount: text before box | SuperCombo_ShowCount 2000 |
| P33 | Strike: direction from the owner's +9 | SuperCombo_Strike 1947 |
| P34 | Strike: animation 0xD | SuperCombo_Strike 2000 |
| P35 | Strike: +4 before the spawns | SuperCombo_Strike 183 |
| P36 | Strike: owner bit 0x20 | SuperCombo_Strike 1512 |
| P37 | Strike: z not copied | SuperCombo_Strike 1763 |
| P38 | Wait: mask 0x7F | SuperCombo_WaitChildren 775 |
| P39 | Wait: animation 5 | SuperCombo_WaitChildren 1019 |
| P40 | End: sound 0x103 | SuperCombo_End 2000 |
| P41 | End: count read before the sound | SuperCombo_End 78 |
| P42 | End: hits to 0x904B97 | SuperCombo_End 2000 |
| P43 | End: done bit 8 | SuperCombo_End 1447 |
| K1 | Child: kinds swapped | SuperComboChild_Task 2000 |
| K2 | Dash_Run: steps 1/2 swapped | SuperComboDash_Run 819 |
| K3 | Dash_Run: update without +0 | SuperComboDash_Run 783 |
| K4 | LeapSetUp: step 0x2001 | SuperComboDash_Start 469, SuperComboImage_Start 511 |
| K5 | LeapSetUp: rise 0x300001 | SuperComboDash_Start 473, SuperComboImage_Start 512 |
| K6 | LeapSetUp: fall 0xFFF80001 | SuperComboDash_Start 473, SuperComboImage_Start 512 |
| K7 | LeapSetUp: landing from z | SuperComboDash_Start 473, SuperComboImage_Start 512 |
| K8 | LeapSetUp: +0x10 1 | SuperComboDash_Start 473, SuperComboImage_Start 512 |
| K9 | Dash_Start: +9 not the size | SuperComboDash_Start 473 |
| K10 | Dash_Start: size before the turn | SuperComboDash_Start 473 |
| K11 | CountDown: sound 0x100 | SuperComboDash_Leap 435, SuperComboDash_Hit 438 |
| K12 | CountDown: actor sound (3, 0) | SuperComboDash_Leap 435, SuperComboDash_Hit 438 |
| K13 | CountDown: flags 0x11 | SuperComboDash_Leap 435, SuperComboDash_Hit 438 |
| K14 | CountDown: no hits | SuperComboDash_Leap 435, SuperComboDash_Hit 438 |
| K15 | CountDown: 0xFF counts down too | SuperComboDash_Leap 443, SuperComboDash_Hit 452 |
| K16 | CountDown: +9 0xFE | SuperComboDash_Leap 429, SuperComboDash_Hit 432 |
| K17 | LeapMove: no fall | SuperComboDash_Leap 2000, SuperComboImage_Leap 2000 |
| K18 | LeapMove: fall before height | SuperComboDash_Leap 2000, SuperComboImage_Leap 2000 |
| K19 | Dash_Leap: landing against +0x40 | SuperComboDash_Leap 531 |
| K20 | Dash_Leap: tick before the move | SuperComboDash_Leap 236 |
| K21 | Dash_Hit: back step 0xFFFFD000 | SuperComboDash_Hit 832 |
| K22 | Dash_Hit: no turn | SuperComboDash_Hit 832 |
| K23 | Dash_Hit: on at 0xFE | SuperComboDash_Hit 832 |
| K24 | GroundMove: z by +0xC | SuperComboDash_Return 2000, SuperComboImage_Return 2000 |
| K25 | BackAtStart: x only | SuperComboDash_Return 375, SuperComboImage_Return 392 |
| K26 | Dash_End: at most 2 | SuperComboDash_End 342 |
| K27 | Dash_End: not counted down | SuperComboDash_End 977 |
| K28 | Image_Run: steps 2/3 swapped | SuperComboImage_Run 1002 |
| K29 | Image_Start: bit 0x10 | SuperComboImage_Start 435 |
| K30 | Image_Start: +0x5C 2 | SuperComboImage_Start 512 |
| K31 | Image_Start: shade x 0xE0 | SuperComboImage_Start 477 |
| K32 | Image_Start: +0x5E not set | SuperComboImage_Start 511 |
| K33 | Image_Start: tint stride 4 | SuperComboImage_Start 505 |
| K34 | Image_Start: tint channels rotated | SuperComboImage_Start 471 |
| K35 | Image_Start: tint a 0 | SuperComboImage_Start 512 |
| K36 | Image_Leap: no tick | SuperComboImage_Leap 2000 |
| K37 | Image_Turn: back step 0xFFFFE001 | SuperComboImage_Turn 1984 |
| K38 | Image_Turn: tick before the turn | SuperComboImage_Turn 2000 |
| K39 | Image_Return: owner +0xB not down | SuperComboImage_Return 144 |
| K40 | Image_Return: release the actor record | SuperComboImage_Return 144 |
| H1 | Hit_Run: frame table 0x8C5D84 | SuperComboHit_Run 2000 |
| H2 | Hit_Run: table not put back | SuperComboHit_Run 2000 |
| H3 | Hit_Run: queued with +2 0 | SuperComboHit_Run 500 |
| H4 | Hit_Run: steps swapped | SuperComboHit_Run 2000 |
| H5 | Hit_Start: direction from src +9 | SuperComboHit_Start 519 |
| H6 | Hit_Start: height from +0x38 | SuperComboHit_Start 530 |
| H7 | Hit_Start: +0x25 0x1E | SuperComboHit_Start 530 |
| H8 | Hit_Start: +0x24 0x80 | SuperComboHit_Start 530 |
| H9 | Hit_Start: +0x27 less 0x4F | SuperComboHit_Start 530 |
| H10 | Hit_Start: +0x2A bit 1 | SuperComboHit_Start 271 |
| H11 | Hit_Start: animation from +4 | SuperComboHit_Start 530 |
| H12 | Hit_Start: +0x29 1 | SuperComboHit_Start 530 |
| H13 | Hit_Play: the first tick's answer | SuperComboHit_Play 1104 |
| H14 | Hit_Play: owner +0xB kept | SuperComboHit_Play 1336 |
| H15 | Spawn: dash +1 1 | SuperCombo_SpawnDash 2000 |
| H16 | Spawn: dash +9 2 | SuperCombo_SpawnDash 2000 |
| H17 | Spawn: dash parameter 2 | SuperCombo_SpawnDash 2000 |
| H18 | CopyRecord: 0x7C bytes | SuperCombo_SpawnDash 2000, SuperCombo_SpawnImages 2000, ElemStrike_Start 2000 |
| H19 | ActorRecord: party at 0..3 | SuperCombo_SpawnDash 379, SuperCombo_SpawnImages 445, ElemStrike_Start 398 |
| H20 | Spawn: images +9 from 6 | SuperCombo_SpawnImages 2000 |
| H21 | Spawn: three images | SuperCombo_SpawnImages 2000 |
| H22 | Spawn: images +0xB from 1 | SuperCombo_SpawnImages 2000 |
| H23 | Spawn: images +2 1 | SuperCombo_SpawnImages 2000 |
| H24 | SpawnHits: owner +0xA not tested | SuperCombo_SpawnHits 218 |
| H25 | SpawnHits: +9 from 2 | SuperCombo_SpawnHits 1782 |
| H26 | SpawnHits: +0xB (i >> 1) & 3 | SuperCombo_SpawnHits 1029 |
| H27 | SpawnHits: +0xB i & 7 | SuperCombo_SpawnHits 1543 |
| H28 | SpawnHits: owner read once | SuperCombo_SpawnHits 700 |
| H29 | SpawnHits: +4 i + 1 | SuperCombo_SpawnHits 1782 |
| H30 | SpawnHits: +1 not cleared | SuperCombo_SpawnHits 1782 |
| H31 | SpawnHits: bound read once | SuperCombo_SpawnHits 690 |
| H32 | SpawnHits: step 3 | SuperCombo_SpawnHits 1543 |
| H33 | Alloc: bit 0 not set | SuperComboHit_Alloc 1487 |
| H34 | Alloc: none free 0xFE | SuperComboHit_Alloc 513 |
| H35 | Alloc: from entry 1 | SuperComboHit_Alloc 493 |
| H36 | Pool: stride 0x80 | an access violation in ours (exit 0xC0000005), no count |
| D1 | Text: centred by 5 a glyph | SuperCombo_DrawText 1873 |
| D2 | Text: space 5 | SuperCombo_DrawText 639 |
| D3 | Text: glyph base 0x40 | SuperCombo_DrawText 1873 |
| D4 | Text: 20 glyphs a row | SuperCombo_DrawText 1618 |
| D5 | Text: v from row 2 | SuperCombo_DrawText 1873 |
| D6 | Text: advance 11 | SuperCombo_DrawText 1789 |
| D7 | Text: y from +0x32 | SuperCombo_DrawText 1873 |
| D8 | Text: lengths table + 1 | SuperCombo_DrawText 1388 |
| D9 | Text: commit 0x44 | SuperCombo_DrawText 1873 |
| D10 | Text: first glyph not tested | SuperCombo_DrawText 80 |
| D12 | Quad12: size 13 | SuperCombo_DrawText 1873, SuperCombo_DrawButton 2000, SuperCombo_DrawCount 2000 |
| D13 | Quad12: y2 at y | SuperCombo_DrawText 1873, SuperCombo_DrawButton 2000, SuperCombo_DrawCount 2000 |
| D14 | GlyphPage: tpage y 0x3C1 | SuperCombo_DrawText 1873, SuperCombo_DrawCount 2000 |
| D15 | GlyphPage: clut y 0x1E1 | SuperCombo_DrawText 1873, SuperCombo_DrawCount 2000 |
| D16 | GlyphUv: shade 0x81 | SuperCombo_DrawText 1873, SuperCombo_DrawButton 2000, SuperCombo_DrawCount 2000 |
| D17 | GlyphUv: u2 from u1 | SuperCombo_DrawText 1873, SuperCombo_DrawButton 2000, SuperCombo_DrawCount 2000 |
| D18 | Button: u (b + 14) | SuperCombo_DrawButton 2000 |
| D19 | Button: v 0x31 | SuperCombo_DrawButton 2000 |
| D20 | Button: clut table + 1 | SuperCombo_DrawButton 1685 |
| D21 | Button: tpage 0x14 | SuperCombo_DrawButton 2000 |
| D22 | Count: x - 0x40 | SuperCombo_DrawCount 2000 |
| D23 | Count: tens 11 left | SuperCombo_DrawCount 980 |
| D24 | Count: 0 tens drawn | SuperCombo_DrawCount 1020 |
| D25 | Count: base 16 | SuperCombo_DrawCount 980 |
| D26 | Count: digit u (d + 5) | SuperCombo_DrawCount 2000 |
| D27 | Count: tens v 0x19 | SuperCombo_DrawCount 980 |
| D28 | Count: packet not re-read for the tens | SuperCombo_DrawCount 980 |
| D29 | Box: tpage 0x56 | SuperCombo_DrawBox 2000 |
| D30 | Box: grow 4, 7 | SuperCombo_DrawBox 2000 |
| D31 | Box: y from the x word | SuperCombo_DrawBox 1618 |
| D32 | Box: w + grow | SuperCombo_DrawBox 2000 |
| D33 | Box: shade 0x21 | SuperCombo_DrawBox 2000 |
| D34 | Box: tile commit 0x18 | SuperCombo_DrawBox 2000 |
| D35 | Box: stride 6 | SuperCombo_DrawBox 1584 |
| D36 | Box: no semi-transparency | SuperCombo_DrawBox 2000 |
| D37 | Box: draw mode not put back | SuperCombo_DrawBox 2000 |
| D38 | Box: screen point read once | SuperCombo_DrawBox 169 |
| E1 | Task: phases 1/2 swapped | ElemStrike_Task 797 |
| E2 | Start: +9 1 | ElemStrike_Start 1909 |
| E3 | Start: the kind not taken | ElemStrike_Start 2000 |
| E4 | Start: parameter 0x47 | ElemStrike_Start 2000 |
| E5 | Start: copy +5 0x45 | ElemStrike_Start 2000 |
| E6 | Start: copy +6 0 | ElemStrike_Start 2000 |
| E7 | Start: effect +1 0 | ElemStrike_Start 2000 |
| E8 | Start: effect +4 from +3 | ElemStrike_Start 1991 |
| E9 | Start: 0x904AA9 bit 0x40 | ElemStrike_Start 1489 |
| E10 | Start: CLUT with STP | ElemStrike_Start 2000 |
| E11 | Start: CLUT row 25 | ElemStrike_Start 2000 |
| E12 | Start: dirty not set | ElemStrike_Start 1995 |
| E13 | Start: second child +0xB not counted | ElemStrike_Start 2000 |
| E14 | Start: owner bit 0x80 | ElemStrike_Start 1526 |
| E15 | Tint: at most 2 | ElemStrike_Tint 458 |
| E16 | Tint: table stride 4 | ElemStrike_Tint 891 |
| E17 | Tint: tint a 1 | ElemStrike_Tint 898 |
| E18 | Tint: slot to +9 | ElemStrike_Tint 898 |
| E19 | Tint: owner mask 0x7F | ElemStrike_Tint 621 |
| E20 | Tint: flags 0x10 | ElemStrike_Tint 898 |
| E21 | Tint: claws from 0x84 | ElemStrike_Tint 31 |
| E22 | Tint: claws to 0x89 | ElemStrike_Tint 40 |
| E23 | Tint: the target party at 0..3 | ElemStrike_Tint 68, ElemStrike_Fade 69, ElemStrike_End 51 |
| E24 | Tint: no release | ElemStrike_Tint 898 |
| E25 | Tint: animation (4, 1) | ElemStrike_Tint 898 |
| E26 | Hit: sound table + 2 | ElemStrike_Hit 900 |
| E27 | Hit: 0 sound played | ElemStrike_Hit 171 |
| E28 | Hit: +9 1 | ElemStrike_Hit 1019 |
| E29 | Hit: at +0xB 1 | ElemStrike_Hit 1024 |
| E30 | Fade: below 0xD | ElemStrike_Fade 491 |
| E31 | Fade: +9 up by 3 | ElemStrike_Fade 557 |
| E32 | Fade: down by 1 | ElemStrike_Fade 1923 |
| E33 | Fade: clamped at 0 | ElemStrike_Fade 212 |
| E34 | Fade: record offset 1 | ElemStrike_Fade 2000 |
| E35 | Fade: ends on two channels | ElemStrike_Fade 67 |
| E36 | Fade: flash the actor | ElemStrike_Fade 659 |
| E37 | Fade: 0x4A29C0 not called | ElemStrike_Fade 558 |
| E38 | End: state 5 | ElemStrike_End 413 |
| E39 | End: out not tested | ElemStrike_End 752 |
| E40 | End: state read before the call | ElemStrike_End 177 |
| E41 | End: done bit 2 | ElemStrike_End 1118 |
| E42 | End: the record from the target after the call | ElemStrike_End 22 |
| E43 | Kind: cases 5/9 swapped | ElemStrike_Kind 45 |
| E44 | Kind: default (0, 6) | ElemStrike_Kind 1331 |
| E45 | Kind: bound 0x97 | ElemStrike_Kind 87 |
| E46 | Kind: less 3 | ElemStrike_Kind 832 |
| E47 | Kind: the low byte of the ability | ElemStrike_Kind 16 |
| E48 | Kind: +3 for case 3 0 | ElemStrike_Kind 36 |
| E49 | Child: kinds swapped | ElemStrikeChild_Task 2000 |
| E50 | Copy_Run: steps 1/2 swapped | ElemStrikeCopy_Run 978 |
| E51 | Copy_Run: update always | ElemStrikeCopy_Run 942 |
| E52 | Copy_Play: from 0x86 | ElemStrikeCopy_Play 24 |
| E53 | Copy_Play: second 3 | ElemStrikeCopy_Play 482 |
| E54 | Copy_Play: no tick | ElemStrikeCopy_Play 2000 |
| E55 | Fx_Run: layer 2 | ElemStrikeFx_Run 2000 |
| E56 | Fx_Run: frame table 0x8E3584 | ElemStrikeFx_Run 2000 |
| E57 | Fx_Run: table not put back | ElemStrikeFx_Run 2000 |
| E58 | Fx_Run: steps 1/2 swapped | ElemStrikeFx_Run 1363 |
| E59 | Fx_Run: phase read before the draw mode | ElemStrikeFx_Run 114 |
| E60 | Fx_Start: 0x800001 higher | ElemStrikeFx_Start 1992 |
| E61 | Fx_Start: z from +0x34 | ElemStrikeFx_Start 2000 |
| E62 | Fx_Start: +0x27 0x1B | ElemStrikeFx_Start 1014 |
| E63 | Fx_Start: +0x24 5 | ElemStrikeFx_Start 986 |
| E64 | Fx_Start: +0x29 3 | ElemStrikeFx_Start 2000 |
| E65 | Fx_Start: +0x5C kept | ElemStrikeFx_Start 1987 |
| E66 | Fx_Start: +0x2B 0 | ElemStrikeFx_Start 2000 |
| E67 | Fx_Start: animation 1 | ElemStrikeFx_Start 2000 |
| E68 | Fx_Start: delay table + 1 | ElemStrikeFx_Start 1573 |
| E69 | Fx_Start: +3 tested as +4 | ElemStrikeFx_Start 991 |
| E70 | Fx_Play: sound 0x100 | ElemStrikeFx_Play 532 |
| E71 | Fx_Play: sound at 1 | ElemStrikeFx_Play 821 |
| E72 | Fx_Play: owner +0xB kept | ElemStrikeFx_Play 1326 |
| E73 | Fx_Play: update after the free too | ElemStrikeFx_Play 1333 |
| E74 | Fx_Play: 0 counts down | ElemStrikeFx_Play 506 |

**Re-run 2026-09-26 on the kFlag-fixed harness ([`magic_harness.md`](magic_harness.md) §8): 43 controls in the affected functions, 43 refused**, each by exit 3 with a count only in the functions it touches; the clean self-test after them 0 mismatches, exit 0. Selected: every control planting in one of the eleven section-8 functions - K4..K25 (`LeapSetUp`, `DashCountDown`, `LeapMove`, `GroundMove`, `BackAtStart` and the dash's phases), K36..K40, H13, H14, E23 (`TargetRecord`, used by `ElemStrike_End` too), E38..E42, E52..E54, E70..E74. Skipped: H19 (`ActorRecord`, called only by the two spawns and `ElemStrike_Start`) and H36 (the pool stride, read only by `SuperCombo_Task` and `SuperComboHit_Alloc`), neither in a section-8 function. The plants were rebuilt from the table's descriptions. The thinnest: E42 12 (was 22), E52 14 (was 24), E23 39 in `ElemStrike_End` (was 51), K39 136, K40 137. No fuzz change.

## 7. What nothing reached

No recorded route casts any of these abilities (queue §5); the live check is
the owner using them, with a save that has them or DIV-0045's cheat. Things to
look for:

- Super Combo: a box and a line of text, then button glyphs one at a time,
  each to be pressed within a shortening time; the count; the caster leaping
  with four fading copies behind; one hit sprite per press at the target.
- The Strikes and Claws: the caster's copy playing an animation, an effect
  sprite at the target, the target tinted and fading back, a flash.

What the prompt's texts say is `0x66A0D8`'s data, not read here.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D92, D93, D96 and D120 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the six stack tables and the
  four `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `SuperCombo_SpawnDash`, `_SpawnImages` (four calls) and `ElemStrike_Start`
  (two): slot 255 is `0x93A000 + 255 x 0x84 = 0x9423FC`, past the image's end
  (`0x93F000`), and the record copy writes there - an access violation, in
  ours as in the original.
- **`SuperComboHit_Alloc`'s "none free" (0xFF) is unchecked** in
  `SuperCombo_SpawnHits`: entry 255 lies at `0x6769C0 + 255 x 0x84 =
  0x67ED3C`, inside the image's `.bss`, so the original silently writes six
  bytes and a pointer into whatever lies there (ours writes the same
  addresses). By reading it is not reached: the pool is emptied by
  `SuperCombo_Start`, the count stops at 0x20 (the pool's size), and one dash
  spawns the hits once. The fuzz's stand-in never answers 0xFF, because the
  write lands outside every compared region.
- **The actor and target records are indexed by the battle index - 3,
  unchecked** (the copies of the actor record, the tint of the target).
- **`ElemStrike_Fade` would never end on an odd tint byte** (1 - 2 wraps to
  0xFF and the channel never reaches 0). All 36 bytes of the kind table are
  even (read 2026-09-26), so it is unreachable as the data stands.
- **`SuperCombo_ReadButton` compares the whole Input_Pressed word**: a press
  of the right button together with any other bit counts as a miss and ends
  the prompt. Whether that is felt in play depends on what Input_Pressed
  holds (edges or levels), not measured here.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 41 lines under a `group S02` comment: 38 new,
plus three host extents re-listed smaller (`0049B080 12`, `0049BA90 57`,
`0049BF90 1D5`, each the function's own size). Seven were listed right
already (`0049B1F0`, `0049B2B0`, `0049B390`, `0049B420`, `0049B5F0`,
`0049B700`, `0049B900`).
