# Group R1C: party sets 6..9's field actions, `0x51F210..0x520E08`

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, group
R1C, from the round branch's tip `ba2c3c3` (branch `phase-3/round14-r1c`).
**51 functions ours** (`src/game/rest_1c.cpp`, shadow name `rest_1c`; the 50
rows the cut `analysis/round14_cut.tsv` gives R1C and `0x51FA30`, code in the
band no list had), each read with capstone to its last instruction and fuzzed
through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
204,000 rounds, 0 mismatches. 101 controls planted one at a time: 97 refused by a count, 3 refused only by a crash (re-planted inside the tables and refused by a count), 1 equivalent mutant not refused, its near variant refused (section 6). No recorded route enters any of the
51 (section 8): fuzz only.

The band is **one thing**: the field actions of party sets 6, 7, 8 and part of
9 - what `Field_FormActions` (`0x660A44`, state 6 of the leader) and
`Field_ActionBySet` (`0x6609D0`, state 10) reach for those sets, through a
dispatcher by the form (the u16 `Sprite_Current +0x2C`), a form's state table
(by `+2`) and, for two forms, a state's step table (by `+3`). The cut's
classes ("field core" for all, three `hypothesis`) were proposals; every row
is a function, none a case or a shared tail. What a set, a form or an action
is in play is not read here: the names say where the code sits in the tables
and what it tests.

## 1. What each function does

"Sprite_Current" is the leader or member whose state handler runs: its
direction `+8`, its 16.16 position `+0x34` / `+0x38`, its height word `+0x3E`,
the form word `+0x2C`, the state `+2`, the step `+3`, the counters `+9`,
`+0xA`, the bytes `+6`, `+7`, `+0xB`, `+0x2B`. "A step" is a row of
`Field_DirectionSteps` (`0x6697B0`, half a cell an axis), read in place with
the direction unmasked, as every reader of it does. "The turn" is `(+8 - 1) /
2` as a signed division (`cdq; sub; sar`, so direction 0 gives 0). "Marked"
is bit 0 of `+0x80` of the object `Sprite_ObjectAt` found (`Sprite_Objects`
0..0x1D, `Sprite_ObjectsExtra` 0x1E..0x21).

| Function | Entry | Bytes | What it is |
|---|---|--:|---|
| 28 dispatchers | below | 0x12 / 0x13 | `jmp [table + index * 4]`, the index unchecked (section 3) |
| `PartyAction6_Form0Begin` | `0x51F210` | 0x1B1 | `PartyAction5_Form0Begin`'s code (section 2) |
| `PartyAction6_Form0Resolve` | `0x51F3D0` | 0xDB | `PartyAction5_Form0Resolve`'s code, set 6's pickup |
| `PartyAction7_Form2Resolve` | `0x51FF20` | 0xDB | the same, set 7's pickup |
| `PartyAction8_Form2Resolve` | `0x5205E0` | 0xDB | the same, set 8's pickup |
| `PartyAction6_CellPickup` | `0x51F4B0` | 0x11F | `Field_CellPickup`'s code |
| `PartyAction7_CellPickup` | `0x520000` | 0x11F | the same, the zenny's tenfold a twentyfold |
| `PartyAction8_CellPickup` | `0x5206C0` | 0x11F | the same as set 7's |
| `PartyAction7_Form2Begin` | `0x51FD40` | 0x1D4 | Form0Begin with the steep test on the ground's rise |
| `PartyAction8_Form2Begin` | `0x520400` | 0x1D4 | the same |
| `PartyAction7_Form0Begin` | `0x51FAF0` | 0x166 | the jump at a lined-up kind-0x30 effect object |
| `PartyAction8_Form0Begin` | `0x5201A0` | 0x166 | the same |
| `PartyAction9_Form0Begin` | `0x5208D0` | 0x166 | the same |
| `PartyAction6_Form2Begin` | `0x51F670` | 0x87 | the turn from a blocked way, the side probes |
| `PartyAction9_Form1Begin` | `0x520AA0` | 0x87 | the same |
| `PartyAction6_Form2Resolve` | `0x51F700` | 0x14C | the strike: an effect object ahead, or an object and the cells |
| `PartyAction9_Form1Resolve` | `0x520B30` | 0x14C | the same, set 9's cell hit |
| `PartyAction6_CellHit` | `0x51F880` | 0x188 | what a struck cell does |
| `PartyAction9_CellHit` | `0x520C80` | 0x188 | the same |
| `PartyAction6_Form2Probe` | `0x51FA30` | 0x35 | the side probes, an animation, a timer |
| `PartyAction_TickThenFace` | `0x51F850` | 0x23 | a script tick, then face (9 cells) |
| `PartyAction_TurnToSide` | `0x520840` | 0x70 | turn toward side 3 or 5 (34 cells) |
| `PartyAction_TurnStep` | `0x51FC80` | 0x7E | its steps (31 cells) |
| `PartyAction_WaitEffectEnd` | `0x520350` | 0x63 | a script tick until an effect object ends (7 cells) |

