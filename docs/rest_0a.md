# Group R0A: the party sets' field actions' seven shared helpers

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md) section 3), the
stage-A group, on the round branch's tip `dafd4a3` (the docs' `aed35f8` is
the same code before the history rewrite). **7 functions ours**
(`src/game/rest_0a.cpp`, declarations in `src/game/rest_0a.h`, shadow name
`rest_0a`): the cut's seven rows for R0A (`analysis/round14_cut.tsv`), each
read to its last instruction with capstone and fuzzed through the scenario
harness in field mode ([`scenario_harness.md`](scenario_harness.md) section
7), used unchanged: 140,000 rounds, 0 mismatches. 53 controls planted one at
a time: 52 refused, 1 equivalent mutant not refused, its near variant refused
(section 6). No recorded route enters any of the seven (section 9).

| Function | Entry | Bytes | Answers | Call sites (`E8`) |
|---|---|--:|---|--:|
| `PartyAction_TargetAhead` | `0x51C390` | 0x9F | al: 0 / 1 | 36 |
| `PartyAction_MemberBeyondEffect` | `0x51C6A0` | 0x92 | al: 0 / 1 | 8 |
| `PartyAction_MemberOnEffect` | `0x51DD70` | 0x6A | al: 0 / 1 | 8 |
| `PartyAction_Kind30Ahead` | `0x521510` | 0xAA | al: an index 0..19 or 0xFF | 8 |
| `PartyAction_BlockedAhead` | `0x522560` | 0xED | al: 0 / 1 | 18 |
| `Effect_SpawnAtCellHigh` | `0x522FB0` | 0x76 | nothing any caller reads | 45 |
| `PartyAction_SideProbes` | `0x524DA0` | 0xAF | nothing any caller reads | 14 |

All 137 sites but two lie in wave one's groups R1A..R1F; the other two are
ours already (`PartyAction5_Form0Begin` `0x51E930` calls `0x51C390` twice,
through `field_hidden`'s pointer table). No PSX twin is paired for any of the
seven (the cut's name column is empty, `how` none or neighbour), so every
name is from what the PC code does. The cut's classes ("field core", one
"PLP" by neighbour) were proposals; all seven are helpers called by `E8` from
the party sets' state handlers - none a state, a case or a shared tail - and
all seven are taken.

## 1. What each function does

"Sprite_Current" is the sprite whose state handler is running: its direction
byte `+8`, its 16.16 position `+0x34` (x) / `+0x38` (z), its height word
`+0x3E`, a byte `+0x2B`. "A step" is a row of `Field_DirectionSteps`
(`0x6697B0`, 8 rows of two longs, half a cell per axis in the exe), read **in
place with the direction byte unmasked** (`shl eax, 3` on the zero-extended
byte), as every reader of that table in the originals does. "The members" are
`ObjTrio`'s records 1 .. `Field_MemberCount` - 1 (`0x802E8C` on by 0x14C; the
leader, record 0, is never tested), the count re-read for each.

