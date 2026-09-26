# The effect library: the helpers every spell calls

**Status:** IN PROGRESS (2026-09-26) - 25 functions ours
(`src/game/magic_lib.cpp`, shadow name `magic_lib`), fuzzed headless through
the shared harness, which this group extended for functions that take
arguments and answer; 106 negative controls, every one refused by a count (exit 3). No recorded route calls any of them:
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

## 4. What the fuzz needs of the harness

A spell's phase takes and answers nothing; the library's functions take up
to seven arguments and answer in eax, and four of their callees cannot be
recorders that answer garbage. This group first extended the harness on
its own branch; group HX folded the wave's extensions into one API
([`magic_harness.md`](magic_harness.md) section 7), and the fuzz uses it:

- **`Group::args`** fills the eight words each function is called with (the
  same on both passes); **`Clone::ret_mask`** is what of eax is compared:
  `0xFF` for `MagicFx_ApplyBuff` and `BattleActor_FxSizeB`, the whole of it
  for `MagicFx_StepAround`, the four near tests and
  `SpriteClut_CopyToFxRow`, nothing for the rest.
- **`Callee::effect`** on four callees, in `magic_lib_fuzz.cpp`:
  `Gte_VectorNormalS` run for real on the vector it was handed (its two
  pointers, each function's own frame, masked off; what `in` held
  `Note`d); `Battle_ActorIsOut` answering from a mask the seed sets, with
  someone standing on each side; `Gfx_CommitPrim` moving `Gfx_PacketNext`
  on by the size, as the real one does, so the popup's two quads land apart;
  the buff roll `0x44FC10` noting what it would read - the target byte, the
  result record pointer, the stats copy - and answering a flag.
- **Ten-argument stand-ins** (`Gpu_SetDrawMode` has five arguments,
  `MagicFx_LinkByDepth` seven).
- **The library in `kStandard`**: this group's hunk of
  `magic_harness.cpp`, the seventeen functions an overlay calls, by name.
  `StandIn` also finds a stand-in by the original's address, so a group
  that calls one by `MH_AT(type, 0x4FBC30)` meets it, and its own listing
  of the callee is ignored (the first registration stands).
- **`Disturb` leaves the target enemy alone** unless the target byte is
  3..10 (a side flag - `0x40` the enemies - has no record inside the image).

## 5. The fuzz, and the controls

`BOF3X_SHADOW=magic_lib`: 25 copies (the two popup tasks' eight immediates
re-aimed at handler recorders, every call at its recorder), the standard
regions plus the group's - `Gfx_PacketNext`, `MapView_Origin`,
`Gfx_BufferIndex`, the depth nodes `0x8022C0..`, the battle bytes
`0x904B50..0x904B8F`, `0x939F80`, the first 0x200 bytes of the packet pool, the first 16 rows of the enemy-type table at `0x8C5652`,
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

Result (2026-09-26, on the consolidated harness, in this worktree - the
counts move with the build directory, the DLL's load address being in the
state):

    shadow      magic_lib self-test: 50000 rounds over 25 functions (2000 each), 57167 calls to the stand-ins,
                0 MISMATCHES; 33884 bytes of state (19 regions) and the stand-ins' log compared
    shadow      magic_lib coverage (calls the originals made): BuffPopup_Draw 2258, Gpu_SetDrawMode 2000,
                Gfx_CommitPrim 6000, Gpu_SetPolyFT4 4000, Gpu_GetTPage 4000, Gpu_GetClut 4000, Gpu_LinkPrim 3097,
                Gte_VectorNormalS 4000, Math_Ratan2 2000, Math_Cos 2000, Math_Sin 2000, Battle_ActorIsOut 7764,
                0x44FC10 2000, BattleTask_Create 2000, BattleTask_FreeCurrent 1372, BattleActor_UpdateScreenXY 2676,
                MagicFx_ApplyBuff 2000, phase 0x4FB100 488, phase 0x4FB190 938, phase 0x4FB1F0 984,
                phase 0x4FB230 1080, phase 0x4FB2C0 510

`BOF3X_SHADOW='*'`: exit 0. The controls below were run again on the
ported fuzz, with the same result as before the port: every one refused.