- **Form0Begin** (`0x51F210`): an even direction is turned back one eighth
  (`- 1`, `& 7`); when `PartyAction_TargetAhead` finds nothing that way, two
  on, and when nothing there either, back two. Then `MapView_SlopeAt` one step
  ahead in `+8`: steep (`AreaMap_Slope`'s flag byte `0x903850` set and the
  slope's low word above 0x40, signed) - `Sprite_EnsureAnimation(the turn +
  0x46)` and `+2` one on; else `+0x2B` = 1, the side probes in directions 3 and
  5 (each: the point one row that way, rows `0x6697C8` / `0x6697D8` read by
  address; steep there and `MapView_GroundAt` above the height word, `+0x2B` =
  0), `Sound_PlayEffect(u16 +0x2C + 0x100)`, `Sprite_EnsureAnimation(the turn
  + 0x42)`, `+0xA` = 5. Then `+0xB` = 0 and `+2` one on (a steep slope moves
  `+2` by two, to `PartyAction_Finish`).
- **Form2Begin** (`0x51FD40`, `0x520400`): the same, but steep is **the ground
  one step ahead above the sprite's height word by more than 0x40** (16-bit,
  signed) with the flag set: `MapView_GroundAt` first (its answer kept in the
  function's own local), the height read after it, then `MapView_SlopeAt`
  there with the direction re-read - its answer dropped, called for the flag.
- **Form0Resolve** (`0x51F3D0`, `0x51FF20`, `0x5205E0`): `+0xA` counted down;
  at 0, the point two steps ahead: the object `Sprite_ObjectAt(point, 0)`
  finds is marked; the set's pickup on the point's cell, then on the cell one
  on in x (when x has a fraction), then in z (when z has one), each only while
  none answered 1; `+2` one on. `Sprite_ScriptTickOnce` every time.
- **CellPickup** (`0x51F4B0` x10, `0x520000` / `0x5206C0` x20): the map cell
  (x, z): `0xF2` with an effect object free - `Effect_SpawnAtCell(0)`, a `Rand`
  nibble of 13..15 finds zenny 2 (5 on 15), ten or twenty times that when
  `Field_InputFlags` has bit 1 or 2 and a second `Rand & 3` is 0 (a byte
  product), `Field_GiveZenny`, `Effect_SpawnAtCell(1)`, then `+0xB` = 1;
  `0xF8` - `Effect_SpawnAtCell(0)`, item 0x56's name into `Text_Records`,
  `Inventory_Add(0, 0x56, 1)`: `Sound_PlayEffect(0x106)` and
  `Msg_OpenSystem(2)`, else `Msg_OpenSystem(3)`; `Field_Request` 2, `+0xB` = 1.
  Both clear the cell and answer 1 (with no object free, the cell is cleared
  and nothing else is done); anything else answers 0.
- **Kind30Begin** (`0x51FAF0`, `0x5201A0`, `0x5208D0`):
  `PartyAction_Kind30Ahead`. With one lined up: when a member is on it or
  beyond it (`PartyAction_MemberOnEffect`, then `_MemberBeyondEffect`),
  `Field_State +0x137` = 0 and no more; else that record's `+0xB` = 1,
  `Sound_PlayEffect(u16 +0x2C + 0x100)`, `Sprite_EnsureAnimation(+8 + 8)`,
  `Field_State +0x128` = 2, `Field_ScriptFlags |= 0x1000` (its high byte
  `0x9039A3 |= 0x10`), `Field_JumpStart`, `+9` one down,
  `Field_LeaderStepTick`, `Field_State +0x137` = 1, `+2` one on. With none:
  unless bit 0 of `Field_State +0x138` is set, the object two steps ahead
  (`Sprite_ObjectAt`, margin **1**) is marked; `Field_State +0x137` = 0.
- **BlockedBegin** (`0x51F670`, `0x520AA0`): the first turn as Form0Begin's,
  by `PartyAction_BlockedAhead`; `PartyAction_SideProbes`;
  `Sprite_EnsureAnimation(the turn + 0x42)`; `+0xB` = 0, `+0xA` = 0xB, `+3` = 1.
- **BlockedResolve** (`0x51F700`, `0x520B30`): `+0xA` counted down when not 0;
  at 0: `Sound_PlayEffect(u16 +0x2C + 0x100)`; an effect object of kind 0x17
  ahead (`Field_EffectAhead`) gets `Sound_PlayEffect(0x10B)`, its `+8` the
  direction, its `+0xA` 1, and the sprite's `+6` its index and `+3` one on;
  with none, the point two steps ahead: the object there marked (with
  `Sound_PlayEffect(0x10B)`), then the set's cell hit on the point's cell and
  the cells one on as Form0Resolve's. Then `+3` one on (an effect object found
  moves it by two). `Sprite_ScriptTickOnce` every time.
- **CellHit** (`0x51F880`, `0x520C80`): the map cell (x, z): `0xF0`, `0xF1`,
  `0xF4` - `Effect_SpawnAtCellHigh(0)`, a second (state 4) when `Rand & 7` is
  above 5, `Sound_PlayEffect(0x10B)`; `0xF6`, `0xF7` -
  `Effect_SpawnAtCellHigh(0)`, `Sound_PlayEffect(0x10B)`, then by `Rand & 0xF`:
  below 7, `Effect_SpawnAtCellHigh(3)`, item 0x29's name into `Text_Records`,
  `Inventory_Add(0, 0x29, 1)` (taken: `Sound_PlayEffect(0x106)`,
  `Msg_OpenSystem(2)`; else `Msg_OpenSystem(3)`), `+0xB` = 2; 7..0xB nothing
  more; above 0xB, `Effect_SpawnAtCellHigh(2)`, `Sprite_FlashClut(0)`,
  `Char_LoseHp(1, Field_State +0x89)`, `Msg_OpenSystem(0xD9)`, `+0xB` = 1;
  `Field_Request` 2 in all three. Each answers 1; anything else 0.
- **Form2Probe** (`0x51FA30`): `PartyAction_SideProbes`,
  `Sprite_EnsureAnimation(the turn + 0x42)`, `+0xA` = 0xB, `+3` = 1.
- **TickThenFace** (`0x51F850`): `Sprite_ScriptTickOnce`; when it answers
  non-zero, `Sprite_SetAnimation(+8)` and `+3` one on.
- **TurnToSide** (`0x520840`): the side the direction is nearer, 3 or 5
  (`|d - 3|` against `|d - 5|` as ints; a tie is 3): `+0xB` =
  `Sprite_TurnSense(that side)` (1 or 0xFF), `+3` = it; `+9` = 2, `+2` one on.
- **TurnStep** (`0x51FC80`): at the side (`+8` = `+3`):
  `Sprite_EnsureAnimation(0x40)` at 3, `(0x41)` at 5, none at another, `+2` one
  on; else `+9` one down, and at 0: `+9` = 2, `+8` = (`+8` + `+0xB`) & 7,
  `Sprite_EnsureAnimation(+8)`.
- **WaitEffect** (`0x520350`): with `+7` set, `Sprite_ScriptTickOnce` and, when
  it answers non-zero, `Sprite_EnsureAnimation(+8)` and `+7` = 0; else
  `Sprite_ScriptTickOnce` and, when it answers non-zero and effect object `+0xB`
  is no longer in use, `+0x2B` = 0 and `Field_State +0x137` = 0 (section 5).

Every one re-reads `Sprite_Current` and `Field_State` after each call, as the
originals do; ours does the same.

## 2. The shapes compared

The band repeats a handful of codes. `r1c/r1dis.py --cmp` (scratch) compares
two functions instruction for instruction with every address inside them made
relative; these are the results.

| Function | Against | Differs only in |
|---|---|---|
| `0x51F210` | `PartyAction5_Form0Begin` `0x51E930` | nothing |
| `0x51F3D0`, `0x51FF20`, `0x5205E0` | `PartyAction5_Form0Resolve` `0x51EAF0` | the three calls: `0x51F4B0`, `0x520000`, `0x5206C0` for `Field_CellPickup` |
| `0x51F4B0` | `Field_CellPickup` `0x51EBD0` | nothing |
| `0x520000`, `0x5206C0` | `Field_CellPickup` | `mov cl, 0x14` for `mov cl, 0xA` (the zenny's multiplier) |
| `0x520400` | `0x51FD40` | nothing |
| `0x5201A0`, `0x5208D0` | `0x51FAF0` | nothing |
| `0x520AA0` | `0x51F670` | nothing |
| `0x520B30` | `0x51F700` | the three calls: `0x520C80` for `0x51F880` |
| `0x520C80` | `0x51F880` | nothing |
| 22 state dispatchers, 3 step dispatchers, 6 form dispatchers | each other | the table, and the byte (`+2`, `+3`) or word (`+0x2C`) |

So ours is one C++ body per shape (`rest_1c.cpp`'s anonymous namespace) with
the set's callee or multiplier passed in, and `field_hidden.cpp`'s three set-5
originals are the reference the first three rows were checked against.

## 3. The tables

28 `.data` tables, each named in `symbols.toml` (`[[data]]`, `unsigned long`).
**No reader bounds its index** (`xor eax, eax; mov al / ax, ...; jmp [eax*4 +
table]`); each count is the run of code pointers up to the next table, read by
hand from the dwords `0x65FC24..0x65FDC4` and checked against the next
dispatcher's operand. Past a table an index reads the next table's entries
(code, so the original jumps there); ours reads in place and aborts only where
the word is not in `.text` (`scena_sc0.cpp`'s `CodeAt` rule).

| Table | Count | Reader | Index |
|---|--:|---|---|
| `PartyFormAction6_Forms` `0x65FC88` | 3 | `PartyFormAction6_ByForm` `0x51FA70` (`Field_FormActions[6]`) | u16 `+0x2C` |
| `PartyAction6_Forms` `0x65FC94` | 3 | `PartyAction6_ByForm` `0x51FA90` (`Field_ActionBySet[6]`) | u16 `+0x2C` |
| `PartyFormAction6_Form1States` `0x65FC3C` | 3 | `0x51F5D0` | `+2` |
| `PartyAction6_Form1States` `0x65FC48` | 3 | `0x51F5F0` | `+2` |
| `PartyFormAction6_Form2States` `0x65FC54` | 3 | `0x51F610` | `+2` |
| `PartyAction6_Form2States` `0x65FC60` | 2 | `0x51F630` | `+2` |
| `PartyAction6_Form2State0Steps` `0x65FC68` | 5 | `0x51F650` | `+3` |
| `PartyAction6_Form2State1Steps` `0x65FC7C` | 3 | `0x51FA10` | `+3` |
| `PartyFormAction7_Forms` `0x65FCD8` | 3 | `0x520120` (`Field_FormActions[7]`) | u16 `+0x2C` |
| `PartyAction7_Forms` `0x65FCE4` | 3 | `0x520140` (`Field_ActionBySet[7]`) | u16 `+0x2C` |
| `PartyFormAction7_Form0States` `0x65FCA0` | 3 | `0x51FAB0` | `+2` |
| `PartyAction7_Form0States` `0x65FCAC` | 2 | `0x51FAD0` | `+2` |
| `PartyFormAction7_Form1States` `0x65FCB4` | 3 | `0x51FC60` | `+2` |
| `PartyFormAction7_Form2States` `0x65FCC0` | 3 | `0x51FD00` | `+2` |
| `PartyAction7_Form2States` `0x65FCCC` | 3 | `0x51FD20` | `+2` |
| `PartyFormAction8_Forms` `0x65FD34` | 3 | `0x5207E0` (`Field_FormActions[8]`) | u16 `+0x2C` |
| `PartyAction8_Forms` `0x65FD40` | 3 | `0x520800` (`Field_ActionBySet[8]`) | u16 `+0x2C` |
| `PartyFormAction8_Form0States` `0x65FCF0` | 3 | `0x520160` | `+2` |
| `PartyAction8_Form0States` `0x65FCFC` | 2 | `0x520180` | `+2` |
| `PartyFormAction8_Form1States` `0x65FD04` | 3 | `0x520310` | `+2` |
| `PartyAction8_Form1States` `0x65FD10` | 3 | `0x520330` | `+2` |
| `PartyFormAction8_Form2States` `0x65FD1C` | 3 | `0x5203C0` | `+2` |
| `PartyAction8_Form2States` `0x65FD28` | 3 | `0x5203E0` | `+2` |
| `PartyFormAction9_Form0States` `0x65FD4C` | 3 | `0x520820` | `+2` |
| `PartyAction9_Form0States` `0x65FD58` | 2 | `0x5208B0` | `+2` |
| `PartyFormAction9_Form1States` `0x65FD60` | 3 | `0x520A40` | `+2` |
| `PartyAction9_Form1States` `0x65FD6C` | 2 | `0x520A60` | `+2` |
| `PartyAction9_Form1State0Steps` `0x65FD74` | 5 | `0x520A80` | `+3` |

Set 6's form 0 (`0x65FC24`, `0x65FC30`, read by R1B's `0x51F1D0` / `0x51F1F0`)
and set 9's forms (`0x65FDAC`, `0x65FDB8`, read by R1D's `0x521320` /
`0x521340`) and `0x65FD88` (R1D's `0x520E10`) are their readers' groups' to
name; R1C's `0x51F210` / `0x51F3D0` sit in `0x65FC30` and its set-9
dispatchers in `0x65FDAC` / `0x65FDB8`. Two entries are worth a note:
`BossOp_ScriptTick` `0x437CA0` is state 2 of `PartyFormAction6_Form2States`
and `PartyFormAction9_Form1States` (where the other forms have
`PartyAction_ScriptEnd`), and R1E's `0x5226D0`, `PartyAction5_Forms[2]`, is
also `PartyAction7_Forms[1]`.

## 4. The fuzz (`rest_1c_fuzz.cpp`)

`scenario_harness::Run` in field mode: 46 `Shape::kSprite` clones (the
dispatchers and state handlers) and five `Shape::kCall` (the pickups and cell
hits, `ret_mask` 0xFF: their three callers each test al only), **4,000 rounds
per function**. `BOF3X_R1C_ONLY=<name>` runs the clones whose name contains it.
The 28 tables are `DataTable`s, swapped for handler recorders while the fuzz
runs; every entry of every table was reached (the coverage line).

**The callees.** The group's own listing, registered before the standard rows:

| Callee | Masks | Answers |
|---|---|---|
| R0A's `PartyAction_TargetAhead`, `_BlockedAhead` | - | al 0 half the time, else 1 or any byte |
| `PartyAction_Kind30Ahead` | - | al 0xFF a third of the time, else 0..19 (all it answers) |
| `PartyAction_MemberOnEffect`, `_MemberBeyondEffect` | the low byte (the originals pass a dword over their caller's ecx) | `kFlag` |
| `PartyAction_SideProbes` | - | garbage (no caller reads eax or `+0x2B` after it) |
| `Effect_SpawnAtCellHigh`, `Effect_SpawnAtCell` | the state's byte, x and z 16 bits (movsx) | garbage |
| `Field_GiveZenny` | whole (a byte product pushed whole) | garbage |
| `AreaMap_ClearCell` | 16 bits each ([`field_hidden.md`](field_hidden.md) section 3) | garbage |
| `Field_EffectAhead` | - | al 0xFF half the time, else 0..19 |
| `Sprite_ObjectAt` | whole | al 0xFF half the time, else 0..0x21, the edges 0, 0x1D, 0x1E, 0x21 often |
| `Sprite_FlashClut` | the low byte | garbage |
| `Sprite_TurnSense` | whole | garbage (stored as a byte) |
| `AreaMap_ByteAt` | 16 bits each | al 0xF0..0xF9, 0xEF, 0, 0xFF, or any byte |
| `MapView_SlopeAt` | x, z whole; the direction's low byte (`AreaMap_Slope` reads that byte: the originals push a whole register, `0x51FD40` one holding `Sprite_Current`'s upper bytes) | the flag byte `0x903850` 0 a third of the time; ax at 0x40, 0x41, 0x3F, the s16 limits, ... |
| `MapView_GroundAt` | whole | ax the height word + 0, +-1, +0x40, +0x41, +0x3F, 0x8000-ish, or random |
| the five helpers of the group | 16 bits each | `kFlag` |

The standard rows serve `Sound_PlayEffect`, `Sprite_EnsureAnimation`,
`Sprite_SetAnimation`, `Sprite_ScriptTickOnce`, `Item_NamePtr` (into the text
buffer), `Inventory_Add`, `Msg_OpenSystem`, `Char_LoseHp`, `Field_JumpStart`,
`Field_LeaderStepTick`, `Effect_FindFree` and `Rand` (`kRand`; the seed sets
its hint to the draws' edges).

**The state.** Field mode's 38 standard regions and two of the group's:
`Field_DirectionSteps` (0x40, `.data`, restored after the run) and
`Text_Records` (0x20, the item names). 23,692 bytes, 40 regions.

**The seeds** (`Seed(k)`): the steps table half the time in the exe's shape
(0, +-0x8000), else a boundary or random; the direction 0..7 two times in
three, else 8, 9, 15, 0x80, 0xFF or random (read in place past the table);
x and z a cell at 0, 1, 2, small, the s16 / u16 limits or random with a
fraction 0 (three in eight), a half, a quarter, 1, 0xFFFF or random; the
height around 0, 0x40, the s16 limits; `+0xA` 0, 1, 2, 5, 0xB, 0xFF or random;
`+9` 0..3, 0xFF or random; `+0xB` 0, 1, 2, 7, 0xFF or random; `+7` 0 or not;
`Field_InputFlags` 0, 2, 4, 6, 1, 8 or random; `Field_State +0x138` 0, 1, 2,
3, 0xFE or random and `+0x89` 0..2 or random. A dispatcher's index below its
table's length (`+2`, `+3` or the u16 `+0x2C`); `PartyAction_TurnStep`'s side
`+3` at 3, 5, 4, 0 or random and `+8` equal to it half the time;
`PartyAction_WaitEffectEnd`'s `+0xB` a record 0..19 two times in three, else 0xFF,
20, 21 or random, and every record's in-use byte 0 or not; `Rand`'s hint at
0xC..0xF, 0, 3..7, 0xB. The helpers' x and z low words 0, 1, 0x7FFF, 0x8000,
0xFFFF, 0x40 or random under random upper halves.

**The disturbance** (the group's case, from its hash only): the direction,
x or z, the height, `+0xA`, `+9`, `+0xB`, `+6` / `+7`, `+0x2B` / the form's low
byte, `Field_State +0x89` / `+0x128` / `+0x137` / `+0x138`, the in-use byte
of the record `+0xB` names, the sloped flag, a dword of the steps table. The
harness's own moves `Sprite_Current` among the records and its state bytes
`+1..+4`.

**A pitfall paid for**: the first `Sprite_ObjectAt` stand-in picked its edge
values with `PickOf`, which draws `Next()` - the seed's stream, not shared by
the two passes - and 633 rounds mismatched at the marked objects' `+0x80`;
a stand-in's answer draws from `Noise()` only (R0A's disturbance pitfall, in an
effect).

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_1c`, exit 0): 204,000 rounds over 51 functions, 336,024
calls to the stand-ins, **0 mismatches**; `inject: 8706 ours, 0 left
original` (8,655 + 51). `BOF3X_SHADOW='*'` (this worktree, 2026-10-04): exit 0, `inject: 8706 ours, 0 left original`, 1,020 self-test lines of 0 mismatches and none other (`rest_1c` among them: 204,000 rounds, 0 mismatches); the same with `BOF3X_WIDE=1`: exit 0, 1,020, 8,706 ours. Each passed on its first complete run (a first `'*'` was stopped by the scratch runner's own timeout, not by a fault).

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **`PartyAction_WaitEffectEnd` reads an effect index unchecked, and play can
  hand it 0xFF.** The state before it in the form-1 tables (R1B's `0x51DE20`
  and its likes) stores `Effect_FindFree`'s answer in `+0xB`, 0xFF when the
  20 records are all in use, and moves to this state all the same; this one
  then tests byte `0x7E11E0 + 0xFF * 0x80` (`0x7E9160`, past `Effect_Objects`)
  for "no longer in use". **Reproduced** (read in place, not aborted): the read
  does not fault, and an abort would stop the game where the original goes
  on - as `field_hidden`'s `Member_EffectState` reads its index. What the
  original then does depends on whatever lies at `0x7E9160`; whether play
  ever fills all 20 records at that moment is not established. The seed
  covers it (0xFF, 20, 21).
- **The effect index `0x51F700` writes through is not bounded** (a signed byte
  from `Field_EffectAhead`) and **the one `0x51FAF0` writes through** (from
  `PartyAction_Kind30Ahead`): both callees answer 0..19 or 0xFF only, and the
  callers test 0xFF first, so play never hands another. **Ours aborts** past
  19 with a `Fatal` naming the index.
- **The object index each marking writes through is not bounded**
  (`Sprite_ObjectAt`'s answer: `0x51F700` and the Form0Resolves as a signed
  byte, `0x51FAF0` unsigned): `Sprite_ObjectAt` answers 0..0x21 or 0xFF only.
  **Ours aborts** on any other byte.
- **The dispatchers' indexes are not bounded** (section 3): past a table the
  original jumps through the next table's words; ours does the same and aborts
  only where the word is not code.
- **The direction is not masked** where a step is read (all the Begin and
  Resolve shapes): a direction above 7 reads the `.data` after
  `Field_DirectionSteps`. Reproduced, as R0A and `field_hidden` do.
- **Uninitialised stack the callees never read**: the Resolve shapes push the
  cells as dwords whose upper halves are their own stack (as
  `PartyAction5_Form0Resolve`), and the cell hits store the cell byte over
  their z argument's slot and push that dword as a fourth argument
  `Effect_SpawnAtCellHigh` does not read. Every callee reads the low words.
- **Ranges, not defects**: the zenny's product is a byte (5 x 20 = 100 fits);
  the cell one on is the dword + 1, its low word wrapping as ours does.

None of these needs a ledger entry: nothing the original reads unwritten
reaches what is drawn or decided except `PartyAction_WaitEffectEnd`'s byte, which
ours reads exactly as the original does.

## 6. Controls

`r1c/controls.py` (scratch): each plant replaces a string that occurs exactly
once in `rest_1c.cpp`, rebuilds, runs the self-test on the clones it names
(`BOF3X_R1C_ONLY`), restores the file and rebuilds. The count is the rounds
that mismatched of 4,000 a clone run.

**101 planted: 97 refused by a count; 3 (C01..C03) refused only by a crash** - the plant indexed past the run of
tables into words that are not code, which the fuzz cannot count (HANDOFF's trap: a hang or crash proves less), so each
was planted again inside the run (C01b..C03b) and refused in every round; **1 not refused (C85), an equivalent
mutant**, its near variant C85b refused.

The thinnest are the zenny's tenfold (C35, 3 rounds; C36 / C37, 6): it needs the 0xF2 cell, an effect free, a Rand
nibble of 13..15, an input bit and a second Rand & 3 of 0 in one round. C29 (28) needs `Sprite_ObjectAt` to answer
exactly 0x1D. Every function has at least one control refused in hundreds of rounds or more.

| # | Clones run | Plant | Refused (rounds of 4,000) |
|---|---|---|--:|
| C01 | `PartyAction6_Form1` | the state dispatchers by +3 for +2 | a crash (0xC0000005): an index past the run of tables into words that are not code; see C01b |
| C02 | `PartyAction6_Form2State0` | the step dispatchers by +2 for +3 | a crash (0xC0000005): an index past the run of tables into words that are not code; see C02b |
| C03 | `PartyAction6_ByForm` | the form word +0x2E | a crash (0xC0000005): an index past the run of tables into words that are not code; see C03b |
| C04 | `PartyAction7_ByForm` | set 7's action through the form actions' table | 4000 |
| C05 | `PartyAction6_Form2` | the table one entry on | 4000 of 24,000 |
| C06 | `PartyAction6_Form0Begin` | the first turn forward | 1930 |
| C07 | `PartyAction6_Form0Begin` | the turn back by three | 486 |
| C08 | `PartyAction6_Form0Begin` | the odd test on bit 1 | 1620 |
| C09 | `PartyAction6_Form0Begin` | steep from 0x40 | 192 |
| C10 | `PartyAction6_Form0Begin` | the steep animation 0x47 | 1087 |
| C11 | `PartyAction6_Form0Begin` | the sound + 0x101 | 2913 |
| C12 | `PartyAction6_Form0Begin` | +0xA = 6 | 2913 |
| C13 | `PartyAction6_Form0Begin` | the end +0xB = 1 | 4000 |
| C14 | `PartyAction6_Form0Begin` | the probe clears +0x2B to 2 | 676 |
| C15 | `PartyAction6_Form0Begin` | the probe on level ground too | 95 |
| C16 | `PartyAction6_Form0Begin` | the second probe pushes 7 | 2913 |
| C17 | `PartyAction6_Form0Begin` | the first probe on row 5 | 2861 |
| C18 | `PartyAction6_Form0Begin` | the turn by a shift (direction 0 gives -1) | 4000 |
| C19 | `PartyAction6_Form0Begin` | the sloped flag not tested | 516 |
| C20 | `PartyAction6_Form0Begin` | the slope two steps ahead in x | 3273 |
| C21 | `PartyAction7_Form2Begin` | the rise from 0x40 | 207 |
| C22 | `PartyAction7_Form2Begin` | the height before the ground call | 133 |
| C23 | `PartyAction7_Form2Begin` | the slope direction not re-read | 133 |
| C24 | `PartyAction7_Form2Begin` | the rise reversed | 1371 |
| C25 | `PartyAction6_Form0Resolve` | the count by two | 4000 |
| C26 | `PartyAction6_Form0Resolve` | the object margin 1 | 947 |
| C27 | `PartyAction6_Form0Resolve` | the z probe one on in x too | 113 |
| C28 | `PartyAction6_Form0Resolve` | the x probe answer inverted | 174 |
| C29 | `PartyAction6_Form0Resolve` | object 0x1D among the extra | 28 |
| C30 | `PartyAction6_Form0Resolve` | the mark bit 1 | 276 |
| C31 | `PartyAction6_Form0Resolve` | the x cell from >> 15 | 872 |
| C32 | `PartyAction6_CellPickup` | zenny from 14 | 16 |
| C33 | `PartyAction6_CellPickup` | five on 14 too | 17 |
| C34 | `PartyAction6_CellPickup` | the input bit 2 not tested | 11 |
| C35 | `PartyAction6_CellPickup` | the tenfold on Rand & 1 | 3 |
| C36 | `PartyAction6_CellPickup` | set 6's twentyfold | 6 |
| C37 | `PartyAction7_CellPickup` | set 7's tenfold | 6 |
| C38 | `PartyAction6_CellPickup` | the second spawn state 2 | 51 |
| C39 | `PartyAction6_CellPickup` | twelve bytes of the name | 271 |
| C40 | `PartyAction6_CellPickup` | two of the item | 271 |
| C41 | `PartyAction6_CellPickup` | the cell 0xF3 | 537 |
| C42 | `PartyAction6_CellPickup` | the find +0xB = 2 | 267 |
| C43 | `PartyAction6_CellPickup` | the item request 3 | 253 |
| C44 | `PartyAction6_Form2Begin` | the turn by TargetAhead | 1930 |
| C45 | `PartyAction6_Form2Begin` | +0xB = 1 | 4000 |
| C46 | `PartyAction6_Form2Begin` | +0xA = 0xA | 4000 |
| C47 | `PartyAction6_Form2Resolve` | the count from 2 | 947 |
| C48 | `PartyAction6_Form2Resolve` | the sound 0x10C | 479 |
| C49 | `PartyAction6_Form2Resolve` | the record +8 from +9 | 441 |
| C50 | `PartyAction6_Form2Resolve` | the record +0xA = 2 | 479 |
| C51 | `PartyAction6_Form2Resolve` | the index into +7 | 479 |
| C52 | `PartyAction6_Form2Resolve` | +3 one on, not two | 475 |
| C53 | `PartyAction6_Form2Resolve` | the object margin 1 | 468 |
| C54 | `PartyAction6_Form2Resolve` | no sound on the object | 225 |
| C55 | `PartyAction6_Form2Resolve` | the x probe on the cell itself | 132 |
| C57 | `PartyAction6_CellHit` | the cell 0xF5 for 0xF4 | 537 |
| C58 | `PartyAction6_CellHit` | the second spawn above 4 | 138 |
| C59 | `PartyAction6_CellHit` | the cell 0xF8 for 0xF7 | 546 |
| C60 | `PartyAction6_CellHit` | the item below 6 | 31 |
| C61 | `PartyAction6_CellHit` | the hurt from 0xB | 30 |
| C62 | `PartyAction6_CellHit` | item 0x2A | 257 |
| C63 | `PartyAction6_CellHit` | the item +0xB = 1 | 257 |
| C64 | `PartyAction6_CellHit` | the member from +0x88 | 150 |
| C65 | `PartyAction6_CellHit` | message 0xDA | 152 |
| C66 | `PartyAction6_CellHit` | the hurt +0xB = 2 | 152 |
| C67 | `PartyAction6_CellHit` | the request 3 | 539 |
| C68 | `PartyAction6_CellHit` | the flash colour 1 | 152 |
| C69 | `PartyAction6_CellHit` | the break sound 0x10A | 777 |
| C70 | `PartyAction6_CellHit` | the second spawn state 5 | 172 |
| C71 | `PartyAction7_Form0Begin` | the member on it tested against 1 | 871 |
| C72 | `PartyAction7_Form0Begin` | the record +0xA | 288 |
| C73 | `PartyAction7_Form0Begin` | the animation + 9 | 288 |
| C74 | `PartyAction7_Form0Begin` | the pace 3 | 287 |
| C75 | `PartyAction7_Form0Begin` | the flag 0x2000 | 218 |
| C76 | `PartyAction7_Form0Begin` | +9 one up | 288 |
| C77 | `PartyAction7_Form0Begin` | +0x137 = 2 | 288 |
| C78 | `PartyAction7_Form0Begin` | the bit 1 of +0x138 | 653 |
| C79 | `PartyAction7_Form0Begin` | the object margin 0 | 865 |
| C80 | `PartyAction7_Form0Begin` | the end +0x137 = 1 | 1339 |
| C81 | `PartyAction7_Form0Begin` | one step ahead in x | 729 |
| C83 | `PartyAction_TurnToSide` | a tie to 5 | 345 |
| C84 | `PartyAction_TurnToSide` | +9 = 3 | 4000 |
| C85 | `PartyAction_TurnToSide` | the side 6 for 5 | **not refused**: equivalent - on integers \|d - 3\| > \|d - 6\| and \|d - 3\| > \|d - 5\| both mean d >= 5; near variant C85b refused |
| C86 | `PartyAction_TurnToSide` | +0xB the side | 3984 |
| C87 | `PartyAction_TurnStep` | the two animations swapped | 1250 |
| C88 | `PartyAction_TurnStep` | side 4 for 5 | 944 |
| C89 | `PartyAction_TurnStep` | +9 = 1 | 557 |
| C90 | `PartyAction_TurnStep` | the direction & 3 | 254 |
| C91 | `PartyAction_TurnStep` | the turn at +9 1 | 270 |
| C92 | `PartyAction_TurnStep` | Sprite_Current not re-read after the animation | 50 |
| C93 | `PartyAction_TickThenFace` | the animation from +9 | 2517 |
| C94 | `PartyAction_TickThenFace` | +2 for +3 | 2712 |
| C95 | `PartyAction_WaitEffectEnd` | +6 for +7 | 1355 |
| C96 | `PartyAction_WaitEffectEnd` | +7 = 1 | 1304 |
| C97 | `PartyAction_WaitEffectEnd` | the record +1 for +0 | 445 |
| C98 | `PartyAction_WaitEffectEnd` | +0x2B = 1 | 900 |
| C99 | `PartyAction6_Form2Probe` | +0xA = 0xC | 4000 |
| C01b | `PartyAction6_Form1` | the state index ^ 1 (in the run of tables) | 4000 |
| C02b | `PartyAction6_Form2State0` | the step index ^ 1 | 4000 |
| C03b | `PartyAction6_ByForm` | the form index ^ 1 | 4000 |
| C85b | `PartyAction_TurnToSide` | the side 7 for 5 (C85 near variant) | 337 |

## 7. Calls across groups

**Out of R1C**: every call is to a function already ours - R0A's seven
helpers by name through `symbols.gen.h` (32 sites: `PartyAction_TargetAhead`
6, `_BlockedAhead` 4, `_SideProbes` 3, `_Kind30Ahead` 3, `_MemberOnEffect` 3,
`_MemberBeyondEffect` 3, `Effect_SpawnAtCellHigh` 10, as `band_rows.py
--edges` lists) and the engine's (`Sound_PlayEffect`, `Sprite_*`, `MapView_*`,
`AreaMap_*`, `Effect_*`, `Field_*`, `Item_NamePtr`, `Inventory_Add`,
`Msg_OpenSystem`, `Char_LoseHp`), plus Capcom's CRT `Rand` `0x5B93D2`. **No
raw-address call to another group of this round**, and no `rest_1c_callees.h`
is needed. The dispatchers jump through their tables to other groups'
handlers - R1A's `0x51C490`, `0x51D6D0`; R1B's `0x51DE20`, `0x51D440`,
`0x51F1D0`, `0x51F1F0`; R1D's `0x521C40`, `0x520E90`, `0x520E10`; R1E's
`0x5239F0`, `0x5226D0`, `0x522DE0`; R1F's `0x523F10` - by the word in the
table, which becomes the jmp to ours when those merge: nothing to rebind.

**Into R1C** (for the rebinding pass): no `E8` / `E9` from outside the group.
Everything reaches R1C through `.data`: `Field_FormActions[6..8]` and
`Field_ActionBySet[6..8]` (read by `Field_FormActionState` `0x52F4F0` and
`Field_ActionState` `0x52FB60`, ours); R1B's table `0x65FC30` (`0x51F210`,
`0x51F3D0`); R1D's tables `0x65FDAC` / `0x65FDB8` (the set-9 dispatchers); and
the shared states from other sets' tables - `PartyAction_TurnToSide` 34
cells, `PartyAction_TurnStep` 31, `PartyAction_TickThenFace` 9,
`PartyAction_WaitEffectEnd` 7 (`band_rows.py --byte-tables`, the cells listed in
each `symbols.toml` entry). The 32 pickup / cell-hit sites are the group's
own.

