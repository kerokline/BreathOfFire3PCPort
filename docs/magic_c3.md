# Group C3: the overlays no ability loads (MAGIC002, MAGIC111)

**Status:** IN PROGRESS (2026-09-26). All 20 functions are ours
(`src/game/magic_c3.cpp`, shadow name `magic_c3`), fuzzed headless through
the shared harness: 0 mismatches over 40,000 rounds. 117 of 120 negative controls are refused; the other three are equivalent mutants (section 6). Nothing
recorded reaches either overlay, so this is fuzz only (section 7).

Round nine, second spell wave, group C3
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4):
two `Magic_Rows` overlays that no ability id's row byte names
([`cut-content.md`](cut-content.md) §2).

| Row | File | Overlay | Loaded by | Extent |
|---|---|---|---|---|
| 2 | 0x22B | MAGIC002 | nothing on the PC (section 1) | `0x499D80..0x49A7AD`, 7 functions, 2,570 bytes |
| 119 | 0x280 | MAGIC111 | item magic: category 0, index 32 | `0x4D6110..0x4D67E5`, 13 functions, 1,673 bytes |

The extents are `tools/magic_rows.py --unit MAGIC002 / MAGIC111 --clones`.
All 20 functions lie inside them; none was found inside or missing. The
names below describe what the code does. Neither row has a label in the
sibling's `names/magic.toml`, since no ability loads it.

## 1. Who loads rows 2, 7, 119 and 147

The brief asked which of the four rows cut-content §2 left open are item
magic, enemy magic or cut content. A row is started only by a kind-2 battle
task whose parameter is the row. A scan of all 265 `E8` calls to
`BattleTask_Create` in `.text` finds two that create kind 2:

- `Battle_StartItemMagic` `0x437780` (the call at `0x4377A6`): the row is
  the byte at `[0x64B274 + (id >> 8) x 4] + (id & 0xFF)`;
- `Battle_StartAbilityMagic` `0x437930` (at `0x437952`): the row is the byte
  at `0x64C1D0 + ability`.

The two loaders `Magic_LoadForItem` / `Magic_LoadForAbility` read the same
two tables. The only reader of the rows' code pointers is
`BattleMagicRow_Run`'s `jmp [eax x 8 + 0x64C2BC]`. So a row is reached
through one of these two tables or not at all.

**The tables, measured 2026-09-26** (a scratch script over `bof3/BOF3.exe`,
not kept):

- **Ability rows** `0x64C1D0`: 232 bytes, up to `Magic_Rows`. Entry `0xE3`
  (227) holds 147, and no entry holds 2, 7 or 119.
- **Item rows** `0x64B274`: four pointers, to `0x64B284`, `0x64B2E8`,
  `0x64B344` and `0x675ED8`. The first three are 100, 92 and 72 bytes apart,
  as the PSX's are (the sibling's `Magic_LoadForItem` evidence). Inside those
  sizes:
  - category 0 index 32 holds 119, and nothing else holds a row of the four;
  - category 3's pointer, `0x675ED8`, points at bytes that are zero in the
    image. No `.text` or `.data` dword but the pointer itself names that
    address, so nothing fills it. Every category-3 item starts row 0, the
    engine's.

  Indices past a sub-table's end do read a 2 (category 1 from index 215,
  category 2 from 123): those are the bytes of whatever follows. The
  originals do not check the index.
