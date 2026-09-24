# The blocked-ahead test

**Status:** IN PROGRESS (2026-09-22) - nine functions ours
(`src/game/field_blocked.cpp`): the two tests the field objects ask before a
step, `Field_ObjectBlockedAhead` and `Field_ObjectBlocked`, the map test under
the first, and the object tests both of them share. Each fuzzed against a copy
of Capcom's with every call re-aimed at a recorder; 28 negative controls, all
refused by count. **Through the live batch check `ab22` + `ab22b`** (2026-09-22, the second parallel round - [`HANDOFF.md`](HANDOFF.md)); section 6 had it owed.

Group G of the second parallel round. The callers were read last round:
[`object-kinds.md`](object-kinds.md) (`Field_ObjectTurn`,
`Field_ObjectOpenDirection`, `Field_ObjectIdleLong`) and
[`movement-script.md`](movement-script.md) section 1b (`Field_ObjectUpdate`,
which asks `Field_ObjectBlocked` before each step of a scripted move).
The map lookups underneath are [`kind2-object.md`](kind2-object.md) section 7
(`AreaMap_Slope`) and [`sprite-draw-order.md`](sprite-draw-order.md) section 16
(`AreaMap_Elevation`).

## 1. The functions

The sprite object's fields these read: `+0` in use (a byte, any non-zero),
`+7` bit `0x80` (the sprite lists skip it), `+8` direction (`& 7`), `+0x34` /
`+0x38` position (16.16, x then y), `+0x3E` height (s16), `+0x70` size - the
object tests' margin, and for the map test how many steps ahead to look.

| Address | Name | Bytes | PSX | What it does |
|---|---|--:|---|---|
| `0x518080` | `Field_ObjectBlockedAhead` | `0x7A` | `FUN_801A2AEC` | the point one step ahead (the argument's position + `Field_DirectionSteps[argument +8 & 7]`, taken before any call); 1 if `Field_MapBlockedAhead(object)`, or `Sprite_ObjectAt` / `Party_MemberAt` there with margin `Sprite_Current +0x70` (re-read for each) find something |
| `0x519670` | `Field_ObjectBlocked` | `0x68` | `FUN_801A4AD0` | the same without the map test |
| `0x518100` | `Field_MapBlockedAhead` | `0x7D` | `FUN_801A2BC8` | (size + 1) steps ahead - the argument's position but **`Sprite_Current`'s** direction and size, `imul` wrapping; size non-zero: `AreaMap_BlockedWide`, size 0: `AreaMap_BlockedNarrow` (Capcom's) |
| `0x518180` | `AreaMap_BlockedWide` | `0x234` (code `0x218`, table `0x1C`) | `FUN_801A2C64` | section 1.1 |
| `0x518620` | `AreaMap_CellBlocked` | `0x131` (code `0x33`, then its two tables) | `FUN_801A3100` | `AreaMap_ByteAt(x, y)`'s high nibble 1-5, A, B or F: 1 |
| `0x518760` | `AreaMap_TooSteep` | `0x3F` | `FUN_801A31F4` | `AreaMap_Slope(x, y, Sprite_Current +8 & 7)`; 1 when sloped (scratch byte `0x903850`, read after) and the result's low 16 bits, signed, exceed `0x40` |
| `0x531CF0` | `Sprite_ObjectAt` | `0xB8` | `0x801BEE0C` | h = `Field_ProbeHeight(x, y)`; the first of the 30 `Sprite_Objects`, then the 4 `Sprite_ObjectsExtra` (numbered from `0x1E`), not `Sprite_Current`, in use, without `+7` bit `0x80`, for which `Sprite_PointInReach(x, y, h, margin, object)`; `0xFF` for none |
| `0x531DB0` | `Field_ProbeHeight` | `0x3B` | `0x801BECF0` | the ground's height at (x, y) (`AreaMap_Elevation`), unless it is more than `0x100` from `Sprite_Current +0x3E` - then that; zero-extended from 16 bits |
| `0x531F10` | `Party_MemberAt` | `0x71` | `0x801BF0B8` | `Sprite_ObjectAt` over `ObjTrio`'s records (`0x14C` apart), `Field_MemberCount` of them re-read after each, **without** the `+7` test |

Extents are the last reachable instruction plus any table the function jumps
through (capstone reachability, 2026-09-22); `attract_catalog.md` has
`0x239` for `0x518180` and `0x136` for `0x518620`, both running on into the
`nop` padding. Every jump of all nine stays inside; the calls out are in the
clone table in the source.

Named data: `Field_DirectionSteps` `0x6697B0` - eight (x, y) pairs, half a
cell (`0x8000`) per axis: 0 (-,-), 1 (0,-), 2 (+,-), 3 (+,0), 4 (+,+), 5 (0,+),
6 (-,+), 7 (-,0). The odd directions are the four axes. The tables the tests
read are the ones already named: the area's cell bytes (`AreaMap_Bytes`,
through `AreaMap_ByteAt`), its height grid (`AreaMap_Slope`,
`AreaMap_Elevation`), and the three object lists `Sprite_Objects`,
`Sprite_ObjectsExtra`, `ObjTrio`.

### 1.1 `AreaMap_BlockedWide`

With X, Y the cell (the high words of x, y) and fx, fy the fractions:

| direction & 0xFF | cells tested (`AreaMap_CellBlocked`) | slopes tested (`AreaMap_TooSteep`) |
|---|---|---|
| 1 | row Y: X, X+1, and X-1 if fx == 0 | (x, y), (x + 0x8000, y), (x - 0x8000, y) |
| 5 | row Y+1 if fy != 0, else Y: the same | the same |
| 7 | column X: Y, Y+1, and Y-1 if fy == 0 | (x, y), (x, y + 0x8000), (x, y - 0x8000) |
| 3 | column X+1 if fx != 0, else X: the same | the same |
| anything else | - (blocked: 1) | - |

In that order; the results are summed as a **byte** and the sum's non-zero-ness
returned (the callees return 0 or 1, so the wrap never happens in play - the
fuzz makes it happen, control C10). A sized sprite therefore never steps
diagonally. The cell coordinates are the low words of dwords whose upper
halves are the neighbouring argument's bytes (the original reads its arguments
unaligned, `[esp + 0x26]`, `[esp + 0x2A]`); `AreaMap_ByteAt` reads 16 bits,
so ours passes shorts.

