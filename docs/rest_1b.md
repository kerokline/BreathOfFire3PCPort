# Group R1B: party sets 2..6's field actions

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, from
the round branch's tip `ba2c3c3`. **47 functions ours**
(`src/game/rest_1b.cpp`, declarations in `src/game/rest_1b.h`, shadow name
`rest_1b`): the cut's 47 rows for R1B (`analysis/round14_cut.tsv`, the band
`0x51D710..0x51F202`), none added, none dropped. Each read to its last
instruction with capstone and fuzzed through the scenario harness in field
mode ([`scenario_harness.md`](scenario_harness.md) section 7), used
unchanged: 188,000 rounds, 0 mismatches. 88 controls planted one at a time: 85 refused by a mismatch, 1 (C06) by a fault before any, 2 equivalent mutants not refused, each with a near variant refused (section 11). **Fuzz only**: no
recorded route enters any of the 47 (section 9). No divergence, no full-frame
fill, no byte patch inside the band.

The band is one thing: the party sets' field actions for sets 2..6, the
pattern the PSX kept per character set in the PLP overlays. A party set's
action is reached two ways - leader state 10 (`Field_ActionState`
`0x52FB60`) calls `Field_ActionBySet[set]`, leader state 6
(`Field_FormActionState` `0x52F4F0`) calls `Field_FormActions[set]` - and each
of those dispatches by the form word `Sprite_Current +0x2C` to a form, which
dispatches by the state byte `+2`, and some states by a step byte `+3`. The
names follow round eight's for set 5 (`PartyAction5_ByForm`,
`PartyAction5_Forms`, `PartyAction5_Form0States`, `field_hidden.cpp`):
`PartyActionN_*` on the `Field_ActionBySet` path, `PartyFormActionN_*` on the
`Field_FormActions` path. What a form or a state is in play (which character,
which field skill) is not read here; the names say where the code sits and
what it tests.

| Kind | Functions | Bytes read |
|---|--:|--:|
| dispatchers (`jmp [table + index * 4]`) | 29 | 0x12 / 0x13 each |
| states | 14 | 0x35 .. 0x1B1 |
| cell handlers (x, z) answering al | 4 | 0x11F, 0x188 |

Seven of them are **instruction-for-instruction copies of code already ours**
(`rdis.py cmp` in the scratch: every mnemonic and operand equal, only the
relative call targets differ): `0x51D790` and `0x51DF10` are
`PartyAction5_Form0Begin` `0x51E930`; `0x51D950` and `0x51E0D0` are
`PartyAction5_Form0Resolve` `0x51EAF0` calling their own cell handler;
`0x51DAA0` and `0x51E1B0` are `Field_CellPickup` `0x51EBD0`. `0x51E310` is
`0x51DC00` and `0x51EF30` is `0x51E6C0` the same way. Each copy is its own
function because the tables and the `E8` sites reach each by its own address;
ours shares one C++ body per shape (section 1).

## 1. What each function does

