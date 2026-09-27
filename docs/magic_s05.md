# Group S05: MAGIC017 and MAGIC018/019 - the actor's images

**Status:** IN PROGRESS (2026-09-26). All 29 functions are ours
(`src/game/magic_s05.cpp`, shadow name `magic_s05`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 58,000 rounds. 126 of 126 negative controls refused,
every one by a count (exit 3). Nothing recorded casts these spells, so this
is fuzz only until the owner sees them cast.

Round nine, third spell wave, group S05
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability ids | Read one id down | Entry |
|--:|---|---|---|---|---|
| 43 | 0x237 | MAGIC017 | 0x11, 0xAB | Astral Warp, Shadowwalk | `0x4A11E0` |
| 5 | 0x238 | MAGIC018 | 0x12 | Giant Growth | `0x4A1980` |
| 6 | 0x239 | MAGIC019 | 0x13 | Aura | `0x4A1B20` |
| 53 | 0x238 | MAGIC018 | 0x16 | SpiritBlast | `0x4A1CF0` |
| 54 | 0x238 | MAGIC018 | 0x77 | Double Blow | `0x4A1F00` |
| 68 | 0x238 | MAGIC018 | 0x0B, 0x8D | Multistrike, Triple Blow | `0x4A2060` |

The extents are `tools/magic_rows.py --unit MAGIC017` / `--unit
MAGIC018/MAGIC019 --clones` (capstone recursive descent; no jump table,
nothing `REFUSED`): MAGIC017 `0x4A11E0..0x4A1977` (15 functions, 1,843
bytes), MAGIC018/019 `0x4A1980..0x4A2187` (14 functions, 1,951 bytes;
MAGIC019's one row sits inside MAGIC018's code, queue §1). All 29 lie in the
extents; none was found inside or missing from them, and none was ours
before. 3,794 bytes, as the queue counted.

The ability labels are the sibling's read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses, and not used as names:
the functions are named by unit and row (`Magic017_*`, `Magic018Row5_*`,
`Magic019_*`, ...) and by what the code does. What each looks like in play
has not been seen.

## 1. What the code does

Every row's start does one thing with different counts: the task takes the
owner's direction (+8) and position (+0x34 / +0x38 / +0x3C), plays the
actor's animation 0xC (`BattleActor_SetAnimation(0xC, 0 or 2)`), and makes
**images**: kind-1 tasks over whose first 0x80 bytes the acting actor's
record is copied (`rep movsd`; party `0x802D40 + 0x14C i` for an actor byte
`0x904B34` of 0..2, else enemy `0x93B960 + 0x128 (i - 3)`), their +0x80 the
task. Then the task waits for the images to count its +0xB down.

**MAGIC017 (row 43).** Six images of its own (kind 1, parameter 0x21), in
three pairs (+9 = 1, 1, 3, 3, 5, 5: the launch delay), +0xB the image's
number 0..5; the task's +0xB counts them (6). The owner's CLUT goes to the
fx row (`SpriteClut_CopyToFxRow`, `_SetStp`), the owner's +0 bit 0x40 is
set, sound 0x100.

| Address | Name | What it does |
|---|---|---|
| `0x4A11E0` | `Magic017_Task` | three-entry stack table by +1: Start, Wait, End |
| `0x4A1210` | `Magic017_Start` | the above |
| `0x4A13B0` | `Magic017_Wait` | at +0xB 0: animation (4, 0), the owner's bit 0x40 off, +1 on (also reached from MAGIC021) |
| `0x4A13E0` | `Magic017_End` | the fx CLUT row restored, the done flag `0x904AA8` bit 2, the task freed |
| `0x4A1400` | `Magic017Image_Task` | the image's kind-1 task: `jmp [0x65A69C + 4 x +1]`, one entry |
| `0x4A1420` | `Magic017Image_Run` | ten-entry stack table by +2, then `Sprite_UpdateScreen` while +0 and +2 are set |
| `0x4A14A0` | `Magic017Image_Appear` | +9 down; at 0 shown semi-transparent at tint 0xC0 (+0 bit 0x20, +0x5C, +0x2B, +0x5D..+0x5F), the owner's CLUT row +0x27, the step (0, +-0x1000 by the number's parity) turned by the direction (`0x446770`); +9 0x10 |
| `0x4A1560` | `Magic017Image_Drift` | on by the step, tint down by 4, 16 frames |
| `0x4A15D0` | `Magic017Image_Leap` | +9 up to 0x10, then to the source sprite (`0x904B4C`) at an offset (-0x20000, +-0x10000) turned; a new step (0, +-0x1000) turned; image 0 plays sound 0x101 |
| `0x4A16D0` | `Magic017Image_Return` | on by the step, tint up by 4, 16 frames; then images 1..5 count the task's +0xB down and free themselves; image 0 turns solid, an enemy actor's animation 2 (`0x435A70`), +9 `BattleActor_FxSize` |
| `0x4A17A0` | `Magic017Image_Strike` | while the task's +0xB is 1 (image 0 alone left): the image's script ticked, +9 down; at 0 the target flagged 0x40, `BattleActor_PlaySound(2, 4)` |
| `0x4A17F0` | `Magic017Image_Script` | the script ticked to its end, +9 8 |
| `0x4A1810` | `Magic017Image_Reshow` | 8 frames; an enemy actor's animation 0; shown at tint 0xC0 again |
| `0x4A1890` | `Magic017Image_Home` | tint down by 8, 8 frames; back to the owner's position |
| `0x4A1920` | `Magic017Image_Done` | tint up by 8, 8 frames; the task's +0xB down (to 0: `Magic017_Wait` goes on); then `BattleFx_FreeTask` |

