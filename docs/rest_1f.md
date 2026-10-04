# Group R1F: party sets 16..18's field actions, a raised sprite's cell ahead, the leader's state 9 stage 0

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, on the
round branch's tip `ba2c3c3`. **49 functions ours** (`src/game/rest_1f.cpp`,
declarations in `src/game/rest_1f.h`, shadow name `rest_1f`): the cut's 49
rows for R1F (`analysis/round14_cut.tsv`), each read to its last instruction
with capstone and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
392,000 rounds, 0 mismatches. CONTROLS_SUMMARY No recorded route enters any
of the 49 (section 9): fuzz only.

The band is three things, not one (the cut's `unit` column guessed "field
core" for all 49):

| Part | Rows | What |
|---|--:|---|
| party sets 16, 17, 18 | 38 | the entries of `Field_FormActions` / `Field_ActionBySet` for the three sets, 21 dispatchers below them, their state handlers, three cell pickups and set 18's cell strike, three state handlers other sets' tables share |
| a raised sprite's cell ahead | 6 | `Field_CellAhead`'s other half (it tail-jumps to `0x527640` when `+0x70` is set) and five helpers |
| the leader's state 9, stage 0 | 5 | `Field_LeaderStates[9]`, its dispatcher through `LeaderPanel_Stages`, and stage 0's three steps (E1E's `LeaderPanel_*`, [`effect_1e.md`](effect_1e.md)) |

No start is a jump-table case, a shared tail or data: every one is entered
by a `.data` cell, an `E8` or (`0x527640`) a tail `jmp`, and each has its own
`ret`. The tool's extents (`band_rows.py --byte-tables`, through the round's
`band14.py`) agree with the code for all 49; the cut's sizes include the
padding after 38 of them. No code in the band is outside a group: the tool
reports "0 not listed" for R1F. The gap `0x525370..0x527640` is other
groups' (`0x525390..` FC3's, ours since round twelve; `0x526DB0..0x527631`
member-sprites').

## 1. What each function does

"Sprite_Current" is the sprite whose state runs (the leader's `ObjTrio`
record in play); `+2` / `+3` its state / step bytes, `+8` its facing, `+0x2C`
a word (the form), `+0x34` / `+0x38` its 16.16 x / z, `+0x36` / `+0x3A` their
cell words, `+0x3E` its height word, `+0x70` the raised flag. "A row" is one
of `Field_DirectionSteps` (`0x6697B0`), read in place with the facing
unmasked (as R0A's helpers read it). "The pose (d)" is
`Sprite_EnsureAnimation(d)`; `HalfTurn(f)` is `(f - 1) / 2` as `dec; cdq;
sub; sar` computes it (0 for 0).

### 1.1 The dispatchers (23 + `LeaderPanel_Run`)

Each is `mov ecx, [Sprite_Current]; xor eax, eax; mov al / ax, [ecx + k];
jmp [eax * 4 + table]` - 0x12 bytes by a byte, 0x13 by the word `+0x2C` -
with no bound. Ours calls the table's entry in place (the fuzz swaps the
cells for recorders) and aborts past the table's own entries (section 7).

| Function | Entry | By | Table | Reached from |
|---|---|---|---|---|
| `PartyAction16_FormAction` | `0x5243D0` | `+0x2C` | `PartyAction16_FormActionForms` | `Field_FormActions[16]` |
| `PartyAction16_ByForm` | `0x5243F0` | `+0x2C` | `PartyAction16_Forms` | `Field_ActionBySet[16]` |
| `PartyAction16_FormAction2` | `0x523FB0` | `+2` | `PartyAction16_FormAction2States` | `PartyAction16_FormActionForms[2]` |
| `PartyAction16_Form2` | `0x523FD0` | `+2` | `PartyAction16_Form2States` | `PartyAction16_Forms[2]` |
| `PartyAction17_FormAction` | `0x524930` | `+0x2C` | `PartyAction17_FormActionForms` | `Field_FormActions[17]` |
| `PartyAction17_ByForm` | `0x524950` | `+0x2C` | `PartyAction17_Forms` | `Field_ActionBySet[17]` |
| `PartyAction17_FormAction0` / `1` / `2` | `0x524410` / `0x524450` / `0x5248F0` | `+2` | `PartyAction17_FormAction0States` / `1` / `2` | `PartyAction17_FormActionForms[0..2]` |
| `PartyAction17_Form0` / `1` / `2` | `0x524430` / `0x524470` / `0x524910` | `+2` | `PartyAction17_Form0States` / `1` / `2` | `PartyAction17_Forms[0..2]` |
| `PartyAction18_FormAction` | `0x525330` | `+0x2C` | `PartyAction18_FormActionForms` | `Field_FormActions[18]` |
| `PartyAction18_ByForm` | `0x525350` | `+0x2C` | `PartyAction18_Forms` | `Field_ActionBySet[18]` |
| `PartyAction18_FormAction0` / `1` / `2` | `0x524970` / `0x524E50` / `0x525270` | `+2` | `PartyAction18_FormAction0States` / `1` / `2` | `PartyAction18_FormActionForms[0..2]` |
| `PartyAction18_Form0` | `0x524990` | `+2` | `PartyAction18_Form0Subs` | `PartyAction18_Forms[0]` |
| `PartyAction18_Form0Sub0` | `0x5249B0` | `+3` | `PartyAction18_Form0Sub0Steps` | `PartyAction18_Form0Subs[0]` |
| `PartyAction18_Form0Sub1` | `0x524D40` | `+3` | `PartyAction18_Form0Sub1Steps` | `PartyAction18_Form0Subs[1]` |
| `PartyAction18_Form1` / `2` | `0x524E70` / `0x525290` | `+2` | `PartyAction18_Form1States` / `2` | `PartyAction18_Forms[1..2]` |
| `LeaderPanel_Run` | `0x528880` | `+2` | `LeaderPanel_Stages` (E1E's, 12) | `Field_LeaderStates[9]` |
| `LeaderPanel_S0` | `0x5288A0` | `+3` | `LeaderPanel_Stage0Steps` | `LeaderPanel_Stages[0]` |

`Field_FormActions` is called by `Field_FormActionState` (the leader's state
6) and `Field_ActionBySet` by `Field_ActionState` (state 10), both by the
party set `0x90412C & 0x7F` ([`field_e1.md`](field_e1.md)). The names
`PartyActionN_*` follow `PartyAction5_ByForm` / `PartyAction5_Form0`
(field_hidden): N is the set, `Form` the `+0x2C` value, then the `+2` state
and (set 18's form 0) the `+3` step. What a set, a form or an action is in
play is not stated here: nothing in the code names it.

### 1.2 The state handlers

**Three shapes, each byte-identical across the three sets** (capstone,
the disassembly normalised for in-function jumps and call targets, scratch
`r1f/cmp.py`): one implementation each in ours, exported under each name.

- **`PartyAction16_Form2Begin` `0x523FF0`, `PartyAction17_Form1Begin`
  `0x524490`, `PartyAction18_Form1Begin` `0x524E90`** (0x1D4 bytes, state 0 of
  their form) - an even facing turned one eighth back (`-1 & 7`); when
  `PartyAction_TargetAhead` finds nothing that way, two on (`+2`), and nothing
  there either, back (`-2`); an odd facing is kept, unmasked. The point one
  row ahead: `MapView_GroundAt` there less the height word (16 bits) is the
  **rise**; `MapView_SlopeAt` there in the facing (its answer unused). The
  sloped flag `0x903850` set and the rise above 0x40 (s16): the pose
  `HalfTurn + 0x46` and `+2` one on. Otherwise `+0x2B = 1`, the two side
  probes inline (PartyAction_SideProbes' code, rows 3 and 5 by address: a
  steep slope there and the ground above the height word clear `+0x2B`),
  `Sound_PlayEffect(u16 +0x2C + 0x100)`, the pose `HalfTurn + 0x42`, `+0xA =
  5`. Then `+0xB = 0` and `+2` one on. It is `PartyAction5_Form0Begin`'s
  shape (field_hidden) with the ground's rise where that one tests the
  slope's own word.
- **`PartyAction16_Form2Resolve` `0x5241D0`, `PartyAction17_Form1Resolve`
  `0x524670`, `PartyAction18_Form1Resolve` `0x525070`** (0xDB, state 1) -
  `PartyAction5_Form0Resolve`'s bytes with their own pickup: `+0xA` down; at 0
  the point two rows ahead; the object `Sprite_ObjectAt(point, 0)` finds gets
  bit 0 of its `+0x80`; the pickup on the point's cell, then (only while the
  earlier found nothing) the cell one on in x when x has a fraction, one on
  in z when z has one; `+2` one on. `Sprite_ScriptTickOnce` every time.
- **`PartyAction16_CellPickup` `0x5242B0`, `PartyAction17_CellPickup`
  `0x524750`, `PartyAction18_CellPickup` `0x525150`** (0x11F, `(x, z)`, al) -
  `Field_CellPickup` `0x51EBD0`'s code with **one constant changed: the found
  zenny times 20 where that one has 10** (`imul cl` with `cl` 0x14, not 0xA).
  Cell 0xF2: with an effect object free, `Effect_SpawnAtCell(0, x, z)`; a
  `Rand` nibble of 13..15 finds 2 (5 on 15), twenty times that when
  `Field_InputFlags & 6` and a second `Rand & 3` is 0;
  `Field_GiveZenny`, `Effect_SpawnAtCell(1, x, z)`; `+0xB = 1`. Cell 0xF8:
  `Effect_SpawnAtCell(0, x, z)`, item 0x56's name to `Text_Records`,
  `Inventory_Add(0, 0x56, 1)`: taken - sound 0x106 and `Msg_OpenSystem(2)`,
  else `Msg_OpenSystem(3)`; `Field_Request = 2`, `+0xB = 1`. Both clear the
  cell (`AreaMap_ClearCell`) and answer 1; else 0.

**Set 18's form 0** (`PartyAction18_Form0Subs` by `+2`, each sub by `+3`):

- **`PartyAction18_Form0Sub0Begin` `0x5249D0`** (step 0) - the facing turned
  as the Begin states turn it, on `PartyAction_BlockedAhead` (an open way
  turns on); `PartyAction_SideProbes`; the pose `HalfTurn + 0x42`; `+0xB = 0`,
  `+0xA = 0xB`, `+3 = 1`.
- **`PartyAction18_Form0Sub0Strike` `0x524A60`** (step 1) - `+0xA`, when not
  0, down; reaching 0: `Sound_PlayEffect(u16 +0x2C + 0x100)`;
  `Field_EffectAhead` finds an effect object: sound 0x10B, its `+8` = the
  facing, its `+0xA` = 1, `Sprite_Current +6` = its index, and `+3` two on
  (one `inc` there, one at the common end - to `Form0Sub0Steps[3]`); none:
  the point two rows ahead, the object there marked (bit 0 of `+0x80`) with
  sound 0x10B, then `PartyAction18_CellStrike` on its cell and the cells one
  on (as the Resolve states), and `+3` one on (to `[2]`, `0x51D440`).
  `Sprite_ScriptTickOnce` every time.
- **`PartyAction18_CellStrike` `0x524BB0`** (0x188, `(x, z)`, al) - cell 0xF0,
  0xF1 or 0xF4: `Effect_SpawnAtCellHigh(0, x, z)`, a second (state 4) when
  `Rand & 7` is 6 or 7, sound 0x10B, 1. Cell 0xF6 or 0xF7:
  `Effect_SpawnAtCellHigh(0, x, z)`, sound 0x10B, then `Rand & 0xF`: below 7
  `Effect_SpawnAtCellHigh(3, ...)`, item 0x29's name to `Text_Records`,
  `Inventory_Add(0, 0x29, 1)` taken - sound 0x106 and `Msg_OpenSystem(2)` -
  else `Msg_OpenSystem(3)`, `+0xB = 2`; above 0xB
  `Effect_SpawnAtCellHigh(2, ...)`, `Sprite_FlashClut(0)`, `Char_LoseHp(1,
  Field_State +0x89)`, `Msg_OpenSystem(0xD9)`, `+0xB = 1`; `Field_Request =
  2` on all three; 1. Else 0. R1A's `0x51D4E0`, R1C's `0x51F880` and R1E's
  `0x522E20` are the same 0x188 bytes (capstone, normalised: `r1f/cmp.py`) -
  for those groups to know.
- **`PartyAction18_ProbeStart` `0x524D60`** (Sub1 step 0) = **`PartyAction_ProbeStart`
  `0x523ED0`** (byte-identical, 0x35) - `PartyAction_SideProbes`, the pose
  `HalfTurn + 0x42`, `+0xA = 0xB`, `+3 = 1`. `0x523ED0` is entry 0 of the
  step table `0x66000C`, which R1E's `0x523EB0` reads.

**Shared by other sets' tables:**

- **`PartyAction_EffectCountdown` `0x523F10`** (0x97; seven step tables, each
  after a ProbeStart-shaped step: `0x65FAC0`, `0x65FB94`, `0x65FC80`,
  `0x65FD8C`, `0x65FF44`, `0x660010`, `PartyAction18_Form0Sub1Steps[1]`) -
  `Sprite_ScriptTickOnce` non-zero: the pose `+8` and `+3 = 2`. Else `+0xA`,
  when not 0, down; reaching 0: `Sound_PlayEffect(u16 +0x2C + 0x100)`, then
  effect object `+0xB`'s `+8` = the facing, `+7` raised to 1 when it is 0,
  the object's `+0xA` = `+7`, sound 0x10B.
- **`PartyAction_SpawnKind1B` `0x5252B0`** (0x7B; seven state tables:
  `0x65F9D8`, `0x65FA30`, `0x65FA8C`, `0x65FDFC`, `0x65FFAC`,
  `PartyAction17_Form2States[0]`, `PartyAction18_Form2States[0]`) - an even
  facing one eighth back; `+0xB = Effect_FindFree`; not 0xFF: that object's
  `+0 = 1`, kind `+5 = 0x1B`; the pose `+8 >> 1` (unsigned) `+ 0x42`;
  `Sound_PlayEffect(u16 +0x2C + 0x100)`; `+2` one on.

### 1.3 A raised sprite's cell ahead

`Field_CellAhead` `0x526DB0` (ours, member_sprites) is `cmp [+0x70], 0; jne
0x527640; jmp Field_CellAheadFlat`. `0x527640` is the raised half; its
callers read al ([`member-sprites.md`](member-sprites.md) section 4: 0 stop,
1 go on, 3 nothing in the way that this knows, 6 / 7 a state change, or
`Field_CellSlope`'s 2 / 4 / 5). "Cell n" is the byte `0x903850 + n`.

- **`Field_CellAheadRaised` `0x527640`** (0x766) - 1 at once when
  `Field_ScriptFlags` has bit 10, **or when both fractions are 0** (the flat
  half answers 1 when both are non-zero). The cell ahead is the cell words
  plus `Field_CellOffsets`' signed byte pair for the facing (an offset of 1
  counts 2), seen from the sprite's own cell (one on where the offset is 1):
  `Field_ReadCellsRaised(ahead, from)`. Then by the facing and the fractions
  (re-read):
  - straight, both fractions: cell 8 = class of cells 1..5, 9 of 4, 5, 10 of
    2, 3; 0xB0: 6; cell 9 0x70: turn (4, 6, 5), 7; cell 10 0x70: turn (2, 4,
    3), 7; `Field_CornerTurn` non-zero: 0; then the slope sides - cell 9 a
    0xA_ with the facing 1 or 5: `Field_CellSlope(9)`; cell 10 with 3 or 7:
    `CellSlope(0xA)`; cell 9 a 0xA_ otherwise: the facing put back, turn (4,
    6, 5), `CellSlope(9)`; cell 10 likewise (2, 4, 3), `CellSlope(0xA)`;
    else `Field_RaisedEdgeTurns(ahead)`, 3;
  - straight, x mid-cell: cell 8 = class of 3, 2, 6; 0xB0: 6; 0x10 / 0x20:
    turn (4, 6, 5); a 0xA_: turn (2, 4, 3) unless now diagonal,
    `CellSlope(8)`; else (straight) two `Field_SlopeBetween` probes on z, each
    turning (4, 6, 5); 3;
  - straight, z mid-cell: the same with cells 5, 4, 7, the turns swapped and
    the probes on x;
  - diagonal, both fractions: cell 8 = class of 2, 3 (facing with bit 1) or
    4, 5; 0xB0: 6; 0x70: 7; 0x10 / 0x20: 0; 0xA_: `CellSlope(8)`; else one
    slope probe: steep 0, flat 3;
  - diagonal, one fraction (only facings 3 / 7 for x mid-cell, 1 / 5 for z):
    cell 8 = class of three, cells 9..0xB `Field_CellClass` of each; 0xB0:
    6; 0x70: 7; 0x20 with cell 2 (or 4) a 0x2_: 0; 0x10 / 0x20: the last
    class 0x10: 0, else three `Field_CellPairTurn` tests (0x10, then 0xA2
    with the direction cell 0xB picks) - any: 0; then `CellSlope(8)` for a
    0xA_; else two slope probes setting the facing to 1 / 5 (or 7 / 3) and
    answering 1, or 3.
- **`Field_CellClass5` `0x527DB0`** (0x23B, five bytes) - `Field_CellClass`'s
  rules for up to five cell indices: two 0x70 alone 0x70; a 0x70 counts as
  0xFF; 0xFF anywhere 0x10; all 0xB0 (or no cell) 0xB0. Two cells: 0xA0 in
  either, or 0xA2 in either with an even facing, 0x10; 0xA3 first: 0xA3 with
  0xA3, else 0x10; a 0xA_ in either: the first when both are equal, else
  0x10. Three: 0xA3 anywhere 0x10, 0xA2 anywhere with an even facing 0x10; a
  0xA_ anywhere: the first and third read 0xA1 for 0xA0 (the third in the
  list), the first when all three agree, else 0x10. One, four or five: any
  0xA_ 0x10. Otherwise 0x20 when the first 0x2_ differs from a later 0x2_,
  else 0.
- **`Field_CornerTurn` `0x527FF0`** (0x74) - cell 8 0x10 / 0x20: cells 9 and
  10 both 0x10 / 0x20: 1; only 9: turn (2, 4, 3); only 10: turn (4, 6, 5);
  neither: the facing one on; 0. Else 0. Field_CellAheadFlat's corner code
  as a function.
- **`Field_SlopeBetween` `0x5280A0`** (0x4F) - `MapView_SlopeAt` at the
  midpoint of two cells (each sum of words as 16.16, halved toward zero) in
  the direction given; 1 when the sloped flag is set and the answer's low
  word is above 0x40 (s16). `Field_ObjectAhead`'s test at a midpoint.
- **`Field_RaisedEdgeTurns` `0x528190`** (0x122) - four slope probes, each
  only while the facing (re-read) is even: `(x, x, zs, zs + 1)` and `(x, x,
  zs, z)` / `(x, x, z, zs + 1)` (by the facing's bit 2) in direction 1 turn
  (4, 6, 5); `(xs, xs + 1, z, z)` and `(xs, x, z, z)` / `(xs + 1, x, z, z)`
  (facing 0 or 6 / other) in direction 3 turn (2, 4, 3).
- **`Field_ReadCellsRaised` `0x5287B0`** (0xC3) - cells 1..7 by
  `Field_CellKind`: the cell ahead from the cell stepped from, then the
  sprite's own row and column either side (`zs`, `zs + 1`, `xs`, `xs + 1`,
  `zs - 1`, `xs - 1`), `Sprite_Current` re-read before each.
  `Field_ReadCells`' seven-cell counterpart.

The turns are `Field_TurnUnless(a, b, to)`; every function re-reads
`Sprite_Current` and the cells after each call, as the original does.

### 1.4 The leader's state 9, stage 0

E1E took stages 2..9 of `LeaderPanel_Stages` ([`effect_1e.md`](effect_1e.md));
stage 0 and the state's own dispatcher are here, stage 1 is R1G's.

- **`LeaderPanel_S0Begin` `0x5288C0`** - the pose byte `0x6BC716`, the count
  `0x6BC709` and `FieldPanel_DrawBlink`'s switch `0x939A28` cleared; `+0x29 =
  5`; the height word = `AreaMap_Elevation(+0x34, +0x38)`; `Field_State
  +0x89` 0: `Sprite_SetAnimationAt(0xA, 2)`, else `Sprite_SetAnimation(0xB)`;
  `+3` one on.
- **`LeaderPanel_S0Wait` `0x528940`** - once `MoveScript_WaitWordDA` is 0:
  effect record 4's `+6 = 0` and its state `+1` one on, `+3` one on.
- **`LeaderPanel_S0End` `0x528970`** - once effect record 4's state is 4:
  record 1's state one on, `+2` one on (stage 1), `+3 = 0`.

`0x528880` pairs with the PSX `0x801D9B9C` (`pairs_propagated.json`,
table-anchored); the sibling names no function there, so the name is from
the PC code.

## 2. Calling convention, arguments, answers

All 49 are `cdecl`; every call out is `E8` and leaves the function
(`band_rows.py --clones`: 0 to 51 sites, `0x527640` the most), every jump
stays inside but the 24 dispatchers' table jumps.

| Function | Arguments | eax at the `ret` |
|---|---|---|
| the 36 state handlers and dispatchers | none | whatever the last callee left; no caller reads it |
| the three pickups, `PartyAction18_CellStrike` | `(x, z)` - dwords whose low words are the cells (the Resolve / Strike states push their own stack's high words: the upper halves are uninitialised stack); every callee reads 16 bits | al 1 / 0; the callers test al |
| `Field_CellAheadRaised` | none | al (`Field_CellAhead` returns it) |
| `Field_CellClass5` | five dwords pushed as immediates, read as bytes | al (stored to a cell, compared) |
| `Field_CornerTurn` | none | al 0 / 1 (tested) |
| `Field_SlopeBetween` | four words (the callers push `eax` over leftovers) and a whole direction | al 0 / 1 (tested) |
| `Field_RaisedEdgeTurns`, `Field_ReadCellsRaised` | two / four words | nothing read (the caller sets al 3 after) |

So the eight that answer are `unsigned char` in ours, compared on al
(`ret_mask` 0xFF); the rest `void`.

**What the callees read**, for the masks: `AreaMap_ByteAt`,
`Effect_SpawnAtCell`, `Effect_SpawnAtCellHigh`, `AreaMap_ClearCell`,
`Field_CellKind`, `Field_SlopeBetween` and the four pickups / strike read
their cell arguments as 16 bits (movsx or a word load; their `symbols.toml`
entries); the spawns read the state as a byte. `MapView_SlopeAt` hands its
direction to `AreaMap_Slope`, which reads a byte: the Begin states push it
as `al` over `Sprite_Current`'s address. `Effect_SpawnAtCellHigh` is pushed a
fourth dword (the strike: its z slot with the cell in the low byte), not
read.

## 3. The tables

23 `[[data]]` entries named in `symbols.toml` (`count` the run of code
pointers to the next table a dispatcher reads, checked by hand: no reader
bounds its index):

| Table | At | Entries | Read by |
|---|---|--:|---|
| `PartyAction16_FormAction2States` | `0x660018` | 3 | `PartyAction16_FormAction2` by `+2` |
| `PartyAction16_Form2States` | `0x660024` | 3 | `PartyAction16_Form2` |
| `PartyAction16_FormActionForms` | `0x660030` | 3 | `PartyAction16_FormAction` by `+0x2C` |
| `PartyAction16_Forms` | `0x66003C` | 3 | `PartyAction16_ByForm` by `+0x2C` |
| `PartyAction17_FormAction0States` | `0x660048` | 3 | `PartyAction17_FormAction0` |
| `PartyAction17_Form0States` | `0x660054` | 3 | `PartyAction17_Form0` |
| `PartyAction17_FormAction1States` | `0x660060` | 3 | `PartyAction17_FormAction1` |
| `PartyAction17_Form1States` | `0x66006C` | 3 | `PartyAction17_Form1` |
| `PartyAction17_FormAction2States` | `0x660078` | 3 | `PartyAction17_FormAction2` |
| `PartyAction17_Form2States` | `0x660084` | 2 | `PartyAction17_Form2` |
| `PartyAction17_FormActionForms` | `0x66008C` | 3 | `PartyAction17_FormAction` |
| `PartyAction17_Forms` | `0x660098` | 3 | `PartyAction17_ByForm` |
| `PartyAction18_FormAction0States` | `0x6600A4` | 3 | `PartyAction18_FormAction0` |
| `PartyAction18_Form0Subs` | `0x6600B0` | 2 | `PartyAction18_Form0` |
| `PartyAction18_Form0Sub0Steps` | `0x6600B8` | **5** | `PartyAction18_Form0Sub0` by `+3` |
| `PartyAction18_Form0Sub1Steps` | `0x6600CC` | 3 | `PartyAction18_Form0Sub1` by `+3` |
| `PartyAction18_FormAction1States` | `0x6600D8` | 3 | `PartyAction18_FormAction1` |
| `PartyAction18_Form1States` | `0x6600E4` | 3 | `PartyAction18_Form1` |
| `PartyAction18_FormAction2States` | `0x6600F0` | 3 | `PartyAction18_FormAction2` |
| `PartyAction18_Form2States` | `0x6600FC` | 2 | `PartyAction18_Form2` |
| `PartyAction18_FormActionForms` | `0x660104` | 3 | `PartyAction18_FormAction` |
| `PartyAction18_Forms` | `0x660110` | 3 | `PartyAction18_ByForm` |
| `LeaderPanel_Stage0Steps` | `0x660220` | 3 | `LeaderPanel_S0` by `+3` |

`PartyAction18_Form0Sub0Steps` holds five: its two own steps and
`0x51D440`, `0x51F850`, `0x522DE0` at `0x6600C0..` - no code names
`0x6600C0`, but the Strike step moves `+3` to 2 (none ahead) or 3 (an effect
object ahead: `0x51F850`, which moves it to 4, `0x522DE0`), and R1E's table
`0x65FFF8` has the same three after its own two. The tool's band counts
("202 code entries" and so on) run on through every later table; each count
above is the reader's reach. Not named here (another group's reader):
`0x66000C` (R1E's `0x523EB0`; it holds `PartyAction_ProbeStart`,
`PartyAction_EffectCountdown`, `0x51D6D0`) and the set tables `0x65F9..` /
`0x65FA..` / `0x65FD..` / `0x65FF..` where `PartyAction_EffectCountdown` and
`PartyAction_SpawnKind1B` also sit.

## 4. The fuzz (`rest_1f_fuzz.cpp`)

`scenario_harness::Run` in field mode, **8,000 rounds per function**,
`sprite_span` 2 (the smallest table; each dispatcher's own index is seeded
below its own table). `BOF3X_R1F_ONLY=<name>` runs the clones whose name
contains it (the controls). The 36 state handlers and dispatchers are
`kSprite`; the 13 helpers `kCall`, the eight that answer with `ret_mask`
0xFF. The 24 tables (the 23 above and E1E's `LeaderPanel_Stages`) are
swapped for recorders on both sides.

**The callees** (the group's listing, registered before the standard rows):

| Callee | Masks | Answers |
|---|---|---|
| the group's own: the three pickups, `PartyAction18_CellStrike` | 16 bits each | `kFlag` |
| `Field_CellClass5` | whole x5 | al a class value (0xB0, 0x70, 0x10, 0x20, 0xA0..0xA3, 0xA5, 0, 0x21, 0xFF) or any byte |
| `Field_CornerTurn` | - | al 0 two times in three, else non-zero |
| `Field_SlopeBetween` | 16 bits x4, the direction whole | `kFlag` |
| `Field_RaisedEdgeTurns`, `Field_ReadCellsRaised` | 16 bits each | garbage |
| R0A's `PartyAction_TargetAhead`, `_BlockedAhead`, `_SideProbes` | - | `kFlag` / garbage |
| `Effect_SpawnAtCellHigh`, `Effect_SpawnAtCell` | the state's byte, the cells' 16 bits | garbage |
| `Field_TurnUnless`, `Field_CellPairTurn` | whole | **write the facing `+8`** half the time (the real ones do); the pair test al 1 a third of the time |
| `Field_CellSlope`, `Field_CellClass`, `Field_CellKind` | whole / whole / 16 bits | garbage / a class / garbage |
| `AreaMap_ClearCell` | 16 bits each | garbage |
| `Field_GiveZenny`, `Sprite_FlashClut` | whole | garbage |
| `Field_EffectAhead` | - | al 0xFF half the time, else 0..19 (what the real one answers) |
| `Sprite_ObjectAt` | whole x3 | al 0xFF half the time, else 0..0x21 |
| `AreaMap_ByteAt` | 16 bits each | al the codes 0xF0..0xF8, 0xEF, 0, 0xFF or any byte |
| `MapView_GroundAt` | whole | ax the height word + 0x40, 0x41, 0x3F, 0, 1, -1, 0x8040, 0x7FFF, 0x8000, or random |
| `MapView_SlopeAt` | whole, whole, 8 bits | sets the sloped flag (0 a third of the time); ax around 0x40 and the s16 limits |

The standard rows serve the rest: `Effect_FindFree` (0xFF or 0..0x13),
`Rand` (`kRand`, the hint on each branch's nibble), `Item_NamePtr` (into the
text buffer), `Inventory_Add`, `Msg_OpenSystem`, `Sound_PlayEffect`,
`Sprite_EnsureAnimation`, `Sprite_SetAnimation` / `At`,
`AreaMap_Elevation`, `Sprite_ScriptTickOnce`, `Char_LoseHp`.

**The state**: field mode's standard regions (Sprite_Current and the sprite
records, `ObjTrio`, `Field_State`, `Effect_Objects`, the cells
`0x903850..0x90385F`, `Field_InputFlags`, `Field_ScriptFlags`,
`Field_Request`, the wait word) and five of the group's:
`Field_DirectionSteps` (0x40), the cell-ahead offsets `0x66971C` (0x10),
`Text_Records`' first 16 bytes, `0x6BC700` + 0x20 and `0x939A00` + 0x30
(effect_1e's). 23,772 bytes, 43 regions.

**The seeds** (after the harness's fill; `Seed(k)`): the step rows half the
time the exe's shape (0, +-0x8000), else boundaries; the offsets -1, 0, 1
mostly (1 counts 2), else 2, -2, 0x80, 0x7F or random; the cells 1..0xF a
class or kind value (0xB0, 0x70, 0x10, 0xFF, 0x2_, 0xA0..0xA3, 0xA5, 0, 0x52)
and the sloped flag; all four sprite records' facing (0..7 two times in
three, else 8, 9, 15, 0x80, 0xFF or random), `+7`, `+0xA` (0, 1, 2, 5),
`+0xB` (below 20 for `PartyAction_EffectCountdown`, which indexes the
records by it; else 0, 1, 2, 0xFF, below 20 or random), the form word, x
and z (a fraction 0 three times in eight, one in five for
`Field_CellAheadRaised`, which returns at once when both are 0), the height
word, `+0x70`; each dispatcher's index below its table; `Field_InputFlags`'
two bits, `Field_ScriptFlags` bit 10 one round in eight, `Field_State
+0x89`, the wait word, effect record 4's state at 4 often, `Rand`'s hint
on 0xD..0xF, 0xC, 0, 4..7, 0xB. `Field_CellClass5`'s arguments: 0..5 cells
1..0xF, then a 0, then any below 0x10.

**The disturbance** (the group's case of the harness's, from its hash
only): `Sprite_Current`'s facing, `+0xA`, `+7`, its fractions, cell words,
height and form word; the sloped flag and cells 8..0xB (a class value);
`PartyAction_EffectCountdown`'s effect record (`+8` / `+0xA`);
`Field_InputFlags`; effect record 4's state. The harness's own moves
`Sprite_Current` among the four sprite records.

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_1f`, exit 0): **392,000 rounds over 49 functions, 701,527
calls to the stand-ins, 0 mismatches**, on the first run. Coverage (calls
the originals made) includes `Field_CellKind` 56,000, `Field_SlopeBetween`
13,543, `Field_CellClass5` 10,798, `Field_TurnUnless` 10,035, the pickups
about 3,700 each, `PartyAction18_CellStrike` 1,889, `Field_RaisedEdgeTurns`
443, `Field_CellPairTurn` 131 (the diagonal one-fraction paths), every entry
of the 24 tables (`LeaderPanel_Stages`' twelve about 500 each).
STAR_RESULT

## 5. Controls

`r1f/controls.py` (scratch): each plant replaces a string that occurs
exactly once in `rest_1f.cpp`, rebuilds, runs the self-test on the clones
whose name contains the one it touches (`BOF3X_R1F_ONLY`), restores the
file and rebuilds. CONTROLS_TABLE

## 6. Divergence

None. The 49 are faithful; `DIVERGENCE.md`, `cheats.cpp` and
`widescreen.cpp` name no byte in `0x523ED0..0x52899B` (grep, 2026-10-04), no
full-frame fill is drawn here, and no harness row (`scenario_harness*.cpp`,
`boss_harness*.cpp`) names any of the 49. What ours does not reproduce is
the reads past a table or an array of section 7, where ours aborts with a
message (the project's rule, not a choice of behaviour).

## 7. Latent defects and ranges (Capcom's, described, not fixed)

- **No dispatcher bounds its index** (the 24): a state byte, step byte or
  form word past its table jumps through the next table's cells (and past
  the last, through code addresses' neighbours). **Ours aborts** with a
  `Fatal` naming the function, the index and the table. Whether play
  reaches it is not established: every state handler here moves `+2` / `+3`
  only within its table (Begin and Resolve to the next entry, the
  `PartyAction_Finish` / `ScriptEnd` entries ending the action), and the set
  tables' other entries are other groups'.
- **`PartyAction_EffectCountdown` indexes `Effect_Objects` by `+0xB`
  unchecked** (`shl ecx, 7` on the byte): past 19 it writes `+8` / `+0xA` of
  whatever follows the 20 records (0xFF: `0x7E9168`, `0x7E916A`). **Ours
  aborts.** What `+0xB` holds when it runs is left by the steps before it,
  which are other groups' in six of its seven tables; in set 18's
  (`Form0Sub1Steps`) the path that reaches it is not read here. Not
  established either way.
- **`PartyAction_SpawnKind1B` and `PartyAction18_Form0Sub0Strike` use an
  effect index unchecked** (`Effect_FindFree`'s, `Field_EffectAhead`'s, the
  latter `movsx`): the real ones answer 0..19 or 0xFF only, and 0xFF is
  tested; ours aborts on any other.
- **The object mark** (Resolve, Strike): `Sprite_ObjectAt`'s answer as a
  signed byte, 0..0x1D into `Sprite_Objects`, 0x1E.. into
  `Sprite_ObjectsExtra` with no upper bound, a negative byte before
  `Sprite_Objects`. The real one answers 0..0x21 or 0xFF; ours aborts on any
  other (`field_hidden`'s `PartyAction5_Form0Resolve` reproduces it
  unchecked; the rule changed since).
- **The facing is not masked** where it indexes `Field_DirectionSteps` and
  the cell offsets (an odd facing is kept as it is): a byte above 7 reads the
  `.data` after each table. Reproduced (read in place), as R0A's helpers do.
- **Ranges, not defects**: the cells one on wrap at 16 bits; the sound id
  `+0x2C + 0x100` wraps at 16; the zenny `amount * 20` is a byte (100 at
  most); the pickups' and the strike's arguments are 16-bit cells whose
  upper halves the callers leave as stack garbage, read by no callee.

## 8. Calls across groups

- **Out**: only into R0A (merged, by name): `PartyAction_TargetAhead` (6
  sites), `PartyAction_BlockedAhead` (2), `PartyAction_SideProbes` (3),
  `Effect_SpawnAtCellHigh` (5) - 16, as `band_rows.py --edges` counts. No
  call into a group of this round or a later wave; so **no
  `rest_1f_callees.h`**: every callee is ours or Capcom's `Rand` by name.
- **In, from outside the group**: `Field_CellAhead` `0x526DB0` (ours,
  member_sprites) tail-jumps to `Field_CellAheadRaised` (rebound, section
  10). The rest are reached through `.data` only: `Field_FormActions[16..18]`,
  `Field_ActionBySet[16..18]`, `Field_LeaderStates[9]`, and for the shared
  states the set tables named in section 3 - `0x66000C` (read by R1E's
  `0x523EB0`) holds `PartyAction_ProbeStart` and
  `PartyAction_EffectCountdown`; `0x65FAC0`, `0x65FB94`, `0x65FC80`,
  `0x65FD8C`, `0x65FF44` hold `PartyAction_EffectCountdown`; `0x65F9D8`,
  `0x65FA30`, `0x65FA8C`, `0x65FDFC`, `0x65FFAC` hold
  `PartyAction_SpawnKind1B` - for whichever groups of wave one read those
  tables. No code of ours or of another group calls any of the 49 by `E8`
  (`band_rows.py --byte-tables`).

## 9. The live route

The catalog's `reach` column marks only `0x527640` (`+`, by its host
`Field_CellClass`'s 182 calls: an upper bound, not the function). No
first-call or counted trace under `analysis/calltrace` names any of the 49
(a scan of every file there but the entries lists, 2026-10-04) - and none
could: none of them was listed in `entries_logic.txt` but the nine with
their own lines (`0x5242B0`, `0x524750`, `0x524BB0`, `0x525150`,
`0x527DB0..0x5287B0`), so the tracer never had the hidden starts.
[`member-sprites.md`](member-sprites.md) section 6 found `0x527640` not
reached by the shop route. So all 49 are fuzz only; the coordinator's state
hash covers whatever a route reaches after the merge. The fishing routes
(R1G's) pass through `Field_LeaderStates[9]`'s later stages; whether they
enter stage 0 (`LeaderPanel_S0*`) is for that hash to show.

## 10. The rebinding

`band_rows.py --refs --group R1F` and `grep -rn -i -E "0x(523ED0|...|528970)"
src/game`: two of the 49 referenced, in member_sprites and effect_1e.

| File | Change |
|---|---|
| `member_sprites.cpp` | `kOriginals`' `Fn<...>(0x527640)` is `Fn<...>(bof3::addr::Field_CellAheadRaised)` (the value unchanged); the comment above `Field_CellAhead` names it |
| `member_sprites_callees.h` | `cell_ahead_raised`'s comment: R1F's `Field_CellAheadRaised`, not nobody's |

**Left raw, on purpose**: `member_sprites_fuzz.cpp`'s `case 0x527640` in its
stand-in switch and the `{0xC, 0x527640}` call site of its `Field_CellAhead`
row (the disassembly's targets, as round thirteen's EGT left its `CallSite`
tables); `effect_1e.cpp`'s header comment naming `Field_LeaderStates[9] =
0x528880` (it describes the table cell). No raw reference in a file another
group of this round is writing.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04): the 40 of the 49 that had
no line, each with the extent read here (`00523ED0 35` .. `00528970 2C`).
The nine already listed keep their lines: five agree with the read
(`0x524750` 11F, `0x527DB0` 23B, `0x527FF0` 74, `0x5280A0` 4F, `0x528190`
122) and four are the hosts' extents that ran over the hidden starts after
them (`0x5242B0` 49B, `0x524BB0` 1E5, `0x525150` 220, `0x5287B0` 520; read
0x11F, 0x188, 0x11F, 0xC3) - those starts now have their own lines, so the
coverage is kept; a second line for the same entry would duplicate it
(as R0A left its seven).