**106 negative controls**, planted one at a time by a script (not
committed: apply to `magic_lib.cpp`, build, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=magic_lib`, restore). All 106 are refused by a count (exit 3);
a control in a helper two functions share is refused in both, every other
only in the function it touches.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | BuffPopup_Task: table entries 0 and 1 swapped | BuffPopup_Task 1,010 |
| T2 | BuffPopup_Task: draw without the +0 test | BuffPopup_Task 380, BuffPopupAt_Task 392 |
| T3 | BuffPopup_Task: draw the icon at +5 | BuffPopup_Task 1,072, BuffPopupAt_Task 1,141 |
| T4 | BuffPopupAt_Task: entry 0 BuffPopup_Start | BuffPopupAt_Task 461 |
| S1 | BuffPopup_Start: +0x38 from the owner's +0x3C | BuffPopup_Start 1,299 |
| S2 | BuffPopup_Start: +9 down by 2 | BuffPopup_Start 2,000 |
| P1 | both starts: lift 0x17 | BuffPopup_Start 1,299, BuffPopupAt_Start 1,330 |
| P2 | both starts: lift by +0xB | BuffPopup_Start 1,295, BuffPopupAt_Start 1,323 |
| P3 | both starts: speed -13 | BuffPopup_Start 1,299, BuffPopupAt_Start 1,330 |
| P4 | both starts: +0xA = 2 | BuffPopup_Start 1,299, BuffPopupAt_Start 1,330 |
| P5 | both starts: Sprite_Current kept across UpdateScreenXY | BuffPopup_Start 38, BuffPopupAt_Start 40 |
| B1 | BuffPopupAt_Start: the actor +0xB ^ 1 | BuffPopupAt_Start 1,330 |
| B2 | BuffPopupAt_Start (and CenterOnSide): party below 4 | BuffPopupAt_Start 125, MagicFx_CenterOnSide 318 |
| R1 | BuffPopup_Rise: height up to 9 | BuffPopup_Rise 413 |
| R2 | BuffPopup_Rise: on at 13 | BuffPopup_Rise 1,310 |
| R3 | BuffPopup_Rise: speed -5 | BuffPopup_Rise 1,307 |
| R4 | BuffPopup_Rise: speed += 3 | BuffPopup_Rise 2,000 |
| F1 | BuffPopup_Fall: on at 17 | BuffPopup_Fall 1,336 |
| F2 | BuffPopup_Fall: y += word +0x12 | BuffPopup_Fall 2,000 |
| E1 | BuffPopup_End: free at 27 | BuffPopup_End 1,370 |
| E2 | BuffPopup_End: the owner's +0xA down | BuffPopup_End 1,368 |
| E3 | BuffPopup_End: no free | BuffPopup_End 1,368 |
| D1 | BuffPopup_Draw: draw mode dither 0 | BuffPopup_Draw 2,000 |
| D2 | BuffPopup_Draw: commit (2, 0xD) | BuffPopup_Draw 2,000 |
| D3 | BuffPopup_Draw: shadow 0x30 wide | BuffPopup_Draw 1,996 |
| D4 | BuffPopup_Draw: shadow shade 2 | BuffPopup_Draw 2,000 |
| D5 | BuffPopup_Draw: page y 0x101 | BuffPopup_Draw 2,000 |
| D6 | BuffPopup_Draw: CLUT y 0x1FB | BuffPopup_Draw 2,000 |
| D7 | BuffPopup_Draw: icon & 7 | BuffPopup_Draw 244 |
| D8 | BuffPopup_Draw: v at +0x35 from u | BuffPopup_Draw 1,790 |
| D9 | BuffPopup_Draw: the packet re-read after GetTPage | BuffPopup_Draw 74 |
| D10 | BuffPopup_Draw: shadow y0 without +1 | BuffPopup_Draw 1,998 |
| AB1 | MagicFx_ApplyBuff: party result +0x120 | MagicFx_ApplyBuff 516 |
| AB2 | MagicFx_ApplyBuff: enemy result +0x100 | MagicFx_ApplyBuff 1,465 |
| AB3 | MagicFx_ApplyBuff: 28 bytes copied | MagicFx_ApplyBuff 2,000 |
| AB4 | MagicFx_ApplyBuff: target not put back | MagicFx_ApplyBuff 1,989 |
| AB5 | MagicFx_ApplyBuff: answer inverted | MagicFx_ApplyBuff 2,000 |
| AB6 | MagicFx_ApplyBuff: party below 2 | MagicFx_ApplyBuff 172 |
| AB7 | MagicFx_ApplyBuff: record not put back | MagicFx_ApplyBuff 2,000 |
| AB8 | MagicFx_ApplyBuff: stats from +0xA4 | MagicFx_ApplyBuff 522 |
| BP1 | MagicFx_BuffPopup: enemy who + 2 | MagicFx_BuffPopup 966 |
| BP2 | MagicFx_BuffPopup: stat by kind & 1 | MagicFx_BuffPopup 1,003 |
| BP3 | MagicFx_BuffPopup: resisted icon 9 | MagicFx_BuffPopup 679 |
| BP4 | MagicFx_BuffPopup: +9 = 2 | MagicFx_BuffPopup 2,000 |
| BP5 | MagicFx_BuffPopup: owner read before the create | MagicFx_BuffPopup 58 |
| BP6 | MagicFx_BuffPopup: create (1, 0x47) | MagicFx_BuffPopup 2,000 |
| BP7 | MagicFx_BuffPopup: enemies by bit 7 | MagicFx_BuffPopup 463 |
| L1 | MagicFx_LinkByDepth: depth + 1 | MagicFx_LinkByDepth 934 |
| L2 | MagicFx_LinkByDepth: depth 0x38 accepted | MagicFx_LinkByDepth 95 |
| L3 | MagicFx_LinkByDepth: pool end: < not <= | MagicFx_LinkByDepth 157 |
| L4 | MagicFx_LinkByDepth: give back one | MagicFx_LinkByDepth 994 |
| L5 | MagicFx_LinkByDepth: keys unsigned | MagicFx_LinkByDepth 675 |
| L6 | MagicFx_LinkByDepth: the last of equals | MagicFx_LinkByDepth 446 |
| L7 | MagicFx_LinkByDepth: zero key linked | MagicFx_LinkByDepth 730 |
| L8 | MagicFx_LinkByDepth: key not zeroed | MagicFx_LinkByDepth 835 |
| L9 | MagicFx_LinkByDepth: the head not re-computed after the link | MagicFx_LinkByDepth 8 |
| L10 | MagicFx_LinkByDepth: lo(x) test dropped | MagicFx_LinkByDepth 223 |
| L11 | MagicFx_LinkByDepth: origin x not subtracted | MagicFx_LinkByDepth 922 |
| L12 | MagicFx_LinkByDepth: hi unsigned | MagicFx_LinkByDepth 816 |
| L13 | MagicFx_LinkByDepth: depth row * 5 | MagicFx_LinkByDepth 714 |
| ST1 | MagicFx_StepToward: x sar 8 | MagicFx_StepToward 1,758 |
| ST2 | MagicFx_StepToward: height: difference of the halves | MagicFx_StepToward 437 |
| SA1 | both steps: x step sar 4 | MagicFx_StepToward 1,758, MagicFx_StepTowardPoint 2,000 |
| SA2 | both steps: height sar 12 | MagicFx_StepToward 1,663, MagicFx_StepTowardPoint 1,999 |
| SA3 | both steps: speed unsigned | MagicFx_StepToward 861, MagicFx_StepTowardPoint 952 |
| SA4 | both steps: Sprite_Current kept across the normalisation | MagicFx_StepToward 56, MagicFx_StepTowardPoint 64 |
| SP1 | MagicFx_StepTowardPoint: x less 0x3FFF | MagicFx_StepTowardPoint 2,000 |
| SP2 | MagicFx_StepTowardPoint: height sar 2 | MagicFx_StepTowardPoint 2,000 |
| AR1 | MagicFx_StepAround: Ratan2 arguments swapped | MagicFx_StepAround 1,744 |
| AR2 | MagicFx_StepAround: angle & 0x7FF | MagicFx_StepAround 985 |
| AR3 | MagicFx_StepAround: cos sar 11 | MagicFx_StepAround 2,000 |
| AR4 | MagicFx_StepAround: the x slot read after the cosine | MagicFx_StepAround 51 |
| AR5 | MagicFx_StepAround: z by the cosine | MagicFx_StepAround 2,000 |
| N1 | the four near tests: x: >= not > | MagicFx_NearSprite3D 164, MagicFx_NearSprite 198, MagicFx_NearPoint3D 147, MagicFx_NearPoint 188 |
| N2 | the four near tests: half a quarter | MagicFx_NearSprite3D 543, MagicFx_NearSprite 566, MagicFx_NearPoint3D 615, MagicFx_NearPoint 656 |
| N3 | MagicFx_NearSprite3D: height < not <= | MagicFx_NearSprite3D 189 |
| N4 | MagicFx_NearSprite3D: height size >> 8 | MagicFx_NearSprite3D 126 |
| N5 | MagicFx_NearSprite: z against x | MagicFx_NearSprite 884 |
| N6 | MagicFx_NearPoint3D: height the low word | MagicFx_NearPoint3D 325 |
| N7 | MagicFx_NearPoint3D: height < not <= | MagicFx_NearPoint3D 156 |
| N8 | MagicFx_NearPoint: x and z swapped | MagicFx_NearPoint 934 |
| C1 | the three CLUT helpers: row + 0x20 | SpriteClut_SetStp 291, SpriteClut_ClearEntry31 303, SpriteClut_CopyToFxRow 182 |
| C2 | the three CLUT helpers: column + 1 | SpriteClut_SetStp 1,480, SpriteClut_ClearEntry31 2,000, SpriteClut_CopyToFxRow 2,000 |
| C3 | the three CLUT helpers: row bit 3 | SpriteClut_SetStp 1,006, SpriteClut_ClearEntry31 994, SpriteClut_CopyToFxRow 999 |
| SS1 | SpriteClut_SetStp: from entry 0 | SpriteClut_SetStp 1,001 |
| SS2 | SpriteClut_SetStp: the low byte | SpriteClut_SetStp 2,000 |
| SS3 | SpriteClut_SetStp: not dirty | SpriteClut_SetStp 1,995 |
| CE1 | SpriteClut_ClearEntry31: entry 30 | SpriteClut_ClearEntry31 2,000 |
| CE2 | SpriteClut_ClearEntry31: not dirty | SpriteClut_ClearEntry31 1,993 |
| CR1 | SpriteClut_CopyToFxRow: one entry up | SpriteClut_CopyToFxRow 2,000 |
| CR2 | SpriteClut_CopyToFxRow: divisor * 4 | SpriteClut_CopyToFxRow 2,000 |
| CR3 | SpriteClut_CopyToFxRow: one entry fewer | SpriteClut_CopyToFxRow 1,716 |
| RR1 | SpriteClut_RestoreFxRow: one entry fewer | SpriteClut_RestoreFxRow 2,000 |
| RR2 | SpriteClut_RestoreFxRow: not dirty | SpriteClut_RestoreFxRow 1,992 |
| CS1 | MagicFx_CenterOnSide: both bits not a return | MagicFx_CenterOnSide 438 |
| CS2 | MagicFx_CenterOnSide: seven enemies | MagicFx_CenterOnSide 621 |
| CS3 | MagicFx_CenterOnSide: x / 512 not sar 9 | MagicFx_CenterOnSide 750 |
| CS4 | MagicFx_CenterOnSide: x divided unsigned | MagicFx_CenterOnSide 570 |
| CS5 | MagicFx_CenterOnSide: the height unsigned | MagicFx_CenterOnSide 1,181 |
| CS6 | MagicFx_CenterOnSide: party asked as 1..3 | MagicFx_CenterOnSide 941 |
| FS1 | BattleActor_FxSizeB: bit 0 | BattleActor_FxSizeB 236 |
| FS2 | BattleActor_FxSizeB: FxSize's enemy byte | BattleActor_FxSizeB 1,425 |
| FS3 | BattleActor_FxSizeB: the first table for both | BattleActor_FxSizeB 251 |
| FO1 | MagicFx_FormationOffset: +8 & 7 | MagicFx_FormationOffset 997 |
| FO2 | MagicFx_FormationOffset: +0x10 the first short | MagicFx_FormationOffset 1,886 |
| FO3 | MagicFx_FormationOffset: rows of 3 | MagicFx_FormationOffset 793 |

The thinnest are the re-reads across a call - L9 (8: the depth node's
address recomputed after the link, shown only when the disturbance flips
`Gfx_BufferIndex` during it), SA4, AR4, P5, BP5, D9 (38..74) - and the
bounds L2 (95) and D7 (244: icon 8 alone).

**What a first pass missed, and how it was fixed.** Four controls were not
refused by the first seed: AB1 and AB2 (the result record's address, set
only during the buff roll's call) until the roll's recorder became an act
that notes the target byte, the record pointer and the stats copy it would
read; BP1 and BP7 (the enemy branch) until the seed set target bit 6; CS2
and CS6 (an actor dropped) until the side's out-mask left at most one party
member out; FS2 (the enemy table's byte) until the enemies' type bytes were
held to 16 rows made a region. D3, D4 and D10 (the shadow quad) were refused
in some 50 rounds only, because both quads landed on the same packet bytes;
`Gfx_CommitPrim`'s recorder now moves the packet pointer on by the size, as
the real one does. B1 as first planted (the actor from `+0xA`) crashed both
sides on an index past the records and was replanted as `+0xB ^ 1`.

Not planted, because no fuzz can see it: the order of reads with no call
between them (the popup phases re-read Sprite_Current per field; nothing
runs between), and `SpriteClut_CopyToFxRow`'s forward copy against a
memmove (kinds 0..4's cells tile the strip, so no source half-overlaps row
2).

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
appended under a `group L` comment. Its `004FC2D0 14D`, which ran over into MAGIC080
(`0x4FC330`), is corrected to `52` (by the coordinator).