**MAGIC018/019 (rows 5, 6, 53, 54, 68).** The images are kind 1, parameter
0x1F: **MAGIC008's code** (group S06's), with the type +1 saying which of its
kinds. This unit only starts them and waits.

| Address | Name | What it does |
|---|---|---|
| `0x4A1980` | `Magic018Row5_Task` | four entries by +1: Start, Wait, `MagicFx_DoneAndFree`, `MagicFx_FlagTargetEnd` |
| `0x4A19C0` | `Magic018Row5_Start` | the shared start, animation (0xC, 2); one image of type 0; the owner's bit 0x40; sound 0x100 |
| `0x4A1AF0` | `Magic018Row5_Wait` | at +0xB 0xFF: animation (4, 0), the owner's bit off, +1 on |
| `0x4A1B20` | `Magic019_Task` | three entries: Start, End, `MagicFx_FlagTargetEnd` |
| `0x4A1B50` | `Magic019_Start` | the shared start, animation (0xC, 2); one image of type 1; the owner's CLUT to the fx row, `SpriteClut_ClearEntry31` of the task; no sound, no owner bit |
| `0x4A1CC0` | `Magic019_End` | at +0xB 0xFF: the fx row restored, animation (4, 0), the done flag, the task freed |
| `0x4A1CF0` | `Magic018Row53_Task` | four entries: Start, Wait, `MagicFx_DoneAndFree`, `MagicFx_FlagTargetEnd` |
| `0x4A1D30` | `Magic018Row53_Start` | the shared start, animation (0xC, 2); three images of type 3, +0xB 0..2, counted in the task's +0xB; the owner's CLUT to the fx row; the owner's bit 0x40 |
| `0x4A1EC0` | `Magic018Row53_Wait` | at +0xB 0: the fx row restored, animation (4, 0), the owner's bit off, +1 on (also reached from MAGIC015 / 016) |
| `0x4A1F00` | `Magic018Row54_Task` | four entries: Start, `Magic018_WaitOneImage`, `MagicFx_DoneAndFree`, `MagicFx_FlagTargetEnd` |
| `0x4A1F40` | `Magic018Row54_Start` | the shared start, animation (0xC, 2); one image of type 4; the owner's bit 0x40 |
| `0x4A2060` | `Magic018Row68_Task` | three entries: Start, `Magic018_WaitOneImage`, `MagicFx_DoneAndFree` |
| `0x4A2090` | `Magic018Row68_Start` | +0xB 0, +1 on, animation (0xC, 2) - the owner's direction and position **not** taken; one image of type 5; the owner's bit 0x40 |
| `0x4A2160` | `Magic018_WaitOneImage` | at +0xB 0xFF: the owner's bit off first, then animation (4, 0), +1 on |