"`S`" is `Sprite_Current` (the leader whose action runs): its state bytes
`+2` / `+3`, `+6`, `+7`, the direction `+8`, `+9`, the counter `+0xA`, `+0xB`,
`+0x2B`, the form word `+0x2C`, the 16.16 position `+0x34` / `+0x38`, the
height word `+0x3E`, the script position word `+0x58`. "A step" is a row of
`Field_DirectionSteps` read in place with the direction unmasked, as R0A
describes ([`rest_0a.md`](rest_0a.md) section 1). "The form's sound" is
`Sound_PlayEffect(u16 S+0x2C + 0x100)`; "the pose" is
`Sprite_EnsureAnimation((S+8 - 1) / 2 + c)` with the signed division of
`cdq; sub; sar` (direction 0 gives 0). `S` is re-read after every call, as
the originals do (the harness's disturbance moves it).

### 1.1 The dispatchers

Each is `mov ecx, [Sprite_Current]; xor eax, eax; mov ax / al, [ecx + k];
jmp [eax * 4 + table]`, the index unchecked. Ours reads the table in place and
calls the entry, aborting past the table's own count (section 6).

| Function | Entry | By | Table (count) | Reached through |
|---|---|---|---|---|
| `PartyFormAction2_ByForm` | `0x51D710` | word `+0x2C` | `PartyFormAction2_Forms` `0x65FAC8` (3) | `Field_FormActions[2]` |
| `PartyAction2_ByForm` | `0x51D730` | word `+0x2C` | `PartyAction2_Forms` `0x65FAD4` (3) | `Field_ActionBySet[2]` |
| `PartyFormAction3_Form0` | `0x51D750` | `+2` | `PartyFormAction3_Form0States` `0x65FAE0` (3) | `PartyFormAction3_Forms[0]` |
| `PartyAction3_Form0` | `0x51D770` | `+2` | `PartyAction3_Form0States` `0x65FAEC` (3) | `PartyAction3_Forms[0]` |
| `PartyFormAction3_Form1` | `0x51DBC0` | `+2` | `PartyFormAction3_Form1States` `0x65FAF8` (3) | `PartyFormAction3_Forms[1]` |
| `PartyAction3_Form1` | `0x51DBE0` | `+2` | `PartyAction3_Form1States` `0x65FB04` (2) | `PartyAction3_Forms[1]` |
| `PartyFormAction3_Form2` | `0x51DDE0` | `+2` | `PartyFormAction3_Form2States` `0x65FB0C` (3) | `PartyFormAction3_Forms[2]` |
| `PartyAction3_Form2` | `0x51DE00` | `+2` | `PartyAction3_Form2States` `0x65FB18` (3) | `PartyAction3_Forms[2]` |
| `PartyFormAction3_ByForm` | `0x51DE90` | word `+0x2C` | `PartyFormAction3_Forms` `0x65FB24` (3) | `Field_FormActions[3]` |
| `PartyAction3_ByForm` | `0x51DEB0` | word `+0x2C` | `PartyAction3_Forms` `0x65FB30` (3) | `Field_ActionBySet[3]` |
| `PartyFormAction4_Form0` | `0x51DED0` | `+2` | `PartyFormAction4_Form0States` `0x65FB3C` (3) | `PartyFormAction4_Forms[0]` |
| `PartyAction4_Form0` | `0x51DEF0` | `+2` | `PartyAction4_Form0States` `0x65FB48` (3) | `PartyAction4_Forms[0]` |
| `PartyFormAction4_Form1` | `0x51E2D0` | `+2` | `PartyFormAction4_Form1States` `0x65FB54` (3) | `PartyFormAction4_Forms[1]` |
| `PartyAction4_Form1` | `0x51E2F0` | `+2` | `PartyAction4_Form1States` `0x65FB60` (2) | `PartyAction4_Forms[1]` |
| `PartyFormAction4_Form2` | `0x51E480` | `+2` | `PartyFormAction4_Form2States` `0x65FB68` (3) | `PartyFormAction4_Forms[2]` |
| `PartyAction4_Form2` | `0x51E4A0` | `+2` | `PartyAction4_Form2States` `0x65FB74` (2) | `PartyAction4_Forms[2]` |
| `PartyAction4_Form2State0` | `0x51E4C0` | `+3` | `PartyAction4_Form2State0Steps` `0x65FB7C` (5) | `PartyAction4_Form2States[0]` |
| `PartyAction4_Form2State1` | `0x51E850` | `+3` | `PartyAction4_Form2State1Steps` `0x65FB90` (3) | `PartyAction4_Form2States[1]` |
| `PartyFormAction4_ByForm` | `0x51E8B0` | word `+0x2C` | `PartyFormAction4_Forms` `0x65FB9C` (3) | `Field_FormActions[4]` |
| `PartyAction4_ByForm` | `0x51E8D0` | word `+0x2C` | `PartyAction4_Forms` `0x65FBA8` (3) | `Field_ActionBySet[4]` |
| `PartyFormAction5_Form0` | `0x51E8F0` | `+2` | `PartyFormAction5_Form0States` `0x65FBB4` (3) | `PartyFormAction5_Forms[0]` |
| `PartyFormAction5_Form1` | `0x51ECF0` | `+2` | `PartyFormAction5_Form1States` `0x65FBCC` (3) | `PartyFormAction5_Forms[1]` |
| `PartyAction5_Form1` | `0x51ED10` | `+2` | `PartyAction5_Form1States` `0x65FBD8` (2) | `PartyAction5_Forms[1]` (round eight's table) |
| `PartyAction5_Form1State0` | `0x51ED30` | `+3` | `PartyAction5_Form1State0Steps` `0x65FBE0` (5) | `PartyAction5_Form1States[0]` |
| `PartyAction5_Form1State1` | `0x51F0C0` | `+3` | `PartyAction5_Form1State1Steps` `0x65FBF4` (3) | `PartyAction5_Form1States[1]` |
| `PartyFormAction5_Form2` | `0x51F170` | `+2` | `PartyFormAction5_Form2States` `0x65FC00` (3) | `PartyFormAction5_Forms[2]` |
| `PartyFormAction5_ByForm` | `0x51F190` | word `+0x2C` | `PartyFormAction5_Forms` `0x65FC0C` (3) | `Field_FormActions[5]` |
| `PartyFormAction6_Form0` | `0x51F1D0` | `+2` | `PartyFormAction6_Form0States` `0x65FC24` (3) | set 6's form-action table `0x65FC88[0]` (R1C's `0x51FA70`, `Field_FormActions[6]`) |
| `PartyAction6_Form0` | `0x51F1F0` | `+2` | `PartyAction6_Form0States` `0x65FC30` (3) | set 6's action table `0x65FC94[0]` (R1C's `0x51FA90`, `Field_ActionBySet[6]`) |

Set 2's forms, the `PartyFormAction*` forms' states (`0x51BEB0`, `0x51CC60`,
`0x520840`, `0x51C490`, `0x51FC80` - other groups'; `0x52F5C0`
`PartyAction_ScriptEnd` and `0x437CA0` `BossOp_ScriptTick` - ours) and the
other entries named in sections 2 and 7 are not this group's; every table is
read in place.

### 1.2 Set 3's and set 4's form 0 (begin, resolve, finish)