## 8. The live route

The cut's `reach` column (the catalog's) is empty for all 50 rows, and no
first-call or counted trace under `analysis/calltrace` (every file but the
logs and the entry lists, searched for the 17 shapes' entries) names any of
them. So no recorded route enters R1C: **fuzz only**. The coordinator's state
hash covers whatever a route reaches after the merge.

## 9. The rebinding

`band_rows.py --refs --group R1C`: ten raw references to one of the 51, all to
`0x520000` - and every one a coordinate (`ChangeArea(0xA, 0x520000, ...)` in
`scena_sc1.cpp`, the area boxes of `scena_sc6`, `scena_sc9b`, `scena_sc15`,
`mode_states` and their fuzz rows), not this function. `grep -rn -i` over
`src/game` for the other 50 entries: none. **Nothing rebound, nothing left
raw.** No harness or fuzz row lists any of the 51 as theirs
(`scenario_harness*.cpp`, `boss_harness*.cpp`: grep), so none broke.

## 10. Divergence and patches

None. `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no byte in
`0x51F210..0x520E08` (grep, 2026-10-04); nothing in the band draws, so there
is no full-frame fill for DIV-0041.

## 11. What the cut and the tool said, settled

- **The extents**: the tool's read extents are right for all 51; the cut's
  sizes (the catalog's) run to the next 16-byte boundary (43 rows, padding
  only) and once into code: the cut's `0x51FA10` (96 bytes) holds the 0x12-byte
  dispatcher and **`0x51FA30`**, a 0x35-byte state handler reached only
  through `PartyAction6_Form2State1Steps[0]` - code no list had, taken here.
- **45 hidden starts**: every one is a function reached through a table
  (none a jump-table case, none a shared tail); their hosts are this group's
  (`0x51F4B0`, `0x51F880`, `0x520000`, `0x5206C0`) or R1B's `0x51EF30`
  (`0x51F210`, `0x51F3D0`).
- **The three `hypothesis` rows** (`0x51F610`, `0x520000`, `0x520A40`) are
  functions like the rest.
- **`analysis/calltrace/entries_logic.txt`** (main checkout): 46 lines
  appended with the extents read here. Five entries were already there and
  are not duplicated: `00520000 11F` is right; four carry their **host**
  extents - `0051F4B0 3C3`, `0051F880 77B`, `005206C0 5BC`, `00520C80 57B` (the
  last running into R1D's `0x520E10..`) - where the functions' own are 0x11F,
  0x188, 0x11F and 0x188, for whoever owns the file after wave one to split
  (R0A's section 10 left its seven the same way).
