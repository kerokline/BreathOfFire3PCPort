# Group S34: Charm, (no label), Timed Blow, Transfer and Monopolize (MAGIC158, 159, 161, 162, 166)

**Status:** IN PROGRESS (2026-09-27). All 47 functions are ours
(`src/game/magic_s34.cpp`, shadow name `magic_s34`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 94,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S34
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 89 | 0x2A2 | MAGIC158 | 0x9E | Charm | `0x4ED670..0x4EDDC6` | 10 |
| 120 | 0x2A3 | MAGIC159 | 0x9F | (no label) | `0x4EDDD0..0x4EE2D7` | 9 |
| 111 | 0x2A4 | MAGIC161 | 0xA1 | Timed Blow | `0x4EE2E0..0x4EE6BC` | 9 |
| 116 | 0x2A5 | MAGIC162 | 0xA2 | Transfer | `0x4EE6C0..0x4EF20E` | 12 |
| 90 | 0x2A6 | MAGIC166 | 0xA6 | Monopolize | `0x4EF210..0x4EF616` | 7 |

The extents are `tools/magic_rows.py --unit MAGIC1NN --clones` (capstone
recursive descent; no jump table; nothing `REFUSED`). All 47 functions lie in
the units' extents; none was found inside or missing from them, and none was
ours before. MAGIC162's extent holds one function more, `BattleFx_WaitStep4`
(`0x4EE8A0`, group CK's, already ours), which Transfer's table calls by name.
7,664 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. Note the row order: the
queue lists the rows 89, 90, 111, 116, 120 against the overlays in file
order, but `Magic_Rows` pairs MAGIC159 with row 120 and MAGIC166 with row 90
(`analysis/magic_rows.tsv`); the names in the queue line follow the overlays.
What each spell looks like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Charm (MAGIC158)** and **Monopolize (MAGIC166)** are one effect with two
  pools. Each owns a pool of 64 task-like records of 0x84 bytes in `.bss`
  (`Charm_Pool` `0x6A9108`, `Monopolize_Pool` `0x6AC810`): the kind-2 task
  runs its two-entry stack table (`_Start`, `BattleFx_Finish`) and then walks
  the pool, making each live record (bit 0 of +0) `Sprite_Current` with its
  +0x80 as the owner for the record's task, both put back after each.
  - `_Start` clears the pool's +0..+2, puts the task at the source sprite
    (`0x904B4C`: facing and point), and takes 16 (Charm) or 32 (Monopolize)
    records from its allocator (`_PoolAlloc`, first free by bit 0, 0xFF when
    full, tested), each owned by the task, +0 or 0x41, numbered in +0xB, a
    delay of 1..0x20 in +9; sound 0x100.
  - A mote's task is a one-entry `.data` table to `_Run`, which steps through
    a `.data` table by +2 and, while +0 and +2 are set, pushes
    `CharmMote_PushMatrix` (angles 0x400, 0xE00 and the frame counter's low
    five bits, inverted on odd motes; at the mote's point and height, after the
    camera), draws `CharmMote_Draw` (a fan of five semi-transparent flat quads,
    radii +9 x 2 / x 4, coloured by the mote's triple x +0xA) and pops it.
    Monopolize's run calls Charm's push and draw.
  - The steps: `_Launch` waits out the delay, then places the mote round the
    owner (the kind-2 task, at the source sprite's point) at the angle
    (+0xB & 0xF) << 8, picks one of eight colour triples (`Charm_Colours` /
    `Monopolize_Colours`, which hold the same bytes), speed 0x80 and a fall
    (-16 / -8). Charm puts its height word 0x480 off the owner's and has
    `_ToOwner` step it back by 0x60 a frame until it is the owner's, growing
    +9 / +0xA to 0x10; then `_Fall` spirals it (the speed into the height,
    the fall into the speed) and frees it when +0xA runs out. Monopolize
    launches at the owner's height and `_Fall` grows +9 to 0x10 before
    counting +0xA down.
- **MAGIC159** (no label one id down): `Magic159_Start` puts the task at the
  source sprite, makes one child (kind 1, 0x58, +9 8), restores CLUT row 26
  with its STP bits, flags the target 0x10 and plays 0x100. The child
  (`Magic159Child_*`) waits +9 frames, moves to the owner, then grows (+0xA
  up by 2 to 0x10), holds (+9 past 0x20) and fades (+0xA down by 2, then the
  owner's count down and freed), drawing each frame one semi-transparent
  textured gouraud quad at the owner's screen point + (8, -0x10), radius
  +9 x 2, shaded +0xA x 8 on the top corners and +0xA on the bottom ones.
- **Timed Blow (MAGIC161)**: `TimedBlow_Start` puts the task at the owner,
  sets the actor's animation 0xC (`BattleActor_SetAnimation(0xC, 2)`), makes
  a child (kind 1, 0x54) that is a byte copy of the acting actor's record's
  first 0x80 bytes, and sets bit 0x40 of the owner's +0 (what that bit does
  to the owner was not read). The copy
  (`TimedBlowCopy_*`) sizes itself (`BattleFx_SetSize`), ticks its script
  until +9 runs out (`_Strike`: `BattleActor_PlaySound(2, 4)`, the target
  flagged 0x40), then flashes (`_Flash`: +9 up by 4 to 0x10, sound 0x100)
  drawing `TimedBlow_DrawFlash` - two screen-wide gouraud quads,
  (0, 0)..(319, 120) and (0, 120)..(319, 359) in float constants, red at the
  outer edges fading to +9 x (0xC, 6, 6) at the shared one - then
  `BattleFx_ScriptToEnd` (a shared step of eleven files: the script ticked
  until it reports its end, then the owner's count down) and
  `BattleFx_FreeTask`. `TimedBlow_Wait` clears the owner's bit 0x40 and sets
  animation 0x18 once the copy is gone.
- **Transfer (MAGIC162)** has a different pool: 128 records of 0x2C bytes
  (`Transfer_Pool` `0x6AB208`, owner at +0x28) run through a pointer that
  follows the pool (`TransferMote_Current` `0x6AC808`), not through
  `Sprite_Current`. The task's six-entry table is `Transfer_Start`, then four
  shared steps (`BattleFx_TintActor`, `_Brighten`, `_WaitStep4`,
  `Sparkle_End`) and `BattleFx_Finish`, and after it the walk.
  - `Transfer_Start` clears the pool, sets kind +4 = 4, and makes
    `Transfer_Counts[4]` records (60 by the table), each with a colour
    variant +3 (Rand & 3), its number +7 and a delay +5 of (number / 4) x
    `Transfer_Delays[4]` + 1.
  - A record's steps: `_Start` waits the delay, then takes the owner's point
    and screen point, offsets the screen point by `TransferMote_Offsets[+7 &
    0x1F]` and Rand & 7, and sets a sway (+8 base, +0xA the y step Rand & 1,
    +0xC a phase) and a rise length +7; `_Rise` sways it (x = base + a sine
    of the phase x 24 >> 12, y by +0xA) for +7 frames; `_Fade` sways it,
    shrinks +6 every fourth frame and frees it after 0x10 frames.
  - Each frame, a live record draws a star of eight gouraud triangles
    (`_DrawStar`, radius +6 + Rand & 3, centre shade from
    `TransferMote_Colours` x +5, rim +5) and, when its phase +0xC & 3 is not
    0, four gouraud rays (`_DrawRays`) and four two-segment forks
    (`_DrawForks`) turning with the frame counter. The vertices are floats
    (`fild` / `fstp`), as in every PC draw of the port.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the project's
one precedent: a phase past any of the dispatch tables (six stack tables, eight
`.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3).
Calls that push more than the callee takes push it in ours too (the flag
pointer to `Gte_RotTrans`, the depth and flag to `Gte_RotTransPers4`).

## 3. Calls to other units

None by raw address: every callee outside the units is already ours and
called by name - `BattleFx_Finish`, `BattleFx_SetSize`, `BattleFx_TintActor`,
`BattleFx_Brighten`, `BattleFx_WaitStep4` (group CK), `Sparkle_End` (CJ),
`MagicFx_DoneAndFree` (E), `BattleFx_FreeTask`, the effect library's
`BattleActor_*` (L), and the GTE / GPU / sprite / sound / battle-task library.

Shared bodies: `BattleFx_ScriptToEnd` (`0x4EE560`) is a step of eleven files
(`analysis/magic_funcs.tsv`); group S02's `ElemStrikeCopy_Run` calls it by
address (`magic_s02.cpp`'s `kCopyStep2`), which keeps working through the
jmp. `CharmMote_PushMatrix` / `_Draw` are reached from MAGIC158 and MAGIC166,
both in this group.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `Charm_Pool` | `0x6A9108` | 64 x 0x84 |
| `Transfer_Pool` | `0x6AB208` | 128 x 0x2C |
| `TransferMote_Current` | `0x6AC808` | pointer |
| `Monopolize_Pool` | `0x6AC810` | 64 x 0x84 |
| `Charm_Colours` | `0x65BF00` | 8 x 3 |
| `CharmMote_TaskTable` | `0x65BF18` | 1 |
| `CharmMote_Steps` | `0x65BF1C` | 3 |
| `Magic159Child_TaskTable` | `0x65BF28` | 1 |
| `Magic159Child_Steps` | `0x65BF2C` | 4 |
| `TimedBlowCopy_TaskTable` | `0x65BF3C` | 1 |
| `TransferMote_Offsets` | `0x65BF40` | 32 x 2 |
| `TransferMote_Colours` | `0x65BFE0` | 5 x 4 x 3 |
| `Transfer_Counts` | `0x65C01C` | 5 |
| `Transfer_Delays` | `0x65C024` | 5 |
| `Transfer_Rise` | `0x65C02C` | 5 |
| `TransferMote_TaskTable` | `0x65C034` | 1 |
| `Monopolize_Colours` | `0x65C038` | 8 x 3 |
| `MonopolizeMote_TaskTable` | `0x65C050` | 1 |
| `MonopolizeMote_Steps` | `0x65C054` | 2 |

Each count is where the next table starts (the dump of `0x65BF00..0x65C05C`
read 2026-09-27) and, for the byte tables, what the code indexes; the three
by-kind tables are padded to 8 bytes and only kind 4 is ever set. The 0x20
bytes `0x65BFC0..0x65BFDF` after the offsets are read by nothing in the
group. The pools' extents are the walks' and the allocators' counts; the
three are consecutive `.bss`, the current pointer between the second and the
third.

## 5. The fuzz

`BOF3X_SHADOW=magic_s34` runs `magic_harness::Run` over the 47 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s34_fuzz.cpp`:

- **Callees** (38 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs each primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own (group S31's);
  - the projection logs its four SVECTORs through `deref`; the matrix push's
    GTE callees log theirs and write a result where the real ones write;
  - `Sprite_ScriptTickOnce` and `Sprite_UpdateScreen` log which sprite they
    act on;
  - this group's own functions called directly: the three allocators answer
    `kByte` "none or an index inside the pool" (0xFF..0x3F, 0xFF..0x7F) - an
    al answer, and each allocator's clone has `ret_mask 0xFF`; the pool
    tasks, pushes and draws are `kPhase` recorders, MAGIC162's with
    `masks[0]` = `0x6AC808` so the record they run for is logged; the two
    line draws, which take two words, log their arguments masked to 16 bits
    and the current record (an `effect`).
- **Tables:** the eight `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer, `Prim_VertexScratch`, `0x903850..0x90385F`, the three pools and the
  current pointer (`0x6A9108..0x6AE90F`), CLUT row 26 and its source. 43,164
  bytes of state in 15 regions.
- **Seed:** the current record pointer at a pool record; each dispatcher
  inside its table; each count one step before and at its threshold (the
  delays at 1, `_ToOwner`'s 0x10s and the height word 0x60 above the
  owner's, Grow's 0x10, Hold's 0x20, Fade's 2, the flash's 0x10, the
  record's +5 / +6 / +7, every fourth frame); the allocators' pools full, all
  but one taken, or random; the walks' records' owner fields at real slots
  (the harness's disturbance writes through the owner).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte, the current record pointer and a byte of that record, a pool record's
  live bit, and the task's words +0x14, +0x20, +0x2E, +0x30, +0x3E.

Result in this worktree (2026-09-27):

    shadow      magic_s34 self-test: 94000 rounds over 47 functions (2000 each), 1031250 calls to the stand-ins,
                0 MISMATCHES; 43164 bytes of state (15 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (1,027,669
stand-in calls for this group in that run, before the last seed change).

## 6. Controls

CONTROLS_TEXT

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat.
Things to look for, by reading:

- Charm: sixteen coloured motes appear one by one off the source sprite's
  point, close on its height and spiral.
- Monopolize: 32 such motes, launched at that height.
- MAGIC159: the target flagged, one textured glow at its screen point that
  grows, holds and fades.
- Timed Blow: a copy of the acting actor strikes (a sound, the target
  flagged), the screen flashes red at top and bottom.
- Transfer: the shared tint and brighten steps, then sixty sparks rising
  from the owner along a sine, with rays and forks on some.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the six stack tables and the
  eight `.data` tables (an index past a one-entry task table reads the next
  table). Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in
  `Magic159_Start` and `TimedBlow_Start`: slot 255 lies past the image's end
  - an access violation, in ours as in the original (the same address is
  written).
- **`TimedBlow_Start` indexes the enemy records by the actor byte - 3**,
  unchecked above 10, and copies 0x80 bytes from there.
- **`TransferMote_DrawStar` indexes `TransferMote_Colours` by +4 x 4 + +3**
  with no bound; +4 is only ever 4 (`Transfer_Start`), so it stays inside
  the table.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 42 lines under a `group S34` comment: 35 new,
plus seven host extents re-listed smaller (`004ED7D0 12`, `004EDD70 57`,
`004EE580 13D`, `004EE8C0 12`, `004EF190 4A`, `004EF370 12`, `004EF5C0 57`,
each the function's own size). Five were listed right already (`004EDA60`,
`004EDB30`, `004EEBE0`, `004EED70`, `004EEF60`).