- **`PartyAction3_Form0Begin` `0x51D790` / `PartyAction4_Form0Begin`
  `0x51DF10`** (0x1B1 each; `PartyAction5_Form0Begin`'s code): an even `+8`
  is turned one eighth back (-1, & 7); while `PartyAction_TargetAhead`
  answers 0, two on (+2) and then back (-2). The slope one step ahead
  (`MapView_SlopeAt`, the direction byte pushed in `bl` over the caller's
  `ebx`): steep (`DamageScratch`'s flag and the low word, s16, above 0x40) -
  the pose + 0x46 and `+2` one on; else `+0x2B` = 1, the side probes in
  directions 3 and 5 (the rows at `0x6697C8` / `0x6697D8` read by address;
  steep and the ground above `+0x3E`: `+0x2B` = 0), the form's sound, the pose
  + 0x42, `+0xA` = 5. Then `+0xB` = 0 and `+2` one on (so a steep slope moves
  `+2` by two, to `PartyAction_Finish`).
- **`PartyAction3_Form0Resolve` `0x51D950` / `PartyAction4_Form0Resolve`
  `0x51E0D0`** (0xDB each; `PartyAction5_Form0Resolve`'s code): `+0xA`
  counted down; at 0 the point two steps ahead; `Sprite_ObjectAt(point, 0)`
  not 0xFF: bit 0 of that object's `+0x80` (the byte compared signed: below
  0x1E a `Sprite_Objects` record, else `Sprite_ObjectsExtra[i - 0x1E]`); then
  the set's cell handler on the point's cell, and while it answers 0, on the
  cell one on in x (only when x's low word is not 0), then one on in z (z's
  not 0); `+2` one on. `Sprite_ScriptTickOnce` every call.
- **`PartyAction3_CellPickup` `0x51DAA0` / `PartyAction4_CellPickup`
  `0x51E1B0`** (0x11F each; `Field_CellPickup`'s code, `field_hidden.md`):
  `AreaMap_ByteAt(x, z)` 0xF2 - with an object free, `Effect_SpawnAtCell(0)`,
  `Rand & 0xF` at least 0xD finds zenny (2; 5 on 0xF; ten times that when
  `Field_InputFlags & 6` and a second `Rand & 3` is 0) through
  `Field_GiveZenny` and `Effect_SpawnAtCell(1)`, `+0xB` = 1; 0xF8 -
  `Effect_SpawnAtCell(0)`, item 0x56's name into `Text_Records`,
  `Inventory_Add(0, 0x56, 1)`: taken, sound 0x106 and system message 2, else
  message 3; `Field_Request` = 2, `+0xB` = 1. Both clear the cell
  (`AreaMap_ClearCell`, also when no object was free) and answer 1; any other
  code 0.

### 1.3 Set 3's and set 4's form 1 state 0, and the kind-0x3A spawn

- **`PartyAction3_Form1Begin` `0x51DC00` / `PartyAction4_Form1Begin`
  `0x51E310`** (0x166 each): `PartyAction_Kind30Ahead`; an index: when
  `PartyAction_MemberOnEffect(it)` or `PartyAction_MemberBeyondEffect(it)`
  answers non-zero, `Field_State +0x137` = 0 and nothing more; else that
  effect object's `+0xB` = 1, the form's sound, `Sprite_EnsureAnimation(+8 +
  8)`, `Field_State +0x128` = 2, `Field_ScriptFlags` bit 12 (`or byte
  [0x9039A3], 0x10`), `Field_JumpStart`, `+9` one down,
  `Field_LeaderStepTick`, `Field_State +0x137` = 1, `+2` one on (state 1 is
  R1D's `0x521C40`, which counts `+9` down and ends the action). None: the
  point two steps ahead; unless `Field_State +0x138` bit 0,
  `Sprite_ObjectAt(point, 1)` not 0xFF marks the object (compared unsigned
  here); `Field_State +0x137` = 0. The index is pushed as the dword whose low
  byte the function stored over its own saved `ecx` (the callee reads the
  byte).
- **`PartyAction_SpawnKind3A` `0x51DE20`** (0x6E): `Sprite_ScriptTick`; when
  the script position word `+0x58` is then 0xA, `+0xB` = `Effect_FindFree`,
  read back; not 0xFF: that record `+0` = 1 and kind `+5` = 0x3A. The form's
  sound either way, `+7` = 0, `+2` one on. Seven `.data` cells hold it
  (`PartyAction3_Form2States[1]` and six of other sets' tables, section 7).

### 1.4 Set 4's form 2 and set 5's form 1 (aim, hit, again, wait)

Both forms have two states, each dispatching by its step `+3` through a
table of its own (`PartyAction4_Form2State0Steps` /
`PartyAction5_Form1State0Steps`, 5 entries; `..State1Steps`, 3).

- **`PartyAction4_Form2Aim` `0x51E4E0`** (0x87, step 0 of state 0): the turn
  of the Begin states with `PartyAction_BlockedAhead` (an answer of 1 keeps
  the turn: it turns toward a blocked cell); `PartyAction_SideProbes`; the
  pose + 0x42; `+0xB` = 0, `+0xA` = 0xB, `+3` = 1.
- **`PartyAction5_Form1Aim` `0x51ED50`** (0xA0): the same turn; the form's
  sound; the pose + 0x42; `+0xB` = 0, `+6` = 0, `+0xA` = 8, `+3` = 1.
- **`PartyAction4_Form2Hit` `0x51E570`** (0x14C, step 1): with `+0xA` not 0,
  counted down; at 0: the form's sound; `Field_EffectAhead` an index: sound
  0x10B, that effect object's `+8` = `S+8` and `+0xA` = 1, `+6` = the index
  (`S` read after the sound), `+3` one on; none: the point two steps ahead,
  `Sprite_ObjectAt(point, 0)` not 0xFF marks the object (signed) and sound
  0x10B, then `PartyAction4_CellHit` on the cells as the Resolve states do.
  Then `+3` one on - two in all for an effect object. `Sprite_ScriptTickOnce`
  every call.
- **`PartyAction5_Form1Hit` `0x51EDF0`** (0x140): the same, without the form's
  sound first, the effect object's `+8` / `+0xA` written before sound 0x10B
  (`S` read before it for them, after it for `+6`), and `PartyAction5_CellHit`.
- **`PartyAction4_CellHit` `0x51E6C0` / `PartyAction5_CellHit` `0x51EF30`**
  (0x188 each): `AreaMap_ByteAt(x, z)` (the code also stored over the low byte
  of the z argument's own slot): 0xF0, 0xF1, 0xF4 - `Effect_SpawnAtCellHigh(0,
  x, z)`, a second one in state 4 when `Rand & 7` is above 5, sound 0x10B,
  answer 1; 0xF6, 0xF7 - `Effect_SpawnAtCellHigh(0)`, sound 0x10B, then `Rand
  & 0xF`: below 7 `Effect_SpawnAtCellHigh(3)`, item 0x29's name into
  `Text_Records`, `Inventory_Add(0, 0x29, 1)` (taken: sound 0x106 and message
  2, else message 3), `+0xB` = 2; 7..0xB nothing more; above 0xB
  `Effect_SpawnAtCellHigh(2)`, `Sprite_FlashClut(0)`, `Char_LoseHp(1, Field_State
  +0x89)`, system message 0xD9, `+0xB` = 1; then `Field_Request` = 2 and answer
  1. Anything else 0. Every `Effect_SpawnAtCellHigh` call pushes a fourth dword
  (the code over z's upper bytes) R0A's helper never reads.
- **`PartyAction4_Form2Again` `0x51E870`** (0x35, step 0 of state 1):
  `PartyAction_SideProbes`, the pose + 0x42, `+0xA` = 0xB, `+3` = 1.
- **`PartyAction5_Form1Again` `0x51F0E0`** (0x46): the form's sound, the pose
  + 0x42, `+0xA` = 8, `+3` = 1.
- **`PartyAction5_Form1Wait` `0x51F130`** (0x3C, step 2 of state 1): the
  `Effect_Objects` record `+0xB` names, free (`+0`) or in state 1 (`+1`), and
  `Field_Kind2Hold` 0: `Field_State +0x137` = 0. A tail jump to
  `Sprite_ScriptTick` (ours calls it; its answer is no caller's).

## 2. The tables

29 `[[data]]` entries in `symbols.toml`, each `unsigned long`. **The count is
the run of code pointers to the next table's start**, read by hand
(`rdis.py dw 0x65FA74 120`), and checked against what the states store into
the index: the two-entry tables' second state never moves `+2` on (R1D's
`0x521C40` ends the action; set 4's / set 5's states hand over by `+2` = 1
from their step tables' last entries), the five-entry step tables are walked
by Aim (`+3` = 1), Hit (one or two on) and the other groups' steps after it.
None of the 29 dispatchers bounds its index; `band_rows.py` reads 256 code
entries for every table (its note), which runs on through the next tables.

| Count | Tables |
|--:|---|
| 2 | `PartyAction3_Form1States`, `PartyAction4_Form1States`, `PartyAction4_Form2States`, `PartyAction5_Form1States` |
| 3 | the 23 others |
| 5 | `PartyAction4_Form2State0Steps`, `PartyAction5_Form1State0Steps` |

The tables lie back to back from `0x65FAC8` to `0x65FC3B`, with round
eight's `PartyAction5_Form0States` `0x65FBC0` and `PartyAction5_Forms`
`0x65FC18` among them.

## 3. The fuzz (`rest_1b_fuzz.cpp`)

`scenario_harness::Run` in field mode (`g.field`), **4,000 rounds per
function**; `BOF3X_R1B_ONLY=<name>` runs the clones whose name contains it.
The 29 dispatchers and 14 states are `kSprite`, the four cell handlers
`kCall` with `ret_mask` 0xFF (all 12 of their `E8` sites test al first). The
29 tables are `DataTable`s (their entries recorders on both sides;
`sprite_span` is 0 and each dispatcher's index is seeded below its own
table's count, the form word too). The harness names each clone "outside the
field runs" in the log (the band is not one of round twelve's runs; named,
not refused).

**The callees** (the group's own stand-ins, registered before the harness's
standard rows; masks by what each callee reads):

| Callee | Masks | Answers |
|---|---|---|
| `PartyAction_TargetAhead`, `_BlockedAhead` (R0A) | - | `kFlag` |
| `PartyAction_Kind30Ahead` (R0A) | - | 0xFF or 0..0x13 (what R0A's answers) |
| `PartyAction_MemberOnEffect`, `_MemberBeyondEffect` (R0A) | the index byte | `kFlag` |
| `PartyAction_SideProbes`, `Effect_SpawnAtCellHigh` (R0A) | -; state byte, x and z words | garbage |
| `Sprite_ObjectAt` | whole | 0xFF half the time, else 0..0x21 (its 34 objects, the edges often) |
| `AreaMap_ByteAt` | 16 bits each | 0xF0..0xF9, 0xEF, 0, 0xFF, 0x72, or any byte |
| `Field_EffectAhead` | - | 0xFF half the time, else 0..19 |
| `MapView_SlopeAt` | whole, whole, the direction's byte | `DamageScratch`'s flag 0 a third of the time; the low word about 0x40 and at the s16 limits |
| `MapView_GroundAt` | whole | the low word at, about or far from `S+0x3E` |
| `Effect_SpawnAtCell` | state byte, x and z words | garbage |
| `Field_GiveZenny`, `Sprite_FlashClut` | whole | garbage |
| `AreaMap_ClearCell` | 16 bits each | garbage |
| the group's four cell handlers, where the states call them | 16 bits each | `kFlag` |

Standard rows used as they are: `Sound_PlayEffect`, `Sprite_EnsureAnimation`,
`Sprite_ScriptTick`, `Sprite_ScriptTickOnce`, `Effect_FindFree` (0xFF or
0..0x13), `Rand` (its hint seeded per round), `Item_NamePtr` (answers into the
harness's text buffer), `Inventory_Add`, `Msg_OpenSystem`, `Char_LoseHp`,
`Field_JumpStart`, `Field_LeaderStepTick`.

**The state.** Field mode's standard regions and the group's two:
`Field_DirectionSteps` `0x6697B0` (0x40, seeded at its boundaries as R0A's)
and `Text_Records` `0x904CE0` (the 16 bytes the item names are copied into).
23,676 bytes in 40 regions.

**The seeds** (after the harness's random fill): the steps table (half the
time the exe's shape, else boundaries); `S`'s direction (0..7 two times in
three, else 8, 9, 15, 0x80, 0xFF or random), position (cells 0, 1, 2, small,
0x7FFF, 0x8000, 0xFFFF, 0xFFFE with a fraction 0 three times in eight),
height, `+0xA` (0, 1 three times in seven, 2, 0xFF, random), `+0xB` 0..19,
`+0x58` (0xA three times in eight, else 9, 0xB, 0, 0x10A, random);
`Field_State +0x138` (0 three times in eight, else 1, 2, 3, 0xFE, random),
`Field_Kind2Hold` (0 three times in five), `Field_InputFlags` (0, 2, 4, 6, 1,
0xF9, random), `Rand`'s hint (0xD, 0xE, 0xF, 0xC, 6, 7, 0xB, 5, 0x10, 0x14),
every effect record's `+0` and `+1` (0, 1, 2 or random); the dispatcher's
index below its count. The cell handlers' x and z low words 0, 1, 0x40,
0x7FFF, 0x8000, 0xFFFF, small or random under random upper halves.

**The disturbance** (the group's case of the harness's, from its hash only):
`S`'s direction, x or z, height, `+0xA`, `+0xB` (0..19), the form or script
word; `Field_State +0x89` / `+0x138`; an effect record's `+0`, `+1`, `+8`,
`+0xA`, or the record `+0xB` names; `Field_Kind2Hold`; `Field_InputFlags`; a
dword of the steps table. The harness's own moves `S` among the sprite
records and its state bytes.

**The first run** found one mismatch class, the fuzz's: the
`Sprite_ObjectAt` stand-in picked its edge answers with `Pick`, which draws
the seed's stream (`Next()`), so the two passes answered differently (384
rounds in Resolve, Form1Begin and Hit). It draws from `Noise()` now.

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_1b`, exit 0): 188,000 rounds over 47 functions, 296,192
calls to the stand-ins, **0 mismatches**; every entry of the 29 tables
reached (each handler recorder 786..11,900 calls); `AreaMap_ByteAt` 16,000,
`Sprite_ObjectAt` 5,465, `Field_EffectAhead` 3,452, `MapView_SlopeAt` 19,894,
`MapView_GroundAt` 3,099, `Effect_SpawnAtCellHigh` 3,629, `Effect_SpawnAtCell`
1,060, `Field_GiveZenny` 86, `Sprite_FlashClut` / `Char_LoseHp` 321,
`Inventory_Add` 946, `Rand` 3,024, the cell handlers' recorders 1,170..2,398.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
1,019 lines of `0 MISMATCHES` and no other mismatch line, `inject: 8702
ours, 0 left original` (8,655 + 47); `rest_1b` there 188,000 rounds, 295,675
calls, 0 mismatches. **With `BOF3X_WIDE=1`**: exit 0, the same 1,019 lines
and counts. Each passed on its first run (neither died silently).
`tools/ledger_check.py`: 72 entries, 0 errors.

## 4. Divergence

None. `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no byte in
`0x51D710..0x51F202` (a grep of every `0x51D...`..`0x51F...` address,
2026-10-04). No full-frame fill: nothing in the band draws.

## 5. What the cut said, settled

- **Extents**: `band_rows.py` read the 47 (5,147 bytes against the cut's
  5,645: 42 differ by padding only, none by code); each checked by hand to its
  `ret` or tail `jmp`. The tool lists no code of the band no list has.
- **Hidden starts**: 43, each an entry by address - a cell of one of the
  tables above, of `Field_ActionBySet` / `Field_FormActions`, or of another
  set's table (`0x51F0E0`, `0x51F130`, `0x51DE20`, `0x51F1D0`, `0x51F1F0`) -
  none a case, none a shared tail. Their recorded hosts: R1A's `0x51D4E0`
  (`0x51D710..0x51D950`), ours `PartyAction_MemberOnEffect` `0x51DD70` (R0A:
  `0x51DDE0..0x51E0D0`), ours `Field_CellPickup` `0x51EBD0`
  (`0x51ECF0..0x51EDF0`), and this group's `0x51DAA0`, `0x51E1B0`, `0x51E6C0`,
  `0x51EF30`. No host's code contains any of them as a fall-through: the code
  before each start ends at a `ret` and padding (R1A's `0x51D6D0` at
  `0x51D70C`, `PartyAction_MemberOnEffect` at `0x51DDD9`, `Field_CellPickup` at
  `0x51ECEE`, ours' own hosts at theirs).
- **The cut's columns**: the "PLP (11)" rows are sets 3's dispatchers and form
  0 (`0x51D750..0x51DE20`); only `0x51DE20` has a PSX twin (by callers, six PLP
  overlay sections: `0x801CF018`, `0x801CE820`, `0x801CF548`, `0x801CEA3C`,
  `0x801CF91C`, `0x801CFD40`, `analysis/pairs_propagated.json`); the sibling's
  `names/` and `symbols.toml` name none of them, so every name is the PC
  code's. The four `hypothesis` rows (`0x51E480`, `0x51ED50`, `0x51EDF0`,
  `0x51EF30`) are functions as the others. "Table 0x65FB9C read by 0x51E8B0"
  and its kind are right; the "(4)", "(3)" are rows per hint.
- **The harness's rows**: none of the 47 addresses has a row in
  `scenario_harness*.cpp` or `boss_harness*.cpp` (grep), so taking them breaks
  no harness row.

## 6. Latent defects and ranges (Capcom's, described, not fixed)

- **Every dispatcher's index is unchecked** (29): the byte `+2` / `+3` or the
  word `+0x2C` times 4 from its table. Past a 2- or 3-entry table the original
  jumps through the next table's code pointers (another state's handler), and
  past the run into data. Ours **aborts at the table's own count** (its
  `symbols.toml` count), before the read, with a message. Every writer this
  group reads keeps the indexes inside (section 2); whether another writes the
  form word above 2 is not established here. **One rule for wave one's seven
  groups** (2026-10-05, the round's end, `round-14-review.md` item 7): R1A,
  R1B and R1C read on into the next table until then, R1D..R1G aborted at the
  count; all seven abort at the count now, as the round's rule since round
  nine has it - loudly, before the fault or the read it guards, never by
  running another table's handler.
- **An object index past the 34** (the Resolve, Form1Begin and Hit states):
  `Sprite_ObjectAt`'s answer marks `Sprite_Objects` below 0x1E and
  `Sprite_ObjectsExtra` from it - signed in the Resolve and Hit states (0x80..
  0xFE would index before `Sprite_Objects`), unsigned in Form1Begin (0x22..0xFE
  past `Sprite_ObjectsExtra`). `Sprite_ObjectAt` answers 0..0x21 or 0xFF
  (`symbols.toml`), so no play reaches it; **ours aborts** with a message.
- **Effect indexes used unchecked**: `Field_EffectAhead`'s (Hit, `movsx`),
  `PartyAction_Kind30Ahead`'s (Form1Begin), `Effect_FindFree`'s
  (`SpawnKind3A`), and `S+0xB` (`PartyAction5_Form1Wait`). The first three
  answer 0..19 or 0xFF, tested first; ours aborts past 19. `+0xB` is a small
  counter in the steps around Wait (Aim writes 0, the cell handlers 1 or 2,
  R1A's `0x51D440` 1..3): Wait reads one of the first records whatever effect
  object Hit found (Hit keeps that in `+6`). Ours aborts on a `+0xB` past 19;
  no writer this group read gives one.
- **`Field_Request` = 2 with nothing opened**: a `CellHit` on 0xF6 / 0xF7 whose
  `Rand & 0xF` is 7..0xB (5 in 16) sets `Field_Request` 2 - which the other
  branches set after opening a system message - with no message. What the
  field does with it then is not established here (the owner's to see).
  Reproduced.
- **The direction is not masked** (Begin's slope, the two-steps-ahead points):
  as R0A's section 5; reproduced, the seeds cover it.
- **Ranges, not defects**: the cell one on is the high word + 1 (the
  original's dword carries into its uninitialised upper half; every callee
  reads 16 bits); the form's sound is a 16-bit add.

## 7. Calls across groups

**Outbound**: 26 sites into R0A (merged), by name through `rest_0a.h`
(`band_rows.py --edges`): `PartyAction_TargetAhead` 4, `_Kind30Ahead` 2,
`_MemberOnEffect` 2, `_MemberBeyondEffect` 2, `_BlockedAhead` 4,
`_SideProbes` 2, `Effect_SpawnAtCellHigh` 10. Every other callee is already
ours, by name; `Rand` is Capcom's C runtime (the symbol's macro). **No
raw-address call**, so there is no `rest_1b_callees.h`. Table entries that are
other groups' functions (R1A's `0x51BEB0`, `0x51C490`, `0x51CC40..0x51D220`,
`0x51CC60`, `0x51D440`, `0x51D6D0`; R1C's `0x51F210`, `0x51F3D0`, `0x51F850`,
`0x51FC80`, `0x520350`, `0x520840`; R1D's `0x521C40`, `0x5224D0`; R1E's
`0x522DE0`, `0x5239F0`; R1F's `0x523F10`) are reached through the tables, read
in place - no reference in our source.

**Inbound from outside the group** (for the rebinding pass; all through
`.data`, none by `E8`):

| Reached by | Owner | Functions |
|---|---|---|
| `Field_ActionBySet` `[2]`, `[3]`, `[4]` (ours, `Field_ActionState`) | ours | `PartyAction2_ByForm`, `PartyAction3_ByForm`, `PartyAction4_ByForm` |
| `Field_FormActions` `[2]`..`[5]` (ours, `Field_FormActionState`) | ours | the four `PartyFormActionN_ByForm` |
| `PartyAction5_Forms[1]` `0x65FC1C` (`PartyAction5_ByForm`, round eight) | ours | `PartyAction5_Form1` |
| set 6's `0x65FC88[0]`, `0x65FC94[0]` (read by `0x51FA70`, `0x51FA90`) | R1C | `PartyFormAction6_Form0`, `PartyAction6_Form0` |
| `0x65FA48` (`PartyAction1_Form0States`' neighbour run), `0x65FC4C`, `0x65FD14`, `0x65FED4`, `0x65FFDC`, `0x660058` | R1A, R1C and later sets' dispatchers | `PartyAction_SpawnKind3A` |
| `0x65FE88`, `0x65FE90` | a later set's step table (the run after `0x65FC24`; its dispatcher another wave-one group's) | `PartyAction5_Form1Again`, `PartyAction5_Form1Wait` |

## 8. The rebinding

`band_rows.py --refs --group R1B` and a grep of the 47 addresses and the 29
tables in `src/`: one reference, a comment in `field_hidden.cpp`
(`PartyAction5_ByForm`'s, "0x51E910, 0x51ED10, 0x5226D0"), now
`PartyAction5_Form1` on its own line. **Left raw, on purpose**: the
`symbols.toml` evidence of `Field_ActionBySet`, `PartyAction5_Forms` and
`PartyAction5_Form0States` (their entry lists, the record of round eight's
read) and `docs/field_hidden.md`'s. No `_callees.h` constant of another group
names any of the 47 (grep), and **no harness row lists any as theirs**.
Nothing in a file another group of this round is writing.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns are empty for 42 rows; the
five `0x51ECF0..0x51EDF0` carry the world-map mark of their recorded host,
`Field_CellPickup` `0x51EBD0` (an upper bound: the host, not the start). No
first-call or counted trace under `analysis/calltrace` names any of the 47 (a
grep of every file: the hits are the extent lists `entries*.txt`). **Fuzz
only.** No live run was made (the brief); the coordinator's state hash after
the merge covers whatever a route reaches.

## 10. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04): 47 lines, the read
extents. Four of them (`0051DAA0 11F`, `0051E1B0 11F`, `0051E6C0 188`,
`0051EF30 188`) are the smaller extent beside a host line that ran over the
hidden starts after its `ret` (`2C6`, `50C`, `250`, `280`); the file's rule
(duplicates keep the smaller extent) takes them.

## 11. Controls

`r1b/controls.py` (scratch): each plant replaces a string that occurs exactly once
in `rest_1b.cpp`, rebuilds, runs the self-test on the clones whose name holds
the filter (`BOF3X_R1B_ONLY`), restores the file and rebuilds. **88 planted:
85 refused by a mismatch, C06 by a fault, 2 not refused - equivalent
mutants, each with a near variant refused.** The count is the rounds that mismatched (of 4,000 per clone run),
the round the first refusal came in; "fault" is a control whose wrong index
sent the run through a table entry that is not a recorder after the first
mismatch was logged (C03, C04) or before (C06): the process died, exit
0xC0000005.

| # | Run (`_ONLY`) | Plant | Refused |
|---|---|---|---|
| C01 | `PartyFormAction2_ByForm` | set 2's form-action table read as its action table | 4,000 (first round 0) |
| C02 | `PartyAction3_Form1` | set 3's form 1 through the form-action form 1 table | 4,000 (first round 0) |
| C03 | `PartyAction4_Form0` | the state dispatchers index by +3 | fault after the first mismatch, round 0 |
| C04 | `Form2State0` | the step dispatchers index by +2 | fault after the first mismatch, round 0 |
| C05 | `ByForm` | the form read as the byte +0x2C, not the word | **not refused**: equivalent - the form word is seeded below its table's 3 (an index past it would jump through the next table), so its high byte is 0 and the byte reads the same; near variants C06 and C88 refused |
| C06 | `ByForm` | the form word read from +0x2A (C05's near variant) | fault (exit 0xC0000005, no mismatch logged first) |
| C07 | `PartyAction6_Form0` | set 6's action form 0 through the form-action table | 4,000 (first round 0) |
| C08 | `Form1State1` | set 5's state 1 steps through set 4's | 4,000 (first round 0) |
| C09 | `PartyAction3_Form0Begin` | the first turn +1, not -1 | 1,930 (first round 1) |
| C10 | `PartyAction3_Form0Begin` | the turn skipped on bit 1, not bit 0 | 1,683 (first round 3) |
| C11 | `PartyAction3_Form0Begin` | the second turn +3 | 647 (first round 1) |
| C12 | `PartyAction3_Form0Begin` | the third turn -1 | 213 (first round 16) |
| C13 | `PartyAction4_Form0Begin` | the slope two steps ahead, not one | 3,326 (first round 0) |
| C14 | `PartyAction3_Form0Begin` | steep from 0x40, not above it | 177 (first round 47) |
| C15 | `PartyAction3_Form0Begin` | the steep pose + 0x45 | 1,010 (first round 0) |
| C16 | `PartyAction3_Form0Begin` | +0xA = 6 | 2,990 (first round 1) |
| C17 | `PartyAction4_Form0Begin` | the side probe clears when level too | 150 (first round 12) |
| C18 | `PartyAction3_Form0Begin` | the second side probe direction 7 | 2,990 (first round 1) |
| C19 | `PartyAction3_Form0Begin` | the form's sound + 0x101 | 2,990 (first round 1) |
| C20 | `PartyAction3_Form0Begin` | +0xB = 1 at the end | 4,000 (first round 0) |
| C21 | `PartyAction4_Form2Again` | the pose's half turn as >> 1 (direction 0 gives -1) | 337 (first round 16) |
| C22 | `PartyAction3_Form0Begin` | Sprite_Current not re-read for the side probe's ground test | 40 (first round 49) |
| C23 | `PartyAction3_Form0Resolve` | the object's +0x80 |= 2 | 548 (first round 10) |
| C24 | `PartyAction4_Form0Resolve` | Sprite_ObjectsExtra one record on | 120 (first round 22) |
| C25 | `PartyAction3_Form0Resolve` | object 0x1E taken as Sprite_Objects' | 31 (first round 197) |
| C26 | `PartyAction3_Form0Resolve` | Sprite_ObjectAt margin 1 | 1,726 (first round 2) |
| C27 | `PartyAction4_Form0Resolve` | x's fraction tested & 0xFFFE | 52 (first round 147) |
| C28 | `PartyAction3_Form0Resolve` | the z cell one on in x too | 256 (first round 10) |
| C29 | `PartyAction3_Form0Resolve` | the first cell's answer ignored | 1,020 (first round 2) |
| C30 | `PartyAction4_Form0Resolve` | the count-down ends at 1 | 2,281 (first round 2) |
| C31 | `PartyAction3_Form0Resolve` | three steps ahead, not two | 1,473 (first round 2) |
| C32 | `PartyAction3_CellPickup` | zenny from 0xE | 22 (first round 90) |
| C33 | `PartyAction4_CellPickup` | 6 zenny on 0xF | 12 (first round 31) |
| C34 | `PartyAction3_CellPickup` | Field_InputFlags bit 2 only | 13 (first round 122) |
| C35 | `PartyAction4_CellPickup` | times 11 | 13 (first round 333) |
| C36 | `PartyAction3_CellPickup` | the zenny effect state 2 | 55 (first round 31) |
| C37 | `PartyAction4_CellPickup` | item 0x57 | 253 (first round 43) |
| C38 | `PartyAction3_CellPickup` | no object free: the cell not cleared | 12 (first round 351) |
| C39 | `PartyAction4_CellPickup` | Field_Request 1 after the item | 239 (first round 43) |
| C40 | `PartyAction3_CellPickup` | twelve bytes of the name | 253 (first round 43) |
| C41 | `PartyAction3_Form1Begin` | the effect object's +0xA, not +0xB | 437 (first round 6) |
| C42 | `PartyAction4_Form1Begin` | the pose +8 + 9 | 437 (first round 6) |
| C43 | `PartyAction3_Form1Begin` | Field_State +0x128 = 3 | 437 (first round 6) |
| C44 | `PartyAction4_Form1Begin` | Field_ScriptFlags bit 13 | 318 (first round 6) |
| C45 | `PartyAction3_Form1Begin` | +9 one up | 437 (first round 6) |
| C46 | `PartyAction3_Form1Begin` | Field_State +0x137 = 2 | 437 (first round 6) |
| C47 | `PartyAction4_Form1Begin` | +0x138 tested & 2 | 78 (first round 96) |
| C48 | `PartyAction3_Form1Begin` | Sprite_ObjectAt margin 0 | 129 (first round 17) |
| C49 | `PartyAction4_Form1Begin` | both member tests needed | 3,822 (first round 0) |
| C50 | `PartyAction3_Form1Begin` | the pose from Sprite_Current read before the sound | 15 (first round 6) |
| C51 | `PartyAction4_Form1Begin` | the object marked signed (0x80.. before Sprite_Objects) | **not refused**: equivalent - `Sprite_ObjectAt` answers 0..0x21 or 0xFF, where the signed and unsigned compares agree (ours aborts on the rest); near variants C24, C25 refused |
| C52 | `SpawnKind3A` | script position 0xB | 1,968 (first round 2) |
| C53 | `SpawnKind3A` | kind 0x3B | 1,441 (first round 2) |
| C54 | `SpawnKind3A` | +6 = 0, not +7 | 1,512 (first round 2) |
| C55 | `SpawnKind3A` | the sound only with an object | 71 (first round 113) |
| C56 | `PartyAction4_Form2Aim` | +0xB = 1 | 4,000 (first round 0) |
| C57 | `PartyAction4_Form2Aim` | no side probes | 4,000 (first round 0) |
| C58 | `PartyAction5_Form1Aim` | +7 = 0, not +6 | 4,000 (first round 0) |
| C59 | `PartyAction5_Form1Aim` | +0xA = 9 | 4,000 (first round 0) |
| C60 | `PartyAction4_Form2Aim` | the turn by PartyAction_TargetAhead | 1,930 (first round 1) |
| C61 | `PartyAction4_Form2Hit` | +0xA 1 counted as 0 | 1,726 (first round 2) |
| C62 | `PartyAction4_Form2Hit` | no form sound first | 1,726 (first round 2) |
| C63 | `PartyAction4_Form2Hit` | the effect object's +0xA = 2 | 844 (first round 11) |
| C64 | `PartyAction5_Form1Hit` | +3 one on, not two, for an effect object | 898 (first round 10) |
| C65 | `PartyAction4_Form2Hit` | the mark's sound 0x10C | 434 (first round 2) |
| C66 | `PartyAction5_Form1Hit` | Sprite_ObjectAt margin 1 | 824 (first round 2) |
| C67 | `PartyAction5_Form1Hit` | set 5's with the form sound first | 1,726 (first round 2) |
| C68 | `PartyAction4_Form2Hit` | the effect object faces +9 | 842 (first round 11) |
| C69 | `PartyAction5_Form1Hit` | +6 written through Sprite_Current read before the sound | 30 (first round 145) |
| C70 | `PartyAction4_CellHit` | 0xF5 for 0xF4 | 482 (first round 0) |
| C71 | `PartyAction5_CellHit` | the second effect above 4 | 105 (first round 36) |
| C72 | `PartyAction4_CellHit` | the second effect state 5 | 169 (first round 1) |
| C73 | `PartyAction5_CellHit` | 0xF8 for 0xF7 | 496 (first round 10) |
| C74 | `PartyAction4_CellHit` | the item from 7 | 31 (first round 27) |
| C75 | `PartyAction5_CellHit` | the damage from 0xB | 31 (first round 10) |
| C76 | `PartyAction4_CellHit` | item 0x28 | 217 (first round 51) |
| C77 | `PartyAction5_CellHit` | +0xB = 3 after the item | 217 (first round 51) |
| C78 | `PartyAction4_CellHit` | two points of damage | 146 (first round 111) |
| C79 | `PartyAction5_CellHit` | message 0xDA | 146 (first round 111) |
| C80 | `PartyAction4_CellHit` | Field_Request 1 | 501 (first round 10) |
| C81 | `PartyAction5_CellHit` | the blocking codes answer 0 | 702 (first round 1) |
| C82 | `PartyAction4_CellHit` | the sound before the effect object | 501 (first round 10) |
| C83 | `PartyAction4_Form2Again` | +0xA = 0xC | 4,000 (first round 0) |
| C84 | `PartyAction5_Form1Again` | +3 = 2 | 4,000 (first round 0) |
| C85 | `PartyAction5_Form1Wait` | state 2, not 1 | 937 (first round 8) |
| C86 | `PartyAction5_Form1Wait` | Field_Kind2Hold set | 2,350 (first round 0) |
| C87 | `PartyAction5_Form1Wait` | free and in state 1 both | 1,066 (first round 3) |
| C88 | `ByForm` | the form word one on, inside the table (C05's and C06's near variant) | 28,000 (first round 0) |

**2026-10-05, under the repaired disturbance (round fourteen's review item
1).** The group case is now `sh::DisturbCase(h, 12)` (`b9dfe34`), so the cases
0 (`+8`), 3 (`+0xA`), 6 (`Field_State` +0x89 / +0x138) and 9
(`Field_InputFlags`), dead under `h % 12`, run. **C01..C88 re-run at
`451edeb` with the fuzz change below: 88 planted, 83 refused by a count, C03,
C04 and C06 by the same fault, C05 and C51 not refused (the same equivalent
mutants).** No count went to 0. Moved against the table: C09 1,914, C11 640,
C13 3,327, C21 336, C22 39, C34 15, C47 76, C48 116, C52 1,966, C53 1,436,
C54 1,508, C55 72. New controls C89..C95, one or more per formerly dead case;
each also run with the old switch put back (`h % 12`) to show what the case
adds. Case 0: C89..C91 refused, 27 / 62 / 26 rounds under the old switch (the
harness's move of `Sprite_Current` refuses a value read before a call too), 30
/ 69 / 33 under the new. Case 3: no function re-reads `+0xA` after a call
(each reads it at entry, or decrements and tests it with no call between);
the case does test where a store sits against a call, so C92 is a store-order
control (180 old, 206 new). Cases 6 and 9: **C93..C95 were not refused, 0
rounds under both switches** - the fuzz was blind to them, not ours wrong. The
three re-reads sit on paths that run in a few hundred rounds of 4,000 (C78,
C47, C34), and the group case lands on about one call in 288, so the case
never moved the cell in the calls between. **Fuzz change:** the stand-ins of
`Sprite_FlashClut`, `PartyAction_Kind30Ahead` and `Effect_SpawnAtCell` now move
`Field_State` +0x89, +0x138 and `Field_InputFlags` (respectively) a quarter of
the time from the noise (`FxFlash`, `FxKind30`, `FxSpawn` in
`rest_1b_fuzz.cpp`); the shadow passes, 188,000 rounds, 0 mismatches; C93..C95
are then refused (counts below), and C01..C88 above were run on it. A
second attribution, each control run with only its own case switched off
(`if (sh::DisturbCase(h, 12) == N) return;` planted at the top of `Disturb`):
C89..C92 give 27, 62, 26, 180, so cases 0 and 3 add 3, 7, 7 and 26 rounds;
C93..C95 give 28, 23, 8 - the same as with the case on, so cases 6 and 9 add
nothing to these controls and the stand-ins' moves are what refuses them.

| # | Run (`_ONLY`) | Plant | Refused |
|---|---|---|---|
| C89 | `Form0Begin` | case 0, `+8`: `TurnUntil`'s second turn from the direction read before the probe | 30 (first round 305) |
| C90 | `Form0Begin` | case 0, `+8`: `Begin`'s steep pose from the direction read before `MapView_SlopeAt` | 69 (first round 83) |
| C91 | `PartyAction4_Form2Hit` | case 0, `+8`: the effect object's +8 from the direction read before the 0x10B sound | 33 (first round 36) |
| C92 | `Form0Begin` | case 3, `+0xA`: `Begin`'s +0xA = 5 stored before the pose's call, not after (store order) | 206 (first round 4) |
| C93 | `PartyAction4_CellHit` | case 6, `Field_State` +0x89: the member read before the spawn and the flash | 28 (first round 127); 0 before the fuzz change |
| C94 | `PartyAction4_Form1Begin` | case 6, `Field_State` +0x138: read before `PartyAction_Kind30Ahead` | 23 (first round 93); 0 before the fuzz change |
| C95 | `PartyAction3_CellPickup` | case 9, `Field_InputFlags`: read before `Effect_FindFree`, the spawn and `Rand` | 8 (first round 31); 0 before the fuzz change |