- **The PSX, for comparison:** the USA disc's `BIN/BATTLE/BATTLE.EMI` (read
  with `tools/psx_disc.py`, scratch only) holds the PC's 232 ability bytes and
  its first three item sub-tables byte for byte. Its fourth sub-table (the
  sibling's `0x800B3B08`, 96 bytes) has a 2 at index 41. It also holds values
  up to 228, far past the 151 rows, so it may not be a row table at all. The
  PC points category 3 elsewhere.

**So, row by row:**

| Row | Overlay | On the PC |
|---|---|---|
| 2 | MAGIC002 | **Unreachable.** No ability, no in-range item. Cut content, or an item's magic the port dropped with category 3 (the PSX's fourth sub-table has a 2 at index 41, if that is a row table). |
| 7 | MAGIC015's file `0x235`, entry `0x49FCF0` | **Unreachable** on both platforms' tables. Its entry is row 58's (Air Raid read one id down, MAGIC016 folded into MAGIC015); only the file differs. A leftover duplicate row. |
| 119 | MAGIC111 | **Item magic**: item id `0x0020` (category 0, index 32). The sibling's `items.toml` labels consumable 32 "Hourglass". Which category the id's high byte 0 selects is not established, though: the sub-table sizes match no `items.toml` table, the sibling's own open question (`OVERLAY_HEADERS.md` "Open"). So the name is a hypothesis. |
| 147 | MAGIC227 | **Ability `0xE3` (227)**, one past the 227 ids (0..226) in the sibling's list. The ability records at `0x65C4C8` (0x18 bytes each) run to 227, and 227's record is byte-identical to 226's (MeteorStrike, read one id down 225's label). Its file `0x2B6` is MAGIC227, numbered after the ability id as the others are. Who uses 227 (an enemy's AI, an event) was not measured. It is group S38's. |

Enemy magic is not a third path: an enemy's skill goes through the same
ability table.

## 2. What each function does

`symbols.toml` gives each to the instruction. In outline:

- **MAGIC002 (row 2): a ball that flies and shatters.**
  - `Magic002_Task` is the kind-2 task. It is a three-entry stack table:
    `_Start`, `_Wait`, the engine's `0x43FE80` (done flag, free).
  - `Magic002_Start` makes a kind-3 task of parameter 1, `Magic002Ball_Task`:
    - the ball starts at party member 0's screen point, as 16.16;
    - it steps a tenth of the way to the source sprite's (`0x904B4C`) each
      frame;
    - radius 0x28, spread and scale 0x100.
    
    The ball's address goes to the **current slot's** (`0x93B8C4`) `+0x80`,
    over the task's own owner pointer.
  - `Magic002_Wait` goes on once the ball's `+9` reaches 0x28.
  - The ball's two steps:
    - `_Fly`, ten frames: moves, radius up by 4, turn up by 0x100;
    - `_Shatter`, thirty frames: spread up by 4, scale down by 8, turn up by
      0x20, then freed.

    Both call `Magic002Ball_Draw(x, y, radius / 2, turn, spread, scale)`.
  - `Magic002Ball_Draw` draws the ball in screen space, with no GTE
    transform:
    - Four latitude rings, 0x100..0x400, at `(-sin, -cos) x radius >> 12`.
    - Each ring's points run from angle `turn & 0xFF` in steps of 0x100:
      nine points, plus the far end.
    - Each point is shaded against the light `(-0x93D, -0x93D, -0x93D)`
      twice: `Gte_VectorNormalS`, then the integer square root `0x5A7A90` of
      the dot, `<< 8 >> 12`. The first pass uses the normal as it is, the
      second with its y negated.
    - Between two rings, each segment gets two semi-transparent gouraud
      quads:
      - the upper dome, and the lower one mirrored in y;
      - each quad scaled about its own centre by `scale >> 8`, the centre
        pushed out by `spread >> 8`.

    So the shatter is the scale falling while the spread grows. Each point's
    z is computed and never read.
- **MAGIC111 (row 119): a copy of the caster, a wash, a flash.**
  - `Magic111_Task` is a three-entry stack table: `_Start`, `_Wait`,
    `0x43FE80`.
  - `Magic111_Start` does four things:
    - puts the task at the owner;
    - makes three kind-1 children of 0x57, counted in `+0xB`:
      - a copy of the acting actor's record (`0x904B34`: party, or enemy -
        3), its first 0x80 bytes, with the task bytes put back;
      - a wash (`+1 = 1`);
      - a flash (`+1 = 2`);
    - plays sounds 0x100 and 0x101;
    - sets the owner's `+0` bit 0x40.
  - `Magic111_Wait` waits for the children to be gone. Then it plays
    animation 4 on the caster, clears the owner's bit 0x40, sets the target
    flag 0x40, and goes on.
  - `Magic111Child_Task` jumps through `Magic111Child_Kinds` by `+1`.
  - **The copy** (`Magic111Double_Run`): `_Begin` sets `+0x29 = 2`, `_Wait`
    waits for the count to be 1 or less, takes one off it, and the copy is
    freed. It gets `Sprite_UpdateScreen` each frame.
  - **The wash** (`Magic111Wash_Run`, `Magic111Wash_Steps`):
    - `_Start` sets two random angle counters (`+0xC`, `+0x10`).
    - Then MAGIC086's `BarrierRing_Grow` (`+9` up to 0x10), MAGIC078's
      `MagicFx_WaitA` (0x3C frames), and MAGIC060's `0x4B1740` (`+9` down,
      then the count down and free).
    - `Magic111Wash_Draw` draws a full-screen quad with subtractive blending
      (tpage 0x55). Each corner's colour pulses by `Math_Sin` of its angle
      times the fade `+9`.
  - **The flash** (`Magic111Flash_Run`, `Magic111Flash_Steps`):
    - `_Wait` waits for the count to reach 2 (the wash gone).
    - Then MAGIC131's `0x4E5950` (`+9` up by 2 to 0x10) and MAGIC060's
      `0x4B18B0` (down by 2, the count down, free).
    - `Magic111Flash_Draw` draws a full-screen additive quad (tpage 0x35),
      grey `+9 x 15`.

