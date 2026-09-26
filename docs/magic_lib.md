# The effect library: the helpers every spell calls

**Status:** IN PROGRESS (2026-09-25) - 25 functions ours
(`src/game/magic_lib.cpp`, shadow name `magic_lib`), fuzzed headless through
the shared harness, which this group extended for functions that take
arguments and answer; CONTROLS_LINE. No recorded route calls any of them:
fuzz only until the owner's eye.

Group L of round nine ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md),
first wave). The unit `LIBRARY` of `tools/magic_rows.py`: the code linked
after MAGIC226/227 and before MAGIC080 that the BMAGIC overlays call - up to
94 of them one function - and that the engine calls now and then (Restore
Form's `0x4528A3`, `0x43D8E2`). The nine functions already ours
(`FxDim_*`, [`magic_fx_reached.md`](magic_fx_reached.md); `BattleActor_*`,
[`battle_items.md`](battle_items.md)) pair with the PSX's `BATTLE.EMI`
section 15, so on the PSX this is engine code the overlays link against,
not an overlay.

## 1. The extent, and its first function

`0x4FAFF0..0x4FC32F`: 34 functions, 9 ours before, **25 taken here**
(3,899 bytes to their last instructions). The queue gave 35 from
`0x4FAF90`; its first, `0x4FAF90`, is **not the library's** but
MAGIC226/227's last, settled by reading:

- `0x4FAF90` (0x57 bytes) is a pool allocator: the first of 48 records of
  0x84 bytes at `0x6BAD60` whose bit 0 is clear gets bit 0, its index in al,
  0xFF when all are taken. Its only callers are two sites in `0x4FA1C0`
  (MAGIC226/227's extent); nothing in the library calls it.
- The pool is touched by exactly two functions (`pe_xref.py --range
  0x6BAD60:0x6BC5E0`): `0x4FAF90` and, at `0x4F9E1A`, MAGIC226's own entry
  `0x4F9DE0` (the linear sweep files that site under `0x4F9D80`), which
  initialises it. No library function reads it.
- **MAGIC225 ends the same way.** Its last function, `0x4F9D80`, is the
  same 0x57-byte allocator instruction for instruction, for 32 records at
  `0x6B9CE0`; and `0x6B9CE0 + 32 * 0x84 = 0x6BAD60`: the two pools are
  consecutive `.bss` in file order, as the overlays' `.data` is (queue
  section 1). Each overlay carries its own allocator and pool; `0x4FAF90` is
  MAGIC226/227's.

`tools/magic_rows.py`'s `LIBRARY_LO` is now `0x4FAFF0`: the tool puts
`0x4FAF90` in MAGIC226/227 (23 functions there, queue group S38 54, this
unit 25) and reports no new exception. The queue's table and section 1 are
updated to match.

## 2. The functions, and what the spell groups bind to

Every prototype is `symbols.toml`'s; the spell groups of this wave call these
by address (`MH_AT`) and bind to the names after the merge. `u8` means only
the low byte of the pushed dword is read.

| Address | Name | Prototype | Callers (E8 scan) |
|---|---|---|---|
| `0x4FB0A0` | `BuffPopup_Task` | `void (void)` | kind 1 parameter 0x48 |
| `0x4FB100` | `BuffPopup_Start` | `void (void)` | its entry 0 |
| `0x4FB190` | `BuffPopup_Rise` | `void (void)` | entry 1 of both popup tasks |
| `0x4FB1F0` | `BuffPopup_Fall` | `void (void)` | entry 2 of both |
| `0x4FB230` | `BuffPopup_End` | `void (void)` | entry 3 of both |
| `0x4FB260` | `BuffPopupAt_Task` | `void (void)` | kind 1 parameter 2 |
| `0x4FB2C0` | `BuffPopupAt_Start` | `void (void)` | its entry 0 |
| `0x4FB3E0` | `BuffPopup_Draw` | `void (unsigned icon)` (u8) | the two popup tasks |
| `0x4FB6F0` | `MagicFx_ApplyBuff` | `unsigned char (unsigned stat, unsigned actor)` (u8, u8) | 10: MAGIC008 013 040 043 082 088, 225 x2, `MagicFx_BuffPopup` x2 |
| `0x4FB790` | `MagicFx_BuffPopup` | `void (unsigned kind, unsigned who)` (u8, u8) | MAGIC038, 083 |
| `0x4FB880` | `MagicFx_LinkByDepth` | `void (unsigned x, unsigned z, int *keys, unsigned prims, unsigned count, unsigned size, unsigned bias)` (last three u8) | 9: MAGIC062 078 082 083 x2, 114 |
| `0x4FB9F0` | `MagicFx_StepToward` | `void (const unsigned char *to, int speed)` (speed a short) | 13, 4 the engine's |
| `0x4FBA90` | `MagicFx_StepTowardPoint` | `void (unsigned x, unsigned z, unsigned y, unsigned pad, int speed)` (a VECTOR by value, then a short) | 12 |
| `0x4FBB40` | `MagicFx_StepAround` | `int (const unsigned char *to, int radius, int offset)` | MAGIC015 x6, 100 x2 |
| `0x4FBBD0` | `MagicFx_NearSprite3D` | `int (const unsigned char *other, unsigned size)` | 7, 4 the engine's |
| `0x4FBC30` | `MagicFx_NearSprite` | `int (const unsigned char *other, unsigned size)` | 12 |
| `0x4FBC70` | `MagicFx_NearPoint3D` | `int (unsigned x, unsigned z, unsigned yw, unsigned size)` (yw's upper word) | MAGIC048, 108 |
| `0x4FBCD0` | `MagicFx_NearPoint` | `int (unsigned x, unsigned z, unsigned size)` | MAGIC219 |
| `0x4FBE30` | `SpriteClut_SetStp` | `void (const unsigned char *s)` | 10 |
| `0x4FBED0` | `SpriteClut_ClearEntry31` | `void (const unsigned char *s)` | MAGIC001, 015 x2, 018/019 |
| `0x4FBF50` | `SpriteClut_CopyToFxRow` | `unsigned (const unsigned char *s)` | 10 |
| `0x4FC000` | `SpriteClut_RestoreFxRow` | `void (void)` | 7 |
| `0x4FC0E0` | `MagicFx_CenterOnSide` | `void (void)` | 31 (29 overlays, one engine) |
| `0x4FC260` | `BattleActor_FxSizeB` | `unsigned char (void)` | MAGIC009 |
| `0x4FC2D0` | `MagicFx_FormationOffset` | `void (void)` | MAGIC120..124 |

The nine already ours, for completeness: `FxDim_Dispatch` `0x4FAFF0`,
`FxDim_Down` `0x4FB010`, `FxDim_Hold` `0x4FB050`, `FxDim_Up` `0x4FB070`
(all `void (void)`), `BattleActor_SetAnimation` `0x4FB830`,
`BattleActor_UpdateScreenXY` `0x4FBD10`, `BattleActor_Flash` `0x4FBDB0`,
`BattleActor_PlaySound` `0x4FC030`, `BattleActor_FxSize` `0x4FC1F0`.

The seventeen an overlay calls are now in the harness's standard set
(section 3); the popup tasks' phases and draw are reached only from the
library itself.

## 3. What each does

Sprite_Current (`0x937F88`) is the running task slot throughout; "the
owner" is `0x93B940`, the slot's `+0x80`.

**The buff popup.** `MagicFx_BuffPopup(kind, who)` rolls the stat
`MagicFx_BuffStats[kind & 3]` on `who` - an enemy (`who + 3`) when the
target byte `0x904B44` has bit 6, else a party member - through
`MagicFx_ApplyBuff`, then creates a kind-1 task with parameter 0x48
(`BuffPopup_Task`) owned by the current slot, `+4` the icon: `kind`, or 8
when resisted. `MagicFx_ApplyBuff(stat, actor)` points the ability path's
target byte `0x904B54` and result record `0x904B60` at the actor (party
`+0x124`, enemy `+0x104`), copies its 32 bytes from `+0xA0` / `+0xB0` to
`0x939F80`, calls the engine's `0x44FC10(stat)` and puts both back; al is 1
when `0x44FC10` answered 0. `0x44FC10` (not ours, in no group) answers 1
when its roll `0x44F6A0` says so, else moves the result record's byte `+0x14
+ stat` by a signed step clamped to -25..50 and answers 0: a stat change,
so **"buff" is a hypothesis** from reading, as is "resisted" for icon 8.
That icons 0..3 raise and 4..7 lower (the stat is `kind & 3`, the icon the
whole kind) is a guess.

The popup's task runs `BuffPopup_Start` (wait `+9` frames, then the
owner's position and its screen point lifted 0x18 and by `+0xA`),
`_Rise` (the quad grows to 8 high; `+0x10` a speed from -14 by 2 a frame
moves the screen y; 12 frames), `_Fall` (the same from -6; 16 frames) and
`_End` (28 frames, then the owner's child count `+0xB` down and the slot
freed), and after each phase past the first, while the slot is live,
`BuffPopup_Draw(+4)`: a draw mode to slot 2 and two POLY_FT4s - a shadow
one pixel right and down, one wider, shade 1, then the icon, shade 0x80 -
of the icon's cell in `BuffPopup_IconUV` (texture page at (0x380, 0x100),
CLUT at (0x10, 0x1FA)). `BuffPopupAt_Task` is the same with
`BuffPopupAt_Start`, which takes the position of the actor whose battle
index is `+0xB` (MAGIC225 creates it).

**The depth sort.** `MagicFx_LinkByDepth(x, z, keys, prims, count, size,
bias)` links `count` primitives of `size` bytes into one depth node of the
frame (`0x8022C4 + (Gfx_BufferIndex + depth * 6) * 8`), the largest key
(signed) first, zero keys skipped and each key zeroed once used. The depth
is `hi(z) - (lo(z) == 0) - (lo(x) == 0) + hi(x) - MapView_Origin[1] -
MapView_Origin[0] + bias + 2` (hi and lo a dword's words; the `== 0` terms
are as the code has them - what the source wrote is not known). A depth
outside 0..0x37, or a pool already too full for one more primitive, gives
all `count` back (`Gfx_PacketNext -= count * size`).

**The steps.** `MagicFx_StepToward(to, speed)` and
`MagicFx_StepTowardPoint(x, z, y, pad, speed)` normalise the difference to
the target (`Gte_VectorNormalS`, 4096 a unit) and move Sprite_Current by
`n * speed >> 3` on x and z and `>> 11` on the height word `+0x3E`; the
point form takes the effects' coordinates (`(+0x34 sar 9) - 0x4000`, the
same for z, `+0x3E sar 1`). `MagicFx_StepAround(to, radius, offset)` moves
Sprite_Current by `radius` at the angle toward `to` plus `offset`
(`Math_Ratan2`, 12 bits) and answers the angle.

**The near tests.** `MagicFx_NearSprite(other, size)` is 1 when
`sc - other + size / 2` is at most `size` (unsigned) on x and z;
`MagicFx_NearSprite3D` adds the height, `((size >> 9) - other + sc) &
0xFFFF` at most `size >> 8`; the `Point` forms take x, z (and the upper
word of `yw`, which callers pass as a sprite's dword `+0x3C`) instead of a
sprite.

**The sprite-CLUT helpers.** A record's CLUT cell in `Gfx_ClutStrip`: kind
`+0x28`, index `+0x27`, row `index / SpriteClut_Divisors[kind]` (plus 0x10
with `+0x24` bit 2), column `(index % that) * SpriteClut_Mults[kind]`, a
byte each; `SpriteClut_Counts[kind] * 16` entries. `SpriteClut_SetStp` sets
bit 15 on entries 1.. of it, `_ClearEntry31` zeroes entry 31,
`_CopyToFxRow` copies it to the strip's row 2 (`0x80F980`) and answers the
divisor times 2, `_RestoreFxRow` puts row 2 back from
`Gfx_ClutStripSource`; each marks the strip dirty. The names say what the
code does to the palette; what it looks like is the owner's eye.

**The rest.** `MagicFx_CenterOnSide` moves Sprite_Current to the mean
position of the target side's actors not out (`Battle_ActorIsOut`):
enemies with target bit 6, the party otherwise, nothing with bits 6 and 7.
`BattleActor_FxSizeB` is `BattleActor_FxSize`'s twin one byte along (tables
`FxSizeB_Table` / `FxSizeB_AltTable`, the enemy byte `0x8C5653 + 0x8C *
type`); its caller stores it to `+9` as FxSize's do, so "a size" is the
hypothesis. `MagicFx_FormationOffset` sets `+0xC` / `+0x10` from a pair of
shorts in `MagicFx_FormationOffsets` by the row
`MagicFx_FormationRows[byte 0x904B89]` and `+8 & 3`; "formation" for
`0x904B89` is a guess.

No divergence and no ledger entry: each function is a faithful
replacement. Where the original would call through its own stack (a popup
phase past 3) or divide by zero (section 6), ours aborts with a message
(the project's precedent, [`magic_fx_reached.md`](magic_fx_reached.md) §3).

## 4. The harness, extended

A spell's phase takes and answers nothing; the library's functions take up
to seven arguments and answer in eax, and three of their callees cannot be
recorders that answer garbage. [`magic_harness.h`](../src/game/magic_harness.h)
gains, without changing what a spell group writes:

- **`Run(group, Extras)`**: `args(k, a)` fills the eight words function `k`
  is called with (both passes the same), `returns[k]` is what of eax is
  compared (logged after the call), and `acts` - a callee's recorder that
  computes its answer: `Gte_VectorNormalS` run for real on the frame's
  vector it was handed (its two pointers masked off, what `in` held
  `Note`d), `Battle_ActorIsOut` answering from a mask the seed sets with
  someone standing on each side.
- **Eight arguments logged** (`Callee::masks[8]`, the stand-ins take eight):
  `Gpu_SetDrawMode` has five, `MagicFx_LinkByDepth` seven.
- **The library in the standard set** (the seventeen an overlay calls).
  `StandIn` finds a stand-in by the original's address too, so a group of
  this wave that calls one by `MH_AT(type, 0x4FBC30)` still meets it after
  the merge, and its own listing of the callee is ignored (the first
  registration stands).
- **`Disturb` leaves the target enemy alone** when the target byte is 11 or
  more (a side flag - `0x40` the enemies - has no record inside the image;
  the write crashed).

`magic_harness.md` section 4 says the same in its place.

## 5. The fuzz, and the controls

`BOF3X_SHADOW=magic_lib`: 25 copies (the two popup tasks' eight immediates
re-aimed at handler recorders, every call at its recorder), the standard
regions plus the group's - `Gfx_PacketNext`, `MapView_Origin`,
`Gfx_BufferIndex`, the depth nodes `0x8022C0..`, the battle bytes
`0x904B50..0x904B8F`, `0x939F80`, the first 0x140 bytes of the packet pool,
the whole `Gfx_ClutStrip`, row 2 of `Gfx_ClutStripSource`, the keys; 2,000
rounds a function. The group's disturbance moves `Gfx_PacketNext` between
two cells, `Gfx_BufferIndex`, and the ability target and result record.

The seed (`magic_lib_fuzz.cpp`): popup phases 0..3 and the slot's `+0`
sometimes 0; each start's delay at 1 two times in three, the rise's height
at 6..9 and 0xFF and its counter at 11, the fall's at 15, the end's at 27;
the draw's packet inside the pool and icons 0..8; actors 0..10 with garbage
above; for the depth sort a key array of small values (ties, zeros,
negatives) or random ones, a count of 0..12, the depth aimed at -1, 0, 1,
0x36, 0x37 and 0x38 half the time, and the packet pointer one primitive
from the pool's end (one below, at, one and two above) half the time; for
the near tests the other point at the box's edges (`d` = 0, size, size + 1,
size - 1) or inside, and the height at `size >> 8` or one above; CLUT
records of kinds 0..4 whose cell stays inside the strip; the side's target
byte from 0, 1, 2, 0x40, 0x43, 0x80, 0x81, 0xC0, 0xC3, 0x7F.

Result (2026-09-25):

    RESULT_BLOCK

CONTROLS_SECTION

## 6. Defects (Capcom's, latent, kept)

For the coordinator to number; none is new in kind.

- **`MagicFx_CenterOnSide` divides by zero** when every actor of the chosen
  side is out (`idiv` by the count): an integer-divide exception, a crash.
  Whether any caller can run it then (an all-enemies effect finishing after
  the last enemy fell) is not measured. Ours aborts with a message.
- **The CLUT helpers divide by zero for kinds 5..7** (`SpriteClut_Divisors`
  holds 0 there) and read past their tables for kind 8 and up; `+0x28` is
  unchecked. What kinds sprites carry is not measured.
- **`MagicFx_BuffPopup` does not test `BattleTask_Create`'s 0xFF** (every
  slot taken): it writes the popup's fields past the 48 slots - the class
  every creator in the round has.
- **The popup tasks' stack tables are unbounded** (a phase past 3 calls
  through the stack; ours aborts), as every task in the round.
- **Unchecked indices**: `BuffPopup_Draw`'s icon (9 cells; `MagicFx_BuffPopup`
  passes the caller's whole kind byte), `MagicFx_ApplyBuff`'s and
  `BuffPopupAt_Start`'s actor (past 10 reads beyond the enemy records),
  `MagicFx_FormationOffset`'s two table indices.

## 7. What the route reaches, and cross-group calls

Nothing: the combat route's traces enter only the nine library functions
that were ours already (queue section 5). The live check is the owner
casting a spell that calls these (DIV-0045's cheat, or a save): a buff
spell's icon rising and bouncing over its target (MAGIC038 or 083 - which
spells those are is the queue's reading, a hypothesis), effects stepping
toward and circling a target, a palette flash on a sprite.

Calls out of the unit, all by name: `BattleTask_Create`,
`BattleTask_FreeCurrent`, `BattleActor_UpdateScreenXY`, `Battle_ActorIsOut`,
`Gpu_*`, `Gfx_CommitPrim`, `Gte_VectorNormalS`, `Math_Ratan2` / `_Cos` /
`_Sin` (all ours), and one by address, `0x44FC10`, the engine's buff roll
(no group of the round: it is not in the band). No call reaches another
spell group's unit.

## 8. For `analysis/calltrace/entries_logic.txt`

The main checkout's list had 18 of the 25 already, with the extents read
here; the seven popup functions (`004FB0A0 53`, `004FB100 8D`, `004FB190
55`, `004FB1F0 37`, `004FB230 26`, `004FB260 53`, `004FB2C0 112`) are
appended under a `group L` comment. Its `004FC2D0 14D` runs over into
MAGIC080 (`0x4FC330`); the function is `52` - noted in the comment, the
line left for the coordinator.
