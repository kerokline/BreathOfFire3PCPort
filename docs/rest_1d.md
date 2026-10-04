# Group R1D: party sets 9 to 12's field actions

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, on the
round branch's tip `ba2c3c3`. **46 functions ours** (`src/game/rest_1d.cpp`,
declarations in `src/game/rest_1d.h`, the state tables' addresses in
`src/game/rest_1d_callees.h`, shadow name `rest_1d`): the cut's 46 rows for
R1D (`analysis/round14_cut.tsv`), each read to its last instruction with
capstone and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
276,000 rounds, 0 mismatches. 103 controls planted one at a time: 101
refused, 2 equivalent mutants not refused, each with its near variant
refused (section 6). No recorded route enters any of the 46 (section 9).

The band is one thing: **the dispatchers and state handlers of party sets 9,
10, 11 and 12** (and one dispatcher of set 13) under the two tables the field
core indexes by the party set (`Field_ActionBySet` `0x6609D0`, leader state
10, and `Field_FormActions` `0x660A44`, leader state 6). No start is a case, a
shared tail or data; none was dropped or added; the tool's extents
(`band_rows.py --byte-tables`) are right for all 46, the cut's sizes are
padding past them. No PSX twin is paired for any (`pairs_propagated.json`
names none), so every name is from what the PC code does and the table that
reaches it. What the sets, forms and cell codes are in play is not read here;
the names say what the code does.

## 1. What each function does

"Sprite_Current" is the sprite whose state runs (the leader or a member): its
state bytes `+2` / `+3`, its counters `+9` / `+0xA`, `+6`, `+7`, `+0xB`, its
direction `+8`, its u16 form word `+0x2C`, its 16.16 position `+0x34` /
`+0x38`, its height word `+0x3E`, the byte `+0x2B`. "A step" is a row of
`Field_DirectionSteps` (`0x6697B0`), read in place with the direction
unmasked, as every reader of it does. "The pose k" is
`Sprite_EnsureAnimation((+8 - 1) / 2 + k)` with the signed division the
originals compute (`cdq`, `sub`, `sar`: direction 0 gives 0).

### 1.1 The dispatchers (27)

Each is `mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2 or 3]` (or
`mov ax, [ecx + 0x2C]`) `; jmp [eax * 4 + table]`, 0x12 or 0x13 bytes, the
index unchecked. Ours jumps through the cell as it holds it (Capcom's address;
a recorder while the fuzz runs) and aborts past the table's own count
(section 5).