What the three children look like together (a stopped-time effect, going by
the sibling's label for the item) is the owner's eye to judge, not this
reading.

## 3. Calls to other units (by raw address, not bound)

| Address | Owner | Reached as |
|---|---|---|
| `0x43FE80` | the engine (group E, row 128's end) | entry 2 of both kind-2 tasks' stack tables: `0x904AA8 |= 4`, free |
| `0x4B1740` | MAGIC060 (S12, not yet cut) | `Magic111Wash_Steps` entry 3 |
| `0x4B18B0` | MAGIC060 | `Magic111Flash_Steps` entry 2 |
| `0x4E5950` | MAGIC131 (S30, this wave) | `Magic111Flash_Steps` entry 1 |
| `0x5A7A90` | engine, unnamed, in no group | the integer square root: `fild`, `fsqrt`, tail `jmp` to the CRT's `_ftol` `0x5B9550` |

These are called by name, since they are ours already:
- `BarrierRing_Grow` and `MagicFx_WaitA`, which sit in the wash's table;
- `BattleFx_FreeTask`, which sits in the copy's stack table;
- the GPU / GTE layer.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|---|
| `Magic111Child_Kinds` | `0x65B9C8` | 3 |
| `Magic111Wash_Steps` | `0x65B9D4` | 4 |
| `Magic111Flash_Steps` | `0x65B9E4` | 3 |

MAGIC002 has no `.data` of its own that its code reads. The light, the
angles and the full-screen corners are immediates.

## 5. The fuzz

`BOF3X_SHADOW=magic_c3` runs `magic_harness::Run` over all 20 clones, 2,000
rounds each, through the consolidated harness without edits.

**What the group adds.**

- **Callees:** `Gpu_GetTPage`, `Gpu_SetDrawMode`, `Gpu_SetPolyG4` /
  `F4`, `Gpu_SetSemiTrans`, `Math_Sin` / `Cos` (garbage answers, so the
  arithmetic is tested past the sine's range), `0x5A7A90`, and the group's
  three draws called by address (the ball's with its six argument masks).
- **`Gfx_CommitPrim`'s `effect`** logs each primitive's bytes, as many as it
  commits. Every quad of a draw is built in the same bytes, because no
  recorder advances `Gfx_PacketNext`.
- **`Gte_VectorNormalS` is `kThrough`**, run for real on both sides. The ball
  hands it a vector in its own frame and reads the normal back. The square
  root's logged argument, the dot with the light, compares the two.
- **`Group::args`:** the ball's draw gets six words. Half the time they are
  small and on screen; the rest they are anything, with high halves that must
  not matter.
- **Regions:** the wash's angle words `0x903850..53`, `Gfx_PacketNext`, and a
  512-byte packet buffer of the fuzz's own.
- **The three `.data` tables** are swapped for recorders.

**The seed.**

- Every dispatcher gets an index inside its table.
- The acting actor is 0..10, so every party record and enemy record the
  copy can come from.
- Each wait sits either side of its threshold: the ball's `+9` at
  0x27..0x29 through the current slot's `+0x80`, the owner's count at 0..3,
  `+9` at 9..11 and 0x27..0x29.
- `+0` is clear half the time, where a draw is gated on it.

**The disturbance** moves one of these after a call:
- `Gfx_PacketNext`;
- one of the angle words;
- the fade byte `+9`, which the wash re-reads after each sine;
- the owner's count.

Result in this worktree (2026-09-26):

    shadow      magic_c3 self-test: 40000 rounds over 20 functions (2000 each), 871118 calls to the stand-ins,
                0 MISMATCHES; 11880 bytes of state (11 regions) and the stand-ins' log compared

Coverage (calls the originals made): `Gpu_GetTPage` 2000, `Gpu_SetDrawMode`
10000, `Gfx_CommitPrim` 158000, `Gpu_SetPolyG4` 146000, `Gpu_SetPolyF4`
2000, `Gpu_SetSemiTrans` 148000, `Math_Sin` 96000, `Math_Cos` 72000,
`0x5A7A90` 200000, the ball's draw 4000, `Magic111Wash_Draw` 1039,
`Magic111Flash_Draw` 697, `BattleTask_Create` 8000, `BattleTask_FreeCurrent`
1468, `Battle_SetTargetFlag40` 429, `Sound_PlayById` 4000,
`BattleActor_SetAnimation` 429, `Sprite_UpdateScreen` 1056, `Rand` 2000,
and every phase of the five tables (465..1349 each).

`BOF3X_SHADOW='*'`: exit 0, with every group at 0 mismatches.

## 6. Controls

There are 120 plants in `magic_c3.cpp`, each put in one at a time by a
script that was not committed (scratch `c3plant.py`). For each it planted,
rebuilt, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_c3`, restored and
rebuilt. Each anchor was checked to occur exactly once. **117 are refused**:
each exits 3, with a count only in the functions the plant touches. The
figures are mismatched rounds out of 2,000, in this worktree, with the final
seed. **Three are not refused, and all three are equivalent mutants:**

- **D10e:** the last ring's y is kept as a short. It is only ever read as a
  short, so no input can tell the difference. Its near variant D10 (the first
  ring's y taken as +radius) is refused.
- **D42:** meant to copy the previous ring after the ring's first point is
  written, but the plant put the copy before that write. Nothing writes the
  ring between the two places, so it is equivalent as planted. D42b plants it
  where it was meant to go, and is refused.
- **M37e:** the wash's corner 2 red is read back from the scratch cell instead
  of the register. No call lies between the store and the read, so nothing can
  move the cell. Its near variant M37 (corner 2 blue from the other angle) is
  refused.

| | Planted | Refused in |
|---|---|---|
| A1 | Magic002_Task: entries 0/1 swapped | Magic002_Task 1,336 |
| A2 | Start: BattleTask_Create(3, 2) | Magic002_Start 2,000 |
| A3 | Start: x from party +0x2C | Magic002_Start 2,000 |
| A4 | Start: x step / 9 | Magic002_Start 2,000 |
| A5 | Start: y step from x0 | Magic002_Start 2,000 |
| A6 | Start: radius 0x29 | Magic002_Start 2,000 |
| A7 | Start: ball address to Sprite_Current +0x80 | Magic002_Start 823 |
| A8 | Start: scale 0x101 | Magic002_Start 2,000 |
| A9 | Start: spread 0xFF | Magic002_Start 2,000 |
| A10 | Wait: at 0x27 | Magic002_Wait 409 |
| A11 | Wait: signed compare | Magic002_Wait 324 |
| A12 | Ball_Task: entries swapped | Magic002Ball_Task 2,000 |
| A13 | Fly: turn += 0x101 | Magic002Ball_Fly 2,000 |
| A14 | Fly: radius += 5 | Magic002Ball_Fly 2,000 |
| A15 | Fly: on at 10 | Magic002Ball_Fly 409 |
| A16 | Fly: signed compare | Magic002Ball_Fly 385 |
| A17 | draw call: radius >> 2 | Magic002Ball_Fly 2,000, Magic002Ball_Shatter 2,000 |
| A18 | draw call: spread and scale swapped | Magic002Ball_Fly 2,000, Magic002Ball_Shatter 2,000 |
| A19 | Shatter: scale -= 7 | Magic002Ball_Shatter 2,000 |
| A20 | Shatter: freed at 0x28 | Magic002Ball_Shatter 426 |
| A21 | Shatter: spread += 3 | Magic002Ball_Shatter 2,000 |
| A22 | Shatter: turn += 0x21 | Magic002Ball_Shatter 2,000 |
| D1 | draw: GetTPage arguments in PSX order | Magic002Ball_Draw 2,000 |
| D2 | draw: draw mode dtd 1 | Magic002Ball_Draw 2,000 |
| D3 | draw: light -0x93C | Magic002Ball_Draw 1,993 |
| D4 | draw: shade >> 11 | Magic002Ball_Draw 2,000 |
| D5 | draw: lower shade from x, not -y | Magic002Ball_Draw 1,993 |
| D6 | draw: pole at +radius | Magic002Ball_Draw 1,993 |
| D7 | draw: first latitude 0x80 | Magic002Ball_Draw 2,000 |
| D8 | draw: s not negated | Magic002Ball_Draw 1,993 |
| D9 | draw: c >> 11 | Magic002Ball_Draw 1,993 |
| D10 | draw: the pole ring y +radius (equivalent-check near variant) | Magic002Ball_Draw 1,993 |
| D10e | draw: last ring y kept as a short (equivalent: read only as a short) | **not refused**: equivalent |
| D11 | draw: start angle turn & 0x7F | Magic002Ball_Draw 1,020 |
| D12 | draw: angle step 0x101 | Magic002Ball_Draw 2,000 |
| D13 | draw: point x >> 11 | Magic002Ball_Draw 1,993 |
| D14 | draw: point z by c | Magic002Ball_Draw 1,993 |
| D15 | draw: far end +s | Magic002Ball_Draw 1,993 |
| D16 | draw: far end shaded at +s | Magic002Ball_Draw 1,993 |
| D17 | draw: mid / 3 | Magic002Ball_Draw 1,993 |
| D18 | draw: centre floored (>> 2), not truncated | Magic002Ball_Draw 1,991 |
| D19 | draw: xp0 >> 7 | Magic002Ball_Draw 1,993 |
| D20 | draw: xw1 + 1 | Magic002Ball_Draw 1,965 |
| D21 | draw: yp cp + mid | Magic002Ball_Draw 1,993 |
| D22 | draw: yw by spread | Magic002Ball_Draw 1,992 |
| D23 | draw: ox by scale | Magic002Ball_Draw 1,992 |
| D24 | draw: oy >> 7 | Magic002Ball_Draw 1,993 |
| D25 | draw: upper v0 shade prev[j + 1] | Magic002Ball_Draw 2,000 |
| D26 | draw: upper v2 shade [j] | Magic002Ball_Draw 2,000 |
| D27 | draw: lower v0 shade this ring | Magic002Ball_Draw 2,000 |
| D28 | draw: lower blue 1 | Magic002Ball_Draw 2,000 |
| D29 | draw: lower v0 y + oy | Magic002Ball_Draw 1,993 |
| D30 | draw: upper v3 y from yp | Magic002Ball_Draw 1,993 |
| D31 | draw: v1 x from xp0 | Magic002Ball_Draw 1,993 |
| D32 | draw: upper quad opaque | Magic002Ball_Draw 2,000 |
| D33 | draw: lower quad committed 0x40 | Magic002Ball_Draw 2,000 |
| D34 | draw: packet not re-read for the lower quad | Magic002Ball_Draw 1,015 |
| D35 | draw: one segment fewer | Magic002Ball_Draw 2,000 |
| D36 | draw: x not read as a short | Magic002Ball_Draw 2,000 |
| D37 | draw: upper dot with x for y | Magic002Ball_Draw 1,993 |
| D38 | draw: dot z term from y | Magic002Ball_Draw 1,993 |
| D39 | draw: seven points a ring | Magic002Ball_Draw 2,000 |
| D40 | draw: ring start point z 1 | Magic002Ball_Draw 100 |
| D41 | draw: scale not a short | Magic002Ball_Draw 1,983 |
| D42 | draw: prev copied after the ring start | **not refused**: equivalent |
| M1 | Magic111_Task: entries 0/1 swapped | Magic111_Task 1,315 |
| M2 | Start: +8 from the owner +9 | Magic111_Start 1,884 |
| M3 | Start: +0x3C from the owner +0x38 | Magic111_Start 1,954 |
| M4 | Start: first child parameter 0x58 | Magic111_Start 2,000 |
| M5 | Start: party below 4 | Magic111_Start 201 |
| M6 | Start: 0x7C bytes copied | Magic111_Start 2,000 |
| M7 | Start: child +6 = 2 | Magic111_Start 2,000 |
| M8 | Start: child +2 left | Magic111_Start 1,986 |
| M9 | Start: children kinds 2 and 3 | Magic111_Start 2,000 |
| M10 | Start: second sound 0x102 | Magic111_Start 2,000 |
| M11 | Start: owner bit 0x20 | Magic111_Start 1,485 |
| M12 | Start: first child not counted | Magic111_Start 1,940 |
| M13 | Start: first child +0x80 the owner | Magic111_Start 1,706 |
| M14 | Wait: animation 5 | Magic111_Wait 429 |
| M15 | Wait: owner and 0x9F | Magic111_Wait 212 |
| M16 | Wait: flag 0x40 on the actor | Magic111_Wait 390 |
| M17 | Wait: at one child | Magic111_Wait 478 |
| M18 | Child_Task: kinds 0/1 swapped | Magic111Child_Task 1,325 |
| M19 | Double_Run: update when +0 is 0 | Magic111Double_Run 2,000 |
| M20 | Double_Run: steps 0/1 swapped | Magic111Double_Run 1,319 |
| M21 | Double_Begin: +0x29 = 3 | Magic111Double_Begin 2,000 |
| M22 | Double_Wait: at 0 only | Magic111Double_Wait 342 |
| M23 | Double_Wait: down by 2 | Magic111Double_Wait 681 |
| M24 | Double_Wait: signed compare | Magic111Double_Wait 329 |
| M25 | Wash_Run: steps 1/2 swapped | Magic111Wash_Run 962 |
| M26 | Wash_Run: +0x10 up by 2 | Magic111Wash_Run 1,039 |
| M27 | Wash_Run: drawn with +0 clear | Magic111Wash_Run 961 |
| M28 | Wash_Start: Rand & 0x1F | Magic111Wash_Start 1,035 |
| M29 | Wash_Start: + 0x20 | Magic111Wash_Start 2,000 |
| M30 | Wash_Start: +0xA 0x3B | Magic111Wash_Start 2,000 |
| M31 | Pulse: x 5 | Magic111Wash_Draw 1,991 |
| M32 | Pulse: + 9 | Magic111Wash_Draw 1,991 |
| M33 | Pulse: +9 read before the call | Magic111Wash_Draw 845 |
| M34 | Wash_Draw: tpage 0x56 | Magic111Wash_Draw 2,000 |
| M35 | Wash_Draw: corner 0 green from the first angle | Magic111Wash_Draw 1,967 |
| M36 | Wash_Draw: corner 1 offset 0x10 | Magic111Wash_Draw 1,993 |
| M37 | Wash_Draw: corner 2 blue from the second angle | Magic111Wash_Draw 1,969 |
| M37e | Wash_Draw: corner 2 red from the cell, not the register (equivalent: no call between) | **not refused**: equivalent |
| M38 | Wash_Draw: corner 3 offset -0x10 | Magic111Wash_Draw 2,000 |
| M39 | Wash_Draw: corner 3 blue = green | Magic111Wash_Draw 1,960 |
| M40 | Wash_Draw: flat quad | Magic111Wash_Draw 2,000 |
| M41 | FullScreen: 240.0 | Magic111Wash_Draw 2,000, Magic111Flash_Draw 2,000 |
| M42 | Wash_Draw: committed 0x40 | Magic111Wash_Draw 2,000 |
| M43 | Wash_Draw: closing tpage 0x16 | Magic111Wash_Draw 2,000 |
| M44 | Flash_Run: drawn at +2 0 | Magic111Flash_Run 325 |
| M45 | Flash_Run: steps 1/2 swapped | Magic111Flash_Run 1,377 |
| M46 | Flash_Wait: at 1 | Magic111Flash_Wait 357 |
| M47 | Flash_Wait: +9 = 1 | Magic111Flash_Wait 997 |
| M48 | Flash_Draw: grey x 14 | Magic111Flash_Draw 1,993 |
| M49 | Flash_Draw: tpage 0x36 | Magic111Flash_Draw 2,000 |
| M50 | Flash_Draw: committed 0x3C | Magic111Flash_Draw 2,000 |
| M51 | Flash_Draw: last corner x and y swapped | Magic111Flash_Draw 2,000 |
| M52 | Flash_Draw: blue not written | Magic111Flash_Draw 1,995 |
| M53 | Wash_Draw: packet read before the first commit | Magic111Wash_Draw 27 |
| D42b | draw: prev copied after the ring start is written | Magic002Ball_Draw 1,993 |

**The thinnest:**

- **M53 (27):** the wash's packet pointer read before the first commit, not
  after it. It shows only when the disturbance moves `Gfx_PacketNext` during
  that commit.
- **D40 (100):** the ring's first point shaded with z 1 instead of 0. It
  shows only where the normal's rounding changes.
- **M5 (201):** the party / enemy split moved by one. It shows only for
  actor 3.

M17 (the wait gated at one child) was refused in 6 rounds before the seed
put `+0xB` at 0..2 two times in three; it is now refused in 478.

## 7. What nothing reached

No recorded route reaches either overlay. The combat route casts nothing in
the band but rows 21, 44 and 70 (queue §5). Row 2 cannot be reached through
any table on the PC (section 1), so short of a cheat it is dead code. It is
taken for completeness and for the owner, should the living project want to
give it an item.

Row 119's live check is the owner using item `0x0020` in battle, if a save
has one. What to look for:

- the caster standing still, doubled by its copy;
- the screen washed with pulsing subtractive colour;
- a white additive flash;
- the caster's animation 4.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96, D118 and D119 in [`known-defects.md`](known-defects.md).

These are described here, not numbered:

- **Neither start tests `BattleTask_Create`'s 0xFF.** With all 48 slots
  taken, `Magic002_Start` writes the ball's fields at slot 255
  (`0x9423FC..`, past the array). `Magic111_Start` copies 0x80 bytes of the
  actor's record there, three times over. Ours does the same.
- **`Magic002_Start` overwrites its own owner pointer.** It writes the ball's
  address into the current slot's `+0x80`, which is the row task's own owner
  pointer: the owner is lost for the rest of the effect.
  - Nothing after that reads the owner: `Magic002_Wait` reads the field as the
    ball, and `0x43FE80` does not read it.
  - It writes through `0x93B8C4` (the slot `BattleTask_RunAll` is on), not
    `Sprite_Current`. The two are the same slot while `BattleTask_RunAll`
    runs the task.
- **`Magic111_Start` indexes the enemy records by the actor byte - 3,
  unchecked.** An actor above 10 reads past the eight records.
- **Every dispatcher's index is unchecked:** the four stack tables, the
  child kinds by `+1`, the wash's and the flash's `.data` tables by `+2`. Ours
  aborts.
- **`Magic002Ball_Draw`'s oddities.** None is harmful, and all are kept:
  - `Gpu_GetTPage` gets `(0x3C0, 0x100, 0, 0)`, which looks like libgpu's
    `(tp, abr, x, y)` given as `(x, y, tp, abr)`. The page becomes (0, 0)
    instead of (0x3C0, 0x100). That makes no difference, because the quads
    are untextured and the blend is 0 either way.
  - The turn is read as its low byte, so the ring's start angle is the turn
    `& 0xFF`. While the ball flies (the turn up by 0x100 a frame) the ball
    does not turn at all. It turns only in the shatter.
  - The pole's shade is computed ten times over, and every point's z is
    computed and never read.

## 9. Divergence

No ledger entry. Each function is a faithful replacement, except that a
phase past any of the eight dispatch tables aborts. That follows the
project's precedent ([`magic_fx_reached.md`](magic_fx_reached.md) §3).
`Magic002Ball_Draw` also aborts if a ring had more than ten points, which
the original's arithmetic cannot produce: the turn's low byte always gives
ten.

## 10. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy got 20 lines under a `group C3` comment, each
function's own extent. `00499FF0 1085` was a host extent, and is now
re-listed at `7BE`. The host `00499BD0 415` (MAGIC001's last function, S01's)
runs over `0x499D80..`. The consolidation cuts it at the new entry.