The tasks' fourth entries (`MagicFx_FlagTargetEnd` in rows 5, 53, 54, and
row 6's third) are never reached: the entry before frees the task.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, except that a phase
past any of the eight dispatch tables (six stack tables by +1, one by +2, the
one-entry `.data` table) aborts ([`magic_fx_reached.md`](magic_fx_reached.md)
§3). `Magic017Image_Task` calls `Magic017Image_Run` by its address rather
than reading its one table entry (group S31's precedent for a one-entry
table); nothing moves an image's +1 from the 0 its start stores.

`0x435A70` is pushed the actor byte with stale upper bytes in the original
(the Sprite_Current pointer's in `Return`, the caller's ecx in `Reshow`);
ours passes the byte. The callee masks to a byte.

## 3. Calls to other units

By raw address (Capcom's, unnamed, in no group; never bound or renamed here):

| Address | Reached as |
|---|---|
| `0x446770` | the dx / dz turn of a task's +0xC / +0x10 by its +8 (as S22, S23, S31 call it) |
| `0x435A70` | (actor, animation): the enemy record of battle index `actor` made the current enemy `0x939AD8` for `BattleEnemy_SetAnimation(animation)`, then the old one back |

Step handlers held in the tables, called through their addresses by name
(`bof3::addr::`): `MagicFx_DoneAndFree` `0x43FE80` and `MagicFx_FlagTargetEnd`
`0x43F460` (group E), `BattleFx_FreeTask` `0x4AEE90` (round eight).

By name, already ours: the effect library (`BattleActor_SetAnimation`,
`_PlaySound`, `_FxSize`, `SpriteClut_CopyToFxRow`, `_SetStp`,
`_ClearEntry31`, `_RestoreFxRow`), `BattleTask_Create`,
`BattleTask_FreeCurrent`, `Battle_SetTargetFlag40`, `Sound_PlayById`,
`Sprite_ScriptTickOnce`, `Sprite_UpdateScreen`.

**Created, not called:** MAGIC018/019's images are kind 1, parameter 0x1F -
MAGIC008's code, group S06's (third wave). Whether rows 5, 6, 54 and 68 end
depends on that code counting the task's +0xB down to 0xFF (row 53's three
to 0), which this group did not read.

**Shared bodies:** `magic_rows.py` has MAGIC021 (group S07) reaching
`Magic017_Wait` and MAGIC015 / 016 (group S04) reaching `Magic018Row53_Wait`
(through their stack tables' addresses: taking them changes nothing for
those callers).

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `Magic017Image_TaskTable` | `0x65A69C` | 1 |

The tool's "7 code entries" note counts every code pointer that follows:
the next dword (`0x4A2480`) is MAGIC008's.

## 5. The fuzz

`BOF3X_SHADOW=magic_s05` runs `magic_harness::Run` over the 29 clones, 2,000
rounds each (`src/game/magic_s05_fuzz.cpp`). Beyond the standard set the
group lists, in its own file:

- `0x446770`: logs the task's direction and pair, writes a new pair where the
  real one writes (S31's effect);
- `0x435A70`: two byte arguments;
- `Sprite_ScriptTickOnce` and `Sprite_UpdateScreen` as custom stand-ins that
  log `Sprite_Current`; the tick answers from its own stream, not the
  disturbance's, and `BattleActor_FxSize` answers from its own stream too -
  the `kFlag` blind spot worked round as group E did (a `kFlag` recorder
  answers 0 exactly when its own disturbance did nothing);
- the `.data` table `0x65A69C`, one entry.

No region beyond the standard ones (the images are written into the task
slots, read from the party and enemy records; the CLUT rows are the library's,
recorded). **Seeds:** the actor byte anywhere in 0..10 half the time (party
0..2, enemies 0..7 - both branches of the record choice and of the
`actor >= 3` tests); each dispatcher inside its table; each counter one step
before or at its threshold (+9 at 1 / 2, or 0xF / 0x10 for `Leap`); the
image number +0xB at 0..2 for `Leap` and `Return`, the task's +0xB either side
of each wait's end (0 / 1, 0xFE / 0xFF); the owner's +0xB at 1 for `Strike`.

Result in this worktree (2026-09-26):

    shadow      magic_s05 self-test: 58000 rounds over 29 functions (2000 each), 87982 calls to the stand-ins,
                0 MISMATCHES; 11360 bytes of state (8 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (the
coverage line in `build/bof3x.log`; the thinnest `BattleActor_FxSize` 120 and
`Battle_SetTargetFlag40` 199). `BOF3X_SHADOW='*'`: exit 0 (88,111 stand-in
calls for this group in that run: the harness's pointers into the DLL move a
few branches, 0 mismatches).

## 6. Controls

126 plants, each put in `magic_s05.cpp` one at a time by a script (not
committed) that anchored each on a unique string, planted, rebuilt, checked
the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=magic_s05`, restored; after the last it restored, rebuilt and ran
the clean self-test (0 mismatches). **126 of 126 refused**, every one by exit
3 with a count only in the functions the plant touches. The first run left
one standing, M43 (`Return` freeing images above 1 only): not an equivalent
mutant but a seed gap - the image number was never 1 at the threshold. The
seed now puts it at 0..2, and the whole set was run again (the counts below).
No equivalent mutant was planted; none is left standing.

The thinnest (fewer than 40 rounds) are all a cell read before a call where
the original reads it after, refused only when the disturbance moves it:

- **M61** (`Reshow`: the task read before the call): 11
- **R35** (`WaitOneImage`: the owner and-ed after the call): 16
- **M29** (`Appear`: +2 through the pointer read before the turn): 17
- **M47** (`Return`: enemies above 3): 18
- **M55** (`Strike`: +9 down before the tick): 35

M- plants are MAGIC017's functions, R- MAGIC018/019's, H- the shared helpers
(the start, the image, the CLUT, the tint).

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| M1 | Magic017_Task: entries 1/2 swapped | Magic017_Task 1325 |
| M2 | Magic017_Start: animation arg 1 | Magic017_Start 2000 |
| M3 | Magic017_Start: five images | Magic017_Start 2000 |
| M4 | Magic017_Start: image +9 i + 1 | Magic017_Start 2000 |
| M5 | Magic017_Start: image +0xB i ^ 1 | Magic017_Start 2000 |
| M6 | Magic017_Start: task +0xB up before the image fields | Magic017_Start 218 |
| M7 | Magic017_Start: image +5 0x22 | Magic017_Start 2000 |
| M8 | Magic017_Start: sound 0x101 | Magic017_Start 2000 |
| M9 | Magic017_Start: owner bit 0x20 | Magic017_Start 1482 |
| M10 | Magic017_Start: image +1 1 | Magic017_Start 2000 |
| M11 | Magic017_Start: image +6 2 | Magic017_Start 2000 |
| M12 | Magic017_Start: image +0x80 the owner | Magic017_Start 1829 |
| M13 | Magic017_Wait: at 1 | Magic017_Wait 1385 |
| M14 | Magic017_Wait: animation (4, 1) | Magic017_Wait 702 |
| M15 | Magic017_Wait: owner &= 0x9F | Magic017_Wait 362 |
| M16 | Magic017_End: done bit 2 | Magic017_End 1433 |
| M17 | Magic017_End: no CLUT restore | Magic017_End 2000 |
| M18 | Magic017Image_Task: the wrong handler | Magic017Image_Task 2000 |
| M19 | Magic017Image_Run: Drift / Leap swapped | Magic017Image_Run 389 |
| M20 | Magic017Image_Run: update gated on +1 | Magic017Image_Run 384 |
| M21 | Magic017Image_Run: update without the +2 test | Magic017Image_Run 123 |
| M22 | Appear: at 1 | Magic017Image_Appear 1016 |
| M23 | Appear: +0x27 the owner's +0x28 | Magic017Image_Appear 514 |
| M24 | Appear: side by bit 1 | Magic017Image_Appear 265 |
| M25 | Appear: step 0x1001 | Magic017Image_Appear 268 |
| M26 | Appear: +9 0x11 | Magic017Image_Appear 515 |
| M27 | Appear: +0xC 1 | Magic017Image_Appear 514 |
| M28 | Appear: the owner turned | Magic017Image_Appear 447 |
| M29 | Appear: +2 through the old pointer | Magic017Image_Appear 17 |
| M30 | Drift: y by the x step | Magic017Image_Drift 2000 |
| M31 | Drift: tint - 3 | Magic017Image_Drift 2000 |
| M32 | Drift: on at 1 | Magic017Image_Drift 1023 |
| M33 | Leap: at 0x11 | Magic017Image_Leap 951 |
| M34 | Leap: offset x -0x10000 | Magic017Image_Leap 470 |
| M35 | Leap: offset sides swapped | Magic017Image_Leap 470 |
| M36 | Leap: y from the source's +0x34 | Magic017Image_Leap 470 |
| M37 | Leap: height the source's +0x38 | Magic017Image_Leap 470 |
| M38 | Leap: second step 0x800 | Magic017Image_Leap 217 |
| M39 | Leap: sound 0x102 | Magic017Image_Leap 127 |
| M40 | Leap: sound from image 1 | Magic017Image_Leap 236 |
| M41 | Leap: +9 0xF | Magic017Image_Leap 470 |
| M42 | Return: tint + 5 | Magic017Image_Return 1880 |
| M43 | Return: images above 1 free | Magic017Image_Return 111 |
| M44 | Return: its own +0xB down | Magic017Image_Return 340 |
| M45 | Return: +0 &= 0xDE | Magic017Image_Return 63 |
| M46 | Return: +0x5C 1 | Magic017Image_Return 120 |
| M47 | Return: enemies above 3 | Magic017Image_Return 18 |
| M48 | Return: animation 3 | Magic017Image_Return 63 |
| M49 | Return: +9 size + 1 | Magic017Image_Return 120 |
| M50 | Return: x by the y step | Magic017Image_Return 2000 |
| M51 | Strike: while the owner's +0xB is 0 | Magic017Image_Strike 1065 |
| M52 | Strike: no tick | Magic017Image_Strike 850 |
| M53 | Strike: the actor flagged | Magic017Image_Strike 190 |
| M54 | Strike: sound (2, 5) | Magic017Image_Strike 199 |
| M55 | Strike: +9 down before the tick | Magic017Image_Strike 35 |
| M56 | Script: on while running | Magic017Image_Script 2000 |
| M57 | Script: +9 9 | Magic017Image_Script 1328 |
| M58 | Reshow: from actor 2 | Magic017Image_Reshow 78 |
| M59 | Reshow: animation 1 | Magic017Image_Reshow 283 |
| M60 | Reshow: +9 7 | Magic017Image_Reshow 499 |
| M61 | Reshow: the task read before the call | Magic017Image_Reshow 11 |
| M62 | Home: tint - 16 | Magic017Image_Home 2000 |
| M63 | Home: height the owner's +0x38 | Magic017Image_Home 518 |
| M64 | Home: +9 0 | Magic017Image_Home 518 |
| M65 | Done: tint + 7 | Magic017Image_Done 2000 |
| M66 | Done: the owner's +0xB up | Magic017Image_Done 452 |
| M67 | Done: +9 not counted | Magic017Image_Done 2000 |
| M68 | Home: owner x skipped | Magic017Image_Home 462 |
| H1 | shared start: facing from the owner's +9 | Magic017_Start 1661, Magic018Row5_Start 1886, Magic019_Start 1883, Magic018Row53_Start 1821, Magic018Row54_Start 1933 |
| H2 | shared start: y from the owner's +0x3C | Magic017_Start 1769, Magic018Row5_Start 1944, Magic019_Start 1950, Magic018Row53_Start 1886, Magic018Row54_Start 1964 |
| H3 | shared start: +9 1 | Magic017_Start 1685, Magic018Row5_Start 1902, Magic019_Start 1895, Magic018Row53_Start 1817, Magic018Row54_Start 1948 |
| H4 | shared start: animation 0xD | Magic017_Start 2000, Magic018Row5_Start 2000, Magic019_Start 2000, Magic018Row53_Start 2000, Magic018Row54_Start 2000 |
| H5 | shared start: +1 on after the call | Magic017_Start 69, Magic018Row5_Start 79, Magic019_Start 80, Magic018Row53_Start 80, Magic018Row54_Start 74 |
| H6 | image: the actor read before the task | Magic017_Start 392, Magic018Row5_Start 79, Magic019_Start 80, Magic018Row53_Start 202, Magic018Row54_Start 82, Magic018Row68_Start 73 |
| H7 | image: 0x7C bytes copied | Magic017_Start 2000, Magic018Row5_Start 2000, Magic019_Start 2000, Magic018Row53_Start 2000, Magic018Row54_Start 2000, Magic018Row68_Start 2000 |
| H8 | image: party below 2 | Magic017_Start 347, Magic018Row5_Start 291, Magic019_Start 296, Magic018Row53_Start 331, Magic018Row54_Start 292, Magic018Row68_Start 285 |
| H9 | image: enemies by index - 2 | Magic017_Start 1188, Magic018Row5_Start 1120, Magic019_Start 1120, Magic018Row53_Start 1130, Magic018Row54_Start 1088, Magic018Row68_Start 1094 |
| H10 | image 008: +6 2 | Magic018Row5_Start 2000, Magic019_Start 2000, Magic018Row54_Start 2000, Magic018Row68_Start 2000 |
| H11 | image 008: +5 0x20 | Magic018Row5_Start 2000, Magic019_Start 2000, Magic018Row54_Start 2000, Magic018Row68_Start 2000 |
| H12 | image 008: +2 1 | Magic018Row5_Start 2000, Magic019_Start 1999, Magic018Row54_Start 2000, Magic018Row68_Start 2000 |
| H13 | image 008: +0x80 the owner | Magic018Row5_Start 1736, Magic019_Start 1753, Magic018Row54_Start 1733, Magic018Row68_Start 1733 |
| H14 | CLUT: +0x28 the owner's +0x24 | Magic017_Start 1994, Magic019_Start 1987, Magic018Row53_Start 1996 |
| H15 | CLUT: the owner's STP bits | Magic017_Start 1745, Magic019_Start 1754, Magic018Row53_Start 1735 |
| H16 | CLUT: the task's row copied | Magic017_Start 1751, Magic019_Start 1753, Magic018Row53_Start 1735 |
| H17 | tint: +0x5F not moved | Magic017Image_Drift 2000, Magic017Image_Return 1880, Magic017Image_Home 2000, Magic017Image_Done 2000 |
| H18 | shown: +0x2B 0 | Magic017Image_Appear 515, Magic017Image_Reshow 499 |
| H19 | shown: +0x5E 0xC1 | Magic017Image_Appear 515, Magic017Image_Reshow 499 |
| H20 | shown: +0 bit 0x10 | Magic017Image_Appear 392, Magic017Image_Reshow 381 |
| H21 | CLUT: +0x24 the owner's +0x27 | Magic017_Start 1984, Magic019_Start 1995, Magic018Row53_Start 1994 |
| R1 | Row5_Task: entries 1/2 swapped | Magic018Row5_Task 997 |
| R2 | Row5_Start: animation arg 0 | Magic018Row5_Start 2000 |
| R3 | Row5_Start: image type 2 | Magic018Row5_Start 2000 |
| R4 | Row5_Start: no sound | Magic018Row5_Start 2000 |
| R5 | Row5_Start: owner bits 0x41 | Magic018Row5_Start 1002 |
| R6 | Row5_Wait: at 0xFE | Magic018Row5_Wait 1347 |
| R7 | Row5_Wait: animation 5 | Magic018Row5_Wait 669 |
| R8 | Magic019_Task: entries 1/2 swapped | Magic019_Task 1309 |
| R9 | Magic019_Start: image type 0 | Magic019_Start 2000 |
| R10 | Magic019_Start: entry 31 of the owner | Magic019_Start 1756 |
| R11 | Magic019_Start: no entry 31 | Magic019_Start 2000 |
| R12 | Magic019_End: at 0 | Magic019_End 681 |
| R13 | Magic019_End: no CLUT restore | Magic019_End 677 |
| R14 | Magic019_End: done bits 0xC | Magic019_End 318 |
| R15 | Row53_Task: entries 2/3 swapped | Magic018Row53_Task 1015 |
| R16 | Row53_Start: two images | Magic018Row53_Start 2000 |
| R17 | Row53_Start: type 2 | Magic018Row53_Start 2000 |
| R18 | Row53_Start: +0xB 2 - i | Magic018Row53_Start 2000 |
| R19 | Row53_Start: the task's +0xB down | Magic018Row53_Start 1976 |
| R20 | Row53_Start: owner bit 0x80 | Magic018Row53_Start 1468 |
| R21 | Row53_Start: +0x80 + 4 | Magic018Row53_Start 2000 |
| R22 | Row53_Wait: at 0xFF | Magic018Row53_Wait 662 |
| R23 | Row53_Wait: owner bit kept | Magic018Row53_Wait 313 |
| R24 | Row54_Task: entries 0/1 swapped | Magic018Row54_Task 996 |
| R25 | Row54_Start: type 3 | Magic018Row54_Start 2000 |
| R26 | Row54_Start: animation arg 1 | Magic018Row54_Start 2000 |
| R27 | Row54_Start: no owner bit | Magic018Row54_Start 993 |
| R28 | Row68_Task: entries 1/2 swapped | Magic018Row68_Task 1341 |
| R29 | Row68_Start: +0xB 1 | Magic018Row68_Start 1938 |
| R30 | Row68_Start: animation arg 0 | Magic018Row68_Start 2000 |
| R31 | Row68_Start: type 4 | Magic018Row68_Start 2000 |
| R32 | Row68_Start: owner bits 0x60 | Magic018Row68_Start 1022 |
| R33 | Row68_Start: +1 on after the call | Magic018Row68_Start 78 |
| R34 | WaitOneImage: at 0xFE | Magic018_WaitOneImage 1299 |
| R35 | WaitOneImage: the owner after the call | Magic018_WaitOneImage 16 |
| R36 | WaitOneImage: animation (4, 2) | Magic018_WaitOneImage 671 |
| R37 | Magic019_Start: sound added | Magic019_Start 2000 |

## 7. What nothing reached

No recorded route casts any of these rows (queue §5); the live check is the
owner casting them, with a save that has them or DIV-0045's cheat. By
reading, things to look for:

- row 43: six semi-transparent copies of the caster moving out in pairs,
  jumping to the target side, coming back; one of them striking; the caster's
  colours in the fx CLUT row;
- rows 5, 6, 53, 54, 68: one copy (three for row 53) of the caster driven by
  MAGIC008's code (S06), the caster in animation 0xC until it ends.

## 8. Latent defects (Capcom's, kept)

Numbered D89, D90, D96 and D99 in [`known-defects.md`](known-defects.md).

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the six stack tables by +1, the
  image's ten-entry table by +2, the image's one-entry `.data` table (an index
  past it runs MAGIC008's table). Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in all six
  starts: slot 255 is `0x93A000 + 255 x 0x84 = 0x9423FC`, and the 0x80-byte
  copy writes there - past the image's end (`0x93F000`, group S31's
  reading), an access violation, in ours as in the original (the same
  address is written).
- **The actor's record is indexed by the actor byte - 3**, unchecked above
  10 (read-only: a copy of whatever lies there).
- **Rows 5, 6, 54 and 68 wait for +0xB to reach 0xFF, row 53 and row 43 for
  0**, with no limit: if the images never count it down (a slot that was
  never created, above), the task waits forever. Whether MAGIC008's code
  always counts it was not read here.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets the 29 functions' extents (each to its last
instruction) under a `group S05` comment; none was listed before, and the
host line `004A1050 1140` (MAGIC015's last, as S04 re-listed it at `189`)
ran over the whole range.

**An incident, 2026-09-26 19:35:** this group's first script to add those
lines opened the file for writing before a failing `write` and left it empty
for about three minutes. It was rebuilt at 19:38 from the 19:32 raw snapshot
`consolidate_entries.py` had kept (`entries_logic_0926_1932_raw.txt`),
consolidated the same way, plus the functions of the groups that had added
lines after it - S03, S04, S08 (read from their worktrees' `symbols.toml`:
pc and the evidence's byte count, each equal to `magic_funcs.tsv`'s size),
S07 (whose own script was written in the same minute and may have run before
the truncation; its comment is left for it to add) and this group's.
Duplicate starts keep the smaller extent, as the consolidation would; every
start of the 19:32 snapshot is present. What the rebuild cannot restore: a
hand edit made between 19:32 and 19:35 by anyone else. The coordinator's
consolidation should be run over it as usual.