| Function | Entry | By | Table (count) | Reached from |
|---|---|---|---|---|
| `PartyAction9_ByForm` | `0x521340` | u16 `+0x2C` | `PartyAction9_Forms` `0x65FDB8` (3) | `Field_ActionBySet[9]` |
| `PartyFormAction9_ByForm` | `0x521320` | u16 `+0x2C` | `PartyFormAction9_Forms` `0x65FDAC` (3) | `Field_FormActions[9]` |
| `PartyAction9_Form1State1` | `0x520E10` | `+3` | `PartyAction9_Form1State1Steps` `0x65FD88` (3) | `0x65FD6C[1]` (R1C's form 1, `0x520A60`) |
| `PartyFormAction9_Form2` | `0x520E70` | `+2` | `PartyFormAction9_Form2States` `0x65FD94` (3) | `PartyFormAction9_Forms[2]` |
| `PartyAction9_Form2` | `0x520F20` | `+2` | `PartyAction9_Form2States` `0x65FDA0` (3) | `PartyAction9_Forms[2]` |
| `PartyAction10_ByForm` | `0x521A70` | u16 `+0x2C` | `PartyAction10_Forms` `0x65FE10` (3) | `Field_ActionBySet[10]` |
| `PartyFormAction10_ByForm` | `0x521A50` | u16 `+0x2C` | `PartyFormAction10_Forms` `0x65FE04` (3) | `Field_FormActions[10]` |
| `PartyFormAction10_Form0` | `0x521360` | `+2` | `PartyFormAction10_Form0States` `0x65FDC4` (3) | its forms table |
| `PartyAction10_Form0` | `0x521380` | `+2` | `PartyAction10_Form0States` `0x65FDD0` (2) | its forms table |
| `PartyFormAction10_Form1` | `0x5215C0` | `+2` | `PartyFormAction10_Form1States` `0x65FDD8` (3) | |
| `PartyAction10_Form1` | `0x5215E0` | `+2` | `PartyAction10_Form1States` `0x65FDE4` (3) | |
| `PartyFormAction10_Form2` | `0x5219E0` | `+2` | `PartyFormAction10_Form2States` `0x65FDF0` (3) | |
| `PartyAction10_Form2` | `0x521A00` | `+2` | `PartyAction10_Form2States` `0x65FDFC` (2) | |
| `PartyAction11_ByForm` | `0x5220C0` | u16 `+0x2C` | `PartyAction11_Forms` `0x65FE54` (3) | `Field_ActionBySet[11]` |
| `PartyFormAction11_ByForm` | `0x5220A0` | u16 `+0x2C` | `PartyFormAction11_Forms` `0x65FE48` (3) | `Field_FormActions[11]` |
| `PartyFormAction11_Form0` | `0x521A90` | `+2` | `PartyFormAction11_Form0States` `0x65FE1C` (3) | |
| `PartyAction11_Form0` | `0x521AB0` | `+2` | `PartyAction11_Form0States` `0x65FE28` (2) | |
| `PartyFormAction11_Form1` | `0x521C80` | `+2` | `PartyFormAction11_Form1States` `0x65FE30` (3) | |
| `PartyAction11_Form1` | `0x521CA0` | `+2` | `PartyAction11_Form1States` `0x65FE3C` (3) | |
| `PartyAction12_ByForm` | `0x522690` | u16 `+0x2C` | `PartyAction12_Forms` `0x65FEAC` (3) | `Field_ActionBySet[12]` |
| `PartyFormAction12_ByForm` | `0x522670` | u16 `+0x2C` | `PartyFormAction12_Forms` `0x65FEA0` (3) | `Field_FormActions[12]` |
| `PartyFormAction12_Form0` | `0x5220E0` | `+2` | `PartyFormAction12_Form0States` `0x65FE60` (3) | |
| `PartyAction12_Form0` | `0x522100` | `+2` | `PartyAction12_Form0States` `0x65FE6C` (2) | |
| `PartyAction12_Form0State0` | `0x522120` | `+3` | `PartyAction12_Form0State0Steps` `0x65FE74` (5) | `PartyAction12_Form0States[0]` |
| `PartyAction12_Form0State1` | `0x5224B0` | `+3` | `PartyAction12_Form0State1Steps` `0x65FE88` (3) | `PartyAction12_Form0States[1]` |
| `PartyFormAction12_Form1` | `0x522650` | `+2` | `PartyFormAction12_Form1States` `0x65FE94` (3) | |
| `PartyFormAction13_Form0` | `0x5226B0` | `+2` | `PartyFormAction13_Form0States` `0x65FEB8` (3) | `0x65FEF4[0]`, the table `Field_FormActions[13]` (`0x522B40`, R1E's) reads |

The "Form" names follow round eight's `PartyAction5_ByForm` /
`PartyAction5_Form0` / `PartyAction5_Form0States` (`field_hidden.md`):
`PartyActionN_*` hang under `Field_ActionBySet`, `PartyFormActionN_*` under
`Field_FormActions`. The other entries of these tables (`0x520840`,
`0x51C490`, `0x51FC80`, `0x5226D0`, `0x5252B0`, ...) are other groups' and
are reached through the tables, never called by address.

### 1.2 The state handlers and the cell probes (19)

Three bodies occur three times unchanged and one twice (capstone over each
pair, scratch `r1d/cmp.py`: the same instructions, the relative calls to the
same targets, but the resolve's calls to its own set's pickup). Ours shares
one C++ body per shape behind its own exported functions; each copy is its
own clone in the fuzz.

- **`PartyAction9_Form1Start` `0x520E30`** (0x35) -
  `PartyAction_SideProbes`, the pose 0x42, `+0xA` = 0xB, `+3` = 1.
- **`PartyFormAction_TurnToSide` `0x520E90`** (0x87; entry 0 of eleven state
  tables, a scan of `.data`) - with `Cond_ByteFA` 0xF only `Field_State
  +0x137` = 0. Otherwise the side direction 5 when |`+8` - 3| > |`+8` - 5|
  (signed, `cdq` absolute values), else 3, into `+3`; `+0xB` =
  `Sprite_TurnSense(that)`; `+9` = 2; `+2` one on. (The next state, `0x51FC80`,
  turns `+8` toward `+3`.)
- **`PartyAction9_Form2Begin` `0x520F40`, `PartyAction10_Form1Begin`
  `0x521600`, `PartyAction11_Form1Begin` `0x521CC0`** (0x1D4 each) - an even
  direction turned one eighth back; when `PartyAction_TargetAhead` finds
  nothing that way, two on; when nothing there either, back to the first turn.
  Then one step ahead in `+8`: `MapView_GroundAt` there less the height word
  (re-read after the call) as a short is **the rise**, and `MapView_SlopeAt`
  there with the direction re-read after the ground; `DamageScratch`'s flag set
  and the rise above 0x40 - the pose 0x46 and `+2` one on; otherwise `+0x2B` =
  1, the side probes in directions 3 and 5 inline (`PartyAction_SideProbes`'
  code), `Sound_PlayEffect(+0x2C + 0x100)`, the pose 0x42 and `+0xA` = 5.
  Then `+0xB` = 0 and `+2` one on (so a steep slope moves `+2` by two). It
  differs from `PartyAction5_Form0Begin` (round eight) in the steep test: that
  one compares the slope's answer, this one the ground's rise; the slope's
  answer is not read here.
- **`PartyAction9_Form2Resolve` `0x521120`, `PartyAction10_Form1Resolve`
  `0x5217E0`, `PartyAction11_Form1Resolve` `0x521EA0`** (0xDB each) -
  `PartyAction5_Form0Resolve` `0x51EAF0`'s instructions calling the set's own
  pickup: `+0xA` one down; at 0, the point two steps ahead; the object
  `Sprite_ObjectAt(point, 0)` finds gets `+0x80` bit 0 (`Sprite_Objects` below
  0x1E, else `Sprite_ObjectsExtra`, the byte compared signed); the pickup on the
  point's cell, then on the cell one on in x (x with a fraction), then one on
  in z (z with one), each only while the last answered 0; `+2` one on.
  `Sprite_ScriptTickOnce` every time.
- **`PartyAction9_CellPickup` `0x521200`, `PartyAction10_CellPickup`
  `0x5218C0`, `PartyAction11_CellPickup` `0x521F80`** (0x11F each, al 0 / 1) -
  `Field_CellPickup` `0x51EBD0`'s instructions with **one constant changed**:
  the zenny bonus's multiplier is 0x14 (`mov cl, 0x14; imul cl` at +0x63), not
  0xA. `AreaMap_ByteAt(x, z)`: 0xF2 - with an effect object free,
  `Effect_SpawnAtCell(0, x, z)`, `Rand & 0xF` at least 0xD finds zenny (2, 5 on
  0xF; twenty times that when `Field_InputFlags & 6`, read after the draw, and
  a second `Rand & 3` is 0), `Field_GiveZenny`, `Effect_SpawnAtCell(1, x, z)`;
  `+0xB` = 1. 0xF8 - `Effect_SpawnAtCell(0, x, z)`, item 0x56's name into
  `Text_Records`, `Inventory_Add(0, 0x56, 1)`: the sound 0x106 and message 2,
  else message 3; `Field_Request` = 2, `+0xB` = 1. Both clear the cell
  (`AreaMap_ClearCell`) and answer 1; any other cell 0.
- **`PartyAction10_Form0Begin` `0x5213A0`, `PartyAction11_Form0Begin`
  `0x521AD0`** (0x166 each) - `PartyAction_Kind30Ahead`'s object k: when
  `PartyAction_MemberOnEffect(k)` and then `_MemberBeyondEffect(k)` both
  answer 0, the object's `+0xB` = 1, `Sound_PlayEffect(+0x2C + 0x100)`, the
  animation `+8 + 8`, `Field_State +0x128` = 2, `Field_ScriptFlags`' bit
  0x1000 set (its high byte `| 0x10`), `Field_JumpStart`, `+9` one down,
  `Field_LeaderStepTick`, `Field_State +0x137` = 1 and `+2` one on; when
  either answers not 0, `Field_State +0x137` = 0. With none lined up: unless
  `Field_State +0x138` has bit 0, the object `Sprite_ObjectAt(two steps ahead,
  margin 1)` finds gets `+0x80` bit 0 (the byte compared **unsigned** here);
  `Field_State +0x137` = 0.
- **`PartyAction_WaitEffectDone` `0x521A20`** (0x2A; entry 1 after `0x5252B0`
  in seven tables) - `Field_State +0x137` = 0 once effect object `+0xB`'s
  in-use byte is 0; then a tail jmp to `Sprite_ScriptTick`.
- **`PartyAction_StepCountdown` `0x521C40`** (0x33; eight cells, among them
  the entry after `PartyAction10/11_Form0Begin`) - `+9` at 0:
  `Field_State +0x137` = 0 and `Field_ScriptFlags &= 0xEFFF` (the bit the
  starts set); else `+9` one down and `Field_LeaderStepTick`. Then a tail jmp to
  `Sprite_ScriptTick`.
- **`PartyAction12_Form0Begin` `0x522140`** (0xA0) - the turn of the
  `Form2Begin`s with `PartyAction_BlockedAhead` for `TargetAhead`;
  `Sound_PlayEffect(+0x2C + 0x100)`, the pose 0x42, `+0xB` = `+6` = 0, `+0xA`
  = 8, `+3` = 1.
- **`PartyAction12_Form0Resolve` `0x5221E0`** (0x140) - with `+0xA` not 0,
  one down; reaching 0: `Field_EffectAhead`'s object (signed, `movsx`) gets
  `+8` = the direction and `+0xA` = 1, `Sound_PlayEffect(0x10B)`, `+6` = its
  index and `+3` one on - and one on again below (two in all); with none, the
  object `Sprite_ObjectAt(two steps ahead, 0)` finds is marked (signed) with
  the sound 0x10B, and the cells are tried with `PartyAction12_CellHit` as the
  resolves try theirs; `+3` one on. `Sprite_ScriptTickOnce` every time.
- **`PartyAction12_CellHit` `0x522320`** (0x188, al 0 / 1) -
  `AreaMap_ByteAt(x, z)`: 0xF0, 0xF1, 0xF4 - `Effect_SpawnAtCellHigh(0, x,
  z)`, and `(4, x, z)` when `Rand & 7` is above 5; the sound 0x10B; 1. 0xF6,
  0xF7 - `Effect_SpawnAtCellHigh(0, x, z)`, the sound 0x10B; `Rand & 0xF` below
  7: `(3, x, z)`, item 0x29's name into `Text_Records`, `Inventory_Add(0, 0x29,
  1)`: the sound 0x106 and message 2, else message 3, `+0xB` = 2; above 0xB:
  `(2, x, z)`, `Sprite_FlashClut(0)`, `Char_LoseHp(1, Field_State +0x89)` (read
  after the flash), message 0xD9, `+0xB` = 1; 7..0xB nothing more; each
  `Field_Request` = 2 and 1. Any other cell 0. Its twins `0x51F880` and
  `0x522E20` are other groups' (R1C's, R1E's).
- **`PartyAction12_Form0EffectSet` `0x5224D0`** (0x81) -
  `Sprite_ScriptTickOnce` answering not 0: the pose `+8` (whole) and `+3` = 2.
  Else with `+0xA` not 0, one down; reaching 0: effect object `+0xB` gets `+8`
  = the direction; `+7` = 1 when 0; the object (`+0xB` read again) gets `+0xA`
  = `+7`; the sound 0x10B.

## 2. Calling convention, arguments, answers

All 46 are `cdecl`. The 44 handlers take nothing and answer nothing a caller
reads (they are jumped to or called through `.data` cells by the field core's
`call [table]`; the two that end in a tail jmp to `Sprite_ScriptTick` leave
its eax, which no caller reads). The two cell probes take `(x, z)` and answer
al, which their three callers each test first (`test al, al` at `0x5211B7`,
`0x5211CD`, `0x5222DC`, `0x5222F2` and the copies): `ret_mask` 0xFF.

**The cells the resolves pass.** As in round eight's
`PartyAction5_Form0Resolve`, the resolves store the point on their own stack
and push its high words as dwords read 2 bytes into each (`[esp+0x12]`,
`[esp+0x1A]`): the upper halves are stack the function never wrote. Every
callee of the probes reads 16 bits (`AreaMap_ByteAt`, `Effect_SpawnAtCell`,
`Effect_SpawnAtCellHigh` `movsx` the words; `AreaMap_ClearCell` casts to
short), so ours passes the cells as they are and the fuzz logs 16 bits.

**Bytes and words pushed whole.** `Sprite_EnsureAnimation`'s argument is the
pose in al over whatever eax held (a callee's answer, `Sprite_Current`);
`Sound_PlayEffect`'s the form word + 0x100 in cx over ecx; `Char_LoseHp`'s
member byte in dl; `MapView_SlopeAt`'s direction in al over the
`Sprite_Current` pointer; `PartyAction_Member*Effect`'s index over the
caller's ecx. Each callee reads the byte or word only (their `symbols.toml`
evidence; `AreaMap_Slope` reads the direction byte); the fuzz masks them so.
The callers push a fourth dword to `Effect_SpawnAtCellHigh` (the cell's byte
stored over the z argument's slot) and to `Inventory_Add` (0), neither read.

## 3. The state tables

27 `[[data]]` entries, one per dispatcher. **Each count is its reader's reach,
read by hand**: none of the readers bounds its index, and the 27 tables are
contiguous in `.data` (`0x65FD88..0x65FEC3`), each running into the next; so
each ends where the next table's own reader starts (every start is the
operand of exactly one `jmp [eax * 4 + T]` in the band - a scan of the image
for each address finds one `.text` reference). Two-entry tables: `0x65FDD0`,
`0x65FDFC`, `0x65FE28`, `0x65FE6C`; five: `0x65FE74`; the rest three. The
states and steps the handlers write stay inside them: the starts move `+2`
or `+3` by one or two to entries that exist, `0x520E30` sets `+3` = 1,
`0x5224D0` `+3` = 2, `0x522DE0` (R1E's) `+3` -= 2 within the five; the forms
word `+0x2C` is the set's form, three in every set's table.

## 4. The fuzz (`rest_1d_fuzz.cpp`)

`scenario_harness::Run` in field mode, **6,000 rounds per function**: the
dispatchers and handlers `Shape::kSprite`, the two probes `Shape::kCall`
(`ret_mask` 0xFF). `BOF3X_R1D_ONLY=<name>` runs the clones whose name
contains it (the controls). The 27 tables are `DataTable`s: their entries
are swapped for recorders while the fuzz runs, so a dispatcher's clone and
ours both land in the recorder of the entry they index, and a wrong table or
index logs a different handler.

**The callees** (32, all the group's own listing, registered before the
standard rows):

| Callee | Masks | Answers |
|---|---|---|
| `PartyAction_TargetAhead`, `_BlockedAhead` | - | `kFlag` (al 0 a third of the time) |
| `PartyAction_Kind30Ahead`, `Field_EffectAhead`, `Effect_FindFree` | - | al 0xFF half the time, else 0..19 (what the real ones answer) |
| `PartyAction_MemberOnEffect`, `_MemberBeyondEffect` | the index byte | al 0 three times in four |
| `PartyAction_SideProbes`, `Field_JumpStart` | - | garbage |
| `Effect_SpawnAtCellHigh`, `Effect_SpawnAtCell` | the state's byte, x and z 16 bits | garbage |
| `MapView_GroundAt` | whole | ax the height word + 0, +-1, 0x40, 0x41, 0x3F, 0x7FFF, 0x8000, 0x8041, or random |
| `MapView_SlopeAt` | whole, whole, the direction byte | `DamageScratch`'s flag 0 a third of the time; ax around 0x40 and the s16 limits |
| `Sprite_EnsureAnimation`, `Sprite_TurnSense`, `Sprite_FlashClut` | the byte | `kFlag` / garbage |
| `Sound_PlayEffect`, `Msg_OpenSystem` | 16 bits | garbage |
| `Sprite_ObjectAt` | whole | al 0xFF half the time, else 0..0x21 |
| `Sprite_ScriptTick`, `_ScriptTickOnce`, `Field_LeaderStepTick` | - | `kFlag` |
| `AreaMap_ByteAt` | 16 bits each | al one of 0xF0..0xF8, 0xEF, 0, 0xFF, or any byte |
| `AreaMap_ClearCell` | 16 bits each | garbage |
| `Field_GiveZenny` | whole (the amount `and edx, 0xFF`) | garbage |
| `Inventory_Add` | the bytes | `kFlag` |
| `Char_LoseHp` | whole, the member byte | garbage |
| the group's `PartyAction9/10/11_CellPickup`, `PartyAction12_CellHit` | 16 bits each | `kFlag` |

`Rand` (Capcom's CRT, `0x5B93D2`) is re-listed as the harness's `kRand` draw
(with a hint the seed sets) that also flips `Field_InputFlags`' bit 1 or 2 a
third of the time (section 6, P05). `Item_NamePtr` is the harness's standard
field row (a name in its text buffer).

**The state.** Field mode's standard regions and the group's two:
`Field_DirectionSteps` `0x6697B0` (0x40 bytes) and `Text_Records`' first 16
bytes. 23,676 bytes, 40 regions.

**The seeds** (after the harness's random fill; `Seed(k)`): Sprite_Current
one of `ObjTrio`'s records half the time; every record the disturbance can
move it to (the four sprite records, the three `ObjTrio` ones) given a
direction (0..7 two times in three, else 8, 9, 15, 0x80, 0xFF or any:
rows past the steps table are read in place, the same on both passes), `+9`
and `+0xA` at 0, 1, 2 or any, `+7` 0 or any, `+0xB` 0..19, a position (a cell
at 0, 1, 2, small, the u16 / s16 limits, with a fraction 0, a half, a quarter,
1, 0xFFFF or any) and a height word; `Field_DirectionSteps` as R0A seeds it;
`Field_State +0x138` 0..3 or any; `Field_InputFlags` with bits 1 / 2 set and
clear. A dispatcher's index byte (or the form word) below its table's count.
`PartyFormAction_TurnToSide`: `Cond_ByteFA` 0xF a third of the time.
`PartyAction_WaitEffectDone`: `+0xB` 0xFF one round in eight (read in place),
else a record in use or free. The pickups and the cell hit: the harness's Rand
hint at the thresholds (0xC..0xF; 5..7, 0xB, 0xC), `Field_State +0x89` 0..7.
The probes' x and z: a cell at 0, 1, 0x7FFF, 0x8000, 0xFFFF or small under
random upper halves, half the time.

**The disturbance** (the group's case of the harness's, from its hash only):
Sprite_Current's direction, `+9` / `+0xA`, `+0xB` (below 20), `+7`, the form
word, x or z, the height word; `Field_State +0x89` / `+0x138`;
`Field_InputFlags` bit 1 or 2 (the pickups read it after the first draw);
`DamageScratch`'s flag; a dword of `Field_DirectionSteps`; an effect record's
in-use byte. The harness's own moves Sprite_Current among the sprite records
and its state bytes.

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_1d`, exit 0, its first run): 276,000 rounds over 46
functions, 470,461 calls to the stand-ins, **0 mismatches** (the final fuzz;
the first form, before section 6's Rand flip, passed on its first run too); every table
entry reached (coverage `phase 0x...` for all 72 cells' handlers), every
callee reached (`Field_GiveZenny` 188, `Sprite_FlashClut` and `Char_LoseHp`
204 the thinnest). `BOF3X_SHADOW='*'`: section 10.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **L1 - `PartyAction_WaitEffectDone` reads effect record 0xFF, and play
  reaches it.** `0x5252B0` (the state before it in all seven tables) stores
  `Effect_FindFree`'s answer in `+0xB` without testing it (it tests it only to
  skip its own writes) and moves `+2` on; with no record free, `0x521A20` then
  tests the byte at `0x7E11E0 + 0xFF * 0x80` = `0x7E9160`, inside
  `Gfx_PacketPools`, as "the effect's in-use byte": the action ends
  (`Field_State +0x137` = 0) only when that packet byte is 0. **Ours reads the
  byte in place**, as the original does (a read, inside the image's `.bss`):
  no divergence, and so no ledger entry; whether the owner wants the action to
  end at once when no effect was placed is the owner's word. Reached only with
  all 20 effect records in use.
- **L2 - `PartyAction12_Form0EffectSet` writes effect record `+0xB`
  unchecked** (`+8` and `+0xA` of it). What sets `+0xB` on the way into
  `PartyAction12_Form0State1` was not traced (the state before it,
  `0x51F0E0`, does not write it). **Ours aborts at 20 and above** with a
  message (the project's rule for an index past a table whose reach is not
  established).
- **L3 - the dispatchers do not bound their index**: past its count each
  jumps through the next table's cells (always code here: the tables are
  contiguous). **Ours aborts** past the count; no handler writes such an index
  (section 3).
- **L4 - the object marks are indexed by `Sprite_ObjectAt`'s answer
  unchecked**, signed in the resolves and `PartyAction12_Form0Resolve` (an
  answer 0x80..0xFE would mark before `Sprite_Objects`), unsigned in the
  kind-0x30 starts (0x22..0xFE past `Sprite_ObjectsExtra`'s four). The real
  callee answers 0..0x21 or 0xFF; **ours aborts** on any other.
- **L5 - `PartyAction12_Form0Resolve` indexes `Effect_Objects` by
  `Field_EffectAhead`'s answer, signed**: the callee answers 0..19 or 0xFF;
  ours aborts on any other. The kind-0x30 starts index it by
  `PartyAction_Kind30Ahead`'s answer (0..19): likewise.
- **Ranges, not defects**: the direction is not masked where the steps table
  is read (reproduced in place, as R0A does); the cell one on wraps at 16
  bits; the zenny bonus 5 * 0x14 = 100 fits its byte.

## 6. Controls

`r1d/controls.py` (scratch): each plant replaces a string that occurs exactly
once in `rest_1d.cpp`, rebuilds, runs the self-test on the clones it names
(`BOF3X_R1D_ONLY`), restores the file and rebuilds. **103 planted: 101
refused, 2 not refused - both equivalent mutants, each with its near variant
refused.** A plant in a shared body was run on one copy (named below); the
count is the rounds that mismatched of 6,000 (12,000 where the filter matched
two clones).

| # | Clone | Plant | Refused |
|---|---|---|--:|
| D01 | PartyAction9_ByForm | through FormActions' table | 6,000 |
| D02 | PartyAction10_Form0 | by `+3`, not `+2` | 2,991 of 12,000 |
| D03 | PartyAction12_Form0State0 | by `+2`, not `+3` | 4,848 |
| D04 | PartyFormAction13_Form0 | through set 12 form 1's table | **not refused**: equivalent - the two tables hold the same three addresses (`0x520840`, `0x51FC80`, `0x52F5C0`), one recorder each. Near variant D05 refused |
| D05 | PartyFormAction13_Form0 | through set 11 form 1's (D04's near variant) | 1,955 |
| D06 | PartyAction10_ByForm | the form word's low byte | **not refused**: equivalent under the seed - the form words are below 3, so the high byte is 0 (an index above 0xFF aborts on both readings). Near variant D07 refused |
| D07 | PartyAction10_ByForm | the high byte (D06's near variant) | 4,045 |
| D08 | PartyAction9_Form1State1 | the steps one on | 6,000 |
| F01 | PartyAction9_Form2Begin | the first turn one on, not back | 2,778 |
| F02 | | the turn back one, not two | 307 |
| F03 | | an odd direction turned too | 3,217 |
| F04 | | steep from a rise of 0x40 | 370 |
| F05 | | the slope's answer as the rise (`PartyAction5_Form0Begin`'s test) | 1,669 |
| F06 | | the height read before the ground call | 57 (by the disturbance) |
| F07 | | the slope's direction from before the ground call | 217 |
| F08 | | the steep pose + 0x47 | 1,073 |
| F09 | | the steep path moves `+2` once | 1,073 |
| F10 | | `+0xA` = 4 | 4,927 |
| F11 | | the side probes in the other order | 4,927 |
| F12 | | a side slope steep from 0x40 | 461 |
| F13 | | the side ground at the height clears too | 157 |
| F14 | | the form's sound + 0x101 | 4,927 |
| F15 | | `+0xB` = 1 at the end | 6,000 |
| F16 | | `+0x2B` = 1 after the first probe | 663 |
| F17 | | the side probe's height not re-read after the ground | 42 (by the disturbance) |
| R01 | PartyAction9_Form2Resolve | `Sprite_ObjectAt` margin 1 | 2,392 |
| R02 | | `+0xA` two down | 5,997 |
| R03 | | `+2` two on | 2,365 |
| R04 | | `Sprite_ScriptTick` for `TickOnce` | 6,000 |
| R05 | | the z cell tried after x's found something | 311 |
| R06 | | the z cell one on in x too | 306 |
| R07 | | `Sprite_Objects` below 0x1D | 11 |
| R08 | | the mark bit 1 | 689 |
| R09 | | the point one step ahead | 2,045 |
| R10 | PartyAction10_Form1Resolve | set 10's resolve calling set 9's pickup | 2,392 |
| R11 | | the cell's fraction tested on 15 bits | 62 |
| P01 | PartyAction9_CellPickup | the multiplier 10 (`Field_CellPickup`'s) | 13 |
| P02 | | zenny from 0xC | 25 |
| P03 | | 5 from 0xE | 16 |
| P04 | | `Field_InputFlags` bit 1 only | 9 |
| P05 | | `Field_InputFlags` read before the first draw | 14 (by the Rand stand-in's flip) |
| P06 | | the second draw `& 7` | 5 |
| P07 | | the zenny's object state 0 | 58 |
| P08 | | `+0xB` = 1 with no object free too | 209 |
| P09 | | item 0x57's name | 451 |
| P10 | | 12 bytes of the name | 451 |
| P11 | | message 4 when not taken | 151 |
| P12 | | `Field_Request` 1 | 431 |
| P13 | | the cell not cleared with no object free | 217 |
| P14 | | 0xF9 for 0xF8 | 452 |
| K01 | PartyAction10_Form0Begin | `MemberBeyondEffect` asked first | 3,018 |
| K02 | | the object's `+0xC` | 1,632 |
| K03 | | the animation `+8 + 9` | 1,632 |
| K04 | | `Field_State +0x128` = 3 | 1,632 |
| K05 | | `Field_ScriptFlags`' bit 0x2000 | 1,206 |
| K06 | | `+9` down before the jump | 68 (by the disturbance) |
| K07 | | `Field_State +0x137` = 0 on the jump | 1,632 |
| K08 | | margin 0 with none lined up | 1,758 |
| K09 | | `Field_State +0x138` tested `& 3` | 598 |
| K10 | | a member on it leaves `+0x137` at 1 | 1,386 |
| K11 | PartyAction11_Form0Begin | object 19 taken as none | 136 |
| W01 | PartyAction_WaitEffectDone | the record's `+1` tested | 2,626 |
| W02 | | `Sprite_ScriptTickOnce` for `Tick` | 6,000 |
| W03 | | the index masked below 20 (the 0xFF read) | 765 |
| S01 | PartyAction_StepCountdown | `& 0xDFFF` | 1,089 |
| S02 | | the end at 1 | 1,469 |
| S03 | | no step tick | 4,523 |
| T01 | PartyFormAction_TurnToSide | the chapter 0xE | 2,578 |
| T02 | | 5 also on a tie | 356 |
| T03 | | `+9` = 3 | 4,330 |
| T04 | | the direction masked to 7 | 907 |
| T05 | | `+3` = the sense, not the target | 4,312 |
| A01 | PartyAction9_Form1Start | `+0xA` = 0xC | 6,000 |
| A02 | | the side probes after the pose | 6,000 |
| A03 | | the pose `(d - 1) >> 1` unsigned | 516 |
| B01 | PartyAction12_Form0Begin | `+6` = 1 | 6,000 |
| B02 | | `+0xA` = 7 | 6,000 |
| B03 | | back to the first turn when blocked | 957 |
| B04 | | `PartyAction_TargetAhead` for `BlockedAhead` | 2,783 |
| E01 | PartyAction12_Form0Resolve | the object's `+0xA` = 2 | 1,194 |
| E02 | | `+3` one on with an object ahead | 1,186 |
| E03 | | `+0xA` counted down from 0 | 1,213 |
| E04 | | no sound for a marked object | 629 |
| E05 | | margin 1 | 1,198 |
| E06 | | the object's `+8` from `+9` | 1,111 |
| E07 | | `+6` = the index + 1 | 1,194 |
| E08 | | the cells hit by set 9's pickup | 1,198 |
| H01 | PartyAction12_CellHit | 0xF4 not hit | 477 |
| H02 | | the second object above 4 | 243 |
| H03 | | the second object's state 5 | 299 |
| H04 | | the item below 6 | 85 |
| H05 | | the hurt above 0xA | 88 |
| H06 | | item 0x28 | 382 |
| H07 | | `+0xB` = 1 after the item | 382 |
| H08 | | `Field_State +0x89` read before `Sprite_FlashClut` | 1 (by the disturbance) |
| H09 | | message 0xD8 | 190 |
| H10 | | `Field_Request` untouched between 7 and 0xB | 251 |
| H11 | | the sound before the object (0xF6 / 0xF7) | 862 |
| H12 | | the hurt's amount 2 | 190 |
| X01 | PartyAction12_Form0EffectSet | `+3` = 1 at the script's end | 4,014 |
| X02 | | `+7` not raised from 0 | 419 |
| X03 | | the object's `+9` | 787 |
| X04 | | `+0xA` counted down from 0 | 406 |

The first run left P05 unrefused: the flags move only by the group's
disturbance, one case in sixteen of one harness case in sixteen. The group
now re-lists `Rand` (the harness's `kRand` draw) with an effect that flips
`Field_InputFlags`' bit 1 or 2 a third of the time, and gives the flags and
`Field_State` three disturbance cases each; P05 is refused (14). The
thinnest: H08 (1 round: the disturbance must move `Field_State +0x89`
between the flash and the read, on the hurt path), the zenny path's P01..P06
(4..25: three conditions on two draws), R07 (11: the answers 0x1D only).

## 7. Calls across groups

All 20 edges out of the group (`band_rows.py --edges`) go to R0A's helpers,
merged: `PartyAction_TargetAhead` (6 sites), `PartyAction_BlockedAhead` (2),
`PartyAction_Kind30Ahead`, `_MemberOnEffect`, `_MemberBeyondEffect` (2 each),
`Effect_SpawnAtCellHigh` (5), `PartyAction_SideProbes` (1), all called by
name through `game/rest_0a.h`. Every other callee is ours already. **No raw
address is called.** The tables' other entries (R1A's `0x51C490`,
`0x51D440`, `0x51D6D0`; R1B's `0x51F0E0`, `0x51F130`; R1C's `0x51F850`,
`0x51FC80`, `0x520820`, `0x520840`, `0x5208B0`, `0x520A40`, `0x520A60`; R1E's
`0x5226D0`, `0x522DE0`; R1F's `0x523F10`, `0x5252B0`; ours
`PartyAction_ScriptEnd`, `PartyAction_Finish`) are reached through the cells, never by
address, and need no rebinding.

**Inbound** (from outside the group): none by `E8`. The group is reached
through `.data` only: `Field_ActionBySet[9..12]` and `Field_FormActions[9..12]`
(the field core's `Field_ActionState` `0x52FB60` and `Field_FormActionState`
`0x52F4F0`, ours), R1C's
`PartyAction9_Form1` table `0x65FD6C` (`0x520E10`), R1E's table `0x65FEF4`
(`0x5226B0`), and the other sets' state tables holding the four shared
handlers (`0x520E90` in eleven, `0x521A20` in seven, `0x521C40` in eight,
`0x5224D0` also at `0x65FBF8`).

## 8. The rebinding

`band_rows.py --refs --group R1D` and `grep -rn -i` for every address and
every table in `src/game`: **no raw reference to any of the 46 or the 27
tables** in our source (none of `scenario_harness*.cpp`, `boss_harness*.cpp`
or any `*_callees.h` lists them), so nothing was rebound and no harness row
breaks. `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no byte in
`0x520E10..0x5226C1`.

## 9. The live route

The cut's `reach` column is empty for all 46 rows. No first-call or counted
trace under `analysis/calltrace` names any of them (a grep for the 46
addresses finds only DLL addresses that share digits). So **all 46 are fuzz
only**; the coordinator's state hash covers whatever a route reaches after
the merge.

## 10. Self-tests and the entry list

2026-10-04, this worktree, headless (`BOF3X_SELFTEST_ONLY=1`), the final
build:

- `BOF3X_SHADOW=rest_1d`: exit 0, 276,000 rounds, 0 mismatches.
- `BOF3X_SHADOW='*'`: exit 0, `inject: 8701 ours, 0 left original`, 718
  self-test lines, none with a mismatch (`rest_1d` among them).
- `BOF3X_SHADOW='*'` with `BOF3X_WIDE=1`: exit 0, the same 718 and 8,701.
  Each passed on its first run; none died silently. The same two passed
  before the controls' fuzz change, too.
- `tools/ledger_check.py`: 0 errors (2 notes, not this group's).

**`analysis/calltrace/entries_logic.txt`** (the main checkout's): 42 lines
appended with the extents read here (section 1). Four of the 46 already had
lines carrying a **host's** extent - `00521200 306`, `005218C0 6BB`,
`00521F80 3A0`, `00522320 231` (the catalog's hosts of the hidden starts after
them, each covering this group's own later functions) - left as they are, as
R0A left its seven: a second line for the same address would duplicate it;
their read extents are 0x11F, 0x11F, 0x11F and 0x188. R0A's host lines
`00521510 3AB` and `00522560 4BB` likewise cover this group's
`0x5215C0..0x5218BA` and `0x522650..0x5226C1`, which now have lines of their
own.

**Code in the band that no group holds**: none - between the 46 extents are
only `nop` / `int3` padding (`band_rows.py`: "0 not listed").