### 1.2 The cell table

`AreaMap_CellBlocked` indexes a 241-byte table at `0x518660` with
`byte & 0xF0` (only its 16 multiples of `0x10` are ever read) and jumps
through `0x518654`: values 0 and 2 go to `xor al, al`, 1 to `mov al, 1`.
Blocking high nibbles: 1, 2, 3, 4, 5, A, B, F (mask `0x8C3E`). The PSX twin
compares the same eight values in a chain of `if`s. Ours keeps the mask and
**checks it against the exe's two tables at every start-up** (`Fatal` if
they disagree), shadow or not.

## 2. Registers and slots the originals leave, and why ours do not

- **Only al is a result.** Every caller of every one of these reads al alone -
  `test al, al`, `cmp al, 0xFF`, `add bl, al`, `mov [esp+n], al`, or
  `movsx eax, al` (an E8 scan of the whole `.text`: 6 callers of
  `Field_ObjectBlockedAhead`, 2 of `Field_ObjectBlocked`, about 60 of
  `Sprite_ObjectAt`, 12 of `Party_MemberAt`; the in-group ones by reading).
  The originals leave eax's upper bits as their last callee left them (and
  `or al, 0xFF` for "none"); ours return a byte. `Field_ProbeHeight` is the
  exception - its two callers pass the whole eax on - and it is returned
  exactly: the upper half is the upper half of an absolute difference of two
  s16, always 0.
- **Stack bytes in a direction dword.** `Field_MapBlockedAhead` stores the
  direction as one byte of a local dword and pushes the dword (the other
  three bytes: whatever that slot held); `AreaMap_TooSteep` does the same
  with its caller's `ecx` above the byte. Every receiver masks to the byte
  (`AreaMap_BlockedWide` `and ecx, 0xFF`, `AreaMap_BlockedNarrow`
  `and eax, 0xFF`, `AreaMap_Slope` `& 0xFF`, and with a direction of 0..7 it
  never indexes past its four corners). Ours passes zeros above the byte.
  This is **not** a DIV-0021 / DIV-0023 case: those put different bytes into
  memory someone could read; here nothing reads them. No ledger entry.
- **An argument slot overwritten.** `Sprite_ObjectAt` and `Party_MemberAt`
  store the probe height over their own `y` argument slot (they keep y in a
  register). Under cdecl the slot is the callee's; every caller's next
  instruction that touches the stack is `add esp, 0xC` (or `0x14`) - none reads
  it back. Ours does not write it. (`AreaMap_Slope` does emulate its own
  scribbles, because it reads them back itself.)