- **`PartyAction_TargetAhead` `0x51C390`** - something to act on a cell
  ahead. The point two steps ahead, stored before any call. 1 when
  `Sprite_ObjectAt(point, 0)` is not 0xFF, or `AreaMap_ByteAt` is **0xF2** at
  the point's cell, at the cell one on in x (only when x's low word is not 0),
  or at the cell one on in z (only when z's is not 0); else 0. Every call is
  made whatever the earlier ones answered.
- **`PartyAction_MemberBeyondEffect` `0x51C6A0`** `(index)` - the point two
  steps beyond effect object `index` (its `+0x34` / `+0x38`), in
  **Sprite_Current's** direction, read once; 1 when a member is in reach
  (`Sprite_PointInReach(point, the record's +0x3E re-read, margin 1,
  member)`), else 0.
- **`PartyAction_MemberOnEffect` `0x51DD70`** `(index)` - the same test at
  the record's own position with its height word + 0x200 (a 16-bit add), all
  three re-read for each member; with the count 1 or less the record is not
  read at all and the answer is 0.
- **`PartyAction_Kind30Ahead` `0x521510`** - 0xFF unless `Field_State +0x89`
  is 2 and `+0x138` bit 0 is clear. The point one step ahead, read once; then
  of `Effect_Objects`' 20 records, those in use (`+0`) of kind 0x30 (`+5`)
  that `Sprite_PointInReach(point, Sprite_Current's +0x3E, 1, record)` answers
  non-zero for: the index of the first whose x equals Sprite_Current's x, or
  whose z equals its z (Sprite_Current re-read after the call). A record in
  reach but not lined up is passed over. 0xFF at the end.
- **`PartyAction_BlockedAhead` `0x522560`** - the way a cell ahead blocked.
  The point as `PartyAction_TargetAhead`'s; 1 when `Sprite_ObjectAt(point,
  0)` is not 0xFF, `Field_EffectAhead` is not 0xFF, or `AreaMap_ByteAt` is
  **0xF0, 0xF1, 0xF4, 0xF6 or 0xF7** at the cell and the fraction cells as
  above; every call made.
- **`Effect_SpawnAtCellHigh` `0x522FB0`** `(state, x, z)` - an effect object
  of kind 0x34 on the cell (x, z). It is `Effect_SpawnAtCell` `0x524870`'s code
  (ours, `scena_se.cpp`) with two constants changed: with `Effect_FindFree`'s
  slot not 0xFF, `+0` = 1, `+5` = 0x34, `+1` = the state's byte, `+0x34` /
  `+0x38` = the words x and z sign-extended and shifted to 16.16, **`+0x3E` =
  `AreaMap_Elevation` there + 0x200** (`Effect_SpawnAtCell`: + 0x100), **`+0xB`
  = 1** (there: 0). x is read back from the record for the elevation call.
- **`PartyAction_SideProbes` `0x524DA0`** - `Sprite_Current +0x2B` = 1; then
  for direction 3 and then 5 (the rows at `0x6697C8` and `0x6697D8`, read by
  address; the direction pushed as an immediate): the point one step that way
  from Sprite_Current (re-read); when `MapView_SlopeAt` there is steep
  (`DamageScratch`'s flag byte set and the slope's low word, signed, above
  0x40) and `MapView_GroundAt` there is above the sprite's height word (s16
  against s16), `+0x2B` = 0. It is the probe `PartyAction5_Form0Begin` makes
  inline (`field_hidden.cpp`, `ProbeSide`).

What the cell codes, kind 0x30 and kind 0x34 are in play is not read here;
the names say what the code tests, not what the player sees.

## 2. Calling convention, arguments, answers

All seven are `cdecl`; every call out is relative (`E8`) and leaves the
function (`band_rows.py --clones`: 4, 1, 1, 1, 5, 2, 4 sites), every jump
stays inside, no jump table, no register argument.

| Function | Arguments read (at entry) | eax at the `ret` |
|---|---|---|
| `0x51C390` | none | al 0 / 1; above it `AreaMap_ByteAt`'s leftovers |
| `0x51C6A0` | `[esp+4]`'s low byte (`and esi, 0xFF`) | al 0 / 1 (`xor al, al` / `mov al, 1`) |
| `0x51DD70` | `[esp+4]`'s low byte | al 0 / 1 |
| `0x521510` | none | al the index (`mov al, bl`) or 0xFF (`or al, 0xFF`) |
| `0x522560` | none | al 0 / 1 |
| `0x522FB0` | `[esp+4]`'s byte, `[esp+8]` and `[esp+0xC]` as words (`movsx`) | `AreaMap_Elevation`'s eax + 0x200, or `Effect_FindFree`'s |
| `0x524DA0` | none | the last callee's eax |

**What the callers read** (capstone over the straight-line code after every
`E8` site, scratch `r0a/eaxuse.py`): all 36 sites of `0x51C390`, the 8 of
`0x51C6A0` and of `0x51DD70`, and the 18 of `0x522560` test al first (`test
al, al`); the 8 of `0x521510` compare al with 0xFF; the 45 of `0x522FB0` call
on before touching eax; the 14 of `0x524DA0` write eax first. So ours answers
`unsigned char` for the five (the fuzz compares al, `ret_mask` 0xFF) and
`void` for the two (compared on the state and the log only).

**The arguments the callers push.** `0x51DD70` and `0x51C6A0` are handed the
dword `[esp+8]` of their caller into whose low byte `PartyAction_Kind30Ahead`'s
al was stored (the upper three bytes the caller's ecx); all 8 sites call
`0x521510`, skip on 0xFF, then call `0x51DD70` and `0x51C6A0` with that dword,
so the index is always 0..19. `0x522FB0`'s 45 sites push **four** dwords; the
fourth is never read.

**Uninitialised stack the callees never read.** `0x51C390` and `0x522560`
store the point on their own stack and push its high words to
`AreaMap_ByteAt` as dwords read 2 bytes into each (`[esp+0x12]`,
`[esp+0x1A]`; `[esp+0x16]`, `[esp+0x1E]`): the upper half of each is a stack
word the function never wrote. `AreaMap_ByteAt` sign-extends its arguments
from 16 bits (its `symbols.toml` entry), so only the cells reach it; ours
passes the cells as shorts and the fuzz logs 16 bits of each. The cell one on
is the dword + 1: its low word wraps at 0xFFFF as ours does (`CellPlusOne`).

## 3. The fuzz (`rest_0a_fuzz.cpp`)

`scenario_harness::Run` with the seven as `Shape::kCall` clones (field mode,
`g.field`), **20,000 rounds per function**. `BOF3X_R0A_ONLY=<name>` runs the
clones whose name contains it (the controls).

**The callees.** All eight are the group's own stand-ins, registered before
the harness's standard rows (`Sprite_ObjectAt`, `AreaMap_ByteAt`,
`Effect_FindFree`, `AreaMap_Elevation`, `MapView_SlopeAt`, `MapView_GroundAt`
have standard rows; the group's listing stands):

| Callee | Masks | Answers |
|---|---|---|
| `Sprite_ObjectAt` (x, y, margin) | whole, whole, whole | al 0xFF half the time, else 0..0x21 or any byte; garbage above |
| `AreaMap_ByteAt` (x, y) | 16 bits each | al one of 0xF0..0xF8, 0xEF, 0, 0xFF, 0x72, or any byte; garbage above |
| `Sprite_PointInReach` (x, y, z, margin, object) | whole, whole, 16 bits, whole, whole (the record's address, the same on both passes) | `kFlag`: al 0 a third of the time |
| `Field_EffectAhead` () | - | al 0xFF half the time, else 0..19 or any byte |
| `Effect_FindFree` () | - | al 0xFF a third of the time, else a slot 0..19 (what the real one hands out) |
| `AreaMap_Elevation` (x, y) | whole, whole | garbage |
| `MapView_SlopeAt` (x, y, direction) | whole each (the direction is pushed as an immediate) | `DamageScratch`'s flag 0 a third of the time, else 1 or any non-zero byte; ax 0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x140, 0xFFC0, 0x1000 or random; garbage above |
| `MapView_GroundAt` (x, z) | whole, whole | ax the sprite's height word + 0, +-1, +-2, 0x8000, 0x7FFF, or random; garbage above |

**The state.** Field mode's standard regions (which hold `Sprite_Current` and
the sprite records, `ObjTrio`, `Field_State`, `Field_MemberCount`,
`Effect_Objects` and `DamageScratch`'s byte) and the group's one:
`Field_DirectionSteps` `0x6697B0`, 0x40 bytes (`.data`, writable; the harness
restores every region after the run). 23,660 bytes, 39 regions.

**The seeds** (after the harness's random fill; `Seed(k)`):

- `Field_DirectionSteps`: half the time each dword 0, 0x8000 or -0x8000 (the
  exe's shape), else 0, +-0x8000, +-0x10000, 0x4000, 1, -1, the dword limits
  or random.
- Sprite_Current's direction 0..7 two times in three, else 8, 9, 15, 0x80,
  0xFF or random (rows past the table: read in place, the same on both
  passes); its x and z a cell of 0, 1, 2, small, 0x7FFF, 0x8000, 0xFFFF,
  0xFFFE or random with a fraction 0 (three times in eight), 0x8000, 0x4000,
  1, 0xFFFF or random; its height word at 0, 1, -1, the s16 limits, around
  0x7E00 and 0xFE00 (where + 0x200 crosses the sign or wraps), small or random.
- `0x51C6A0` / `0x51DD70`: `Field_MemberCount` 0, 1, 2, 3, 4 (and one round in
  16 any byte: members past `ObjTrio`'s three, as the original computes their
  addresses; the stand-in only logs them); every effect record's position and
  height as Sprite_Current's.
- `0x521510`: `Field_State +0x89` 2 four times in eight, else 0, 1, 3 or
  random; `+0x138` 0 three times in eight, else 2, 0xFE, 1, 3 or random;
  every record's `+0` 0 a fifth of the time, `+5` 0x30 three times in seven,
  else 0x17, 0x31, 0x2F or random; its x and z each Sprite_Current's about
  one time in three, else a coordinate.
- `0x524DA0`: `+0x2B` 0, 1, 2 or random (it must be written).
- The arguments (`Args`): the effect index's low byte 0..19 under random upper
  bytes; `0x522FB0`'s x and z low words 0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x40 or
  random under random upper halves, its state and fourth dword random.

**The disturbance** (the group's case of the harness's, from its hash only):
Sprite_Current's direction, x or z, height, `+0x2B`; `Field_MemberCount` (0..4,
two cases); the round's effect record's (the index `Args` drew; three cases)
or a random record's x, z or height; a random record's in-use byte or kind; a
dword of `Field_DirectionSteps`; `Field_State +0x89` / `+0x138`. The harness's
own moves `Sprite_Current` among the sprite records. **A pitfall paid for**: a
disturbance must draw only from its hash - the first version picked the field
with `Pick`, which draws `Next()`, the seed's stream, and the two passes
diverged (862 rounds) on bytes neither side wrote.

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_0a`, exit 0): 140,000 rounds over 7 functions, 303,357
calls to the stand-ins, **0 mismatches**; coverage `Sprite_ObjectAt` 40,000,
`AreaMap_ByteAt` 102,079, `Sprite_PointInReach` 57,497, `Field_EffectAhead`
20,000, `Effect_FindFree` 20,000, `AreaMap_Elevation` 13,408,
`MapView_SlopeAt` 40,000, `MapView_GroundAt` 10,373. `BOF3X_SHADOW='*'` after
the rebinding (2026-10-04, this worktree): exit 0, `inject: 8655 ours, 0 left
original`, 1,018 self-test lines of 0 mismatches and none other (among them
`rest_0a` and `field_hidden`, whose `0x51C390` constant was rebound); the same
with `BOF3X_WIDE=1`: exit 0, 1,018, 8,655 ours. Each passed on its first run.

