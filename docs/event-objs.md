# The event script's object ops (group V2)

**Status:** IN PROGRESS (2026-09-23) - twenty-two functions ours in
[`src/game/event_objs.cpp`](../src/game/event_objs.cpp), each read to its
last instruction against its PSX twin (`GAME.EMI` section 0, capture
`9d00fd19`), fuzzed against a copy of Capcom's at start-up
(`BOF3X_SHADOW=event_objs`, 110,000 rounds, 0 mismatches) with 50 negative
controls, 46 refused by a count and 4 that change nothing.
**Not yet through the live batch** - the sixth round's shop A/B runs
centrally after the merge ([`takeover-queue-round6.md`](takeover-queue-round6.md)).
No divergence; one latent defect written down
([`known-defects.md`](known-defects.md) D33).

The sixth parallel round's group V2: the functions around `0x534590`..
`0x536670` that the owner's shop route reaches and the attract sequence does
not (`analysis/shop_catalog.md`, call counts from
`analysis/calltrace/recipe_shop/bof3x.callcounts.tsv`). The queue called them
"the event script's object ops"; read, they are the leader's per-frame field
work - the tiles under its feet, the jump off a ledge, a sprite's shade fade,
two actor-state timers and an equipment tick - called from the leader's state
handlers `0x51AD60` (group Z), `0x52DB90` / `0x52E160` (group V1) and the
state table at `0x660158`. The names are hypotheses about purpose (what a
tile code or an actor bit *means* in the game is not established here -
guesses are marked as such); the evidence for each is its body.

## 1. The functions

Extents by capstone (each body to its last instruction before the `nop`
padding; every jump internal); all 22 sizes agree with the catalogue's.

| PC | name | bytes | PSX twin | shop calls | what |
|---|---|--:|---|--:|---|
| `0x534590` | `Sprite_ShadeFadeBegin` | `0x4C` | `0x801C3430` | 14 | bit 15 on words 0..30 of the object's CLUT in row 15 of `Gfx_ClutStrip` (`0x811380 + +5 * 64`), `+0` bit 0x20, `Gfx_ClutStripDirty`, word 31 = 0 |
| `0x5345E0` | `Field_JumpStart` | `0x2A` | `0x801C34C8` | 100 | `Field_JumpSetUp`; then, for object 0 (`+5`) with bit 3 of neither flag word's low byte, a tail `jmp` to `Field_JumpCamera` |
| `0x534610` | `Field_JumpSetUp` | `0xF6` | `0x801C3530` | 179 | frames `+9`, step `+0xC` / `+0x10`, rise `+0x14` (section 2) |
| `0x534710` | `Field_JumpCamera` | `0x79` | `0x801C3708` | 100 | `MoveScript_F3Divisor`, `Field_Kind2X` / `Z` = the landing point, `MoveScript_FAWord` = the rise |
| `0x534790` | `Sprite_ShadeFadeStep` | `0x67` | `0x801C37D8` | 112 | `Sprite_ShadeRaise(step)`; when done, the palette back from `0x80D380 + +5 * 64` (`Sprite_LoadPalette(.., 0)`), `+0` bit 0x20 and `+0x5C..+0x5F` cleared |
| `0x534800` | `Sprite_ShadeRaise` | `0x7E` | `0x801C388C` | 112 | each non-zero shade byte `+0x5D..+0x5F` plus the step, made `0xC0` once above -64 signed; al 1 when all three are `0xC0` |
| `0x534920` | `Field_TileD0` | `0x62` | `0x801C3AA4` | 181 | footprint all of cell code `0xD0`: state `+1` = 2, `+2` = 6 |
| `0x534990` | `Field_TileA4` | `0x62` | `0x801C3B44` | 103 | the same for `0xA4`, `+2` = 7 |
| `0x534A00` | `Field_FloorDamage` | `0x220` | `0x801C3BE4` | 181 | the first code `0x80..0x88` the footprint is all of: `0x534C20(n)`, `0x534DB0(kind[n])` |
| `0x534F10` | `Field_Bit80Tick` | `0x1A4` | `0x801C4294` | 178 | while an actor has state bit 0x80: a countdown, then the flash, 1 HP off, an effect object (section 3) |
| `0x5350C0` | `Field_Bit20Tick` | `0x56` | `0x801C4590` | 181 | while the leader's actor has bit 0x20: `Field_State +0x125` counts down, the bit cleared at 0 |
| `0x535120` | `Field_Tile89` | `0x27` | `0x801C4788` | 1,816 | `Field_TileTurn(0x89, 2, turn) != 0`, unless `Field_ScriptFlags` bit 10 |
| `0x535150` | `Field_TileTurn` | `0xE7` | `0x801C4644` | 3,632 | on a cell of the code: optionally turn to a free direction, then state 2 / 8, `+0xB` = value |
| `0x535240` | `Field_Tile8A` | `0x27` | `0x801C47D4` | 1,816 | `Field_TileTurn(0x8A, 1, turn) != 0` |
| `0x535270` | `Field_EquipTick` | `0x9B` | `0x801C4820` | 181 | three `Actor_EquipCount` tests of the leader, each worth `0x5373F0(1, member)` |
| `0x535310` | `Actor_EquipCount` | `0x79` | `0x801C48EC` | 561 | how many of a record's equipment bytes of a kind (1: `+0x12`; 2: `+0x13..+0x15`; 3: `+0x16`, `+0x17`) are a value |
| `0x535390` | `AreaMap_CellsAll` | `0x42` | `0x801C49D0` | 2,016 | `wide` (a whole dword) 0: `AreaMap_CellsAll4`, else Capcom's `0x535490` |
| `0x5353E0` | `AreaMap_CellsAll4` | `0xA2` | `0x801C4A10` | 2,016 | the 2 x 2 footprint's cells all the code (section 4) |
| `0x535C50` | `AreaMap_CellsNone` | `0x42` | `0x801C5724` | 3,735 | the same dispatch over `AreaMap_CellsNone4` / `0x535D60` |
| `0x535CA0` | `AreaMap_CellsNone4` | `0xBE` | `0x801C5764` | 3,735 | none of the footprint's cells the code, `0x2n` cells cut to `0x2n & 0x21` first |
| `0x535F50` | `Field_JumpCheckHeight` | `0x6D` | `0x801C5C40` | 176 | ground 0x80 or more above at the landing point: `Field_State +0x128` one less, `Field_JumpStart` again |
| `0x536670` | `Sprite_ApplyVelocity` | `0x2E` | `0x801C67AC` | 520 | `+0x34 += +0xC`, `+0x38 += +0x10`, word `+0x3E +=` word `+0x14` |