## 3. The fuzz

`BOF3X_SHADOW=field_blocked`: nine byte-copies, every relative call re-aimed
at a recorder, the two `jmp [reg*4 + table]` of `AreaMap_BlockedWide` and
`AreaMap_CellBlocked` relocated into their copies. The recorders stand in for
everything called, ours and Capcom's alike: `AreaMap_BlockedNarrow`,
`AreaMap_ByteAt`, `AreaMap_Slope` (which sets the scratch flag, the side
effect `AreaMap_TooSteep` reads), `AreaMap_Elevation`, `Sprite_PointInReach`,
and the group's own functions under each other. They record their arguments
(the low 16 bits where the callee reads 16, the low byte of a direction) and,
driven by a hash of the round and the call count, disturb what a caller might
re-read: `Sprite_Current` itself, its size, direction and height, the
object's position and direction, `Field_MemberCount`, an object's in-use byte
or `+7` flag.

One round: one function; the three object lists, the object and sprite
buffers random; then the boundaries - `Sprite_Current` a buffer or any listed
object or member, the object `Sprite_Current` itself half the time (the loops'
case), objects out of use or flagged at 1-in-1 to 1-in-6, the hit early, in
the extra four, a member, or nowhere; sizes 0, 1, 2, `0x80`, `0xFF`;
fractions 0, `0x8000`, `0xFFFF`, 1, `0x7FFF`; directions 1, 3, 5, 7 half the
time, 0..8, `0xFF` and any byte otherwise, with noise above the byte; heights
`0`, `0x7FFF`, `0x8000`, `0xFFFF`; the elevation recorder's answer at 0,
+-`0xFF`, +-`0x100`, +-`0x101`, `0x8000` from the sprite's height; the slope
recorder's at `0x3F`, `0x40`, `0x41`, `0x7FFF`, `0x8000`, `0x8040`, `0xFFFF`;
byte answers mostly 0 (six zeros in a row is how the wide test says open),
then 1, `0x7F`, `0x80`, `0xFF`, and one round in eight every non-zero answer
`0x80`. Theirs, then ours from the same state; compared: al (all of eax for
`Field_ProbeHeight`), the recorders' log, both buffers, the three lists, the
scratch bytes, `Field_MemberCount` and `Sprite_Current`.

Result, 2026-09-22: **45,000 rounds (5,000 a function), 113,372 recorder
calls, 0 mismatches.** Blocked / found per 5,000: ahead 3,092, blocked 2,593,
map 976, wide 3,831, cell 2,574, steep 1,614, object 1,864, member 1,146. With
`BOF3X_SHADOW='*'` every self-test passes (exit 0, 219 ours).

### 3.1 Negative controls

Each planted in ours, built, run headless; the count is mismatching rounds of
45,000. All refused by comparison, none by a hang.

| # | Control | Refused |
|---|---|--:|
| C1 | `Field_ObjectBlockedAhead`: the direction from `Sprite_Current`, not the object | 1,156 |
| C2 | the same: the point computed after the map test | 300 |
| C3 | the same: the size read once for both object tests | 664 |
| C4 | `Field_ObjectBlocked`: a member found as non-zero, not as not-`0xFF` | 2,553 |
| C5 | `Field_MapBlockedAhead`: size steps ahead, not size + 1 | 5,000 |
| C6 | the same: the object's direction, not `Sprite_Current`'s | 1,346 |
| C7 | the same: the wide test for size 0 | 5,000 |
| C8 | `AreaMap_BlockedWide`: direction 5's row chosen by fx, not fy | 365 |
| C9 | the same: the third row cell when fx != 0 | 1,539 |
| C10 | the same: the sum in an int, no byte wrap | 128 |
| C11 | the same: a row's two side slopes swapped | 1,539 |
| C12 | the same: direction 3 always column X | 514 |
| C13 | the same: direction 0 open | 144 |
| C14 | the same: direction masked with 7, not `0xFF` | 900 |
| C15 | `AreaMap_CellBlocked`: the low nibble | 2,514 |
| C16 | the same: nibble D blocking too | 286 |
| C17 | `AreaMap_TooSteep`: `>= 0x40` | 400 |
| C18 | the same: all 32 bits compared, not the low 16 signed | 2,225 |
| C19 | the same: the scratch flag read before the call | 228 |
| C20 | `Field_ProbeHeight`: `>= 0x100` | 800 |
| C21 | the same: the sprite's height read before the call | 682 |
| C22 | the same: sign-extended, not zero-extended | 2,465 |
| C23 | `Sprite_ObjectAt`: `+7` bit `0x80` not skipped | 3,800 |
| C24 | the same: the extra four numbered from `0x1D` | 173 |
| C25 | `Party_MemberAt`: the count read once | 69 |
| C26 | the same: `+7` bit `0x80` skipped | 1,700 |
| C27 | both lists: `Sprite_Current` read once | 312 |
| C28 | both lists: in-use as bit 0, not any non-zero byte | 4,737 |