## 4. Divergence

None. The seven are faithful; `DIVERGENCE.md`, `cheats.cpp` and
`widescreen.cpp` name no byte in `0x51C390..0x524E4E` (grep, 2026-10-04). The
one behaviour ours does not reproduce is the index fault of section 5, which no
caller reaches; ours aborts there with a message (the project's rule, not a
choice of behaviour).

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **The effect index is not bounded** (`0x51C6A0`, `0x51DD70`): the argument's
  low byte times 0x80 from `Effect_Objects`, any byte - past the 20 records
  (`0x7E1BE0`..) the originals read whatever follows. **Ours aborts** with a
  `Fatal` naming the index and the address. Ordinary play does not reach it:
  all 16 call sites hand `PartyAction_Kind30Ahead`'s answer after testing it
  against 0xFF, and that answer is 0..19 (section 2).
- **The direction is not masked** (all five that step): a direction byte above
  7 reads `Field_DirectionSteps` past its 8 rows, into the `.data` after it.
  Reproduced (read in place), not aborted: the read does not fault, and whether
  play ever holds such a byte in `+8` is not established - the same choice
  `Field_EffectAhead` and `PartyAction5_Form0Begin` made before. The seeds
  cover it.
- **The member loop is bounded only by `Field_MemberCount`**: a count above 3
  hands `Sprite_PointInReach` addresses past `ObjTrio`'s three records.
  Reproduced (the address computed as the original computes it); whether the
  count ever exceeds 3 is not established here.