**The PSX pairing, corrected twice and extended once.** `psx_pair`'s
`call` tier has `0x5353E0` and `0x535490` swapped (it pairs `0x5353E0` with
`0x801C4B70`), and `0x535CA0` with `0x535D60` likewise (`0x801C58F4`): by
the bodies, `0x801C4A10` - called by `0x801C49D0` when its third argument is
0, the four-cell test - is `0x5353E0`'s twin, and `0x801C5764` is
`0x535CA0`'s. `0x5350C0` had no pair; `0x801C4590` is its twin by the body.
`0x536670` is `call-disputed` there; `0x801C67AC` agrees term for term. Every
other twin is the pair table's, and each was read. Differences between the
platforms are only of form: the PSX divides the frames unsigned (`divu`,
the same values), takes the opposite direction with `xori 4` where the PC has
`xor 0xFC` (the same low three bits), compares the three shade bytes as one
masked word, and its actor records sit 4 bytes lower.

**Not in the list, not taken:** `0x534880` (the fade's mirror, towards
`0x80`), `0x534C20` (the floor damage by kind: HP, MP, the two timers of
section 3), `0x534DB0` (the CLUT flash and sound `0x108`), `0x535490` /
`0x535D60` (the wide footprints), `0x535610` (the turn probe) and
`0x537480` / `0x5373F0` (HP down / up): all called from here, none reached
by the shop route (none in `recipe_shop`'s counts), each a stand-in in the
fuzz, reached through a raw address in `event_objs_callees.h`. The
neighbours `0x535FC0`, `0x535FE0`, `0x536050`, `0x5360C0`.. (paired with
`0x801C5D14`..) are unreached too. No case label posed as a function in this
group, and no pointer-reached function turned up.

## 2. The jump

`Field_JumpSetUp` (called by `Field_JumpStart`, `0x51AE66` and the unreached
`0x535FF7`):

```
speed  = Field_MoveSpeeds[Field_State +0x128]            (a whole byte into a table of six)
frames = (direction 2 or 6 ? 0x20 : 0x10) / speed        (idiv)
frames *= 2 if Field_ScriptFlags bit 12
step   = the direction's pair at 0x6696DC * speed        (x, z)
land   = step * frames + (+0x34, +0x38)
ground = 0x5725C0(land, direction); if DamageScratch's byte: MapView_GroundAt(land)
rise   = (s16 ground - s16 +0x3E) / frames               (idiv)
```

The step table is move_cmds' `kKind2Steps`: `(-2048, -2048)`, `(0, -2048)`,
`(1024, -1024)`, `(2048, 0)`, `(2048, 2048)`, `(0, 2048)`, `(-1024, 1024)`,
`(-2048, 0)` - directions 2 and 6 move half as far per frame, hence twice
the frames. The landing distance is `16 * step` whatever the speed (up to the
division's rounding), so the speed sets only how many frames the jump takes.
`Field_JumpCamera` stores the landing point where the camera follows the
kind-2 object (`Field_Kind2X` / `Z`), and a divisor of 4 or 8 times the speed
in `MoveScript_F3Divisor`. `Field_JumpCheckHeight` looks at the ground at the
same point: if `0x5725C0` says flat and the ground is `0x80` or more above
the object, the speed index goes down by one and the jump starts again.
`Sprite_ApplyVelocity` is the per-frame step (its caller `0x52E140`).

Note the two ground tests read `0x5725C0`'s slope byte in opposite senses:
the set-up takes `MapView_GroundAt` *on* a slope, the check only *off* one.
Both platforms; kept.

## 3. The actor timers and the equipment tick

`Field_State` points at the leader's `ObjTrio` entry
([`field-event.md`](field-event.md) section 2): `+0x124` / `+0x125` two
timers, `+0x148` the actor index, `+0x89` the member. The actor record is
`0x903A70 + n * 0xA4`, its state word `+0x10` (`Field_ActorStates`).

- **`Field_Bit80Tick`**: with `Field_InputFlags` bit 0, the party list at
  `0x904062` (`Party_Count(0)` long, each member's record through
  `MoveScript_EffectState`) is searched for bit 0x80; without it, only the
  leader's. If found, `+0x124` counts down; at 0 it is 10 again,
  `0x534DB0(1)` (a flash of the leader's CLUT and sound `0x108`), then
  `0x537480(1, member)` - one HP, bounded below - for each such member (or for
  `+0x89`), and if any answered non-zero in `ax`, an `Effect_FindFree` object
  with kind `0x41` at the leader's `+0x2E` / `+0x30`. *A guess, not
  checked in game: bit 0x80 is poison, taking a point of HP every ten
  frames.* `Field_MemberTimers` (ours) sets the same 10 from the same bit.
- **`Field_Bit20Tick`**: bit 0x20 and `+0x125`, which `Field_MemberTimers`
  sets to 0x28; at 0 the bit is cleared. `0x534C20` (the floor damage, by
  kind from a stack table) ORs a flags byte into the leader's state and sets
  `+0x124` = 10 for its bit 0x80, `+0x125` = 0x28 for 0x20 - read, not taken.
- **`Field_EquipTick`**: `Actor_EquipCount(leader, 3, 0x16)`, `(3, 0x17)`,
  `(2, 0x1F)`, each non-zero one worth `0x5373F0(1, +0x89)` - one HP up,
  bounded by the maximum. *A guess: equipment that heals as one walks.*
  `Actor_EquipCount` is the "equipment count" of
  [`field-event.md`](field-event.md) (`Field_ZoneCounterRoll` calls it).

## 4. The cells

`AreaMap_CellsAll4` / `AreaMap_CellsNone4` take a 16.16 position and read
`AreaMap_ByteAt` at `(x >> 16, z >> 16)`, `(+1, 0)`, `(0, +1)`, `(+1, +1)`
in that order (the original pushes the dwords whose low words those are;
`AreaMap_ByteAt` reads 16 bits). A cell right of the first counts only if x
has a fraction, one below only if z has, the diagonal only if both. With the
mask byte set the cells' low nibbles are dropped (the code's are not); the
"none" test first cuts each `0x2n` cell to `0x2n & 0x21`. The `wide` choice
in `AreaMap_CellsAll` / `None` tests the whole dword (every caller passes
`+0x70 & 0xFF`). The codes the callers ask for: `0xD0`, `0xA4`, `0x80`..
`0x88`, `0x89`, `0x8A`, `0xA6` (`0x52E825`), `0xA7` (`0x52E715`). What any of
them means in the game is not established.

## 5. Quirks kept

Each is said in a comment where it is implemented.

- **Both divisions of `Field_JumpSetUp` fault on 0** (an `idiv`, not a C++
  `/`): the speed is `Field_MoveSpeeds[Field_State +0x128]`, a whole byte into
  six entries with zeros at 0, 6 and 7; a speed above `0x20` (index 8: 64)
  leaves the frames 0 and the second division faults. See D33.
- **Arguments pushed with stale upper bytes.** `Field_FloorDamage` pushes its
  table index and kind from registers whose upper bytes are left over;
  `Field_JumpSetUp` / `Field_JumpCheckHeight` push the direction in `eax`
  over `Sprite_Current`'s upper bytes; `Field_TileTurn` pushes the height
  word over them; `Field_EquipTick` pushes `Field_State`'s. Every callee reads
  only the low byte or word (read: `0x534C20`, `0x534DB0`, `AreaMap_Slope`,
  `0x535640` / `0x535830` under `0x535610`, `Actor_EquipCount`), so ours
  passes the clean value and the fuzz's stand-ins record only what is read.
- **Answers in al only.** Every function here that answers leaves the rest of
  `eax` as it was (`Field_TileD0` returns `Field_ScriptFlags`' dword with al
  cleared); every call site the shop route counts (section 1's callers) tests
  `al` - read 2026-09-23, not a full E8 scan.
  `AreaMap_CellsAll` / `None` pass their callee's `eax` on whole.
- **Nothing bounds an index.** `Sprite_ShadeFadeBegin` writes CLUT `+5` of
  row 15 for any byte - a row holds eight 32-word CLUTs, so `+5` of 8 or more
  writes into rows 16 and down, and 136 or more past `Gfx_ClutStrip`'s end; `Field_Bit80Tick` writes effect object `n` for any
  byte `Effect_FindFree` answers (it answers `0xFF` or below 20); the actor
  records are indexed by any byte.
- **`Sprite_ShadeRaise` only ever ends at `0xC0`**: a shade byte above -64
  after the add becomes `0xC0` at once, so a positive step from `0x80` climbs
  and snaps, and a byte that was 1..`0x3F` snaps on the first step. A zero
  byte is never raised, and then the fade never ends.
- **`Field_TileTurn` with every direction refused** faces the eighth one
  tried, not the one it had.
- **The re-reads.** Where the original reads `Sprite_Current` or
  `Field_State` again after a call, ours does; where it holds one in a
  register (`Field_JumpCamera`, `Sprite_ShadeFadeBegin`'s loop,
  `Field_Bit80Tick` up to the countdown), ours holds it too.

## 6. The fuzz

`BOF3X_SHADOW=event_objs` ([`event_objs_fuzz.cpp`](../src/game/event_objs_fuzz.cpp)).
Every one of the 22 originals is byte-copied with **every** call re-aimed
at a recording stand-in - the calls between the 22 included, and
`Field_JumpStart`'s tail `jmp` - so each is tested alone; ours runs with the
same stand-ins through `event_objs::g`. No jump tables, no x87, no CRT.

A round: random state with each branch's boundaries seeded, the copy, the
same state again, ours; then compared: one block from `DamageScratch`
through the 256th actor record's equipment bytes (`0x903850..0x90DDE4`: the
slope byte, `Field_ScriptFlags`, every record an index byte can name, the
party list and its 256-byte reach, `MoveScript_FAWord`, `Field_InputFlags`,
`Field_ScriptFlags2`, the `Field_State` pointer, `Field_Kind2Z` / `X`), the
12 bytes from `Sprite_Current` (with `MoveScript_F3Divisor` and
`Gfx_ClutStripDirty`), two sprite and two `Field_State` buffers, the
stand-ins' log (count, hash of every entry, the first 32 kept), the answer's
al; and, for the one function each that writes them, all 256 CLUTs of row 15
and 256 effect objects.

The stand-ins give back what the real callee leaves for the caller:
`0x5725C0` the slope byte in `DamageScratch` (0, 1 or any), `MapView_GroundAt`
a ground either side of the `0x80` threshold, `0x537480` a result in `ax`
with random upper bits, `Party_Count` a count in al over random upper bits
(0, 1..6, `0x80`, `0xFF`), `Effect_FindFree` `0xFF`, 0..19 or any byte,
`AreaMap_ByteAt` mostly the round's code or it with another nibble, or a
`0x2n` cell. And now and then they disturb what the caller reads after them:
`Sprite_Current` and `Field_State` swapped to their second buffers, `+5`,
`+8`, `Field_InputFlags`, both flag words, `Field_State +0x124` / `+0x148` /
`+0x89`. The speed index of `Field_JumpSetUp` rounds is drawn from the
entries that divide (1..5, 14, 15) and no `+9` is ever 0, so that the
fault the original keeps is not what a round meets.

**22 x 5,000 rounds, 175,621 stand-in calls, 0 mismatches** (about a
second; also passes with `BOF3X_SHADOW='*'`). Coverage, from the log: al 1
in 3,340 `ShadeFadeStep` rounds, 2,528 `ShadeRaise`, 782 `TileD0`, 1,850
`Tile89`, 3,346 `TileTurn`, 906 `EquipCount`, 735 `CellsAll4`, 1,728
`CellsNone4` (of 5,000 each); the stand-ins called: `0x5725C0` 20,000,
`MapView_GroundAt` 10,030, `AreaMap_ByteAt` 80,000, `0x534C20` 6,606,
`0x534DB0` 9,462, `0x537480` 33,776, `Effect_FindFree` 1,692, `0x535610`
11,714, `0x5373F0` 14,926, `Field_JumpStart` 1,278, `Field_JumpCamera`
2,106, `Sprite_LoadPalette` 6,680, the wide footprints 5,772 and 5,766.

## 7. The negative controls

One planted bug each, built and run through the fuzz (the runner is a
scratch script), then removed. **46 of 50 refused, every one by a count and
none by a hang or a fault; the other 4 are changes that change nothing.**
Rounds differing, of 5,000 per function:

| # | control | refused |
|--:|---|---|
| 0 | `ShadeFadeBegin`: 30 words, not 31 | 2,496 |
| 1 | `ShadeFadeBegin`: word 30 zeroed, not 31 | 5,000 |
| 2 | `ShadeFadeBegin`: flag 0x40, not 0x20 | 3,757 |
| 3 | `ShadeFadeBegin`: `+5` read once for the loop | **not refused** |
| 4 | `ShadeFadeStep`: palette index 1 | 3,350 |
| 5 | `ShadeFadeStep`: `+0x5C` left | 3,330 |
| 6 | `ShadeRaise`: the clamp at -64 inclusive | **not refused** |
| 7 | `ShadeRaise`: a zero byte raised too | 577 |
| 8 | `JumpStart`: flag bit 4, not 8 | 1,117 |
| 9 | `JumpStart`: `+5` read before the set-up | 234 |
| 10 | `JumpSetUp`: 0x20 frames for direction 2 only | 719 |
| 11 | `JumpSetUp`: flag 0x2000, not 0x1000 | 2,486 |
| 12 | `JumpSetUp`: the slope test inverted | 5,000 |
| 13 | `JumpSetUp`: the z step from the x column | 4,264 |
| 14 | `JumpSetUp`: the rise divided through the SC held before the calls | 779 |
| 15 | `JumpSetUp`: the speed masked to seven bits | **not refused** |
| 16 | `JumpCamera`: divisor 2, not 4 | 714 |
| 17 | `JumpCamera`: `MoveScript_FAWord` from `+0x16` | 4,518 |
| 18 | `JumpCheckHeight`: the threshold 0x81 | 190 |
| 19 | `JumpCheckHeight`: the slope test inverted | 5,000 |
| 20 | `JumpCheckHeight`: the height from the SC held before the calls | 145 |
| 21 | `ApplyVelocity`: the rise added as a signed byte | 3,761 |
| 22 | `TileD0`: state 5, not 6 | 696 |
| 23 | `TileD0` / `A4`: flag bit 0x200 | 2,486 + 2,532 |
| 24 | `TileD0` / `A4`: `+0x70` as the whole dword | 3,698 + 3,764 |
| 25 | `FloorDamage`: kind[4] 0 | 271 |
| 26 | `FloorDamage`: eight codes | 635 |
| 27 | `Tile89`: value 3 | 3,777 |
| 28 | `TileTurn`: the search from the object's own direction | 1,638 |
| 29 | `TileTurn`: seven tries | 218 |
| 30 | `TileTurn`: the turn byte as the whole dword | 1,709 |
| 31 | `TileTurn`: `+8` read once before the search | 457 |
| 32 | `TileTurn`: the probe one step out, not two | 1,562 |
| 33 | `CellsAll`: `wide` tested as a byte | 822 |
| 34 | `CellsAll4`: the mask tested as the whole dword | 438 |
| 35 | `CellsAll4`: the z fraction from its high byte only | 204 (first 2) |
| 36 | `Cells`: the second and third calls swapped | 5,000 + 5,000 |
| 37 | `CellsNone4`: the `0x2n` cut to `0x20` | 244 |
| 38 | `CellsNone4`: the diagonal on either fraction | 116 |
| 39 | `Bit80Tick`: the HP taken tested as the whole eax | 177 (first 62) |
| 40 | `Bit80Tick`: the timer back at 9 | 1,186 |
| 41 | `Bit80Tick`: the effect's `+0xB` from `+8` | 620 |
| 42 | `Bit80Tick`: `+0x89` through the `Field_State` held | 212 (first 9) |
| 43 | `Bit80Tick`: the input flag read once | 518 (first 9) |
| 44 | `Bit80Tick`: the countdown checked through the `Field_State` held | **not refused** |
| 45 | `Bit20Tick`: bit 0x10 cleared too | 73 |
| 46 | `EquipTick`: the third test kind 3 | 5,000 |
| 47 | `EquipTick`: `Field_State` held across the calls | 678 |
| 48 | `EquipCount`: kind 3 counts `+0x15` and `+0x16` | 255 |
| 49 | `EquipCount`: the member as the record index | 967 |

The four not refused change nothing any input can show (the trap from
[`HANDOFF.md`](HANDOFF.md)): the loop of 3 writes only the CLUT region, never
the sprite, so `+5` cannot change under it; in 6 a sum of exactly `0xC0`
becomes `0xC0` either way; in 15 every speed that divides is below `0x80`
(the table's non-zero entries are 1..64), and any other speed faults on both
sides; in 44 no call stands between the decrement and the test, so
`Field_State` cannot move. They are kept in the list as the record of that.

Three controls were first refused only weakly, and each showed a thin spot
in the fuzz, now seeded: 35 (2 rounds - fractions were whole random words,
so a low byte alone was rare; the fractions now draw 0, 1, 0x80, 0xFF in
either byte), 42 and 43 (9 rounds each - the flash's stand-in now swaps
`Field_State` and flips `Field_InputFlags` bit 0, the two things
`Field_Bit80Tick` reads again after it) and 39 (62 - the countdown now ends
from 1 in half the rounds). The numbers above for them are the reruns'.

## 8. What reaches them, and what nothing reached

The shop route reaches all 22 (counts in section 1): walking, the leader's
tile tests every frame, and a jump (`Field_JumpStart` 100 calls). **Only the
fuzz has run:** a floor-damage tile (`0x534C20` / `0x534DB0` never called on
the route: no footprint was all of a code `0x80..0x88`), the turn search of
`Field_TileTurn` (`0x535610` never called: every call had its turn byte 0 or
no code-`0x89`/`0x8A` cell), the wide footprints (`0x535490` / `0x535D60`:
every caller's `+0x70` byte was 0), the bit-0x80 countdown's end (`0x537480`
never called) and the equipment tick's heal (`0x5373F0` never called). A
poisoned walk, a floor-damage tile and the equipment of kind 3 value `0x16`
would reach them - *guesses at what those are, from the bodies*.

## 9. Names

`symbols.toml`'s 2026-09-23 group V2 block: 22 functions with `impl`, all
new entries. The callees owned elsewhere or still Capcom's are raw addresses
in `event_objs_callees.h` - `0x5725C0` is group M's this round - and no
entry names them. Facts for other groups: `0x5725C0` returns
`AreaMap_Slope`'s long and leaves `DamageScratch`'s byte as the slope says
(its callers here test that byte straight after); `0x534C20`, `0x534DB0`,
`0x537480`, `0x537500`, `0x5373F0` are described above for whoever takes
them.
