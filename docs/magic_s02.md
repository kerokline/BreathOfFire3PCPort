# Group S02: Super Combo, and the Strikes and Claws (MAGIC003, MAGIC004 and the nine files folded into it)

**Status:** IN PROGRESS (2026-09-26). All 48 functions are ours
(`src/game/magic_s02.cpp`, shadow name `magic_s02`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 96,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
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

- **Callees** (28 listed; the standard set supplies the rest):
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
  - `SuperComboHit_Alloc`'s stand-in answers 0..31 only (section 8).
- **Tables:** the four `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; the pool; `0x904B80..0x904B97` (the ability word, the hit count);
  Input_Pressed; `0x9039D8`; `MoveScript_TintRecords`; CLUT row 26 and its
  source. 27,908 bytes of state in 17 regions.
- **Seed:** every pool entry's owner a task slot (the disturbance writes
  through the owner while `SuperCombo_Task` runs an entry); `0x904B3C` at one
  of the harness's sprite records; the ability word the group's ids half the
  time; each dispatcher inside its table; each count-down at 1 or 2; the
  prompt's count at 0xF / 0x10 and 0x1F / 0x20; Input_Pressed nothing, one
  bit, several, the wanted bit alone (half the time) or with bit 0x100; the
  dash's +9 0, 1, 2 or 0xFF; the landing and start points one step away
  (or one off it); the owner's +0xB 0..2 and +0xA 0..0x20; the pool full a
  quarter of the time for the allocator; the fade's channels 0..3; the
  target's state 6; the ability word around the kind table's ends.
  `Group::args` gives the draws a text 0..3, a box 0..4, any button and
  count (half below 10), with garbage above the byte half the time.
  `phase_span = 3`: `ElemStrikeFx_Run` reads its phase after a call.
- **Disturb** (the group's case): `Gfx_PacketNext`, Input_Pressed, the
  ability word, `0x9039D8`, a pool entry's byte, `0x904B3C`, a tint byte.

Result in this worktree (2026-09-26):

    shadow      magic_s02 self-test: 96000 rounds over 48 functions (2000 each), 320813 calls to the stand-ins,
                0 MISMATCHES; 27908 bytes of state (17 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (319,994
stand-in calls for this group in that run: the harness's pointers into the DLL
move a few branches, 0 mismatches).

## 6. Controls

CONTROLS_TEXT

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