- **`Effect_FindFree`'s slot is used unchecked** (`0x522FB0`), as
  `Effect_SpawnAtCell` uses it: the real one answers 0..19 or 0xFF only.
- **Ranges, not defects**: the cell one on wraps at 16 bits; the height + 0x200
  of `0x51DD70` wraps at 16 bits; `0x522FB0`'s x and z are 16-bit words, so
  a cell above 0x7FFF lands negative. Reproduced; the seeds cross each.

## 6. Controls

`r0a/controls.py` (scratch): each plant replaces a string that occurs exactly
once in `rest_0a.cpp`, rebuilds, runs the self-test on the one clone it
touches (`BOF3X_R0A_ONLY`), restores the file and rebuilds. **53 planted: 52
refused, 1 not refused - an equivalent mutant with its near variant
refused.** The count is the rounds that mismatched of 20,000.

| # | Function | Plant | Refused |
|---|---|---|--:|
| C01 | TargetAhead | `Sprite_ObjectAt` margin 1 | 20,000 |
| C02 | TargetAhead | the target code 0xF3 | 2,912 |
| C03 | TargetAhead | x's fraction tested `& 0xFFFE` | 1,570 |
| C04 | TargetAhead | the z probe one on in x too | 15,529 |
| C05 | TargetAhead | one step ahead, not two | 17,060 |
| C06 | TargetAhead | the direction masked to 7 | 6,598 |
| C07 | TargetAhead | the cell from `>> 15` | 19,870 |
| C08 | TargetAhead | an object found returns at once (the map calls skipped) | 9,937 |
| C09 | MemberBeyondEffect | the members from record 0 | 17,561 |
| C10 | MemberBeyondEffect | margin 0 | 15,232 |
| C11 | MemberBeyondEffect | x from the record's z | 15,013 |
| C12 | MemberBeyondEffect | the height read once, before the loop | 16 (by the disturbance) |
| C13 | MemberBeyondEffect | the count read once | 21 (by the disturbance) |
| C14 | MemberBeyondEffect | the index `% 20`, not the low byte | 12,218 |
| C15 | MemberBeyondEffect | the members' stride from record 0 | 15,232 |
| C16 | MemberBeyondEffect | the direction the record's, not Sprite_Current's | 15,096 |
| C17 | MemberOnEffect | + 0x100 | 15,232 |
| C18 | MemberOnEffect | the height saturated, not wrapped | 3,478 |
| C19 | MemberOnEffect | x and z swapped | 15,013 |
| C20 | MemberOnEffect | the height read once, before the loop | 16 (by the disturbance) |
| C21 | MemberOnEffect | the loop to the count inclusive | 4,678 |
| C22 | Kind30Ahead | `+0x138` tested `& 3` | 2,740 |
| C23 | Kind30Ahead | `+0x89` below 2 refused, not other than 2 | 3,511 |
| C24 | Kind30Ahead | the in-use byte not tested | 2,857 |
| C25 | Kind30Ahead | kind 0x31 | 6,811 |
| C26 | Kind30Ahead | Sprite_Current's x not re-read after the call | 379 (by the disturbance) |
| C27 | Kind30Ahead | z compared with Sprite_Current's x | 2,397 |
| C28 | Kind30Ahead | two steps ahead, not one | 5,761 |
| C29 | Kind30Ahead | the height from the first Sprite_Current read | 490 (by the disturbance) |
| C30 | Kind30Ahead | margin 0 | 6,810 |
| C31 | Kind30Ahead | a record in reach but not lined up ends the search | 3,105 |
| C32 | BlockedAhead | 0xF6 not blocking | 427 |
| C33 | BlockedAhead | 0xF2 blocking too | 410 |
| C34 | BlockedAhead | `Field_EffectAhead` tested against 0 | 1,930 |
| C35 | BlockedAhead | `Field_EffectAhead` called before `Sprite_ObjectAt` | 20,000 |
| C36 | BlockedAhead | the z probe on x's fraction | 6,232 |
| C37 | SpawnAtCellHigh | 0x100 above the ground | 13,410 |
| C38 | SpawnAtCellHigh | `+0xB` 0 | 13,410 |
| C39 | SpawnAtCellHigh | kind 0x35 | 13,409 |
| C40 | SpawnAtCellHigh | x zero-extended before the shift | **not refused**: equivalent - the sign extension's upper bits are shifted out by `<< 16`, so both give the same dword. Near variant C41 refused |
| C41 | SpawnAtCellHigh | x `<< 15` (C40's near variant) | 11,476 |
| C42 | SpawnAtCellHigh | the elevation's arguments swapped | 11,761 |
| C43 | SpawnAtCellHigh | the state's second byte | 13,366 |
| C44 | SpawnAtCellHigh | slot 0 taken as none | 709 |
| C45 | SideProbes | `+0x2B` = 2 | 16,056 |
| C46 | SideProbes | direction 7 for 5 | 20,000 |
| C47 | SideProbes | steep from 0x40, not above it | 1,860 |
| C48 | SideProbes | the slope compared whole | 11,235 |
| C49 | SideProbes | cleared when level too (`<=`) | 1,094 |
| C50 | SideProbes | Sprite_Current not re-read after the calls | 234 (by the disturbance) |
| C51 | SideProbes | the scratch flag's second byte | 4,916 |
| C52 | SideProbes | the direction pushed with a high bit | 20,000 |
| C53 | SideProbes | the ground compared whole | 4,470 |

The first run left C12 and C20 unrefused: the disturbance picked one of the
20 records at random, so it moved the one being read one time in sixty. It
now moves the round's record in three of its thirteen cases (`g_index`, set by
`Args`), and both are refused - but they and C13 stay the thinnest (16 to 21
rounds): they need a loop of two or more members and the disturbance to land
between two of its calls.