C10 was **not refused at first** - and was a change that changed nothing:
widening the sum's variable left each `static_cast<unsigned char>(sum + ...)`
in place. Planted properly (the casts removed too) it is refused, 128.

What the fuzz cannot see: the real `AreaMap_BlockedNarrow` (Capcom's, never
run by it - its recorder only proves our `Field_MapBlockedAhead` calls it with
the same arguments) and the interaction with the real map and object lists,
which is the batch check's.

## 4. What the attract cycle reaches

Call counts from `hidden_b` (`attract-remaining.md` 4.5): 827
`Field_ObjectBlockedAhead` = 827 `Field_MapBlockedAhead` = 827
`AreaMap_BlockedWide`, 1,752 `AreaMap_CellBlocked`, 2,481 `AreaMap_TooSteep`,
1,084 `Field_ObjectBlocked`, 1,840 `Sprite_ObjectAt`, 3,664
`Field_ProbeHeight`, 1,824 `Party_MemberAt`. So:

- **`AreaMap_BlockedNarrow` is never called**: every sprite that asks has a
  non-zero `+0x70`. A size-0 object asking `Field_ObjectBlockedAhead`, or any
  caller of `0x5187A0`, would reach it (and it would be the next takeover
  under this group, 0x240 bytes + a 7-entry table at `0x518600`).
- Which of `AreaMap_BlockedWide`'s four direction cases and fraction branches
  run is not measured. 1,752 cell tests over 827 calls is about 2.1 a call:
  an axis direction makes two or three, a diagonal none, so both kinds occur,
  in proportions a per-branch trace would have to give.
- The callers outside the object kinds - `0x40D713` and `0x41F796` (asking
  `Field_ObjectBlockedAhead`), `0x518FEE` (`Field_ObjectBestDirection`), and
  the other ~65 callers of the two object tests (battle and event code in
  `0x409CF2`..`0x5250C1`, `0x535C26`, `0x592549`) - are unread; walking in the
  field with the party (the input recipes) reaches `Party_MemberAt` from the
  leader's side.

## 5. Found on the way

- **`psx_pair.py` paired `0x518180` with `0x801A2EE0` and `0x5183C0` with
  `0x801A2C64`** (both `call` tier) - the wrong way round. `FUN_801A2BC8`
  calls `0x801A2EE0` for a size of 0 and `0x801A2C64` otherwise; the PC's
  `0x518100` calls `0x5183C0` for 0 and `0x518180` otherwise; and the bodies
  agree once swapped (`0x5183C0` masks the points to whole cells with
  `& 0xFFFF0000`, as `0x801A2EE0` does). symbols.toml has the corrected pair.
  A sibling pair called from one site in a two-way branch is exactly what the
  call tier cannot order.
- **The map test and the object tests look along different directions when
  the argument is not `Sprite_Current`.** `Field_ObjectBlockedAhead` takes the
  object tests' point from the argument's direction, `Field_MapBlockedAhead`
  takes the argument's position with `Sprite_Current`'s direction and size.
  In the kind handlers the two are the same object; `0x40D713` / `0x41F796`
  are unread, so whether any caller passes another object is open. The PSX
  does the same. A candidate latent quirk, left alone.
- **A sized sprite cannot step diagonally**: `AreaMap_BlockedWide` answers 1
  for any even direction, on both platforms. By design as far as can be seen
  (`Field_ObjectRandomTurn` only ever picks odd directions), noted because a
  living game that adds diagonal walking meets it first.
- `Party_MemberAt` does not test `+7` bit `0x80` where `Sprite_ObjectAt` does;
  the PSX twins differ the same way. Not a defect as far as can be seen.

No DIV entry; no behaviour change.

## 6. The batch check

Owed: the round's single live batch after the merge. The trace list needs the
nine ranges of section 1 with the sizes there (`0x518180` `0x234`,
`0x518620` `0x131` replacing the list's `0x239` / `0x136`, which reach into
padding - either covers the code).