## 7. Calls across groups

Every caller in the cut calls these by name from now on. `band_rows.py
--byte-tables --group R0A` (through the scratch `band14.py`; the tool itself
stops with `settle: no fixpoint in 8 rounds` on this cut, as the round doc
says) lists who reaches them: R1A..R1F and ours `PartyAction5_Form0Begin`.
For wave one's groups:

- **Include `game/rest_0a.h`** and call `SH_CALL(PartyAction_...)` /
  `SH_CALL(Effect_SpawnAtCellHigh)`. The answers are `unsigned char`; compare
  al only. The two `void`s answer nothing a caller reads.
- **A stand-in** for them in a group's fuzz: the five answer al (0 / 1, or for
  `PartyAction_Kind30Ahead` 0..19 or 0xFF - keep a stand-in's index in 0..19,
  since `0x51C6A0` / `0x51DD70` abort past it if the group calls the real ones);
  `Effect_SpawnAtCellHigh` writes one `Effect_Objects` record (or nothing);
  `PartyAction_SideProbes` writes `Sprite_Current +0x2B`. None takes a pointer.
- **`Effect_SpawnAtCellHigh` reads three arguments**; the callers push four.
  A typed call passes three.
- **`0x51D690`, `0x51E870`, `0x523050`, `0x523ED0` and others begin with a
  call to `0x524DA0`** (the band tool lists the site at the function's own
  start): `PartyAction_SideProbes` is their first act, not a tail.

## 8. The rebinding

`band_rows.py --refs --group R0A` and `grep -rn -i -E
"0x(51C390|51C6A0|51DD70|521510|522560|522FB0|524DA0)" src/game`: one
function referenced, `0x51C390`, in `field_hidden` (round eight).

| File | Change |
|---|---|
| `field_hidden_callees.h` | `kTargetAhead = bof3::addr::PartyAction_TargetAhead` (the value unchanged); its two comments now say it is R0A's, not nobody's |

Ours in `field_hidden.cpp` still calls through `g.target_ahead`, which holds
that constant, so its fuzz keys stand. **Left raw, on purpose**:
`field_hidden_fuzz.cpp`'s `case 0x51C390` in `StubFor` and the
`{0x14, 0x51C390}`, `{0x2E, 0x51C390}` rows of `kBeginCalls` (the
disassembly's targets, as round thirteen's EGT left its `CallSite` tables);
`field_hidden.cpp`'s comment and `docs/field_hidden.md`'s "nobody's" list,
which describe that round. **No harness or fuzz row listed any of the seven
as THEIRS** (`scenario_harness.cpp`, `boss_harness.cpp`, the
`*_callees.h` files: grep), so none broke. `scenario_harness.*` holds no raw
reference to the seven.

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract,
shop, worldmap, combat) are empty for all seven. No first-call or counted
trace under `analysis/calltrace` (`reach_*`, `hash_*`, `ab*`) names any of
them, though `entries_logic.txt` has listed all seven since the earlier
rounds - so no recorded route enters them, and all seven are fuzz only. The
coordinator's frame hash covers whatever a route reaches after the merge.

## 10. For `analysis/calltrace/entries_logic.txt`

The main checkout's file already holds all seven, each with its **host's**
extent - the function and the hidden starts after it (`0051C390 306`,
`0051C6A0 3BB`, `0051DD70 43B`, `00521510 3AB`, `00522560 4BB`, `00522FB0
3DB`, `00524DA0 3AB`) - which wave one's groups own. Nothing appended: a
second line for the same entry with the shorter extent read here (0x9F, 0x92,
0x6A, 0xAA, 0xED, 0x76, 0xAF) would duplicate the address and drop the
hidden starts' coverage; whoever owns the file after wave one splits them.
